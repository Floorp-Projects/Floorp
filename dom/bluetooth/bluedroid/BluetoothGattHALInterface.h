/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_bluetoothgatthalinterface_h__
#define mozilla_dom_bluetooth_bluedroid_bluetoothgatthalinterface_h__

#include <hardware/bluetooth.h>
#if ANDROID_VERSION >= 19
#include <hardware/bt_gatt.h>
#endif
#include "BluetoothCommon.h"
#include "BluetoothInterface.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothHALInterface;

class BluetoothGattClientHALInterface final
  : public BluetoothGattClientInterface
{
public:
  friend class BluetoothGattHALInterface;

  /* Register / Unregister */
  void RegisterClient(const BluetoothUuid& aUuid,
                      BluetoothGattClientResultHandler* aRes);
  void UnregisterClient(int aClientIf,
                        BluetoothGattClientResultHandler* aRes);

  /* Start / Stop LE Scan */
  void Scan(int aClientIf, bool aStart,
            BluetoothGattClientResultHandler* aRes);

  /* Connect / Disconnect */
  void Connect(int aClientIf,
               const nsAString& aBdAddr,
               bool aIsDirect, /* auto connect */
               BluetoothTransport aTransport,
               BluetoothGattClientResultHandler* aRes);
  void Disconnect(int aClientIf,
                  const nsAString& aBdAddr,
                  int aConnId,
                  BluetoothGattClientResultHandler* aRes);

  /* Start / Stop advertisements to listen for incoming connections */
  void Listen(int aClientIf,
              bool aIsStart,
              BluetoothGattClientResultHandler* aRes);

  /* Clear the attribute cache for a given device*/
  void Refresh(int aClientIf,
               const nsAString& aBdAddr,
               BluetoothGattClientResultHandler* aRes);

  /* Enumerate Attributes */
  void SearchService(int aConnId,
                     bool aSearchAll,
                     const BluetoothUuid& aUuid,
                     BluetoothGattClientResultHandler* aRes);
  void GetIncludedService(int aConnId,
                          const BluetoothGattServiceId& aServiceId,
                          bool aFirst,
                          const BluetoothGattServiceId& aStartServiceId,
                          BluetoothGattClientResultHandler* aRes);
  void GetCharacteristic(int aConnId,
                         const BluetoothGattServiceId& aServiceId,
                         bool aFirst,
                         const BluetoothGattId& aStartCharId,
                         BluetoothGattClientResultHandler* aRes);
  void GetDescriptor(int aConnId,
                     const BluetoothGattServiceId& aServiceId,
                     const BluetoothGattId& aCharId,
                     bool aFirst,
                     const BluetoothGattId& aDescriptorId,
                     BluetoothGattClientResultHandler* aRes);

  /* Read / Write An Attribute */
  void ReadCharacteristic(int aConnId,
                          const BluetoothGattServiceId& aServiceId,
                          const BluetoothGattId& aCharId,
                          BluetoothGattAuthReq aAuthReq,
                          BluetoothGattClientResultHandler* aRes);
  void WriteCharacteristic(int aConnId,
                           const BluetoothGattServiceId& aServiceId,
                           const BluetoothGattId& aCharId,
                           BluetoothGattWriteType aWriteType,
                           BluetoothGattAuthReq aAuthReq,
                           const nsTArray<uint8_t>& aValue,
                           BluetoothGattClientResultHandler* aRes);
  void ReadDescriptor(int aConnId,
                      const BluetoothGattServiceId& aServiceId,
                      const BluetoothGattId& aCharId,
                      const BluetoothGattId& aDescriptorId,
                      BluetoothGattAuthReq aAuthReq,
                      BluetoothGattClientResultHandler* aRes);
  void WriteDescriptor(int aConnId,
                       const BluetoothGattServiceId& aServiceId,
                       const BluetoothGattId& aCharId,
                       const BluetoothGattId& aDescriptorId,
                       BluetoothGattWriteType aWriteType,
                       BluetoothGattAuthReq aAuthReq,
                       const nsTArray<uint8_t>& aValue,
                       BluetoothGattClientResultHandler* aRes);

  /* Execute / Abort Prepared Write*/
  void ExecuteWrite(int aConnId,
                    int aIsExecute,
                    BluetoothGattClientResultHandler* aRes);


  /* Register / Deregister Characteristic Notifications or Indications */
  void RegisterNotification(int aClientIf,
                            const nsAString& aBdAddr,
                            const BluetoothGattServiceId& aServiceId,
                            const BluetoothGattId& aCharId,
                            BluetoothGattClientResultHandler* aRes);
  void DeregisterNotification(int aClientIf,
                              const nsAString& aBdAddr,
                              const BluetoothGattServiceId& aServiceId,
                              const BluetoothGattId& aCharId,
                              BluetoothGattClientResultHandler* aRes);

  void ReadRemoteRssi(int aClientIf,
                      const nsAString& aBdAddr,
                      BluetoothGattClientResultHandler* aRes);

  void GetDeviceType(const nsAString& aBdAddr,
                     BluetoothGattClientResultHandler* aRes);

  /* Set advertising data or scan response data */
  void SetAdvData(int aServerIf,
                  bool aIsScanRsp,
                  bool aIsNameIncluded,
                  bool aIsTxPowerIncluded,
                  int aMinInterval,
                  int aMaxInterval,
                  int aApperance,
                  uint16_t aManufacturerLen, char* aManufacturerData,
                  uint16_t aServiceDataLen, char* aServiceData,
                  uint16_t aServiceUUIDLen, char* aServiceUUID,
                  BluetoothGattClientResultHandler* aRes);

  void TestCommand(int aCommand, const BluetoothGattTestParam& aTestParam,
                   BluetoothGattClientResultHandler* aRes);

protected:
  BluetoothGattClientHALInterface(
#if ANDROID_VERSION >= 19
    const btgatt_client_interface_t* aInterface
#endif
    );
  ~BluetoothGattClientHALInterface();

private:
#if ANDROID_VERSION >= 19
  const btgatt_client_interface_t* mInterface;
#endif
};

class BluetoothGattServerHALInterface final
  : public BluetoothGattServerInterface
{
public:
  friend class BluetoothGattHALInterface;

  /* Register / Unregister */
  void RegisterServer(const BluetoothUuid& aUuid,
                      BluetoothGattServerResultHandler* aRes);
  void UnregisterServer(int aServerIf,
                        BluetoothGattServerResultHandler* aRes);

  /* Connect / Disconnect */
  void ConnectPeripheral(int aServerIf,
                         const nsAString& aBdAddr,
                         bool aIsDirect, /* auto connect */
                         BluetoothTransport aTransport,
                         BluetoothGattServerResultHandler* aRes);
  void DisconnectPeripheral(int aServerIf,
                            const nsAString& aBdAddr,
                            int aConnId,
                            BluetoothGattServerResultHandler* aRes);

  /* Add a services / a characteristic / a descriptor */
  void AddService(int aServerIf,
                  const BluetoothGattServiceId& aServiceId,
                  int aNumHandles,
                  BluetoothGattServerResultHandler* aRes);
  void AddIncludedService(int aServerIf,
                          int aServiceHandle,
                          int aIncludedServiceHandle,
                          BluetoothGattServerResultHandler* aRes);
  void AddCharacteristic(int aServerIf,
                         int aServiceHandle,
                         const BluetoothUuid& aUuid,
                         BluetoothGattCharProp aProperties,
                         BluetoothGattAttrPerm aPermissions,
                         BluetoothGattServerResultHandler* aRes);
  void AddDescriptor(int aServerIf,
                     int aServiceHandle,
                     const BluetoothUuid& aUuid,
                     BluetoothGattAttrPerm aPermissions,
                     BluetoothGattServerResultHandler* aRes);

  /* Start / Stop / Delete a service */
  void StartService(int aServerIf,
                    int aServiceHandle,
                    BluetoothTransport aTransport,
                    BluetoothGattServerResultHandler* aRes);
  void StopService(int aServerIf,
                   int aServiceHandle,
                   BluetoothGattServerResultHandler* aRes);
  void DeleteService(int aServerIf,
                     int aServiceHandle,
                     BluetoothGattServerResultHandler* aRes);

  /* Send an indication or a notification */
  void SendIndication(int aServerIf,
                      int aAttributeHandle,
                      int aConnId,
                      const nsTArray<uint8_t>& aValue,
                      bool aConfirm, /* true: indication, false: notification */
                      BluetoothGattServerResultHandler* aRes);

  /* Send a response for an incoming indication */
  void SendResponse(int aConnId,
                    int aTransId,
                    BluetoothGattStatus aStatus,
                    const BluetoothGattResponse& aResponse,
                    BluetoothGattServerResultHandler* aRes);

protected:
  BluetoothGattServerHALInterface(
#if ANDROID_VERSION >= 19
    const btgatt_server_interface_t* aInterface
#endif
    );
  ~BluetoothGattServerHALInterface();

private:
#if ANDROID_VERSION >= 19
  const btgatt_server_interface_t* mInterface;
#endif
};

class BluetoothGattHALInterface final
 : public BluetoothGattInterface
{
public:
  friend class BluetoothHALInterface;

  void Init(BluetoothGattNotificationHandler* aNotificationHandler,
            BluetoothGattResultHandler* aRes);
  void Cleanup(BluetoothGattResultHandler* aRes);

  BluetoothGattClientInterface* GetBluetoothGattClientInterface();
  BluetoothGattServerInterface* GetBluetoothGattServerInterface();

protected:
  BluetoothGattHALInterface(
#if ANDROID_VERSION >= 19
    const btgatt_interface_t* aInterface
#endif
    );
  ~BluetoothGattHALInterface();

private:
#if ANDROID_VERSION >= 19
  const btgatt_interface_t* mInterface;
#endif
};

END_BLUETOOTH_NAMESPACE

#endif
