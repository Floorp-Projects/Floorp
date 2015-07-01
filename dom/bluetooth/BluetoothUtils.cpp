/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "nsIUUIDGenerator.h"
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
GenerateUuid(nsAString &aUuidString)
{
  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> uuidGenerator =
    do_GetService("@mozilla.org/uuid-generator;1", &rv);
  NS_ENSURE_SUCCESS_VOID(rv);

  nsID uuid;
  rv = uuidGenerator->GenerateUUIDInPlace(&uuid);
  NS_ENSURE_SUCCESS_VOID(rv);

  // Build a string in {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx} format
  char uuidBuffer[NSID_LENGTH];
  uuid.ToProvidedString(uuidBuffer);
  NS_ConvertASCIItoUTF16 uuidString(uuidBuffer);

  // Remove {} and the null terminator
  aUuidString.Assign(Substring(uuidString, 1, NSID_LENGTH - 3));
}

void
GeneratePathFromGattId(const BluetoothGattId& aId,
                       nsAString& aPath)
{
  nsString uuidStr;
  UuidToString(aId.mUuid, uuidStr);

  aPath.Assign(uuidStr);
  aPath.AppendLiteral("_");
  aPath.AppendInt(aId.mInstanceId);
}

void
RegisterBluetoothSignalHandler(const nsAString& aPath,
                               BluetoothSignalObserver* aHandler)
{
  MOZ_ASSERT(!aPath.IsEmpty());
  MOZ_ASSERT(aHandler);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  bs->RegisterBluetoothSignalHandler(aPath, aHandler);
  aHandler->SetSignalRegistered(true);
}

void
UnregisterBluetoothSignalHandler(const nsAString& aPath,
                                 BluetoothSignalObserver* aHandler)
{
  MOZ_ASSERT(!aPath.IsEmpty());
  MOZ_ASSERT(aHandler);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  bs->UnregisterBluetoothSignalHandler(aPath, aHandler);
  aHandler->SetSignalRegistered(false);
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
        val.setString(jsData);
        break;
      }
      case BluetoothValue::Tuint32_t:
        val.setInt32(v.get_uint32_t());
        break;
      case BluetoothValue::Tbool:
        val.setBoolean(v.get_bool());
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
    value.setString(jsData);
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

  // Reply will be deleted by the runnable after running on main thread
#ifndef MOZ_B2G_BT_API_V1
  BluetoothReply* reply =
    new BluetoothReply(BluetoothReplyError(STATUS_FAIL, nsString(aErrorStr)));
#else
  BluetoothReply* reply =
    new BluetoothReply(BluetoothReplyError(nsString(aErrorStr)));
#endif

  aRunnable->SetReply(reply); // runnable will delete reply after Run()
  NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(aRunnable)));
}

void
DispatchReplyError(BluetoothReplyRunnable* aRunnable,
                   const enum BluetoothStatus aStatus)
{
  MOZ_ASSERT(aRunnable);
  MOZ_ASSERT(aStatus != STATUS_SUCCESS);

  // Reply will be deleted by the runnable after running on main thread
#ifndef MOZ_B2G_BT_API_V1
  BluetoothReply* reply =
    new BluetoothReply(BluetoothReplyError(aStatus, EmptyString()));
#else
  BluetoothReply* reply =
    new BluetoothReply(
      BluetoothReplyError(NS_LITERAL_STRING("Internal error")));
#endif

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

#ifndef MOZ_B2G_BT_API_V1
  bs->DistributeSignal(aType, NS_LITERAL_STRING(KEY_ADAPTER), data);
#else
  BluetoothSignal signal(nsString(aType), NS_LITERAL_STRING(KEY_ADAPTER), data);
  bs->DistributeSignal(signal);
#endif
}

bool
IsMainProcess()
{
  return XRE_GetProcessType() == GeckoProcessType_Default;
}

END_BLUETOOTH_NAMESPACE
