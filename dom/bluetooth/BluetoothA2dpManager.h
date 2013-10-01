/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetootha2dpmanager_h__
#define mozilla_dom_bluetooth_bluetootha2dpmanager_h__

#include "BluetoothCommon.h"
#include "BluetoothProfileController.h"
#include "BluetoothProfileManagerBase.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothA2dpManager : public BluetoothProfileManagerBase
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  enum SinkState {
    SINK_DISCONNECTED = 1,
    SINK_CONNECTING,
    SINK_CONNECTED,
    SINK_PLAYING,
    SINK_DISCONNECTING
  };

  static BluetoothA2dpManager* Get();
  ~BluetoothA2dpManager();
  void ResetA2dp();
  void ResetAvrcp();

  // The following functions are inherited from BluetoothProfileManagerBase
  virtual void OnGetServiceChannel(const nsAString& aDeviceAddress,
                                   const nsAString& aServiceUuid,
                                   int aChannel) MOZ_OVERRIDE;
  virtual void OnUpdateSdpRecords(const nsAString& aDeviceAddress) MOZ_OVERRIDE;
  virtual void GetAddress(nsAString& aDeviceAddress) MOZ_OVERRIDE;
  virtual bool IsConnected() MOZ_OVERRIDE;
  virtual void Connect(const nsAString& aDeviceAddress,
                       BluetoothProfileController* aController) MOZ_OVERRIDE;
  virtual void Disconnect(BluetoothProfileController* aController) MOZ_OVERRIDE;
  virtual void OnConnect(const nsAString& aErrorStr) MOZ_OVERRIDE;
  virtual void OnDisconnect(const nsAString& aErrorStr) MOZ_OVERRIDE;

  virtual void GetName(nsACString& aName)
  {
    aName.AssignLiteral("A2DP");
  }

  // A2DP-specific functions
  void HandleSinkPropertyChanged(const BluetoothSignal& aSignal);

  // AVRCP-specific functions
  void SetAvrcpConnected(bool aConnected);
  bool IsAvrcpConnected();
  void UpdateMetaData(const nsAString& aTitle,
                      const nsAString& aArtist,
                      const nsAString& aAlbum,
                      uint32_t aMediaNumber,
                      uint32_t aTotalMediaCount,
                      uint32_t aDuration);
  void UpdatePlayStatus(uint32_t aDuration,
                        uint32_t aPosition,
                        ControlPlayStatus aPlayStatus);
  void GetAlbum(nsAString& aAlbum);
  uint32_t GetDuration();
  ControlPlayStatus GetPlayStatus();
  uint32_t GetPosition();
  uint32_t GetMediaNumber();
  void GetTitle(nsAString& aTitle);

private:
  BluetoothA2dpManager();
  bool Init();

  void HandleShutdown();
  void NotifyConnectionStatusChanged();

  nsString mDeviceAddress;
  nsRefPtr<BluetoothProfileController> mController;

  // A2DP data member
  bool mA2dpConnected;
  SinkState mSinkState;

  // AVRCP data member
  bool mAvrcpConnected;
  nsString mAlbum;
  nsString mArtist;
  nsString mTitle;
  uint32_t mDuration;
  uint32_t mMediaNumber;
  uint32_t mTotalMediaCount;
  uint32_t mPosition;
  ControlPlayStatus mPlayStatus;
};

END_BLUETOOTH_NAMESPACE

#endif
