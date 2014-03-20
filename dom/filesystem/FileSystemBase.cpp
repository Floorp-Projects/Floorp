/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FileSystemBase.h"

#include "DeviceStorageFileSystem.h"
#include "nsCharSeparatedTokenizer.h"

namespace mozilla {
namespace dom {

// static
already_AddRefed<FileSystemBase>
FileSystemBase::FromString(const nsAString& aString)
{
  if (StringBeginsWith(aString, NS_LITERAL_STRING("devicestorage-"))) {
    // The string representation of devicestorage file system is of the format:
    // devicestorage-StorageType-StorageName

    nsCharSeparatedTokenizer tokenizer(aString, char16_t('-'));
    tokenizer.nextToken();

    nsString storageType;
    if (tokenizer.hasMoreTokens()) {
      storageType = tokenizer.nextToken();
    }

    nsString storageName;
    if (tokenizer.hasMoreTokens()) {
      storageName = tokenizer.nextToken();
    }

    nsRefPtr<DeviceStorageFileSystem> f =
      new DeviceStorageFileSystem(storageType, storageName);
    return f.forget();
  }
  return nullptr;
}

FileSystemBase::FileSystemBase()
  : mShutdown(false)
  , mIsTesting(false)
{
}

FileSystemBase::~FileSystemBase()
{
}

void
FileSystemBase::Shutdown()
{
  mShutdown = true;
}

nsPIDOMWindow*
FileSystemBase::GetWindow() const
{
  return nullptr;
}

bool
FileSystemBase::IsSafeFile(nsIFile* aFile) const
{
  return false;
}

bool
FileSystemBase::IsSafeDirectory(Directory* aDir) const
{
  return false;
}

} // namespace dom
} // namespace mozilla
