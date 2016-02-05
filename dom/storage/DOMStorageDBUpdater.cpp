/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMStorageManager.h"

#include "mozIStorageBindingParamsArray.h"
#include "mozIStorageBindingParams.h"
#include "mozIStorageValueArray.h"
#include "mozIStorageFunction.h"
#include "mozilla/BasePrincipal.h"
#include "nsVariant.h"
#include "mozilla/Services.h"
#include "mozilla/Tokenizer.h"

// Current version of the database schema
#define CURRENT_SCHEMA_VERSION 1

namespace mozilla {
namespace dom {

extern void
ReverseString(const nsCSubstring& aSource, nsCSubstring& aResult);

namespace {

class nsReverseStringSQLFunction final : public mozIStorageFunction
{
  ~nsReverseStringSQLFunction() {}

  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION
};

NS_IMPL_ISUPPORTS(nsReverseStringSQLFunction, mozIStorageFunction)

NS_IMETHODIMP
nsReverseStringSQLFunction::OnFunctionCall(
    mozIStorageValueArray* aFunctionArguments, nsIVariant** aResult)
{
  nsresult rv;

  nsAutoCString stringToReverse;
  rv = aFunctionArguments->GetUTF8String(0, stringToReverse);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString result;
  ReverseString(stringToReverse, result);

  RefPtr<nsVariant> outVar(new nsVariant());
  rv = outVar->SetAsAUTF8String(result);
  NS_ENSURE_SUCCESS(rv, rv);

  outVar.forget(aResult);
  return NS_OK;
}

// "scope" to "origin attributes suffix" and "origin key" convertor

class ExtractOriginData : protected mozilla::Tokenizer
{
public:
  ExtractOriginData(const nsACString& scope, nsACString& suffix, nsACString& origin)
    : mozilla::Tokenizer(scope)
  {
    using mozilla::OriginAttributes;

    // Parse optional appId:isInIsolatedMozBrowserElement: string, in case
    // we don't find it, the scope is our new origin key and suffix
    // is empty.
    suffix.Truncate();
    origin.Assign(scope);

    // Bail out if it isn't appId.
    uint32_t appId;
    if (!ReadInteger(&appId)) {
      return;
    }

    // Should be followed by a colon.
    if (!CheckChar(':')) {
      return;
    }

    // Bail out if it isn't 'isolatedBrowserFlag'.
    nsDependentCSubstring isolatedBrowserFlag;
    if (!ReadWord(isolatedBrowserFlag)) {
      return;
    }

    bool inIsolatedMozBrowser = isolatedBrowserFlag == "t";
    bool notInIsolatedBrowser = isolatedBrowserFlag == "f";
    if (!inIsolatedMozBrowser && !notInIsolatedBrowser) {
      return;
    }

    // Should be followed by a colon.
    if (!CheckChar(':')) {
      return;
    }

    // OK, we have found appId and inIsolatedMozBrowser flag, create the suffix
    // from it and take the rest as the origin key.

    // If the profile went through schema 1 -> schema 0 -> schema 1 switching
    // we may have stored the full attributes origin suffix when there were
    // more than just appId and inIsolatedMozBrowser set on storage principal's
    // OriginAttributes.
    //
    // To preserve full uniqueness we store this suffix to the scope key.
    // Schema 0 code will just ignore it while keeping the scoping unique.
    //
    // The whole scope string is in one of the following forms (when we are here):
    //
    // "1001:f:^appId=1001&inBrowser=false&addonId=101:gro.allizom.rxd.:https:443"
    // "1001:f:gro.allizom.rxd.:https:443"
    //         |
    //         +- the parser cursor position.
    //
    // If there is '^', the full origin attributes suffix follows.  We search
    // for ':' since it is the delimiter used in the scope string and is never
    // contained in the origin attributes suffix.  Remaining string after
    // the comma is the reversed-domain+schema+port tuple.
    Record();
    if (CheckChar('^')) {
      Token t;
      while (Next(t)) {
        if (t.Equals(Token::Char(':'))) {
          Claim(suffix);
          break;
        }
      }
    } else {
      PrincipalOriginAttributes attrs(appId, inIsolatedMozBrowser);
      attrs.CreateSuffix(suffix);
    }

    // Consume the rest of the input as "origin".
    origin.Assign(Substring(mCursor, mEnd));
  }
};

class GetOriginParticular final : public mozIStorageFunction
{
public:
  enum EParticular {
    ORIGIN_ATTRIBUTES_SUFFIX,
    ORIGIN_KEY
  };

  explicit GetOriginParticular(EParticular aParticular)
    : mParticular(aParticular) {}

private:
  GetOriginParticular() = delete;
  ~GetOriginParticular() {}

  EParticular mParticular;

  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION
};

NS_IMPL_ISUPPORTS(GetOriginParticular, mozIStorageFunction)

NS_IMETHODIMP
GetOriginParticular::OnFunctionCall(
    mozIStorageValueArray* aFunctionArguments, nsIVariant** aResult)
{
  nsresult rv;

  nsAutoCString scope;
  rv = aFunctionArguments->GetUTF8String(0, scope);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString suffix, origin;
  ExtractOriginData(scope, suffix, origin);

  nsCOMPtr<nsIWritableVariant> outVar(new nsVariant());

  switch (mParticular) {
  case EParticular::ORIGIN_ATTRIBUTES_SUFFIX:
    rv = outVar->SetAsAUTF8String(suffix);
    break;
  case EParticular::ORIGIN_KEY:
    rv = outVar->SetAsAUTF8String(origin);
    break;
  }

  NS_ENSURE_SUCCESS(rv, rv);

  outVar.forget(aResult);
  return NS_OK;
}

} // namespace

namespace DOMStorageDBUpdater {

nsresult CreateSchema1Tables(mozIStorageConnection *aWorkerConnection)
{
  nsresult rv;

  rv = aWorkerConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "CREATE TABLE IF NOT EXISTS webappsstore2 ("
          "originAttributes TEXT, "
          "originKey TEXT, "
          "scope TEXT, " // Only for schema0 downgrade compatibility
          "key TEXT, "
          "value TEXT)"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aWorkerConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "CREATE UNIQUE INDEX IF NOT EXISTS origin_key_index"
        " ON webappsstore2(originAttributes, originKey, key)"));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult Update(mozIStorageConnection *aWorkerConnection)
{
  nsresult rv;

  mozStorageTransaction transaction(aWorkerConnection, false);

  bool doVacuum = false;

  int32_t schemaVer;
  rv = aWorkerConnection->GetSchemaVersion(&schemaVer);
  NS_ENSURE_SUCCESS(rv, rv);

  // downgrade (v0) -> upgrade (v1+) specific code
  if (schemaVer >= 1) {
    bool schema0IndexExists;
    rv = aWorkerConnection->IndexExists(NS_LITERAL_CSTRING("scope_key_index"),
                                        &schema0IndexExists);
    NS_ENSURE_SUCCESS(rv, rv);

    if (schema0IndexExists) {
      // If this index exists, the database (already updated to schema >1)
      // has been run again on schema 0 code.  That recreated that index
      // and might store some new rows while updating only the 'scope' column.
      // For such added rows we must fill the new 'origin*' columns correctly
      // otherwise there would be a data loss.  The safest way to do it is to
      // simply run the whole update to schema 1 again.
      schemaVer = 0;
    }
  }

  switch (schemaVer) {
  case 0: {
    bool webappsstore2Exists, webappsstoreExists, moz_webappsstoreExists;

    rv = aWorkerConnection->TableExists(NS_LITERAL_CSTRING("webappsstore2"),
                                        &webappsstore2Exists);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aWorkerConnection->TableExists(NS_LITERAL_CSTRING("webappsstore"),
                                        &webappsstoreExists);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aWorkerConnection->TableExists(NS_LITERAL_CSTRING("moz_webappsstore"),
                                        &moz_webappsstoreExists);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!webappsstore2Exists && !webappsstoreExists && !moz_webappsstoreExists) {
      // The database is empty, this is the first start.  Just create the schema table
      // and break to the next version to update to, i.e. bypass update from the old version.

      rv = CreateSchema1Tables(aWorkerConnection);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aWorkerConnection->SetSchemaVersion(CURRENT_SCHEMA_VERSION);
      NS_ENSURE_SUCCESS(rv, rv);

      break;
    }

    doVacuum = true;

    // Ensure Gecko 1.9.1 storage table
    rv = aWorkerConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
           "CREATE TABLE IF NOT EXISTS webappsstore2 ("
           "scope TEXT, "
           "key TEXT, "
           "value TEXT, "
           "secure INTEGER, "
           "owner TEXT)"));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aWorkerConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "CREATE UNIQUE INDEX IF NOT EXISTS scope_key_index"
          " ON webappsstore2(scope, key)"));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<mozIStorageFunction> function1(new nsReverseStringSQLFunction());
    NS_ENSURE_TRUE(function1, NS_ERROR_OUT_OF_MEMORY);

    rv = aWorkerConnection->CreateFunction(NS_LITERAL_CSTRING("REVERSESTRING"), 1, function1);
    NS_ENSURE_SUCCESS(rv, rv);

    // Check if there is storage of Gecko 1.9.0 and if so, upgrade that storage
    // to actual webappsstore2 table and drop the obsolete table. First process
    // this newer table upgrade to priority potential duplicates from older
    // storage table.
    if (webappsstoreExists) {
      rv = aWorkerConnection->ExecuteSimpleSQL(
        NS_LITERAL_CSTRING("INSERT OR IGNORE INTO "
                           "webappsstore2(scope, key, value, secure, owner) "
                           "SELECT REVERSESTRING(domain) || '.:', key, value, secure, owner "
                           "FROM webappsstore"));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aWorkerConnection->ExecuteSimpleSQL(
        NS_LITERAL_CSTRING("DROP TABLE webappsstore"));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Check if there is storage of Gecko 1.8 and if so, upgrade that storage
    // to actual webappsstore2 table and drop the obsolete table. Potential
    // duplicates will be ignored.
    if (moz_webappsstoreExists) {
      rv = aWorkerConnection->ExecuteSimpleSQL(
        NS_LITERAL_CSTRING("INSERT OR IGNORE INTO "
                           "webappsstore2(scope, key, value, secure, owner) "
                           "SELECT REVERSESTRING(domain) || '.:', key, value, secure, domain "
                           "FROM moz_webappsstore"));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aWorkerConnection->ExecuteSimpleSQL(
        NS_LITERAL_CSTRING("DROP TABLE moz_webappsstore"));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    aWorkerConnection->RemoveFunction(NS_LITERAL_CSTRING("REVERSESTRING"));

    // Update the scoping to match the new implememntation: split to oa suffix and origin key
    // First rename the old table, we want to remove some columns no longer needed,
    // but even before that drop all indexes from it (CREATE IF NOT EXISTS for index on the
    // new table would falsely find the index!)
    rv = aWorkerConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "DROP INDEX IF EXISTS webappsstore2.origin_key_index"));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aWorkerConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "DROP INDEX IF EXISTS webappsstore2.scope_key_index"));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aWorkerConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "ALTER TABLE webappsstore2 RENAME TO webappsstore2_old"));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<mozIStorageFunction> oaSuffixFunc(
      new GetOriginParticular(GetOriginParticular::ORIGIN_ATTRIBUTES_SUFFIX));
    rv = aWorkerConnection->CreateFunction(NS_LITERAL_CSTRING("GET_ORIGIN_SUFFIX"), 1, oaSuffixFunc);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<mozIStorageFunction> originKeyFunc(
      new GetOriginParticular(GetOriginParticular::ORIGIN_KEY));
    rv = aWorkerConnection->CreateFunction(NS_LITERAL_CSTRING("GET_ORIGIN_KEY"), 1, originKeyFunc);
    NS_ENSURE_SUCCESS(rv, rv);

    // Here we ensure this schema tables when we are updating.
    rv = CreateSchema1Tables(aWorkerConnection);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aWorkerConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
          "INSERT OR IGNORE INTO "
          "webappsstore2 (originAttributes, originKey, scope, key, value) "
          "SELECT GET_ORIGIN_SUFFIX(scope), GET_ORIGIN_KEY(scope), scope, key, value "
          "FROM webappsstore2_old"));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aWorkerConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("DROP TABLE webappsstore2_old"));
    NS_ENSURE_SUCCESS(rv, rv);

    aWorkerConnection->RemoveFunction(NS_LITERAL_CSTRING("GET_ORIGIN_SUFFIX"));
    aWorkerConnection->RemoveFunction(NS_LITERAL_CSTRING("GET_ORIGIN_KEY"));

    rv = aWorkerConnection->SetSchemaVersion(1);
    NS_ENSURE_SUCCESS(rv, rv);

    MOZ_FALLTHROUGH;
  }
  case CURRENT_SCHEMA_VERSION:
    // Ensure the tables and indexes are up.  This is mostly a no-op
    // in common scenarios.
    rv = CreateSchema1Tables(aWorkerConnection);
    NS_ENSURE_SUCCESS(rv, rv);

    // Nothing more to do here, this is the current schema version
    break;

  default:
    MOZ_ASSERT(false);
    break;
  } // switch

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  if (doVacuum) {
    // In some cases this can make the disk file of the database significantly smaller.
    // VACUUM cannot be executed inside a transaction.
    rv = aWorkerConnection->ExecuteSimpleSQL(
      NS_LITERAL_CSTRING("VACUUM"));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

} // namespace DOMStorageDBUpdater
} // namespace dom
} // namespace mozilla
