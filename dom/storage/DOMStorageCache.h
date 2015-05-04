/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMStorageCache_h___
#define nsDOMStorageCache_h___

#include "nsIPrincipal.h"
#include "nsITimer.h"

#include "nsString.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "mozilla/Monitor.h"
#include "mozilla/Telemetry.h"
#include "nsAutoPtr.h"

namespace mozilla {
namespace dom {

class DOMStorage;
class DOMStorageUsage;
class DOMStorageManager;
class DOMStorageDBBridge;

// Interface class on which only the database or IPC may call.
// Used to populate the cache with DB data.
class DOMStorageCacheBridge
{
public:
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void);
  NS_IMETHOD_(void) Release(void);

  // The scope (origin) in the database usage format (reversed)
  virtual const nsCString& Scope() const = 0;

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
  virtual ~DOMStorageCacheBridge() {}

  ThreadSafeAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
};

// Implementation of scope cache that is responsible for preloading data
// for persistent storage (localStorage) and hold data for non-private,
// private and session-only cookie modes.  It is also responsible for
// persisting data changes using the database, works as a write-back cache.
class DOMStorageCache : public DOMStorageCacheBridge
{
public:
  NS_IMETHOD_(void) Release(void);

  explicit DOMStorageCache(const nsACString* aScope);

protected:
  virtual ~DOMStorageCache();

public:
  void Init(DOMStorageManager* aManager, bool aPersistent, nsIPrincipal* aPrincipal,
            const nsACString& aQuotaScope);

  // Copies all data from the other storage.
  void CloneFrom(const DOMStorageCache* aThat);

  // Starts async preload of this cache if it persistent and not loaded.
  void Preload();

  // Keeps the cache alive (i.e. present in the manager's hash table) for a time.
  void KeepAlive();

  // The set of methods that are invoked by DOM storage web API.
  // We are passing the DOMStorage object just to let the cache
  // read properties like mPrivate and mSessionOnly.
  // Get* methods return error when load from the database has failed.
  nsresult GetLength(const DOMStorage* aStorage, uint32_t* aRetval);
  nsresult GetKey(const DOMStorage* aStorage, uint32_t index, nsAString& aRetval);
  nsresult GetItem(const DOMStorage* aStorage, const nsAString& aKey, nsAString& aRetval);
  nsresult SetItem(const DOMStorage* aStorage, const nsAString& aKey, const nsString& aValue, nsString& aOld);
  nsresult RemoveItem(const DOMStorage* aStorage, const nsAString& aKey, nsString& aOld);
  nsresult Clear(const DOMStorage* aStorage);

  void GetKeys(const DOMStorage* aStorage, nsTArray<nsString>& aKeys);

  // Whether the principal equals principal the cache was created for
  bool CheckPrincipal(nsIPrincipal* aPrincipal) const;
  nsIPrincipal* Principal() const { return mPrincipal; }

  // Starts the database engine thread or the IPC bridge
  static DOMStorageDBBridge* StartDatabase();
  static DOMStorageDBBridge* GetDatabase();

  // Stops the thread and flushes all uncommited data
  static nsresult StopDatabase();

  // DOMStorageCacheBridge

  virtual const nsCString& Scope() const { return mScope; }
  virtual bool Loaded() { return mLoaded; }
  virtual uint32_t LoadedCount();
  virtual bool LoadItem(const nsAString& aKey, const nsString& aValue);
  virtual void LoadDone(nsresult aRv);
  virtual void LoadWait();

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
  friend class DOMStorageManager;

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
  void WaitForPreload(mozilla::Telemetry::ID aTelemetryID);

  // Helper to get one of the 3 data sets (regular, private, session)
  Data& DataSet(const DOMStorage* aStorage);

  // Whether the storage change is about to persist
  bool Persist(const DOMStorage* aStorage) const;

  // Changes the quota usage on the given data set if it fits the quota.
  // If not, then false is returned and no change to the set must be done.
  bool ProcessUsageDelta(uint32_t aGetDataSetIndex, const int64_t aDelta);
  bool ProcessUsageDelta(const DOMStorage* aStorage, const int64_t aDelta);

private:
  // When a cache is reponsible for its life time (in case of localStorage data
  // cache) we need to refer our manager since removal of the cache from the hash
  // table is handled in the destructor by call to the manager.
  // Cache could potentially overlive the manager, hence the hard ref.
  nsRefPtr<DOMStorageManager> mManager;

  // Reference to the usage counter object we check on for eTLD+1 quota limit.
  // Obtained from the manager during initialization (Init method).
  nsRefPtr<DOMStorageUsage> mUsage;

  // Timer that holds this cache alive for a while after it has been preloaded.
  nsCOMPtr<nsITimer> mKeepAliveTimer;

  // Principal the cache has been initially created for, this is used only
  // for sessionStorage access checks since sessionStorage objects are strictly
  // scoped by a principal.  localStorage objects on the other hand are scoped by
  // origin only.
  nsCOMPtr<nsIPrincipal> mPrincipal;

  // The scope this cache belongs to in the "DB format", i.e. reversed
  nsCString mScope;

  // The eTLD+1 scope used to count quota usage.
  nsCString mQuotaScope;

  // Non-private Browsing, Private Browsing and Session Only sets.
  Data mData[kDataSetCount];

  // This monitor is used to wait for full load of data.
  mozilla::Monitor mMonitor;

  // Flag that is initially false.  When the cache is about to work with
  // the database (i.e. it is persistent) this flags is set to true after
  // all keys and coresponding values are loaded from the database.
  // This flag never goes from true back to false.
  bool mLoaded;

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

  // Whether we have already captured state of the cache preload on our first access.
  bool mPreloadTelemetryRecorded : 1;

  // DOMStorageDBThread on the parent or single process,
  // DOMStorageDBChild on the child process.
  static DOMStorageDBBridge* sDatabase;

  // False until we shut the database down.
  static bool sDatabaseDown;
};

// DOMStorageUsage
// Infrastructure to manage and check eTLD+1 quota
class DOMStorageUsageBridge
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DOMStorageUsageBridge)

  virtual const nsCString& Scope() = 0;
  virtual void LoadUsage(const int64_t aUsage) = 0;

protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~DOMStorageUsageBridge() {}
};

class DOMStorageUsage : public DOMStorageUsageBridge
{
public:
  explicit DOMStorageUsage(const nsACString& aScope);

  bool CheckAndSetETLD1UsageDelta(uint32_t aDataSetIndex, int64_t aUsageDelta);

private:
  virtual const nsCString& Scope() { return mScope; }
  virtual void LoadUsage(const int64_t aUsage);

  nsCString mScope;
  int64_t mUsage[DOMStorageCache::kDataSetCount];
};

} // ::dom
} // ::mozilla

#endif
