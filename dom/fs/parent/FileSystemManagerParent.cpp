/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemManagerParent.h"
#include "nsNetCID.h"
#include "mozilla/dom/FileSystemDataManager.h"
#include "mozilla/dom/FileSystemTypes.h"
#include "mozilla/ipc/Endpoint.h"

using IPCResult = mozilla::ipc::IPCResult;

namespace mozilla {
extern LazyLogModule gOPFSLog;
}

#define LOG(args) MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Verbose, args)

#define LOG_DEBUG(args) \
  MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Debug, args)

namespace mozilla::dom {

FileSystemManagerParent::FileSystemManagerParent(
    TaskQueue* aTaskQueue, RefPtr<fs::data::FileSystemDataManager> aDataManager,
    const EntryId& aRootEntry)
    : mTaskQueue(aTaskQueue),
      mDataManager(std::move(aDataManager)),
      mRootEntry(aRootEntry) {}

IPCResult FileSystemManagerParent::RecvGetRootHandleMsg(
    GetRootHandleMsgResolver&& aResolver) {
  FileSystemGetHandleResponse response(mRootEntry);
  aResolver(response);

  return IPC_OK();
}

IPCResult FileSystemManagerParent::RecvGetDirectoryHandleMsg(
    FileSystemGetHandleRequest&& /* aRequest */,
    GetDirectoryHandleMsgResolver&& aResolver) {
  FileSystemGetHandleResponse response(NS_ERROR_NOT_IMPLEMENTED);
  aResolver(response);

  return IPC_OK();
}

IPCResult FileSystemManagerParent::RecvGetFileHandleMsg(
    FileSystemGetHandleRequest&& aRequest,
    GetFileHandleMsgResolver&& aResolver) {
  FileSystemGetHandleResponse response(NS_ERROR_NOT_IMPLEMENTED);
  aResolver(response);

  return IPC_OK();
}

IPCResult FileSystemManagerParent::RecvGetFileMsg(
    FileSystemGetFileRequest&& aRequest, GetFileMsgResolver&& aResolver) {
  FileSystemGetFileResponse response(NS_ERROR_NOT_IMPLEMENTED);
  aResolver(response);

  return IPC_OK();
}

IPCResult FileSystemManagerParent::RecvResolveMsg(
    FileSystemResolveRequest&& aRequest, ResolveMsgResolver&& aResolver) {
  FileSystemResolveResponse response(NS_ERROR_NOT_IMPLEMENTED);
  aResolver(response);

  return IPC_OK();
}

IPCResult FileSystemManagerParent::RecvGetEntriesMsg(
    FileSystemGetEntriesRequest&& aRequest, GetEntriesMsgResolver&& aResolver) {
  FileSystemGetEntriesResponse response(NS_ERROR_NOT_IMPLEMENTED);
  aResolver(response);

  return IPC_OK();
}

IPCResult FileSystemManagerParent::RecvRemoveEntryMsg(
    FileSystemRemoveEntryRequest&& aRequest,
    RemoveEntryMsgResolver&& aResolver) {
  FileSystemRemoveEntryResponse response(NS_ERROR_NOT_IMPLEMENTED);
  aResolver(response);

  return IPC_OK();
}

IPCResult FileSystemManagerParent::RecvCloseFile(
    FileSystemGetFileRequest&& aRequest) {
  LOG(("Closing file"));  // painful to print out the id

  return IPC_OK();
}

IPCResult FileSystemManagerParent::RecvGetAccessHandle(
    FileSystemGetFileRequest&& aRequest, GetAccessHandleResolver&& aResolver) {
  FileSystemGetAccessHandleResponse response(NS_ERROR_NOT_IMPLEMENTED);
  aResolver(response);

  return IPC_OK();
}

IPCResult FileSystemManagerParent::RecvGetWritable(
    FileSystemGetFileRequest&& aRequest, GetWritableResolver&& aResolver) {
  FileSystemGetAccessHandleResponse response(NS_ERROR_NOT_IMPLEMENTED);
  aResolver(response);

  return IPC_OK();
}

IPCResult FileSystemManagerParent::RecvNeedQuota(
    FileSystemQuotaRequest&& aRequest, NeedQuotaResolver&& aResolver) {
  aResolver(0u);

  return IPC_OK();
}

FileSystemManagerParent::~FileSystemManagerParent() {
  LOG(("Destroying FileSystemManagerParent %p", this));
  if (mTaskQueue) {
    mTaskQueue->BeginShutdown();
  }

  nsCOMPtr<nsISerialEventTarget> target =
      mDataManager->MutableBackgroundTargetPtr();

  NS_ProxyRelease("ReleaseFileSystemDataManager", target,
                  mDataManager.forget());
}

}  // namespace mozilla::dom
