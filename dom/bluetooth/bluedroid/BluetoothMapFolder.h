/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_BluetoothMapFolder_h
#define mozilla_dom_bluetooth_bluedroid_BluetoothMapFolder_h

#include "BluetoothCommon.h"
#include "nsRefPtrHashtable.h"
#include "nsTArray.h"

BEGIN_BLUETOOTH_NAMESPACE

/* This class maps MAP virtual folder structures */
class BluetoothMapFolder
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BluetoothMapFolder)

  BluetoothMapFolder(const nsAString& aFolderName, BluetoothMapFolder* aParent);
  // Add virtual folder to subfolders
  BluetoothMapFolder* AddSubFolder(const nsAString& aFolderName);
  BluetoothMapFolder* GetSubFolder(const nsAString& aFolderName);
  BluetoothMapFolder* GetParentFolder();
  int GetSubFolderCount();
  // Format folder listing object string
  void GetFolderListingObjectString(nsAString& aString, uint16_t aMaxListCount,
                                    uint16_t aStartOffset);
  void DumpFolderInfo();
private:
  ~BluetoothMapFolder();
  nsString mName;
  RefPtr<BluetoothMapFolder> mParent;
  nsRefPtrHashtable<nsStringHashKey, BluetoothMapFolder> mSubFolders;
};

END_BLUETOOTH_NAMESPACE
#endif //mozilla_dom_bluetooth_bluedroid_BluetoothMapFolder_h
