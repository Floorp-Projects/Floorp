/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StorageDBThread.h"

#include "StorageCommon.h"
#include "StorageDBUpdater.h"
#include "StorageUtils.h"
#include "LocalStorageCache.h"
#include "LocalStorageManager.h"

#include "nsComponentManagerUtils.h"
#include "nsDirectoryServiceUtils.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsThreadUtils.h"
#include "nsProxyRelease.h"
#include "mozStorageCID.h"
#include "mozStorageHelper.h"
#include "mozIStorageService.h"
#include "mozIStorageBindingParams.h"
#include "mozIStorageValueArray.h"
#include "mozIStorageFunction.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "nsIObserverService.h"
#include "nsThread.h"
#include "nsThreadManager.h"
#include "nsVariant.h"
#include "mozilla/EventQueue.h"
#include "mozilla/IOInterposer.h"
#include "mozilla/OriginAttributes.h"
#include "mozilla/ThreadEventQueue.h"
#include "mozilla/Services.h"
#include "mozilla/Tokenizer.h"
#include "GeckoProfiler.h"

// How long we collect write oprerations
// before they are flushed to the database
// In milliseconds.
#define FLUSHING_INTERVAL_MS 5000

// Write Ahead Log's maximum size is 512KB
#define MAX_WAL_SIZE_BYTES 512 * 1024

// Current version of the database schema
#define CURRENT_SCHEMA_VERSION 2

namespace mozilla {
namespace dom {

using namespace StorageUtils;

namespace {  // anon

StorageDBThread* sStorageThread[kPrivateBrowsingIdCount] = {nullptr, nullptr};

// False until we shut the storage thread down.
bool sStorageThreadDown[kPrivateBrowsingIdCount] = {false, false};

}  // namespace

// XXX Fix me!
#if 0
StorageDBBridge::StorageDBBridge()
{
}
#endif

class StorageDBThread::InitHelper final : public Runnable {
  nsCOMPtr<nsIEventTarget> mOwningThread;
  mozilla::Mutex mMutex;
  mozilla::CondVar mCondVar;
  nsString mProfilePath;
  nsresult mMainThreadResultCode;
  bool mWaiting;

 public:
  InitHelper()
      : Runnable("dom::StorageDBThread::InitHelper"),
        mOwningThread(GetCurrentEventTarget()),
        mMutex("InitHelper::mMutex"),
        mCondVar(mMutex, "InitHelper::mCondVar"),
        mMainThreadResultCode(NS_OK),
        mWaiting(true) {}

  // Because of the `sync Preload` IPC, we need to be able to synchronously
  // initialize, which includes consulting and initializing
  // some main-thread-only APIs. Bug 1386441 discusses improving this situation.
  nsresult SyncDispatchAndReturnProfilePath(nsAString& aProfilePath);

 private:
  ~InitHelper() override = default;

  nsresult RunOnMainThread();

  NS_DECL_NSIRUNNABLE
};

class StorageDBThread::NoteBackgroundThreadRunnable final : public Runnable {
  // Expected to be only 0 or 1.
  const uint32_t mPrivateBrowsingId;
  nsCOMPtr<nsIEventTarget> mOwningThread;

 public:
  explicit NoteBackgroundThreadRunnable(const uint32_t aPrivateBrowsingId)
      : Runnable("dom::StorageDBThread::NoteBackgroundThreadRunnable"),
        mPrivateBrowsingId(aPrivateBrowsingId),
        mOwningThread(GetCurrentEventTarget()) {
    MOZ_RELEASE_ASSERT(aPrivateBrowsingId < kPrivateBrowsingIdCount);
  }

 private:
  ~NoteBackgroundThreadRunnable() override = default;

  NS_DECL_NSIRUNNABLE
};

StorageDBThread::StorageDBThread(const uint32_t aPrivateBrowsingId)
    : mThread(nullptr),
      mThreadObserver(new ThreadObserver()),
      mStopIOThread(false),
      mWALModeEnabled(false),
      mDBReady(false),
      mStatus(NS_OK),
      mWorkerStatements(mWorkerConnection),
      mReaderStatements(mReaderConnection),
      mFlushImmediately(false),
      mPrivateBrowsingId(aPrivateBrowsingId),
      mPriorityCounter(0) {
  MOZ_RELEASE_ASSERT(aPrivateBrowsingId < kPrivateBrowsingIdCount);
}

// static
StorageDBThread* StorageDBThread::Get(const uint32_t aPrivateBrowsingId) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_RELEASE_ASSERT(aPrivateBrowsingId < kPrivateBrowsingIdCount);

  return sStorageThread[aPrivateBrowsingId];
}

// static
StorageDBThread* StorageDBThread::GetOrCreate(
    const nsString& aProfilePath, const uint32_t aPrivateBrowsingId) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_RELEASE_ASSERT(aPrivateBrowsingId < kPrivateBrowsingIdCount);

  StorageDBThread*& storageThread = sStorageThread[aPrivateBrowsingId];
  if (storageThread || sStorageThreadDown[aPrivateBrowsingId]) {
    // When sStorageThreadDown is at true, sStorageThread is null.
    // Checking sStorageThreadDown flag here prevents reinitialization of
    // the storage thread after shutdown.
    return storageThread;
  }

  auto newStorageThread = MakeUnique<StorageDBThread>(aPrivateBrowsingId);

  nsresult rv = newStorageThread->Init(aProfilePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  storageThread = newStorageThread.release();

  return storageThread;
}

// static
nsresult StorageDBThread::GetProfilePath(nsString& aProfilePath) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  // Need to determine location on the main thread, since
  // NS_GetSpecialDirectory accesses the atom table that can
  // only be accessed on the main thread.
  nsCOMPtr<nsIFile> profileDir;
  nsresult rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                       getter_AddRefs(profileDir));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = profileDir->GetPath(aProfilePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // This service has to be started on the main thread currently.
  nsCOMPtr<mozIStorageService> ss =
      do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult StorageDBThread::Init(const nsString& aProfilePath) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  if (mPrivateBrowsingId == 0) {
    nsresult rv;

    nsString profilePath;
    if (aProfilePath.IsEmpty()) {
      RefPtr<InitHelper> helper = new InitHelper();

      rv = helper->SyncDispatchAndReturnProfilePath(profilePath);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      profilePath = aProfilePath;
    }

    mDatabaseFile = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = mDatabaseFile->InitWithPath(profilePath);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = mDatabaseFile->Append(u"webappsstore.sqlite"_ns);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Need to keep the lock to avoid setting mThread later then
  // the thread body executes.
  MonitorAutoLock monitor(mThreadObserver->GetMonitor());

  mThread = PR_CreateThread(PR_USER_THREAD, &StorageDBThread::ThreadFunc, this,
                            PR_PRIORITY_LOW, PR_GLOBAL_THREAD,
                            PR_JOINABLE_THREAD, 262144);
  if (!mThread) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  RefPtr<NoteBackgroundThreadRunnable> runnable =
      new NoteBackgroundThreadRunnable(mPrivateBrowsingId);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(runnable));

  return NS_OK;
}

nsresult StorageDBThread::Shutdown() {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  if (!mThread) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  Telemetry::AutoTimer<Telemetry::LOCALDOMSTORAGE_SHUTDOWN_DATABASE_MS> timer;

  {
    MonitorAutoLock monitor(mThreadObserver->GetMonitor());

    // After we stop, no other operations can be accepted
    mFlushImmediately = true;
    mStopIOThread = true;
    monitor.Notify();
  }

  PR_JoinThread(mThread);
  mThread = nullptr;

  return mStatus;
}

void StorageDBThread::SyncPreload(LocalStorageCacheBridge* aCache,
                                  bool aForceSync) {
  AUTO_PROFILER_LABEL("StorageDBThread::SyncPreload", OTHER);
  if (!aForceSync && aCache->LoadedCount()) {
    // Preload already started for this cache, just wait for it to finish.
    // LoadWait will exit after LoadDone on the cache has been called.
    SetHigherPriority();
    aCache->LoadWait();
    SetDefaultPriority();
    return;
  }

  // Bypass sync load when an update is pending in the queue to write, we would
  // get incosistent data in the cache.  Also don't allow sync main-thread
  // preload when DB open and init is still pending on the background thread.
  if (mDBReady && mWALModeEnabled) {
    bool pendingTasks;
    {
      MonitorAutoLock monitor(mThreadObserver->GetMonitor());
      pendingTasks = mPendingTasks.IsOriginUpdatePending(
                         aCache->OriginSuffix(), aCache->OriginNoSuffix()) ||
                     mPendingTasks.IsOriginClearPending(
                         aCache->OriginSuffix(), aCache->OriginNoSuffix());
    }

    if (!pendingTasks) {
      // WAL is enabled, thus do the load synchronously on the main thread.
      DBOperation preload(DBOperation::opPreload, aCache);
      preload.PerformAndFinalize(this);
      return;
    }
  }

  // Need to go asynchronously since WAL is not allowed or scheduled updates
  // need to be flushed first.
  // Schedule preload for this cache as the first operation.
  nsresult rv =
      InsertDBOp(MakeUnique<DBOperation>(DBOperation::opPreloadUrgent, aCache));

  // LoadWait exits after LoadDone of the cache has been called.
  if (NS_SUCCEEDED(rv)) {
    aCache->LoadWait();
  }
}

void StorageDBThread::AsyncFlush() {
  MonitorAutoLock monitor(mThreadObserver->GetMonitor());
  mFlushImmediately = true;
  monitor.Notify();
}

bool StorageDBThread::ShouldPreloadOrigin(const nsACString& aOrigin) {
  MonitorAutoLock monitor(mThreadObserver->GetMonitor());
  return mOriginsHavingData.Contains(aOrigin);
}

void StorageDBThread::GetOriginsHavingData(nsTArray<nsCString>* aOrigins) {
  MonitorAutoLock monitor(mThreadObserver->GetMonitor());
  AppendToArray(*aOrigins, mOriginsHavingData);
}

nsresult StorageDBThread::InsertDBOp(
    UniquePtr<StorageDBThread::DBOperation> aOperation) {
  MonitorAutoLock monitor(mThreadObserver->GetMonitor());

  if (NS_FAILED(mStatus)) {
    MonitorAutoUnlock unlock(mThreadObserver->GetMonitor());
    aOperation->Finalize(mStatus);
    return mStatus;
  }

  if (mStopIOThread) {
    // Thread use after shutdown demanded.
    MOZ_ASSERT(false);
    return NS_ERROR_NOT_INITIALIZED;
  }

  switch (aOperation->Type()) {
    case DBOperation::opPreload:
    case DBOperation::opPreloadUrgent:
      if (mPendingTasks.IsOriginUpdatePending(aOperation->OriginSuffix(),
                                              aOperation->OriginNoSuffix())) {
        // If there is a pending update operation for the scope first do the
        // flush before we preload the cache.  This may happen in an extremely
        // rare case when a child process throws away its cache before flush on
        // the parent has finished.  If we would preloaded the cache as a
        // priority operation before the pending flush, we would have got an
        // inconsistent cache content.
        mFlushImmediately = true;
      } else if (mPendingTasks.IsOriginClearPending(
                     aOperation->OriginSuffix(),
                     aOperation->OriginNoSuffix())) {
        // The scope is scheduled to be cleared, so just quickly load as empty.
        // We need to do this to prevent load of the DB data before the scope
        // has actually been cleared from the database.  Preloads are processed
        // immediately before update and clear operations on the database that
        // are flushed periodically in batches.
        MonitorAutoUnlock unlock(mThreadObserver->GetMonitor());
        aOperation->Finalize(NS_OK);
        return NS_OK;
      }
      [[fallthrough]];

    case DBOperation::opGetUsage:
      if (aOperation->Type() == DBOperation::opPreloadUrgent) {
        SetHigherPriority();  // Dropped back after urgent preload execution
        mPreloads.InsertElementAt(0, aOperation.release());
      } else {
        mPreloads.AppendElement(aOperation.release());
      }

      // Immediately start executing this.
      monitor.Notify();
      break;

    default:
      // Update operations are first collected, coalesced and then flushed
      // after a short time.
      mPendingTasks.Add(std::move(aOperation));

      ScheduleFlush();
      break;
  }

  return NS_OK;
}

void StorageDBThread::SetHigherPriority() {
  ++mPriorityCounter;
  PR_SetThreadPriority(mThread, PR_PRIORITY_URGENT);
}

void StorageDBThread::SetDefaultPriority() {
  if (--mPriorityCounter <= 0) {
    PR_SetThreadPriority(mThread, PR_PRIORITY_LOW);
  }
}

void StorageDBThread::ThreadFunc(void* aArg) {
  {
    auto queue = MakeRefPtr<ThreadEventQueue>(MakeUnique<EventQueue>());
    Unused << nsThreadManager::get().CreateCurrentThread(
        queue, nsThread::NOT_MAIN_THREAD);
  }

  AUTO_PROFILER_REGISTER_THREAD("localStorage DB");
  NS_SetCurrentThreadName("localStorage DB");
  mozilla::IOInterposer::RegisterCurrentThread();

  StorageDBThread* thread = static_cast<StorageDBThread*>(aArg);
  thread->ThreadFunc();
  mozilla::IOInterposer::UnregisterCurrentThread();
}

void StorageDBThread::ThreadFunc() {
  nsresult rv = InitDatabase();

  MonitorAutoLock lockMonitor(mThreadObserver->GetMonitor());

  if (NS_FAILED(rv)) {
    mStatus = rv;
    mStopIOThread = true;
    return;
  }

  // Create an nsIThread for the current PRThread, so we can observe runnables
  // dispatched to it.
  nsCOMPtr<nsIThread> thread = NS_GetCurrentThread();
  nsCOMPtr<nsIThreadInternal> threadInternal = do_QueryInterface(thread);
  MOZ_ASSERT(threadInternal);  // Should always succeed.
  threadInternal->SetObserver(mThreadObserver);

  while (MOZ_LIKELY(!mStopIOThread || mPreloads.Length() ||
                    mPendingTasks.HasTasks() ||
                    mThreadObserver->HasPendingEvents())) {
    // Process xpcom events first.
    while (MOZ_UNLIKELY(mThreadObserver->HasPendingEvents())) {
      mThreadObserver->ClearPendingEvents();
      MonitorAutoUnlock unlock(mThreadObserver->GetMonitor());
      bool processedEvent;
      do {
        rv = thread->ProcessNextEvent(false, &processedEvent);
      } while (NS_SUCCEEDED(rv) && processedEvent);
    }

    TimeDuration timeUntilFlush = TimeUntilFlush();
    if (MOZ_UNLIKELY(timeUntilFlush.IsZero())) {
      // Flush time is up or flush has been forced, do it now.
      UnscheduleFlush();
      if (mPendingTasks.Prepare()) {
        {
          MonitorAutoUnlock unlockMonitor(mThreadObserver->GetMonitor());
          rv = mPendingTasks.Execute(this);
        }

        if (!mPendingTasks.Finalize(rv)) {
          mStatus = rv;
          NS_WARNING("localStorage DB access broken");
        }
      }
      NotifyFlushCompletion();
    } else if (MOZ_LIKELY(mPreloads.Length())) {
      UniquePtr<DBOperation> op(mPreloads[0]);
      mPreloads.RemoveElementAt(0);
      {
        MonitorAutoUnlock unlockMonitor(mThreadObserver->GetMonitor());
        op->PerformAndFinalize(this);
      }

      if (op->Type() == DBOperation::opPreloadUrgent) {
        SetDefaultPriority();  // urgent preload unscheduled
      }
    } else if (MOZ_UNLIKELY(!mStopIOThread)) {
      AUTO_PROFILER_LABEL("StorageDBThread::ThreadFunc::Wait", IDLE);
      lockMonitor.Wait(timeUntilFlush);
    }
  }  // thread loop

  mStatus = ShutdownDatabase();

  if (threadInternal) {
    threadInternal->SetObserver(nullptr);
  }
}

NS_IMPL_ISUPPORTS(StorageDBThread::ThreadObserver, nsIThreadObserver)

NS_IMETHODIMP
StorageDBThread::ThreadObserver::OnDispatchedEvent() {
  MonitorAutoLock lock(mMonitor);
  mHasPendingEvents = true;
  lock.Notify();
  return NS_OK;
}

NS_IMETHODIMP
StorageDBThread::ThreadObserver::OnProcessNextEvent(nsIThreadInternal* aThread,
                                                    bool mayWait) {
  return NS_OK;
}

NS_IMETHODIMP
StorageDBThread::ThreadObserver::AfterProcessNextEvent(
    nsIThreadInternal* aThread, bool eventWasProcessed) {
  return NS_OK;
}

nsresult StorageDBThread::OpenDatabaseConnection() {
  nsresult rv;

  MOZ_ASSERT(!NS_IsMainThread());

  nsCOMPtr<mozIStorageService> service =
      do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mPrivateBrowsingId == 0) {
    MOZ_ASSERT(mDatabaseFile);

    rv = service->OpenUnsharedDatabase(mDatabaseFile,
                                       getter_AddRefs(mWorkerConnection));
    if (rv == NS_ERROR_FILE_CORRUPTED) {
      // delete the db and try opening again
      rv = mDatabaseFile->Remove(false);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = service->OpenUnsharedDatabase(mDatabaseFile,
                                         getter_AddRefs(mWorkerConnection));
    }
  } else {
    MOZ_ASSERT(mPrivateBrowsingId == 1);

    rv = service->OpenSpecialDatabase(kMozStorageMemoryStorageKey,
                                      "lsprivatedb"_ns,
                                      getter_AddRefs(mWorkerConnection));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult StorageDBThread::OpenAndUpdateDatabase() {
  nsresult rv;

  // Here we are on the worker thread. This opens the worker connection.
  MOZ_ASSERT(!NS_IsMainThread());

  rv = OpenDatabaseConnection();
  NS_ENSURE_SUCCESS(rv, rv);

  // SQLite doesn't support WAL journals for in-memory databases.
  if (mPrivateBrowsingId == 0) {
    rv = TryJournalMode();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult StorageDBThread::InitDatabase() {
  nsresult rv;

  // Here we are on the worker thread. This opens the worker connection.
  MOZ_ASSERT(!NS_IsMainThread());

  rv = OpenAndUpdateDatabase();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = StorageDBUpdater::Update(mWorkerConnection);
  if (NS_FAILED(rv)) {
    if (mPrivateBrowsingId == 0) {
      // Update has failed, rather throw the database away and try
      // opening and setting it up again.
      rv = mWorkerConnection->Close();
      mWorkerConnection = nullptr;
      NS_ENSURE_SUCCESS(rv, rv);

      rv = mDatabaseFile->Remove(false);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = OpenAndUpdateDatabase();
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Create a read-only clone
  (void)mWorkerConnection->Clone(true, getter_AddRefs(mReaderConnection));
  NS_ENSURE_TRUE(mReaderConnection, NS_ERROR_FAILURE);

  // Database open and all initiation operation are done.  Switching this flag
  // to true allow main thread to read directly from the database.  If we would
  // allow this sooner, we would have opened a window where main thread read
  // might operate on a totally broken and incosistent database.
  mDBReady = true;

  // List scopes having any stored data
  nsCOMPtr<mozIStorageStatement> stmt;
  // Note: result of this select must match StorageManager::CreateOrigin()
  rv = mWorkerConnection->CreateStatement(
      nsLiteralCString("SELECT DISTINCT originAttributes || ':' || originKey "
                       "FROM webappsstore2"),
      getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  mozStorageStatementScoper scope(stmt);

  bool exists;
  while (NS_SUCCEEDED(rv = stmt->ExecuteStep(&exists)) && exists) {
    nsAutoCString foundOrigin;
    rv = stmt->GetUTF8String(0, foundOrigin);
    NS_ENSURE_SUCCESS(rv, rv);

    MonitorAutoLock monitor(mThreadObserver->GetMonitor());
    mOriginsHavingData.Insert(foundOrigin);
  }

  return NS_OK;
}

nsresult StorageDBThread::SetJournalMode(bool aIsWal) {
  nsresult rv;

  nsAutoCString stmtString(MOZ_STORAGE_UNIQUIFY_QUERY_STR
                           "PRAGMA journal_mode = ");
  if (aIsWal) {
    stmtString.AppendLiteral("wal");
  } else {
    stmtString.AppendLiteral("truncate");
  }

  nsCOMPtr<mozIStorageStatement> stmt;
  rv = mWorkerConnection->CreateStatement(stmtString, getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);
  mozStorageStatementScoper scope(stmt);

  bool hasResult = false;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!hasResult) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString journalMode;
  rv = stmt->GetUTF8String(0, journalMode);
  NS_ENSURE_SUCCESS(rv, rv);
  if ((aIsWal && !journalMode.EqualsLiteral("wal")) ||
      (!aIsWal && !journalMode.EqualsLiteral("truncate"))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult StorageDBThread::TryJournalMode() {
  nsresult rv;

  rv = SetJournalMode(true);
  if (NS_FAILED(rv)) {
    mWALModeEnabled = false;

    rv = SetJournalMode(false);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    mWALModeEnabled = true;

    rv = ConfigureWALBehavior();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult StorageDBThread::ConfigureWALBehavior() {
  // Get the DB's page size
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = mWorkerConnection->CreateStatement(
      nsLiteralCString(MOZ_STORAGE_UNIQUIFY_QUERY_STR "PRAGMA page_size"),
      getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasResult = false;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && hasResult, NS_ERROR_FAILURE);

  int32_t pageSize = 0;
  rv = stmt->GetInt32(0, &pageSize);
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && pageSize > 0, NS_ERROR_UNEXPECTED);

  // Set the threshold for auto-checkpointing the WAL.
  // We don't want giant logs slowing down reads & shutdown.
  int32_t thresholdInPages =
      static_cast<int32_t>(MAX_WAL_SIZE_BYTES / pageSize);
  nsAutoCString thresholdPragma("PRAGMA wal_autocheckpoint = ");
  thresholdPragma.AppendInt(thresholdInPages);
  rv = mWorkerConnection->ExecuteSimpleSQL(thresholdPragma);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set the maximum WAL log size to reduce footprint on mobile (large empty
  // WAL files will be truncated)
  nsAutoCString journalSizePragma("PRAGMA journal_size_limit = ");
  // bug 600307: mak recommends setting this to 3 times the auto-checkpoint
  // threshold
  journalSizePragma.AppendInt(MAX_WAL_SIZE_BYTES * 3);
  rv = mWorkerConnection->ExecuteSimpleSQL(journalSizePragma);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult StorageDBThread::ShutdownDatabase() {
  // Has to be called on the worker thread.
  MOZ_ASSERT(!NS_IsMainThread());

  nsresult rv = mStatus;

  mDBReady = false;

  // Finalize the cached statements.
  mReaderStatements.FinalizeStatements();
  mWorkerStatements.FinalizeStatements();

  if (mReaderConnection) {
    // No need to sync access to mReaderConnection since the main thread
    // is right now joining this thread, unable to execute any events.
    mReaderConnection->Close();
    mReaderConnection = nullptr;
  }

  if (mWorkerConnection) {
    rv = mWorkerConnection->Close();
    mWorkerConnection = nullptr;
  }

  return rv;
}

void StorageDBThread::ScheduleFlush() {
  if (mDirtyEpoch) {
    return;  // Already scheduled
  }

  // Must be non-zero to indicate we are scheduled
  mDirtyEpoch = TimeStamp::Now();

  // Wake the monitor from indefinite sleep...
  (mThreadObserver->GetMonitor()).Notify();
}

void StorageDBThread::UnscheduleFlush() {
  // We are just about to do the flush, drop flags
  mFlushImmediately = false;
  mDirtyEpoch = TimeStamp();
}

TimeDuration StorageDBThread::TimeUntilFlush() {
  if (mFlushImmediately) {
    return 0;  // Do it now regardless the timeout.
  }

  if (!mDirtyEpoch) {
    return TimeDuration::Forever();  // No pending task...
  }

  TimeStamp now = TimeStamp::Now();
  TimeDuration age = now - mDirtyEpoch;
  static const TimeDuration kMaxAge =
      TimeDuration::FromMilliseconds(FLUSHING_INTERVAL_MS);
  if (age > kMaxAge) {
    return 0;  // It is time.
  }

  return kMaxAge - age;  // Time left. This is used to sleep the monitor.
}

void StorageDBThread::NotifyFlushCompletion() {
#ifdef DOM_STORAGE_TESTS
  if (!NS_IsMainThread()) {
    RefPtr<nsRunnableMethod<StorageDBThread, void, false>> event =
        NewNonOwningRunnableMethod(
            "dom::StorageDBThread::NotifyFlushCompletion", this,
            &StorageDBThread::NotifyFlushCompletion);
    NS_DispatchToMainThread(event);
    return;
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr, "domstorage-test-flushed", nullptr);
  }
#endif
}

// Helper SQL function classes

namespace {

class OriginAttrsPatternMatchSQLFunction final : public mozIStorageFunction {
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION

  explicit OriginAttrsPatternMatchSQLFunction(
      OriginAttributesPattern const& aPattern)
      : mPattern(aPattern) {}

 private:
  OriginAttrsPatternMatchSQLFunction() = delete;
  ~OriginAttrsPatternMatchSQLFunction() = default;

  OriginAttributesPattern mPattern;
};

NS_IMPL_ISUPPORTS(OriginAttrsPatternMatchSQLFunction, mozIStorageFunction)

NS_IMETHODIMP
OriginAttrsPatternMatchSQLFunction::OnFunctionCall(
    mozIStorageValueArray* aFunctionArguments, nsIVariant** aResult) {
  nsresult rv;

  nsAutoCString suffix;
  rv = aFunctionArguments->GetUTF8String(0, suffix);
  NS_ENSURE_SUCCESS(rv, rv);

  OriginAttributes oa;
  bool success = oa.PopulateFromSuffix(suffix);
  NS_ENSURE_TRUE(success, NS_ERROR_FAILURE);
  bool result = mPattern.Matches(oa);

  RefPtr<nsVariant> outVar(new nsVariant());
  rv = outVar->SetAsBool(result);
  NS_ENSURE_SUCCESS(rv, rv);

  outVar.forget(aResult);
  return NS_OK;
}

}  // namespace

// StorageDBThread::DBOperation

StorageDBThread::DBOperation::DBOperation(const OperationType aType,
                                          LocalStorageCacheBridge* aCache,
                                          const nsAString& aKey,
                                          const nsAString& aValue)
    : mType(aType), mCache(aCache), mKey(aKey), mValue(aValue) {
  MOZ_ASSERT(mType == opPreload || mType == opPreloadUrgent ||
             mType == opAddItem || mType == opUpdateItem ||
             mType == opRemoveItem || mType == opClear || mType == opClearAll);
  MOZ_COUNT_CTOR(StorageDBThread::DBOperation);
}

StorageDBThread::DBOperation::DBOperation(const OperationType aType,
                                          StorageUsageBridge* aUsage)
    : mType(aType), mUsage(aUsage) {
  MOZ_ASSERT(mType == opGetUsage);
  MOZ_COUNT_CTOR(StorageDBThread::DBOperation);
}

StorageDBThread::DBOperation::DBOperation(const OperationType aType,
                                          const nsACString& aOriginNoSuffix)
    : mType(aType), mCache(nullptr), mOrigin(aOriginNoSuffix) {
  MOZ_ASSERT(mType == opClearMatchingOrigin);
  MOZ_COUNT_CTOR(StorageDBThread::DBOperation);
}

StorageDBThread::DBOperation::DBOperation(
    const OperationType aType, const OriginAttributesPattern& aOriginNoSuffix)
    : mType(aType), mCache(nullptr), mOriginPattern(aOriginNoSuffix) {
  MOZ_ASSERT(mType == opClearMatchingOriginAttributes);
  MOZ_COUNT_CTOR(StorageDBThread::DBOperation);
}

StorageDBThread::DBOperation::~DBOperation() {
  MOZ_COUNT_DTOR(StorageDBThread::DBOperation);
}

const nsCString StorageDBThread::DBOperation::OriginNoSuffix() const {
  if (mCache) {
    return mCache->OriginNoSuffix();
  }

  return ""_ns;
}

const nsCString StorageDBThread::DBOperation::OriginSuffix() const {
  if (mCache) {
    return mCache->OriginSuffix();
  }

  return ""_ns;
}

const nsCString StorageDBThread::DBOperation::Origin() const {
  if (mCache) {
    return mCache->Origin();
  }

  return mOrigin;
}

const nsCString StorageDBThread::DBOperation::Target() const {
  switch (mType) {
    case opAddItem:
    case opUpdateItem:
    case opRemoveItem:
      return Origin() + "|"_ns + NS_ConvertUTF16toUTF8(mKey);

    default:
      return Origin();
  }
}

void StorageDBThread::DBOperation::PerformAndFinalize(
    StorageDBThread* aThread) {
  Finalize(Perform(aThread));
}

nsresult StorageDBThread::DBOperation::Perform(StorageDBThread* aThread) {
  nsresult rv;

  switch (mType) {
    case opPreload:
    case opPreloadUrgent: {
      // Already loaded?
      if (mCache->Loaded()) {
        break;
      }

      StatementCache* statements;
      if (MOZ_UNLIKELY(::mozilla::ipc::IsOnBackgroundThread())) {
        statements = &aThread->mReaderStatements;
      } else {
        statements = &aThread->mWorkerStatements;
      }

      // OFFSET is an optimization when we have to do a sync load
      // and cache has already loaded some parts asynchronously.
      // It skips keys we have already loaded.
      nsCOMPtr<mozIStorageStatement> stmt = statements->GetCachedStatement(
          "SELECT key, value FROM webappsstore2 "
          "WHERE originAttributes = :originAttributes AND originKey = "
          ":originKey "
          "ORDER BY key LIMIT -1 OFFSET :offset");
      NS_ENSURE_STATE(stmt);
      mozStorageStatementScoper scope(stmt);

      rv = stmt->BindUTF8StringByName("originAttributes"_ns,
                                      mCache->OriginSuffix());
      NS_ENSURE_SUCCESS(rv, rv);

      rv = stmt->BindUTF8StringByName("originKey"_ns, mCache->OriginNoSuffix());
      NS_ENSURE_SUCCESS(rv, rv);

      rv = stmt->BindInt32ByName("offset"_ns,
                                 static_cast<int32_t>(mCache->LoadedCount()));
      NS_ENSURE_SUCCESS(rv, rv);

      bool exists;
      while (NS_SUCCEEDED(rv = stmt->ExecuteStep(&exists)) && exists) {
        nsAutoString key;
        rv = stmt->GetString(0, key);
        NS_ENSURE_SUCCESS(rv, rv);

        nsAutoString value;
        rv = stmt->GetString(1, value);
        NS_ENSURE_SUCCESS(rv, rv);

        if (!mCache->LoadItem(key, value)) {
          break;
        }
      }
      // The loop condition's call to ExecuteStep() may have terminated because
      // !NS_SUCCEEDED(), we need an early return to cover that case.  This also
      // covers success cases as well, but that's inductively safe.
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    }

    case opGetUsage: {
      // Bug 1676410 fixed a regression caused by bug 1165214. However, it
      // turns out that 100% correct checking of the eTLD+1 usage is not
      // possible to recover easily, see bug 1683299.
#if 0
      // This is how it should be done, but due to other problems like lack
      // of usage synchronization between content processes, we temporarily
      // disabled the matching using "%".

      nsCOMPtr<mozIStorageStatement> stmt =
          aThread->mWorkerStatements.GetCachedStatement(
              "SELECT SUM(LENGTH(key) + LENGTH(value)) FROM webappsstore2 "
              "WHERE (originAttributes || ':' || originKey) LIKE :usageOrigin "
              "ESCAPE '\\'");
      NS_ENSURE_STATE(stmt);

      mozStorageStatementScoper scope(stmt);

      // The database schema is built around cleverly reversing domain names
      // (the "originKey") so that we can efficiently group usage by eTLD+1.
      // "foo.example.org" has an eTLD+1 of ".example.org". They reverse to
      // "gro.elpmaxe.oof" and "gro.elpmaxe." respectively, noting that the
      // reversed eTLD+1 is a prefix of its reversed sub-domain. To this end,
      // we can calculate all of the usage for an eTLD+1 by summing up all the
      // rows which have the reversed eTLD+1 as a prefix. In SQL we can
      // accomplish this using LIKE which provides for case-insensitive
      // matching with "_" as a single-character wildcard match and "%" any
      // sequence of zero or more characters. So by suffixing the reversed
      // eTLD+1 and using "%" we get our case-insensitive (domain names are
      // case-insensitive) matching. Note that although legal domain names
      // don't include "_" or "%", file origins can include them, so we need
      // to escape our OriginScope for correctness.
      nsAutoCString originScopeEscaped;
      rv = stmt->EscapeUTF8StringForLIKE(mUsage->OriginScope(), '\\',
                                         originScopeEscaped);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = stmt->BindUTF8StringByName("usageOrigin"_ns,
                                      originScopeEscaped + "%"_ns);
      NS_ENSURE_SUCCESS(rv, rv);
#else
      // This is the code before bug 1676410 and bug 1676973. The returned
      // usage will be zero in most of the cases, but due to lack of usage
      // synchronization between content processes we have to live with this
      // semi-broken behaviour because it causes less harm than the matching
      // using "%".

      nsCOMPtr<mozIStorageStatement> stmt =
          aThread->mWorkerStatements.GetCachedStatement(
              "SELECT SUM(LENGTH(key) + LENGTH(value)) FROM webappsstore2 "
              "WHERE (originAttributes || ':' || originKey) LIKE :usageOrigin");
      NS_ENSURE_STATE(stmt);

      mozStorageStatementScoper scope(stmt);

      rv = stmt->BindUTF8StringByName("usageOrigin"_ns, mUsage->OriginScope());
      NS_ENSURE_SUCCESS(rv, rv);
#endif

      bool exists;
      rv = stmt->ExecuteStep(&exists);
      NS_ENSURE_SUCCESS(rv, rv);

      int64_t usage = 0;
      if (exists) {
        rv = stmt->GetInt64(0, &usage);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      mUsage->LoadUsage(usage);
      break;
    }

    case opAddItem:
    case opUpdateItem: {
      MOZ_ASSERT(!NS_IsMainThread());

      nsCOMPtr<mozIStorageStatement> stmt =
          aThread->mWorkerStatements.GetCachedStatement(
              "INSERT OR REPLACE INTO webappsstore2 (originAttributes, "
              "originKey, scope, key, value) "
              "VALUES (:originAttributes, :originKey, :scope, :key, :value) ");
      NS_ENSURE_STATE(stmt);

      mozStorageStatementScoper scope(stmt);

      rv = stmt->BindUTF8StringByName("originAttributes"_ns,
                                      mCache->OriginSuffix());
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->BindUTF8StringByName("originKey"_ns, mCache->OriginNoSuffix());
      NS_ENSURE_SUCCESS(rv, rv);
      // Filling the 'scope' column just for downgrade compatibility reasons
      rv = stmt->BindUTF8StringByName(
          "scope"_ns,
          Scheme0Scope(mCache->OriginSuffix(), mCache->OriginNoSuffix()));
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->BindStringByName("key"_ns, mKey);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->BindStringByName("value"_ns, mValue);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = stmt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);

      MonitorAutoLock monitor(aThread->mThreadObserver->GetMonitor());
      aThread->mOriginsHavingData.Insert(Origin());
      break;
    }

    case opRemoveItem: {
      MOZ_ASSERT(!NS_IsMainThread());

      nsCOMPtr<mozIStorageStatement> stmt =
          aThread->mWorkerStatements.GetCachedStatement(
              "DELETE FROM webappsstore2 "
              "WHERE originAttributes = :originAttributes AND originKey = "
              ":originKey "
              "AND key = :key ");
      NS_ENSURE_STATE(stmt);
      mozStorageStatementScoper scope(stmt);

      rv = stmt->BindUTF8StringByName("originAttributes"_ns,
                                      mCache->OriginSuffix());
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->BindUTF8StringByName("originKey"_ns, mCache->OriginNoSuffix());
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->BindStringByName("key"_ns, mKey);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = stmt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);

      break;
    }

    case opClear: {
      MOZ_ASSERT(!NS_IsMainThread());

      nsCOMPtr<mozIStorageStatement> stmt =
          aThread->mWorkerStatements.GetCachedStatement(
              "DELETE FROM webappsstore2 "
              "WHERE originAttributes = :originAttributes AND originKey = "
              ":originKey");
      NS_ENSURE_STATE(stmt);
      mozStorageStatementScoper scope(stmt);

      rv = stmt->BindUTF8StringByName("originAttributes"_ns,
                                      mCache->OriginSuffix());
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->BindUTF8StringByName("originKey"_ns, mCache->OriginNoSuffix());
      NS_ENSURE_SUCCESS(rv, rv);

      rv = stmt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);

      MonitorAutoLock monitor(aThread->mThreadObserver->GetMonitor());
      aThread->mOriginsHavingData.Remove(Origin());
      break;
    }

    case opClearAll: {
      MOZ_ASSERT(!NS_IsMainThread());

      nsCOMPtr<mozIStorageStatement> stmt =
          aThread->mWorkerStatements.GetCachedStatement(
              "DELETE FROM webappsstore2");
      NS_ENSURE_STATE(stmt);
      mozStorageStatementScoper scope(stmt);

      rv = stmt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);

      MonitorAutoLock monitor(aThread->mThreadObserver->GetMonitor());
      aThread->mOriginsHavingData.Clear();
      break;
    }

    case opClearMatchingOrigin: {
      MOZ_ASSERT(!NS_IsMainThread());

      nsCOMPtr<mozIStorageStatement> stmt =
          aThread->mWorkerStatements.GetCachedStatement(
              "DELETE FROM webappsstore2"
              " WHERE originKey GLOB :scope");
      NS_ENSURE_STATE(stmt);
      mozStorageStatementScoper scope(stmt);

      rv = stmt->BindUTF8StringByName("scope"_ns, mOrigin + "*"_ns);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = stmt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);

      // No need to selectively clear mOriginsHavingData here.  That hashtable
      // only prevents preload for scopes with no data.  Leaving a false record
      // in it has a negligible effect on performance.
      break;
    }

    case opClearMatchingOriginAttributes: {
      MOZ_ASSERT(!NS_IsMainThread());

      // Register the ORIGIN_ATTRS_PATTERN_MATCH function, initialized with the
      // pattern
      nsCOMPtr<mozIStorageFunction> patternMatchFunction(
          new OriginAttrsPatternMatchSQLFunction(mOriginPattern));

      rv = aThread->mWorkerConnection->CreateFunction(
          "ORIGIN_ATTRS_PATTERN_MATCH"_ns, 1, patternMatchFunction);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<mozIStorageStatement> stmt =
          aThread->mWorkerStatements.GetCachedStatement(
              "DELETE FROM webappsstore2"
              " WHERE ORIGIN_ATTRS_PATTERN_MATCH(originAttributes)");

      if (stmt) {
        mozStorageStatementScoper scope(stmt);
        rv = stmt->Execute();
      } else {
        rv = NS_ERROR_UNEXPECTED;
      }

      // Always remove the function
      aThread->mWorkerConnection->RemoveFunction(
          "ORIGIN_ATTRS_PATTERN_MATCH"_ns);

      NS_ENSURE_SUCCESS(rv, rv);

      // No need to selectively clear mOriginsHavingData here.  That hashtable
      // only prevents preload for scopes with no data.  Leaving a false record
      // in it has a negligible effect on performance.
      break;
    }

    default:
      NS_ERROR("Unknown task type");
      break;
  }

  return NS_OK;
}

void StorageDBThread::DBOperation::Finalize(nsresult aRv) {
  switch (mType) {
    case opPreloadUrgent:
    case opPreload:
      if (NS_FAILED(aRv)) {
        // When we are here, something failed when loading from the database.
        // Notify that the storage is loaded to prevent deadlock of the main
        // thread, even though it is actually empty or incomplete.
        NS_WARNING("Failed to preload localStorage");
      }

      mCache->LoadDone(aRv);
      break;

    case opGetUsage:
      if (NS_FAILED(aRv)) {
        mUsage->LoadUsage(0);
      }

      break;

    default:
      if (NS_FAILED(aRv)) {
        NS_WARNING(
            "localStorage update/clear operation failed,"
            " data may not persist or clean up");
      }

      break;
  }
}

// StorageDBThread::PendingOperations

StorageDBThread::PendingOperations::PendingOperations()
    : mFlushFailureCount(0) {}

bool StorageDBThread::PendingOperations::HasTasks() const {
  return !!mUpdates.Count() || !!mClears.Count();
}

namespace {

bool OriginPatternMatches(const nsACString& aOriginSuffix,
                          const OriginAttributesPattern& aPattern) {
  OriginAttributes oa;
  DebugOnly<bool> rv = oa.PopulateFromSuffix(aOriginSuffix);
  MOZ_ASSERT(rv);
  return aPattern.Matches(oa);
}

}  // namespace

bool StorageDBThread::PendingOperations::CheckForCoalesceOpportunity(
    DBOperation* aNewOp, DBOperation::OperationType aPendingType,
    DBOperation::OperationType aNewType) {
  if (aNewOp->Type() != aNewType) {
    return false;
  }

  StorageDBThread::DBOperation* pendingTask;
  if (!mUpdates.Get(aNewOp->Target(), &pendingTask)) {
    return false;
  }

  if (pendingTask->Type() != aPendingType) {
    return false;
  }

  return true;
}

void StorageDBThread::PendingOperations::Add(
    UniquePtr<StorageDBThread::DBOperation> aOperation) {
  // Optimize: when a key to remove has never been written to disk
  // just bypass this operation.  A key is new when an operation scheduled
  // to write it to the database is of type opAddItem.
  if (CheckForCoalesceOpportunity(aOperation.get(), DBOperation::opAddItem,
                                  DBOperation::opRemoveItem)) {
    mUpdates.Remove(aOperation->Target());
    return;
  }

  // Optimize: when changing a key that is new and has never been
  // written to disk, keep type of the operation to store it at opAddItem.
  // This allows optimization to just forget adding a new key when
  // it is removed from the storage before flush.
  if (CheckForCoalesceOpportunity(aOperation.get(), DBOperation::opAddItem,
                                  DBOperation::opUpdateItem)) {
    aOperation->mType = DBOperation::opAddItem;
  }

  // Optimize: to prevent lose of remove operation on a key when doing
  // remove/set/remove on a previously existing key we have to change
  // opAddItem to opUpdateItem on the new operation when there is opRemoveItem
  // pending for the key.
  if (CheckForCoalesceOpportunity(aOperation.get(), DBOperation::opRemoveItem,
                                  DBOperation::opAddItem)) {
    aOperation->mType = DBOperation::opUpdateItem;
  }

  switch (aOperation->Type()) {
      // Operations on single keys

    case DBOperation::opAddItem:
    case DBOperation::opUpdateItem:
    case DBOperation::opRemoveItem:
      // Override any existing operation for the target (=scope+key).
      mUpdates.InsertOrUpdate(aOperation->Target(), std::move(aOperation));
      break;

      // Clear operations

    case DBOperation::opClear:
    case DBOperation::opClearMatchingOrigin:
    case DBOperation::opClearMatchingOriginAttributes:
      // Drop all update (insert/remove) operations for equivavelent or matching
      // scope.  We do this as an optimization as well as a must based on the
      // logic, if we would not delete the update tasks, changes would have been
      // stored to the database after clear operations have been executed.
      for (auto iter = mUpdates.Iter(); !iter.Done(); iter.Next()) {
        const auto& pendingTask = iter.Data();

        if (aOperation->Type() == DBOperation::opClear &&
            (pendingTask->OriginNoSuffix() != aOperation->OriginNoSuffix() ||
             pendingTask->OriginSuffix() != aOperation->OriginSuffix())) {
          continue;
        }

        if (aOperation->Type() == DBOperation::opClearMatchingOrigin &&
            !StringBeginsWith(pendingTask->OriginNoSuffix(),
                              aOperation->Origin())) {
          continue;
        }

        if (aOperation->Type() ==
                DBOperation::opClearMatchingOriginAttributes &&
            !OriginPatternMatches(pendingTask->OriginSuffix(),
                                  aOperation->OriginPattern())) {
          continue;
        }

        iter.Remove();
      }

      mClears.InsertOrUpdate(aOperation->Target(), std::move(aOperation));
      break;

    case DBOperation::opClearAll:
      // Drop simply everything, this is a super-operation.
      mUpdates.Clear();
      mClears.Clear();
      mClears.InsertOrUpdate(aOperation->Target(), std::move(aOperation));
      break;

    default:
      MOZ_ASSERT(false);
      break;
  }
}

bool StorageDBThread::PendingOperations::Prepare() {
  // Called under the lock

  // First collect clear operations and then updates, we can
  // do this since whenever a clear operation for a scope is
  // scheduled, we drop all updates matching that scope. So,
  // all scope-related update operations we have here now were
  // scheduled after the clear operations.
  for (auto iter = mClears.Iter(); !iter.Done(); iter.Next()) {
    mExecList.AppendElement(std::move(iter.Data()));
  }
  mClears.Clear();

  for (auto iter = mUpdates.Iter(); !iter.Done(); iter.Next()) {
    mExecList.AppendElement(std::move(iter.Data()));
  }
  mUpdates.Clear();

  return !!mExecList.Length();
}

nsresult StorageDBThread::PendingOperations::Execute(StorageDBThread* aThread) {
  // Called outside the lock

  mozStorageTransaction transaction(aThread->mWorkerConnection, false);

  nsresult rv = transaction.Start();
  if (NS_FAILED(rv)) {
    return rv;
  }

  for (uint32_t i = 0; i < mExecList.Length(); ++i) {
    const auto& task = mExecList[i];
    rv = task->Perform(aThread);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  rv = transaction.Commit();
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

bool StorageDBThread::PendingOperations::Finalize(nsresult aRv) {
  // Called under the lock

  // The list is kept on a failure to retry it
  if (NS_FAILED(aRv)) {
    // XXX Followup: we may try to reopen the database and flush these
    // pending tasks, however testing showed that even though I/O is actually
    // broken some amount of operations is left in sqlite+system buffers and
    // seems like successfully flushed to disk.
    // Tested by removing a flash card and disconnecting from network while
    // using a network drive on Windows system.
    NS_WARNING("Flush operation on localStorage database failed");

    ++mFlushFailureCount;

    return mFlushFailureCount >= 5;
  }

  mFlushFailureCount = 0;
  mExecList.Clear();
  return true;
}

namespace {

bool FindPendingClearForOrigin(
    const nsACString& aOriginSuffix, const nsACString& aOriginNoSuffix,
    StorageDBThread::DBOperation* aPendingOperation) {
  if (aPendingOperation->Type() == StorageDBThread::DBOperation::opClearAll) {
    return true;
  }

  if (aPendingOperation->Type() == StorageDBThread::DBOperation::opClear &&
      aOriginNoSuffix == aPendingOperation->OriginNoSuffix() &&
      aOriginSuffix == aPendingOperation->OriginSuffix()) {
    return true;
  }

  if (aPendingOperation->Type() ==
          StorageDBThread::DBOperation::opClearMatchingOrigin &&
      StringBeginsWith(aOriginNoSuffix, aPendingOperation->Origin())) {
    return true;
  }

  if (aPendingOperation->Type() ==
          StorageDBThread::DBOperation::opClearMatchingOriginAttributes &&
      OriginPatternMatches(aOriginSuffix, aPendingOperation->OriginPattern())) {
    return true;
  }

  return false;
}

}  // namespace

bool StorageDBThread::PendingOperations::IsOriginClearPending(
    const nsACString& aOriginSuffix, const nsACString& aOriginNoSuffix) const {
  // Called under the lock

  for (const auto& clear : mClears.Values()) {
    if (FindPendingClearForOrigin(aOriginSuffix, aOriginNoSuffix,
                                  clear.get())) {
      return true;
    }
  }

  for (uint32_t i = 0; i < mExecList.Length(); ++i) {
    if (FindPendingClearForOrigin(aOriginSuffix, aOriginNoSuffix,
                                  mExecList[i].get())) {
      return true;
    }
  }

  return false;
}

namespace {

bool FindPendingUpdateForOrigin(
    const nsACString& aOriginSuffix, const nsACString& aOriginNoSuffix,
    StorageDBThread::DBOperation* aPendingOperation) {
  if ((aPendingOperation->Type() == StorageDBThread::DBOperation::opAddItem ||
       aPendingOperation->Type() ==
           StorageDBThread::DBOperation::opUpdateItem ||
       aPendingOperation->Type() ==
           StorageDBThread::DBOperation::opRemoveItem) &&
      aOriginNoSuffix == aPendingOperation->OriginNoSuffix() &&
      aOriginSuffix == aPendingOperation->OriginSuffix()) {
    return true;
  }

  return false;
}

}  // namespace

bool StorageDBThread::PendingOperations::IsOriginUpdatePending(
    const nsACString& aOriginSuffix, const nsACString& aOriginNoSuffix) const {
  // Called under the lock

  for (const auto& update : mUpdates.Values()) {
    if (FindPendingUpdateForOrigin(aOriginSuffix, aOriginNoSuffix,
                                   update.get())) {
      return true;
    }
  }

  for (uint32_t i = 0; i < mExecList.Length(); ++i) {
    if (FindPendingUpdateForOrigin(aOriginSuffix, aOriginNoSuffix,
                                   mExecList[i].get())) {
      return true;
    }
  }

  return false;
}

nsresult StorageDBThread::InitHelper::SyncDispatchAndReturnProfilePath(
    nsAString& aProfilePath) {
  ::mozilla::ipc::AssertIsOnBackgroundThread();

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(this));

  mozilla::MutexAutoLock autolock(mMutex);
  while (mWaiting) {
    mCondVar.Wait();
  }

  if (NS_WARN_IF(NS_FAILED(mMainThreadResultCode))) {
    return mMainThreadResultCode;
  }

  aProfilePath = mProfilePath;
  return NS_OK;
}

NS_IMETHODIMP
StorageDBThread::InitHelper::Run() {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = GetProfilePath(mProfilePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mMainThreadResultCode = rv;
  }

  mozilla::MutexAutoLock lock(mMutex);
  MOZ_ASSERT(mWaiting);

  mWaiting = false;
  mCondVar.Notify();

  return NS_OK;
}

NS_IMETHODIMP
StorageDBThread::NoteBackgroundThreadRunnable::Run() {
  MOZ_ASSERT(NS_IsMainThread());

  StorageObserver* observer = StorageObserver::Self();
  MOZ_ASSERT(observer);

  observer->NoteBackgroundThread(mPrivateBrowsingId, mOwningThread);

  return NS_OK;
}

NS_IMETHODIMP
StorageDBThread::ShutdownRunnable::Run() {
  if (NS_IsMainThread()) {
    mDone = true;

    return NS_OK;
  }

  ::mozilla::ipc::AssertIsOnBackgroundThread();
  MOZ_RELEASE_ASSERT(mPrivateBrowsingId < kPrivateBrowsingIdCount);

  StorageDBThread*& storageThread = sStorageThread[mPrivateBrowsingId];
  if (storageThread) {
    sStorageThreadDown[mPrivateBrowsingId] = true;

    storageThread->Shutdown();

    delete storageThread;
    storageThread = nullptr;
  }

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(this));

  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
