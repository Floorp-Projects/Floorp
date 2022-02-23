/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_FILESYSTEMFILEHANDLE_H_
#define DOM_FS_FILESYSTEMFILEHANDLE_H_

#include "mozilla/dom/FileSystemHandle.h"

namespace mozilla::dom {

struct FileSystemCreateWritableOptions;

class FileSystemFileHandle final : public FileSystemHandle {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(FileSystemFileHandle,
                                           FileSystemHandle)

  // WebIDL Boilerplate
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL interface
  FileSystemHandleKind Kind() override;

  already_AddRefed<Promise> GetFile();

#ifdef MOZ_DOM_STREAMS
  already_AddRefed<Promise> CreateWritable(
      const FileSystemCreateWritableOptions& aOptions);
#endif

  already_AddRefed<Promise> CreateSyncAccessHandle();

 private:
  ~FileSystemFileHandle() = default;
};

}  // namespace mozilla::dom

#endif  // DOM_FS_FILESYSTEMFILEHANDLE_H_
