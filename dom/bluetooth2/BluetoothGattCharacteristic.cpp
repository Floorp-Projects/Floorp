/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothUtils.h"
#include "mozilla/dom/BluetoothGattCharacteristicBinding.h"
#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "mozilla/dom/bluetooth/BluetoothGattCharacteristic.h"
#include "mozilla/dom/bluetooth/BluetoothGattDescriptor.h"
#include "mozilla/dom/bluetooth/BluetoothGattService.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/dom/Promise.h"

using namespace mozilla;
using namespace mozilla::dom;

USING_BLUETOOTH_NAMESPACE

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(
  BluetoothGattCharacteristic, mOwner, mService, mDescriptors)
NS_IMPL_CYCLE_COLLECTING_ADDREF(BluetoothGattCharacteristic)
NS_IMPL_CYCLE_COLLECTING_RELEASE(BluetoothGattCharacteristic)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BluetoothGattCharacteristic)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

BluetoothGattCharacteristic::BluetoothGattCharacteristic(
  nsPIDOMWindow* aOwner,
  BluetoothGattService* aService,
  const BluetoothGattId& aCharId)
  : mOwner(aOwner)
  , mService(aService)
  , mCharId(aCharId)
{
  MOZ_ASSERT(aOwner);
  MOZ_ASSERT(mService);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  // Generate bluetooth signal path and a string representation to provide uuid
  // of this characteristic to applications
  nsString path;
  GeneratePathFromGattId(mCharId, path, mUuidStr);
  bs->RegisterBluetoothSignalHandler(path, this);
}

BluetoothGattCharacteristic::~BluetoothGattCharacteristic()
{
  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  nsString path;
  GeneratePathFromGattId(mCharId, path);
  bs->UnregisterBluetoothSignalHandler(path, this);
}

already_AddRefed<Promise>
BluetoothGattCharacteristic::StartNotifications(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, NS_ERROR_NOT_AVAILABLE);
  BT_ENSURE_TRUE_REJECT(mService, NS_ERROR_NOT_AVAILABLE);

  nsRefPtr<BluetoothReplyRunnable> result =
    new BluetoothVoidReplyRunnable(
      nullptr /* DOMRequest */,
      promise,
      NS_LITERAL_STRING("GattClientStartNotifications"));
  bs->GattClientStartNotificationsInternal(mService->GetAppUuid(),
                                           mService->GetServiceId(),
                                           mCharId,
                                           result);

  return promise.forget();
}

already_AddRefed<Promise>
BluetoothGattCharacteristic::StopNotifications(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, NS_ERROR_NOT_AVAILABLE);
  BT_ENSURE_TRUE_REJECT(mService, NS_ERROR_NOT_AVAILABLE);

  nsRefPtr<BluetoothReplyRunnable> result =
    new BluetoothVoidReplyRunnable(
      nullptr /* DOMRequest */,
      promise,
      NS_LITERAL_STRING("GattClientStopNotifications"));
  bs->GattClientStopNotificationsInternal(mService->GetAppUuid(),
                                          mService->GetServiceId(),
                                          mCharId,
                                          result);

  return promise.forget();
}

void
BluetoothGattCharacteristic::HandleDescriptorsDiscovered(
  const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothGattId);

  const InfallibleTArray<BluetoothGattId>& descriptorIds =
    aValue.get_ArrayOfBluetoothGattId();

  for (uint32_t i = 0; i < descriptorIds.Length(); i++) {
    mDescriptors.AppendElement(new BluetoothGattDescriptor(
      GetParentObject(), this, descriptorIds[i]));
  }

  BluetoothGattCharacteristicBinding::ClearCachedDescriptorsValue(this);
}

void
BluetoothGattCharacteristic::Notify(const BluetoothSignal& aData)
{
  BT_LOGD("[D] %s", NS_ConvertUTF16toUTF8(aData.name()).get());

  BluetoothValue v = aData.value();
  if (aData.name().EqualsLiteral("DescriptorsDiscovered")) {
    HandleDescriptorsDiscovered(v);
  } else {
    BT_WARNING("Not handling GATT Characteristic signal: %s",
               NS_ConvertUTF16toUTF8(aData.name()).get());
  }
}

JSObject*
BluetoothGattCharacteristic::WrapObject(JSContext* aContext,
                                        JS::Handle<JSObject*> aGivenProto)
{
  return BluetoothGattCharacteristicBinding::Wrap(aContext, this, aGivenProto);
}
