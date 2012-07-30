/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Hal.h"
#include "nsITimer.h"

#include "smslib.h"
#define MEAN_GRAVITY 9.80665
#define DEFAULT_SENSOR_POLL 100

using namespace mozilla::hal;

namespace mozilla {
namespace hal_impl {

static nsITimer* sUpdateTimer = nullptr;

void UpdateHandler(nsITimer *aTimer, void *aClosure)
{
  sms_acceleration accel;
  smsGetData(&accel);

  InfallibleTArray<float> values;
  values.AppendElement(accel.x * MEAN_GRAVITY);
  values.AppendElement(accel.y * MEAN_GRAVITY);
  values.AppendElement(accel.z * MEAN_GRAVITY);
  hal::SensorData sdata(hal::SENSOR_ACCELERATION,
			PR_Now(),
			values,
			hal::SENSOR_ACCURACY_UNKNOWN);
  hal::NotifySensorChange(sdata);
}

void
EnableSensorNotifications(SensorType aSensor)
{
  if (aSensor != SENSOR_ACCELERATION)
    return;

  if (sUpdateTimer)
    return;

  smsStartup(nil, nil);
  smsLoadCalibration();

  CallCreateInstance("@mozilla.org/timer;1", &sUpdateTimer);
  if (sUpdateTimer)
    sUpdateTimer->InitWithFuncCallback(UpdateHandler,
                                       NULL,
                                       DEFAULT_SENSOR_POLL,
                                       nsITimer::TYPE_REPEATING_SLACK);
}

void
DisableSensorNotifications(SensorType aSensor)
{
  if (aSensor != SENSOR_ACCELERATION)
    return;

  if (sUpdateTimer) {
    sUpdateTimer->Cancel();
    NS_RELEASE(sUpdateTimer);
  }
  smsShutdown();
}

} // hal_impl
} // mozilla
