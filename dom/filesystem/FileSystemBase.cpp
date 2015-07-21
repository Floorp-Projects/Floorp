/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FileSystemBase.h"

#include "DeviceStorageFileSystem.h"
#include "nsCharSeparatedTokenizer.h"
#include "OSFileSystem.h"

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
  return nsRefPtr<OSFileSystem>(new OSFileSystem(aString)).forget();
}

FileSystemBase::FileSystemBase()
  : mShutdown(false)
  , mRequiresPermissionChecks(true)
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

already_AddRefed<nsIFile>
FileSystemBase::GetLocalFile(const nsAString& aRealPath) const
{
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Should be on parent process!");
  nsAutoString localPath;
  FileSystemUtils::NormalizedPathToLocalPath(aRealPath, localPath);
  localPath = mLocalRootPath + localPath;
  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_NewLocalFile(localPath, false, getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }
  return file.forget();
}

bool
FileSystemBase::GetRealPath(BlobImpl* aFile, nsAString& aRealPath) const
{
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Should be on parent process!");
  MOZ_ASSERT(aFile, "aFile Should not be null.");

  aRealPath.Truncate();

  nsAutoString filePath;
  ErrorResult rv;
  aFile->GetMozFullPathInternal(filePath, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return false;
  }

  return LocalPathToRealPath(filePath, aRealPath);
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

bool
FileSystemBase::LocalPathToRealPath(const nsAString& aLocalPath,
                                    nsAString& aRealPath) const
{
  nsAutoString path;
  FileSystemUtils::LocalPathToNormalizedPath(aLocalPath, path);
  if (!FileSystemUtils::IsDescendantPath(mNormalizedLocalRootPath, path)) {
    aRealPath.Truncate();
    return false;
  }
  aRealPath = Substring(path, mNormalizedLocalRootPath.Length());
  return true;
}

} // namespace dom
} // namespace mozilla
