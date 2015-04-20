/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothdevice_h__
#define mozilla_dom_bluetooth_bluetoothdevice_h__

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/BluetoothDevice2Binding.h"
#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "nsString.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {
  class Promise;
}
}

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothClassOfDevice;
class BluetoothGatt;
class BluetoothNamedValue;
class BluetoothValue;
class BluetoothSignal;
class BluetoothSocket;

class BluetoothDevice final : public DOMEventTargetHelper
                            , public BluetoothSignalObserver
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(BluetoothDevice,
                                           DOMEventTargetHelper)

  /****************************************************************************
   * Attribute Getters
   ***************************************************************************/
  void GetAddress(nsString& aAddress) const
  {
    aAddress = mAddress;
  }

  BluetoothClassOfDevice* Cod() const
  {
    return mCod;
  }

  void GetName(nsString& aName) const
  {
    aName = mName;
  }

  bool Paired() const
  {
    return mPaired;
  }

  void GetUuids(nsTArray<nsString>& aUuids) const
  {
    aUuids = mUuids;
  }

  BluetoothDeviceType Type() const
  {
    return mType;
  }

  BluetoothGatt* GetGatt();

  /****************************************************************************
   * Event Handlers
   ***************************************************************************/
  IMPL_EVENT_HANDLER(attributechanged);

  /****************************************************************************
   * Methods (Web API Implementation)
   ***************************************************************************/
  already_AddRefed<Promise> FetchUuids(ErrorResult& aRv);

  /****************************************************************************
   * Others
   ***************************************************************************/
  static already_AddRefed<BluetoothDevice>
    Create(nsPIDOMWindow* aOwner, const BluetoothValue& aValue);

  void Notify(const BluetoothSignal& aParam); // BluetoothSignalObserver
  nsPIDOMWindow* GetParentObject() const
  {
     return GetOwner();
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  virtual void DisconnectFromOwner() override;

private:
  BluetoothDevice(nsPIDOMWindow* aOwner, const BluetoothValue& aValue);
  ~BluetoothDevice();

  /**
   * Set device properties according to properties array
   *
   * @param aValue [in] Properties array to set with
   */
  void SetPropertyByValue(const BluetoothNamedValue& aValue);

  /**
   * Handle "PropertyChanged" bluetooth signal.
   *
   * @param aValue [in] Array of changed properties
   */
  void HandlePropertyChanged(const BluetoothValue& aValue);

  /**
   * Fire BluetoothAttributeEvent to trigger onattributechanged event handler.
   */
  void DispatchAttributeEvent(const Sequence<nsString>& aTypes);

  /**
   * Convert uint32_t to BluetoothDeviceType.
   *
   * @param aValue [in] uint32_t to convert
   */
  BluetoothDeviceType ConvertUint32ToDeviceType(const uint32_t aValue);

  /**
   * Convert string to BluetoothDeviceAttribute.
   *
   * @param aString [in] String to convert
   */
  BluetoothDeviceAttribute
    ConvertStringToDeviceAttribute(const nsAString& aString);

  /**
   * Check whether value of given device property has changed.
   *
   * @param aType  [in] Device property to check
   * @param aValue [in] New value of the device property
   */
  bool IsDeviceAttributeChanged(BluetoothDeviceAttribute aType,
                                const BluetoothValue& aValue);

  /****************************************************************************
   * Variables
   ***************************************************************************/
  /**
   * BD address of this device.
   */
  nsString mAddress;

  /**
   * Class of device (CoD) that describes this device's capabilities.
   */
  nsRefPtr<BluetoothClassOfDevice> mCod;

  /**
   * Human-readable name of this device.
   */
  nsString mName;

  /**
   * Whether this device is paired or not.
   */
  bool mPaired;

  /**
   * Cached UUID list of services which this device provides.
   */
  nsTArray<nsString> mUuids;

  /**
   * Type of this device. Can be unknown/classic/le/dual.
   */
  BluetoothDeviceType mType;

  /**
   * GATT client object to interact with the remote device.
   */
  nsRefPtr<BluetoothGatt> mGatt;
};

END_BLUETOOTH_NAMESPACE

/**
 * Explicit Specialization of Function Templates
 *
 * Allows customizing the template code for a given set of template arguments.
 * With this function template, nsTArray can handle comparison of
 * 'nsRefPtr<BluetoothDevice>' properly, including IndexOf() and Contains();
 */
template <>
class nsDefaultComparator <nsRefPtr<mozilla::dom::bluetooth::BluetoothDevice>,
                           nsRefPtr<mozilla::dom::bluetooth::BluetoothDevice>> {
  public:

    bool Equals(
      const nsRefPtr<mozilla::dom::bluetooth::BluetoothDevice>& aDeviceA,
      const nsRefPtr<mozilla::dom::bluetooth::BluetoothDevice>& aDeviceB) const
    {
      nsString addressA, addressB;
      aDeviceA->GetAddress(addressA);
      aDeviceB->GetAddress(addressB);

      return addressA.Equals(addressB);
    }
};

/**
 * Explicit Specialization of Function Templates
 *
 * Allows customizing the template code for a given set of template arguments.
 * With this function template, nsTArray can handle comparison between
 * 'nsRefPtr<BluetoothDevice>' and nsString properly, including IndexOf() and
 * Contains();
 */
template <>
class nsDefaultComparator <nsRefPtr<mozilla::dom::bluetooth::BluetoothDevice>,
                           nsString> {
public:
  bool Equals(
    const nsRefPtr<mozilla::dom::bluetooth::BluetoothDevice>& aDevice,
    const nsString& aAddress) const
  {
    nsString deviceAddress;
    aDevice->GetAddress(deviceAddress);

    return deviceAddress.Equals(aAddress);
  }
};

#endif
