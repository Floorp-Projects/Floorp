/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothhfpmanager_h__
#define mozilla_dom_bluetooth_bluetoothhfpmanager_h__

#include "BluetoothCommon.h"
#include "BluetoothProfileManagerBase.h"
#include "BluetoothSocketObserver.h"
#include "BluetoothTelephonyListener.h"
#include "mozilla/ipc/UnixSocket.h"
#include "nsIObserver.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothHfpManagerObserver;
class BluetoothReplyRunnable;
class BluetoothSocket;
class Call;

/**
 * These costants are defined in 4.33.2 "AT Capabilities Re-Used from GSM 07.07
 * and 3GPP 27.007" in Bluetooth hands-free profile 1.6
 */
enum BluetoothCmeError {
  AG_FAILURE = 0,
  NO_CONNECTION_TO_PHONE = 1,
  OPERATION_NOT_ALLOWED = 3,
  OPERATION_NOT_SUPPORTED = 4,
  PIN_REQUIRED = 5,
  SIM_NOT_INSERTED = 10,
  SIM_PIN_REQUIRED = 11,
  SIM_PUK_REQUIRED = 12,
  SIM_FAILURE = 13,
  SIM_BUSY = 14,
  INCORRECT_PASSWORD = 16,
  SIM_PIN2_REQUIRED = 17,
  SIM_PUK2_REQUIRED = 18,
  MEMORY_FULL = 20,
  INVALID_INDEX = 21,
  MEMORY_FAILURE = 23,
  TEXT_STRING_TOO_LONG = 24,
  INVALID_CHARACTERS_IN_TEXT_STRING = 25,
  DIAL_STRING_TOO_LONG = 26,
  INVALID_CHARACTERS_IN_DIAL_STRING = 27,
  NO_NETWORK_SERVICE = 30,
  NETWORK_TIMEOUT = 31,
  NETWORK_NOT_ALLOWED = 32
};

class BluetoothHfpManager : public BluetoothSocketObserver
                          , public BluetoothProfileManagerBase
{
public:
  NS_DECL_ISUPPORTS

  static BluetoothHfpManager* Get();
  ~BluetoothHfpManager();

  virtual void ReceiveSocketData(
    BluetoothSocket* aSocket,
    nsAutoPtr<mozilla::ipc::UnixSocketRawData>& aMessage) MOZ_OVERRIDE;
  virtual void OnConnectSuccess(BluetoothSocket* aSocket) MOZ_OVERRIDE;
  virtual void OnConnectError(BluetoothSocket* aSocket) MOZ_OVERRIDE;
  virtual void OnDisconnect(BluetoothSocket* aSocket) MOZ_OVERRIDE;
  virtual void OnGetServiceChannel(const nsAString& aDeviceAddress,
                                   const nsAString& aServiceUuid,
                                   int aChannel) MOZ_OVERRIDE;
  virtual void OnUpdateSdpRecords(const nsAString& aDeviceAddress) MOZ_OVERRIDE;
  virtual void GetAddress(nsAString& aDeviceAddress) MOZ_OVERRIDE;

  void Connect(const nsAString& aDeviceAddress,
               const bool aIsHandsfree,
               BluetoothReplyRunnable* aRunnable);
  void Disconnect();
  bool Listen();
  bool ConnectSco(BluetoothReplyRunnable* aRunnable = nullptr);
  bool DisconnectSco();
  bool ListenSco();

  /**
   * @param aSend A boolean indicates whether we need to notify headset or not
   */
  void HandleCallStateChanged(uint32_t aCallIndex, uint16_t aCallState,
                              const nsAString& aNumber, const bool aIsOutgoing,
                              bool aSend);

  bool IsConnected();
  bool IsScoConnected();

private:
  class GetVolumeTask;
  class RespondToBLDNTask;
  class SendRingIndicatorTask;

  friend class GetVolumeTask;
  friend class RespondToBLDNTask;
  friend class SendRingIndicatorTask;
  friend class BluetoothHfpManagerObserver;

  BluetoothHfpManager();
  nsresult HandleIccInfoChanged();
  nsresult HandleShutdown();
  nsresult HandleVolumeChanged(const nsAString& aData);
  nsresult HandleVoiceConnectionChanged();

  bool Init();
  void Cleanup();
  void Reset();
  void ResetCallArray();
  uint32_t FindFirstCall(uint16_t aState);
  uint32_t GetNumberOfCalls(uint16_t aState);

  void NotifyDialer(const nsAString& aCommand);
  void NotifyStatusChanged(const nsAString& aType);
  void NotifyAudioManager(bool aStatus);

  bool SendCommand(const char* aCommand, uint32_t aValue = 0);
  bool SendLine(const char* aMessage);
  void UpdateCIND(uint8_t aType, uint8_t aValue, bool aSend);
  void OnScoConnectSuccess();
  void OnScoConnectError();
  void OnScoDisconnect();

  int mCurrentVgs;
  int mCurrentVgm;
  bool mBSIR;
  bool mCCWA;
  bool mCLIP;
  bool mCMEE;
  bool mCMER;
  bool mFirstCKPD;
  int mNetworkSelectionMode;
  bool mReceiveVgsFlag;
  bool mDialingRequestProcessed;
  bool mIsHandsfree;
  bool mNeedsUpdatingSdpRecords;
  nsString mDeviceAddress;
  nsString mMsisdn;
  nsString mOperatorName;

  nsTArray<Call> mCurrentCallArray;
  nsAutoPtr<BluetoothTelephonyListener> mListener;
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
  nsRefPtr<BluetoothReplyRunnable> mScoRunnable;

  // If a connection has been established, mSocket will be the socket
  // communicating with the remote socket. We maintain the invariant that if
  // mSocket is non-null, mHandsfreeSocket and mHeadsetSocket must be null (and
  // vice versa).
  nsRefPtr<BluetoothSocket> mSocket;

  // Server sockets. Once an inbound connection is established, it will hand
  // over the ownership to mSocket, and get a new server socket while Listen()
  // is called.
  nsRefPtr<BluetoothSocket> mHandsfreeSocket;
  nsRefPtr<BluetoothSocket> mHeadsetSocket;
  nsRefPtr<BluetoothSocket> mScoSocket;
  SocketConnectionStatus mScoSocketStatus;
};

END_BLUETOOTH_NAMESPACE

#endif
