/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothutils_h__
#define mozilla_dom_bluetooth_bluetoothutils_h__

#include <hardware/bluetooth.h>

#include "BluetoothCommon.h"
#include "js/TypeDecls.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothNamedValue;
class BluetoothValue;
class BluetoothReplyRunnable;

void
StringToBdAddressType(const nsAString& aBdAddress,
                      bt_bdaddr_t *aRetBdAddressType);

void
BdAddressTypeToString(bt_bdaddr_t* aBdAddressType,
                      nsAString& aRetBdAddress);

void
UuidToString(const BluetoothUuid& aUuid, nsAString& aString);

bool
SetJsObject(JSContext* aContext,
            const BluetoothValue& aValue,
            JS::Handle<JSObject*> aObj);

bool
BroadcastSystemMessage(const nsAString& aType,
                       const BluetoothValue& aData);

/**
 * Dispatch Bluetooth reply to main thread. The reply will contain an error
 * string if the request fails.
 *
 * This function is mainly designed for DOMRequest-based methods which return
 * 'DOMRequest'. If aErrorStr is not empty, the DOMRequest property 'error.name'
 * would be updated to aErrorStr right before the callback function 'onerror'
 * is fired.
 *
 * @param aRunnable  the runnable to reply the bluetooth request.
 * @param aValue     the Bluetooth value which is used to reply a Bluetooth
 *                   request when the request finished successfully.
 * @param aErrorStr  the error string which is used to reply a Bluetooth
 *                   request when the request failed.
 */
void
DispatchBluetoothReply(BluetoothReplyRunnable* aRunnable,
                       const BluetoothValue& aValue,
                       const nsAString& aErrorStr);

/**
 * Dispatch Bluetooth reply to main thread. The reply will contain an error
 * status if the request fails.
 *
 * This function mainly designed for Bluetooth APIs which return 'Promise'.
 * If aStatusCode is not STATUS_SUCCESS, the Promise would reject with an
 * Exception object.
 * The ns error used by Exception will be associated with aStatusCode.
 * The name and messege of Exception are defined in dom/base/domerr.msg and
 * will be filled automatically during promise rejection.
 *
 * @param aRunnable   the runnable to reply the bluetooth request.
 * @param aValue      the Bluetooth value which is used to reply a Bluetooth
 *                    request if the request finished successfully.
 * @param aStatusCode the error status which is used to reply a Bluetooth
 *                    request when the request failed.
 */

void
DispatchBluetoothReply(BluetoothReplyRunnable* aRunnable,
                       const BluetoothValue& aValue,
                       const enum BluetoothStatus aStatusCode);

void
DispatchStatusChangedEvent(const nsAString& aType,
                           const nsAString& aDeviceAddress,
                           bool aStatus);


END_BLUETOOTH_NAMESPACE

#endif
