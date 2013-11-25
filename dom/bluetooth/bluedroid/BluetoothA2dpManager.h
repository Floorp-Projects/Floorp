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
  ~BluetoothA2dpManager();
  void ResetA2dp();
  void ResetAvrcp();

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
  void UpdateRegisterNotification(int aEventId, int aParam);
  void GetAlbum(nsAString& aAlbum);
  uint32_t GetDuration();
  ControlPlayStatus GetPlayStatus();
  uint32_t GetPosition();
  uint32_t GetMediaNumber();
  uint32_t GetTotalMediaNumber();
  void GetTitle(nsAString& aTitle);
  void GetArtist(nsAString& aArtist);
private:
  class SinkPropertyChangedHandler;
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
  /*
   * mPlaybackInterval specifies the time interval (in seconds) at which
   * the change in playback position will be notified. If the song is being
   * forwarded / rewound, a notification will be received whenever the playback
   * position will change by this value.
   */
  uint32_t mPlaybackInterval;
  ControlPlayStatus mPlayStatus;
  /*
   * Notification types: 1. INTERIM 2. CHANGED
   * 1. The initial response to this Notify command shall be an INTERIM
   * response with current status.
   * 2. The following response shall be a CHANGED response with the updated
   * status.
   * mPlayStatusChangedNotifType, mTrackChangedNotifType,
   * mPlayPosChangedNotifType represents current RegisterNotification
   * notification type.
   */
  int mPlayStatusChangedNotifyType;
  int mTrackChangedNotifyType;
  int mPlayPosChangedNotifyType;
};

END_BLUETOOTH_NAMESPACE

#endif
