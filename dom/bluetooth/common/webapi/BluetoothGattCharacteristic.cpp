/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothUtils.h"
#include "mozilla/dom/BluetoothGattCharacteristicBinding.h"
#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "mozilla/dom/bluetooth/BluetoothGattCharacteristic.h"
#include "mozilla/dom/bluetooth/BluetoothGattService.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/dom/Promise.h"

using namespace mozilla;
using namespace mozilla::dom;

USING_BLUETOOTH_NAMESPACE

NS_IMPL_CYCLE_COLLECTION_CLASS(BluetoothGattCharacteristic)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(BluetoothGattCharacteristic)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwner)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mService)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDescriptors)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER

  /**
   * Unregister the bluetooth signal handler after unlinked.
   *
   * This is needed to avoid ending up with exposing a deleted object to JS or
   * accessing deleted objects while receiving signals from parent process
   * after unlinked. Please see Bug 1138267 for detail informations.
   */
  nsString path;
  GeneratePathFromGattId(tmp->mCharId, path);
  UnregisterBluetoothSignalHandler(path, tmp);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(BluetoothGattCharacteristic)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mService)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDescriptors)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(BluetoothGattCharacteristic)

NS_IMPL_CYCLE_COLLECTING_ADDREF(BluetoothGattCharacteristic)
NS_IMPL_CYCLE_COLLECTING_RELEASE(BluetoothGattCharacteristic)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BluetoothGattCharacteristic)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/*
 * "A characteristic definition shall contain a characteristic declaration, a
 *  Characteristic Value declaration and may contain characteristic descriptor
 *  declarations."
 *  ...
 * "Each declaration above is contained in a separate Attribute. Two required
 *  declarations are the characteristic declaration and the Characteristic
 *  Value declaration."
 * -- Bluetooth Core Specification version 4.2, Volume 3, Part G, Section 3.3
 */
const uint16_t BluetoothGattCharacteristic::sHandleCount = 2;

// Constructor of BluetoothGattCharacteristic in ATT client role
BluetoothGattCharacteristic::BluetoothGattCharacteristic(
  nsPIDOMWindowInner* aOwner,
  BluetoothGattService* aService,
  const BluetoothGattCharAttribute& aChar)
  : mOwner(aOwner)
  , mService(aService)
  , mCharId(aChar.mId)
  , mPermissions(BLUETOOTH_EMPTY_GATT_ATTR_PERM)
  , mProperties(aChar.mProperties)
  , mWriteType(aChar.mWriteType)
  , mAttRole(ATT_CLIENT_ROLE)
  , mActive(true)
{
  MOZ_ASSERT(aOwner);
  MOZ_ASSERT(mService);

  UuidToString(mCharId.mUuid, mUuidStr);

  // Generate bluetooth signal path of this characteristic to applications
  nsString path;
  GeneratePathFromGattId(mCharId, path);
  RegisterBluetoothSignalHandler(path, this);
}


// Constructor of BluetoothGattCharacteristic in ATT server role
BluetoothGattCharacteristic::BluetoothGattCharacteristic(
  nsPIDOMWindowInner* aOwner,
  BluetoothGattService* aService,
  const nsAString& aCharacteristicUuid,
  const GattPermissions& aPermissions,
  const GattCharacteristicProperties& aProperties,
  const ArrayBuffer& aValue)
  : mOwner(aOwner)
  , mService(aService)
  , mUuidStr(aCharacteristicUuid)
  , mPermissions(BLUETOOTH_EMPTY_GATT_ATTR_PERM)
  , mProperties(BLUETOOTH_EMPTY_GATT_CHAR_PROP)
  , mWriteType(GATT_WRITE_TYPE_NORMAL)
  , mAttRole(ATT_SERVER_ROLE)
  , mActive(false)
{
  MOZ_ASSERT(aOwner);
  MOZ_ASSERT(aService);

  // UUID
  memset(&mCharId, 0, sizeof(mCharId));
  StringToUuid(aCharacteristicUuid, mCharId.mUuid);

  // permissions
  GattPermissionsToBits(aPermissions, mPermissions);

  // properties
  GattPropertiesToBits(aProperties, mProperties);

  // value
  aValue.ComputeLengthAndData();
  mValue.AppendElements(aValue.Data(), aValue.Length());
}

BluetoothGattCharacteristic::~BluetoothGattCharacteristic()
{
  nsString path;
  GeneratePathFromGattId(mCharId, path);
  UnregisterBluetoothSignalHandler(path, this);
}

already_AddRefed<Promise>
BluetoothGattCharacteristic::StartNotifications(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BT_ENSURE_TRUE_REJECT(mAttRole == ATT_CLIENT_ROLE,
                        promise,
                        NS_ERROR_UNEXPECTED);

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);
  BT_ENSURE_TRUE_REJECT(mService, promise, NS_ERROR_NOT_AVAILABLE);

  BluetoothUuid appUuid;
  BT_ENSURE_TRUE_REJECT(NS_SUCCEEDED(StringToUuid(mService->GetAppUuid(),
                                                  appUuid)),
                        promise,
                        NS_ERROR_DOM_OPERATION_ERR);

  bs->GattClientStartNotificationsInternal(
    appUuid, mService->GetServiceId(), mCharId,
    new BluetoothVoidReplyRunnable(nullptr, promise));

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

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BT_ENSURE_TRUE_REJECT(mAttRole == ATT_CLIENT_ROLE,
                        promise,
                        NS_ERROR_UNEXPECTED);

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);
  BT_ENSURE_TRUE_REJECT(mService, promise, NS_ERROR_NOT_AVAILABLE);

  BluetoothUuid appUuid;
  BT_ENSURE_TRUE_REJECT(NS_SUCCEEDED(StringToUuid(mService->GetAppUuid(),
                                                  appUuid)),
                        promise,
                        NS_ERROR_DOM_OPERATION_ERR);

  bs->GattClientStopNotificationsInternal(
    appUuid, mService->GetServiceId(), mCharId,
    new BluetoothVoidReplyRunnable(nullptr, promise));

  return promise.forget();
}

void
BluetoothGattCharacteristic::AssignDescriptors(
  const nsTArray<BluetoothGattId>& aDescriptorIds)
{
  mDescriptors.Clear();
  for (uint32_t i = 0; i < aDescriptorIds.Length(); i++) {
    mDescriptors.AppendElement(new BluetoothGattDescriptor(
      GetParentObject(), this, aDescriptorIds[i]));
  }

  BluetoothGattCharacteristicBinding::ClearCachedDescriptorsValue(this);
}

void
BluetoothGattCharacteristic::HandleCharacteristicValueUpdated(
  const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfuint8_t);

  mValue = aValue.get_ArrayOfuint8_t();
}

void
BluetoothGattCharacteristic::AssignCharacteristicHandle(
  const BluetoothAttributeHandle& aCharacteristicHandle)
{
  MOZ_ASSERT(mAttRole == ATT_SERVER_ROLE);
  MOZ_ASSERT(!mActive);
  MOZ_ASSERT(!mCharacteristicHandle.mHandle);

  mCharacteristicHandle = aCharacteristicHandle;
  mActive = true;
}

void
BluetoothGattCharacteristic::AssignDescriptorHandle(
  const BluetoothUuid& aDescriptorUuid,
  const BluetoothAttributeHandle& aDescriptorHandle)
{
  MOZ_ASSERT(mAttRole == ATT_SERVER_ROLE);
  MOZ_ASSERT(mActive);

  size_t index = mDescriptors.IndexOf(aDescriptorUuid);
  NS_ENSURE_TRUE_VOID(index != mDescriptors.NoIndex);
  mDescriptors[index]->AssignDescriptorHandle(aDescriptorHandle);
}

void
BluetoothGattCharacteristic::Notify(const BluetoothSignal& aData)
{
  BT_LOGD("[D] %s", NS_ConvertUTF16toUTF8(aData.name()).get());
  NS_ENSURE_TRUE_VOID(mSignalRegistered);

  BluetoothValue v = aData.value();
  if (aData.name().EqualsLiteral("CharacteristicValueUpdated")) {
    HandleCharacteristicValueUpdated(v);
  } else {
    BT_WARNING("Not handling GATT Characteristic signal: %s",
               NS_ConvertUTF16toUTF8(aData.name()).get());
  }
}

void
BluetoothGattCharacteristic::GetUuid(BluetoothUuid& aUuid) const
{
  aUuid = mCharId.mUuid;
}

uint16_t
BluetoothGattCharacteristic::GetHandleCount() const
{
  uint16_t count = sHandleCount;
  for (size_t i = 0; i < mDescriptors.Length(); ++i) {
    count += mDescriptors[i]->GetHandleCount();
  }
  return count;
}

JSObject*
BluetoothGattCharacteristic::WrapObject(JSContext* aContext,
                                        JS::Handle<JSObject*> aGivenProto)
{
  return BluetoothGattCharacteristicBinding::Wrap(aContext, this, aGivenProto);
}

void
BluetoothGattCharacteristic::GetValue(JSContext* cx,
                                      JS::MutableHandle<JSObject*> aValue) const
{
  aValue.set(mValue.IsEmpty()
             ? nullptr
             : ArrayBuffer::Create(cx, mValue.Length(), mValue.Elements()));
}

void
BluetoothGattCharacteristic::GetPermissions(
  GattPermissions& aPermissions) const
{
  GattPermissionsToDictionary(mPermissions, aPermissions);
}

void
BluetoothGattCharacteristic::GetProperties(
  GattCharacteristicProperties& aProperties) const
{
  GattPropertiesToDictionary(mProperties, aProperties);
}

class ReadValueTask final : public BluetoothReplyRunnable
{
public:
  ReadValueTask(BluetoothGattCharacteristic* aCharacteristic, Promise* aPromise)
    : BluetoothReplyRunnable(nullptr, aPromise)
    , mCharacteristic(aCharacteristic)
  {
    MOZ_ASSERT(aCharacteristic);
    MOZ_ASSERT(aPromise);
  }

  bool
  ParseSuccessfulReply(JS::MutableHandle<JS::Value> aValue)
  {
    aValue.setUndefined();

    const BluetoothValue& v = mReply->get_BluetoothReplySuccess().value();
    NS_ENSURE_TRUE(v.type() == BluetoothValue::TArrayOfuint8_t, false);

    AutoJSAPI jsapi;
    NS_ENSURE_TRUE(jsapi.Init(mCharacteristic->GetParentObject()), false);

    JSContext* cx = jsapi.cx();
    if (!ToJSValue(cx, v.get_ArrayOfuint8_t(), aValue)) {
      jsapi.ClearException();
      return false;
    }

    return true;
  }

  void
  ReleaseMembers()
  {
    BluetoothReplyRunnable::ReleaseMembers();
    mCharacteristic = nullptr;
  }

private:
  RefPtr<BluetoothGattCharacteristic> mCharacteristic;
};

already_AddRefed<Promise>
BluetoothGattCharacteristic::ReadValue(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  if (mAttRole == ATT_SERVER_ROLE) {
    promise->MaybeResolve(mValue);
    return promise.forget();
  }

  BT_ENSURE_TRUE_REJECT(mProperties & GATT_CHAR_PROP_BIT_READ,
                        promise,
                        NS_ERROR_NOT_AVAILABLE);

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  BluetoothUuid appUuid;
  BT_ENSURE_TRUE_REJECT(NS_SUCCEEDED(StringToUuid(mService->GetAppUuid(),
                                                  appUuid)),
                        promise,
                        NS_ERROR_DOM_OPERATION_ERR);

  bs->GattClientReadCharacteristicValueInternal(
    appUuid, mService->GetServiceId(), mCharId,
    new ReadValueTask(this, promise));

  return promise.forget();
}

already_AddRefed<Promise>
BluetoothGattCharacteristic::WriteValue(const ArrayBuffer& aValue,
                                        ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  aValue.ComputeLengthAndData();

  if (mAttRole == ATT_SERVER_ROLE) {
    mValue.Clear();
    mValue.AppendElements(aValue.Data(), aValue.Length());

    promise->MaybeResolveWithUndefined();
    return promise.forget();
  }

  BT_ENSURE_TRUE_REJECT(mProperties &
                          (GATT_CHAR_PROP_BIT_WRITE_NO_RESPONSE |
                           GATT_CHAR_PROP_BIT_WRITE |
                           GATT_CHAR_PROP_BIT_SIGNED_WRITE),
                        promise,
                        NS_ERROR_NOT_AVAILABLE);

  nsTArray<uint8_t> value;
  value.AppendElements(aValue.Data(), aValue.Length());

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  BluetoothUuid appUuid;
  BT_ENSURE_TRUE_REJECT(NS_SUCCEEDED(StringToUuid(mService->GetAppUuid(),
                                                  appUuid)),
                        promise,
                        NS_ERROR_DOM_OPERATION_ERR);

  bs->GattClientWriteCharacteristicValueInternal(
    appUuid, mService->GetServiceId(), mCharId, mWriteType, value,
    new BluetoothVoidReplyRunnable(nullptr, promise));

  return promise.forget();
}

already_AddRefed<Promise>
BluetoothGattCharacteristic::AddDescriptor(const nsAString& aDescriptorUuid,
                                           const GattPermissions& aPermissions,
                                           const ArrayBuffer& aValue,
                                           ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BT_ENSURE_TRUE_REJECT(mAttRole == ATT_SERVER_ROLE,
                        promise,
                        NS_ERROR_UNEXPECTED);

  /* The characteristic should not be actively acting with the Bluetooth
   * backend. Otherwise, descriptors cannot be added into the characteristic. */
  BT_ENSURE_TRUE_REJECT(!mActive, promise, NS_ERROR_UNEXPECTED);

  RefPtr<BluetoothGattDescriptor> descriptor =
    new BluetoothGattDescriptor(GetParentObject(),
                                this,
                                aDescriptorUuid,
                                aPermissions,
                                aValue);

  mDescriptors.AppendElement(descriptor);
  promise->MaybeResolve(descriptor);

  return promise.forget();
}
