// ESP8266 + HC-SR04 + traffic routine w/ restart-on-RED
// GPIO mapping:
// TRIG: 14 (D5) | ECHO: 12 (D6, level-shift to 3.3V)
// GREEN: 5 (D1) | YELLOW: 4 (D2) | RED: 13 (D7) | TOUCH: 15 (D8)

#include <Arduino.h>

const uint8_t TRIG       = 14;
const uint8_t ECHO       = 12;
const uint8_t GREENLED   = 5;
const uint8_t REDLED     = 13;
const uint8_t YELLOWLED  = 4;
const uint8_t TOUCHSENS  = 15;

const unsigned long TRAFFIC_INTERVAL_MS = 15000UL;  // swap every 15 s

// Routine phase: false = GREEN phase, true = RED phase
bool g_redPhase = false;
unsigned long g_lastPhaseChange = 0;

float readDistanceCm() {
  pinMode(TRIG, OUTPUT);
  digitalWrite(TRIG, LOW); delayMicroseconds(2);
  digitalWrite(TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  pinMode(ECHO, INPUT);
  unsigned long dur = pulseIn(ECHO, HIGH, 30000UL); // µs
  if (dur == 0) return NAN;

  return (dur * 0.0343f) / 2.0f; // cm
}

// Non-blocking RED/GREEN timer (phase only)
void trafficTick() {
  unsigned long now = millis();
  if (now - g_lastPhaseChange >= TRAFFIC_INTERVAL_MS) {
    g_lastPhaseChange = now;
    g_redPhase = !g_redPhase;    // flip phase every 15 s
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nHC-SR04 + traffic w/ restart-on-RED");

  pinMode(GREENLED, OUTPUT);
  pinMode(REDLED,   OUTPUT);
  pinMode(YELLOWLED,OUTPUT);
  pinMode(TOUCHSENS, INPUT);     // typical TTP223: HIGH when touched

  // start in GREEN phase
  g_redPhase = false;
  g_lastPhaseChange = millis();

  digitalWrite(GREENLED, HIGH);
  digitalWrite(REDLED,   LOW);
  digitalWrite(YELLOWLED,LOW);
}

void loop() {
  // Advance routine timer
  trafficTick();

  // Read sensors
  float d = readDistanceCm();
  bool touchActive = (digitalRead(TOUCHSENS) == HIGH);

  if (!isnan(d)) {
    Serial.printf("Distance: %.1f cm | touch:%d | phase:%s\n",
                  d, touchActive, g_redPhase ? "RED" : "GREEN");
  } else {
    Serial.println("timeout");
  }

  // --- Restart routine on RED when (touch OFF && distance < 10 cm) ---
  if (!touchActive && !isnan(d) && d < 10.0f) {
    g_redPhase = true;                 // switch to RED phase
    g_lastPhaseChange = millis();      // restart 15s timer from now
  }

  // Yellow ON only when: GREEN phase, touch ON, and distance < 10 cm
  bool yellowOn = (!g_redPhase) && touchActive && (!isnan(d)) && (d < 10.0f);

  // Drive LEDs according to current phase (after any restart)
  digitalWrite(REDLED,   g_redPhase ? HIGH : LOW);
  digitalWrite(GREENLED, g_redPhase ? LOW  : HIGH);
  digitalWrite(YELLOWLED, yellowOn ? HIGH : LOW);

  delay(60);   // sensor settle
  yield();     // keep WDT happy
}
