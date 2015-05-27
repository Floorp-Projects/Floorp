/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothDaemonGattInterface.h"
#include "BluetoothDaemonSetupInterface.h"
#include "mozilla/unused.h"

BEGIN_BLUETOOTH_NAMESPACE

//
// GATT module
//

const int BluetoothDaemonGattModule::MAX_NUM_CLIENTS = 1;

BluetoothGattNotificationHandler*
  BluetoothDaemonGattModule::sNotificationHandler;

void
BluetoothDaemonGattModule::SetNotificationHandler(
  BluetoothGattNotificationHandler* aNotificationHandler)
{
  sNotificationHandler = aNotificationHandler;
}

nsresult
BluetoothDaemonGattModule::Send(BluetoothDaemonPDU* aPDU,
                                BluetoothGattResultHandler* aRes)
{
  if (aRes) {
    aRes->AddRef(); // Keep reference for response
  }
  return Send(aPDU, static_cast<void*>(aRes));
}

nsresult
BluetoothDaemonGattModule::Send(BluetoothDaemonPDU* aPDU,
                                BluetoothGattClientResultHandler* aRes)
{
  if (aRes) {
    aRes->AddRef(); // Keep reference for response
  }
  return Send(aPDU, static_cast<void*>(aRes));
}

void
BluetoothDaemonGattModule::HandleSvc(const BluetoothDaemonPDUHeader& aHeader,
                                     BluetoothDaemonPDU& aPDU, void* aUserData)
{
  static void (BluetoothDaemonGattModule::* const HandleOp[])(
    const BluetoothDaemonPDUHeader&, BluetoothDaemonPDU&, void*) = {
    INIT_ARRAY_AT(0, &BluetoothDaemonGattModule::HandleRsp),
    INIT_ARRAY_AT(1, &BluetoothDaemonGattModule::HandleNtf),
  };

  MOZ_ASSERT(!NS_IsMainThread());

  // Negate twice to map bit to 0/1
  unsigned long isNtf = !!(aHeader.mOpcode & 0x80);

  (this->*(HandleOp[isNtf]))(aHeader, aPDU, aUserData);
}

// Commands
//

nsresult
BluetoothDaemonGattModule::ClientRegisterCmd(
  const BluetoothUuid& aUuid, BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<BluetoothDaemonPDU> pdu(
    new BluetoothDaemonPDU(SERVICE_ID, OPCODE_CLIENT_REGISTER,
                           16)); // Service UUID

  nsresult rv = PackPDU(aUuid, *pdu);

  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return NS_OK;
}

nsresult
BluetoothDaemonGattModule::ClientUnregisterCmd(
  int aClientIf, BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<BluetoothDaemonPDU> pdu(
    new BluetoothDaemonPDU(SERVICE_ID, OPCODE_CLIENT_UNREGISTER,
                           4)); // Client Interface

  nsresult rv = PackPDU(aClientIf, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return NS_OK;
}

nsresult
BluetoothDaemonGattModule::ClientScanCmd(
  int aClientIf, bool aStart, BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<BluetoothDaemonPDU> pdu(
    new BluetoothDaemonPDU(SERVICE_ID, OPCODE_CLIENT_SCAN,
                           4 + // Client Interface
                           1)); // Start

  nsresult rv = PackPDU(PackConversion<int, int32_t>(aClientIf),
                        PackConversion<bool, uint8_t>(aStart), *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return NS_OK;
}

nsresult
BluetoothDaemonGattModule::ClientConnectCmd(
  int aClientIf, const nsAString& aBdAddr, bool aIsDirect,
  BluetoothTransport aTransport, BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<BluetoothDaemonPDU> pdu(
    new BluetoothDaemonPDU(SERVICE_ID, OPCODE_CLIENT_CONNECT,
                           4 + // Client Interface
                           6 + // Remote Address
                           1 + // Is Direct
                           4)); // Transport

  nsresult rv = PackPDU(
    PackConversion<int, int32_t>(aClientIf),
    PackConversion<nsAString, BluetoothAddress>(aBdAddr),
    PackConversion<bool, uint8_t>(aIsDirect),
    PackConversion<BluetoothTransport, int32_t>(aTransport), *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return NS_OK;
}

nsresult
BluetoothDaemonGattModule::ClientDisconnectCmd(
  int aClientIf, const nsAString& aBdAddr, int aConnId,
  BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<BluetoothDaemonPDU> pdu(
    new BluetoothDaemonPDU(SERVICE_ID, OPCODE_CLIENT_DISCONNECT,
                           4 + // Client Interface
                           6 + // Remote Address
                           4)); // Connection ID

  nsresult rv;
  rv = PackPDU(PackConversion<int, int32_t>(aClientIf),
               PackConversion<nsAString, BluetoothAddress>(aBdAddr),
               PackConversion<int, int32_t>(aConnId), *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return NS_OK;
}

nsresult
BluetoothDaemonGattModule::ClientListenCmd(
  int aClientIf, bool aIsStart, BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<BluetoothDaemonPDU> pdu(
    new BluetoothDaemonPDU(SERVICE_ID, OPCODE_CLIENT_LISTEN,
                           4 + // Client Interface
                           1)); // Start

  nsresult rv;
  rv = PackPDU(PackConversion<int, int32_t>(aClientIf),
               PackConversion<bool, uint8_t>(aIsStart), *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return NS_OK;
}

nsresult
BluetoothDaemonGattModule::ClientRefreshCmd(
  int aClientIf, const nsAString& aBdAddr,
  BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<BluetoothDaemonPDU> pdu(
    new BluetoothDaemonPDU(SERVICE_ID, OPCODE_CLIENT_REFRESH,
                           4 + // Client Interface
                           6)); // Remote Address

  nsresult rv = PackPDU(PackConversion<int, int32_t>(aClientIf),
                        PackConversion<nsAString, BluetoothAddress>(aBdAddr),
                        *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return NS_OK;
}

nsresult
BluetoothDaemonGattModule::ClientSearchServiceCmd(
  int aConnId, bool aFiltered, const BluetoothUuid& aUuid,
  BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<BluetoothDaemonPDU> pdu(
    new BluetoothDaemonPDU(SERVICE_ID, OPCODE_CLIENT_SEARCH_SERVICE,
                           4 + // Connection ID
                           1 + // Filtered
                           16)); // UUID

  nsresult rv = PackPDU(PackConversion<int, int32_t>(aConnId),
                        PackConversion<bool, uint8_t>(aFiltered), aUuid, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return NS_OK;
}

nsresult
BluetoothDaemonGattModule::ClientGetIncludedServiceCmd(
  int aConnId, const BluetoothGattServiceId& aServiceId, bool aContinuation,
  const BluetoothGattServiceId& aStartServiceId,
  BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<BluetoothDaemonPDU> pdu(
    new BluetoothDaemonPDU(SERVICE_ID, OPCODE_CLIENT_GET_INCLUDED_SERVICE,
                           4 + // Connection ID
                           18 + // Service ID
                           1 + // Continuation
                           18)); // Start Service ID

  nsresult rv = PackPDU(PackConversion<int, int32_t>(aConnId), aServiceId,
                        PackConversion<bool, uint8_t>(aContinuation),
                        aStartServiceId, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return NS_OK;
}

nsresult
BluetoothDaemonGattModule::ClientGetCharacteristicCmd(
  int aConnId, const BluetoothGattServiceId& aServiceId, bool aContinuation,
  const BluetoothGattId& aStartCharId,
  BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<BluetoothDaemonPDU> pdu(
    new BluetoothDaemonPDU(SERVICE_ID, OPCODE_CLIENT_GET_CHARACTERISTIC,
                           4 + // Connection ID
                           18 + // Service ID
                           1 + // Continuation
                           17)); // Start Characteristic ID

  nsresult rv = PackPDU(PackConversion<int, int32_t>(aConnId), aServiceId,
                        PackConversion<bool, uint8_t>(aContinuation),
                        aStartCharId, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return NS_OK;
}

nsresult
BluetoothDaemonGattModule::ClientGetDescriptorCmd(
  int aConnId, const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharId, bool aContinuation,
  const BluetoothGattId& aStartDescriptorId,
  BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<BluetoothDaemonPDU> pdu(
    new BluetoothDaemonPDU(SERVICE_ID, OPCODE_CLIENT_GET_DESCRIPTOR,
                           4 + // Connection ID
                           18 + // Service ID
                           17 + // Characteristic ID
                           1 + // Continuation
                           17)); // Start Descriptor ID

  nsresult rv = PackPDU(PackConversion<int, int32_t>(aConnId),
                        aServiceId, aCharId,
                        PackConversion<bool, uint8_t>(aContinuation),
                        aStartDescriptorId, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return NS_OK;
}

nsresult
BluetoothDaemonGattModule::ClientReadCharacteristicCmd(
  int aConnId, const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharId, BluetoothGattAuthReq aAuthReq,
  BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<BluetoothDaemonPDU> pdu(
    new BluetoothDaemonPDU(SERVICE_ID, OPCODE_CLIENT_READ_CHARACTERISTIC,
                           4 + // Connection ID
                           18 + // Service ID
                           17 + // Characteristic ID
                           4)); // Authorization

  nsresult rv = PackPDU(PackConversion<int, int32_t>(aConnId), aServiceId,
                        aCharId, aAuthReq, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return NS_OK;
}

nsresult
BluetoothDaemonGattModule::ClientWriteCharacteristicCmd(
  int aConnId, const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharId, BluetoothGattWriteType aWriteType,
  int aLength, BluetoothGattAuthReq aAuthReq, char* aValue,
  BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<BluetoothDaemonPDU> pdu(
    new BluetoothDaemonPDU(SERVICE_ID, OPCODE_CLIENT_WRITE_CHARACTERISTIC, 0));

  nsresult rv = PackPDU(PackConversion<int, int32_t>(aConnId), aServiceId,
                        aCharId, aWriteType,
                        PackConversion<int, int32_t>(aLength), aAuthReq,
                        PackArray<char>(aValue, aLength), *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return NS_OK;
}

nsresult
BluetoothDaemonGattModule::ClientReadDescriptorCmd(
  int aConnId, const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharId, const BluetoothGattId& aDescriptorId,
  BluetoothGattAuthReq aAuthReq, BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<BluetoothDaemonPDU> pdu(
    new BluetoothDaemonPDU(SERVICE_ID, OPCODE_CLIENT_READ_DESCRIPTOR,
                           4 + // Connection ID
                           18 + // Service ID
                           17 + // Characteristic ID
                           17 + // Descriptor ID
                           4)); // Authorization

  nsresult rv = PackPDU(PackConversion<int, int32_t>(aConnId), aServiceId,
                        aCharId, aDescriptorId, aAuthReq, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return NS_OK;
}

nsresult
BluetoothDaemonGattModule::ClientWriteDescriptorCmd(
  int aConnId, const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharId, const BluetoothGattId& aDescriptorId,
  BluetoothGattWriteType aWriteType, int aLength,
  BluetoothGattAuthReq aAuthReq, char* aValue,
  BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<BluetoothDaemonPDU> pdu(
    new BluetoothDaemonPDU(SERVICE_ID, OPCODE_CLIENT_WRITE_DESCRIPTOR, 0));

  nsresult rv = PackPDU(PackConversion<int, int32_t>(aConnId), aServiceId,
                        aCharId, aDescriptorId, aWriteType,
                        PackConversion<int, int32_t>(aLength), aAuthReq,
                        PackArray<char>(aValue, aLength), *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return NS_OK;
}

nsresult
BluetoothDaemonGattModule::ClientExecuteWriteCmd(
  int aConnId, int aIsExecute, BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<BluetoothDaemonPDU> pdu(
    new BluetoothDaemonPDU(SERVICE_ID, OPCODE_CLIENT_EXECUTE_WRITE,
                           4 + // Connection ID
                           4)); // Execute

  nsresult rv = PackPDU(PackConversion<int, int32_t>(aConnId),
                        PackConversion<int, int32_t>(aIsExecute), *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return NS_OK;
}

nsresult
BluetoothDaemonGattModule::ClientRegisterNotificationCmd(
  int aClientIf, const nsAString& aBdAddr,
  const BluetoothGattServiceId& aServiceId, const BluetoothGattId& aCharId,
  BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<BluetoothDaemonPDU> pdu(
    new BluetoothDaemonPDU(SERVICE_ID, OPCODE_CLIENT_REGISTER_NOTIFICATION,
                           4 + // Client Interface
                           6 + // Remote Address
                           18 + // Service ID
                           17)); // Characteristic ID

  nsresult rv = PackPDU(PackConversion<int, int32_t>(aClientIf),
                        PackConversion<nsAString, BluetoothAddress>(aBdAddr),
                        aServiceId, aCharId, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return NS_OK;
}

nsresult
BluetoothDaemonGattModule::ClientDeregisterNotificationCmd(
  int aClientIf, const nsAString& aBdAddr,
  const BluetoothGattServiceId& aServiceId, const BluetoothGattId& aCharId,
  BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<BluetoothDaemonPDU> pdu(
    new BluetoothDaemonPDU(SERVICE_ID, OPCODE_CLIENT_DEREGISTER_NOTIFICATION,
                           4 + // Client Interface
                           6 + // Remote Address
                           18 + // Service ID
                           17)); // Characteristic ID

  nsresult rv = PackPDU(PackConversion<int, int32_t>(aClientIf),
                        PackConversion<nsAString, BluetoothAddress>(aBdAddr),
                        aServiceId, aCharId, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return NS_OK;
}

nsresult
BluetoothDaemonGattModule::ClientReadRemoteRssiCmd(
  int aClientIf, const nsAString& aBdAddr,
  BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<BluetoothDaemonPDU> pdu(
    new BluetoothDaemonPDU(SERVICE_ID, OPCODE_CLIENT_READ_REMOTE_RSSI,
                           4 + // Client Interface
                           6)); // Remote Address

  nsresult rv = PackPDU(PackConversion<int, int32_t>(aClientIf),
                        PackConversion<nsAString, BluetoothAddress>(aBdAddr),
                        *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return NS_OK;
}

nsresult
BluetoothDaemonGattModule::ClientGetDeviceTypeCmd(
  const nsAString& aBdAddr, BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<BluetoothDaemonPDU> pdu(
    new BluetoothDaemonPDU(SERVICE_ID, OPCODE_CLIENT_GET_DEVICE_TYPE,
                           6)); // Remote Address

  nsresult rv = PackPDU(
    PackConversion<nsAString, BluetoothAddress>(aBdAddr), *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return NS_OK;
}

nsresult
BluetoothDaemonGattModule::ClientSetAdvDataCmd(
  int aServerIf, bool aIsScanRsp, bool aIsNameIncluded,
  bool aIsTxPowerIncluded, int aMinInterval, int aMaxInterval, int aApperance,
  uint16_t aManufacturerLen, char* aManufacturerData,
  uint16_t aServiceDataLen, char* aServiceData,
  uint16_t aServiceUuidLen, char* aServiceUuid,
  BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<BluetoothDaemonPDU> pdu(
    new BluetoothDaemonPDU(SERVICE_ID, OPCODE_CLIENT_SET_ADV_DATA, 0));

  nsresult rv = PackPDU(
    PackConversion<int, int32_t>(aServerIf),
    PackConversion<bool, uint8_t>(aIsScanRsp),
    PackConversion<bool, uint8_t>(aIsNameIncluded),
    PackConversion<bool, uint8_t>(aIsTxPowerIncluded),
    PackConversion<int, int32_t>(aMinInterval),
    PackConversion<int, int32_t>(aMaxInterval),
    PackConversion<int, int32_t>(aApperance),
    aManufacturerLen, PackArray<char>(aManufacturerData, aManufacturerLen),
    aServiceDataLen, PackArray<char>(aServiceData, aServiceDataLen),
    aServiceUuidLen, PackArray<char>(aServiceUuid, aServiceUuidLen), *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return NS_OK;
}

nsresult
BluetoothDaemonGattModule::ClientTestCommandCmd(
  int aCommand, const BluetoothGattTestParam& aTestParam,
  BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<BluetoothDaemonPDU> pdu(
    new BluetoothDaemonPDU(SERVICE_ID, OPCODE_CLIENT_TEST_COMMAND,
                           4 + // Command
                           6 + // Address
                           16 + // UUID
                           2 + // U1
                           2 + // U2
                           2 + // U3
                           2 + // U4
                           2)); // U5

  nsresult rv = PackPDU(
    PackConversion<int, int32_t>(aCommand),
    PackConversion<nsAString, BluetoothAddress>(aTestParam.mBdAddr),
    aTestParam.mU1, aTestParam.mU2, aTestParam.mU3, aTestParam.mU4,
    aTestParam.mU5, *pdu);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = Send(pdu, aRes);
  if (NS_FAILED(rv)) {
    return rv;
  }
  unused << pdu.forget();
  return NS_OK;
}

// Responses
//

void
BluetoothDaemonGattModule::ErrorRsp(
  const BluetoothDaemonPDUHeader& aHeader,
  BluetoothDaemonPDU& aPDU, BluetoothGattResultHandler* aRes)
{
  ErrorRunnable::Dispatch(
    aRes, &BluetoothGattResultHandler::OnError, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientRegisterRsp(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::RegisterClient,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientUnregisterRsp(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::UnregisterClient,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientScanRsp(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::Scan, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientConnectRsp(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::Connect, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientDisconnectRsp(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::Disconnect,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientListenRsp(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::Listen, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientRefreshRsp(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::Refresh, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientSearchServiceRsp(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::SearchService,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientGetIncludedServiceRsp(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::GetIncludedService,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientGetCharacteristicRsp(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::GetCharacteristic,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientGetDescriptorRsp(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::GetDescriptor,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientReadCharacteristicRsp(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::ReadCharacteristic,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientWriteCharacteristicRsp(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::WriteCharacteristic,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientReadDescriptorRsp(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::ReadDescriptor,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientWriteDescriptorRsp(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::WriteDescriptor,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientExecuteWriteRsp(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::ExecuteWrite,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientRegisterNotificationRsp(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::RegisterNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientDeregisterNotificationRsp(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::DeregisterNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientReadRemoteRssiRsp(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::ReadRemoteRssi,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientGetDeviceTypeRsp(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::GetDeviceType,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientSetAdvDataRsp(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::SetAdvData,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientTestCommandRsp(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::TestCommand,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::HandleRsp(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  void* aUserData)
{
  static void (BluetoothDaemonGattModule::* const HandleRsp[])(
    const BluetoothDaemonPDUHeader&,
    BluetoothDaemonPDU&,
    BluetoothGattResultHandler*) = {
    INIT_ARRAY_AT(OPCODE_ERROR,
      &BluetoothDaemonGattModule::ErrorRsp)
    };

  static void (BluetoothDaemonGattModule::* const HandleClientRsp[])(
    const BluetoothDaemonPDUHeader&,
    BluetoothDaemonPDU&,
    BluetoothGattClientResultHandler*) = {
    INIT_ARRAY_AT(0, nullptr),
    INIT_ARRAY_AT(OPCODE_CLIENT_REGISTER,
      &BluetoothDaemonGattModule::ClientRegisterRsp),
    INIT_ARRAY_AT(OPCODE_CLIENT_UNREGISTER,
      &BluetoothDaemonGattModule::ClientUnregisterRsp),
    INIT_ARRAY_AT(OPCODE_CLIENT_SCAN,
      &BluetoothDaemonGattModule::ClientScanRsp),
    INIT_ARRAY_AT(OPCODE_CLIENT_CONNECT,
      &BluetoothDaemonGattModule::ClientConnectRsp),
    INIT_ARRAY_AT(OPCODE_CLIENT_DISCONNECT,
      &BluetoothDaemonGattModule::ClientDisconnectRsp),
    INIT_ARRAY_AT(OPCODE_CLIENT_LISTEN,
      &BluetoothDaemonGattModule::ClientListenRsp),
    INIT_ARRAY_AT(OPCODE_CLIENT_REFRESH,
      &BluetoothDaemonGattModule::ClientRefreshRsp),
    INIT_ARRAY_AT(OPCODE_CLIENT_SEARCH_SERVICE,
      &BluetoothDaemonGattModule::ClientSearchServiceRsp),
    INIT_ARRAY_AT(OPCODE_CLIENT_GET_INCLUDED_SERVICE,
      &BluetoothDaemonGattModule::ClientGetIncludedServiceRsp),
    INIT_ARRAY_AT(OPCODE_CLIENT_GET_CHARACTERISTIC,
      &BluetoothDaemonGattModule::ClientGetCharacteristicRsp),
    INIT_ARRAY_AT(OPCODE_CLIENT_GET_DESCRIPTOR,
      &BluetoothDaemonGattModule::ClientGetDescriptorRsp),
    INIT_ARRAY_AT(OPCODE_CLIENT_READ_CHARACTERISTIC,
      &BluetoothDaemonGattModule::ClientReadCharacteristicRsp),
    INIT_ARRAY_AT(OPCODE_CLIENT_WRITE_CHARACTERISTIC,
      &BluetoothDaemonGattModule::ClientWriteCharacteristicRsp),
    INIT_ARRAY_AT(OPCODE_CLIENT_READ_DESCRIPTOR,
      &BluetoothDaemonGattModule::ClientReadDescriptorRsp),
    INIT_ARRAY_AT(OPCODE_CLIENT_WRITE_DESCRIPTOR,
      &BluetoothDaemonGattModule::ClientWriteDescriptorRsp),
    INIT_ARRAY_AT(OPCODE_CLIENT_EXECUTE_WRITE,
      &BluetoothDaemonGattModule::ClientExecuteWriteRsp),
    INIT_ARRAY_AT(OPCODE_CLIENT_REGISTER_NOTIFICATION,
      &BluetoothDaemonGattModule::ClientRegisterNotificationRsp),
    INIT_ARRAY_AT(OPCODE_CLIENT_DEREGISTER_NOTIFICATION,
      &BluetoothDaemonGattModule::ClientDeregisterNotificationRsp),
    INIT_ARRAY_AT(OPCODE_CLIENT_READ_REMOTE_RSSI,
      &BluetoothDaemonGattModule::ClientReadRemoteRssiRsp),
    INIT_ARRAY_AT(OPCODE_CLIENT_GET_DEVICE_TYPE,
      &BluetoothDaemonGattModule::ClientGetDeviceTypeRsp),
    INIT_ARRAY_AT(OPCODE_CLIENT_SET_ADV_DATA,
      &BluetoothDaemonGattModule::ClientSetAdvDataRsp),
    INIT_ARRAY_AT(OPCODE_CLIENT_TEST_COMMAND,
      &BluetoothDaemonGattModule::ClientTestCommandRsp)
  };

  MOZ_ASSERT(!NS_IsMainThread()); // I/O thread

  size_t length = MOZ_ARRAY_LENGTH(HandleRsp) +
                  MOZ_ARRAY_LENGTH(HandleClientRsp);
  bool isInGattArray = HandleRsp[aHeader.mOpcode];
  bool isInGattClientArray = HandleClientRsp[aHeader.mOpcode];

  if (NS_WARN_IF(!(aHeader.mOpcode < length)) ||
      NS_WARN_IF(!(isInGattArray || isInGattClientArray))) {
    return;
  }

  if (!aUserData) {
    return; // Return early if no result handler has been set for response
  }

  if (isInGattArray) {
    nsRefPtr<BluetoothGattResultHandler> res =
      already_AddRefed<BluetoothGattResultHandler>(
        static_cast<BluetoothGattResultHandler*>(aUserData));
    (this->*(HandleRsp[aHeader.mOpcode]))(aHeader, aPDU, res);
  } else {
    nsRefPtr<BluetoothGattClientResultHandler> res =
      already_AddRefed<BluetoothGattClientResultHandler>(
        static_cast<BluetoothGattClientResultHandler*>(aUserData));
    (this->*(HandleClientRsp[aHeader.mOpcode]))(aHeader, aPDU, res);
  }

}

// Notifications
//

// Returns the current notification handler to a notification runnable
class BluetoothDaemonGattModule::NotificationHandlerWrapper final
{
public:
  typedef BluetoothGattNotificationHandler ObjectType;

  static ObjectType* GetInstance()
  {
    MOZ_ASSERT(NS_IsMainThread());

    return sNotificationHandler;
  }
};

// Returns the current notification handler to a notification runnable
class BluetoothDaemonGattModule::ClientNotificationHandlerWrapper final
{
public:
  typedef BluetoothGattClientNotificationHandler ObjectType;

  static ObjectType* GetInstance()
  {
    MOZ_ASSERT(NS_IsMainThread());

    return sNotificationHandler;
  }
};

void
BluetoothDaemonGattModule::ClientRegisterNtf(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU)
{
  ClientRegisterNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::RegisterClientNotification,
    UnpackPDUInitOp(aPDU));
}

// Init operator class for ClientScanResultNotification
class BluetoothDaemonGattModule::ClientScanResultInitOp final
  : private PDUInitOp
{
public:
  ClientScanResultInitOp(BluetoothDaemonPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (nsString& aArg1,
               int& aArg2,
               BluetoothGattAdvData& aArg3) const
  {
    BluetoothDaemonPDU& pdu = GetPDU();

    /* Read address */
    nsresult rv = UnpackPDU(
      pdu, UnpackConversion<BluetoothAddress, nsAString>(aArg1));
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read RSSI */
    rv = UnpackPDU(pdu, aArg2);
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read Length */
    size_t length;
    rv = UnpackPDU(pdu, length);
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read Adv Data */
    rv = UnpackPDU(pdu, aArg3);
    if (NS_FAILED(rv)) {
      return rv;
    }

    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonGattModule::ClientScanResultNtf(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU)
{
  ClientScanResultNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::ScanResultNotification,
    ClientScanResultInitOp(aPDU));
}

// Init operator class for ClientConnect/DisconnectNotification
class BluetoothDaemonGattModule::ClientConnectDisconnectInitOp final
  : private PDUInitOp
{
public:
  ClientConnectDisconnectInitOp(BluetoothDaemonPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (int& aArg1,
               BluetoothGattStatus& aArg2,
               int& aArg3,
               nsString& aArg4) const
  {
    BluetoothDaemonPDU& pdu = GetPDU();

    /* Read connection ID */
    nsresult rv = UnpackPDU(pdu, aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read status */
    rv = UnpackPDU(pdu, aArg2);
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read client interface */
    rv = UnpackPDU(pdu, aArg3);
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read address */
    rv = UnpackPDU(
      pdu, UnpackConversion<BluetoothAddress, nsAString>(aArg4));
    if (NS_FAILED(rv)) {
      return rv;
    }

    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonGattModule::ClientConnectNtf(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU)
{
  ClientConnectNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::ConnectNotification,
    ClientConnectDisconnectInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientDisconnectNtf(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU)
{
  ClientDisconnectNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::DisconnectNotification,
    ClientConnectDisconnectInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientSearchCompleteNtf(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU)
{
  ClientSearchCompleteNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::SearchCompleteNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientSearchResultNtf(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU)
{
  ClientSearchResultNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::SearchResultNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientGetCharacteristicNtf(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU)
{
  ClientGetCharacteristicNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::GetCharacteristicNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientGetDescriptorNtf(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU)
{
  ClientGetDescriptorNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::GetDescriptorNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientGetIncludedServiceNtf(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU)
{
  ClientGetIncludedServiceNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::GetIncludedServiceNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientRegisterNotificationNtf(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU)
{
  ClientRegisterNotificationNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::RegisterNotificationNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientNotifyNtf(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU)
{
  ClientNotifyNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::NotifyNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientReadCharacteristicNtf(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU)
{
  ClientReadCharacteristicNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::ReadCharacteristicNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientWriteCharacteristicNtf(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU)
{
  ClientWriteCharacteristicNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::WriteCharacteristicNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientReadDescriptorNtf(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU)
{
  ClientReadDescriptorNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::ReadDescriptorNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientWriteDescriptorNtf(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU)
{
  ClientWriteDescriptorNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::WriteDescriptorNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientExecuteWriteNtf(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU)
{
  ClientExecuteWriteNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::ExecuteWriteNotification,
    UnpackPDUInitOp(aPDU));
}

// Init operator class for ClientReadRemoteRssiNotification
class BluetoothDaemonGattModule::ClientReadRemoteRssiInitOp final
  : private PDUInitOp
{
public:
  ClientReadRemoteRssiInitOp(BluetoothDaemonPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (int& aArg1,
               nsString& aArg2,
               int& aArg3,
               BluetoothGattStatus& aArg4) const
  {
    BluetoothDaemonPDU& pdu = GetPDU();

    /* Read client interface */
    nsresult rv = UnpackPDU(pdu, aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read address */
    rv = UnpackPDU(pdu, UnpackConversion<BluetoothAddress, nsAString>(aArg2));
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read RSSI */
    rv = UnpackPDU(pdu, aArg3);
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read status */
    rv = UnpackPDU(pdu, aArg4);
    if (NS_FAILED(rv)) {
      return rv;
    }

    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonGattModule::ClientReadRemoteRssiNtf(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU)
{
  ClientReadRemoteRssiNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::ReadRemoteRssiNotification,
    ClientReadRemoteRssiInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientListenNtf(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU)
{
  ClientListenNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::ListenNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::HandleNtf(
  const BluetoothDaemonPDUHeader& aHeader, BluetoothDaemonPDU& aPDU,
  void* aUserData)
{
  static void (BluetoothDaemonGattModule::* const HandleNtf[])(
    const BluetoothDaemonPDUHeader&, BluetoothDaemonPDU&) = {
    INIT_ARRAY_AT(0, &BluetoothDaemonGattModule::ClientRegisterNtf),
    INIT_ARRAY_AT(1, &BluetoothDaemonGattModule::ClientScanResultNtf),
    INIT_ARRAY_AT(2, &BluetoothDaemonGattModule::ClientConnectNtf),
    INIT_ARRAY_AT(3, &BluetoothDaemonGattModule::ClientDisconnectNtf),
    INIT_ARRAY_AT(4, &BluetoothDaemonGattModule::ClientSearchCompleteNtf),
    INIT_ARRAY_AT(5, &BluetoothDaemonGattModule::ClientSearchResultNtf),
    INIT_ARRAY_AT(6, &BluetoothDaemonGattModule::ClientGetCharacteristicNtf),
    INIT_ARRAY_AT(7, &BluetoothDaemonGattModule::ClientGetDescriptorNtf),
    INIT_ARRAY_AT(8, &BluetoothDaemonGattModule::ClientGetIncludedServiceNtf),
    INIT_ARRAY_AT(9, &BluetoothDaemonGattModule::ClientRegisterNotificationNtf),
    INIT_ARRAY_AT(10, &BluetoothDaemonGattModule::ClientNotifyNtf),
    INIT_ARRAY_AT(11, &BluetoothDaemonGattModule::ClientReadCharacteristicNtf),
    INIT_ARRAY_AT(12, &BluetoothDaemonGattModule::ClientWriteCharacteristicNtf),
    INIT_ARRAY_AT(13, &BluetoothDaemonGattModule::ClientReadDescriptorNtf),
    INIT_ARRAY_AT(14, &BluetoothDaemonGattModule::ClientWriteDescriptorNtf),
    INIT_ARRAY_AT(15, &BluetoothDaemonGattModule::ClientExecuteWriteNtf),
    INIT_ARRAY_AT(16, &BluetoothDaemonGattModule::ClientReadRemoteRssiNtf),
    INIT_ARRAY_AT(17, &BluetoothDaemonGattModule::ClientListenNtf)
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
