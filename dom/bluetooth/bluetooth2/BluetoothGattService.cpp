/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothService.h"
#include "BluetoothUtils.h"
#include "mozilla/dom/BluetoothGattServiceBinding.h"
#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "mozilla/dom/bluetooth/BluetoothGattCharacteristic.h"
#include "mozilla/dom/bluetooth/BluetoothGattService.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"

using namespace mozilla;
using namespace mozilla::dom;

USING_BLUETOOTH_NAMESPACE

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(BluetoothGattService,
                                      mOwner,
                                      mIncludedServices,
                                      mCharacteristics)

NS_IMPL_CYCLE_COLLECTING_ADDREF(BluetoothGattService)
NS_IMPL_CYCLE_COLLECTING_RELEASE(BluetoothGattService)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BluetoothGattService)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

BluetoothGattService::BluetoothGattService(
  nsPIDOMWindow* aOwner, const nsAString& aAppUuid,
  const BluetoothGattServiceId& aServiceId)
  : mOwner(aOwner)
  , mAppUuid(aAppUuid)
  , mServiceId(aServiceId)
{
  MOZ_ASSERT(aOwner);
  MOZ_ASSERT(!mAppUuid.IsEmpty());

  UuidToString(mServiceId.mId.mUuid, mUuidStr);
}

BluetoothGattService::~BluetoothGattService()
{
}

void
BluetoothGattService::AssignIncludedServices(
  const nsTArray<BluetoothGattServiceId>& aServiceIds)
{
  mIncludedServices.Clear();
  for (uint32_t i = 0; i < aServiceIds.Length(); i++) {
    mIncludedServices.AppendElement(new BluetoothGattService(
      GetParentObject(), mAppUuid, aServiceIds[i]));
  }

  BluetoothGattServiceBinding::ClearCachedIncludedServicesValue(this);
}

void
BluetoothGattService::AssignCharacteristics(
  const nsTArray<BluetoothGattCharAttribute>& aCharacteristics)
{
  mCharacteristics.Clear();
  for (uint32_t i = 0; i < aCharacteristics.Length(); i++) {
    mCharacteristics.AppendElement(new BluetoothGattCharacteristic(
      GetParentObject(), this, aCharacteristics[i]));
  }

  BluetoothGattServiceBinding::ClearCachedCharacteristicsValue(this);
}

void
BluetoothGattService::AssignDescriptors(
  const BluetoothGattId& aCharacteristicId,
  const nsTArray<BluetoothGattId>& aDescriptorIds)
{
  size_t index = mCharacteristics.IndexOf(aCharacteristicId);
  NS_ENSURE_TRUE_VOID(index != mCharacteristics.NoIndex);

  nsRefPtr<BluetoothGattCharacteristic> characteristic =
    mCharacteristics.ElementAt(index);
  characteristic->AssignDescriptors(aDescriptorIds);
}

JSObject*
BluetoothGattService::WrapObject(JSContext* aContext,
                                 JS::Handle<JSObject*> aGivenProto)
{
  return BluetoothGattServiceBinding::Wrap(aContext, this, aGivenProto);
}
