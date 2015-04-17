/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothUtils.h"
#include "mozilla/dom/BluetoothGattDescriptorBinding.h"
#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "mozilla/dom/bluetooth/BluetoothGattCharacteristic.h"
#include "mozilla/dom/bluetooth/BluetoothGattDescriptor.h"
#include "mozilla/dom/bluetooth/BluetoothGattService.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"

using namespace mozilla;
using namespace mozilla::dom;

USING_BLUETOOTH_NAMESPACE

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(
  BluetoothGattDescriptor, mOwner, mCharacteristic)
NS_IMPL_CYCLE_COLLECTING_ADDREF(BluetoothGattDescriptor)
NS_IMPL_CYCLE_COLLECTING_RELEASE(BluetoothGattDescriptor)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BluetoothGattDescriptor)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

BluetoothGattDescriptor::BluetoothGattDescriptor(
  nsPIDOMWindow* aOwner,
  BluetoothGattCharacteristic* aCharacteristic,
  const BluetoothGattId& aDescriptorId)
  : mOwner(aOwner)
  , mCharacteristic(aCharacteristic)
  , mDescriptorId(aDescriptorId)
{
  MOZ_ASSERT(aOwner);
  MOZ_ASSERT(aCharacteristic);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  // Generate bluetooth signal path and a string representation to provide uuid
  // of this descriptor to applications
  nsString path;
  GeneratePathFromGattId(mDescriptorId, path, mUuidStr);
  bs->RegisterBluetoothSignalHandler(path, this);
}

BluetoothGattDescriptor::~BluetoothGattDescriptor()
{
  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  nsString path;
  GeneratePathFromGattId(mDescriptorId, path);
  bs->UnregisterBluetoothSignalHandler(path, this);
}

void
BluetoothGattDescriptor::HandleDescriptorValueUpdated(
  const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfuint8_t);

  mValue = aValue.get_ArrayOfuint8_t();
}

void
BluetoothGattDescriptor::Notify(const BluetoothSignal& aData)
{
  BT_LOGD("[D] %s", NS_ConvertUTF16toUTF8(aData.name()).get());

  BluetoothValue v = aData.value();
  if (aData.name().EqualsLiteral("DescriptorValueUpdated")) {
    HandleDescriptorValueUpdated(v);
  } else {
    BT_WARNING("Not handling GATT Descriptor signal: %s",
               NS_ConvertUTF16toUTF8(aData.name()).get());
  }
}

JSObject*
BluetoothGattDescriptor::WrapObject(JSContext* aContext,
                                    JS::Handle<JSObject*> aGivenProto)
{
  return BluetoothGattDescriptorBinding::Wrap(aContext, this, aGivenProto);
}

void
BluetoothGattDescriptor::GetValue(JSContext* cx,
                                  JS::MutableHandle<JSObject*> aValue) const
{
  MOZ_ASSERT(aValue);

  aValue.set(mValue.IsEmpty()
             ? nullptr
             : ArrayBuffer::Create(cx, mValue.Length(), mValue.Elements()));
}

class ReadValueTask final : public BluetoothReplyRunnable
{
public:
  ReadValueTask(BluetoothGattDescriptor* aDescriptor, Promise* aPromise)
    : BluetoothReplyRunnable(
        nullptr, aPromise,
        NS_LITERAL_STRING("GattClientReadDescriptorValue"))
    , mDescriptor(aDescriptor)
  {
    MOZ_ASSERT(aDescriptor);
    MOZ_ASSERT(aPromise);
  }

  bool
  ParseSuccessfulReply(JS::MutableHandle<JS::Value> aValue)
  {
    aValue.setUndefined();

    const BluetoothValue& v = mReply->get_BluetoothReplySuccess().value();
    NS_ENSURE_TRUE(v.type() == BluetoothValue::TArrayOfuint8_t, false);

    AutoJSAPI jsapi;
    NS_ENSURE_TRUE(jsapi.Init(mDescriptor->GetParentObject()), false);

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
    mDescriptor = nullptr;
  }

private:
  nsRefPtr<BluetoothGattDescriptor> mDescriptor;
};

already_AddRefed<Promise>
BluetoothGattDescriptor::ReadValue(ErrorResult& aRv)
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

  nsRefPtr<BluetoothReplyRunnable> result = new ReadValueTask(this, promise);
  bs->GattClientReadDescriptorValueInternal(
    mCharacteristic->Service()->GetAppUuid(),
    mCharacteristic->Service()->GetServiceId(),
    mCharacteristic->GetCharacteristicId(),
    mDescriptorId,
    result);

  return promise.forget();
}

already_AddRefed<Promise>
BluetoothGattDescriptor::WriteValue(
  const RootedTypedArray<ArrayBuffer>& aValue, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  aValue.ComputeLengthAndData();

  nsTArray<uint8_t> value;
  value.AppendElements(aValue.Data(), aValue.Length());

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, NS_ERROR_NOT_AVAILABLE);

  nsRefPtr<BluetoothReplyRunnable> result = new BluetoothVoidReplyRunnable(
    nullptr, promise, NS_LITERAL_STRING("GattClientWriteDescriptorValue"));
  bs->GattClientWriteDescriptorValueInternal(
    mCharacteristic->Service()->GetAppUuid(),
    mCharacteristic->Service()->GetServiceId(),
    mCharacteristic->GetCharacteristicId(),
    mDescriptorId,
    value,
    result);

  return promise.forget();
}
