/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LocalStorageManager.h"
#include "StorageUtils.h"

#include "mozIStorageBindingParams.h"
#include "mozIStorageValueArray.h"
#include "mozIStorageFunction.h"
#include "mozilla/BasePrincipal.h"
#include "nsVariant.h"
#include "mozilla/Tokenizer.h"
#include "mozIStorageConnection.h"
#include "mozStorageHelper.h"

// Current version of the database schema
#define CURRENT_SCHEMA_VERSION 2

namespace mozilla {
namespace dom {

using namespace StorageUtils;

namespace {

class nsReverseStringSQLFunction final : public mozIStorageFunction {
  ~nsReverseStringSQLFunction() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION
};

NS_IMPL_ISUPPORTS(nsReverseStringSQLFunction, mozIStorageFunction)

NS_IMETHODIMP
nsReverseStringSQLFunction::OnFunctionCall(
    mozIStorageValueArray* aFunctionArguments, nsIVariant** aResult) {
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

class ExtractOriginData : protected mozilla::Tokenizer {
 public:
  ExtractOriginData(const nsACString& scope, nsACString& suffix,
                    nsACString& origin)
      : mozilla::Tokenizer(scope) {
    using mozilla::OriginAttributes;

    // Parse optional appId:isInIsolatedMozBrowserElement: string, in case
    // we don't find it, the scope is our new origin key and suffix
    // is empty.
    suffix.Truncate();
    origin.Assign(scope);

    // Bail out if it isn't appId.
    // AppId doesn't exist any more but we could have old storage data...
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
    // The whole scope string is in one of the following forms (when we are
    // here):
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
      OriginAttributes attrs(inIsolatedMozBrowser);
      attrs.CreateSuffix(suffix);
    }

    // Consume the rest of the input as "origin".
    origin.Assign(Substring(mCursor, mEnd));
  }
};

class GetOriginParticular final : public mozIStorageFunction {
 public:
  enum EParticular { ORIGIN_ATTRIBUTES_SUFFIX, ORIGIN_KEY };

  explicit GetOriginParticular(EParticular aParticular)
      : mParticular(aParticular) {}

 private:
  GetOriginParticular() = delete;
  ~GetOriginParticular() = default;

  EParticular mParticular;

  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION
};

NS_IMPL_ISUPPORTS(GetOriginParticular, mozIStorageFunction)

NS_IMETHODIMP
GetOriginParticular::OnFunctionCall(mozIStorageValueArray* aFunctionArguments,
                                    nsIVariant** aResult) {
  nsresult rv;

  nsAutoCString scope;
  rv = aFunctionArguments->GetUTF8String(0, scope);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString suffix, origin;
  ExtractOriginData extractor(scope, suffix, origin);

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

class StripOriginAddonId final : public mozIStorageFunction {
 public:
  explicit StripOriginAddonId() = default;

 private:
  ~StripOriginAddonId() = default;

  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEFUNCTION
};

NS_IMPL_ISUPPORTS(StripOriginAddonId, mozIStorageFunction)

NS_IMETHODIMP
StripOriginAddonId::OnFunctionCall(mozIStorageValueArray* aFunctionArguments,
                                   nsIVariant** aResult) {
  nsresult rv;

  nsAutoCString suffix;
  rv = aFunctionArguments->GetUTF8String(0, suffix);
  NS_ENSURE_SUCCESS(rv, rv);

  // Deserialize and re-serialize to automatically drop any obsolete origin
  // attributes.
  OriginAttributes oa;
  bool ok = oa.PopulateFromSuffix(suffix);
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

  nsAutoCString newSuffix;
  oa.CreateSuffix(newSuffix);

  nsCOMPtr<nsIWritableVariant> outVar = new nsVariant();
  rv = outVar->SetAsAUTF8String(newSuffix);
  NS_ENSURE_SUCCESS(rv, rv);

  outVar.forget(aResult);
  return NS_OK;
}

nsresult CreateSchema1Tables(mozIStorageConnection* aWorkerConnection) {
  nsresult rv;

  rv = aWorkerConnection->ExecuteSimpleSQL(nsLiteralCString(
      "CREATE TABLE IF NOT EXISTS webappsstore2 ("
      "originAttributes TEXT, "
      "originKey TEXT, "
      "scope TEXT, "  // Only for schema0 downgrade compatibility
      "key TEXT, "
      "value TEXT)"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aWorkerConnection->ExecuteSimpleSQL(
      nsLiteralCString("CREATE UNIQUE INDEX IF NOT EXISTS origin_key_index"
                       " ON webappsstore2(originAttributes, originKey, key)"));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult TablesExist(mozIStorageConnection* aWorkerConnection,
                     bool* aWebappsstore2Exists, bool* aWebappsstoreExists,
                     bool* aMoz_webappsstoreExists) {
  nsresult rv =
      aWorkerConnection->TableExists("webappsstore2"_ns, aWebappsstore2Exists);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aWorkerConnection->TableExists("webappsstore"_ns, aWebappsstoreExists);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = aWorkerConnection->TableExists("moz_webappsstore"_ns,
                                      aMoz_webappsstoreExists);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult CreateCurrentSchemaOnEmptyTableInternal(
    mozIStorageConnection* aWorkerConnection) {
  nsresult rv = CreateSchema1Tables(aWorkerConnection);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aWorkerConnection->SetSchemaVersion(CURRENT_SCHEMA_VERSION);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

}  // namespace

namespace StorageDBUpdater {

nsresult CreateCurrentSchema(mozIStorageConnection* aConnection) {
  mozStorageTransaction transaction(aConnection, false);

  nsresult rv = transaction.Start();
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  {
    int32_t schemaVer;
    nsresult rv = aConnection->GetSchemaVersion(&schemaVer);
    NS_ENSURE_SUCCESS(rv, rv);

    MOZ_DIAGNOSTIC_ASSERT(0 == schemaVer);

    bool webappsstore2Exists, webappsstoreExists, moz_webappsstoreExists;
    rv = TablesExist(aConnection, &webappsstore2Exists, &webappsstoreExists,
                     &moz_webappsstoreExists);
    NS_ENSURE_SUCCESS(rv, rv);

    MOZ_DIAGNOSTIC_ASSERT(!webappsstore2Exists && !webappsstoreExists &&
                          !moz_webappsstoreExists);
  }
#endif

  rv = CreateCurrentSchemaOnEmptyTableInternal(aConnection);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult Update(mozIStorageConnection* aWorkerConnection) {
  mozStorageTransaction transaction(aWorkerConnection, false);

  nsresult rv = transaction.Start();
  NS_ENSURE_SUCCESS(rv, rv);

  bool doVacuum = false;

  int32_t schemaVer;
  rv = aWorkerConnection->GetSchemaVersion(&schemaVer);
  NS_ENSURE_SUCCESS(rv, rv);

  // downgrade (v0) -> upgrade (v1+) specific code
  if (schemaVer >= 1) {
    bool schema0IndexExists;
    rv = aWorkerConnection->IndexExists("scope_key_index"_ns,
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
      rv = TablesExist(aWorkerConnection, &webappsstore2Exists,
                       &webappsstoreExists, &moz_webappsstoreExists);
      NS_ENSURE_SUCCESS(rv, rv);

      if (!webappsstore2Exists && !webappsstoreExists &&
          !moz_webappsstoreExists) {
        // The database is empty, this is the first start.  Just create the
        // schema table and break to the next version to update to, i.e. bypass
        // update from the old version.

        // XXX What does "break to the next version to update to" mean here? It
        // seems to refer to the 'break' statement below, but that breaks out of
        // the 'switch' statement and continues with committing the transaction.
        // Either this is wrong, or the comment above is misleading.

        rv = CreateCurrentSchemaOnEmptyTableInternal(aWorkerConnection);
        NS_ENSURE_SUCCESS(rv, rv);

        break;
      }

      doVacuum = true;

      // Ensure Gecko 1.9.1 storage table
      rv = aWorkerConnection->ExecuteSimpleSQL(
          nsLiteralCString("CREATE TABLE IF NOT EXISTS webappsstore2 ("
                           "scope TEXT, "
                           "key TEXT, "
                           "value TEXT, "
                           "secure INTEGER, "
                           "owner TEXT)"));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aWorkerConnection->ExecuteSimpleSQL(
          nsLiteralCString("CREATE UNIQUE INDEX IF NOT EXISTS scope_key_index"
                           " ON webappsstore2(scope, key)"));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<mozIStorageFunction> function1(new nsReverseStringSQLFunction());
      NS_ENSURE_TRUE(function1, NS_ERROR_OUT_OF_MEMORY);

      rv = aWorkerConnection->CreateFunction("REVERSESTRING"_ns, 1, function1);
      NS_ENSURE_SUCCESS(rv, rv);

      // Check if there is storage of Gecko 1.9.0 and if so, upgrade that
      // storage to actual webappsstore2 table and drop the obsolete table.
      // First process this newer table upgrade to priority potential duplicates
      // from older storage table.
      if (webappsstoreExists) {
        rv = aWorkerConnection->ExecuteSimpleSQL(nsLiteralCString(
            "INSERT OR IGNORE INTO "
            "webappsstore2(scope, key, value, secure, owner) "
            "SELECT REVERSESTRING(domain) || '.:', key, value, secure, owner "
            "FROM webappsstore"));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = aWorkerConnection->ExecuteSimpleSQL("DROP TABLE webappsstore"_ns);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Check if there is storage of Gecko 1.8 and if so, upgrade that storage
      // to actual webappsstore2 table and drop the obsolete table. Potential
      // duplicates will be ignored.
      if (moz_webappsstoreExists) {
        rv = aWorkerConnection->ExecuteSimpleSQL(nsLiteralCString(
            "INSERT OR IGNORE INTO "
            "webappsstore2(scope, key, value, secure, owner) "
            "SELECT REVERSESTRING(domain) || '.:', key, value, secure, domain "
            "FROM moz_webappsstore"));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = aWorkerConnection->ExecuteSimpleSQL(
            "DROP TABLE moz_webappsstore"_ns);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      aWorkerConnection->RemoveFunction("REVERSESTRING"_ns);

      // Update the scoping to match the new implememntation: split to oa suffix
      // and origin key First rename the old table, we want to remove some
      // columns no longer needed, but even before that drop all indexes from it
      // (CREATE IF NOT EXISTS for index on the new table would falsely find the
      // index!)
      rv = aWorkerConnection->ExecuteSimpleSQL(nsLiteralCString(
          "DROP INDEX IF EXISTS webappsstore2.origin_key_index"));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aWorkerConnection->ExecuteSimpleSQL(nsLiteralCString(
          "DROP INDEX IF EXISTS webappsstore2.scope_key_index"));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aWorkerConnection->ExecuteSimpleSQL(nsLiteralCString(
          "ALTER TABLE webappsstore2 RENAME TO webappsstore2_old"));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<mozIStorageFunction> oaSuffixFunc(new GetOriginParticular(
          GetOriginParticular::ORIGIN_ATTRIBUTES_SUFFIX));
      rv = aWorkerConnection->CreateFunction("GET_ORIGIN_SUFFIX"_ns, 1,
                                             oaSuffixFunc);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<mozIStorageFunction> originKeyFunc(
          new GetOriginParticular(GetOriginParticular::ORIGIN_KEY));
      rv = aWorkerConnection->CreateFunction("GET_ORIGIN_KEY"_ns, 1,
                                             originKeyFunc);
      NS_ENSURE_SUCCESS(rv, rv);

      // Here we ensure this schema tables when we are updating.
      rv = CreateSchema1Tables(aWorkerConnection);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aWorkerConnection->ExecuteSimpleSQL(nsLiteralCString(
          "INSERT OR IGNORE INTO "
          "webappsstore2 (originAttributes, originKey, scope, key, value) "
          "SELECT GET_ORIGIN_SUFFIX(scope), GET_ORIGIN_KEY(scope), scope, key, "
          "value "
          "FROM webappsstore2_old"));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aWorkerConnection->ExecuteSimpleSQL(
          "DROP TABLE webappsstore2_old"_ns);
      NS_ENSURE_SUCCESS(rv, rv);

      aWorkerConnection->RemoveFunction("GET_ORIGIN_SUFFIX"_ns);
      aWorkerConnection->RemoveFunction("GET_ORIGIN_KEY"_ns);

      rv = aWorkerConnection->SetSchemaVersion(1);
      NS_ENSURE_SUCCESS(rv, rv);

      [[fallthrough]];
    }
    case 1: {
      nsCOMPtr<mozIStorageFunction> oaStripAddonId(new StripOriginAddonId());
      rv = aWorkerConnection->CreateFunction("STRIP_ADDON_ID"_ns, 1,
                                             oaStripAddonId);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aWorkerConnection->ExecuteSimpleSQL(nsLiteralCString(
          "UPDATE webappsstore2 "
          "SET originAttributes = STRIP_ADDON_ID(originAttributes) "
          "WHERE originAttributes LIKE '^%'"));
      NS_ENSURE_SUCCESS(rv, rv);

      aWorkerConnection->RemoveFunction("STRIP_ADDON_ID"_ns);

      rv = aWorkerConnection->SetSchemaVersion(2);
      NS_ENSURE_SUCCESS(rv, rv);

      [[fallthrough]];
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
  }  // switch

  rv = transaction.Commit();
  NS_ENSURE_SUCCESS(rv, rv);

  if (doVacuum) {
    // In some cases this can make the disk file of the database significantly
    // smaller.  VACUUM cannot be executed inside a transaction.
    rv = aWorkerConnection->ExecuteSimpleSQL("VACUUM"_ns);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

}  // namespace StorageDBUpdater
}  // namespace dom
}  // namespace mozilla
