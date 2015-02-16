/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothutils_h
#define mozilla_dom_bluetooth_bluetoothutils_h

#include "BluetoothCommon.h"
#include "js/TypeDecls.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothNamedValue;
class BluetoothValue;
class BluetoothReplyRunnable;

void
UuidToString(const BluetoothUuid& aUuid, nsAString& aString);

/**
 * Convert xxxxxxxx-xxxx-xxxx-xxxxxxxxxxxx uuid string to BluetoothUuid object.
 * This utility function is used by gecko internal only to convert uuid string
 * created by gecko back to BluetoothUuid representation.
 */
void
StringToUuid(const char* aString, BluetoothUuid& aUuid);

bool
SetJsObject(JSContext* aContext,
            const BluetoothValue& aValue,
            JS::Handle<JSObject*> aObj);

bool
BroadcastSystemMessage(const nsAString& aType,
                       const BluetoothValue& aData);

bool
BroadcastSystemMessage(const nsAString& aType,
                       const InfallibleTArray<BluetoothNamedValue>& aData);

/**
 * Dispatch BluetoothReply to main thread. The reply contains an error string
 * if the request fails.
 *
 * This function is for methods returning DOMRequest. If |aErrorStr| is not
 * empty, the DOMRequest property 'error.name' would be updated to |aErrorStr|
 * before callback function 'onerror' is fired.
 *
 * @param aRunnable  the runnable to reply bluetooth request.
 * @param aValue     the BluetoothValue used to reply successful request.
 * @param aErrorStr  the error string used to reply failed request.
 */
void
DispatchBluetoothReply(BluetoothReplyRunnable* aRunnable,
                       const BluetoothValue& aValue,
                       const nsAString& aErrorStr);

/**
 * Dispatch BluetoothReply to main thread. The reply contains an error string
 * if the request fails.
 *
 * This function is for methods returning Promise. If |aStatusCode| is not
 * STATUS_SUCCESS, the Promise would reject with an Exception object with
 * nsError associated with |aStatusCode|. The name and messege of Exception
 * (defined in dom/base/domerr.msg) are filled automatically during promise
 * rejection.
 *
 * @param aRunnable   the runnable to reply bluetooth request.
 * @param aValue      the BluetoothValue to reply successful request.
 * @param aStatusCode the error status to reply failed request.
 */
void
DispatchBluetoothReply(BluetoothReplyRunnable* aRunnable,
                       const BluetoothValue& aValue,
                       const enum BluetoothStatus aStatusCode);

void
DispatchStatusChangedEvent(const nsAString& aType,
                           const nsAString& aDeviceAddress,
                           bool aStatus);

/**
 * Check whether the caller runs at B2G process.
 *
 * @return true if the caller runs at B2G process, false otherwise.
 */
bool
IsMainProcess();

END_BLUETOOTH_NAMESPACE

#endif
