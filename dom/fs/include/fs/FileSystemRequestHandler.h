/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_CHILD_FILESYSTEMREQUESTHANDLER_H_
#define DOM_FS_CHILD_FILESYSTEMREQUESTHANDLER_H_

#include "nsStringFwd.h"
#include "mozilla/dom/FileSystemTypes.h"
#include "mozilla/dom/FileSystemHandle.h"

template <class T>
class RefPtr;

namespace mozilla::dom {

// TODO: Replace this dummy class with real implementation
class FileSystemActorHolder {
  NS_INLINE_DECL_REFCOUNTING(FileSystemActorHolder)
 protected:
  virtual ~FileSystemActorHolder() = default;
};

class FileSystemHandle;
class Promise;
class OriginPrivateFileSystemChild;
}  // namespace mozilla::dom

namespace mozilla::dom::fs {

class FileSystemChildMetadata;
class FileSystemEntryMetadata;

class ArrayAppendable {};

class FileSystemRequestHandler {
 public:
  virtual void GetRoot(const Origin& aOrigin, RefPtr<Promise> aPromise);

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

}  // namespace mozilla::dom::fs

#endif  // DOM_FS_CHILD_FILESYSTEMREQUESTHANDLER_H_
