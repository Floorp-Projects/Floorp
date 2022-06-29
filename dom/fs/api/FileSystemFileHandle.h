/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_FILESYSTEMFILEHANDLE_H_
#define DOM_FS_FILESYSTEMFILEHANDLE_H_

#include "mozilla/dom/FileSystemHandle.h"

namespace mozilla {

class ErrorResult;

namespace dom {

struct FileSystemCreateWritableOptions;

class FileSystemFileHandle final : public FileSystemHandle {
 public:
  FileSystemFileHandle(nsIGlobalObject* aGlobal,
                       const fs::FileSystemEntryMetadata& aMetadata,
                       fs::FileSystemRequestHandler* aRequestHandler);

  FileSystemFileHandle(nsIGlobalObject* aGlobal,
                       const fs::FileSystemEntryMetadata& aMetadata);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FileSystemFileHandle,
                                           FileSystemHandle)

  // WebIDL Boilerplate
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL interface
  FileSystemHandleKind Kind() const override;

  already_AddRefed<Promise> GetFile(ErrorResult& aError);

  already_AddRefed<Promise> CreateWritable(
      const FileSystemCreateWritableOptions& aOptions, ErrorResult& aError);

  already_AddRefed<Promise> CreateSyncAccessHandle(ErrorResult& aError);

 private:
  ~FileSystemFileHandle() = default;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_FS_FILESYSTEMFILEHANDLE_H_
