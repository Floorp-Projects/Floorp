/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothDaemonSetupInterface.h"
#include "mozilla/unused.h"

BEGIN_BLUETOOTH_NAMESPACE

using namespace mozilla::ipc;

//
// Setup module
//

// Called to handle PDUs with Service field equal to 0x00, which
// contains internal operations for setup and configuration.
void
BluetoothDaemonSetupModule::HandleSvc(const DaemonSocketPDUHeader& aHeader,
                                      DaemonSocketPDU& aPDU,
                                      DaemonSocketResultHandler* aRes)
{
  static void (BluetoothDaemonSetupModule::* const HandleRsp[])(
    const DaemonSocketPDUHeader&,
    DaemonSocketPDU&,
    BluetoothSetupResultHandler*) = {
    [OPCODE_ERROR] =
      &BluetoothDaemonSetupModule::ErrorRsp,
    [OPCODE_REGISTER_MODULE] =
      &BluetoothDaemonSetupModule::RegisterModuleRsp,
    [OPCODE_UNREGISTER_MODULE] =
      &BluetoothDaemonSetupModule::UnregisterModuleRsp,
    [OPCODE_CONFIGURATION] =
      &BluetoothDaemonSetupModule::ConfigurationRsp
  };

  if (NS_WARN_IF(aHeader.mOpcode >= MOZ_ARRAY_LENGTH(HandleRsp)) ||
      NS_WARN_IF(!HandleRsp[aHeader.mOpcode])) {
    return;
  }

  RefPtr<BluetoothSetupResultHandler> res =
    static_cast<BluetoothSetupResultHandler*>(aRes);

  if (!aRes) {
    return; // Return early if no result handler has been set
  }

  (this->*(HandleRsp[aHeader.mOpcode]))(aHeader, aPDU, res);
}

// Commands
//

nsresult
BluetoothDaemonSetupModule::RegisterModuleCmd(
  BluetoothSetupServiceId aId, uint8_t aMode, uint32_t aMaxNumClients,
  BluetoothSetupResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_REGISTER_MODULE,
                        0));

#if ANDROID_VERSION >= 21
  nsresult rv = PackPDU(aId, aMode, aMaxNumClients, *pdu);
#else
  nsresult rv = PackPDU(aId, aMode, *pdu);
#endif
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  Unused << pdu.forget();
  return rv;
}

nsresult
BluetoothDaemonSetupModule::UnregisterModuleCmd(
  BluetoothSetupServiceId aId, BluetoothSetupResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_UNREGISTER_MODULE,
                        0));

  nsresult rv = PackPDU(aId, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  Unused << pdu.forget();
  return rv;
}

nsresult
BluetoothDaemonSetupModule::ConfigurationCmd(
  const BluetoothConfigurationParameter* aParam, uint8_t aLen,
  BluetoothSetupResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CONFIGURATION,
                        0));

  nsresult rv = PackPDU(
    aLen, PackArray<BluetoothConfigurationParameter>(aParam, aLen), *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  Unused << pdu.forget();
  return rv;
}

// Responses
//

void
BluetoothDaemonSetupModule::ErrorRsp(const DaemonSocketPDUHeader& aHeader,
                                     DaemonSocketPDU& aPDU,
                                     BluetoothSetupResultHandler* aRes)
{
  ErrorRunnable::Dispatch(
    aRes, &BluetoothSetupResultHandler::OnError, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonSetupModule::RegisterModuleRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothSetupResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothSetupResultHandler::RegisterModule,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonSetupModule::UnregisterModuleRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothSetupResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothSetupResultHandler::UnregisterModule,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonSetupModule::ConfigurationRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothSetupResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &BluetoothSetupResultHandler::Configuration,
    UnpackPDUInitOp(aPDU));
}

//
// Setup interface
//

BluetoothDaemonSetupInterface::BluetoothDaemonSetupInterface(
  BluetoothDaemonSetupModule* aModule)
  : mModule(aModule)
{ }

BluetoothDaemonSetupInterface::~BluetoothDaemonSetupInterface()
{ }

void
BluetoothDaemonSetupInterface::RegisterModule(
  BluetoothSetupServiceId aId, uint8_t aMode, uint32_t aMaxNumClients,
  BluetoothSetupResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->RegisterModuleCmd(aId, aMode, aMaxNumClients, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonSetupInterface::UnregisterModule(
  BluetoothSetupServiceId aId,
  BluetoothSetupResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->UnregisterModuleCmd(aId, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonSetupInterface::Configuration(
  const BluetoothConfigurationParameter* aParam, uint8_t aLen,
  BluetoothSetupResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ConfigurationCmd(aParam, aLen, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonSetupInterface::DispatchError(
  BluetoothSetupResultHandler* aRes, BluetoothStatus aStatus)
{
  DaemonResultRunnable1<BluetoothSetupResultHandler, void,
                        BluetoothStatus, BluetoothStatus>::Dispatch(
    aRes, &BluetoothSetupResultHandler::OnError,
    ConstantInitOp1<BluetoothStatus>(aStatus));
}

void
BluetoothDaemonSetupInterface::DispatchError(
  BluetoothSetupResultHandler* aRes, nsresult aRv)
{
  BluetoothStatus status;

  if (NS_WARN_IF(NS_FAILED(Convert(aRv, status)))) {
    status = STATUS_FAIL;
  }
  DispatchError(aRes, status);
}

END_BLUETOOTH_NAMESPACE
