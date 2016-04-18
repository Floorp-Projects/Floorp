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
#include "nsGlobalWindow.h"

namespace mozilla {
namespace dom {

DeviceStorageFileSystem::DeviceStorageFileSystem(const nsAString& aStorageType,
                                                 const nsAString& aStorageName)
  : mStorageType(aStorageType)
  , mStorageName(aStorageName)
  , mWindowId(0)
{
  mPermissionCheckType = ePermissionCheckByTestingPref;

  if (NS_IsMainThread()) {
    if (mozilla::Preferences::GetBool("device.storage.prompt.testing", false)) {
      mPermissionCheckType = ePermissionCheckNotRequired;
    } else {
      mPermissionCheckType = ePermissionCheckRequired;
    }
  } else {
    AssertIsOnBackgroundThread();
  }

  // Get the permission name required to access the file system.
  nsresult rv =
    DeviceStorageTypeChecker::GetPermissionForType(mStorageType, mPermission);
  NS_WARN_IF(NS_FAILED(rv));

  // Get the local path of the file system root.
  nsCOMPtr<nsIFile> rootFile;
  DeviceStorageFile::GetRootDirectoryForType(aStorageType,
                                             aStorageName,
                                             getter_AddRefs(rootFile));

  NS_WARN_IF(!rootFile || NS_FAILED(rootFile->GetPath(mLocalOrDeviceStorageRootPath)));

  if (!XRE_IsParentProcess()) {
    return;
  }

  // DeviceStorageTypeChecker is a singleton object and must be initialized on
  // the main thread. We initialize it here so that we can use it on the worker
  // thread.
  if (NS_IsMainThread()) {
    DebugOnly<DeviceStorageTypeChecker*> typeChecker =
      DeviceStorageTypeChecker::CreateOrGet();
    MOZ_ASSERT(typeChecker);
  }
}

DeviceStorageFileSystem::~DeviceStorageFileSystem()
{
  AssertIsOnOwningThread();
}

already_AddRefed<FileSystemBase>
DeviceStorageFileSystem::Clone()
{
  AssertIsOnOwningThread();

  RefPtr<DeviceStorageFileSystem> fs =
    new DeviceStorageFileSystem(mStorageType, mStorageName);

  fs->mWindowId = mWindowId;

  return fs.forget();
}

void
DeviceStorageFileSystem::Init(nsDOMDeviceStorage* aDeviceStorage)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  AssertIsOnOwningThread();
  MOZ_ASSERT(aDeviceStorage);

  nsCOMPtr<nsPIDOMWindowInner> window = aDeviceStorage->GetOwner();
  MOZ_ASSERT(window->IsInnerWindow());
  mWindowId = window->WindowID();
}

void
DeviceStorageFileSystem::Shutdown()
{
  AssertIsOnOwningThread();
  mShutdown = true;
}

nsISupports*
DeviceStorageFileSystem::GetParentObject() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  AssertIsOnOwningThread();

  nsGlobalWindow* window = nsGlobalWindow::GetInnerWindowWithId(mWindowId);
  MOZ_ASSERT_IF(!mShutdown, window);
  return window ? window->AsInner() : nullptr;
}

void
DeviceStorageFileSystem::GetDirectoryName(nsIFile* aFile, nsAString& aRetval,
                                          ErrorResult& aRv) const
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aFile);

  nsCOMPtr<nsIFile> rootPath;
  aRv = NS_NewLocalFile(LocalOrDeviceStorageRootPath(), false,
                        getter_AddRefs(rootPath));
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  bool equal = false;
  aRv = aFile->Equals(rootPath, &equal);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (equal) {
    aRetval = mStorageName;
    return;
  }

  FileSystemBase::GetDirectoryName(aFile, aRetval, aRv);
  NS_WARN_IF(aRv.Failed());
}

bool
DeviceStorageFileSystem::IsSafeFile(nsIFile* aFile) const
{
  MOZ_ASSERT(XRE_IsParentProcess(), "Should be on parent process!");
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aFile);

  nsCOMPtr<nsIFile> rootPath;
  nsresult rv = NS_NewLocalFile(LocalOrDeviceStorageRootPath(), false,
                                getter_AddRefs(rootPath));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  // Check if this file belongs to this storage.
  if (NS_WARN_IF(!FileSystemUtils::IsDescendantPath(rootPath, aFile))) {
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
  AssertIsOnOwningThread();
  MOZ_ASSERT(aDir);

  ErrorResult rv;
  RefPtr<FileSystemBase> fs = aDir->GetFileSystem(rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
    return false;
  }

  nsAutoString fsSerialization;
  fs->SerializeDOMPath(fsSerialization);

  nsAutoString thisSerialization;
  SerializeDOMPath(thisSerialization);

  // Check if the given directory is from this storage.
  return fsSerialization == thisSerialization;
}

void
DeviceStorageFileSystem::SerializeDOMPath(nsAString& aString) const
{
  AssertIsOnOwningThread();

  // Generate the string representation of the file system.
  aString.AssignLiteral("devicestorage-");
  aString.Append(mStorageType);
  aString.Append('-');
  aString.Append(mStorageName);
}

nsresult
DeviceStorageFileSystem::MainThreadWork()
{
  MOZ_ASSERT(NS_IsMainThread());

  DebugOnly<DeviceStorageTypeChecker*> typeChecker =
    DeviceStorageTypeChecker::CreateOrGet();
  MOZ_ASSERT(typeChecker);
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
