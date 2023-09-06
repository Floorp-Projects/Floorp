/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_FILESYSTEMMANAGERPARENT_H_
#define DOM_FS_PARENT_FILESYSTEMMANAGERPARENT_H_

#include "ErrorList.h"
#include "mozilla/dom/FlippedOnce.h"
#include "mozilla/dom/PFileSystemManagerParent.h"
#include "mozilla/dom/quota/DebugOnlyMacro.h"
#include "nsISupports.h"

namespace mozilla::dom {

namespace fs::data {
class FileSystemDataManager;
}  // namespace fs::data

class FileSystemManagerParent : public PFileSystemManagerParent {
 public:
  FileSystemManagerParent(RefPtr<fs::data::FileSystemDataManager> aDataManager,
                          const EntryId& aRootEntry);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FileSystemManagerParent, override)

  void AssertIsOnIOTarget() const;

#ifdef DEBUG
  void SetRegistered(bool aRegistered) { mRegistered = aRegistered; }
#endif

  // Safe to call while the actor is live.
  const RefPtr<fs::data::FileSystemDataManager>& DataManagerStrongRef() const;

  mozilla::ipc::IPCResult RecvGetRootHandle(GetRootHandleResolver&& aResolver);

  mozilla::ipc::IPCResult RecvGetDirectoryHandle(
      FileSystemGetHandleRequest&& aRequest,
      GetDirectoryHandleResolver&& aResolver);

  mozilla::ipc::IPCResult RecvGetFileHandle(
      FileSystemGetHandleRequest&& aRequest, GetFileHandleResolver&& aResolver);

  mozilla::ipc::IPCResult RecvGetAccessHandle(
      FileSystemGetAccessHandleRequest&& aRequest,
      GetAccessHandleResolver&& aResolver);

  mozilla::ipc::IPCResult RecvGetWritable(
      FileSystemGetWritableRequest&& aRequest, GetWritableResolver&& aResolver);

  mozilla::ipc::IPCResult RecvGetFile(FileSystemGetFileRequest&& aRequest,
                                      GetFileResolver&& aResolver);

  mozilla::ipc::IPCResult RecvResolve(FileSystemResolveRequest&& aRequest,
                                      ResolveResolver&& aResolver);

  mozilla::ipc::IPCResult RecvGetEntries(FileSystemGetEntriesRequest&& aRequest,
                                         GetEntriesResolver&& aResolver);

  mozilla::ipc::IPCResult RecvRemoveEntry(
      FileSystemRemoveEntryRequest&& aRequest, RemoveEntryResolver&& aResolver);

  mozilla::ipc::IPCResult RecvMoveEntry(FileSystemMoveEntryRequest&& aRequest,
                                        MoveEntryResolver&& aResolver);

  mozilla::ipc::IPCResult RecvRenameEntry(
      FileSystemRenameEntryRequest&& aRequest, MoveEntryResolver&& aResolver);

  void RequestAllowToClose();

  void ActorDestroy(ActorDestroyReason aWhy) override;

 protected:
  virtual ~FileSystemManagerParent();

 private:
  RefPtr<fs::data::FileSystemDataManager> mDataManager;

  FileSystemGetHandleResponse mRootResponse;

  FlippedOnce<false> mRequestedAllowToClose;

  DEBUGONLY(bool mRegistered = false);

  DEBUGONLY(bool mActorDestroyed = false);
};

}  // namespace mozilla::dom

#endif  // DOM_FS_PARENT_FILESYSTEMMANAGERPARENT_H_
