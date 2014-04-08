/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "BluetoothDevice.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothUtils.h"

#include "nsDOMClassInfo.h"
#include "nsTArrayHelpers.h"

#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/dom/BluetoothDeviceBinding.h"

USING_BLUETOOTH_NAMESPACE

DOMCI_DATA(BluetoothDevice, BluetoothDevice)

NS_IMPL_CYCLE_COLLECTION_CLASS(BluetoothDevice)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(BluetoothDevice,
                                               DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mJsUuids)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mJsServices)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(BluetoothDevice,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(BluetoothDevice,
                                                DOMEventTargetHelper)
  tmp->Unroot();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BluetoothDevice)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(BluetoothDevice, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(BluetoothDevice, DOMEventTargetHelper)

BluetoothDevice::BluetoothDevice(nsPIDOMWindow* aWindow,
                                 const nsAString& aAdapterPath,
                                 const BluetoothValue& aValue)
  : DOMEventTargetHelper(aWindow)
  , BluetoothPropertyContainer(BluetoothObjectType::TYPE_DEVICE)
  , mJsUuids(nullptr)
  , mJsServices(nullptr)
  , mAdapterPath(aAdapterPath)
  , mIsRooted(false)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(IsDOMBinding());

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
  Unroot();
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
BluetoothDevice::Root()
{
  if (!mIsRooted) {
    mozilla::HoldJSObjects(this);
    mIsRooted = true;
  }
}

void
BluetoothDevice::Unroot()
{
  if (mIsRooted) {
    mJsUuids = nullptr;
    mJsServices = nullptr;
    mozilla::DropJSObjects(this);
    mIsRooted = false;
  }
}

void
BluetoothDevice::SetPropertyByValue(const BluetoothNamedValue& aValue)
{
  const nsString& name = aValue.name();
  const BluetoothValue& value = aValue.value();
  if (name.EqualsLiteral("Name")) {
    mName = value.get_nsString();
  } else if (name.EqualsLiteral("Path")) {
    MOZ_ASSERT(value.get_nsString().Length() > 0);
    mPath = value.get_nsString();
  } else if (name.EqualsLiteral("Address")) {
    mAddress = value.get_nsString();
  } else if (name.EqualsLiteral("Class")) {
    mClass = value.get_uint32_t();
  } else if (name.EqualsLiteral("Icon")) {
    mIcon = value.get_nsString();
  } else if (name.EqualsLiteral("Connected")) {
    mConnected = value.get_bool();
  } else if (name.EqualsLiteral("Paired")) {
    mPaired = value.get_bool();
  } else if (name.EqualsLiteral("UUIDs")) {
    mUuids = value.get_ArrayOfnsString();
    nsresult rv;
    nsIScriptContext* sc = GetContextForEventHandlers(&rv);
    NS_ENSURE_SUCCESS_VOID(rv);
    NS_ENSURE_TRUE_VOID(sc);

    AutoPushJSContext cx(sc->GetNativeContext());

    JS::Rooted<JSObject*> uuids(cx);
    if (NS_FAILED(nsTArrayToJSArray(cx, mUuids, uuids.address()))) {
      BT_WARNING("Cannot set JS UUIDs object!");
      return;
    }
    mJsUuids = uuids;
    Root();
  } else if (name.EqualsLiteral("Services")) {
    mServices = value.get_ArrayOfnsString();
    nsresult rv;
    nsIScriptContext* sc = GetContextForEventHandlers(&rv);
    NS_ENSURE_SUCCESS_VOID(rv);
    NS_ENSURE_TRUE_VOID(sc);

    AutoPushJSContext cx(sc->GetNativeContext());

    JS::Rooted<JSObject*> services(cx);
    if (NS_FAILED(nsTArrayToJSArray(cx, mServices, services.address()))) {
      BT_WARNING("Cannot set JS Services object!");
      return;
    }
    mJsServices = services;
    Root();
  } else {
    nsCString warningMsg;
    warningMsg.AssignLiteral("Not handling device property: ");
    warningMsg.Append(NS_ConvertUTF16toUTF8(name));
    BT_WARNING(warningMsg.get());
  }
}

// static
already_AddRefed<BluetoothDevice>
BluetoothDevice::Create(nsPIDOMWindow* aWindow,
                        const nsAString& aAdapterPath,
                        const BluetoothValue& aValue)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  nsRefPtr<BluetoothDevice> device =
    new BluetoothDevice(aWindow, aAdapterPath, aValue);
  return device.forget();
}

void
BluetoothDevice::Notify(const BluetoothSignal& aData)
{
  BT_LOGD("[D] %s: %s", __FUNCTION__, NS_ConvertUTF16toUTF8(aData.name()).get());

  BluetoothValue v = aData.value();
  if (aData.name().EqualsLiteral("PropertyChanged")) {
    MOZ_ASSERT(v.type() == BluetoothValue::TArrayOfBluetoothNamedValue);

    const InfallibleTArray<BluetoothNamedValue>& arr =
      v.get_ArrayOfBluetoothNamedValue();

    for (uint32_t i = 0, propCount = arr.Length(); i < propCount; ++i) {
      SetPropertyByValue(arr[i]);
    }
  } else {
#ifdef DEBUG
    nsCString warningMsg;
    warningMsg.AssignLiteral("Not handling device signal: ");
    warningMsg.Append(NS_ConvertUTF16toUTF8(aData.name()));
    BT_WARNING(warningMsg.get());
#endif
  }
}

JS::Value
BluetoothDevice::GetUuids(JSContext* aCx, ErrorResult& aRv)
{
  if (!mJsUuids) {
    BT_WARNING("UUIDs not yet set!");
    aRv.Throw(NS_ERROR_FAILURE);
    return JS::NullValue();
  }

  JS::ExposeObjectToActiveJS(mJsUuids);
  return JS::ObjectValue(*mJsUuids);
}

JS::Value
BluetoothDevice::GetServices(JSContext* aCx, ErrorResult& aRv)
{
  if (!mJsServices) {
    BT_WARNING("Services not yet set!");
    aRv.Throw(NS_ERROR_FAILURE);
    return JS::Value(JSVAL_NULL);
  }

  JS::ExposeObjectToActiveJS(mJsServices);
  return JS::ObjectValue(*mJsServices);
}

JSObject*
BluetoothDevice::WrapObject(JSContext* aContext)
{
  return BluetoothDeviceBinding::Wrap(aContext, this);
}
