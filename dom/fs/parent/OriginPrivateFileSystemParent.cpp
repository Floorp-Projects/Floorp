/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OriginPrivateFileSystemParent.h"
#include "nsNetCID.h"
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

OriginPrivateFileSystemParent::OriginPrivateFileSystemParent(
    TaskQueue* aTaskQueue, const EntryId& aRootEntry)
    : mTaskQueue(aTaskQueue), mData(), mRootEntry(aRootEntry) {}

IPCResult OriginPrivateFileSystemParent::RecvGetRootHandle(
    GetRootHandleResolver&& aResolver) {
  FileSystemGetHandleResponse response(mRootEntry);
  aResolver(response);

  return IPC_OK();
}

IPCResult OriginPrivateFileSystemParent::RecvGetDirectoryHandle(
    FileSystemGetHandleRequest&& /* aRequest */,
    GetDirectoryHandleResolver&& aResolver) {
  FileSystemGetHandleResponse response(NS_ERROR_NOT_IMPLEMENTED);
  aResolver(response);

  return IPC_OK();
}

IPCResult OriginPrivateFileSystemParent::RecvGetFileHandle(
    FileSystemGetHandleRequest&& aRequest, GetFileHandleResolver&& aResolver) {
  FileSystemGetHandleResponse response(NS_ERROR_NOT_IMPLEMENTED);
  aResolver(response);

  return IPC_OK();
}

IPCResult OriginPrivateFileSystemParent::RecvGetFile(
    FileSystemGetFileRequest&& aRequest, GetFileResolver&& aResolver) {
  FileSystemGetFileResponse response(NS_ERROR_NOT_IMPLEMENTED);
  aResolver(response);

  return IPC_OK();
}

IPCResult OriginPrivateFileSystemParent::RecvResolve(
    FileSystemResolveRequest&& aRequest, ResolveResolver&& aResolver) {
  FileSystemResolveResponse response(NS_ERROR_NOT_IMPLEMENTED);
  aResolver(response);

  return IPC_OK();
}

IPCResult OriginPrivateFileSystemParent::RecvGetEntries(
    FileSystemGetEntriesRequest&& aRequest, GetEntriesResolver&& aResolver) {
  FileSystemGetEntriesResponse response(NS_ERROR_NOT_IMPLEMENTED);
  aResolver(response);

  return IPC_OK();
}

IPCResult OriginPrivateFileSystemParent::RecvRemoveEntry(
    FileSystemRemoveEntryRequest&& aRequest, RemoveEntryResolver&& aResolver) {
  FileSystemRemoveEntryResponse response(NS_ERROR_NOT_IMPLEMENTED);
  aResolver(response);

  return IPC_OK();
}

IPCResult OriginPrivateFileSystemParent::RecvCloseFile(
    FileSystemGetFileRequest&& aRequest) {
  LOG(("Closing file"));  // painful to print out the id

  return IPC_OK();
}

IPCResult OriginPrivateFileSystemParent::RecvGetAccessHandle(
    FileSystemGetFileRequest&& aRequest, GetAccessHandleResolver&& aResolver) {
  FileSystemGetAccessHandleResponse response(NS_ERROR_NOT_IMPLEMENTED);
  aResolver(response);

  return IPC_OK();
}

IPCResult OriginPrivateFileSystemParent::RecvGetWritable(
    FileSystemGetFileRequest&& aRequest, GetWritableResolver&& aResolver) {
  FileSystemGetAccessHandleResponse response(NS_ERROR_NOT_IMPLEMENTED);
  aResolver(response);

  return IPC_OK();
}

IPCResult OriginPrivateFileSystemParent::RecvNeedQuota(
    FileSystemQuotaRequest&& aRequest, NeedQuotaResolver&& aResolver) {
  aResolver(0u);

  return IPC_OK();
}

OriginPrivateFileSystemParent::~OriginPrivateFileSystemParent() {
  LOG(("Destroying OPFS Parent %p", this));
  if (mTaskQueue) {
    mTaskQueue->BeginShutdown();
  }
}

}  // namespace mozilla::dom
