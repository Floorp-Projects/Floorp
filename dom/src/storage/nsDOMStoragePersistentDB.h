/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Neil Deakin <enndeakin@sympatico.ca>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsDOMStoragePersistentDB_h___
#define nsDOMStoragePersistentDB_h___

#include "nscore.h"
#include "nsDOMStorageBaseDB.h"
#include "mozIStorageConnection.h"
#include "mozIStorageStatement.h"
#include "nsTHashtable.h"
#include "nsDataHashtable.h"
#include "mozilla/TimeStamp.h"

class DOMStorageImpl;
class nsSessionStorageEntry;

using mozilla::TimeStamp;
using mozilla::TimeDuration;

class nsDOMStoragePersistentDB : public nsDOMStorageBaseDB
{
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

  nsCOMPtr<mozIStorageStatement> mCopyToTempTableStatement;
  nsCOMPtr<mozIStorageStatement> mCopyBackToDiskStatement;
  nsCOMPtr<mozIStorageStatement> mDeleteTemporaryTableStatement;
  nsCOMPtr<mozIStorageStatement> mGetAllKeysStatement;
  nsCOMPtr<mozIStorageStatement> mGetKeyValueStatement;
  nsCOMPtr<mozIStorageStatement> mInsertKeyStatement;
  nsCOMPtr<mozIStorageStatement> mSetSecureStatement;
  nsCOMPtr<mozIStorageStatement> mRemoveKeyStatement;
  nsCOMPtr<mozIStorageStatement> mRemoveOwnerStatement;
  nsCOMPtr<mozIStorageStatement> mRemoveStorageStatement;
  nsCOMPtr<mozIStorageStatement> mRemoveAllStatement;
  nsCOMPtr<mozIStorageStatement> mGetOfflineExcludedUsageStatement;
  nsCOMPtr<mozIStorageStatement> mGetFullUsageStatement;
  // If you add an statement, remember to null in in Close.

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
};

#endif /* nsDOMStorageDB_h___ */
