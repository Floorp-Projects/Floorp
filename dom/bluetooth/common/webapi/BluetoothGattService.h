/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothGattService_h
#define mozilla_dom_bluetooth_BluetoothGattService_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/BluetoothGattServiceBinding.h"
#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "mozilla/dom/bluetooth/BluetoothGattCharacteristic.h"
#include "nsCOMPtr.h"
#include "nsWrapperCache.h"
#include "nsPIDOMWindow.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothGatt;
class BluetoothSignal;
class BluetoothValue;

class BluetoothGattService final : public nsISupports
                                 , public nsWrapperCache
{
  friend class BluetoothGatt;
  friend class BluetoothGattServer;
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(BluetoothGattService)

  /****************************************************************************
   * Attribute Getters
   ***************************************************************************/
  bool IsPrimary() const
  {
    return mServiceId.mIsPrimary;
  }

  void GetUuid(nsString& aUuidStr) const
  {
    aUuidStr = mUuidStr;
  }

  int InstanceId() const
  {
    return mServiceId.mId.mInstanceId;
  }

  void GetIncludedServices(
    nsTArray<nsRefPtr<BluetoothGattService>>& aIncludedServices) const
  {
    aIncludedServices = mIncludedServices;
  }

  void GetCharacteristics(
    nsTArray<nsRefPtr<BluetoothGattCharacteristic>>& aCharacteristics) const
  {
    aCharacteristics = mCharacteristics;
  }

  /****************************************************************************
   * Methods (Web API Implementation)
   ***************************************************************************/
  static already_AddRefed<BluetoothGattService> Constructor(
    const GlobalObject& aGlobal,
    const BluetoothGattServiceInit& aInit,
    ErrorResult& aRv);
  already_AddRefed<Promise> AddCharacteristic(
    const nsAString& aCharacteristicUuid,
    const GattPermissions& aPermissions,
    const GattCharacteristicProperties& aProperties,
    const ArrayBuffer& aValue,
    ErrorResult& aRv);
  already_AddRefed<Promise> AddIncludedService(
    BluetoothGattService& aIncludedService,
    ErrorResult& aRv);

  /****************************************************************************
   * Others
   ***************************************************************************/
  const nsAString& GetAppUuid() const
  {
    return mAppUuid;
  }

  const BluetoothGattServiceId& GetServiceId() const
  {
    return mServiceId;
  }

  const BluetoothAttributeHandle& GetServiceHandle() const
  {
    return mServiceHandle;
  }

  nsPIDOMWindow* GetParentObject() const
  {
     return mOwner;
  }

  uint16_t GetHandleCount() const;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // Constructor of BluetoothGattService in ATT client role
  BluetoothGattService(nsPIDOMWindow* aOwner,
                       const nsAString& aAppUuid,
                       const BluetoothGattServiceId& aServiceId);
  // Constructor of BluetoothGattService in ATT server role
  BluetoothGattService(nsPIDOMWindow* aOwner,
                       const BluetoothGattServiceInit& aInit);

private:
  ~BluetoothGattService();

  /**
   * Add newly discovered GATT included services into mIncludedServices and
   * update the cache value of mIncludedServices.
   *
   * @param aServiceIds [in] An array of BluetoothGattServiceId for each
   *                         included service that belongs to this service.
   */
  void AssignIncludedServices(
    const nsTArray<BluetoothGattServiceId>& aServiceIds);

  /**
   * Add newly discovered GATT characteristics into mCharacteristics and
   * update the cache value of mCharacteristics.
   *
   * @param aCharacteristics [in] An array of BluetoothGattCharAttribute for
   *                              each characteristic that belongs to this
   *                              service.
   */
  void AssignCharacteristics(
    const nsTArray<BluetoothGattCharAttribute>& aCharacteristics);

  /**
   * Add newly discovered GATT descriptors into mDescriptors of
   * BluetoothGattCharacteristic and update the cache value of mDescriptors.
   *
   * @param aCharacteristicId [in] BluetoothGattId of a characteristic that
   *                               belongs to this service.
   * @param aDescriptorIds [in] An array of BluetoothGattId for each descriptor
   *                            that belongs to the characteristic referred by
   *                            aCharacteristicId.
   */
  void AssignDescriptors(
    const BluetoothGattId& aCharacteristicId,
    const nsTArray<BluetoothGattId>& aDescriptorIds);

  /**
   * Assign AppUuid of this GATT service.
   *
   * @param aAppUuid The value of AppUuid.
   */
  void AssignAppUuid(const nsAString& aAppUuid);

  /**
   * Assign the handle value for this GATT service. This function would be
   * called only after a valid handle value is retrieved from the Bluetooth
   * backend.
   *
   * @param aServiceHandle [in] The handle value of this GATT service.
   */
  void AssignServiceHandle(const BluetoothAttributeHandle& aServiceHandle);

  /**
   * Assign the handle value for one of the characteristic within this GATT
   * service. This function would be called only after a valid handle value is
   * retrieved from the Bluetooth backend.
   *
   * @param aCharacteristicUuid [in] BluetoothUuid of this GATT characteristic.
   * @param aCharacteristicHandle [in] The handle value of this GATT
   *                                   characteristic.
   */
  void AssignCharacteristicHandle(
    const BluetoothUuid& aCharacteristicUuid,
    const BluetoothAttributeHandle& aCharacteristicHandle);

  /**
   * Assign the handle value for one of the descriptor within this GATT
   * service. This function would be called only after a valid handle value is
   * retrieved from the Bluetooth backend.
   *
   * @param aDescriptorUuid [in] BluetoothUuid of this GATT descriptor.
   * @param aCharacteristicHandle [in] The handle value of this GATT
   *                                   characteristic.
   * @param aDescriptorHandle [in] The handle value of this GATT descriptor.
   */
  void AssignDescriptorHandle(
    const BluetoothUuid& aDescriptorUuid,
    const BluetoothAttributeHandle& aCharacteristicHandle,
    const BluetoothAttributeHandle& aDescriptorHandle);

  /**
   * Examine whether this GATT service can react with the Bluetooth backend.
   *
   * @return true if this service can react with the Bluetooth backend; false
   *         if this service cannot react with the Bluetooth backend.
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
   * UUID of the GATT client.
   */
  nsString mAppUuid;

  /**
   * ServiceId of this GATT service which contains
   * 1) mId.mUuid: UUID of this service in byte array format.
   * 2) mId.mInstanceId: Instance id of this service.
   * 3) mIsPrimary: Indicate whether this is a primary service or not.
   */
  BluetoothGattServiceId mServiceId;

  /**
   * UUID string of this GATT service.
   */
  nsString mUuidStr;

  /**
   * Array of discovered included services for this service.
   */
  nsTArray<nsRefPtr<BluetoothGattService>> mIncludedServices;

  /**
   * Array of discovered characteristics for this service.
   */
  nsTArray<nsRefPtr<BluetoothGattCharacteristic>> mCharacteristics;

  /**
   * ATT role of this GATT service.
   */
  const BluetoothAttRole mAttRole;

  /**
   * Activeness of this GATT service.
   *
   * True means this service does react with the Bluetooth backend. False means
   * this service doesn't react with the Bluetooth backend. The value should be
   * true if |mAttRole| equals |ATT_CLIENT_ROLE| because the service instance
   * could be created only when the Bluetooth backend has found one GATT
   * service. The value would be false at the beginning if |mAttRole| equals
   * |ATT_SERVER_ROLE|. Then the value would become true later if this GATT
   * service has been added into Bluetooth backend.
   */
  bool mActive;

  /**
   * Handle of this GATT service.
   *
   * The value is only valid if |mAttRole| equals |ATT_SERVER_ROLE|.
   */
  BluetoothAttributeHandle mServiceHandle;

  /**
   * Total count of handles of this GATT service itself.
   */
  static const uint16_t sHandleCount;
};

END_BLUETOOTH_NAMESPACE

/**
 * Explicit Specialization of Function Templates
 *
 * Allows customizing the template code for a given set of template arguments.
 * With this function template, nsTArray can handle comparison between
 * 'nsRefPtr<BluetoothGattService>' and 'BluetoothGattServiceId' properly,
 * including IndexOf() and Contains();
 */
template <>
class nsDefaultComparator <
  nsRefPtr<mozilla::dom::bluetooth::BluetoothGattService>,
  mozilla::dom::bluetooth::BluetoothGattServiceId> {
public:
  bool Equals(
    const nsRefPtr<mozilla::dom::bluetooth::BluetoothGattService>& aService,
    const mozilla::dom::bluetooth::BluetoothGattServiceId& aServiceId) const
  {
    return aService->GetServiceId() == aServiceId;
  }
};

/**
 * Explicit Specialization of Function Templates
 *
 * Allows customizing the template code for a given set of template arguments.
 * With this function template, nsTArray can handle comparison between
 * 'nsRefPtr<BluetoothGattService>' and 'BluetoothAttributeHandle' properly,
 * including IndexOf() and Contains();
 */
template <>
class nsDefaultComparator <
  nsRefPtr<mozilla::dom::bluetooth::BluetoothGattService>,
  mozilla::dom::bluetooth::BluetoothAttributeHandle> {
public:
  bool Equals(
    const nsRefPtr<mozilla::dom::bluetooth::BluetoothGattService>& aService,
    const mozilla::dom::bluetooth::BluetoothAttributeHandle& aServiceHandle)
    const
  {
    return aService->GetServiceHandle() == aServiceHandle;
  }
};

#endif // mozilla_dom_bluetooth_BluetoothGattService_h
