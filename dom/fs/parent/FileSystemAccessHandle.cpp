/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemAccessHandle.h"

#include "FileSystemDatabaseManager.h"
#include "mozilla/Result.h"
#include "mozilla/dom/FileSystemDataManager.h"
#include "mozilla/dom/FileSystemHelpers.h"
#include "mozilla/dom/FileSystemLog.h"
#include "mozilla/dom/quota/FileStreams.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/RemoteQuotaObjectParent.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/ipc/RandomAccessStreamParams.h"
#include "mozilla/ipc/RandomAccessStreamUtils.h"
#include "nsIFileStreams.h"

namespace mozilla::dom {

FileSystemAccessHandle::FileSystemAccessHandle(
    RefPtr<fs::data::FileSystemDataManager> aDataManager,
    const fs::EntryId& aEntryId)
    : mEntryId(aEntryId),
      mDataManager(std::move(aDataManager)),
      mActor(nullptr),
      mControlActor(nullptr),
      mRegCount(0),
      mLocked(false),
      mRegistered(false),
      mClosed(false) {}

FileSystemAccessHandle::~FileSystemAccessHandle() {
  MOZ_DIAGNOSTIC_ASSERT(mClosed);
}

// static
RefPtr<FileSystemAccessHandle::CreatePromise> FileSystemAccessHandle::Create(
    RefPtr<fs::data::FileSystemDataManager> aDataManager,
    const fs::EntryId& aEntryId) {
  MOZ_ASSERT(aDataManager);
  aDataManager->AssertIsOnIOTarget();

  RefPtr<FileSystemAccessHandle> accessHandle =
      new FileSystemAccessHandle(std::move(aDataManager), aEntryId);

  return accessHandle->BeginInit()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [accessHandle = fs::Registered<FileSystemAccessHandle>(accessHandle)](
          InitPromise::ResolveOrRejectValue&& value) mutable {
        if (value.IsReject()) {
          return CreatePromise::CreateAndReject(value.RejectValue(), __func__);
        }

        mozilla::ipc::RandomAccessStreamParams streamParams =
            std::move(value.ResolveValue());

        return CreatePromise::CreateAndResolve(
            std::pair(std::move(accessHandle), std::move(streamParams)),
            __func__);
      });
}

NS_IMPL_ISUPPORTS_INHERITED0(FileSystemAccessHandle, FileSystemStreamCallbacks)

void FileSystemAccessHandle::Register() { ++mRegCount; }

void FileSystemAccessHandle::Unregister() {
  MOZ_ASSERT(mRegCount > 0);

  --mRegCount;

  if (IsInactive() && IsOpen()) {
    BeginClose();
  }
}

void FileSystemAccessHandle::RegisterActor(
    NotNull<FileSystemAccessHandleParent*> aActor) {
  MOZ_ASSERT(!mActor);

  mActor = aActor;
}

void FileSystemAccessHandle::UnregisterActor(
    NotNull<FileSystemAccessHandleParent*> aActor) {
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mActor == aActor);

  mActor = nullptr;

  if (IsInactive() && IsOpen()) {
    BeginClose();
  }
}

void FileSystemAccessHandle::RegisterControlActor(
    NotNull<FileSystemAccessHandleControlParent*> aControlActor) {
  MOZ_ASSERT(!mControlActor);

  mControlActor = aControlActor;
}

void FileSystemAccessHandle::UnregisterControlActor(
    NotNull<FileSystemAccessHandleControlParent*> aControlActor) {
  MOZ_ASSERT(mControlActor);
  MOZ_ASSERT(mControlActor == aControlActor);

  mControlActor = nullptr;

  if (IsInactive() && IsOpen()) {
    BeginClose();
  }
}

bool FileSystemAccessHandle::IsOpen() const { return !mClosed; }

RefPtr<BoolPromise> FileSystemAccessHandle::BeginClose() {
  MOZ_ASSERT(IsOpen());

  LOG(("Closing AccessHandle"));

  mClosed = true;

  if (mRemoteQuotaObjectParent) {
    mRemoteQuotaObjectParent->Close();
  }

  if (mLocked) {
    mDataManager->UnlockExclusive(mEntryId);
  }

  return InvokeAsync(
      mDataManager->MutableBackgroundTargetPtr(), __func__,
      [self = RefPtr(this)]() {
        if (self->mRegistered) {
          self->mDataManager->UnregisterAccessHandle(WrapNotNull(self));
        }

        self->mDataManager = nullptr;

        return BoolPromise::CreateAndResolve(true, __func__);
      });
}

bool FileSystemAccessHandle::IsInactive() const {
  return !mRegCount && !mActor && !mControlActor;
}

RefPtr<FileSystemAccessHandle::InitPromise>
FileSystemAccessHandle::BeginInit() {
  if (!mDataManager->LockExclusive(mEntryId)) {
    return InitPromise::CreateAndReject(
        NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR, __func__);
  }

  mLocked = true;

  auto CreateAndRejectInitPromise = [](const char* aFunc, nsresult aRv) {
    return CreateAndRejectMozPromise<InitPromise>(aFunc, aRv);
  };

  nsString type;
  fs::TimeStamp lastModifiedMilliSeconds;
  fs::Path path;
  nsCOMPtr<nsIFile> file;
  QM_TRY(MOZ_TO_RESULT(mDataManager->MutableDatabaseManagerPtr()->GetFile(
             mEntryId, type, lastModifiedMilliSeconds, path, file)),
         CreateAndRejectInitPromise);

  if (LOG_ENABLED()) {
    nsAutoString path;
    if (NS_SUCCEEDED(file->GetPath(path))) {
      LOG(("Opening SyncAccessHandle %s", NS_ConvertUTF16toUTF8(path).get()));
    }
  }

  QM_TRY_UNWRAP(
      nsCOMPtr<nsIRandomAccessStream> stream,
      CreateFileRandomAccessStream(quota::PERSISTENCE_TYPE_DEFAULT,
                                   mDataManager->OriginMetadataRef(),
                                   quota::Client::FILESYSTEM, file, -1, -1,
                                   nsIFileRandomAccessStream::DEFER_OPEN),
      CreateAndRejectInitPromise);

  mozilla::ipc::RandomAccessStreamParams streamParams =
      mozilla::ipc::SerializeRandomAccessStream(
          WrapMovingNotNullUnchecked(std::move(stream)), this);

  return InvokeAsync(
      mDataManager->MutableBackgroundTargetPtr(), __func__,
      [self = RefPtr(this), streamParams = std::move(streamParams)]() mutable {
        self->mDataManager->RegisterAccessHandle(WrapNotNull(self));

        self->mRegistered = true;

        return InitPromise::CreateAndResolve(std::move(streamParams), __func__);
      });
}

}  // namespace mozilla::dom
