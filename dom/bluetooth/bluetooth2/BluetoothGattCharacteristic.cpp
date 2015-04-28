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

BluetoothGattCharacteristic::BluetoothGattCharacteristic(
  nsPIDOMWindow* aOwner,
  BluetoothGattService* aService,
  const BluetoothGattCharAttribute& aChar)
  : mOwner(aOwner)
  , mService(aService)
  , mCharId(aChar.mId)
  , mProperties(aChar.mProperties)
  , mWriteType(aChar.mWriteType)
{
  MOZ_ASSERT(aOwner);
  MOZ_ASSERT(mService);

  // Generate bluetooth signal path and a string representation to provide uuid
  // of this characteristic to applications
  nsString path;
  GeneratePathFromGattId(mCharId, path, mUuidStr);
  RegisterBluetoothSignalHandler(path, this);
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

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);
  BT_ENSURE_TRUE_REJECT(mService, promise, NS_ERROR_NOT_AVAILABLE);

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
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);
  BT_ENSURE_TRUE_REJECT(mService, promise, NS_ERROR_NOT_AVAILABLE);

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
BluetoothGattCharacteristic::HandleCharacteristicValueUpdated(
  const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfuint8_t);

  mValue = aValue.get_ArrayOfuint8_t();
}

void
BluetoothGattCharacteristic::Notify(const BluetoothSignal& aData)
{
  BT_LOGD("[D] %s", NS_ConvertUTF16toUTF8(aData.name()).get());
  NS_ENSURE_TRUE_VOID(mSignalRegistered);

  BluetoothValue v = aData.value();
  if (aData.name().EqualsLiteral("DescriptorsDiscovered")) {
    HandleDescriptorsDiscovered(v);
  } else if (aData.name().EqualsLiteral("CharacteristicValueUpdated")) {
    HandleCharacteristicValueUpdated(v);
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

void
BluetoothGattCharacteristic::GetValue(JSContext* cx,
                                      JS::MutableHandle<JSObject*> aValue) const
{
  MOZ_ASSERT(aValue);

  aValue.set(mValue.IsEmpty()
             ? nullptr
             : ArrayBuffer::Create(cx, mValue.Length(), mValue.Elements()));
}

void
BluetoothGattCharacteristic::GetProperties(
  mozilla::dom::GattCharacteristicProperties& aProperties) const
{
  aProperties.mBroadcast = mProperties & GATT_CHAR_PROP_BIT_BROADCAST;
  aProperties.mRead = mProperties & GATT_CHAR_PROP_BIT_READ;
  aProperties.mWriteNoResponse = mProperties & GATT_CHAR_PROP_BIT_WRITE_NO_RESPONSE;
  aProperties.mWrite = mProperties & GATT_CHAR_PROP_BIT_WRITE;
  aProperties.mNotify = mProperties & GATT_CHAR_PROP_BIT_NOTIFY;
  aProperties.mIndicate = mProperties & GATT_CHAR_PROP_BIT_INDICATE;
  aProperties.mSignedWrite = mProperties & GATT_CHAR_PROP_BIT_SIGNED_WRITE;
  aProperties.mExtendedProps = mProperties & GATT_CHAR_PROP_BIT_EXTENDED_PROPERTIES;
}

class ReadValueTask final : public BluetoothReplyRunnable
{
public:
  ReadValueTask(BluetoothGattCharacteristic* aCharacteristic, Promise* aPromise)
    : BluetoothReplyRunnable(
        nullptr, aPromise,
        NS_LITERAL_STRING("GattClientReadCharacteristicValue"))
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
      JS_ClearPendingException(cx);
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
  nsRefPtr<BluetoothGattCharacteristic> mCharacteristic;
};

already_AddRefed<Promise>
BluetoothGattCharacteristic::ReadValue(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BT_ENSURE_TRUE_REJECT(mProperties & GATT_CHAR_PROP_BIT_READ,
                        promise,
                        NS_ERROR_NOT_AVAILABLE);

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  nsRefPtr<BluetoothReplyRunnable> result = new ReadValueTask(this, promise);
  bs->GattClientReadCharacteristicValueInternal(mService->GetAppUuid(),
                                                mService->GetServiceId(),
                                                mCharId,
                                                result);

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

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BT_ENSURE_TRUE_REJECT(mProperties &
                          (GATT_CHAR_PROP_BIT_WRITE_NO_RESPONSE ||
                           GATT_CHAR_PROP_BIT_WRITE ||
                           GATT_CHAR_PROP_BIT_SIGNED_WRITE),
                        promise,
                        NS_ERROR_NOT_AVAILABLE);

  aValue.ComputeLengthAndData();

  nsTArray<uint8_t> value;
  value.AppendElements(aValue.Data(), aValue.Length());

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  nsRefPtr<BluetoothReplyRunnable> result = new BluetoothVoidReplyRunnable(
    nullptr, promise, NS_LITERAL_STRING("GattClientWriteCharacteristicValue"));
  bs->GattClientWriteCharacteristicValueInternal(mService->GetAppUuid(),
                                                 mService->GetServiceId(),
                                                 mCharId,
                                                 mWriteType,
                                                 value,
                                                 result);

  return promise.forget();
}
