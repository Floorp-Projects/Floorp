/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileSystemRootDirectoryReader_h
#define mozilla_dom_FileSystemRootDirectoryReader_h

#include "FileSystemDirectoryReader.h"

namespace mozilla {
namespace dom {

class FileSystemRootDirectoryReader final : public FileSystemDirectoryReader
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FileSystemRootDirectoryReader,
                                           FileSystemDirectoryReader)

  explicit FileSystemRootDirectoryReader(FileSystemDirectoryEntry* aParentEntry,
                                         FileSystem* aFileSystem,
                                         const Sequence<RefPtr<FileSystemEntry>>& aEntries);

  virtual void
  ReadEntries(FileSystemEntriesCallback& aSuccessCallback,
              const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback,
              ErrorResult& aRv) override;

private:
  ~FileSystemRootDirectoryReader();

  Sequence<RefPtr<FileSystemEntry>> mEntries;
  bool mAlreadyRead;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileSystemRootDirectoryReader_h
