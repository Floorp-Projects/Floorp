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
BluetoothDaemonGattModule::Send(DaemonSocketPDU* aPDU,
                                BluetoothGattResultHandler* aRes)
{
  if (aRes) {
    aRes->AddRef(); // Keep reference for response
  }
  return Send(aPDU, static_cast<void*>(aRes));
}

nsresult
BluetoothDaemonGattModule::Send(DaemonSocketPDU* aPDU,
                                BluetoothGattClientResultHandler* aRes)
{
  if (aRes) {
    aRes->AddRef(); // Keep reference for response
  }
  return Send(aPDU, static_cast<void*>(aRes));
}

nsresult
BluetoothDaemonGattModule::Send(DaemonSocketPDU* aPDU,
                                BluetoothGattServerResultHandler* aRes)
{
  aRes->AddRef(); // Keep reference for response
  return Send(aPDU, static_cast<void*>(aRes));
}

void
BluetoothDaemonGattModule::HandleSvc(const DaemonSocketPDUHeader& aHeader,
                                     DaemonSocketPDU& aPDU, void* aUserData)
{
  static void (BluetoothDaemonGattModule::* const HandleOp[])(
    const DaemonSocketPDUHeader&, DaemonSocketPDU&, void*) = {
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

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CLIENT_REGISTER,
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

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CLIENT_UNREGISTER,
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

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CLIENT_SCAN,
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

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CLIENT_CONNECT,
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

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CLIENT_DISCONNECT,
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

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CLIENT_LISTEN,
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

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CLIENT_REFRESH,
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

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CLIENT_SEARCH_SERVICE,
                        4 + // Connection ID
                        1 + // Filtered
                        16)); // UUID

  nsresult rv = PackPDU(PackConversion<int, int32_t>(aConnId),
                        PackConversion<bool, uint8_t>(aFiltered),
                        PackReversed<BluetoothUuid>(aUuid),
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
BluetoothDaemonGattModule::ClientGetIncludedServiceCmd(
  int aConnId, const BluetoothGattServiceId& aServiceId, bool aContinuation,
  const BluetoothGattServiceId& aStartServiceId,
  BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CLIENT_GET_INCLUDED_SERVICE,
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

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CLIENT_GET_CHARACTERISTIC,
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

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CLIENT_GET_DESCRIPTOR,
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

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CLIENT_READ_CHARACTERISTIC,
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

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CLIENT_WRITE_CHARACTERISTIC, 0));

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

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CLIENT_READ_DESCRIPTOR,
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

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CLIENT_WRITE_DESCRIPTOR, 0));

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

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CLIENT_EXECUTE_WRITE,
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

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CLIENT_REGISTER_NOTIFICATION,
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

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CLIENT_DEREGISTER_NOTIFICATION,
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

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CLIENT_READ_REMOTE_RSSI,
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

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CLIENT_GET_DEVICE_TYPE,
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

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CLIENT_SET_ADV_DATA, 0));

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

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_CLIENT_TEST_COMMAND,
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

nsresult
BluetoothDaemonGattModule::ServerRegisterCmd(
  const BluetoothUuid& aUuid, BluetoothGattServerResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_SERVER_REGISTER,
                        16)); // Uuid

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
BluetoothDaemonGattModule::ServerUnregisterCmd(
  int aServerIf, BluetoothGattServerResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_SERVER_UNREGISTER,
                        4)); // Server Interface

  nsresult rv = PackPDU(PackConversion<int, int32_t>(aServerIf), *pdu);
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
BluetoothDaemonGattModule::ServerConnectPeripheralCmd(
  int aServerIf, const nsAString& aBdAddr, bool aIsDirect,
  BluetoothTransport aTransport, BluetoothGattServerResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_SERVER_CONNECT_PERIPHERAL,
                        4 + // Server Interface
                        6 + // Remote Address
                        1 + // Is Direct
                        4)); // Transport

  nsresult rv = PackPDU(
    PackConversion<int, int32_t>(aServerIf),
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
BluetoothDaemonGattModule::ServerDisconnectPeripheralCmd(
  int aServerIf, const nsAString& aBdAddr, int aConnId,
  BluetoothGattServerResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_SERVER_DISCONNECT_PERIPHERAL,
                        4 + // Server Interface
                        6 + // Remote Address
                        4)); // Connection Id

  nsresult rv = PackPDU(PackConversion<int, int32_t>(aServerIf),
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
BluetoothDaemonGattModule::ServerAddServiceCmd(
  int aServerIf, const BluetoothGattServiceId& aServiceId, int aNumHandles,
  BluetoothGattServerResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_SERVER_ADD_SERVICE,
                        4 + // Server Interface
                        18 + // Service ID
                        4)); // Number of Handles

  nsresult rv = PackPDU(PackConversion<int, int32_t>(aServerIf), aServiceId,
                        PackConversion<int, int32_t>(aNumHandles), *pdu);
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
BluetoothDaemonGattModule::ServerAddIncludedServiceCmd(
  int aServerIf, int aServiceHandle, int aIncludedServiceHandle,
  BluetoothGattServerResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_SERVER_ADD_INCLUDED_SERVICE,
                        4 + // Server Interface
                        4 + // Service Handle
                        4)); // Included Service Handle

  nsresult rv = PackPDU(PackConversion<int, int32_t>(aServerIf),
                        PackConversion<int, int32_t>(aServiceHandle),
                        PackConversion<int, int32_t>(aIncludedServiceHandle),
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
BluetoothDaemonGattModule::ServerAddCharacteristicCmd(
  int aServerIf, int aServiceHandle, const BluetoothUuid& aUuid,
  BluetoothGattCharProp aProperties, BluetoothGattAttrPerm aPermissions,
  BluetoothGattServerResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_SERVER_ADD_CHARACTERISTIC,
                        4 + // Server Interface
                        4 + // Service Handle
                        16 + // UUID
                        4 + // Properties
                        4)); // Permissions

  nsresult rv = PackPDU(PackConversion<int, int32_t>(aServerIf),
                        PackConversion<int, int32_t>(aServiceHandle),
                        aUuid, aProperties, aPermissions, *pdu);
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
BluetoothDaemonGattModule::ServerAddDescriptorCmd(
  int aServerIf, int aServiceHandle, const BluetoothUuid& aUuid,
  BluetoothGattAttrPerm aPermissions, BluetoothGattServerResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_SERVER_ADD_DESCRIPTOR,
                        4 + // Server Interface
                        4 + // Service Handle
                        16 + // UUID
                        4)); // Permissions

  nsresult rv = PackPDU(PackConversion<int, int32_t>(aServerIf),
                        PackConversion<int, int32_t>(aServiceHandle),
                        aUuid, aPermissions, *pdu);
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
BluetoothDaemonGattModule::ServerStartServiceCmd(
  int aServerIf, int aServiceHandle, BluetoothTransport aTransport,
  BluetoothGattServerResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_SERVER_START_SERVICE,
                        4 + // Server Interface
                        4 + // Service Handle
                        4)); // Transport

  nsresult rv = PackPDU(
    PackConversion<int, int32_t>(aServerIf),
    PackConversion<int, int32_t>(aServiceHandle),
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
BluetoothDaemonGattModule::ServerStopServiceCmd(
  int aServerIf, int aServiceHandle, BluetoothGattServerResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_SERVER_STOP_SERVICE,
                        4 + // Server Interface
                        4)); // Service Handle

  nsresult rv = PackPDU(PackConversion<int, int32_t>(aServerIf),
                        PackConversion<int, int32_t>(aServiceHandle), *pdu);
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
BluetoothDaemonGattModule::ServerDeleteServiceCmd(
  int aServerIf, int aServiceHandle, BluetoothGattServerResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_SERVER_DELETE_SERVICE,
                        4 + // Server Interface
                        4)); // Service Handle

  nsresult rv = PackPDU(PackConversion<int, int32_t>(aServerIf),
                        PackConversion<int, int32_t>(aServiceHandle), *pdu);
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
BluetoothDaemonGattModule::ServerSendIndicationCmd(
  int aServerIf, int aAttributeHandle, int aConnId, int aLength, bool aConfirm,
  uint8_t* aValue, BluetoothGattServerResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_SERVER_SEND_INDICATION,
                        0));

  nsresult rv = PackPDU(PackConversion<int, int32_t>(aServerIf),
                        PackConversion<int, int32_t>(aAttributeHandle),
                        PackConversion<int, int32_t>(aConnId),
                        PackConversion<int, int32_t>(aLength),
                        PackConversion<bool, int32_t>(aConfirm),
                        PackArray<uint8_t>(aValue, aLength), *pdu);
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
BluetoothDaemonGattModule::ServerSendResponseCmd(
  int aConnId, int aTransId, BluetoothGattStatus aStatus,
  const BluetoothGattResponse& aResponse,
  BluetoothGattServerResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoPtr<DaemonSocketPDU> pdu(
    new DaemonSocketPDU(SERVICE_ID, OPCODE_SERVER_SEND_RESPONSE,
                        0));

  nsresult rv = PackPDU(
    PackConversion<int, int32_t>(aConnId),
    PackConversion<int, int32_t>(aTransId),
    aResponse.mHandle,
    aResponse.mOffset,
    PackConversion<BluetoothGattAuthReq, uint8_t>(aResponse.mAuthReq),
    PackConversion<BluetoothGattStatus, int32_t>(aStatus),
    aResponse.mLength,
    PackArray<uint8_t>(aResponse.mValue, aResponse.mLength), *pdu);

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
  const DaemonSocketPDUHeader& aHeader,
  DaemonSocketPDU& aPDU, BluetoothGattResultHandler* aRes)
{
  ErrorRunnable::Dispatch(
    aRes, &BluetoothGattResultHandler::OnError, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientRegisterRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::RegisterClient,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientUnregisterRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::UnregisterClient,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientScanRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::Scan, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientConnectRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::Connect, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientDisconnectRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::Disconnect,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientListenRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::Listen, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientRefreshRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::Refresh, UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientSearchServiceRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::SearchService,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientGetIncludedServiceRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::GetIncludedService,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientGetCharacteristicRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::GetCharacteristic,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientGetDescriptorRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::GetDescriptor,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientReadCharacteristicRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::ReadCharacteristic,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientWriteCharacteristicRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::WriteCharacteristic,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientReadDescriptorRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::ReadDescriptor,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientWriteDescriptorRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::WriteDescriptor,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientExecuteWriteRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::ExecuteWrite,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientRegisterNotificationRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::RegisterNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientDeregisterNotificationRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::DeregisterNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientReadRemoteRssiRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::ReadRemoteRssi,
    UnpackPDUInitOp(aPDU));
}

// Init operator class for ClientGetDeviceTypeRsp
class BluetoothDaemonGattModule::ClientGetDeviceTypeInitOp final
  : private PDUInitOp
{
public:
  ClientGetDeviceTypeInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (BluetoothTypeOfDevice& aArg1) const
  {
    /* Read device type */
    nsresult rv = UnpackPDU(
      GetPDU(), UnpackConversion<uint8_t, BluetoothTypeOfDevice>(aArg1));
    if (NS_FAILED(rv)) {
      return rv;
    }
    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonGattModule::ClientGetDeviceTypeRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientGetDeviceTypeResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::GetDeviceType,
    ClientGetDeviceTypeInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientSetAdvDataRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::SetAdvData,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientTestCommandRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattClientResultHandler* aRes)
{
  ClientResultRunnable::Dispatch(
    aRes, &BluetoothGattClientResultHandler::TestCommand,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ServerRegisterRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattServerResultHandler* aRes)
{
  ServerResultRunnable::Dispatch(
    aRes, &BluetoothGattServerResultHandler::RegisterServer,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ServerUnregisterRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattServerResultHandler* aRes)
{
  ServerResultRunnable::Dispatch(
    aRes, &BluetoothGattServerResultHandler::UnregisterServer,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ServerConnectPeripheralRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattServerResultHandler* aRes)
{
  ServerResultRunnable::Dispatch(
    aRes, &BluetoothGattServerResultHandler::ConnectPeripheral,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ServerDisconnectPeripheralRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattServerResultHandler* aRes)
{
  ServerResultRunnable::Dispatch(
    aRes, &BluetoothGattServerResultHandler::DisconnectPeripheral,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ServerAddServiceRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattServerResultHandler* aRes)
{
  ServerResultRunnable::Dispatch(
    aRes, &BluetoothGattServerResultHandler::AddService,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ServerAddIncludedServiceRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattServerResultHandler* aRes)
{
  ServerResultRunnable::Dispatch(
    aRes, &BluetoothGattServerResultHandler::AddIncludedService,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ServerAddCharacteristicRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattServerResultHandler* aRes)
{
  ServerResultRunnable::Dispatch(
    aRes, &BluetoothGattServerResultHandler::AddCharacteristic,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ServerAddDescriptorRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattServerResultHandler* aRes)
{
  ServerResultRunnable::Dispatch(
    aRes, &BluetoothGattServerResultHandler::AddDescriptor,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ServerStartServiceRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattServerResultHandler* aRes)
{
  ServerResultRunnable::Dispatch(
    aRes, &BluetoothGattServerResultHandler::StartService,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ServerStopServiceRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattServerResultHandler* aRes)
{
  ServerResultRunnable::Dispatch(
    aRes, &BluetoothGattServerResultHandler::StopService,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ServerDeleteServiceRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattServerResultHandler* aRes)
{
  ServerResultRunnable::Dispatch(
    aRes, &BluetoothGattServerResultHandler::DeleteService,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ServerSendIndicationRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattServerResultHandler* aRes)
{
  ServerResultRunnable::Dispatch(
    aRes, &BluetoothGattServerResultHandler::SendIndication,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ServerSendResponseRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  BluetoothGattServerResultHandler* aRes)
{
  ServerResultRunnable::Dispatch(
    aRes, &BluetoothGattServerResultHandler::SendResponse,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::HandleRsp(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  void* aUserData)
{
  static void (BluetoothDaemonGattModule::* const HandleRsp[])(
    const DaemonSocketPDUHeader&,
    DaemonSocketPDU&,
    BluetoothGattResultHandler*) = {
    INIT_ARRAY_AT(OPCODE_ERROR,
      &BluetoothDaemonGattModule::ErrorRsp)
    };

  static void (BluetoothDaemonGattModule::* const HandleClientRsp[])(
    const DaemonSocketPDUHeader&,
    DaemonSocketPDU&,
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

  /**
   * These client opcodes are added since non-trivial designated initializers
   * are not supported.
   * We could use a single array for GattRsp, GattClientRsp, and GattServerRsp
   * after combining result handlers in Bug 1181512.
   **/
  static void (BluetoothDaemonGattModule::* const HandleServerRsp[])(
    const DaemonSocketPDUHeader&,
    DaemonSocketPDU&,
    BluetoothGattServerResultHandler*) = {
    INIT_ARRAY_AT(0, nullptr),
    INIT_ARRAY_AT(OPCODE_CLIENT_REGISTER, nullptr),
    INIT_ARRAY_AT(OPCODE_CLIENT_UNREGISTER, nullptr),
    INIT_ARRAY_AT(OPCODE_CLIENT_SCAN, nullptr),
    INIT_ARRAY_AT(OPCODE_CLIENT_CONNECT, nullptr),
    INIT_ARRAY_AT(OPCODE_CLIENT_DISCONNECT, nullptr),
    INIT_ARRAY_AT(OPCODE_CLIENT_LISTEN, nullptr),
    INIT_ARRAY_AT(OPCODE_CLIENT_REFRESH, nullptr),
    INIT_ARRAY_AT(OPCODE_CLIENT_SEARCH_SERVICE, nullptr),
    INIT_ARRAY_AT(OPCODE_CLIENT_GET_INCLUDED_SERVICE, nullptr),
    INIT_ARRAY_AT(OPCODE_CLIENT_GET_CHARACTERISTIC, nullptr),
    INIT_ARRAY_AT(OPCODE_CLIENT_GET_DESCRIPTOR, nullptr),
    INIT_ARRAY_AT(OPCODE_CLIENT_READ_CHARACTERISTIC, nullptr),
    INIT_ARRAY_AT(OPCODE_CLIENT_WRITE_CHARACTERISTIC, nullptr),
    INIT_ARRAY_AT(OPCODE_CLIENT_READ_DESCRIPTOR, nullptr),
    INIT_ARRAY_AT(OPCODE_CLIENT_WRITE_DESCRIPTOR, nullptr),
    INIT_ARRAY_AT(OPCODE_CLIENT_EXECUTE_WRITE, nullptr),
    INIT_ARRAY_AT(OPCODE_CLIENT_REGISTER_NOTIFICATION, nullptr),
    INIT_ARRAY_AT(OPCODE_CLIENT_DEREGISTER_NOTIFICATION, nullptr),
    INIT_ARRAY_AT(OPCODE_CLIENT_READ_REMOTE_RSSI, nullptr),
    INIT_ARRAY_AT(OPCODE_CLIENT_GET_DEVICE_TYPE, nullptr),
    INIT_ARRAY_AT(OPCODE_CLIENT_SET_ADV_DATA, nullptr),
    INIT_ARRAY_AT(OPCODE_CLIENT_TEST_COMMAND, nullptr),
    INIT_ARRAY_AT(OPCODE_SERVER_REGISTER,
      &BluetoothDaemonGattModule::ServerRegisterRsp),
    INIT_ARRAY_AT(OPCODE_SERVER_UNREGISTER,
      &BluetoothDaemonGattModule::ServerUnregisterRsp),
    INIT_ARRAY_AT(OPCODE_SERVER_CONNECT_PERIPHERAL,
      &BluetoothDaemonGattModule::ServerConnectPeripheralRsp),
    INIT_ARRAY_AT(OPCODE_SERVER_DISCONNECT_PERIPHERAL,
      &BluetoothDaemonGattModule::ServerDisconnectPeripheralRsp),
    INIT_ARRAY_AT(OPCODE_SERVER_ADD_SERVICE,
      &BluetoothDaemonGattModule::ServerAddServiceRsp),
    INIT_ARRAY_AT(OPCODE_SERVER_ADD_INCLUDED_SERVICE,
      &BluetoothDaemonGattModule::ServerAddIncludedServiceRsp),
    INIT_ARRAY_AT(OPCODE_SERVER_ADD_CHARACTERISTIC,
      &BluetoothDaemonGattModule::ServerAddCharacteristicRsp),
    INIT_ARRAY_AT(OPCODE_SERVER_ADD_DESCRIPTOR,
      &BluetoothDaemonGattModule::ServerAddDescriptorRsp),
    INIT_ARRAY_AT(OPCODE_SERVER_START_SERVICE,
      &BluetoothDaemonGattModule::ServerStartServiceRsp),
    INIT_ARRAY_AT(OPCODE_SERVER_STOP_SERVICE,
      &BluetoothDaemonGattModule::ServerStopServiceRsp),
    INIT_ARRAY_AT(OPCODE_SERVER_DELETE_SERVICE,
      &BluetoothDaemonGattModule::ServerDeleteServiceRsp),
    INIT_ARRAY_AT(OPCODE_SERVER_SEND_INDICATION,
      &BluetoothDaemonGattModule::ServerSendIndicationRsp),
    INIT_ARRAY_AT(OPCODE_SERVER_SEND_RESPONSE,
      &BluetoothDaemonGattModule::ServerSendResponseRsp)
  };

  MOZ_ASSERT(!NS_IsMainThread()); // I/O thread

  bool isInGattArray = aHeader.mOpcode < MOZ_ARRAY_LENGTH(HandleRsp) &&
                       HandleRsp[aHeader.mOpcode];
  bool isInGattClientArray =
    aHeader.mOpcode < MOZ_ARRAY_LENGTH(HandleClientRsp) &&
    HandleClientRsp[aHeader.mOpcode];

  bool isInGattServerArray =
    aHeader.mOpcode < MOZ_ARRAY_LENGTH(HandleServerRsp) &&
    HandleServerRsp[aHeader.mOpcode];

  if (NS_WARN_IF(!isInGattArray && !isInGattClientArray &&
                 !isInGattServerArray)) {
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
  } else if (isInGattClientArray) {
    nsRefPtr<BluetoothGattClientResultHandler> res =
      already_AddRefed<BluetoothGattClientResultHandler>(
        static_cast<BluetoothGattClientResultHandler*>(aUserData));
    (this->*(HandleClientRsp[aHeader.mOpcode]))(aHeader, aPDU, res);
  } else { // isInGattServerArray
    nsRefPtr<BluetoothGattServerResultHandler> res =
      already_AddRefed<BluetoothGattServerResultHandler>(
        static_cast<BluetoothGattServerResultHandler*>(aUserData));
    (this->*(HandleServerRsp[aHeader.mOpcode]))(aHeader, aPDU, res);
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

// Returns the current notification handler to a notification runnable
class BluetoothDaemonGattModule::ServerNotificationHandlerWrapper final
{
public:
  typedef BluetoothGattServerNotificationHandler ObjectType;

  static ObjectType* GetInstance()
  {
    MOZ_ASSERT(NS_IsMainThread());

    return sNotificationHandler;
  }
};

void
BluetoothDaemonGattModule::ClientRegisterNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
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
  ClientScanResultInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (nsString& aArg1,
               int& aArg2,
               BluetoothGattAdvData& aArg3) const
  {
    DaemonSocketPDU& pdu = GetPDU();

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
    uint16_t length;
    rv = UnpackPDU(pdu, length);
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read Adv Data */
    rv = UnpackPDU(pdu, UnpackArray<uint8_t>(aArg3.mAdvData, length));
    if (NS_FAILED(rv)) {
      return rv;
    }

    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonGattModule::ClientScanResultNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
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
  ClientConnectDisconnectInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (int& aArg1,
               BluetoothGattStatus& aArg2,
               int& aArg3,
               nsString& aArg4) const
  {
    DaemonSocketPDU& pdu = GetPDU();

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
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ClientConnectNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::ConnectNotification,
    ClientConnectDisconnectInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientDisconnectNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ClientDisconnectNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::DisconnectNotification,
    ClientConnectDisconnectInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientSearchCompleteNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ClientSearchCompleteNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::SearchCompleteNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientSearchResultNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ClientSearchResultNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::SearchResultNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientGetCharacteristicNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ClientGetCharacteristicNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::GetCharacteristicNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientGetDescriptorNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ClientGetDescriptorNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::GetDescriptorNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientGetIncludedServiceNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ClientGetIncludedServiceNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::GetIncludedServiceNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientRegisterNotificationNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ClientRegisterNotificationNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::RegisterNotificationNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientNotifyNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ClientNotifyNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::NotifyNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientReadCharacteristicNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ClientReadCharacteristicNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::ReadCharacteristicNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientWriteCharacteristicNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ClientWriteCharacteristicNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::WriteCharacteristicNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientReadDescriptorNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ClientReadDescriptorNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::ReadDescriptorNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientWriteDescriptorNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ClientWriteDescriptorNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::WriteDescriptorNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientExecuteWriteNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
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
  ClientReadRemoteRssiInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (int& aArg1,
               nsString& aArg2,
               int& aArg3,
               BluetoothGattStatus& aArg4) const
  {
    DaemonSocketPDU& pdu = GetPDU();

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
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ClientReadRemoteRssiNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::ReadRemoteRssiNotification,
    ClientReadRemoteRssiInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ClientListenNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ClientListenNotification::Dispatch(
    &BluetoothGattClientNotificationHandler::ListenNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ServerRegisterNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ServerRegisterNotification::Dispatch(
    &BluetoothGattServerNotificationHandler::RegisterServerNotification,
    UnpackPDUInitOp(aPDU));
}

// Init operator class for ServerConnectionNotification
class BluetoothDaemonGattModule::ServerConnectionInitOp final
  : private PDUInitOp
{
public:
  ServerConnectionInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (int& aArg1,
               int& aArg2,
               bool& aArg3,
               nsString& aArg4) const
  {
    DaemonSocketPDU& pdu = GetPDU();

    /* Read connection ID */
    nsresult rv = UnpackPDU(pdu, aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read server interface */
    rv = UnpackPDU(pdu, aArg2);
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read connected */
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
BluetoothDaemonGattModule::ServerConnectionNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ServerConnectionNotification::Dispatch(
    &BluetoothGattServerNotificationHandler::ConnectionNotification,
    ServerConnectionInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ServerServiceAddedNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ServerServiceAddedNotification::Dispatch(
    &BluetoothGattServerNotificationHandler::ServiceAddedNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ServerIncludedServiceAddedNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ServerIncludedServiceAddedNotification::Dispatch(
    &BluetoothGattServerNotificationHandler::IncludedServiceAddedNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ServerCharacteristicAddedNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ServerCharacteristicAddedNotification::Dispatch(
    &BluetoothGattServerNotificationHandler::CharacteristicAddedNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ServerDescriptorAddedNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ServerDescriptorAddedNotification::Dispatch(
    &BluetoothGattServerNotificationHandler::DescriptorAddedNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ServerServiceStartedNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ServerServiceStartedNotification::Dispatch(
    &BluetoothGattServerNotificationHandler::ServiceStartedNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ServerServiceStoppedNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ServerServiceStoppedNotification::Dispatch(
    &BluetoothGattServerNotificationHandler::ServiceStoppedNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ServerServiceDeletedNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ServerServiceDeletedNotification::Dispatch(
    &BluetoothGattServerNotificationHandler::ServiceDeletedNotification,
    UnpackPDUInitOp(aPDU));
}

// Init operator class for ServerRequestReadNotification
class BluetoothDaemonGattModule::ServerRequestReadInitOp final
  : private PDUInitOp
{
public:
  ServerRequestReadInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (int& aArg1,
               int& aArg2,
               nsString& aArg3,
               int& aArg4,
               int& aArg5,
               bool& aArg6) const
  {
    DaemonSocketPDU& pdu = GetPDU();

    /* Read connection ID */
    nsresult rv = UnpackPDU(pdu, aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read trans ID */
    rv = UnpackPDU(pdu, aArg2);
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read address */
    rv = UnpackPDU(
      pdu, UnpackConversion<BluetoothAddress, nsAString>(aArg3));
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read attribute handle */
    rv = UnpackPDU(pdu, aArg4);
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read offset */
    rv = UnpackPDU(pdu, aArg5);
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read isLong */
    rv = UnpackPDU(pdu, aArg6);
    if (NS_FAILED(rv)) {
      return rv;
    }

    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonGattModule::ServerRequestReadNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ServerRequestReadNotification::Dispatch(
    &BluetoothGattServerNotificationHandler::RequestReadNotification,
    ServerRequestReadInitOp(aPDU));
}

// Init operator class for ServerRequestWriteNotification
class BluetoothDaemonGattModule::ServerRequestWriteInitOp final
  : private PDUInitOp
{
public:
  ServerRequestWriteInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (int& aArg1,
               int& aArg2,
               nsString& aArg3,
               int& aArg4,
               int& aArg5,
               int& aArg6,
               nsAutoArrayPtr<uint8_t>& aArg7,
               bool& aArg8,
               bool& aArg9) const
  {
    DaemonSocketPDU& pdu = GetPDU();

    /* Read connection ID */
    nsresult rv = UnpackPDU(pdu, aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read trans ID */
    rv = UnpackPDU(pdu, aArg2);
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read address */
    rv = UnpackPDU(
      pdu, UnpackConversion<BluetoothAddress, nsAString>(aArg3));
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read attribute handle */
    rv = UnpackPDU(pdu, aArg4);
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read offset */
    rv = UnpackPDU(pdu, aArg5);
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read Length */
    rv = UnpackPDU(pdu, aArg6);
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read value */
    rv = UnpackPDU(pdu,
                   UnpackArray<uint8_t>(aArg7, static_cast<size_t>(aArg6)));
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read need response */
    rv = UnpackPDU(pdu, aArg8);
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read isPrepare */
    rv = UnpackPDU(pdu, aArg9);
    if (NS_FAILED(rv)) {
      return rv;
    }

    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonGattModule::ServerRequestWriteNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ServerRequestWriteNotification::Dispatch(
    &BluetoothGattServerNotificationHandler::RequestWriteNotification,
    ServerRequestWriteInitOp(aPDU));
}

// Init operator class for ServerRequestExecuteWriteNotification
class BluetoothDaemonGattModule::ServerRequestExecuteWriteInitOp final
  : private PDUInitOp
{
public:
  ServerRequestExecuteWriteInitOp(DaemonSocketPDU& aPDU)
    : PDUInitOp(aPDU)
  { }

  nsresult
  operator () (int& aArg1,
               int& aArg2,
               nsString& aArg3,
               bool& aArg4) const
  {
    DaemonSocketPDU& pdu = GetPDU();

    /* Read connection ID */
    nsresult rv = UnpackPDU(pdu, aArg1);
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read trans ID */
    rv = UnpackPDU(pdu, aArg2);
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read address */
    rv = UnpackPDU(
      pdu, UnpackConversion<BluetoothAddress, nsAString>(aArg3));
    if (NS_FAILED(rv)) {
      return rv;
    }
    /* Read execute write */
    rv = UnpackPDU(pdu, aArg4);
    if (NS_FAILED(rv)) {
      return rv;
    }

    WarnAboutTrailingData();
    return NS_OK;
  }
};

void
BluetoothDaemonGattModule::ServerRequestExecuteWriteNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ServerRequestExecuteWriteNotification::Dispatch(
    &BluetoothGattServerNotificationHandler::RequestExecuteWriteNotification,
    ServerRequestExecuteWriteInitOp(aPDU));
}

void
BluetoothDaemonGattModule::ServerResponseConfirmationNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU)
{
  ServerResponseConfirmationNotification::Dispatch(
    &BluetoothGattServerNotificationHandler::ResponseConfirmationNotification,
    UnpackPDUInitOp(aPDU));
}

void
BluetoothDaemonGattModule::HandleNtf(
  const DaemonSocketPDUHeader& aHeader, DaemonSocketPDU& aPDU,
  void* aUserData)
{
  static void (BluetoothDaemonGattModule::* const HandleNtf[])(
    const DaemonSocketPDUHeader&, DaemonSocketPDU&) = {
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
    INIT_ARRAY_AT(17, &BluetoothDaemonGattModule::ClientListenNtf),
    INIT_ARRAY_AT(18, &BluetoothDaemonGattModule::ServerRegisterNtf),
    INIT_ARRAY_AT(19, &BluetoothDaemonGattModule::ServerConnectionNtf),
    INIT_ARRAY_AT(20, &BluetoothDaemonGattModule::ServerServiceAddedNtf),
    INIT_ARRAY_AT(21, &BluetoothDaemonGattModule::ServerIncludedServiceAddedNtf),
    INIT_ARRAY_AT(22, &BluetoothDaemonGattModule::ServerCharacteristicAddedNtf),
    INIT_ARRAY_AT(23, &BluetoothDaemonGattModule::ServerDescriptorAddedNtf),
    INIT_ARRAY_AT(24, &BluetoothDaemonGattModule::ServerServiceStartedNtf),
    INIT_ARRAY_AT(25, &BluetoothDaemonGattModule::ServerServiceStoppedNtf),
    INIT_ARRAY_AT(26, &BluetoothDaemonGattModule::ServerServiceDeletedNtf),
    INIT_ARRAY_AT(27, &BluetoothDaemonGattModule::ServerRequestReadNtf),
    INIT_ARRAY_AT(28, &BluetoothDaemonGattModule::ServerRequestWriteNtf),
    INIT_ARRAY_AT(29, &BluetoothDaemonGattModule::ServerRequestExecuteWriteNtf),
    INIT_ARRAY_AT(30, &BluetoothDaemonGattModule::ServerResponseConfirmationNtf)
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
// Gatt interface
//

BluetoothDaemonGattClientInterface::BluetoothDaemonGattClientInterface(
  BluetoothDaemonGattModule* aModule)
  : mModule(aModule)
{ }

BluetoothDaemonGattInterface::BluetoothDaemonGattInterface(
  BluetoothDaemonGattModule* aModule)
  : mModule(aModule)
{ }

BluetoothDaemonGattInterface::~BluetoothDaemonGattInterface()
{ }

class BluetoothDaemonGattInterface::InitResultHandler final
  : public BluetoothSetupResultHandler
{
public:
  InitResultHandler(BluetoothGattResultHandler* aRes)
    : mRes(aRes)
  {
    MOZ_ASSERT(mRes);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());

    mRes->OnError(aStatus);
  }

  void RegisterModule() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    mRes->Init();
  }

private:
  nsRefPtr<BluetoothGattResultHandler> mRes;
};

void
BluetoothDaemonGattInterface::Init(
  BluetoothGattNotificationHandler* aNotificationHandler,
  BluetoothGattResultHandler* aRes)
{
  // Set notification handler _before_ registering the module. It could
  // happen that we receive notifications, before the result handler runs.
  mModule->SetNotificationHandler(aNotificationHandler);

  InitResultHandler* res;

  if (aRes) {
    res = new InitResultHandler(aRes);
  } else {
    // We don't need a result handler if the caller is not interested.
    res = nullptr;
  }

  nsresult rv = mModule->RegisterModule(
    BluetoothDaemonGattModule::SERVICE_ID, 0x00,
    BluetoothDaemonGattModule::MAX_NUM_CLIENTS, res);

  if (NS_FAILED(rv) && aRes) {
    DispatchError(aRes, rv);
  }
}

class BluetoothDaemonGattInterface::CleanupResultHandler final
  : public BluetoothSetupResultHandler
{
public:
  CleanupResultHandler(BluetoothDaemonGattModule* aModule,
                       BluetoothGattResultHandler* aRes)
    : mModule(aModule)
    , mRes(aRes)
  {
    MOZ_ASSERT(mModule);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (mRes) {
      mRes->OnError(aStatus);
    }
  }

  void UnregisterModule() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    // Clear notification handler _after_ module has been
    // unregistered. While unregistering the module, we might
    // still receive notifications.
    mModule->SetNotificationHandler(nullptr);

    if (mRes) {
      mRes->Cleanup();
    }
  }

private:
  BluetoothDaemonGattModule* mModule;
  nsRefPtr<BluetoothGattResultHandler> mRes;
};

void
BluetoothDaemonGattInterface::Cleanup(
  BluetoothGattResultHandler* aRes)
{
  nsresult rv = mModule->UnregisterModule(
    BluetoothDaemonGattModule::SERVICE_ID,
    new CleanupResultHandler(mModule, aRes));
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Register / Unregister */
void
BluetoothDaemonGattClientInterface::RegisterClient(
  const BluetoothUuid& aUuid, BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ClientRegisterCmd(aUuid, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonGattClientInterface::UnregisterClient(
  int aClientIf, BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ClientUnregisterCmd(aClientIf, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Start / Stop LE Scan */
void
BluetoothDaemonGattClientInterface::Scan(
  int aClientIf, bool aStart, BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ClientScanCmd(aClientIf, aStart, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Connect / Disconnect */

void
BluetoothDaemonGattClientInterface::Connect(
  int aClientIf, const nsAString& aBdAddr, bool aIsDirect,
  BluetoothTransport aTransport, BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ClientConnectCmd(
    aClientIf, aBdAddr, aIsDirect, aTransport, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonGattClientInterface::Disconnect(
  int aClientIf, const nsAString& aBdAddr, int aConnId,
  BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ClientDisconnectCmd(
    aClientIf, aBdAddr, aConnId, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Start / Stop advertisements to listen for incoming connections */
void
BluetoothDaemonGattClientInterface::Listen(
  int aClientIf, bool aIsStart, BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ClientListenCmd(aClientIf, aIsStart, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Clear the attribute cache for a given device*/
void
BluetoothDaemonGattClientInterface::Refresh(
  int aClientIf, const nsAString& aBdAddr, BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ClientRefreshCmd(aClientIf, aBdAddr, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Enumerate Attributes */
void
BluetoothDaemonGattClientInterface::SearchService(
  int aConnId, bool aSearchAll, const BluetoothUuid& aUuid,
  BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ClientSearchServiceCmd(
    aConnId, !aSearchAll /* Filtered */, aUuid, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonGattClientInterface::GetIncludedService(
  int aConnId, const BluetoothGattServiceId& aServiceId, bool aFirst,
  const BluetoothGattServiceId& aStartServiceId,
  BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ClientGetIncludedServiceCmd(
    aConnId, aServiceId, !aFirst /* Continuation */, aStartServiceId, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonGattClientInterface::GetCharacteristic(
  int aConnId, const BluetoothGattServiceId& aServiceId, bool aFirst,
  const BluetoothGattId& aStartCharId, BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ClientGetCharacteristicCmd(
    aConnId, aServiceId, !aFirst /* Continuation */, aStartCharId, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonGattClientInterface::GetDescriptor(
  int aConnId, const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharId, bool aFirst,
  const BluetoothGattId& aDescriptorId, BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ClientGetDescriptorCmd(
    aConnId, aServiceId, aCharId, !aFirst /* Continuation */, aDescriptorId,
    aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Read / Write An Attribute */
void
BluetoothDaemonGattClientInterface::ReadCharacteristic(
  int aConnId, const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharId, BluetoothGattAuthReq aAuthReq,
  BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ClientReadCharacteristicCmd(
    aConnId, aServiceId, aCharId, aAuthReq, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonGattClientInterface::WriteCharacteristic(
  int aConnId, const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharId, BluetoothGattWriteType aWriteType,
  BluetoothGattAuthReq aAuthReq, const nsTArray<uint8_t>& aValue,
  BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ClientWriteCharacteristicCmd(
    aConnId, aServiceId, aCharId, aWriteType,
    aValue.Length() * sizeof(uint8_t), aAuthReq,
    reinterpret_cast<char*>(const_cast<uint8_t*>(aValue.Elements())), aRes);

  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonGattClientInterface::ReadDescriptor(
  int aConnId, const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharId, const BluetoothGattId& aDescriptorId,
  BluetoothGattAuthReq aAuthReq, BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ClientReadDescriptorCmd(
    aConnId, aServiceId, aCharId, aDescriptorId, aAuthReq, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonGattClientInterface::WriteDescriptor(
  int aConnId, const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharId, const BluetoothGattId& aDescriptorId,
  BluetoothGattWriteType aWriteType, BluetoothGattAuthReq aAuthReq,
  const nsTArray<uint8_t>& aValue, BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ClientWriteDescriptorCmd(
    aConnId, aServiceId, aCharId, aDescriptorId, aWriteType,
    aValue.Length() * sizeof(uint8_t), aAuthReq,
    reinterpret_cast<char*>(const_cast<uint8_t*>(aValue.Elements())), aRes);

  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Execute / Abort Prepared Write*/
void
BluetoothDaemonGattClientInterface::ExecuteWrite(
  int aConnId, int aIsExecute, BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ClientExecuteWriteCmd(aConnId, aIsExecute, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

/* Register / Deregister Characteristic Notifications or Indications */
void
BluetoothDaemonGattClientInterface::RegisterNotification(
  int aClientIf, const nsAString& aBdAddr,
  const BluetoothGattServiceId& aServiceId, const BluetoothGattId& aCharId,
  BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ClientRegisterNotificationCmd(
    aClientIf, aBdAddr, aServiceId, aCharId, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonGattClientInterface::DeregisterNotification(
  int aClientIf, const nsAString& aBdAddr,
  const BluetoothGattServiceId& aServiceId, const BluetoothGattId& aCharId,
  BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ClientDeregisterNotificationCmd(
    aClientIf, aBdAddr, aServiceId, aCharId, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonGattClientInterface::ReadRemoteRssi(
  int aClientIf, const nsAString& aBdAddr,
  BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ClientReadRemoteRssiCmd(
    aClientIf, aBdAddr, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonGattClientInterface::GetDeviceType(
  const nsAString& aBdAddr, BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ClientGetDeviceTypeCmd(aBdAddr, aRes);
  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonGattClientInterface::SetAdvData(
  int aServerIf, bool aIsScanRsp, bool aIsNameIncluded,
  bool aIsTxPowerIncluded, int aMinInterval, int aMaxInterval, int aApperance,
  uint16_t aManufacturerLen, char* aManufacturerData,
  uint16_t aServiceDataLen, char* aServiceData,
  uint16_t aServiceUUIDLen, char* aServiceUUID,
  BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ClientSetAdvDataCmd(
    aServerIf, aIsScanRsp, aIsNameIncluded, aIsTxPowerIncluded, aMinInterval,
    aMaxInterval, aApperance, aManufacturerLen, aManufacturerData,
    aServiceDataLen, aServiceData, aServiceUUIDLen, aServiceUUID, aRes);

  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonGattClientInterface::TestCommand(
  int aCommand, const BluetoothGattTestParam& aTestParam,
  BluetoothGattClientResultHandler* aRes)
{
  MOZ_ASSERT(mModule);

  nsresult rv = mModule->ClientTestCommandCmd(aCommand, aTestParam, aRes);

  if (NS_FAILED(rv)) {
    DispatchError(aRes, rv);
  }
}

void
BluetoothDaemonGattClientInterface::DispatchError(
  BluetoothGattClientResultHandler* aRes, BluetoothStatus aStatus)
{
  BluetoothResultRunnable1<BluetoothGattClientResultHandler, void,
                           BluetoothStatus, BluetoothStatus>::Dispatch(
    aRes, &BluetoothGattResultHandler::OnError,
    ConstantInitOp1<BluetoothStatus>(aStatus));
}

void
BluetoothDaemonGattClientInterface::DispatchError(
  BluetoothGattClientResultHandler* aRes, nsresult aRv)
{
  BluetoothStatus status;

  if (NS_WARN_IF(NS_FAILED(Convert(aRv, status)))) {
    status = STATUS_FAIL;
  }
  DispatchError(aRes, status);
}

void
BluetoothDaemonGattInterface::DispatchError(
  BluetoothGattResultHandler* aRes, BluetoothStatus aStatus)
{
  BluetoothResultRunnable1<BluetoothGattResultHandler, void,
                           BluetoothStatus, BluetoothStatus>::Dispatch(
    aRes, &BluetoothGattResultHandler::OnError,
    ConstantInitOp1<BluetoothStatus>(aStatus));
}

void
BluetoothDaemonGattInterface::DispatchError(
  BluetoothGattResultHandler* aRes, nsresult aRv)
{
  BluetoothStatus status;

  if (NS_WARN_IF(NS_FAILED(Convert(aRv, status)))) {
    status = STATUS_FAIL;
  }
  DispatchError(aRes, status);
}

BluetoothGattClientInterface*
BluetoothDaemonGattInterface::GetBluetoothGattClientInterface()
{
  MOZ_ASSERT(mModule);

  BluetoothDaemonGattClientInterface* gattClientInterface =
    new BluetoothDaemonGattClientInterface(mModule);

  return gattClientInterface;
}

END_BLUETOOTH_NAMESPACE
