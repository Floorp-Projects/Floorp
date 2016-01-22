/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_BluetoothA2dpManager_h
#define mozilla_dom_bluetooth_bluedroid_BluetoothA2dpManager_h

#include "BluetoothCommon.h"
#include "BluetoothInterface.h"
#include "BluetoothProfileController.h"
#include "BluetoothProfileManagerBase.h"

BEGIN_BLUETOOTH_NAMESPACE
class BluetoothA2dpManager : public BluetoothProfileManagerBase
                           , public BluetoothA2dpNotificationHandler
{
public:
  static const int MAX_NUM_CLIENTS;

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
  static void InitA2dpInterface(BluetoothProfileResultHandler* aRes);
  static void DeinitA2dpInterface(BluetoothProfileResultHandler* aRes);

  void OnConnectError();
  void OnDisconnectError();

  // A2DP-specific functions
  void HandleSinkPropertyChanged(const BluetoothSignal& aSignal);

  void HandleBackendError();

protected:
  virtual ~BluetoothA2dpManager();

private:
  class ConnectResultHandler;
  class DeinitProfileResultHandlerRunnable;
  class DisconnectResultHandler;
  class InitProfileResultHandlerRunnable;
  class RegisterModuleResultHandler;
  class UnregisterModuleResultHandler;

  BluetoothA2dpManager();

  void Uninit();
  void HandleShutdown();
  void NotifyConnectionStatusChanged();

  void ConnectionStateNotification(BluetoothA2dpConnectionState aState,
                                   const BluetoothAddress& aBdAddr) override;
  void AudioStateNotification(BluetoothA2dpAudioState aState,
                              const BluetoothAddress& aBdAddr) override;

  BluetoothAddress mDeviceAddress;
  RefPtr<BluetoothProfileController> mController;

  // A2DP data member
  bool mA2dpConnected;
  SinkState mSinkState;
};

END_BLUETOOTH_NAMESPACE

#endif
