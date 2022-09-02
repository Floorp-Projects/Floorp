/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemManagerParent.h"

#include "FileSystemDatabaseManager.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/FileSystemDataManager.h"
#include "mozilla/dom/FileSystemTypes.h"
#include "mozilla/dom/QMResult.h"
#include "mozilla/dom/quota/ForwardDecls.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "nsString.h"
#include "nsTArray.h"

using IPCResult = mozilla::ipc::IPCResult;

namespace mozilla {
extern LazyLogModule gOPFSLog;
}

#define LOG(args) MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Verbose, args)

#define LOG_DEBUG(args) \
  MOZ_LOG(mozilla::gOPFSLog, mozilla::LogLevel::Debug, args)

namespace mozilla::dom {

FileSystemManagerParent::FileSystemManagerParent(
    RefPtr<fs::data::FileSystemDataManager> aDataManager,
    const EntryId& aRootEntry)
    : mDataManager(std::move(aDataManager)), mRootEntry(aRootEntry) {}

FileSystemManagerParent::~FileSystemManagerParent() {
  LOG(("Destroying FileSystemManagerParent %p", this));
}

IPCResult FileSystemManagerParent::RecvGetRootHandleMsg(
    GetRootHandleMsgResolver&& aResolver) {
  FileSystemGetHandleResponse response(mRootEntry);
  aResolver(response);

  return IPC_OK();
}

IPCResult FileSystemManagerParent::RecvGetDirectoryHandleMsg(
    FileSystemGetHandleRequest&& aRequest,
    GetDirectoryHandleMsgResolver&& aResolver) {
  MOZ_ASSERT(!aRequest.handle().parentId().IsEmpty());
  MOZ_ASSERT(mDataManager);

  auto reportError = [&aResolver](const QMResult& aRv) {
    FileSystemGetHandleResponse response(ToNSResult(aRv));
    aResolver(response);
  };

  QM_TRY_UNWRAP(fs::EntryId entryId,
                mDataManager->MutableDatabaseManagerPtr()->GetOrCreateDirectory(
                    aRequest.handle(), aRequest.create()),
                IPC_OK(), reportError);
  MOZ_ASSERT(!entryId.IsEmpty());

  FileSystemGetHandleResponse response(entryId);
  aResolver(response);

  return IPC_OK();
}

IPCResult FileSystemManagerParent::RecvGetFileHandleMsg(
    FileSystemGetHandleRequest&& aRequest,
    GetFileHandleMsgResolver&& aResolver) {
  MOZ_ASSERT(!aRequest.handle().parentId().IsEmpty());
  MOZ_ASSERT(mDataManager);

  auto reportError = [&aResolver](const QMResult& aRv) {
    FileSystemGetHandleResponse response(ToNSResult(aRv));
    aResolver(response);
  };

  QM_TRY_UNWRAP(fs::EntryId entryId,
                mDataManager->MutableDatabaseManagerPtr()->GetOrCreateFile(
                    aRequest.handle(), aRequest.create()),
                IPC_OK(), reportError);
  MOZ_ASSERT(!entryId.IsEmpty());

  FileSystemGetHandleResponse response(entryId);
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
  MOZ_ASSERT(!aRequest.endpoints().parentId().IsEmpty());
  MOZ_ASSERT(!aRequest.endpoints().childId().IsEmpty());
  MOZ_ASSERT(mDataManager);

  fs::Path filePath;
  if (aRequest.endpoints().parentId() == aRequest.endpoints().childId()) {
    FileSystemResolveResponse response(Some(FileSystemPath(filePath)));
    aResolver(response);

    return IPC_OK();
  }

  auto reportError = [&aResolver](const QMResult& aRv) {
    FileSystemResolveResponse response(ToNSResult(aRv));
    aResolver(response);
  };

  QM_TRY_UNWRAP(
      filePath,
      mDataManager->MutableDatabaseManagerPtr()->Resolve(aRequest.endpoints()),
      IPC_OK(), reportError);

  if (filePath.IsEmpty()) {
    FileSystemResolveResponse response(Nothing{});
    aResolver(response);

    return IPC_OK();
  }

  FileSystemResolveResponse response(Some(FileSystemPath(filePath)));
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
  MOZ_ASSERT(!aRequest.handle().parentId().IsEmpty());
  MOZ_ASSERT(mDataManager);

  auto reportError = [&aResolver](const QMResult& aRv) {
    FileSystemRemoveEntryResponse response(ToNSResult(aRv));
    aResolver(response);
  };

  QM_TRY_UNWRAP(
      bool isDeleted,
      mDataManager->MutableDatabaseManagerPtr()->RemoveFile(aRequest.handle()),
      IPC_OK(), reportError);

  if (isDeleted) {
    FileSystemRemoveEntryResponse response(NS_OK);
    aResolver(response);

    return IPC_OK();
  }

  QM_TRY_UNWRAP(isDeleted,
                mDataManager->MutableDatabaseManagerPtr()->RemoveDirectory(
                    aRequest.handle(), aRequest.recursive()),
                IPC_OK(), reportError);

  if (!isDeleted) {
    FileSystemRemoveEntryResponse response(NS_ERROR_UNEXPECTED);
    aResolver(response);

    return IPC_OK();
  }

  FileSystemRemoveEntryResponse response(NS_OK);
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

void FileSystemManagerParent::RequestAllowToClose() {
  if (mRequestedAllowToClose) {
    return;
  }

  mRequestedAllowToClose.Flip();

  // XXX Send a message to the child and wait for a response before closing!

  Close();
}

void FileSystemManagerParent::OnChannelClose() {
  if (!mAllowedToClose) {
    AllowToClose();
  }

  PFileSystemManagerParent::OnChannelClose();
}

void FileSystemManagerParent::OnChannelError() {
  if (!mAllowedToClose) {
    AllowToClose();
  }

  PFileSystemManagerParent::OnChannelError();
}

void FileSystemManagerParent::AllowToClose() {
  mAllowedToClose.Flip();

  InvokeAsync(mDataManager->MutableBackgroundTargetPtr(), __func__,
              [self = RefPtr<FileSystemManagerParent>(this)]() {
                self->mDataManager->UnregisterActor(WrapNotNull(self));

                self->mDataManager = nullptr;

                return BoolPromise::CreateAndResolve(true, __func__);
              });
}

}  // namespace mozilla::dom
