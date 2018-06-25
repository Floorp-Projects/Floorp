/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemEntry.h"
#include "FileSystemDirectoryEntry.h"
#include "FileSystemFileEntry.h"
#include "mozilla/dom/FileSystemEntryBinding.h"
#include "mozilla/dom/UnionTypes.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FileSystemEntry, mParent, mParentEntry,
                                      mFileSystem)

NS_IMPL_CYCLE_COLLECTING_ADDREF(FileSystemEntry)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FileSystemEntry)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FileSystemEntry)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* static */ already_AddRefed<FileSystemEntry>
FileSystemEntry::Create(nsIGlobalObject* aGlobalObject,
                        const OwningFileOrDirectory& aFileOrDirectory,
                        FileSystem* aFileSystem)
{
  MOZ_ASSERT(aGlobalObject);
  MOZ_ASSERT(aFileSystem);

  RefPtr<FileSystemEntry> entry;
  if (aFileOrDirectory.IsFile()) {
    entry = new FileSystemFileEntry(aGlobalObject,
                                    aFileOrDirectory.GetAsFile(),
                                    nullptr,
                                    aFileSystem);
  } else {
    MOZ_ASSERT(aFileOrDirectory.IsDirectory());
    entry = new FileSystemDirectoryEntry(aGlobalObject,
                                         aFileOrDirectory.GetAsDirectory(),
                                         nullptr,
                                         aFileSystem);
  }

  return entry.forget();
}

FileSystemEntry::FileSystemEntry(nsIGlobalObject* aGlobal,
                                 FileSystemEntry* aParentEntry,
                                 FileSystem* aFileSystem)
  : mParent(aGlobal)
  , mParentEntry(aParentEntry)
  , mFileSystem(aFileSystem)
{
  MOZ_ASSERT(aGlobal);
  MOZ_ASSERT(aFileSystem);
}

FileSystemEntry::~FileSystemEntry()
{}

JSObject*
FileSystemEntry::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return FileSystemEntry_Binding::Wrap(aCx, this, aGivenProto);
}

void
FileSystemEntry::GetParent(const Optional<OwningNonNull<FileSystemEntryCallback>>& aSuccessCallback,
                           const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback)
{
  if (!aSuccessCallback.WasPassed() && !aErrorCallback.WasPassed()) {
    return;
  }

  if (mParentEntry) {
    FileSystemEntryCallbackHelper::Call(GetParentObject(), aSuccessCallback,
                                        mParentEntry);
    return;
  }

  FileSystemEntryCallbackHelper::Call(GetParentObject(), aSuccessCallback, this);
}

} // dom namespace
} // mozilla namespace
