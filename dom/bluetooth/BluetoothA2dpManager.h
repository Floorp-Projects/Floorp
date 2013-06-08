/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetootha2dpmanager_h__
#define mozilla_dom_bluetooth_bluetootha2dpmanager_h__

#include "BluetoothCommon.h"
#include "BluetoothProfileManagerBase.h"

BEGIN_BLUETOOTH_NAMESPACE

enum SinkState {
  SINK_DISCONNECTED = 1,
  SINK_CONNECTING,
  SINK_CONNECTED,
  SINK_PLAYING,
  SINK_DISCONNECTING
};

class BluetoothA2dpManagerObserver;
class BluetoothValue;
class BluetoothSocket;

class BluetoothA2dpManager : public BluetoothProfileManagerBase
{
public:
  NS_DECL_ISUPPORTS

  static BluetoothA2dpManager* Get();
  ~BluetoothA2dpManager();

  bool Connect(const nsAString& aDeviceAddress);
  void Disconnect();
  void HandleSinkPropertyChanged(const BluetoothSignal& aSignal);
  nsresult HandleShutdown();

  virtual void OnGetServiceChannel(const nsAString& aDeviceAddress,
                                   const nsAString& aServiceUuid,
                                   int aChannel) MOZ_OVERRIDE;
  virtual void OnUpdateSdpRecords(const nsAString& aDeviceAddress) MOZ_OVERRIDE;
  virtual void GetAddress(nsAString& aDeviceAddress) MOZ_OVERRIDE;

private:
  BluetoothA2dpManager();
  bool Init();
  void Cleanup();

  void HandleSinkStateChanged(SinkState aState);

  void NotifyStatusChanged();
  void NotifyAudioManager();

  bool mConnected;
  bool mPlaying;
  nsString mDeviceAddress;
  SinkState mSinkState;
};

END_BLUETOOTH_NAMESPACE

#endif
