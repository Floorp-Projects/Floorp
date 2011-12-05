/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Indexed Database.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef mozilla_dom_indexeddb_indexeddatabasemanager_h__
#define mozilla_dom_indexeddb_indexeddatabasemanager_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"
#include "mozilla/dom/indexedDB/IDBDatabase.h"
#include "mozilla/dom/indexedDB/IDBRequest.h"

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
class nsITimer;

BEGIN_INDEXEDDB_NAMESPACE

class AsyncConnectionHelper;

class CheckQuotaHelper;

class IndexedDatabaseManager : public nsIIndexedDatabaseManager,
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

  typedef void (*WaitingOnDatabasesCallback)(nsTArray<nsRefPtr<IDBDatabase> >&, void*);

  // Acquire exclusive access to the database given (waits for all others to
  // close).  If databases need to close first, the callback will be invoked
  // with an array of said databases.
  nsresult AcquireExclusiveAccess(IDBDatabase* aDatabase,
                                  AsyncConnectionHelper* aHelper,
                                  WaitingOnDatabasesCallback aCallback,
                                  void* aClosure)
  {
    NS_ASSERTION(aDatabase, "Need a DB here!");
    return AcquireExclusiveAccess(aDatabase->Origin(), aDatabase, aHelper,
                                  aCallback, aClosure);
  }
  nsresult AcquireExclusiveAccess(const nsACString& aOrigin, 
                                  AsyncConnectionHelper* aHelper,
                                  WaitingOnDatabasesCallback aCallback,
                                  void* aClosure)
  {
    return AcquireExclusiveAccess(aOrigin, nsnull, aHelper, aCallback,
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

  nsresult EnsureQuotaManagementForDirectory(nsIFile* aDirectory);

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

private:
  IndexedDatabaseManager();
  ~IndexedDatabaseManager();

  nsresult AcquireExclusiveAccess(const nsACString& aOrigin, 
                                  IDBDatabase* aDatabase,
                                  AsyncConnectionHelper* aHelper,
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
  class OriginClearRunnable : public nsIRunnable
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIRUNNABLE

    OriginClearRunnable(const nsACString& aOrigin,
                        nsIThread* aThread)
    : mOrigin(aOrigin),
      mThread(aThread),
      mFirstCallback(true)
    { }

    nsCString mOrigin;
    nsCOMPtr<nsIThread> mThread;
    bool mFirstCallback;
  };

  bool IsClearOriginPending(const nsACString& origin);

  // Responsible for calculating the amount of space taken up by databases of a
  // certain origin. Created when nsIIDBIndexedDatabaseManager::GetUsageForURI
  // is called. May be canceled with
  // nsIIDBIndexedDatabaseManager::CancelGetUsageForURI. Runs twice, first on
  // the IO thread, then again on the main thread. While on the IO thread the
  // runnable will calculate the size of all files in the origin's directory
  // before dispatching itself back to the main thread. When on the main thread
  // the runnable will call the callback and then notify the
  // IndexedDatabaseManager that the job has been completed.
  class AsyncUsageRunnable : public nsIRunnable
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIRUNNABLE

    AsyncUsageRunnable(nsIURI* aURI,
                       const nsACString& aOrigin,
                       nsIIndexedDatabaseUsageCallback* aCallback);

    // Sets the canceled flag so that the callback is never called.
    void Cancel();

    // Run calls the RunInternal method and makes sure that we always dispatch
    // to the main thread in case of an error.
    inline nsresult RunInternal();

    nsCOMPtr<nsIURI> mURI;
    nsCString mOrigin;
    nsCOMPtr<nsIIndexedDatabaseUsageCallback> mCallback;
    PRUint64 mUsage;
    PRInt32 mCanceled;
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
    nsTArray<nsCOMPtr<nsIRunnable> > mDelayedRunnables;
    nsTArray<nsRefPtr<IDBDatabase> > mDatabases;
  };

  // A callback runnable used by the TransactionPool when it's safe to proceed
  // with a SetVersion/DeleteDatabase/etc.
  class WaitForTransactionsToFinishRunnable : public nsIRunnable
  {
  public:
    WaitForTransactionsToFinishRunnable(SynchronizedOp* aOp)
    : mOp(aOp)
    {
      NS_ASSERTION(mOp, "Why don't we have a runnable?");
      NS_ASSERTION(mOp->mDatabases.IsEmpty(), "We're here too early!");
      NS_ASSERTION(mOp->mHelper, "What are we supposed to do when we're done?");
    }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIRUNNABLE

  private:
    // The IndexedDatabaseManager holds this alive.
    SynchronizedOp* mOp;
  };

  static nsresult DispatchHelper(AsyncConnectionHelper* aHelper);

  // Maintains a list of live databases per origin.
  nsClassHashtable<nsCStringHashKey, nsTArray<IDBDatabase*> > mLiveDatabases;

  // TLS storage index for the current thread's window
  PRUintn mCurrentWindowIndex;

  // Lock protecting mQuotaHelperHash
  mozilla::Mutex mQuotaHelperMutex;

  // A map of Windows to the corresponding quota helper.
  nsRefPtrHashtable<nsPtrHashKey<nsPIDOMWindow>, CheckQuotaHelper> mQuotaHelperHash;

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

  // A list of all paths that are under SQLite's quota tracking system. This
  // list isn't protected by any mutex but it is only ever touched on the IO
  // thread.
  nsTArray<nsCString> mTrackedQuotaPaths;
};

class AutoEnterWindow
{
public:
  AutoEnterWindow(nsPIDOMWindow* aWindow)
  {
    NS_ASSERTION(aWindow, "This should never be null!");
    IndexedDatabaseManager::SetCurrentWindow(aWindow);
  }

  ~AutoEnterWindow()
  {
    IndexedDatabaseManager::SetCurrentWindow(nsnull);
  }
};

END_INDEXEDDB_NAMESPACE

#endif /* mozilla_dom_indexeddb_indexeddatabasemanager_h__ */
