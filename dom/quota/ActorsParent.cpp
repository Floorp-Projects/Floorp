/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActorsParent.h"

#include "mozIStorageConnection.h"
#include "mozIStorageService.h"
#include "nsIBinaryInputStream.h"
#include "nsIBinaryOutputStream.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsIObserverService.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsIRunnable.h"
#include "nsISimpleEnumerator.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsITimer.h"
#include "nsIURI.h"
#include "nsPIDOMWindow.h"

#include <algorithm>
#include "GeckoProfiler.h"
#include "mozilla/Atomics.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/CondVar.h"
#include "mozilla/dom/PContent.h"
#include "mozilla/dom/asmjscache/AsmJSCache.h"
#include "mozilla/dom/cache/QuotaClient.h"
#include "mozilla/dom/indexedDB/ActorsParent.h"
#include "mozilla/dom/quota/PQuotaParent.h"
#include "mozilla/dom/quota/PQuotaRequestParent.h"
#include "mozilla/dom/quota/PQuotaUsageRequestParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/Mutex.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/unused.h"
#include "mozStorageCID.h"
#include "mozStorageHelper.h"
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
#include "prio.h"
#include "xpcpublic.h"

#include "OriginScope.h"
#include "QuotaManager.h"
#include "QuotaManagerService.h"
#include "QuotaObject.h"
#include "UsageInfo.h"

#define DISABLE_ASSERTS_FOR_FUZZING 0

#if DISABLE_ASSERTS_FOR_FUZZING
#define ASSERT_UNLESS_FUZZING(...) do { } while (0)
#else
#define ASSERT_UNLESS_FUZZING(...) MOZ_ASSERT(false, __VA_ARGS__)
#endif

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

#define KB * 1024ULL
#define MB * 1024ULL KB
#define GB * 1024ULL MB

namespace mozilla {
namespace dom {
namespace quota {

using namespace mozilla::ipc;

const bool QuotaManager::kRunningXPCShellTests = !!PR_GetEnv("XPCSHELL_TEST_PROFILE_DIR");

// We want profiles to be platform-independent so we always need to replace
// the same characters on every platform. Windows has the most extensive set
// of illegal characters so we use its FILE_ILLEGAL_CHARACTERS and
// FILE_PATH_SEPARATOR.
const char QuotaManager::kReplaceChars[] = CONTROL_CHARACTERS "/:*?\"<>|\\";

namespace {

/*******************************************************************************
 * Constants
 ******************************************************************************/

const uint32_t kSQLitePageSizeOverride = 512;

// Major storage version. Bump for backwards-incompatible changes.
const uint32_t kMajorStorageVersion = 1;

// Minor storage version. Bump for backwards-compatible changes.
const uint32_t kMinorStorageVersion = 0;

// The storage version we store in the SQLite database is a (signed) 32-bit
// integer. The major version is left-shifted 16 bits so the max value is
// 0xFFFF. The minor version occupies the lower 16 bits and its max is 0xFFFF.
static_assert(kMajorStorageVersion <= 0xFFFF,
              "Major version needs to fit in 16 bits.");
static_assert(kMinorStorageVersion <= 0xFFFF,
              "Minor version needs to fit in 16 bits.");

const int32_t kStorageVersion =
  int32_t((kMajorStorageVersion << 16) + kMinorStorageVersion);

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

const char kChromeOrigin[] = "chrome";
const char kAboutHomeOriginPrefix[] = "moz-safe-about:home";
const char kIndexedDBOriginPrefix[] = "indexeddb://";
const char kResourceOriginPrefix[] = "resource://";

#define INDEXEDDB_DIRECTORY_NAME "indexedDB"
#define STORAGE_DIRECTORY_NAME "storage"
#define PERSISTENT_DIRECTORY_NAME "persistent"
#define PERMANENT_DIRECTORY_NAME "permanent"
#define TEMPORARY_DIRECTORY_NAME "temporary"
#define DEFAULT_DIRECTORY_NAME "default"

enum AppId {
  kNoAppId = nsIScriptSecurityManager::NO_APP_ID,
  kUnknownAppId = nsIScriptSecurityManager::UNKNOWN_APP_ID
};

#define STORAGE_FILE_NAME "storage.sqlite"

// The name of the file that we use to load/save the last access time of an
// origin.
#define METADATA_FILE_NAME ".metadata"
#define METADATA_V2_FILE_NAME ".metadata-v2"

/******************************************************************************
 * SQLite functions
 ******************************************************************************/

#if 0
int32_t
MakeStorageVersion(uint32_t aMajorStorageVersion,
                   uint32_t aMinorStorageVersion)
{
  return int32_t((aMajorStorageVersion << 16) + aMinorStorageVersion);
}
#endif

uint32_t
GetMajorStorageVersion(int32_t aStorageVersion)
{
  return uint32_t(aStorageVersion >> 16);
}

/******************************************************************************
 * Quota manager class declarations
 ******************************************************************************/

} // namespace

class DirectoryLockImpl final
  : public DirectoryLock
{
  RefPtr<QuotaManager> mQuotaManager;

  const Nullable<PersistenceType> mPersistenceType;
  const nsCString mGroup;
  const OriginScope mOriginScope;
  const Nullable<bool> mIsApp;
  const Nullable<Client::Type> mClientType;
  RefPtr<OpenDirectoryListener> mOpenListener;

  nsTArray<DirectoryLockImpl*> mBlocking;
  nsTArray<DirectoryLockImpl*> mBlockedOn;

  const bool mExclusive;

  // Internal quota manager operations use this flag to prevent directory lock
  // registraction/unregistration from updating origin access time, etc.
  const bool mInternal;

  bool mInvalidated;

public:
  DirectoryLockImpl(QuotaManager* aQuotaManager,
                    Nullable<PersistenceType> aPersistenceType,
                    const nsACString& aGroup,
                    const OriginScope& aOriginScope,
                    Nullable<bool> aIsApp,
                    Nullable<Client::Type> aClientType,
                    bool aExclusive,
                    bool aInternal,
                    OpenDirectoryListener* aOpenListener);

  void
  AssertIsOnOwningThread() const
#ifdef DEBUG
  ;
#else
  { }
#endif

  const Nullable<PersistenceType>&
  GetPersistenceType() const
  {
    return mPersistenceType;
  }

  const nsACString&
  GetGroup() const
  {
    return mGroup;
  }

  const OriginScope&
  GetOriginScope() const
  {
    return mOriginScope;
  }

  const Nullable<bool>&
  GetIsApp() const
  {
    return mIsApp;
  }

  const Nullable<Client::Type>&
  GetClientType() const
  {
    return mClientType;
  }

  bool
  IsInternal() const
  {
    return mInternal;
  }

  bool
  ShouldUpdateLockTable()
  {
    return !mInternal &&
           mPersistenceType.Value() != PERSISTENCE_TYPE_PERSISTENT;
  }

  // Test whether this DirectoryLock needs to wait for the given lock.
  bool
  MustWaitFor(const DirectoryLockImpl& aLock);

  void
  AddBlockingLock(DirectoryLockImpl* aLock)
  {
    AssertIsOnOwningThread();

    mBlocking.AppendElement(aLock);
  }

  const nsTArray<DirectoryLockImpl*>&
  GetBlockedOnLocks()
  {
    return mBlockedOn;
  }

  void
  AddBlockedOnLock(DirectoryLockImpl* aLock)
  {
    AssertIsOnOwningThread();

    mBlockedOn.AppendElement(aLock);
  }

  void
  MaybeUnblock(DirectoryLockImpl* aLock)
  {
    AssertIsOnOwningThread();

    mBlockedOn.RemoveElement(aLock);
    if (mBlockedOn.IsEmpty()) {
      NotifyOpenListener();
    }
  }

  void
  NotifyOpenListener();

  void
  Invalidate()
  {
    AssertIsOnOwningThread();

    mInvalidated = true;
  }

  NS_INLINE_DECL_REFCOUNTING(DirectoryLockImpl)

private:
  ~DirectoryLockImpl();
};

class QuotaManager::CreateRunnable final
  : public BackgroundThreadObject
  , public Runnable
{
  nsTArray<nsCOMPtr<nsIRunnable>> mCallbacks;
  nsString mBaseDirPath;
  RefPtr<QuotaManager> mManager;
  nsresult mResultCode;

  enum class State
  {
    Initial,
    CreatingManager,
    RegisteringObserver,
    CallingCallbacks,
    Completed
  };

  State mState;

public:
  CreateRunnable()
    : mResultCode(NS_OK)
    , mState(State::Initial)
  {
    AssertIsOnBackgroundThread();
  }

  void
  AddCallback(nsIRunnable* aCallback)
  {
    AssertIsOnOwningThread();
    MOZ_ASSERT(aCallback);

    mCallbacks.AppendElement(aCallback);
  }

private:
  ~CreateRunnable()
  { }

  nsresult
  Init();

  nsresult
  CreateManager();

  nsresult
  RegisterObserver();

  void
  CallCallbacks();

  State
  GetNextState(nsCOMPtr<nsIEventTarget>& aThread);

  NS_DECL_NSIRUNNABLE
};

class QuotaManager::ShutdownRunnable final
  : public Runnable
{
  // Only touched on the main thread.
  bool& mDone;

public:
  explicit ShutdownRunnable(bool& aDone)
    : mDone(aDone)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

private:
  ~ShutdownRunnable()
  { }

  NS_DECL_NSIRUNNABLE
};

class QuotaManager::ShutdownObserver final
  : public nsIObserver
{
  nsCOMPtr<nsIEventTarget> mBackgroundThread;

public:
  explicit ShutdownObserver(nsIEventTarget* aBackgroundThread)
    : mBackgroundThread(aBackgroundThread)
  {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

private:
  ~ShutdownObserver()
  {
    MOZ_ASSERT(NS_IsMainThread());
  }
};

namespace {

/*******************************************************************************
 * Local class declarations
 ******************************************************************************/

} // namespace

class OriginInfo final
{
  friend class GroupInfo;
  friend class QuotaManager;
  friend class QuotaObject;

public:
  OriginInfo(GroupInfo* aGroupInfo, const nsACString& aOrigin, bool aIsApp,
             uint64_t aUsage, int64_t aAccessTime)
  : mGroupInfo(aGroupInfo), mOrigin(aOrigin), mUsage(aUsage),
    mAccessTime(aAccessTime), mIsApp(aIsApp)
  {
    MOZ_COUNT_CTOR(OriginInfo);
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(OriginInfo)

  int64_t
  AccessTime() const
  {
    return mAccessTime;
  }

private:
  // Private destructor, to discourage deletion outside of Release():
  ~OriginInfo()
  {
    MOZ_COUNT_DTOR(OriginInfo);

    MOZ_ASSERT(!mQuotaObjects.Count());
  }

  void
  LockedDecreaseUsage(int64_t aSize);

  void
  LockedUpdateAccessTime(int64_t aAccessTime)
  {
    AssertCurrentThreadOwnsQuotaMutex();

    mAccessTime = aAccessTime;
  }

  nsDataHashtable<nsStringHashKey, QuotaObject*> mQuotaObjects;

  GroupInfo* mGroupInfo;
  const nsCString mOrigin;
  uint64_t mUsage;
  int64_t mAccessTime;
  const bool mIsApp;
};

class OriginInfoLRUComparator
{
public:
  bool
  Equals(const OriginInfo* a, const OriginInfo* b) const
  {
    return
      a && b ? a->AccessTime() == b->AccessTime() : !a && !b ? true : false;
  }

  bool
  LessThan(const OriginInfo* a, const OriginInfo* b) const
  {
    return a && b ? a->AccessTime() < b->AccessTime() : b ? true : false;
  }
};

class GroupInfo final
{
  friend class GroupInfoPair;
  friend class OriginInfo;
  friend class QuotaManager;
  friend class QuotaObject;

public:
  GroupInfo(GroupInfoPair* aGroupInfoPair, PersistenceType aPersistenceType,
            const nsACString& aGroup)
  : mGroupInfoPair(aGroupInfoPair), mPersistenceType(aPersistenceType),
    mGroup(aGroup), mUsage(0)
  {
    MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

    MOZ_COUNT_CTOR(GroupInfo);
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GroupInfo)

private:
  // Private destructor, to discourage deletion outside of Release():
  ~GroupInfo()
  {
    MOZ_COUNT_DTOR(GroupInfo);
  }

  already_AddRefed<OriginInfo>
  LockedGetOriginInfo(const nsACString& aOrigin);

  void
  LockedAddOriginInfo(OriginInfo* aOriginInfo);

  void
  LockedRemoveOriginInfo(const nsACString& aOrigin);

  void
  LockedRemoveOriginInfos();

  bool
  LockedHasOriginInfos()
  {
    AssertCurrentThreadOwnsQuotaMutex();

    return !mOriginInfos.IsEmpty();
  }

  nsTArray<RefPtr<OriginInfo> > mOriginInfos;

  GroupInfoPair* mGroupInfoPair;
  PersistenceType mPersistenceType;
  nsCString mGroup;
  uint64_t mUsage;
};

class GroupInfoPair
{
  friend class QuotaManager;
  friend class QuotaObject;

public:
  GroupInfoPair()
  {
    MOZ_COUNT_CTOR(GroupInfoPair);
  }

  ~GroupInfoPair()
  {
    MOZ_COUNT_DTOR(GroupInfoPair);
  }

private:
  already_AddRefed<GroupInfo>
  LockedGetGroupInfo(PersistenceType aPersistenceType)
  {
    AssertCurrentThreadOwnsQuotaMutex();
    MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

    RefPtr<GroupInfo> groupInfo =
      GetGroupInfoForPersistenceType(aPersistenceType);
    return groupInfo.forget();
  }

  void
  LockedSetGroupInfo(PersistenceType aPersistenceType, GroupInfo* aGroupInfo)
  {
    AssertCurrentThreadOwnsQuotaMutex();
    MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

    RefPtr<GroupInfo>& groupInfo =
      GetGroupInfoForPersistenceType(aPersistenceType);
    groupInfo = aGroupInfo;
  }

  void
  LockedClearGroupInfo(PersistenceType aPersistenceType)
  {
    AssertCurrentThreadOwnsQuotaMutex();
    MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

    RefPtr<GroupInfo>& groupInfo =
      GetGroupInfoForPersistenceType(aPersistenceType);
    groupInfo = nullptr;
  }

  bool
  LockedHasGroupInfos()
  {
    AssertCurrentThreadOwnsQuotaMutex();

    return mTemporaryStorageGroupInfo || mDefaultStorageGroupInfo;
  }

  RefPtr<GroupInfo>&
  GetGroupInfoForPersistenceType(PersistenceType aPersistenceType);

  RefPtr<GroupInfo> mTemporaryStorageGroupInfo;
  RefPtr<GroupInfo> mDefaultStorageGroupInfo;
};

namespace {

class CollectOriginsHelper final
  : public Runnable
{
  uint64_t mMinSizeToBeFreed;

  Mutex& mMutex;
  CondVar mCondVar;

  // The members below are protected by mMutex.
  nsTArray<RefPtr<DirectoryLockImpl>> mLocks;
  uint64_t mSizeToBeFreed;
  bool mWaiting;

public:
  CollectOriginsHelper(mozilla::Mutex& aMutex,
                       uint64_t aMinSizeToBeFreed);

  // Blocks the current thread until origins are collected on the main thread.
  // The returned value contains an aggregate size of those origins.
  int64_t
  BlockAndReturnOriginsForEviction(
                                 nsTArray<RefPtr<DirectoryLockImpl>>& aLocks);

private:
  ~CollectOriginsHelper()
  { }

  NS_IMETHOD
  Run();
};

class OriginOperationBase
  : public BackgroundThreadObject
  , public Runnable
{
protected:
  nsresult mResultCode;

  enum State {
    // Not yet run.
    State_Initial,

    // Running initialization on the main thread.
    State_Initializing,

    // Running initialization on the owning thread.
    State_FinishingInit,

    // Running quota manager initialization on the owning thread.
    State_CreatingQuotaManager,

    // Running on the owning thread in the listener for OpenDirectory.
    State_DirectoryOpenPending,

    // Running on the IO thread.
    State_DirectoryWorkOpen,

    // Running on the owning thread after all work is done.
    State_UnblockingOpen,

    // All done.
    State_Complete
  };

private:
  State mState;
  bool mActorDestroyed;

protected:
  bool mNeedsMainThreadInit;
  bool mNeedsQuotaManagerInit;

public:
  void
  NoteActorDestroyed()
  {
    AssertIsOnOwningThread();

    mActorDestroyed = true;
  }

  bool
  IsActorDestroyed() const
  {
    AssertIsOnOwningThread();

    return mActorDestroyed;
  }

protected:
  explicit OriginOperationBase(
                          nsIEventTarget* aOwningThread = NS_GetCurrentThread())
    : BackgroundThreadObject(aOwningThread)
    , mResultCode(NS_OK)
    , mState(State_Initial)
    , mActorDestroyed(false)
    , mNeedsMainThreadInit(false)
    , mNeedsQuotaManagerInit(false)
  { }

  // Reference counted.
  virtual ~OriginOperationBase()
  {
    MOZ_ASSERT(mState == State_Complete);
    MOZ_ASSERT(mActorDestroyed);
  }

  State
  GetState() const
  {
    return mState;
  }

  void
  SetState(State aState)
  {
    MOZ_ASSERT(mState == State_Initial);
    mState = aState;
  }

  void
  AdvanceState()
  {
    switch (mState) {
      case State_Initial:
        mState = State_Initializing;
        return;
      case State_Initializing:
        mState = State_FinishingInit;
        return;
      case State_FinishingInit:
        mState = State_CreatingQuotaManager;
        return;
      case State_CreatingQuotaManager:
        mState = State_DirectoryOpenPending;
        return;
      case State_DirectoryOpenPending:
        mState = State_DirectoryWorkOpen;
        return;
      case State_DirectoryWorkOpen:
        mState = State_UnblockingOpen;
        return;
      case State_UnblockingOpen:
        mState = State_Complete;
        return;
      default:
        MOZ_CRASH("Bad state!");
    }
  }

  NS_IMETHOD
  Run();

  virtual nsresult
  DoInitOnMainThread()
  {
    return NS_OK;
  }

  virtual void
  Open() = 0;

  nsresult
  DirectoryOpen();

  virtual nsresult
  DoDirectoryWork(QuotaManager* aQuotaManager) = 0;

  void
  Finish(nsresult aResult);

  virtual void
  UnblockOpen() = 0;

private:
  nsresult
  Init();

  nsresult
  InitOnMainThread();

  nsresult
  FinishInit();

  nsresult
  QuotaManagerOpen();

  nsresult
  DirectoryWork();
};

class FinalizeOriginEvictionOp
  : public OriginOperationBase
{
  nsTArray<RefPtr<DirectoryLockImpl>> mLocks;

public:
  FinalizeOriginEvictionOp(nsIEventTarget* aBackgroundThread,
                           nsTArray<RefPtr<DirectoryLockImpl>>& aLocks)
    : OriginOperationBase(aBackgroundThread)
  {
    MOZ_ASSERT(!NS_IsMainThread());

    mLocks.SwapElements(aLocks);
  }

  void
  Dispatch();

  void
  RunOnIOThreadImmediately();

private:
  ~FinalizeOriginEvictionOp()
  { }

  virtual void
  Open() override;

  virtual nsresult
  DoDirectoryWork(QuotaManager* aQuotaManager) override;

  virtual void
  UnblockOpen() override;
};

class NormalOriginOperationBase
  : public OriginOperationBase
  , public OpenDirectoryListener
{
  RefPtr<DirectoryLock> mDirectoryLock;

protected:
  Nullable<PersistenceType> mPersistenceType;
  OriginScope mOriginScope;
  const bool mExclusive;

public:
  void
  RunImmediately()
  {
    MOZ_ASSERT(GetState() == State_Initial);

    MOZ_ALWAYS_SUCCEEDS(this->Run());
  }

protected:
  NormalOriginOperationBase(Nullable<PersistenceType> aPersistenceType,
                            const OriginScope& aOriginScope,
                            bool aExclusive)
    : mPersistenceType(aPersistenceType)
    , mOriginScope(aOriginScope)
    , mExclusive(aExclusive)
  {
    AssertIsOnOwningThread();
  }

  ~NormalOriginOperationBase()
  { }

private:
  NS_DECL_ISUPPORTS_INHERITED

  virtual void
  Open() override;

  virtual void
  UnblockOpen() override;

  // OpenDirectoryListener overrides.
  virtual void
  DirectoryLockAcquired(DirectoryLock* aLock) override;

  virtual void
  DirectoryLockFailed() override;

  // Used to send results before unblocking open.
  virtual void
  SendResults() = 0;
};

class SaveOriginAccessTimeOp
  : public NormalOriginOperationBase
{
  int64_t mTimestamp;

public:
  SaveOriginAccessTimeOp(PersistenceType aPersistenceType,
                         const nsACString& aOrigin,
                         int64_t aTimestamp)
    : NormalOriginOperationBase(Nullable<PersistenceType>(aPersistenceType),
                                OriginScope::FromOrigin(aOrigin),
                                /* aExclusive */ false)
    , mTimestamp(aTimestamp)
  {
    AssertIsOnOwningThread();
  }

private:
  ~SaveOriginAccessTimeOp()
  { }

  virtual nsresult
  DoDirectoryWork(QuotaManager* aQuotaManager) override;

  virtual void
  SendResults() override;
};

/*******************************************************************************
 * Actor class declarations
 ******************************************************************************/

class Quota final
  : public PQuotaParent
{
#ifdef DEBUG
  bool mActorDestroyed;
#endif

public:
  Quota();

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::dom::quota::Quota)

private:
  ~Quota();

  void
  StartIdleMaintenance();

  // IPDL methods.
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override;

  virtual PQuotaUsageRequestParent*
  AllocPQuotaUsageRequestParent(const UsageRequestParams& aParams) override;

  virtual bool
  RecvPQuotaUsageRequestConstructor(PQuotaUsageRequestParent* aActor,
                                    const UsageRequestParams& aParams) override;

  virtual bool
  DeallocPQuotaUsageRequestParent(PQuotaUsageRequestParent* aActor) override;

  virtual PQuotaRequestParent*
  AllocPQuotaRequestParent(const RequestParams& aParams) override;

  virtual bool
  RecvPQuotaRequestConstructor(PQuotaRequestParent* aActor,
                               const RequestParams& aParams) override;

  virtual bool
  DeallocPQuotaRequestParent(PQuotaRequestParent* aActor) override;

  virtual bool
  RecvStartIdleMaintenance() override;

  virtual bool
  RecvStopIdleMaintenance() override;
};

class GetUsageOp final
  : public NormalOriginOperationBase
  , public PQuotaUsageRequestParent
{
  UsageInfo mUsageInfo;

  const UsageParams mParams;
  nsCString mGroup;
  bool mIsApp;

public:
  explicit GetUsageOp(const UsageRequestParams& aParams);

  bool
  Init(Quota* aQuota);

private:
  ~GetUsageOp()
  { }

  virtual nsresult
  DoInitOnMainThread() override;

  nsresult
  AddToUsage(QuotaManager* aQuotaManager,
             PersistenceType aPersistenceType);

  virtual nsresult
  DoDirectoryWork(QuotaManager* aQuotaManager) override;

  virtual void
  SendResults() override;

  // IPDL methods.
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override;

  virtual bool
  RecvCancel() override;
};

class QuotaRequestBase
  : public NormalOriginOperationBase
  , public PQuotaRequestParent
{
public:
  // May be overridden by subclasses if they need to perform work on the
  // background thread before being run.
  virtual bool
  Init(Quota* aQuota);

protected:
  explicit QuotaRequestBase(bool aExclusive)
    : NormalOriginOperationBase(Nullable<PersistenceType>(),
                                OriginScope::FromNull(),
                                aExclusive)
  { }

  // Subclasses use this override to set the IPDL response value.
  virtual void
  GetResponse(RequestResponse& aResponse) = 0;

private:
  virtual void
  SendResults() override;

  // IPDL methods.
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override;
};

class ResetOrClearOp final
  : public QuotaRequestBase
{
  const bool mClear;

public:
  explicit ResetOrClearOp(bool aClear)
    : QuotaRequestBase(/* aExclusive */ true)
    , mClear(aClear)
  {
    AssertIsOnOwningThread();
  }

private:
  ~ResetOrClearOp()
  { }

  void
  DeleteFiles(QuotaManager* aQuotaManager);

  virtual nsresult
  DoDirectoryWork(QuotaManager* aQuotaManager) override;

  virtual void
  GetResponse(RequestResponse& aResponse) override;
};

class OriginClearOp final
  : public QuotaRequestBase
{
  const RequestParams mParams;
  const bool mMultiple;

public:
  explicit OriginClearOp(const RequestParams& aParams);

  virtual bool
  Init(Quota* aQuota) override;

private:
  ~OriginClearOp()
  { }

  virtual nsresult
  DoInitOnMainThread() override;

  void
  DeleteFiles(QuotaManager* aQuotaManager,
              PersistenceType aPersistenceType);

  virtual nsresult
  DoDirectoryWork(QuotaManager* aQuotaManager) override;

  virtual void
  GetResponse(RequestResponse& aResponse) override;
};

/*******************************************************************************
 * Helper Functions
 ******************************************************************************/

template <typename T, bool = mozilla::IsUnsigned<T>::value>
struct IntChecker
{
  static void
  Assert(T aInt)
  {
    static_assert(mozilla::IsIntegral<T>::value, "Not an integer!");
    MOZ_ASSERT(aInt >= 0);
  }
};

template <typename T>
struct IntChecker<T, true>
{
  static void
  Assert(T aInt)
  {
    static_assert(mozilla::IsIntegral<T>::value, "Not an integer!");
  }
};

template <typename T>
void
AssertNoOverflow(uint64_t aDest, T aArg)
{
  IntChecker<T>::Assert(aDest);
  IntChecker<T>::Assert(aArg);
  MOZ_ASSERT(UINT64_MAX - aDest >= uint64_t(aArg));
}

template <typename T, typename U>
void
AssertNoUnderflow(T aDest, U aArg)
{
  IntChecker<T>::Assert(aDest);
  IntChecker<T>::Assert(aArg);
  MOZ_ASSERT(uint64_t(aDest) >= uint64_t(aArg));
}

} // namespace

BackgroundThreadObject::BackgroundThreadObject()
  : mOwningThread(NS_GetCurrentThread())
{
  AssertIsOnOwningThread();
}

BackgroundThreadObject::BackgroundThreadObject(nsIEventTarget* aOwningThread)
  : mOwningThread(aOwningThread)
{
}

#ifdef DEBUG

void
BackgroundThreadObject::AssertIsOnOwningThread() const
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mOwningThread);
  bool current;
  MOZ_ASSERT(NS_SUCCEEDED(mOwningThread->IsOnCurrentThread(&current)));
  MOZ_ASSERT(current);
}

#endif // DEBUG

nsIEventTarget*
BackgroundThreadObject::OwningThread() const
{
  MOZ_ASSERT(mOwningThread);
  return mOwningThread;
}

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

namespace {

StaticRefPtr<QuotaManager> gInstance;
bool gCreateFailed = false;
StaticRefPtr<QuotaManager::CreateRunnable> gCreateRunnable;
mozilla::Atomic<bool> gShutdown(false);

// Constants for temporary storage limit computing.
static const int32_t kDefaultFixedLimitKB = -1;
static const uint32_t kDefaultChunkSizeKB = 10 * 1024;
int32_t gFixedLimitKB = kDefaultFixedLimitKB;
uint32_t gChunkSizeKB = kDefaultChunkSizeKB;

bool gTestingEnabled = false;

class StorageDirectoryHelper
  : public Runnable
{
  mozilla::Mutex mMutex;
  mozilla::CondVar mCondVar;
  nsresult mMainThreadResultCode;
  bool mWaiting;

protected:
  struct OriginProps;

  nsTArray<OriginProps> mOriginProps;

  nsCOMPtr<nsIFile> mDirectory;

public:
  StorageDirectoryHelper(nsIFile* aDirectory)
    : mMutex("StorageDirectoryHelper::mMutex")
    , mCondVar(mMutex, "StorageDirectoryHelper::mCondVar")
    , mMainThreadResultCode(NS_OK)
    , mWaiting(true)
    , mDirectory(aDirectory)
  {
    AssertIsOnIOThread();
  }

protected:
  ~StorageDirectoryHelper()
  { }

  nsresult
  AddOriginDirectory(nsIFile* aDirectory,
                     OriginProps** aOriginProps);

  nsresult
  ProcessOriginDirectories();

  virtual nsresult
  DoProcessOriginDirectories() = 0;

private:
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
  PrincipalOriginAttributes mAttrs;
  int64_t mTimestamp;
  nsCString mSuffix;
  nsCString mGroup;
  nsCString mOrigin;

  Type mType;
  bool mIsApp;
  bool mNeedsRestore;
  bool mIgnore;

public:
  explicit OriginProps()
    : mTimestamp(0)
    , mType(eContent)
    , mIsApp(false)
    , mNeedsRestore(false)
    , mIgnore(false)
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
    eExpectingEmptyTokenOrDriveLetterOrPathnameComponent,
    eExpectingEmptyTokenOrPathnameComponent,
    eComplete,
    eHandledTrailingSeparator
  };

  const nsCString mOrigin;
  const PrincipalOriginAttributes mOriginAttributes;
  Tokenizer mTokenizer;

  uint32_t mAppId;
  nsCString mSchema;
  nsCString mHost;
  Nullable<uint32_t> mPort;
  nsTArray<nsCString> mPathnameComponents;
  nsCString mHandledTokens;

  SchemaType mSchemaType;
  State mState;
  bool mInIsolatedMozBrowser;
  bool mMaybeDriveLetter;
  bool mError;

public:
  OriginParser(const nsACString& aOrigin,
               const PrincipalOriginAttributes& aOriginAttributes)
    : mOrigin(aOrigin)
    , mOriginAttributes(aOriginAttributes)
    , mTokenizer(aOrigin, '+')
    , mAppId(kNoAppId)
    , mPort()
    , mSchemaType(eNone)
    , mState(eExpectingAppIdOrSchema)
    , mInIsolatedMozBrowser(false)
    , mMaybeDriveLetter(false)
    , mError(false)
  { }

  static bool
  ParseOrigin(const nsACString& aOrigin,
              nsCString& aSpec,
              PrincipalOriginAttributes* aAttrs);

  bool
  Parse(nsACString& aSpec, PrincipalOriginAttributes* aAttrs);

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

class CreateOrUpgradeDirectoryMetadataHelper final
  : public StorageDirectoryHelper
{
  const bool mPersistent;

public:
  CreateOrUpgradeDirectoryMetadataHelper(nsIFile* aDirectory,
                                         bool aPersistent)
    : StorageDirectoryHelper(aDirectory)
    , mPersistent(aPersistent)
  { }

  nsresult
  CreateOrUpgradeMetadataFiles();

private:
  nsresult
  MaybeUpgradeOriginDirectory(nsIFile* aDirectory);

  nsresult
  GetDirectoryMetadata(nsIFile* aDirectory,
                       int64_t* aTimestamp,
                       nsACString& aGroup,
                       nsACString& aOrigin,
                       bool* aHasIsApp);

  virtual nsresult
  DoProcessOriginDirectories();
};

class UpgradeDirectoryMetadataFrom1To2Helper final
  : public StorageDirectoryHelper
{
  const bool mPersistent;

public:
  UpgradeDirectoryMetadataFrom1To2Helper(nsIFile* aDirectory,
                                         bool aPersistent)
    : StorageDirectoryHelper(aDirectory)
    , mPersistent(aPersistent)
  { }

  nsresult
  UpgradeMetadataFiles();

private:
  nsresult
  GetDirectoryMetadata(nsIFile* aDirectory,
                       int64_t* aTimestamp,
                       nsACString& aGroup,
                       nsACString& aOrigin,
                       bool* aIsApp);

  virtual nsresult
  DoProcessOriginDirectories() override;
};

class RestoreDirectoryMetadata2Helper final
  : public StorageDirectoryHelper
{
  const bool mPersistent;

public:
  RestoreDirectoryMetadata2Helper(nsIFile* aDirectory,
                                  bool aPersistent)
    : StorageDirectoryHelper(aDirectory)
    , mPersistent(aPersistent)
  { }

  nsresult
  RestoreMetadata2File();

private:
  virtual nsresult
  DoProcessOriginDirectories();
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

void
SanitizeOriginString(nsCString& aOrigin)
{

#ifdef XP_WIN
  NS_ASSERTION(!strcmp(QuotaManager::kReplaceChars,
                       FILE_ILLEGAL_CHARACTERS FILE_PATH_SEPARATOR),
               "Illegal file characters have changed!");
#endif

  aOrigin.ReplaceChar(QuotaManager::kReplaceChars, '+');
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
                 const nsAString& aStorageName,
                 nsAString& aStoragePath)
{
  nsresult rv;

  nsCOMPtr<nsIFile> storageDir;
  rv = aBaseDir->Clone(getter_AddRefs(storageDir));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = storageDir->Append(aStorageName);
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
            leafName.EqualsLiteral(METADATA_V2_FILE_NAME) ||
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
GetOutputStream(nsIFile* aDirectory,
                const nsAString& aFilename,
                FileFlag aFileFlag,
                nsIOutputStream** aStream)
{
  AssertIsOnIOThread();

  nsCOMPtr<nsIFile> file;
  nsresult rv = aDirectory->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = file->Append(aFilename);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIOutputStream> outputStream;
  switch (aFileFlag) {
    case kTruncateFileFlag: {
      rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream),
                                       file);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      break;
    }

    case kUpdateFileFlag: {
      bool exists;
      rv = file->Exists(&exists);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (!exists) {
        *aStream = nullptr;
        return NS_OK;
      }

      nsCOMPtr<nsIFileStream> stream;
      rv = NS_NewLocalFileStream(getter_AddRefs(stream), file);
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
                                       file,
                                       PR_WRONLY | PR_CREATE_FILE | PR_APPEND);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      break;
    }

    default:
      MOZ_CRASH("Should never get here!");
  }

  outputStream.forget(aStream);
  return NS_OK;
}

nsresult
GetBinaryOutputStream(nsIFile* aDirectory,
                      const nsAString& aFilename,
                      FileFlag aFileFlag,
                      nsIBinaryOutputStream** aStream)
{
  nsCOMPtr<nsIOutputStream> outputStream;
  nsresult rv = GetOutputStream(aDirectory,
                                aFilename,
                                aFileFlag,
                                getter_AddRefs(outputStream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
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

void
GetJarPrefix(uint32_t aAppId,
             bool aInIsolatedMozBrowser,
             nsACString& aJarPrefix)
{
  MOZ_ASSERT(aAppId != nsIScriptSecurityManager::UNKNOWN_APP_ID);

  if (aAppId == nsIScriptSecurityManager::UNKNOWN_APP_ID) {
    aAppId = nsIScriptSecurityManager::NO_APP_ID;
  }

  aJarPrefix.Truncate();

  // Fallback.
  if (aAppId == nsIScriptSecurityManager::NO_APP_ID && !aInIsolatedMozBrowser) {
    return;
  }

  // aJarPrefix = appId + "+" + { 't', 'f' } + "+";
  aJarPrefix.AppendInt(aAppId);
  aJarPrefix.Append('+');
  aJarPrefix.Append(aInIsolatedMozBrowser ? 't' : 'f');
  aJarPrefix.Append('+');
}

nsresult
CreateDirectoryMetadata(nsIFile* aDirectory, int64_t aTimestamp,
                        const nsACString& aSuffix, const nsACString& aGroup,
                        const nsACString& aOrigin, bool aIsApp)
{
  AssertIsOnIOThread();

  PrincipalOriginAttributes groupAttributes;

  nsCString groupNoSuffix;
  bool ok = groupAttributes.PopulateFromOrigin(aGroup, groupNoSuffix);
  if (!ok) {
    return NS_ERROR_FAILURE;
  }

  nsCString groupPrefix;
  GetJarPrefix(groupAttributes.mAppId,
               groupAttributes.mInIsolatedMozBrowser,
               groupPrefix);

  nsCString group = groupPrefix + groupNoSuffix;

  PrincipalOriginAttributes originAttributes;

  nsCString originNoSuffix;
  ok = originAttributes.PopulateFromOrigin(aOrigin, originNoSuffix);
  if (!ok) {
    return NS_ERROR_FAILURE;
  }

  nsCString originPrefix;
  GetJarPrefix(originAttributes.mAppId,
               originAttributes.mInIsolatedMozBrowser,
               originPrefix);

  nsCString origin = originPrefix + originNoSuffix;

  MOZ_ASSERT(groupPrefix == originPrefix);

  nsCOMPtr<nsIBinaryOutputStream> stream;
  nsresult rv = GetBinaryOutputStream(aDirectory,
                                      NS_LITERAL_STRING(METADATA_FILE_NAME),
                                      kTruncateFileFlag,
                                      getter_AddRefs(stream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(stream);

  rv = stream->Write64(aTimestamp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stream->WriteStringZ(group.get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stream->WriteStringZ(origin.get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stream->WriteBoolean(aIsApp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
CreateDirectoryMetadata2(nsIFile* aDirectory, int64_t aTimestamp,
                         const nsACString& aSuffix, const nsACString& aGroup,
                         const nsACString& aOrigin, bool aIsApp)
{
  AssertIsOnIOThread();

  nsCOMPtr<nsIBinaryOutputStream> stream;
  nsresult rv = GetBinaryOutputStream(aDirectory,
                                      NS_LITERAL_STRING(METADATA_V2_FILE_NAME),
                                      kTruncateFileFlag,
                                      getter_AddRefs(stream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(stream);

  rv = stream->Write64(aTimestamp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Reserved for navigator.persist()
  rv = stream->WriteBoolean(false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Reserved data 1
  rv = stream->Write32(0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Reserved data 2
  rv = stream->Write32(0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stream->WriteStringZ(PromiseFlatCString(aSuffix).get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stream->WriteStringZ(PromiseFlatCString(aGroup).get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stream->WriteStringZ(PromiseFlatCString(aOrigin).get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stream->WriteBoolean(aIsApp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
GetBinaryInputStream(nsIFile* aDirectory,
                     const nsAString& aFilename,
                     nsIBinaryInputStream** aStream)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(aStream);

  nsCOMPtr<nsIFile> file;
  nsresult rv = aDirectory->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = file->Append(aFilename);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIInputStream> stream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(stream), file);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIInputStream> bufferedStream;
  rv = NS_NewBufferedInputStream(getter_AddRefs(bufferedStream), stream, 512);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIBinaryInputStream> binaryStream =
    do_CreateInstance("@mozilla.org/binaryinputstream;1");
  if (NS_WARN_IF(!binaryStream)) {
    return NS_ERROR_FAILURE;
  }

  rv = binaryStream->SetInputStream(bufferedStream);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  binaryStream.forget(aStream);
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

} // namespace

/*******************************************************************************
 * Exported functions
 ******************************************************************************/

PQuotaParent*
AllocPQuotaParent()
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(QuotaManager::IsShuttingDown())) {
    return nullptr;
  }

  RefPtr<Quota> actor = new Quota();

  return actor.forget().take();
}

bool
DeallocPQuotaParent(PQuotaParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  RefPtr<Quota> actor = dont_AddRef(static_cast<Quota*>(aActor));
  return true;
}

/*******************************************************************************
 * Directory lock
 ******************************************************************************/

DirectoryLockImpl::DirectoryLockImpl(QuotaManager* aQuotaManager,
                                     Nullable<PersistenceType> aPersistenceType,
                                     const nsACString& aGroup,
                                     const OriginScope& aOriginScope,
                                     Nullable<bool> aIsApp,
                                     Nullable<Client::Type> aClientType,
                                     bool aExclusive,
                                     bool aInternal,
                                     OpenDirectoryListener* aOpenListener)
  : mQuotaManager(aQuotaManager)
  , mPersistenceType(aPersistenceType)
  , mGroup(aGroup)
  , mOriginScope(aOriginScope)
  , mIsApp(aIsApp)
  , mClientType(aClientType)
  , mOpenListener(aOpenListener)
  , mExclusive(aExclusive)
  , mInternal(aInternal)
  , mInvalidated(false)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aQuotaManager);
  MOZ_ASSERT_IF(aOriginScope.IsOrigin(), !aOriginScope.GetOrigin().IsEmpty());
  MOZ_ASSERT_IF(!aInternal, !aPersistenceType.IsNull());
  MOZ_ASSERT_IF(!aInternal,
                aPersistenceType.Value() != PERSISTENCE_TYPE_INVALID);
  MOZ_ASSERT_IF(!aInternal, !aGroup.IsEmpty());
  MOZ_ASSERT_IF(!aInternal, aOriginScope.IsOrigin());
  MOZ_ASSERT_IF(!aInternal, !aIsApp.IsNull());
  MOZ_ASSERT_IF(!aInternal, !aClientType.IsNull());
  MOZ_ASSERT_IF(!aInternal, aClientType.Value() != Client::TYPE_MAX);
  MOZ_ASSERT_IF(!aInternal, aOpenListener);
}

DirectoryLockImpl::~DirectoryLockImpl()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mQuotaManager);

  for (DirectoryLockImpl* blockingLock : mBlocking) {
    blockingLock->MaybeUnblock(this);
  }

  mBlocking.Clear();

  mQuotaManager->UnregisterDirectoryLock(this);
}

#ifdef DEBUG

void
DirectoryLockImpl::AssertIsOnOwningThread() const
{
  MOZ_ASSERT(mQuotaManager);
  mQuotaManager->AssertIsOnOwningThread();
}

#endif // DEBUG

bool
DirectoryLockImpl::MustWaitFor(const DirectoryLockImpl& aExistingLock)
{
  AssertIsOnOwningThread();

  // Waiting is never required if the ops in comparison represent shared locks.
  if (!aExistingLock.mExclusive && !mExclusive) {
    return false;
  }

  // If the persistence types don't overlap, the op can proceed.
  if (!aExistingLock.mPersistenceType.IsNull() && !mPersistenceType.IsNull() &&
      aExistingLock.mPersistenceType.Value() != mPersistenceType.Value()) {
    return false;
  }

  // If the origin scopes don't overlap, the op can proceed.
  bool match = aExistingLock.mOriginScope.Matches(mOriginScope);
  if (!match) {
    return false;
  }

  // If the client types don't overlap, the op can proceed.
  if (!aExistingLock.mClientType.IsNull() && !mClientType.IsNull() &&
      aExistingLock.mClientType.Value() != mClientType.Value()) {
    return false;
  }

  // Otherwise, when all attributes overlap (persistence type, origin scope and
  // client type) the op must wait.
  return true;
}

void
DirectoryLockImpl::NotifyOpenListener()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mQuotaManager);
  MOZ_ASSERT(mOpenListener);

  if (mInvalidated) {
    mOpenListener->DirectoryLockFailed();
  } else {
    mOpenListener->DirectoryLockAcquired(this);
  }

  mOpenListener = nullptr;

  mQuotaManager->RemovePendingDirectoryLock(this);
}

nsresult
QuotaManager::
CreateRunnable::Init()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State::Initial);

  nsresult rv;

  nsCOMPtr<nsIFile> baseDir;
  rv = NS_GetSpecialDirectory(NS_APP_INDEXEDDB_PARENT_DIR,
                              getter_AddRefs(baseDir));
  if (NS_FAILED(rv)) {
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                getter_AddRefs(baseDir));
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = baseDir->GetPath(mBaseDirPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
QuotaManager::
CreateRunnable::CreateManager()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::CreatingManager);

  mManager = new QuotaManager();

  nsresult rv = mManager->Init(mBaseDirPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
QuotaManager::
CreateRunnable::RegisterObserver()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State::RegisteringObserver);

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

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (NS_WARN_IF(!observerService)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIObserver> observer = new ShutdownObserver(mOwningThread);

  nsresult rv = observerService->AddObserver(observer,
                                             PROFILE_BEFORE_CHANGE_OBSERVER_ID,
                                             false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // This service has to be started on the main thread currently.
  nsCOMPtr<mozIStorageService> ss =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  QuotaManagerService* qms = QuotaManagerService::GetOrCreate();
  if (NS_WARN_IF(!qms)) {
    return rv;
  }

  qms->NoteLiveManager(mManager);

  return NS_OK;
}

void
QuotaManager::
CreateRunnable::CallCallbacks()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::CallingCallbacks);

  gCreateRunnable = nullptr;

  if (NS_FAILED(mResultCode)) {
    gCreateFailed = true;
  } else {
    gInstance = mManager;
  }

  mManager = nullptr;

  nsTArray<nsCOMPtr<nsIRunnable>> callbacks;
  mCallbacks.SwapElements(callbacks);

  for (nsCOMPtr<nsIRunnable>& callback : callbacks) {
    Unused << callback->Run();
  }
}

auto
QuotaManager::
CreateRunnable::GetNextState(nsCOMPtr<nsIEventTarget>& aThread) -> State
{
  switch (mState) {
    case State::Initial:
      aThread = mOwningThread;
      return State::CreatingManager;
    case State::CreatingManager:
      aThread = do_GetMainThread();
      return State::RegisteringObserver;
    case State::RegisteringObserver:
      aThread = mOwningThread;
      return State::CallingCallbacks;
    case State::CallingCallbacks:
      aThread = nullptr;
      return State::Completed;
    default:
      MOZ_CRASH("Bad state!");
  }
}

NS_IMETHODIMP
QuotaManager::
CreateRunnable::Run()
{
  nsresult rv;

  switch (mState) {
    case State::Initial:
      rv = Init();
      break;

    case State::CreatingManager:
      rv = CreateManager();
      break;

    case State::RegisteringObserver:
      rv = RegisterObserver();
      break;

    case State::CallingCallbacks:
      CallCallbacks();
      rv = NS_OK;
      break;

    case State::Completed:
    default:
      MOZ_CRASH("Bad state!");
  }

  nsCOMPtr<nsIEventTarget> thread;
  if (NS_WARN_IF(NS_FAILED(rv))) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = rv;
    }

    mState = State::CallingCallbacks;
    thread = mOwningThread;
  } else {
    mState = GetNextState(thread);
  }

  if (thread) {
    MOZ_ALWAYS_SUCCEEDS(thread->Dispatch(this, NS_DISPATCH_NORMAL));
  }

  return NS_OK;
}

NS_IMETHODIMP
QuotaManager::
ShutdownRunnable::Run()
{
  if (NS_IsMainThread()) {
    QuotaManagerService* qms = QuotaManagerService::Get();
    MOZ_ASSERT(qms);

    qms->NoteFinishedManager();

    mDone = true;

    return NS_OK;
  }

  AssertIsOnBackgroundThread();

  RefPtr<QuotaManager> quotaManager = gInstance.get();
  if (quotaManager) {
    quotaManager->Shutdown();

    gInstance = nullptr;
  }

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(this));

  return NS_OK;
}

NS_IMPL_ISUPPORTS(QuotaManager::ShutdownObserver, nsIObserver)

NS_IMETHODIMP
QuotaManager::
ShutdownObserver::Observe(nsISupports* aSubject,
                          const char* aTopic,
                          const char16_t* aData)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!strcmp(aTopic, PROFILE_BEFORE_CHANGE_OBSERVER_ID));

  bool done = false;

  RefPtr<ShutdownRunnable> shutdownRunnable = new ShutdownRunnable(done);
  MOZ_ALWAYS_SUCCEEDS(
    mBackgroundThread->Dispatch(shutdownRunnable, NS_DISPATCH_NORMAL));

  nsIThread* currentThread = NS_GetCurrentThread();
  MOZ_ASSERT(currentThread);

  while (!done) {
    MOZ_ALWAYS_TRUE(NS_ProcessNextEvent(currentThread));
  }

  return NS_OK;
}

/*******************************************************************************
 * Quota object
 ******************************************************************************/

void
QuotaObject::AddRef()
{
  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    NS_ERROR("Null quota manager, this shouldn't happen, possible leak!");

    ++mRefCnt;

    return;
  }

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  ++mRefCnt;
}

void
QuotaObject::Release()
{
  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    NS_ERROR("Null quota manager, this shouldn't happen, possible leak!");

    nsrefcnt count = --mRefCnt;
    if (count == 0) {
      mRefCnt = 1;
      delete this;
    }

    return;
  }

  {
    MutexAutoLock lock(quotaManager->mQuotaMutex);

    --mRefCnt;

    if (mRefCnt > 0) {
      return;
    }

    if (mOriginInfo) {
      mOriginInfo->mQuotaObjects.Remove(mPath);
    }
  }

  delete this;
}

bool
QuotaObject::MaybeUpdateSize(int64_t aSize, bool aTruncate)
{
  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  if (mQuotaCheckDisabled) {
    return true;
  }

  if (mSize == aSize) {
    return true;
  }

  if (!mOriginInfo) {
    mSize = aSize;
    return true;
  }

  GroupInfo* groupInfo = mOriginInfo->mGroupInfo;
  MOZ_ASSERT(groupInfo);

  if (mSize > aSize) {
    if (aTruncate) {
      const int64_t delta = mSize - aSize;

      AssertNoUnderflow(quotaManager->mTemporaryStorageUsage, delta);
      quotaManager->mTemporaryStorageUsage -= delta;

      AssertNoUnderflow(groupInfo->mUsage, delta);
      groupInfo->mUsage -= delta;

      AssertNoUnderflow(mOriginInfo->mUsage, delta);
      mOriginInfo->mUsage -= delta;

      mSize = aSize;
    }
    return true;
  }

  MOZ_ASSERT(mSize < aSize);

  RefPtr<GroupInfo> complementaryGroupInfo =
    groupInfo->mGroupInfoPair->LockedGetGroupInfo(
      ComplementaryPersistenceType(groupInfo->mPersistenceType));

  uint64_t delta = aSize - mSize;

  AssertNoOverflow(mOriginInfo->mUsage, delta);
  uint64_t newUsage = mOriginInfo->mUsage + delta;

  // Temporary storage has no limit for origin usage (there's a group and the
  // global limit though).

  AssertNoOverflow(groupInfo->mUsage, delta);
  uint64_t newGroupUsage = groupInfo->mUsage + delta;

  uint64_t groupUsage = groupInfo->mUsage;
  if (complementaryGroupInfo) {
    AssertNoOverflow(groupUsage, complementaryGroupInfo->mUsage);
    groupUsage += complementaryGroupInfo->mUsage;
  }

  // Temporary storage has a hard limit for group usage (20 % of the global
  // limit).
  AssertNoOverflow(groupUsage, delta);
  if (groupUsage + delta > quotaManager->GetGroupLimit()) {
    return false;
  }

  AssertNoOverflow(quotaManager->mTemporaryStorageUsage, delta);
  uint64_t newTemporaryStorageUsage = quotaManager->mTemporaryStorageUsage +
                                      delta;

  if (newTemporaryStorageUsage > quotaManager->mTemporaryStorageLimit) {
    // This will block the thread without holding the lock while waitting.

    AutoTArray<RefPtr<DirectoryLockImpl>, 10> locks;

    uint64_t sizeToBeFreed =
      quotaManager->LockedCollectOriginsForEviction(delta, locks);

    if (!sizeToBeFreed) {
      return false;
    }

    NS_ASSERTION(sizeToBeFreed >= delta, "Huh?");

    {
      MutexAutoUnlock autoUnlock(quotaManager->mQuotaMutex);

      for (RefPtr<DirectoryLockImpl>& lock : locks) {
        MOZ_ASSERT(!lock->GetPersistenceType().IsNull());
        MOZ_ASSERT(lock->GetOriginScope().IsOrigin());
        MOZ_ASSERT(!lock->GetOriginScope().GetOrigin().IsEmpty());

        quotaManager->DeleteFilesForOrigin(lock->GetPersistenceType().Value(),
                                           lock->GetOriginScope().GetOrigin());
      }
    }

    // Relocked.

    NS_ASSERTION(mOriginInfo, "How come?!");

    for (DirectoryLockImpl* lock : locks) {
      MOZ_ASSERT(!lock->GetPersistenceType().IsNull());
      MOZ_ASSERT(!lock->GetGroup().IsEmpty());
      MOZ_ASSERT(lock->GetOriginScope().IsOrigin());
      MOZ_ASSERT(!lock->GetOriginScope().GetOrigin().IsEmpty());
      MOZ_ASSERT(lock->GetOriginScope().GetOrigin() != mOriginInfo->mOrigin,
                 "Deleted itself!");

      quotaManager->LockedRemoveQuotaForOrigin(
                                             lock->GetPersistenceType().Value(),
                                             lock->GetGroup(),
                                             lock->GetOriginScope().GetOrigin());
    }

    // We unlocked and relocked several times so we need to recompute all the
    // essential variables and recheck the group limit.

    AssertNoUnderflow(aSize, mSize);
    delta = aSize - mSize;

    AssertNoOverflow(mOriginInfo->mUsage, delta);
    newUsage = mOriginInfo->mUsage + delta;

    AssertNoOverflow(groupInfo->mUsage, delta);
    newGroupUsage = groupInfo->mUsage + delta;

    groupUsage = groupInfo->mUsage;
    if (complementaryGroupInfo) {
      AssertNoOverflow(groupUsage, complementaryGroupInfo->mUsage);
      groupUsage += complementaryGroupInfo->mUsage;
    }

    AssertNoOverflow(groupUsage, delta);
    if (groupUsage + delta > quotaManager->GetGroupLimit()) {
      // Unfortunately some other thread increased the group usage in the
      // meantime and we are not below the group limit anymore.

      // However, the origin eviction must be finalized in this case too.
      MutexAutoUnlock autoUnlock(quotaManager->mQuotaMutex);

      quotaManager->FinalizeOriginEviction(locks);

      return false;
    }

    AssertNoOverflow(quotaManager->mTemporaryStorageUsage, delta);
    newTemporaryStorageUsage = quotaManager->mTemporaryStorageUsage + delta;

    NS_ASSERTION(newTemporaryStorageUsage <=
                 quotaManager->mTemporaryStorageLimit, "How come?!");

    // Ok, we successfully freed enough space and the operation can continue
    // without throwing the quota error.
    mOriginInfo->mUsage = newUsage;
    groupInfo->mUsage = newGroupUsage;
    quotaManager->mTemporaryStorageUsage = newTemporaryStorageUsage;;

    // Some other thread could increase the size in the meantime, but no more
    // than this one.
    MOZ_ASSERT(mSize < aSize);
    mSize = aSize;

    // Finally, release IO thread only objects and allow next synchronized
    // ops for the evicted origins.
    MutexAutoUnlock autoUnlock(quotaManager->mQuotaMutex);

    quotaManager->FinalizeOriginEviction(locks);

    return true;
  }

  mOriginInfo->mUsage = newUsage;
  groupInfo->mUsage = newGroupUsage;
  quotaManager->mTemporaryStorageUsage = newTemporaryStorageUsage;

  mSize = aSize;

  return true;
}

void
QuotaObject::DisableQuotaCheck()
{
  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  mQuotaCheckDisabled = true;
}

void
QuotaObject::EnableQuotaCheck()
{
  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  MutexAutoLock lock(quotaManager->mQuotaMutex);

  mQuotaCheckDisabled = false;
}

/*******************************************************************************
 * Quota manager
 ******************************************************************************/

QuotaManager::QuotaManager()
: mQuotaMutex("QuotaManager.mQuotaMutex"),
  mTemporaryStorageLimit(0),
  mTemporaryStorageUsage(0),
  mTemporaryStorageInitialized(false),
  mStorageInitialized(false)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!gInstance);
}

QuotaManager::~QuotaManager()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!gInstance || gInstance == this);
}

void
QuotaManager::GetOrCreate(nsIRunnable* aCallback)
{
  AssertIsOnBackgroundThread();

  if (IsShuttingDown()) {
    MOZ_ASSERT(false, "Calling GetOrCreate() after shutdown!");
    return;
  }

  if (gInstance || gCreateFailed) {
    MOZ_ASSERT(!gCreateRunnable);
    MOZ_ASSERT_IF(gCreateFailed, !gInstance);

    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(aCallback));
  } else {
    if (!gCreateRunnable) {
      gCreateRunnable = new CreateRunnable();
      MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(gCreateRunnable));
    }

    gCreateRunnable->AddCallback(aCallback);
  }
}

// static
QuotaManager*
QuotaManager::Get()
{
  MOZ_ASSERT(!NS_IsMainThread());

  // Does not return an owning reference.
  return gInstance;
}

// static
bool
QuotaManager::IsShuttingDown()
{
  return gShutdown;
}

auto
QuotaManager::CreateDirectoryLock(Nullable<PersistenceType> aPersistenceType,
                                  const nsACString& aGroup,
                                  const OriginScope& aOriginScope,
                                  Nullable<bool> aIsApp,
                                  Nullable<Client::Type> aClientType,
                                  bool aExclusive,
                                  bool aInternal,
                                  OpenDirectoryListener* aOpenListener)
  -> already_AddRefed<DirectoryLockImpl>
{
  AssertIsOnOwningThread();
  MOZ_ASSERT_IF(aOriginScope.IsOrigin(), !aOriginScope.GetOrigin().IsEmpty());
  MOZ_ASSERT_IF(!aInternal, !aPersistenceType.IsNull());
  MOZ_ASSERT_IF(!aInternal,
                aPersistenceType.Value() != PERSISTENCE_TYPE_INVALID);
  MOZ_ASSERT_IF(!aInternal, !aGroup.IsEmpty());
  MOZ_ASSERT_IF(!aInternal, aOriginScope.IsOrigin());
  MOZ_ASSERT_IF(!aInternal, !aIsApp.IsNull());
  MOZ_ASSERT_IF(!aInternal, !aClientType.IsNull());
  MOZ_ASSERT_IF(!aInternal, aClientType.Value() != Client::TYPE_MAX);
  MOZ_ASSERT_IF(!aInternal, aOpenListener);

  RefPtr<DirectoryLockImpl> lock = new DirectoryLockImpl(this,
                                                           aPersistenceType,
                                                           aGroup,
                                                           aOriginScope,
                                                           aIsApp,
                                                           aClientType,
                                                           aExclusive,
                                                           aInternal,
                                                           aOpenListener);

  mPendingDirectoryLocks.AppendElement(lock);

  // See if this lock needs to wait.
  bool blocked = false;
  for (uint32_t index = mDirectoryLocks.Length(); index > 0; index--) {
    DirectoryLockImpl* existingLock = mDirectoryLocks[index - 1];
    if (lock->MustWaitFor(*existingLock)) {
      existingLock->AddBlockingLock(lock);
      lock->AddBlockedOnLock(existingLock);
      blocked = true;
    }
  }

  RegisterDirectoryLock(lock);

  // Otherwise, notify the open listener immediately.
  if (!blocked) {
    lock->NotifyOpenListener();
  }

  return lock.forget();
}

auto
QuotaManager::CreateDirectoryLockForEviction(PersistenceType aPersistenceType,
                                             const nsACString& aGroup,
                                             const nsACString& aOrigin,
                                             bool aIsApp)
  -> already_AddRefed<DirectoryLockImpl>
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_INVALID);
  MOZ_ASSERT(!aOrigin.IsEmpty());

  RefPtr<DirectoryLockImpl> lock =
    new DirectoryLockImpl(this,
                          Nullable<PersistenceType>(aPersistenceType),
                          aGroup,
                          OriginScope::FromOrigin(aOrigin),
                          Nullable<bool>(aIsApp),
                          Nullable<Client::Type>(),
                          /* aExclusive */ true,
                          /* aInternal */ true,
                          nullptr);

#ifdef DEBUG
  for (uint32_t index = mDirectoryLocks.Length(); index > 0; index--) {
    DirectoryLockImpl* existingLock = mDirectoryLocks[index - 1];
    MOZ_ASSERT(!lock->MustWaitFor(*existingLock));
  }
#endif

  RegisterDirectoryLock(lock);

  return lock.forget();
}

void
QuotaManager::RegisterDirectoryLock(DirectoryLockImpl* aLock)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aLock);

  mDirectoryLocks.AppendElement(aLock);

  if (aLock->ShouldUpdateLockTable()) {
    const Nullable<PersistenceType>& persistenceType =
      aLock->GetPersistenceType();
    const OriginScope& originScope = aLock->GetOriginScope();

    MOZ_ASSERT(!persistenceType.IsNull());
    MOZ_ASSERT(!aLock->GetGroup().IsEmpty());
    MOZ_ASSERT(originScope.IsOrigin());
    MOZ_ASSERT(!originScope.GetOrigin().IsEmpty());

    DirectoryLockTable& directoryLockTable =
      GetDirectoryLockTable(persistenceType.Value());

    nsTArray<DirectoryLockImpl*>* array;
    if (!directoryLockTable.Get(originScope.GetOrigin(), &array)) {
      array = new nsTArray<DirectoryLockImpl*>();
      directoryLockTable.Put(originScope.GetOrigin(), array);

      if (!IsShuttingDown()) {
        UpdateOriginAccessTime(persistenceType.Value(),
                               aLock->GetGroup(),
                               originScope.GetOrigin());
      }
    }
    array->AppendElement(aLock);
  }
}

void
QuotaManager::UnregisterDirectoryLock(DirectoryLockImpl* aLock)
{
  AssertIsOnOwningThread();

  MOZ_ALWAYS_TRUE(mDirectoryLocks.RemoveElement(aLock));

  if (aLock->ShouldUpdateLockTable()) {
    const Nullable<PersistenceType>& persistenceType =
      aLock->GetPersistenceType();
    const OriginScope& originScope = aLock->GetOriginScope();

    MOZ_ASSERT(!persistenceType.IsNull());
    MOZ_ASSERT(!aLock->GetGroup().IsEmpty());
    MOZ_ASSERT(originScope.IsOrigin());
    MOZ_ASSERT(!originScope.GetOrigin().IsEmpty());

    DirectoryLockTable& directoryLockTable =
      GetDirectoryLockTable(persistenceType.Value());

    nsTArray<DirectoryLockImpl*>* array;
    MOZ_ALWAYS_TRUE(directoryLockTable.Get(originScope.GetOrigin(), &array));

    MOZ_ALWAYS_TRUE(array->RemoveElement(aLock));
    if (array->IsEmpty()) {
      directoryLockTable.Remove(originScope.GetOrigin());

      if (!IsShuttingDown()) {
        UpdateOriginAccessTime(persistenceType.Value(),
                               aLock->GetGroup(),
                               originScope.GetOrigin());
      }
    }
  }
}

void
QuotaManager::RemovePendingDirectoryLock(DirectoryLockImpl* aLock)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aLock);

  MOZ_ALWAYS_TRUE(mPendingDirectoryLocks.RemoveElement(aLock));
}

uint64_t
QuotaManager::CollectOriginsForEviction(
                                  uint64_t aMinSizeToBeFreed,
                                  nsTArray<RefPtr<DirectoryLockImpl>>& aLocks)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aLocks.IsEmpty());

  struct MOZ_STACK_CLASS Helper final
  {
    static void
    GetInactiveOriginInfos(nsTArray<RefPtr<OriginInfo>>& aOriginInfos,
                           nsTArray<DirectoryLockImpl*>& aLocks,
                           nsTArray<OriginInfo*>& aInactiveOriginInfos)
    {
      for (OriginInfo* originInfo : aOriginInfos) {
        MOZ_ASSERT(IsTreatedAsTemporary(originInfo->mGroupInfo->mPersistenceType,
                                        originInfo->mIsApp));

        OriginScope originScope = OriginScope::FromOrigin(originInfo->mOrigin);

        bool match = false;
        for (uint32_t j = aLocks.Length(); j > 0; j--) {
          DirectoryLockImpl* lock = aLocks[j - 1];
          if (originScope.Matches(lock->GetOriginScope())) {
            match = true;
            break;
          }
        }

        if (!match) {
          MOZ_ASSERT(!originInfo->mQuotaObjects.Count(),
                     "Inactive origin shouldn't have open files!");
          aInactiveOriginInfos.InsertElementSorted(originInfo,
                                                   OriginInfoLRUComparator());
        }
      }
    }
  };

  // Split locks into separate arrays and filter out locks for persistent
  // storage, they can't block us.
  nsTArray<DirectoryLockImpl*> temporaryStorageLocks;
  nsTArray<DirectoryLockImpl*> defaultStorageLocks;
  for (DirectoryLockImpl* lock : mDirectoryLocks) {
    const Nullable<PersistenceType>& persistenceType =
      lock->GetPersistenceType();

    if (persistenceType.IsNull()) {
      temporaryStorageLocks.AppendElement(lock);
      defaultStorageLocks.AppendElement(lock);
    } else if (persistenceType.Value() == PERSISTENCE_TYPE_TEMPORARY) {
      temporaryStorageLocks.AppendElement(lock);
    } else if (persistenceType.Value() == PERSISTENCE_TYPE_DEFAULT) {
      defaultStorageLocks.AppendElement(lock);
    } else {
      MOZ_ASSERT(persistenceType.Value() == PERSISTENCE_TYPE_PERSISTENT);

      // Do nothing here, persistent origins don't need to be collected ever.
    }
  }

  nsTArray<OriginInfo*> inactiveOrigins;

  // Enumerate and process inactive origins. This must be protected by the
  // mutex.
  MutexAutoLock lock(mQuotaMutex);

  for (auto iter = mGroupInfoPairs.Iter(); !iter.Done(); iter.Next()) {
    GroupInfoPair* pair = iter.UserData();

    MOZ_ASSERT(!iter.Key().IsEmpty());
    MOZ_ASSERT(pair);

    RefPtr<GroupInfo> groupInfo =
      pair->LockedGetGroupInfo(PERSISTENCE_TYPE_TEMPORARY);
    if (groupInfo) {
      Helper::GetInactiveOriginInfos(groupInfo->mOriginInfos,
                                     temporaryStorageLocks,
                                     inactiveOrigins);
    }

    groupInfo = pair->LockedGetGroupInfo(PERSISTENCE_TYPE_DEFAULT);
    if (groupInfo) {
      Helper::GetInactiveOriginInfos(groupInfo->mOriginInfos,
                                     defaultStorageLocks,
                                     inactiveOrigins);
    }
  }

#ifdef DEBUG
  // Make sure the array is sorted correctly.
  for (uint32_t index = inactiveOrigins.Length(); index > 1; index--) {
    MOZ_ASSERT(inactiveOrigins[index - 1]->mAccessTime >=
               inactiveOrigins[index - 2]->mAccessTime);
  }
#endif

  // Create a list of inactive and the least recently used origins
  // whose aggregate size is greater or equals the minimal size to be freed.
  uint64_t sizeToBeFreed = 0;
  for(uint32_t count = inactiveOrigins.Length(), index = 0;
      index < count;
      index++) {
    if (sizeToBeFreed >= aMinSizeToBeFreed) {
      inactiveOrigins.TruncateLength(index);
      break;
    }

    sizeToBeFreed += inactiveOrigins[index]->mUsage;
  }

  if (sizeToBeFreed >= aMinSizeToBeFreed) {
    // Success, add directory locks for these origins, so any other
    // operations for them will be delayed (until origin eviction is finalized).

    for (OriginInfo* originInfo : inactiveOrigins) {
      RefPtr<DirectoryLockImpl> lock =
        CreateDirectoryLockForEviction(originInfo->mGroupInfo->mPersistenceType,
                                       originInfo->mGroupInfo->mGroup,
                                       originInfo->mOrigin,
                                       originInfo->mIsApp);
      aLocks.AppendElement(lock.forget());
    }

    return sizeToBeFreed;
  }

  return 0;
}

nsresult
QuotaManager::Init(const nsAString& aBasePath)
{
  nsresult rv;

  mBasePath = aBasePath;

  nsCOMPtr<nsIFile> baseDir = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = baseDir->InitWithPath(aBasePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = CloneStoragePath(baseDir,
                        NS_LITERAL_STRING(INDEXEDDB_DIRECTORY_NAME),
                        mIndexedDBPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = baseDir->Append(NS_LITERAL_STRING(STORAGE_DIRECTORY_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = baseDir->GetPath(mStoragePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = CloneStoragePath(baseDir,
                        NS_LITERAL_STRING(PERMANENT_DIRECTORY_NAME),
                        mPermanentStoragePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = CloneStoragePath(baseDir,
                        NS_LITERAL_STRING(TEMPORARY_DIRECTORY_NAME),
                        mTemporaryStoragePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = CloneStoragePath(baseDir,
                        NS_LITERAL_STRING(DEFAULT_DIRECTORY_NAME),
                        mDefaultStoragePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Make a lazy thread for any IO we need (like clearing or enumerating the
  // contents of storage directories).
  mIOThread = new LazyIdleThread(DEFAULT_THREAD_TIMEOUT_MS,
                                 NS_LITERAL_CSTRING("Storage I/O"),
                                 LazyIdleThread::ManualShutdown);

  // Make a timer here to avoid potential failures later. We don't actually
  // initialize the timer until shutdown.
  mShutdownTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  if (NS_WARN_IF(!mShutdownTimer)) {
    return NS_ERROR_FAILURE;
  }

  static_assert(Client::IDB == 0 && Client::ASMJS == 1 && Client::DOMCACHE == 2 &&
                Client::TYPE_MAX == 3, "Fix the registration!");

  MOZ_ASSERT(mClients.Capacity() == Client::TYPE_MAX,
             "Should be using an auto array with correct capacity!");

  // Register clients.
  mClients.AppendElement(indexedDB::CreateQuotaClient());
  mClients.AppendElement(asmjscache::CreateClient());
  mClients.AppendElement(cache::CreateQuotaClient());

  return NS_OK;
}

void
QuotaManager::Shutdown()
{
  AssertIsOnOwningThread();

  // Setting this flag prevents the service from being recreated and prevents
  // further storagess from being created.
  if (gShutdown.exchange(true)) {
    NS_ERROR("Shutdown more than once?!");
  }

  StopIdleMaintenance();

  // Kick off the shutdown timer.
  MOZ_ALWAYS_SUCCEEDS(
    mShutdownTimer->InitWithFuncCallback(&ShutdownTimerCallback,
                                         this,
                                         DEFAULT_SHUTDOWN_TIMER_MS,
                                         nsITimer::TYPE_ONE_SHOT));

  // Each client will spin the event loop while we wait on all the threads
  // to close. Our timer may fire during that loop.
  for (uint32_t index = 0; index < Client::TYPE_MAX; index++) {
    mClients[index]->ShutdownWorkThreads();
  }

  // Cancel the timer regardless of whether it actually fired.
  if (NS_FAILED(mShutdownTimer->Cancel())) {
    NS_WARNING("Failed to cancel shutdown timer!");
  }

  // NB: It's very important that runnable is destroyed on this thread
  // (i.e. after we join the IO thread) because we can't release the
  // QuotaManager on the IO thread. This should probably use
  // NewNonOwningRunnableMethod ...
  RefPtr<Runnable> runnable =
    NewRunnableMethod(this, &QuotaManager::ReleaseIOThreadObjects);
  MOZ_ASSERT(runnable);

  // Give clients a chance to cleanup IO thread only objects.
  if (NS_FAILED(mIOThread->Dispatch(runnable, NS_DISPATCH_NORMAL))) {
    NS_WARNING("Failed to dispatch runnable!");
  }

  // Make sure to join with our IO thread.
  if (NS_FAILED(mIOThread->Shutdown())) {
    NS_WARNING("Failed to shutdown IO thread!");
  }

  for (RefPtr<DirectoryLockImpl>& lock : mPendingDirectoryLocks) {
    lock->Invalidate();
  }
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

  RefPtr<GroupInfo> groupInfo = pair->LockedGetGroupInfo(aPersistenceType);
  if (!groupInfo) {
    groupInfo = new GroupInfo(pair, aPersistenceType, aGroup);
    pair->LockedSetGroupInfo(aPersistenceType, groupInfo);
  }

  RefPtr<OriginInfo> originInfo =
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

  RefPtr<GroupInfo> groupInfo = pair->LockedGetGroupInfo(aPersistenceType);
  if (!groupInfo) {
    return;
  }

  RefPtr<OriginInfo> originInfo = groupInfo->LockedGetOriginInfo(aOrigin);
  if (originInfo) {
    originInfo->LockedDecreaseUsage(aSize);
  }
}

void
QuotaManager::UpdateOriginAccessTime(PersistenceType aPersistenceType,
                                     const nsACString& aGroup,
                                     const nsACString& aOrigin)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

  MutexAutoLock lock(mQuotaMutex);

  GroupInfoPair* pair;
  if (!mGroupInfoPairs.Get(aGroup, &pair)) {
    return;
  }

  RefPtr<GroupInfo> groupInfo = pair->LockedGetGroupInfo(aPersistenceType);
  if (!groupInfo) {
    return;
  }

  RefPtr<OriginInfo> originInfo = groupInfo->LockedGetOriginInfo(aOrigin);
  if (originInfo) {
    int64_t timestamp = PR_Now();
    originInfo->LockedUpdateAccessTime(timestamp);

    MutexAutoUnlock autoUnlock(mQuotaMutex);

    RefPtr<SaveOriginAccessTimeOp> op =
      new SaveOriginAccessTimeOp(aPersistenceType, aOrigin, timestamp);

    op->RunImmediately();
  }
}

void
QuotaManager::RemoveQuota()
{
  MutexAutoLock lock(mQuotaMutex);

  for (auto iter = mGroupInfoPairs.Iter(); !iter.Done(); iter.Next()) {
    nsAutoPtr<GroupInfoPair>& pair = iter.Data();

    MOZ_ASSERT(!iter.Key().IsEmpty(), "Empty key!");
    MOZ_ASSERT(pair, "Null pointer!");

    RefPtr<GroupInfo> groupInfo =
      pair->LockedGetGroupInfo(PERSISTENCE_TYPE_TEMPORARY);
    if (groupInfo) {
      groupInfo->LockedRemoveOriginInfos();
    }

    groupInfo = pair->LockedGetGroupInfo(PERSISTENCE_TYPE_DEFAULT);
    if (groupInfo) {
      groupInfo->LockedRemoveOriginInfos();
    }

    iter.Remove();
  }

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

  RefPtr<QuotaObject> result;
  {
    MutexAutoLock lock(mQuotaMutex);

    GroupInfoPair* pair;
    if (!mGroupInfoPairs.Get(group, &pair)) {
      return nullptr;
    }

    RefPtr<GroupInfo> groupInfo =
      pair->LockedGetGroupInfo(aPersistenceType);

    if (!groupInfo) {
      return nullptr;
    }

    RefPtr<OriginInfo> originInfo = groupInfo->LockedGetOriginInfo(origin);

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

void
QuotaManager::AbortOperationsForProcess(ContentParentId aContentParentId)
{
  AssertIsOnOwningThread();

  for (RefPtr<Client>& client : mClients) {
    client->AbortOperationsForProcess(aContentParentId);
  }
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
QuotaManager::RestoreDirectoryMetadata2(nsIFile* aDirectory, bool aPersistent)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(mStorageInitialized);

  RefPtr<RestoreDirectoryMetadata2Helper> helper =
    new RestoreDirectoryMetadata2Helper(aDirectory, aPersistent);

  nsresult rv = helper->RestoreMetadata2File();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
QuotaManager::GetDirectoryMetadata2(nsIFile* aDirectory,
                                    int64_t* aTimestamp,
                                    nsACString& aSuffix,
                                    nsACString& aGroup,
                                    nsACString& aOrigin,
                                    bool* aIsApp)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(aTimestamp);
  MOZ_ASSERT(aIsApp);
  MOZ_ASSERT(mStorageInitialized);

  nsCOMPtr<nsIBinaryInputStream> binaryStream;
  nsresult rv = GetBinaryInputStream(aDirectory,
                                     NS_LITERAL_STRING(METADATA_V2_FILE_NAME),
                                     getter_AddRefs(binaryStream));
  NS_ENSURE_SUCCESS(rv, rv);

  uint64_t timestamp;
  rv = binaryStream->Read64(&timestamp);
  NS_ENSURE_SUCCESS(rv, rv);

  bool persisted;
  rv = binaryStream->ReadBoolean(&persisted);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint32_t reservedData1;
  rv = binaryStream->Read32(&reservedData1);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint32_t reservedData2;
  rv = binaryStream->Read32(&reservedData2);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString suffix;
  rv = binaryStream->ReadCString(suffix);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString group;
  rv = binaryStream->ReadCString(group);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString origin;
  rv = binaryStream->ReadCString(origin);
  NS_ENSURE_SUCCESS(rv, rv);

  bool isApp;
  rv = binaryStream->ReadBoolean(&isApp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aTimestamp = timestamp;
  aSuffix = suffix;
  aGroup = group;
  aOrigin = origin;
  *aIsApp = isApp;
  return NS_OK;
}

nsresult
QuotaManager::GetDirectoryMetadata2WithRestore(nsIFile* aDirectory,
                                               bool aPersistent,
                                               int64_t* aTimestamp,
                                               nsACString& aSuffix,
                                               nsACString& aGroup,
                                               nsACString& aOrigin,
                                               bool* aIsApp)
{
  nsresult rv = GetDirectoryMetadata2(aDirectory,
                                      aTimestamp,
                                      aSuffix,
                                      aGroup,
                                      aOrigin,
                                      aIsApp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    rv = RestoreDirectoryMetadata2(aDirectory, aPersistent);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = GetDirectoryMetadata2(aDirectory,
                               aTimestamp,
                               aSuffix,
                               aGroup,
                               aOrigin,
                               aIsApp);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult
QuotaManager::GetDirectoryMetadata2(nsIFile* aDirectory, int64_t* aTimestamp)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(aTimestamp);
  MOZ_ASSERT(mStorageInitialized);

  nsCOMPtr<nsIBinaryInputStream> binaryStream;
  nsresult rv = GetBinaryInputStream(aDirectory,
                                     NS_LITERAL_STRING(METADATA_V2_FILE_NAME),
                                     getter_AddRefs(binaryStream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint64_t timestamp;
  rv = binaryStream->Read64(&timestamp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aTimestamp = timestamp;
  return NS_OK;
}

nsresult
QuotaManager::GetDirectoryMetadata2WithRestore(nsIFile* aDirectory,
                                               bool aPersistent,
                                               int64_t* aTimestamp)
{
  nsresult rv = GetDirectoryMetadata2(aDirectory, aTimestamp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    rv = RestoreDirectoryMetadata2(aDirectory, aPersistent);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = GetDirectoryMetadata2(aDirectory, aTimestamp);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult
QuotaManager::InitializeRepository(PersistenceType aPersistenceType)
{
  MOZ_ASSERT(aPersistenceType == PERSISTENCE_TYPE_TEMPORARY ||
             aPersistenceType == PERSISTENCE_TYPE_DEFAULT);

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

      if (leafName.EqualsLiteral(DSSTORE_FILE_NAME)) {
        continue;
      }

      QM_WARNING("Something (%s) in the repository that doesn't belong!",
                 NS_ConvertUTF16toUTF8(leafName).get());
      return NS_ERROR_UNEXPECTED;
    }

    int64_t timestamp;
    nsCString suffix;
    nsCString group;
    nsCString origin;
    bool isApp;
    rv = GetDirectoryMetadata2WithRestore(childDirectory,
                                          /* aPersistent */ false,
                                          &timestamp,
                                          suffix,
                                          group,
                                          origin,
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

namespace {

// The Cache API was creating top level morgue directories by accident for
// a short time in nightly.  This unfortunately prevents all storage from
// working.  So recover these profiles by removing these corrupt directories.
// This should be removed at some point in the future.
bool
MaybeRemoveCorruptDirectory(const nsAString& aLeafName, nsIFile* aDir)
{
#ifdef NIGHTLY_BUILD
  MOZ_ASSERT(aDir);

  if (aLeafName != NS_LITERAL_STRING("morgue")) {
    return false;
  }

  NS_WARNING("QuotaManager removing corrupt morgue directory!");

  nsresult rv = aDir->Remove(true /* recursive */);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return true;
#else
  return false;
#endif // NIGHTLY_BUILD
}

} // namespace

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
        leafName.EqualsLiteral(METADATA_V2_FILE_NAME) ||
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

    if (MaybeRemoveCorruptDirectory(leafName, file)) {
      continue;
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

  rv = persistentStorageDir->Append(NS_LITERAL_STRING(PERSISTENT_DIRECTORY_NAME));
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
  rv = indexedDBDir->MoveTo(storageDir, NS_LITERAL_STRING(PERSISTENT_DIRECTORY_NAME));
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

  rv = persistentStorageDir->Append(NS_LITERAL_STRING(PERSISTENT_DIRECTORY_NAME));
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
  RefPtr<CreateOrUpgradeDirectoryMetadataHelper> helper =
    new CreateOrUpgradeDirectoryMetadataHelper(persistentStorageDir,
                                               /* aPersistent */ true);

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

    helper =
      new CreateOrUpgradeDirectoryMetadataHelper(temporaryStorageDir,
                                                 /* aPersistent */ false);

    rv = helper->CreateOrUpgradeMetadataFiles();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // And finally rename persistent to default.
  rv = persistentStorageDir->RenameTo(nullptr, NS_LITERAL_STRING(DEFAULT_DIRECTORY_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
QuotaManager::MaybeRemoveOldDirectories()
{
  AssertIsOnIOThread();

  nsresult rv;

  nsCOMPtr<nsIFile> indexedDBDir =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = indexedDBDir->InitWithPath(mIndexedDBPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = indexedDBDir->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    QM_WARNING("Deleting old <profile>/indexedDB directory!");

    rv = indexedDBDir->Remove(/* aRecursive */ true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  nsCOMPtr<nsIFile> persistentStorageDir =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = persistentStorageDir->InitWithPath(mStoragePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = persistentStorageDir->Append(NS_LITERAL_STRING(PERSISTENT_DIRECTORY_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = persistentStorageDir->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    QM_WARNING("Deleting old <profile>/storage/persistent directory!");

    rv = persistentStorageDir->Remove(/* aRecursive */ true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult
QuotaManager::UpgradeStorageFrom0ToCurrent(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  nsresult rv;

  for (const PersistenceType persistenceType : kAllPersistenceTypes) {
    nsCOMPtr<nsIFile> directory =
      do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = directory->InitWithPath(GetStoragePath(persistenceType));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    bool persistent = persistenceType == PERSISTENCE_TYPE_PERSISTENT;
    RefPtr<UpgradeDirectoryMetadataFrom1To2Helper> helper =
      new UpgradeDirectoryMetadataFrom1To2Helper(directory, persistent);

    rv = helper->UpgradeMetadataFiles();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

#ifdef DEBUG
  {
    int32_t storageVersion;
    rv = aConnection->GetSchemaVersion(&storageVersion);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(storageVersion == 0);
  }
#endif

  rv = aConnection->SetSchemaVersion(kStorageVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

#if 0
nsresult
QuotaManager::UpgradeStorageFrom1To2(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  nsresult rv = aConnection->SetSchemaVersion(MakeStorageVersion(2, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}
#endif

nsresult
QuotaManager::EnsureStorageIsInitialized()
{
  AssertIsOnIOThread();

  if (mStorageInitialized) {
    return NS_OK;
  }

  nsresult rv;

  nsCOMPtr<nsIFile> storageFile =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = storageFile->InitWithPath(mBasePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = storageFile->Append(NS_LITERAL_STRING(STORAGE_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageService> ss =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageConnection> connection;
  rv = ss->OpenUnsharedDatabase(storageFile, getter_AddRefs(connection));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    // Nuke the database file.
    rv = storageFile->Remove(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = ss->OpenUnsharedDatabase(storageFile, getter_AddRefs(connection));
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // We want extra durability for this important file.
  rv = connection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "PRAGMA synchronous = EXTRA;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Check to make sure that the storage version is correct.
  int32_t storageVersion;
  rv = connection->GetSchemaVersion(&storageVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (GetMajorStorageVersion(storageVersion) > kMajorStorageVersion) {
    NS_WARNING("Unable to initialize storage, version is too high!");
    return NS_ERROR_FAILURE;
  }

  if (storageVersion < kStorageVersion) {
    const bool newDatabase = !storageVersion;

    if (newDatabase) {
      // Set the page size first.
      if (kSQLitePageSizeOverride) {
        rv = connection->ExecuteSimpleSQL(
          nsPrintfCString("PRAGMA page_size = %lu;", kSQLitePageSizeOverride)
        );
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }

    mozStorageTransaction transaction(connection, false,
                                  mozIStorageConnection::TRANSACTION_IMMEDIATE);
    if (newDatabase) {
      rv = MaybeUpgradeIndexedDBDirectory();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = MaybeUpgradePersistentStorageDirectory();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = MaybeRemoveOldDirectories();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = UpgradeStorageFrom0ToCurrent(connection);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      MOZ_ASSERT(NS_SUCCEEDED(connection->GetSchemaVersion(&storageVersion)));
      MOZ_ASSERT(storageVersion == kStorageVersion);
    } else {
      // This logic needs to change next time we change the storage!
      static_assert(kStorageVersion == int32_t((1 << 16) + 0),
                    "Upgrade function needed due to storage version increase.");

      while (storageVersion != kStorageVersion) {
        /* if (storageVersion == MakeStorageVersion(1, 0)) {
          rv = UpgradeStorageFrom1To2(connection);
        } else */ {
          NS_WARNING("Unable to initialize storage, no upgrade path is "
                     "available!");
          return NS_ERROR_FAILURE;
        }

        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        rv = connection->GetSchemaVersion(&storageVersion);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }

      MOZ_ASSERT(storageVersion == kStorageVersion);
    }

    rv = transaction.Commit();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  mStorageInitialized = true;

  return NS_OK;
}

void
QuotaManager::OpenDirectory(PersistenceType aPersistenceType,
                            const nsACString& aGroup,
                            const nsACString& aOrigin,
                            bool aIsApp,
                            Client::Type aClientType,
                            bool aExclusive,
                            OpenDirectoryListener* aOpenListener)
{
  AssertIsOnOwningThread();

  RefPtr<DirectoryLockImpl> lock =
    CreateDirectoryLock(Nullable<PersistenceType>(aPersistenceType),
                        aGroup,
                        OriginScope::FromOrigin(aOrigin),
                        Nullable<bool>(aIsApp),
                        Nullable<Client::Type>(aClientType),
                        aExclusive,
                        false,
                        aOpenListener);
  MOZ_ASSERT(lock);
}

void
QuotaManager::OpenDirectoryInternal(Nullable<PersistenceType> aPersistenceType,
                                    const OriginScope& aOriginScope,
                                    Nullable<Client::Type> aClientType,
                                    bool aExclusive,
                                    OpenDirectoryListener* aOpenListener)
{
  AssertIsOnOwningThread();

  RefPtr<DirectoryLockImpl> lock =
    CreateDirectoryLock(aPersistenceType,
                        EmptyCString(),
                        aOriginScope,
                        Nullable<bool>(),
                        Nullable<Client::Type>(aClientType),
                        aExclusive,
                        true,
                        aOpenListener);
  MOZ_ASSERT(lock);

  if (!aExclusive) {
    return;
  }

  // All the locks that block this new exclusive lock need to be invalidated.
  // We also need to notify clients to abort operations for them.
  AutoTArray<nsAutoPtr<nsTHashtable<nsCStringHashKey>>,
               Client::TYPE_MAX> origins;
  origins.SetLength(Client::TYPE_MAX);

  const nsTArray<DirectoryLockImpl*>& blockedOnLocks =
    lock->GetBlockedOnLocks();

  for (DirectoryLockImpl* blockedOnLock : blockedOnLocks) {
    blockedOnLock->Invalidate();

    if (!blockedOnLock->IsInternal()) {
      MOZ_ASSERT(!blockedOnLock->GetClientType().IsNull());
      Client::Type clientType = blockedOnLock->GetClientType().Value();
      MOZ_ASSERT(clientType < Client::TYPE_MAX);

      const OriginScope& originScope = blockedOnLock->GetOriginScope();
      MOZ_ASSERT(originScope.IsOrigin());
      MOZ_ASSERT(!originScope.GetOrigin().IsEmpty());

      nsAutoPtr<nsTHashtable<nsCStringHashKey>>& origin = origins[clientType];
      if (!origin) {
        origin = new nsTHashtable<nsCStringHashKey>();
      }
      origin->PutEntry(originScope.GetOrigin());
    }
  }

  for (uint32_t index : MakeRange(uint32_t(Client::TYPE_MAX))) {
    if (origins[index]) {
      for (auto iter = origins[index]->Iter(); !iter.Done(); iter.Next()) {
        MOZ_ASSERT(mClients[index]);

        mClients[index]->AbortOperations(iter.Get()->GetKey());
      }
    }
  }
}

nsresult
QuotaManager::EnsureOriginIsInitialized(PersistenceType aPersistenceType,
                                        const nsACString& aSuffix,
                                        const nsACString& aGroup,
                                        const nsACString& aOrigin,
                                        bool aIsApp,
                                        nsIFile** aDirectory)
{
  AssertIsOnIOThread();

  nsresult rv = EnsureStorageIsInitialized();
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

      rv = CreateDirectoryMetadata(directory,
                                   timestamp,
                                   aSuffix,
                                   aGroup,
                                   aOrigin,
                                   aIsApp);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = CreateDirectoryMetadata2(directory,
                                    timestamp,
                                    aSuffix,
                                    aGroup,
                                    aOrigin,
                                    aIsApp);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      bool persistent = aPersistenceType == PERSISTENCE_TYPE_PERSISTENT;
      rv = GetDirectoryMetadata2WithRestore(directory,
                                            persistent,
                                            &timestamp);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      MOZ_ASSERT(timestamp <= PR_Now());
    }

    rv = InitializeOrigin(aPersistenceType, aGroup, aOrigin, aIsApp, timestamp,
                          directory);
    NS_ENSURE_SUCCESS(rv, rv);

    mInitializedOrigins.AppendElement(OriginKey(aPersistenceType, aOrigin));
  } else if (created) {
    timestamp = PR_Now();

    rv = CreateDirectoryMetadata(directory,
                                 timestamp,
                                 aSuffix,
                                 aGroup,
                                 aOrigin,
                                 aIsApp);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = CreateDirectoryMetadata2(directory,
                                  timestamp,
                                  aSuffix,
                                  aGroup,
                                  aOrigin,
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
  mStorageInitialized = false;

  ReleaseIOThreadObjects();
}

Client*
QuotaManager::GetClient(Client::Type aClientType)
{
  MOZ_ASSERT(aClientType >= Client::IDB);
  MOZ_ASSERT(aClientType < Client::TYPE_MAX);

  return mClients.ElementAt(aClientType);
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
                           nsACString& aDatabaseId)
{
  nsAutoCString str;
  str.AppendInt(aPersistenceType);
  str.Append('*');
  str.Append(aOrigin);
  str.Append('*');
  str.AppendInt(aClientType);

  aDatabaseId = str;
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
                                   nsACString* aSuffix,
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
    GetInfoForChrome(aSuffix, aGroup, aOrigin, aIsApp);
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
  rv = aPrincipal->GetOrigin(origin);
  NS_ENSURE_SUCCESS(rv, rv);

  if (origin.EqualsLiteral(kChromeOrigin)) {
    NS_WARNING("Non-chrome principal can't use chrome origin!");
    return NS_ERROR_FAILURE;
  }

  nsCString suffix;
  BasePrincipal::Cast(aPrincipal)->OriginAttributesRef().CreateSuffix(suffix);

  if (aSuffix)
  {
    aSuffix->Assign(suffix);
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
      aGroup->Assign(origin);
    } else {
      aGroup->Assign(baseDomain + suffix);
    }
  }

  if (aOrigin) {
    aOrigin->Assign(origin);
  }

  if (aIsApp) {
    *aIsApp = aPrincipal->GetAppStatus() !=
                nsIPrincipal::APP_STATUS_NOT_INSTALLED;
  }

  return NS_OK;
}

// static
nsresult
QuotaManager::GetInfoFromWindow(nsPIDOMWindowOuter* aWindow,
                                nsACString* aSuffix,
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

  nsresult rv =
    GetInfoFromPrincipal(principal, aSuffix, aGroup, aOrigin, aIsApp);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// static
void
QuotaManager::GetInfoForChrome(nsACString* aSuffix,
                               nsACString* aGroup,
                               nsACString* aOrigin,
                               bool* aIsApp)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(nsContentUtils::LegacyIsCallerChromeOrNativeCode());

  if (aSuffix) {
    aSuffix->Assign(EmptyCString());
  }
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
      StringBeginsWith(aOrigin, nsDependentCString(kAboutHomeOriginPrefix)) ||
      StringBeginsWith(aOrigin, nsDependentCString(kIndexedDBOriginPrefix)) ||
      StringBeginsWith(aOrigin, nsDependentCString(kResourceOriginPrefix))) {
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

uint64_t
QuotaManager::LockedCollectOriginsForEviction(
                                  uint64_t aMinSizeToBeFreed,
                                  nsTArray<RefPtr<DirectoryLockImpl>>& aLocks)
{
  mQuotaMutex.AssertCurrentThreadOwns();

  RefPtr<CollectOriginsHelper> helper =
    new CollectOriginsHelper(mQuotaMutex, aMinSizeToBeFreed);

  // Unlock while calling out to XPCOM (code behind the dispatch method needs
  // to acquire its own lock which can potentially lead to a deadlock and it
  // also calls an observer that can do various stuff like IO, so it's better
  // to not hold our mutex while that happens).
  {
    MutexAutoUnlock autoUnlock(mQuotaMutex);

    MOZ_ALWAYS_SUCCEEDS(mOwningThread->Dispatch(helper, NS_DISPATCH_NORMAL));
  }

  return helper->BlockAndReturnOriginsForEviction(aLocks);
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

  RefPtr<GroupInfo> groupInfo = pair->LockedGetGroupInfo(aPersistenceType);
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

void
QuotaManager::CheckTemporaryStorageLimits()
{
  AssertIsOnIOThread();

  nsTArray<OriginInfo*> doomedOriginInfos;
  {
    MutexAutoLock lock(mQuotaMutex);

    for (auto iter = mGroupInfoPairs.Iter(); !iter.Done(); iter.Next()) {
      GroupInfoPair* pair = iter.UserData();

      MOZ_ASSERT(!iter.Key().IsEmpty(), "Empty key!");
      MOZ_ASSERT(pair, "Null pointer!");

      uint64_t groupUsage = 0;

      RefPtr<GroupInfo> temporaryGroupInfo =
        pair->LockedGetGroupInfo(PERSISTENCE_TYPE_TEMPORARY);
      if (temporaryGroupInfo) {
        groupUsage += temporaryGroupInfo->mUsage;
      }

      RefPtr<GroupInfo> defaultGroupInfo =
        pair->LockedGetGroupInfo(PERSISTENCE_TYPE_DEFAULT);
      if (defaultGroupInfo) {
        groupUsage += defaultGroupInfo->mUsage;
      }

      if (groupUsage > 0) {
        QuotaManager* quotaManager = QuotaManager::Get();
        MOZ_ASSERT(quotaManager, "Shouldn't be null!");

        if (groupUsage > quotaManager->GetGroupLimit()) {
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

            doomedOriginInfos.AppendElement(originInfo);
            groupUsage -= originInfo->mUsage;

            if (groupUsage <= quotaManager->GetGroupLimit()) {
              break;
            }
          }
        }
      }
    }

    uint64_t usage = 0;
    for (uint32_t index = 0; index < doomedOriginInfos.Length(); index++) {
      usage += doomedOriginInfos[index]->mUsage;
    }

    if (mTemporaryStorageUsage - usage > mTemporaryStorageLimit) {
      nsTArray<OriginInfo*> originInfos;

      for (auto iter = mGroupInfoPairs.Iter(); !iter.Done(); iter.Next()) {
        GroupInfoPair* pair = iter.UserData();

        MOZ_ASSERT(!iter.Key().IsEmpty(), "Empty key!");
        MOZ_ASSERT(pair, "Null pointer!");

        RefPtr<GroupInfo> groupInfo =
          pair->LockedGetGroupInfo(PERSISTENCE_TYPE_TEMPORARY);
        if (groupInfo) {
          originInfos.AppendElements(groupInfo->mOriginInfos);
        }

        groupInfo = pair->LockedGetGroupInfo(PERSISTENCE_TYPE_DEFAULT);
        if (groupInfo) {
          originInfos.AppendElements(groupInfo->mOriginInfos);
        }
      }

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

  for (const OriginParams& doomedOrigin : doomedOrigins) {
    OriginClearCompleted(doomedOrigin.mPersistenceType,
                         doomedOrigin.mOrigin,
                         doomedOrigin.mIsApp);
  }
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
QuotaManager::FinalizeOriginEviction(
                                  nsTArray<RefPtr<DirectoryLockImpl>>& aLocks)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  RefPtr<FinalizeOriginEvictionOp> op =
    new FinalizeOriginEvictionOp(mOwningThread, aLocks);

  if (IsOnIOThread()) {
    op->RunOnIOThreadImmediately();
  } else {
    op->Dispatch();
  }
}

void
QuotaManager::ShutdownTimerCallback(nsITimer* aTimer, void* aClosure)
{
  AssertIsOnBackgroundThread();

  auto quotaManager = static_cast<QuotaManager*>(aClosure);
  MOZ_ASSERT(quotaManager);

  NS_WARNING("Some storage operations are taking longer than expected "
             "during shutdown and will be aborted!");

  // Abort all operations.
  for (RefPtr<Client>& client : quotaManager->mClients) {
    client->AbortOperations(NullCString());
  }
}

auto
QuotaManager::GetDirectoryLockTable(PersistenceType aPersistenceType)
  -> DirectoryLockTable&
{
  switch (aPersistenceType) {
    case PERSISTENCE_TYPE_TEMPORARY:
      return mTemporaryDirectoryLockTable;
    case PERSISTENCE_TYPE_DEFAULT:
      return mDefaultDirectoryLockTable;

    case PERSISTENCE_TYPE_PERSISTENT:
    case PERSISTENCE_TYPE_INVALID:
    default:
      MOZ_CRASH("Bad persistence type value!");
  }
}

/*******************************************************************************
 * Local class implementations
 ******************************************************************************/

void
OriginInfo::LockedDecreaseUsage(int64_t aSize)
{
  AssertCurrentThreadOwnsQuotaMutex();

  AssertNoUnderflow(mUsage, aSize);
  mUsage -= aSize;

  AssertNoUnderflow(mGroupInfo->mUsage, aSize);
  mGroupInfo->mUsage -= aSize;

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  AssertNoUnderflow(quotaManager->mTemporaryStorageUsage, aSize);
  quotaManager->mTemporaryStorageUsage -= aSize;
}

already_AddRefed<OriginInfo>
GroupInfo::LockedGetOriginInfo(const nsACString& aOrigin)
{
  AssertCurrentThreadOwnsQuotaMutex();

  for (RefPtr<OriginInfo>& originInfo : mOriginInfos) {
    if (originInfo->mOrigin == aOrigin) {
      RefPtr<OriginInfo> result = originInfo;
      return result.forget();
    }
  }

  return nullptr;
}

void
GroupInfo::LockedAddOriginInfo(OriginInfo* aOriginInfo)
{
  AssertCurrentThreadOwnsQuotaMutex();

  NS_ASSERTION(!mOriginInfos.Contains(aOriginInfo),
               "Replacing an existing entry!");
  mOriginInfos.AppendElement(aOriginInfo);

  AssertNoOverflow(mUsage, aOriginInfo->mUsage);
  mUsage += aOriginInfo->mUsage;

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  AssertNoOverflow(quotaManager->mTemporaryStorageUsage, aOriginInfo->mUsage);
  quotaManager->mTemporaryStorageUsage += aOriginInfo->mUsage;
}

void
GroupInfo::LockedRemoveOriginInfo(const nsACString& aOrigin)
{
  AssertCurrentThreadOwnsQuotaMutex();

  for (uint32_t index = 0; index < mOriginInfos.Length(); index++) {
    if (mOriginInfos[index]->mOrigin == aOrigin) {
      AssertNoUnderflow(mUsage, mOriginInfos[index]->mUsage);
      mUsage -= mOriginInfos[index]->mUsage;

      QuotaManager* quotaManager = QuotaManager::Get();
      MOZ_ASSERT(quotaManager);

      AssertNoUnderflow(quotaManager->mTemporaryStorageUsage,
                        mOriginInfos[index]->mUsage);
      quotaManager->mTemporaryStorageUsage -= mOriginInfos[index]->mUsage;

      mOriginInfos.RemoveElementAt(index);

      return;
    }
  }
}

void
GroupInfo::LockedRemoveOriginInfos()
{
  AssertCurrentThreadOwnsQuotaMutex();

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  for (uint32_t index = mOriginInfos.Length(); index > 0; index--) {
    OriginInfo* originInfo = mOriginInfos[index - 1];

    AssertNoUnderflow(mUsage, originInfo->mUsage);
    mUsage -= originInfo->mUsage;

    AssertNoUnderflow(quotaManager->mTemporaryStorageUsage, originInfo->mUsage);
    quotaManager->mTemporaryStorageUsage -= originInfo->mUsage;

    mOriginInfos.RemoveElementAt(index - 1);
  }
}

RefPtr<GroupInfo>&
GroupInfoPair::GetGroupInfoForPersistenceType(PersistenceType aPersistenceType)
{
  switch (aPersistenceType) {
    case PERSISTENCE_TYPE_TEMPORARY:
      return mTemporaryStorageGroupInfo;
    case PERSISTENCE_TYPE_DEFAULT:
      return mDefaultStorageGroupInfo;

    case PERSISTENCE_TYPE_PERSISTENT:
    case PERSISTENCE_TYPE_INVALID:
    default:
      MOZ_CRASH("Bad persistence type value!");
  }
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
                                  nsTArray<RefPtr<DirectoryLockImpl>>& aLocks)
{
  MOZ_ASSERT(!NS_IsMainThread(), "Wrong thread!");
  mMutex.AssertCurrentThreadOwns();

  while (mWaiting) {
    mCondVar.Wait();
  }

  mLocks.SwapElements(aLocks);
  return mSizeToBeFreed;
}

NS_IMETHODIMP
CollectOriginsHelper::Run()
{
  AssertIsOnBackgroundThread();

  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "Shouldn't be null!");

  // We use extra stack vars here to avoid race detector warnings (the same
  // memory accessed with and without the lock held).
  nsTArray<RefPtr<DirectoryLockImpl>> locks;
  uint64_t sizeToBeFreed =
    quotaManager->CollectOriginsForEviction(mMinSizeToBeFreed, locks);

  MutexAutoLock lock(mMutex);

  NS_ASSERTION(mWaiting, "Huh?!");

  mLocks.SwapElements(locks);
  mSizeToBeFreed = sizeToBeFreed;
  mWaiting = false;
  mCondVar.Notify();

  return NS_OK;
}

/*******************************************************************************
 * OriginOperationBase
 ******************************************************************************/

NS_IMETHODIMP
OriginOperationBase::Run()
{
  nsresult rv;

  switch (mState) {
    case State_Initial: {
      rv = Init();
      break;
    }

    case State_Initializing: {
      rv = InitOnMainThread();
      break;
    }

    case State_FinishingInit: {
      rv = FinishInit();
      break;
    }

    case State_CreatingQuotaManager: {
      rv = QuotaManagerOpen();
      break;
    }

    case State_DirectoryOpenPending: {
      rv = DirectoryOpen();
      break;
    }

    case State_DirectoryWorkOpen: {
      rv = DirectoryWork();
      break;
    }

    case State_UnblockingOpen: {
      UnblockOpen();
      return NS_OK;
    }

    default:
      MOZ_CRASH("Bad state!");
  }

  if (NS_WARN_IF(NS_FAILED(rv)) && mState != State_UnblockingOpen) {
    Finish(rv);
  }

  return NS_OK;
}

nsresult
OriginOperationBase::DirectoryOpen()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_DirectoryOpenPending);

  QuotaManager* quotaManager = QuotaManager::Get();
  if (NS_WARN_IF(!quotaManager)) {
    return NS_ERROR_FAILURE;
  }

  // Must set this before dispatching otherwise we will race with the IO thread.
  AdvanceState();

  nsresult rv = quotaManager->IOThread()->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void
OriginOperationBase::Finish(nsresult aResult)
{
  if (NS_SUCCEEDED(mResultCode)) {
    mResultCode = aResult;
  }

  // Must set mState before dispatching otherwise we will race with the main
  // thread.
  mState = State_UnblockingOpen;

  MOZ_ALWAYS_SUCCEEDS(mOwningThread->Dispatch(this, NS_DISPATCH_NORMAL));
}

nsresult
OriginOperationBase::Init()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_Initial);

  AdvanceState();

  if (mNeedsMainThreadInit) {
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(this));
  } else {
    AdvanceState();
    MOZ_ALWAYS_SUCCEEDS(Run());
  }

  return NS_OK;
}

nsresult
OriginOperationBase::InitOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == State_Initializing);

  nsresult rv = DoInitOnMainThread();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  AdvanceState();

  MOZ_ALWAYS_SUCCEEDS(mOwningThread->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

nsresult
OriginOperationBase::FinishInit()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_FinishingInit);

  if (QuotaManager::IsShuttingDown()) {
    return NS_ERROR_FAILURE;
  }

  AdvanceState();

  if (mNeedsQuotaManagerInit && !QuotaManager::Get()) {
    QuotaManager::GetOrCreate(this);
  } else {
    Open();
  }

  return NS_OK;
}

nsresult
OriginOperationBase::QuotaManagerOpen()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State_CreatingQuotaManager);

  if (NS_WARN_IF(!QuotaManager::Get())) {
    return NS_ERROR_FAILURE;
  }

  Open();

  return NS_OK;
}

nsresult
OriginOperationBase::DirectoryWork()
{
  AssertIsOnIOThread();
  MOZ_ASSERT(mState == State_DirectoryWorkOpen);

  QuotaManager* quotaManager = QuotaManager::Get();
  if (NS_WARN_IF(!quotaManager)) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;

  if (mNeedsQuotaManagerInit) {
    rv = quotaManager->EnsureStorageIsInitialized();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = DoDirectoryWork(quotaManager);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Must set mState before dispatching otherwise we will race with the owning
  // thread.
  AdvanceState();

  MOZ_ALWAYS_SUCCEEDS(mOwningThread->Dispatch(this, NS_DISPATCH_NORMAL));

  return NS_OK;
}

void
FinalizeOriginEvictionOp::Dispatch()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(GetState() == State_Initial);

  SetState(State_DirectoryOpenPending);

  MOZ_ALWAYS_SUCCEEDS(mOwningThread->Dispatch(this, NS_DISPATCH_NORMAL));
}

void
FinalizeOriginEvictionOp::RunOnIOThreadImmediately()
{
  AssertIsOnIOThread();
  MOZ_ASSERT(GetState() == State_Initial);

  SetState(State_DirectoryWorkOpen);

  MOZ_ALWAYS_SUCCEEDS(this->Run());
}

void
FinalizeOriginEvictionOp::Open()
{
  MOZ_CRASH("Shouldn't get here!");
}

nsresult
FinalizeOriginEvictionOp::DoDirectoryWork(QuotaManager* aQuotaManager)
{
  AssertIsOnIOThread();

  PROFILER_LABEL("Quota", "FinalizeOriginEvictionOp::DoDirectoryWork",
                 js::ProfileEntry::Category::OTHER);

  for (RefPtr<DirectoryLockImpl>& lock : mLocks) {
    aQuotaManager->OriginClearCompleted(lock->GetPersistenceType().Value(),
                                        lock->GetOriginScope().GetOrigin(),
                                        lock->GetIsApp().Value());
  }

  return NS_OK;
}

void
FinalizeOriginEvictionOp::UnblockOpen()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(GetState() == State_UnblockingOpen);

#ifdef DEBUG
  NoteActorDestroyed();
#endif

  mLocks.Clear();

  AdvanceState();
}

NS_IMPL_ISUPPORTS_INHERITED0(NormalOriginOperationBase, Runnable)

void
NormalOriginOperationBase::Open()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(GetState() == State_CreatingQuotaManager);
  MOZ_ASSERT(QuotaManager::Get());

  AdvanceState();

  QuotaManager::Get()->OpenDirectoryInternal(mPersistenceType,
                                             mOriginScope,
                                             Nullable<Client::Type>(),
                                             mExclusive,
                                             this);
}

void
NormalOriginOperationBase::UnblockOpen()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(GetState() == State_UnblockingOpen);

  SendResults();

  mDirectoryLock = nullptr;

  AdvanceState();
}

void
NormalOriginOperationBase::DirectoryLockAcquired(DirectoryLock* aLock)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aLock);
  MOZ_ASSERT(GetState() == State_DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  mDirectoryLock = aLock;

  nsresult rv = DirectoryOpen();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Finish(rv);
    return;
  }
}

void
NormalOriginOperationBase::DirectoryLockFailed()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(GetState() == State_DirectoryOpenPending);
  MOZ_ASSERT(!mDirectoryLock);

  Finish(NS_ERROR_FAILURE);
}

nsresult
SaveOriginAccessTimeOp::DoDirectoryWork(QuotaManager* aQuotaManager)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(!mPersistenceType.IsNull());
  MOZ_ASSERT(mOriginScope.IsOrigin());

  PROFILER_LABEL("Quota", "SaveOriginAccessTimeOp::DoDirectoryWork",
                 js::ProfileEntry::Category::OTHER);

  nsCOMPtr<nsIFile> directory;
  nsresult rv =
    aQuotaManager->GetDirectoryForOrigin(mPersistenceType.Value(),
                                         mOriginScope.GetOrigin(),
                                         getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIBinaryOutputStream> stream;
  rv = GetBinaryOutputStream(directory,
                             NS_LITERAL_STRING(METADATA_V2_FILE_NAME),
                             kUpdateFileFlag,
                             getter_AddRefs(stream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // The origin directory may not exist anymore.
  if (stream) {
    rv = stream->Write64(mTimestamp);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

void
SaveOriginAccessTimeOp::SendResults()
{
#ifdef DEBUG
  NoteActorDestroyed();
#endif
}

/*******************************************************************************
 * Quota
 ******************************************************************************/

Quota::Quota()
#ifdef DEBUG
  : mActorDestroyed(false)
#endif
{
}

Quota::~Quota()
{
  MOZ_ASSERT(mActorDestroyed);
}

void
Quota::StartIdleMaintenance()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!QuotaManager::IsShuttingDown());

  QuotaManager* quotaManager = QuotaManager::Get();
  if (NS_WARN_IF(!quotaManager)) {
    return;
  }

  quotaManager->StartIdleMaintenance();
}

void
Quota::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnBackgroundThread();
#ifdef DEBUG
  MOZ_ASSERT(!mActorDestroyed);
  mActorDestroyed = true;
#endif
}

PQuotaUsageRequestParent*
Quota::AllocPQuotaUsageRequestParent(const UsageRequestParams& aParams)
{
  RefPtr<GetUsageOp> actor = new GetUsageOp(aParams);

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

bool
Quota::RecvPQuotaUsageRequestConstructor(PQuotaUsageRequestParent* aActor,
                                         const UsageRequestParams& aParams)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != UsageRequestParams::T__None);

  auto* op = static_cast<GetUsageOp*>(aActor);

  if (NS_WARN_IF(!op->Init(this))) {
    return false;
  }

  op->RunImmediately();
  return true;
}

bool
Quota::DeallocPQuotaUsageRequestParent(PQuotaUsageRequestParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<GetUsageOp> actor =
    dont_AddRef(static_cast<GetUsageOp*>(aActor));
  return true;
}

PQuotaRequestParent*
Quota::AllocPQuotaRequestParent(const RequestParams& aParams)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  if (aParams.type() == RequestParams::TClearOriginsParams) {
    PBackgroundParent* actor = Manager();
    MOZ_ASSERT(actor);

    if (BackgroundParent::IsOtherProcessActor(actor)) {
      ASSERT_UNLESS_FUZZING();
      return nullptr;
    }
  }

  RefPtr<QuotaRequestBase> actor;

  switch (aParams.type()) {
    case RequestParams::TClearOriginParams:
    case RequestParams::TClearOriginsParams:
      actor = new OriginClearOp(aParams);
      break;

    case RequestParams::TClearAllParams:
      actor = new ResetOrClearOp(/* aClear */ true);
      break;

    case RequestParams::TResetAllParams:
      actor = new ResetOrClearOp(/* aClear */ false);
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  MOZ_ASSERT(actor);

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

bool
Quota::RecvPQuotaRequestConstructor(PQuotaRequestParent* aActor,
                                    const RequestParams& aParams)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  auto* op = static_cast<QuotaRequestBase*>(aActor);
  if (NS_WARN_IF(!op->Init(this))) {
    return false;
  }

  op->RunImmediately();
  return true;
}

bool
Quota::DeallocPQuotaRequestParent(PQuotaRequestParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<QuotaRequestBase> actor =
    dont_AddRef(static_cast<QuotaRequestBase*>(aActor));
  return true;
}

bool
Quota::RecvStartIdleMaintenance()
{
  AssertIsOnBackgroundThread();

  PBackgroundParent* actor = Manager();
  MOZ_ASSERT(actor);

  if (BackgroundParent::IsOtherProcessActor(actor)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  if (QuotaManager::IsShuttingDown()) {
    return true;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    nsCOMPtr<nsIRunnable> callback =
      NewRunnableMethod(this, &Quota::StartIdleMaintenance);

    QuotaManager::GetOrCreate(callback);
    return true;
  }

  quotaManager->StartIdleMaintenance();

  return true;
}

bool
Quota::RecvStopIdleMaintenance()
{
  AssertIsOnBackgroundThread();

  PBackgroundParent* actor = Manager();
  MOZ_ASSERT(actor);

  if (BackgroundParent::IsOtherProcessActor(actor)) {
    ASSERT_UNLESS_FUZZING();
    return false;
  }

  if (QuotaManager::IsShuttingDown()) {
    return true;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    return true;
  }

  quotaManager->StopIdleMaintenance();

  return true;
}

GetUsageOp::GetUsageOp(const UsageRequestParams& aParams)
  : NormalOriginOperationBase(Nullable<PersistenceType>(),
                              OriginScope::FromNull(),
                              /* aExclusive */ false)
  , mParams(aParams.get_UsageParams())
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aParams.type() == UsageRequestParams::TUsageParams);
}

bool
GetUsageOp::Init(Quota* aQuota)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aQuota);

  mNeedsMainThreadInit = true;
  mNeedsQuotaManagerInit = true;

  return true;
}

nsresult
GetUsageOp::DoInitOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(GetState() == State_Initializing);
  MOZ_ASSERT(mNeedsMainThreadInit);

  const PrincipalInfo& principalInfo = mParams.principalInfo();

  nsresult rv;
  nsCOMPtr<nsIPrincipal> principal =
    PrincipalInfoToPrincipal(principalInfo, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Figure out which origin we're dealing with.
  nsCString origin;
  rv = QuotaManager::GetInfoFromPrincipal(principal, nullptr, &mGroup, &origin,
                                          &mIsApp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mOriginScope.SetFromOrigin(origin);

  return NS_OK;
}

nsresult
GetUsageOp::AddToUsage(QuotaManager* aQuotaManager,
                       PersistenceType aPersistenceType)
{
  AssertIsOnIOThread();

  nsCOMPtr<nsIFile> directory;
  nsresult rv = aQuotaManager->GetDirectoryForOrigin(aPersistenceType,
                                                     mOriginScope.GetOrigin(),
                                                     getter_AddRefs(directory));
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists;
  rv = directory->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  // If the directory exists then enumerate all the files inside, adding up
  // the sizes to get the final usage statistic.
  if (exists && !mUsageInfo.Canceled()) {
    bool initialized;

    if (IsTreatedAsPersistent(aPersistenceType, mIsApp)) {
      nsCString originKey =
        OriginKey(aPersistenceType, mOriginScope.GetOrigin());
      initialized = aQuotaManager->IsOriginInitialized(originKey);
    } else {
      initialized = aQuotaManager->IsTemporaryStorageInitialized();
    }

    nsCOMPtr<nsISimpleEnumerator> entries;
    rv = directory->GetDirectoryEntries(getter_AddRefs(entries));
    NS_ENSURE_SUCCESS(rv, rv);

    bool hasMore;
    while (NS_SUCCEEDED((rv = entries->HasMoreElements(&hasMore))) &&
           hasMore && !mUsageInfo.Canceled()) {
      nsCOMPtr<nsISupports> entry;
      rv = entries->GetNext(getter_AddRefs(entry));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIFile> file = do_QueryInterface(entry);
      NS_ENSURE_TRUE(file, NS_NOINTERFACE);

      nsString leafName;
      rv = file->GetLeafName(leafName);
      NS_ENSURE_SUCCESS(rv, rv);

      if (leafName.EqualsLiteral(METADATA_FILE_NAME) ||
          leafName.EqualsLiteral(METADATA_V2_FILE_NAME) ||
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

      if (MaybeRemoveCorruptDirectory(leafName, file)) {
        continue;
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

      Client* client = aQuotaManager->GetClient(clientType);
      MOZ_ASSERT(client);

      if (initialized) {
        rv = client->GetUsageForOrigin(aPersistenceType,
                                       mGroup,
                                       mOriginScope.GetOrigin(),
                                       &mUsageInfo);
      }
      else {
        rv = client->InitOrigin(aPersistenceType,
                                mGroup,
                                mOriginScope.GetOrigin(),
                                &mUsageInfo);
      }
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

nsresult
GetUsageOp::DoDirectoryWork(QuotaManager* aQuotaManager)
{
  AssertIsOnIOThread();

  PROFILER_LABEL("Quota", "GetUsageOp::DoDirectoryWork",
                 js::ProfileEntry::Category::OTHER);

  // Add all the persistent/temporary/default storage files we care about.
  nsresult rv;
  for (const PersistenceType type : kAllPersistenceTypes) {
    rv = AddToUsage(aQuotaManager, type);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

void
GetUsageOp::SendResults()
{
  AssertIsOnOwningThread();

  if (IsActorDestroyed()) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = NS_ERROR_FAILURE;
    }
  } else {
    if (mUsageInfo.Canceled()) {
      mResultCode = NS_ERROR_FAILURE;
    }

    UsageRequestResponse response;

    if (NS_SUCCEEDED(mResultCode)) {
      UsageResponse usageResponse;
      usageResponse.usage() = mUsageInfo.TotalUsage();
      usageResponse.fileUsage() = mUsageInfo.FileUsage();
      response = usageResponse;
    } else {
      response = mResultCode;
    }

    Unused << PQuotaUsageRequestParent::Send__delete__(this, response);
  }
}

void
GetUsageOp::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();

  NoteActorDestroyed();
}

bool
GetUsageOp::RecvCancel()
{
  AssertIsOnOwningThread();

  nsresult rv = mUsageInfo.Cancel();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return true;
}

bool
QuotaRequestBase::Init(Quota* aQuota)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aQuota);

  mNeedsQuotaManagerInit = true;

  return true;
}

void
QuotaRequestBase::SendResults()
{
  AssertIsOnOwningThread();

  if (IsActorDestroyed()) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = NS_ERROR_FAILURE;
    }
  } else {
    RequestResponse response;

    if (NS_SUCCEEDED(mResultCode)) {
      GetResponse(response);
    } else {
      response = mResultCode;
    }

    Unused << PQuotaRequestParent::Send__delete__(this, response);
  }
}

void
QuotaRequestBase::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();

  NoteActorDestroyed();
}

void
ResetOrClearOp::DeleteFiles(QuotaManager* aQuotaManager)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aQuotaManager);

  nsresult rv;

  nsCOMPtr<nsIFile> directory =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  rv = directory->InitWithPath(aQuotaManager->GetStoragePath());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  rv = directory->Remove(true);
  if (rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST &&
      rv != NS_ERROR_FILE_NOT_FOUND && NS_FAILED(rv)) {
    // This should never fail if we've closed all storage connections
    // correctly...
    MOZ_ASSERT(false, "Failed to remove storage directory!");
  }

  nsCOMPtr<nsIFile> storageFile =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  rv = storageFile->InitWithPath(aQuotaManager->GetBasePath());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  rv = storageFile->Append(NS_LITERAL_STRING(STORAGE_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  rv = storageFile->Remove(true);
  if (rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST &&
      rv != NS_ERROR_FILE_NOT_FOUND && NS_FAILED(rv)) {
    // This should never fail if we've closed the storage connection
    // correctly...
    MOZ_ASSERT(false, "Failed to remove storage file!");
  }
}

nsresult
ResetOrClearOp::DoDirectoryWork(QuotaManager* aQuotaManager)
{
  AssertIsOnIOThread();

  PROFILER_LABEL("Quota", "ResetOrClearOp::DoDirectoryWork",
                 js::ProfileEntry::Category::OTHER);

  if (mClear) {
    DeleteFiles(aQuotaManager);
  }

  aQuotaManager->RemoveQuota();

  aQuotaManager->ResetOrClearCompleted();

  return NS_OK;
}

void
ResetOrClearOp::GetResponse(RequestResponse& aResponse)
{
  AssertIsOnOwningThread();
  if (mClear) {
    aResponse = ClearAllResponse();
  } else {
    aResponse = ResetAllResponse();
  }
}

OriginClearOp::OriginClearOp(const RequestParams& aParams)
  : QuotaRequestBase(/* aExclusive */ true)
  , mParams(aParams)
  , mMultiple(aParams.type() == RequestParams::TClearOriginsParams)
{
  MOZ_ASSERT(aParams.type() == RequestParams::TClearOriginParams ||
             aParams.type() == RequestParams::TClearOriginsParams);
}

bool
OriginClearOp::Init(Quota* aQuota)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aQuota);

  if (NS_WARN_IF(!QuotaRequestBase::Init(aQuota))) {
    return false;
  }

  if (!mMultiple) {
    const ClearOriginParams& params = mParams.get_ClearOriginParams();

    if (params.persistenceTypeIsExplicit()) {
      MOZ_ASSERT(params.persistenceType() != PERSISTENCE_TYPE_INVALID);

      mPersistenceType.SetValue(params.persistenceType());
    }
  }

  mNeedsMainThreadInit = true;

  return true;
}

nsresult
OriginClearOp::DoInitOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(GetState() == State_Initializing);
  MOZ_ASSERT(mNeedsMainThreadInit);

  if (mMultiple) {
    const ClearOriginsParams& params = mParams.get_ClearOriginsParams();

    mOriginScope.SetFromJSONPattern(params.pattern());
  } else {
    const ClearOriginParams& params = mParams.get_ClearOriginParams();

    const PrincipalInfo& principalInfo = params.principalInfo();

    nsresult rv;
    nsCOMPtr<nsIPrincipal> principal =
      PrincipalInfoToPrincipal(principalInfo, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Figure out which origin we're dealing with.
    nsCString origin;
    rv = QuotaManager::GetInfoFromPrincipal(principal, nullptr, nullptr, &origin,
                                            nullptr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mOriginScope.SetFromOrigin(origin);
  }

  return NS_OK;
}

void
OriginClearOp::DeleteFiles(QuotaManager* aQuotaManager,
                           PersistenceType aPersistenceType)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aQuotaManager);

  nsresult rv;

  nsCOMPtr<nsIFile> directory =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  rv = directory->InitWithPath(aQuotaManager->GetStoragePath(aPersistenceType));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsCOMPtr<nsISimpleEnumerator> entries;
  if (NS_WARN_IF(NS_FAILED(
        directory->GetDirectoryEntries(getter_AddRefs(entries)))) || !entries) {
    return;
  }

  OriginScope originScope = mOriginScope.Clone();
  if (originScope.IsOrigin()) {
    nsCString originSanitized(originScope.GetOrigin());
    SanitizeOriginString(originSanitized);
    originScope.SetOrigin(originSanitized);
  }

  bool hasMore;
  while (NS_SUCCEEDED((rv = entries->HasMoreElements(&hasMore))) && hasMore) {
    nsCOMPtr<nsISupports> entry;
    rv = entries->GetNext(getter_AddRefs(entry));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    nsCOMPtr<nsIFile> file = do_QueryInterface(entry);
    MOZ_ASSERT(file);

    bool isDirectory;
    rv = file->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    nsString leafName;
    rv = file->GetLeafName(leafName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    if (!isDirectory) {
      if (!leafName.EqualsLiteral(DSSTORE_FILE_NAME)) {
        QM_WARNING("Something (%s) in the repository that doesn't belong!",
                   NS_ConvertUTF16toUTF8(leafName).get());
      }
      continue;
    }

    // Skip the origin directory if it doesn't match the pattern.
    if (!originScope.MatchesOrigin(OriginScope::FromOrigin(
                                     NS_ConvertUTF16toUTF8(leafName)))) {
      continue;
    }

    bool persistent = aPersistenceType == PERSISTENCE_TYPE_PERSISTENT;

    int64_t timestamp;
    nsCString suffix;
    nsCString group;
    nsCString origin;
    bool isApp;
    rv = aQuotaManager->GetDirectoryMetadata2WithRestore(file,
                                                         persistent,
                                                         &timestamp,
                                                         suffix,
                                                         group,
                                                         origin,
                                                         &isApp);
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

nsresult
OriginClearOp::DoDirectoryWork(QuotaManager* aQuotaManager)
{
  AssertIsOnIOThread();

  PROFILER_LABEL("Quota", "OriginClearOp::DoDirectoryWork",
                 js::ProfileEntry::Category::OTHER);

  if (mPersistenceType.IsNull()) {
    for (const PersistenceType type : kAllPersistenceTypes) {
      DeleteFiles(aQuotaManager, type);
    }
  } else {
    DeleteFiles(aQuotaManager, mPersistenceType.Value());
  }

  return NS_OK;
}

void
OriginClearOp::GetResponse(RequestResponse& aResponse)
{
  AssertIsOnOwningThread();

  if (mMultiple) {
    aResponse = ClearOriginsResponse();
  } else {
    aResponse = ClearOriginResponse();
  }
}

nsresult
StorageDirectoryHelper::AddOriginDirectory(nsIFile* aDirectory,
                                           OriginProps** aOriginProps)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);

  OriginProps* originProps;

  nsString leafName;
  nsresult rv = aDirectory->GetLeafName(leafName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (leafName.EqualsLiteral(kChromeOrigin)) {
    originProps = mOriginProps.AppendElement();
    originProps->mDirectory = aDirectory;
    originProps->mSpec = kChromeOrigin;
    originProps->mType = OriginProps::eChrome;
  } else {
    nsCString spec;
    PrincipalOriginAttributes attrs;
    bool result = OriginParser::ParseOrigin(NS_ConvertUTF16toUTF8(leafName),
                                            spec, &attrs);
    if (NS_WARN_IF(!result)) {
      return NS_ERROR_FAILURE;
    }

    originProps = mOriginProps.AppendElement();
    originProps->mDirectory = aDirectory;
    originProps->mSpec = spec;
    originProps->mAttrs = attrs;
    originProps->mType = OriginProps::eContent;
  }

  if (aOriginProps) {
    *aOriginProps = originProps;
  }

  return NS_OK;
}

nsresult
StorageDirectoryHelper::ProcessOriginDirectories()
{
  AssertIsOnIOThread();
  MOZ_ASSERT(!mOriginProps.IsEmpty());

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(this));

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

  nsresult rv = DoProcessOriginDirectories();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
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
        QuotaManager::GetInfoForChrome(&originProps.mSuffix,
                                       &originProps.mGroup,
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

        nsCOMPtr<nsIPrincipal> principal =
          BasePrincipal::CreateCodebasePrincipal(uri, originProps.mAttrs);
        if (NS_WARN_IF(!principal)) {
          return NS_ERROR_FAILURE;
        }

        rv = QuotaManager::GetInfoFromPrincipal(principal,
                                                &originProps.mSuffix,
                                                &originProps.mGroup,
                                                &originProps.mOrigin,
                                                &originProps.mIsApp);
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
                          nsCString& aSpec,
                          PrincipalOriginAttributes* aAttrs)
{
  MOZ_ASSERT(!aOrigin.IsEmpty());
  MOZ_ASSERT(aAttrs);

  PrincipalOriginAttributes originAttributes;

  nsCString originNoSuffix;
  bool ok = originAttributes.PopulateFromOrigin(aOrigin, originNoSuffix);
  if (!ok) {
    return false;
  }

  OriginParser parser(originNoSuffix, originAttributes);
  return parser.Parse(aSpec, aAttrs);
}

bool
OriginParser::Parse(nsACString& aSpec, PrincipalOriginAttributes* aAttrs)
{
  MOZ_ASSERT(aAttrs);

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

  if (mAppId == kNoAppId) {
    *aAttrs = mOriginAttributes;
  } else {
    MOZ_ASSERT(mOriginAttributes.mAppId == kNoAppId);

    *aAttrs = PrincipalOriginAttributes(mAppId, mInIsolatedMozBrowser);
  }

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
      aToken.EqualsLiteral("app") ||
      aToken.EqualsLiteral("resource")) {
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
  MOZ_ASSERT(mState == eExpectingEmptyTokenOrDriveLetterOrPathnameComponent ||
             mState == eExpectingEmptyTokenOrPathnameComponent);
  MOZ_ASSERT(mSchemaType == eFile);

  mPathnameComponents.AppendElement(aToken);

  mState = mTokenizer.hasMoreTokens() ? eExpectingEmptyTokenOrPathnameComponent
                                      : eComplete;
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
        mInIsolatedMozBrowser = true;
      } else if (aToken.First() == 'f') {
        mInIsolatedMozBrowser = false;
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

      mState = mTokenizer.hasMoreTokens()
                 ? eExpectingEmptyTokenOrDriveLetterOrPathnameComponent
                 : eComplete;

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

    case eExpectingEmptyTokenOrDriveLetterOrPathnameComponent: {
      MOZ_ASSERT(mSchemaType == eFile);

      if (aToken.IsEmpty()) {
        mPathnameComponents.AppendElement(EmptyCString());

        mState =
          mTokenizer.hasMoreTokens() ? eExpectingEmptyTokenOrPathnameComponent
                                     : eComplete;

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

      if (aToken.IsEmpty()) {
        if (mMaybeDriveLetter) {
          MOZ_ASSERT(mPathnameComponents.Length() == 1);

          nsCString& pathnameComponent = mPathnameComponents[0];
          pathnameComponent.Append(':');

          mMaybeDriveLetter = false;
        } else {
          mPathnameComponents.AppendElement(EmptyCString());
        }

        mState =
          mTokenizer.hasMoreTokens() ? eExpectingEmptyTokenOrPathnameComponent
                                     : eComplete;

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
  MOZ_ASSERT(mState == eComplete);
  MOZ_ASSERT(mSchemaType == eFile);

  mPathnameComponents.AppendElement(EmptyCString());

  mState = eHandledTrailingSeparator;
}

nsresult
CreateOrUpgradeDirectoryMetadataHelper::CreateOrUpgradeMetadataFiles()
{
  AssertIsOnIOThread();

  bool exists;
  nsresult rv = mDirectory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    return NS_OK;
  }

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

    if (isDirectory) {
      if (leafName.EqualsLiteral("moz-safe-about+++home")) {
        // This directory was accidentally created by a buggy nightly and can
        // be safely removed.

        QM_WARNING("Deleting accidental moz-safe-about+++home directory!");

        rv = originDir->Remove(/* aRecursive */ true);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        continue;
      }
    } else {
      if (!leafName.EqualsLiteral(DSSTORE_FILE_NAME)) {
        QM_WARNING("Something (%s) in the storage directory that doesn't belong!",
                   NS_ConvertUTF16toUTF8(leafName).get());

      }
      continue;
    }

    if (mPersistent) {
      rv = MaybeUpgradeOriginDirectory(originDir);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    OriginProps* originProps;
    rv = AddOriginDirectory(originDir, &originProps);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!mPersistent) {
      int64_t timestamp;
      nsCString group;
      nsCString origin;
      bool hasIsApp;
      rv = GetDirectoryMetadata(originDir,
                                &timestamp,
                                group,
                                origin,
                                &hasIsApp);
      if (NS_FAILED(rv)) {
        timestamp = INT64_MIN;
        rv = GetLastModifiedTime(originDir, &timestamp);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        originProps->mTimestamp = timestamp;
        originProps->mNeedsRestore = true;
      } else if (hasIsApp) {
        originProps->mIgnore = true;
      }
    }
    else if (!QuotaManager::IsOriginWhitelistedForPersistentStorage(
                                                          originProps->mSpec)) {
      int64_t timestamp = INT64_MIN;
      rv = GetLastModifiedTime(originDir, &timestamp);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      originProps->mTimestamp = timestamp;
    }
  }

  if (mOriginProps.IsEmpty()) {
    return NS_OK;
  }

  rv = ProcessOriginDirectories();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
CreateOrUpgradeDirectoryMetadataHelper::MaybeUpgradeOriginDirectory(
                                                            nsIFile* aDirectory)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);

  nsCOMPtr<nsIFile> metadataFile;
  nsresult rv = aDirectory->Clone(getter_AddRefs(metadataFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = metadataFile->Append(NS_LITERAL_STRING(METADATA_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = metadataFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    // Directory structure upgrade needed.
    // Move all files to IDB specific directory.

    nsString idbDirectoryName;
    rv = Client::TypeToText(Client::IDB, idbDirectoryName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCOMPtr<nsIFile> idbDirectory;
    rv = aDirectory->Clone(getter_AddRefs(idbDirectory));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = idbDirectory->Append(idbDirectoryName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = idbDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755);
    if (rv == NS_ERROR_FILE_ALREADY_EXISTS) {
      NS_WARNING("IDB directory already exists!");

      bool isDirectory;
      rv = idbDirectory->IsDirectory(&isDirectory);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (NS_WARN_IF(!isDirectory)) {
        return NS_ERROR_UNEXPECTED;
      }
    }
    else {
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    nsCOMPtr<nsISimpleEnumerator> entries;
    rv = aDirectory->GetDirectoryEntries(getter_AddRefs(entries));
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
      if (NS_WARN_IF(!file)) {
        return rv;
      }

      nsString leafName;
      rv = file->GetLeafName(leafName);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (!leafName.Equals(idbDirectoryName)) {
        rv = file->MoveTo(idbDirectory, EmptyString());
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }

    rv = metadataFile->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult
CreateOrUpgradeDirectoryMetadataHelper::GetDirectoryMetadata(
                                                            nsIFile* aDirectory,
                                                            int64_t* aTimestamp,
                                                            nsACString& aGroup,
                                                            nsACString& aOrigin,
                                                            bool* aHasIsApp)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(aTimestamp);
  MOZ_ASSERT(aHasIsApp);
  MOZ_ASSERT(!mPersistent);

  nsCOMPtr<nsIBinaryInputStream> binaryStream;
  nsresult rv = GetBinaryInputStream(aDirectory,
                                     NS_LITERAL_STRING(METADATA_FILE_NAME),
                                     getter_AddRefs(binaryStream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint64_t timestamp;
  rv = binaryStream->Read64(&timestamp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString group;
  rv = binaryStream->ReadCString(group);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString origin;
  rv = binaryStream->ReadCString(origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool dummyIsApp;
  bool hasIsApp = NS_SUCCEEDED(binaryStream->ReadBoolean(&dummyIsApp));

  *aTimestamp = timestamp;
  aGroup = group;
  aOrigin = origin;
  *aHasIsApp = hasIsApp;
  return NS_OK;
}

nsresult
CreateOrUpgradeDirectoryMetadataHelper::DoProcessOriginDirectories()
{
  AssertIsOnIOThread();

  nsresult rv;

  nsCOMPtr<nsIFile> permanentStorageDir;

  for (uint32_t count = mOriginProps.Length(), index = 0;
       index < count;
       index++) {
    OriginProps& originProps = mOriginProps[index];

    if (mPersistent) {
      rv = CreateDirectoryMetadata(originProps.mDirectory,
                                   originProps.mTimestamp,
                                   originProps.mSuffix,
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

        nsString leafName;
        rv = originProps.mDirectory->GetLeafName(leafName);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        nsCOMPtr<nsIFile> newDirectory;
        rv = permanentStorageDir->Clone(getter_AddRefs(newDirectory));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        rv = newDirectory->Append(leafName);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        bool exists;
        rv = newDirectory->Exists(&exists);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        if (exists) {
          QM_WARNING("Found %s in storage/persistent and storage/permanent !",
                     NS_ConvertUTF16toUTF8(leafName).get());

          rv = originProps.mDirectory->Remove(/* recursive */ true);
        } else {
          rv = originProps.mDirectory->MoveTo(permanentStorageDir, EmptyString());
        }
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    } else if (originProps.mNeedsRestore) {
      rv = CreateDirectoryMetadata(originProps.mDirectory,
                                   originProps.mTimestamp,
                                   originProps.mSuffix,
                                   originProps.mGroup,
                                   originProps.mOrigin,
                                   originProps.mIsApp);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else if (!originProps.mIgnore) {
      nsCOMPtr<nsIBinaryOutputStream> stream;
      rv = GetBinaryOutputStream(originProps.mDirectory,
                                 NS_LITERAL_STRING(METADATA_FILE_NAME),
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
UpgradeDirectoryMetadataFrom1To2Helper::UpgradeMetadataFiles()
{
  AssertIsOnIOThread();

  bool exists;
  nsresult rv = mDirectory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    return NS_OK;
  }

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

    bool isDirectory;
    rv = originDir->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!isDirectory) {
      nsString leafName;
      rv = originDir->GetLeafName(leafName);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (!leafName.EqualsLiteral(DSSTORE_FILE_NAME)) {
        QM_WARNING("Something (%s) in the storage directory that doesn't belong!",
                   NS_ConvertUTF16toUTF8(leafName).get());

      }
      continue;
    }

    OriginProps* originProps;
    rv = AddOriginDirectory(originDir, &originProps);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    int64_t timestamp;
    nsCString group;
    nsCString origin;
    bool isApp;
    nsresult rv = GetDirectoryMetadata(originDir,
                                       &timestamp,
                                       group,
                                       origin,
                                       &isApp);
    if (NS_FAILED(rv)) {
      if (!mPersistent) {
        rv = GetLastModifiedTime(originDir, &timestamp);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        originProps->mTimestamp = timestamp;
      }
      originProps->mNeedsRestore = true;
    } else {
      originProps->mTimestamp = timestamp;
    }
  }

  if (mOriginProps.IsEmpty()) {
    return NS_OK;
  }

  rv = ProcessOriginDirectories();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
UpgradeDirectoryMetadataFrom1To2Helper::GetDirectoryMetadata(
                                                            nsIFile* aDirectory,
                                                            int64_t* aTimestamp,
                                                            nsACString& aGroup,
                                                            nsACString& aOrigin,
                                                            bool* aIsApp)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(aTimestamp);
  MOZ_ASSERT(aIsApp);

  nsCOMPtr<nsIBinaryInputStream> binaryStream;
  nsresult rv = GetBinaryInputStream(aDirectory,
                                     NS_LITERAL_STRING(METADATA_FILE_NAME),
                                     getter_AddRefs(binaryStream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint64_t timestamp;
  rv = binaryStream->Read64(&timestamp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString group;
  rv = binaryStream->ReadCString(group);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCString origin;
  rv = binaryStream->ReadCString(origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool isApp;
  rv = binaryStream->ReadBoolean(&isApp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aTimestamp = timestamp;
  aGroup = group;
  aOrigin = origin;
  *aIsApp = isApp;
  return NS_OK;
}

nsresult
UpgradeDirectoryMetadataFrom1To2Helper::DoProcessOriginDirectories()
{
  AssertIsOnIOThread();

  nsCOMPtr<nsIFile> permanentStorageDir;

  for (uint32_t count = mOriginProps.Length(), index = 0;
       index < count;
       index++) {
    OriginProps& originProps = mOriginProps[index];

    nsresult rv;

    if (originProps.mNeedsRestore) {
      rv = CreateDirectoryMetadata(originProps.mDirectory,
                                   originProps.mTimestamp,
                                   originProps.mSuffix,
                                   originProps.mGroup,
                                   originProps.mOrigin,
                                   originProps.mIsApp);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    rv = CreateDirectoryMetadata2(originProps.mDirectory,
                                  originProps.mTimestamp,
                                  originProps.mSuffix,
                                  originProps.mGroup,
                                  originProps.mOrigin,
                                  originProps.mIsApp);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsString oldName;
    rv = originProps.mDirectory->GetLeafName(oldName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsAutoCString originSanitized(originProps.mOrigin);
    SanitizeOriginString(originSanitized);

    NS_ConvertASCIItoUTF16 newName(originSanitized);

    if (!oldName.Equals(newName)) {
      rv = originProps.mDirectory->RenameTo(nullptr, newName);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  return NS_OK;
}

nsresult
RestoreDirectoryMetadata2Helper::RestoreMetadata2File()
{
  AssertIsOnIOThread();

  nsresult rv;

  OriginProps* originProps;
  rv = AddOriginDirectory(mDirectory, &originProps);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!mPersistent) {
    int64_t timestamp = INT64_MIN;
    rv = GetLastModifiedTime(mDirectory, &timestamp);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    originProps->mTimestamp = timestamp;
  }

  rv = ProcessOriginDirectories();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
RestoreDirectoryMetadata2Helper::DoProcessOriginDirectories()
{
  AssertIsOnIOThread();
  MOZ_ASSERT(mOriginProps.Length() == 1);

  OriginProps& originProps = mOriginProps[0];

  nsresult rv = CreateDirectoryMetadata2(originProps.mDirectory,
                                         originProps.mTimestamp,
                                         originProps.mSuffix,
                                         originProps.mGroup,
                                         originProps.mOrigin,
                                         originProps.mIsApp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

} // namespace quota
} // namespace dom
} // namespace mozilla
