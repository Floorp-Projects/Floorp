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

#include "IDBDatabase.h"
#include "IDBFactory.h"
#include "LazyIdleThread.h"
#include "TransactionThreadPool.h"

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

// Called when an IDBDatabase is constructed.
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

// Called when an IDBDatabase is destroyed.
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

// Called when OriginClearRunnable has finished its Run() method.
void
IndexedDatabaseManager::OnOriginClearComplete(OriginClearRunnable* aRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aRunnable, "Null pointer!");
  NS_ASSERTION(!aRunnable->mThread, "Thread should be null!");
  NS_ASSERTION(aRunnable->mDatabasesWaiting.IsEmpty(), "Databases waiting?!");
  NS_ASSERTION(aRunnable->mDelayedRunnables.IsEmpty(),
               "Delayed runnables should have been dispatched already!");

  if (!mOriginClearRunnables.RemoveElement(aRunnable)) {
    NS_ERROR("Don't know anything about this runnable!");
  }
}

// Called when AsyncUsageRunnable has finished its Run() method.
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

// Waits until it is safe for a new database to be created with the given origin
// before dispatching the given runnable.
nsresult
IndexedDatabaseManager::WaitForClearAndDispatch(const nsACString& aOrigin,
                                                nsIRunnable* aRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
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

  // We aren't currently clearing databases for this origin, so dispatch the
  // runnable immediately.
  return NS_DispatchToCurrentThread(aRunnable);
}

// static
bool
IndexedDatabaseManager::IsShuttingDown()
{
  return !!gShutdown;
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
    new OriginClearRunnable(origin, mIOThread, liveDatabases);

  NS_ASSERTION(liveDatabases.IsEmpty(), "Should have swapped!");

  // Make a new entry for this origin in mOriginClearRunnables.
  nsRefPtr<OriginClearRunnable>* newRunnable =
    mOriginClearRunnables.AppendElement(runnable);
  NS_ENSURE_TRUE(newRunnable, NS_ERROR_OUT_OF_MEMORY);

  if (!runnable->mDatabasesWaiting.IsEmpty()) {
    PRUint32 count = runnable->mDatabasesWaiting.Length();

    // Invalidate all the live databases first.
    for (PRUint32 index = 0; index < count; index++) {
      runnable->mDatabasesWaiting[index]->Invalidate();
    }

    // Now set up our callbacks so that we know when they have finished.
    TransactionThreadPool* pool = TransactionThreadPool::Get();
    for (PRUint32 index = 0; index < count; index++) {
      if (!pool->WaitForAllTransactionsToComplete(
                                  runnable->mDatabasesWaiting[index],
                                  OriginClearRunnable::DatabaseCompleteCallback,
                                  runnable)) {
        NS_WARNING("Out of memory!");
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }

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

// Called by the TransactionThreadPool when the given database has completed all
// of its transactions.
void
IndexedDatabaseManager::OriginClearRunnable::OnDatabaseComplete(
                                                         IDBDatabase* aDatabase)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aDatabase, "Null pointer!");
  NS_ASSERTION(mThread, "This shouldn't be cleared yet!");

  // Remove the database from the list of databases that we're waiting on.
  if (!mDatabasesWaiting.RemoveElement(aDatabase)) {
    NS_ERROR("Don't know anything about this database!");
  }

  // Now dispatch this runnable to the IO thread if the list is empty.
  if (mDatabasesWaiting.IsEmpty()) {
    if (NS_FAILED(mThread->Dispatch(this, NS_DISPATCH_NORMAL))) {
      NS_WARNING("Can't dispatch to IO thread!");
    }

    // We no longer need to keep the thread alive.
    mThread = nsnull;
  }
}

NS_IMPL_THREADSAFE_ISUPPORTS1(IndexedDatabaseManager::OriginClearRunnable,
                              nsIRunnable)

// Runs twice, first on the IO thread, then again on the main thread. While on
// the IO thread the runnable will actually remove the origin's database files
// and the directory that contains them before dispatching itself back to the
// main thread. When on the main thread the runnable will dispatch any queued
// runnables and then notify the IndexedDatabaseManager that the job has been
// completed.
NS_IMETHODIMP
IndexedDatabaseManager::OriginClearRunnable::Run()
{
  if (NS_IsMainThread()) {
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

// Sets the canceled flag so that the callback is never called.
void
IndexedDatabaseManager::AsyncUsageRunnable::Cancel()
{
  if (PR_AtomicSet(&mCanceled, 1)) {
    NS_ERROR("Canceled more than once?!");
  }
}

// Runs twice, first on the IO thread, then again on the main thread. While on
// the IO thread the runnable will calculate the size of all files in the
// origin's directory before dispatching itself back to the main thread. When on
// the main thread the runnable will call the callback and then notify the
// IndexedDatabaseManager that the job has been completed.
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

// Calls the RunInternal method and makes sure that we always dispatch to the
// main thread in case of an error.
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
