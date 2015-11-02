/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothDaemonA2dpInterface.h"
#include "mozilla/unused.h"

BEGIN_BLUETOOTH_NAMESPACE

using namespace mozilla::ipc;

//
// A2DP module
//

BluetoothA2dpNotificationHandler*
  BluetoothDaemonA2dpModule::sNotificationHandler;

void
BluetoothDaemonA2dpModule::SetNotificationHandler(
  BluetoothA2dpNotificationHandler* aNotificationHandler)
{
  sNotificationHandler = aNotificationHandler;
}

void
BluetoothDaemonA2dpModule::HandleSvc(const DaemonSocketPDUHeader& aHeader,
                                     DaemonSocketPDU& aPDU,
                                     DaemonSocketResultHandler* aRes)
{
  static void (BluetoothDaemonA2dpModule::* const HandleOp[])(
    const DaemonSocketPDUHeader&, DaemonSocketPDU&,
    DaemonSocketResultHandler*) = {
    [0] = &BluetoothDaemonA2dpModule::HandleRsp,
    [1] = &BluetoothDaemonA2dpModule::HandleNtf
  };

  MOZ_ASSERT(!NS_IsMainThread());

  // negate twice to map bit to 0/1
  unsigned int isNtf = !!(aHeader.mOpcode & 0x80);

  (this->*(HandleOp[isNtf]))(aHeader, aPDU, aRes);
}

// Commands
//

nsresult
BluetoothDaemonA2dpModule::ConnectCmd(
  const BluetoothAddress& aRemoteAddr, BluetoothA2dpResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(new DaemonSocketPDU(SERVICE_ID,
                                                     OPCODE_CONNECT,
                                                     6)); // Address
  nsresult rv = PackPDU(aRemoteAddr, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  Unused << pdu.forget();
  return NS_OK;
}

nsresult
BluetoothDaemonA2dpModule::DisconnectCmd(
  const BluetoothAddress& aRemoteAddr, BluetoothA2dpResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(new DaemonSocketPDU(SERVICE_ID,
                                                     OPCODE_DISCONNECT,
                                                     6)); // Address
  nsresult rv = PackPDU(aRemoteAddr, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  Unused << pdu.forget();
  return NS_OK;
}

// Responses
//

void
BluetoothDaemonA2dpModule::ErrorRsp(
  const DaemonSocketPDUHeader& aHeader,
  DaemonSocketPDU& aPDU, BluetoothA2dpResultHandler* aRes)
{
  ErrorRunnable::Dispatch(
    aRes, &BluetoothA2dpResultHandler::OnError, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonA2dpModule::ConnectRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothA2dpResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothA2dpResultHandler::Connect, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonA2dpModule::DisconnectRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothA2dpResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothA2dpResultHandler::Disconnect, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonA2dpModule::HandleRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  DaemonSocketResultHandler* aRes)
{
  static void (BluetoothDaemonA2dpModule::* const HandleRsp[])(
    const DaemonSocketPDUHeader&,
    DaemonSocketPDU&,
    BluetoothA2dpResultHandler*) = {
    [OPCODE_ERROR] = &BluetoothDaemonA2dpModule::ErrorRsp,
    [OPCODE_CONNECT] = &BluetoothDaemonA2dpModule::ConnectRsp,
    [OPCODE_DISCONNECT] = &BluetoothDaemonA2dpModule::DisconnectRsp
  };

  MOZ_ASSERT(!NS_IsMainThread()); // I/O thread

  if (NS_WARN_IF(!(aHeader.mOpcode < MOZ_ARRAY_LENGTH(HandleRsp))) ||
      NS_WARN_IF(!HandleRsp[aHeader.mOpcode])) {
    return;
  }

  RefPtr<BluetoothA2dpResultHandler> res =
    static_cast<BluetoothA2dpResultHandler*>(aRes);

  if (!res) {
    return; // Return early if no result handler has been set for response
  }

  (this->*(HandleRsp[aHeader.mOpcode]))(aHeader, aPDU, res);
}

// Notifications
//

// Returns the current notification handler to a notification runnable
class BluetoothDaemonA2dpModule::NotificationHandlerWrapper final
{
public:
  typedef BluetoothA2dpNotificationHandler ObjectType;

  static ObjectType* GetInstance()
  {
    MOZ_ASSERT(NS_IsMainThread());

    return sNotificationHandler;
  }
};

void
BluetoothDaemonA2dpModule::ConnectionStateNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ConnectionStateNotification::Dispatch(
    &BluetoothA2dpNotificationHandler::ConnectionStateNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonA2dpModule::AudioStateNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  AudioStateNotification::Dispatch(
    &BluetoothA2dpNotificationHandler::AudioStateNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonA2dpModule::AudioConfigNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  AudioConfigNotification::Dispatch(
    &BluetoothA2dpNotificationHandler::AudioConfigNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonA2dpModule::HandleNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  DaemonSocketResultHandler* aRes)
{
  static void (BluetoothDaemonA2dpModule::* const HandleNtf[])(
    const DaemonSocketPDUHeader&, DaemonSocketPDU&) = {
    [0] = &BluetoothDaemonA2dpModule::ConnectionStateNtf,
    [1] = &BluetoothDaemonA2dpModule::AudioStateNtf,
#if ANDROID_VERSION >= 21
    [2] = &BluetoothDaemonA2dpModule::AudioConfigNtf
#endif
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
// A2DP interface
//

BluetoothDaemonA2dpInterface::BluetoothDaemonA2dpInterface(
  BluetoothDaemonA2dpModule* aModule)
  : mModule(aModule)
{ }

BluetoothDaemonA2dpInterface::~BluetoothDaemonA2dpInterface()
{ }

void
BluetoothDaemonA2dpInterface::SetNotificationHandler(
  BluetoothA2dpNotificationHandler* aNotificationHandler)
{
  MOZ_ASSERT(mModule);

  mModule->SetNotificationHandler(aNotificationHandler);
}

/* Connect / Disconnect */

void
BluetoothDaemonA2dpInterface::Connect(
  const BluetoothAddress& aBdAddr, BluetoothA2dpResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ConnectCmd(aBdAddr, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonA2dpInterface::Disconnect(
  const BluetoothAddress& aBdAddr, BluetoothA2dpResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->DisconnectCmd(aBdAddr, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonA2dpInterface::DispatchError(
  BluetoothA2dpResultHandler* aRes, BluetoothStatus aStatus)
{
  DaemonResultRunnable1<BluetoothA2dpResultHandler, void,
                        BluetoothStatus, BluetoothStatus>::Dispatch(
    aRes, &BluetoothA2dpResultHandler::OnError,
    ConstantInitOp1<BluetoothStatus>(aStatus));
}

void
BluetoothDaemonA2dpInterface::DispatchError(
  BluetoothA2dpResultHandler* aRes, nsresult aRv)
{
  BluetoothStatus status;

  if (NS_WARN_IF(NS_FAILED(Convert(aRv, status)))) {
    status = STATUS_FAIL;
  }
  DispatchError(aRes, status);
}

END_BLUETOOTH_NAMESPACE
