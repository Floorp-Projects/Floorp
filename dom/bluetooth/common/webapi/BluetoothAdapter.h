/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothAdapter_h
#define mozilla_dom_bluetooth_BluetoothAdapter_h

#include "BluetoothCommon.h"
#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/BluetoothAdapterBinding.h"
#include "mozilla/dom/BluetoothDeviceEvent.h"
#include "mozilla/dom/BluetoothMapParametersBinding.h"
#include "mozilla/dom/BluetoothPbapParametersBinding.h"
#include "mozilla/dom/Promise.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {
class Blob;
class DOMRequest;
struct MediaMetaData;
struct MediaPlayStatus;
}
}

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothDevice;
class BluetoothDiscoveryHandle;
class BluetoothGattServer;
class BluetoothNamedValue;
class BluetoothPairingListener;
class BluetoothSignal;
class BluetoothValue;

class BluetoothAdapter : public DOMEventTargetHelper
                       , public BluetoothSignalObserver
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(BluetoothAdapter,
                                           DOMEventTargetHelper)

  /****************************************************************************
   * Attribute Getters
   ***************************************************************************/
  BluetoothAdapterState State() const
  {
    return mState;
  }

  void GetAddress(nsString& aAddress) const
  {
    aAddress = mAddress;
  }

  void
  GetName(nsString& aName) const
  {
    aName = mName;
  }

  bool
  Discovering() const
  {
    return mDiscovering;
  }

  bool
  Discoverable() const
  {
    return mDiscoverable;
  }

  BluetoothPairingListener* GetPairingReqs() const
  {
    return mPairingReqs;
  }

  BluetoothGattServer* GetGattServer();

  /****************************************************************************
   * Event Handlers
   ***************************************************************************/
  IMPL_EVENT_HANDLER(attributechanged);
  // PAIRING
  IMPL_EVENT_HANDLER(devicepaired);
  IMPL_EVENT_HANDLER(deviceunpaired);
  IMPL_EVENT_HANDLER(pairingaborted);
  // HFP/A2DP/AVRCP/HID
  IMPL_EVENT_HANDLER(a2dpstatuschanged);
  IMPL_EVENT_HANDLER(hfpstatuschanged);
  IMPL_EVENT_HANDLER(hidstatuschanged);
  IMPL_EVENT_HANDLER(scostatuschanged);
  IMPL_EVENT_HANDLER(requestmediaplaystatus);
  // PBAP
  IMPL_EVENT_HANDLER(obexpasswordreq);
  IMPL_EVENT_HANDLER(pullphonebookreq);
  IMPL_EVENT_HANDLER(pullvcardentryreq);
  IMPL_EVENT_HANDLER(pullvcardlistingreq);
  // MAP
  IMPL_EVENT_HANDLER(mapfolderlistingreq);
  IMPL_EVENT_HANDLER(mapmessageslistingreq);
  IMPL_EVENT_HANDLER(mapgetmessagereq);
  IMPL_EVENT_HANDLER(mapsetmessagestatusreq);
  IMPL_EVENT_HANDLER(mapsendmessagereq);
  IMPL_EVENT_HANDLER(mapmessageupdatereq);

  /****************************************************************************
   * Methods (Web API Implementation)
   ***************************************************************************/
  already_AddRefed<Promise> Enable(ErrorResult& aRv);
  already_AddRefed<Promise> Disable(ErrorResult& aRv);

  already_AddRefed<Promise> SetName(const nsAString& aName, ErrorResult& aRv);
  already_AddRefed<Promise> SetDiscoverable(bool aDiscoverable,
                                            ErrorResult& aRv);
  already_AddRefed<Promise> StartDiscovery(ErrorResult& aRv);
  already_AddRefed<Promise> StopDiscovery(ErrorResult& aRv);

  already_AddRefed<Promise> StartLeScan(
    const nsTArray<nsString>& aServiceUuids, ErrorResult& aRv);
  already_AddRefed<Promise> StopLeScan(
    BluetoothDiscoveryHandle& aDiscoveryHandle, ErrorResult& aRv);

  already_AddRefed<Promise> Pair(const nsAString& aDeviceAddress,
                                 ErrorResult& aRv);
  already_AddRefed<Promise> Unpair(const nsAString& aDeviceAddress,
                                   ErrorResult& aRv);

  /**
   * Get a list of paired bluetooth devices.
   *
   * @param aDevices [out] Devices array to return
   */
  void GetPairedDevices(nsTArray<RefPtr<BluetoothDevice> >& aDevices);

  // Connection related methods
  already_AddRefed<DOMRequest>
    Connect(BluetoothDevice& aDevice,
            const Optional<short unsigned int>& aServiceUuid,
            ErrorResult& aRv);
  already_AddRefed<DOMRequest>
    Disconnect(BluetoothDevice& aDevice,
               const Optional<short unsigned int>& aServiceUuid,
               ErrorResult& aRv);
  already_AddRefed<DOMRequest> GetConnectedDevices(uint16_t aServiceUuid,
                                                   ErrorResult& aRv);

  // OPP file transfer related methods
  already_AddRefed<DOMRequest> SendFile(const nsAString& aDeviceAddress,
                                        Blob& aBlob,
                                        ErrorResult& aRv);
  already_AddRefed<DOMRequest> StopSendingFile(const nsAString& aDeviceAddress,
                                               ErrorResult& aRv);
  already_AddRefed<DOMRequest>
    ConfirmReceivingFile(const nsAString& aDeviceAddress,
                         bool aConfirmation,
                         ErrorResult& aRv);

  // SCO related methods
  already_AddRefed<DOMRequest> ConnectSco(ErrorResult& aRv);
  already_AddRefed<DOMRequest> DisconnectSco(ErrorResult& aRv);
  already_AddRefed<DOMRequest> IsScoConnected(ErrorResult& aRv);

  // Handfree CDMA related methods
  already_AddRefed<DOMRequest> AnswerWaitingCall(ErrorResult& aRv);
  already_AddRefed<DOMRequest> IgnoreWaitingCall(ErrorResult& aRv);
  already_AddRefed<DOMRequest> ToggleCalls(ErrorResult& aRv);

  // AVRCP related methods
  already_AddRefed<DOMRequest>
    SendMediaMetaData(const MediaMetaData& aMediaMetaData, ErrorResult& aRv);
  already_AddRefed<DOMRequest>
    SendMediaPlayStatus(const MediaPlayStatus& aMediaPlayStatus,
                        ErrorResult& aRv);

  /****************************************************************************
   * Others
   ***************************************************************************/
  static already_AddRefed<BluetoothAdapter>
    Create(nsPIDOMWindowInner* aOwner, const BluetoothValue& aValue);

  void Notify(const BluetoothSignal& aParam); // BluetoothSignalObserver
  nsPIDOMWindowInner* GetParentObject() const
  {
     return GetOwner();
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  virtual void DisconnectFromOwner() override;

  void GetPairedDeviceProperties(
    const nsTArray<BluetoothAddress>& aDeviceAddresses);

  /**
   * Set this adapter's discovery handle in use (mDiscoveryHandleInUse).
   *
   * |mDiscoveryHandleInUse| is set to the latest discovery handle when adapter
   * just starts discovery, and is reset to nullptr when discovery is stopped
   * by some adapter.
   *
   * @param aDiscoveryHandle [in] Discovery handle to set.
   */
  void SetDiscoveryHandleInUse(BluetoothDiscoveryHandle* aDiscoveryHandle);

  /**
   * Append a BluetoothDiscoveryHandle to LeScan handle array.
   *
   * @param aDiscoveryHandle [in] Discovery handle to be appended.
   */
  void AppendLeScanHandle(BluetoothDiscoveryHandle* aDiscoveryHandle);

  /**
   * Remove the BluetoothDiscoverHandle with the given UUID from LeScan handle
   * array.
   *
   * @param aScanUuid [in] The UUID of the LE scan task.
   */
  void RemoveLeScanHandle(const BluetoothUuid& aScanUuid);

private:
  BluetoothAdapter(nsPIDOMWindowInner* aOwner, const BluetoothValue& aValue);
  ~BluetoothAdapter();

  /**
   * Unregister signal handler and clean up LE scan handles.
   */
  void Cleanup();

  /**
   * Set adapter properties according to properties array.
   *
   * @param aValue [in] Properties array to set with
   */
  void SetPropertyByValue(const BluetoothNamedValue& aValue);

  /**
   * Set adapter state and fire BluetoothAttributeEvent if state changed.
   *
   * @param aState [in] The new adapter state
   */
  void SetAdapterState(BluetoothAdapterState aState);

  /**
   * Pair/Unpair adapter to device of given address.
   * This function is called by methods Enable() and Disable().
   *
   * @param aPair          [in]  Whether to pair or unpair adapter to device.
   * @param aDeviceAddress [in]  Address of device to pair/unpair.
   * @param aRv            [out] Error result to set in case of error.
   */
  already_AddRefed<Promise> PairUnpair(bool aPair,
                            const nsAString& aDeviceAddress,
                            ErrorResult& aRv);

  /**
   * Retrieve properties of paired devices.
   *
   * @param aDeviceAddresses [in] Addresses array of paired devices
   */
  void GetPairedDeviceProperties(const nsTArray<nsString>& aDeviceAddresses);

  /**
   * Handle "PropertyChanged" bluetooth signal.
   *
   * @param aValue [in] Array of changed properties
   */
  void HandlePropertyChanged(const BluetoothValue& aValue);

  /**
   * Handle "DeviceFound" bluetooth signal.
   *
   * @param aValue [in] Properties array of the discovered device.
   */
  void HandleDeviceFound(const BluetoothValue& aValue);

  /**
   * Handle DEVICE_PAIRED_ID bluetooth signal.
   *
   * @param aValue [in] Properties array of the paired device.
   *                    The array should contain two properties:
   *                    - nsString  'Address'
   *                    - bool      'Paired'
   */
  void HandleDevicePaired(const BluetoothValue& aValue);

  /**
   * Handle DEVICE_UNPAIRED_ID bluetooth signal.
   *
   * @param aValue [in] Properties array of the unpaired device.
   *                    The array should contain two properties:
   *                    - nsString  'Address'
   *                    - bool      'Paired'
   */
  void HandleDeviceUnpaired(const BluetoothValue& aValue);

  /**
   * Handle "LeDeviceFound" bluetooth signal.
   *
   * @param aValue [in] Properties array of the scanned device.
   */
  void HandleLeDeviceFound(const BluetoothValue& aValue);

  /**
   * Handle PULL_PHONEBOOK_REQ_ID bluetooth signal.
   *
   * @param aValue [in] Properties array of the PBAP request.
   *                    The array should contain few properties:
   *                    - nsString   'name'
   *                    - bool       'format'
   *                    - uint32_t[] 'propSelector'
   *                    - uint32_t   'maxListCount'
   *                    - uint32_t   'listStartOffset'
   *                    - uint32_t[] 'vCardSelector_AND'
   *                    - uint32_t[] 'vCardSelector_OR'
   */
  void HandlePullPhonebookReq(const BluetoothValue& aValue);

  /**
   * Handle PULL_VCARD_ENTRY_REQ_ID bluetooth signal.
   *
   * @param aValue [in] Properties array of the PBAP request.
   *                    The array should contain few properties:
   *                    - nsString   'name'
   *                    - bool       'format'
   *                    - uint32_t[] 'propSelector'
   */
  void HandlePullVCardEntryReq(const BluetoothValue& aValue);

  /**
   * Handle PULL_VCARD_LISTING_REQ_ID bluetooth signal.
   *
   * @param aValue [in] Properties array of the PBAP request.
   *                    The array should contain few properties:
   *                    - nsString   'name'
   *                    - nsString   'order'
   *                    - nsString   'searchText'
   *                    - nsString   'searchKey'
   *                    - uint32_t   'maxListCount'
   *                    - uint32_t   'listStartOffset'
   *                    - uint32_t[] 'vCardSelector_AND'
   *                    - uint32_t[] 'vCardSelector_OR'
   */
  void HandlePullVCardListingReq(const BluetoothValue& aValue);

  /**
   * Handle OBEX_PASSWORD_REQ_ID bluetooth signal.
   *
   * @param aValue [in] Properties array of the PBAP request.
   *                    The array may contain the property:
   *                    - nsString   'userId'
   */
  void HandleObexPasswordReq(const BluetoothValue& aValue);

  /**
   * Get a Sequence of vCard properies from a BluetoothValue. The name of
   * BluetoothValue must be propSelector, vCardSelector_OR or vCardSelector_AND.
   *
   * @param aValue [in] a BluetoothValue with 'TArrayOfuint32_t' type
   *                    The name of BluetoothValue must be 'propSelector',
   *                    'vCardSelector_OR' or 'vCardSelector_AND'.
   */
  Sequence<vCardProperties> getVCardProperties(const BluetoothValue &aValue);

   /**
   * Handle "MapFolderListing" bluetooth signal.
   *
   * @param aValue [in] Properties array of the MAP request.
   *                    The array should contain a few properties:
   *                    - uint32_t  'MaxListCount'
   *                    - uint32_t  'ListStartOffset'
   */
  void HandleMapFolderListing(const BluetoothValue& aValue);

  /**
   * Handle "MapMessageListing" bluetooth signal.
   *
   * @param aValue [in] Properties array of the MAP request.
   *                    The array should contain a few properties:
   *                    - uint32_t  'MaxListCount'
   *                    - uint32_t  'ListStartOffset'
   *                    - uint32_t  'SubjectLength'
   *                    - uint32_t  'ParameterMask'
   *                    - uint32_t  'FilterMessageType'
   *                    - nsString  'FilterPeriodBegin'
   *                    - nsString  'FilterPeriodEnd'
   *                    - uint32_t  'FilterReadStatus'
   *                    - nsString  'FilterRecipient'
   *                    - nsString  'FilterOriginator'
   *                    - uint32_t  'FilterPriority'
   */
  void HandleMapMessagesListing(const BluetoothValue& aValue);

  /**
   * Handle "MapGetMessage" bluetooth signal.
   *
   * @param aValue [in] Properties array of the MAP request.
   *                    The array should contain a few properties:
   *                    - bool       'Attachment'
   *                    - nsString   'Charset'
   */
  void HandleMapGetMessage(const BluetoothValue& aValue);

  /**
   * Handle "MapSetMessageStatus" bluetooth signal.
   *
   * @param aValue [in] Properties array of the scanned device.
   *                    The array should contain a few properties:
   *                    - long       'HandleId'
   *                    - uint32_t   'StatusIndicator'
   *                    - bool       'StatusValue'
   */
  void HandleMapSetMessageStatus(const BluetoothValue& aValue);

  /**
   * Handle "MapSendMessage" bluetooth signal.
   *
   * @param aValue [in] Properties array of the scanned device.
   *                    The array should contain a few properties:
   *                    - nsString    'Recipient'
   *                    - nsString    'MessageBody'
   *                    - uint32_t    'Retry'
   */
  void HandleMapSendMessage(const BluetoothValue& aValue);

  /**
   * Handle "MapMessageUpdate" bluetooth signal.
   *
   * @param aValue [in] Properties array of the scanned device.
   *                    - nsString     'MASInstanceID'
   */
  void HandleMapMessageUpdate(const BluetoothValue& aValue);

  /**
   * Get a Sequence of ParameterMask from a BluetoothValue. The name of
   * BluetoothValue must be parameterMask.
   *
   * @param aValue [in] a BluetoothValue with 'TArrayOfuint32_t' type
   *                    The name of BluetoothValue must be 'parameterMask'.
   */
  Sequence<ParameterMask> GetParameterMask(const BluetoothValue &aValue);

  /**
   * Fire BluetoothAttributeEvent to trigger onattributechanged event handler.
   *
   * @param aTypes [in] Array of changed attributes. Must be non-empty.
   */
  void DispatchAttributeEvent(const Sequence<nsString>& aTypes);

  /**
   * Fire BluetoothDeviceEvent to trigger
   * ondeviceparied/ondeviceunpaired event handler.
   *
   * @param aType [in] Event type to fire
   * @param aInit [in] Event initialization value
   */
  void DispatchDeviceEvent(const nsAString& aType,
                           const BluetoothDeviceEventInit& aInit);

  /**
   * Fire event with no argument
   *
   * @param aType [in] Event type to fire
   */
  void DispatchEmptyEvent(const nsAString& aType);

  /**
   * Convert string to BluetoothAdapterAttribute.
   *
   * @param aString [in] String to convert
   *
   * @return the adapter attribute converted from |aString|
   */
  BluetoothAdapterAttribute
    ConvertStringToAdapterAttribute(const nsAString& aString);

  /**
   * Check whether value of given adapter property has changed.
   *
   * @param aType  [in] Adapter property to check
   * @param aValue [in] New value of the adapter property
   *
   * @return true if the adapter property has changed; false otherwise
   */
  bool IsAdapterAttributeChanged(BluetoothAdapterAttribute aType,
                                 const BluetoothValue& aValue);

  /**
   * Check whether this adapter belongs to Bluetooth certified app.
   *
   * @return true if this adapter belongs to Bluetooth app; false otherwise
   */
  bool IsBluetoothCertifiedApp();

  /****************************************************************************
   * Variables
   ***************************************************************************/
  /**
   * Current state of this adapter. Can be Disabled/Disabling/Enabled/Enabling.
   */
  BluetoothAdapterState mState;

  /**
   * BD address of this adapter.
   */
  nsString mAddress;

  /**
   * Human-readable name of this adapter.
   */
  nsString mName;

  /**
   * Whether this adapter can be discovered by nearby devices.
   */
  bool mDiscoverable;

  /**
   * Whether this adapter is discovering nearby devices.
   */
  bool mDiscovering;

  /**
   * GATT server object of this adapter.
   *
   * A new GATT server object will be created at the first time when
   * |GetGattServer| is called after the adapter has been enabled. If the
   * adapter has been disabled later on, the created GATT server will be
   * discard by the adapter, and this GATT object should stop working till the
   * end of its life. When |GetGattServer| is called after the adapter has been
   * enabled again, a new GATT server object will be created.
   */
  RefPtr<BluetoothGattServer> mGattServer;

  /**
   * Handle to fire pairing requests of different pairing types.
   */
  RefPtr<BluetoothPairingListener> mPairingReqs;

  /**
   * Handle to fire 'ondevicefound' event handler for discovered device.
   *
   * This variable is set to the latest discovery handle when adapter just
   * starts discovery, and is reset to nullptr when discovery is stopped by
   * some adapter.
   */
  RefPtr<BluetoothDiscoveryHandle> mDiscoveryHandleInUse;

  /**
   * Handles to fire 'ondevicefound' event handler for scanned device
   *
   * Each non-stopped LeScan process has a LeScan handle which is
   * responsible to dispatch LeDeviceEvent.
   */
  nsTArray<RefPtr<BluetoothDiscoveryHandle> > mLeScanHandleArray;

  /**
   * RefPtr array of BluetoothDevices created by this adapter. The array is
   * empty when adapter state is Disabled.
   *
   * Devices will be appended when
   *   1) adapter is enabling: Paired devices reported by stack.
   *   2) adapter is discovering: Discovered devices during discovery operation.
   *   3) adapter paired with a device: The paired device reported by stack.
   * Note devices with identical address won't be appended.
   *
   * Devices will be removed when
   *   1) adapter is disabling: All devices will be removed.
   *   2) adapter starts discovery: All unpaired devices will be removed before
   *      this new discovery starts.
   *   3) adapter unpaired with a device: The unpaired device will be removed.
   */
  nsTArray<RefPtr<BluetoothDevice> > mDevices;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_BluetoothAdapter_h
