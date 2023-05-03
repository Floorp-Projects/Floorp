/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemDataManager.h"

#include "FileSystemDatabaseManager.h"
#include "FileSystemDatabaseManagerVersion001.h"
#include "FileSystemFileManager.h"
#include "FileSystemHashSource.h"
#include "ResultStatement.h"
#include "SchemaVersion001.h"
#include "fs/FileSystemConstants.h"
#include "mozIStorageService.h"
#include "mozStorageCID.h"
#include "mozilla/Result.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/FileSystemLog.h"
#include "mozilla/dom/FileSystemManagerParent.h"
#include "mozilla/dom/quota/ClientImpl.h"
#include "mozilla/dom/quota/DirectoryLock.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "nsBaseHashtable.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsIFile.h"
#include "nsIFileURL.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsThreadUtils.h"

namespace mozilla::dom::fs::data {

namespace {

// nsCStringHashKey with disabled memmove
class nsCStringHashKeyDM : public nsCStringHashKey {
 public:
  explicit nsCStringHashKeyDM(const nsCStringHashKey::KeyTypePointer aKey)
      : nsCStringHashKey(aKey) {}
  enum { ALLOW_MEMMOVE = false };
};

// When CheckedUnsafePtr's checking is enabled, it's necessary to ensure that
// the hashtable uses the copy constructor instead of memmove for moving entries
// since memmove will break CheckedUnsafePtr in a memory-corrupting way.
using FileSystemDataManagerHashKey =
    std::conditional<DiagnosticAssertEnabled::value, nsCStringHashKeyDM,
                     nsCStringHashKey>::type;

// Raw (but checked when the diagnostic assert is enabled) references as we
// don't want to keep FileSystemDataManager objects alive forever. When a
// FileSystemDataManager is destroyed it calls RemoveFileSystemDataManager
// to clear itself.
using FileSystemDataManagerHashtable =
    nsBaseHashtable<FileSystemDataManagerHashKey,
                    NotNull<CheckedUnsafePtr<FileSystemDataManager>>,
                    MovingNotNull<CheckedUnsafePtr<FileSystemDataManager>>>;

// This hashtable isn't protected by any mutex but it is only ever touched on
// the PBackground thread.
StaticAutoPtr<FileSystemDataManagerHashtable> gDataManagers;

RefPtr<FileSystemDataManager> GetFileSystemDataManager(const Origin& aOrigin) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  if (gDataManagers) {
    auto maybeDataManager = gDataManagers->MaybeGet(aOrigin);
    if (maybeDataManager) {
      RefPtr<FileSystemDataManager> result(
          std::move(*maybeDataManager).unwrapBasePtr());
      return result;
    }
  }

  return nullptr;
}

void AddFileSystemDataManager(
    const Origin& aOrigin, const RefPtr<FileSystemDataManager>& aDataManager) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_ASSERT(!quota::QuotaManager::IsShuttingDown());

  if (!gDataManagers) {
    gDataManagers = new FileSystemDataManagerHashtable();
  }

  MOZ_ASSERT(!gDataManagers->Contains(aOrigin));
  gDataManagers->InsertOrUpdate(aOrigin,
                                WrapMovingNotNullUnchecked(aDataManager));
}

void RemoveFileSystemDataManager(const Origin& aOrigin) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  MOZ_ASSERT(gDataManagers);
  const DebugOnly<bool> removed = gDataManagers->Remove(aOrigin);
  MOZ_ASSERT(removed);

  if (!gDataManagers->Count()) {
    gDataManagers = nullptr;
  }
}

Result<ResultConnection, QMResult> GetStorageConnection(
    const quota::OriginMetadata& aOriginMetadata,
    const int64_t aDirectoryLockId) {
  MOZ_ASSERT(aDirectoryLockId >= 0);

  // Ensure that storage is initialized and file system folder exists!
  QM_TRY_INSPECT(const auto& dbFileUrl,
                 GetDatabaseFileURL(aOriginMetadata, aDirectoryLockId));

  QM_TRY_INSPECT(
      const auto& storageService,
      QM_TO_RESULT_TRANSFORM(MOZ_TO_RESULT_GET_TYPED(
          nsCOMPtr<mozIStorageService>, MOZ_SELECT_OVERLOAD(do_GetService),
          MOZ_STORAGE_SERVICE_CONTRACTID)));

  QM_TRY_UNWRAP(auto connection,
                QM_TO_RESULT_TRANSFORM(MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(
                    nsCOMPtr<mozIStorageConnection>, storageService,
                    OpenDatabaseWithFileURL, dbFileUrl, ""_ns,
                    mozIStorageService::CONNECTION_DEFAULT)));

  ResultConnection result(connection);

  return result;
}

}  // namespace

Result<EntryId, QMResult> GetRootHandle(const Origin& origin) {
  MOZ_ASSERT(!origin.IsEmpty());

  return FileSystemHashSource::GenerateHash(origin, kRootString);
}

Result<EntryId, QMResult> GetEntryHandle(
    const FileSystemChildMetadata& aHandle) {
  MOZ_ASSERT(!aHandle.parentId().IsEmpty());

  return FileSystemHashSource::GenerateHash(aHandle.parentId(),
                                            aHandle.childName());
}

FileSystemDataManager::FileSystemDataManager(
    const quota::OriginMetadata& aOriginMetadata,
    RefPtr<quota::QuotaManager> aQuotaManager,
    MovingNotNull<nsCOMPtr<nsIEventTarget>> aIOTarget,
    MovingNotNull<RefPtr<TaskQueue>> aIOTaskQueue)
    : mOriginMetadata(aOriginMetadata),
      mQuotaManager(std::move(aQuotaManager)),
      mBackgroundTarget(WrapNotNull(GetCurrentSerialEventTarget())),
      mIOTarget(std::move(aIOTarget)),
      mIOTaskQueue(std::move(aIOTaskQueue)),
      mRegCount(0),
      mState(State::Initial) {}

FileSystemDataManager::~FileSystemDataManager() {
  NS_ASSERT_OWNINGTHREAD(FileSystemDataManager);
  MOZ_ASSERT(mState == State::Closed);
  MOZ_ASSERT(!mDatabaseManager);
}

RefPtr<FileSystemDataManager::CreatePromise>
FileSystemDataManager::GetOrCreateFileSystemDataManager(
    const quota::OriginMetadata& aOriginMetadata) {
  if (quota::QuotaManager::IsShuttingDown()) {
    return CreatePromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  if (RefPtr<FileSystemDataManager> dataManager =
          GetFileSystemDataManager(aOriginMetadata.mOrigin)) {
    if (dataManager->IsOpening()) {
      // We have to wait for the open to be finished before resolving the
      // promise. The manager can't close itself in the meantime because we
      // add a new registration in the lambda capture list.
      return dataManager->OnOpen()->Then(
          GetCurrentSerialEventTarget(), __func__,
          [dataManager = Registered<FileSystemDataManager>(dataManager)](
              const BoolPromise::ResolveOrRejectValue& aValue) {
            if (aValue.IsReject()) {
              return CreatePromise::CreateAndReject(aValue.RejectValue(),
                                                    __func__);
            }
            return CreatePromise::CreateAndResolve(dataManager, __func__);
          });
    }

    if (dataManager->IsClosing()) {
      // First, we need to wait for the close to be finished. After that the
      // manager is closed and it can't be opened again. The only option is
      // to create a new manager and open it. However, all this stuff is
      // asynchronous, so it can happen that something else called
      // `GetOrCreateFileSystemManager` in the meantime. For that reason, we
      // shouldn't try to create a new manager and open it here, a "recursive"
      // call to `GetOrCreateFileSystemManager` will handle any new situation.
      return dataManager->OnClose()->Then(
          GetCurrentSerialEventTarget(), __func__,
          [aOriginMetadata](const BoolPromise::ResolveOrRejectValue&) {
            return GetOrCreateFileSystemDataManager(aOriginMetadata);
          });
    }

    return CreatePromise::CreateAndResolve(
        Registered<FileSystemDataManager>(std::move(dataManager)), __func__);
  }

  RefPtr<quota::QuotaManager> quotaManager = quota::QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  QM_TRY_UNWRAP(auto streamTransportService,
                MOZ_TO_RESULT_GET_TYPED(nsCOMPtr<nsIEventTarget>,
                                        MOZ_SELECT_OVERLOAD(do_GetService),
                                        NS_STREAMTRANSPORTSERVICE_CONTRACTID),
                CreatePromise::CreateAndReject(NS_ERROR_FAILURE, __func__));

  nsCString taskQueueName("OPFS "_ns + aOriginMetadata.mOrigin);

  RefPtr<TaskQueue> ioTaskQueue =
      TaskQueue::Create(do_AddRef(streamTransportService), taskQueueName.get());

  auto dataManager = MakeRefPtr<FileSystemDataManager>(
      aOriginMetadata, std::move(quotaManager),
      WrapMovingNotNull(streamTransportService),
      WrapMovingNotNull(ioTaskQueue));

  AddFileSystemDataManager(aOriginMetadata.mOrigin, dataManager);

  return dataManager->BeginOpen()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [dataManager = Registered<FileSystemDataManager>(dataManager)](
          const BoolPromise::ResolveOrRejectValue& aValue) {
        if (aValue.IsReject()) {
          return CreatePromise::CreateAndReject(aValue.RejectValue(), __func__);
        }

        return CreatePromise::CreateAndResolve(dataManager, __func__);
      });
}

// static
void FileSystemDataManager::AbortOperationsForLocks(
    const quota::Client::DirectoryLockIdTable& aDirectoryLockIds) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  // XXX Share the iteration code with `InitiateShutdown`, for example by
  // creating a helper function which would take a predicate function.

  if (!gDataManagers) {
    return;
  }

  for (const auto& dataManager : gDataManagers->Values()) {
    // Check if the Manager holds an acquired DirectoryLock. Origin clearing
    // can't be blocked by this Manager if there is no acquired DirectoryLock.
    // If there is an acquired DirectoryLock, check if the table contains the
    // lock for the Manager.
    if (quota::Client::IsLockForObjectAcquiredAndContainedInLockTable(
            *dataManager, aDirectoryLockIds)) {
      dataManager->RequestAllowToClose();
    }
  }
}

// static
void FileSystemDataManager::InitiateShutdown() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  if (!gDataManagers) {
    return;
  }

  for (const auto& dataManager : gDataManagers->Values()) {
    dataManager->RequestAllowToClose();
  }
}

// static
bool FileSystemDataManager::IsShutdownCompleted() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  return !gDataManagers;
}

void FileSystemDataManager::AssertIsOnIOTarget() const {
  DebugOnly<bool> current = false;
  MOZ_ASSERT(NS_SUCCEEDED(mIOTarget->IsOnCurrentThread(&current)));
  MOZ_ASSERT(current);
}

void FileSystemDataManager::Register() { mRegCount++; }

void FileSystemDataManager::Unregister() {
  MOZ_ASSERT(mRegCount > 0);

  mRegCount--;

  if (IsInactive()) {
    BeginClose();
  }
}

void FileSystemDataManager::RegisterActor(
    NotNull<FileSystemManagerParent*> aActor) {
  MOZ_ASSERT(!mBackgroundThreadAccessible.Access()->mActors.Contains(aActor));

  mBackgroundThreadAccessible.Access()->mActors.Insert(aActor);
}

void FileSystemDataManager::UnregisterActor(
    NotNull<FileSystemManagerParent*> aActor) {
  MOZ_ASSERT(mBackgroundThreadAccessible.Access()->mActors.Contains(aActor));

  mBackgroundThreadAccessible.Access()->mActors.Remove(aActor);

  if (IsInactive()) {
    BeginClose();
  }
}

void FileSystemDataManager::RegisterAccessHandle(
    NotNull<FileSystemAccessHandle*> aAccessHandle) {
  MOZ_ASSERT(!mBackgroundThreadAccessible.Access()->mAccessHandles.Contains(
      aAccessHandle));

  mBackgroundThreadAccessible.Access()->mAccessHandles.Insert(aAccessHandle);
}

void FileSystemDataManager::UnregisterAccessHandle(
    NotNull<FileSystemAccessHandle*> aAccessHandle) {
  MOZ_ASSERT(mBackgroundThreadAccessible.Access()->mAccessHandles.Contains(
      aAccessHandle));

  mBackgroundThreadAccessible.Access()->mAccessHandles.Remove(aAccessHandle);

  if (IsInactive()) {
    BeginClose();
  }
}

RefPtr<BoolPromise> FileSystemDataManager::OnOpen() {
  MOZ_ASSERT(mState == State::Opening);

  return mOpenPromiseHolder.Ensure(__func__);
}

RefPtr<BoolPromise> FileSystemDataManager::OnClose() {
  MOZ_ASSERT(mState == State::Closing);

  return mClosePromiseHolder.Ensure(__func__);
}

bool FileSystemDataManager::IsLocked(const EntryId& aEntryId) const {
  return mExclusiveLocks.Contains(aEntryId);
}

nsresult FileSystemDataManager::LockExclusive(const EntryId& aEntryId) {
  if (IsLocked(aEntryId)) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  // If the file has been removed, we should get a file not found error.
  // Otherwise, if usage tracking cannot be started because file size is not
  // known and attempts to read it are failing, lock is denied to freeze the
  // quota usage until the (external) blocker is gone or the file is removed.
  QM_TRY(MOZ_TO_RESULT(mDatabaseManager->BeginUsageTracking(aEntryId)));

  LOG_VERBOSE(("ExclusiveLock"));
  mExclusiveLocks.Insert(aEntryId);

  return NS_OK;
}

void FileSystemDataManager::UnlockExclusive(const EntryId& aEntryId) {
  MOZ_ASSERT(mExclusiveLocks.Contains(aEntryId));

  LOG_VERBOSE(("ExclusiveUnlock"));
  mExclusiveLocks.Remove(aEntryId);

  // On error, usage tracking remains on to prevent writes until usage is
  // updated successfully.
  QM_TRY(MOZ_TO_RESULT(mDatabaseManager->UpdateUsage(aEntryId)), QM_VOID);
  QM_TRY(MOZ_TO_RESULT(mDatabaseManager->EndUsageTracking(aEntryId)), QM_VOID);
}

nsresult FileSystemDataManager::LockShared(const EntryId& aEntryId) {
  return LockExclusive(aEntryId);
}

void FileSystemDataManager::UnlockShared(const EntryId& aEntryId) {
  UnlockExclusive(aEntryId);
}

bool FileSystemDataManager::IsInactive() const {
  auto data = mBackgroundThreadAccessible.Access();
  return !mRegCount && !data->mActors.Count() && !data->mAccessHandles.Count();
}

void FileSystemDataManager::RequestAllowToClose() {
  for (const auto& actor : mBackgroundThreadAccessible.Access()->mActors) {
    actor->RequestAllowToClose();
  }
}

RefPtr<BoolPromise> FileSystemDataManager::BeginOpen() {
  MOZ_ASSERT(mQuotaManager);
  MOZ_ASSERT(mState == State::Initial);

  mState = State::Opening;

  RefPtr<quota::ClientDirectoryLock> directoryLock =
      mQuotaManager->CreateDirectoryLock(
          quota::PERSISTENCE_TYPE_DEFAULT, mOriginMetadata,
          mozilla::dom::quota::Client::FILESYSTEM,
          /* aExclusive */ false);

  directoryLock->Acquire()
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [self = RefPtr<FileSystemDataManager>(this),
              directoryLock = directoryLock](
                 const BoolPromise::ResolveOrRejectValue& value) mutable {
               if (value.IsReject()) {
                 return BoolPromise::CreateAndReject(value.RejectValue(),
                                                     __func__);
               }

               self->mDirectoryLock = std::move(directoryLock);

               return BoolPromise::CreateAndResolve(true, __func__);
             })
      ->Then(mQuotaManager->IOThread(), __func__,
             [self = RefPtr<FileSystemDataManager>(this)](
                 const BoolPromise::ResolveOrRejectValue& value) {
               if (value.IsReject()) {
                 return BoolPromise::CreateAndReject(value.RejectValue(),
                                                     __func__);
               }

               QM_TRY(MOZ_TO_RESULT(
                          EnsureFileSystemDirectory(self->mOriginMetadata)),
                      CreateAndRejectBoolPromise);

               return BoolPromise::CreateAndResolve(true, __func__);
             })
      ->Then(MutableIOTaskQueuePtr(), __func__,
             [self = RefPtr<FileSystemDataManager>(this)](
                 const BoolPromise::ResolveOrRejectValue& value) {
               if (value.IsReject()) {
                 return BoolPromise::CreateAndReject(value.RejectValue(),
                                                     __func__);
               }

               QM_TRY_UNWRAP(
                   auto connection,
                   fs::data::GetStorageConnection(self->mOriginMetadata,
                                                  self->mDirectoryLock->Id()),
                   CreateAndRejectBoolPromiseFromQMResult);

               QM_TRY_UNWRAP(DatabaseVersion version,
                             SchemaVersion001::InitializeConnection(
                                 connection, self->mOriginMetadata.mOrigin),
                             CreateAndRejectBoolPromiseFromQMResult);

               if (1 == version) {
                 QM_TRY_UNWRAP(
                     FileSystemFileManager fmRes,
                     FileSystemFileManager::CreateFileSystemFileManager(
                         self->mOriginMetadata),
                     CreateAndRejectBoolPromiseFromQMResult);

                 QM_TRY_UNWRAP(
                     EntryId rootId,
                     fs::data::GetRootHandle(self->mOriginMetadata.mOrigin),
                     CreateAndRejectBoolPromiseFromQMResult);

                 self->mDatabaseManager =
                     MakeUnique<FileSystemDatabaseManagerVersion001>(
                         self, std::move(connection),
                         MakeUnique<FileSystemFileManager>(std::move(fmRes)),
                         rootId);
               }

               return BoolPromise::CreateAndResolve(true, __func__);
             })
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [self = RefPtr<FileSystemDataManager>(this)](
                 const BoolPromise::ResolveOrRejectValue& value) {
               if (value.IsReject()) {
                 self->mState = State::Initial;

                 self->mOpenPromiseHolder.RejectIfExists(value.RejectValue(),
                                                         __func__);

               } else {
                 self->mState = State::Open;

                 self->mOpenPromiseHolder.ResolveIfExists(true, __func__);
               }
             });

  return OnOpen();
}

RefPtr<BoolPromise> FileSystemDataManager::BeginClose() {
  MOZ_ASSERT(mState != State::Closing && mState != State::Closed);
  MOZ_ASSERT(IsInactive());

  mState = State::Closing;

  InvokeAsync(MutableIOTaskQueuePtr(), __func__,
              [self = RefPtr<FileSystemDataManager>(this)]() {
                if (self->mDatabaseManager) {
                  self->mDatabaseManager->Close();
                  self->mDatabaseManager = nullptr;
                }

                return BoolPromise::CreateAndResolve(true, __func__);
              })
      ->Then(MutableBackgroundTargetPtr(), __func__,
             [self = RefPtr<FileSystemDataManager>(this)](
                 const BoolPromise::ResolveOrRejectValue&) {
               return self->mIOTaskQueue->BeginShutdown();
             })
      ->Then(MutableBackgroundTargetPtr(), __func__,
             [self = RefPtr<FileSystemDataManager>(this)](
                 const ShutdownPromise::ResolveOrRejectValue&) {
               self->mDirectoryLock = nullptr;

               RemoveFileSystemDataManager(self->mOriginMetadata.mOrigin);

               self->mState = State::Closed;

               self->mClosePromiseHolder.ResolveIfExists(true, __func__);
             });

  return OnClose();
}

}  // namespace mozilla::dom::fs::data
