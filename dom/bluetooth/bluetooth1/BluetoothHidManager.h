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
  BT_DECL_PROFILE_MGR_BASE
  virtual void GetName(nsACString& aName)
  {
    aName.AssignLiteral("HID");
  }

  static BluetoothHidManager* Get();

  // HID-specific functions
  void HandleInputPropertyChanged(const BluetoothSignal& aSignal);

protected:
  virtual ~BluetoothHidManager();

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
