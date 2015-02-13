/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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
  class CreateBondResultHandler;
  class DisableResultHandler;
  class EnableResultHandler;
  class GetRemoteDevicePropertiesResultHandler;
  class GetRemoteServicesResultHandler;
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

  virtual nsresult StartInternal(BluetoothReplyRunnable* aRunnable);
  virtual nsresult StopInternal(BluetoothReplyRunnable* aRunnable);

  virtual nsresult
  GetAdaptersInternal(BluetoothReplyRunnable* aRunnable);

  virtual nsresult
  GetConnectedDevicePropertiesInternal(uint16_t aProfileId,
                                       BluetoothReplyRunnable* aRunnable);

  virtual nsresult
  GetPairedDevicePropertiesInternal(const nsTArray<nsString>& aDeviceAddress,
                                    BluetoothReplyRunnable* aRunnable);

  virtual nsresult
  FetchUuidsInternal(const nsAString& aDeviceAddress,
                     BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual nsresult StartDiscoveryInternal(BluetoothReplyRunnable* aRunnable);
  virtual nsresult StopDiscoveryInternal(BluetoothReplyRunnable* aRunnable);

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

  virtual void
  SetPinCodeInternal(const nsAString& aDeviceAddress, const nsAString& aPinCode,
                     BluetoothReplyRunnable* aRunnable);

  virtual void
  SetPasskeyInternal(const nsAString& aDeviceAddress, uint32_t aPasskey,
                     BluetoothReplyRunnable* aRunnable);

  virtual void
  SetPairingConfirmationInternal(const nsAString& aDeviceAddress, bool aConfirm,
                                 BluetoothReplyRunnable* aRunnable);

  virtual void
  Connect(const nsAString& aDeviceAddress,
          uint32_t aCod,
          uint16_t aServiceUuid,
          BluetoothReplyRunnable* aRunnable);

  virtual bool
  IsConnected(uint16_t aProfileId);

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
           nsIDOMBlob* aBlob,
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
               BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void
  SendPlayStatus(int64_t aDuration,
                 int64_t aPosition,
                 const nsAString& aPlayStatus,
                 BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void
  UpdatePlayStatus(uint32_t aDuration,
                   uint32_t aPosition,
                   ControlPlayStatus aPlayStatus) MOZ_OVERRIDE;

  virtual nsresult
  SendSinkMessage(const nsAString& aDeviceAddresses,
                  const nsAString& aMessage) MOZ_OVERRIDE;

  virtual nsresult
  SendInputMessage(const nsAString& aDeviceAddresses,
                   const nsAString& aMessage) MOZ_OVERRIDE;

  //
  // GATT Client
  //

  virtual void
  ConnectGattClientInternal(const nsAString& aAppUuid,
                            const nsAString& aDeviceAddress,
                            BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void
  DisconnectGattClientInternal(const nsAString& aAppUuid,
                               const nsAString& aDeviceAddress,
                               BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  virtual void
  UnregisterGattClientInternal(int aClientIf,
                               BluetoothReplyRunnable* aRunnable) MOZ_OVERRIDE;

  //
  // Bluetooth notifications
  //

  virtual void AdapterStateChangedNotification(bool aState) MOZ_OVERRIDE;
  virtual void AdapterPropertiesNotification(
    BluetoothStatus aStatus, int aNumProperties,
    const BluetoothProperty* aProperties) MOZ_OVERRIDE;

  virtual void RemoteDevicePropertiesNotification(
    BluetoothStatus aStatus, const nsAString& aBdAddr,
    int aNumProperties, const BluetoothProperty* aProperties) MOZ_OVERRIDE;

  virtual void DeviceFoundNotification(
    int aNumProperties, const BluetoothProperty* aProperties) MOZ_OVERRIDE;

  virtual void DiscoveryStateChangedNotification(bool aState) MOZ_OVERRIDE;

  virtual void PinRequestNotification(const nsAString& aRemoteBdAddr,
                                      const nsAString& aBdName,
                                      uint32_t aCod) MOZ_OVERRIDE;
  virtual void SspRequestNotification(const nsAString& aRemoteBdAddr,
                                      const nsAString& aBdName,
                                      uint32_t aCod,
                                      BluetoothSspVariant aPairingVariant,
                                      uint32_t aPasskey) MOZ_OVERRIDE;

  virtual void BondStateChangedNotification(
    BluetoothStatus aStatus, const nsAString& aRemoteBdAddr,
    BluetoothBondState aState) MOZ_OVERRIDE;
  virtual void AclStateChangedNotification(BluetoothStatus aStatus,
                                           const nsAString& aRemoteBdAddr,
                                           bool aState) MOZ_OVERRIDE;

  virtual void DutModeRecvNotification(uint16_t aOpcode,
                                       const uint8_t* aBuf,
                                       uint8_t aLen) MOZ_OVERRIDE;
  virtual void LeTestModeNotification(BluetoothStatus aStatus,
                                      uint16_t aNumPackets) MOZ_OVERRIDE;

protected:
  static nsresult StartGonkBluetooth();
  static nsresult StopGonkBluetooth();
  static bool EnsureBluetoothHalLoad();

  static void ConnectDisconnect(bool aConnect,
                                const nsAString& aDeviceAddress,
                                BluetoothReplyRunnable* aRunnable,
                                uint16_t aServiceUuid, uint32_t aCod = 0);
  static void NextBluetoothProfileController();
  static ControlPlayStatus PlayStatusStringToControlPlayStatus(
    const nsAString& aPlayStatus);
  static void ReplyStatusError(BluetoothReplyRunnable* aReplyRunnable,
                               BluetoothStatus aStatusCode,
                               const nsAString& aCustomMsg);
};

END_BLUETOOTH_NAMESPACE

#endif

