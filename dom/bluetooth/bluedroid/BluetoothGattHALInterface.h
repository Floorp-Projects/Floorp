/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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
                  uint8_t aManufacturerLen,
                  const ArrayBuffer& aManufacturerData,
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

// TODO: Add server interface

class BluetoothGattHALInterface final
 : public BluetoothGattInterface
{
public:
  friend class BluetoothHALInterface;

  void Init(BluetoothGattNotificationHandler* aNotificationHandler,
            BluetoothGattResultHandler* aRes);
  void Cleanup(BluetoothGattResultHandler* aRes);

  BluetoothGattClientInterface* GetBluetoothGattClientInterface();

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
