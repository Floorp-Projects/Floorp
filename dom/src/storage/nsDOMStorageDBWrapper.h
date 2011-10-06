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
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Honza Bambas <honzab@firemni.cz>
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

#ifndef nsDOMStorageDB_h___
#define nsDOMStorageDB_h___

#include "nscore.h"
#include "nsTHashtable.h"

#include "nsDOMStoragePersistentDB.h"
#include "nsDOMStorageMemoryDB.h"

extern void ReverseString(const nsCSubstring& source, nsCSubstring& result);

class nsDOMStorage;
class nsSessionStorageEntry;

/**
 * For the purposes of quota checking, we want to be able to efficiently
 * reference data items that belong to a host or its subhosts.  We do this by
 * using a reversed domain name as the key for an item.  For example, a
 * storage for foo.bar.com would use a key of 'moc.rab.oof.".
 *
 * Additionally, globalStorage and localStorage items must be distinguished.
 * globalStorage items are scoped to the host, and localStorage are items are
 * scoped to the scheme/host/port.  To scope localStorage data, its port and
 * scheme are appended to its key.  http://foo.bar.com is stored as
 * moc.rab.foo.:http:80.
 *
 * So the following queries can be used, for http://foo.bar.com:
 *
 * All data owned by globalStorage["foo.bar.com"] -> SELECT * WHERE Domain =
 * "moc.rab.foo.:"
 *
 * All data owned by localStorage -> SELECT * WHERE Domain =
 * "moc.rab.foo.:http:80"
 *
 * All data owned by foo.bar.com, in any storage ->
 * SELECT * WHERE Domain GLOB "moc.rab.foo.:*"
 *
 * All data owned by foo.bar.com or any subdomain, in any storage ->
 * SELECT * WHERE Domain GLOB "moc.rab.foo.*".
 *
 * This key is called the "scope DB key" throughout the code.  So the scope DB
 * key for localStorage at http://foo.bar.com is "moc.rab.foo.:http:80".
 *
 * When calculating quotas, we want to lump together everything in an ETLD+1.
 * So we use a "quota key" during lookups to calculate the quota.  So the
 * quota key for localStorage at http://foo.bar.com is "moc.rab.". */

class nsDOMStorageDBWrapper
{
public:
  nsDOMStorageDBWrapper();
  ~nsDOMStorageDBWrapper();

  nsresult
  Init();

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
              PRBool* aSecure);

  /**
   * Set the value and secure flag for a key in storage.
   */
  nsresult
  SetKey(DOMStorageImpl* aStorage,
         const nsAString& aKey,
         const nsAString& aValue,
         PRBool aSecure,
         PRInt32 aQuota,
         PRBool aExcludeOfflineFromUsage,
         PRInt32* aNewUsage);

  /**
   * Set the secure flag for a key in storage. Does nothing if the key was
   * not found.
   */
  nsresult
  SetSecure(DOMStorageImpl* aStorage,
            const nsAString& aKey,
            const PRBool aSecure);

  /**
   * Removes a key from storage.
   */
  nsresult
  RemoveKey(DOMStorageImpl* aStorage,
            const nsAString& aKey,
            PRBool aExcludeOfflineFromUsage,
            PRInt32 aKeyUsage);

  /**
    * Remove all keys belonging to this storage.
    */
  nsresult
  ClearStorage(DOMStorageImpl* aStorage);

  /**
   * Drop session-only storage for a specific host and all it's subdomains
   */
  nsresult
  DropSessionOnlyStoragesForHost(const nsACString& aHostName);

  /**
   * Drop everything we gathered to private browsing in-memory database
   */
  nsresult
  DropPrivateBrowsingStorages();

  /**
   * Removes all keys added by a given domain.
   */
  nsresult
  RemoveOwner(const nsACString& aOwner, PRBool aIncludeSubDomains);

  /**
   * Removes keys owned by domains that either match or don't match the
   * list.
   */
  nsresult
  RemoveOwners(const nsTArray<nsString>& aOwners,
               PRBool aIncludeSubDomains, PRBool aMatch);

  /**
   * Removes all keys from storage. Used when clearing storage.
   */
  nsresult
  RemoveAll();

  /**
    * Returns usage for a storage using its GetQuotaDomainDBKey() as a key.
    */
  nsresult
  GetUsage(DOMStorageImpl* aStorage, PRBool aExcludeOfflineFromUsage, PRInt32 *aUsage);

  /**
    * Returns usage of the domain and optionaly by any subdomain.
    */
  nsresult
  GetUsage(const nsACString& aDomain, PRBool aIncludeSubDomains, PRInt32 *aUsage);

  /**
   * Marks the storage as "cached" after the DOMStorageImpl object has loaded
   * all items to its memory copy of the entries - IsScopeDirty returns false
   * after call of this method for this storage.
   *
   * When a key is changed or deleted in the storage, the storage scope is
   * marked as "dirty" again and makes the DOMStorageImpl object recache its
   * keys on next access, because IsScopeDirty returns true again.
   */
  void
  MarkScopeCached(DOMStorageImpl* aStorage);

  /**
   * Test whether the storage for the scope (i.e. origin or host) has been
   * changed since the last MarkScopeCached call.
   */
  bool
  IsScopeDirty(DOMStorageImpl* aStorage);

  /**
    * Turns "http://foo.bar.com:80" to "moc.rab.oof.:http:80",
    * i.e. reverses the host, appends a dot, appends the schema
    * and a port number.
    */
  static nsresult CreateOriginScopeDBKey(nsIURI* aUri, nsACString& aKey);

  /**
    * Turns "http://foo.bar.com" to "moc.rab.oof.",
    * i.e. reverses the host and appends a dot.
    */
  static nsresult CreateDomainScopeDBKey(nsIURI* aUri, nsACString& aKey);
  static nsresult CreateDomainScopeDBKey(const nsACString& aAsciiDomain, nsACString& aKey);

  /**
    * Turns "foo.bar.com" to "moc.rab.",
    * i.e. extracts eTLD+1 from the host, reverses the result
    * and appends a dot.
    */
  static nsresult CreateQuotaDomainDBKey(const nsACString& aAsciiDomain,
                                         PRBool aIncludeSubDomains, PRBool aETLDplus1Only,
                                         nsACString& aKey);

  static nsresult GetDomainFromScopeKey(const nsACString& aScope,
                                         nsACString& aDomain);

  /**
   * Ensures the temp table flush timer is running. This is called when we add
   * data that will need to be flushed.
   */
  void EnsureTempTableFlushTimer();

  /**
   * Called by the timer or on shutdown/profile change to flush all temporary
   * tables that are too long in memory to disk.
   * Set force to flush even a table doesn't meet the age limits.  Used during
   * shutdown.
   */
  nsresult FlushAndDeleteTemporaryTables(bool force);

  /**
   * Stops the temp table flush timer.
   */
  void StopTempTableFlushTimer();

protected:
  nsDOMStoragePersistentDB mChromePersistentDB;
  nsDOMStoragePersistentDB mPersistentDB;
  nsDOMStorageMemoryDB mSessionOnlyDB;
  nsDOMStorageMemoryDB mPrivateBrowsingDB;

  nsCOMPtr<nsITimer> mTempTableFlushTimer;
};

#endif /* nsDOMStorageDB_h___ */
