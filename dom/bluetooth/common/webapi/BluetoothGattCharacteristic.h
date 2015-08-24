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

  void GetPermissions(GattPermissions& aPermissions) const;

  void GetProperties(GattCharacteristicProperties& aProperties) const;

  /****************************************************************************
   * Methods (Web API Implementation)
   ***************************************************************************/
  already_AddRefed<Promise> ReadValue(ErrorResult& aRv);
  already_AddRefed<Promise> WriteValue(const ArrayBuffer& aValue,
                                       ErrorResult& aRv);
  already_AddRefed<Promise> StartNotifications(ErrorResult& aRv);
  already_AddRefed<Promise> StopNotifications(ErrorResult& aRv);
  already_AddRefed<Promise> AddDescriptor(const nsAString& aDescriptorUuid,
                                          const GattPermissions& aPermissions,
                                          const ArrayBuffer& aValue,
                                          ErrorResult& aRv);

  /****************************************************************************
   * Others
   ***************************************************************************/
  const BluetoothGattId& GetCharacteristicId() const
  {
    return mCharId;
  }

  void Notify(const BluetoothSignal& aData); // BluetoothSignalObserver

  const BluetoothAttributeHandle& GetCharacteristicHandle() const
  {
    return mCharacteristicHandle;
  }

  void GetUuid(BluetoothUuid& aUuid) const;

  nsPIDOMWindow* GetParentObject() const
  {
     return mOwner;
  }

  BluetoothGattAttrPerm GetPermissions() const
  {
    return mPermissions;
  }

  BluetoothGattCharProp GetProperties() const
  {
    return mProperties;
  }

  uint16_t GetHandleCount() const;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // Constructor of BluetoothGattCharacteristic in ATT client role
  BluetoothGattCharacteristic(nsPIDOMWindow* aOwner,
                              BluetoothGattService* aService,
                              const BluetoothGattCharAttribute& aChar);

  // Constructor of BluetoothGattCharacteristic in ATT server role
  BluetoothGattCharacteristic(nsPIDOMWindow* aOwner,
                              BluetoothGattService* aService,
                              const nsAString& aCharacteristicUuid,
                              const GattPermissions& aPermissions,
                              const GattCharacteristicProperties& aProperties,
                              const ArrayBuffer& aValue);

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

  /**
   * Assign the handle value for this GATT characteristic. This function would
   * be called only after a valid handle value is retrieved from the Bluetooth
   * backend.
   *
   * @param aCharacteristicHandle [in] The handle value of this GATT
   *                                   characteristic.
   */
  void AssignCharacteristicHandle(
    const BluetoothAttributeHandle& aCharacteristicHandle);

  /**
   * Assign the handle value for one of the descriptor within this GATT
   * characteristic. This function would be called only after a valid handle
   * value is retrieved from the Bluetooth backend.
   *
   * @param aDescriptorUuid [in] BluetoothUuid of the target GATT descriptor.
   * @param aDescriptorHandle [in] The handle value of the target GATT
   *                               descriptor.
   */
  void AssignDescriptorHandle(
    const BluetoothUuid& aDescriptorUuid,
    const BluetoothAttributeHandle& aDescriptorHandle);

  /**
   * Examine whether this GATT characteristic can react with the Bluetooth
   * backend.
   *
   * @return true if this characteristic can react with the Bluetooth backend;
   *         false if this characteristic cannot react with the Bluetooth
   *         backend.
   */
  bool IsActivated() const
  {
    return mActive;
  }

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
   * Permissions of this GATT characteristic.
   */
  BluetoothGattAttrPerm mPermissions;

  /**
   * Properties of this GATT characteristic.
   */
  BluetoothGattCharProp mProperties;

  /**
   * Write type of this GATT characteristic.
   */
  BluetoothGattWriteType mWriteType;

  /**
   * ATT role of this GATT characteristic.
   */
  const BluetoothAttRole mAttRole;

  /**
   * Activeness of this GATT characteristic.
   *
   * True means this service does react with the Bluetooth backend. False means
   * this characteristic doesn't react with the Bluetooth backend. The value
   * should be true if |mAttRole| equals |ATT_CLIENT_ROLE| because the
   * characteristic instance could be created only when the Bluetooth backend
   * has found one GATT characteristic. The value would be false at the
   * beginning if |mAttRole| equals |ATT_SERVER_ROLE|. Then the value would
   * become true later if the corresponding GATT service has been added into
   * Bluetooth backend.
   */
  bool mActive;

  /**
   * Handle of this GATT characteristic.
   *
   * The value is only valid if |mAttRole| equals |ATT_SERVER_ROLE|.
   */
  BluetoothAttributeHandle mCharacteristicHandle;

  /**
   * Total count of handles of this GATT characteristic itself.
   */
  static const uint16_t sHandleCount;
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

/**
 * Explicit Specialization of Function Templates
 *
 * Allows customizing the template code for a given set of template arguments.
 * With this function template, nsTArray can handle comparison between
 * 'nsRefPtr<BluetoothGattCharacteristic>' and 'BluetoothUuid' properly,
 * including IndexOf() and Contains();
 */
template <>
class nsDefaultComparator <
  nsRefPtr<mozilla::dom::bluetooth::BluetoothGattCharacteristic>,
  mozilla::dom::bluetooth::BluetoothUuid> {
public:
  bool Equals(
    const nsRefPtr<mozilla::dom::bluetooth::BluetoothGattCharacteristic>& aChar,
    const mozilla::dom::bluetooth::BluetoothUuid& aUuid) const
  {
    mozilla::dom::bluetooth::BluetoothUuid uuid;
    aChar->GetUuid(uuid);
    return uuid == aUuid;
  }
};

/**
 * Explicit Specialization of Function Templates
 *
 * Allows customizing the template code for a given set of template arguments.
 * With this function template, nsTArray can handle comparison between
 * 'nsRefPtr<BluetoothGattCharacteristic>' and 'BluetoothAttributeHandle'
 * properly, including IndexOf() and Contains();
 */
template <>
class nsDefaultComparator <
  nsRefPtr<mozilla::dom::bluetooth::BluetoothGattCharacteristic>,
  mozilla::dom::bluetooth::BluetoothAttributeHandle> {
public:
  bool Equals(
    const nsRefPtr<mozilla::dom::bluetooth::BluetoothGattCharacteristic>& aChar,
    const mozilla::dom::bluetooth::BluetoothAttributeHandle& aCharacteristicHandle)
    const
  {
    return aChar->GetCharacteristicHandle() == aCharacteristicHandle;
  }
};

#endif // mozilla_dom_bluetooth_BluetoothGattCharacteristic_h
