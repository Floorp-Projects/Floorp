/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemAccessHandle.h"

#include "mozilla/Result.h"
#include "mozilla/dom/FileSystemDataManager.h"
#include "mozilla/dom/FileSystemHelpers.h"
#include "mozilla/dom/FileSystemLog.h"

namespace mozilla::dom {

FileSystemAccessHandle::FileSystemAccessHandle(
    RefPtr<fs::data::FileSystemDataManager> aDataManager,
    const fs::EntryId& aEntryId)
    : mEntryId(aEntryId),
      mDataManager(std::move(aDataManager)),
      mActor(nullptr),
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
          const BoolPromise::ResolveOrRejectValue& value) {
        if (value.IsReject()) {
          return CreatePromise::CreateAndReject(value.RejectValue(), __func__);
        }

        return CreatePromise::CreateAndResolve(accessHandle, __func__);
      });
}

void FileSystemAccessHandle::Register() { ++mRegCount; }

void FileSystemAccessHandle::Unregister() {
  MOZ_ASSERT(mRegCount > 0);

  --mRegCount;

  if (IsInactive() && IsOpen()) {
    Close();
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
    Close();
  }
}

bool FileSystemAccessHandle::IsOpen() const { return !mClosed; }

void FileSystemAccessHandle::Close() {
  MOZ_ASSERT(IsOpen());

  LOG(("Closing AccessHandle"));

  mClosed = true;

  if (mLocked) {
    mDataManager->UnlockExclusive(mEntryId);
  }

  InvokeAsync(mDataManager->MutableBackgroundTargetPtr(), __func__,
              [self = RefPtr(this)]() {
                if (self->mRegistered) {
                  self->mDataManager->UnregisterAccessHandle(WrapNotNull(self));
                }

                self->mDataManager = nullptr;

                return BoolPromise::CreateAndResolve(true, __func__);
              });
}

bool FileSystemAccessHandle::IsInactive() const {
  return !mRegCount && !mActor;
}

RefPtr<BoolPromise> FileSystemAccessHandle::BeginInit() {
  if (!mDataManager->LockExclusive(mEntryId)) {
    return BoolPromise::CreateAndReject(
        NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR, __func__);
  }

  mLocked = true;

  return InvokeAsync(
      mDataManager->MutableBackgroundTargetPtr(), __func__,
      [self = RefPtr(this)]() {
        self->mDataManager->RegisterAccessHandle(WrapNotNull(self));

        self->mRegistered = true;

        return BoolPromise::CreateAndResolve(true, __func__);
      });
}

}  // namespace mozilla::dom
