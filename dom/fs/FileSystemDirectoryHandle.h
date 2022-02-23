/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_FILESYSTEMDIRECTORYHANDLE_H_
#define DOM_FS_FILESYSTEMDIRECTORYHANDLE_H_

#include "mozilla/dom/FileSystemHandle.h"

namespace mozilla::dom {

class FileSystemDirectoryIterator;
struct FileSystemGetFileOptions;
struct FileSystemGetDirectoryOptions;
struct FileSystemRemoveOptions;

class FileSystemDirectoryHandle final : public FileSystemHandle {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FileSystemDirectoryHandle,
                                           FileSystemHandle)

  // WebIDL Boilerplate
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Interface
  FileSystemHandleKind Kind() override;

  [[nodiscard]] already_AddRefed<FileSystemDirectoryIterator> Entries();

  [[nodiscard]] already_AddRefed<FileSystemDirectoryIterator> Keys();

  [[nodiscard]] already_AddRefed<FileSystemDirectoryIterator> Values();

  already_AddRefed<Promise> GetFileHandle(
      const nsAString& aName, const FileSystemGetFileOptions& aOptions);

  already_AddRefed<Promise> GetDirectoryHandle(
      const nsAString& aName, const FileSystemGetDirectoryOptions& aOptions);

  already_AddRefed<Promise> RemoveEntry(
      const nsAString& aName, const FileSystemRemoveOptions& aOptions);

  already_AddRefed<Promise> Resolve(FileSystemHandle& aPossibleDescendant);

 private:
  ~FileSystemDirectoryHandle() = default;
};

}  // namespace mozilla::dom

#endif  // DOM_FS_FILESYSTEMDIRECTORYHANDLE_H_
