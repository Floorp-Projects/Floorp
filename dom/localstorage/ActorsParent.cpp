/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActorsParent.h"

#include "LocalStorageCommon.h"
#include "LSObject.h"
#include "mozIStorageConnection.h"
#include "mozIStorageFunction.h"
#include "mozIStorageService.h"
#include "mozStorageCID.h"
#include "mozStorageHelper.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ClientManagerService.h"
#include "mozilla/dom/LSWriteOptimizer.h"
#include "mozilla/dom/PBackgroundLSDatabaseParent.h"
#include "mozilla/dom/PBackgroundLSObserverParent.h"
#include "mozilla/dom/PBackgroundLSRequestParent.h"
#include "mozilla/dom/PBackgroundLSSharedTypes.h"
#include "mozilla/dom/PBackgroundLSSimpleRequestParent.h"
#include "mozilla/dom/PBackgroundLSSnapshotParent.h"
#include "mozilla/dom/StorageDBUpdater.h"
#include "mozilla/dom/StorageUtils.h"
#include "mozilla/dom/quota/CheckedUnsafePtr.h"
#include "mozilla/dom/quota/OriginScope.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/QuotaObject.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "mozilla/Logging.h"
#include "mozilla/storage/Variant.h"
#include "mozilla/StoragePrincipalHelper.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "nsExceptionHandler.h"
#include "nsInterfaceHashtable.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsISimpleEnumerator.h"
#include "nsNetUtil.h"
#include "nsRefPtrHashtable.h"
#include "ReportInternalError.h"

#define DISABLE_ASSERTS_FOR_FUZZING 0

#if DISABLE_ASSERTS_FOR_FUZZING
#  define ASSERT_UNLESS_FUZZING(...) \
    do {                             \
    } while (0)
#else
#  define ASSERT_UNLESS_FUZZING(...) MOZ_ASSERT(false, __VA_ARGS__)
#endif

#define LS_LOG_TEST() MOZ_LOG_TEST(GetLocalStorageLogger(), LogLevel::Info)
#define LS_LOG(_args) MOZ_LOG(GetLocalStorageLogger(), LogLevel::Info, _args)

#if defined(MOZ_WIDGET_ANDROID)
#  define LS_MOBILE
#endif

namespace mozilla {
namespace dom {

using namespace mozilla::dom::quota;
using namespace mozilla::dom::StorageUtils;
using namespace mozilla::ipc;

namespace {

struct ArchivedOriginInfo;
class ArchivedOriginScope;
class Connection;
class ConnectionThread;
class Database;
class Observer;
class PrepareDatastoreOp;
class PreparedDatastore;
class QuotaClient;
class Snapshot;

typedef nsClassHashtable<nsCStringHashKey, ArchivedOriginInfo>
    ArchivedOriginHashtable;

/*******************************************************************************
 * Constants
 ******************************************************************************/

// Major schema version. Bump for almost everything.
const uint32_t kMajorSchemaVersion = 4;

// Minor schema version. Should almost always be 0 (maybe bump on release
// branches if we have to).
const uint32_t kMinorSchemaVersion = 0;

// The schema version we store in the SQLite database is a (signed) 32-bit
// integer. The major version is left-shifted 4 bits so the max value is
// 0xFFFFFFF. The minor version occupies the lower 4 bits and its max is 0xF.
static_assert(kMajorSchemaVersion <= 0xFFFFFFF,
              "Major version needs to fit in 28 bits.");
static_assert(kMinorSchemaVersion <= 0xF,
              "Minor version needs to fit in 4 bits.");

const int32_t kSQLiteSchemaVersion =
    int32_t((kMajorSchemaVersion << 4) + kMinorSchemaVersion);

// Changing the value here will override the page size of new databases only.
// A journal mode change and VACUUM are needed to change existing databases, so
// the best way to do that is to use the schema version upgrade mechanism.
const uint32_t kSQLitePageSizeOverride =
#ifdef LS_MOBILE
    512;
#else
    1024;
#endif

static_assert(kSQLitePageSizeOverride == /* mozStorage default */ 0 ||
                  (kSQLitePageSizeOverride % 2 == 0 &&
                   kSQLitePageSizeOverride >= 512 &&
                   kSQLitePageSizeOverride <= 65536),
              "Must be 0 (disabled) or a power of 2 between 512 and 65536!");

// Set to some multiple of the page size to grow the database in larger chunks.
const uint32_t kSQLiteGrowthIncrement = kSQLitePageSizeOverride * 2;

static_assert(kSQLiteGrowthIncrement >= 0 &&
                  kSQLiteGrowthIncrement % kSQLitePageSizeOverride == 0 &&
                  kSQLiteGrowthIncrement < uint32_t(INT32_MAX),
              "Must be 0 (disabled) or a positive multiple of the page size!");

/**
 * The database name for LocalStorage data in a per-origin directory.
 */
#define DATA_FILE_NAME "data.sqlite"
/**
 * The journal corresponding to DATA_FILE_NAME.  (We don't use WAL mode.)
 */
#define JOURNAL_FILE_NAME "data.sqlite-journal"

/**
 * This file contains the current usage of the LocalStorage database as defined
 * by the mozLength totals of all keys and values for the database, which
 * differs from the actual size on disk.  We store this value in a separate
 * file as a cache so that we can initialize the QuotaClient faster.
 * In the future, this file will be eliminated and the information will be
 * stored in PROFILE/storage.sqlite or similar QuotaManager-wide storage.
 *
 * The file contains a binary verification cookie (32-bits) followed by the
 * actual usage (64-bits).
 */
#define USAGE_FILE_NAME "usage"

/**
 * Following a QuotaManager idiom, this journal file's existence is a marker
 * that the usage file was in the process of being updated and is currently
 * invalid.  This file is created prior to updating the usage file and only
 * deleted after the usage file has been written and closed and any pending
 * database transactions have been committed.  Note that this idiom is expected
 * to work if Gecko crashes in the middle of a write, but is not expected to be
 * foolproof in the face of a system crash, as we do not explicitly attempt to
 * fsync the directory containing the journal file.
 *
 * If the journal file is found to exist at origin initialization time, the
 * usage will be re-computed from the current state of DATA_FILE_NAME.
 */
#define USAGE_JOURNAL_FILE_NAME "usage-journal"

static const uint32_t kUsageFileSize = 12;
static const uint32_t kUsageFileCookie = 0x420a420a;

/**
 * How long between the first moment we know we have data to be written on a
 * `Connection` and when we should actually perform the write.  This helps
 * limit disk churn under silly usage patterns and is historically consistent
 * with the previous, legacy implementation.
 *
 * Note that flushing happens downstream of Snapshot checkpointing and its
 * batch mechanism which helps avoid wasteful IPC in the case of silly content
 * code.
 */
const uint32_t kFlushTimeoutMs = 5000;

const char kPrivateBrowsingObserverTopic[] = "last-pb-context-exited";

const uint32_t kDefaultNextGen = false;
const uint32_t kDefaultShadowWrites = true;
const uint32_t kDefaultSnapshotPrefill = 16384;
const uint32_t kDefaultSnapshotGradualPrefill = 4096;
const uint32_t kDefaultClientValidation = true;
/**
 *
 */
const char kNextGenPref[] = "dom.storage.next_gen";
/**
 * Should all mutations also be reflected in the "shadow" database, which is
 * the legacy webappsstore.sqlite database.  When this is enabled, users can
 * downgrade their version of Firefox and/or otherwise fall back to the legacy
 * implementation without loss of data.  (Older versions of Firefox will
 * recognize the presence of ls-archive.sqlite and purge it and the other
 * LocalStorage directories so privacy is maintained.)
 */
const char kShadowWritesPref[] = "dom.storage.shadow_writes";
/**
 * Byte budget for sending data down to the LSSnapshot instance when it is first
 * created.  If there is less data than this (measured by tallying the string
 * length of the keys and values), all data is sent, otherwise partial data is
 * sent.  See `Snapshot`.
 */
const char kSnapshotPrefillPref[] = "dom.storage.snapshot_prefill";
/**
 * When a specific value is requested by an LSSnapshot that is not already fully
 * populated, gradual prefill is used. This preference specifies the number of
 * bytes to be used to send values beyond the specific value that is requested.
 * (The size of the explicitly requested value does not impact this preference.)
 * Setting the value to 0 disables gradual prefill. Tests may set this value to
 * -1 which is converted to INT_MAX in order to cause gradual prefill to send
 * all values not previously sent.
 */
const char kSnapshotGradualPrefillPref[] =
    "dom.storage.snapshot_gradual_prefill";

const char kClientValidationPref[] = "dom.storage.client_validation";

/**
 * The amount of time a PreparedDatastore instance should stick around after a
 * preload is triggered in order to give time for the page to use LocalStorage
 * without triggering worst-case synchronous jank.
 */
const uint32_t kPreparedDatastoreTimeoutMs = 20000;

/**
 * Cold storage for LocalStorage data extracted from webappsstore.sqlite at
 * LSNG first-run that has not yet been migrated to its own per-origin directory
 * by use.
 *
 * In other words, at first run, LSNG copies the contents of webappsstore.sqlite
 * into this database.  As requests are made for that LocalStorage data, the
 * contents are removed from this database and placed into per-origin QM
 * storage.  So the contents of this database are always old, unused
 * LocalStorage data that we can potentially get rid of at some point in the
 * future.
 */
#define LS_ARCHIVE_FILE_NAME "ls-archive.sqlite"
/**
 * The legacy LocalStorage database.  Its contents are maintained as our
 * "shadow" database so that LSNG can be disabled without loss of user data.
 */
#define WEB_APPS_STORE_FILE_NAME "webappsstore.sqlite"

// Shadow database Write Ahead Log's maximum size is 512KB
const uint32_t kShadowMaxWALSize = 512 * 1024;

const uint32_t kShadowJournalSizeLimit = kShadowMaxWALSize * 3;

/**
 * Automatically crash the browser if LocalStorage shutdown takes this long.
 * We've chosen a value that is longer than the value for QuotaManager shutdown
 * timer which is currently set to 30 seconds.  We've also chosen a value that
 * is long enough that it is unlikely for the problem to be falsely triggered by
 * slow system I/O.  We've also chosen a value long enough so that automated
 * tests should time out and fail if LocalStorage shutdown hangs.  Also, this
 * value is long enough so that testers can notice the LocalStorage shutdown
 * hang; we want to know about the hangs, not hide them.  On the other hand this
 * value is less than 60 seconds which is used by nsTerminator to crash a hung
 * main process.
 */
#define SHUTDOWN_TIMEOUT_MS 50000

bool IsOnConnectionThread();

void AssertIsOnConnectionThread();

/*******************************************************************************
 * SQLite functions
 ******************************************************************************/

int32_t MakeSchemaVersion(uint32_t aMajorSchemaVersion,
                          uint32_t aMinorSchemaVersion) {
  return int32_t((aMajorSchemaVersion << 4) + aMinorSchemaVersion);
}

nsCString GetArchivedOriginHashKey(const nsACString& aOriginSuffix,
                                   const nsACString& aOriginNoSuffix) {
  return aOriginSuffix + NS_LITERAL_CSTRING(":") + aOriginNoSuffix;
}

nsresult CreateTables(mozIStorageConnection* aConnection) {
  MOZ_ASSERT(IsOnIOThread() || IsOnConnectionThread());
  MOZ_ASSERT(aConnection);

  // Table `database`
  nsresult rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE database"
                         "( origin TEXT NOT NULL"
                         ", usage INTEGER NOT NULL DEFAULT 0"
                         ", last_vacuum_time INTEGER NOT NULL DEFAULT 0"
                         ", last_analyze_time INTEGER NOT NULL DEFAULT 0"
                         ", last_vacuum_size INTEGER NOT NULL DEFAULT 0"
                         ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Table `data`
  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("CREATE TABLE data"
                         "( key TEXT PRIMARY KEY"
                         ", value TEXT NOT NULL"
                         ", utf16Length INTEGER NOT NULL DEFAULT 0"
                         ", compressed INTEGER NOT NULL DEFAULT 0"
                         ", lastAccessTime INTEGER NOT NULL DEFAULT 0"
                         ");"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(kSQLiteSchemaVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom1_0To2_0(mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  nsresult rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "ALTER TABLE database ADD COLUMN usage INTEGER NOT NULL DEFAULT 0;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "UPDATE database "
      "SET usage = (SELECT total(utf16Length(key) + utf16Length(value)) "
      "FROM data);"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(MakeSchemaVersion(2, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom2_0To3_0(mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  nsresult rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "ALTER TABLE data ADD COLUMN utf16Length INTEGER NOT NULL DEFAULT 0;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("UPDATE data SET utf16Length = utf16Length(value);"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(MakeSchemaVersion(3, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult UpgradeSchemaFrom3_0To4_0(mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  nsresult rv = aConnection->SetSchemaVersion(MakeSchemaVersion(4, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult SetDefaultPragmas(mozIStorageConnection* aConnection) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConnection);

  nsresult rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("PRAGMA synchronous = FULL;"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#ifndef LS_MOBILE
  if (kSQLiteGrowthIncrement) {
    // This is just an optimization so ignore the failure if the disk is
    // currently too full.
    rv =
        aConnection->SetGrowthIncrement(kSQLiteGrowthIncrement, EmptyCString());
    if (rv != NS_ERROR_FILE_TOO_BIG && NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
#endif  // LS_MOBILE

  return NS_OK;
}

nsresult CreateStorageConnection(nsIFile* aDBFile, nsIFile* aUsageFile,
                                 const nsACString& aOrigin,
                                 mozIStorageConnection** aConnection,
                                 bool* aRemovedUsageFile) {
  MOZ_ASSERT(IsOnIOThread() || IsOnConnectionThread());
  MOZ_ASSERT(aDBFile);
  MOZ_ASSERT(aUsageFile);
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(aRemovedUsageFile);

  // aRemovedUsageFile has to be initialized even when this method fails.
  *aRemovedUsageFile = false;

  nsresult rv;

  nsCOMPtr<mozIStorageService> ss =
      do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = ss->OpenDatabase(aDBFile, getter_AddRefs(connection));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    // Remove the usage file first.
    rv = aUsageFile->Remove(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Let the caller know that the usage file has been removed.
    *aRemovedUsageFile = true;

    // Nuke the database file.
    rv = aDBFile->Remove(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = ss->OpenDatabase(aDBFile, getter_AddRefs(connection));
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = SetDefaultPragmas(connection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Check to make sure that the database schema is correct.
  int32_t schemaVersion;
  rv = connection->GetSchemaVersion(&schemaVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (schemaVersion > kSQLiteSchemaVersion) {
    LS_WARNING("Unable to open LocalStorage database, schema is too high!");
    return NS_ERROR_FAILURE;
  }

  if (schemaVersion != kSQLiteSchemaVersion) {
    const bool newDatabase = !schemaVersion;

    if (newDatabase) {
      // Set the page size first.
      if (kSQLitePageSizeOverride) {
        rv = connection->ExecuteSimpleSQL(nsPrintfCString(
            "PRAGMA page_size = %" PRIu32 ";", kSQLitePageSizeOverride));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }

      // We have to set the auto_vacuum mode before opening a transaction.
      rv = connection->ExecuteSimpleSQL(
#ifdef LS_MOBILE
          // Turn on full auto_vacuum mode to reclaim disk space on mobile
          // devices (at the cost of some COMMIT speed).
          NS_LITERAL_CSTRING("PRAGMA auto_vacuum = FULL;")
#else
          // Turn on incremental auto_vacuum mode on desktop builds.
          NS_LITERAL_CSTRING("PRAGMA auto_vacuum = INCREMENTAL;")
#endif
      );
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    mozStorageTransaction transaction(
        connection, false, mozIStorageConnection::TRANSACTION_IMMEDIATE);

    if (newDatabase) {
      rv = CreateTables(connection);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      MOZ_ASSERT(NS_SUCCEEDED(connection->GetSchemaVersion(&schemaVersion)));
      MOZ_ASSERT(schemaVersion == kSQLiteSchemaVersion);

      nsCOMPtr<mozIStorageStatement> stmt;
      nsresult rv = connection->CreateStatement(
          NS_LITERAL_CSTRING("INSERT INTO database (origin) "
                             "VALUES (:origin)"),
          getter_AddRefs(stmt));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("origin"), aOrigin);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = stmt->Execute();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      // This logic needs to change next time we change the schema!
      static_assert(kSQLiteSchemaVersion == int32_t((4 << 4) + 0),
                    "Upgrade function needed due to schema version increase.");

      while (schemaVersion != kSQLiteSchemaVersion) {
        if (schemaVersion == MakeSchemaVersion(1, 0)) {
          rv = UpgradeSchemaFrom1_0To2_0(connection);
        } else if (schemaVersion == MakeSchemaVersion(2, 0)) {
          rv = UpgradeSchemaFrom2_0To3_0(connection);
        } else if (schemaVersion == MakeSchemaVersion(3, 0)) {
          rv = UpgradeSchemaFrom3_0To4_0(connection);
        } else {
          LS_WARNING(
              "Unable to open LocalStorage database, no upgrade path is "
              "available!");
          return NS_ERROR_FAILURE;
        }

        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        rv = connection->GetSchemaVersion(&schemaVersion);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }

      MOZ_ASSERT(schemaVersion == kSQLiteSchemaVersion);
    }

    rv = transaction.Commit();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (newDatabase) {
      // Windows caches the file size, let's force it to stat the file again.
      bool dummy;
      rv = aDBFile->Exists(&dummy);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      int64_t fileSize;
      rv = aDBFile->GetFileSize(&fileSize);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      MOZ_ASSERT(fileSize > 0);

      PRTime vacuumTime = PR_Now();
      MOZ_ASSERT(vacuumTime);

      nsCOMPtr<mozIStorageStatement> vacuumTimeStmt;
      rv = connection->CreateStatement(
          NS_LITERAL_CSTRING("UPDATE database "
                             "SET last_vacuum_time = :time"
                             ", last_vacuum_size = :size;"),
          getter_AddRefs(vacuumTimeStmt));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = vacuumTimeStmt->BindInt64ByName(NS_LITERAL_CSTRING("time"),
                                           vacuumTime);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv =
          vacuumTimeStmt->BindInt64ByName(NS_LITERAL_CSTRING("size"), fileSize);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = vacuumTimeStmt->Execute();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  connection.forget(aConnection);
  return NS_OK;
}

nsresult GetStorageConnection(const nsAString& aDatabaseFilePath,
                              mozIStorageConnection** aConnection) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(!aDatabaseFilePath.IsEmpty());
  MOZ_ASSERT(StringEndsWith(aDatabaseFilePath, NS_LITERAL_STRING(".sqlite")));
  MOZ_ASSERT(aConnection);

  nsCOMPtr<nsIFile> databaseFile;
  nsresult rv =
      NS_NewLocalFile(aDatabaseFilePath, false, getter_AddRefs(databaseFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = databaseFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!exists)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<mozIStorageService> ss =
      do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = ss->OpenDatabase(databaseFile, getter_AddRefs(connection));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = SetDefaultPragmas(connection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  connection.forget(aConnection);
  return NS_OK;
}

nsresult GetArchiveFile(const nsAString& aStoragePath, nsIFile** aArchiveFile) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!aStoragePath.IsEmpty());
  MOZ_ASSERT(aArchiveFile);

  nsCOMPtr<nsIFile> archiveFile;
  nsresult rv =
      NS_NewLocalFile(aStoragePath, false, getter_AddRefs(archiveFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = archiveFile->Append(NS_LITERAL_STRING(LS_ARCHIVE_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  archiveFile.forget(aArchiveFile);
  return NS_OK;
}

nsresult CreateArchiveStorageConnection(const nsAString& aStoragePath,
                                        mozIStorageConnection** aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!aStoragePath.IsEmpty());
  MOZ_ASSERT(aConnection);

  nsCOMPtr<nsIFile> archiveFile;
  nsresult rv = GetArchiveFile(aStoragePath, getter_AddRefs(archiveFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // QuotaManager ensures this file always exists.
  DebugOnly<bool> exists;
  MOZ_ASSERT(NS_SUCCEEDED(archiveFile->Exists(&exists)));
  MOZ_ASSERT(exists);

  bool isDirectory;
  rv = archiveFile->IsDirectory(&isDirectory);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (isDirectory) {
    LS_WARNING("ls-archive is not a file!");
    *aConnection = nullptr;
    return NS_OK;
  }

  nsCOMPtr<mozIStorageService> ss =
      do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = ss->OpenUnsharedDatabase(archiveFile, getter_AddRefs(connection));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    // Don't throw an error, leave a corrupted ls-archive database as it is.
    *aConnection = nullptr;
    return NS_OK;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = StorageDBUpdater::Update(connection);
  if (NS_FAILED(rv)) {
    // Don't throw an error, leave a non-updateable ls-archive database as
    // it is.
    *aConnection = nullptr;
    return NS_OK;
  }

  connection.forget(aConnection);
  return NS_OK;
}

nsresult AttachArchiveDatabase(const nsAString& aStoragePath,
                               mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!aStoragePath.IsEmpty());
  MOZ_ASSERT(aConnection);
  nsCOMPtr<nsIFile> archiveFile;

  nsresult rv = GetArchiveFile(aStoragePath, getter_AddRefs(archiveFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#ifdef DEBUG
  bool exists;
  rv = archiveFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(exists);
#endif

  nsString path;
  rv = archiveFile->GetPath(path);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageStatement> stmt;
  rv = aConnection->CreateStatement(
      NS_LITERAL_CSTRING("ATTACH DATABASE :path AS archive;"),
      getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("path"), path);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult DetachArchiveDatabase(mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  nsresult rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DETACH DATABASE archive"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult GetShadowFile(const nsAString& aBasePath, nsIFile** aArchiveFile) {
  MOZ_ASSERT(IsOnIOThread() || IsOnConnectionThread());
  MOZ_ASSERT(!aBasePath.IsEmpty());
  MOZ_ASSERT(aArchiveFile);

  nsCOMPtr<nsIFile> archiveFile;
  nsresult rv = NS_NewLocalFile(aBasePath, false, getter_AddRefs(archiveFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = archiveFile->Append(NS_LITERAL_STRING(WEB_APPS_STORE_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  archiveFile.forget(aArchiveFile);
  return NS_OK;
}

nsresult SetShadowJournalMode(mozIStorageConnection* aConnection) {
  MOZ_ASSERT(IsOnIOThread() || IsOnConnectionThread());
  MOZ_ASSERT(aConnection);

  // Try enabling WAL mode. This can fail in various circumstances so we have to
  // check the results here.
  NS_NAMED_LITERAL_CSTRING(journalModeQueryStart, "PRAGMA journal_mode = ");
  NS_NAMED_LITERAL_CSTRING(journalModeWAL, "wal");

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aConnection->CreateStatement(
      journalModeQueryStart + journalModeWAL, getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(hasResult);

  nsCString journalMode;
  rv = stmt->GetUTF8String(0, journalMode);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (journalMode.Equals(journalModeWAL)) {
    // WAL mode successfully enabled. Set limits on its size here.

    // Set the threshold for auto-checkpointing the WAL. We don't want giant
    // logs slowing down us.
    rv = aConnection->CreateStatement(NS_LITERAL_CSTRING("PRAGMA page_size;"),
                                      getter_AddRefs(stmt));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    bool hasResult;
    rv = stmt->ExecuteStep(&hasResult);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(hasResult);

    int32_t pageSize;
    rv = stmt->GetInt32(0, &pageSize);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(pageSize >= 512 && pageSize <= 65536);

    nsAutoCString pageCount;
    pageCount.AppendInt(static_cast<int32_t>(kShadowMaxWALSize / pageSize));

    rv = aConnection->ExecuteSimpleSQL(
        NS_LITERAL_CSTRING("PRAGMA wal_autocheckpoint = ") + pageCount);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Set the maximum WAL log size to reduce footprint on mobile (large empty
    // WAL files will be truncated)
    nsAutoCString sizeLimit;
    sizeLimit.AppendInt(kShadowJournalSizeLimit);

    rv = aConnection->ExecuteSimpleSQL(
        NS_LITERAL_CSTRING("PRAGMA journal_size_limit = ") + sizeLimit);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    rv = aConnection->ExecuteSimpleSQL(journalModeQueryStart +
                                       NS_LITERAL_CSTRING("truncate"));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult CreateShadowStorageConnection(const nsAString& aBasePath,
                                       mozIStorageConnection** aConnection) {
  MOZ_ASSERT(IsOnIOThread() || IsOnConnectionThread());
  MOZ_ASSERT(!aBasePath.IsEmpty());
  MOZ_ASSERT(aConnection);

  nsCOMPtr<nsIFile> shadowFile;
  nsresult rv = GetShadowFile(aBasePath, getter_AddRefs(shadowFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageService> ss =
      do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = ss->OpenUnsharedDatabase(shadowFile, getter_AddRefs(connection));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    rv = shadowFile->Remove(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = ss->OpenUnsharedDatabase(shadowFile, getter_AddRefs(connection));
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = SetShadowJournalMode(connection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = StorageDBUpdater::Update(connection);
  if (NS_FAILED(rv)) {
    rv = connection->Close();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = shadowFile->Remove(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = ss->OpenUnsharedDatabase(shadowFile, getter_AddRefs(connection));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = SetShadowJournalMode(connection);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = StorageDBUpdater::Update(connection);
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  connection.forget(aConnection);
  return NS_OK;
}

nsresult GetShadowStorageConnection(const nsAString& aBasePath,
                                    mozIStorageConnection** aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(!aBasePath.IsEmpty());
  MOZ_ASSERT(aConnection);

  nsCOMPtr<nsIFile> shadowFile;
  nsresult rv = GetShadowFile(aBasePath, getter_AddRefs(shadowFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = shadowFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!exists)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<mozIStorageService> ss =
      do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = ss->OpenUnsharedDatabase(shadowFile, getter_AddRefs(connection));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  connection.forget(aConnection);
  return NS_OK;
}

nsresult AttachShadowDatabase(const nsAString& aBasePath,
                              mozIStorageConnection* aConnection) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(!aBasePath.IsEmpty());
  MOZ_ASSERT(aConnection);

  nsCOMPtr<nsIFile> shadowFile;
  nsresult rv = GetShadowFile(aBasePath, getter_AddRefs(shadowFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#ifdef DEBUG
  bool exists;
  rv = shadowFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(exists);
#endif

  nsString path;
  rv = shadowFile->GetPath(path);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageStatement> stmt;
  rv = aConnection->CreateStatement(
      NS_LITERAL_CSTRING("ATTACH DATABASE :path AS shadow;"),
      getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("path"), path);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult DetachShadowDatabase(mozIStorageConnection* aConnection) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(aConnection);

  nsresult rv = aConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DETACH DATABASE shadow"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult GetUsageFile(const nsAString& aDirectoryPath, nsIFile** aUsageFile) {
  MOZ_ASSERT(IsOnIOThread() || IsOnConnectionThread());
  MOZ_ASSERT(!aDirectoryPath.IsEmpty());
  MOZ_ASSERT(aUsageFile);

  nsCOMPtr<nsIFile> usageFile;
  nsresult rv =
      NS_NewLocalFile(aDirectoryPath, false, getter_AddRefs(usageFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = usageFile->Append(NS_LITERAL_STRING(USAGE_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  usageFile.forget(aUsageFile);
  return NS_OK;
}

nsresult GetUsageJournalFile(const nsAString& aDirectoryPath,
                             nsIFile** aUsageJournalFile) {
  MOZ_ASSERT(IsOnIOThread() || IsOnConnectionThread());
  MOZ_ASSERT(!aDirectoryPath.IsEmpty());
  MOZ_ASSERT(aUsageJournalFile);

  nsCOMPtr<nsIFile> usageJournalFile;
  nsresult rv =
      NS_NewLocalFile(aDirectoryPath, false, getter_AddRefs(usageJournalFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = usageJournalFile->Append(NS_LITERAL_STRING(USAGE_JOURNAL_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  usageJournalFile.forget(aUsageJournalFile);
  return NS_OK;
}

nsresult UpdateUsageFile(nsIFile* aUsageFile, nsIFile* aUsageJournalFile,
                         int64_t aUsage) {
  MOZ_ASSERT(IsOnIOThread() || IsOnConnectionThread());
  MOZ_ASSERT(aUsageFile);
  MOZ_ASSERT(aUsageJournalFile);
  MOZ_ASSERT(aUsage >= 0);

  bool isDirectory;
  nsresult rv = aUsageJournalFile->IsDirectory(&isDirectory);
  if (rv != NS_ERROR_FILE_NOT_FOUND &&
      rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST) {
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (NS_WARN_IF(isDirectory)) {
      return NS_ERROR_FAILURE;
    }
  } else {
    rv = aUsageJournalFile->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  nsCOMPtr<nsIOutputStream> stream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(stream), aUsageFile);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIBinaryOutputStream> binaryStream =
      NS_NewObjectOutputStream(stream);

  rv = binaryStream->Write32(kUsageFileCookie);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = binaryStream->Write64(aUsage);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stream->Close();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult LoadUsageFile(nsIFile* aUsageFile, int64_t* aUsage) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aUsageFile);
  MOZ_ASSERT(aUsage);

  int64_t fileSize;
  nsresult rv = aUsageFile->GetFileSize(&fileSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(fileSize != kUsageFileSize)) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  nsCOMPtr<nsIInputStream> stream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(stream), aUsageFile);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIInputStream> bufferedStream;
  rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedStream),
                                 stream.forget(), 16);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIBinaryInputStream> binaryStream =
      NS_NewObjectInputStream(bufferedStream);

  uint32_t cookie;
  rv = binaryStream->Read32(&cookie);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(cookie != kUsageFileCookie)) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  uint64_t usage;
  rv = binaryStream->Read64(&usage);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aUsage = usage;
  return NS_OK;
}

/*******************************************************************************
 * Non-actor class declarations
 ******************************************************************************/

/**
 * Coalescing manipulation queue used by `Datastore`.  Used by `Datastore` to
 * update `Datastore::mOrderedItems` efficiently/for code simplification.
 * (Datastore does not actually depend on the coalescing, as mutations are
 * applied atomically when a Snapshot Checkpoints, and with `Datastore::mValues`
 * being updated at the same time the mutations are applied to Datastore's
 * mWriteOptimizer.)
 */
class DatastoreWriteOptimizer final : public LSWriteOptimizer<LSValue> {
 public:
  void ApplyAndReset(nsTArray<LSItemInfo>& aOrderedItems);
};

/**
 * Coalescing manipulation queue used by `Connection`.  Used by `Connection` to
 * buffer and coalesce manipulations applied to the Datastore in batches by
 * Snapshot Checkpointing until flushed to disk.
 */
class ConnectionWriteOptimizer final : public LSWriteOptimizer<LSValue> {
 public:
  nsresult Perform(Connection* aConnection, bool aShadowWrites,
                   int64_t& aOutUsage);

 private:
  /**
   * Handlers for specific mutations.  Each method knows how to `Perform` the
   * manipulation against a `Connection` and the "shadow" database (legacy
   * webappsstore.sqlite database that exists so LSNG can be disabled/safely
   * downgraded from.)
   */
  nsresult PerformInsertOrUpdate(Connection* aConnection, bool aShadowWrites,
                                 const nsAString& aKey, const LSValue& aValue);

  nsresult PerformDelete(Connection* aConnection, bool aShadowWrites,
                         const nsAString& aKey);

  nsresult PerformTruncate(Connection* aConnection, bool aShadowWrites);
};

class DatastoreOperationBase : public Runnable {
  nsCOMPtr<nsIEventTarget> mOwningEventTarget;
  nsresult mResultCode;
  Atomic<bool> mMayProceedOnNonOwningThread;
  bool mMayProceed;

 public:
  nsIEventTarget* OwningEventTarget() const {
    MOZ_ASSERT(mOwningEventTarget);

    return mOwningEventTarget;
  }

  bool IsOnOwningThread() const {
    MOZ_ASSERT(mOwningEventTarget);

    bool current;
    return NS_SUCCEEDED(mOwningEventTarget->IsOnCurrentThread(&current)) &&
           current;
  }

  void AssertIsOnOwningThread() const {
    MOZ_ASSERT(IsOnBackgroundThread());
    MOZ_ASSERT(IsOnOwningThread());
  }

  nsresult ResultCode() const { return mResultCode; }

  void SetFailureCode(nsresult aErrorCode) {
    MOZ_ASSERT(NS_SUCCEEDED(mResultCode));
    MOZ_ASSERT(NS_FAILED(aErrorCode));

    mResultCode = aErrorCode;
  }

  void MaybeSetFailureCode(nsresult aErrorCode) {
    MOZ_ASSERT(NS_FAILED(aErrorCode));

    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = aErrorCode;
    }
  }

  void NoteComplete() {
    AssertIsOnOwningThread();

    mMayProceed = false;
    mMayProceedOnNonOwningThread = false;
  }

  bool MayProceed() const {
    AssertIsOnOwningThread();

    return mMayProceed;
  }

  // May be called on any thread, but you should call MayProceed() if you know
  // you're on the background thread because it is slightly faster.
  bool MayProceedOnNonOwningThread() const {
    return mMayProceedOnNonOwningThread;
  }

 protected:
  DatastoreOperationBase()
      : Runnable("dom::DatastoreOperationBase"),
        mOwningEventTarget(GetCurrentThreadEventTarget()),
        mResultCode(NS_OK),
        mMayProceedOnNonOwningThread(true),
        mMayProceed(true) {}

  ~DatastoreOperationBase() override { MOZ_ASSERT(!mMayProceed); }
};

class ConnectionDatastoreOperationBase : public DatastoreOperationBase {
 protected:
  RefPtr<Connection> mConnection;
  /**
   * This boolean flag is used by the CloseOp to avoid creating empty databases.
   */
  const bool mEnsureStorageConnection;

 public:
  // This callback will be called on the background thread before releasing the
  // final reference to this request object. Subclasses may perform any
  // additional cleanup here but must always call the base class implementation.
  virtual void Cleanup();

 protected:
  ConnectionDatastoreOperationBase(Connection* aConnection,
                                   bool aEnsureStorageConnection = true);

  ~ConnectionDatastoreOperationBase();

  // Must be overridden in subclasses. Called on the target thread to allow the
  // subclass to perform necessary datastore operations. A successful return
  // value will trigger an OnSuccess callback on the background thread while
  // while a failure value will trigger an OnFailure callback.
  virtual nsresult DoDatastoreWork() = 0;

  // Methods that subclasses may implement.
  virtual void OnSuccess();

  virtual void OnFailure(nsresult aResultCode);

 private:
  void RunOnConnectionThread();

  void RunOnOwningThread();

  // Not to be overridden by subclasses.
  NS_DECL_NSIRUNNABLE
};

class Connection final {
  friend class ConnectionThread;

 public:
  class CachedStatement;

 private:
  class InitOriginHelper;

  class FlushOp;
  class CloseOp;

  RefPtr<ConnectionThread> mConnectionThread;
  RefPtr<QuotaClient> mQuotaClient;
  nsCOMPtr<nsITimer> mFlushTimer;
  nsCOMPtr<mozIStorageConnection> mStorageConnection;
  nsAutoPtr<ArchivedOriginScope> mArchivedOriginScope;
  nsInterfaceHashtable<nsCStringHashKey, mozIStorageStatement>
      mCachedStatements;
  ConnectionWriteOptimizer mWriteOptimizer;
  const nsCString mSuffix;
  const nsCString mGroup;
  const nsCString mOrigin;
  nsString mDirectoryPath;
  /**
   * Propagated from PrepareDatastoreOp. PrepareDatastoreOp may defer the
   * creation of the localstorage client directory and database on the
   * QuotaManager IO thread in its DatabaseWork method to
   * Connection::EnsureStorageConnection, in which case the method needs to know
   * it is responsible for taking those actions (without redundantly performing
   * the existence checks).
   */
  const bool mDatabaseWasNotAvailable;
  bool mHasCreatedDatabase;
  bool mFlushScheduled;
#ifdef DEBUG
  bool mInUpdateBatch;
  bool mFinished;
#endif

 public:
  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::Connection)

  void AssertIsOnOwningThread() const { NS_ASSERT_OWNINGTHREAD(Connection); }

  QuotaClient* GetQuotaClient() const {
    MOZ_ASSERT(mQuotaClient);

    return mQuotaClient;
  }

  ArchivedOriginScope* GetArchivedOriginScope() const {
    return mArchivedOriginScope;
  }

  const nsCString& Origin() const { return mOrigin; }

  const nsString& DirectoryPath() const { return mDirectoryPath; }

  void GetFinishInfo(bool& aDatabaseWasNotAvailable,
                     bool& aHasCreatedDatabase) const {
    AssertIsOnOwningThread();
    MOZ_ASSERT(mFinished);

    aDatabaseWasNotAvailable = mDatabaseWasNotAvailable;
    aHasCreatedDatabase = mHasCreatedDatabase;
  }

  //////////////////////////////////////////////////////////////////////////////
  // Methods which can only be called on the owning thread.

  // This method is used to asynchronously execute a connection datastore
  // operation on the connection thread.
  void Dispatch(ConnectionDatastoreOperationBase* aOp);

  // This method is used to asynchronously close the storage connection on the
  // connection thread.
  void Close(nsIRunnable* aCallback);

  void SetItem(const nsString& aKey, const LSValue& aValue, int64_t aDelta,
               bool aIsNewItem);

  void RemoveItem(const nsString& aKey, int64_t aDelta);

  void Clear(int64_t aDelta);

  void BeginUpdateBatch();

  void EndUpdateBatch();

  //////////////////////////////////////////////////////////////////////////////
  // Methods which can only be called on the connection thread.

  nsresult EnsureStorageConnection();

  mozIStorageConnection* StorageConnection() const {
    AssertIsOnConnectionThread();

    return mStorageConnection;
  }

  void CloseStorageConnection();

  nsresult GetCachedStatement(const nsACString& aQuery,
                              CachedStatement* aCachedStatement);

  nsresult BeginWriteTransaction();

  nsresult CommitWriteTransaction();

  nsresult RollbackWriteTransaction();

 private:
  // Only created by ConnectionThread.
  Connection(ConnectionThread* aConnectionThread, const nsACString& aSuffix,
             const nsACString& aGroup, const nsACString& aOrigin,
             nsAutoPtr<ArchivedOriginScope>&& aArchivedOriginScope,
             bool aDatabaseWasNotAvailable);

  ~Connection();

  void ScheduleFlush();

  void Flush();

  static void FlushTimerCallback(nsITimer* aTimer, void* aClosure);
};

class Connection::CachedStatement final {
  friend class Connection;

  nsCOMPtr<mozIStorageStatement> mStatement;
  Maybe<mozStorageStatementScoper> mScoper;

 public:
  CachedStatement();
  ~CachedStatement();

  operator mozIStorageStatement*() const;

  mozIStorageStatement* operator->() const MOZ_NO_ADDREF_RELEASE_ON_RETURN;

 private:
  // Only called by Connection.
  void Assign(Connection* aConnection,
              already_AddRefed<mozIStorageStatement> aStatement);

  // No funny business allowed.
  CachedStatement(const CachedStatement&) = delete;
  CachedStatement& operator=(const CachedStatement&) = delete;
};

/**
 * Helper to invoke EnsureOriginIsInitialized on the QuotaManager IO thread from
 * the LocalStorage connection thread when creating a database connection on
 * demand. This is necessary because we attempt to defer the creation of the
 * origin directory and the database until absolutely needed, but the directory
 * creation and origin initialization must happen on the QM IO thread for
 * invariant reasons. (We can't just use a mutex because there could be logic on
 * the IO thread that also wants to deal with the same origin, so we need to
 * queue a runnable and wait our turn.)
 */
class Connection::InitOriginHelper final : public Runnable {
  mozilla::Monitor mMonitor;
  const nsCString mSuffix;
  const nsCString mGroup;
  const nsCString mOrigin;
  nsString mOriginDirectoryPath;
  nsresult mIOThreadResultCode;
  bool mWaiting;

 public:
  InitOriginHelper(const nsACString& aSuffix, const nsACString& aGroup,
                   const nsACString& aOrigin)
      : Runnable("dom::localstorage::Connection::InitOriginHelper"),
        mMonitor("InitOriginHelper::mMonitor"),
        mSuffix(aSuffix),
        mGroup(aGroup),
        mOrigin(aOrigin),
        mIOThreadResultCode(NS_OK),
        mWaiting(true) {
    AssertIsOnConnectionThread();
  }

  nsresult BlockAndReturnOriginDirectoryPath(nsAString& aOriginDirectoryPath);

 private:
  ~InitOriginHelper() {}

  nsresult RunOnIOThread();

  NS_DECL_NSIRUNNABLE
};

class Connection::FlushOp final : public ConnectionDatastoreOperationBase {
  ConnectionWriteOptimizer mWriteOptimizer;
  bool mShadowWrites;

 public:
  FlushOp(Connection* aConnection, ConnectionWriteOptimizer&& aWriteOptimizer);

 private:
  nsresult DoDatastoreWork() override;

  void Cleanup() override;
};

class Connection::CloseOp final : public ConnectionDatastoreOperationBase {
  nsCOMPtr<nsIRunnable> mCallback;

 public:
  CloseOp(Connection* aConnection, nsIRunnable* aCallback)
      : ConnectionDatastoreOperationBase(aConnection,
                                         /* aEnsureStorageConnection */ false),
        mCallback(aCallback) {}

 private:
  nsresult DoDatastoreWork() override;

  void Cleanup() override;
};

class ConnectionThread final {
  friend class Connection;

  nsCOMPtr<nsIThread> mThread;
  nsRefPtrHashtable<nsCStringHashKey, Connection> mConnections;

 public:
  ConnectionThread();

  void AssertIsOnOwningThread() const {
    NS_ASSERT_OWNINGTHREAD(ConnectionThread);
  }

  bool IsOnConnectionThread();

  void AssertIsOnConnectionThread();

  already_AddRefed<Connection> CreateConnection(
      const nsACString& aSuffix, const nsACString& aGroup,
      const nsACString& aOrigin,
      nsAutoPtr<ArchivedOriginScope>&& aArchivedOriginScope,
      bool aDatabaseWasNotAvailable);

  void Shutdown();

  NS_INLINE_DECL_REFCOUNTING(ConnectionThread)

 private:
  ~ConnectionThread();
};

/**
 * Canonical state of Storage for an origin, containing all keys and their
 * values in the parent process.  Specifically, this is the state that will
 * be handed out to freshly created Snapshots and that will be persisted to disk
 * when the Connection's flush completes.  State is mutated in batches as
 * Snapshot instances Checkpoint their mutations locally accumulated in the
 * child LSSnapshots.
 */
class Datastore final
    : public SupportsCheckedUnsafePtr<CheckIf<DiagnosticAssertEnabled>> {
  RefPtr<DirectoryLock> mDirectoryLock;
  RefPtr<Connection> mConnection;
  RefPtr<QuotaObject> mQuotaObject;
  nsCOMPtr<nsIRunnable> mCompleteCallback;
  /**
   * PrepareDatastoreOps register themselves with the Datastore at
   * and unregister in PrepareDatastoreOp::Cleanup.
   */
  nsTHashtable<nsPtrHashKey<PrepareDatastoreOp>> mPrepareDatastoreOps;
  /**
   * PreparedDatastore instances register themselves with their associated
   * Datastore at construction time and unregister at destruction time.  They
   * hang around for kPreparedDatastoreTimeoutMs in order to keep the Datastore
   * from closing itself via MaybeClose(), thereby giving the document enough
   * time to load and access LocalStorage.
   */
  nsTHashtable<nsPtrHashKey<PreparedDatastore>> mPreparedDatastores;
  /**
   * A database is live (and in this hashtable) if it has a live LSDatabase
   * actor.  There is at most one Database per origin per content process.  Each
   * Database corresponds to an LSDatabase in its associated content process.
   */
  nsTHashtable<nsPtrHashKey<Database>> mDatabases;
  /**
   * A database is active if it has a non-null `mSnapshot`.  As long as there
   * are any active databases final deltas can't be calculated and
   * `UpdateUsage()` can't be invoked.
   */
  nsTHashtable<nsPtrHashKey<Database>> mActiveDatabases;
  /**
   * Non-authoritative hashtable representation of mOrderedItems for efficient
   * lookup.
   */
  nsDataHashtable<nsStringHashKey, LSValue> mValues;
  /**
   * The authoritative ordered state of the Datastore; mValue also exists as an
   * unordered hashtable for efficient lookup.
   */
  nsTArray<LSItemInfo> mOrderedItems;
  nsTArray<int64_t> mPendingUsageDeltas;
  DatastoreWriteOptimizer mWriteOptimizer;
  const nsCString mGroup;
  const nsCString mOrigin;
  const uint32_t mPrivateBrowsingId;
  int64_t mUsage;
  int64_t mUpdateBatchUsage;
  int64_t mSizeOfKeys;
  int64_t mSizeOfItems;
  bool mClosed;
  bool mInUpdateBatch;

 public:
  // Created by PrepareDatastoreOp.
  Datastore(const nsACString& aGroup, const nsACString& aOrigin,
            uint32_t aPrivateBrowsingId, int64_t aUsage, int64_t aSizeOfKeys,
            int64_t aSizeOfItems,
            already_AddRefed<DirectoryLock>&& aDirectoryLock,
            already_AddRefed<Connection>&& aConnection,
            already_AddRefed<QuotaObject>&& aQuotaObject,
            nsDataHashtable<nsStringHashKey, LSValue>& aValues,
            nsTArray<LSItemInfo>& aOrderedItems);

  const nsCString& Origin() const { return mOrigin; }

  uint32_t PrivateBrowsingId() const { return mPrivateBrowsingId; }

  bool IsPersistent() const {
    // Private-browsing is forbidden from touching disk, but
    // StorageAccess::eSessionScoped is allowed to touch disk because
    // QuotaManager's storage for such origins is wiped at shutdown.
    return mPrivateBrowsingId == 0;
  }

  void Close();

  bool IsClosed() const {
    AssertIsOnBackgroundThread();

    return mClosed;
  }

  void WaitForConnectionToComplete(nsIRunnable* aCallback);

  void NoteLivePrepareDatastoreOp(PrepareDatastoreOp* aPrepareDatastoreOp);

  void NoteFinishedPrepareDatastoreOp(PrepareDatastoreOp* aPrepareDatastoreOp);

  void NoteLivePreparedDatastore(PreparedDatastore* aPreparedDatastore);

  void NoteFinishedPreparedDatastore(PreparedDatastore* aPreparedDatastore);

  void NoteLiveDatabase(Database* aDatabase);

  void NoteFinishedDatabase(Database* aDatabase);

  void NoteActiveDatabase(Database* aDatabase);

  void NoteInactiveDatabase(Database* aDatabase);

  void GetSnapshotLoadInfo(const nsString& aKey, bool& aAddKeyToUnknownItems,
                           nsTHashtable<nsStringHashKey>& aLoadedItems,
                           nsTArray<LSItemInfo>& aItemInfos,
                           uint32_t& aNextLoadIndex,
                           LSSnapshot::LoadState& aLoadState);

  uint32_t GetLength() const { return mValues.Count(); }

  const nsTArray<LSItemInfo>& GetOrderedItems() const { return mOrderedItems; }

  void GetItem(const nsString& aKey, LSValue& aValue) const;

  void GetKeys(nsTArray<nsString>& aKeys) const;

  //////////////////////////////////////////////////////////////////////////////
  // Mutation Methods
  //
  // These are only called during Snapshot::RecvCheckpoint

  /**
   * Used by Snapshot::RecvCheckpoint to set a key/value pair as part of a an
   * explicit batch.
   */
  void SetItem(Database* aDatabase, const nsString& aKey,
               const LSValue& aValue);

  void RemoveItem(Database* aDatabase, const nsString& aKey);

  void Clear(Database* aDatabase);

  void BeginUpdateBatch(int64_t aSnapshotInitialUsage);

  int64_t EndUpdateBatch(int64_t aSnapshotPeakUsage);

  int64_t GetUsage() const { return mUsage; }

  int64_t RequestUpdateUsage(int64_t aRequestedSize, int64_t aMinSize);

  bool HasOtherProcessObservers(Database* aDatabase);

  void NotifyOtherProcessObservers(Database* aDatabase,
                                   const nsString& aDocumentURI,
                                   const nsString& aKey,
                                   const LSValue& aOldValue,
                                   const LSValue& aNewValue);

  void NoteChangedObserverArray(const nsTArray<Observer*>& aObservers);

  NS_INLINE_DECL_REFCOUNTING(Datastore)

 private:
  // Reference counted.
  ~Datastore();

  bool UpdateUsage(int64_t aDelta);

  void MaybeClose();

  void ConnectionClosedCallback();

  void CleanupMetadata();

  void NotifySnapshots(Database* aDatabase, const nsAString& aKey,
                       const LSValue& aOldValue, bool aAffectsOrder);
};

class PreparedDatastore {
  RefPtr<Datastore> mDatastore;
  nsCOMPtr<nsITimer> mTimer;
  const Maybe<ContentParentId> mContentParentId;
  // Strings share buffers if possible, so it's not a problem to duplicate the
  // origin here.
  const nsCString mOrigin;
  uint64_t mDatastoreId;
  bool mForPreload;
  bool mInvalidated;

 public:
  PreparedDatastore(Datastore* aDatastore,
                    const Maybe<ContentParentId>& aContentParentId,
                    const nsACString& aOrigin, uint64_t aDatastoreId,
                    bool aForPreload)
      : mDatastore(aDatastore),
        mTimer(NS_NewTimer()),
        mContentParentId(aContentParentId),
        mOrigin(aOrigin),
        mDatastoreId(aDatastoreId),
        mForPreload(aForPreload),
        mInvalidated(false) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aDatastore);
    MOZ_ASSERT(mTimer);

    aDatastore->NoteLivePreparedDatastore(this);

    MOZ_ALWAYS_SUCCEEDS(mTimer->InitWithNamedFuncCallback(
        TimerCallback, this, kPreparedDatastoreTimeoutMs,
        nsITimer::TYPE_ONE_SHOT, "PreparedDatastore::TimerCallback"));
  }

  ~PreparedDatastore() {
    MOZ_ASSERT(mDatastore);
    MOZ_ASSERT(mTimer);

    mTimer->Cancel();

    mDatastore->NoteFinishedPreparedDatastore(this);
  }

  Datastore* GetDatastore() const {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mDatastore);

    return mDatastore;
  }

  const Maybe<ContentParentId>& GetContentParentId() const {
    return mContentParentId;
  }

  const nsCString& Origin() const { return mOrigin; }

  void Invalidate() {
    AssertIsOnBackgroundThread();

    mInvalidated = true;

    if (mForPreload) {
      mTimer->Cancel();

      MOZ_ALWAYS_SUCCEEDS(mTimer->InitWithNamedFuncCallback(
          TimerCallback, this, 0, nsITimer::TYPE_ONE_SHOT,
          "PreparedDatastore::TimerCallback"));
    }
  }

  bool IsInvalidated() const {
    AssertIsOnBackgroundThread();

    return mInvalidated;
  }

 private:
  void Destroy();

  static void TimerCallback(nsITimer* aTimer, void* aClosure);
};

/*******************************************************************************
 * Actor class declarations
 ******************************************************************************/

class Database final
    : public PBackgroundLSDatabaseParent,
      public SupportsCheckedUnsafePtr<CheckIf<DiagnosticAssertEnabled>> {
  RefPtr<Datastore> mDatastore;
  Snapshot* mSnapshot;
  const PrincipalInfo mPrincipalInfo;
  const Maybe<ContentParentId> mContentParentId;
  // Strings share buffers if possible, so it's not a problem to duplicate the
  // origin here.
  nsCString mOrigin;
  uint32_t mPrivateBrowsingId;
  bool mAllowedToClose;
  bool mActorDestroyed;
  bool mRequestedAllowToClose;
#ifdef DEBUG
  bool mActorWasAlive;
#endif

 public:
  // Created in AllocPBackgroundLSDatabaseParent.
  Database(const PrincipalInfo& aPrincipalInfo,
           const Maybe<ContentParentId>& aContentParentId,
           const nsACString& aOrigin, uint32_t aPrivateBrowsingId);

  Datastore* GetDatastore() const {
    AssertIsOnBackgroundThread();
    return mDatastore;
  }

  const PrincipalInfo& GetPrincipalInfo() const { return mPrincipalInfo; }

  bool IsOwnedByProcess(ContentParentId aContentParentId) const {
    return mContentParentId && mContentParentId.value() == aContentParentId;
  }

  uint32_t PrivateBrowsingId() const { return mPrivateBrowsingId; }

  const nsCString& Origin() const { return mOrigin; }

  void SetActorAlive(Datastore* aDatastore);

  void RegisterSnapshot(Snapshot* aSnapshot);

  void UnregisterSnapshot(Snapshot* aSnapshot);

  Snapshot* GetSnapshot() const {
    AssertIsOnBackgroundThread();
    return mSnapshot;
  }

  void RequestAllowToClose();

  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::Database)

 private:
  // Reference counted.
  ~Database();

  void AllowToClose();

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvDeleteMe() override;

  mozilla::ipc::IPCResult RecvAllowToClose() override;

  PBackgroundLSSnapshotParent* AllocPBackgroundLSSnapshotParent(
      const nsString& aDocumentURI, const nsString& aKey,
      const bool& aIncreasePeakUsage, const int64_t& aRequestedSize,
      const int64_t& aMinSize, LSSnapshotInitInfo* aInitInfo) override;

  mozilla::ipc::IPCResult RecvPBackgroundLSSnapshotConstructor(
      PBackgroundLSSnapshotParent* aActor, const nsString& aDocumentURI,
      const nsString& aKey, const bool& aIncreasePeakUsage,
      const int64_t& aRequestedSize, const int64_t& aMinSize,
      LSSnapshotInitInfo* aInitInfo) override;

  bool DeallocPBackgroundLSSnapshotParent(
      PBackgroundLSSnapshotParent* aActor) override;
};

/**
 * Attempts to capture the state of the underlying Datastore at the time of its
 * creation so run-to-completion semantics can be honored.
 *
 * Rather than simply duplicate the contents of `DataStore::mValues` and
 * `Datastore::mOrderedItems` at the time of their creation, the Snapshot tracks
 * mutations to the Datastore as they happen, saving off the state of values as
 * they existed when the Snapshot was created.  In other words, given an initial
 * Datastore state of { foo: 'bar', bar: 'baz' }, the Snapshot won't store those
 * values until it hears via `SaveItem` that "foo" is being over-written.  At
 * that time, it will save off foo='bar' in mValues.
 *
 * ## Quota Allocation ##
 *
 * ## States ##
 *
 */
class Snapshot final : public PBackgroundLSSnapshotParent {
  /**
   * The Database that owns this snapshot.  There is a 1:1 relationship between
   * snapshots and databases.
   */
  RefPtr<Database> mDatabase;
  RefPtr<Datastore> mDatastore;
  /**
   * The set of keys for which values have been sent to the child LSSnapshot.
   * Cleared once all values have been sent as indicated by
   * mLoadedItems.Count()==mTotalLength and therefore mLoadedAllItems should be
   * true.  No requests should be received for keys already in this set, and
   * this is enforced by fatal IPC error (unless fuzzing).
   */
  nsTHashtable<nsStringHashKey> mLoadedItems;
  /**
   * The set of keys for which a RecvLoadValueAndMoreItems request was received
   * but there was no such key, and so null was returned.  The child LSSnapshot
   * will also cache these values, so redundant requests are also handled with
   * fatal process termination just like for mLoadedItems.  Also cleared when
   * mLoadedAllItems becomes true because then the child can infer that all
   * other values must be null.  (Note: this could also be done when
   * mLoadKeysReceived is true as a further optimization, but is not.)
   */
  nsTHashtable<nsStringHashKey> mUnknownItems;
  /**
   * Values that have changed in mDatastore as reported by SaveItem
   * notifications that are not yet known to the child LSSnapshot.
   *
   * The naive way to snapshot the state of mDatastore would be to duplicate its
   * internal mValues at the time of our creation, but that is wasteful if few
   * changes are made to the Datastore's state.  So we only track values that
   * are changed/evicted from the Datastore as they happen, as reported to us by
   * SaveItem notifications.
   */
  nsDataHashtable<nsStringHashKey, LSValue> mValues;
  /**
   * Latched state of mDatastore's keys during a SaveItem notification with
   * aAffectsOrder=true.  The ordered keys needed to be saved off so that a
   * consistent ordering could be presented to the child LSSnapshot when it asks
   * for them via RecvLoadKeys.
   */
  nsTArray<nsString> mKeys;
  nsString mDocumentURI;
  /**
   * The index used for restoring iteration over not yet sent key/value pairs to
   * the child LSSnapshot.
   */
  uint32_t mNextLoadIndex;
  /**
   * The number of key/value pairs that were present in the Datastore at the
   * time the snapshot was created.  Once we have sent this many values to the
   * child LSSnapshot, we can infer that it has received all of the keys/values
   * and set mLoadedAllItems to true and clear mLoadedItems and mUnknownItems.
   * Note that knowing the keys/values is not the same as knowing their ordering
   * and so mKeys may be retained.
   */
  uint32_t mTotalLength;
  int64_t mUsage;
  int64_t mPeakUsage;
  /**
   * True if SaveItem has saved mDatastore's keys into mKeys because a SaveItem
   * notification with aAffectsOrder=true was received.
   */
  bool mSavedKeys;
  bool mActorDestroyed;
  bool mFinishReceived;
  bool mLoadedReceived;
  /**
   * True if LSSnapshot's mLoadState should be LoadState::AllOrderedItems or
   * LoadState::AllUnorderedItems.  It will be AllOrderedItems if the initial
   * snapshot contained all the data or if the state was AllOrderedKeys and
   * successive RecvLoadValueAndMoreItems requests have resulted in the
   * LSSnapshot being told all of the key/value pairs.  It will be
   * AllUnorderedItems if the state was LoadState::Partial and successive
   * RecvLoadValueAndMoreItem requests got all the keys/values but the key
   * ordering was not retrieved.
   */
  bool mLoadedAllItems;
  /**
   * True if LSSnapshot's mLoadState should be LoadState::AllOrderedItems or
   * AllOrderedKeys.  This can occur because of the initial snapshot, or because
   * a RecvLoadKeys request was received.
   */
  bool mLoadKeysReceived;
  bool mSentMarkDirty;

  bool mHasOtherProcessObservers;

 public:
  // Created in AllocPBackgroundLSSnapshotParent.
  Snapshot(Database* aDatabase, const nsAString& aDocumentURI);

  void Init(nsTHashtable<nsStringHashKey>& aLoadedItems,
            nsTHashtable<nsStringHashKey>& aUnknownItems,
            uint32_t aNextLoadIndex, uint32_t aTotalLength,
            int64_t aInitialUsage, int64_t aPeakUsage,
            LSSnapshot::LoadState aLoadState, bool aHasOtherProcessObservers) {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aInitialUsage >= 0);
    MOZ_ASSERT(aPeakUsage >= aInitialUsage);
    MOZ_ASSERT_IF(aLoadState != LSSnapshot::LoadState::AllOrderedItems,
                  aNextLoadIndex < aTotalLength);
    MOZ_ASSERT(mTotalLength == 0);
    MOZ_ASSERT(mUsage == -1);
    MOZ_ASSERT(mPeakUsage == -1);

    mLoadedItems.SwapElements(aLoadedItems);
    mUnknownItems.SwapElements(aUnknownItems);
    mNextLoadIndex = aNextLoadIndex;
    mTotalLength = aTotalLength;
    mUsage = aInitialUsage;
    mPeakUsage = aPeakUsage;
    if (aLoadState == LSSnapshot::LoadState::AllOrderedKeys) {
      MOZ_ASSERT(mUnknownItems.Count() == 0);
      mLoadKeysReceived = true;
    } else if (aLoadState == LSSnapshot::LoadState::AllOrderedItems) {
      MOZ_ASSERT(mLoadedItems.Count() == 0);
      MOZ_ASSERT(mUnknownItems.Count() == 0);
      MOZ_ASSERT(mNextLoadIndex == mTotalLength);
      mLoadedReceived = true;
      mLoadedAllItems = true;
      mLoadKeysReceived = true;
    }
    mHasOtherProcessObservers = aHasOtherProcessObservers;
  }

  /**
   * Called via NotifySnapshots by Datastore whenever it is updating its
   * internal state so that snapshots can save off the state of a value at the
   * time of their creation.
   */
  void SaveItem(const nsAString& aKey, const LSValue& aOldValue,
                bool aAffectsOrder);

  void MarkDirty();

  bool IsDirty() const {
    AssertIsOnBackgroundThread();

    return mSentMarkDirty;
  }

  bool HasOtherProcessObservers() const {
    AssertIsOnBackgroundThread();

    return mHasOtherProcessObservers;
  }

  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::Snapshot)

 private:
  // Reference counted.
  ~Snapshot();

  void Finish();

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvDeleteMe() override;

  mozilla::ipc::IPCResult RecvCheckpoint(
      nsTArray<LSWriteInfo>&& aWriteInfos) override;

  mozilla::ipc::IPCResult RecvCheckpointAndNotify(
      nsTArray<LSWriteAndNotifyInfo>&& aWriteAndNotifyInfos) override;

  mozilla::ipc::IPCResult RecvFinish() override;

  mozilla::ipc::IPCResult RecvLoaded() override;

  mozilla::ipc::IPCResult RecvLoadValueAndMoreItems(
      const nsString& aKey, LSValue* aValue,
      nsTArray<LSItemInfo>* aItemInfos) override;

  mozilla::ipc::IPCResult RecvLoadKeys(nsTArray<nsString>* aKeys) override;

  mozilla::ipc::IPCResult RecvIncreasePeakUsage(const int64_t& aRequestedSize,
                                                const int64_t& aMinSize,
                                                int64_t* aSize) override;

  mozilla::ipc::IPCResult RecvPing() override;
};

class Observer final : public PBackgroundLSObserverParent {
  nsCString mOrigin;
  bool mActorDestroyed;

 public:
  // Created in AllocPBackgroundLSObserverParent.
  explicit Observer(const nsACString& aOrigin);

  const nsCString& Origin() const { return mOrigin; }

  void Observe(Database* aDatabase, const nsString& aDocumentURI,
               const nsString& aKey, const LSValue& aOldValue,
               const LSValue& aNewValue);

  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::Observer)

 private:
  // Reference counted.
  ~Observer();

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvDeleteMe() override;
};

class LSRequestBase : public DatastoreOperationBase,
                      public PBackgroundLSRequestParent {
 protected:
  enum class State {
    // Just created on the PBackground thread. Next step is StartingRequest.
    Initial,

    // Waiting to start/starting request on the PBackground thread. Next step is
    // either Nesting if a subclass needs to process more nested states or
    // SendingReadyMessage if a subclass doesn't need any nested processing.
    StartingRequest,

    // Doing nested processing.
    Nesting,

    // Waiting to send/sending the ready message on the PBackground thread. Next
    // step is WaitingForFinish.
    SendingReadyMessage,

    // Waiting for the finish message on the PBackground thread. Next step is
    // SendingResults.
    WaitingForFinish,

    // Waiting to send/sending results on the PBackground thread. Next step is
    // Completed.
    SendingResults,

    // All done.
    Completed
  };

  nsCOMPtr<nsIEventTarget> mMainEventTarget;
  const LSRequestParams mParams;
  Maybe<ContentParentId> mContentParentId;
  State mState;
  bool mWaitingForFinish;

 public:
  LSRequestBase(nsIEventTarget* aMainEventTarget,
                const LSRequestParams& aParams,
                const Maybe<ContentParentId>& aContentParentId);

  void StringifyState(nsCString& aResult) const;

  void Dispatch();

 protected:
  ~LSRequestBase() override;

  virtual nsresult Start() = 0;

  virtual nsresult NestedRun();

  virtual void GetResponse(LSRequestResponse& aResponse) = 0;

  virtual void Cleanup() {}

  void LogState();

  virtual void LogNestedState() {}

 private:
  bool VerifyRequestParams();

  nsresult StartRequest();

  void SendReadyMessage();

  nsresult SendReadyMessageInternal();

  void Finish();

  void FinishInternal();

  void SendResults();

 protected:
  // Common nsIRunnable implementation that subclasses may not override.
  NS_IMETHOD
  Run() final;

  // IPDL methods.
  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  mozilla::ipc::IPCResult RecvCancel() final;

  mozilla::ipc::IPCResult RecvFinish() final;
};

class PrepareDatastoreOp
    : public LSRequestBase,
      public OpenDirectoryListener,
      public SupportsCheckedUnsafePtr<CheckIf<DiagnosticAssertEnabled>> {
  class LoadDataOp;

  class CompressFunction;
  class CompressibleFunction;

  enum class NestedState {
    // The nesting has not yet taken place. Next step is
    // CheckExistingOperations.
    BeforeNesting,

    // Checking if a prepare datastore operation is already running for given
    // origin on the PBackground thread. Next step is CheckClosingDatastore.
    CheckExistingOperations,

    // Checking if a datastore is closing the connection for given origin on
    // the PBackground thread. Next step is PreparationPending.
    CheckClosingDatastore,

    // Opening directory or initializing quota manager on the PBackground
    // thread. Next step is either DirectoryOpenPending if quota manager is
    // already initialized or QuotaManagerPending if quota manager needs to be
    // initialized.
    // If a datastore already exists for given origin then the next state is
    // SendingReadyMessage.
    PreparationPending,

    // Waiting for quota manager initialization to complete on the PBackground
    // thread. Next step is either SendingReadyMessage if initialization failed
    // or DirectoryOpenPending if initialization succeeded.
    QuotaManagerPending,

    // Waiting for directory open allowed on the PBackground thread. The next
    // step is either SendingReadyMessage if directory lock failed to acquire,
    // or DatabaseWorkOpen if directory lock is acquired.
    DirectoryOpenPending,

    // Waiting to do/doing work on the QuotaManager IO thread. Its next step is
    // BeginLoadData.
    DatabaseWorkOpen,

    // Starting a load data operation on the PBackground thread. Next step is
    // DatabaseWorkLoadData.
    BeginLoadData,

    // Waiting to do/doing work on the connection thread. This involves waiting
    // for the LoadDataOp to do its work. Eventually the state will transition
    // to SendingReadyMessage.
    DatabaseWorkLoadData,

    // The nesting has completed.
    AfterNesting
  };

  nsCOMPtr<nsIEventTarget> mMainEventTarget;
  RefPtr<PrepareDatastoreOp> mDelayedOp;
  RefPtr<DirectoryLock> mPendingDirectoryLock;
  RefPtr<DirectoryLock> mDirectoryLock;
  RefPtr<Connection> mConnection;
  RefPtr<Datastore> mDatastore;
  nsAutoPtr<ArchivedOriginScope> mArchivedOriginScope;
  LoadDataOp* mLoadDataOp;
  nsDataHashtable<nsStringHashKey, LSValue> mValues;
  nsTArray<LSItemInfo> mOrderedItems;
  nsCString mSuffix;
  nsCString mGroup;
  nsCString mMainThreadOrigin;
  nsCString mOrigin;
  nsString mDatabaseFilePath;
  uint32_t mPrivateBrowsingId;
  int64_t mUsage;
  int64_t mSizeOfKeys;
  int64_t mSizeOfItems;
  uint64_t mDatastoreId;
  NestedState mNestedState;
  const bool mForPreload;
  bool mDatabaseNotAvailable;
  bool mRequestedDirectoryLock;
  bool mInvalidated;

#ifdef DEBUG
  int64_t mDEBUGUsage;
#endif

 public:
  PrepareDatastoreOp(nsIEventTarget* aMainEventTarget,
                     const LSRequestParams& aParams,
                     const Maybe<ContentParentId>& aContentParentId);

  bool OriginIsKnown() const {
    AssertIsOnOwningThread();

    return !mOrigin.IsEmpty();
  }

  const nsCString& Origin() const {
    AssertIsOnOwningThread();
    MOZ_ASSERT(OriginIsKnown());

    return mOrigin;
  }

  void StringifyNestedState(nsCString& aResult) const;

  bool RequestedDirectoryLock() const {
    AssertIsOnOwningThread();

    return mRequestedDirectoryLock;
  }

  void Invalidate() {
    AssertIsOnOwningThread();

    mInvalidated = true;
  }

 private:
  ~PrepareDatastoreOp() override;

  nsresult Start() override;

  nsresult CheckExistingOperations();

  nsresult CheckClosingDatastoreInternal();

  nsresult CheckClosingDatastore();

  nsresult BeginDatastorePreparationInternal();

  nsresult BeginDatastorePreparation();

  nsresult QuotaManagerOpen();

  nsresult OpenDirectory();

  void SendToIOThread();

  nsresult DatabaseWork();

  nsresult DatabaseNotAvailable();

  nsresult EnsureDirectoryEntry(nsIFile* aEntry, bool aCreateIfNotExists,
                                bool aDirectory,
                                bool* aAlreadyExisted = nullptr);

  nsresult VerifyDatabaseInformation(mozIStorageConnection* aConnection);

  already_AddRefed<QuotaObject> GetQuotaObject();

  nsresult BeginLoadData();

  void FinishNesting();

  nsresult FinishNestingOnNonOwningThread();

  nsresult NestedRun() override;

  void GetResponse(LSRequestResponse& aResponse) override;

  void Cleanup() override;

  void ConnectionClosedCallback();

  void CleanupMetadata();

  void LogNestedState() override;

  NS_DECL_ISUPPORTS_INHERITED

  // IPDL overrides.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  // OpenDirectoryListener overrides.
  void DirectoryLockAcquired(DirectoryLock* aLock) override;

  void DirectoryLockFailed() override;
};

class PrepareDatastoreOp::LoadDataOp final
    : public ConnectionDatastoreOperationBase {
  RefPtr<PrepareDatastoreOp> mPrepareDatastoreOp;

 public:
  explicit LoadDataOp(PrepareDatastoreOp* aPrepareDatastoreOp)
      : ConnectionDatastoreOperationBase(aPrepareDatastoreOp->mConnection),
        mPrepareDatastoreOp(aPrepareDatastoreOp) {}

 private:
  ~LoadDataOp() = default;

  nsresult DoDatastoreWork() override;

  void OnSuccess() override;

  void OnFailure(nsresult aResultCode) override;

  void Cleanup() override;
};

class PrepareDatastoreOp::CompressFunction final : public mozIStorageFunction {
 private:
  ~CompressFunction() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION
};

class PrepareDatastoreOp::CompressibleFunction final
    : public mozIStorageFunction {
 private:
  ~CompressibleFunction() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION
};

class PrepareObserverOp : public LSRequestBase {
  nsCString mOrigin;

 public:
  PrepareObserverOp(nsIEventTarget* aMainEventTarget,
                    const LSRequestParams& aParams,
                    const Maybe<ContentParentId>& aContentParentId);

 private:
  nsresult Start() override;

  void GetResponse(LSRequestResponse& aResponse) override;
};

class LSSimpleRequestBase : public DatastoreOperationBase,
                            public PBackgroundLSSimpleRequestParent {
 protected:
  enum class State {
    // Just created on the PBackground thread. Next step is StartingRequest.
    Initial,

    // Waiting to start/starting request on the PBackground thread. Next step is
    // SendingResults.
    StartingRequest,

    // Waiting to send/sending results on the PBackground thread. Next step is
    // Completed.
    SendingResults,

    // All done.
    Completed
  };

  const LSSimpleRequestParams mParams;
  Maybe<ContentParentId> mContentParentId;
  State mState;

 public:
  LSSimpleRequestBase(const LSSimpleRequestParams& aParams,
                      const Maybe<ContentParentId>& aContentParentId);

  void Dispatch();

 protected:
  ~LSSimpleRequestBase() override;

  virtual nsresult Start() = 0;

  virtual void GetResponse(LSSimpleRequestResponse& aResponse) = 0;

 private:
  bool VerifyRequestParams();

  nsresult StartRequest();

  void SendResults();

  // Common nsIRunnable implementation that subclasses may not override.
  NS_IMETHOD
  Run() final;

  // IPDL methods.
  void ActorDestroy(ActorDestroyReason aWhy) override;
};

class PreloadedOp : public LSSimpleRequestBase {
  nsCString mOrigin;

 public:
  PreloadedOp(const LSSimpleRequestParams& aParams,
              const Maybe<ContentParentId>& aContentParentId);

 private:
  nsresult Start() override;

  void GetResponse(LSSimpleRequestResponse& aResponse) override;
};

/*******************************************************************************
 * Other class declarations
 ******************************************************************************/

struct ArchivedOriginInfo {
  OriginAttributes mOriginAttributes;
  nsCString mOriginNoSuffix;

  ArchivedOriginInfo(const OriginAttributes& aOriginAttributes,
                     const nsACString& aOriginNoSuffix)
      : mOriginAttributes(aOriginAttributes),
        mOriginNoSuffix(aOriginNoSuffix) {}
};

class ArchivedOriginScope {
  struct Origin {
    nsCString mOriginSuffix;
    nsCString mOriginNoSuffix;

    Origin(const nsACString& aOriginSuffix, const nsACString& aOriginNoSuffix)
        : mOriginSuffix(aOriginSuffix), mOriginNoSuffix(aOriginNoSuffix) {}

    const nsACString& OriginSuffix() const { return mOriginSuffix; }

    const nsACString& OriginNoSuffix() const { return mOriginNoSuffix; }
  };

  struct Prefix {
    nsCString mOriginNoSuffix;

    explicit Prefix(const nsACString& aOriginNoSuffix)
        : mOriginNoSuffix(aOriginNoSuffix) {}

    const nsACString& OriginNoSuffix() const { return mOriginNoSuffix; }
  };

  struct Pattern {
    UniquePtr<OriginAttributesPattern> mPattern;

    explicit Pattern(const OriginAttributesPattern& aPattern)
        : mPattern(MakeUnique<OriginAttributesPattern>(aPattern)) {}

    Pattern(const Pattern& aOther)
        : mPattern(MakeUnique<OriginAttributesPattern>(*aOther.mPattern)) {}

    Pattern(Pattern&& aOther) = default;

    const OriginAttributesPattern& GetPattern() const {
      MOZ_ASSERT(mPattern);
      return *mPattern;
    }
  };

  struct Null {};

  using DataType = Variant<Origin, Pattern, Prefix, Null>;

  DataType mData;

 public:
  static ArchivedOriginScope* CreateFromOrigin(
      const nsACString& aOriginAttrSuffix, const nsACString& aOriginKey);

  static ArchivedOriginScope* CreateFromPrefix(const nsACString& aOriginKey);

  static ArchivedOriginScope* CreateFromPattern(
      const OriginAttributesPattern& aPattern);

  static ArchivedOriginScope* CreateFromNull();

  bool IsOrigin() const { return mData.is<Origin>(); }

  bool IsPrefix() const { return mData.is<Prefix>(); }

  bool IsPattern() const { return mData.is<Pattern>(); }

  bool IsNull() const { return mData.is<Null>(); }

  const nsACString& OriginSuffix() const {
    MOZ_ASSERT(IsOrigin());

    return mData.as<Origin>().OriginSuffix();
  }

  const nsACString& OriginNoSuffix() const {
    MOZ_ASSERT(IsOrigin() || IsPrefix());

    if (IsOrigin()) {
      return mData.as<Origin>().OriginNoSuffix();
    }
    return mData.as<Prefix>().OriginNoSuffix();
  }

  const OriginAttributesPattern& GetPattern() const {
    MOZ_ASSERT(IsPattern());

    return mData.as<Pattern>().GetPattern();
  }

  void GetBindingClause(nsACString& aBindingClause) const;

  nsresult BindToStatement(mozIStorageStatement* aStatement) const;

  bool HasMatches(ArchivedOriginHashtable* aHashtable) const;

  void RemoveMatches(ArchivedOriginHashtable* aHashtable) const;

 private:
  // Move constructors
  explicit ArchivedOriginScope(const Origin&& aOrigin) : mData(aOrigin) {}

  explicit ArchivedOriginScope(const Pattern&& aPattern) : mData(aPattern) {}

  explicit ArchivedOriginScope(const Prefix&& aPrefix) : mData(aPrefix) {}

  explicit ArchivedOriginScope(const Null&& aNull) : mData(aNull) {}
};

class QuotaClient final : public mozilla::dom::quota::Client {
  class Observer;
  class MatchFunction;

  static QuotaClient* sInstance;

  Mutex mShadowDatabaseMutex;
  bool mShutdownRequested;

 public:
  QuotaClient();

  static nsresult Initialize();

  static QuotaClient* GetInstance() {
    AssertIsOnBackgroundThread();

    return sInstance;
  }

  static bool IsShuttingDownOnBackgroundThread() {
    AssertIsOnBackgroundThread();

    if (sInstance) {
      return sInstance->IsShuttingDown();
    }

    return QuotaManager::IsShuttingDown();
  }

  static bool IsShuttingDownOnNonBackgroundThread() {
    MOZ_ASSERT(!IsOnBackgroundThread());

    return QuotaManager::IsShuttingDown();
  }

  mozilla::Mutex& ShadowDatabaseMutex() {
    MOZ_ASSERT(IsOnIOThread() || IsOnConnectionThread());

    return mShadowDatabaseMutex;
  }

  bool IsShuttingDown() const {
    AssertIsOnBackgroundThread();

    return mShutdownRequested;
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::dom::QuotaClient, override)

  Type GetType() override;

  nsresult InitOrigin(PersistenceType aPersistenceType,
                      const nsACString& aGroup, const nsACString& aOrigin,
                      const AtomicBool& aCanceled, UsageInfo* aUsageInfo,
                      bool aForGetUsage) override;

  nsresult GetUsageForOrigin(PersistenceType aPersistenceType,
                             const nsACString& aGroup,
                             const nsACString& aOrigin,
                             const AtomicBool& aCanceled,
                             UsageInfo* aUsageInfo) override;

  nsresult AboutToClearOrigins(
      const Nullable<PersistenceType>& aPersistenceType,
      const OriginScope& aOriginScope) override;

  void OnOriginClearCompleted(PersistenceType aPersistenceType,
                              const nsACString& aOrigin) override;

  void ReleaseIOThreadObjects() override;

  void AbortOperations(const nsACString& aOrigin) override;

  void AbortOperationsForProcess(ContentParentId aContentParentId) override;

  void StartIdleMaintenance() override;

  void StopIdleMaintenance() override;

  void ShutdownWorkThreads() override;

 private:
  ~QuotaClient() override;

  void ShutdownTimedOut();

  nsresult CreateArchivedOriginScope(
      const OriginScope& aOriginScope,
      nsAutoPtr<ArchivedOriginScope>& aArchivedOriginScope);

  nsresult PerformDelete(mozIStorageConnection* aConnection,
                         const nsACString& aSchemaName,
                         ArchivedOriginScope* aArchivedOriginScope) const;
};

class QuotaClient::Observer final : public nsIObserver {
 public:
  static nsresult Initialize();

 private:
  Observer() { MOZ_ASSERT(NS_IsMainThread()); }

  ~Observer() { MOZ_ASSERT(NS_IsMainThread()); }

  nsresult Init();

  nsresult Shutdown();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

class QuotaClient::MatchFunction final : public mozIStorageFunction {
  OriginAttributesPattern mPattern;

 public:
  explicit MatchFunction(const OriginAttributesPattern& aPattern)
      : mPattern(aPattern) {}

 private:
  ~MatchFunction() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION
};

/*******************************************************************************
 * Helper classes
 ******************************************************************************/

class MOZ_STACK_CLASS AutoWriteTransaction final {
  Connection* mConnection;
  Maybe<MutexAutoLock> mShadowDatabaseLock;
  bool mShadowWrites;

 public:
  explicit AutoWriteTransaction(bool aShadowWrites);

  ~AutoWriteTransaction();

  nsresult Start(Connection* aConnection);

  nsresult Commit();

 private:
  nsresult LockAndAttachShadowDatabase(Connection* aConnection);

  nsresult DetachShadowDatabaseAndUnlock();
};

/*******************************************************************************
 * Globals
 ******************************************************************************/

#ifdef DEBUG
bool gLocalStorageInitialized = false;
#endif

typedef nsTArray<CheckedUnsafePtr<PrepareDatastoreOp>> PrepareDatastoreOpArray;

StaticAutoPtr<PrepareDatastoreOpArray> gPrepareDatastoreOps;

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
typedef std::conditional<DiagnosticAssertEnabled::value, nsCStringHashKeyDM,
                         nsCStringHashKey>::type DatastoreHashKey;

typedef nsDataHashtable<DatastoreHashKey, CheckedUnsafePtr<Datastore>>
    DatastoreHashtable;

StaticAutoPtr<DatastoreHashtable> gDatastores;

uint64_t gLastDatastoreId = 0;

typedef nsClassHashtable<nsUint64HashKey, PreparedDatastore>
    PreparedDatastoreHashtable;

StaticAutoPtr<PreparedDatastoreHashtable> gPreparedDatastores;

typedef nsTArray<CheckedUnsafePtr<Database>> LiveDatabaseArray;

StaticAutoPtr<LiveDatabaseArray> gLiveDatabases;

StaticRefPtr<ConnectionThread> gConnectionThread;

uint64_t gLastObserverId = 0;

typedef nsRefPtrHashtable<nsUint64HashKey, Observer> PreparedObserverHashtable;

StaticAutoPtr<PreparedObserverHashtable> gPreparedObsevers;

typedef nsClassHashtable<nsCStringHashKey, nsTArray<Observer*>>
    ObserverHashtable;

StaticAutoPtr<ObserverHashtable> gObservers;

Atomic<bool> gNextGen(kDefaultNextGen);
Atomic<bool> gShadowWrites(kDefaultShadowWrites);
Atomic<int32_t, Relaxed> gSnapshotPrefill(kDefaultSnapshotPrefill);
Atomic<int32_t, Relaxed> gSnapshotGradualPrefill(
    kDefaultSnapshotGradualPrefill);
Atomic<bool> gClientValidation(kDefaultClientValidation);

typedef nsDataHashtable<nsCStringHashKey, int64_t> UsageHashtable;

StaticAutoPtr<ArchivedOriginHashtable> gArchivedOrigins;

// Can only be touched on the Quota Manager I/O thread.
bool gInitializedShadowStorage = false;

bool IsOnConnectionThread() {
  MOZ_ASSERT(gConnectionThread);
  return gConnectionThread->IsOnConnectionThread();
}

void AssertIsOnConnectionThread() {
  MOZ_ASSERT(gConnectionThread);
  gConnectionThread->AssertIsOnConnectionThread();
}

already_AddRefed<Datastore> GetDatastore(const nsACString& aOrigin) {
  AssertIsOnBackgroundThread();

  if (gDatastores) {
    CheckedUnsafePtr<Datastore> datastore;
    if (gDatastores->Get(aOrigin, &datastore)) {
      MOZ_ASSERT(datastore);

      RefPtr<Datastore> result(datastore);
      return result.forget();
    }
  }

  return nullptr;
}

nsresult LoadArchivedOrigins() {
  AssertIsOnIOThread();
  MOZ_ASSERT(!gArchivedOrigins);

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  // Ensure that the webappsstore.sqlite is moved to new place.
  nsresult rv = quotaManager->EnsureStorageIsInitialized();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = CreateArchiveStorageConnection(quotaManager->GetStoragePath(),
                                      getter_AddRefs(connection));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!connection) {
    gArchivedOrigins = new ArchivedOriginHashtable();
    return NS_OK;
  }

  nsCOMPtr<mozIStorageStatement> stmt;
  rv = connection->CreateStatement(
      NS_LITERAL_CSTRING("SELECT DISTINCT originAttributes, originKey "
                         "FROM webappsstore2;"),
      getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoPtr<ArchivedOriginHashtable> archivedOrigins(
      new ArchivedOriginHashtable());

  bool hasResult;
  while (NS_SUCCEEDED(rv = stmt->ExecuteStep(&hasResult)) && hasResult) {
    nsCString originSuffix;
    rv = stmt->GetUTF8String(0, originSuffix);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCString originNoSuffix;
    rv = stmt->GetUTF8String(1, originNoSuffix);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCString hashKey = GetArchivedOriginHashKey(originSuffix, originNoSuffix);

    OriginAttributes originAttributes;
    if (NS_WARN_IF(!originAttributes.PopulateFromSuffix(originSuffix))) {
      return NS_ERROR_FAILURE;
    }

    nsAutoPtr<ArchivedOriginInfo> archivedOriginInfo(
        new ArchivedOriginInfo(originAttributes, originNoSuffix));

    archivedOrigins->Put(hashKey, archivedOriginInfo.forget());
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  gArchivedOrigins = archivedOrigins.forget();
  return NS_OK;
}

nsresult GetUsage(mozIStorageConnection* aConnection,
                  ArchivedOriginScope* aArchivedOriginScope, int64_t* aUsage) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(aUsage);

  nsresult rv;

  nsCOMPtr<mozIStorageStatement> stmt;
  if (aArchivedOriginScope) {
    rv = aConnection->CreateStatement(
        NS_LITERAL_CSTRING("SELECT "
                           "total(utf16Length(key) + utf16Length(value)) "
                           "FROM webappsstore2 "
                           "WHERE originKey = :originKey "
                           "AND originAttributes = :originAttributes;"),
        getter_AddRefs(stmt));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = aArchivedOriginScope->BindToStatement(stmt);
  } else {
    rv = aConnection->CreateStatement(NS_LITERAL_CSTRING("SELECT usage "
                                                         "FROM database"),
                                      getter_AddRefs(stmt));
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!hasResult)) {
    return NS_ERROR_FAILURE;
  }

  int64_t usage;
  rv = stmt->GetInt64(0, &usage);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aUsage = usage;
  return NS_OK;
}

void ShadowWritesPrefChangedCallback(const char* aPrefName, void* aClosure) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!strcmp(aPrefName, kShadowWritesPref));
  MOZ_ASSERT(!aClosure);

  gShadowWrites = Preferences::GetBool(aPrefName, kDefaultShadowWrites);
}

void SnapshotPrefillPrefChangedCallback(const char* aPrefName, void* aClosure) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!strcmp(aPrefName, kSnapshotPrefillPref));
  MOZ_ASSERT(!aClosure);

  int32_t snapshotPrefill =
      Preferences::GetInt(aPrefName, kDefaultSnapshotPrefill);

  // The magic -1 is for use only by tests.
  if (snapshotPrefill == -1) {
    snapshotPrefill = INT32_MAX;
  }

  gSnapshotPrefill = snapshotPrefill;
}

void SnapshotGradualPrefillPrefChangedCallback(const char* aPrefName,
                                               void* aClosure) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!strcmp(aPrefName, kSnapshotGradualPrefillPref));
  MOZ_ASSERT(!aClosure);

  int32_t snapshotGradualPrefill =
      Preferences::GetInt(aPrefName, kDefaultSnapshotGradualPrefill);

  // The magic -1 is for use only by tests.
  if (snapshotGradualPrefill == -1) {
    snapshotGradualPrefill = INT32_MAX;
  }

  gSnapshotGradualPrefill = snapshotGradualPrefill;
}

void ClientValidationPrefChangedCallback(const char* aPrefName,
                                         void* aClosure) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!strcmp(aPrefName, kClientValidationPref));
  MOZ_ASSERT(!aClosure);

  gClientValidation = Preferences::GetBool(aPrefName, kDefaultClientValidation);
}

template <typename P>
void RequestAllowToCloseIf(P aPredicate) {
  AssertIsOnBackgroundThread();

  if (!gLiveDatabases) {
    return;
  }

  nsTArray<RefPtr<Database>> databases;

  for (Database* database : *gLiveDatabases) {
    if (aPredicate(database)) {
      databases.AppendElement(database);
    }
  }

  for (Database* database : databases) {
    database->RequestAllowToClose();
  }

  databases.Clear();
}

bool VerifyPrincipalInfo(const PrincipalInfo& aPrincipalInfo,
                         const PrincipalInfo& aStoragePrincipalInfo) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!QuotaManager::IsPrincipalInfoValid(aPrincipalInfo))) {
    return false;
  }

  if (NS_WARN_IF(!StoragePrincipalHelper::
                     VerifyValidStoragePrincipalInfoForPrincipalInfo(
                         aStoragePrincipalInfo, aPrincipalInfo))) {
    return false;
  }

  return true;
}

bool VerifyClientId(const Maybe<ContentParentId>& aContentParentId,
                    const PrincipalInfo& aPrincipalInfo,
                    const Maybe<nsID>& aClientId) {
  AssertIsOnBackgroundThread();

  if (gClientValidation) {
    if (NS_WARN_IF(aClientId.isNothing())) {
      return false;
    }

    RefPtr<ClientManagerService> svc = ClientManagerService::GetInstance();
    if (svc && NS_WARN_IF(!svc->HasWindow(aContentParentId, aPrincipalInfo,
                                          aClientId.ref()))) {
      return false;
    }
  }

  return true;
}

bool VerifyOriginKey(const nsACString& aOriginKey,
                     const PrincipalInfo& aPrincipalInfo) {
  AssertIsOnBackgroundThread();

  nsCString originAttrSuffix;
  nsCString originKey;
  nsresult rv = GenerateOriginKey2(aPrincipalInfo, originAttrSuffix, originKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  if (NS_WARN_IF(originKey != aOriginKey)) {
    LS_WARNING("originKey (%s) doesn't match passed one (%s)!", originKey.get(),
               nsCString(aOriginKey).get());
    return false;
  }

  return true;
}

}  // namespace

/*******************************************************************************
 * Exported functions
 ******************************************************************************/

void InitializeLocalStorage() {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!gLocalStorageInitialized);

  if (!QuotaManager::IsRunningGTests()) {
    // This service has to be started on the main thread currently.
    nsCOMPtr<mozIStorageService> ss;
    if (NS_WARN_IF(!(ss = do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID)))) {
      NS_WARNING("Failed to get storage service!");
    }
  }

  if (NS_FAILED(QuotaClient::Initialize())) {
    NS_WARNING("Failed to initialize quota client!");
  }

  Preferences::AddAtomicBoolVarCache(&gNextGen, kNextGenPref, kDefaultNextGen);

  Preferences::RegisterCallbackAndCall(ShadowWritesPrefChangedCallback,
                                       kShadowWritesPref);

  Preferences::RegisterCallbackAndCall(SnapshotPrefillPrefChangedCallback,
                                       kSnapshotPrefillPref);

  Preferences::RegisterCallbackAndCall(
      SnapshotGradualPrefillPrefChangedCallback, kSnapshotGradualPrefillPref);

  Preferences::RegisterCallbackAndCall(ClientValidationPrefChangedCallback,
                                       kClientValidationPref);

#ifdef DEBUG
  gLocalStorageInitialized = true;
#endif
}

bool GetCurrentNextGenPrefValue() { return gNextGen; }

PBackgroundLSDatabaseParent* AllocPBackgroundLSDatabaseParent(
    const PrincipalInfo& aPrincipalInfo, const uint32_t& aPrivateBrowsingId,
    const uint64_t& aDatastoreId) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread())) {
    return nullptr;
  }

  if (NS_WARN_IF(!gPreparedDatastores)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  PreparedDatastore* preparedDatastore = gPreparedDatastores->Get(aDatastoreId);
  if (NS_WARN_IF(!preparedDatastore)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  // If we ever decide to return null from this point on, we need to make sure
  // that the datastore is closed and the prepared datastore is removed from the
  // gPreparedDatastores hashtable.
  // We also assume that IPDL must call RecvPBackgroundLSDatabaseConstructor
  // once we return a valid actor in this method.

  RefPtr<Database> database =
      new Database(aPrincipalInfo, preparedDatastore->GetContentParentId(),
                   preparedDatastore->Origin(), aPrivateBrowsingId);

  // Transfer ownership to IPDL.
  return database.forget().take();
}

bool RecvPBackgroundLSDatabaseConstructor(PBackgroundLSDatabaseParent* aActor,
                                          const PrincipalInfo& aPrincipalInfo,
                                          const uint32_t& aPrivateBrowsingId,
                                          const uint64_t& aDatastoreId) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(gPreparedDatastores);
  MOZ_ASSERT(gPreparedDatastores->Get(aDatastoreId));
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());

  // The actor is now completely built (it has a manager, channel and it's
  // registered as a subprotocol).
  // ActorDestroy will be called if we fail here.

  nsAutoPtr<PreparedDatastore> preparedDatastore;
  gPreparedDatastores->Remove(aDatastoreId, &preparedDatastore);
  MOZ_ASSERT(preparedDatastore);

  auto* database = static_cast<Database*>(aActor);

  database->SetActorAlive(preparedDatastore->GetDatastore());

  // It's possible that AbortOperations was called before the database actor
  // was created and became live. Let the child know that the database in no
  // longer valid.
  if (preparedDatastore->IsInvalidated()) {
    database->RequestAllowToClose();
  }

  return true;
}

bool DeallocPBackgroundLSDatabaseParent(PBackgroundLSDatabaseParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<Database> actor = dont_AddRef(static_cast<Database*>(aActor));

  return true;
}

PBackgroundLSObserverParent* AllocPBackgroundLSObserverParent(
    const uint64_t& aObserverId) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread())) {
    return nullptr;
  }

  if (NS_WARN_IF(!gPreparedObsevers)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  RefPtr<Observer> observer = gPreparedObsevers->Get(aObserverId);
  if (NS_WARN_IF(!observer)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  // observer->SetObject(this);

  // Transfer ownership to IPDL.
  return observer.forget().take();
}

bool RecvPBackgroundLSObserverConstructor(PBackgroundLSObserverParent* aActor,
                                          const uint64_t& aObserverId) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(gPreparedObsevers);
  MOZ_ASSERT(gPreparedObsevers->GetWeak(aObserverId));

  RefPtr<Observer> observer;
  gPreparedObsevers->Remove(aObserverId, observer.StartAssignment());
  MOZ_ASSERT(observer);

  if (!gPreparedObsevers->Count()) {
    gPreparedObsevers = nullptr;
  }

  if (!gObservers) {
    gObservers = new ObserverHashtable();
  }

  nsTArray<Observer*>* array;
  if (!gObservers->Get(observer->Origin(), &array)) {
    array = new nsTArray<Observer*>();
    gObservers->Put(observer->Origin(), array);
  }
  array->AppendElement(observer);

  if (RefPtr<Datastore> datastore = GetDatastore(observer->Origin())) {
    datastore->NoteChangedObserverArray(*array);
  }

  return true;
}

bool DeallocPBackgroundLSObserverParent(PBackgroundLSObserverParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<Observer> actor = dont_AddRef(static_cast<Observer*>(aActor));

  return true;
}

PBackgroundLSRequestParent* AllocPBackgroundLSRequestParent(
    PBackgroundParent* aBackgroundActor, const LSRequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != LSRequestParams::T__None);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread())) {
    return nullptr;
  }

  Maybe<ContentParentId> contentParentId;

  uint64_t childID = BackgroundParent::GetChildID(aBackgroundActor);
  if (childID) {
    contentParentId = Some(ContentParentId(childID));
  }

  // If we're in the same process as the actor, we need to get the target event
  // queue from the current RequestHelper.
  nsCOMPtr<nsIEventTarget> mainEventTarget;
  if (!BackgroundParent::IsOtherProcessActor(aBackgroundActor)) {
    mainEventTarget = LSObject::GetSyncLoopEventTarget();
  }

  RefPtr<LSRequestBase> actor;

  switch (aParams.type()) {
    case LSRequestParams::TLSRequestPreloadDatastoreParams:
    case LSRequestParams::TLSRequestPrepareDatastoreParams: {
      RefPtr<PrepareDatastoreOp> prepareDatastoreOp =
          new PrepareDatastoreOp(mainEventTarget, aParams, contentParentId);

      if (!gPrepareDatastoreOps) {
        gPrepareDatastoreOps = new PrepareDatastoreOpArray();
      }
      gPrepareDatastoreOps->AppendElement(prepareDatastoreOp);

      actor = std::move(prepareDatastoreOp);

      break;
    }

    case LSRequestParams::TLSRequestPrepareObserverParams: {
      RefPtr<PrepareObserverOp> prepareObserverOp =
          new PrepareObserverOp(mainEventTarget, aParams, contentParentId);

      actor = std::move(prepareObserverOp);

      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

bool RecvPBackgroundLSRequestConstructor(PBackgroundLSRequestParent* aActor,
                                         const LSRequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != LSRequestParams::T__None);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());

  // The actor is now completely built.

  auto* op = static_cast<LSRequestBase*>(aActor);

  op->Dispatch();

  return true;
}

bool DeallocPBackgroundLSRequestParent(PBackgroundLSRequestParent* aActor) {
  AssertIsOnBackgroundThread();

  // Transfer ownership back from IPDL.
  RefPtr<LSRequestBase> actor =
      dont_AddRef(static_cast<LSRequestBase*>(aActor));

  return true;
}

PBackgroundLSSimpleRequestParent* AllocPBackgroundLSSimpleRequestParent(
    PBackgroundParent* aBackgroundActor, const LSSimpleRequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != LSSimpleRequestParams::T__None);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread())) {
    return nullptr;
  }

  Maybe<ContentParentId> contentParentId;

  uint64_t childID = BackgroundParent::GetChildID(aBackgroundActor);
  if (childID) {
    contentParentId = Some(ContentParentId(childID));
  }

  RefPtr<LSSimpleRequestBase> actor;

  switch (aParams.type()) {
    case LSSimpleRequestParams::TLSSimpleRequestPreloadedParams: {
      RefPtr<PreloadedOp> preloadedOp =
          new PreloadedOp(aParams, contentParentId);

      actor = std::move(preloadedOp);

      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

bool RecvPBackgroundLSSimpleRequestConstructor(
    PBackgroundLSSimpleRequestParent* aActor,
    const LSSimpleRequestParams& aParams) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != LSSimpleRequestParams::T__None);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());

  // The actor is now completely built.

  auto* op = static_cast<LSSimpleRequestBase*>(aActor);

  op->Dispatch();

  return true;
}

bool DeallocPBackgroundLSSimpleRequestParent(
    PBackgroundLSSimpleRequestParent* aActor) {
  AssertIsOnBackgroundThread();

  // Transfer ownership back from IPDL.
  RefPtr<LSSimpleRequestBase> actor =
      dont_AddRef(static_cast<LSSimpleRequestBase*>(aActor));

  return true;
}

bool RecvLSClearPrivateBrowsing() {
  AssertIsOnBackgroundThread();

  if (gDatastores) {
    for (auto iter = gDatastores->ConstIter(); !iter.Done(); iter.Next()) {
      Datastore* datastore = iter.Data();
      MOZ_ASSERT(datastore);

      if (datastore->PrivateBrowsingId()) {
        datastore->Clear(nullptr);
      }
    }
  }

  return true;
}

namespace localstorage {

already_AddRefed<mozilla::dom::quota::Client> CreateQuotaClient() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(CachedNextGenLocalStorageEnabled());

  RefPtr<QuotaClient> client = new QuotaClient();
  return client.forget();
}

}  // namespace localstorage

/*******************************************************************************
 * DatastoreWriteOptimizer
 ******************************************************************************/

void DatastoreWriteOptimizer::ApplyAndReset(
    nsTArray<LSItemInfo>& aOrderedItems) {
  AssertIsOnOwningThread();

  // The mWriteInfos hash table contains all write infos, but it keeps them in
  // an arbitrary order, which means write infos need to be sorted before being
  // processed. However, the order is not important for deletions and normal
  // updates. Usually, filtering out deletions and updates would require extra
  // work, but we have to check the hash table for each ordered item anyway, so
  // we can remove the write info if it is a deletion or update without adding
  // extra overhead. In the end, only insertions need to be sorted before being
  // processed.

  if (mTruncateInfo) {
    aOrderedItems.Clear();
    mTruncateInfo = nullptr;
  }

  for (int32_t index = aOrderedItems.Length() - 1; index >= 0; index--) {
    LSItemInfo& item = aOrderedItems[index];

    if (auto entry = mWriteInfos.Lookup(item.key())) {
      WriteInfo* writeInfo = entry.Data();

      switch (writeInfo->GetType()) {
        case WriteInfo::DeleteItem:
          aOrderedItems.RemoveElementAt(index);
          entry.Remove();
          break;

        case WriteInfo::UpdateItem: {
          auto updateItemInfo = static_cast<UpdateItemInfo*>(writeInfo);
          if (updateItemInfo->UpdateWithMove()) {
            // See the comment in LSWriteOptimizer::InsertItem for more details
            // about the UpdateWithMove flag.

            aOrderedItems.RemoveElementAt(index);
            entry.Data() = new InsertItemInfo(updateItemInfo->SerialNumber(),
                                              updateItemInfo->GetKey(),
                                              updateItemInfo->GetValue());
          } else {
            item.value() = updateItemInfo->GetValue();
            entry.Remove();
          }
          break;
        }

        case WriteInfo::InsertItem:
          break;

        default:
          MOZ_CRASH("Bad type!");
      }
    }
  }

  nsTArray<WriteInfo*> writeInfos;
  GetSortedWriteInfos(writeInfos);

  for (WriteInfo* writeInfo : writeInfos) {
    MOZ_ASSERT(writeInfo->GetType() == WriteInfo::InsertItem);

    auto insertItemInfo = static_cast<InsertItemInfo*>(writeInfo);

    LSItemInfo* itemInfo = aOrderedItems.AppendElement();
    itemInfo->key() = insertItemInfo->GetKey();
    itemInfo->value() = insertItemInfo->GetValue();
  }

  mWriteInfos.Clear();
}

/*******************************************************************************
 * ConnectionWriteOptimizer
 ******************************************************************************/

nsresult ConnectionWriteOptimizer::Perform(Connection* aConnection,
                                           bool aShadowWrites,
                                           int64_t& aOutUsage) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(aConnection);

  // The order of elements is not stored in the database, so write infos don't
  // need to be sorted before being processed.

  nsresult rv;

  if (mTruncateInfo) {
    rv = PerformTruncate(aConnection, aShadowWrites);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  for (auto iter = mWriteInfos.ConstIter(); !iter.Done(); iter.Next()) {
    WriteInfo* writeInfo = iter.Data();

    switch (writeInfo->GetType()) {
      case WriteInfo::InsertItem:
      case WriteInfo::UpdateItem: {
        auto insertItemInfo = static_cast<InsertItemInfo*>(writeInfo);

        rv = PerformInsertOrUpdate(aConnection, aShadowWrites,
                                   insertItemInfo->GetKey(),
                                   insertItemInfo->GetValue());
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        break;
      }

      case WriteInfo::DeleteItem: {
        auto deleteItemInfo = static_cast<DeleteItemInfo*>(writeInfo);

        rv =
            PerformDelete(aConnection, aShadowWrites, deleteItemInfo->GetKey());
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        break;
      }

      default:
        MOZ_CRASH("Bad type!");
    }
  }

  Connection::CachedStatement stmt;
  rv = aConnection->GetCachedStatement(
      NS_LITERAL_CSTRING("UPDATE database "
                         "SET usage = usage + :delta"),
      &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("delta"), mTotalDelta);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->GetCachedStatement(NS_LITERAL_CSTRING("SELECT usage "
                                                          "FROM database"),
                                       &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!hasResult)) {
    return NS_ERROR_FAILURE;
  }

  int64_t usage;
  rv = stmt->GetInt64(0, &usage);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  aOutUsage = usage;
  return NS_OK;
}

nsresult ConnectionWriteOptimizer::PerformInsertOrUpdate(
    Connection* aConnection, bool aShadowWrites, const nsAString& aKey,
    const LSValue& aValue) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(aConnection);

  Connection::CachedStatement stmt;
  nsresult rv = aConnection->GetCachedStatement(
      NS_LITERAL_CSTRING(
          "INSERT OR REPLACE INTO data (key, value, utf16Length, compressed) "
          "VALUES(:key, :value, :utf16Length, :compressed)"),
      &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("key"), aKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("value"), aValue);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("utf16Length"),
                             aValue.UTF16Length());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("compressed"),
                             aValue.IsCompressed());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!aShadowWrites) {
    return NS_OK;
  }

  rv = aConnection->GetCachedStatement(
      NS_LITERAL_CSTRING(
          "INSERT OR REPLACE INTO shadow.webappsstore2 "
          "(originAttributes, originKey, scope, key, value) "
          "VALUES (:originAttributes, :originKey, :scope, :key, :value) "),
      &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  ArchivedOriginScope* archivedOriginScope =
      aConnection->GetArchivedOriginScope();

  rv = archivedOriginScope->BindToStatement(stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString scope = Scheme0Scope(archivedOriginScope->OriginSuffix(),
                                 archivedOriginScope->OriginNoSuffix());

  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("scope"), scope);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("key"), aKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aValue.IsCompressed()) {
    nsCString value;
    if (NS_WARN_IF(!SnappyUncompress(aValue, value))) {
      return NS_ERROR_FAILURE;
    }
    rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("value"), value);
  } else {
    rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("value"), aValue);
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult ConnectionWriteOptimizer::PerformDelete(Connection* aConnection,
                                                 bool aShadowWrites,
                                                 const nsAString& aKey) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(aConnection);

  Connection::CachedStatement stmt;
  nsresult rv =
      aConnection->GetCachedStatement(NS_LITERAL_CSTRING("DELETE FROM data "
                                                         "WHERE key = :key;"),
                                      &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("key"), aKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!aShadowWrites) {
    return NS_OK;
  }

  rv = aConnection->GetCachedStatement(
      NS_LITERAL_CSTRING("DELETE FROM shadow.webappsstore2 "
                         "WHERE originAttributes = :originAttributes "
                         "AND originKey = :originKey "
                         "AND key = :key;"),
      &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->GetArchivedOriginScope()->BindToStatement(stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("key"), aKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult ConnectionWriteOptimizer::PerformTruncate(Connection* aConnection,
                                                   bool aShadowWrites) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(aConnection);

  Connection::CachedStatement stmt;
  nsresult rv = aConnection->GetCachedStatement(
      NS_LITERAL_CSTRING("DELETE FROM data;"), &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!aShadowWrites) {
    return NS_OK;
  }

  rv = aConnection->GetCachedStatement(
      NS_LITERAL_CSTRING("DELETE FROM shadow.webappsstore2 "
                         "WHERE originAttributes = :originAttributes "
                         "AND originKey = :originKey;"),
      &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->GetArchivedOriginScope()->BindToStatement(stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

/*******************************************************************************
 * DatastoreOperationBase
 ******************************************************************************/

/*******************************************************************************
 * ConnectionDatastoreOperationBase
 ******************************************************************************/

ConnectionDatastoreOperationBase::ConnectionDatastoreOperationBase(
    Connection* aConnection, bool aEnsureStorageConnection)
    : mConnection(aConnection),
      mEnsureStorageConnection(aEnsureStorageConnection) {
  MOZ_ASSERT(aConnection);
}

ConnectionDatastoreOperationBase::~ConnectionDatastoreOperationBase() {
  MOZ_ASSERT(!mConnection,
             "ConnectionDatabaseOperationBase::Cleanup() was not called by a "
             "subclass!");
}

void ConnectionDatastoreOperationBase::Cleanup() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mConnection);

  mConnection = nullptr;

  NoteComplete();
}

void ConnectionDatastoreOperationBase::OnSuccess() { AssertIsOnOwningThread(); }

void ConnectionDatastoreOperationBase::OnFailure(nsresult aResultCode) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aResultCode));
}

void ConnectionDatastoreOperationBase::RunOnConnectionThread() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mConnection);
  MOZ_ASSERT(NS_SUCCEEDED(ResultCode()));

  if (!MayProceedOnNonOwningThread()) {
    SetFailureCode(NS_ERROR_FAILURE);
  } else {
    nsresult rv = NS_OK;

    // The boolean flag is only used by the CloseOp to avoid creating empty
    // databases.
    if (mEnsureStorageConnection) {
      rv = mConnection->EnsureStorageConnection();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        SetFailureCode(rv);
      } else {
        MOZ_ASSERT(mConnection->StorageConnection());
      }
    }

    if (NS_SUCCEEDED(rv)) {
      rv = DoDatastoreWork();
      if (NS_FAILED(rv)) {
        SetFailureCode(rv);
      }
    }
  }

  MOZ_ALWAYS_SUCCEEDS(OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL));
}

void ConnectionDatastoreOperationBase::RunOnOwningThread() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mConnection);

  if (!MayProceed()) {
    MaybeSetFailureCode(NS_ERROR_FAILURE);
  }

  if (NS_SUCCEEDED(ResultCode())) {
    OnSuccess();
  } else {
    OnFailure(ResultCode());
  }

  Cleanup();
}

NS_IMETHODIMP
ConnectionDatastoreOperationBase::Run() {
  if (IsOnConnectionThread()) {
    RunOnConnectionThread();
  } else {
    RunOnOwningThread();
  }

  return NS_OK;
}

/*******************************************************************************
 * Connection implementation
 ******************************************************************************/

Connection::Connection(ConnectionThread* aConnectionThread,
                       const nsACString& aSuffix, const nsACString& aGroup,
                       const nsACString& aOrigin,
                       nsAutoPtr<ArchivedOriginScope>&& aArchivedOriginScope,
                       bool aDatabaseWasNotAvailable)
    : mConnectionThread(aConnectionThread),
      mQuotaClient(QuotaClient::GetInstance()),
      mArchivedOriginScope(std::move(aArchivedOriginScope)),
      mSuffix(aSuffix),
      mGroup(aGroup),
      mOrigin(aOrigin),
      mDatabaseWasNotAvailable(aDatabaseWasNotAvailable),
      mHasCreatedDatabase(false),
      mFlushScheduled(false)
#ifdef DEBUG
      ,
      mInUpdateBatch(false),
      mFinished(false)
#endif
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!aGroup.IsEmpty());
  MOZ_ASSERT(!aOrigin.IsEmpty());
}

Connection::~Connection() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mStorageConnection);
  MOZ_ASSERT(!mCachedStatements.Count());
  MOZ_ASSERT(!mFlushScheduled);
  MOZ_ASSERT(!mInUpdateBatch);
  MOZ_ASSERT(mFinished);
}

void Connection::Dispatch(ConnectionDatastoreOperationBase* aOp) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mConnectionThread);

  MOZ_ALWAYS_SUCCEEDS(
      mConnectionThread->mThread->Dispatch(aOp, NS_DISPATCH_NORMAL));
}

void Connection::Close(nsIRunnable* aCallback) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aCallback);

  if (mFlushScheduled) {
    MOZ_ASSERT(mFlushTimer);
    MOZ_ALWAYS_SUCCEEDS(mFlushTimer->Cancel());

    Flush();

    mFlushTimer = nullptr;
  }

  RefPtr<CloseOp> op = new CloseOp(this, aCallback);

  Dispatch(op);
}

void Connection::SetItem(const nsString& aKey, const LSValue& aValue,
                         int64_t aDelta, bool aIsNewItem) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mInUpdateBatch);

  if (aIsNewItem) {
    mWriteOptimizer.InsertItem(aKey, aValue, aDelta);
  } else {
    mWriteOptimizer.UpdateItem(aKey, aValue, aDelta);
  }
}

void Connection::RemoveItem(const nsString& aKey, int64_t aDelta) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mInUpdateBatch);

  mWriteOptimizer.DeleteItem(aKey, aDelta);
}

void Connection::Clear(int64_t aDelta) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mInUpdateBatch);

  mWriteOptimizer.Truncate(aDelta);
}

void Connection::BeginUpdateBatch() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mInUpdateBatch);

#ifdef DEBUG
  mInUpdateBatch = true;
#endif
}

void Connection::EndUpdateBatch() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mInUpdateBatch);

  if (mWriteOptimizer.HasWrites() && !mFlushScheduled) {
    ScheduleFlush();
  }

#ifdef DEBUG
  mInUpdateBatch = false;
#endif
}

nsresult Connection::EnsureStorageConnection() {
  AssertIsOnConnectionThread();

  if (mStorageConnection) {
    return NS_OK;
  }

  nsresult rv;

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  if (!mDatabaseWasNotAvailable || mHasCreatedDatabase) {
    nsCOMPtr<nsIFile> directoryEntry;
    rv = quotaManager->GetDirectoryForOrigin(PERSISTENCE_TYPE_DEFAULT, mOrigin,
                                             getter_AddRefs(directoryEntry));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = directoryEntry->Append(NS_LITERAL_STRING(LS_DIRECTORY_NAME));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = directoryEntry->GetPath(mDirectoryPath);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = directoryEntry->Append(NS_LITERAL_STRING(DATA_FILE_NAME));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsString databaseFilePath;
    rv = directoryEntry->GetPath(databaseFilePath);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCOMPtr<mozIStorageConnection> storageConnection;
    rv = GetStorageConnection(databaseFilePath,
                              getter_AddRefs(storageConnection));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mStorageConnection = storageConnection;

    return NS_OK;
  }

  RefPtr<InitOriginHelper> helper =
      new InitOriginHelper(mSuffix, mGroup, mOrigin);

  nsString originDirectoryPath;
  rv = helper->BlockAndReturnOriginDirectoryPath(originDirectoryPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIFile> directoryEntry;
  rv = NS_NewLocalFile(originDirectoryPath, false,
                       getter_AddRefs(directoryEntry));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = directoryEntry->Append(NS_LITERAL_STRING(LS_DIRECTORY_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = directoryEntry->GetPath(mDirectoryPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = directoryEntry->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    rv = directoryEntry->Create(nsIFile::DIRECTORY_TYPE, 0755);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = directoryEntry->Append(NS_LITERAL_STRING(DATA_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#ifdef DEBUG
  rv = directoryEntry->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  MOZ_ASSERT(!exists);
#endif

  nsCOMPtr<nsIFile> usageFile;
  rv = GetUsageFile(mDirectoryPath, getter_AddRefs(usageFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> storageConnection;
  bool removedUsageFile;

  auto autoRemove = MakeScopeExit([&] {
    if (storageConnection) {
      MOZ_ALWAYS_SUCCEEDS(storageConnection->Close());
    }

    nsresult rv = directoryEntry->Remove(false);
    if (rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST &&
        rv != NS_ERROR_FILE_NOT_FOUND && NS_FAILED(rv)) {
      NS_WARNING("Failed to remove database file!");
    }
  });

  rv = CreateStorageConnection(directoryEntry, usageFile, mOrigin,
                               getter_AddRefs(storageConnection),
                               &removedUsageFile);

  MOZ_ASSERT(!removedUsageFile);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(mQuotaClient);

  MutexAutoLock shadowDatabaseLock(mQuotaClient->ShadowDatabaseMutex());

  nsCOMPtr<mozIStorageConnection> shadowConnection;
  if (!gInitializedShadowStorage) {
    rv = CreateShadowStorageConnection(quotaManager->GetBasePath(),
                                       getter_AddRefs(shadowConnection));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    gInitializedShadowStorage = true;
  }

  autoRemove.release();

  if (!mHasCreatedDatabase) {
    mHasCreatedDatabase = true;
  }

  mStorageConnection = storageConnection;

  return NS_OK;
}

void Connection::CloseStorageConnection() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mStorageConnection);

  mCachedStatements.Clear();

  MOZ_ALWAYS_SUCCEEDS(mStorageConnection->Close());
  mStorageConnection = nullptr;
}

nsresult Connection::GetCachedStatement(const nsACString& aQuery,
                                        CachedStatement* aCachedStatement) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(!aQuery.IsEmpty());
  MOZ_ASSERT(aCachedStatement);
  MOZ_ASSERT(mStorageConnection);

  nsCOMPtr<mozIStorageStatement> stmt;

  if (!mCachedStatements.Get(aQuery, getter_AddRefs(stmt))) {
    nsresult rv =
        mStorageConnection->CreateStatement(aQuery, getter_AddRefs(stmt));
    if (NS_FAILED(rv)) {
#ifdef DEBUG
      nsCString msg;
      MOZ_ALWAYS_SUCCEEDS(mStorageConnection->GetLastErrorString(msg));

      nsAutoCString error =
          NS_LITERAL_CSTRING("The statement '") + aQuery +
          NS_LITERAL_CSTRING("' failed to compile with the error message '") +
          msg + NS_LITERAL_CSTRING("'.");

      NS_WARNING(error.get());
#endif
      return rv;
    }

    mCachedStatements.Put(aQuery, stmt);
  }

  aCachedStatement->Assign(this, stmt.forget());
  return NS_OK;
}

nsresult Connection::BeginWriteTransaction() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mStorageConnection);

  CachedStatement stmt;
  nsresult rv =
      GetCachedStatement(NS_LITERAL_CSTRING("BEGIN IMMEDIATE;"), &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult Connection::CommitWriteTransaction() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mStorageConnection);

  CachedStatement stmt;
  nsresult rv = GetCachedStatement(NS_LITERAL_CSTRING("COMMIT;"), &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult Connection::RollbackWriteTransaction() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mStorageConnection);

  CachedStatement stmt;
  nsresult rv = GetCachedStatement(NS_LITERAL_CSTRING("ROLLBACK;"), &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // This may fail if SQLite already rolled back the transaction so ignore any
  // errors.
  Unused << stmt->Execute();

  return NS_OK;
}

void Connection::ScheduleFlush() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mWriteOptimizer.HasWrites());
  MOZ_ASSERT(!mFlushScheduled);

  if (!mFlushTimer) {
    mFlushTimer = NS_NewTimer();
    MOZ_ASSERT(mFlushTimer);
  }

  MOZ_ALWAYS_SUCCEEDS(mFlushTimer->InitWithNamedFuncCallback(
      FlushTimerCallback, this, kFlushTimeoutMs, nsITimer::TYPE_ONE_SHOT,
      "Connection::FlushTimerCallback"));

  mFlushScheduled = true;
}

void Connection::Flush() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mFlushScheduled);

  if (mWriteOptimizer.HasWrites()) {
    RefPtr<FlushOp> op = new FlushOp(this, std::move(mWriteOptimizer));

    Dispatch(op);
  }

  mFlushScheduled = false;
}

// static
void Connection::FlushTimerCallback(nsITimer* aTimer, void* aClosure) {
  MOZ_ASSERT(aClosure);

  auto* self = static_cast<Connection*>(aClosure);
  MOZ_ASSERT(self);
  MOZ_ASSERT(self->mFlushScheduled);

  self->Flush();
}

Connection::CachedStatement::CachedStatement() {
  AssertIsOnConnectionThread();

  MOZ_COUNT_CTOR(Connection::CachedStatement);
}

Connection::CachedStatement::~CachedStatement() {
  AssertIsOnConnectionThread();

  MOZ_COUNT_DTOR(Connection::CachedStatement);
}

Connection::CachedStatement::operator mozIStorageStatement*() const {
  AssertIsOnConnectionThread();

  return mStatement;
}

mozIStorageStatement* Connection::CachedStatement::operator->() const {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mStatement);

  return mStatement;
}

void Connection::CachedStatement::Assign(
    Connection* aConnection,
    already_AddRefed<mozIStorageStatement> aStatement) {
  AssertIsOnConnectionThread();

  mScoper.reset();

  mStatement = aStatement;

  if (mStatement) {
    mScoper.emplace(mStatement);
  }
}

nsresult Connection::InitOriginHelper::BlockAndReturnOriginDirectoryPath(
    nsAString& aOriginDirectoryPath) {
  AssertIsOnConnectionThread();

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  MOZ_ALWAYS_SUCCEEDS(
      quotaManager->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL));

  mozilla::MonitorAutoLock lock(mMonitor);
  while (mWaiting) {
    lock.Wait();
  }

  if (NS_WARN_IF(NS_FAILED(mIOThreadResultCode))) {
    return mIOThreadResultCode;
  }

  aOriginDirectoryPath = mOriginDirectoryPath;
  return NS_OK;
}

nsresult Connection::InitOriginHelper::RunOnIOThread() {
  AssertIsOnIOThread();

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  nsCOMPtr<nsIFile> directoryEntry;
  nsresult rv = quotaManager->EnsureOriginIsInitialized(
      PERSISTENCE_TYPE_DEFAULT, mSuffix, mGroup, mOrigin,
      getter_AddRefs(directoryEntry));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = directoryEntry->GetPath(mOriginDirectoryPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
Connection::InitOriginHelper::Run() {
  AssertIsOnIOThread();

  nsresult rv = RunOnIOThread();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mIOThreadResultCode = rv;
  }

  mozilla::MonitorAutoLock lock(mMonitor);
  MOZ_ASSERT(mWaiting);

  mWaiting = false;
  lock.Notify();

  return NS_OK;
}

Connection::FlushOp::FlushOp(Connection* aConnection,
                             ConnectionWriteOptimizer&& aWriteOptimizer)
    : ConnectionDatastoreOperationBase(aConnection),
      mWriteOptimizer(std::move(aWriteOptimizer)),
      mShadowWrites(gShadowWrites) {}

nsresult Connection::FlushOp::DoDatastoreWork() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mConnection);

  AutoWriteTransaction autoWriteTransaction(mShadowWrites);

  nsresult rv = autoWriteTransaction.Start(mConnection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int64_t usage;
  rv = mWriteOptimizer.Perform(mConnection, mShadowWrites, usage);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIFile> usageFile;
  rv = GetUsageFile(mConnection->DirectoryPath(), getter_AddRefs(usageFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIFile> usageJournalFile;
  rv = GetUsageJournalFile(mConnection->DirectoryPath(),
                           getter_AddRefs(usageJournalFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = UpdateUsageFile(usageFile, usageJournalFile, usage);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = autoWriteTransaction.Commit();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = usageJournalFile->Remove(false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void Connection::FlushOp::Cleanup() {
  AssertIsOnOwningThread();

  mWriteOptimizer.Reset();

  MOZ_ASSERT(!mWriteOptimizer.HasWrites());

  ConnectionDatastoreOperationBase::Cleanup();
}

nsresult Connection::CloseOp::DoDatastoreWork() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mConnection);

  if (mConnection->StorageConnection()) {
    mConnection->CloseStorageConnection();
  }

  return NS_OK;
}

void Connection::CloseOp::Cleanup() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mConnection);

  mConnection->mConnectionThread->mConnections.Remove(mConnection->mOrigin);

#ifdef DEBUG
  MOZ_ASSERT(!mConnection->mFinished);
  mConnection->mFinished = true;
#endif

  nsCOMPtr<nsIRunnable> callback;
  mCallback.swap(callback);

  callback->Run();

  ConnectionDatastoreOperationBase::Cleanup();
}

/*******************************************************************************
 * ConnectionThread implementation
 ******************************************************************************/

ConnectionThread::ConnectionThread() {
  AssertIsOnOwningThread();
  AssertIsOnBackgroundThread();

  MOZ_ALWAYS_SUCCEEDS(NS_NewNamedThread("LS Thread", getter_AddRefs(mThread)));
}

ConnectionThread::~ConnectionThread() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mConnections.Count());
}

bool ConnectionThread::IsOnConnectionThread() {
  MOZ_ASSERT(mThread);

  bool current;
  return NS_SUCCEEDED(mThread->IsOnCurrentThread(&current)) && current;
}

void ConnectionThread::AssertIsOnConnectionThread() {
  MOZ_ASSERT(IsOnConnectionThread());
}

already_AddRefed<Connection> ConnectionThread::CreateConnection(
    const nsACString& aSuffix, const nsACString& aGroup,
    const nsACString& aOrigin,
    nsAutoPtr<ArchivedOriginScope>&& aArchivedOriginScope,
    bool aDatabaseWasNotAvailable) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!aOrigin.IsEmpty());
  MOZ_ASSERT(!mConnections.GetWeak(aOrigin));

  RefPtr<Connection> connection =
      new Connection(this, aSuffix, aGroup, aOrigin,
                     std::move(aArchivedOriginScope), aDatabaseWasNotAvailable);
  mConnections.Put(aOrigin, connection);

  return connection.forget();
}

void ConnectionThread::Shutdown() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mThread);

  mThread->Shutdown();
}

/*******************************************************************************
 * Datastore
 ******************************************************************************/

Datastore::Datastore(const nsACString& aGroup, const nsACString& aOrigin,
                     uint32_t aPrivateBrowsingId, int64_t aUsage,
                     int64_t aSizeOfKeys, int64_t aSizeOfItems,
                     already_AddRefed<DirectoryLock>&& aDirectoryLock,
                     already_AddRefed<Connection>&& aConnection,
                     already_AddRefed<QuotaObject>&& aQuotaObject,
                     nsDataHashtable<nsStringHashKey, LSValue>& aValues,
                     nsTArray<LSItemInfo>& aOrderedItems)
    : mDirectoryLock(std::move(aDirectoryLock)),
      mConnection(std::move(aConnection)),
      mQuotaObject(std::move(aQuotaObject)),
      mGroup(aGroup),
      mOrigin(aOrigin),
      mPrivateBrowsingId(aPrivateBrowsingId),
      mUsage(aUsage),
      mUpdateBatchUsage(-1),
      mSizeOfKeys(aSizeOfKeys),
      mSizeOfItems(aSizeOfItems),
      mClosed(false),
      mInUpdateBatch(false) {
  AssertIsOnBackgroundThread();

  mValues.SwapElements(aValues);
  mOrderedItems.SwapElements(aOrderedItems);
}

Datastore::~Datastore() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mClosed);
}

void Datastore::Close() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mClosed);
  MOZ_ASSERT(!mPrepareDatastoreOps.Count());
  MOZ_ASSERT(!mPreparedDatastores.Count());
  MOZ_ASSERT(!mDatabases.Count());
  MOZ_ASSERT(mDirectoryLock);

  mClosed = true;

  if (IsPersistent()) {
    MOZ_ASSERT(mConnection);
    MOZ_ASSERT(mQuotaObject);

    // We can't release the directory lock and unregister itself from the
    // hashtable until the connection is fully closed.
    nsCOMPtr<nsIRunnable> callback =
        NewRunnableMethod("dom::Datastore::ConnectionClosedCallback", this,
                          &Datastore::ConnectionClosedCallback);
    mConnection->Close(callback);
  } else {
    MOZ_ASSERT(!mConnection);
    MOZ_ASSERT(!mQuotaObject);

    // There's no connection, so it's safe to release the directory lock and
    // unregister itself from the hashtable.

    mDirectoryLock = nullptr;

    CleanupMetadata();
  }
}

void Datastore::WaitForConnectionToComplete(nsIRunnable* aCallback) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(!mCompleteCallback);
  MOZ_ASSERT(mClosed);

  mCompleteCallback = aCallback;
}

void Datastore::NoteLivePrepareDatastoreOp(
    PrepareDatastoreOp* aPrepareDatastoreOp) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aPrepareDatastoreOp);
  MOZ_ASSERT(!mPrepareDatastoreOps.GetEntry(aPrepareDatastoreOp));
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(!mClosed);

  mPrepareDatastoreOps.PutEntry(aPrepareDatastoreOp);
}

void Datastore::NoteFinishedPrepareDatastoreOp(
    PrepareDatastoreOp* aPrepareDatastoreOp) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aPrepareDatastoreOp);
  MOZ_ASSERT(mPrepareDatastoreOps.GetEntry(aPrepareDatastoreOp));
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(!mClosed);

  mPrepareDatastoreOps.RemoveEntry(aPrepareDatastoreOp);

  MaybeClose();
}

void Datastore::NoteLivePreparedDatastore(
    PreparedDatastore* aPreparedDatastore) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aPreparedDatastore);
  MOZ_ASSERT(!mPreparedDatastores.GetEntry(aPreparedDatastore));
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(!mClosed);

  mPreparedDatastores.PutEntry(aPreparedDatastore);
}

void Datastore::NoteFinishedPreparedDatastore(
    PreparedDatastore* aPreparedDatastore) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aPreparedDatastore);
  MOZ_ASSERT(mPreparedDatastores.GetEntry(aPreparedDatastore));
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(!mClosed);

  mPreparedDatastores.RemoveEntry(aPreparedDatastore);

  MaybeClose();
}

void Datastore::NoteLiveDatabase(Database* aDatabase) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);
  MOZ_ASSERT(!mDatabases.GetEntry(aDatabase));
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(!mClosed);

  mDatabases.PutEntry(aDatabase);
}

void Datastore::NoteFinishedDatabase(Database* aDatabase) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);
  MOZ_ASSERT(mDatabases.GetEntry(aDatabase));
  MOZ_ASSERT(!mActiveDatabases.GetEntry(aDatabase));
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(!mClosed);

  mDatabases.RemoveEntry(aDatabase);

  MaybeClose();
}

void Datastore::NoteActiveDatabase(Database* aDatabase) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);
  MOZ_ASSERT(mDatabases.GetEntry(aDatabase));
  MOZ_ASSERT(!mActiveDatabases.GetEntry(aDatabase));
  MOZ_ASSERT(!mClosed);

  mActiveDatabases.PutEntry(aDatabase);
}

void Datastore::NoteInactiveDatabase(Database* aDatabase) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);
  MOZ_ASSERT(mDatabases.GetEntry(aDatabase));
  MOZ_ASSERT(mActiveDatabases.GetEntry(aDatabase));
  MOZ_ASSERT(!mClosed);

  mActiveDatabases.RemoveEntry(aDatabase);

  if (!mActiveDatabases.Count() && mPendingUsageDeltas.Length()) {
    int64_t finalDelta = 0;

    for (auto delta : mPendingUsageDeltas) {
      finalDelta += delta;
    }

    MOZ_ASSERT(finalDelta <= 0);

    if (finalDelta != 0) {
      DebugOnly<bool> ok = UpdateUsage(finalDelta);
      MOZ_ASSERT(ok);
    }

    mPendingUsageDeltas.Clear();
  }
}

void Datastore::GetSnapshotLoadInfo(const nsString& aKey,
                                    bool& aAddKeyToUnknownItems,
                                    nsTHashtable<nsStringHashKey>& aLoadedItems,
                                    nsTArray<LSItemInfo>& aItemInfos,
                                    uint32_t& aNextLoadIndex,
                                    LSSnapshot::LoadState& aLoadState) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mClosed);
  MOZ_ASSERT(!mInUpdateBatch);

#ifdef DEBUG
  int64_t sizeOfKeys = 0;
  int64_t sizeOfItems = 0;
  for (auto item : mOrderedItems) {
    int64_t sizeOfKey = static_cast<int64_t>(item.key().Length());
    sizeOfKeys += sizeOfKey;
    sizeOfItems += sizeOfKey + static_cast<int64_t>(item.value().Length());
  }
  MOZ_ASSERT(mSizeOfKeys == sizeOfKeys);
  MOZ_ASSERT(mSizeOfItems == sizeOfItems);
#endif

  // Computes load state optimized for current size of keys and items.
  // Zero key length and value can be passed to do a quick initial estimation.
  // If computed load state is already AllOrderedItems then excluded key length
  // and value length can't make it any better.
  auto GetLoadState = [&](auto aKeyLength, auto aValueLength) {
    if (mSizeOfKeys - aKeyLength <= gSnapshotPrefill) {
      if (mSizeOfItems - aKeyLength - aValueLength <= gSnapshotPrefill) {
        return LSSnapshot::LoadState::AllOrderedItems;
      }

      return LSSnapshot::LoadState::AllOrderedKeys;
    }

    return LSSnapshot::LoadState::Partial;
  };

  // Value for given aKey if aKey is not void (can be void too if value doesn't
  // exist for given aKey).
  LSValue value;
  // If aKey and value are not void, checkKey will be set to true. Once we find
  // an item for given aKey in one of the loops below, checkKey is set to false
  // to prevent additional comparison of strings (string implementation compares
  // string lengths first to avoid char by char comparison if possible).
  bool checkKey = false;

  // Avoid additional hash lookup if all ordered items fit into initial prefill
  // already.
  LSSnapshot::LoadState loadState = GetLoadState(/* aKeyLength */ 0,
                                                 /* aValueLength */ 0);
  if (loadState != LSSnapshot::LoadState::AllOrderedItems && !aKey.IsVoid()) {
    GetItem(aKey, value);
    if (!value.IsVoid()) {
      // Ok, we have a non void aKey and value.

      // We have to watch for aKey during one of the loops below to exclude it
      // from the size computation. The super fast mode (AllOrderedItems)
      // doesn't have to do that though.
      checkKey = true;

      // We have to compute load state again because aKey length and value
      // length is excluded from the size in this case.
      loadState = GetLoadState(aKey.Length(), value.Length());
    }
  }

  switch (loadState) {
    case LSSnapshot::LoadState::AllOrderedItems: {
      // We're sending all ordered items, we don't need to check keys because
      // mOrderedItems must contain a value for aKey if checkKey is true.

      aItemInfos.AppendElements(mOrderedItems);

      MOZ_ASSERT(aItemInfos.Length() == mValues.Count());
      aNextLoadIndex = mValues.Count();

      aAddKeyToUnknownItems = false;

      break;
    }

    case LSSnapshot::LoadState::AllOrderedKeys: {
      // We don't have enough snapshot budget to send all items, but we do have
      // enough to send all of the keys and to make a best effort to populate as
      // many values as possible. We send void string values once we run out of
      // budget. A complicating factor is that we want to make sure that we send
      // the value for aKey which is a localStorage read that's triggering this
      // request. Since that key can happen anywhere in the list of items, we
      // need to handle it specially.
      //
      // The loop is effectively doing 2 things in parallel:
      //
      //   1. Looking for the `aKey` to send. This is tracked by `checkKey`
      //      which is true if there was an `aKey` specified and until we
      //      populate its value, and false thereafter.
      //   2. Sending values until we run out of `size` budget and switch to
      //      sending void values. `doneSendingValues` tracks when we've run out
      //      of size budget, with `setVoidValue` tracking whether a value
      //      should be sent for each turn of the event loop but can be
      //      overridden when `aKey` is found.

      int64_t size = mSizeOfKeys;
      bool setVoidValue = false;
      bool doneSendingValues = false;
      for (uint32_t index = 0; index < mOrderedItems.Length(); index++) {
        const LSItemInfo& item = mOrderedItems[index];

        const nsString& key = item.key();
        const LSValue& value = item.value();

        if (checkKey && key == aKey) {
          checkKey = false;
          setVoidValue = false;
        } else if (!setVoidValue) {
          if (doneSendingValues) {
            setVoidValue = true;
          } else {
            size += static_cast<int64_t>(value.Length());

            if (size > gSnapshotPrefill) {
              setVoidValue = true;
              doneSendingValues = true;

              // We set doneSendingValues to true and that will guard against
              // entering this branch during next iterations. So aNextLoadIndex
              // is set only once.
              aNextLoadIndex = index;
            }
          }
        }

        LSItemInfo* itemInfo = aItemInfos.AppendElement();
        itemInfo->key() = key;
        if (setVoidValue) {
          itemInfo->value().SetIsVoid(true);
        } else {
          aLoadedItems.PutEntry(key);
          itemInfo->value() = value;
        }
      }

      aAddKeyToUnknownItems = false;

      break;
    }

    case LSSnapshot::LoadState::Partial: {
      int64_t size = 0;
      for (uint32_t index = 0; index < mOrderedItems.Length(); index++) {
        const LSItemInfo& item = mOrderedItems[index];

        const nsString& key = item.key();
        const LSValue& value = item.value();

        if (checkKey && key == aKey) {
          checkKey = false;
        } else {
          size += static_cast<int64_t>(key.Length()) +
                  static_cast<int64_t>(value.Length());

          if (size > gSnapshotPrefill) {
            aNextLoadIndex = index;
            break;
          }
        }

        aLoadedItems.PutEntry(key);

        LSItemInfo* itemInfo = aItemInfos.AppendElement();
        itemInfo->key() = key;
        itemInfo->value() = value;
      }

      aAddKeyToUnknownItems = false;

      if (!aKey.IsVoid()) {
        if (value.IsVoid()) {
          aAddKeyToUnknownItems = true;
        } else if (checkKey) {
          // The item wasn't added in the loop above, add it here.

          LSItemInfo* itemInfo = aItemInfos.AppendElement();
          itemInfo->key() = aKey;
          itemInfo->value() = value;
        }
      }

      MOZ_ASSERT(aItemInfos.Length() < mOrderedItems.Length());

      break;
    }

    default:
      MOZ_CRASH("Bad load state value!");
  }

  aLoadState = loadState;
}

void Datastore::GetItem(const nsString& aKey, LSValue& aValue) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mClosed);

  if (!mValues.Get(aKey, &aValue)) {
    aValue.SetIsVoid(true);
  }
}

void Datastore::GetKeys(nsTArray<nsString>& aKeys) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mClosed);

  for (auto item : mOrderedItems) {
    aKeys.AppendElement(item.key());
  }
}

void Datastore::SetItem(Database* aDatabase, const nsString& aKey,
                        const LSValue& aValue) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);
  MOZ_ASSERT(!mClosed);
  MOZ_ASSERT(mInUpdateBatch);

  LSValue oldValue;
  GetItem(aKey, oldValue);

  if (oldValue != aValue) {
    bool isNewItem = oldValue.IsVoid();

    NotifySnapshots(aDatabase, aKey, oldValue, /* affectsOrder */ isNewItem);

    mValues.Put(aKey, aValue);

    int64_t delta;

    if (isNewItem) {
      mWriteOptimizer.InsertItem(aKey, aValue);

      int64_t sizeOfKey = static_cast<int64_t>(aKey.Length());

      delta = sizeOfKey + static_cast<int64_t>(aValue.UTF16Length());

      mUpdateBatchUsage += delta;

      mSizeOfKeys += sizeOfKey;
      mSizeOfItems += sizeOfKey + static_cast<int64_t>(aValue.Length());
    } else {
      mWriteOptimizer.UpdateItem(aKey, aValue);

      delta = static_cast<int64_t>(aValue.UTF16Length()) -
              static_cast<int64_t>(oldValue.UTF16Length());

      mUpdateBatchUsage += delta;

      mSizeOfItems += static_cast<int64_t>(aValue.Length()) -
                      static_cast<int64_t>(oldValue.Length());
    }

    if (IsPersistent()) {
      mConnection->SetItem(aKey, aValue, delta, isNewItem);
    }
  }
}

void Datastore::RemoveItem(Database* aDatabase, const nsString& aKey) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);
  MOZ_ASSERT(!mClosed);
  MOZ_ASSERT(mInUpdateBatch);

  LSValue oldValue;
  GetItem(aKey, oldValue);

  if (!oldValue.IsVoid()) {
    NotifySnapshots(aDatabase, aKey, oldValue, /* aAffectsOrder */ true);

    mValues.Remove(aKey);

    mWriteOptimizer.DeleteItem(aKey);

    int64_t sizeOfKey = static_cast<int64_t>(aKey.Length());

    int64_t delta = -sizeOfKey - static_cast<int64_t>(oldValue.UTF16Length());

    mUpdateBatchUsage += delta;

    mSizeOfKeys -= sizeOfKey;
    mSizeOfItems -= sizeOfKey + static_cast<int64_t>(oldValue.Length());

    if (IsPersistent()) {
      mConnection->RemoveItem(aKey, delta);
    }
  }
}

void Datastore::Clear(Database* aDatabase) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mClosed);

  if (mValues.Count()) {
    int64_t delta = 0;
    for (auto iter = mValues.ConstIter(); !iter.Done(); iter.Next()) {
      const nsAString& key = iter.Key();
      const LSValue& value = iter.Data();

      delta += -static_cast<int64_t>(key.Length()) -
               static_cast<int64_t>(value.UTF16Length());

      NotifySnapshots(aDatabase, key, value, /* aAffectsOrder */ true);
    }

    mValues.Clear();

    if (mInUpdateBatch) {
      mWriteOptimizer.Truncate();

      mUpdateBatchUsage += delta;
    } else {
      mOrderedItems.Clear();

      DebugOnly<bool> ok = UpdateUsage(delta);
      MOZ_ASSERT(ok);
    }

    mSizeOfKeys = 0;
    mSizeOfItems = 0;

    if (IsPersistent()) {
      mConnection->Clear(delta);
    }
  }
}

void Datastore::BeginUpdateBatch(int64_t aSnapshotInitialUsage) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aSnapshotInitialUsage >= 0);
  MOZ_ASSERT(!mClosed);
  MOZ_ASSERT(mUpdateBatchUsage == -1);
  MOZ_ASSERT(!mInUpdateBatch);

  mUpdateBatchUsage = aSnapshotInitialUsage;

  if (IsPersistent()) {
    mConnection->BeginUpdateBatch();
  }

  mInUpdateBatch = true;
}

int64_t Datastore::EndUpdateBatch(int64_t aSnapshotPeakUsage) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mClosed);
  MOZ_ASSERT(mInUpdateBatch);

  mWriteOptimizer.ApplyAndReset(mOrderedItems);

  MOZ_ASSERT(!mWriteOptimizer.HasWrites());

  if (aSnapshotPeakUsage >= 0) {
    int64_t delta = mUpdateBatchUsage - aSnapshotPeakUsage;

    if (mActiveDatabases.Count()) {
      // We can't apply deltas while other databases are still active.
      // The final delta must be zero or negative, but individual deltas can
      // be positive. A positive delta can't be applied asynchronously since
      // there's no way to fire the quota exceeded error event.

      mPendingUsageDeltas.AppendElement(delta);
    } else {
      MOZ_ASSERT(delta <= 0);
      if (delta != 0) {
        DebugOnly<bool> ok = UpdateUsage(delta);
        MOZ_ASSERT(ok);
      }
    }
  }

  int64_t result = mUpdateBatchUsage;
  mUpdateBatchUsage = -1;

  if (IsPersistent()) {
    mConnection->EndUpdateBatch();
  }

  mInUpdateBatch = false;

  return result;
}

int64_t Datastore::RequestUpdateUsage(int64_t aRequestedSize,
                                      int64_t aMinSize) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aRequestedSize > 0);
  MOZ_ASSERT(aMinSize > 0);

  if (UpdateUsage(aRequestedSize)) {
    return aRequestedSize;
  }

  if (UpdateUsage(aMinSize)) {
    return aMinSize;
  }

  return 0;
}

bool Datastore::HasOtherProcessObservers(Database* aDatabase) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);

  if (!gObservers) {
    return false;
  }

  nsTArray<Observer*>* array;
  if (!gObservers->Get(mOrigin, &array)) {
    return false;
  }

  MOZ_ASSERT(array);

  PBackgroundParent* databaseBackgroundActor = aDatabase->Manager();

  for (Observer* observer : *array) {
    if (observer->Manager() != databaseBackgroundActor) {
      return true;
    }
  }

  return false;
}

void Datastore::NotifyOtherProcessObservers(Database* aDatabase,
                                            const nsString& aDocumentURI,
                                            const nsString& aKey,
                                            const LSValue& aOldValue,
                                            const LSValue& aNewValue) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);

  if (!gObservers) {
    return;
  }

  nsTArray<Observer*>* array;
  if (!gObservers->Get(mOrigin, &array)) {
    return;
  }

  MOZ_ASSERT(array);

  // We do not want to send information about events back to the content process
  // that caused the change.
  PBackgroundParent* databaseBackgroundActor = aDatabase->Manager();

  for (Observer* observer : *array) {
    if (observer->Manager() != databaseBackgroundActor) {
      observer->Observe(aDatabase, aDocumentURI, aKey, aOldValue, aNewValue);
    }
  }
}

void Datastore::NoteChangedObserverArray(
    const nsTArray<Observer*>& aObservers) {
  AssertIsOnBackgroundThread();

  for (auto iter = mActiveDatabases.ConstIter(); !iter.Done(); iter.Next()) {
    Database* database = iter.Get()->GetKey();

    Snapshot* snapshot = database->GetSnapshot();
    MOZ_ASSERT(snapshot);

    if (snapshot->IsDirty()) {
      continue;
    }

    bool hasOtherProcessObservers = false;

    PBackgroundParent* databaseBackgroundActor = database->Manager();

    for (Observer* observer : aObservers) {
      if (observer->Manager() != databaseBackgroundActor) {
        hasOtherProcessObservers = true;
        break;
      }
    }

    if (snapshot->HasOtherProcessObservers() != hasOtherProcessObservers) {
      snapshot->MarkDirty();
    }
  }
}

bool Datastore::UpdateUsage(int64_t aDelta) {
  AssertIsOnBackgroundThread();

  // Check internal LocalStorage origin limit.
  int64_t newUsage = mUsage + aDelta;

  MOZ_ASSERT(newUsage >= 0);

  if (newUsage > StaticPrefs::dom_storage_default_quota() * 1024) {
    return false;
  }

  // Check QuotaManager limits (group and global limit).
  if (IsPersistent()) {
    MOZ_ASSERT(mQuotaObject);

    if (!mQuotaObject->MaybeUpdateSize(newUsage, /* aTruncate */ true)) {
      return false;
    }
  }

  // Quota checks passed, set new usage.
  mUsage = newUsage;

  return true;
}

void Datastore::MaybeClose() {
  AssertIsOnBackgroundThread();

  if (!mPrepareDatastoreOps.Count() && !mPreparedDatastores.Count() &&
      !mDatabases.Count()) {
    Close();
  }
}

void Datastore::ConnectionClosedCallback() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(mConnection);
  MOZ_ASSERT(mQuotaObject);
  MOZ_ASSERT(mClosed);

  // Release the quota object first.
  mQuotaObject = nullptr;

  bool databaseWasNotAvailable;
  bool hasCreatedDatabase;
  mConnection->GetFinishInfo(databaseWasNotAvailable, hasCreatedDatabase);

  if (databaseWasNotAvailable && !hasCreatedDatabase) {
    MOZ_ASSERT(mUsage == 0);

    QuotaManager* quotaManager = QuotaManager::Get();
    MOZ_ASSERT(quotaManager);

    quotaManager->ResetUsageForClient(PERSISTENCE_TYPE_DEFAULT, mGroup, mOrigin,
                                      mozilla::dom::quota::Client::LS);
  }

  mConnection = nullptr;

  // Now it's safe to release the directory lock and unregister itself from
  // the hashtable.

  mDirectoryLock = nullptr;

  CleanupMetadata();

  if (mCompleteCallback) {
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(mCompleteCallback.forget()));
  }
}

void Datastore::CleanupMetadata() {
  AssertIsOnBackgroundThread();

  MOZ_ASSERT(gDatastores);
  MOZ_ASSERT(gDatastores->Get(mOrigin));
  gDatastores->Remove(mOrigin);

  if (!gDatastores->Count()) {
    gDatastores = nullptr;
  }
}

void Datastore::NotifySnapshots(Database* aDatabase, const nsAString& aKey,
                                const LSValue& aOldValue, bool aAffectsOrder) {
  AssertIsOnBackgroundThread();

  for (auto iter = mDatabases.ConstIter(); !iter.Done(); iter.Next()) {
    Database* database = iter.Get()->GetKey();

    MOZ_ASSERT(database);

    if (database == aDatabase) {
      continue;
    }

    Snapshot* snapshot = database->GetSnapshot();
    if (snapshot) {
      snapshot->SaveItem(aKey, aOldValue, aAffectsOrder);
    }
  }
}

/*******************************************************************************
 * PreparedDatastore
 ******************************************************************************/

void PreparedDatastore::Destroy() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(gPreparedDatastores);
  MOZ_ASSERT(gPreparedDatastores->Get(mDatastoreId));

  nsAutoPtr<PreparedDatastore> preparedDatastore;
  gPreparedDatastores->Remove(mDatastoreId, &preparedDatastore);
  MOZ_ASSERT(preparedDatastore);
}

// static
void PreparedDatastore::TimerCallback(nsITimer* aTimer, void* aClosure) {
  AssertIsOnBackgroundThread();

  auto* self = static_cast<PreparedDatastore*>(aClosure);
  MOZ_ASSERT(self);

  self->Destroy();
}

/*******************************************************************************
 * Database
 ******************************************************************************/

Database::Database(const PrincipalInfo& aPrincipalInfo,
                   const Maybe<ContentParentId>& aContentParentId,
                   const nsACString& aOrigin, uint32_t aPrivateBrowsingId)
    : mSnapshot(nullptr),
      mPrincipalInfo(aPrincipalInfo),
      mContentParentId(aContentParentId),
      mOrigin(aOrigin),
      mPrivateBrowsingId(aPrivateBrowsingId),
      mAllowedToClose(false),
      mActorDestroyed(false),
      mRequestedAllowToClose(false)
#ifdef DEBUG
      ,
      mActorWasAlive(false)
#endif
{
  AssertIsOnBackgroundThread();
}

Database::~Database() {
  MOZ_ASSERT_IF(mActorWasAlive, mAllowedToClose);
  MOZ_ASSERT_IF(mActorWasAlive, mActorDestroyed);
}

void Database::SetActorAlive(Datastore* aDatastore) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorWasAlive);
  MOZ_ASSERT(!mActorDestroyed);

#ifdef DEBUG
  mActorWasAlive = true;
#endif

  mDatastore = aDatastore;

  mDatastore->NoteLiveDatabase(this);

  if (!gLiveDatabases) {
    gLiveDatabases = new LiveDatabaseArray();
  }

  gLiveDatabases->AppendElement(this);
}

void Database::RegisterSnapshot(Snapshot* aSnapshot) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aSnapshot);
  MOZ_ASSERT(!mSnapshot);
  MOZ_ASSERT(!mAllowedToClose);

  // Only one snapshot at a time is currently supported.
  mSnapshot = aSnapshot;

  mDatastore->NoteActiveDatabase(this);
}

void Database::UnregisterSnapshot(Snapshot* aSnapshot) {
  MOZ_ASSERT(aSnapshot);
  MOZ_ASSERT(mSnapshot == aSnapshot);

  mSnapshot = nullptr;

  mDatastore->NoteInactiveDatabase(this);
}

void Database::RequestAllowToClose() {
  AssertIsOnBackgroundThread();

  if (mRequestedAllowToClose) {
    return;
  }

  mRequestedAllowToClose = true;

  // Send the RequestAllowToClose message to the child to avoid racing with the
  // child actor. Except the case when the actor was already destroyed.
  if (mActorDestroyed) {
    MOZ_ASSERT(mAllowedToClose);
    return;
  }

  if (NS_WARN_IF(!SendRequestAllowToClose()) && !mSnapshot) {
    // This is not necessary, because there should be a runnable scheduled that
    // will call ActorDestroy which calls AllowToClose. However we can speedup
    // the shutdown a bit if we do it here directly, but only if there's no
    // registered snapshot.
    AllowToClose();
  }
}

void Database::AllowToClose() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mAllowedToClose);
  MOZ_ASSERT(mDatastore);
  MOZ_ASSERT(!mSnapshot);

  mAllowedToClose = true;

  mDatastore->NoteFinishedDatabase(this);

  mDatastore = nullptr;

  MOZ_ASSERT(gLiveDatabases);
  gLiveDatabases->RemoveElement(this);

  if (gLiveDatabases->IsEmpty()) {
    gLiveDatabases = nullptr;
  }
}

void Database::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  mActorDestroyed = true;

  if (!mAllowedToClose) {
    AllowToClose();
  }
}

mozilla::ipc::IPCResult Database::RecvDeleteMe() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  IProtocol* mgr = Manager();
  if (!PBackgroundLSDatabaseParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult Database::RecvAllowToClose() {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(mAllowedToClose)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  AllowToClose();

  return IPC_OK();
}

PBackgroundLSSnapshotParent* Database::AllocPBackgroundLSSnapshotParent(
    const nsString& aDocumentURI, const nsString& aKey,
    const bool& aIncreasePeakUsage, const int64_t& aRequestedSize,
    const int64_t& aMinSize, LSSnapshotInitInfo* aInitInfo) {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(aIncreasePeakUsage && aRequestedSize <= 0)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  if (NS_WARN_IF(aIncreasePeakUsage && aMinSize <= 0)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  if (NS_WARN_IF(mAllowedToClose)) {
    ASSERT_UNLESS_FUZZING();
    return nullptr;
  }

  RefPtr<Snapshot> snapshot = new Snapshot(this, aDocumentURI);

  // Transfer ownership to IPDL.
  return snapshot.forget().take();
}

mozilla::ipc::IPCResult Database::RecvPBackgroundLSSnapshotConstructor(
    PBackgroundLSSnapshotParent* aActor, const nsString& aDocumentURI,
    const nsString& aKey, const bool& aIncreasePeakUsage,
    const int64_t& aRequestedSize, const int64_t& aMinSize,
    LSSnapshotInitInfo* aInitInfo) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT_IF(aIncreasePeakUsage, aRequestedSize > 0);
  MOZ_ASSERT_IF(aIncreasePeakUsage, aMinSize > 0);
  MOZ_ASSERT(aInitInfo);
  MOZ_ASSERT(!mAllowedToClose);

  auto* snapshot = static_cast<Snapshot*>(aActor);

  bool addKeyToUnknownItems;
  nsTHashtable<nsStringHashKey> loadedItems;
  nsTArray<LSItemInfo> itemInfos;
  uint32_t nextLoadIndex;
  LSSnapshot::LoadState loadState;
  mDatastore->GetSnapshotLoadInfo(aKey, addKeyToUnknownItems, loadedItems,
                                  itemInfos, nextLoadIndex, loadState);

  nsTHashtable<nsStringHashKey> unknownItems;
  if (addKeyToUnknownItems) {
    unknownItems.PutEntry(aKey);
  }

  uint32_t totalLength = mDatastore->GetLength();

  int64_t initialUsage = mDatastore->GetUsage();

  int64_t peakUsage = initialUsage;

  if (aIncreasePeakUsage) {
    int64_t size = mDatastore->RequestUpdateUsage(aRequestedSize, aMinSize);
    peakUsage += size;
  }

  bool hasOtherProcessObservers = mDatastore->HasOtherProcessObservers(this);

  snapshot->Init(loadedItems, unknownItems, nextLoadIndex, totalLength,
                 initialUsage, peakUsage, loadState, hasOtherProcessObservers);

  RegisterSnapshot(snapshot);

  aInitInfo->addKeyToUnknownItems() = addKeyToUnknownItems;
  aInitInfo->itemInfos() = std::move(itemInfos);
  aInitInfo->totalLength() = totalLength;
  aInitInfo->initialUsage() = initialUsage;
  aInitInfo->peakUsage() = peakUsage;
  aInitInfo->loadState() = loadState;
  aInitInfo->hasOtherProcessObservers() = hasOtherProcessObservers;

  return IPC_OK();
}

bool Database::DeallocPBackgroundLSSnapshotParent(
    PBackgroundLSSnapshotParent* aActor) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<Snapshot> actor = dont_AddRef(static_cast<Snapshot*>(aActor));

  return true;
}

/*******************************************************************************
 * Snapshot
 ******************************************************************************/

Snapshot::Snapshot(Database* aDatabase, const nsAString& aDocumentURI)
    : mDatabase(aDatabase),
      mDatastore(aDatabase->GetDatastore()),
      mDocumentURI(aDocumentURI),
      mTotalLength(0),
      mUsage(-1),
      mPeakUsage(-1),
      mSavedKeys(false),
      mActorDestroyed(false),
      mFinishReceived(false),
      mLoadedReceived(false),
      mLoadedAllItems(false),
      mLoadKeysReceived(false),
      mSentMarkDirty(false) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);
}

Snapshot::~Snapshot() {
  MOZ_ASSERT(mActorDestroyed);
  MOZ_ASSERT(mFinishReceived);
}

void Snapshot::SaveItem(const nsAString& aKey, const LSValue& aOldValue,
                        bool aAffectsOrder) {
  AssertIsOnBackgroundThread();

  MarkDirty();

  if (mLoadedAllItems) {
    return;
  }

  if (!mLoadedItems.GetEntry(aKey) && !mUnknownItems.GetEntry(aKey)) {
    LSValue oldValue(aOldValue);
    mValues.LookupForAdd(aKey).OrInsert([oldValue]() { return oldValue; });
  }

  if (aAffectsOrder && !mSavedKeys) {
    mDatastore->GetKeys(mKeys);
    mSavedKeys = true;
  }
}

void Snapshot::MarkDirty() {
  AssertIsOnBackgroundThread();

  if (!mSentMarkDirty) {
    Unused << SendMarkDirty();
    mSentMarkDirty = true;
  }
}

void Snapshot::Finish() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mDatabase);
  MOZ_ASSERT(mDatastore);
  MOZ_ASSERT(!mFinishReceived);

  mDatastore->BeginUpdateBatch(mUsage);

  mDatastore->EndUpdateBatch(mPeakUsage);

  mDatabase->UnregisterSnapshot(this);

  mFinishReceived = true;
}

void Snapshot::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  mActorDestroyed = true;

  if (!mFinishReceived) {
    Finish();
  }
}

mozilla::ipc::IPCResult Snapshot::RecvDeleteMe() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  IProtocol* mgr = Manager();
  if (!PBackgroundLSSnapshotParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult Snapshot::RecvCheckpoint(
    nsTArray<LSWriteInfo>&& aWriteInfos) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mUsage >= 0);
  MOZ_ASSERT(mPeakUsage >= mUsage);

  if (NS_WARN_IF(aWriteInfos.IsEmpty())) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mHasOtherProcessObservers)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  mDatastore->BeginUpdateBatch(mUsage);

  for (uint32_t index = 0; index < aWriteInfos.Length(); index++) {
    const LSWriteInfo& writeInfo = aWriteInfos[index];

    switch (writeInfo.type()) {
      case LSWriteInfo::TLSSetItemInfo: {
        const LSSetItemInfo& info = writeInfo.get_LSSetItemInfo();

        mDatastore->SetItem(mDatabase, info.key(), info.value());

        break;
      }

      case LSWriteInfo::TLSRemoveItemInfo: {
        const LSRemoveItemInfo& info = writeInfo.get_LSRemoveItemInfo();

        mDatastore->RemoveItem(mDatabase, info.key());

        break;
      }

      case LSWriteInfo::TLSClearInfo: {
        mDatastore->Clear(mDatabase);

        break;
      }

      default:
        MOZ_CRASH("Should never get here!");
    }
  }

  mUsage = mDatastore->EndUpdateBatch(-1);

  return IPC_OK();
}

mozilla::ipc::IPCResult Snapshot::RecvCheckpointAndNotify(
    nsTArray<LSWriteAndNotifyInfo>&& aWriteAndNotifyInfos) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mUsage >= 0);
  MOZ_ASSERT(mPeakUsage >= mUsage);

  if (NS_WARN_IF(aWriteAndNotifyInfos.IsEmpty())) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(!mHasOtherProcessObservers)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  mDatastore->BeginUpdateBatch(mUsage);

  for (uint32_t index = 0; index < aWriteAndNotifyInfos.Length(); index++) {
    const LSWriteAndNotifyInfo& writeAndNotifyInfo =
        aWriteAndNotifyInfos[index];

    switch (writeAndNotifyInfo.type()) {
      case LSWriteAndNotifyInfo::TLSSetItemAndNotifyInfo: {
        const LSSetItemAndNotifyInfo& info =
            writeAndNotifyInfo.get_LSSetItemAndNotifyInfo();

        mDatastore->SetItem(mDatabase, info.key(), info.value());

        mDatastore->NotifyOtherProcessObservers(
            mDatabase, mDocumentURI, info.key(), info.oldValue(), info.value());

        break;
      }

      case LSWriteAndNotifyInfo::TLSRemoveItemAndNotifyInfo: {
        const LSRemoveItemAndNotifyInfo& info =
            writeAndNotifyInfo.get_LSRemoveItemAndNotifyInfo();

        mDatastore->RemoveItem(mDatabase, info.key());

        mDatastore->NotifyOtherProcessObservers(mDatabase, mDocumentURI,
                                                info.key(), info.oldValue(),
                                                VoidLSValue());

        break;
      }

      case LSWriteAndNotifyInfo::TLSClearInfo: {
        mDatastore->Clear(mDatabase);

        mDatastore->NotifyOtherProcessObservers(mDatabase, mDocumentURI,
                                                VoidString(), VoidLSValue(),
                                                VoidLSValue());

        break;
      }

      default:
        MOZ_CRASH("Should never get here!");
    }
  }

  mUsage = mDatastore->EndUpdateBatch(-1);

  return IPC_OK();
}

mozilla::ipc::IPCResult Snapshot::RecvFinish() {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(mFinishReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  Finish();

  return IPC_OK();
}

mozilla::ipc::IPCResult Snapshot::RecvLoaded() {
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(mFinishReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mLoadedReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mLoadedAllItems)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mLoadKeysReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  mLoadedReceived = true;

  mLoadedItems.Clear();
  mUnknownItems.Clear();
  mValues.Clear();
  mKeys.Clear();
  mLoadedAllItems = true;
  mLoadKeysReceived = true;

  return IPC_OK();
}

mozilla::ipc::IPCResult Snapshot::RecvLoadValueAndMoreItems(
    const nsString& aKey, LSValue* aValue, nsTArray<LSItemInfo>* aItemInfos) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aValue);
  MOZ_ASSERT(aItemInfos);
  MOZ_ASSERT(mDatastore);

  if (NS_WARN_IF(mFinishReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mLoadedReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mLoadedAllItems)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (mLoadedItems.GetEntry(aKey) || mUnknownItems.GetEntry(aKey)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (auto entry = mValues.Lookup(aKey)) {
    *aValue = entry.Data();
    entry.Remove();
  } else {
    mDatastore->GetItem(aKey, *aValue);
  }

  if (aValue->IsVoid()) {
    mUnknownItems.PutEntry(aKey);
  } else {
    mLoadedItems.PutEntry(aKey);

    // mLoadedItems.Count()==mTotalLength is checked below.
  }

  // Load some more key/value pairs (as many as the snapshot gradual prefill
  // byte budget allows).

  if (gSnapshotGradualPrefill > 0) {
    const nsTArray<LSItemInfo>& orderedItems = mDatastore->GetOrderedItems();

    uint32_t length;
    if (mSavedKeys) {
      length = mKeys.Length();
    } else {
      length = orderedItems.Length();
    }

    int64_t size = 0;
    while (mNextLoadIndex < length) {
      // If the datastore's ordering has changed, mSavedKeys will be true and
      // mKeys contains an ordered list of the keys. Otherwise we can use the
      // datastore's key ordering which is still the same as when the snapshot
      // was created.

      nsString key;
      if (mSavedKeys) {
        key = mKeys[mNextLoadIndex];
      } else {
        key = orderedItems[mNextLoadIndex].key();
      }

      // Normally we would do this:
      // if (!mLoadedItems.GetEntry(key)) {
      //   ...
      //   mLoadedItems.PutEntry(key);
      // }
      // but that requires two hash lookups. We can reduce that to just one
      // hash lookup if we always call PutEntry and check the number of entries
      // before and after the put (which is very cheap). However, if we reach
      // the prefill limit, we need to call RemoveEntry, but that is also cheap
      // because we pass the entry (not the key).

      uint32_t countBeforePut = mLoadedItems.Count();
      auto loadedItemEntry = mLoadedItems.PutEntry(key);
      if (countBeforePut != mLoadedItems.Count()) {
        // Check mValues first since that contains values as they existed when
        // our snapshot was created, but have since been changed/removed in the
        // datastore. If it's not there, then the datastore has the
        // still-current value. However, if the datastore's key ordering has
        // changed, we need to do a hash lookup rather than being able to do an
        // optimized direct access to the index.

        LSValue value;
        auto valueEntry = mValues.Lookup(key);
        if (valueEntry) {
          value = valueEntry.Data();
        } else if (mSavedKeys) {
          mDatastore->GetItem(nsString(key), value);
        } else {
          value = orderedItems[mNextLoadIndex].value();
        }

        // All not loaded keys must have a value.
        MOZ_ASSERT(!value.IsVoid());

        size += static_cast<int64_t>(key.Length()) +
                static_cast<int64_t>(value.Length());

        if (size > gSnapshotGradualPrefill) {
          mLoadedItems.RemoveEntry(loadedItemEntry);

          // mNextLoadIndex is not incremented, so we will resume at the same
          // position next time.
          break;
        }

        if (valueEntry) {
          valueEntry.Remove();
        }

        LSItemInfo* itemInfo = aItemInfos->AppendElement();
        itemInfo->key() = key;
        itemInfo->value() = value;
      }

      mNextLoadIndex++;
    }
  }

  if (mLoadedItems.Count() == mTotalLength) {
    mLoadedItems.Clear();
    mUnknownItems.Clear();
#ifdef DEBUG
    for (auto iter = mValues.ConstIter(); !iter.Done(); iter.Next()) {
      MOZ_ASSERT(iter.Data().IsVoid());
    }
#endif
    mValues.Clear();
    mLoadedAllItems = true;
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult Snapshot::RecvLoadKeys(nsTArray<nsString>* aKeys) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aKeys);
  MOZ_ASSERT(mDatastore);

  if (NS_WARN_IF(mFinishReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mLoadedReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mLoadKeysReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  mLoadKeysReceived = true;

  if (mSavedKeys) {
    aKeys->AppendElements(std::move(mKeys));
  } else {
    mDatastore->GetKeys(*aKeys);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult Snapshot::RecvIncreasePeakUsage(
    const int64_t& aRequestedSize, const int64_t& aMinSize, int64_t* aSize) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aSize);

  if (NS_WARN_IF(aRequestedSize <= 0)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(aMinSize <= 0)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (NS_WARN_IF(mFinishReceived)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  int64_t size = mDatastore->RequestUpdateUsage(aRequestedSize, aMinSize);

  mPeakUsage += size;

  *aSize = size;

  return IPC_OK();
}

mozilla::ipc::IPCResult Snapshot::RecvPing() {
  AssertIsOnBackgroundThread();

  // Do nothing here. This is purely a sync message allowing the child to
  // confirm that the actor has received previous async message.

  return IPC_OK();
}

/*******************************************************************************
 * Observer
 ******************************************************************************/

Observer::Observer(const nsACString& aOrigin)
    : mOrigin(aOrigin), mActorDestroyed(false) {
  AssertIsOnBackgroundThread();
}

Observer::~Observer() { MOZ_ASSERT(mActorDestroyed); }

void Observer::Observe(Database* aDatabase, const nsString& aDocumentURI,
                       const nsString& aKey, const LSValue& aOldValue,
                       const LSValue& aNewValue) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);

  Unused << SendObserve(aDatabase->GetPrincipalInfo(),
                        aDatabase->PrivateBrowsingId(), aDocumentURI, aKey,
                        aOldValue, aNewValue);
}

void Observer::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  mActorDestroyed = true;

  MOZ_ASSERT(gObservers);

  nsTArray<Observer*>* array;
  gObservers->Get(mOrigin, &array);
  MOZ_ASSERT(array);

  array->RemoveElement(this);

  if (RefPtr<Datastore> datastore = GetDatastore(mOrigin)) {
    datastore->NoteChangedObserverArray(*array);
  }

  if (array->IsEmpty()) {
    gObservers->Remove(mOrigin);
  }

  if (!gObservers->Count()) {
    gObservers = nullptr;
  }
}

mozilla::ipc::IPCResult Observer::RecvDeleteMe() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  IProtocol* mgr = Manager();
  if (!PBackgroundLSObserverParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

/*******************************************************************************
 * LSRequestBase
 ******************************************************************************/

LSRequestBase::LSRequestBase(nsIEventTarget* aMainEventTarget,
                             const LSRequestParams& aParams,
                             const Maybe<ContentParentId>& aContentParentId)
    : mMainEventTarget(aMainEventTarget),
      mParams(aParams),
      mContentParentId(aContentParentId),
      mState(State::Initial),
      mWaitingForFinish(false) {}

LSRequestBase::~LSRequestBase() {
  MOZ_ASSERT_IF(MayProceedOnNonOwningThread(),
                mState == State::Initial || mState == State::Completed);
}

void LSRequestBase::StringifyState(nsCString& aResult) const {
  AssertIsOnOwningThread();

  switch (mState) {
    case State::Initial:
      aResult.AssignLiteral("Initial");
      return;

    case State::StartingRequest:
      aResult.AssignLiteral("StartingRequest");
      return;

    case State::Nesting:
      aResult.AssignLiteral("Nesting");
      return;

    case State::SendingReadyMessage:
      aResult.AssignLiteral("SendingReadyMessage");
      return;

    case State::WaitingForFinish:
      aResult.AssignLiteral("WaitingForFinish");
      return;

    case State::SendingResults:
      aResult.AssignLiteral("SendingResults");
      return;

    case State::Completed:
      aResult.AssignLiteral("Completed");
      return;

    default:
      MOZ_CRASH("Bad state!");
  }
}

void LSRequestBase::Dispatch() {
  AssertIsOnOwningThread();

  mState = State::StartingRequest;

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(this));
}

nsresult LSRequestBase::NestedRun() { return NS_OK; }

void LSRequestBase::LogState() {
  AssertIsOnOwningThread();

  if (!LS_LOG_TEST()) {
    return;
  }

  LS_LOG(("LSRequestBase [%p]", this));

  nsCString state;
  StringifyState(state);

  LS_LOG(("  mState: %s", state.get()));

  switch (mState) {
    case State::Nesting:
      LogNestedState();
      break;

    default:;
  }
}

bool LSRequestBase::VerifyRequestParams() {
  AssertIsOnBackgroundThread();

  MOZ_ASSERT(mParams.type() != LSRequestParams::T__None);

  switch (mParams.type()) {
    case LSRequestParams::TLSRequestPreloadDatastoreParams: {
      const LSRequestCommonParams& params =
          mParams.get_LSRequestPreloadDatastoreParams().commonParams();

      if (NS_WARN_IF(!VerifyPrincipalInfo(params.principalInfo(),
                                          params.storagePrincipalInfo()))) {
        return false;
      }

      if (NS_WARN_IF(
              !VerifyOriginKey(params.originKey(), params.principalInfo()))) {
        return false;
      }

      break;
    }

    case LSRequestParams::TLSRequestPrepareDatastoreParams: {
      const LSRequestPrepareDatastoreParams& params =
          mParams.get_LSRequestPrepareDatastoreParams();

      const LSRequestCommonParams& commonParams = params.commonParams();

      if (NS_WARN_IF(
              !VerifyPrincipalInfo(commonParams.principalInfo(),
                                   commonParams.storagePrincipalInfo()))) {
        return false;
      }

      if (NS_WARN_IF(!VerifyClientId(mContentParentId,
                                     commonParams.principalInfo(),
                                     params.clientId()))) {
        return false;
      }

      if (NS_WARN_IF(!VerifyOriginKey(commonParams.originKey(),
                                      commonParams.principalInfo()))) {
        return false;
      }

      break;
    }

    case LSRequestParams::TLSRequestPrepareObserverParams: {
      const LSRequestPrepareObserverParams& params =
          mParams.get_LSRequestPrepareObserverParams();

      if (NS_WARN_IF(!VerifyPrincipalInfo(params.principalInfo(),
                                          params.storagePrincipalInfo()))) {
        return false;
      }

      if (NS_WARN_IF(!VerifyClientId(mContentParentId, params.principalInfo(),
                                     params.clientId()))) {
        return false;
      }

      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  return true;
}

nsresult LSRequestBase::StartRequest() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::StartingRequest);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !MayProceed()) {
    return NS_ERROR_FAILURE;
  }

#ifdef DEBUG
  // Always verify parameters in DEBUG builds!
  bool trustParams = false;
#else
  bool trustParams = !BackgroundParent::IsOtherProcessActor(Manager());
#endif

  if (!trustParams && NS_WARN_IF(!VerifyRequestParams())) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = Start();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void LSRequestBase::SendReadyMessage() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingReadyMessage);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !MayProceed()) {
    MaybeSetFailureCode(NS_ERROR_FAILURE);
  }

  nsresult rv = SendReadyMessageInternal();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MaybeSetFailureCode(rv);

    FinishInternal();
  }
}

nsresult LSRequestBase::SendReadyMessageInternal() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingReadyMessage);

  if (!MayProceed()) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(!SendReady())) {
    return NS_ERROR_FAILURE;
  }

  mState = State::WaitingForFinish;

  mWaitingForFinish = true;

  return NS_OK;
}

void LSRequestBase::Finish() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::WaitingForFinish);

  mWaitingForFinish = false;

  FinishInternal();
}

void LSRequestBase::FinishInternal() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingReadyMessage ||
             mState == State::WaitingForFinish);

  mState = State::SendingResults;

  // This LSRequestBase can only be held alive by the IPDL. Run() can end up
  // with clearing that last reference. So we need to add a self reference here.
  RefPtr<LSRequestBase> kungFuDeathGrip = this;

  MOZ_ALWAYS_SUCCEEDS(this->Run());
}

void LSRequestBase::SendResults() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingResults);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !MayProceed()) {
    MaybeSetFailureCode(NS_ERROR_FAILURE);
  }

  if (MayProceed()) {
    LSRequestResponse response;

    if (NS_SUCCEEDED(ResultCode())) {
      GetResponse(response);
    } else {
      response = ResultCode();
    }

    Unused << PBackgroundLSRequestParent::Send__delete__(this, response);
  }

  Cleanup();

  mState = State::Completed;
}

NS_IMETHODIMP
LSRequestBase::Run() {
  nsresult rv;

  switch (mState) {
    case State::StartingRequest:
      rv = StartRequest();
      break;

    case State::Nesting:
      rv = NestedRun();
      break;

    case State::SendingReadyMessage:
      SendReadyMessage();
      return NS_OK;

    case State::SendingResults:
      SendResults();
      return NS_OK;

    default:
      MOZ_CRASH("Bad state!");
  }

  if (NS_WARN_IF(NS_FAILED(rv)) && mState != State::SendingReadyMessage) {
    MaybeSetFailureCode(rv);

    // Must set mState before dispatching otherwise we will race with the owning
    // thread.
    mState = State::SendingReadyMessage;

    if (IsOnOwningThread()) {
      SendReadyMessage();
    } else {
      MOZ_ALWAYS_SUCCEEDS(
          OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL));
    }
  }

  return NS_OK;
}

void LSRequestBase::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  NoteComplete();

  // Assume ActorDestroy can happen at any time, so we can't probe the current
  // state since mState can be modified on any thread (only one thread at a time
  // based on the state machine).  However we can use mWaitingForFinish which is
  // only touched on the owning thread.  If mWaitingForFinisg is true, we can
  // also modify mState since we are guaranteed that there are no pending
  // runnables which would probe mState to decide what code needs to run (there
  // shouldn't be any running runnables on other threads either).

  if (mWaitingForFinish) {
    Finish();
  }

  // We don't have to handle the case when mWaitingForFinish is not true since
  // it means that either nothing has been initialized yet, so nothing to
  // cleanup or there are pending runnables that will detect that the actor has
  // been destroyed and cleanup accordingly.
}

mozilla::ipc::IPCResult LSRequestBase::RecvCancel() {
  AssertIsOnOwningThread();

  LogState();

  const char* crashOnCancel = PR_GetEnv("LSNG_CRASH_ON_CANCEL");
  if (crashOnCancel) {
    MOZ_CRASH("LSNG: Crash on cancel.");
  }

  IProtocol* mgr = Manager();
  if (!PBackgroundLSRequestParent::Send__delete__(this, NS_ERROR_FAILURE)) {
    return IPC_FAIL_NO_REASON(mgr);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult LSRequestBase::RecvFinish() {
  AssertIsOnOwningThread();

  Finish();

  return IPC_OK();
}

/*******************************************************************************
 * PrepareDatastoreOp
 ******************************************************************************/

PrepareDatastoreOp::PrepareDatastoreOp(
    nsIEventTarget* aMainEventTarget, const LSRequestParams& aParams,
    const Maybe<ContentParentId>& aContentParentId)
    : LSRequestBase(aMainEventTarget, aParams, aContentParentId),
      mMainEventTarget(aMainEventTarget),
      mLoadDataOp(nullptr),
      mPrivateBrowsingId(0),
      mUsage(0),
      mSizeOfKeys(0),
      mSizeOfItems(0),
      mDatastoreId(0),
      mNestedState(NestedState::BeforeNesting),
      mForPreload(aParams.type() ==
                  LSRequestParams::TLSRequestPreloadDatastoreParams),
      mDatabaseNotAvailable(false),
      mRequestedDirectoryLock(false),
      mInvalidated(false)
#ifdef DEBUG
      ,
      mDEBUGUsage(0)
#endif
{
  MOZ_ASSERT(
      aParams.type() == LSRequestParams::TLSRequestPreloadDatastoreParams ||
      aParams.type() == LSRequestParams::TLSRequestPrepareDatastoreParams);
}

PrepareDatastoreOp::~PrepareDatastoreOp() {
  MOZ_ASSERT(!mDirectoryLock);
  MOZ_ASSERT_IF(MayProceedOnNonOwningThread(),
                mState == State::Initial || mState == State::Completed);
  MOZ_ASSERT(!mLoadDataOp);
}

void PrepareDatastoreOp::StringifyNestedState(nsCString& aResult) const {
  AssertIsOnOwningThread();

  switch (mNestedState) {
    case NestedState::BeforeNesting:
      aResult.AssignLiteral("BeforeNesting");
      return;

    case NestedState::CheckExistingOperations:
      aResult.AssignLiteral("CheckExistingOperations");
      return;

    case NestedState::CheckClosingDatastore:
      aResult.AssignLiteral("CheckClosingDatastore");
      return;

    case NestedState::PreparationPending:
      aResult.AssignLiteral("PreparationPending");
      return;

    case NestedState::QuotaManagerPending:
      aResult.AssignLiteral("QuotaManagerPending");
      return;

    case NestedState::DirectoryOpenPending:
      aResult.AssignLiteral("DirectoryOpenPending");
      return;

    case NestedState::DatabaseWorkOpen:
      aResult.AssignLiteral("DatabaseWorkOpen");
      return;

    case NestedState::BeginLoadData:
      aResult.AssignLiteral("BeginLoadData");
      return;

    case NestedState::DatabaseWorkLoadData:
      aResult.AssignLiteral("DatabaseWorkLoadData");
      return;

    case NestedState::AfterNesting:
      aResult.AssignLiteral("AfterNesting");
      return;

    default:
      MOZ_CRASH("Bad state!");
  }
}

nsresult PrepareDatastoreOp::Start() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::StartingRequest);
  MOZ_ASSERT(mNestedState == NestedState::BeforeNesting);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());
  MOZ_ASSERT(MayProceed());

  const LSRequestCommonParams& commonParams =
      mForPreload
          ? mParams.get_LSRequestPreloadDatastoreParams().commonParams()
          : mParams.get_LSRequestPrepareDatastoreParams().commonParams();

  const PrincipalInfo& storagePrincipalInfo =
      commonParams.storagePrincipalInfo();

  if (storagePrincipalInfo.type() == PrincipalInfo::TSystemPrincipalInfo) {
    QuotaManager::GetInfoForChrome(&mSuffix, &mGroup, &mOrigin);
  } else {
    MOZ_ASSERT(storagePrincipalInfo.type() ==
               PrincipalInfo::TContentPrincipalInfo);

    QuotaManager::GetInfoFromValidatedPrincipalInfo(
        storagePrincipalInfo, &mSuffix, &mGroup, &mMainThreadOrigin);
  }

  mState = State::Nesting;
  mNestedState = NestedState::CheckExistingOperations;

  MOZ_ALWAYS_SUCCEEDS(OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

nsresult PrepareDatastoreOp::CheckExistingOperations() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::CheckExistingOperations);
  MOZ_ASSERT(gPrepareDatastoreOps);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !MayProceed()) {
    return NS_ERROR_FAILURE;
  }

  const LSRequestCommonParams& commonParams =
      mForPreload
          ? mParams.get_LSRequestPreloadDatastoreParams().commonParams()
          : mParams.get_LSRequestPrepareDatastoreParams().commonParams();

  const PrincipalInfo& storagePrincipalInfo =
      commonParams.storagePrincipalInfo();

  nsCString originAttrSuffix;
  uint32_t privateBrowsingId;

  if (storagePrincipalInfo.type() == PrincipalInfo::TSystemPrincipalInfo) {
    privateBrowsingId = 0;
  } else {
    MOZ_ASSERT(storagePrincipalInfo.type() ==
               PrincipalInfo::TContentPrincipalInfo);

    const ContentPrincipalInfo& info =
        storagePrincipalInfo.get_ContentPrincipalInfo();
    const OriginAttributes& attrs = info.attrs();
    attrs.CreateSuffix(originAttrSuffix);

    privateBrowsingId = attrs.mPrivateBrowsingId;
  }

  mArchivedOriginScope = ArchivedOriginScope::CreateFromOrigin(
      originAttrSuffix, commonParams.originKey());
  MOZ_ASSERT(mArchivedOriginScope);

  // Normally it's safe to access member variables without a mutex because even
  // though we hop between threads, the variables are never accessed by multiple
  // threads at the same time.
  // However, the methods OriginIsKnown and Origin can be called at any time.
  // So we have to make sure the member variable is set on the same thread as
  // those methods are called.
  mOrigin = mMainThreadOrigin;

  MOZ_ASSERT(!mOrigin.IsEmpty());

  mPrivateBrowsingId = privateBrowsingId;

  mNestedState = NestedState::CheckClosingDatastore;

  // See if this PrepareDatastoreOp needs to wait.
  bool foundThis = false;
  for (uint32_t index = gPrepareDatastoreOps->Length(); index > 0; index--) {
    PrepareDatastoreOp* existingOp = (*gPrepareDatastoreOps)[index - 1];

    if (existingOp == this) {
      foundThis = true;
      continue;
    }

    if (foundThis && existingOp->Origin() == mOrigin) {
      // Only one op can be delayed.
      MOZ_ASSERT(!existingOp->mDelayedOp);
      existingOp->mDelayedOp = this;

      return NS_OK;
    }
  }

  nsresult rv = CheckClosingDatastoreInternal();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult PrepareDatastoreOp::CheckClosingDatastore() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::CheckClosingDatastore);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !MayProceed()) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = CheckClosingDatastoreInternal();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult PrepareDatastoreOp::CheckClosingDatastoreInternal() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::CheckClosingDatastore);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());
  MOZ_ASSERT(MayProceed());

  mNestedState = NestedState::PreparationPending;

  RefPtr<Datastore> datastore;
  if ((datastore = GetDatastore(mOrigin)) && datastore->IsClosed()) {
    datastore->WaitForConnectionToComplete(this);

    return NS_OK;
  }

  nsresult rv = BeginDatastorePreparationInternal();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult PrepareDatastoreOp::BeginDatastorePreparation() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::PreparationPending);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !MayProceed()) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = BeginDatastorePreparationInternal();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult PrepareDatastoreOp::BeginDatastorePreparationInternal() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::PreparationPending);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());
  MOZ_ASSERT(MayProceed());

  if ((mDatastore = GetDatastore(mOrigin))) {
    MOZ_ASSERT(!mDatastore->IsClosed());

    mDatastore->NoteLivePrepareDatastoreOp(this);

    FinishNesting();

    return NS_OK;
  }

  if (QuotaManager::Get()) {
    nsresult rv = OpenDirectory();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

  mNestedState = NestedState::QuotaManagerPending;
  QuotaManager::GetOrCreate(this, mMainEventTarget);

  return NS_OK;
}

nsresult PrepareDatastoreOp::QuotaManagerOpen() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::QuotaManagerPending);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !MayProceed()) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(!QuotaManager::Get())) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = OpenDirectory();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult PrepareDatastoreOp::OpenDirectory() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::PreparationPending ||
             mNestedState == NestedState::QuotaManagerPending);
  MOZ_ASSERT(!mOrigin.IsEmpty());
  MOZ_ASSERT(!mDirectoryLock);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());
  MOZ_ASSERT(MayProceed());
  MOZ_ASSERT(QuotaManager::Get());

  mNestedState = NestedState::DirectoryOpenPending;
  RefPtr<DirectoryLock> pendingDirectoryLock =
      QuotaManager::Get()->CreateDirectoryLock(PERSISTENCE_TYPE_DEFAULT, mGroup,
                                               mOrigin,
                                               mozilla::dom::quota::Client::LS,
                                               /* aExclusive */ false, this);
  MOZ_ASSERT(pendingDirectoryLock);

  if (mNestedState == NestedState::DirectoryOpenPending) {
    mPendingDirectoryLock = pendingDirectoryLock.forget();
  }

  mRequestedDirectoryLock = true;

  return NS_OK;
}

void PrepareDatastoreOp::SendToIOThread() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::DirectoryOpenPending);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());
  MOZ_ASSERT(MayProceed());

  // Skip all disk related stuff and transition to SendingReadyMessage if we
  // are preparing a datastore for private browsing.
  // Note that we do use a directory lock for private browsing even though we
  // don't do any stuff on disk. The thing is that without a directory lock,
  // quota manager wouldn't call AbortOperations for our private browsing
  // origin when a clear origin operation is requested. AbortOperations
  // requests all databases to close and the datastore is destroyed in the end.
  // Any following LocalStorage API call will trigger preparation of a new
  // (empty) datastore.
  if (mPrivateBrowsingId) {
    FinishNesting();

    return;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  // Must set this before dispatching otherwise we will race with the IO thread.
  mNestedState = NestedState::DatabaseWorkOpen;

  MOZ_ALWAYS_SUCCEEDS(
      quotaManager->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL));
}

nsresult PrepareDatastoreOp::DatabaseWork() {
  AssertIsOnIOThread();
  MOZ_ASSERT(mArchivedOriginScope);
  MOZ_ASSERT(mUsage == 0);
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::DatabaseWorkOpen);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      !MayProceedOnNonOwningThread()) {
    return NS_ERROR_FAILURE;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  // This must be called before EnsureTemporaryStorageIsInitialized.
  nsresult rv = quotaManager->EnsureStorageIsInitialized();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // This ensures that usages for existings origin directories are cached in
  // memory.
  rv = quotaManager->EnsureTemporaryStorageIsInitialized();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint64_t usage;
  bool hasUsage =
      quotaManager->GetUsageForClient(PERSISTENCE_TYPE_DEFAULT, mGroup, mOrigin,
                                      mozilla::dom::quota::Client::LS, usage);

  if (!gArchivedOrigins) {
    rv = LoadArchivedOrigins();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    MOZ_ASSERT(gArchivedOrigins);
  }

  bool hasDataForMigration = mArchivedOriginScope->HasMatches(gArchivedOrigins);

  // If there's nothing to preload (except the case when we want to migrate data
  // during preloading), then we can finish the operation without creating a
  // datastore in GetResponse (GetResponse won't create a datastore if
  // mDatatabaseNotAvailable and mForPreload are both true).
  if (mForPreload && !hasUsage && !hasDataForMigration) {
    return DatabaseNotAvailable();
  }

  // The origin directory doesn't need to be created when we don't have data for
  // migration. It will be created on the connection thread in
  // Connection::EnsureStorageConnection.
  // However, origin quota must be initialized, GetQuotaObject in GetResponse
  // would fail otherwise.
  nsCOMPtr<nsIFile> directoryEntry;
  if (hasDataForMigration) {
    rv = quotaManager->EnsureOriginIsInitialized(
        PERSISTENCE_TYPE_DEFAULT, mSuffix, mGroup, mOrigin,
        getter_AddRefs(directoryEntry));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    rv = quotaManager->GetDirectoryForOrigin(PERSISTENCE_TYPE_DEFAULT, mOrigin,
                                             getter_AddRefs(directoryEntry));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    quotaManager->EnsureQuotaForOrigin(PERSISTENCE_TYPE_DEFAULT, mGroup,
                                       mOrigin);
  }

  rv = directoryEntry->Append(NS_LITERAL_STRING(LS_DIRECTORY_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsString directoryPath;
  rv = directoryEntry->GetPath(directoryPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // The ls directory doesn't need to be created when we don't have data for
  // migration. It will be created on the connection thread in
  // Connection::EnsureStorageConnection.
  rv = EnsureDirectoryEntry(directoryEntry,
                            /* aCreateIfNotExists */ hasDataForMigration,
                            /* aIsDirectory */ true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = directoryEntry->Append(NS_LITERAL_STRING(DATA_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = directoryEntry->GetPath(mDatabaseFilePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // The database doesn't need to be created when we don't have data for
  // migration. It will be created on the connection thread in
  // Connection::EnsureStorageConnection.
  bool alreadyExisted;
  rv = EnsureDirectoryEntry(directoryEntry,
                            /* aCreateIfNotExists */ hasDataForMigration,
                            /* aIsDirectory */ false, &alreadyExisted);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (alreadyExisted) {
    // The database does exist.
    MOZ_ASSERT(hasUsage);
    mUsage = usage;
  } else {
    // The database doesn't exist.
    MOZ_ASSERT(!hasUsage);

    if (!hasDataForMigration) {
      // The database doesn't exist and we don't have data for migration.
      // Finish the operation, but create an empty datastore in GetResponse
      // (GetResponse will create an empty datastore if mDatabaseNotAvailable
      // is true and mForPreload is false).
      return DatabaseNotAvailable();
    }
  }

  // We initialized mDatabaseFilePath and mUsage, GetQuotaObject can be called
  // from now on.
  RefPtr<QuotaObject> quotaObject;

  nsCOMPtr<nsIFile> usageFile;
  rv = GetUsageFile(directoryPath, getter_AddRefs(usageFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIFile> usageJournalFile;
  rv = GetUsageJournalFile(directoryPath, getter_AddRefs(usageJournalFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  bool removedUsageFile;

  rv = CreateStorageConnection(directoryEntry, usageFile, mOrigin,
                               getter_AddRefs(connection), &removedUsageFile);

  // removedUsageFile must be checked before rv since we may need to reset usage
  // even when CreateStorageConnection failed.
  if (removedUsageFile) {
    if (!quotaObject) {
      quotaObject = GetQuotaObject();
      MOZ_ASSERT(quotaObject);
    }

    MOZ_ALWAYS_TRUE(quotaObject->MaybeUpdateSize(0, /* aTruncate */ true));

    mUsage = 0;
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = VerifyDatabaseInformation(connection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (hasDataForMigration) {
    MOZ_ASSERT(mUsage == 0);

    rv = AttachArchiveDatabase(quotaManager->GetStoragePath(), connection);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    int64_t newUsage;
    rv = GetUsage(connection, mArchivedOriginScope, &newUsage);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!quotaObject) {
      quotaObject = GetQuotaObject();
      MOZ_ASSERT(quotaObject);
    }

    if (!quotaObject->MaybeUpdateSize(newUsage, /* aTruncate */ true)) {
      return NS_ERROR_FILE_NO_DEVICE_SPACE;
    }

    auto autoUpdateSize = MakeScopeExit([&quotaObject] {
      MOZ_ALWAYS_TRUE(quotaObject->MaybeUpdateSize(0, /* aTruncate */ true));
    });

    mozStorageTransaction transaction(
        connection, false, mozIStorageConnection::TRANSACTION_IMMEDIATE);

    nsCOMPtr<mozIStorageFunction> function = new CompressFunction();

    rv =
        connection->CreateFunction(NS_LITERAL_CSTRING("compress"), 1, function);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    function = new CompressibleFunction();

    rv = connection->CreateFunction(NS_LITERAL_CSTRING("compressible"), 1,
                                    function);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCOMPtr<mozIStorageStatement> stmt;
    rv = connection->CreateStatement(
        NS_LITERAL_CSTRING(
            "INSERT INTO data (key, value, utf16Length, compressed) "
            "SELECT key, compress(value), utf16Length(value), "
            "compressible(value) "
            "FROM webappsstore2 "
            "WHERE originKey = :originKey "
            "AND originAttributes = :originAttributes;"

            ),
        getter_AddRefs(stmt));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = mArchivedOriginScope->BindToStatement(stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = connection->RemoveFunction(NS_LITERAL_CSTRING("compress"));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = connection->RemoveFunction(NS_LITERAL_CSTRING("compressible"));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = connection->CreateStatement(
        NS_LITERAL_CSTRING("UPDATE database SET usage = :usage;"),
        getter_AddRefs(stmt));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("usage"), newUsage);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = connection->CreateStatement(
        NS_LITERAL_CSTRING("DELETE FROM webappsstore2 "
                           "WHERE originKey = :originKey "
                           "AND originAttributes = :originAttributes;"),
        getter_AddRefs(stmt));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = mArchivedOriginScope->BindToStatement(stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = UpdateUsageFile(usageFile, usageJournalFile, newUsage);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = transaction.Commit();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    autoUpdateSize.release();

    rv = usageJournalFile->Remove(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = DetachArchiveDatabase(connection);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(gArchivedOrigins);
    MOZ_ASSERT(mArchivedOriginScope->HasMatches(gArchivedOrigins));
    mArchivedOriginScope->RemoveMatches(gArchivedOrigins);

    mUsage = newUsage;
  }

  nsCOMPtr<mozIStorageConnection> shadowConnection;
  if (!gInitializedShadowStorage) {
    rv = CreateShadowStorageConnection(quotaManager->GetBasePath(),
                                       getter_AddRefs(shadowConnection));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    gInitializedShadowStorage = true;
  }

  // Must close connections before dispatching otherwise we might race with the
  // connection thread which needs to open the same databases.
  MOZ_ALWAYS_SUCCEEDS(connection->Close());

  if (shadowConnection) {
    MOZ_ALWAYS_SUCCEEDS(shadowConnection->Close());
  }

  // Must set this before dispatching otherwise we will race with the owning
  // thread.
  mNestedState = NestedState::BeginLoadData;

  rv = OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult PrepareDatastoreOp::DatabaseNotAvailable() {
  AssertIsOnIOThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::DatabaseWorkOpen);

  mDatabaseNotAvailable = true;

  nsresult rv = FinishNestingOnNonOwningThread();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult PrepareDatastoreOp::EnsureDirectoryEntry(nsIFile* aEntry,
                                                  bool aCreateIfNotExists,
                                                  bool aIsDirectory,
                                                  bool* aAlreadyExisted) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aEntry);

  bool exists;
  nsresult rv = aEntry->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    if (!aCreateIfNotExists) {
      if (aAlreadyExisted) {
        *aAlreadyExisted = false;
      }
      return NS_OK;
    }

    if (aIsDirectory) {
      rv = aEntry->Create(nsIFile::DIRECTORY_TYPE, 0755);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }
#ifdef DEBUG
  else {
    bool isDirectory;
    MOZ_ASSERT(NS_SUCCEEDED(aEntry->IsDirectory(&isDirectory)));
    MOZ_ASSERT(isDirectory == aIsDirectory);
  }
#endif

  if (aAlreadyExisted) {
    *aAlreadyExisted = exists;
  }
  return NS_OK;
}

nsresult PrepareDatastoreOp::VerifyDatabaseInformation(
    mozIStorageConnection* aConnection) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv =
      aConnection->CreateStatement(NS_LITERAL_CSTRING("SELECT origin "
                                                      "FROM database"),
                                   getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!hasResult)) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  nsCString origin;
  rv = stmt->GetUTF8String(0, origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!QuotaManager::AreOriginsEqualOnDisk(mOrigin, origin))) {
    return NS_ERROR_FILE_CORRUPTED;
  }

  return NS_OK;
}

already_AddRefed<QuotaObject> PrepareDatastoreOp::GetQuotaObject() {
  MOZ_ASSERT(IsOnOwningThread() || IsOnIOThread());
  MOZ_ASSERT(!mGroup.IsEmpty());
  MOZ_ASSERT(!mOrigin.IsEmpty());
  MOZ_ASSERT(!mDatabaseFilePath.IsEmpty());

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  RefPtr<QuotaObject> quotaObject = quotaManager->GetQuotaObject(
      PERSISTENCE_TYPE_DEFAULT, mGroup, mOrigin,
      mozilla::dom::quota::Client::LS, mDatabaseFilePath, mUsage);
  MOZ_ASSERT(quotaObject);

  return quotaObject.forget();
}

nsresult PrepareDatastoreOp::BeginLoadData() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::BeginLoadData);
  MOZ_ASSERT(!mConnection);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !MayProceed()) {
    return NS_ERROR_FAILURE;
  }

  if (!gConnectionThread) {
    gConnectionThread = new ConnectionThread();
  }

  mConnection = gConnectionThread->CreateConnection(
      mSuffix, mGroup, mOrigin, std::move(mArchivedOriginScope),
      /* aDatabaseWasNotAvailable */ false);
  MOZ_ASSERT(mConnection);

  // Must set this before dispatching otherwise we will race with the
  // connection thread.
  mNestedState = NestedState::DatabaseWorkLoadData;

  // Can't assign to mLoadDataOp directly since that's a weak reference and
  // LoadDataOp is reference counted.
  RefPtr<LoadDataOp> loadDataOp = new LoadDataOp(this);

  // This add refs loadDataOp.
  mConnection->Dispatch(loadDataOp);

  // This is cleared in LoadDataOp::Cleanup() before the load data op is
  // destroyed.
  mLoadDataOp = loadDataOp;

  return NS_OK;
}

void PrepareDatastoreOp::FinishNesting() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);

  // The caller holds a strong reference to us, no need for a self reference
  // before calling Run().

  mState = State::SendingReadyMessage;
  mNestedState = NestedState::AfterNesting;

  MOZ_ALWAYS_SUCCEEDS(Run());
}

nsresult PrepareDatastoreOp::FinishNestingOnNonOwningThread() {
  MOZ_ASSERT(!IsOnOwningThread());
  MOZ_ASSERT(mState == State::Nesting);

  // Must set mState before dispatching otherwise we will race with the owning
  // thread.
  mState = State::SendingReadyMessage;
  mNestedState = NestedState::AfterNesting;

  nsresult rv = OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult PrepareDatastoreOp::NestedRun() {
  nsresult rv;

  switch (mNestedState) {
    case NestedState::CheckExistingOperations:
      rv = CheckExistingOperations();
      break;

    case NestedState::CheckClosingDatastore:
      rv = CheckClosingDatastore();
      break;

    case NestedState::PreparationPending:
      rv = BeginDatastorePreparation();
      break;

    case NestedState::QuotaManagerPending:
      rv = QuotaManagerOpen();
      break;

    case NestedState::DatabaseWorkOpen:
      rv = DatabaseWork();
      break;

    case NestedState::BeginLoadData:
      rv = BeginLoadData();
      break;

    default:
      MOZ_CRASH("Bad state!");
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    mNestedState = NestedState::AfterNesting;

    return rv;
  }

  return NS_OK;
}

void PrepareDatastoreOp::GetResponse(LSRequestResponse& aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingResults);
  MOZ_ASSERT(NS_SUCCEEDED(ResultCode()));
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());
  MOZ_ASSERT(MayProceed());

  // A datastore is not created when we are just trying to preload data and
  // there's no database file.
  if (mDatabaseNotAvailable && mForPreload) {
    LSRequestPreloadDatastoreResponse preloadDatastoreResponse;

    aResponse = preloadDatastoreResponse;

    return;
  }

  if (!mDatastore) {
    MOZ_ASSERT(mUsage == mDEBUGUsage);

    RefPtr<QuotaObject> quotaObject;

    if (mPrivateBrowsingId == 0) {
      if (!mConnection) {
        // This can happen when there's no database file.
        MOZ_ASSERT(mDatabaseNotAvailable);

        // Even though there's no database file, we need to create a connection
        // and pass it to datastore.
        if (!gConnectionThread) {
          gConnectionThread = new ConnectionThread();
        }

        mConnection = gConnectionThread->CreateConnection(
            mSuffix, mGroup, mOrigin, std::move(mArchivedOriginScope),
            /* aDatabaseWasNotAvailable */ true);
        MOZ_ASSERT(mConnection);
      }

      quotaObject = GetQuotaObject();
      MOZ_ASSERT(quotaObject);
    }

    mDatastore = new Datastore(mGroup, mOrigin, mPrivateBrowsingId, mUsage,
                               mSizeOfKeys, mSizeOfItems,
                               mDirectoryLock.forget(), mConnection.forget(),
                               quotaObject.forget(), mValues, mOrderedItems);

    mDatastore->NoteLivePrepareDatastoreOp(this);

    if (!gDatastores) {
      gDatastores = new DatastoreHashtable();
    }

    MOZ_ASSERT(!gDatastores->Get(mOrigin));
    gDatastores->Put(mOrigin, mDatastore);
  }

  mDatastoreId = ++gLastDatastoreId;

  nsAutoPtr<PreparedDatastore> preparedDatastore(
      new PreparedDatastore(mDatastore, mContentParentId, mOrigin, mDatastoreId,
                            /* aForPreload */ mForPreload));

  if (!gPreparedDatastores) {
    gPreparedDatastores = new PreparedDatastoreHashtable();
  }
  gPreparedDatastores->Put(mDatastoreId, preparedDatastore);

  if (mInvalidated) {
    preparedDatastore->Invalidate();
  }

  preparedDatastore.forget();

  if (mForPreload) {
    LSRequestPreloadDatastoreResponse preloadDatastoreResponse;

    aResponse = preloadDatastoreResponse;
  } else {
    LSRequestPrepareDatastoreResponse prepareDatastoreResponse;
    prepareDatastoreResponse.datastoreId() = mDatastoreId;

    aResponse = prepareDatastoreResponse;
  }
}

void PrepareDatastoreOp::Cleanup() {
  AssertIsOnOwningThread();

  if (mDatastore) {
    MOZ_ASSERT(mDatastoreId > 0);
    MOZ_ASSERT(!mDirectoryLock);
    MOZ_ASSERT(!mConnection);

    if (NS_FAILED(ResultCode())) {
      // Just in case we failed to send datastoreId to the child, we need to
      // destroy prepared datastore, otherwise it won't be destroyed until the
      // timer fires (after 20 seconds).
      MOZ_ASSERT(gPreparedDatastores);
      MOZ_ASSERT(gPreparedDatastores->Get(mDatastoreId));

      nsAutoPtr<PreparedDatastore> preparedDatastore;
      gPreparedDatastores->Remove(mDatastoreId, &preparedDatastore);
      MOZ_ASSERT(preparedDatastore);
    }

    // Make sure to release the datastore on this thread.

    mDatastore->NoteFinishedPrepareDatastoreOp(this);

    mDatastore = nullptr;

    CleanupMetadata();
  } else if (mConnection) {
    // If we have a connection then the operation must have failed and there
    // must be a directory lock too.
    MOZ_ASSERT(NS_FAILED(ResultCode()));
    MOZ_ASSERT(mDirectoryLock);

    // We must close the connection on the connection thread before releasing
    // it on this thread. The directory lock can't be released either.
    nsCOMPtr<nsIRunnable> callback =
        NewRunnableMethod("dom::OpenDatabaseOp::ConnectionClosedCallback", this,
                          &PrepareDatastoreOp::ConnectionClosedCallback);

    mConnection->Close(callback);
  } else {
    // If we don't have a connection, but we do have a directory lock then the
    // operation must have failed or we were preloading a datastore and there
    // was no physical database on disk.
    MOZ_ASSERT_IF(mDirectoryLock,
                  NS_FAILED(ResultCode()) || mDatabaseNotAvailable);

    // There's no connection, so it's safe to release the directory lock and
    // unregister itself from the array.

    mDirectoryLock = nullptr;

    CleanupMetadata();
  }
}

void PrepareDatastoreOp::ConnectionClosedCallback() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(ResultCode()));
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(mConnection);

  mConnection = nullptr;
  mDirectoryLock = nullptr;

  CleanupMetadata();
}

void PrepareDatastoreOp::CleanupMetadata() {
  AssertIsOnOwningThread();

  if (mDelayedOp) {
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(mDelayedOp.forget()));
  }

  MOZ_ASSERT(gPrepareDatastoreOps);
  gPrepareDatastoreOps->RemoveElement(this);

  if (gPrepareDatastoreOps->IsEmpty()) {
    gPrepareDatastoreOps = nullptr;
  }
}

void PrepareDatastoreOp::LogNestedState() {
  AssertIsOnOwningThread();

  nsCString nestedState;
  StringifyNestedState(nestedState);

  LS_LOG(("  mNestedState: %s", nestedState.get()));

  switch (mNestedState) {
    case NestedState::CheckClosingDatastore: {
      for (uint32_t index = gPrepareDatastoreOps->Length(); index > 0;
           index--) {
        PrepareDatastoreOp* existingOp = (*gPrepareDatastoreOps)[index - 1];

        if (existingOp->mDelayedOp == this) {
          LS_LOG(("  mDelayedBy: [%p]", existingOp));

          existingOp->LogState();

          break;
        }
      }

      break;
    }

    case NestedState::DirectoryOpenPending: {
      MOZ_ASSERT(mPendingDirectoryLock);

      LS_LOG(("  mPendingDirectoryLock: [%p]", mPendingDirectoryLock.get()));

      mPendingDirectoryLock->LogState();

      break;
    }

    default:;
  }
}

NS_IMPL_ISUPPORTS_INHERITED0(PrepareDatastoreOp, LSRequestBase)

void PrepareDatastoreOp::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  LSRequestBase::ActorDestroy(aWhy);

  if (mLoadDataOp) {
    mLoadDataOp->NoteComplete();
  }
}

void PrepareDatastoreOp::DirectoryLockAcquired(DirectoryLock* aLock) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  mPendingDirectoryLock = nullptr;

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !MayProceed()) {
    MaybeSetFailureCode(NS_ERROR_FAILURE);

    FinishNesting();

    return;
  }

  mDirectoryLock = aLock;

  SendToIOThread();
}

void PrepareDatastoreOp::DirectoryLockFailed() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  mPendingDirectoryLock = nullptr;

  MaybeSetFailureCode(NS_ERROR_FAILURE);

  FinishNesting();
}

nsresult PrepareDatastoreOp::LoadDataOp::DoDatastoreWork() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mConnection);
  MOZ_ASSERT(mPrepareDatastoreOp);
  MOZ_ASSERT(mPrepareDatastoreOp->mState == State::Nesting);
  MOZ_ASSERT(mPrepareDatastoreOp->mNestedState ==
             NestedState::DatabaseWorkLoadData);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      !MayProceedOnNonOwningThread()) {
    return NS_ERROR_FAILURE;
  }

  Connection::CachedStatement stmt;
  nsresult rv = mConnection->GetCachedStatement(
      NS_LITERAL_CSTRING("SELECT key, value, utf16Length, compressed "
                         "FROM data;"),
      &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool hasResult;
  while (NS_SUCCEEDED(rv = stmt->ExecuteStep(&hasResult)) && hasResult) {
    nsString key;
    rv = stmt->GetString(0, key);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCString buffer;
    rv = stmt->GetUTF8String(1, buffer);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    int32_t utf16Length;
    rv = stmt->GetInt32(2, &utf16Length);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    int32_t compressed;
    rv = stmt->GetInt32(3, &compressed);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    LSValue value(buffer, utf16Length, compressed);

    mPrepareDatastoreOp->mValues.Put(key, value);
    auto item = mPrepareDatastoreOp->mOrderedItems.AppendElement();
    item->key() = key;
    item->value() = value;
    mPrepareDatastoreOp->mSizeOfKeys += key.Length();
    mPrepareDatastoreOp->mSizeOfItems += key.Length() + value.Length();
#ifdef DEBUG
    mPrepareDatastoreOp->mDEBUGUsage += key.Length() + value.UTF16Length();
#endif
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void PrepareDatastoreOp::LoadDataOp::OnSuccess() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mPrepareDatastoreOp);
  MOZ_ASSERT(mPrepareDatastoreOp->mState == State::Nesting);
  MOZ_ASSERT(mPrepareDatastoreOp->mNestedState ==
             NestedState::DatabaseWorkLoadData);
  MOZ_ASSERT(mPrepareDatastoreOp->mLoadDataOp == this);

  mPrepareDatastoreOp->FinishNesting();
}

void PrepareDatastoreOp::LoadDataOp::OnFailure(nsresult aResultCode) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mPrepareDatastoreOp);
  MOZ_ASSERT(mPrepareDatastoreOp->mState == State::Nesting);
  MOZ_ASSERT(mPrepareDatastoreOp->mNestedState ==
             NestedState::DatabaseWorkLoadData);
  MOZ_ASSERT(mPrepareDatastoreOp->mLoadDataOp == this);

  mPrepareDatastoreOp->SetFailureCode(aResultCode);

  mPrepareDatastoreOp->FinishNesting();
}

void PrepareDatastoreOp::LoadDataOp::Cleanup() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mPrepareDatastoreOp);
  MOZ_ASSERT(mPrepareDatastoreOp->mLoadDataOp == this);

  mPrepareDatastoreOp->mLoadDataOp = nullptr;
  mPrepareDatastoreOp = nullptr;

  ConnectionDatastoreOperationBase::Cleanup();
}

NS_IMPL_ISUPPORTS(PrepareDatastoreOp::CompressFunction, mozIStorageFunction)

NS_IMETHODIMP
PrepareDatastoreOp::CompressFunction::OnFunctionCall(
    mozIStorageValueArray* aFunctionArguments, nsIVariant** aResult) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aFunctionArguments);
  MOZ_ASSERT(aResult);

#ifdef DEBUG
  {
    uint32_t argCount;
    MOZ_ALWAYS_SUCCEEDS(aFunctionArguments->GetNumEntries(&argCount));
    MOZ_ASSERT(argCount == 1);

    int32_t type;
    MOZ_ALWAYS_SUCCEEDS(aFunctionArguments->GetTypeOfIndex(0, &type));
    MOZ_ASSERT(type == mozIStorageValueArray::VALUE_TYPE_TEXT);
  }
#endif

  nsCString value;
  nsresult rv = aFunctionArguments->GetUTF8String(0, value);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString compressed;
  if (!SnappyCompress(value, compressed)) {
    compressed = value;
  }

  nsCOMPtr<nsIVariant> result = new storage::UTF8TextVariant(compressed);

  result.forget(aResult);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(PrepareDatastoreOp::CompressibleFunction, mozIStorageFunction)

NS_IMETHODIMP
PrepareDatastoreOp::CompressibleFunction::OnFunctionCall(
    mozIStorageValueArray* aFunctionArguments, nsIVariant** aResult) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aFunctionArguments);
  MOZ_ASSERT(aResult);

#ifdef DEBUG
  {
    uint32_t argCount;
    MOZ_ALWAYS_SUCCEEDS(aFunctionArguments->GetNumEntries(&argCount));
    MOZ_ASSERT(argCount == 1);

    int32_t type;
    MOZ_ALWAYS_SUCCEEDS(aFunctionArguments->GetTypeOfIndex(0, &type));
    MOZ_ASSERT(type == mozIStorageValueArray::VALUE_TYPE_TEXT);
  }
#endif

  nsCString value;
  nsresult rv = aFunctionArguments->GetUTF8String(0, value);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString compressed;
  bool compressible = SnappyCompress(value, compressed);

  nsCOMPtr<nsIVariant> result = new storage::IntegerVariant(compressible);

  result.forget(aResult);
  return NS_OK;
}

/*******************************************************************************
 * PrepareObserverOp
 ******************************************************************************/

PrepareObserverOp::PrepareObserverOp(
    nsIEventTarget* aMainEventTarget, const LSRequestParams& aParams,
    const Maybe<ContentParentId>& aContentParentId)
    : LSRequestBase(aMainEventTarget, aParams, aContentParentId) {
  MOZ_ASSERT(aParams.type() ==
             LSRequestParams::TLSRequestPrepareObserverParams);
}

nsresult PrepareObserverOp::Start() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::StartingRequest);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());
  MOZ_ASSERT(MayProceed());

  const LSRequestPrepareObserverParams params =
      mParams.get_LSRequestPrepareObserverParams();

  const PrincipalInfo& storagePrincipalInfo = params.storagePrincipalInfo();

  if (storagePrincipalInfo.type() == PrincipalInfo::TSystemPrincipalInfo) {
    QuotaManager::GetInfoForChrome(nullptr, nullptr, &mOrigin);
  } else {
    MOZ_ASSERT(storagePrincipalInfo.type() ==
               PrincipalInfo::TContentPrincipalInfo);

    QuotaManager::GetInfoFromValidatedPrincipalInfo(storagePrincipalInfo,
                                                    nullptr, nullptr, &mOrigin);
  }

  mState = State::SendingReadyMessage;
  MOZ_ALWAYS_SUCCEEDS(OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

void PrepareObserverOp::GetResponse(LSRequestResponse& aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingResults);
  MOZ_ASSERT(NS_SUCCEEDED(ResultCode()));
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());
  MOZ_ASSERT(MayProceed());

  uint64_t observerId = ++gLastObserverId;

  RefPtr<Observer> observer = new Observer(mOrigin);

  if (!gPreparedObsevers) {
    gPreparedObsevers = new PreparedObserverHashtable();
  }
  gPreparedObsevers->Put(observerId, observer);

  LSRequestPrepareObserverResponse prepareObserverResponse;
  prepareObserverResponse.observerId() = observerId;

  aResponse = prepareObserverResponse;
}

/*******************************************************************************
+ * LSSimpleRequestBase
+
******************************************************************************/

LSSimpleRequestBase::LSSimpleRequestBase(
    const LSSimpleRequestParams& aParams,
    const Maybe<ContentParentId>& aContentParentId)
    : mParams(aParams),
      mContentParentId(aContentParentId),
      mState(State::Initial) {}

LSSimpleRequestBase::~LSSimpleRequestBase() {
  MOZ_ASSERT_IF(MayProceedOnNonOwningThread(),
                mState == State::Initial || mState == State::Completed);
}

void LSSimpleRequestBase::Dispatch() {
  AssertIsOnOwningThread();

  mState = State::StartingRequest;

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(this));
}

bool LSSimpleRequestBase::VerifyRequestParams() {
  AssertIsOnBackgroundThread();

  MOZ_ASSERT(mParams.type() != LSSimpleRequestParams::T__None);

  switch (mParams.type()) {
    case LSSimpleRequestParams::TLSSimpleRequestPreloadedParams: {
      const LSSimpleRequestPreloadedParams& params =
          mParams.get_LSSimpleRequestPreloadedParams();

      if (NS_WARN_IF(!VerifyPrincipalInfo(params.principalInfo(),
                                          params.storagePrincipalInfo()))) {
        return false;
      }

      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  return true;
}

nsresult LSSimpleRequestBase::StartRequest() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::StartingRequest);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !MayProceed()) {
    return NS_ERROR_FAILURE;
  }

#ifdef DEBUG
  // Always verify parameters in DEBUG builds!
  bool trustParams = false;
#else
  bool trustParams = !BackgroundParent::IsOtherProcessActor(Manager());
#endif

  if (!trustParams && NS_WARN_IF(!VerifyRequestParams())) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = Start();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void LSSimpleRequestBase::SendResults() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingResults);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !MayProceed()) {
    MaybeSetFailureCode(NS_ERROR_FAILURE);
  }

  if (MayProceed()) {
    LSSimpleRequestResponse response;

    if (NS_SUCCEEDED(ResultCode())) {
      GetResponse(response);
    } else {
      response = ResultCode();
    }

    Unused << PBackgroundLSSimpleRequestParent::Send__delete__(this, response);
  }

  mState = State::Completed;
}

NS_IMETHODIMP
LSSimpleRequestBase::Run() {
  nsresult rv;

  switch (mState) {
    case State::StartingRequest:
      rv = StartRequest();
      break;

    case State::SendingResults:
      SendResults();
      return NS_OK;

    default:
      MOZ_CRASH("Bad state!");
  }

  if (NS_WARN_IF(NS_FAILED(rv)) && mState != State::SendingResults) {
    MaybeSetFailureCode(rv);

    // Must set mState before dispatching otherwise we will race with the owning
    // thread.
    mState = State::SendingResults;

    if (IsOnOwningThread()) {
      SendResults();
    } else {
      MOZ_ALWAYS_SUCCEEDS(
          OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL));
    }
  }

  return NS_OK;
}

void LSSimpleRequestBase::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  NoteComplete();
}

/*******************************************************************************
 * PreloadedOp
 ******************************************************************************/

PreloadedOp::PreloadedOp(const LSSimpleRequestParams& aParams,
                         const Maybe<ContentParentId>& aContentParentId)
    : LSSimpleRequestBase(aParams, aContentParentId) {
  MOZ_ASSERT(aParams.type() ==
             LSSimpleRequestParams::TLSSimpleRequestPreloadedParams);
}

nsresult PreloadedOp::Start() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::StartingRequest);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());
  MOZ_ASSERT(MayProceed());

  const LSSimpleRequestPreloadedParams& params =
      mParams.get_LSSimpleRequestPreloadedParams();

  const PrincipalInfo& storagePrincipalInfo = params.storagePrincipalInfo();

  if (storagePrincipalInfo.type() == PrincipalInfo::TSystemPrincipalInfo) {
    QuotaManager::GetInfoForChrome(nullptr, nullptr, &mOrigin);
  } else {
    MOZ_ASSERT(storagePrincipalInfo.type() ==
               PrincipalInfo::TContentPrincipalInfo);

    QuotaManager::GetInfoFromValidatedPrincipalInfo(storagePrincipalInfo,
                                                    nullptr, nullptr, &mOrigin);
  }

  mState = State::SendingResults;
  MOZ_ALWAYS_SUCCEEDS(OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

void PreloadedOp::GetResponse(LSSimpleRequestResponse& aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingResults);
  MOZ_ASSERT(NS_SUCCEEDED(ResultCode()));
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());
  MOZ_ASSERT(MayProceed());

  bool preloaded;
  RefPtr<Datastore> datastore;
  if ((datastore = GetDatastore(mOrigin)) && !datastore->IsClosed()) {
    preloaded = true;
  } else {
    preloaded = false;
  }

  LSSimpleRequestPreloadedResponse preloadedResponse;
  preloadedResponse.preloaded() = preloaded;

  aResponse = preloadedResponse;
}

/*******************************************************************************
 * ArchivedOriginScope
 ******************************************************************************/

// static
ArchivedOriginScope* ArchivedOriginScope::CreateFromOrigin(
    const nsACString& aOriginAttrSuffix, const nsACString& aOriginKey) {
  return new ArchivedOriginScope(
      std::move(Origin(aOriginAttrSuffix, aOriginKey)));
}

// static
ArchivedOriginScope* ArchivedOriginScope::CreateFromPrefix(
    const nsACString& aOriginKey) {
  return new ArchivedOriginScope(std::move(Prefix(aOriginKey)));
}

// static
ArchivedOriginScope* ArchivedOriginScope::CreateFromPattern(
    const OriginAttributesPattern& aPattern) {
  return new ArchivedOriginScope(std::move(Pattern(aPattern)));
}

// static
ArchivedOriginScope* ArchivedOriginScope::CreateFromNull() {
  return new ArchivedOriginScope(std::move(Null()));
}

void ArchivedOriginScope::GetBindingClause(nsACString& aBindingClause) const {
  struct Matcher {
    nsACString* mBindingClause;

    explicit Matcher(nsACString* aBindingClause)
        : mBindingClause(aBindingClause) {}

    void operator()(const Origin& aOrigin) {
      *mBindingClause = NS_LITERAL_CSTRING(
          " WHERE originKey = :originKey "
          "AND originAttributes = :originAttributes");
    }

    void operator()(const Prefix& aPrefix) {
      *mBindingClause = NS_LITERAL_CSTRING(" WHERE originKey = :originKey");
    }

    void operator()(const Pattern& aPattern) {
      *mBindingClause = NS_LITERAL_CSTRING(
          " WHERE originAttributes MATCH :originAttributesPattern");
    }

    void operator()(const Null& aNull) { *mBindingClause = EmptyCString(); }
  };

  mData.match(Matcher(&aBindingClause));
}

nsresult ArchivedOriginScope::BindToStatement(
    mozIStorageStatement* aStmt) const {
  MOZ_ASSERT(IsOnIOThread() || IsOnConnectionThread());
  MOZ_ASSERT(aStmt);

  struct Matcher {
    mozIStorageStatement* mStmt;

    explicit Matcher(mozIStorageStatement* aStmt) : mStmt(aStmt) {}

    nsresult operator()(const Origin& aOrigin) {
      nsresult rv = mStmt->BindUTF8StringByName(NS_LITERAL_CSTRING("originKey"),
                                                aOrigin.OriginNoSuffix());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = mStmt->BindUTF8StringByName(NS_LITERAL_CSTRING("originAttributes"),
                                       aOrigin.OriginSuffix());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      return NS_OK;
    }

    nsresult operator()(const Prefix& aPrefix) {
      nsresult rv = mStmt->BindUTF8StringByName(NS_LITERAL_CSTRING("originKey"),
                                                aPrefix.OriginNoSuffix());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      return NS_OK;
    }

    nsresult operator()(const Pattern& aPattern) {
      nsresult rv = mStmt->BindUTF8StringByName(
          NS_LITERAL_CSTRING("originAttributesPattern"),
          NS_LITERAL_CSTRING("pattern1"));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      return NS_OK;
    }

    nsresult operator()(const Null& aNull) { return NS_OK; }
  };

  nsresult rv = mData.match(Matcher(aStmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

bool ArchivedOriginScope::HasMatches(
    ArchivedOriginHashtable* aHashtable) const {
  AssertIsOnIOThread();
  MOZ_ASSERT(aHashtable);

  struct Matcher {
    ArchivedOriginHashtable* mHashtable;

    explicit Matcher(ArchivedOriginHashtable* aHashtable)
        : mHashtable(aHashtable) {}

    bool operator()(const Origin& aOrigin) {
      nsCString hashKey = GetArchivedOriginHashKey(aOrigin.OriginSuffix(),
                                                   aOrigin.OriginNoSuffix());

      ArchivedOriginInfo* archivedOriginInfo;
      return mHashtable->Get(hashKey, &archivedOriginInfo);
    }

    bool operator()(const Prefix& aPrefix) {
      for (auto iter = mHashtable->ConstIter(); !iter.Done(); iter.Next()) {
        ArchivedOriginInfo* archivedOriginInfo = iter.Data();

        if (archivedOriginInfo->mOriginNoSuffix == aPrefix.OriginNoSuffix()) {
          return true;
        }
      }

      return false;
    }

    bool operator()(const Pattern& aPattern) {
      for (auto iter = mHashtable->ConstIter(); !iter.Done(); iter.Next()) {
        ArchivedOriginInfo* archivedOriginInfo = iter.Data();

        if (aPattern.GetPattern().Matches(
                archivedOriginInfo->mOriginAttributes)) {
          return true;
        }
      }

      return false;
    }

    bool operator()(const Null& aNull) { return mHashtable->Count(); }
  };

  return mData.match(Matcher(aHashtable));
}

void ArchivedOriginScope::RemoveMatches(
    ArchivedOriginHashtable* aHashtable) const {
  AssertIsOnIOThread();
  MOZ_ASSERT(aHashtable);

  struct Matcher {
    ArchivedOriginHashtable* mHashtable;

    explicit Matcher(ArchivedOriginHashtable* aHashtable)
        : mHashtable(aHashtable) {}

    void operator()(const Origin& aOrigin) {
      nsCString hashKey = GetArchivedOriginHashKey(aOrigin.OriginSuffix(),
                                                   aOrigin.OriginNoSuffix());

      mHashtable->Remove(hashKey);
    }

    void operator()(const Prefix& aPrefix) {
      for (auto iter = mHashtable->Iter(); !iter.Done(); iter.Next()) {
        ArchivedOriginInfo* archivedOriginInfo = iter.Data();

        if (archivedOriginInfo->mOriginNoSuffix == aPrefix.OriginNoSuffix()) {
          iter.Remove();
        }
      }
    }

    void operator()(const Pattern& aPattern) {
      for (auto iter = mHashtable->Iter(); !iter.Done(); iter.Next()) {
        ArchivedOriginInfo* archivedOriginInfo = iter.Data();

        if (aPattern.GetPattern().Matches(
                archivedOriginInfo->mOriginAttributes)) {
          iter.Remove();
        }
      }
    }

    void operator()(const Null& aNull) { mHashtable->Clear(); }
  };

  mData.match(Matcher(aHashtable));
}

/*******************************************************************************
 * QuotaClient
 ******************************************************************************/

QuotaClient* QuotaClient::sInstance = nullptr;

QuotaClient::QuotaClient()
    : mShadowDatabaseMutex("LocalStorage mShadowDatabaseMutex"),
      mShutdownRequested(false) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!sInstance, "We expect this to be a singleton!");

  sInstance = this;
}

QuotaClient::~QuotaClient() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(sInstance == this, "We expect this to be a singleton!");

  sInstance = nullptr;
}

// static
nsresult QuotaClient::Initialize() {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = Observer::Initialize();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

mozilla::dom::quota::Client::Type QuotaClient::GetType() {
  return QuotaClient::LS;
}

nsresult QuotaClient::InitOrigin(PersistenceType aPersistenceType,
                                 const nsACString& aGroup,
                                 const nsACString& aOrigin,
                                 const AtomicBool& aCanceled,
                                 UsageInfo* aUsageInfo, bool aForGetUsage) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aPersistenceType == PERSISTENCE_TYPE_DEFAULT);

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  nsCOMPtr<nsIFile> directory;
  nsresult rv = quotaManager->GetDirectoryForOrigin(aPersistenceType, aOrigin,
                                                    getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    REPORT_TELEMETRY_INIT_ERR(kQuotaExternalError, LS_GetDirForOrigin);
    return rv;
  }

  MOZ_ASSERT(directory);

  rv = directory->Append(NS_LITERAL_STRING(LS_DIRECTORY_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    REPORT_TELEMETRY_INIT_ERR(kQuotaExternalError, LS_Append);
    return rv;
  }

#ifdef DEBUG
  bool exists;
  rv = directory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    REPORT_TELEMETRY_INIT_ERR(kQuotaExternalError, LS_Exists);
    return rv;
  }

  MOZ_ASSERT(exists);
#endif

  nsString directoryPath;
  rv = directory->GetPath(directoryPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    REPORT_TELEMETRY_INIT_ERR(kQuotaExternalError, LS_GetPath);
    return rv;
  }

  nsCOMPtr<nsIFile> usageFile;
  rv = GetUsageFile(directoryPath, getter_AddRefs(usageFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    REPORT_TELEMETRY_INIT_ERR(kQuotaExternalError, LS_GetUsageFile);
    return rv;
  }

  bool usageFileExists;

  bool isDirectory;
  rv = usageFile->IsDirectory(&isDirectory);
  if (rv != NS_ERROR_FILE_NOT_FOUND &&
      rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST) {
    if (NS_WARN_IF(NS_FAILED(rv))) {
      REPORT_TELEMETRY_INIT_ERR(kQuotaExternalError, LS_IsDirectory);
      return rv;
    }

    if (NS_WARN_IF(isDirectory)) {
      REPORT_TELEMETRY_INIT_ERR(kQuotaInternalError, LS_UnexpectedDir);
      return NS_ERROR_FAILURE;
    }

    usageFileExists = true;
  } else {
    usageFileExists = false;
  }

  nsCOMPtr<nsIFile> usageJournalFile;
  rv = GetUsageJournalFile(directoryPath, getter_AddRefs(usageJournalFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    REPORT_TELEMETRY_INIT_ERR(kQuotaExternalError, LS_GetUsageForJFile);
    return rv;
  }

  rv = usageJournalFile->IsDirectory(&isDirectory);
  if (rv != NS_ERROR_FILE_NOT_FOUND &&
      rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST) {
    if (NS_WARN_IF(NS_FAILED(rv))) {
      REPORT_TELEMETRY_INIT_ERR(kQuotaExternalError, LS_IsDirectory2);
      return rv;
    }

    if (NS_WARN_IF(isDirectory)) {
      REPORT_TELEMETRY_INIT_ERR(kQuotaInternalError, LS_UnexpectedDir2);
      return NS_ERROR_FAILURE;
    }

    if (usageFileExists) {
      rv = usageFile->Remove(false);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        REPORT_TELEMETRY_INIT_ERR(kQuotaExternalError, LS_Remove);
        return rv;
      }

      usageFileExists = false;
    }

    rv = usageJournalFile->Remove(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      REPORT_TELEMETRY_INIT_ERR(kQuotaExternalError, LS_Remove2);
      return rv;
    }
  }

  nsCOMPtr<nsIFile> file;
  rv = directory->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    REPORT_TELEMETRY_INIT_ERR(kQuotaExternalError, LS_Clone);
    return rv;
  }

  rv = file->Append(NS_LITERAL_STRING(DATA_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    REPORT_TELEMETRY_INIT_ERR(kQuotaExternalError, LS_Append2);
    return rv;
  }

  rv = file->IsDirectory(&isDirectory);
  if (rv != NS_ERROR_FILE_NOT_FOUND &&
      rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST) {
    if (NS_WARN_IF(NS_FAILED(rv))) {
      REPORT_TELEMETRY_INIT_ERR(kQuotaExternalError, LS_IsDirectory3);
      return rv;
    }

    if (NS_WARN_IF(isDirectory)) {
      REPORT_TELEMETRY_INIT_ERR(kQuotaInternalError, LS_UnexpectedDir3);
      return NS_ERROR_FAILURE;
    }

    int64_t usage;
    rv = LoadUsageFile(usageFile, &usage);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      nsCOMPtr<mozIStorageConnection> connection;
      bool dummy;
      rv = CreateStorageConnection(file, usageFile, aOrigin,
                                   getter_AddRefs(connection), &dummy);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        REPORT_TELEMETRY_INIT_ERR(kQuotaExternalError, LS_CreateConnection);
        return rv;
      }

      rv = GetUsage(connection, /* aArchivedOriginScope */ nullptr, &usage);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        REPORT_TELEMETRY_INIT_ERR(kQuotaExternalError, LS_GetUsage);
        return rv;
      }

      rv = UpdateUsageFile(usageFile, usageJournalFile, usage);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        REPORT_TELEMETRY_INIT_ERR(kQuotaExternalError, LS_UpdateUsageFile);
        return rv;
      }

      rv = usageJournalFile->Remove(false);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        REPORT_TELEMETRY_INIT_ERR(kQuotaExternalError, LS_Remove3);
        return rv;
      }
    }

    MOZ_ASSERT(usage >= 0);

    aUsageInfo->AppendToDatabaseUsage(Some(uint64_t(usage)));
  } else if (usageFileExists) {
    rv = usageFile->Remove(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      REPORT_TELEMETRY_INIT_ERR(kQuotaExternalError, LS_Remove4);
      return rv;
    }
  }

  // Report unknown files in debug builds, but don't fail, just warn.

#ifdef DEBUG
  nsCOMPtr<nsIDirectoryEnumerator> directoryEntries;
  rv = directory->GetDirectoryEntries(getter_AddRefs(directoryEntries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    REPORT_TELEMETRY_INIT_ERR(kQuotaExternalError, LS_GetDirEntries);
    return rv;
  }

  if (!directoryEntries) {
    return NS_OK;
  }

  while (true) {
    if (aCanceled) {
      break;
    }

    nsCOMPtr<nsIFile> file;
    rv = directoryEntries->GetNextFile(getter_AddRefs(file));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      REPORT_TELEMETRY_INIT_ERR(kQuotaExternalError, LS_GetNextFile);
      return rv;
    }

    if (!file) {
      break;
    }

    nsString leafName;
    rv = file->GetLeafName(leafName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      REPORT_TELEMETRY_INIT_ERR(kQuotaExternalError, LS_GetLeafName);
      return rv;
    }

    // Don't need to check for USAGE_JOURNAL_FILE_NAME. We removed it above
    // (if there was any).
    if (leafName.EqualsLiteral(DATA_FILE_NAME) ||
        leafName.EqualsLiteral(USAGE_FILE_NAME)) {
      // Don't need to check if it is a directory or file. We did that above.
      continue;
    }

    if (leafName.EqualsLiteral(JOURNAL_FILE_NAME)) {
      bool isDirectory;
      rv = file->IsDirectory(&isDirectory);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        REPORT_TELEMETRY_INIT_ERR(kQuotaExternalError, LS_IsDirectory4);
        return rv;
      }

      if (!isDirectory) {
        continue;
      }
    }

    LS_WARNING("Something (%s) in the directory that doesn't belong!",
               NS_ConvertUTF16toUTF8(leafName).get());
  }
#endif

  return NS_OK;
}

nsresult QuotaClient::GetUsageForOrigin(PersistenceType aPersistenceType,
                                        const nsACString& aGroup,
                                        const nsACString& aOrigin,
                                        const AtomicBool& aCanceled,
                                        UsageInfo* aUsageInfo) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aPersistenceType == PERSISTENCE_TYPE_DEFAULT);
  MOZ_ASSERT(aUsageInfo);

  // We can't open the database at this point, since it can be already used
  // by the connection thread. Use the cached value instead.

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  uint64_t usage;
  if (quotaManager->GetUsageForClient(PERSISTENCE_TYPE_DEFAULT, aGroup, aOrigin,
                                      Client::LS, usage)) {
    aUsageInfo->AppendToDatabaseUsage(Some(usage));
  }

  return NS_OK;
}

nsresult QuotaClient::AboutToClearOrigins(
    const Nullable<PersistenceType>& aPersistenceType,
    const OriginScope& aOriginScope) {
  AssertIsOnIOThread();

  // This method is not called when the clearing is triggered by the eviction
  // process. It's on purpose to avoid a problem with the origin access time
  // which can be described as follows:
  // When there's a storage pressure condition and quota manager starts
  // collecting origins for eviction, there can be an origin that hasn't been
  // touched for long time. However, the old implementation of local storage
  // could have touched the origin only recently and the new implementation
  // hasn't had a chance to create a new per origin database for it yet (the
  // data is still in the archive database), so the origin access time hasn't
  // been updated either. In the end, the origin would be evicted despite the
  // fact that there was recent local storage activity.
  // So this method clears the archived data and shadow database entries for
  // given origin scope, but only if it's a privacy-related origin clearing.

  if (!aPersistenceType.IsNull() &&
      aPersistenceType.Value() != PERSISTENCE_TYPE_DEFAULT) {
    return NS_OK;
  }

  bool shadowWrites = gShadowWrites;

  nsAutoPtr<ArchivedOriginScope> archivedOriginScope;
  nsresult rv = CreateArchivedOriginScope(aOriginScope, archivedOriginScope);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!gArchivedOrigins) {
    rv = LoadArchivedOrigins();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    MOZ_ASSERT(gArchivedOrigins);
  }

  bool hasDataForRemoval = archivedOriginScope->HasMatches(gArchivedOrigins);

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  nsString basePath = quotaManager->GetBasePath();

  {
    MutexAutoLock shadowDatabaseLock(mShadowDatabaseMutex);

    nsCOMPtr<mozIStorageConnection> connection;
    if (gInitializedShadowStorage) {
      rv = GetShadowStorageConnection(basePath, getter_AddRefs(connection));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      rv = CreateShadowStorageConnection(basePath, getter_AddRefs(connection));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      gInitializedShadowStorage = true;
    }

    if (hasDataForRemoval) {
      rv = AttachArchiveDatabase(quotaManager->GetStoragePath(), connection);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    if (archivedOriginScope->IsPattern()) {
      nsCOMPtr<mozIStorageFunction> function(
          new MatchFunction(archivedOriginScope->GetPattern()));

      rv = connection->CreateFunction(NS_LITERAL_CSTRING("match"), 2, function);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    nsCOMPtr<mozIStorageStatement> stmt;
    rv = connection->CreateStatement(NS_LITERAL_CSTRING("BEGIN IMMEDIATE;"),
                                     getter_AddRefs(stmt));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (shadowWrites) {
      rv = PerformDelete(connection, NS_LITERAL_CSTRING("main"),
                         archivedOriginScope);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    if (hasDataForRemoval) {
      rv = PerformDelete(connection, NS_LITERAL_CSTRING("archive"),
                         archivedOriginScope);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    rv = connection->CreateStatement(NS_LITERAL_CSTRING("COMMIT;"),
                                     getter_AddRefs(stmt));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    stmt = nullptr;

    if (archivedOriginScope->IsPattern()) {
      rv = connection->RemoveFunction(NS_LITERAL_CSTRING("match"));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    if (hasDataForRemoval) {
      rv = DetachArchiveDatabase(connection);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      MOZ_ASSERT(gArchivedOrigins);
      MOZ_ASSERT(archivedOriginScope->HasMatches(gArchivedOrigins));
      archivedOriginScope->RemoveMatches(gArchivedOrigins);
    }

    rv = connection->Close();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (aOriginScope.IsNull()) {
    nsCOMPtr<nsIFile> shadowFile;
    rv = GetShadowFile(basePath, getter_AddRefs(shadowFile));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = shadowFile->Remove(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    gInitializedShadowStorage = false;
  }

  return NS_OK;
}

void QuotaClient::OnOriginClearCompleted(PersistenceType aPersistenceType,
                                         const nsACString& aOrigin) {
  AssertIsOnIOThread();
}

void QuotaClient::ReleaseIOThreadObjects() {
  AssertIsOnIOThread();

  // Delete archived origins hashtable since QuotaManager clears the whole
  // storage directory including ls-archive.sqlite.

  gArchivedOrigins = nullptr;
}

void QuotaClient::AbortOperations(const nsACString& aOrigin) {
  AssertIsOnBackgroundThread();

  // A PrepareDatastoreOp object could already acquire a directory lock for
  // the given origin. Its last step is creation of a Datastore object (which
  // will take ownership of the directory lock) and a PreparedDatastore object
  // which keeps the Datastore alive until a database actor is created.
  // We need to invalidate the PreparedDatastore object when it's created,
  // otherwise the Datastore object can block the origin clear operation for
  // long time. It's not a problem that we don't fail the PrepareDatastoreOp
  // immediatelly (avoiding the creation of the Datastore and PreparedDatastore
  // object). We will call RequestAllowToClose on the database actor once it's
  // created and the child actor will respond by sending AllowToClose which
  // will close the Datastore on the parent side (the closing releases the
  // directory lock).

  if (gPrepareDatastoreOps) {
    for (PrepareDatastoreOp* prepareDatastoreOp : *gPrepareDatastoreOps) {
      MOZ_ASSERT(prepareDatastoreOp);

      // Explicitely check if a directory lock has been requested.
      // Origin clearing can't be blocked by this PrepareDatastoreOp if it
      // hasn't requested a directory lock yet, so we can just ignore it.
      // This will also guarantee that PrepareDatastoreOp has a known origin.
      // And it also ensures that the ordering is right. Without the check we
      // could invalidate ops whose directory locks were requested after we
      // requested a directory lock for origin clearing.
      if (!prepareDatastoreOp->RequestedDirectoryLock()) {
        continue;
      }

      MOZ_ASSERT(prepareDatastoreOp->OriginIsKnown());

      if (aOrigin.IsVoid() || prepareDatastoreOp->Origin() == aOrigin) {
        prepareDatastoreOp->Invalidate();
      }
    }
  }

  if (gPreparedDatastores) {
    for (auto iter = gPreparedDatastores->ConstIter(); !iter.Done();
         iter.Next()) {
      PreparedDatastore* preparedDatastore = iter.Data();
      MOZ_ASSERT(preparedDatastore);

      if (aOrigin.IsVoid() || preparedDatastore->Origin() == aOrigin) {
        preparedDatastore->Invalidate();
      }
    }
  }

  if (aOrigin.IsVoid()) {
    RequestAllowToCloseIf([](const Database* const) { return true; });
  } else {
    RequestAllowToCloseIf([&aOrigin](const Database* const aDatabase) {
      return aDatabase->Origin() == aOrigin;
    });
  }
}

void QuotaClient::AbortOperationsForProcess(ContentParentId aContentParentId) {
  AssertIsOnBackgroundThread();

  RequestAllowToCloseIf([aContentParentId](const Database* const aDatabase) {
    return aDatabase->IsOwnedByProcess(aContentParentId);
  });
}

void QuotaClient::StartIdleMaintenance() { AssertIsOnBackgroundThread(); }

void QuotaClient::StopIdleMaintenance() { AssertIsOnBackgroundThread(); }

void QuotaClient::ShutdownWorkThreads() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mShutdownRequested);

  mShutdownRequested = true;

  // gPrepareDatastoreOps are short lived objects running a state machine.
  // The shutdown flag is checked between states, so we don't have to notify
  // all the objects here.
  // Allocation of a new PrepareDatastoreOp object is prevented once the
  // shutdown flag is set.
  // When the last PrepareDatastoreOp finishes, the gPrepareDatastoreOps array
  // is destroyed.

  if (gPreparedDatastores) {
    gPreparedDatastores->Clear();
    gPreparedDatastores = nullptr;
  }

  RequestAllowToCloseIf([](const Database* const) { return true; });

  if (gPreparedObsevers) {
    gPreparedObsevers->Clear();
    gPreparedObsevers = nullptr;
  }

  nsCOMPtr<nsITimer> timer = NS_NewTimer();

  MOZ_ALWAYS_SUCCEEDS(timer->InitWithNamedFuncCallback(
      [](nsITimer* aTimer, void* aClosure) {
        auto quotaClient = static_cast<QuotaClient*>(aClosure);

        quotaClient->ShutdownTimedOut();
      },
      this, SHUTDOWN_TIMEOUT_MS, nsITimer::TYPE_ONE_SHOT,
      "localstorage::QuotaClient::ShutdownWorkThreads::SpinEventLoopTimer"));

  // This should release any local storage related quota objects or directory
  // locks.
  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() {
    // Don't have to check gPreparedDatastores since we nulled it out above.
    return !gPrepareDatastoreOps && !gDatastores && !gLiveDatabases;
  }));

  MOZ_ALWAYS_SUCCEEDS(timer->Cancel());

  // And finally, shutdown the connection thread.
  if (gConnectionThread) {
    gConnectionThread->Shutdown();

    gConnectionThread = nullptr;
  }
}

void QuotaClient::ShutdownTimedOut() {
  AssertIsOnBackgroundThread();
  MOZ_DIAGNOSTIC_ASSERT(gPrepareDatastoreOps || gDatastores || gLiveDatabases);

  nsCString data;

  if (gPrepareDatastoreOps) {
    data.Append("gPrepareDatastoreOps: ");
    data.AppendInt(static_cast<uint32_t>(gPrepareDatastoreOps->Length()));

    nsTHashtable<nsCStringHashKey> ids;
    for (uint32_t index = 0; index < gPrepareDatastoreOps->Length(); index++) {
      CheckedUnsafePtr<PrepareDatastoreOp>& prepareDatastoreOp =
          (*gPrepareDatastoreOps)[index];

      nsCString origin;
      if (prepareDatastoreOp->OriginIsKnown()) {
        origin = prepareDatastoreOp->Origin();
        SanitizeOrigin(origin);
      }

      nsCString state;
      prepareDatastoreOp->StringifyState(state);

      nsCString nestedState;
      prepareDatastoreOp->StringifyNestedState(nestedState);

      NS_NAMED_LITERAL_CSTRING(delimiter, "*");

      nsCString id = origin + delimiter + state + delimiter + nestedState;

      ids.PutEntry(id);
    }

    data.Append(" (");

    bool first = true;
    for (auto iter = ids.ConstIter(); !iter.Done(); iter.Next()) {
      if (first) {
        first = false;
      } else {
        data.Append(", ");
      }

      const nsACString& id = iter.Get()->GetKey();

      data.Append(id);
    }

    data.Append(")\n");
  }

  if (gDatastores) {
    data.Append("gDatastores: ");
    data.AppendInt(gDatastores->Count());
    data.Append("\n");
  }

  if (gLiveDatabases) {
    data.Append("gLiveDatabases: ");
    data.AppendInt(static_cast<uint32_t>(gLiveDatabases->Length()));
    data.Append("\n");
  }

  CrashReporter::AnnotateCrashReport(
      CrashReporter::Annotation::LocalStorageShutdownTimeout, data);

  MOZ_CRASH("LocalStorage shutdown timed out");
}

nsresult QuotaClient::CreateArchivedOriginScope(
    const OriginScope& aOriginScope,
    nsAutoPtr<ArchivedOriginScope>& aArchivedOriginScope) {
  AssertIsOnIOThread();

  nsresult rv;

  nsAutoPtr<ArchivedOriginScope> archivedOriginScope;

  if (aOriginScope.IsOrigin()) {
    nsCString spec;
    OriginAttributes attrs;
    if (NS_WARN_IF(!QuotaManager::ParseOrigin(aOriginScope.GetOrigin(), spec,
                                              &attrs))) {
      return NS_ERROR_FAILURE;
    }

    ContentPrincipalInfo contentPrincipalInfo;
    contentPrincipalInfo.attrs() = attrs;
    contentPrincipalInfo.spec() = spec;

    PrincipalInfo storagePrincipalInfo(contentPrincipalInfo);

    nsCString originAttrSuffix;
    nsCString originKey;
    rv = GenerateOriginKey2(storagePrincipalInfo, originAttrSuffix, originKey);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    archivedOriginScope =
        ArchivedOriginScope::CreateFromOrigin(originAttrSuffix, originKey);
  } else if (aOriginScope.IsPrefix()) {
    nsCString spec;
    OriginAttributes attrs;
    if (NS_WARN_IF(!QuotaManager::ParseOrigin(aOriginScope.GetOriginNoSuffix(),
                                              spec, &attrs))) {
      return NS_ERROR_FAILURE;
    }

    ContentPrincipalInfo contentPrincipalInfo;
    contentPrincipalInfo.attrs() = attrs;
    contentPrincipalInfo.spec() = spec;

    PrincipalInfo storagePrincipalInfo(contentPrincipalInfo);

    nsCString originAttrSuffix;
    nsCString originKey;
    rv = GenerateOriginKey2(storagePrincipalInfo, originAttrSuffix, originKey);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    archivedOriginScope = ArchivedOriginScope::CreateFromPrefix(originKey);
  } else if (aOriginScope.IsPattern()) {
    archivedOriginScope =
        ArchivedOriginScope::CreateFromPattern(aOriginScope.GetPattern());
  } else {
    MOZ_ASSERT(aOriginScope.IsNull());

    archivedOriginScope = ArchivedOriginScope::CreateFromNull();
  }

  MOZ_ASSERT(archivedOriginScope);

  aArchivedOriginScope = std::move(archivedOriginScope);
  return NS_OK;
}

nsresult QuotaClient::PerformDelete(
    mozIStorageConnection* aConnection, const nsACString& aSchemaName,
    ArchivedOriginScope* aArchivedOriginScope) const {
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(aArchivedOriginScope);

  nsresult rv;

  nsCString bindingClause;
  aArchivedOriginScope->GetBindingClause(bindingClause);

  nsCOMPtr<mozIStorageStatement> stmt;
  rv = aConnection->CreateStatement(NS_LITERAL_CSTRING("DELETE FROM ") +
                                        aSchemaName +
                                        NS_LITERAL_CSTRING(".webappsstore2") +
                                        bindingClause + NS_LITERAL_CSTRING(";"),
                                    getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aArchivedOriginScope->BindToStatement(stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

// static
nsresult QuotaClient::Observer::Initialize() {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<Observer> observer = new Observer();

  nsresult rv = observer->Init();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult QuotaClient::Observer::Init() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = obs->AddObserver(this, kPrivateBrowsingObserverTopic, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    return rv;
  }

  return NS_OK;
}

nsresult QuotaClient::Observer::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ALWAYS_SUCCEEDS(obs->RemoveObserver(this, kPrivateBrowsingObserverTopic));
  MOZ_ALWAYS_SUCCEEDS(obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID));

  // In general, the instance will have died after the latter removal call, so
  // it's not safe to do anything after that point.
  // However, Shutdown is currently called from Observe which is called by the
  // Observer Service which holds a strong reference to the observer while the
  // Observe method is being called.

  return NS_OK;
}

NS_IMPL_ISUPPORTS(QuotaClient::Observer, nsIObserver)

NS_IMETHODIMP
QuotaClient::Observer::Observe(nsISupports* aSubject, const char* aTopic,
                               const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;

  if (!strcmp(aTopic, kPrivateBrowsingObserverTopic)) {
    PBackgroundChild* backgroundActor =
        BackgroundChild::GetOrCreateForCurrentThread();
    if (NS_WARN_IF(!backgroundActor)) {
      return NS_ERROR_FAILURE;
    }

    if (NS_WARN_IF(!backgroundActor->SendLSClearPrivateBrowsing())) {
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    rv = Shutdown();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

  NS_WARNING("Unknown observer topic!");
  return NS_OK;
}

NS_IMPL_ISUPPORTS(QuotaClient::MatchFunction, mozIStorageFunction)

NS_IMETHODIMP
QuotaClient::MatchFunction::OnFunctionCall(
    mozIStorageValueArray* aFunctionArguments, nsIVariant** aResult) {
  AssertIsOnIOThread();
  MOZ_ASSERT(aFunctionArguments);
  MOZ_ASSERT(aResult);

  nsCString suffix;
  nsresult rv = aFunctionArguments->GetUTF8String(1, suffix);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  OriginAttributes oa;
  if (NS_WARN_IF(!oa.PopulateFromSuffix(suffix))) {
    return NS_ERROR_FAILURE;
  }

  bool result = mPattern.Matches(oa);

  RefPtr<nsVariant> outVar(new nsVariant());
  rv = outVar->SetAsBool(result);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  outVar.forget(aResult);
  return NS_OK;
}

/*******************************************************************************
 * AutoWriteTransaction
 ******************************************************************************/

AutoWriteTransaction::AutoWriteTransaction(bool aShadowWrites)
    : mConnection(nullptr), mShadowWrites(aShadowWrites) {
  AssertIsOnConnectionThread();

  MOZ_COUNT_CTOR(mozilla::dom::AutoWriteTransaction);
}

AutoWriteTransaction::~AutoWriteTransaction() {
  AssertIsOnConnectionThread();

  MOZ_COUNT_DTOR(mozilla::dom::AutoWriteTransaction);

  if (mConnection) {
    if (NS_FAILED(mConnection->RollbackWriteTransaction())) {
      NS_WARNING("Failed to rollback write transaction!");
    }

    if (mShadowWrites && NS_FAILED(DetachShadowDatabaseAndUnlock())) {
      NS_WARNING("Failed to detach shadow database!");
    }
  }
}

nsresult AutoWriteTransaction::Start(Connection* aConnection) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(!mConnection);

  nsresult rv;

  if (mShadowWrites) {
    rv = LockAndAttachShadowDatabase(aConnection);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = aConnection->BeginWriteTransaction();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mConnection = aConnection;

  return NS_OK;
}

nsresult AutoWriteTransaction::Commit() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mConnection);

  nsresult rv = mConnection->CommitWriteTransaction();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mShadowWrites) {
    rv = DetachShadowDatabaseAndUnlock();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  mConnection = nullptr;

  return NS_OK;
}

nsresult AutoWriteTransaction::LockAndAttachShadowDatabase(
    Connection* aConnection) {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(!mConnection);
  MOZ_ASSERT(mShadowDatabaseLock.isNothing());
  MOZ_ASSERT(mShadowWrites);

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  nsCOMPtr<mozIStorageConnection> storageConnection =
      aConnection->StorageConnection();
  MOZ_ASSERT(storageConnection);

  mShadowDatabaseLock.emplace(
      aConnection->GetQuotaClient()->ShadowDatabaseMutex());

  nsresult rv =
      AttachShadowDatabase(quotaManager->GetBasePath(), storageConnection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult AutoWriteTransaction::DetachShadowDatabaseAndUnlock() {
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mConnection);
  MOZ_ASSERT(mShadowDatabaseLock.isSome());
  MOZ_ASSERT(mShadowWrites);

  nsCOMPtr<mozIStorageConnection> storageConnection =
      mConnection->StorageConnection();
  MOZ_ASSERT(storageConnection);

  nsresult rv = DetachShadowDatabase(storageConnection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mShadowDatabaseLock.reset();

  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
