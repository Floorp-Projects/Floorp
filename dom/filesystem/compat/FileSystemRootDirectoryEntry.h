/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileSystemRootDirectoryEntry_h
#define mozilla_dom_FileSystemRootDirectoryEntry_h

#include "mozilla/dom/FileSystemDirectoryEntry.h"

namespace mozilla {
namespace dom {

class FileSystemRootDirectoryEntry final : public FileSystemDirectoryEntry
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FileSystemRootDirectoryEntry, FileSystemDirectoryEntry)

  FileSystemRootDirectoryEntry(nsIGlobalObject* aGlobalObject,
                               const Sequence<RefPtr<FileSystemEntry>>& aEntries,
                               FileSystem* aFileSystem);

  virtual void
  GetName(nsAString& aName, ErrorResult& aRv) const override;

  virtual void
  GetFullPath(nsAString& aFullPath, ErrorResult& aRv) const override;

  virtual already_AddRefed<FileSystemDirectoryReader>
  CreateReader() override;

private:
  ~FileSystemRootDirectoryEntry();

  virtual void
  GetInternal(const nsAString& aPath, const FileSystemFlags& aFlag,
              const Optional<OwningNonNull<FileSystemEntryCallback>>& aSuccessCallback,
              const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback,
              GetInternalType aType) override;

  void
  Error(const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback,
        nsresult aError) const;

  Sequence<RefPtr<FileSystemEntry>> mEntries;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileSystemRootDirectoryEntry_h
