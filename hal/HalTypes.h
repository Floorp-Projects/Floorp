/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_hal_Types_h
#define mozilla_hal_Types_h

#include "IPCMessageUtils.h"

namespace mozilla {
namespace hal {

/**
 * These are defined by libhardware, specifically, hardware/libhardware/include/hardware/lights.h
 * in the gonk subsystem.
 * If these change and are exposed to JS, make sure nsIHal.idl is updated as well.
 */
enum LightType {
    eHalLightID_Backlight = 0,
    eHalLightID_Keyboard = 1,
    eHalLightID_Buttons = 2,
    eHalLightID_Battery = 3,
    eHalLightID_Notifications = 4,
    eHalLightID_Attention = 5,
    eHalLightID_Bluetooth = 6,
    eHalLightID_Wifi = 7,
    eHalLightID_Count = 8         // This should stay at the end
};
enum LightMode {
    eHalLightMode_User = 0,       // brightness is managed by user setting
    eHalLightMode_Sensor = 1      // brightness is managed by a light sensor
};
enum FlashMode {
    eHalLightFlash_None = 0,
    eHalLightFlash_Timed = 1,     // timed flashing.  Use flashOnMS and flashOffMS for timing
    eHalLightFlash_Hardware = 2   // hardware assisted flashing
};

class SwitchEvent;

enum SwitchDevice {
  SWITCH_DEVICE_UNKNOWN = -1,
  SWITCH_HEADPHONES,
  SWITCH_USB,
  NUM_SWITCH_DEVICE
};

enum SwitchState {
  SWITCH_STATE_UNKNOWN = -1,
  SWITCH_STATE_ON,
  SWITCH_STATE_OFF,
  NUM_SWITCH_STATE
};

typedef Observer<SwitchEvent> SwitchObserver;

enum ProcessPriority {
  PROCESS_PRIORITY_BACKGROUND,
  PROCESS_PRIORITY_FOREGROUND,
  PROCESS_PRIORITY_MASTER,
  NUM_PROCESS_PRIORITY
};

/**
 * Used by ModifyWakeLock
 */
enum WakeLockControl {
  WAKE_LOCK_REMOVE_ONE = -1,
  WAKE_LOCK_NO_CHANGE  = 0,
  WAKE_LOCK_ADD_ONE    = 1,
};

} // namespace hal
} // namespace mozilla

namespace IPC {

/**
 * Light type serializer.
 */
template <>
struct ParamTraits<mozilla::hal::LightType>
  : public EnumSerializer<mozilla::hal::LightType,
                          mozilla::hal::eHalLightID_Backlight,
                          mozilla::hal::eHalLightID_Count>
{};

/**
 * Light mode serializer.
 */
template <>
struct ParamTraits<mozilla::hal::LightMode>
  : public EnumSerializer<mozilla::hal::LightMode,
                          mozilla::hal::eHalLightMode_User,
                          mozilla::hal::eHalLightMode_Sensor>
{};

/**
 * Flash mode serializer.
 */
template <>
struct ParamTraits<mozilla::hal::FlashMode>
  : public EnumSerializer<mozilla::hal::FlashMode,
                          mozilla::hal::eHalLightFlash_None,
                          mozilla::hal::eHalLightFlash_Hardware>
{};

/**
 * WakeLockControl serializer.
 */
template <>
struct ParamTraits<mozilla::hal::WakeLockControl>
  : public EnumSerializer<mozilla::hal::WakeLockControl,
                          mozilla::hal::WAKE_LOCK_REMOVE_ONE,
                          mozilla::hal::WAKE_LOCK_ADD_ONE>
{};

/**
 * Serializer for SwitchState
 */
template <>
struct ParamTraits<mozilla::hal::SwitchState>:
  public EnumSerializer<mozilla::hal::SwitchState,
                        mozilla::hal::SWITCH_STATE_UNKNOWN,
                        mozilla::hal::NUM_SWITCH_STATE> {
};

/**
 * Serializer for SwitchDevice
 */
template <>
struct ParamTraits<mozilla::hal::SwitchDevice>:
  public EnumSerializer<mozilla::hal::SwitchDevice,
                        mozilla::hal::SWITCH_DEVICE_UNKNOWN,
                        mozilla::hal::NUM_SWITCH_DEVICE> {
};

template <>
struct ParamTraits<mozilla::hal::ProcessPriority>:
  public EnumSerializer<mozilla::hal::ProcessPriority,
                        mozilla::hal::PROCESS_PRIORITY_BACKGROUND,
                        mozilla::hal::NUM_PROCESS_PRIORITY> {
};


} // namespace IPC

#endif // mozilla_hal_Types_h
