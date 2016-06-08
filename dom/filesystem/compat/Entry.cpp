/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Entry.h"
#include "DirectoryEntry.h"
#include "FileEntry.h"
#include "mozilla/dom/UnionTypes.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Entry, mParent, mFileSystem)

NS_IMPL_CYCLE_COLLECTING_ADDREF(Entry)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Entry)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Entry)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* static */ already_AddRefed<Entry>
Entry::Create(nsIGlobalObject* aGlobalObject,
              const OwningFileOrDirectory& aFileOrDirectory,
              DOMFileSystem* aFileSystem)
{
  MOZ_ASSERT(aGlobalObject);
  MOZ_ASSERT(aFileSystem);

  RefPtr<Entry> entry;
  if (aFileOrDirectory.IsFile()) {
    entry = new FileEntry(aGlobalObject,
                          aFileOrDirectory.GetAsFile(),
                          aFileSystem);
  } else {
    MOZ_ASSERT(aFileOrDirectory.IsDirectory());
    entry = new DirectoryEntry(aGlobalObject,
                               aFileOrDirectory.GetAsDirectory(),
                               aFileSystem);
  }

  return entry.forget();
}

Entry::Entry(nsIGlobalObject* aGlobal, DOMFileSystem* aFileSystem)
  : mParent(aGlobal)
  , mFileSystem(aFileSystem)
{
  MOZ_ASSERT(aGlobal);
  MOZ_ASSERT(aFileSystem);
}

Entry::~Entry()
{}

JSObject*
Entry::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return EntryBinding::Wrap(aCx, this, aGivenProto);
}

} // dom namespace
} // mozilla namespace
