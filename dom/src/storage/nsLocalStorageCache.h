/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsLocalStorageCache_h___
#define nsLocalStorageCache_h___

#include "nscore.h"
#include "nsClassHashtable.h"
#include "nsTHashtable.h"
#include "nsTArray.h"

class DOMStorageImpl;
class nsSessionStorageEntry;

/**
 * nsScopeCache is the representation of a single scope's contents
 * and its flush state.
 *
 * Typical state progression for a scope:
 *
 * 1. Scope is loaded into memory from disk:
 *    mIsDirty = false;
 *    mIsFlushPending = false;
 *
 * 2. A key is modified/deleted in the scope, or the entire scope is deleted:
 *    mIsDirty = *true*;
 *    mIsFlushPending = false;
 *
 * 3. Flush timer fires and dirty data is collected:
 *    mIsDirty = *false*;
 *    mIsFlushPending = *true*;
 *
 * 4a. If the flush succeds:
 *     mIsDirty = false;
 *     mIsFlushPending = *false*;
 *
 * 4b. If the flush fails for some reason
 *     e.g. profile dir is on network storage and the network is flaky
 *
 *     mIsDirty = *true*;
 *     mIsFlushPending = *false*;
 *
 *     Note that in this case, no data would be lost.
 *     On next flush, the scope would get deleted from the DB to bring
 *     the DB back to a known state and then ALL the cached keys would
 *     get written out.
 *     Failed flushes ought to be very rare.
 */
class nsScopeCache
{
public:
  nsScopeCache();
  nsresult AddEntry(const nsAString& aKey,
                    const nsAString& aValue,
                    bool aSecure);
  nsresult GetAllKeys(DOMStorageImpl* aStorage,
                      nsTHashtable<nsSessionStorageEntry>* aKeys) const;
  bool GetKey(const nsAString& aKey, nsAString& aValue, bool* aSecure) const;
  nsresult SetKey(const nsAString& aKey, const nsAString& aValue, bool aSecure);
  void SetSecure(const nsAString& aKey, bool aSecure);
  void RemoveKey(const nsAString& aKey);
  void DeleteScope();
  int32_t GetQuotaUsage() const;

  /**
   * A single key/value pair in a scope.
   */
  class KeyEntry
  {
  public:
    KeyEntry() : mIsSecure(false), mIsDirty(false) {}
    nsString mValue;
    bool mIsSecure;
    bool mIsDirty;
  };

private:
  friend class nsLocalStorageCache;

  // Scope data
  // -- Whether a scope should be deleted from DB first before any DB inserts
  bool mWasScopeDeleted;
  // -- Keys to be deleted from DB on next flush
  nsTArray<nsString> mDeletedKeys;
  // -- Mapping of scope keys to scope values
  nsClassHashtable<nsStringHashKey, KeyEntry> mTable;

  // Scope state:
  // -- The last time the scope was used, helps decide whether to evict scope
  PRIntervalTime mAccessTime;
  // -- Whether the scope needs to be flushed to disk
  // Note that dirty scopes can't be evicted
  bool mIsDirty;
  // -- Prevents eviction + is used to recover from bad flushes
  bool mIsFlushPending;
};

/**
 * nsLocalStorageCache manages all the cached LocalStorage scopes.
 */
class nsLocalStorageCache
{
public:
  nsLocalStorageCache();

  nsScopeCache* GetScope(const nsACString& aScopeName);
  void AddScope(const nsACString& aScopeName, nsScopeCache* aScopeCache);
  bool IsScopeCached(const nsACString& aScopeName) const;
  uint32_t Count() const;

  int32_t GetQuotaUsage(const nsACString& aQuotaKey) const;
  void MarkMatchingScopesDeleted(const nsACString& aPattern);
  void ForgetAllScopes();

  /**
   * Collection of all the dirty data across all scopes.
   */
  struct FlushData
  {
  public:
    struct ChangeSet
    {
      bool mWasDeleted;
      nsTArray<nsString>* mDeletedKeys;
      nsTArray<nsString> mDirtyKeys;
      nsTArray<nsScopeCache::KeyEntry*> mDirtyValues;
    };
    // A scope's name in mScopeNames has the same index as the
    // scope's ChangeSet in mChanged
    nsTArray<nsCString> mScopeNames;
    nsTArray<ChangeSet> mChanged;
  };

  /**
   * Fills the FlushData structure with the dirty items in the cache.
   */
  void GetFlushData(FlushData* aData) const;

  /**
   * These methods update the state of each cached scope and
   * its keys. See description in nsScopeCache.
   */
  void MarkScopesPending();
  void MarkScopesFlushed();
  void MarkFlushFailed();

  /**
   * Evicts scopes not accessed since aMinAccessTime.
   * Returns info about evicted scopes in aEvicted & aEvictedSize.
   */
  void EvictScopes(nsTArray<nsCString>& aEvicted,
                   nsTArray<int32_t>& aEvictedSize);

private:

  static PLDHashOperator
  GetDirtyDataEnum(const nsACString& aScopeName,
                   nsScopeCache* aScopeCache,
                   void* aParams);

  enum FlushState {
    FLUSH_PENDING,
    FLUSHED,
    FLUSH_FAILED
  };
  static PLDHashOperator
  SetFlushStateEnum(const nsACString& aScopeName,
                    nsAutoPtr<nsScopeCache>& aScopeCache,
                    void* aParams);

  static PLDHashOperator
  EvictEnum(const nsACString& aScopeName,
            nsAutoPtr<nsScopeCache>& aScopeCache,
            void* aParams);

private:
  // Scopes currently cached in memory
  // Maps: scope name -> scope's cache info
  nsClassHashtable<nsCStringHashKey, nsScopeCache> mScopeCaches;
};

#endif
