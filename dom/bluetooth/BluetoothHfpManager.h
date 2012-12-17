/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothhfpmanager_h__
#define mozilla_dom_bluetooth_bluetoothhfpmanager_h__

#include "BluetoothCommon.h"
#include "BluetoothRilListener.h"
#include "mozilla/ipc/UnixSocket.h"
#include "nsIObserver.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothReplyRunnable;
class BluetoothHfpManagerObserver;

class BluetoothHfpManager : public mozilla::ipc::UnixSocketConsumer
{
public:
  ~BluetoothHfpManager();
  static BluetoothHfpManager* Get();
  virtual void ReceiveSocketData(mozilla::ipc::UnixSocketRawData* aMessage)
    MOZ_OVERRIDE;
  bool Connect(const nsAString& aDeviceObjectPath,
               const bool aIsHandsfree,
               BluetoothReplyRunnable* aRunnable);
  void Disconnect();
  bool SendLine(const char* aMessage);
  bool SendCommand(const char* aCommand, const int aValue);
  void CallStateChanged(int aCallIndex, int aCallState,
                        const char* aNumber, bool aIsActive);
  void EnumerateCallState(int aCallIndex, int aCallState,
                          const char* aNumber, bool aIsActive);
  void SetupCIND(int aCallIndex, int aCallState,
                 const char* aPhoneNumber, bool aInitial);
  bool Listen();
  void SetVolume(int aVolume);

private:
  friend class BluetoothHfpManagerObserver;
  BluetoothHfpManager();
  nsresult HandleVolumeChanged(const nsAString& aData);
  nsresult HandleShutdown();
  nsresult HandleIccInfoChanged();

  bool Init();
  void Cleanup();
  void NotifyDialer(const nsAString& aCommand);
  void NotifySettings();
  virtual void OnConnectSuccess() MOZ_OVERRIDE;
  virtual void OnConnectError() MOZ_OVERRIDE;
  virtual void OnDisconnect() MOZ_OVERRIDE;

  int mCurrentVgs;
  int mCurrentCallIndex;
  bool mCLIP;
  bool mReceiveVgsFlag;
  nsString mDevicePath;
  nsString mMsisdn;
  enum mozilla::ipc::SocketConnectionStatus mSocketStatus;
  nsTArray<int> mCurrentCallStateArray;
  nsAutoPtr<BluetoothRilListener> mListener;
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
};

END_BLUETOOTH_NAMESPACE

#endif
