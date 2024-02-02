#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <AltSoftSerial.h>

const String PHONR = "+255768827609";

SoftwareSerial sim800(2, 3);
#define RELAY_PIN 7
AltSoftSerial neo6m;
TinyGPSPlus gps;

enum class SMSSendState {
    SetMode,
    SetRecipient,
    SendMessage,
    WaitForConfirmation,
    SMSComplete
};

boolean antiTheftActivated = false;
SMSSendState smsState = SMSSendState::SetMode;
unsigned long smsTimer = 0;
const unsigned long smsTimeout = 5000;
int smsCount = 0;

void setup() {
    Serial.begin(9600);
    Serial.println("Initializing...");
    sim800.begin(9600);
    neo6m.begin(9600);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);
    delay(3000);
    
    sim800.print("AT+CMGF=1\r");
    delay(1000);
    sendCommand("AT");
    delay(1000);
    sendCommand("AT+CREG");
    delay(1000);
}

void loop(){
    if(sim800.available()){
        char command = sim800.read();
        
        if(command == 'L') {
            sendLocationSMS();
        }else if (command == '1') {
            turnCarOn();
        }else if (command == '0') {
            turnCarOff();
        }else if(command == 'A') {
            activateAntiTheft();
        }else if (command == 'X') {
            deactivateAntiTheft();
        }
    }
    
    while (neo6m.available() > 0) {
        if (gps.encode(neo6m.read())) {
            if(gps.location.isUpdated()) {
                Serial.print("Latitude:");
                Serial.println(gps.location.lat(), 6);
                Serial.print("Longitude:");
                Serial.println(gps.location.lng(), 6);
            }
        }
    }
    if (smsCount >= 100) {
        deleteALLSMS();
        smsCount = 0;
    }
}
void sendCommand(const char* cmd){
  sim800.println(cmd);
  Serial.print("Sent command: ");
  Serial.println(cmd);
}


void readSerial() {
  while (sim800.available()) {
    char c = sim800.read();
    Serial.write(c);
  }
}
String getGPSLocation() {
    if(gps.location.isValid()) {
        String latitude = String(gps.location.lat(),6);
        String longitude = String(gps.location.lng(), 6);
        return "Latitude: " + latitude + ", Longitude: " +  longitude;
    } else  {
        return "GPS location not available";
    }
}
void turnCarOn() {
    if(antiTheftActivated) {
        sendSMS("Cannot turn on. Anti-theft system is active.");
    }else {
        digitalWrite(RELAY_PIN, HIGH);
        sendSMS("Car has been turn on..");
    }
}
void turnCarOff() {
    digitalWrite(RELAY_PIN, LOW);
    sendSMS("Car has been turn off..");
    
}
void activateAntiTheft() {
    antiTheftActivated = true;
    sendSMS("Anti-Theft system activated..");
}
void deactivateAntiTheft() {
    antiTheftActivated = false;
    sendSMS("Anti-Theft system deactivated..");
}
void deleteALLSMS() {
    sim800.println("AT+CMDGA=\"DEL ALL\"");
    delay(1000);
}
void sendLocationSMS() {
    Serial.println("Requesting GPS Location...");
    String location = getGPSLocation();
    sendSMS(location);
}
void sendSMS(const String&message) {
    switch(smsState) {
        case SMSSendState:: SetMode:
        sim800.println("AT+CMGF=1");
        smsTimer = millis();
        smsState = SMSSendState::SetRecipient;
        break;
        
        case SMSSendState::SetRecipient:
        sim800.println("AT+CMGS=\"+255768827609\"");
        smsTimer = millis();
        smsState = SMSSendState::SendMessage;
        break;
        
        case SMSSendState::SendMessage:
        sim800.print(message);
        sim800.println((char)26);
        smsTimer = millis();
        smsState = SMSSendState::WaitForConfirmation;
        break;
        
        case SMSSendState::WaitForConfirmation:
        if (millis()-smsTimer>=smsTimeout) {
            smsState = SMSSendState::SetMode;
            Serial.println("SMS sending timeout");
        }else {
            if (waitForResponce("OK", 5000)) {
                smsState= SMSSendState::SMSComplete;
            }else {
                smsState = SMSSendState::SetMode;
                Serial.println("Failed to send SMS");
            }
        }
        break;
        case SMSSendState::SMSComplete:
        smsState = SMSSendState::SetMode;
        break;
    }
}
boolean waitForResponce(const String&expectedResponce, unsigned long timeout) {
    unsigned long startTime = millis();
    String response = "";
    while (millis() - startTime < timeout) {
        if (sim800.available()) {
            response += sim800.read();
            if (response.indexOf(expectedResponce) != -1) {
                return true;
            }
        }
    }
    return false;
        
}
