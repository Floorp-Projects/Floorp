/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActorsParent.h"

#include "LocalStorageCommon.h"
#include "LSObject.h"
#include "mozIStorageConnection.h"
#include "mozIStorageService.h"
#include "mozStorageCID.h"
#include "mozStorageHelper.h"
#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/PBackgroundLSDatabaseParent.h"
#include "mozilla/dom/PBackgroundLSObjectParent.h"
#include "mozilla/dom/PBackgroundLSObserverParent.h"
#include "mozilla/dom/PBackgroundLSRequestParent.h"
#include "mozilla/dom/PBackgroundLSSharedTypes.h"
#include "mozilla/dom/PBackgroundLSSimpleRequestParent.h"
#include "mozilla/dom/StorageDBUpdater.h"
#include "mozilla/dom/StorageUtils.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/QuotaObject.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "nsInterfaceHashtable.h"
#include "nsISimpleEnumerator.h"
#include "nsRefPtrHashtable.h"
#include "ReportInternalError.h"

#define DISABLE_ASSERTS_FOR_FUZZING 0

#if DISABLE_ASSERTS_FOR_FUZZING
#define ASSERT_UNLESS_FUZZING(...) do { } while (0)
#else
#define ASSERT_UNLESS_FUZZING(...) MOZ_ASSERT(false, __VA_ARGS__)
#endif

#if defined(MOZ_WIDGET_ANDROID)
#define LS_MOBILE
#endif

namespace mozilla {
namespace dom {

using namespace mozilla::dom::quota;
using namespace mozilla::dom::StorageUtils;
using namespace mozilla::ipc;

namespace {

class ArchivedOriginInfo;
class Connection;
class ConnectionThread;
class Database;
class PrepareDatastoreOp;
class PreparedDatastore;

/*******************************************************************************
 * Constants
 ******************************************************************************/

// Major schema version. Bump for almost everything.
const uint32_t kMajorSchemaVersion = 1;

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
               kSQLitePageSizeOverride >= 512  &&
               kSQLitePageSizeOverride <= 65536),
              "Must be 0 (disabled) or a power of 2 between 512 and 65536!");

// Set to some multiple of the page size to grow the database in larger chunks.
const uint32_t kSQLiteGrowthIncrement = kSQLitePageSizeOverride * 2;

static_assert(kSQLiteGrowthIncrement >= 0 &&
              kSQLiteGrowthIncrement % kSQLitePageSizeOverride == 0 &&
              kSQLiteGrowthIncrement < uint32_t(INT32_MAX),
              "Must be 0 (disabled) or a positive multiple of the page size!");

#define DATA_FILE_NAME "data.sqlite"
#define JOURNAL_FILE_NAME "data.sqlite-journal"

const uint32_t kAutoCommitTimeoutMs = 5000;

const char kPrivateBrowsingObserverTopic[] = "last-pb-context-exited";

const uint32_t kDefaultOriginLimitKB = 5 * 1024;
const char kDefaultQuotaPref[] = "dom.storage.default_quota";

const uint32_t kPreparedDatastoreTimeoutMs = 20000;

#define LS_ARCHIVE_FILE_NAME "ls-archive.sqlite"

bool
IsOnConnectionThread();

void
AssertIsOnConnectionThread();

/*******************************************************************************
 * SQLite functions
 ******************************************************************************/

#if 0
int32_t
MakeSchemaVersion(uint32_t aMajorSchemaVersion,
                  uint32_t aMinorSchemaVersion)
{
  return int32_t((aMajorSchemaVersion << 4) + aMinorSchemaVersion);
}
#endif

nsresult
CreateTables(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  // Table `database`
  nsresult rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE database"
      "( origin TEXT NOT NULL"
      ", last_vacuum_time INTEGER NOT NULL DEFAULT 0"
      ", last_analyze_time INTEGER NOT NULL DEFAULT 0"
      ", last_vacuum_size INTEGER NOT NULL DEFAULT 0"
      ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Table `data`
  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE data"
      "( key TEXT PRIMARY KEY"
      ", value TEXT NOT NULL"
      ", compressed INTEGER NOT NULL DEFAULT 0"
      ", lastAccessTime INTEGER NOT NULL DEFAULT 0"
      ");"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aConnection->SetSchemaVersion(kSQLiteSchemaVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

#if 0
nsresult
UpgradeSchemaFrom1_0To2_0(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  nsresult rv = aConnection->SetSchemaVersion(MakeSchemaVersion(2, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}
#endif

nsresult
SetDefaultPragmas(mozIStorageConnection* aConnection)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConnection);

  nsresult rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "PRAGMA synchronous = FULL;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#ifndef LS_MOBILE
  if (kSQLiteGrowthIncrement) {
    // This is just an optimization so ignore the failure if the disk is
    // currently too full.
    rv = aConnection->SetGrowthIncrement(kSQLiteGrowthIncrement,
                                         EmptyCString());
    if (rv != NS_ERROR_FILE_TOO_BIG && NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
#endif // LS_MOBILE

  return NS_OK;
}

nsresult
CreateStorageConnection(nsIFile* aDBFile,
                        const nsACString& aOrigin,
                        mozIStorageConnection** aConnection)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aDBFile);
  MOZ_ASSERT(aConnection);

  nsresult rv;

  nsCOMPtr<mozIStorageService> ss =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = ss->OpenDatabase(aDBFile, getter_AddRefs(connection));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
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
        rv = connection->ExecuteSimpleSQL(
          nsPrintfCString("PRAGMA page_size = %" PRIu32 ";", kSQLitePageSizeOverride)
        );
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

    mozStorageTransaction transaction(connection, false,
                                  mozIStorageConnection::TRANSACTION_IMMEDIATE);

    if (newDatabase) {
      rv = CreateTables(connection);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      MOZ_ASSERT(NS_SUCCEEDED(connection->GetSchemaVersion(&schemaVersion)));
      MOZ_ASSERT(schemaVersion == kSQLiteSchemaVersion);

      nsCOMPtr<mozIStorageStatement> stmt;
      nsresult rv = connection->CreateStatement(NS_LITERAL_CSTRING(
        "INSERT INTO database (origin) "
        "VALUES (:origin)"
      ), getter_AddRefs(stmt));
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
    } else  {
      // This logic needs to change next time we change the schema!
      static_assert(kSQLiteSchemaVersion == int32_t((1 << 4) + 0),
                    "Upgrade function needed due to schema version increase.");

      while (schemaVersion != kSQLiteSchemaVersion) {
#if 0
        if (schemaVersion == MakeSchemaVersion(1, 0)) {
          rv = UpgradeSchemaFrom1_0To2_0(connection);
        } else {
#endif
          LS_WARNING("Unable to open LocalStorage database, no upgrade path is "
                     "available!");
          return NS_ERROR_FAILURE;
#if 0
        }
#endif

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
      rv = connection->CreateStatement(NS_LITERAL_CSTRING(
        "UPDATE database "
          "SET last_vacuum_time = :time"
            ", last_vacuum_size = :size;"
      ), getter_AddRefs(vacuumTimeStmt));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = vacuumTimeStmt->BindInt64ByName(NS_LITERAL_CSTRING("time"),
                                           vacuumTime);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = vacuumTimeStmt->BindInt64ByName(NS_LITERAL_CSTRING("size"),
                                           fileSize);
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

nsresult
GetStorageConnection(const nsAString& aDatabaseFilePath,
                     mozIStorageConnection** aConnection)
{
  AssertIsOnConnectionThread();
  MOZ_ASSERT(!aDatabaseFilePath.IsEmpty());
  MOZ_ASSERT(StringEndsWith(aDatabaseFilePath, NS_LITERAL_STRING(".sqlite")));
  MOZ_ASSERT(aConnection);

  nsCOMPtr<nsIFile> databaseFile;
  nsresult rv = NS_NewLocalFile(aDatabaseFilePath, false,
                                getter_AddRefs(databaseFile));
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

nsresult
GetArchiveFile(const nsAString& aStoragePath,
               nsIFile** aArchiveFile)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(!aStoragePath.IsEmpty());
  MOZ_ASSERT(aArchiveFile);

  nsCOMPtr<nsIFile> archiveFile;
  nsresult rv = NS_NewLocalFile(aStoragePath,
                                false,
                                getter_AddRefs(archiveFile));
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

nsresult
CreateArchiveStorageConnection(const nsAString& aStoragePath,
                               mozIStorageConnection** aConnection)
{
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

nsresult
AttachArchiveDatabase(const nsAString& aStoragePath,
                      mozIStorageConnection* aConnection)
{
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

nsresult
DetachArchiveDatabase(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  nsresult rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DETACH DATABASE archive"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

/*******************************************************************************
 * Non-actor class declarations
 ******************************************************************************/

class DatastoreOperationBase
  : public Runnable
{
  nsCOMPtr<nsIEventTarget> mOwningEventTarget;
  nsresult mResultCode;
  Atomic<bool> mMayProceedOnNonOwningThread;
  bool mMayProceed;

public:
  nsIEventTarget*
  OwningEventTarget() const
  {
    MOZ_ASSERT(mOwningEventTarget);

    return mOwningEventTarget;
  }

  bool
  IsOnOwningThread() const
  {
    MOZ_ASSERT(mOwningEventTarget);

    bool current;
    return
      NS_SUCCEEDED(mOwningEventTarget->IsOnCurrentThread(&current)) && current;
  }

  void
  AssertIsOnOwningThread() const
  {
    MOZ_ASSERT(IsOnBackgroundThread());
    MOZ_ASSERT(IsOnOwningThread());
  }

  nsresult
  ResultCode() const
  {
    return mResultCode;
  }

  void
  SetFailureCode(nsresult aErrorCode)
  {
    MOZ_ASSERT(NS_SUCCEEDED(mResultCode));
    MOZ_ASSERT(NS_FAILED(aErrorCode));

    mResultCode = aErrorCode;
  }

  void
  MaybeSetFailureCode(nsresult aErrorCode)
  {
    MOZ_ASSERT(NS_FAILED(aErrorCode));

    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = aErrorCode;
    }
  }

  void
  NoteComplete()
  {
    AssertIsOnOwningThread();

    mMayProceed = false;
    mMayProceedOnNonOwningThread = false;
  }

  bool
  MayProceed() const
  {
    AssertIsOnOwningThread();

    return mMayProceed;
  }

  // May be called on any thread, but you should call MayProceed() if you know
  // you're on the background thread because it is slightly faster.
  bool
  MayProceedOnNonOwningThread() const
  {
    return mMayProceedOnNonOwningThread;
  }

protected:
  DatastoreOperationBase()
    : Runnable("dom::DatastoreOperationBase")
    , mOwningEventTarget(GetCurrentThreadEventTarget())
    , mResultCode(NS_OK)
    , mMayProceedOnNonOwningThread(true)
    , mMayProceed(true)
  { }

  ~DatastoreOperationBase() override
  {
    MOZ_ASSERT(!mMayProceed);
  }
};

class ConnectionDatastoreOperationBase
  : public DatastoreOperationBase
{
protected:
  RefPtr<Connection> mConnection;

public:
  // This callback will be called on the background thread before releasing the
  // final reference to this request object. Subclasses may perform any
  // additional cleanup here but must always call the base class implementation.
  virtual void
  Cleanup();

protected:
  ConnectionDatastoreOperationBase(Connection* aConnection);

  ~ConnectionDatastoreOperationBase();

  // Must be overridden in subclasses. Called on the target thread to allow the
  // subclass to perform necessary datastore operations. A successful return
  // value will trigger an OnSuccess callback on the background thread while
  // while a failure value will trigger an OnFailure callback.
  virtual nsresult
  DoDatastoreWork() = 0;

  // Methods that subclasses may implement.
  virtual void
  OnSuccess();

  virtual void
  OnFailure(nsresult aResultCode);

private:
  void
  RunOnConnectionThread();

  void
  RunOnOwningThread();

  // Not to be overridden by subclasses.
  NS_DECL_NSIRUNNABLE
};

class Connection final
{
  friend class ConnectionThread;

public:
  class CachedStatement;

private:
  class BeginOp;
  class CommitOp;
  class CloseOp;

  RefPtr<ConnectionThread> mConnectionThread;
  nsCOMPtr<mozIStorageConnection> mStorageConnection;
  nsInterfaceHashtable<nsCStringHashKey, mozIStorageStatement>
    mCachedStatements;
  const nsCString mOrigin;
  const nsString mFilePath;
  bool mInTransaction;

public:
  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::Connection)

  void
  AssertIsOnOwningThread() const
  {
    NS_ASSERT_OWNINGTHREAD(Connection);
  }

  // Methods which can only be called on the owning thread.

  bool
  InTransaction()
  {
    AssertIsOnOwningThread();
    return mInTransaction;
  }

  // This method is used to asynchronously execute a connection datastore
  // operation on the connection thread.
  void
  Dispatch(ConnectionDatastoreOperationBase* aOp);

  // This method is used to asynchronously start a transaction on the connection
  // thread.
  void
  Begin(bool aReadonly);

  // This method is used to asynchronously end a transaction on the connection
  // thread.
  void
  Commit();

  // This method is used to asynchronously close the storage connection on the
  // connection thread.
  void
  Close(nsIRunnable* aCallback);

  // Methods which can only be called on the connection thread.

  nsresult
  EnsureStorageConnection();

  mozIStorageConnection*
  StorageConnection() const
  {
    AssertIsOnConnectionThread();
    MOZ_ASSERT(mStorageConnection);

    return mStorageConnection;
  }

  void
  CloseStorageConnection();

  nsresult
  GetCachedStatement(const nsACString& aQuery,
                     CachedStatement* aCachedStatement);

private:
  // Only created by ConnectionThread.
  Connection(ConnectionThread* aConnectionThread,
             const nsACString& aOrigin,
             const nsAString& aFilePath);

  ~Connection();
};

class Connection::CachedStatement final
{
  friend class Connection;

  nsCOMPtr<mozIStorageStatement> mStatement;
  Maybe<mozStorageStatementScoper> mScoper;

public:
  CachedStatement();
  ~CachedStatement();

  mozIStorageStatement*
  operator->() const MOZ_NO_ADDREF_RELEASE_ON_RETURN;

private:
  // Only called by Connection.
  void
  Assign(Connection* aConnection,
         already_AddRefed<mozIStorageStatement> aStatement);

  // No funny business allowed.
  CachedStatement(const CachedStatement&) = delete;
  CachedStatement& operator=(const CachedStatement&) = delete;
};

class Connection::BeginOp final
  : public ConnectionDatastoreOperationBase
{
  const bool mReadonly;

public:
  BeginOp(Connection* aConnection,
          bool aReadonly)
    : ConnectionDatastoreOperationBase(aConnection)
    , mReadonly(aReadonly)
  { }

private:
  nsresult
  DoDatastoreWork() override;
};

class Connection::CommitOp final
  : public ConnectionDatastoreOperationBase
{
public:
  explicit CommitOp(Connection* aConnection)
    : ConnectionDatastoreOperationBase(aConnection)
  { }

private:
  nsresult
  DoDatastoreWork() override;
};

class Connection::CloseOp final
  : public ConnectionDatastoreOperationBase
{
  nsCOMPtr<nsIRunnable> mCallback;

public:
  CloseOp(Connection* aConnection,
          nsIRunnable* aCallback)
    : ConnectionDatastoreOperationBase(aConnection)
    , mCallback(aCallback)
  { }

private:
  nsresult
  DoDatastoreWork() override;

  void
  Cleanup() override;
};

class ConnectionThread final
{
  friend class Connection;

  nsCOMPtr<nsIThread> mThread;
  nsRefPtrHashtable<nsCStringHashKey, Connection> mConnections;

public:
  ConnectionThread();

  void
  AssertIsOnOwningThread() const
  {
    NS_ASSERT_OWNINGTHREAD(ConnectionThread);
  }

  bool
  IsOnConnectionThread();

  void
  AssertIsOnConnectionThread();

  already_AddRefed<Connection>
  CreateConnection(const nsACString& aOrigin,
                   const nsAString& aFilePath);

  void
  Shutdown();

  NS_INLINE_DECL_REFCOUNTING(ConnectionThread)

private:
  ~ConnectionThread();
};

class SetItemOp final
  : public ConnectionDatastoreOperationBase
{
  nsString mKey;
  nsString mValue;

public:
  SetItemOp(Connection* aConnection,
            const nsAString& aKey,
            const nsAString& aValue)
    : ConnectionDatastoreOperationBase(aConnection)
    , mKey(aKey)
    , mValue(aValue)
  { }

private:
  nsresult
  DoDatastoreWork() override;
};

class RemoveItemOp final
  : public ConnectionDatastoreOperationBase
{
  nsString mKey;

public:
  RemoveItemOp(Connection* aConnection,
               const nsAString& aKey)
    : ConnectionDatastoreOperationBase(aConnection)
    , mKey(aKey)
  { }

private:
  nsresult
  DoDatastoreWork() override;
};

class ClearOp final
  : public ConnectionDatastoreOperationBase
{
public:
  explicit ClearOp(Connection* aConnection)
    : ConnectionDatastoreOperationBase(aConnection)
  { }

private:
  nsresult
  DoDatastoreWork() override;
};

class Datastore final
{
  RefPtr<DirectoryLock> mDirectoryLock;
  RefPtr<Connection> mConnection;
  RefPtr<QuotaObject> mQuotaObject;
  nsCOMPtr<nsITimer> mAutoCommitTimer;
  nsCOMPtr<nsIRunnable> mCompleteCallback;
  nsTHashtable<nsPtrHashKey<PrepareDatastoreOp>> mPrepareDatastoreOps;
  nsTHashtable<nsPtrHashKey<PreparedDatastore>> mPreparedDatastores;
  nsTHashtable<nsPtrHashKey<Database>> mDatabases;
  nsDataHashtable<nsStringHashKey, nsString> mValues;
  const nsCString mOrigin;
  const uint32_t mPrivateBrowsingId;
  int64_t mUsage;
  bool mClosed;

public:
  // Created by PrepareDatastoreOp.
  Datastore(const nsACString& aOrigin,
            uint32_t aPrivateBrowsingId,
            int64_t aUsage,
            already_AddRefed<DirectoryLock>&& aDirectoryLock,
            already_AddRefed<Connection>&& aConnection,
            already_AddRefed<QuotaObject>&& aQuotaObject,
            nsDataHashtable<nsStringHashKey, nsString>& aValues);

  const nsCString&
  Origin() const
  {
    return mOrigin;
  }

  uint32_t
  PrivateBrowsingId() const
  {
    return mPrivateBrowsingId;
  }

  bool
  IsPersistent() const
  {
    return mPrivateBrowsingId == 0;
  }

  void
  Close();

  bool
  IsClosed() const
  {
    AssertIsOnBackgroundThread();

    return mClosed;
  }

  void
  WaitForConnectionToComplete(nsIRunnable* aCallback);

  void
  NoteLivePrepareDatastoreOp(PrepareDatastoreOp* aPrepareDatastoreOp);

  void
  NoteFinishedPrepareDatastoreOp(PrepareDatastoreOp* aPrepareDatastoreOp);

  void
  NoteLivePreparedDatastore(PreparedDatastore* aPreparedDatastore);

  void
  NoteFinishedPreparedDatastore(PreparedDatastore* aPreparedDatastore);

#ifdef DEBUG
  bool
  HasLivePreparedDatastores() const;
#endif

  void
  NoteLiveDatabase(Database* aDatabase);

  void
  NoteFinishedDatabase(Database* aDatabase);

#ifdef DEBUG
  bool
  HasLiveDatabases() const;
#endif

  uint32_t
  GetLength() const;

  void
  GetKey(uint32_t aIndex, nsString& aKey) const;

  void
  GetItem(const nsString& aKey, nsString& aValue) const;

  void
  SetItem(Database* aDatabase,
          const nsString& aKey,
          const nsString& aValue,
          LSWriteOpResponse& aResponse);

  void
  RemoveItem(Database* aDatabase,
             const nsString& aKey,
             LSWriteOpResponse& aResponse);

  void
  Clear(Database* aDatabase,
        LSWriteOpResponse& aResponse);

  void
  GetKeys(nsTArray<nsString>& aKeys) const;

  NS_INLINE_DECL_REFCOUNTING(Datastore)

private:
  // Reference counted.
  ~Datastore();

  void
  MaybeClose();

  void
  ConnectionClosedCallback();

  void
  CleanupMetadata();

  bool
  UpdateUsage(int64_t aDelta);

  void
  NotifyObservers(Database* aDatabase,
                  const nsString& aKey,
                  const nsString& aOldValue,
                  const nsString& aNewValue);

  void
  EnsureTransaction();

  static void
  AutoCommitTimerCallback(nsITimer* aTimer, void* aClosure);
};

class PreparedDatastore
{
  RefPtr<Datastore> mDatastore;
  nsCOMPtr<nsITimer> mTimer;
  // Strings share buffers if possible, so it's not a problem to duplicate the
  // origin here.
  const nsCString mOrigin;
  uint64_t mDatastoreId;
  bool mForPreload;
  bool mInvalidated;

public:
  PreparedDatastore(Datastore* aDatastore,
                    const nsACString& aOrigin,
                    uint64_t aDatastoreId,
                    bool aForPreload)
    : mDatastore(aDatastore)
    , mTimer(NS_NewTimer())
    , mOrigin(aOrigin)
    , mDatastoreId(aDatastoreId)
    , mForPreload(aForPreload)
    , mInvalidated(false)
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aDatastore);
    MOZ_ASSERT(mTimer);

    aDatastore->NoteLivePreparedDatastore(this);

    MOZ_ALWAYS_SUCCEEDS(
      mTimer->InitWithNamedFuncCallback(TimerCallback,
                                        this,
                                        kPreparedDatastoreTimeoutMs,
                                        nsITimer::TYPE_ONE_SHOT,
                                        "PreparedDatastore::TimerCallback"));
  }

  ~PreparedDatastore()
  {
    MOZ_ASSERT(mDatastore);
    MOZ_ASSERT(mTimer);

    mTimer->Cancel();

    mDatastore->NoteFinishedPreparedDatastore(this);
  }

  Datastore*
  GetDatastore() const
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mDatastore);

    return mDatastore;
  }

  const nsCString&
  Origin() const
  {
    return mOrigin;
  }

  void
  Invalidate()
  {
    AssertIsOnBackgroundThread();

    mInvalidated = true;

    if (mForPreload) {
      mTimer->Cancel();

      MOZ_ALWAYS_SUCCEEDS(
        mTimer->InitWithNamedFuncCallback(TimerCallback,
                                          this,
                                          0,
                                          nsITimer::TYPE_ONE_SHOT,
                                          "PreparedDatastore::TimerCallback"));
    }
  }

  bool
  IsInvalidated() const
  {
    AssertIsOnBackgroundThread();

    return mInvalidated;
  }

private:
  void
  Destroy();

  static void
  TimerCallback(nsITimer* aTimer, void* aClosure);
};

/*******************************************************************************
 * Actor class declarations
 ******************************************************************************/

class Object final
  : public PBackgroundLSObjectParent
{
  const PrincipalInfo mPrincipalInfo;
  const nsString mDocumentURI;
  uint32_t mPrivateBrowsingId;
  bool mActorDestroyed;

public:
  // Created in AllocPBackgroundLSObjectParent.
  Object(const PrincipalInfo& aPrincipalInfo,
         const nsAString& aDocumentURI,
         uint32_t aPrivateBrowsingId);

  const PrincipalInfo&
  GetPrincipalInfo() const
  {
    return mPrincipalInfo;
  }

  uint32_t
  PrivateBrowsingId() const
  {
    return mPrivateBrowsingId;
  }

  const nsString&
  DocumentURI() const
  {
    return mDocumentURI;
  }

  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::Object)

private:
  // Reference counted.
  ~Object();

  // IPDL methods are only called by IPDL.
  void
  ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult
  RecvDeleteMe() override;

  PBackgroundLSDatabaseParent*
  AllocPBackgroundLSDatabaseParent(const uint64_t& aDatastoreId) override;

  mozilla::ipc::IPCResult
  RecvPBackgroundLSDatabaseConstructor(PBackgroundLSDatabaseParent* aActor,
                                       const uint64_t& aDatastoreId) override;

  bool
  DeallocPBackgroundLSDatabaseParent(PBackgroundLSDatabaseParent* aActor)
                                     override;
};

class Database final
  : public PBackgroundLSDatabaseParent
{
  RefPtr<Object> mObject;
  RefPtr<Datastore> mDatastore;
  // Strings share buffers if possible, so it's not a problem to duplicate the
  // origin here.
  nsCString mOrigin;
  bool mAllowedToClose;
  bool mActorDestroyed;
  bool mRequestedAllowToClose;
#ifdef DEBUG
  bool mActorWasAlive;
#endif

public:
  // Created in AllocPBackgroundLSDatabaseParent.
  Database(Object* aObject,
           const nsACString& aOrigin);

  Object*
  GetObject() const
  {
    AssertIsOnBackgroundThread();

    return mObject;
  }

  const nsCString&
  Origin() const
  {
    return mOrigin;
  }

  void
  SetActorAlive(Datastore* aDatastore);

  void
  RequestAllowToClose();

  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::Database)

private:
  // Reference counted.
  ~Database();

  void
  AllowToClose();

  // IPDL methods are only called by IPDL.
  void
  ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult
  RecvDeleteMe() override;

  mozilla::ipc::IPCResult
  RecvAllowToClose() override;

  mozilla::ipc::IPCResult
  RecvGetLength(uint32_t* aLength) override;

  mozilla::ipc::IPCResult
  RecvGetKey(const uint32_t& aIndex, nsString* aKey) override;

  mozilla::ipc::IPCResult
  RecvGetItem(const nsString& aKey, nsString* aValue) override;

  mozilla::ipc::IPCResult
  RecvGetKeys(nsTArray<nsString>* aKeys) override;

  mozilla::ipc::IPCResult
  RecvSetItem(const nsString& aKey,
              const nsString& aValue,
              LSWriteOpResponse* aResponse) override;

  mozilla::ipc::IPCResult
  RecvRemoveItem(const nsString& aKey,
                 LSWriteOpResponse* aResponse) override;

  mozilla::ipc::IPCResult
  RecvClear(LSWriteOpResponse* aResponse) override;
};

class Observer final
  : public PBackgroundLSObserverParent
{
  nsCString mOrigin;
  bool mActorDestroyed;

public:
  // Created in AllocPBackgroundLSObserverParent.
  explicit Observer(const nsACString& aOrigin);

  const nsCString&
  Origin() const
  {
    return mOrigin;
  }

  void
  Observe(Database* aDatabase,
          const nsString& aKey,
          const nsString& aOldValue,
          const nsString& aNewValue);

  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::Observer)

private:
  // Reference counted.
  ~Observer();

  // IPDL methods are only called by IPDL.
  void
  ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult
  RecvDeleteMe() override;
};

class LSRequestBase
  : public DatastoreOperationBase
  , public PBackgroundLSRequestParent
{
protected:
  enum class State
  {
    // Just created on the PBackground thread. Next step is Opening.
    Initial,

    // Waiting to open/opening on the main thread. Next step is either
    // Nesting if a subclass needs to process more nested states or
    // SendingReadyMessage if a subclass doesn't need any nested processing.
    Opening,

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
  State mState;

public:
  explicit LSRequestBase(nsIEventTarget* aMainEventTarget);

  void
  Dispatch();

protected:
  ~LSRequestBase() override;

  virtual nsresult
  Open() = 0;

  virtual nsresult
  NestedRun();

  virtual void
  GetResponse(LSRequestResponse& aResponse) = 0;

  virtual void
  Cleanup()
  { }

private:
  void
  SendReadyMessage();

  void
  SendResults();

protected:
  // Common nsIRunnable implementation that subclasses may not override.
  NS_IMETHOD
  Run() final;

  // IPDL methods.
  void
  ActorDestroy(ActorDestroyReason aWhy) override;

private:
  mozilla::ipc::IPCResult
  RecvCancel() override;

  mozilla::ipc::IPCResult
  RecvFinish() override;
};

class PrepareDatastoreOp
  : public LSRequestBase
  , public OpenDirectoryListener
{
  class LoadDataOp;

  enum class NestedState
  {
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
  RefPtr<DirectoryLock> mDirectoryLock;
  RefPtr<Connection> mConnection;
  RefPtr<Datastore> mDatastore;
  nsAutoPtr<ArchivedOriginInfo> mArchivedOriginInfo;
  LoadDataOp* mLoadDataOp;
  nsDataHashtable<nsStringHashKey, nsString> mValues;
  const LSRequestPrepareDatastoreParams mParams;
  nsCString mSuffix;
  nsCString mGroup;
  nsCString mMainThreadOrigin;
  nsCString mOrigin;
  nsString mDatabaseFilePath;
  uint32_t mPrivateBrowsingId;
  int64_t mUsage;
  NestedState mNestedState;
  bool mDatabaseNotAvailable;
  bool mRequestedDirectoryLock;
  bool mInvalidated;

#ifdef DEBUG
  int64_t mDEBUGUsage;
#endif

public:
  PrepareDatastoreOp(nsIEventTarget* aMainEventTarget,
                     const LSRequestParams& aParams);

  bool
  OriginIsKnown() const
  {
    AssertIsOnOwningThread();

    return !mOrigin.IsEmpty();
  }

  const nsCString&
  Origin() const
  {
    AssertIsOnOwningThread();
    MOZ_ASSERT(OriginIsKnown());

    return mOrigin;
  }

  bool
  RequestedDirectoryLock() const
  {
    AssertIsOnOwningThread();

    return mRequestedDirectoryLock;
  }

  void
  Invalidate()
  {
    AssertIsOnOwningThread();

    mInvalidated = true;
  }

private:
  ~PrepareDatastoreOp() override;

  nsresult
  Open() override;

  nsresult
  CheckExistingOperations();

  nsresult
  CheckClosingDatastore();

  nsresult
  BeginDatastorePreparation();

  nsresult
  QuotaManagerOpen();

  nsresult
  OpenDirectory();

  nsresult
  SendToIOThread();

  nsresult
  DatabaseWork();

  nsresult
  DatabaseNotAvailable();

  nsresult
  EnsureDirectoryEntry(nsIFile* aEntry,
                       bool aCreateIfNotExists,
                       bool aDirectory,
                       bool* aAlreadyExisted = nullptr);

  nsresult
  VerifyDatabaseInformation(mozIStorageConnection* aConnection);

  already_AddRefed<QuotaObject>
  GetQuotaObject();

  nsresult
  BeginLoadData();

  nsresult
  NestedRun() override;

  void
  GetResponse(LSRequestResponse& aResponse) override;

  void
  Cleanup() override;

  void
  ConnectionClosedCallback();

  void
  CleanupMetadata();

  NS_DECL_ISUPPORTS_INHERITED

  // IPDL overrides.
  void
  ActorDestroy(ActorDestroyReason aWhy) override;

  // OpenDirectoryListener overrides.
  void
  DirectoryLockAcquired(DirectoryLock* aLock) override;

  void
  DirectoryLockFailed() override;
};

class PrepareDatastoreOp::LoadDataOp final
  : public ConnectionDatastoreOperationBase
{
  RefPtr<PrepareDatastoreOp> mPrepareDatastoreOp;

public:
  explicit LoadDataOp(PrepareDatastoreOp* aPrepareDatastoreOp)
    : ConnectionDatastoreOperationBase(aPrepareDatastoreOp->mConnection)
    , mPrepareDatastoreOp(aPrepareDatastoreOp)
  { }

private:
  ~LoadDataOp() = default;

  nsresult
  DoDatastoreWork() override;

  void
  OnSuccess() override;

  void
  OnFailure(nsresult aResultCode) override;

  void
  Cleanup() override;
};

class PrepareObserverOp
  : public LSRequestBase
{
  const LSRequestPrepareObserverParams mParams;
  nsCString mOrigin;

public:
  PrepareObserverOp(nsIEventTarget* aMainEventTarget,
                    const LSRequestParams& aParams);

private:
  nsresult
  Open() override;

  void
  GetResponse(LSRequestResponse& aResponse) override;
};

class LSSimpleRequestBase
  : public DatastoreOperationBase
  , public PBackgroundLSSimpleRequestParent
{
protected:
  enum class State
  {
    // Just created on the PBackground thread. Next step is Opening.
    Initial,

    // Waiting to open/opening on the main thread. Next step is SendingResults.
    Opening,

    // Waiting to send/sending results on the PBackground thread. Next step is
    // Completed.
    SendingResults,

    // All done.
    Completed
  };

  State mState;

public:
  LSSimpleRequestBase();

  void
  Dispatch();

protected:
  ~LSSimpleRequestBase() override;

  virtual nsresult
  Open() = 0;

  virtual void
  GetResponse(LSSimpleRequestResponse& aResponse) = 0;

private:
  void
  SendResults();

  // Common nsIRunnable implementation that subclasses may not override.
  NS_IMETHOD
  Run() final;

  // IPDL methods.
  void
  ActorDestroy(ActorDestroyReason aWhy) override;
};

class PreloadedOp
  : public LSSimpleRequestBase
{
  const LSSimpleRequestPreloadedParams mParams;
  nsCString mOrigin;

public:
  explicit PreloadedOp(const LSSimpleRequestParams& aParams);

private:
  nsresult
  Open() override;

  void
  GetResponse(LSSimpleRequestResponse& aResponse) override;
};

/*******************************************************************************
 * Other class declarations
 ******************************************************************************/

class ArchivedOriginInfo
{
  nsCString mOriginSuffix;
  nsCString mOriginNoSuffix;

public:
  static ArchivedOriginInfo*
  Create(nsIPrincipal* aPrincipal);

  const nsCString&
  OriginSuffix() const
  {
    return mOriginSuffix;
  }

  const nsCString&
  OriginNoSuffix() const
  {
    return mOriginNoSuffix;
  }

  const nsCString
  Origin() const
  {
    return mOriginSuffix + NS_LITERAL_CSTRING(":") + mOriginNoSuffix;
  }

  nsresult
  BindToStatement(mozIStorageStatement* aStatement) const;

private:
  ArchivedOriginInfo(const nsACString& aOriginSuffix,
                     const nsACString& aOriginNoSuffix)
    : mOriginSuffix(aOriginSuffix)
    , mOriginNoSuffix(aOriginNoSuffix)
  { }
};

class QuotaClient final
  : public mozilla::dom::quota::Client
{
  class ClearPrivateBrowsingRunnable;
  class Observer;

  static QuotaClient* sInstance;
  static bool sObserversRegistered;

  bool mShutdownRequested;

public:
  QuotaClient();

  static bool
  IsShuttingDownOnBackgroundThread()
  {
    AssertIsOnBackgroundThread();

    if (sInstance) {
      return sInstance->IsShuttingDown();
    }

    return QuotaManager::IsShuttingDown();
  }

  static bool
  IsShuttingDownOnNonBackgroundThread()
  {
    MOZ_ASSERT(!IsOnBackgroundThread());

    return QuotaManager::IsShuttingDown();
  }

  static nsresult
  RegisterObservers(nsIEventTarget* aBackgroundEventTarget);

  bool
  IsShuttingDown() const
  {
    AssertIsOnBackgroundThread();

    return mShutdownRequested;
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::dom::QuotaClient, override)

  Type
  GetType() override;

  nsresult
  InitOrigin(PersistenceType aPersistenceType,
             const nsACString& aGroup,
             const nsACString& aOrigin,
             const AtomicBool& aCanceled,
             UsageInfo* aUsageInfo) override;

  nsresult
  GetUsageForOrigin(PersistenceType aPersistenceType,
                    const nsACString& aGroup,
                    const nsACString& aOrigin,
                    const AtomicBool& aCanceled,
                    UsageInfo* aUsageInfo) override;

  void
  OnOriginClearCompleted(PersistenceType aPersistenceType,
                         const nsACString& aOrigin)
                         override;

  void
  ReleaseIOThreadObjects() override;

  void
  AbortOperations(const nsACString& aOrigin) override;

  void
  AbortOperationsForProcess(ContentParentId aContentParentId) override;

  void
  StartIdleMaintenance() override;

  void
  StopIdleMaintenance() override;

  void
  ShutdownWorkThreads() override;

private:
  ~QuotaClient() override;
};

class QuotaClient::ClearPrivateBrowsingRunnable final
  : public Runnable
{
public:
  ClearPrivateBrowsingRunnable()
    : Runnable("mozilla::dom::ClearPrivateBrowsingRunnable")
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

private:
  ~ClearPrivateBrowsingRunnable() = default;

  NS_DECL_NSIRUNNABLE
};

class QuotaClient::Observer final
  : public nsIObserver
{
  nsCOMPtr<nsIEventTarget> mBackgroundEventTarget;

public:
  explicit Observer(nsIEventTarget* aBackgroundEventTarget)
    : mBackgroundEventTarget(aBackgroundEventTarget)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_DECL_ISUPPORTS

private:
  ~Observer()
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_DECL_NSIOBSERVER
};

/*******************************************************************************
 * Globals
 ******************************************************************************/

typedef nsTArray<PrepareDatastoreOp*> PrepareDatastoreOpArray;

StaticAutoPtr<PrepareDatastoreOpArray> gPrepareDatastoreOps;

typedef nsDataHashtable<nsCStringHashKey, Datastore*> DatastoreHashtable;

StaticAutoPtr<DatastoreHashtable> gDatastores;

uint64_t gLastDatastoreId = 0;

typedef nsClassHashtable<nsUint64HashKey, PreparedDatastore>
  PreparedDatastoreHashtable;

StaticAutoPtr<PreparedDatastoreHashtable> gPreparedDatastores;

typedef nsTArray<Database*> LiveDatabaseArray;

StaticAutoPtr<LiveDatabaseArray> gLiveDatabases;

StaticRefPtr<ConnectionThread> gConnectionThread;

uint64_t gLastObserverId = 0;

typedef nsRefPtrHashtable<nsUint64HashKey, Observer> PreparedObserverHashtable;

StaticAutoPtr<PreparedObserverHashtable> gPreparedObsevers;

typedef nsClassHashtable<nsCStringHashKey, nsTArray<Observer*>>
  ObserverHashtable;

StaticAutoPtr<ObserverHashtable> gObservers;

Atomic<uint32_t, Relaxed> gOriginLimitKB(kDefaultOriginLimitKB);

typedef nsDataHashtable<nsCStringHashKey, int64_t> UsageHashtable;

// Can only be touched on the Quota Manager I/O thread.
StaticAutoPtr<UsageHashtable> gUsages;

typedef nsTHashtable<nsCStringHashKey> ArchivedOriginHashtable;

StaticAutoPtr<ArchivedOriginHashtable> gArchivedOrigins;

bool
IsOnConnectionThread()
{
  MOZ_ASSERT(gConnectionThread);
  return gConnectionThread->IsOnConnectionThread();
}

void
AssertIsOnConnectionThread()
{
  MOZ_ASSERT(gConnectionThread);
  gConnectionThread->AssertIsOnConnectionThread();
}

void
InitUsageForOrigin(const nsACString& aOrigin, int64_t aUsage)
{
  AssertIsOnIOThread();

  if (!gUsages) {
    gUsages = new UsageHashtable();
  }

  MOZ_ASSERT(!gUsages->Contains(aOrigin));
  gUsages->Put(aOrigin, aUsage);
}

nsresult
LoadArchivedOrigins()
{
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
  rv = connection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT DISTINCT originAttributes || ':' || originKey "
      "FROM webappsstore2;"
  ), getter_AddRefs(stmt));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoPtr<ArchivedOriginHashtable> archivedOrigins(
    new ArchivedOriginHashtable());

  bool hasResult;
  while (NS_SUCCEEDED(rv = stmt->ExecuteStep(&hasResult)) && hasResult) {
    nsCString origin;
    rv = stmt->GetUTF8String(0, origin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    archivedOrigins->PutEntry(origin);
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  gArchivedOrigins = archivedOrigins.forget();
  return NS_OK;
}

nsresult
GetUsage(mozIStorageConnection* aConnection,
         ArchivedOriginInfo* aArchivedOriginInfo,
         int64_t* aUsage)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);
  MOZ_ASSERT(aUsage);

  nsresult rv;

  nsCOMPtr<mozIStorageStatement> stmt;
  if (aArchivedOriginInfo) {
    rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT sum(length(key) + length(value)) "
      "FROM webappsstore2 "
      "WHERE originKey = :originKey "
      "AND originAttributes = :originAttributes;"
    ), getter_AddRefs(stmt));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = aArchivedOriginInfo->BindToStatement(stmt);
  } else {
    rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT sum(length(key) + length(value)) "
      "FROM data"
    ), getter_AddRefs(stmt));
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

} // namespace

/*******************************************************************************
 * Exported functions
 ******************************************************************************/

PBackgroundLSObjectParent*
AllocPBackgroundLSObjectParent(const PrincipalInfo& aPrincipalInfo,
                               const nsString& aDocumentURI,
                               const uint32_t& aPrivateBrowsingId)
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread())) {
    return nullptr;
  }

  RefPtr<Object> object =
    new Object(aPrincipalInfo, aDocumentURI, aPrivateBrowsingId);

  // Transfer ownership to IPDL.
  return object.forget().take();
}

bool
RecvPBackgroundLSObjectConstructor(PBackgroundLSObjectParent* aActor,
                                   const PrincipalInfo& aPrincipalInfo,
                                   const nsString& aDocumentURI,
                                   const uint32_t& aPrivateBrowsingId)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());

  return true;
}

bool
DeallocPBackgroundLSObjectParent(PBackgroundLSObjectParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<Object> actor = dont_AddRef(static_cast<Object*>(aActor));

  return true;
}

PBackgroundLSObserverParent*
AllocPBackgroundLSObserverParent(const uint64_t& aObserverId)
{
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

  //observer->SetObject(this);

  // Transfer ownership to IPDL.
  return observer.forget().take();
}

bool
RecvPBackgroundLSObserverConstructor(PBackgroundLSObserverParent* aActor,
                                     const uint64_t& aObserverId)
{
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

  return true;
}

bool
DeallocPBackgroundLSObserverParent(PBackgroundLSObserverParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<Observer> actor = dont_AddRef(static_cast<Observer*>(aActor));

  return true;
}

PBackgroundLSRequestParent*
AllocPBackgroundLSRequestParent(PBackgroundParent* aBackgroundActor,
                                const LSRequestParams& aParams)
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread())) {
    return nullptr;
  }

  // If we're in the same process as the actor, we need to get the target event
  // queue from the current RequestHelper.
  nsCOMPtr<nsIEventTarget> mainEventTarget;
  if (!BackgroundParent::IsOtherProcessActor(aBackgroundActor)) {
    mainEventTarget = LSObject::GetSyncLoopEventTarget();
  }

  RefPtr<LSRequestBase> actor;

  switch (aParams.type()) {
    case LSRequestParams::TLSRequestPrepareDatastoreParams: {
      RefPtr<PrepareDatastoreOp> prepareDatastoreOp =
        new PrepareDatastoreOp(mainEventTarget, aParams);

      if (!gPrepareDatastoreOps) {
        gPrepareDatastoreOps = new PrepareDatastoreOpArray();
      }
      gPrepareDatastoreOps->AppendElement(prepareDatastoreOp);

      actor = std::move(prepareDatastoreOp);

      break;
    }

    case LSRequestParams::TLSRequestPrepareObserverParams: {
      RefPtr<PrepareObserverOp> prepareObserverOp =
        new PrepareObserverOp(mainEventTarget, aParams);

      actor = std::move(prepareObserverOp);

      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

bool
RecvPBackgroundLSRequestConstructor(PBackgroundLSRequestParent* aActor,
                                    const LSRequestParams& aParams)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != LSRequestParams::T__None);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());

  // The actor is now completely built.

  auto* op = static_cast<LSRequestBase*>(aActor);

  op->Dispatch();

  return true;
}

bool
DeallocPBackgroundLSRequestParent(PBackgroundLSRequestParent* aActor)
{
  AssertIsOnBackgroundThread();

  // Transfer ownership back from IPDL.
  RefPtr<LSRequestBase> actor =
    dont_AddRef(static_cast<LSRequestBase*>(aActor));

  return true;
}

PBackgroundLSSimpleRequestParent*
AllocPBackgroundLSSimpleRequestParent(const LSSimpleRequestParams& aParams)
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread())) {
    return nullptr;
  }

  RefPtr<LSSimpleRequestBase> actor;

  switch (aParams.type()) {
    case LSSimpleRequestParams::TLSSimpleRequestPreloadedParams: {
      RefPtr<PreloadedOp> preloadedOp =
        new PreloadedOp(aParams);

      actor = std::move(preloadedOp);

      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

bool
RecvPBackgroundLSSimpleRequestConstructor(
                                       PBackgroundLSSimpleRequestParent* aActor,
                                       const LSSimpleRequestParams& aParams)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != LSSimpleRequestParams::T__None);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());

  // The actor is now completely built.

  auto* op = static_cast<LSSimpleRequestBase*>(aActor);

  op->Dispatch();

  return true;
}

bool
DeallocPBackgroundLSSimpleRequestParent(
                                       PBackgroundLSSimpleRequestParent* aActor)
{
  AssertIsOnBackgroundThread();

  // Transfer ownership back from IPDL.
  RefPtr<LSSimpleRequestBase> actor =
    dont_AddRef(static_cast<LSSimpleRequestBase*>(aActor));

  return true;
}

namespace localstorage {

already_AddRefed<mozilla::dom::quota::Client>
CreateQuotaClient()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(CachedNextGenLocalStorageEnabled());

  RefPtr<QuotaClient> client = new QuotaClient();
  return client.forget();
}

} // namespace localstorage

/*******************************************************************************
 * DatastoreOperationBase
 ******************************************************************************/

/*******************************************************************************
 * ConnectionDatastoreOperationBase
 ******************************************************************************/

ConnectionDatastoreOperationBase::ConnectionDatastoreOperationBase(
                                                        Connection* aConnection)
  : mConnection(aConnection)
{
  MOZ_ASSERT(aConnection);
}

ConnectionDatastoreOperationBase::~ConnectionDatastoreOperationBase()
{
  MOZ_ASSERT(!mConnection,
             "ConnectionDatabaseOperationBase::Cleanup() was not called by a "
             "subclass!");
}

void
ConnectionDatastoreOperationBase::Cleanup()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mConnection);

  mConnection = nullptr;

  NoteComplete();
}

void
ConnectionDatastoreOperationBase::OnSuccess()
{
  AssertIsOnOwningThread();
}

void
ConnectionDatastoreOperationBase::OnFailure(nsresult aResultCode)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aResultCode));
}

void
ConnectionDatastoreOperationBase::RunOnConnectionThread()
{
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mConnection);
  MOZ_ASSERT(NS_SUCCEEDED(ResultCode()));

  if (!MayProceedOnNonOwningThread()) {
    SetFailureCode(NS_ERROR_FAILURE);
  } else {
    nsresult rv = mConnection->EnsureStorageConnection();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      SetFailureCode(rv);
    } else {
      MOZ_ASSERT(mConnection->StorageConnection());

      rv = DoDatastoreWork();
      if (NS_FAILED(rv)) {
        SetFailureCode(rv);
      }
    }
  }

  MOZ_ALWAYS_SUCCEEDS(OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL));
}

void
ConnectionDatastoreOperationBase::RunOnOwningThread()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mConnection);

  if (!MayProceed()) {
    MaybeSetFailureCode(NS_ERROR_FAILURE);
  } else {
    if (NS_SUCCEEDED(ResultCode())) {
      OnSuccess();
    } else {
      OnFailure(ResultCode());
    }
  }

  Cleanup();
}

NS_IMETHODIMP
ConnectionDatastoreOperationBase::Run()
{
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
                       const nsACString& aOrigin,
                       const nsAString& aFilePath)
  : mConnectionThread(aConnectionThread)
  , mOrigin(aOrigin)
  , mFilePath(aFilePath)
  , mInTransaction(false)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!aOrigin.IsEmpty());
  MOZ_ASSERT(!aFilePath.IsEmpty());
}

Connection::~Connection()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mStorageConnection);
  MOZ_ASSERT(!mCachedStatements.Count());
  MOZ_ASSERT(!mInTransaction);
}

void
Connection::Dispatch(ConnectionDatastoreOperationBase* aOp)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mConnectionThread);

  MOZ_ALWAYS_SUCCEEDS(mConnectionThread->mThread->Dispatch(aOp,
                                                           NS_DISPATCH_NORMAL));
}

void
Connection::Begin(bool aReadonly)
{
  AssertIsOnOwningThread();

  RefPtr<BeginOp> op = new BeginOp(this, aReadonly);

  Dispatch(op);

  mInTransaction = true;
}

void
Connection::Commit()
{
  AssertIsOnOwningThread();

  RefPtr<CommitOp> op = new CommitOp(this);

  Dispatch(op);

  mInTransaction = false;
}

void
Connection::Close(nsIRunnable* aCallback)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aCallback);

  RefPtr<CloseOp> op = new CloseOp(this, aCallback);

  Dispatch(op);
}

nsresult
Connection::EnsureStorageConnection()
{
  AssertIsOnConnectionThread();

  if (!mStorageConnection) {
    nsCOMPtr<mozIStorageConnection> storageConnection;
    nsresult rv =
      GetStorageConnection(mFilePath,
                           getter_AddRefs(storageConnection));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mStorageConnection = storageConnection;
  }

  return NS_OK;
}

void
Connection::CloseStorageConnection()
{
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mStorageConnection);

  mCachedStatements.Clear();

  MOZ_ALWAYS_SUCCEEDS(mStorageConnection->Close());
  mStorageConnection = nullptr;
}

nsresult
Connection::GetCachedStatement(const nsACString& aQuery,
                                       CachedStatement* aCachedStatement)
{
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

Connection::
CachedStatement::CachedStatement()
{
  AssertIsOnConnectionThread();

  MOZ_COUNT_CTOR(Connection::CachedStatement);
}

Connection::
CachedStatement::~CachedStatement()
{
  AssertIsOnConnectionThread();

  MOZ_COUNT_DTOR(Connection::CachedStatement);
}

mozIStorageStatement*
Connection::
CachedStatement::operator->() const
{
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mStatement);

  return mStatement;
}

void
Connection::
CachedStatement::Assign(Connection* aConnection,
                        already_AddRefed<mozIStorageStatement> aStatement)
{
  AssertIsOnConnectionThread();

  mScoper.reset();

  mStatement = aStatement;

  if (mStatement) {
    mScoper.emplace(mStatement);
  }
}

nsresult
Connection::
BeginOp::DoDatastoreWork()
{
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mConnection);

  CachedStatement stmt;
  nsresult rv;
  if (mReadonly) {
    rv = mConnection->GetCachedStatement(NS_LITERAL_CSTRING("BEGIN;"), &stmt);
  } else {
    rv = mConnection->GetCachedStatement(NS_LITERAL_CSTRING("BEGIN IMMEDIATE;"),
                                         &stmt);
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

nsresult
Connection::
CommitOp::DoDatastoreWork()
{
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mConnection);

  CachedStatement stmt;
  nsresult rv =
    mConnection->GetCachedStatement(NS_LITERAL_CSTRING("COMMIT;"), &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
Connection::
CloseOp::DoDatastoreWork()
{
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mConnection);

  mConnection->CloseStorageConnection();

  return NS_OK;
}

void
Connection::
CloseOp::Cleanup()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mConnection);

  mConnection->mConnectionThread->mConnections.Remove(mConnection->mOrigin);

  nsCOMPtr<nsIRunnable> callback;
  mCallback.swap(callback);

  callback->Run();

  ConnectionDatastoreOperationBase::Cleanup();
}

/*******************************************************************************
 * ConnectionThread implementation
 ******************************************************************************/

ConnectionThread::ConnectionThread()
{
  AssertIsOnOwningThread();
  AssertIsOnBackgroundThread();

  MOZ_ALWAYS_SUCCEEDS(NS_NewNamedThread("LS Thread", getter_AddRefs(mThread)));
}

ConnectionThread::~ConnectionThread()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mConnections.Count());
}

bool
ConnectionThread::IsOnConnectionThread()
{
  MOZ_ASSERT(mThread);

  bool current;
  return NS_SUCCEEDED(mThread->IsOnCurrentThread(&current)) && current;
}

void
ConnectionThread::AssertIsOnConnectionThread()
{
  MOZ_ASSERT(IsOnConnectionThread());
}

already_AddRefed<Connection>
ConnectionThread::CreateConnection(const nsACString& aOrigin,
                                   const nsAString& aFilePath)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!aOrigin.IsEmpty());
  MOZ_ASSERT(!mConnections.GetWeak(aOrigin));

  RefPtr<Connection> connection = new Connection(this, aOrigin, aFilePath);
  mConnections.Put(aOrigin, connection);

  return connection.forget();
}

void
ConnectionThread::Shutdown()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mThread);

  mThread->Shutdown();
}

nsresult
SetItemOp::DoDatastoreWork()
{
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mConnection);

  Connection::CachedStatement stmt;
  nsresult rv = mConnection->GetCachedStatement(NS_LITERAL_CSTRING(
    "INSERT OR REPLACE INTO data (key, value) "
    "VALUES(:key, :value)"),
    &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("key"), mKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("value"), mValue);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
RemoveItemOp::DoDatastoreWork()
{
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mConnection);

  Connection::CachedStatement stmt;
  nsresult rv = mConnection->GetCachedStatement(NS_LITERAL_CSTRING(
    "DELETE FROM data "
      "WHERE key = :key;"),
    &stmt);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("key"), mKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stmt->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
ClearOp::DoDatastoreWork()
{
  AssertIsOnConnectionThread();
  MOZ_ASSERT(mConnection);

  Connection::CachedStatement stmt;
  nsresult rv = mConnection->GetCachedStatement(NS_LITERAL_CSTRING(
    "DELETE FROM data;"),
    &stmt);
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
 * Datastore
 ******************************************************************************/

Datastore::Datastore(const nsACString& aOrigin,
                     uint32_t aPrivateBrowsingId,
                     int64_t aUsage,
                     already_AddRefed<DirectoryLock>&& aDirectoryLock,
                     already_AddRefed<Connection>&& aConnection,
                     already_AddRefed<QuotaObject>&& aQuotaObject,
                     nsDataHashtable<nsStringHashKey, nsString>& aValues)
  : mDirectoryLock(std::move(aDirectoryLock))
  , mConnection(std::move(aConnection))
  , mQuotaObject(std::move(aQuotaObject))
  , mOrigin(aOrigin)
  , mPrivateBrowsingId(aPrivateBrowsingId)
  , mUsage(aUsage)
  , mClosed(false)
{
  AssertIsOnBackgroundThread();

  mValues.SwapElements(aValues);
}

Datastore::~Datastore()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mClosed);
}

void
Datastore::Close()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mClosed);
  MOZ_ASSERT(!mDatabases.Count());
  MOZ_ASSERT(mDirectoryLock);

  mClosed = true;

  if (IsPersistent()) {
    MOZ_ASSERT(mConnection);
    MOZ_ASSERT(mQuotaObject);

    if (mConnection->InTransaction()) {
      MOZ_ASSERT(mAutoCommitTimer);
      MOZ_ALWAYS_SUCCEEDS(mAutoCommitTimer->Cancel());

      mConnection->Commit();

      mAutoCommitTimer = nullptr;
    }

    // We can't release the directory lock and unregister itself from the
    // hashtable until the connection is fully closed.
    nsCOMPtr<nsIRunnable> callback =
      NewRunnableMethod("dom::Datastore::ConnectionClosedCallback",
                        this,
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

void
Datastore::WaitForConnectionToComplete(nsIRunnable* aCallback)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(!mCompleteCallback);
  MOZ_ASSERT(mClosed);

  mCompleteCallback = aCallback;
}

void
Datastore::NoteLivePrepareDatastoreOp(PrepareDatastoreOp* aPrepareDatastoreOp)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aPrepareDatastoreOp);
  MOZ_ASSERT(!mPrepareDatastoreOps.GetEntry(aPrepareDatastoreOp));
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(!mClosed);

  mPrepareDatastoreOps.PutEntry(aPrepareDatastoreOp);
}

void
Datastore::NoteFinishedPrepareDatastoreOp(
                                        PrepareDatastoreOp* aPrepareDatastoreOp)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aPrepareDatastoreOp);
  MOZ_ASSERT(mPrepareDatastoreOps.GetEntry(aPrepareDatastoreOp));
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(!mClosed);

  mPrepareDatastoreOps.RemoveEntry(aPrepareDatastoreOp);

  MaybeClose();
}

void
Datastore::NoteLivePreparedDatastore(PreparedDatastore* aPreparedDatastore)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aPreparedDatastore);
  MOZ_ASSERT(!mPreparedDatastores.GetEntry(aPreparedDatastore));
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(!mClosed);

  mPreparedDatastores.PutEntry(aPreparedDatastore);
}

void
Datastore::NoteFinishedPreparedDatastore(PreparedDatastore* aPreparedDatastore)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aPreparedDatastore);
  MOZ_ASSERT(mPreparedDatastores.GetEntry(aPreparedDatastore));
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(!mClosed);

  mPreparedDatastores.RemoveEntry(aPreparedDatastore);

  MaybeClose();
}

#ifdef DEBUG
bool
Datastore::HasLivePreparedDatastores() const
{
  AssertIsOnBackgroundThread();

  return mPreparedDatastores.Count();
}
#endif

void
Datastore::NoteLiveDatabase(Database* aDatabase)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);
  MOZ_ASSERT(!mDatabases.GetEntry(aDatabase));
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(!mClosed);

  mDatabases.PutEntry(aDatabase);
}

void
Datastore::NoteFinishedDatabase(Database* aDatabase)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);
  MOZ_ASSERT(mDatabases.GetEntry(aDatabase));
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(!mClosed);

  mDatabases.RemoveEntry(aDatabase);

  MaybeClose();
}

#ifdef DEBUG
bool
Datastore::HasLiveDatabases() const
{
  AssertIsOnBackgroundThread();

  return mDatabases.Count();
}
#endif

uint32_t
Datastore::GetLength() const
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mClosed);

  return mValues.Count();
}

void
Datastore::GetKey(uint32_t aIndex, nsString& aKey) const
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mClosed);

  aKey.SetIsVoid(true);
  for (auto iter = mValues.ConstIter(); !iter.Done(); iter.Next()) {
    if (aIndex == 0) {
      aKey = iter.Key();
      return;
    }
    aIndex--;
  }
}

void
Datastore::GetItem(const nsString& aKey, nsString& aValue) const
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mClosed);

  if (!mValues.Get(aKey, &aValue)) {
    aValue.SetIsVoid(true);
  }
}

void
Datastore::SetItem(Database* aDatabase,
                   const nsString& aKey,
                   const nsString& aValue,
                   LSWriteOpResponse& aResponse)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);
  MOZ_ASSERT(!mClosed);

  nsString oldValue;
  if (!mValues.Get(aKey, &oldValue)) {
    oldValue.SetIsVoid(true);
  }

  bool changed;
  if (oldValue == aValue && oldValue.IsVoid() == aValue.IsVoid()) {
    changed = false;
  } else {
    changed = true;

    int64_t delta = 0;

    if (oldValue.IsVoid()) {
      delta += static_cast<int64_t>(aKey.Length());
    }

    delta += static_cast<int64_t>(aValue.Length()) -
             static_cast<int64_t>(oldValue.Length());

    if (!UpdateUsage(delta)) {
      aResponse = NS_ERROR_FILE_NO_DEVICE_SPACE;
      return;
    }

    mValues.Put(aKey, aValue);

    NotifyObservers(aDatabase, aKey, oldValue, aValue);

    if (IsPersistent()) {
      EnsureTransaction();

      RefPtr<SetItemOp> op = new SetItemOp(mConnection, aKey, aValue);
      mConnection->Dispatch(op);
    }
  }

  LSNotifyInfo info;
  info.changed() = changed;
  info.oldValue() = oldValue;
  aResponse = info;
}

void
Datastore::RemoveItem(Database* aDatabase,
                      const nsString& aKey,
                      LSWriteOpResponse& aResponse)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);
  MOZ_ASSERT(!mClosed);

  bool changed;
  nsString oldValue;
  if (!mValues.Get(aKey, &oldValue)) {
    oldValue.SetIsVoid(true);
    changed = false;
  } else {
    changed = true;

    int64_t delta = -(static_cast<int64_t>(aKey.Length()) +
                      static_cast<int64_t>(oldValue.Length()));

    DebugOnly<bool> ok = UpdateUsage(delta);
    MOZ_ASSERT(ok);

    mValues.Remove(aKey);

    NotifyObservers(aDatabase, aKey, oldValue, VoidString());

    if (IsPersistent()) {
      EnsureTransaction();

      RefPtr<RemoveItemOp> op = new RemoveItemOp(mConnection, aKey);
      mConnection->Dispatch(op);
    }
  }

  LSNotifyInfo info;
  info.changed() = changed;
  info.oldValue() = oldValue;
  aResponse = info;
}

void
Datastore::Clear(Database* aDatabase,
                 LSWriteOpResponse& aResponse)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mClosed);

  bool changed;
  if (!mValues.Count()) {
    changed = false;
  } else {
    changed = true;

    DebugOnly<bool> ok = UpdateUsage(-mUsage);
    MOZ_ASSERT(ok);

    mValues.Clear();

    if (aDatabase) {
      NotifyObservers(aDatabase, VoidString(), VoidString(), VoidString());
    }

    if (IsPersistent()) {
      EnsureTransaction();

      RefPtr<ClearOp> op = new ClearOp(mConnection);
      mConnection->Dispatch(op);
    }
  }

  LSNotifyInfo info;
  info.changed() = changed;
  aResponse = info;
}

void
Datastore::GetKeys(nsTArray<nsString>& aKeys) const
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mClosed);

  for (auto iter = mValues.ConstIter(); !iter.Done(); iter.Next()) {
    aKeys.AppendElement(iter.Key());
  }
}

void
Datastore::MaybeClose()
{
  AssertIsOnBackgroundThread();

  if (!mPrepareDatastoreOps.Count() &&
      !mPreparedDatastores.Count() &&
      !mDatabases.Count()) {
    Close();
  }
}

void
Datastore::ConnectionClosedCallback()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(mConnection);
  MOZ_ASSERT(mQuotaObject);
  MOZ_ASSERT(mClosed);

  // Release the quota object first.
  mQuotaObject = nullptr;

  // Now it's safe to release the directory lock and unregister itself from
  // the hashtable.

  mDirectoryLock = nullptr;
  mConnection = nullptr;

  CleanupMetadata();

  if (mCompleteCallback) {
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(mCompleteCallback.forget()));
  }
}

void
Datastore::CleanupMetadata()
{
  AssertIsOnBackgroundThread();

  MOZ_ASSERT(gDatastores);
  MOZ_ASSERT(gDatastores->Get(mOrigin));
  gDatastores->Remove(mOrigin);

  if (!gDatastores->Count()) {
    gDatastores = nullptr;
  }
}

bool
Datastore::UpdateUsage(int64_t aDelta)
{
  AssertIsOnBackgroundThread();

  // Check internal LocalStorage origin limit.
  int64_t newUsage = mUsage + aDelta;
  if (newUsage > gOriginLimitKB * 1024) {
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

  if (IsPersistent()) {
    RefPtr<Runnable> runnable = NS_NewRunnableFunction(
      "Datastore::UpdateUsage",
      [origin = mOrigin, newUsage] () {
        MOZ_ASSERT(gUsages);
        MOZ_ASSERT(gUsages->Contains(origin));
        gUsages->Put(origin, newUsage);
      });

    QuotaManager* quotaManager = QuotaManager::Get();
    MOZ_ASSERT(quotaManager);

    MOZ_ALWAYS_SUCCEEDS(
      quotaManager->IOThread()->Dispatch(runnable, NS_DISPATCH_NORMAL));
  }

  return true;
}

void
Datastore::NotifyObservers(Database* aDatabase,
                           const nsString& aKey,
                           const nsString& aOldValue,
                           const nsString& aNewValue)
{
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

  PBackgroundParent* databaseBackgroundActor = aDatabase->Manager()->Manager();

  for (Observer* observer : *array) {
    if (observer->Manager() != databaseBackgroundActor) {
      observer->Observe(aDatabase, aKey, aOldValue, aNewValue);
    }
  }
}

void
Datastore::EnsureTransaction()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mConnection);

  if (!mConnection->InTransaction()) {
    mConnection->Begin(/* aReadonly */ false);

    if (!mAutoCommitTimer) {
      mAutoCommitTimer = NS_NewTimer();
      MOZ_ASSERT(mAutoCommitTimer);
    }

    MOZ_ALWAYS_SUCCEEDS(
      mAutoCommitTimer->InitWithNamedFuncCallback(
                                          AutoCommitTimerCallback,
                                          this,
                                          kAutoCommitTimeoutMs,
                                          nsITimer::TYPE_ONE_SHOT,
                                          "Database::AutoCommitTimerCallback"));
  }
}

// static
void
Datastore::AutoCommitTimerCallback(nsITimer* aTimer, void* aClosure)
{
  MOZ_ASSERT(aClosure);

  auto* self = static_cast<Datastore*>(aClosure);
  MOZ_ASSERT(self);

  MOZ_ASSERT(self->mConnection);
  MOZ_ASSERT(self->mConnection->InTransaction());

  self->mConnection->Commit();
}

/*******************************************************************************
 * PreparedDatastore
 ******************************************************************************/

void
PreparedDatastore::Destroy()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(gPreparedDatastores);
  MOZ_ASSERT(gPreparedDatastores->Get(mDatastoreId));

  nsAutoPtr<PreparedDatastore> preparedDatastore;
  gPreparedDatastores->Remove(mDatastoreId, &preparedDatastore);
  MOZ_ASSERT(preparedDatastore);
}

// static
void
PreparedDatastore::TimerCallback(nsITimer* aTimer, void* aClosure)
{
  AssertIsOnBackgroundThread();

  auto* self = static_cast<PreparedDatastore*>(aClosure);
  MOZ_ASSERT(self);

  self->Destroy();
}

/*******************************************************************************
 * Object
 ******************************************************************************/

Object::Object(const PrincipalInfo& aPrincipalInfo,
               const nsAString& aDocumentURI,
               uint32_t aPrivateBrowsingId)
  : mPrincipalInfo(aPrincipalInfo)
  , mDocumentURI(aDocumentURI)
  , mPrivateBrowsingId(aPrivateBrowsingId)
  , mActorDestroyed(false)
{
  AssertIsOnBackgroundThread();
}

Object::~Object()
{
  MOZ_ASSERT(mActorDestroyed);
}

void
Object::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  mActorDestroyed = true;
}

mozilla::ipc::IPCResult
Object::RecvDeleteMe()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  IProtocol* mgr = Manager();
  if (!PBackgroundLSObjectParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

PBackgroundLSDatabaseParent*
Object::AllocPBackgroundLSDatabaseParent(const uint64_t& aDatastoreId)
{
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

  RefPtr<Database> database = new Database(this,
                                           preparedDatastore->Origin());

  // Transfer ownership to IPDL.
  return database.forget().take();
}

mozilla::ipc::IPCResult
Object::RecvPBackgroundLSDatabaseConstructor(
                                            PBackgroundLSDatabaseParent* aActor,
                                            const uint64_t& aDatastoreId)
{
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

  return IPC_OK();
}

bool
Object::DeallocPBackgroundLSDatabaseParent(PBackgroundLSDatabaseParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<Database> actor = dont_AddRef(static_cast<Database*>(aActor));

  return true;
}

/*******************************************************************************
 * Database
 ******************************************************************************/

Database::Database(Object* aObject,
                   const nsACString& aOrigin)
  : mObject(aObject)
  , mOrigin(aOrigin)
  , mAllowedToClose(false)
  , mActorDestroyed(false)
  , mRequestedAllowToClose(false)
#ifdef DEBUG
  , mActorWasAlive(false)
#endif
{
  AssertIsOnBackgroundThread();
}

Database::~Database()
{
  MOZ_ASSERT_IF(mActorWasAlive, mAllowedToClose);
  MOZ_ASSERT_IF(mActorWasAlive, mActorDestroyed);
}

void
Database::SetActorAlive(Datastore* aDatastore)
{
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

void
Database::RequestAllowToClose()
{
  AssertIsOnBackgroundThread();

  if (mRequestedAllowToClose) {
    return;
  }

  mRequestedAllowToClose = true;

  // Send the RequestAllowToClose message to the child to avoid racing with the
  // child actor. Except the case when the actor was already destroyed.
  if (mActorDestroyed) {
    MOZ_ASSERT(mAllowedToClose);
  } else {
    Unused << SendRequestAllowToClose();
  }
}

void
Database::AllowToClose()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mAllowedToClose);
  MOZ_ASSERT(mDatastore);

  mAllowedToClose = true;

  mDatastore->NoteFinishedDatabase(this);

  mDatastore = nullptr;

  MOZ_ASSERT(gLiveDatabases);
  gLiveDatabases->RemoveElement(this);

  if (gLiveDatabases->IsEmpty()) {
    gLiveDatabases = nullptr;
  }
}

void
Database::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  mActorDestroyed = true;

  if (!mAllowedToClose) {
    AllowToClose();
  }
}

mozilla::ipc::IPCResult
Database::RecvDeleteMe()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  IProtocol* mgr = Manager();
  if (!PBackgroundLSDatabaseParent::Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
Database::RecvAllowToClose()
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(mAllowedToClose)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  AllowToClose();

  return IPC_OK();
}

mozilla::ipc::IPCResult
Database::RecvGetLength(uint32_t* aLength)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aLength);
  MOZ_ASSERT(mDatastore);

  if (NS_WARN_IF(mAllowedToClose)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  *aLength = mDatastore->GetLength();

  return IPC_OK();
}

mozilla::ipc::IPCResult
Database::RecvGetKey(const uint32_t& aIndex, nsString* aKey)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aKey);
  MOZ_ASSERT(mDatastore);

  if (NS_WARN_IF(mAllowedToClose)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  mDatastore->GetKey(aIndex, *aKey);

  return IPC_OK();
}

mozilla::ipc::IPCResult
Database::RecvGetItem(const nsString& aKey, nsString* aValue)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aValue);
  MOZ_ASSERT(mDatastore);

  if (NS_WARN_IF(mAllowedToClose)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  mDatastore->GetItem(aKey, *aValue);

  return IPC_OK();
}

mozilla::ipc::IPCResult
Database::RecvSetItem(const nsString& aKey,
                      const nsString& aValue,
                      LSWriteOpResponse* aResponse)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aResponse);
  MOZ_ASSERT(mDatastore);

  if (NS_WARN_IF(mAllowedToClose)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  mDatastore->SetItem(this, aKey, aValue, *aResponse);

  return IPC_OK();
}

mozilla::ipc::IPCResult
Database::RecvRemoveItem(const nsString& aKey,
                         LSWriteOpResponse* aResponse)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aResponse);
  MOZ_ASSERT(mDatastore);

  if (NS_WARN_IF(mAllowedToClose)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  mDatastore->RemoveItem(this, aKey, *aResponse);

  return IPC_OK();
}

mozilla::ipc::IPCResult
Database::RecvClear(LSWriteOpResponse* aResponse)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aResponse);
  MOZ_ASSERT(mDatastore);

  if (NS_WARN_IF(mAllowedToClose)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  mDatastore->Clear(this, *aResponse);

  return IPC_OK();
}

mozilla::ipc::IPCResult
Database::RecvGetKeys(nsTArray<nsString>* aKeys)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aKeys);
  MOZ_ASSERT(mDatastore);

  if (NS_WARN_IF(mAllowedToClose)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  mDatastore->GetKeys(*aKeys);

  return IPC_OK();
}

/*******************************************************************************
 * Observer
 ******************************************************************************/

Observer::Observer(const nsACString& aOrigin)
  : mOrigin(aOrigin)
  , mActorDestroyed(false)
{
  AssertIsOnBackgroundThread();
}

Observer::~Observer()
{
  MOZ_ASSERT(mActorDestroyed);
}

void
Observer::Observe(Database* aDatabase,
                  const nsString& aKey,
                  const nsString& aOldValue,
                  const nsString& aNewValue)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aDatabase);

  Object* object = aDatabase->GetObject();
  MOZ_ASSERT(object);

  Unused << SendObserve(object->GetPrincipalInfo(),
                        object->PrivateBrowsingId(),
                        object->DocumentURI(),
                        aKey,
                        aOldValue,
                        aNewValue);
}

void
Observer::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorDestroyed);

  mActorDestroyed = true;

  MOZ_ASSERT(gObservers);

  nsTArray<Observer*>* array;
  gObservers->Get(mOrigin, &array);
  MOZ_ASSERT(array);

  array->RemoveElement(this);

  if (array->IsEmpty()) {
    gObservers->Remove(mOrigin);
  }

  if (!gObservers->Count()) {
    gObservers = nullptr;
  }
}

mozilla::ipc::IPCResult
Observer::RecvDeleteMe()
{
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

LSRequestBase::LSRequestBase(nsIEventTarget* aMainEventTarget)
  : mMainEventTarget(aMainEventTarget)
  , mState(State::Initial)
{
}

LSRequestBase::~LSRequestBase()
{
  MOZ_ASSERT_IF(MayProceedOnNonOwningThread(),
                mState == State::Initial || mState == State::Completed);
}

void
LSRequestBase::Dispatch()
{
  AssertIsOnOwningThread();

  mState = State::Opening;

  if (mMainEventTarget) {
    MOZ_ALWAYS_SUCCEEDS(mMainEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));
  } else {
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(this));
  }
}

nsresult
LSRequestBase::NestedRun()
{
  return NS_OK;
}

void
LSRequestBase::SendReadyMessage()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingReadyMessage);

  if (!MayProceed()) {
    MaybeSetFailureCode(NS_ERROR_FAILURE);

    Cleanup();

    mState = State::Completed;
  } else {
    Unused << SendReady();

    mState = State::WaitingForFinish;
  }
}

void
LSRequestBase::SendResults()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingResults);

  if (!MayProceed()) {
    MaybeSetFailureCode(NS_ERROR_FAILURE);
  } else {
    LSRequestResponse response;

    if (NS_SUCCEEDED(ResultCode())) {
      GetResponse(response);
    } else {
      response = ResultCode();
    }

    Unused <<
      PBackgroundLSRequestParent::Send__delete__(this, response);
  }

  Cleanup();

  mState = State::Completed;
}

NS_IMETHODIMP
LSRequestBase::Run()
{
  nsresult rv;

  switch (mState) {
    case State::Opening:
      rv = Open();
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

void
LSRequestBase::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();

  NoteComplete();
}

mozilla::ipc::IPCResult
LSRequestBase::RecvCancel()
{
  AssertIsOnOwningThread();

  IProtocol* mgr = Manager();
  if (!PBackgroundLSRequestParent::Send__delete__(this, NS_ERROR_FAILURE)) {
    return IPC_FAIL_NO_REASON(mgr);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
LSRequestBase::RecvFinish()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::WaitingForFinish);

  mState = State::SendingResults;
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(this));

  return IPC_OK();
}

/*******************************************************************************
 * PrepareDatastoreOp
 ******************************************************************************/

PrepareDatastoreOp::PrepareDatastoreOp(nsIEventTarget* aMainEventTarget,
                                       const LSRequestParams& aParams)
  : LSRequestBase(aMainEventTarget)
  , mMainEventTarget(aMainEventTarget)
  , mLoadDataOp(nullptr)
  , mParams(aParams.get_LSRequestPrepareDatastoreParams())
  , mPrivateBrowsingId(0)
  , mUsage(0)
  , mNestedState(NestedState::BeforeNesting)
  , mDatabaseNotAvailable(false)
  , mRequestedDirectoryLock(false)
  , mInvalidated(false)
#ifdef DEBUG
  , mDEBUGUsage(0)
#endif
{
  MOZ_ASSERT(aParams.type() ==
               LSRequestParams::TLSRequestPrepareDatastoreParams);
}

PrepareDatastoreOp::~PrepareDatastoreOp()
{
  MOZ_ASSERT(!mDirectoryLock);
  MOZ_ASSERT_IF(MayProceedOnNonOwningThread(),
                mState == State::Initial || mState == State::Completed);
  MOZ_ASSERT(!mLoadDataOp);
}

nsresult
PrepareDatastoreOp::Open()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State::Opening);
  MOZ_ASSERT(mNestedState == NestedState::BeforeNesting);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      !MayProceedOnNonOwningThread()) {
    return NS_ERROR_FAILURE;
  }

  const PrincipalInfo& principalInfo = mParams.principalInfo();

  if (principalInfo.type() == PrincipalInfo::TSystemPrincipalInfo) {
    QuotaManager::GetInfoForChrome(&mSuffix, &mGroup, &mOrigin);
  } else {
    MOZ_ASSERT(principalInfo.type() == PrincipalInfo::TContentPrincipalInfo);

    nsresult rv;
    nsCOMPtr<nsIPrincipal> principal =
      PrincipalInfoToPrincipal(principalInfo, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = QuotaManager::GetInfoFromPrincipal(principal,
                                            &mSuffix,
                                            &mGroup,
                                            &mMainThreadOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = principal->GetPrivateBrowsingId(&mPrivateBrowsingId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mArchivedOriginInfo = ArchivedOriginInfo::Create(principal);
    if (NS_WARN_IF(!mArchivedOriginInfo)) {
      return NS_ERROR_FAILURE;
    }
  }

  // This service has to be started on the main thread currently.
  nsCOMPtr<mozIStorageService> ss;
  if (NS_WARN_IF(!(ss = do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID)))) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = QuotaClient::RegisterObservers(OwningEventTarget());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mState = State::Nesting;
  mNestedState = NestedState::CheckExistingOperations;

  MOZ_ALWAYS_SUCCEEDS(OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

nsresult
PrepareDatastoreOp::CheckExistingOperations()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::CheckExistingOperations);
  MOZ_ASSERT(gPrepareDatastoreOps);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !MayProceed()) {
    return NS_ERROR_FAILURE;
  }

  // Normally it's safe to access member variables without a mutex because even
  // though we hop between threads, the variables are never accessed by multiple
  // threads at the same time.
  // However, the methods OriginIsKnown and Origin can be called at any time.
  // So we have to make sure the member variable is set on the same thread as
  // those methods are called.
  mOrigin = mMainThreadOrigin;

  MOZ_ASSERT(!mOrigin.IsEmpty());

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

  nsresult rv = CheckClosingDatastore();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
PrepareDatastoreOp::CheckClosingDatastore()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::CheckClosingDatastore);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !MayProceed()) {
    return NS_ERROR_FAILURE;
  }

  mNestedState = NestedState::PreparationPending;

  RefPtr<Datastore> datastore;
  if (gDatastores &&
      (datastore = gDatastores->Get(mOrigin)) &&
      datastore->IsClosed()) {
    datastore->WaitForConnectionToComplete(this);

    return NS_OK;
  }

  nsresult rv = BeginDatastorePreparation();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
PrepareDatastoreOp::BeginDatastorePreparation()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::PreparationPending);

  if (gDatastores && (mDatastore = gDatastores->Get(mOrigin))) {
    MOZ_ASSERT(!mDatastore->IsClosed());

    mDatastore->NoteLivePrepareDatastoreOp(this);

    mState = State::SendingReadyMessage;
    mNestedState = NestedState::AfterNesting;

    Unused << this->Run();

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

nsresult
PrepareDatastoreOp::QuotaManagerOpen()
{
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

nsresult
PrepareDatastoreOp::OpenDirectory()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::PreparationPending ||
             mNestedState == NestedState::QuotaManagerPending);
  MOZ_ASSERT(!mOrigin.IsEmpty());
  MOZ_ASSERT(!mDirectoryLock);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());
  MOZ_ASSERT(QuotaManager::Get());

  mNestedState = NestedState::DirectoryOpenPending;
  QuotaManager::Get()->OpenDirectory(PERSISTENCE_TYPE_DEFAULT,
                                     mGroup,
                                     mOrigin,
                                     mozilla::dom::quota::Client::LS,
                                     /* aExclusive */ false,
                                     this);

  mRequestedDirectoryLock = true;

  return NS_OK;
}

nsresult
PrepareDatastoreOp::SendToIOThread()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::DirectoryOpenPending);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !MayProceed()) {
    return NS_ERROR_FAILURE;
  }

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
    mState = State::SendingReadyMessage;
    mNestedState = NestedState::AfterNesting;

    Unused << this->Run();

    return NS_OK;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  // Must set this before dispatching otherwise we will race with the IO thread.
  mNestedState = NestedState::DatabaseWorkOpen;

  nsresult rv = quotaManager->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
PrepareDatastoreOp::DatabaseWork()
{
  AssertIsOnIOThread();
  MOZ_ASSERT(mArchivedOriginInfo);
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::DatabaseWorkOpen);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      !MayProceedOnNonOwningThread()) {
    return NS_ERROR_FAILURE;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  nsresult rv;

  if (!gArchivedOrigins) {
    rv = LoadArchivedOrigins();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    MOZ_ASSERT(gArchivedOrigins);
  }

  bool hasDataForMigration =
    gArchivedOrigins->GetEntry(mArchivedOriginInfo->Origin());

  bool createIfNotExists = mParams.createIfNotExists() || hasDataForMigration;

  nsCOMPtr<nsIFile> directoryEntry;
  rv = quotaManager->EnsureOriginIsInitialized(PERSISTENCE_TYPE_DEFAULT,
                                               mSuffix,
                                               mGroup,
                                               mOrigin,
                                               createIfNotExists,
                                               getter_AddRefs(directoryEntry));
  if (rv == NS_ERROR_NOT_AVAILABLE) {
    return DatabaseNotAvailable();
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = directoryEntry->Append(NS_LITERAL_STRING(LS_DIRECTORY_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = EnsureDirectoryEntry(directoryEntry,
                            createIfNotExists,
                            /* aIsDirectory */ true);
  if (rv == NS_ERROR_NOT_AVAILABLE) {
    return DatabaseNotAvailable();
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = directoryEntry->Append(NS_LITERAL_STRING(DATA_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool alreadyExisted;
  rv = EnsureDirectoryEntry(directoryEntry,
                            createIfNotExists,
                            /* aIsDirectory */ false,
                            &alreadyExisted);
  if (rv == NS_ERROR_NOT_AVAILABLE) {
    return DatabaseNotAvailable();
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (alreadyExisted) {
    MOZ_ASSERT(gUsages);
    MOZ_ASSERT(gUsages->Get(mOrigin, &mUsage));
  } else {
    MOZ_ASSERT(mUsage == 0);
    InitUsageForOrigin(mOrigin, mUsage);
  }

  rv = directoryEntry->GetPath(mDatabaseFilePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = CreateStorageConnection(directoryEntry,
                               mOrigin,
                               getter_AddRefs(connection));
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
    rv = GetUsage(connection, mArchivedOriginInfo, &newUsage);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    RefPtr<QuotaObject> quotaObject = GetQuotaObject();
    MOZ_ASSERT(quotaObject);

    if (!quotaObject->MaybeUpdateSize(newUsage, /* aTruncate */ true)) {
      return NS_ERROR_FILE_NO_DEVICE_SPACE;
    }

    mozStorageTransaction transaction(connection, false,
                                  mozIStorageConnection::TRANSACTION_IMMEDIATE);

    nsCOMPtr<mozIStorageStatement> stmt;
    rv = connection->CreateStatement(NS_LITERAL_CSTRING(
      "INSERT INTO data (key, value) "
        "SELECT key, value "
        "FROM webappsstore2 "
        "WHERE originKey = :originKey "
        "AND originAttributes = :originAttributes;"

    ), getter_AddRefs(stmt));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = mArchivedOriginInfo->BindToStatement(stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = connection->CreateStatement(NS_LITERAL_CSTRING(
      "DELETE FROM webappsstore2 "
        "WHERE originKey = :originKey "
        "AND originAttributes = :originAttributes;"
    ), getter_AddRefs(stmt));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = mArchivedOriginInfo->BindToStatement(stmt);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = stmt->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = transaction.Commit();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = DetachArchiveDatabase(connection);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(gArchivedOrigins);
    MOZ_ASSERT(gArchivedOrigins->GetEntry(mArchivedOriginInfo->Origin()));
    gArchivedOrigins->RemoveEntry(mArchivedOriginInfo->Origin());

    mUsage = newUsage;

    MOZ_ASSERT(gUsages);
    MOZ_ASSERT(gUsages->Contains(mOrigin));
    gUsages->Put(mOrigin, newUsage);
  }

  // Must close connection before dispatching otherwise we might race with the
  // connection thread which needs to open the same database.
  MOZ_ALWAYS_SUCCEEDS(connection->Close());

  // Must set this before dispatching otherwise we will race with the owning
  // thread.
  mNestedState = NestedState::BeginLoadData;

  rv = OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
PrepareDatastoreOp::DatabaseNotAvailable()
{
  AssertIsOnIOThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::DatabaseWorkOpen);

  mDatabaseNotAvailable = true;

  // Must set this before dispatching otherwise we will race with the owning
  // thread.
  mState = State::SendingReadyMessage;
  mNestedState = NestedState::AfterNesting;

  nsresult rv = OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
PrepareDatastoreOp::EnsureDirectoryEntry(nsIFile* aEntry,
                                         bool aCreateIfNotExists,
                                         bool aIsDirectory,
                                         bool* aAlreadyExisted)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aEntry);

  bool exists;
  nsresult rv = aEntry->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    if (!aCreateIfNotExists) {
      return NS_ERROR_NOT_AVAILABLE;
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

nsresult
PrepareDatastoreOp::VerifyDatabaseInformation(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT origin "
    "FROM database"
  ), getter_AddRefs(stmt));
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

already_AddRefed<QuotaObject>
PrepareDatastoreOp::GetQuotaObject()
{
  MOZ_ASSERT(IsOnOwningThread() || IsOnIOThread());
  MOZ_ASSERT(!mGroup.IsEmpty());
  MOZ_ASSERT(!mOrigin.IsEmpty());
  MOZ_ASSERT(!mDatabaseFilePath.IsEmpty());

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  RefPtr<QuotaObject> quotaObject =
    quotaManager->GetQuotaObject(PERSISTENCE_TYPE_DEFAULT,
                                 mGroup,
                                 mOrigin,
                                 mDatabaseFilePath,
                                 mUsage);
  MOZ_ASSERT(quotaObject);

  return quotaObject.forget();
}

nsresult
PrepareDatastoreOp::BeginLoadData()
{
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

  mConnection = gConnectionThread->CreateConnection(mOrigin,
                                                    mDatabaseFilePath);
  MOZ_ASSERT(mConnection);

  MOZ_ASSERT(!mConnection->InTransaction());

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

nsresult
PrepareDatastoreOp::NestedRun()
{
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

void
PrepareDatastoreOp::GetResponse(LSRequestResponse& aResponse)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingResults);
  MOZ_ASSERT(NS_SUCCEEDED(ResultCode()));

  if (mDatabaseNotAvailable) {
    MOZ_ASSERT(!mParams.createIfNotExists());

    LSRequestPrepareDatastoreResponse prepareDatastoreResponse;
    prepareDatastoreResponse.datastoreId() = null_t();

    aResponse = prepareDatastoreResponse;

    return;
  }

  if (!mDatastore) {
    MOZ_ASSERT(mUsage == mDEBUGUsage);

    RefPtr<QuotaObject> quotaObject;

    if (mPrivateBrowsingId == 0) {
      quotaObject = GetQuotaObject();
      MOZ_ASSERT(quotaObject);
    }

    mDatastore = new Datastore(mOrigin,
                               mPrivateBrowsingId,
                               mUsage,
                               mDirectoryLock.forget(),
                               mConnection.forget(),
                               quotaObject.forget(),
                               mValues);

    mDatastore->NoteLivePrepareDatastoreOp(this);

    if (!gDatastores) {
      gDatastores = new DatastoreHashtable();
    }

    MOZ_ASSERT(!gDatastores->Get(mOrigin));
    gDatastores->Put(mOrigin, mDatastore);
  }

  uint64_t datastoreId = ++gLastDatastoreId;

  nsAutoPtr<PreparedDatastore> preparedDatastore(
    new PreparedDatastore(mDatastore,
                          mOrigin,
                          datastoreId,
                          /* aForPreload */ !mParams.createIfNotExists()));

  if (!gPreparedDatastores) {
    gPreparedDatastores = new PreparedDatastoreHashtable();
  }
  gPreparedDatastores->Put(datastoreId, preparedDatastore);

  if (mInvalidated) {
    preparedDatastore->Invalidate();
  }

  preparedDatastore.forget();

  LSRequestPrepareDatastoreResponse prepareDatastoreResponse;
  prepareDatastoreResponse.datastoreId() = datastoreId;

  aResponse = prepareDatastoreResponse;
}

void
PrepareDatastoreOp::Cleanup()
{
  AssertIsOnOwningThread();

  if (mDatastore) {
    MOZ_ASSERT(!mDirectoryLock);
    MOZ_ASSERT(!mConnection);

    if (NS_FAILED(ResultCode())) {
      MOZ_ASSERT(!mDatastore->IsClosed());
      MOZ_ASSERT(!mDatastore->HasLiveDatabases());
      MOZ_ASSERT(!mDatastore->HasLivePreparedDatastores());
      mDatastore->Close();
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
      NewRunnableMethod("dom::OpenDatabaseOp::ConnectionClosedCallback",
                        this,
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

void
PrepareDatastoreOp::ConnectionClosedCallback()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(ResultCode()));
  MOZ_ASSERT(mDirectoryLock);
  MOZ_ASSERT(mConnection);

  mConnection = nullptr;
  mDirectoryLock = nullptr;

  CleanupMetadata();
}

void
PrepareDatastoreOp::CleanupMetadata()
{
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

NS_IMPL_ISUPPORTS_INHERITED0(PrepareDatastoreOp, LSRequestBase)

void
PrepareDatastoreOp::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();

  LSRequestBase::ActorDestroy(aWhy);

  if (mLoadDataOp) {
    mLoadDataOp->NoteComplete();
  }
}

void
PrepareDatastoreOp::DirectoryLockAcquired(DirectoryLock* aLock)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  mDirectoryLock = aLock;

  nsresult rv = SendToIOThread();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MaybeSetFailureCode(rv);

    // The caller holds a strong reference to us, no need for a self reference
    // before calling Run().

    mState = State::SendingReadyMessage;
    mNestedState = NestedState::AfterNesting;

    MOZ_ALWAYS_SUCCEEDS(Run());

    return;
  }
}

void
PrepareDatastoreOp::DirectoryLockFailed()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Nesting);
  MOZ_ASSERT(mNestedState == NestedState::DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  MaybeSetFailureCode(NS_ERROR_FAILURE);

  // The caller holds a strong reference to us, no need for a self reference
  // before calling Run().

  mState = State::SendingReadyMessage;
  mNestedState = NestedState::AfterNesting;

  MOZ_ALWAYS_SUCCEEDS(Run());
}

nsresult
PrepareDatastoreOp::
LoadDataOp::DoDatastoreWork()
{
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
  nsresult rv = mConnection->GetCachedStatement(NS_LITERAL_CSTRING(
    "SELECT key, value "
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

    nsString value;
    rv = stmt->GetString(1, value);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mPrepareDatastoreOp->mValues.Put(key, value);
#ifdef DEBUG
    mPrepareDatastoreOp->mDEBUGUsage += key.Length() + value.Length();
#endif
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void
PrepareDatastoreOp::
LoadDataOp::OnSuccess()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mPrepareDatastoreOp);
  MOZ_ASSERT(mPrepareDatastoreOp->mState == State::Nesting);
  MOZ_ASSERT(mPrepareDatastoreOp->mNestedState ==
               NestedState::DatabaseWorkLoadData);
  MOZ_ASSERT(mPrepareDatastoreOp->mLoadDataOp == this);

  mPrepareDatastoreOp->mState = State::SendingReadyMessage;
  mPrepareDatastoreOp->mNestedState = NestedState::AfterNesting;

  MOZ_ALWAYS_SUCCEEDS(mPrepareDatastoreOp->Run());
}

void
PrepareDatastoreOp::
LoadDataOp::OnFailure(nsresult aResultCode)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mPrepareDatastoreOp);
  MOZ_ASSERT(mPrepareDatastoreOp->mState == State::Nesting);
  MOZ_ASSERT(mPrepareDatastoreOp->mNestedState ==
               NestedState::DatabaseWorkLoadData);
  MOZ_ASSERT(mPrepareDatastoreOp->mLoadDataOp == this);

  mPrepareDatastoreOp->SetFailureCode(aResultCode);
  mPrepareDatastoreOp->mState = State::SendingReadyMessage;
  mPrepareDatastoreOp->mNestedState = NestedState::AfterNesting;

  MOZ_ALWAYS_SUCCEEDS(mPrepareDatastoreOp->Run());
}

void
PrepareDatastoreOp::
LoadDataOp::Cleanup()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mPrepareDatastoreOp);
  MOZ_ASSERT(mPrepareDatastoreOp->mLoadDataOp == this);

  mPrepareDatastoreOp->mLoadDataOp = nullptr;
  mPrepareDatastoreOp = nullptr;

  ConnectionDatastoreOperationBase::Cleanup();
}

/*******************************************************************************
 * PrepareObserverOp
 ******************************************************************************/

PrepareObserverOp::PrepareObserverOp(nsIEventTarget* aMainEventTarget,
                                     const LSRequestParams& aParams)
  : LSRequestBase(aMainEventTarget)
  , mParams(aParams.get_LSRequestPrepareObserverParams())
{
  MOZ_ASSERT(aParams.type() ==
               LSRequestParams::TLSRequestPrepareObserverParams);
}

nsresult
PrepareObserverOp::Open()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State::Opening);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      !MayProceedOnNonOwningThread()) {
    return NS_ERROR_FAILURE;
  }

  const PrincipalInfo& principalInfo = mParams.principalInfo();

  if (principalInfo.type() == PrincipalInfo::TSystemPrincipalInfo) {
    QuotaManager::GetInfoForChrome(nullptr, nullptr, &mOrigin);
  } else {
    MOZ_ASSERT(principalInfo.type() == PrincipalInfo::TContentPrincipalInfo);

    nsresult rv;
    nsCOMPtr<nsIPrincipal> principal =
      PrincipalInfoToPrincipal(principalInfo, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = QuotaManager::GetInfoFromPrincipal(principal,
                                            nullptr,
                                            nullptr,
                                            &mOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  mState = State::SendingReadyMessage;
  MOZ_ALWAYS_SUCCEEDS(OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

void
PrepareObserverOp::GetResponse(LSRequestResponse& aResponse)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingResults);
  MOZ_ASSERT(NS_SUCCEEDED(ResultCode()));

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
+ ******************************************************************************/

LSSimpleRequestBase::LSSimpleRequestBase()
  : mState(State::Initial)
{
}

LSSimpleRequestBase::~LSSimpleRequestBase()
{
  MOZ_ASSERT_IF(MayProceedOnNonOwningThread(),
                mState == State::Initial || mState == State::Completed);
}

void
LSSimpleRequestBase::Dispatch()
{
  AssertIsOnOwningThread();

  mState = State::Opening;

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(this));
}

void
LSSimpleRequestBase::SendResults()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingResults);

  if (!MayProceed()) {
    MaybeSetFailureCode(NS_ERROR_FAILURE);
  } else {
    LSSimpleRequestResponse response;

    if (NS_SUCCEEDED(ResultCode())) {
      GetResponse(response);
    } else {
      response = ResultCode();
    }

    Unused <<
      PBackgroundLSSimpleRequestParent::Send__delete__(this, response);
  }

  mState = State::Completed;
}

NS_IMETHODIMP
LSSimpleRequestBase::Run()
{
  nsresult rv;

  switch (mState) {
    case State::Opening:
      rv = Open();
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

void
LSSimpleRequestBase::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();

  NoteComplete();
}

/*******************************************************************************
 * PreloadedOp
 ******************************************************************************/

PreloadedOp::PreloadedOp(const LSSimpleRequestParams& aParams)
  : mParams(aParams.get_LSSimpleRequestPreloadedParams())
{
  MOZ_ASSERT(aParams.type() ==
               LSSimpleRequestParams::TLSSimpleRequestPreloadedParams);
}

nsresult
PreloadedOp::Open()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State::Opening);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      !MayProceedOnNonOwningThread()) {
    return NS_ERROR_FAILURE;
  }

  const PrincipalInfo& principalInfo = mParams.principalInfo();

  if (principalInfo.type() == PrincipalInfo::TSystemPrincipalInfo) {
    QuotaManager::GetInfoForChrome(nullptr, nullptr, &mOrigin);
  } else {
    MOZ_ASSERT(principalInfo.type() == PrincipalInfo::TContentPrincipalInfo);

    nsresult rv;
    nsCOMPtr<nsIPrincipal> principal =
      PrincipalInfoToPrincipal(principalInfo, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = QuotaManager::GetInfoFromPrincipal(principal,
                                            nullptr,
                                            nullptr,
                                            &mOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  mState = State::SendingResults;
  MOZ_ALWAYS_SUCCEEDS(OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

void
PreloadedOp::GetResponse(LSSimpleRequestResponse& aResponse)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingResults);
  MOZ_ASSERT(NS_SUCCEEDED(ResultCode()));

  bool preloaded;
  RefPtr<Datastore> datastore;
  if (gDatastores &&
      (datastore = gDatastores->Get(mOrigin)) &&
      !datastore->IsClosed()) {
    preloaded = true;
  } else {
    preloaded = false;
  }

  LSSimpleRequestPreloadedResponse preloadedResponse;
  preloadedResponse.preloaded() = preloaded;

  aResponse = preloadedResponse;
}

/*******************************************************************************
 * ArchivedOriginInfo
 ******************************************************************************/

// static
ArchivedOriginInfo*
ArchivedOriginInfo::Create(nsIPrincipal* aPrincipal)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  nsCString originAttrSuffix;
  nsCString originKey;
  nsresult rv = GenerateOriginKey(aPrincipal, originAttrSuffix, originKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  return new ArchivedOriginInfo(originAttrSuffix, originKey);
}

nsresult
ArchivedOriginInfo::BindToStatement(mozIStorageStatement* aStatement) const
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aStatement);

  nsresult rv =
    aStatement->BindUTF8StringByName(NS_LITERAL_CSTRING("originKey"),
                                     mOriginNoSuffix);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aStatement->BindUTF8StringByName(NS_LITERAL_CSTRING("originAttributes"),
                                        mOriginSuffix);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

/*******************************************************************************
 * QuotaClient
 ******************************************************************************/

QuotaClient* QuotaClient::sInstance = nullptr;
bool QuotaClient::sObserversRegistered = false;

QuotaClient::QuotaClient()
  : mShutdownRequested(false)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!sInstance, "We expect this to be a singleton!");

  sInstance = this;
}

QuotaClient::~QuotaClient()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(sInstance == this, "We expect this to be a singleton!");

  sInstance = nullptr;
}

mozilla::dom::quota::Client::Type
QuotaClient::GetType()
{
  return QuotaClient::LS;
}

// static
nsresult
QuotaClient::RegisterObservers(nsIEventTarget* aBackgroundEventTarget)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aBackgroundEventTarget);

  if (!sObserversRegistered) {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (NS_WARN_IF(!obs)) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIObserver> observer = new Observer(aBackgroundEventTarget);

    nsresult rv =
      obs->AddObserver(observer, kPrivateBrowsingObserverTopic, false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (NS_FAILED(Preferences::AddAtomicUintVarCache(&gOriginLimitKB,
                                                     kDefaultQuotaPref,
                                                     kDefaultOriginLimitKB))) {
      NS_WARNING("Unable to respond to default quota pref changes!");
    }

    sObserversRegistered = true;
  }

  return NS_OK;
}

nsresult
QuotaClient::InitOrigin(PersistenceType aPersistenceType,
                        const nsACString& aGroup,
                        const nsACString& aOrigin,
                        const AtomicBool& aCanceled,
                        UsageInfo* aUsageInfo)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aPersistenceType == PERSISTENCE_TYPE_DEFAULT);

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  nsCOMPtr<nsIFile> directory;
  nsresult rv = quotaManager->GetDirectoryForOrigin(aPersistenceType,
                                                    aOrigin,
                                                    getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(directory);

  rv = directory->Append(NS_LITERAL_STRING(LS_DIRECTORY_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
#ifdef DEBUG
  rv = directory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(exists);
#endif

  nsCOMPtr<nsIFile> file;
  rv = directory->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = file->Append(NS_LITERAL_STRING(DATA_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = file->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    bool isDirectory;
    rv = file->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (NS_WARN_IF(isDirectory)) {
      return NS_ERROR_FAILURE;
    }

    // TODO: Use a special file that contains logical size of the database.
    //       For now, get the usage from the database.

    nsCOMPtr<mozIStorageConnection> connection;
    rv = CreateStorageConnection(file, aOrigin, getter_AddRefs(connection));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    int64_t usage;
    rv = GetUsage(connection, /* aArchivedOriginInfo */ nullptr, &usage);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    InitUsageForOrigin(aOrigin, usage);

    aUsageInfo->AppendToDatabaseUsage(uint64_t(usage));
  }

  // Report unknown files, don't fail, just warn.

  nsCOMPtr<nsIDirectoryEnumerator> directoryEntries;
  rv = directory->GetDirectoryEntries(getter_AddRefs(directoryEntries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
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
      return rv;
    }

    if (!file) {
      break;
    }

    nsString leafName;
    rv = file->GetLeafName(leafName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (leafName.EqualsLiteral(DATA_FILE_NAME)) {
      // Don't need to check if it is a directory or file. We did that above.
      continue;
    }

    if (leafName.EqualsLiteral(JOURNAL_FILE_NAME)) {
      bool isDirectory;
      rv = file->IsDirectory(&isDirectory);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (!isDirectory) {
        continue;
      }
    }

    LS_WARNING("Something (%s) in the directory that doesn't belong!", \
               NS_ConvertUTF16toUTF8(leafName).get());
  }

  return NS_OK;
}

nsresult
QuotaClient::GetUsageForOrigin(PersistenceType aPersistenceType,
                               const nsACString& aGroup,
                               const nsACString& aOrigin,
                               const AtomicBool& aCanceled,
                               UsageInfo* aUsageInfo)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aPersistenceType == PERSISTENCE_TYPE_DEFAULT);
  MOZ_ASSERT(aUsageInfo);

  // We can't open the database at this point, since it can be already used
  // by the connection thread. Use the cached value instead.

  if (gUsages) {
    int64_t usage;
    if (gUsages->Get(aOrigin, &usage)) {
      aUsageInfo->AppendToDatabaseUsage(usage);
    }
  }

  return NS_OK;
}

void
QuotaClient::OnOriginClearCompleted(PersistenceType aPersistenceType,
                                    const nsACString& aOrigin)
{
  AssertIsOnIOThread();

  if (aPersistenceType != PERSISTENCE_TYPE_DEFAULT) {
    return;
  }

  if (gUsages) {
    gUsages->Remove(aOrigin);
  }
}

void
QuotaClient::ReleaseIOThreadObjects()
{
  AssertIsOnIOThread();

  gUsages = nullptr;

  gArchivedOrigins = nullptr;
}

void
QuotaClient::AbortOperations(const nsACString& aOrigin)
{
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
    for (auto iter = gPreparedDatastores->ConstIter();
         !iter.Done();
         iter.Next()) {
      PreparedDatastore* preparedDatastore = iter.Data();
      MOZ_ASSERT(preparedDatastore);

      if (aOrigin.IsVoid() || preparedDatastore->Origin() == aOrigin) {
        preparedDatastore->Invalidate();
      }
    }
  }

  if (gLiveDatabases) {
    for (Database* database : *gLiveDatabases) {
      if (aOrigin.IsVoid() || database->Origin() == aOrigin) {
        // TODO: This just allows the database to close, but we can actually
        //       set a flag to abort any existing operations, so we can
        //       eventually close faster.

        database->RequestAllowToClose();
      }
    }
  }
}

void
QuotaClient::AbortOperationsForProcess(ContentParentId aContentParentId)
{
  AssertIsOnBackgroundThread();
}

void
QuotaClient::StartIdleMaintenance()
{
  AssertIsOnBackgroundThread();
}

void
QuotaClient::StopIdleMaintenance()
{
  AssertIsOnBackgroundThread();
}

void
QuotaClient::ShutdownWorkThreads()
{
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

  if (gLiveDatabases) {
    for (Database* database : *gLiveDatabases) {
      database->RequestAllowToClose();
    }
  }

  if (gPreparedObsevers) {
    gPreparedObsevers->Clear();
    gPreparedObsevers = nullptr;
  }

  // This should release any local storage related quota objects or directory
  // locks.
  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() {
    // Don't have to check gPreparedDatastores since we nulled it out above.
    return !gPrepareDatastoreOps && !gDatastores && !gLiveDatabases;
  }));

  // And finally, shutdown the connection thread.
  if (gConnectionThread) {
    gConnectionThread->Shutdown();

    gConnectionThread = nullptr;
  }
}

NS_IMETHODIMP
QuotaClient::
ClearPrivateBrowsingRunnable::Run()
{
  AssertIsOnBackgroundThread();

  if (gDatastores) {
    for (auto iter = gDatastores->ConstIter(); !iter.Done(); iter.Next()) {
      Datastore* datastore = iter.Data();
      MOZ_ASSERT(datastore);

      if (datastore->PrivateBrowsingId()) {
        LSWriteOpResponse dummy;
        datastore->Clear(nullptr, dummy);
      }
    }
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(QuotaClient::Observer, nsIObserver)

NS_IMETHODIMP
QuotaClient::
Observer::Observe(nsISupports* aSubject,
                  const char* aTopic,
                  const char16_t* aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!strcmp(aTopic, kPrivateBrowsingObserverTopic)) {
    RefPtr<ClearPrivateBrowsingRunnable> runnable =
      new ClearPrivateBrowsingRunnable();

    MOZ_ALWAYS_SUCCEEDS(
      mBackgroundEventTarget->Dispatch(runnable, NS_DISPATCH_NORMAL));

    return NS_OK;
  }

  NS_WARNING("Unknown observer topic!");
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
