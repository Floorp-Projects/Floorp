/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOMStorageDBThread_h___
#define DOMStorageDBThread_h___

#include "prthread.h"
#include "prinrval.h"
#include "nsTArray.h"
#include "mozilla/Monitor.h"
#include "mozilla/storage/StatementCache.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsClassHashtable.h"
#include "nsIFile.h"

class mozIStorageConnection;

namespace mozilla {
namespace dom {

class DOMStorageCacheBridge;
class DOMStorageUsageBridge;
class DOMStorageUsage;

typedef mozilla::storage::StatementCache<mozIStorageStatement> StatementCache;

// Interface used by the cache to post operations to the asynchronous
// database thread or process.
class DOMStorageDBBridge
{
public:
  DOMStorageDBBridge();
  virtual ~DOMStorageDBBridge() {}

  // Ensures the database engine is started
  virtual nsresult Init() = 0;

  // Releases the database and disallows its usage
  virtual nsresult Shutdown() = 0;

  // Asynchronously fills the cache with data from the database for first use.
  // When |aPriority| is true, the preload operation is scheduled as the first one.
  // This method is responsible to keep hard reference to the cache for the time of
  // the preload or, when preload cannot be performed, call LoadDone() immediately.
  virtual void AsyncPreload(DOMStorageCacheBridge* aCache, bool aPriority = false) = 0;

  // Asynchronously fill the |usage| object with actual usage of data by its scope.
  // The scope is eTLD+1 tops, never deeper subdomains.
  virtual void AsyncGetUsage(DOMStorageUsageBridge* aUsage) = 0;

  // Synchronously fills the cache, when |aForceSync| is false and cache already got some
  // data before, the method waits for the running preload to finish
  virtual void SyncPreload(DOMStorageCacheBridge* aCache, bool aForceSync = false) = 0;

  // Called when an existing key is modified in the storage, schedules update to the database
  virtual nsresult AsyncAddItem(DOMStorageCacheBridge* aCache, const nsAString& aKey, const nsAString& aValue) = 0;

  // Called when an existing key is modified in the storage, schedules update to the database
  virtual nsresult AsyncUpdateItem(DOMStorageCacheBridge* aCache, const nsAString& aKey, const nsAString& aValue) = 0;

  // Called when an item is removed from the storage, schedules delete of the key
  virtual nsresult AsyncRemoveItem(DOMStorageCacheBridge* aCache, const nsAString& aKey) = 0;

  // Called when the whole storage is cleared by the DOM API, schedules delete of the scope
  virtual nsresult AsyncClear(DOMStorageCacheBridge* aCache) = 0;

  // Called when chrome deletes e.g. cookies, schedules delete of the whole database
  virtual void AsyncClearAll() = 0;

  // Called when only a domain and its subdomains or an app data is about to clear
  virtual void AsyncClearMatchingScope(const nsACString& aScope) = 0;

  // Forces scheduled DB operations to be early flushed to the disk
  virtual void AsyncFlush() = 0;

  // Check whether the scope has any data stored on disk and is thus allowed to preload
  virtual bool ShouldPreloadScope(const nsACString& aScope) = 0;

  // Get the complete list of scopes having data
  virtual void GetScopesHavingData(InfallibleTArray<nsCString>* aScopes) = 0;
};

// The implementation of the the database engine, this directly works
// with the sqlite or any other db API we are based on
// This class is resposible for collecting and processing asynchronous 
// DB operations over caches (DOMStorageCache) communicating though 
// DOMStorageCacheBridge interface class
class DOMStorageDBThread MOZ_FINAL : public DOMStorageDBBridge
{
public:
  class PendingOperations;

  // Representation of a singe database task, like adding and removing keys,
  // (pre)loading the whole origin data, cleaning.
  class DBOperation
  {
  public:
    typedef enum {
      // Only operation that reads data from the database
      opPreload,
      // The same as opPreload, just executed with highest priority
      opPreloadUrgent,

      // Load usage of a scope
      opGetUsage,

      // Operations invoked by the DOM content API
      opAddItem,
      opUpdateItem,
      opRemoveItem,
      opClear,

      // Operations invoked by chrome
      opClearAll,
      opClearMatchingScope,
    } OperationType;

    DBOperation(const OperationType aType,
                DOMStorageCacheBridge* aCache = nullptr,
                const nsAString& aKey = EmptyString(),
                const nsAString& aValue = EmptyString());
    DBOperation(const OperationType aType,
                DOMStorageUsageBridge* aUsage);
    DBOperation(const OperationType aType,
                const nsACString& aScope);
    ~DBOperation();

    // Executes the operation, doesn't necessarity have to be called on the I/O thread
    void PerformAndFinalize(DOMStorageDBThread* aThread);

    // Finalize the operation, i.e. do any internal cleanup and finish calls
    void Finalize(nsresult aRv);

    // The operation type
    OperationType Type() { return mType; }

    // The operation scope (=origin)
    const nsCString Scope();

    // |Scope + key| the operation is working with
    const nsCString Target();

  private:
    // The operation implementation body
    nsresult Perform(DOMStorageDBThread* aThread);

    friend class PendingOperations;
    OperationType mType;
    nsRefPtr<DOMStorageCacheBridge> mCache;
    nsRefPtr<DOMStorageUsageBridge> mUsage;
    nsString mKey;
    nsString mValue;
    nsCString mScope;
  };

  // Encapsulation of collective and coalescing logic for all pending operations
  // except preloads that are handled separately as priority operations
  class PendingOperations {
  public:
    PendingOperations();

    // Method responsible for coalescing redundant update operations with the same
    // |Target()| or clear operations with the same or matching |Scope()|
    void Add(DBOperation* aOperation);

    // True when there are some scheduled operations to flush on disk
    bool HasTasks();

    // Moves collected operations to a local flat list to allow execution of the operation
    // list out of the thread lock
    bool Prepare();

    // Executes the previously |Prepared()'ed| list of operations, retuns result, but doesn't
    // handle it in any way in case of a failure
    nsresult Execute(DOMStorageDBThread* aThread);

    // Finalizes the pending operation list, returns false when too many operations failed
    // to flush what indicates a long standing issue with the database access.
    bool Finalize(nsresult aRv);

    // true when a clear that deletes the given |scope| is among the pending operations;
    // when a preload for that scope is being scheduled, it must be finished right away
    bool IsScopeClearPending(const nsACString& aScope);

    // Checks whether there is a pending update (or clear, actually) operation for this scope.
    bool IsScopeUpdatePending(const nsACString& aScope);

  private:
    // Returns true iff new operation is of type newType and there is a pending 
    // operation of type pendingType for the same key (target).
    bool CheckForCoalesceOpportunity(DBOperation* aNewOp,
                                     DBOperation::OperationType aPendingType,
                                     DBOperation::OperationType aNewType);

    // List of all clearing operations, executed first
    nsClassHashtable<nsCStringHashKey, DBOperation> mClears;

    // List of all update/insert operations, executed as second
    nsClassHashtable<nsCStringHashKey, DBOperation> mUpdates;

    // Collection of all tasks, valid only between Prepare() and Execute()
    nsTArray<nsAutoPtr<DBOperation> > mExecList;

    // Number of failing flush attempts
    uint32_t mFlushFailureCount;
  };

public:
  DOMStorageDBThread();
  virtual ~DOMStorageDBThread() {}

  virtual nsresult Init();
  virtual nsresult Shutdown();

  virtual void AsyncPreload(DOMStorageCacheBridge* aCache, bool aPriority = false)
    { InsertDBOp(new DBOperation(aPriority ? DBOperation::opPreloadUrgent : DBOperation::opPreload, aCache)); }

  virtual void SyncPreload(DOMStorageCacheBridge* aCache, bool aForce = false);

  virtual void AsyncGetUsage(DOMStorageUsageBridge * aUsage)
    { InsertDBOp(new DBOperation(DBOperation::opGetUsage, aUsage)); }

  virtual nsresult AsyncAddItem(DOMStorageCacheBridge* aCache, const nsAString& aKey, const nsAString& aValue)
    { return InsertDBOp(new DBOperation(DBOperation::opAddItem, aCache, aKey, aValue)); }

  virtual nsresult AsyncUpdateItem(DOMStorageCacheBridge* aCache, const nsAString& aKey, const nsAString& aValue)
    { return InsertDBOp(new DBOperation(DBOperation::opUpdateItem, aCache, aKey, aValue)); }

  virtual nsresult AsyncRemoveItem(DOMStorageCacheBridge* aCache, const nsAString& aKey)
    { return InsertDBOp(new DBOperation(DBOperation::opRemoveItem, aCache, aKey)); }

  virtual nsresult AsyncClear(DOMStorageCacheBridge* aCache)
    { return InsertDBOp(new DBOperation(DBOperation::opClear, aCache)); }

  virtual void AsyncClearAll()
    { InsertDBOp(new DBOperation(DBOperation::opClearAll)); }

  virtual void AsyncClearMatchingScope(const nsACString& aScope)
    { InsertDBOp(new DBOperation(DBOperation::opClearMatchingScope, aScope)); }

  virtual void AsyncFlush();

  virtual bool ShouldPreloadScope(const nsACString& aScope);
  virtual void GetScopesHavingData(InfallibleTArray<nsCString>* aScopes);

private:
  nsCOMPtr<nsIFile> mDatabaseFile;
  PRThread* mThread;

  // The monitor we drive the thread with
  Monitor mMonitor;

  // Flag to stop, protected by the monitor
  bool mStopIOThread;

  // Whether WAL is enabled
  bool mWALModeEnabled;

  // Whether DB has already been open, avoid races between main thread reads
  // and pending DB init in the background I/O thread
  bool mDBReady;

  // State of the database initiation
  nsresult mStatus;

  // List of scopes having data, for optimization purposes only
  nsTHashtable<nsCStringHashKey> mScopesHavingData;

  StatementCache mWorkerStatements;
  StatementCache mReaderStatements;

  // Connection used by the worker thread for all read and write ops
  nsCOMPtr<mozIStorageConnection> mWorkerConnection;

  // Connection used only on the main thread for sync read operations
  nsCOMPtr<mozIStorageConnection> mReaderConnection;

  // Time the first pending operation has been added to the pending operations
  // list
  PRIntervalTime mDirtyEpoch;

  // Flag to force immediate flush of all pending operations
  bool mFlushImmediately;

  // List of preloading operations, in chronological or priority order.
  // Executed prioritly over pending update operations.
  nsTArray<DBOperation*> mPreloads;

  // Collector of pending update operations
  PendingOperations mPendingTasks;

  // Counter of calls for thread priority rising.
  int32_t mPriorityCounter;

  // Helper to direct an operation to one of the arrays above;
  // also checks IsScopeClearPending for preloads
  nsresult InsertDBOp(DBOperation* aOperation);

  // Opens the database, first thing we do after start of the thread.
  nsresult OpenDatabaseConnection();
  nsresult InitDatabase();
  nsresult ShutdownDatabase();

  // Tries to establish WAL mode
  nsresult SetJournalMode(bool aIsWal);
  nsresult TryJournalMode();

  // Sets the threshold for auto-checkpointing the WAL.
  nsresult ConfigureWALBehavior();

  void SetHigherPriority();
  void SetDefaultPriority();

  // Ensures we flush pending tasks in some reasonble time
  void ScheduleFlush();

  // Called when flush of pending tasks is being executed
  void UnscheduleFlush();

  // This method is used for two purposes:
  // 1. as a value passed to monitor.Wait() method
  // 2. as in indicator that flush has to be performed
  //
  // Return:
  // - PR_INTERVAL_NO_TIMEOUT when no pending tasks are scheduled
  // - larger then zero when tasks have been scheduled, but it is
  //   still not time to perform the flush ; it is actual interval
  //   time to wait until the flush has to happen
  // - 0 when it is time to do the flush
  PRIntervalTime TimeUntilFlush();

  // Notifies to the main thread that flush has completed
  void NotifyFlushCompletion();

  // Thread loop
  static void ThreadFunc(void* aArg);
  void ThreadFunc();
};

} // ::dom
} // ::mozilla

#endif /* DOMStorageDBThread_h___ */
