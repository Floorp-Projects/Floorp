/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothUtils_h
#define mozilla_dom_bluetooth_BluetoothUtils_h

#include "BluetoothCommon.h"
#include "js/TypeDecls.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothNamedValue;
class BluetoothReplyRunnable;
class BluetoothValue;

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
void
StringToUuid(const char* aString, BluetoothUuid& aUuid);

/**
 * Generate a random uuid.
 *
 * @param aUuidString [out] String to store the generated uuid.
 */
void
GenerateUuid(nsAString &aUuidString);

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
 * Register the bluetooth signal handler.
 *
 * @param aPath Path of the signal to be registered.
 * @param aHandler The message handler object to be added into the observer
 *                 list. Note that this function doesn't take references to it.
 */
void
RegisterBluetoothSignalHandler(const nsAString& aPath,
                               BluetoothSignalObserver* aHandler);

/**
 * Unregister the bluetooth signal handler.
 *
 * @param aPath Path of the signal to be unregistered.
 * @param aHandler The message handler object to be removed from the observer
 *                 list. Note that this function doesn't take references to it.
 */
void
UnregisterBluetoothSignalHandler(const nsAString& aPath,
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

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_BluetoothUtils_h
