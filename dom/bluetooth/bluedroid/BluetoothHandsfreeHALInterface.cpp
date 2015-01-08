/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothHandsfreeHALInterface.h"
#include "BluetoothHALHelpers.h"

BEGIN_BLUETOOTH_NAMESPACE

typedef
  BluetoothHALInterfaceRunnable0<BluetoothHandsfreeResultHandler, void>
  BluetoothHandsfreeHALResultRunnable;

typedef
  BluetoothHALInterfaceRunnable1<BluetoothHandsfreeResultHandler, void,
                                 BluetoothStatus, BluetoothStatus>
  BluetoothHandsfreeHALErrorRunnable;

static nsresult
DispatchBluetoothHandsfreeHALResult(
  BluetoothHandsfreeResultHandler* aRes,
  void (BluetoothHandsfreeResultHandler::*aMethod)(),
  BluetoothStatus aStatus)
{
  MOZ_ASSERT(aRes);

  nsRunnable* runnable;

  if (aStatus == STATUS_SUCCESS) {
    runnable = new BluetoothHandsfreeHALResultRunnable(aRes, aMethod);
  } else {
    runnable = new BluetoothHandsfreeHALErrorRunnable(aRes,
      &BluetoothHandsfreeResultHandler::OnError, aStatus);
  }
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
  }
  return rv;
}

// Notification handling
//

static BluetoothHandsfreeNotificationHandler* sHandsfreeNotificationHandler;

struct BluetoothHandsfreeHALCallback
{
  class HandsfreeNotificationHandlerWrapper
  {
  public:
    typedef BluetoothHandsfreeNotificationHandler ObjectType;

    static ObjectType* GetInstance()
    {
      MOZ_ASSERT(NS_IsMainThread());

      return sHandsfreeNotificationHandler;
    }
  };

  // Notifications

  typedef BluetoothNotificationHALRunnable2<
    HandsfreeNotificationHandlerWrapper, void,
    BluetoothHandsfreeConnectionState, nsString,
    BluetoothHandsfreeConnectionState, const nsAString&>
    ConnectionStateNotification;

  typedef BluetoothNotificationHALRunnable2<
    HandsfreeNotificationHandlerWrapper, void,
    BluetoothHandsfreeAudioState, nsString,
    BluetoothHandsfreeAudioState, const nsAString&>
    AudioStateNotification;

  typedef BluetoothNotificationHALRunnable1<
    HandsfreeNotificationHandlerWrapper, void,
    BluetoothHandsfreeVoiceRecognitionState>
    VoiceRecognitionNotification;

  typedef BluetoothNotificationHALRunnable0<
    HandsfreeNotificationHandlerWrapper, void>
    AnswerCallNotification;

  typedef BluetoothNotificationHALRunnable0<
    HandsfreeNotificationHandlerWrapper, void>
    HangupCallNotification;

  typedef BluetoothNotificationHALRunnable2<
    HandsfreeNotificationHandlerWrapper, void,
    BluetoothHandsfreeVolumeType, int>
    VolumeNotification;

  typedef BluetoothNotificationHALRunnable1<
    HandsfreeNotificationHandlerWrapper, void, nsString, const nsAString&>
    DialCallNotification;

  typedef BluetoothNotificationHALRunnable1<
    HandsfreeNotificationHandlerWrapper, void, char>
    DtmfNotification;

  typedef BluetoothNotificationHALRunnable1<
    HandsfreeNotificationHandlerWrapper, void, BluetoothHandsfreeNRECState>
    NRECNotification;

  typedef BluetoothNotificationHALRunnable1<
    HandsfreeNotificationHandlerWrapper, void, BluetoothHandsfreeCallHoldType>
    CallHoldNotification;

  typedef BluetoothNotificationHALRunnable0<
    HandsfreeNotificationHandlerWrapper, void>
    CnumNotification;

  typedef BluetoothNotificationHALRunnable0<
    HandsfreeNotificationHandlerWrapper, void>
    CindNotification;

  typedef BluetoothNotificationHALRunnable0<
    HandsfreeNotificationHandlerWrapper, void>
    CopsNotification;

  typedef BluetoothNotificationHALRunnable0<
    HandsfreeNotificationHandlerWrapper, void>
    ClccNotification;

  typedef BluetoothNotificationHALRunnable1<
    HandsfreeNotificationHandlerWrapper, void, nsCString, const nsACString&>
    UnknownAtNotification;

  typedef BluetoothNotificationHALRunnable0<
    HandsfreeNotificationHandlerWrapper, void>
    KeyPressedNotification;

  // Bluedroid Handsfree callbacks

  static void
  ConnectionState(bthf_connection_state_t aState, bt_bdaddr_t* aBdAddr)
  {
    ConnectionStateNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::ConnectionStateNotification,
      aState, aBdAddr);
  }

  static void
  AudioState(bthf_audio_state_t aState, bt_bdaddr_t* aBdAddr)
  {
    AudioStateNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::AudioStateNotification,
      aState, aBdAddr);
  }

  static void
  VoiceRecognition(bthf_vr_state_t aState)
  {
    VoiceRecognitionNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::VoiceRecognitionNotification,
      aState);
  }

  static void
  AnswerCall()
  {
    AnswerCallNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::AnswerCallNotification);
  }

  static void
  HangupCall()
  {
    HangupCallNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::HangupCallNotification);
  }

  static void
  Volume(bthf_volume_type_t aType, int aVolume)
  {
    VolumeNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::VolumeNotification,
      aType, aVolume);
  }

  static void
  DialCall(char* aNumber)
  {
    DialCallNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::DialCallNotification, aNumber);
  }

  static void
  Dtmf(char aDtmf)
  {
    DtmfNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::DtmfNotification, aDtmf);
  }

  static void
  NoiseReductionEchoCancellation(bthf_nrec_t aNrec)
  {
    NRECNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::NRECNotification, aNrec);
  }

  static void
  CallHold(bthf_chld_type_t aChld)
  {
    CallHoldNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::CallHoldNotification, aChld);
  }

  static void
  Cnum()
  {
    CnumNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::CnumNotification);
  }

  static void
  Cind()
  {
    CindNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::CindNotification);
  }

  static void
  Cops()
  {
    CopsNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::CopsNotification);
  }

  static void
  Clcc()
  {
    ClccNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::ClccNotification);
  }

  static void
  UnknownAt(char* aAtString)
  {
    UnknownAtNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::UnknownAtNotification,
      aAtString);
  }

  static void
  KeyPressed()
  {
    KeyPressedNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::KeyPressedNotification);
  }
};

// Interface
//

BluetoothHandsfreeHALInterface::BluetoothHandsfreeHALInterface(
  const bthf_interface_t* aInterface)
: mInterface(aInterface)
{
  MOZ_ASSERT(mInterface);
}

BluetoothHandsfreeHALInterface::~BluetoothHandsfreeHALInterface()
{ }

void
BluetoothHandsfreeHALInterface::Init(
  BluetoothHandsfreeNotificationHandler* aNotificationHandler,
  BluetoothHandsfreeResultHandler* aRes)
{
  static bthf_callbacks_t sCallbacks = {
    sizeof(sCallbacks),
    BluetoothHandsfreeHALCallback::ConnectionState,
    BluetoothHandsfreeHALCallback::AudioState,
    BluetoothHandsfreeHALCallback::VoiceRecognition,
    BluetoothHandsfreeHALCallback::AnswerCall,
    BluetoothHandsfreeHALCallback::HangupCall,
    BluetoothHandsfreeHALCallback::Volume,
    BluetoothHandsfreeHALCallback::DialCall,
    BluetoothHandsfreeHALCallback::Dtmf,
    BluetoothHandsfreeHALCallback::NoiseReductionEchoCancellation,
    BluetoothHandsfreeHALCallback::CallHold,
    BluetoothHandsfreeHALCallback::Cnum,
    BluetoothHandsfreeHALCallback::Cind,
    BluetoothHandsfreeHALCallback::Cops,
    BluetoothHandsfreeHALCallback::Clcc,
    BluetoothHandsfreeHALCallback::UnknownAt,
    BluetoothHandsfreeHALCallback::KeyPressed
  };

  sHandsfreeNotificationHandler = aNotificationHandler;

  bt_status_t status = mInterface->init(&sCallbacks);

  if (aRes) {
    DispatchBluetoothHandsfreeHALResult(
      aRes, &BluetoothHandsfreeResultHandler::Init,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHandsfreeHALInterface::Cleanup(
  BluetoothHandsfreeResultHandler* aRes)
{
  mInterface->cleanup();

  if (aRes) {
    DispatchBluetoothHandsfreeHALResult(
      aRes, &BluetoothHandsfreeResultHandler::Cleanup, STATUS_SUCCESS);
  }
}

/* Connect / Disconnect */

void
BluetoothHandsfreeHALInterface::Connect(
  const nsAString& aBdAddr, BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;
  bt_bdaddr_t bdAddr;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->connect(&bdAddr);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHandsfreeHALResult(
      aRes, &BluetoothHandsfreeResultHandler::Connect,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHandsfreeHALInterface::Disconnect(
  const nsAString& aBdAddr, BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;
  bt_bdaddr_t bdAddr;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->disconnect(&bdAddr);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHandsfreeHALResult(
      aRes, &BluetoothHandsfreeResultHandler::Disconnect,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHandsfreeHALInterface::ConnectAudio(
  const nsAString& aBdAddr, BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;
  bt_bdaddr_t bdAddr;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->connect_audio(&bdAddr);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHandsfreeHALResult(
      aRes, &BluetoothHandsfreeResultHandler::ConnectAudio,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHandsfreeHALInterface::DisconnectAudio(
  const nsAString& aBdAddr, BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;
  bt_bdaddr_t bdAddr;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->disconnect_audio(&bdAddr);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHandsfreeHALResult(
      aRes, &BluetoothHandsfreeResultHandler::DisconnectAudio,
      ConvertDefault(status, STATUS_FAIL));
  }
}

/* Voice Recognition */

void
BluetoothHandsfreeHALInterface::StartVoiceRecognition(
  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status = mInterface->start_voice_recognition();

  if (aRes) {
    DispatchBluetoothHandsfreeHALResult(
      aRes, &BluetoothHandsfreeResultHandler::StartVoiceRecognition,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHandsfreeHALInterface::StopVoiceRecognition(
  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status = mInterface->stop_voice_recognition();

  if (aRes) {
    DispatchBluetoothHandsfreeHALResult(
      aRes, &BluetoothHandsfreeResultHandler::StopVoiceRecognition,
      ConvertDefault(status, STATUS_FAIL));
  }
}

/* Volume */

void
BluetoothHandsfreeHALInterface::VolumeControl(
  BluetoothHandsfreeVolumeType aType, int aVolume,
  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;
  bthf_volume_type_t type = BTHF_VOLUME_TYPE_SPK;

  if (NS_SUCCEEDED(Convert(aType, type))) {
    status = mInterface->volume_control(type, aVolume);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHandsfreeHALResult(
      aRes, &BluetoothHandsfreeResultHandler::VolumeControl,
      ConvertDefault(status, STATUS_FAIL));
  }
}

/* Device status */

void
BluetoothHandsfreeHALInterface::DeviceStatusNotification(
  BluetoothHandsfreeNetworkState aNtkState,
  BluetoothHandsfreeServiceType aSvcType, int aSignal,
  int aBattChg, BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;
  bthf_network_state_t ntkState = BTHF_NETWORK_STATE_NOT_AVAILABLE;
  bthf_service_type_t svcType = BTHF_SERVICE_TYPE_HOME;

  if (NS_SUCCEEDED(Convert(aNtkState, ntkState)) &&
      NS_SUCCEEDED(Convert(aSvcType, svcType))) {
    status = mInterface->device_status_notification(ntkState, svcType,
                                                    aSignal, aBattChg);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHandsfreeHALResult(
      aRes, &BluetoothHandsfreeResultHandler::DeviceStatusNotification,
      ConvertDefault(status, STATUS_FAIL));
  }
}

/* Responses */

void
BluetoothHandsfreeHALInterface::CopsResponse(
  const char* aCops, BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status = mInterface->cops_response(aCops);

  if (aRes) {
    DispatchBluetoothHandsfreeHALResult(
      aRes, &BluetoothHandsfreeResultHandler::CopsResponse,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHandsfreeHALInterface::CindResponse(
  int aSvc, int aNumActive, int aNumHeld,
  BluetoothHandsfreeCallState aCallSetupState,
  int aSignal, int aRoam, int aBattChg,
  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;
  bthf_call_state_t callSetupState = BTHF_CALL_STATE_ACTIVE;

  if (NS_SUCCEEDED(Convert(aCallSetupState, callSetupState))) {
    status = mInterface->cind_response(aSvc, aNumActive, aNumHeld,
                                       callSetupState, aSignal,
                                       aRoam, aBattChg);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHandsfreeHALResult(
      aRes, &BluetoothHandsfreeResultHandler::CindResponse,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHandsfreeHALInterface::FormattedAtResponse(
  const char* aRsp, BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status = mInterface->formatted_at_response(aRsp);

  if (aRes) {
    DispatchBluetoothHandsfreeHALResult(
      aRes, &BluetoothHandsfreeResultHandler::FormattedAtResponse,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHandsfreeHALInterface::AtResponse(
  BluetoothHandsfreeAtResponse aResponseCode, int aErrorCode,
  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;
  bthf_at_response_t responseCode = BTHF_AT_RESPONSE_ERROR;

  if (NS_SUCCEEDED(Convert(aResponseCode, responseCode))) {
    status = mInterface->at_response(responseCode, aErrorCode);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHandsfreeHALResult(
      aRes, &BluetoothHandsfreeResultHandler::AtResponse,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHandsfreeHALInterface::ClccResponse(
  int aIndex,
  BluetoothHandsfreeCallDirection aDir,
  BluetoothHandsfreeCallState aState,
  BluetoothHandsfreeCallMode aMode,
  BluetoothHandsfreeCallMptyType aMpty,
  const nsAString& aNumber,
  BluetoothHandsfreeCallAddressType aType,
  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;
  bthf_call_direction_t dir = BTHF_CALL_DIRECTION_OUTGOING;
  bthf_call_state_t state = BTHF_CALL_STATE_ACTIVE;
  bthf_call_mode_t mode = BTHF_CALL_TYPE_VOICE;
  bthf_call_mpty_type_t mpty = BTHF_CALL_MPTY_TYPE_SINGLE;
  bthf_call_addrtype_t type = BTHF_CALL_ADDRTYPE_UNKNOWN;

  if (NS_SUCCEEDED(Convert(aDir, dir)) &&
      NS_SUCCEEDED(Convert(aState, state)) &&
      NS_SUCCEEDED(Convert(aMode, mode)) &&
      NS_SUCCEEDED(Convert(aMpty, mpty)) &&
      NS_SUCCEEDED(Convert(aType, type))) {
    status = mInterface->clcc_response(aIndex, dir, state, mode, mpty,
                                       NS_ConvertUTF16toUTF8(aNumber).get(),
                                       type);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHandsfreeHALResult(
      aRes, &BluetoothHandsfreeResultHandler::ClccResponse,
      ConvertDefault(status, STATUS_FAIL));
  }
}

/* Phone State */

void
BluetoothHandsfreeHALInterface::PhoneStateChange(int aNumActive, int aNumHeld,
  BluetoothHandsfreeCallState aCallSetupState, const nsAString& aNumber,
  BluetoothHandsfreeCallAddressType aType,
  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;
  bthf_call_state_t callSetupState = BTHF_CALL_STATE_ACTIVE;
  bthf_call_addrtype_t type = BTHF_CALL_ADDRTYPE_UNKNOWN;

  if (NS_SUCCEEDED(Convert(aCallSetupState, callSetupState)) &&
      NS_SUCCEEDED(Convert(aType, type))) {
    status = mInterface->phone_state_change(
      aNumActive, aNumHeld, callSetupState,
      NS_ConvertUTF16toUTF8(aNumber).get(), type);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }

  if (aRes) {
    DispatchBluetoothHandsfreeHALResult(
      aRes, &BluetoothHandsfreeResultHandler::PhoneStateChange,
      ConvertDefault(status, STATUS_FAIL));
  }
}

END_BLUETOOTH_NAMESPACE
