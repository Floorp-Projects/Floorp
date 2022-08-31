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

  mozilla::ipc::IPCResult RecvGetRootHandleMsg(
      GetRootHandleMsgResolver&& aResolver);

  mozilla::ipc::IPCResult RecvGetDirectoryHandleMsg(
      FileSystemGetHandleRequest&& aRequest,
      GetDirectoryHandleMsgResolver&& aResolver);

  mozilla::ipc::IPCResult RecvGetFileHandleMsg(
      FileSystemGetHandleRequest&& aRequest,
      GetFileHandleMsgResolver&& aResolver);

  mozilla::ipc::IPCResult RecvGetFileMsg(FileSystemGetFileRequest&& aRequest,
                                         GetFileMsgResolver&& aResolver);

  mozilla::ipc::IPCResult RecvResolveMsg(FileSystemResolveRequest&& aRequest,
                                         ResolveMsgResolver&& aResolver);

  mozilla::ipc::IPCResult RecvGetEntriesMsg(
      FileSystemGetEntriesRequest&& aRequest,
      GetEntriesMsgResolver&& aResolver);

  mozilla::ipc::IPCResult RecvRemoveEntryMsg(
      FileSystemRemoveEntryRequest&& aRequest,
      RemoveEntryMsgResolver&& aResolver);

  mozilla::ipc::IPCResult RecvCloseFile(FileSystemGetFileRequest&& aRequest);

  mozilla::ipc::IPCResult RecvGetAccessHandle(
      FileSystemGetFileRequest&& aRequest, GetAccessHandleResolver&& aResolver);

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
