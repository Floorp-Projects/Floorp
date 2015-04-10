/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothInterface.h"
#if ANDROID_VERSION >= 17
#include <cutils/properties.h>
#endif
#ifdef MOZ_B2G_BT_BLUEDROID
#include "BluetoothHALInterface.h"
#endif
#ifdef MOZ_B2G_BT_DAEMON
#include "BluetoothDaemonInterface.h"
#endif

BEGIN_BLUETOOTH_NAMESPACE

//
// Socket Interface
//

BluetoothSocketInterface::~BluetoothSocketInterface()
{ }

//
// Handsfree Interface
//

// Notification handling
//

BluetoothHandsfreeNotificationHandler::
  ~BluetoothHandsfreeNotificationHandler()
{ }

// Interface
//

BluetoothHandsfreeInterface::BluetoothHandsfreeInterface()
{ }

BluetoothHandsfreeInterface::~BluetoothHandsfreeInterface()
{ }

//
// Bluetooth Advanced Audio Interface
//

// Notification handling
//

BluetoothA2dpNotificationHandler::~BluetoothA2dpNotificationHandler()
{ }

// Interface
//

BluetoothA2dpInterface::BluetoothA2dpInterface()
{ }

BluetoothA2dpInterface::~BluetoothA2dpInterface()
{ }

//
// Bluetooth AVRCP Interface
//

// Notification handling
//

BluetoothAvrcpNotificationHandler::~BluetoothAvrcpNotificationHandler()
{ }

// Interface
//

BluetoothAvrcpInterface::BluetoothAvrcpInterface()
{ }

BluetoothAvrcpInterface::~BluetoothAvrcpInterface()
{ }

//
// Bluetooth GATT Interface
//

// Notification handling
//

BluetoothGattClientNotificationHandler::~BluetoothGattClientNotificationHandler()
{ }

BluetoothGattServerNotificationHandler::~BluetoothGattServerNotificationHandler()
{ }

BluetoothGattNotificationHandler::~BluetoothGattNotificationHandler()
{ }

// Interface
//

BluetoothGattClientInterface::BluetoothGattClientInterface()
{ }

BluetoothGattClientInterface::~BluetoothGattClientInterface()
{ }

BluetoothGattInterface::BluetoothGattInterface()
{ }

BluetoothGattInterface::~BluetoothGattInterface()
{ }

//
// Bluetooth Core Interface
//

// Notification handling
//

BluetoothNotificationHandler::~BluetoothNotificationHandler()
{ }

// Interface
//

BluetoothInterface*
BluetoothInterface::GetInstance()
{
#if ANDROID_VERSION >= 17
  /* We pick a default backend from the available ones. The branches
   * are ordered by preference.
   */
#ifdef MOZ_B2G_BT_BLUEDROID
  static const char sDefaultBackend[] = "bluedroid";
#else
#ifdef MOZ_B2G_BT_DAEMON
  static const char sDefaultBackend[] = "bluetoothd";
#else
  static const char* const sDefaultBackend = nullptr;
#endif
#endif

  /* Here's where we decide which implementation to use. Currently
   * there is only Bluedroid and the Bluetooth daemon, but others are
   * possible. Having multiple interfaces built-in and selecting the
   * correct one at runtime is also an option.
   */

  char value[PROPERTY_VALUE_MAX];
  int len;

  len = property_get("ro.moz.bluetooth.backend", value, sDefaultBackend);
  if (len < 0) {
    BT_WARNING("No Bluetooth backend available.");
    return nullptr;
  }

  const nsDependentCString backend(value, len);

#ifdef MOZ_B2G_BT_BLUEDROID
  if (backend.LowerCaseEqualsLiteral("bluedroid")) {
    return BluetoothHALInterface::GetInstance();
  } else
#endif
#ifdef MOZ_B2G_BT_DAEMON
  if (backend.LowerCaseEqualsLiteral("bluetoothd")) {
    return BluetoothDaemonInterface::GetInstance();
  } else
#endif
  {
    BT_WARNING("Bluetooth backend '%s' is unknown or not available.",
               backend.get());
  }
  return nullptr;

#else
  /* Anything that's not Android 4.2 or later uses BlueZ instead. The
   * code should actually never reach this point.
   */
  BT_WARNING("No Bluetooth backend available for your system.");
  return nullptr;
#endif
}

BluetoothInterface::BluetoothInterface()
{ }

BluetoothInterface::~BluetoothInterface()
{ }

END_BLUETOOTH_NAMESPACE
