/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemManagerParent.h"

#include "FileSystemDatabaseManager.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/FileBlobImpl.h"
#include "mozilla/dom/FileSystemAccessHandleParent.h"
#include "mozilla/dom/FileSystemDataManager.h"
#include "mozilla/dom/FileSystemTypes.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/dom/QMResult.h"
#include "mozilla/dom/quota/ForwardDecls.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/FileDescriptorUtils.h"
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

void FileSystemManagerParent::AssertIsOnIOTarget() const {
  MOZ_ASSERT(mDataManager);

  mDataManager->AssertIsOnIOTarget();
}

const RefPtr<fs::data::FileSystemDataManager>&
FileSystemManagerParent::DataManagerStrongRef() const {
  return mDataManager;
}

IPCResult FileSystemManagerParent::RecvGetRootHandle(
    GetRootHandleResolver&& aResolver) {
  AssertIsOnIOTarget();

  FileSystemGetHandleResponse response(mRootEntry);
  aResolver(response);

  return IPC_OK();
}

IPCResult FileSystemManagerParent::RecvGetDirectoryHandle(
    FileSystemGetHandleRequest&& aRequest,
    GetDirectoryHandleResolver&& aResolver) {
  LOG(("GetDirectoryHandle %s ",
       NS_ConvertUTF16toUTF8(aRequest.handle().childName()).get()));
  AssertIsOnIOTarget();
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

IPCResult FileSystemManagerParent::RecvGetFileHandle(
    FileSystemGetHandleRequest&& aRequest, GetFileHandleResolver&& aResolver) {
  AssertIsOnIOTarget();
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

mozilla::ipc::IPCResult FileSystemManagerParent::RecvGetAccessHandle(
    const FileSystemGetAccessHandleRequest& aRequest,
    GetAccessHandleResolver&& aResolver) {
  AssertIsOnIOTarget();
  MOZ_ASSERT(mDataManager);

  if (!mDataManager->LockExclusive(aRequest.entryId())) {
    aResolver(NS_ERROR_FAILURE);
    return IPC_OK();
  }

  auto autoUnlock =
      MakeScopeExit([self = RefPtr<FileSystemManagerParent>(this), aRequest] {
        self->mDataManager->UnlockExclusive(aRequest.entryId());
      });

  auto reportError = [aResolver](nsresult rv) { aResolver(rv); };

  nsString type;
  fs::TimeStamp lastModifiedMilliSeconds;
  fs::Path path;
  nsCOMPtr<nsIFile> file;
  QM_TRY(MOZ_TO_RESULT(mDataManager->MutableDatabaseManagerPtr()->GetFile(
             {mRootEntry, aRequest.entryId()}, type,
             lastModifiedMilliSeconds, path, file)),
         IPC_OK(), reportError);

  if (MOZ_LOG_TEST(gOPFSLog, mozilla::LogLevel::Debug)) {
    nsAutoString path;
    if (NS_SUCCEEDED(file->GetPath(path))) {
      LOG(("Opening %s", NS_ConvertUTF16toUTF8(path).get()));
    }
  }

  FILE* fileHandle;
  QM_TRY(MOZ_TO_RESULT(file->OpenANSIFileDesc("r+", &fileHandle)), IPC_OK(),
         reportError);

  LOG(("Opened"));

  FileDescriptor fileDescriptor =
      mozilla::ipc::FILEToFileDescriptor(fileHandle);

  auto accessHandleParent =
      MakeRefPtr<FileSystemAccessHandleParent>(this, aRequest.entryId());

  autoUnlock.release();

  if (!SendPFileSystemAccessHandleConstructor(accessHandleParent,
                                              fileDescriptor)) {
    aResolver(NS_ERROR_FAILURE);
    return IPC_OK();
  }

  aResolver(FileSystemGetAccessHandleResponse(accessHandleParent));
  return IPC_OK();
}

IPCResult FileSystemManagerParent::RecvGetFile(
    FileSystemGetFileRequest&& aRequest, GetFileResolver&& aResolver) {
  AssertIsOnIOTarget();

  // XXX Spec https://www.w3.org/TR/FileAPI/#dfn-file wants us to snapshot the
  // state of the file at getFile() time

  // You can create a File with getFile() even if the file is locked
  // XXX factor out this part of the code for accesshandle/ and getfile
  auto reportError = [aResolver](nsresult rv) {
    LOG(("getFile() Failed!"));
    aResolver(rv);
  };

  nsString type;
  fs::TimeStamp lastModifiedMilliSeconds;
  fs::Path path;
  nsCOMPtr<nsIFile> fileObject;
  QM_TRY(MOZ_TO_RESULT(mDataManager->MutableDatabaseManagerPtr()->GetFile(
             {mRootEntry, aRequest.entryId()}, type,
             lastModifiedMilliSeconds, path, fileObject)),
         IPC_OK(), reportError);

  if (MOZ_LOG_TEST(gOPFSLog, mozilla::LogLevel::Debug)) {
    nsAutoString path;
    if (NS_SUCCEEDED(fileObject->GetPath(path))) {
      LOG(("Opening %s", NS_ConvertUTF16toUTF8(path).get()));
    }
  }

  RefPtr<BlobImpl> blob = MakeRefPtr<FileBlobImpl>(fileObject);

  IPCBlob ipcBlob;
  QM_TRY(MOZ_TO_RESULT(IPCBlobUtils::Serialize(blob, ipcBlob)), IPC_OK(),
         reportError);

  aResolver(
      FileSystemFileProperties(lastModifiedMilliSeconds, ipcBlob, type, path));
  return IPC_OK();
}

IPCResult FileSystemManagerParent::RecvResolve(
    FileSystemResolveRequest&& aRequest, ResolveResolver&& aResolver) {
  AssertIsOnIOTarget();
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

IPCResult FileSystemManagerParent::RecvGetEntries(
    FileSystemGetEntriesRequest&& aRequest, GetEntriesResolver&& aResolver) {
  AssertIsOnIOTarget();
  MOZ_ASSERT(!aRequest.parentId().IsEmpty());
  MOZ_ASSERT(mDataManager);

  auto reportError = [&aResolver](const QMResult& aRv) {
    FileSystemGetEntriesResponse response(ToNSResult(aRv));
    aResolver(response);
  };

  QM_TRY_UNWRAP(FileSystemDirectoryListing entries,
                mDataManager->MutableDatabaseManagerPtr()->GetDirectoryEntries(
                    aRequest.parentId(), aRequest.page()),
                IPC_OK(), reportError);

  FileSystemGetEntriesResponse response(entries);
  aResolver(response);

  return IPC_OK();
}

IPCResult FileSystemManagerParent::RecvRemoveEntry(
    FileSystemRemoveEntryRequest&& aRequest, RemoveEntryResolver&& aResolver) {
  LOG(("RemoveEntry %s",
       NS_ConvertUTF16toUTF8(aRequest.handle().childName()).get()));
  AssertIsOnIOTarget();
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

IPCResult FileSystemManagerParent::RecvMoveEntry(
    FileSystemMoveEntryRequest&& aRequest, MoveEntryResolver&& aResolver) {
  LOG(("MoveEntry %s to %s",
       NS_ConvertUTF16toUTF8(aRequest.handle().entryName()).get(),
       NS_ConvertUTF16toUTF8(aRequest.destHandle().childName()).get()));
  MOZ_ASSERT(!aRequest.handle().entryId().IsEmpty());
  MOZ_ASSERT(!aRequest.destHandle().parentId().IsEmpty());
  MOZ_ASSERT(mDataManager);

  auto reportError = [&aResolver](const QMResult& aRv) {
    FileSystemMoveEntryResponse response(ToNSResult(aRv));
    aResolver(response);
  };

  QM_TRY_UNWRAP(EntryId parentId,
                mDataManager->MutableDatabaseManagerPtr()->GetParentEntryId(
                    aRequest.handle().entryId()),
                IPC_OK(), reportError);
  FileSystemChildMetadata sourceHandle;
  sourceHandle.parentId() = parentId;
  sourceHandle.childName() = aRequest.handle().entryName();

  QM_TRY_UNWRAP(bool moved,
                mDataManager->MutableDatabaseManagerPtr()->MoveEntry(
                    sourceHandle, aRequest.destHandle()),
                IPC_OK(), reportError);

  fs::FileSystemMoveEntryResponse response(moved ? NS_OK : NS_ERROR_FAILURE);
  aResolver(response);
  return IPC_OK();
}

IPCResult FileSystemManagerParent::RecvRenameEntry(
    FileSystemRenameEntryRequest&& aRequest, MoveEntryResolver&& aResolver) {
  // if destHandle's parentId is empty, then we're renaming in the same
  // directory
  LOG(("RenameEntry %s to %s",
       NS_ConvertUTF16toUTF8(aRequest.handle().entryName()).get(),
       NS_ConvertUTF16toUTF8(aRequest.name()).get()));
  MOZ_ASSERT(!aRequest.handle().entryId().IsEmpty());
  MOZ_ASSERT(mDataManager);

  auto reportError = [&aResolver](const QMResult& aRv) {
    FileSystemMoveEntryResponse response(ToNSResult(aRv));
    aResolver(response);
  };

  QM_TRY_UNWRAP(EntryId parentId,
                mDataManager->MutableDatabaseManagerPtr()->GetParentEntryId(
                    aRequest.handle().entryId()),
                IPC_OK(), reportError);
  FileSystemChildMetadata sourceHandle;
  sourceHandle.parentId() = parentId;
  sourceHandle.childName() = aRequest.handle().entryName();

  FileSystemChildMetadata newHandle;
  newHandle.parentId() = parentId;
  newHandle.childName() = aRequest.name();

  QM_TRY_UNWRAP(bool moved,
                mDataManager->MutableDatabaseManagerPtr()->MoveEntry(
                    sourceHandle, newHandle),
                IPC_OK(), reportError);

  fs::FileSystemMoveEntryResponse response(moved ? NS_OK : NS_ERROR_FAILURE);
  aResolver(response);
  return IPC_OK();
}

IPCResult FileSystemManagerParent::RecvGetWritable(
    FileSystemGetFileRequest&& aRequest, GetWritableResolver&& aResolver) {
  AssertIsOnIOTarget();

  FileSystemGetAccessHandleResponse response(NS_ERROR_NOT_IMPLEMENTED);
  aResolver(response);

  return IPC_OK();
}

IPCResult FileSystemManagerParent::RecvNeedQuota(
    FileSystemQuotaRequest&& aRequest, NeedQuotaResolver&& aResolver) {
  AssertIsOnIOTarget();

  aResolver(0u);

  return IPC_OK();
}

void FileSystemManagerParent::RequestAllowToClose() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  if (mRequestedAllowToClose) {
    return;
  }

  mRequestedAllowToClose.Flip();

  InvokeAsync(mDataManager->MutableIOTargetPtr(), __func__,
              [self = RefPtr<FileSystemManagerParent>(this)]() {
                return self->SendCloseAll();
              })
      ->Then(mDataManager->MutableIOTargetPtr(), __func__,
             [self = RefPtr<FileSystemManagerParent>(this)](
                 const CloseAllPromise::ResolveOrRejectValue& aValue) {
               self->Close();

               return BoolPromise::CreateAndResolve(true, __func__);
             });
}

void FileSystemManagerParent::OnChannelClose() {
  AssertIsOnIOTarget();

  if (!mAllowedToClose) {
    AllowToClose();
  }

  PFileSystemManagerParent::OnChannelClose();
}

void FileSystemManagerParent::OnChannelError() {
  AssertIsOnIOTarget();

  if (!mAllowedToClose) {
    AllowToClose();
  }

  PFileSystemManagerParent::OnChannelError();
}

void FileSystemManagerParent::AllowToClose() {
  AssertIsOnIOTarget();

  mAllowedToClose.Flip();

  InvokeAsync(mDataManager->MutableBackgroundTargetPtr(), __func__,
              [self = RefPtr<FileSystemManagerParent>(this)]() {
                self->mDataManager->UnregisterActor(WrapNotNull(self));

                self->mDataManager = nullptr;

                return BoolPromise::CreateAndResolve(true, __func__);
              });
}

}  // namespace mozilla::dom
