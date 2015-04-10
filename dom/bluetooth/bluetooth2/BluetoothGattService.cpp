/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(
  BluetoothGattService, mOwner, mIncludedServices, mCharacteristics)
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

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  // Generate bluetooth signal path and a string representation to provide
  // uuid of this service to applications
  nsString path;
  GeneratePathFromGattId(mServiceId.mId, path, mUuidStr);
  bs->RegisterBluetoothSignalHandler(path, this);
}

BluetoothGattService::~BluetoothGattService()
{
  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  nsString path;
  GeneratePathFromGattId(mServiceId.mId, path);
  bs->UnregisterBluetoothSignalHandler(path, this);
}

void
BluetoothGattService::HandleIncludedServicesDiscovered(
  const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothGattServiceId);

  const InfallibleTArray<BluetoothGattServiceId>& includedServIds =
    aValue.get_ArrayOfBluetoothGattServiceId();

  for (uint32_t i = 0; i < includedServIds.Length(); i++) {
    mIncludedServices.AppendElement(new BluetoothGattService(
      GetParentObject(), mAppUuid, includedServIds[i]));
  }

  BluetoothGattServiceBinding::ClearCachedIncludedServicesValue(this);
}

void
BluetoothGattService::HandleCharacteristicsDiscovered(
  const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothGattId);

  const InfallibleTArray<BluetoothGattId>& characteristicIds =
    aValue.get_ArrayOfBluetoothGattId();

  for (uint32_t i = 0; i < characteristicIds.Length(); i++) {
    mCharacteristics.AppendElement(new BluetoothGattCharacteristic(
      GetParentObject(), this, characteristicIds[i]));
  }

  BluetoothGattServiceBinding::ClearCachedCharacteristicsValue(this);
}

void
BluetoothGattService::Notify(const BluetoothSignal& aData)
{
  BT_LOGD("[D] %s", NS_ConvertUTF16toUTF8(aData.name()).get());

  BluetoothValue v = aData.value();
  if (aData.name().EqualsLiteral("IncludedServicesDiscovered")) {
    HandleIncludedServicesDiscovered(v);
  } else if (aData.name().EqualsLiteral("CharacteristicsDiscovered")) {
    HandleCharacteristicsDiscovered(v);
  } else {
    BT_WARNING("Not handling GATT Service signal: %s",
               NS_ConvertUTF16toUTF8(aData.name()).get());
  }
}

JSObject*
BluetoothGattService::WrapObject(JSContext* aContext,
                                 JS::Handle<JSObject*> aGivenProto)
{
  return BluetoothGattServiceBinding::Wrap(aContext, this, aGivenProto);
}
