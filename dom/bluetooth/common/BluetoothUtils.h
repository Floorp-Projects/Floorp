/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothUtils_h
#define mozilla_dom_bluetooth_BluetoothUtils_h

#include "BluetoothCommon.h"
#include "js/TypeDecls.h"

namespace mozilla {
namespace dom {
class GattPermissions;
class GattCharacteristicProperties;
}
}

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothNamedValue;
class BluetoothReplyRunnable;
class BluetoothValue;

//
// Address/String conversion
//

void
AddressToString(const BluetoothAddress& aAddress, nsAString& aString);

nsresult
StringToAddress(const nsAString& aString, BluetoothAddress& aAddress);

//
// Pin code/string conversion
//

nsresult
PinCodeToString(const BluetoothPinCode& aPinCode, nsAString& aString);

nsresult
StringToPinCode(const nsAString& aString, BluetoothPinCode& aPinCode);

//
// Play status/string conversion
//

nsresult
StringToControlPlayStatus(const nsAString& aString,
                          ControlPlayStatus& aPlayStatus);

//
// Property type/string conversion
//

nsresult
StringToPropertyType(const nsAString& aString, BluetoothPropertyType& aType);

//
// Property conversion
//

nsresult
NamedValueToProperty(const BluetoothNamedValue& aIn,
                     BluetoothProperty& aProperty);

//
// Remote name/string conversion
//

void
RemoteNameToString(const BluetoothRemoteName& aRemoteName, nsAString& aString);

//
// Service name/string conversion
//

nsresult
StringToServiceName(const nsAString& aString,
                    BluetoothServiceName& aServiceName);

//
// BluetoothUuid <-> uuid string conversion
//

/**
 * Convert BluetoothUuid object to xxxxxxxx-xxxx-xxxx-xxxxxxxxxxxx uuid string.
 *
 * Note: This utility function is used by gecko internal only to convert
 * BluetoothUuid created by bluetooth stack to uuid string representation.
 */
void
UuidToString(const BluetoothUuid& aUuid, nsAString& aString);

/**
 * Convert xxxxxxxx-xxxx-xxxx-xxxxxxxxxxxx uuid string to BluetoothUuid object.
 *
 * Note: This utility function is used by gecko internal only to convert uuid
 * string created by gecko back to BluetoothUuid representation.
 */
nsresult
StringToUuid(const nsAString& aString, BluetoothUuid& aUuid);

/**
 * Generate a random uuid.
 *
 * @param aUuid [out] The generated uuid.
 */
nsresult
GenerateUuid(BluetoothUuid &aUuid);

/**
 * Generate a random uuid.
 *
 * @param aUuidString [out] String to store the generated uuid.
 */
nsresult
GenerateUuid(nsAString &aUuidString);

/**
 * Convert BluetoothGattAttrPerm bit masks to GattPermissions object.
 *
 * @param aBits [in] BluetoothGattAttrPerm bit masks.
 * @param aPermissions [out] GattPermissions object.
 */
void
GattPermissionsToDictionary(BluetoothGattAttrPerm aBits,
                            GattPermissions& aPermissions);

/**
 * Convert GattPermissions object to BluetoothGattAttrPerm bit masks.
 *
 * @param aPermissions [in] GattPermissions object.
 * @param aBits [out] BluetoothGattAttrPerm bit masks.
 */
void
GattPermissionsToBits(const GattPermissions& aPermissions,
                      BluetoothGattAttrPerm& aBits);

/**
 * Convert BluetoothGattCharProp bit masks to GattCharacteristicProperties
 * object.
 *
 * @param aBits [in] BluetoothGattCharProp bit masks.
 * @param aProperties [out] GattCharacteristicProperties object.
 */
void
GattPropertiesToDictionary(BluetoothGattCharProp aBits,
                           GattCharacteristicProperties& aProperties);

/**
 * Convert GattCharacteristicProperties object to BluetoothGattCharProp bit
 * masks.
 *
 * @param aProperties [in] GattCharacteristicProperties object.
 * @param aBits [out] BluetoothGattCharProp bit masks.
 */
void
GattPropertiesToBits(const GattCharacteristicProperties& aProperties,
                     BluetoothGattCharProp& aBits);

//
// Generate bluetooth signal path from GattId
//

/**
 * Generate bluetooth signal path from a GattId.
 *
 * @param aId   [in] GattId value to convert.
 * @param aPath [out] Bluetooth signal path generated from aId.
 */
void
GeneratePathFromGattId(const BluetoothGattId& aId,
                       nsAString& aPath);

//
// Register/Unregister bluetooth signal handlers
//

/**
 * Register the Bluetooth signal handler.
 *
 * @param aPath Path of the signal to be registered.
 * @param aHandler The message handler object to be added into the observer
 *                 list. Note that this function doesn't take references to it.
 */
void
RegisterBluetoothSignalHandler(const nsAString& aPath,
                               BluetoothSignalObserver* aHandler);

/**
 * Register the Bluetooth signal handler.
 *
 * @param aAddress Address of the signal to be unregistered.
 * @param aHandler The message handler object to be added into the observer
 *                 list. Note that this function doesn't take references to it.
 */
void
RegisterBluetoothSignalHandler(const BluetoothAddress& aAddress,
                               BluetoothSignalObserver* aHandler);

/**
 * Register the Bluetooth signal handler.
 *
 * @param aUuid UUID of the signal to be unregistered.
 * @param aHandler The message handler object to be added into the observer
 *                 list. Note that this function doesn't take references to it.
 */
void
RegisterBluetoothSignalHandler(const BluetoothUuid& aUuid,
                               BluetoothSignalObserver* aHandler);

/**
 * Unregister the Bluetooth signal handler.
 *
 * @param aPath Path of the signal to be unregistered.
 * @param aHandler The message handler object to be removed from the observer
 *                 list. Note that this function doesn't take references to it.
 */
void
UnregisterBluetoothSignalHandler(const nsAString& aPath,
                                 BluetoothSignalObserver* aHandler);

/**
 * Unregister the Bluetooth signal handler.
 *
 * @param aAddress Address of the signal to be unregistered.
 * @param aHandler The message handler object to be removed from the observer
 *                 list. Note that this function doesn't take references to it.
 */
void
UnregisterBluetoothSignalHandler(const BluetoothAddress& aAddress,
                                 BluetoothSignalObserver* aHandler);

/**
 * Unregister the Bluetooth signal handler.
 *
 * @param aUuid UUID of the signal to be unregistered.
 * @param aHandler The message handler object to be removed from the observer
 *                 list. Note that this function doesn't take references to it.
 */
void
UnregisterBluetoothSignalHandler(const BluetoothUuid& aUuid,
                                 BluetoothSignalObserver* aHandler);

//
// Broadcast system message
//

bool
BroadcastSystemMessage(const nsAString& aType,
                       const BluetoothValue& aData);

bool
BroadcastSystemMessage(const nsAString& aType,
                       const InfallibleTArray<BluetoothNamedValue>& aData);

//
// Dispatch bluetooth reply to main thread
//

/**
 * Dispatch successful bluetooth reply with NO value to reply request.
 *
 * @param aRunnable  the runnable to reply bluetooth request.
 */
void
DispatchReplySuccess(BluetoothReplyRunnable* aRunnable);

/**
 * Dispatch successful bluetooth reply with value to reply request.
 *
 * @param aRunnable  the runnable to reply bluetooth request.
 * @param aValue     the BluetoothValue to reply successful request.
 */
void
DispatchReplySuccess(BluetoothReplyRunnable* aRunnable,
                     const BluetoothValue& aValue);

/**
 * Dispatch failed bluetooth reply with error string.
 *
 * This function is for methods returning DOMRequest. If |aErrorStr| is not
 * empty, the DOMRequest property 'error.name' would be updated to |aErrorStr|
 * before callback function 'onerror' is fired.
 *
 * NOTE: For methods returning Promise, |aErrorStr| would be ignored and only
 * STATUS_FAIL is returned in BluetoothReplyRunnable.
 *
 * @param aRunnable  the runnable to reply bluetooth request.
 * @param aErrorStr  the error string to reply failed request.
 */
void
DispatchReplyError(BluetoothReplyRunnable* aRunnable,
                   const nsAString& aErrorStr);

/**
 * Dispatch failed bluetooth reply with error status.
 *
 * This function is for methods returning Promise. The Promise would reject
 * with an Exception object that carries nsError associated with |aStatus|.
 * The name and messege of Exception (defined in dom/base/domerr.msg) are
 * filled automatically during promise rejection.
 *
 * @param aRunnable  the runnable to reply bluetooth request.
 * @param aStatus    the error status to reply failed request.
 */
void
DispatchReplyError(BluetoothReplyRunnable* aRunnable,
                   const enum BluetoothStatus aStatus);

void
DispatchStatusChangedEvent(const nsAString& aType,
                           const nsAString& aDeviceAddress,
                           bool aStatus);

//
// BluetoothNamedValue manipulation
//

/**
 * Wrap literal name and value into a BluetoothNamedValue and
 * append it to the array.
 */
void AppendNamedValue(InfallibleTArray<BluetoothNamedValue>& aArray,
                      const char* aName, const BluetoothValue& aValue);

/**
 * Wrap literal name and value into a BluetoothNamedValue and
 * insert it to the array.
 */
void InsertNamedValue(InfallibleTArray<BluetoothNamedValue>& aArray,
                      uint8_t aIndex, const char* aName,
                      const BluetoothValue& aValue);

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_BluetoothUtils_h
