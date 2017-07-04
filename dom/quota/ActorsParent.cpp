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
#include "nsISupportsPrimitives.h"
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
#include "mozilla/Unused.h"
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

#define UNKNOWN_FILE_WARNING(_leafName) \
  QM_WARNING("Something (%s) in the directory that doesn't belong!", \
             NS_ConvertUTF16toUTF8(leafName).get())

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
#define PROFILE_BEFORE_CHANGE_QM_OBSERVER_ID "profile-before-change-qm"

#define KB * 1024ULL
#define MB * 1024ULL KB
#define GB * 1024ULL MB

namespace mozilla {
namespace dom {
namespace quota {

using namespace mozilla::ipc;

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
const uint32_t kMajorStorageVersion = 2;

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
// XXX We should get rid of old metadata files at some point, bug 1343576.
#define METADATA_FILE_NAME ".metadata"
#define METADATA_TMP_FILE_NAME ".metadata-tmp"
#define METADATA_V2_FILE_NAME ".metadata-v2"
#define METADATA_V2_TMP_FILE_NAME ".metadata-v2-tmp"

/******************************************************************************
 * SQLite functions
 ******************************************************************************/

int32_t
MakeStorageVersion(uint32_t aMajorStorageVersion,
                   uint32_t aMinorStorageVersion)
{
  return int32_t((aMajorStorageVersion << 16) + aMinorStorageVersion);
}

uint32_t
GetMajorStorageVersion(int32_t aStorageVersion)
{
  return uint32_t(aStorageVersion >> 16);

}

nsresult
CreateTables(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  // The database doesn't have any tables for now. It's only used for storage
  // version checking.
  // However, this is the place where any future tables should be created.

  nsresult rv;

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
                    const Nullable<PersistenceType>& aPersistenceType,
                    const nsACString& aGroup,
                    const OriginScope& aOriginScope,
                    const Nullable<Client::Type>& aClientType,
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

class QuotaObject::StoragePressureRunnable final
  : public Runnable
{
  const uint64_t mUsage;

public:
  explicit StoragePressureRunnable(uint64_t aUsage)
    : Runnable("dom::quota::QuotaObject::StoragePressureRunnable")
    , mUsage(aUsage)
  { }

private:
  ~StoragePressureRunnable()
  { }

  NS_DECL_NSIRUNNABLE
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
    : Runnable("dom::quota::QuotaManager::CreateRunnable")
    , mResultCode(NS_OK)
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
    : Runnable("dom::quota::QuotaManager::ShutdownRunnable")
    , mDone(aDone)
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
  OriginInfo(GroupInfo* aGroupInfo, const nsACString& aOrigin,
             uint64_t aUsage, int64_t aAccessTime, bool aPersisted);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(OriginInfo)

  int64_t
  LockedAccessTime() const
  {
    AssertCurrentThreadOwnsQuotaMutex();

    return mAccessTime;
  }

  bool
  LockedPersisted() const
  {
    AssertCurrentThreadOwnsQuotaMutex();

    return mPersisted;
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

  void
  LockedPersist();

  nsDataHashtable<nsStringHashKey, QuotaObject*> mQuotaObjects;

  GroupInfo* mGroupInfo;
  const nsCString mOrigin;
  uint64_t mUsage;
  int64_t mAccessTime;
  bool mPersisted;
};

class OriginInfoLRUComparator
{
public:
  bool
  Equals(const OriginInfo* a, const OriginInfo* b) const
  {
    return a && b ?
             a->LockedAccessTime() == b->LockedAccessTime() :
             !a && !b ? true : false;
  }

  bool
  LessThan(const OriginInfo* a, const OriginInfo* b) const
  {
    return
      a && b ? a->LockedAccessTime() < b->LockedAccessTime() : b ? true : false;
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
  Run() override;
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
        nsIEventTarget* aOwningThread = GetCurrentThreadEventTarget())
    : BackgroundThreadObject(aOwningThread)
    , Runnable("dom::quota::OriginOperationBase")
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

#ifdef DEBUG
  State
  GetState() const
  {
    return mState;
  }
#endif

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
  Run() override;

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
  mozilla::Atomic<bool> mCanceled;
  const bool mExclusive;

public:
  void
  RunImmediately()
  {
    MOZ_ASSERT(GetState() == State_Initial);

    MOZ_ALWAYS_SUCCEEDS(this->Run());
  }

protected:
  NormalOriginOperationBase(const Nullable<PersistenceType>& aPersistenceType,
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

  virtual mozilla::ipc::IPCResult
  RecvPQuotaUsageRequestConstructor(PQuotaUsageRequestParent* aActor,
                                    const UsageRequestParams& aParams) override;

  virtual bool
  DeallocPQuotaUsageRequestParent(PQuotaUsageRequestParent* aActor) override;

  virtual PQuotaRequestParent*
  AllocPQuotaRequestParent(const RequestParams& aParams) override;

  virtual mozilla::ipc::IPCResult
  RecvPQuotaRequestConstructor(PQuotaRequestParent* aActor,
                               const RequestParams& aParams) override;

  virtual bool
  DeallocPQuotaRequestParent(PQuotaRequestParent* aActor) override;

  virtual mozilla::ipc::IPCResult
  RecvStartIdleMaintenance() override;

  virtual mozilla::ipc::IPCResult
  RecvStopIdleMaintenance() override;
};

class QuotaUsageRequestBase
  : public NormalOriginOperationBase
  , public PQuotaUsageRequestParent
{
public:
  // May be overridden by subclasses if they need to perform work on the
  // background thread before being run.
  virtual bool
  Init(Quota* aQuota);

protected:
  QuotaUsageRequestBase()
    : NormalOriginOperationBase(Nullable<PersistenceType>(),
                                OriginScope::FromNull(),
                                /* aExclusive */ false)
  { }

  nsresult
  GetUsageForOrigin(QuotaManager* aQuotaManager,
                    PersistenceType aPersistenceType,
                    const nsACString& aGroup,
                    const nsACString& aOrigin,
                    UsageInfo* aUsageInfo);

  // Subclasses use this override to set the IPDL response value.
  virtual void
  GetResponse(UsageRequestResponse& aResponse) = 0;

private:
  void
  SendResults() override;

  // IPDL methods.
  void
  ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult
  RecvCancel() override;
};

class GetUsageOp final
  : public QuotaUsageRequestBase
{
  nsTArray<OriginUsage> mOriginUsages;
  nsDataHashtable<nsCStringHashKey, uint32_t> mOriginUsagesIndex;

  bool mGetAll;

public:
  explicit GetUsageOp(const UsageRequestParams& aParams);

private:
  ~GetUsageOp()
  { }

  nsresult
  TraverseRepository(QuotaManager* aQuotaManager,
                     PersistenceType aPersistenceType);

  nsresult
  DoDirectoryWork(QuotaManager* aQuotaManager) override;

  void
  GetResponse(UsageRequestResponse& aResponse) override;
};

class GetOriginUsageOp final
  : public QuotaUsageRequestBase
{
  // If mGetGroupUsage is false, we use mUsageInfo to record the origin usage
  // and the file usage. Otherwise, we use it to record the group usage and the
  // limit.
  UsageInfo mUsageInfo;

  const OriginUsageParams mParams;
  nsCString mSuffix;
  nsCString mGroup;
  bool mGetGroupUsage;

public:
  explicit GetOriginUsageOp(const UsageRequestParams& aParams);

  MOZ_IS_CLASS_INIT bool
  Init(Quota* aQuota) override;

private:
  ~GetOriginUsageOp()
  { }

  MOZ_IS_CLASS_INIT virtual nsresult
  DoInitOnMainThread() override;

  virtual nsresult
  DoDirectoryWork(QuotaManager* aQuotaManager) override;

  void
  GetResponse(UsageRequestResponse& aResponse) override;
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

class InitOp final
  : public QuotaRequestBase
{
public:
  InitOp()
    : QuotaRequestBase(/* aExclusive */ false)
  {
    AssertIsOnOwningThread();
  }

private:
  ~InitOp()
  { }

  nsresult
  DoDirectoryWork(QuotaManager* aQuotaManager) override;

  void
  GetResponse(RequestResponse& aResponse) override;
};

class InitOriginOp final
  : public QuotaRequestBase
{
  const InitOriginParams mParams;
  nsCString mSuffix;
  nsCString mGroup;
  bool mCreated;

public:
  explicit InitOriginOp(const RequestParams& aParams);

  bool
  Init(Quota* aQuota) override;

private:
  ~InitOriginOp()
  { }

  nsresult
  DoInitOnMainThread() override;

  nsresult
  DoDirectoryWork(QuotaManager* aQuotaManager) override;

  void
  GetResponse(RequestResponse& aResponse) override;
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

class ClearRequestBase
  : public QuotaRequestBase
{
protected:
  explicit ClearRequestBase(bool aExclusive)
    : QuotaRequestBase(aExclusive)
  {
    AssertIsOnOwningThread();
  }

  void
  DeleteFiles(QuotaManager* aQuotaManager,
              PersistenceType aPersistenceType);

  nsresult
  DoDirectoryWork(QuotaManager* aQuotaManager) override;
};

class ClearOriginOp final
  : public ClearRequestBase
{
  const ClearOriginParams mParams;

public:
  explicit ClearOriginOp(const RequestParams& aParams);

  bool
  Init(Quota* aQuota) override;

private:
  ~ClearOriginOp()
  { }

  nsresult
  DoInitOnMainThread() override;

  void
  GetResponse(RequestResponse& aResponse) override;
};

class ClearDataOp final
  : public ClearRequestBase
{
  const ClearDataParams mParams;

public:
  explicit ClearDataOp(const RequestParams& aParams);

  bool
  Init(Quota* aQuota) override;

private:
  ~ClearDataOp()
  { }

  nsresult
  DoInitOnMainThread() override;

  void
  GetResponse(RequestResponse& aResponse) override;
};

class PersistRequestBase
  : public QuotaRequestBase
{
  const PrincipalInfo mPrincipalInfo;

protected:
  nsCString mSuffix;
  nsCString mGroup;

public:
  bool
  Init(Quota* aQuota) override;

protected:
  explicit PersistRequestBase(const PrincipalInfo& aPrincipalInfo);

private:
  nsresult
  DoInitOnMainThread() override;
};

class PersistedOp final
  : public PersistRequestBase
{
  bool mPersisted;

public:
  explicit PersistedOp(const RequestParams& aParams);

private:
  ~PersistedOp()
  { }

  nsresult
  DoDirectoryWork(QuotaManager* aQuotaManager) override;

  void
  GetResponse(RequestResponse& aResponse) override;
};

class PersistOp final
  : public PersistRequestBase
{
public:
  explicit PersistOp(const RequestParams& aParams);

private:
  ~PersistOp()
  { }

  nsresult
  DoDirectoryWork(QuotaManager* aQuotaManager) override;

  void
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

bool
IsOSMetadata(const nsAString& aFileName)
{
  return aFileName.EqualsLiteral(DSSTORE_FILE_NAME);
}

bool
IsOriginMetadata(const nsAString& aFileName)
{
  return aFileName.EqualsLiteral(METADATA_FILE_NAME) ||
         aFileName.EqualsLiteral(METADATA_V2_FILE_NAME) ||
         IsOSMetadata(aFileName);
}

bool
IsTempMetadata(const nsAString& aFileName)
{
  return aFileName.EqualsLiteral(METADATA_TMP_FILE_NAME) ||
         aFileName.EqualsLiteral(METADATA_V2_TMP_FILE_NAME);
}

} // namespace

BackgroundThreadObject::BackgroundThreadObject()
  : mOwningThread(GetCurrentThreadEventTarget())
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
                          "Quota %s: %s:%" PRIu32, aStr, aFile, aLine)),
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

  const bool mPersistent;

public:
  StorageDirectoryHelper(nsIFile* aDirectory, bool aPersistent)
    : Runnable("dom::quota::StorageDirectoryHelper")
    , mMutex("StorageDirectoryHelper::mMutex")
    , mCondVar(mMutex, "StorageDirectoryHelper::mCondVar")
    , mMainThreadResultCode(NS_OK)
    , mWaiting(true)
    , mDirectory(aDirectory)
    , mPersistent(aPersistent)
  {
    AssertIsOnIOThread();
  }

protected:
  ~StorageDirectoryHelper()
  { }

  nsresult
  GetDirectoryMetadata(nsIFile* aDirectory,
                       int64_t& aTimestamp,
                       nsACString& aGroup,
                       nsACString& aOrigin,
                       Nullable<bool>& aIsApp);

  // Upgrade helper to load the contents of ".metadata-v2" files from previous
  // schema versions.  Although QuotaManager has a similar GetDirectoryMetadata2
  // method, it is only intended to read current version ".metadata-v2" files.
  // And unlike the old ".metadata" files, the ".metadata-v2" format can evolve
  // because our "storage.sqlite" lets us track the overall version of the
  // storage directory.
  nsresult
  GetDirectoryMetadata2(nsIFile* aDirectory,
                        int64_t& aTimestamp,
                        nsACString& aSuffix,
                        nsACString& aGroup,
                        nsACString& aOrigin,
                        bool& aIsApp);

  nsresult
  RemoveObsoleteOrigin(const OriginProps& aOriginProps);

  nsresult
  ProcessOriginDirectories();

  virtual nsresult
  ProcessOriginDirectory(const OriginProps& aOriginProps) = 0;

private:
  nsresult
  RunOnMainThread();

  NS_IMETHOD
  Run() override;
};

struct StorageDirectoryHelper::OriginProps
{
  enum Type
  {
    eChrome,
    eContent,
    eObsolete
  };

  nsCOMPtr<nsIFile> mDirectory;
  nsString mLeafName;
  nsCString mSpec;
  OriginAttributes mAttrs;
  int64_t mTimestamp;
  nsCString mSuffix;
  nsCString mGroup;
  nsCString mOrigin;

  Type mType;
  bool mNeedsRestore;
  bool mNeedsRestore2;
  bool mIgnore;

public:
  explicit OriginProps()
    : mTimestamp(0)
    , mType(eContent)
    , mNeedsRestore(false)
    , mNeedsRestore2(false)
    , mIgnore(false)
  { }

  nsresult
  Init(nsIFile* aDirectory);
};

class MOZ_STACK_CLASS OriginParser final
{
public:
  enum ResultType {
    InvalidOrigin,
    ObsoleteOrigin,
    ValidOrigin
  };

private:
  static bool
  IgnoreWhitespace(char16_t /* aChar */)
  {
    return false;
  }

  typedef nsCCharSeparatedTokenizerTemplate<IgnoreWhitespace> Tokenizer;

  enum SchemeType {
    eNone,
    eFile,
    eAbout
  };

  enum State {
    eExpectingAppIdOrScheme,
    eExpectingInMozBrowser,
    eExpectingScheme,
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
  const OriginAttributes mOriginAttributes;
  Tokenizer mTokenizer;

  uint32_t mAppId;
  nsCString mScheme;
  nsCString mHost;
  Nullable<uint32_t> mPort;
  nsTArray<nsCString> mPathnameComponents;
  nsCString mHandledTokens;

  SchemeType mSchemeType;
  State mState;
  bool mInIsolatedMozBrowser;
  bool mMaybeDriveLetter;
  bool mError;

public:
  OriginParser(const nsACString& aOrigin,
               const OriginAttributes& aOriginAttributes)
    : mOrigin(aOrigin)
    , mOriginAttributes(aOriginAttributes)
    , mTokenizer(aOrigin, '+')
    , mAppId(kNoAppId)
    , mPort()
    , mSchemeType(eNone)
    , mState(eExpectingAppIdOrScheme)
    , mInIsolatedMozBrowser(false)
    , mMaybeDriveLetter(false)
    , mError(false)
  { }

  static ResultType
  ParseOrigin(const nsACString& aOrigin,
              nsCString& aSpec,
              OriginAttributes* aAttrs);

  ResultType
  Parse(nsACString& aSpec, OriginAttributes* aAttrs);

private:
  void
  HandleScheme(const nsDependentCSubstring& aToken);

  void
  HandlePathnameComponent(const nsDependentCSubstring& aToken);

  void
  HandleToken(const nsDependentCSubstring& aToken);

  void
  HandleTrailingSeparator();
};

class CreateOrUpgradeDirectoryMetadataHelper final
  : public StorageDirectoryHelper
{
  nsCOMPtr<nsIFile> mPermanentStorageDir;

public:
  CreateOrUpgradeDirectoryMetadataHelper(nsIFile* aDirectory,
                                         bool aPersistent)
    : StorageDirectoryHelper(aDirectory, aPersistent)
  { }

  nsresult
  CreateOrUpgradeMetadataFiles();

private:
  nsresult
  MaybeUpgradeOriginDirectory(nsIFile* aDirectory);

  nsresult
  ProcessOriginDirectory(const OriginProps& aOriginProps) override;
};

class UpgradeStorageFrom0_0To1_0Helper final
  : public StorageDirectoryHelper
{
public:
  UpgradeStorageFrom0_0To1_0Helper(nsIFile* aDirectory,
                                   bool aPersistent)
    : StorageDirectoryHelper(aDirectory, aPersistent)
  { }

  nsresult
  DoUpgrade();

private:
  nsresult
  ProcessOriginDirectory(const OriginProps& aOriginProps) override;
};

class UpgradeStorageFrom1_0To2_0Helper final
  : public StorageDirectoryHelper
{
public:
  UpgradeStorageFrom1_0To2_0Helper(nsIFile* aDirectory,
                                   bool aPersistent)
    : StorageDirectoryHelper(aDirectory, aPersistent)
  { }

  nsresult
  DoUpgrade();

private:
  nsresult
  MaybeUpgradeClients(const OriginProps& aOriginProps);

  nsresult
  MaybeRemoveAppsData(const OriginProps& aOriginProps,
                      bool* aRemoved);

  nsresult
  MaybeStripObsoleteOriginAttributes(const OriginProps& aOriginProps,
                                     bool* aStripped);

  nsresult
  ProcessOriginDirectory(const OriginProps& aOriginProps) override;
};

class RestoreDirectoryMetadata2Helper final
  : public StorageDirectoryHelper
{
public:
  RestoreDirectoryMetadata2Helper(nsIFile* aDirectory,
                                  bool aPersistent)
    : StorageDirectoryHelper(aDirectory, aPersistent)
  { }

  nsresult
  RestoreMetadata2File();

private:
  nsresult
  ProcessOriginDirectory(const OriginProps& aOriginProps) override;
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

int64_t
GetLastModifiedTime(nsIFile* aFile, bool aPersistent)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aFile);

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

        if (IsOriginMetadata(leafName) ||
            IsTempMetadata(leafName)) {
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

  if (aPersistent) {
    return PR_Now();
  }

  int64_t timestamp = INT64_MIN;
  nsresult rv = Helper::GetLastModifiedTime(aFile, &timestamp);
  if (NS_FAILED(rv)) {
    timestamp = PR_Now();
  }

  return timestamp;
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
EnsureOriginDirectory(nsIFile* aDirectory, bool* aCreated)
{
  AssertIsOnIOThread();

  nsresult rv;

#ifndef RELEASE_OR_BETA
  bool exists;
  rv = aDirectory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    nsString leafName;
    nsresult rv = aDirectory->GetLeafName(leafName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!leafName.EqualsLiteral(kChromeOrigin)) {
      nsCString spec;
      OriginAttributes attrs;
      OriginParser::ResultType result =
        OriginParser::ParseOrigin(NS_ConvertUTF16toUTF8(leafName),
                                  spec,
                                  &attrs);
      if (NS_WARN_IF(result != OriginParser::ValidOrigin)) {
        QM_WARNING("Preventing creation of a new origin directory which is not "
                   "supported by our origin parser or is obsolete!");

        return NS_ERROR_FAILURE;
      }
    }
  }
#endif

  rv = EnsureDirectory(aDirectory, aCreated);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

enum FileFlag {
  kTruncateFileFlag,
  kUpdateFileFlag,
  kAppendFileFlag
};

nsresult
GetOutputStream(nsIFile* aFile,
                FileFlag aFileFlag,
                nsIOutputStream** aStream)
{
  AssertIsOnIOThread();

  nsresult rv;

  nsCOMPtr<nsIOutputStream> outputStream;
  switch (aFileFlag) {
    case kTruncateFileFlag: {
      rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream),
                                       aFile);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      break;
    }

    case kUpdateFileFlag: {
      bool exists;
      rv = aFile->Exists(&exists);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (!exists) {
        *aStream = nullptr;
        return NS_OK;
      }

      nsCOMPtr<nsIFileStream> stream;
      rv = NS_NewLocalFileStream(getter_AddRefs(stream), aFile);
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
                                       aFile,
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
GetBinaryOutputStream(nsIFile* aFile,
                      FileFlag aFileFlag,
                      nsIBinaryOutputStream** aStream)
{
  nsCOMPtr<nsIOutputStream> outputStream;
  nsresult rv = GetOutputStream(aFile,
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
                        const nsACString& aOrigin)
{
  AssertIsOnIOThread();

  OriginAttributes groupAttributes;

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

  OriginAttributes originAttributes;

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

  nsCOMPtr<nsIFile> file;
  nsresult rv = aDirectory->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = file->Append(NS_LITERAL_STRING(METADATA_TMP_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIBinaryOutputStream> stream;
  rv = GetBinaryOutputStream(file, kTruncateFileFlag, getter_AddRefs(stream));
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

  // Currently unused (used to be isApp).
  rv = stream->WriteBoolean(false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stream->Flush();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stream->Close();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = file->RenameTo(nullptr, NS_LITERAL_STRING(METADATA_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
CreateDirectoryMetadata2(nsIFile* aDirectory,
                         int64_t aTimestamp,
                         bool aPersisted,
                         const nsACString& aSuffix,
                         const nsACString& aGroup,
                         const nsACString& aOrigin)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);

  nsCOMPtr<nsIFile> file;
  nsresult rv = aDirectory->Clone(getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = file->Append(NS_LITERAL_STRING(METADATA_V2_TMP_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIBinaryOutputStream> stream;
  rv = GetBinaryOutputStream(file, kTruncateFileFlag, getter_AddRefs(stream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(stream);

  rv = stream->Write64(aTimestamp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stream->WriteBoolean(aPersisted);
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

  // The suffix isn't used right now, but we might need it in future. It's
  // a bit of redundancy we can live with given how painful is to upgrade
  // metadata files.
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

  // Currently unused (used to be isApp).
  rv = stream->WriteBoolean(false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stream->Flush();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = stream->Close();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = file->RenameTo(nullptr, NS_LITERAL_STRING(METADATA_V2_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
CreateDirectoryMetadataFiles(nsIFile* aDirectory,
                             bool aPersisted,
                             const nsACString& aSuffix,
                             const nsACString& aGroup,
                             const nsACString& aOrigin,
                             int64_t* aTimestamp)
{
  AssertIsOnIOThread();

  int64_t timestamp = PR_Now();

  nsresult rv = CreateDirectoryMetadata(aDirectory,
                                        timestamp,
                                        aSuffix,
                                        aGroup,
                                        aOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = CreateDirectoryMetadata2(aDirectory,
                                timestamp,
                                aPersisted,
                                aSuffix,
                                aGroup,
                                aOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aTimestamp) {
    *aTimestamp = timestamp;
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
                                     const Nullable<PersistenceType>& aPersistenceType,
                                     const nsACString& aGroup,
                                     const OriginScope& aOriginScope,
                                     const Nullable<Client::Type>& aClientType,
                                     bool aExclusive,
                                     bool aInternal,
                                     OpenDirectoryListener* aOpenListener)
  : mQuotaManager(aQuotaManager)
  , mPersistenceType(aPersistenceType)
  , mGroup(aGroup)
  , mOriginScope(aOriginScope)
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

  nsresult rv =
    observerService->AddObserver(observer,
                                 PROFILE_BEFORE_CHANGE_QM_OBSERVER_ID,
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

  for (RefPtr<Client>& client : mManager->mClients) {
    client->DidInitialize(mManager);
  }

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
      aThread = GetMainThreadEventTarget();
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
  MOZ_ASSERT(!strcmp(aTopic, PROFILE_BEFORE_CHANGE_QM_OBSERVER_ID));
  MOZ_ASSERT(gInstance);

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (NS_WARN_IF(!observerService)) {
    return NS_ERROR_FAILURE;
  }

  // Unregister ourselves from the observer service first to make sure the
  // nested event loop below will not cause re-entrancy issues.
  Unused <<
    observerService->RemoveObserver(this,
                                    PROFILE_BEFORE_CHANGE_QM_OBSERVER_ID);

  QuotaManagerService* qms = QuotaManagerService::Get();
  MOZ_ASSERT(qms);

  qms->NoteShuttingDownManager();

  for (RefPtr<Client>& client : gInstance->mClients) {
    client->WillShutdown();
  }

  bool done = false;

  RefPtr<ShutdownRunnable> shutdownRunnable = new ShutdownRunnable(done);
  MOZ_ALWAYS_SUCCEEDS(
    mBackgroundThread->Dispatch(shutdownRunnable, NS_DISPATCH_NORMAL));

  MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return done; }));

  return NS_OK;
}

/*******************************************************************************
 * Quota object
 ******************************************************************************/

NS_IMETHODIMP
QuotaObject::
StoragePressureRunnable::Run()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
  if (NS_WARN_IF(!obsSvc)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsISupportsPRUint64> wrapper =
    do_CreateInstance(NS_SUPPORTS_PRUINT64_CONTRACTID);
  if (NS_WARN_IF(!wrapper)) {
    return NS_ERROR_FAILURE;
  }

  wrapper->SetData(mUsage);

  obsSvc->NotifyObservers(wrapper, "QuotaManager::StoragePressure", u"");

  return NS_OK;
}

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

      if (!mOriginInfo->LockedPersisted()) {
        AssertNoUnderflow(groupInfo->mUsage, delta);
        groupInfo->mUsage -= delta;
      }

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

  uint64_t newGroupUsage = groupInfo->mUsage;
  if (!mOriginInfo->LockedPersisted()) {
    AssertNoOverflow(groupInfo->mUsage, delta);
    newGroupUsage += delta;

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
      MutexAutoUnlock autoUnlock(quotaManager->mQuotaMutex);

      // Notify pressure event.
      RefPtr<StoragePressureRunnable> storagePressureRunnable =
        new StoragePressureRunnable(quotaManager->mTemporaryStorageUsage);

      MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(storagePressureRunnable));

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

    newGroupUsage = groupInfo->mUsage;
    if (!mOriginInfo->LockedPersisted()) {
      AssertNoOverflow(groupInfo->mUsage, delta);
      newGroupUsage += delta;

      uint64_t groupUsage = groupInfo->mUsage;
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
    }

    AssertNoOverflow(quotaManager->mTemporaryStorageUsage, delta);
    newTemporaryStorageUsage = quotaManager->mTemporaryStorageUsage + delta;

    NS_ASSERTION(newTemporaryStorageUsage <=
                 quotaManager->mTemporaryStorageLimit, "How come?!");

    // Ok, we successfully freed enough space and the operation can continue
    // without throwing the quota error.
    mOriginInfo->mUsage = newUsage;
    if (!mOriginInfo->LockedPersisted()) {
      groupInfo->mUsage = newGroupUsage;
    }
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
  if (!mOriginInfo->LockedPersisted()) {
    groupInfo->mUsage = newGroupUsage;
  }
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
QuotaManager::CreateDirectoryLock(const Nullable<PersistenceType>& aPersistenceType,
                                  const nsACString& aGroup,
                                  const OriginScope& aOriginScope,
                                  const Nullable<Client::Type>& aClientType,
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
  MOZ_ASSERT_IF(!aInternal, !aClientType.IsNull());
  MOZ_ASSERT_IF(!aInternal, aClientType.Value() != Client::TYPE_MAX);
  MOZ_ASSERT_IF(!aInternal, aOpenListener);

  RefPtr<DirectoryLockImpl> lock = new DirectoryLockImpl(this,
                                                           aPersistenceType,
                                                           aGroup,
                                                           aOriginScope,
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
                                             const nsACString& aOrigin)
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
        MOZ_ASSERT(originInfo->mGroupInfo->mPersistenceType !=
                     PERSISTENCE_TYPE_PERSISTENT);

        if (originInfo->LockedPersisted()) {
          continue;
        }

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
                                       originInfo->mOrigin);
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
    mShutdownTimer->InitWithNamedFuncCallback(&ShutdownTimerCallback,
                                              this,
                                              DEFAULT_SHUTDOWN_TIMER_MS,
                                              nsITimer::TYPE_ONE_SHOT,
                                              "QuotaManager::ShutdownTimerCallback"));

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
    NewRunnableMethod("dom::quota::QuotaManager::ReleaseIOThreadObjects",
                      this,
                      &QuotaManager::ReleaseIOThreadObjects);
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
                                 uint64_t aUsageBytes,
                                 int64_t aAccessTime,
                                 bool aPersisted)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

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
    new OriginInfo(groupInfo, aOrigin, aUsageBytes, aAccessTime, aPersisted);
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
  AssertIsOnIOThread();

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
  nsAutoCString group;
  rv = NS_EscapeURL(aGroup, esc_Query, group, fallible);
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsAutoCString origin;
  rv = NS_EscapeURL(aOrigin, esc_Query, origin, fallible);
  NS_ENSURE_SUCCESS(rv, nullptr);

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

Nullable<bool>
QuotaManager::OriginPersisted(const nsACString& aGroup,
                              const nsACString& aOrigin)
{
  AssertIsOnIOThread();

  MutexAutoLock lock(mQuotaMutex);

  RefPtr<OriginInfo> originInfo = LockedGetOriginInfo(PERSISTENCE_TYPE_DEFAULT,
                                                      aGroup,
                                                      aOrigin);
  if (originInfo) {
    return Nullable<bool>(originInfo->LockedPersisted());
  }

  return Nullable<bool>();
}

void
QuotaManager::PersistOrigin(const nsACString& aGroup,
                            const nsACString& aOrigin)
{
  AssertIsOnIOThread();

  MutexAutoLock lock(mQuotaMutex);

  RefPtr<OriginInfo> originInfo = LockedGetOriginInfo(PERSISTENCE_TYPE_DEFAULT,
                                                      aGroup,
                                                      aOrigin);
  if (originInfo && !originInfo->LockedPersisted()) {
    originInfo->LockedPersist();
  }
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
                                    bool* aPersisted,
                                    nsACString& aSuffix,
                                    nsACString& aGroup,
                                    nsACString& aOrigin)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(aTimestamp);
  MOZ_ASSERT(aPersisted);
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

  // Currently unused (used to be isApp).
  bool dummy;
  rv = binaryStream->ReadBoolean(&dummy);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aTimestamp = timestamp;
  *aPersisted = persisted;
  aSuffix = suffix;
  aGroup = group;
  aOrigin = origin;
  return NS_OK;
}

nsresult
QuotaManager::GetDirectoryMetadata2WithRestore(nsIFile* aDirectory,
                                               bool aPersistent,
                                               int64_t* aTimestamp,
                                               bool* aPersisted,
                                               nsACString& aSuffix,
                                               nsACString& aGroup,
                                               nsACString& aOrigin)
{
  nsresult rv = GetDirectoryMetadata2(aDirectory,
                                      aTimestamp,
                                      aPersisted,
                                      aSuffix,
                                      aGroup,
                                      aOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    rv = RestoreDirectoryMetadata2(aDirectory, aPersistent);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = GetDirectoryMetadata2(aDirectory,
                               aTimestamp,
                               aPersisted,
                               aSuffix,
                               aGroup,
                               aOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult
QuotaManager::GetDirectoryMetadata2(nsIFile* aDirectory,
                                    int64_t* aTimestamp,
                                    bool* aPersisted)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(aTimestamp || aPersisted);
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

  bool persisted;
  if (aPersisted) {
    rv = binaryStream->ReadBoolean(&persisted);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (aTimestamp) {
    *aTimestamp = timestamp;
  }
  if (aPersisted) {
    *aPersisted = persisted;
  }
  return NS_OK;
}

nsresult
QuotaManager::GetDirectoryMetadata2WithRestore(nsIFile* aDirectory,
                                               bool aPersistent,
                                               int64_t* aTimestamp,
                                               bool* aPersisted)
{
  nsresult rv = GetDirectoryMetadata2(aDirectory, aTimestamp, aPersisted);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    rv = RestoreDirectoryMetadata2(aDirectory, aPersistent);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = GetDirectoryMetadata2(aDirectory, aTimestamp, aPersisted);
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

      if (IsOSMetadata(leafName)) {
        continue;
      }

      UNKNOWN_FILE_WARNING(leafName);
      return NS_ERROR_UNEXPECTED;
    }

    int64_t timestamp;
    bool persisted;
    nsCString suffix;
    nsCString group;
    nsCString origin;
    rv = GetDirectoryMetadata2WithRestore(childDirectory,
                                          /* aPersistent */ false,
                                          &timestamp,
                                          &persisted,
                                          suffix,
                                          group,
                                          origin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = InitializeOrigin(aPersistenceType, group, origin, timestamp, persisted,
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
                               int64_t aAccessTime,
                               bool aPersisted,
                               nsIFile* aDirectory)
{
  AssertIsOnIOThread();

  nsresult rv;

  bool trackQuota = aPersistenceType != PERSISTENCE_TYPE_PERSISTENT;

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

    bool isDirectory;
    rv = file->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsString leafName;
    rv = file->GetLeafName(leafName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!isDirectory) {
      if (IsOriginMetadata(leafName)) {
        continue;
      }

      if (IsTempMetadata(leafName)) {
        rv = file->Remove(/* recursive */ false);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        continue;
      }

      UNKNOWN_FILE_WARNING(leafName);
      return NS_ERROR_UNEXPECTED;
    }

    Client::Type clientType;
    rv = Client::TypeFromText(leafName, clientType);
    if (NS_FAILED(rv)) {
      UNKNOWN_FILE_WARNING(leafName);
      return NS_ERROR_UNEXPECTED;
    }

    Atomic<bool> dummy(false);
    rv = mClients[clientType]->InitOrigin(aPersistenceType,
                                          aGroup,
                                          aOrigin,
                                          /* aCanceled */ dummy,
                                          usageInfo);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (trackQuota) {
    InitQuotaForOrigin(aPersistenceType, aGroup, aOrigin,
                       usageInfo->TotalUsage(), aAccessTime, aPersisted);
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
QuotaManager::UpgradeStorageFrom0_0To1_0(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  nsresult rv = MaybeUpgradeIndexedDBDirectory();
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
    RefPtr<UpgradeStorageFrom0_0To1_0Helper> helper =
      new UpgradeStorageFrom0_0To1_0Helper(directory, persistent);

    rv = helper->DoUpgrade();
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

  rv = aConnection->SetSchemaVersion(MakeStorageVersion(1, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
QuotaManager::UpgradeStorageFrom1_0To2_0(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aConnection);

  // The upgrade consists of a number of logically distinct bugs that
  // intentionally got fixed at the same time to trigger just one major
  // version bump.
  //
  //
  // Morgue directory cleanup
  // [Feature/Bug]:
  // The original bug that added "on demand" morgue cleanup is 1165119.
  //
  // [Mutations]:
  // Morgue directories are removed from all origin directories during the
  // upgrade process. Origin initialization and usage calculation doesn't try
  // to remove morgue directories anymore.
  //
  // [Downgrade-incompatible changes]:
  // Morgue directories can reappear if user runs an already upgraded profile
  // in an older version of Firefox. Morgue directories then prevent current
  // Firefox from initializing and using the storage.
  //
  //
  // App data removal
  // [Feature/Bug]:
  // The bug that removes isApp flags is 1311057.
  //
  // [Mutations]:
  // Origin directories with appIds are removed during the upgrade process.
  //
  // [Downgrade-incompatible changes]:
  // Origin directories with appIds can reappear if user runs an already
  // upgraded profile in an older version of Firefox. Origin directories with
  // appIds don't prevent current Firefox from initializing and using the
  // storage, but they wouldn't ever be removed again, potentially causing
  // problems once appId is removed from origin attributes.
  //
  //
  // Strip obsolete origin attributes
  // [Feature/Bug]:
  // The bug that strips obsolete origin attributes is 1314361.
  //
  // [Mutations]:
  // Origin directories with obsolete origin attributes are renamed and their
  // metadata files are updated during the upgrade process.
  //
  // [Downgrade-incompatible changes]:
  // Origin directories with obsolete origin attributes can reappear if user
  // runs an already upgraded profile in an older version of Firefox. Origin
  // directories with obsolete origin attributes don't prevent current Firefox
  // from initializing and using the storage, but they wouldn't ever be upgraded
  // again, potentially causing problems in future.
  //
  //
  // File manager directory renaming (client specific)
  // [Feature/Bug]:
  // The original bug that added "on demand" file manager directory renaming is
  // 1056939.
  //
  // [Mutations]:
  // All file manager directories are renamed to contain the ".files" suffix.
  //
  // [Downgrade-incompatible changes]:
  // File manager directories with the ".files" suffix prevent older versions of
  // Firefox from initializing and using the storage.
  // File manager directories without the ".files" suffix can appear if user
  // runs an already upgraded profile in an older version of Firefox. File
  // manager directories without the ".files" suffix then prevent current
  // Firefox from initializing and using the storage.

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

    bool exists;
    rv = directory->Exists(&exists);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!exists) {
      continue;
    }

    bool persistent = persistenceType == PERSISTENCE_TYPE_PERSISTENT;
    RefPtr<UpgradeStorageFrom1_0To2_0Helper> helper =
      new UpgradeStorageFrom1_0To2_0Helper(directory, persistent);

    rv = helper->DoUpgrade();
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

    MOZ_ASSERT(storageVersion == MakeStorageVersion(1, 0));
  }
#endif

  rv = aConnection->SetSchemaVersion(MakeStorageVersion(2, 0));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

#ifdef DEBUG

void
QuotaManager::AssertStorageIsInitialized() const
{
  AssertIsOnIOThread();
  MOZ_ASSERT(mStorageInitialized);
}

#endif // DEBUG

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

    nsCOMPtr<nsIFile> storageDir =
      do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = storageDir->InitWithPath(mStoragePath);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    bool exists;
    rv = storageDir->Exists(&exists);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!exists) {
      nsCOMPtr<nsIFile> indexedDBDir =
        do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = indexedDBDir->InitWithPath(mIndexedDBPath);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = indexedDBDir->Exists(&exists);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    const bool newDirectory = !exists;

    if (newDatabase) {
      // Set the page size first.
      if (kSQLitePageSizeOverride) {
        rv = connection->ExecuteSimpleSQL(
          nsPrintfCString("PRAGMA page_size = %" PRIu32 ";", kSQLitePageSizeOverride)
        );
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }

    mozStorageTransaction transaction(connection, false,
                                  mozIStorageConnection::TRANSACTION_IMMEDIATE);

    // An upgrade method can upgrade the database, the storage or both.
    // The upgrade loop below can only be avoided when there's no database and
    // no storage yet (e.g. new profile).
    if (newDatabase && newDirectory) {
      rv = CreateTables(connection);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      MOZ_ASSERT(NS_SUCCEEDED(connection->GetSchemaVersion(&storageVersion)));
      MOZ_ASSERT(storageVersion == kStorageVersion);
    } else {
      // This logic needs to change next time we change the storage!
      static_assert(kStorageVersion == int32_t((2 << 16) + 0),
                    "Upgrade function needed due to storage version increase.");

      while (storageVersion != kStorageVersion) {
        if (storageVersion == 0) {
          rv = UpgradeStorageFrom0_0To1_0(connection);
        } else if (storageVersion == MakeStorageVersion(1, 0)) {
          rv = UpgradeStorageFrom1_0To2_0(connection);
        } else {
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
                            Client::Type aClientType,
                            bool aExclusive,
                            OpenDirectoryListener* aOpenListener)
{
  AssertIsOnOwningThread();

  RefPtr<DirectoryLockImpl> lock =
    CreateDirectoryLock(Nullable<PersistenceType>(aPersistenceType),
                        aGroup,
                        OriginScope::FromOrigin(aOrigin),
                        Nullable<Client::Type>(aClientType),
                        aExclusive,
                        false,
                        aOpenListener);
  MOZ_ASSERT(lock);
}

void
QuotaManager::OpenDirectoryInternal(const Nullable<PersistenceType>& aPersistenceType,
                                    const OriginScope& aOriginScope,
                                    const Nullable<Client::Type>& aClientType,
                                    bool aExclusive,
                                    OpenDirectoryListener* aOpenListener)
{
  AssertIsOnOwningThread();

  RefPtr<DirectoryLockImpl> lock =
    CreateDirectoryLock(aPersistenceType,
                        EmptyCString(),
                        aOriginScope,
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

  for (uint32_t index : IntegerRange(uint32_t(Client::TYPE_MAX))) {
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
                                        nsIFile** aDirectory)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);

  nsCOMPtr<nsIFile> directory;
  bool created;
  nsresult rv = EnsureOriginIsInitializedInternal(aPersistenceType,
                                                  aSuffix,
                                                  aGroup,
                                                  aOrigin,
                                                  getter_AddRefs(directory),
                                                  &created);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  directory.forget(aDirectory);
  return NS_OK;
}

nsresult
QuotaManager::EnsureOriginIsInitializedInternal(
                                               PersistenceType aPersistenceType,
                                               const nsACString& aSuffix,
                                               const nsACString& aGroup,
                                               const nsACString& aOrigin,
                                               nsIFile** aDirectory,
                                               bool* aCreated)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);
  MOZ_ASSERT(aCreated);

  nsresult rv = EnsureStorageIsInitialized();
  NS_ENSURE_SUCCESS(rv, rv);

  // Get directory for this origin and persistence type.
  nsCOMPtr<nsIFile> directory;
  rv = GetDirectoryForOrigin(aPersistenceType, aOrigin,
                             getter_AddRefs(directory));
  NS_ENSURE_SUCCESS(rv, rv);

  if (aPersistenceType == PERSISTENCE_TYPE_PERSISTENT) {
    if (mInitializedOrigins.Contains(aOrigin)) {
      directory.forget(aDirectory);
      *aCreated = false;
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
      mTemporaryStorageLimit = static_cast<uint64_t>(gFixedLimitKB) * 1024;
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

  bool created;
  rv = EnsureOriginDirectory(directory, &created);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int64_t timestamp;
  if (aPersistenceType == PERSISTENCE_TYPE_PERSISTENT) {
    if (created) {
      rv = CreateDirectoryMetadataFiles(directory,
                                        /* aPersisted */ true,
                                        aSuffix,
                                        aGroup,
                                        aOrigin,
                                        &timestamp);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    } else {
      rv = GetDirectoryMetadata2WithRestore(directory,
                                            /* aPersistent */ true,
                                            &timestamp,
                                            /* aPersisted */ nullptr);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      MOZ_ASSERT(timestamp <= PR_Now());
    }

    rv = InitializeOrigin(aPersistenceType, aGroup, aOrigin, timestamp,
                          /* aPersisted */ true, directory);
    NS_ENSURE_SUCCESS(rv, rv);

    mInitializedOrigins.AppendElement(aOrigin);
  } else if (created) {
    rv = CreateDirectoryMetadataFiles(directory,
                                      /* aPersisted */ false,
                                      aSuffix,
                                      aGroup,
                                      aOrigin,
                                      &timestamp);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = InitializeOrigin(aPersistenceType, aGroup, aOrigin, timestamp,
                          /* aPersisted */ false, directory);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  directory.forget(aDirectory);
  *aCreated = created;
  return NS_OK;
}

void
QuotaManager::OriginClearCompleted(PersistenceType aPersistenceType,
                                   const nsACString& aOrigin)
{
  AssertIsOnIOThread();

  if (aPersistenceType == PERSISTENCE_TYPE_PERSISTENT) {
    mInitializedOrigins.RemoveElement(aOrigin);
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

void
QuotaManager::GetGroupUsageAndLimit(const nsACString& aGroup,
                                    UsageInfo* aUsageInfo)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aUsageInfo);

  {
    MutexAutoLock lock(mQuotaMutex);

    aUsageInfo->SetLimit(GetGroupLimit());
    aUsageInfo->ResetUsage();

    GroupInfoPair* pair;
    if (!mGroupInfoPairs.Get(aGroup, &pair)) {
      return;
    }

    // Calculate temporary group usage
    RefPtr<GroupInfo> temporaryGroupInfo =
      pair->LockedGetGroupInfo(PERSISTENCE_TYPE_TEMPORARY);
    if (temporaryGroupInfo) {
      aUsageInfo->AppendToDatabaseUsage(temporaryGroupInfo->mUsage);
    }

    // Calculate default group usage
    RefPtr<GroupInfo> defaultGroupInfo =
      pair->LockedGetGroupInfo(PERSISTENCE_TYPE_DEFAULT);
    if (defaultGroupInfo) {
      aUsageInfo->AppendToDatabaseUsage(defaultGroupInfo->mUsage);
    }
  }
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

// static
nsresult
QuotaManager::GetInfoFromPrincipal(nsIPrincipal* aPrincipal,
                                   nsACString* aSuffix,
                                   nsACString* aGroup,
                                   nsACString* aOrigin)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  if (nsContentUtils::IsSystemPrincipal(aPrincipal)) {
    GetInfoForChrome(aSuffix, aGroup, aOrigin);
    return NS_OK;
  }


  if (aPrincipal->GetIsNullPrincipal()) {
    NS_WARNING("IndexedDB not supported from this principal!");
    return NS_ERROR_FAILURE;
  }

  nsCString origin;
  nsresult rv = aPrincipal->GetOrigin(origin);
  NS_ENSURE_SUCCESS(rv, rv);

  if (origin.EqualsLiteral(kChromeOrigin)) {
    NS_WARNING("Non-chrome principal can't use chrome origin!");
    return NS_ERROR_FAILURE;
  }

  nsCString suffix;
  aPrincipal->OriginAttributesRef().CreateSuffix(suffix);

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

  return NS_OK;
}

// static
nsresult
QuotaManager::GetInfoFromWindow(nsPIDOMWindowOuter* aWindow,
                                nsACString* aSuffix,
                                nsACString* aGroup,
                                nsACString* aOrigin)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);

  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aWindow);
  NS_ENSURE_TRUE(sop, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPrincipal> principal = sop->GetPrincipal();
  NS_ENSURE_TRUE(principal, NS_ERROR_FAILURE);

  nsresult rv =
    GetInfoFromPrincipal(principal, aSuffix, aGroup, aOrigin);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// static
void
QuotaManager::GetInfoForChrome(nsACString* aSuffix,
                               nsACString* aGroup,
                               nsACString* aOrigin)
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
}

// static
bool
QuotaManager::IsOriginInternal(const nsACString& aOrigin)
{
  // The first prompt is not required for these origins.
  if (aOrigin.EqualsLiteral(kChromeOrigin) ||
      StringBeginsWith(aOrigin, nsDependentCString(kAboutHomeOriginPrefix)) ||
      StringBeginsWith(aOrigin, nsDependentCString(kIndexedDBOriginPrefix)) ||
      StringBeginsWith(aOrigin, nsDependentCString(kResourceOriginPrefix))) {
    return true;
  }

  return false;
}

// static
void
QuotaManager::ChromeOrigin(nsACString& aOrigin)
{
  aOrigin.AssignLiteral(kChromeOrigin);
}

// static
bool
QuotaManager::AreOriginsEqualOnDisk(nsACString& aOrigin1,
                                    nsACString& aOrigin2)
{
  nsCString origin1Sanitized(aOrigin1);
  SanitizeOriginString(origin1Sanitized);

  nsCString origin2Sanitized(aOrigin2);
  SanitizeOriginString(origin2Sanitized);

  return origin1Sanitized == origin2Sanitized;
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

already_AddRefed<OriginInfo>
QuotaManager::LockedGetOriginInfo(PersistenceType aPersistenceType,
                                  const nsACString& aGroup,
                                  const nsACString& aOrigin)
{
  mQuotaMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(aPersistenceType != PERSISTENCE_TYPE_PERSISTENT);

  GroupInfoPair* pair;
  if (mGroupInfoPairs.Get(aGroup, &pair)) {
    RefPtr<GroupInfo> groupInfo = pair->LockedGetGroupInfo(aPersistenceType);
    if (groupInfo) {
      return groupInfo->LockedGetOriginInfo(aOrigin);
    }
  }

  return nullptr;
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
            if (originInfo->LockedPersisted()) {
              continue;
            }

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
        if (doomedOriginInfos.Contains(originInfos[index - 1]) ||
            originInfos[index - 1]->LockedPersisted()) {
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

#ifdef DEBUG
    {
      MutexAutoLock lock(mQuotaMutex);
      MOZ_ASSERT(!doomedOriginInfo->LockedPersisted());
    }
#endif

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
      LockedRemoveQuotaForOrigin(persistenceType, group, origin);

#ifdef DEBUG
      doomedOriginInfos[index] = nullptr;
#endif

      doomedOrigins.AppendElement(OriginParams(persistenceType, origin));
    }
  }

  for (const OriginParams& doomedOrigin : doomedOrigins) {
    OriginClearCompleted(doomedOrigin.mPersistenceType,
                         doomedOrigin.mOrigin);
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

OriginInfo::OriginInfo(GroupInfo* aGroupInfo, const nsACString& aOrigin,
                       uint64_t aUsage, int64_t aAccessTime, bool aPersisted)
  : mGroupInfo(aGroupInfo), mOrigin(aOrigin), mUsage(aUsage),
    mAccessTime(aAccessTime), mPersisted(aPersisted)
{
  MOZ_ASSERT(aGroupInfo);
  MOZ_ASSERT_IF(aPersisted,
                aGroupInfo->mPersistenceType == PERSISTENCE_TYPE_DEFAULT);

  MOZ_COUNT_CTOR(OriginInfo);
}

void
OriginInfo::LockedDecreaseUsage(int64_t aSize)
{
  AssertCurrentThreadOwnsQuotaMutex();

  AssertNoUnderflow(mUsage, aSize);
  mUsage -= aSize;

  if (!LockedPersisted()) {
    AssertNoUnderflow(mGroupInfo->mUsage, aSize);
    mGroupInfo->mUsage -= aSize;
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  AssertNoUnderflow(quotaManager->mTemporaryStorageUsage, aSize);
  quotaManager->mTemporaryStorageUsage -= aSize;
}

void
OriginInfo::LockedPersist()
{
  AssertCurrentThreadOwnsQuotaMutex();
  MOZ_ASSERT(mGroupInfo->mPersistenceType == PERSISTENCE_TYPE_DEFAULT);
  MOZ_ASSERT(!mPersisted);

  mPersisted = true;

  // Remove Usage from GroupInfo
  AssertNoUnderflow(mGroupInfo->mUsage, mUsage);
  mGroupInfo->mUsage -= mUsage;
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

  if (!aOriginInfo->LockedPersisted()) {
    AssertNoOverflow(mUsage, aOriginInfo->mUsage);
    mUsage += aOriginInfo->mUsage;
  }

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
      if (!mOriginInfos[index]->LockedPersisted()) {
        AssertNoUnderflow(mUsage, mOriginInfos[index]->mUsage);
        mUsage -= mOriginInfos[index]->mUsage;
      }

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

    if (!originInfo->LockedPersisted()) {
      AssertNoUnderflow(mUsage, originInfo->mUsage);
      mUsage -= originInfo->mUsage;
    }

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
  : Runnable("dom::quota::CollectOriginsHelper")
  , mMinSizeToBeFreed(aMinSizeToBeFreed)
  , mMutex(aMutex)
  , mCondVar(aMutex, "CollectOriginsHelper::mCondVar")
  , mSizeToBeFreed(0)
  , mWaiting(true)
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

  AUTO_PROFILER_LABEL("FinalizeOriginEvictionOp::DoDirectoryWork", OTHER);

  for (RefPtr<DirectoryLockImpl>& lock : mLocks) {
    aQuotaManager->OriginClearCompleted(lock->GetPersistenceType().Value(),
                                        lock->GetOriginScope().GetOrigin());
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

  AUTO_PROFILER_LABEL("SaveOriginAccessTimeOp::DoDirectoryWork", OTHER);

  nsCOMPtr<nsIFile> file;
  nsresult rv =
    aQuotaManager->GetDirectoryForOrigin(mPersistenceType.Value(),
                                         mOriginScope.GetOrigin(),
                                         getter_AddRefs(file));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = file->Append(NS_LITERAL_STRING(METADATA_V2_FILE_NAME));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIBinaryOutputStream> stream;
  rv = GetBinaryOutputStream(file, kUpdateFileFlag, getter_AddRefs(stream));
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
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != UsageRequestParams::T__None);

  RefPtr<QuotaUsageRequestBase> actor;

  switch (aParams.type()) {
    case UsageRequestParams::TAllUsageParams:
      actor = new GetUsageOp(aParams);
      break;

    case UsageRequestParams::TOriginUsageParams:
      actor = new GetOriginUsageOp(aParams);
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  MOZ_ASSERT(actor);

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

mozilla::ipc::IPCResult
Quota::RecvPQuotaUsageRequestConstructor(PQuotaUsageRequestParent* aActor,
                                         const UsageRequestParams& aParams)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != UsageRequestParams::T__None);

  auto* op = static_cast<QuotaUsageRequestBase*>(aActor);

  if (NS_WARN_IF(!op->Init(this))) {
    return IPC_FAIL_NO_REASON(this);
  }

  op->RunImmediately();
  return IPC_OK();
}

bool
Quota::DeallocPQuotaUsageRequestParent(PQuotaUsageRequestParent* aActor)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  // Transfer ownership back from IPDL.
  RefPtr<QuotaUsageRequestBase> actor =
    dont_AddRef(static_cast<QuotaUsageRequestBase*>(aActor));
  return true;
}

PQuotaRequestParent*
Quota::AllocPQuotaRequestParent(const RequestParams& aParams)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  if (aParams.type() == RequestParams::TClearDataParams) {
    PBackgroundParent* actor = Manager();
    MOZ_ASSERT(actor);

    if (BackgroundParent::IsOtherProcessActor(actor)) {
      ASSERT_UNLESS_FUZZING();
      return nullptr;
    }
  }

  RefPtr<QuotaRequestBase> actor;

  switch (aParams.type()) {
    case RequestParams::TInitParams:
      actor = new InitOp();
      break;

    case RequestParams::TInitOriginParams:
      actor = new InitOriginOp(aParams);
      break;

    case RequestParams::TClearOriginParams:
      actor = new ClearOriginOp(aParams);
      break;

    case RequestParams::TClearDataParams:
      actor = new ClearDataOp(aParams);
      break;

    case RequestParams::TClearAllParams:
      actor = new ResetOrClearOp(/* aClear */ true);
      break;

    case RequestParams::TResetAllParams:
      actor = new ResetOrClearOp(/* aClear */ false);
      break;

    case RequestParams::TPersistedParams:
      actor = new PersistedOp(aParams);
      break;

    case RequestParams::TPersistParams:
      actor = new PersistOp(aParams);
      break;

    default:
      MOZ_CRASH("Should never get here!");
  }

  MOZ_ASSERT(actor);

  // Transfer ownership to IPDL.
  return actor.forget().take();
}

mozilla::ipc::IPCResult
Quota::RecvPQuotaRequestConstructor(PQuotaRequestParent* aActor,
                                    const RequestParams& aParams)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  auto* op = static_cast<QuotaRequestBase*>(aActor);

  if (NS_WARN_IF(!op->Init(this))) {
    return IPC_FAIL_NO_REASON(this);
  }

  op->RunImmediately();
  return IPC_OK();
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

mozilla::ipc::IPCResult
Quota::RecvStartIdleMaintenance()
{
  AssertIsOnBackgroundThread();

  PBackgroundParent* actor = Manager();
  MOZ_ASSERT(actor);

  if (BackgroundParent::IsOtherProcessActor(actor)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (QuotaManager::IsShuttingDown()) {
    return IPC_OK();
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    nsCOMPtr<nsIRunnable> callback =
      NewRunnableMethod("dom::quota::Quota::StartIdleMaintenance",
                        this,
                        &Quota::StartIdleMaintenance);

    QuotaManager::GetOrCreate(callback);
    return IPC_OK();
  }

  quotaManager->StartIdleMaintenance();

  return IPC_OK();
}

mozilla::ipc::IPCResult
Quota::RecvStopIdleMaintenance()
{
  AssertIsOnBackgroundThread();

  PBackgroundParent* actor = Manager();
  MOZ_ASSERT(actor);

  if (BackgroundParent::IsOtherProcessActor(actor)) {
    ASSERT_UNLESS_FUZZING();
    return IPC_FAIL_NO_REASON(this);
  }

  if (QuotaManager::IsShuttingDown()) {
    return IPC_OK();
  }

  QuotaManager* quotaManager = QuotaManager::Get();
  if (!quotaManager) {
    return IPC_OK();
  }

  quotaManager->StopIdleMaintenance();

  return IPC_OK();
}

bool
QuotaUsageRequestBase::Init(Quota* aQuota)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aQuota);

  mNeedsQuotaManagerInit = true;

  return true;
}

nsresult
QuotaUsageRequestBase::GetUsageForOrigin(QuotaManager* aQuotaManager,
                                         PersistenceType aPersistenceType,
                                         const nsACString& aGroup,
                                         const nsACString& aOrigin,
                                         UsageInfo* aUsageInfo)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aQuotaManager);
  MOZ_ASSERT(aUsageInfo);
  MOZ_ASSERT(aUsageInfo->TotalUsage() == 0);

  nsCOMPtr<nsIFile> directory;
  nsresult rv = aQuotaManager->GetDirectoryForOrigin(aPersistenceType,
                                                     aOrigin,
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
      initialized = aQuotaManager->IsOriginInitialized(aOrigin);
    } else {
      initialized = aQuotaManager->IsTemporaryStorageInitialized();
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

      bool isDirectory;
      rv = file->IsDirectory(&isDirectory);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      nsString leafName;
      rv = file->GetLeafName(leafName);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      if (!isDirectory) {
        // We are maintaining existing behavior here (failing if the origin is
        // not yet initialized or just continuing otherwise).
        // This can possibly be used by developers to add temporary backups into
        // origin directories without losing get usage functionality.
        if (IsOriginMetadata(leafName)) {
          continue;
        }

        if (IsTempMetadata(leafName)) {
          if (!initialized) {
            rv = file->Remove(/* recursive */ false);
            if (NS_WARN_IF(NS_FAILED(rv))) {
              return rv;
            }
          }

          continue;
        }

        UNKNOWN_FILE_WARNING(leafName);
        if (!initialized) {
          return NS_ERROR_UNEXPECTED;
        }
        continue;
      }

      Client::Type clientType;
      rv = Client::TypeFromText(leafName, clientType);
      if (NS_FAILED(rv)) {
        UNKNOWN_FILE_WARNING(leafName);
        if (!initialized) {
          return NS_ERROR_UNEXPECTED;
        }
        continue;
      }

      Client* client = aQuotaManager->GetClient(clientType);
      MOZ_ASSERT(client);

      if (initialized) {
        rv = client->GetUsageForOrigin(aPersistenceType,
                                       aGroup,
                                       aOrigin,
                                       mCanceled,
                                       aUsageInfo);
      }
      else {
        rv = client->InitOrigin(aPersistenceType,
                                aGroup,
                                aOrigin,
                                mCanceled,
                                aUsageInfo);
      }
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

void
QuotaUsageRequestBase::SendResults()
{
  AssertIsOnOwningThread();

  if (IsActorDestroyed()) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = NS_ERROR_FAILURE;
    }
  } else {
    if (mCanceled) {
      mResultCode = NS_ERROR_FAILURE;
    }

    UsageRequestResponse response;

    if (NS_SUCCEEDED(mResultCode)) {
      GetResponse(response);
    } else {
      response = mResultCode;
    }

    Unused << PQuotaUsageRequestParent::Send__delete__(this, response);
  }
}

void
QuotaUsageRequestBase::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnOwningThread();

  NoteActorDestroyed();
}

mozilla::ipc::IPCResult
QuotaUsageRequestBase::RecvCancel()
{
  AssertIsOnOwningThread();

  if (mCanceled.exchange(true)) {
    NS_WARNING("Canceled more than once?!");
    return IPC_FAIL_NO_REASON(this);
  }

  return IPC_OK();
}

GetUsageOp::GetUsageOp(const UsageRequestParams& aParams)
  : mGetAll(aParams.get_AllUsageParams().getAll())
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aParams.type() == UsageRequestParams::TAllUsageParams);
}

nsresult
GetUsageOp::TraverseRepository(QuotaManager* aQuotaManager,
                               PersistenceType aPersistenceType)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aQuotaManager);

  nsresult rv;

  nsCOMPtr<nsIFile> directory =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = directory->InitWithPath(aQuotaManager->GetStoragePath(aPersistenceType));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = directory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    return NS_OK;
  }

  nsCOMPtr<nsISimpleEnumerator> entries;
  rv = directory->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool persistent = aPersistenceType == PERSISTENCE_TYPE_PERSISTENT;

  bool hasMore;
  while (NS_SUCCEEDED((rv = entries->HasMoreElements(&hasMore))) &&
         hasMore && !mCanceled) {
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

      if (!IsOSMetadata(leafName)) {
        UNKNOWN_FILE_WARNING(leafName);
      }
      continue;
    }

    int64_t timestamp;
    bool persisted;
    nsCString suffix;
    nsCString group;
    nsCString origin;
    rv = aQuotaManager->GetDirectoryMetadata2WithRestore(originDir,
                                                         persistent,
                                                         &timestamp,
                                                         &persisted,
                                                         suffix,
                                                         group,
                                                         origin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!mGetAll && aQuotaManager->IsOriginInternal(origin)) {
      continue;
    }

    OriginUsage* originUsage;

    // We can't store pointers to OriginUsage objects in the hashtable
    // since AppendElement() reallocates its internal array buffer as number
    // of elements grows.
    uint32_t index;
    if (mOriginUsagesIndex.Get(origin, &index)) {
      originUsage = &mOriginUsages[index];
    } else {
      index = mOriginUsages.Length();

      originUsage = mOriginUsages.AppendElement();

      originUsage->origin() = origin;
      originUsage->persisted() = false;
      originUsage->usage() = 0;

      mOriginUsagesIndex.Put(origin, index);
    }

    if (aPersistenceType == PERSISTENCE_TYPE_DEFAULT) {
      originUsage->persisted() = persisted;
    }

    UsageInfo usageInfo;
    rv = GetUsageForOrigin(aQuotaManager,
                           aPersistenceType,
                           group,
                           origin,
                           &usageInfo);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    originUsage->usage() = originUsage->usage() + usageInfo.TotalUsage();
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
GetUsageOp::DoDirectoryWork(QuotaManager* aQuotaManager)
{
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("GetUsageOp::DoDirectoryWork", OTHER);

  nsresult rv;

  for (const PersistenceType type : kAllPersistenceTypes) {
    rv = TraverseRepository(aQuotaManager, type);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

void
GetUsageOp::GetResponse(UsageRequestResponse& aResponse)
{
  AssertIsOnOwningThread();

  aResponse = AllUsageResponse();

  if (!mOriginUsages.IsEmpty()) {
    nsTArray<OriginUsage>& originUsages =
      aResponse.get_AllUsageResponse().originUsages();

    mOriginUsages.SwapElements(originUsages);
  }
}

GetOriginUsageOp::GetOriginUsageOp(const UsageRequestParams& aParams)
  : mParams(aParams.get_OriginUsageParams())
  , mGetGroupUsage(aParams.get_OriginUsageParams().getGroupUsage())
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aParams.type() == UsageRequestParams::TOriginUsageParams);
}

bool
GetOriginUsageOp::Init(Quota* aQuota)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aQuota);

  if (NS_WARN_IF(!QuotaUsageRequestBase::Init(aQuota))) {
    return false;
  }

  mNeedsMainThreadInit = true;

  return true;
}

nsresult
GetOriginUsageOp::DoInitOnMainThread()
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
  rv = QuotaManager::GetInfoFromPrincipal(principal, &mSuffix, &mGroup,
                                          &origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mOriginScope.SetFromOrigin(origin);

  return NS_OK;
}

nsresult
GetOriginUsageOp::DoDirectoryWork(QuotaManager* aQuotaManager)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(mUsageInfo.TotalUsage() == 0);

  AUTO_PROFILER_LABEL("GetOriginUsageOp::DoDirectoryWork", OTHER);

  nsresult rv;

  if (mGetGroupUsage) {
    nsCOMPtr<nsIFile> directory;

    // Ensure origin is initialized first. It will initialize all origins for
    // temporary storage including origins belonging to our group.
    rv = aQuotaManager->EnsureOriginIsInitialized(PERSISTENCE_TYPE_TEMPORARY,
                                                  mSuffix, mGroup,
                                                  mOriginScope.GetOrigin(),
                                                  getter_AddRefs(directory));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Get cached usage and limit (the method doesn't have to stat any files).
    aQuotaManager->GetGroupUsageAndLimit(mGroup, &mUsageInfo);

    return NS_OK;
  }

  // Add all the persistent/temporary/default storage files we care about.
  for (const PersistenceType type : kAllPersistenceTypes) {
    UsageInfo usageInfo;
    rv = GetUsageForOrigin(aQuotaManager,
                           type,
                           mGroup,
                           mOriginScope.GetOrigin(),
                           &usageInfo);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mUsageInfo.Append(usageInfo);
  }

  return NS_OK;
}

void
GetOriginUsageOp::GetResponse(UsageRequestResponse& aResponse)
{
  AssertIsOnOwningThread();

  OriginUsageResponse usageResponse;

  // We'll get the group usage when mGetGroupUsage is true and get the
  // origin usage when mGetGroupUsage is false.
  usageResponse.usage() = mUsageInfo.TotalUsage();

  if (mGetGroupUsage) {
    usageResponse.limit() = mUsageInfo.Limit();
  } else {
    usageResponse.fileUsage() = mUsageInfo.FileUsage();
  }

  aResponse = usageResponse;
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

nsresult
InitOp::DoDirectoryWork(QuotaManager* aQuotaManager)
{
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("InitOp::DoDirectoryWork", OTHER);

  aQuotaManager->AssertStorageIsInitialized();

  return NS_OK;
}

void
InitOp::GetResponse(RequestResponse& aResponse)
{
  AssertIsOnOwningThread();

  aResponse = InitResponse();
}

InitOriginOp::InitOriginOp(const RequestParams& aParams)
  : QuotaRequestBase(/* aExclusive */ false)
  , mParams(aParams.get_InitOriginParams())
  , mCreated(false)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aParams.type() == RequestParams::TInitOriginParams);
}

bool
InitOriginOp::Init(Quota* aQuota)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aQuota);

  if (NS_WARN_IF(!QuotaRequestBase::Init(aQuota))) {
    return false;
  }

  MOZ_ASSERT(mParams.persistenceType() != PERSISTENCE_TYPE_INVALID);

  mPersistenceType.SetValue(mParams.persistenceType());

  mNeedsMainThreadInit = true;

  return true;
}

nsresult
InitOriginOp::DoInitOnMainThread()
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
  rv = QuotaManager::GetInfoFromPrincipal(principal, &mSuffix, &mGroup,
                                          &origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mOriginScope.SetFromOrigin(origin);

  return NS_OK;
}

nsresult
InitOriginOp::DoDirectoryWork(QuotaManager* aQuotaManager)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(!mPersistenceType.IsNull());

  AUTO_PROFILER_LABEL("InitOriginOp::DoDirectoryWork", OTHER);

  nsCOMPtr<nsIFile> directory;
  bool created;
  nsresult rv =
    aQuotaManager->EnsureOriginIsInitializedInternal(mPersistenceType.Value(),
                                                     mSuffix,
                                                     mGroup,
                                                     mOriginScope.GetOrigin(),
                                                     getter_AddRefs(directory),
                                                     &created);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mCreated = created;

  return NS_OK;
}

void
InitOriginOp::GetResponse(RequestResponse& aResponse)
{
  AssertIsOnOwningThread();

  InitOriginResponse response;

  response.created() = mCreated;

  aResponse = response;
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

  AUTO_PROFILER_LABEL("ResetOrClearOp::DoDirectoryWork", OTHER);

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

void
ClearRequestBase::DeleteFiles(QuotaManager* aQuotaManager,
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
  } else if (originScope.IsPrefix()) {
    nsCString prefixSanitized(originScope.GetPrefix());
    SanitizeOriginString(prefixSanitized);
    originScope.SetPrefix(prefixSanitized);
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
      // Unknown files during clearing are allowed. Just warn if we find them.
      if (!IsOSMetadata(leafName)) {
        UNKNOWN_FILE_WARNING(leafName);
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
    bool persisted;
    rv = aQuotaManager->GetDirectoryMetadata2WithRestore(file,
                                                         persistent,
                                                         &timestamp,
                                                         &persisted,
                                                         suffix,
                                                         group,
                                                         origin);
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

    aQuotaManager->OriginClearCompleted(aPersistenceType, origin);
  }

}

nsresult
ClearRequestBase::DoDirectoryWork(QuotaManager* aQuotaManager)
{
  AssertIsOnIOThread();

  AUTO_PROFILER_LABEL("ClearRequestBase::DoDirectoryWork", OTHER);

  if (mPersistenceType.IsNull()) {
    for (const PersistenceType type : kAllPersistenceTypes) {
      DeleteFiles(aQuotaManager, type);
    }
  } else {
    DeleteFiles(aQuotaManager, mPersistenceType.Value());
  }

  return NS_OK;
}

ClearOriginOp::ClearOriginOp(const RequestParams& aParams)
  : ClearRequestBase(/* aExclusive */ true)
  , mParams(aParams)
{
  MOZ_ASSERT(aParams.type() == RequestParams::TClearOriginParams);
}

bool
ClearOriginOp::Init(Quota* aQuota)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aQuota);

  if (NS_WARN_IF(!QuotaRequestBase::Init(aQuota))) {
    return false;
  }

  if (mParams.persistenceTypeIsExplicit()) {
    MOZ_ASSERT(mParams.persistenceType() != PERSISTENCE_TYPE_INVALID);

    mPersistenceType.SetValue(mParams.persistenceType());
  }

  mNeedsMainThreadInit = true;

  return true;
}

nsresult
ClearOriginOp::DoInitOnMainThread()
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
  rv = QuotaManager::GetInfoFromPrincipal(principal, nullptr, nullptr,
                                          &origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mParams.clearAll()) {
    mOriginScope.SetFromPrefix(origin);
  } else {
    mOriginScope.SetFromOrigin(origin);
  }

  return NS_OK;
}

void
ClearOriginOp::GetResponse(RequestResponse& aResponse)
{
  AssertIsOnOwningThread();

  aResponse = ClearOriginResponse();
}

ClearDataOp::ClearDataOp(const RequestParams& aParams)
  : ClearRequestBase(/* aExclusive */ true)
  , mParams(aParams)
{
  MOZ_ASSERT(aParams.type() == RequestParams::TClearDataParams);
}

bool
ClearDataOp::Init(Quota* aQuota)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aQuota);

  if (NS_WARN_IF(!QuotaRequestBase::Init(aQuota))) {
    return false;
  }

  mNeedsMainThreadInit = true;

  return true;
}

nsresult
ClearDataOp::DoInitOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(GetState() == State_Initializing);
  MOZ_ASSERT(mNeedsMainThreadInit);

  mOriginScope.SetFromJSONPattern(mParams.pattern());

  return NS_OK;
}

void
ClearDataOp::GetResponse(RequestResponse& aResponse)
{
  AssertIsOnOwningThread();

  aResponse = ClearDataResponse();
}

PersistRequestBase::PersistRequestBase(const PrincipalInfo& aPrincipalInfo)
  : QuotaRequestBase(/* aExclusive */ false)
  , mPrincipalInfo(aPrincipalInfo)
{
  AssertIsOnOwningThread();
}

bool
PersistRequestBase::Init(Quota* aQuota)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aQuota);

  if (NS_WARN_IF(!QuotaRequestBase::Init(aQuota))) {
    return false;
  }

  mPersistenceType.SetValue(PERSISTENCE_TYPE_DEFAULT);

  mNeedsMainThreadInit = true;

  return true;
}

nsresult
PersistRequestBase::DoInitOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(GetState() == State_Initializing);
  MOZ_ASSERT(mNeedsMainThreadInit);

  nsresult rv;
  nsCOMPtr<nsIPrincipal> principal =
    PrincipalInfoToPrincipal(mPrincipalInfo, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Figure out which origin we're dealing with.
  nsCString origin;
  rv = QuotaManager::GetInfoFromPrincipal(principal, &mSuffix, &mGroup,
                                          &origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mOriginScope.SetFromOrigin(origin);

  return NS_OK;
}

PersistedOp::PersistedOp(const RequestParams& aParams)
  : PersistRequestBase(aParams.get_PersistedParams().principalInfo())
  , mPersisted(false)
{
  MOZ_ASSERT(aParams.type() == RequestParams::TPersistedParams);
}

nsresult
PersistedOp::DoDirectoryWork(QuotaManager* aQuotaManager)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(!mPersistenceType.IsNull());
  MOZ_ASSERT(mPersistenceType.Value() == PERSISTENCE_TYPE_DEFAULT);
  MOZ_ASSERT(mOriginScope.IsOrigin());

  AUTO_PROFILER_LABEL("PersistedOp::DoDirectoryWork", OTHER);

  Nullable<bool> persisted =
    aQuotaManager->OriginPersisted(mGroup, mOriginScope.GetOrigin());

  if (!persisted.IsNull()) {
    mPersisted = persisted.Value();
    return NS_OK;
  }

  // If we get here, it means the origin hasn't been initialized yet.
  // Try to get the persisted flag from directory metadata on disk.

  nsCOMPtr<nsIFile> directory;
  nsresult rv = aQuotaManager->GetDirectoryForOrigin(mPersistenceType.Value(),
                                                     mOriginScope.GetOrigin(),
                                                     getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = directory->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    // Get the persisted flag.
    bool persisted;
    rv =
      aQuotaManager->GetDirectoryMetadata2WithRestore(directory,
                                                      /* aPersistent */ false,
                                                      /* aTimestamp */ nullptr,
                                                      &persisted);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mPersisted = persisted;
  } else {
    // The directory has not been created yet.
    mPersisted = false;
  }

  return NS_OK;
}

void
PersistedOp::GetResponse(RequestResponse& aResponse)
{
  AssertIsOnOwningThread();

  PersistedResponse persistedResponse;
  persistedResponse.persisted() = mPersisted;

  aResponse = persistedResponse;
}

PersistOp::PersistOp(const RequestParams& aParams)
  : PersistRequestBase(aParams.get_PersistParams().principalInfo())
{
  MOZ_ASSERT(aParams.type() == RequestParams::TPersistParams);
}

nsresult
PersistOp::DoDirectoryWork(QuotaManager* aQuotaManager)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(!mPersistenceType.IsNull());
  MOZ_ASSERT(mPersistenceType.Value() == PERSISTENCE_TYPE_DEFAULT);
  MOZ_ASSERT(mOriginScope.IsOrigin());

  AUTO_PROFILER_LABEL("PersistOp::DoDirectoryWork", OTHER);

  // Update directory metadata on disk first.
  nsCOMPtr<nsIFile> directory;
  nsresult rv = aQuotaManager->GetDirectoryForOrigin(mPersistenceType.Value(),
                                                     mOriginScope.GetOrigin(),
                                                     getter_AddRefs(directory));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool created;
  rv = EnsureOriginDirectory(directory, &created);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (created) {
    rv = CreateDirectoryMetadataFiles(directory,
                                      /* aPersisted */ true,
                                      mSuffix,
                                      mGroup,
                                      mOriginScope.GetOrigin(),
                                      /* aTimestamp */ nullptr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    // Get the persisted flag (restore the metadata file if necessary).
    bool persisted;
    rv =
      aQuotaManager->GetDirectoryMetadata2WithRestore(directory,
                                                      /* aPersistent */ false,
                                                      /* aTimestamp */ nullptr,
                                                      &persisted);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!persisted) {
      nsCOMPtr<nsIFile> file;
      nsresult rv = directory->Clone(getter_AddRefs(file));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = file->Append(NS_LITERAL_STRING(METADATA_V2_FILE_NAME));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      nsCOMPtr<nsIBinaryOutputStream> stream;
      rv = GetBinaryOutputStream(file, kUpdateFileFlag, getter_AddRefs(stream));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      MOZ_ASSERT(stream);

      // Update origin access time while we are here.
      rv = stream->Write64(PR_Now());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      // Set the persisted flag to true.
      rv = stream->WriteBoolean(true);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  // Directory metadata has been successfully created/updated, try to update
  // OriginInfo too (it's ok if OriginInfo doesn't exist yet).
  aQuotaManager->PersistOrigin(mGroup, mOriginScope.GetOrigin());

  return NS_OK;
}

void
PersistOp::GetResponse(RequestResponse& aResponse)
{
  AssertIsOnOwningThread();

  aResponse = PersistResponse();
}

nsresult
StorageDirectoryHelper::GetDirectoryMetadata(nsIFile* aDirectory,
                                             int64_t& aTimestamp,
                                             nsACString& aGroup,
                                             nsACString& aOrigin,
                                             Nullable<bool>& aIsApp)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);

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

  Nullable<bool> isApp;
  bool value;
  if (NS_SUCCEEDED(binaryStream->ReadBoolean(&value))) {
    isApp.SetValue(value);
  }

  aTimestamp = timestamp;
  aGroup = group;
  aOrigin = origin;
  aIsApp = Move(isApp);
  return NS_OK;
}

nsresult
StorageDirectoryHelper::GetDirectoryMetadata2(nsIFile* aDirectory,
                                              int64_t& aTimestamp,
                                              nsACString& aSuffix,
                                              nsACString& aGroup,
                                              nsACString& aOrigin,
                                              bool& aIsApp)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);

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

  aTimestamp = timestamp;
  aSuffix = suffix;
  aGroup = group;
  aOrigin = origin;
  aIsApp = isApp;
  return NS_OK;
}

nsresult
StorageDirectoryHelper::RemoveObsoleteOrigin(const OriginProps& aOriginProps)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aOriginProps.mDirectory);

  QM_WARNING("Deleting obsolete %s directory that is no longer a legal "
             "origin!", NS_ConvertUTF16toUTF8(aOriginProps.mLeafName).get());

  nsresult rv = aOriginProps.mDirectory->Remove(/* recursive */ true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
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

  nsresult rv;

  // Don't try to upgrade obsolete origins, remove them right after we detect
  // them.
  for (auto& originProps : mOriginProps) {
    if (originProps.mType == OriginProps::eObsolete) {
      MOZ_ASSERT(originProps.mSuffix.IsEmpty());
      MOZ_ASSERT(originProps.mGroup.IsEmpty());
      MOZ_ASSERT(originProps.mOrigin.IsEmpty());

      rv = RemoveObsoleteOrigin(originProps);
    } else {
      MOZ_ASSERT(!originProps.mGroup.IsEmpty());
      MOZ_ASSERT(!originProps.mOrigin.IsEmpty());

      rv = ProcessOriginDirectory(originProps);
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
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
        QuotaManager::GetInfoForChrome(&originProps.mSuffix,
                                       &originProps.mGroup,
                                       &originProps.mOrigin);
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
                                                &originProps.mOrigin);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        break;
      }

      case OriginProps::eObsolete: {
        // There's no way to get info for obsolete origins.
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

nsresult
StorageDirectoryHelper::
OriginProps::Init(nsIFile* aDirectory)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aDirectory);

  nsString leafName;
  nsresult rv = aDirectory->GetLeafName(leafName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (leafName.EqualsLiteral(kChromeOrigin)) {
    mDirectory = aDirectory;
    mLeafName = leafName;
    mSpec = kChromeOrigin;
    mType = eChrome;
  } else {
    nsCString spec;
    OriginAttributes attrs;
    OriginParser::ResultType result =
      OriginParser::ParseOrigin(NS_ConvertUTF16toUTF8(leafName), spec, &attrs);
    if (NS_WARN_IF(result == OriginParser::InvalidOrigin)) {
      return NS_ERROR_FAILURE;
    }

    mDirectory = aDirectory;
    mLeafName = leafName;
    mSpec = spec;
    mAttrs = attrs;
    mType = result == OriginParser::ObsoleteOrigin ? eObsolete : eContent;
  }

  return NS_OK;
}

// static
auto
OriginParser::ParseOrigin(const nsACString& aOrigin,
                          nsCString& aSpec,
                          OriginAttributes* aAttrs) -> ResultType
{
  MOZ_ASSERT(!aOrigin.IsEmpty());
  MOZ_ASSERT(aAttrs);

  OriginAttributes originAttributes;

  nsCString originNoSuffix;
  bool ok = originAttributes.PopulateFromOrigin(aOrigin, originNoSuffix);
  if (!ok) {
    return InvalidOrigin;
  }

  OriginParser parser(originNoSuffix, originAttributes);
  return parser.Parse(aSpec, aAttrs);
}

auto
OriginParser::Parse(nsACString& aSpec, OriginAttributes* aAttrs) -> ResultType
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

    return InvalidOrigin;
  }

  MOZ_ASSERT(mState == eComplete || mState == eHandledTrailingSeparator);

  if (mAppId == kNoAppId) {
    *aAttrs = mOriginAttributes;
  } else {
    MOZ_ASSERT(mOriginAttributes.mAppId == kNoAppId);

    *aAttrs = OriginAttributes(mAppId, mInIsolatedMozBrowser);
  }

  nsAutoCString spec(mScheme);

  if (mSchemeType == eFile) {
    spec.AppendLiteral("://");

    for (uint32_t count = mPathnameComponents.Length(), index = 0;
         index < count;
         index++) {
      spec.Append('/');
      spec.Append(mPathnameComponents[index]);
    }

    aSpec = spec;

    return ValidOrigin;
  }

  if (mSchemeType == eAbout) {
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

  return mScheme.EqualsLiteral("app") ? ObsoleteOrigin : ValidOrigin;
}

void
OriginParser::HandleScheme(const nsDependentCSubstring& aToken)
{
  MOZ_ASSERT(!aToken.IsEmpty());
  MOZ_ASSERT(mState == eExpectingAppIdOrScheme || mState == eExpectingScheme);

  bool isAbout = false;
  bool isFile = false;
  if (aToken.EqualsLiteral("http") ||
      aToken.EqualsLiteral("https") ||
      (isAbout = aToken.EqualsLiteral("about") ||
                 aToken.EqualsLiteral("moz-safe-about")) ||
      aToken.EqualsLiteral("indexeddb") ||
      (isFile = aToken.EqualsLiteral("file")) ||
      aToken.EqualsLiteral("app") ||
      aToken.EqualsLiteral("resource") ||
      aToken.EqualsLiteral("moz-extension")) {
    mScheme = aToken;

    if (isAbout) {
      mSchemeType = eAbout;
      mState = eExpectingHost;
    } else {
      if (isFile) {
        mSchemeType = eFile;
      }
      mState = eExpectingEmptyToken1;
    }

    return;
  }

  QM_WARNING("'%s' is not a valid scheme!", nsCString(aToken).get());

  mError = true;
}

void
OriginParser::HandlePathnameComponent(const nsDependentCSubstring& aToken)
{
  MOZ_ASSERT(!aToken.IsEmpty());
  MOZ_ASSERT(mState == eExpectingEmptyTokenOrDriveLetterOrPathnameComponent ||
             mState == eExpectingEmptyTokenOrPathnameComponent);
  MOZ_ASSERT(mSchemeType == eFile);

  mPathnameComponents.AppendElement(aToken);

  mState = mTokenizer.hasMoreTokens() ? eExpectingEmptyTokenOrPathnameComponent
                                      : eComplete;
}

void
OriginParser::HandleToken(const nsDependentCSubstring& aToken)
{
  switch (mState) {
    case eExpectingAppIdOrScheme: {
      if (aToken.IsEmpty()) {
        QM_WARNING("Expected an app id or scheme (not an empty string)!");

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

      HandleScheme(aToken);

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

      mState = eExpectingScheme;

      return;
    }

    case eExpectingScheme: {
      if (aToken.IsEmpty()) {
        QM_WARNING("Expected a scheme (not an empty string)!");

        mError = true;
        return;
      }

      HandleScheme(aToken);

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

      if (mSchemeType == eFile) {
        mState = eExpectingEmptyToken3;
      } else {
        mState = eExpectingHost;
      }

      return;
    }

    case eExpectingEmptyToken3: {
      MOZ_ASSERT(mSchemeType == eFile);

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
      MOZ_ASSERT(mSchemeType == eNone);

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
      MOZ_ASSERT(mSchemeType == eFile);

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
      MOZ_ASSERT(mSchemeType == eFile);

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
  MOZ_ASSERT(mSchemeType == eFile);

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
      // Unknown files during upgrade are allowed. Just warn if we find them.
      if (!IsOSMetadata(leafName)) {
        UNKNOWN_FILE_WARNING(leafName);
      }
      continue;
    }

    if (mPersistent) {
      rv = MaybeUpgradeOriginDirectory(originDir);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    OriginProps originProps;
    rv = originProps.Init(originDir);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!mPersistent) {
      int64_t timestamp;
      nsCString group;
      nsCString origin;
      Nullable<bool> isApp;
      rv = GetDirectoryMetadata(originDir,
                                timestamp,
                                group,
                                origin,
                                isApp);
      if (NS_FAILED(rv)) {
        originProps.mTimestamp = GetLastModifiedTime(originDir, mPersistent);
        originProps.mNeedsRestore = true;
      } else if (!isApp.IsNull()) {
        originProps.mIgnore = true;
      }
    }
    else {
      bool persistent = QuotaManager::IsOriginInternal(originProps.mSpec);
      originProps.mTimestamp = GetLastModifiedTime(originDir, persistent);
    }

    mOriginProps.AppendElement(Move(originProps));
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
CreateOrUpgradeDirectoryMetadataHelper::ProcessOriginDirectory(
                                                const OriginProps& aOriginProps)
{
  AssertIsOnIOThread();

  nsresult rv;

  if (mPersistent) {
    rv = CreateDirectoryMetadata(aOriginProps.mDirectory,
                                 aOriginProps.mTimestamp,
                                 aOriginProps.mSuffix,
                                 aOriginProps.mGroup,
                                 aOriginProps.mOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // Move internal origins to new persistent storage.
    if (QuotaManager::IsOriginInternal(aOriginProps.mSpec)) {
      if (!mPermanentStorageDir) {
        mPermanentStorageDir =
          do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        QuotaManager* quotaManager = QuotaManager::Get();
        MOZ_ASSERT(quotaManager);

        const nsString& permanentStoragePath =
          quotaManager->GetStoragePath(PERSISTENCE_TYPE_PERSISTENT);

        rv = mPermanentStorageDir->InitWithPath(permanentStoragePath);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }

      nsString leafName;
      rv = aOriginProps.mDirectory->GetLeafName(leafName);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      nsCOMPtr<nsIFile> newDirectory;
      rv = mPermanentStorageDir->Clone(getter_AddRefs(newDirectory));
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

        rv = aOriginProps.mDirectory->Remove(/* recursive */ true);
      } else {
        rv = aOriginProps.mDirectory->MoveTo(mPermanentStorageDir,
                                             EmptyString());
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  } else if (aOriginProps.mNeedsRestore) {
    rv = CreateDirectoryMetadata(aOriginProps.mDirectory,
                                 aOriginProps.mTimestamp,
                                 aOriginProps.mSuffix,
                                 aOriginProps.mGroup,
                                 aOriginProps.mOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else if (!aOriginProps.mIgnore) {
    nsCOMPtr<nsIFile> file;
    rv = aOriginProps.mDirectory->Clone(getter_AddRefs(file));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = file->Append(NS_LITERAL_STRING(METADATA_FILE_NAME));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsCOMPtr<nsIBinaryOutputStream> stream;
    rv = GetBinaryOutputStream(file, kAppendFileFlag, getter_AddRefs(stream));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ASSERT(stream);

    // Currently unused (used to be isApp).
    rv = stream->WriteBoolean(false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult
UpgradeStorageFrom0_0To1_0Helper::DoUpgrade()
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

      // Unknown files during upgrade are allowed. Just warn if we find them.
      if (!IsOSMetadata(leafName)) {
        UNKNOWN_FILE_WARNING(leafName);
      }
      continue;
    }

    OriginProps originProps;
    rv = originProps.Init(originDir);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    int64_t timestamp;
    nsCString group;
    nsCString origin;
    Nullable<bool> isApp;
    nsresult rv = GetDirectoryMetadata(originDir,
                                       timestamp,
                                       group,
                                       origin,
                                       isApp);
    if (NS_FAILED(rv) || isApp.IsNull()) {
      originProps.mTimestamp = GetLastModifiedTime(originDir, mPersistent);
      originProps.mNeedsRestore = true;
    } else {
      originProps.mTimestamp = timestamp;
    }

    mOriginProps.AppendElement(Move(originProps));
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
UpgradeStorageFrom0_0To1_0Helper::ProcessOriginDirectory(
                                                const OriginProps& aOriginProps)
{
  AssertIsOnIOThread();

  nsresult rv;

  if (aOriginProps.mNeedsRestore) {
    rv = CreateDirectoryMetadata(aOriginProps.mDirectory,
                                 aOriginProps.mTimestamp,
                                 aOriginProps.mSuffix,
                                 aOriginProps.mGroup,
                                 aOriginProps.mOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = CreateDirectoryMetadata2(aOriginProps.mDirectory,
                                aOriginProps.mTimestamp,
                                /* aPersisted */ false,
                                aOriginProps.mSuffix,
                                aOriginProps.mGroup,
                                aOriginProps.mOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsString oldName;
  rv = aOriginProps.mDirectory->GetLeafName(oldName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString originSanitized(aOriginProps.mOrigin);
  SanitizeOriginString(originSanitized);

  NS_ConvertASCIItoUTF16 newName(originSanitized);

  if (!oldName.Equals(newName)) {
    rv = aOriginProps.mDirectory->RenameTo(nullptr, newName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult
UpgradeStorageFrom1_0To2_0Helper::DoUpgrade()
{
  AssertIsOnIOThread();

  DebugOnly<bool> exists;
  MOZ_ASSERT(NS_SUCCEEDED(mDirectory->Exists(&exists)));
  MOZ_ASSERT(exists);

  nsCOMPtr<nsISimpleEnumerator> entries;
  nsresult rv = mDirectory->GetDirectoryEntries(getter_AddRefs(entries));
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

      // Unknown files during upgrade are allowed. Just warn if we find them.
      if (!IsOSMetadata(leafName)) {
        UNKNOWN_FILE_WARNING(leafName);
      }
      continue;
    }

    OriginProps originProps;
    rv = originProps.Init(originDir);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = MaybeUpgradeClients(originProps);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    bool removed;
    rv = MaybeRemoveAppsData(originProps, &removed);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (removed) {
      continue;
    }

    int64_t timestamp;
    nsCString group;
    nsCString origin;
    Nullable<bool> isApp;
    nsresult rv = GetDirectoryMetadata(originDir,
                                       timestamp,
                                       group,
                                       origin,
                                       isApp);
    if (NS_FAILED(rv) || isApp.IsNull()) {
      originProps.mNeedsRestore = true;
    }

    nsCString suffix;
    rv = GetDirectoryMetadata2(originDir,
                               timestamp,
                               suffix,
                               group,
                               origin,
                               isApp.SetValue());
    if (NS_FAILED(rv)) {
      originProps.mTimestamp = GetLastModifiedTime(originDir, mPersistent);
      originProps.mNeedsRestore2 = true;
    } else {
      originProps.mTimestamp = timestamp;
    }

    mOriginProps.AppendElement(Move(originProps));
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
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
UpgradeStorageFrom1_0To2_0Helper::MaybeUpgradeClients(
                                                const OriginProps& aOriginProps)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aOriginProps.mDirectory);

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  nsCOMPtr<nsISimpleEnumerator> entries;
  nsresult rv =
    aOriginProps.mDirectory->GetDirectoryEntries(getter_AddRefs(entries));
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

    bool isDirectory;
    rv = file->IsDirectory(&isDirectory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsString leafName;
    rv = file->GetLeafName(leafName);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!isDirectory) {
      // Unknown files during upgrade are allowed. Just warn if we find them.
      if (!IsOriginMetadata(leafName) &&
          !IsTempMetadata(leafName)) {
        UNKNOWN_FILE_WARNING(leafName);
      }
      continue;
    }

    // The Cache API was creating top level morgue directories by accident for
    // a short time in nightly.  This unfortunately prevents all storage from
    // working.  So recover these profiles permanently by removing these corrupt
    // directories as part of this upgrade.
    if (leafName.EqualsLiteral("morgue")) {
      QM_WARNING("Deleting accidental morgue directory!");

      rv = file->Remove(/* recursive */ true);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      continue;
    }

    Client::Type clientType;
    rv = Client::TypeFromText(leafName, clientType);
    if (NS_FAILED(rv)) {
      UNKNOWN_FILE_WARNING(leafName);
      continue;
    }

    Client* client = quotaManager->GetClient(clientType);
    MOZ_ASSERT(client);

    rv = client->UpgradeStorageFrom1_0To2_0(file);
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
UpgradeStorageFrom1_0To2_0Helper::MaybeRemoveAppsData(
                                                const OriginProps& aOriginProps,
                                                bool* aRemoved)
{
  AssertIsOnIOThread();

  // XXX This will need to be reworked as part of bug 1320404 (appId is
  //     going to be removed from origin attributes).
  if (aOriginProps.mAttrs.mAppId != kNoAppId &&
      aOriginProps.mAttrs.mAppId != kUnknownAppId) {
    nsresult rv = RemoveObsoleteOrigin(aOriginProps);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    *aRemoved = true;
    return NS_OK;
  }

  *aRemoved = false;
  return NS_OK;
}

nsresult
UpgradeStorageFrom1_0To2_0Helper::MaybeStripObsoleteOriginAttributes(
                                                const OriginProps& aOriginProps,
                                                bool* aStripped)
{
  AssertIsOnIOThread();
  MOZ_ASSERT(aOriginProps.mDirectory);

  const nsAString& oldLeafName = aOriginProps.mLeafName;

  nsCString originSanitized(aOriginProps.mOrigin);
  SanitizeOriginString(originSanitized);

  NS_ConvertUTF8toUTF16 newLeafName(originSanitized);

  if (oldLeafName == newLeafName) {
    *aStripped = false;
    return NS_OK;
  }

  nsresult rv = CreateDirectoryMetadata(aOriginProps.mDirectory,
                                        aOriginProps.mTimestamp,
                                        aOriginProps.mSuffix,
                                        aOriginProps.mGroup,
                                        aOriginProps.mOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = CreateDirectoryMetadata2(aOriginProps.mDirectory,
                                aOriginProps.mTimestamp,
                                /* aPersisted */ false,
                                aOriginProps.mSuffix,
                                aOriginProps.mGroup,
                                aOriginProps.mOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIFile> newFile;
  rv = aOriginProps.mDirectory->GetParent(getter_AddRefs(newFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = newFile->Append(newLeafName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool exists;
  rv = newFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (exists) {
    QM_WARNING("Can't rename %s directory, %s directory already exists, "
               "removing!",
               NS_ConvertUTF16toUTF8(oldLeafName).get(),
               NS_ConvertUTF16toUTF8(newLeafName).get());

    rv = aOriginProps.mDirectory->Remove(/* recursive */ true);
  } else {
    rv = aOriginProps.mDirectory->RenameTo(nullptr, newLeafName);
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aStripped = true;
  return NS_OK;
}

nsresult
UpgradeStorageFrom1_0To2_0Helper::ProcessOriginDirectory(
                                                const OriginProps& aOriginProps)
{
  AssertIsOnIOThread();

  bool stripped;
  nsresult rv = MaybeStripObsoleteOriginAttributes(aOriginProps, &stripped);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (stripped) {
    return NS_OK;
  }

  if (aOriginProps.mNeedsRestore) {
    rv = CreateDirectoryMetadata(aOriginProps.mDirectory,
                                 aOriginProps.mTimestamp,
                                 aOriginProps.mSuffix,
                                 aOriginProps.mGroup,
                                 aOriginProps.mOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (aOriginProps.mNeedsRestore2) {
    rv = CreateDirectoryMetadata2(aOriginProps.mDirectory,
                                  aOriginProps.mTimestamp,
                                  /* aPersisted */ false,
                                  aOriginProps.mSuffix,
                                  aOriginProps.mGroup,
                                  aOriginProps.mOrigin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult
RestoreDirectoryMetadata2Helper::RestoreMetadata2File()
{
  AssertIsOnIOThread();

  nsresult rv;

  OriginProps originProps;
  rv = originProps.Init(mDirectory);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  originProps.mTimestamp = GetLastModifiedTime(mDirectory, mPersistent);

  mOriginProps.AppendElement(Move(originProps));

  rv = ProcessOriginDirectories();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
RestoreDirectoryMetadata2Helper::ProcessOriginDirectory(
                                                const OriginProps& aOriginProps)
{
  AssertIsOnIOThread();

  // We don't have any approach to restore aPersisted, so reset it to false.
  nsresult rv = CreateDirectoryMetadata2(aOriginProps.mDirectory,
                                         aOriginProps.mTimestamp,
                                         /* aPersisted */ false,
                                         aOriginProps.mSuffix,
                                         aOriginProps.mGroup,
                                         aOriginProps.mOrigin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

} // namespace quota
} // namespace dom
} // namespace mozilla
