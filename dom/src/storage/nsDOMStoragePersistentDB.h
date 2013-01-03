/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMStoragePersistentDB_h___
#define nsDOMStoragePersistentDB_h___

#include "nscore.h"
#include "nsDOMStorageBaseDB.h"
#include "mozIStorageConnection.h"
#include "mozIStorageStatement.h"
#include "mozilla/storage/StatementCache.h"
#include "mozIStorageBindingParamsArray.h"
#include "nsTHashtable.h"
#include "nsDataHashtable.h"
#include "nsIThread.h"

#include "nsLocalStorageCache.h"

class DOMStorageImpl;
class nsSessionStorageEntry;

class nsDOMStoragePersistentDB : public nsDOMStorageBaseDB
{
  typedef mozilla::storage::StatementCache<mozIStorageStatement> StatementCache;
  typedef nsLocalStorageCache::FlushData FlushData;

public:
  nsDOMStoragePersistentDB();
  ~nsDOMStoragePersistentDB() {}

  nsresult
  Init(const nsString& aDatabaseName);

  /**
   * Close the connection, finalizing all the cached statements.
   */
  void
  Close();

  /**
   * Indicates whether any data is cached that might need to be
   * flushed or evicted.
   */
  bool
  IsFlushTimerNeeded() const;

  /**
   * Retrieve a list of all the keys associated with a particular domain.
   */
  nsresult
  GetAllKeys(DOMStorageImpl* aStorage,
             nsTHashtable<nsSessionStorageEntry>* aKeys);

  /**
   * Retrieve a value and secure flag for a key from storage.
   *
   * @throws NS_ERROR_DOM_NOT_FOUND_ERR if key not found
   */
  nsresult
  GetKeyValue(DOMStorageImpl* aStorage,
              const nsAString& aKey,
              nsAString& aValue,
              bool* aSecure);

  /**
   * Set the value and secure flag for a key in storage.
   */
  nsresult
  SetKey(DOMStorageImpl* aStorage,
         const nsAString& aKey,
         const nsAString& aValue,
         bool aSecure);

  /**
   * Set the secure flag for a key in storage. Does nothing if the key was
   * not found.
   */
  nsresult
  SetSecure(DOMStorageImpl* aStorage,
            const nsAString& aKey,
            const bool aSecure);

  /**
   * Removes a key from storage.
   */
  nsresult
  RemoveKey(DOMStorageImpl* aStorage,
            const nsAString& aKey);

  /**
   * Remove all keys belonging to this storage.
   */
  nsresult
  ClearStorage(DOMStorageImpl* aStorage);

  /**
   * Removes all keys added by a given domain.
   */
  nsresult
  RemoveOwner(const nsACString& aOwner);

  /**
   * Removes all keys from storage. Used when clearing storage.
   */
  nsresult
  RemoveAll();

  /**
   * Removes all keys from storage for a specific app.
   * If aOnlyBrowserElement is true, it will remove only keys with the
   * browserElement flag set.
   * aAppId has to be a valid app id. It can't be NO_APP_ID or UNKNOWN_APP_ID.
   */
  nsresult
  RemoveAllForApp(uint32_t aAppId, bool aOnlyBrowserElement);

  /**
   * Returns usage for a storage using its GetQuotaDBKey() as a key.
   */
  nsresult
  GetUsage(DOMStorageImpl* aStorage, int32_t *aUsage);

  /**
   * Returns usage of the domain and optionaly by any subdomain.
   */
  nsresult
  GetUsage(const nsACString& aDomain, int32_t *aUsage);

  /**
   * Clears all in-memory data from private browsing mode
   */
  nsresult
  ClearAllPrivateBrowsingData();

  /**
   * This is the top-level method called by timer & shutdown code.
   *
   * LocalStorage now flushes dirty items off the main thread to reduce
   * main thread jank.
   *
   * When the flush timer fires, pointers to changed data are retrieved from
   * nsLocalStorageCache on the main thread and are used to build an SQLite
   * statement array with bound parameters that is then executed on a
   * background thread. Only one flush operation is outstanding at once.
   * After the flush operation completes, a notification task
   * is enqueued on the main thread which updates the cache state.
   *
   * Cached scopes are evicted if they aren't dirty and haven't been used for
   * the maximum idle time.
   */
  nsresult
  FlushAndEvictFromCache(bool aMainThread);

  /**
   * Executes SQL statements for flushing dirty data to disk.
   */
  nsresult
  Flush();

  /**
   * The notifier task calls this method to update cache state after a
   * flush operation completes.
   */
  void
  HandleFlushComplete(bool aSucceeded);

private:

  friend class nsDOMStorageMemoryDB;

  /**
   * Sets the database's journal mode to WAL or TRUNCATE.
   */
  nsresult
  SetJournalMode(bool aIsWal);

  /**
   * Ensures that the scope's keys are cached.
   */
  nsresult
  EnsureScopeLoaded(DOMStorageImpl* aStorage);

  /**
   * Fetches the scope's data from the database.
   */
  nsresult
  FetchScope(DOMStorageImpl* aStorage, nsScopeCache* aScopeCache);

  /**
   * Ensures that the quota usage information for the site is known.
   *
   * The current quota usage is calculated from the space occupied by
   * the cached scopes + the size of the uncached scopes on disk.
   * This method ensures that the size of the site's uncached scopes
   * on disk is known.
   */
  nsresult
  EnsureQuotaUsageLoaded(const nsACString& aQuotaKey);

  /**
   * Fetches the size of scopes matching aQuotaDBKey from the database.
   */
  nsresult
  FetchQuotaUsage(const nsACString& aQuotaDBKey);

  nsresult
  GetUsageInternal(const nsACString& aQuotaDBKey, int32_t *aUsage);

  /**
   * Helper function for RemoveOwner and RemoveAllForApp.
   */
  nsresult
  FetchMatchingScopeNames(const nsACString& aPattern);

  nsresult
  PrepareFlushStatements(const FlushData& aFlushData);

  /**
   * Gathers the dirty data from the cache, prepares SQL statements and
   * updates state flags
   */
  nsresult
  PrepareForFlush();

  /**
   * Removes non-dirty scopes that haven't been used in X seconds.
   */
  void
  EvictUnusedScopes();

  /**
   * DB data structures
   */
  nsTArray<nsCOMPtr<mozIStorageStatement> > mFlushStatements;
  nsTArray<nsCOMPtr<mozIStorageBindingParamsArray> > mFlushStatementParams;
  StatementCache mStatements;
  nsCOMPtr<mozIStorageConnection> mConnection;

  /**
   * Cache state data
   */
  nsLocalStorageCache mCache;
  nsDataHashtable<nsCStringHashKey, int32_t> mQuotaUseByUncached;
  // Set if the DB needs to be emptied on next flush
  bool mWasRemoveAllCalled;
  // Set if the DB is currently getting emptied in an async flush
  bool mIsRemoveAllPending;
  // Set if a flush operation has been enqueued in the async thread
  bool mIsFlushPending;

  // Helper thread for flushing
  nsCOMPtr<nsIThread> mFlushThread;
};

#endif /* nsDOMStorageDB_h___ */
