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
#include "DatabaseInfo.h"

#include "nsIDOMScriptObjectFactory.h"
#include "nsIFile.h"
#include "nsIObserverService.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsISHEntry.h"
#include "nsISimpleEnumerator.h"
#include "nsITimer.h"

#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/storage.h"
#include "nsContentUtils.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "nsXPCOMPrivate.h"
#include "test_quota.h"
#include "xpcpublic.h"

#include "AsyncConnectionHelper.h"
#include "CheckQuotaHelper.h"
#include "IDBDatabase.h"
#include "IDBEvents.h"
#include "IDBFactory.h"
#include "IDBKeyRange.h"
#include "LazyIdleThread.h"
#include "OpenDatabaseHelper.h"
#include "TransactionThreadPool.h"

// The amount of time, in milliseconds, that our IO thread will stay alive
// after the last event it processes.
#define DEFAULT_THREAD_TIMEOUT_MS 30000

// The amount of time, in milliseconds, that we will wait for active database
// transactions on shutdown before aborting them.
#define DEFAULT_SHUTDOWN_TIMER_MS 30000

// Amount of space that IndexedDB databases may use by default in megabytes.
#define DEFAULT_QUOTA_MB 50

// Preference that users can set to override DEFAULT_QUOTA_MB
#define PREF_INDEXEDDB_QUOTA "dom.indexedDB.warningQuota"

USING_INDEXEDDB_NAMESPACE
using namespace mozilla::services;
using mozilla::Preferences;

static NS_DEFINE_CID(kDOMSOF_CID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

namespace {

PRInt32 gShutdown = 0;
PRInt32 gClosed = 0;

// Does not hold a reference.
IndexedDatabaseManager* gInstance = nsnull;

PRInt32 gIndexedDBQuotaMB = DEFAULT_QUOTA_MB;

bool
GetBaseFilename(const nsAString& aFilename,
                nsAString& aBaseFilename)
{
  NS_ASSERTION(!aFilename.IsEmpty(), "Bad argument!");

  NS_NAMED_LITERAL_STRING(sqlite, ".sqlite");
  nsAString::size_type filenameLen = aFilename.Length();
  nsAString::size_type sqliteLen = sqlite.Length();

  if (sqliteLen > filenameLen ||
      Substring(aFilename, filenameLen - sqliteLen, sqliteLen) != sqlite) {
    return false;
  }

  aBaseFilename = Substring(aFilename, 0, filenameLen - sqliteLen);

  return true;
}

class QuotaCallback : public mozIStorageQuotaCallback
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD
  QuotaExceeded(const nsACString& aFilename,
                PRInt64 aCurrentSizeLimit,
                PRInt64 aCurrentTotalSize,
                nsISupports* aUserData,
                PRInt64* _retval)
  {
    if (IndexedDatabaseManager::QuotaIsLifted()) {
      *_retval = 0;
      return NS_OK;
    }

    return NS_ERROR_FAILURE;
  }
};

NS_IMPL_THREADSAFE_ISUPPORTS1(QuotaCallback, mozIStorageQuotaCallback)

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

PLDHashOperator
InvalidateAllFileManagers(const nsACString& aKey,
                          nsTArray<nsRefPtr<FileManager> >* aValue,
                          void* aUserArg)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aKey.IsEmpty(), "Empty key!");
  NS_ASSERTION(aValue, "Null pointer!");

  for (PRUint32 i = 0; i < aValue->Length(); i++) {
    nsRefPtr<FileManager> fileManager = aValue->ElementAt(i);
    fileManager->Invalidate();
  }

  return PL_DHASH_NEXT;
}

} // anonymous namespace

IndexedDatabaseManager::IndexedDatabaseManager()
: mCurrentWindowIndex(BAD_TLS_INDEX),
  mQuotaHelperMutex("IndexedDatabaseManager.mQuotaHelperMutex"),
  mFileMutex("IndexedDatabaseManager.mFileMutex")
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
    if (NS_FAILED(Preferences::AddIntVarCache(&gIndexedDBQuotaMB,
                                              PREF_INDEXEDDB_QUOTA,
                                              DEFAULT_QUOTA_MB))) {
      NS_WARNING("Unable to respond to quota pref changes!");
      gIndexedDBQuotaMB = DEFAULT_QUOTA_MB;
    }

    instance = new IndexedDatabaseManager();

    if (!instance->mLiveDatabases.Init() ||
        !instance->mQuotaHelperHash.Init() ||
        !instance->mFileManagers.Init()) {
      NS_WARNING("Out of memory!");
      return nsnull;
    }

    // We need a thread-local to hold the current window.
    NS_ASSERTION(instance->mCurrentWindowIndex == BAD_TLS_INDEX, "Huh?");

    if (PR_NewThreadPrivateIndex(&instance->mCurrentWindowIndex, nsnull) !=
        PR_SUCCESS) {
      NS_ERROR("PR_NewThreadPrivateIndex failed, IndexedDB disabled");
      instance->mCurrentWindowIndex = BAD_TLS_INDEX;
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
                                   false);
    NS_ENSURE_SUCCESS(rv, nsnull);

    // We don't really need this callback but we want the observer service to
    // hold us alive until XPCOM shutdown. That way other consumers can continue
    // to use this service until shutdown.
    rv = obs->AddObserver(instance, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID,
                          false);
    NS_ENSURE_SUCCESS(rv, nsnull);

    // Make a lazy thread for any IO we need (like clearing or enumerating the
    // contents of indexedDB database directories).
    instance->mIOThread = new LazyIdleThread(DEFAULT_THREAD_TIMEOUT_MS,
                                             LazyIdleThread::ManualShutdown);

    // We need one quota callback object to hand to SQLite.
    instance->mQuotaCallbackSingleton = new QuotaCallback();

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

nsresult
IndexedDatabaseManager::WaitForOpenAllowed(const nsACString& aOrigin,
                                           nsIAtom* aId,
                                           nsIRunnable* aRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aOrigin.IsEmpty(), "Empty origin!");
  NS_ASSERTION(aRunnable, "Null pointer!");

  nsAutoPtr<SynchronizedOp> op(new SynchronizedOp(aOrigin, aId));

  // See if this runnable needs to wait.
  bool delayed = false;
  for (PRUint32 index = mSynchronizedOps.Length(); index > 0; index--) {
    nsAutoPtr<SynchronizedOp>& existingOp = mSynchronizedOps[index - 1];
    if (op->MustWaitFor(*existingOp)) {
      existingOp->DelayRunnable(aRunnable);
      delayed = true;
      break;
    }
  }

  // Otherwise, dispatch it immediately.
  if (!delayed) {
    nsresult rv = NS_DispatchToCurrentThread(aRunnable);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Adding this to the synchronized ops list will block any additional
  // ops from proceeding until this one is done.
  mSynchronizedOps.AppendElement(op.forget());

  return NS_OK;
}

void
IndexedDatabaseManager::AllowNextSynchronizedOp(const nsACString& aOrigin,
                                                nsIAtom* aId)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aOrigin.IsEmpty(), "Empty origin!");

  PRUint32 count = mSynchronizedOps.Length();
  for (PRUint32 index = 0; index < count; index++) {
    nsAutoPtr<SynchronizedOp>& op = mSynchronizedOps[index];
    if (op->mOrigin.Equals(aOrigin)) {
      if (op->mId == aId) {
        NS_ASSERTION(op->mDatabases.IsEmpty(), "How did this happen?");

        op->DispatchDelayedRunnables();

        mSynchronizedOps.RemoveElementAt(index);
        return;
      }

      // If one or the other is for an origin clear, we should have matched
      // solely on origin.
      NS_ASSERTION(op->mId && aId, "Why didn't we match earlier?");
    }
  }

  NS_NOTREACHED("Why didn't we find a SynchronizedOp?");
}

nsresult
IndexedDatabaseManager::AcquireExclusiveAccess(const nsACString& aOrigin, 
                                               IDBDatabase* aDatabase,
                                               AsyncConnectionHelper* aHelper,
                                               WaitingOnDatabasesCallback aCallback,
                                               void* aClosure)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aHelper, "Why are you talking to me?");

  // Find the right SynchronizedOp.
  SynchronizedOp* op = nsnull;
  PRUint32 count = mSynchronizedOps.Length();
  for (PRUint32 index = 0; index < count; index++) {
    SynchronizedOp* currentop = mSynchronizedOps[index].get();
    if (currentop->mOrigin.Equals(aOrigin)) {
      if (!currentop->mId ||
          (aDatabase && currentop->mId == aDatabase->Id())) {
        // We've found the right one.
        NS_ASSERTION(!currentop->mHelper,
                     "SynchronizedOp already has a helper?!?");
        op = currentop;
        break;
      }
    }
  }

  NS_ASSERTION(op, "We didn't find a SynchronizedOp?");

  nsTArray<IDBDatabase*>* array;
  mLiveDatabases.Get(aOrigin, &array);

  // We need to wait for the databases to go away.
  // Hold on to all database objects that represent the same database file
  // (except the one that is requesting this version change).
  nsTArray<nsRefPtr<IDBDatabase> > liveDatabases;

  if (array) {
    PRUint32 count = array->Length();
    for (PRUint32 index = 0; index < count; index++) {
      IDBDatabase*& database = array->ElementAt(index);
      if (!database->IsClosed() &&
          (!aDatabase ||
           (aDatabase &&
            database != aDatabase &&
            database->Id() == aDatabase->Id()))) {
        liveDatabases.AppendElement(database);
      }
    }
  }

  if (liveDatabases.IsEmpty()) {
    IndexedDatabaseManager::DispatchHelper(aHelper);
    return NS_OK;
  }

  NS_ASSERTION(op->mDatabases.IsEmpty(), "How do we already have databases here?");
  op->mDatabases.AppendElements(liveDatabases);
  op->mHelper = aHelper;

  // Give our callback the databases so it can decide what to do with them.
  aCallback(liveDatabases, aClosure);

  NS_ASSERTION(liveDatabases.IsEmpty(),
               "Should have done something with the array!");
  return NS_OK;
}

// static
bool
IndexedDatabaseManager::IsShuttingDown()
{
  return !!gShutdown;
}

// static
bool
IndexedDatabaseManager::IsClosed()
{
  return !!gClosed;
}

void
IndexedDatabaseManager::AbortCloseDatabasesForWindow(nsPIDOMWindow* aWindow)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aWindow, "Null pointer!");

  nsAutoTArray<IDBDatabase*, 50> liveDatabases;
  mLiveDatabases.EnumerateRead(EnumerateToTArray, &liveDatabases);

  TransactionThreadPool* pool = TransactionThreadPool::Get();

  for (PRUint32 index = 0; index < liveDatabases.Length(); index++) {
    IDBDatabase*& database = liveDatabases[index];
    if (database->Owner() == aWindow) {
      if (NS_FAILED(database->Close())) {
        NS_WARNING("Failed to close database for dying window!");
      }

      if (pool) {
        pool->AbortTransactionsForDatabase(database);
      }
    }
  }
}

bool
IndexedDatabaseManager::HasOpenTransactions(nsPIDOMWindow* aWindow)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aWindow, "Null pointer!");

  nsAutoTArray<IDBDatabase*, 50> liveDatabases;
  mLiveDatabases.EnumerateRead(EnumerateToTArray, &liveDatabases);

  TransactionThreadPool* pool = TransactionThreadPool::Get();
  if (!pool) {
    return false;
  }

  for (PRUint32 index = 0; index < liveDatabases.Length(); index++) {
    IDBDatabase*& database = liveDatabases[index];
    if (database->Owner() == aWindow &&
        pool->HasTransactionsForDatabase(database)) {
      return true;
    }
  }
  
  return false;
}

void
IndexedDatabaseManager::OnDatabaseClosed(IDBDatabase* aDatabase)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aDatabase, "Null pointer!");

  // Check through the list of SynchronizedOps to see if any are waiting for
  // this database to close before proceeding.
  PRUint32 count = mSynchronizedOps.Length();
  for (PRUint32 index = 0; index < count; index++) {
    nsAutoPtr<SynchronizedOp>& op = mSynchronizedOps[index];

    if (op->mOrigin == aDatabase->Origin() &&
        (op->mId == aDatabase->Id() || !op->mId)) {
      // This database is in the scope of this SynchronizedOp.  Remove it
      // from the list if necessary.
      if (op->mDatabases.RemoveElement(aDatabase)) {
        // Now set up the helper if there are no more live databases.
        NS_ASSERTION(op->mHelper, "How did we get rid of the helper before "
                     "removing the last database?");
        if (op->mDatabases.IsEmpty()) {
          // At this point, all databases are closed, so no new transactions
          // can be started.  There may, however, still be outstanding
          // transactions that have not completed.  We need to wait for those
          // before we dispatch the helper.

          TransactionThreadPool* pool = TransactionThreadPool::GetOrCreate();
          if (!pool) {
            NS_ERROR("IndexedDB is totally broken.");
            return;
          }

          nsRefPtr<WaitForTransactionsToFinishRunnable> waitRunnable =
            new WaitForTransactionsToFinishRunnable(op);

          nsAutoTArray<nsRefPtr<IDBDatabase>, 1> array;
          array.AppendElement(aDatabase);

          // Use the WaitForTransactionsToFinishRunnable as the callback.
          if (!pool->WaitForAllDatabasesToComplete(array, waitRunnable)) {
            NS_WARNING("Failed to wait for transaction to complete!");
          }
        }
        break;
      }
    }
  }
}

void
IndexedDatabaseManager::SetCurrentWindowInternal(nsPIDOMWindow* aWindow)
{
  if (aWindow) {
#ifdef DEBUG
    NS_ASSERTION(!PR_GetThreadPrivate(mCurrentWindowIndex),
                 "Somebody forgot to clear the current window!");
#endif
    PR_SetThreadPrivate(mCurrentWindowIndex, aWindow);
  }
  else {
    // We cannot assert PR_GetThreadPrivate(mCurrentWindowIndex) here
    // because we cannot distinguish between the thread private became
    // null and that it was set to null on the first place, 
    // because we didn't have a window.
    PR_SetThreadPrivate(mCurrentWindowIndex, nsnull);
  }
}

// static
PRUint32
IndexedDatabaseManager::GetIndexedDBQuotaMB()
{
  return PRUint32(NS_MAX(gIndexedDBQuotaMB, 0));
}

nsresult
IndexedDatabaseManager::EnsureOriginIsInitialized(const nsACString& aOrigin,
                                                  nsIFile** aDirectory)
{
#ifdef DEBUG
  {
    bool correctThread;
    NS_ASSERTION(NS_SUCCEEDED(mIOThread->IsOnCurrentThread(&correctThread)) &&
                 correctThread,
                 "Running on the wrong thread!");
  }
#endif

  nsCOMPtr<nsIFile> directory;
  nsresult rv = IDBFactory::GetDirectoryForOrigin(aOrigin,
                                                  getter_AddRefs(directory));
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists;
  rv = directory->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {
    bool isDirectory;
    rv = directory->IsDirectory(&isDirectory);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(isDirectory, NS_ERROR_UNEXPECTED);
  }
  else {
    rv = directory->Create(nsIFile::DIRECTORY_TYPE, 0755);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (mFileManagers.Get(aOrigin)) {
    NS_ADDREF(*aDirectory = directory);
    return NS_OK;
  }

  // First figure out the filename pattern we'll use.
  nsCOMPtr<nsIFile> patternFile;
  rv = directory->Clone(getter_AddRefs(patternFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = patternFile->Append(NS_LITERAL_STRING("*"));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString pattern;
  rv = patternFile->GetNativePath(pattern);
  NS_ENSURE_SUCCESS(rv, rv);

  // Now tell SQLite to start tracking this pattern.
  nsCOMPtr<mozIStorageServiceQuotaManagement> ss =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(ss, NS_ERROR_FAILURE);

  rv = ss->SetQuotaForFilenamePattern(pattern,
                                      GetIndexedDBQuotaMB() * 1024 * 1024,
                                      mQuotaCallbackSingleton, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  // We need to see if there are any files in the directory already. If they
  // are database files then we need to create file managers for them and also
  // tell SQLite about all of them.

  nsAutoTArray<nsString, 20> subdirsToProcess;
  nsAutoTArray<nsCOMPtr<nsIFile> , 20> unknownFiles;

  nsAutoPtr<nsTArray<nsRefPtr<FileManager> > > fileManagers(
    new nsTArray<nsRefPtr<FileManager> >());

  nsTHashtable<nsStringHashKey> validSubdirs;
  NS_ENSURE_TRUE(validSubdirs.Init(20), NS_ERROR_OUT_OF_MEMORY);
  
  nsCOMPtr<nsISimpleEnumerator> entries;
  rv = directory->GetDirectoryEntries(getter_AddRefs(entries));
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasMore;
  while (NS_SUCCEEDED((rv = entries->HasMoreElements(&hasMore))) && hasMore) {
    nsCOMPtr<nsISupports> entry;
    rv = entries->GetNext(getter_AddRefs(entry));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> file = do_QueryInterface(entry);
    NS_ENSURE_TRUE(file, NS_NOINTERFACE);

    nsString leafName;
    rv = file->GetLeafName(leafName);
    NS_ENSURE_SUCCESS(rv, rv);

    bool isDirectory;
    rv = file->IsDirectory(&isDirectory);
    NS_ENSURE_SUCCESS(rv, rv);

    if (isDirectory) {
      if (!validSubdirs.GetEntry(leafName)) {
        subdirsToProcess.AppendElement(leafName);
      }
      continue;
    }

    nsString dbBaseFilename;
    if (!GetBaseFilename(leafName, dbBaseFilename)) {
      unknownFiles.AppendElement(file);
      continue;
    }

    nsCOMPtr<nsIFile> fileManagerDirectory;
    rv = directory->Clone(getter_AddRefs(fileManagerDirectory));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = fileManagerDirectory->Append(dbBaseFilename);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<mozIStorageConnection> connection;
    rv = OpenDatabaseHelper::CreateDatabaseConnection(
      NullString(), file, fileManagerDirectory, getter_AddRefs(connection));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<mozIStorageStatement> stmt;
    rv = connection->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT name "
      "FROM database"
    ), getter_AddRefs(stmt));
    NS_ENSURE_SUCCESS(rv, rv);

    bool hasResult;
    rv = stmt->ExecuteStep(&hasResult);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!hasResult) {
      NS_ERROR("Database has no name!");
      return NS_ERROR_UNEXPECTED;
    }

    nsString databaseName;
    rv = stmt->GetString(0, databaseName);
    NS_ENSURE_SUCCESS(rv, rv);

    nsRefPtr<FileManager> fileManager = new FileManager(aOrigin, databaseName);

    rv = fileManager->Init(fileManagerDirectory, connection);
    NS_ENSURE_SUCCESS(rv, rv);

    fileManagers->AppendElement(fileManager);

    rv = ss->UpdateQuotaInformationForFile(file);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!validSubdirs.PutEntry(dbBaseFilename)) {
      NS_WARNING("Out of memory?");
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 i = 0; i < subdirsToProcess.Length(); i++) {
    const nsString& subdir = subdirsToProcess[i];
    if (!validSubdirs.GetEntry(subdir)) {
      NS_WARNING("Unknown subdirectory found!");
      return NS_ERROR_UNEXPECTED;
    }
  }

  for (PRUint32 i = 0; i < unknownFiles.Length(); i++) {
    nsCOMPtr<nsIFile>& unknownFile = unknownFiles[i];

    // Some temporary SQLite files could disappear, so we have to check if the
    // unknown file still exists.
    bool exists;
    rv = unknownFile->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);

    if (exists) {
      nsString leafName;
      unknownFile->GetLeafName(leafName);

      // The journal file may exists even after db has been correctly opened.
      if (!StringEndsWith(leafName, NS_LITERAL_STRING(".sqlite-journal"))) {
        NS_WARNING("Unknown file found!");
        return NS_ERROR_UNEXPECTED;
      }
    }
  }

  if (!mFileManagers.Put(aOrigin, fileManagers)) {
    NS_WARNING("Out of memory?");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  fileManagers.forget();

  NS_ADDREF(*aDirectory = directory);
  return NS_OK;
}

bool
IndexedDatabaseManager::QuotaIsLiftedInternal()
{
  nsPIDOMWindow* window = nsnull;
  nsRefPtr<CheckQuotaHelper> helper = nsnull;
  bool createdHelper = false;

  window =
    static_cast<nsPIDOMWindow*>(PR_GetThreadPrivate(mCurrentWindowIndex));

  // Once IDB is supported outside of Windows this should become an early
  // return true.
  NS_ASSERTION(window, "Why don't we have a Window here?");

  // Hold the lock from here on.
  MutexAutoLock autoLock(mQuotaHelperMutex);

  mQuotaHelperHash.Get(window, getter_AddRefs(helper));

  if (!helper) {
    helper = new CheckQuotaHelper(window, mQuotaHelperMutex);
    createdHelper = true;

    bool result = mQuotaHelperHash.Put(window, helper);
    NS_ENSURE_TRUE(result, result);

    // Unlock while calling out to XPCOM
    {
      MutexAutoUnlock autoUnlock(mQuotaHelperMutex);

      nsresult rv = NS_DispatchToMainThread(helper);
      NS_ENSURE_SUCCESS(rv, false);
    }

    // Relocked.  If any other threads hit the quota limit on the same Window,
    // they are using the helper we created here and are now blocking in
    // PromptAndReturnQuotaDisabled.
  }

  bool result = helper->PromptAndReturnQuotaIsDisabled();

  // If this thread created the helper and added it to the hash, this thread
  // must remove it.
  if (createdHelper) {
    mQuotaHelperHash.Remove(window);
  }

  return result;
}

void
IndexedDatabaseManager::CancelPromptsForWindowInternal(nsPIDOMWindow* aWindow)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<CheckQuotaHelper> helper;

  MutexAutoLock autoLock(mQuotaHelperMutex);

  mQuotaHelperHash.Get(aWindow, getter_AddRefs(helper));

  if (helper) {
    helper->Cancel();
  }
}

// static
nsresult
IndexedDatabaseManager::GetASCIIOriginFromWindow(nsPIDOMWindow* aWindow,
                                                 nsCString& aASCIIOrigin)
{
  NS_ASSERTION(NS_IsMainThread(),
               "We're about to touch a window off the main thread!");

  if (!aWindow) {
    aASCIIOrigin.AssignLiteral("chrome");
    NS_ASSERTION(nsContentUtils::IsCallerChrome(), 
                 "Null window but not chrome!");
    return NS_OK;
  }

  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aWindow);
  NS_ENSURE_TRUE(sop, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsCOMPtr<nsIPrincipal> principal = sop->GetPrincipal();
  NS_ENSURE_TRUE(principal, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (nsContentUtils::IsSystemPrincipal(principal)) {
    aASCIIOrigin.AssignLiteral("chrome");
  }
  else {
    nsresult rv = nsContentUtils::GetASCIIOrigin(principal, aASCIIOrigin);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    if (aASCIIOrigin.EqualsLiteral("null")) {
      NS_WARNING("IndexedDB databases not allowed for this principal!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }
  }

  return NS_OK;
}

already_AddRefed<FileManager>
IndexedDatabaseManager::GetOrCreateFileManager(const nsACString& aOrigin,
                                               const nsAString& aDatabaseName)
{
  nsTArray<nsRefPtr<FileManager> >* array;
  if (!mFileManagers.Get(aOrigin, &array)) {
    nsAutoPtr<nsTArray<nsRefPtr<FileManager> > > newArray(
      new nsTArray<nsRefPtr<FileManager> >());
    if (!mFileManagers.Put(aOrigin, newArray)) {
      NS_WARNING("Out of memory?");
      return nsnull;
    }
    array = newArray.forget();
  }

  nsRefPtr<FileManager> fileManager;
  for (PRUint32 i = 0; i < array->Length(); i++) {
    nsRefPtr<FileManager> fm = array->ElementAt(i);

    if (fm->DatabaseName().Equals(aDatabaseName)) {
      fileManager = fm.forget();
      break;
    }
  }
  
  if (!fileManager) {
    fileManager = new FileManager(aOrigin, aDatabaseName);

    array->AppendElement(fileManager);
  }

  return fileManager.forget();
}

already_AddRefed<FileManager>
IndexedDatabaseManager::GetFileManager(const nsACString& aOrigin,
                                       const nsAString& aDatabaseName)
{
  nsTArray<nsRefPtr<FileManager> >* array;
  if (!mFileManagers.Get(aOrigin, &array)) {
    return nsnull;
  }

  for (PRUint32 i = 0; i < array->Length(); i++) {
    nsRefPtr<FileManager>& fileManager = array->ElementAt(i);

    if (fileManager->DatabaseName().Equals(aDatabaseName)) {
      nsRefPtr<FileManager> result = fileManager;
      return result.forget();
    }
  }
  
  return nsnull;
}

void
IndexedDatabaseManager::InvalidateFileManagersForOrigin(
                                                     const nsACString& aOrigin)
{
  nsTArray<nsRefPtr<FileManager> >* array;
  if (mFileManagers.Get(aOrigin, &array)) {
    for (PRUint32 i = 0; i < array->Length(); i++) {
      nsRefPtr<FileManager> fileManager = array->ElementAt(i);
      fileManager->Invalidate();
    }
    mFileManagers.Remove(aOrigin);
  }
}

void
IndexedDatabaseManager::InvalidateFileManager(const nsACString& aOrigin,
                                              const nsAString& aDatabaseName)
{
  nsTArray<nsRefPtr<FileManager> >* array;
  if (!mFileManagers.Get(aOrigin, &array)) {
    return;
  }

  for (PRUint32 i = 0; i < array->Length(); i++) {
    nsRefPtr<FileManager> fileManager = array->ElementAt(i);
    if (fileManager->DatabaseName().Equals(aDatabaseName)) {
      fileManager->Invalidate();
      array->RemoveElementAt(i);

      if (array->IsEmpty()) {
        mFileManagers.Remove(aOrigin);
      }

      break;
    }
  }
}

nsresult
IndexedDatabaseManager::AsyncDeleteFile(FileManager* aFileManager,
                                        PRInt64 aFileId)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  NS_ENSURE_ARG_POINTER(aFileManager);

  // See if we're currently clearing the databases for this origin. If so then
  // we pretend that we've already deleted everything.
  if (IsClearOriginPending(aFileManager->Origin())) {
    return NS_OK;
  }

  nsCOMPtr<nsIFile> directory = aFileManager->GetDirectory();
  NS_ENSURE_TRUE(directory, NS_ERROR_FAILURE);

  nsCOMPtr<nsIFile> file = aFileManager->GetFileForId(directory, aFileId);
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

  nsString filePath;
  nsresult rv = file->GetPath(filePath);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<AsyncDeleteFileRunnable> runnable =
    new AsyncDeleteFileRunnable(filePath);

  rv = mIOThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// static
nsresult
IndexedDatabaseManager::DispatchHelper(AsyncConnectionHelper* aHelper)
{
  nsresult rv = NS_OK;

  // If the helper has a transaction, dispatch it to the transaction
  // threadpool.
  if (aHelper->HasTransaction()) {
    rv = aHelper->DispatchToTransactionPool();
  }
  else {
    // Otherwise, dispatch it to the IO thread.
    IndexedDatabaseManager* manager = IndexedDatabaseManager::Get();
    NS_ASSERTION(manager, "We should definitely have a manager here");

    rv = aHelper->Dispatch(manager->IOThread());
  }

  NS_ENSURE_SUCCESS(rv, rv);
  return rv;
}

bool
IndexedDatabaseManager::IsClearOriginPending(const nsACString& origin)
{
  // Iterate through our SynchronizedOps to see if we have an entry that matches
  // this origin and has no id.
  PRUint32 count = mSynchronizedOps.Length();
  for (PRUint32 index = 0; index < count; index++) {
    nsAutoPtr<SynchronizedOp>& op = mSynchronizedOps[index];
    if (op->mOrigin.Equals(origin) && !op->mId) {
      return true;
    }
  }

  return false;
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
  if (IsClearOriginPending(origin)) {
    rv = NS_DispatchToCurrentThread(runnable);
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
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

    bool equals;
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

  // If there is a pending or running clear operation for this origin, return
  // immediately.
  if (IsClearOriginPending(origin)) {
    return NS_OK;
  }

  // Queue up the origin clear runnable.
  nsRefPtr<OriginClearRunnable> runnable =
    new OriginClearRunnable(origin, mIOThread);

  rv = WaitForOpenAllowed(origin, nsnull, runnable);
  NS_ENSURE_SUCCESS(rv, rv);

  // Give the runnable some help by invalidating any databases in the way.
  // We need to grab references to any live databases here to prevent them from
  // dying while we invalidate them.
  nsTArray<nsRefPtr<IDBDatabase> > liveDatabases;

  // Grab all live databases for this origin.
  nsTArray<IDBDatabase*>* array;
  if (mLiveDatabases.Get(origin, &array)) {
    liveDatabases.AppendElements(*array);
  }

  // Invalidate all the live databases first.
  for (PRUint32 index = 0; index < liveDatabases.Length(); index++) {
    liveDatabases[index]->Invalidate();
  }
  
  DatabaseInfo::RemoveAllForOrigin(origin);

  // After everything has been invalidated the helper should be dispatched to
  // the end of the event queue.

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
    if (PR_ATOMIC_SET(&gShutdown, 1)) {
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

    mFileManagers.EnumerateRead(InvalidateAllFileManagers, nsnull);

    if (PR_ATOMIC_SET(&gClosed, 1)) {
      NS_ERROR("Close more than once?!");
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
    // On the first time on the main thread we dispatch to the IO thread.
    if (mFirstCallback) {
      NS_ASSERTION(mThread, "Should have a thread here!");

      mFirstCallback = false;

      nsCOMPtr<nsIThread> thread;
      mThread.swap(thread);

      // Dispatch to the IO thread.
      if (NS_FAILED(thread->Dispatch(this, NS_DISPATCH_NORMAL))) {
        NS_WARNING("Failed to dispatch to IO thread!");
        return NS_ERROR_FAILURE;
      }

      return NS_OK;
    }

    NS_ASSERTION(!mThread, "Should have been cleared already!");

    IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
    NS_ASSERTION(mgr, "This should never fail!");

    mgr->InvalidateFileManagersForOrigin(mOrigin);

    // Tell the IndexedDatabaseManager that we're done.
    mgr->AllowNextSynchronizedOp(mOrigin, nsnull);

    return NS_OK;
  }

  NS_ASSERTION(!mThread, "Should have been cleared already!");

  // Remove the directory that contains all our databases.
  nsCOMPtr<nsIFile> directory;
  nsresult rv = IDBFactory::GetDirectoryForOrigin(mOrigin,
                                                  getter_AddRefs(directory));
  if (NS_SUCCEEDED(rv)) {
    bool exists;
    rv = directory->Exists(&exists);
    if (NS_SUCCEEDED(rv) && exists) {
      rv = directory->Remove(true);
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
  mFileUsage(0),
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
  if (PR_ATOMIC_SET(&mCanceled, 1)) {
    NS_ERROR("Canceled more than once?!");
  }
}

inline void
IncrementUsage(PRUint64* aUsage, PRUint64 aDelta)
{
  // Watch for overflow!
  if ((LL_MAXINT - *aUsage) <= aDelta) {
    NS_WARNING("Database sizes exceed max we can report!");
    *aUsage = LL_MAXINT;
  }
  else {
    *aUsage += aDelta;
  }
}

nsresult
IndexedDatabaseManager::AsyncUsageRunnable::RunInternal()
{
  if (NS_IsMainThread()) {
    // Call the callback unless we were canceled.
    if (!mCanceled) {
      PRUint64 usage = mUsage;
      IncrementUsage(&usage, mFileUsage);
      mCallback->OnUsageResult(mURI, usage, mFileUsage);
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

  bool exists;
  rv = directory->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  // If the directory exists then enumerate all the files inside, adding up the
  // sizes to get the final usage statistic.
  if (exists && !mCanceled) {
    rv = GetUsageForDirectory(directory, &mUsage);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult
IndexedDatabaseManager::AsyncUsageRunnable::GetUsageForDirectory(
                                     nsIFile* aDirectory,
                                     PRUint64* aUsage)
{
  NS_ASSERTION(aDirectory, "Null pointer!");
  NS_ASSERTION(aUsage, "Null pointer!");

  nsCOMPtr<nsISimpleEnumerator> entries;
  nsresult rv = aDirectory->GetDirectoryEntries(getter_AddRefs(entries));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!entries) {
    return NS_OK;
  }

  bool hasMore;
  while (NS_SUCCEEDED((rv = entries->HasMoreElements(&hasMore))) &&
         hasMore && !mCanceled) {
    nsCOMPtr<nsISupports> entry;
    rv = entries->GetNext(getter_AddRefs(entry));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> file(do_QueryInterface(entry));
    NS_ASSERTION(file, "Don't know what this is!");

    bool isDirectory;
    rv = file->IsDirectory(&isDirectory);
    NS_ENSURE_SUCCESS(rv, rv);

    if (isDirectory) {
      if (aUsage == &mFileUsage) {
        NS_WARNING("Unknown directory found!");
      }
      else {
        rv = GetUsageForDirectory(file, &mFileUsage);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      continue;
    }

    PRInt64 fileSize;
    rv = file->GetFileSize(&fileSize);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ASSERTION(fileSize > 0, "Negative size?!");

    IncrementUsage(aUsage, PRUint64(fileSize));
  }
  NS_ENSURE_SUCCESS(rv, rv);
 
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

NS_IMPL_THREADSAFE_ISUPPORTS1(IndexedDatabaseManager::WaitForTransactionsToFinishRunnable,
                              nsIRunnable)

NS_IMETHODIMP
IndexedDatabaseManager::WaitForTransactionsToFinishRunnable::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mOp && mOp->mHelper, "What?");

  // Don't hold the callback alive longer than necessary.
  nsRefPtr<AsyncConnectionHelper> helper;
  helper.swap(mOp->mHelper);

  mOp = nsnull;

  IndexedDatabaseManager::DispatchHelper(helper);

  // The helper is responsible for calling
  // IndexedDatabaseManager::AllowNextSynchronizedOp.

  return NS_OK;
}


IndexedDatabaseManager::SynchronizedOp::SynchronizedOp(const nsACString& aOrigin,
                                                       nsIAtom* aId)
: mOrigin(aOrigin), mId(aId)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  MOZ_COUNT_CTOR(IndexedDatabaseManager::SynchronizedOp);
}

IndexedDatabaseManager::SynchronizedOp::~SynchronizedOp()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  MOZ_COUNT_DTOR(IndexedDatabaseManager::SynchronizedOp);
}

bool
IndexedDatabaseManager::SynchronizedOp::MustWaitFor(const SynchronizedOp& aRhs)
  const
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // If the origins don't match, the second can proceed.
  if (!aRhs.mOrigin.Equals(mOrigin)) {
    return false;
  }

  // If the origins and the ids match, the second must wait.
  if (aRhs.mId == mId) {
    return true;
  }

  // Waiting is required if either one corresponds to an origin clearing
  // (a null Id).
  if (!aRhs.mId || !mId) {
    return true;
  }

  // Otherwise, things for the same origin but different databases can proceed
  // independently.
  return false;
}

void
IndexedDatabaseManager::SynchronizedOp::DelayRunnable(nsIRunnable* aRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mDelayedRunnables.IsEmpty() || !mId,
               "Only ClearOrigin operations can delay multiple runnables!");

  mDelayedRunnables.AppendElement(aRunnable);
}

void
IndexedDatabaseManager::SynchronizedOp::DispatchDelayedRunnables()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!mHelper, "Any helper should be gone by now!");

  PRUint32 count = mDelayedRunnables.Length();
  for (PRUint32 index = 0; index < count; index++) {
    NS_DispatchToCurrentThread(mDelayedRunnables[index]);
  }

  mDelayedRunnables.Clear();
}

NS_IMETHODIMP
IndexedDatabaseManager::InitWindowless(const jsval& aObj, JSContext* aCx)
{
  NS_ENSURE_TRUE(nsContentUtils::IsCallerChrome(), NS_ERROR_NOT_AVAILABLE);
  
  // Instantiating this class will register exception providers so even 
  // in xpcshell we will get typed (dom) exceptions, instead of general exceptions.
  nsCOMPtr<nsIDOMScriptObjectFactory> sof(do_GetService(kDOMSOF_CID));

  // Defining IDBKeyrange static functions on the global.
  if (JSVAL_IS_PRIMITIVE(aObj)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIIDBFactory> factory = IDBFactory::Create(nsnull);
  NS_ASSERTION(factory, "IDBFactory should not be null.");

  JSObject* obj = JSVAL_TO_OBJECT(aObj);
  jsval mozIndexedDBVal;
  nsresult rv = nsContentUtils::WrapNative(aCx, obj, factory, &mozIndexedDBVal);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!JS_DefineProperty(aCx, obj, "mozIndexedDB", mozIndexedDBVal, 
      nsnull, nsnull, JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  JSObject* keyrangeObj = JS_NewObject(aCx, nsnull, nsnull, nsnull);
  NS_ENSURE_TRUE(keyrangeObj, NS_ERROR_OUT_OF_MEMORY);
    
  if (!IDBKeyRange::DefineConstructors(aCx, keyrangeObj)) {
    return NS_ERROR_FAILURE;
  }

  if (!JS_DefineProperty(aCx, obj, "IDBKeyRange", OBJECT_TO_JSVAL(keyrangeObj),
                         nsnull, nsnull, JSPROP_ENUMERATE)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(IndexedDatabaseManager::AsyncDeleteFileRunnable,
                              nsIRunnable)

NS_IMETHODIMP
IndexedDatabaseManager::AsyncDeleteFileRunnable::Run()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  int rc = sqlite3_quota_remove(NS_ConvertUTF16toUTF8(mFilePath).get());
  if (rc != SQLITE_OK) {
    NS_WARNING("Failed to delete stored file!");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}
