/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothprofilemanagerbase_h__
#define mozilla_dom_bluetooth_bluetoothprofilemanagerbase_h__

#define ERR_SERVICE_CHANNEL_NOT_FOUND "DeviceChannelRetrievalError"

#include "BluetoothCommon.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothProfileManagerBase : public nsISupports
{
public:
  virtual void OnGetServiceChannel(const nsAString& aDeviceAddress,
                                   const nsAString& aServiceUuid,
                                   int aChannel) = 0;
  virtual void OnUpdateSdpRecords(const nsAString& aDeviceAddress) = 0;
  virtual void GetAddress(nsAString& aDeviceAddress) = 0;
};

END_BLUETOOTH_NAMESPACE

#endif  //#ifndef mozilla_dom_bluetooth_bluetoothprofilemanagerbase_h__
