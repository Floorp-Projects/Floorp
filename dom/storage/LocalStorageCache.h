/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_LocalStorageCache_h
#define mozilla_dom_LocalStorageCache_h

#include "nsIPrincipal.h"
#include "nsITimer.h"

#include "nsString.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "mozilla/Monitor.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Atomics.h"

namespace mozilla {
namespace dom {

class LocalStorage;
class LocalStorageManager;
class StorageUsage;
class StorageDBBridge;

// Interface class on which only the database or IPC may call.
// Used to populate the cache with DB data.
class LocalStorageCacheBridge
{
public:
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void);
  NS_IMETHOD_(void) Release(void);

  // The origin of the cache, result is concatenation of OriginNoSuffix() and
  // OriginSuffix(), see below.
  virtual const nsCString Origin() const = 0;

  // The origin attributes suffix alone, this is usually passed as an
  // |aOriginSuffix| argument to various methods
  virtual const nsCString& OriginSuffix() const = 0;

  // The origin in the database usage format (reversed) and without the suffix
  virtual const nsCString& OriginNoSuffix() const = 0;

  // Whether the cache is already fully loaded
  virtual bool Loaded() = 0;

  // How many items has so far been loaded into the cache, used
  // for optimization purposes
  virtual uint32_t LoadedCount() = 0;

  // Called by the database to load a key and its value to the cache
  virtual bool LoadItem(const nsAString& aKey, const nsString& aValue) = 0;

  // Called by the database after all keys and values has been loaded
  // to this cache
  virtual void LoadDone(nsresult aRv) = 0;

  // Use to synchronously wait until the cache gets fully loaded with data,
  // this method exits after LoadDone has been called
  virtual void LoadWait() = 0;

protected:
  virtual ~LocalStorageCacheBridge() {}

  ThreadSafeAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
};

// Implementation of scope cache that is responsible for preloading data
// for persistent storage (localStorage) and hold data for non-private,
// private and session-only cookie modes.  It is also responsible for
// persisting data changes using the database, works as a write-back cache.
class LocalStorageCache : public LocalStorageCacheBridge
{
public:
  NS_IMETHOD_(void) Release(void) override;

  enum MutationSource {
    // The mutation is a result of an explicit JS mutation in this process.
    // The mutation should be sent to the sDatabase. Quota will be checked and
    // QuotaExceededError may be returned without the mutation being applied.
    ContentMutation,
    // The mutation initially was triggered in a different process and is being
    // propagated to this cache via LocalStorage::ApplyEvent.  The mutation should
    // not be sent to the sDatabase because the originating process is already
    // doing that.  (In addition to the redundant writes being wasteful, there
    // is the potential for other processes to see inconsistent state from the
    // database while preloading.)  Quota will be updated but not checked
    // because it's assumed it was checked in another process and data-coherency
    // is more important than slightly exceeding quota.
    E10sPropagated
  };

  // Note: We pass aOriginNoSuffix through the ctor here, because
  // LocalStorageCacheHashKey's ctor is creating this class and
  // accepts reversed-origin-no-suffix as an argument - the hashing key.
  explicit LocalStorageCache(const nsACString* aOriginNoSuffix);

protected:
  virtual ~LocalStorageCache();

public:
  void Init(LocalStorageManager* aManager, bool aPersistent,
            nsIPrincipal* aPrincipal, const nsACString& aQuotaOriginScope);

  // Get size of per-origin data.
  int64_t GetOriginQuotaUsage(const LocalStorage* aStorage) const;

  // Starts async preload of this cache if it persistent and not loaded.
  void Preload();

  // The set of methods that are invoked by DOM storage web API.
  // We are passing the LocalStorage object just to let the cache
  // read properties like mPrivate and mSessionOnly.
  // Get* methods return error when load from the database has failed.
  nsresult GetLength(const LocalStorage* aStorage, uint32_t* aRetval);
  nsresult GetKey(const LocalStorage* aStorage, uint32_t index, nsAString& aRetval);
  nsresult GetItem(const LocalStorage* aStorage, const nsAString& aKey,
                   nsAString& aRetval);
  nsresult SetItem(const LocalStorage* aStorage, const nsAString& aKey,
                   const nsString& aValue, nsString& aOld,
                   const MutationSource aSource=ContentMutation);
  nsresult RemoveItem(const LocalStorage* aStorage, const nsAString& aKey,
                      nsString& aOld,
                      const MutationSource aSource=ContentMutation);
  nsresult Clear(const LocalStorage* aStorage,
                 const MutationSource aSource=ContentMutation);

  void GetKeys(const LocalStorage* aStorage, nsTArray<nsString>& aKeys);

  // LocalStorageCacheBridge

  const nsCString Origin() const override;
  const nsCString& OriginNoSuffix() const override { return mOriginNoSuffix; }
  const nsCString& OriginSuffix() const override { return mOriginSuffix; }
  bool Loaded() override { return mLoaded; }
  uint32_t LoadedCount() override;
  bool LoadItem(const nsAString& aKey, const nsString& aValue) override;
  void LoadDone(nsresult aRv) override;
  void LoadWait() override;

  // Cache keeps 3 sets of data: regular, private and session-only.
  // This class keeps keys and values for a set and also caches
  // size of the data for quick per-origin quota checking.
  class Data
  {
  public:
    Data() : mOriginQuotaUsage(0) {}
    int64_t mOriginQuotaUsage;
    nsDataHashtable<nsStringHashKey, nsString> mKeys;
  };

public:
  // Number of data sets we keep: default, private, session
  static const uint32_t kDataSetCount = 3;

private:
  // API to clear the cache data, this is invoked by chrome operations
  // like cookie deletion.
  friend class LocalStorageManager;

  static const uint32_t kUnloadDefault = 1 << 0;
  static const uint32_t kUnloadPrivate = 1 << 1;
  static const uint32_t kUnloadSession = 1 << 2;
  static const uint32_t kUnloadComplete =
    kUnloadDefault | kUnloadPrivate | kUnloadSession;

#ifdef DOM_STORAGE_TESTS
  static const uint32_t kTestReload    = 1 << 15;
#endif

  void UnloadItems(uint32_t aUnloadFlags);

private:
  // Synchronously blocks until the cache is fully loaded from the database
  void WaitForPreload(mozilla::Telemetry::HistogramID aTelemetryID);

  // Helper to get one of the 3 data sets (regular, private, session)
  Data& DataSet(const LocalStorage* aStorage);

  // Whether the storage change is about to persist
  bool Persist(const LocalStorage* aStorage) const;

  // Changes the quota usage on the given data set if it fits the quota.
  // If not, then false is returned and no change to the set must be done.
  // A special case is if aSource==E10sPropagated, then we will return true even
  // if the change would put us over quota.  This is done to ensure coherency of
  // caches between processes in the face of races.  It does allow an attacker
  // to potentially use N multiples of the quota storage limit if they can
  // arrange for their origin to execute code in N processes.  However, this is
  // not considered a particularly concerning threat model because it's already
  // very possible for a rogue page to attempt to intentionally fill up the
  // user's storage through the use of multiple domains.
  bool ProcessUsageDelta(uint32_t aGetDataSetIndex, const int64_t aDelta,
                         const MutationSource aSource=ContentMutation);
  bool ProcessUsageDelta(const LocalStorage* aStorage, const int64_t aDelta,
                         const MutationSource aSource=ContentMutation);

private:
  // When a cache is reponsible for its life time (in case of localStorage data
  // cache) we need to refer our manager since removal of the cache from the
  // hash table is handled in the destructor by call to the manager.  Cache
  // could potentially overlive the manager, hence the hard ref.
  RefPtr<LocalStorageManager> mManager;

  // Reference to the usage counter object we check on for eTLD+1 quota limit.
  // Obtained from the manager during initialization (Init method).
  RefPtr<StorageUsage> mUsage;

  // The origin this cache belongs to in the "DB format", i.e. reversed
  nsCString mOriginNoSuffix;

  // The origin attributes suffix
  nsCString mOriginSuffix;

  // The eTLD+1 scope used to count quota usage.  It is in the reversed format
  // and contains the origin attributes suffix.
  nsCString mQuotaOriginScope;

  // Non-private Browsing, Private Browsing and Session Only sets.
  Data mData[kDataSetCount];

  // This monitor is used to wait for full load of data.
  mozilla::Monitor mMonitor;

  // Flag that is initially false.  When the cache is about to work with
  // the database (i.e. it is persistent) this flags is set to true after
  // all keys and coresponding values are loaded from the database.
  // This flag never goes from true back to false.  Since this flag is
  // critical for mData hashtable synchronization, it's made atomic.
  Atomic<bool, ReleaseAcquire> mLoaded;

  // Result of load from the database.  Valid after mLoaded flag has been set.
  nsresult mLoadResult;

  // Init() method has been called
  bool mInitialized : 1;

  // This cache is about to be bound with the database (i.e. it has
  // to load from the DB first and has to persist when modifying the
  // default data set.)
  bool mPersistent : 1;

  // - False when the session-only data set was never used.
  // - True after access to session-only data has been made for the first time.
  // We also fill session-only data set with the default one at that moment.
  // Drops back to false when session-only data are cleared from chrome.
  bool mSessionOnlyDataSetActive : 1;

  // Whether we have already captured state of the cache preload on our first
  // access.
  bool mPreloadTelemetryRecorded : 1;
};

// StorageUsage
// Infrastructure to manage and check eTLD+1 quota
class StorageUsageBridge
{
public:
  NS_INLINE_DECL_THREADSAFE_VIRTUAL_REFCOUNTING(StorageUsageBridge)

  virtual const nsCString& OriginScope() = 0;
  virtual void LoadUsage(const int64_t aUsage) = 0;

protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~StorageUsageBridge() {}
};

class StorageUsage : public StorageUsageBridge
{
public:
  explicit StorageUsage(const nsACString& aOriginScope);

  bool CheckAndSetETLD1UsageDelta(uint32_t aDataSetIndex, int64_t aUsageDelta,
                                  const LocalStorageCache::MutationSource aSource);

private:
  const nsCString& OriginScope() override { return mOriginScope; }
  void LoadUsage(const int64_t aUsage) override;

  nsCString mOriginScope;
  int64_t mUsage[LocalStorageCache::kDataSetCount];
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_LocalStorageCache_h
