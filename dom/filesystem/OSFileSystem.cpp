/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/OSFileSystem.h"

#include "mozilla/dom/Directory.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "nsIGlobalObject.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsIFile.h"

namespace mozilla {
namespace dom {

OSFileSystem::OSFileSystem(const nsAString& aRootDir)
{
  mLocalOrDeviceStorageRootPath = aRootDir;

  // Non-mobile devices don't have the concept of separate permissions to
  // access different parts of devices storage like Pictures, or Videos, etc.
  mRequiresPermissionChecks = false;

#ifdef DEBUG
  mPermission.AssignLiteral("never-used");
#endif
}

already_AddRefed<FileSystemBase>
OSFileSystem::Clone()
{
  RefPtr<OSFileSystem> fs = new OSFileSystem(mLocalOrDeviceStorageRootPath);
  if (mParent) {
    fs->Init(mParent);
  }

  return fs.forget();
}

void
OSFileSystem::Init(nsISupports* aParent)
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  MOZ_ASSERT(!mParent, "No duple Init() calls");
  MOZ_ASSERT(aParent);
  mParent = aParent;

#ifdef DEBUG
  nsCOMPtr<nsIGlobalObject> obj = do_QueryInterface(aParent);
  MOZ_ASSERT(obj);
#endif
}

nsISupports*
OSFileSystem::GetParentObject() const
{
  MOZ_ASSERT(NS_IsMainThread(), "Only call on main thread!");
  return mParent;
}

void
OSFileSystem::GetRootName(nsAString& aRetval) const
{
  aRetval.AssignLiteral(FILESYSTEM_DOM_PATH_SEPARATOR_LITERAL);
}

bool
OSFileSystem::IsSafeFile(nsIFile* aFile) const
{
  // The concept of "safe files" is specific to the Device Storage API where
  // files are only "safe" if they're of a type that is appropriate for the
  // area of device storage that is being used.
  MOZ_CRASH("Don't use OSFileSystem with the Device Storage API");
  return true;
}

bool
OSFileSystem::IsSafeDirectory(Directory* aDir) const
{
  // The concept of "safe directory" is specific to the Device Storage API
  // where a directory is only "safe" if it belongs to the area of device
  // storage that it is being used with.
  MOZ_CRASH("Don't use OSFileSystem with the Device Storage API");
  return true;
}

void
OSFileSystem::Unlink()
{
  mParent = nullptr;
}

void
OSFileSystem::Traverse(nsCycleCollectionTraversalCallback &cb)
{
  OSFileSystem* tmp = this;
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent);
}

void
OSFileSystem::SerializeDOMPath(nsAString& aOutput) const
{
  aOutput = mLocalOrDeviceStorageRootPath;
}

} // namespace dom
} // namespace mozilla
