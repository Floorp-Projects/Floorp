/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef hal_gonk_SensorsTypes_h
#define hal_gonk_SensorsTypes_h

namespace mozilla {
namespace hal {

enum SensorsDeliveryMode {
  SENSORS_DELIVERY_MODE_BEST_EFFORT,
  SENSORS_DELIVERY_MODE_IMMEDIATE
};

enum SensorsError {
  SENSORS_ERROR_NONE,
  SENSORS_ERROR_FAIL,
  SENSORS_ERROR_NOT_READY,
  SENSORS_ERROR_NOMEM,
  SENSORS_ERROR_BUSY,
  SENSORS_ERROR_DONE,
  SENSORS_ERROR_UNSUPPORTED,
  SENSORS_ERROR_PARM_INVALID
};

enum SensorsStatus {
  SENSORS_STATUS_NO_CONTACT,
  SENSORS_STATUS_UNRELIABLE,
  SENSORS_STATUS_ACCURACY_LOW,
  SENSORS_STATUS_ACCURACY_MEDIUM,
  SENSORS_STATUS_ACCURACY_HIGH
};

enum SensorsTriggerMode {
  SENSORS_TRIGGER_MODE_CONTINUOUS,
  SENSORS_TRIGGER_MODE_ON_CHANGE,
  SENSORS_TRIGGER_MODE_ONE_SHOT,
  SENSORS_TRIGGER_MODE_SPECIAL
};

enum SensorsType {
  SENSORS_TYPE_ACCELEROMETER,
  SENSORS_TYPE_GEOMAGNETIC_FIELD,
  SENSORS_TYPE_ORIENTATION,
  SENSORS_TYPE_GYROSCOPE,
  SENSORS_TYPE_LIGHT,
  SENSORS_TYPE_PRESSURE,
  SENSORS_TYPE_TEMPERATURE,
  SENSORS_TYPE_PROXIMITY,
  SENSORS_TYPE_GRAVITY,
  SENSORS_TYPE_LINEAR_ACCELERATION,
  SENSORS_TYPE_ROTATION_VECTOR,
  SENSORS_TYPE_RELATIVE_HUMIDITY,
  SENSORS_TYPE_AMBIENT_TEMPERATURE,
  SENSORS_TYPE_MAGNETIC_FIELD_UNCALIBRATED,
  SENSORS_TYPE_GAME_ROTATION_VECTOR,
  SENSORS_TYPE_GYROSCOPE_UNCALIBRATED,
  SENSORS_TYPE_SIGNIFICANT_MOTION,
  SENSORS_TYPE_STEP_DETECTED,
  SENSORS_TYPE_STEP_COUNTER,
  SENSORS_TYPE_GEOMAGNETIC_ROTATION_VECTOR,
  SENSORS_TYPE_HEART_RATE,
  SENSORS_TYPE_TILT_DETECTOR,
  SENSORS_TYPE_WAKE_GESTURE,
  SENSORS_TYPE_GLANCE_GESTURE,
  SENSORS_TYPE_PICK_UP_GESTURE,
  SENSORS_TYPE_WRIST_TILT_GESTURE,
  SENSORS_NUM_TYPES
};

struct SensorsEvent {
  SensorsType mType;
  SensorsStatus mStatus;
  SensorsDeliveryMode mDeliveryMode;
  int64_t mTimestamp;
  union {
    float mFloat[6];
    uint64_t mUint[1];
  } mData;
};

/**
 * |SensorsSensor| represents a device sensor; either single or composite.
 */
struct SensorsSensor {
  SensorsSensor(int32_t aId, SensorsType aType,
                float aRange, float aResolution,
                float aPower, int32_t aMinPeriod,
                int32_t aMaxPeriod,
                SensorsTriggerMode aTriggerMode,
                SensorsDeliveryMode aDeliveryMode)
    : mId(aId)
    , mType(aType)
    , mRange(aRange)
    , mResolution(aResolution)
    , mPower(aPower)
    , mMinPeriod(aMinPeriod)
    , mMaxPeriod(aMaxPeriod)
    , mTriggerMode(aTriggerMode)
    , mDeliveryMode(aDeliveryMode)
  { }

  int32_t mId;
  SensorsType mType;
  float mRange;
  float mResolution;
  float mPower;
  int32_t mMinPeriod;
  int32_t mMaxPeriod;
  SensorsTriggerMode mTriggerMode;
  SensorsDeliveryMode mDeliveryMode;
};

/**
 * |SensorClass| represents the status of a specific sensor type.
 */
struct SensorsSensorClass {
  SensorsSensorClass()
    : mActivated(0)
    , mMinValue(0)
    , mMaxValue(0)
  { }

  void UpdateFromSensor(const SensorsSensor& aSensor)
  {
    mMaxValue = std::max(aSensor.mRange, mMaxValue);
  }

  uint32_t mActivated;
  float mMinValue;
  float mMaxValue;
};

} // namespace hal
} // namespace mozilla

#endif // hal_gonk_SensorsTypes_h
