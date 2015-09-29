/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_BluetoothHfpManager_h
#define mozilla_dom_bluetooth_bluedroid_BluetoothHfpManager_h

#include "BluetoothInterface.h"
#include "BluetoothCommon.h"
#include "BluetoothHfpManagerBase.h"
#include "BluetoothRilListener.h"
#include "BluetoothSocketObserver.h"
#include "mozilla/ipc/SocketBase.h"
#include "mozilla/Hal.h"

BEGIN_BLUETOOTH_NAMESPACE

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

enum PhoneType {
  NONE, // no connection
  GSM,
  CDMA
};

class Call {
public:
  Call();
  void Set(const nsAString& aNumber, const bool aIsOutgoing);
  void Reset();
  bool IsActive();

  uint16_t mState;
  nsString mNumber;
  BluetoothHandsfreeCallDirection mDirection;
  BluetoothHandsfreeCallAddressType mType;
};

class BluetoothHfpManager : public BluetoothHfpManagerBase
                          , public BluetoothHandsfreeNotificationHandler
                          , public BatteryObserver
{
public:
  BT_DECL_HFP_MGR_BASE

  static const int MAX_NUM_CLIENTS;

  void OnConnectError();
  void OnDisconnectError();

  virtual void GetName(nsACString& aName)
  {
    aName.AssignLiteral("HFP/HSP");
  }

  static BluetoothHfpManager* Get();
  static void InitHfpInterface(BluetoothProfileResultHandler* aRes);
  static void DeinitHfpInterface(BluetoothProfileResultHandler* aRes);

  bool ConnectSco();
  bool DisconnectSco();

  /**
   * @param aSend A boolean indicates whether we need to notify headset or not
   */
  void HandleCallStateChanged(uint32_t aCallIndex, uint16_t aCallState,
                              const nsAString& aError, const nsAString& aNumber,
                              const bool aIsOutgoing, const bool aIsConference,
                              bool aSend);
  void HandleIccInfoChanged(uint32_t aClientId);
  void HandleVoiceConnectionChanged(uint32_t aClientId);

  // CDMA-specific functions
  void UpdateSecondNumber(const nsAString& aNumber);
  void AnswerWaitingCall();
  void IgnoreWaitingCall();
  void ToggleCalls();

  // Handle unexpected backend crash
  void HandleBackendError();

  //
  // Bluetooth notifications
  //

  void ConnectionStateNotification(BluetoothHandsfreeConnectionState aState,
                                   const nsAString& aBdAddress) override;
  void AudioStateNotification(BluetoothHandsfreeAudioState aState,
                              const nsAString& aBdAddress) override;
  void AnswerCallNotification(const nsAString& aBdAddress) override;
  void HangupCallNotification(const nsAString& aBdAddress) override;
  void VolumeNotification(BluetoothHandsfreeVolumeType aType,
                          int aVolume,
                          const nsAString& aBdAddress) override;
  void DtmfNotification(char aDtmf,
                        const nsAString& aBdAddress) override;
  void NRECNotification(BluetoothHandsfreeNRECState aNrec,
                        const nsAString& aBdAddr) override;
  void CallHoldNotification(BluetoothHandsfreeCallHoldType aChld,
                            const nsAString& aBdAddress) override;
  void DialCallNotification(const nsAString& aNumber,
                            const nsAString& aBdAddress) override;
  void CnumNotification(const nsAString& aBdAddress) override;
  void CindNotification(const nsAString& aBdAddress) override;
  void CopsNotification(const nsAString& aBdAddress) override;
  void ClccNotification(const nsAString& aBdAddress) override;
  void UnknownAtNotification(const nsACString& aAtString,
                             const nsAString& aBdAddress) override;
  void KeyPressedNotification(const nsAString& aBdAddress) override;

protected:
  virtual ~BluetoothHfpManager();

private:
  class AtResponseResultHandler;
  class CindResponseResultHandler;
  class ConnectAudioResultHandler;
  class ConnectResultHandler;
  class CopsResponseResultHandler;
  class ClccResponseResultHandler;
  class CleanupInitResultHandler;
  class CleanupResultHandler;
  class CloseScoRunnable;
  class CloseScoTask;
  class DeinitResultHandlerRunnable;
  class DeviceStatusNotificationResultHandler;
  class DisconnectAudioResultHandler;
  class DisconnectResultHandler;
  class FormattedAtResponseResultHandler;
  class GetVolumeTask;
  class InitResultHandlerRunnable;
  class MainThreadTask;
  class OnErrorProfileResultHandlerRunnable;
  class PhoneStateChangeResultHandler;
  class RespondToBLDNTask;
  class VolumeControlResultHandler;

  friend class BluetoothHfpManagerObserver;
  friend class GetVolumeTask;
  friend class CloseScoTask;
  friend class RespondToBLDNTask;
  friend class MainThreadTask;

  BluetoothHfpManager();
  bool Init();

  void Cleanup();

  void HandleShutdown();
  void HandleVolumeChanged(nsISupports* aSubject);
  void Notify(const hal::BatteryInformation& aBatteryInfo);

  void NotifyConnectionStateChanged(const nsAString& aType);
  void NotifyDialer(const nsAString& aCommand);

  PhoneType GetPhoneType(const nsAString& aType);
  void ResetCallArray();
  uint32_t FindFirstCall(uint16_t aState);
  uint32_t GetNumberOfCalls(uint16_t aState);
  uint16_t GetCallSetupState();
  bool IsTransitionState(uint16_t aCallState, bool aIsConference);
  BluetoothHandsfreeCallState
    ConvertToBluetoothHandsfreeCallState(int aCallState) const;

  void UpdatePhoneCIND(uint32_t aCallIndex);
  void UpdateDeviceCIND();
  void SendCLCC(Call& aCall, int aIndex);
  void SendLine(const char* aMessage);
  void SendResponse(BluetoothHandsfreeAtResponse aResponseCode);

  BluetoothHandsfreeConnectionState mConnectionState;
  BluetoothHandsfreeConnectionState mPrevConnectionState;
  BluetoothHandsfreeAudioState mAudioState;
  // Device CIND
  int mBattChg;
  BluetoothHandsfreeNetworkState mService;
  BluetoothHandsfreeServiceType mRoam;
  int mSignal;

  int mCurrentVgs;
  int mCurrentVgm;
  bool mReceiveVgsFlag;
  // This flag is for HFP only, not for HSP.
  bool mDialingRequestProcessed;
  bool mNrecEnabled;
  PhoneType mPhoneType;
  nsString mDeviceAddress;
  nsString mMsisdn;
  nsString mOperatorName;

  nsTArray<Call> mCurrentCallArray;
  nsAutoPtr<BluetoothRilListener> mListener;
  nsRefPtr<BluetoothProfileController> mController;

  // CDMA-specific variable
  Call mCdmaSecondCall;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluedroid_BluetoothHfpManager_h
