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
FileSystemBase::DeserializeDOMPath(const nsAString& aString)
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

    RefPtr<DeviceStorageFileSystem> f =
      new DeviceStorageFileSystem(storageType, storageName);
    return f.forget();
  }

  return RefPtr<OSFileSystem>(new OSFileSystem(aString)).forget();
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

nsISupports*
FileSystemBase::GetParentObject() const
{
  return nullptr;
}

bool
FileSystemBase::GetRealPath(BlobImpl* aFile, nsIFile** aPath) const
{
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Should be on parent process!");
  MOZ_ASSERT(aFile, "aFile Should not be null.");

  nsAutoString filePath;
  ErrorResult rv;
  aFile->GetMozFullPathInternal(filePath, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return false;
  }

  rv = NS_NewNativeLocalFile(NS_ConvertUTF16toUTF8(filePath),
                             true, aPath);
  if (NS_WARN_IF(rv.Failed())) {
    return false;
  }

  return true;
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

void
FileSystemBase::GetDOMPath(nsIFile* aFile,
                           Directory::DirectoryType aType,
                           nsAString& aRetval,
                           ErrorResult& aRv) const
{
  MOZ_ASSERT(aFile);

  if (aType == Directory::eDOMRootDirectory) {
    aRetval.AssignLiteral(FILESYSTEM_DOM_PATH_SEPARATOR_LITERAL);
    return;
  }

  nsCOMPtr<nsIFile> fileSystemPath;
  aRv = NS_NewNativeLocalFile(NS_ConvertUTF16toUTF8(LocalOrDeviceStorageRootPath()),
                              true, getter_AddRefs(fileSystemPath));
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  MOZ_ASSERT(FileSystemUtils::IsDescendantPath(fileSystemPath, aFile));

  nsCOMPtr<nsIFile> path;
  aRv = aFile->Clone(getter_AddRefs(path));
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsTArray<nsString> parts;

  while (true) {
    bool equal = false;
    aRv = fileSystemPath->Equals(path, &equal);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    if (equal) {
      break;
    }

    nsAutoString leafName;
    aRv = path->GetLeafName(leafName);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    parts.AppendElement(leafName);

    nsCOMPtr<nsIFile> parentPath;
    aRv = path->GetParent(getter_AddRefs(parentPath));
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    MOZ_ASSERT(parentPath);

    aRv = parentPath->Clone(getter_AddRefs(path));
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }
  }

  MOZ_ASSERT(!parts.IsEmpty());

  aRetval.Truncate();

  for (int32_t i = parts.Length() - 1; i >= 0; --i) {
    aRetval.AppendLiteral(FILESYSTEM_DOM_PATH_SEPARATOR_LITERAL);
    aRetval.Append(parts[i]);
  }
}

} // namespace dom
} // namespace mozilla
