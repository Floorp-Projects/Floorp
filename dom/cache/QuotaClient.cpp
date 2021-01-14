/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaClientImpl.h"

#include "DBAction.h"
#include "FileUtilsImpl.h"
#include "mozilla/dom/cache/DBSchema.h"
#include "mozilla/dom/cache/Manager.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "nsIFile.h"
#include "nsThreadUtils.h"

namespace mozilla::dom::cache {

using mozilla::dom::quota::AssertIsOnIOThread;
using mozilla::dom::quota::Client;
using mozilla::dom::quota::CloneFileAndAppend;
using mozilla::dom::quota::DatabaseUsageType;
using mozilla::dom::quota::GroupAndOrigin;
using mozilla::dom::quota::PERSISTENCE_TYPE_DEFAULT;
using mozilla::dom::quota::PersistenceType;
using mozilla::dom::quota::QuotaManager;
using mozilla::dom::quota::UsageInfo;
using mozilla::ipc::AssertIsOnBackgroundThread;

namespace {

template <typename StepFunc>
Result<UsageInfo, nsresult> ReduceUsageInfo(nsIFile& aDir,
                                            const Atomic<bool>& aCanceled,
                                            const StepFunc& aStepFunc) {
  CACHE_TRY_RETURN(quota::ReduceEachFileAtomicCancelable(
      aDir, aCanceled, UsageInfo{},
      [&aStepFunc](UsageInfo usageInfo, const nsCOMPtr<nsIFile>& bodyDir)
          -> Result<UsageInfo, nsresult> {
        CACHE_TRY(OkIf(!QuotaManager::IsShuttingDown()), Err(NS_ERROR_ABORT));

        CACHE_TRY_INSPECT(const auto& stepUsageInfo, aStepFunc(bodyDir));

        return usageInfo + stepUsageInfo;
      }));
}

Result<UsageInfo, nsresult> GetBodyUsage(nsIFile& aMorgueDir,
                                         const Atomic<bool>& aCanceled,
                                         const bool aInitializing) {
  AssertIsOnIOThread();

  CACHE_TRY_RETURN(ReduceUsageInfo(
      aMorgueDir, aCanceled,
      [aInitializing](
          const nsCOMPtr<nsIFile>& bodyDir) -> Result<UsageInfo, nsresult> {
        CACHE_TRY_INSPECT(const bool& isDir,
                          MOZ_TO_RESULT_INVOKE(bodyDir, IsDirectory));

        if (!isDir) {
          const DebugOnly<nsresult> result =
              RemoveNsIFile(QuotaInfo{}, *bodyDir, /* aTrackQuota */ false);
          // Try to remove the unexpected files, and keep moving on even if it
          // fails because it might be created by virus or the operation system
          MOZ_ASSERT(NS_SUCCEEDED(result));
          return UsageInfo{};
        }

        UsageInfo usageInfo;
        const auto getUsage =
            [&usageInfo](nsIFile& bodyFile,
                         const nsACString& leafName) -> Result<bool, nsresult> {
          Unused << leafName;

          CACHE_TRY_INSPECT(const int64_t& fileSize,
                            MOZ_TO_RESULT_INVOKE(bodyFile, GetFileSize));
          MOZ_DIAGNOSTIC_ASSERT(fileSize >= 0);
          // FIXME: Separate file usage and database usage in OriginInfo so that
          // the workaround for treating body file size as database usage can be
          // removed.
          //
          // This is needed because we want to remove the mutex lock for padding
          // files. The lock is needed because the padding file is accessed on
          // the QM IO thread while getting origin usage and is accessed on the
          // Cache IO thread in normal Cache operations. Using the cached usage
          // in QM while getting origin usage can remove the access on the QM IO
          // thread and thus we can remove the mutex lock. However, QM only
          // separates usage types in initialization, and the separation is gone
          // after that. So, before extending the separation of usage types in
          // QM, this is a workaround to avoid the file usage mismatching in our
          // tests. Note that file usage hasn't been exposed to users yet.
          usageInfo += DatabaseUsageType(Some(fileSize));

          return false;
        };
        CACHE_TRY(BodyTraverseFiles(QuotaInfo{}, *bodyDir, getUsage,
                                    /* aCanRemoveFiles */
                                    aInitializing,
                                    /* aTrackQuota */ false));
        return usageInfo;
      }));
}

Result<int64_t, nsresult> LockedGetPaddingSizeFromDB(
    nsIFile& aDir, const GroupAndOrigin& aGroupAndOrigin) {
  QuotaInfo quotaInfo;
  static_cast<GroupAndOrigin&>(quotaInfo) = aGroupAndOrigin;
  // quotaInfo.mDirectoryLockId must be -1 (which is default for new QuotaInfo)
  // because this method should only be called from QuotaClient::InitOrigin
  // (via QuotaClient::GetUsageForOriginInternal) when the temporary storage
  // hasn't been initialized yet. At that time, the in-memory objects (e.g.
  // OriginInfo) are only being created so it doesn't make sense to tunnel
  // quota information to TelemetryVFS to get corresponding QuotaObject instance
  // for the SQLite file).
  MOZ_DIAGNOSTIC_ASSERT(quotaInfo.mDirectoryLockId == -1);

  CACHE_TRY_INSPECT(const auto& dbFile,
                    CloneFileAndAppend(aDir, kCachesSQLiteFilename));

  CACHE_TRY_INSPECT(const bool& exists, MOZ_TO_RESULT_INVOKE(dbFile, Exists));

  // Return size = 0 if caches.sqlite doesn't exist.
  // This function is only called if the value of the padding size couldn't be
  // determined from the padding file, possibly because it doesn't exist, or a
  // leftover temporary padding file was found.
  // There is no other way to get the overall padding size of an origin.
  if (!exists) {
    return 0;
  }

  CACHE_TRY_INSPECT(const auto& conn, OpenDBConnection(quotaInfo, *dbFile));

  // Make sure that the database has the latest schema before we try to read
  // from it. We have to do this because LockedGetPaddingSizeFromDB is called
  // by QuotaClient::GetUsageForOrigin which may run at any time (there's no
  // guarantee that SetupAction::RunSyncWithDBOnTarget already checked the
  // schema for the given origin).
  CACHE_TRY(db::CreateOrMigrateSchema(*conn));

  CACHE_TRY_RETURN(LockedDirectoryPaddingRestore(aDir, *conn,
                                                 /* aMustRestore */ false));
}

}  // namespace

const nsLiteralString kCachesSQLiteFilename = u"caches.sqlite"_ns;

CacheQuotaClient::CacheQuotaClient()
    : mDirPaddingFileMutex("DOMCacheQuotaClient.mDirPaddingFileMutex") {
  AssertIsOnBackgroundThread();
  MOZ_DIAGNOSTIC_ASSERT(!sInstance);
  sInstance = this;
}

// static
CacheQuotaClient* CacheQuotaClient::Get() {
  MOZ_DIAGNOSTIC_ASSERT(sInstance);
  return sInstance;
}

CacheQuotaClient::Type CacheQuotaClient::GetType() { return DOMCACHE; }

Result<UsageInfo, nsresult> CacheQuotaClient::InitOrigin(
    PersistenceType aPersistenceType, const GroupAndOrigin& aGroupAndOrigin,
    const AtomicBool& aCanceled) {
  AssertIsOnIOThread();

  return GetUsageForOriginInternal(aPersistenceType, aGroupAndOrigin, aCanceled,
                                   /* aInitializing*/ true);
}

nsresult CacheQuotaClient::InitOriginWithoutTracking(
    PersistenceType aPersistenceType, const GroupAndOrigin& aGroupAndOrigin,
    const AtomicBool& aCanceled) {
  AssertIsOnIOThread();

  // This is called when a storage/permanent/chrome/cache directory exists. Even
  // though this shouldn't happen with a "good" profile, we shouldn't return an
  // error here, since that would cause origin initialization to fail. We just
  // warn and otherwise ignore that.
  UNKNOWN_FILE_WARNING(NS_LITERAL_STRING_FROM_CSTRING(DOMCACHE_DIRECTORY_NAME));
  return NS_OK;
}

Result<UsageInfo, nsresult> CacheQuotaClient::GetUsageForOrigin(
    PersistenceType aPersistenceType, const GroupAndOrigin& aGroupAndOrigin,
    const AtomicBool& aCanceled) {
  AssertIsOnIOThread();

  return GetUsageForOriginInternal(aPersistenceType, aGroupAndOrigin, aCanceled,
                                   /* aInitializing*/ false);
}

void CacheQuotaClient::OnOriginClearCompleted(PersistenceType aPersistenceType,
                                              const nsACString& aOrigin) {
  // Nothing to do here.
}

void CacheQuotaClient::ReleaseIOThreadObjects() {
  // Nothing to do here as the Context handles cleaning everything up
  // automatically.
}

void CacheQuotaClient::AbortOperationsForLocks(
    const DirectoryLockIdTable& aDirectoryLockIds) {
  AssertIsOnBackgroundThread();

  Manager::Abort(aDirectoryLockIds);
}

void CacheQuotaClient::AbortOperationsForProcess(
    ContentParentId aContentParentId) {
  // The Cache and Context can be shared by multiple client processes.  They
  // are not exclusively owned by a single process.
  //
  // As far as I can tell this is used by QuotaManager to abort operations
  // when a particular process goes away.  We definitely don't want this
  // since we are shared.  Also, the Cache actor code already properly
  // handles asynchronous actor destruction when the child process dies.
  //
  // Therefore, do nothing here.
}

void CacheQuotaClient::AbortAllOperations() {
  AssertIsOnBackgroundThread();

  Manager::AbortAll();
}

void CacheQuotaClient::StartIdleMaintenance() {}

void CacheQuotaClient::StopIdleMaintenance() {}

void CacheQuotaClient::InitiateShutdown() {
  AssertIsOnBackgroundThread();

  Manager::InitiateShutdown();
}

bool CacheQuotaClient::IsShutdownCompleted() const {
  AssertIsOnBackgroundThread();

  return Manager::IsShutdownAllComplete();
}

void CacheQuotaClient::ForceKillActors() {
  // Currently we don't implement killing actors (are there any to kill here?).
}

nsCString CacheQuotaClient::GetShutdownStatus() const {
  AssertIsOnBackgroundThread();

  return Manager::GetShutdownStatus();
}

void CacheQuotaClient::FinalizeShutdown() {
  // Nothing to do here.
}

nsresult CacheQuotaClient::UpgradeStorageFrom2_0To2_1(nsIFile* aDirectory) {
  AssertIsOnIOThread();
  MOZ_DIAGNOSTIC_ASSERT(aDirectory);

  MutexAutoLock lock(mDirPaddingFileMutex);

  nsresult rv = LockedDirectoryPaddingInit(*aDirectory);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return rv;
}

nsresult CacheQuotaClient::RestorePaddingFileInternal(
    nsIFile* aBaseDir, mozIStorageConnection* aConn) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);
  MOZ_DIAGNOSTIC_ASSERT(aConn);

  MutexAutoLock lock(mDirPaddingFileMutex);

  CACHE_TRY_INSPECT(const int64_t& dummyPaddingSize,
                    LockedDirectoryPaddingRestore(*aBaseDir, *aConn,
                                                  /* aMustRestore */ true));
  Unused << dummyPaddingSize;

  return NS_OK;
}

nsresult CacheQuotaClient::WipePaddingFileInternal(const QuotaInfo& aQuotaInfo,
                                                   nsIFile* aBaseDir) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);

  MutexAutoLock lock(mDirPaddingFileMutex);

  MOZ_ASSERT(DirectoryPaddingFileExists(*aBaseDir, DirPaddingFile::FILE));

  CACHE_TRY_INSPECT(
      const int64_t& paddingSize, ([&aBaseDir]() -> Result<int64_t, nsresult> {
        const bool temporaryPaddingFileExist =
            DirectoryPaddingFileExists(*aBaseDir, DirPaddingFile::TMP_FILE);

        Maybe<int64_t> directoryPaddingGetResult;
        if (!temporaryPaddingFileExist) {
          CACHE_TRY_UNWRAP(
              directoryPaddingGetResult,
              ([&aBaseDir]() -> Result<Maybe<int64_t>, nsresult> {
                CACHE_TRY_RETURN(
                    LockedDirectoryPaddingGet(*aBaseDir).map(Some<int64_t>),
                    Maybe<int64_t>{});
              }()));
        }

        if (temporaryPaddingFileExist || !directoryPaddingGetResult) {
          // XXXtt: Maybe have a method in the QuotaManager to clean the usage
          // under the quota client and the origin. There is nothing we can do
          // to recover the file.
          NS_WARNING("Cannnot read padding size from file!");
          return 0;
        }

        return *directoryPaddingGetResult;
      }()));

  if (paddingSize > 0) {
    DecreaseUsageForQuotaInfo(aQuotaInfo, paddingSize);
  }

  nsresult rv =
      LockedDirectoryPaddingDeleteFile(*aBaseDir, DirPaddingFile::FILE);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Remove temporary file if we have one.
  rv = LockedDirectoryPaddingDeleteFile(*aBaseDir, DirPaddingFile::TMP_FILE);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = LockedDirectoryPaddingInit(*aBaseDir);
  Unused << NS_WARN_IF(NS_FAILED(rv));

  return rv;
}

CacheQuotaClient::~CacheQuotaClient() {
  AssertIsOnBackgroundThread();
  MOZ_DIAGNOSTIC_ASSERT(sInstance == this);

  sInstance = nullptr;
}

Result<UsageInfo, nsresult> CacheQuotaClient::GetUsageForOriginInternal(
    PersistenceType aPersistenceType, const GroupAndOrigin& aGroupAndOrigin,
    const AtomicBool& aCanceled, const bool aInitializing) {
  AssertIsOnIOThread();

  QuotaManager* const qm = QuotaManager::Get();
  MOZ_DIAGNOSTIC_ASSERT(qm);

  CACHE_TRY_INSPECT(
      const auto& dir,
      qm->GetDirectoryForOrigin(aPersistenceType, aGroupAndOrigin.mOrigin));

  CACHE_TRY(
      dir->Append(NS_LITERAL_STRING_FROM_CSTRING(DOMCACHE_DIRECTORY_NAME)));

  CACHE_TRY_INSPECT(
      const auto& maybePaddingSize,
      ([this, &dir, aInitializing,
        &aGroupAndOrigin]() -> Result<Maybe<int64_t>, nsresult> {
        // If the temporary file still exists after locking, it means the
        // previous action failed, so restore the padding file.
        MutexAutoLock lock(mDirPaddingFileMutex);

        if (!DirectoryPaddingFileExists(*dir, DirPaddingFile::TMP_FILE)) {
          const auto& maybePaddingSize = [&dir]() -> Maybe<int64_t> {
            CACHE_TRY_RETURN(LockedDirectoryPaddingGet(*dir).map(Some<int64_t>),
                             Nothing{});
          }();

          if (maybePaddingSize) {
            return maybePaddingSize;
          }
        }

        if (aInitializing) {
          CACHE_TRY_RETURN(LockedGetPaddingSizeFromDB(*dir, aGroupAndOrigin)
                               .map(Some<int64_t>));
        }

        // We can't open the database at this point, since it can be already
        // used by Cache IO thread. Use the cached value instead. (In
        // theory, we could check if the database is actually used by Cache
        // IO thread at this moment, but it's probably not worth additional
        // complexity.)

        return Maybe<int64_t>{};
      }()));

  if (!maybePaddingSize) {
    return qm->GetUsageForClient(PERSISTENCE_TYPE_DEFAULT, aGroupAndOrigin,
                                 Client::DOMCACHE);
  }

  CACHE_TRY_INSPECT(
      const auto& innerUsageInfo,
      ReduceUsageInfo(
          *dir, aCanceled,
          [&aCanceled, aInitializing](
              const nsCOMPtr<nsIFile>& file) -> Result<UsageInfo, nsresult> {
            CACHE_TRY_INSPECT(
                const auto& leafName,
                MOZ_TO_RESULT_INVOKE_TYPED(nsAutoString, file, GetLeafName));

            CACHE_TRY_INSPECT(const bool& isDir,
                              MOZ_TO_RESULT_INVOKE(file, IsDirectory));

            if (isDir) {
              if (leafName.EqualsLiteral("morgue")) {
                CACHE_TRY_RETURN(GetBodyUsage(*file, aCanceled, aInitializing));
              } else {
                NS_WARNING("Unknown Cache directory found!");
              }

              return UsageInfo{};
            }

            // Ignore transient sqlite files and marker files
            if (leafName.EqualsLiteral("caches.sqlite-journal") ||
                leafName.EqualsLiteral("caches.sqlite-shm") ||
                leafName.Find("caches.sqlite-mj"_ns, false, 0, 0) == 0 ||
                leafName.EqualsLiteral("context_open.marker")) {
              return UsageInfo{};
            }

            if (leafName.Equals(kCachesSQLiteFilename) ||
                leafName.EqualsLiteral("caches.sqlite-wal")) {
              CACHE_TRY_INSPECT(const int64_t& fileSize,
                                MOZ_TO_RESULT_INVOKE(file, GetFileSize));
              MOZ_DIAGNOSTIC_ASSERT(fileSize >= 0);

              return UsageInfo{DatabaseUsageType(Some(fileSize))};
            }

            // Ignore directory padding file
            if (leafName.EqualsLiteral(PADDING_FILE_NAME) ||
                leafName.EqualsLiteral(PADDING_TMP_FILE_NAME)) {
              return UsageInfo{};
            }

            NS_WARNING("Unknown Cache file found!");

            return UsageInfo{};
          }));

  // FIXME: Separate file usage and database usage in OriginInfo so that the
  // workaround for treating padding file size as database usage can be removed.
  return UsageInfo{DatabaseUsageType(maybePaddingSize)} + innerUsageInfo;
}

// static
CacheQuotaClient* CacheQuotaClient::sInstance = nullptr;

// static
already_AddRefed<quota::Client> CreateQuotaClient() {
  AssertIsOnBackgroundThread();

  RefPtr<CacheQuotaClient> ref = new CacheQuotaClient();
  return ref.forget();
}

// static
nsresult RestorePaddingFile(nsIFile* aBaseDir, mozIStorageConnection* aConn) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);
  MOZ_DIAGNOSTIC_ASSERT(aConn);

  RefPtr<CacheQuotaClient> cacheQuotaClient = CacheQuotaClient::Get();
  MOZ_DIAGNOSTIC_ASSERT(cacheQuotaClient);

  nsresult rv = cacheQuotaClient->RestorePaddingFileInternal(aBaseDir, aConn);
  Unused << NS_WARN_IF(NS_FAILED(rv));

  return rv;
}

// static
nsresult WipePaddingFile(const QuotaInfo& aQuotaInfo, nsIFile* aBaseDir) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);

  RefPtr<CacheQuotaClient> cacheQuotaClient = CacheQuotaClient::Get();
  MOZ_DIAGNOSTIC_ASSERT(cacheQuotaClient);

  nsresult rv = cacheQuotaClient->WipePaddingFileInternal(aQuotaInfo, aBaseDir);
  Unused << NS_WARN_IF(NS_FAILED(rv));

  return rv;
}

}  // namespace mozilla::dom::cache
