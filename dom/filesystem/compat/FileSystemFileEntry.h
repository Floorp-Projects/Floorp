/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FileSystemFileEntry_h
#define mozilla_dom_FileSystemFileEntry_h

#include "mozilla/dom/FileSystemEntry.h"

namespace mozilla {
namespace dom {

class BlobCallback;
class File;
class FileSystemDirectoryEntry;

class FileSystemFileEntry final : public FileSystemEntry
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FileSystemFileEntry, FileSystemEntry)

  FileSystemFileEntry(nsIGlobalObject* aGlobalObject, File* aFile,
                      FileSystemDirectoryEntry* aParentEntry,
                      FileSystem* aFileSystem);

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  virtual bool
  IsFile() const override
  {
    return true;
  }

  virtual void
  GetName(nsAString& aName, ErrorResult& aRv) const override;

  virtual void
  GetFullPath(nsAString& aFullPath, ErrorResult& aRv) const override;

  void
  CreateWriter(VoidCallback& aSuccessCallback,
               const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback) const;

  void
  GetFile(BlobCallback& aSuccessCallback,
          const Optional<OwningNonNull<ErrorCallback>>& aErrorCallback) const;

private:
  ~FileSystemFileEntry();

  RefPtr<File> mFile;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FileSystemFileEntry_h
