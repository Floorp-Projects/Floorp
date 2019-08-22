/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/QuotaClient.h"

#include "DBAction.h"
#include "FileUtils.h"
#include "mozilla/dom/cache/Manager.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "nsIFile.h"
#include "nsISimpleEnumerator.h"
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
using mozilla::dom::quota::PersistenceType;
using mozilla::dom::quota::QuotaManager;
using mozilla::dom::quota::UsageInfo;
using mozilla::ipc::AssertIsOnBackgroundThread;

static nsresult GetBodyUsage(nsIFile* aDir, const Atomic<bool>& aCanceled,
                             UsageInfo* aUsageInfo) {
  AssertIsOnIOThread();

  nsCOMPtr<nsIDirectoryEnumerator> entries;
  nsresult rv = aDir->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIFile> file;
  while (NS_SUCCEEDED(rv = entries->GetNextFile(getter_AddRefs(file))) &&
         file && !aCanceled) {
    bool isDir;
    rv = file->IsDirectory(&isDir);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (isDir) {
      rv = GetBodyUsage(file, aCanceled, aUsageInfo);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      continue;
    }

    int64_t fileSize;
    rv = file->GetFileSize(&fileSize);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    MOZ_DIAGNOSTIC_ASSERT(fileSize >= 0);

    aUsageInfo->AppendToFileUsage(Some(fileSize));
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

  nsCOMPtr<mozIStorageConnection> conn;
  QuotaInfo quotaInfo;
  quotaInfo.mGroup = aGroup;
  quotaInfo.mOrigin = aOrigin;
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

class CacheQuotaClient final : public Client {
  static CacheQuotaClient* sInstance;

 public:
  CacheQuotaClient()
      : mDirPaddingFileMutex("DOMCacheQuotaClient.mDirPaddingFileMutex") {
    AssertIsOnBackgroundThread();
    MOZ_DIAGNOSTIC_ASSERT(!sInstance);
    sInstance = this;
  }

  static CacheQuotaClient* Get() {
    MOZ_DIAGNOSTIC_ASSERT(sInstance);
    return sInstance;
  }

  virtual Type GetType() override { return DOMCACHE; }

  virtual nsresult InitOrigin(PersistenceType aPersistenceType,
                              const nsACString& aGroup,
                              const nsACString& aOrigin,
                              const AtomicBool& aCanceled,
                              UsageInfo* aUsageInfo,
                              bool aForGetUsage) override {
    AssertIsOnIOThread();

    // The QuotaManager passes a nullptr UsageInfo if there is no quota being
    // enforced against the origin.
    if (!aUsageInfo) {
      return NS_OK;
    }

    return GetUsageForOriginInternal(aPersistenceType, aGroup, aOrigin,
                                     aCanceled, aUsageInfo,
                                     /* aInitializing*/ true);
  }

  virtual nsresult GetUsageForOrigin(PersistenceType aPersistenceType,
                                     const nsACString& aGroup,
                                     const nsACString& aOrigin,
                                     const AtomicBool& aCanceled,
                                     UsageInfo* aUsageInfo) override {
    return GetUsageForOriginInternal(aPersistenceType, aGroup, aOrigin,
                                     aCanceled, aUsageInfo,
                                     /* aInitializing*/ false);
  }

  virtual void OnOriginClearCompleted(PersistenceType aPersistenceType,
                                      const nsACString& aOrigin) override {
    // Nothing to do here.
  }

  virtual void ReleaseIOThreadObjects() override {
    // Nothing to do here as the Context handles cleaning everything up
    // automatically.
  }

  virtual void AbortOperations(const nsACString& aOrigin) override {
    AssertIsOnBackgroundThread();

    Manager::Abort(aOrigin);
  }

  virtual void AbortOperationsForProcess(
      ContentParentId aContentParentId) override {
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

  virtual void StartIdleMaintenance() override {}

  virtual void StopIdleMaintenance() override {}

  virtual void ShutdownWorkThreads() override {
    AssertIsOnBackgroundThread();

    // spins the event loop and synchronously shuts down all Managers
    Manager::ShutdownAll();
  }

  nsresult UpgradeStorageFrom2_0To2_1(nsIFile* aDirectory) override {
    AssertIsOnIOThread();
    MOZ_DIAGNOSTIC_ASSERT(aDirectory);

    MutexAutoLock lock(mDirPaddingFileMutex);

    nsresult rv = mozilla::dom::cache::LockedDirectoryPaddingInit(aDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return rv;
  }

  // static
  template <typename Callable>
  nsresult MaybeUpdatePaddingFileInternal(nsIFile* aBaseDir,
                                          mozIStorageConnection* aConn,
                                          const int64_t aIncreaseSize,
                                          const int64_t aDecreaseSize,
                                          Callable aCommitHook) {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_DIAGNOSTIC_ASSERT(aBaseDir);
    MOZ_DIAGNOSTIC_ASSERT(aConn);
    MOZ_DIAGNOSTIC_ASSERT(aIncreaseSize >= 0);
    MOZ_DIAGNOSTIC_ASSERT(aDecreaseSize >= 0);

    nsresult rv;

    // Temporary should be removed at the end of each action. If not, it means
    // the failure happened.
    bool temporaryPaddingFileExist =
        mozilla::dom::cache::DirectoryPaddingFileExists(
            aBaseDir, DirPaddingFile::TMP_FILE);

    if (aIncreaseSize == aDecreaseSize && !temporaryPaddingFileExist) {
      // Early return here, since most cache actions won't modify padding size.
      rv = aCommitHook();
      Unused << NS_WARN_IF(NS_FAILED(rv));
      return rv;
    }

    {
      MutexAutoLock lock(mDirPaddingFileMutex);
      rv = mozilla::dom::cache::LockedUpdateDirectoryPaddingFile(
          aBaseDir, aConn, aIncreaseSize, aDecreaseSize,
          temporaryPaddingFileExist);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        // Don't delete the temporary padding file here to force the next action
        // recalculate the padding size.
        return rv;
      }

      rv = aCommitHook();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        // Don't delete the temporary padding file here to force the next action
        // recalculate the padding size.
        return rv;
      }

      rv = mozilla::dom::cache::LockedDirectoryPaddingFinalizeWrite(aBaseDir);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        // Force restore file next time.
        Unused << mozilla::dom::cache::LockedDirectoryPaddingDeleteFile(
            aBaseDir, DirPaddingFile::FILE);

        // Ensure that we are able to force the padding file to be restored.
        MOZ_ASSERT(mozilla::dom::cache::DirectoryPaddingFileExists(
            aBaseDir, DirPaddingFile::TMP_FILE));

        // Since both the body file and header have been stored in the
        // file-system, just make the action be resolve and let the padding file
        // be restored in the next action.
        rv = NS_OK;
      }
    }

    return rv;
  }

  // static
  nsresult RestorePaddingFileInternal(nsIFile* aBaseDir,
                                      mozIStorageConnection* aConn) {
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

  // static
  nsresult WipePaddingFileInternal(const QuotaInfo& aQuotaInfo,
                                   nsIFile* aBaseDir) {
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_DIAGNOSTIC_ASSERT(aBaseDir);

    MutexAutoLock lock(mDirPaddingFileMutex);

    MOZ_ASSERT(mozilla::dom::cache::DirectoryPaddingFileExists(
        aBaseDir, DirPaddingFile::FILE));

    int64_t paddingSize = 0;
    bool temporaryPaddingFileExist =
        mozilla::dom::cache::DirectoryPaddingFileExists(
            aBaseDir, DirPaddingFile::TMP_FILE);

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

 private:
  ~CacheQuotaClient() {
    AssertIsOnBackgroundThread();
    MOZ_DIAGNOSTIC_ASSERT(sInstance == this);

    sInstance = nullptr;
  }

  nsresult GetUsageForOriginInternal(PersistenceType aPersistenceType,
                                     const nsACString& aGroup,
                                     const nsACString& aOrigin,
                                     const AtomicBool& aCanceled,
                                     UsageInfo* aUsageInfo,
                                     const bool aInitializing) {
    AssertIsOnIOThread();
    MOZ_DIAGNOSTIC_ASSERT(aUsageInfo);
#ifndef NIGHTLY_BUILD
    Unused << aInitializing;
#endif

    QuotaManager* qm = QuotaManager::Get();
    MOZ_DIAGNOSTIC_ASSERT(qm);

    nsCOMPtr<nsIFile> dir;
    nsresult rv = qm->GetDirectoryForOrigin(aPersistenceType, aOrigin,
                                            getter_AddRefs(dir));
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

    int64_t paddingSize = 0;
    {
      // If the tempoary file still exists after locking, it means the previous
      // action fails, so restore the padding file.
      MutexAutoLock lock(mDirPaddingFileMutex);

      if (mozilla::dom::cache::DirectoryPaddingFileExists(
              dir, DirPaddingFile::TMP_FILE) ||
          NS_WARN_IF(NS_FAILED(mozilla::dom::cache::LockedDirectoryPaddingGet(
              dir, &paddingSize)))) {
        rv = LockedGetPaddingSizeFromDB(dir, aGroup, aOrigin, &paddingSize);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          REPORT_TELEMETRY_ERR_IN_INIT(aInitializing, kQuotaInternalError,
                                       Cache_GetPaddingSize);
          return rv;
        }
      }
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
          rv = GetBodyUsage(file, aCanceled, aUsageInfo);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            REPORT_TELEMETRY_ERR_IN_INIT(aInitializing, kQuotaExternalError,
                                         Cache_GetBodyUsage);
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

    return NS_OK;
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CacheQuotaClient, override)

  // Mutex lock to protect directroy padding files. It should only be acquired
  // in DOM Cache IO threads and Quota IO thread.
  mozilla::Mutex mDirPaddingFileMutex;
};

// static
CacheQuotaClient* CacheQuotaClient::sInstance = nullptr;

}  // namespace

namespace mozilla {
namespace dom {
namespace cache {

// static
already_AddRefed<quota::Client> CreateQuotaClient() {
  AssertIsOnBackgroundThread();

  RefPtr<CacheQuotaClient> ref = new CacheQuotaClient();
  return ref.forget();
}

// static
template <typename Callable>
nsresult MaybeUpdatePaddingFile(nsIFile* aBaseDir, mozIStorageConnection* aConn,
                                const int64_t aIncreaseSize,
                                const int64_t aDecreaseSize,
                                Callable aCommitHook) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(aBaseDir);
  MOZ_DIAGNOSTIC_ASSERT(aConn);
  MOZ_DIAGNOSTIC_ASSERT(aIncreaseSize >= 0);
  MOZ_DIAGNOSTIC_ASSERT(aDecreaseSize >= 0);

  RefPtr<CacheQuotaClient> cacheQuotaClient = CacheQuotaClient::Get();
  MOZ_DIAGNOSTIC_ASSERT(cacheQuotaClient);

  nsresult rv = cacheQuotaClient->MaybeUpdatePaddingFileInternal(
      aBaseDir, aConn, aIncreaseSize, aDecreaseSize, aCommitHook);
  Unused << NS_WARN_IF(NS_FAILED(rv));

  return rv;
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
