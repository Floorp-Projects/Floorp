/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothadapter_h__
#define mozilla_dom_bluetooth_bluetoothadapter_h__

#include "BluetoothCommon.h"
#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/BluetoothAdapter2Binding.h"
#include "mozilla/dom/BluetoothDeviceEvent.h"
#include "mozilla/dom/Promise.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {
class DOMRequest;
class File;
struct MediaMetaData;
struct MediaPlayStatus;
}
}

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothDevice;
class BluetoothDiscoveryHandle;
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

  BluetoothPairingListener* PairingReqs() const
  {
    return mPairingReqs;
  }

  /****************************************************************************
   * Event Handlers
   ***************************************************************************/
  IMPL_EVENT_HANDLER(attributechanged);
  IMPL_EVENT_HANDLER(devicepaired);
  IMPL_EVENT_HANDLER(deviceunpaired);
  IMPL_EVENT_HANDLER(a2dpstatuschanged);
  IMPL_EVENT_HANDLER(hfpstatuschanged);
  IMPL_EVENT_HANDLER(requestmediaplaystatus);
  IMPL_EVENT_HANDLER(scostatuschanged);

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
  already_AddRefed<Promise> Pair(const nsAString& aDeviceAddress,
                                 ErrorResult& aRv);
  already_AddRefed<Promise> Unpair(const nsAString& aDeviceAddress,
                                   ErrorResult& aRv);

  /**
   * Get a list of paired bluetooth devices.
   *
   * @param aDevices [out] Devices array to return
   */
  void GetPairedDevices(nsTArray<nsRefPtr<BluetoothDevice> >& aDevices);

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
                                        File& aBlob,
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
    Create(nsPIDOMWindow* aOwner, const BluetoothValue& aValue);

  void Notify(const BluetoothSignal& aParam); // BluetoothSignalObserver
  nsPIDOMWindow* GetParentObject() const
  {
     return GetOwner();
  }

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;
  virtual void DisconnectFromOwner() MOZ_OVERRIDE;

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

private:
  BluetoothAdapter(nsPIDOMWindow* aOwner, const BluetoothValue& aValue);
  ~BluetoothAdapter();

  /**
   * Set adapter properties according to properties array
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
   * Handle "DeviceFound" bluetooth signal.
   *
   * @param aValue [in] Properties array of the discovered device.
   */
  void HandleDeviceFound(const BluetoothValue& aValue);

  /**
   * Handle "PairingRequest" bluetooth signal.
   *
   * @param aValue [in] Array of information about the pairing request.
   */
  void HandlePairingRequest(const BluetoothValue& aValue);

  /**
   * Fire BluetoothAttributeEvent to trigger onattributechanged event handler.
   */
  void DispatchAttributeEvent(const nsTArray<nsString>& aTypes);

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
   * Convert string to BluetoothAdapterAttribute.
   *
   * @param aString [in] String to convert
   */
  BluetoothAdapterAttribute
    ConvertStringToAdapterAttribute(const nsAString& aString);

  /**
   * Check whether value of given adapter property has changed.
   *
   * @param aType  [in] Adapter property to check
   * @param aValue [in] New value of the adapter property
   */
  bool IsAdapterAttributeChanged(BluetoothAdapterAttribute aType,
                                 const BluetoothValue& aValue);

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
   * Handle to fire pairing requests of different pairing types.
   */
  nsRefPtr<BluetoothPairingListener> mPairingReqs;

  /**
   * Handle to fire 'ondevicefound' event handler for discovered device.
   *
   * This variable is set to the latest discovery handle when adapter just
   * starts discovery, and is reset to nullptr when discovery is stopped by
   * some adapter.
   */
  nsRefPtr<BluetoothDiscoveryHandle> mDiscoveryHandleInUse;

  /**
   * Arrays of references to BluetoothDevices created by this adapter.
   * This array is empty when adapter state is Disabled.
   *
   * Devices will be appended when
   * 1) Enabling BT: Paired devices reported by stack.
   * 2) Discovering: Discovered devices during discovery operation.
   * A device won't be appended if a device object with the same
   * address already exists.
   *
   * Devices will be removed when
   * 1) Starting discovery: All unpaired devices will be removed before this
   *    adapter starts a new discovery.
   * 2) Disabling BT: All devices will be removed.
   */
  nsTArray<nsRefPtr<BluetoothDevice> > mDevices;
};

END_BLUETOOTH_NAMESPACE

#endif
