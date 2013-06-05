/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaManager.h"

#include "mozIApplicationClearPrivateDataParams.h"
#include "nsIAtom.h"
#include "nsIFile.h"
#include "nsIObserverService.h"
#include "nsIOfflineStorage.h"
#include "nsIPrincipal.h"
#include "nsIQuotaRequest.h"
#include "nsIRunnable.h"
#include "nsISimpleEnumerator.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsITimer.h"
#include "nsIURI.h"
#include "nsIUsageCallback.h"

#include <algorithm>
#include "GeckoProfiler.h"
#include "mozilla/dom/file/FileService.h"
#include "mozilla/dom/indexedDB/Client.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsCRTGlue.h"
#include "nsDirectoryServiceUtils.h"
#include "nsScriptSecurityManager.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "xpcpublic.h"

#include "AcquireListener.h"
#include "CheckQuotaHelper.h"
#include "OriginOrPatternString.h"
#include "QuotaObject.h"
#include "StorageMatcher.h"
#include "UsageRunnable.h"
#include "Utilities.h"

// The amount of time, in milliseconds, that our IO thread will stay alive
// after the last event it processes.
#define DEFAULT_THREAD_TIMEOUT_MS 30000

// The amount of time, in milliseconds, that we will wait for active storage
// transactions on shutdown before aborting them.
#define DEFAULT_SHUTDOWN_TIMER_MS 30000

// Amount of space that storages may use by default in megabytes.
#define DEFAULT_QUOTA_MB 50

// Preference that users can set to override DEFAULT_QUOTA_MB
#define PREF_STORAGE_QUOTA "dom.indexedDB.warningQuota"

// profile-before-change, when we need to shut down quota manager
#define PROFILE_BEFORE_CHANGE_OBSERVER_ID "profile-before-change"

#define METADATA_FILE_NAME ".metadata"

USING_QUOTA_NAMESPACE
using namespace mozilla::dom;
using mozilla::dom::file::FileService;

BEGIN_QUOTA_NAMESPACE

// A struct that contains the information corresponding to a pending or
// running operation that requires synchronization (e.g. opening a db,
// clearing dbs for an origin, etc).
struct SynchronizedOp
{
  SynchronizedOp(const OriginOrPatternString& aOriginOrPattern,
                 nsISupports* aId);

  ~SynchronizedOp();

  // Test whether this SynchronizedOp needs to wait for the given op.
  bool
  MustWaitFor(const SynchronizedOp& aOp);

  void
  DelayRunnable(nsIRunnable* aRunnable);

  void
  DispatchDelayedRunnables();

  const OriginOrPatternString mOriginOrPattern;
  nsCOMPtr<nsISupports> mId;
  nsRefPtr<AcquireListener> mListener;
  nsTArray<nsCOMPtr<nsIRunnable> > mDelayedRunnables;
  ArrayCluster<nsIOfflineStorage*> mStorages;
};

// Responsible for clearing the storage files for a particular origin on the
// IO thread. Created when nsIQuotaManager::ClearStoragesForURI is called.
// Runs three times, first on the main thread, next on the IO thread, and then
// finally again on the main thread. While on the IO thread the runnable will
// actually remove the origin's storage files and the directory that contains
// them before dispatching itself back to the main thread. When back on the main
// thread the runnable will notify the QuotaManager that the job has been
// completed.
class OriginClearRunnable MOZ_FINAL : public nsIRunnable,
                                      public AcquireListener
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

  // AcquireListener override
  virtual nsresult
  OnExclusiveAccessAcquired() MOZ_OVERRIDE;

  OriginClearRunnable(const OriginOrPatternString& aOriginOrPattern)
  : mOriginOrPattern(aOriginOrPattern),
    mCallbackState(Pending)
  { }

  void
  AdvanceState()
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

  static void
  InvalidateOpenedStorages(nsTArray<nsCOMPtr<nsIOfflineStorage> >& aStorages,
                           void* aClosure);

  void
  DeleteFiles(QuotaManager* aQuotaManager);

private:
  OriginOrPatternString mOriginOrPattern;
  CallbackState mCallbackState;
};

// Responsible for calculating the amount of space taken up by storages of a
// certain origin. Created when nsIQuotaManager::GetUsageForURI is called.
// May be canceled with nsIQuotaRequest::Cancel. Runs three times, first
// on the main thread, next on the IO thread, and then finally again on the main
// thread. While on the IO thread the runnable will calculate the size of all
// files in the origin's directory before dispatching itself back to the main
// thread. When on the main thread the runnable will call the callback and then
// notify the QuotaManager that the job has been completed.
class AsyncUsageRunnable MOZ_FINAL : public UsageRunnable,
                                     public nsIRunnable,
                                     public nsIQuotaRequest
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
  NS_DECL_NSIQUOTAREQUEST

  AsyncUsageRunnable(uint32_t aAppId,
                     bool aInMozBrowserOnly,
                     const OriginOrPatternString& aOrigin,
                     nsIURI* aURI,
                     nsIUsageCallback* aCallback);

  void
  AdvanceState()
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

  nsresult
  TakeShortcut();

  // Run calls the RunInternal method and makes sure that we always dispatch
  // to the main thread in case of an error.
  inline nsresult
  RunInternal();

  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIUsageCallback> mCallback;
  uint32_t mAppId;
  OriginOrPatternString mOrigin;
  CallbackState mCallbackState;
  bool mInMozBrowserOnly;
};

#ifdef DEBUG
void
AssertIsOnIOThread()
{
  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "Must have a manager here!");

  bool correctThread;
  NS_ASSERTION(NS_SUCCEEDED(quotaManager->IOThread()->
                            IsOnCurrentThread(&correctThread)) && correctThread,
               "Running on the wrong thread!");
}
#endif

END_QUOTA_NAMESPACE

namespace {

QuotaManager* gInstance = nullptr;
int32_t gShutdown = 0;

int32_t gStorageQuotaMB = DEFAULT_QUOTA_MB;

// A callback runnable used by the TransactionPool when it's safe to proceed
// with a SetVersion/DeleteDatabase/etc.
class WaitForTransactionsToFinishRunnable MOZ_FINAL : public nsIRunnable
{
public:
  WaitForTransactionsToFinishRunnable(SynchronizedOp* aOp)
  : mOp(aOp), mCountdown(1)
  {
    NS_ASSERTION(mOp, "Why don't we have a runnable?");
    NS_ASSERTION(mOp->mStorages.IsEmpty(), "We're here too early!");
    NS_ASSERTION(mOp->mListener,
                 "What are we supposed to do when we're done?");
    NS_ASSERTION(mCountdown, "Wrong countdown!");
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  void
  AddRun()
  {
    mCountdown++;
  }

private:
  // The QuotaManager holds this alive.
  SynchronizedOp* mOp;
  uint32_t mCountdown;
};

class WaitForLockedFilesToFinishRunnable MOZ_FINAL : public nsIRunnable
{
public:
  WaitForLockedFilesToFinishRunnable()
  : mBusy(true)
  { }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  bool
  IsBusy() const
  {
    return mBusy;
  }

private:
  bool mBusy;
};

bool
IsMainProcess()
{
  return XRE_GetProcessType() == GeckoProcessType_Default;
}

void
SanitizeOriginString(nsCString& aOrigin)
{
  // We want profiles to be platform-independent so we always need to replace
  // the same characters on every platform. Windows has the most extensive set
  // of illegal characters so we use its FILE_ILLEGAL_CHARACTERS and
  // FILE_PATH_SEPARATOR.
  static const char kReplaceChars[] = CONTROL_CHARACTERS "/:*?\"<>|\\";

#ifdef XP_WIN
  NS_ASSERTION(!strcmp(kReplaceChars,
                       FILE_ILLEGAL_CHARACTERS FILE_PATH_SEPARATOR),
               "Illegal file characters have changed!");
#endif

  aOrigin.ReplaceChar(kReplaceChars, '+');
}

PLDHashOperator
RemoveQuotaForPatternCallback(const nsACString& aKey,
                              nsRefPtr<OriginInfo>& aValue,
                              void* aUserArg)
{
  NS_ASSERTION(!aKey.IsEmpty(), "Empty key!");
  NS_ASSERTION(aValue, "Null pointer!");
  NS_ASSERTION(aUserArg, "Null pointer!");

  const nsACString* pattern =
    static_cast<const nsACString*>(aUserArg);

  if (PatternMatchesOrigin(*pattern, aKey)) {
    return PL_DHASH_REMOVE;
  }

  return PL_DHASH_NEXT;
}

} // anonymous namespace

QuotaManager::QuotaManager()
: mCurrentWindowIndex(BAD_TLS_INDEX),
  mQuotaMutex("QuotaManager.mQuotaMutex")
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!gInstance, "More than one instance!");
}

QuotaManager::~QuotaManager()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!gInstance || gInstance == this, "Different instances!");
  gInstance = nullptr;
}

// static
QuotaManager*
QuotaManager::GetOrCreate()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (IsShuttingDown()) {
    NS_ERROR("Calling GetOrCreate() after shutdown!");
    return nullptr;
  }

  if (!gInstance) {
    nsRefPtr<QuotaManager> instance(new QuotaManager());

    nsresult rv = instance->Init();
    NS_ENSURE_SUCCESS(rv, nullptr);

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    NS_ENSURE_TRUE(obs, nullptr);

    // We need this callback to know when to shut down all our threads.
    rv = obs->AddObserver(instance, PROFILE_BEFORE_CHANGE_OBSERVER_ID, false);
    NS_ENSURE_SUCCESS(rv, nullptr);

    // The observer service will hold our last reference, don't AddRef here.
    gInstance = instance;
  }

  return gInstance;
}

// static
QuotaManager*
QuotaManager::Get()
{
  // Does not return an owning reference.
  return gInstance;
}

// static
QuotaManager*
QuotaManager::FactoryCreate()
{
  // Returns a raw pointer that carries an owning reference! Lame, but the
  // singleton factory macros force this.
  QuotaManager* quotaManager = GetOrCreate();
  NS_IF_ADDREF(quotaManager);
  return quotaManager;
}

// static
bool
QuotaManager::IsShuttingDown()
{
  return !!gShutdown;
}

nsresult
QuotaManager::Init()
{
  // We need a thread-local to hold the current window.
  NS_ASSERTION(mCurrentWindowIndex == BAD_TLS_INDEX, "Huh?");

  if (PR_NewThreadPrivateIndex(&mCurrentWindowIndex, nullptr) != PR_SUCCESS) {
    NS_ERROR("PR_NewThreadPrivateIndex failed, QuotaManager disabled");
    mCurrentWindowIndex = BAD_TLS_INDEX;
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  if (IsMainProcess()) {
    nsCOMPtr<nsIFile> dbBaseDirectory;
    rv = NS_GetSpecialDirectory(NS_APP_INDEXEDDB_PARENT_DIR,
                                getter_AddRefs(dbBaseDirectory));
    if (NS_FAILED(rv)) {
      rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                  getter_AddRefs(dbBaseDirectory));
    }
    NS_ENSURE_SUCCESS(rv, rv);

    rv = dbBaseDirectory->Append(NS_LITERAL_STRING("indexedDB"));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = dbBaseDirectory->GetPath(mStorageBasePath);
    NS_ENSURE_SUCCESS(rv, rv);

    // Make a lazy thread for any IO we need (like clearing or enumerating the
    // contents of storage directories).
    mIOThread = new LazyIdleThread(DEFAULT_THREAD_TIMEOUT_MS,
                                   NS_LITERAL_CSTRING("Storage I/O"),
                                   LazyIdleThread::ManualShutdown);

    // Make a timer here to avoid potential failures later. We don't actually
    // initialize the timer until shutdown.
    mShutdownTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
    NS_ENSURE_TRUE(mShutdownTimer, NS_ERROR_FAILURE);
  }

  if (NS_FAILED(Preferences::AddIntVarCache(&gStorageQuotaMB,
                                            PREF_STORAGE_QUOTA,
                                            DEFAULT_QUOTA_MB))) {
    NS_WARNING("Unable to respond to quota pref changes!");
    gStorageQuotaMB = DEFAULT_QUOTA_MB;
  }

  mOriginInfos.Init();
  mCheckQuotaHelpers.Init();
  mLiveStorages.Init();

  MOZ_STATIC_ASSERT(Client::IDB == 0 && Client::TYPE_MAX == 1,
                    "Fix the registration!");

  NS_ASSERTION(mClients.Capacity() == Client::TYPE_MAX,
               "Should be using an auto array with correct capacity!");

  // Register IndexedDB
  mClients.AppendElement(new indexedDB::Client());

  return NS_OK;
}

void
QuotaManager::InitQuotaForOrigin(const nsACString& aOrigin,
                                 int64_t aLimitBytes,
                                 int64_t aUsageBytes)
{
  MOZ_ASSERT(aUsageBytes >= 0);
  MOZ_ASSERT(aLimitBytes > 0);
  MOZ_ASSERT(aUsageBytes <= aLimitBytes);

  OriginInfo* info = new OriginInfo(aOrigin, aLimitBytes, aUsageBytes);

  MutexAutoLock lock(mQuotaMutex);

  NS_ASSERTION(!mOriginInfos.GetWeak(aOrigin), "Replacing an existing entry!");
  mOriginInfos.Put(aOrigin, info);
}

void
QuotaManager::DecreaseUsageForOrigin(const nsACString& aOrigin,
                                     int64_t aSize)
{
  MutexAutoLock lock(mQuotaMutex);

  nsRefPtr<OriginInfo> originInfo;
  mOriginInfos.Get(aOrigin, getter_AddRefs(originInfo));

  if (originInfo) {
    originInfo->mUsage -= aSize;
  }
}

void
QuotaManager::RemoveQuotaForPattern(const nsACString& aPattern)
{
  NS_ASSERTION(!aPattern.IsEmpty(), "Empty pattern!");

  MutexAutoLock lock(mQuotaMutex);

  mOriginInfos.Enumerate(RemoveQuotaForPatternCallback,
                         const_cast<nsACString*>(&aPattern));
}

already_AddRefed<QuotaObject>
QuotaManager::GetQuotaObject(const nsACString& aOrigin,
                             nsIFile* aFile)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsString path;
  nsresult rv = aFile->GetPath(path);
  NS_ENSURE_SUCCESS(rv, nullptr);

  int64_t fileSize;

  bool exists;
  rv = aFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, nullptr);

  if (exists) {
    rv = aFile->GetFileSize(&fileSize);
    NS_ENSURE_SUCCESS(rv, nullptr);
  }
  else {
    fileSize = 0;
  }

  QuotaObject* info = nullptr;
  {
    MutexAutoLock lock(mQuotaMutex);

    nsRefPtr<OriginInfo> originInfo;
    mOriginInfos.Get(aOrigin, getter_AddRefs(originInfo));

    if (!originInfo) {
      return nullptr;
    }

    originInfo->mQuotaObjects.Get(path, &info);

    if (!info) {
      info = new QuotaObject(originInfo, path, fileSize);
      originInfo->mQuotaObjects.Put(path, info);
    }
  }

  nsRefPtr<QuotaObject> result = info;
  return result.forget();
}

already_AddRefed<QuotaObject>
QuotaManager::GetQuotaObject(const nsACString& aOrigin,
                             const nsAString& aPath)
{
  nsresult rv;
  nsCOMPtr<nsIFile> file = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, nullptr);

  rv = file->InitWithPath(aPath);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return GetQuotaObject(aOrigin, file);
}

bool
QuotaManager::RegisterStorage(nsIOfflineStorage* aStorage)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aStorage, "Null pointer!");

  // Don't allow any new storages to be created after shutdown.
  if (IsShuttingDown()) {
    return false;
  }

  // Add this storage to its origin info if it exists, create it otherwise.
  const nsACString& origin = aStorage->Origin();
  ArrayCluster<nsIOfflineStorage*>* cluster;
  if (!mLiveStorages.Get(origin, &cluster)) {
    cluster = new ArrayCluster<nsIOfflineStorage*>();
    mLiveStorages.Put(origin, cluster);
  }
  (*cluster)[aStorage->GetClient()->GetType()].AppendElement(aStorage);

  return true;
}

void
QuotaManager::UnregisterStorage(nsIOfflineStorage* aStorage)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aStorage, "Null pointer!");

  // Remove this storage from its origin array, maybe remove the array if it
  // is then empty.
  const nsACString& origin = aStorage->Origin();
  ArrayCluster<nsIOfflineStorage*>* cluster;
  if (mLiveStorages.Get(origin, &cluster) &&
      (*cluster)[aStorage->GetClient()->GetType()].RemoveElement(aStorage)) {
    if (cluster->IsEmpty()) {
      mLiveStorages.Remove(origin);
    }
    return;
  }
  NS_ERROR("Didn't know anything about this storage!");
}

void
QuotaManager::OnStorageClosed(nsIOfflineStorage* aStorage)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aStorage, "Null pointer!");

  // Check through the list of SynchronizedOps to see if any are waiting for
  // this storage to close before proceeding.
  SynchronizedOp* op =
    FindSynchronizedOp(aStorage->Origin(), aStorage->Id());
  if (op) {
    Client::Type clientType = aStorage->GetClient()->GetType();

    // This storage is in the scope of this SynchronizedOp.  Remove it
    // from the list if necessary.
    if (op->mStorages[clientType].RemoveElement(aStorage)) {
      // Now set up the helper if there are no more live storages.
      NS_ASSERTION(op->mListener,
                   "How did we get rid of the listener before removing the "
                    "last storage?");
      if (op->mStorages[clientType].IsEmpty()) {
        // At this point, all storages are closed, so no new transactions
        // can be started.  There may, however, still be outstanding
        // transactions that have not completed.  We need to wait for those
        // before we dispatch the helper.
        if (NS_FAILED(RunSynchronizedOp(aStorage, op))) {
          NS_WARNING("Failed to run synchronized op!");
        }
      }
    }
  }
}

void
QuotaManager::AbortCloseStoragesForWindow(nsPIDOMWindow* aWindow)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aWindow, "Null pointer!");

  FileService* service = FileService::Get();

  StorageMatcher<ArrayCluster<nsIOfflineStorage*> > liveStorages;
  liveStorages.Find(mLiveStorages);

  for (uint32_t i = 0; i < Client::TYPE_MAX; i++) {
    nsRefPtr<Client>& client = mClients[i];
    bool utilized = service && client->IsFileServiceUtilized();
    bool activated = client->IsTransactionServiceActivated();

    nsTArray<nsIOfflineStorage*>& array = liveStorages[i];
    for (uint32_t j = 0; j < array.Length(); j++) {
      nsIOfflineStorage*& storage = array[j];

      if (storage->IsOwned(aWindow)) {
        if (NS_FAILED(storage->Close())) {
          NS_WARNING("Failed to close storage for dying window!");
        }

        if (utilized) {
          service->AbortLockedFilesForStorage(storage);
        }

        if (activated) {
          client->AbortTransactionsForStorage(storage);
        }
      }
    }
  }
}

bool
QuotaManager::HasOpenTransactions(nsPIDOMWindow* aWindow)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aWindow, "Null pointer!");

  FileService* service = FileService::Get();

  nsAutoPtr<StorageMatcher<ArrayCluster<nsIOfflineStorage*> > > liveStorages;

  for (uint32_t i = 0; i < Client::TYPE_MAX; i++) {
    nsRefPtr<Client>& client = mClients[i];
    bool utilized = service && client->IsFileServiceUtilized();
    bool activated = client->IsTransactionServiceActivated();

    if (utilized || activated) {
      if (!liveStorages) {
        liveStorages = new StorageMatcher<ArrayCluster<nsIOfflineStorage*> >();
        liveStorages->Find(mLiveStorages);
      }

      nsTArray<nsIOfflineStorage*>& storages = liveStorages->ArrayAt(i);
      for (uint32_t j = 0; j < storages.Length(); j++) {
        nsIOfflineStorage*& storage = storages[j];

        if (storage->IsOwned(aWindow) &&
            ((utilized && service->HasLockedFilesForStorage(storage)) ||
             (activated && client->HasTransactionsForStorage(storage)))) {
          return true;
        }
      }
    }
  }

  return false;
}

nsresult
QuotaManager::WaitForOpenAllowed(const OriginOrPatternString& aOriginOrPattern,
                                 nsIAtom* aId,
                                 nsIRunnable* aRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aOriginOrPattern.IsEmpty(), "Empty pattern!");
  NS_ASSERTION(aRunnable, "Null pointer!");

  nsAutoPtr<SynchronizedOp> op(new SynchronizedOp(aOriginOrPattern, aId));

  // See if this runnable needs to wait.
  bool delayed = false;
  for (uint32_t index = mSynchronizedOps.Length(); index > 0; index--) {
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
QuotaManager::AllowNextSynchronizedOp(
                                  const OriginOrPatternString& aOriginOrPattern,
                                  nsIAtom* aId)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aOriginOrPattern.IsEmpty(), "Empty origin/pattern!");

  uint32_t count = mSynchronizedOps.Length();
  for (uint32_t index = 0; index < count; index++) {
    nsAutoPtr<SynchronizedOp>& op = mSynchronizedOps[index];
    if (op->mOriginOrPattern.IsOrigin() == aOriginOrPattern.IsOrigin() &&
        op->mOriginOrPattern == aOriginOrPattern) {
      if (op->mId == aId) {
        NS_ASSERTION(op->mStorages.IsEmpty(), "How did this happen?");

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
QuotaManager::GetDirectoryForOrigin(const nsACString& aASCIIOrigin,
                                    nsIFile** aDirectory) const
{
  nsresult rv;
  nsCOMPtr<nsIFile> directory =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = directory->InitWithPath(mStorageBasePath);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString originSanitized(aASCIIOrigin);
  SanitizeOriginString(originSanitized);

  rv = directory->Append(NS_ConvertASCIItoUTF16(originSanitized));
  NS_ENSURE_SUCCESS(rv, rv);

  directory.forget(aDirectory);
  return NS_OK;
}

nsresult
QuotaManager::EnsureOriginIsInitialized(const nsACString& aOrigin,
                                        bool aTrackQuota,
                                        nsIFile** aDirectory)
{
  AssertIsOnIOThread();

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

    nsCOMPtr<nsIFile> metadataFile;
    rv = directory->Clone(getter_AddRefs(metadataFile));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = metadataFile->Append(NS_LITERAL_STRING(METADATA_FILE_NAME));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = metadataFile->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (mInitializedOrigins.Contains(aOrigin)) {
    NS_ADDREF(*aDirectory = directory);
    return NS_OK;
  }

  rv = MaybeUpgradeOriginDirectory(directory);
  NS_ENSURE_SUCCESS(rv, rv);

  // We need to initialize directories of all clients if they exists and also
  // get the total usage to initialize the quota.
  nsAutoPtr<UsageRunnable> runnable;
  if (aTrackQuota) {
    runnable = new UsageRunnable();
  }

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

    if (leafName.EqualsLiteral(METADATA_FILE_NAME)) {
      continue;
    }

    bool isDirectory;
    rv = file->IsDirectory(&isDirectory);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!isDirectory) {
      NS_WARNING("Unknown file found!");
      return NS_ERROR_UNEXPECTED;
    }

    Client::Type clientType;
    rv = Client::TypeFromText(leafName, clientType);
    if (NS_FAILED(rv)) {
      NS_WARNING("Unknown directory found!");
      return NS_ERROR_UNEXPECTED;
    }

    rv = mClients[clientType]->InitOrigin(aOrigin, runnable);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aTrackQuota) {
    uint64_t quotaMaxBytes = GetStorageQuotaMB() * 1024 * 1024;
    uint64_t totalUsageBytes = runnable->TotalUsage();

    if (totalUsageBytes > quotaMaxBytes) {
      NS_WARNING("Origin is already using more storage than allowed by quota!");
      return NS_ERROR_UNEXPECTED;
    }

    // XXX This signed/unsigned mismatch must be fixed.
    int64_t limit = quotaMaxBytes >= uint64_t(INT64_MAX) ?
                    INT64_MAX :
                    int64_t(quotaMaxBytes);

    int64_t usage = totalUsageBytes >= uint64_t(INT64_MAX) ?
                    INT64_MAX :
                    int64_t(totalUsageBytes);

    InitQuotaForOrigin(aOrigin, limit, usage);
  }

  mInitializedOrigins.AppendElement(aOrigin);

  NS_ADDREF(*aDirectory = directory);
  return NS_OK;
}

void
QuotaManager::OriginClearCompleted(const nsACString& aPattern)
{
  AssertIsOnIOThread();

  int32_t i;
  for (i = mInitializedOrigins.Length() - 1; i >= 0; i--) {
    if (PatternMatchesOrigin(aPattern, mInitializedOrigins[i])) {
      mInitializedOrigins.RemoveElementAt(i);
    }
  }

  for (i = 0; i < Client::TYPE_MAX; i++) {
    mClients[i]->OnOriginClearCompleted(aPattern);
  }
}

already_AddRefed<mozilla::dom::quota::Client>
QuotaManager::GetClient(Client::Type aClientType)
{
  nsRefPtr<Client> client = mClients.SafeElementAt(aClientType);
  return client.forget();
}

// static
uint32_t
QuotaManager::GetStorageQuotaMB()
{
  return uint32_t(std::max(gStorageQuotaMB, 0));
}

// static
already_AddRefed<nsIAtom>
QuotaManager::GetStorageId(const nsACString& aOrigin,
                           const nsAString& aName)
{
  nsCString str(aOrigin);
  str.Append("*");
  str.Append(NS_ConvertUTF16toUTF8(aName));

  nsCOMPtr<nsIAtom> atom = do_GetAtom(str);
  NS_ENSURE_TRUE(atom, nullptr);

  return atom.forget();
}

// static
nsresult
QuotaManager::GetASCIIOriginFromURI(nsIURI* aURI,
                                    uint32_t aAppId,
                                    bool aInMozBrowser,
                                    nsACString& aASCIIOrigin)
{
  NS_ASSERTION(aURI, "Null uri!");

  nsCString origin;
  mozilla::GetExtendedOrigin(aURI, aAppId, aInMozBrowser, origin);

  if (origin.IsEmpty()) {
    NS_WARNING("GetExtendedOrigin returned empty string!");
    return NS_ERROR_FAILURE;
  }

  aASCIIOrigin.Assign(origin);
  return NS_OK;
}

// static
nsresult
QuotaManager::GetASCIIOriginFromPrincipal(nsIPrincipal* aPrincipal,
                                          nsACString& aASCIIOrigin)
{
  NS_ASSERTION(aPrincipal, "Don't hand me a null principal!");

  static const char kChromeOrigin[] = "chrome";

  nsCString origin;
  if (nsContentUtils::IsSystemPrincipal(aPrincipal)) {
    origin.AssignLiteral(kChromeOrigin);
  }
  else {
    bool isNullPrincipal;
    nsresult rv = aPrincipal->GetIsNullPrincipal(&isNullPrincipal);
    NS_ENSURE_SUCCESS(rv, rv);

    if (isNullPrincipal) {
      NS_WARNING("IndexedDB not supported from this principal!");
      return NS_ERROR_FAILURE;
    }

    rv = aPrincipal->GetExtendedOrigin(origin);
    NS_ENSURE_SUCCESS(rv, rv);

    if (origin.EqualsLiteral(kChromeOrigin)) {
      NS_WARNING("Non-chrome principal can't use chrome origin!");
      return NS_ERROR_FAILURE;
    }
  }

  aASCIIOrigin.Assign(origin);
  return NS_OK;
}

// static
nsresult
QuotaManager::GetASCIIOriginFromWindow(nsPIDOMWindow* aWindow,
                                       nsACString& aASCIIOrigin)
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
  NS_ENSURE_TRUE(sop, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPrincipal> principal = sop->GetPrincipal();
  NS_ENSURE_TRUE(principal, NS_ERROR_FAILURE);

  nsresult rv = GetASCIIOriginFromPrincipal(principal, aASCIIOrigin);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMPL_ISUPPORTS2(QuotaManager, nsIQuotaManager, nsIObserver)

NS_IMETHODIMP
QuotaManager::GetUsageForURI(nsIURI* aURI,
                             nsIUsageCallback* aCallback,
                             uint32_t aAppId,
                             bool aInMozBrowserOnly,
                             uint8_t aOptionalArgCount,
                             nsIQuotaRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_ARG_POINTER(aCallback);

  // This only works from the main process.
  NS_ENSURE_TRUE(IsMainProcess(), NS_ERROR_NOT_AVAILABLE);

  if (!aOptionalArgCount) {
    aAppId = nsIScriptSecurityManager::NO_APP_ID;
  }

  // Figure out which origin we're dealing with.
  nsCString origin;
  nsresult rv = GetASCIIOriginFromURI(aURI, aAppId, aInMozBrowserOnly, origin);
  NS_ENSURE_SUCCESS(rv, rv);

  OriginOrPatternString oops = OriginOrPatternString::FromOrigin(origin);

  nsRefPtr<AsyncUsageRunnable> runnable =
    new AsyncUsageRunnable(aAppId, aInMozBrowserOnly, oops, aURI, aCallback);

  // Put the computation runnable in the queue.
  rv = WaitForOpenAllowed(oops, nullptr, runnable);
  NS_ENSURE_SUCCESS(rv, rv);

  runnable->AdvanceState();

  runnable.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManager::ClearStoragesForURI(nsIURI* aURI,
                                  uint32_t aAppId,
                                  bool aInMozBrowserOnly,
                                  uint8_t aOptionalArgCount)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  NS_ENSURE_ARG_POINTER(aURI);

  // This only works from the main process.
  NS_ENSURE_TRUE(IsMainProcess(), NS_ERROR_NOT_AVAILABLE);

  if (!aOptionalArgCount) {
    aAppId = nsIScriptSecurityManager::NO_APP_ID;
  }

  // Figure out which origin we're dealing with.
  nsCString origin;
  nsresult rv = GetASCIIOriginFromURI(aURI, aAppId, aInMozBrowserOnly, origin);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString pattern;
  GetOriginPatternString(aAppId, aInMozBrowserOnly, origin, pattern);

  // If there is a pending or running clear operation for this origin, return
  // immediately.
  if (IsClearOriginPending(pattern)) {
    return NS_OK;
  }

  OriginOrPatternString oops = OriginOrPatternString::FromPattern(pattern);

  // Queue up the origin clear runnable.
  nsRefPtr<OriginClearRunnable> runnable = new OriginClearRunnable(oops);

  rv = WaitForOpenAllowed(oops, nullptr, runnable);
  NS_ENSURE_SUCCESS(rv, rv);

  runnable->AdvanceState();

  // Give the runnable some help by invalidating any storages in the way.
  StorageMatcher<nsAutoTArray<nsIOfflineStorage*, 20> > matches;
  matches.Find(mLiveStorages, pattern);

  for (uint32_t index = 0; index < matches.Length(); index++) {
    // We need to grab references to any live storages here to prevent them
    // from dying while we invalidate them.
    nsCOMPtr<nsIOfflineStorage> storage = matches[index];
    storage->Invalidate();
  }

  // After everything has been invalidated the helper should be dispatched to
  // the end of the event queue.
  return NS_OK;
}

NS_IMETHODIMP
QuotaManager::Observe(nsISupports* aSubject,
                      const char* aTopic,
                      const PRUnichar* aData)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!strcmp(aTopic, PROFILE_BEFORE_CHANGE_OBSERVER_ID)) {
    // Setting this flag prevents the service from being recreated and prevents
    // further storagess from being created.
    if (PR_ATOMIC_SET(&gShutdown, 1)) {
      NS_ERROR("Shutdown more than once?!");
    }

    if (IsMainProcess()) {
      FileService* service = FileService::Get();
      if (service) {
        // This should only wait for storages registered in this manager
        // to complete. Other storages may still have running locked files.
        // If the necko service (thread pool) gets the shutdown notification
        // first then the sync loop won't be processed at all, otherwise it will
        // lock the main thread until all storages registered in this manager
        // are finished.

        nsTArray<uint32_t> indexes;
        for (uint32_t index = 0; index < Client::TYPE_MAX; index++) {
          if (mClients[index]->IsFileServiceUtilized()) {
            indexes.AppendElement(index);
          }
        }

        StorageMatcher<nsTArray<nsCOMPtr<nsIFileStorage> > > liveStorages;
        liveStorages.Find(mLiveStorages, &indexes);

        if (!liveStorages.IsEmpty()) {
          nsRefPtr<WaitForLockedFilesToFinishRunnable> runnable =
            new WaitForLockedFilesToFinishRunnable();

          service->WaitForStoragesToComplete(liveStorages, runnable);

          nsIThread* thread = NS_GetCurrentThread();
          while (runnable->IsBusy()) {
            if (!NS_ProcessNextEvent(thread)) {
              NS_ERROR("Failed to process next event!");
              break;
            }
          }
        }
      }

      // Give clients a chance to cleanup IO thread only objects.
      nsCOMPtr<nsIRunnable> runnable =
        NS_NewRunnableMethod(this, &QuotaManager::ReleaseIOThreadObjects);
      if (!runnable) {
        NS_WARNING("Failed to create runnable!");
      }

      if (NS_FAILED(mIOThread->Dispatch(runnable, NS_DISPATCH_NORMAL))) {
        NS_WARNING("Failed to dispatch runnable!");
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

      // Each client will spin the event loop while we wait on all the threads
      // to close. Our timer may fire during that loop.
      for (uint32_t index = 0; index < Client::TYPE_MAX; index++) {
        mClients[index]->ShutdownTransactionService();
      }

      // Cancel the timer regardless of whether it actually fired.
      if (NS_FAILED(mShutdownTimer->Cancel())) {
        NS_WARNING("Failed to cancel shutdown timer!");
      }
    }

    return NS_OK;
  }

  if (!strcmp(aTopic, NS_TIMER_CALLBACK_TOPIC)) {
    NS_ASSERTION(IsMainProcess(), "Should only happen in the main process!");

    NS_WARNING("Some storage operations are taking longer than expected "
               "during shutdown and will be aborted!");

    // Grab all live storages, for all origins.
    StorageMatcher<nsAutoTArray<nsIOfflineStorage*, 50> > liveStorages;
    liveStorages.Find(mLiveStorages);

    // Invalidate them all.
    if (!liveStorages.IsEmpty()) {
      uint32_t count = liveStorages.Length();
      for (uint32_t index = 0; index < count; index++) {
        liveStorages[index]->Invalidate();
      }
    }

    return NS_OK;
  }

  if (!strcmp(aTopic, TOPIC_WEB_APP_CLEAR_DATA)) {
    nsCOMPtr<mozIApplicationClearPrivateDataParams> params =
      do_QueryInterface(aSubject);
    NS_ENSURE_TRUE(params, NS_ERROR_UNEXPECTED);

    uint32_t appId;
    nsresult rv = params->GetAppId(&appId);
    NS_ENSURE_SUCCESS(rv, rv);

    bool browserOnly;
    rv = params->GetBrowserOnly(&browserOnly);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = ClearStoragesForApp(appId, browserOnly);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_NOTREACHED("Unknown topic!");
  return NS_ERROR_UNEXPECTED;
}

void
QuotaManager::SetCurrentWindowInternal(nsPIDOMWindow* aWindow)
{
  NS_ASSERTION(mCurrentWindowIndex != BAD_TLS_INDEX,
               "Should have a valid TLS storage index!");

  if (aWindow) {
    NS_ASSERTION(!PR_GetThreadPrivate(mCurrentWindowIndex),
                 "Somebody forgot to clear the current window!");
    PR_SetThreadPrivate(mCurrentWindowIndex, aWindow);
  }
  else {
    // We cannot assert PR_GetThreadPrivate(mCurrentWindowIndex) here because
    // there are some cases where we did not already have a window.
    PR_SetThreadPrivate(mCurrentWindowIndex, nullptr);
  }
}

void
QuotaManager::CancelPromptsForWindowInternal(nsPIDOMWindow* aWindow)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<CheckQuotaHelper> helper;

  MutexAutoLock autoLock(mQuotaMutex);

  if (mCheckQuotaHelpers.Get(aWindow, getter_AddRefs(helper))) {
    helper->Cancel();
  }
}

bool
QuotaManager::LockedQuotaIsLifted()
{
  mQuotaMutex.AssertCurrentThreadOwns();

  NS_ASSERTION(mCurrentWindowIndex != BAD_TLS_INDEX,
               "Should have a valid TLS storage index!");

  nsPIDOMWindow* window =
    static_cast<nsPIDOMWindow*>(PR_GetThreadPrivate(mCurrentWindowIndex));

  // Quota is not enforced in chrome contexts (e.g. for components and JSMs)
  // so we must have a window here.
  NS_ASSERTION(window, "Why don't we have a Window here?");

  bool createdHelper = false;

  nsRefPtr<CheckQuotaHelper> helper;
  if (!mCheckQuotaHelpers.Get(window, getter_AddRefs(helper))) {
    helper = new CheckQuotaHelper(window, mQuotaMutex);
    createdHelper = true;

    mCheckQuotaHelpers.Put(window, helper);

    // Unlock while calling out to XPCOM
    {
      MutexAutoUnlock autoUnlock(mQuotaMutex);

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
    mCheckQuotaHelpers.Remove(window);
  }

  return result;
}

nsresult
QuotaManager::AcquireExclusiveAccess(const nsACString& aPattern,
                                     nsIOfflineStorage* aStorage,
                                     AcquireListener* aListener,
                                     WaitingOnStoragesCallback aCallback,
                                     void* aClosure)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aListener, "Need a listener!");

  // Find the right SynchronizedOp.
  SynchronizedOp* op =
    FindSynchronizedOp(aPattern, aStorage ? aStorage->Id() : nullptr);

  NS_ASSERTION(op, "We didn't find a SynchronizedOp?");
  NS_ASSERTION(!op->mListener, "SynchronizedOp already has a listener?!?");

  nsTArray<nsCOMPtr<nsIOfflineStorage> > liveStorages;

  if (aStorage) {
    // We need to wait for the storages to go away.
    // Hold on to all storage objects that represent the same storage file
    // (except the one that is requesting this version change).

    Client::Type clientType = aStorage->GetClient()->GetType();

    StorageMatcher<nsAutoTArray<nsIOfflineStorage*, 20> > matches;
    matches.Find(mLiveStorages, aPattern, clientType);

    if (!matches.IsEmpty()) {
      // Grab all storages that are not yet closed but whose storage id match
      // the one we're looking for.
      for (uint32_t index = 0; index < matches.Length(); index++) {
        nsIOfflineStorage*& storage = matches[index];
        if (!storage->IsClosed() &&
            storage != aStorage &&
            storage->Id() == aStorage->Id()) {
          liveStorages.AppendElement(storage);
        }
      }
    }

    if (!liveStorages.IsEmpty()) {
      NS_ASSERTION(op->mStorages[clientType].IsEmpty(),
                   "How do we already have storages here?");
      op->mStorages[clientType].AppendElements(liveStorages);
    }
  }
  else {
    StorageMatcher<ArrayCluster<nsIOfflineStorage*> > matches;
    matches.Find(mLiveStorages, aPattern);

    if (!matches.IsEmpty()) {
      // We want *all* storages, even those that are closed, when we're going to
      // clear the origin.
      matches.AppendElementsTo(liveStorages);

      NS_ASSERTION(op->mStorages.IsEmpty(),
                   "How do we already have storages here?");
      matches.SwapElements(op->mStorages);
    }
  }

  op->mListener = aListener;

  if (!liveStorages.IsEmpty()) {
    // Give our callback the storages so it can decide what to do with them.
    aCallback(liveStorages, aClosure);

    NS_ASSERTION(liveStorages.IsEmpty(),
                 "Should have done something with the array!");

    if (aStorage) {
      // Wait for those storages to close.
      return NS_OK;
    }
  }

  // If we're trying to open a storage and nothing blocks it, or if we're
  // clearing an origin, then go ahead and schedule the op.
  nsresult rv = RunSynchronizedOp(aStorage, op);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
QuotaManager::RunSynchronizedOp(nsIOfflineStorage* aStorage,
                                SynchronizedOp* aOp)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aOp, "Null pointer!");
  NS_ASSERTION(aOp->mListener, "No listener on this op!");
  NS_ASSERTION(!aStorage ||
               aOp->mStorages[aStorage->GetClient()->GetType()].IsEmpty(),
               "This op isn't ready to run!");

  ArrayCluster<nsIOfflineStorage*> storages;

  uint32_t startIndex;
  uint32_t endIndex;

  if (aStorage) {
    Client::Type clientType = aStorage->GetClient()->GetType();

    storages[clientType].AppendElement(aStorage);

    startIndex = clientType;
    endIndex = clientType + 1;
  }
  else {
    aOp->mStorages.SwapElements(storages);

    startIndex = 0;
    endIndex = Client::TYPE_MAX;
  }

  nsRefPtr<WaitForTransactionsToFinishRunnable> runnable =
    new WaitForTransactionsToFinishRunnable(aOp);

  // Ask the file service to call us back when it's done with this storage.
  FileService* service = FileService::Get();

  if (service) {
    // Have to copy here in case a transaction service needs a list too.
    nsTArray<nsCOMPtr<nsIFileStorage> > array;

    for (uint32_t index = startIndex; index < endIndex; index++)  {
      if (!storages[index].IsEmpty() &&
          mClients[index]->IsFileServiceUtilized()) {
        array.AppendElements(storages[index]);
      }
    }

    if (!array.IsEmpty()) {
      runnable->AddRun();

      service->WaitForStoragesToComplete(array, runnable);
    }
  }

  // Ask each transaction service to call us back when they're done with this
  // storage.
  for (uint32_t index = startIndex; index < endIndex; index++)  {
    nsRefPtr<Client>& client = mClients[index];
    if (!storages[index].IsEmpty() && client->IsTransactionServiceActivated()) {
      runnable->AddRun();

      client->WaitForStoragesToComplete(storages[index], runnable);
    }
  }

  nsresult rv = runnable->Run();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

SynchronizedOp*
QuotaManager::FindSynchronizedOp(const nsACString& aPattern,
                                 nsISupports* aId)
{
  for (uint32_t index = 0; index < mSynchronizedOps.Length(); index++) {
    const nsAutoPtr<SynchronizedOp>& currentOp = mSynchronizedOps[index];
    if (PatternMatchesOrigin(aPattern, currentOp->mOriginOrPattern) &&
        (!currentOp->mId || currentOp->mId == aId)) {
      return currentOp;
    }
  }

  return nullptr;
}

nsresult
QuotaManager::ClearStoragesForApp(uint32_t aAppId, bool aBrowserOnly)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aAppId != nsIScriptSecurityManager::UNKNOWN_APP_ID,
               "Bad appId!");

  // This only works from the main process.
  NS_ENSURE_TRUE(IsMainProcess(), NS_ERROR_NOT_AVAILABLE);

  nsAutoCString pattern;
  GetOriginPatternStringMaybeIgnoreBrowser(aAppId, aBrowserOnly, pattern);

  // If there is a pending or running clear operation for this app, return
  // immediately.
  if (IsClearOriginPending(pattern)) {
    return NS_OK;
  }

  OriginOrPatternString oops = OriginOrPatternString::FromPattern(pattern);

  // Queue up the origin clear runnable.
  nsRefPtr<OriginClearRunnable> runnable = new OriginClearRunnable(oops);

  nsresult rv = WaitForOpenAllowed(oops, nullptr, runnable);
  NS_ENSURE_SUCCESS(rv, rv);

  runnable->AdvanceState();

  // Give the runnable some help by invalidating any storages in the way.
  StorageMatcher<nsAutoTArray<nsIOfflineStorage*, 20> > matches;
  matches.Find(mLiveStorages, pattern);

  for (uint32_t index = 0; index < matches.Length(); index++) {
    // We need to grab references here to prevent the storage from dying while
    // we invalidate it.
    nsCOMPtr<nsIOfflineStorage> storage = matches[index];
    storage->Invalidate();
  }

  return NS_OK;
}

nsresult
QuotaManager::MaybeUpgradeOriginDirectory(nsIFile* aDirectory)
{
  NS_ASSERTION(aDirectory, "Null pointer!");

  nsCOMPtr<nsIFile> metadataFile;
  nsresult rv = aDirectory->Clone(getter_AddRefs(metadataFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = metadataFile->Append(NS_LITERAL_STRING(METADATA_FILE_NAME));
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists;
  rv = metadataFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!exists) {
    // Directory structure upgrade needed.
    // Move all files to IDB specific directory.

    nsString idbDirectoryName;
    rv = Client::TypeToText(Client::IDB, idbDirectoryName);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> idbDirectory;
    rv = aDirectory->Clone(getter_AddRefs(idbDirectory));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = idbDirectory->Append(idbDirectoryName);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = idbDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755);
    if (NS_FAILED(rv)) {
      NS_WARNING("IDB directory already exists!");
    }

    nsCOMPtr<nsISimpleEnumerator> entries;
    rv = aDirectory->GetDirectoryEntries(getter_AddRefs(entries));
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

      if (!leafName.Equals(idbDirectoryName)) {
        rv = file->MoveTo(idbDirectory, EmptyString());
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }

    rv = metadataFile->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

void
QuotaManager::GetOriginPatternString(uint32_t aAppId,
                                     MozBrowserPatternFlag aBrowserFlag,
                                     const nsACString& aOrigin,
                                     nsAutoCString& _retval)
{
  NS_ASSERTION(aAppId != nsIScriptSecurityManager::UNKNOWN_APP_ID,
               "Bad appId!");
  NS_ASSERTION(aOrigin.IsEmpty() || aBrowserFlag != IgnoreMozBrowser,
               "Bad args!");

  if (aOrigin.IsEmpty()) {
    _retval.Truncate();

    _retval.AppendInt(aAppId);
    _retval.Append('+');

    if (aBrowserFlag != IgnoreMozBrowser) {
      if (aBrowserFlag == MozBrowser) {
        _retval.Append('t');
      }
      else {
        _retval.Append('f');
      }
      _retval.Append('+');
    }

    return;
  }

#ifdef DEBUG
  if (aAppId != nsIScriptSecurityManager::NO_APP_ID ||
      aBrowserFlag == MozBrowser) {
    nsAutoCString pattern;
    GetOriginPatternString(aAppId, aBrowserFlag, EmptyCString(), pattern);
    NS_ASSERTION(PatternMatchesOrigin(pattern, aOrigin),
                 "Origin doesn't match parameters!");
  }
#endif

  _retval = aOrigin;
}

SynchronizedOp::SynchronizedOp(const OriginOrPatternString& aOriginOrPattern,
                               nsISupports* aId)
: mOriginOrPattern(aOriginOrPattern), mId(aId)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  MOZ_COUNT_CTOR(SynchronizedOp);
}

SynchronizedOp::~SynchronizedOp()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  MOZ_COUNT_DTOR(SynchronizedOp);
}

bool
SynchronizedOp::MustWaitFor(const SynchronizedOp& aExistingOp)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  bool match;

  if (aExistingOp.mOriginOrPattern.IsOrigin()) {
    if (mOriginOrPattern.IsOrigin()) {
      match = aExistingOp.mOriginOrPattern.Equals(mOriginOrPattern);
    }
    else {
      match = PatternMatchesOrigin(mOriginOrPattern, aExistingOp.mOriginOrPattern);
    }
  }
  else if (mOriginOrPattern.IsOrigin()) {
    match = PatternMatchesOrigin(aExistingOp.mOriginOrPattern, mOriginOrPattern);
  }
  else {
    match = PatternMatchesOrigin(mOriginOrPattern, aExistingOp.mOriginOrPattern) ||
            PatternMatchesOrigin(aExistingOp.mOriginOrPattern, mOriginOrPattern);
  }

  // If the origins don't match, the second can proceed.
  if (!match) {
    return false;
  }

  // If the origins and the ids match, the second must wait.
  if (aExistingOp.mId == mId) {
    return true;
  }

  // Waiting is required if either one corresponds to an origin clearing
  // (a null Id).
  if (!aExistingOp.mId || !mId) {
    return true;
  }

  // Otherwise, things for the same origin but different storages can proceed
  // independently.
  return false;
}

void
SynchronizedOp::DelayRunnable(nsIRunnable* aRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mDelayedRunnables.IsEmpty() || !mId,
               "Only ClearOrigin operations can delay multiple runnables!");

  mDelayedRunnables.AppendElement(aRunnable);
}

void
SynchronizedOp::DispatchDelayedRunnables()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!mListener, "Any listener should be gone by now!");

  uint32_t count = mDelayedRunnables.Length();
  for (uint32_t index = 0; index < count; index++) {
    NS_DispatchToCurrentThread(mDelayedRunnables[index]);
  }

  mDelayedRunnables.Clear();
}

nsresult
OriginClearRunnable::OnExclusiveAccessAcquired()
{
  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "This should never fail!");

  nsresult rv = quotaManager->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// static
void
OriginClearRunnable::InvalidateOpenedStorages(
                              nsTArray<nsCOMPtr<nsIOfflineStorage> >& aStorages,
                              void* aClosure)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsTArray<nsCOMPtr<nsIOfflineStorage> > storages;
  storages.SwapElements(aStorages);

  for (uint32_t index = 0; index < storages.Length(); index++) {
    storages[index]->Invalidate();
  }
}

void
OriginClearRunnable::DeleteFiles(QuotaManager* aQuotaManager)
{
  AssertIsOnIOThread();
  NS_ASSERTION(aQuotaManager, "Don't pass me null!");

  nsresult rv;

  nsCOMPtr<nsIFile> directory =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS_VOID(rv);

  rv = directory->InitWithPath(aQuotaManager->GetBaseDirectory());
  NS_ENSURE_SUCCESS_VOID(rv);

  nsCOMPtr<nsISimpleEnumerator> entries;
  if (NS_FAILED(directory->GetDirectoryEntries(getter_AddRefs(entries))) ||
      !entries) {
    return;
  }

  nsCString originSanitized(mOriginOrPattern);
  SanitizeOriginString(originSanitized);

  bool hasMore;
  while (NS_SUCCEEDED((rv = entries->HasMoreElements(&hasMore))) && hasMore) {
    nsCOMPtr<nsISupports> entry;
    rv = entries->GetNext(getter_AddRefs(entry));
    NS_ENSURE_SUCCESS_VOID(rv);

    nsCOMPtr<nsIFile> file = do_QueryInterface(entry);
    NS_ASSERTION(file, "Don't know what this is!");

    bool isDirectory;
    rv = file->IsDirectory(&isDirectory);
    NS_ENSURE_SUCCESS_VOID(rv);

    if (!isDirectory) {
      NS_WARNING("Something in the IndexedDB directory that doesn't belong!");
      continue;
    }

    nsString leafName;
    rv = file->GetLeafName(leafName);
    NS_ENSURE_SUCCESS_VOID(rv);

    // Skip storages for other apps.
    if (!PatternMatchesOrigin(originSanitized,
                              NS_ConvertUTF16toUTF8(leafName))) {
      continue;
    }

    if (NS_FAILED(file->Remove(true))) {
      // This should never fail if we've closed all storage connections
      // correctly...
      NS_ERROR("Failed to remove directory!");
    }
  }

  aQuotaManager->RemoveQuotaForPattern(mOriginOrPattern);

  aQuotaManager->OriginClearCompleted(mOriginOrPattern);
}

NS_IMPL_THREADSAFE_ISUPPORTS1(OriginClearRunnable, nsIRunnable)

NS_IMETHODIMP
OriginClearRunnable::Run()
{
  PROFILER_LABEL("Quota", "OriginClearRunnable::Run");

  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "This should never fail!");

  switch (mCallbackState) {
    case Pending: {
      NS_NOTREACHED("Should never get here without being dispatched!");
      return NS_ERROR_UNEXPECTED;
    }

    case OpenAllowed: {
      NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

      AdvanceState();

      // Now we have to wait until the thread pool is done with all of the
      // storages we care about.
      nsresult rv =
        quotaManager->AcquireExclusiveAccess(mOriginOrPattern, this,
                                             InvalidateOpenedStorages, nullptr);
      NS_ENSURE_SUCCESS(rv, rv);

      return NS_OK;
    }

    case IO: {
      AssertIsOnIOThread();

      AdvanceState();

      DeleteFiles(quotaManager);

      // Now dispatch back to the main thread.
      if (NS_FAILED(NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL))) {
        NS_WARNING("Failed to dispatch to main thread!");
        return NS_ERROR_FAILURE;
      }

      return NS_OK;
    }

    case Complete: {
      NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

      // Tell the QuotaManager that we're done.
      quotaManager->AllowNextSynchronizedOp(mOriginOrPattern, nullptr);

      return NS_OK;
    }

    default:
      NS_ERROR("Unknown state value!");
      return NS_ERROR_UNEXPECTED;
  }

  NS_NOTREACHED("Should never get here!");
  return NS_ERROR_UNEXPECTED;
}

AsyncUsageRunnable::AsyncUsageRunnable(uint32_t aAppId,
                                       bool aInMozBrowserOnly,
                                       const OriginOrPatternString& aOrigin,
                                       nsIURI* aURI,
                                       nsIUsageCallback* aCallback)
: mURI(aURI),
  mCallback(aCallback),
  mAppId(aAppId),
  mOrigin(aOrigin),
  mCallbackState(Pending),
  mInMozBrowserOnly(aInMozBrowserOnly)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aURI, "Null pointer!");
  NS_ASSERTION(aOrigin.IsOrigin(), "Expect origin only here!");
  NS_ASSERTION(!aOrigin.IsEmpty(), "Empty origin!");
  NS_ASSERTION(aCallback, "Null pointer!");
}

nsresult
AsyncUsageRunnable::TakeShortcut()
{
  NS_ASSERTION(mCallbackState == Pending, "Huh?");

  nsresult rv = NS_DispatchToCurrentThread(this);
  NS_ENSURE_SUCCESS(rv, rv);

  mCallbackState = Shortcut;
  return NS_OK;
}

nsresult
AsyncUsageRunnable::RunInternal()
{
  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "This should never fail!");

  nsresult rv;

  switch (mCallbackState) {
    case Pending: {
      NS_NOTREACHED("Should never get here without being dispatched!");
      return NS_ERROR_UNEXPECTED;
    }

    case OpenAllowed: {
      NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

      AdvanceState();

      rv = quotaManager->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL);
      if (NS_FAILED(rv)) {
        NS_WARNING("Failed to dispatch to the IO thread!");
      }

      return NS_OK;
    }

    case IO: {
      AssertIsOnIOThread();

      AdvanceState();

      // Get the directory that contains all the storage files we care about.
      nsCOMPtr<nsIFile> directory;
      rv = quotaManager->GetDirectoryForOrigin(mOrigin,
                                               getter_AddRefs(directory));
      NS_ENSURE_SUCCESS(rv, rv);

      bool exists;
      rv = directory->Exists(&exists);
      NS_ENSURE_SUCCESS(rv, rv);

      // If the directory exists then enumerate all the files inside, adding up
      // the sizes to get the final usage statistic.
      if (exists && !mCanceled) {
        bool initialized = quotaManager->mInitializedOrigins.Contains(mOrigin);

        if (!initialized) {
          rv = quotaManager->MaybeUpgradeOriginDirectory(directory);
          NS_ENSURE_SUCCESS(rv, rv);
        }

        nsCOMPtr<nsISimpleEnumerator> entries;
        rv = directory->GetDirectoryEntries(getter_AddRefs(entries));
        NS_ENSURE_SUCCESS(rv, rv);

        bool hasMore;
        while (NS_SUCCEEDED((rv = entries->HasMoreElements(&hasMore))) &&
               hasMore && !mCanceled) {
          nsCOMPtr<nsISupports> entry;
          rv = entries->GetNext(getter_AddRefs(entry));
          NS_ENSURE_SUCCESS(rv, rv);

          nsCOMPtr<nsIFile> file = do_QueryInterface(entry);
          NS_ENSURE_TRUE(file, NS_NOINTERFACE);

          nsString leafName;
          rv = file->GetLeafName(leafName);
          NS_ENSURE_SUCCESS(rv, rv);

          if (leafName.EqualsLiteral(METADATA_FILE_NAME)) {
            continue;
          }

          if (!initialized) {
            bool isDirectory;
            rv = file->IsDirectory(&isDirectory);
            NS_ENSURE_SUCCESS(rv, rv);

            if (!isDirectory) {
              NS_WARNING("Unknown file found!");
              return NS_ERROR_UNEXPECTED;
            }
          }

          Client::Type clientType;
          rv = Client::TypeFromText(leafName, clientType);
          if (NS_FAILED(rv)) {
            NS_WARNING("Unknown directory found!");
            if (!initialized) {
              return NS_ERROR_UNEXPECTED;
            }
            continue;
          }

          nsRefPtr<Client>& client = quotaManager->mClients[clientType];

          if (!initialized) {
            rv = client->InitOrigin(mOrigin, this);
          }
          else {
            rv = client->GetUsageForOrigin(mOrigin, this);
          }
          NS_ENSURE_SUCCESS(rv, rv);
        }
      }

      // Run dispatches us back to the main thread.
      return NS_OK;
    }

    case Complete: // Fall through
    case Shortcut: {
      NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

      // Call the callback unless we were canceled.
      if (!mCanceled) {
        mCallback->OnUsageResult(mURI, TotalUsage(), FileUsage(), mAppId,
                                 mInMozBrowserOnly);
      }

      // Clean up.
      mURI = nullptr;
      mCallback = nullptr;

      // And tell the QuotaManager that we're done.
      if (mCallbackState == Complete) {
        quotaManager->AllowNextSynchronizedOp(mOrigin, nullptr);
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

NS_IMPL_THREADSAFE_ISUPPORTS2(AsyncUsageRunnable,
                              nsIRunnable,
                              nsIQuotaRequest)

NS_IMETHODIMP
AsyncUsageRunnable::Run()
{
  PROFILER_LABEL("Quota", "AsyncUsageRunnable::Run");

  nsresult rv = RunInternal();

  if (!NS_IsMainThread()) {
    if (NS_FAILED(rv)) {
      ResetUsage();
    }

    if (NS_FAILED(NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL))) {
      NS_WARNING("Failed to dispatch to main thread!");
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
AsyncUsageRunnable::Cancel()
{
  if (PR_ATOMIC_SET(&mCanceled, 1)) {
    NS_WARNING("Canceled more than once?!");
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(WaitForTransactionsToFinishRunnable, nsIRunnable)

NS_IMETHODIMP
WaitForTransactionsToFinishRunnable::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mOp, "Null op!");
  NS_ASSERTION(mOp->mListener, "Nothing to run!");
  NS_ASSERTION(mCountdown, "Wrong countdown!");

  if (--mCountdown) {
    return NS_OK;
  }

  // Don't hold the listener alive longer than necessary.
  nsRefPtr<AcquireListener> listener;
  listener.swap(mOp->mListener);

  mOp = nullptr;

  nsresult rv = listener->OnExclusiveAccessAcquired();
  NS_ENSURE_SUCCESS(rv, rv);

  // The listener is responsible for calling
  // QuotaManager::AllowNextSynchronizedOp.
  return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(WaitForLockedFilesToFinishRunnable, nsIRunnable)

NS_IMETHODIMP
WaitForLockedFilesToFinishRunnable::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  mBusy = false;

  return NS_OK;
}
