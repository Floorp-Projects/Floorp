/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActorsParent.h"

// Local includes
#include "Flatten.h"
#include "FirstInitializationAttemptsImpl.h"
#include "OriginScope.h"
#include "QuotaCommon.h"
#include "QuotaManager.h"
#include "QuotaObject.h"
#include "UsageInfo.h"

// Global includes
#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <new>
#include <numeric>
#include <tuple>
#include <type_traits>
#include <utility>
#include "DirectoryLockImpl.h"
#include "ErrorList.h"
#include "MainThreadUtils.h"
#include "mozIStorageAsyncConnection.h"
#include "mozIStorageConnection.h"
#include "mozIStorageService.h"
#include "mozIStorageStatement.h"
#include "mozStorageCID.h"
#include "mozStorageHelper.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/CondVar.h"
#include "mozilla/InitializedOnce.h"
#include "mozilla/Logging.h"
#include "mozilla/MacroForEach.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "mozilla/NotNull.h"
#include "mozilla/OriginAttributes.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Result.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryHistogramEnums.h"
#include "mozilla/TextUtils.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/Variant.h"
#include "mozilla/dom/FlippedOnce.h"
#include "mozilla/dom/LocalStorageCommon.h"
#include "mozilla/dom/StorageActivityService.h"
#include "mozilla/dom/StorageDBUpdater.h"
#include "mozilla/dom/StorageTypeBinding.h"
#include "mozilla/dom/cache/QuotaClient.h"
#include "mozilla/dom/indexedDB/ActorsParent.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/dom/localstorage/ActorsParent.h"
#include "mozilla/dom/QMResultInlines.h"
#include "mozilla/dom/quota/CheckedUnsafePtr.h"
#include "mozilla/dom/quota/Client.h"
#include "mozilla/dom/quota/DirectoryLock.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "mozilla/dom/quota/PQuota.h"
#include "mozilla/dom/quota/PQuotaParent.h"
#include "mozilla/dom/quota/PQuotaRequest.h"
#include "mozilla/dom/quota/PQuotaRequestParent.h"
#include "mozilla/dom/quota/PQuotaUsageRequest.h"
#include "mozilla/dom/quota/PQuotaUsageRequestParent.h"
#include "mozilla/dom/quota/ScopedLogExtraInfo.h"
#include "mozilla/dom/simpledb/ActorsParent.h"
#include "mozilla/fallible.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/net/MozURL.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsBaseHashtable.h"
#include "nsCOMPtr.h"
#include "nsCRTGlue.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsClassHashtable.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsTHashMap.h"
#include "nsDebug.h"
#include "nsDirectoryServiceUtils.h"
#include "nsError.h"
#include "nsHashKeys.h"
#include "nsIBinaryInputStream.h"
#include "nsIBinaryOutputStream.h"
#include "nsIConsoleService.h"
#include "nsIDirectoryEnumerator.h"
#include "nsIEventTarget.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsIInputStream.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIOutputStream.h"
#include "nsIPlatformInfo.h"
#include "nsIPrincipal.h"
#include "nsIRunnable.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsISupports.h"
#include "nsISupportsPrimitives.h"
#include "nsIThread.h"
#include "nsITimer.h"
#include "nsIURI.h"
#include "nsIWidget.h"
#include "nsLiteralString.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsStringFlags.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsTHashtable.h"
#include "nsTLiteralString.h"
#include "nsTPromiseFlatString.h"
#include "nsTStringRepr.h"
#include "nsThreadUtils.h"
#include "nsURLHelper.h"
#include "nsXPCOM.h"
#include "nsXPCOMCID.h"
#include "nsXULAppAPI.h"
#include "prinrval.h"
#include "prio.h"
#include "prthread.h"
#include "prtime.h"

#define DISABLE_ASSERTS_FOR_FUZZING 0

#if DISABLE_ASSERTS_FOR_FUZZING
#  define ASSERT_UNLESS_FUZZING(...) \
    do {                             \
    } while (0)
#else
#  define ASSERT_UNLESS_FUZZING(...) MOZ_ASSERT(false, __VA_ARGS__)
#endif

// As part of bug 1536596 in order to identify the remaining sources of
// principal info inconsistencies, we have added anonymized crash logging and
// are temporarily making these checks occur on both debug and optimized
// nightly, dev-edition, and early beta builds through use of
// EARLY_BETA_OR_EARLIER during Firefox 82.  The plan is to return this
// condition to MOZ_DIAGNOSTIC_ASSERT_ENABLED during Firefox 84 at the latest.
// The analysis and disabling is tracked by bug 1536596.

#ifdef EARLY_BETA_OR_EARLIER
#  define QM_PRINCIPALINFO_VERIFICATION_ENABLED
#endif

// The amount of time, in milliseconds, that our IO thread will stay alive
// after the last event it processes.
#define DEFAULT_THREAD_TIMEOUT_MS 30000

/**
 * If shutdown takes this long, kill actors of a quota client, to avoid reaching
 * the crash timeout.
 */
#define SHUTDOWN_FORCE_KILL_TIMEOUT_MS 5000

/**
 * Automatically crash the browser if shutdown of a quota client takes this
 * long. We've chosen a value that is long enough that it is unlikely for the
 * problem to be falsely triggered by slow system I/O.  We've also chosen a
 * value long enough so that automated tests should time out and fail if
 * shutdown of a quota client takes too long.  Also, this value is long enough
 * so that testers can notice the timeout; we want to know about the timeouts,
 * not hide them. On the other hand this value is less than 60 seconds which is
 * used by nsTerminator to crash a hung main process.
 */
#define SHUTDOWN_FORCE_CRASH_TIMEOUT_MS 45000

// profile-before-change, when we need to shut down quota manager
#define PROFILE_BEFORE_CHANGE_QM_OBSERVER_ID "profile-before-change-qm"

#define KB *1024ULL
#define MB *1024ULL KB
#define GB *1024ULL MB

namespace mozilla::dom::quota {

using namespace mozilla::ipc;
using mozilla::net::MozURL;

// We want profiles to be platform-independent so we always need to replace
// the same characters on every platform. Windows has the most extensive set
// of illegal characters so we use its FILE_ILLEGAL_CHARACTERS and
// FILE_PATH_SEPARATOR.
const char QuotaManager::kReplaceChars[] = CONTROL_CHARACTERS "/:*?\"<>|\\";

namespace {

template <typename T>
void AssertNoOverflow(uint64_t aDest, T aArg);

/*******************************************************************************
 * Constants
 ******************************************************************************/

const uint32_t kSQLitePageSizeOverride = 512;

// Important version history:
// - Bug 1290481 bumped our schema from major.minor 2.0 to 3.0 in Firefox 57
//   which caused Firefox 57 release concerns because the major schema upgrade
//   means anyone downgrading to Firefox 56 will experience a non-operational
//   QuotaManager and all of its clients.
// - Bug 1404344 got very concerned about that and so we decided to effectively
//   rename 3.0 to 2.1, effective in Firefox 57.  This works because post
//   storage.sqlite v1.0, QuotaManager doesn't care about minor storage version
//   increases.  It also works because all the upgrade did was give the DOM
//   Cache API QuotaClient an opportunity to create its newly added .padding
//   files during initialization/upgrade, which isn't functionally necessary as
//   that can be done on demand.

// Major storage version. Bump for backwards-incompatible changes.
// (The next major version should be 4 to distinguish from the Bug 1290481
// downgrade snafu.)
const uint32_t kMajorStorageVersion = 2;

// Minor storage version. Bump for backwards-compatible changes.
const uint32_t kMinorStorageVersion = 3;

// The storage version we store in the SQLite database is a (signed) 32-bit
// integer. The major version is left-shifted 16 bits so the max value is
// 0xFFFF. The minor version occupies the lower 16 bits and its max is 0xFFFF.
static_assert(kMajorStorageVersion <= 0xFFFF,
              "Major version needs to fit in 16 bits.");
static_assert(kMinorStorageVersion <= 0xFFFF,
              "Minor version needs to fit in 16 bits.");

const int32_t kStorageVersion =
    int32_t((kMajorStorageVersion << 16) + kMinorStorageVersion);

// See comments above about why these are a thing.
const int32_t kHackyPreDowngradeStorageVersion = int32_t((3 << 16) + 0);
const int32_t kHackyPostDowngradeStorageVersion = int32_t((2 << 16) + 1);

static_assert(static_cast<uint32_t>(StorageType::Persistent) ==
                  static_cast<uint32_t>(PERSISTENCE_TYPE_PERSISTENT),
              "Enum values should match.");

static_assert(static_cast<uint32_t>(StorageType::Temporary) ==
                  static_cast<uint32_t>(PERSISTENCE_TYPE_TEMPORARY),
              "Enum values should match.");

static_assert(static_cast<uint32_t>(StorageType::Default) ==
                  static_cast<uint32_t>(PERSISTENCE_TYPE_DEFAULT),
              "Enum values should match.");

const char kChromeOrigin[] = "chrome";
const char kAboutHomeOriginPrefix[] = "moz-safe-about:home";
const char kIndexedDBOriginPrefix[] = "indexeddb://";
const char kResourceOriginPrefix[] = "resource://";

constexpr auto kStorageName = u"storage"_ns;
constexpr auto kSQLiteSuffix = u".sqlite"_ns;

#define INDEXEDDB_DIRECTORY_NAME u"indexedDB"
#define PERSISTENT_DIRECTORY_NAME u"persistent"
#define PERMANENT_DIRECTORY_NAME u"permanent"
#define TEMPORARY_DIRECTORY_NAME u"temporary"
#define DEFAULT_DIRECTORY_NAME u"default"

// The name of the file that we use to load/save the last access time of an
// origin.
// XXX We should get rid of old metadata files at some point, bug 1343576.
#define METADATA_FILE_NAME u".metadata"
#define METADATA_TMP_FILE_NAME u".metadata-tmp"
#define METADATA_V2_FILE_NAME u".metadata-v2"
#define METADATA_V2_TMP_FILE_NAME u".metadata-v2-tmp"

#define WEB_APPS_STORE_FILE_NAME u"webappsstore.sqlite"
#define LS_ARCHIVE_FILE_NAME u"ls-archive.sqlite"
#define LS_ARCHIVE_TMP_FILE_NAME u"ls-archive-tmp.sqlite"

const int32_t kLocalStorageArchiveVersion = 4;

const char kProfileDoChangeTopic[] = "profile-do-change";

const int32_t kCacheVersion = 2;

/******************************************************************************
 * SQLite functions
 ******************************************************************************/

int32_t MakeStorageVersion(uint32_t aMajorStorageVersion,
                           uint32_t aMinorStorageVersion) {
  return int32_t((aMajorStorageVersion << 16) + aMinorStorageVersion);
}

uint32_t GetMajorStorageVersion(int32_t aStorageVersion) {
  return uint32_t(aStorageVersion >> 16);
}

nsresult CreateTables(mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  // Table `database`
  QM_TRY(
      aConnection->ExecuteSimpleSQL("CREATE TABLE database"
                                    "( cache_version INTEGER NOT NULL DEFAULT 0"
                                    ");"_ns));

#ifdef DEBUG
  {
    QM_TRY_INSPECT(const int32_t& storageVersion,
                   MOZ_TO_RESULT_INVOKE(aConnection, GetSchemaVersion));

    MOZ_ASSERT(storageVersion == 0);
  }
#endif

  QM_TRY(aConnection->SetSchemaVersion(kStorageVersion));

  return NS_OK;
}

Result<int32_t, nsresult> LoadCacheVersion(mozIStorageConnection& aConnection) {
  AssertIsOnIOThread();

  QM_TRY_INSPECT(const auto& stmt,
                 CreateAndExecuteSingleStepStatement<
                     SingleStepResult::ReturnNullIfNoResult>(
                     aConnection, "SELECT cache_version FROM database"_ns));

  QM_TRY(OkIf(stmt), Err(NS_ERROR_FILE_CORRUPTED));

  QM_TRY_RETURN(MOZ_TO_RESULT_INVOKE(stmt, GetInt32, 0));
}

nsresult SaveCacheVersion(mozIStorageConnection& aConnection,
                          int32_t aVersion) {
  AssertIsOnIOThread();

  QM_TRY_INSPECT(
      const auto& stmt,
      MOZ_TO_RESULT_INVOKE_TYPED(
          nsCOMPtr<mozIStorageStatement>, aConnection, CreateStatement,
          "UPDATE database SET cache_version = :version;"_ns));

  QM_TRY(stmt->BindInt32ByName("version"_ns, aVersion));

  QM_TRY(stmt->Execute());

  return NS_OK;
}

nsresult CreateCacheTables(mozIStorageConnection& aConnection) {
  AssertIsOnIOThread();

  // Table `cache`
  QM_TRY(
      aConnection.ExecuteSimpleSQL("CREATE TABLE cache"
                                   "( valid INTEGER NOT NULL DEFAULT 0"
                                   ", build_id TEXT NOT NULL DEFAULT ''"
                                   ");"_ns));

  // Table `repository`
  QM_TRY(
      aConnection.ExecuteSimpleSQL("CREATE TABLE repository"
                                   "( id INTEGER PRIMARY KEY"
                                   ", name TEXT NOT NULL"
                                   ");"_ns));

  // Table `origin`
  QM_TRY(
      aConnection.ExecuteSimpleSQL("CREATE TABLE origin"
                                   "( repository_id INTEGER NOT NULL"
                                   ", suffix TEXT"
                                   ", group_ TEXT NOT NULL"
                                   ", origin TEXT NOT NULL"
                                   ", client_usages TEXT NOT NULL"
                                   ", usage INTEGER NOT NULL"
                                   ", last_access_time INTEGER NOT NULL"
                                   ", accessed INTEGER NOT NULL"
                                   ", persisted INTEGER NOT NULL"
                                   ", PRIMARY KEY (repository_id, origin)"
                                   ", FOREIGN KEY (repository_id) "
                                   "REFERENCES repository(id) "
                                   ");"_ns));

#ifdef DEBUG
  {
    QM_TRY_INSPECT(const int32_t& cacheVersion, LoadCacheVersion(aConnection));
    MOZ_ASSERT(cacheVersion == 0);
  }
#endif

  QM_TRY(SaveCacheVersion(aConnection, kCacheVersion));

  return NS_OK;
}

nsresult InvalidateCache(mozIStorageConnection& aConnection) {
  AssertIsOnIOThread();

  static constexpr auto kDeleteCacheQuery = "DELETE FROM origin;"_ns;
  static constexpr auto kSetInvalidFlagQuery = "UPDATE cache SET valid = 0"_ns;

  QM_TRY(QM_OR_ELSE_WARN(
      // Expression.
      ([&]() -> Result<Ok, nsresult> {
        mozStorageTransaction transaction(&aConnection, false);

        QM_TRY(transaction.Start());
        QM_TRY(aConnection.ExecuteSimpleSQL(kDeleteCacheQuery));
        QM_TRY(aConnection.ExecuteSimpleSQL(kSetInvalidFlagQuery));
        QM_TRY(transaction.Commit());

        return Ok{};
      }()),
      // Fallback.
      ([&](const nsresult rv) -> Result<Ok, nsresult> {
        QM_TRY(aConnection.ExecuteSimpleSQL(kSetInvalidFlagQuery));

        return Ok{};
      })));
  return NS_OK;
}

nsresult UpgradeCacheFrom1To2(mozIStorageConnection& aConnection) {
  AssertIsOnIOThread();

  QM_TRY(aConnection.ExecuteSimpleSQL(
      "ALTER TABLE origin ADD COLUMN suffix TEXT"_ns));

  QM_TRY(InvalidateCache(aConnection));

#ifdef DEBUG
  {
    QM_TRY_INSPECT(const int32_t& cacheVersion, LoadCacheVersion(aConnection));

    MOZ_ASSERT(cacheVersion == 1);
  }
#endif

  QM_TRY(SaveCacheVersion(aConnection, 2));

  return NS_OK;
}

Result<bool, nsresult> MaybeCreateOrUpgradeCache(
    mozIStorageConnection& aConnection) {
  bool cacheUsable = true;

  QM_TRY_UNWRAP(int32_t cacheVersion, LoadCacheVersion(aConnection));

  if (cacheVersion > kCacheVersion) {
    cacheUsable = false;
  } else if (cacheVersion != kCacheVersion) {
    const bool newCache = !cacheVersion;

    mozStorageTransaction transaction(
        &aConnection, false, mozIStorageConnection::TRANSACTION_IMMEDIATE);

    QM_TRY(transaction.Start());

    if (newCache) {
      QM_TRY(CreateCacheTables(aConnection));

#ifdef DEBUG
      {
        QM_TRY_INSPECT(const int32_t& cacheVersion,
                       LoadCacheVersion(aConnection));
        MOZ_ASSERT(cacheVersion == kCacheVersion);
      }
#endif

      QM_TRY(aConnection.ExecuteSimpleSQL(
          nsLiteralCString("INSERT INTO cache (valid, build_id) "
                           "VALUES (0, '')")));

      nsCOMPtr<mozIStorageStatement> insertStmt;

      for (const PersistenceType persistenceType : kAllPersistenceTypes) {
        if (insertStmt) {
          MOZ_ALWAYS_SUCCEEDS(insertStmt->Reset());
        } else {
          QM_TRY_UNWRAP(insertStmt, MOZ_TO_RESULT_INVOKE_TYPED(
                                        nsCOMPtr<mozIStorageStatement>,
                                        aConnection, CreateStatement,
                                        "INSERT INTO repository (id, name) "
                                        "VALUES (:id, :name)"_ns));
        }

        QM_TRY(insertStmt->BindInt32ByName("id"_ns, persistenceType));

        QM_TRY(insertStmt->BindUTF8StringByName(
            "name"_ns, PersistenceTypeToString(persistenceType)));

        QM_TRY(insertStmt->Execute());
      }
    } else {
      // This logic needs to change next time we change the cache!
      static_assert(kCacheVersion == 2,
                    "Upgrade function needed due to cache version increase.");

      while (cacheVersion != kCacheVersion) {
        if (cacheVersion == 1) {
          QM_TRY(UpgradeCacheFrom1To2(aConnection));
        } else {
          QM_FAIL(Err(NS_ERROR_FAILURE), []() {
            QM_WARNING(
                "Unable to initialize cache, no upgrade path is "
                "available!");
          });
        }

        QM_TRY_UNWRAP(cacheVersion, LoadCacheVersion(aConnection));
      }

      MOZ_ASSERT(cacheVersion == kCacheVersion);
    }

    QM_TRY(transaction.Commit());
  }

  return cacheUsable;
}

Result<nsCOMPtr<mozIStorageConnection>, nsresult> CreateWebAppsStoreConnection(
    nsIFile& aWebAppsStoreFile, mozIStorageService& aStorageService) {
  AssertIsOnIOThread();

  // Check if the old database exists at all.
  QM_TRY_INSPECT(const bool& exists,
                 MOZ_TO_RESULT_INVOKE(aWebAppsStoreFile, Exists));

  if (!exists) {
    // webappsstore.sqlite doesn't exist, return a null connection.
    return nsCOMPtr<mozIStorageConnection>{};
  }

  QM_TRY_INSPECT(const bool& isDirectory,
                 MOZ_TO_RESULT_INVOKE(aWebAppsStoreFile, IsDirectory));

  if (isDirectory) {
    QM_WARNING("webappsstore.sqlite is not a file!");
    return nsCOMPtr<mozIStorageConnection>{};
  }

  QM_TRY_INSPECT(const auto& connection,
                 QM_OR_ELSE_WARN_IF(
                     // Expression.
                     MOZ_TO_RESULT_INVOKE_TYPED(
                         nsCOMPtr<mozIStorageConnection>, aStorageService,
                         OpenUnsharedDatabase, &aWebAppsStoreFile),
                     // Predicate.
                     IsDatabaseCorruptionError,
                     // Fallback. Don't throw an error, leave a corrupted
                     // webappsstore database as it is.
                     ErrToDefaultOk<nsCOMPtr<mozIStorageConnection>>));

  if (connection) {
    // Don't propagate an error, leave a non-updateable webappsstore database as
    // it is.
    QM_TRY(StorageDBUpdater::Update(connection),
           nsCOMPtr<mozIStorageConnection>{});
  }

  return connection;
}

Result<nsCOMPtr<nsIFile>, nsresult> GetLocalStorageArchiveFile(
    const nsAString& aDirectoryPath) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!aDirectoryPath.IsEmpty());

  QM_TRY_UNWRAP(auto lsArchiveFile, QM_NewLocalFile(aDirectoryPath));

  QM_TRY(lsArchiveFile->Append(nsLiteralString(LS_ARCHIVE_FILE_NAME)));

  return lsArchiveFile;
}

Result<nsCOMPtr<nsIFile>, nsresult> GetLocalStorageArchiveTmpFile(
    const nsAString& aDirectoryPath) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!aDirectoryPath.IsEmpty());

  QM_TRY_UNWRAP(auto lsArchiveTmpFile, QM_NewLocalFile(aDirectoryPath));

  QM_TRY(lsArchiveTmpFile->Append(nsLiteralString(LS_ARCHIVE_TMP_FILE_NAME)));

  return lsArchiveTmpFile;
}

Result<bool, nsresult> IsLocalStorageArchiveInitialized(
    mozIStorageConnection& aConnection) {
  AssertIsOnIOThread();

  QM_TRY_RETURN(MOZ_TO_RESULT_INVOKE(aConnection, TableExists, "database"_ns));
}

nsresult InitializeLocalStorageArchive(mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

#ifdef DEBUG
  {
    QM_TRY_INSPECT(const auto& initialized,
                   IsLocalStorageArchiveInitialized(*aConnection));
    MOZ_ASSERT(!initialized);
  }
#endif

  QM_TRY(aConnection->ExecuteSimpleSQL(
      "CREATE TABLE database(version INTEGER NOT NULL DEFAULT 0);"_ns));

  QM_TRY_INSPECT(
      const auto& stmt,
      MOZ_TO_RESULT_INVOKE_TYPED(
          nsCOMPtr<mozIStorageStatement>, aConnection, CreateStatement,
          "INSERT INTO database (version) VALUES (:version)"_ns));

  QM_TRY(stmt->BindInt32ByName("version"_ns, 0));
  QM_TRY(stmt->Execute());

  return NS_OK;
}

Result<int32_t, nsresult> LoadLocalStorageArchiveVersion(
    mozIStorageConnection& aConnection) {
  AssertIsOnIOThread();

  QM_TRY_INSPECT(const auto& stmt,
                 CreateAndExecuteSingleStepStatement<
                     SingleStepResult::ReturnNullIfNoResult>(
                     aConnection, "SELECT version FROM database"_ns));

  QM_TRY(OkIf(stmt), Err(NS_ERROR_FILE_CORRUPTED));

  QM_TRY_RETURN(MOZ_TO_RESULT_INVOKE(stmt, GetInt32, 0));
}

nsresult SaveLocalStorageArchiveVersion(mozIStorageConnection* aConnection,
                                        int32_t aVersion) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aConnection->CreateStatement(
      "UPDATE database SET version = :version;"_ns, getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt32ByName("version"_ns, aVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

template <typename FileFunc, typename DirectoryFunc>
Result<mozilla::Ok, nsresult> CollectEachFileEntry(
    nsIFile& aDirectory, const FileFunc& aFileFunc,
    const DirectoryFunc& aDirectoryFunc) {
  AssertIsOnIOThread();

  return CollectEachFile(
      aDirectory,
      [&aFileFunc, &aDirectoryFunc](
          const nsCOMPtr<nsIFile>& file) -> Result<mozilla::Ok, nsresult> {
        QM_TRY_INSPECT(const auto& dirEntryKind, GetDirEntryKind(*file));

        switch (dirEntryKind) {
          case nsIFileKind::ExistsAsDirectory:
            return aDirectoryFunc(file);

          case nsIFileKind::ExistsAsFile:
            return aFileFunc(file);

          case nsIFileKind::DoesNotExist:
            // Ignore files that got removed externally while iterating.
            break;
        }

        return Ok{};
      });
}

/******************************************************************************
 * Quota manager class declarations
 ******************************************************************************/

}  // namespace

class QuotaManager::Observer final : public nsIObserver {
  static Observer* sInstance;

  bool mPendingProfileChange;
  bool mShutdownComplete;

 public:
  static nsresult Initialize();

  static void ShutdownCompleted();

 private:
  Observer() : mPendingProfileChange(false), mShutdownComplete(false) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  ~Observer() { MOZ_ASSERT(NS_IsMainThread()); }

  nsresult Init();

  nsresult Shutdown();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

namespace {

/*******************************************************************************
 * Local class declarations
 ******************************************************************************/

}  // namespace

// XXX Change this not to derive from AutoTArray.
class ClientUsageArray final
    : public AutoTArray<Maybe<uint64_t>, Client::TYPE_MAX> {
 public:
  ClientUsageArray() { SetLength(Client::TypeMax()); }

  void Serialize(nsACString& aText) const;

  nsresult Deserialize(const nsACString& aText);

  ClientUsageArray Clone() const {
    ClientUsageArray res;
    res.Assign(*this);
    return res;
  }
};

class OriginInfo final {
  friend class GroupInfo;
  friend class QuotaManager;
  friend class QuotaObject;

 public:
  OriginInfo(GroupInfo* aGroupInfo, const nsACString& aOrigin,
             const ClientUsageArray& aClientUsages, uint64_t aUsage,
             int64_t aAccessTime, bool aPersisted, bool aDirectoryExists);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(OriginInfo)

  GroupInfo* GetGroupInfo() const { return mGroupInfo; }

  const nsCString& Origin() const { return mOrigin; }

  int64_t LockedUsage() const {
    AssertCurrentThreadOwnsQuotaMutex();

#ifdef DEBUG
    QuotaManager* quotaManager = QuotaManager::Get();
    MOZ_ASSERT(quotaManager);

    uint64_t usage = 0;
    for (Client::Type type : quotaManager->AllClientTypes()) {
      AssertNoOverflow(usage, mClientUsages[type].valueOr(0));
      usage += mClientUsages[type].valueOr(0);
    }
    MOZ_ASSERT(mUsage == usage);
#endif

    return mUsage;
  }

  int64_t LockedAccessTime() const {
    AssertCurrentThreadOwnsQuotaMutex();

    return mAccessTime;
  }

  bool LockedPersisted() const {
    AssertCurrentThreadOwnsQuotaMutex();

    return mPersisted;
  }

  OriginMetadata FlattenToOriginMetadata() const;

  nsresult LockedBindToStatement(mozIStorageStatement* aStatement) const;

 private:
  // Private destructor, to discourage deletion outside of Release():
  ~OriginInfo() {
    MOZ_COUNT_DTOR(OriginInfo);

    MOZ_ASSERT(!mQuotaObjects.Count());
  }

  void LockedDecreaseUsage(Client::Type aClientType, int64_t aSize);

  void LockedResetUsageForClient(Client::Type aClientType);

  UsageInfo LockedGetUsageForClient(Client::Type aClientType);

  void LockedUpdateAccessTime(int64_t aAccessTime) {
    AssertCurrentThreadOwnsQuotaMutex();

    mAccessTime = aAccessTime;
    if (!mAccessed) {
      mAccessed = true;
    }
  }

  void LockedPersist();

  bool IsExtensionOrigin() { return mIsExtension; }

  nsTHashMap<nsStringHashKey, NotNull<QuotaObject*>> mQuotaObjects;
  ClientUsageArray mClientUsages;
  GroupInfo* mGroupInfo;
  const nsCString mOrigin;
  bool mIsExtension;
  uint64_t mUsage;
  int64_t mAccessTime;
  bool mAccessed;
  bool mPersisted;
  /**
   * In some special cases like the LocalStorage client where it's possible to
   * create a Quota-using representation but not actually write any data, we
   * want to be able to track quota for an origin without creating its origin
   * directory or the per-client files until they are actually needed to store
   * data. In those cases, the OriginInfo will be created by
   * EnsureQuotaForOrigin and the resulting mDirectoryExists will be false until
   * the origin actually needs to be created. It is possible for mUsage to be
   * greater than zero while mDirectoryExists is false, representing a state
   * where a client like LocalStorage has reserved quota for disk writes, but
   * has not yet flushed the data to disk.
   */
  bool mDirectoryExists;
};

class OriginInfoAccessTimeComparator {
 public:
  bool Equals(const NotNull<RefPtr<const OriginInfo>>& a,
              const NotNull<RefPtr<const OriginInfo>>& b) const {
    return a->LockedAccessTime() == b->LockedAccessTime();
  }

  bool LessThan(const NotNull<RefPtr<const OriginInfo>>& a,
                const NotNull<RefPtr<const OriginInfo>>& b) const {
    return a->LockedAccessTime() < b->LockedAccessTime();
  }
};

class GroupInfo final {
  friend class GroupInfoPair;
  friend class OriginInfo;
  friend class QuotaManager;
  friend class QuotaObject;

 public:
  GroupInfo(GroupInfoPair* aGroupInfoPair, PersistenceType aPersistenceType)
      : mGroupInfoPair(aGroupInfoPair),
        mPersistenceType(aPersistenceType),
        mUsage(0) {
    MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

    MOZ_COUNT_CTOR(GroupInfo);
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GroupInfo)

  PersistenceType GetPersistenceType() const { return mPersistenceType; }

 private:
  // Private destructor, to discourage deletion outside of Release():
  MOZ_COUNTED_DTOR(GroupInfo)

  already_AddRefed<OriginInfo> LockedGetOriginInfo(const nsACString& aOrigin);

  void LockedAddOriginInfo(NotNull<RefPtr<OriginInfo>>&& aOriginInfo);

  void LockedAdjustUsageForRemovedOriginInfo(const OriginInfo& aOriginInfo);

  void LockedRemoveOriginInfo(const nsACString& aOrigin);

  void LockedRemoveOriginInfos();

  bool LockedHasOriginInfos() {
    AssertCurrentThreadOwnsQuotaMutex();

    return !mOriginInfos.IsEmpty();
  }

  nsTArray<NotNull<RefPtr<OriginInfo>>> mOriginInfos;

  GroupInfoPair* mGroupInfoPair;
  PersistenceType mPersistenceType;
  uint64_t mUsage;
};

// XXX Consider a new name for this class, it has other data members now
// (besides two GroupInfo objects).
class GroupInfoPair {
 public:
  GroupInfoPair(const nsACString& aSuffix, const nsACString& aGroup)
      : mSuffix(aSuffix), mGroup(aGroup) {
    MOZ_COUNT_CTOR(GroupInfoPair);
  }

  MOZ_COUNTED_DTOR(GroupInfoPair)

  const nsCString& Suffix() const { return mSuffix; }

  const nsCString& Group() const { return mGroup; }

  RefPtr<GroupInfo> LockedGetGroupInfo(PersistenceType aPersistenceType) {
    AssertCurrentThreadOwnsQuotaMutex();
    MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

    return GetGroupInfoForPersistenceType(aPersistenceType);
  }

  void LockedSetGroupInfo(PersistenceType aPersistenceType,
                          GroupInfo* aGroupInfo) {
    AssertCurrentThreadOwnsQuotaMutex();
    MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

    RefPtr<GroupInfo>& groupInfo =
        GetGroupInfoForPersistenceType(aPersistenceType);
    groupInfo = aGroupInfo;
  }

  void LockedClearGroupInfo(PersistenceType aPersistenceType) {
    AssertCurrentThreadOwnsQuotaMutex();
    MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

    RefPtr<GroupInfo>& groupInfo =
        GetGroupInfoForPersistenceType(aPersistenceType);
    groupInfo = nullptr;
  }

  bool LockedHasGroupInfos() {
    AssertCurrentThreadOwnsQuotaMutex();

    return mTemporaryStorageGroupInfo || mDefaultStorageGroupInfo;
  }

 private:
  RefPtr<GroupInfo>& GetGroupInfoForPersistenceType(
      PersistenceType aPersistenceType);

  const nsCString mSuffix;
  const nsCString mGroup;
  RefPtr<GroupInfo> mTemporaryStorageGroupInfo;
  RefPtr<GroupInfo> mDefaultStorageGroupInfo;
};

namespace {

class CollectOriginsHelper final : public Runnable {
  uint64_t mMinSizeToBeFreed;

  Mutex& mMutex;
  CondVar mCondVar;

  // The members below are protected by mMutex.
  nsTArray<RefPtr<OriginDirectoryLock>> mLocks;
  uint64_t mSizeToBeFreed;
  bool mWaiting;

 public:
  CollectOriginsHelper(mozilla::Mutex& aMutex, uint64_t aMinSizeToBeFreed);

  // Blocks the current thread until origins are collected on the main thread.
  // The returned value contains an aggregate size of those origins.
  int64_t BlockAndReturnOriginsForEviction(
      nsTArray<RefPtr<OriginDirectoryLock>>& aLocks);

 private:
  ~CollectOriginsHelper() = default;

  NS_IMETHOD
  Run() override;
};

class OriginOperationBase : public BackgroundThreadObject, public Runnable {
 protected:
  nsresult mResultCode;

  enum State {
    // Not yet run.
    State_Initial,

    // Running quota manager initialization on the owning thread.
    State_CreatingQuotaManager,

    // Running on the owning thread in the listener for OpenDirectory.
    State_DirectoryOpenPending,

    // Running on the IO thread.
    State_DirectoryWorkOpen,

    // Running on the owning thread after all work is done.
    State_UnblockingOpen,

    // All done.
    State_Complete
  };

 private:
  State mState;
  bool mActorDestroyed;

 protected:
  bool mNeedsQuotaManagerInit;
  bool mNeedsStorageInit;

 public:
  void NoteActorDestroyed() {
    AssertIsOnOwningThread();

    mActorDestroyed = true;
  }

  bool IsActorDestroyed() const {
    AssertIsOnOwningThread();

    return mActorDestroyed;
  }

 protected:
  explicit OriginOperationBase(
      nsIEventTarget* aOwningThread = GetCurrentEventTarget())
      : BackgroundThreadObject(aOwningThread),
        Runnable("dom::quota::OriginOperationBase"),
        mResultCode(NS_OK),
        mState(State_Initial),
        mActorDestroyed(false),
        mNeedsQuotaManagerInit(false),
        mNeedsStorageInit(false) {}

  // Reference counted.
  virtual ~OriginOperationBase() {
    MOZ_ASSERT(mState == State_Complete);
    MOZ_ASSERT(mActorDestroyed);
  }

#ifdef DEBUG
  State GetState() const { return mState; }
#endif

  void SetState(State aState) {
    MOZ_ASSERT(mState == State_Initial);
    mState = aState;
  }

  void AdvanceState() {
    switch (mState) {
      case State_Initial:
        mState = State_CreatingQuotaManager;
        return;
      case State_CreatingQuotaManager:
        mState = State_DirectoryOpenPending;
        return;
      case State_DirectoryOpenPending:
        mState = State_DirectoryWorkOpen;
        return;
      case State_DirectoryWorkOpen:
        mState = State_UnblockingOpen;
        return;
      case State_UnblockingOpen:
        mState = State_Complete;
        return;
      default:
        MOZ_CRASH("Bad state!");
    }
  }

  NS_IMETHOD
  Run() override;

  virtual void Open() = 0;

  nsresult DirectoryOpen();

  virtual nsresult DoDirectoryWork(QuotaManager& aQuotaManager) = 0;

  void Finish(nsresult aResult);

  virtual void UnblockOpen() = 0;

 private:
  nsresult Init();

  nsresult FinishInit();

  nsresult QuotaManagerOpen();

  nsresult DirectoryWork();
};

class FinalizeOriginEvictionOp : public OriginOperationBase {
  nsTArray<RefPtr<OriginDirectoryLock>> mLocks;

 public:
  FinalizeOriginEvictionOp(nsIEventTarget* aBackgroundThread,
                           nsTArray<RefPtr<OriginDirectoryLock>>&& aLocks)
      : OriginOperationBase(aBackgroundThread), mLocks(std::move(aLocks)) {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  void Dispatch();

  void RunOnIOThreadImmediately();

 private:
  ~FinalizeOriginEvictionOp() = default;

  virtual void Open() override;

  virtual nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  virtual void UnblockOpen() override;
};

class NormalOriginOperationBase
    : public OriginOperationBase,
      public OpenDirectoryListener,
      public SupportsCheckedUnsafePtr<CheckIf<DiagnosticAssertEnabled>> {
  RefPtr<DirectoryLock> mDirectoryLock;

 protected:
  Nullable<PersistenceType> mPersistenceType;
  OriginScope mOriginScope;
  Nullable<Client::Type> mClientType;
  mozilla::Atomic<bool> mCanceled;
  const bool mExclusive;
  bool mNeedsDirectoryLocking;

 public:
  void RunImmediately() {
    MOZ_ASSERT(GetState() == State_Initial);

    MOZ_ALWAYS_SUCCEEDS(this->Run());
  }

 protected:
  NormalOriginOperationBase(const Nullable<PersistenceType>& aPersistenceType,
                            const OriginScope& aOriginScope, bool aExclusive)
      : mPersistenceType(aPersistenceType),
        mOriginScope(aOriginScope),
        mExclusive(aExclusive),
        mNeedsDirectoryLocking(true) {
    AssertIsOnOwningThread();
  }

  ~NormalOriginOperationBase() = default;

 private:
  // Need to declare refcounting unconditionally, because
  // OpenDirectoryListener has pure-virtual refcounting.
  NS_DECL_ISUPPORTS_INHERITED

  virtual void Open() override;

  virtual void UnblockOpen() override;

  // OpenDirectoryListener overrides.
  virtual void DirectoryLockAcquired(DirectoryLock* aLock) override;

  virtual void DirectoryLockFailed() override;

  // Used to send results before unblocking open.
  virtual void SendResults() = 0;
};

class SaveOriginAccessTimeOp : public NormalOriginOperationBase {
  int64_t mTimestamp;

 public:
  SaveOriginAccessTimeOp(PersistenceType aPersistenceType,
                         const nsACString& aOrigin, int64_t aTimestamp)
      : NormalOriginOperationBase(Nullable<PersistenceType>(aPersistenceType),
                                  OriginScope::FromOrigin(aOrigin),
                                  /* aExclusive */ false),
        mTimestamp(aTimestamp) {
    AssertIsOnOwningThread();
  }

 private:
  ~SaveOriginAccessTimeOp() = default;

  virtual nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  virtual void SendResults() override;
};

/*******************************************************************************
 * Actor class declarations
 ******************************************************************************/

class Quota final : public PQuotaParent {
#ifdef DEBUG
  bool mActorDestroyed;
#endif

 public:
  Quota();

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::dom::quota::Quota)

 private:
  ~Quota();

  void StartIdleMaintenance();

  bool VerifyRequestParams(const UsageRequestParams& aParams) const;

  bool VerifyRequestParams(const RequestParams& aParams) const;

  // IPDL methods.
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual PQuotaUsageRequestParent* AllocPQuotaUsageRequestParent(
      const UsageRequestParams& aParams) override;

  virtual mozilla::ipc::IPCResult RecvPQuotaUsageRequestConstructor(
      PQuotaUsageRequestParent* aActor,
      const UsageRequestParams& aParams) override;

  virtual bool DeallocPQuotaUsageRequestParent(
      PQuotaUsageRequestParent* aActor) override;

  virtual PQuotaRequestParent* AllocPQuotaRequestParent(
      const RequestParams& aParams) override;

  virtual mozilla::ipc::IPCResult RecvPQuotaRequestConstructor(
      PQuotaRequestParent* aActor, const RequestParams& aParams) override;

  virtual bool DeallocPQuotaRequestParent(PQuotaRequestParent* aActor) override;

  virtual mozilla::ipc::IPCResult RecvStartIdleMaintenance() override;

  virtual mozilla::ipc::IPCResult RecvStopIdleMaintenance() override;

  virtual mozilla::ipc::IPCResult RecvAbortOperationsForProcess(
      const ContentParentId& aContentParentId) override;
};

class QuotaUsageRequestBase : public NormalOriginOperationBase,
                              public PQuotaUsageRequestParent {
 public:
  // May be overridden by subclasses if they need to perform work on the
  // background thread before being run.
  virtual void Init(Quota& aQuota);

 protected:
  QuotaUsageRequestBase()
      : NormalOriginOperationBase(Nullable<PersistenceType>(),
                                  OriginScope::FromNull(),
                                  /* aExclusive */ false) {}

  mozilla::Result<UsageInfo, nsresult> GetUsageForOrigin(
      QuotaManager& aQuotaManager, PersistenceType aPersistenceType,
      const OriginMetadata& aOriginMetadata);

  // Subclasses use this override to set the IPDL response value.
  virtual void GetResponse(UsageRequestResponse& aResponse) = 0;

 private:
  mozilla::Result<UsageInfo, nsresult> GetUsageForOriginEntries(
      QuotaManager& aQuotaManager, PersistenceType aPersistenceType,
      const OriginMetadata& aOriginMetadata, nsIFile& aDirectory,
      bool aInitialized);

  void SendResults() override;

  // IPDL methods.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvCancel() final;
};

// A mix-in class to simplify operations that need to process every origin in
// one or more repositories. Sub-classes should call TraverseRepository in their
// DoDirectoryWork and implement a ProcessOrigin method for their per-origin
// logic.
class TraverseRepositoryHelper {
 public:
  TraverseRepositoryHelper() = default;

 protected:
  virtual ~TraverseRepositoryHelper() = default;

  // If ProcessOrigin returns an error, TraverseRepository will immediately
  // terminate and return the received error code to its caller.
  nsresult TraverseRepository(QuotaManager& aQuotaManager,
                              PersistenceType aPersistenceType);

 private:
  virtual const Atomic<bool>& GetIsCanceledFlag() = 0;

  virtual nsresult ProcessOrigin(QuotaManager& aQuotaManager,
                                 nsIFile& aOriginDir, const bool aPersistent,
                                 const PersistenceType aPersistenceType) = 0;
};

class GetUsageOp final : public QuotaUsageRequestBase,
                         public TraverseRepositoryHelper {
  nsTArray<OriginUsage> mOriginUsages;
  nsTHashMap<nsCStringHashKey, uint32_t> mOriginUsagesIndex;

  bool mGetAll;

 public:
  explicit GetUsageOp(const UsageRequestParams& aParams);

 private:
  ~GetUsageOp() = default;

  void ProcessOriginInternal(QuotaManager* aQuotaManager,
                             const PersistenceType aPersistenceType,
                             const nsACString& aOrigin,
                             const int64_t aTimestamp, const bool aPersisted,
                             const uint64_t aUsage);

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  const Atomic<bool>& GetIsCanceledFlag() override;

  nsresult ProcessOrigin(QuotaManager& aQuotaManager, nsIFile& aOriginDir,
                         const bool aPersistent,
                         const PersistenceType aPersistenceType) override;

  void GetResponse(UsageRequestResponse& aResponse) override;
};

class GetOriginUsageOp final : public QuotaUsageRequestBase {
  nsCString mSuffix;
  nsCString mGroup;
  uint64_t mUsage;
  uint64_t mFileUsage;
  bool mFromMemory;

 public:
  explicit GetOriginUsageOp(const UsageRequestParams& aParams);

 private:
  ~GetOriginUsageOp() = default;

  virtual nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(UsageRequestResponse& aResponse) override;
};

class QuotaRequestBase : public NormalOriginOperationBase,
                         public PQuotaRequestParent {
 public:
  // May be overridden by subclasses if they need to perform work on the
  // background thread before being run.
  virtual void Init(Quota& aQuota);

 protected:
  explicit QuotaRequestBase(bool aExclusive)
      : NormalOriginOperationBase(Nullable<PersistenceType>(),
                                  OriginScope::FromNull(), aExclusive) {}

  // Subclasses use this override to set the IPDL response value.
  virtual void GetResponse(RequestResponse& aResponse) = 0;

 private:
  virtual void SendResults() override;

  // IPDL methods.
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
};

class StorageNameOp final : public QuotaRequestBase {
  nsString mName;

 public:
  StorageNameOp();

  void Init(Quota& aQuota) override;

 private:
  ~StorageNameOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class InitializedRequestBase : public QuotaRequestBase {
 protected:
  bool mInitialized;

 public:
  void Init(Quota& aQuota) override;

 protected:
  InitializedRequestBase();
};

class StorageInitializedOp final : public InitializedRequestBase {
 private:
  ~StorageInitializedOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class TemporaryStorageInitializedOp final : public InitializedRequestBase {
 private:
  ~TemporaryStorageInitializedOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class InitOp final : public QuotaRequestBase {
 public:
  InitOp();

  void Init(Quota& aQuota) override;

 private:
  ~InitOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class InitTemporaryStorageOp final : public QuotaRequestBase {
 public:
  InitTemporaryStorageOp();

  void Init(Quota& aQuota) override;

 private:
  ~InitTemporaryStorageOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class InitializeOriginRequestBase : public QuotaRequestBase {
 protected:
  nsCString mSuffix;
  nsCString mGroup;
  bool mCreated;

 public:
  void Init(Quota& aQuota) override;

 protected:
  InitializeOriginRequestBase(PersistenceType aPersistenceType,
                              const PrincipalInfo& aPrincipalInfo);
};

class InitializePersistentOriginOp final : public InitializeOriginRequestBase {
 public:
  explicit InitializePersistentOriginOp(const RequestParams& aParams);

 private:
  ~InitializePersistentOriginOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class InitializeTemporaryOriginOp final : public InitializeOriginRequestBase {
 public:
  explicit InitializeTemporaryOriginOp(const RequestParams& aParams);

 private:
  ~InitializeTemporaryOriginOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class ResetOrClearOp final : public QuotaRequestBase {
  const bool mClear;

 public:
  explicit ResetOrClearOp(bool aClear);

  void Init(Quota& aQuota) override;

 private:
  ~ResetOrClearOp() = default;

  void DeleteFiles(QuotaManager& aQuotaManager);

  void DeleteStorageFile(QuotaManager& aQuotaManager);

  virtual nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  virtual void GetResponse(RequestResponse& aResponse) override;
};

class ClearRequestBase : public QuotaRequestBase {
 protected:
  explicit ClearRequestBase(bool aExclusive) : QuotaRequestBase(aExclusive) {
    AssertIsOnOwningThread();
  }

  void DeleteFiles(QuotaManager& aQuotaManager,
                   PersistenceType aPersistenceType);

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;
};

class ClearOriginOp final : public ClearRequestBase {
  const ClearResetOriginParams mParams;
  const bool mMatchAll;

 public:
  explicit ClearOriginOp(const RequestParams& aParams);

  void Init(Quota& aQuota) override;

 private:
  ~ClearOriginOp() = default;

  void GetResponse(RequestResponse& aResponse) override;
};

class ClearDataOp final : public ClearRequestBase {
  const ClearDataParams mParams;

 public:
  explicit ClearDataOp(const RequestParams& aParams);

  void Init(Quota& aQuota) override;

 private:
  ~ClearDataOp() = default;

  void GetResponse(RequestResponse& aResponse) override;
};

class ResetOriginOp final : public QuotaRequestBase {
 public:
  explicit ResetOriginOp(const RequestParams& aParams);

  void Init(Quota& aQuota) override;

 private:
  ~ResetOriginOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class PersistRequestBase : public QuotaRequestBase {
  const PrincipalInfo mPrincipalInfo;

 protected:
  nsCString mSuffix;
  nsCString mGroup;

 public:
  void Init(Quota& aQuota) override;

 protected:
  explicit PersistRequestBase(const PrincipalInfo& aPrincipalInfo);
};

class PersistedOp final : public PersistRequestBase {
  bool mPersisted;

 public:
  explicit PersistedOp(const RequestParams& aParams);

 private:
  ~PersistedOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class PersistOp final : public PersistRequestBase {
 public:
  explicit PersistOp(const RequestParams& aParams);

 private:
  ~PersistOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class EstimateOp final : public QuotaRequestBase {
  nsCString mGroup;
  uint64_t mUsage;
  uint64_t mLimit;

 public:
  explicit EstimateOp(const RequestParams& aParams);

 private:
  ~EstimateOp() = default;

  virtual nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  void GetResponse(RequestResponse& aResponse) override;
};

class ListOriginsOp final : public QuotaRequestBase,
                            public TraverseRepositoryHelper {
  // XXX Bug 1521541 will make each origin has it's own state.
  nsTArray<nsCString> mOrigins;

 public:
  ListOriginsOp();

  void Init(Quota& aQuota) override;

 private:
  ~ListOriginsOp() = default;

  nsresult DoDirectoryWork(QuotaManager& aQuotaManager) override;

  const Atomic<bool>& GetIsCanceledFlag() override;

  nsresult ProcessOrigin(QuotaManager& aQuotaManager, nsIFile& aOriginDir,
                         const bool aPersistent,
                         const PersistenceType aPersistenceType) override;

  void GetResponse(RequestResponse& aResponse) override;
};

/*******************************************************************************
 * Other class declarations
 ******************************************************************************/

class StoragePressureRunnable final : public Runnable {
  const uint64_t mUsage;

 public:
  explicit StoragePressureRunnable(uint64_t aUsage)
      : Runnable("dom::quota::QuotaObject::StoragePressureRunnable"),
        mUsage(aUsage) {}

 private:
  ~StoragePressureRunnable() = default;

  NS_DECL_NSIRUNNABLE
};

class RecordQuotaInfoLoadTimeHelper final : public Runnable {
  // TimeStamps that are set on the IO thread.
  LazyInitializedOnceNotNull<const TimeStamp> mStartTime;
  LazyInitializedOnceNotNull<const TimeStamp> mEndTime;

  // A TimeStamp that is set on the main thread.
  LazyInitializedOnceNotNull<const TimeStamp> mInitializedTime;

 public:
  RecordQuotaInfoLoadTimeHelper()
      : Runnable("dom::quota::RecordQuotaInfoLoadTimeHelper") {}

  void Start();

  void End();

 private:
  ~RecordQuotaInfoLoadTimeHelper() = default;

  NS_DECL_NSIRUNNABLE
};

/*******************************************************************************
 * Helper classes
 ******************************************************************************/

#ifdef QM_PRINCIPALINFO_VERIFICATION_ENABLED

class PrincipalVerifier final : public Runnable {
  nsTArray<PrincipalInfo> mPrincipalInfos;

 public:
  static already_AddRefed<PrincipalVerifier> CreateAndDispatch(
      nsTArray<PrincipalInfo>&& aPrincipalInfos);

 private:
  explicit PrincipalVerifier(nsTArray<PrincipalInfo>&& aPrincipalInfos)
      : Runnable("dom::quota::PrincipalVerifier"),
        mPrincipalInfos(std::move(aPrincipalInfos)) {
    AssertIsOnIOThread();
  }

  virtual ~PrincipalVerifier() = default;

  Result<Ok, nsCString> CheckPrincipalInfoValidity(
      const PrincipalInfo& aPrincipalInfo);

  NS_DECL_NSIRUNNABLE
};

#endif

/*******************************************************************************
 * Helper Functions
 ******************************************************************************/

template <typename T, bool = std::is_unsigned_v<T>>
struct IntChecker {
  static void Assert(T aInt) {
    static_assert(std::is_integral_v<T>, "Not an integer!");
    MOZ_ASSERT(aInt >= 0);
  }
};

template <typename T>
struct IntChecker<T, true> {
  static void Assert(T aInt) {
    static_assert(std::is_integral_v<T>, "Not an integer!");
  }
};

template <typename T>
void AssertNoOverflow(uint64_t aDest, T aArg) {
  IntChecker<T>::Assert(aDest);
  IntChecker<T>::Assert(aArg);
  MOZ_ASSERT(UINT64_MAX - aDest >= uint64_t(aArg));
}

template <typename T, typename U>
void AssertNoUnderflow(T aDest, U aArg) {
  IntChecker<T>::Assert(aDest);
  IntChecker<T>::Assert(aArg);
  MOZ_ASSERT(uint64_t(aDest) >= uint64_t(aArg));
}

inline bool IsDotFile(const nsAString& aFileName) {
  return QuotaManager::IsDotFile(aFileName);
}

inline bool IsOSMetadata(const nsAString& aFileName) {
  return QuotaManager::IsOSMetadata(aFileName);
}

bool IsOriginMetadata(const nsAString& aFileName) {
  return aFileName.EqualsLiteral(METADATA_FILE_NAME) ||
         aFileName.EqualsLiteral(METADATA_V2_FILE_NAME) ||
         IsOSMetadata(aFileName);
}

bool IsTempMetadata(const nsAString& aFileName) {
  return aFileName.EqualsLiteral(METADATA_TMP_FILE_NAME) ||
         aFileName.EqualsLiteral(METADATA_V2_TMP_FILE_NAME);
}

// Return whether the group was actually updated.
Result<bool, nsresult> MaybeUpdateGroupForOrigin(
    OriginMetadata& aOriginMetadata) {
  MOZ_ASSERT(!NS_IsMainThread());

  bool updated = false;

  if (aOriginMetadata.mOrigin.EqualsLiteral(kChromeOrigin)) {
    if (!aOriginMetadata.mGroup.EqualsLiteral(kChromeOrigin)) {
      aOriginMetadata.mGroup.AssignLiteral(kChromeOrigin);
      updated = true;
    }
  } else {
    OriginAttributes originAttributes;
    nsCString originNoSuffix;
    QM_TRY(OkIf(originAttributes.PopulateFromOrigin(aOriginMetadata.mOrigin,
                                                    originNoSuffix)),
           Err(NS_ERROR_FAILURE));

    RefPtr<MozURL> url;
    QM_TRY(MozURL::Init(getter_AddRefs(url), originNoSuffix), QM_PROPAGATE,
           [&originNoSuffix](const nsresult) {
             QM_WARNING("A URL %s is not recognized by MozURL",
                        originNoSuffix.get());
           });

    QM_TRY_INSPECT(const auto& baseDomain,
                   MOZ_TO_RESULT_INVOKE_TYPED(nsAutoCString, *url, BaseDomain));

    const nsCString upToDateGroup = baseDomain + aOriginMetadata.mSuffix;

    if (aOriginMetadata.mGroup != upToDateGroup) {
      aOriginMetadata.mGroup = upToDateGroup;
      updated = true;

#ifdef QM_PRINCIPALINFO_VERIFICATION_ENABLED
      ContentPrincipalInfo contentPrincipalInfo;
      contentPrincipalInfo.attrs() = originAttributes;
      contentPrincipalInfo.originNoSuffix() = originNoSuffix;
      contentPrincipalInfo.spec() = originNoSuffix;
      contentPrincipalInfo.baseDomain() = baseDomain;

      PrincipalInfo principalInfo(contentPrincipalInfo);

      nsTArray<PrincipalInfo> principalInfos;
      principalInfos.AppendElement(principalInfo);

      RefPtr<PrincipalVerifier> principalVerifier =
          PrincipalVerifier::CreateAndDispatch(std::move(principalInfos));
#endif
    }
  }

  return updated;
}

}  // namespace

BackgroundThreadObject::BackgroundThreadObject()
    : mOwningThread(GetCurrentEventTarget()) {
  AssertIsOnOwningThread();
}

BackgroundThreadObject::BackgroundThreadObject(nsIEventTarget* aOwningThread)
    : mOwningThread(aOwningThread) {}

#ifdef DEBUG

void BackgroundThreadObject::AssertIsOnOwningThread() const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mOwningThread);
  bool current;
  MOZ_ASSERT(NS_SUCCEEDED(mOwningThread->IsOnCurrentThread(&current)));
  MOZ_ASSERT(current);
}

#endif  // DEBUG

nsIEventTarget* BackgroundThreadObject::OwningThread() const {
  MOZ_ASSERT(mOwningThread);
  return mOwningThread;
}

bool IsOnIOThread() {
  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "Must have a manager here!");

  bool currentThread;
  return NS_SUCCEEDED(
             quotaManager->IOThread()->IsOnCurrentThread(&currentThread)) &&
         currentThread;
}

void AssertIsOnIOThread() {
  NS_ASSERTION(IsOnIOThread(), "Running on the wrong thread!");
}

void DiagnosticAssertIsOnIOThread() { MOZ_DIAGNOSTIC_ASSERT(IsOnIOThread()); }

void AssertCurrentThreadOwnsQuotaMutex() {
#ifdef DEBUG
  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "Must have a manager here!");

  quotaManager->AssertCurrentThreadOwnsQuotaMutex();
#endif
}

void ReportInternalError(const char* aFile, uint32_t aLine, const char* aStr) {
  // Get leaf of file path
  for (const char* p = aFile; *p; ++p) {
    if (*p == '/' && *(p + 1)) {
      aFile = p + 1;
    }
  }

  nsContentUtils::LogSimpleConsoleError(
      NS_ConvertUTF8toUTF16(
          nsPrintfCString("Quota %s: %s:%" PRIu32, aStr, aFile, aLine)),
      "quota", false /* Quota Manager is not active in private browsing mode */,
      true /* Quota Manager runs always in a chrome context */);
}

namespace {

bool gInvalidateQuotaCache = false;
StaticAutoPtr<nsString> gBasePath;
StaticAutoPtr<nsString> gStorageName;
StaticAutoPtr<nsCString> gBuildId;

#ifdef DEBUG
bool gQuotaManagerInitialized = false;
#endif

StaticRefPtr<QuotaManager> gInstance;
mozilla::Atomic<bool> gShutdown(false);

// A time stamp that can only be accessed on the main thread.
TimeStamp gLastOSWake;

typedef nsTArray<CheckedUnsafePtr<NormalOriginOperationBase>>
    NormalOriginOpArray;
StaticAutoPtr<NormalOriginOpArray> gNormalOriginOps;

// Constants for temporary storage limit computing.
static const uint32_t kDefaultChunkSizeKB = 10 * 1024;

void RegisterNormalOriginOp(NormalOriginOperationBase& aNormalOriginOp) {
  AssertIsOnBackgroundThread();

  if (!gNormalOriginOps) {
    gNormalOriginOps = new NormalOriginOpArray();
  }

  gNormalOriginOps->AppendElement(&aNormalOriginOp);
}

void UnregisterNormalOriginOp(NormalOriginOperationBase& aNormalOriginOp) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(gNormalOriginOps);

  gNormalOriginOps->RemoveElement(&aNormalOriginOp);

  if (gNormalOriginOps->IsEmpty()) {
    gNormalOriginOps = nullptr;
  }
}

class StorageOperationBase {
 protected:
  struct OriginProps {
    enum Type { eChrome, eContent, eObsolete, eInvalid };

    NotNull<nsCOMPtr<nsIFile>> mDirectory;
    nsString mLeafName;
    nsCString mSpec;
    OriginAttributes mAttrs;
    int64_t mTimestamp;
    OriginMetadata mOriginMetadata;
    nsCString mOriginalSuffix;

    LazyInitializedOnceEarlyDestructible<const PersistenceType>
        mPersistenceType;
    Type mType;
    bool mNeedsRestore;
    bool mNeedsRestore2;
    bool mIgnore;

   public:
    explicit OriginProps(MovingNotNull<nsCOMPtr<nsIFile>> aDirectory)
        : mDirectory(std::move(aDirectory)),
          mTimestamp(0),
          mType(eContent),
          mNeedsRestore(false),
          mNeedsRestore2(false),
          mIgnore(false) {}

    template <typename PersistenceTypeFunc>
    nsresult Init(PersistenceTypeFunc&& aPersistenceTypeFunc);
  };

  nsTArray<OriginProps> mOriginProps;

  nsCOMPtr<nsIFile> mDirectory;

 public:
  explicit StorageOperationBase(nsIFile* aDirectory) : mDirectory(aDirectory) {
    AssertIsOnIOThread();
  }

  NS_INLINE_DECL_REFCOUNTING(StorageOperationBase)

 protected:
  virtual ~StorageOperationBase() = default;

  nsresult GetDirectoryMetadata(nsIFile* aDirectory, int64_t& aTimestamp,
                                nsACString& aGroup, nsACString& aOrigin,
                                Nullable<bool>& aIsApp);

  // Upgrade helper to load the contents of ".metadata-v2" files from previous
  // schema versions.  Although QuotaManager has a similar GetDirectoryMetadata2
  // method, it is only intended to read current version ".metadata-v2" files.
  // And unlike the old ".metadata" files, the ".metadata-v2" format can evolve
  // because our "storage.sqlite" lets us track the overall version of the
  // storage directory.
  nsresult GetDirectoryMetadata2(nsIFile* aDirectory, int64_t& aTimestamp,
                                 nsACString& aSuffix, nsACString& aGroup,
                                 nsACString& aOrigin, bool& aIsApp);

  int64_t GetOriginLastModifiedTime(const OriginProps& aOriginProps);

  nsresult RemoveObsoleteOrigin(const OriginProps& aOriginProps);

  /**
   * Rename the origin if the origin string generation from nsIPrincipal
   * changed. This consists of renaming the origin in the metadata files and
   * renaming the origin directory itself. For simplicity, the origin in
   * metadata files is not actually updated, but the metadata files are
   * recreated instead.
   *
   * @param  aOriginProps the properties of the origin to check.
   *
   * @return whether origin was renamed.
   */
  Result<bool, nsresult> MaybeRenameOrigin(const OriginProps& aOriginProps);

  nsresult ProcessOriginDirectories();

  virtual nsresult ProcessOriginDirectory(const OriginProps& aOriginProps) = 0;
};

class MOZ_STACK_CLASS OriginParser final {
 public:
  enum ResultType { InvalidOrigin, ObsoleteOrigin, ValidOrigin };

 private:
  using Tokenizer =
      nsCCharSeparatedTokenizerTemplate<NS_TokenizerIgnoreNothing>;

  enum SchemeType { eNone, eFile, eAbout, eChrome };

  enum State {
    eExpectingAppIdOrScheme,
    eExpectingInMozBrowser,
    eExpectingScheme,
    eExpectingEmptyToken1,
    eExpectingEmptyToken2,
    eExpectingEmptyTokenOrUniversalFileOrigin,
    eExpectingHost,
    eExpectingPort,
    eExpectingEmptyTokenOrDriveLetterOrPathnameComponent,
    eExpectingEmptyTokenOrPathnameComponent,
    eExpectingEmptyToken1OrHost,

    // We transit from eExpectingHost to this state when we encounter a host
    // beginning with "[" which indicates an IPv6 literal. Because we mangle the
    // IPv6 ":" delimiter to be a "+", we will receive separate tokens for each
    // portion of the IPv6 address, including a final token that ends with "]".
    // (Note that we do not mangle "[" or "]".) Note that the URL spec
    // explicitly disclaims support for "<zone_id>" and so we don't have to deal
    // with that.
    eExpectingIPV6Token,
    eComplete,
    eHandledTrailingSeparator
  };

  const nsCString mOrigin;
  Tokenizer mTokenizer;

  nsCString mScheme;
  nsCString mHost;
  Nullable<uint32_t> mPort;
  nsTArray<nsCString> mPathnameComponents;
  nsCString mHandledTokens;

  SchemeType mSchemeType;
  State mState;
  bool mInIsolatedMozBrowser;
  bool mUniversalFileOrigin;
  bool mMaybeDriveLetter;
  bool mError;
  bool mMaybeObsolete;

  // Number of group which a IPv6 address has. Should be less than 9.
  uint8_t mIPGroup;

 public:
  explicit OriginParser(const nsACString& aOrigin)
      : mOrigin(aOrigin),
        mTokenizer(aOrigin, '+'),
        mPort(),
        mSchemeType(eNone),
        mState(eExpectingAppIdOrScheme),
        mInIsolatedMozBrowser(false),
        mUniversalFileOrigin(false),
        mMaybeDriveLetter(false),
        mError(false),
        mMaybeObsolete(false),
        mIPGroup(0) {}

  static ResultType ParseOrigin(const nsACString& aOrigin, nsCString& aSpec,
                                OriginAttributes* aAttrs,
                                nsCString& aOriginalSuffix);

  ResultType Parse(nsACString& aSpec);

 private:
  void HandleScheme(const nsDependentCSubstring& aToken);

  void HandlePathnameComponent(const nsDependentCSubstring& aToken);

  void HandleToken(const nsDependentCSubstring& aToken);

  void HandleTrailingSeparator();
};

class RepositoryOperationBase : public StorageOperationBase {
 public:
  explicit RepositoryOperationBase(nsIFile* aDirectory)
      : StorageOperationBase(aDirectory) {}

  nsresult ProcessRepository();

 protected:
  virtual ~RepositoryOperationBase() = default;

  template <typename UpgradeMethod>
  nsresult MaybeUpgradeClients(const OriginProps& aOriginsProps,
                               UpgradeMethod aMethod);

 private:
  virtual PersistenceType PersistenceTypeFromSpec(const nsCString& aSpec) = 0;

  virtual nsresult PrepareOriginDirectory(OriginProps& aOriginProps,
                                          bool* aRemoved) = 0;

  virtual nsresult PrepareClientDirectory(nsIFile* aFile,
                                          const nsAString& aLeafName,
                                          bool& aRemoved);
};

class CreateOrUpgradeDirectoryMetadataHelper final
    : public RepositoryOperationBase {
  nsCOMPtr<nsIFile> mPermanentStorageDir;

  // The legacy PersistenceType, before the default repository introduction.
  enum class LegacyPersistenceType {
    Persistent = 0,
    Temporary
    // The PersistenceType had also PERSISTENCE_TYPE_INVALID, but we don't need
    // it here.
  };

  LazyInitializedOnce<const LegacyPersistenceType> mLegacyPersistenceType;

 public:
  explicit CreateOrUpgradeDirectoryMetadataHelper(nsIFile* aDirectory)
      : RepositoryOperationBase(aDirectory) {}

  nsresult Init();

 private:
  Maybe<LegacyPersistenceType> LegacyPersistenceTypeFromFile(nsIFile& aFile,
                                                             const fallible_t&);

  PersistenceType PersistenceTypeFromLegacyPersistentSpec(
      const nsCString& aSpec);

  PersistenceType PersistenceTypeFromSpec(const nsCString& aSpec) override;

  nsresult MaybeUpgradeOriginDirectory(nsIFile* aDirectory);

  nsresult PrepareOriginDirectory(OriginProps& aOriginProps,
                                  bool* aRemoved) override;

  nsresult ProcessOriginDirectory(const OriginProps& aOriginProps) override;
};

class UpgradeStorageHelperBase : public RepositoryOperationBase {
  LazyInitializedOnce<const PersistenceType> mPersistenceType;

 public:
  explicit UpgradeStorageHelperBase(nsIFile* aDirectory)
      : RepositoryOperationBase(aDirectory) {}

  nsresult Init();

 private:
  PersistenceType PersistenceTypeFromSpec(const nsCString& aSpec) override;
};

class UpgradeStorageFrom0_0To1_0Helper final : public UpgradeStorageHelperBase {
 public:
  explicit UpgradeStorageFrom0_0To1_0Helper(nsIFile* aDirectory)
      : UpgradeStorageHelperBase(aDirectory) {}

 private:
  nsresult PrepareOriginDirectory(OriginProps& aOriginProps,
                                  bool* aRemoved) override;

  nsresult ProcessOriginDirectory(const OriginProps& aOriginProps) override;
};

class UpgradeStorageFrom1_0To2_0Helper final : public UpgradeStorageHelperBase {
 public:
  explicit UpgradeStorageFrom1_0To2_0Helper(nsIFile* aDirectory)
      : UpgradeStorageHelperBase(aDirectory) {}

 private:
  nsresult MaybeRemoveMorgueDirectory(const OriginProps& aOriginProps);

  /**
   * Remove the origin directory if appId is present in origin attributes.
   *
   * @param aOriginProps the properties of the origin to check.
   *
   * @return whether the origin directory was removed.
   */
  Result<bool, nsresult> MaybeRemoveAppsData(const OriginProps& aOriginProps);

  nsresult PrepareOriginDirectory(OriginProps& aOriginProps,
                                  bool* aRemoved) override;

  nsresult ProcessOriginDirectory(const OriginProps& aOriginProps) override;
};

class UpgradeStorageFrom2_0To2_1Helper final : public UpgradeStorageHelperBase {
 public:
  explicit UpgradeStorageFrom2_0To2_1Helper(nsIFile* aDirectory)
      : UpgradeStorageHelperBase(aDirectory) {}

 private:
  nsresult PrepareOriginDirectory(OriginProps& aOriginProps,
                                  bool* aRemoved) override;

  nsresult ProcessOriginDirectory(const OriginProps& aOriginProps) override;
};

class UpgradeStorageFrom2_1To2_2Helper final : public UpgradeStorageHelperBase {
 public:
  explicit UpgradeStorageFrom2_1To2_2Helper(nsIFile* aDirectory)
      : UpgradeStorageHelperBase(aDirectory) {}

 private:
  nsresult PrepareOriginDirectory(OriginProps& aOriginProps,
                                  bool* aRemoved) override;

  nsresult ProcessOriginDirectory(const OriginProps& aOriginProps) override;

  nsresult PrepareClientDirectory(nsIFile* aFile, const nsAString& aLeafName,
                                  bool& aRemoved) override;
};

class RestoreDirectoryMetadata2Helper final : public StorageOperationBase {
  LazyInitializedOnce<const PersistenceType> mPersistenceType;

 public:
  explicit RestoreDirectoryMetadata2Helper(nsIFile* aDirectory)
      : StorageOperationBase(aDirectory) {}

  nsresult Init();

  nsresult RestoreMetadata2File();

 private:
  nsresult ProcessOriginDirectory(const OriginProps& aOriginProps) override;
};

auto MakeSanitizedOriginCString(const nsACString& aOrigin) {
#ifdef XP_WIN
  NS_ASSERTION(!strcmp(QuotaManager::kReplaceChars,
                       FILE_ILLEGAL_CHARACTERS FILE_PATH_SEPARATOR),
               "Illegal file characters have changed!");
#endif

  nsAutoCString res{aOrigin};

  res.ReplaceChar(QuotaManager::kReplaceChars, '+');

  return res;
}

auto MakeSanitizedOriginString(const nsACString& aOrigin) {
  // An origin string is ASCII-only, since it is obtained via
  // nsIPrincipal::GetOrigin, which returns an ACString.
  return NS_ConvertASCIItoUTF16(MakeSanitizedOriginCString(aOrigin));
}

Result<nsAutoString, nsresult> GetPathForStorage(
    nsIFile& aBaseDir, const nsAString& aStorageName) {
  QM_TRY_INSPECT(const auto& storageDir,
                 CloneFileAndAppend(aBaseDir, aStorageName));

  QM_TRY_RETURN(MOZ_TO_RESULT_INVOKE_TYPED(nsAutoString, storageDir, GetPath));
}

int64_t GetLastModifiedTime(PersistenceType aPersistenceType, nsIFile& aFile) {
  AssertIsOnIOThread();

  class MOZ_STACK_CLASS Helper final {
   public:
    static nsresult GetLastModifiedTime(nsIFile* aFile, int64_t* aTimestamp) {
      AssertIsOnIOThread();
      MOZ_ASSERT(aFile);
      MOZ_ASSERT(aTimestamp);

      QM_TRY_INSPECT(const auto& dirEntryKind, GetDirEntryKind(*aFile));

      switch (dirEntryKind) {
        case nsIFileKind::ExistsAsDirectory:
          QM_TRY(CollectEachFile(*aFile,
                                 [&aTimestamp](const nsCOMPtr<nsIFile>& file)
                                     -> Result<mozilla::Ok, nsresult> {
                                   QM_TRY(
                                       GetLastModifiedTime(file, aTimestamp));

                                   return Ok{};
                                 }));
          break;

        case nsIFileKind::ExistsAsFile: {
          QM_TRY_INSPECT(
              const auto& leafName,
              MOZ_TO_RESULT_INVOKE_TYPED(nsAutoString, aFile, GetLeafName));

          // Bug 1595445 will handle unknown files here.

          if (IsOriginMetadata(leafName) || IsTempMetadata(leafName) ||
              IsDotFile(leafName)) {
            return NS_OK;
          }

          QM_TRY_UNWRAP(int64_t timestamp,
                        MOZ_TO_RESULT_INVOKE(aFile, GetLastModifiedTime));

          // Need to convert from milliseconds to microseconds.
          MOZ_ASSERT((INT64_MAX / PR_USEC_PER_MSEC) > timestamp);
          timestamp *= int64_t(PR_USEC_PER_MSEC);

          if (timestamp > *aTimestamp) {
            *aTimestamp = timestamp;
          }
          break;
        }

        case nsIFileKind::DoesNotExist:
          // Ignore files that got removed externally while iterating.
          break;
      }

      return NS_OK;
    }
  };

  if (aPersistenceType == PERSISTENCE_TYPE_PERSISTENT) {
    return PR_Now();
  }

  int64_t timestamp = INT64_MIN;
  nsresult rv = Helper::GetLastModifiedTime(&aFile, &timestamp);
  if (NS_FAILED(rv)) {
    timestamp = PR_Now();
  }

  return timestamp;
}

// Returns a bool indicating whether the directory was newly created.
Result<bool, nsresult> EnsureDirectory(nsIFile& aDirectory) {
  AssertIsOnIOThread();

  // Callers call this function without checking if the directory already
  // exists (idempotent usage). QM_OR_ELSE_WARN_IF is not used here since we
  // just want to log NS_ERROR_FILE_ALREADY_EXISTS result and not spam the
  // reports.
  QM_TRY_INSPECT(const auto& exists,
                 QM_OR_ELSE_LOG_VERBOSE_IF(
                     // Expression.
                     MOZ_TO_RESULT_INVOKE(aDirectory, Create,
                                          nsIFile::DIRECTORY_TYPE, 0755,
                                          /* aSkipAncestors = */ false)
                         .map([](Ok) { return false; }),
                     // Predicate.
                     IsSpecificError<NS_ERROR_FILE_ALREADY_EXISTS>,
                     // Fallback.
                     ErrToOk<true>));

  if (exists) {
    QM_TRY_INSPECT(const bool& isDirectory,
                   MOZ_TO_RESULT_INVOKE(aDirectory, IsDirectory));
    QM_TRY(OkIf(isDirectory), Err(NS_ERROR_UNEXPECTED));
  }

  return !exists;
}

enum FileFlag { Truncate, Update, Append };

Result<nsCOMPtr<nsIOutputStream>, nsresult> GetOutputStream(
    nsIFile& aFile, FileFlag aFileFlag) {
  AssertIsOnIOThread();

  switch (aFileFlag) {
    case FileFlag::Truncate:
      QM_TRY_RETURN(NS_NewLocalFileOutputStream(&aFile));

    case FileFlag::Update: {
      QM_TRY_INSPECT(const bool& exists, MOZ_TO_RESULT_INVOKE(&aFile, Exists));

      if (!exists) {
        return nsCOMPtr<nsIOutputStream>();
      }

      QM_TRY_INSPECT(const auto& stream, NS_NewLocalFileStream(&aFile));

      nsCOMPtr<nsIOutputStream> outputStream = do_QueryInterface(stream);
      QM_TRY(OkIf(outputStream), Err(NS_ERROR_FAILURE));

      return outputStream;
    }

    case FileFlag::Append:
      QM_TRY_RETURN(NS_NewLocalFileOutputStream(
          &aFile, PR_WRONLY | PR_CREATE_FILE | PR_APPEND));

    default:
      MOZ_CRASH("Should never get here!");
  }
}

Result<nsCOMPtr<nsIBinaryOutputStream>, nsresult> GetBinaryOutputStream(
    nsIFile& aFile, FileFlag aFileFlag) {
  QM_TRY_UNWRAP(auto outputStream, GetOutputStream(aFile, aFileFlag));

  QM_TRY(OkIf(outputStream), Err(NS_ERROR_UNEXPECTED));

  return nsCOMPtr<nsIBinaryOutputStream>(
      NS_NewObjectOutputStream(outputStream));
}

void GetJarPrefix(bool aInIsolatedMozBrowser, nsACString& aJarPrefix) {
  aJarPrefix.Truncate();

  // Fallback.
  if (!aInIsolatedMozBrowser) {
    return;
  }

  // AppId is an unused b2g identifier. Let's set it to 0 all the time (see bug
  // 1320404).
  // aJarPrefix = appId + "+" + { 't', 'f' } + "+";
  aJarPrefix.AppendInt(0);  // TODO: this is the appId, to be removed.
  aJarPrefix.Append('+');
  aJarPrefix.Append(aInIsolatedMozBrowser ? 't' : 'f');
  aJarPrefix.Append('+');
}

nsresult CreateDirectoryMetadata(nsIFile& aDirectory, int64_t aTimestamp,
                                 const OriginMetadata& aOriginMetadata) {
  AssertIsOnIOThread();

  OriginAttributes groupAttributes;

  nsCString groupNoSuffix;
  QM_TRY(OkIf(groupAttributes.PopulateFromOrigin(aOriginMetadata.mGroup,
                                                 groupNoSuffix)),
         NS_ERROR_FAILURE);

  nsCString groupPrefix;
  GetJarPrefix(groupAttributes.mInIsolatedMozBrowser, groupPrefix);

  nsCString group = groupPrefix + groupNoSuffix;

  OriginAttributes originAttributes;

  nsCString originNoSuffix;
  QM_TRY(OkIf(originAttributes.PopulateFromOrigin(aOriginMetadata.mOrigin,
                                                  originNoSuffix)),
         NS_ERROR_FAILURE);

  nsCString originPrefix;
  GetJarPrefix(originAttributes.mInIsolatedMozBrowser, originPrefix);

  nsCString origin = originPrefix + originNoSuffix;

  MOZ_ASSERT(groupPrefix == originPrefix);

  QM_TRY_INSPECT(const auto& file, MOZ_TO_RESULT_INVOKE_TYPED(
                                       nsCOMPtr<nsIFile>, aDirectory, Clone));

  QM_TRY(file->Append(nsLiteralString(METADATA_TMP_FILE_NAME)));

  QM_TRY_INSPECT(const auto& stream,
                 GetBinaryOutputStream(*file, FileFlag::Truncate));
  MOZ_ASSERT(stream);

  QM_TRY(stream->Write64(aTimestamp));

  QM_TRY(stream->WriteStringZ(group.get()));

  QM_TRY(stream->WriteStringZ(origin.get()));

  // Currently unused (used to be isApp).
  QM_TRY(stream->WriteBoolean(false));

  QM_TRY(stream->Flush());

  QM_TRY(stream->Close());

  QM_TRY(file->RenameTo(nullptr, nsLiteralString(METADATA_FILE_NAME)));

  return NS_OK;
}

nsresult CreateDirectoryMetadata2(nsIFile& aDirectory, int64_t aTimestamp,
                                  bool aPersisted,
                                  const OriginMetadata& aOriginMetadata) {
  AssertIsOnIOThread();

  QM_TRY_INSPECT(const auto& file, MOZ_TO_RESULT_INVOKE_TYPED(
                                       nsCOMPtr<nsIFile>, aDirectory, Clone));

  QM_TRY(file->Append(nsLiteralString(METADATA_V2_TMP_FILE_NAME)));

  QM_TRY_INSPECT(const auto& stream,
                 GetBinaryOutputStream(*file, FileFlag::Truncate));
  MOZ_ASSERT(stream);

  QM_TRY(stream->Write64(aTimestamp));

  QM_TRY(stream->WriteBoolean(aPersisted));

  // Reserved data 1
  QM_TRY(stream->Write32(0));

  // Reserved data 2
  QM_TRY(stream->Write32(0));

  // The suffix isn't used right now, but we might need it in future. It's
  // a bit of redundancy we can live with given how painful is to upgrade
  // metadata files.
  QM_TRY(stream->WriteStringZ(aOriginMetadata.mSuffix.get()));

  QM_TRY(stream->WriteStringZ(aOriginMetadata.mGroup.get()));

  QM_TRY(stream->WriteStringZ(aOriginMetadata.mOrigin.get()));

  // Currently unused (used to be isApp).
  QM_TRY(stream->WriteBoolean(false));

  QM_TRY(stream->Flush());

  QM_TRY(stream->Close());

  QM_TRY(file->RenameTo(nullptr, nsLiteralString(METADATA_V2_FILE_NAME)));

  return NS_OK;
}

Result<nsCOMPtr<nsIBinaryInputStream>, nsresult> GetBinaryInputStream(
    nsIFile& aDirectory, const nsAString& aFilename) {
  MOZ_ASSERT(!NS_IsMainThread());

  QM_TRY_INSPECT(const auto& file, MOZ_TO_RESULT_INVOKE_TYPED(
                                       nsCOMPtr<nsIFile>, aDirectory, Clone));

  QM_TRY(file->Append(aFilename));

  QM_TRY_UNWRAP(auto stream, NS_NewLocalFileInputStream(file));

  QM_TRY_INSPECT(const auto& bufferedStream,
                 NS_NewBufferedInputStream(stream.forget(), 512));

  QM_TRY(OkIf(bufferedStream), Err(NS_ERROR_FAILURE));

  return nsCOMPtr<nsIBinaryInputStream>(
      NS_NewObjectInputStream(bufferedStream));
}

// This method computes and returns our best guess for the temporary storage
// limit (in bytes), based on available space.
uint64_t GetTemporaryStorageLimit(uint64_t aAvailableSpaceBytes) {
  // The fixed limit pref can be used to override temporary storage limit
  // calculation.
  if (StaticPrefs::dom_quotaManager_temporaryStorage_fixedLimit() >= 0) {
    return static_cast<uint64_t>(
               StaticPrefs::dom_quotaManager_temporaryStorage_fixedLimit()) *
           1024;
  }

  uint64_t availableSpaceKB = aAvailableSpaceBytes / 1024;

  // Prevent division by zero below.
  uint32_t chunkSizeKB;
  if (StaticPrefs::dom_quotaManager_temporaryStorage_chunkSize()) {
    chunkSizeKB = StaticPrefs::dom_quotaManager_temporaryStorage_chunkSize();
  } else {
    chunkSizeKB = kDefaultChunkSizeKB;
  }

  // Grow/shrink in chunkSizeKB units, deliberately, so that in the common case
  // we don't shrink temporary storage and evict origin data every time we
  // initialize.
  availableSpaceKB = (availableSpaceKB / chunkSizeKB) * chunkSizeKB;

  // Allow temporary storage to consume up to half the available space.
  return availableSpaceKB * .50 * 1024;
}

}  // namespace

/*******************************************************************************
 * Exported functions
 ******************************************************************************/

void InitializeQuotaManager() {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!gQuotaManagerInitialized);

#ifdef QM_SCOPED_LOG_EXTRA_INFO_ENABLED
  ScopedLogExtraInfo::Initialize();
#endif

  if (!QuotaManager::IsRunningGTests()) {
    // This service has to be started on the main thread currently.
    const nsCOMPtr<mozIStorageService> ss =
        do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);

    QM_WARNONLY_TRY(OkIf(ss));
  }

  QM_WARNONLY_TRY(QuotaManager::Initialize());

#ifdef DEBUG
  gQuotaManagerInitialized = true;
#endif
}

PQuotaParent* AllocPQuotaParent() {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(QuotaManager::IsShuttingDown())) {
    return nullptr;
  }

  auto actor = MakeRefPtr<Quota>();

  return actor.forget().take();
}

bool DeallocPQuotaParent(PQuotaParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  RefPtr<Quota> actor = dont_AddRef(static_cast<Quota*>(aActor));
  return true;
}

bool RecvShutdownQuotaManager() {
  AssertIsOnBackgroundThread();

  QuotaManager::ShutdownInstance();

  return true;
}

QuotaManager::Observer* QuotaManager::Observer::sInstance = nullptr;

// static
nsresult QuotaManager::Observer::Initialize() {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<Observer> observer = new Observer();

  nsresult rv = observer->Init();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  sInstance = observer;

  return NS_OK;
}

// static
void QuotaManager::Observer::ShutdownCompleted() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sInstance);

  sInstance->mShutdownComplete = true;
}

nsresult QuotaManager::Observer::Init() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return NS_ERROR_FAILURE;
  }

  // XXX: Improve the way that we remove observer in failure cases.
  nsresult rv = obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = obs->AddObserver(this, kProfileDoChangeTopic, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    return rv;
  }

  rv = obs->AddObserver(this, PROFILE_BEFORE_CHANGE_QM_OBSERVER_ID, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    obs->RemoveObserver(this, kProfileDoChangeTopic);
    obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    return rv;
  }

  rv = obs->AddObserver(this, NS_WIDGET_WAKE_OBSERVER_TOPIC, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    obs->RemoveObserver(this, kProfileDoChangeTopic);
    obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    obs->RemoveObserver(this, PROFILE_BEFORE_CHANGE_QM_OBSERVER_ID);
    return rv;
  }

  return NS_OK;
}

nsresult QuotaManager::Observer::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ALWAYS_SUCCEEDS(obs->RemoveObserver(this, NS_WIDGET_WAKE_OBSERVER_TOPIC));
  MOZ_ALWAYS_SUCCEEDS(
      obs->RemoveObserver(this, PROFILE_BEFORE_CHANGE_QM_OBSERVER_ID));
  MOZ_ALWAYS_SUCCEEDS(obs->RemoveObserver(this, kProfileDoChangeTopic));
  MOZ_ALWAYS_SUCCEEDS(obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID));

  sInstance = nullptr;

  // In general, the instance will have died after the latter removal call, so
  // it's not safe to do anything after that point.
  // However, Shutdown is currently called from Observe which is called by the
  // Observer Service which holds a strong reference to the observer while the
  // Observe method is being called.

  return NS_OK;
}

NS_IMPL_ISUPPORTS(QuotaManager::Observer, nsIObserver)

NS_IMETHODIMP
QuotaManager::Observer::Observe(nsISupports* aSubject, const char* aTopic,
                                const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;

  if (!strcmp(aTopic, kProfileDoChangeTopic)) {
    if (NS_WARN_IF(gBasePath)) {
      NS_WARNING(
          "profile-before-change-qm must precede repeated "
          "profile-do-change!");
      return NS_OK;
    }

    Telemetry::SetEventRecordingEnabled("dom.quota.try"_ns, true);

    gBasePath = new nsString();

    nsCOMPtr<nsIFile> baseDir;
    rv = NS_GetSpecialDirectory(NS_APP_INDEXEDDB_PARENT_DIR,
                                getter_AddRefs(baseDir));
    if (NS_FAILED(rv)) {
      rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                  getter_AddRefs(baseDir));
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = baseDir->GetPath(*gBasePath);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    gStorageName = new nsString();

    rv = Preferences::GetString("dom.quotaManager.storageName", *gStorageName);
    if (NS_FAILED(rv)) {
      *gStorageName = kStorageName;
    }

    gBuildId = new nsCString();

    nsCOMPtr<nsIPlatformInfo> platformInfo =
        do_GetService("@mozilla.org/xre/app-info;1");
    if (NS_WARN_IF(!platformInfo)) {
      return NS_ERROR_FAILURE;
    }

    rv = platformInfo->GetPlatformBuildID(*gBuildId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MaybeEnableNextGenLocalStorage();

    return NS_OK;
  }

  if (!strcmp(aTopic, PROFILE_BEFORE_CHANGE_QM_OBSERVER_ID)) {
    if (NS_WARN_IF(!gBasePath)) {
      NS_WARNING("profile-do-change must precede profile-before-change-qm!");
      return NS_OK;
    }

    // mPendingProfileChange is our re-entrancy guard (the nested event loop
    // below may cause re-entrancy).
    if (mPendingProfileChange) {
      return NS_OK;
    }

    AutoRestore<bool> pending(mPendingProfileChange);
    mPendingProfileChange = true;

    mShutdownComplete = false;

    PBackgroundChild* backgroundActor =
        BackgroundChild::GetOrCreateForCurrentThread();
    if (NS_WARN_IF(!backgroundActor)) {
      return NS_ERROR_FAILURE;
    }

    if (NS_WARN_IF(!backgroundActor->SendShutdownQuotaManager())) {
      return NS_ERROR_FAILURE;
    }

    MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return mShutdownComplete; }));

    gBasePath = nullptr;

    gStorageName = nullptr;

    gBuildId = nullptr;

    Telemetry::SetEventRecordingEnabled("dom.quota.try"_ns, false);

    return NS_OK;
  }

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    rv = Shutdown();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

  if (!strcmp(aTopic, NS_WIDGET_WAKE_OBSERVER_TOPIC)) {
    gLastOSWake = TimeStamp::Now();

    return NS_OK;
  }

  NS_WARNING("Unknown observer topic!");
  return NS_OK;
}

/*******************************************************************************
 * Quota object
 ******************************************************************************/

void QuotaObject::AddRef() {
  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    NS_ERROR("Null quota manager, this shouldn't happen, possible leak!");

    ++mRefCnt;

    return;
  }

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  ++mRefCnt;
}

void QuotaObject::Release() {
  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    NS_ERROR("Null quota manager, this shouldn't happen, possible leak!");

    nsrefcnt count = --mRefCnt;
    if (count == 0) {
      mRefCnt = 1;
      delete this;
    }

    return;
  }

  {
    MutexAutoLock lock(quotaManager->mQuotaMutex);

    --mRefCnt;

    if (mRefCnt > 0) {
      return;
    }

    if (mOriginInfo) {
      mOriginInfo->mQuotaObjects.Remove(mPath);
    }
  }

  delete this;
}

bool QuotaObject::MaybeUpdateSize(int64_t aSize, bool aTruncate) {
  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  return LockedMaybeUpdateSize(aSize, aTruncate);
}

bool QuotaObject::IncreaseSize(int64_t aDelta) {
  MOZ_ASSERT(aDelta >= 0);

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  AssertNoOverflow(mSize, aDelta);
  int64_t size = mSize + aDelta;

  return LockedMaybeUpdateSize(size, /* aTruncate */ false);
}

void QuotaObject::DisableQuotaCheck() {
  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  mQuotaCheckDisabled = true;
}

void QuotaObject::EnableQuotaCheck() {
  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  mQuotaCheckDisabled = false;
}

bool QuotaObject::LockedMaybeUpdateSize(int64_t aSize, bool aTruncate) {
  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  quotaManager->mQuotaMutex.AssertCurrentThreadOwns();

  if (mWritingDone == false && mOriginInfo) {
    mWritingDone = true;
    StorageActivityService::SendActivity(mOriginInfo->mOrigin);
  }

  if (mQuotaCheckDisabled) {
    return true;
  }

  if (mSize == aSize) {
    return true;
  }

  if (!mOriginInfo) {
    mSize = aSize;
    return true;
  }

  GroupInfo* groupInfo = mOriginInfo->mGroupInfo;
  MOZ_ASSERT(groupInfo);

  if (mSize > aSize) {
    if (aTruncate) {
      const int64_t delta = mSize - aSize;

      AssertNoUnderflow(quotaManager->mTemporaryStorageUsage, delta);
      quotaManager->mTemporaryStorageUsage -= delta;

      if (!mOriginInfo->LockedPersisted()) {
        AssertNoUnderflow(groupInfo->mUsage, delta);
        groupInfo->mUsage -= delta;
      }

      AssertNoUnderflow(mOriginInfo->mUsage, delta);
      mOriginInfo->mUsage -= delta;

      MOZ_ASSERT(mOriginInfo->mClientUsages[mClientType].isSome());
      AssertNoUnderflow(mOriginInfo->mClientUsages[mClientType].value(), delta);
      mOriginInfo->mClientUsages[mClientType] =
          Some(mOriginInfo->mClientUsages[mClientType].value() - delta);

      mSize = aSize;
    }
    return true;
  }

  MOZ_ASSERT(mSize < aSize);

  RefPtr<GroupInfo> complementaryGroupInfo =
      groupInfo->mGroupInfoPair->LockedGetGroupInfo(
          ComplementaryPersistenceType(groupInfo->mPersistenceType));

  uint64_t delta = aSize - mSize;

  AssertNoOverflow(mOriginInfo->mClientUsages[mClientType].valueOr(0), delta);
  uint64_t newClientUsage =
      mOriginInfo->mClientUsages[mClientType].valueOr(0) + delta;

  AssertNoOverflow(mOriginInfo->mUsage, delta);
  uint64_t newUsage = mOriginInfo->mUsage + delta;

  // Temporary storage has no limit for origin usage (there's a group and the
  // global limit though).

  uint64_t newGroupUsage = groupInfo->mUsage;
  if (!mOriginInfo->LockedPersisted()) {
    AssertNoOverflow(groupInfo->mUsage, delta);
    newGroupUsage += delta;

    uint64_t groupUsage = groupInfo->mUsage;
    if (complementaryGroupInfo) {
      AssertNoOverflow(groupUsage, complementaryGroupInfo->mUsage);
      groupUsage += complementaryGroupInfo->mUsage;
    }

    // Temporary storage has a hard limit for group usage (20 % of the global
    // limit).
    AssertNoOverflow(groupUsage, delta);
    if (groupUsage + delta > quotaManager->GetGroupLimit()) {
      return false;
    }
  }

  AssertNoOverflow(quotaManager->mTemporaryStorageUsage, delta);
  uint64_t newTemporaryStorageUsage =
      quotaManager->mTemporaryStorageUsage + delta;

  if (newTemporaryStorageUsage > quotaManager->mTemporaryStorageLimit) {
    // This will block the thread without holding the lock while waitting.

    AutoTArray<RefPtr<OriginDirectoryLock>, 10> locks;
    uint64_t sizeToBeFreed;

    if (IsOnBackgroundThread()) {
      MutexAutoUnlock autoUnlock(quotaManager->mQuotaMutex);

      sizeToBeFreed = quotaManager->CollectOriginsForEviction(delta, locks);
    } else {
      sizeToBeFreed =
          quotaManager->LockedCollectOriginsForEviction(delta, locks);
    }

    if (!sizeToBeFreed) {
      uint64_t usage = quotaManager->mTemporaryStorageUsage;

      MutexAutoUnlock autoUnlock(quotaManager->mQuotaMutex);

      quotaManager->NotifyStoragePressure(usage);

      return false;
    }

    NS_ASSERTION(sizeToBeFreed >= delta, "Huh?");

    {
      MutexAutoUnlock autoUnlock(quotaManager->mQuotaMutex);

      for (const auto& lock : locks) {
        quotaManager->DeleteFilesForOrigin(lock->GetPersistenceType(),
                                           lock->Origin());
      }
    }

    // Relocked.

    NS_ASSERTION(mOriginInfo, "How come?!");

    for (const auto& lock : locks) {
      MOZ_ASSERT(!(lock->GetPersistenceType() == groupInfo->mPersistenceType &&
                   lock->Origin() == mOriginInfo->mOrigin),
                 "Deleted itself!");

      quotaManager->LockedRemoveQuotaForOrigin(lock->GetPersistenceType(),
                                               lock->OriginMetadata());
    }

    // We unlocked and relocked several times so we need to recompute all the
    // essential variables and recheck the group limit.

    AssertNoUnderflow(aSize, mSize);
    delta = aSize - mSize;

    AssertNoOverflow(mOriginInfo->mClientUsages[mClientType].valueOr(0), delta);
    newClientUsage = mOriginInfo->mClientUsages[mClientType].valueOr(0) + delta;

    AssertNoOverflow(mOriginInfo->mUsage, delta);
    newUsage = mOriginInfo->mUsage + delta;

    newGroupUsage = groupInfo->mUsage;
    if (!mOriginInfo->LockedPersisted()) {
      AssertNoOverflow(groupInfo->mUsage, delta);
      newGroupUsage += delta;

      uint64_t groupUsage = groupInfo->mUsage;
      if (complementaryGroupInfo) {
        AssertNoOverflow(groupUsage, complementaryGroupInfo->mUsage);
        groupUsage += complementaryGroupInfo->mUsage;
      }

      AssertNoOverflow(groupUsage, delta);
      if (groupUsage + delta > quotaManager->GetGroupLimit()) {
        // Unfortunately some other thread increased the group usage in the
        // meantime and we are not below the group limit anymore.

        // However, the origin eviction must be finalized in this case too.
        MutexAutoUnlock autoUnlock(quotaManager->mQuotaMutex);

        quotaManager->FinalizeOriginEviction(std::move(locks));

        return false;
      }
    }

    AssertNoOverflow(quotaManager->mTemporaryStorageUsage, delta);
    newTemporaryStorageUsage = quotaManager->mTemporaryStorageUsage + delta;

    NS_ASSERTION(
        newTemporaryStorageUsage <= quotaManager->mTemporaryStorageLimit,
        "How come?!");

    // Ok, we successfully freed enough space and the operation can continue
    // without throwing the quota error.
    mOriginInfo->mClientUsages[mClientType] = Some(newClientUsage);

    mOriginInfo->mUsage = newUsage;
    if (!mOriginInfo->LockedPersisted()) {
      groupInfo->mUsage = newGroupUsage;
    }
    quotaManager->mTemporaryStorageUsage = newTemporaryStorageUsage;
    ;

    // Some other thread could increase the size in the meantime, but no more
    // than this one.
    MOZ_ASSERT(mSize < aSize);
    mSize = aSize;

    // Finally, release IO thread only objects and allow next synchronized
    // ops for the evicted origins.
    MutexAutoUnlock autoUnlock(quotaManager->mQuotaMutex);

    quotaManager->FinalizeOriginEviction(std::move(locks));

    return true;
  }

  mOriginInfo->mClientUsages[mClientType] = Some(newClientUsage);

  mOriginInfo->mUsage = newUsage;
  if (!mOriginInfo->LockedPersisted()) {
    groupInfo->mUsage = newGroupUsage;
  }
  quotaManager->mTemporaryStorageUsage = newTemporaryStorageUsage;

  mSize = aSize;

  return true;
}

/*******************************************************************************
 * Quota manager
 ******************************************************************************/

QuotaManager::QuotaManager(const nsAString& aBasePath,
                           const nsAString& aStorageName)
    : mQuotaMutex("QuotaManager.mQuotaMutex"),
      mBasePath(aBasePath),
      mStorageName(aStorageName),
      mTemporaryStorageUsage(0),
      mNextDirectoryLockId(0),
      mTemporaryStorageInitialized(false),
      mCacheUsable(false) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!gInstance);
}

QuotaManager::~QuotaManager() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!gInstance || gInstance == this);
}

// static
nsresult QuotaManager::Initialize() {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = Observer::Initialize();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

// static
Result<MovingNotNull<RefPtr<QuotaManager>>, nsresult>
QuotaManager::GetOrCreate() {
  AssertIsOnBackgroundThread();

  if (gInstance) {
    return WrapMovingNotNullUnchecked(RefPtr<QuotaManager>{gInstance});
  }

  QM_TRY(OkIf(gBasePath), Err(NS_ERROR_FAILURE), [](const auto&) {
    NS_WARNING(
        "Trying to create QuotaManager before profile-do-change! "
        "Forgot to call do_get_profile()?");
  });

  QM_TRY(OkIf(!IsShuttingDown()), Err(NS_ERROR_FAILURE), [](const auto&) {
    MOZ_ASSERT(false,
               "Trying to create QuotaManager after profile-before-change-qm!");
  });

  auto instance = MakeRefPtr<QuotaManager>(*gBasePath, *gStorageName);

  QM_TRY(instance->Init());

  gInstance = instance;

  return WrapMovingNotNullUnchecked(std::move(instance));
}

void QuotaManager::GetOrCreate(nsIRunnable* aCallback) {
  AssertIsOnBackgroundThread();

  Unused << GetOrCreate();

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(aCallback));
}

// static
QuotaManager* QuotaManager::Get() {
  // Does not return an owning reference.
  return gInstance;
}

// static
bool QuotaManager::IsShuttingDown() { return gShutdown; }

// static
void QuotaManager::ShutdownInstance() {
  AssertIsOnBackgroundThread();

  if (gInstance) {
    gInstance->Shutdown();

    gInstance = nullptr;
  }

  RefPtr<Runnable> runnable =
      NS_NewRunnableFunction("dom::quota::QuotaManager::ShutdownCompleted",
                             []() { Observer::ShutdownCompleted(); });
  MOZ_ASSERT(runnable);

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(runnable.forget()));
}

// static
bool QuotaManager::IsOSMetadata(const nsAString& aFileName) {
  return aFileName.EqualsLiteral(DSSTORE_FILE_NAME) ||
         aFileName.EqualsLiteral(DESKTOP_FILE_NAME) ||
         aFileName.LowerCaseEqualsLiteral(DESKTOP_INI_FILE_NAME) ||
         aFileName.LowerCaseEqualsLiteral(THUMBS_DB_FILE_NAME);
}

// static
bool QuotaManager::IsDotFile(const nsAString& aFileName) {
  return aFileName.First() == char16_t('.');
}

void QuotaManager::RegisterDirectoryLock(DirectoryLockImpl& aLock) {
  AssertIsOnOwningThread();

  mDirectoryLocks.AppendElement(WrapNotNullUnchecked(&aLock));

  if (aLock.ShouldUpdateLockIdTable()) {
    MutexAutoLock lock(mQuotaMutex);

    MOZ_DIAGNOSTIC_ASSERT(!mDirectoryLockIdTable.Contains(aLock.Id()));
    mDirectoryLockIdTable.InsertOrUpdate(aLock.Id(),
                                         WrapNotNullUnchecked(&aLock));
  }

  if (aLock.ShouldUpdateLockTable()) {
    DirectoryLockTable& directoryLockTable =
        GetDirectoryLockTable(aLock.GetPersistenceType());

    // XXX It seems that the contents of the array are never actually used, we
    // just use that like an inefficient use counter. Can't we just change
    // DirectoryLockTable to a nsTHashMap<nsCStringHashKey, uint32_t>?
    directoryLockTable
        .LookupOrInsertWith(
            aLock.Origin(),
            [this, &aLock] {
              if (!IsShuttingDown()) {
                UpdateOriginAccessTime(aLock.GetPersistenceType(),
                                       aLock.OriginMetadata());
              }
              return MakeUnique<nsTArray<NotNull<DirectoryLockImpl*>>>();
            })
        ->AppendElement(WrapNotNullUnchecked(&aLock));
  }

  aLock.SetRegistered(true);
}

void QuotaManager::UnregisterDirectoryLock(DirectoryLockImpl& aLock) {
  AssertIsOnOwningThread();

  MOZ_ALWAYS_TRUE(mDirectoryLocks.RemoveElement(&aLock));

  if (aLock.ShouldUpdateLockIdTable()) {
    MutexAutoLock lock(mQuotaMutex);

    MOZ_DIAGNOSTIC_ASSERT(mDirectoryLockIdTable.Contains(aLock.Id()));
    mDirectoryLockIdTable.Remove(aLock.Id());
  }

  if (aLock.ShouldUpdateLockTable()) {
    DirectoryLockTable& directoryLockTable =
        GetDirectoryLockTable(aLock.GetPersistenceType());

    nsTArray<NotNull<DirectoryLockImpl*>>* array;
    MOZ_ALWAYS_TRUE(directoryLockTable.Get(aLock.Origin(), &array));

    MOZ_ALWAYS_TRUE(array->RemoveElement(&aLock));
    if (array->IsEmpty()) {
      directoryLockTable.Remove(aLock.Origin());

      if (!IsShuttingDown()) {
        UpdateOriginAccessTime(aLock.GetPersistenceType(),
                               aLock.OriginMetadata());
      }
    }
  }

  aLock.SetRegistered(false);
}

void QuotaManager::AddPendingDirectoryLock(DirectoryLockImpl& aLock) {
  AssertIsOnOwningThread();

  mPendingDirectoryLocks.AppendElement(&aLock);
}

void QuotaManager::RemovePendingDirectoryLock(DirectoryLockImpl& aLock) {
  AssertIsOnOwningThread();

  MOZ_ALWAYS_TRUE(mPendingDirectoryLocks.RemoveElement(&aLock));
}

uint64_t QuotaManager::CollectOriginsForEviction(
    uint64_t aMinSizeToBeFreed, nsTArray<RefPtr<OriginDirectoryLock>>& aLocks) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aLocks.IsEmpty());

  // XXX This looks as if this could/should also use CollectLRUOriginInfosUntil,
  // or maybe a generalization if that.

  struct MOZ_STACK_CLASS Helper final {
    static void GetInactiveOriginInfos(
        const nsTArray<NotNull<RefPtr<OriginInfo>>>& aOriginInfos,
        const nsTArray<NotNull<const DirectoryLockImpl*>>& aLocks,
        OriginInfosFlatTraversable& aInactiveOriginInfos) {
      for (const auto& originInfo : aOriginInfos) {
        MOZ_ASSERT(originInfo->mGroupInfo->mPersistenceType !=
                   PERSISTENCE_TYPE_PERSISTENT);

        if (originInfo->LockedPersisted()) {
          continue;
        }

        // Never evict PERSISTENCE_TYPE_DEFAULT data associated to a
        // moz-extension origin, unlike websites (which may more likely using
        // the local data as a cache but still able to retrieve the same data
        // from the server side) extensions do not have the same data stored
        // anywhere else and evicting the data would result into potential data
        // loss for the users.
        //
        // Also, unlike a website the extensions are explicitly installed and
        // uninstalled by the user and all data associated to the extension
        // principal will be completely removed once the addon is uninstalled.
        if (originInfo->mGroupInfo->mPersistenceType !=
                PERSISTENCE_TYPE_TEMPORARY &&
            originInfo->IsExtensionOrigin()) {
          continue;
        }

        const auto originScope = OriginScope::FromOrigin(originInfo->mOrigin);

        const bool match =
            std::any_of(aLocks.begin(), aLocks.end(),
                        [&originScope](const DirectoryLockImpl* const lock) {
                          return originScope.Matches(lock->GetOriginScope());
                        });

        if (!match) {
          MOZ_ASSERT(!originInfo->mQuotaObjects.Count(),
                     "Inactive origin shouldn't have open files!");
          aInactiveOriginInfos.InsertElementSorted(
              originInfo, OriginInfoAccessTimeComparator());
        }
      }
    }
  };

  // Split locks into separate arrays and filter out locks for persistent
  // storage, they can't block us.
  const auto [temporaryStorageLocks, defaultStorageLocks] = [this] {
    nsTArray<NotNull<const DirectoryLockImpl*>> temporaryStorageLocks;
    nsTArray<NotNull<const DirectoryLockImpl*>> defaultStorageLocks;
    for (NotNull<const DirectoryLockImpl*> const lock : mDirectoryLocks) {
      const Nullable<PersistenceType>& persistenceType =
          lock->NullablePersistenceType();

      if (persistenceType.IsNull()) {
        temporaryStorageLocks.AppendElement(lock);
        defaultStorageLocks.AppendElement(lock);
      } else if (persistenceType.Value() == PERSISTENCE_TYPE_TEMPORARY) {
        temporaryStorageLocks.AppendElement(lock);
      } else if (persistenceType.Value() == PERSISTENCE_TYPE_DEFAULT) {
        defaultStorageLocks.AppendElement(lock);
      } else {
        MOZ_ASSERT(persistenceType.Value() == PERSISTENCE_TYPE_PERSISTENT);

        // Do nothing here, persistent origins don't need to be collected ever.
      }
    }

    return std::pair(std::move(temporaryStorageLocks),
                     std::move(defaultStorageLocks));
  }();

  // Enumerate and process inactive origins. This must be protected by the
  // mutex.
  MutexAutoLock lock(mQuotaMutex);

  const auto [inactiveOrigins, sizeToBeFreed] =
      [this, &temporaryStorageLocks = temporaryStorageLocks,
       &defaultStorageLocks = defaultStorageLocks, aMinSizeToBeFreed] {
        nsTArray<NotNull<RefPtr<const OriginInfo>>> inactiveOrigins;
        for (const auto& entry : mGroupInfoPairs) {
          const auto& pair = entry.GetData();

          MOZ_ASSERT(!entry.GetKey().IsEmpty());
          MOZ_ASSERT(pair);

          RefPtr<GroupInfo> groupInfo =
              pair->LockedGetGroupInfo(PERSISTENCE_TYPE_TEMPORARY);
          if (groupInfo) {
            Helper::GetInactiveOriginInfos(groupInfo->mOriginInfos,
                                           temporaryStorageLocks,
                                           inactiveOrigins);
          }

          groupInfo = pair->LockedGetGroupInfo(PERSISTENCE_TYPE_DEFAULT);
          if (groupInfo) {
            Helper::GetInactiveOriginInfos(
                groupInfo->mOriginInfos, defaultStorageLocks, inactiveOrigins);
          }
        }

#ifdef DEBUG
        // Make sure the array is sorted correctly.
        const bool inactiveOriginsSorted =
            std::is_sorted(inactiveOrigins.cbegin(), inactiveOrigins.cend(),
                           [](const auto& lhs, const auto& rhs) {
                             return lhs->mAccessTime < rhs->mAccessTime;
                           });
        MOZ_ASSERT(inactiveOriginsSorted);
#endif

        // Create a list of inactive and the least recently used origins
        // whose aggregate size is greater or equals the minimal size to be
        // freed.
        uint64_t sizeToBeFreed = 0;
        for (uint32_t count = inactiveOrigins.Length(), index = 0;
             index < count; index++) {
          if (sizeToBeFreed >= aMinSizeToBeFreed) {
            inactiveOrigins.TruncateLength(index);
            break;
          }

          sizeToBeFreed += inactiveOrigins[index]->LockedUsage();
        }

        return std::pair(std::move(inactiveOrigins), sizeToBeFreed);
      }();

  if (sizeToBeFreed >= aMinSizeToBeFreed) {
    // Success, add directory locks for these origins, so any other
    // operations for them will be delayed (until origin eviction is finalized).

    for (const auto& originInfo : inactiveOrigins) {
      auto lock = DirectoryLockImpl::CreateForEviction(
          WrapNotNullUnchecked(this), originInfo->mGroupInfo->mPersistenceType,
          originInfo->FlattenToOriginMetadata());

      lock->AcquireImmediately();

      aLocks.AppendElement(lock.forget());
    }

    return sizeToBeFreed;
  }

  return 0;
}

template <typename P>
void QuotaManager::CollectPendingOriginsForListing(P aPredicate) {
  MutexAutoLock lock(mQuotaMutex);

  for (const auto& entry : mGroupInfoPairs) {
    const auto& pair = entry.GetData();

    MOZ_ASSERT(!entry.GetKey().IsEmpty());
    MOZ_ASSERT(pair);

    RefPtr<GroupInfo> groupInfo =
        pair->LockedGetGroupInfo(PERSISTENCE_TYPE_DEFAULT);
    if (groupInfo) {
      for (const auto& originInfo : groupInfo->mOriginInfos) {
        if (!originInfo->mDirectoryExists) {
          aPredicate(originInfo);
        }
      }
    }
  }
}

nsresult QuotaManager::Init() {
  AssertIsOnOwningThread();

#ifdef XP_WIN
  CacheUseDOSDevicePathSyntaxPrefValue();
#endif

  QM_TRY_INSPECT(const auto& baseDir, QM_NewLocalFile(mBasePath));

  QM_TRY_UNWRAP(
      do_Init(mIndexedDBPath),
      GetPathForStorage(*baseDir, nsLiteralString(INDEXEDDB_DIRECTORY_NAME)));

  QM_TRY(baseDir->Append(mStorageName));

  QM_TRY_UNWRAP(do_Init(mStoragePath),
                MOZ_TO_RESULT_INVOKE_TYPED(nsString, baseDir, GetPath));

  QM_TRY_UNWRAP(
      do_Init(mPermanentStoragePath),
      GetPathForStorage(*baseDir, nsLiteralString(PERMANENT_DIRECTORY_NAME)));

  QM_TRY_UNWRAP(
      do_Init(mTemporaryStoragePath),
      GetPathForStorage(*baseDir, nsLiteralString(TEMPORARY_DIRECTORY_NAME)));

  QM_TRY_UNWRAP(
      do_Init(mDefaultStoragePath),
      GetPathForStorage(*baseDir, nsLiteralString(DEFAULT_DIRECTORY_NAME)));

  QM_TRY_UNWRAP(do_Init(mIOThread),
                ToResultInvoke<nsCOMPtr<nsIThread>>(
                    MOZ_SELECT_OVERLOAD(NS_NewNamedThread), "QuotaManager IO"));

  // Make a timer here to avoid potential failures later. We don't actually
  // initialize the timer until shutdown.
  nsCOMPtr shutdownTimer = NS_NewTimer();
  QM_TRY(OkIf(shutdownTimer), Err(NS_ERROR_FAILURE));

  mShutdownTimer.init(WrapNotNullUnchecked(std::move(shutdownTimer)));

  static_assert(Client::IDB == 0 && Client::DOMCACHE == 1 && Client::SDB == 2 &&
                    Client::LS == 3 && Client::TYPE_MAX == 4,
                "Fix the registration!");

  // Register clients.
  auto clients = decltype(mClients)::ValueType{};
  clients.AppendElement(indexedDB::CreateQuotaClient());
  clients.AppendElement(cache::CreateQuotaClient());
  clients.AppendElement(simpledb::CreateQuotaClient());
  if (NextGenLocalStorageEnabled()) {
    clients.AppendElement(localstorage::CreateQuotaClient());
  } else {
    clients.SetLength(Client::TypeMax());
  }

  mClients.init(std::move(clients));

  MOZ_ASSERT(mClients->Capacity() == Client::TYPE_MAX,
             "Should be using an auto array with correct capacity!");

  mAllClientTypes.init(ClientTypesArray{Client::Type::IDB,
                                        Client::Type::DOMCACHE,
                                        Client::Type::SDB, Client::Type::LS});
  mAllClientTypesExceptLS.init(ClientTypesArray{
      Client::Type::IDB, Client::Type::DOMCACHE, Client::Type::SDB});

  return NS_OK;
}

// static
void QuotaManager::SafeMaybeRecordQuotaClientShutdownStep(
    const Client::Type aClientType, const nsACString& aStepDescription) {
  // Callable on any thread.

  if (auto* const quotaManager = QuotaManager::Get()) {
    quotaManager->MaybeRecordShutdownStep(Some(aClientType), aStepDescription);
  }
}

void QuotaManager::MaybeRecordQuotaManagerShutdownStep(
    const nsACString& aStepDescription) {
  // Callable on any thread.

  MaybeRecordShutdownStep(Nothing{}, aStepDescription);
}

void QuotaManager::MaybeRecordShutdownStep(
    const Maybe<Client::Type> aClientType, const nsACString& aStepDescription) {
  if (!mShutdownStarted) {
    // We are not shutting down yet, we intentionally ignore this here to avoid
    // that every caller has to make a distinction for shutdown vs. non-shutdown
    // situations.
    return;
  }

  const TimeDuration elapsedSinceShutdownStart =
      TimeStamp::NowLoRes() - *mShutdownStartedAt;

  const auto stepString =
      nsPrintfCString("%fs: %s", elapsedSinceShutdownStart.ToSeconds(),
                      nsPromiseFlatCString(aStepDescription).get());

  if (aClientType) {
    AssertIsOnBackgroundThread();

    mShutdownSteps[*aClientType].Append(stepString + "\n"_ns);
  } else {
    // Callable on any thread.
    MutexAutoLock lock(mQuotaMutex);

    mQuotaManagerShutdownSteps.Append(stepString + "\n"_ns);
  }

#ifdef DEBUG
  // XXX Probably this isn't the mechanism that should be used here.

  NS_DebugBreak(
      NS_DEBUG_WARNING,
      nsAutoCString(aClientType ? Client::TypeToText(*aClientType)
                                : "quota manager"_ns + " shutdown step"_ns)
          .get(),
      stepString.get(), __FILE__, __LINE__);
#endif
}

void QuotaManager::Shutdown() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mShutdownStarted);

  // Setting this flag prevents the service from being recreated and prevents
  // further storagess from being created.
  if (gShutdown.exchange(true)) {
    NS_ERROR("Shutdown more than once?!");
  }

  StopIdleMaintenance();

  mShutdownStartedAt.init(TimeStamp::NowLoRes());
  mShutdownStarted = true;

  const auto& allClientTypes = AllClientTypes();

  bool needsToWait = false;
  for (Client::Type type : allClientTypes) {
    needsToWait |= (*mClients)[type]->InitiateShutdownWorkThreads();
  }
  needsToWait |= static_cast<bool>(gNormalOriginOps);

  // If any clients cannot shutdown immediately, spin the event loop while we
  // wait on all the threads to close. Our timer may fire during that loop.
  if (needsToWait) {
    MOZ_ALWAYS_SUCCEEDS(
        (*mShutdownTimer)
            ->InitWithNamedFuncCallback(
                [](nsITimer* aTimer, void* aClosure) {
                  auto* const quotaManager =
                      static_cast<QuotaManager*>(aClosure);

                  for (Client::Type type : quotaManager->AllClientTypes()) {
                    // XXX This is a workaround to unblock shutdown, which ought
                    // to be removed by Bug 1682326.
                    if (type == Client::IDB) {
                      (*quotaManager->mClients)[type]->AbortAllOperations();
                    }

                    (*quotaManager->mClients)[type]->ForceKillActors();
                  }

                  MOZ_ALWAYS_SUCCEEDS(aTimer->InitWithNamedFuncCallback(
                      [](nsITimer* aTimer, void* aClosure) {
                        auto* const quotaManager =
                            static_cast<QuotaManager*>(aClosure);

                        nsCString annotation;

                        {
                          for (Client::Type type :
                               quotaManager->AllClientTypes()) {
                            auto& quotaClient =
                                *(*quotaManager->mClients)[type];

                            if (!quotaClient.IsShutdownCompleted()) {
                              annotation.AppendPrintf(
                                  "%s: %s\nIntermediate steps:\n%s\n\n",
                                  Client::TypeToText(type).get(),
                                  quotaClient.GetShutdownStatus().get(),
                                  quotaManager->mShutdownSteps[type].get());
                            }
                          }

                          if (gNormalOriginOps) {
                            MutexAutoLock lock(quotaManager->mQuotaMutex);

                            annotation.AppendPrintf(
                                "QM: %zu normal origin ops "
                                "pending\nIntermediate "
                                "steps:\n%s\n",
                                gNormalOriginOps->Length(),
                                quotaManager->mQuotaManagerShutdownSteps.get());
                          }
                        }

                        // We expect that at least one quota client didn't
                        // complete its shutdown.
                        MOZ_DIAGNOSTIC_ASSERT(!annotation.IsEmpty());

                        CrashReporter::AnnotateCrashReport(
                            CrashReporter::Annotation::
                                QuotaManagerShutdownTimeout,
                            annotation);

                        MOZ_CRASH("Quota manager shutdown timed out");
                      },
                      aClosure, SHUTDOWN_FORCE_CRASH_TIMEOUT_MS,
                      nsITimer::TYPE_ONE_SHOT,
                      "quota::QuotaManager::ForceCrashTimer"));
                },
                this, SHUTDOWN_FORCE_KILL_TIMEOUT_MS, nsITimer::TYPE_ONE_SHOT,
                "quota::QuotaManager::ForceKillTimer"));

    MOZ_ALWAYS_TRUE(SpinEventLoopUntil([this, &allClientTypes] {
      return !gNormalOriginOps &&
             std::all_of(allClientTypes.cbegin(), allClientTypes.cend(),
                         [&self = *this](const auto type) {
                           return (*self.mClients)[type]->IsShutdownCompleted();
                         });
    }));
  }

  for (Client::Type type : allClientTypes) {
    (*mClients)[type]->FinalizeShutdownWorkThreads();
  }

  // Cancel the timer regardless of whether it actually fired.
  QM_WARNONLY_TRY((*mShutdownTimer)->Cancel());

  // NB: It's very important that runnable is destroyed on this thread
  // (i.e. after we join the IO thread) because we can't release the
  // QuotaManager on the IO thread. This should probably use
  // NewNonOwningRunnableMethod ...
  RefPtr<Runnable> runnable =
      NewRunnableMethod("dom::quota::QuotaManager::ShutdownStorage", this,
                        &QuotaManager::ShutdownStorage);
  MOZ_ASSERT(runnable);

  // Give clients a chance to cleanup IO thread only objects.
  QM_WARNONLY_TRY((*mIOThread)->Dispatch(runnable, NS_DISPATCH_NORMAL));

  // Make sure to join with our IO thread.
  QM_WARNONLY_TRY((*mIOThread)->Shutdown());

  for (RefPtr<DirectoryLockImpl>& lock : mPendingDirectoryLocks) {
    lock->Invalidate();
  }
}

void QuotaManager::InitQuotaForOrigin(
    const FullOriginMetadata& aFullOriginMetadata,
    const ClientUsageArray& aClientUsages, uint64_t aUsageBytes) {
  AssertIsOnIOThread();
  MOZ_ASSERT(IsBestEffortPersistenceType(aFullOriginMetadata.mPersistenceType));

  MutexAutoLock lock(mQuotaMutex);

  RefPtr<GroupInfo> groupInfo = LockedGetOrCreateGroupInfo(
      aFullOriginMetadata.mPersistenceType, aFullOriginMetadata.mSuffix,
      aFullOriginMetadata.mGroup);

  groupInfo->LockedAddOriginInfo(MakeNotNull<RefPtr<OriginInfo>>(
      groupInfo, aFullOriginMetadata.mOrigin, aClientUsages, aUsageBytes,
      aFullOriginMetadata.mLastAccessTime, aFullOriginMetadata.mPersisted,
      /* aDirectoryExists */ true));
}

void QuotaManager::EnsureQuotaForOrigin(const OriginMetadata& aOriginMetadata) {
  AssertIsOnIOThread();
  MOZ_ASSERT(IsBestEffortPersistenceType(aOriginMetadata.mPersistenceType));

  MutexAutoLock lock(mQuotaMutex);

  RefPtr<GroupInfo> groupInfo = LockedGetOrCreateGroupInfo(
      aOriginMetadata.mPersistenceType, aOriginMetadata.mSuffix,
      aOriginMetadata.mGroup);

  RefPtr<OriginInfo> originInfo =
      groupInfo->LockedGetOriginInfo(aOriginMetadata.mOrigin);
  if (!originInfo) {
    groupInfo->LockedAddOriginInfo(MakeNotNull<RefPtr<OriginInfo>>(
        groupInfo, aOriginMetadata.mOrigin, ClientUsageArray(),
        /* aUsageBytes */ 0,
        /* aAccessTime */ PR_Now(), /* aPersisted */ false,
        /* aDirectoryExists */ false));
  }
}

int64_t QuotaManager::NoteOriginDirectoryCreated(
    const OriginMetadata& aOriginMetadata, bool aPersisted) {
  AssertIsOnIOThread();
  MOZ_ASSERT(IsBestEffortPersistenceType(aOriginMetadata.mPersistenceType));

  int64_t timestamp;

  MutexAutoLock lock(mQuotaMutex);

  RefPtr<GroupInfo> groupInfo = LockedGetOrCreateGroupInfo(
      aOriginMetadata.mPersistenceType, aOriginMetadata.mSuffix,
      aOriginMetadata.mGroup);

  RefPtr<OriginInfo> originInfo =
      groupInfo->LockedGetOriginInfo(aOriginMetadata.mOrigin);
  if (originInfo) {
    timestamp = originInfo->LockedAccessTime();
    originInfo->mPersisted = aPersisted;
    originInfo->mDirectoryExists = true;
  } else {
    timestamp = PR_Now();
    groupInfo->LockedAddOriginInfo(MakeNotNull<RefPtr<OriginInfo>>(
        groupInfo, aOriginMetadata.mOrigin, ClientUsageArray(),
        /* aUsageBytes */ 0,
        /* aAccessTime */ timestamp, aPersisted, /* aDirectoryExists */ true));
  }

  return timestamp;
}

void QuotaManager::DecreaseUsageForClient(const ClientMetadata& aClientMetadata,
                                          int64_t aSize) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(IsBestEffortPersistenceType(aClientMetadata.mPersistenceType));

  MutexAutoLock lock(mQuotaMutex);

  GroupInfoPair* pair;
  if (!mGroupInfoPairs.Get(aClientMetadata.mGroup, &pair)) {
    return;
  }

  RefPtr<GroupInfo> groupInfo =
      pair->LockedGetGroupInfo(aClientMetadata.mPersistenceType);
  if (!groupInfo) {
    return;
  }

  RefPtr<OriginInfo> originInfo =
      groupInfo->LockedGetOriginInfo(aClientMetadata.mOrigin);
  if (originInfo) {
    originInfo->LockedDecreaseUsage(aClientMetadata.mClientType, aSize);
  }
}

void QuotaManager::ResetUsageForClient(const ClientMetadata& aClientMetadata) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(IsBestEffortPersistenceType(aClientMetadata.mPersistenceType));

  MutexAutoLock lock(mQuotaMutex);

  GroupInfoPair* pair;
  if (!mGroupInfoPairs.Get(aClientMetadata.mGroup, &pair)) {
    return;
  }

  RefPtr<GroupInfo> groupInfo =
      pair->LockedGetGroupInfo(aClientMetadata.mPersistenceType);
  if (!groupInfo) {
    return;
  }

  RefPtr<OriginInfo> originInfo =
      groupInfo->LockedGetOriginInfo(aClientMetadata.mOrigin);
  if (originInfo) {
    originInfo->LockedResetUsageForClient(aClientMetadata.mClientType);
  }
}

UsageInfo QuotaManager::GetUsageForClient(PersistenceType aPersistenceType,
                                          const OriginMetadata& aOriginMetadata,
                                          Client::Type aClientType) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

  MutexAutoLock lock(mQuotaMutex);

  GroupInfoPair* pair;
  if (!mGroupInfoPairs.Get(aOriginMetadata.mGroup, &pair)) {
    return UsageInfo{};
  }

  RefPtr<GroupInfo> groupInfo = pair->LockedGetGroupInfo(aPersistenceType);
  if (!groupInfo) {
    return UsageInfo{};
  }

  RefPtr<OriginInfo> originInfo =
      groupInfo->LockedGetOriginInfo(aOriginMetadata.mOrigin);
  if (!originInfo) {
    return UsageInfo{};
  }

  return originInfo->LockedGetUsageForClient(aClientType);
}

void QuotaManager::UpdateOriginAccessTime(
    PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);
  MOZ_ASSERT(!IsShuttingDown());

  MutexAutoLock lock(mQuotaMutex);

  GroupInfoPair* pair;
  if (!mGroupInfoPairs.Get(aOriginMetadata.mGroup, &pair)) {
    return;
  }

  RefPtr<GroupInfo> groupInfo = pair->LockedGetGroupInfo(aPersistenceType);
  if (!groupInfo) {
    return;
  }

  RefPtr<OriginInfo> originInfo =
      groupInfo->LockedGetOriginInfo(aOriginMetadata.mOrigin);
  if (originInfo) {
    int64_t timestamp = PR_Now();
    originInfo->LockedUpdateAccessTime(timestamp);

    MutexAutoUnlock autoUnlock(mQuotaMutex);

    auto op = MakeRefPtr<SaveOriginAccessTimeOp>(
        aPersistenceType, aOriginMetadata.mOrigin, timestamp);

    RegisterNormalOriginOp(*op);

    op->RunImmediately();
  }
}

void QuotaManager::RemoveQuota() {
  AssertIsOnIOThread();

  MutexAutoLock lock(mQuotaMutex);

  for (const auto& entry : mGroupInfoPairs) {
    const auto& pair = entry.GetData();

    MOZ_ASSERT(!entry.GetKey().IsEmpty());
    MOZ_ASSERT(pair);

    RefPtr<GroupInfo> groupInfo =
        pair->LockedGetGroupInfo(PERSISTENCE_TYPE_TEMPORARY);
    if (groupInfo) {
      groupInfo->LockedRemoveOriginInfos();
    }

    groupInfo = pair->LockedGetGroupInfo(PERSISTENCE_TYPE_DEFAULT);
    if (groupInfo) {
      groupInfo->LockedRemoveOriginInfos();
    }
  }

  mGroupInfoPairs.Clear();

  MOZ_ASSERT(mTemporaryStorageUsage == 0, "Should be zero!");
}

nsresult QuotaManager::LoadQuota() {
  AssertIsOnIOThread();
  MOZ_ASSERT(mStorageConnection);
  MOZ_ASSERT(!mTemporaryStorageInitialized);

  auto recordQuotaInfoLoadTimeHelper =
      MakeRefPtr<RecordQuotaInfoLoadTimeHelper>();
  recordQuotaInfoLoadTimeHelper->Start();

  auto LoadQuotaFromCache = [&]() -> nsresult {
    QM_TRY_INSPECT(
        const auto& stmt,
        MOZ_TO_RESULT_INVOKE_TYPED(nsCOMPtr<mozIStorageStatement>,
                                   mStorageConnection, CreateStatement,
                                   "SELECT repository_id, suffix, group_, "
                                   "origin, client_usages, usage, "
                                   "last_access_time, accessed, persisted "
                                   "FROM origin"_ns));

    auto autoRemoveQuota = MakeScopeExit([&] { RemoveQuota(); });

    QM_TRY(quota::CollectWhileHasResult(
        *stmt, [this](auto& stmt) -> Result<Ok, nsresult> {
          QM_TRY_INSPECT(const int32_t& repositoryId,
                         MOZ_TO_RESULT_INVOKE(stmt, GetInt32, 0));

          const auto maybePersistenceType =
              PersistenceTypeFromInt32(repositoryId, fallible);
          QM_TRY(OkIf(maybePersistenceType.isSome()), Err(NS_ERROR_FAILURE));

          FullOriginMetadata fullOriginMetadata;

          fullOriginMetadata.mPersistenceType = maybePersistenceType.value();

          QM_TRY_UNWRAP(
              fullOriginMetadata.mSuffix,
              MOZ_TO_RESULT_INVOKE_TYPED(nsCString, stmt, GetUTF8String, 1));

          QM_TRY_UNWRAP(
              fullOriginMetadata.mGroup,
              MOZ_TO_RESULT_INVOKE_TYPED(nsCString, stmt, GetUTF8String, 2));

          QM_TRY_UNWRAP(
              fullOriginMetadata.mOrigin,
              MOZ_TO_RESULT_INVOKE_TYPED(nsCString, stmt, GetUTF8String, 3));

          QM_TRY_INSPECT(const bool& updated,
                         MaybeUpdateGroupForOrigin(fullOriginMetadata));

          Unused << updated;

          // We don't need to update the .metadata-v2 file on disk here,
          // EnsureTemporaryOriginIsInitialized is responsible for doing that.
          // We just need to use correct group before initializing quota for the
          // given origin. (Note that calling LoadFullOriginMetadataWithRestore
          // below might update the group in the metadata file, but only as a
          // side-effect. The actual place we ensure consistency is in
          // EnsureTemporaryOriginIsInitialized.)

          QM_TRY_INSPECT(
              const auto& clientUsagesText,
              MOZ_TO_RESULT_INVOKE_TYPED(nsCString, stmt, GetUTF8String, 4));

          ClientUsageArray clientUsages;
          QM_TRY(clientUsages.Deserialize(clientUsagesText));

          QM_TRY_INSPECT(const int64_t& usage,
                         MOZ_TO_RESULT_INVOKE(stmt, GetInt64, 5));
          QM_TRY_UNWRAP(fullOriginMetadata.mLastAccessTime,
                        MOZ_TO_RESULT_INVOKE(stmt, GetInt64, 6));
          QM_TRY_INSPECT(const int64_t& accessed,
                         MOZ_TO_RESULT_INVOKE(stmt, GetInt32, 7));
          QM_TRY_UNWRAP(fullOriginMetadata.mPersisted,
                        MOZ_TO_RESULT_INVOKE(stmt, GetInt32, 8));

          if (accessed) {
            QM_TRY_INSPECT(
                const auto& directory,
                GetDirectoryForOrigin(fullOriginMetadata.mPersistenceType,
                                      fullOriginMetadata.mOrigin));

            QM_TRY_INSPECT(const bool& exists,
                           MOZ_TO_RESULT_INVOKE(directory, Exists));

            QM_TRY(OkIf(exists), Err(NS_ERROR_FAILURE));

            QM_TRY_INSPECT(const bool& isDirectory,
                           MOZ_TO_RESULT_INVOKE(directory, IsDirectory));

            QM_TRY(OkIf(isDirectory), Err(NS_ERROR_FAILURE));

            // Calling LoadFullOriginMetadataWithRestore might update the group
            // in the metadata file, but only as a side-effect. The actual place
            // we ensure consistency is in EnsureTemporaryOriginIsInitialized.

            QM_TRY_INSPECT(const auto& metadata,
                           LoadFullOriginMetadataWithRestore(directory));

            QM_TRY(OkIf(fullOriginMetadata.mLastAccessTime ==
                        metadata.mLastAccessTime),
                   Err(NS_ERROR_FAILURE));

            QM_TRY(OkIf(fullOriginMetadata.mPersisted == metadata.mPersisted),
                   Err(NS_ERROR_FAILURE));

            QM_TRY(OkIf(fullOriginMetadata.mPersistenceType ==
                        metadata.mPersistenceType),
                   Err(NS_ERROR_FAILURE));

            QM_TRY(OkIf(fullOriginMetadata.mSuffix == metadata.mSuffix),
                   Err(NS_ERROR_FAILURE));

            QM_TRY(OkIf(fullOriginMetadata.mGroup == metadata.mGroup),
                   Err(NS_ERROR_FAILURE));

            QM_TRY(OkIf(fullOriginMetadata.mOrigin == metadata.mOrigin),
                   Err(NS_ERROR_FAILURE));

            QM_TRY(InitializeOrigin(fullOriginMetadata.mPersistenceType,
                                    fullOriginMetadata,
                                    fullOriginMetadata.mLastAccessTime,
                                    fullOriginMetadata.mPersisted, directory));
          } else {
            InitQuotaForOrigin(fullOriginMetadata, clientUsages, usage);
          }

          return Ok{};
        }));

    autoRemoveQuota.release();

    return NS_OK;
  };

  QM_TRY_INSPECT(
      const bool& loadQuotaFromCache, ([this]() -> Result<bool, nsresult> {
        if (mCacheUsable) {
          QM_TRY_INSPECT(
              const auto& stmt,
              CreateAndExecuteSingleStepStatement<
                  SingleStepResult::ReturnNullIfNoResult>(
                  *mStorageConnection, "SELECT valid, build_id FROM cache"_ns));

          QM_TRY(OkIf(stmt), Err(NS_ERROR_FILE_CORRUPTED));

          QM_TRY_INSPECT(const int32_t& valid,
                         MOZ_TO_RESULT_INVOKE(stmt, GetInt32, 0));

          if (valid) {
            if (!StaticPrefs::dom_quotaManager_caching_checkBuildId()) {
              return true;
            }

            QM_TRY_INSPECT(const auto& buildId,
                           MOZ_TO_RESULT_INVOKE_TYPED(nsAutoCString, stmt,
                                                      GetUTF8String, 1));

            return buildId == *gBuildId;
          }
        }

        return false;
      }()));

  auto autoRemoveQuota = MakeScopeExit([&] { RemoveQuota(); });

  if (!loadQuotaFromCache ||
      !StaticPrefs::dom_quotaManager_loadQuotaFromCache() ||
      ![&LoadQuotaFromCache] {
        QM_WARNONLY_TRY_UNWRAP(auto res, ToResult(LoadQuotaFromCache()));
        return static_cast<bool>(res);
      }()) {
    // A keeper to defer the return only in Nightly, so that the telemetry data
    // for whole profile can be collected.
#ifdef NIGHTLY_BUILD
    nsresult statusKeeper = NS_OK;
#endif

    const auto statusKeeperFunc = [&](const nsresult rv) {
      RECORD_IN_NIGHTLY(statusKeeper, rv);
    };

    for (const PersistenceType type : kBestEffortPersistenceTypes) {
      if (NS_WARN_IF(IsShuttingDown())) {
        RETURN_STATUS_OR_RESULT(statusKeeper, NS_ERROR_ABORT);
      }

      QM_TRY(([&]() -> Result<Ok, nsresult> {
        QM_TRY(([this, type] {
                 const auto innerFunc = [&](const auto&) -> nsresult {
                   return InitializeRepository(type);
                 };

                 return ExecuteInitialization(
                     type == PERSISTENCE_TYPE_DEFAULT
                         ? Initialization::DefaultRepository
                         : Initialization::TemporaryRepository,
                     innerFunc);
               }()),
               OK_IN_NIGHTLY_PROPAGATE_IN_OTHERS, statusKeeperFunc);

        return Ok{};
      }()));
    }

#ifdef NIGHTLY_BUILD
    if (NS_FAILED(statusKeeper)) {
      return statusKeeper;
    }
#endif
  }

  recordQuotaInfoLoadTimeHelper->End();

  autoRemoveQuota.release();

  return NS_OK;
}

void QuotaManager::UnloadQuota() {
  AssertIsOnIOThread();
  MOZ_ASSERT(mStorageConnection);
  MOZ_ASSERT(mTemporaryStorageInitialized);
  MOZ_ASSERT(mCacheUsable);

  auto autoRemoveQuota = MakeScopeExit([&] { RemoveQuota(); });

  mozStorageTransaction transaction(
      mStorageConnection, false, mozIStorageConnection::TRANSACTION_IMMEDIATE);

  QM_TRY(transaction.Start(), QM_VOID);

  QM_TRY(mStorageConnection->ExecuteSimpleSQL("DELETE FROM origin;"_ns),
         QM_VOID);

  nsCOMPtr<mozIStorageStatement> insertStmt;

  {
    MutexAutoLock lock(mQuotaMutex);

    for (auto iter = mGroupInfoPairs.Iter(); !iter.Done(); iter.Next()) {
      MOZ_ASSERT(!iter.Key().IsEmpty());

      GroupInfoPair* const pair = iter.UserData();
      MOZ_ASSERT(pair);

      for (const PersistenceType type : kBestEffortPersistenceTypes) {
        RefPtr<GroupInfo> groupInfo = pair->LockedGetGroupInfo(type);
        if (!groupInfo) {
          continue;
        }

        for (const auto& originInfo : groupInfo->mOriginInfos) {
          MOZ_ASSERT(!originInfo->mQuotaObjects.Count());

          if (!originInfo->mDirectoryExists) {
            continue;
          }

          if (insertStmt) {
            MOZ_ALWAYS_SUCCEEDS(insertStmt->Reset());
          } else {
            QM_TRY_UNWRAP(
                insertStmt,
                MOZ_TO_RESULT_INVOKE_TYPED(
                    nsCOMPtr<mozIStorageStatement>, mStorageConnection,
                    CreateStatement,
                    "INSERT INTO origin (repository_id, suffix, group_, "
                    "origin, client_usages, usage, last_access_time, "
                    "accessed, persisted) "
                    "VALUES (:repository_id, :suffix, :group_, :origin, "
                    ":client_usages, :usage, :last_access_time, :accessed, "
                    ":persisted)"_ns),
                QM_VOID);
          }

          QM_TRY(originInfo->LockedBindToStatement(insertStmt), QM_VOID);

          QM_TRY(insertStmt->Execute(), QM_VOID);
        }

        groupInfo->LockedRemoveOriginInfos();
      }

      iter.Remove();
    }
  }

  QM_TRY_INSPECT(
      const auto& stmt,
      MOZ_TO_RESULT_INVOKE_TYPED(
          nsCOMPtr<mozIStorageStatement>, mStorageConnection, CreateStatement,
          "UPDATE cache SET valid = :valid, build_id = :buildId;"_ns),
      QM_VOID);

  QM_TRY(stmt->BindInt32ByName("valid"_ns, 1), QM_VOID);
  QM_TRY(stmt->BindUTF8StringByName("buildId"_ns, *gBuildId), QM_VOID);
  QM_TRY(stmt->Execute(), QM_VOID);
  QM_TRY(transaction.Commit(), QM_VOID);
}

already_AddRefed<QuotaObject> QuotaManager::GetQuotaObject(
    PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata,
    Client::Type aClientType, nsIFile* aFile, int64_t aFileSize,
    int64_t* aFileSizeOut /* = nullptr */) {
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (aFileSizeOut) {
    *aFileSizeOut = 0;
  }

  if (aPersistenceType == PERSISTENCE_TYPE_PERSISTENT) {
    return nullptr;
  }

  QM_TRY_INSPECT(const auto& path,
                 MOZ_TO_RESULT_INVOKE_TYPED(nsString, aFile, GetPath), nullptr);

#ifdef DEBUG
  {
    QM_TRY_INSPECT(
        const auto& directory,
        GetDirectoryForOrigin(aPersistenceType, aOriginMetadata.mOrigin),
        nullptr);

    nsAutoString clientType;
    QM_TRY(OkIf(Client::TypeToText(aClientType, clientType, fallible)),
           nullptr);

    QM_TRY(directory->Append(clientType), nullptr);

    QM_TRY_INSPECT(const auto& directoryPath,
                   MOZ_TO_RESULT_INVOKE_TYPED(nsString, directory, GetPath),
                   nullptr);

    MOZ_ASSERT(StringBeginsWith(path, directoryPath));
  }
#endif

  QM_TRY_INSPECT(const int64_t fileSize,
                 ([&aFile, aFileSize]() -> Result<int64_t, nsresult> {
                   if (aFileSize == -1) {
                     QM_TRY_INSPECT(const bool& exists,
                                    MOZ_TO_RESULT_INVOKE(aFile, Exists));

                     if (exists) {
                       QM_TRY_RETURN(MOZ_TO_RESULT_INVOKE(aFile, GetFileSize));
                     }

                     return 0;
                   }

                   return aFileSize;
                 }()),
                 nullptr);

  RefPtr<QuotaObject> result;
  {
    MutexAutoLock lock(mQuotaMutex);

    GroupInfoPair* pair;
    if (!mGroupInfoPairs.Get(aOriginMetadata.mGroup, &pair)) {
      return nullptr;
    }

    RefPtr<GroupInfo> groupInfo = pair->LockedGetGroupInfo(aPersistenceType);

    if (!groupInfo) {
      return nullptr;
    }

    RefPtr<OriginInfo> originInfo =
        groupInfo->LockedGetOriginInfo(aOriginMetadata.mOrigin);

    if (!originInfo) {
      return nullptr;
    }

    // We need this extra raw pointer because we can't assign to the smart
    // pointer directly since QuotaObject::AddRef would try to acquire the same
    // mutex.
    const NotNull<QuotaObject*> quotaObject =
        originInfo->mQuotaObjects.LookupOrInsertWith(path, [&] {
          // Create a new QuotaObject. The hashtable is not responsible to
          // delete the QuotaObject.
          return WrapNotNullUnchecked(
              new QuotaObject(originInfo, aClientType, path, fileSize));
        });

    // Addref the QuotaObject and move the ownership to the result. This must
    // happen before we unlock!
    result = quotaObject->LockedAddRef();
  }

  if (aFileSizeOut) {
    *aFileSizeOut = fileSize;
  }

  // The caller becomes the owner of the QuotaObject, that is, the caller is
  // is responsible to delete it when the last reference is removed.
  return result.forget();
}

already_AddRefed<QuotaObject> QuotaManager::GetQuotaObject(
    PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata,
    Client::Type aClientType, const nsAString& aPath, int64_t aFileSize,
    int64_t* aFileSizeOut /* = nullptr */) {
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (aFileSizeOut) {
    *aFileSizeOut = 0;
  }

  QM_TRY_INSPECT(const auto& file, QM_NewLocalFile(aPath), nullptr);

  return GetQuotaObject(aPersistenceType, aOriginMetadata, aClientType, file,
                        aFileSize, aFileSizeOut);
}

already_AddRefed<QuotaObject> QuotaManager::GetQuotaObject(
    const int64_t aDirectoryLockId, const nsAString& aPath) {
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  Maybe<MutexAutoLock> lock;

  // See the comment for mDirectoryLockIdTable in QuotaManager.h
  if (!IsOnBackgroundThread()) {
    lock.emplace(mQuotaMutex);
  }

  if (auto maybeDirectoryLock =
          mDirectoryLockIdTable.MaybeGet(aDirectoryLockId)) {
    const auto& directoryLock = *maybeDirectoryLock;
    MOZ_DIAGNOSTIC_ASSERT(directoryLock->ShouldUpdateLockIdTable());

    const PersistenceType persistenceType = directoryLock->GetPersistenceType();
    const OriginMetadata& originMetadata = directoryLock->OriginMetadata();
    const Client::Type clientType = directoryLock->ClientType();

    lock.reset();

    return GetQuotaObject(persistenceType, originMetadata, clientType, aPath);
  }

  MOZ_CRASH("Getting quota object for an unregistered directory lock?");
}

Nullable<bool> QuotaManager::OriginPersisted(
    const OriginMetadata& aOriginMetadata) {
  AssertIsOnIOThread();

  MutexAutoLock lock(mQuotaMutex);

  RefPtr<OriginInfo> originInfo =
      LockedGetOriginInfo(PERSISTENCE_TYPE_DEFAULT, aOriginMetadata);
  if (originInfo) {
    return Nullable<bool>(originInfo->LockedPersisted());
  }

  return Nullable<bool>();
}

void QuotaManager::PersistOrigin(const OriginMetadata& aOriginMetadata) {
  AssertIsOnIOThread();

  MutexAutoLock lock(mQuotaMutex);

  RefPtr<OriginInfo> originInfo =
      LockedGetOriginInfo(PERSISTENCE_TYPE_DEFAULT, aOriginMetadata);
  if (originInfo && !originInfo->LockedPersisted()) {
    originInfo->LockedPersist();
  }
}

void QuotaManager::AbortOperationsForLocks(
    const DirectoryLockIdTableArray& aLockIds) {
  for (Client::Type type : AllClientTypes()) {
    if (aLockIds[type].Filled()) {
      (*mClients)[type]->AbortOperationsForLocks(aLockIds[type]);
    }
  }
}

void QuotaManager::AbortOperationsForProcess(ContentParentId aContentParentId) {
  AssertIsOnOwningThread();

  for (const RefPtr<Client>& client : *mClients) {
    client->AbortOperationsForProcess(aContentParentId);
  }
}

Result<nsCOMPtr<nsIFile>, nsresult> QuotaManager::GetDirectoryForOrigin(
    PersistenceType aPersistenceType, const nsACString& aASCIIOrigin) const {
  QM_TRY_UNWRAP(auto directory,
                QM_NewLocalFile(GetStoragePath(aPersistenceType)));

  QM_TRY(directory->Append(MakeSanitizedOriginString(aASCIIOrigin)));

  return directory;
}

nsresult QuotaManager::RestoreDirectoryMetadata2(nsIFile* aDirectory) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(mStorageConnection);

  RefPtr<RestoreDirectoryMetadata2Helper> helper =
      new RestoreDirectoryMetadata2Helper(aDirectory);

  QM_TRY(helper->Init());

  QM_TRY(helper->RestoreMetadata2File());

  return NS_OK;
}

Result<FullOriginMetadata, nsresult> QuotaManager::LoadFullOriginMetadata(
    nsIFile* aDirectory, PersistenceType aPersistenceType) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(mStorageConnection);

  QM_TRY_INSPECT(const auto& binaryStream,
                 GetBinaryInputStream(*aDirectory,
                                      nsLiteralString(METADATA_V2_FILE_NAME)));

  FullOriginMetadata fullOriginMetadata;

  QM_TRY_UNWRAP(fullOriginMetadata.mLastAccessTime,
                MOZ_TO_RESULT_INVOKE(binaryStream, Read64));

  QM_TRY_UNWRAP(fullOriginMetadata.mPersisted,
                MOZ_TO_RESULT_INVOKE(binaryStream, ReadBoolean));

  QM_TRY_INSPECT(const bool& reservedData1,
                 MOZ_TO_RESULT_INVOKE(binaryStream, Read32));
  Unused << reservedData1;

  // XXX Use for the persistence type.
  QM_TRY_INSPECT(const bool& reservedData2,
                 MOZ_TO_RESULT_INVOKE(binaryStream, Read32));
  Unused << reservedData2;

  fullOriginMetadata.mPersistenceType = aPersistenceType;

  QM_TRY_UNWRAP(
      fullOriginMetadata.mSuffix,
      MOZ_TO_RESULT_INVOKE_TYPED(nsCString, binaryStream, ReadCString));

  QM_TRY_UNWRAP(
      fullOriginMetadata.mGroup,
      MOZ_TO_RESULT_INVOKE_TYPED(nsCString, binaryStream, ReadCString));

  QM_TRY_UNWRAP(
      fullOriginMetadata.mOrigin,
      MOZ_TO_RESULT_INVOKE_TYPED(nsCString, binaryStream, ReadCString));

  // Currently unused (used to be isApp).
  QM_TRY_INSPECT(const bool& dummy,
                 MOZ_TO_RESULT_INVOKE(binaryStream, ReadBoolean));
  Unused << dummy;

  QM_TRY(binaryStream->Close());

  QM_TRY_INSPECT(const bool& updated,
                 MaybeUpdateGroupForOrigin(fullOriginMetadata));

  if (updated) {
    // Only overwriting .metadata-v2 (used to overwrite .metadata too) to reduce
    // I/O.
    QM_TRY(CreateDirectoryMetadata2(
        *aDirectory, fullOriginMetadata.mLastAccessTime,
        fullOriginMetadata.mPersisted, fullOriginMetadata));
  }

  return fullOriginMetadata;
}

Result<FullOriginMetadata, nsresult>
QuotaManager::LoadFullOriginMetadataWithRestore(nsIFile* aDirectory) {
  // XXX Once the persistence type is stored in the metadata file, this block
  // for getting the persistence type from the parent directory name can be
  // removed.
  nsCOMPtr<nsIFile> parentDir;
  QM_TRY(aDirectory->GetParent(getter_AddRefs(parentDir)));

  const auto maybePersistenceType =
      PersistenceTypeFromFile(*parentDir, fallible);
  QM_TRY(OkIf(maybePersistenceType.isSome()), Err(NS_ERROR_FAILURE));

  const auto& persistenceType = maybePersistenceType.value();

  QM_TRY_RETURN(QM_OR_ELSE_WARN(
      // Expression.
      LoadFullOriginMetadata(aDirectory, persistenceType),
      // Fallback.
      ([&aDirectory, &persistenceType,
        this](const nsresult rv) -> Result<FullOriginMetadata, nsresult> {
        QM_TRY(RestoreDirectoryMetadata2(aDirectory));

        QM_TRY_RETURN(LoadFullOriginMetadata(aDirectory, persistenceType));
      })));
}

nsresult QuotaManager::InitializeRepository(PersistenceType aPersistenceType) {
  MOZ_ASSERT(aPersistenceType == PERSISTENCE_TYPE_TEMPORARY ||
             aPersistenceType == PERSISTENCE_TYPE_DEFAULT);

  QM_TRY_INSPECT(const auto& directory,
                 QM_NewLocalFile(GetStoragePath(aPersistenceType)));

  QM_TRY_INSPECT(const bool& created, EnsureDirectory(*directory));

  Unused << created;

  // A keeper to defer the return only in Nightly, so that the telemetry data
  // for whole profile can be collected
#ifdef NIGHTLY_BUILD
  nsresult statusKeeper = NS_OK;
#endif

  const auto statusKeeperFunc = [&](const nsresult rv) {
    RECORD_IN_NIGHTLY(statusKeeper, rv);
  };

  struct RenameAndInitInfo {
    nsCOMPtr<nsIFile> mOriginDirectory;
    FullOriginMetadata mFullOriginMetadata;
    int64_t mTimestamp;
    bool mPersisted;
  };
  nsTArray<RenameAndInitInfo> renameAndInitInfos;

  QM_TRY(([&]() -> Result<Ok, nsresult> {
    QM_TRY(
        CollectEachFile(
            *directory,
            [&](nsCOMPtr<nsIFile>&& childDirectory) -> Result<Ok, nsresult> {
              if (NS_WARN_IF(IsShuttingDown())) {
                RETURN_STATUS_OR_RESULT(statusKeeper, NS_ERROR_ABORT);
              }

              QM_TRY(
                  ([this, &childDirectory, &renameAndInitInfos,
                    aPersistenceType]() -> Result<Ok, nsresult> {
                    QM_TRY_INSPECT(
                        const auto& leafName,
                        MOZ_TO_RESULT_INVOKE_TYPED(nsAutoString, childDirectory,
                                                   GetLeafName));

                    QM_TRY_INSPECT(const auto& dirEntryKind,
                                   GetDirEntryKind(*childDirectory));

                    switch (dirEntryKind) {
                      case nsIFileKind::ExistsAsDirectory: {
                        QM_TRY_UNWRAP(
                            auto metadata,
                            LoadFullOriginMetadataWithRestore(childDirectory));

                        MOZ_ASSERT(metadata.mPersistenceType ==
                                   aPersistenceType);

                        // FIXME(tt): The check for origin name consistency can
                        // be removed once we have an upgrade to traverse origin
                        // directories and check through the directory metadata
                        // files.
                        const auto originSanitized =
                            MakeSanitizedOriginCString(metadata.mOrigin);

                        NS_ConvertUTF16toUTF8 utf8LeafName(leafName);
                        if (!originSanitized.Equals(utf8LeafName)) {
                          QM_WARNING(
                              "The name of the origin directory (%s) doesn't "
                              "match the sanitized origin string (%s) in the "
                              "metadata file!",
                              utf8LeafName.get(), originSanitized.get());

                          // If it's the known case, we try to restore the
                          // origin directory name if it's possible.
                          if (originSanitized.Equals(utf8LeafName + "."_ns)) {
                            const int64_t lastAccessTime =
                                metadata.mLastAccessTime;
                            const bool persisted = metadata.mPersisted;
                            renameAndInitInfos.AppendElement(RenameAndInitInfo{
                                std::move(childDirectory), std::move(metadata),
                                lastAccessTime, persisted});
                            break;
                          }

                          // XXXtt: Try to restore the unknown cases base on the
                          // content for their metadata files. Note that if the
                          // restore fails, QM should maintain a list and ensure
                          // they won't be accessed after initialization.
                        }

                        QM_TRY(QM_OR_ELSE_WARN_IF(
                            // Expression.
                            ToResult(InitializeOrigin(
                                aPersistenceType, metadata,
                                metadata.mLastAccessTime, metadata.mPersisted,
                                childDirectory)),
                            // Predicate.
                            IsDatabaseCorruptionError,
                            // Fallback.
                            ([&childDirectory](
                                 const nsresult rv) -> Result<Ok, nsresult> {
                              // If the origin can't be initialized due to
                              // corruption, this is a permanent
                              // condition, and we need to remove all data
                              // for the origin on disk.

                              QM_TRY(childDirectory->Remove(true));

                              return Ok{};
                            })));

                        break;
                      }

                      case nsIFileKind::ExistsAsFile:
                        if (IsOSMetadata(leafName) || IsDotFile(leafName)) {
                          break;
                        }

                        // Unknown files during initialization are now allowed.
                        // Just warn if we find them.
                        UNKNOWN_FILE_WARNING(leafName);
                        break;

                      case nsIFileKind::DoesNotExist:
                        // Ignore files that got removed externally while
                        // iterating.
                        break;
                    }

                    return Ok{};
                  }()),
                  OK_IN_NIGHTLY_PROPAGATE_IN_OTHERS, statusKeeperFunc);

              return Ok{};
            }),
        OK_IN_NIGHTLY_PROPAGATE_IN_OTHERS, statusKeeperFunc);

    return Ok{};
  }()));

  for (const auto& info : renameAndInitInfos) {
    QM_TRY(([&]() -> Result<Ok, nsresult> {
      QM_TRY(([&directory, &info, this,
               aPersistenceType]() -> Result<Ok, nsresult> {
               const auto originDirName =
                   MakeSanitizedOriginString(info.mFullOriginMetadata.mOrigin);

               // Check if targetDirectory exist.
               QM_TRY_INSPECT(const auto& targetDirectory,
                              CloneFileAndAppend(*directory, originDirName));

               QM_TRY_INSPECT(const bool& exists,
                              MOZ_TO_RESULT_INVOKE(targetDirectory, Exists));

               if (exists) {
                 QM_TRY(info.mOriginDirectory->Remove(true));

                 return Ok{};
               }

               QM_TRY(info.mOriginDirectory->RenameTo(nullptr, originDirName));

               QM_TRY(InitializeOrigin(
                   aPersistenceType, info.mFullOriginMetadata, info.mTimestamp,
                   info.mPersisted, targetDirectory));

               return Ok{};
             }()),
             OK_IN_NIGHTLY_PROPAGATE_IN_OTHERS, statusKeeperFunc);

      return Ok{};
    }()));
  }

#ifdef NIGHTLY_BUILD
  if (NS_FAILED(statusKeeper)) {
    return statusKeeper;
  }
#endif

  return NS_OK;
}

nsresult QuotaManager::InitializeOrigin(PersistenceType aPersistenceType,
                                        const OriginMetadata& aOriginMetadata,
                                        int64_t aAccessTime, bool aPersisted,
                                        nsIFile* aDirectory) {
  AssertIsOnIOThread();

  const bool trackQuota = aPersistenceType != PERSISTENCE_TYPE_PERSISTENT;

  // We need to initialize directories of all clients if they exists and also
  // get the total usage to initialize the quota.

  ClientUsageArray clientUsages;

  // A keeper to defer the return only in Nightly, so that the telemetry data
  // for whole profile can be collected
#ifdef NIGHTLY_BUILD
  nsresult statusKeeper = NS_OK;
#endif

  QM_TRY(([&, statusKeeperFunc = [&](const nsresult rv) {
            RECORD_IN_NIGHTLY(statusKeeper, rv);
          }]() -> Result<Ok, nsresult> {
    QM_TRY(
        CollectEachFile(
            *aDirectory,
            [&](const nsCOMPtr<nsIFile>& file) -> Result<Ok, nsresult> {
              if (NS_WARN_IF(IsShuttingDown())) {
                RETURN_STATUS_OR_RESULT(statusKeeper, NS_ERROR_ABORT);
              }

              QM_TRY(
                  ([this, &file, trackQuota, aPersistenceType, &aOriginMetadata,
                    &clientUsages]() -> Result<Ok, nsresult> {
                    QM_TRY_INSPECT(const auto& leafName,
                                   MOZ_TO_RESULT_INVOKE_TYPED(
                                       nsAutoString, file, GetLeafName));

                    QM_TRY_INSPECT(const auto& dirEntryKind,
                                   GetDirEntryKind(*file));

                    switch (dirEntryKind) {
                      case nsIFileKind::ExistsAsDirectory: {
                        Client::Type clientType;
                        const bool ok = Client::TypeFromText(
                            leafName, clientType, fallible);
                        if (!ok) {
                          // Unknown directories during initialization are now
                          // allowed. Just warn if we find them.
                          UNKNOWN_FILE_WARNING(leafName);
                          break;
                        }

                        if (trackQuota) {
                          QM_TRY_INSPECT(
                              const auto& usageInfo,
                              (*mClients)[clientType]->InitOrigin(
                                  aPersistenceType, aOriginMetadata,
                                  /* aCanceled */ Atomic<bool>(false)));

                          MOZ_ASSERT(!clientUsages[clientType]);

                          if (usageInfo.TotalUsage()) {
                            // XXX(Bug 1683863) Until we identify the root cause
                            // of seemingly converted-from-negative usage
                            // values, we will just treat them as unset here,
                            // but log a warning to the browser console.
                            if (static_cast<int64_t>(*usageInfo.TotalUsage()) >=
                                0) {
                              clientUsages[clientType] = usageInfo.TotalUsage();
                            } else {
#if defined(EARLY_BETA_OR_EARLIER) || defined(DEBUG)
                              const nsCOMPtr<nsIConsoleService> console =
                                  do_GetService(NS_CONSOLESERVICE_CONTRACTID);
                              if (console) {
                                console->LogStringMessage(
                                    nsString(
                                        u"QuotaManager warning: client "_ns +
                                        leafName +
                                        u" reported negative usage for group "_ns +
                                        NS_ConvertUTF8toUTF16(
                                            aOriginMetadata.mGroup) +
                                        u", origin "_ns +
                                        NS_ConvertUTF8toUTF16(
                                            aOriginMetadata.mOrigin))
                                        .get());
                              }
#endif
                            }
                          }
                        } else {
                          QM_TRY((*mClients)[clientType]
                                     ->InitOriginWithoutTracking(
                                         aPersistenceType, aOriginMetadata,
                                         /* aCanceled */ Atomic<bool>(false)));
                        }

                        break;
                      }

                      case nsIFileKind::ExistsAsFile:
                        if (IsOriginMetadata(leafName)) {
                          break;
                        }

                        if (IsTempMetadata(leafName)) {
                          QM_TRY(file->Remove(/* recursive */ false));

                          break;
                        }

                        if (IsOSMetadata(leafName) || IsDotFile(leafName)) {
                          break;
                        }

                        // Unknown files during initialization are now allowed.
                        // Just warn if we find them.
                        UNKNOWN_FILE_WARNING(leafName);
                        // Bug 1595448 will handle the case for unknown files
                        // like idb, cache, or ls.
                        break;

                      case nsIFileKind::DoesNotExist:
                        // Ignore files that got removed externally while
                        // iterating.
                        break;
                    }

                    return Ok{};
                  }()),
                  OK_IN_NIGHTLY_PROPAGATE_IN_OTHERS, statusKeeperFunc);

              return Ok{};
            }),
        OK_IN_NIGHTLY_PROPAGATE_IN_OTHERS, statusKeeperFunc);

    return Ok{};
  }()));

#ifdef NIGHTLY_BUILD
  if (NS_FAILED(statusKeeper)) {
    return statusKeeper;
  }
#endif

  if (trackQuota) {
    const auto usage = std::accumulate(
        clientUsages.cbegin(), clientUsages.cend(), CheckedUint64(0),
        [](CheckedUint64 value, const Maybe<uint64_t>& clientUsage) {
          return value + clientUsage.valueOr(0);
        });

    // XXX Should we log more information, i.e. the whole clientUsages array, in
    // case usage is not valid?

    QM_TRY(OkIf(usage.isValid()), NS_ERROR_FAILURE);

    InitQuotaForOrigin(
        FullOriginMetadata{aOriginMetadata, aPersisted, aAccessTime},
        clientUsages, usage.value());
  }

  return NS_OK;
}

nsresult
QuotaManager::UpgradeFromIndexedDBDirectoryToPersistentStorageDirectory(
    nsIFile* aIndexedDBDir) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aIndexedDBDir);

  const auto innerFunc = [this, &aIndexedDBDir](const auto&) -> nsresult {
    bool isDirectory;
    QM_TRY(aIndexedDBDir->IsDirectory(&isDirectory));

    if (!isDirectory) {
      NS_WARNING("indexedDB entry is not a directory!");
      return NS_OK;
    }

    auto persistentStorageDirOrErr = QM_NewLocalFile(*mStoragePath);
    if (NS_WARN_IF(persistentStorageDirOrErr.isErr())) {
      return persistentStorageDirOrErr.unwrapErr();
    }

    nsCOMPtr<nsIFile> persistentStorageDir = persistentStorageDirOrErr.unwrap();

    QM_TRY(persistentStorageDir->Append(
        nsLiteralString(PERSISTENT_DIRECTORY_NAME)));

    bool exists;
    QM_TRY(persistentStorageDir->Exists(&exists));

    if (exists) {
      QM_WARNING("Deleting old <profile>/indexedDB directory!");

      QM_TRY(aIndexedDBDir->Remove(/* aRecursive */ true));

      return NS_OK;
    }

    nsCOMPtr<nsIFile> storageDir;
    QM_TRY(persistentStorageDir->GetParent(getter_AddRefs(storageDir)));

    // MoveTo() is atomic if the move happens on the same volume which should
    // be our case, so even if we crash in the middle of the operation nothing
    // breaks next time we try to initialize.
    // However there's a theoretical possibility that the indexedDB directory
    // is on different volume, but it should be rare enough that we don't have
    // to worry about it.
    QM_TRY(aIndexedDBDir->MoveTo(storageDir,
                                 nsLiteralString(PERSISTENT_DIRECTORY_NAME)));

    return NS_OK;
  };

  return ExecuteInitialization(Initialization::UpgradeFromIndexedDBDirectory,
                               innerFunc);
}

nsresult
QuotaManager::UpgradeFromPersistentStorageDirectoryToDefaultStorageDirectory(
    nsIFile* aPersistentStorageDir) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aPersistentStorageDir);

  const auto innerFunc = [this,
                          &aPersistentStorageDir](const auto&) -> nsresult {
    QM_TRY_INSPECT(const bool& isDirectory,
                   MOZ_TO_RESULT_INVOKE(aPersistentStorageDir, IsDirectory));

    if (!isDirectory) {
      NS_WARNING("persistent entry is not a directory!");
      return NS_OK;
    }

    {
      QM_TRY_INSPECT(const auto& defaultStorageDir,
                     QM_NewLocalFile(*mDefaultStoragePath));

      QM_TRY_INSPECT(const bool& exists,
                     MOZ_TO_RESULT_INVOKE(defaultStorageDir, Exists));

      if (exists) {
        QM_WARNING("Deleting old <profile>/storage/persistent directory!");

        QM_TRY(aPersistentStorageDir->Remove(/* aRecursive */ true));

        return NS_OK;
      }
    }

    {
      // Create real metadata files for origin directories in persistent
      // storage.
      auto helper = MakeRefPtr<CreateOrUpgradeDirectoryMetadataHelper>(
          aPersistentStorageDir);

      QM_TRY(helper->Init());

      QM_TRY(helper->ProcessRepository());

      // Upgrade metadata files for origin directories in temporary storage.
      QM_TRY_INSPECT(const auto& temporaryStorageDir,
                     QM_NewLocalFile(*mTemporaryStoragePath));

      QM_TRY_INSPECT(const bool& exists,
                     MOZ_TO_RESULT_INVOKE(temporaryStorageDir, Exists));

      if (exists) {
        QM_TRY_INSPECT(const bool& isDirectory,
                       MOZ_TO_RESULT_INVOKE(temporaryStorageDir, IsDirectory));

        if (!isDirectory) {
          NS_WARNING("temporary entry is not a directory!");
          return NS_OK;
        }

        helper = MakeRefPtr<CreateOrUpgradeDirectoryMetadataHelper>(
            temporaryStorageDir);

        QM_TRY(helper->Init());

        QM_TRY(helper->ProcessRepository());
      }
    }

    // And finally rename persistent to default.
    QM_TRY(aPersistentStorageDir->RenameTo(
        nullptr, nsLiteralString(DEFAULT_DIRECTORY_NAME)));

    return NS_OK;
  };

  return ExecuteInitialization(
      Initialization::UpgradeFromPersistentStorageDirectory, innerFunc);
}

template <typename Helper>
nsresult QuotaManager::UpgradeStorage(const int32_t aOldVersion,
                                      const int32_t aNewVersion,
                                      mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aNewVersion > aOldVersion);
  MOZ_ASSERT(aNewVersion <= kStorageVersion);
  MOZ_ASSERT(aConnection);

  for (const PersistenceType persistenceType : kAllPersistenceTypes) {
    QM_TRY_UNWRAP(auto directory,
                  QM_NewLocalFile(GetStoragePath(persistenceType)));

    QM_TRY_INSPECT(const bool& exists, MOZ_TO_RESULT_INVOKE(directory, Exists));

    if (!exists) {
      continue;
    }

    RefPtr<UpgradeStorageHelperBase> helper = new Helper(directory);

    QM_TRY(helper->Init());

    QM_TRY(helper->ProcessRepository());
  }

#ifdef DEBUG
  {
    QM_TRY_INSPECT(const int32_t& storageVersion,
                   MOZ_TO_RESULT_INVOKE(aConnection, GetSchemaVersion));

    MOZ_ASSERT(storageVersion == aOldVersion);
  }
#endif

  QM_TRY(aConnection->SetSchemaVersion(aNewVersion));

  return NS_OK;
}

nsresult QuotaManager::UpgradeStorageFrom0_0To1_0(
    mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  const auto innerFunc = [this, &aConnection](const auto&) -> nsresult {
    QM_TRY(UpgradeStorage<UpgradeStorageFrom0_0To1_0Helper>(
        0, MakeStorageVersion(1, 0), aConnection));

    return NS_OK;
  };

  return ExecuteInitialization(Initialization::UpgradeStorageFrom0_0To1_0,
                               innerFunc);
}

nsresult QuotaManager::UpgradeStorageFrom1_0To2_0(
    mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  // The upgrade consists of a number of logically distinct bugs that
  // intentionally got fixed at the same time to trigger just one major
  // version bump.
  //
  //
  // Morgue directory cleanup
  // [Feature/Bug]:
  // The original bug that added "on demand" morgue cleanup is 1165119.
  //
  // [Mutations]:
  // Morgue directories are removed from all origin directories during the
  // upgrade process. Origin initialization and usage calculation doesn't try
  // to remove morgue directories anymore.
  //
  // [Downgrade-incompatible changes]:
  // Morgue directories can reappear if user runs an already upgraded profile
  // in an older version of Firefox. Morgue directories then prevent current
  // Firefox from initializing and using the storage.
  //
  //
  // App data removal
  // [Feature/Bug]:
  // The bug that removes isApp flags is 1311057.
  //
  // [Mutations]:
  // Origin directories with appIds are removed during the upgrade process.
  //
  // [Downgrade-incompatible changes]:
  // Origin directories with appIds can reappear if user runs an already
  // upgraded profile in an older version of Firefox. Origin directories with
  // appIds don't prevent current Firefox from initializing and using the
  // storage, but they wouldn't ever be removed again, potentially causing
  // problems once appId is removed from origin attributes.
  //
  //
  // Strip obsolete origin attributes
  // [Feature/Bug]:
  // The bug that strips obsolete origin attributes is 1314361.
  //
  // [Mutations]:
  // Origin directories with obsolete origin attributes are renamed and their
  // metadata files are updated during the upgrade process.
  //
  // [Downgrade-incompatible changes]:
  // Origin directories with obsolete origin attributes can reappear if user
  // runs an already upgraded profile in an older version of Firefox. Origin
  // directories with obsolete origin attributes don't prevent current Firefox
  // from initializing and using the storage, but they wouldn't ever be upgraded
  // again, potentially causing problems in future.
  //
  //
  // File manager directory renaming (client specific)
  // [Feature/Bug]:
  // The original bug that added "on demand" file manager directory renaming is
  // 1056939.
  //
  // [Mutations]:
  // All file manager directories are renamed to contain the ".files" suffix.
  //
  // [Downgrade-incompatible changes]:
  // File manager directories with the ".files" suffix prevent older versions of
  // Firefox from initializing and using the storage.
  // File manager directories without the ".files" suffix can appear if user
  // runs an already upgraded profile in an older version of Firefox. File
  // manager directories without the ".files" suffix then prevent current
  // Firefox from initializing and using the storage.

  const auto innerFunc = [this, &aConnection](const auto&) -> nsresult {
    QM_TRY(UpgradeStorage<UpgradeStorageFrom1_0To2_0Helper>(
        MakeStorageVersion(1, 0), MakeStorageVersion(2, 0), aConnection));

    return NS_OK;
  };

  return ExecuteInitialization(Initialization::UpgradeStorageFrom1_0To2_0,
                               innerFunc);
}

nsresult QuotaManager::UpgradeStorageFrom2_0To2_1(
    mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  // The upgrade is mainly to create a directory padding file in DOM Cache
  // directory to record the overall padding size of an origin.

  const auto innerFunc = [this, &aConnection](const auto&) -> nsresult {
    QM_TRY(UpgradeStorage<UpgradeStorageFrom2_0To2_1Helper>(
        MakeStorageVersion(2, 0), MakeStorageVersion(2, 1), aConnection));

    return NS_OK;
  };

  return ExecuteInitialization(Initialization::UpgradeStorageFrom2_0To2_1,
                               innerFunc);
}

nsresult QuotaManager::UpgradeStorageFrom2_1To2_2(
    mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  // The upgrade is mainly to clean obsolete origins in the repositoies, remove
  // asmjs client, and ".tmp" file in the idb folers.

  const auto innerFunc = [this, &aConnection](const auto&) -> nsresult {
    QM_TRY(UpgradeStorage<UpgradeStorageFrom2_1To2_2Helper>(
        MakeStorageVersion(2, 1), MakeStorageVersion(2, 2), aConnection));

    return NS_OK;
  };

  return ExecuteInitialization(Initialization::UpgradeStorageFrom2_1To2_2,
                               innerFunc);
}

nsresult QuotaManager::UpgradeStorageFrom2_2To2_3(
    mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  const auto innerFunc = [&aConnection](const auto&) -> nsresult {
    // Table `database`
    QM_TRY(aConnection->ExecuteSimpleSQL(
        nsLiteralCString("CREATE TABLE database"
                         "( cache_version INTEGER NOT NULL DEFAULT 0"
                         ");")));

    QM_TRY(aConnection->ExecuteSimpleSQL(
        nsLiteralCString("INSERT INTO database (cache_version) "
                         "VALUES (0)")));

#ifdef DEBUG
    {
      QM_TRY_INSPECT(const int32_t& storageVersion,
                     MOZ_TO_RESULT_INVOKE(aConnection, GetSchemaVersion));

      MOZ_ASSERT(storageVersion == MakeStorageVersion(2, 2));
    }
#endif

    QM_TRY(aConnection->SetSchemaVersion(MakeStorageVersion(2, 3)));

    return NS_OK;
  };

  return ExecuteInitialization(Initialization::UpgradeStorageFrom2_2To2_3,
                               innerFunc);
}

nsresult QuotaManager::MaybeRemoveLocalStorageDataAndArchive(
    nsIFile& aLsArchiveFile) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!CachedNextGenLocalStorageEnabled());

  QM_TRY_INSPECT(const bool& exists,
                 MOZ_TO_RESULT_INVOKE(aLsArchiveFile, Exists));

  if (!exists) {
    // If the ls archive doesn't exist then ls directories can't exist either.
    return NS_OK;
  }

  QM_TRY(MaybeRemoveLocalStorageDirectories());

  InvalidateQuotaCache();

  // Finally remove the ls archive, so we don't have to check all origin
  // directories next time this method is called.
  QM_TRY(aLsArchiveFile.Remove(false));

  return NS_OK;
}

nsresult QuotaManager::MaybeRemoveLocalStorageDirectories() {
  AssertIsOnIOThread();

  QM_TRY_INSPECT(const auto& defaultStorageDir,
                 QM_NewLocalFile(*mDefaultStoragePath));

  QM_TRY_INSPECT(const bool& exists,
                 MOZ_TO_RESULT_INVOKE(defaultStorageDir, Exists));

  if (!exists) {
    return NS_OK;
  }

  QM_TRY(CollectEachFile(
      *defaultStorageDir,
      [](const nsCOMPtr<nsIFile>& originDir) -> Result<Ok, nsresult> {
#ifdef DEBUG
        {
          QM_TRY_INSPECT(const bool& exists,
                         MOZ_TO_RESULT_INVOKE(originDir, Exists));
          MOZ_ASSERT(exists);
        }
#endif

        QM_TRY_INSPECT(const auto& dirEntryKind, GetDirEntryKind(*originDir));

        switch (dirEntryKind) {
          case nsIFileKind::ExistsAsDirectory: {
            QM_TRY_INSPECT(
                const auto& lsDir,
                CloneFileAndAppend(*originDir, NS_LITERAL_STRING_FROM_CSTRING(
                                                   LS_DIRECTORY_NAME)));

            {
              QM_TRY_INSPECT(const bool& exists,
                             MOZ_TO_RESULT_INVOKE(lsDir, Exists));

              if (!exists) {
                return Ok{};
              }
            }

            {
              QM_TRY_INSPECT(const bool& isDirectory,
                             MOZ_TO_RESULT_INVOKE(lsDir, IsDirectory));

              if (!isDirectory) {
                QM_WARNING("ls entry is not a directory!");

                return Ok{};
              }
            }

            nsString path;
            QM_TRY(lsDir->GetPath(path));

            QM_WARNING("Deleting %s directory!",
                       NS_ConvertUTF16toUTF8(path).get());

            QM_TRY(lsDir->Remove(/* aRecursive */ true));

            break;
          }

          case nsIFileKind::ExistsAsFile: {
            QM_TRY_INSPECT(const auto& leafName,
                           MOZ_TO_RESULT_INVOKE_TYPED(nsAutoString, originDir,
                                                      GetLeafName));

            // Unknown files during upgrade are allowed. Just warn if we find
            // them.
            if (!IsOSMetadata(leafName)) {
              UNKNOWN_FILE_WARNING(leafName);
            }

            break;
          }

          case nsIFileKind::DoesNotExist:
            // Ignore files that got removed externally while iterating.
            break;
        }
        return Ok{};
      }));

  return NS_OK;
}

Result<Ok, nsresult> QuotaManager::CopyLocalStorageArchiveFromWebAppsStore(
    nsIFile& aLsArchiveFile) const {
  AssertIsOnIOThread();
  MOZ_ASSERT(CachedNextGenLocalStorageEnabled());

#ifdef DEBUG
  {
    QM_TRY_INSPECT(const bool& exists,
                   MOZ_TO_RESULT_INVOKE(aLsArchiveFile, Exists));
    MOZ_ASSERT(!exists);
  }
#endif

  // Get the storage service first, we will need it at multiple places.
  QM_TRY_INSPECT(const auto& ss, ToResultGet<nsCOMPtr<mozIStorageService>>(
                                     MOZ_SELECT_OVERLOAD(do_GetService),
                                     MOZ_STORAGE_SERVICE_CONTRACTID));

  // Get the web apps store file.
  QM_TRY_INSPECT(const auto& webAppsStoreFile, QM_NewLocalFile(mBasePath));

  QM_TRY(webAppsStoreFile->Append(nsLiteralString(WEB_APPS_STORE_FILE_NAME)));

  // Now check if the web apps store is useable.
  QM_TRY_INSPECT(const auto& connection,
                 CreateWebAppsStoreConnection(*webAppsStoreFile, *ss));

  if (connection) {
    // Find out the journal mode.
    QM_TRY_INSPECT(const auto& stmt,
                   CreateAndExecuteSingleStepStatement(
                       *connection, "PRAGMA journal_mode;"_ns));

    QM_TRY_INSPECT(
        const auto& journalMode,
        MOZ_TO_RESULT_INVOKE_TYPED(nsAutoCString, *stmt, GetUTF8String, 0));

    QM_TRY(stmt->Finalize());

    if (journalMode.EqualsLiteral("wal")) {
      // We don't copy the WAL file, so make sure the old database is fully
      // checkpointed.
      QM_TRY(
          connection->ExecuteSimpleSQL("PRAGMA wal_checkpoint(TRUNCATE);"_ns));
    }

    // Explicitely close the connection before the old database is copied.
    QM_TRY(connection->Close());

    // Copy the old database. The database is copied from
    // <profile>/webappsstore.sqlite to
    // <profile>/storage/ls-archive-tmp.sqlite
    // We use a "-tmp" postfix since we are not done yet.
    QM_TRY_INSPECT(const auto& storageDir, QM_NewLocalFile(*mStoragePath));

    QM_TRY(webAppsStoreFile->CopyTo(storageDir,
                                    nsLiteralString(LS_ARCHIVE_TMP_FILE_NAME)));

    QM_TRY_INSPECT(const auto& lsArchiveTmpFile,
                   GetLocalStorageArchiveTmpFile(*mStoragePath));

    if (journalMode.EqualsLiteral("wal")) {
      QM_TRY_INSPECT(
          const auto& lsArchiveTmpConnection,
          MOZ_TO_RESULT_INVOKE_TYPED(nsCOMPtr<mozIStorageConnection>, ss,
                                     OpenUnsharedDatabase, lsArchiveTmpFile));

      // The archive will only be used for lazy data migration. There won't be
      // any concurrent readers and writers that could benefit from Write-Ahead
      // Logging. So switch to a standard rollback journal. The standard
      // rollback journal also provides atomicity across multiple attached
      // databases which is import for the lazy data migration to work safely.
      QM_TRY(lsArchiveTmpConnection->ExecuteSimpleSQL(
          "PRAGMA journal_mode = DELETE;"_ns));

      // Close the connection explicitly. We are going to rename the file below.
      QM_TRY(lsArchiveTmpConnection->Close());
    }

    // Finally, rename ls-archive-tmp.sqlite to ls-archive.sqlite
    QM_TRY(lsArchiveTmpFile->MoveTo(nullptr,
                                    nsLiteralString(LS_ARCHIVE_FILE_NAME)));

    return Ok{};
  }

  // If webappsstore database is not useable, just create an empty archive.
  // XXX The code below should be removed and the caller should call us only
  // when webappstore.sqlite exists. CreateWebAppsStoreConnection should be
  // reworked to propagate database corruption instead of returning null
  // connection.
  // So, if there's no webappsstore.sqlite
  // MaybeCreateOrUpgradeLocalStorageArchive will call
  // CreateEmptyLocalStorageArchive instead of
  // CopyLocalStorageArchiveFromWebAppsStore.
  // If there's any corruption detected during
  // MaybeCreateOrUpgradeLocalStorageArchive (including nested calls like
  // CopyLocalStorageArchiveFromWebAppsStore and CreateWebAppsStoreConnection)
  // EnsureStorageIsInitialized will fallback to
  // CreateEmptyLocalStorageArchive.

  // Ensure the storage directory actually exists.
  QM_TRY_INSPECT(const auto& storageDirectory, QM_NewLocalFile(*mStoragePath));

  QM_TRY_INSPECT(const bool& created, EnsureDirectory(*storageDirectory));

  Unused << created;

  QM_TRY_UNWRAP(
      auto lsArchiveConnection,
      MOZ_TO_RESULT_INVOKE_TYPED(nsCOMPtr<mozIStorageConnection>, ss,
                                 OpenUnsharedDatabase, &aLsArchiveFile));

  QM_TRY(StorageDBUpdater::CreateCurrentSchema(lsArchiveConnection));

  return Ok{};
}

Result<nsCOMPtr<mozIStorageConnection>, nsresult>
QuotaManager::CreateLocalStorageArchiveConnection(
    nsIFile& aLsArchiveFile) const {
  AssertIsOnIOThread();
  MOZ_ASSERT(CachedNextGenLocalStorageEnabled());

#ifdef DEBUG
  {
    QM_TRY_INSPECT(const bool& exists,
                   MOZ_TO_RESULT_INVOKE(aLsArchiveFile, Exists));
    MOZ_ASSERT(exists);
  }
#endif

  QM_TRY_INSPECT(const bool& isDirectory,
                 MOZ_TO_RESULT_INVOKE(aLsArchiveFile, IsDirectory));

  // A directory with the name of the archive file is treated as corruption
  // (similarly as wrong content of the file).
  QM_TRY(OkIf(!isDirectory), Err(NS_ERROR_FILE_CORRUPTED));

  QM_TRY_INSPECT(const auto& ss, ToResultGet<nsCOMPtr<mozIStorageService>>(
                                     MOZ_SELECT_OVERLOAD(do_GetService),
                                     MOZ_STORAGE_SERVICE_CONTRACTID));

  // This may return NS_ERROR_FILE_CORRUPTED too.
  QM_TRY_UNWRAP(auto connection, MOZ_TO_RESULT_INVOKE_TYPED(
                                     nsCOMPtr<mozIStorageConnection>, ss,
                                     OpenUnsharedDatabase, &aLsArchiveFile));

  // The legacy LS implementation removes the database and creates an empty one
  // when the schema can't be updated. The same effect can be achieved here by
  // mapping all errors to NS_ERROR_FILE_CORRUPTED. One such case is tested by
  // sub test case 3 of dom/localstorage/test/unit/test_archive.js
  QM_TRY(
      ToResult(StorageDBUpdater::Update(connection))
          .mapErr([](const nsresult rv) { return NS_ERROR_FILE_CORRUPTED; }));

  return connection;
}

Result<nsCOMPtr<mozIStorageConnection>, nsresult>
QuotaManager::RecopyLocalStorageArchiveFromWebAppsStore(
    nsIFile& aLsArchiveFile) {
  AssertIsOnIOThread();
  MOZ_ASSERT(CachedNextGenLocalStorageEnabled());

  QM_TRY(MaybeRemoveLocalStorageDirectories());

#ifdef DEBUG
  {
    QM_TRY_INSPECT(const bool& exists,
                   MOZ_TO_RESULT_INVOKE(aLsArchiveFile, Exists));

    MOZ_ASSERT(exists);
  }
#endif

  QM_TRY(aLsArchiveFile.Remove(false));

  QM_TRY(CopyLocalStorageArchiveFromWebAppsStore(aLsArchiveFile));

  QM_TRY_UNWRAP(auto connection,
                CreateLocalStorageArchiveConnection(aLsArchiveFile));

  QM_TRY(InitializeLocalStorageArchive(connection));

  return connection;
}

Result<nsCOMPtr<mozIStorageConnection>, nsresult>
QuotaManager::DowngradeLocalStorageArchive(nsIFile& aLsArchiveFile) {
  AssertIsOnIOThread();
  MOZ_ASSERT(CachedNextGenLocalStorageEnabled());

  QM_TRY_UNWRAP(auto connection,
                RecopyLocalStorageArchiveFromWebAppsStore(aLsArchiveFile));

  QM_TRY(
      SaveLocalStorageArchiveVersion(connection, kLocalStorageArchiveVersion));

  return connection;
}

Result<nsCOMPtr<mozIStorageConnection>, nsresult>
QuotaManager::UpgradeLocalStorageArchiveFromLessThan4To4(
    nsIFile& aLsArchiveFile) {
  AssertIsOnIOThread();
  MOZ_ASSERT(CachedNextGenLocalStorageEnabled());

  QM_TRY_UNWRAP(auto connection,
                RecopyLocalStorageArchiveFromWebAppsStore(aLsArchiveFile));

  QM_TRY(SaveLocalStorageArchiveVersion(connection, 4));

  return connection;
}

/*
nsresult QuotaManager::UpgradeLocalStorageArchiveFrom4To5(
    nsCOMPtr<mozIStorageConnection>& aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(CachedNextGenLocalStorageEnabled());

  nsresult rv = SaveLocalStorageArchiveVersion(aConnection, 5);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}
*/

#ifdef DEBUG

void QuotaManager::AssertStorageIsInitialized() const {
  AssertIsOnIOThread();
  MOZ_ASSERT(IsStorageInitialized());
}

#endif  // DEBUG

nsresult QuotaManager::MaybeUpgradeToDefaultStorageDirectory(
    nsIFile& aStorageFile) {
  AssertIsOnIOThread();

  QM_TRY_INSPECT(const auto& storageFileExists,
                 MOZ_TO_RESULT_INVOKE(aStorageFile, Exists));

  if (!storageFileExists) {
    QM_TRY_INSPECT(const auto& indexedDBDir, QM_NewLocalFile(*mIndexedDBPath));

    QM_TRY_INSPECT(const auto& indexedDBDirExists,
                   MOZ_TO_RESULT_INVOKE(indexedDBDir, Exists));

    if (indexedDBDirExists) {
      QM_TRY(UpgradeFromIndexedDBDirectoryToPersistentStorageDirectory(
          indexedDBDir));
    }

    QM_TRY_INSPECT(const auto& persistentStorageDir,
                   QM_NewLocalFile(*mStoragePath));

    QM_TRY(persistentStorageDir->Append(
        nsLiteralString(PERSISTENT_DIRECTORY_NAME)));

    QM_TRY_INSPECT(const auto& persistentStorageDirExists,
                   MOZ_TO_RESULT_INVOKE(persistentStorageDir, Exists));

    if (persistentStorageDirExists) {
      QM_TRY(UpgradeFromPersistentStorageDirectoryToDefaultStorageDirectory(
          persistentStorageDir));
    }
  }

  return NS_OK;
}

nsresult QuotaManager::MaybeCreateOrUpgradeStorage(
    mozIStorageConnection& aConnection) {
  AssertIsOnIOThread();

  QM_TRY_UNWRAP(auto storageVersion,
                MOZ_TO_RESULT_INVOKE(aConnection, GetSchemaVersion));

  // Hacky downgrade logic!
  // If we see major.minor of 3.0, downgrade it to be 2.1.
  if (storageVersion == kHackyPreDowngradeStorageVersion) {
    storageVersion = kHackyPostDowngradeStorageVersion;
    QM_TRY(aConnection.SetSchemaVersion(storageVersion), QM_PROPAGATE,
           [](const auto&) { MOZ_ASSERT(false, "Downgrade didn't take."); });
  }

  QM_TRY(OkIf(GetMajorStorageVersion(storageVersion) <= kMajorStorageVersion),
         NS_ERROR_FAILURE, [](const auto&) {
           NS_WARNING("Unable to initialize storage, version is too high!");
         });

  if (storageVersion < kStorageVersion) {
    const bool newDatabase = !storageVersion;

    QM_TRY_INSPECT(const auto& storageDir, QM_NewLocalFile(*mStoragePath));

    QM_TRY_INSPECT(const auto& storageDirExists,
                   MOZ_TO_RESULT_INVOKE(storageDir, Exists));

    const bool newDirectory = !storageDirExists;

    if (newDatabase) {
      // Set the page size first.
      if (kSQLitePageSizeOverride) {
        QM_TRY(aConnection.ExecuteSimpleSQL(nsPrintfCString(
            "PRAGMA page_size = %" PRIu32 ";", kSQLitePageSizeOverride)));
      }
    }

    mozStorageTransaction transaction(
        &aConnection, false, mozIStorageConnection::TRANSACTION_IMMEDIATE);

    QM_TRY(transaction.Start());

    // An upgrade method can upgrade the database, the storage or both.
    // The upgrade loop below can only be avoided when there's no database and
    // no storage yet (e.g. new profile).
    if (newDatabase && newDirectory) {
      QM_TRY(CreateTables(&aConnection));

#ifdef DEBUG
      {
        QM_TRY_INSPECT(const int32_t& storageVersion,
                       MOZ_TO_RESULT_INVOKE(aConnection, GetSchemaVersion),
                       QM_ASSERT_UNREACHABLE);
        MOZ_ASSERT(storageVersion == kStorageVersion);
      }
#endif

      QM_TRY(aConnection.ExecuteSimpleSQL(
          nsLiteralCString("INSERT INTO database (cache_version) "
                           "VALUES (0)")));
    } else {
      // This logic needs to change next time we change the storage!
      static_assert(kStorageVersion == int32_t((2 << 16) + 3),
                    "Upgrade function needed due to storage version increase.");

      while (storageVersion != kStorageVersion) {
        if (storageVersion == 0) {
          QM_TRY(UpgradeStorageFrom0_0To1_0(&aConnection));
        } else if (storageVersion == MakeStorageVersion(1, 0)) {
          QM_TRY(UpgradeStorageFrom1_0To2_0(&aConnection));
        } else if (storageVersion == MakeStorageVersion(2, 0)) {
          QM_TRY(UpgradeStorageFrom2_0To2_1(&aConnection));
        } else if (storageVersion == MakeStorageVersion(2, 1)) {
          QM_TRY(UpgradeStorageFrom2_1To2_2(&aConnection));
        } else if (storageVersion == MakeStorageVersion(2, 2)) {
          QM_TRY(UpgradeStorageFrom2_2To2_3(&aConnection));
        } else {
          QM_FAIL(NS_ERROR_FAILURE, []() {
            NS_WARNING(
                "Unable to initialize storage, no upgrade path is "
                "available!");
          });
        }

        QM_TRY_UNWRAP(storageVersion,
                      MOZ_TO_RESULT_INVOKE(aConnection, GetSchemaVersion));
      }

      MOZ_ASSERT(storageVersion == kStorageVersion);
    }

    QM_TRY(transaction.Commit());
  }

  return NS_OK;
}

OkOrErr QuotaManager::MaybeRemoveLocalStorageArchiveTmpFile() {
  AssertIsOnIOThread();

  QM_TRY_INSPECT(
      const auto& lsArchiveTmpFile,
      GetLocalStorageArchiveTmpFile(*mStoragePath).mapErr(ToQMResult));

  QM_TRY_INSPECT(
      const bool& exists,
      MOZ_TO_RESULT_INVOKE(lsArchiveTmpFile, Exists).mapErr(ToQMResult));

  if (exists) {
    QM_TRY(QM_TO_RESULT(lsArchiveTmpFile->Remove(false)));
  }

  return Ok{};
}

Result<Ok, nsresult> QuotaManager::MaybeCreateOrUpgradeLocalStorageArchive(
    nsIFile& aLsArchiveFile) {
  AssertIsOnIOThread();

  QM_TRY_INSPECT(
      const bool& lsArchiveFileExisted,
      ([this, &aLsArchiveFile]() -> Result<bool, nsresult> {
        QM_TRY_INSPECT(const bool& exists,
                       MOZ_TO_RESULT_INVOKE(aLsArchiveFile, Exists));

        if (!exists) {
          QM_TRY(CopyLocalStorageArchiveFromWebAppsStore(aLsArchiveFile));
        }

        return exists;
      }()));

  QM_TRY_UNWRAP(auto connection,
                CreateLocalStorageArchiveConnection(aLsArchiveFile));

  QM_TRY_INSPECT(const auto& initialized,
                 IsLocalStorageArchiveInitialized(*connection));

  if (!initialized) {
    QM_TRY(InitializeLocalStorageArchive(connection));
  }

  QM_TRY_UNWRAP(int32_t version, LoadLocalStorageArchiveVersion(*connection));

  if (version > kLocalStorageArchiveVersion) {
    // Close local storage archive connection. We are going to remove underlying
    // file.
    QM_TRY(connection->Close());

    // This will wipe the archive and any migrated data and recopy the archive
    // from webappsstore.sqlite.
    QM_TRY_UNWRAP(connection, DowngradeLocalStorageArchive(aLsArchiveFile));

    QM_TRY_UNWRAP(version, LoadLocalStorageArchiveVersion(*connection));

    MOZ_ASSERT(version == kLocalStorageArchiveVersion);
  } else if (version != kLocalStorageArchiveVersion) {
    // The version can be zero either when the archive didn't exist or it did
    // exist, but the archive was created without any version information.
    // We don't need to do any upgrades only if it didn't exist because existing
    // archives without version information must be recopied to really fix bug
    // 1542104. See also bug 1546305 which introduced archive versions.
    if (!lsArchiveFileExisted) {
      MOZ_ASSERT(version == 0);

      QM_TRY(SaveLocalStorageArchiveVersion(connection,
                                            kLocalStorageArchiveVersion));
    } else {
      static_assert(kLocalStorageArchiveVersion == 4,
                    "Upgrade function needed due to LocalStorage archive "
                    "version increase.");

      while (version != kLocalStorageArchiveVersion) {
        if (version < 4) {
          // Close local storage archive connection. We are going to remove
          // underlying file.
          QM_TRY(connection->Close());

          // This won't do an "upgrade" in a normal sense. It will wipe the
          // archive and any migrated data and recopy the archive from
          // webappsstore.sqlite
          QM_TRY_UNWRAP(connection, UpgradeLocalStorageArchiveFromLessThan4To4(
                                        aLsArchiveFile));
        } /* else if (version == 4) {
          QM_TRY(UpgradeLocalStorageArchiveFrom4To5(connection));
        } */
        else {
          QM_FAIL(Err(NS_ERROR_FAILURE), []() {
            QM_WARNING(
                "Unable to initialize LocalStorage archive, no upgrade path "
                "is available!");
          });
        }

        QM_TRY_UNWRAP(version, LoadLocalStorageArchiveVersion(*connection));
      }

      MOZ_ASSERT(version == kLocalStorageArchiveVersion);
    }
  }

  // At this point, we have finished initializing the local storage archive, and
  // can continue storage initialization. We don't know though if the actual
  // data in the archive file is readable. We can't do a PRAGMA integrity_check
  // here though, because that would be too heavyweight.

  return Ok{};
}

Result<Ok, nsresult> QuotaManager::CreateEmptyLocalStorageArchive(
    nsIFile& aLsArchiveFile) const {
  AssertIsOnIOThread();

  QM_TRY_INSPECT(const bool& exists,
                 MOZ_TO_RESULT_INVOKE(aLsArchiveFile, Exists));

  // If it exists, remove it. It might be a directory, so remove it recursively.
  if (exists) {
    QM_TRY(aLsArchiveFile.Remove(true));

    // XXX If we crash right here, the next session will copy the archive from
    // webappsstore.sqlite again!
    // XXX Create a marker file before removing the archive which can be
    // used in MaybeCreateOrUpgradeLocalStorageArchive to create an empty
    // archive instead of recopying it from webapppstore.sqlite (in other
    // words, finishing what was started here).
  }

  QM_TRY_INSPECT(const auto& ss, ToResultGet<nsCOMPtr<mozIStorageService>>(
                                     MOZ_SELECT_OVERLOAD(do_GetService),
                                     MOZ_STORAGE_SERVICE_CONTRACTID));

  QM_TRY_UNWRAP(
      const auto connection,
      MOZ_TO_RESULT_INVOKE_TYPED(nsCOMPtr<mozIStorageConnection>, ss,
                                 OpenUnsharedDatabase, &aLsArchiveFile));

  QM_TRY(StorageDBUpdater::CreateCurrentSchema(connection));

  QM_TRY(InitializeLocalStorageArchive(connection));

  QM_TRY(
      SaveLocalStorageArchiveVersion(connection, kLocalStorageArchiveVersion));

  return Ok{};
}

nsresult QuotaManager::EnsureStorageIsInitialized() {
  DiagnosticAssertIsOnIOThread();

  const auto innerFunc =
      [&](const auto& firstInitializationAttempt) -> nsresult {
    if (mStorageConnection) {
      MOZ_ASSERT(firstInitializationAttempt.Recorded());
      return NS_OK;
    }

    QM_TRY_INSPECT(const auto& storageFile, QM_NewLocalFile(mBasePath));
    QM_TRY(storageFile->Append(mStorageName + kSQLiteSuffix));

    QM_TRY(MaybeUpgradeToDefaultStorageDirectory(*storageFile));

    QM_TRY_INSPECT(const auto& ss, ToResultGet<nsCOMPtr<mozIStorageService>>(
                                       MOZ_SELECT_OVERLOAD(do_GetService),
                                       MOZ_STORAGE_SERVICE_CONTRACTID));

    QM_TRY_UNWRAP(
        auto connection,
        QM_OR_ELSE_WARN_IF(
            // Expression.
            MOZ_TO_RESULT_INVOKE_TYPED(nsCOMPtr<mozIStorageConnection>, ss,
                                       OpenUnsharedDatabase, storageFile),
            // Predicate.
            IsDatabaseCorruptionError,
            // Fallback.
            ErrToDefaultOk<nsCOMPtr<mozIStorageConnection>>));

    if (!connection) {
      // Nuke the database file.
      QM_TRY(storageFile->Remove(false));

      QM_TRY_UNWRAP(connection, MOZ_TO_RESULT_INVOKE_TYPED(
                                    nsCOMPtr<mozIStorageConnection>, ss,
                                    OpenUnsharedDatabase, storageFile));
    }

    // We want extra durability for this important file.
    QM_TRY(connection->ExecuteSimpleSQL("PRAGMA synchronous = EXTRA;"_ns));

    // Check to make sure that the storage version is correct.
    QM_TRY(MaybeCreateOrUpgradeStorage(*connection));

    QM_TRY(MaybeRemoveLocalStorageArchiveTmpFile());

    QM_TRY_INSPECT(const auto& lsArchiveFile,
                   GetLocalStorageArchiveFile(*mStoragePath));

    if (CachedNextGenLocalStorageEnabled()) {
      QM_TRY(QM_OR_ELSE_WARN_IF(
          // Expression.
          MaybeCreateOrUpgradeLocalStorageArchive(*lsArchiveFile),
          // Predicate.
          IsDatabaseCorruptionError,
          // Fallback.
          ([&](const nsresult rv) -> Result<Ok, nsresult> {
            QM_TRY_RETURN(CreateEmptyLocalStorageArchive(*lsArchiveFile));
          })));
    } else {
      QM_TRY(MaybeRemoveLocalStorageDataAndArchive(*lsArchiveFile));
    }

    QM_TRY_UNWRAP(mCacheUsable, MaybeCreateOrUpgradeCache(*connection));

    if (mCacheUsable && gInvalidateQuotaCache) {
      QM_TRY(InvalidateCache(*connection));

      gInvalidateQuotaCache = false;
    }

    mStorageConnection = std::move(connection);

    return NS_OK;
  };

  return ExecuteInitialization(
      Initialization::Storage,
      "dom::quota::FirstInitializationAttempt::Storage"_ns, innerFunc);
}

RefPtr<ClientDirectoryLock> QuotaManager::CreateDirectoryLock(
    PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata,
    Client::Type aClientType, bool aExclusive) {
  AssertIsOnOwningThread();

  return DirectoryLockImpl::Create(WrapNotNullUnchecked(this), aPersistenceType,
                                   aOriginMetadata, aClientType, aExclusive);
}

RefPtr<UniversalDirectoryLock> QuotaManager::CreateDirectoryLockInternal(
    const Nullable<PersistenceType>& aPersistenceType,
    const OriginScope& aOriginScope, const Nullable<Client::Type>& aClientType,
    bool aExclusive) {
  AssertIsOnOwningThread();

  return DirectoryLockImpl::CreateInternal(WrapNotNullUnchecked(this),
                                           aPersistenceType, aOriginScope,
                                           aClientType, aExclusive);
}

Result<std::pair<nsCOMPtr<nsIFile>, bool>, nsresult>
QuotaManager::EnsurePersistentOriginIsInitialized(
    const OriginMetadata& aOriginMetadata) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aOriginMetadata.mPersistenceType == PERSISTENCE_TYPE_PERSISTENT);
  MOZ_DIAGNOSTIC_ASSERT(mStorageConnection);

  const auto innerFunc = [&aOriginMetadata,
                          this](const auto& firstInitializationAttempt)
      -> mozilla::Result<std::pair<nsCOMPtr<nsIFile>, bool>, nsresult> {
    QM_TRY_UNWRAP(auto directory,
                  GetDirectoryForOrigin(PERSISTENCE_TYPE_PERSISTENT,
                                        aOriginMetadata.mOrigin));

    if (mInitializedOrigins.Contains(aOriginMetadata.mOrigin)) {
      MOZ_ASSERT(firstInitializationAttempt.Recorded());
      return std::pair(std::move(directory), false);
    }

    QM_TRY_INSPECT(const bool& created, EnsureOriginDirectory(*directory));

    QM_TRY_INSPECT(const int64_t& timestamp,
                   ([this, created, &directory,
                     &aOriginMetadata]() -> Result<int64_t, nsresult> {
                     if (created) {
                       const int64_t timestamp = PR_Now();

                       // Only creating .metadata-v2 to reduce IO.
                       QM_TRY(CreateDirectoryMetadata2(*directory, timestamp,
                                                       /* aPersisted */ true,
                                                       aOriginMetadata));

                       return timestamp;
                     }

                     // Get the metadata. We only use the timestamp.
                     QM_TRY_INSPECT(
                         const auto& metadata,
                         LoadFullOriginMetadataWithRestore(directory));

                     MOZ_ASSERT(metadata.mLastAccessTime <= PR_Now());

                     return metadata.mLastAccessTime;
                   }()));

    QM_TRY(InitializeOrigin(PERSISTENCE_TYPE_PERSISTENT, aOriginMetadata,
                            timestamp,
                            /* aPersisted */ true, directory));

    mInitializedOrigins.AppendElement(aOriginMetadata.mOrigin);

    return std::pair(std::move(directory), created);
  };

  return ExecuteOriginInitialization(
      aOriginMetadata.mOrigin, OriginInitialization::PersistentOrigin,
      "dom::quota::FirstOriginInitializationAttempt::PersistentOrigin"_ns,
      innerFunc);
}

Result<std::pair<nsCOMPtr<nsIFile>, bool>, nsresult>
QuotaManager::EnsureTemporaryOriginIsInitialized(
    PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);
  MOZ_DIAGNOSTIC_ASSERT(mStorageConnection);
  MOZ_DIAGNOSTIC_ASSERT(mTemporaryStorageInitialized);

  const auto innerFunc = [&aPersistenceType, &aOriginMetadata,
                          this](const auto&)
      -> mozilla::Result<std::pair<nsCOMPtr<nsIFile>, bool>, nsresult> {
    // Get directory for this origin and persistence type.
    QM_TRY_UNWRAP(
        auto directory,
        GetDirectoryForOrigin(aPersistenceType, aOriginMetadata.mOrigin));

    QM_TRY_INSPECT(const bool& created, EnsureOriginDirectory(*directory));

    if (created) {
      const int64_t timestamp =
          NoteOriginDirectoryCreated(aOriginMetadata, /* aPersisted */ false);

      // Only creating .metadata-v2 to reduce IO.
      QM_TRY(CreateDirectoryMetadata2(*directory, timestamp,
                                      /* aPersisted */ false, aOriginMetadata));
    }

    // TODO: If the metadata file exists and we didn't call
    //       LoadFullOriginMetadataWithRestore for it (because the quota info
    //       was loaded from the cache), then the group in the metadata file
    //       may be wrong, so it should be checked and eventually updated.
    //       It's not a big deal that we are not doing it here, because the
    //       origin will be marked as "accessed", so
    //       LoadFullOriginMetadataWithRestore will be called for the metadata
    //       file in next session in LoadQuotaFromCache.

    return std::pair(std::move(directory), created);
  };

  return ExecuteOriginInitialization(
      aOriginMetadata.mOrigin, OriginInitialization::TemporaryOrigin,
      "dom::quota::FirstOriginInitializationAttempt::TemporaryOrigin"_ns,
      innerFunc);
}

nsresult QuotaManager::EnsureTemporaryStorageIsInitialized() {
  AssertIsOnIOThread();
  MOZ_DIAGNOSTIC_ASSERT(mStorageConnection);

  const auto innerFunc =
      [&](const auto& firstInitializationAttempt) -> nsresult {
    if (mTemporaryStorageInitialized) {
      MOZ_ASSERT(firstInitializationAttempt.Recorded());
      return NS_OK;
    }

    QM_TRY_INSPECT(
        const auto& storageDir,
        ToResultGet<nsCOMPtr<nsIFile>>(MOZ_SELECT_OVERLOAD(do_CreateInstance),
                                       NS_LOCAL_FILE_CONTRACTID));

    QM_TRY(storageDir->InitWithPath(GetStoragePath()));

    // The storage directory must exist before calling GetDiskSpaceAvailable.
    QM_TRY_INSPECT(const bool& created, EnsureDirectory(*storageDir));

    Unused << created;

    // Check for available disk space users have on their device where storage
    // directory lives.
    QM_TRY_INSPECT(const int64_t& diskSpaceAvailable,
                   MOZ_TO_RESULT_INVOKE(storageDir, GetDiskSpaceAvailable));

    MOZ_ASSERT(diskSpaceAvailable >= 0);

    QM_TRY(LoadQuota());

    mTemporaryStorageInitialized = true;

    // Available disk space shouldn't be used directly for temporary storage
    // limit calculation since available disk space is affected by existing data
    // stored in temporary storage. So we need to increase it by the temporary
    // storage size (that has been calculated in LoadQuota) before passing to
    // GetTemporaryStorageLimit.
    mTemporaryStorageLimit = GetTemporaryStorageLimit(
        /* aAvailableSpaceBytes */ diskSpaceAvailable + mTemporaryStorageUsage);

    CleanupTemporaryStorage();

    if (mCacheUsable) {
      QM_TRY(InvalidateCache(*mStorageConnection));
    }

    return NS_OK;
  };

  return ExecuteInitialization(
      Initialization::TemporaryStorage,
      "dom::quota::FirstInitializationAttempt::TemporaryStorage"_ns, innerFunc);
}

void QuotaManager::ShutdownStorage() {
  AssertIsOnIOThread();

  if (mStorageConnection) {
    mInitializationInfo.ResetOriginInitializationInfos();
    mInitializedOrigins.Clear();

    if (mTemporaryStorageInitialized) {
      if (mCacheUsable) {
        UnloadQuota();
      } else {
        RemoveQuota();
      }

      mTemporaryStorageInitialized = false;
    }

    ReleaseIOThreadObjects();

    mStorageConnection = nullptr;
    mCacheUsable = false;
  }

  mInitializationInfo.ResetFirstInitializationAttempts();
}

Result<bool, nsresult> QuotaManager::EnsureOriginDirectory(
    nsIFile& aDirectory) {
  AssertIsOnIOThread();

  QM_TRY_INSPECT(const bool& exists, MOZ_TO_RESULT_INVOKE(aDirectory, Exists));

  if (!exists) {
    QM_TRY_INSPECT(const auto& leafName,
                   MOZ_TO_RESULT_INVOKE_TYPED(nsString, aDirectory, GetLeafName)
                       .map([](const auto& leafName) {
                         return NS_ConvertUTF16toUTF8(leafName);
                       }));

    QM_TRY(OkIf(IsSanitizedOriginValid(leafName)), Err(NS_ERROR_FAILURE),
           [](const auto&) {
             QM_WARNING(
                 "Preventing creation of a new origin directory which is not "
                 "supported by our origin parser or is obsolete!");
           });
  }

  QM_TRY_RETURN(EnsureDirectory(aDirectory));
}

nsresult QuotaManager::AboutToClearOrigins(
    const Nullable<PersistenceType>& aPersistenceType,
    const OriginScope& aOriginScope,
    const Nullable<Client::Type>& aClientType) {
  AssertIsOnIOThread();

  if (aClientType.IsNull()) {
    for (Client::Type type : AllClientTypes()) {
      QM_TRY((*mClients)[type]->AboutToClearOrigins(aPersistenceType,
                                                    aOriginScope));
    }
  } else {
    QM_TRY((*mClients)[aClientType.Value()]->AboutToClearOrigins(
        aPersistenceType, aOriginScope));
  }

  return NS_OK;
}

void QuotaManager::OriginClearCompleted(
    PersistenceType aPersistenceType, const nsACString& aOrigin,
    const Nullable<Client::Type>& aClientType) {
  AssertIsOnIOThread();

  if (aClientType.IsNull()) {
    if (aPersistenceType == PERSISTENCE_TYPE_PERSISTENT) {
      mInitializedOrigins.RemoveElement(aOrigin);
    }

    for (Client::Type type : AllClientTypes()) {
      (*mClients)[type]->OnOriginClearCompleted(aPersistenceType, aOrigin);
    }
  } else {
    (*mClients)[aClientType.Value()]->OnOriginClearCompleted(aPersistenceType,
                                                             aOrigin);
  }
}

Client* QuotaManager::GetClient(Client::Type aClientType) {
  MOZ_ASSERT(aClientType >= Client::IDB);
  MOZ_ASSERT(aClientType < Client::TypeMax());

  return (*mClients)[aClientType];
}

const AutoTArray<Client::Type, Client::TYPE_MAX>&
QuotaManager::AllClientTypes() {
  if (CachedNextGenLocalStorageEnabled()) {
    return *mAllClientTypes;
  }
  return *mAllClientTypesExceptLS;
}

uint64_t QuotaManager::GetGroupLimit() const {
  // To avoid one group evicting all the rest, limit the amount any one group
  // can use to 20% resp. a fifth. To prevent individual sites from using
  // exorbitant amounts of storage where there is a lot of free space, cap the
  // group limit to 2GB.
  const uint64_t x = std::min<uint64_t>(mTemporaryStorageLimit / 5, 2 GB);

  // In low-storage situations, make an exception (while not exceeding the total
  // storage limit).
  return std::min<uint64_t>(mTemporaryStorageLimit,
                            std::max<uint64_t>(x, 10 MB));
}

uint64_t QuotaManager::GetGroupUsage(const nsACString& aGroup) {
  AssertIsOnIOThread();

  uint64_t usage = 0;

  {
    MutexAutoLock lock(mQuotaMutex);

    GroupInfoPair* pair;
    if (mGroupInfoPairs.Get(aGroup, &pair)) {
      for (const PersistenceType type : kBestEffortPersistenceTypes) {
        RefPtr<GroupInfo> groupInfo = pair->LockedGetGroupInfo(type);
        if (groupInfo) {
          AssertNoOverflow(usage, groupInfo->mUsage);
          usage += groupInfo->mUsage;
        }
      }
    }
  }

  return usage;
}

uint64_t QuotaManager::GetOriginUsage(
    const PrincipalMetadata& aPrincipalMetadata) {
  AssertIsOnIOThread();

  uint64_t usage = 0;

  {
    MutexAutoLock lock(mQuotaMutex);

    GroupInfoPair* pair;
    if (mGroupInfoPairs.Get(aPrincipalMetadata.mGroup, &pair)) {
      for (const PersistenceType type : kBestEffortPersistenceTypes) {
        RefPtr<GroupInfo> groupInfo = pair->LockedGetGroupInfo(type);
        if (groupInfo) {
          RefPtr<OriginInfo> originInfo =
              groupInfo->LockedGetOriginInfo(aPrincipalMetadata.mOrigin);
          if (originInfo) {
            AssertNoOverflow(usage, originInfo->LockedUsage());
            usage += originInfo->LockedUsage();
          }
        }
      }
    }
  }

  return usage;
}

void QuotaManager::NotifyStoragePressure(uint64_t aUsage) {
  mQuotaMutex.AssertNotCurrentThreadOwns();

  RefPtr<StoragePressureRunnable> storagePressureRunnable =
      new StoragePressureRunnable(aUsage);

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(storagePressureRunnable));
}

// static
void QuotaManager::GetStorageId(PersistenceType aPersistenceType,
                                const nsACString& aOrigin,
                                Client::Type aClientType,
                                nsACString& aDatabaseId) {
  nsAutoCString str;
  str.AppendInt(aPersistenceType);
  str.Append('*');
  str.Append(aOrigin);
  str.Append('*');
  str.AppendInt(aClientType);

  aDatabaseId = str;
}

// static
bool QuotaManager::IsPrincipalInfoValid(const PrincipalInfo& aPrincipalInfo) {
  switch (aPrincipalInfo.type()) {
    // A system principal is acceptable.
    case PrincipalInfo::TSystemPrincipalInfo: {
      return true;
    }

    // Validate content principals to ensure that the spec, originNoSuffix and
    // baseDomain are sane.
    case PrincipalInfo::TContentPrincipalInfo: {
      const ContentPrincipalInfo& info =
          aPrincipalInfo.get_ContentPrincipalInfo();

      // Verify the principal spec parses.
      RefPtr<MozURL> specURL;
      nsresult rv = MozURL::Init(getter_AddRefs(specURL), info.spec());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        QM_WARNING("A URL %s is not recognized by MozURL", info.spec().get());
        return false;
      }

      // Verify the principal originNoSuffix matches spec.
      nsCString originNoSuffix;
      specURL->Origin(originNoSuffix);

      if (NS_WARN_IF(originNoSuffix != info.originNoSuffix())) {
        QM_WARNING("originNoSuffix (%s) doesn't match passed one (%s)!",
                   originNoSuffix.get(), info.originNoSuffix().get());
        return false;
      }

      if (NS_WARN_IF(info.originNoSuffix().EqualsLiteral(kChromeOrigin))) {
        return false;
      }

      if (NS_WARN_IF(info.originNoSuffix().FindChar('^', 0) != -1)) {
        QM_WARNING("originNoSuffix (%s) contains the '^' character!",
                   info.originNoSuffix().get());
        return false;
      }

      // Verify the principal baseDomain exists.
      if (NS_WARN_IF(info.baseDomain().IsVoid())) {
        return false;
      }

      // Verify the principal baseDomain matches spec.
      nsCString baseDomain;
      rv = specURL->BaseDomain(baseDomain);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return false;
      }

      if (NS_WARN_IF(baseDomain != info.baseDomain())) {
        QM_WARNING("baseDomain (%s) doesn't match passed one (%s)!",
                   baseDomain.get(), info.baseDomain().get());
        return false;
      }

      return true;
    }

    default: {
      break;
    }
  }

  // Null and expanded principals are not acceptable.
  return false;
}

// static
PrincipalMetadata QuotaManager::GetInfoFromValidatedPrincipalInfo(
    const PrincipalInfo& aPrincipalInfo) {
  MOZ_ASSERT(IsPrincipalInfoValid(aPrincipalInfo));

  switch (aPrincipalInfo.type()) {
    case PrincipalInfo::TSystemPrincipalInfo: {
      return GetInfoForChrome();
    }

    case PrincipalInfo::TContentPrincipalInfo: {
      const ContentPrincipalInfo& info =
          aPrincipalInfo.get_ContentPrincipalInfo();

      PrincipalMetadata principalMetadata;

      info.attrs().CreateSuffix(principalMetadata.mSuffix);

      principalMetadata.mGroup = info.baseDomain() + principalMetadata.mSuffix;

      principalMetadata.mOrigin =
          info.originNoSuffix() + principalMetadata.mSuffix;

      return principalMetadata;
    }

    default: {
      MOZ_CRASH("Should never get here!");
    }
  }
}

// static
nsAutoCString QuotaManager::GetOriginFromValidatedPrincipalInfo(
    const PrincipalInfo& aPrincipalInfo) {
  MOZ_ASSERT(IsPrincipalInfoValid(aPrincipalInfo));

  switch (aPrincipalInfo.type()) {
    case PrincipalInfo::TSystemPrincipalInfo: {
      return nsAutoCString{GetOriginForChrome()};
    }

    case PrincipalInfo::TContentPrincipalInfo: {
      const ContentPrincipalInfo& info =
          aPrincipalInfo.get_ContentPrincipalInfo();

      nsAutoCString suffix;

      info.attrs().CreateSuffix(suffix);

      return info.originNoSuffix() + suffix;
    }

    default: {
      MOZ_CRASH("Should never get here!");
    }
  }
}

// static
Result<PrincipalMetadata, nsresult> QuotaManager::GetInfoFromPrincipal(
    nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  if (aPrincipal->IsSystemPrincipal()) {
    return GetInfoForChrome();
  }

  if (aPrincipal->GetIsNullPrincipal()) {
    NS_WARNING("IndexedDB not supported from this principal!");
    return Err(NS_ERROR_FAILURE);
  }

  PrincipalMetadata principalMetadata;

  QM_TRY(aPrincipal->GetOrigin(principalMetadata.mOrigin));

  if (principalMetadata.mOrigin.EqualsLiteral(kChromeOrigin)) {
    NS_WARNING("Non-chrome principal can't use chrome origin!");
    return Err(NS_ERROR_FAILURE);
  }

  aPrincipal->OriginAttributesRef().CreateSuffix(principalMetadata.mSuffix);

  nsAutoCString baseDomain;
  QM_TRY(aPrincipal->GetBaseDomain(baseDomain));

  MOZ_ASSERT(!baseDomain.IsEmpty());

  principalMetadata.mGroup = baseDomain + principalMetadata.mSuffix;

  return principalMetadata;
}

// static
Result<nsAutoCString, nsresult> QuotaManager::GetOriginFromPrincipal(
    nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  if (aPrincipal->IsSystemPrincipal()) {
    return nsAutoCString{GetOriginForChrome()};
  }

  if (aPrincipal->GetIsNullPrincipal()) {
    NS_WARNING("IndexedDB not supported from this principal!");
    return Err(NS_ERROR_FAILURE);
  }

  QM_TRY_UNWRAP(const auto origin, MOZ_TO_RESULT_INVOKE_TYPED(
                                       nsAutoCString, aPrincipal, GetOrigin));

  if (origin.EqualsLiteral(kChromeOrigin)) {
    NS_WARNING("Non-chrome principal can't use chrome origin!");
    return Err(NS_ERROR_FAILURE);
  }

  return origin;
}

// static
Result<nsAutoCString, nsresult> QuotaManager::GetOriginFromWindow(
    nsPIDOMWindowOuter* aWindow) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aWindow);
  QM_TRY(OkIf(sop), Err(NS_ERROR_FAILURE));

  nsCOMPtr<nsIPrincipal> principal = sop->GetPrincipal();
  QM_TRY(OkIf(principal), Err(NS_ERROR_FAILURE));

  QM_TRY_RETURN(GetOriginFromPrincipal(principal));
}

// static
PrincipalMetadata QuotaManager::GetInfoForChrome() {
  return {{}, GetOriginForChrome(), GetOriginForChrome()};
}

// static
nsLiteralCString QuotaManager::GetOriginForChrome() {
  return nsLiteralCString{kChromeOrigin};
}

// static
bool QuotaManager::IsOriginInternal(const nsACString& aOrigin) {
  MOZ_ASSERT(!aOrigin.IsEmpty());

  // The first prompt is not required for these origins.
  if (aOrigin.EqualsLiteral(kChromeOrigin) ||
      StringBeginsWith(aOrigin, nsDependentCString(kAboutHomeOriginPrefix)) ||
      StringBeginsWith(aOrigin, nsDependentCString(kIndexedDBOriginPrefix)) ||
      StringBeginsWith(aOrigin, nsDependentCString(kResourceOriginPrefix))) {
    return true;
  }

  return false;
}

// static
bool QuotaManager::AreOriginsEqualOnDisk(const nsACString& aOrigin1,
                                         const nsACString& aOrigin2) {
  return MakeSanitizedOriginCString(aOrigin1) ==
         MakeSanitizedOriginCString(aOrigin2);
}

// static
Result<PrincipalInfo, nsresult> QuotaManager::ParseOrigin(
    const nsACString& aOrigin) {
  // An origin string either corresponds to a SystemPrincipalInfo or a
  // ContentPrincipalInfo, see
  // QuotaManager::GetOriginFromValidatedPrincipalInfo.

  if (aOrigin.Equals(kChromeOrigin)) {
    return PrincipalInfo{SystemPrincipalInfo{}};
  }

  ContentPrincipalInfo contentPrincipalInfo;

  nsCString originalSuffix;
  const OriginParser::ResultType result = OriginParser::ParseOrigin(
      MakeSanitizedOriginCString(aOrigin), contentPrincipalInfo.spec(),
      &contentPrincipalInfo.attrs(), originalSuffix);
  QM_TRY(OkIf(result == OriginParser::ValidOrigin), Err(NS_ERROR_FAILURE));

  return PrincipalInfo{std::move(contentPrincipalInfo)};
}

// static
void QuotaManager::InvalidateQuotaCache() { gInvalidateQuotaCache = true; }

uint64_t QuotaManager::LockedCollectOriginsForEviction(
    uint64_t aMinSizeToBeFreed, nsTArray<RefPtr<OriginDirectoryLock>>& aLocks) {
  mQuotaMutex.AssertCurrentThreadOwns();

  RefPtr<CollectOriginsHelper> helper =
      new CollectOriginsHelper(mQuotaMutex, aMinSizeToBeFreed);

  // Unlock while calling out to XPCOM (code behind the dispatch method needs
  // to acquire its own lock which can potentially lead to a deadlock and it
  // also calls an observer that can do various stuff like IO, so it's better
  // to not hold our mutex while that happens).
  {
    MutexAutoUnlock autoUnlock(mQuotaMutex);

    MOZ_ALWAYS_SUCCEEDS(mOwningThread->Dispatch(helper, NS_DISPATCH_NORMAL));
  }

  return helper->BlockAndReturnOriginsForEviction(aLocks);
}

void QuotaManager::LockedRemoveQuotaForOrigin(
    PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata) {
  mQuotaMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

  GroupInfoPair* pair;
  if (!mGroupInfoPairs.Get(aOriginMetadata.mGroup, &pair)) {
    return;
  }

  MOZ_ASSERT(pair);

  if (RefPtr<GroupInfo> groupInfo =
          pair->LockedGetGroupInfo(aPersistenceType)) {
    groupInfo->LockedRemoveOriginInfo(aOriginMetadata.mOrigin);

    if (!groupInfo->LockedHasOriginInfos()) {
      pair->LockedClearGroupInfo(aPersistenceType);

      if (!pair->LockedHasGroupInfos()) {
        mGroupInfoPairs.Remove(aOriginMetadata.mGroup);
      }
    }
  }
}

already_AddRefed<GroupInfo> QuotaManager::LockedGetOrCreateGroupInfo(
    PersistenceType aPersistenceType, const nsACString& aSuffix,
    const nsACString& aGroup) {
  mQuotaMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

  GroupInfoPair* const pair =
      mGroupInfoPairs.GetOrInsertNew(aGroup, aSuffix, aGroup);

  RefPtr<GroupInfo> groupInfo = pair->LockedGetGroupInfo(aPersistenceType);
  if (!groupInfo) {
    groupInfo = new GroupInfo(pair, aPersistenceType);
    pair->LockedSetGroupInfo(aPersistenceType, groupInfo);
  }

  return groupInfo.forget();
}

already_AddRefed<OriginInfo> QuotaManager::LockedGetOriginInfo(
    PersistenceType aPersistenceType, const OriginMetadata& aOriginMetadata) {
  mQuotaMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

  GroupInfoPair* pair;
  if (mGroupInfoPairs.Get(aOriginMetadata.mGroup, &pair)) {
    RefPtr<GroupInfo> groupInfo = pair->LockedGetGroupInfo(aPersistenceType);
    if (groupInfo) {
      return groupInfo->LockedGetOriginInfo(aOriginMetadata.mOrigin);
    }
  }

  return nullptr;
}

template <typename Iterator>
void QuotaManager::MaybeInsertNonPersistedOriginInfos(
    Iterator aDest, const RefPtr<GroupInfo>& aTemporaryGroupInfo,
    const RefPtr<GroupInfo>& aDefaultGroupInfo) {
  const auto copy = [&aDest](const GroupInfo& groupInfo) {
    std::copy_if(
        groupInfo.mOriginInfos.cbegin(), groupInfo.mOriginInfos.cend(), aDest,
        [](const auto& originInfo) { return !originInfo->LockedPersisted(); });
  };

  if (aTemporaryGroupInfo) {
    MOZ_ASSERT(PERSISTENCE_TYPE_TEMPORARY ==
               aTemporaryGroupInfo->GetPersistenceType());

    copy(*aTemporaryGroupInfo);
  }
  if (aDefaultGroupInfo) {
    MOZ_ASSERT(PERSISTENCE_TYPE_DEFAULT ==
               aDefaultGroupInfo->GetPersistenceType());

    copy(*aDefaultGroupInfo);
  }
}

template <typename Collect, typename Pred>
QuotaManager::OriginInfosFlatTraversable
QuotaManager::CollectLRUOriginInfosUntil(Collect&& aCollect, Pred&& aPred) {
  OriginInfosFlatTraversable originInfos;

  std::forward<Collect>(aCollect)(MakeBackInserter(originInfos));

  originInfos.Sort(OriginInfoAccessTimeComparator());

  const auto foundIt = std::find_if(originInfos.cbegin(), originInfos.cend(),
                                    std::forward<Pred>(aPred));

  originInfos.TruncateLength(foundIt - originInfos.cbegin());

  return originInfos;
}

QuotaManager::OriginInfosNestedTraversable
QuotaManager::GetOriginInfosExceedingGroupLimit() const {
  MutexAutoLock lock(mQuotaMutex);

  OriginInfosNestedTraversable originInfos;

  for (const auto& entry : mGroupInfoPairs) {
    const auto& pair = entry.GetData();

    MOZ_ASSERT(!entry.GetKey().IsEmpty());
    MOZ_ASSERT(pair);

    uint64_t groupUsage = 0;

    const RefPtr<GroupInfo> temporaryGroupInfo =
        pair->LockedGetGroupInfo(PERSISTENCE_TYPE_TEMPORARY);
    if (temporaryGroupInfo) {
      groupUsage += temporaryGroupInfo->mUsage;
    }

    const RefPtr<GroupInfo> defaultGroupInfo =
        pair->LockedGetGroupInfo(PERSISTENCE_TYPE_DEFAULT);
    if (defaultGroupInfo) {
      groupUsage += defaultGroupInfo->mUsage;
    }

    if (groupUsage > 0) {
      QuotaManager* quotaManager = QuotaManager::Get();
      MOZ_ASSERT(quotaManager, "Shouldn't be null!");

      if (groupUsage > quotaManager->GetGroupLimit()) {
        originInfos.AppendElement(CollectLRUOriginInfosUntil(
            [&temporaryGroupInfo, &defaultGroupInfo](auto inserter) {
              MaybeInsertNonPersistedOriginInfos(
                  std::move(inserter), temporaryGroupInfo, defaultGroupInfo);
            },
            [&groupUsage, quotaManager](const auto& originInfo) {
              groupUsage -= originInfo->LockedUsage();

              return groupUsage <= quotaManager->GetGroupLimit();
            }));
      }
    }
  }

  return originInfos;
}

QuotaManager::OriginInfosNestedTraversable
QuotaManager::GetOriginInfosExceedingGlobalLimit() const {
  MutexAutoLock lock(mQuotaMutex);

  QuotaManager::OriginInfosNestedTraversable res;
  res.AppendElement(CollectLRUOriginInfosUntil(
      // XXX The lambda only needs to capture this, but due to Bug 1421435 it
      // can't.
      [&](auto inserter) {
        for (const auto& entry : mGroupInfoPairs) {
          const auto& pair = entry.GetData();

          MOZ_ASSERT(!entry.GetKey().IsEmpty());
          MOZ_ASSERT(pair);

          MaybeInsertNonPersistedOriginInfos(
              inserter, pair->LockedGetGroupInfo(PERSISTENCE_TYPE_TEMPORARY),
              pair->LockedGetGroupInfo(PERSISTENCE_TYPE_DEFAULT));
        }
      },
      [temporaryStorageUsage = mTemporaryStorageUsage,
       temporaryStorageLimit = mTemporaryStorageLimit,
       doomedUsage = uint64_t{0}](const auto& originInfo) mutable {
        if (temporaryStorageUsage - doomedUsage <= temporaryStorageLimit) {
          return true;
        }

        doomedUsage += originInfo->LockedUsage();
        return false;
      }));

  return res;
}

void QuotaManager::ClearOrigins(
    const OriginInfosNestedTraversable& aDoomedOriginInfos) {
  AssertIsOnIOThread();

  // XXX Does this need to be done a) in order and/or b) sequentially?
  for (const auto& doomedOriginInfo :
       Flatten<OriginInfosFlatTraversable::elem_type>(aDoomedOriginInfos)) {
#ifdef DEBUG
    {
      MutexAutoLock lock(mQuotaMutex);
      MOZ_ASSERT(!doomedOriginInfo->LockedPersisted());
    }
#endif

    DeleteFilesForOrigin(doomedOriginInfo->mGroupInfo->mPersistenceType,
                         doomedOriginInfo->mOrigin);
  }

  struct OriginParams {
    nsCString mOrigin;
    PersistenceType mPersistenceType;
  };

  nsTArray<OriginParams> clearedOrigins;

  {
    MutexAutoLock lock(mQuotaMutex);

    for (const auto& doomedOriginInfo :
         Flatten<OriginInfosFlatTraversable::elem_type>(aDoomedOriginInfos)) {
      // LockedRemoveQuotaForOrigin might remove the group info;
      // OriginInfo::mGroupInfo is only a raw pointer, so we need to store the
      // information for calling OriginClearCompleted below in a separate array.
      clearedOrigins.AppendElement(
          OriginParams{doomedOriginInfo->mOrigin,
                       doomedOriginInfo->mGroupInfo->mPersistenceType});

      LockedRemoveQuotaForOrigin(doomedOriginInfo->mGroupInfo->mPersistenceType,
                                 doomedOriginInfo->FlattenToOriginMetadata());
    }
  }

  for (const auto& clearedOrigin : clearedOrigins) {
    OriginClearCompleted(clearedOrigin.mPersistenceType, clearedOrigin.mOrigin,
                         Nullable<Client::Type>());
  }
}

void QuotaManager::CleanupTemporaryStorage() {
  AssertIsOnIOThread();

  // Evicting origins that exceed their group limit also affects the global
  // temporary storage usage, so these steps have to be taken sequentially.
  // Combining them doesn't seem worth the added complexity.
  ClearOrigins(GetOriginInfosExceedingGroupLimit());
  ClearOrigins(GetOriginInfosExceedingGlobalLimit());

  if (mTemporaryStorageUsage > mTemporaryStorageLimit) {
    // If disk space is still low after origin clear, notify storage pressure.
    NotifyStoragePressure(mTemporaryStorageUsage);
  }
}

void QuotaManager::DeleteFilesForOrigin(PersistenceType aPersistenceType,
                                        const nsACString& aOrigin) {
  QM_TRY_INSPECT(const auto& directory,
                 GetDirectoryForOrigin(aPersistenceType, aOrigin), QM_VOID);

  nsresult rv = directory->Remove(true);
  if (rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST &&
      rv != NS_ERROR_FILE_NOT_FOUND && NS_FAILED(rv)) {
    // This should never fail if we've closed all storage connections
    // correctly...
    NS_ERROR("Failed to remove directory!");
  }
}

void QuotaManager::FinalizeOriginEviction(
    nsTArray<RefPtr<OriginDirectoryLock>>&& aLocks) {
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  RefPtr<FinalizeOriginEvictionOp> op =
      new FinalizeOriginEvictionOp(mOwningThread, std::move(aLocks));

  if (IsOnIOThread()) {
    op->RunOnIOThreadImmediately();
  } else {
    op->Dispatch();
  }
}

auto QuotaManager::GetDirectoryLockTable(PersistenceType aPersistenceType)
    -> DirectoryLockTable& {
  switch (aPersistenceType) {
    case PERSISTENCE_TYPE_TEMPORARY:
      return mTemporaryDirectoryLockTable;
    case PERSISTENCE_TYPE_DEFAULT:
      return mDefaultDirectoryLockTable;

    case PERSISTENCE_TYPE_PERSISTENT:
    case PERSISTENCE_TYPE_INVALID:
    default:
      MOZ_CRASH("Bad persistence type value!");
  }
}

bool QuotaManager::IsSanitizedOriginValid(const nsACString& aSanitizedOrigin) {
  AssertIsOnIOThread();

  // Do not parse this sanitized origin string, if we already parsed it.
  return mValidOrigins.LookupOrInsertWith(
      aSanitizedOrigin, [&aSanitizedOrigin] {
        nsCString spec;
        OriginAttributes attrs;
        nsCString originalSuffix;
        const auto result = OriginParser::ParseOrigin(aSanitizedOrigin, spec,
                                                      &attrs, originalSuffix);

        return result == OriginParser::ValidOrigin;
      });
}

int64_t QuotaManager::GenerateDirectoryLockId() {
  const int64_t directorylockId = mNextDirectoryLockId;

  if (CheckedInt64 result = CheckedInt64(mNextDirectoryLockId) + 1;
      result.isValid()) {
    mNextDirectoryLockId = result.value();
  } else {
    NS_WARNING("Quota manager has run out of ids for directory locks!");

    // There's very little chance for this to happen given the max size of
    // 64 bit integer but if it happens we can just reset mNextDirectoryLockId
    // to zero since such old directory locks shouldn't exist anymore.
    mNextDirectoryLockId = 0;
  }

  // TODO: Maybe add an assertion here to check that there is no existing
  //       directory lock with given id.

  return directorylockId;
}

template <typename Func>
auto QuotaManager::ExecuteInitialization(const Initialization aInitialization,
                                         Func&& aFunc)
    -> std::invoke_result_t<Func, const FirstInitializationAttempt<
                                      Initialization, StringGenerator>&> {
  return quota::ExecuteInitialization(mInitializationInfo, aInitialization,
                                      std::forward<Func>(aFunc));
}

template <typename Func>
auto QuotaManager::ExecuteInitialization(const Initialization aInitialization,
                                         const nsACString& aContext,
                                         Func&& aFunc)
    -> std::invoke_result_t<Func, const FirstInitializationAttempt<
                                      Initialization, StringGenerator>&> {
  return quota::ExecuteInitialization(mInitializationInfo, aInitialization,
                                      aContext, std::forward<Func>(aFunc));
}

template <typename Func>
auto QuotaManager::ExecuteOriginInitialization(
    const nsACString& aOrigin, const OriginInitialization aInitialization,
    const nsACString& aContext, Func&& aFunc)
    -> std::invoke_result_t<Func, const FirstInitializationAttempt<
                                      Initialization, StringGenerator>&> {
  return quota::ExecuteInitialization(
      mInitializationInfo.MutableOriginInitializationInfoRef(
          aOrigin, CreateIfNonExistent{}),
      aInitialization, aContext, std::forward<Func>(aFunc));
}

/*******************************************************************************
 * Local class implementations
 ******************************************************************************/

void ClientUsageArray::Serialize(nsACString& aText) const {
  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  bool first = true;

  for (Client::Type type : quotaManager->AllClientTypes()) {
    const Maybe<uint64_t>& clientUsage = ElementAt(type);
    if (clientUsage.isSome()) {
      if (first) {
        first = false;
      } else {
        aText.Append(" ");
      }

      aText.Append(Client::TypeToPrefix(type));
      aText.AppendInt(clientUsage.value());
    }
  }
}

nsresult ClientUsageArray::Deserialize(const nsACString& aText) {
  for (const auto& token :
       nsCCharSeparatedTokenizerTemplate<NS_TokenizerIgnoreNothing>(aText, ' ')
           .ToRange()) {
    QM_TRY(OkIf(token.Length() >= 2), NS_ERROR_FAILURE);

    Client::Type clientType;
    QM_TRY(OkIf(Client::TypeFromPrefix(token.First(), clientType, fallible)),
           NS_ERROR_FAILURE);

    nsresult rv;
    const uint64_t usage = Substring(token, 1).ToInteger(&rv);
    QM_TRY(ToResult(rv));

    ElementAt(clientType) = Some(usage);
  }

  return NS_OK;
}

OriginInfo::OriginInfo(GroupInfo* aGroupInfo, const nsACString& aOrigin,
                       const ClientUsageArray& aClientUsages, uint64_t aUsage,
                       int64_t aAccessTime, bool aPersisted,
                       bool aDirectoryExists)
    : mClientUsages(aClientUsages.Clone()),
      mGroupInfo(aGroupInfo),
      mOrigin(aOrigin),
      mUsage(aUsage),
      mAccessTime(aAccessTime),
      mAccessed(false),
      mPersisted(aPersisted),
      mDirectoryExists(aDirectoryExists) {
  MOZ_ASSERT(aGroupInfo);
  MOZ_ASSERT(aClientUsages.Length() == Client::TypeMax());
  MOZ_ASSERT_IF(aPersisted,
                aGroupInfo->mPersistenceType == PERSISTENCE_TYPE_DEFAULT);

  // This constructor is called from the "QuotaManager IO" thread and so
  // we can't check if the principal has a WebExtensionPolicy instance
  // associated to it, and even besides that if the extension is currently
  // disabled (and so no WebExtensionPolicy instance would actually exist)
  // its stored data shouldn't be cleared until the extension is uninstalled
  // and so here we resort to check the origin scheme instead.
  mIsExtension = StringBeginsWith(mOrigin, "moz-extension://"_ns);

#ifdef DEBUG
  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  uint64_t usage = 0;
  for (Client::Type type : quotaManager->AllClientTypes()) {
    AssertNoOverflow(usage, aClientUsages[type].valueOr(0));
    usage += aClientUsages[type].valueOr(0);
  }
  MOZ_ASSERT(aUsage == usage);
#endif

  MOZ_COUNT_CTOR(OriginInfo);
}

OriginMetadata OriginInfo::FlattenToOriginMetadata() const {
  return {mGroupInfo->mGroupInfoPair->Suffix(),
          mGroupInfo->mGroupInfoPair->Group(), mOrigin,
          mGroupInfo->mPersistenceType};
}

nsresult OriginInfo::LockedBindToStatement(
    mozIStorageStatement* aStatement) const {
  AssertCurrentThreadOwnsQuotaMutex();
  MOZ_ASSERT(mGroupInfo);

  QM_TRY(aStatement->BindInt32ByName("repository_id"_ns,
                                     mGroupInfo->mPersistenceType));

  QM_TRY(aStatement->BindUTF8StringByName(
      "suffix"_ns, mGroupInfo->mGroupInfoPair->Suffix()));
  QM_TRY(aStatement->BindUTF8StringByName("group_"_ns,
                                          mGroupInfo->mGroupInfoPair->Group()));
  QM_TRY(aStatement->BindUTF8StringByName("origin"_ns, mOrigin));

  nsCString clientUsagesText;
  mClientUsages.Serialize(clientUsagesText);

  QM_TRY(
      aStatement->BindUTF8StringByName("client_usages"_ns, clientUsagesText));
  QM_TRY(aStatement->BindInt64ByName("usage"_ns, mUsage));
  QM_TRY(aStatement->BindInt64ByName("last_access_time"_ns, mAccessTime));
  QM_TRY(aStatement->BindInt32ByName("accessed"_ns, mAccessed));
  QM_TRY(aStatement->BindInt32ByName("persisted"_ns, mPersisted));

  return NS_OK;
}

void OriginInfo::LockedDecreaseUsage(Client::Type aClientType, int64_t aSize) {
  AssertCurrentThreadOwnsQuotaMutex();

  MOZ_ASSERT(mClientUsages[aClientType].isSome());
  AssertNoUnderflow(mClientUsages[aClientType].value(), aSize);
  mClientUsages[aClientType] = Some(mClientUsages[aClientType].value() - aSize);

  AssertNoUnderflow(mUsage, aSize);
  mUsage -= aSize;

  if (!LockedPersisted()) {
    AssertNoUnderflow(mGroupInfo->mUsage, aSize);
    mGroupInfo->mUsage -= aSize;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  AssertNoUnderflow(quotaManager->mTemporaryStorageUsage, aSize);
  quotaManager->mTemporaryStorageUsage -= aSize;
}

void OriginInfo::LockedResetUsageForClient(Client::Type aClientType) {
  AssertCurrentThreadOwnsQuotaMutex();

  uint64_t size = mClientUsages[aClientType].valueOr(0);

  mClientUsages[aClientType].reset();

  AssertNoUnderflow(mUsage, size);
  mUsage -= size;

  if (!LockedPersisted()) {
    AssertNoUnderflow(mGroupInfo->mUsage, size);
    mGroupInfo->mUsage -= size;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  AssertNoUnderflow(quotaManager->mTemporaryStorageUsage, size);
  quotaManager->mTemporaryStorageUsage -= size;
}

UsageInfo OriginInfo::LockedGetUsageForClient(Client::Type aClientType) {
  AssertCurrentThreadOwnsQuotaMutex();

  // The current implementation of this method only supports DOMCACHE and LS,
  // which only use DatabaseUsage. If this assertion is lifted, the logic below
  // must be adapted.
  MOZ_ASSERT(aClientType == Client::Type::DOMCACHE ||
             aClientType == Client::Type::LS);

  return UsageInfo{DatabaseUsageType{mClientUsages[aClientType]}};
}

void OriginInfo::LockedPersist() {
  AssertCurrentThreadOwnsQuotaMutex();
  MOZ_ASSERT(mGroupInfo->mPersistenceType == PERSISTENCE_TYPE_DEFAULT);
  MOZ_ASSERT(!mPersisted);

  mPersisted = true;

  // Remove Usage from GroupInfo
  AssertNoUnderflow(mGroupInfo->mUsage, mUsage);
  mGroupInfo->mUsage -= mUsage;
}

already_AddRefed<OriginInfo> GroupInfo::LockedGetOriginInfo(
    const nsACString& aOrigin) {
  AssertCurrentThreadOwnsQuotaMutex();

  for (const auto& originInfo : mOriginInfos) {
    if (originInfo->mOrigin == aOrigin) {
      RefPtr<OriginInfo> result = originInfo;
      return result.forget();
    }
  }

  return nullptr;
}

void GroupInfo::LockedAddOriginInfo(NotNull<RefPtr<OriginInfo>>&& aOriginInfo) {
  AssertCurrentThreadOwnsQuotaMutex();

  NS_ASSERTION(!mOriginInfos.Contains(aOriginInfo),
               "Replacing an existing entry!");
  mOriginInfos.AppendElement(std::move(aOriginInfo));

  uint64_t usage = aOriginInfo->LockedUsage();

  if (!aOriginInfo->LockedPersisted()) {
    AssertNoOverflow(mUsage, usage);
    mUsage += usage;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  AssertNoOverflow(quotaManager->mTemporaryStorageUsage, usage);
  quotaManager->mTemporaryStorageUsage += usage;
}

void GroupInfo::LockedAdjustUsageForRemovedOriginInfo(
    const OriginInfo& aOriginInfo) {
  const uint64_t usage = aOriginInfo.LockedUsage();

  if (!aOriginInfo.LockedPersisted()) {
    AssertNoUnderflow(mUsage, usage);
    mUsage -= usage;
  }

  QuotaManager* const quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  AssertNoUnderflow(quotaManager->mTemporaryStorageUsage, usage);
  quotaManager->mTemporaryStorageUsage -= usage;
}

void GroupInfo::LockedRemoveOriginInfo(const nsACString& aOrigin) {
  AssertCurrentThreadOwnsQuotaMutex();

  const auto foundIt = std::find_if(mOriginInfos.cbegin(), mOriginInfos.cend(),
                                    [&aOrigin](const auto& originInfo) {
                                      return originInfo->mOrigin == aOrigin;
                                    });

  // XXX Or can we MOZ_ASSERT(foundIt != mOriginInfos.cend()) ?
  if (foundIt != mOriginInfos.cend()) {
    LockedAdjustUsageForRemovedOriginInfo(**foundIt);

    mOriginInfos.RemoveElementAt(foundIt);
  }
}

void GroupInfo::LockedRemoveOriginInfos() {
  AssertCurrentThreadOwnsQuotaMutex();

  for (const auto& originInfo : std::exchange(mOriginInfos, {})) {
    LockedAdjustUsageForRemovedOriginInfo(*originInfo);
  }
}

RefPtr<GroupInfo>& GroupInfoPair::GetGroupInfoForPersistenceType(
    PersistenceType aPersistenceType) {
  switch (aPersistenceType) {
    case PERSISTENCE_TYPE_TEMPORARY:
      return mTemporaryStorageGroupInfo;
    case PERSISTENCE_TYPE_DEFAULT:
      return mDefaultStorageGroupInfo;

    case PERSISTENCE_TYPE_PERSISTENT:
    case PERSISTENCE_TYPE_INVALID:
    default:
      MOZ_CRASH("Bad persistence type value!");
  }
}

CollectOriginsHelper::CollectOriginsHelper(mozilla::Mutex& aMutex,
                                           uint64_t aMinSizeToBeFreed)
    : Runnable("dom::quota::CollectOriginsHelper"),
      mMinSizeToBeFreed(aMinSizeToBeFreed),
      mMutex(aMutex),
      mCondVar(aMutex, "CollectOriginsHelper::mCondVar"),
      mSizeToBeFreed(0),
      mWaiting(true) {
  MOZ_ASSERT(!NS_IsMainThread(), "Wrong thread!");
  mMutex.AssertCurrentThreadOwns();
}

int64_t CollectOriginsHelper::BlockAndReturnOriginsForEviction(
    nsTArray<RefPtr<OriginDirectoryLock>>& aLocks) {
  MOZ_ASSERT(!NS_IsMainThread(), "Wrong thread!");
  mMutex.AssertCurrentThreadOwns();

  while (mWaiting) {
    mCondVar.Wait();
  }

  mLocks.SwapElements(aLocks);
  return mSizeToBeFreed;
}

NS_IMETHODIMP
CollectOriginsHelper::Run() {
  AssertIsOnBackgroundThread();

  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "Shouldn't be null!");

  // We use extra stack vars here to avoid race detector warnings (the same
  // memory accessed with and without the lock held).
  nsTArray<RefPtr<OriginDirectoryLock>> locks;
  uint64_t sizeToBeFreed =
      quotaManager->CollectOriginsForEviction(mMinSizeToBeFreed, locks);

  MutexAutoLock lock(mMutex);

  NS_ASSERTION(mWaiting, "Huh?!");

  mLocks.SwapElements(locks);
  mSizeToBeFreed = sizeToBeFreed;
  mWaiting = false;
  mCondVar.Notify();

  return NS_OK;
}

/*******************************************************************************
 * OriginOperationBase
 ******************************************************************************/

NS_IMETHODIMP
OriginOperationBase::Run() {
  nsresult rv;

  switch (mState) {
    case State_Initial: {
      rv = Init();
      break;
    }

    case State_CreatingQuotaManager: {
      rv = QuotaManagerOpen();
      break;
    }

    case State_DirectoryOpenPending: {
      rv = DirectoryOpen();
      break;
    }

    case State_DirectoryWorkOpen: {
      rv = DirectoryWork();
      break;
    }

    case State_UnblockingOpen: {
      UnblockOpen();
      return NS_OK;
    }

    default:
      MOZ_CRASH("Bad state!");
  }

  if (NS_WARN_IF(NS_FAILED(rv)) && mState != State_UnblockingOpen) {
    Finish(rv);
  }

  return NS_OK;
}

nsresult OriginOperationBase::DirectoryOpen() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_DirectoryOpenPending);

  QuotaManager* const quotaManager = QuotaManager::Get();
  QM_TRY(OkIf(quotaManager), NS_ERROR_FAILURE);

  // Must set this before dispatching otherwise we will race with the IO thread.
  AdvanceState();

  QM_TRY(quotaManager->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL),
         NS_ERROR_FAILURE);

  return NS_OK;
}

void OriginOperationBase::Finish(nsresult aResult) {
  if (NS_SUCCEEDED(mResultCode)) {
    mResultCode = aResult;
  }

  // Must set mState before dispatching otherwise we will race with the main
  // thread.
  mState = State_UnblockingOpen;

  MOZ_ALWAYS_SUCCEEDS(mOwningThread->Dispatch(this, NS_DISPATCH_NORMAL));
}

nsresult OriginOperationBase::Init() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_Initial);

  if (QuotaManager::IsShuttingDown()) {
    return NS_ERROR_ABORT;
  }

  AdvanceState();

  if (mNeedsQuotaManagerInit && !QuotaManager::Get()) {
    QuotaManager::GetOrCreate(this);
  } else {
    Open();
  }

  return NS_OK;
}

nsresult OriginOperationBase::QuotaManagerOpen() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_CreatingQuotaManager);

  if (NS_WARN_IF(!QuotaManager::Get())) {
    return NS_ERROR_FAILURE;
  }

  Open();

  return NS_OK;
}

nsresult OriginOperationBase::DirectoryWork() {
  AssertIsOnIOThread();
  MOZ_ASSERT(mState == State_DirectoryWorkOpen);

  QuotaManager* const quotaManager = QuotaManager::Get();
  QM_TRY(OkIf(quotaManager), NS_ERROR_FAILURE);

  if (mNeedsStorageInit) {
    QM_TRY(quotaManager->EnsureStorageIsInitialized());
  }

  QM_TRY(DoDirectoryWork(*quotaManager));

  // Must set mState before dispatching otherwise we will race with the owning
  // thread.
  AdvanceState();

  MOZ_ALWAYS_SUCCEEDS(mOwningThread->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

void FinalizeOriginEvictionOp::Dispatch() {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(GetState() == State_Initial);

  SetState(State_DirectoryOpenPending);

  MOZ_ALWAYS_SUCCEEDS(mOwningThread->Dispatch(this, NS_DISPATCH_NORMAL));
}

void FinalizeOriginEvictionOp::RunOnIOThreadImmediately() {
  AssertIsOnIOThread();
  MOZ_ASSERT(GetState() == State_Initial);

  SetState(State_DirectoryWorkOpen);

  MOZ_ALWAYS_SUCCEEDS(this->Run());
}

void FinalizeOriginEvictionOp::Open() { MOZ_CRASH("Shouldn't get here!"); }

nsresult FinalizeOriginEvictionOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("FinalizeOriginEvictionOp::DoDirectoryWork", OTHER);

  for (const auto& lock : mLocks) {
    aQuotaManager.OriginClearCompleted(
        lock->GetPersistenceType(), lock->Origin(), Nullable<Client::Type>());
  }

  return NS_OK;
}

void FinalizeOriginEvictionOp::UnblockOpen() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(GetState() == State_UnblockingOpen);

#ifdef DEBUG
  NoteActorDestroyed();
#endif

  mLocks.Clear();

  AdvanceState();
}

NS_IMPL_ISUPPORTS_INHERITED0(NormalOriginOperationBase, Runnable)

void NormalOriginOperationBase::Open() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(GetState() == State_CreatingQuotaManager);
  MOZ_ASSERT(QuotaManager::Get());

  AdvanceState();

  if (mNeedsDirectoryLocking) {
    RefPtr<DirectoryLock> directoryLock =
        QuotaManager::Get()->CreateDirectoryLockInternal(
            mPersistenceType, mOriginScope, mClientType, mExclusive);

    directoryLock->Acquire(this);
  } else {
    QM_TRY(DirectoryOpen(), QM_VOID, [this](const nsresult rv) { Finish(rv); });
  }
}

void NormalOriginOperationBase::UnblockOpen() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(GetState() == State_UnblockingOpen);

  SendResults();

  if (mNeedsDirectoryLocking) {
    mDirectoryLock = nullptr;
  }

  UnregisterNormalOriginOp(*this);

  AdvanceState();
}

void NormalOriginOperationBase::DirectoryLockAcquired(DirectoryLock* aLock) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aLock);
  MOZ_ASSERT(GetState() == State_DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  mDirectoryLock = aLock;

  QM_TRY(DirectoryOpen(), QM_VOID, [this](const nsresult rv) { Finish(rv); });
}

void NormalOriginOperationBase::DirectoryLockFailed() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(GetState() == State_DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  Finish(NS_ERROR_FAILURE);
}

nsresult SaveOriginAccessTimeOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!mPersistenceType.IsNull());
  MOZ_ASSERT(mOriginScope.IsOrigin());

  AUTO_PROFILER_LABEL("SaveOriginAccessTimeOp::DoDirectoryWork", OTHER);

  QM_TRY_INSPECT(const auto& file,
                 aQuotaManager.GetDirectoryForOrigin(mPersistenceType.Value(),
                                                     mOriginScope.GetOrigin()));

  // The origin directory might not exist
  // anymore, because it was deleted by a clear operation.
  QM_TRY_INSPECT(const bool& exists, MOZ_TO_RESULT_INVOKE(file, Exists));

  if (exists) {
    QM_TRY(file->Append(nsLiteralString(METADATA_V2_FILE_NAME)));

    QM_TRY_INSPECT(const auto& stream,
                   GetBinaryOutputStream(*file, FileFlag::Update));
    MOZ_ASSERT(stream);

    QM_TRY(stream->Write64(mTimestamp));
  }

  return NS_OK;
}

void SaveOriginAccessTimeOp::SendResults() {
#ifdef DEBUG
  NoteActorDestroyed();
#endif
}

NS_IMETHODIMP
StoragePressureRunnable::Run() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
  if (NS_WARN_IF(!obsSvc)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsISupportsPRUint64> wrapper =
      do_CreateInstance(NS_SUPPORTS_PRUINT64_CONTRACTID);
  if (NS_WARN_IF(!wrapper)) {
    return NS_ERROR_FAILURE;
  }

  wrapper->SetData(mUsage);

  obsSvc->NotifyObservers(wrapper, "QuotaManager::StoragePressure", u"");

  return NS_OK;
}

void RecordQuotaInfoLoadTimeHelper::Start() {
  AssertIsOnIOThread();

  // XXX: If a OS sleep/wake occur after mStartTime is initialized but before
  // gLastOSWake is set, then this time duration would still be recorded with
  // key "Normal". We are assumming this is rather rare to happen.
  mStartTime.init(TimeStamp::Now());
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(this));
}

void RecordQuotaInfoLoadTimeHelper::End() {
  AssertIsOnIOThread();

  mEndTime.init(TimeStamp::Now());
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(this));
}

NS_IMETHODIMP
RecordQuotaInfoLoadTimeHelper::Run() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mInitializedTime.isSome()) {
    // Keys for QM_QUOTA_INFO_LOAD_TIME_V0:
    // Normal: Normal conditions.
    // WasSuspended: There was a OS sleep so that it was suspended.
    // TimeStampErr1: The recorded start time is unexpectedly greater than the
    //                end time.
    // TimeStampErr2: The initialized time for the recording class is unexpectly
    //                greater than the last OS wake time.
    const auto key = [this, wasSuspended = gLastOSWake > *mInitializedTime]() {
      if (wasSuspended) {
        return "WasSuspended"_ns;
      }

      // XXX File a bug if we have data for this key.
      // We found negative values in our query in STMO for
      // ScalarID::QM_REPOSITORIES_INITIALIZATION_TIME. This shouldn't happen
      // because the documentation for TimeStamp::Now() says it returns a
      // monotonically increasing number.
      if (*mStartTime > *mEndTime) {
        return "TimeStampErr1"_ns;
      }

      if (*mInitializedTime > gLastOSWake) {
        return "TimeStampErr2"_ns;
      }

      return "Normal"_ns;
    }();

    Telemetry::AccumulateTimeDelta(Telemetry::QM_QUOTA_INFO_LOAD_TIME_V0, key,
                                   *mStartTime, *mEndTime);

    return NS_OK;
  }

  gLastOSWake = TimeStamp::Now();
  mInitializedTime.init(gLastOSWake);

  return NS_OK;
}

/*******************************************************************************
 * Quota
 ******************************************************************************/

Quota::Quota()
#ifdef DEBUG
    : mActorDestroyed(false)
#endif
{
}

Quota::~Quota() { MOZ_ASSERT(mActorDestroyed); }

void Quota::StartIdleMaintenance() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!QuotaManager::IsShuttingDown());

  QuotaManager* const quotaManager = QuotaManager::Get();
  QM_TRY(OkIf(quotaManager), QM_VOID);

  quotaManager->StartIdleMaintenance();
}

bool Quota::VerifyRequestParams(const UsageRequestParams& aParams) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != UsageRequestParams::T__None);

  switch (aParams.type()) {
    case UsageRequestParams::TAllUsageParams:
      break;

    case UsageRequestParams::TOriginUsageParams: {
      const OriginUsageParams& params = aParams.get_OriginUsageParams();

      if (NS_WARN_IF(
              !QuotaManager::IsPrincipalInfoValid(params.principalInfo()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  return true;
}

bool Quota::VerifyRequestParams(const RequestParams& aParams) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  switch (aParams.type()) {
    case RequestParams::TStorageNameParams:
    case RequestParams::TStorageInitializedParams:
    case RequestParams::TTemporaryStorageInitializedParams:
    case RequestParams::TInitParams:
    case RequestParams::TInitTemporaryStorageParams:
      break;

    case RequestParams::TInitializePersistentOriginParams: {
      const InitializePersistentOriginParams& params =
          aParams.get_InitializePersistentOriginParams();

      if (NS_WARN_IF(
              !QuotaManager::IsPrincipalInfoValid(params.principalInfo()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    case RequestParams::TInitializeTemporaryOriginParams: {
      const InitializeTemporaryOriginParams& params =
          aParams.get_InitializeTemporaryOriginParams();

      if (NS_WARN_IF(!IsBestEffortPersistenceType(params.persistenceType()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      if (NS_WARN_IF(
              !QuotaManager::IsPrincipalInfoValid(params.principalInfo()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    case RequestParams::TClearOriginParams: {
      const ClearResetOriginParams& params =
          aParams.get_ClearOriginParams().commonParams();

      if (NS_WARN_IF(
              !QuotaManager::IsPrincipalInfoValid(params.principalInfo()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      if (params.persistenceTypeIsExplicit()) {
        if (NS_WARN_IF(!IsValidPersistenceType(params.persistenceType()))) {
          ASSERT_UNLESS_FUZZING();
          return false;
        }
      }

      if (params.clientTypeIsExplicit()) {
        if (NS_WARN_IF(!Client::IsValidType(params.clientType()))) {
          ASSERT_UNLESS_FUZZING();
          return false;
        }
      }

      break;
    }

    case RequestParams::TResetOriginParams: {
      const ClearResetOriginParams& params =
          aParams.get_ResetOriginParams().commonParams();

      if (NS_WARN_IF(
              !QuotaManager::IsPrincipalInfoValid(params.principalInfo()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      if (params.persistenceTypeIsExplicit()) {
        if (NS_WARN_IF(!IsValidPersistenceType(params.persistenceType()))) {
          ASSERT_UNLESS_FUZZING();
          return false;
        }
      }

      if (params.clientTypeIsExplicit()) {
        if (NS_WARN_IF(!Client::IsValidType(params.clientType()))) {
          ASSERT_UNLESS_FUZZING();
          return false;
        }
      }

      break;
    }

    case RequestParams::TClearDataParams: {
      if (BackgroundParent::IsOtherProcessActor(Manager())) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    case RequestParams::TClearAllParams:
    case RequestParams::TResetAllParams:
    case RequestParams::TListOriginsParams:
      break;

    case RequestParams::TPersistedParams: {
      const PersistedParams& params = aParams.get_PersistedParams();

      if (NS_WARN_IF(
              !QuotaManager::IsPrincipalInfoValid(params.principalInfo()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    case RequestParams::TPersistParams: {
      const PersistParams& params = aParams.get_PersistParams();

      if (NS_WARN_IF(
              !QuotaManager::IsPrincipalInfoValid(params.principalInfo()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    case RequestParams::TEstimateParams: {
      const EstimateParams& params = aParams.get_EstimateParams();

      if (NS_WARN_IF(
              !QuotaManager::IsPrincipalInfoValid(params.principalInfo()))) {
        ASSERT_UNLESS_FUZZING();
        return false;
      }

      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  return true;
}

void Quota::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();
#ifdef DEBUG
  MOZ_ASSERT(!mActorDestroyed);
  mActorDestroyed = true;
#endif
}

PQuotaUsageRequestParent* Quota::AllocPQuotaUsageRequestParent(
    const UsageRequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != UsageRequestParams::T__None);

  if (NS_WARN_IF(QuotaManager::IsShuttingDown())) {
    return nullptr;
  }

#ifdef DEBUG
  // Always verify parameters in DEBUG builds!
  bool trustParams = false;
#else
  bool trustParams = !BackgroundParent::IsOtherProcessActor(Manager());
#endif

  if (!trustParams && NS_WARN_IF(!VerifyRequestParams(aParams))) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  auto actor = [&]() -> RefPtr<QuotaUsageRequestBase> {
    switch (aParams.type()) {
      case UsageRequestParams::TAllUsageParams:
        return MakeRefPtr<GetUsageOp>(aParams);

      case UsageRequestParams::TOriginUsageParams:
        return MakeRefPtr<GetOriginUsageOp>(aParams);

      default:
        MOZ_CRASH("Should never get here!");
    }
  }();

  MOZ_ASSERT(actor);

  RegisterNormalOriginOp(*actor);

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

mozilla::ipc::IPCResult Quota::RecvPQuotaUsageRequestConstructor(
    PQuotaUsageRequestParent* aActor, const UsageRequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != UsageRequestParams::T__None);
  MOZ_ASSERT(!QuotaManager::IsShuttingDown());

  auto* op = static_cast<QuotaUsageRequestBase*>(aActor);

  op->Init(*this);

  op->RunImmediately();
  return IPC_OK();
}

bool Quota::DeallocPQuotaUsageRequestParent(PQuotaUsageRequestParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<QuotaUsageRequestBase> actor =
      dont_AddRef(static_cast<QuotaUsageRequestBase*>(aActor));
  return true;
}

PQuotaRequestParent* Quota::AllocPQuotaRequestParent(
    const RequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  if (NS_WARN_IF(QuotaManager::IsShuttingDown())) {
    return nullptr;
  }

#ifdef DEBUG
  // Always verify parameters in DEBUG builds!
  bool trustParams = false;
#else
  bool trustParams = !BackgroundParent::IsOtherProcessActor(Manager());
#endif

  if (!trustParams && NS_WARN_IF(!VerifyRequestParams(aParams))) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  auto actor = [&]() -> RefPtr<QuotaRequestBase> {
    switch (aParams.type()) {
      case RequestParams::TStorageNameParams:
        return MakeRefPtr<StorageNameOp>();

      case RequestParams::TStorageInitializedParams:
        return MakeRefPtr<StorageInitializedOp>();

      case RequestParams::TTemporaryStorageInitializedParams:
        return MakeRefPtr<TemporaryStorageInitializedOp>();

      case RequestParams::TInitParams:
        return MakeRefPtr<InitOp>();

      case RequestParams::TInitTemporaryStorageParams:
        return MakeRefPtr<InitTemporaryStorageOp>();

      case RequestParams::TInitializePersistentOriginParams:
        return MakeRefPtr<InitializePersistentOriginOp>(aParams);

      case RequestParams::TInitializeTemporaryOriginParams:
        return MakeRefPtr<InitializeTemporaryOriginOp>(aParams);

      case RequestParams::TClearOriginParams:
        return MakeRefPtr<ClearOriginOp>(aParams);

      case RequestParams::TResetOriginParams:
        return MakeRefPtr<ResetOriginOp>(aParams);

      case RequestParams::TClearDataParams:
        return MakeRefPtr<ClearDataOp>(aParams);

      case RequestParams::TClearAllParams:
        return MakeRefPtr<ResetOrClearOp>(/* aClear */ true);

      case RequestParams::TResetAllParams:
        return MakeRefPtr<ResetOrClearOp>(/* aClear */ false);

      case RequestParams::TPersistedParams:
        return MakeRefPtr<PersistedOp>(aParams);

      case RequestParams::TPersistParams:
        return MakeRefPtr<PersistOp>(aParams);

      case RequestParams::TEstimateParams:
        return MakeRefPtr<EstimateOp>(aParams);

      case RequestParams::TListOriginsParams:
        return MakeRefPtr<ListOriginsOp>();

      default:
        MOZ_CRASH("Should never get here!");
    }
  }();

  MOZ_ASSERT(actor);

  RegisterNormalOriginOp(*actor);

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

mozilla::ipc::IPCResult Quota::RecvPQuotaRequestConstructor(
    PQuotaRequestParent* aActor, const RequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);
  MOZ_ASSERT(!QuotaManager::IsShuttingDown());

  auto* op = static_cast<QuotaRequestBase*>(aActor);

  op->Init(*this);

  op->RunImmediately();
  return IPC_OK();
}

bool Quota::DeallocPQuotaRequestParent(PQuotaRequestParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<QuotaRequestBase> actor =
      dont_AddRef(static_cast<QuotaRequestBase*>(aActor));
  return true;
}

mozilla::ipc::IPCResult Quota::RecvStartIdleMaintenance() {
  AssertIsOnBackgroundThread();

  PBackgroundParent* actor = Manager();
  MOZ_ASSERT(actor);

  if (BackgroundParent::IsOtherProcessActor(actor)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (QuotaManager::IsShuttingDown()) {
    return IPC_OK();
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    nsCOMPtr<nsIRunnable> callback =
        NewRunnableMethod("dom::quota::Quota::StartIdleMaintenance", this,
                          &Quota::StartIdleMaintenance);

    QuotaManager::GetOrCreate(callback);
    return IPC_OK();
  }

  quotaManager->StartIdleMaintenance();

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvStopIdleMaintenance() {
  AssertIsOnBackgroundThread();

  PBackgroundParent* actor = Manager();
  MOZ_ASSERT(actor);

  if (BackgroundParent::IsOtherProcessActor(actor)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (QuotaManager::IsShuttingDown()) {
    return IPC_OK();
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    return IPC_OK();
  }

  quotaManager->StopIdleMaintenance();

  return IPC_OK();
}

mozilla::ipc::IPCResult Quota::RecvAbortOperationsForProcess(
    const ContentParentId& aContentParentId) {
  AssertIsOnBackgroundThread();

  PBackgroundParent* actor = Manager();
  MOZ_ASSERT(actor);

  if (BackgroundParent::IsOtherProcessActor(actor)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (QuotaManager::IsShuttingDown()) {
    return IPC_OK();
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    return IPC_OK();
  }

  quotaManager->AbortOperationsForProcess(aContentParentId);

  return IPC_OK();
}

void QuotaUsageRequestBase::Init(Quota& aQuota) {
  AssertIsOnOwningThread();

  mNeedsQuotaManagerInit = true;
  mNeedsStorageInit = true;
}

Result<UsageInfo, nsresult> QuotaUsageRequestBase::GetUsageForOrigin(
    QuotaManager& aQuotaManager, PersistenceType aPersistenceType,
    const OriginMetadata& aOriginMetadata) {
  AssertIsOnIOThread();

  QM_TRY_INSPECT(const auto& directory,
                 aQuotaManager.GetDirectoryForOrigin(aPersistenceType,
                                                     aOriginMetadata.mOrigin));

  QM_TRY_INSPECT(const bool& exists, MOZ_TO_RESULT_INVOKE(directory, Exists));

  if (!exists || mCanceled) {
    return UsageInfo();
  }

  // If the directory exists then enumerate all the files inside, adding up
  // the sizes to get the final usage statistic.
  bool initialized;

  if (aPersistenceType == PERSISTENCE_TYPE_PERSISTENT) {
    initialized = aQuotaManager.IsOriginInitialized(aOriginMetadata.mOrigin);
  } else {
    initialized = aQuotaManager.IsTemporaryStorageInitialized();
  }

  return GetUsageForOriginEntries(aQuotaManager, aPersistenceType,
                                  aOriginMetadata, *directory, initialized);
}

Result<UsageInfo, nsresult> QuotaUsageRequestBase::GetUsageForOriginEntries(
    QuotaManager& aQuotaManager, PersistenceType aPersistenceType,
    const OriginMetadata& aOriginMetadata, nsIFile& aDirectory,
    const bool aInitialized) {
  AssertIsOnIOThread();

  QM_TRY_RETURN((ReduceEachFileAtomicCancelable(
      aDirectory, mCanceled, UsageInfo{},
      [&](UsageInfo oldUsageInfo, const nsCOMPtr<nsIFile>& file)
          -> mozilla::Result<UsageInfo, nsresult> {
        QM_TRY_INSPECT(
            const auto& leafName,
            MOZ_TO_RESULT_INVOKE_TYPED(nsAutoString, file, GetLeafName));

        QM_TRY_INSPECT(const auto& dirEntryKind, GetDirEntryKind(*file));

        switch (dirEntryKind) {
          case nsIFileKind::ExistsAsDirectory: {
            Client::Type clientType;
            const bool ok =
                Client::TypeFromText(leafName, clientType, fallible);
            if (!ok) {
              // Unknown directories during getting usage for an origin (even
              // for an uninitialized origin) are now allowed. Just warn if we
              // find them.
              UNKNOWN_FILE_WARNING(leafName);
              break;
            }

            Client* const client = aQuotaManager.GetClient(clientType);
            MOZ_ASSERT(client);

            QM_TRY_INSPECT(
                const auto& usageInfo,
                aInitialized ? client->GetUsageForOrigin(
                                   aPersistenceType, aOriginMetadata, mCanceled)
                             : client->InitOrigin(aPersistenceType,
                                                  aOriginMetadata, mCanceled));
            return oldUsageInfo + usageInfo;
          }

          case nsIFileKind::ExistsAsFile:
            // We are maintaining existing behavior for unknown files here (just
            // continuing).
            // This can possibly be used by developers to add temporary backups
            // into origin directories without losing get usage functionality.
            if (IsTempMetadata(leafName)) {
              if (!aInitialized) {
                QM_TRY(file->Remove(/* recursive */ false));
              }

              break;
            }

            if (IsOriginMetadata(leafName) || IsOSMetadata(leafName) ||
                IsDotFile(leafName)) {
              break;
            }

            // Unknown files during getting usage for an origin (even for an
            // uninitialized origin) are now allowed. Just warn if we find them.
            UNKNOWN_FILE_WARNING(leafName);
            break;

          case nsIFileKind::DoesNotExist:
            // Ignore files that got removed externally while iterating.
            break;
        }

        return oldUsageInfo;
      })));
}

void QuotaUsageRequestBase::SendResults() {
  AssertIsOnOwningThread();

  if (IsActorDestroyed()) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = NS_ERROR_FAILURE;
    }
  } else {
    if (mCanceled) {
      mResultCode = NS_ERROR_FAILURE;
    }

    UsageRequestResponse response;

    if (NS_SUCCEEDED(mResultCode)) {
      GetResponse(response);
    } else {
      response = mResultCode;
    }

    Unused << PQuotaUsageRequestParent::Send__delete__(this, response);
  }
}

void QuotaUsageRequestBase::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  NoteActorDestroyed();
}

mozilla::ipc::IPCResult QuotaUsageRequestBase::RecvCancel() {
  AssertIsOnOwningThread();

  if (mCanceled.exchange(true)) {
    NS_WARNING("Canceled more than once?!");
    return IPC_FAIL_NO_REASON(this);
  }

  return IPC_OK();
}

nsresult TraverseRepositoryHelper::TraverseRepository(
    QuotaManager& aQuotaManager, PersistenceType aPersistenceType) {
  AssertIsOnIOThread();

  QM_TRY_INSPECT(
      const auto& directory,
      QM_NewLocalFile(aQuotaManager.GetStoragePath(aPersistenceType)));

  QM_TRY_INSPECT(const bool& exists, MOZ_TO_RESULT_INVOKE(directory, Exists));

  if (!exists) {
    return NS_OK;
  }

  QM_TRY(CollectEachFileAtomicCancelable(
      *directory, GetIsCanceledFlag(),
      [this, aPersistenceType, &aQuotaManager,
       persistent = aPersistenceType == PERSISTENCE_TYPE_PERSISTENT](
          const nsCOMPtr<nsIFile>& originDir) -> Result<Ok, nsresult> {
        QM_TRY_INSPECT(const auto& dirEntryKind, GetDirEntryKind(*originDir));

        switch (dirEntryKind) {
          case nsIFileKind::ExistsAsDirectory:
            QM_TRY(ProcessOrigin(aQuotaManager, *originDir, persistent,
                                 aPersistenceType));
            break;

          case nsIFileKind::ExistsAsFile: {
            QM_TRY_INSPECT(const auto& leafName,
                           MOZ_TO_RESULT_INVOKE_TYPED(nsAutoString, originDir,
                                                      GetLeafName));

            // Unknown files during getting usages are allowed. Just warn if we
            // find them.
            if (!IsOSMetadata(leafName)) {
              UNKNOWN_FILE_WARNING(leafName);
            }

            break;
          }

          case nsIFileKind::DoesNotExist:
            // Ignore files that got removed externally while iterating.
            break;
        }

        return Ok{};
      }));

  return NS_OK;
}

GetUsageOp::GetUsageOp(const UsageRequestParams& aParams)
    : mGetAll(aParams.get_AllUsageParams().getAll()) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aParams.type() == UsageRequestParams::TAllUsageParams);
}

void GetUsageOp::ProcessOriginInternal(QuotaManager* aQuotaManager,
                                       const PersistenceType aPersistenceType,
                                       const nsACString& aOrigin,
                                       const int64_t aTimestamp,
                                       const bool aPersisted,
                                       const uint64_t aUsage) {
  if (!mGetAll && aQuotaManager->IsOriginInternal(aOrigin)) {
    return;
  }

  // We can't store pointers to OriginUsage objects in the hashtable
  // since AppendElement() reallocates its internal array buffer as number
  // of elements grows.
  const auto& originUsage =
      mOriginUsagesIndex.WithEntryHandle(aOrigin, [&](auto&& entry) {
        if (entry) {
          return WrapNotNullUnchecked(&mOriginUsages[entry.Data()]);
        }

        entry.Insert(mOriginUsages.Length());

        return mOriginUsages.EmplaceBack(nsCString{aOrigin}, false, 0, 0);
      });

  if (aPersistenceType == PERSISTENCE_TYPE_DEFAULT) {
    originUsage->persisted() = aPersisted;
  }

  originUsage->usage() = originUsage->usage() + aUsage;

  originUsage->lastAccessed() =
      std::max<int64_t>(originUsage->lastAccessed(), aTimestamp);
}

const Atomic<bool>& GetUsageOp::GetIsCanceledFlag() {
  AssertIsOnIOThread();

  return mCanceled;
}

// XXX Remove aPersistent
// XXX Remove aPersistenceType once GetUsageForOrigin uses the persistence
// type from OriginMetadata
nsresult GetUsageOp::ProcessOrigin(QuotaManager& aQuotaManager,
                                   nsIFile& aOriginDir, const bool aPersistent,
                                   const PersistenceType aPersistenceType) {
  AssertIsOnIOThread();

  QM_TRY_INSPECT(const auto& metadata,
                 aQuotaManager.LoadFullOriginMetadataWithRestore(&aOriginDir));

  QM_TRY_INSPECT(const auto& usageInfo,
                 GetUsageForOrigin(aQuotaManager, aPersistenceType, metadata));

  ProcessOriginInternal(&aQuotaManager, aPersistenceType, metadata.mOrigin,
                        metadata.mLastAccessTime, metadata.mPersisted,
                        usageInfo.TotalUsage().valueOr(0));

  return NS_OK;
}

nsresult GetUsageOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitialized();

  AUTO_PROFILER_LABEL("GetUsageOp::DoDirectoryWork", OTHER);

  nsresult rv;

  for (const PersistenceType type : kAllPersistenceTypes) {
    rv = TraverseRepository(aQuotaManager, type);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // TraverseRepository above only consulted the filesystem. We also need to
  // consider origins which may have pending quota usage, such as buffered
  // LocalStorage writes for an origin which didn't previously have any
  // LocalStorage data.

  aQuotaManager.CollectPendingOriginsForListing(
      [this, &aQuotaManager](const auto& originInfo) {
        ProcessOriginInternal(
            &aQuotaManager, originInfo->GetGroupInfo()->GetPersistenceType(),
            originInfo->Origin(), originInfo->LockedAccessTime(),
            originInfo->LockedPersisted(), originInfo->LockedUsage());
      });

  return NS_OK;
}

void GetUsageOp::GetResponse(UsageRequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = AllUsageResponse();

  aResponse.get_AllUsageResponse().originUsages() = std::move(mOriginUsages);
}

GetOriginUsageOp::GetOriginUsageOp(const UsageRequestParams& aParams)
    : mUsage(0), mFileUsage(0) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aParams.type() == UsageRequestParams::TOriginUsageParams);

  const OriginUsageParams& params = aParams.get_OriginUsageParams();

  PrincipalMetadata principalMetadata =
      QuotaManager::GetInfoFromValidatedPrincipalInfo(params.principalInfo());

  mSuffix = std::move(principalMetadata.mSuffix);
  mGroup = std::move(principalMetadata.mGroup);
  mOriginScope.SetFromOrigin(principalMetadata.mOrigin);

  mFromMemory = params.fromMemory();

  // Overwrite NormalOriginOperationBase default values.
  if (mFromMemory) {
    mNeedsDirectoryLocking = false;
  }

  // Overwrite OriginOperationBase default values.
  mNeedsQuotaManagerInit = true;
  mNeedsStorageInit = true;
}

nsresult GetOriginUsageOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitialized();
  MOZ_ASSERT(mUsage == 0);
  MOZ_ASSERT(mFileUsage == 0);

  AUTO_PROFILER_LABEL("GetOriginUsageOp::DoDirectoryWork", OTHER);

  if (mFromMemory) {
    const PrincipalMetadata principalMetadata = {
        mSuffix, mGroup, nsCString{mOriginScope.GetOrigin()}};

    // Ensure temporary storage is initialized. If temporary storage hasn't been
    // initialized yet, the method will initialize it by traversing the
    // repositories for temporary and default storage (including our origin).
    QM_TRY(aQuotaManager.EnsureTemporaryStorageIsInitialized());

    // Get cached usage (the method doesn't have to stat any files). File usage
    // is not tracked in memory separately, so just add to the total usage.
    mUsage = aQuotaManager.GetOriginUsage(principalMetadata);

    return NS_OK;
  }

  UsageInfo usageInfo;

  // Add all the persistent/temporary/default storage files we care about.
  for (const PersistenceType type : kAllPersistenceTypes) {
    const OriginMetadata originMetadata = {
        mSuffix, mGroup, nsCString{mOriginScope.GetOrigin()}, type};

    auto usageInfoOrErr =
        GetUsageForOrigin(aQuotaManager, type, originMetadata);
    if (NS_WARN_IF(usageInfoOrErr.isErr())) {
      return usageInfoOrErr.unwrapErr();
    }

    usageInfo += usageInfoOrErr.unwrap();
  }

  mUsage = usageInfo.TotalUsage().valueOr(0);
  mFileUsage = usageInfo.FileUsage().valueOr(0);

  return NS_OK;
}

void GetOriginUsageOp::GetResponse(UsageRequestResponse& aResponse) {
  AssertIsOnOwningThread();

  OriginUsageResponse usageResponse;

  usageResponse.usage() = mUsage;
  usageResponse.fileUsage() = mFileUsage;

  aResponse = usageResponse;
}

void QuotaRequestBase::Init(Quota& aQuota) {
  AssertIsOnOwningThread();

  mNeedsQuotaManagerInit = true;
  mNeedsStorageInit = true;
}

void QuotaRequestBase::SendResults() {
  AssertIsOnOwningThread();

  if (IsActorDestroyed()) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = NS_ERROR_FAILURE;
    }
  } else {
    RequestResponse response;

    if (NS_SUCCEEDED(mResultCode)) {
      GetResponse(response);
    } else {
      response = mResultCode;
    }

    Unused << PQuotaRequestParent::Send__delete__(this, response);
  }
}

void QuotaRequestBase::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  NoteActorDestroyed();
}

StorageNameOp::StorageNameOp() : QuotaRequestBase(/* aExclusive */ false) {
  AssertIsOnOwningThread();

  // Overwrite NormalOriginOperationBase default values.
  mNeedsDirectoryLocking = false;

  // Overwrite OriginOperationBase default values.
  mNeedsQuotaManagerInit = true;
  mNeedsStorageInit = false;
}

void StorageNameOp::Init(Quota& aQuota) { AssertIsOnOwningThread(); }

nsresult StorageNameOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("StorageNameOp::DoDirectoryWork", OTHER);

  mName = aQuotaManager.GetStorageName();

  return NS_OK;
}

void StorageNameOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  StorageNameResponse storageNameResponse;

  storageNameResponse.name() = mName;

  aResponse = storageNameResponse;
}

InitializedRequestBase::InitializedRequestBase()
    : QuotaRequestBase(/* aExclusive */ false), mInitialized(false) {
  AssertIsOnOwningThread();

  // Overwrite NormalOriginOperationBase default values.
  mNeedsDirectoryLocking = false;

  // Overwrite OriginOperationBase default values.
  mNeedsQuotaManagerInit = true;
  mNeedsStorageInit = false;
}

void InitializedRequestBase::Init(Quota& aQuota) { AssertIsOnOwningThread(); }

nsresult StorageInitializedOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("StorageInitializedOp::DoDirectoryWork", OTHER);

  mInitialized = aQuotaManager.IsStorageInitialized();

  return NS_OK;
}

void StorageInitializedOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  StorageInitializedResponse storageInitializedResponse;

  storageInitializedResponse.initialized() = mInitialized;

  aResponse = storageInitializedResponse;
}

nsresult TemporaryStorageInitializedOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("TemporaryStorageInitializedOp::DoDirectoryWork", OTHER);

  mInitialized = aQuotaManager.IsTemporaryStorageInitialized();

  return NS_OK;
}

void TemporaryStorageInitializedOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  TemporaryStorageInitializedResponse temporaryStorageInitializedResponse;

  temporaryStorageInitializedResponse.initialized() = mInitialized;

  aResponse = temporaryStorageInitializedResponse;
}

InitOp::InitOp() : QuotaRequestBase(/* aExclusive */ false) {
  AssertIsOnOwningThread();

  // Overwrite OriginOperationBase default values.
  mNeedsQuotaManagerInit = true;
  mNeedsStorageInit = false;
}

void InitOp::Init(Quota& aQuota) { AssertIsOnOwningThread(); }

nsresult InitOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("InitOp::DoDirectoryWork", OTHER);

  QM_TRY(aQuotaManager.EnsureStorageIsInitialized());

  return NS_OK;
}

void InitOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = InitResponse();
}

InitTemporaryStorageOp::InitTemporaryStorageOp()
    : QuotaRequestBase(/* aExclusive */ false) {
  AssertIsOnOwningThread();

  // Overwrite OriginOperationBase default values.
  mNeedsQuotaManagerInit = true;
  mNeedsStorageInit = false;
}

void InitTemporaryStorageOp::Init(Quota& aQuota) { AssertIsOnOwningThread(); }

nsresult InitTemporaryStorageOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("InitTemporaryStorageOp::DoDirectoryWork", OTHER);

  QM_TRY(OkIf(aQuotaManager.IsStorageInitialized()), NS_ERROR_FAILURE;);

  QM_TRY(aQuotaManager.EnsureTemporaryStorageIsInitialized());

  return NS_OK;
}

void InitTemporaryStorageOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = InitTemporaryStorageResponse();
}

InitializeOriginRequestBase::InitializeOriginRequestBase(
    const PersistenceType aPersistenceType, const PrincipalInfo& aPrincipalInfo)
    : QuotaRequestBase(/* aExclusive */ false), mCreated(false) {
  AssertIsOnOwningThread();

  auto principalMetadata =
      QuotaManager::GetInfoFromValidatedPrincipalInfo(aPrincipalInfo);

  // Overwrite OriginOperationBase default values.
  mNeedsQuotaManagerInit = true;
  mNeedsStorageInit = false;

  // Overwrite NormalOriginOperationBase default values.
  mPersistenceType.SetValue(aPersistenceType);
  mOriginScope.SetFromOrigin(principalMetadata.mOrigin);

  // Overwrite InitializeOriginRequestBase default values.
  mSuffix = std::move(principalMetadata.mSuffix);
  mGroup = std::move(principalMetadata.mGroup);
}

void InitializeOriginRequestBase::Init(Quota& aQuota) {
  AssertIsOnOwningThread();
}

InitializePersistentOriginOp::InitializePersistentOriginOp(
    const RequestParams& aParams)
    : InitializeOriginRequestBase(
          PERSISTENCE_TYPE_PERSISTENT,
          aParams.get_InitializePersistentOriginParams().principalInfo()) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aParams.type() ==
             RequestParams::TInitializePersistentOriginParams);
}

nsresult InitializePersistentOriginOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!mPersistenceType.IsNull());

  AUTO_PROFILER_LABEL("InitializePersistentOriginOp::DoDirectoryWork", OTHER);

  QM_TRY(OkIf(aQuotaManager.IsStorageInitialized()), NS_ERROR_FAILURE);

  QM_TRY_UNWRAP(mCreated,
                (aQuotaManager
                     .EnsurePersistentOriginIsInitialized(OriginMetadata{
                         mSuffix, mGroup, nsCString{mOriginScope.GetOrigin()},
                         PERSISTENCE_TYPE_PERSISTENT})
                     .map([](const auto& res) { return res.second; })));

  return NS_OK;
}

void InitializePersistentOriginOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = InitializePersistentOriginResponse(mCreated);
}

InitializeTemporaryOriginOp::InitializeTemporaryOriginOp(
    const RequestParams& aParams)
    : InitializeOriginRequestBase(
          aParams.get_InitializeTemporaryOriginParams().persistenceType(),
          aParams.get_InitializeTemporaryOriginParams().principalInfo()) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aParams.type() == RequestParams::TInitializeTemporaryOriginParams);
}

nsresult InitializeTemporaryOriginOp::DoDirectoryWork(
    QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!mPersistenceType.IsNull());

  AUTO_PROFILER_LABEL("InitializeTemporaryOriginOp::DoDirectoryWork", OTHER);

  QM_TRY(OkIf(aQuotaManager.IsStorageInitialized()), NS_ERROR_FAILURE);

  QM_TRY(OkIf(aQuotaManager.IsTemporaryStorageInitialized()), NS_ERROR_FAILURE);

  QM_TRY_UNWRAP(mCreated,
                (aQuotaManager
                     .EnsureTemporaryOriginIsInitialized(
                         mPersistenceType.Value(),
                         OriginMetadata{mSuffix, mGroup,
                                        nsCString{mOriginScope.GetOrigin()},
                                        mPersistenceType.Value()})
                     .map([](const auto& res) { return res.second; })));

  return NS_OK;
}

void InitializeTemporaryOriginOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = InitializeTemporaryOriginResponse(mCreated);
}

ResetOrClearOp::ResetOrClearOp(bool aClear)
    : QuotaRequestBase(/* aExclusive */ true), mClear(aClear) {
  AssertIsOnOwningThread();

  // Overwrite OriginOperationBase default values.
  mNeedsQuotaManagerInit = true;
  mNeedsStorageInit = false;
}

void ResetOrClearOp::Init(Quota& aQuota) { AssertIsOnOwningThread(); }

void ResetOrClearOp::DeleteFiles(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  nsresult rv = aQuotaManager.AboutToClearOrigins(Nullable<PersistenceType>(),
                                                  OriginScope::FromNull(),
                                                  Nullable<Client::Type>());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  auto directoryOrErr = QM_NewLocalFile(aQuotaManager.GetStoragePath());
  if (NS_WARN_IF(directoryOrErr.isErr())) {
    return;
  }

  nsCOMPtr<nsIFile> directory = directoryOrErr.unwrap();

  rv = directory->Remove(true);
  if (rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST &&
      rv != NS_ERROR_FILE_NOT_FOUND && NS_FAILED(rv)) {
    // This should never fail if we've closed all storage connections
    // correctly...
    MOZ_ASSERT(false, "Failed to remove storage directory!");
  }
}

void ResetOrClearOp::DeleteStorageFile(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  QM_TRY_INSPECT(const auto& storageFile,
                 QM_NewLocalFile(aQuotaManager.GetBasePath()), QM_VOID);

  QM_TRY(storageFile->Append(aQuotaManager.GetStorageName() + kSQLiteSuffix),
         QM_VOID);

  const nsresult rv = storageFile->Remove(true);
  if (rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST &&
      rv != NS_ERROR_FILE_NOT_FOUND && NS_FAILED(rv)) {
    // This should never fail if we've closed the storage connection
    // correctly...
    MOZ_ASSERT(false, "Failed to remove storage file!");
  }
}

nsresult ResetOrClearOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("ResetOrClearOp::DoDirectoryWork", OTHER);

  if (mClear) {
    DeleteFiles(aQuotaManager);

    aQuotaManager.RemoveQuota();
  }

  aQuotaManager.ShutdownStorage();

  if (mClear) {
    DeleteStorageFile(aQuotaManager);
  }

  return NS_OK;
}

void ResetOrClearOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();
  if (mClear) {
    aResponse = ClearAllResponse();
  } else {
    aResponse = ResetAllResponse();
  }
}

void ClearRequestBase::DeleteFiles(QuotaManager& aQuotaManager,
                                   PersistenceType aPersistenceType) {
  AssertIsOnIOThread();

  QM_TRY(aQuotaManager.AboutToClearOrigins(
             Nullable<PersistenceType>(aPersistenceType), mOriginScope,
             mClientType),
         QM_VOID);

  QM_TRY_INSPECT(
      const auto& directory,
      QM_NewLocalFile(aQuotaManager.GetStoragePath(aPersistenceType)), QM_VOID);

  nsTArray<nsCOMPtr<nsIFile>> directoriesForRemovalRetry;

  aQuotaManager.MaybeRecordQuotaManagerShutdownStep(
      "ClearRequestBase: Starting deleting files"_ns);

  QM_TRY(CollectEachFile(
             *directory,
             [originScope =
                  [this] {
                    OriginScope originScope = mOriginScope.Clone();
                    if (originScope.IsOrigin()) {
                      originScope.SetOrigin(
                          MakeSanitizedOriginCString(originScope.GetOrigin()));
                    } else if (originScope.IsPrefix()) {
                      originScope.SetOriginNoSuffix(MakeSanitizedOriginCString(
                          originScope.GetOriginNoSuffix()));
                    }
                    return originScope;
                  }(),
              aPersistenceType, &aQuotaManager, &directoriesForRemovalRetry,
              this](nsCOMPtr<nsIFile>&& file) -> mozilla::Result<Ok, nsresult> {
               QM_TRY_INSPECT(
                   const auto& leafName,
                   MOZ_TO_RESULT_INVOKE_TYPED(nsAutoString, file, GetLeafName));

               QM_TRY_INSPECT(const auto& dirEntryKind, GetDirEntryKind(*file));

               switch (dirEntryKind) {
                 case nsIFileKind::ExistsAsDirectory: {
                   // Skip the origin directory if it doesn't match the pattern.
                   if (!originScope.Matches(OriginScope::FromOrigin(
                           NS_ConvertUTF16toUTF8(leafName)))) {
                     break;
                   }

                   QM_TRY_INSPECT(
                       const auto& metadata,
                       aQuotaManager.LoadFullOriginMetadataWithRestore(file));

                   MOZ_ASSERT(metadata.mPersistenceType == aPersistenceType);

                   if (!mClientType.IsNull()) {
                     nsAutoString clientDirectoryName;
                     QM_TRY(OkIf(Client::TypeToText(mClientType.Value(),
                                                    clientDirectoryName,
                                                    fallible)),
                            Err(NS_ERROR_FAILURE));

                     QM_TRY(file->Append(clientDirectoryName));

                     QM_TRY_INSPECT(const bool& exists,
                                    MOZ_TO_RESULT_INVOKE(file, Exists));

                     if (!exists) {
                       break;
                     }
                   }

                   // We can't guarantee that this will always succeed on
                   // Windows...
                   QM_WARNONLY_TRY(file->Remove(true), [&](const auto&) {
                     directoriesForRemovalRetry.AppendElement(std::move(file));
                   });

                   const bool initialized =
                       aPersistenceType == PERSISTENCE_TYPE_PERSISTENT
                           ? aQuotaManager.IsOriginInitialized(metadata.mOrigin)
                           : aQuotaManager.IsTemporaryStorageInitialized();

                   // If it hasn't been initialized, we don't need to update the
                   // quota and notify the removing client.
                   if (!initialized) {
                     break;
                   }

                   if (aPersistenceType != PERSISTENCE_TYPE_PERSISTENT) {
                     if (mClientType.IsNull()) {
                       aQuotaManager.RemoveQuotaForOrigin(aPersistenceType,
                                                          metadata);
                     } else {
                       aQuotaManager.ResetUsageForClient(
                           ClientMetadata{metadata, mClientType.Value()});
                     }
                   }

                   aQuotaManager.OriginClearCompleted(
                       aPersistenceType, metadata.mOrigin, mClientType);

                   break;
                 }

                 case nsIFileKind::ExistsAsFile:
                   // Unknown files during clearing are allowed. Just warn if we
                   // find them.
                   if (!IsOSMetadata(leafName)) {
                     UNKNOWN_FILE_WARNING(leafName);
                   }

                   break;

                 case nsIFileKind::DoesNotExist:
                   // Ignore files that got removed externally while iterating.
                   break;
               }

               return Ok{};
             }),
         QM_VOID);

  // Retry removing any directories that failed to be removed earlier now.
  //
  // XXX This will still block this operation. We might instead dispatch a
  // runnable to our own thread for each retry round with a timer. We must
  // ensure that the directory lock is upheld until we complete or give up
  // though.
  for (uint32_t index = 0; index < 10; index++) {
    aQuotaManager.MaybeRecordQuotaManagerShutdownStep(
        "ClearRequestBase: Retrying directory removal"_ns);

    for (auto&& file : std::exchange(directoriesForRemovalRetry,
                                     nsTArray<nsCOMPtr<nsIFile>>{})) {
      if (NS_FAILED((file->Remove(true)))) {
        directoriesForRemovalRetry.AppendElement(std::move(file));
      }
    }

    if (directoriesForRemovalRetry.IsEmpty()) {
      break;
    }

    PR_Sleep(PR_MillisecondsToInterval(200));
  }

  if (!directoriesForRemovalRetry.IsEmpty()) {
    NS_WARNING("Failed to remove one or more directories, giving up!");
  }

  aQuotaManager.MaybeRecordQuotaManagerShutdownStep(
      "ClearRequestBase: Completed deleting files"_ns);
}

nsresult ClearRequestBase::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitialized();

  AUTO_PROFILER_LABEL("ClearRequestBase::DoDirectoryWork", OTHER);

  if (mPersistenceType.IsNull()) {
    for (const PersistenceType type : kAllPersistenceTypes) {
      DeleteFiles(aQuotaManager, type);
    }
  } else {
    DeleteFiles(aQuotaManager, mPersistenceType.Value());
  }

  return NS_OK;
}

ClearOriginOp::ClearOriginOp(const RequestParams& aParams)
    : ClearRequestBase(/* aExclusive */ true),
      mParams(aParams.get_ClearOriginParams().commonParams()),
      mMatchAll(aParams.get_ClearOriginParams().matchAll()) {
  MOZ_ASSERT(aParams.type() == RequestParams::TClearOriginParams);
}

void ClearOriginOp::Init(Quota& aQuota) {
  AssertIsOnOwningThread();

  QuotaRequestBase::Init(aQuota);

  if (mParams.persistenceTypeIsExplicit()) {
    mPersistenceType.SetValue(mParams.persistenceType());
  }

  // Figure out which origin we're dealing with.
  const auto origin = QuotaManager::GetOriginFromValidatedPrincipalInfo(
      mParams.principalInfo());

  if (mMatchAll) {
    mOriginScope.SetFromPrefix(origin);
  } else {
    mOriginScope.SetFromOrigin(origin);
  }

  if (mParams.clientTypeIsExplicit()) {
    mClientType.SetValue(mParams.clientType());
  }
}

void ClearOriginOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = ClearOriginResponse();
}

ClearDataOp::ClearDataOp(const RequestParams& aParams)
    : ClearRequestBase(/* aExclusive */ true), mParams(aParams) {
  MOZ_ASSERT(aParams.type() == RequestParams::TClearDataParams);
}

void ClearDataOp::Init(Quota& aQuota) {
  AssertIsOnOwningThread();

  QuotaRequestBase::Init(aQuota);

  mOriginScope.SetFromPattern(mParams.pattern());
}

void ClearDataOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = ClearDataResponse();
}

ResetOriginOp::ResetOriginOp(const RequestParams& aParams)
    : QuotaRequestBase(/* aExclusive */ true) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aParams.type() == RequestParams::TResetOriginParams);

  const ClearResetOriginParams& params =
      aParams.get_ResetOriginParams().commonParams();

  const auto origin =
      QuotaManager::GetOriginFromValidatedPrincipalInfo(params.principalInfo());

  // Overwrite OriginOperationBase default values.
  mNeedsQuotaManagerInit = true;
  mNeedsStorageInit = false;

  // Overwrite NormalOriginOperationBase default values.
  if (params.persistenceTypeIsExplicit()) {
    mPersistenceType.SetValue(params.persistenceType());
  }

  mOriginScope.SetFromOrigin(origin);

  if (params.clientTypeIsExplicit()) {
    mClientType.SetValue(params.clientType());
  }
}

void ResetOriginOp::Init(Quota& aQuota) { AssertIsOnOwningThread(); }

nsresult ResetOriginOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("ResetOriginOp::DoDirectoryWork", OTHER);

  // All the work is handled by NormalOriginOperationBase parent class. In this
  // particular case, we just needed to acquire an exclusive directory lock and
  // that's it.

  return NS_OK;
}

void ResetOriginOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = ResetOriginResponse();
}

PersistRequestBase::PersistRequestBase(const PrincipalInfo& aPrincipalInfo)
    : QuotaRequestBase(/* aExclusive */ false), mPrincipalInfo(aPrincipalInfo) {
  AssertIsOnOwningThread();
}

void PersistRequestBase::Init(Quota& aQuota) {
  AssertIsOnOwningThread();

  QuotaRequestBase::Init(aQuota);

  mPersistenceType.SetValue(PERSISTENCE_TYPE_DEFAULT);

  // Figure out which origin we're dealing with.
  PrincipalMetadata principalMetadata =
      QuotaManager::GetInfoFromValidatedPrincipalInfo(mPrincipalInfo);

  mSuffix = std::move(principalMetadata.mSuffix);
  mGroup = std::move(principalMetadata.mGroup);
  mOriginScope.SetFromOrigin(principalMetadata.mOrigin);
}

PersistedOp::PersistedOp(const RequestParams& aParams)
    : PersistRequestBase(aParams.get_PersistedParams().principalInfo()),
      mPersisted(false) {
  MOZ_ASSERT(aParams.type() == RequestParams::TPersistedParams);
}

nsresult PersistedOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitialized();
  MOZ_ASSERT(!mPersistenceType.IsNull());
  MOZ_ASSERT(mPersistenceType.Value() == PERSISTENCE_TYPE_DEFAULT);
  MOZ_ASSERT(mOriginScope.IsOrigin());

  AUTO_PROFILER_LABEL("PersistedOp::DoDirectoryWork", OTHER);

  Nullable<bool> persisted = aQuotaManager.OriginPersisted(
      OriginMetadata{mSuffix, mGroup, nsCString{mOriginScope.GetOrigin()},
                     mPersistenceType.Value()});

  if (!persisted.IsNull()) {
    mPersisted = persisted.Value();
    return NS_OK;
  }

  // If we get here, it means the origin hasn't been initialized yet.
  // Try to get the persisted flag from directory metadata on disk.

  QM_TRY_INSPECT(const auto& directory,
                 aQuotaManager.GetDirectoryForOrigin(mPersistenceType.Value(),
                                                     mOriginScope.GetOrigin()));

  QM_TRY_INSPECT(const bool& exists, MOZ_TO_RESULT_INVOKE(directory, Exists));

  if (exists) {
    // Get the metadata. We only use the persisted flag.
    QM_TRY_INSPECT(const auto& metadata,
                   aQuotaManager.LoadFullOriginMetadataWithRestore(directory));

    mPersisted = metadata.mPersisted;
  } else {
    // The directory has not been created yet.
    mPersisted = false;
  }

  return NS_OK;
}

void PersistedOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  PersistedResponse persistedResponse;
  persistedResponse.persisted() = mPersisted;

  aResponse = persistedResponse;
}

PersistOp::PersistOp(const RequestParams& aParams)
    : PersistRequestBase(aParams.get_PersistParams().principalInfo()) {
  MOZ_ASSERT(aParams.type() == RequestParams::TPersistParams);
}

nsresult PersistOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitialized();
  MOZ_ASSERT(!mPersistenceType.IsNull());
  MOZ_ASSERT(mPersistenceType.Value() == PERSISTENCE_TYPE_DEFAULT);
  MOZ_ASSERT(mOriginScope.IsOrigin());

  const OriginMetadata originMetadata = {mSuffix, mGroup,
                                         nsCString{mOriginScope.GetOrigin()},
                                         mPersistenceType.Value()};

  AUTO_PROFILER_LABEL("PersistOp::DoDirectoryWork", OTHER);

  // Update directory metadata on disk first. Then, create/update the originInfo
  // if needed.
  QM_TRY_INSPECT(const auto& directory,
                 aQuotaManager.GetDirectoryForOrigin(mPersistenceType.Value(),
                                                     originMetadata.mOrigin));

  QM_TRY_INSPECT(const bool& created,
                 aQuotaManager.EnsureOriginDirectory(*directory));

  if (created) {
    int64_t timestamp;

    // Origin directory has been successfully created.
    // Create OriginInfo too if temporary storage was already initialized.
    if (aQuotaManager.IsTemporaryStorageInitialized()) {
      timestamp = aQuotaManager.NoteOriginDirectoryCreated(
          originMetadata, /* aPersisted */ true);
    } else {
      timestamp = PR_Now();
    }

    QM_TRY(CreateDirectoryMetadata2(*directory, timestamp,
                                    /* aPersisted */ true, originMetadata));
  } else {
    // Get the metadata (restore the metadata file if necessary). We only use
    // the persisted flag.
    QM_TRY_INSPECT(const auto& metadata,
                   aQuotaManager.LoadFullOriginMetadataWithRestore(directory));

    if (!metadata.mPersisted) {
      QM_TRY_INSPECT(const auto& file,
                     CloneFileAndAppend(
                         *directory, nsLiteralString(METADATA_V2_FILE_NAME)));

      QM_TRY_INSPECT(const auto& stream,
                     GetBinaryOutputStream(*file, FileFlag::Update));

      MOZ_ASSERT(stream);

      // Update origin access time while we are here.
      QM_TRY(stream->Write64(PR_Now()));

      // Set the persisted flag to true.
      QM_TRY(stream->WriteBoolean(true));
    }

    // Directory metadata has been successfully updated.
    // Update OriginInfo too if temporary storage was already initialized.
    if (aQuotaManager.IsTemporaryStorageInitialized()) {
      aQuotaManager.PersistOrigin(originMetadata);
    }
  }

  return NS_OK;
}

void PersistOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = PersistResponse();
}

EstimateOp::EstimateOp(const RequestParams& aParams)
    : QuotaRequestBase(/* aExclusive */ false), mUsage(0), mLimit(0) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aParams.type() == RequestParams::TEstimateParams);

  // XXX We don't use the quota info components other than the group here.
  mGroup = std::move(QuotaManager::GetInfoFromValidatedPrincipalInfo(
                         aParams.get_EstimateParams().principalInfo())
                         .mGroup);

  // Overwrite NormalOriginOperationBase default values.
  mNeedsDirectoryLocking = false;

  // Overwrite OriginOperationBase default values.
  mNeedsQuotaManagerInit = true;
  mNeedsStorageInit = true;
}

nsresult EstimateOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitialized();

  AUTO_PROFILER_LABEL("EstimateOp::DoDirectoryWork", OTHER);

  // Ensure temporary storage is initialized. If temporary storage hasn't been
  // initialized yet, the method will initialize it by traversing the
  // repositories for temporary and default storage (including origins belonging
  // to our group).
  QM_TRY(aQuotaManager.EnsureTemporaryStorageIsInitialized());

  // Get cached usage (the method doesn't have to stat any files).
  mUsage = aQuotaManager.GetGroupUsage(mGroup);

  mLimit = aQuotaManager.GetGroupLimit();

  return NS_OK;
}

void EstimateOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  EstimateResponse estimateResponse;

  estimateResponse.usage() = mUsage;
  estimateResponse.limit() = mLimit;

  aResponse = estimateResponse;
}

ListOriginsOp::ListOriginsOp()
    : QuotaRequestBase(/* aExclusive */ false), TraverseRepositoryHelper() {
  AssertIsOnOwningThread();
}

void ListOriginsOp::Init(Quota& aQuota) {
  AssertIsOnOwningThread();

  mNeedsQuotaManagerInit = true;
  mNeedsStorageInit = true;
}

nsresult ListOriginsOp::DoDirectoryWork(QuotaManager& aQuotaManager) {
  AssertIsOnIOThread();
  aQuotaManager.AssertStorageIsInitialized();

  AUTO_PROFILER_LABEL("ListOriginsOp::DoDirectoryWork", OTHER);

  for (const PersistenceType type : kAllPersistenceTypes) {
    QM_TRY(TraverseRepository(aQuotaManager, type));
  }

  // TraverseRepository above only consulted the file-system to get a list of
  // known origins, but we also need to include origins that have pending quota
  // usage.

  aQuotaManager.CollectPendingOriginsForListing([this](const auto& originInfo) {
    mOrigins.AppendElement(originInfo->Origin());
  });

  return NS_OK;
}

const Atomic<bool>& ListOriginsOp::GetIsCanceledFlag() {
  AssertIsOnIOThread();

  return mCanceled;
}

nsresult ListOriginsOp::ProcessOrigin(QuotaManager& aQuotaManager,
                                      nsIFile& aOriginDir,
                                      const bool aPersistent,
                                      const PersistenceType aPersistenceType) {
  AssertIsOnIOThread();

  // XXX We only use metadata.mOriginMetadata.mOrigin...
  QM_TRY_UNWRAP(auto metadata,
                aQuotaManager.LoadFullOriginMetadataWithRestore(&aOriginDir));

  if (aQuotaManager.IsOriginInternal(metadata.mOrigin)) {
    return NS_OK;
  }

  mOrigins.AppendElement(std::move(metadata.mOrigin));

  return NS_OK;
}

void ListOriginsOp::GetResponse(RequestResponse& aResponse) {
  AssertIsOnOwningThread();

  aResponse = ListOriginsResponse();
  if (mOrigins.IsEmpty()) {
    return;
  }

  nsTArray<nsCString>& origins = aResponse.get_ListOriginsResponse().origins();
  mOrigins.SwapElements(origins);
}

#ifdef QM_PRINCIPALINFO_VERIFICATION_ENABLED

// static
already_AddRefed<PrincipalVerifier> PrincipalVerifier::CreateAndDispatch(
    nsTArray<PrincipalInfo>&& aPrincipalInfos) {
  AssertIsOnIOThread();

  RefPtr<PrincipalVerifier> verifier =
      new PrincipalVerifier(std::move(aPrincipalInfos));

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(verifier));

  return verifier.forget();
}

Result<Ok, nsCString> PrincipalVerifier::CheckPrincipalInfoValidity(
    const PrincipalInfo& aPrincipalInfo) {
  MOZ_ASSERT(NS_IsMainThread());

  switch (aPrincipalInfo.type()) {
    // A system principal is acceptable.
    case PrincipalInfo::TSystemPrincipalInfo: {
      return Ok{};
    }

    case PrincipalInfo::TContentPrincipalInfo: {
      const ContentPrincipalInfo& info =
          aPrincipalInfo.get_ContentPrincipalInfo();

      nsCOMPtr<nsIURI> uri;
      nsresult rv = NS_NewURI(getter_AddRefs(uri), info.spec());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return Err("NS_NewURI failed"_ns);
      }

      nsCOMPtr<nsIPrincipal> principal =
          BasePrincipal::CreateContentPrincipal(uri, info.attrs());
      if (NS_WARN_IF(!principal)) {
        return Err("CreateContentPrincipal failed"_ns);
      }

      nsCString originNoSuffix;
      rv = principal->GetOriginNoSuffix(originNoSuffix);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return Err("GetOriginNoSuffix failed"_ns);
      }

      if (NS_WARN_IF(originNoSuffix != info.originNoSuffix())) {
        static const char messageTemplate[] =
            "originNoSuffix (%s) doesn't match passed one (%s)!";

        QM_WARNING(messageTemplate, originNoSuffix.get(),
                   info.originNoSuffix().get());

        return Err(nsPrintfCString(
            messageTemplate, AnonymizedOriginString(originNoSuffix).get(),
            AnonymizedOriginString(info.originNoSuffix()).get()));
      }

      nsCString baseDomain;
      rv = principal->GetBaseDomain(baseDomain);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return Err("GetBaseDomain failed"_ns);
      }

      if (NS_WARN_IF(baseDomain != info.baseDomain())) {
        static const char messageTemplate[] =
            "baseDomain (%s) doesn't match passed one (%s)!";

        QM_WARNING(messageTemplate, baseDomain.get(), info.baseDomain().get());

        return Err(nsPrintfCString(messageTemplate,
                                   AnonymizedCString(baseDomain).get(),
                                   AnonymizedCString(info.baseDomain()).get()));
      }

      return Ok{};
    }

    default: {
      break;
    }
  }

  return Err("Null and expanded principals are not acceptable"_ns);
}

NS_IMETHODIMP
PrincipalVerifier::Run() {
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoCString allDetails;
  for (auto& principalInfo : mPrincipalInfos) {
    const auto res = CheckPrincipalInfoValidity(principalInfo);
    if (res.isErr()) {
      if (!allDetails.IsEmpty()) {
        allDetails.AppendLiteral(", ");
      }

      allDetails.Append(res.inspectErr());
    }
  }

  if (!allDetails.IsEmpty()) {
    allDetails.Insert("Invalid principal infos found: ", 0);

    // In case of invalid principal infos, this will produce a crash reason such
    // as:
    //   Invalid principal infos found: originNoSuffix (https://aaa.aaaaaaa.aaa)
    //   doesn't match passed one (about:aaaa)!
    //
    // In case of errors while validating a principal, it will contain a
    // different message describing that error, which does not contain any
    // details of the actual principal info at the moment.
    //
    // This string will be leaked.
    MOZ_CRASH_UNSAFE(strdup(allDetails.BeginReading()));
  }

  return NS_OK;
}

#endif

nsresult StorageOperationBase::GetDirectoryMetadata(nsIFile* aDirectory,
                                                    int64_t& aTimestamp,
                                                    nsACString& aGroup,
                                                    nsACString& aOrigin,
                                                    Nullable<bool>& aIsApp) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);

  QM_TRY_INSPECT(
      const auto& binaryStream,
      GetBinaryInputStream(*aDirectory, nsLiteralString(METADATA_FILE_NAME)));

  QM_TRY_INSPECT(const uint64_t& timestamp,
                 MOZ_TO_RESULT_INVOKE(binaryStream, Read64));

  QM_TRY_INSPECT(const auto& group, MOZ_TO_RESULT_INVOKE_TYPED(
                                        nsCString, binaryStream, ReadCString));

  QM_TRY_INSPECT(const auto& origin, MOZ_TO_RESULT_INVOKE_TYPED(
                                         nsCString, binaryStream, ReadCString));

  Nullable<bool> isApp;
  bool value;
  if (NS_SUCCEEDED(binaryStream->ReadBoolean(&value))) {
    isApp.SetValue(value);
  }

  aTimestamp = timestamp;
  aGroup = group;
  aOrigin = origin;
  aIsApp = std::move(isApp);
  return NS_OK;
}

nsresult StorageOperationBase::GetDirectoryMetadata2(
    nsIFile* aDirectory, int64_t& aTimestamp, nsACString& aSuffix,
    nsACString& aGroup, nsACString& aOrigin, bool& aIsApp) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);

  QM_TRY_INSPECT(const auto& binaryStream,
                 GetBinaryInputStream(*aDirectory,
                                      nsLiteralString(METADATA_V2_FILE_NAME)));

  QM_TRY_INSPECT(const uint64_t& timestamp,
                 MOZ_TO_RESULT_INVOKE(binaryStream, Read64));

  QM_TRY_INSPECT(const bool& persisted,
                 MOZ_TO_RESULT_INVOKE(binaryStream, ReadBoolean));
  Unused << persisted;

  QM_TRY_INSPECT(const bool& reservedData1,
                 MOZ_TO_RESULT_INVOKE(binaryStream, Read32));
  Unused << reservedData1;

  QM_TRY_INSPECT(const bool& reservedData2,
                 MOZ_TO_RESULT_INVOKE(binaryStream, Read32));
  Unused << reservedData2;

  QM_TRY_INSPECT(const auto& suffix, MOZ_TO_RESULT_INVOKE_TYPED(
                                         nsCString, binaryStream, ReadCString));

  QM_TRY_INSPECT(const auto& group, MOZ_TO_RESULT_INVOKE_TYPED(
                                        nsCString, binaryStream, ReadCString));

  QM_TRY_INSPECT(const auto& origin, MOZ_TO_RESULT_INVOKE_TYPED(
                                         nsCString, binaryStream, ReadCString));

  QM_TRY_INSPECT(const bool& isApp,
                 MOZ_TO_RESULT_INVOKE(binaryStream, ReadBoolean));

  aTimestamp = timestamp;
  aSuffix = suffix;
  aGroup = group;
  aOrigin = origin;
  aIsApp = isApp;
  return NS_OK;
}

int64_t StorageOperationBase::GetOriginLastModifiedTime(
    const OriginProps& aOriginProps) {
  return GetLastModifiedTime(*aOriginProps.mPersistenceType,
                             *aOriginProps.mDirectory);
}

nsresult StorageOperationBase::RemoveObsoleteOrigin(
    const OriginProps& aOriginProps) {
  AssertIsOnIOThread();

  QM_WARNING(
      "Deleting obsolete %s directory that is no longer a legal "
      "origin!",
      NS_ConvertUTF16toUTF8(aOriginProps.mLeafName).get());

  QM_TRY(aOriginProps.mDirectory->Remove(/* recursive */ true));

  return NS_OK;
}

Result<bool, nsresult> StorageOperationBase::MaybeRenameOrigin(
    const OriginProps& aOriginProps) {
  AssertIsOnIOThread();

  const nsAString& oldLeafName = aOriginProps.mLeafName;

  const auto newLeafName =
      MakeSanitizedOriginString(aOriginProps.mOriginMetadata.mOrigin);

  if (oldLeafName == newLeafName) {
    return false;
  }

  QM_TRY(CreateDirectoryMetadata(*aOriginProps.mDirectory,
                                 aOriginProps.mTimestamp,
                                 aOriginProps.mOriginMetadata));

  QM_TRY(CreateDirectoryMetadata2(
      *aOriginProps.mDirectory, aOriginProps.mTimestamp,
      /* aPersisted */ false, aOriginProps.mOriginMetadata));

  QM_TRY_INSPECT(const auto& newFile,
                 MOZ_TO_RESULT_INVOKE_TYPED(
                     nsCOMPtr<nsIFile>, *aOriginProps.mDirectory, GetParent));

  QM_TRY(newFile->Append(newLeafName));

  QM_TRY_INSPECT(const bool& exists, MOZ_TO_RESULT_INVOKE(newFile, Exists));

  if (exists) {
    QM_WARNING(
        "Can't rename %s directory to %s, the target already exists, removing "
        "instead of renaming!",
        NS_ConvertUTF16toUTF8(oldLeafName).get(),
        NS_ConvertUTF16toUTF8(newLeafName).get());
  }

  QM_TRY(CallWithDelayedRetriesIfAccessDenied(
      [&exists, &aOriginProps, &newLeafName] {
        if (exists) {
          QM_TRY_RETURN(aOriginProps.mDirectory->Remove(/* recursive */ true));
        }
        QM_TRY_RETURN(aOriginProps.mDirectory->RenameTo(nullptr, newLeafName));
      },
      StaticPrefs::dom_quotaManager_directoryRemovalOrRenaming_maxRetries(),
      StaticPrefs::dom_quotaManager_directoryRemovalOrRenaming_delayMs()));

  return true;
}

nsresult StorageOperationBase::ProcessOriginDirectories() {
  AssertIsOnIOThread();
  MOZ_ASSERT(!mOriginProps.IsEmpty());

#ifdef QM_PRINCIPALINFO_VERIFICATION_ENABLED
  nsTArray<PrincipalInfo> principalInfos;
#endif

  for (auto& originProps : mOriginProps) {
    switch (originProps.mType) {
      case OriginProps::eChrome: {
        originProps.mOriginMetadata = {QuotaManager::GetInfoForChrome(),
                                       *originProps.mPersistenceType};
        break;
      }

      case OriginProps::eContent: {
        RefPtr<MozURL> specURL;
        nsresult rv = MozURL::Init(getter_AddRefs(specURL), originProps.mSpec);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          // If a URL cannot be understood by MozURL during restoring or
          // upgrading, either marking the directory as broken or removing that
          // corresponding directory should be considered. While the cost of
          // marking the directory as broken during a upgrade is too high,
          // removing the directory is a better choice rather than blocking the
          // initialization or the upgrade.
          QM_WARNING(
              "A URL (%s) for the origin directory is not recognized by "
              "MozURL. The directory will be deleted for now to pass the "
              "initialization or the upgrade.",
              originProps.mSpec.get());

          originProps.mType = OriginProps::eObsolete;
          break;
        }

        nsCString originNoSuffix;
        specURL->Origin(originNoSuffix);

        QM_TRY_INSPECT(
            const auto& baseDomain,
            MOZ_TO_RESULT_INVOKE_TYPED(nsCString, specURL, BaseDomain));

        ContentPrincipalInfo contentPrincipalInfo;
        contentPrincipalInfo.attrs() = originProps.mAttrs;
        contentPrincipalInfo.originNoSuffix() = originNoSuffix;
        contentPrincipalInfo.spec() = originProps.mSpec;
        contentPrincipalInfo.baseDomain() = baseDomain;

        PrincipalInfo principalInfo(contentPrincipalInfo);

        originProps.mOriginMetadata = {
            QuotaManager::GetInfoFromValidatedPrincipalInfo(principalInfo),
            *originProps.mPersistenceType};

#ifdef QM_PRINCIPALINFO_VERIFICATION_ENABLED
        principalInfos.AppendElement(principalInfo);
#endif

        break;
      }

      case OriginProps::eObsolete: {
        // There's no way to get info for obsolete origins.
        break;
      }

      default:
        MOZ_CRASH("Bad type!");
    }
  }

#ifdef QM_PRINCIPALINFO_VERIFICATION_ENABLED
  if (!principalInfos.IsEmpty()) {
    RefPtr<PrincipalVerifier> principalVerifier =
        PrincipalVerifier::CreateAndDispatch(std::move(principalInfos));
  }
#endif

  // Don't try to upgrade obsolete origins, remove them right after we detect
  // them.
  for (const auto& originProps : mOriginProps) {
    if (originProps.mType == OriginProps::eObsolete) {
      MOZ_ASSERT(originProps.mOriginMetadata.mSuffix.IsEmpty());
      MOZ_ASSERT(originProps.mOriginMetadata.mGroup.IsEmpty());
      MOZ_ASSERT(originProps.mOriginMetadata.mOrigin.IsEmpty());

      QM_TRY(RemoveObsoleteOrigin(originProps));
    } else {
      MOZ_ASSERT(!originProps.mOriginMetadata.mGroup.IsEmpty());
      MOZ_ASSERT(!originProps.mOriginMetadata.mOrigin.IsEmpty());

      QM_TRY(ProcessOriginDirectory(originProps));
    }
  }

  return NS_OK;
}

// XXX Do the fallible initialization in a separate non-static member function
// of StorageOperationBase and eventually get rid of this method and use a
// normal constructor instead.
template <typename PersistenceTypeFunc>
nsresult StorageOperationBase::OriginProps::Init(
    PersistenceTypeFunc&& aPersistenceTypeFunc) {
  AssertIsOnIOThread();

  QM_TRY_INSPECT(
      const auto& leafName,
      MOZ_TO_RESULT_INVOKE_TYPED(nsAutoString, *mDirectory, GetLeafName));

  nsCString spec;
  OriginAttributes attrs;
  nsCString originalSuffix;
  OriginParser::ResultType result = OriginParser::ParseOrigin(
      NS_ConvertUTF16toUTF8(leafName), spec, &attrs, originalSuffix);
  if (NS_WARN_IF(result == OriginParser::InvalidOrigin)) {
    mType = OriginProps::eInvalid;
    return NS_OK;
  }

  const auto persistenceType = [&]() -> PersistenceType {
    // XXX We shouldn't continue with initialization if OriginParser returned
    // anything else but ValidOrigin. Otherwise, we have to deal with empty
    // spec when the origin is obsolete, like here. The caller should handle
    // the errors. Until it's fixed, we have to treat obsolete origins as
    // origins with unknown/invalid persistence type.
    if (result != OriginParser::ValidOrigin) {
      return PERSISTENCE_TYPE_INVALID;
    }
    return std::forward<PersistenceTypeFunc>(aPersistenceTypeFunc)(spec);
  }();

  mLeafName = leafName;
  mSpec = spec;
  mAttrs = attrs;
  mOriginalSuffix = originalSuffix;
  mPersistenceType.init(persistenceType);
  if (result == OriginParser::ObsoleteOrigin) {
    mType = eObsolete;
  } else if (mSpec.EqualsLiteral(kChromeOrigin)) {
    mType = eChrome;
  } else {
    mType = eContent;
  }

  return NS_OK;
}

// static
auto OriginParser::ParseOrigin(const nsACString& aOrigin, nsCString& aSpec,
                               OriginAttributes* aAttrs,
                               nsCString& aOriginalSuffix) -> ResultType {
  MOZ_ASSERT(!aOrigin.IsEmpty());
  MOZ_ASSERT(aAttrs);

  nsCString origin(aOrigin);
  int32_t pos = origin.RFindChar('^');

  if (pos == kNotFound) {
    aOriginalSuffix.Truncate();
  } else {
    aOriginalSuffix = Substring(origin, pos);
  }

  OriginAttributes originAttributes;

  nsCString originNoSuffix;
  bool ok = originAttributes.PopulateFromOrigin(aOrigin, originNoSuffix);
  if (!ok) {
    return InvalidOrigin;
  }

  OriginParser parser(originNoSuffix);

  *aAttrs = originAttributes;
  return parser.Parse(aSpec);
}

auto OriginParser::Parse(nsACString& aSpec) -> ResultType {
  while (mTokenizer.hasMoreTokens()) {
    const nsDependentCSubstring& token = mTokenizer.nextToken();

    HandleToken(token);

    if (mError) {
      break;
    }

    if (!mHandledTokens.IsEmpty()) {
      mHandledTokens.AppendLiteral(", ");
    }
    mHandledTokens.Append('\'');
    mHandledTokens.Append(token);
    mHandledTokens.Append('\'');
  }

  if (!mError && mTokenizer.separatorAfterCurrentToken()) {
    HandleTrailingSeparator();
  }

  if (mError) {
    QM_WARNING("Origin '%s' failed to parse, handled tokens: %s", mOrigin.get(),
               mHandledTokens.get());

    return (mSchemeType == eChrome || mSchemeType == eAbout) ? ObsoleteOrigin
                                                             : InvalidOrigin;
  }

  MOZ_ASSERT(mState == eComplete || mState == eHandledTrailingSeparator);

  // For IPv6 URL, it should at least have three groups.
  MOZ_ASSERT_IF(mIPGroup > 0, mIPGroup >= 3);

  nsAutoCString spec(mScheme);

  if (mSchemeType == eFile) {
    spec.AppendLiteral("://");

    if (mUniversalFileOrigin) {
      MOZ_ASSERT(mPathnameComponents.Length() == 1);

      spec.Append(mPathnameComponents[0]);
    } else {
      for (uint32_t count = mPathnameComponents.Length(), index = 0;
           index < count; index++) {
        spec.Append('/');
        spec.Append(mPathnameComponents[index]);
      }
    }

    aSpec = spec;

    return ValidOrigin;
  }

  if (mSchemeType == eAbout) {
    if (mMaybeObsolete) {
      // The "moz-safe-about+++home" was acciedntally created by a buggy nightly
      // and can be safely removed.
      return mHost.EqualsLiteral("home") ? ObsoleteOrigin : InvalidOrigin;
    }
    spec.Append(':');
  } else if (mSchemeType != eChrome) {
    spec.AppendLiteral("://");
  }

  spec.Append(mHost);

  if (!mPort.IsNull()) {
    spec.Append(':');
    spec.AppendInt(mPort.Value());
  }

  aSpec = spec;

  return mScheme.EqualsLiteral("app") ? ObsoleteOrigin : ValidOrigin;
}

void OriginParser::HandleScheme(const nsDependentCSubstring& aToken) {
  MOZ_ASSERT(!aToken.IsEmpty());
  MOZ_ASSERT(mState == eExpectingAppIdOrScheme || mState == eExpectingScheme);

  bool isAbout = false;
  bool isMozSafeAbout = false;
  bool isFile = false;
  bool isChrome = false;
  if (aToken.EqualsLiteral("http") || aToken.EqualsLiteral("https") ||
      (isAbout = aToken.EqualsLiteral("about") ||
                 (isMozSafeAbout = aToken.EqualsLiteral("moz-safe-about"))) ||
      aToken.EqualsLiteral("indexeddb") ||
      (isFile = aToken.EqualsLiteral("file")) || aToken.EqualsLiteral("app") ||
      aToken.EqualsLiteral("resource") ||
      aToken.EqualsLiteral("moz-extension") ||
      (isChrome = aToken.EqualsLiteral(kChromeOrigin))) {
    mScheme = aToken;

    if (isAbout) {
      mSchemeType = eAbout;
      mState = isMozSafeAbout ? eExpectingEmptyToken1OrHost : eExpectingHost;
    } else if (isChrome) {
      mSchemeType = eChrome;
      if (mTokenizer.hasMoreTokens()) {
        mError = true;
      }
      mState = eComplete;
    } else {
      if (isFile) {
        mSchemeType = eFile;
      }
      mState = eExpectingEmptyToken1;
    }

    return;
  }

  QM_WARNING("'%s' is not a valid scheme!", nsCString(aToken).get());

  mError = true;
}

void OriginParser::HandlePathnameComponent(
    const nsDependentCSubstring& aToken) {
  MOZ_ASSERT(!aToken.IsEmpty());
  MOZ_ASSERT(mState == eExpectingEmptyTokenOrDriveLetterOrPathnameComponent ||
             mState == eExpectingEmptyTokenOrPathnameComponent);
  MOZ_ASSERT(mSchemeType == eFile);

  mPathnameComponents.AppendElement(aToken);

  mState = mTokenizer.hasMoreTokens() ? eExpectingEmptyTokenOrPathnameComponent
                                      : eComplete;
}

void OriginParser::HandleToken(const nsDependentCSubstring& aToken) {
  switch (mState) {
    case eExpectingAppIdOrScheme: {
      if (aToken.IsEmpty()) {
        QM_WARNING("Expected an app id or scheme (not an empty string)!");

        mError = true;
        return;
      }

      if (IsAsciiDigit(aToken.First())) {
        // nsDependentCSubstring doesn't provice ToInteger()
        nsCString token(aToken);

        nsresult rv;
        Unused << token.ToInteger(&rv);
        if (NS_SUCCEEDED(rv)) {
          mState = eExpectingInMozBrowser;
          return;
        }
      }

      HandleScheme(aToken);

      return;
    }

    case eExpectingInMozBrowser: {
      if (aToken.Length() != 1) {
        QM_WARNING("'%d' is not a valid length for the inMozBrowser flag!",
                   aToken.Length());

        mError = true;
        return;
      }

      if (aToken.First() == 't') {
        mInIsolatedMozBrowser = true;
      } else if (aToken.First() == 'f') {
        mInIsolatedMozBrowser = false;
      } else {
        QM_WARNING("'%s' is not a valid value for the inMozBrowser flag!",
                   nsCString(aToken).get());

        mError = true;
        return;
      }

      mState = eExpectingScheme;

      return;
    }

    case eExpectingScheme: {
      if (aToken.IsEmpty()) {
        QM_WARNING("Expected a scheme (not an empty string)!");

        mError = true;
        return;
      }

      HandleScheme(aToken);

      return;
    }

    case eExpectingEmptyToken1: {
      if (!aToken.IsEmpty()) {
        QM_WARNING("Expected the first empty token!");

        mError = true;
        return;
      }

      mState = eExpectingEmptyToken2;

      return;
    }

    case eExpectingEmptyToken2: {
      if (!aToken.IsEmpty()) {
        QM_WARNING("Expected the second empty token!");

        mError = true;
        return;
      }

      if (mSchemeType == eFile) {
        mState = eExpectingEmptyTokenOrUniversalFileOrigin;
      } else {
        if (mSchemeType == eAbout) {
          mMaybeObsolete = true;
        }
        mState = eExpectingHost;
      }

      return;
    }

    case eExpectingEmptyTokenOrUniversalFileOrigin: {
      MOZ_ASSERT(mSchemeType == eFile);

      if (aToken.IsEmpty()) {
        mState = mTokenizer.hasMoreTokens()
                     ? eExpectingEmptyTokenOrDriveLetterOrPathnameComponent
                     : eComplete;

        return;
      }

      if (aToken.EqualsLiteral("UNIVERSAL_FILE_URI_ORIGIN")) {
        mUniversalFileOrigin = true;

        mPathnameComponents.AppendElement(aToken);

        mState = eComplete;

        return;
      }

      QM_WARNING(
          "Expected the third empty token or "
          "UNIVERSAL_FILE_URI_ORIGIN!");

      mError = true;
      return;
    }

    case eExpectingHost: {
      if (aToken.IsEmpty()) {
        QM_WARNING("Expected a host (not an empty string)!");

        mError = true;
        return;
      }

      mHost = aToken;

      if (aToken.First() == '[') {
        MOZ_ASSERT(mIPGroup == 0);

        ++mIPGroup;
        mState = eExpectingIPV6Token;

        MOZ_ASSERT(mTokenizer.hasMoreTokens());
        return;
      }

      if (mTokenizer.hasMoreTokens()) {
        if (mSchemeType == eAbout) {
          QM_WARNING("Expected an empty string after host!");

          mError = true;
          return;
        }

        mState = eExpectingPort;

        return;
      }

      mState = eComplete;

      return;
    }

    case eExpectingPort: {
      MOZ_ASSERT(mSchemeType == eNone);

      if (aToken.IsEmpty()) {
        QM_WARNING("Expected a port (not an empty string)!");

        mError = true;
        return;
      }

      // nsDependentCSubstring doesn't provice ToInteger()
      nsCString token(aToken);

      nsresult rv;
      uint32_t port = token.ToInteger(&rv);
      if (NS_SUCCEEDED(rv)) {
        mPort.SetValue() = port;
      } else {
        QM_WARNING("'%s' is not a valid port number!", token.get());

        mError = true;
        return;
      }

      mState = eComplete;

      return;
    }

    case eExpectingEmptyTokenOrDriveLetterOrPathnameComponent: {
      MOZ_ASSERT(mSchemeType == eFile);

      if (aToken.IsEmpty()) {
        mPathnameComponents.AppendElement(""_ns);

        mState = mTokenizer.hasMoreTokens()
                     ? eExpectingEmptyTokenOrPathnameComponent
                     : eComplete;

        return;
      }

      if (aToken.Length() == 1 && IsAsciiAlpha(aToken.First())) {
        mMaybeDriveLetter = true;

        mPathnameComponents.AppendElement(aToken);

        mState = mTokenizer.hasMoreTokens()
                     ? eExpectingEmptyTokenOrPathnameComponent
                     : eComplete;

        return;
      }

      HandlePathnameComponent(aToken);

      return;
    }

    case eExpectingEmptyTokenOrPathnameComponent: {
      MOZ_ASSERT(mSchemeType == eFile);

      if (aToken.IsEmpty()) {
        if (mMaybeDriveLetter) {
          MOZ_ASSERT(mPathnameComponents.Length() == 1);

          nsCString& pathnameComponent = mPathnameComponents[0];
          pathnameComponent.Append(':');

          mMaybeDriveLetter = false;
        } else {
          mPathnameComponents.AppendElement(""_ns);
        }

        mState = mTokenizer.hasMoreTokens()
                     ? eExpectingEmptyTokenOrPathnameComponent
                     : eComplete;

        return;
      }

      HandlePathnameComponent(aToken);

      return;
    }

    case eExpectingEmptyToken1OrHost: {
      MOZ_ASSERT(mSchemeType == eAbout &&
                 mScheme.EqualsLiteral("moz-safe-about"));

      if (aToken.IsEmpty()) {
        mState = eExpectingEmptyToken2;
      } else {
        mHost = aToken;
        mState = mTokenizer.hasMoreTokens() ? eExpectingPort : eComplete;
      }

      return;
    }

    case eExpectingIPV6Token: {
      // A safe check for preventing infinity recursion.
      if (++mIPGroup > 8) {
        mError = true;
        return;
      }

      mHost.AppendLiteral(":");
      mHost.Append(aToken);
      if (!aToken.IsEmpty() && aToken.Last() == ']') {
        mState = mTokenizer.hasMoreTokens() ? eExpectingPort : eComplete;
      }

      return;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }
}

void OriginParser::HandleTrailingSeparator() {
  MOZ_ASSERT(mState == eComplete);
  MOZ_ASSERT(mSchemeType == eFile);

  mPathnameComponents.AppendElement(""_ns);

  mState = eHandledTrailingSeparator;
}

nsresult RepositoryOperationBase::ProcessRepository() {
  AssertIsOnIOThread();

#ifdef DEBUG
  {
    QM_TRY_INSPECT(const bool& exists, MOZ_TO_RESULT_INVOKE(mDirectory, Exists),
                   QM_ASSERT_UNREACHABLE);
    MOZ_ASSERT(exists);
  }
#endif

  QM_TRY(CollectEachFileEntry(
      *mDirectory,
      [](const auto& originFile) -> Result<mozilla::Ok, nsresult> {
        QM_TRY_INSPECT(
            const auto& leafName,
            MOZ_TO_RESULT_INVOKE_TYPED(nsAutoString, originFile, GetLeafName));

        // Unknown files during upgrade are allowed. Just warn if we find
        // them.
        if (!IsOSMetadata(leafName)) {
          UNKNOWN_FILE_WARNING(leafName);
        }

        return mozilla::Ok{};
      },
      [&self = *this](const auto& originDir) -> Result<mozilla::Ok, nsresult> {
        OriginProps originProps(WrapMovingNotNullUnchecked(originDir));
        QM_TRY(originProps.Init([&self](const auto& aSpec) {
          return self.PersistenceTypeFromSpec(aSpec);
        }));
        // Bypass invalid origins while upgrading
        QM_TRY(OkIf(originProps.mType != OriginProps::eInvalid), mozilla::Ok{});

        if (originProps.mType != OriginProps::eObsolete) {
          QM_TRY_INSPECT(
              const bool& removed,
              MOZ_TO_RESULT_INVOKE(self, PrepareOriginDirectory, originProps));
          if (removed) {
            return mozilla::Ok{};
          }
        }

        self.mOriginProps.AppendElement(std::move(originProps));

        return mozilla::Ok{};
      }));

  if (mOriginProps.IsEmpty()) {
    return NS_OK;
  }

  QM_TRY(ProcessOriginDirectories());

  return NS_OK;
}

template <typename UpgradeMethod>
nsresult RepositoryOperationBase::MaybeUpgradeClients(
    const OriginProps& aOriginProps, UpgradeMethod aMethod) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aMethod);

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  QM_TRY(CollectEachFileEntry(
      *aOriginProps.mDirectory,
      [](const auto& file) -> Result<mozilla::Ok, nsresult> {
        QM_TRY_INSPECT(
            const auto& leafName,
            MOZ_TO_RESULT_INVOKE_TYPED(nsAutoString, file, GetLeafName));

        if (!IsOriginMetadata(leafName) && !IsTempMetadata(leafName)) {
          UNKNOWN_FILE_WARNING(leafName);
        }

        return mozilla::Ok{};
      },
      [quotaManager, &aMethod,
       &self = *this](const auto& dir) -> Result<mozilla::Ok, nsresult> {
        QM_TRY_INSPECT(
            const auto& leafName,
            MOZ_TO_RESULT_INVOKE_TYPED(nsAutoString, dir, GetLeafName));

        QM_TRY_INSPECT(
            const bool& removed,
            MOZ_TO_RESULT_INVOKE(self, PrepareClientDirectory, dir, leafName));
        if (removed) {
          return mozilla::Ok{};
        }

        Client::Type clientType;
        bool ok = Client::TypeFromText(leafName, clientType, fallible);
        if (!ok) {
          UNKNOWN_FILE_WARNING(leafName);
          return mozilla::Ok{};
        }

        Client* client = quotaManager->GetClient(clientType);
        MOZ_ASSERT(client);

        QM_TRY((client->*aMethod)(dir));

        return mozilla::Ok{};
      }));

  return NS_OK;
}

nsresult RepositoryOperationBase::PrepareClientDirectory(
    nsIFile* aFile, const nsAString& aLeafName, bool& aRemoved) {
  AssertIsOnIOThread();

  aRemoved = false;
  return NS_OK;
}

nsresult CreateOrUpgradeDirectoryMetadataHelper::Init() {
  AssertIsOnIOThread();
  MOZ_ASSERT(mDirectory);

  const auto maybeLegacyPersistenceType =
      LegacyPersistenceTypeFromFile(*mDirectory, fallible);
  QM_TRY(OkIf(maybeLegacyPersistenceType.isSome()), Err(NS_ERROR_FAILURE));

  mLegacyPersistenceType.init(maybeLegacyPersistenceType.value());

  return NS_OK;
}

Maybe<CreateOrUpgradeDirectoryMetadataHelper::LegacyPersistenceType>
CreateOrUpgradeDirectoryMetadataHelper::LegacyPersistenceTypeFromFile(
    nsIFile& aFile, const fallible_t&) {
  nsAutoString leafName;
  MOZ_ALWAYS_SUCCEEDS(aFile.GetLeafName(leafName));

  if (leafName.Equals(u"persistent"_ns)) {
    return Some(LegacyPersistenceType::Persistent);
  }

  if (leafName.Equals(u"temporary"_ns)) {
    return Some(LegacyPersistenceType::Temporary);
  }

  return Nothing();
}

PersistenceType
CreateOrUpgradeDirectoryMetadataHelper::PersistenceTypeFromLegacyPersistentSpec(
    const nsCString& aSpec) {
  if (QuotaManager::IsOriginInternal(aSpec)) {
    return PERSISTENCE_TYPE_PERSISTENT;
  }

  return PERSISTENCE_TYPE_DEFAULT;
}

PersistenceType CreateOrUpgradeDirectoryMetadataHelper::PersistenceTypeFromSpec(
    const nsCString& aSpec) {
  switch (*mLegacyPersistenceType) {
    case LegacyPersistenceType::Persistent:
      return PersistenceTypeFromLegacyPersistentSpec(aSpec);
    case LegacyPersistenceType::Temporary:
      return PERSISTENCE_TYPE_TEMPORARY;
  }
  MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Bad legacy persistence type value!");
}

nsresult CreateOrUpgradeDirectoryMetadataHelper::MaybeUpgradeOriginDirectory(
    nsIFile* aDirectory) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);

  QM_TRY_INSPECT(
      const auto& metadataFile,
      CloneFileAndAppend(*aDirectory, nsLiteralString(METADATA_FILE_NAME)));

  QM_TRY_INSPECT(const bool& exists,
                 MOZ_TO_RESULT_INVOKE(metadataFile, Exists));

  if (!exists) {
    // Directory structure upgrade needed.
    // Move all files to IDB specific directory.

    nsString idbDirectoryName;
    QM_TRY(OkIf(Client::TypeToText(Client::IDB, idbDirectoryName, fallible)),
           NS_ERROR_FAILURE);

    QM_TRY_INSPECT(const auto& idbDirectory,
                   CloneFileAndAppend(*aDirectory, idbDirectoryName));

    // Usually we only use QM_OR_ELSE_LOG_VERBOSE/QM_OR_ELSE_LOG_VERBOSE_IF
    // with Create and NS_ERROR_FILE_ALREADY_EXISTS check, but typically the
    // idb directory shouldn't exist during the upgrade and the upgrade runs
    // only once in most of the cases, so the use of QM_OR_ELSE_WARN_IF is ok
    // here.
    QM_TRY(QM_OR_ELSE_WARN_IF(
        // Expression.
        ToResult(idbDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755)),
        // Predicate.
        IsSpecificError<NS_ERROR_FILE_ALREADY_EXISTS>,
        // Fallback.
        ([&idbDirectory](const nsresult rv) -> Result<Ok, nsresult> {
          QM_TRY_INSPECT(const bool& isDirectory,
                         MOZ_TO_RESULT_INVOKE(idbDirectory, IsDirectory));

          QM_TRY(OkIf(isDirectory), Err(NS_ERROR_UNEXPECTED));

          return Ok{};
        })));

    QM_TRY(CollectEachFile(
        *aDirectory,
        [&idbDirectory, &idbDirectoryName](
            const nsCOMPtr<nsIFile>& file) -> Result<Ok, nsresult> {
          QM_TRY_INSPECT(
              const auto& leafName,
              MOZ_TO_RESULT_INVOKE_TYPED(nsAutoString, file, GetLeafName));

          if (!leafName.Equals(idbDirectoryName)) {
            QM_TRY(file->MoveTo(idbDirectory, u""_ns));
          }

          return Ok{};
        }));

    QM_TRY(metadataFile->Create(nsIFile::NORMAL_FILE_TYPE, 0644));
  }

  return NS_OK;
}

nsresult CreateOrUpgradeDirectoryMetadataHelper::PrepareOriginDirectory(
    OriginProps& aOriginProps, bool* aRemoved) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aRemoved);

  if (*mLegacyPersistenceType == LegacyPersistenceType::Persistent) {
    QM_TRY(MaybeUpgradeOriginDirectory(aOriginProps.mDirectory.get()));

    aOriginProps.mTimestamp = GetOriginLastModifiedTime(aOriginProps);
  } else {
    int64_t timestamp;
    nsCString group;
    nsCString origin;
    Nullable<bool> isApp;

    QM_WARNONLY_TRY_UNWRAP(
        const auto maybeDirectoryMetadata,
        ToResult(GetDirectoryMetadata(aOriginProps.mDirectory.get(), timestamp,
                                      group, origin, isApp)));
    if (!maybeDirectoryMetadata) {
      aOriginProps.mTimestamp = GetOriginLastModifiedTime(aOriginProps);
      aOriginProps.mNeedsRestore = true;
    } else if (!isApp.IsNull()) {
      aOriginProps.mIgnore = true;
    }
  }

  *aRemoved = false;
  return NS_OK;
}

nsresult CreateOrUpgradeDirectoryMetadataHelper::ProcessOriginDirectory(
    const OriginProps& aOriginProps) {
  AssertIsOnIOThread();

  if (*mLegacyPersistenceType == LegacyPersistenceType::Persistent) {
    QM_TRY(CreateDirectoryMetadata(*aOriginProps.mDirectory,
                                   aOriginProps.mTimestamp,
                                   aOriginProps.mOriginMetadata));

    // Move internal origins to new persistent storage.
    if (PersistenceTypeFromLegacyPersistentSpec(aOriginProps.mSpec) ==
        PERSISTENCE_TYPE_PERSISTENT) {
      if (!mPermanentStorageDir) {
        QuotaManager* quotaManager = QuotaManager::Get();
        MOZ_ASSERT(quotaManager);

        const nsString& permanentStoragePath =
            quotaManager->GetStoragePath(PERSISTENCE_TYPE_PERSISTENT);

        QM_TRY_UNWRAP(mPermanentStorageDir,
                      QM_NewLocalFile(permanentStoragePath));
      }

      const nsAString& leafName = aOriginProps.mLeafName;

      QM_TRY_INSPECT(const auto& newDirectory,
                     CloneFileAndAppend(*mPermanentStorageDir, leafName));

      QM_TRY_INSPECT(const bool& exists,
                     MOZ_TO_RESULT_INVOKE(newDirectory, Exists));

      if (exists) {
        QM_WARNING("Found %s in storage/persistent and storage/permanent !",
                   NS_ConvertUTF16toUTF8(leafName).get());

        QM_TRY(aOriginProps.mDirectory->Remove(/* recursive */ true));
      } else {
        QM_TRY(aOriginProps.mDirectory->MoveTo(mPermanentStorageDir, u""_ns));
      }
    }
  } else if (aOriginProps.mNeedsRestore) {
    QM_TRY(CreateDirectoryMetadata(*aOriginProps.mDirectory,
                                   aOriginProps.mTimestamp,
                                   aOriginProps.mOriginMetadata));
  } else if (!aOriginProps.mIgnore) {
    QM_TRY_INSPECT(const auto& file,
                   CloneFileAndAppend(*aOriginProps.mDirectory,
                                      nsLiteralString(METADATA_FILE_NAME)));

    QM_TRY_INSPECT(const auto& stream,
                   GetBinaryOutputStream(*file, FileFlag::Append));

    MOZ_ASSERT(stream);

    // Currently unused (used to be isApp).
    QM_TRY(stream->WriteBoolean(false));
  }

  return NS_OK;
}

nsresult UpgradeStorageHelperBase::Init() {
  AssertIsOnIOThread();
  MOZ_ASSERT(mDirectory);

  const auto maybePersistenceType =
      PersistenceTypeFromFile(*mDirectory, fallible);
  QM_TRY(OkIf(maybePersistenceType.isSome()), Err(NS_ERROR_FAILURE));

  mPersistenceType.init(maybePersistenceType.value());

  return NS_OK;
}

PersistenceType UpgradeStorageHelperBase::PersistenceTypeFromSpec(
    const nsCString& aSpec) {
  // There's no moving of origin directories between repositories like in the
  // CreateOrUpgradeDirectoryMetadataHelper
  return *mPersistenceType;
}

nsresult UpgradeStorageFrom0_0To1_0Helper::PrepareOriginDirectory(
    OriginProps& aOriginProps, bool* aRemoved) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aRemoved);

  int64_t timestamp;
  nsCString group;
  nsCString origin;
  Nullable<bool> isApp;

  QM_WARNONLY_TRY_UNWRAP(
      const auto maybeDirectoryMetadata,
      ToResult(GetDirectoryMetadata(aOriginProps.mDirectory.get(), timestamp,
                                    group, origin, isApp)));
  if (!maybeDirectoryMetadata || isApp.IsNull()) {
    aOriginProps.mTimestamp = GetOriginLastModifiedTime(aOriginProps);
    aOriginProps.mNeedsRestore = true;
  } else {
    aOriginProps.mTimestamp = timestamp;
  }

  *aRemoved = false;
  return NS_OK;
}

nsresult UpgradeStorageFrom0_0To1_0Helper::ProcessOriginDirectory(
    const OriginProps& aOriginProps) {
  AssertIsOnIOThread();

  // This handles changes in origin string generation from nsIPrincipal,
  // especially the change from: appId+inMozBrowser+originNoSuffix
  // to: origin (with origin suffix).
  QM_TRY_INSPECT(const bool& renamed, MaybeRenameOrigin(aOriginProps));
  if (renamed) {
    return NS_OK;
  }

  if (aOriginProps.mNeedsRestore) {
    QM_TRY(CreateDirectoryMetadata(*aOriginProps.mDirectory,
                                   aOriginProps.mTimestamp,
                                   aOriginProps.mOriginMetadata));
  }

  QM_TRY(CreateDirectoryMetadata2(
      *aOriginProps.mDirectory, aOriginProps.mTimestamp,
      /* aPersisted */ false, aOriginProps.mOriginMetadata));

  return NS_OK;
}

nsresult UpgradeStorageFrom1_0To2_0Helper::MaybeRemoveMorgueDirectory(
    const OriginProps& aOriginProps) {
  AssertIsOnIOThread();

  // The Cache API was creating top level morgue directories by accident for
  // a short time in nightly.  This unfortunately prevents all storage from
  // working.  So recover these profiles permanently by removing these corrupt
  // directories as part of this upgrade.

  QM_TRY_INSPECT(const auto& morgueDir,
                 MOZ_TO_RESULT_INVOKE_TYPED(nsCOMPtr<nsIFile>,
                                            *aOriginProps.mDirectory, Clone));

  QM_TRY(morgueDir->Append(u"morgue"_ns));

  QM_TRY_INSPECT(const bool& exists, MOZ_TO_RESULT_INVOKE(morgueDir, Exists));

  if (exists) {
    QM_WARNING("Deleting accidental morgue directory!");

    QM_TRY(morgueDir->Remove(/* recursive */ true));
  }

  return NS_OK;
}

Result<bool, nsresult> UpgradeStorageFrom1_0To2_0Helper::MaybeRemoveAppsData(
    const OriginProps& aOriginProps) {
  AssertIsOnIOThread();

  // TODO: This method was empty for some time due to accidental changes done
  //       in bug 1320404. This led to renaming of origin directories like:
  //         https+++developer.cdn.mozilla.net^appId=1007&inBrowser=1
  //       to:
  //         https+++developer.cdn.mozilla.net^inBrowser=1
  //       instead of just removing them.

  const nsCString& originalSuffix = aOriginProps.mOriginalSuffix;
  if (!originalSuffix.IsEmpty()) {
    MOZ_ASSERT(originalSuffix[0] == '^');

    if (!URLParams::Parse(
            Substring(originalSuffix, 1, originalSuffix.Length() - 1),
            [](const nsAString& aName, const nsAString& aValue) {
              if (aName.EqualsLiteral("appId")) {
                return false;
              }

              return true;
            })) {
      QM_TRY(RemoveObsoleteOrigin(aOriginProps));

      return true;
    }
  }

  return false;
}

nsresult UpgradeStorageFrom1_0To2_0Helper::PrepareOriginDirectory(
    OriginProps& aOriginProps, bool* aRemoved) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aRemoved);

  QM_TRY(MaybeRemoveMorgueDirectory(aOriginProps));

  QM_TRY(
      MaybeUpgradeClients(aOriginProps, &Client::UpgradeStorageFrom1_0To2_0));

  QM_TRY_INSPECT(const bool& removed, MaybeRemoveAppsData(aOriginProps));
  if (removed) {
    *aRemoved = true;
    return NS_OK;
  }

  int64_t timestamp;
  nsCString group;
  nsCString origin;
  Nullable<bool> isApp;
  QM_WARNONLY_TRY_UNWRAP(
      const auto maybeDirectoryMetadata,
      ToResult(GetDirectoryMetadata(aOriginProps.mDirectory.get(), timestamp,
                                    group, origin, isApp)));
  if (!maybeDirectoryMetadata || isApp.IsNull()) {
    aOriginProps.mNeedsRestore = true;
  }

  nsCString suffix;
  QM_WARNONLY_TRY_UNWRAP(
      const auto maybeDirectoryMetadata2,
      ToResult(GetDirectoryMetadata2(aOriginProps.mDirectory.get(), timestamp,
                                     suffix, group, origin, isApp.SetValue())));
  if (!maybeDirectoryMetadata2) {
    aOriginProps.mTimestamp = GetOriginLastModifiedTime(aOriginProps);
    aOriginProps.mNeedsRestore2 = true;
  } else {
    aOriginProps.mTimestamp = timestamp;
  }

  *aRemoved = false;
  return NS_OK;
}

nsresult UpgradeStorageFrom1_0To2_0Helper::ProcessOriginDirectory(
    const OriginProps& aOriginProps) {
  AssertIsOnIOThread();

  // This handles changes in origin string generation from nsIPrincipal,
  // especially the stripping of obsolete origin attributes like addonId.
  QM_TRY_INSPECT(const bool& renamed, MaybeRenameOrigin(aOriginProps));
  if (renamed) {
    return NS_OK;
  }

  if (aOriginProps.mNeedsRestore) {
    QM_TRY(CreateDirectoryMetadata(*aOriginProps.mDirectory,
                                   aOriginProps.mTimestamp,
                                   aOriginProps.mOriginMetadata));
  }

  if (aOriginProps.mNeedsRestore2) {
    QM_TRY(CreateDirectoryMetadata2(
        *aOriginProps.mDirectory, aOriginProps.mTimestamp,
        /* aPersisted */ false, aOriginProps.mOriginMetadata));
  }

  return NS_OK;
}

nsresult UpgradeStorageFrom2_0To2_1Helper::PrepareOriginDirectory(
    OriginProps& aOriginProps, bool* aRemoved) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aRemoved);

  QM_TRY(
      MaybeUpgradeClients(aOriginProps, &Client::UpgradeStorageFrom2_0To2_1));

  int64_t timestamp;
  nsCString group;
  nsCString origin;
  Nullable<bool> isApp;
  QM_WARNONLY_TRY_UNWRAP(
      const auto maybeDirectoryMetadata,
      ToResult(GetDirectoryMetadata(aOriginProps.mDirectory.get(), timestamp,
                                    group, origin, isApp)));
  if (!maybeDirectoryMetadata || isApp.IsNull()) {
    aOriginProps.mNeedsRestore = true;
  }

  nsCString suffix;
  QM_WARNONLY_TRY_UNWRAP(
      const auto maybeDirectoryMetadata2,
      ToResult(GetDirectoryMetadata2(aOriginProps.mDirectory.get(), timestamp,
                                     suffix, group, origin, isApp.SetValue())));
  if (!maybeDirectoryMetadata2) {
    aOriginProps.mTimestamp = GetOriginLastModifiedTime(aOriginProps);
    aOriginProps.mNeedsRestore2 = true;
  } else {
    aOriginProps.mTimestamp = timestamp;
  }

  *aRemoved = false;
  return NS_OK;
}

nsresult UpgradeStorageFrom2_0To2_1Helper::ProcessOriginDirectory(
    const OriginProps& aOriginProps) {
  AssertIsOnIOThread();

  if (aOriginProps.mNeedsRestore) {
    QM_TRY(CreateDirectoryMetadata(*aOriginProps.mDirectory,
                                   aOriginProps.mTimestamp,
                                   aOriginProps.mOriginMetadata));
  }

  if (aOriginProps.mNeedsRestore2) {
    QM_TRY(CreateDirectoryMetadata2(
        *aOriginProps.mDirectory, aOriginProps.mTimestamp,
        /* aPersisted */ false, aOriginProps.mOriginMetadata));
  }

  return NS_OK;
}

nsresult UpgradeStorageFrom2_1To2_2Helper::PrepareOriginDirectory(
    OriginProps& aOriginProps, bool* aRemoved) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aRemoved);

  QM_TRY(
      MaybeUpgradeClients(aOriginProps, &Client::UpgradeStorageFrom2_1To2_2));

  int64_t timestamp;
  nsCString group;
  nsCString origin;
  Nullable<bool> isApp;
  QM_WARNONLY_TRY_UNWRAP(
      const auto maybeDirectoryMetadata,
      ToResult(GetDirectoryMetadata(aOriginProps.mDirectory.get(), timestamp,
                                    group, origin, isApp)));
  if (!maybeDirectoryMetadata || isApp.IsNull()) {
    aOriginProps.mNeedsRestore = true;
  }

  nsCString suffix;
  QM_WARNONLY_TRY_UNWRAP(
      const auto maybeDirectoryMetadata2,
      ToResult(GetDirectoryMetadata2(aOriginProps.mDirectory.get(), timestamp,
                                     suffix, group, origin, isApp.SetValue())));
  if (!maybeDirectoryMetadata2) {
    aOriginProps.mTimestamp = GetOriginLastModifiedTime(aOriginProps);
    aOriginProps.mNeedsRestore2 = true;
  } else {
    aOriginProps.mTimestamp = timestamp;
  }

  *aRemoved = false;
  return NS_OK;
}

nsresult UpgradeStorageFrom2_1To2_2Helper::ProcessOriginDirectory(
    const OriginProps& aOriginProps) {
  AssertIsOnIOThread();

  if (aOriginProps.mNeedsRestore) {
    QM_TRY(CreateDirectoryMetadata(*aOriginProps.mDirectory,
                                   aOriginProps.mTimestamp,
                                   aOriginProps.mOriginMetadata));
  }

  if (aOriginProps.mNeedsRestore2) {
    QM_TRY(CreateDirectoryMetadata2(
        *aOriginProps.mDirectory, aOriginProps.mTimestamp,
        /* aPersisted */ false, aOriginProps.mOriginMetadata));
  }

  return NS_OK;
}

nsresult UpgradeStorageFrom2_1To2_2Helper::PrepareClientDirectory(
    nsIFile* aFile, const nsAString& aLeafName, bool& aRemoved) {
  AssertIsOnIOThread();

  if (Client::IsDeprecatedClient(aLeafName)) {
    QM_WARNING("Deleting deprecated %s client!",
               NS_ConvertUTF16toUTF8(aLeafName).get());

    QM_TRY(aFile->Remove(true));

    aRemoved = true;
  } else {
    aRemoved = false;
  }

  return NS_OK;
}

nsresult RestoreDirectoryMetadata2Helper::Init() {
  AssertIsOnIOThread();
  MOZ_ASSERT(mDirectory);

  nsCOMPtr<nsIFile> parentDir;
  QM_TRY(mDirectory->GetParent(getter_AddRefs(parentDir)));

  const auto maybePersistenceType =
      PersistenceTypeFromFile(*parentDir, fallible);
  QM_TRY(OkIf(maybePersistenceType.isSome()), Err(NS_ERROR_FAILURE));

  mPersistenceType.init(maybePersistenceType.value());

  return NS_OK;
}

nsresult RestoreDirectoryMetadata2Helper::RestoreMetadata2File() {
  OriginProps originProps(WrapMovingNotNull(mDirectory));
  QM_TRY(originProps.Init(
      [&self = *this](const auto& aSpec) { return *self.mPersistenceType; }));

  QM_TRY(OkIf(originProps.mType != OriginProps::eInvalid), NS_ERROR_FAILURE);

  originProps.mTimestamp = GetOriginLastModifiedTime(originProps);

  mOriginProps.AppendElement(std::move(originProps));

  QM_TRY(ProcessOriginDirectories());

  return NS_OK;
}

nsresult RestoreDirectoryMetadata2Helper::ProcessOriginDirectory(
    const OriginProps& aOriginProps) {
  AssertIsOnIOThread();

  // We don't have any approach to restore aPersisted, so reset it to false.
  QM_TRY(CreateDirectoryMetadata2(
      *aOriginProps.mDirectory, aOriginProps.mTimestamp,
      /* aPersisted */ false, aOriginProps.mOriginMetadata));

  return NS_OK;
}

}  // namespace mozilla::dom::quota
