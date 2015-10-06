/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothDaemonCoreInterface.h"
#include "mozilla/unused.h"

BEGIN_BLUETOOTH_NAMESPACE

using namespace mozilla::ipc;

//
// Core module
//

const int BluetoothDaemonCoreModule::MAX_NUM_CLIENTS = 1;

BluetoothNotificationHandler* BluetoothDaemonCoreModule::sNotificationHandler;

void
BluetoothDaemonCoreModule::SetNotificationHandler(
  BluetoothNotificationHandler* aNotificationHandler)
{
  sNotificationHandler = aNotificationHandler;
}

BluetoothNotificationHandler*
BluetoothDaemonCoreModule::GetNotificationHandler()
{
  return sNotificationHandler;
}

void
BluetoothDaemonCoreModule::HandleSvc(const DaemonSocketPDUHeader& aHeader,
                                     DaemonSocketPDU& aPDU,
                                     DaemonSocketResultHandler* aRes)
{
  static void (BluetoothDaemonCoreModule::* const HandleOp[])(
    const DaemonSocketPDUHeader&, DaemonSocketPDU&,
    DaemonSocketResultHandler*) = {
    [0] = &BluetoothDaemonCoreModule::HandleRsp,
    [1] = &BluetoothDaemonCoreModule::HandleNtf
  };

  MOZ_ASSERT(!NS_IsMainThread());

  (this->*(HandleOp[!!(aHeader.mOpcode & 0x80)]))(aHeader, aPDU, aRes);
}

// Commands
//

nsresult
BluetoothDaemonCoreModule::EnableCmd(BluetoothResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_ENABLE,
                        0));

  nsresult rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return rv;
}

nsresult
BluetoothDaemonCoreModule::DisableCmd(BluetoothResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_DISABLE,
                        0));

  nsresult rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return rv;
}

nsresult
BluetoothDaemonCoreModule::GetAdapterPropertiesCmd(
  BluetoothResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_GET_ADAPTER_PROPERTIES,
                        0));

  nsresult rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return rv;
}

nsresult
BluetoothDaemonCoreModule::GetAdapterPropertyCmd(BluetoothPropertyType aType,
                                                 BluetoothResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_GET_ADAPTER_PROPERTY,
                        0));

  nsresult rv = PackPDU(aType, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return rv;
}

nsresult
BluetoothDaemonCoreModule::SetAdapterPropertyCmd(
  const BluetoothProperty& aProperty, BluetoothResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_SET_ADAPTER_PROPERTY,
                        0));

  nsresult rv = PackPDU(aProperty, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return rv;
}

nsresult
BluetoothDaemonCoreModule::GetRemoteDevicePropertiesCmd(
  const BluetoothAddress& aRemoteAddr, BluetoothResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_GET_REMOTE_DEVICE_PROPERTIES,
                        0));

  nsresult rv = PackPDU(aRemoteAddr, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return rv;
}

nsresult
BluetoothDaemonCoreModule::GetRemoteDevicePropertyCmd(
  const BluetoothAddress& aRemoteAddr,
  BluetoothPropertyType aType,
  BluetoothResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_GET_REMOTE_DEVICE_PROPERTY,
                        0));

  nsresult rv = PackPDU(aRemoteAddr, aType, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return rv;
}

nsresult
BluetoothDaemonCoreModule::SetRemoteDevicePropertyCmd(
  const BluetoothAddress& aRemoteAddr,
  const BluetoothProperty& aProperty,
  BluetoothResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_SET_REMOTE_DEVICE_PROPERTY,
                        0));

  nsresult rv = PackPDU(aRemoteAddr, aProperty, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return rv;
}

nsresult
BluetoothDaemonCoreModule::GetRemoteServiceRecordCmd(
  const BluetoothAddress& aRemoteAddr, const BluetoothUuid& aUuid,
  BluetoothResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_GET_REMOTE_SERVICE_RECORD,
                        0));

  nsresult rv = PackPDU(aRemoteAddr, aUuid, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return rv;
}

nsresult
BluetoothDaemonCoreModule::GetRemoteServicesCmd(
  const BluetoothAddress& aRemoteAddr, BluetoothResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_GET_REMOTE_SERVICES, 0));

  nsresult rv = PackPDU(aRemoteAddr, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return rv;
}

nsresult
BluetoothDaemonCoreModule::StartDiscoveryCmd(BluetoothResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_START_DISCOVERY,
                        0));

  nsresult rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return rv;
}

nsresult
BluetoothDaemonCoreModule::CancelDiscoveryCmd(BluetoothResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CANCEL_DISCOVERY,
                        0));

  nsresult rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return rv;
}

nsresult
BluetoothDaemonCoreModule::CreateBondCmd(const BluetoothAddress& aBdAddr,
                                         BluetoothTransport aTransport,
                                         BluetoothResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CREATE_BOND,
                        0));

#if ANDROID_VERSION >= 21
  nsresult rv = PackPDU(aBdAddr, aTransport, *pdu);
#else
  nsresult rv = PackPDU(aBdAddr, *pdu);
#endif
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return rv;
}

nsresult
BluetoothDaemonCoreModule::RemoveBondCmd(const BluetoothAddress& aBdAddr,
                                         BluetoothResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_REMOVE_BOND,
                        0));

  nsresult rv = PackPDU(aBdAddr, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return rv;
}

nsresult
BluetoothDaemonCoreModule::CancelBondCmd(const BluetoothAddress& aBdAddr,
                                         BluetoothResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CANCEL_BOND,
                        0));

  nsresult rv = PackPDU(aBdAddr, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return rv;
}

nsresult
BluetoothDaemonCoreModule::PinReplyCmd(const BluetoothAddress& aBdAddr,
                                       bool aAccept,
                                       const BluetoothPinCode& aPinCode,
                                       BluetoothResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_PIN_REPLY,
                        0));

  nsresult rv = PackPDU(aBdAddr, aAccept, aPinCode, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return rv;
}

nsresult
BluetoothDaemonCoreModule::SspReplyCmd(const BluetoothAddress& aBdAddr,
                                       BluetoothSspVariant aVariant,
                                       bool aAccept, uint32_t aPasskey,
                                       BluetoothResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_SSP_REPLY,
                        0));

  nsresult rv = PackPDU(aBdAddr, aVariant, aAccept, aPasskey, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return rv;
}

nsresult
BluetoothDaemonCoreModule::DutModeConfigureCmd(bool aEnable,
                                               BluetoothResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_DUT_MODE_CONFIGURE,
                        0));

  nsresult rv = PackPDU(aEnable, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return rv;
}

nsresult
BluetoothDaemonCoreModule::DutModeSendCmd(uint16_t aOpcode,
                                          uint8_t* aBuf, uint8_t aLen,
                                          BluetoothResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_DUT_MODE_SEND,
                        0));

  nsresult rv = PackPDU(aOpcode, aLen, PackArray<uint8_t>(aBuf, aLen),
                        *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return rv;
}

nsresult
BluetoothDaemonCoreModule::LeTestModeCmd(uint16_t aOpcode,
                                         uint8_t* aBuf, uint8_t aLen,
                                         BluetoothResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_LE_TEST_MODE,
                        0));

  nsresult rv = PackPDU(aOpcode, aLen, PackArray<uint8_t>(aBuf, aLen),
                        *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return rv;
}

// Responses
//

void
BluetoothDaemonCoreModule::ErrorRsp(const DaemonSocketPDUHeader& aHeader,
                                    DaemonSocketPDU& aPDU,
                                    BluetoothResultHandler* aRes)
{
  ErrorRunnable::Dispatch(
    aRes, &BluetoothResultHandler::OnError, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::EnableRsp(const DaemonSocketPDUHeader& aHeader,
                                     DaemonSocketPDU& aPDU,
                                     BluetoothResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothResultHandler::Enable, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::DisableRsp(const DaemonSocketPDUHeader& aHeader,
                                      DaemonSocketPDU& aPDU,
                                      BluetoothResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothResultHandler::Disable, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::GetAdapterPropertiesRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothResultHandler::GetAdapterProperties,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::GetAdapterPropertyRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothResultHandler::GetAdapterProperty,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::SetAdapterPropertyRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothResultHandler::SetAdapterProperty,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::GetRemoteDevicePropertiesRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothResultHandler::GetRemoteDeviceProperties,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::GetRemoteDevicePropertyRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothResultHandler::GetRemoteDeviceProperty,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::SetRemoteDevicePropertyRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothResultHandler::SetRemoteDeviceProperty,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::GetRemoteServiceRecordRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothResultHandler::GetRemoteServiceRecord,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::GetRemoteServicesRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothResultHandler::GetRemoteServices,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::StartDiscoveryRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothResultHandler::StartDiscovery,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::CancelDiscoveryRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothResultHandler::CancelDiscovery,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::CreateBondRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothResultHandler::CreateBond,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::RemoveBondRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothResultHandler::RemoveBond,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::CancelBondRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothResultHandler::CancelBond,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::PinReplyRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothResultHandler::PinReply,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::SspReplyRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothResultHandler::SspReply,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::DutModeConfigureRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothResultHandler::DutModeConfigure,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::DutModeSendRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothResultHandler::DutModeSend,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::LeTestModeRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothResultHandler::LeTestMode,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::HandleRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  DaemonSocketResultHandler* aRes)
{
  static void (BluetoothDaemonCoreModule::* const HandleRsp[])(
    const DaemonSocketPDUHeader&,
    DaemonSocketPDU&,
    BluetoothResultHandler*) = {
    [OPCODE_ERROR] =
      &BluetoothDaemonCoreModule::ErrorRsp,
    [OPCODE_ENABLE] =
      &BluetoothDaemonCoreModule::EnableRsp,
    [OPCODE_DISABLE] =
      &BluetoothDaemonCoreModule::DisableRsp,
    [OPCODE_GET_ADAPTER_PROPERTIES] =
      &BluetoothDaemonCoreModule::GetAdapterPropertiesRsp,
    [OPCODE_GET_ADAPTER_PROPERTY] =
      &BluetoothDaemonCoreModule::GetAdapterPropertyRsp,
    [OPCODE_SET_ADAPTER_PROPERTY] =
      &BluetoothDaemonCoreModule::SetAdapterPropertyRsp,
    [OPCODE_GET_REMOTE_DEVICE_PROPERTIES] =
      &BluetoothDaemonCoreModule::GetRemoteDevicePropertiesRsp,
    [OPCODE_GET_REMOTE_DEVICE_PROPERTY] =
      &BluetoothDaemonCoreModule::GetRemoteDevicePropertyRsp,
    [OPCODE_SET_REMOTE_DEVICE_PROPERTY] =
      &BluetoothDaemonCoreModule::SetRemoteDevicePropertyRsp,
    [OPCODE_GET_REMOTE_SERVICE_RECORD] =
      &BluetoothDaemonCoreModule::GetRemoteServiceRecordRsp,
    [OPCODE_GET_REMOTE_SERVICES] =
      &BluetoothDaemonCoreModule::GetRemoteServicesRsp,
    [OPCODE_START_DISCOVERY] =
      &BluetoothDaemonCoreModule::StartDiscoveryRsp,
    [OPCODE_CANCEL_DISCOVERY] =
      &BluetoothDaemonCoreModule::CancelDiscoveryRsp,
    [OPCODE_CREATE_BOND] =
      &BluetoothDaemonCoreModule::CreateBondRsp,
    [OPCODE_REMOVE_BOND] =
      &BluetoothDaemonCoreModule::RemoveBondRsp,
    [OPCODE_CANCEL_BOND] =
      &BluetoothDaemonCoreModule::CancelBondRsp,
    [OPCODE_PIN_REPLY] =
      &BluetoothDaemonCoreModule::PinReplyRsp,
    [OPCODE_SSP_REPLY] =
      &BluetoothDaemonCoreModule::SspReplyRsp,
    [OPCODE_DUT_MODE_CONFIGURE] =
      &BluetoothDaemonCoreModule::DutModeConfigureRsp,
    [OPCODE_DUT_MODE_SEND] =
      &BluetoothDaemonCoreModule::DutModeSendRsp,
    [OPCODE_LE_TEST_MODE] =
      &BluetoothDaemonCoreModule::LeTestModeRsp,
  };

  MOZ_ASSERT(!NS_IsMainThread());

  if (NS_WARN_IF(!(aHeader.mOpcode < MOZ_ARRAY_LENGTH(HandleRsp))) ||
      NS_WARN_IF(!HandleRsp[aHeader.mOpcode])) {
    return;
  }

  nsRefPtr<BluetoothResultHandler> res =
    static_cast<BluetoothResultHandler*>(aRes);

  if (!res) {
    return; // Return early if no result handler has been set for response
  }

  (this->*(HandleRsp[aHeader.mOpcode]))(aHeader, aPDU, res);
}

// Notifications
//

class BluetoothDaemonCoreModule::NotificationHandlerWrapper final
{
public:
  typedef BluetoothNotificationHandler ObjectType;

  static ObjectType* GetInstance()
  {
    MOZ_ASSERT(NS_IsMainThread());

    return sNotificationHandler;
  }
};

void
BluetoothDaemonCoreModule::AdapterStateChangedNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  AdapterStateChangedNotification::Dispatch(
    &BluetoothNotificationHandler::AdapterStateChangedNotification,
    UnpackPDUInitOp(aPDU));
}

// Init operator class for AdapterPropertiesNotification
class BluetoothDaemonCoreModule::AdapterPropertiesInitOp final
  : private PDUInitOp
{
public:
  AdapterPropertiesInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (BluetoothStatus& aArg1, int& aArg2,
               nsAutoArrayPtr<BluetoothProperty>& aArg3) const
  {
    DaemonSocketPDU& pdu = GetPDU();

    /* Read status */
    nsresult rv = UnpackPDU(pdu, aArg1);
    if (NS_FAILED(rv)) {
        return rv;
    }

    /* Read number of properties */
    uint8_t numProperties;
    rv = UnpackPDU(pdu, numProperties);
    if (NS_FAILED(rv)) {
      return rv;
    }
    aArg2 = numProperties;

    /* Read properties array */
    UnpackArray<BluetoothProperty> properties(aArg3, aArg2);
    rv = UnpackPDU(pdu, properties);
    if (NS_FAILED(rv)) {
      return rv;
    }
    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonCoreModule::AdapterPropertiesNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  AdapterPropertiesNotification::Dispatch(
    &BluetoothNotificationHandler::AdapterPropertiesNotification,
    AdapterPropertiesInitOp(aPDU));
}

// Init operator class for RemoteDevicePropertiesNotification
class BluetoothDaemonCoreModule::RemoteDevicePropertiesInitOp final
  : private PDUInitOp
{
public:
  RemoteDevicePropertiesInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (BluetoothStatus& aArg1, BluetoothAddress& aArg2, int& aArg3,
               nsAutoArrayPtr<BluetoothProperty>& aArg4) const
  {
    DaemonSocketPDU& pdu = GetPDU();

    /* Read status */
    nsresult rv = UnpackPDU(pdu, aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }

    /* Read address */
    rv = UnpackPDU(pdu, aArg2);
    if (NS_FAILED(rv)) {
      return rv;
    }

    /* Read number of properties */
    uint8_t numProperties;
    rv = UnpackPDU(pdu, numProperties);
    if (NS_FAILED(rv)) {
      return rv;
    }
    aArg3 = numProperties;

    /* Read properties array */
    UnpackArray<BluetoothProperty> properties(aArg4, aArg3);
    rv = UnpackPDU(pdu, properties);
    if (NS_FAILED(rv)) {
      return rv;
    }
    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonCoreModule::RemoteDevicePropertiesNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  RemoteDevicePropertiesNotification::Dispatch(
    &BluetoothNotificationHandler::RemoteDevicePropertiesNotification,
    RemoteDevicePropertiesInitOp(aPDU));
}

// Init operator class for DeviceFoundNotification
class BluetoothDaemonCoreModule::DeviceFoundInitOp final
  : private PDUInitOp
{
public:
  DeviceFoundInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (int& aArg1, nsAutoArrayPtr<BluetoothProperty>& aArg2) const
  {
    DaemonSocketPDU& pdu = GetPDU();

    /* Read number of properties */
    uint8_t numProperties;
    nsresult rv = UnpackPDU(pdu, numProperties);
    if (NS_FAILED(rv)) {
      return rv;
    }
    aArg1 = numProperties;

    /* Read properties array */
    UnpackArray<BluetoothProperty> properties(aArg2, aArg1);
    rv = UnpackPDU(pdu, properties);
    if (NS_FAILED(rv)) {
      return rv;
    }
    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonCoreModule::DeviceFoundNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  DeviceFoundNotification::Dispatch(
    &BluetoothNotificationHandler::DeviceFoundNotification,
    DeviceFoundInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::DiscoveryStateChangedNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  DiscoveryStateChangedNotification::Dispatch(
    &BluetoothNotificationHandler::DiscoveryStateChangedNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::PinRequestNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  PinRequestNotification::Dispatch(
    &BluetoothNotificationHandler::PinRequestNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::SspRequestNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  SspRequestNotification::Dispatch(
    &BluetoothNotificationHandler::SspRequestNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::BondStateChangedNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  BondStateChangedNotification::Dispatch(
    &BluetoothNotificationHandler::BondStateChangedNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::AclStateChangedNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  AclStateChangedNotification::Dispatch(
    &BluetoothNotificationHandler::AclStateChangedNotification,
    UnpackPDUInitOp(aPDU));
}

// Init operator class for DutModeRecvNotification
class BluetoothDaemonCoreModule::DutModeRecvInitOp final
  : private PDUInitOp
{
public:
  DutModeRecvInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (uint16_t& aArg1, nsAutoArrayPtr<uint8_t>& aArg2,
               uint8_t& aArg3) const
  {
    DaemonSocketPDU& pdu = GetPDU();

    /* Read opcode */
    nsresult rv = UnpackPDU(pdu, aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }

    /* Read length */
    rv = UnpackPDU(pdu, aArg3);
    if (NS_FAILED(rv)) {
      return rv;
    }

    /* Read data */
    rv = UnpackPDU(pdu, UnpackArray<uint8_t>(aArg2, aArg3));
    if (NS_FAILED(rv)) {
      return rv;
    }
    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonCoreModule::DutModeRecvNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  DutModeRecvNotification::Dispatch(
    &BluetoothNotificationHandler::DutModeRecvNotification,
    DutModeRecvInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::LeTestModeNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  LeTestModeNotification::Dispatch(
    &BluetoothNotificationHandler::LeTestModeNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonCoreModule::HandleNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  DaemonSocketResultHandler* aRes)
{
  static void (BluetoothDaemonCoreModule::* const HandleNtf[])(
    const DaemonSocketPDUHeader&, DaemonSocketPDU&) = {
    [0] = &BluetoothDaemonCoreModule::AdapterStateChangedNtf,
    [1] = &BluetoothDaemonCoreModule::AdapterPropertiesNtf,
    [2] = &BluetoothDaemonCoreModule::RemoteDevicePropertiesNtf,
    [3] = &BluetoothDaemonCoreModule::DeviceFoundNtf,
    [4] = &BluetoothDaemonCoreModule::DiscoveryStateChangedNtf,
    [5] = &BluetoothDaemonCoreModule::PinRequestNtf,
    [6] = &BluetoothDaemonCoreModule::SspRequestNtf,
    [7] = &BluetoothDaemonCoreModule::BondStateChangedNtf,
    [8] = &BluetoothDaemonCoreModule::AclStateChangedNtf,
    [9] = &BluetoothDaemonCoreModule::DutModeRecvNtf,
    [10] = &BluetoothDaemonCoreModule::LeTestModeNtf
  };

  MOZ_ASSERT(!NS_IsMainThread());

  uint8_t index = aHeader.mOpcode - 0x81;

  if (NS_WARN_IF(!(index < MOZ_ARRAY_LENGTH(HandleNtf))) ||
      NS_WARN_IF(!HandleNtf[index])) {
    return;
  }

    (this->*(HandleNtf[index]))(aHeader, aPDU);
}

END_BLUETOOTH_NAMESPACE
