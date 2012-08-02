/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_indexeddatabasemanager_h__
#define mozilla_dom_indexeddb_indexeddatabasemanager_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "mozilla/Mutex.h"

#include "nsIIndexedDatabaseManager.h"
#include "nsIObserver.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "nsIURI.h"

#include "nsClassHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsHashKeys.h"

#define INDEXEDDB_MANAGER_CONTRACTID "@mozilla.org/dom/indexeddb/manager;1"

class mozIStorageQuotaCallback;
class nsIAtom;
class nsIFile;
class nsITimer;
class nsPIDOMWindow;
class nsEventChainPostVisitor;

BEGIN_INDEXEDDB_NAMESPACE

class AsyncConnectionHelper;
class CheckQuotaHelper;
class FileManager;
class IDBDatabase;

class IndexedDatabaseManager MOZ_FINAL : public nsIIndexedDatabaseManager,
                                         public nsIObserver
{
  friend class IDBDatabase;

public:
  static already_AddRefed<IndexedDatabaseManager> GetOrCreate();

  // Returns a non-owning reference.
  static IndexedDatabaseManager* Get();

  // Returns an owning reference! No one should call this but the factory.
  static IndexedDatabaseManager* FactoryCreate();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIINDEXEDDATABASEMANAGER
  NS_DECL_NSIOBSERVER

  // Waits for databases to be cleared and for version change transactions to
  // complete before dispatching the given runnable.
  nsresult WaitForOpenAllowed(const nsACString& aOrigin,
                              nsIAtom* aId,
                              nsIRunnable* aRunnable);

  void AllowNextSynchronizedOp(const nsACString& aOrigin,
                               nsIAtom* aId);

  nsIThread* IOThread()
  {
    NS_ASSERTION(mIOThread, "This should never be null!");
    return mIOThread;
  }

  // Returns true if we've begun the shutdown process.
  static bool IsShuttingDown();

  static bool IsClosed();

  typedef void
  (*WaitingOnDatabasesCallback)(nsTArray<nsRefPtr<IDBDatabase> >&, void*);

  // Acquire exclusive access to the database given (waits for all others to
  // close).  If databases need to close first, the callback will be invoked
  // with an array of said databases.
  nsresult AcquireExclusiveAccess(IDBDatabase* aDatabase,
                                  const nsACString& aOrigin,
                                  AsyncConnectionHelper* aHelper,
                                  WaitingOnDatabasesCallback aCallback,
                                  void* aClosure)
  {
    NS_ASSERTION(aDatabase, "Need a DB here!");
    return AcquireExclusiveAccess(aOrigin, aDatabase, aHelper, nullptr,
                                  aCallback, aClosure);
  }

  nsresult AcquireExclusiveAccess(const nsACString& aOrigin,
                                  nsIRunnable* aRunnable,
                                  WaitingOnDatabasesCallback aCallback,
                                  void* aClosure)
  {
    return AcquireExclusiveAccess(aOrigin, nullptr, nullptr, aRunnable, aCallback,
                                  aClosure);
  }

  // Called when a window is being purged from the bfcache or the user leaves
  // a page which isn't going into the bfcache. Forces any live database
  // objects to close themselves and aborts any running transactions.
  void AbortCloseDatabasesForWindow(nsPIDOMWindow* aWindow);

  // Used to check if there are running transactions in a given window.
  bool HasOpenTransactions(nsPIDOMWindow* aWindow);

  // Set the Window that the current thread is doing operations for.
  // The caller is responsible for ensuring that aWindow is held alive.
  static inline void
  SetCurrentWindow(nsPIDOMWindow* aWindow)
  {
    IndexedDatabaseManager* mgr = Get();
    NS_ASSERTION(mgr, "Must have a manager here!");

    return mgr->SetCurrentWindowInternal(aWindow);
  }

  static PRUint32
  GetIndexedDBQuotaMB();

  nsresult EnsureOriginIsInitialized(const nsACString& aOrigin,
                                     FactoryPrivilege aPrivilege,
                                     nsIFile** aDirectory);

  // Determine if the quota is lifted for the Window the current thread is
  // using.
  static inline bool
  QuotaIsLifted()
  {
    IndexedDatabaseManager* mgr = Get();
    NS_ASSERTION(mgr, "Must have a manager here!");

    return mgr->QuotaIsLiftedInternal();
  }

  static inline void
  CancelPromptsForWindow(nsPIDOMWindow* aWindow)
  {
    IndexedDatabaseManager* mgr = Get();
    NS_ASSERTION(mgr, "Must have a manager here!");

    mgr->CancelPromptsForWindowInternal(aWindow);
  }

  static nsresult
  GetASCIIOriginFromWindow(nsPIDOMWindow* aWindow, nsCString& aASCIIOrigin);

  static bool
  IsMainProcess()
#ifdef DEBUG
  ;
#else
  {
    return sIsMainProcess;
  }
#endif

  already_AddRefed<FileManager>
  GetOrCreateFileManager(const nsACString& aOrigin,
                         const nsAString& aDatabaseName);

  already_AddRefed<FileManager>
  GetFileManager(const nsACString& aOrigin,
                 const nsAString& aDatabaseName);

  void InvalidateFileManagersForOrigin(const nsACString& aOrigin);

  void InvalidateFileManager(const nsACString& aOrigin,
                             const nsAString& aDatabaseName);

  nsresult AsyncDeleteFile(FileManager* aFileManager,
                           PRInt64 aFileId);

  const nsString&
  GetBaseDirectory() const
  {
    return mDatabaseBasePath;
  }

  nsresult
  GetDirectoryForOrigin(const nsACString& aASCIIOrigin,
                        nsIFile** aDirectory) const;

  static mozilla::Mutex& FileMutex()
  {
    IndexedDatabaseManager* mgr = Get();
    NS_ASSERTION(mgr, "Must have a manager here!");

    return mgr->mFileMutex;
  }

  static already_AddRefed<nsIAtom>
  GetDatabaseId(const nsACString& aOrigin,
                const nsAString& aName);

  static nsresult
  FireWindowOnError(nsPIDOMWindow* aOwner, nsEventChainPostVisitor& aVisitor);
private:
  IndexedDatabaseManager();
  ~IndexedDatabaseManager();

  nsresult AcquireExclusiveAccess(const nsACString& aOrigin,
                                  IDBDatabase* aDatabase,
                                  AsyncConnectionHelper* aHelper,
                                  nsIRunnable* aRunnable,
                                  WaitingOnDatabasesCallback aCallback,
                                  void* aClosure);

  void SetCurrentWindowInternal(nsPIDOMWindow* aWindow);
  bool QuotaIsLiftedInternal();
  void CancelPromptsForWindowInternal(nsPIDOMWindow* aWindow);

  // Called when a database is created.
  bool RegisterDatabase(IDBDatabase* aDatabase);

  // Called when a database is being unlinked or destroyed.
  void UnregisterDatabase(IDBDatabase* aDatabase);

  // Called when a database has been closed.
  void OnDatabaseClosed(IDBDatabase* aDatabase);

  // Responsible for clearing the database files for a particular origin on the
  // IO thread. Created when nsIIDBIndexedDatabaseManager::ClearDatabasesForURI
  // is called. Runs three times, first on the main thread, next on the IO
  // thread, and then finally again on the main thread. While on the IO thread
  // the runnable will actually remove the origin's database files and the
  // directory that contains them before dispatching itself back to the main
  // thread. When back on the main thread the runnable will notify the
  // IndexedDatabaseManager that the job has been completed.
  class OriginClearRunnable MOZ_FINAL : public nsIRunnable
  {
    enum CallbackState {
      // Not yet run.
      Pending = 0,

      // Running on the main thread in the callback for OpenAllowed.
      OpenAllowed,

      // Running on the IO thread.
      IO,

      // Running on the main thread after all work is done.
      Complete
    };

  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIRUNNABLE

    OriginClearRunnable(const nsACString& aOrigin)
    : mOrigin(aOrigin),
      mCallbackState(Pending)
    { }

    void AdvanceState()
    {
      switch (mCallbackState) {
        case Pending:
          mCallbackState = OpenAllowed;
          return;
        case OpenAllowed:
          mCallbackState = IO;
          return;
        case IO:
          mCallbackState = Complete;
          return;
        default:
          NS_NOTREACHED("Can't advance past Complete!");
      }
    }

    static void InvalidateOpenedDatabases(
                                   nsTArray<nsRefPtr<IDBDatabase> >& aDatabases,
                                   void* aClosure);

  private:
    nsCString mOrigin;
    CallbackState mCallbackState;
  };

  // Responsible for calculating the amount of space taken up by databases of a
  // certain origin. Created when nsIIDBIndexedDatabaseManager::GetUsageForURI
  // is called. May be canceled with
  // nsIIDBIndexedDatabaseManager::CancelGetUsageForURI. Runs twice, first on
  // the IO thread, then again on the main thread. While on the IO thread the
  // runnable will calculate the size of all files in the origin's directory
  // before dispatching itself back to the main thread. When on the main thread
  // the runnable will call the callback and then notify the
  // IndexedDatabaseManager that the job has been completed.
  class AsyncUsageRunnable MOZ_FINAL : public nsIRunnable
  {
    enum CallbackState {
      // Not yet run.
      Pending = 0,

      // Running on the main thread in the callback for OpenAllowed.
      OpenAllowed,

      // Running on the IO thread.
      IO,

      // Running on the main thread after all work is done.
      Complete,

      // Running on the main thread after skipping the work
      Shortcut
    };
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIRUNNABLE

    AsyncUsageRunnable(nsIURI* aURI,
                       const nsACString& aOrigin,
                       nsIIndexedDatabaseUsageCallback* aCallback);

    // Sets the canceled flag so that the callback is never called.
    void Cancel();

    void AdvanceState()
    {
      switch (mCallbackState) {
        case Pending:
          mCallbackState = OpenAllowed;
          return;
        case OpenAllowed:
          mCallbackState = IO;
          return;
        case IO:
          mCallbackState = Complete;
          return;
        default:
          NS_NOTREACHED("Can't advance past Complete!");
      }
    }

    nsresult TakeShortcut();

    // Run calls the RunInternal method and makes sure that we always dispatch
    // to the main thread in case of an error.
    inline nsresult RunInternal();

    nsresult GetUsageForDirectory(nsIFile* aDirectory,
                                  PRUint64* aUsage);

    nsCOMPtr<nsIURI> mURI;
    nsCString mOrigin;

    nsCOMPtr<nsIIndexedDatabaseUsageCallback> mCallback;
    PRUint64 mUsage;
    PRUint64 mFileUsage;
    PRInt32 mCanceled;
    CallbackState mCallbackState;
  };

  // Called when AsyncUsageRunnable has finished its Run() method.
  inline void OnUsageCheckComplete(AsyncUsageRunnable* aRunnable);

  // A struct that contains the information corresponding to a pending or
  // running operation that requires synchronization (e.g. opening a db,
  // clearing dbs for an origin, etc).
  struct SynchronizedOp
  {
    SynchronizedOp(const nsACString& aOrigin, nsIAtom* aId);
    ~SynchronizedOp();

    // Test whether the second SynchronizedOp needs to get behind this one.
    bool MustWaitFor(const SynchronizedOp& aRhs) const;

    void DelayRunnable(nsIRunnable* aRunnable);
    void DispatchDelayedRunnables();

    const nsCString mOrigin;
    nsCOMPtr<nsIAtom> mId;
    nsRefPtr<AsyncConnectionHelper> mHelper;
    nsCOMPtr<nsIRunnable> mRunnable;
    nsTArray<nsCOMPtr<nsIRunnable> > mDelayedRunnables;
    nsTArray<nsRefPtr<IDBDatabase> > mDatabases;
  };

  // A callback runnable used by the TransactionPool when it's safe to proceed
  // with a SetVersion/DeleteDatabase/etc.
  class WaitForTransactionsToFinishRunnable MOZ_FINAL : public nsIRunnable
  {
  public:
    WaitForTransactionsToFinishRunnable(SynchronizedOp* aOp,
                                        PRUint32 aCountdown)
    : mOp(aOp), mCountdown(aCountdown)
    {
      NS_ASSERTION(mOp, "Why don't we have a runnable?");
      NS_ASSERTION(mOp->mDatabases.IsEmpty(), "We're here too early!");
      NS_ASSERTION(mOp->mHelper || mOp->mRunnable,
                   "What are we supposed to do when we're done?");
      NS_ASSERTION(mCountdown, "Wrong countdown!");
    }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIRUNNABLE

  private:
    // The IndexedDatabaseManager holds this alive.
    SynchronizedOp* mOp;
    PRUint32 mCountdown;
  };

  class WaitForLockedFilesToFinishRunnable MOZ_FINAL : public nsIRunnable
  {
  public:
    WaitForLockedFilesToFinishRunnable()
    : mBusy(true)
    { }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIRUNNABLE

    bool IsBusy() const
    {
      return mBusy;
    }

  private:
    bool mBusy;
  };

  class AsyncDeleteFileRunnable MOZ_FINAL : public nsIRunnable
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIRUNNABLE
    AsyncDeleteFileRunnable(const nsAString& aFilePath)
    : mFilePath(aFilePath)
    { }

  private:
    nsString mFilePath;
  };

  static nsresult RunSynchronizedOp(IDBDatabase* aDatabase,
                                    SynchronizedOp* aOp);

  SynchronizedOp* FindSynchronizedOp(const nsACString& aOrigin,
                                     nsIAtom* aId)
  {
    for (PRUint32 index = 0; index < mSynchronizedOps.Length(); index++) {
      const nsAutoPtr<SynchronizedOp>& currentOp = mSynchronizedOps[index];
      if (currentOp->mOrigin == aOrigin &&
          (!currentOp->mId || currentOp->mId == aId)) {
        return currentOp;
      }
    }
    return nullptr;
  }

  bool IsClearOriginPending(const nsACString& aOrigin)
  {
    return !!FindSynchronizedOp(aOrigin, nullptr);
  }

  // Maintains a list of live databases per origin.
  nsClassHashtable<nsCStringHashKey, nsTArray<IDBDatabase*> > mLiveDatabases;

  // TLS storage index for the current thread's window
  PRUintn mCurrentWindowIndex;

  // Lock protecting mQuotaHelperHash
  mozilla::Mutex mQuotaHelperMutex;

  // A map of Windows to the corresponding quota helper.
  nsRefPtrHashtable<nsPtrHashKey<nsPIDOMWindow>, CheckQuotaHelper> mQuotaHelperHash;

  // Maintains a list of all file managers per origin. The list is actually also
  // a list of all origins that were successfully initialized. This list
  // isn't protected by any mutex but it is only ever touched on the IO thread.
  nsClassHashtable<nsCStringHashKey,
                   nsTArray<nsRefPtr<FileManager> > > mFileManagers;

  // Maintains a list of origins that we're currently enumerating to gather
  // usage statistics.
  nsAutoTArray<nsRefPtr<AsyncUsageRunnable>, 1> mUsageRunnables;

  // Maintains a list of synchronized operatons that are in progress or queued.
  nsAutoTArray<nsAutoPtr<SynchronizedOp>, 5> mSynchronizedOps;

  // Thread on which IO is performed.
  nsCOMPtr<nsIThread> mIOThread;

  // A timer that gets activated at shutdown to ensure we close all databases.
  nsCOMPtr<nsITimer> mShutdownTimer;

  // A single threadsafe instance of our quota callback. Created on the main
  // thread during GetOrCreate().
  nsCOMPtr<mozIStorageQuotaCallback> mQuotaCallbackSingleton;

  // Lock protecting FileManager.mFileInfos and nsDOMFileBase.mFileInfos
  // It's s also used to atomically update FileInfo.mRefCnt, FileInfo.mDBRefCnt
  // and FileInfo.mSliceRefCnt
  mozilla::Mutex mFileMutex;

  nsString mDatabaseBasePath;

  static bool sIsMainProcess;
};

class AutoEnterWindow
{
public:
  AutoEnterWindow(nsPIDOMWindow* aWindow)
  {
    IndexedDatabaseManager::SetCurrentWindow(aWindow);
  }

  ~AutoEnterWindow()
  {
    IndexedDatabaseManager::SetCurrentWindow(nullptr);
  }
};

END_INDEXEDDB_NAMESPACE

#endif /* mozilla_dom_indexeddb_indexeddatabasemanager_h__ */
