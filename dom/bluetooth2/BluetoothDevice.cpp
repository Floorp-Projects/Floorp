/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothClassOfDevice.h"
#include "BluetoothDevice.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothUtils.h"

#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/dom/BluetoothAttributeEvent.h"
#include "mozilla/dom/BluetoothDevice2Binding.h"
#include "mozilla/dom/Promise.h"

using namespace mozilla;
using namespace mozilla::dom;

USING_BLUETOOTH_NAMESPACE

NS_IMPL_CYCLE_COLLECTION_INHERITED(BluetoothDevice, DOMEventTargetHelper, mCod)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BluetoothDevice)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(BluetoothDevice, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(BluetoothDevice, DOMEventTargetHelper)

class FetchUuidsTask : public BluetoothReplyRunnable
{
public:
  FetchUuidsTask(Promise* aPromise,
                 const nsAString& aName,
                 BluetoothDevice* aDevice)
    : BluetoothReplyRunnable(nullptr /* DOMRequest */, aPromise, aName)
    , mDevice(aDevice)
  {
    MOZ_ASSERT(aPromise);
    MOZ_ASSERT(aDevice);
  }

  bool ParseSuccessfulReply(JS::MutableHandle<JS::Value> aValue)
  {
    aValue.setUndefined();

    const BluetoothValue& v = mReply->get_BluetoothReplySuccess().value();
    NS_ENSURE_TRUE(v.type() == BluetoothValue::TArrayOfnsString, false);
    const InfallibleTArray<nsString>& uuids = v.get_ArrayOfnsString();

    AutoJSAPI jsapi;
    NS_ENSURE_TRUE(jsapi.Init(mDevice->GetParentObject()), false);

    JSContext* cx = jsapi.cx();
    if (!ToJSValue(cx, uuids, aValue)) {
      BT_WARNING("Cannot create JS array!");
      JS_ClearPendingException(cx);
      return false;
    }

    return true;
  }

  virtual void ReleaseMembers() MOZ_OVERRIDE
  {
    BluetoothReplyRunnable::ReleaseMembers();
    mDevice = nullptr;
  }

private:
  nsRefPtr<BluetoothDevice> mDevice;
};

BluetoothDevice::BluetoothDevice(nsPIDOMWindow* aWindow,
                                 const BluetoothValue& aValue)
  : DOMEventTargetHelper(aWindow)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(IsDOMBinding());

  mCod = BluetoothClassOfDevice::Create(aWindow);

  const InfallibleTArray<BluetoothNamedValue>& values =
    aValue.get_ArrayOfBluetoothNamedValue();
  for (uint32_t i = 0; i < values.Length(); ++i) {
    SetPropertyByValue(values[i]);
  }

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);
  bs->RegisterBluetoothSignalHandler(mAddress, this);
}

BluetoothDevice::~BluetoothDevice()
{
  BluetoothService* bs = BluetoothService::Get();
  // bs can be null on shutdown, where destruction might happen.
  NS_ENSURE_TRUE_VOID(bs);
  bs->UnregisterBluetoothSignalHandler(mAddress, this);
}

void
BluetoothDevice::DisconnectFromOwner()
{
  DOMEventTargetHelper::DisconnectFromOwner();

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);
  bs->UnregisterBluetoothSignalHandler(mAddress, this);
}

void
BluetoothDevice::SetPropertyByValue(const BluetoothNamedValue& aValue)
{
  const nsString& name = aValue.name();
  const BluetoothValue& value = aValue.value();
  if (name.EqualsLiteral("Name")) {
    mName = value.get_nsString();
  } else if (name.EqualsLiteral("Address")) {
    mAddress = value.get_nsString();
  } else if (name.EqualsLiteral("Cod")) {
    mCod->Update(value.get_uint32_t());
  } else if (name.EqualsLiteral("Paired")) {
    mPaired = value.get_bool();
  } else if (name.EqualsLiteral("UUIDs")) {
    // We assume the received uuids array is sorted without duplicate items.
    // If it's not, we require additional processing before assigning it
    // directly.
    mUuids = value.get_ArrayOfnsString();
    BluetoothDeviceBinding::ClearCachedUuidsValue(this);
  } else {
    BT_WARNING("Not handling device property: %s",
               NS_ConvertUTF16toUTF8(name).get());
  }
}

already_AddRefed<Promise>
BluetoothDevice::FetchUuids(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Promise> promise = new Promise(global);

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, NS_ERROR_NOT_AVAILABLE);

  nsRefPtr<BluetoothReplyRunnable> result =
    new FetchUuidsTask(promise,
                       NS_LITERAL_STRING("FetchUuids"),
                       this);

  nsresult rv = bs->FetchUuidsInternal(mAddress, result);
  BT_ENSURE_TRUE_REJECT(NS_SUCCEEDED(rv), NS_ERROR_DOM_OPERATION_ERR);

  return promise.forget();
}

// static
already_AddRefed<BluetoothDevice>
BluetoothDevice::Create(nsPIDOMWindow* aWindow,
                        const BluetoothValue& aValue)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  nsRefPtr<BluetoothDevice> device = new BluetoothDevice(aWindow, aValue);
  return device.forget();
}

void
BluetoothDevice::Notify(const BluetoothSignal& aData)
{
  BT_LOGD("[D] %s", NS_ConvertUTF16toUTF8(aData.name()).get());

  BluetoothValue v = aData.value();
  if (aData.name().EqualsLiteral("PropertyChanged")) {
    HandlePropertyChanged(v);
  } else {
    BT_WARNING("Not handling device signal: %s",
               NS_ConvertUTF16toUTF8(aData.name()).get());
  }
}

BluetoothDeviceAttribute
BluetoothDevice::ConvertStringToDeviceAttribute(const nsAString& aString)
{
  using namespace
    mozilla::dom::BluetoothDeviceAttributeValues;

  for (size_t index = 0; index < ArrayLength(strings) - 1; index++) {
    if (aString.LowerCaseEqualsASCII(strings[index].value,
                                     strings[index].length)) {
      return static_cast<BluetoothDeviceAttribute>(index);
    }
  }

  return BluetoothDeviceAttribute::Unknown;
}

bool
BluetoothDevice::IsDeviceAttributeChanged(BluetoothDeviceAttribute aType,
                                          const BluetoothValue& aValue)
{
  switch (aType) {
    case BluetoothDeviceAttribute::Cod:
      MOZ_ASSERT(aValue.type() == BluetoothValue::Tuint32_t);
      return !mCod->Equals(aValue.get_uint32_t());
    case BluetoothDeviceAttribute::Name:
      MOZ_ASSERT(aValue.type() == BluetoothValue::TnsString);
      return !mName.Equals(aValue.get_nsString());
    case BluetoothDeviceAttribute::Paired:
      MOZ_ASSERT(aValue.type() == BluetoothValue::Tbool);
      return mPaired != aValue.get_bool();
    case BluetoothDeviceAttribute::Uuids: {
      MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfnsString);
      const InfallibleTArray<nsString>& uuids = aValue.get_ArrayOfnsString();
      // We assume the received uuids array is sorted without duplicate items.
      // If it's not, we require additional processing before comparing it
      // directly.
      return mUuids != uuids;
    }
    default:
      BT_WARNING("Type %d is not handled", uint32_t(aType));
      return false;
  }
}

void
BluetoothDevice::HandlePropertyChanged(const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);

  const InfallibleTArray<BluetoothNamedValue>& arr =
    aValue.get_ArrayOfBluetoothNamedValue();

  nsTArray<nsString> types;
  for (uint32_t i = 0, propCount = arr.Length(); i < propCount; ++i) {
    BluetoothDeviceAttribute type =
      ConvertStringToDeviceAttribute(arr[i].name());

    // Non-BluetoothDeviceAttribute properties
    if (type == BluetoothDeviceAttribute::Unknown) {
      SetPropertyByValue(arr[i]);
      continue;
    }

    // BluetoothDeviceAttribute properties
    if (IsDeviceAttributeChanged(type, arr[i].value())) {
      SetPropertyByValue(arr[i]);
      BT_APPEND_ENUM_STRING(types, BluetoothDeviceAttribute, type);
    }
  }

  DispatchAttributeEvent(types);
}

void
BluetoothDevice::DispatchAttributeEvent(const nsTArray<nsString>& aTypes)
{
  NS_ENSURE_TRUE_VOID(aTypes.Length());

  AutoJSAPI jsapi;
  NS_ENSURE_TRUE_VOID(jsapi.Init(GetOwner()));
  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> value(cx);

  if (!ToJSValue(cx, aTypes, &value)) {
    JS_ClearPendingException(cx);
    return;
  }

  RootedDictionary<BluetoothAttributeEventInit> init(cx);
  init.mAttrs = value;
  nsRefPtr<BluetoothAttributeEvent> event =
    BluetoothAttributeEvent::Constructor(this,
                                         NS_LITERAL_STRING("attributechanged"),
                                         init);
  DispatchTrustedEvent(event);
}

JSObject*
BluetoothDevice::WrapObject(JSContext* aContext)
{
  return BluetoothDeviceBinding::Wrap(aContext, this);
}
