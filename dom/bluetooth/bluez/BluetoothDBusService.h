/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluez_BluetoothDBusService_h
#define mozilla_dom_bluetooth_bluez_BluetoothDBusService_h

#include "mozilla/Attributes.h"
#include "BluetoothCommon.h"
#include "mozilla/ipc/RawDBusConnection.h"
#include "BluetoothService.h"
#include "nsIThread.h"

class DBusMessage;

BEGIN_BLUETOOTH_NAMESPACE

/**
 * BluetoothDBusService is the implementation of BluetoothService for DBus on
 * linux/android/B2G. Function comments are in BluetoothService.h
 */

class BluetoothDBusService : public BluetoothService
{
public:
  /**
   * For DBus Control method of "UpdateNotification", event id should be
   * specified as following:
   * (Please see specification of AVRCP 1.3, Table 5.28 for more details.)
   */
  enum ControlEventId {
    EVENT_PLAYBACK_STATUS_CHANGED            = 0x01,
    EVENT_TRACK_CHANGED                      = 0x02,
    EVENT_TRACK_REACHED_END                  = 0x03,
    EVENT_TRACK_REACHED_START                = 0x04,
    EVENT_PLAYBACK_POS_CHANGED               = 0x05,
    EVENT_BATT_STATUS_CHANGED                = 0x06,
    EVENT_SYSTEM_STATUS_CHANGED              = 0x07,
    EVENT_PLAYER_APPLICATION_SETTING_CHANGED = 0x08,
    EVENT_UNKNOWN
  };

  BluetoothDBusService();
  ~BluetoothDBusService();

  bool IsReady();

  virtual nsresult StartInternal(BluetoothReplyRunnable* aRunnable) override;

  virtual nsresult StopInternal(BluetoothReplyRunnable* aRunnable) override;

  virtual nsresult
  GetAdaptersInternal(BluetoothReplyRunnable* aRunnable) override;

  virtual nsresult
  GetConnectedDevicePropertiesInternal(
    uint16_t aServiceUuid, BluetoothReplyRunnable* aRunnable) override;

  virtual nsresult
  GetPairedDevicePropertiesInternal(
    const nsTArray<BluetoothAddress>& aDeviceAddresses,
    BluetoothReplyRunnable* aRunnable) override;

  virtual nsresult
  FetchUuidsInternal(const BluetoothAddress& aDeviceAddress,
                     BluetoothReplyRunnable* aRunnable) override;

  virtual void
  StartDiscoveryInternal(BluetoothReplyRunnable* aRunnable) override;

  virtual void
  StopDiscoveryInternal(BluetoothReplyRunnable* aRunnable) override;

  virtual nsresult
  SetProperty(BluetoothObjectType aType,
              const BluetoothNamedValue& aValue,
              BluetoothReplyRunnable* aRunnable) override;

  virtual nsresult
  GetServiceChannel(const BluetoothAddress& aDeviceAddress,
                    const BluetoothUuid& aServiceUuid,
                    BluetoothProfileManagerBase* aManager) override;

  virtual bool
  UpdateSdpRecords(const BluetoothAddress& aDeviceAddress,
                   BluetoothProfileManagerBase* aManager) override;

  virtual nsresult
  CreatePairedDeviceInternal(const BluetoothAddress& aDeviceAddress,
                             int aTimeout,
                             BluetoothReplyRunnable* aRunnable) override;

  virtual nsresult
  RemoveDeviceInternal(const BluetoothAddress& aDeviceAddress,
                       BluetoothReplyRunnable* aRunnable) override;

  virtual void
  PinReplyInternal(const BluetoothAddress& aDeviceAddress,
                   bool aAccept,
                   const BluetoothPinCode& aPinCode,
                   BluetoothReplyRunnable* aRunnable);

  virtual void
  SspReplyInternal(const BluetoothAddress& aDeviceAddress,
                   BluetoothSspVariant aVariant,
                   bool aAccept,
                   BluetoothReplyRunnable* aRunnable);

  virtual void
  SetPinCodeInternal(const BluetoothAddress& aDeviceAddress,
                     const BluetoothPinCode& aPinCode,
                     BluetoothReplyRunnable* aRunnable) override;

  virtual void
  SetPasskeyInternal(const BluetoothAddress& aDeviceAddress, uint32_t aPasskey,
                     BluetoothReplyRunnable* aRunnable) override;

  virtual void
  SetPairingConfirmationInternal(const BluetoothAddress& aDeviceAddress,
                                 bool aConfirm,
                                 BluetoothReplyRunnable* aRunnable) override;

  virtual void
  Connect(const BluetoothAddress& aDeviceAddress,
          uint32_t aCod,
          uint16_t aServiceUuid,
          BluetoothReplyRunnable* aRunnable) override;

  virtual void
  Disconnect(const BluetoothAddress& aDeviceAddress, uint16_t aServiceUuid,
             BluetoothReplyRunnable* aRunnable) override;

  virtual void
  SendFile(const BluetoothAddress& aDeviceAddress,
           BlobParent* aBlobParent,
           BlobChild* aBlobChild,
           BluetoothReplyRunnable* aRunnable) override;

  virtual void
  SendFile(const BluetoothAddress& aDeviceAddress,
           Blob* aBlob,
           BluetoothReplyRunnable* aRunnable) override;

  virtual void
  StopSendingFile(const BluetoothAddress& aDeviceAddress,
                  BluetoothReplyRunnable* aRunnable) override;

  virtual void
  ConfirmReceivingFile(const BluetoothAddress& aDeviceAddress, bool aConfirm,
                       BluetoothReplyRunnable* aRunnable) override;

  virtual void
  ConnectSco(BluetoothReplyRunnable* aRunnable) override;

  virtual void
  DisconnectSco(BluetoothReplyRunnable* aRunnable) override;

  virtual void
  IsScoConnected(BluetoothReplyRunnable* aRunnable) override;

  virtual void
  SetObexPassword(const nsAString& aPassword,
                  BluetoothReplyRunnable* aRunnable) override;

  virtual void
  RejectObexAuth(BluetoothReplyRunnable* aRunnable) override;

  virtual void
  ReplyTovCardPulling(BlobParent* aBlobParent,
                      BlobChild* aBlobChild,
                      BluetoothReplyRunnable* aRunnable) override;

  virtual void
  ReplyTovCardPulling(Blob* aBlob,
                      BluetoothReplyRunnable* aRunnable) override;

  virtual void
  ReplyToPhonebookPulling(BlobParent* aBlobParent,
                          BlobChild* aBlobChild,
                          uint16_t aPhonebookSize,
                          BluetoothReplyRunnable* aRunnable) override;

  virtual void
  ReplyToPhonebookPulling(Blob* aBlob,
                          uint16_t aPhonebookSize,
                          BluetoothReplyRunnable* aRunnable) override;

  virtual void
  ReplyTovCardListing(BlobParent* aBlobParent,
                      BlobChild* aBlobChild,
                      uint16_t aPhonebookSize,
                      BluetoothReplyRunnable* aRunnable) override;

  virtual void
  ReplyTovCardListing(Blob* aBlob,
                      uint16_t aPhonebookSize,
                      BluetoothReplyRunnable* aRunnable) override;

  virtual void
  ReplyToMapFolderListing(long aMasId, const nsAString& aFolderlists,
                          BluetoothReplyRunnable* aRunnable) override;

  virtual void
  ReplyToMapMessagesListing(BlobParent* aBlobParent, BlobChild* aBlobChild,
                            long aMasId,
                            bool aNewMessage,
                            const nsAString& aTimestamp,
                            int aSize,
                            BluetoothReplyRunnable* aRunnable) override;

  virtual void
  ReplyToMapMessagesListing(long aMasId,
                            Blob* aBlob,
                            bool aNewMessage,
                            const nsAString& aTimestamp,
                            int aSize,
                            BluetoothReplyRunnable* aRunnable) override;

  virtual void
  ReplyToMapGetMessage(BlobParent* aBlobParent,
                       BlobChild* aBlobChild,
                       long aMasId,
                       BluetoothReplyRunnable* aRunnable) override;

  virtual void
  ReplyToMapGetMessage(Blob* aBlob,
                       long aMasId,
                       BluetoothReplyRunnable* aRunnable) override;

  virtual void
  ReplyToMapSetMessageStatus(long aMasId,
                             bool aStatus,
                             BluetoothReplyRunnable* aRunnable) override;

  virtual void
  ReplyToMapSendMessage(long aMasId,
                        const nsAString& aHandleId,
                        bool aStatus,
                        BluetoothReplyRunnable* aRunnable) override;

  virtual void
  ReplyToMapMessageUpdate(long aMasId,
                          bool aStatus,
                          BluetoothReplyRunnable* aRunnable) override;

#ifdef MOZ_B2G_RIL
  virtual void
  AnswerWaitingCall(BluetoothReplyRunnable* aRunnable);

  virtual void
  IgnoreWaitingCall(BluetoothReplyRunnable* aRunnable);

  virtual void
  ToggleCalls(BluetoothReplyRunnable* aRunnable);
#endif

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
                 ControlPlayStatus aPlayStatus,
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

  virtual void
  StartLeScanInternal(const nsTArray<BluetoothUuid>& aServiceUuids,
                      BluetoothReplyRunnable* aRunnable) override;

  virtual void
  StopLeScanInternal(const BluetoothUuid& aAppUuid,
                     BluetoothReplyRunnable* aRunnable) override;

  virtual void
  ConnectGattClientInternal(const BluetoothUuid& aAppUuid,
                            const BluetoothAddress& aDeviceAddress,
                            BluetoothReplyRunnable* aRunnable) override;

  virtual void
  DisconnectGattClientInternal(const BluetoothUuid& aAppUuid,
                               const BluetoothAddress& aDeviceAddress,
                               BluetoothReplyRunnable* aRunnable) override;

  virtual void
  DiscoverGattServicesInternal(const BluetoothUuid& aAppUuid,
                               BluetoothReplyRunnable* aRunnable) override;

  virtual void
  GattClientStartNotificationsInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothGattServiceId& aServId,
    const BluetoothGattId& aCharId,
    BluetoothReplyRunnable* aRunnable) override;

  virtual void
  GattClientStopNotificationsInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothGattServiceId& aServId,
    const BluetoothGattId& aCharId,
    BluetoothReplyRunnable* aRunnable) override;

  virtual void
  UnregisterGattClientInternal(int aClientIf,
                               BluetoothReplyRunnable* aRunnable) override;

  virtual void
  GattClientReadRemoteRssiInternal(
    int aClientIf, const BluetoothAddress& aDeviceAddress,
    BluetoothReplyRunnable* aRunnable) override;

  virtual void
  GattClientReadCharacteristicValueInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharacteristicId,
    BluetoothReplyRunnable* aRunnable) override;

  virtual void
  GattClientWriteCharacteristicValueInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharacteristicId,
    const BluetoothGattWriteType& aWriteType,
    const nsTArray<uint8_t>& aValue,
    BluetoothReplyRunnable* aRunnable) override;

  virtual void
  GattClientReadDescriptorValueInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharacteristicId,
    const BluetoothGattId& aDescriptorId,
    BluetoothReplyRunnable* aRunnable) override;

  virtual void
  GattClientWriteDescriptorValueInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharacteristicId,
    const BluetoothGattId& aDescriptorId,
    const nsTArray<uint8_t>& aValue,
    BluetoothReplyRunnable* aRunnable) override;

  virtual void
  GattServerConnectPeripheralInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothAddress& aAddress,
    BluetoothReplyRunnable* aRunnable) override;

  virtual void
  GattServerDisconnectPeripheralInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothAddress& aAddress,
    BluetoothReplyRunnable* aRunnable) override;

  virtual void
  UnregisterGattServerInternal(int aServerIf,
                               BluetoothReplyRunnable* aRunnable) override;

  virtual void
  GattServerAddServiceInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothGattServiceId& aServiceId,
    uint16_t aHandleCount,
    BluetoothReplyRunnable* aRunnable) override;

  virtual void
  GattServerAddIncludedServiceInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothAttributeHandle& aServiceHandle,
    const BluetoothAttributeHandle& aIncludedServiceHandle,
    BluetoothReplyRunnable* aRunnable) override;

  virtual void
  GattServerAddCharacteristicInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothAttributeHandle& aServiceHandle,
    const BluetoothUuid& aCharacteristicUuid,
    BluetoothGattAttrPerm aPermissions,
    BluetoothGattCharProp aProperties,
    BluetoothReplyRunnable* aRunnable) override;

  virtual void
  GattServerAddDescriptorInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothAttributeHandle& aServiceHandle,
    const BluetoothAttributeHandle& aCharacteristicHandle,
    const BluetoothUuid& aDescriptorUuid,
    BluetoothGattAttrPerm aPermissions,
    BluetoothReplyRunnable* aRunnable) override;

  virtual void
  GattServerRemoveServiceInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothAttributeHandle& aServiceHandle,
    BluetoothReplyRunnable* aRunnable) override;

  virtual void
  GattServerStartServiceInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothAttributeHandle& aServiceHandle,
    BluetoothReplyRunnable* aRunnable) override;

  virtual void
  GattServerStopServiceInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothAttributeHandle& aServiceHandle,
    BluetoothReplyRunnable* aRunnable) override;

  virtual void
  GattServerSendResponseInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothAddress& aAddress,
    uint16_t aStatus,
    int32_t aRequestId,
    const BluetoothGattResponse& aRsp,
    BluetoothReplyRunnable* aRunnable) override;

  virtual void
  GattServerSendIndicationInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothAddress& aAddress,
    const BluetoothAttributeHandle& aCharacteristicHandle,
    bool aConfirm,
    const nsTArray<uint8_t>& aValue,
    BluetoothReplyRunnable* aRunnable) override;

private:
  nsresult SendGetPropertyMessage(const nsAString& aPath,
                                  const char* aInterface,
                                  void (*aCB)(DBusMessage *, void *),
                                  BluetoothReplyRunnable* aRunnable);

  nsresult SendDiscoveryMessage(const char* aMessageName,
                                BluetoothReplyRunnable* aRunnable);

  nsresult SendSetPropertyMessage(const char* aInterface,
                                  const BluetoothNamedValue& aValue,
                                  BluetoothReplyRunnable* aRunnable);

  void UpdateNotification(ControlEventId aEventId, uint64_t aData);

  nsresult SendAsyncDBusMessage(const nsAString& aObjectPath,
                                const char* aInterface,
                                const nsAString& aMessage,
                                mozilla::ipc::DBusReplyCallback aCallback);
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluez_BluetoothDBusService_h
