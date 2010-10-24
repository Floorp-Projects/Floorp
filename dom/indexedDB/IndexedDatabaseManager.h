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

#include "nsIIndexedDatabaseManager.h"
#include "nsIObserver.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "nsIURI.h"

#include "nsClassHashtable.h"
#include "nsHashKeys.h"

#define INDEXEDDB_MANAGER_CONTRACTID "@mozilla.org/dom/indexeddb/manager;1"

class nsITimer;

BEGIN_INDEXEDDB_NAMESPACE

class AsyncConnectionHelper;

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
  // complete before dispatching the give runnable.
  nsresult WaitForOpenAllowed(const nsAString& aName,
                              const nsACString& aOrigin,
                              nsIRunnable* aRunnable);

  nsIThread* IOThread()
  {
    NS_ASSERTION(mIOThread, "This should never be null!");
    return mIOThread;
  }

  // Returns true if we've begun the shutdown process.
  static bool IsShuttingDown();

  // Begins the process of setting a database version.
  nsresult SetDatabaseVersion(IDBDatabase* aDatabase,
                              IDBVersionChangeRequest* aRequest,
                              const nsAString& aVersion,
                              AsyncConnectionHelper* aHelper);

  // Called when a window is being purged from the bfcache in order to force any
  // live database objects to close themselves.
  void CloseDatabasesForWindow(nsPIDOMWindow* aWindow);

private:
  IndexedDatabaseManager();
  ~IndexedDatabaseManager();

  // Called when a database is created.
  bool RegisterDatabase(IDBDatabase* aDatabase);

  // Called when a database is being unlinked or destroyed.
  void UnregisterDatabase(IDBDatabase* aDatabase);

  // Called when a database has been closed.
  void OnDatabaseClosed(IDBDatabase* aDatabase);

  // Called when a version change transaction can run immediately.
  void RunSetVersionTransaction(IDBDatabase* aDatabase)
  {
    OnDatabaseClosed(aDatabase);
  }

  // Responsible for clearing the database files for a particular origin on the
  // IO thread. Created when nsIIDBIndexedDatabaseManager::ClearDatabasesForURI
  // is called. Runs three times, first on the main thread, next on the IO
  // thread, and then finally again on the main thread. While on the IO thread
  // the runnable will actually remove the origin's database files and the
  // directory that contains them before dispatching itself back to the main
  // thread. When on the main thread the runnable will dispatch any queued
  // runnables and then notify the IndexedDatabaseManager that the job has been
  // completed.
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
    nsTArray<nsCOMPtr<nsIRunnable> > mDelayedRunnables;
    bool mFirstCallback;
  };

  // Called when OriginClearRunnable has finished its Run() method.
  inline void OnOriginClearComplete(OriginClearRunnable* aRunnable);

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

  // Responsible for waiting until all databases have been closed before running
  // the version change transaction. Created when
  // IndexedDatabaseManager::SetDatabaseVersion is called. Runs only once on the
  // main thread when the version change transaction has completed.
  class SetVersionRunnable : public nsIRunnable
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIRUNNABLE

    SetVersionRunnable(IDBDatabase* aDatabase,
                       nsTArray<nsRefPtr<IDBDatabase> >& aDatabases);
    ~SetVersionRunnable();

    nsRefPtr<IDBDatabase> mRequestingDatabase;
    nsTArray<nsRefPtr<IDBDatabase> > mDatabases;
    nsRefPtr<AsyncConnectionHelper> mHelper;
    nsTArray<nsCOMPtr<nsIRunnable> > mDelayedRunnables;
  };

  // Called when SetVersionRunnable has finished its Run() method.
  inline void OnSetVersionRunnableComplete(SetVersionRunnable* aRunnable);

  // Maintains a list of live databases per origin.
  nsClassHashtable<nsCStringHashKey, nsTArray<IDBDatabase*> > mLiveDatabases;

  // Maintains a list of origins that are currently being cleared.
  nsAutoTArray<nsRefPtr<OriginClearRunnable>, 1> mOriginClearRunnables;

  // Maintains a list of origins that we're currently enumerating to gather
  // usage statistics.
  nsAutoTArray<nsRefPtr<AsyncUsageRunnable>, 1> mUsageRunnables;

  // Maintains a list of SetVersion calls that are in progress.
  nsAutoTArray<nsRefPtr<SetVersionRunnable>, 1> mSetVersionRunnables;

  // Thread on which IO is performed.
  nsCOMPtr<nsIThread> mIOThread;

  // A timer that gets activated at shutdown to ensure we close all databases.
  nsCOMPtr<nsITimer> mShutdownTimer;
};

END_INDEXEDDB_NAMESPACE

#endif /* mozilla_dom_indexeddb_indexeddatabasemanager_h__ */
