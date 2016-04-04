/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkSensorsRegistryInterface.h"
#include "GonkSensorsHelpers.h"
#include "HalLog.h"
#include <mozilla/UniquePtr.h>

namespace mozilla {
namespace hal {

using namespace mozilla::ipc;

//
// GonkSensorsRegistryResultHandler
//

void
GonkSensorsRegistryResultHandler::OnError(SensorsError aError)
{
  HAL_ERR("Received error code %d", static_cast<int>(aError));
}

void
GonkSensorsRegistryResultHandler::RegisterModule(uint32_t aProtocolVersion)
{ }

void
GonkSensorsRegistryResultHandler::UnregisterModule()
{ }

GonkSensorsRegistryResultHandler::~GonkSensorsRegistryResultHandler()
{ }

//
// GonkSensorsRegistryModule
//

GonkSensorsRegistryModule::~GonkSensorsRegistryModule()
{ }

void
GonkSensorsRegistryModule::HandleSvc(const DaemonSocketPDUHeader& aHeader,
                                     DaemonSocketPDU& aPDU,
                                     DaemonSocketResultHandler* aRes)
{
  static void (GonkSensorsRegistryModule::* const HandleRsp[])(
    const DaemonSocketPDUHeader&,
    DaemonSocketPDU&,
    GonkSensorsRegistryResultHandler*) = {
    [OPCODE_ERROR] = &GonkSensorsRegistryModule::ErrorRsp,
    [OPCODE_REGISTER_MODULE] = &GonkSensorsRegistryModule::RegisterModuleRsp,
    [OPCODE_UNREGISTER_MODULE] = &GonkSensorsRegistryModule::UnregisterModuleRsp
  };

  if ((aHeader.mOpcode >= MOZ_ARRAY_LENGTH(HandleRsp)) ||
      !HandleRsp[aHeader.mOpcode]) {
    HAL_ERR("Sensors registry response opcode %d unknown", aHeader.mOpcode);
    return;
  }

  RefPtr<GonkSensorsRegistryResultHandler> res =
    static_cast<GonkSensorsRegistryResultHandler*>(aRes);

  if (!res) {
    return; // Return early if no result handler has been set
  }

  (this->*(HandleRsp[aHeader.mOpcode]))(aHeader, aPDU, res);
}

// Commands
//

nsresult
GonkSensorsRegistryModule::RegisterModuleCmd(
  uint8_t aId, GonkSensorsRegistryResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  UniquePtr<DaemonSocketPDU> pdu =
    MakeUnique<DaemonSocketPDU>(SERVICE_ID, OPCODE_REGISTER_MODULE, 0);

  nsresult rv = PackPDU(aId, *pdu);
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
GonkSensorsRegistryModule::UnregisterModuleCmd(
  uint8_t aId, GonkSensorsRegistryResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  UniquePtr<DaemonSocketPDU> pdu =
    MakeUnique<DaemonSocketPDU>(SERVICE_ID, OPCODE_UNREGISTER_MODULE, 0);

  nsresult rv = PackPDU(aId, *pdu);
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
GonkSensorsRegistryModule::ErrorRsp(
  const DaemonSocketPDUHeader& aHeader,
  DaemonSocketPDU& aPDU, GonkSensorsRegistryResultHandler* aRes)
{
  ErrorRunnable::Dispatch(
    aRes, &GonkSensorsRegistryResultHandler::OnError, UnpackPDUInitOp(aPDU));
}

void
GonkSensorsRegistryModule::RegisterModuleRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  GonkSensorsRegistryResultHandler* aRes)
{
  Uint32ResultRunnable::Dispatch(
    aRes,
    &GonkSensorsRegistryResultHandler::RegisterModule,
    UnpackPDUInitOp(aPDU));
}

void
GonkSensorsRegistryModule::UnregisterModuleRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  GonkSensorsRegistryResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes,
    &GonkSensorsRegistryResultHandler::UnregisterModule,
    UnpackPDUInitOp(aPDU));
}

//
// GonkSensorsRegistryInterface
//

GonkSensorsRegistryInterface::GonkSensorsRegistryInterface(
  GonkSensorsRegistryModule* aModule)
  : mModule(aModule)
{ }

GonkSensorsRegistryInterface::~GonkSensorsRegistryInterface()
{ }

void
GonkSensorsRegistryInterface::RegisterModule(
  uint8_t aId, GonkSensorsRegistryResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->RegisterModuleCmd(aId, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
GonkSensorsRegistryInterface::UnregisterModule(
  uint8_t aId, GonkSensorsRegistryResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->UnregisterModuleCmd(aId, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
GonkSensorsRegistryInterface::DispatchError(
  GonkSensorsRegistryResultHandler* aRes, SensorsError aError)
{
  DaemonResultRunnable1<GonkSensorsRegistryResultHandler, void,
                        SensorsError, SensorsError>::Dispatch(
    aRes, &GonkSensorsRegistryResultHandler::OnError,
    ConstantInitOp1<SensorsError>(aError));
}

void
GonkSensorsRegistryInterface::DispatchError(
  GonkSensorsRegistryResultHandler* aRes, nsresult aRv)
{
  SensorsError error;

  if (NS_FAILED(Convert(aRv, error))) {
    error = SENSORS_ERROR_FAIL;
  }
  DispatchError(aRes, error);
}

} // namespace hal
} // namespace mozilla
