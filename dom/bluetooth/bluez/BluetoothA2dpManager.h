/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluez_BluetoothA2dpManager_h
#define mozilla_dom_bluetooth_bluez_BluetoothA2dpManager_h

#include "BluetoothCommon.h"
#include "BluetoothProfileController.h"
#include "BluetoothProfileManagerBase.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothA2dpManager : public BluetoothProfileManagerBase
{
public:
  BT_DECL_PROFILE_MGR_BASE
  virtual void GetName(nsACString& aName)
  {
    aName.AssignLiteral("A2DP");
  }

  enum SinkState {
    SINK_UNKNOWN,
    SINK_DISCONNECTED,
    SINK_CONNECTING,
    SINK_CONNECTED,
    SINK_PLAYING,
  };

  static BluetoothA2dpManager* Get();

  // A2DP-specific functions
  void HandleSinkPropertyChanged(const BluetoothSignal& aSignal);

protected:
  virtual ~BluetoothA2dpManager();

private:
  BluetoothA2dpManager();
  bool Init();
  void ResetA2dp();

  void HandleShutdown();
  void NotifyConnectionStatusChanged();

  BluetoothAddress mDeviceAddress;
  RefPtr<BluetoothProfileController> mController;

  // A2DP data member
  bool mA2dpConnected;
  SinkState mSinkState;

};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluez_BluetoothA2dpManager_h
