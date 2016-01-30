/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothGattDescriptor_h
#define mozilla_dom_bluetooth_BluetoothGattDescriptor_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/BluetoothGattDescriptorBinding.h"
#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/TypedArray.h"
#include "nsCOMPtr.h"
#include "nsWrapperCache.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {
struct GattPermissions;
}
}

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothGattCharacteristic;
class BluetoothSignal;
class BluetoothValue;

class BluetoothGattDescriptor final : public nsISupports
                                    , public nsWrapperCache
                                    , public BluetoothSignalObserver
{
  friend class BluetoothGattServer;
  friend class BluetoothGattCharacteristic;
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(BluetoothGattDescriptor)

  /****************************************************************************
   * Attribute Getters
   ***************************************************************************/
  BluetoothGattCharacteristic* Characteristic() const
  {
    return mCharacteristic;
  }

  void GetUuid(nsString& aUuidStr) const
  {
    aUuidStr = mUuidStr;
  }

  void GetValue(JSContext* cx, JS::MutableHandle<JSObject*> aValue) const;

  void GetPermissions(GattPermissions& aPermissions) const;

  /****************************************************************************
   * Methods (Web API Implementation)
   ***************************************************************************/
  already_AddRefed<Promise> ReadValue(ErrorResult& aRv);
  already_AddRefed<Promise> WriteValue(
    const RootedTypedArray<ArrayBuffer>& aValue, ErrorResult& aRv);

  /****************************************************************************
   * Others
   ***************************************************************************/
  void Notify(const BluetoothSignal& aData); // BluetoothSignalObserver

  const BluetoothAttributeHandle& GetDescriptorHandle() const
  {
    return mDescriptorHandle;
  }

  nsPIDOMWindowInner* GetParentObject() const
  {
     return mOwner;
  }

  void GetUuid(BluetoothUuid& aUuid) const;

  BluetoothGattAttrPerm GetPermissions() const
  {
    return mPermissions;
  }

  uint16_t GetHandleCount() const
  {
    return sHandleCount;
  }

  const nsTArray<uint8_t>& GetValue() const
  {
    return mValue;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // Constructor of BluetoothGattDescriptor in ATT client role
  BluetoothGattDescriptor(nsPIDOMWindowInner* aOwner,
                          BluetoothGattCharacteristic* aCharacteristic,
                          const BluetoothGattId& aDescriptorId);

  // Constructor of BluetoothGattDescriptor in ATT server role
  BluetoothGattDescriptor(nsPIDOMWindowInner* aOwner,
                          BluetoothGattCharacteristic* aCharacteristic,
                          const nsAString& aDescriptorUuid,
                          const GattPermissions& aPermissions,
                          const ArrayBuffer& aValue);

private:
  ~BluetoothGattDescriptor();

  /**
   * Update the value of this descriptor.
   *
   * @param aValue [in] BluetoothValue which contains an uint8_t array.
   */
  void HandleDescriptorValueUpdated(const BluetoothValue& aValue);

  /**
   * Assign AppUuid of this GATT descriptor.
   *
   * @param aAppUuid The value of AppUuid.
   */
  void AssignAppUuid(const nsAString& aAppUuid);

  /**
   * Assign the handle value for this GATT descriptor. This function would be
   * called only after a valid handle value is retrieved from the Bluetooth
   * backend.
   *
   * @param aDescriptorHandle [in] The handle value of this GATT descriptor.
   */
  void AssignDescriptorHandle(const BluetoothAttributeHandle& aDescriptorHandle);

  /**
   * Examine whether this GATT descriptor can react with the Bluetooth backend.
   *
   * @return true if this descriptor can react with the Bluetooth backend;
   *         false if this descriptor cannot react with the Bluetooth backend.
   */
  bool IsActivated() const
  {
    return mActive;
  }

  /****************************************************************************
   * Variables
   ***************************************************************************/
  nsCOMPtr<nsPIDOMWindowInner> mOwner;

  /**
   * Characteristic that this descriptor belongs to.
   */
  RefPtr<BluetoothGattCharacteristic> mCharacteristic;

  /**
   * GattId of this GATT descriptor which contains
   * 1) mUuid: UUID of this descriptor in byte array format.
   * 2) mInstanceId: Instance id of this descriptor.
   */
  BluetoothGattId mDescriptorId;

  /**
   * UUID string of this GATT descriptor.
   */
  nsString mUuidStr;

  /**
   * Value of this GATT descriptor.
   */
  nsTArray<uint8_t> mValue;

  /**
   * Permissions of this GATT descriptor.
   */
  BluetoothGattAttrPerm mPermissions;

  /**
   * ATT role of this GATT descriptor.
   */
  const BluetoothAttRole mAttRole;

  /**
   * Activeness of this GATT descriptor.
   *
   * True means this service does react with the Bluetooth backend. False means
   * this descriptor doesn't react with the Bluetooth backend. The value should
   * be true if |mAttRole| equals |ATT_CLIENT_ROLE| because the descriptor
   * instance could be created only when the Bluetooth backend has found one
   * GATT descriptor. The value would be false at the beginning if |mAttRole|
   * equals |ATT_SERVER_ROLE|. Then the value would become true later if the
   * corresponding GATT service has been added into Bluetooth backend.
   */
  bool mActive;

  /**
   * Handle of this GATT descriptor.
   *
   * The value is only valid if |mAttRole| equals |ATT_SERVER_ROLE|.
   */
  BluetoothAttributeHandle mDescriptorHandle;

  /**
   * Total count of handles of this GATT descriptor itself.
   */
  static const uint16_t sHandleCount;
};

END_BLUETOOTH_NAMESPACE

/**
 * Explicit Specialization of Function Templates
 *
 * Allows customizing the template code for a given set of template arguments.
 * With this function template, nsTArray can handle comparison between
 * 'RefPtr<BluetoothGattDescriptor>' and 'BluetoothUuid' properly,
 * including IndexOf() and Contains();
 */
template <>
class nsDefaultComparator <
  RefPtr<mozilla::dom::bluetooth::BluetoothGattDescriptor>,
  mozilla::dom::bluetooth::BluetoothUuid> {
public:
  bool Equals(
    const RefPtr<mozilla::dom::bluetooth::BluetoothGattDescriptor>& aDesc,
    const mozilla::dom::bluetooth::BluetoothUuid& aUuid) const
  {
    mozilla::dom::bluetooth::BluetoothUuid uuid;
    aDesc->GetUuid(uuid);
    return uuid == aUuid;
  }
};

/**
 * Explicit Specialization of Function Templates
 *
 * Allows customizing the template code for a given set of template arguments.
 * With this function template, nsTArray can handle comparison between
 * 'RefPtr<BluetoothGattDescriptor>' and 'BluetoothAttributeHandle'
 * properly, including IndexOf() and Contains();
 */
template <>
class nsDefaultComparator <
  RefPtr<mozilla::dom::bluetooth::BluetoothGattDescriptor>,
  mozilla::dom::bluetooth::BluetoothAttributeHandle> {
public:
  bool Equals(
    const RefPtr<mozilla::dom::bluetooth::BluetoothGattDescriptor>& aDesc,
    const mozilla::dom::bluetooth::BluetoothAttributeHandle& aHandle) const
  {
    return aDesc->GetDescriptorHandle() == aHandle;
  }
};

#endif // mozilla_dom_bluetooth_BluetoothGattDescriptor_h
