/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QuotaManager.h"

#include "mozIApplicationClearPrivateDataParams.h"
#include "nsIBinaryInputStream.h"
#include "nsIBinaryOutputStream.h"
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
#include "mozilla/Atomics.h"
#include "mozilla/CondVar.h"
#include "mozilla/dom/asmjscache/AsmJSCache.h"
#include "mozilla/dom/file/FileService.h"
#include "mozilla/dom/indexedDB/Client.h"
#include "mozilla/Mutex.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsCRTGlue.h"
#include "nsDirectoryServiceUtils.h"
#include "nsNetUtil.h"
#include "nsScriptSecurityManager.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "xpcpublic.h"

#include "AcquireListener.h"
#include "CheckQuotaHelper.h"
#include "OriginCollection.h"
#include "OriginOrPatternString.h"
#include "QuotaObject.h"
#include "StorageMatcher.h"
#include "UsageInfo.h"
#include "Utilities.h"

// The amount of time, in milliseconds, that our IO thread will stay alive
// after the last event it processes.
#define DEFAULT_THREAD_TIMEOUT_MS 30000

// The amount of time, in milliseconds, that we will wait for active storage
// transactions on shutdown before aborting them.
#define DEFAULT_SHUTDOWN_TIMER_MS 30000

// Preference that users can set to override DEFAULT_QUOTA_MB
#define PREF_STORAGE_QUOTA "dom.indexedDB.warningQuota"

// Preference that users can set to override temporary storage smart limit
// calculation.
#define PREF_FIXED_LIMIT "dom.quotaManager.temporaryStorage.fixedLimit"

// Preferences that are used during temporary storage smart limit calculation
#define PREF_SMART_LIMIT_PREFIX "dom.quotaManager.temporaryStorage.smartLimit."
#define PREF_SMART_LIMIT_MIN PREF_SMART_LIMIT_PREFIX "min"
#define PREF_SMART_LIMIT_MAX PREF_SMART_LIMIT_PREFIX "max"
#define PREF_SMART_LIMIT_CHUNK PREF_SMART_LIMIT_PREFIX "chunk"
#define PREF_SMART_LIMIT_RATIO PREF_SMART_LIMIT_PREFIX "ratio"

// Preference that is used to enable testing features
#define PREF_TESTING_FEATURES "dom.quotaManager.testing"

// profile-before-change, when we need to shut down quota manager
#define PROFILE_BEFORE_CHANGE_OBSERVER_ID "profile-before-change"

// The name of the file that we use to load/save the last access time of an
// origin.
#define METADATA_FILE_NAME ".metadata"

#define PERMISSION_DEFAUT_PERSISTENT_STORAGE "default-persistent-storage"

USING_QUOTA_NAMESPACE
using namespace mozilla::dom;
using mozilla::dom::file::FileService;

static_assert(
  static_cast<uint32_t>(StorageType::Persistent) ==
  static_cast<uint32_t>(PERSISTENCE_TYPE_PERSISTENT),
  "Enum values should match.");

static_assert(
  static_cast<uint32_t>(StorageType::Temporary) ==
  static_cast<uint32_t>(PERSISTENCE_TYPE_TEMPORARY),
  "Enum values should match.");

BEGIN_QUOTA_NAMESPACE

// A struct that contains the information corresponding to a pending or
// running operation that requires synchronization (e.g. opening a db,
// clearing dbs for an origin, etc).
struct SynchronizedOp
{
  SynchronizedOp(const OriginOrPatternString& aOriginOrPattern,
                 Nullable<PersistenceType> aPersistenceType,
                 const nsACString& aId);

  ~SynchronizedOp();

  // Test whether this SynchronizedOp needs to wait for the given op.
  bool
  MustWaitFor(const SynchronizedOp& aOp);

  void
  DelayRunnable(nsIRunnable* aRunnable);

  void
  DispatchDelayedRunnables();

  const OriginOrPatternString mOriginOrPattern;
  Nullable<PersistenceType> mPersistenceType;
  nsCString mId;
  nsRefPtr<AcquireListener> mListener;
  nsTArray<nsCOMPtr<nsIRunnable> > mDelayedRunnables;
  ArrayCluster<nsIOfflineStorage*> mStorages;
};

class CollectOriginsHelper MOZ_FINAL : public nsRunnable
{
public:
  CollectOriginsHelper(mozilla::Mutex& aMutex, uint64_t aMinSizeToBeFreed);

  NS_IMETHOD
  Run();

  // Blocks the current thread until origins are collected on the main thread.
  // The returned value contains an aggregate size of those origins.
  int64_t
  BlockAndReturnOriginsForEviction(nsTArray<OriginInfo*>& aOriginInfos);

private:
  ~CollectOriginsHelper()
  { }

  uint64_t mMinSizeToBeFreed;

  mozilla::Mutex& mMutex;
  mozilla::CondVar mCondVar;

  // The members below are protected by mMutex.
  nsTArray<OriginInfo*> mOriginInfos;
  uint64_t mSizeToBeFreed;
  bool mWaiting;
};

// Responsible for clearing the storage files for a particular origin on the
// IO thread. Created when nsIQuotaManager::ClearStoragesForURI is called.
// Runs three times, first on the main thread, next on the IO thread, and then
// finally again on the main thread. While on the IO thread the runnable will
// actually remove the origin's storage files and the directory that contains
// them before dispatching itself back to the main thread. When back on the main
// thread the runnable will notify the QuotaManager that the job has been
// completed.
class OriginClearRunnable MOZ_FINAL : public nsRunnable,
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
  NS_DECL_ISUPPORTS_INHERITED

  OriginClearRunnable(const OriginOrPatternString& aOriginOrPattern)
  : mOriginOrPattern(aOriginOrPattern),
    mCallbackState(Pending)
  { }

  NS_IMETHOD
  Run();

  // AcquireListener override
  virtual nsresult
  OnExclusiveAccessAcquired() MOZ_OVERRIDE;

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
  DeleteFiles(QuotaManager* aQuotaManager,
              PersistenceType aPersistenceType);

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
class AsyncUsageRunnable MOZ_FINAL : public UsageInfo,
                                     public nsRunnable,
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
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIQUOTAREQUEST

  AsyncUsageRunnable(uint32_t aAppId,
                     bool aInMozBrowserOnly,
                     const nsACString& aGroup,
                     const OriginOrPatternString& aOrigin,
                     nsIURI* aURI,
                     nsIUsageCallback* aCallback);

  NS_IMETHOD
  Run();

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

private:
  // Run calls the RunInternal method and makes sure that we always dispatch
  // to the main thread in case of an error.
  inline nsresult
  RunInternal();

  nsresult
  AddToUsage(QuotaManager* aQuotaManager,
             PersistenceType aPersistenceType);

  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIUsageCallback> mCallback;
  uint32_t mAppId;
  nsCString mGroup;
  OriginOrPatternString mOrigin;
  CallbackState mCallbackState;
  bool mInMozBrowserOnly;
};

class ResetOrClearRunnable MOZ_FINAL : public nsRunnable,
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
  NS_DECL_ISUPPORTS_INHERITED

  ResetOrClearRunnable(bool aClear)
  : mCallbackState(Pending),
    mClear(aClear)
  { }

  NS_IMETHOD
  Run();

  // AcquireListener override
  virtual nsresult
  OnExclusiveAccessAcquired() MOZ_OVERRIDE;

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
  DeleteFiles(QuotaManager* aQuotaManager,
              PersistenceType aPersistenceType);

private:
  CallbackState mCallbackState;
  bool mClear;
};

// Responsible for finalizing eviction of certian origins (storage files have
// been already cleared, we just need to release IO thread only objects and
// allow next synchronized ops for evicted origins). Created when
// QuotaManager::FinalizeOriginEviction is called. Runs three times, first
// on the main thread, next on the IO thread, and then finally again on the main
// thread. While on the IO thread the runnable will release IO thread only
// objects before dispatching itself back to the main thread. When back on the
// main thread the runnable will call QuotaManager::AllowNextSynchronizedOp.
// The runnable can also run in a shortened mode (runs only twice).
class FinalizeOriginEvictionRunnable MOZ_FINAL : public nsRunnable
{
  enum CallbackState {
    // Not yet run.
    Pending = 0,

    // Running on the main thread in the callback for OpenAllowed.
    OpenAllowed,

    // Running on the IO thread.
    IO,

    // Running on the main thread after IO work is done.
    Complete
  };

public:
  FinalizeOriginEvictionRunnable(nsTArray<nsCString>& aOrigins)
  : mCallbackState(Pending)
  {
    mOrigins.SwapElements(aOrigins);
  }

  NS_IMETHOD
  Run();

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
        MOZ_ASSUME_UNREACHABLE("Can't advance past Complete!");
    }
  }

  nsresult
  Dispatch();

  nsresult
  RunImmediately();

private:
  CallbackState mCallbackState;
  nsTArray<nsCString> mOrigins;
};

bool
IsOnIOThread()
{
  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "Must have a manager here!");

  bool currentThread;
  return NS_SUCCEEDED(quotaManager->IOThread()->
                      IsOnCurrentThread(&currentThread)) && currentThread;
}

void
AssertIsOnIOThread()
{
  NS_ASSERTION(IsOnIOThread(), "Running on the wrong thread!");
}

void
AssertCurrentThreadOwnsQuotaMutex()
{
#ifdef DEBUG
  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "Must have a manager here!");

  quotaManager->AssertCurrentThreadOwnsQuotaMutex();
#endif
}

END_QUOTA_NAMESPACE

namespace {

// Amount of space that storages may use by default in megabytes.
static const int32_t  kDefaultQuotaMB =             50;

// Constants for temporary storage limit computing.
static const int32_t  kDefaultFixedLimitKB =        -1;
#ifdef ANDROID
// On Android, smaller/older devices may have very little storage and
// device owners may be sensitive to storage footprint: Use a smaller
// percentage of available space and a smaller minimum/maximum.
static const uint32_t kDefaultSmartLimitMinKB =     10 * 1024;
static const uint32_t kDefaultSmartLimitMaxKB =    200 * 1024;
static const uint32_t kDefaultSmartLimitChunkKB =    2 * 1024;
static const float    kDefaultSmartLimitRatio =     .2f;
#else
static const uint64_t kDefaultSmartLimitMinKB =     50 * 1024;
static const uint64_t kDefaultSmartLimitMaxKB =   1024 * 1024;
static const uint32_t kDefaultSmartLimitChunkKB =   10 * 1024;
static const float    kDefaultSmartLimitRatio =     .4f;
#endif

QuotaManager* gInstance = nullptr;
mozilla::Atomic<uint32_t> gShutdown(0);

int32_t gStorageQuotaMB = kDefaultQuotaMB;

int32_t gFixedLimitKB = kDefaultFixedLimitKB;
uint32_t gSmartLimitMinKB = kDefaultSmartLimitMinKB;
uint32_t gSmartLimitMaxKB = kDefaultSmartLimitMaxKB;
uint32_t gSmartLimitChunkKB = kDefaultSmartLimitChunkKB;
float gSmartLimitRatio = kDefaultSmartLimitRatio;

bool gTestingEnabled = false;

// A callback runnable used by the TransactionPool when it's safe to proceed
// with a SetVersion/DeleteDatabase/etc.
class WaitForTransactionsToFinishRunnable MOZ_FINAL : public nsRunnable
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

  NS_IMETHOD
  Run();

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

class WaitForLockedFilesToFinishRunnable MOZ_FINAL : public nsRunnable
{
public:
  WaitForLockedFilesToFinishRunnable()
  : mBusy(true)
  { }

  NS_IMETHOD
  Run();

  bool
  IsBusy() const
  {
    return mBusy;
  }

private:
  bool mBusy;
};

class SaveOriginAccessTimeRunnable MOZ_FINAL : public nsRunnable
{
public:
  SaveOriginAccessTimeRunnable(const nsACString& aOrigin, int64_t aTimestamp)
  : mOrigin(aOrigin), mTimestamp(aTimestamp)
  { }

  NS_IMETHOD
  Run();

private:
  nsCString mOrigin;
  int64_t mTimestamp;
};

struct MOZ_STACK_CLASS RemoveQuotaInfo
{
  RemoveQuotaInfo(PersistenceType aPersistenceType, const nsACString& aPattern)
  : persistenceType(aPersistenceType), pattern(aPattern)
  { }

  PersistenceType persistenceType;
  nsCString pattern;
};

struct MOZ_STACK_CLASS InactiveOriginsInfo
{
  InactiveOriginsInfo(OriginCollection& aCollection,
                      nsTArray<OriginInfo*>& aOrigins)
  : collection(aCollection), origins(aOrigins)
  { }

  OriginCollection& collection;
  nsTArray<OriginInfo*>& origins;
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

nsresult
EnsureDirectory(nsIFile* aDirectory, bool* aCreated)
{
  AssertIsOnIOThread();

  nsresult rv = aDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755);
  if (rv == NS_ERROR_FILE_ALREADY_EXISTS) {
    bool isDirectory;
    rv = aDirectory->IsDirectory(&isDirectory);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(isDirectory, NS_ERROR_UNEXPECTED);

    *aCreated = false;
  }
  else {
    NS_ENSURE_SUCCESS(rv, rv);

    *aCreated = true;
  }

  return NS_OK;
}

nsresult
CreateDirectoryUpgradeStamp(nsIFile* aDirectory)
{
  AssertIsOnIOThread();

  nsCOMPtr<nsIFile> metadataFile;
  nsresult rv = aDirectory->Clone(getter_AddRefs(metadataFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = metadataFile->Append(NS_LITERAL_STRING(METADATA_FILE_NAME));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = metadataFile->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
GetDirectoryMetadataStream(nsIFile* aDirectory, bool aUpdate,
                           nsIBinaryOutputStream** aStream)
{
  AssertIsOnIOThread();

  nsCOMPtr<nsIFile> metadataFile;
  nsresult rv = aDirectory->Clone(getter_AddRefs(metadataFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = metadataFile->Append(NS_LITERAL_STRING(METADATA_FILE_NAME));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIOutputStream> outputStream;
  if (aUpdate) {
    bool exists;
    rv = metadataFile->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!exists) {
      *aStream = nullptr;
      return NS_OK;
    }

    nsCOMPtr<nsIFileStream> stream;
    rv = NS_NewLocalFileStream(getter_AddRefs(stream), metadataFile);
    NS_ENSURE_SUCCESS(rv, rv);

    outputStream = do_QueryInterface(stream);
    NS_ENSURE_TRUE(outputStream, NS_ERROR_FAILURE);
  }
  else {
    rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream),
                                     metadataFile);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIBinaryOutputStream> binaryStream =
    do_CreateInstance("@mozilla.org/binaryoutputstream;1");
  NS_ENSURE_TRUE(binaryStream, NS_ERROR_FAILURE);

  rv = binaryStream->SetOutputStream(outputStream);
  NS_ENSURE_SUCCESS(rv, rv);

  binaryStream.forget(aStream);
  return NS_OK;
}

nsresult
CreateDirectoryMetadata(nsIFile* aDirectory, int64_t aTimestamp,
                        const nsACString& aGroup, const nsACString& aOrigin)
{
  AssertIsOnIOThread();

  nsCOMPtr<nsIBinaryOutputStream> stream;
  nsresult rv =
    GetDirectoryMetadataStream(aDirectory, false, getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(stream, "This shouldn't be null!");

  rv = stream->Write64(aTimestamp);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stream->WriteStringZ(PromiseFlatCString(aGroup).get());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stream->WriteStringZ(PromiseFlatCString(aOrigin).get());
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
GetDirectoryMetadata(nsIFile* aDirectory, int64_t* aTimestamp,
                     nsACString& aGroup, nsACString& aOrigin)
{
  AssertIsOnIOThread();

  nsCOMPtr<nsIFile> metadataFile;
  nsresult rv = aDirectory->Clone(getter_AddRefs(metadataFile));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = metadataFile->Append(NS_LITERAL_STRING(METADATA_FILE_NAME));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> stream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(stream), metadataFile);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> bufferedStream;
  rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedStream), stream, 512);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIBinaryInputStream> binaryStream =
    do_CreateInstance("@mozilla.org/binaryinputstream;1");
  NS_ENSURE_TRUE(binaryStream, NS_ERROR_FAILURE);

  rv = binaryStream->SetInputStream(bufferedStream);
  NS_ENSURE_SUCCESS(rv, rv);

  uint64_t timestamp;
  rv = binaryStream->Read64(&timestamp);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString group;
  rv = binaryStream->ReadCString(group);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString origin;
  rv = binaryStream->ReadCString(origin);
  NS_ENSURE_SUCCESS(rv, rv);

  *aTimestamp = timestamp;
  aGroup = group;
  aOrigin = origin;
  return NS_OK;
}

nsresult
MaybeUpgradeOriginDirectory(nsIFile* aDirectory)
{
  AssertIsOnIOThread();
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
    if (rv == NS_ERROR_FILE_ALREADY_EXISTS) {
      NS_WARNING("IDB directory already exists!");

      bool isDirectory;
      rv = idbDirectory->IsDirectory(&isDirectory);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_TRUE(isDirectory, NS_ERROR_UNEXPECTED);
    }
    else {
      NS_ENSURE_SUCCESS(rv, rv);
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

// This method computes and returns our best guess for the temporary storage
// limit (in bytes), based on the amount of space users have free on their hard
// drive and on given temporary storage usage (also in bytes). We use a tiered
// scheme: the more space available, the larger the temporary storage will be.
// However, we do not want to enable the temporary storage to grow to an
// unbounded size, so the larger the user's available space is, the smaller of
// a percentage we take. We set a lower bound of gTemporaryStorageLimitMinKB
// and an upper bound of gTemporaryStorageLimitMaxKB.
nsresult
GetTemporaryStorageLimit(nsIFile* aDirectory, uint64_t aCurrentUsage,
                         uint64_t* aLimit)
{
  // Check for free space on device where temporary storage directory lives.
  int64_t bytesAvailable;
  nsresult rv = aDirectory->GetDiskSpaceAvailable(&bytesAvailable);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(bytesAvailable >= 0, "Negative bytes available?!");

  uint64_t availableKB =
    static_cast<uint64_t>((bytesAvailable + aCurrentUsage) / 1024);

  // Grow/shrink in gTemporaryStorageLimitChunkKB units, deliberately, so that
  // in the common case we don't shrink temporary storage and evict origin data
  // every time we initialize.
  availableKB = (availableKB / gSmartLimitChunkKB) * gSmartLimitChunkKB;

  // The tier scheme:
  // .5 % of space above 25 GB
  // 1 % of space between 7GB -> 25 GB
  // 5 % of space between 500 MB -> 7 GB
  // gTemporaryStorageLimitRatio of space up to 500 MB

  static const uint64_t _25GB = 25 * 1024 * 1024;
  static const uint64_t _7GB = 7 * 1024 * 1024;
  static const uint64_t _500MB = 500 * 1024;

#define PERCENTAGE(a, b, c) ((a - b) * c)

  uint64_t resultKB;
  if (availableKB > _25GB) {
    resultKB = static_cast<uint64_t>(
      PERCENTAGE(availableKB, _25GB, .005) +
      PERCENTAGE(_25GB, _7GB, .01) +
      PERCENTAGE(_7GB, _500MB, .05) +
      PERCENTAGE(_500MB, 0, gSmartLimitRatio)
    );
  }
  else if (availableKB > _7GB) {
    resultKB = static_cast<uint64_t>(
      PERCENTAGE(availableKB, _7GB, .01) +
      PERCENTAGE(_7GB, _500MB, .05) +
      PERCENTAGE(_500MB, 0, gSmartLimitRatio)
    );
  }
  else if (availableKB > _500MB) {
    resultKB = static_cast<uint64_t>(
      PERCENTAGE(availableKB, _500MB, .05) +
      PERCENTAGE(_500MB, 0, gSmartLimitRatio)
    );
  }
  else {
    resultKB = static_cast<uint64_t>(
      PERCENTAGE(availableKB, 0, gSmartLimitRatio)
    );
  }

#undef PERCENTAGE

  *aLimit = 1024 * mozilla::clamped<uint64_t>(resultKB, gSmartLimitMinKB,
                                              gSmartLimitMaxKB);
  return NS_OK;
}

} // anonymous namespace

QuotaManager::QuotaManager()
: mCurrentWindowIndex(BAD_TLS_INDEX),
  mQuotaMutex("QuotaManager.mQuotaMutex"),
  mTemporaryStorageLimit(0),
  mTemporaryStorageUsage(0),
  mTemporaryStorageInitialized(false),
  mStorageAreaInitialized(false)
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
    nsCOMPtr<nsIFile> baseDir;
    rv = NS_GetSpecialDirectory(NS_APP_INDEXEDDB_PARENT_DIR,
                                getter_AddRefs(baseDir));
    if (NS_FAILED(rv)) {
      rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                  getter_AddRefs(baseDir));
    }
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> indexedDBDir;
    rv = baseDir->Clone(getter_AddRefs(indexedDBDir));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = indexedDBDir->Append(NS_LITERAL_STRING("indexedDB"));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = indexedDBDir->GetPath(mIndexedDBPath);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = baseDir->Append(NS_LITERAL_STRING("storage"));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> persistentStorageDir;
    rv = baseDir->Clone(getter_AddRefs(persistentStorageDir));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = persistentStorageDir->Append(NS_LITERAL_STRING("persistent"));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = persistentStorageDir->GetPath(mPersistentStoragePath);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> temporaryStorageDir;
    rv = baseDir->Clone(getter_AddRefs(temporaryStorageDir));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = temporaryStorageDir->Append(NS_LITERAL_STRING("temporary"));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = temporaryStorageDir->GetPath(mTemporaryStoragePath);
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
                                            kDefaultQuotaMB))) {
    NS_WARNING("Unable to respond to quota pref changes!");
  }

  if (NS_FAILED(Preferences::AddIntVarCache(&gFixedLimitKB, PREF_FIXED_LIMIT,
                                             kDefaultFixedLimitKB)) ||
      NS_FAILED(Preferences::AddUintVarCache(&gSmartLimitMinKB,
                                             PREF_SMART_LIMIT_MIN,
                                             kDefaultSmartLimitMinKB)) ||
      NS_FAILED(Preferences::AddUintVarCache(&gSmartLimitMaxKB,
                                             PREF_SMART_LIMIT_MAX,
                                             kDefaultSmartLimitMaxKB)) ||
      NS_FAILED(Preferences::AddUintVarCache(&gSmartLimitChunkKB,
                                             PREF_SMART_LIMIT_CHUNK,
                                             kDefaultSmartLimitChunkKB)) ||
      NS_FAILED(Preferences::AddFloatVarCache(&gSmartLimitRatio,
                                              PREF_SMART_LIMIT_RATIO,
                                              kDefaultSmartLimitRatio))) {
    NS_WARNING("Unable to respond to temp storage pref changes!");
  }

  if (NS_FAILED(Preferences::AddBoolVarCache(&gTestingEnabled,
                                             PREF_TESTING_FEATURES, false))) {
    NS_WARNING("Unable to respond to testing pref changes!");
  }

  static_assert(Client::IDB == 0 && Client::ASMJS == 1 && Client::TYPE_MAX == 2,
                "Fix the registration!");

  NS_ASSERTION(mClients.Capacity() == Client::TYPE_MAX,
               "Should be using an auto array with correct capacity!");

  // Register IndexedDB
  mClients.AppendElement(new indexedDB::Client());
  mClients.AppendElement(asmjscache::CreateClient());

  return NS_OK;
}

void
QuotaManager::InitQuotaForOrigin(PersistenceType aPersistenceType,
                                 const nsACString& aGroup,
                                 const nsACString& aOrigin,
                                 uint64_t aLimitBytes,
                                 uint64_t aUsageBytes,
                                 int64_t aAccessTime)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aLimitBytes > 0 ||
             aPersistenceType == PERSISTENCE_TYPE_TEMPORARY);
  MOZ_ASSERT(aUsageBytes <= aLimitBytes ||
             aPersistenceType == PERSISTENCE_TYPE_TEMPORARY);

  MutexAutoLock lock(mQuotaMutex);

  GroupInfoPair* pair;
  if (!mGroupInfoPairs.Get(aGroup, &pair)) {
    pair = new GroupInfoPair();
    mGroupInfoPairs.Put(aGroup, pair);
    // The hashtable is now responsible to delete the GroupInfoPair.
  }

  nsRefPtr<GroupInfo> groupInfo = pair->LockedGetGroupInfo(aPersistenceType);
  if (!groupInfo) {
    groupInfo = new GroupInfo(aPersistenceType, aGroup);
    pair->LockedSetGroupInfo(groupInfo);
  }

  nsRefPtr<OriginInfo> originInfo =
    new OriginInfo(groupInfo, aOrigin, aLimitBytes, aUsageBytes, aAccessTime);
  groupInfo->LockedAddOriginInfo(originInfo);
}

void
QuotaManager::DecreaseUsageForOrigin(PersistenceType aPersistenceType,
                                     const nsACString& aGroup,
                                     const nsACString& aOrigin,
                                     int64_t aSize)
{
  AssertIsOnIOThread();

  MutexAutoLock lock(mQuotaMutex);

  GroupInfoPair* pair;
  if (!mGroupInfoPairs.Get(aGroup, &pair)) {
    return;
  }

  nsRefPtr<GroupInfo> groupInfo = pair->LockedGetGroupInfo(aPersistenceType);
  if (!groupInfo) {
    return;
  }

  nsRefPtr<OriginInfo> originInfo = groupInfo->LockedGetOriginInfo(aOrigin);
  if (originInfo) {
    originInfo->LockedDecreaseUsage(aSize);
  }
}

void
QuotaManager::UpdateOriginAccessTime(PersistenceType aPersistenceType,
                                     const nsACString& aGroup,
                                     const nsACString& aOrigin)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  MutexAutoLock lock(mQuotaMutex);

  GroupInfoPair* pair;
  if (!mGroupInfoPairs.Get(aGroup, &pair)) {
    return;
  }

  nsRefPtr<GroupInfo> groupInfo =
    pair->LockedGetGroupInfo(PERSISTENCE_TYPE_TEMPORARY);
  if (!groupInfo) {
    return;
  }

  nsRefPtr<OriginInfo> originInfo = groupInfo->LockedGetOriginInfo(aOrigin);
  if (originInfo) {
    int64_t timestamp = PR_Now();
    originInfo->LockedUpdateAccessTime(timestamp);

    if (!groupInfo->IsForTemporaryStorage()) {
      return;
    }

    MutexAutoUnlock autoUnlock(mQuotaMutex);

    SaveOriginAccessTime(aOrigin, timestamp);
  }
}

// static
PLDHashOperator
QuotaManager::RemoveQuotaCallback(const nsACString& aKey,
                                  nsAutoPtr<GroupInfoPair>& aValue,
                                  void* aUserArg)
{
  NS_ASSERTION(!aKey.IsEmpty(), "Empty key!");
  NS_ASSERTION(aValue, "Null pointer!");

  nsRefPtr<GroupInfo> groupInfo =
    aValue->LockedGetGroupInfo(PERSISTENCE_TYPE_TEMPORARY);
  if (groupInfo) {
    groupInfo->LockedRemoveOriginInfos();
  }

  return PL_DHASH_REMOVE;
}

void
QuotaManager::RemoveQuota()
{
  MutexAutoLock lock(mQuotaMutex);

  mGroupInfoPairs.Enumerate(RemoveQuotaCallback, nullptr);

  NS_ASSERTION(mTemporaryStorageUsage == 0, "Should be zero!");
}

// static
PLDHashOperator
QuotaManager::RemoveQuotaForPersistenceTypeCallback(
                                               const nsACString& aKey,
                                               nsAutoPtr<GroupInfoPair>& aValue,
                                               void* aUserArg)
{
  NS_ASSERTION(!aKey.IsEmpty(), "Empty key!");
  NS_ASSERTION(aValue, "Null pointer!");
  NS_ASSERTION(aUserArg, "Null pointer!");

  PersistenceType& persistenceType = *static_cast<PersistenceType*>(aUserArg);

  if (persistenceType == PERSISTENCE_TYPE_TEMPORARY) {
    nsRefPtr<GroupInfo> groupInfo =
      aValue->LockedGetGroupInfo(persistenceType);
    if (groupInfo) {
      groupInfo->LockedRemoveOriginInfos();
    }
  }

  aValue->LockedClearGroupInfo(persistenceType);

  return aValue->LockedHasGroupInfos() ? PL_DHASH_NEXT : PL_DHASH_REMOVE;
}

void
QuotaManager::RemoveQuotaForPersistenceType(PersistenceType aPersistenceType)
{
  MutexAutoLock lock(mQuotaMutex);

  mGroupInfoPairs.Enumerate(RemoveQuotaForPersistenceTypeCallback,
                            &aPersistenceType);

  NS_ASSERTION(aPersistenceType == PERSISTENCE_TYPE_PERSISTENT ||
               mTemporaryStorageUsage == 0, "Should be zero!");
}

// static
PLDHashOperator
QuotaManager::RemoveQuotaForPatternCallback(const nsACString& aKey,
                                            nsAutoPtr<GroupInfoPair>& aValue,
                                            void* aUserArg)
{
  NS_ASSERTION(!aKey.IsEmpty(), "Empty key!");
  NS_ASSERTION(aValue, "Null pointer!");
  NS_ASSERTION(aUserArg, "Null pointer!");

  RemoveQuotaInfo* info = static_cast<RemoveQuotaInfo*>(aUserArg);

  nsRefPtr<GroupInfo> groupInfo =
    aValue->LockedGetGroupInfo(info->persistenceType);
  if (groupInfo) {
    groupInfo->LockedRemoveOriginInfosForPattern(info->pattern);

    if (!groupInfo->LockedHasOriginInfos()) {
      aValue->LockedClearGroupInfo(info->persistenceType);

      if (!aValue->LockedHasGroupInfos()) {
        return PL_DHASH_REMOVE;
      }
    }
  }

  return PL_DHASH_NEXT;
}

void
QuotaManager::RemoveQuotaForPattern(PersistenceType aPersistenceType,
                                    const nsACString& aPattern)
{
  NS_ASSERTION(!aPattern.IsEmpty(), "Empty pattern!");

  RemoveQuotaInfo info(aPersistenceType, aPattern);

  MutexAutoLock lock(mQuotaMutex);

  mGroupInfoPairs.Enumerate(RemoveQuotaForPatternCallback, &info);
}

already_AddRefed<QuotaObject>
QuotaManager::GetQuotaObject(PersistenceType aPersistenceType,
                             const nsACString& aGroup,
                             const nsACString& aOrigin,
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

  nsRefPtr<QuotaObject> result;
  {
    MutexAutoLock lock(mQuotaMutex);

    GroupInfoPair* pair;
    if (!mGroupInfoPairs.Get(aGroup, &pair)) {
      return nullptr;
    }

    nsRefPtr<GroupInfo> groupInfo = pair->LockedGetGroupInfo(aPersistenceType);

    if (!groupInfo) {
      return nullptr;
    }

    nsRefPtr<OriginInfo> originInfo = groupInfo->LockedGetOriginInfo(aOrigin);

    if (!originInfo) {
      return nullptr;
    }

    // We need this extra raw pointer because we can't assign to the smart
    // pointer directly since QuotaObject::AddRef would try to acquire the same
    // mutex.
    QuotaObject* quotaObject;
    if (!originInfo->mQuotaObjects.Get(path, &quotaObject)) {
      // Create a new QuotaObject.
      quotaObject = new QuotaObject(originInfo, path, fileSize);

      // Put it to the hashtable. The hashtable is not responsible to delete
      // the QuotaObject.
      originInfo->mQuotaObjects.Put(path, quotaObject);
    }

    // Addref the QuotaObject and move the ownership to the result. This must
    // happen before we unlock!
    result = quotaObject->LockedAddRef();
  }

  // The caller becomes the owner of the QuotaObject, that is, the caller is
  // is responsible to delete it when the last reference is removed.
  return result.forget();
}

already_AddRefed<QuotaObject>
QuotaManager::GetQuotaObject(PersistenceType aPersistenceType,
                             const nsACString& aGroup,
                             const nsACString& aOrigin,
                             const nsAString& aPath)
{
  nsresult rv;
  nsCOMPtr<nsIFile> file = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, nullptr);

  rv = file->InitWithPath(aPath);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return GetQuotaObject(aPersistenceType, aGroup, aOrigin, file);
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

    UpdateOriginAccessTime(aStorage->Type(), aStorage->Group(), origin);
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

      UpdateOriginAccessTime(aStorage->Type(), aStorage->Group(), origin);
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
    FindSynchronizedOp(aStorage->Origin(),
                       Nullable<PersistenceType>(aStorage->Type()),
                       aStorage->Id());
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
                                 Nullable<PersistenceType> aPersistenceType,
                                 const nsACString& aId, nsIRunnable* aRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aOriginOrPattern.IsEmpty() || aOriginOrPattern.IsNull(),
               "Empty pattern!");
  NS_ASSERTION(aRunnable, "Null pointer!");

  nsAutoPtr<SynchronizedOp> op(new SynchronizedOp(aOriginOrPattern,
                                                  aPersistenceType, aId));

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
QuotaManager::AddSynchronizedOp(const OriginOrPatternString& aOriginOrPattern,
                                Nullable<PersistenceType> aPersistenceType)
{
  nsAutoPtr<SynchronizedOp> op(new SynchronizedOp(aOriginOrPattern,
                                                  aPersistenceType,
                                                  EmptyCString()));

#ifdef DEBUG
  for (uint32_t index = mSynchronizedOps.Length(); index > 0; index--) {
    nsAutoPtr<SynchronizedOp>& existingOp = mSynchronizedOps[index - 1];
    NS_ASSERTION(!op->MustWaitFor(*existingOp), "What?");
  }
#endif

  mSynchronizedOps.AppendElement(op.forget());
}

void
QuotaManager::AllowNextSynchronizedOp(
                                  const OriginOrPatternString& aOriginOrPattern,
                                  Nullable<PersistenceType> aPersistenceType,
                                  const nsACString& aId)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aOriginOrPattern.IsEmpty() || aOriginOrPattern.IsNull(),
               "Empty origin/pattern!");

  uint32_t count = mSynchronizedOps.Length();
  for (uint32_t index = 0; index < count; index++) {
    nsAutoPtr<SynchronizedOp>& op = mSynchronizedOps[index];
    if (op->mOriginOrPattern.IsOrigin() == aOriginOrPattern.IsOrigin() &&
        op->mOriginOrPattern == aOriginOrPattern &&
        op->mPersistenceType == aPersistenceType) {
      if (op->mId == aId) {
        NS_ASSERTION(op->mStorages.IsEmpty(), "How did this happen?");

        op->DispatchDelayedRunnables();

        mSynchronizedOps.RemoveElementAt(index);
        return;
      }

      // If one or the other is for an origin clear, we should have matched
      // solely on origin.
      NS_ASSERTION(!op->mId.IsEmpty() && !aId.IsEmpty(),
                   "Why didn't we match earlier?");
    }
  }

  NS_NOTREACHED("Why didn't we find a SynchronizedOp?");
}

nsresult
QuotaManager::GetDirectoryForOrigin(PersistenceType aPersistenceType,
                                    const nsACString& aASCIIOrigin,
                                    nsIFile** aDirectory) const
{
  nsresult rv;
  nsCOMPtr<nsIFile> directory =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = directory->InitWithPath(GetStoragePath(aPersistenceType));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString originSanitized(aASCIIOrigin);
  SanitizeOriginString(originSanitized);

  rv = directory->Append(NS_ConvertASCIItoUTF16(originSanitized));
  NS_ENSURE_SUCCESS(rv, rv);

  directory.forget(aDirectory);
  return NS_OK;
}

nsresult
QuotaManager::InitializeOrigin(PersistenceType aPersistenceType,
                               const nsACString& aGroup,
                               const nsACString& aOrigin,
                               bool aTrackQuota,
                               int64_t aAccessTime,
                               nsIFile* aDirectory)
{
  AssertIsOnIOThread();

  nsresult rv;

  bool temporaryStorage = aPersistenceType == PERSISTENCE_TYPE_TEMPORARY;
  if (!temporaryStorage) {
    rv = MaybeUpgradeOriginDirectory(aDirectory);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // We need to initialize directories of all clients if they exists and also
  // get the total usage to initialize the quota.
  nsAutoPtr<UsageInfo> usageInfo;
  if (aTrackQuota) {
    usageInfo = new UsageInfo();
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

    if (leafName.EqualsLiteral(METADATA_FILE_NAME) ||
        leafName.EqualsLiteral(DSSTORE_FILE_NAME)) {
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

    rv = mClients[clientType]->InitOrigin(aPersistenceType, aGroup, aOrigin,
                                          usageInfo);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aTrackQuota) {
    uint64_t quotaMaxBytes;
    uint64_t totalUsageBytes = usageInfo->TotalUsage();

    if (temporaryStorage) {
      // Temporary storage has no limit for origin usage (there's a group and
      // the global limit though).
      quotaMaxBytes = 0;
    }
    else {
      quotaMaxBytes = GetStorageQuotaMB() * 1024 * 1024;
      if (totalUsageBytes > quotaMaxBytes) {
        NS_WARNING("Origin is already using more storage than allowed!");
        return NS_ERROR_UNEXPECTED;
      }
    }

    InitQuotaForOrigin(aPersistenceType, aGroup, aOrigin, quotaMaxBytes,
                       totalUsageBytes, aAccessTime);
  }

  return NS_OK;
}

nsresult
QuotaManager::MaybeUpgradeIndexedDBDirectory()
{
  AssertIsOnIOThread();

  if (mStorageAreaInitialized) {
    return NS_OK;
  }

  nsresult rv;

  nsCOMPtr<nsIFile> indexedDBDir =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = indexedDBDir->InitWithPath(mIndexedDBPath);
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists;
  rv = indexedDBDir->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!exists) {
    // Nothing to upgrade.
    mStorageAreaInitialized = true;

    return NS_OK;
  }

  bool isDirectory;
  rv = indexedDBDir->IsDirectory(&isDirectory);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!isDirectory) {
    NS_WARNING("indexedDB entry is not a directory!");
    return NS_OK;
  }

  nsCOMPtr<nsIFile> persistentStorageDir =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = persistentStorageDir->InitWithPath(mPersistentStoragePath);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = persistentStorageDir->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {
    NS_WARNING("indexedDB directory shouldn't exist after the upgrade!");
    return NS_OK;
  }

  nsCOMPtr<nsIFile> storageDir;
  rv = persistentStorageDir->GetParent(getter_AddRefs(storageDir));
  NS_ENSURE_SUCCESS(rv, rv);

  nsString persistentStorageName;
  rv = persistentStorageDir->GetLeafName(persistentStorageName);
  NS_ENSURE_SUCCESS(rv, rv);

  // MoveTo() is atomic if the move happens on the same volume which should
  // be our case, so even if we crash in the middle of the operation nothing
  // breaks next time we try to initialize.
  // However there's a theoretical possibility that the indexedDB directory
  // is on different volume, but it should be rare enough that we don't have
  // to worry about it.
  rv = indexedDBDir->MoveTo(storageDir, persistentStorageName);
  NS_ENSURE_SUCCESS(rv, rv);

  mStorageAreaInitialized = true;

  return NS_OK;
}

nsresult
QuotaManager::EnsureOriginIsInitialized(PersistenceType aPersistenceType,
                                        const nsACString& aGroup,
                                        const nsACString& aOrigin,
                                        bool aTrackQuota,
                                        nsIFile** aDirectory)
{
  AssertIsOnIOThread();

  nsresult rv = MaybeUpgradeIndexedDBDirectory();
  NS_ENSURE_SUCCESS(rv, rv);

  // Get directory for this origin and persistence type.
  nsCOMPtr<nsIFile> directory;
  rv = GetDirectoryForOrigin(aPersistenceType, aOrigin,
                             getter_AddRefs(directory));
  NS_ENSURE_SUCCESS(rv, rv);

  if (aPersistenceType == PERSISTENCE_TYPE_PERSISTENT) {
    if (mInitializedOrigins.Contains(aOrigin)) {
      NS_ADDREF(*aDirectory = directory);
      return NS_OK;
    }

    bool created;
    rv = EnsureDirectory(directory, &created);
    NS_ENSURE_SUCCESS(rv, rv);

    if (created) {
      rv = CreateDirectoryUpgradeStamp(directory);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = InitializeOrigin(aPersistenceType, aGroup, aOrigin, aTrackQuota, 0,
                          directory);
    NS_ENSURE_SUCCESS(rv, rv);

    mInitializedOrigins.AppendElement(aOrigin);

    directory.forget(aDirectory);
    return NS_OK;
  }

  NS_ASSERTION(aPersistenceType == PERSISTENCE_TYPE_TEMPORARY, "Huh?");
  NS_ASSERTION(aTrackQuota, "Huh?");

  if (!mTemporaryStorageInitialized) {
    nsCOMPtr<nsIFile> parentDirectory;
    rv = directory->GetParent(getter_AddRefs(parentDirectory));
    NS_ENSURE_SUCCESS(rv, rv);

    bool created;
    rv = EnsureDirectory(parentDirectory, &created);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISimpleEnumerator> entries;
    rv = parentDirectory->GetDirectoryEntries(getter_AddRefs(entries));
    NS_ENSURE_SUCCESS(rv, rv);

    bool hasMore;
    while (NS_SUCCEEDED((rv = entries->HasMoreElements(&hasMore))) && hasMore) {
      nsCOMPtr<nsISupports> entry;
      rv = entries->GetNext(getter_AddRefs(entry));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIFile> childDirectory = do_QueryInterface(entry);
      NS_ENSURE_TRUE(childDirectory, NS_NOINTERFACE);

      bool isDirectory;
      rv = childDirectory->IsDirectory(&isDirectory);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_TRUE(isDirectory, NS_ERROR_UNEXPECTED);

      int64_t timestamp;
      nsCString group;
      nsCString origin;
      rv = GetDirectoryMetadata(childDirectory, &timestamp, group, origin);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = InitializeOrigin(aPersistenceType, group, origin, aTrackQuota,
                            timestamp, childDirectory);
      if (NS_FAILED(rv)) {
        NS_WARNING("Failed to initialize origin!");

        // We have to cleanup partially initialized quota for temporary storage.
        RemoveQuotaForPersistenceType(aPersistenceType);

        return rv;
      }
    }

    if (gFixedLimitKB >= 0) {
      mTemporaryStorageLimit = gFixedLimitKB * 1024;
    }
    else {
      rv = GetTemporaryStorageLimit(parentDirectory, mTemporaryStorageUsage,
                                    &mTemporaryStorageLimit);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    CheckTemporaryStorageLimits();

    mTemporaryStorageInitialized = true;
  }

  bool created;
  rv = EnsureDirectory(directory, &created);
  NS_ENSURE_SUCCESS(rv, rv);

  if (created) {
    int64_t timestamp = PR_Now();

    rv = CreateDirectoryMetadata(directory, timestamp, aGroup, aOrigin);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = InitializeOrigin(aPersistenceType, aGroup, aOrigin, aTrackQuota,
                          timestamp, directory);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  directory.forget(aDirectory);
  return NS_OK;
}

void
QuotaManager::OriginClearCompleted(
                                  PersistenceType aPersistenceType,
                                  const OriginOrPatternString& aOriginOrPattern)
{
  AssertIsOnIOThread();

  if (aPersistenceType == PERSISTENCE_TYPE_PERSISTENT) {
    if (aOriginOrPattern.IsOrigin()) {
      mInitializedOrigins.RemoveElement(aOriginOrPattern);
    }
    else {
      for (uint32_t index = mInitializedOrigins.Length(); index > 0; index--) {
        if (PatternMatchesOrigin(aOriginOrPattern,
                                 mInitializedOrigins[index - 1])) {
          mInitializedOrigins.RemoveElementAt(index - 1);
        }
      }
    }
  }

  for (uint32_t index = 0; index < Client::TYPE_MAX; index++) {
    mClients[index]->OnOriginClearCompleted(aPersistenceType, aOriginOrPattern);
  }
}

void
QuotaManager::ResetOrClearCompleted()
{
  AssertIsOnIOThread();

  mInitializedOrigins.Clear();
  mTemporaryStorageInitialized = false;

  ReleaseIOThreadObjects();
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
void
QuotaManager::GetStorageId(PersistenceType aPersistenceType,
                           const nsACString& aOrigin,
                           Client::Type aClientType,
                           const nsAString& aName,
                           nsACString& aDatabaseId)
{
  nsAutoCString str;
  str.AppendInt(aPersistenceType);
  str.Append('*');
  str.Append(aOrigin);
  str.Append('*');
  str.AppendInt(aClientType);
  str.Append('*');
  str.Append(NS_ConvertUTF16toUTF8(aName));

  aDatabaseId = str;
}

// static
nsresult
QuotaManager::GetInfoFromURI(nsIURI* aURI,
                             uint32_t aAppId,
                             bool aInMozBrowser,
                             nsACString* aGroup,
                             nsACString* aASCIIOrigin,
                             StoragePrivilege* aPrivilege,
                             PersistenceType* aDefaultPersistenceType)
{
  NS_ASSERTION(aURI, "Null uri!");

  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  NS_ENSURE_TRUE(secMan, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv = secMan->GetAppCodebasePrincipal(aURI, aAppId, aInMozBrowser,
                                                getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = GetInfoFromPrincipal(principal, aGroup, aASCIIOrigin, aPrivilege,
                            aDefaultPersistenceType);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// static
nsresult
QuotaManager::GetInfoFromPrincipal(nsIPrincipal* aPrincipal,
                                   nsACString* aGroup,
                                   nsACString* aASCIIOrigin,
                                   StoragePrivilege* aPrivilege,
                                   PersistenceType* aDefaultPersistenceType)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aPrincipal, "Don't hand me a null principal!");

  if (nsContentUtils::IsSystemPrincipal(aPrincipal)) {
    GetInfoForChrome(aGroup, aASCIIOrigin, aPrivilege, aDefaultPersistenceType);
    return NS_OK;
  }

  bool isNullPrincipal;
  nsresult rv = aPrincipal->GetIsNullPrincipal(&isNullPrincipal);
  NS_ENSURE_SUCCESS(rv, rv);

  if (isNullPrincipal) {
    NS_WARNING("IndexedDB not supported from this principal!");
    return NS_ERROR_FAILURE;
  }

  nsCString origin;
  rv = aPrincipal->GetOrigin(getter_Copies(origin));
  NS_ENSURE_SUCCESS(rv, rv);

  if (origin.EqualsLiteral("chrome")) {
    NS_WARNING("Non-chrome principal can't use chrome origin!");
    return NS_ERROR_FAILURE;
  }

  nsCString jarPrefix;
  if (aGroup || aASCIIOrigin) {
    rv = aPrincipal->GetJarPrefix(jarPrefix);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aGroup) {
    nsCString baseDomain;
    rv = aPrincipal->GetBaseDomain(baseDomain);
    if (NS_FAILED(rv)) {
      // A hack for JetPack.

      nsCOMPtr<nsIURI> uri;
      rv = aPrincipal->GetURI(getter_AddRefs(uri));
      NS_ENSURE_SUCCESS(rv, rv);

      bool isIndexedDBURI = false;
      rv = uri->SchemeIs("indexedDB", &isIndexedDBURI);
      NS_ENSURE_SUCCESS(rv, rv);

      if (isIndexedDBURI) {
        rv = NS_OK;
      }
    }
    NS_ENSURE_SUCCESS(rv, rv);

    if (baseDomain.IsEmpty()) {
      aGroup->Assign(jarPrefix + origin);
    }
    else {
      aGroup->Assign(jarPrefix + baseDomain);
    }
  }

  if (aASCIIOrigin) {
    aASCIIOrigin->Assign(jarPrefix + origin);
  }

  if (aPrivilege) {
    *aPrivilege = Content;
  }

  if (aDefaultPersistenceType) {
    *aDefaultPersistenceType = PERSISTENCE_TYPE_PERSISTENT;
  }

  return NS_OK;
}

// static
nsresult
QuotaManager::GetInfoFromWindow(nsPIDOMWindow* aWindow,
                                nsACString* aGroup,
                                nsACString* aASCIIOrigin,
                                StoragePrivilege* aPrivilege,
                                PersistenceType* aDefaultPersistenceType)
{
  NS_ASSERTION(NS_IsMainThread(),
               "We're about to touch a window off the main thread!");
  NS_ASSERTION(aWindow, "Don't hand me a null window!");

  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aWindow);
  NS_ENSURE_TRUE(sop, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPrincipal> principal = sop->GetPrincipal();
  NS_ENSURE_TRUE(principal, NS_ERROR_FAILURE);

  nsresult rv = GetInfoFromPrincipal(principal, aGroup, aASCIIOrigin,
                                     aPrivilege, aDefaultPersistenceType);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// static
void
QuotaManager::GetInfoForChrome(nsACString* aGroup,
                               nsACString* aASCIIOrigin,
                               StoragePrivilege* aPrivilege,
                               PersistenceType* aDefaultPersistenceType)
{
  NS_ASSERTION(nsContentUtils::IsCallerChrome(), "Only for chrome!");

  static const char kChromeOrigin[] = "chrome";

  if (aGroup) {
    aGroup->AssignLiteral(kChromeOrigin);
  }
  if (aASCIIOrigin) {
    aASCIIOrigin->AssignLiteral(kChromeOrigin);
  }
  if (aPrivilege) {
    *aPrivilege = Chrome;
  }
  if (aDefaultPersistenceType) {
    *aDefaultPersistenceType = PERSISTENCE_TYPE_PERSISTENT;
  }
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
  nsCString group;
  nsCString origin;
  nsresult rv = GetInfoFromURI(aURI, aAppId, aInMozBrowserOnly, &group, &origin,
                               nullptr, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  OriginOrPatternString oops = OriginOrPatternString::FromOrigin(origin);

  nsRefPtr<AsyncUsageRunnable> runnable =
    new AsyncUsageRunnable(aAppId, aInMozBrowserOnly, group, oops, aURI,
                           aCallback);

  // Put the computation runnable in the queue.
  rv = WaitForOpenAllowed(oops, Nullable<PersistenceType>(), EmptyCString(),
                          runnable);
  NS_ENSURE_SUCCESS(rv, rv);

  runnable->AdvanceState();

  runnable.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
QuotaManager::Clear()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!gTestingEnabled) {
    NS_WARNING("Testing features are not enabled!");
    return NS_OK;
  }

  OriginOrPatternString oops = OriginOrPatternString::FromNull();

  nsRefPtr<ResetOrClearRunnable> runnable = new ResetOrClearRunnable(true);

  // Put the clear runnable in the queue.
  nsresult rv =
    WaitForOpenAllowed(oops, Nullable<PersistenceType>(), EmptyCString(),
                       runnable);
  NS_ENSURE_SUCCESS(rv, rv);

  runnable->AdvanceState();

  // Give the runnable some help by invalidating any storages in the way.
  StorageMatcher<nsAutoTArray<nsIOfflineStorage*, 20> > matches;
  matches.Find(mLiveStorages);

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
  nsresult rv = GetInfoFromURI(aURI, aAppId, aInMozBrowserOnly, nullptr, &origin,
                               nullptr, nullptr);
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

  rv = WaitForOpenAllowed(oops, Nullable<PersistenceType>(), EmptyCString(),
                          runnable);
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
QuotaManager::Reset()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!gTestingEnabled) {
    NS_WARNING("Testing features are not enabled!");
    return NS_OK;
  }

  OriginOrPatternString oops = OriginOrPatternString::FromNull();

  nsRefPtr<ResetOrClearRunnable> runnable = new ResetOrClearRunnable(false);

  // Put the reset runnable in the queue.
  nsresult rv =
    WaitForOpenAllowed(oops, Nullable<PersistenceType>(), EmptyCString(),
                       runnable);
  NS_ENSURE_SUCCESS(rv, rv);

  runnable->AdvanceState();

  // Give the runnable some help by invalidating any storages in the way.
  StorageMatcher<nsAutoTArray<nsIOfflineStorage*, 20> > matches;
  matches.Find(mLiveStorages);

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
                      const char16_t* aData)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!strcmp(aTopic, PROFILE_BEFORE_CHANGE_OBSERVER_ID)) {
    // Setting this flag prevents the service from being recreated and prevents
    // further storagess from being created.
    if (gShutdown.exchange(1)) {
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

    // Unlock while calling out to XPCOM (code behind the dispatch method needs
    // to acquire its own lock which can potentially lead to a deadlock and it
    // also calls an observer that can do various stuff like IO, so it's better
    // to not hold our mutex while that happens).
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

uint64_t
QuotaManager::LockedCollectOriginsForEviction(
                                            uint64_t aMinSizeToBeFreed,
                                            nsTArray<OriginInfo*>& aOriginInfos)
{
  mQuotaMutex.AssertCurrentThreadOwns();

  nsRefPtr<CollectOriginsHelper> helper =
    new CollectOriginsHelper(mQuotaMutex, aMinSizeToBeFreed);

  // Unlock while calling out to XPCOM (see the detailed comment in
  // LockedQuotaIsLifted)
  {
    MutexAutoUnlock autoUnlock(mQuotaMutex);

    if (NS_FAILED(NS_DispatchToMainThread(helper))) {
      NS_WARNING("Failed to dispatch to the main thread!");
    }
  }

  return helper->BlockAndReturnOriginsForEviction(aOriginInfos);
}

void
QuotaManager::LockedRemoveQuotaForOrigin(PersistenceType aPersistenceType,
                                         const nsACString& aGroup,
                                         const nsACString& aOrigin)
{
  mQuotaMutex.AssertCurrentThreadOwns();

  GroupInfoPair* pair;
  mGroupInfoPairs.Get(aGroup, &pair);

  if (!pair) {
    return;
  }

  nsRefPtr<GroupInfo> groupInfo = pair->LockedGetGroupInfo(aPersistenceType);
  if (groupInfo) {
    groupInfo->LockedRemoveOriginInfo(aOrigin);

    if (!groupInfo->LockedHasOriginInfos()) {
      pair->LockedClearGroupInfo(aPersistenceType);

      if (!pair->LockedHasGroupInfos()) {
        mGroupInfoPairs.Remove(aGroup);
      }
    }
  }
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
  SynchronizedOp* op;
  if (aStorage) {
    op = FindSynchronizedOp(aPattern,
                            Nullable<PersistenceType>(aStorage->Type()),
                            aStorage->Id());
  }
  else {
    op = FindSynchronizedOp(aPattern, Nullable<PersistenceType>(),
                            EmptyCString());
  }

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
    if (aPattern.IsVoid()) {
      matches.Find(mLiveStorages);
    }
    else {
      matches.Find(mLiveStorages, aPattern);
    }

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
                                 Nullable<PersistenceType> aPersistenceType,
                                 const nsACString& aId)
{
  for (uint32_t index = 0; index < mSynchronizedOps.Length(); index++) {
    const nsAutoPtr<SynchronizedOp>& currentOp = mSynchronizedOps[index];
    if (PatternMatchesOrigin(aPattern, currentOp->mOriginOrPattern) &&
        (currentOp->mPersistenceType.IsNull() ||
         currentOp->mPersistenceType == aPersistenceType) &&
        (currentOp->mId.IsEmpty() || currentOp->mId == aId)) {
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

  nsresult rv =
    WaitForOpenAllowed(oops, Nullable<PersistenceType>(), EmptyCString(),
                       runnable);
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

// static
PLDHashOperator
QuotaManager::GetOriginsExceedingGroupLimit(const nsACString& aKey,
                                            GroupInfoPair* aValue,
                                            void* aUserArg)
{
  NS_ASSERTION(!aKey.IsEmpty(), "Empty key!");
  NS_ASSERTION(aValue, "Null pointer!");

  nsRefPtr<GroupInfo> groupInfo =
    aValue->LockedGetGroupInfo(PERSISTENCE_TYPE_TEMPORARY);
  if (groupInfo) {
    QuotaManager* quotaManager = QuotaManager::Get();
    NS_ASSERTION(quotaManager, "Shouldn't be null!");

    if (groupInfo->mUsage > quotaManager->GetGroupLimit()) {
      nsTArray<OriginInfo*>* doomedOriginInfos =
        static_cast<nsTArray<OriginInfo*>*>(aUserArg);

      nsTArray<nsRefPtr<OriginInfo> >& originInfos = groupInfo->mOriginInfos;
      originInfos.Sort(OriginInfoLRUComparator());

      uint64_t usage = groupInfo->mUsage;
      for (uint32_t i = 0; i < originInfos.Length(); i++) {
        OriginInfo* originInfo = originInfos[i];

        doomedOriginInfos->AppendElement(originInfo);
        usage -= originInfo->mUsage;

        if (usage <= quotaManager->GetGroupLimit()) {
          break;
        }
      }
    }
  }

  return PL_DHASH_NEXT;
}

// static
PLDHashOperator
QuotaManager::GetAllTemporaryStorageOrigins(const nsACString& aKey,
                                            GroupInfoPair* aValue,
                                            void* aUserArg)
{
  NS_ASSERTION(!aKey.IsEmpty(), "Empty key!");
  NS_ASSERTION(aValue, "Null pointer!");

  nsRefPtr<GroupInfo> groupInfo =
    aValue->LockedGetGroupInfo(PERSISTENCE_TYPE_TEMPORARY);
  if (groupInfo) {
    nsTArray<OriginInfo*>* originInfos =
      static_cast<nsTArray<OriginInfo*>*>(aUserArg);

    originInfos->AppendElements(groupInfo->mOriginInfos);
  }

  return PL_DHASH_NEXT;
}

void
QuotaManager::CheckTemporaryStorageLimits()
{
  AssertIsOnIOThread();

  nsTArray<OriginInfo*> doomedOriginInfos;
  {
    MutexAutoLock lock(mQuotaMutex);

    mGroupInfoPairs.EnumerateRead(GetOriginsExceedingGroupLimit,
                                  &doomedOriginInfos);

    uint64_t usage = 0;
    for (uint32_t index = 0; index < doomedOriginInfos.Length(); index++) {
      usage += doomedOriginInfos[index]->mUsage;
    }

    if (mTemporaryStorageUsage - usage > mTemporaryStorageLimit) {
      nsTArray<OriginInfo*> originInfos;

      mGroupInfoPairs.EnumerateRead(GetAllTemporaryStorageOrigins,
                                    &originInfos);

      for (uint32_t index = originInfos.Length(); index > 0; index--) {
        if (doomedOriginInfos.Contains(originInfos[index - 1])) {
          originInfos.RemoveElementAt(index - 1);
        }
      }

      originInfos.Sort(OriginInfoLRUComparator());

      for (uint32_t i = 0; i < originInfos.Length(); i++) {
        if (mTemporaryStorageUsage - usage <= mTemporaryStorageLimit) {
          originInfos.TruncateLength(i);
          break;
        }

        usage += originInfos[i]->mUsage;
      }

      doomedOriginInfos.AppendElements(originInfos);
    }
  }

  for (uint32_t index = 0; index < doomedOriginInfos.Length(); index++) {
    DeleteTemporaryFilesForOrigin(doomedOriginInfos[index]->mOrigin);
  }

  nsTArray<nsCString> doomedOrigins;
  {
    MutexAutoLock lock(mQuotaMutex);

    for (uint32_t index = 0; index < doomedOriginInfos.Length(); index++) {
      OriginInfo* doomedOriginInfo = doomedOriginInfos[index];

      nsCString group = doomedOriginInfo->mGroupInfo->mGroup;
      nsCString origin = doomedOriginInfo->mOrigin;
      LockedRemoveQuotaForOrigin(PERSISTENCE_TYPE_TEMPORARY, group, origin);

#ifdef DEBUG
      doomedOriginInfos[index] = nullptr;
#endif

      doomedOrigins.AppendElement(origin);
    }
  }

  for (uint32_t index = 0; index < doomedOrigins.Length(); index++) {
    OriginClearCompleted(
                       PERSISTENCE_TYPE_TEMPORARY,
                       OriginOrPatternString::FromOrigin(doomedOrigins[index]));
  }
}

// static
PLDHashOperator
QuotaManager::AddTemporaryStorageOrigins(
                                       const nsACString& aKey,
                                       ArrayCluster<nsIOfflineStorage*>* aValue,
                                       void* aUserArg)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aKey.IsEmpty(), "Empty key!");
  NS_ASSERTION(aValue, "Null pointer!");
  NS_ASSERTION(aUserArg, "Null pointer!");

  OriginCollection& collection = *static_cast<OriginCollection*>(aUserArg);

  if (collection.ContainsOrigin(aKey)) {
    return PL_DHASH_NEXT;
  }

  for (uint32_t i = 0; i < Client::TYPE_MAX; i++) {
    nsTArray<nsIOfflineStorage*>& array = (*aValue)[i];
    for (uint32_t j = 0; j < array.Length(); j++) {
      nsIOfflineStorage*& storage = array[j];
      if (storage->Type() == PERSISTENCE_TYPE_TEMPORARY) {
        collection.AddOrigin(aKey);
        return PL_DHASH_NEXT;
      }
    }
  }

  return PL_DHASH_NEXT;
}

// static
PLDHashOperator
QuotaManager::GetInactiveTemporaryStorageOrigins(const nsACString& aKey,
                                                 GroupInfoPair* aValue,
                                                 void* aUserArg)
{
  NS_ASSERTION(!aKey.IsEmpty(), "Empty key!");
  NS_ASSERTION(aValue, "Null pointer!");
  NS_ASSERTION(aUserArg, "Null pointer!");

  nsRefPtr<GroupInfo> groupInfo =
    aValue->LockedGetGroupInfo(PERSISTENCE_TYPE_TEMPORARY);
  if (groupInfo) {
    InactiveOriginsInfo* info = static_cast<InactiveOriginsInfo*>(aUserArg);

    nsTArray<nsRefPtr<OriginInfo> >& originInfos = groupInfo->mOriginInfos;

    for (uint32_t i = 0; i < originInfos.Length(); i++) {
      OriginInfo* originInfo = originInfos[i];

      if (!info->collection.ContainsOrigin(originInfo->mOrigin)) {
        NS_ASSERTION(!originInfo->mQuotaObjects.Count(),
                     "Inactive origin shouldn't have open files!");
        info->origins.AppendElement(originInfo);
      }
    }
  }

  return PL_DHASH_NEXT;
}

uint64_t
QuotaManager::CollectOriginsForEviction(uint64_t aMinSizeToBeFreed,
                                        nsTArray<OriginInfo*>& aOriginInfos)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // Collect active origins first.
  OriginCollection originCollection;

  // Add patterns and origins that have running or pending synchronized ops.
  // (add patterns first to reduce redundancy in the origin collection).
  uint32_t index;
  for (index = 0; index < mSynchronizedOps.Length(); index++) {
    nsAutoPtr<SynchronizedOp>& op = mSynchronizedOps[index];
    if (op->mPersistenceType.IsNull() ||
        op->mPersistenceType.Value() == PERSISTENCE_TYPE_TEMPORARY) {
      if (op->mOriginOrPattern.IsPattern() &&
          !originCollection.ContainsPattern(op->mOriginOrPattern)) {
        originCollection.AddPattern(op->mOriginOrPattern);
      }
    }
  }

  for (index = 0; index < mSynchronizedOps.Length(); index++) {
    nsAutoPtr<SynchronizedOp>& op = mSynchronizedOps[index];
    if (op->mPersistenceType.IsNull() ||
        op->mPersistenceType.Value() == PERSISTENCE_TYPE_TEMPORARY) {
      if (op->mOriginOrPattern.IsOrigin() &&
          !originCollection.ContainsOrigin(op->mOriginOrPattern)) {
        originCollection.AddOrigin(op->mOriginOrPattern);
      }
    }
  }

  // Add origins that have live temporary storages.
  mLiveStorages.EnumerateRead(AddTemporaryStorageOrigins, &originCollection);

  // Enumerate inactive origins. This must be protected by the mutex.
  nsTArray<OriginInfo*> inactiveOrigins;
  {
    InactiveOriginsInfo info(originCollection, inactiveOrigins);
    MutexAutoLock lock(mQuotaMutex);
    mGroupInfoPairs.EnumerateRead(GetInactiveTemporaryStorageOrigins, &info);
  }

  // We now have a list of all inactive origins. So it's safe to sort the list
  // and calculate available size without holding the lock.

  // Sort by the origin access time.
  inactiveOrigins.Sort(OriginInfoLRUComparator());

  // Create a list of inactive and the least recently used origins
  // whose aggregate size is greater or equals the minimal size to be freed.
  uint64_t sizeToBeFreed = 0;
  for(index = 0; index < inactiveOrigins.Length(); index++) {
    if (sizeToBeFreed >= aMinSizeToBeFreed) {
      inactiveOrigins.TruncateLength(index);
      break;
    }

    sizeToBeFreed += inactiveOrigins[index]->mUsage;
  }

  if (sizeToBeFreed >= aMinSizeToBeFreed) {
    // Success, add synchronized ops for these origins, so any other
    // operations for them will be delayed (until origin eviction is finalized).

    for(index = 0; index < inactiveOrigins.Length(); index++) {
      OriginOrPatternString oops =
        OriginOrPatternString::FromOrigin(inactiveOrigins[index]->mOrigin);

      AddSynchronizedOp(oops,
                        Nullable<PersistenceType>(PERSISTENCE_TYPE_TEMPORARY));
    }

    inactiveOrigins.SwapElements(aOriginInfos);
    return sizeToBeFreed;
  }

  return 0;
}

void
QuotaManager::DeleteTemporaryFilesForOrigin(const nsACString& aOrigin)
{
  nsCOMPtr<nsIFile> directory;
  nsresult rv = GetDirectoryForOrigin(PERSISTENCE_TYPE_TEMPORARY, aOrigin,
                                      getter_AddRefs(directory));
  NS_ENSURE_SUCCESS_VOID(rv);

  rv = directory->Remove(true);
  if (rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST &&
      rv != NS_ERROR_FILE_NOT_FOUND && NS_FAILED(rv)) {
    // This should never fail if we've closed all storage connections
    // correctly...
    NS_ERROR("Failed to remove directory!");
  }
}

void
QuotaManager::FinalizeOriginEviction(nsTArray<nsCString>& aOrigins)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<FinalizeOriginEvictionRunnable> runnable =
    new FinalizeOriginEvictionRunnable(aOrigins);

  nsresult rv = IsOnIOThread() ? runnable->RunImmediately()
                               : runnable->Dispatch();
  NS_ENSURE_SUCCESS_VOID(rv);
}

void
QuotaManager::SaveOriginAccessTime(const nsACString& aOrigin,
                                   int64_t aTimestamp)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (QuotaManager::IsShuttingDown()) {
    return;
  }

  nsRefPtr<SaveOriginAccessTimeRunnable> runnable =
    new SaveOriginAccessTimeRunnable(aOrigin, aTimestamp);

  if (NS_FAILED(mIOThread->Dispatch(runnable, NS_DISPATCH_NORMAL))) {
    NS_WARNING("Failed to dispatch runnable!");
  }
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
                               Nullable<PersistenceType> aPersistenceType,
                               const nsACString& aId)
: mOriginOrPattern(aOriginOrPattern), mPersistenceType(aPersistenceType),
  mId(aId)
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

  if (aExistingOp.mOriginOrPattern.IsNull() || mOriginOrPattern.IsNull()) {
    return true;
  }

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

  // If the origins match but the persistence types are different, the second
  // can proceed.
  if (!aExistingOp.mPersistenceType.IsNull() && !mPersistenceType.IsNull() &&
      aExistingOp.mPersistenceType.Value() != mPersistenceType.Value()) {
    return false;
  }

  // If the origins and the ids match, the second must wait.
  if (aExistingOp.mId == mId) {
    return true;
  }

  // Waiting is required if either one corresponds to an origin clearing
  // (an empty Id).
  if (aExistingOp.mId.IsEmpty() || mId.IsEmpty()) {
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
  NS_ASSERTION(mDelayedRunnables.IsEmpty() || mId.IsEmpty(),
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

CollectOriginsHelper::CollectOriginsHelper(mozilla::Mutex& aMutex,
                                           uint64_t aMinSizeToBeFreed)
: mMinSizeToBeFreed(aMinSizeToBeFreed),
  mMutex(aMutex),
  mCondVar(aMutex, "CollectOriginsHelper::mCondVar"),
  mSizeToBeFreed(0),
  mWaiting(true)
{
  MOZ_ASSERT(!NS_IsMainThread(), "Wrong thread!");
  mMutex.AssertCurrentThreadOwns();
}

int64_t
CollectOriginsHelper::BlockAndReturnOriginsForEviction(
                                            nsTArray<OriginInfo*>& aOriginInfos)
{
  MOZ_ASSERT(!NS_IsMainThread(), "Wrong thread!");
  mMutex.AssertCurrentThreadOwns();

  while (mWaiting) {
    mCondVar.Wait();
  }

  mOriginInfos.SwapElements(aOriginInfos);
  return mSizeToBeFreed;
}

NS_IMETHODIMP
CollectOriginsHelper::Run()
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "Shouldn't be null!");

  // We use extra stack vars here to avoid race detector warnings (the same
  // memory accessed with and without the lock held).
  nsTArray<OriginInfo*> originInfos;
  uint64_t sizeToBeFreed =
    quotaManager->CollectOriginsForEviction(mMinSizeToBeFreed, originInfos);

  MutexAutoLock lock(mMutex);

  NS_ASSERTION(mWaiting, "Huh?!");

  mOriginInfos.SwapElements(originInfos);
  mSizeToBeFreed = sizeToBeFreed;
  mWaiting = false;
  mCondVar.Notify();

  return NS_OK;
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
OriginClearRunnable::DeleteFiles(QuotaManager* aQuotaManager,
                                 PersistenceType aPersistenceType)
{
  AssertIsOnIOThread();
  NS_ASSERTION(aQuotaManager, "Don't pass me null!");

  nsresult rv;

  nsCOMPtr<nsIFile> directory =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS_VOID(rv);

  rv = directory->InitWithPath(aQuotaManager->GetStoragePath(aPersistenceType));
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

  aQuotaManager->RemoveQuotaForPattern(aPersistenceType, mOriginOrPattern);

  aQuotaManager->OriginClearCompleted(aPersistenceType, mOriginOrPattern);
}

NS_IMPL_ISUPPORTS_INHERITED0(OriginClearRunnable, nsRunnable)

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

      DeleteFiles(quotaManager, PERSISTENCE_TYPE_PERSISTENT);

      DeleteFiles(quotaManager, PERSISTENCE_TYPE_TEMPORARY);

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
      quotaManager->AllowNextSynchronizedOp(mOriginOrPattern,
                                            Nullable<PersistenceType>(),
                                            EmptyCString());

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
                                       const nsACString& aGroup,
                                       const OriginOrPatternString& aOrigin,
                                       nsIURI* aURI,
                                       nsIUsageCallback* aCallback)
: mURI(aURI),
  mCallback(aCallback),
  mAppId(aAppId),
  mGroup(aGroup),
  mOrigin(aOrigin),
  mCallbackState(Pending),
  mInMozBrowserOnly(aInMozBrowserOnly)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aURI, "Null pointer!");
  NS_ASSERTION(!aGroup.IsEmpty(), "Empty group!");
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

      // Add all the persistent storage files we care about.
      rv = AddToUsage(quotaManager, PERSISTENCE_TYPE_PERSISTENT);
      NS_ENSURE_SUCCESS(rv, rv);

      // Add all the temporary storage files we care about.
      rv = AddToUsage(quotaManager, PERSISTENCE_TYPE_TEMPORARY);
      NS_ENSURE_SUCCESS(rv, rv);

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
        quotaManager->AllowNextSynchronizedOp(mOrigin,
                                              Nullable<PersistenceType>(),
                                              EmptyCString());
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
AsyncUsageRunnable::AddToUsage(QuotaManager* aQuotaManager,
                               PersistenceType aPersistenceType)
{
  AssertIsOnIOThread();

  nsCOMPtr<nsIFile> directory;
  nsresult rv = aQuotaManager->GetDirectoryForOrigin(aPersistenceType, mOrigin,
                                                     getter_AddRefs(directory));
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists;
  rv = directory->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  // If the directory exists then enumerate all the files inside, adding up
  // the sizes to get the final usage statistic.
  if (exists && !mCanceled) {
    bool initialized;

    if (aPersistenceType == PERSISTENCE_TYPE_PERSISTENT) {
      initialized = aQuotaManager->mInitializedOrigins.Contains(mOrigin);

      if (!initialized) {
        rv = MaybeUpgradeOriginDirectory(directory);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
    else {
      NS_ASSERTION(aPersistenceType == PERSISTENCE_TYPE_TEMPORARY, "Huh?");
      initialized = aQuotaManager->mTemporaryStorageInitialized;
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

      if (leafName.EqualsLiteral(METADATA_FILE_NAME) ||
          leafName.EqualsLiteral(DSSTORE_FILE_NAME)) {
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

      nsRefPtr<Client>& client = aQuotaManager->mClients[clientType];

      if (initialized) {
        rv = client->GetUsageForOrigin(aPersistenceType, mGroup, mOrigin, this);
      }
      else {
        rv = client->InitOrigin(aPersistenceType, mGroup, mOrigin, this);
      }
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED1(AsyncUsageRunnable, nsRunnable, nsIQuotaRequest)

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
  if (mCanceled.exchange(1)) {
    NS_WARNING("Canceled more than once?!");
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

nsresult
ResetOrClearRunnable::OnExclusiveAccessAcquired()
{
  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "This should never fail!");

  nsresult rv = quotaManager->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// static
void
ResetOrClearRunnable::InvalidateOpenedStorages(
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
ResetOrClearRunnable::DeleteFiles(QuotaManager* aQuotaManager,
                                  PersistenceType aPersistenceType)
{
  AssertIsOnIOThread();
  NS_ASSERTION(aQuotaManager, "Don't pass me null!");

  nsresult rv;

  nsCOMPtr<nsIFile> directory =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS_VOID(rv);

  rv = directory->InitWithPath(aQuotaManager->GetStoragePath(aPersistenceType));
  NS_ENSURE_SUCCESS_VOID(rv);

  rv = directory->Remove(true);
  if (rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST &&
      rv != NS_ERROR_FILE_NOT_FOUND && NS_FAILED(rv)) {
    // This should never fail if we've closed all storage connections
    // correctly...
    NS_ERROR("Failed to remove directory!");
  }
}

NS_IMPL_ISUPPORTS_INHERITED0(ResetOrClearRunnable, nsRunnable)

NS_IMETHODIMP
ResetOrClearRunnable::Run()
{
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
        quotaManager->AcquireExclusiveAccess(NullCString(), this,
                                             InvalidateOpenedStorages, nullptr);
      NS_ENSURE_SUCCESS(rv, rv);

      return NS_OK;
    }

    case IO: {
      AssertIsOnIOThread();

      AdvanceState();

      if (mClear) {
        DeleteFiles(quotaManager, PERSISTENCE_TYPE_PERSISTENT);
        DeleteFiles(quotaManager, PERSISTENCE_TYPE_TEMPORARY);
      }

      quotaManager->RemoveQuota();
      quotaManager->ResetOrClearCompleted();

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
      quotaManager->AllowNextSynchronizedOp(OriginOrPatternString::FromNull(),
                                            Nullable<PersistenceType>(),
                                            EmptyCString());

      return NS_OK;
    }

    default:
      NS_ERROR("Unknown state value!");
      return NS_ERROR_UNEXPECTED;
  }

  NS_NOTREACHED("Should never get here!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
FinalizeOriginEvictionRunnable::Run()
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

      for (uint32_t index = 0; index < mOrigins.Length(); index++) {
        quotaManager->OriginClearCompleted(
                            PERSISTENCE_TYPE_TEMPORARY,
                            OriginOrPatternString::FromOrigin(mOrigins[index]));
      }

      if (NS_FAILED(NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL))) {
        NS_WARNING("Failed to dispatch to main thread!");
        return NS_ERROR_FAILURE;
      }

      return NS_OK;
    }

    case Complete: {
      NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

      for (uint32_t index = 0; index < mOrigins.Length(); index++) {
        quotaManager->AllowNextSynchronizedOp(
                          OriginOrPatternString::FromOrigin(mOrigins[index]),
                          Nullable<PersistenceType>(PERSISTENCE_TYPE_TEMPORARY),
                          EmptyCString());
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
FinalizeOriginEvictionRunnable::Dispatch()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mCallbackState == Pending, "Huh?");

  mCallbackState = OpenAllowed;
  return NS_DispatchToMainThread(this);
}

nsresult
FinalizeOriginEvictionRunnable::RunImmediately()
{
  AssertIsOnIOThread();
  NS_ASSERTION(mCallbackState == Pending, "Huh?");

  mCallbackState = IO;
  return this->Run();
}

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

NS_IMETHODIMP
WaitForLockedFilesToFinishRunnable::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  mBusy = false;

  return NS_OK;
}

NS_IMETHODIMP
SaveOriginAccessTimeRunnable::Run()
{
  AssertIsOnIOThread();

  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "This should never fail!");

  nsCOMPtr<nsIFile> directory;
  nsresult rv =
    quotaManager->GetDirectoryForOrigin(PERSISTENCE_TYPE_TEMPORARY, mOrigin,
                                        getter_AddRefs(directory));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIBinaryOutputStream> stream;
  rv = GetDirectoryMetadataStream(directory, true, getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  // The origin directory may not exist anymore.
  if (stream) {
    rv = stream->Write64(mTimestamp);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}
