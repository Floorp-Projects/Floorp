/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothService_h
#define mozilla_dom_bluetooth_BluetoothService_h

#include "BluetoothCommon.h"
#include "BluetoothInterface.h"
#include "BluetoothProfileManagerBase.h"
#include "nsClassHashtable.h"
#include "nsIObserver.h"
#include "nsTObserverArray.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {
class Blob;
class BlobChild;
class BlobParent;
}
}

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothManager;
class BluetoothNamedValue;
class BluetoothReplyRunnable;
class BluetoothSignal;

typedef mozilla::ObserverList<BluetoothSignal> BluetoothSignalObserverList;

class BluetoothService : public nsIObserver
{
  class ToggleBtTask;
  friend class ToggleBtTask;

  class StartupTask;
  friend class StartupTask;

public:
  class ToggleBtAck : public Runnable
  {
  public:
    ToggleBtAck(bool aEnabled);
    NS_IMETHOD Run();

  private:
    bool mEnabled;
  };
  friend class ToggleBtAck;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  /**
   * Add a message handler object from message distribution observer.
   * Must be called from the main thread.
   *
   * @param aNodeName Node name of the object
   * @param aMsgHandler Weak pointer to the object
   */
  virtual void
  RegisterBluetoothSignalHandler(const nsAString& aNodeName,
                                 BluetoothSignalObserver* aMsgHandler);

  /**
   * Remove a message handler object from message distribution observer.
   * Must be called from the main thread.
   *
   * @param aNodeName Node name of the object
   * @param aMsgHandler Weak pointer to the object
   */
  virtual void
  UnregisterBluetoothSignalHandler(const nsAString& aNodeName,
                                   BluetoothSignalObserver* aMsgHandler);

  /**
   * Remove a message handlers for the given observer.
   * Must be called from the main thread.
   *
   * @param aMsgHandler Weak pointer to the object
   */
  void
  UnregisterAllSignalHandlers(BluetoothSignalObserver* aMsgHandler);

  /**
   * Create a signal without value and distribute it to the observer list
   *
   * @param aName Name of the signal
   * @param aPath Path of the signal to distribute to
   */
  void
  DistributeSignal(const nsAString& aName, const nsAString& aPath);

  /**
   * Create a signal and distribute it to the observer list
   *
   * @param aName Name of the signal
   * @param aPath Path of the signal to distribute to
   * @param aValue Value of the signal to carry
   */
  void
  DistributeSignal(const nsAString& aName, const nsAString& aPath,
                   const BluetoothValue& aValue);

  /**
   * Create a signal without value and distribute it to the observer list
   *
   * @param aName Name of the signal
   * @param aAddress Path of the signal to distribute to
   */
  void
  DistributeSignal(const nsAString& aName, const BluetoothAddress& aAddress);

  /**
   * Create a signal and distribute it to the observer list
   *
   * @param aName Name of the signal
   * @param aAddress Path of the signal to distribute to
   * @param aValue Value of the signal to carry
   */
  void
  DistributeSignal(const nsAString& aName, const BluetoothAddress& aAddress,
                   const BluetoothValue& aValue);

  /**
   * Create a signal without value and distribute it to the observer list
   *
   * @param aName Name of the signal
   * @param aUuid Path of the signal to distribute to
   */
  void
  DistributeSignal(const nsAString& aName, const BluetoothUuid& aUuid);

  /**
   * Create a signal and distribute it to the observer list
   *
   * @param aName Name of the signal
   * @param aUuid Path of the signal to distribute to
   * @param aValue Value of the signal to carry
   */
  void
  DistributeSignal(const nsAString& aName, const BluetoothUuid& aUuid,
                   const BluetoothValue& aValue);

  /**
   * Distribute a signal to the observer list
   *
   * @param aSignal Signal object to distribute
   */
  void
  DistributeSignal(const BluetoothSignal& aSignal);

  /**
   * Returns the BluetoothService singleton. Only to be called from main thread.
   *
   * @param aService Pointer to return singleton into.
   *
   * @return non-nullptr on proper assignment, nullptr otherwise (if service
   * has not yet been started, for instance)
   */
  static BluetoothService*
  Get();

  static already_AddRefed<BluetoothService>
  FactoryCreate()
  {
    RefPtr<BluetoothService> service = Get();
    return service.forget();
  }

  /**
   * Returns an array of each adapter's properties, implemented via a platform
   * specific method.
   *
   * @return NS_OK on success, NS_ERROR_FAILURE otherwise
   */
  virtual nsresult
  GetAdaptersInternal(BluetoothReplyRunnable* aRunnable) = 0;

  /**
   * Returns the properties of paired devices, implemented via a platform
   * specific method.
   *
   * @return NS_OK on success, NS_ERROR_FAILURE otherwise
   */
  virtual nsresult
  GetPairedDevicePropertiesInternal(
    const nsTArray<BluetoothAddress>& aDeviceAddresses,
    BluetoothReplyRunnable* aRunnable) = 0;

  /**
   * Returns the properties of connected devices regarding to specific profile,
   * implemented via a platform specific methood.
   *
   * @return NS_OK on success, NS_ERROR_FAILURE otherwise
   */
  virtual nsresult
  GetConnectedDevicePropertiesInternal(uint16_t aServiceUuid,
                                       BluetoothReplyRunnable* aRunnable) = 0;

  /**
   * Returns up-to-date uuids of given device address,
   * implemented via a platform specific methood.
   *
   * @return NS_OK on success, NS_ERROR_FAILURE otherwise
   */
  virtual nsresult
  FetchUuidsInternal(const BluetoothAddress& aDeviceAddress,
                     BluetoothReplyRunnable* aRunnable) = 0;

  /**
   * Stop device discovery (platform specific implementation)
   */
  virtual void
  StopDiscoveryInternal(BluetoothReplyRunnable* aRunnable) = 0;

  /**
   * Start device discovery (platform specific implementation)
   */
  virtual void
  StartDiscoveryInternal(BluetoothReplyRunnable* aRunnable) = 0;

  /**
   * Stops an ongoing Bluetooth LE device scan.
   */
  virtual void
  StopLeScanInternal(const BluetoothUuid& aScanUuid,
                     BluetoothReplyRunnable* aRunnable) = 0;

  /**
   * Starts a Bluetooth LE device scan.
   */
  virtual void
  StartLeScanInternal(const nsTArray<BluetoothUuid>& aServiceUuids,
                      BluetoothReplyRunnable* aRunnable) = 0;

  /**
   * Start/Stop advertising.
   */
  virtual void
  StartAdvertisingInternal(const BluetoothUuid& aAppUuid,
                           const BluetoothGattAdvertisingData& aAdvData,
                           BluetoothReplyRunnable* aRunnable) { }

  virtual void
  StopAdvertisingInternal(const BluetoothUuid& aAppUuid,
                          BluetoothReplyRunnable* aRunnable) { }

  /**
   * Set a property for the specified object
   *
   * @param aPropName Name of the property
   * @param aValue Boolean value
   * @param aRunnable Runnable to run on async reply
   *
   * @return NS_OK if property is set correctly, NS_ERROR_FAILURE otherwise
   */
  virtual nsresult
  SetProperty(BluetoothObjectType aType,
              const BluetoothNamedValue& aValue,
              BluetoothReplyRunnable* aRunnable) = 0;

  virtual nsresult
  CreatePairedDeviceInternal(const BluetoothAddress& aDeviceAddress,
                             int aTimeout,
                             BluetoothReplyRunnable* aRunnable) = 0;

  virtual nsresult
  RemoveDeviceInternal(const BluetoothAddress& aDeviceAddress,
                       BluetoothReplyRunnable* aRunnable) = 0;

  /**
   * Get corresponding service channel of specific service on remote device.
   * It's usually the very first step of establishing an outbound connection.
   *
   * @param aObjectPath Address of remote device
   * @param aServiceUuid UUID of the target service
   * @param aManager Instance which has callback function OnGetServiceChannel()
   *
   * @return NS_OK if the task begins, NS_ERROR_FAILURE otherwise
   */
  virtual nsresult
  GetServiceChannel(const BluetoothAddress& aDeviceAddress,
                    const BluetoothUuid& aServiceUuid,
                    BluetoothProfileManagerBase* aManager) = 0;

  virtual bool
  UpdateSdpRecords(const BluetoothAddress& aDeviceAddress,
                   BluetoothProfileManagerBase* aManager) = 0;

  virtual void
  PinReplyInternal(const BluetoothAddress& aDeviceAddress,
                   bool aAccept,
                   const BluetoothPinCode& aPinCode,
                   BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  SspReplyInternal(const BluetoothAddress& aDeviceAddress,
                   BluetoothSspVariant aVariant,
                   bool aAccept,
                   BluetoothReplyRunnable* aRunnable) = 0;

  /**
   * Legacy method used by bluez only to reply pincode request.
   */
  virtual void
  SetPinCodeInternal(const BluetoothAddress& aDeviceAddress,
                     const BluetoothPinCode& aPinCode,
                     BluetoothReplyRunnable* aRunnable) = 0;

  /**
   * Legacy method used by bluez only to reply passkey entry request.
   */
  virtual void
  SetPasskeyInternal(const BluetoothAddress& aDeviceAddress, uint32_t aPasskey,
                     BluetoothReplyRunnable* aRunnable) = 0;

  /**
   * Legacy method used by bluez only to reply pairing confirmation request.
   */
  virtual void
  SetPairingConfirmationInternal(const BluetoothAddress& aDeviceAddress,
                                 bool aConfirm,
                                 BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  Connect(const BluetoothAddress& aDeviceAddress,
          uint32_t aCod, uint16_t aServiceUuid,
          BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  Disconnect(const BluetoothAddress& aDeviceAddress, uint16_t aServiceUuid,
             BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  SendFile(const BluetoothAddress& aDeviceAddress,
           BlobParent* aBlobParent,
           BlobChild* aBlobChild,
           BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  SendFile(const BluetoothAddress& aDeviceAddress,
           Blob* aBlob,
           BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  StopSendingFile(const BluetoothAddress& aDeviceAddress,
                  BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  ConfirmReceivingFile(const BluetoothAddress& aDeviceAddress, bool aConfirm,
                       BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  ConnectSco(BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  DisconnectSco(BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  IsScoConnected(BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  SetObexPassword(const nsAString& aPassword,
                  BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  RejectObexAuth(BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  ReplyTovCardPulling(BlobParent* aBlobParent,
                      BlobChild* aBlobChild,
                      BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  ReplyTovCardPulling(Blob* aBlob,
                      BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  ReplyToPhonebookPulling(BlobParent* aBlobParent,
                          BlobChild* aBlobChild,
                          uint16_t aPhonebookSize,
                          BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  ReplyToPhonebookPulling(Blob* aBlob,
                          uint16_t aPhonebookSize,
                          BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  ReplyTovCardListing(BlobParent* aBlobParent,
                      BlobChild* aBlobChild,
                      uint16_t aPhonebookSize,
                      BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  ReplyTovCardListing(Blob* aBlob,
                      uint16_t aPhonebookSize,
                      BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  ReplyToMapFolderListing(long aMasId,
                          const nsAString& aFolderlists,
                          BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  ReplyToMapMessagesListing(BlobParent* aBlobParent,
                            BlobChild* aBlobChild,
                            long aMasId,
                            bool aNewMessage,
                            const nsAString& aTimestamp,
                            int aSize,
                            BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  ReplyToMapMessagesListing(long aMasId,
                            Blob* aBlob,
                            bool aNewMessage,
                            const nsAString& aTimestamp,
                            int aSize,
                            BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  ReplyToMapGetMessage(BlobParent* aBlobParent,
                       BlobChild* aBlobChild,
                       long aMasId,
                       BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  ReplyToMapGetMessage(Blob* aBlob,
                       long aMasId,
                       BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  ReplyToMapSetMessageStatus(long aMasId,
                             bool aStatus,
                             BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  ReplyToMapSendMessage(
    long aMasId, const nsAString& aHandleId, bool aStatus,
    BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  ReplyToMapMessageUpdate(
    long aMasId, bool aStatus, BluetoothReplyRunnable* aRunnable) = 0;

#ifdef MOZ_B2G_RIL
  virtual void
  AnswerWaitingCall(BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  IgnoreWaitingCall(BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  ToggleCalls(BluetoothReplyRunnable* aRunnable) = 0;
#endif

  virtual void
  SendMetaData(const nsAString& aTitle,
               const nsAString& aArtist,
               const nsAString& aAlbum,
               int64_t aMediaNumber,
               int64_t aTotalMediaCount,
               int64_t aDuration,
               BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  SendPlayStatus(int64_t aDuration,
                 int64_t aPosition,
                 ControlPlayStatus aPlayStatus,
                 BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  UpdatePlayStatus(uint32_t aDuration,
                   uint32_t aPosition,
                   ControlPlayStatus aPlayStatus) = 0;

  virtual nsresult
  SendSinkMessage(const nsAString& aDeviceAddresses,
                  const nsAString& aMessage) = 0;

  virtual nsresult
  SendInputMessage(const nsAString& aDeviceAddresses,
                   const nsAString& aMessage) = 0;

  /**
   * Connect to a remote GATT server. (platform specific implementation)
   */
  virtual void
  ConnectGattClientInternal(const BluetoothUuid& aAppUuid,
                            const BluetoothAddress& aDeviceAddress,
                            BluetoothReplyRunnable* aRunnable) = 0;

  /**
   * Disconnect GATT client from a remote GATT server.
   * (platform specific implementation)
   */
  virtual void
  DisconnectGattClientInternal(const BluetoothUuid& aAppUuid,
                               const BluetoothAddress& aDeviceAddress,
                               BluetoothReplyRunnable* aRunnable) = 0;

  /**
   * Discover GATT services, characteristic, descriptors from a remote GATT
   * server. (platform specific implementation)
   */
  virtual void
  DiscoverGattServicesInternal(const BluetoothUuid& aAppUuid,
                               BluetoothReplyRunnable* aRunnable) = 0;

  /**
   * Enable notifications of a given GATT characteristic.
   * (platform specific implementation)
   */
  virtual void
  GattClientStartNotificationsInternal(const BluetoothUuid& aAppUuid,
                                       const BluetoothGattServiceId& aServId,
                                       const BluetoothGattId& aCharId,
                                       BluetoothReplyRunnable* aRunnable) = 0;

  /**
   * Disable notifications of a given GATT characteristic.
   * (platform specific implementation)
   */
  virtual void
  GattClientStopNotificationsInternal(const BluetoothUuid& aAppUuid,
                                      const BluetoothGattServiceId& aServId,
                                      const BluetoothGattId& aCharId,
                                      BluetoothReplyRunnable* aRunnable) = 0;

  /**
   * Unregister a GATT client. (platform specific implementation)
   */
  virtual void
  UnregisterGattClientInternal(int aClientIf,
                               BluetoothReplyRunnable* aRunnable) = 0;

  /**
   * Request RSSI for a remote GATT server. (platform specific implementation)
   */
  virtual void
  GattClientReadRemoteRssiInternal(int aClientIf,
                                   const BluetoothAddress& aDeviceAddress,
                                   BluetoothReplyRunnable* aRunnable) = 0;

  /**
   * Read the value of a characteristic on a GATT client.
   * (platform specific implementation)
   */
  virtual void
  GattClientReadCharacteristicValueInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharacteristicId,
    BluetoothReplyRunnable* aRunnable) = 0;

  /**
   * Write the value of a characteristic on a GATT client.
   * (platform specific implementation)
   */
  virtual void
  GattClientWriteCharacteristicValueInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharacteristicId,
    const BluetoothGattWriteType& aWriteType,
    const nsTArray<uint8_t>& aValue,
    BluetoothReplyRunnable* aRunnable) = 0;

  /**
   * Read the value of a descriptor of a characteristic on a GATT client.
   * (platform specific implementation)
   */
  virtual void
  GattClientReadDescriptorValueInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharacteristicId,
    const BluetoothGattId& aDescriptorId,
    BluetoothReplyRunnable* aRunnable) = 0;

  /**
   * Write the value of a descriptor of a characteristic on a GATT client.
   * (platform specific implementation)
   */
  virtual void
  GattClientWriteDescriptorValueInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothGattServiceId& aServiceId,
    const BluetoothGattId& aCharacteristicId,
    const BluetoothGattId& aDescriptorId,
    const nsTArray<uint8_t>& aValue,
    BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  GattServerRegisterInternal(
    const BluetoothUuid& aAppUuid,
    BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  GattServerConnectPeripheralInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothAddress& aAddress,
    BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  GattServerDisconnectPeripheralInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothAddress& aAddress,
    BluetoothReplyRunnable* aRunnable) = 0;

  /**
   * Unregister a GATT server. (platform specific implementation)
   */
  virtual void
  UnregisterGattServerInternal(int aServerIf,
                               BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  GattServerAddServiceInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothGattServiceId& aServiceId,
    uint16_t aHandleCount,
    BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  GattServerAddIncludedServiceInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothAttributeHandle& aServiceHandle,
    const BluetoothAttributeHandle& aIncludedServiceHandle,
    BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  GattServerAddCharacteristicInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothAttributeHandle& aServiceHandle,
    const BluetoothUuid& aCharacteristicUuid,
    BluetoothGattAttrPerm aPermissions,
    BluetoothGattCharProp aProperties,
    BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  GattServerAddDescriptorInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothAttributeHandle& aServiceHandle,
    const BluetoothAttributeHandle& aCharacteristicHandle,
    const BluetoothUuid& aDescriptorUuid,
    BluetoothGattAttrPerm aPermissions,
    BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  GattServerRemoveServiceInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothAttributeHandle& aServiceHandle,
    BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  GattServerStartServiceInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothAttributeHandle& aServiceHandle,
    BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  GattServerStopServiceInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothAttributeHandle& aServiceHandle,
    BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  GattServerSendResponseInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothAddress& aAddress,
    uint16_t aStatus,
    int32_t aRequestId,
    const BluetoothGattResponse& aRsp,
    BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  GattServerSendIndicationInternal(
    const BluetoothUuid& aAppUuid,
    const BluetoothAddress& aAddress,
    const BluetoothAttributeHandle& aCharacteristicHandle,
    bool aConfirm,
    const nsTArray<uint8_t>& aValue,
    BluetoothReplyRunnable* aRunnable) = 0;

  bool
  IsEnabled() const
  {
    return mEnabled;
  }

  bool
  IsToggling() const;

  static void AcknowledgeToggleBt(bool aEnabled);

  void FireAdapterStateChanged(bool aEnable);
  nsresult EnableDisable(bool aEnable,
                         BluetoothReplyRunnable* aRunnable);

  /**
   * Platform specific startup functions go here. Usually deals with member
   * variables, so not static. Guaranteed to be called outside of main thread.
   *
   * @return NS_OK on correct startup, NS_ERROR_FAILURE otherwise
   */
  virtual nsresult
  StartInternal(BluetoothReplyRunnable* aRunnable) = 0;

  /**
   * Platform specific startup functions go here. Usually deals with member
   * variables, so not static. Guaranteed to be called outside of main thread.
   *
   * @return NS_OK on correct startup, NS_ERROR_FAILURE otherwise
   */
  virtual nsresult
  StopInternal(BluetoothReplyRunnable* aRunnable) = 0;

protected:
  BluetoothService() : mEnabled(false)
  { }

  virtual ~BluetoothService();

  bool
  Init();

  void
  Cleanup();

  nsresult
  StartBluetooth(bool aIsStartup, BluetoothReplyRunnable* aRunnable);

  nsresult
  StopBluetooth(bool aIsStartup, BluetoothReplyRunnable* aRunnable);

  nsresult
  StartStopBluetooth(bool aStart,
                     bool aIsStartup,
                     BluetoothReplyRunnable* aRunnable);

  /**
   * Called when XPCOM first creates this service.
   */
  virtual nsresult
  HandleStartup();

  virtual void
  CompleteToggleBt(bool aEnabled);

  /**
   * Called when the startup settings check has completed.
   */
  nsresult
  HandleStartupSettingsCheck(bool aEnable);

  /**
   * Called when "mozsettings-changed" observer topic fires.
   */
  nsresult
  HandleSettingsChanged(nsISupports* aSubject);

  /**
   * Called when XPCOM is shutting down.
   */
  virtual nsresult
  HandleShutdown();

  // Called by ToggleBtAck.
  void
  SetEnabled(bool aEnabled);

  // Called by Get().
  static BluetoothService*
  Create();

  typedef nsClassHashtable<nsStringHashKey, BluetoothSignalObserverList >
  BluetoothSignalObserverTable;

  BluetoothSignalObserverTable mBluetoothSignalObserverTable;

  nsTArray<BluetoothSignal> mPendingPairReqSignals;

  bool mEnabled;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_BluetoothService_h
