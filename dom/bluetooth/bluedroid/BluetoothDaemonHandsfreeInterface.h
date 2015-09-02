/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_BluetoothDaemonHandsfreeInterface_h
#define mozilla_dom_bluetooth_bluedroid_BluetoothDaemonHandsfreeInterface_h

#include "BluetoothDaemonHelpers.h"
#include "BluetoothInterface.h"
#include "mozilla/ipc/DaemonRunnables.h"

BEGIN_BLUETOOTH_NAMESPACE

using mozilla::ipc::DaemonSocketPDU;
using mozilla::ipc::DaemonSocketPDUHeader;

class BluetoothSetupResultHandler;

class BluetoothDaemonHandsfreeModule
{
public:
  enum {
    SERVICE_ID = 0x05
  };

  enum {
    OPCODE_ERROR = 0x00,
    OPCODE_CONNECT = 0x01,
    OPCODE_DISCONNECT = 0x02,
    OPCODE_CONNECT_AUDIO = 0x03,
    OPCODE_DISCONNECT_AUDIO = 0x04,
    OPCODE_START_VOICE_RECOGNITION = 0x05,
    OPCODE_STOP_VOICE_RECOGNITION =0x06,
    OPCODE_VOLUME_CONTROL = 0x07,
    OPCODE_DEVICE_STATUS_NOTIFICATION = 0x08,
    OPCODE_COPS_RESPONSE = 0x09,
    OPCODE_CIND_RESPONSE = 0x0a,
    OPCODE_FORMATTED_AT_RESPONSE = 0x0b,
    OPCODE_AT_RESPONSE = 0x0c,
    OPCODE_CLCC_RESPONSE = 0x0d,
    OPCODE_PHONE_STATE_CHANGE = 0x0e,
    OPCODE_CONFIGURE_WBS = 0x0f
  };

  virtual nsresult Send(DaemonSocketPDU* aPDU, void* aUserData) = 0;

  virtual nsresult RegisterModule(uint8_t aId, uint8_t aMode,
                                  uint32_t aMaxNumClients,
                                  BluetoothSetupResultHandler* aRes) = 0;

  virtual nsresult UnregisterModule(uint8_t aId,
                                    BluetoothSetupResultHandler* aRes) = 0;

  void SetNotificationHandler(
    BluetoothHandsfreeNotificationHandler* aNotificationHandler);

  //
  // Commands
  //

  nsresult ConnectCmd(const nsAString& aBdAddr,
                      BluetoothHandsfreeResultHandler* aRes);
  nsresult DisconnectCmd(const nsAString& aBdAddr,
                         BluetoothHandsfreeResultHandler* aRes);
  nsresult ConnectAudioCmd(const nsAString& aBdAddr,
                           BluetoothHandsfreeResultHandler* aRes);
  nsresult DisconnectAudioCmd(const nsAString& aBdAddr,
                              BluetoothHandsfreeResultHandler* aRes);

  /* Voice Recognition */

  nsresult StartVoiceRecognitionCmd(const nsAString& aBdAddr,
                                    BluetoothHandsfreeResultHandler* aRes);
  nsresult StopVoiceRecognitionCmd(const nsAString& aBdAddr,
                                   BluetoothHandsfreeResultHandler* aRes);

  /* Volume */

  nsresult VolumeControlCmd(BluetoothHandsfreeVolumeType aType, int aVolume,
                            const nsAString& aBdAddr,
                            BluetoothHandsfreeResultHandler* aRes);

  /* Device status */

  nsresult DeviceStatusNotificationCmd(
    BluetoothHandsfreeNetworkState aNtkState,
    BluetoothHandsfreeServiceType aSvcType,
    int aSignal, int aBattChg,
    BluetoothHandsfreeResultHandler* aRes);

  /* Responses */

  nsresult CopsResponseCmd(const char* aCops, const nsAString& aBdAddr,
                           BluetoothHandsfreeResultHandler* aRes);
  nsresult CindResponseCmd(int aSvc, int aNumActive, int aNumHeld,
                           BluetoothHandsfreeCallState aCallSetupState,
                           int aSignal, int aRoam, int aBattChg,
                           const nsAString& aBdAddr,
                           BluetoothHandsfreeResultHandler* aRes);
  nsresult FormattedAtResponseCmd(const char* aRsp, const nsAString& aBdAddr,
                                  BluetoothHandsfreeResultHandler* aRes);
  nsresult AtResponseCmd(BluetoothHandsfreeAtResponse aResponseCode,
                         int aErrorCode, const nsAString& aBdAddr,
                         BluetoothHandsfreeResultHandler* aRes);
  nsresult ClccResponseCmd(int aIndex, BluetoothHandsfreeCallDirection aDir,
                           BluetoothHandsfreeCallState aState,
                           BluetoothHandsfreeCallMode aMode,
                           BluetoothHandsfreeCallMptyType aMpty,
                           const nsAString& aNumber,
                           BluetoothHandsfreeCallAddressType aType,
                           const nsAString& aBdAddr,
                           BluetoothHandsfreeResultHandler* aRes);

  /* Phone State */

  nsresult PhoneStateChangeCmd(int aNumActive, int aNumHeld,
                               BluetoothHandsfreeCallState aCallSetupState,
                               const nsAString& aNumber,
                               BluetoothHandsfreeCallAddressType aType,
                               BluetoothHandsfreeResultHandler* aRes);

  /* Wide Band Speech */

  nsresult ConfigureWbsCmd(const nsAString& aBdAddr,
                           BluetoothHandsfreeWbsConfig aConfig,
                           BluetoothHandsfreeResultHandler* aRes);

protected:
  nsresult Send(DaemonSocketPDU* aPDU,
                BluetoothHandsfreeResultHandler* aRes);

  void HandleSvc(const DaemonSocketPDUHeader& aHeader,
                 DaemonSocketPDU& aPDU, void* aUserData);

  //
  // Responses
  //

  typedef mozilla::ipc::DaemonResultRunnable0<
    BluetoothHandsfreeResultHandler, void>
    ResultRunnable;

  typedef mozilla::ipc::DaemonResultRunnable1<
    BluetoothHandsfreeResultHandler, void, BluetoothStatus, BluetoothStatus>
    ErrorRunnable;

  void ErrorRsp(const DaemonSocketPDUHeader& aHeader,
                DaemonSocketPDU& aPDU,
                BluetoothHandsfreeResultHandler* aRes);

  void ConnectRsp(const DaemonSocketPDUHeader& aHeader,
                  DaemonSocketPDU& aPDU,
                  BluetoothHandsfreeResultHandler* aRes);

  void DisconnectRsp(const DaemonSocketPDUHeader& aHeader,
                     DaemonSocketPDU& aPDU,
                     BluetoothHandsfreeResultHandler* aRes);

  void ConnectAudioRsp(const DaemonSocketPDUHeader& aHeader,
                       DaemonSocketPDU& aPDU,
                       BluetoothHandsfreeResultHandler* aRes);

  void DisconnectAudioRsp(const DaemonSocketPDUHeader& aHeader,
                          DaemonSocketPDU& aPDU,
                          BluetoothHandsfreeResultHandler* aRes);

  void StartVoiceRecognitionRsp(const DaemonSocketPDUHeader& aHeader,
                                DaemonSocketPDU& aPDU,
                                BluetoothHandsfreeResultHandler* aRes);

  void StopVoiceRecognitionRsp(const DaemonSocketPDUHeader& aHeader,
                               DaemonSocketPDU& aPDU,
                               BluetoothHandsfreeResultHandler* aRes);

  void VolumeControlRsp(const DaemonSocketPDUHeader& aHeader,
                        DaemonSocketPDU& aPDU,
                        BluetoothHandsfreeResultHandler* aRes);

  void DeviceStatusNotificationRsp(const DaemonSocketPDUHeader& aHeader,
                                   DaemonSocketPDU& aPDU,
                                   BluetoothHandsfreeResultHandler* aRes);

  void CopsResponseRsp(const DaemonSocketPDUHeader& aHeader,
                       DaemonSocketPDU& aPDU,
                       BluetoothHandsfreeResultHandler* aRes);

  void CindResponseRsp(const DaemonSocketPDUHeader& aHeader,
                       DaemonSocketPDU& aPDU,
                       BluetoothHandsfreeResultHandler* aRes);

  void FormattedAtResponseRsp(const DaemonSocketPDUHeader& aHeader,
                              DaemonSocketPDU& aPDU,
                              BluetoothHandsfreeResultHandler* aRes);

  void AtResponseRsp(const DaemonSocketPDUHeader& aHeader,
                     DaemonSocketPDU& aPDU,
                     BluetoothHandsfreeResultHandler* aRes);

  void ClccResponseRsp(const DaemonSocketPDUHeader& aHeader,
                       DaemonSocketPDU& aPDU,
                       BluetoothHandsfreeResultHandler* aRes);

  void PhoneStateChangeRsp(const DaemonSocketPDUHeader& aHeader,
                           DaemonSocketPDU& aPDU,
                           BluetoothHandsfreeResultHandler* aRes);

  void ConfigureWbsRsp(const DaemonSocketPDUHeader& aHeader,
                       DaemonSocketPDU& aPDU,
                       BluetoothHandsfreeResultHandler* aRes);

  void HandleRsp(const DaemonSocketPDUHeader& aHeader,
                 DaemonSocketPDU& aPDU,
                 void* aUserData);

  //
  // Notifications
  //

  class NotificationHandlerWrapper;

  typedef mozilla::ipc::DaemonNotificationRunnable2<
    NotificationHandlerWrapper, void, BluetoothHandsfreeConnectionState,
    nsString, BluetoothHandsfreeConnectionState, const nsAString&>
    ConnectionStateNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable2<
    NotificationHandlerWrapper, void, BluetoothHandsfreeAudioState,
    nsString, BluetoothHandsfreeAudioState, const nsAString&>
    AudioStateNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable2<
    NotificationHandlerWrapper, void, BluetoothHandsfreeVoiceRecognitionState,
    nsString, BluetoothHandsfreeVoiceRecognitionState, const nsAString&>
    VoiceRecognitionNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable1<
    NotificationHandlerWrapper, void, nsString, const nsAString&>
    AnswerCallNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable1<
    NotificationHandlerWrapper, void, nsString, const nsAString&>
    HangupCallNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable3<
    NotificationHandlerWrapper, void, BluetoothHandsfreeVolumeType,
    int, nsString, BluetoothHandsfreeVolumeType, int, const nsAString&>
    VolumeNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable2<
    NotificationHandlerWrapper, void, nsString, nsString, const nsAString&,
    const nsAString&>
    DialCallNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable2<
    NotificationHandlerWrapper, void, char, nsString, char, const nsAString&>
    DtmfNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable2<
    NotificationHandlerWrapper, void, BluetoothHandsfreeNRECState, nsString,
    BluetoothHandsfreeNRECState, const nsAString&>
    NRECNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable2<
    NotificationHandlerWrapper, void, BluetoothHandsfreeCallHoldType,
    nsString, BluetoothHandsfreeCallHoldType, const nsAString&>
    CallHoldNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable1<
    NotificationHandlerWrapper, void, nsString, const nsAString&>
    CnumNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable1<
    NotificationHandlerWrapper, void, nsString, const nsAString&>
    CindNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable1<
    NotificationHandlerWrapper, void, nsString, const nsAString&>
    CopsNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable1<
    NotificationHandlerWrapper, void, nsString, const nsAString&>
    ClccNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable2<
    NotificationHandlerWrapper, void, nsCString, nsString,
    const nsACString&, const nsAString&>
    UnknownAtNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable1<
    NotificationHandlerWrapper, void, nsString, const nsAString&>
    KeyPressedNotification;

  typedef mozilla::ipc::DaemonNotificationRunnable2<
    NotificationHandlerWrapper, void, BluetoothHandsfreeWbsConfig, nsString,
    BluetoothHandsfreeWbsConfig, const nsAString&>
    WbsNotification;

  class ConnectionStateInitOp;
  class AudioStateInitOp;
  class VoiceRecognitionInitOp;
  class AnswerCallInitOp;
  class HangupCallInitOp;
  class VolumeInitOp;
  class DialCallInitOp;
  class DtmfInitOp;
  class NRECInitOp;
  class CallHoldInitOp;
  class CnumInitOp;
  class CindInitOp;
  class CopsInitOp;
  class ClccInitOp;
  class VolumeInitOp;
  class UnknownAtInitOp;
  class KeyPressedInitOp;
  class WbsInitOp;

  void ConnectionStateNtf(const DaemonSocketPDUHeader& aHeader,
                          DaemonSocketPDU& aPDU);

  void AudioStateNtf(const DaemonSocketPDUHeader& aHeader,
                     DaemonSocketPDU& aPDU);

  void VoiceRecognitionNtf(const DaemonSocketPDUHeader& aHeader,
                           DaemonSocketPDU& aPDU);

  void AnswerCallNtf(const DaemonSocketPDUHeader& aHeader,
                     DaemonSocketPDU& aPDU);

  void HangupCallNtf(const DaemonSocketPDUHeader& aHeader,
                     DaemonSocketPDU& aPDU);

  void VolumeNtf(const DaemonSocketPDUHeader& aHeader,
                 DaemonSocketPDU& aPDU);

  void DialCallNtf(const DaemonSocketPDUHeader& aHeader,
                   DaemonSocketPDU& aPDU);

  void DtmfNtf(const DaemonSocketPDUHeader& aHeader,
               DaemonSocketPDU& aPDU);

  void NRECNtf(const DaemonSocketPDUHeader& aHeader,
               DaemonSocketPDU& aPDU);

  void CallHoldNtf(const DaemonSocketPDUHeader& aHeader,
                   DaemonSocketPDU& aPDU);

  void CnumNtf(const DaemonSocketPDUHeader& aHeader,
               DaemonSocketPDU& aPDU);

  void CindNtf(const DaemonSocketPDUHeader& aHeader,
               DaemonSocketPDU& aPDU);

  void CopsNtf(const DaemonSocketPDUHeader& aHeader,
               DaemonSocketPDU& aPDU);

  void ClccNtf(const DaemonSocketPDUHeader& aHeader,
               DaemonSocketPDU& aPDU);

  void UnknownAtNtf(const DaemonSocketPDUHeader& aHeader,
                    DaemonSocketPDU& aPDU);

  void KeyPressedNtf(const DaemonSocketPDUHeader& aHeader,
                     DaemonSocketPDU& aPDU);

  void WbsNtf(const DaemonSocketPDUHeader& aHeader,
              DaemonSocketPDU& aPDU);

  void HandleNtf(const DaemonSocketPDUHeader& aHeader,
                 DaemonSocketPDU& aPDU,
                 void* aUserData);

  static BluetoothHandsfreeNotificationHandler* sNotificationHandler;
#if ANDROID_VERSION < 21
  /* |sConnectedDeviceAddress| stores Bluetooth device address of the
   * connected device. Before BlueZ 5.25, we maintain this address by ourselves
   * through ConnectionStateNtf(); after BlueZ 5.25, every callback carries
   * this address directly so we don't have to keep it.
   */
  static nsString sConnectedDeviceAddress;
#endif
};

class BluetoothDaemonHandsfreeInterface final
  : public BluetoothHandsfreeInterface
{
  class CleanupResultHandler;
  class InitResultHandler;

  enum {
    MODE_HEADSET = 0x00,
    MODE_NARROWBAND_SPEECH = 0x01,
    MODE_NARRAWBAND_WIDEBAND_SPEECH = 0x02
  };

public:
  BluetoothDaemonHandsfreeInterface(BluetoothDaemonHandsfreeModule* aModule);
  ~BluetoothDaemonHandsfreeInterface();

  void Init(
    BluetoothHandsfreeNotificationHandler* aNotificationHandler,
    int aMaxNumClients, BluetoothHandsfreeResultHandler* aRes);
  void Cleanup(BluetoothHandsfreeResultHandler* aRes);

  /* Connect / Disconnect */

  void Connect(const nsAString& aBdAddr,
               BluetoothHandsfreeResultHandler* aRes);
  void Disconnect(const nsAString& aBdAddr,
                  BluetoothHandsfreeResultHandler* aRes);
  void ConnectAudio(const nsAString& aBdAddr,
                    BluetoothHandsfreeResultHandler* aRes);
  void DisconnectAudio(const nsAString& aBdAddr,
                       BluetoothHandsfreeResultHandler* aRes);

  /* Voice Recognition */

  void StartVoiceRecognition(const nsAString& aBdAddr,
                             BluetoothHandsfreeResultHandler* aRes);
  void StopVoiceRecognition(const nsAString& aBdAddr,
                            BluetoothHandsfreeResultHandler* aRes);

  /* Volume */

  void VolumeControl(BluetoothHandsfreeVolumeType aType, int aVolume,
                     const nsAString& aBdAddr,
                     BluetoothHandsfreeResultHandler* aRes);

  /* Device status */

  void DeviceStatusNotification(BluetoothHandsfreeNetworkState aNtkState,
                                BluetoothHandsfreeServiceType aSvcType,
                                int aSignal, int aBattChg,
                                BluetoothHandsfreeResultHandler* aRes);

  /* Responses */

  void CopsResponse(const char* aCops, const nsAString& aBdAddr,
                    BluetoothHandsfreeResultHandler* aRes);
  void CindResponse(int aSvc, int aNumActive, int aNumHeld,
                    BluetoothHandsfreeCallState aCallSetupState,
                    int aSignal, int aRoam, int aBattChg,
                    const nsAString& aBdAddr,
                    BluetoothHandsfreeResultHandler* aRes);
  void FormattedAtResponse(const char* aRsp, const nsAString& aBdAddr,
                           BluetoothHandsfreeResultHandler* aRes);
  void AtResponse(BluetoothHandsfreeAtResponse aResponseCode, int aErrorCode,
                  const nsAString& aBdAddr,
                  BluetoothHandsfreeResultHandler* aRes);
  void ClccResponse(int aIndex, BluetoothHandsfreeCallDirection aDir,
                    BluetoothHandsfreeCallState aState,
                    BluetoothHandsfreeCallMode aMode,
                    BluetoothHandsfreeCallMptyType aMpty,
                    const nsAString& aNumber,
                    BluetoothHandsfreeCallAddressType aType,
                    const nsAString& aBdAddr,
                    BluetoothHandsfreeResultHandler* aRes);

  /* Phone State */

  void PhoneStateChange(int aNumActive, int aNumHeld,
                        BluetoothHandsfreeCallState aCallSetupState,
                        const nsAString& aNumber,
                        BluetoothHandsfreeCallAddressType aType,
                        BluetoothHandsfreeResultHandler* aRes);

  /* Wide Band Speech */
  void ConfigureWbs(const nsAString& aBdAddr,
                    BluetoothHandsfreeWbsConfig aConfig,
                    BluetoothHandsfreeResultHandler* aRes);

private:
  void DispatchError(BluetoothHandsfreeResultHandler* aRes,
                     BluetoothStatus aStatus);
  void DispatchError(BluetoothHandsfreeResultHandler* aRes, nsresult aRv);

  BluetoothDaemonHandsfreeModule* mModule;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluedroid_BluetoothDaemonHandsfreeInterface_h
