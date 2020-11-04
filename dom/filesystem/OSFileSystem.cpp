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

namespace mozilla::dom {

OSFileSystem::OSFileSystem(const nsAString& aRootDir) {
  mLocalRootPath = aRootDir;
}

already_AddRefed<FileSystemBase> OSFileSystem::Clone() {
  AssertIsOnOwningThread();

  RefPtr<OSFileSystem> fs = new OSFileSystem(mLocalRootPath);
  if (mGlobal) {
    fs->Init(mGlobal);
  }

  return fs.forget();
}

void OSFileSystem::Init(nsIGlobalObject* aGlobal) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mGlobal, "No duple Init() calls");
  MOZ_ASSERT(aGlobal);

  mGlobal = aGlobal;
}

nsIGlobalObject* OSFileSystem::GetParentObject() const {
  AssertIsOnOwningThread();
  return mGlobal;
}

bool OSFileSystem::IsSafeFile(nsIFile* aFile) const {
  // The concept of "safe files" is specific to the Device Storage API where
  // files are only "safe" if they're of a type that is appropriate for the
  // area of device storage that is being used.
  MOZ_CRASH("Don't use OSFileSystem with the Device Storage API");
  return true;
}

bool OSFileSystem::IsSafeDirectory(Directory* aDir) const {
  // The concept of "safe directory" is specific to the Device Storage API
  // where a directory is only "safe" if it belongs to the area of device
  // storage that it is being used with.
  MOZ_CRASH("Don't use OSFileSystem with the Device Storage API");
  return true;
}

void OSFileSystem::Unlink() {
  AssertIsOnOwningThread();
  mGlobal = nullptr;
}

void OSFileSystem::Traverse(nsCycleCollectionTraversalCallback& cb) {
  AssertIsOnOwningThread();

  OSFileSystem* tmp = this;
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal);
}

void OSFileSystem::SerializeDOMPath(nsAString& aOutput) const {
  AssertIsOnOwningThread();
  aOutput = mLocalRootPath;
}

/**
 * OSFileSystemParent
 */

OSFileSystemParent::OSFileSystemParent(const nsAString& aRootDir) {
  mLocalRootPath = aRootDir;
}

}  // namespace mozilla::dom
