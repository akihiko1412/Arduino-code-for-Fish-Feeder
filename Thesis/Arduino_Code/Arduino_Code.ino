#include <MFRC522.h>
#include <Wire.h>
#include <DS18B20.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// Constants for different modes

#define AUTOMATIC_MODE 0
#define MANUAL_MODE 1
#define TESTING_MODE 2

#define SDA_PIN 10
#define RST_PIN 3
#define DS18B20WP_PIN_DQ 6
#define BUTTON_PIN 7
#define BUZZER_PIN 8
#define BUTTON_status 2


Servo servoMotor;
DS18B20 ds18b20wp(DS18B20WP_PIN_DQ);
RTC_DS3231 rtcDS;
LiquidCrystal_I2C lcd(0x27, 16, 2);



const float LowerThreshold = 25.0;
const float UpperThreshold = 27.0;

int relayPin1 = 5;
int relayPin2 = 4;
unsigned long lastButtonPressTime = 0;
const unsigned long debounceDelay = 3000;
unsigned long lastFeedingTime = 0;

// Add global variable declarations for EEPROM, rtcDS, and lcd

int feedingHour = 6; // Start with 6 AM
int nextFeedingHours = 18; // Set to 6 PM initially

unsigned long startTime = 0; // Variable to store the start time

int currentMode = AUTOMATIC_MODE; // Start with automatic mode

// Function declarations

void activateFeeder(float temperature);
void initializeRemainingSetup();
void activateBuzzer();
void followUpFeeder();
void setFeedingSchedule();
void displayFeedingComplete();
void displayFeeding6Hours();
void displayFeedisrunning();
void automaticFeeder(float temperature);
void printTimeAndTemperature(String formattedTime, float temperature);
void normalFeeding();
void halfFeeding();

bool followUpFeedingActivated = false;
bool isFollowUpFeedingCompleted = false;
bool firstRFIDActivation = true; // Flag to indicate first RFID activation

void setup() {
    Serial.begin(9600);
    SPI.begin();
    
    initializeRemainingSetup(); 
}   
void loop() {
      
      DateTime now = rtcDS.now();
    
      float ds18b20wpTempC = ds18b20wp.getTempC();
      float temperature; // Declare temperature variable

      String formattedTime = formatTime(now.hour(), now.minute(), now.second(), now.isPM());

      // To Maintain the data in APP put this every serial print
      String temperatureString = String(ds18b20wpTempC);
      printTimeAndTemperature(temperatureString, LowerThreshold, UpperThreshold);  
  
      lcd.setCursor(0, 0);
      lcd.print("Time: ");
      lcd.print(formattedTime);
      lcd.setCursor(0, 1);
      lcd.print("Temp: ");
      lcd.print(ds18b20wpTempC);
      lcd.print(" C");
      
    
    updateRelays(ds18b20wpTempC);
    handleFollowUpFeeding();
    handleSerialInput(ds18b20wpTempC);
    

     switch (currentMode) {
        case AUTOMATIC_MODE:
       if ((now.hour() == feedingHour && now.minute() == 0 && now.second() == 0) || (now.hour() == nextFeedingHours && now.minute() == 0 && now.second() == 0)) {
        activateBuzzer();
        automaticFeeder(ds18b20wpTempC);
  
        
       
    }
            break;
        case MANUAL_MODE:
            handleButtonPress(ds18b20wpTempC);
            break;
        case TESTING_MODE:
             // Testing mode logic...
            // Run all possible commands in automatic feeder
            float temperature = ds18b20wpTempC;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print(" Testing Feeder");
            lcd.setCursor(0, 1);
            lcd.print("   Automatic");
            delay(2000);
            automaticFeeder(ds18b20wpTempC);

            // Run all possible commands in activate feeder
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("  Testing Feeder");
            lcd.setCursor(0, 1);
            lcd.print("     Manual");
            delay(2000);
            lcd.clear();
        
        lcd.setCursor(0, 0);
        if (temperature < LowerThreshold) {
            lcd.print(" LOW Temperature");
            lcd.setCursor(0, 1);
            lcd.print(" Alarm Activated");
        } else if (temperature > UpperThreshold) {
            lcd.print("High Temperature");
            lcd.setCursor(0, 1);
            lcd.print(" Alarm Activated");
        } else {
            lcd.print(" Temperature is");
            lcd.setCursor(0, 1);
            lcd.print("     Normal");
        }

        activateBuzzer();
        activateFeeder(temperature);

        delay(5000); // Adjust the delay time as needed
        followUpFeedingActivated = false;
        
        lcd.clear();
        

            // Switch back to automatic mode after testing mode
            currentMode = AUTOMATIC_MODE;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("  Testing Done");
            lcd.setCursor(0, 1);
            lcd.print("");
            delay(2000);
            lcd.clear();

            
                    lcd.setCursor(0, 0);
                    lcd.print(" Going Back to");
                    lcd.setCursor(0, 1);
                    lcd.print(" Automatic Mode");
                    activateBuzzer();
                    
                    delay(2000);
                    lcd.clear();

                    // Display current feeding schedule
                    lcd.setCursor(0, 0);
                    lcd.print("Current feeding:");
                    lcd.setCursor(0, 1);
                    lcd.print("Time: ");
                    lcd.print(feedingHour);
                    lcd.print(" AM/PM");
                    delay(3000);

                    
                    lcd.clear();
                    
            break;
    }
} 
    
String formatTime(int hour, int minute, int second, bool isPM) {
    char timeBuffer[15];
    sprintf(timeBuffer, "%02d:%02d:%02d%s", hour % 12, minute, second, isPM ? "PM" : "AM");
    return String(timeBuffer);
}
bool compareUID(byte* array1, byte* array2, byte size) {
    for (byte i = 0; i < size; i++) {
        if (array1[i] != array2[i]) {
            return false;
        }
    }
    return true;
}
void handleSerialInput(float temperature) {
    while (Serial.available()) {
        String bluetooth = String(Serial.read());
        Serial.println(bluetooth);

        if (bluetooth == "48") {
          setFeedingSchedule();
          // To Maintain the data in APP put this every serial print
              String temperatureString = String(temperature);
              printTimeAndTemperature(temperatureString, LowerThreshold, UpperThreshold);
        }
        if (bluetooth == "50") {
          switch(currentMode) {
                case AUTOMATIC_MODE:
                    currentMode = MANUAL_MODE;
                    lcd.clear();
                    lcd.print("  Manual Mode");
                    break;
                case MANUAL_MODE:
                    currentMode = TESTING_MODE;
                    lcd.clear();
                    lcd.print("  Testing Mode");
                    break;
                case TESTING_MODE:
                    currentMode = AUTOMATIC_MODE; // Transition directly to automatic mode from testing mode
                    lcd.clear();
                    lcd.print("  Automatic Mode");
                    break;
            }
            // Beep sound
            activateBuzzer();
            // To Maintain the data in APP put this every serial print
              String temperatureString = String(temperature);
              printTimeAndTemperature(temperatureString, LowerThreshold, UpperThreshold);
            delay(2000);
        }
        if (bluetooth == "49") { // Assuming "49" is the command to trigger feeding

            if (temperature < LowerThreshold || temperature > UpperThreshold) {
                    lcd.clear();
        lcd.setCursor(0, 0);
        if (temperature < LowerThreshold) {
            lcd.print(" LOW Temperature");
            lcd.setCursor(0, 1);
            lcd.print(" Alarm Activated");
        } else if (temperature > UpperThreshold) {
            lcd.print("High Temperature");
            lcd.setCursor(0, 1);
            lcd.print(" Alarm Activated");
        } else {
            lcd.print(" Temperature is");
            lcd.setCursor(0, 1);
            lcd.print("     Normal");
        }

        // To Maintain the data in APP put this every serial print
              String temperatureString = String(temperature);
              printTimeAndTemperature(temperatureString, LowerThreshold, UpperThreshold);

        activateBuzzer();
        activateFeeder(temperature);

        delay(5000); // Adjust the delay time as needed
        
        lcd.clear();
        
        // Determine next feeding time based on temperature
            }
        }
    }
}
void handleButtonPress(float temperature) {
    if (digitalRead(BUTTON_PIN) == LOW && millis() - lastButtonPressTime > debounceDelay) {
        lastButtonPressTime = millis();
        lcd.clear();
        lcd.setCursor(0, 0);
        if (temperature < LowerThreshold) {
            lcd.print(" LOW Temperature");
            lcd.setCursor(0, 1);
            lcd.print(" Alarm Activated");
        } else if (temperature > UpperThreshold) {
            lcd.print("High Temperature");
            lcd.setCursor(0, 1);
            lcd.print(" Alarm Activated");
        } else {
            lcd.print(" Temperature is");
            lcd.setCursor(0, 1);
            lcd.print("     Normal");
        }

        activateBuzzer();
        activateFeeder(temperature);

        delay(5000); // Adjust the delay time as needed
        
        lcd.clear();
        
        // Determine next feeding time based on temperature
        
    }
}
void automaticFeeder(float temperature) {
    
    if (temperature >= LowerThreshold && temperature <= UpperThreshold) {
        displayFeedisrunning();
        normalfeeding();
        displayFeedingComplete();
        
        followUpFeedingActivated = false;

       
    } else {
        displayFeedisrunning();
        halfFeeding();
        
        
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Small Amount");
    
          

            lcd.setCursor(0, 1);
            lcd.print("Next in: ");
            lcd.print("6 hrs");
            delay(2000);

            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("  It will Run");
            lcd.setCursor(0, 1);
            lcd.print(" Automatically");
    

            lastFeedingTime = millis();
            followUpFeedingActivated = true;
    
    
        
    }
    servoMotor.write(90);
    delay(4000);
    lcd.clear();

    

    if (temperature >= LowerThreshold && temperature <= UpperThreshold) {
        if (startTime == 0) {
            startTime = millis();
        }
        if (millis() - startTime >= 60 * 1000) {
            startTime = 0;
        }
    }
}


void handleFollowUpFeeding() {
    if (followUpFeedingActivated && millis() - lastFeedingTime >= 21600000) {
        followUpFeeder();
        lastFeedingTime = millis();
        followUpFeedingActivated = false;
    }
}
void updateRelays(float temperature) {
    digitalWrite(relayPin1, temperature < LowerThreshold ? HIGH : LOW);
    digitalWrite(relayPin2, temperature > UpperThreshold ? HIGH : LOW);
    digitalWrite(BUTTON_status, temperature >= LowerThreshold && temperature <= UpperThreshold ? HIGH : LOW);
}
void activateFeeder(float temperature) {
    if (temperature >= LowerThreshold && temperature <= UpperThreshold) {
    displayFeedisrunning();
    normalfeeding();
    displayFeedingComplete();
    
    
    followUpFeedingActivated = false;
    } else {
    displayFeedisrunning();
    halfFeeding();
    displayFeeding6Hours();
    followUpFeedingActivated = true;
    
    }

    servoMotor.write(90); // Assuming 90 is the angle for halffeeding

    // Convert hours to milliseconds
    int nextFeedingMilliseconds = nextFeedingHours * 60 * 60 * 1000;

    // Calculate the time when the next feeding will occur
    unsigned long nextFeedingTime = millis() + nextFeedingMilliseconds;


      if (temperature >= LowerThreshold && temperature <= UpperThreshold) {
        if (startTime == 0) {
            startTime = millis();
        }
        if (millis() - startTime >= 60 * 1000) {
            startTime = 0;
        }
      }
}
void activateBuzzer() {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(1000);
}
void followUpFeeder() {

    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(3000);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Another Small");
    lcd.setCursor(0, 1);
    lcd.print("Amount of Feeds");
    
    

    halfFeeding();
    servoMotor.write(90);

    delay(1000);
    lcd.clear();

    isFollowUpFeedingCompleted = false;
}
void setFeedingSchedule() {
    // Reset follow-up feeding activation when setting new schedule
    followUpFeedingActivated = false;

    lcd.clear();
    lcd.print("    Adjust 1hr");
    lcd.setCursor(0, 1);
    lcd.print("   Feeding Time");
    delay(2000);
    

    // If it's the first RFID activation, set feedingHour to 7:00 AM
    if (firstRFIDActivation) {
        feedingHour = 7;
        nextFeedingHours = 19; // 7 PM for the next feeding
        firstRFIDActivation = false; // Set the flag to false after the first activation
    } else {
        // Increment feedingHour by 1 for subsequent activations
        feedingHour += 1;
        nextFeedingHours += 1;

        // Ensure feedingHour stays within 1-12 range
        if (feedingHour > 12) {
            feedingHour = 1;
        }

        // Ensure nextFeedingHours stays within 1-24 range
        if (nextFeedingHours > 24) {
            nextFeedingHours = 13;
        }
    }
  
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);

    lcd.clear();
    lcd.print("  New Schedule:");
    lcd.setCursor(0, 1);
    lcd.print("    ");
    lcd.print(feedingHour);
    lcd.print(" AM/PM");
    delay(2000);
    lcd.clear();
}
void displayFeedingComplete() {
    // Implementation for displaying feeding completion with seconds
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Feeds has been");
    lcd.setCursor(0, 1);
    lcd.print("  Fully Drop");
    delay(2000);
    lcd.clear();
}
void displayFeeding6Hours() {
    // Implementation for displaying feeding completion with hours
    lcd.clear();
    lcd.print("Feeding Complete");
    lcd.setCursor(0, 1);
    lcd.print("Next in ");
    lcd.print("6 hrs");
    
  }

void displayFeedisrunning() {
    // Implementation for displaying feeding completion with hours
    lcd.clear();
    lcd.print("Feeder Activated");
    
    }
void printTimeAndTemperature(String temperature, float lowerThreshold, float upperThreshold) {
    Serial.print(temperature);
    Serial.print("|");
    Serial.print(LowerThreshold);
    Serial.print("|");
    Serial.print(UpperThreshold);
    Serial.println("|");
}
void scrollText(String text[], int numRows, int duration) {
    int textLength = text[0].length(); // Assuming both strings have the same length
    int lcdWidth = 16; // Assuming 16x2 LCD

    for (int i = 0; i <= textLength - lcdWidth; i++) {
        lcd.clear();
        for (int j = 0; j < numRows; j++) {
            lcd.setCursor(0, j);
            lcd.print(text[j].substring(i, i + lcdWidth));
        }
        delay(duration);
    }

    // Print the remaining part of the text
    lcd.clear();
    for (int j = 0; j < numRows; j++) {
        lcd.setCursor(0, j);
        lcd.print(text[j].substring(textLength - lcdWidth));
    }
    delay(duration);
}
void displayStaticText(String staticText) {
    lcd.setCursor(0, 0); // Set cursor to the first row
    lcd.print(staticText);
}
void scrollText(String text, int row, int duration) {
    int textLength = text.length();
    int lcdWidth = 16; // Assuming 16 columns for your LCD

    for (int i = 0; i <= textLength - lcdWidth; i++) {
        lcd.setCursor(0, row); // Set cursor to the specified row
        lcd.print(text.substring(i, i + lcdWidth));
        delay(duration);
    }

    // Print the remaining part of the text
    lcd.setCursor(0, row); // Set cursor to the specified row
    lcd.print(text.substring(textLength - lcdWidth));
    delay(duration);
}
void initializeRemainingSetup() {
    // Initialize RTC
    rtcDS.begin();
    //rtcDS.adjust(DateTime(F(__DATE__), F(__TIME__)));
    servoMotor.write(90);
    if (rtcDS.lostPower()) {
        rtcDS.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    // Set RTC time for testing
    //rtcDS.adjust(DateTime(2024, 4, 27, 10, 57, 0)); // testing
    
    // Initialize other hardware
    pinMode(relayPin1, OUTPUT);
    pinMode(relayPin2, OUTPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(BUTTON_status, OUTPUT);

    // Initialize servo motor
    servoMotor.attach(9);
    

    // Display the current feeding schedule on the LCD
    lcd.begin();
    // Display static text on the first row
    displayStaticText("  THESIS TITLE:");
    // Scroll text on the second row
    scrollText("        Sensor-based aquarium with automated fish feeder        ", 1, 300); // Adjust duration as needed
    lcd.setCursor(0, 0);
    lcd.print("Current feeding:");
    lcd.setCursor(0, 1);
    lcd.print("Time: ");
    lcd.print(feedingHour);
    lcd.print(" AM/PM");
    delay(3000);
    lcd.clear();
    // Display Lower Threshold and Upper Threshold together
    String thresholds[] = {"          Lower limit: " + String(LowerThreshold) + " C                ", "          Upper limit: " + String(UpperThreshold) + " C                "};
    scrollText(thresholds, 2, 300); // Display on both rows, scrolling together
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" Automatic Mode");
    lcd.setCursor(0, 1);
    lcd.print(" about to Start");
    delay(2000);
    lcd.clear();
}
void normalfeeding() {
    for (int i = 0; i < 8; i++) { // Repeat the sequence 8 times
        servoMotor.write(180);
        delay(1000);
        servoMotor.write(0);
        delay(1000);
    }
}
void halfFeeding() {
    for (int i = 0; i < 4; i++) { // Repeat the sequence 4 times
        servoMotor.write(180);
        delay(1000);
        servoMotor.write(0);
        delay(1000);
    }
}