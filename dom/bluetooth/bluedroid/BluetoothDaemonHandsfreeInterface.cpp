/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothDaemonHandsfreeInterface.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"

BEGIN_BLUETOOTH_NAMESPACE

using namespace mozilla::ipc;

//
// Handsfree module
//

BluetoothHandsfreeNotificationHandler*
  BluetoothDaemonHandsfreeModule::sNotificationHandler;

#if ANDROID_VERSION < 21
BluetoothAddress BluetoothDaemonHandsfreeModule::sConnectedDeviceAddress(
  BluetoothAddress::ANY());
#endif

void
BluetoothDaemonHandsfreeModule::SetNotificationHandler(
  BluetoothHandsfreeNotificationHandler* aNotificationHandler)
{
  sNotificationHandler = aNotificationHandler;
}

void
BluetoothDaemonHandsfreeModule::HandleSvc(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  DaemonSocketResultHandler* aRes)
{
  static void (BluetoothDaemonHandsfreeModule::* const HandleOp[])(
    const DaemonSocketPDUHeader&, DaemonSocketPDU&,
    DaemonSocketResultHandler*) = {
    [0] = &BluetoothDaemonHandsfreeModule::HandleRsp,
    [1] = &BluetoothDaemonHandsfreeModule::HandleNtf
  };

  MOZ_ASSERT(!NS_IsMainThread());

  // Negate twice to map bit to 0/1
  unsigned long isNtf = !!(aHeader.mOpcode & 0x80);

  (this->*(HandleOp[isNtf]))(aHeader, aPDU, aRes);
}

// Commands
//

nsresult
BluetoothDaemonHandsfreeModule::ConnectCmd(
  const BluetoothAddress& aRemoteAddr, BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  UniquePtr<DaemonSocketPDU> pdu =
    MakeUnique<DaemonSocketPDU>(SERVICE_ID, OPCODE_CONNECT,
                                6); // Address

  nsresult rv = PackPDU(aRemoteAddr, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu.get(), aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  Unused << pdu.release();
  return NS_OK;
}

nsresult
BluetoothDaemonHandsfreeModule::DisconnectCmd(
  const BluetoothAddress& aRemoteAddr, BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  UniquePtr<DaemonSocketPDU> pdu =
    MakeUnique<DaemonSocketPDU>(SERVICE_ID, OPCODE_DISCONNECT,
                                6); // Address

  nsresult rv = PackPDU(aRemoteAddr, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu.get(), aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  Unused << pdu.release();
  return NS_OK;
}

nsresult
BluetoothDaemonHandsfreeModule::ConnectAudioCmd(
  const BluetoothAddress& aRemoteAddr, BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  UniquePtr<DaemonSocketPDU> pdu(
    MakeUnique<DaemonSocketPDU>(SERVICE_ID, OPCODE_CONNECT_AUDIO,
                                6)); // Address

  nsresult rv = PackPDU(aRemoteAddr, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu.get(), aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  Unused << pdu.release();
  return NS_OK;
}

nsresult
BluetoothDaemonHandsfreeModule::DisconnectAudioCmd(
  const BluetoothAddress& aRemoteAddr, BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  UniquePtr<DaemonSocketPDU> pdu =
    MakeUnique<DaemonSocketPDU>(SERVICE_ID, OPCODE_DISCONNECT_AUDIO,
                                6); // Address

  nsresult rv = PackPDU(aRemoteAddr, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu.get(), aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  Unused << pdu.release();
  return NS_OK;
}

nsresult
BluetoothDaemonHandsfreeModule::StartVoiceRecognitionCmd(
  const BluetoothAddress& aRemoteAddr, BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  UniquePtr<DaemonSocketPDU> pdu =
    MakeUnique<DaemonSocketPDU>(SERVICE_ID, OPCODE_START_VOICE_RECOGNITION,
                                6); // Address (BlueZ 5.25)

  nsresult rv;
#if ANDROID_VERSION >= 21
  rv = PackPDU(aRemoteAddr, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
#endif
  rv = Send(pdu.get(), aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  Unused << pdu.release();
  return NS_OK;
}

nsresult
BluetoothDaemonHandsfreeModule::StopVoiceRecognitionCmd(
  const BluetoothAddress& aRemoteAddr, BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  UniquePtr<DaemonSocketPDU> pdu =
    MakeUnique<DaemonSocketPDU>(SERVICE_ID, OPCODE_STOP_VOICE_RECOGNITION,
                                6); // Address (BlueZ 5.25)

  nsresult rv;
#if ANDROID_VERSION >= 21
  rv = PackPDU(aRemoteAddr, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
#endif
  rv = Send(pdu.get(), aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  Unused << pdu.release();
  return NS_OK;
}

nsresult
BluetoothDaemonHandsfreeModule::VolumeControlCmd(
  BluetoothHandsfreeVolumeType aType, int aVolume,
  const BluetoothAddress& aRemoteAddr, BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  UniquePtr<DaemonSocketPDU> pdu =
    MakeUnique<DaemonSocketPDU>(SERVICE_ID, OPCODE_VOLUME_CONTROL,
                                1 + // Volume type
                                1 + // Volume
                                6); // Address (BlueZ 5.25)

#if ANDROID_VERSION >= 21
  nsresult rv = PackPDU(
    aType, PackConversion<int, uint8_t>(aVolume), aRemoteAddr, *pdu);
#else
  nsresult rv = PackPDU(aType, PackConversion<int, uint8_t>(aVolume), *pdu);
#endif
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu.get(), aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  Unused << pdu.release();
  return NS_OK;
}

nsresult
BluetoothDaemonHandsfreeModule::DeviceStatusNotificationCmd(
  BluetoothHandsfreeNetworkState aNtkState,
  BluetoothHandsfreeServiceType aSvcType, int aSignal, int aBattChg,
  BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  UniquePtr<DaemonSocketPDU> pdu =
    MakeUnique<DaemonSocketPDU>(SERVICE_ID, OPCODE_DEVICE_STATUS_NOTIFICATION,
                                1 + // Network state
                                1 + // Service type
                                1 + // Signal strength
                                1); // Battery level

  nsresult rv = PackPDU(aNtkState, aSvcType,
                        PackConversion<int, uint8_t>(aSignal),
                        PackConversion<int, uint8_t>(aBattChg), *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu.get(), aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  Unused << pdu.release();
  return NS_OK;
}

nsresult
BluetoothDaemonHandsfreeModule::CopsResponseCmd(
  const char* aCops, const BluetoothAddress& aRemoteAddr,
  BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  UniquePtr<DaemonSocketPDU> pdu =
    MakeUnique<DaemonSocketPDU>(SERVICE_ID, OPCODE_COPS_RESPONSE,
                                0 + // Dynamically allocated
                                6); // Address (BlueZ 5.25)

#if ANDROID_VERSION >= 21
  nsresult rv = PackPDU(PackCString0(nsDependentCString(aCops)), aRemoteAddr,
                        *pdu);
#else
  nsresult rv = PackPDU(PackCString0(nsDependentCString(aCops)), *pdu);
#endif
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu.get(), aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  Unused << pdu.release();
  return NS_OK;
}

nsresult
BluetoothDaemonHandsfreeModule::CindResponseCmd(
  int aSvc, int aNumActive, int aNumHeld,
  BluetoothHandsfreeCallState aCallSetupState,
  int aSignal, int aRoam, int aBattChg,
  const BluetoothAddress& aRemoteAddr,
  BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  UniquePtr<DaemonSocketPDU> pdu =
    MakeUnique<DaemonSocketPDU>(SERVICE_ID, OPCODE_CIND_RESPONSE,
                                1 + // Service
                                1 + // # Active
                                1 + // # Held
                                1 + // Call state
                                1 + // Signal strength
                                1 + // Roaming
                                1 + // Battery level
                                6); // Address (BlueZ 5.25)

#if ANDROID_VERSION >= 21
  nsresult rv = PackPDU(
    PackConversion<int, uint8_t>(aSvc),
    PackConversion<int, uint8_t>(aNumActive),
    PackConversion<int, uint8_t>(aNumHeld),
    aCallSetupState,
    PackConversion<int, uint8_t>(aSignal),
    PackConversion<int, uint8_t>(aRoam),
    PackConversion<int, uint8_t>(aBattChg),
    aRemoteAddr, *pdu);
#else
  nsresult rv = PackPDU(PackConversion<int, uint8_t>(aSvc),
                        PackConversion<int, uint8_t>(aNumActive),
                        PackConversion<int, uint8_t>(aNumHeld),
                        aCallSetupState,
                        PackConversion<int, uint8_t>(aSignal),
                        PackConversion<int, uint8_t>(aRoam),
                        PackConversion<int, uint8_t>(aBattChg), *pdu);
#endif
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu.get(), aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  Unused << pdu.release();
  return NS_OK;
}

nsresult
BluetoothDaemonHandsfreeModule::FormattedAtResponseCmd(
  const char* aRsp, const BluetoothAddress& aRemoteAddr,
  BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  UniquePtr<DaemonSocketPDU> pdu =
    MakeUnique<DaemonSocketPDU>(SERVICE_ID, OPCODE_FORMATTED_AT_RESPONSE,
                                0 + // Dynamically allocated
                                6); // Address (BlueZ 5.25)

#if ANDROID_VERSION >= 21
  nsresult rv = PackPDU(PackCString0(nsDependentCString(aRsp)), aRemoteAddr,
                                     *pdu);
#else
  nsresult rv = PackPDU(PackCString0(nsDependentCString(aRsp)), *pdu);
#endif
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu.get(), aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  Unused << pdu.release();
  return NS_OK;
}

nsresult
BluetoothDaemonHandsfreeModule::AtResponseCmd(
  BluetoothHandsfreeAtResponse aResponseCode, int aErrorCode,
  const BluetoothAddress& aRemoteAddr, BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  UniquePtr<DaemonSocketPDU> pdu =
    MakeUnique<DaemonSocketPDU>(SERVICE_ID, OPCODE_AT_RESPONSE,
                                1 + // AT Response code
                                1 + // Error code
                                6); // Address (BlueZ 5.25)

#if ANDROID_VERSION >= 21
  nsresult rv = PackPDU(aResponseCode,
                        PackConversion<int, uint8_t>(aErrorCode),
                        aRemoteAddr, *pdu);
#else
  nsresult rv = PackPDU(aResponseCode,
                        PackConversion<int, uint8_t>(aErrorCode), *pdu);
#endif
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu.get(), aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  Unused << pdu.release();
  return NS_OK;
}

nsresult
BluetoothDaemonHandsfreeModule::ClccResponseCmd(
  int aIndex,
  BluetoothHandsfreeCallDirection aDir, BluetoothHandsfreeCallState aState,
  BluetoothHandsfreeCallMode aMode, BluetoothHandsfreeCallMptyType aMpty,
  const nsAString& aNumber, BluetoothHandsfreeCallAddressType aType,
  const BluetoothAddress& aRemoteAddr, BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ConvertUTF16toUTF8 number(aNumber);

  UniquePtr<DaemonSocketPDU> pdu =
    MakeUnique<DaemonSocketPDU>(SERVICE_ID, OPCODE_CLCC_RESPONSE,
                                1 + // Call index
                                1 + // Call direction
                                1 + // Call state
                                1 + // Call mode
                                1 + // Call MPTY
                                1 + // Address type
                                number.Length() + 1 + // Number string + \0
                                6); // Address (BlueZ 5.25)

#if ANDROID_VERSION >= 21
  nsresult rv = PackPDU(PackConversion<int, uint8_t>(aIndex),
                        aDir, aState, aMode, aMpty, aType,
                        PackCString0(number), aRemoteAddr, *pdu);
#else
  nsresult rv = PackPDU(PackConversion<int, uint8_t>(aIndex),
                        aDir, aState, aMode, aMpty, aType,
                        PackCString0(number), *pdu);
#endif
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu.get(), aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  Unused << pdu.release();
  return NS_OK;
}

nsresult
BluetoothDaemonHandsfreeModule::PhoneStateChangeCmd(
  int aNumActive, int aNumHeld, BluetoothHandsfreeCallState aCallSetupState,
  const nsAString& aNumber, BluetoothHandsfreeCallAddressType aType,
  BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ConvertUTF16toUTF8 number(aNumber);

  UniquePtr<DaemonSocketPDU> pdu =
    MakeUnique<DaemonSocketPDU>(SERVICE_ID, OPCODE_PHONE_STATE_CHANGE,
                                1 + // # Active
                                1 + // # Held
                                1 + // Call state
                                1 + // Address type
                                number.Length() + 1); // Number string + \0

  nsresult rv = PackPDU(PackConversion<int, uint8_t>(aNumActive),
                        PackConversion<int, uint8_t>(aNumHeld),
                        aCallSetupState, aType,
                        PackCString0(NS_ConvertUTF16toUTF8(aNumber)), *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu.get(), aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  Unused << pdu.release();
  return NS_OK;
}

nsresult
BluetoothDaemonHandsfreeModule::ConfigureWbsCmd(
  const BluetoothAddress& aRemoteAddr,
  BluetoothHandsfreeWbsConfig aConfig,
  BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  UniquePtr<DaemonSocketPDU> pdu =
    MakeUnique<DaemonSocketPDU>(SERVICE_ID, OPCODE_CONFIGURE_WBS,
                                6 + // Address
                                1); // Config

  nsresult rv = PackPDU(aRemoteAddr, aConfig, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu.get(), aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  Unused << pdu.release();
  return NS_OK;
}

// Responses
//

void
BluetoothDaemonHandsfreeModule::ErrorRsp(
  const DaemonSocketPDUHeader& aHeader,
  DaemonSocketPDU& aPDU, BluetoothHandsfreeResultHandler* aRes)
{
  ErrorRunnable::Dispatch(
    aRes, &BluetoothHandsfreeResultHandler::OnError, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHandsfreeModule::ConnectRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothHandsfreeResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothHandsfreeResultHandler::Connect, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHandsfreeModule::DisconnectRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothHandsfreeResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothHandsfreeResultHandler::Disconnect, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHandsfreeModule::ConnectAudioRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothHandsfreeResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothHandsfreeResultHandler::ConnectAudio,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHandsfreeModule::DisconnectAudioRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothHandsfreeResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothHandsfreeResultHandler::DisconnectAudio,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHandsfreeModule::StartVoiceRecognitionRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothHandsfreeResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothHandsfreeResultHandler::StartVoiceRecognition,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHandsfreeModule::StopVoiceRecognitionRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothHandsfreeResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothHandsfreeResultHandler::StopVoiceRecognition,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHandsfreeModule::VolumeControlRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothHandsfreeResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothHandsfreeResultHandler::VolumeControl,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHandsfreeModule::DeviceStatusNotificationRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothHandsfreeResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothHandsfreeResultHandler::DeviceStatusNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHandsfreeModule::CopsResponseRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothHandsfreeResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothHandsfreeResultHandler::CopsResponse,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHandsfreeModule::CindResponseRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothHandsfreeResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothHandsfreeResultHandler::CindResponse,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHandsfreeModule::FormattedAtResponseRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothHandsfreeResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothHandsfreeResultHandler::FormattedAtResponse,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHandsfreeModule::AtResponseRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothHandsfreeResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothHandsfreeResultHandler::AtResponse,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHandsfreeModule::ClccResponseRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothHandsfreeResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothHandsfreeResultHandler::ClccResponse,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHandsfreeModule::PhoneStateChangeRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothHandsfreeResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothHandsfreeResultHandler::PhoneStateChange,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHandsfreeModule::ConfigureWbsRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothHandsfreeResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothHandsfreeResultHandler::ConfigureWbs,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHandsfreeModule::HandleRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  DaemonSocketResultHandler* aRes)
{
  static void (BluetoothDaemonHandsfreeModule::* const HandleRsp[])(
    const DaemonSocketPDUHeader&,
    DaemonSocketPDU&,
    BluetoothHandsfreeResultHandler*) = {
    [OPCODE_ERROR] =
      &BluetoothDaemonHandsfreeModule::ErrorRsp,
    [OPCODE_CONNECT] =
      &BluetoothDaemonHandsfreeModule::ConnectRsp,
    [OPCODE_DISCONNECT] =
      &BluetoothDaemonHandsfreeModule::DisconnectRsp,
    [OPCODE_CONNECT_AUDIO] =
      &BluetoothDaemonHandsfreeModule::ConnectAudioRsp,
    [OPCODE_DISCONNECT_AUDIO] =
      &BluetoothDaemonHandsfreeModule::DisconnectAudioRsp,
    [OPCODE_START_VOICE_RECOGNITION] =
      &BluetoothDaemonHandsfreeModule::StartVoiceRecognitionRsp,
    [OPCODE_STOP_VOICE_RECOGNITION] =
      &BluetoothDaemonHandsfreeModule::StopVoiceRecognitionRsp,
    [OPCODE_VOLUME_CONTROL] =
      &BluetoothDaemonHandsfreeModule::VolumeControlRsp,
    [OPCODE_DEVICE_STATUS_NOTIFICATION] =
      &BluetoothDaemonHandsfreeModule::DeviceStatusNotificationRsp,
    [OPCODE_COPS_RESPONSE] =
      &BluetoothDaemonHandsfreeModule::CopsResponseRsp,
    [OPCODE_CIND_RESPONSE] =
      &BluetoothDaemonHandsfreeModule::CindResponseRsp,
    [OPCODE_FORMATTED_AT_RESPONSE] =
      &BluetoothDaemonHandsfreeModule::FormattedAtResponseRsp,
    [OPCODE_AT_RESPONSE] =
      &BluetoothDaemonHandsfreeModule::AtResponseRsp,
    [OPCODE_CLCC_RESPONSE] =
      &BluetoothDaemonHandsfreeModule::ClccResponseRsp,
    [OPCODE_PHONE_STATE_CHANGE] =
      &BluetoothDaemonHandsfreeModule::PhoneStateChangeRsp,
    [OPCODE_CONFIGURE_WBS] =
      &BluetoothDaemonHandsfreeModule::ConfigureWbsRsp
  };

  MOZ_ASSERT(!NS_IsMainThread()); // I/O thread

  if (NS_WARN_IF(!(aHeader.mOpcode < MOZ_ARRAY_LENGTH(HandleRsp))) ||
      NS_WARN_IF(!HandleRsp[aHeader.mOpcode])) {
    return;
  }

  RefPtr<BluetoothHandsfreeResultHandler> res =
    static_cast<BluetoothHandsfreeResultHandler*>(aRes);

  if (!res) {
    return; // Return early if no result handler has been set for response
  }

  (this->*(HandleRsp[aHeader.mOpcode]))(aHeader, aPDU, res);
}

// Notifications
//

// Returns the current notification handler to a notification runnable
class BluetoothDaemonHandsfreeModule::NotificationHandlerWrapper final
{
public:
  typedef BluetoothHandsfreeNotificationHandler ObjectType;

  static ObjectType* GetInstance()
  {
    MOZ_ASSERT(NS_IsMainThread());

    return sNotificationHandler;
  }
};

// Init operator class for ConnectionStateNotification
class BluetoothDaemonHandsfreeModule::ConnectionStateInitOp final
  : private PDUInitOp
{
public:
  ConnectionStateInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (BluetoothHandsfreeConnectionState& aArg1,
               BluetoothAddress& aArg2) const
  {
    DaemonSocketPDU& pdu = GetPDU();

    /* Read state */
    nsresult rv = UnpackPDU(pdu, aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }

    /* Read address */
    rv = UnpackPDU(pdu, aArg2);
    if (NS_FAILED(rv)) {
      return rv;
    }

#if ANDROID_VERSION < 21
    if (aArg1 == HFP_CONNECTION_STATE_CONNECTED) {
      sConnectedDeviceAddress = aArg2;
    } else if (aArg1 == HFP_CONNECTION_STATE_DISCONNECTED) {
      sConnectedDeviceAddress = BluetoothAddress::ANY();
    }
#endif
    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonHandsfreeModule::ConnectionStateNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ConnectionStateNotification::Dispatch(
    &BluetoothHandsfreeNotificationHandler::ConnectionStateNotification,
    ConnectionStateInitOp(aPDU));
}

void
BluetoothDaemonHandsfreeModule::AudioStateNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  AudioStateNotification::Dispatch(
    &BluetoothHandsfreeNotificationHandler::AudioStateNotification,
    UnpackPDUInitOp(aPDU));
}

// Init operator class for VoiceRecognitionNotification
class BluetoothDaemonHandsfreeModule::VoiceRecognitionInitOp final
  : private PDUInitOp
{
public:
  VoiceRecognitionInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (BluetoothHandsfreeVoiceRecognitionState& aArg1,
               BluetoothAddress& aArg2) const
  {
    DaemonSocketPDU& pdu = GetPDU();

    /* Read state */
    nsresult rv = UnpackPDU(pdu, aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }

    /* Read address */
#if ANDROID_VERSION >= 21
    rv = UnpackPDU(pdu, aArg2);
    if (NS_FAILED(rv)) {
      return rv;
    }
#else
    aArg2 = sConnectedDeviceAddress;
#endif
    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonHandsfreeModule::VoiceRecognitionNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  VoiceRecognitionNotification::Dispatch(
    &BluetoothHandsfreeNotificationHandler::VoiceRecognitionNotification,
    VoiceRecognitionInitOp(aPDU));
}

// Init operator class for AnswerCallNotification
class BluetoothDaemonHandsfreeModule::AnswerCallInitOp final
  : private PDUInitOp
{
public:
  AnswerCallInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (BluetoothAddress& aArg1) const
  {
    /* Read address */
#if ANDROID_VERSION >= 21
    nsresult rv = UnpackPDU(GetPDU(), aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }
#else
    aArg1 = sConnectedDeviceAddress;
#endif
    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonHandsfreeModule::AnswerCallNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  AnswerCallNotification::Dispatch(
    &BluetoothHandsfreeNotificationHandler::AnswerCallNotification,
    AnswerCallInitOp(aPDU));
}

// Init operator class for HangupCallNotification
class BluetoothDaemonHandsfreeModule::HangupCallInitOp final
  : private PDUInitOp
{
public:
  HangupCallInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (BluetoothAddress& aArg1) const
  {
    /* Read address */
#if ANDROID_VERSION >= 21
    nsresult rv = UnpackPDU(GetPDU(), aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }
#else
    aArg1 = sConnectedDeviceAddress;
#endif
    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonHandsfreeModule::HangupCallNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  HangupCallNotification::Dispatch(
    &BluetoothHandsfreeNotificationHandler::HangupCallNotification,
    HangupCallInitOp(aPDU));
}

// Init operator class for VolumeNotification
class BluetoothDaemonHandsfreeModule::VolumeInitOp final
  : private PDUInitOp
{
public:
  VolumeInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (BluetoothHandsfreeVolumeType& aArg1, int& aArg2,
               BluetoothAddress& aArg3) const
  {
    DaemonSocketPDU& pdu = GetPDU();

    /* Read volume type */
    nsresult rv = UnpackPDU(pdu, aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }

    /* Read volume */
    rv = UnpackPDU(pdu, UnpackConversion<uint8_t, int>(aArg2));
    if (NS_FAILED(rv)) {
      return rv;
    }

    /* Read address */
#if ANDROID_VERSION >= 21
    rv = UnpackPDU(pdu, aArg3);
    if (NS_FAILED(rv)) {
      return rv;
    }
#else
    aArg3 = sConnectedDeviceAddress;
#endif
    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonHandsfreeModule::VolumeNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  VolumeNotification::Dispatch(
    &BluetoothHandsfreeNotificationHandler::VolumeNotification,
    VolumeInitOp(aPDU));
}

// Init operator class for DialCallNotification
class BluetoothDaemonHandsfreeModule::DialCallInitOp final
  : private PDUInitOp
{
public:
  DialCallInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (nsString& aArg1, BluetoothAddress& aArg2) const
  {
    DaemonSocketPDU& pdu = GetPDU();

    nsresult rv;
    /* Read address
     * It's a little weird to parse aArg2 (aBdAddr) before parsing
     * aArg1 (aNumber), but this order is defined in BlueZ 5.25 anyway.
     */
#if ANDROID_VERSION >= 21
    rv = UnpackPDU( pdu, aArg2);
    if (NS_FAILED(rv)) {
      return rv;
    }
#else
    aArg2 = sConnectedDeviceAddress;
#endif

    /* Read number */
    rv = UnpackPDU(pdu, UnpackString0(aArg1));
    if (NS_FAILED(rv)) {
      return rv;
    }
    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonHandsfreeModule::DialCallNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  DialCallNotification::Dispatch(
    &BluetoothHandsfreeNotificationHandler::DialCallNotification,
    DialCallInitOp(aPDU));
}

// Init operator class for DtmfNotification
class BluetoothDaemonHandsfreeModule::DtmfInitOp final
  : private PDUInitOp
{
public:
  DtmfInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (char& aArg1, BluetoothAddress& aArg2) const
  {
    DaemonSocketPDU& pdu = GetPDU();

    /* Read tone */
    nsresult rv = UnpackPDU(pdu, aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }

    /* Read address */
#if ANDROID_VERSION >= 21
    rv = UnpackPDU(pdu, aArg2);
    if (NS_FAILED(rv)) {
      return rv;
    }
#else
    aArg2 = sConnectedDeviceAddress;
#endif
    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonHandsfreeModule::DtmfNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  DtmfNotification::Dispatch(
    &BluetoothHandsfreeNotificationHandler::DtmfNotification,
    DtmfInitOp(aPDU));
}

// Init operator class for NRECNotification
class BluetoothDaemonHandsfreeModule::NRECInitOp final
  : private PDUInitOp
{
public:
  NRECInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (BluetoothHandsfreeNRECState& aArg1,
               BluetoothAddress& aArg2) const
  {
    DaemonSocketPDU& pdu = GetPDU();

    /* Read state */
    nsresult rv = UnpackPDU(pdu, aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }

    /* Read address */
#if ANDROID_VERSION >= 21
    rv = UnpackPDU(pdu, aArg2);
    if (NS_FAILED(rv)) {
      return rv;
    }
#else
    aArg2 = sConnectedDeviceAddress;
#endif
    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonHandsfreeModule::NRECNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  NRECNotification::Dispatch(
    &BluetoothHandsfreeNotificationHandler::NRECNotification,
    NRECInitOp(aPDU));
}

// Init operator class for CallHoldNotification
class BluetoothDaemonHandsfreeModule::CallHoldInitOp final
  : private PDUInitOp
{
public:
  CallHoldInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (BluetoothHandsfreeCallHoldType& aArg1,
               BluetoothAddress& aArg2) const
  {
    DaemonSocketPDU& pdu = GetPDU();

    /* Read type */
    nsresult rv = UnpackPDU(pdu, aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }

    /* Read address */
#if ANDROID_VERSION >= 21
    rv = UnpackPDU(pdu, aArg2);
    if (NS_FAILED(rv)) {
      return rv;
    }
#else
    aArg2 = sConnectedDeviceAddress;
#endif
    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonHandsfreeModule::CallHoldNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  CallHoldNotification::Dispatch(
    &BluetoothHandsfreeNotificationHandler::CallHoldNotification,
    CallHoldInitOp(aPDU));
}

// Init operator class for CnumNotification
class BluetoothDaemonHandsfreeModule::CnumInitOp final
  : private PDUInitOp
{
public:
  CnumInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (BluetoothAddress& aArg1) const
  {
    /* Read address */
#if ANDROID_VERSION >= 21
    nsresult rv = UnpackPDU(GetPDU(), aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }
#else
    aArg1 = sConnectedDeviceAddress;
#endif
    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonHandsfreeModule::CnumNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  CnumNotification::Dispatch(
    &BluetoothHandsfreeNotificationHandler::CnumNotification,
    CnumInitOp(aPDU));
}

// Init operator class for CindNotification
class BluetoothDaemonHandsfreeModule::CindInitOp final
  : private PDUInitOp
{
public:
  CindInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (BluetoothAddress& aArg1) const
  {
    /* Read address */
#if ANDROID_VERSION >= 21
    nsresult rv = UnpackPDU(GetPDU(), aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }
#else
    aArg1 = sConnectedDeviceAddress;
#endif
    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonHandsfreeModule::CindNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  CindNotification::Dispatch(
    &BluetoothHandsfreeNotificationHandler::CindNotification,
    CindInitOp(aPDU));
}

// Init operator class for CopsNotification
class BluetoothDaemonHandsfreeModule::CopsInitOp final
  : private PDUInitOp
{
public:
  CopsInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (BluetoothAddress& aArg1) const
  {
    /* Read address */
#if ANDROID_VERSION >= 21
    nsresult rv = UnpackPDU(GetPDU(), aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }
#else
    aArg1 = sConnectedDeviceAddress;
#endif
    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonHandsfreeModule::CopsNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  CopsNotification::Dispatch(
    &BluetoothHandsfreeNotificationHandler::CopsNotification,
    CopsInitOp(aPDU));
}

// Init operator class for ClccNotification
class BluetoothDaemonHandsfreeModule::ClccInitOp final
  : private PDUInitOp
{
public:
  ClccInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (BluetoothAddress& aArg1) const
  {
    /* Read address */
#if ANDROID_VERSION >= 21
    nsresult rv = UnpackPDU(GetPDU(), aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }
#else
    aArg1 = sConnectedDeviceAddress;
#endif
    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonHandsfreeModule::ClccNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ClccNotification::Dispatch(
    &BluetoothHandsfreeNotificationHandler::ClccNotification,
    ClccInitOp(aPDU));
}

// Init operator class for UnknownAtNotification
class BluetoothDaemonHandsfreeModule::UnknownAtInitOp final
  : private PDUInitOp
{
public:
  UnknownAtInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (nsCString& aArg1, BluetoothAddress& aArg2) const
  {
    DaemonSocketPDU& pdu = GetPDU();

    nsresult rv;
    /* Read address
     * It's a little weird to parse aArg2(aBdAddr) before parsing
     * aArg1(aAtString), but this order is defined in BlueZ 5.25 anyway.
     */
#if ANDROID_VERSION >= 21
    rv = UnpackPDU(pdu, aArg2);
    if (NS_FAILED(rv)) {
      return rv;
    }
#else
    aArg2 = sConnectedDeviceAddress;
#endif

    /* Read string */
    rv = UnpackPDU(pdu, UnpackCString0(aArg1));
    if (NS_FAILED(rv)) {
      return rv;
    }
    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonHandsfreeModule::UnknownAtNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  UnknownAtNotification::Dispatch(
    &BluetoothHandsfreeNotificationHandler::UnknownAtNotification,
    UnknownAtInitOp(aPDU));
}

// Init operator class for KeyPressedNotification
class BluetoothDaemonHandsfreeModule::KeyPressedInitOp final
  : private PDUInitOp
{
public:
  KeyPressedInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (BluetoothAddress& aArg1) const
  {
    /* Read address */
#if ANDROID_VERSION >= 21
    nsresult rv = UnpackPDU(GetPDU(), aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }
#else
    aArg1 = sConnectedDeviceAddress;
#endif
    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonHandsfreeModule::KeyPressedNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  KeyPressedNotification::Dispatch(
    &BluetoothHandsfreeNotificationHandler::KeyPressedNotification,
    KeyPressedInitOp(aPDU));
}

void
BluetoothDaemonHandsfreeModule::WbsNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  WbsNotification::Dispatch(
    &BluetoothHandsfreeNotificationHandler::WbsNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHandsfreeModule::HandleNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  DaemonSocketResultHandler* aRes)
{
  static void (BluetoothDaemonHandsfreeModule::* const HandleNtf[])(
    const DaemonSocketPDUHeader&, DaemonSocketPDU&) = {
    [0] = &BluetoothDaemonHandsfreeModule::ConnectionStateNtf,
    [1] = &BluetoothDaemonHandsfreeModule::AudioStateNtf,
    [2] = &BluetoothDaemonHandsfreeModule::VoiceRecognitionNtf,
    [3] = &BluetoothDaemonHandsfreeModule::AnswerCallNtf,
    [4] = &BluetoothDaemonHandsfreeModule::HangupCallNtf,
    [5] = &BluetoothDaemonHandsfreeModule::VolumeNtf,
    [6] = &BluetoothDaemonHandsfreeModule::DialCallNtf,
    [7] = &BluetoothDaemonHandsfreeModule::DtmfNtf,
    [8] = &BluetoothDaemonHandsfreeModule::NRECNtf,
    [9] = &BluetoothDaemonHandsfreeModule::CallHoldNtf,
    [10] = &BluetoothDaemonHandsfreeModule::CnumNtf,
    [11] = &BluetoothDaemonHandsfreeModule::CindNtf,
    [12] = &BluetoothDaemonHandsfreeModule::CopsNtf,
    [13] = &BluetoothDaemonHandsfreeModule::ClccNtf,
    [14] = &BluetoothDaemonHandsfreeModule::UnknownAtNtf,
    [15] = &BluetoothDaemonHandsfreeModule::KeyPressedNtf,
    [16] = &BluetoothDaemonHandsfreeModule::WbsNtf
  };

  MOZ_ASSERT(!NS_IsMainThread());

  uint8_t index = aHeader.mOpcode - 0x81;

  if (NS_WARN_IF(!(index < MOZ_ARRAY_LENGTH(HandleNtf))) ||
      NS_WARN_IF(!HandleNtf[index])) {
    return;
  }

  (this->*(HandleNtf[index]))(aHeader, aPDU);
}

//
// Handsfree interface
//

BluetoothDaemonHandsfreeInterface::BluetoothDaemonHandsfreeInterface(
  BluetoothDaemonHandsfreeModule* aModule)
  : mModule(aModule)
{ }

BluetoothDaemonHandsfreeInterface::~BluetoothDaemonHandsfreeInterface()
{ }

void BluetoothDaemonHandsfreeInterface::SetNotificationHandler(
  BluetoothHandsfreeNotificationHandler* aNotificationHandler)
{
  MOZ_ASSERT(mModule);

  mModule->SetNotificationHandler(aNotificationHandler);
}

/* Connect / Disconnect */

void
BluetoothDaemonHandsfreeInterface::Connect(
  const BluetoothAddress& aBdAddr, BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ConnectCmd(aBdAddr, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonHandsfreeInterface::Disconnect(
  const BluetoothAddress& aBdAddr, BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->DisconnectCmd(aBdAddr, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonHandsfreeInterface::ConnectAudio(
  const BluetoothAddress& aBdAddr, BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ConnectAudioCmd(aBdAddr, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonHandsfreeInterface::DisconnectAudio(
  const BluetoothAddress& aBdAddr, BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->DisconnectAudioCmd(aBdAddr, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Voice Recognition */

void
BluetoothDaemonHandsfreeInterface::StartVoiceRecognition(
  const BluetoothAddress& aBdAddr, BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->StartVoiceRecognitionCmd(aBdAddr, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonHandsfreeInterface::StopVoiceRecognition(
  const BluetoothAddress& aBdAddr, BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->StopVoiceRecognitionCmd(aBdAddr, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Volume */

void
BluetoothDaemonHandsfreeInterface::VolumeControl(
  BluetoothHandsfreeVolumeType aType, int aVolume,
  const BluetoothAddress& aBdAddr,
  BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->VolumeControlCmd(aType, aVolume, aBdAddr, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Device status */

void
BluetoothDaemonHandsfreeInterface::DeviceStatusNotification(
  BluetoothHandsfreeNetworkState aNtkState,
  BluetoothHandsfreeServiceType aSvcType, int aSignal, int aBattChg,
  BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->DeviceStatusNotificationCmd(aNtkState, aSvcType,
                                                     aSignal, aBattChg,
                                                     aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Responses */

void
BluetoothDaemonHandsfreeInterface::CopsResponse(
  const char* aCops, const BluetoothAddress& aBdAddr,
  BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->CopsResponseCmd(aCops, aBdAddr, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonHandsfreeInterface::CindResponse(
  int aSvc, int aNumActive, int aNumHeld,
  BluetoothHandsfreeCallState aCallSetupState,
  int aSignal, int aRoam, int aBattChg,
  const BluetoothAddress& aBdAddr, BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->CindResponseCmd(aSvc, aNumActive, aNumHeld,
                                         aCallSetupState, aSignal,
                                         aRoam, aBattChg, aBdAddr, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonHandsfreeInterface::FormattedAtResponse(
  const char* aRsp, const BluetoothAddress& aBdAddr,
  BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->FormattedAtResponseCmd(aRsp, aBdAddr, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonHandsfreeInterface::AtResponse(
  BluetoothHandsfreeAtResponse aResponseCode, int aErrorCode,
  const BluetoothAddress& aBdAddr, BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->AtResponseCmd(aResponseCode, aErrorCode,
                                       aBdAddr, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonHandsfreeInterface::ClccResponse(
  int aIndex, BluetoothHandsfreeCallDirection aDir,
  BluetoothHandsfreeCallState aState,
  BluetoothHandsfreeCallMode aMode,
  BluetoothHandsfreeCallMptyType aMpty,
  const nsAString& aNumber,
  BluetoothHandsfreeCallAddressType aType,
  const BluetoothAddress& aBdAddr,
  BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ClccResponseCmd(aIndex, aDir, aState, aMode, aMpty,
                                         aNumber, aType, aBdAddr, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Phone State */

void
BluetoothDaemonHandsfreeInterface::PhoneStateChange(
  int aNumActive, int aNumHeld,
  BluetoothHandsfreeCallState aCallSetupState,
  const nsAString& aNumber,
  BluetoothHandsfreeCallAddressType aType,
  BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->PhoneStateChangeCmd(aNumActive, aNumHeld,
                                             aCallSetupState, aNumber,
                                             aType, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Wide Band Speech */

void
BluetoothDaemonHandsfreeInterface::ConfigureWbs(
  const BluetoothAddress& aBdAddr, BluetoothHandsfreeWbsConfig aConfig,
  BluetoothHandsfreeResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ConfigureWbsCmd(aBdAddr, aConfig, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonHandsfreeInterface::DispatchError(
  BluetoothHandsfreeResultHandler* aRes, BluetoothStatus aStatus)
{
  DaemonResultRunnable1<BluetoothHandsfreeResultHandler, void,
                        BluetoothStatus, BluetoothStatus>::Dispatch(
    aRes, &BluetoothHandsfreeResultHandler::OnError,
    ConstantInitOp1<BluetoothStatus>(aStatus));
}

void
BluetoothDaemonHandsfreeInterface::DispatchError(
  BluetoothHandsfreeResultHandler* aRes, nsresult aRv)
{
  BluetoothStatus status;

  if (NS_WARN_IF(NS_FAILED(Convert(aRv, status)))) {
    status = STATUS_FAIL;
  }
  DispatchError(aRes, status);
}

END_BLUETOOTH_NAMESPACE
