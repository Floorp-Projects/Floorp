/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystem.h"
#include "FileSystemRootDirectoryEntry.h"
#include "mozilla/dom/FileSystemBinding.h"
#include "nsContentUtils.h"
#include "nsIDUtils.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FileSystem, mParent, mRoot)

NS_IMPL_CYCLE_COLLECTING_ADDREF(FileSystem)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FileSystem)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FileSystem)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* static */
already_AddRefed<FileSystem> FileSystem::Create(nsIGlobalObject* aGlobalObject)

{
  MOZ_ASSERT(aGlobalObject);

  nsID id;
  nsresult rv = nsContentUtils::GenerateUUIDInPlace(id);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  NSID_TrimBracketsUTF16 name(id);

  RefPtr<FileSystem> fs = new FileSystem(aGlobalObject, name);

  return fs.forget();
}

FileSystem::FileSystem(nsIGlobalObject* aGlobal, const nsAString& aName)
    : mParent(aGlobal), mName(aName) {
  MOZ_ASSERT(aGlobal);
}

FileSystem::~FileSystem() = default;

JSObject* FileSystem::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return FileSystem_Binding::Wrap(aCx, this, aGivenProto);
}

void FileSystem::CreateRoot(const Sequence<RefPtr<FileSystemEntry>>& aEntries) {
  MOZ_ASSERT(!mRoot);
  mRoot = new FileSystemRootDirectoryEntry(mParent, aEntries, this);
}

}  // namespace mozilla::dom
