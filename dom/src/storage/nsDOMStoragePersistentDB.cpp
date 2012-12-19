/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"

#include "nsCOMPtr.h"
#include "nsError.h"
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
#include "mozIStorageBindingParamsArray.h"
#include "mozIStorageBindingParams.h"
#include "mozIStorageValueArray.h"
#include "mozIStorageFunction.h"
#include "nsNetUtil.h"
#include "mozilla/Telemetry.h"

#include "sampler.h"

using namespace mozilla;

// Temporary tables for a storage scope will be flushed if found older
// then this time in seconds since the load
#define TEMP_TABLE_MAX_AGE (10) // seconds

class nsReverseStringSQLFunction MOZ_FINAL : public mozIStorageFunction
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

  nsAutoCString stringToReverse;
  rv = aFunctionArguments->GetUTF8String(0, stringToReverse);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString result;
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

nsDOMStoragePersistentDB::nsDOMStoragePersistentDB()
: mStatements(mConnection)
{
  mTempTableLoads.Init(16);
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
  rv = service->OpenUnsharedDatabase(storageFile, getter_AddRefs(mConnection));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    // delete the db and try opening again
    rv = storageFile->Remove(false);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = service->OpenUnsharedDatabase(storageFile, getter_AddRefs(mConnection));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        MOZ_STORAGE_UNIQUIFY_QUERY_STR "PRAGMA temp_store = MEMORY"));
  NS_ENSURE_SUCCESS(rv, rv);

  mozStorageTransaction transaction(mConnection, false);

  // Ensure Gecko 1.9.1 storage table
  rv = mConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
         "CREATE TABLE IF NOT EXISTS webappsstore2 ("
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

  rv = mConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
         "CREATE TEMPORARY TABLE webappsstore2_temp ("
         "scope TEXT, "
         "key TEXT, "
         "value TEXT, "
         "secure INTEGER, "
         "owner TEXT, "
         "modified INTEGER DEFAULT 0)"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "CREATE UNIQUE INDEX scope_key_index_temp"
        " ON webappsstore2_temp(scope, key)"));
  NS_ENSURE_SUCCESS(rv, rv);


  rv = mConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "CREATE TEMPORARY VIEW webappsstore2_view AS "
        "SELECT scope, key, value, secure, owner FROM webappsstore2_temp "
        "UNION ALL "
        "SELECT scope, key, value, secure, owner FROM webappsstore2 "
          "WHERE NOT EXISTS ("
            "SELECT scope, key FROM webappsstore2_temp "
            "WHERE scope = webappsstore2.scope AND key = webappsstore2.key)"));
  NS_ENSURE_SUCCESS(rv, rv);

  // carry deletion to both the temporary table and the disk table
  rv = mConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "CREATE TEMPORARY TRIGGER webappsstore2_view_delete_trigger "
        "INSTEAD OF DELETE ON webappsstore2_view "
        "BEGIN "
          "DELETE FROM webappsstore2_temp "
          "WHERE scope = OLD.scope AND key = OLD.key; "
          "DELETE FROM webappsstore2 "
          "WHERE scope = OLD.scope AND key = OLD.key; "
        "END"));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageFunction> function1(new nsReverseStringSQLFunction());
  NS_ENSURE_TRUE(function1, NS_ERROR_OUT_OF_MEMORY);

  rv = mConnection->CreateFunction(NS_LITERAL_CSTRING("REVERSESTRING"), 1, function1);
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists;

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

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
nsDOMStoragePersistentDB::Close()
{
  // Finalize the cached statements.
  mStatements.FinalizeStatements();

  DebugOnly<nsresult> rv = mConnection->Close();
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

nsresult
nsDOMStoragePersistentDB::EnsureLoadTemporaryTableForStorage(DOMStorageImpl* aStorage)
{
  TimeStamp timeStamp;

  if (!mTempTableLoads.Get(aStorage->GetScopeDBKey(), &timeStamp)) {
    nsresult rv;

    Telemetry::AutoTimer<Telemetry::LOCALDOMSTORAGE_FETCH_DOMAIN_MS> timer;

    rv = MaybeCommitInsertTransaction();
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<mozIStorageStatement> stmt = mStatements.GetCachedStatement(
      "INSERT INTO webappsstore2_temp (scope, key, value, secure, owner) "
        "SELECT scope, key, value, secure, owner FROM webappsstore2 "
        "WHERE scope = :scope "
          "AND NOT EXISTS ( "
            "SELECT scope, key FROM webappsstore2_temp "
            "WHERE scope = webappsstore2.scope AND key = webappsstore2.key "
          ") "
    );
    NS_ENSURE_STATE(stmt);
    mozStorageStatementScoper scope(stmt);

    rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("scope"), 
                                    aStorage->GetScopeDBKey());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    mTempTableLoads.Put(aStorage->GetScopeDBKey(), TimeStamp::Now());

    DOMStorageImpl::gStorageDB->EnsureTempTableFlushTimer();
  }

  return NS_OK;
}

/* static */
PLDHashOperator
nsDOMStoragePersistentDB::FlushTemporaryTable(nsCStringHashKey::KeyType aKey,
                                              TimeStamp& aData,
                                              void* aUserArg)
{
  FlushTemporaryTableData* data = (FlushTemporaryTableData*)aUserArg;

  if (!data->mForce && 
      ((TimeStamp::Now() - aData).ToSeconds() < TEMP_TABLE_MAX_AGE))
    return PL_DHASH_NEXT;

  {
    nsCOMPtr<mozIStorageStatement> stmt =
      data->mDB->mStatements.GetCachedStatement(
        "SELECT SUM(modified) * 100.0 / COUNT(*) FROM webappsstore2_temp "
        "WHERE scope = :scope"
      );
    mozStorageStatementScoper scope(stmt);

    nsresult rv;
    bool exists;
    double percentFlushed;
    if (stmt) {
      rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("scope"), aKey);
      if (NS_SUCCEEDED(rv)) {
        rv = stmt->ExecuteStep(&exists);
        if (NS_SUCCEEDED(rv) && exists) {
          rv = stmt->GetDouble(0, &percentFlushed);
          if (NS_SUCCEEDED(rv) && percentFlushed > 0.0) {
            Telemetry::Accumulate(Telemetry::LOCALDOMSTORAGE_PERCENT_FLUSHED,
                                  uint32_t(percentFlushed));
          }
        }
      }
    }
  }

  {
    nsCOMPtr<mozIStorageStatement> stmt =
      data->mDB->mStatements.GetCachedStatement(
        "INSERT OR REPLACE INTO webappsstore2 "
         "SELECT scope, key, value, secure, owner FROM webappsstore2_temp "
         "WHERE scope = :scope AND modified = 1"
      );
    NS_ENSURE_TRUE(stmt, PL_DHASH_STOP);
    mozStorageStatementScoper scope(stmt);

    data->mRV = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("scope"), aKey);
    NS_ENSURE_SUCCESS(data->mRV, PL_DHASH_STOP);

    data->mRV = stmt->Execute();
    NS_ENSURE_SUCCESS(data->mRV, PL_DHASH_STOP);
  }

  {
    nsCOMPtr<mozIStorageStatement> stmt =
      data->mDB->mStatements.GetCachedStatement(
        "DELETE FROM webappsstore2_temp "
         "WHERE scope = :scope "
      );
    NS_ENSURE_TRUE(stmt, PL_DHASH_STOP);
    mozStorageStatementScoper scope(stmt);

    data->mRV = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("scope"), aKey);
    NS_ENSURE_SUCCESS(data->mRV, PL_DHASH_STOP);

    data->mRV = stmt->Execute();
    NS_ENSURE_SUCCESS(data->mRV, PL_DHASH_STOP);
  }

  return PL_DHASH_REMOVE;
}

nsresult
nsDOMStoragePersistentDB::FlushTemporaryTables(bool force)
{
  nsCOMPtr<mozIStorageStatement> stmt =
    mStatements.GetCachedStatement(
      "SELECT COUNT(*) FROM webappsstore2_temp WHERE modified = 1"
    );
  mozStorageStatementScoper scope(stmt);

  TimeStamp startTime;
  bool exists;
  int32_t dirtyCount;
  if (stmt &&
      NS_SUCCEEDED(stmt->ExecuteStep(&exists)) && exists &&
      NS_SUCCEEDED(stmt->GetInt32(0, &dirtyCount)) && dirtyCount > 0) {
    // Time the operation if dirty entries will be flushed
    startTime = TimeStamp::Now();
  }

  mozStorageTransaction trans(mConnection, false);
  nsresult rv;

  FlushTemporaryTableData data;
  data.mDB = this;
  data.mForce = force;
  data.mRV = NS_OK;

  mTempTableLoads.Enumerate(FlushTemporaryTable, &data);
  NS_ENSURE_SUCCESS(data.mRV, data.mRV);

  rv = trans.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = MaybeCommitInsertTransaction();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!startTime.IsNull()) {
    Telemetry::AutoTimer<Telemetry::LOCALDOMSTORAGE_TIMER_FLUSH_MS> timer(startTime);
  }

  return NS_OK;
}

nsresult
nsDOMStoragePersistentDB::GetAllKeys(DOMStorageImpl* aStorage,
                                     nsTHashtable<nsSessionStorageEntry>* aKeys)
{
  Telemetry::AutoTimer<Telemetry::LOCALDOMSTORAGE_GETALLKEYS_MS> timer;
  nsresult rv;

  rv = MaybeCommitInsertTransaction();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = EnsureLoadTemporaryTableForStorage(aStorage);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageStatement> stmt = mStatements.GetCachedStatement(
    "SELECT key, value, secure FROM webappsstore2_temp "
    "WHERE scope = :scope "
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scope(stmt);

  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("scope"),
                                  aStorage->GetScopeDBKey());
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists;
  while (NS_SUCCEEDED(rv = stmt->ExecuteStep(&exists)) && exists) {
    nsAutoString key;
    rv = stmt->GetString(0, key);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString value;
    rv = stmt->GetString(1, value);
    NS_ENSURE_SUCCESS(rv, rv);

    int32_t secureInt = 0;
    rv = stmt->GetInt32(2, &secureInt);
    NS_ENSURE_SUCCESS(rv, rv);

    nsSessionStorageEntry* entry = aKeys->PutEntry(key);
    NS_ENSURE_TRUE(entry, NS_ERROR_OUT_OF_MEMORY);

    entry->mItem = new nsDOMStorageItem(aStorage, key, value, secureInt);
    if (!entry->mItem) {
      aKeys->RawRemoveEntry(entry);
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return NS_OK;
}

nsresult
nsDOMStoragePersistentDB::GetKeyValue(DOMStorageImpl* aStorage,
                                      const nsAString& aKey,
                                      nsAString& aValue,
                                      bool* aSecure)
{
  Telemetry::AutoTimer<Telemetry::LOCALDOMSTORAGE_GETVALUE_MS> timer;
  SAMPLE_LABEL("nsDOMStoragePersistentDB", "GetKeyValue");
  nsresult rv;

  rv = MaybeCommitInsertTransaction();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = EnsureLoadTemporaryTableForStorage(aStorage);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageStatement> stmt = mStatements.GetCachedStatement(
    "SELECT value, secure FROM webappsstore2_temp "
    "WHERE scope = :scope "
      "AND key = :key "
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scope(stmt);

  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("scope"),
                                  aStorage->GetScopeDBKey());
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("key"),
                              aKey);
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists;
  rv = stmt->ExecuteStep(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t secureInt = 0;
  if (exists) {
    rv = stmt->GetString(0, aValue);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->GetInt32(1, &secureInt);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    rv = NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  *aSecure = !!secureInt;

  return rv;
}

nsresult
nsDOMStoragePersistentDB::SetKey(DOMStorageImpl* aStorage,
                                 const nsAString& aKey,
                                 const nsAString& aValue,
                                 bool aSecure)
{
  Telemetry::AutoTimer<Telemetry::LOCALDOMSTORAGE_SETVALUE_MS> timer;

  nsresult rv;

  rv = EnsureLoadTemporaryTableForStorage(aStorage);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t usage = 0;
  if (!aStorage->GetQuotaDBKey().IsEmpty()) {
    rv = GetUsage(aStorage, &usage);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  usage += aKey.Length() + aValue.Length();

  nsAutoString previousValue;
  bool secure;
  rv = aStorage->GetCachedValue(aKey, previousValue, &secure);
  if (NS_SUCCEEDED(rv)) {
    if (!aSecure && secure)
      return NS_ERROR_DOM_SECURITY_ERR;
    usage -= aKey.Length() + previousValue.Length();
  }

  if (usage > GetQuota()) {
    return NS_ERROR_DOM_QUOTA_REACHED;
  }

  rv = EnsureInsertTransaction();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageStatement> stmt = mStatements.GetCachedStatement(
    "INSERT OR REPLACE INTO webappsstore2_temp (scope, key, value, secure, modified) "
    "VALUES (:scope, :key, :value, :secure, 1) "
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scopeinsert(stmt);

  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("scope"),
                                  aStorage->GetScopeDBKey());
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("key"),
                              aKey);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("value"),
                              aValue);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("secure"),
                             aSecure ? 1 : 0);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aStorage->GetQuotaDBKey().IsEmpty()) {
    // No need to set mCachedOwner since it was set by GetUsage()
    mCachedUsage = usage;
  }

  MarkScopeDirty(aStorage);

  return NS_OK;
}

nsresult
nsDOMStoragePersistentDB::SetSecure(DOMStorageImpl* aStorage,
                                    const nsAString& aKey,
                                    const bool aSecure)
{
  nsresult rv;

  rv = EnsureLoadTemporaryTableForStorage(aStorage);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = EnsureInsertTransaction();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageStatement> stmt = mStatements.GetCachedStatement(
    "UPDATE webappsstore2_temp "
    "SET secure = :secure, modified = 1 "
    "WHERE scope = :scope "
      "AND key = :key "
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scope(stmt);

  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("scope"),
                                  aStorage->GetScopeDBKey());
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("key"),
                              aKey);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("secure"),
                             aSecure ? 1 : 0);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  MarkScopeDirty(aStorage);

  return NS_OK;
}

nsresult
nsDOMStoragePersistentDB::RemoveKey(DOMStorageImpl* aStorage,
                                    const nsAString& aKey)
{
  Telemetry::AutoTimer<Telemetry::LOCALDOMSTORAGE_REMOVEKEY_MS> timer;
  nsresult rv;

  rv = MaybeCommitInsertTransaction();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageStatement> stmt = mStatements.GetCachedStatement(
    "DELETE FROM webappsstore2_view "
    "WHERE scope = :scope "
      "AND key = :key "
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scope(stmt);

  if (DomainMaybeCached(
      aStorage->GetQuotaDBKey())) {
    mCachedUsage = 0;
    mCachedOwner.Truncate();
  }

  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("scope"),
                                  aStorage->GetScopeDBKey());
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("key"),
                              aKey);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  MarkScopeDirty(aStorage);

  return NS_OK;
}

nsresult
nsDOMStoragePersistentDB::ClearStorage(DOMStorageImpl* aStorage)
{
  nsresult rv;

  rv = MaybeCommitInsertTransaction();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageStatement> stmt = mStatements.GetCachedStatement(
    "DELETE FROM webappsstore2_view "
    "WHERE scope = :scope "
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scope(stmt);

  mCachedUsage = 0;
  mCachedOwner.Truncate();

  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("scope"),
                                  aStorage->GetScopeDBKey());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  MarkScopeDirty(aStorage);

  return NS_OK;
}

nsresult
nsDOMStoragePersistentDB::RemoveOwner(const nsACString& aOwner)
{
  nsresult rv;

  rv = MaybeCommitInsertTransaction();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageStatement> stmt = mStatements.GetCachedStatement(
    "DELETE FROM webappsstore2_view "
    "WHERE scope GLOB :scope "
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scope(stmt);

  nsAutoCString subdomainsDBKey;
  nsDOMStorageDBWrapper::CreateReversedDomain(aOwner, subdomainsDBKey);

  if (DomainMaybeCached(subdomainsDBKey)) {
    mCachedUsage = 0;
    mCachedOwner.Truncate();
  }

  subdomainsDBKey.AppendLiteral("*");

  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("scope"),
                                  subdomainsDBKey);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  MarkAllScopesDirty();

  return NS_OK;
}

nsresult
nsDOMStoragePersistentDB::RemoveAll()
{
  Telemetry::AutoTimer<Telemetry::LOCALDOMSTORAGE_REMOVEALL_MS> timer;
  nsresult rv;

  rv = MaybeCommitInsertTransaction();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageStatement> stmt = mStatements.GetCachedStatement(
    "DELETE FROM webappsstore2_view "
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scope(stmt);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  MarkAllScopesDirty();

  return NS_OK;
}

nsresult
nsDOMStoragePersistentDB::RemoveAllForApp(uint32_t aAppId, bool aOnlyBrowserElement)
{
  nsresult rv;

  rv = MaybeCommitInsertTransaction();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageStatement> stmt = mStatements.GetCachedStatement(
    "DELETE FROM webappsstore2_view "
    "WHERE scope LIKE :scope"
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scopeStmt(stmt);

  nsAutoCString scope;
  scope.AppendInt(aAppId);
  if (aOnlyBrowserElement) {
    scope.Append(NS_LITERAL_CSTRING(":t:%"));
  } else {
    scope.Append(NS_LITERAL_CSTRING(":_:%"));
  }
  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("scope"), scope);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  MarkAllScopesDirty();

  return NS_OK;
}

nsresult
nsDOMStoragePersistentDB::GetUsage(DOMStorageImpl* aStorage,
                                   int32_t *aUsage)
{
  return GetUsageInternal(aStorage->GetQuotaDBKey(), aUsage);
}

nsresult
nsDOMStoragePersistentDB::GetUsage(const nsACString& aDomain,
                                   int32_t *aUsage)
{
  nsresult rv;

  nsAutoCString quotaDBKey;
  rv = nsDOMStorageDBWrapper::CreateQuotaDBKey(aDomain, quotaDBKey);
  NS_ENSURE_SUCCESS(rv, rv);

  return GetUsageInternal(quotaDBKey, aUsage);
}

nsresult
nsDOMStoragePersistentDB::GetUsageInternal(const nsACString& aQuotaDBKey,
                                           int32_t *aUsage)
{
  if (aQuotaDBKey == mCachedOwner) {
    *aUsage = mCachedUsage;
    return NS_OK;
  }

  Telemetry::AutoTimer<Telemetry::LOCALDOMSTORAGE_FETCH_QUOTA_USE_MS> timer;

  nsresult rv;

  rv = MaybeCommitInsertTransaction();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageStatement> stmt;
  stmt = mStatements.GetCachedStatement(
    "SELECT SUM(LENGTH(key) + LENGTH(value)) "
    "FROM ( "
      "SELECT key,value FROM webappsstore2_temp "
      "WHERE scope GLOB :scope "
      "UNION ALL "
      "SELECT key,value FROM webappsstore2 "
      "WHERE scope GLOB :scope "
        "AND NOT EXISTS ( "
          "SELECT scope, key "
          "FROM webappsstore2_temp "
          "WHERE scope = webappsstore2.scope "
            "AND key = webappsstore2.key "
        ") "
    ") "
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scope(stmt);

  nsAutoCString scopeValue(aQuotaDBKey);
  scopeValue += NS_LITERAL_CSTRING("*");

  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("scope"), scopeValue);
  NS_ENSURE_SUCCESS(rv, rv);

  bool exists;
  rv = stmt->ExecuteStep(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!exists) {
    *aUsage = 0;
    return NS_OK;
  }

  rv = stmt->GetInt32(0, aUsage);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aQuotaDBKey.IsEmpty()) {
    mCachedOwner = aQuotaDBKey;
    mCachedUsage = *aUsage;
  }

  return NS_OK;
}

nsresult
nsDOMStoragePersistentDB::EnsureInsertTransaction()
{
  if (!mConnection)
    return NS_ERROR_UNEXPECTED;

  bool transactionInProgress;
  nsresult rv = mConnection->GetTransactionInProgress(&transactionInProgress);
  NS_ENSURE_SUCCESS(rv, rv);

  if (transactionInProgress)
    return NS_OK;

  rv = mConnection->BeginTransaction();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsDOMStoragePersistentDB::MaybeCommitInsertTransaction()
{
  if (!mConnection)
    return NS_ERROR_UNEXPECTED;

  bool transactionInProgress;
  nsresult rv = mConnection->GetTransactionInProgress(&transactionInProgress);
  if (NS_FAILED(rv)) {
    NS_WARNING("nsDOMStoragePersistentDB::MaybeCommitInsertTransaction: "
               "connection probably already dead");
  }

  if (NS_FAILED(rv) || !transactionInProgress)
    return NS_OK;

  rv = mConnection->CommitTransaction();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

bool
nsDOMStoragePersistentDB::DomainMaybeCached(const nsACString& aDomain)
{
  if (mCachedOwner.IsEmpty())
    return false;

  // if cached owner contains colon we must ignore it
  if (mCachedOwner[mCachedOwner.Length() - 1] == ':')
    return StringBeginsWith(aDomain, Substring(mCachedOwner, 0,
                                               mCachedOwner.Length() - 1));
  else
    return StringBeginsWith(aDomain, mCachedOwner);
}
