/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This file contains hash-table keys for Bluetooth classes. */

#ifndef mozilla_dom_bluetooth_BluetoothHashKeys_h
#define mozilla_dom_bluetooth_BluetoothHashKeys_h

#include "BluetoothCommon.h"
#include <mozilla/HashFunctions.h>
#include <nsHashKeys.h>

BEGIN_BLUETOOTH_NAMESPACE

/**
 * Implements a hash-table key for |BluetoothAddress|.
 */
class BluetoothAddressHashKey : public PLDHashEntryHdr
{
public:
  enum {
    ALLOW_MEMMOVE = true
  };

  typedef const BluetoothAddress& KeyType;
  typedef const BluetoothAddress* KeyTypePointer;

  explicit BluetoothAddressHashKey(KeyTypePointer aKey)
    : mValue(*aKey)
  { }
  BluetoothAddressHashKey(const BluetoothAddressHashKey& aToCopy)
    : mValue(aToCopy.mValue)
  { }
  ~BluetoothAddressHashKey()
  { }
  KeyType GetKey() const
  {
    return mValue;
  }
  bool KeyEquals(KeyTypePointer aKey) const
  {
    return *aKey == mValue;
  }
  static KeyTypePointer KeyToPointer(KeyType aKey)
  {
    return &aKey;
  }
  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    return HashBytes(aKey->mAddr, MOZ_ARRAY_LENGTH(aKey->mAddr));
  }
private:
  const BluetoothAddress mValue;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_BluetoothHashKeys_h
