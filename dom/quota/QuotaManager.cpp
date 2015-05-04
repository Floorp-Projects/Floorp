/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsIQuotaRequest.h"
#include "nsIRunnable.h"
#include "nsISimpleEnumerator.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsITimer.h"
#include "nsIURI.h"
#include "nsIUsageCallback.h"
#include "nsPIDOMWindow.h"

#include <algorithm>
#include "GeckoProfiler.h"
#include "mozilla/Atomics.h"
#include "mozilla/CondVar.h"
#include "mozilla/dom/asmjscache/AsmJSCache.h"
#include "mozilla/dom/cache/QuotaClient.h"
#include "mozilla/dom/indexedDB/ActorsParent.h"
#include "mozilla/Mutex.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsComponentManagerUtils.h"
#include "nsAboutProtocolUtils.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"
#include "nsCRTGlue.h"
#include "nsDirectoryServiceUtils.h"
#include "nsEscape.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsScriptSecurityManager.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "xpcpublic.h"

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

// Preference that users can set to override temporary storage smart limit
// calculation.
#define PREF_FIXED_LIMIT "dom.quotaManager.temporaryStorage.fixedLimit"
#define PREF_CHUNK_SIZE "dom.quotaManager.temporaryStorage.chunkSize"

// Preference that is used to enable testing features
#define PREF_TESTING_FEATURES "dom.quotaManager.testing"

// profile-before-change, when we need to shut down quota manager
#define PROFILE_BEFORE_CHANGE_OBSERVER_ID "profile-before-change"

// The name of the file that we use to load/save the last access time of an
// origin.
#define METADATA_FILE_NAME ".metadata"

#define PERMISSION_DEFAUT_PERSISTENT_STORAGE "default-persistent-storage"

#define KB * 1024ULL
#define MB * 1024ULL KB
#define GB * 1024ULL MB

USING_QUOTA_NAMESPACE
using namespace mozilla;
using namespace mozilla::dom;

static_assert(
  static_cast<uint32_t>(StorageType::Persistent) ==
  static_cast<uint32_t>(PERSISTENCE_TYPE_PERSISTENT),
  "Enum values should match.");

static_assert(
  static_cast<uint32_t>(StorageType::Temporary) ==
  static_cast<uint32_t>(PERSISTENCE_TYPE_TEMPORARY),
  "Enum values should match.");

static_assert(
  static_cast<uint32_t>(StorageType::Default) ==
  static_cast<uint32_t>(PERSISTENCE_TYPE_DEFAULT),
  "Enum values should match.");

namespace {

const char kChromeOrigin[] = "chrome";
const char kAboutHomeOrigin[] = "moz-safe-about:home";
const char kIndexedDBOriginPrefix[] = "indexeddb://";

const char kIndexedDBDirectoryName[] = "indexedDB";
const char kStorageDirectoryName[] = "storage";
const char kPersistentDirectoryName[] = "persistent";
const char kPermanentDirectoryName[] = "permanent";
const char kTemporaryDirectoryName[] = "temporary";
const char kDefaultDirectoryName[] = "default";

enum AppId {
  kNoAppId = nsIScriptSecurityManager::NO_APP_ID,
  kUnknownAppId = nsIScriptSecurityManager::UNKNOWN_APP_ID
};

} // anonymous namespace

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
  nsCOMPtr<nsIRunnable> mRunnable;
  nsTArray<nsCOMPtr<nsIRunnable> > mDelayedRunnables;
};

class CollectOriginsHelper final : public nsRunnable
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
class OriginClearRunnable final : public nsRunnable
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

  OriginClearRunnable(const OriginOrPatternString& aOriginOrPattern,
                      Nullable<PersistenceType> aPersistenceType)
  : mOriginOrPattern(aOriginOrPattern),
    mPersistenceType(aPersistenceType),
    mCallbackState(Pending)
  { }

  NS_IMETHOD
  Run() override;

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

  void
  DeleteFiles(QuotaManager* aQuotaManager,
              PersistenceType aPersistenceType);

private:
  ~OriginClearRunnable() {}

  OriginOrPatternString mOriginOrPattern;
  Nullable<PersistenceType> mPersistenceType;
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
class AsyncUsageRunnable final : public UsageInfo,
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
                     bool aIsApp,
                     nsIURI* aURI,
                     nsIUsageCallback* aCallback);

  NS_IMETHOD
  Run() override;

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
  ~AsyncUsageRunnable() {}

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
  const bool mIsApp;
};

class ResetOrClearRunnable final : public nsRunnable
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

  explicit ResetOrClearRunnable(bool aClear)
  : mCallbackState(Pending),
    mClear(aClear)
  { }

  NS_IMETHOD
  Run() override;

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

  void
  DeleteFiles(QuotaManager* aQuotaManager);

private:
  ~ResetOrClearRunnable() {}

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
class FinalizeOriginEvictionRunnable final : public nsRunnable
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
  explicit FinalizeOriginEvictionRunnable(nsTArray<OriginParams>& aOrigins)
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
        MOZ_ASSERT_UNREACHABLE("Can't advance past Complete!");
    }
  }

  nsresult
  Dispatch();

  nsresult
  RunImmediately();

private:
  CallbackState mCallbackState;
  nsTArray<OriginParams> mOrigins;
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

void
ReportInternalError(const char* aFile, uint32_t aLine, const char* aStr)
{
  // Get leaf of file path
  for (const char* p = aFile; *p; ++p) {
    if (*p == '/' && *(p + 1)) {
      aFile = p + 1;
    }
  }

  nsContentUtils::LogSimpleConsoleError(
    NS_ConvertUTF8toUTF16(nsPrintfCString(
                          "Quota %s: %s:%lu", aStr, aFile, aLine)),
    "quota");
}

END_QUOTA_NAMESPACE

namespace {

QuotaManager* gInstance = nullptr;
mozilla::Atomic<bool> gShutdown(false);

// Constants for temporary storage limit computing.
static const int32_t kDefaultFixedLimitKB = -1;
static const uint32_t kDefaultChunkSizeKB = 10 * 1024;
int32_t gFixedLimitKB = kDefaultFixedLimitKB;
uint32_t gChunkSizeKB = kDefaultChunkSizeKB;

bool gTestingEnabled = false;

// A callback runnable used by the TransactionPool when it's safe to proceed
// with a SetVersion/DeleteDatabase/etc.
class WaitForTransactionsToFinishRunnable final : public nsRunnable
{
public:
  explicit WaitForTransactionsToFinishRunnable(SynchronizedOp* aOp)
  : mOp(aOp), mCountdown(1)
  {
    NS_ASSERTION(mOp, "Why don't we have a runnable?");
    NS_ASSERTION(mOp->mRunnable,
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

class SaveOriginAccessTimeRunnable final : public nsRunnable
{
public:
  SaveOriginAccessTimeRunnable(PersistenceType aPersistenceType,
                               const nsACString& aOrigin,
                               int64_t aTimestamp)
  : mPersistenceType(aPersistenceType), mOrigin(aOrigin), mTimestamp(aTimestamp)
  { }

  NS_IMETHOD
  Run();

private:
  PersistenceType mPersistenceType;
  nsCString mOrigin;
  int64_t mTimestamp;
};

class StorageDirectoryHelper final
  : public nsRunnable
{
  struct OriginProps;

  nsTArray<OriginProps> mOriginProps;

  nsCOMPtr<nsIFile> mDirectory;
  mozilla::Mutex mMutex;
  mozilla::CondVar mCondVar;
  nsresult mMainThreadResultCode;
  bool mPersistent;
  bool mWaiting;

public:
  StorageDirectoryHelper(nsIFile* aDirectory,
                         bool aPersistent)
    : mDirectory(aDirectory)
    , mMutex("StorageDirectoryHelper::mMutex")
    , mCondVar(mMutex, "StorageDirectoryHelper::mCondVar")
    , mMainThreadResultCode(NS_OK)
    , mPersistent(aPersistent)
    , mWaiting(true)
  {
    AssertIsOnIOThread();
  }

  nsresult
  CreateOrUpgradeMetadataFiles();

private:
  ~StorageDirectoryHelper()
  { }

  nsresult
  RunOnMainThread();

  NS_IMETHOD
  Run();
};

struct StorageDirectoryHelper::OriginProps
{
  enum Type
  {
    eChrome,
    eContent
  };

  nsCOMPtr<nsIFile> mDirectory;
  nsCString mSpec;
  uint32_t mAppId;
  int64_t mTimestamp;
  nsCString mGroup;
  nsCString mOrigin;

  Type mType;
  bool mInMozBrowser;
  bool mIsApp;

public:
  explicit OriginProps()
    : mAppId(kNoAppId)
    , mTimestamp(0)
    , mType(eContent)
    , mInMozBrowser(false)
    , mIsApp(false)
  { }
};

class MOZ_STACK_CLASS OriginParser final
{
  static bool
  IgnoreWhitespace(char16_t /* aChar */)
  {
    return false;
  }

  typedef nsCCharSeparatedTokenizerTemplate<IgnoreWhitespace> Tokenizer;

  enum SchemaType {
    eNone,
    eFile,
    eMozSafeAbout
  };

  enum State {
    eExpectingAppIdOrSchema,
    eExpectingInMozBrowser,
    eExpectingSchema,
    eExpectingEmptyToken1,
    eExpectingEmptyToken2,
    eExpectingEmptyToken3,
    eExpectingHost,
    eExpectingPort,
    eExpectingDriveLetterOrPathnameComponent,
    eExpectingEmptyTokenOrPathnameComponent,
    eExpectingPathnameComponent,
    eComplete,
    eHandledTrailingSeparator
  };

  const nsCString mOrigin;
  Tokenizer mTokenizer;

  uint32_t mAppId;
  nsCString mSchema;
  nsCString mHost;
  Nullable<uint32_t> mPort;
  nsTArray<nsCString> mPathnameComponents;
  nsCString mHandledTokens;

  SchemaType mSchemaType;
  State mState;
  bool mInMozBrowser;
  bool mMaybeDriveLetter;
  bool mError;

public:
  explicit OriginParser(const nsACString& aOrigin)
    : mOrigin(aOrigin)
    , mTokenizer(aOrigin, '+')
    , mAppId(kNoAppId)
    , mPort()
    , mSchemaType(eNone)
    , mState(eExpectingAppIdOrSchema)
    , mInMozBrowser(false)
    , mMaybeDriveLetter(false)
    , mError(false)
  { }

  static bool
  ParseOrigin(const nsACString& aOrigin,
              uint32_t* aAppId,
              bool* aInMozBrowser,
              nsCString& aSpec);

  bool
  Parse(uint32_t* aAppId,
        bool* aInMozBrowser,
        nsACString& aSpec);

private:
  void
  HandleSchema(const nsDependentCSubstring& aSchema);

  void
  HandlePathnameComponent(const nsDependentCSubstring& aSchema);

  void
  HandleToken(const nsDependentCSubstring& aToken);

  void
  HandleTrailingSeparator();
};

class OriginKey : public nsAutoCString
{
public:
  OriginKey(PersistenceType aPersistenceType,
            const nsACString& aOrigin)
  {
    PersistenceTypeToText(aPersistenceType, *this);
    Append(':');
    Append(aOrigin);
  }
};

struct MOZ_STACK_CLASS InactiveOriginsInfo
{
  InactiveOriginsInfo(OriginCollection& aPersistentCollection,
                      OriginCollection& aTemporaryCollection,
                      nsTArray<OriginInfo*>& aOrigins)
  : persistentCollection(aPersistentCollection),
    temporaryCollection(aTemporaryCollection),
    origins(aOrigins)
  { }

  OriginCollection& persistentCollection;
  OriginCollection& temporaryCollection;
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

bool
IsTreatedAsPersistent(PersistenceType aPersistenceType,
                      bool aIsApp)
{
  if (aPersistenceType == PERSISTENCE_TYPE_PERSISTENT ||
      (aPersistenceType == PERSISTENCE_TYPE_DEFAULT && aIsApp)) {
    return true;
  }

  return false;
}

bool
IsTreatedAsTemporary(PersistenceType aPersistenceType,
                     bool aIsApp)
{
  return !IsTreatedAsPersistent(aPersistenceType, aIsApp);
}

nsresult
CloneStoragePath(nsIFile* aBaseDir,
                 const nsACString& aStorageName,
                 nsAString& aStoragePath)
{
  nsresult rv;

  nsCOMPtr<nsIFile> storageDir;
  rv = aBaseDir->Clone(getter_AddRefs(storageDir));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  NS_ConvertASCIItoUTF16 dirName(aStorageName);
  rv = storageDir->Append(dirName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = storageDir->GetPath(aStoragePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
GetLastModifiedTime(nsIFile* aFile, int64_t* aTimestamp)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aFile);
  MOZ_ASSERT(aTimestamp);

  class MOZ_STACK_CLASS Helper final
  {
  public:
    static nsresult
    GetLastModifiedTime(nsIFile* aFile, int64_t* aTimestamp)
    {
      AssertIsOnIOThread();
      MOZ_ASSERT(aFile);
      MOZ_ASSERT(aTimestamp);

      bool isDirectory;
      nsresult rv = aFile->IsDirectory(&isDirectory);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (!isDirectory) {
        nsString leafName;
        rv = aFile->GetLeafName(leafName);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        if (leafName.EqualsLiteral(METADATA_FILE_NAME) ||
            leafName.EqualsLiteral(DSSTORE_FILE_NAME)) {
          return NS_OK;
        }

        int64_t timestamp;
        rv = aFile->GetLastModifiedTime(&timestamp);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        // Need to convert from milliseconds to microseconds.
        MOZ_ASSERT((INT64_MAX / PR_USEC_PER_MSEC) > timestamp);
        timestamp *= int64_t(PR_USEC_PER_MSEC);

        if (timestamp > *aTimestamp) {
          *aTimestamp = timestamp;
        }
        return NS_OK;
      }

      nsCOMPtr<nsISimpleEnumerator> entries;
      rv = aFile->GetDirectoryEntries(getter_AddRefs(entries));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      bool hasMore;
      while (NS_SUCCEEDED((rv = entries->HasMoreElements(&hasMore))) && hasMore) {
        nsCOMPtr<nsISupports> entry;
        rv = entries->GetNext(getter_AddRefs(entry));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        nsCOMPtr<nsIFile> file = do_QueryInterface(entry);
        MOZ_ASSERT(file);

        rv = GetLastModifiedTime(file, aTimestamp);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      return NS_OK;
    }
  };

  int64_t timestamp = INT64_MIN;
  nsresult rv = Helper::GetLastModifiedTime(aFile, &timestamp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aTimestamp = timestamp;
  return NS_OK;
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

enum FileFlag {
  kTruncateFileFlag,
  kUpdateFileFlag,
  kAppendFileFlag
};

nsresult
GetDirectoryMetadataOutputStream(nsIFile* aDirectory, FileFlag aFileFlag,
                                 nsIBinaryOutputStream** aStream)
{
  AssertIsOnIOThread();

  nsCOMPtr<nsIFile> metadataFile;
  nsresult rv = aDirectory->Clone(getter_AddRefs(metadataFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = metadataFile->Append(NS_LITERAL_STRING(METADATA_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIOutputStream> outputStream;
  switch (aFileFlag) {
    case kTruncateFileFlag: {
      rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream),
                                       metadataFile);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      break;
    }

    case kUpdateFileFlag: {
      bool exists;
      rv = metadataFile->Exists(&exists);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (!exists) {
        *aStream = nullptr;
        return NS_OK;
      }

      nsCOMPtr<nsIFileStream> stream;
      rv = NS_NewLocalFileStream(getter_AddRefs(stream), metadataFile);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      outputStream = do_QueryInterface(stream);
      if (NS_WARN_IF(!outputStream)) {
        return NS_ERROR_FAILURE;
      }

      break;
    }

    case kAppendFileFlag: {
      rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream),
                                       metadataFile,
                                       PR_WRONLY | PR_CREATE_FILE | PR_APPEND);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  nsCOMPtr<nsIBinaryOutputStream> binaryStream =
    do_CreateInstance("@mozilla.org/binaryoutputstream;1");
  if (NS_WARN_IF(!binaryStream)) {
    return NS_ERROR_FAILURE;
  }

  rv = binaryStream->SetOutputStream(outputStream);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  binaryStream.forget(aStream);
  return NS_OK;
}

nsresult
CreateDirectoryMetadata(nsIFile* aDirectory, int64_t aTimestamp,
                        const nsACString& aGroup, const nsACString& aOrigin,
                        bool aIsApp)
{
  AssertIsOnIOThread();

  nsCOMPtr<nsIBinaryOutputStream> stream;
  nsresult rv =
    GetDirectoryMetadataOutputStream(aDirectory, kTruncateFileFlag,
                                     getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(stream, "This shouldn't be null!");

  rv = stream->Write64(aTimestamp);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stream->WriteStringZ(PromiseFlatCString(aGroup).get());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stream->WriteStringZ(PromiseFlatCString(aOrigin).get());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stream->WriteBoolean(aIsApp);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
GetDirectoryMetadataInputStream(nsIFile* aDirectory,
                                nsIBinaryInputStream** aStream)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(aStream);

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

  binaryStream.forget(aStream);
  return NS_OK;
}

nsresult
GetDirectoryMetadata(nsIFile* aDirectory,
                     int64_t* aTimestamp,
                     nsACString& aGroup,
                     nsACString& aOrigin,
                     bool* aIsApp)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(aTimestamp);

  nsCOMPtr<nsIBinaryInputStream> binaryStream;
  nsresult rv =
    GetDirectoryMetadataInputStream(aDirectory, getter_AddRefs(binaryStream));
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

  bool isApp;
  if (aIsApp) {
    rv = binaryStream->ReadBoolean(&isApp);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  *aTimestamp = timestamp;
  aGroup = group;
  aOrigin = origin;
  if (aIsApp) {
    *aIsApp = isApp;
  }
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
// drive and on given temporary storage usage (also in bytes).
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

  // Grow/shrink in gChunkSizeKB units, deliberately, so that in the common case
  // we don't shrink temporary storage and evict origin data every time we
  // initialize.
  availableKB = (availableKB / gChunkSizeKB) * gChunkSizeKB;

  // Allow temporary storage to consume up to half the available space.
  uint64_t resultKB = availableKB * .50;

  *aLimit = resultKB * 1024;
  return NS_OK;
}

} // anonymous namespace

QuotaManager::QuotaManager()
: mQuotaMutex("QuotaManager.mQuotaMutex"),
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
  return gShutdown;
}

nsresult
QuotaManager::Init()
{
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

    rv = CloneStoragePath(baseDir,
                          NS_LITERAL_CSTRING(kIndexedDBDirectoryName),
                          mIndexedDBPath);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ConvertASCIItoUTF16 dirName(NS_LITERAL_CSTRING(kStorageDirectoryName));
    rv = baseDir->Append(dirName);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = baseDir->GetPath(mStoragePath);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = CloneStoragePath(baseDir,
                          NS_LITERAL_CSTRING(kPermanentDirectoryName),
                          mPermanentStoragePath);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = CloneStoragePath(baseDir,
                          NS_LITERAL_CSTRING(kTemporaryDirectoryName),
                          mTemporaryStoragePath);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = CloneStoragePath(baseDir,
                          NS_LITERAL_CSTRING(kDefaultDirectoryName),
                          mDefaultStoragePath);
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

  if (NS_FAILED(Preferences::AddIntVarCache(&gFixedLimitKB, PREF_FIXED_LIMIT,
                                            kDefaultFixedLimitKB)) ||
      NS_FAILED(Preferences::AddUintVarCache(&gChunkSizeKB,
                                             PREF_CHUNK_SIZE,
                                             kDefaultChunkSizeKB))) {
    NS_WARNING("Unable to respond to temp storage pref changes!");
  }

  if (NS_FAILED(Preferences::AddBoolVarCache(&gTestingEnabled,
                                             PREF_TESTING_FEATURES, false))) {
    NS_WARNING("Unable to respond to testing pref changes!");
  }

  static_assert(Client::IDB == 0 && Client::ASMJS == 1 && Client::DOMCACHE == 2 &&
                Client::TYPE_MAX == 3, "Fix the registration!");

  NS_ASSERTION(mClients.Capacity() == Client::TYPE_MAX,
               "Should be using an auto array with correct capacity!");

  nsRefPtr<Client> idbClient = indexedDB::CreateQuotaClient();

  // Register clients.
  mClients.AppendElement(idbClient);
  mClients.AppendElement(asmjscache::CreateClient());
  mClients.AppendElement(cache::CreateQuotaClient());

  return NS_OK;
}

void
QuotaManager::InitQuotaForOrigin(PersistenceType aPersistenceType,
                                 const nsACString& aGroup,
                                 const nsACString& aOrigin,
                                 bool aIsApp,
                                 uint64_t aUsageBytes,
                                 int64_t aAccessTime)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(IsTreatedAsTemporary(aPersistenceType, aIsApp));

  MutexAutoLock lock(mQuotaMutex);

  GroupInfoPair* pair;
  if (!mGroupInfoPairs.Get(aGroup, &pair)) {
    pair = new GroupInfoPair();
    mGroupInfoPairs.Put(aGroup, pair);
    // The hashtable is now responsible to delete the GroupInfoPair.
  }

  nsRefPtr<GroupInfo> groupInfo = pair->LockedGetGroupInfo(aPersistenceType);
  if (!groupInfo) {
    groupInfo = new GroupInfo(pair, aPersistenceType, aGroup);
    pair->LockedSetGroupInfo(aPersistenceType, groupInfo);
  }

  nsRefPtr<OriginInfo> originInfo =
    new OriginInfo(groupInfo, aOrigin, aIsApp, aUsageBytes, aAccessTime);
  groupInfo->LockedAddOriginInfo(originInfo);
}

void
QuotaManager::DecreaseUsageForOrigin(PersistenceType aPersistenceType,
                                     const nsACString& aGroup,
                                     const nsACString& aOrigin,
                                     int64_t aSize)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

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
  MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

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
    int64_t timestamp = PR_Now();
    originInfo->LockedUpdateAccessTime(timestamp);

    MutexAutoUnlock autoUnlock(mQuotaMutex);

    SaveOriginAccessTime(aPersistenceType, aOrigin, timestamp);
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

  groupInfo = aValue->LockedGetGroupInfo(PERSISTENCE_TYPE_DEFAULT);
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

already_AddRefed<QuotaObject>
QuotaManager::GetQuotaObject(PersistenceType aPersistenceType,
                             const nsACString& aGroup,
                             const nsACString& aOrigin,
                             nsIFile* aFile)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (aPersistenceType == PERSISTENCE_TYPE_PERSISTENT) {
    return nullptr;
  }

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

  // Re-escape our parameters above to make sure we get the right quota group.
  nsAutoCString tempStorage1;
  const nsCSubstring& group = NS_EscapeURL(aGroup, esc_Query, tempStorage1);

  nsAutoCString tempStorage2;
  const nsCSubstring& origin = NS_EscapeURL(aOrigin, esc_Query, tempStorage2);

  nsRefPtr<QuotaObject> result;
  {
    MutexAutoLock lock(mQuotaMutex);

    GroupInfoPair* pair;
    if (!mGroupInfoPairs.Get(group, &pair)) {
      return nullptr;
    }

    nsRefPtr<GroupInfo> groupInfo =
      pair->LockedGetGroupInfo(aPersistenceType);

    if (!groupInfo) {
      return nullptr;
    }

    nsRefPtr<OriginInfo> originInfo = groupInfo->LockedGetOriginInfo(origin);

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
  }
  (*cluster)[aStorage->GetClient()->GetType()].AppendElement(aStorage);

  if (aStorage->Type() != PERSISTENCE_TYPE_PERSISTENT) {
    LiveStorageTable& liveStorageTable = GetLiveStorageTable(aStorage->Type());

    nsTArray<nsIOfflineStorage*>* array;
    if (!liveStorageTable.Get(origin, &array)) {
      array = new nsTArray<nsIOfflineStorage*>();
      liveStorageTable.Put(origin, array);

      UpdateOriginAccessTime(aStorage->Type(), aStorage->Group(), origin);
    }
    array->AppendElement(aStorage);
  }

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
  MOZ_ALWAYS_TRUE(mLiveStorages.Get(origin, &cluster));

  MOZ_ALWAYS_TRUE(
    (*cluster)[aStorage->GetClient()->GetType()].RemoveElement(aStorage));
  if (cluster->IsEmpty()) {
    mLiveStorages.Remove(origin);
  }

  if (aStorage->Type() != PERSISTENCE_TYPE_PERSISTENT) {
    LiveStorageTable& liveStorageTable = GetLiveStorageTable(aStorage->Type());

    nsTArray<nsIOfflineStorage*>* array;
    MOZ_ALWAYS_TRUE(liveStorageTable.Get(origin, &array));

    MOZ_ALWAYS_TRUE(array->RemoveElement(aStorage));
    if (array->IsEmpty()) {
      liveStorageTable.Remove(origin);

      UpdateOriginAccessTime(aStorage->Type(), aStorage->Group(), origin);
    }
  }
}

void
QuotaManager::AbortCloseStoragesForProcess(ContentParent* aContentParent)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aContentParent);

  // FileHandle API is not yet supported in child processes, so we don't have
  // to worry about aborting file handles for given child process.

  StorageMatcher<ArrayCluster<nsIOfflineStorage*>> liveStorages;
  liveStorages.Find(mLiveStorages);

  for (uint32_t i = 0; i < Client::TYPE_MAX; i++) {
    nsTArray<nsIOfflineStorage*>& array = liveStorages[i];
    for (uint32_t j = 0; j < array.Length(); j++) {
      nsCOMPtr<nsIOfflineStorage> storage = array[j];

      if (storage->IsOwnedByProcess(aContentParent)) {
        if (NS_FAILED(storage->Close())) {
          NS_WARNING("Failed to close storage for dying process!");
        }
      }
    }
  }
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
QuotaManager::InitializeRepository(PersistenceType aPersistenceType)
{
  nsresult rv;

  nsCOMPtr<nsIFile> directory =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = directory->InitWithPath(GetStoragePath(aPersistenceType));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool created;
  rv = EnsureDirectory(directory, &created);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsISimpleEnumerator> entries;
  rv = directory->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool hasMore;
  while (NS_SUCCEEDED((rv = entries->HasMoreElements(&hasMore))) && hasMore) {
    nsCOMPtr<nsISupports> entry;
    rv = entries->GetNext(getter_AddRefs(entry));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCOMPtr<nsIFile> childDirectory = do_QueryInterface(entry);
    MOZ_ASSERT(childDirectory);

    bool isDirectory;
    rv = childDirectory->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!isDirectory) {
      nsString leafName;
      rv = childDirectory->GetLeafName(leafName);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (leafName.EqualsLiteral(METADATA_FILE_NAME) ||
          leafName.EqualsLiteral(DSSTORE_FILE_NAME)) {
        continue;
      }

      QM_WARNING("Something (%s) in the repository that doesn't belong!",
                 NS_ConvertUTF16toUTF8(leafName).get());
      return NS_ERROR_UNEXPECTED;
    }

    int64_t timestamp;
    nsCString group;
    nsCString origin;
    bool isApp;
    rv = GetDirectoryMetadata(childDirectory, &timestamp, group, origin,
                              &isApp);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (IsTreatedAsPersistent(aPersistenceType, isApp)) {
      continue;
    }

    rv = InitializeOrigin(aPersistenceType, group, origin, isApp, timestamp,
                          childDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
QuotaManager::InitializeOrigin(PersistenceType aPersistenceType,
                               const nsACString& aGroup,
                               const nsACString& aOrigin,
                               bool aIsApp,
                               int64_t aAccessTime,
                               nsIFile* aDirectory)
{
  AssertIsOnIOThread();

  nsresult rv;

  if (aPersistenceType == PERSISTENCE_TYPE_PERSISTENT) {
    rv = MaybeUpgradeOriginDirectory(aDirectory);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  bool trackQuota = IsQuotaEnforced(aPersistenceType, aOrigin, aIsApp);

  // We need to initialize directories of all clients if they exists and also
  // get the total usage to initialize the quota.
  nsAutoPtr<UsageInfo> usageInfo;
  if (trackQuota) {
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

  if (trackQuota) {
    InitQuotaForOrigin(aPersistenceType, aGroup, aOrigin, aIsApp,
                       usageInfo->TotalUsage(), aAccessTime);
  }

  return NS_OK;
}

nsresult
QuotaManager::MaybeUpgradeIndexedDBDirectory()
{
  AssertIsOnIOThread();

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

  rv = persistentStorageDir->InitWithPath(mStoragePath);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ConvertASCIItoUTF16 dirName(NS_LITERAL_CSTRING(kPersistentDirectoryName));
  rv = persistentStorageDir->Append(dirName);
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

  // MoveTo() is atomic if the move happens on the same volume which should
  // be our case, so even if we crash in the middle of the operation nothing
  // breaks next time we try to initialize.
  // However there's a theoretical possibility that the indexedDB directory
  // is on different volume, but it should be rare enough that we don't have
  // to worry about it.
  rv = indexedDBDir->MoveTo(storageDir, dirName);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
QuotaManager::MaybeUpgradePersistentStorageDirectory()
{
  AssertIsOnIOThread();

  nsresult rv;

  nsCOMPtr<nsIFile> persistentStorageDir =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = persistentStorageDir->InitWithPath(mStoragePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  NS_ConvertASCIItoUTF16 dirName(NS_LITERAL_CSTRING(kPersistentDirectoryName));
  rv = persistentStorageDir->Append(dirName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = persistentStorageDir->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    // Nothing to upgrade.
    return NS_OK;
  }

  bool isDirectory;
  rv = persistentStorageDir->IsDirectory(&isDirectory);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!isDirectory) {
    NS_WARNING("persistent entry is not a directory!");
    return NS_OK;
  }

  nsCOMPtr<nsIFile> defaultStorageDir =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = defaultStorageDir->InitWithPath(mDefaultStoragePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = defaultStorageDir->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    NS_WARNING("storage/persistent shouldn't exist after the upgrade!");
    return NS_OK;
  }

  // Create real metadata files for origin directories in persistent storage.
  nsRefPtr<StorageDirectoryHelper> helper =
    new StorageDirectoryHelper(persistentStorageDir, /* aPersistent */ true);

  rv = helper->CreateOrUpgradeMetadataFiles();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Upgrade metadata files for origin directories in temporary storage.
  nsCOMPtr<nsIFile> temporaryStorageDir =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = temporaryStorageDir->InitWithPath(mTemporaryStoragePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = temporaryStorageDir->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    rv = temporaryStorageDir->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!isDirectory) {
      NS_WARNING("temporary entry is not a directory!");
      return NS_OK;
    }

    helper = new StorageDirectoryHelper(temporaryStorageDir,
                                        /* aPersistent */ false);

    rv = helper->CreateOrUpgradeMetadataFiles();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // And finally rename persistent to default.
  NS_ConvertASCIItoUTF16 defDirName(NS_LITERAL_CSTRING(kDefaultDirectoryName));
  rv = persistentStorageDir->RenameTo(nullptr, defDirName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
QuotaManager::MaybeUpgradeStorageArea()
{
  AssertIsOnIOThread();

  if (mStorageAreaInitialized) {
    return NS_OK;
  }

  nsresult rv = MaybeUpgradeIndexedDBDirectory();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = MaybeUpgradePersistentStorageDirectory();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mStorageAreaInitialized = true;

  return NS_OK;
}

nsresult
QuotaManager::EnsureOriginIsInitialized(PersistenceType aPersistenceType,
                                        const nsACString& aGroup,
                                        const nsACString& aOrigin,
                                        bool aIsApp,
                                        nsIFile** aDirectory)
{
  AssertIsOnIOThread();

  nsresult rv = MaybeUpgradeStorageArea();
  NS_ENSURE_SUCCESS(rv, rv);

  // Get directory for this origin and persistence type.
  nsCOMPtr<nsIFile> directory;
  rv = GetDirectoryForOrigin(aPersistenceType, aOrigin,
                             getter_AddRefs(directory));
  NS_ENSURE_SUCCESS(rv, rv);

  if (IsTreatedAsPersistent(aPersistenceType, aIsApp)) {
    if (mInitializedOrigins.Contains(OriginKey(aPersistenceType, aOrigin))) {
      directory.forget(aDirectory);
      return NS_OK;
    }
  } else if (!mTemporaryStorageInitialized) {
    rv = InitializeRepository(aPersistenceType);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      // We have to cleanup partially initialized quota.
      RemoveQuota();

      return rv;
    }

    rv = InitializeRepository(ComplementaryPersistenceType(aPersistenceType));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      // We have to cleanup partially initialized quota.
      RemoveQuota();

      return rv;
    }

    if (gFixedLimitKB >= 0) {
      mTemporaryStorageLimit = gFixedLimitKB * 1024;
    }
    else {
      nsCOMPtr<nsIFile> storageDir =
        do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = storageDir->InitWithPath(GetStoragePath());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = GetTemporaryStorageLimit(storageDir, mTemporaryStorageUsage,
                                    &mTemporaryStorageLimit);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    mTemporaryStorageInitialized = true;

    CheckTemporaryStorageLimits();
  }

  int64_t timestamp;

  bool created;
  rv = EnsureDirectory(directory, &created);
  NS_ENSURE_SUCCESS(rv, rv);

  if (IsTreatedAsPersistent(aPersistenceType, aIsApp)) {
    if (created) {
      timestamp = PR_Now();

      rv = CreateDirectoryMetadata(directory, timestamp, aGroup, aOrigin,
                                   aIsApp);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      nsCOMPtr<nsIBinaryInputStream> stream;
      rv = GetDirectoryMetadataInputStream(directory, getter_AddRefs(stream));
      NS_ENSURE_SUCCESS(rv, rv);

      uint64_t ts;
      rv = stream->Read64(&ts);
      NS_ENSURE_SUCCESS(rv, rv);

      timestamp = ts;

      MOZ_ASSERT(timestamp <= PR_Now());
    }

    rv = InitializeOrigin(aPersistenceType, aGroup, aOrigin, aIsApp, timestamp,
                          directory);
    NS_ENSURE_SUCCESS(rv, rv);

    mInitializedOrigins.AppendElement(OriginKey(aPersistenceType, aOrigin));
  } else if (created) {
    timestamp = PR_Now();

    rv = CreateDirectoryMetadata(directory, timestamp, aGroup, aOrigin,
                                 aIsApp);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = InitializeOrigin(aPersistenceType, aGroup, aOrigin, aIsApp, timestamp,
                          directory);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  directory.forget(aDirectory);
  return NS_OK;
}

void
QuotaManager::OriginClearCompleted(PersistenceType aPersistenceType,
                                   const nsACString& aOrigin,
                                   bool aIsApp)
{
  AssertIsOnIOThread();

  if (IsTreatedAsPersistent(aPersistenceType, aIsApp)) {
    mInitializedOrigins.RemoveElement(OriginKey(aPersistenceType, aOrigin));
  }

  for (uint32_t index = 0; index < Client::TYPE_MAX; index++) {
    mClients[index]->OnOriginClearCompleted(aPersistenceType, aOrigin);
  }
}

void
QuotaManager::ResetOrClearCompleted()
{
  AssertIsOnIOThread();

  mInitializedOrigins.Clear();
  mTemporaryStorageInitialized = false;
  mStorageAreaInitialized = false;

  ReleaseIOThreadObjects();
}

already_AddRefed<mozilla::dom::quota::Client>
QuotaManager::GetClient(Client::Type aClientType)
{
  nsRefPtr<Client> client = mClients.SafeElementAt(aClientType);
  return client.forget();
}

uint64_t
QuotaManager::GetGroupLimit() const
{
  MOZ_ASSERT(mTemporaryStorageInitialized);

  // To avoid one group evicting all the rest, limit the amount any one group
  // can use to 20%. To prevent individual sites from using exorbitant amounts
  // of storage where there is a lot of free space, cap the group limit to 2GB.
  uint64_t x = std::min<uint64_t>(mTemporaryStorageLimit * .20, 2 GB);

  // In low-storage situations, make an exception (while not exceeding the total
  // storage limit).
  return std::min<uint64_t>(mTemporaryStorageLimit,
                            std::max<uint64_t>(x, 10 MB));
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
                             nsACString* aOrigin,
                             bool* aIsApp)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aURI);

  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  NS_ENSURE_TRUE(secMan, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv = secMan->GetAppCodebasePrincipal(aURI, aAppId, aInMozBrowser,
                                                getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = GetInfoFromPrincipal(principal, aGroup, aOrigin, aIsApp);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

static nsresult
TryGetInfoForAboutURI(nsIPrincipal* aPrincipal,
                      nsACString& aGroup,
                      nsACString& aASCIIOrigin,
                      bool* aIsApp)
{
  NS_ASSERTION(aPrincipal, "Don't hand me a null principal!");

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aPrincipal->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!uri) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  bool isAbout;
  rv = uri->SchemeIs("about", &isAbout);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!isAbout) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIAboutModule> module;
  rv = NS_GetAboutModule(uri, getter_AddRefs(module));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> inner = NS_GetInnermostURI(uri);
  NS_ENSURE_TRUE(inner, NS_ERROR_FAILURE);

  nsAutoString postfix;
  rv = module->GetIndexedDBOriginPostfix(uri, postfix);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString origin;
  if (DOMStringIsNull(postfix)) {
    rv = inner->GetSpec(origin);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    nsAutoCString scheme;
    rv = inner->GetScheme(scheme);
    NS_ENSURE_SUCCESS(rv, rv);

    origin = scheme + NS_LITERAL_CSTRING(":") + NS_ConvertUTF16toUTF8(postfix);
  }

  ToLowerCase(origin);
  aGroup.Assign(origin);
  aASCIIOrigin.Assign(origin);

  if (aIsApp) {
    *aIsApp = false;
  }

  return NS_OK;
}

// static
nsresult
QuotaManager::GetInfoFromPrincipal(nsIPrincipal* aPrincipal,
                                   nsACString* aGroup,
                                   nsACString* aOrigin,
                                   bool* aIsApp)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  if (aGroup && aOrigin) {
    nsresult rv =
      TryGetInfoForAboutURI(aPrincipal, *aGroup, *aOrigin, aIsApp);
    if (NS_SUCCEEDED(rv)) {
      return NS_OK;
    }
  }

  if (nsContentUtils::IsSystemPrincipal(aPrincipal)) {
    GetInfoForChrome(aGroup, aOrigin, aIsApp);
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

  if (origin.EqualsLiteral(kChromeOrigin)) {
    NS_WARNING("Non-chrome principal can't use chrome origin!");
    return NS_ERROR_FAILURE;
  }

  nsCString jarPrefix;
  if (aGroup || aOrigin) {
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

  if (aOrigin) {
    aOrigin->Assign(jarPrefix + origin);
  }

  if (aIsApp) {
    *aIsApp = aPrincipal->GetAppStatus() !=
                nsIPrincipal::APP_STATUS_NOT_INSTALLED;
  }

  return NS_OK;
}

// static
nsresult
QuotaManager::GetInfoFromWindow(nsPIDOMWindow* aWindow,
                                nsACString* aGroup,
                                nsACString* aOrigin,
                                bool* aIsApp)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aWindow);
  NS_ENSURE_TRUE(sop, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPrincipal> principal = sop->GetPrincipal();
  NS_ENSURE_TRUE(principal, NS_ERROR_FAILURE);

  nsresult rv = GetInfoFromPrincipal(principal, aGroup, aOrigin, aIsApp);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// static
void
QuotaManager::GetInfoForChrome(nsACString* aGroup,
                               nsACString* aOrigin,
                               bool* aIsApp)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  if (aGroup) {
    ChromeOrigin(*aGroup);
  }
  if (aOrigin) {
    ChromeOrigin(*aOrigin);
  }
  if (aIsApp) {
    *aIsApp = false;
  }
}

// static
bool
QuotaManager::IsOriginWhitelistedForPersistentStorage(const nsACString& aOrigin)
{
  // The first prompt and quota tracking is not required for these origins in
  // persistent storage.
  if (aOrigin.EqualsLiteral(kChromeOrigin) ||
      aOrigin.EqualsLiteral(kAboutHomeOrigin) ||
      StringBeginsWith(aOrigin, nsDependentCString(kIndexedDBOriginPrefix))) {
    return true;
  }

  return false;
}

// static
bool
QuotaManager::IsFirstPromptRequired(PersistenceType aPersistenceType,
                                    const nsACString& aOrigin,
                                    bool aIsApp)
{
  if (IsTreatedAsTemporary(aPersistenceType, aIsApp)) {
    return false;
  }

  return !IsOriginWhitelistedForPersistentStorage(aOrigin);
}

// static
bool
QuotaManager::IsQuotaEnforced(PersistenceType aPersistenceType,
                              const nsACString& aOrigin,
                              bool aIsApp)
{
  return IsTreatedAsTemporary(aPersistenceType, aIsApp);
}

// static
void
QuotaManager::ChromeOrigin(nsACString& aOrigin)
{
  aOrigin.AssignLiteral(kChromeOrigin);
}

NS_IMPL_ISUPPORTS(QuotaManager, nsIQuotaManager, nsIObserver)

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
  bool isApp;
  nsresult rv = GetInfoFromURI(aURI, aAppId, aInMozBrowserOnly, &group, &origin,
                               &isApp);
  NS_ENSURE_SUCCESS(rv, rv);

  OriginOrPatternString oops = OriginOrPatternString::FromOrigin(origin);

  nsRefPtr<AsyncUsageRunnable> runnable =
    new AsyncUsageRunnable(aAppId, aInMozBrowserOnly, group, oops, isApp, aURI,
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
                                  const nsACString& aPersistenceType,
                                  uint8_t aOptionalArgCount)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  NS_ENSURE_ARG_POINTER(aURI);

  Nullable<PersistenceType> persistenceType;
  nsresult rv =
    NullablePersistenceTypeFromText(aPersistenceType, &persistenceType);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_INVALID_ARG;
  }

  // This only works from the main process.
  NS_ENSURE_TRUE(IsMainProcess(), NS_ERROR_NOT_AVAILABLE);

  if (!aOptionalArgCount) {
    aAppId = nsIScriptSecurityManager::NO_APP_ID;
  }

  // Figure out which origin we're dealing with.
  nsCString origin;
  rv = GetInfoFromURI(aURI, aAppId, aInMozBrowserOnly, nullptr, &origin,
                      nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString pattern;
  GetOriginPatternString(aAppId, aInMozBrowserOnly, origin, pattern);

  // If there is a pending or running clear operation for this origin, return
  // immediately.
  if (IsClearOriginPending(pattern, persistenceType)) {
    return NS_OK;
  }

  OriginOrPatternString oops = OriginOrPatternString::FromPattern(pattern);

  // Queue up the origin clear runnable.
  nsRefPtr<OriginClearRunnable> runnable =
    new OriginClearRunnable(oops, persistenceType);

  rv = WaitForOpenAllowed(oops, persistenceType, EmptyCString(), runnable);
  NS_ENSURE_SUCCESS(rv, rv);

  runnable->AdvanceState();

  // Give the runnable some help by invalidating any storages in the way.
  StorageMatcher<nsAutoTArray<nsIOfflineStorage*, 20> > matches;
  matches.Find(mLiveStorages, pattern);

  for (uint32_t index = 0; index < matches.Length(); index++) {
    if (persistenceType.IsNull() ||
        matches[index]->Type() == persistenceType.Value()) {
      // We need to grab references to any live storages here to prevent them
      // from dying while we invalidate them.
      nsCOMPtr<nsIOfflineStorage> storage = matches[index];
      storage->Invalidate();
    }
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
    if (gShutdown.exchange(true)) {
      NS_ERROR("Shutdown more than once?!");
    }

    if (IsMainProcess()) {
      // Kick off the shutdown timer.
      if (NS_FAILED(mShutdownTimer->Init(this, DEFAULT_SHUTDOWN_TIMER_MS,
                                         nsITimer::TYPE_ONE_SHOT))) {
        NS_WARNING("Failed to initialize shutdown timer!");
      }

      // Each client will spin the event loop while we wait on all the threads
      // to close. Our timer may fire during that loop.
      for (uint32_t index = 0; index < Client::TYPE_MAX; index++) {
        mClients[index]->ShutdownWorkThreads();
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

uint64_t
QuotaManager::LockedCollectOriginsForEviction(
                                            uint64_t aMinSizeToBeFreed,
                                            nsTArray<OriginInfo*>& aOriginInfos)
{
  mQuotaMutex.AssertCurrentThreadOwns();

  nsRefPtr<CollectOriginsHelper> helper =
    new CollectOriginsHelper(mQuotaMutex, aMinSizeToBeFreed);

  // Unlock while calling out to XPCOM (code behind the dispatch method needs
  // to acquire its own lock which can potentially lead to a deadlock and it
  // also calls an observer that can do various stuff like IO, so it's better
  // to not hold our mutex while that happens).
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
  MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

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
                                     Nullable<PersistenceType> aPersistenceType,
                                     nsIRunnable* aRunnable)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aRunnable, "Need a runnable!");

  // Find the right SynchronizedOp.
  SynchronizedOp* op =
    FindSynchronizedOp(aPattern, aPersistenceType, EmptyCString());

  NS_ASSERTION(op, "We didn't find a SynchronizedOp?");
  NS_ASSERTION(!op->mRunnable, "SynchronizedOp already has a runnable?!?");

  ArrayCluster<nsIOfflineStorage*> liveStorages;

  StorageMatcher<ArrayCluster<nsIOfflineStorage*> > matches;
  if (aPattern.IsVoid()) {
    matches.Find(mLiveStorages);
  }
  else {
    matches.Find(mLiveStorages, aPattern);
  }

  // We want *all* storages that match the given persistence type, even those
  // that are closed, when we're going to clear the origin.
  if (!matches.IsEmpty()) {
    for (uint32_t i = 0; i < Client::TYPE_MAX; i++) {
      nsTArray<nsIOfflineStorage*>& storages = matches.ArrayAt(i);
      for (uint32_t j = 0; j < storages.Length(); j++) {
        nsIOfflineStorage* storage = storages[j];
        if (aPersistenceType.IsNull() ||
            aPersistenceType.Value() == storage->Type()) {
          storage->Invalidate();
          liveStorages[i].AppendElement(storage);
        }
      }
    }
  }

  op->mRunnable = aRunnable;

  nsRefPtr<WaitForTransactionsToFinishRunnable> runnable =
    new WaitForTransactionsToFinishRunnable(op);

  if (!liveStorages.IsEmpty()) {
    // Ask each transaction service to call us back when they're done with this
    // storage.
    for (uint32_t index = 0; index < Client::TYPE_MAX; index++)  {
      nsRefPtr<Client>& client = mClients[index];
      if (!liveStorages[index].IsEmpty()) {
        runnable->AddRun();

        client->WaitForStoragesToComplete(liveStorages[index], runnable);
      }
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
  NS_ASSERTION(aAppId != kUnknownAppId, "Bad appId!");

  // This only works from the main process.
  NS_ENSURE_TRUE(IsMainProcess(), NS_ERROR_NOT_AVAILABLE);

  nsAutoCString pattern;
  GetOriginPatternStringMaybeIgnoreBrowser(aAppId, aBrowserOnly, pattern);

  // Clear both temporary and persistent storages.
  Nullable<PersistenceType> persistenceType;

  // If there is a pending or running clear operation for this app, return
  // immediately.
  if (IsClearOriginPending(pattern, persistenceType)) {
    return NS_OK;
  }

  OriginOrPatternString oops = OriginOrPatternString::FromPattern(pattern);

  // Queue up the origin clear runnable.
  nsRefPtr<OriginClearRunnable> runnable =
    new OriginClearRunnable(oops, persistenceType);

  nsresult rv =
    WaitForOpenAllowed(oops, persistenceType, EmptyCString(), runnable);
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

  uint64_t groupUsage = 0;

  nsRefPtr<GroupInfo> temporaryGroupInfo =
    aValue->LockedGetGroupInfo(PERSISTENCE_TYPE_TEMPORARY);
  if (temporaryGroupInfo) {
    groupUsage += temporaryGroupInfo->mUsage;
  }

  nsRefPtr<GroupInfo> defaultGroupInfo =
    aValue->LockedGetGroupInfo(PERSISTENCE_TYPE_DEFAULT);
  if (defaultGroupInfo) {
    groupUsage += defaultGroupInfo->mUsage;
  }

  if (groupUsage > 0) {
    QuotaManager* quotaManager = QuotaManager::Get();
    NS_ASSERTION(quotaManager, "Shouldn't be null!");

    if (groupUsage > quotaManager->GetGroupLimit()) {
      nsTArray<OriginInfo*>* doomedOriginInfos =
        static_cast<nsTArray<OriginInfo*>*>(aUserArg);

      nsTArray<OriginInfo*> originInfos;
      if (temporaryGroupInfo) {
        originInfos.AppendElements(temporaryGroupInfo->mOriginInfos);
      }
      if (defaultGroupInfo) {
        originInfos.AppendElements(defaultGroupInfo->mOriginInfos);
      }
      originInfos.Sort(OriginInfoLRUComparator());

      for (uint32_t i = 0; i < originInfos.Length(); i++) {
        OriginInfo* originInfo = originInfos[i];

        doomedOriginInfos->AppendElement(originInfo);
        groupUsage -= originInfo->mUsage;

        if (groupUsage <= quotaManager->GetGroupLimit()) {
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

  nsTArray<OriginInfo*>* originInfos =
    static_cast<nsTArray<OriginInfo*>*>(aUserArg);

  nsRefPtr<GroupInfo> groupInfo =
    aValue->LockedGetGroupInfo(PERSISTENCE_TYPE_TEMPORARY);
  if (groupInfo) {
    originInfos->AppendElements(groupInfo->mOriginInfos);
  }

  groupInfo = aValue->LockedGetGroupInfo(PERSISTENCE_TYPE_DEFAULT);
  if (groupInfo) {
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
    OriginInfo* doomedOriginInfo = doomedOriginInfos[index];

    DeleteFilesForOrigin(doomedOriginInfo->mGroupInfo->mPersistenceType,
                         doomedOriginInfo->mOrigin);
  }

  nsTArray<OriginParams> doomedOrigins;
  {
    MutexAutoLock lock(mQuotaMutex);

    for (uint32_t index = 0; index < doomedOriginInfos.Length(); index++) {
      OriginInfo* doomedOriginInfo = doomedOriginInfos[index];

      PersistenceType persistenceType =
        doomedOriginInfo->mGroupInfo->mPersistenceType;
      nsCString group = doomedOriginInfo->mGroupInfo->mGroup;
      nsCString origin = doomedOriginInfo->mOrigin;
      bool isApp = doomedOriginInfo->mIsApp;
      LockedRemoveQuotaForOrigin(persistenceType, group, origin);

#ifdef DEBUG
      doomedOriginInfos[index] = nullptr;
#endif

      doomedOrigins.AppendElement(OriginParams(persistenceType, origin, isApp));
    }
  }

  for (uint32_t index = 0; index < doomedOrigins.Length(); index++) {
    const OriginParams& doomedOrigin = doomedOrigins[index];

    OriginClearCompleted(
                        doomedOrigin.mPersistenceType,
                        OriginOrPatternString::FromOrigin(doomedOrigin.mOrigin),
                        doomedOrigin.mIsApp);
  }
}

// static
PLDHashOperator
QuotaManager::AddLiveStorageOrigins(const nsACString& aKey,
                                    nsTArray<nsIOfflineStorage*>* aValue,
                                    void* aUserArg)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aKey.IsEmpty(), "Empty key!");
  NS_ASSERTION(aValue, "Null pointer!");
  NS_ASSERTION(aUserArg, "Null pointer!");

  OriginCollection& collection = *static_cast<OriginCollection*>(aUserArg);

  if (!collection.ContainsOrigin(aKey)) {
    collection.AddOrigin(aKey);
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

  InactiveOriginsInfo* info = static_cast<InactiveOriginsInfo*>(aUserArg);

  nsRefPtr<GroupInfo> groupInfo =
    aValue->LockedGetGroupInfo(PERSISTENCE_TYPE_TEMPORARY);
  if (groupInfo) {
    nsTArray<nsRefPtr<OriginInfo> >& originInfos = groupInfo->mOriginInfos;

    for (uint32_t i = 0; i < originInfos.Length(); i++) {
      OriginInfo* originInfo = originInfos[i];

      MOZ_ASSERT(IsTreatedAsTemporary(originInfo->mGroupInfo->mPersistenceType,
                                      originInfo->mIsApp));

      if (!info->persistentCollection.ContainsOrigin(originInfo->mOrigin)) {
        NS_ASSERTION(!originInfo->mQuotaObjects.Count(),
                     "Inactive origin shouldn't have open files!");
        info->origins.AppendElement(originInfo);
      }
    }
  }

  groupInfo = aValue->LockedGetGroupInfo(PERSISTENCE_TYPE_DEFAULT);
  if (groupInfo) {
    nsTArray<nsRefPtr<OriginInfo> >& originInfos = groupInfo->mOriginInfos;

    for (uint32_t i = 0; i < originInfos.Length(); i++) {
      OriginInfo* originInfo = originInfos[i];

      MOZ_ASSERT(IsTreatedAsTemporary(originInfo->mGroupInfo->mPersistenceType,
                                      originInfo->mIsApp));

      if (!info->temporaryCollection.ContainsOrigin(originInfo->mOrigin)) {
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
  OriginCollection temporaryOriginCollection;
  OriginCollection defaultOriginCollection;

  // Add patterns and origins that have running or pending synchronized ops.
  // (add patterns first to reduce redundancy in the origin collection).
  uint32_t index;
  for (index = 0; index < mSynchronizedOps.Length(); index++) {
    nsAutoPtr<SynchronizedOp>& op = mSynchronizedOps[index];

    const OriginOrPatternString& originOrPattern = op->mOriginOrPattern;

    if (!originOrPattern.IsPattern()) {
      continue;
    }

    Nullable<PersistenceType>& persistenceType = op->mPersistenceType;

    if (persistenceType.IsNull()) {
      temporaryOriginCollection.AddPattern(originOrPattern);
      defaultOriginCollection.AddPattern(originOrPattern);
    } else if (persistenceType.Value() == PERSISTENCE_TYPE_TEMPORARY) {
      temporaryOriginCollection.AddPattern(originOrPattern);
    } else if (persistenceType.Value() == PERSISTENCE_TYPE_DEFAULT) {
      defaultOriginCollection.AddPattern(originOrPattern);
    }
  }

  for (index = 0; index < mSynchronizedOps.Length(); index++) {
    nsAutoPtr<SynchronizedOp>& op = mSynchronizedOps[index];

    const OriginOrPatternString& originOrPattern = op->mOriginOrPattern;

    if (!originOrPattern.IsOrigin()) {
      continue;
    }

    Nullable<PersistenceType>& persistenceType = op->mPersistenceType;

    if (persistenceType.IsNull()) {
      temporaryOriginCollection.AddOrigin(originOrPattern);
      defaultOriginCollection.AddOrigin(originOrPattern);
    } else if (persistenceType.Value() == PERSISTENCE_TYPE_TEMPORARY) {
      temporaryOriginCollection.AddOrigin(originOrPattern);
    } else if (persistenceType.Value() == PERSISTENCE_TYPE_DEFAULT) {
      defaultOriginCollection.AddOrigin(originOrPattern);
    }
  }

  // Add origins that have live temporary storages.
  mTemporaryLiveStorageTable.EnumerateRead(AddLiveStorageOrigins,
                                           &temporaryOriginCollection);

  // Add origins that have live persistent storages.
  mDefaultLiveStorageTable.EnumerateRead(AddLiveStorageOrigins,
                                         &defaultOriginCollection);

  // Enumerate inactive origins. This must be protected by the mutex.
  nsTArray<OriginInfo*> inactiveOrigins;
  {
    InactiveOriginsInfo info(temporaryOriginCollection,
                             defaultOriginCollection,
                             inactiveOrigins);
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
      OriginInfo* inactiveOrigin = inactiveOrigins[index];

      OriginOrPatternString oops =
        OriginOrPatternString::FromOrigin(inactiveOrigin->mOrigin);

      Nullable<PersistenceType> persistenceType =
        Nullable<PersistenceType>(inactiveOrigin->mGroupInfo->mPersistenceType);

      AddSynchronizedOp(oops, persistenceType);
    }

    inactiveOrigins.SwapElements(aOriginInfos);
    return sizeToBeFreed;
  }

  return 0;
}

void
QuotaManager::DeleteFilesForOrigin(PersistenceType aPersistenceType,
                                   const nsACString& aOrigin)
{
  nsCOMPtr<nsIFile> directory;
  nsresult rv = GetDirectoryForOrigin(aPersistenceType, aOrigin,
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
QuotaManager::FinalizeOriginEviction(nsTArray<OriginParams>& aOrigins)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<FinalizeOriginEvictionRunnable> runnable =
    new FinalizeOriginEvictionRunnable(aOrigins);

  nsresult rv = IsOnIOThread() ? runnable->RunImmediately()
                               : runnable->Dispatch();
  NS_ENSURE_SUCCESS_VOID(rv);
}

void
QuotaManager::SaveOriginAccessTime(PersistenceType aPersistenceType,
                                   const nsACString& aOrigin,
                                   int64_t aTimestamp)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (QuotaManager::IsShuttingDown()) {
    return;
  }

  nsRefPtr<SaveOriginAccessTimeRunnable> runnable =
    new SaveOriginAccessTimeRunnable(aPersistenceType, aOrigin, aTimestamp);

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
  NS_ASSERTION(aAppId != kUnknownAppId, "Bad appId!");
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

auto
QuotaManager::GetLiveStorageTable(PersistenceType aPersistenceType)
  -> LiveStorageTable&
{
  switch (aPersistenceType) {
    case PERSISTENCE_TYPE_TEMPORARY:
      return mTemporaryLiveStorageTable;
    case PERSISTENCE_TYPE_DEFAULT:
      return mDefaultLiveStorageTable;

    case PERSISTENCE_TYPE_PERSISTENT:
    case PERSISTENCE_TYPE_INVALID:
    default:
      MOZ_CRASH("Bad persistence type value!");
  }
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
  NS_ASSERTION(!mRunnable, "Any runnable should be gone by now!");

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

    nsString leafName;
    rv = file->GetLeafName(leafName);
    NS_ENSURE_SUCCESS_VOID(rv);

    bool isDirectory;
    rv = file->IsDirectory(&isDirectory);
    NS_ENSURE_SUCCESS_VOID(rv);

    if (!isDirectory) {
      if (!leafName.EqualsLiteral(DSSTORE_FILE_NAME)) {
        NS_WARNING("Something in the IndexedDB directory that doesn't belong!");
      }
      continue;
    }

    // Skip storages for other apps.
    if (!PatternMatchesOrigin(originSanitized,
                              NS_ConvertUTF16toUTF8(leafName))) {
      continue;
    }

    int64_t timestamp;
    nsCString group;
    nsCString origin;
    bool isApp;
    rv = GetDirectoryMetadata(file, &timestamp, group, origin, &isApp);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    for (uint32_t index = 0; index < 10; index++) {
      // We can't guarantee that this will always succeed on Windows...
      if (NS_SUCCEEDED((rv = file->Remove(true)))) {
        break;
      }

      NS_WARNING("Failed to remove directory, retrying after a short delay.");

      PR_Sleep(PR_MillisecondsToInterval(200));
    }

    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to remove directory, giving up!");
    }

    if (aPersistenceType != PERSISTENCE_TYPE_PERSISTENT) {
      aQuotaManager->RemoveQuotaForOrigin(aPersistenceType, group, origin);
    }

    aQuotaManager->OriginClearCompleted(aPersistenceType, origin, isApp);
  }
}

NS_IMPL_ISUPPORTS_INHERITED0(OriginClearRunnable, nsRunnable)

NS_IMETHODIMP
OriginClearRunnable::Run()
{
  PROFILER_LABEL("OriginClearRunnable", "Run",
    js::ProfileEntry::Category::OTHER);

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
        quotaManager->AcquireExclusiveAccess(mOriginOrPattern, mPersistenceType,
                                             this);
      NS_ENSURE_SUCCESS(rv, rv);

      return NS_OK;
    }

    case IO: {
      AssertIsOnIOThread();

      AdvanceState();

      if (mPersistenceType.IsNull()) {
        DeleteFiles(quotaManager, PERSISTENCE_TYPE_PERSISTENT);
        DeleteFiles(quotaManager, PERSISTENCE_TYPE_TEMPORARY);
        DeleteFiles(quotaManager, PERSISTENCE_TYPE_DEFAULT);
      } else {
        DeleteFiles(quotaManager, mPersistenceType.Value());
      }

      // Now dispatch back to the main thread.
      if (NS_FAILED(NS_DispatchToMainThread(this))) {
        NS_WARNING("Failed to dispatch to main thread!");
        return NS_ERROR_FAILURE;
      }

      return NS_OK;
    }

    case Complete: {
      NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

      // Tell the QuotaManager that we're done.
      quotaManager->AllowNextSynchronizedOp(mOriginOrPattern, mPersistenceType,
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
                                       bool aIsApp,
                                       nsIURI* aURI,
                                       nsIUsageCallback* aCallback)
: mURI(aURI),
  mCallback(aCallback),
  mAppId(aAppId),
  mGroup(aGroup),
  mOrigin(aOrigin),
  mCallbackState(Pending),
  mInMozBrowserOnly(aInMozBrowserOnly),
  mIsApp(aIsApp)
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

      // Add all the default storage files we care about.
      rv = AddToUsage(quotaManager, PERSISTENCE_TYPE_DEFAULT);
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

    if (IsTreatedAsPersistent(aPersistenceType, mIsApp)) {
      nsCString originKey = OriginKey(aPersistenceType, mOrigin);
      initialized = aQuotaManager->mInitializedOrigins.Contains(originKey);

    } else {
      initialized = aQuotaManager->mTemporaryStorageInitialized;
    }

    if (aPersistenceType == PERSISTENCE_TYPE_PERSISTENT && !initialized) {
      rv = MaybeUpgradeOriginDirectory(directory);
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

NS_IMPL_ISUPPORTS_INHERITED(AsyncUsageRunnable, nsRunnable, nsIQuotaRequest)

NS_IMETHODIMP
AsyncUsageRunnable::Run()
{
  PROFILER_LABEL("Quota", "AsyncUsageRunnable::Run",
    js::ProfileEntry::Category::OTHER);

  nsresult rv = RunInternal();

  if (!NS_IsMainThread()) {
    if (NS_FAILED(rv)) {
      ResetUsage();
    }

    if (NS_FAILED(NS_DispatchToMainThread(this))) {
      NS_WARNING("Failed to dispatch to main thread!");
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
AsyncUsageRunnable::Cancel()
{
  if (mCanceled.exchange(true)) {
    NS_WARNING("Canceled more than once?!");
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

void
ResetOrClearRunnable::DeleteFiles(QuotaManager* aQuotaManager)
{
  AssertIsOnIOThread();
  NS_ASSERTION(aQuotaManager, "Don't pass me null!");

  nsresult rv;

  nsCOMPtr<nsIFile> directory =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS_VOID(rv);

  rv = directory->InitWithPath(aQuotaManager->GetStoragePath());
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
        quotaManager->AcquireExclusiveAccess(NullCString(),
                                             Nullable<PersistenceType>(), this);
      NS_ENSURE_SUCCESS(rv, rv);

      return NS_OK;
    }

    case IO: {
      AssertIsOnIOThread();

      AdvanceState();

      if (mClear) {
        DeleteFiles(quotaManager);
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
        const OriginParams& origin = mOrigins[index];

        quotaManager->OriginClearCompleted(
                              origin.mPersistenceType,
                              OriginOrPatternString::FromOrigin(origin.mOrigin),
                              origin.mIsApp);
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
        const OriginParams& origin = mOrigins[index];

        quotaManager->AllowNextSynchronizedOp(
                             OriginOrPatternString::FromOrigin(origin.mOrigin),
                             Nullable<PersistenceType>(origin.mPersistenceType),
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
  NS_ASSERTION(mOp->mRunnable, "Nothing to run!");
  NS_ASSERTION(mCountdown, "Wrong countdown!");

  if (--mCountdown) {
    return NS_OK;
  }

  // Don't hold the runnable alive longer than necessary.
  nsCOMPtr<nsIRunnable> runnable;
  runnable.swap(mOp->mRunnable);

  mOp = nullptr;

  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "This should never fail!");

  nsresult rv =
    quotaManager->IOThread()->Dispatch(runnable, NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  // The listener is responsible for calling
  // QuotaManager::AllowNextSynchronizedOp.
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
    quotaManager->GetDirectoryForOrigin(mPersistenceType, mOrigin,
                                        getter_AddRefs(directory));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIBinaryOutputStream> stream;
  rv = GetDirectoryMetadataOutputStream(directory, kUpdateFileFlag,
                                        getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  // The origin directory may not exist anymore.
  if (stream) {
    rv = stream->Write64(mTimestamp);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
StorageDirectoryHelper::CreateOrUpgradeMetadataFiles()
{
  AssertIsOnIOThread();

  nsresult rv;

  nsCOMPtr<nsISimpleEnumerator> entries;
  rv = mDirectory->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool hasMore;
  while (NS_SUCCEEDED((rv = entries->HasMoreElements(&hasMore))) && hasMore) {
    nsCOMPtr<nsISupports> entry;
    rv = entries->GetNext(getter_AddRefs(entry));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCOMPtr<nsIFile> originDir = do_QueryInterface(entry);
    MOZ_ASSERT(originDir);

    nsString leafName;
    rv = originDir->GetLeafName(leafName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    bool isDirectory;
    rv = originDir->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (!isDirectory) {
      if (!leafName.EqualsLiteral(DSSTORE_FILE_NAME)) {
        NS_WARNING("Something in the storage directory that doesn't belong!");
      }
      continue;
    }

    if (mPersistent) {
      rv = MaybeUpgradeOriginDirectory(originDir);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    if (leafName.EqualsLiteral(kChromeOrigin)) {
      OriginProps* originProps = mOriginProps.AppendElement();
      originProps->mDirectory = originDir;
      originProps->mSpec = kChromeOrigin;
      originProps->mType = OriginProps::eChrome;
    } else {
      nsCString spec;
      uint32_t appId;
      bool inMozBrowser;
      if (NS_WARN_IF(!OriginParser::ParseOrigin(NS_ConvertUTF16toUTF8(leafName),
                                                &appId, &inMozBrowser, spec))) {
        return NS_ERROR_FAILURE;
      }

      OriginProps* originProps = mOriginProps.AppendElement();
      originProps->mDirectory = originDir;
      originProps->mSpec = spec;
      originProps->mAppId = appId;
      originProps->mType = OriginProps::eContent;
      originProps->mInMozBrowser = inMozBrowser;

      if (mPersistent) {
        int64_t timestamp = INT64_MIN;
        rv = GetLastModifiedTime(originDir, &timestamp);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        originProps->mTimestamp = timestamp;
      }
    }
  }

  if (mOriginProps.IsEmpty()) {
    return NS_OK;
  }

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(this)));

  {
    mozilla::MutexAutoLock autolock(mMutex);
    while (mWaiting) {
      mCondVar.Wait();
    }
  }

  if (NS_WARN_IF(NS_FAILED(mMainThreadResultCode))) {
    return mMainThreadResultCode;
  }

  // Verify that the bounce to the main thread didn't start the shutdown
  // sequence.
  if (NS_WARN_IF(QuotaManager::IsShuttingDown())) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIFile> permanentStorageDir;

  for (uint32_t count = mOriginProps.Length(), index = 0;
       index < count;
       index++) {
    OriginProps& originProps = mOriginProps[index];

    if (mPersistent) {
      rv = CreateDirectoryMetadata(originProps.mDirectory,
                                   originProps.mTimestamp,
                                   originProps.mGroup,
                                   originProps.mOrigin,
                                   originProps.mIsApp);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      // Move whitelisted origins to new persistent storage.
      if (QuotaManager::IsOriginWhitelistedForPersistentStorage(
                                                           originProps.mSpec)) {
        if (!permanentStorageDir) {
          permanentStorageDir =
            do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }

          QuotaManager* quotaManager = QuotaManager::Get();
          MOZ_ASSERT(quotaManager);

          const nsString& permanentStoragePath =
            quotaManager->GetStoragePath(PERSISTENCE_TYPE_PERSISTENT);

          rv = permanentStorageDir->InitWithPath(permanentStoragePath);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            return rv;
          }
        }

        rv = originProps.mDirectory->MoveTo(permanentStorageDir, EmptyString());
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    } else {
      nsCOMPtr<nsIBinaryOutputStream> stream;
      rv = GetDirectoryMetadataOutputStream(originProps.mDirectory,
                                            kAppendFileFlag,
                                            getter_AddRefs(stream));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      MOZ_ASSERT(stream);

      rv = stream->WriteBoolean(originProps.mIsApp);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  return NS_OK;
}

nsresult
StorageDirectoryHelper::RunOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mOriginProps.IsEmpty());

  nsresult rv;

  nsCOMPtr<nsIScriptSecurityManager> secMan =
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  for (uint32_t count = mOriginProps.Length(), index = 0;
       index < count;
       index++) {
    OriginProps& originProps = mOriginProps[index];

    switch (originProps.mType) {
      case OriginProps::eChrome: {
        QuotaManager::GetInfoForChrome(&originProps.mGroup,
                                       &originProps.mOrigin,
                                       &originProps.mIsApp);
        break;
      }

      case OriginProps::eContent: {
        nsCOMPtr<nsIURI> uri;
        rv = NS_NewURI(getter_AddRefs(uri), originProps.mSpec);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        nsCOMPtr<nsIPrincipal> principal;
        if (originProps.mAppId == kUnknownAppId) {
          rv = secMan->GetSimpleCodebasePrincipal(uri,
                                                  getter_AddRefs(principal));
        } else {
          rv = secMan->GetAppCodebasePrincipal(uri,
                                               originProps.mAppId,
                                               originProps.mInMozBrowser,
                                               getter_AddRefs(principal));
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        if (mPersistent) {
          rv = QuotaManager::GetInfoFromPrincipal(principal,
                                                  &originProps.mGroup,
                                                  &originProps.mOrigin,
                                                  &originProps.mIsApp);
        } else {
          rv = QuotaManager::GetInfoFromPrincipal(principal,
                                                  nullptr,
                                                  nullptr,
                                                  &originProps.mIsApp);
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        break;
      }

      default:
        MOZ_CRASH("Bad type!");
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
StorageDirectoryHelper::Run()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = RunOnMainThread();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mMainThreadResultCode = rv;
  }

  MutexAutoLock lock(mMutex);
  MOZ_ASSERT(mWaiting);

  mWaiting = false;
  mCondVar.Notify();

  return NS_OK;
}

// static
bool
OriginParser::ParseOrigin(const nsACString& aOrigin,
                          uint32_t* aAppId,
                          bool* aInMozBrowser,
                          nsCString& aSpec)
{
  MOZ_ASSERT(!aOrigin.IsEmpty());
  MOZ_ASSERT(aAppId);
  MOZ_ASSERT(aInMozBrowser);

  OriginParser parser(aOrigin);

  if (!parser.Parse(aAppId, aInMozBrowser, aSpec)) {
    return false;
  }

  return true;
}

bool
OriginParser::Parse(uint32_t* aAppId,
                    bool* aInMozBrowser,
                    nsACString& aSpec)
{
  MOZ_ASSERT(aAppId);
  MOZ_ASSERT(aInMozBrowser);

  while (mTokenizer.hasMoreTokens()) {
    const nsDependentCSubstring& token = mTokenizer.nextToken();

    HandleToken(token);

    if (mError) {
      break;
    }

    if (!mHandledTokens.IsEmpty()) {
      mHandledTokens.Append(NS_LITERAL_CSTRING(", "));
    }
    mHandledTokens.Append('\'');
    mHandledTokens.Append(token);
    mHandledTokens.Append('\'');
  }

  if (!mError && mTokenizer.separatorAfterCurrentToken()) {
    HandleTrailingSeparator();
  }

  if (mError) {
    QM_WARNING("Origin '%s' failed to parse, handled tokens: %s", mOrigin.get(),
               mHandledTokens.get());

    return false;
  }

  MOZ_ASSERT(mState == eComplete || mState == eHandledTrailingSeparator);

  *aAppId = mAppId;
  *aInMozBrowser = mInMozBrowser;

  nsAutoCString spec(mSchema);

  if (mSchemaType == eFile) {
    spec.AppendLiteral("://");

    for (uint32_t count = mPathnameComponents.Length(), index = 0;
         index < count;
         index++) {
      spec.Append('/');
      spec.Append(mPathnameComponents[index]);
    }

    aSpec = spec;

    return true;
  }

  if (mSchemaType == eMozSafeAbout) {
    spec.Append(':');
  } else {
    spec.AppendLiteral("://");
  }

  spec.Append(mHost);

  if (!mPort.IsNull()) {
    spec.Append(':');
    spec.AppendInt(mPort.Value());
  }

  aSpec = spec;

  return true;
}

void
OriginParser::HandleSchema(const nsDependentCSubstring& aToken)
{
  MOZ_ASSERT(!aToken.IsEmpty());
  MOZ_ASSERT(mState == eExpectingAppIdOrSchema || mState == eExpectingSchema);

  bool isMozSafeAbout = false;
  bool isFile = false;
  if (aToken.EqualsLiteral("http") ||
      aToken.EqualsLiteral("https") ||
      (isMozSafeAbout = aToken.EqualsLiteral("moz-safe-about")) ||
      aToken.EqualsLiteral("indexeddb") ||
      (isFile = aToken.EqualsLiteral("file")) ||
      aToken.EqualsLiteral("app")) {
    mSchema = aToken;

    if (isMozSafeAbout) {
      mSchemaType = eMozSafeAbout;
      mState = eExpectingHost;
    } else {
      if (isFile) {
        mSchemaType = eFile;
      }
      mState = eExpectingEmptyToken1;
    }

    return;
  }

  QM_WARNING("'%s' is not a valid schema!", nsCString(aToken).get());

  mError = true;
}

void
OriginParser::HandlePathnameComponent(const nsDependentCSubstring& aToken)
{
  MOZ_ASSERT(!aToken.IsEmpty());
  MOZ_ASSERT(mState == eExpectingDriveLetterOrPathnameComponent ||
             mState == eExpectingEmptyTokenOrPathnameComponent ||
             mState == eExpectingPathnameComponent);
  MOZ_ASSERT(mSchemaType == eFile);

  mPathnameComponents.AppendElement(aToken);

  mState = mTokenizer.hasMoreTokens() ? eExpectingPathnameComponent : eComplete;
}

void
OriginParser::HandleToken(const nsDependentCSubstring& aToken)
{
  switch (mState) {
    case eExpectingAppIdOrSchema: {
      if (aToken.IsEmpty()) {
        QM_WARNING("Expected an app id or schema (not an empty string)!");

        mError = true;
        return;
      }

      if (NS_IsAsciiDigit(aToken.First())) {
        // nsDependentCSubstring doesn't provice ToInteger()
        nsCString token(aToken);

        nsresult rv;
        uint32_t appId = token.ToInteger(&rv);
        if (NS_SUCCEEDED(rv)) {
          mAppId = appId;
          mState = eExpectingInMozBrowser;
          return;
        }
      }

      HandleSchema(aToken);

      return;
    }

    case eExpectingInMozBrowser: {
      if (aToken.Length() != 1) {
        QM_WARNING("'%d' is not a valid length for the inMozBrowser flag!",
                   aToken.Length());

        mError = true;
        return;
      }

      if (aToken.First() == 't') {
        mInMozBrowser = true;
      } else if (aToken.First() == 'f') {
        mInMozBrowser = false;
      } else {
        QM_WARNING("'%s' is not a valid value for the inMozBrowser flag!",
                   nsCString(aToken).get());

        mError = true;
        return;
      }

      mState = eExpectingSchema;

      return;
    }

    case eExpectingSchema: {
      if (aToken.IsEmpty()) {
        QM_WARNING("Expected a schema (not an empty string)!");

        mError = true;
        return;
      }

      HandleSchema(aToken);

      return;
    }

    case eExpectingEmptyToken1: {
      if (!aToken.IsEmpty()) {
        QM_WARNING("Expected the first empty token!");

        mError = true;
        return;
      }

      mState = eExpectingEmptyToken2;

      return;
    }

    case eExpectingEmptyToken2: {
      if (!aToken.IsEmpty()) {
        QM_WARNING("Expected the second empty token!");

        mError = true;
        return;
      }

      if (mSchemaType == eFile) {
        mState = eExpectingEmptyToken3;
      } else {
        mState = eExpectingHost;
      }

      return;
    }

    case eExpectingEmptyToken3: {
      MOZ_ASSERT(mSchemaType == eFile);

      if (!aToken.IsEmpty()) {
        QM_WARNING("Expected the third empty token!");

        mError = true;
        return;
      }

      mState = eExpectingDriveLetterOrPathnameComponent;

      return;
    }

    case eExpectingHost: {
      if (aToken.IsEmpty()) {
        QM_WARNING("Expected a host (not an empty string)!");

        mError = true;
        return;
      }

      mHost = aToken;

      mState = mTokenizer.hasMoreTokens() ? eExpectingPort : eComplete;

      return;
    }

    case eExpectingPort: {
      MOZ_ASSERT(mSchemaType == eNone);

      if (aToken.IsEmpty()) {
        QM_WARNING("Expected a port (not an empty string)!");

        mError = true;
        return;
      }

      // nsDependentCSubstring doesn't provice ToInteger()
      nsCString token(aToken);

      nsresult rv;
      uint32_t port = token.ToInteger(&rv);
      if (NS_SUCCEEDED(rv)) {
        mPort.SetValue() = port;
      } else {
        QM_WARNING("'%s' is not a valid port number!", token.get());

        mError = true;
        return;
      }

      mState = eComplete;

      return;
    }

    case eExpectingDriveLetterOrPathnameComponent: {
      MOZ_ASSERT(mSchemaType == eFile);

      if (aToken.IsEmpty()) {
        QM_WARNING("Expected a drive letter or pathname component "
                   "(not an empty string)!");

        mError = true;
        return;
      }

      if (aToken.Length() == 1 && NS_IsAsciiAlpha(aToken.First())) {
        mMaybeDriveLetter = true;

        mPathnameComponents.AppendElement(aToken);

        mState =
          mTokenizer.hasMoreTokens() ? eExpectingEmptyTokenOrPathnameComponent
                                     : eComplete;

        return;
      }

      HandlePathnameComponent(aToken);

      return;
    }

    case eExpectingEmptyTokenOrPathnameComponent: {
      MOZ_ASSERT(mSchemaType == eFile);

      if (mMaybeDriveLetter && aToken.IsEmpty()) {
        MOZ_ASSERT(mPathnameComponents.Length() == 1);

        nsCString& pathnameComponent = mPathnameComponents[0];
        pathnameComponent.Append(':');

        mState = mTokenizer.hasMoreTokens() ? eExpectingPathnameComponent
                                            : eComplete;

        return;
      }

      HandlePathnameComponent(aToken);

      return;
    }

    case eExpectingPathnameComponent: {
      if (aToken.IsEmpty()) {
        QM_WARNING("Expected a pathname component (not an empty string)!");

        mError = true;
        return;
      }

      HandlePathnameComponent(aToken);

      return;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }
}

void
OriginParser::HandleTrailingSeparator()
{
  MOZ_ASSERT(mState = eComplete);
  MOZ_ASSERT(mSchemaType == eFile);

  mPathnameComponents.AppendElement(EmptyCString());

  mState = eHandledTrailingSeparator;
}
