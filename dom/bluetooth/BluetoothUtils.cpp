/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothUtils.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "jsapi.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "nsContentUtils.h"
#include "nsISystemMessagesInternal.h"
#include "nsServiceManagerUtils.h"
#include "nsXULAppAPI.h"

BEGIN_BLUETOOTH_NAMESPACE

void
UuidToString(const BluetoothUuid& aUuid, nsAString& aString)
{
  char uuidStr[37];
  uint32_t uuid0, uuid4;
  uint16_t uuid1, uuid2, uuid3, uuid5;

  memcpy(&uuid0, &aUuid.mUuid[0], sizeof(uint32_t));
  memcpy(&uuid1, &aUuid.mUuid[4], sizeof(uint16_t));
  memcpy(&uuid2, &aUuid.mUuid[6], sizeof(uint16_t));
  memcpy(&uuid3, &aUuid.mUuid[8], sizeof(uint16_t));
  memcpy(&uuid4, &aUuid.mUuid[10], sizeof(uint32_t));
  memcpy(&uuid5, &aUuid.mUuid[14], sizeof(uint16_t));

  snprintf(uuidStr, sizeof(uuidStr),
           "%.8x-%.4x-%.4x-%.4x-%.8x%.4x",
           ntohl(uuid0), ntohs(uuid1),
           ntohs(uuid2), ntohs(uuid3),
           ntohl(uuid4), ntohs(uuid5));

  aString.Truncate();
  aString.AssignLiteral(uuidStr);
}

void
ReversedUuidToString(const BluetoothUuid& aUuid, nsAString& aString)
{
  BluetoothUuid uuid;
  for (uint8_t i = 0; i < 16; i++) {
    uuid.mUuid[i] = aUuid.mUuid[15 - i];
  }

  UuidToString(uuid, aString);
}

void
StringToUuid(const char* aString, BluetoothUuid& aUuid)
{
  uint32_t uuid0, uuid4;
  uint16_t uuid1, uuid2, uuid3, uuid5;

  sscanf(aString, "%08x-%04hx-%04hx-%04hx-%08x%04hx",
         &uuid0, &uuid1, &uuid2, &uuid3, &uuid4, &uuid5);

  uuid0 = htonl(uuid0);
  uuid1 = htons(uuid1);
  uuid2 = htons(uuid2);
  uuid3 = htons(uuid3);
  uuid4 = htonl(uuid4);
  uuid5 = htons(uuid5);

  memcpy(&aUuid.mUuid[0], &uuid0, sizeof(uint32_t));
  memcpy(&aUuid.mUuid[4], &uuid1, sizeof(uint16_t));
  memcpy(&aUuid.mUuid[6], &uuid2, sizeof(uint16_t));
  memcpy(&aUuid.mUuid[8], &uuid3, sizeof(uint16_t));
  memcpy(&aUuid.mUuid[10], &uuid4, sizeof(uint32_t));
  memcpy(&aUuid.mUuid[14], &uuid5, sizeof(uint16_t));
}

void
GeneratePathFromGattId(const BluetoothGattId& aId,
                       nsAString& aPath,
                       nsAString& aUuidStr)
{
  ReversedUuidToString(aId.mUuid, aUuidStr);

  aPath.Assign(aUuidStr);
  aPath.AppendLiteral("_");
  aPath.AppendInt(aId.mInstanceId);
}

void
GeneratePathFromGattId(const BluetoothGattId& aId,
                       nsAString& aPath)
{
  nsString uuidStr;
  GeneratePathFromGattId(aId, aPath, uuidStr);
}

/**
 * |SetJsObject| is an internal function used by |BroadcastSystemMessage| only
 */
static bool
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
  MOZ_ASSERT(!::JS_IsExceptionPending(cx),
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
    JS::Rooted<JSObject*> obj(cx, JS_NewPlainObject(cx));
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

bool
BroadcastSystemMessage(const nsAString& aType,
                       const InfallibleTArray<BluetoothNamedValue>& aData)
{
  mozilla::AutoSafeJSContext cx;
  MOZ_ASSERT(!::JS_IsExceptionPending(cx),
      "Shouldn't get here when an exception is pending!");

  JS::Rooted<JSObject*> obj(cx, JS_NewPlainObject(cx));
  if (!obj) {
    BT_WARNING("Failed to new JSObject for system message!");
    return false;
  }

  if (!SetJsObject(cx, aData, obj)) {
    BT_WARNING("Failed to set properties of system message!");
    return false;
  }

  nsCOMPtr<nsISystemMessagesInternal> systemMessenger =
    do_GetService("@mozilla.org/system-message-internal;1");
  NS_ENSURE_TRUE(systemMessenger, false);

  JS::Rooted<JS::Value> value(cx, JS::ObjectValue(*obj));
  systemMessenger->BroadcastMessage(aType, value,
                                    JS::UndefinedHandleValue);

  return true;
}

#ifdef MOZ_B2G_BT_API_V2
void
DispatchReplySuccess(BluetoothReplyRunnable* aRunnable)
{
  DispatchReplySuccess(aRunnable, BluetoothValue(true));
}

void
DispatchReplySuccess(BluetoothReplyRunnable* aRunnable,
                     const BluetoothValue& aValue)
{
  MOZ_ASSERT(aRunnable);
  MOZ_ASSERT(aValue.type() != BluetoothValue::T__None);

  BluetoothReply* reply = new BluetoothReply(BluetoothReplySuccess(aValue));

  aRunnable->SetReply(reply); // runnable will delete reply after Run()
  NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(aRunnable)));
}

void
DispatchReplyError(BluetoothReplyRunnable* aRunnable,
                   const nsAString& aErrorStr)
{
  MOZ_ASSERT(aRunnable);
  MOZ_ASSERT(!aErrorStr.IsEmpty());

  BluetoothReply* reply =
    new BluetoothReply(BluetoothReplyError(STATUS_FAIL, nsString(aErrorStr)));

  aRunnable->SetReply(reply); // runnable will delete reply after Run()
  NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(aRunnable)));
}

void
DispatchReplyError(BluetoothReplyRunnable* aRunnable,
                   const enum BluetoothStatus aStatus)
{
  MOZ_ASSERT(aRunnable);
  MOZ_ASSERT(aStatus != STATUS_SUCCESS);

  BluetoothReply* reply =
    new BluetoothReply(BluetoothReplyError(aStatus, EmptyString()));

  aRunnable->SetReply(reply); // runnable will delete reply after Run()
  NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(aRunnable)));
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

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);
  bs->DistributeSignal(aType, NS_LITERAL_STRING(KEY_ADAPTER), data);
}
#else
// TODO: remove with bluetooth1
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

// TODO: remove with bluetooth1
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
#endif

bool
IsMainProcess()
{
  return XRE_GetProcessType() == GeckoProcessType_Default;
}

END_BLUETOOTH_NAMESPACE
