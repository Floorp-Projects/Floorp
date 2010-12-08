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

#include "IndexedDatabaseManager.h"

#include "nsIFile.h"
#include "nsIObserverService.h"
#include "nsISimpleEnumerator.h"
#include "nsITimer.h"

#include "mozilla/Services.h"
#include "nsContentUtils.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "nsXPCOMPrivate.h"

#include "AsyncConnectionHelper.h"
#include "IDBDatabase.h"
#include "IDBEvents.h"
#include "IDBFactory.h"
#include "LazyIdleThread.h"
#include "TransactionThreadPool.h"
#include "nsISHEntry.h"

// The amount of time, in milliseconds, that our IO thread will stay alive
// after the last event it processes.
#define DEFAULT_THREAD_TIMEOUT_MS 30000

// The amount of time, in milliseconds, that we will wait for active database
// transactions on shutdown before aborting them.
#define DEFAULT_SHUTDOWN_TIMER_MS 30000

USING_INDEXEDDB_NAMESPACE
using namespace mozilla::services;

namespace {

PRInt32 gShutdown = 0;

// Does not hold a reference.
IndexedDatabaseManager* gInstance = nsnull;

// Adds all databases in the hash to the given array.
PLDHashOperator
EnumerateToTArray(const nsACString& aKey,
                  nsTArray<IDBDatabase*>* aValue,
                  void* aUserArg)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aKey.IsEmpty(), "Empty key!");
  NS_ASSERTION(aValue, "Null pointer!");
  NS_ASSERTION(aUserArg, "Null pointer!");

  nsTArray<IDBDatabase*>* array =
    static_cast<nsTArray<IDBDatabase*>*>(aUserArg);

  if (!array->AppendElements(*aValue)) {
    NS_WARNING("Out of memory!");
    return PL_DHASH_STOP;
  }

  return PL_DHASH_NEXT;
}

// Responsible for calling IDBDatabase.setVersion after a pending version change
// transaction has completed.
class DelayedSetVersion : public nsRunnable
{
public:
  DelayedSetVersion(IDBDatabase* aDatabase,
                    IDBVersionChangeRequest* aRequest,
                    const nsAString& aVersion,
                    AsyncConnectionHelper* aHelper)
  : mDatabase(aDatabase),
    mRequest(aRequest),
    mVersion(aVersion),
    mHelper(aHelper)
  { }

  NS_IMETHOD Run()
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

    IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
    NS_ASSERTION(mgr, "This should never be null!");

    nsresult rv = mgr->SetDatabaseVersion(mDatabase, mRequest, mVersion,
                                          mHelper);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

private:
  nsRefPtr<IDBDatabase> mDatabase;
  nsRefPtr<IDBVersionChangeRequest> mRequest;
  nsString mVersion;
  nsRefPtr<AsyncConnectionHelper> mHelper;
};

// Responsible for firing "versionchange" events at all live and non-closed
// databases, and for firing a "blocked" event at the requesting database if any
// databases fail to close.
class VersionChangeEventsRunnable : public nsRunnable
{
public:
  VersionChangeEventsRunnable(
                            IDBDatabase* aRequestingDatabase,
                            IDBVersionChangeRequest* aRequest,
                            nsTArray<nsRefPtr<IDBDatabase> >& aWaitingDatabases,
                            const nsAString& aVersion)
  : mRequestingDatabase(aRequestingDatabase),
    mRequest(aRequest),
    mVersion(aVersion)
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    NS_ASSERTION(aRequestingDatabase, "Null pointer!");
    NS_ASSERTION(aRequest, "Null pointer!");

    if (!mWaitingDatabases.SwapElements(aWaitingDatabases)) {
      NS_ERROR("This should never fail!");
    }
  }

  NS_IMETHOD Run()
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

    // Fire version change events at all of the databases that are not already
    // closed. Also kick bfcached documents out of bfcache.
    for (PRUint32 index = 0; index < mWaitingDatabases.Length(); index++) {
      nsRefPtr<IDBDatabase>& database = mWaitingDatabases[index];

      if (database->IsClosed()) {
        continue;
      }

      // First check if the document the IDBDatabase is part of is bfcached
      nsCOMPtr<nsIDocument> ownerDoc = database->GetOwnerDocument();
      nsISHEntry* shEntry;
      if (ownerDoc && (shEntry = ownerDoc->GetBFCacheEntry())) {
        nsCOMPtr<nsISHEntryInternal> sheInternal = do_QueryInterface(shEntry);
        if (sheInternal) {
          sheInternal->RemoveFromBFCacheSync();
        }
        NS_ASSERTION(database->IsClosed(),
                     "Kicking doc out of bfcache should have closed database");
        continue;
      }

      // Otherwise fire a versionchange event.
      nsCOMPtr<nsIDOMEvent> event(IDBVersionChangeEvent::Create(mVersion));
      NS_ENSURE_TRUE(event, NS_ERROR_FAILURE);

      PRBool dummy;
      database->DispatchEvent(event, &dummy);
    }

    // Now check to see if any didn't close. If there are some running still
    // then fire the blocked event.
    for (PRUint32 index = 0; index < mWaitingDatabases.Length(); index++) {
      if (!mWaitingDatabases[index]->IsClosed()) {
        nsISupports* source =
          static_cast<nsPIDOMEventTarget*>(mRequestingDatabase);

        nsCOMPtr<nsIDOMEvent> event =
          IDBVersionChangeEvent::CreateBlocked(source, mVersion);
        NS_ENSURE_TRUE(event, NS_ERROR_FAILURE);

        PRBool dummy;
        mRequest->DispatchEvent(event, &dummy);

        break;
      }
    }

    return NS_OK;
  }

private:
  nsRefPtr<IDBDatabase> mRequestingDatabase;
  nsRefPtr<IDBVersionChangeRequest> mRequest;
  nsTArray<nsRefPtr<IDBDatabase> > mWaitingDatabases;
  nsString mVersion;
};

} // anonymous namespace

IndexedDatabaseManager::IndexedDatabaseManager()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!gInstance, "More than one instance!");
}

IndexedDatabaseManager::~IndexedDatabaseManager()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(gInstance == this, "Different instances!");
  gInstance = nsnull;
}

// static
already_AddRefed<IndexedDatabaseManager>
IndexedDatabaseManager::GetOrCreate()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (IsShuttingDown()) {
    NS_ERROR("Calling GetOrCreateInstance() after shutdown!");
    return nsnull;
  }

  nsRefPtr<IndexedDatabaseManager> instance(gInstance);

  if (!instance) {
    instance = new IndexedDatabaseManager();

    if (!instance->mLiveDatabases.Init()) {
      NS_WARNING("Out of memory!");
      return nsnull;
    }

    // Make a timer here to avoid potential failures later. We don't actually
    // initialize the timer until shutdown.
    instance->mShutdownTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
    NS_ENSURE_TRUE(instance->mShutdownTimer, nsnull);

    nsCOMPtr<nsIObserverService> obs = GetObserverService();
    NS_ENSURE_TRUE(obs, nsnull);

    // We need this callback to know when to shut down all our threads.
    nsresult rv = obs->AddObserver(instance, NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                   PR_FALSE);
    NS_ENSURE_SUCCESS(rv, nsnull);

    // We don't really need this callback but we want the observer service to
    // hold us alive until XPCOM shutdown. That way other consumers can continue
    // to use this service until shutdown.
    rv = obs->AddObserver(instance, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID,
                          PR_FALSE);
    NS_ENSURE_SUCCESS(rv, nsnull);

    // Make a lazy thread for any IO we need (like clearing or enumerating the
    // contents of indexedDB database directories).
    instance->mIOThread = new LazyIdleThread(DEFAULT_THREAD_TIMEOUT_MS,
                                             LazyIdleThread::ManualShutdown);

    // The observer service will hold our last reference, don't AddRef here.
    gInstance = instance;
  }

  return instance.forget();
}

// static
IndexedDatabaseManager*
IndexedDatabaseManager::Get()
{
  // Does not return an owning reference.
  return gInstance;
}

// static
IndexedDatabaseManager*
IndexedDatabaseManager::FactoryCreate()
{
  // Returns a raw pointer that carries an owning reference! Lame, but the
  // singleton factory macros force this.
  return GetOrCreate().get();
}

bool
IndexedDatabaseManager::RegisterDatabase(IDBDatabase* aDatabase)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aDatabase, "Null pointer!");

  // Don't allow any new databases to be created after shutdown.
  if (IsShuttingDown()) {
    return false;
  }

  // Add this database to its origin array if it exists, create it otherwise.
  nsTArray<IDBDatabase*>* array;
  if (!mLiveDatabases.Get(aDatabase->Origin(), &array)) {
    nsAutoPtr<nsTArray<IDBDatabase*> > newArray(new nsTArray<IDBDatabase*>());
    if (!mLiveDatabases.Put(aDatabase->Origin(), newArray)) {
      NS_WARNING("Out of memory?");
      return false;
    }
    array = newArray.forget();
  }
  if (!array->AppendElement(aDatabase)) {
    NS_WARNING("Out of memory?");
    return false;
  }

  aDatabase->mRegistered = true;
  return true;
}

void
IndexedDatabaseManager::UnregisterDatabase(IDBDatabase* aDatabase)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aDatabase, "Null pointer!");

  // Remove this database from its origin array, maybe remove the array if it
  // is then empty.
  nsTArray<IDBDatabase*>* array;
  if (mLiveDatabases.Get(aDatabase->Origin(), &array) &&
      array->RemoveElement(aDatabase)) {
    if (array->IsEmpty()) {
      mLiveDatabases.Remove(aDatabase->Origin());
    }
    return;
  }
  NS_ERROR("Didn't know anything about this database!");
}

void
IndexedDatabaseManager::OnOriginClearComplete(OriginClearRunnable* aRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aRunnable, "Null pointer!");
  NS_ASSERTION(!aRunnable->mThread, "Thread should be null!");
  NS_ASSERTION(aRunnable->mDelayedRunnables.IsEmpty(),
               "Delayed runnables should have been dispatched already!");

  if (!mOriginClearRunnables.RemoveElement(aRunnable)) {
    NS_ERROR("Don't know anything about this runnable!");
  }
}

void
IndexedDatabaseManager::OnUsageCheckComplete(AsyncUsageRunnable* aRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aRunnable, "Null pointer!");
  NS_ASSERTION(!aRunnable->mURI, "Should have been cleared!");
  NS_ASSERTION(!aRunnable->mCallback, "Should have been cleared!");

  if (!mUsageRunnables.RemoveElement(aRunnable)) {
    NS_ERROR("Don't know anything about this runnable!");
  }
}

void
IndexedDatabaseManager::OnSetVersionRunnableComplete(
                                                  SetVersionRunnable* aRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aRunnable, "Null pointer!");
  NS_ASSERTION(aRunnable->mDelayedRunnables.IsEmpty(),
               "Delayed runnables should have been dispatched already!");

  // Remove this runnable from the list. This will allow other databases to
  // begin to request version changes.
  if (!mSetVersionRunnables.RemoveElement(aRunnable)) {
    NS_ERROR("Don't know anything about this runnable!");
  }
}

nsresult
IndexedDatabaseManager::WaitForOpenAllowed(const nsAString& aName,
                                           const nsACString& aOrigin,
                                           nsIRunnable* aRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aName.IsEmpty(), "Empty name!");
  NS_ASSERTION(!aOrigin.IsEmpty(), "Empty origin!");
  NS_ASSERTION(aRunnable, "Null pointer!");

  // See if we're currently clearing database files for this origin. If so then
  // queue the runnable for later dispatch after we're done clearing.
  PRUint32 count = mOriginClearRunnables.Length();
  for (PRUint32 index = 0; index < count; index++) {
    nsRefPtr<OriginClearRunnable>& data = mOriginClearRunnables[index];
    if (data->mOrigin == aOrigin) {
      nsCOMPtr<nsIRunnable>* newPtr =
        data->mDelayedRunnables.AppendElement(aRunnable);
      NS_ENSURE_TRUE(newPtr, NS_ERROR_OUT_OF_MEMORY);

      return NS_OK;
    }
  }

  // Check to see if we're currently doing a SetVersion transaction for this
  // database. If so then we delay this runnable for later.
  for (PRUint32 index = 0; index < mSetVersionRunnables.Length(); index++) {
    nsRefPtr<SetVersionRunnable>& runnable = mSetVersionRunnables[index];
    if (runnable->mRequestingDatabase->Name() == aName &&
        runnable->mRequestingDatabase->Origin() == aOrigin) {
      nsCOMPtr<nsIRunnable>* newPtr =
        runnable->mDelayedRunnables.AppendElement(aRunnable);
      NS_ENSURE_TRUE(newPtr, NS_ERROR_OUT_OF_MEMORY);

      return NS_OK;
    }
  }

  // We aren't currently clearing databases for this origin and we're not
  // running a SetVersion transaction for this database so dispatch the runnable
  // immediately.
  return NS_DispatchToCurrentThread(aRunnable);
}

// static
bool
IndexedDatabaseManager::IsShuttingDown()
{
  return !!gShutdown;
}

nsresult
IndexedDatabaseManager::SetDatabaseVersion(IDBDatabase* aDatabase,
                                           IDBVersionChangeRequest* aRequest,
                                           const nsAString& aVersion,
                                           AsyncConnectionHelper* aHelper)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aDatabase, "Null pointer!");
  NS_ASSERTION(aHelper, "Null pointer!");

  nsresult rv;

  // See if another database has already asked to change the version.
  for (PRUint32 index = 0; index < mSetVersionRunnables.Length(); index++) {
    nsRefPtr<SetVersionRunnable>& runnable = mSetVersionRunnables[index];
    if (runnable->mRequestingDatabase->Id() == aDatabase->Id()) {
      if (runnable->mRequestingDatabase == aDatabase) {
        // Same database, just queue this call to run after the current
        // SetVersion transaction completes.
        nsRefPtr<DelayedSetVersion> delayed =
          new DelayedSetVersion(aDatabase, aRequest, aVersion, aHelper);
        if (!runnable->mDelayedRunnables.AppendElement(delayed)) {
          NS_WARNING("Out of memory!");
          return NS_ERROR_OUT_OF_MEMORY;
        }
        return NS_OK;
      }

      // Different database, we can't let this one succeed.
      aHelper->SetError(NS_ERROR_DOM_INDEXEDDB_DEADLOCK_ERR);

      rv = NS_DispatchToCurrentThread(aHelper);
      NS_ENSURE_SUCCESS(rv, rv);

      return NS_OK;
    }
  }

  // Grab all live databases for the same origin.
  nsTArray<IDBDatabase*>* array;
  if (!mLiveDatabases.Get(aDatabase->Origin(), &array)) {
    NS_ERROR("Must have some alive if we've got a live argument!");
  }

  // Hold on to all database objects that represent the same database file
  // (except the one that is requesting this version change).
  nsTArray<nsRefPtr<IDBDatabase> > liveDatabases;

  for (PRUint32 index = 0; index < array->Length(); index++) {
    IDBDatabase*& database = array->ElementAt(index);
    if (database != aDatabase &&
        database->Id() == aDatabase->Id() &&
        !database->IsClosed() &&
        !liveDatabases.AppendElement(database)) {
      NS_WARNING("Out of memory?");
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  // Adding an element to this array here will keep other databases from
  // requesting a version change.
  nsRefPtr<SetVersionRunnable> runnable =
    new SetVersionRunnable(aDatabase, liveDatabases);
  if (!mSetVersionRunnables.AppendElement(runnable)) {
    NS_WARNING("Out of memory!");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ASSERTION(liveDatabases.IsEmpty(), "Should have swapped!");

  // When all databases are closed we want to dispatch the SetVersion
  // transaction to the transaction pool.
  runnable->mHelper = aHelper;

  if (runnable->mDatabases.IsEmpty()) {
    // There are no other databases that need to be closed. Go ahead and run
    // the transaction now.
    RunSetVersionTransaction(aDatabase);
  }
  else {
    // Otherwise we need to wait for all the other databases to complete.
    // Schedule a version change events runnable .
    nsTArray<nsRefPtr<IDBDatabase> > waitingDatabases;
    if (!waitingDatabases.AppendElements(runnable->mDatabases)) {
      NS_WARNING("Out of memory!");
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsRefPtr<VersionChangeEventsRunnable> eventsRunnable =
      new VersionChangeEventsRunnable(aDatabase, aRequest, waitingDatabases,
                                      aVersion);

    rv = NS_DispatchToCurrentThread(eventsRunnable);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

void
IndexedDatabaseManager::CloseDatabasesForWindow(nsPIDOMWindow* aWindow)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aWindow, "Null pointer!");

  nsAutoTArray<IDBDatabase*, 50> liveDatabases;
  mLiveDatabases.EnumerateRead(EnumerateToTArray, &liveDatabases);

  for (PRUint32 index = 0; index < liveDatabases.Length(); index++) {
    IDBDatabase*& database = liveDatabases[index];
    if (database->Owner() == aWindow && NS_FAILED(database->Close())) {
      NS_WARNING("Failed to close database for dying window!");
    }
  }
}

void
IndexedDatabaseManager::OnDatabaseClosed(IDBDatabase* aDatabase)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aDatabase, "Null pointer!");

  // Check through the list of SetVersionRunnables we have amassed to see if
  // this database is part of a SetVersion callback.
  for (PRUint32 index = 0; index < mSetVersionRunnables.Length(); index++) {
    nsRefPtr<SetVersionRunnable>& runnable = mSetVersionRunnables[index];

    if (runnable->mRequestingDatabase->Id() == aDatabase->Id()) {
      // This is the SetVersionRunnable for the given database file. Remove the
      // database from the list of databases that need to be closed. Since we
      // use this hook for SetVersion requests that don't actually need to wait
      // for other databases the mDatabases array may be empty.
      if (!runnable->mDatabases.IsEmpty() &&
          !runnable->mDatabases.RemoveElement(aDatabase)) {
        NS_ERROR("Didn't have this database in our list!");
      }

      // Now run the helper if there are no more live databases.
      if (runnable->mHelper && runnable->mDatabases.IsEmpty()) {
        // Don't hold the callback alive longer than necessary.
        nsRefPtr<AsyncConnectionHelper> helper;
        helper.swap(runnable->mHelper);

        if (NS_FAILED(helper->DispatchToTransactionPool())) {
          NS_WARNING("Failed to dispatch to thread pool!");
        }

        // Now wait for the transaction to complete. Completing the transaction
        // will be our cue to remove the SetVersionRunnable from our list and
        // therefore allow other SetVersion requests to begin.
        TransactionThreadPool* pool = TransactionThreadPool::Get();
        NS_ASSERTION(pool, "This should never be null!");

        // All other databases should be closed, so we only need to wait on this
        // one.
        nsAutoTArray<nsRefPtr<IDBDatabase>, 1> array;
        if (!array.AppendElement(aDatabase)) {
          NS_ERROR("This should never fail!");
        }

        // Use the SetVersionRunnable as the callback.
        if (!pool->WaitForAllDatabasesToComplete(array, runnable)) {
          NS_WARNING("Failed to wait for transaction to complete!");
        }
      }
      break;
    }
  }
}

NS_IMPL_ISUPPORTS2(IndexedDatabaseManager, nsIIndexedDatabaseManager,
                                           nsIObserver)

NS_IMETHODIMP
IndexedDatabaseManager::GetUsageForURI(
                                     nsIURI* aURI,
                                     nsIIndexedDatabaseUsageCallback* aCallback)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aCallback);

  // Figure out which origin we're dealing with.
  nsCString origin;
  nsresult rv = nsContentUtils::GetASCIIOrigin(aURI, origin);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<AsyncUsageRunnable> runnable =
    new AsyncUsageRunnable(aURI, origin, aCallback);

  nsRefPtr<AsyncUsageRunnable>* newRunnable =
    mUsageRunnables.AppendElement(runnable);
  NS_ENSURE_TRUE(newRunnable, NS_ERROR_OUT_OF_MEMORY);

  // Non-standard URIs can't create databases anyway so fire the callback
  // immediately.
  if (origin.EqualsLiteral("null")) {
    rv = NS_DispatchToCurrentThread(runnable);
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }

  // See if we're currently clearing the databases for this origin. If so then
  // we pretend that we've already deleted everything.
  for (PRUint32 index = 0; index < mOriginClearRunnables.Length(); index++) {
    if (mOriginClearRunnables[index]->mOrigin == origin) {
      rv = NS_DispatchToCurrentThread(runnable);
      NS_ENSURE_SUCCESS(rv, rv);
      return NS_OK;
    }
  }

  // Otherwise dispatch to the IO thread to actually compute the usage.
  rv = mIOThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
IndexedDatabaseManager::CancelGetUsageForURI(
                                     nsIURI* aURI,
                                     nsIIndexedDatabaseUsageCallback* aCallback)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aCallback);

  // See if one of our pending callbacks matches both the URI and the callback
  // given. Cancel an remove it if so.
  for (PRUint32 index = 0; index < mUsageRunnables.Length(); index++) {
    nsRefPtr<AsyncUsageRunnable>& runnable = mUsageRunnables[index];

    PRBool equals;
    nsresult rv = runnable->mURI->Equals(aURI, &equals);
    NS_ENSURE_SUCCESS(rv, rv);

    if (equals && SameCOMIdentity(aCallback, runnable->mCallback)) {
      runnable->Cancel();
      break;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
IndexedDatabaseManager::ClearDatabasesForURI(nsIURI* aURI)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  NS_ENSURE_ARG_POINTER(aURI);

  // Figure out which origin we're dealing with.
  nsCString origin;
  nsresult rv = nsContentUtils::GetASCIIOrigin(aURI, origin);
  NS_ENSURE_SUCCESS(rv, rv);

  // Non-standard URIs can't create databases anyway, so return immediately.
  if (origin.EqualsLiteral("null")) {
    return NS_OK;
  }

  // If we're already clearing out files for this origin then return
  // immediately.
  PRUint32 clearDataCount = mOriginClearRunnables.Length();
  for (PRUint32 index = 0; index < clearDataCount; index++) {
    if (mOriginClearRunnables[index]->mOrigin == origin) {
      return NS_OK;
    }
  }

  // We need to grab references to any live databases here to prevent them from
  // dying while we invalidate them.
  nsTArray<nsRefPtr<IDBDatabase> > liveDatabases;

  // Grab all live databases for this origin.
  nsTArray<IDBDatabase*>* array;
  if (mLiveDatabases.Get(origin, &array)) {
    if (!liveDatabases.AppendElements(*array)) {
      NS_WARNING("Out of memory?");
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  nsRefPtr<OriginClearRunnable> runnable =
    new OriginClearRunnable(origin, mIOThread);

  // Make a new entry for this origin in mOriginClearRunnables.
  nsRefPtr<OriginClearRunnable>* newRunnable =
    mOriginClearRunnables.AppendElement(runnable);
  NS_ENSURE_TRUE(newRunnable, NS_ERROR_OUT_OF_MEMORY);

  if (liveDatabases.IsEmpty()) {
    rv = runnable->Run();
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  // Invalidate all the live databases first.
  for (PRUint32 index = 0; index < liveDatabases.Length(); index++) {
    liveDatabases[index]->Invalidate();
  }

  // Now set up our callback so that we know when they have finished.
  TransactionThreadPool* pool = TransactionThreadPool::GetOrCreate();
  NS_ENSURE_TRUE(pool, NS_ERROR_FAILURE);

  if (!pool->WaitForAllDatabasesToComplete(liveDatabases, runnable)) {
    NS_WARNING("Can't wait on databases!");
    return NS_ERROR_FAILURE;
  }

  NS_ASSERTION(liveDatabases.IsEmpty(), "Should have swapped!");

  return NS_OK;
}

NS_IMETHODIMP
IndexedDatabaseManager::Observe(nsISupports* aSubject,
                                const char* aTopic,
                                const PRUnichar* aData)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID)) {
    // Setting this flag prevents the service from being recreated and prevents
    // further databases from being created.
    if (PR_AtomicSet(&gShutdown, 1)) {
      NS_ERROR("Shutdown more than once?!");
    }

    // Make sure to join with our IO thread.
    if (NS_FAILED(mIOThread->Shutdown())) {
      NS_WARNING("Failed to shutdown IO thread!");
    }

    // Kick off the shutdown timer.
    if (NS_FAILED(mShutdownTimer->Init(this, DEFAULT_SHUTDOWN_TIMER_MS,
                                       nsITimer::TYPE_ONE_SHOT))) {
      NS_WARNING("Failed to initialize shutdown timer!");
    }

    // This will spin the event loop while we wait on all the database threads
    // to close. Our timer may fire during that loop.
    TransactionThreadPool::Shutdown();

    // Cancel the timer regardless of whether it actually fired.
    if (NS_FAILED(mShutdownTimer->Cancel())) {
      NS_WARNING("Failed to cancel shutdown timer!");
    }

    return NS_OK;
  }

  if (!strcmp(aTopic, NS_TIMER_CALLBACK_TOPIC)) {
    NS_WARNING("Some database operations are taking longer than expected "
               "during shutdown and will be aborted!");

    // Grab all live databases, for all origins.
    nsAutoTArray<IDBDatabase*, 50> liveDatabases;
    mLiveDatabases.EnumerateRead(EnumerateToTArray, &liveDatabases);

    // Invalidate them all.
    if (!liveDatabases.IsEmpty()) {
      PRUint32 count = liveDatabases.Length();
      for (PRUint32 index = 0; index < count; index++) {
        liveDatabases[index]->Invalidate();
      }
    }

    return NS_OK;
  }

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    // We're dying now.
    return NS_OK;
  }

  NS_NOTREACHED("Unknown topic!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(IndexedDatabaseManager::OriginClearRunnable,
                              nsIRunnable)

NS_IMETHODIMP
IndexedDatabaseManager::OriginClearRunnable::Run()
{
  if (NS_IsMainThread()) {
    // On the first time on the main thread we simply dispatch to the IO thread.
    if (mFirstCallback) {
      NS_ASSERTION(mThread, "Should have a thread here!");

      mFirstCallback = false;

      // Dispatch to the IO thread.
      if (NS_FAILED(mThread->Dispatch(this, NS_DISPATCH_NORMAL))) {
        NS_WARNING("Failed to dispatch to IO thread!");
        return NS_ERROR_FAILURE;
      }

      // Don't need this any longer.
      mThread = nsnull;
      return NS_OK;
    }

    NS_ASSERTION(!mThread, "Should have been cleared already!");

    // Dispatch any queued runnables that we collected while we were waiting.
    for (PRUint32 index = 0; index < mDelayedRunnables.Length(); index++) {
      if (NS_FAILED(NS_DispatchToCurrentThread(mDelayedRunnables[index]))) {
        NS_WARNING("Failed to dispatch delayed runnable!");
      }
    }
    mDelayedRunnables.Clear();

    // Tell the IndexedDatabaseManager that we're done.
    IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
    if (mgr) {
      mgr->OnOriginClearComplete(this);
    }

    return NS_OK;
  }

  NS_ASSERTION(!mThread, "Should have been cleared already!");

  // Remove the directory that contains all our databases.
  nsCOMPtr<nsIFile> directory;
  nsresult rv = IDBFactory::GetDirectoryForOrigin(mOrigin,
                                                  getter_AddRefs(directory));
  if (NS_SUCCEEDED(rv)) {
    PRBool exists;
    rv = directory->Exists(&exists);
    if (NS_SUCCEEDED(rv) && exists) {
      rv = directory->Remove(PR_TRUE);
    }
  }
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Failed to remove directory!");

  // Switch back to the main thread to complete the sequence.
  rv = NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

IndexedDatabaseManager::AsyncUsageRunnable::AsyncUsageRunnable(
                                     nsIURI* aURI,
                                     const nsACString& aOrigin,
                                     nsIIndexedDatabaseUsageCallback* aCallback)
: mURI(aURI),
  mOrigin(aOrigin),
  mCallback(aCallback),
  mUsage(0),
  mCanceled(0)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aURI, "Null pointer!");
  NS_ASSERTION(!aOrigin.IsEmpty(), "Empty origin!");
  NS_ASSERTION(aCallback, "Null pointer!");
}

void
IndexedDatabaseManager::AsyncUsageRunnable::Cancel()
{
  if (PR_AtomicSet(&mCanceled, 1)) {
    NS_ERROR("Canceled more than once?!");
  }
}

nsresult
IndexedDatabaseManager::AsyncUsageRunnable::RunInternal()
{
  if (NS_IsMainThread()) {
    // Call the callback unless we were canceled.
    if (!mCanceled) {
      mCallback->OnUsageResult(mURI, mUsage);
    }

    // Clean up.
    mURI = nsnull;
    mCallback = nsnull;

    // And tell the IndexedDatabaseManager that we're done.
    IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
    if (mgr) {
      mgr->OnUsageCheckComplete(this);
    }

    return NS_OK;
  }

  if (mCanceled) {
    return NS_OK;
  }

  // Get the directory that contains all the database files we care about.
  nsCOMPtr<nsIFile> directory;
  nsresult rv = IDBFactory::GetDirectoryForOrigin(mOrigin,
                                                  getter_AddRefs(directory));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool exists;
  rv = directory->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  // If the directory exists then enumerate all the files inside, adding up the
  // sizes to get the final usage statistic.
  if (exists && !mCanceled) {
    nsCOMPtr<nsISimpleEnumerator> entries;
    rv = directory->GetDirectoryEntries(getter_AddRefs(entries));
    NS_ENSURE_SUCCESS(rv, rv);

    if (entries) {
      PRBool hasMore;
      while (NS_SUCCEEDED((rv = entries->HasMoreElements(&hasMore))) &&
             hasMore && !mCanceled) {
        nsCOMPtr<nsISupports> entry;
        rv = entries->GetNext(getter_AddRefs(entry));
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIFile> file(do_QueryInterface(entry));
        NS_ASSERTION(file, "Don't know what this is!");

        PRInt64 fileSize;
        rv = file->GetFileSize(&fileSize);
        NS_ENSURE_SUCCESS(rv, rv);

        NS_ASSERTION(fileSize > 0, "Negative size?!");

        // Watch for overflow!
        if (NS_UNLIKELY((LL_MAXINT - mUsage) <= PRUint64(fileSize))) {
          NS_WARNING("Database sizes exceed max we can report!");
          mUsage = LL_MAXINT;
        }
        else {
          mUsage += fileSize;
        }
      }
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(IndexedDatabaseManager::AsyncUsageRunnable,
                              nsIRunnable)

NS_IMETHODIMP
IndexedDatabaseManager::AsyncUsageRunnable::Run()
{
  nsresult rv = RunInternal();

  if (!NS_IsMainThread()) {
    if (NS_FAILED(rv)) {
      mUsage = 0;
    }

    if (NS_FAILED(NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL))) {
      NS_WARNING("Failed to dispatch to main thread!");
    }
  }

  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

IndexedDatabaseManager::SetVersionRunnable::SetVersionRunnable(
                                   IDBDatabase* aDatabase,
                                   nsTArray<nsRefPtr<IDBDatabase> >& aDatabases)
: mRequestingDatabase(aDatabase)
{
  NS_ASSERTION(aDatabase, "Null database!");
  if (!mDatabases.SwapElements(aDatabases)) {
    NS_ERROR("This should never fail!");
  }
}

IndexedDatabaseManager::SetVersionRunnable::~SetVersionRunnable()
{
}

NS_IMPL_ISUPPORTS1(IndexedDatabaseManager::SetVersionRunnable, nsIRunnable)

NS_IMETHODIMP
IndexedDatabaseManager::SetVersionRunnable::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!mHelper, "Should have been cleared already!");

  // Dispatch any queued runnables that we picked up while waiting for the
  // SetVersion transaction to complete.
  for (PRUint32 index = 0; index < mDelayedRunnables.Length(); index++) {
    if (NS_FAILED(NS_DispatchToCurrentThread(mDelayedRunnables[index]))) {
      NS_WARNING("Failed to dispatch delayed runnable!");
    }
  }

  // No need to hold these alive any longer.
  mDelayedRunnables.Clear();

  IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
  NS_ASSERTION(mgr, "This should never be null!");

  // Let the IndexedDatabaseManager know that the SetVersion transaction has
  // completed.
  mgr->OnSetVersionRunnableComplete(this);

  return NS_OK;
}
