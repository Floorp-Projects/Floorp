/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaClientImpl.h"

#include "DBAction.h"
#include "FileUtilsImpl.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/cache/DBSchema.h"
#include "mozilla/dom/cache/Manager.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "nsIFile.h"
#include "nsThreadUtils.h"

namespace mozilla::dom::cache {

using mozilla::dom::quota::AssertIsOnIOThread;
using mozilla::dom::quota::Client;
using mozilla::dom::quota::CloneFileAndAppend;
using mozilla::dom::quota::DatabaseUsageType;
using mozilla::dom::quota::GetDirEntryKind;
using mozilla::dom::quota::nsIFileKind;
using mozilla::dom::quota::OriginMetadata;
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
                                         const Atomic<bool>& aCanceled) {
  AssertIsOnIOThread();

  CACHE_TRY_RETURN(ReduceUsageInfo(
      aMorgueDir, aCanceled,
      [](const nsCOMPtr<nsIFile>& bodyDir) -> Result<UsageInfo, nsresult> {
        CACHE_TRY_INSPECT(const auto& dirEntryKind, GetDirEntryKind(*bodyDir));

        if (dirEntryKind != nsIFileKind::ExistsAsDirectory) {
          if (dirEntryKind == nsIFileKind::ExistsAsFile) {
            const DebugOnly<nsresult> result =
                RemoveNsIFile(QuotaInfo{}, *bodyDir, /* aTrackQuota */ false);
            // Try to remove the unexpected files, and keep moving on even if it
            // fails because it might be created by virus or the operation
            // system
            MOZ_ASSERT(NS_SUCCEEDED(result));
          }

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
        CACHE_TRY(ToResult(BodyTraverseFiles(QuotaInfo{}, *bodyDir, getUsage,
                                             /* aCanRemoveFiles */ true,
                                             /* aTrackQuota */ false))
#ifdef WIN32
                      .orElse([](const nsresult rv) -> Result<Ok, nsresult> {
                        // We treat ERROR_FILE_CORRUPT as if the directory did
                        // not exist at all.
                        if (NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_WIN32 &&
                            NS_ERROR_GET_CODE(rv) == ERROR_FILE_CORRUPT) {
                          return Ok{};
                        }

                        return Err(rv);
                      })
#endif
        );
        return usageInfo;
      }));
}

Result<int64_t, nsresult> GetPaddingSizeFromDB(
    nsIFile& aDir, nsIFile& aDBFile, const OriginMetadata& aOriginMetadata) {
  QuotaInfo quotaInfo;
  static_cast<OriginMetadata&>(quotaInfo) = aOriginMetadata;
  // quotaInfo.mDirectoryLockId must be -1 (which is default for new QuotaInfo)
  // because this method should only be called from QuotaClient::InitOrigin when
  // the temporary storage hasn't been initialized yet. At that time, the
  // in-memory objects (e.g. OriginInfo) are only being created so it doesn't
  // make sense to tunnel quota information to TelemetryVFS to get corresponding
  // QuotaObject instance for the SQLite file).
  MOZ_DIAGNOSTIC_ASSERT(quotaInfo.mDirectoryLockId == -1);

#ifdef DEBUG
  {
    CACHE_TRY_INSPECT(const bool& exists,
                      MOZ_TO_RESULT_INVOKE(aDBFile, Exists));
    MOZ_ASSERT(exists);
  }
#endif

  CACHE_TRY_INSPECT(const auto& conn, OpenDBConnection(quotaInfo, aDBFile));

  // Make sure that the database has the latest schema before we try to read
  // from it. We have to do this because GetPaddingSizeFromDB is called
  // by InitOrigin. And it means that SetupAction::RunSyncWithDBOnTarget hasn't
  // checked the schema for the given origin yet).
  CACHE_TRY(db::CreateOrMigrateSchema(*conn));

  CACHE_TRY_RETURN(DirectoryPaddingRestore(aDir, *conn,
                                           /* aMustRestore */ false));
}

}  // namespace

const nsLiteralString kCachesSQLiteFilename = u"caches.sqlite"_ns;
const nsLiteralString kMorgueDirectoryFilename = u"morgue"_ns;

CacheQuotaClient::CacheQuotaClient() {
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
    PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata,
    const AtomicBool& aCanceled) {
  AssertIsOnIOThread();

  QuotaManager* const qm = QuotaManager::Get();
  MOZ_DIAGNOSTIC_ASSERT(qm);

  CACHE_TRY_INSPECT(
      const auto& dir,
      qm->GetDirectoryForOrigin(aPersistenceType, aOriginMetadata.mOrigin));

  CACHE_TRY(
      dir->Append(NS_LITERAL_STRING_FROM_CSTRING(DOMCACHE_DIRECTORY_NAME)));

  CACHE_TRY_INSPECT(
      const auto& cachesSQLiteFile,
      ([dir]() -> Result<nsCOMPtr<nsIFile>, nsresult> {
        CACHE_TRY_INSPECT(const auto& cachesSQLite,
                          CloneFileAndAppend(*dir, kCachesSQLiteFilename));

        // IsDirectory is used to check if caches.sqlite exists or not. Another
        // benefit of this is that we can test the failed cases by creating a
        // directory named "caches.sqlite".
        CACHE_TRY_INSPECT(const auto& dirEntryKind,
                          GetDirEntryKind(*cachesSQLite));
        if (dirEntryKind == nsIFileKind::DoesNotExist) {
          // We only ensure padding files and morgue directory get removed like
          // WipeDatabase in DBAction.cpp. The -wal journal file will be
          // automatically deleted by sqlite when the new database is created.
          // XXX Ideally, we would delete the -wal journal file as well (here
          // and also in WipeDatabase).
          // XXX We should have something like WipeDatabaseNoQuota for this.
          // XXX Long term, we might even think about removing entire origin
          // directory because missing caches.sqlite while other files exist can
          // be interpreted as database corruption.
          CACHE_TRY(mozilla::dom::cache::DirectoryPaddingDeleteFile(
              *dir, DirPaddingFile::TMP_FILE));

          CACHE_TRY(mozilla::dom::cache::DirectoryPaddingDeleteFile(
              *dir, DirPaddingFile::FILE));

          CACHE_TRY_INSPECT(const auto& morgueDir,
                            CloneFileAndAppend(*dir, kMorgueDirectoryFilename));

          QuotaInfo dummy;
          CACHE_TRY(mozilla::dom::cache::RemoveNsIFileRecursively(
              dummy, *morgueDir,
              /* aTrackQuota */ false));

          return nsCOMPtr<nsIFile>{nullptr};
        }

        CACHE_TRY(OkIf(dirEntryKind == nsIFileKind::ExistsAsFile),
                  Err(NS_ERROR_FAILURE));

        return cachesSQLite;
      }()));

  // If the caches.sqlite doesn't exist, then padding files and morgue directory
  // should have been removed if they existed. We ignore the rest of known files
  // because we assume that they will be removed when a new database is created.
  // XXX Ensure the -wel file is removed if the caches.sqlite doesn't exist.
  CACHE_TRY(OkIf(!!cachesSQLiteFile), UsageInfo{});

  CACHE_TRY_INSPECT(
      const auto& paddingSize,
      ([dir, cachesSQLiteFile,
        &aOriginMetadata]() -> Result<int64_t, nsresult> {
        if (!DirectoryPaddingFileExists(*dir, DirPaddingFile::TMP_FILE)) {
          const auto& maybePaddingSize = [dir]() -> Maybe<int64_t> {
            CACHE_TRY_RETURN(DirectoryPaddingGet(*dir).map(Some<int64_t>),
                             Nothing{});
          }();

          if (maybePaddingSize) {
            return maybePaddingSize.ref();
          }
        }

        // If the temporary file still exists or failing to get the padding size
        // from the padding file, then we need to get the padding size from the
        // database and restore the padding file.
        CACHE_TRY_RETURN(
            GetPaddingSizeFromDB(*dir, *cachesSQLiteFile, aOriginMetadata));
      }()));

  CACHE_TRY_INSPECT(
      const auto& innerUsageInfo,
      ReduceUsageInfo(
          *dir, aCanceled,
          [&aCanceled](
              const nsCOMPtr<nsIFile>& file) -> Result<UsageInfo, nsresult> {
            CACHE_TRY_INSPECT(
                const auto& leafName,
                MOZ_TO_RESULT_INVOKE_TYPED(nsAutoString, file, GetLeafName));

            CACHE_TRY_INSPECT(const auto& dirEntryKind, GetDirEntryKind(*file));

            switch (dirEntryKind) {
              case nsIFileKind::ExistsAsDirectory:
                if (leafName.EqualsLiteral("morgue")) {
                  CACHE_TRY_RETURN(GetBodyUsage(*file, aCanceled));
                } else {
                  NS_WARNING("Unknown Cache directory found!");
                }

                break;

              case nsIFileKind::ExistsAsFile:
                // Ignore transient sqlite files and marker files
                if (leafName.EqualsLiteral("caches.sqlite-journal") ||
                    leafName.EqualsLiteral("caches.sqlite-shm") ||
                    leafName.Find("caches.sqlite-mj"_ns, false, 0, 0) == 0 ||
                    leafName.EqualsLiteral("context_open.marker")) {
                  break;
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
                  break;
                }

                NS_WARNING("Unknown Cache file found!");

                break;

              case nsIFileKind::DoesNotExist:
                // Ignore files that got removed externally while iterating.
                break;
            }

            return UsageInfo{};
          }));

  // FIXME: Separate file usage and database usage in OriginInfo so that the
  // workaround for treating padding file size as database usage can be removed.
  return UsageInfo{DatabaseUsageType(Some(paddingSize))} + innerUsageInfo;
}

nsresult CacheQuotaClient::InitOriginWithoutTracking(
    PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata,
    const AtomicBool& aCanceled) {
  AssertIsOnIOThread();

  // This is called when a storage/permanent/${origin}/cache directory exists.
  // Even though this shouldn't happen with a "good" profile, we shouldn't
  // return an error here, since that would cause origin initialization to fail.
  // We just warn and otherwise ignore that.
  UNKNOWN_FILE_WARNING(NS_LITERAL_STRING_FROM_CSTRING(DOMCACHE_DIRECTORY_NAME));
  return NS_OK;
}

Result<UsageInfo, nsresult> CacheQuotaClient::GetUsageForOrigin(
    PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata,
    const AtomicBool& aCanceled) {
  AssertIsOnIOThread();

  // We can't open the database at this point, since it can be already used by
  // the Cache IO thread. Use the cached value instead.

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  return quotaManager->GetUsageForClient(PERSISTENCE_TYPE_DEFAULT,
                                         aOriginMetadata, Client::DOMCACHE);
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

  CACHE_TRY(DirectoryPaddingInit(*aDirectory));

  return NS_OK;
}

nsresult CacheQuotaClient::RestorePaddingFileInternal(
    nsIFile* aBaseDir, mozIStorageConnection* aConn) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);
  MOZ_DIAGNOSTIC_ASSERT(aConn);

  CACHE_TRY_INSPECT(const int64_t& dummyPaddingSize,
                    DirectoryPaddingRestore(*aBaseDir, *aConn,
                                            /* aMustRestore */ true));
  Unused << dummyPaddingSize;

  return NS_OK;
}

nsresult CacheQuotaClient::WipePaddingFileInternal(const QuotaInfo& aQuotaInfo,
                                                   nsIFile* aBaseDir) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);

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
                    DirectoryPaddingGet(*aBaseDir).map(Some<int64_t>),
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

  CACHE_TRY(DirectoryPaddingDeleteFile(*aBaseDir, DirPaddingFile::FILE));

  // Remove temporary file if we have one.
  CACHE_TRY(DirectoryPaddingDeleteFile(*aBaseDir, DirPaddingFile::TMP_FILE));

  CACHE_TRY(DirectoryPaddingInit(*aBaseDir));

  return NS_OK;
}

CacheQuotaClient::~CacheQuotaClient() {
  AssertIsOnBackgroundThread();
  MOZ_DIAGNOSTIC_ASSERT(sInstance == this);

  sInstance = nullptr;
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

  CACHE_TRY(cacheQuotaClient->RestorePaddingFileInternal(aBaseDir, aConn));

  return NS_OK;
}

// static
nsresult WipePaddingFile(const QuotaInfo& aQuotaInfo, nsIFile* aBaseDir) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);

  RefPtr<CacheQuotaClient> cacheQuotaClient = CacheQuotaClient::Get();
  MOZ_DIAGNOSTIC_ASSERT(cacheQuotaClient);

  CACHE_TRY(cacheQuotaClient->WipePaddingFileInternal(aQuotaInfo, aBaseDir));

  return NS_OK;
}

}  // namespace mozilla::dom::cache
