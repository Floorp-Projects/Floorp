/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

  nsRefPtr<nsRunnable> runnable;

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

  typedef BluetoothNotificationHALRunnable2<
    HandsfreeNotificationHandlerWrapper, void,
    BluetoothHandsfreeVoiceRecognitionState, nsString,
    BluetoothHandsfreeVoiceRecognitionState, const nsAString&>
    VoiceRecognitionNotification;

  typedef BluetoothNotificationHALRunnable1<
    HandsfreeNotificationHandlerWrapper, void,
    nsString, const nsAString&>
    AnswerCallNotification;

  typedef BluetoothNotificationHALRunnable1<
    HandsfreeNotificationHandlerWrapper, void,
    nsString, const nsAString&>
    HangupCallNotification;

  typedef BluetoothNotificationHALRunnable3<
    HandsfreeNotificationHandlerWrapper, void,
    BluetoothHandsfreeVolumeType, int, nsString,
    BluetoothHandsfreeVolumeType, int, const nsAString&>
    VolumeNotification;

  typedef BluetoothNotificationHALRunnable2<
    HandsfreeNotificationHandlerWrapper, void,
    nsString, nsString, const nsAString&, const nsAString&>
    DialCallNotification;

  typedef BluetoothNotificationHALRunnable2<
    HandsfreeNotificationHandlerWrapper, void,
    char, nsString, char, const nsAString&>
    DtmfNotification;

  typedef BluetoothNotificationHALRunnable2<
    HandsfreeNotificationHandlerWrapper, void,
    BluetoothHandsfreeNRECState, nsString,
    BluetoothHandsfreeNRECState, const nsAString&>
    NRECNotification;

  typedef BluetoothNotificationHALRunnable2<
    HandsfreeNotificationHandlerWrapper, void,
    BluetoothHandsfreeWbsConfig, nsString,
    BluetoothHandsfreeWbsConfig, const nsAString&>
    WbsNotification;

  typedef BluetoothNotificationHALRunnable2<
    HandsfreeNotificationHandlerWrapper, void,
    BluetoothHandsfreeCallHoldType, nsString,
    BluetoothHandsfreeCallHoldType, const nsAString&>
    CallHoldNotification;

  typedef BluetoothNotificationHALRunnable1<
    HandsfreeNotificationHandlerWrapper, void,
    nsString, const nsAString&>
    CnumNotification;

  typedef BluetoothNotificationHALRunnable1<
    HandsfreeNotificationHandlerWrapper, void,
    nsString, const nsAString&>
    CindNotification;

  typedef BluetoothNotificationHALRunnable1<
    HandsfreeNotificationHandlerWrapper, void,
    nsString, const nsAString&>
    CopsNotification;

  typedef BluetoothNotificationHALRunnable1<
    HandsfreeNotificationHandlerWrapper, void,
    nsString, const nsAString&>
    ClccNotification;

  typedef BluetoothNotificationHALRunnable2<
    HandsfreeNotificationHandlerWrapper, void,
    nsCString, nsString, const nsACString&, const nsAString&>
    UnknownAtNotification;

  typedef BluetoothNotificationHALRunnable1<
    HandsfreeNotificationHandlerWrapper, void,
    nsString, const nsAString&>
    KeyPressedNotification;

  // Bluedroid Handsfree callbacks

  static void
  ConnectionState(bthf_connection_state_t aState, bt_bdaddr_t* aBdAddr)
  {
#if ANDROID_VERSION < 21
    if (aState == BTHF_CONNECTION_STATE_CONNECTED && aBdAddr) {
      memcpy(&sConnectedDeviceAddress, aBdAddr,
             sizeof(sConnectedDeviceAddress));
    } else if (aState == BTHF_CONNECTION_STATE_DISCONNECTED) {
      memset(&sConnectedDeviceAddress, 0,
             sizeof(sConnectedDeviceAddress));
    }
#endif

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

#if ANDROID_VERSION >= 21
  static void
  VoiceRecognition(bthf_vr_state_t aState, bt_bdaddr_t* aBdAddr)
  {
    VoiceRecognitionNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::VoiceRecognitionNotification,
      aState, aBdAddr);
  }
#else
  static void
  VoiceRecognition(bthf_vr_state_t aState)
  {
    VoiceRecognitionNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::VoiceRecognitionNotification,
      aState, &sConnectedDeviceAddress);
  }
#endif

#if ANDROID_VERSION >= 21
  static void
  AnswerCall(bt_bdaddr_t* aBdAddr)
  {
    AnswerCallNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::AnswerCallNotification,
      aBdAddr);
  }
#else
  static void
  AnswerCall()
  {
    AnswerCallNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::AnswerCallNotification,
      &sConnectedDeviceAddress);
  }
#endif

#if ANDROID_VERSION >= 21
  static void
  HangupCall(bt_bdaddr_t* aBdAddr)
  {
    HangupCallNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::HangupCallNotification,
      aBdAddr);
  }
#else
  static void
  HangupCall()
  {
    HangupCallNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::HangupCallNotification,
      &sConnectedDeviceAddress);
  }
#endif

#if ANDROID_VERSION >= 21
  static void
  Volume(bthf_volume_type_t aType, int aVolume, bt_bdaddr_t* aBdAddr)
  {
    VolumeNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::VolumeNotification,
      aType, aVolume, aBdAddr);
  }
#else
  static void
  Volume(bthf_volume_type_t aType, int aVolume)
  {
    VolumeNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::VolumeNotification,
      aType, aVolume, &sConnectedDeviceAddress);
  }
#endif

#if ANDROID_VERSION >= 21
  static void
  DialCall(char* aNumber, bt_bdaddr_t* aBdAddr)
  {
    DialCallNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::DialCallNotification,
      aNumber, aBdAddr);
  }
#else
  static void
  DialCall(char* aNumber)
  {
    DialCallNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::DialCallNotification,
      aNumber, &sConnectedDeviceAddress);
  }
#endif

#if ANDROID_VERSION >= 21
  static void
  Dtmf(char aDtmf, bt_bdaddr_t* aBdAddr)
  {
    DtmfNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::DtmfNotification,
      aDtmf, aBdAddr);
  }
#else
  static void
  Dtmf(char aDtmf)
  {
    DtmfNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::DtmfNotification,
      aDtmf, &sConnectedDeviceAddress);
  }
#endif

#if ANDROID_VERSION >= 21
  static void
  NoiseReductionEchoCancellation(bthf_nrec_t aNrec, bt_bdaddr_t* aBdAddr)
  {
    NRECNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::NRECNotification,
      aNrec, aBdAddr);
  }
#else
  static void
  NoiseReductionEchoCancellation(bthf_nrec_t aNrec)
  {
    NRECNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::NRECNotification,
      aNrec, &sConnectedDeviceAddress);
  }
#endif

#if ANDROID_VERSION >= 21
  static void
  WideBandSpeech(bthf_wbs_config_t aWbs, bt_bdaddr_t* aBdAddr)
  {
    WbsNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::WbsNotification,
      aWbs, aBdAddr);
  }
#endif

#if ANDROID_VERSION >= 21
  static void
  CallHold(bthf_chld_type_t aChld, bt_bdaddr_t* aBdAddr)
  {
    CallHoldNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::CallHoldNotification,
      aChld, aBdAddr);
  }
#else
  static void
  CallHold(bthf_chld_type_t aChld)
  {
    CallHoldNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::CallHoldNotification,
      aChld, &sConnectedDeviceAddress);
  }
#endif

#if ANDROID_VERSION >= 21
  static void
  Cnum(bt_bdaddr_t* aBdAddr)
  {
    CnumNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::CnumNotification,
      aBdAddr);
  }
#else
  static void
  Cnum()
  {
    CnumNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::CnumNotification,
      &sConnectedDeviceAddress);
  }
#endif

#if ANDROID_VERSION >= 21
  static void
  Cind(bt_bdaddr_t* aBdAddr)
  {
    CindNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::CindNotification,
      aBdAddr);
  }
#else
  static void
  Cind()
  {
    CindNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::CindNotification,
      &sConnectedDeviceAddress);
  }
#endif

#if ANDROID_VERSION >= 21
  static void
  Cops(bt_bdaddr_t* aBdAddr)
  {
    CopsNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::CopsNotification,
      aBdAddr);
  }
#else
  static void
  Cops()
  {
    CopsNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::CopsNotification,
      &sConnectedDeviceAddress);
  }
#endif

#if ANDROID_VERSION >= 21
  static void
  Clcc(bt_bdaddr_t* aBdAddr)
  {
    ClccNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::ClccNotification,
      aBdAddr);
  }
#else
  static void
  Clcc()
  {
    ClccNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::ClccNotification,
      &sConnectedDeviceAddress);
  }
#endif

#if ANDROID_VERSION >= 21
  static void
  UnknownAt(char* aAtString, bt_bdaddr_t* aBdAddr)
  {
    UnknownAtNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::UnknownAtNotification,
      aAtString, aBdAddr);
  }
#else
  static void
  UnknownAt(char* aAtString)
  {
    UnknownAtNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::UnknownAtNotification,
      aAtString, &sConnectedDeviceAddress);
  }
#endif

#if ANDROID_VERSION >= 21
  static void
  KeyPressed(bt_bdaddr_t* aBdAddr)
  {
    KeyPressedNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::KeyPressedNotification,
      aBdAddr);
  }
#else
  static void
  KeyPressed()
  {
    KeyPressedNotification::Dispatch(
      &BluetoothHandsfreeNotificationHandler::KeyPressedNotification,
      &sConnectedDeviceAddress);
  }
#endif

#if ANDROID_VERSION < 21
  /* |sConnectedDeviceAddress| stores Bluetooth device address of the
  * connected device. Before Android Lollipop, we maintain this address by
  * ourselves through ConnectionState(); after Android Lollipop, every callback
  * carries this address directly so we don't have to keep it.
  */
  static bt_bdaddr_t sConnectedDeviceAddress;
#endif
};

#if ANDROID_VERSION < 21
bt_bdaddr_t BluetoothHandsfreeHALCallback::sConnectedDeviceAddress = {
  {0, 0, 0, 0, 0, 0}
};
#endif

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
  int aMaxNumClients, BluetoothHandsfreeResultHandler* aRes)
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
#if ANDROID_VERSION >= 21
    BluetoothHandsfreeHALCallback::WideBandSpeech,
#endif
    BluetoothHandsfreeHALCallback::CallHold,
    BluetoothHandsfreeHALCallback::Cnum,
    BluetoothHandsfreeHALCallback::Cind,
    BluetoothHandsfreeHALCallback::Cops,
    BluetoothHandsfreeHALCallback::Clcc,
    BluetoothHandsfreeHALCallback::UnknownAt,
    BluetoothHandsfreeHALCallback::KeyPressed
  };

  sHandsfreeNotificationHandler = aNotificationHandler;

#if ANDROID_VERSION >= 21
  bt_status_t status = mInterface->init(&sCallbacks, aMaxNumClients);
#else
  bt_status_t status = mInterface->init(&sCallbacks);
#endif

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
  const nsAString& aBdAddr,
  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;

#if ANDROID_VERSION >= 21
  bt_bdaddr_t bdAddr;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->start_voice_recognition(&bdAddr);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }
#else
  status = mInterface->start_voice_recognition();
#endif

  if (aRes) {
    DispatchBluetoothHandsfreeHALResult(
      aRes, &BluetoothHandsfreeResultHandler::StartVoiceRecognition,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHandsfreeHALInterface::StopVoiceRecognition(
  const nsAString& aBdAddr,
  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;

#if ANDROID_VERSION >= 21
  bt_bdaddr_t bdAddr;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->stop_voice_recognition(&bdAddr);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }
#else
  status = mInterface->stop_voice_recognition();
#endif

  if (aRes) {
    DispatchBluetoothHandsfreeHALResult(
      aRes, &BluetoothHandsfreeResultHandler::StopVoiceRecognition,
      ConvertDefault(status, STATUS_FAIL));
  }
}

/* Volume */

void
BluetoothHandsfreeHALInterface::VolumeControl(
  BluetoothHandsfreeVolumeType aType, int aVolume, const nsAString& aBdAddr,
  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;
  bthf_volume_type_t type = BTHF_VOLUME_TYPE_SPK;

#if ANDROID_VERSION >= 21
  bt_bdaddr_t bdAddr;

  if (NS_SUCCEEDED(Convert(aType, type)) &&
      NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->volume_control(type, aVolume, &bdAddr);
#else
  if (NS_SUCCEEDED(Convert(aType, type))) {
    status = mInterface->volume_control(type, aVolume);
#endif
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
  const char* aCops, const nsAString& aBdAddr,
  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;

#if ANDROID_VERSION >= 21
  bt_bdaddr_t bdAddr;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->cops_response(aCops, &bdAddr);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }
#else
  status = mInterface->cops_response(aCops);
#endif

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
  const nsAString& aBdAddr,
  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;
  bthf_call_state_t callSetupState = BTHF_CALL_STATE_ACTIVE;

#if ANDROID_VERSION >= 21
  bt_bdaddr_t bdAddr;

  if (NS_SUCCEEDED(Convert(aCallSetupState, callSetupState)) &&
      NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->cind_response(aSvc, aNumActive, aNumHeld,
                                       callSetupState, aSignal,
                                       aRoam, aBattChg, &bdAddr);
#else
  if (NS_SUCCEEDED(Convert(aCallSetupState, callSetupState))) {
    status = mInterface->cind_response(aSvc, aNumActive, aNumHeld,
                                       callSetupState, aSignal,
                                       aRoam, aBattChg);
#endif
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
  const char* aRsp, const nsAString& aBdAddr,
  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;

#if ANDROID_VERSION >= 21
  bt_bdaddr_t bdAddr;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->formatted_at_response(aRsp, &bdAddr);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }
#else
  status = mInterface->formatted_at_response(aRsp);
#endif

  if (aRes) {
    DispatchBluetoothHandsfreeHALResult(
      aRes, &BluetoothHandsfreeResultHandler::FormattedAtResponse,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothHandsfreeHALInterface::AtResponse(
  BluetoothHandsfreeAtResponse aResponseCode, int aErrorCode,
  const nsAString& aBdAddr,
  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;
  bthf_at_response_t responseCode = BTHF_AT_RESPONSE_ERROR;

#if ANDROID_VERSION >= 21
  bt_bdaddr_t bdAddr;

  if (NS_SUCCEEDED(Convert(aResponseCode, responseCode)) &&
      NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->at_response(responseCode, aErrorCode, &bdAddr);
#else
  if (NS_SUCCEEDED(Convert(aResponseCode, responseCode))) {
    status = mInterface->at_response(responseCode, aErrorCode);
#endif
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
  const nsAString& aBdAddr,
  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;
  bthf_call_direction_t dir = BTHF_CALL_DIRECTION_OUTGOING;
  bthf_call_state_t state = BTHF_CALL_STATE_ACTIVE;
  bthf_call_mode_t mode = BTHF_CALL_TYPE_VOICE;
  bthf_call_mpty_type_t mpty = BTHF_CALL_MPTY_TYPE_SINGLE;
  bthf_call_addrtype_t type = BTHF_CALL_ADDRTYPE_UNKNOWN;

#if ANDROID_VERSION >= 21
  bt_bdaddr_t bdAddr;

  if (NS_SUCCEEDED(Convert(aDir, dir)) &&
      NS_SUCCEEDED(Convert(aState, state)) &&
      NS_SUCCEEDED(Convert(aMode, mode)) &&
      NS_SUCCEEDED(Convert(aMpty, mpty)) &&
      NS_SUCCEEDED(Convert(aType, type)) &&
      NS_SUCCEEDED(Convert(aBdAddr, bdAddr))) {
    status = mInterface->clcc_response(aIndex, dir, state, mode, mpty,
                                       NS_ConvertUTF16toUTF8(aNumber).get(),
                                       type, &bdAddr);
#else
  if (NS_SUCCEEDED(Convert(aDir, dir)) &&
      NS_SUCCEEDED(Convert(aState, state)) &&
      NS_SUCCEEDED(Convert(aMode, mode)) &&
      NS_SUCCEEDED(Convert(aMpty, mpty)) &&
      NS_SUCCEEDED(Convert(aType, type))) {
    status = mInterface->clcc_response(aIndex, dir, state, mode, mpty,
                                       NS_ConvertUTF16toUTF8(aNumber).get(),
                                       type);
#endif
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

/* Wide Band Speech */

void
BluetoothHandsfreeHALInterface::ConfigureWbs(
  const nsAString& aBdAddr,
  BluetoothHandsfreeWbsConfig aConfig,
  BluetoothHandsfreeResultHandler* aRes)
{
  bt_status_t status;

#if ANDROID_VERSION >= 21
  bt_bdaddr_t bdAddr;
  bthf_wbs_config_t wbsConfig;

  if (NS_SUCCEEDED(Convert(aBdAddr, bdAddr)) &&
      NS_SUCCEEDED(Convert(aConfig, wbsConfig))) {
    status = mInterface->configure_wbs(&bdAddr, wbsConfig);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }
#else
  status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothHandsfreeHALResult(
      aRes, &BluetoothHandsfreeResultHandler::ConfigureWbs,
      ConvertDefault(status, STATUS_FAIL));
  }
}

END_BLUETOOTH_NAMESPACE
