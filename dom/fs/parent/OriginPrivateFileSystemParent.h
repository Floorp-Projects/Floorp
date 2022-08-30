/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FS_PARENT_ORIGINPRIVATEFILESYSTEMPARENT_H_
#define DOM_FS_PARENT_ORIGINPRIVATEFILESYSTEMPARENT_H_

#include "ErrorList.h"
#include "mozilla/dom/PBackgroundFileSystemParent.h"
#include "mozilla/dom/POriginPrivateFileSystemParent.h"
#include "mozilla/TaskQueue.h"
#include "nsISupports.h"

namespace mozilla {
extern LazyLogModule gOPFSLog;
}

#define LOG(args) MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Verbose, args)

#define LOG_DEBUG(args) \
  MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Debug, args)

namespace mozilla::dom {

namespace fs::data {
// TODO: Replace dummy with real FileSystemDataManager
class FileSystemDataManagerBase {};
}  // namespace fs::data

class OriginPrivateFileSystemParent : public POriginPrivateFileSystemParent {
 public:
  OriginPrivateFileSystemParent(TaskQueue* aTaskQueue,
                                const EntryId& aRootEntry);

  mozilla::ipc::IPCResult RecvGetRootHandle(GetRootHandleResolver&& aResolver);

  mozilla::ipc::IPCResult RecvGetDirectoryHandle(
      FileSystemGetHandleRequest&& aRequest,
      GetDirectoryHandleResolver&& aResolver);

  mozilla::ipc::IPCResult RecvGetFileHandle(
      FileSystemGetHandleRequest&& aRequest, GetFileHandleResolver&& aResolver);

  mozilla::ipc::IPCResult RecvGetFile(FileSystemGetFileRequest&& aRequest,
                                      GetFileResolver&& aResolver);

  mozilla::ipc::IPCResult RecvResolve(FileSystemResolveRequest&& aRequest,
                                      ResolveResolver&& aResolver);

  mozilla::ipc::IPCResult RecvGetEntries(FileSystemGetEntriesRequest&& aRequest,
                                         GetEntriesResolver&& aResolver);

  mozilla::ipc::IPCResult RecvRemoveEntry(
      FileSystemRemoveEntryRequest&& aRequest, RemoveEntryResolver&& aResolver);

  mozilla::ipc::IPCResult RecvCloseFile(FileSystemGetFileRequest&& aRequest);

  mozilla::ipc::IPCResult RecvGetAccessHandle(
      FileSystemGetFileRequest&& aRequest, GetAccessHandleResolver&& aResolver);

  mozilla::ipc::IPCResult RecvGetWritable(FileSystemGetFileRequest&& aRequest,
                                          GetWritableResolver&& aResolver);

  mozilla::ipc::IPCResult RecvNeedQuota(FileSystemQuotaRequest&& aRequest,
                                        NeedQuotaResolver&& aResolver);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(OriginPrivateFileSystemParent)

 protected:
  virtual ~OriginPrivateFileSystemParent();

 private:
  RefPtr<TaskQueue> mTaskQueue;

  UniquePtr<fs::data::FileSystemDataManagerBase> mData;

  const EntryId mRootEntry;
};

}  // namespace mozilla::dom

#endif  // DOM_FS_PARENT_ORIGINPRIVATEFILESYSTEMPARENT_H_
