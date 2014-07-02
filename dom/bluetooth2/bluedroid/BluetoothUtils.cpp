/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothServiceBluedroid.h"
#include "BluetoothUtils.h"
#include "jsapi.h"
#include "mozilla/Scoped.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsIScriptContext.h"
#include "nsISystemMessagesInternal.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsServiceManagerUtils.h"

BEGIN_BLUETOOTH_NAMESPACE

void
StringToBdAddressType(const nsAString& aBdAddress,
                      bt_bdaddr_t *aRetBdAddressType)
{
  NS_ConvertUTF16toUTF8 bdAddressUTF8(aBdAddress);
  const char* str = bdAddressUTF8.get();

  for (int i = 0; i < 6; i++) {
    aRetBdAddressType->address[i] = (uint8_t) strtoul(str, (char **)&str, 16);
    str++;
  }
}

void
BdAddressTypeToString(bt_bdaddr_t* aBdAddressType, nsAString& aRetBdAddress)
{
  uint8_t* addr = aBdAddressType->address;
  char bdstr[18];

  sprintf(bdstr, "%02x:%02x:%02x:%02x:%02x:%02x",
          (int)addr[0],(int)addr[1],(int)addr[2],
          (int)addr[3],(int)addr[4],(int)addr[5]);

  aRetBdAddress = NS_ConvertUTF8toUTF16(bdstr);
}

bool
SetJsObject(JSContext* aContext,
            const BluetoothValue& aValue,
            JS::Handle<JSObject*> aObj)
{
  MOZ_ASSERT(aContext && aObj);

  if (aValue.type() != BluetoothValue::TArrayOfBluetoothNamedValue) {
    BT_WARNING("SetJsObject: Invalid parameter type");
    return false;
  }

  const nsTArray<BluetoothNamedValue>& arr =
    aValue.get_ArrayOfBluetoothNamedValue();

  for (uint32_t i = 0; i < arr.Length(); i++) {
    JS::Rooted<JS::Value> val(aContext);
    const BluetoothValue& v = arr[i].value();

    switch(v.type()) {
       case BluetoothValue::TnsString: {
        JSString* jsData = JS_NewUCStringCopyN(aContext,
                                     v.get_nsString().BeginReading(),
                                     v.get_nsString().Length());
        NS_ENSURE_TRUE(jsData, false);
        val = STRING_TO_JSVAL(jsData);
        break;
      }
      case BluetoothValue::Tuint32_t:
        val = INT_TO_JSVAL(v.get_uint32_t());
        break;
      case BluetoothValue::Tbool:
        val = BOOLEAN_TO_JSVAL(v.get_bool());
        break;
      default:
        BT_WARNING("SetJsObject: Parameter is not handled");
        break;
    }

    if (!JS_SetProperty(aContext, aObj,
                        NS_ConvertUTF16toUTF8(arr[i].name()).get(),
                        val)) {
      BT_WARNING("Failed to set property");
      return false;
    }
  }

  return true;
}

bool
BroadcastSystemMessage(const nsAString& aType,
                       const BluetoothValue& aData)
{
  mozilla::AutoSafeJSContext cx;
  NS_ASSERTION(!::JS_IsExceptionPending(cx),
      "Shouldn't get here when an exception is pending!");

  nsCOMPtr<nsISystemMessagesInternal> systemMessenger =
    do_GetService("@mozilla.org/system-message-internal;1");
  NS_ENSURE_TRUE(systemMessenger, false);

  JS::Rooted<JS::Value> value(cx);
  if (aData.type() == BluetoothValue::TnsString) {
    JSString* jsData = JS_NewUCStringCopyN(cx,
                                           aData.get_nsString().BeginReading(),
                                           aData.get_nsString().Length());
    value = STRING_TO_JSVAL(jsData);
  } else if (aData.type() == BluetoothValue::TArrayOfBluetoothNamedValue) {
    JS::Rooted<JSObject*> obj(cx, JS_NewObject(cx, nullptr, JS::NullPtr(),
                                               JS::NullPtr()));
    if (!obj) {
      BT_WARNING("Failed to new JSObject for system message!");
      return false;
    }

    if (!SetJsObject(cx, aData, obj)) {
      BT_WARNING("Failed to set properties of system message!");
      return false;
    }
    value = JS::ObjectValue(*obj);
  } else {
    BT_WARNING("Not support the unknown BluetoothValue type");
    return false;
  }

  systemMessenger->BroadcastMessage(aType, value,
                                    JS::UndefinedHandleValue);

  return true;
}

void
DispatchBluetoothReply(BluetoothReplyRunnable* aRunnable,
                       const BluetoothValue& aValue,
                       const nsAString& aErrorStr)
{
  // Reply will be deleted by the runnable after running on main thread
  BluetoothReply* reply;
  if (!aErrorStr.IsEmpty()) {
    nsString err(aErrorStr);
    reply = new BluetoothReply(BluetoothReplyError(err));
  } else {
    MOZ_ASSERT(aValue.type() != BluetoothValue::T__None);
    reply = new BluetoothReply(BluetoothReplySuccess(aValue));
  }

  aRunnable->SetReply(reply);
  if (NS_FAILED(NS_DispatchToMainThread(aRunnable))) {
    BT_WARNING("Failed to dispatch to main thread!");
  }
}

void
DispatchStatusChangedEvent(const nsAString& aType,
                           const nsAString& aAddress,
                           bool aStatus)
{
  MOZ_ASSERT(NS_IsMainThread());

  InfallibleTArray<BluetoothNamedValue> data;
  BT_APPEND_NAMED_VALUE(data, "address", nsString(aAddress));
  BT_APPEND_NAMED_VALUE(data, "status", aStatus);

  BluetoothSignal signal(nsString(aType), NS_LITERAL_STRING(KEY_ADAPTER), data);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);
  bs->DistributeSignal(signal);
}

END_BLUETOOTH_NAMESPACE
