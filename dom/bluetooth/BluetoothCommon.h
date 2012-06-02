/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothcommon_h__
#define mozilla_dom_bluetooth_bluetoothcommon_h__

#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/Observer.h"

#define BEGIN_BLUETOOTH_NAMESPACE \
  namespace mozilla { namespace dom { namespace bluetooth {
#define END_BLUETOOTH_NAMESPACE \
  } /* namespace bluetooth */ } /* namespace dom */ } /* namespace mozilla */
#define USING_BLUETOOTH_NAMESPACE \
  using namespace mozilla::dom::bluetooth;

class nsCString;

BEGIN_BLUETOOTH_NAMESPACE

/**
 * BluetoothEvents usually hand back one of 3 types:
 *
 * - 32-bit Int
 * - String
 * - Bool
 *
 * BluetoothVariant encases the types into a single structure.
 */
struct BluetoothVariant
{
  uint32_t mUint32;
  nsCString mString;  
};

/**
 * BluetoothNamedVariant is a variant with a name value, for passing around
 * things like properties with variant values.
 */
struct BluetoothNamedVariant
{
  nsCString mName;
  BluetoothVariant mValue;
};

/**
 * BluetoothEvent holds a variant value and the name of an event, such as
 * PropertyChanged or DeviceFound.
 */
struct BluetoothEvent
{
  nsCString mEventName;
  nsTArray<BluetoothNamedVariant> mValues;
};

typedef mozilla::Observer<BluetoothEvent> BluetoothEventObserver;
typedef mozilla::ObserverList<BluetoothEvent> BluetoothEventObserverList;

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_bluetoothcommon_h__
