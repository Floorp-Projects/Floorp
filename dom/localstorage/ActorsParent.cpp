/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActorsParent.h"

#include "LocalStorageCommon.h"
#include "mozIStorageConnection.h"
#include "mozIStorageService.h"
#include "mozStorageCID.h"
#include "mozStorageHelper.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/PBackgroundLSDatabaseParent.h"
#include "mozilla/dom/PBackgroundLSRequestParent.h"
#include "mozilla/dom/PBackgroundLSSharedTypes.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsClassHashtable.h"
#include "nsDataHashtable.h"
#include "nsISimpleEnumerator.h"
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
using namespace mozilla::ipc;

namespace {

class Database;

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

class Datastore final
{
  RefPtr<DirectoryLock> mDirectoryLock;
  nsTHashtable<nsPtrHashKey<Database>> mDatabases;
  nsDataHashtable<nsStringHashKey, nsString> mValues;
  const nsCString mOrigin;
  bool mClosed;

public:
  // Created by PrepareDatastoreOp.
  Datastore(const nsACString& aOrigin,
            already_AddRefed<DirectoryLock>&& aDirectoryLock);

  const nsCString&
  Origin() const
  {
    return mOrigin;
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
  NoteLiveDatabase(Database* aDatabase);

  void
  NoteFinishedDatabase(Database* aDatabase);

  bool
  HasLiveDatabases() const;

  uint32_t
  GetLength() const;

  void
  GetKey(uint32_t aIndex, nsString& aKey) const;

  void
  GetItem(const nsString& aKey, nsString& aValue) const;

  void
  SetItem(const nsString& aKey, const nsString& aValue);

  void
  RemoveItem(const nsString& aKey);

  void
  Clear();

  void
  GetKeys(nsTArray<nsString>& aKeys) const;

  NS_INLINE_DECL_REFCOUNTING(Datastore)

private:
  // Reference counted.
  ~Datastore();
};

class PreparedDatastore
{
  RefPtr<Datastore> mDatastore;
  // Strings share buffers if possible, so it's not a problem to duplicate the
  // origin here.
  const nsCString mOrigin;
  bool mInvalidated;

public:
  PreparedDatastore(Datastore* aDatastore,
                    const nsACString& aOrigin)
    : mDatastore(aDatastore)
    , mOrigin(aOrigin)
    , mInvalidated(false)
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(aDatastore);
  }

  already_AddRefed<Datastore>
  ForgetDatastore()
  {
    AssertIsOnBackgroundThread();
    MOZ_ASSERT(mDatastore);

    return mDatastore.forget();
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
  }

  bool
  IsInvalidated() const
  {
    AssertIsOnBackgroundThread();

    return mInvalidated;
  }
};

/*******************************************************************************
 * Actor class declarations
 ******************************************************************************/

class Database final
  : public PBackgroundLSDatabaseParent
{
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
  explicit Database(const nsACString& aOrigin);

  const nsCString&
  Origin() const
  {
    return mOrigin;
  }

  void
  SetActorAlive(already_AddRefed<Datastore>&& aDatastore);

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
  RecvSetItem(const nsString& aKey, const nsString& aValue) override;

  mozilla::ipc::IPCResult
  RecvRemoveItem(const nsString& aKey) override;

  mozilla::ipc::IPCResult
  RecvClear() override;
};

class LSRequestBase
  : public DatastoreOperationBase
  , public PBackgroundLSRequestParent
{
public:
  virtual void
  Dispatch() = 0;

private:
  // IPDL methods.
  void
  ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult
  RecvCancel() override;
};

class PrepareDatastoreOp
  : public LSRequestBase
  , public OpenDirectoryListener
{
  enum class State
  {
    // Just created on the PBackground thread. Next step is OpeningOnMainThread
    // or OpeningOnOwningThread.
    Initial,

    // Waiting to open/opening on the main thread. Next step is FinishOpen.
    OpeningOnMainThread,

    // Waiting to open/opening on the owning thread. Next step is FinishOpen.
    OpeningOnOwningThread,

    // Checking if a prepare datastore operation is already running for given
    // origin on the PBackground thread. Next step is PreparationPending.
    FinishOpen,

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
    // SendingReadyMessage.
    DatabaseWorkOpen,

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

  RefPtr<PrepareDatastoreOp> mDelayedOp;
  RefPtr<DirectoryLock> mDirectoryLock;
  RefPtr<Datastore> mDatastore;
  const LSRequestPrepareDatastoreParams mParams;
  nsCString mSuffix;
  nsCString mGroup;
  nsCString mMainThreadOrigin;
  nsCString mOrigin;
  State mState;
  bool mRequestedDirectoryLock;
  bool mInvalidated;

public:
  explicit PrepareDatastoreOp(const LSRequestParams& aParams);

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

  void
  Dispatch() override;

private:
  ~PrepareDatastoreOp() override;

  nsresult
  OpenOnMainThread();

  nsresult
  OpenOnOwningThread();

  nsresult
  FinishOpen();

  nsresult
  PreparationOpen();

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
  VerifyDatabaseInformation(mozIStorageConnection* aConnection);

  void
  SendReadyMessage();

  void
  SendResults();

  void
  Cleanup();

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD
  Run() override;

  // IPDL overrides.
  mozilla::ipc::IPCResult
  RecvFinish() override;

  // OpenDirectoryListener overrides.
  void
  DirectoryLockAcquired(DirectoryLock* aLock) override;

  void
  DirectoryLockFailed() override;
};

/*******************************************************************************
 * Other class declarations
 ******************************************************************************/

class QuotaClient final
  : public mozilla::dom::quota::Client
{
  static QuotaClient* sInstance;

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

} // namespace

/*******************************************************************************
 * Exported functions
 ******************************************************************************/

PBackgroundLSDatabaseParent*
AllocPBackgroundLSDatabaseParent(const uint64_t& aDatastoreId)
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

  RefPtr<Database> database = new Database(preparedDatastore->Origin());

  // Transfer ownership to IPDL.
  return database.forget().take();
}

bool
RecvPBackgroundLSDatabaseConstructor(PBackgroundLSDatabaseParent* aActor,
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

  database->SetActorAlive(preparedDatastore->ForgetDatastore());

  // It's possible that AbortOperations was called before the database actor
  // was created and became live. Let the child know that the database in no
  // longer valid.
  if (preparedDatastore->IsInvalidated()) {
    database->RequestAllowToClose();
  }

  return true;
}

bool
DeallocPBackgroundLSDatabaseParent(PBackgroundLSDatabaseParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<Database> actor = dont_AddRef(static_cast<Database*>(aActor));

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

  RefPtr<LSRequestBase> actor;

  switch (aParams.type()) {
    case LSRequestParams::TLSRequestPrepareDatastoreParams: {
      bool isOtherProcess =
        BackgroundParent::IsOtherProcessActor(aBackgroundActor);

      const LSRequestPrepareDatastoreParams& params =
        aParams.get_LSRequestPrepareDatastoreParams();

      const PrincipalOrQuotaInfo& info = params.info();

      PrincipalOrQuotaInfo::Type infoType = info.type();

      bool paramsOk =
        (isOtherProcess && infoType == PrincipalOrQuotaInfo::TPrincipalInfo) ||
        (!isOtherProcess && infoType == PrincipalOrQuotaInfo::TQuotaInfo);

      if (NS_WARN_IF(!paramsOk)) {
        ASSERT_UNLESS_FUZZING();
        return nullptr;
      }

      RefPtr<PrepareDatastoreOp> prepareDatastoreOp =
        new PrepareDatastoreOp(aParams);

      if (!gPrepareDatastoreOps) {
        gPrepareDatastoreOps = new PrepareDatastoreOpArray();
      }
      gPrepareDatastoreOps->AppendElement(prepareDatastoreOp);

      actor = std::move(prepareDatastoreOp);

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
 * Datastore
 ******************************************************************************/

Datastore::Datastore(const nsACString& aOrigin,
                     already_AddRefed<DirectoryLock>&& aDirectoryLock)
  : mDirectoryLock(std::move(aDirectoryLock))
  , mOrigin(aOrigin)
  , mClosed(false)
{
  AssertIsOnBackgroundThread();
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

  mDirectoryLock = nullptr;

  MOZ_ASSERT(gDatastores);
  MOZ_ASSERT(gDatastores->Get(mOrigin));
  gDatastores->Remove(mOrigin);

  if (!gDatastores->Count()) {
    gDatastores = nullptr;
  }
}

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

  if (!mDatabases.Count()) {
    Close();
  }
}

bool
Datastore::HasLiveDatabases() const
{
  AssertIsOnBackgroundThread();

  return mDatabases.Count();
}

uint32_t
Datastore::GetLength() const
{
  AssertIsOnBackgroundThread();

  return mValues.Count();
}

void
Datastore::GetKey(uint32_t aIndex, nsString& aKey) const
{
  AssertIsOnBackgroundThread();

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

  if (!mValues.Get(aKey, &aValue)) {
    aValue.SetIsVoid(true);
  }
}

void
Datastore::SetItem(const nsString& aKey, const nsString& aValue)
{
  AssertIsOnBackgroundThread();

  mValues.Put(aKey, aValue);
}

void
Datastore::RemoveItem(const nsString& aKey)
{
  AssertIsOnBackgroundThread();

  mValues.Remove(aKey);
}

void
Datastore::Clear()
{
  AssertIsOnBackgroundThread();

  mValues.Clear();
}

void
Datastore::GetKeys(nsTArray<nsString>& aKeys) const
{
  AssertIsOnBackgroundThread();

  for (auto iter = mValues.ConstIter(); !iter.Done(); iter.Next()) {
    aKeys.AppendElement(iter.Key());
  }
}

/*******************************************************************************
 * Database
 ******************************************************************************/

Database::Database(const nsACString& aOrigin)
  : mOrigin(aOrigin)
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
Database::SetActorAlive(already_AddRefed<Datastore>&& aDatastore)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mActorWasAlive);
  MOZ_ASSERT(!mActorDestroyed);

#ifdef DEBUG
  mActorWasAlive = true;
#endif

  mDatastore = std::move(aDatastore);

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
Database::RecvSetItem(const nsString& aKey, const nsString& aValue)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mDatastore);

  if (NS_WARN_IF(mAllowedToClose)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  mDatastore->SetItem(aKey, aValue);

  return IPC_OK();
}

mozilla::ipc::IPCResult
Database::RecvRemoveItem(const nsString& aKey)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mDatastore);

  if (NS_WARN_IF(mAllowedToClose)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  mDatastore->RemoveItem(aKey);

  return IPC_OK();
}

mozilla::ipc::IPCResult
Database::RecvClear()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mDatastore);

  if (NS_WARN_IF(mAllowedToClose)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  mDatastore->Clear();

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
 * LSRequestBase
 ******************************************************************************/

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

/*******************************************************************************
 * PrepareDatastoreOp
 ******************************************************************************/

PrepareDatastoreOp::PrepareDatastoreOp(const LSRequestParams& aParams)
  : mParams(aParams.get_LSRequestPrepareDatastoreParams())
  , mState(State::Initial)
  , mRequestedDirectoryLock(false)
  , mInvalidated(false)
{
  MOZ_ASSERT(aParams.type() ==
               LSRequestParams::TLSRequestPrepareDatastoreParams);
}

PrepareDatastoreOp::~PrepareDatastoreOp()
{
  MOZ_ASSERT(!mDirectoryLock);
  MOZ_ASSERT_IF(MayProceedOnNonOwningThread(),
                mState == State::Initial || mState == State::Completed);
}

void
PrepareDatastoreOp::Dispatch()
{
  AssertIsOnOwningThread();

  const PrincipalOrQuotaInfo& info = mParams.info();

  if (info.type() == PrincipalOrQuotaInfo::TPrincipalInfo) {
    mState = State::OpeningOnMainThread;
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(this));
  } else {
    MOZ_ASSERT(info.type() == PrincipalOrQuotaInfo::TQuotaInfo);

    mState = State::OpeningOnOwningThread;
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(this));
  }
}

nsresult
PrepareDatastoreOp::OpenOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State::OpeningOnMainThread);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      !MayProceedOnNonOwningThread()) {
    return NS_ERROR_FAILURE;
  }

  const PrincipalOrQuotaInfo& info = mParams.info();

  MOZ_ASSERT(info.type() == PrincipalOrQuotaInfo::TPrincipalInfo);

  const PrincipalInfo& principalInfo = info.get_PrincipalInfo();

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
  }

  // This service has to be started on the main thread currently.
  nsCOMPtr<mozIStorageService> ss;
  if (NS_WARN_IF(!(ss = do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID)))) {
    return NS_ERROR_FAILURE;
  }

  mState = State::FinishOpen;
  MOZ_ALWAYS_SUCCEEDS(OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

nsresult
PrepareDatastoreOp::OpenOnOwningThread()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::OpeningOnOwningThread);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !MayProceed()) {
    return NS_ERROR_FAILURE;
  }

  const PrincipalOrQuotaInfo& info = mParams.info();

  MOZ_ASSERT(info.type() == PrincipalOrQuotaInfo::TQuotaInfo);

  const QuotaInfo& quotaInfo = info.get_QuotaInfo();

  mSuffix = quotaInfo.suffix();
  mGroup = quotaInfo.group();
  mOrigin = quotaInfo.origin();

  mState = State::FinishOpen;
  MOZ_ALWAYS_SUCCEEDS(OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

nsresult
PrepareDatastoreOp::FinishOpen()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::FinishOpen);
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
  if (mParams.info().type() == PrincipalOrQuotaInfo::TPrincipalInfo) {
    mOrigin = mMainThreadOrigin;
  }

  MOZ_ASSERT(!mOrigin.IsEmpty());

  mState = State::PreparationPending;

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
  MOZ_ASSERT(mState == State::PreparationPending);

  if (gDatastores && (mDatastore = gDatastores->Get(mOrigin))) {
    mState = State::SendingReadyMessage;
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

  mState = State::QuotaManagerPending;
  QuotaManager::GetOrCreate(this);

  return NS_OK;
}

nsresult
PrepareDatastoreOp::QuotaManagerOpen()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::QuotaManagerPending);

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
  MOZ_ASSERT(mState == State::PreparationPending ||
             mState == State::QuotaManagerPending);
  MOZ_ASSERT(!mOrigin.IsEmpty());
  MOZ_ASSERT(!mDirectoryLock);
  MOZ_ASSERT(!QuotaClient::IsShuttingDownOnBackgroundThread());
  MOZ_ASSERT(QuotaManager::Get());

  mState = State::DirectoryOpenPending;
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
  MOZ_ASSERT(mState == State::DirectoryOpenPending);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnBackgroundThread()) ||
      !MayProceed()) {
    return NS_ERROR_FAILURE;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  // Must set this before dispatching otherwise we will race with the IO thread.
  mState = State::DatabaseWorkOpen;

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
  MOZ_ASSERT(mState == State::DatabaseWorkOpen);

  if (NS_WARN_IF(QuotaClient::IsShuttingDownOnNonBackgroundThread()) ||
      !MayProceedOnNonOwningThread()) {
    return NS_ERROR_FAILURE;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  nsCOMPtr<nsIFile> dbDirectory;
  nsresult rv =
    quotaManager->EnsureOriginIsInitialized(PERSISTENCE_TYPE_DEFAULT,
                                            mSuffix,
                                            mGroup,
                                            mOrigin,
                                            getter_AddRefs(dbDirectory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = dbDirectory->Append(NS_LITERAL_STRING(LS_DIRECTORY_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = dbDirectory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    rv = dbDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
#ifdef DEBUG
  else {
    bool isDirectory;
    MOZ_ASSERT(NS_SUCCEEDED(dbDirectory->IsDirectory(&isDirectory)));
    MOZ_ASSERT(isDirectory);
  }
#endif

  nsCOMPtr<nsIFile> dbFile;
  rv = dbDirectory->Clone(getter_AddRefs(dbFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = dbFile->Append(NS_LITERAL_STRING(DATA_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = CreateStorageConnection(dbFile, mOrigin, getter_AddRefs(connection));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = VerifyDatabaseInformation(connection);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Must set mState before dispatching otherwise we will race with the owning
  // thread.
  mState = State::SendingReadyMessage;

  rv = OwningEventTarget()->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
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

void
PrepareDatastoreOp::SendReadyMessage()
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
PrepareDatastoreOp::SendResults()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::SendingResults);

  if (!MayProceed()) {
    MaybeSetFailureCode(NS_ERROR_FAILURE);
  } else {
    LSRequestResponse response;

    if (NS_SUCCEEDED(ResultCode())) {
      if (!mDatastore) {
        mDatastore = new Datastore(mOrigin, mDirectoryLock.forget());

        if (!gDatastores) {
          gDatastores = new DatastoreHashtable();
        }

        MOZ_ASSERT(!gDatastores->Get(mOrigin));
        gDatastores->Put(mOrigin, mDatastore);
      }

      uint64_t datastoreId = ++gLastDatastoreId;

      nsAutoPtr<PreparedDatastore> preparedDatastore(
        new PreparedDatastore(mDatastore, mOrigin));

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

      response = prepareDatastoreResponse;
    } else {
      response = ResultCode();
    }

    Unused <<
      PBackgroundLSRequestParent::Send__delete__(this, response);
  }

  Cleanup();

  mState = State::Completed;
}

void
PrepareDatastoreOp::Cleanup()
{
  AssertIsOnOwningThread();

  if (mDatastore) {
    MOZ_ASSERT(!mDirectoryLock);

    if (NS_FAILED(ResultCode())) {
      MOZ_ASSERT(!mDatastore->IsClosed());
      MOZ_ASSERT(!mDatastore->HasLiveDatabases());
      mDatastore->Close();
    }

    // Make sure to release the datastore on this thread.
    mDatastore = nullptr;
  } else if (mDirectoryLock) {
    // If we have a directory lock then the operation must have failed.
    MOZ_ASSERT(NS_FAILED(ResultCode()));

    mDirectoryLock = nullptr;
  }

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

NS_IMETHODIMP
PrepareDatastoreOp::Run()
{
  nsresult rv;

  switch (mState) {
    case State::OpeningOnMainThread:
      rv = OpenOnMainThread();
      break;

    case State::OpeningOnOwningThread:
      rv = OpenOnOwningThread();
      break;

    case State::FinishOpen:
      rv = FinishOpen();
      break;

    case State::PreparationPending:
      rv = BeginDatastorePreparation();
      break;

    case State::QuotaManagerPending:
      rv = QuotaManagerOpen();
      break;

    case State::DatabaseWorkOpen:
      rv = DatabaseWork();
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

mozilla::ipc::IPCResult
PrepareDatastoreOp::RecvFinish()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::WaitingForFinish);

  mState = State::SendingResults;
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(this));

  return IPC_OK();
}

void
PrepareDatastoreOp::DirectoryLockAcquired(DirectoryLock* aLock)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  mDirectoryLock = aLock;

  nsresult rv = SendToIOThread();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MaybeSetFailureCode(rv);

    // The caller holds a strong reference to us, no need for a self reference
    // before calling Run().

    mState = State::SendingReadyMessage;
    MOZ_ALWAYS_SUCCEEDS(Run());

    return;
  }
}

void
PrepareDatastoreOp::DirectoryLockFailed()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  MaybeSetFailureCode(NS_ERROR_FAILURE);

  // The caller holds a strong reference to us, no need for a self reference
  // before calling Run().

  mState = State::SendingReadyMessage;
  MOZ_ALWAYS_SUCCEEDS(Run());
}

/*******************************************************************************
 * QuotaClient
 ******************************************************************************/

QuotaClient* QuotaClient::sInstance = nullptr;

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

nsresult
QuotaClient::InitOrigin(PersistenceType aPersistenceType,
                        const nsACString& aGroup,
                        const nsACString& aOrigin,
                        const AtomicBool& aCanceled,
                        UsageInfo* aUsageInfo)
{
  AssertIsOnIOThread();

  if (!aUsageInfo) {
    return NS_OK;
  }

  return GetUsageForOrigin(aPersistenceType,
                           aGroup,
                           aOrigin,
                           aCanceled,
                           aUsageInfo);
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
    //       For now, don't add to origin usage.
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

void
QuotaClient::OnOriginClearCompleted(PersistenceType aPersistenceType,
                                    const nsACString& aOrigin)
{
  AssertIsOnIOThread();
}

void
QuotaClient::ReleaseIOThreadObjects()
{
  AssertIsOnIOThread();
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

  // There may be datastores that are only held alive by prepared datastores
  // (ones which have no live database actors). We need to explicitly close
  // them here.
  if (gDatastores) {
    for (auto iter = gDatastores->ConstIter(); !iter.Done(); iter.Next()) {
      Datastore* datastore = iter.Data();
      MOZ_ASSERT(datastore);

      if (!datastore->IsClosed() && !datastore->HasLiveDatabases()) {
        datastore->Close();
      }
    }
  }

  // If database actors haven't been created yet, don't do anything special.
  // We are shutting down and we can release prepared datastores immediatelly
  // since database actors will never be created for them.
  if (gPreparedDatastores) {
    gPreparedDatastores->Clear();
    gPreparedDatastores = nullptr;
  }

  if (gLiveDatabases) {
    for (Database* database : *gLiveDatabases) {
      database->RequestAllowToClose();
    }
  }

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() {
    // Don't have to check gPreparedDatastores since we nulled it out above.
    return !gPrepareDatastoreOps && !gDatastores && !gLiveDatabases;
  }));
}

} // namespace dom
} // namespace mozilla
