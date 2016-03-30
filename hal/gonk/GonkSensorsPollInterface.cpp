/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkSensorsPollInterface.h"
#include "HalLog.h"
#include <mozilla/UniquePtr.h>

namespace mozilla {
namespace hal {

using namespace mozilla::ipc;

//
// GonkSensorsPollResultHandler
//

void
GonkSensorsPollResultHandler::OnError(SensorsError aError)
{
  HAL_ERR("Received error code %d", static_cast<int>(aError));
}

void
GonkSensorsPollResultHandler::EnableSensor()
{ }

void
GonkSensorsPollResultHandler::DisableSensor()
{ }

void
GonkSensorsPollResultHandler::SetPeriod()
{ }

GonkSensorsPollResultHandler::~GonkSensorsPollResultHandler()
{ }

//
// GonkSensorsPollNotificationHandler
//

void
GonkSensorsPollNotificationHandler::ErrorNotification(SensorsError aError)
{
  HAL_ERR("Received error code %d", static_cast<int>(aError));
}

void
GonkSensorsPollNotificationHandler::SensorDetectedNotification(
  int32_t aId,
  SensorsType aType,
  float aRange,
  float aResolution,
  float aPower,
  int32_t aMinPeriod,
  int32_t aMaxPeriod,
  SensorsTriggerMode aTriggerMode,
  SensorsDeliveryMode aDeliveryMode)
{ }

void
GonkSensorsPollNotificationHandler::SensorLostNotification(int32_t aId)
{ }

void
GonkSensorsPollNotificationHandler::EventNotification(int32_t aId,
                                                  const SensorsEvent& aEvent)
{ }

GonkSensorsPollNotificationHandler::~GonkSensorsPollNotificationHandler()
{ }

//
// GonkSensorsPollModule
//

GonkSensorsPollModule::GonkSensorsPollModule()
  : mProtocolVersion(0)
{ }

GonkSensorsPollModule::~GonkSensorsPollModule()
{ }

nsresult
GonkSensorsPollModule::SetProtocolVersion(unsigned long aProtocolVersion)
{
  if ((aProtocolVersion < MIN_PROTOCOL_VERSION) ||
      (aProtocolVersion > MAX_PROTOCOL_VERSION)) {
    HAL_ERR("Sensors Poll protocol version %lu not supported",
            aProtocolVersion);
    return NS_ERROR_ILLEGAL_VALUE;
  }
  mProtocolVersion = aProtocolVersion;
  return NS_OK;
}

void
GonkSensorsPollModule::HandleSvc(const DaemonSocketPDUHeader& aHeader,
                             DaemonSocketPDU& aPDU,
                             DaemonSocketResultHandler* aRes)
{
  static void (GonkSensorsPollModule::* const HandleOp[])(
    const DaemonSocketPDUHeader&, DaemonSocketPDU&,
    DaemonSocketResultHandler*) = {
    [0] = &GonkSensorsPollModule::HandleRsp,
    [1] = &GonkSensorsPollModule::HandleNtf
  };

  MOZ_ASSERT(!NS_IsMainThread()); // I/O thread

  // Negate twice to map bit to 0/1
  unsigned long isNtf = !!(aHeader.mOpcode & 0x80);

  (this->*(HandleOp[isNtf]))(aHeader, aPDU, aRes);
}

// Commands
//

nsresult
GonkSensorsPollModule::EnableSensorCmd(int32_t aId, GonkSensorsPollResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  UniquePtr<DaemonSocketPDU> pdu =
    MakeUnique<DaemonSocketPDU>(SERVICE_ID, OPCODE_ENABLE_SENSOR, 0);

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
GonkSensorsPollModule::DisableSensorCmd(int32_t aId, GonkSensorsPollResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  UniquePtr<DaemonSocketPDU> pdu =
    MakeUnique<DaemonSocketPDU>(SERVICE_ID, OPCODE_DISABLE_SENSOR, 0);

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
GonkSensorsPollModule::SetPeriodCmd(int32_t aId, uint64_t aPeriod,
                                GonkSensorsPollResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  UniquePtr<DaemonSocketPDU> pdu =
    MakeUnique<DaemonSocketPDU>(SERVICE_ID, OPCODE_SET_PERIOD, 0);

  nsresult rv = PackPDU(aId, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = PackPDU(aPeriod, *pdu);
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
GonkSensorsPollModule::ErrorRsp(
  const DaemonSocketPDUHeader& aHeader,
  DaemonSocketPDU& aPDU, GonkSensorsPollResultHandler* aRes)
{
  ErrorRunnable::Dispatch(
    aRes, &GonkSensorsPollResultHandler::OnError, UnpackPDUInitOp(aPDU));
}

void
GonkSensorsPollModule::EnableSensorRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  GonkSensorsPollResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &GonkSensorsPollResultHandler::EnableSensor, UnpackPDUInitOp(aPDU));
}

void
GonkSensorsPollModule::DisableSensorRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  GonkSensorsPollResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &GonkSensorsPollResultHandler::DisableSensor, UnpackPDUInitOp(aPDU));
}

void
GonkSensorsPollModule::SetPeriodRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  GonkSensorsPollResultHandler* aRes)
{
  ResultRunnable::Dispatch(
    aRes, &GonkSensorsPollResultHandler::SetPeriod, UnpackPDUInitOp(aPDU));
}

void
GonkSensorsPollModule::HandleRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  DaemonSocketResultHandler* aRes)
{
  static void (GonkSensorsPollModule::* const sHandleRsp[])(
    const DaemonSocketPDUHeader&, DaemonSocketPDU&,
    GonkSensorsPollResultHandler*) = {
    [OPCODE_ERROR] = &GonkSensorsPollModule::ErrorRsp,
    [OPCODE_ENABLE_SENSOR] = &GonkSensorsPollModule::EnableSensorRsp,
    [OPCODE_DISABLE_SENSOR] = &GonkSensorsPollModule::DisableSensorRsp,
    [OPCODE_SET_PERIOD] = &GonkSensorsPollModule::SetPeriodRsp,
  };

  MOZ_ASSERT(!NS_IsMainThread()); // I/O thread

  if (!(aHeader.mOpcode < MOZ_ARRAY_LENGTH(sHandleRsp)) ||
      !sHandleRsp[aHeader.mOpcode]) {
    HAL_ERR("Sensors poll response opcode %d unknown", aHeader.mOpcode);
    return;
  }

  RefPtr<GonkSensorsPollResultHandler> res =
    static_cast<GonkSensorsPollResultHandler*>(aRes);

  if (!res) {
    return; // Return early if no result handler has been set for response
  }

  (this->*(sHandleRsp[aHeader.mOpcode]))(aHeader, aPDU, res);
}

// Notifications
//

// Returns the current notification handler to a notification runnable
class GonkSensorsPollModule::NotificationHandlerWrapper final
{
public:
  typedef GonkSensorsPollNotificationHandler ObjectType;

  static ObjectType* GetInstance()
  {
    MOZ_ASSERT(NS_IsMainThread());

    return sNotificationHandler;
  }

  static GonkSensorsPollNotificationHandler* sNotificationHandler;
};

GonkSensorsPollNotificationHandler*
  GonkSensorsPollModule::NotificationHandlerWrapper::sNotificationHandler;

void
GonkSensorsPollModule::ErrorNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ErrorNotification::Dispatch(
    &GonkSensorsPollNotificationHandler::ErrorNotification,
    UnpackPDUInitOp(aPDU));
}

void
GonkSensorsPollModule::SensorDetectedNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  SensorDetectedNotification::Dispatch(
    &GonkSensorsPollNotificationHandler::SensorDetectedNotification,
    UnpackPDUInitOp(aPDU));
}

void
GonkSensorsPollModule::SensorLostNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  SensorLostNotification::Dispatch(
    &GonkSensorsPollNotificationHandler::SensorLostNotification,
    UnpackPDUInitOp(aPDU));
}

void
GonkSensorsPollModule::EventNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  EventNotification::Dispatch(
    &GonkSensorsPollNotificationHandler::EventNotification,
    UnpackPDUInitOp(aPDU));
}

void
GonkSensorsPollModule::HandleNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  DaemonSocketResultHandler* aRes)
{
  static void (GonkSensorsPollModule::* const sHandleNtf[])(
    const DaemonSocketPDUHeader&, DaemonSocketPDU&) = {
    [0] = &GonkSensorsPollModule::ErrorNtf,
    [1] = &GonkSensorsPollModule::SensorDetectedNtf,
    [2] = &GonkSensorsPollModule::SensorLostNtf,
    [3] = &GonkSensorsPollModule::EventNtf
  };

  MOZ_ASSERT(!NS_IsMainThread());

  uint8_t index = aHeader.mOpcode - 0x80;

  if (!(index < MOZ_ARRAY_LENGTH(sHandleNtf)) || !sHandleNtf[index]) {
    HAL_ERR("Sensors poll notification opcode %d unknown", aHeader.mOpcode);
    return;
  }

  (this->*(sHandleNtf[index]))(aHeader, aPDU);
}

//
// GonkSensorsPollInterface
//

GonkSensorsPollInterface::GonkSensorsPollInterface(
  GonkSensorsPollModule* aModule)
  : mModule(aModule)
{ }

GonkSensorsPollInterface::~GonkSensorsPollInterface()
{ }

void
GonkSensorsPollInterface::SetNotificationHandler(
  GonkSensorsPollNotificationHandler* aNotificationHandler)
{
  MOZ_ASSERT(NS_IsMainThread());

  GonkSensorsPollModule::NotificationHandlerWrapper::sNotificationHandler =
    aNotificationHandler;
}

nsresult
GonkSensorsPollInterface::SetProtocolVersion(unsigned long aProtocolVersion)
{
  MOZ_ASSERT(mModule);

  return mModule->SetProtocolVersion(aProtocolVersion);
}

void
GonkSensorsPollInterface::EnableSensor(int32_t aId,
                                       GonkSensorsPollResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->EnableSensorCmd(aId, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
GonkSensorsPollInterface::DisableSensor(int32_t aId,
                                        GonkSensorsPollResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->DisableSensorCmd(aId, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
GonkSensorsPollInterface::SetPeriod(int32_t aId, uint64_t aPeriod,
                                    GonkSensorsPollResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->SetPeriodCmd(aId, aPeriod, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
GonkSensorsPollInterface::DispatchError(
  GonkSensorsPollResultHandler* aRes, SensorsError aError)
{
  DaemonResultRunnable1<GonkSensorsPollResultHandler, void,
                        SensorsError, SensorsError>::Dispatch(
    aRes, &GonkSensorsPollResultHandler::OnError,
    ConstantInitOp1<SensorsError>(aError));
}

void
GonkSensorsPollInterface::DispatchError(
  GonkSensorsPollResultHandler* aRes, nsresult aRv)
{
  SensorsError error;

  if (NS_FAILED(Convert(aRv, error))) {
    error = SENSORS_ERROR_FAIL;
  }
  DispatchError(aRes, error);
}

} // namespace hal
} // namespace mozilla
