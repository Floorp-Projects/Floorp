/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StorageCache.h"

#include "Storage.h"
#include "StorageDBThread.h"
#include "StorageIPC.h"
#include "StorageManager.h"

#include "nsAutoPtr.h"
#include "nsDOMString.h"
#include "nsXULAppAPI.h"
#include "mozilla/Unused.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {

#define DOM_STORAGE_CACHE_KEEP_ALIVE_TIME_MS 20000

// static
StorageDBBridge* StorageCache::sDatabase = nullptr;
bool StorageCache::sDatabaseDown = false;

namespace {

const uint32_t kDefaultSet = 0;
const uint32_t kPrivateSet = 1;
const uint32_t kSessionSet = 2;

inline uint32_t
GetDataSetIndex(bool aPrivate, bool aSessionOnly)
{
  if (aPrivate) {
    return kPrivateSet;
  }

  if (aSessionOnly) {
    return kSessionSet;
  }

  return kDefaultSet;
}

inline uint32_t
GetDataSetIndex(const Storage* aStorage)
{
  return GetDataSetIndex(aStorage->IsPrivate(), aStorage->IsSessionOnly());
}

} // namespace

// StorageCacheBridge

NS_IMPL_ADDREF(StorageCacheBridge)

// Since there is no consumer of return value of Release, we can turn this 
// method to void to make implementation of asynchronous StorageCache::Release
// much simpler.
NS_IMETHODIMP_(void) StorageCacheBridge::Release(void)
{
  MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");
  nsrefcnt count = --mRefCnt;
  NS_LOG_RELEASE(this, count, "StorageCacheBridge");
  if (0 == count) {
    mRefCnt = 1; /* stabilize */
    /* enable this to find non-threadsafe destructors: */
    /* NS_ASSERT_OWNINGTHREAD(_class); */
    delete (this);
  }
}

// StorageCache

StorageCache::StorageCache(const nsACString* aOriginNoSuffix)
  : mOriginNoSuffix(*aOriginNoSuffix)
  , mMonitor("StorageCache")
  , mLoaded(false)
  , mLoadResult(NS_OK)
  , mInitialized(false)
  , mPersistent(false)
  , mSessionOnlyDataSetActive(false)
  , mPreloadTelemetryRecorded(false)
{
  MOZ_COUNT_CTOR(StorageCache);
}

StorageCache::~StorageCache()
{
  if (mManager) {
    mManager->DropCache(this);
  }

  MOZ_COUNT_DTOR(StorageCache);
}

NS_IMETHODIMP_(void)
StorageCache::Release(void)
{
  // We must actually release on the main thread since the cache removes it
  // self from the manager's hash table.  And we don't want to lock access to
  // that hash table.
  if (NS_IsMainThread()) {
    StorageCacheBridge::Release();
    return;
  }

  RefPtr<nsRunnableMethod<StorageCacheBridge, void, false> > event =
    NewNonOwningRunnableMethod(static_cast<StorageCacheBridge*>(this),
                               &StorageCacheBridge::Release);

  nsresult rv = NS_DispatchToMainThread(event);
  if (NS_FAILED(rv)) {
    NS_WARNING("StorageCache::Release() on a non-main thread");
    StorageCacheBridge::Release();
  }
}

void
StorageCache::Init(StorageManagerBase* aManager,
                   bool aPersistent,
                   nsIPrincipal* aPrincipal,
                   const nsACString& aQuotaOriginScope)
{
  if (mInitialized) {
    return;
  }

  mInitialized = true;
  mPrincipal = aPrincipal;
  aPrincipal->OriginAttributesRef().CreateSuffix(mOriginSuffix);
  mPersistent = aPersistent;
  if (aQuotaOriginScope.IsEmpty()) {
    mQuotaOriginScope = Origin();
  } else {
    mQuotaOriginScope = aQuotaOriginScope;
  }

  if (mPersistent) {
    mManager = aManager;
    Preload();
  }

  // Check the quota string has (or has not) the identical origin suffix as
  // this storage cache is bound to.
  MOZ_ASSERT(StringBeginsWith(mQuotaOriginScope, mOriginSuffix));
  MOZ_ASSERT(mOriginSuffix.IsEmpty() != StringBeginsWith(mQuotaOriginScope, 
                                                         NS_LITERAL_CSTRING("^")));

  mUsage = aManager->GetOriginUsage(mQuotaOriginScope);
}

inline bool
StorageCache::Persist(const Storage* aStorage) const
{
  return mPersistent &&
         !aStorage->IsSessionOnly() &&
         !aStorage->IsPrivate();
}

const nsCString
StorageCache::Origin() const
{
  return StorageManagerBase::CreateOrigin(mOriginSuffix, mOriginNoSuffix);
}

StorageCache::Data&
StorageCache::DataSet(const Storage* aStorage)
{
  uint32_t index = GetDataSetIndex(aStorage);

  if (index == kSessionSet && !mSessionOnlyDataSetActive) {
    // Session only data set is demanded but not filled with
    // current data set, copy to session only set now.

    WaitForPreload(Telemetry::LOCALDOMSTORAGE_SESSIONONLY_PRELOAD_BLOCKING_MS);

    Data& defaultSet = mData[kDefaultSet];
    Data& sessionSet = mData[kSessionSet];

    for (auto iter = defaultSet.mKeys.Iter(); !iter.Done(); iter.Next()) {
      sessionSet.mKeys.Put(iter.Key(), iter.UserData());
    }

    mSessionOnlyDataSetActive = true;

    // This updates sessionSet.mOriginQuotaUsage and also updates global usage
    // for all session only data
    ProcessUsageDelta(kSessionSet, defaultSet.mOriginQuotaUsage);
  }

  return mData[index];
}

bool
StorageCache::ProcessUsageDelta(const Storage* aStorage, int64_t aDelta)
{
  return ProcessUsageDelta(GetDataSetIndex(aStorage), aDelta);
}

bool
StorageCache::ProcessUsageDelta(uint32_t aGetDataSetIndex, const int64_t aDelta)
{
  // Check if we are in a low disk space situation
  if (aDelta > 0 && mManager && mManager->IsLowDiskSpace()) {
    return false;
  }

  // Check limit per this origin
  Data& data = mData[aGetDataSetIndex];
  uint64_t newOriginUsage = data.mOriginQuotaUsage + aDelta;
  if (aDelta > 0 && newOriginUsage > StorageManagerBase::GetQuota()) {
    return false;
  }

  // Now check eTLD+1 limit
  if (mUsage && !mUsage->CheckAndSetETLD1UsageDelta(aGetDataSetIndex, aDelta)) {
    return false;
  }

  // Update size in our data set
  data.mOriginQuotaUsage = newOriginUsage;
  return true;
}

void
StorageCache::Preload()
{
  if (mLoaded || !mPersistent) {
    return;
  }

  if (!StartDatabase()) {
    mLoaded = true;
    mLoadResult = NS_ERROR_FAILURE;
    return;
  }

  sDatabase->AsyncPreload(this);
}

namespace {

// This class is passed to timer as a tick observer.  It refers the cache
// and keeps it alive for a time.
class StorageCacheHolder : public nsITimerCallback
{
  virtual ~StorageCacheHolder() {}

  NS_DECL_ISUPPORTS

  NS_IMETHOD
  Notify(nsITimer* aTimer) override
  {
    mCache = nullptr;
    return NS_OK;
  }

  RefPtr<StorageCache> mCache;

public:
  explicit StorageCacheHolder(StorageCache* aCache) : mCache(aCache) {}
};

NS_IMPL_ISUPPORTS(StorageCacheHolder, nsITimerCallback)

} // namespace

void
StorageCache::KeepAlive()
{
  // Missing reference back to the manager means the cache is not responsible
  // for its lifetime.  Used for keeping sessionStorage live forever.
  if (!mManager) {
    return;
  }

  if (!NS_IsMainThread()) {
    // Timer and the holder must be initialized on the main thread.
    NS_DispatchToMainThread(NewRunnableMethod(this, &StorageCache::KeepAlive));
    return;
  }

  nsCOMPtr<nsITimer> timer = do_CreateInstance("@mozilla.org/timer;1");
  if (!timer) {
    return;
  }

  RefPtr<StorageCacheHolder> holder = new StorageCacheHolder(this);
  timer->InitWithCallback(holder, DOM_STORAGE_CACHE_KEEP_ALIVE_TIME_MS,
                          nsITimer::TYPE_ONE_SHOT);

  mKeepAliveTimer.swap(timer);
}

namespace {

// The AutoTimer provided by telemetry headers is only using static,
// i.e. compile time known ID, but here we know the ID only at run time.
// Hence a new class.
class TelemetryAutoTimer
{
public:
  explicit TelemetryAutoTimer(Telemetry::ID aId)
    : id(aId), start(TimeStamp::Now())
  {}

  ~TelemetryAutoTimer()
  {
    Telemetry::AccumulateDelta_impl<Telemetry::Millisecond>::compute(id, start);
  }

private:
  Telemetry::ID id;
  const TimeStamp start;
};

} // namespace

void
StorageCache::WaitForPreload(Telemetry::ID aTelemetryID)
{
  if (!mPersistent) {
    return;
  }

  bool loaded = mLoaded;

  // Telemetry of rates of pending preloads
  if (!mPreloadTelemetryRecorded) {
    mPreloadTelemetryRecorded = true;
    Telemetry::Accumulate(
      Telemetry::LOCALDOMSTORAGE_PRELOAD_PENDING_ON_FIRST_ACCESS,
      !loaded);
  }

  if (loaded) {
    return;
  }

  // Measure which operation blocks and for how long
  TelemetryAutoTimer timer(aTelemetryID);

  // If preload already started (i.e. we got some first data, but not all)
  // SyncPreload will just wait for it to finish rather then synchronously
  // read from the database.  It seems to me more optimal.

  // TODO place for A/B testing (force main thread load vs. let preload finish)

  // No need to check sDatabase for being non-null since preload is either
  // done before we've shut the DB down or when the DB could not start,
  // preload has not even be started.
  sDatabase->SyncPreload(this);
}

nsresult
StorageCache::GetLength(const Storage* aStorage, uint32_t* aRetval)
{
  if (Persist(aStorage)) {
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_GETLENGTH_BLOCKING_MS);
    if (NS_FAILED(mLoadResult)) {
      return mLoadResult;
    }
  }

  *aRetval = DataSet(aStorage).mKeys.Count();
  return NS_OK;
}

nsresult
StorageCache::GetKey(const Storage* aStorage, uint32_t aIndex,
                     nsAString& aRetval)
{
  // XXX: This does a linear search for the key at index, which would
  // suck if there's a large numer of indexes. Do we care? If so,
  // maybe we need to have a lazily populated key array here or
  // something?
  if (Persist(aStorage)) {
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_GETKEY_BLOCKING_MS);
    if (NS_FAILED(mLoadResult)) {
      return mLoadResult;
    }
  }

  aRetval.SetIsVoid(true);
  for (auto iter = DataSet(aStorage).mKeys.Iter(); !iter.Done(); iter.Next()) {
    if (aIndex == 0) {
      aRetval = iter.Key();
      break;
    }
    aIndex--;
  }

  return NS_OK;
}

void
StorageCache::GetKeys(const Storage* aStorage, nsTArray<nsString>& aKeys)
{
  if (Persist(aStorage)) {
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_GETALLKEYS_BLOCKING_MS);
  }

  if (NS_FAILED(mLoadResult)) {
    return;
  }

  for (auto iter = DataSet(aStorage).mKeys.Iter(); !iter.Done(); iter.Next()) {
    aKeys.AppendElement(iter.Key());
  }
}

nsresult
StorageCache::GetItem(const Storage* aStorage, const nsAString& aKey,
                      nsAString& aRetval)
{
  if (Persist(aStorage)) {
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_GETVALUE_BLOCKING_MS);
    if (NS_FAILED(mLoadResult)) {
      return mLoadResult;
    }
  }

  // not using AutoString since we don't want to copy buffer to result
  nsString value;
  if (!DataSet(aStorage).mKeys.Get(aKey, &value)) {
    SetDOMStringToNull(value);
  }

  aRetval = value;

  return NS_OK;
}

nsresult
StorageCache::SetItem(const Storage* aStorage, const nsAString& aKey,
                      const nsString& aValue, nsString& aOld)
{
  // Size of the cache that will change after this action.
  int64_t delta = 0;

  if (Persist(aStorage)) {
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_SETVALUE_BLOCKING_MS);
    if (NS_FAILED(mLoadResult)) {
      return mLoadResult;
    }
  }

  Data& data = DataSet(aStorage);
  if (!data.mKeys.Get(aKey, &aOld)) {
    SetDOMStringToNull(aOld);

    // We only consider key size if the key doesn't exist before.
    delta += static_cast<int64_t>(aKey.Length());
  }

  delta += static_cast<int64_t>(aValue.Length()) -
           static_cast<int64_t>(aOld.Length());

  if (!ProcessUsageDelta(aStorage, delta)) {
    return NS_ERROR_DOM_QUOTA_REACHED;
  }

  if (aValue == aOld && DOMStringIsNull(aValue) == DOMStringIsNull(aOld)) {
    return NS_SUCCESS_DOM_NO_OPERATION;
  }

  data.mKeys.Put(aKey, aValue);

  if (Persist(aStorage)) {
    if (!sDatabase) {
      NS_ERROR("Writing to localStorage after the database has been shut down"
               ", data lose!");
      return NS_ERROR_NOT_INITIALIZED;
    }

    if (DOMStringIsNull(aOld)) {
      return sDatabase->AsyncAddItem(this, aKey, aValue);
    }

    return sDatabase->AsyncUpdateItem(this, aKey, aValue);
  }

  return NS_OK;
}

nsresult
StorageCache::RemoveItem(const Storage* aStorage, const nsAString& aKey,
                         nsString& aOld)
{
  if (Persist(aStorage)) {
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_REMOVEKEY_BLOCKING_MS);
    if (NS_FAILED(mLoadResult)) {
      return mLoadResult;
    }
  }

  Data& data = DataSet(aStorage);
  if (!data.mKeys.Get(aKey, &aOld)) {
    SetDOMStringToNull(aOld);
    return NS_SUCCESS_DOM_NO_OPERATION;
  }

  // Recalculate the cached data size
  const int64_t delta = -(static_cast<int64_t>(aOld.Length()) +
                          static_cast<int64_t>(aKey.Length()));
  Unused << ProcessUsageDelta(aStorage, delta);
  data.mKeys.Remove(aKey);

  if (Persist(aStorage)) {
    if (!sDatabase) {
      NS_ERROR("Writing to localStorage after the database has been shut down"
               ", data lose!");
      return NS_ERROR_NOT_INITIALIZED;
    }

    return sDatabase->AsyncRemoveItem(this, aKey);
  }

  return NS_OK;
}

nsresult
StorageCache::Clear(const Storage* aStorage)
{
  bool refresh = false;
  if (Persist(aStorage)) {
    // We need to preload all data (know the size) before we can proceeed
    // to correctly decrease cached usage number.
    // XXX as in case of unload, this is not technically needed now, but
    // after super-scope quota introduction we have to do this.  Get telemetry
    // right now.
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_CLEAR_BLOCKING_MS);
    if (NS_FAILED(mLoadResult)) {
      // When we failed to load data from the database, force delete of the
      // scope data and make use of the storage possible again.
      refresh = true;
      mLoadResult = NS_OK;
    }
  }

  Data& data = DataSet(aStorage);
  bool hadData = !!data.mKeys.Count();

  if (hadData) {
    Unused << ProcessUsageDelta(aStorage, -data.mOriginQuotaUsage);
    data.mKeys.Clear();
  }

  if (Persist(aStorage) && (refresh || hadData)) {
    if (!sDatabase) {
      NS_ERROR("Writing to localStorage after the database has been shut down"
               ", data lose!");
      return NS_ERROR_NOT_INITIALIZED;
    }

    return sDatabase->AsyncClear(this);
  }

  return hadData ? NS_OK : NS_SUCCESS_DOM_NO_OPERATION;
}

void
StorageCache::CloneFrom(const StorageCache* aThat)
{
  // This will never be called on anything else than SessionStorage.
  // This means mData will never be touched on any other thread than
  // the main thread and it never went through the loading process.
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mPersistent);
  MOZ_ASSERT(!(bool)aThat->mLoaded);

  mLoaded = false;
  mInitialized = aThat->mInitialized;
  mPersistent = false;
  mSessionOnlyDataSetActive = aThat->mSessionOnlyDataSetActive;

  for (uint32_t i = 0; i < kDataSetCount; ++i) {
    for (auto it = aThat->mData[i].mKeys.ConstIter(); !it.Done(); it.Next()) {
      mData[i].mKeys.Put(it.Key(), it.UserData());
    }
    ProcessUsageDelta(i, aThat->mData[i].mOriginQuotaUsage);
  }
}

// Defined in StorageManager.cpp
extern bool
PrincipalsEqual(nsIPrincipal* aObjectPrincipal,
                nsIPrincipal* aSubjectPrincipal);

bool
StorageCache::CheckPrincipal(nsIPrincipal* aPrincipal) const
{
  return PrincipalsEqual(mPrincipal, aPrincipal);
}

void
StorageCache::UnloadItems(uint32_t aUnloadFlags)
{
  if (aUnloadFlags & kUnloadDefault) {
    // Must wait for preload to pass correct usage to ProcessUsageDelta
    // XXX this is not technically needed right now since there is just
    // per-origin isolated quota handling, but when we introduce super-
    // -scope quotas, we have to do this.  Better to start getting
    // telemetry right now.
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_UNLOAD_BLOCKING_MS);

    mData[kDefaultSet].mKeys.Clear();
    ProcessUsageDelta(kDefaultSet, -mData[kDefaultSet].mOriginQuotaUsage);
  }

  if (aUnloadFlags & kUnloadPrivate) {
    mData[kPrivateSet].mKeys.Clear();
    ProcessUsageDelta(kPrivateSet, -mData[kPrivateSet].mOriginQuotaUsage);
  }

  if (aUnloadFlags & kUnloadSession) {
    mData[kSessionSet].mKeys.Clear();
    ProcessUsageDelta(kSessionSet, -mData[kSessionSet].mOriginQuotaUsage);
    mSessionOnlyDataSetActive = false;
  }

#ifdef DOM_STORAGE_TESTS
  if (aUnloadFlags & kTestReload) {
    WaitForPreload(Telemetry::LOCALDOMSTORAGE_UNLOAD_BLOCKING_MS);

    mData[kDefaultSet].mKeys.Clear();
    mLoaded = false; // This is only used in testing code
    Preload();
  }
#endif
}

// StorageCacheBridge

uint32_t
StorageCache::LoadedCount()
{
  MonitorAutoLock monitor(mMonitor);
  Data& data = mData[kDefaultSet];
  return data.mKeys.Count();
}

bool
StorageCache::LoadItem(const nsAString& aKey, const nsString& aValue)
{
  MonitorAutoLock monitor(mMonitor);
  if (mLoaded) {
    return false;
  }

  Data& data = mData[kDefaultSet];
  if (data.mKeys.Get(aKey, nullptr)) {
    return true; // don't stop, just don't override
  }

  data.mKeys.Put(aKey, aValue);
  data.mOriginQuotaUsage += aKey.Length() + aValue.Length();
  return true;
}

void
StorageCache::LoadDone(nsresult aRv)
{
  // Keep the preloaded cache alive for a time
  KeepAlive();

  MonitorAutoLock monitor(mMonitor);
  mLoadResult = aRv;
  mLoaded = true;
  monitor.Notify();
}

void
StorageCache::LoadWait()
{
  MonitorAutoLock monitor(mMonitor);
  while (!mLoaded) {
    monitor.Wait();
  }
}

// StorageUsage

StorageUsage::StorageUsage(const nsACString& aOriginScope)
  : mOriginScope(aOriginScope)
{
  mUsage[kDefaultSet] = mUsage[kPrivateSet] = mUsage[kSessionSet] = 0LL;
}

namespace {

class LoadUsageRunnable : public Runnable
{
public:
  LoadUsageRunnable(int64_t* aUsage, const int64_t aDelta)
    : mTarget(aUsage)
    , mDelta(aDelta)
  {}

private:
  int64_t* mTarget;
  int64_t mDelta;

  NS_IMETHOD Run() override { *mTarget = mDelta; return NS_OK; }
};

} // namespace

void
StorageUsage::LoadUsage(const int64_t aUsage)
{
  // Using kDefaultSet index since it is the index for the persitent data
  // stored in the database we have just loaded usage for.
  if (!NS_IsMainThread()) {
    // In single process scenario we get this call from the DB thread
    RefPtr<LoadUsageRunnable> r =
      new LoadUsageRunnable(mUsage + kDefaultSet, aUsage);
    NS_DispatchToMainThread(r);
  } else {
    // On a child process we get this on the main thread already
    mUsage[kDefaultSet] += aUsage;
  }
}

bool
StorageUsage::CheckAndSetETLD1UsageDelta(uint32_t aDataSetIndex,
                                         const int64_t aDelta)
{
  MOZ_ASSERT(NS_IsMainThread());

  int64_t newUsage = mUsage[aDataSetIndex] + aDelta;
  if (aDelta > 0 && newUsage > StorageManagerBase::GetQuota()) {
    return false;
  }

  mUsage[aDataSetIndex] = newUsage;
  return true;
}


// static
StorageDBBridge*
StorageCache::StartDatabase()
{
  if (sDatabase || sDatabaseDown) {
    // When sDatabaseDown is at true, sDatabase is null.
    // Checking sDatabaseDown flag here prevents reinitialization of
    // the database after shutdown.
    return sDatabase;
  }

  if (XRE_IsParentProcess()) {
    nsAutoPtr<StorageDBThread> db(new StorageDBThread());

    nsresult rv = db->Init();
    if (NS_FAILED(rv)) {
      return nullptr;
    }

    sDatabase = db.forget();
  } else {
    // Use DOMLocalStorageManager::Ensure in case we're called from
    // DOMSessionStorageManager's initializer and we haven't yet initialized the
    // local storage manager.
    RefPtr<StorageDBChild> db = new StorageDBChild(
        DOMLocalStorageManager::Ensure());

    nsresult rv = db->Init();
    if (NS_FAILED(rv)) {
      return nullptr;
    }

    db.forget(&sDatabase);
  }

  return sDatabase;
}

// static
StorageDBBridge*
StorageCache::GetDatabase()
{
  return sDatabase;
}

// static
nsresult
StorageCache::StopDatabase()
{
  if (!sDatabase) {
    return NS_OK;
  }

  sDatabaseDown = true;

  nsresult rv = sDatabase->Shutdown();
  if (XRE_IsParentProcess()) {
    delete sDatabase;
  } else {
    StorageDBChild* child = static_cast<StorageDBChild*>(sDatabase);
    NS_RELEASE(child);
  }

  sDatabase = nullptr;
  return rv;
}

} // namespace dom
} // namespace mozilla
