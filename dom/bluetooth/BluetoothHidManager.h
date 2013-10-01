/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothhidmanager_h__
#define mozilla_dom_bluetooth_bluetoothhidmanager_h__

#include "BluetoothCommon.h"
#include "BluetoothProfileController.h"
#include "BluetoothProfileManagerBase.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothHidManager : public BluetoothProfileManagerBase
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static BluetoothHidManager* Get();
  ~BluetoothHidManager();

  // The following functions are inherited from BluetoothProfileManagerBase
  virtual void OnGetServiceChannel(const nsAString& aDeviceAddress,
                                   const nsAString& aServiceUuid,
                                   int aChannel) MOZ_OVERRIDE;
  virtual void OnUpdateSdpRecords(const nsAString& aDeviceAddress) MOZ_OVERRIDE;
  virtual void GetAddress(nsAString& aDeviceAddress) MOZ_OVERRIDE;
  virtual bool IsConnected() MOZ_OVERRIDE;
  virtual void Connect(const nsAString& aDeviceAddress,
                       BluetoothProfileController* aController) MOZ_OVERRIDE;
  virtual void Disconnect(BluetoothProfileController* aController)
                          MOZ_OVERRIDE;
  virtual void OnConnect(const nsAString& aErrorStr) MOZ_OVERRIDE;
  virtual void OnDisconnect(const nsAString& aErrorStr) MOZ_OVERRIDE;

  virtual void GetName(nsACString& aName)
  {
    aName.AssignLiteral("HID");
  }

  // HID-specific functions
  void HandleInputPropertyChanged(const BluetoothSignal& aSignal);

private:
  BluetoothHidManager();
  bool Init();
  void Cleanup();
  void HandleShutdown();

  void NotifyStatusChanged();

  // data member
  bool mConnected;
  nsString mDeviceAddress;
  nsRefPtr<BluetoothProfileController> mController;
};

END_BLUETOOTH_NAMESPACE

#endif //#ifndef mozilla_dom_bluetooth_bluetoothhidmanager_h__
