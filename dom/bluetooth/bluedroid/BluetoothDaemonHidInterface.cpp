/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothDaemonHidInterface.h"
#include "mozilla/unused.h"

BEGIN_BLUETOOTH_NAMESPACE

using namespace mozilla::ipc;

//
// Hid module
//

BluetoothHidNotificationHandler*
  BluetoothDaemonHidModule::sNotificationHandler;

void
BluetoothDaemonHidModule::SetNotificationHandler(
  BluetoothHidNotificationHandler* aNotificationHandler)
{
  sNotificationHandler = aNotificationHandler;
}

void
BluetoothDaemonHidModule::HandleSvc(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  DaemonSocketResultHandler* aRes)
{
  static void (BluetoothDaemonHidModule::* const HandleOp[])(
    const DaemonSocketPDUHeader&, DaemonSocketPDU&,
    DaemonSocketResultHandler*) = {
    [0] = &BluetoothDaemonHidModule::HandleRsp,
    [1] = &BluetoothDaemonHidModule::HandleNtf
  };

  MOZ_ASSERT(!NS_IsMainThread());

  // Negate twice to map bit to 0/1
  unsigned long isNtf = !!(aHeader.mOpcode & 0x80);

  (this->*(HandleOp[isNtf]))(aHeader, aPDU, aRes);
}

// Commands
//

nsresult
BluetoothDaemonHidModule::ConnectCmd(
  const BluetoothAddress& aRemoteAddr, BluetoothHidResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CONNECT, 6)); // Address

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
BluetoothDaemonHidModule::DisconnectCmd(
  const BluetoothAddress& aRemoteAddr, BluetoothHidResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_DISCONNECT, 6)); // Address

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
BluetoothDaemonHidModule::VirtualUnplugCmd(
  const BluetoothAddress& aRemoteAddr, BluetoothHidResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_VIRTUAL_UNPLUG, 6)); // Address

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
BluetoothDaemonHidModule::SetInfoCmd(
  const BluetoothAddress& aRemoteAddr,
  const BluetoothHidInfoParam& aHidInfoParam,
  BluetoothHidResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_SET_INFO, 0));

  nsresult rv = PackPDU(aRemoteAddr, aHidInfoParam, *pdu);
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
BluetoothDaemonHidModule::GetProtocolCmd(
  const BluetoothAddress& aRemoteAddr,
  BluetoothHidProtocolMode aHidProtocolMode,
  BluetoothHidResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_GET_PROTOCOL,
                        6 + // Address
                        1)); // Protocol Mode

  nsresult rv = PackPDU(aRemoteAddr, aHidProtocolMode, *pdu);
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
BluetoothDaemonHidModule::SetProtocolCmd(
  const BluetoothAddress& aRemoteAddr,
  BluetoothHidProtocolMode aHidProtocolMode,
  BluetoothHidResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_SET_PROTOCOL,
                        6 + // Remote Address
                        1)); // Protocol Mode

  nsresult rv = PackPDU(aRemoteAddr, aHidProtocolMode, *pdu);
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
BluetoothDaemonHidModule::GetReportCmd(
  const BluetoothAddress& aRemoteAddr, BluetoothHidReportType aType,
  uint8_t aReportId, uint16_t aBuffSize, BluetoothHidResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_GET_REPORT,
                        6 + // Address
                        1 + // Report Type
                        1 + // Report ID
                        2)); // Buffer Size

  nsresult rv = PackPDU(aRemoteAddr, aType, aReportId, aBuffSize, *pdu);
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
BluetoothDaemonHidModule::SetReportCmd(
  const BluetoothAddress& aRemoteAddr, BluetoothHidReportType aType,
  const BluetoothHidReport& aReport, BluetoothHidResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_SET_REPORT, 0));

  nsresult rv = PackPDU(aRemoteAddr, aType, aReport, *pdu);

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
BluetoothDaemonHidModule::SendDataCmd(
  const BluetoothAddress& aRemoteAddr, uint16_t aDataLen, const uint8_t* aData,
  BluetoothHidResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_SEND_DATA, 0));

  nsresult rv = PackPDU(aRemoteAddr, aDataLen,
                        PackArray<uint8_t>(aData, aDataLen), *pdu);
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
BluetoothDaemonHidModule::ErrorRsp(
  const DaemonSocketPDUHeader& aHeader,
  DaemonSocketPDU& aPDU, BluetoothHidResultHandler* aRes)
{
  ErrorRunnable::Dispatch(
    aRes, &BluetoothHidResultHandler::OnError, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHidModule::ConnectRsp(
  const DaemonSocketPDUHeader& aHeader,
  DaemonSocketPDU& aPDU, BluetoothHidResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothHidResultHandler::Connect, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHidModule::DisconnectRsp(
  const DaemonSocketPDUHeader& aHeader,
  DaemonSocketPDU& aPDU, BluetoothHidResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothHidResultHandler::Disconnect, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHidModule::VirtualUnplugRsp(
  const DaemonSocketPDUHeader& aHeader,
  DaemonSocketPDU& aPDU, BluetoothHidResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothHidResultHandler::VirtualUnplug, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHidModule::SetInfoRsp(
  const DaemonSocketPDUHeader& aHeader,
  DaemonSocketPDU& aPDU, BluetoothHidResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothHidResultHandler::SetInfo, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHidModule::GetProtocolRsp(
  const DaemonSocketPDUHeader& aHeader,
  DaemonSocketPDU& aPDU, BluetoothHidResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothHidResultHandler::GetProtocol, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHidModule::SetProtocolRsp(
  const DaemonSocketPDUHeader& aHeader,
  DaemonSocketPDU& aPDU, BluetoothHidResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothHidResultHandler::SetProtocol, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHidModule::GetReportRsp(
  const DaemonSocketPDUHeader& aHeader,
  DaemonSocketPDU& aPDU, BluetoothHidResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothHidResultHandler::GetReport, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHidModule::SetReportRsp(
  const DaemonSocketPDUHeader& aHeader,
  DaemonSocketPDU& aPDU, BluetoothHidResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothHidResultHandler::SetReport, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHidModule::SendDataRsp(
  const DaemonSocketPDUHeader& aHeader,
  DaemonSocketPDU& aPDU, BluetoothHidResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothHidResultHandler::SendData, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHidModule::HandleRsp(
  const DaemonSocketPDUHeader& aHeader,
  DaemonSocketPDU& aPDU, DaemonSocketResultHandler* aRes)
{
  static void (BluetoothDaemonHidModule::* const HandleRsp[])(
    const DaemonSocketPDUHeader&,
    DaemonSocketPDU&,
    BluetoothHidResultHandler*) = {
    [OPCODE_ERROR] = &BluetoothDaemonHidModule::ErrorRsp,
    [OPCODE_CONNECT] = &BluetoothDaemonHidModule::ConnectRsp,
    [OPCODE_DISCONNECT] = &BluetoothDaemonHidModule::DisconnectRsp,
    [OPCODE_VIRTUAL_UNPLUG] = &BluetoothDaemonHidModule::VirtualUnplugRsp,
    [OPCODE_SET_INFO] = &BluetoothDaemonHidModule::SetInfoRsp,
    [OPCODE_GET_PROTOCOL] = &BluetoothDaemonHidModule::GetProtocolRsp,
    [OPCODE_SET_PROTOCOL] = &BluetoothDaemonHidModule::SetProtocolRsp,
    [OPCODE_GET_REPORT] = &BluetoothDaemonHidModule::GetReportRsp,
    [OPCODE_SET_REPORT] = &BluetoothDaemonHidModule::SetReportRsp,
    [OPCODE_SEND_DATA] = &BluetoothDaemonHidModule::SendDataRsp
  };

  MOZ_ASSERT(!NS_IsMainThread()); // I/O thread

  if (NS_WARN_IF(!(aHeader.mOpcode < MOZ_ARRAY_LENGTH(HandleRsp))) ||
      NS_WARN_IF(!HandleRsp[aHeader.mOpcode])) {
    return;
  }

  RefPtr<BluetoothHidResultHandler> res =
    static_cast<BluetoothHidResultHandler*>(aRes);

  if (!res) {
    return; // Return early if no result handler has been set for response
  }

  (this->*(HandleRsp[aHeader.mOpcode]))(aHeader, aPDU, res);
}

// Notifications
//

// Returns the current notification handler to a notification runnable
class BluetoothDaemonHidModule::NotificationHandlerWrapper final
{
public:
  typedef BluetoothHidNotificationHandler ObjectType;

  static ObjectType* GetInstance()
  {
    MOZ_ASSERT(NS_IsMainThread());

    return sNotificationHandler;
  }
};

void
BluetoothDaemonHidModule::ConnectionStateNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ConnectionStateNotification::Dispatch(
    &BluetoothHidNotificationHandler::ConnectionStateNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHidModule::HidInfoNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  HidInfoNotification::Dispatch(
    &BluetoothHidNotificationHandler::HidInfoNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHidModule::ProtocolModeNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ProtocolModeNotification::Dispatch(
    &BluetoothHidNotificationHandler::ProtocolModeNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHidModule::IdleTimeNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  IdleTimeNotification::Dispatch(
    &BluetoothHidNotificationHandler::IdleTimeNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHidModule::GetReportNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  GetReportNotification::Dispatch(
    &BluetoothHidNotificationHandler::GetReportNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHidModule::VirtualUnplugNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  VirtualUnplugNotification::Dispatch(
    &BluetoothHidNotificationHandler::VirtualUnplugNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHidModule::HandshakeNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  HandshakeNotification::Dispatch(
    &BluetoothHidNotificationHandler::HandshakeNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonHidModule::HandleNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  DaemonSocketResultHandler* aRes)
{
  static void (BluetoothDaemonHidModule::* const HandleNtf[])(
    const DaemonSocketPDUHeader&, DaemonSocketPDU&) = {
      [0] = &BluetoothDaemonHidModule::ConnectionStateNtf,
      [1] = &BluetoothDaemonHidModule::HidInfoNtf,
      [2] = &BluetoothDaemonHidModule::ProtocolModeNtf,
      [3] = &BluetoothDaemonHidModule::IdleTimeNtf,
      [4] = &BluetoothDaemonHidModule::GetReportNtf,
      [5] = &BluetoothDaemonHidModule::VirtualUnplugNtf,
      [6] = &BluetoothDaemonHidModule::HandshakeNtf
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
// Hid Interface
//

BluetoothDaemonHidInterface::BluetoothDaemonHidInterface(
  BluetoothDaemonHidModule* aModule)
  : mModule(aModule)
{ }

BluetoothDaemonHidInterface::~BluetoothDaemonHidInterface()
{ }

void
BluetoothDaemonHidInterface::SetNotificationHandler(
  BluetoothHidNotificationHandler* aNotificationHandler)
{
  MOZ_ASSERT(mModule);

  mModule->SetNotificationHandler(aNotificationHandler);
}

/* Connect / Disconnect */

void
BluetoothDaemonHidInterface::Connect(
  const BluetoothAddress& aBdAddr, BluetoothHidResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ConnectCmd(aBdAddr, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonHidInterface::Disconnect(
  const BluetoothAddress& aBdAddr, BluetoothHidResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->DisconnectCmd(aBdAddr, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Virtual Unplug */

void
BluetoothDaemonHidInterface::VirtualUnplug(
  const BluetoothAddress& aBdAddr, BluetoothHidResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->VirtualUnplugCmd(aBdAddr, aRes);

  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Set Info */

void
BluetoothDaemonHidInterface::SetInfo(
  const BluetoothAddress& aBdAddr,
  const BluetoothHidInfoParam& aHidInfoParam,
  BluetoothHidResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->SetInfoCmd(aBdAddr, aHidInfoParam, aRes);

  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Protocol */

void
BluetoothDaemonHidInterface::GetProtocol(
  const BluetoothAddress& aBdAddr,
  BluetoothHidProtocolMode aHidProtocolMode, BluetoothHidResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->GetProtocolCmd(aBdAddr, aHidProtocolMode, aRes);

  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonHidInterface::SetProtocol(
  const BluetoothAddress& aBdAddr,
  BluetoothHidProtocolMode aHidProtocolMode, BluetoothHidResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->SetProtocolCmd(aBdAddr, aHidProtocolMode, aRes);

  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Report */

void
BluetoothDaemonHidInterface::GetReport(
  const BluetoothAddress& aBdAddr, BluetoothHidReportType aType,
  uint8_t aReportId, uint16_t aBuffSize, BluetoothHidResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->GetReportCmd(
    aBdAddr, aType, aReportId, aBuffSize, aRes);

  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonHidInterface::SetReport(
  const BluetoothAddress& aBdAddr, BluetoothHidReportType aType,
  const BluetoothHidReport& aReport, BluetoothHidResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->SetReportCmd(
    aBdAddr, aType, aReport, aRes);

  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Send Data */

void
BluetoothDaemonHidInterface::SendData(
  const BluetoothAddress& aBdAddr, uint16_t aDataLen, const uint8_t* aData,
  BluetoothHidResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->SendDataCmd(aBdAddr, aDataLen, aData, aRes);

  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonHidInterface::DispatchError(
  BluetoothHidResultHandler* aRes, BluetoothStatus aStatus)
{
  DaemonResultRunnable1<BluetoothHidResultHandler, void,
                        BluetoothStatus, BluetoothStatus>::Dispatch(
    aRes, &BluetoothHidResultHandler::OnError,
    ConstantInitOp1<BluetoothStatus>(aStatus));
}

void
BluetoothDaemonHidInterface::DispatchError(
  BluetoothHidResultHandler* aRes, nsresult aRv)
{
  BluetoothStatus status;

  if (NS_WARN_IF(NS_FAILED(Convert(aRv, status)))) {
    status = STATUS_FAIL;
  }
  DispatchError(aRes, status);
}

END_BLUETOOTH_NAMESPACE
