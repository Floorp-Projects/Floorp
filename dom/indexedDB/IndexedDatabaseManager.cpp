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

#include "mozilla/Services.h"
#include "nsContentUtils.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"

#include "IDBDatabase.h"
#include "IDBFactory.h"

USING_INDEXEDDB_NAMESPACE
using namespace mozilla::services;

namespace {

bool gShutdown = false;

// Holds a reference!
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

  nsTArray<nsRefPtr<IDBDatabase> >* array =
    static_cast<nsTArray<nsRefPtr<IDBDatabase> >* >(aUserArg);

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

// Returns a raw pointer that carries an owning reference! Lame, but the
// singleton factory macros force this.
IndexedDatabaseManager*
IndexedDatabaseManager::GetOrCreateInstance()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (gShutdown) {
    NS_ERROR("Calling GetOrCreateInstance() after shutdown!");
    return nsnull;
  }

  if (!gInstance) {
    nsRefPtr<IndexedDatabaseManager> instance(new IndexedDatabaseManager());

    if (!instance->mLiveDatabases.Init()) {
      NS_WARNING("Out of memory!");
      return nsnull;
    }

    // We need to know when to release the singleton.
    nsCOMPtr<nsIObserverService> obs = GetObserverService();
    nsresult rv = obs->AddObserver(instance, NS_XPCOM_SHUTDOWN_OBSERVER_ID,
                                   PR_FALSE);
    NS_ENSURE_SUCCESS(rv, nsnull);

    instance.forget(&gInstance);
  }

  NS_IF_ADDREF(gInstance);
  return gInstance;
}

// Does not return an owning reference.
IndexedDatabaseManager*
IndexedDatabaseManager::GetInstance()
{
  return gInstance;
}

// Called when an IDBDatabase is constructed.
bool
IndexedDatabaseManager::RegisterDatabase(IDBDatabase* aDatabase)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aDatabase, "Null pointer!");

  // Don't allow any new databases to be created after shutdown.
  if (gShutdown) {
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

nsresult
IndexedDatabaseManager::WaitForClearAndDispatch(const nsACString& aOrigin,
                                                nsIRunnable* aRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aOrigin.IsEmpty(), "Empty origin!");
  NS_ASSERTION(aRunnable, "Null pointer!");

  // See if we're currently clearing database files for this origin. If so then
  // queue the runnable for later dispatch after we're done clearing.
  PRUint32 count = mOriginClearData.Length();
  for (PRUint32 index = 0; index < count; index++) {
    OriginClearData& data = mOriginClearData[index];
    if (data.origin == aOrigin) {
      nsCOMPtr<nsIRunnable>* newPtr =
        data.delayedRunnables.AppendElement(aRunnable);
      NS_ENSURE_TRUE(newPtr, NS_ERROR_OUT_OF_MEMORY);

      return NS_OK;
    }
  }

  // We aren't currently clearing databases for this origin, so dispatch the
  // runnable immediately.
  return NS_DispatchToCurrentThread(aRunnable);
}

NS_IMPL_ISUPPORTS2(IndexedDatabaseManager, nsIIndexedDatabaseManager,
                                           nsIObserver)

NS_IMETHODIMP
IndexedDatabaseManager::GetUsageForURI(nsIURI* aURI,
                                       PRUint64* _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  NS_ENSURE_ARG_POINTER(aURI);

  // Figure out which origin we're dealing with.
  nsCString origin;
  nsresult rv = nsContentUtils::GetASCIIOrigin(aURI, origin);
  NS_ENSURE_SUCCESS(rv, rv);

  // Non-standard URIs can't create databases anyway, so return 0.
  if (origin.EqualsLiteral("null")) {
    *_retval = 0;
    return NS_OK;
  }

  // Get the directory where we may be storing database files for this origin.
  nsCOMPtr<nsIFile> directory;
  rv = IDBFactory::GetDirectoryForOrigin(origin, getter_AddRefs(directory));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool exists;
  rv = directory->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint64 usage = 0;

  // If the directory exists then enumerate all the files inside, adding up the
  // sizes to get the final usage statistic.
  if (exists) {
    nsCOMPtr<nsISimpleEnumerator> entries;
    rv = directory->GetDirectoryEntries(getter_AddRefs(entries));
    NS_ENSURE_SUCCESS(rv, rv);

    if (entries) {
      PRBool hasMore;
      while (NS_SUCCEEDED(entries->HasMoreElements(&hasMore)) && hasMore) {
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
        if (NS_UNLIKELY((LL_MAXINT - usage) <= PRUint64(fileSize))) {
          NS_WARNING("Database sizes exceed max we can report!");
          usage = LL_MAXINT;
        }
        else {
          usage += fileSize;
        }
      }
    }
  }

  *_retval = usage;
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
  PRUint32 clearDataCount = mOriginClearData.Length();
  for (PRUint32 index = 0; index < clearDataCount; index++) {
    if (mOriginClearData[index].origin == origin) {
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

  // Make a new entry for this origin in mOriginClearData. Don't return early
  // while mOriginClearData has this origin in it!
  OriginClearData* data = mOriginClearData.AppendElement();
  NS_ENSURE_TRUE(data, NS_ERROR_OUT_OF_MEMORY);

  data->origin = origin;

  if (!liveDatabases.IsEmpty()) {
    PRUint32 count = liveDatabases.Length();

    // Invalidate all the live databases first.
    for (PRUint32 index = 0; index < count; index++) {
      liveDatabases[index]->Invalidate();
    }

    // Now wait for them to finish.
    for (PRUint32 index = 0; index < count; index++) {
      liveDatabases[index]->WaitForConnectionReleased();
    }
  }

  // Now that all our databases have released their connections to their files
  // we can go ahead and delete the directory. Don't return early while
  // mOriginClearData has this origin in it!
  nsCOMPtr<nsIFile> directory;
  rv = IDBFactory::GetDirectoryForOrigin(origin, getter_AddRefs(directory));
  if (NS_SUCCEEDED(rv)) {
    PRBool exists;
    rv = directory->Exists(&exists);
    if (NS_SUCCEEDED(rv) && exists) {
      rv = directory->Remove(PR_TRUE);
    }
  }

  // Remove this origin's entry from mOriginClearData. Dispatch any runnables
  // that were queued along the way.
  clearDataCount = mOriginClearData.Length();
  for (PRUint32 clearDataIndex = 0; clearDataIndex < clearDataCount;
       clearDataIndex++) {
    OriginClearData& data = mOriginClearData[clearDataIndex];
    if (data.origin == origin) {
      nsTArray<nsCOMPtr<nsIRunnable> >& runnables = data.delayedRunnables;
      PRUint32 runnableCount = runnables.Length();
      for (PRUint32 runnableIndex = 0; runnableIndex < runnableCount;
           runnableIndex++) {
        NS_DispatchToCurrentThread(runnables[runnableIndex]);
      }
      mOriginClearData.RemoveElementAt(clearDataIndex);
      break;
    }
  }

  // Now we can finally return errors.
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
IndexedDatabaseManager::Observe(nsISupports* aSubject,
                                const char* aTopic,
                                const PRUnichar* aData)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    // Setting this flag prevents the servic from being recreated and prevents
    // further databases from being created.
    gShutdown = true;

    // Grab all live databases, for all origins. Need references to keep them
    // alive while we wait on them.
    nsAutoTArray<nsRefPtr<IDBDatabase>, 50> liveDatabases;
    mLiveDatabases.EnumerateRead(EnumerateToTArray, &liveDatabases);

    // Wait for all live databases to finish.
    if (!liveDatabases.IsEmpty()) {
      PRUint32 count = liveDatabases.Length();
      for (PRUint32 index = 0; index < count; index++) {
        liveDatabases[index]->WaitForConnectionReleased();
      }
    }

    mLiveDatabases.Clear();

    // This may kill us.
    gInstance->Release();
    return NS_OK;
  }

  NS_NOTREACHED("Unknown topic!");
  return NS_ERROR_UNEXPECTED;
}
