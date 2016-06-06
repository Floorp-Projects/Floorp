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

void
RootDirectoryEntry::GetInternal(const nsAString& aPath, const FileSystemFlags& aFlag,
                                const Optional<OwningNonNull<EntryCallback>>& aSuccessCallback,
                                const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback,
                                GetInternalType aType) const
{
  if (!aSuccessCallback.WasPassed() && !aErrorCallback.WasPassed()) {
    return;
  }

  if (aFlag.mCreate) {
    ErrorCallbackHelper::Call(GetParentObject(), aErrorCallback,
                              NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsTArray<nsString> parts;
  if (!FileSystemUtils::IsValidRelativeDOMPath(aPath, parts)) {
    ErrorCallbackHelper::Call(GetParentObject(), aErrorCallback,
                              NS_ERROR_DOM_NOT_FOUND_ERR);
    return;
  }

  MOZ_ASSERT(!parts.IsEmpty());

  RefPtr<Entry> entry;
  for (uint32_t i = 0; i < mEntries.Length(); ++i) {
    ErrorResult rv;
    nsAutoString name;
    mEntries[i]->GetName(name, rv);

    if (NS_WARN_IF(rv.Failed())) {
      ErrorCallbackHelper::Call(GetParentObject(), aErrorCallback,
                                rv.StealNSResult());
      return;
    }

    if (name == parts[0]) {
      entry = mEntries[i];
      break;
    }
  }

  // Not found.
  if (!entry) {
    ErrorCallbackHelper::Call(GetParentObject(), aErrorCallback,
                              NS_ERROR_DOM_NOT_FOUND_ERR);
    return;
  }

  // No subdirectory in the path.
  if (parts.Length() == 1) {
    if ((entry->IsFile() && aType == eGetDirectory) ||
        (entry->IsDirectory() && aType == eGetFile)) {
      ErrorCallbackHelper::Call(GetParentObject(), aErrorCallback,
                                NS_ERROR_DOM_TYPE_MISMATCH_ERR);
      return;
    }

    if (aSuccessCallback.WasPassed()) {
      RefPtr<EntryCallbackRunnable> runnable =
        new EntryCallbackRunnable(&aSuccessCallback.Value(), entry);
      nsresult rv = NS_DispatchToMainThread(runnable);
      NS_WARN_IF(NS_FAILED(rv));
    }
    return;
  }

  // Subdirectories, but this is a file.
  if (entry->IsFile()) {
    ErrorCallbackHelper::Call(GetParentObject(), aErrorCallback,
                              NS_ERROR_DOM_NOT_FOUND_ERR);
    return;
  }

  // Let's recreate a path without the first directory.
  nsAutoString path;
  for (uint32_t i = 1, len = parts.Length(); i < len; ++i) {
    path.Append(parts[i]);
    if (i < len - 1) {
      path.AppendLiteral(FILESYSTEM_DOM_PATH_SEPARATOR_LITERAL);
    }
  }

  auto* directoryEntry = static_cast<DirectoryEntry*>(entry.get());
  directoryEntry->GetInternal(path, aFlag, aSuccessCallback, aErrorCallback,
                              aType);
}

} // dom namespace
} // mozilla namespace
