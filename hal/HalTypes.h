/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_hal_Types_h
#define mozilla_hal_Types_h

#include "ipc/IPCMessageUtils.h"
#include "mozilla/Observer.h"

namespace mozilla {
namespace hal {

/**
 * These constants specify special values for content process IDs.  You can get
 * a content process ID by calling ContentChild::GetID() or
 * ContentParent::GetChildID().
 */
const uint64_t CONTENT_PROCESS_ID_UNKNOWN = uint64_t(-1);
const uint64_t CONTENT_PROCESS_ID_MAIN = 0;

/**
 * These are defined by libhardware, specifically, hardware/libhardware/include/hardware/lights.h
 * in the gonk subsystem.
 * If these change and are exposed to JS, make sure nsIHal.idl is updated as well.
 */
enum ShutdownMode {
  eHalShutdownMode_Unknown  = -1,
  eHalShutdownMode_PowerOff = 0,
  eHalShutdownMode_Reboot   = 1,
  eHalShutdownMode_Restart  = 2,
  eHalShutdownMode_Count    = 3
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
  SWITCH_STATE_HEADSET,          // Headphone with microphone
  SWITCH_STATE_HEADPHONE,        // without microphone
  NUM_SWITCH_STATE
};

typedef Observer<SwitchEvent> SwitchObserver;

// Note that we rely on the order of this enum's entries.  Higher priorities
// should have larger int values.
enum ProcessPriority {
  PROCESS_PRIORITY_UNKNOWN = -1,
  PROCESS_PRIORITY_BACKGROUND,
  PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE,
  PROCESS_PRIORITY_FOREGROUND_KEYBOARD,
  // Any priority greater than or equal to FOREGROUND is considered
  // "foreground" for the purposes of priority testing, for example
  // CurrentProcessIsForeground().
  PROCESS_PRIORITY_FOREGROUND,
  PROCESS_PRIORITY_FOREGROUND_HIGH,
  PROCESS_PRIORITY_MASTER,
  NUM_PROCESS_PRIORITY
};

/**
 * Values that can be passed to hal::SetCurrentThreadPriority().  These should be
 * functional in nature, such as COMPOSITOR, instead of levels, like LOW/HIGH.
 * This allows us to tune our priority scheme for the system in one place such
 * that it makes sense holistically for the overall operating system.  On gonk
 * or android we may want different priority schemes than on windows, etc.
 */
enum ThreadPriority {
  THREAD_PRIORITY_COMPOSITOR,
  NUM_THREAD_PRIORITY
};

/**
 * Convert a ProcessPriority enum value to a string.  The strings returned by
 * this function are statically allocated; do not attempt to free one!
 *
 * If you pass an unknown process priority, we fatally assert in debug
 * builds and otherwise return "???".
 */
const char*
ProcessPriorityToString(ProcessPriority aPriority);

/**
 * Convert a ThreadPriority enum value to a string.  The strings returned by
 * this function are statically allocated; do not attempt to free one!
 *
 * If you pass an unknown process priority, we assert in debug builds
 * and otherwise return "???".
 */
const char *
ThreadPriorityToString(ThreadPriority aPriority);

/**
 * Used by ModifyWakeLock
 */
enum WakeLockControl {
  WAKE_LOCK_REMOVE_ONE = -1,
  WAKE_LOCK_NO_CHANGE  = 0,
  WAKE_LOCK_ADD_ONE    = 1,
  NUM_WAKE_LOCK
};

} // namespace hal
} // namespace mozilla

namespace IPC {

/**
 * Serializer for ShutdownMode.
 */
template <>
struct ParamTraits<mozilla::hal::ShutdownMode>
  : public ContiguousEnumSerializer<
             mozilla::hal::ShutdownMode,
             mozilla::hal::eHalShutdownMode_Unknown,
             mozilla::hal::eHalShutdownMode_Count>
{};

/**
 * WakeLockControl serializer.
 */
template <>
struct ParamTraits<mozilla::hal::WakeLockControl>
  : public ContiguousEnumSerializer<
             mozilla::hal::WakeLockControl,
             mozilla::hal::WAKE_LOCK_REMOVE_ONE,
             mozilla::hal::NUM_WAKE_LOCK>
{};

/**
 * Serializer for SwitchState
 */
template <>
struct ParamTraits<mozilla::hal::SwitchState>:
  public ContiguousEnumSerializer<
           mozilla::hal::SwitchState,
           mozilla::hal::SWITCH_STATE_UNKNOWN,
           mozilla::hal::NUM_SWITCH_STATE> {
};

/**
 * Serializer for SwitchDevice
 */
template <>
struct ParamTraits<mozilla::hal::SwitchDevice>:
  public ContiguousEnumSerializer<
           mozilla::hal::SwitchDevice,
           mozilla::hal::SWITCH_DEVICE_UNKNOWN,
           mozilla::hal::NUM_SWITCH_DEVICE> {
};

template <>
struct ParamTraits<mozilla::hal::ProcessPriority>:
  public ContiguousEnumSerializer<
           mozilla::hal::ProcessPriority,
           mozilla::hal::PROCESS_PRIORITY_UNKNOWN,
           mozilla::hal::NUM_PROCESS_PRIORITY> {
};


} // namespace IPC

#endif // mozilla_hal_Types_h
