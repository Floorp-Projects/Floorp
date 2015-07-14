/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothservicebluedroid_h__
#define mozilla_dom_bluetooth_bluetoothservicebluedroid_h__

#include "BluetoothCommon.h"
#include "BluetoothInterface.h"
#include "BluetoothService.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothServiceBluedroid : public BluetoothService
                                , public BluetoothNotificationHandler
{
  class CancelDiscoveryResultHandler;
  class CleanupResultHandler;
  class CreateBondResultHandler;
  class DisableResultHandler;
  class EnableResultHandler;
  class GetRemoteDevicePropertiesResultHandler;
#ifndef MOZ_B2G_BT_API_V1
  class GetRemoteServicesResultHandler;
#else
  // Missing in Bluetooth v1
#endif
  class InitResultHandler;
  class PinReplyResultHandler;
  class ProfileDeinitResultHandler;
  class ProfileInitResultHandler;
  class RemoveBondResultHandler;
  class SetAdapterPropertyDiscoverableResultHandler;
  class SetAdapterPropertyResultHandler;
  class SspReplyResultHandler;
  class StartDiscoveryResultHandler;

public:
  BluetoothServiceBluedroid();
  ~BluetoothServiceBluedroid();

#ifndef MOZ_B2G_BT_API_V1
  virtual nsresult StartInternal(BluetoothReplyRunnable* aRunnable);
  virtual nsresult StopInternal(BluetoothReplyRunnable* aRunnable);
#else
  virtual nsresult StartInternal();
  virtual nsresult StopInternal();
#endif

#ifndef MOZ_B2G_BT_API_V1
  virtual nsresult GetAdaptersInternal(BluetoothReplyRunnable* aRunnable);
#else
  virtual nsresult GetDefaultAdapterPathInternal(
    BluetoothReplyRunnable* aRunnable);
#endif

  virtual nsresult
  GetConnectedDevicePropertiesInternal(uint16_t aProfileId,
                                       BluetoothReplyRunnable* aRunnable);

  virtual nsresult
  GetPairedDevicePropertiesInternal(const nsTArray<nsString>& aDeviceAddress,
                                    BluetoothReplyRunnable* aRunnable);

#ifndef MOZ_B2G_BT_API_V1
  virtual nsresult
  FetchUuidsInternal(const nsAString& aDeviceAddress,
                     BluetoothReplyRunnable* aRunnable) override;
#else
  // Missing in Bluetooth v1
#endif

  virtual void StartDiscoveryInternal(BluetoothReplyRunnable* aRunnable);
  virtual void StopDiscoveryInternal(BluetoothReplyRunnable* aRunnable);

  virtual nsresult
  SetProperty(BluetoothObjectType aType,
              const BluetoothNamedValue& aValue,
              BluetoothReplyRunnable* aRunnable);

  virtual nsresult
  GetServiceChannel(const nsAString& aDeviceAddress,
                    const nsAString& aServiceUuid,
                    BluetoothProfileManagerBase* aManager);

  virtual bool
  UpdateSdpRecords(const nsAString& aDeviceAddress,
                   BluetoothProfileManagerBase* aManager);

  virtual nsresult
  CreatePairedDeviceInternal(const nsAString& aDeviceAddress,
                             int aTimeout,
                             BluetoothReplyRunnable* aRunnable);

  virtual nsresult
  RemoveDeviceInternal(const nsAString& aDeviceObjectPath,
                       BluetoothReplyRunnable* aRunnable);

#ifndef MOZ_B2G_BT_API_V1
  virtual void
  PinReplyInternal(const nsAString& aDeviceAddress,
                   bool aAccept,
                   const nsAString& aPinCode,
                   BluetoothReplyRunnable* aRunnable);

  virtual void
  SspReplyInternal(const nsAString& aDeviceAddress,
                   BluetoothSspVariant aVariant,
                   bool aAccept,
                   BluetoothReplyRunnable* aRunnable);
  virtual void
  SetPinCodeInternal(const nsAString& aDeviceAddress,
                     const nsAString& aPinCode,
                     BluetoothReplyRunnable* aRunnable);

  virtual void
  SetPasskeyInternal(const nsAString& aDeviceAddress,
                     uint32_t aPasskey,
                     BluetoothReplyRunnable* aRunnable);

  virtual void
  SetPairingConfirmationInternal(const nsAString& aDeviceAddress,
                                 bool aConfirm,
                                 BluetoothReplyRunnable* aRunnable);
#else
  virtual bool
  SetAuthorizationInternal(const nsAString& aDeviceAddress, bool aAllow,
                           BluetoothReplyRunnable* aRunnable);

  virtual bool
  SetPinCodeInternal(const nsAString& aDeviceAddress,
                     const nsAString& aPinCode,
                     BluetoothReplyRunnable* aRunnable);

  virtual bool
  SetPasskeyInternal(const nsAString& aDeviceAddress,
                     uint32_t aPasskey,
                     BluetoothReplyRunnable* aRunnable);

  virtual bool
  SetPairingConfirmationInternal(const nsAString& aDeviceAddress,
                                 bool aConfirm,
                                 BluetoothReplyRunnable* aRunnable);
#endif

#ifndef MOZ_B2G_BT_API_V1
  // Missing in Bluetooth v2
#else
  virtual nsresult
  PrepareAdapterInternal();
#endif

  virtual void
  Connect(const nsAString& aDeviceAddress,
          uint32_t aCod,
          uint16_t aServiceUuid,
          BluetoothReplyRunnable* aRunnable);

#ifndef MOZ_B2G_BT_API_V1
  virtual bool
  IsConnected(uint16_t aProfileId);
#else
  virtual void
  IsConnected(const uint16_t aServiceUuid,
              BluetoothReplyRunnable* aRunnable) override;
#endif

  virtual void
  Disconnect(const nsAString& aDeviceAddress, uint16_t aServiceUuid,
             BluetoothReplyRunnable* aRunnable);

  virtual void
  SendFile(const nsAString& aDeviceAddress,
           BlobParent* aBlobParent,
           BlobChild* aBlobChild,
           BluetoothReplyRunnable* aRunnable);

  virtual void
  SendFile(const nsAString& aDeviceAddress,
           Blob* aBlob,
           BluetoothReplyRunnable* aRunnable);

  virtual void
  StopSendingFile(const nsAString& aDeviceAddress,
                  BluetoothReplyRunnable* aRunnable);

  virtual void
  ConfirmReceivingFile(const nsAString& aDeviceAddress, bool aConfirm,
                       BluetoothReplyRunnable* aRunnable);

  virtual void
  ConnectSco(BluetoothReplyRunnable* aRunnable);

  virtual void
  DisconnectSco(BluetoothReplyRunnable* aRunnable);

  virtual void
  IsScoConnected(BluetoothReplyRunnable* aRunnable);

  virtual void
  AnswerWaitingCall(BluetoothReplyRunnable* aRunnable);

  virtual void
  IgnoreWaitingCall(BluetoothReplyRunnable* aRunnable);

  virtual void
  ToggleCalls(BluetoothReplyRunnable* aRunnable);

  virtual void
  SendMetaData(const nsAString& aTitle,
               const nsAString& aArtist,
               const nsAString& aAlbum,
               int64_t aMediaNumber,
               int64_t aTotalMediaCount,
               int64_t aDuration,
               BluetoothReplyRunnable* aRunnable) override;

  virtual void
  SendPlayStatus(int64_t aDuration,
                 int64_t aPosition,
                 const nsAString& aPlayStatus,
                 BluetoothReplyRunnable* aRunnable) override;

  virtual void
  UpdatePlayStatus(uint32_t aDuration,
                   uint32_t aPosition,
                   ControlPlayStatus aPlayStatus) override;

  virtual nsresult
  SendSinkMessage(const nsAString& aDeviceAddresses,
                  const nsAString& aMessage) override;

  virtual nsresult
  SendInputMessage(const nsAString& aDeviceAddresses,
                   const nsAString& aMessage) override;

  //
  // GATT Client
  //

#ifndef MOZ_B2G_BT_API_V1
  virtual void StartLeScanInternal(const nsTArray<nsString>& aServiceUuids,
                                   BluetoothReplyRunnable* aRunnable);

  virtual void StopLeScanInternal(const nsAString& aScanUuid,
                                  BluetoothReplyRunnable* aRunnable);

  virtual void
  ConnectGattClientInternal(const nsAString& aAppUuid,
                            const nsAString& aDeviceAddress,
                            BluetoothReplyRunnable* aRunnable) override;

  virtual void
  DisconnectGattClientInternal(const nsAString& aAppUuid,
                               const nsAString& aDeviceAddress,
                               BluetoothReplyRunnable* aRunnable) override;

  virtual void
  DiscoverGattServicesInternal(const nsAString& aAppUuid,
                               BluetoothReplyRunnable* aRunnable) override;

  virtual void
  GattClientStartNotificationsInternal(
    const nsAString& aAppUuid,
    const BluetoothGattServiceId& aServId,
    const BluetoothGattId& aCharId,
    BluetoothReplyRunnable* aRunnable) override;

  virtual void
  GattClientStopNotificationsInternal(
    const nsAString& aAppUuid,
    const BluetoothGattServiceId& aServId,
    const BluetoothGattId& aCharId,
    BluetoothReplyRunnable* aRunnable) override;

  virtual void
  UnregisterGattClientInternal(int aClientIf,
                               BluetoothReplyRunnable* aRunnable) override;

  virtual void
  GattClientReadRemoteRssiInternal(
    int aClientIf, const nsAString& aDeviceAddress,
    BluetoothReplyRunnable* aRunnable) override;

  virtual void
  GattClientReadCharacteristicValueInternal(
    const nsAString& aAppUuid,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharacteristicId,
    BluetoothReplyRunnable* aRunnable) override;

  virtual void
  GattClientWriteCharacteristicValueInternal(
    const nsAString& aAppUuid,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharacteristicId,
    const BluetoothGattWriteType& aWriteType,
    const nsTArray<uint8_t>& aValue,
    BluetoothReplyRunnable* aRunnable) override;

  virtual void
  GattClientReadDescriptorValueInternal(
    const nsAString& aAppUuid,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharacteristicId,
    const BluetoothGattId& aDescriptorId,
    BluetoothReplyRunnable* aRunnable) override;

  virtual void
  GattClientWriteDescriptorValueInternal(
    const nsAString& aAppUuid,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharacteristicId,
    const BluetoothGattId& aDescriptorId,
    const nsTArray<uint8_t>& aValue,
    BluetoothReplyRunnable* aRunnable) override;
#else
  // Missing in Bluetooth v1
#endif

  //
  // Bluetooth notifications
  //

  virtual void AdapterStateChangedNotification(bool aState) override;
  virtual void AdapterPropertiesNotification(
    BluetoothStatus aStatus, int aNumProperties,
    const BluetoothProperty* aProperties) override;

  virtual void RemoteDevicePropertiesNotification(
    BluetoothStatus aStatus, const nsAString& aBdAddr,
    int aNumProperties, const BluetoothProperty* aProperties) override;

  virtual void DeviceFoundNotification(
    int aNumProperties, const BluetoothProperty* aProperties) override;

  virtual void DiscoveryStateChangedNotification(bool aState) override;

  virtual void PinRequestNotification(const nsAString& aRemoteBdAddr,
                                      const nsAString& aBdName,
                                      uint32_t aCod) override;
  virtual void SspRequestNotification(const nsAString& aRemoteBdAddr,
                                      const nsAString& aBdName,
                                      uint32_t aCod,
                                      BluetoothSspVariant aPairingVariant,
                                      uint32_t aPasskey) override;

  virtual void BondStateChangedNotification(
    BluetoothStatus aStatus, const nsAString& aRemoteBdAddr,
    BluetoothBondState aState) override;
  virtual void AclStateChangedNotification(BluetoothStatus aStatus,
                                           const nsAString& aRemoteBdAddr,
                                           bool aState) override;

  virtual void DutModeRecvNotification(uint16_t aOpcode,
                                       const uint8_t* aBuf,
                                       uint8_t aLen) override;
  virtual void LeTestModeNotification(BluetoothStatus aStatus,
                                      uint16_t aNumPackets) override;

  virtual void EnergyInfoNotification(
    const BluetoothActivityEnergyInfo& aInfo) override;

  virtual void BackendErrorNotification(bool aCrashed) override;

  virtual void CompleteToggleBt(bool aEnabled) override;

protected:
  static nsresult StartGonkBluetooth();
  static nsresult StopGonkBluetooth();

  static ControlPlayStatus PlayStatusStringToControlPlayStatus(
    const nsAString& aPlayStatus);

  static void ConnectDisconnect(bool aConnect,
                                const nsAString& aDeviceAddress,
                                BluetoothReplyRunnable* aRunnable,
                                uint16_t aServiceUuid, uint32_t aCod = 0);
  static void NextBluetoothProfileController();

#ifndef MOZ_B2G_BT_API_V1
  // Missing in Bluetooth v2
#else
  static bool EnsureBluetoothHalLoad();

  static void ClassToIcon(uint32_t aClass, nsAString& aRetIcon);

  uint16_t UuidToServiceClassInt(const BluetoothUuid& mUuid);

  static bool IsConnected(const nsAString& aRemoteBdAddr);
#endif
};

END_BLUETOOTH_NAMESPACE

#endif
