/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StorageDBThread_h
#define mozilla_dom_StorageDBThread_h

#include "prthread.h"
#include "prinrval.h"
#include "nsTArray.h"
#include "mozilla/Atomics.h"
#include "mozilla/Monitor.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/OriginAttributes.h"
#include "mozilla/storage/StatementCache.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsClassHashtable.h"
#include "nsIFile.h"
#include "nsIThreadInternal.h"
#include "nsThreadUtils.h"
#include "nsTHashSet.h"

class mozIStorageConnection;

namespace mozilla {
namespace dom {

class LocalStorageCacheBridge;
class StorageUsageBridge;
class StorageUsage;

using StatementCache = mozilla::storage::StatementCache<mozIStorageStatement>;

// XXX Fix me!
//     1. Move comments to StorageDBThread/StorageDBChild.
//     2. Devirtualize relevant methods in StorageDBThread/StorageDBChild.
//     3. Remove relevant methods in StorageDBThread/StorageDBChild that are
//        unused.
//     4. Remove this class completely.
//
//     See bug 1387636 for more details.
#if 0
// Interface used by the cache to post operations to the asynchronous
// database thread or process.
class StorageDBBridge
{
public:
  StorageDBBridge();
  virtual ~StorageDBBridge() {}

  // Ensures the database engine is started
  virtual nsresult Init() = 0;

  // Releases the database and disallows its usage
  virtual nsresult Shutdown() = 0;

  // Asynchronously fills the cache with data from the database for first use.
  // When |aPriority| is true, the preload operation is scheduled as the first
  // one.  This method is responsible to keep hard reference to the cache for
  // the time of the preload or, when preload cannot be performed, call
  // LoadDone() immediately.
  virtual void AsyncPreload(LocalStorageCacheBridge* aCache,
                            bool aPriority = false) = 0;

  // Asynchronously fill the |usage| object with actual usage of data by its
  // scope.  The scope is eTLD+1 tops, never deeper subdomains.
  virtual void AsyncGetUsage(StorageUsageBridge* aUsage) = 0;

  // Synchronously fills the cache, when |aForceSync| is false and cache already
  // got some data before, the method waits for the running preload to finish
  virtual void SyncPreload(LocalStorageCacheBridge* aCache,
                           bool aForceSync = false) = 0;

  // Called when an existing key is modified in the storage, schedules update to
  // the database
  virtual nsresult AsyncAddItem(LocalStorageCacheBridge* aCache,
                                const nsAString& aKey,
                                const nsAString& aValue) = 0;

  // Called when an existing key is modified in the storage, schedules update to
  // the database
  virtual nsresult AsyncUpdateItem(LocalStorageCacheBridge* aCache,
                                   const nsAString& aKey,
                                   const nsAString& aValue) = 0;

  // Called when an item is removed from the storage, schedules delete of the
  // key
  virtual nsresult AsyncRemoveItem(LocalStorageCacheBridge* aCache,
                                   const nsAString& aKey) = 0;

  // Called when the whole storage is cleared by the DOM API, schedules delete
  // of the scope
  virtual nsresult AsyncClear(LocalStorageCacheBridge* aCache) = 0;

  // Called when chrome deletes e.g. cookies, schedules delete of the whole
  // database
  virtual void AsyncClearAll() = 0;

  // Called when only a domain and its subdomains is about to clear
  virtual void AsyncClearMatchingOrigin(const nsACString& aOriginNoSuffix) = 0;

  // Called when data matching an origin pattern have to be cleared
  virtual void AsyncClearMatchingOriginAttributes(const OriginAttributesPattern& aPattern) = 0;

  // Forces scheduled DB operations to be early flushed to the disk
  virtual void AsyncFlush() = 0;

  // Check whether the scope has any data stored on disk and is thus allowed to
  // preload
  virtual bool ShouldPreloadOrigin(const nsACString& aOriginNoSuffix) = 0;
};
#endif

// The implementation of the the database engine, this directly works
// with the sqlite or any other db API we are based on
// This class is resposible for collecting and processing asynchronous
// DB operations over caches (LocalStorageCache) communicating though
// LocalStorageCacheBridge interface class
class StorageDBThread final {
 public:
  class PendingOperations;

  // Representation of a singe database task, like adding and removing keys,
  // (pre)loading the whole origin data, cleaning.
  class DBOperation {
   public:
    enum OperationType {
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
      // Clears a specific single origin data
      opClear,

      // Operations invoked by chrome

      // Clear all the data stored in the database, for all scopes, no
      // exceptions
      opClearAll,
      // Clear data under a domain and all its subdomains regardless
      // OriginAttributes value
      opClearMatchingOrigin,
      // Clear all data matching an OriginAttributesPattern regardless a domain
      opClearMatchingOriginAttributes,
    };

    explicit DBOperation(const OperationType aType,
                         LocalStorageCacheBridge* aCache = nullptr,
                         const nsAString& aKey = u""_ns,
                         const nsAString& aValue = u""_ns);
    DBOperation(const OperationType aType, StorageUsageBridge* aUsage);
    DBOperation(const OperationType aType, const nsACString& aOriginNoSuffix);
    DBOperation(const OperationType aType,
                const OriginAttributesPattern& aOriginNoSuffix);
    ~DBOperation();

    // Executes the operation, doesn't necessarity have to be called on the I/O
    // thread
    void PerformAndFinalize(StorageDBThread* aThread);

    // Finalize the operation, i.e. do any internal cleanup and finish calls
    void Finalize(nsresult aRv);

    // The operation type
    OperationType Type() const { return mType; }

    // The origin in the database usage format (reversed)
    const nsCString OriginNoSuffix() const;

    // The origin attributes suffix
    const nsCString OriginSuffix() const;

    // |origin suffix + origin key| the operation is working with or a scope
    // pattern to delete with simple SQL's "LIKE %" from the database.
    const nsCString Origin() const;

    // |origin suffix + origin key + key| the operation is working with
    const nsCString Target() const;

    // Pattern to delete matching data with this op
    const OriginAttributesPattern& OriginPattern() const {
      return mOriginPattern;
    }

   private:
    // The operation implementation body
    nsresult Perform(StorageDBThread* aThread);

    friend class PendingOperations;
    OperationType mType;
    RefPtr<LocalStorageCacheBridge> mCache;
    RefPtr<StorageUsageBridge> mUsage;
    nsString const mKey;
    nsString const mValue;
    nsCString const mOrigin;
    OriginAttributesPattern const mOriginPattern;
  };

  // Encapsulation of collective and coalescing logic for all pending operations
  // except preloads that are handled separately as priority operations
  class PendingOperations {
   public:
    PendingOperations();

    // Method responsible for coalescing redundant update operations with the
    // same |Target()| or clear operations with the same or matching |Origin()|
    void Add(UniquePtr<DBOperation> aOperation);

    // True when there are some scheduled operations to flush on disk
    bool HasTasks() const;

    // Moves collected operations to a local flat list to allow execution of the
    // operation list out of the thread lock
    bool Prepare();

    // Executes the previously |Prepared()'ed| list of operations, returns
    // result, but doesn't handle it in any way in case of a failure
    nsresult Execute(StorageDBThread* aThread);

    // Finalizes the pending operation list, returns false when too many
    // operations failed to flush what indicates a long standing issue with the
    // database access.
    bool Finalize(nsresult aRv);

    // true when a clear that deletes the given origin attr pattern and/or
    // origin key is among the pending operations; when a preload for that scope
    // is being scheduled, it must be finished right away
    bool IsOriginClearPending(const nsACString& aOriginSuffix,
                              const nsACString& aOriginNoSuffix) const;

    // Checks whether there is a pending update operation for this scope.
    bool IsOriginUpdatePending(const nsACString& aOriginSuffix,
                               const nsACString& aOriginNoSuffix) const;

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
    nsTArray<UniquePtr<DBOperation> > mExecList;

    // Number of failing flush attempts
    uint32_t mFlushFailureCount;
  };

  class ThreadObserver final : public nsIThreadObserver {
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSITHREADOBSERVER

    ThreadObserver()
        : mHasPendingEvents(false), mMonitor("StorageThreadMonitor") {}

    bool HasPendingEvents() {
      mMonitor.AssertCurrentThreadOwns();
      return mHasPendingEvents;
    }
    void ClearPendingEvents() {
      mMonitor.AssertCurrentThreadOwns();
      mHasPendingEvents = false;
    }
    Monitor& GetMonitor() { return mMonitor; }

   private:
    virtual ~ThreadObserver() = default;
    bool mHasPendingEvents;
    // The monitor we drive the thread with
    Monitor mMonitor;
  };

  class InitHelper;

  class NoteBackgroundThreadRunnable;

  class ShutdownRunnable : public Runnable {
    // Expected to be only 0 or 1.
    const uint32_t mPrivateBrowsingId;
    // Only touched on the main thread.
    bool& mDone;

   public:
    explicit ShutdownRunnable(const uint32_t aPrivateBrowsingId, bool& aDone)
        : Runnable("dom::StorageDBThread::ShutdownRunnable"),
          mPrivateBrowsingId(aPrivateBrowsingId),
          mDone(aDone) {
      MOZ_ASSERT(NS_IsMainThread());
    }

   private:
    ~ShutdownRunnable() = default;

    NS_DECL_NSIRUNNABLE
  };

 public:
  explicit StorageDBThread(uint32_t aPrivateBrowsingId);
  virtual ~StorageDBThread() = default;

  static StorageDBThread* Get(uint32_t aPrivateBrowsingId);

  static StorageDBThread* GetOrCreate(const nsString& aProfilePath,
                                      uint32_t aPrivateBrowsingId);

  static nsresult GetProfilePath(nsString& aProfilePath);

  virtual nsresult Init(const nsString& aProfilePath);

  // Flushes all uncommited data and stops the I/O thread.
  virtual nsresult Shutdown();

  virtual void AsyncPreload(LocalStorageCacheBridge* aCache,
                            bool aPriority = false) {
    InsertDBOp(MakeUnique<DBOperation>(
        aPriority ? DBOperation::opPreloadUrgent : DBOperation::opPreload,
        aCache));
  }

  virtual void SyncPreload(LocalStorageCacheBridge* aCache,
                           bool aForce = false);

  virtual void AsyncGetUsage(StorageUsageBridge* aUsage) {
    InsertDBOp(MakeUnique<DBOperation>(DBOperation::opGetUsage, aUsage));
  }

  virtual nsresult AsyncAddItem(LocalStorageCacheBridge* aCache,
                                const nsAString& aKey,
                                const nsAString& aValue) {
    return InsertDBOp(
        MakeUnique<DBOperation>(DBOperation::opAddItem, aCache, aKey, aValue));
  }

  virtual nsresult AsyncUpdateItem(LocalStorageCacheBridge* aCache,
                                   const nsAString& aKey,
                                   const nsAString& aValue) {
    return InsertDBOp(MakeUnique<DBOperation>(DBOperation::opUpdateItem, aCache,
                                              aKey, aValue));
  }

  virtual nsresult AsyncRemoveItem(LocalStorageCacheBridge* aCache,
                                   const nsAString& aKey) {
    return InsertDBOp(
        MakeUnique<DBOperation>(DBOperation::opRemoveItem, aCache, aKey));
  }

  virtual nsresult AsyncClear(LocalStorageCacheBridge* aCache) {
    return InsertDBOp(MakeUnique<DBOperation>(DBOperation::opClear, aCache));
  }

  virtual void AsyncClearAll() {
    InsertDBOp(MakeUnique<DBOperation>(DBOperation::opClearAll));
  }

  virtual void AsyncClearMatchingOrigin(const nsACString& aOriginNoSuffix) {
    InsertDBOp(MakeUnique<DBOperation>(DBOperation::opClearMatchingOrigin,
                                       aOriginNoSuffix));
  }

  virtual void AsyncClearMatchingOriginAttributes(
      const OriginAttributesPattern& aPattern) {
    InsertDBOp(MakeUnique<DBOperation>(
        DBOperation::opClearMatchingOriginAttributes, aPattern));
  }

  virtual void AsyncFlush();

  virtual bool ShouldPreloadOrigin(const nsACString& aOrigin);

  // Get the complete list of scopes having data.
  void GetOriginsHavingData(nsTArray<nsCString>* aOrigins);

 private:
  nsCOMPtr<nsIFile> mDatabaseFile;
  PRThread* mThread;

  // Used to observe runnables dispatched to our thread and to monitor it.
  RefPtr<ThreadObserver> mThreadObserver;

  // Flag to stop, protected by the monitor returned by
  // mThreadObserver->GetMonitor().
  bool mStopIOThread;

  // Whether WAL is enabled
  bool mWALModeEnabled;

  // Whether DB has already been open, avoid races between main thread reads
  // and pending DB init in the background I/O thread
  Atomic<bool, ReleaseAcquire> mDBReady;

  // State of the database initiation
  nsresult mStatus;

  // List of origins (including origin attributes suffix) having data, for
  // optimization purposes only
  nsTHashSet<nsCString> mOriginsHavingData;

  // Connection used by the worker thread for all read and write ops
  nsCOMPtr<mozIStorageConnection> mWorkerConnection;

  // Connection used only on the main thread for sync read operations
  nsCOMPtr<mozIStorageConnection> mReaderConnection;

  StatementCache mWorkerStatements;
  StatementCache mReaderStatements;

  // Time the first pending operation has been added to the pending operations
  // list
  TimeStamp mDirtyEpoch;

  // Flag to force immediate flush of all pending operations
  bool mFlushImmediately;

  // List of preloading operations, in chronological or priority order.
  // Executed prioritly over pending update operations.
  nsTArray<DBOperation*> mPreloads;

  // Collector of pending update operations
  PendingOperations mPendingTasks;

  // Expected to be only 0 or 1.
  const uint32_t mPrivateBrowsingId;

  // Counter of calls for thread priority rising.
  int32_t mPriorityCounter;

  // Helper to direct an operation to one of the arrays above;
  // also checks IsOriginClearPending for preloads
  nsresult InsertDBOp(UniquePtr<DBOperation> aOperation);

  // Opens the database, first thing we do after start of the thread.
  nsresult OpenDatabaseConnection();
  nsresult OpenAndUpdateDatabase();
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
  // - TimeDuration::Forever() when no pending tasks are scheduled
  // - Non-zero TimeDuration when tasks have been scheduled, but it
  //   is still not time to perform the flush ; it is actual time to
  //   wait until the flush has to happen.
  // - 0 TimeDuration when it is time to do the flush
  TimeDuration TimeUntilFlush();

  // Notifies to the main thread that flush has completed
  void NotifyFlushCompletion();

  // Thread loop
  static void ThreadFunc(void* aArg);
  void ThreadFunc();
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_StorageDBThread_h
