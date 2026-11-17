// =====================================================
// SMART CPR FEEDBACK DEVICE (PEAK-DETECTION VERSION)
// Arduino Uno - Wokwi Simulation / Hardware Setup
// =====================================================

// ---------------- PIN DEFINITIONS ----------------
const int FORCE_SENSOR  = A0;  // FSR Voltage Divider (10k resistor to GND)
const int DEPTH_SENSOR  = A1;  // Depth Potentiometer
const int RESET_BUTTON  = 7;   // Reset Button

const int GREEN_LED  = 8;
const int YELLOW_LED = 9;
const int BUZZER     = 10;
const int RED_LED    = 11;


// ---------------- CPR THRESHOLDS ----------------
const int PUSH_START_THRESHOLD   = 150; // Force required to detect stroke start
const int PUSH_RELEASE_THRESHOLD = 70;  // Force level considered "fully released"

const int FORCE_MIN = 500; // Minimum force requirement

// Correct & Moderate depth ranges
const int DEPTH_MIN     = 450;
const int DEPTH_MAX     = 850;
const int MOD_DEPTH_MIN = 400;
const int MOD_DEPTH_MAX = 500;

// Compression rate ranges (CPM / BPM)
const float RATE_MIN     = 100.0; // Correct range: 100 to 130 CPM
const float RATE_MAX     = 130.0; // Updated to include up to 130 CPM
const float MOD_RATE_MIN = 80.0;  // Moderate range: 80-100 or 130-140 CPM
const float MOD_RATE_MAX = 140.0;


// ---------------- STATE & VARIABLES ----------------
enum CompressionState { IDLE, COMPRESSING };
CompressionState state = IDLE;

int compressionCount = 0;
unsigned long lastStrokeTime = 0;
float compressionRate = 0;

// Variables to capture peak values during a single stroke
int peakForce = 0;
int peakDepth = 0;


// =====================================================
// SETUP
// =====================================================
void setup()
{
  pinMode(RESET_BUTTON, INPUT_PULLUP);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  Serial.begin(9600);

  // Initial LED and Buzzer reset
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(RED_LED, LOW);
  noTone(BUZZER);

  Serial.println("======================================");
  Serial.println("   SMART CPR DEVICE (PEAK DETECTION)  ");
  Serial.println("======================================");
  Serial.println("Ready for CPR compressions...");
}


// =====================================================
// MAIN LOOP (STATE MACHINE)
// =====================================================
void loop()
{
  // 1. Check Reset button
  if (digitalRead(RESET_BUTTON) == LOW)
  {
    resetSession();
    delay(300);
  }

  // Read current live sensor values
  int liveForce = analogRead(FORCE_SENSOR);
  int liveDepth = analogRead(DEPTH_SENSOR);

  // 2. State Machine for Compression Peak Detection
  switch (state)
  {
    case IDLE:
      // Detect start of downward stroke
      if (liveForce >= PUSH_START_THRESHOLD)
      {
        state = COMPRESSING;
        peakForce = liveForce;
        peakDepth = liveDepth;
      }
      break;

    case COMPRESSING:
      // Record maximum peak values reached during the press
      if (liveForce > peakForce) peakForce = liveForce;
      if (liveDepth > peakDepth) peakDepth = liveDepth;

      // Detect stroke release
      if (liveForce < PUSH_RELEASE_THRESHOLD)
      {
        processCompletedCompression(peakForce, peakDepth);
        state = IDLE; // Reset back to IDLE state
      }
      break;
  }
}


// =====================================================
// PROCESS COMPLETED STROKE
// =====================================================
void processCompletedCompression(int maxForce, int maxDepth)
{
  unsigned long currentTime = millis();

  // Calculate compression rate (Compressions Per Minute)
  if (lastStrokeTime != 0)
  {
    unsigned long interval = currentTime - lastStrokeTime;
    if (interval > 0)
    {
      compressionRate = 60000.0 / interval;
    }
  }
  lastStrokeTime = currentTime;
  compressionCount++;

  // --- COMPLIANCE CHECKS ---
  bool forceOk = (maxForce >= FORCE_MIN);

  bool depthOk  = (maxDepth >= DEPTH_MIN && maxDepth <= DEPTH_MAX);
  bool depthMod = (maxDepth >= MOD_DEPTH_MIN && maxDepth <= MOD_DEPTH_MAX);

  bool rateOk  = true;
  bool rateMod = true;

  // First compression has no rate calculation yet
  if (compressionCount > 1)
  {
    rateOk  = (compressionRate >= RATE_MIN && compressionRate <= RATE_MAX);
    rateMod = (compressionRate >= MOD_RATE_MIN && compressionRate <= MOD_RATE_MAX);
  }

  // --- PRINT LOGS ---
  Serial.println("--------------------------------------");
  Serial.print("Compression # : "); Serial.println(compressionCount);
  Serial.print("Peak Force    : "); Serial.println(maxForce);
  Serial.print("Peak Depth    : "); Serial.println(maxDepth);

  if (compressionCount == 1) {
    Serial.println("Rate          : First stroke (timing initialized)");
  } else {
    Serial.print("Rate          : "); Serial.print(compressionRate, 1); Serial.println(" CPM");
  }

  // --- FEEDBACK OUTPUTS ---

  // GREEN - Ideal CPR Technique
  if (forceOk && depthOk && rateOk)
  {
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(RED_LED, LOW);
    tone(BUZZER, 1200, 80); // High short beep

    Serial.println("STATUS        : GREEN (Good CPR)");
  }
  // YELLOW - Acceptable / Needs Minor Adjustment
  else if (forceOk && (depthOk || depthMod) && (rateOk || rateMod))
  {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(YELLOW_LED, HIGH);
    digitalWrite(RED_LED, LOW);
    tone(BUZZER, 700, 150); // Medium beep

    Serial.println("STATUS        : YELLOW (Needs Improvement)");
    if (!depthOk) Serial.println("Diagnostic    : Depth in moderate range (400-500)");
    if (!rateOk && compressionCount > 1) Serial.println("Diagnostic    : Rate in moderate range (80-100 or 130-140 CPM)");
  }
  // RED - Incorrect CPR Technique
  else
  {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(RED_LED, HIGH);
    tone(BUZZER, 350, 250); // Low warning tone

    Serial.println("STATUS        : RED (Incorrect Technique)");
    
    // Print explicit diagnostics to identify why RED was triggered
    if (!forceOk) 
      Serial.println("Diagnostic    : Peak Force too low (< 500)");
    if (!depthOk && !depthMod) 
      Serial.println("Diagnostic    : Peak Depth out of range (< 400 or > 850)");
    if (!rateOk && !rateMod && compressionCount > 1) 
      Serial.println("Diagnostic    : Compression Rate out of range (< 80 or > 140 CPM)");
  }

  Serial.println("--------------------------------------\n");
}


// =====================================================
// RESET SESSION
// =====================================================
void resetSession()
{
  compressionCount = 0;
  lastStrokeTime = 0;
  compressionRate = 0;
  state = IDLE;

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(RED_LED, LOW);
  noTone(BUZZER);

  Serial.println("\n======================================");
  Serial.println("          SESSION RESET               ");
  Serial.println("======================================\n");
}
