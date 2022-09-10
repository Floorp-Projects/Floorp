/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_CHILD_FILESYSTEMREQUESTHANDLER_H_
#define DOM_FS_CHILD_FILESYSTEMREQUESTHANDLER_H_

#include "mozilla/dom/FileSystemTypes.h"

template <class T>
class RefPtr;

namespace mozilla::dom {

class FileSystemHandle;
class FileSystemManager;
class Promise;

namespace fs {

class FileSystemChildMetadata;
class FileSystemEntryMetadata;
class FileSystemEntryMetadataArray;
class FileSystemEntryPair;

class FileSystemRequestHandler {
 public:
  virtual void GetRootHandle(RefPtr<FileSystemManager> aManager,
                             RefPtr<Promise> aPromise);

  virtual void GetDirectoryHandle(RefPtr<FileSystemManager>& aManager,
                                  const FileSystemChildMetadata& aDirectory,
                                  bool aCreate, RefPtr<Promise> aPromise);

  virtual void GetFileHandle(RefPtr<FileSystemManager>& aManager,
                             const FileSystemChildMetadata& aFile, bool aCreate,
                             RefPtr<Promise> aPromise);

  virtual void GetFile(RefPtr<FileSystemManager>& aManager,
                       const FileSystemEntryMetadata& aFile,
                       RefPtr<Promise> aPromise);

  virtual void GetAccessHandle(RefPtr<FileSystemManager>& aManager,
                               const FileSystemEntryMetadata& aFile,
                               const RefPtr<Promise>& aPromise);

  virtual void GetEntries(RefPtr<FileSystemManager>& aManager,
                          const EntryId& aDirectory, PageNumber aPage,
                          RefPtr<Promise> aPromise,
                          RefPtr<FileSystemEntryMetadataArray>& aSink);

  virtual void RemoveEntry(RefPtr<FileSystemManager>& aManager,
                           const FileSystemChildMetadata& aEntry,
                           bool aRecursive, RefPtr<Promise> aPromise);

  virtual void MoveEntry(RefPtr<FileSystemManager>& aManager,
                         FileSystemHandle* aHandle,
                         const FileSystemEntryMetadata& aEntry,
                         const FileSystemChildMetadata& aNewEntry,
                         RefPtr<Promise> aPromise);

  virtual void RenameEntry(RefPtr<FileSystemManager>& aManager,
                           FileSystemHandle* aHandle,
                           const FileSystemEntryMetadata& aEntry,
                           const Name& aName, RefPtr<Promise> aPromise);

  virtual void Resolve(RefPtr<FileSystemManager>& aManager,
                       const FileSystemEntryPair& aEndpoints,
                       RefPtr<Promise> aPromise);

  virtual ~FileSystemRequestHandler() = default;
};  // class FileSystemRequestHandler

}  // namespace fs
}  // namespace mozilla::dom

#endif  // DOM_FS_CHILD_FILESYSTEMREQUESTHANDLER_H_
