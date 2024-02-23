#include <RTClib.h>

RTC_DS3231 rtc;

// Configurable constants

int FAN_PIN = 52;

int L_FS = 2;
int L_B = 4;
int L_W = 6;
int R_B = 8;
int R_FS = 10;
int R_W = 12;

struct sun_time {
    int sunRiseMinutes;
    int sunSetMinutes;
};

struct sun_time sun_time_months[] = {
    {.sunRiseMinutes = 7 * 60 + 38, .sunSetMinutes = 16 * 60 + 29 },
    {.sunRiseMinutes = 6 * 60 + 59, .sunSetMinutes = 17 * 60 + 17 },
    {.sunRiseMinutes = 6 * 60 + 6, .sunSetMinutes = 18 * 60 },
    {.sunRiseMinutes = 6 * 60 + 3, .sunSetMinutes = 19 * 60 + 45 },
    {.sunRiseMinutes = 5 * 60 + 13, .sunSetMinutes = 20 * 60 + 28 },
    {.sunRiseMinutes = 4 * 60 + 51, .sunSetMinutes = 20 * 60 + 58},
    {.sunRiseMinutes = 5 * 60 + 7, .sunSetMinutes = 20 * 60 + 53},
    {.sunRiseMinutes = 5 * 60 + 46, .sunSetMinutes = 20 * 60 + 11},
    {.sunRiseMinutes = 6 * 60 + 29, .sunSetMinutes = 19 * 60 + 10},
    {.sunRiseMinutes = 7 * 60 + 12, .sunSetMinutes = 18 * 60 + 8},
    {.sunRiseMinutes = 6 * 60 + 59, .sunSetMinutes = 16 * 60 + 18},
    {.sunRiseMinutes = 7 * 60 + 36, .sunSetMinutes = 16 * 60 + 2},
};

int month_days[] = {
  31,
  28,
  31,
  30,
  31,
  30,
  31,
  31,
  30,
  31,
  30,
  31
};

struct led_setting {
    double blue;
    double white;
    double full_spectrum;
};

struct led_setting_time {
    int time_percent;
    struct led_setting led_setting;
};

struct led_setting_time led_setting_times[] = {
    {.time_percent = 0, .led_setting = {.blue = 0, .white = 0, .full_spectrum = 0 } },
    {.time_percent = 15, .led_setting = {.blue = 180, .white = 80, .full_spectrum = 60 } },
    {.time_percent = 30, .led_setting = {.blue = 150, .white = 100, .full_spectrum = 80 } },
    {.time_percent = 60, .led_setting = {.blue = 150, .white = 120, .full_spectrum = 120 } },
    {.time_percent = 85, .led_setting = {.blue = 180, .white = 60, .full_spectrum =  100 } },
    {.time_percent = 92, .led_setting = {.blue = 120, .white = 30, .full_spectrum = 80 } },
    {.time_percent = 100, .led_setting = {.blue = 0, .white = 0, .full_spectrum = 0 } },
};

int lightingMin = 690;
int lightingMax = 750;


// Calculated constants
int daylightMin = 10000;
int daylightMax = 0;

double lightingModifierX;
double lightingModifierY;


// Daily calculated constants
int calculatedDay;
double currentLightingRise;
double currentLightingSet;


// Helper functions
double adjustLightingTime(double dayLightTime) {
    return dayLightTime * lightingModifierX + lightingModifierY;
}

double calculateCurrentLightingRise(int month, int day) {
    int thisMonthDays = month_days[month - 1];

    int thisMonthSunRise = sun_time_months[month - 1].sunRiseMinutes;
    int thisMonthSunSet = sun_time_months[month - 1].sunSetMinutes;

    int nextMonthSunRise = sun_time_months[month % 12].sunRiseMinutes;
    int nextMonthSunSet = sun_time_months[month % 12].sunSetMinutes;

    double currentSunRise = thisMonthSunRise * ((double)(thisMonthDays - day) / thisMonthDays) + nextMonthSunRise * ((double)day / thisMonthDays);
    double currentSunSet = thisMonthSunSet * ((double)(thisMonthDays - day) / thisMonthDays) + nextMonthSunSet * ((double)day / thisMonthDays);

    double currentLightingTime = adjustLightingTime(currentSunSet - currentSunRise);
    double lightingTimeOffset = (currentSunSet - currentSunRise) - currentLightingTime;

    return currentSunRise + lightingTimeOffset;
}

double calculateCurrentLightingSet(int month, int day) {
    int thisMonthDays = month_days[month - 1];

    int thisMonthSunRise = sun_time_months[month - 1].sunRiseMinutes;
    int thisMonthSunSet = sun_time_months[month - 1].sunSetMinutes;

    int nextMonthSunRise = sun_time_months[month % 12].sunRiseMinutes;
    int nextMonthSunSet = sun_time_months[month % 12].sunSetMinutes;

    double currentSunRise = thisMonthSunRise * ((double)(thisMonthDays - day) / thisMonthDays) + nextMonthSunRise * ((double)day / thisMonthDays);
    double currentSunSet = thisMonthSunSet * ((double)(thisMonthDays - day) / thisMonthDays) + nextMonthSunSet * ((double)day / thisMonthDays);


    double currentLightingTime = adjustLightingTime(currentSunSet - currentSunRise);
    double lightingTimeOffset = (currentSunSet - currentSunRise) - currentLightingTime;

    return currentSunSet - lightingTimeOffset;
}

void initializeConstants(int month, int day) {
    for (int i = 0; i < 12; ++i) {
        int sunRise = sun_time_months[i].sunRiseMinutes;
        int sunSet = sun_time_months[i].sunSetMinutes;
        daylightMin = sunSet - sunRise < daylightMin ? sunSet - sunRise : daylightMin;
        daylightMax = sunSet - sunRise > daylightMax ? sunSet - sunRise : daylightMax;
    }

    lightingModifierX = (double)(lightingMin - lightingMax) / (double)(daylightMin - daylightMax);
    lightingModifierY = (double)lightingMin - (double)daylightMin * lightingModifierX;

    currentLightingRise = calculateCurrentLightingRise(month, day) + 60;
    currentLightingSet = calculateCurrentLightingSet(month, day) + 60;
}

struct led_setting_time findPreviousLedSettings(double lightTimePercentage) {
    int led_setting_times_size = sizeof(led_setting_times) / sizeof(led_setting_times[0]);

    for (int i = 0; i < led_setting_times_size; ++i) {
        int time_percent = led_setting_times[i].time_percent;
        if (time_percent > lightTimePercentage) {
            return led_setting_times[i - 1];
        }
    }
}

struct led_setting_time findNextLedSettings(double lightTimePercentage) {
    int led_setting_times_size = sizeof(led_setting_times) / sizeof(led_setting_times[0]);

    for (int i = led_setting_times_size - 1; i >= 0; --i) {
        int time_percent = led_setting_times[i].time_percent;
        if (time_percent < lightTimePercentage) {
            return led_setting_times[i + 1];
        }
    }
}

struct led_setting calculateCurrentLedSettings(int hour, int minute) {
    int currentMinute = hour * 60 + minute;

    double lightTimeMinutes = currentMinute - currentLightingRise;

    double lightTimePercentage = (lightTimeMinutes / (currentLightingSet - currentLightingRise)) * 100;

    if (lightTimePercentage < 0) {
        lightTimePercentage = 0;
    }

    if (lightTimePercentage > 100) {
        lightTimePercentage = 100;
    }

    struct led_setting_time previousLedSettings = findPreviousLedSettings(lightTimePercentage);
    struct led_setting_time nextLedSettings = findNextLedSettings(lightTimePercentage);


    double lightTimePercentageIntoPeriod = lightTimePercentage - previousLedSettings.time_percent;
    double lightTimePercentagePeriodDifference = nextLedSettings.time_percent - previousLedSettings.time_percent;

    if (lightTimePercentagePeriodDifference == 0) {
        return previousLedSettings.led_setting;
    }

    double currentBlue = previousLedSettings.led_setting.blue * ((lightTimePercentagePeriodDifference - lightTimePercentageIntoPeriod) / lightTimePercentagePeriodDifference) + nextLedSettings.led_setting.blue * (lightTimePercentageIntoPeriod / lightTimePercentagePeriodDifference);
    double currentWhite = previousLedSettings.led_setting.white * ((lightTimePercentagePeriodDifference - lightTimePercentageIntoPeriod) / lightTimePercentagePeriodDifference) + nextLedSettings.led_setting.white * (lightTimePercentageIntoPeriod / lightTimePercentagePeriodDifference);
    double currentFullSpectrum = previousLedSettings.led_setting.full_spectrum * ((lightTimePercentagePeriodDifference - lightTimePercentageIntoPeriod) / lightTimePercentagePeriodDifference) + nextLedSettings.led_setting.full_spectrum * (lightTimePercentageIntoPeriod / lightTimePercentagePeriodDifference);

    struct led_setting current_settings = {
        .blue = currentBlue, .white = currentWhite, .full_spectrum = currentFullSpectrum
    };
    return current_settings;
}


// Arduino code

void setup() {
  Serial.begin(115200);

  rtc.begin();
  rtc.disable32K();

  pinMode(FAN_PIN, OUTPUT);
  
  DateTime now = rtc.now();
  initializeConstants(now.month(), now.day());

  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
}

void loop() {
    DateTime now = rtc.now();

    //Serial.println("-------");
    struct led_setting current_led_settings = calculateCurrentLedSettings(now.hour(), now.minute());
    bool isFanOn = current_led_settings.white > 0 || current_led_settings.blue > 0 || current_led_settings.full_spectrum > 0;
    
    analogWrite(L_W, current_led_settings.white);
    analogWrite(R_W, current_led_settings.white);
    analogWrite(L_B, current_led_settings.blue);
    analogWrite(R_B, current_led_settings.blue);
    analogWrite(L_FS, current_led_settings.full_spectrum);
    analogWrite(R_FS, current_led_settings.full_spectrum);

    Serial.print(now.hour());
    Serial.print(":");
    Serial.print(now.minute());
    Serial.print(" ");
    Serial.print("White: ");
    Serial.print(current_led_settings.white);
    Serial.print(", Blue: ");
    Serial.print(current_led_settings.blue);
    Serial.print(", Full spectrum: ");
    Serial.print(current_led_settings.full_spectrum);
    Serial.print(", Fan: ");
    Serial.println(isFanOn);

    if(isFanOn) {
      digitalWrite(FAN_PIN, HIGH);
    } else {
      digitalWrite(FAN_PIN, LOW);
    }
    delay(10000);
}
