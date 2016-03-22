/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothMapFolder.h"
#include "base/basictypes.h"

BEGIN_BLUETOOTH_NAMESPACE

BluetoothMapFolder::~BluetoothMapFolder()
{ }

BluetoothMapFolder::BluetoothMapFolder(const nsAString& aFolderName,
                                       BluetoothMapFolder* aParent)
  : mName(aFolderName)
  , mParent(aParent)
{
}

BluetoothMapFolder*
BluetoothMapFolder::AddSubFolder(const nsAString& aFolderName)
{
  RefPtr<BluetoothMapFolder> folder = new BluetoothMapFolder(aFolderName,
                                                               this);
  mSubFolders.Put(nsString(aFolderName), folder);

  return folder;
}

BluetoothMapFolder*
BluetoothMapFolder::GetSubFolder(const nsAString& aFolderName)
{
  BluetoothMapFolder* subfolder;
  mSubFolders.Get(aFolderName, &subfolder);

  return subfolder;
}

BluetoothMapFolder*
BluetoothMapFolder::GetParentFolder()
{
  return mParent;
}

int
BluetoothMapFolder::GetSubFolderCount()
{
  return mSubFolders.Count();
}

void
BluetoothMapFolder::GetFolderListingObjectString(nsAString& aString,
                                                 uint16_t aMaxListCount,
                                                 uint16_t aStartOffset)
{
  const char* folderListingPrefix =
    "<?xml version=\"1.0\"?>\n"
    "<!DOCTYPE folder-listing SYSTEM \" obex-folder-listing.dtd\">\n"
    "<folder-listing version=\"1.0\">\n";
  const char* folderListingSuffix = "</folder-listing>";

  // Based on Element Specification, 9.1.1, IrObex 1.2
  nsAutoCString folderListingObejct(folderListingPrefix);

  int count = 0;
  for (auto iter = mSubFolders.Iter(); !iter.Done(); iter.Next()) {
    if (count < aStartOffset) {
      continue;
    }

    if (count > aMaxListCount) {
      break;
    }

    const nsAString& key = iter.Key();
    folderListingObejct.Append("<folder name=\"");
    folderListingObejct.Append(NS_ConvertUTF16toUTF8(key).get());
    folderListingObejct.Append("\"/>");
    count++;
  }

  folderListingObejct.Append(folderListingSuffix);
  aString = NS_ConvertUTF8toUTF16(folderListingObejct);
}

void
BluetoothMapFolder::DumpFolderInfo()
{
  BT_LOGR("Folder name: %s, subfolder counts: %d",
          NS_ConvertUTF16toUTF8(mName).get(), mSubFolders.Count());
}

END_BLUETOOTH_NAMESPACE
