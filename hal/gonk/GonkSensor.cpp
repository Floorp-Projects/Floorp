/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <pthread.h>
#include <stdio.h>

#include "mozilla/DebugOnly.h"
#include "mozilla/Saturate.h"

#include "base/basictypes.h"
#include "base/thread.h"
#include "base/task.h"

#include "GonkSensorsInterface.h"
#include "GonkSensorsPollInterface.h"
#include "GonkSensorsRegistryInterface.h"
#include "Hal.h"
#include "HalLog.h"
#include "HalSensor.h"
#include "hardware/sensors.h"
#include "nsThreadUtils.h"

using namespace mozilla::hal;

namespace mozilla {

//
// Internal implementation
//

// The value from SensorDevice.h (Android)
#define DEFAULT_DEVICE_POLL_RATE 200000000 /*200ms*/
// ProcessOrientation.cpp needs smaller poll rate to detect delay between
// different orientation angles
#define ACCELEROMETER_POLL_RATE 66667000 /*66.667ms*/

// This is present in Android from API level 18 onwards, which is 4.3.  We might
// be building on something before 4.3, so use a local define for its value
#define MOZ_SENSOR_TYPE_GAME_ROTATION_VECTOR   15

double radToDeg(double a) {
  return a * (180.0 / M_PI);
}

static SensorType
HardwareSensorToHalSensor(int type)
{
  switch(type) {
    case SENSOR_TYPE_ORIENTATION:
      return SENSOR_ORIENTATION;
    case SENSOR_TYPE_ACCELEROMETER:
      return SENSOR_ACCELERATION;
    case SENSOR_TYPE_PROXIMITY:
      return SENSOR_PROXIMITY;
    case SENSOR_TYPE_LIGHT:
      return SENSOR_LIGHT;
    case SENSOR_TYPE_GYROSCOPE:
      return SENSOR_GYROSCOPE;
    case SENSOR_TYPE_LINEAR_ACCELERATION:
      return SENSOR_LINEAR_ACCELERATION;
    case SENSOR_TYPE_ROTATION_VECTOR:
      return SENSOR_ROTATION_VECTOR;
    case MOZ_SENSOR_TYPE_GAME_ROTATION_VECTOR:
      return SENSOR_GAME_ROTATION_VECTOR;
    default:
      return SENSOR_UNKNOWN;
  }
}

static SensorAccuracyType
HardwareStatusToHalAccuracy(int status) {
  return static_cast<SensorAccuracyType>(status);
}

static int
HalSensorToHardwareSensor(SensorType type)
{
  switch(type) {
    case SENSOR_ORIENTATION:
      return SENSOR_TYPE_ORIENTATION;
    case SENSOR_ACCELERATION:
      return SENSOR_TYPE_ACCELEROMETER;
    case SENSOR_PROXIMITY:
      return SENSOR_TYPE_PROXIMITY;
    case SENSOR_LIGHT:
      return SENSOR_TYPE_LIGHT;
    case SENSOR_GYROSCOPE:
      return SENSOR_TYPE_GYROSCOPE;
    case SENSOR_LINEAR_ACCELERATION:
      return SENSOR_TYPE_LINEAR_ACCELERATION;
    case SENSOR_ROTATION_VECTOR:
      return SENSOR_TYPE_ROTATION_VECTOR;
    case SENSOR_GAME_ROTATION_VECTOR:
      return MOZ_SENSOR_TYPE_GAME_ROTATION_VECTOR;
    default:
      return -1;
  }
}

static int
SensorseventStatus(const sensors_event_t& data)
{
  int type = data.type;
  switch(type) {
    case SENSOR_ORIENTATION:
      return data.orientation.status;
    case SENSOR_LINEAR_ACCELERATION:
    case SENSOR_ACCELERATION:
      return data.acceleration.status;
    case SENSOR_GYROSCOPE:
      return data.gyro.status;
  }

  return SENSOR_STATUS_UNRELIABLE;
}

class SensorRunnable : public Runnable
{
public:
  SensorRunnable(const sensors_event_t& data, const sensor_t* sensors, ssize_t size)
  {
    mSensorData.sensor() = HardwareSensorToHalSensor(data.type);
    mSensorData.accuracy() = HardwareStatusToHalAccuracy(SensorseventStatus(data));
    mSensorData.timestamp() = data.timestamp;
    if (mSensorData.sensor() == SENSOR_GYROSCOPE) {
      // libhardware returns gyro as rad.  convert.
      mSensorValues.AppendElement(radToDeg(data.data[0]));
      mSensorValues.AppendElement(radToDeg(data.data[1]));
      mSensorValues.AppendElement(radToDeg(data.data[2]));
    } else if (mSensorData.sensor() == SENSOR_PROXIMITY) {
      mSensorValues.AppendElement(data.data[0]);
      mSensorValues.AppendElement(0);

      // Determine the maxRange for this sensor.
      for (ssize_t i = 0; i < size; i++) {
        if (sensors[i].type == SENSOR_TYPE_PROXIMITY) {
          mSensorValues.AppendElement(sensors[i].maxRange);
        }
      }
    } else if (mSensorData.sensor() == SENSOR_LIGHT) {
      mSensorValues.AppendElement(data.data[0]);
    } else if (mSensorData.sensor() == SENSOR_ROTATION_VECTOR) {
      mSensorValues.AppendElement(data.data[0]);
      mSensorValues.AppendElement(data.data[1]);
      mSensorValues.AppendElement(data.data[2]);
      if (data.data[3] == 0.0) {
        // data.data[3] was optional in Android <= API level 18.  It can be computed from 012,
        // but it's better to take the actual value if one is provided.  The computation is
        //   v = 1 - d[0]*d[0] - d[1]*d[1] - d[2]*d[2]
        //   d[3] = v > 0 ? sqrt(v) : 0;
        // I'm assuming that it will be 0 if it's not passed in.  (The values form a unit
        // quaternion, so the angle can be computed from the direction vector.)
        float sx = data.data[0], sy = data.data[1], sz = data.data[2];
        float v = 1.0f - sx*sx - sy*sy - sz*sz;
        mSensorValues.AppendElement(v > 0.0f ? sqrt(v) : 0.0f);
      } else {
        mSensorValues.AppendElement(data.data[3]);
      }
    } else if (mSensorData.sensor() == SENSOR_GAME_ROTATION_VECTOR) {
      mSensorValues.AppendElement(data.data[0]);
      mSensorValues.AppendElement(data.data[1]);
      mSensorValues.AppendElement(data.data[2]);
      mSensorValues.AppendElement(data.data[3]);
    } else {
      mSensorValues.AppendElement(data.data[0]);
      mSensorValues.AppendElement(data.data[1]);
      mSensorValues.AppendElement(data.data[2]);
    }
    mSensorData.values() = mSensorValues;
  }

  ~SensorRunnable() {}

  NS_IMETHOD Run() override
  {
    NotifySensorChange(mSensorData);
    return NS_OK;
  }

private:
  SensorData mSensorData;
  AutoTArray<float, 4> mSensorValues;
};

namespace hal_impl {

static DebugOnly<int> sSensorRefCount[NUM_SENSOR_TYPE];
static base::Thread* sPollingThread;
static sensors_poll_device_t* sSensorDevice;
static sensors_module_t* sSensorModule;

static void
PollSensors()
{
  const size_t numEventMax = 16;
  sensors_event_t buffer[numEventMax];
  const sensor_t* sensors;
  int size = sSensorModule->get_sensors_list(sSensorModule, &sensors);

  do {
    // didn't check sSensorDevice because already be done on creating pollingThread.
    int n = sSensorDevice->poll(sSensorDevice, buffer, numEventMax);
    if (n < 0) {
      HAL_ERR("Error polling for sensor data (err=%d)", n);
      break;
    }

    for (int i = 0; i < n; ++i) {
      // FIXME: bug 802004, add proper support for the magnetic field sensor.
      if (buffer[i].type == SENSOR_TYPE_MAGNETIC_FIELD)
        continue;

      // Bug 938035, transfer HAL data for orientation sensor to meet w3c spec
      // ex: HAL report alpha=90 means East but alpha=90 means West in w3c spec
      if (buffer[i].type == SENSOR_TYPE_ORIENTATION) {
        buffer[i].orientation.azimuth = 360 - buffer[i].orientation.azimuth;
        buffer[i].orientation.pitch = -buffer[i].orientation.pitch;
        buffer[i].orientation.roll = -buffer[i].orientation.roll;
      }

      if (HardwareSensorToHalSensor(buffer[i].type) == SENSOR_UNKNOWN) {
        // Emulator is broken and gives us events without types set
        int index;
        for (index = 0; index < size; index++) {
          if (sensors[index].handle == buffer[i].sensor) {
            break;
          }
        }
        if (index < size &&
            HardwareSensorToHalSensor(sensors[index].type) != SENSOR_UNKNOWN) {
          buffer[i].type = sensors[index].type;
        } else {
          HAL_LOG("Could not determine sensor type of event");
          continue;
        }
      }

      NS_DispatchToMainThread(new SensorRunnable(buffer[i], sensors, size));
    }
  } while (true);
}

static void
SwitchSensor(bool aActivate, sensor_t aSensor, pthread_t aThreadId)
{
  int index = HardwareSensorToHalSensor(aSensor.type);

  MOZ_ASSERT(sSensorRefCount[index] || aActivate);

  sSensorDevice->activate(sSensorDevice, aSensor.handle, aActivate);

  if (aActivate) {
    if (aSensor.type == SENSOR_TYPE_ACCELEROMETER) {
      sSensorDevice->setDelay(sSensorDevice, aSensor.handle,
                   ACCELEROMETER_POLL_RATE);
    } else {
      sSensorDevice->setDelay(sSensorDevice, aSensor.handle,
                   DEFAULT_DEVICE_POLL_RATE);
    }
  }

  if (aActivate) {
    sSensorRefCount[index]++;
  } else {
    sSensorRefCount[index]--;
  }
}

static void
SetSensorState(SensorType aSensor, bool activate)
{
  int type = HalSensorToHardwareSensor(aSensor);
  const sensor_t* sensors = nullptr;

  int size = sSensorModule->get_sensors_list(sSensorModule, &sensors);
  for (ssize_t i = 0; i < size; i++) {
    if (sensors[i].type == type) {
      SwitchSensor(activate, sensors[i], pthread_self());
      break;
    }
  }
}

static void
EnableSensorNotificationsInternal(SensorType aSensor)
{
  if (!sSensorModule) {
    hw_get_module(SENSORS_HARDWARE_MODULE_ID,
                       (hw_module_t const**)&sSensorModule);
    if (!sSensorModule) {
      HAL_ERR("Can't get sensor HAL module\n");
      return;
    }

    sensors_open(&sSensorModule->common, &sSensorDevice);
    if (!sSensorDevice) {
      sSensorModule = nullptr;
      HAL_ERR("Can't get sensor poll device from module \n");
      return;
    }

    sensor_t const* sensors;
    int count = sSensorModule->get_sensors_list(sSensorModule, &sensors);
    for (size_t i=0 ; i<size_t(count) ; i++) {
      sSensorDevice->activate(sSensorDevice, sensors[i].handle, 0);
    }
  }

  if (!sPollingThread) {
    sPollingThread = new base::Thread("GonkSensors");
    MOZ_ASSERT(sPollingThread);
    // sPollingThread never terminates because poll may never return
    sPollingThread->Start();
    sPollingThread->message_loop()->PostTask(
      NewRunnableFunction(PollSensors));
  }

  SetSensorState(aSensor, true);
}

static void
DisableSensorNotificationsInternal(SensorType aSensor)
{
  if (!sSensorModule) {
    return;
  }
  SetSensorState(aSensor, false);
}

//
// Daemon
//

typedef detail::SaturateOp<uint32_t> SaturateOpUint32;

/**
 * The poll notification handler receives all events about sensors and
 * sensor events.
 */
class SensorsPollNotificationHandler final
  : public GonkSensorsPollNotificationHandler
{
public:
  SensorsPollNotificationHandler(GonkSensorsPollInterface* aPollInterface)
    : mPollInterface(aPollInterface)
  {
    MOZ_ASSERT(mPollInterface);

    mPollInterface->SetNotificationHandler(this);
  }

  void EnableSensorsByType(SensorsType aType)
  {
    if (SaturateOpUint32(mClasses[aType].mActivated)++) {
      return;
    }

    SensorsDeliveryMode deliveryMode = DefaultSensorsDeliveryMode(aType);

    // Old ref-count for the sensor type was 0, so we
    // activate all sensors of the type.
    for (size_t i = 0; i < mSensors.Length(); ++i) {
      if (mSensors[i].mType == aType &&
          mSensors[i].mDeliveryMode == deliveryMode) {
        mPollInterface->EnableSensor(mSensors[i].mId, nullptr);
        mPollInterface->SetPeriod(mSensors[i].mId, DefaultSensorPeriod(aType),
                                  nullptr);
      }
    }
  }

  void DisableSensorsByType(SensorsType aType)
  {
    if (SaturateOpUint32(mClasses[aType].mActivated)-- != 1) {
      return;
    }

    SensorsDeliveryMode deliveryMode = DefaultSensorsDeliveryMode(aType);

    // Old ref-count for the sensor type was 1, so we
    // deactivate all sensors of the type.
    for (size_t i = 0; i < mSensors.Length(); ++i) {
      if (mSensors[i].mType == aType &&
          mSensors[i].mDeliveryMode == deliveryMode) {
        mPollInterface->DisableSensor(mSensors[i].mId, nullptr);
      }
    }
  }

  void ClearSensorClasses()
  {
    for (size_t i = 0; i < MOZ_ARRAY_LENGTH(mClasses); ++i) {
      mClasses[i] = SensorsSensorClass();
    }
  }

  void ClearSensors()
  {
    mSensors.Clear();
  }

  // Methods for SensorsPollNotificationHandler
  //

  void ErrorNotification(SensorsError aError) override
  {
    // XXX: Bug 1206056: Try to repair some of the errors or restart cleanly.
  }

  void SensorDetectedNotification(int32_t aId, SensorsType aType,
                                  float aRange, float aResolution,
                                  float aPower, int32_t aMinPeriod,
                                  int32_t aMaxPeriod,
                                  SensorsTriggerMode aTriggerMode,
                                  SensorsDeliveryMode aDeliveryMode) override
  {
    auto i = FindSensorIndexById(aId);
    if (i == -1) {
      // Add a new sensor...
      i = mSensors.Length();
      mSensors.AppendElement(SensorsSensor(aId, aType, aRange, aResolution,
                                           aPower, aMinPeriod, aMaxPeriod,
                                           aTriggerMode, aDeliveryMode));
    } else {
      // ...or update an existing one.
      mSensors[i] = SensorsSensor(aId, aType, aRange, aResolution, aPower,
                                  aMinPeriod, aMaxPeriod, aTriggerMode,
                                  aDeliveryMode);
    }

    mClasses[aType].UpdateFromSensor(mSensors[i]);

    if (mClasses[aType].mActivated &&
        mSensors[i].mDeliveryMode == DefaultSensorsDeliveryMode(aType)) {
      // The new sensor's type is enabled, so enable sensor.
      mPollInterface->EnableSensor(aId, nullptr);
      mPollInterface->SetPeriod(mSensors[i].mId, DefaultSensorPeriod(aType),
                                nullptr);
    }
  }

  void SensorLostNotification(int32_t aId) override
  {
    auto i = FindSensorIndexById(aId);
    if (i != -1) {
      mSensors.RemoveElementAt(i);
    }
  }

  void EventNotification(int32_t aId, const SensorsEvent& aEvent) override
  {
    auto i = FindSensorIndexById(aId);
    if (i == -1) {
      HAL_ERR("Sensor %d not registered", aId);
      return;
    }

    SensorData sensorData;
    auto rv = CreateSensorData(aEvent, mClasses[mSensors[i].mType],
                               sensorData);
    if (NS_FAILED(rv)) {
      return;
    }

    NotifySensorChange(sensorData);
  }

private:
  ssize_t FindSensorIndexById(int32_t aId) const
  {
    for (size_t i = 0; i < mSensors.Length(); ++i) {
      if (mSensors[i].mId == aId) {
        return i;
      }
    }
    return -1;
  }

  uint64_t DefaultSensorPeriod(SensorsType aType) const
  {
    return aType == SENSORS_TYPE_ACCELEROMETER ? ACCELEROMETER_POLL_RATE
                                               : DEFAULT_DEVICE_POLL_RATE;
  }

  SensorsDeliveryMode DefaultSensorsDeliveryMode(SensorsType aType) const
  {
    if (aType == SENSORS_TYPE_PROXIMITY ||
        aType == SENSORS_TYPE_SIGNIFICANT_MOTION) {
      return SENSORS_DELIVERY_MODE_IMMEDIATE;
    }
    return SENSORS_DELIVERY_MODE_BEST_EFFORT;
  }

  SensorType HardwareSensorToHalSensor(SensorsType aType) const
  {
    // FIXME: bug 802004, add proper support for the magnetic-field sensor.
    switch (aType) {
      case SENSORS_TYPE_ORIENTATION:
        return SENSOR_ORIENTATION;
      case SENSORS_TYPE_ACCELEROMETER:
        return SENSOR_ACCELERATION;
      case SENSORS_TYPE_PROXIMITY:
        return SENSOR_PROXIMITY;
      case SENSORS_TYPE_LIGHT:
        return SENSOR_LIGHT;
      case SENSORS_TYPE_GYROSCOPE:
        return SENSOR_GYROSCOPE;
      case SENSORS_TYPE_LINEAR_ACCELERATION:
        return SENSOR_LINEAR_ACCELERATION;
      case SENSORS_TYPE_ROTATION_VECTOR:
        return SENSOR_ROTATION_VECTOR;
      case SENSORS_TYPE_GAME_ROTATION_VECTOR:
        return SENSOR_GAME_ROTATION_VECTOR;
      default:
        NS_NOTREACHED("Invalid sensors type");
    }
    return SENSOR_UNKNOWN;
  }

  SensorAccuracyType HardwareStatusToHalAccuracy(SensorsStatus aStatus) const
  {
    return static_cast<SensorAccuracyType>(aStatus - 1);
  }

  nsresult CreateSensorData(const SensorsEvent& aEvent,
                            const SensorsSensorClass& aSensorClass,
                            SensorData& aSensorData) const
  {
    AutoTArray<float, 4> sensorValues;

    auto sensor = HardwareSensorToHalSensor(aEvent.mType);

    if (sensor == SENSOR_UNKNOWN) {
      return NS_ERROR_ILLEGAL_VALUE;
    }

    aSensorData.sensor() = sensor;
    aSensorData.accuracy() = HardwareStatusToHalAccuracy(aEvent.mStatus);
    aSensorData.timestamp() = aEvent.mTimestamp;

    if (aSensorData.sensor() == SENSOR_ORIENTATION) {
      // Bug 938035: transfer HAL data for orientation sensor to meet W3C spec
      // ex: HAL report alpha=90 means East but alpha=90 means West in W3C spec
      sensorValues.AppendElement(360.0 - radToDeg(aEvent.mData.mFloat[0]));
      sensorValues.AppendElement(-radToDeg(aEvent.mData.mFloat[1]));
      sensorValues.AppendElement(-radToDeg(aEvent.mData.mFloat[2]));
    } else if (aSensorData.sensor() == SENSOR_ACCELERATION) {
      sensorValues.AppendElement(aEvent.mData.mFloat[0]);
      sensorValues.AppendElement(aEvent.mData.mFloat[1]);
      sensorValues.AppendElement(aEvent.mData.mFloat[2]);
    } else if (aSensorData.sensor() == SENSOR_PROXIMITY) {
      sensorValues.AppendElement(aEvent.mData.mFloat[0]);
      sensorValues.AppendElement(aSensorClass.mMinValue);
      sensorValues.AppendElement(aSensorClass.mMaxValue);
    } else if (aSensorData.sensor() == SENSOR_LINEAR_ACCELERATION) {
      sensorValues.AppendElement(aEvent.mData.mFloat[0]);
      sensorValues.AppendElement(aEvent.mData.mFloat[1]);
      sensorValues.AppendElement(aEvent.mData.mFloat[2]);
    } else if (aSensorData.sensor() == SENSOR_GYROSCOPE) {
      sensorValues.AppendElement(radToDeg(aEvent.mData.mFloat[0]));
      sensorValues.AppendElement(radToDeg(aEvent.mData.mFloat[1]));
      sensorValues.AppendElement(radToDeg(aEvent.mData.mFloat[2]));
    } else if (aSensorData.sensor() == SENSOR_LIGHT) {
      sensorValues.AppendElement(aEvent.mData.mFloat[0]);
    } else if (aSensorData.sensor() == SENSOR_ROTATION_VECTOR) {
      sensorValues.AppendElement(aEvent.mData.mFloat[0]);
      sensorValues.AppendElement(aEvent.mData.mFloat[1]);
      sensorValues.AppendElement(aEvent.mData.mFloat[2]);
      sensorValues.AppendElement(aEvent.mData.mFloat[3]);
    } else if (aSensorData.sensor() == SENSOR_GAME_ROTATION_VECTOR) {
      sensorValues.AppendElement(aEvent.mData.mFloat[0]);
      sensorValues.AppendElement(aEvent.mData.mFloat[1]);
      sensorValues.AppendElement(aEvent.mData.mFloat[2]);
      sensorValues.AppendElement(aEvent.mData.mFloat[3]);
    }

    aSensorData.values() = sensorValues;

    return NS_OK;
  }

  GonkSensorsPollInterface* mPollInterface;
  nsTArray<SensorsSensor> mSensors;
  SensorsSensorClass mClasses[SENSORS_NUM_TYPES];
};

static StaticAutoPtr<SensorsPollNotificationHandler> sPollNotificationHandler;

/**
 * This is the notifiaction handler for the Sensors interface. If the backend
 * crashes, we can restart it from here.
 */
class SensorsNotificationHandler final : public GonkSensorsNotificationHandler
{
public:
  SensorsNotificationHandler(GonkSensorsInterface* aInterface)
    : mInterface(aInterface)
  {
    MOZ_ASSERT(mInterface);

    mInterface->SetNotificationHandler(this);
  }

  void BackendErrorNotification(bool aCrashed) override
  {
    // XXX: Bug 1206056: restart sensorsd
  }

private:
  GonkSensorsInterface* mInterface;
};

static StaticAutoPtr<SensorsNotificationHandler> sNotificationHandler;

/**
 * |SensorsRegisterModuleResultHandler| implements the result-handler
 * callback for registering the Poll service and activating the first
 * sensors. If an error occures during the process, the result handler
 * disconnects and closes the backend.
 */
class SensorsRegisterModuleResultHandler final
  : public GonkSensorsRegistryResultHandler
{
public:
  SensorsRegisterModuleResultHandler(
    uint32_t* aSensorsTypeActivated,
    GonkSensorsInterface* aInterface)
    : mSensorsTypeActivated(aSensorsTypeActivated)
    , mInterface(aInterface)
  {
    MOZ_ASSERT(mSensorsTypeActivated);
    MOZ_ASSERT(mInterface);
  }
  void OnError(SensorsError aError) override
  {
    GonkSensorsRegistryResultHandler::OnError(aError); // print error message
    Disconnect(); // Registering failed, so close the connection completely
  }
  void RegisterModule(uint32_t aProtocolVersion) override
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!sPollNotificationHandler);

    // Init, step 3: set notification handler for poll service and vice versa
    auto pollInterface = mInterface->GetSensorsPollInterface();
    if (!pollInterface) {
      Disconnect();
      return;
    }
    if (NS_FAILED(pollInterface->SetProtocolVersion(aProtocolVersion))) {
      Disconnect();
      return;
    }

    sPollNotificationHandler =
      new SensorsPollNotificationHandler(pollInterface);

    // Init, step 4: activate sensors
    for (int i = 0; i < SENSORS_NUM_TYPES; ++i) {
      while (mSensorsTypeActivated[i]) {
        sPollNotificationHandler->EnableSensorsByType(
          static_cast<SensorsType>(i));
        --mSensorsTypeActivated[i];
      }
    }
  }
public:
  void Disconnect()
  {
    class DisconnectResultHandler final : public GonkSensorsResultHandler
    {
    public:
      void OnError(SensorsError aError)
      {
        GonkSensorsResultHandler::OnError(aError); // print error message
        sNotificationHandler = nullptr;
      }
      void Disconnect() override
      {
        sNotificationHandler = nullptr;
      }
    };
    mInterface->Disconnect(new DisconnectResultHandler());
  }
private:
  uint32_t* mSensorsTypeActivated;
  GonkSensorsInterface* mInterface;
};

/**
 * |SensorsConnectResultHandler| implements the result-handler
 * callback for starting the Sensors backend.
 */
class SensorsConnectResultHandler final : public GonkSensorsResultHandler
{
public:
  SensorsConnectResultHandler(
    uint32_t* aSensorsTypeActivated,
    GonkSensorsInterface* aInterface)
    : mSensorsTypeActivated(aSensorsTypeActivated)
    , mInterface(aInterface)
  {
    MOZ_ASSERT(mSensorsTypeActivated);
    MOZ_ASSERT(mInterface);
  }
  void OnError(SensorsError aError) override
  {
    GonkSensorsResultHandler::OnError(aError); // print error message
    sNotificationHandler = nullptr;
  }
  void Connect() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    // Init, step 2: register poll service
    auto registryInterface = mInterface->GetSensorsRegistryInterface();
    if (!registryInterface) {
      return;
    }
    registryInterface->RegisterModule(
      GonkSensorsPollModule::SERVICE_ID,
      new SensorsRegisterModuleResultHandler(mSensorsTypeActivated,
                                             mInterface));
  }
private:
  uint32_t* mSensorsTypeActivated;
  GonkSensorsInterface* mInterface;
};

static uint32_t sSensorsTypeActivated[SENSORS_NUM_TYPES];

static const SensorsType sSensorsType[] = {
  [SENSOR_ORIENTATION] = SENSORS_TYPE_ORIENTATION,
  [SENSOR_ACCELERATION] = SENSORS_TYPE_ACCELEROMETER,
  [SENSOR_PROXIMITY] = SENSORS_TYPE_PROXIMITY,
  [SENSOR_LINEAR_ACCELERATION] = SENSORS_TYPE_LINEAR_ACCELERATION,
  [SENSOR_GYROSCOPE] = SENSORS_TYPE_GYROSCOPE,
  [SENSOR_LIGHT] = SENSORS_TYPE_LIGHT,
  [SENSOR_ROTATION_VECTOR] = SENSORS_TYPE_ROTATION_VECTOR,
  [SENSOR_GAME_ROTATION_VECTOR] = SENSORS_TYPE_GAME_ROTATION_VECTOR
};

void
EnableSensorNotificationsDaemon(SensorType aSensor)
{
  if ((aSensor < 0) ||
      (aSensor > static_cast<ssize_t>(MOZ_ARRAY_LENGTH(sSensorsType)))) {
    HAL_ERR("Sensor type %d not known", aSensor);
    return; // Unsupported sensor type
  }

  auto interface = GonkSensorsInterface::GetInstance();
  if (!interface) {
    return;
  }

  if (sPollNotificationHandler) {
    // Everythings already up and running; enable sensor type.
    sPollNotificationHandler->EnableSensorsByType(sSensorsType[aSensor]);
    return;
  }

  ++SaturateOpUint32(sSensorsTypeActivated[sSensorsType[aSensor]]);

  if (sNotificationHandler) {
    // We are in the middle of a pending start up; nothing else to do.
    return;
  }

  // Start up

  MOZ_ASSERT(!sPollNotificationHandler);
  MOZ_ASSERT(!sNotificationHandler);

  sNotificationHandler = new SensorsNotificationHandler(interface);

  // Init, step 1: connect to Sensors backend
  interface->Connect(
    sNotificationHandler,
    new SensorsConnectResultHandler(sSensorsTypeActivated, interface));
}

void
DisableSensorNotificationsDaemon(SensorType aSensor)
{
  if ((aSensor < 0) ||
      (aSensor > static_cast<ssize_t>(MOZ_ARRAY_LENGTH(sSensorsType)))) {
    HAL_ERR("Sensor type %d not known", aSensor);
    return; // Unsupported sensor type
  }

  if (sPollNotificationHandler) {
    // Everthings up and running; disable sensors type
    sPollNotificationHandler->DisableSensorsByType(sSensorsType[aSensor]);
    return;
  }

  // We might be in the middle of a startup; decrement type's ref-counter.
  --SaturateOpUint32(sSensorsTypeActivated[sSensorsType[aSensor]]);

  // TODO: stop sensorsd if all sensors are disabled
}

//
// Public interface
//

// TODO: Remove in-Gecko sensors code. Until all devices' base
// images come with sensorsd installed, we have to support the
// in-Gecko implementation as well. So we test for the existance
// of the binary. If it's there, we use it. Otherwise we run the
// old code.
static bool
HasDaemon()
{
  static bool tested;
  static bool hasDaemon;

  if (MOZ_UNLIKELY(!tested)) {
    hasDaemon = !access("/system/bin/sensorsd", X_OK);
    tested = true;
  }

  return hasDaemon;
}

void
EnableSensorNotifications(SensorType aSensor)
{
  if (HasDaemon()) {
    EnableSensorNotificationsDaemon(aSensor);
  } else {
    EnableSensorNotificationsInternal(aSensor);
  }
}

void
DisableSensorNotifications(SensorType aSensor)
{
  if (HasDaemon()) {
    DisableSensorNotificationsDaemon(aSensor);
  } else {
    DisableSensorNotificationsInternal(aSensor);
  }
}

} // hal_impl
} // mozilla
