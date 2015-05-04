/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_bluetootha2dphalinterface_h__
#define mozilla_dom_bluetooth_bluedroid_bluetootha2dphalinterface_h__

#include <hardware/bluetooth.h>
#include <hardware/bt_av.h>
#include "BluetoothCommon.h"
#include "BluetoothInterface.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothHALInterface;

class BluetoothA2dpHALInterface final
  : public BluetoothA2dpInterface
{
public:
  friend class BluetoothHALInterface;

  void Init(BluetoothA2dpNotificationHandler* aNotificationHandler,
            BluetoothA2dpResultHandler* aRes);
  void Cleanup(BluetoothA2dpResultHandler* aRes);

  void Connect(const nsAString& aBdAddr,
               BluetoothA2dpResultHandler* aRes);
  void Disconnect(const nsAString& aBdAddr,
                  BluetoothA2dpResultHandler* aRes);

protected:
  BluetoothA2dpHALInterface(const btav_interface_t* aInterface);
  ~BluetoothA2dpHALInterface();

private:
  const btav_interface_t* mInterface;
};

END_BLUETOOTH_NAMESPACE

#endif
