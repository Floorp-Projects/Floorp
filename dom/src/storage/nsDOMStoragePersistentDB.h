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
#include "nsTHashtable.h"
#include "nsDataHashtable.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/storage/StatementCache.h"

class DOMStorageImpl;
class nsSessionStorageEntry;

using mozilla::TimeStamp;
using mozilla::TimeDuration;

class nsDOMStoragePersistentDB : public nsDOMStorageBaseDB
{
  typedef mozilla::storage::StatementCache<mozIStorageStatement> StatementCache;

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
         bool aSecure,
         PRInt32 aQuota,
         bool aExcludeOfflineFromUsage,
         PRInt32* aNewUsage);

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
            const nsAString& aKey,
            bool aExcludeOfflineFromUsage,
            PRInt32 aKeyUsage);

  /**
    * Remove all keys belonging to this storage.
    */
  nsresult ClearStorage(DOMStorageImpl* aStorage);

  /**
   * Removes all keys added by a given domain.
   */
  nsresult
  RemoveOwner(const nsACString& aOwner, bool aIncludeSubDomains);

  /**
   * Removes keys owned by domains that either match or don't match the
   * list.
   */
  nsresult
  RemoveOwners(const nsTArray<nsString>& aOwners,
               bool aIncludeSubDomains, bool aMatch);

  /**
   * Removes all keys from storage. Used when clearing storage.
   */
  nsresult
  RemoveAll();

  /**
    * Returns usage for a storage using its GetQuotaDomainDBKey() as a key.
    */
  nsresult
  GetUsage(DOMStorageImpl* aStorage, bool aExcludeOfflineFromUsage, PRInt32 *aUsage);

  /**
    * Returns usage of the domain and optionaly by any subdomain.
    */
  nsresult
  GetUsage(const nsACString& aDomain, bool aIncludeSubDomains, PRInt32 *aUsage);

  /**
   * Clears all in-memory data from private browsing mode
   */
  nsresult ClearAllPrivateBrowsingData();

  /**
   * We process INSERTs in a transaction because of performance.
   * If there is currently no transaction in progress, start one.
   */
  nsresult EnsureInsertTransaction();

  /**
   * If there is an INSERT transaction in progress, commit it now.
   */
  nsresult MaybeCommitInsertTransaction();

  /**
   * Flushes all temporary tables based on time or forcibly during shutdown. 
   */
  nsresult FlushTemporaryTables(bool force);

protected:
  /**
   * Ensures that a temporary table is correctly filled for the scope of
   * the given storage.
   */
  nsresult EnsureLoadTemporaryTableForStorage(DOMStorageImpl* aStorage);

  struct FlushTemporaryTableData {
    nsDOMStoragePersistentDB* mDB;
    bool mForce;
    nsresult mRV;
  };
  static PLDHashOperator FlushTemporaryTable(nsCStringHashKey::KeyType aKey,
                                             TimeStamp& aData,
                                             void* aUserArg);       

  nsCOMPtr<mozIStorageConnection> mConnection;
  StatementCache mStatements;

  nsCString mCachedOwner;
  PRInt32 mCachedUsage;

  // Maps ScopeDBKey to time of the temporary table load for that scope.
  // If a record is present, the temp table has been loaded. If it is not
  // present, the table has not yet been loaded or has alrady been flushed.
  nsDataHashtable<nsCStringHashKey, TimeStamp> mTempTableLoads; 

  friend class nsDOMStorageDBWrapper;
  friend class nsDOMStorageMemoryDB;
  nsresult
  GetUsageInternal(const nsACString& aQuotaDomainDBKey, bool aExcludeOfflineFromUsage, PRInt32 *aUsage);

  // Compares aDomain with the mCachedOwner and returns false if changes
  // in aDomain don't affect mCachedUsage.
  bool DomainMaybeCached(const nsACString& aDomain);

};

#endif /* nsDOMStorageDB_h___ */
