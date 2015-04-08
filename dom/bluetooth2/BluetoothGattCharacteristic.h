/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothgattcharacteristic_h__
#define mozilla_dom_bluetooth_bluetoothgattcharacteristic_h__

#include "mozilla/Attributes.h"
#include "mozilla/dom/BluetoothGattCharacteristicBinding.h"
#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "mozilla/dom/bluetooth/BluetoothGattDescriptor.h"
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
                              const BluetoothGattId& aCharId);

private:
  ~BluetoothGattCharacteristic();

  /**
   * Add newly discovered GATT descriptors into mDescriptors and update the
   * cache value of mDescriptors.
   *
   * @param aValue [in] BluetoothValue which contains an array of
   *                    BluetoothGattId of all discovered descriptors.
   */
  void HandleDescriptorsDiscovered(const BluetoothValue& aValue);

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
};

END_BLUETOOTH_NAMESPACE

#endif
