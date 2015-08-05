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

  nsPIDOMWindow* GetParentObject() const
  {
     return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  BluetoothGattService(nsPIDOMWindow* aOwner,
                       const nsAString& aAppUuid,
                       const BluetoothGattServiceId& aServiceId);

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

#endif // mozilla_dom_bluetooth_BluetoothGattService_h
