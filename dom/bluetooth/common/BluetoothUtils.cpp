/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothUtils.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "jsapi.h"
#include "mozilla/dom/BluetoothGattCharacteristicBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "nsContentUtils.h"
#include "nsISystemMessagesInternal.h"
#include "nsIUUIDGenerator.h"
#include "nsServiceManagerUtils.h"
#include "nsXULAppAPI.h"

BEGIN_BLUETOOTH_NAMESPACE

void
AddressToString(const BluetoothAddress& aAddress, nsAString& aString)
{
  char str[BLUETOOTH_ADDRESS_LENGTH + 1];

  int res = snprintf(str, sizeof(str), "%02x:%02x:%02x:%02x:%02x:%02x",
                     static_cast<int>(aAddress.mAddr[0]),
                     static_cast<int>(aAddress.mAddr[1]),
                     static_cast<int>(aAddress.mAddr[2]),
                     static_cast<int>(aAddress.mAddr[3]),
                     static_cast<int>(aAddress.mAddr[4]),
                     static_cast<int>(aAddress.mAddr[5]));

  if ((res == EOF) ||
      (res < 0) ||
      (static_cast<size_t>(res) >= sizeof(str))) {
    /* Conversion should have succeeded or (a) we're out of memory, or
     * (b) our code is massively broken. We should crash in both cases.
     */
    MOZ_CRASH("Failed to convert Bluetooth address to string");
  }

  aString = NS_ConvertUTF8toUTF16(str);
}

nsresult
StringToAddress(const nsAString& aString, BluetoothAddress& aAddress)
{
  int res = sscanf(NS_ConvertUTF16toUTF8(aString).get(),
                   "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
                   &aAddress.mAddr[0],
                   &aAddress.mAddr[1],
                   &aAddress.mAddr[2],
                   &aAddress.mAddr[3],
                   &aAddress.mAddr[4],
                   &aAddress.mAddr[5]);
  if (res < static_cast<ssize_t>(MOZ_ARRAY_LENGTH(aAddress.mAddr))) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  return NS_OK;
}

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
StringToUuid(const nsAString& aString, BluetoothUuid& aUuid)
{
  uint32_t uuid0, uuid4;
  uint16_t uuid1, uuid2, uuid3, uuid5;

  sscanf(NS_ConvertUTF16toUTF8(aString).get(),
         "%08x-%04hx-%04hx-%04hx-%08x%04hx",
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

nsresult
GenerateUuid(nsAString &aUuidString)
{
  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> uuidGenerator =
    do_GetService("@mozilla.org/uuid-generator;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsID uuid;
  rv = uuidGenerator->GenerateUUIDInPlace(&uuid);
  NS_ENSURE_SUCCESS(rv, rv);

  // Build a string in {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx} format
  char uuidBuffer[NSID_LENGTH];
  uuid.ToProvidedString(uuidBuffer);
  NS_ConvertASCIItoUTF16 uuidString(uuidBuffer);

  // Remove {} and the null terminator
  aUuidString.Assign(Substring(uuidString, 1, NSID_LENGTH - 3));

  return NS_OK;
}

void
GattPermissionsToDictionary(BluetoothGattAttrPerm aBits,
                            GattPermissions& aPermissions)
{
  aPermissions.mRead = aBits & GATT_ATTR_PERM_BIT_READ;
  aPermissions.mReadEncrypted = aBits & GATT_ATTR_PERM_BIT_READ_ENCRYPTED;
  aPermissions.mReadEncryptedMITM =
    aBits & GATT_ATTR_PERM_BIT_READ_ENCRYPTED_MITM;
  aPermissions.mWrite = aBits & GATT_ATTR_PERM_BIT_WRITE;
  aPermissions.mWriteEncrypted = aBits & GATT_ATTR_PERM_BIT_WRITE_ENCRYPTED;
  aPermissions.mWriteEncryptedMITM =
    aBits & GATT_ATTR_PERM_BIT_WRITE_ENCRYPTED_MITM;
  aPermissions.mWriteSigned = aBits & GATT_ATTR_PERM_BIT_WRITE_SIGNED;
  aPermissions.mWriteSignedMITM = aBits & GATT_ATTR_PERM_BIT_WRITE_SIGNED_MITM;
}

void
GattPermissionsToBits(const GattPermissions& aPermissions,
                      BluetoothGattAttrPerm& aBits)
{
  aBits = BLUETOOTH_EMPTY_GATT_ATTR_PERM;

  if (aPermissions.mRead) {
    aBits |= GATT_ATTR_PERM_BIT_READ;
  }
  if (aPermissions.mReadEncrypted) {
    aBits |= GATT_ATTR_PERM_BIT_READ_ENCRYPTED;
  }
  if (aPermissions.mReadEncryptedMITM) {
    aBits |= GATT_ATTR_PERM_BIT_READ_ENCRYPTED_MITM;
  }
  if (aPermissions.mWrite) {
    aBits |= GATT_ATTR_PERM_BIT_WRITE;
  }
  if (aPermissions.mWriteEncrypted) {
    aBits |= GATT_ATTR_PERM_BIT_WRITE_ENCRYPTED;
  }
  if (aPermissions.mWriteEncryptedMITM) {
    aBits |= GATT_ATTR_PERM_BIT_WRITE_ENCRYPTED_MITM;
  }
  if (aPermissions.mWriteSigned) {
    aBits |= GATT_ATTR_PERM_BIT_WRITE_SIGNED;
  }
  if (aPermissions.mWriteSignedMITM) {
    aBits |= GATT_ATTR_PERM_BIT_WRITE_SIGNED_MITM;
  }
}

void
GattPropertiesToDictionary(BluetoothGattCharProp aBits,
                           GattCharacteristicProperties& aProperties)
{
  aProperties.mBroadcast = aBits & GATT_CHAR_PROP_BIT_BROADCAST;
  aProperties.mRead = aBits & GATT_CHAR_PROP_BIT_READ;
  aProperties.mWriteNoResponse = aBits & GATT_CHAR_PROP_BIT_WRITE_NO_RESPONSE;
  aProperties.mWrite = aBits & GATT_CHAR_PROP_BIT_WRITE;
  aProperties.mNotify = aBits & GATT_CHAR_PROP_BIT_NOTIFY;
  aProperties.mIndicate = aBits & GATT_CHAR_PROP_BIT_INDICATE;
  aProperties.mSignedWrite = aBits & GATT_CHAR_PROP_BIT_SIGNED_WRITE;
  aProperties.mExtendedProps = aBits & GATT_CHAR_PROP_BIT_EXTENDED_PROPERTIES;
}

void
GattPropertiesToBits(const GattCharacteristicProperties& aProperties,
                     BluetoothGattCharProp& aBits)
{
  aBits = BLUETOOTH_EMPTY_GATT_CHAR_PROP;

  if (aProperties.mBroadcast) {
    aBits |= GATT_CHAR_PROP_BIT_BROADCAST;
  }
  if (aProperties.mRead) {
    aBits |= GATT_CHAR_PROP_BIT_READ;
  }
  if (aProperties.mWriteNoResponse) {
    aBits |= GATT_CHAR_PROP_BIT_WRITE_NO_RESPONSE;
  }
  if (aProperties.mWrite) {
    aBits |= GATT_CHAR_PROP_BIT_WRITE;
  }
  if (aProperties.mNotify) {
    aBits |= GATT_CHAR_PROP_BIT_NOTIFY;
  }
  if (aProperties.mIndicate) {
    aBits |= GATT_CHAR_PROP_BIT_INDICATE;
  }
  if (aProperties.mSignedWrite) {
    aBits |= GATT_CHAR_PROP_BIT_SIGNED_WRITE;
  }
  if (aProperties.mExtendedProps) {
    aBits |= GATT_CHAR_PROP_BIT_EXTENDED_PROPERTIES;
  }
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

  nsCOMPtr<nsISupports> promise;
  systemMessenger->BroadcastMessage(aType, value,
                                    JS::UndefinedHandleValue,
                                    getter_AddRefs(promise));

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
  nsCOMPtr<nsISupports> promise;
  systemMessenger->BroadcastMessage(aType, value,
                                    JS::UndefinedHandleValue,
                                    getter_AddRefs(promise));

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
  BluetoothReply* reply =
    new BluetoothReply(BluetoothReplyError(STATUS_FAIL, nsString(aErrorStr)));

  aRunnable->SetReply(reply);
  NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(aRunnable)));
}

void
DispatchReplyError(BluetoothReplyRunnable* aRunnable,
                   const enum BluetoothStatus aStatus)
{
  MOZ_ASSERT(aRunnable);
  MOZ_ASSERT(aStatus != STATUS_SUCCESS);

  // Reply will be deleted by the runnable after running on main thread
  BluetoothReply* reply =
    new BluetoothReply(BluetoothReplyError(aStatus, EmptyString()));

  aRunnable->SetReply(reply);
  NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(aRunnable)));
}

void
DispatchStatusChangedEvent(const nsAString& aType,
                           const nsAString& aAddress,
                           bool aStatus)
{
  MOZ_ASSERT(NS_IsMainThread());

  InfallibleTArray<BluetoothNamedValue> data;
  AppendNamedValue(data, "address", nsString(aAddress));
  AppendNamedValue(data, "status", aStatus);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  bs->DistributeSignal(aType, NS_LITERAL_STRING(KEY_ADAPTER), data);
}

void
AppendNamedValue(InfallibleTArray<BluetoothNamedValue>& aArray,
                 const char* aName, const BluetoothValue& aValue)
{
  nsString name;
  name.AssignASCII(aName);

  aArray.AppendElement(BluetoothNamedValue(name, aValue));
}

void
InsertNamedValue(InfallibleTArray<BluetoothNamedValue>& aArray,
                 uint8_t aIndex, const char* aName,
                 const BluetoothValue& aValue)
{
  nsString name;
  name.AssignASCII(aName);

  aArray.InsertElementAt(aIndex, BluetoothNamedValue(name, aValue));
}

END_BLUETOOTH_NAMESPACE
