/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileEntry.h"
#include "mozilla/dom/File.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(FileEntry, Entry, mFile)

NS_IMPL_ADDREF_INHERITED(FileEntry, Entry)
NS_IMPL_RELEASE_INHERITED(FileEntry, Entry)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(FileEntry)
NS_INTERFACE_MAP_END_INHERITING(Entry)

FileEntry::FileEntry(nsIGlobalObject* aGlobal,
                     File* aFile)
  : Entry(aGlobal)
  , mFile(aFile)
{
  MOZ_ASSERT(aGlobal);
  MOZ_ASSERT(mFile);
}

FileEntry::~FileEntry()
{}

JSObject*
FileEntry::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return FileEntryBinding::Wrap(aCx, this, aGivenProto);
}

void
FileEntry::GetName(nsAString& aName, ErrorResult& aRv) const
{
  mFile->GetName(aName);
}

void
FileEntry::GetFullPath(nsAString& aPath, ErrorResult& aRv) const
{
  mFile->GetPath(aPath);
}

} // dom namespace
} // mozilla namespace
