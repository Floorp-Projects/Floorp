/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DeviceStorageFileSystem.h"

#include "DeviceStorage.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/Directory.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsDeviceStorage.h"
#include "nsIFile.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

DeviceStorageFileSystem::DeviceStorageFileSystem(
  const nsAString& aStorageType,
  const nsAString& aStorageName)
  : mDeviceStorage(nullptr)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");

  mStorageType = aStorageType;
  mStorageName = aStorageName;

  // Generate the string representation of the file system.
  mString.AppendLiteral("devicestorage-");
  mString.Append(mStorageType);
  mString.Append('-');
  mString.Append(mStorageName);

  mRequiresPermissionChecks =
    !mozilla::Preferences::GetBool("device.storage.prompt.testing", false);

  // Get the permission name required to access the file system.
  nsresult rv =
    DeviceStorageTypeChecker::GetPermissionForType(mStorageType, mPermission);
  NS_WARN_IF(NS_FAILED(rv));

  // Get the local path of the file system root.
  // Since the child process is not allowed to access the file system, we only
  // do this from the parent process.
  if (!XRE_IsParentProcess()) {
    return;
  }
  nsCOMPtr<nsIFile> rootFile;
  DeviceStorageFile::GetRootDirectoryForType(aStorageType,
                                             aStorageName,
                                             getter_AddRefs(rootFile));

  NS_WARN_IF(!rootFile || NS_FAILED(rootFile->GetPath(mLocalRootPath)));
  FileSystemUtils::LocalPathToNormalizedPath(mLocalRootPath,
    mNormalizedLocalRootPath);

  // DeviceStorageTypeChecker is a singleton object and must be initialized on
  // the main thread. We initialize it here so that we can use it on the worker
  // thread.
  DebugOnly<DeviceStorageTypeChecker*> typeChecker
    = DeviceStorageTypeChecker::CreateOrGet();
  MOZ_ASSERT(typeChecker);
}

DeviceStorageFileSystem::~DeviceStorageFileSystem()
{
}

void
DeviceStorageFileSystem::Init(nsDOMDeviceStorage* aDeviceStorage)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aDeviceStorage);
  mDeviceStorage = aDeviceStorage;
}

void
DeviceStorageFileSystem::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  mDeviceStorage = nullptr;
  mShutdown = true;
}

nsPIDOMWindow*
DeviceStorageFileSystem::GetWindow() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  if (!mDeviceStorage) {
    return nullptr;
  }
  return mDeviceStorage->GetOwner();
}

void
DeviceStorageFileSystem::GetRootName(nsAString& aRetval) const
{
  aRetval = mStorageName;
}

bool
DeviceStorageFileSystem::IsSafeFile(nsIFile* aFile) const
{
  MOZ_ASSERT(XRE_IsParentProcess(),
             "Should be on parent process!");
  MOZ_ASSERT(aFile);

  // Check if this file belongs to this storage.
  nsAutoString path;
  if (NS_FAILED(aFile->GetPath(path))) {
    return false;
  }
  if (!LocalPathToRealPath(path, path)) {
    return false;
  }

  // Check if the file type is compatible with the storage type.
  DeviceStorageTypeChecker* typeChecker
    = DeviceStorageTypeChecker::CreateOrGet();
  MOZ_ASSERT(typeChecker);
  return typeChecker->Check(mStorageType, aFile);
}

bool
DeviceStorageFileSystem::IsSafeDirectory(Directory* aDir) const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(aDir);
  nsRefPtr<FileSystemBase> fs = aDir->GetFileSystem();
  MOZ_ASSERT(fs);
  // Check if the given directory is from this storage.
  return fs->ToString() == mString;
}

} // namespace dom
} // namespace mozilla
