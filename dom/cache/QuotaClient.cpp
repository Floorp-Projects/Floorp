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

namespace {

using mozilla::Atomic;
using mozilla::MutexAutoLock;
using mozilla::Some;
using mozilla::Unused;
using mozilla::dom::ContentParentId;
using mozilla::dom::cache::DirPaddingFile;
using mozilla::dom::cache::Manager;
using mozilla::dom::cache::QuotaInfo;
using mozilla::dom::quota::AssertIsOnIOThread;
using mozilla::dom::quota::Client;
using mozilla::dom::quota::PERSISTENCE_TYPE_DEFAULT;
using mozilla::dom::quota::PersistenceType;
using mozilla::dom::quota::QuotaManager;
using mozilla::dom::quota::UsageInfo;
using mozilla::ipc::AssertIsOnBackgroundThread;

static nsresult GetBodyUsage(nsIFile* aMorgueDir, const Atomic<bool>& aCanceled,
                             UsageInfo* aUsageInfo, const bool aInitializing) {
  AssertIsOnIOThread();

  nsCOMPtr<nsIDirectoryEnumerator> entries;
  nsresult rv = aMorgueDir->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIFile> bodyDir;
  while (NS_SUCCEEDED(rv = entries->GetNextFile(getter_AddRefs(bodyDir))) &&
         bodyDir && !aCanceled) {
    if (NS_WARN_IF(QuotaManager::IsShuttingDown())) {
      return NS_ERROR_ABORT;
    }
    bool isDir;
    rv = bodyDir->IsDirectory(&isDir);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!isDir) {
      QuotaInfo dummy;
      mozilla::DebugOnly<nsresult> result =
          RemoveNsIFile(dummy, bodyDir, /* aTrackQuota */ false);
      // Try to remove the unexpected files, and keep moving on even if it fails
      // because it might be created by virus or the operation system
      MOZ_ASSERT(NS_SUCCEEDED(result));
      continue;
    }

    const QuotaInfo dummy;
    const auto getUsage = [&aUsageInfo](nsIFile* bodyFile,
                                        const nsACString& leafName,
                                        bool& fileDeleted) {
      MOZ_DIAGNOSTIC_ASSERT(bodyFile);
      Unused << leafName;

      int64_t fileSize;
      nsresult rv = bodyFile->GetFileSize(&fileSize);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      MOZ_DIAGNOSTIC_ASSERT(fileSize >= 0);
      aUsageInfo->AppendToFileUsage(Some(fileSize));

      fileDeleted = false;

      return NS_OK;
    };
    rv = mozilla::dom::cache::BodyTraverseFiles(dummy, bodyDir, getUsage,
                                                /* aCanRemoveFiles */
                                                aInitializing,
                                                /* aTrackQuota */ false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

static nsresult LockedGetPaddingSizeFromDB(nsIFile* aDir,
                                           const nsACString& aGroup,
                                           const nsACString& aOrigin,
                                           int64_t* aPaddingSizeOut) {
  MOZ_DIAGNOSTIC_ASSERT(aDir);
  MOZ_DIAGNOSTIC_ASSERT(aPaddingSizeOut);

  *aPaddingSizeOut = 0;

  QuotaInfo quotaInfo;
  quotaInfo.mGroup = aGroup;
  quotaInfo.mOrigin = aOrigin;
  // quotaInfo.mDirectoryLockId must be -1 (which is default for new QuotaInfo)
  // because this method should only be called from QuotaClient::InitOrigin
  // (via QuotaClient::GetUsageForOriginInternal) when the temporary storage
  // hasn't been initialized yet. At that time, the in-memory objects (e.g.
  // OriginInfo) are only being created so it doesn't make sense to tunnel
  // quota information to TelemetryVFS to get corresponding QuotaObject instance
  // for the SQLite file).
  MOZ_DIAGNOSTIC_ASSERT(quotaInfo.mDirectoryLockId == -1);

  nsCOMPtr<mozIStorageConnection> conn;
  nsresult rv = mozilla::dom::cache::OpenDBConnection(quotaInfo, aDir,
                                                      getter_AddRefs(conn));
  if (rv == NS_ERROR_FILE_NOT_FOUND ||
      rv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST) {
    // Return NS_OK with size = 0 if both the db and padding file don't exist.
    // There is no other way to get the overall padding size of an origin.
    return NS_OK;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Make sure that the database has the latest schema before we try to read
  // from it. We have to do this because LockedGetPaddingSizeFromDB is called
  // by QuotaClient::GetUsageForOrigin which may run at any time (there's no
  // guarantee that SetupAction::RunSyncWithDBOnTarget already checked the
  // schema for the given origin).
  rv = mozilla::dom::cache::db::CreateOrMigrateSchema(conn);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int64_t paddingSize = 0;
  rv = mozilla::dom::cache::LockedDirectoryPaddingRestore(
      aDir, conn, /* aMustRestore */ false, &paddingSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aPaddingSizeOut = paddingSize;

  return rv;
}

}  // namespace

namespace mozilla {
namespace dom {
namespace cache {

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

nsresult CacheQuotaClient::InitOrigin(PersistenceType aPersistenceType,
                                      const nsACString& aGroup,
                                      const nsACString& aOrigin,
                                      const AtomicBool& aCanceled,
                                      UsageInfo* aUsageInfo,
                                      bool aForGetUsage) {
  AssertIsOnIOThread();

  // The QuotaManager passes a nullptr UsageInfo if there is no quota being
  // enforced against the origin.
  if (!aUsageInfo) {
    return NS_OK;
  }

  return GetUsageForOriginInternal(aPersistenceType, aGroup, aOrigin, aCanceled,
                                   aUsageInfo,
                                   /* aInitializing*/ true);
}

nsresult CacheQuotaClient::GetUsageForOrigin(PersistenceType aPersistenceType,
                                             const nsACString& aGroup,
                                             const nsACString& aOrigin,
                                             const AtomicBool& aCanceled,
                                             UsageInfo* aUsageInfo) {
  return GetUsageForOriginInternal(aPersistenceType, aGroup, aOrigin, aCanceled,
                                   aUsageInfo,
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

void CacheQuotaClient::AbortOperations(const nsACString& aOrigin) {
  AssertIsOnBackgroundThread();

  Manager::Abort(aOrigin);
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

void CacheQuotaClient::StartIdleMaintenance() {}

void CacheQuotaClient::StopIdleMaintenance() {}

void CacheQuotaClient::ShutdownWorkThreads() {
  AssertIsOnBackgroundThread();

  // spins the event loop and synchronously shuts down all Managers
  Manager::ShutdownAll();
}

nsresult CacheQuotaClient::UpgradeStorageFrom2_0To2_1(nsIFile* aDirectory) {
  AssertIsOnIOThread();
  MOZ_DIAGNOSTIC_ASSERT(aDirectory);

  MutexAutoLock lock(mDirPaddingFileMutex);

  nsresult rv = mozilla::dom::cache::LockedDirectoryPaddingInit(aDirectory);
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

  int64_t dummyPaddingSize;

  MutexAutoLock lock(mDirPaddingFileMutex);

  nsresult rv = mozilla::dom::cache::LockedDirectoryPaddingRestore(
      aBaseDir, aConn, /* aMustRestore */ true, &dummyPaddingSize);
  Unused << NS_WARN_IF(NS_FAILED(rv));

  return rv;
}

nsresult CacheQuotaClient::WipePaddingFileInternal(const QuotaInfo& aQuotaInfo,
                                                   nsIFile* aBaseDir) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);

  MutexAutoLock lock(mDirPaddingFileMutex);

  MOZ_ASSERT(mozilla::dom::cache::DirectoryPaddingFileExists(
      aBaseDir, DirPaddingFile::FILE));

  int64_t paddingSize = 0;
  bool temporaryPaddingFileExist =
      mozilla::dom::cache::DirectoryPaddingFileExists(aBaseDir,
                                                      DirPaddingFile::TMP_FILE);

  if (temporaryPaddingFileExist ||
      NS_WARN_IF(NS_FAILED(mozilla::dom::cache::LockedDirectoryPaddingGet(
          aBaseDir, &paddingSize)))) {
    // XXXtt: Maybe have a method in the QuotaManager to clean the usage under
    // the quota client and the origin.
    // There is nothing we can do to recover the file.
    NS_WARNING("Cannnot read padding size from file!");
    paddingSize = 0;
  }

  if (paddingSize > 0) {
    mozilla::dom::cache::DecreaseUsageForQuotaInfo(aQuotaInfo, paddingSize);
  }

  nsresult rv = mozilla::dom::cache::LockedDirectoryPaddingDeleteFile(
      aBaseDir, DirPaddingFile::FILE);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Remove temporary file if we have one.
  rv = mozilla::dom::cache::LockedDirectoryPaddingDeleteFile(
      aBaseDir, DirPaddingFile::TMP_FILE);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mozilla::dom::cache::LockedDirectoryPaddingInit(aBaseDir);
  Unused << NS_WARN_IF(NS_FAILED(rv));

  return rv;
}

CacheQuotaClient::~CacheQuotaClient() {
  AssertIsOnBackgroundThread();
  MOZ_DIAGNOSTIC_ASSERT(sInstance == this);

  sInstance = nullptr;
}

nsresult CacheQuotaClient::GetUsageForOriginInternal(
    PersistenceType aPersistenceType, const nsACString& aGroup,
    const nsACString& aOrigin, const AtomicBool& aCanceled,
    UsageInfo* aUsageInfo, const bool aInitializing) {
  AssertIsOnIOThread();
  MOZ_DIAGNOSTIC_ASSERT(aUsageInfo);
#ifndef NIGHTLY_BUILD
  Unused << aInitializing;
#endif

  QuotaManager* qm = QuotaManager::Get();
  MOZ_DIAGNOSTIC_ASSERT(qm);

  nsCOMPtr<nsIFile> dir;
  nsresult rv =
      qm->GetDirectoryForOrigin(aPersistenceType, aOrigin, getter_AddRefs(dir));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    REPORT_TELEMETRY_ERR_IN_INIT(aInitializing, kQuotaExternalError,
                                 Cache_GetDirForOri);
    return rv;
  }

  rv = dir->Append(NS_LITERAL_STRING(DOMCACHE_DIRECTORY_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    REPORT_TELEMETRY_ERR_IN_INIT(aInitializing, kQuotaExternalError,
                                 Cache_Append);
    return rv;
  }

  bool useCachedValue = false;

  int64_t paddingSize = 0;
  {
    // If the tempoary file still exists after locking, it means the previous
    // action failed, so restore the padding file.
    MutexAutoLock lock(mDirPaddingFileMutex);

    if (mozilla::dom::cache::DirectoryPaddingFileExists(
            dir, DirPaddingFile::TMP_FILE) ||
        NS_WARN_IF(NS_FAILED(mozilla::dom::cache::LockedDirectoryPaddingGet(
            dir, &paddingSize)))) {
      if (aInitializing) {
        rv = LockedGetPaddingSizeFromDB(dir, aGroup, aOrigin, &paddingSize);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          REPORT_TELEMETRY_ERR_IN_INIT(aInitializing, kQuotaInternalError,
                                       Cache_GetPaddingSize);
          return rv;
        }
      } else {
        // We can't open the database at this point, since it can be already
        // used by Cache IO thread. Use the cached value instead. (In theory,
        // we could check if the database is actually used by Cache IO thread
        // at this moment, but it's probably not worth additional complexity.)

        useCachedValue = true;
      }
    }
  }

  if (useCachedValue) {
    uint64_t usage;
    if (qm->GetUsageForClient(PERSISTENCE_TYPE_DEFAULT, aGroup, aOrigin,
                              Client::DOMCACHE, usage)) {
      aUsageInfo->AppendToDatabaseUsage(Some(usage));
    }

    return NS_OK;
  }

  aUsageInfo->AppendToFileUsage(Some(paddingSize));

  nsCOMPtr<nsIDirectoryEnumerator> entries;
  rv = dir->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    REPORT_TELEMETRY_ERR_IN_INIT(aInitializing, kQuotaExternalError,
                                 Cache_GetDirEntries);
    return rv;
  }

  nsCOMPtr<nsIFile> file;
  while (NS_SUCCEEDED(rv = entries->GetNextFile(getter_AddRefs(file))) &&
         file && !aCanceled) {
    if (NS_WARN_IF(QuotaManager::IsShuttingDown())) {
      return NS_ERROR_ABORT;
    }

    nsAutoString leafName;
    rv = file->GetLeafName(leafName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      REPORT_TELEMETRY_ERR_IN_INIT(aInitializing, kQuotaExternalError,
                                   Cache_GetLeafName);
      return rv;
    }

    bool isDir;
    rv = file->IsDirectory(&isDir);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      REPORT_TELEMETRY_ERR_IN_INIT(aInitializing, kQuotaExternalError,
                                   Cache_IsDirectory);
      return rv;
    }

    if (isDir) {
      if (leafName.EqualsLiteral("morgue")) {
        rv = GetBodyUsage(file, aCanceled, aUsageInfo, aInitializing);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          if (rv != NS_ERROR_ABORT) {
            REPORT_TELEMETRY_ERR_IN_INIT(aInitializing, kQuotaExternalError,
                                         Cache_GetBodyUsage);
          }
          return rv;
        }
      } else {
        NS_WARNING("Unknown Cache directory found!");
      }

      continue;
    }

    // Ignore transient sqlite files and marker files
    if (leafName.EqualsLiteral("caches.sqlite-journal") ||
        leafName.EqualsLiteral("caches.sqlite-shm") ||
        leafName.Find(NS_LITERAL_CSTRING("caches.sqlite-mj"), false, 0, 0) ==
            0 ||
        leafName.EqualsLiteral("context_open.marker")) {
      continue;
    }

    if (leafName.EqualsLiteral("caches.sqlite") ||
        leafName.EqualsLiteral("caches.sqlite-wal")) {
      int64_t fileSize;
      rv = file->GetFileSize(&fileSize);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        REPORT_TELEMETRY_ERR_IN_INIT(aInitializing, kQuotaExternalError,
                                     Cache_GetFileSize);
        return rv;
      }
      MOZ_DIAGNOSTIC_ASSERT(fileSize >= 0);

      aUsageInfo->AppendToDatabaseUsage(Some(fileSize));
      continue;
    }

    // Ignore directory padding file
    if (leafName.EqualsLiteral(PADDING_FILE_NAME) ||
        leafName.EqualsLiteral(PADDING_TMP_FILE_NAME)) {
      continue;
    }

    NS_WARNING("Unknown Cache file found!");
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
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
}  // namespace cache
}  // namespace dom
}  // namespace mozilla
