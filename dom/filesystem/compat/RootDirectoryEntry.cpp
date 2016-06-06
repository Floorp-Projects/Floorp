/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RootDirectoryEntry.h"
#include "RootDirectoryReader.h"
#include "mozilla/dom/FileSystemUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(RootDirectoryEntry, DirectoryEntry, mEntries)

NS_IMPL_ADDREF_INHERITED(RootDirectoryEntry, DirectoryEntry)
NS_IMPL_RELEASE_INHERITED(RootDirectoryEntry, DirectoryEntry)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(RootDirectoryEntry)
NS_INTERFACE_MAP_END_INHERITING(DirectoryEntry)

RootDirectoryEntry::RootDirectoryEntry(nsIGlobalObject* aGlobal,
                                       const Sequence<RefPtr<Entry>>& aEntries,
                                       DOMFileSystem* aFileSystem)
  : DirectoryEntry(aGlobal, nullptr, aFileSystem)
  , mEntries(aEntries)
{
  MOZ_ASSERT(aGlobal);
}

RootDirectoryEntry::~RootDirectoryEntry()
{}

void
RootDirectoryEntry::GetName(nsAString& aName, ErrorResult& aRv) const
{
  aName.Truncate();
}

void
RootDirectoryEntry::GetFullPath(nsAString& aPath, ErrorResult& aRv) const
{
  aPath.AssignLiteral(FILESYSTEM_DOM_PATH_SEPARATOR_LITERAL);
}

already_AddRefed<DirectoryReader>
RootDirectoryEntry::CreateReader() const
{
  RefPtr<DirectoryReader> reader =
    new RootDirectoryReader(GetParentObject(), Filesystem(), mEntries);
  return reader.forget();
}

} // dom namespace
} // mozilla namespace
