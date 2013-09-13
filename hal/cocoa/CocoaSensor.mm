/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "Hal.h"
#include "nsITimer.h"
#include "smslib.h"

#include <mach/mach.h>
#include <cmath>
#import <IOKit/IOKitLib.h>

#define MEAN_GRAVITY 9.80665
#define DEFAULT_SENSOR_POLL 100
using namespace mozilla::hal;
namespace mozilla {
namespace hal_impl {
static nsITimer* sUpdateTimer = nullptr;
static bool sActiveSensors[NUM_SENSOR_TYPE];
static io_connect_t sDataPort = IO_OBJECT_NULL;
static uint64_t sLastMean = -1;
static float
LMUvalueToLux(uint64_t aValue)
{
  //Conversion formula from regression. See Bug 793728.
  // -3*(10^-27)*x^4 + 2.6*(10^-19)*x^3 + -3.4*(10^-12)*x^2 + 3.9*(10^-5)*x - 0.19
  long double powerC4 = 1/pow((long double)10,27);
  long double powerC3 = 1/pow((long double)10,19);
  long double powerC2 = 1/pow((long double)10,12);
  long double powerC1 = 1/pow((long double)10,5);

  long double term4 = -3.0 * powerC4 * pow(aValue,4);
  long double term3 =  2.6 * powerC3 * pow(aValue,3);
  long double term2 = -3.4 * powerC2 * pow(aValue,2);
  long double term1 =  3.9 * powerC1 * aValue;

  float lux = ceil(static_cast<float>(term4 + term3 + term2 + term1 - 0.19));
  return lux > 0 ? lux : 0;
}
void
UpdateHandler(nsITimer *aTimer, void *aClosure)
{
  for (int i = 0; i < NUM_SENSOR_TYPE; i++) {
    if (!sActiveSensors[i]) {
      continue;
    }
    SensorType sensor = static_cast<SensorType>(i);
    InfallibleTArray<float> values;
    if (sensor == SENSOR_ACCELERATION) {
      sms_acceleration accel;
      smsGetData(&accel);

      values.AppendElement(accel.x * MEAN_GRAVITY);
      values.AppendElement(accel.y * MEAN_GRAVITY);
      values.AppendElement(accel.z * MEAN_GRAVITY);
    } else if (sensor == SENSOR_LIGHT && sDataPort != IO_OBJECT_NULL) {
      kern_return_t kr;
      uint32_t outputs = 2;
      uint64_t lightLMU[outputs];

      kr = IOConnectCallMethod(sDataPort, 0, nil, 0, nil, 0, lightLMU, &outputs, nil, 0);
      if (kr == KERN_SUCCESS) {
        uint64_t mean = (lightLMU[0] + lightLMU[1]) / 2;
        if (mean == sLastMean) {
          continue;
        }
        sLastMean = mean;
        values.AppendElement(LMUvalueToLux(mean));
      } else if (kr == kIOReturnBusy) {
        continue;
      }
    }

    hal::SensorData sdata(sensor,
                          PR_Now(),
                          values,
                          hal::SENSOR_ACCURACY_UNKNOWN);
    hal::NotifySensorChange(sdata);
  }
}
void
EnableSensorNotifications(SensorType aSensor)
{
  if (aSensor == SENSOR_ACCELERATION) {
    int result = smsStartup(nil, nil);

    if (result != SMS_SUCCESS) {
      return;
    }

    if (!smsLoadCalibration()) {
      return;
    }
  } else if (aSensor == SENSOR_LIGHT) {
    io_service_t serviceObject;
    serviceObject = IOServiceGetMatchingService(kIOMasterPortDefault,
                                                IOServiceMatching("AppleLMUController"));
    if (!serviceObject) {
      return;
    }
    kern_return_t kr;
    kr = IOServiceOpen(serviceObject, mach_task_self(), 0, &sDataPort);
    IOObjectRelease(serviceObject);
    if (kr != KERN_SUCCESS) {
      return;
    }
  } else {
    NS_WARNING("EnableSensorNotifications called on an unknown sensor type");
    return;
  }
  sActiveSensors[aSensor] = true;

  if (!sUpdateTimer) {
    CallCreateInstance("@mozilla.org/timer;1", &sUpdateTimer);
    if (sUpdateTimer) {
        sUpdateTimer->InitWithFuncCallback(UpdateHandler,
                                           nullptr,
                                           DEFAULT_SENSOR_POLL,
                                           nsITimer::TYPE_REPEATING_SLACK);
    }
  }
}
void
DisableSensorNotifications(SensorType aSensor)
{
  if (!sActiveSensors[aSensor] || (aSensor != SENSOR_ACCELERATION && aSensor != SENSOR_LIGHT)) {
    return;
  }

  sActiveSensors[aSensor] = false;

  if (aSensor == SENSOR_ACCELERATION) {
    smsShutdown();
  } else if (aSensor == SENSOR_LIGHT) {
    IOServiceClose(sDataPort);
  }
  // If all sensors are disabled, cancel the update timer.
  if (sUpdateTimer) {
    for (int i = 0; i < NUM_SENSOR_TYPE; i++) {
      if (sActiveSensors[i]) {
        return;
      }
    }
    sUpdateTimer->Cancel();
    NS_RELEASE(sUpdateTimer);
  }
}
} // hal_impl
} // mozilla
