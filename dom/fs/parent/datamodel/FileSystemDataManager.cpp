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
#include "GetDirectoryForOrigin.h"
#include "SchemaVersion001.h"
#include "fs/FileSystemConstants.h"
#include "mozIStorageService.h"
#include "mozStorageCID.h"
#include "mozilla/Result.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/FileSystemManagerParent.h"
#include "mozilla/dom/quota/ClientImpl.h"
#include "mozilla/dom/quota/DirectoryLock.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "nsBaseHashtable.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsIFile.h"
#include "nsNetCID.h"
#include "nsProxyRelease.h"
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

nsresult GetOrCreateEntry(const nsAString& aDesiredFilePath, bool& aExists,
                          nsCOMPtr<nsIFile>& aFile,
                          decltype(nsIFile::NORMAL_FILE_TYPE) aKind,
                          uint32_t aPermissions) {
  MOZ_ASSERT(!aDesiredFilePath.IsEmpty());

  QM_TRY(MOZ_TO_RESULT(NS_NewLocalFile(aDesiredFilePath,
                                       /* aFollowLinks */ false,
                                       getter_AddRefs(aFile))));

  QM_TRY(MOZ_TO_RESULT(aFile->Exists(&aExists)));
  if (aExists) {
    return NS_OK;
  }

  QM_TRY(MOZ_TO_RESULT(aFile->Create(aKind, aPermissions)));

  return NS_OK;
}

nsresult GetFileSystemDirectory(const Origin& aOrigin,
                                nsCOMPtr<nsIFile>& aDirectory) {
  quota::QuotaManager* quotaManager = quota::QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

#if 0
  QM_TRY_UNWRAP(aDirectory, quotaManager->GetDirectoryForOrigin(
                                quota::PERSISTENCE_TYPE_DEFAULT, aOrigin));
#else
  QM_TRY_UNWRAP(aDirectory, GetDirectoryForOrigin(*quotaManager, aOrigin));
#endif

  QM_TRY(MOZ_TO_RESULT(aDirectory->AppendRelativePath(u"fs"_ns)));

  return NS_OK;
}

nsresult GetFileSystemDatabaseFile(const Origin& aOrigin,
                                   nsString& aDatabaseFile) {
  nsCOMPtr<nsIFile> directoryPath;
  QM_TRY(MOZ_TO_RESULT(GetFileSystemDirectory(aOrigin, directoryPath)));

  QM_TRY(
      MOZ_TO_RESULT(directoryPath->AppendRelativePath(u"metadata.sqlite"_ns)));

  QM_TRY(MOZ_TO_RESULT(directoryPath->GetPath(aDatabaseFile)));

  return NS_OK;
}

nsresult GetDatabasePath(const Origin& aOrigin, bool& aExists,
                         nsCOMPtr<nsIFile>& aFile) {
  nsString databaseFilePath;
  QM_TRY(MOZ_TO_RESULT(GetFileSystemDatabaseFile(aOrigin, databaseFilePath)));

  MOZ_ASSERT(!databaseFilePath.IsEmpty());

  return GetOrCreateEntry(databaseFilePath, aExists, aFile,
                          nsIFile::NORMAL_FILE_TYPE, 0644);
}

Result<ResultConnection, QMResult> GetStorageConnection(const Origin& aOrigin) {
  bool exists = false;
  nsCOMPtr<nsIFile> databaseFile;
  QM_TRY(QM_TO_RESULT(GetDatabasePath(aOrigin, exists, databaseFile)));

  QM_TRY_INSPECT(
      const auto& storageService,
      QM_TO_RESULT_TRANSFORM(MOZ_TO_RESULT_GET_TYPED(
          nsCOMPtr<mozIStorageService>, MOZ_SELECT_OVERLOAD(do_GetService),
          MOZ_STORAGE_SERVICE_CONTRACTID)));

  const auto flags = mozIStorageService::CONNECTION_DEFAULT;
  ResultConnection connection;
  QM_TRY(QM_TO_RESULT(storageService->OpenDatabase(
      databaseFile, flags, getter_AddRefs(connection))));

  return connection;
}

}  // namespace

Result<EntryId, QMResult> GetRootHandle(const Origin& origin) {
  MOZ_ASSERT(!origin.IsEmpty());

  return FileSystemHashSource::GenerateHash(origin, kRootName);
}

Result<EntryId, QMResult> GetEntryHandle(
    const FileSystemChildMetadata& aHandle) {
  MOZ_ASSERT(!aHandle.parentId().IsEmpty());

  return FileSystemHashSource::GenerateHash(aHandle.parentId(),
                                            aHandle.childName());
}

FileSystemDataManager::FileSystemDataManager(
    const quota::OriginMetadata& aOriginMetadata,
    MovingNotNull<nsCOMPtr<nsIEventTarget>> aIOTarget,
    MovingNotNull<RefPtr<TaskQueue>> aIOTaskQueue)
    : mOriginMetadata(aOriginMetadata),
      mBackgroundTarget(WrapNotNull(GetCurrentSerialEventTarget())),
      mIOTarget(std::move(aIOTarget)),
      mIOTaskQueue(std::move(aIOTaskQueue)),
      mRegCount(0),
      mState(State::Initial) {}

FileSystemDataManager::~FileSystemDataManager() {
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
              const BoolPromise::ResolveOrRejectValue&) {
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

  QM_TRY_UNWRAP(auto streamTransportService,
                MOZ_TO_RESULT_GET_TYPED(nsCOMPtr<nsIEventTarget>,
                                        MOZ_SELECT_OVERLOAD(do_GetService),
                                        NS_STREAMTRANSPORTSERVICE_CONTRACTID),
                CreatePromise::CreateAndReject(NS_ERROR_FAILURE, __func__));

  nsCString taskQueueName("OPFS "_ns + aOriginMetadata.mOrigin);

  RefPtr<TaskQueue> ioTaskQueue =
      TaskQueue::Create(do_AddRef(streamTransportService), taskQueueName.get());

  auto dataManager = MakeRefPtr<FileSystemDataManager>(
      aOriginMetadata, WrapMovingNotNull(streamTransportService),
      WrapMovingNotNull(ioTaskQueue));

  AddFileSystemDataManager(aOriginMetadata.mOrigin, dataManager);

  return dataManager->BeginOpen()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [dataManager = Registered<FileSystemDataManager>(dataManager)](
          const BoolPromise::ResolveOrRejectValue&) {
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

RefPtr<BoolPromise> FileSystemDataManager::OnOpen() {
  MOZ_ASSERT(mState == State::Opening);

  return mOpenPromiseHolder.Ensure(__func__);
}

RefPtr<BoolPromise> FileSystemDataManager::OnClose() {
  MOZ_ASSERT(mState == State::Closing);

  return mClosePromiseHolder.Ensure(__func__);
}

bool FileSystemDataManager::LockExclusive(const EntryId& aEntryId) {
  if (mExclusiveLocks.Contains(aEntryId)) {
    return false;
  }

  mExclusiveLocks.Insert(aEntryId);
  return true;
}

void FileSystemDataManager::UnlockExclusive(const EntryId& aEntryId) {
  MOZ_ASSERT(mExclusiveLocks.Contains(aEntryId));

  mExclusiveLocks.Remove(aEntryId);
}

bool FileSystemDataManager::IsInactive() const {
  return !mRegCount && !mBackgroundThreadAccessible.Access()->mActors.Count();
}

void FileSystemDataManager::RequestAllowToClose() {
  for (const auto& actor : mBackgroundThreadAccessible.Access()->mActors) {
    actor->RequestAllowToClose();
  }
}

RefPtr<BoolPromise> FileSystemDataManager::BeginOpen() {
  MOZ_ASSERT(mState == State::Initial);

  mState = State::Opening;

  QM_TRY_UNWRAP(const NotNull<RefPtr<quota::QuotaManager>> quotaManager,
                quota::QuotaManager::GetOrCreate(), CreateAndRejectBoolPromise);

  RefPtr<quota::ClientDirectoryLock> directoryLock =
      quotaManager->CreateDirectoryLock(quota::PERSISTENCE_TYPE_DEFAULT,
                                        mOriginMetadata,
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
      ->Then(quotaManager->IOThread(), __func__,
             [self = RefPtr<FileSystemDataManager>(this)](
                 const BoolPromise::ResolveOrRejectValue& value) mutable {
               auto autoProxyReleaseManager = MakeScopeExit([&self] {
                 nsCOMPtr<nsISerialEventTarget> target =
                     self->MutableBackgroundTargetPtr();

                 NS_ProxyRelease("ReleaseFileSystemDataManager", target,
                                 self.forget());
               });

               if (value.IsReject()) {
                 return BoolPromise::CreateAndReject(value.RejectValue(),
                                                     __func__);
               }

               quota::QuotaManager* quotaManager = quota::QuotaManager::Get();
               QM_TRY(MOZ_TO_RESULT(quotaManager), CreateAndRejectBoolPromise);

               QM_TRY(MOZ_TO_RESULT(quotaManager->EnsureStorageIsInitialized()),
                      CreateAndRejectBoolPromise);

               QM_TRY(MOZ_TO_RESULT(
                          quotaManager->EnsureTemporaryStorageIsInitialized()),
                      CreateAndRejectBoolPromise);

               QM_TRY_INSPECT(
                   const auto& dirInfo,
                   quotaManager->EnsureTemporaryOriginIsInitialized(
                       quota::PERSISTENCE_TYPE_DEFAULT, self->mOriginMetadata),
                   CreateAndRejectBoolPromise);

               Unused << dirInfo;

               return BoolPromise::CreateAndResolve(true, __func__);
             })
      ->Then(
          MutableIOTargetPtr(), __func__,
          [self = RefPtr<FileSystemDataManager>(this)](
              const BoolPromise::ResolveOrRejectValue& value) mutable {
            auto autoProxyReleaseManager = MakeScopeExit([&self] {
              nsCOMPtr<nsISerialEventTarget> target =
                  self->MutableBackgroundTargetPtr();

              NS_ProxyRelease("ReleaseFileSystemDataManager", target,
                              self.forget());
            });

            if (value.IsReject()) {
              return BoolPromise::CreateAndReject(value.RejectValue(),
                                                  __func__);
            }

            QM_TRY_UNWRAP(
                auto connection,
                fs::data::GetStorageConnection(self->mOriginMetadata.mOrigin),
                CreateAndRejectBoolPromiseFromQMResult);

            QM_TRY_UNWRAP(DatabaseVersion version,
                          SchemaVersion001::InitializeConnection(
                              connection, self->mOriginMetadata.mOrigin),
                          CreateAndRejectBoolPromiseFromQMResult);

            if (1 == version) {
              QM_TRY_UNWRAP(FileSystemFileManager fmRes,
                            FileSystemFileManager::CreateFileSystemFileManager(
                                self->mOriginMetadata.mOrigin),
                            CreateAndRejectBoolPromiseFromQMResult);

              self->mDatabaseManager =
                  MakeUnique<FileSystemDatabaseManagerVersion001>(
                      std::move(connection),
                      MakeUnique<FileSystemFileManager>(std::move(fmRes)));
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

  InvokeAsync(MutableIOTargetPtr(), __func__,
              [self = RefPtr<FileSystemDataManager>(this)]() mutable {
                auto autoProxyReleaseManager = MakeScopeExit([&self] {
                  nsCOMPtr<nsISerialEventTarget> target =
                      self->MutableBackgroundTargetPtr();

                  NS_ProxyRelease("ReleaseFileSystemDataManager", target,
                                  self.forget());
                });

                self->mDatabaseManager = nullptr;

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
