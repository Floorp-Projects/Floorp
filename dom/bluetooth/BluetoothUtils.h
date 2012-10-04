/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothutils_h__
#define mozilla_dom_bluetooth_bluetoothutils_h__

#include "BluetoothCommon.h"

class JSContext;
class JSObject;

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothDevice;
class BluetoothNamedValue;

nsresult
StringArrayToJSArray(JSContext* aCx, JSObject* aGlobal,
                     const nsTArray<nsString>& aSourceArray,
                     JSObject** aResultArray);

nsresult
BluetoothDeviceArrayToJSArray(JSContext* aCx, JSObject* aGlobal,
                              const nsTArray<nsRefPtr<BluetoothDevice> >& aSourceArray,
                              JSObject** aResultArray);

bool
SetJsObject(JSContext* aContext,
            JSObject* aObj,
            const InfallibleTArray<BluetoothNamedValue>& aData);

nsString
GetObjectPathFromAddress(const nsAString& aAdapterPath,
                         const nsAString& aDeviceAddress);

nsString
GetAddressFromObjectPath(const nsAString& aObjectPath);

bool
BroadcastSystemMessage(const nsAString& aType,
                       const InfallibleTArray<BluetoothNamedValue>& aData);

END_BLUETOOTH_NAMESPACE

#endif
