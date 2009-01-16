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

#include "nsCOMPtr.h"
#include "nsDOMError.h"
#include "nsDOMStorage.h"
#include "nsDOMStorageDB.h"
#include "nsIFile.h"
#include "nsAppDirectoryServiceDefs.h"
#include "mozStorageCID.h"
#include "mozStorageHelper.h"
#include "mozIStorageService.h"
#include "mozIStorageValueArray.h"

nsresult
nsDOMStorageDB::Init()
{
  nsresult rv;

  nsCOMPtr<nsIFile> storageFile;
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                              getter_AddRefs(storageFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = storageFile->Append(NS_LITERAL_STRING("webappsstore.sqlite"));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageService> service;

  service = do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = service->OpenDatabase(storageFile, getter_AddRefs(mConnection));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    // delete the db and try opening again
    rv = storageFile->Remove(PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = service->OpenDatabase(storageFile, getter_AddRefs(mConnection));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool exists;
  rv = mConnection->TableExists(NS_LITERAL_CSTRING("webappsstore"), &exists);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! exists) {
    rv = mConnection->ExecuteSimpleSQL(
           NS_LITERAL_CSTRING("CREATE TABLE webappsstore ("
                              "domain TEXT, "
                              "key TEXT, "
                              "value TEXT, "
                              "secure INTEGER, "
                              "owner TEXT)"));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  rv = mConnection->TableExists(NS_LITERAL_CSTRING("moz_webappsstore"),
                                &exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {
      // upgrade an old store
      
      // create a temporary index to handle dup checking
       rv = mConnection->ExecuteSimpleSQL(
              NS_LITERAL_CSTRING("CREATE UNIQUE INDEX webappsstore_tmp "
                                 " ON webappsstore(domain, key)"));

      // if the index can't be created, there are dup domain/key combos
      // in moz_webappstore2, which indicates a bug elsewhere.  Fail to upgrade
      // in this case
      if (NS_SUCCEEDED(rv)) {
          rv = mConnection->ExecuteSimpleSQL(
                 NS_LITERAL_CSTRING("INSERT OR IGNORE INTO "
                                    "webappsstore(domain, key, value, secure, owner) "
                                    "SELECT domain, key, value, secure, domain "
                                    "FROM moz_webappsstore"));

          // try to drop the index even in case of an error
          mConnection->ExecuteSimpleSQL(
            NS_LITERAL_CSTRING("DROP INDEX webappsstore_tmp"));

          NS_ENSURE_SUCCESS(rv, rv);

          rv = mConnection->ExecuteSimpleSQL(
                 NS_LITERAL_CSTRING("DROP TABLE moz_webappsstore"));
          NS_ENSURE_SUCCESS(rv, rv);
      }
  }
  
  // retrieve all keys associated with a domain
  rv = mConnection->CreateStatement(
         NS_LITERAL_CSTRING("SELECT key, secure FROM webappsstore "
                            "WHERE domain = ?1"),
         getter_AddRefs(mGetAllKeysStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // retrieve a value given a domain and a key
  rv = mConnection->CreateStatement(
         NS_LITERAL_CSTRING("SELECT value, secure, owner FROM webappsstore "
                            "WHERE domain = ?1 "
                            "AND key = ?2"),
         getter_AddRefs(mGetKeyValueStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // insert a new key
  rv = mConnection->CreateStatement(
    NS_LITERAL_CSTRING("INSERT INTO "
                       "webappsstore(domain, key, value, secure, owner) "
                       "VALUES (?1, ?2, ?3, ?4, ?5)"),
         getter_AddRefs(mInsertKeyStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // update an existing key
  rv = mConnection->CreateStatement(
         NS_LITERAL_CSTRING("UPDATE webappsstore "
                            "SET value = ?1, secure = ?2, owner = ?3"
                            "WHERE domain = ?4 "
                            "AND key = ?5 "),
         getter_AddRefs(mUpdateKeyStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // update the secure status of an existing key
  rv = mConnection->CreateStatement(
         NS_LITERAL_CSTRING("UPDATE webappsstore "
                            "SET secure = ?1 "
                            "WHERE domain = ?2 "
                            "AND key = ?3 "),
         getter_AddRefs(mSetSecureStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // remove a key
  rv = mConnection->CreateStatement(
         NS_LITERAL_CSTRING("DELETE FROM webappsstore "
                            "WHERE domain = ?1 "
                            "AND key = ?2"),
         getter_AddRefs(mRemoveKeyStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // remove keys owned by a specific domain
  rv = mConnection->CreateStatement(
         NS_LITERAL_CSTRING("DELETE FROM webappsstore "
                            "WHERE owner = ?1"),
         getter_AddRefs(mRemoveOwnerStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // remove all keys
  rv = mConnection->CreateStatement(
         NS_LITERAL_CSTRING("DELETE FROM webappsstore"),
         getter_AddRefs(mRemoveAllStatement));
  NS_ENSURE_SUCCESS(rv, rv);
  
  // check the usage for a given owner
  rv = mConnection->CreateStatement(
         NS_LITERAL_CSTRING("SELECT SUM(LENGTH(key) + LENGTH(value)) "
                            "FROM webappsstore "
                            "WHERE owner = ?1"),
         getter_AddRefs(mGetUsageStatement));

  return rv;
}

nsresult
nsDOMStorageDB::GetAllKeys(const nsAString& aDomain,
                           nsDOMStorage* aStorage,
                           nsTHashtable<nsSessionStorageEntry>* aKeys)
{
  mozStorageStatementScoper scope(mGetAllKeysStatement);

  nsresult rv = mGetAllKeysStatement->BindStringParameter(0, aDomain);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool exists;
  while (NS_SUCCEEDED(rv = mGetAllKeysStatement->ExecuteStep(&exists)) &&
         exists) {

    nsAutoString key;
    rv = mGetAllKeysStatement->GetString(0, key);
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 secureInt = 0;
    rv = mGetAllKeysStatement->GetInt32(1, &secureInt);
    NS_ENSURE_SUCCESS(rv, rv);

    nsSessionStorageEntry* entry = aKeys->PutEntry(key);
    NS_ENSURE_TRUE(entry, NS_ERROR_OUT_OF_MEMORY);
 
    entry->mItem = new nsDOMStorageItem(aStorage, key, EmptyString(), secureInt);
    if (!entry->mItem) {
      aKeys->RawRemoveEntry(entry);
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return NS_OK;
}

nsresult
nsDOMStorageDB::GetKeyValue(const nsAString& aDomain,
                            const nsAString& aKey,
                            nsAString& aValue,
                            PRBool* aSecure,
                            nsAString& aOwner)
{
  mozStorageStatementScoper scope(mGetKeyValueStatement);

  nsresult rv = mGetKeyValueStatement->BindStringParameter(0, aDomain);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mGetKeyValueStatement->BindStringParameter(1, aKey);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool exists;
  rv = mGetKeyValueStatement->ExecuteStep(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 secureInt = 0;
  if (exists) {
    rv = mGetKeyValueStatement->GetString(0, aValue);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mGetKeyValueStatement->GetInt32(1, &secureInt);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mGetKeyValueStatement->GetString(2, aOwner);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    rv = NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  *aSecure = !!secureInt;

  return rv;
}

nsresult
nsDOMStorageDB::SetKey(const nsAString& aDomain,
                       const nsAString& aKey,
                       const nsAString& aValue,
                       PRBool aSecure,
                       const nsAString& aOwner,
                       PRInt32 aQuota,
                       PRInt32 *aNewUsage)
{
  mozStorageStatementScoper scope(mGetKeyValueStatement);
 
  PRInt32 usage = 0;
  nsresult rv;
  if (!aOwner.IsEmpty()) {
    rv = GetUsage(aOwner, &usage);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  usage += aKey.Length() + aValue.Length();

  rv = mGetKeyValueStatement->BindStringParameter(0, aDomain);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mGetKeyValueStatement->BindStringParameter(1, aKey);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool exists;
  rv = mGetKeyValueStatement->ExecuteStep(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {
    if (!aSecure) {
      PRInt32 secureInt = 0;
      rv = mGetKeyValueStatement->GetInt32(1, &secureInt);
      NS_ENSURE_SUCCESS(rv, rv);
      if (secureInt)
        return NS_ERROR_DOM_SECURITY_ERR;
    }

    nsAutoString previousOwner;
    rv = mGetKeyValueStatement->GetString(2, previousOwner);
    NS_ENSURE_SUCCESS(rv, rv);
    
    if (previousOwner == aOwner) {
      nsAutoString previousValue;
      rv = mGetKeyValueStatement->GetString(0, previousValue);
      NS_ENSURE_SUCCESS(rv, rv);
      usage -= aKey.Length() + previousValue.Length();
    }

    mGetKeyValueStatement->Reset();

    if (usage > aQuota) {
      return NS_ERROR_DOM_QUOTA_REACHED;
    }

    mozStorageStatementScoper scopeupdate(mUpdateKeyStatement);

    rv = mUpdateKeyStatement->BindStringParameter(0, aValue);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mUpdateKeyStatement->BindInt32Parameter(1, aSecure);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mUpdateKeyStatement->BindStringParameter(2, aOwner);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mUpdateKeyStatement->BindStringParameter(3, aDomain);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mUpdateKeyStatement->BindStringParameter(4, aKey);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mUpdateKeyStatement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    if (usage > aQuota) {
      return NS_ERROR_DOM_QUOTA_REACHED;
    }
    
    mozStorageStatementScoper scopeinsert(mInsertKeyStatement);
    
    rv = mInsertKeyStatement->BindStringParameter(0, aDomain);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mInsertKeyStatement->BindStringParameter(1, aKey);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mInsertKeyStatement->BindStringParameter(2, aValue);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mInsertKeyStatement->BindInt32Parameter(3, aSecure);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mInsertKeyStatement->BindStringParameter(4, aOwner);
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = mInsertKeyStatement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!aOwner.IsEmpty()) {
    mCachedOwner = aOwner;
    mCachedUsage = usage;
  }

  *aNewUsage = usage;

  return NS_OK;
}

nsresult
nsDOMStorageDB::SetSecure(const nsAString& aDomain,
                          const nsAString& aKey,
                          const PRBool aSecure)
{
  mozStorageStatementScoper scope(mGetKeyValueStatement);

  nsresult rv = mGetKeyValueStatement->BindStringParameter(0, aDomain);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mGetKeyValueStatement->BindStringParameter(1, aKey);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool exists;
  rv = mGetKeyValueStatement->ExecuteStep(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {
    mGetKeyValueStatement->Reset();
    
    mozStorageStatementScoper scopeupdate(mUpdateKeyStatement);

    rv = mSetSecureStatement->BindInt32Parameter(0, aSecure ? 1 : 0);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mSetSecureStatement->BindStringParameter(1, aDomain);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mSetSecureStatement->BindStringParameter(2, aKey);
    NS_ENSURE_SUCCESS(rv, rv);

    return mSetSecureStatement->Execute();
  }

  return NS_OK;
}

nsresult
nsDOMStorageDB::RemoveKey(const nsAString& aDomain,
                          const nsAString& aKey,
                          const nsAString& aOwner,
                          PRInt32 aKeyUsage)
{
  mozStorageStatementScoper scope(mRemoveKeyStatement);

  if (aOwner == mCachedOwner) {
    mCachedUsage -= aKeyUsage;
  }

  nsresult rv = mRemoveKeyStatement->BindStringParameter(0, aDomain);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mRemoveKeyStatement->BindStringParameter(1, aKey);
  NS_ENSURE_SUCCESS(rv, rv);

  return mRemoveKeyStatement->Execute();
}

nsresult
nsDOMStorageDB::RemoveOwner(const nsAString& aOwner)
{
  mozStorageStatementScoper scope(mRemoveOwnerStatement);

  if (aOwner == mCachedOwner) {
    mCachedUsage = 0;
    mCachedOwner.Truncate();
  }

  nsresult rv = mRemoveOwnerStatement->BindStringParameter(0, aOwner);
  NS_ENSURE_SUCCESS(rv, rv);

  return mRemoveOwnerStatement->Execute();
}


nsresult
nsDOMStorageDB::RemoveOwners(const nsTArray<nsString> &aOwners, PRBool aMatch)
{
  if (aOwners.Length() == 0) {
    if (aMatch) {
      return NS_OK;
    }

    return RemoveAll();
  }

  nsCAutoString expression;

  if (aMatch) {
    expression.Assign(NS_LITERAL_CSTRING("DELETE FROM webappsstore "
                                         "WHERE owner IN (?"));
  } else {
    expression.Assign(NS_LITERAL_CSTRING("DELETE FROM webappsstore "
                                         "WHERE owner NOT IN (?"));
  }

  for (PRUint32 i = 1; i < aOwners.Length(); i++) {
    expression.Append(", ?");
  }
  expression.Append(")");

  nsCOMPtr<mozIStorageStatement> statement;

  nsresult rv = mConnection->CreateStatement(expression,
                                             getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 i = 0; i < aOwners.Length(); i++) {
    rv = statement->BindStringParameter(i, aOwners[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return statement->Execute();
}

nsresult
nsDOMStorageDB::RemoveAll()
{
  mozStorageStatementScoper scope(mRemoveAllStatement);
  return mRemoveAllStatement->Execute();
}

nsresult
nsDOMStorageDB::GetUsage(const nsAString &aOwner, PRInt32 *aUsage)
{
  if (aOwner == mCachedOwner) {
    *aUsage = mCachedUsage;
    return NS_OK;
  }

  mozStorageStatementScoper scope(mGetUsageStatement);

  nsresult rv = mGetUsageStatement->BindStringParameter(0, aOwner);
  NS_ENSURE_SUCCESS(rv, rv);
  
  PRBool exists;
  rv = mGetUsageStatement->ExecuteStep(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!exists) {
    *aUsage = 0;
    return NS_OK;
  }
  
  rv = mGetUsageStatement->GetInt32(0, aUsage);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aOwner.IsEmpty()) {
    mCachedOwner = aOwner;
    mCachedUsage = *aUsage;
  }

  return NS_OK;
}
