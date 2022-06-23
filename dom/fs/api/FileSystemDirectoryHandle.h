/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_FILESYSTEMDIRECTORYHANDLE_H_
#define DOM_FS_FILESYSTEMDIRECTORYHANDLE_H_

#include "mozilla/dom/FileSystemHandle.h"

namespace mozilla {

class ErrorResult;

namespace dom {

class FileSystemDirectoryIterator;
struct FileSystemGetFileOptions;
struct FileSystemGetDirectoryOptions;
struct FileSystemRemoveOptions;

class FileSystemDirectoryHandle final : public FileSystemHandle {
 public:
  FileSystemDirectoryHandle(nsIGlobalObject* aGlobal,
                            const fs::FileSystemEntryMetadata& aMetadata,
                            fs::FileSystemRequestHandler* aRequestHandler);

  FileSystemDirectoryHandle(nsIGlobalObject* aGlobal,
                            const fs::FileSystemEntryMetadata& aMetadata);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FileSystemDirectoryHandle,
                                           FileSystemHandle)

  // WebIDL Boilerplate
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Interface
  FileSystemHandleKind Kind() const override;

  [[nodiscard]] already_AddRefed<FileSystemDirectoryIterator> Entries();

  [[nodiscard]] already_AddRefed<FileSystemDirectoryIterator> Keys();

  [[nodiscard]] already_AddRefed<FileSystemDirectoryIterator> Values();

  already_AddRefed<Promise> GetFileHandle(
      const nsAString& aName, const FileSystemGetFileOptions& aOptions,
      ErrorResult& aError);

  already_AddRefed<Promise> GetDirectoryHandle(
      const nsAString& aName, const FileSystemGetDirectoryOptions& aOptions,
      ErrorResult& aError);

  already_AddRefed<Promise> RemoveEntry(const nsAString& aName,
                                        const FileSystemRemoveOptions& aOptions,
                                        ErrorResult& aError);

  already_AddRefed<Promise> Resolve(FileSystemHandle& aPossibleDescendant,
                                    ErrorResult& aError);

 private:
  ~FileSystemDirectoryHandle() = default;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_FS_FILESYSTEMDIRECTORYHANDLE_H_
