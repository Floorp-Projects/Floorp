/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_CHILD_FILESYSTEMREQUESTHANDLER_H_
#define DOM_FS_CHILD_FILESYSTEMREQUESTHANDLER_H_

#include "mozilla/dom/FileSystemActorHolder.h"
#include "mozilla/dom/FileSystemTypes.h"

namespace mozilla::dom {

class Promise;

namespace fs {

class ArrayAppendable {};
class FileSystemChildMetadata;
class FileSystemEntryMetadata;

class FileSystemRequestHandler {
 public:
  virtual void GetRootHandle(RefPtr<FileSystemActorHolder>& aActor,
                             RefPtr<Promise> aPromise);

  virtual void GetDirectoryHandle(RefPtr<FileSystemActorHolder>& aActor,
                                  const FileSystemChildMetadata& aDirectory,
                                  bool aCreate, RefPtr<Promise> aPromise);

  virtual void GetFileHandle(RefPtr<FileSystemActorHolder>& aActor,
                             const FileSystemChildMetadata& aFile, bool aCreate,
                             RefPtr<Promise> aPromise);

  virtual void GetFile(RefPtr<FileSystemActorHolder>& aActor,
                       const FileSystemEntryMetadata& aFile,
                       RefPtr<Promise> aPromise);

  virtual void GetEntries(RefPtr<FileSystemActorHolder>& aActor,
                          const EntryId& aDirectory, PageNumber aPage,
                          RefPtr<Promise> aPromise, ArrayAppendable& aSink);

  virtual void RemoveEntry(RefPtr<FileSystemActorHolder>& aActor,
                           const FileSystemChildMetadata& aEntry,
                           bool aRecursive, RefPtr<Promise> aPromise);

  virtual ~FileSystemRequestHandler() = default;
};  // class FileSystemRequestHandler

}  // namespace fs
}  // namespace mozilla::dom

#endif  // DOM_FS_CHILD_FILESYSTEMREQUESTHANDLER_H_
