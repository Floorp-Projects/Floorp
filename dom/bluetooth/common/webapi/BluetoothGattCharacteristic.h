/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothGattCharacteristic_h
#define mozilla_dom_bluetooth_BluetoothGattCharacteristic_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/BluetoothGattCharacteristicBinding.h"
#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "mozilla/dom/bluetooth/BluetoothGattDescriptor.h"
#include "mozilla/dom/TypedArray.h"
#include "nsCOMPtr.h"
#include "nsWrapperCache.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {
class Promise;
}
}

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothGattService;
class BluetoothSignal;
class BluetoothValue;

class BluetoothGattCharacteristic final : public nsISupports
                                        , public nsWrapperCache
                                        , public BluetoothSignalObserver
{
  friend class BluetoothGattService;
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(BluetoothGattCharacteristic)

  /****************************************************************************
   * Attribute Getters
   ***************************************************************************/
  BluetoothGattService* Service() const
  {
    return mService;
  }

  void GetDescriptors(
    nsTArray<nsRefPtr<BluetoothGattDescriptor>>& aDescriptors) const
  {
    aDescriptors = mDescriptors;
  }

  void GetUuid(nsString& aUuidStr) const
  {
    aUuidStr = mUuidStr;
  }

  int InstanceId() const
  {
    return mCharId.mInstanceId;
  }

  void GetValue(JSContext* cx, JS::MutableHandle<JSObject*> aValue) const;

  void GetProperties(GattCharacteristicProperties& aProperties) const;

  /****************************************************************************
   * Methods (Web API Implementation)
   ***************************************************************************/
  already_AddRefed<Promise> ReadValue(ErrorResult& aRv);
  already_AddRefed<Promise> WriteValue(const ArrayBuffer& aValue,
                                       ErrorResult& aRv);

  /****************************************************************************
   * Methods (Web API Implementation)
   ***************************************************************************/
  already_AddRefed<Promise> StartNotifications(ErrorResult& aRv);
  already_AddRefed<Promise> StopNotifications(ErrorResult& aRv);

  /****************************************************************************
   * Others
   ***************************************************************************/
  const BluetoothGattId& GetCharacteristicId() const
  {
    return mCharId;
  }

  void Notify(const BluetoothSignal& aData); // BluetoothSignalObserver

  nsPIDOMWindow* GetParentObject() const
  {
     return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  BluetoothGattCharacteristic(nsPIDOMWindow* aOwner,
                              BluetoothGattService* aService,
                              const BluetoothGattCharAttribute& aChar);

private:
  ~BluetoothGattCharacteristic();

  /**
   * Add newly discovered GATT descriptors into mDescriptors and update the
   * cache value of mDescriptors.
   *
   * @param aDescriptorIds [in] An array of BluetoothGattId for each descriptor
   *                            that belongs to this characteristic.
   */
  void AssignDescriptors(const nsTArray<BluetoothGattId>& aDescriptorIds);

  /**
   * Update the value of this characteristic.
   *
   * @param aValue [in] BluetoothValue which contains an uint8_t array.
   */
  void HandleCharacteristicValueUpdated(const BluetoothValue& aValue);

  /****************************************************************************
   * Variables
   ***************************************************************************/
  nsCOMPtr<nsPIDOMWindow> mOwner;

  /**
   * Service that this characteristic belongs to.
   */
  nsRefPtr<BluetoothGattService> mService;

  /**
   * Array of discovered descriptors for this characteristic.
   */
  nsTArray<nsRefPtr<BluetoothGattDescriptor>> mDescriptors;

  /**
   * GattId of this GATT characteristic which contains
   * 1) mUuid: UUID of this characteristic in byte array format.
   * 2) mInstanceId: Instance id of this characteristic.
   */
  BluetoothGattId mCharId;

  /**
   * UUID string of this GATT characteristic.
   */
  nsString mUuidStr;

  /**
   * Value of this GATT characteristic.
   */
  nsTArray<uint8_t> mValue;

  /**
   * Properties of this GATT characteristic.
   */
  BluetoothGattCharProp mProperties;

  /**
   * Write type of this GATT characteristic.
   */
  BluetoothGattWriteType mWriteType;
};

END_BLUETOOTH_NAMESPACE

/**
 * Explicit Specialization of Function Templates
 *
 * Allows customizing the template code for a given set of template arguments.
 * With this function template, nsTArray can handle comparison between
 * 'nsRefPtr<BluetoothGattCharacteristic>' and 'BluetoothGattId' properly,
 * including IndexOf() and Contains();
 */
template <>
class nsDefaultComparator <
  nsRefPtr<mozilla::dom::bluetooth::BluetoothGattCharacteristic>,
  mozilla::dom::bluetooth::BluetoothGattId> {
public:
  bool Equals(
    const nsRefPtr<mozilla::dom::bluetooth::BluetoothGattCharacteristic>& aChar,
    const mozilla::dom::bluetooth::BluetoothGattId& aCharId) const
  {
    return aChar->GetCharacteristicId() == aCharId;
  }
};

#endif // mozilla_dom_bluetooth_BluetoothGattCharacteristic_h
