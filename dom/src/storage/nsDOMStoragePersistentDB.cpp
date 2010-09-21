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
#include "nsDOMStorageDBWrapper.h"
#include "nsDOMStoragePersistentDB.h"
#include "nsIFile.h"
#include "nsIVariant.h"
#include "nsIEffectiveTLDService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "mozStorageCID.h"
#include "mozStorageHelper.h"
#include "mozIStorageService.h"
#include "mozIStorageValueArray.h"
#include "mozIStorageFunction.h"
#include "nsPrintfCString.h"
#include "nsNetUtil.h"

class nsReverseStringSQLFunction : public mozIStorageFunction
{
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION
};

NS_IMPL_ISUPPORTS1(nsReverseStringSQLFunction, mozIStorageFunction)

NS_IMETHODIMP
nsReverseStringSQLFunction::OnFunctionCall(
    mozIStorageValueArray *aFunctionArguments, nsIVariant **aResult)
{
  nsresult rv;

  nsCAutoString stringToReverse;
  rv = aFunctionArguments->GetUTF8String(0, stringToReverse);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString result;
  ReverseString(stringToReverse, result);

  nsCOMPtr<nsIWritableVariant> outVar(do_CreateInstance(
      NS_VARIANT_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = outVar->SetAsAUTF8String(result);
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = outVar.get();
  outVar.forget();
  return NS_OK;
}

class nsIsOfflineSQLFunction : public mozIStorageFunction
{
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION
};

NS_IMPL_ISUPPORTS1(nsIsOfflineSQLFunction, mozIStorageFunction)

NS_IMETHODIMP
nsIsOfflineSQLFunction::OnFunctionCall(
    mozIStorageValueArray *aFunctionArguments, nsIVariant **aResult)
{
  nsresult rv;

  nsCAutoString scope;
  rv = aFunctionArguments->GetUTF8String(0, scope);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString domain;
  rv = nsDOMStorageDBWrapper::GetDomainFromScopeKey(scope, domain);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasOfflinePermission = IsOfflineAllowed(domain);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIWritableVariant> outVar(do_CreateInstance(
      NS_VARIANT_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = outVar->SetAsBool(hasOfflinePermission);
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = outVar.get();
  outVar.forget();
  return NS_OK;
}

nsresult
nsDOMStoragePersistentDB::Init(const nsString& aDatabaseName)
{
  nsresult rv;

  nsCOMPtr<nsIFile> storageFile;
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                              getter_AddRefs(storageFile));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = storageFile->Append(aDatabaseName);
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

  // Ensure Gecko 1.9.1 storage table
  rv = mConnection->ExecuteSimpleSQL(
         NS_LITERAL_CSTRING("CREATE TABLE IF NOT EXISTS webappsstore2 ("
                            "scope TEXT, "
                            "key TEXT, "
                            "value TEXT, "
                            "secure INTEGER, "
                            "owner TEXT)"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "CREATE UNIQUE INDEX IF NOT EXISTS scope_key_index"
        " ON webappsstore2(scope, key)"));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageFunction> function1(new nsReverseStringSQLFunction());
  NS_ENSURE_TRUE(function1, NS_ERROR_OUT_OF_MEMORY);

  rv = mConnection->CreateFunction(NS_LITERAL_CSTRING("REVERSESTRING"), 1, function1);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageFunction> function2(new nsIsOfflineSQLFunction());
  NS_ENSURE_TRUE(function2, NS_ERROR_OUT_OF_MEMORY);

  rv = mConnection->CreateFunction(NS_LITERAL_CSTRING("ISOFFLINE"), 1, function2);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool exists;

  // Check if there is storage of Gecko 1.9.0 and if so, upgrade that storage
  // to actual webappsstore2 table and drop the obsolete table. First process
  // this newer table upgrade to priority potential duplicates from older
  // storage table.
  rv = mConnection->TableExists(NS_LITERAL_CSTRING("webappsstore"),
                                &exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {
      rv = mConnection->ExecuteSimpleSQL(
             NS_LITERAL_CSTRING("INSERT OR IGNORE INTO "
                                "webappsstore2(scope, key, value, secure, owner) "
                                "SELECT REVERSESTRING(domain) || '.:', key, value, secure, owner "
                                "FROM webappsstore"));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = mConnection->ExecuteSimpleSQL(
             NS_LITERAL_CSTRING("DROP TABLE webappsstore"));
      NS_ENSURE_SUCCESS(rv, rv);
  }

  // Check if there is storage of Gecko 1.8 and if so, upgrade that storage
  // to actual webappsstore2 table and drop the obsolete table. Potential
  // duplicates will be ignored.
  rv = mConnection->TableExists(NS_LITERAL_CSTRING("moz_webappsstore"),
                                &exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {
      rv = mConnection->ExecuteSimpleSQL(
             NS_LITERAL_CSTRING("INSERT OR IGNORE INTO "
                                "webappsstore2(scope, key, value, secure, owner) "
                                "SELECT REVERSESTRING(domain) || '.:', key, value, secure, domain "
                                "FROM moz_webappsstore"));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = mConnection->ExecuteSimpleSQL(
             NS_LITERAL_CSTRING("DROP TABLE moz_webappsstore"));
      NS_ENSURE_SUCCESS(rv, rv);
  }

  // retrieve all keys associated with a domain
  rv = mConnection->CreateStatement(
         NS_LITERAL_CSTRING("SELECT key, secure FROM webappsstore2 "
                            "WHERE scope = ?1"),
         getter_AddRefs(mGetAllKeysStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // retrieve a value given a domain and a key
  rv = mConnection->CreateStatement(
         NS_LITERAL_CSTRING("SELECT value, secure FROM webappsstore2 "
                            "WHERE scope = ?1 "
                            "AND key = ?2"),
         getter_AddRefs(mGetKeyValueStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // insert a new key
  rv = mConnection->CreateStatement(
    NS_LITERAL_CSTRING("INSERT OR REPLACE INTO "
                            "webappsstore2(scope, key, value, secure) "
                            "VALUES (?1, ?2, ?3, ?4)"),
         getter_AddRefs(mInsertKeyStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // update an existing key
  rv = mConnection->CreateStatement(
         NS_LITERAL_CSTRING("UPDATE webappsstore2 "
                            "SET value = ?1, secure = ?2"
                            "WHERE scope = ?3 "
                            "AND key = ?4"),
         getter_AddRefs(mUpdateKeyStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // update the secure status of an existing key
  rv = mConnection->CreateStatement(
         NS_LITERAL_CSTRING("UPDATE webappsstore2 "
                            "SET secure = ?1 "
                            "WHERE scope = ?2 "
                            "AND key = ?3 "),
         getter_AddRefs(mSetSecureStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // remove a key
  rv = mConnection->CreateStatement(
         NS_LITERAL_CSTRING("DELETE FROM webappsstore2 "
                            "WHERE scope = ?1 "
                            "AND key = ?2"),
         getter_AddRefs(mRemoveKeyStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // remove keys owned by a specific domain
  rv = mConnection->CreateStatement(
         NS_LITERAL_CSTRING("DELETE FROM webappsstore2 "
                            "WHERE scope GLOB ?1"),
         getter_AddRefs(mRemoveOwnerStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // remove keys belonging exactly only to a specific domain
  rv = mConnection->CreateStatement(
         NS_LITERAL_CSTRING("DELETE FROM webappsstore2 "
                            "WHERE scope = ?1"),
         getter_AddRefs(mRemoveStorageStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // remove all keys
  rv = mConnection->CreateStatement(
         NS_LITERAL_CSTRING("DELETE FROM webappsstore2"),
         getter_AddRefs(mRemoveAllStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // check the usage for a given owner that is an offline-app allowed domain
  rv = mConnection->CreateStatement(
         NS_LITERAL_CSTRING("SELECT SUM(LENGTH(key) + LENGTH(value)) "
                            "FROM webappsstore2 "
                            "WHERE scope GLOB ?1"),
         getter_AddRefs(mGetFullUsageStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // check the usage for a given owner that is not an offline-app allowed domain
  rv = mConnection->CreateStatement(
         NS_LITERAL_CSTRING("SELECT SUM(LENGTH(key) + LENGTH(value)) "
                            "FROM webappsstore2 "
                            "WHERE scope GLOB ?1 "
                            "AND NOT ISOFFLINE(scope)"),
         getter_AddRefs(mGetOfflineExcludedUsageStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsDOMStoragePersistentDB::GetAllKeys(nsDOMStorage* aStorage,
                                     nsTHashtable<nsSessionStorageEntry>* aKeys)
{
  mozStorageStatementScoper scope(mGetAllKeysStatement);

  nsresult rv = mGetAllKeysStatement->BindUTF8StringParameter(0, aStorage->GetScopeDBKey());
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
nsDOMStoragePersistentDB::GetKeyValue(nsDOMStorage* aStorage,
                                      const nsAString& aKey,
                                      nsAString& aValue,
                                      PRBool* aSecure)
{
  mozStorageStatementScoper scope(mGetKeyValueStatement);

  nsresult rv = mGetKeyValueStatement->BindUTF8StringParameter(
                                                  0, aStorage->GetScopeDBKey());
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
  }
  else {
    rv = NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  *aSecure = !!secureInt;

  return rv;
}

nsresult
nsDOMStoragePersistentDB::SetKey(nsDOMStorage* aStorage,
                                 const nsAString& aKey,
                                 const nsAString& aValue,
                                 PRBool aSecure,
                                 PRInt32 aQuota,
                                 PRBool aExcludeOfflineFromUsage,
                                 PRInt32 *aNewUsage)
{
  mozStorageStatementScoper scope(mGetKeyValueStatement);

  PRInt32 usage = 0;
  nsresult rv;
  if (!aStorage->GetQuotaDomainDBKey(!aExcludeOfflineFromUsage).IsEmpty()) {
    rv = GetUsage(aStorage, aExcludeOfflineFromUsage, &usage);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  usage += aKey.Length() + aValue.Length();

  rv = mGetKeyValueStatement->BindUTF8StringParameter(0,
                                                      aStorage->GetScopeDBKey());
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

    nsAutoString previousValue;
    rv = mGetKeyValueStatement->GetString(0, previousValue);
    NS_ENSURE_SUCCESS(rv, rv);

    usage -= aKey.Length() + previousValue.Length();

    mGetKeyValueStatement->Reset();

    if (usage > aQuota) {
      return NS_ERROR_DOM_QUOTA_REACHED;
    }

    mozStorageStatementScoper scopeupdate(mUpdateKeyStatement);

    rv = mUpdateKeyStatement->BindStringParameter(0, aValue);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mUpdateKeyStatement->BindInt32Parameter(1, aSecure);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mUpdateKeyStatement->BindUTF8StringParameter(2,
                                                      aStorage->GetScopeDBKey());
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mUpdateKeyStatement->BindStringParameter(3, aKey);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mUpdateKeyStatement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    if (usage > aQuota) {
      return NS_ERROR_DOM_QUOTA_REACHED;
    }

    mozStorageStatementScoper scopeinsert(mInsertKeyStatement);

    rv = mInsertKeyStatement->BindUTF8StringParameter(0,
                                                      aStorage->GetScopeDBKey());
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mInsertKeyStatement->BindStringParameter(1, aKey);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mInsertKeyStatement->BindStringParameter(2, aValue);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mInsertKeyStatement->BindInt32Parameter(3, aSecure);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mInsertKeyStatement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!aStorage->GetQuotaDomainDBKey(!aExcludeOfflineFromUsage).IsEmpty()) {
    mCachedOwner = aStorage->GetQuotaDomainDBKey(!aExcludeOfflineFromUsage);
    mCachedUsage = usage;
  }

  *aNewUsage = usage;

  return NS_OK;
}

nsresult
nsDOMStoragePersistentDB::SetSecure(nsDOMStorage* aStorage,
                                    const nsAString& aKey,
                                    const PRBool aSecure)
{
  nsresult rv;

  mozStorageStatementScoper scope(mSetSecureStatement);

  rv = mSetSecureStatement->BindInt32Parameter(0, aSecure ? 1 : 0);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mSetSecureStatement->BindUTF8StringParameter(1, aStorage->GetScopeDBKey());
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mSetSecureStatement->BindStringParameter(2, aKey);
  NS_ENSURE_SUCCESS(rv, rv);

  return mSetSecureStatement->Execute();
}

nsresult
nsDOMStoragePersistentDB::RemoveKey(nsDOMStorage* aStorage,
                                    const nsAString& aKey,
                                    PRBool aExcludeOfflineFromUsage,
                                    PRInt32 aKeyUsage)
{
  mozStorageStatementScoper scope(mRemoveKeyStatement);

  if (aStorage->GetQuotaDomainDBKey(!aExcludeOfflineFromUsage) == mCachedOwner) {
    mCachedUsage -= aKeyUsage;
  }

  nsresult rv = mRemoveKeyStatement->BindUTF8StringParameter(
                                                0, aStorage->GetScopeDBKey());
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mRemoveKeyStatement->BindStringParameter(1, aKey);
  NS_ENSURE_SUCCESS(rv, rv);

  return mRemoveKeyStatement->Execute();
}

nsresult
nsDOMStoragePersistentDB::ClearStorage(nsDOMStorage* aStorage)
{
  mozStorageStatementScoper scope(mRemoveStorageStatement);

  mCachedUsage = 0;
  mCachedOwner.Truncate();

  nsresult rv;

  rv = mRemoveStorageStatement->BindUTF8StringParameter(
                                                0, aStorage->GetScopeDBKey());
  NS_ENSURE_SUCCESS(rv, rv);

  return mRemoveStorageStatement->Execute();
}

nsresult
nsDOMStoragePersistentDB::RemoveOwner(const nsACString& aOwner,
                                      PRBool aIncludeSubDomains)
{
  mozStorageStatementScoper scope(mRemoveOwnerStatement);

  nsCAutoString subdomainsDBKey;
  nsDOMStorageDBWrapper::CreateDomainScopeDBKey(aOwner, subdomainsDBKey);

  if (!aIncludeSubDomains)
    subdomainsDBKey.AppendLiteral(":");
  subdomainsDBKey.AppendLiteral("*");

  if (subdomainsDBKey == mCachedOwner) {
    mCachedUsage = 0;
    mCachedOwner.Truncate();
  }

  nsresult rv;

  rv = mRemoveOwnerStatement->BindUTF8StringParameter(0, subdomainsDBKey);
  NS_ENSURE_SUCCESS(rv, rv);

  return mRemoveOwnerStatement->Execute();
}


nsresult
nsDOMStoragePersistentDB::RemoveOwners(const nsTArray<nsString> &aOwners,
                                       PRBool aIncludeSubDomains,
                                       PRBool aMatch)
{
  if (aOwners.Length() == 0) {
    if (aMatch) {
      return NS_OK;
    }

    return RemoveAll();
  }

  // Using nsString here because it is going to be very long
  nsCString expression;

  if (aMatch) {
    expression.AppendLiteral("DELETE FROM webappsstore2 WHERE scope IN (");
  } else {
    expression.AppendLiteral("DELETE FROM webappsstore2 WHERE scope NOT IN (");
  }

  for (PRUint32 i = 0; i < aOwners.Length(); i++) {
    if (i)
      expression.AppendLiteral(" UNION ");

    expression.AppendLiteral(
      "SELECT DISTINCT scope FROM webappsstore2 WHERE scope GLOB ?");
  }
  expression.AppendLiteral(");");

  nsCOMPtr<mozIStorageStatement> statement;

  nsresult rv = mConnection->CreateStatement(expression,
                                             getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 i = 0; i < aOwners.Length(); i++) {
    nsCAutoString quotaKey;
    rv = nsDOMStorageDBWrapper::CreateDomainScopeDBKey(
      NS_ConvertUTF16toUTF8(aOwners[i]), quotaKey);

    if (!aIncludeSubDomains)
      quotaKey.AppendLiteral(":");
    quotaKey.AppendLiteral("*");

    rv = statement->BindUTF8StringParameter(i, quotaKey);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsDOMStoragePersistentDB::RemoveAll()
{
  mozStorageStatementScoper scope(mRemoveAllStatement);
  return mRemoveAllStatement->Execute();
}

nsresult
nsDOMStoragePersistentDB::GetUsage(nsDOMStorage* aStorage,
                                   PRBool aExcludeOfflineFromUsage,
                                   PRInt32 *aUsage)
{
  return GetUsageInternal(aStorage->GetQuotaDomainDBKey(!aExcludeOfflineFromUsage),
                                                        aExcludeOfflineFromUsage,
                                                        aUsage);
}

nsresult
nsDOMStoragePersistentDB::GetUsage(const nsACString& aDomain,
                                   PRBool aIncludeSubDomains,
                                   PRInt32 *aUsage)
{
  nsresult rv;

  nsCAutoString quotadomainDBKey;
  rv = nsDOMStorageDBWrapper::CreateQuotaDomainDBKey(aDomain,
                                                     aIncludeSubDomains,
                                                     PR_FALSE,
                                                     quotadomainDBKey);
  NS_ENSURE_SUCCESS(rv, rv);

  return GetUsageInternal(quotadomainDBKey, PR_FALSE, aUsage);
}

nsresult
nsDOMStoragePersistentDB::GetUsageInternal(const nsACString& aQuotaDomainDBKey,
                                           PRBool aExcludeOfflineFromUsage,
                                           PRInt32 *aUsage)
{
  if (aQuotaDomainDBKey == mCachedOwner) {
    *aUsage = mCachedUsage;
    return NS_OK;
  }

  mozIStorageStatement* statement = aExcludeOfflineFromUsage
    ? mGetOfflineExcludedUsageStatement : mGetFullUsageStatement;

  mozStorageStatementScoper scope(statement);

  nsresult rv;

  nsCAutoString scopeValue(aQuotaDomainDBKey);
  scopeValue += NS_LITERAL_CSTRING("*");

  rv = statement->BindUTF8StringParameter(0, scopeValue);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool exists;
  rv = statement->ExecuteStep(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!exists) {
    *aUsage = 0;
    return NS_OK;
  }

  rv = statement->GetInt32(0, aUsage);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aQuotaDomainDBKey.IsEmpty()) {
    mCachedOwner = aQuotaDomainDBKey;
    mCachedUsage = *aUsage;
  }

  return NS_OK;
}
