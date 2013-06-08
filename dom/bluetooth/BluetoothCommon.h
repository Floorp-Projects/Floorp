/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothcommon_h__
#define mozilla_dom_bluetooth_bluetoothcommon_h__

#include "mozilla/Observer.h"
#include "nsStringGlue.h"
#include "nsTArray.h"

extern bool gBluetoothDebugFlag;

#define SWITCH_BT_DEBUG(V) (gBluetoothDebugFlag = V)

#undef BT_LOG
#if defined(MOZ_WIDGET_GONK)
#include <android/log.h>
#define BT_LOG(args...)                                              \
  do {                                                               \
    if (gBluetoothDebugFlag) {                                       \
      __android_log_print(ANDROID_LOG_INFO, "GeckoBluetooth", args); \
    }                                                                \
  } while(0)

#define BT_WARNING(args...)                                          \
  __android_log_print(ANDROID_LOG_WARN, "GeckoBluetooth", args)

#else
#define BT_LOG(args, ...)                                            \
  do {                                                               \
    if (gBluetoothDebugFlag) {                                       \
      printf(args, ##__VA_ARGS__);                                   \
    }                                                                \
  } while(0)

#define BT_WARNING(args, ...) printf(args, ##__VA_ARGS__)
#endif

#define BEGIN_BLUETOOTH_NAMESPACE \
  namespace mozilla { namespace dom { namespace bluetooth {
#define END_BLUETOOTH_NAMESPACE \
  } /* namespace bluetooth */ } /* namespace dom */ } /* namespace mozilla */
#define USING_BLUETOOTH_NAMESPACE \
  using namespace mozilla::dom::bluetooth;

#define KEY_LOCAL_AGENT  "/B2G/bluetooth/agent"
#define KEY_REMOTE_AGENT "/B2G/bluetooth/remote_device_agent"
#define KEY_MANAGER      "/B2G/bluetooth/manager"
#define KEY_ADAPTER      "/B2G/bluetooth/adapter"

/**
 * When connection status of Bluetooth profiles change, we'll notify observers
 * of following topics.
 */
#define BLUETOOTH_SCO_STATUS_CHANGED "bluetooth-sco-status-changed"
#define BLUETOOTH_A2DP_STATUS_CHANGED "bluetooth-a2dp-status-changed"

// Bluetooth address format: xx:xx:xx:xx:xx:xx (or xx_xx_xx_xx_xx_xx)
#define BLUETOOTH_ADDRESS_LENGTH 17
#define BLUETOOTH_ADDRESS_NONE   "00:00:00:00:00:00"

BEGIN_BLUETOOTH_NAMESPACE

enum BluetoothSocketType {
  RFCOMM = 1,
  SCO    = 2,
  L2CAP  = 3,
  EL2CAP = 4
};

class BluetoothSignal;
typedef mozilla::Observer<BluetoothSignal> BluetoothSignalObserver;

// Enums for object types, currently used for shared function lookups
// (get/setproperty, etc...). Possibly discernable via dbus paths, but this
// method is future-proofed for platform independence.
enum BluetoothObjectType {
  TYPE_MANAGER = 0,
  TYPE_ADAPTER = 1,
  TYPE_DEVICE = 2,

  TYPE_INVALID
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluetoothcommon_h__
