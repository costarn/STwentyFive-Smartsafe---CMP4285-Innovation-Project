#include "rgb_lcd.h"
#include <Adafruit_Fingerprint.h>
#if (defined(__AVR__) || defined(ESP8266)) && !defined(__AVR_ATmega2560__)
// For UNO and others without hardware serial, we must use software serial...
// pin #2 is IN from sensor (GREEN wire)
// pin #3 is OUT from arduino  (WHITE wire)
// Set up the serial port to use softwareserial..
SoftwareSerial mySerial(2, 3);

#endif

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

#include <Keypad.h>
#include <Servo.h>
#include "SafeState.h"
/* Locking mechanism definitions */ 
#define SERVO_PIN 5
#define SERVO_LOCK_POS   10
#define SERVO_UNLOCK_POS 95
Servo lockServo;
//reset function
void(* resetFunc) (void) = 0;
/* Display */
rgb_lcd lcd;

/* Keypad setup */
const byte KEYPAD_ROWS = 4;
const byte KEYPAD_COLS = 3;
byte rowPins[KEYPAD_ROWS] = {6, 7, 8, 9};
byte colPins[KEYPAD_COLS] = {10,12,13};
char keys[KEYPAD_ROWS][KEYPAD_COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

int fingerprintFailures = 0; // Global variable to track fingerprint failures

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, KEYPAD_ROWS, KEYPAD_COLS);

/* SafeState stores the secret code in EEPROM */
SafeState safeState;


void lock() {
  lockServo.write(SERVO_LOCK_POS);
  
}

void unlock() {
  lockServo.write(SERVO_UNLOCK_POS);
}

void showStartupMessage() {
  lcd.setCursor(4, 0);
  lcd.print("");
  delay(500);

  lcd.setCursor(0, 2);
  String message = "STwentyFive v1.8";
  for (byte i = 0; i < message.length(); i++) {
    lcd.print(message[i]);
    delay(100);
  }
  delay(500);
}

String inputSecretCode() {
  lcd.setCursor(5, 1);
  lcd.print("[____]");
  lcd.setCursor(6, 1);
  String result = "";
  while (result.length() < 4) {
    char key = keypad.getKey();
    if (key >= '0' && key <= '9') {
      lcd.print('*');
      result += key;
    }
  }
  return result;
}

void showWaitScreen(int delayMs) {
  lcd.setCursor(2, 1);
  lcd.print("[..........]");
  lcd.setCursor(3, 1);
  for (byte i = 0; i < 10; i++) {
    delay(delayMs);
    lcd.print("=");
  }
}

bool setNewCode() {
  lock();
  // Step 1: Verify the current code
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter Old Code:");
  String currentCode = inputSecretCode();

  if (!safeState.unlock(currentCode)) {
    lcd.clear();
    lcd.setCursor(1, 0);
    lcd.print("Wrong Code");
    lcd.setCursor(0, 1);
    lcd.print("Access Denied!");
    delay(2000);
    return false;
  }

  // Step 2: Enter the new code
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter New Code:");
  String newCode = inputSecretCode();

  // Step 3: Confirm the new code
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Confirm New Code:");
  String confirmCode = inputSecretCode();

  // Step 4: Check if the new code matches the confirmation
  if (newCode.equals(confirmCode)) {
    safeState.setCode(newCode);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Code Updated!");
    delay(2000);
    return true;
  } else {
    lcd.clear();
    lcd.setCursor(1, 0);
    lcd.print("Code Mismatch");
    lcd.setCursor(0, 1);
    lcd.print("Try Again!");
    delay(2000);
    return false;
  }
}


void showUnlockMessage() {
  lcd.clear();
  lcd.setCursor(0, 0);
  // lcd.write(ICON_UNLOCKED_CHAR);
  lcd.setCursor(0, 0);
  lcd.print("Code Accepted!");
  lcd.setCursor(15, 0);
  // lcd.write(ICON_UNLOCKED_CHAR);
  delay(1000);
}

void safeUnlockedLogic() {
  lcd.clear();

  lcd.setCursor(0, 0);
  // lcd.write(ICON_UNLOCKED_CHAR);
  lcd.setCursor(2, 0);
  lcd.print(" # to lock");
  lcd.setCursor(15, 0);
  // lcd.write(ICON_UNLOCKED_CHAR);

  bool newCodeNeeded = true;

  if (safeState.hasCode()) {
    lcd.setCursor(0, 1);
    lcd.print("  * = new code");
    newCodeNeeded = false;
  }

  auto key = keypad.getKey();
  while (key != '*' && key != '#') {
    key = keypad.getKey();
  }

  bool readyToLock = true;
  if (key == '*' || newCodeNeeded) {
    readyToLock = setNewCode();
  }

  if (readyToLock) {
    lcd.clear();
    lcd.setCursor(5, 0);
    // lcd.write(ICON_UNLOCKED_CHAR);
    lcd.print(" ");
    // lcd.write(ICON_RIGHT_ARROW);
    lcd.print(" ");
    // lcd.write(ICON_LOCKED_CHAR);

    safeState.lock();
    lock();
    showWaitScreen(100);
  }
  delay(1000);
}

void safeLockedLogic() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" Safe Locked! ");
  
  String userCode = inputSecretCode();
  bool unlockedSuccessfully = safeState.unlock(userCode);
  showWaitScreen(200);

  if (!unlockedSuccessfully) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Access Denied!");
    showWaitScreen(175);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Scan Fingerprint");
    delay(1500);
    
    unsigned long startTime = millis(); // Start time for fingerprint scanning
    bool fingerprintScanned = false; // Flag to track if fingerprint is scanned

    while (!fingerprintScanned && (millis() - startTime < 4500)) {
      if (getFingerprintID() == FINGERPRINT_OK) {
        fingerprintScanned = true; // Set flag to true if fingerprint is scanned successfully
      } else {
        delay(1000); // Wait for 1 second before checking again
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Contact Customer");
        lcd.setCursor(0, 1);
        lcd.print("Service");
        delay(1000);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("In Case of");
        lcd.setCursor(0, 1);
        lcd.print("Forgotten Code");
      }
    }

    // If fingerprint is not scanned within 8 seconds, reset the system
    if (!fingerprintScanned) {
      resetFunc();
    } else{
      delay(1000);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Failed ");
      lcd.setCursor(0, 1);
      lcd.print("Fingerprint");
    }
  }
}




void setup() {
  lcd.begin(16, 2);
  

  lockServo.attach(SERVO_PIN);

  /* Make sure the physical lock is sync with the EEPROM state */
  Serial.begin(115200);
  if (safeState.locked()) {
    lock();
  } else {
    unlock();
  }

  showStartupMessage();
  Serial.println("\n\n finger detect test");

  // set the data rate for the sensor serial port
  finger.begin(57600);
  delay(5);
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    while (1) { delay(1); }
  }
  finger.getParameters();

  finger.getTemplateCount();
}

void loop() {
  if (safeState.locked()) {
    safeLockedLogic();
    // getFingerprintID();
    // delay(50);

    lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Scan Fingerprint");

  // Wait until a finger is detected
  while (getFingerprintID() != FINGERPRINT_OK) {
    delay(500);
  }    
  } else {
    safeUnlockedLogic();
  }
}
  

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println("No finger detected");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!

  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Found a print match!");
    
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Did not find a match");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Fingerprint");
    lcd.setCursor(0, 1);
    lcd.print("Not Recognised!");
    delay(1000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Attempts Left:");
    lcd.setCursor(0, 1);
    lcd.print(3-fingerprintFailures);
    fingerprintFailures++; // Increment the failure counter
   
    if (fingerprintFailures > 2 ) { // Check if failures exceed the limit
      delay(1100);
      lcd.print(fingerprintFailures);
      safeState.lock();
      resetFunc();
    }
    
    

    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }
  
  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Fingerprint");
  lcd.setCursor(0, 1);
  lcd.print("Accepted!");
  fingerprintFailures = 0; // Reset the failure counter on success
  delay(2000);
  resetFunc();

  return finger.fingerID;
}








// uint8_t getFingerprintID() {
//   uint8_t p = finger.getImage();
//   switch (p) {
//     case FINGERPRINT_OK:
//       Serial.println("Image taken");
//       break;
//     case FINGERPRINT_NOFINGER:
//       Serial.println("No finger detected");
//       return p;
//     case FINGERPRINT_PACKETRECIEVEERR:
//       Serial.println("Communication error");
//       return p;
//     case FINGERPRINT_IMAGEFAIL:
//       Serial.println("Imaging error");
//       return p;
//     default:
//       Serial.println("Unknown error");
//       return p;
//   }

//   // OK success!
//   p = finger.image2Tz();
//   switch (p) {
//     case FINGERPRINT_OK:
//       Serial.println("Image converted");
//       break;
//     case FINGERPRINT_IMAGEMESS:
//       Serial.println("Image too messy");
//       return p;
//     case FINGERPRINT_PACKETRECIEVEERR:
//       Serial.println("Communication error");
//       return p;
//     case FINGERPRINT_FEATUREFAIL:
//       Serial.println("Could not find fingerprint features");
//       return p;
//     case FINGERPRINT_INVALIDIMAGE:
//       Serial.println("Could not find fingerprint features");
      
//       return p;
//     default:
//       Serial.println("Unknown error");
//       return p;
//   }

//   // OK converted!
//   p = finger.fingerSearch();
//   if (p == FINGERPRINT_OK) {
//     Serial.println("Found a print match!");
//   } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
//     Serial.println("Communication error");
//     return p;
//   } else if (p == FINGERPRINT_NOTFOUND) {
//     Serial.println("Did not find a match");
//     lcd.clear();
//     lcd.setCursor(0, 0);
//     lcd.print("Fingerprint");
//     lcd.setCursor(0, 1);
//     lcd.print("Not Recognised!");
//     return p;
//   } else {
//     Serial.println("Unknown error");
//     return p;
//   }
  
//   // found a match!
//   Serial.print("Found ID #"); Serial.print(finger.fingerID);
//   Serial.print(" with confidence of "); Serial.println(finger.confidence);
//   // UNLOCK funcction goes here
//   // unlock();
//   // delay(1000);
//   // lock();
//   lcd.clear();
//   lcd.setCursor(0, 0);
//   lcd.print("Fingerprint");
//   lcd.setCursor(0, 1);
//   lcd.print("Accepted!");
  
//   delay(2000);
//   resetFunc();
//   return finger.fingerID;
// }








// returns -1 if failed, otherwise returns ID #
int getFingerprintIDez() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)  return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)  return -1;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK)  return -1;

  // found a match!
  // UNLOCK funcction goes here
  unlock();
  delay(1000);
  lock();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Fingerprint");
  lcd.setCursor(0, 1);
  lcd.print("Accepted!");
  
  // delay(2000);
  // safeUnlockedLogic();
  // //lock();
  
  Serial.print("Found ID #"); Serial.print(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);
  
  return finger.fingerID;
}

