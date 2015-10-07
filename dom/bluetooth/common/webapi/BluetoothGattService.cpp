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

const uint16_t BluetoothGattService::sHandleCount = 1;

// Constructor of BluetoothGattService in ATT client role
BluetoothGattService::BluetoothGattService(
  nsPIDOMWindow* aOwner, const nsAString& aAppUuid,
  const BluetoothGattServiceId& aServiceId)
  : mOwner(aOwner)
  , mAppUuid(aAppUuid)
  , mServiceId(aServiceId)
  , mAttRole(ATT_CLIENT_ROLE)
  , mActive(true)
{
  MOZ_ASSERT(aOwner);
  MOZ_ASSERT(!mAppUuid.IsEmpty());

  UuidToString(mServiceId.mId.mUuid, mUuidStr);
}

// Constructor of BluetoothGattService in ATT server role
BluetoothGattService::BluetoothGattService(
  nsPIDOMWindow* aOwner,
  const BluetoothGattServiceInit& aInit)
  : mOwner(aOwner)
  , mUuidStr(aInit.mUuid)
  , mAttRole(ATT_SERVER_ROLE)
  , mActive(false)
{
  memset(&mServiceId, 0, sizeof(mServiceId));
  StringToUuid(aInit.mUuid, mServiceId.mId.mUuid);
  mServiceId.mIsPrimary = aInit.mIsPrimary;
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

void
BluetoothGattService::AssignAppUuid(const nsAString& aAppUuid)
{
  MOZ_ASSERT(mAttRole == ATT_SERVER_ROLE);

  mAppUuid = aAppUuid;
}

void
BluetoothGattService::AssignServiceHandle(
  const BluetoothAttributeHandle& aServiceHandle)
{
  MOZ_ASSERT(mAttRole == ATT_SERVER_ROLE);
  MOZ_ASSERT(!mActive);
  MOZ_ASSERT(!mServiceHandle.mHandle);

  mServiceHandle = aServiceHandle;
  mActive = true;
}

void
BluetoothGattService::AssignCharacteristicHandle(
  const BluetoothUuid& aCharacteristicUuid,
  const BluetoothAttributeHandle& aCharacteristicHandle)
{
  MOZ_ASSERT(mAttRole == ATT_SERVER_ROLE);
  MOZ_ASSERT(mActive);

  size_t index = mCharacteristics.IndexOf(aCharacteristicUuid);
  NS_ENSURE_TRUE_VOID(index != mCharacteristics.NoIndex);
  mCharacteristics[index]->AssignCharacteristicHandle(aCharacteristicHandle);
}

void
BluetoothGattService::AssignDescriptorHandle(
  const BluetoothUuid& aDescriptorUuid,
  const BluetoothAttributeHandle& aCharacteristicHandle,
  const BluetoothAttributeHandle& aDescriptorHandle)
{
  MOZ_ASSERT(mAttRole == ATT_SERVER_ROLE);
  MOZ_ASSERT(mActive);

  size_t index = mCharacteristics.IndexOf(aCharacteristicHandle);
  NS_ENSURE_TRUE_VOID(index != mCharacteristics.NoIndex);
  mCharacteristics[index]->AssignDescriptorHandle(aDescriptorUuid,
                                                  aDescriptorHandle);
}

uint16_t
BluetoothGattService::GetHandleCount() const
{
  uint16_t count = sHandleCount;
  for (size_t i = 0; i < mCharacteristics.Length(); ++i) {
    count += mCharacteristics[i]->GetHandleCount();
  }
  return count;
}

JSObject*
BluetoothGattService::WrapObject(JSContext* aContext,
                                 JS::Handle<JSObject*> aGivenProto)
{
  return BluetoothGattServiceBinding::Wrap(aContext, this, aGivenProto);
}

already_AddRefed<BluetoothGattService>
BluetoothGattService::Constructor(const GlobalObject& aGlobal,
                                  const BluetoothGattServiceInit& aInit,
                                  ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<BluetoothGattService> service = new BluetoothGattService(window,
                                                                    aInit);

  return service.forget();
}

already_AddRefed<Promise>
BluetoothGattService::AddCharacteristic(
  const nsAString& aCharacteristicUuid,
  const GattPermissions& aPermissions,
  const GattCharacteristicProperties& aProperties,
  const ArrayBuffer& aValue,
  ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BT_ENSURE_TRUE_REJECT(mAttRole == ATT_SERVER_ROLE,
                        promise,
                        NS_ERROR_UNEXPECTED);

  /* The service should not be actively acting with the Bluetooth backend.
   * Otherwise, characteristics cannot be added into the service. */
  BT_ENSURE_TRUE_REJECT(!mActive, promise, NS_ERROR_UNEXPECTED);

  nsRefPtr<BluetoothGattCharacteristic> characteristic =
    new BluetoothGattCharacteristic(GetParentObject(),
                                    this,
                                    aCharacteristicUuid,
                                    aPermissions,
                                    aProperties,
                                    aValue);

  mCharacteristics.AppendElement(characteristic);
  promise->MaybeResolve(characteristic);

  return promise.forget();
}

already_AddRefed<Promise>
BluetoothGattService::AddIncludedService(BluetoothGattService& aIncludedService,
                                         ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BT_ENSURE_TRUE_REJECT(mAttRole == ATT_SERVER_ROLE,
                        promise,
                        NS_ERROR_UNEXPECTED);

  /* The service should not be actively acting with the Bluetooth backend.
   * Otherwise, included services cannot be added into the service. */
  BT_ENSURE_TRUE_REJECT(!mActive, promise, NS_ERROR_UNEXPECTED);

  /* The included service itself should be actively acting with the Bluetooth
   * backend. Otherwise, that service cannot be included by any services. */
  BT_ENSURE_TRUE_REJECT(aIncludedService.mActive,
                        promise,
                        NS_ERROR_UNEXPECTED);

  mIncludedServices.AppendElement(&aIncludedService);
  promise->MaybeResolve(JS::UndefinedHandleValue);

  return promise.forget();
}
