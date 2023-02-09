/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_FILESYSTEMWRITABLEFILESTREAM_H_
#define DOM_FS_PARENT_FILESYSTEMWRITABLEFILESTREAM_H_

#include "mozilla/dom/FileSystemTypes.h"
#include "mozilla/dom/FlippedOnce.h"
#include "mozilla/dom/PFileSystemWritableFileStreamParent.h"

class nsIInterfaceRequestor;

namespace mozilla::dom {

class FileSystemManagerParent;

class FileSystemWritableFileStreamParent
    : public PFileSystemWritableFileStreamParent {
 public:
  FileSystemWritableFileStreamParent(RefPtr<FileSystemManagerParent> aManager,
                                     const fs::EntryId& aEntryId);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FileSystemWritableFileStreamParent,
                                        override)

  mozilla::ipc::IPCResult RecvClose(CloseResolver&& aResolver);

  void ActorDestroy(ActorDestroyReason aWhy) override;

  nsIInterfaceRequestor* GetOrCreateStreamCallbacks();

 private:
  class FileSystemWritableFileStreamCallbacks;

  virtual ~FileSystemWritableFileStreamParent();

  bool IsClosed() const { return mClosed; }

  void Close();

  const RefPtr<FileSystemManagerParent> mManager;

  RefPtr<FileSystemWritableFileStreamCallbacks> mStreamCallbacks;

  const fs::EntryId mEntryId;

  FlippedOnce<false> mClosed;
};

}  // namespace mozilla::dom

#endif  // DOM_FS_PARENT_FILESYSTEMWRITABLEFILESTREAM_H_
