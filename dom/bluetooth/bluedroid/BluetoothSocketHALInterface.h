/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_bluetoothsockethalinterface_h__
#define mozilla_dom_bluetooth_bluedroid_bluetoothsockethalinterface_h__

#include <hardware/bluetooth.h>
#include <hardware/bt_sock.h>
#include "BluetoothCommon.h"
#include "BluetoothInterface.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothHALInterface;

class BluetoothSocketHALInterface MOZ_FINAL
  : public BluetoothSocketInterface
{
public:
  class ConnectWatcher;
  class AcceptWatcher;

  friend class BluetoothHALInterface;

  void Listen(BluetoothSocketType aType,
              const nsAString& aServiceName,
              const uint8_t aServiceUuid[16],
              int aChannel, bool aEncrypt, bool aAuth,
              BluetoothSocketResultHandler* aRes);

  void Connect(const nsAString& aBdAddr,
               BluetoothSocketType aType,
               const uint8_t aUuid[16],
               int aChannel, bool aEncrypt, bool aAuth,
               BluetoothSocketResultHandler* aRes);

  void Accept(int aFd, BluetoothSocketResultHandler* aRes);

  void Close(BluetoothSocketResultHandler* aRes);

protected:
  BluetoothSocketHALInterface(const btsock_interface_t* aInterface);
  ~BluetoothSocketHALInterface();

private:
  const btsock_interface_t* mInterface;
};

END_BLUETOOTH_NAMESPACE

#endif
