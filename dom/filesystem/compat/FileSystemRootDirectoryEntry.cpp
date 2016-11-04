/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemRootDirectoryEntry.h"
#include "FileSystemRootDirectoryReader.h"
#include "mozilla/dom/FileSystemUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(FileSystemRootDirectoryEntry,
                                   FileSystemDirectoryEntry, mEntries)

NS_IMPL_ADDREF_INHERITED(FileSystemRootDirectoryEntry, FileSystemDirectoryEntry)
NS_IMPL_RELEASE_INHERITED(FileSystemRootDirectoryEntry, FileSystemDirectoryEntry)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(FileSystemRootDirectoryEntry)
NS_INTERFACE_MAP_END_INHERITING(FileSystemDirectoryEntry)

FileSystemRootDirectoryEntry::FileSystemRootDirectoryEntry(nsIGlobalObject* aGlobal,
                                                           const Sequence<RefPtr<FileSystemEntry>>& aEntries,
                                                           FileSystem* aFileSystem)
  : FileSystemDirectoryEntry(aGlobal, nullptr, nullptr, aFileSystem)
  , mEntries(aEntries)
{
  MOZ_ASSERT(aGlobal);
}

FileSystemRootDirectoryEntry::~FileSystemRootDirectoryEntry()
{}

void
FileSystemRootDirectoryEntry::GetName(nsAString& aName, ErrorResult& aRv) const
{
  aName.Truncate();
}

void
FileSystemRootDirectoryEntry::GetFullPath(nsAString& aPath, ErrorResult& aRv) const
{
  aPath.AssignLiteral(FILESYSTEM_DOM_PATH_SEPARATOR_LITERAL);
}

already_AddRefed<FileSystemDirectoryReader>
FileSystemRootDirectoryEntry::CreateReader()
{
  RefPtr<FileSystemDirectoryReader> reader =
    new FileSystemRootDirectoryReader(this, Filesystem(), mEntries);
  return reader.forget();
}

void
FileSystemRootDirectoryEntry::GetInternal(const nsAString& aPath,
                                          const FileSystemFlags& aFlag,
                                          const Optional<OwningNonNull<FileSystemEntryCallback>>& aSuccessCallback,
                                          const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback,
                                          GetInternalType aType)
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

  RefPtr<FileSystemEntry> entry;
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
      DebugOnly<nsresult> rv = NS_DispatchToMainThread(runnable);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "NS_DispatchToMainThread failed");
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

  auto* directoryEntry = static_cast<FileSystemDirectoryEntry*>(entry.get());
  directoryEntry->GetInternal(path, aFlag, aSuccessCallback, aErrorCallback,
                              aType);
}

} // dom namespace
} // mozilla namespace
