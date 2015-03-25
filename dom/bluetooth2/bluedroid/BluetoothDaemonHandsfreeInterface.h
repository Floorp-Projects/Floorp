/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothdaemonhandsfreeinterface_h
#define mozilla_dom_bluetooth_bluetoothdaemonhandsfreeinterface_h

#include "BluetoothDaemonHelpers.h"
#include "BluetoothInterface.h"
#include "BluetoothInterfaceHelpers.h"

BEGIN_BLUETOOTH_NAMESPACE

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
    OPCODE_PHONE_STATE_CHANGE = 0x0e
  };

  virtual nsresult Send(BluetoothDaemonPDU* aPDU, void* aUserData) = 0;

  virtual nsresult RegisterModule(uint8_t aId, uint8_t aMode,
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

  nsresult StartVoiceRecognitionCmd(BluetoothHandsfreeResultHandler* aRes);
  nsresult StopVoiceRecognitionCmd(BluetoothHandsfreeResultHandler* aRes);

  /* Volume */

  nsresult VolumeControlCmd(BluetoothHandsfreeVolumeType aType, int aVolume,
                            BluetoothHandsfreeResultHandler* aRes);

  /* Device status */

  nsresult DeviceStatusNotificationCmd(
    BluetoothHandsfreeNetworkState aNtkState,
    BluetoothHandsfreeServiceType aSvcType,
    int aSignal, int aBattChg,
    BluetoothHandsfreeResultHandler* aRes);

  /* Responses */

  nsresult CopsResponseCmd(const char* aCops,
                           BluetoothHandsfreeResultHandler* aRes);
  nsresult CindResponseCmd(int aSvc, int aNumActive, int aNumHeld,
                           BluetoothHandsfreeCallState aCallSetupState,
                           int aSignal, int aRoam, int aBattChg,
                           BluetoothHandsfreeResultHandler* aRes);
  nsresult FormattedAtResponseCmd(const char* aRsp,
                                  BluetoothHandsfreeResultHandler* aRes);
  nsresult AtResponseCmd(BluetoothHandsfreeAtResponse aResponseCode,
                         int aErrorCode,
                         BluetoothHandsfreeResultHandler* aRes);
  nsresult ClccResponseCmd(int aIndex, BluetoothHandsfreeCallDirection aDir,
                           BluetoothHandsfreeCallState aState,
                           BluetoothHandsfreeCallMode aMode,
                           BluetoothHandsfreeCallMptyType aMpty,
                           const nsAString& aNumber,
                           BluetoothHandsfreeCallAddressType aType,
                           BluetoothHandsfreeResultHandler* aRes);

  /* Phone State */

  nsresult PhoneStateChangeCmd(int aNumActive, int aNumHeld,
                               BluetoothHandsfreeCallState aCallSetupState,
                               const nsAString& aNumber,
                               BluetoothHandsfreeCallAddressType aType,
                               BluetoothHandsfreeResultHandler* aRes);

protected:
  nsresult Send(BluetoothDaemonPDU* aPDU,
                BluetoothHandsfreeResultHandler* aRes);

  void HandleSvc(const BluetoothDaemonPDUHeader& aHeader,
                 BluetoothDaemonPDU& aPDU, void* aUserData);

  //
  // Responses
  //

  typedef BluetoothResultRunnable0<BluetoothHandsfreeResultHandler, void>
    ResultRunnable;

  typedef BluetoothResultRunnable1<BluetoothHandsfreeResultHandler, void,
                                   BluetoothStatus, BluetoothStatus>
    ErrorRunnable;

  void ErrorRsp(const BluetoothDaemonPDUHeader& aHeader,
                BluetoothDaemonPDU& aPDU,
                BluetoothHandsfreeResultHandler* aRes);

  void ConnectRsp(const BluetoothDaemonPDUHeader& aHeader,
                  BluetoothDaemonPDU& aPDU,
                  BluetoothHandsfreeResultHandler* aRes);

  void DisconnectRsp(const BluetoothDaemonPDUHeader& aHeader,
                     BluetoothDaemonPDU& aPDU,
                     BluetoothHandsfreeResultHandler* aRes);

  void ConnectAudioRsp(const BluetoothDaemonPDUHeader& aHeader,
                       BluetoothDaemonPDU& aPDU,
                       BluetoothHandsfreeResultHandler* aRes);

  void DisconnectAudioRsp(const BluetoothDaemonPDUHeader& aHeader,
                          BluetoothDaemonPDU& aPDU,
                          BluetoothHandsfreeResultHandler* aRes);

  void StartVoiceRecognitionRsp(const BluetoothDaemonPDUHeader& aHeader,
                                BluetoothDaemonPDU& aPDU,
                                BluetoothHandsfreeResultHandler* aRes);

  void StopVoiceRecognitionRsp(const BluetoothDaemonPDUHeader& aHeader,
                               BluetoothDaemonPDU& aPDU,
                               BluetoothHandsfreeResultHandler* aRes);

  void VolumeControlRsp(const BluetoothDaemonPDUHeader& aHeader,
                        BluetoothDaemonPDU& aPDU,
                        BluetoothHandsfreeResultHandler* aRes);

  void DeviceStatusNotificationRsp(const BluetoothDaemonPDUHeader& aHeader,
                                   BluetoothDaemonPDU& aPDU,
                                   BluetoothHandsfreeResultHandler* aRes);

  void CopsResponseRsp(const BluetoothDaemonPDUHeader& aHeader,
                       BluetoothDaemonPDU& aPDU,
                       BluetoothHandsfreeResultHandler* aRes);

  void CindResponseRsp(const BluetoothDaemonPDUHeader& aHeader,
                       BluetoothDaemonPDU& aPDU,
                       BluetoothHandsfreeResultHandler* aRes);

  void FormattedAtResponseRsp(const BluetoothDaemonPDUHeader& aHeader,
                              BluetoothDaemonPDU& aPDU,
                              BluetoothHandsfreeResultHandler* aRes);

  void AtResponseRsp(const BluetoothDaemonPDUHeader& aHeader,
                     BluetoothDaemonPDU& aPDU,
                     BluetoothHandsfreeResultHandler* aRes);

  void ClccResponseRsp(const BluetoothDaemonPDUHeader& aHeader,
                       BluetoothDaemonPDU& aPDU,
                       BluetoothHandsfreeResultHandler* aRes);

  void PhoneStateChangeRsp(const BluetoothDaemonPDUHeader& aHeader,
                           BluetoothDaemonPDU& aPDU,
                           BluetoothHandsfreeResultHandler* aRes);

  void HandleRsp(const BluetoothDaemonPDUHeader& aHeader,
                 BluetoothDaemonPDU& aPDU,
                 void* aUserData);

  //
  // Notifications
  //

  class NotificationHandlerWrapper;

  typedef BluetoothNotificationRunnable2<NotificationHandlerWrapper, void,
                                         BluetoothHandsfreeConnectionState,
                                         nsString,
                                         BluetoothHandsfreeConnectionState,
                                         const nsAString&>
    ConnectionStateNotification;

  typedef BluetoothNotificationRunnable2<NotificationHandlerWrapper, void,
                                         BluetoothHandsfreeAudioState,
                                         nsString,
                                         BluetoothHandsfreeAudioState,
                                         const nsAString&>
    AudioStateNotification;

  typedef BluetoothNotificationRunnable1<NotificationHandlerWrapper, void,
    BluetoothHandsfreeVoiceRecognitionState>
    VoiceRecognitionNotification;

  typedef BluetoothNotificationRunnable0<NotificationHandlerWrapper, void>
    AnswerCallNotification;

  typedef BluetoothNotificationRunnable0<NotificationHandlerWrapper, void>
    HangupCallNotification;

  typedef BluetoothNotificationRunnable2<NotificationHandlerWrapper, void,
                                         BluetoothHandsfreeVolumeType, int>
    VolumeNotification;

  typedef BluetoothNotificationRunnable1<NotificationHandlerWrapper, void,
                                         nsString, const nsAString&>
    DialCallNotification;

  typedef BluetoothNotificationRunnable1<NotificationHandlerWrapper, void,
                                         char>
    DtmfNotification;

  typedef BluetoothNotificationRunnable1<NotificationHandlerWrapper, void,
                                         BluetoothHandsfreeNRECState>
    NRECNotification;

  typedef BluetoothNotificationRunnable1<NotificationHandlerWrapper, void,
                                         BluetoothHandsfreeCallHoldType>
    CallHoldNotification;

  typedef BluetoothNotificationRunnable0<NotificationHandlerWrapper, void>
    CnumNotification;

  typedef BluetoothNotificationRunnable0<NotificationHandlerWrapper, void>
    CindNotification;

  typedef BluetoothNotificationRunnable0<NotificationHandlerWrapper, void>
    CopsNotification;

  typedef BluetoothNotificationRunnable0<NotificationHandlerWrapper, void>
    ClccNotification;

  typedef BluetoothNotificationRunnable1<NotificationHandlerWrapper, void,
                                         nsCString, const nsACString&>
    UnknownAtNotification;

  typedef BluetoothNotificationRunnable0<NotificationHandlerWrapper, void>
    KeyPressedNotification;

  class AudioStateInitOp;
  class ConnectionStateInitOp;
  class DialCallInitOp;
  class VolumeInitOp;
  class UnknownAtInitOp;

  void ConnectionStateNtf(const BluetoothDaemonPDUHeader& aHeader,
                          BluetoothDaemonPDU& aPDU);

  void AudioStateNtf(const BluetoothDaemonPDUHeader& aHeader,
                     BluetoothDaemonPDU& aPDU);

  void VoiceRecognitionNtf(const BluetoothDaemonPDUHeader& aHeader,
                           BluetoothDaemonPDU& aPDU);

  void AnswerCallNtf(const BluetoothDaemonPDUHeader& aHeader,
                     BluetoothDaemonPDU& aPDU);

  void HangupCallNtf(const BluetoothDaemonPDUHeader& aHeader,
                     BluetoothDaemonPDU& aPDU);

  void VolumeNtf(const BluetoothDaemonPDUHeader& aHeader,
                 BluetoothDaemonPDU& aPDU);

  void DialCallNtf(const BluetoothDaemonPDUHeader& aHeader,
                   BluetoothDaemonPDU& aPDU);

  void DtmfNtf(const BluetoothDaemonPDUHeader& aHeader,
               BluetoothDaemonPDU& aPDU);

  void NRECNtf(const BluetoothDaemonPDUHeader& aHeader,
               BluetoothDaemonPDU& aPDU);

  void CallHoldNtf(const BluetoothDaemonPDUHeader& aHeader,
                   BluetoothDaemonPDU& aPDU);

  void CnumNtf(const BluetoothDaemonPDUHeader& aHeader,
               BluetoothDaemonPDU& aPDU);

  void CindNtf(const BluetoothDaemonPDUHeader& aHeader,
               BluetoothDaemonPDU& aPDU);

  void CopsNtf(const BluetoothDaemonPDUHeader& aHeader,
               BluetoothDaemonPDU& aPDU);

  void ClccNtf(const BluetoothDaemonPDUHeader& aHeader,
               BluetoothDaemonPDU& aPDU);

  void UnknownAtNtf(const BluetoothDaemonPDUHeader& aHeader,
                    BluetoothDaemonPDU& aPDU);

  void KeyPressedNtf(const BluetoothDaemonPDUHeader& aHeader,
                     BluetoothDaemonPDU& aPDU);

  void HandleNtf(const BluetoothDaemonPDUHeader& aHeader,
                 BluetoothDaemonPDU& aPDU,
                 void* aUserData);

  static BluetoothHandsfreeNotificationHandler* sNotificationHandler;
};

class BluetoothDaemonHandsfreeInterface final
  : public BluetoothHandsfreeInterface
{
  class CleanupResultHandler;
  class InitResultHandler;

  enum {
    MODE_HEADSET = 0x00,
    MODE_NARROWBAND_SPEECH = 0x01,
    MODE_NARROWBAND_WIDEBAND_SPEECH = 0x02
  };

public:
  BluetoothDaemonHandsfreeInterface(BluetoothDaemonHandsfreeModule* aModule);
  ~BluetoothDaemonHandsfreeInterface();

  void Init(
    BluetoothHandsfreeNotificationHandler* aNotificationHandler,
    BluetoothHandsfreeResultHandler* aRes);
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

  void StartVoiceRecognition(BluetoothHandsfreeResultHandler* aRes);
  void StopVoiceRecognition(BluetoothHandsfreeResultHandler* aRes);

  /* Volume */

  void VolumeControl(BluetoothHandsfreeVolumeType aType, int aVolume,
                     BluetoothHandsfreeResultHandler* aRes);

  /* Device status */

  void DeviceStatusNotification(BluetoothHandsfreeNetworkState aNtkState,
                                BluetoothHandsfreeServiceType aSvcType,
                                int aSignal, int aBattChg,
                                BluetoothHandsfreeResultHandler* aRes);

  /* Responses */

  void CopsResponse(const char* aCops,
                    BluetoothHandsfreeResultHandler* aRes);
  void CindResponse(int aSvc, int aNumActive, int aNumHeld,
                    BluetoothHandsfreeCallState aCallSetupState,
                    int aSignal, int aRoam, int aBattChg,
                    BluetoothHandsfreeResultHandler* aRes);
  void FormattedAtResponse(const char* aRsp,
                           BluetoothHandsfreeResultHandler* aRes);
  void AtResponse(BluetoothHandsfreeAtResponse aResponseCode, int aErrorCode,
                  BluetoothHandsfreeResultHandler* aRes);
  void ClccResponse(int aIndex, BluetoothHandsfreeCallDirection aDir,
                    BluetoothHandsfreeCallState aState,
                    BluetoothHandsfreeCallMode aMode,
                    BluetoothHandsfreeCallMptyType aMpty,
                    const nsAString& aNumber,
                    BluetoothHandsfreeCallAddressType aType,
                    BluetoothHandsfreeResultHandler* aRes);

  /* Phone State */

  void PhoneStateChange(int aNumActive, int aNumHeld,
                        BluetoothHandsfreeCallState aCallSetupState,
                        const nsAString& aNumber,
                        BluetoothHandsfreeCallAddressType aType,
                        BluetoothHandsfreeResultHandler* aRes);

private:
  void DispatchError(BluetoothHandsfreeResultHandler* aRes,
                     BluetoothStatus aStatus);

  BluetoothDaemonHandsfreeModule* mModule;
};

END_BLUETOOTH_NAMESPACE

#endif
