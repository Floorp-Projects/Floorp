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
#include "nsISupports.h"

namespace mozilla {
extern LazyLogModule gOPFSLog;
}

#define LOG(args) MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Verbose, args)

#define LOG_DEBUG(args) \
  MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Debug, args)

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

  const RefPtr<fs::data::FileSystemDataManager>& DataManagerStrongRef() const;

  mozilla::ipc::IPCResult RecvGetRootHandle(GetRootHandleResolver&& aResolver);

  mozilla::ipc::IPCResult RecvGetDirectoryHandle(
      FileSystemGetHandleRequest&& aRequest,
      GetDirectoryHandleResolver&& aResolver);

  mozilla::ipc::IPCResult RecvGetFileHandle(
      FileSystemGetHandleRequest&& aRequest, GetFileHandleResolver&& aResolver);

  mozilla::ipc::IPCResult RecvGetAccessHandle(
      const FileSystemGetAccessHandleRequest& aRequest,
      GetAccessHandleResolver&& aResolver);

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

  mozilla::ipc::IPCResult RecvGetWritable(FileSystemGetFileRequest&& aRequest,
                                          GetWritableResolver&& aResolver);

  mozilla::ipc::IPCResult RecvNeedQuota(FileSystemQuotaRequest&& aRequest,
                                        NeedQuotaResolver&& aResolver);

  void RequestAllowToClose();

  void OnChannelClose() override;

  void OnChannelError() override;

 protected:
  virtual ~FileSystemManagerParent();

  void AllowToClose();

 private:
  RefPtr<fs::data::FileSystemDataManager> mDataManager;

  const EntryId mRootEntry;

  FlippedOnce<false> mRequestedAllowToClose;
  FlippedOnce<false> mAllowedToClose;
};

}  // namespace mozilla::dom

#endif  // DOM_FS_PARENT_FILESYSTEMMANAGERPARENT_H_
