/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OriginPrivateFileSystemParent.h"
#include "BackgroundFileSystemParent.h"
#include "datamodel/FileSystemDataManager.h"
#include "fs/FileSystemConstants.h"
#include "mozilla/dom/FileSystemTypes.h"
#include "mozilla/ipc/FileDescriptorUtils.h"
#include "mozilla/Maybe.h"

using IPCResult = mozilla::ipc::IPCResult;

namespace mozilla {
extern LazyLogModule gOPFSLog;
}

#define LOG(args) MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Verbose, args)

#define LOG_DEBUG(args) \
  MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Debug, args)

namespace mozilla::dom {

OriginPrivateFileSystemParent::OriginPrivateFileSystemParent(
    TaskQueue* aTaskQueue, fs::data::FileSystemDataManager* aData)
    : mTaskQueue(aTaskQueue), mData(aData) {}

IPCResult OriginPrivateFileSystemParent::RecvGetDirectoryHandle(
    FileSystemGetHandleRequest&& aRequest,
    GetDirectoryHandleResolver&& aResolver) {
  MOZ_ASSERT(!aRequest.handle().parentId().IsEmpty());
  MOZ_ASSERT(mData);

  auto reportError = [&aResolver](const QMResult& aRv) {
    FileSystemGetHandleResponse response(aRv.NSResult());
    aResolver(response);
  };

  QM_TRY_UNWRAP(
      fs::EntryId entryId,
      mData->GetOrCreateDirectory(aRequest.handle(), aRequest.create()),
      IPC_OK(), reportError);
  MOZ_ASSERT(!entryId.IsEmpty());

  FileSystemGetHandleResponse response(entryId);
  aResolver(response);

  return IPC_OK();
}

IPCResult OriginPrivateFileSystemParent::RecvGetFileHandle(
    FileSystemGetHandleRequest&& aRequest, GetFileHandleResolver&& aResolver) {
  MOZ_ASSERT(!aRequest.handle().parentId().IsEmpty());
  MOZ_ASSERT(mData);

  auto reportError = [&aResolver](const QMResult& aRv) {
    FileSystemGetHandleResponse response(aRv.NSResult());
    aResolver(response);
  };

  QM_TRY_UNWRAP(fs::EntryId entryId,
                mData->GetOrCreateFile(aRequest.handle(), aRequest.create()),
                IPC_OK(), reportError);
  MOZ_ASSERT(!entryId.IsEmpty());

  FileSystemGetHandleResponse response(entryId);
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
  MOZ_ASSERT(!aRequest.endpoints().parentId().IsEmpty());
  MOZ_ASSERT(!aRequest.endpoints().childId().IsEmpty());
  MOZ_ASSERT(mData);

  fs::Path filePath;
  if (aRequest.endpoints().parentId() == aRequest.endpoints().childId()) {
    FileSystemResolveResponse response(Some(FileSystemPath(filePath)));
    aResolver(response);

    return IPC_OK();
  }

  auto reportError = [&aResolver](const QMResult& aRv) {
    FileSystemResolveResponse response(aRv.NSResult());
    aResolver(response);
  };

  QM_TRY_UNWRAP(filePath, mData->Resolve(aRequest.endpoints()), IPC_OK(),
                reportError);

  if (filePath.IsEmpty()) {
    FileSystemResolveResponse response(Nothing{});
    aResolver(response);

    return IPC_OK();
  }

  FileSystemResolveResponse response(Some(FileSystemPath(filePath)));
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
  MOZ_ASSERT(!aRequest.handle().parentId().IsEmpty());
  MOZ_ASSERT(mData);

  auto reportError = [&aResolver](const QMResult& aRv) {
    FileSystemRemoveEntryResponse response(aRv.NSResult());
    aResolver(response);
  };

  QM_TRY_UNWRAP(bool isDeleted, mData->RemoveFile(aRequest.handle()), IPC_OK(),
                reportError);

  FileSystemRemoveEntryResponse ok(NS_OK);
  if (isDeleted) {
    aResolver(ok);
    return IPC_OK();
  }

  QM_TRY_UNWRAP(isDeleted,
                mData->RemoveDirectory(aRequest.handle(), aRequest.recursive()),
                IPC_OK(), reportError);

  if (!isDeleted) {
    FileSystemRemoveEntryResponse response(NS_ERROR_UNEXPECTED);
    aResolver(response);

    return IPC_OK();
  }

  aResolver(ok);
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
