/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IndexedDatabaseManager.h"
#include "DatabaseInfo.h"

#include "nsIAtom.h"
#include "nsIConsoleService.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsIFile.h"
#include "nsIFileStorage.h"
#include "nsIObserverService.h"
#include "nsIScriptError.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsISHEntry.h"
#include "nsISimpleEnumerator.h"
#include "nsITimer.h"

#include "mozilla/dom/file/FileService.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/storage.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsContentUtils.h"
#include "nsDirectoryServiceUtils.h"
#include "nsEventDispatcher.h"
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

// profile-before-change, when we need to shut down IDB
#define PROFILE_BEFORE_CHANGE_OBSERVER_ID "profile-before-change"

USING_INDEXEDDB_NAMESPACE
using namespace mozilla::services;
using mozilla::Preferences;
using mozilla::dom::file::FileService;

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

class QuotaCallback MOZ_FINAL : public mozIStorageQuotaCallback
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
template <class T>
PLDHashOperator
EnumerateToTArray(const nsACString& aKey,
                  nsTArray<IDBDatabase*>* aValue,
                  void* aUserArg)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aKey.IsEmpty(), "Empty key!");
  NS_ASSERTION(aValue, "Null pointer!");
  NS_ASSERTION(aUserArg, "Null pointer!");

  nsTArray<T>* array =
    static_cast<nsTArray<T>*>(aUserArg);

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
  NS_ASSERTION(!gInstance || gInstance == this, "Different instances!");
  gInstance = nsnull;
}

bool IndexedDatabaseManager::sIsMainProcess = false;

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
    sIsMainProcess = XRE_GetProcessType() == GeckoProcessType_Default;

    instance = new IndexedDatabaseManager();

    instance->mLiveDatabases.Init();
    instance->mQuotaHelperHash.Init();
    instance->mFileManagers.Init();

    // We need a thread-local to hold the current window.
    NS_ASSERTION(instance->mCurrentWindowIndex == BAD_TLS_INDEX, "Huh?");

    if (PR_NewThreadPrivateIndex(&instance->mCurrentWindowIndex, nsnull) !=
        PR_SUCCESS) {
      NS_ERROR("PR_NewThreadPrivateIndex failed, IndexedDB disabled");
      instance->mCurrentWindowIndex = BAD_TLS_INDEX;
      return nsnull;
    }

    nsresult rv;

    if (sIsMainProcess) {
      nsCOMPtr<nsIFile> dbBaseDirectory;
      rv = NS_GetSpecialDirectory(NS_APP_INDEXEDDB_PARENT_DIR, getter_AddRefs(dbBaseDirectory));
      if (NS_FAILED(rv)) {
          rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(dbBaseDirectory));
      }
      NS_ENSURE_SUCCESS(rv, nsnull);

      rv = dbBaseDirectory->Append(NS_LITERAL_STRING("indexedDB"));
      NS_ENSURE_SUCCESS(rv, nsnull);

      rv = dbBaseDirectory->GetPath(instance->mDatabaseBasePath);
      NS_ENSURE_SUCCESS(rv, nsnull);

      // Make a lazy thread for any IO we need (like clearing or enumerating the
      // contents of indexedDB database directories).
      instance->mIOThread = new LazyIdleThread(DEFAULT_THREAD_TIMEOUT_MS,
                                               NS_LITERAL_CSTRING("IndexedDB I/O"),
                                               LazyIdleThread::ManualShutdown);

      // We need one quota callback object to hand to SQLite.
      instance->mQuotaCallbackSingleton = new QuotaCallback();

      // Make a timer here to avoid potential failures later. We don't actually
      // initialize the timer until shutdown.
      instance->mShutdownTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
      NS_ENSURE_TRUE(instance->mShutdownTimer, nsnull);
    }

    nsCOMPtr<nsIObserverService> obs = GetObserverService();
    NS_ENSURE_TRUE(obs, nsnull);

    // We need this callback to know when to shut down all our threads.
    rv = obs->AddObserver(instance, PROFILE_BEFORE_CHANGE_OBSERVER_ID, false);
    NS_ENSURE_SUCCESS(rv, nsnull);

    if (NS_FAILED(Preferences::AddIntVarCache(&gIndexedDBQuotaMB,
                                              PREF_INDEXEDDB_QUOTA,
                                              DEFAULT_QUOTA_MB))) {
      NS_WARNING("Unable to respond to quota pref changes!");
      gIndexedDBQuotaMB = DEFAULT_QUOTA_MB;
    }

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

nsresult
IndexedDatabaseManager::GetDirectoryForOrigin(const nsACString& aASCIIOrigin,
                                              nsIFile** aDirectory) const
{
  nsresult rv;
  nsCOMPtr<nsIFile> directory =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = directory->InitWithPath(GetBaseDirectory());
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ConvertASCIItoUTF16 originSanitized(aASCIIOrigin);
  originSanitized.ReplaceChar(":/", '+');

  rv = directory->Append(originSanitized);
  NS_ENSURE_SUCCESS(rv, rv);

  directory.forget(aDirectory);
  return NS_OK;
}

// static
already_AddRefed<nsIAtom>
IndexedDatabaseManager::GetDatabaseId(const nsACString& aOrigin,
                                      const nsAString& aName)
{
  nsCString str(aOrigin);
  str.Append("*");
  str.Append(NS_ConvertUTF16toUTF8(aName));

  nsCOMPtr<nsIAtom> atom = do_GetAtom(str);
  NS_ENSURE_TRUE(atom, nsnull);

  return atom.forget();
}

// static
nsresult
IndexedDatabaseManager::FireWindowOnError(nsPIDOMWindow* aOwner,
                                          nsEventChainPostVisitor& aVisitor)
{
  NS_ENSURE_TRUE(aVisitor.mDOMEvent, NS_ERROR_UNEXPECTED);
  if (!aOwner) {
    return NS_OK;
  }

  if (aVisitor.mEventStatus == nsEventStatus_eConsumeNoDefault) {
    return NS_OK;
  }

  nsString type;
  nsresult rv = aVisitor.mDOMEvent->GetType(type);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!type.EqualsLiteral(ERROR_EVT_STR)) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMEventTarget> eventTarget;
  rv = aVisitor.mDOMEvent->GetTarget(getter_AddRefs(eventTarget));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIIDBRequest> strongRequest = do_QueryInterface(eventTarget);
  IDBRequest* request = static_cast<IDBRequest*>(strongRequest.get());
  NS_ENSURE_TRUE(request, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIDOMDOMError> error;
  rv = request->GetError(getter_AddRefs(error));
  NS_ENSURE_SUCCESS(rv, rv);

  nsString errorName;
  if (error) {
    rv = error->GetName(errorName);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsScriptErrorEvent event(true, NS_LOAD_ERROR);
  request->FillScriptErrorEvent(&event);
  event.errorMsg = errorName.get();

  nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(aOwner));
  NS_ASSERTION(sgo, "How can this happen?!");

  nsEventStatus status = nsEventStatus_eIgnore;
  if (NS_FAILED(sgo->HandleScriptError(&event, &status))) {
    NS_WARNING("Failed to dispatch script error event");
    status = nsEventStatus_eIgnore;
  }

  bool preventDefaultCalled = status == nsEventStatus_eConsumeNoDefault;
  if (preventDefaultCalled) {
    return NS_OK;
  }

  // Log an error to the error console.
  nsCOMPtr<nsIScriptError> scriptError =
    do_CreateInstance(NS_SCRIPTERROR_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (NS_FAILED(scriptError->InitWithWindowID(event.errorMsg,
                                              event.fileName,
                                              nsnull, event.lineNr,
                                              0, 0,
                                              "IndexedDB",
                                              aOwner->WindowID()))) {
    NS_WARNING("Failed to init script error!");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIConsoleService> consoleService =
    do_GetService(NS_CONSOLESERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return consoleService->LogMessage(scriptError);
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
    mLiveDatabases.Put(aDatabase->Origin(), newArray);
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
IndexedDatabaseManager::AcquireExclusiveAccess(
                                           const nsACString& aOrigin,
                                           IDBDatabase* aDatabase,
                                           AsyncConnectionHelper* aHelper,
                                           nsIRunnable* aRunnable,
                                           WaitingOnDatabasesCallback aCallback,
                                           void* aClosure)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aDatabase || aHelper, "Need a helper with a database!");
  NS_ASSERTION(aDatabase || aRunnable, "Need a runnable without a database!");

  // Find the right SynchronizedOp.
  SynchronizedOp* op =
    FindSynchronizedOp(aOrigin, aDatabase ? aDatabase->Id() : nsnull);

  NS_ASSERTION(op, "We didn't find a SynchronizedOp?");
  NS_ASSERTION(!op->mHelper, "SynchronizedOp already has a helper?!?");
  NS_ASSERTION(!op->mRunnable, "SynchronizedOp already has a runnable?!?");

  nsTArray<IDBDatabase*>* array;
  mLiveDatabases.Get(aOrigin, &array);

  // We need to wait for the databases to go away.
  // Hold on to all database objects that represent the same database file
  // (except the one that is requesting this version change).
  nsTArray<nsRefPtr<IDBDatabase> > liveDatabases;

  if (array) {
    if (aDatabase) {
      // Grab all databases that are not yet closed but whose database id match
      // the one we're looking for.
      for (PRUint32 index = 0; index < array->Length(); index++) {
        IDBDatabase*& database = array->ElementAt(index);
        if (!database->IsClosed() &&
            database != aDatabase &&
            database->Id() == aDatabase->Id()) {
          liveDatabases.AppendElement(database);
        }
      }
    }
    else {
      // We want *all* databases, even those that are closed, if we're going to
      // clear the origin.
      liveDatabases.AppendElements(*array);
    }
  }

  op->mHelper = aHelper;
  op->mRunnable = aRunnable;

  if (!liveDatabases.IsEmpty()) {
    NS_ASSERTION(op->mDatabases.IsEmpty(),
                 "How do we already have databases here?");
    op->mDatabases.AppendElements(liveDatabases);

    // Give our callback the databases so it can decide what to do with them.
    aCallback(liveDatabases, aClosure);

    NS_ASSERTION(liveDatabases.IsEmpty(),
                 "Should have done something with the array!");

    if (aDatabase) {
      // Wait for those databases to close.
      return NS_OK;
    }
  }

  // If we're trying to open a database and nothing blocks it, or if we're
  // clearing an origin, then go ahead and schedule the op.
  nsresult rv = RunSynchronizedOp(aDatabase, op);
  NS_ENSURE_SUCCESS(rv, rv);

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
  mLiveDatabases.EnumerateRead(EnumerateToTArray<IDBDatabase*>,
                               &liveDatabases);

  FileService* service = FileService::Get();
  TransactionThreadPool* pool = TransactionThreadPool::Get();

  for (PRUint32 index = 0; index < liveDatabases.Length(); index++) {
    IDBDatabase*& database = liveDatabases[index];
    if (database->GetOwner() == aWindow) {
      if (NS_FAILED(database->Close())) {
        NS_WARNING("Failed to close database for dying window!");
      }

      if (service) {
        service->AbortLockedFilesForStorage(database);
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
  mLiveDatabases.EnumerateRead(EnumerateToTArray<IDBDatabase*>,
                               &liveDatabases);
  
  FileService* service = FileService::Get();
  TransactionThreadPool* pool = TransactionThreadPool::Get();
  if (!service && !pool) {
    return false;
  }

  for (PRUint32 index = 0; index < liveDatabases.Length(); index++) {
    IDBDatabase*& database = liveDatabases[index];
    if (database->GetOwner() == aWindow &&
        ((service && service->HasLockedFilesForStorage(database)) ||
         (pool && pool->HasTransactionsForDatabase(database)))) {
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
  SynchronizedOp* op = FindSynchronizedOp(aDatabase->Origin(), aDatabase->Id());
  if (op) {
    // This database is in the scope of this SynchronizedOp.  Remove it
    // from the list if necessary.
    if (op->mDatabases.RemoveElement(aDatabase)) {
      // Now set up the helper if there are no more live databases.
      NS_ASSERTION(op->mHelper || op->mRunnable,
                   "How did we get rid of the helper/runnable before "
                    "removing the last database?");
      if (op->mDatabases.IsEmpty()) {
        // At this point, all databases are closed, so no new transactions
        // can be started.  There may, however, still be outstanding
        // transactions that have not completed.  We need to wait for those
        // before we dispatch the helper.
        if (NS_FAILED(RunSynchronizedOp(aDatabase, op))) {
          NS_WARNING("Failed to run synchronized op!");
        }
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
  nsresult rv = GetDirectoryForOrigin(aOrigin, getter_AddRefs(directory));
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
  validSubdirs.Init(20);

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

    if (StringEndsWith(leafName, NS_LITERAL_STRING(".sqlite-journal"))) {
      continue;
    }

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

    validSubdirs.PutEntry(dbBaseFilename);
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

  mFileManagers.Put(aOrigin, fileManagers);
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

    mQuotaHelperHash.Put(window, helper);

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

#ifdef DEBUG
//static
bool
IndexedDatabaseManager::IsMainProcess()
{
  NS_ASSERTION(gInstance,
               "IsMainProcess() called before indexedDB has been initialized!");
  NS_ASSERTION((XRE_GetProcessType() == GeckoProcessType_Default) ==
               sIsMainProcess, "XRE_GetProcessType changed its tune!");
  return sIsMainProcess;
}
#endif

already_AddRefed<FileManager>
IndexedDatabaseManager::GetOrCreateFileManager(const nsACString& aOrigin,
                                               const nsAString& aDatabaseName)
{
  nsTArray<nsRefPtr<FileManager> >* array;
  if (!mFileManagers.Get(aOrigin, &array)) {
    nsAutoPtr<nsTArray<nsRefPtr<FileManager> > > newArray(
      new nsTArray<nsRefPtr<FileManager> >());
    mFileManagers.Put(aOrigin, newArray);
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
IndexedDatabaseManager::RunSynchronizedOp(IDBDatabase* aDatabase,
                                          SynchronizedOp* aOp)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aOp, "Null pointer!");
  NS_ASSERTION(!aDatabase || aOp->mHelper, "No helper on this op!");
  NS_ASSERTION(aDatabase || aOp->mRunnable, "No runnable on this op!");
  NS_ASSERTION(!aDatabase || aOp->mDatabases.IsEmpty(),
               "This op isn't ready to run!");

  FileService* service = FileService::Get();
  TransactionThreadPool* pool = TransactionThreadPool::Get();

  nsTArray<nsRefPtr<IDBDatabase> > databases;
  if (aDatabase) {
    if (service || pool) {
      databases.AppendElement(aDatabase);
    }
  }
  else {
    aOp->mDatabases.SwapElements(databases);
  }

  PRUint32 waitCount = service && pool && !databases.IsEmpty() ? 2 : 1;

  nsRefPtr<WaitForTransactionsToFinishRunnable> runnable =
    new WaitForTransactionsToFinishRunnable(aOp, waitCount);

  // There's no point in delaying if we don't yet have a transaction thread pool
  // or a file service. Also, if we're not waiting on any databases then we can
  // also run immediately.
  if (!(service || pool) || databases.IsEmpty()) {
    nsresult rv = runnable->Run();
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  // Ask each service to call us back when they're done with this database.
  if (service) {
    // Have to copy here in case the pool needs a list too.
    nsTArray<nsCOMPtr<nsIFileStorage> > array;
    array.AppendElements(databases);

    if (!service->WaitForAllStoragesToComplete(array, runnable)) {
      NS_WARNING("Failed to wait for storages to complete!");
      return NS_ERROR_FAILURE;
    }
  }

  if (pool && !pool->WaitForAllDatabasesToComplete(databases, runnable)) {
    NS_WARNING("Failed to wait for databases to complete!");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
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
    return runnable->TakeShortcut();
  }

  // Otherwise put the computation runnable in the queue.
  rv = WaitForOpenAllowed(origin, nsnull, runnable);
  NS_ENSURE_SUCCESS(rv, rv);

  runnable->AdvanceState();

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
  nsRefPtr<OriginClearRunnable> runnable = new OriginClearRunnable(origin);

  rv = WaitForOpenAllowed(origin, nsnull, runnable);
  NS_ENSURE_SUCCESS(rv, rv);

  runnable->AdvanceState();

  // Give the runnable some help by invalidating any databases in the way. We
  // need to grab references to any live databases here to prevent them from
  // dying while we invalidate them.
  nsTArray<nsRefPtr<IDBDatabase> > liveDatabases;

  nsTArray<IDBDatabase*>* array;
  if (mLiveDatabases.Get(origin, &array)) {
    liveDatabases.AppendElements(*array);
  }

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

  if (!strcmp(aTopic, PROFILE_BEFORE_CHANGE_OBSERVER_ID)) {
    // Setting this flag prevents the service from being recreated and prevents
    // further databases from being created.
    if (PR_ATOMIC_SET(&gShutdown, 1)) {
      NS_ERROR("Shutdown more than once?!");
    }

    if (sIsMainProcess) {
      FileService* service = FileService::Get();
      if (service) {
        // This should only wait for IDB databases (file storages) to complete.
        // Other file storages may still have running locked files.
        // If the necko service (thread pool) gets the shutdown notification
        // first then the sync loop won't be processed at all, otherwise it will
        // lock the main thread until all IDB file storages are finished.

        nsTArray<nsCOMPtr<nsIFileStorage> >
          liveDatabases(mLiveDatabases.Count());
        mLiveDatabases.EnumerateRead(
                                  EnumerateToTArray<nsCOMPtr<nsIFileStorage> >,
                                  &liveDatabases);

        if (!liveDatabases.IsEmpty()) {
          nsRefPtr<WaitForLockedFilesToFinishRunnable> runnable =
            new WaitForLockedFilesToFinishRunnable();

          if (!service->WaitForAllStoragesToComplete(liveDatabases,
                                                     runnable)) {
            NS_WARNING("Failed to wait for databases to complete!");
          }

          nsIThread* thread = NS_GetCurrentThread();
          while (runnable->IsBusy()) {
            if (!NS_ProcessNextEvent(thread)) {
              NS_ERROR("Failed to process next event!");
              break;
            }
          }
        }
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
    }

    mFileManagers.EnumerateRead(InvalidateAllFileManagers, nsnull);

    if (PR_ATOMIC_SET(&gClosed, 1)) {
      NS_ERROR("Close more than once?!");
    }

    return NS_OK;
  }

  if (!strcmp(aTopic, NS_TIMER_CALLBACK_TOPIC)) {
    NS_ASSERTION(sIsMainProcess, "Should only happen in the main process!");

    NS_WARNING("Some database operations are taking longer than expected "
               "during shutdown and will be aborted!");

    // Grab all live databases, for all origins.
    nsAutoTArray<IDBDatabase*, 50> liveDatabases;
    mLiveDatabases.EnumerateRead(EnumerateToTArray<IDBDatabase*>,
                                 &liveDatabases);

    // Invalidate them all.
    if (!liveDatabases.IsEmpty()) {
      PRUint32 count = liveDatabases.Length();
      for (PRUint32 index = 0; index < count; index++) {
        liveDatabases[index]->Invalidate();
      }
    }

    return NS_OK;
  }

  NS_NOTREACHED("Unknown topic!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(IndexedDatabaseManager::OriginClearRunnable,
                              nsIRunnable)

// static
void
IndexedDatabaseManager::
OriginClearRunnable::InvalidateOpenedDatabases(
                                   nsTArray<nsRefPtr<IDBDatabase> >& aDatabases,
                                   void* aClosure)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  OriginClearRunnable* self = static_cast<OriginClearRunnable*>(aClosure);

  nsTArray<nsRefPtr<IDBDatabase> > databases;
  databases.SwapElements(aDatabases);

  for (PRUint32 index = 0; index < databases.Length(); index++) {
    databases[index]->Invalidate();
  }

  DatabaseInfo::RemoveAllForOrigin(self->mOrigin);
}

NS_IMETHODIMP
IndexedDatabaseManager::OriginClearRunnable::Run()
{
  IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
  NS_ASSERTION(mgr, "This should never fail!");

  switch (mCallbackState) {
    case Pending: {
      NS_NOTREACHED("Should never get here without being dispatched!");
      return NS_ERROR_UNEXPECTED;
    }

    case OpenAllowed: {
      NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

      AdvanceState();

      // Now we have to wait until the thread pool is done with all of the
      // databases we care about.
      nsresult rv =
        mgr->AcquireExclusiveAccess(mOrigin, this, InvalidateOpenedDatabases,
                                    this);
      NS_ENSURE_SUCCESS(rv, rv);

      return NS_OK;
    }

    case IO: {
      NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

      AdvanceState();

      // Remove the directory that contains all our databases.
      nsCOMPtr<nsIFile> directory;
      nsresult rv =
        mgr->GetDirectoryForOrigin(mOrigin, getter_AddRefs(directory));
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Failed to get directory to remove!");

      if (NS_SUCCEEDED(rv)) {
        bool exists;
        rv = directory->Exists(&exists);
        NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                         "Failed to check that the directory exists!");

        if (NS_SUCCEEDED(rv) && exists && NS_FAILED(directory->Remove(true))) {
          // This should never fail if we've closed all database connections
          // correctly...
          NS_ERROR("Failed to remove directory!");
        }
      }

      // Now dispatch back to the main thread.
      if (NS_FAILED(NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL))) {
        NS_WARNING("Failed to dispatch to main thread!");
        return NS_ERROR_FAILURE;
      }

      return NS_OK;
    }

    case Complete: {
      NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

      mgr->InvalidateFileManagersForOrigin(mOrigin);

      // Tell the IndexedDatabaseManager that we're done.
      mgr->AllowNextSynchronizedOp(mOrigin, nsnull);

      return NS_OK;
    }

    default:
      NS_ERROR("Unknown state value!");
      return NS_ERROR_UNEXPECTED;
  }

  NS_NOTREACHED("Should never get here!");
  return NS_ERROR_UNEXPECTED;
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
  mCanceled(0),
  mCallbackState(Pending)
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
IndexedDatabaseManager::AsyncUsageRunnable::TakeShortcut()
{
  NS_ASSERTION(mCallbackState == Pending, "Huh?");

  nsresult rv = NS_DispatchToCurrentThread(this);
  NS_ENSURE_SUCCESS(rv, rv);

  mCallbackState = Shortcut;
  return NS_OK;
}

nsresult
IndexedDatabaseManager::AsyncUsageRunnable::RunInternal()
{
  IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
  NS_ASSERTION(mgr, "This should never fail!");

  if (mCanceled) {
    return NS_OK;
  }

  switch (mCallbackState) {
    case Pending: {
      NS_NOTREACHED("Should never get here without being dispatched!");
      return NS_ERROR_UNEXPECTED;
    }

    case OpenAllowed: {
      NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

      AdvanceState();

      if (NS_FAILED(mgr->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL))) {
        NS_WARNING("Failed to dispatch to the IO thread!");
      }

      return NS_OK;
    }

    case IO: {
      NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

      AdvanceState();

      // Get the directory that contains all the database files we care about.
      nsCOMPtr<nsIFile> directory;
      nsresult rv = mgr->GetDirectoryForOrigin(mOrigin, getter_AddRefs(directory));
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

      // Run dispatches us back to the main thread.
      return NS_OK;
    }

    case Complete: // Fall through
    case Shortcut: {
      NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

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
      mgr->OnUsageCheckComplete(this);
      if (mCallbackState == Complete) {
        mgr->AllowNextSynchronizedOp(mOrigin, nsnull);
      }

      return NS_OK;
    }

    default:
      NS_ERROR("Unknown state value!");
      return NS_ERROR_UNEXPECTED;
  }

  NS_NOTREACHED("Should never get here!");
  return NS_ERROR_UNEXPECTED;
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

    NS_ASSERTION(fileSize >= 0, "Negative size?!");

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

  return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(
                    IndexedDatabaseManager::WaitForTransactionsToFinishRunnable,
                    nsIRunnable)

NS_IMETHODIMP
IndexedDatabaseManager::WaitForTransactionsToFinishRunnable::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mOp, "Null op!");
  NS_ASSERTION(mOp->mHelper || mOp->mRunnable, "Nothing to run!");
  NS_ASSERTION(mCountdown, "Wrong countdown!");

  if (--mCountdown) {
    return NS_OK;
  }

  // Don't hold the callback alive longer than necessary.
  nsRefPtr<AsyncConnectionHelper> helper;
  helper.swap(mOp->mHelper);

  nsCOMPtr<nsIRunnable> runnable;
  runnable.swap(mOp->mRunnable);

  mOp = nsnull;

  nsresult rv;

  if (helper && helper->HasTransaction()) {
    // If the helper has a transaction, dispatch it to the transaction
    // threadpool.
    rv = helper->DispatchToTransactionPool();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    // Otherwise, dispatch it to the IO thread.
    IndexedDatabaseManager* manager = IndexedDatabaseManager::Get();
    NS_ASSERTION(manager, "We should definitely have a manager here");

    nsIEventTarget* target = manager->IOThread();

    rv = helper ?
         helper->Dispatch(target) :
         target->Dispatch(runnable, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // The helper or runnable is responsible for calling
  // IndexedDatabaseManager::AllowNextSynchronizedOp.
  return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(
                     IndexedDatabaseManager::WaitForLockedFilesToFinishRunnable,
                     nsIRunnable)

NS_IMETHODIMP
IndexedDatabaseManager::WaitForLockedFilesToFinishRunnable::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  mBusy = false;

  return NS_OK;
}

IndexedDatabaseManager::
SynchronizedOp::SynchronizedOp(const nsACString& aOrigin,
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
  NS_ENSURE_ARG(!JSVAL_IS_PRIMITIVE(aObj));

  // Instantiating this class will register exception providers so even 
  // in xpcshell we will get typed (dom) exceptions, instead of general
  // exceptions.
  nsCOMPtr<nsIDOMScriptObjectFactory> sof(do_GetService(kDOMSOF_CID));

  JSObject* obj = JSVAL_TO_OBJECT(aObj);

  JSObject* global = JS_GetGlobalForObject(aCx, obj);

  nsRefPtr<IDBFactory> factory;
  nsresult rv = IDBFactory::Create(aCx, global, getter_AddRefs(factory));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  NS_ASSERTION(factory, "This should never fail for chrome!");

  jsval indexedDBVal;
  rv = nsContentUtils::WrapNative(aCx, obj, factory, &indexedDBVal);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!JS_DefineProperty(aCx, obj, "indexedDB", indexedDBVal, nsnull,
                         nsnull, JSPROP_ENUMERATE)) {
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

  // sqlite3_quota_remove won't actually remove anything if we're not tracking
  // the quota here. Manually remove the file if it exists.
  nsresult rv;
  nsCOMPtr<nsIFile> file =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = file->InitWithPath(mFilePath);
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists;
  rv = file->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {
    rv = file->Remove(false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}
