/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothHidManager_h
#define mozilla_dom_bluetooth_BluetoothHidManager_h

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

  static void InitHidInterface(BluetoothProfileResultHandler* aRes);
  static void DeinitHidInterface(BluetoothProfileResultHandler* aRes);
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
  BluetoothAddress mDeviceAddress;
  RefPtr<BluetoothProfileController> mController;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_BluetoothHidManager_h
