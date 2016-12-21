/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/DBSchema.h"

#include "ipc/IPCMessageUtils.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/HeadersBinding.h"
#include "mozilla/dom/InternalHeaders.h"
#include "mozilla/dom/RequestBinding.h"
#include "mozilla/dom/ResponseBinding.h"
#include "mozilla/dom/cache/CacheTypes.h"
#include "mozilla/dom/cache/SavedTypes.h"
#include "mozilla/dom/cache/Types.h"
#include "mozilla/dom/cache/TypeUtils.h"
#include "mozIStorageConnection.h"
#include "mozIStorageStatement.h"
#include "mozStorageHelper.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsHttp.h"
#include "nsIContentPolicy.h"
#include "nsICryptoHash.h"
#include "nsNetCID.h"
#include "nsPrintfCString.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {
namespace cache {
namespace db {
const int32_t kFirstShippedSchemaVersion = 15;
namespace {
// Update this whenever the DB schema is changed.
const int32_t kLatestSchemaVersion = 24;
// ---------
// The following constants define the SQL schema.  These are defined in the
// same order the SQL should be executed in CreateOrMigrateSchema().  They are
// broken out as constants for convenient use in validation and migration.
// ---------
// The caches table is the single source of truth about what Cache
// objects exist for the origin.  The contents of the Cache are stored
// in the entries table that references back to caches.
//
// The caches table is also referenced from storage.  Rows in storage
// represent named Cache objects.  There are cases, however, where
// a Cache can still exist, but not be in a named Storage.  For example,
// when content is still using the Cache after CacheStorage::Delete()
// has been run.
//
// For now, the caches table mainly exists for data integrity with
// foreign keys, but could be expanded to contain additional cache object
// information.
//
// AUTOINCREMENT is necessary to prevent CacheId values from being reused.
const char* const kTableCaches =
  "CREATE TABLE caches ("
    "id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT "
  ")";

// Security blobs are quite large and duplicated for every Response from
// the same https origin.  This table is used to de-duplicate this data.
const char* const kTableSecurityInfo =
  "CREATE TABLE security_info ("
    "id INTEGER NOT NULL PRIMARY KEY, "
    "hash BLOB NOT NULL, "  // first 8-bytes of the sha1 hash of data column
    "data BLOB NOT NULL, "  // full security info data, usually a few KB
    "refcount INTEGER NOT NULL"
  ")";

// Index the smaller hash value instead of the large security data blob.
const char* const kIndexSecurityInfoHash =
  "CREATE INDEX security_info_hash_index ON security_info (hash)";

const char* const kTableEntries =
  "CREATE TABLE entries ("
    "id INTEGER NOT NULL PRIMARY KEY, "
    "request_method TEXT NOT NULL, "
    "request_url_no_query TEXT NOT NULL, "
    "request_url_no_query_hash BLOB NOT NULL, " // first 8-bytes of sha1 hash
    "request_url_query TEXT NOT NULL, "
    "request_url_query_hash BLOB NOT NULL, "    // first 8-bytes of sha1 hash
    "request_referrer TEXT NOT NULL, "
    "request_headers_guard INTEGER NOT NULL, "
    "request_mode INTEGER NOT NULL, "
    "request_credentials INTEGER NOT NULL, "
    "request_contentpolicytype INTEGER NOT NULL, "
    "request_cache INTEGER NOT NULL, "
    "request_body_id TEXT NULL, "
    "response_type INTEGER NOT NULL, "
    "response_status INTEGER NOT NULL, "
    "response_status_text TEXT NOT NULL, "
    "response_headers_guard INTEGER NOT NULL, "
    "response_body_id TEXT NULL, "
    "response_security_info_id INTEGER NULL REFERENCES security_info(id), "
    "response_principal_info TEXT NOT NULL, "
    "cache_id INTEGER NOT NULL REFERENCES caches(id) ON DELETE CASCADE, "
    "request_redirect INTEGER NOT NULL, "
    "request_referrer_policy INTEGER NOT NULL, "
    "request_integrity TEXT NOT NULL, "
    "request_url_fragment TEXT NOT NULL"
    // New columns must be added at the end of table to migrate and
    // validate properly.
  ")";
// Create an index to support the QueryCache() matching algorithm.  This
// needs to quickly find entries in a given Cache that match the request
// URL.  The url query is separated in order to support the ignoreSearch
// option.  Finally, we index hashes of the URL values instead of the
// actual strings to avoid excessive disk bloat.  The index will duplicate
// the contents of the columsn in the index.  The hash index will prune
// the vast majority of values from the query result so that normal
// scanning only has to be done on a few values to find an exact URL match.
const char* const kIndexEntriesRequest =
  "CREATE INDEX entries_request_match_index "
            "ON entries (cache_id, request_url_no_query_hash, "
                        "request_url_query_hash)";

const char* const kTableRequestHeaders =
  "CREATE TABLE request_headers ("
    "name TEXT NOT NULL, "
    "value TEXT NOT NULL, "
    "entry_id INTEGER NOT NULL REFERENCES entries(id) ON DELETE CASCADE"
  ")";

const char* const kTableResponseHeaders =
  "CREATE TABLE response_headers ("
    "name TEXT NOT NULL, "
    "value TEXT NOT NULL, "
    "entry_id INTEGER NOT NULL REFERENCES entries(id) ON DELETE CASCADE"
  ")";

// We need an index on response_headers, but not on request_headers,
// because we quickly need to determine if a VARY header is present.
const char* const kIndexResponseHeadersName =
  "CREATE INDEX response_headers_name_index "
            "ON response_headers (name)";

const char* const kTableResponseUrlList =
  "CREATE TABLE response_url_list ("
    "url TEXT NOT NULL, "
    "entry_id INTEGER NOT NULL REFERENCES entries(id) ON DELETE CASCADE"
  ")";

// NOTE: key allows NULL below since that is how "" is represented
//       in a BLOB column.  We use BLOB to avoid encoding issues
//       with storing DOMStrings.
const char* const kTableStorage =
  "CREATE TABLE storage ("
    "namespace INTEGER NOT NULL, "
    "key BLOB NULL, "
    "cache_id INTEGER NOT NULL REFERENCES caches(id), "
    "PRIMARY KEY(namespace, key) "
  ")";

// ---------
// End schema definition
// ---------

const int32_t kMaxEntriesPerStatement = 255;

const uint32_t kPageSize = 4 * 1024;

// Grow the database in chunks to reduce fragmentation
const uint32_t kGrowthSize = 32 * 1024;
const uint32_t kGrowthPages = kGrowthSize / kPageSize;
static_assert(kGrowthSize % kPageSize == 0,
              "Growth size must be multiple of page size");

// Only release free pages when we have more than this limit
const int32_t kMaxFreePages = kGrowthPages;

// Limit WAL journal to a reasonable size
const uint32_t kWalAutoCheckpointSize = 512 * 1024;
const uint32_t kWalAutoCheckpointPages = kWalAutoCheckpointSize / kPageSize;
static_assert(kWalAutoCheckpointSize % kPageSize == 0,
              "WAL checkpoint size must be multiple of page size");

} // namespace

// If any of the static_asserts below fail, it means that you have changed
// the corresponding WebIDL enum in a way that may be incompatible with the
// existing data stored in the DOM Cache.  You would need to update the Cache
// database schema accordingly and adjust the failing static_assert.
static_assert(int(HeadersGuardEnum::None) == 0 &&
              int(HeadersGuardEnum::Request) == 1 &&
              int(HeadersGuardEnum::Request_no_cors) == 2 &&
              int(HeadersGuardEnum::Response) == 3 &&
              int(HeadersGuardEnum::Immutable) == 4 &&
              int(HeadersGuardEnum::EndGuard_) == 5,
              "HeadersGuardEnum values are as expected");
static_assert(int(ReferrerPolicy::_empty) == 0 &&
              int(ReferrerPolicy::No_referrer) == 1 &&
              int(ReferrerPolicy::No_referrer_when_downgrade) == 2 &&
              int(ReferrerPolicy::Origin) == 3 &&
              int(ReferrerPolicy::Origin_when_cross_origin) == 4 &&
              int(ReferrerPolicy::Unsafe_url) == 5 &&
              int(ReferrerPolicy::Same_origin) == 6 &&
              int(ReferrerPolicy::Strict_origin) == 7 &&
              int(ReferrerPolicy::Strict_origin_when_cross_origin) == 8 &&
              int(ReferrerPolicy::EndGuard_) == 9,
              "ReferrerPolicy values are as expected");
static_assert(int(RequestMode::Same_origin) == 0 &&
              int(RequestMode::No_cors) == 1 &&
              int(RequestMode::Cors) == 2 &&
              int(RequestMode::Navigate) == 3 &&
              int(RequestMode::EndGuard_) == 4,
              "RequestMode values are as expected");
static_assert(int(RequestCredentials::Omit) == 0 &&
              int(RequestCredentials::Same_origin) == 1 &&
              int(RequestCredentials::Include) == 2 &&
              int(RequestCredentials::EndGuard_) == 3,
              "RequestCredentials values are as expected");
static_assert(int(RequestCache::Default) == 0 &&
              int(RequestCache::No_store) == 1 &&
              int(RequestCache::Reload) == 2 &&
              int(RequestCache::No_cache) == 3 &&
              int(RequestCache::Force_cache) == 4 &&
              int(RequestCache::Only_if_cached) == 5 &&
              int(RequestCache::EndGuard_) == 6,
              "RequestCache values are as expected");
static_assert(int(RequestRedirect::Follow) == 0 &&
              int(RequestRedirect::Error) == 1 &&
              int(RequestRedirect::Manual) == 2 &&
              int(RequestRedirect::EndGuard_) == 3,
              "RequestRedirect values are as expected");
static_assert(int(ResponseType::Basic) == 0 &&
              int(ResponseType::Cors) == 1 &&
              int(ResponseType::Default) == 2 &&
              int(ResponseType::Error) == 3 &&
              int(ResponseType::Opaque) == 4 &&
              int(ResponseType::Opaqueredirect) == 5 &&
              int(ResponseType::EndGuard_) == 6,
              "ResponseType values are as expected");

// If the static_asserts below fails, it means that you have changed the
// Namespace enum in a way that may be incompatible with the existing data
// stored in the DOM Cache.  You would need to update the Cache database schema
// accordingly and adjust the failing static_assert.
static_assert(DEFAULT_NAMESPACE == 0 &&
              CHROME_ONLY_NAMESPACE == 1 &&
              NUMBER_OF_NAMESPACES == 2,
              "Namespace values are as expected");

// If the static_asserts below fails, it means that you have changed the
// nsContentPolicy enum in a way that may be incompatible with the existing data
// stored in the DOM Cache.  You would need to update the Cache database schema
// accordingly and adjust the failing static_assert.
static_assert(nsIContentPolicy::TYPE_INVALID == 0 &&
              nsIContentPolicy::TYPE_OTHER == 1 &&
              nsIContentPolicy::TYPE_SCRIPT == 2 &&
              nsIContentPolicy::TYPE_IMAGE == 3 &&
              nsIContentPolicy::TYPE_STYLESHEET == 4 &&
              nsIContentPolicy::TYPE_OBJECT == 5 &&
              nsIContentPolicy::TYPE_DOCUMENT == 6 &&
              nsIContentPolicy::TYPE_SUBDOCUMENT == 7 &&
              nsIContentPolicy::TYPE_REFRESH == 8 &&
              nsIContentPolicy::TYPE_XBL == 9 &&
              nsIContentPolicy::TYPE_PING == 10 &&
              nsIContentPolicy::TYPE_XMLHTTPREQUEST == 11 &&
              nsIContentPolicy::TYPE_DATAREQUEST == 11 &&
              nsIContentPolicy::TYPE_OBJECT_SUBREQUEST == 12 &&
              nsIContentPolicy::TYPE_DTD == 13 &&
              nsIContentPolicy::TYPE_FONT == 14 &&
              nsIContentPolicy::TYPE_MEDIA == 15 &&
              nsIContentPolicy::TYPE_WEBSOCKET == 16 &&
              nsIContentPolicy::TYPE_CSP_REPORT == 17 &&
              nsIContentPolicy::TYPE_XSLT == 18 &&
              nsIContentPolicy::TYPE_BEACON == 19 &&
              nsIContentPolicy::TYPE_FETCH == 20 &&
              nsIContentPolicy::TYPE_IMAGESET == 21 &&
              nsIContentPolicy::TYPE_WEB_MANIFEST == 22 &&
              nsIContentPolicy::TYPE_INTERNAL_SCRIPT == 23 &&
              nsIContentPolicy::TYPE_INTERNAL_WORKER == 24 &&
              nsIContentPolicy::TYPE_INTERNAL_SHARED_WORKER == 25 &&
              nsIContentPolicy::TYPE_INTERNAL_EMBED == 26 &&
              nsIContentPolicy::TYPE_INTERNAL_OBJECT == 27 &&
              nsIContentPolicy::TYPE_INTERNAL_FRAME == 28 &&
              nsIContentPolicy::TYPE_INTERNAL_IFRAME == 29 &&
              nsIContentPolicy::TYPE_INTERNAL_AUDIO == 30 &&
              nsIContentPolicy::TYPE_INTERNAL_VIDEO == 31 &&
              nsIContentPolicy::TYPE_INTERNAL_TRACK == 32 &&
              nsIContentPolicy::TYPE_INTERNAL_XMLHTTPREQUEST == 33 &&
              nsIContentPolicy::TYPE_INTERNAL_EVENTSOURCE == 34 &&
              nsIContentPolicy::TYPE_INTERNAL_SERVICE_WORKER == 35 &&
              nsIContentPolicy::TYPE_INTERNAL_SCRIPT_PRELOAD == 36 &&
              nsIContentPolicy::TYPE_INTERNAL_IMAGE == 37 &&
              nsIContentPolicy::TYPE_INTERNAL_IMAGE_PRELOAD == 38 &&
              nsIContentPolicy::TYPE_INTERNAL_STYLESHEET == 39 &&
              nsIContentPolicy::TYPE_INTERNAL_STYLESHEET_PRELOAD == 40 &&
              nsIContentPolicy::TYPE_INTERNAL_IMAGE_FAVICON == 41,
              "nsContentPolicyType values are as expected");

namespace {

typedef int32_t EntryId;

struct IdCount
{
  IdCount() : mId(-1), mCount(0) { }
  explicit IdCount(int32_t aId) : mId(aId), mCount(1) { }
  int32_t mId;
  int32_t mCount;
};

static nsresult QueryAll(mozIStorageConnection* aConn, CacheId aCacheId,
                         nsTArray<EntryId>& aEntryIdListOut);
static nsresult QueryCache(mozIStorageConnection* aConn, CacheId aCacheId,
                           const CacheRequest& aRequest,
                           const CacheQueryParams& aParams,
                           nsTArray<EntryId>& aEntryIdListOut,
                           uint32_t aMaxResults = UINT32_MAX);
static nsresult MatchByVaryHeader(mozIStorageConnection* aConn,
                                  const CacheRequest& aRequest,
                                  EntryId entryId, bool* aSuccessOut);
static nsresult DeleteEntries(mozIStorageConnection* aConn,
                              const nsTArray<EntryId>& aEntryIdList,
                              nsTArray<nsID>& aDeletedBodyIdListOut,
                              nsTArray<IdCount>& aDeletedSecurityIdListOut,
                              uint32_t aPos=0, int32_t aLen=-1);
static nsresult InsertSecurityInfo(mozIStorageConnection* aConn,
                                   nsICryptoHash* aCrypto,
                                   const nsACString& aData, int32_t *aIdOut);
static nsresult DeleteSecurityInfo(mozIStorageConnection* aConn, int32_t aId,
                                   int32_t aCount);
static nsresult DeleteSecurityInfoList(mozIStorageConnection* aConn,
                                       const nsTArray<IdCount>& aDeletedStorageIdList);
static nsresult InsertEntry(mozIStorageConnection* aConn, CacheId aCacheId,
                            const CacheRequest& aRequest,
                            const nsID* aRequestBodyId,
                            const CacheResponse& aResponse,
                            const nsID* aResponseBodyId);
static nsresult ReadResponse(mozIStorageConnection* aConn, EntryId aEntryId,
                             SavedResponse* aSavedResponseOut);
static nsresult ReadRequest(mozIStorageConnection* aConn, EntryId aEntryId,
                            SavedRequest* aSavedRequestOut);

static void AppendListParamsToQuery(nsACString& aQuery,
                                    const nsTArray<EntryId>& aEntryIdList,
                                    uint32_t aPos, int32_t aLen);
static nsresult BindListParamsToQuery(mozIStorageStatement* aState,
                                      const nsTArray<EntryId>& aEntryIdList,
                                      uint32_t aPos, int32_t aLen);
static nsresult BindId(mozIStorageStatement* aState, const nsACString& aName,
                       const nsID* aId);
static nsresult ExtractId(mozIStorageStatement* aState, uint32_t aPos,
                          nsID* aIdOut);
static nsresult CreateAndBindKeyStatement(mozIStorageConnection* aConn,
                                          const char* aQueryFormat,
                                          const nsAString& aKey,
                                          mozIStorageStatement** aStateOut);
static nsresult HashCString(nsICryptoHash* aCrypto, const nsACString& aIn,
                            nsACString& aOut);
nsresult Validate(mozIStorageConnection* aConn);
nsresult Migrate(mozIStorageConnection* aConn);
} // namespace

class MOZ_RAII AutoDisableForeignKeyChecking
{
public:
  explicit AutoDisableForeignKeyChecking(mozIStorageConnection* aConn)
    : mConn(aConn)
    , mForeignKeyCheckingDisabled(false)
  {
    nsCOMPtr<mozIStorageStatement> state;
    nsresult rv = mConn->CreateStatement(NS_LITERAL_CSTRING(
      "PRAGMA foreign_keys;"
    ), getter_AddRefs(state));
    if (NS_WARN_IF(NS_FAILED(rv))) { return; }

    bool hasMoreData = false;
    rv = state->ExecuteStep(&hasMoreData);
    if (NS_WARN_IF(NS_FAILED(rv))) { return; }

    int32_t mode;
    rv = state->GetInt32(0, &mode);
    if (NS_WARN_IF(NS_FAILED(rv))) { return; }

    if (mode) {
      nsresult rv = mConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "PRAGMA foreign_keys = OFF;"
      ));
      if (NS_WARN_IF(NS_FAILED(rv))) { return; }
      mForeignKeyCheckingDisabled = true;
    }
  }

  ~AutoDisableForeignKeyChecking()
  {
    if (mForeignKeyCheckingDisabled) {
      nsresult rv = mConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "PRAGMA foreign_keys = ON;"
      ));
      if (NS_WARN_IF(NS_FAILED(rv))) { return; }
    }
  }

private:
  nsCOMPtr<mozIStorageConnection> mConn;
  bool mForeignKeyCheckingDisabled;
};

nsresult
CreateOrMigrateSchema(mozIStorageConnection* aConn)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  int32_t schemaVersion;
  nsresult rv = aConn->GetSchemaVersion(&schemaVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  if (schemaVersion == kLatestSchemaVersion) {
    // We already have the correct schema version.  Validate it matches
    // our expected schema and then proceed.
    rv = Validate(aConn);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    return rv;
  }

  // Turn off checking foreign keys before starting a transaction, and restore
  // it once we're done.
  AutoDisableForeignKeyChecking restoreForeignKeyChecking(aConn);
  mozStorageTransaction trans(aConn, false,
                              mozIStorageConnection::TRANSACTION_IMMEDIATE);
  bool needVacuum = false;

  if (schemaVersion) {
    // A schema exists, but its not the current version.  Attempt to
    // migrate it to our new schema.
    rv = Migrate(aConn);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    // Migrations happen infrequently and reflect a chance in DB structure.
    // This is a good time to rebuild the database.  It also helps catch
    // if a new migration is incorrect by fast failing on the corruption.
    needVacuum = true;
  } else {
    // There is no schema installed.  Create the database from scratch.
    rv = aConn->ExecuteSimpleSQL(nsDependentCString(kTableCaches));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = aConn->ExecuteSimpleSQL(nsDependentCString(kTableSecurityInfo));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = aConn->ExecuteSimpleSQL(nsDependentCString(kIndexSecurityInfoHash));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = aConn->ExecuteSimpleSQL(nsDependentCString(kTableEntries));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = aConn->ExecuteSimpleSQL(nsDependentCString(kIndexEntriesRequest));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = aConn->ExecuteSimpleSQL(nsDependentCString(kTableRequestHeaders));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = aConn->ExecuteSimpleSQL(nsDependentCString(kTableResponseHeaders));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = aConn->ExecuteSimpleSQL(nsDependentCString(kIndexResponseHeadersName));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = aConn->ExecuteSimpleSQL(nsDependentCString(kTableResponseUrlList));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = aConn->ExecuteSimpleSQL(nsDependentCString(kTableStorage));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = aConn->SetSchemaVersion(kLatestSchemaVersion);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = aConn->GetSchemaVersion(&schemaVersion);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  }

  rv = Validate(aConn);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = trans.Commit();
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  if (needVacuum) {
    // Unfortunately, this must be performed outside of the transaction.
    aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("VACUUM"));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  }

  return rv;
}

nsresult
InitializeConnection(mozIStorageConnection* aConn)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  // This function needs to perform per-connection initialization tasks that
  // need to happen regardless of the schema.

  nsPrintfCString pragmas(
    // Use a smaller page size to improve perf/footprint; default is too large
    "PRAGMA page_size = %u; "
    // Enable auto_vacuum; this must happen after page_size and before WAL
    "PRAGMA auto_vacuum = INCREMENTAL; "
    "PRAGMA foreign_keys = ON; ",
    kPageSize
  );

  // Note, the default encoding of UTF-8 is preferred.  mozStorage does all
  // the work necessary to convert UTF-16 nsString values for us.  We don't
  // need ordering and the binary equality operations are correct.  So, do
  // NOT set PRAGMA encoding to UTF-16.

  nsresult rv = aConn->ExecuteSimpleSQL(pragmas);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Limit fragmentation by growing the database by many pages at once.
  rv = aConn->SetGrowthIncrement(kGrowthSize, EmptyCString());
  if (rv == NS_ERROR_FILE_TOO_BIG) {
    NS_WARNING("Not enough disk space to set sqlite growth increment.");
    rv = NS_OK;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Enable WAL journaling.  This must be performed in a separate transaction
  // after changing the page_size and enabling auto_vacuum.
  nsPrintfCString wal(
    // WAL journal can grow to given number of *pages*
    "PRAGMA wal_autocheckpoint = %u; "
    // Always truncate the journal back to given number of *bytes*
    "PRAGMA journal_size_limit = %u; "
    // WAL must be enabled at the end to allow page size to be changed, etc.
    "PRAGMA journal_mode = WAL; ",
    kWalAutoCheckpointPages,
    kWalAutoCheckpointSize
  );

  rv = aConn->ExecuteSimpleSQL(wal);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Verify that we successfully set the vacuum mode to incremental.  It
  // is very easy to put the database in a state where the auto_vacuum
  // pragma above fails silently.
#ifdef DEBUG
  nsCOMPtr<mozIStorageStatement> state;
  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "PRAGMA auto_vacuum;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool hasMoreData = false;
  rv = state->ExecuteStep(&hasMoreData);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  int32_t mode;
  rv = state->GetInt32(0, &mode);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // integer value 2 is incremental mode
  if (NS_WARN_IF(mode != 2)) { return NS_ERROR_UNEXPECTED; }
#endif

  return NS_OK;
}

nsresult
CreateCacheId(mozIStorageConnection* aConn, CacheId* aCacheIdOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);
  MOZ_ASSERT(aCacheIdOut);

  nsresult rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO caches DEFAULT VALUES;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsCOMPtr<mozIStorageStatement> state;
  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT last_insert_rowid()"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool hasMoreData = false;
  rv = state->ExecuteStep(&hasMoreData);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  if (NS_WARN_IF(!hasMoreData)) { return NS_ERROR_UNEXPECTED; }

  rv = state->GetInt64(0, aCacheIdOut);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

nsresult
DeleteCacheId(mozIStorageConnection* aConn, CacheId aCacheId,
              nsTArray<nsID>& aDeletedBodyIdListOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  // Delete the bodies explicitly as we need to read out the body IDs
  // anyway.  These body IDs must be deleted one-by-one as content may
  // still be referencing them invidivually.
  AutoTArray<EntryId, 256> matches;
  nsresult rv = QueryAll(aConn, aCacheId, matches);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  AutoTArray<IdCount, 16> deletedSecurityIdList;
  rv = DeleteEntries(aConn, matches, aDeletedBodyIdListOut,
                     deletedSecurityIdList);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = DeleteSecurityInfoList(aConn, deletedSecurityIdList);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Delete the remainder of the cache using cascade semantics.
  nsCOMPtr<mozIStorageStatement> state;
  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM caches WHERE id=:id;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt64ByName(NS_LITERAL_CSTRING("id"), aCacheId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

nsresult
IsCacheOrphaned(mozIStorageConnection* aConn, CacheId aCacheId,
                bool* aOrphanedOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);
  MOZ_ASSERT(aOrphanedOut);

  // err on the side of not deleting user data
  *aOrphanedOut = false;

  nsCOMPtr<mozIStorageStatement> state;
  nsresult rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT COUNT(*) FROM storage WHERE cache_id=:cache_id;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt64ByName(NS_LITERAL_CSTRING("cache_id"), aCacheId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool hasMoreData = false;
  rv = state->ExecuteStep(&hasMoreData);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  MOZ_ASSERT(hasMoreData);

  int32_t refCount;
  rv = state->GetInt32(0, &refCount);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  *aOrphanedOut = refCount == 0;

  return rv;
}

nsresult
FindOrphanedCacheIds(mozIStorageConnection* aConn,
                     nsTArray<CacheId>& aOrphanedListOut)
{
  nsCOMPtr<mozIStorageStatement> state;
  nsresult rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id FROM caches "
    "WHERE id NOT IN (SELECT cache_id from storage);"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool hasMoreData = false;
  while (NS_SUCCEEDED(state->ExecuteStep(&hasMoreData)) && hasMoreData) {
    CacheId cacheId = INVALID_CACHE_ID;
    rv = state->GetInt64(0, &cacheId);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
    aOrphanedListOut.AppendElement(cacheId);
  }

  return rv;
}

nsresult
GetKnownBodyIds(mozIStorageConnection* aConn, nsTArray<nsID>& aBodyIdListOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  nsCOMPtr<mozIStorageStatement> state;
  nsresult rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT request_body_id, response_body_id FROM entries;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool hasMoreData = false;
  while (NS_SUCCEEDED(state->ExecuteStep(&hasMoreData)) && hasMoreData) {
    // extract 0 to 2 nsID structs per row
    for (uint32_t i = 0; i < 2; ++i) {
      bool isNull = false;

      rv = state->GetIsNull(i, &isNull);
      if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

      if (!isNull) {
        nsID id;
        rv = ExtractId(state, i, &id);
        if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

        aBodyIdListOut.AppendElement(id);
      }
    }
  }

  return rv;
}

nsresult
CacheMatch(mozIStorageConnection* aConn, CacheId aCacheId,
           const CacheRequest& aRequest,
           const CacheQueryParams& aParams,
           bool* aFoundResponseOut,
           SavedResponse* aSavedResponseOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);
  MOZ_ASSERT(aFoundResponseOut);
  MOZ_ASSERT(aSavedResponseOut);

  *aFoundResponseOut = false;

  AutoTArray<EntryId, 1> matches;
  nsresult rv = QueryCache(aConn, aCacheId, aRequest, aParams, matches, 1);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  if (matches.IsEmpty()) {
    return rv;
  }

  rv = ReadResponse(aConn, matches[0], aSavedResponseOut);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  aSavedResponseOut->mCacheId = aCacheId;
  *aFoundResponseOut = true;

  return rv;
}

nsresult
CacheMatchAll(mozIStorageConnection* aConn, CacheId aCacheId,
              const CacheRequestOrVoid& aRequestOrVoid,
              const CacheQueryParams& aParams,
              nsTArray<SavedResponse>& aSavedResponsesOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);
  nsresult rv;

  AutoTArray<EntryId, 256> matches;
  if (aRequestOrVoid.type() == CacheRequestOrVoid::Tvoid_t) {
    rv = QueryAll(aConn, aCacheId, matches);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  } else {
    rv = QueryCache(aConn, aCacheId, aRequestOrVoid, aParams, matches);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  }

  // TODO: replace this with a bulk load using SQL IN clause (bug 1110458)
  for (uint32_t i = 0; i < matches.Length(); ++i) {
    SavedResponse savedResponse;
    rv = ReadResponse(aConn, matches[i], &savedResponse);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
    savedResponse.mCacheId = aCacheId;
    aSavedResponsesOut.AppendElement(savedResponse);
  }

  return rv;
}

nsresult
CachePut(mozIStorageConnection* aConn, CacheId aCacheId,
         const CacheRequest& aRequest,
         const nsID* aRequestBodyId,
         const CacheResponse& aResponse,
         const nsID* aResponseBodyId,
         nsTArray<nsID>& aDeletedBodyIdListOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  CacheQueryParams params(false, false, false, false,
                           NS_LITERAL_STRING(""));
  AutoTArray<EntryId, 256> matches;
  nsresult rv = QueryCache(aConn, aCacheId, aRequest, params, matches);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  AutoTArray<IdCount, 16> deletedSecurityIdList;
  rv = DeleteEntries(aConn, matches, aDeletedBodyIdListOut,
                     deletedSecurityIdList);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = InsertEntry(aConn, aCacheId, aRequest, aRequestBodyId, aResponse,
                   aResponseBodyId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Delete the security values after doing the insert to avoid churning
  // the security table when its not necessary.
  rv = DeleteSecurityInfoList(aConn, deletedSecurityIdList);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

nsresult
CacheDelete(mozIStorageConnection* aConn, CacheId aCacheId,
            const CacheRequest& aRequest,
            const CacheQueryParams& aParams,
            nsTArray<nsID>& aDeletedBodyIdListOut, bool* aSuccessOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);
  MOZ_ASSERT(aSuccessOut);

  *aSuccessOut = false;

  AutoTArray<EntryId, 256> matches;
  nsresult rv = QueryCache(aConn, aCacheId, aRequest, aParams, matches);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  if (matches.IsEmpty()) {
    return rv;
  }

  AutoTArray<IdCount, 16> deletedSecurityIdList;
  rv = DeleteEntries(aConn, matches, aDeletedBodyIdListOut,
                     deletedSecurityIdList);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = DeleteSecurityInfoList(aConn, deletedSecurityIdList);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  *aSuccessOut = true;

  return rv;
}

nsresult
CacheKeys(mozIStorageConnection* aConn, CacheId aCacheId,
          const CacheRequestOrVoid& aRequestOrVoid,
          const CacheQueryParams& aParams,
          nsTArray<SavedRequest>& aSavedRequestsOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);
  nsresult rv;

  AutoTArray<EntryId, 256> matches;
  if (aRequestOrVoid.type() == CacheRequestOrVoid::Tvoid_t) {
    rv = QueryAll(aConn, aCacheId, matches);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  } else {
    rv = QueryCache(aConn, aCacheId, aRequestOrVoid, aParams, matches);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  }

  // TODO: replace this with a bulk load using SQL IN clause (bug 1110458)
  for (uint32_t i = 0; i < matches.Length(); ++i) {
    SavedRequest savedRequest;
    rv = ReadRequest(aConn, matches[i], &savedRequest);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
    savedRequest.mCacheId = aCacheId;
    aSavedRequestsOut.AppendElement(savedRequest);
  }

  return rv;
}

nsresult
StorageMatch(mozIStorageConnection* aConn,
             Namespace aNamespace,
             const CacheRequest& aRequest,
             const CacheQueryParams& aParams,
             bool* aFoundResponseOut,
             SavedResponse* aSavedResponseOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);
  MOZ_ASSERT(aFoundResponseOut);
  MOZ_ASSERT(aSavedResponseOut);

  *aFoundResponseOut = false;

  nsresult rv;

  // If we are given a cache to check, then simply find its cache ID
  // and perform the match.
  if (!aParams.cacheName().EqualsLiteral("")) {
    bool foundCache = false;
    // no invalid CacheId, init to least likely real value
    CacheId cacheId = INVALID_CACHE_ID;
    rv = StorageGetCacheId(aConn, aNamespace, aParams.cacheName(), &foundCache,
                           &cacheId);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
    if (!foundCache) { return NS_OK; }

    rv = CacheMatch(aConn, cacheId, aRequest, aParams, aFoundResponseOut,
                    aSavedResponseOut);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    return rv;
  }

  // Otherwise we need to get a list of all the cache IDs in this namespace.

  nsCOMPtr<mozIStorageStatement> state;
  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT cache_id FROM storage WHERE namespace=:namespace ORDER BY rowid;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32ByName(NS_LITERAL_CSTRING("namespace"), aNamespace);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  AutoTArray<CacheId, 32> cacheIdList;

  bool hasMoreData = false;
  while (NS_SUCCEEDED(state->ExecuteStep(&hasMoreData)) && hasMoreData) {
    CacheId cacheId = INVALID_CACHE_ID;
    rv = state->GetInt64(0, &cacheId);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
    cacheIdList.AppendElement(cacheId);
  }

  // Now try to find a match in each cache in order
  for (uint32_t i = 0; i < cacheIdList.Length(); ++i) {
    rv = CacheMatch(aConn, cacheIdList[i], aRequest, aParams, aFoundResponseOut,
                    aSavedResponseOut);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    if (*aFoundResponseOut) {
      aSavedResponseOut->mCacheId = cacheIdList[i];
      return rv;
    }
  }

  return NS_OK;
}

nsresult
StorageGetCacheId(mozIStorageConnection* aConn, Namespace aNamespace,
                  const nsAString& aKey, bool* aFoundCacheOut,
                  CacheId* aCacheIdOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);
  MOZ_ASSERT(aFoundCacheOut);
  MOZ_ASSERT(aCacheIdOut);

  *aFoundCacheOut = false;

  // How we constrain the key column depends on the value of our key.  Use
  // a format string for the query and let CreateAndBindKeyStatement() fill
  // it in for us.
  const char* query = "SELECT cache_id FROM storage "
                      "WHERE namespace=:namespace AND %s "
                      "ORDER BY rowid;";

  nsCOMPtr<mozIStorageStatement> state;
  nsresult rv = CreateAndBindKeyStatement(aConn, query, aKey,
                                          getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32ByName(NS_LITERAL_CSTRING("namespace"), aNamespace);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool hasMoreData = false;
  rv = state->ExecuteStep(&hasMoreData);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  if (!hasMoreData) {
    return rv;
  }

  rv = state->GetInt64(0, aCacheIdOut);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  *aFoundCacheOut = true;
  return rv;
}

nsresult
StoragePutCache(mozIStorageConnection* aConn, Namespace aNamespace,
                const nsAString& aKey, CacheId aCacheId)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  nsCOMPtr<mozIStorageStatement> state;
  nsresult rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO storage (namespace, key, cache_id) "
                 "VALUES (:namespace, :key, :cache_id);"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32ByName(NS_LITERAL_CSTRING("namespace"), aNamespace);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindStringAsBlobByName(NS_LITERAL_CSTRING("key"), aKey);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt64ByName(NS_LITERAL_CSTRING("cache_id"), aCacheId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

nsresult
StorageForgetCache(mozIStorageConnection* aConn, Namespace aNamespace,
                   const nsAString& aKey)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  // How we constrain the key column depends on the value of our key.  Use
  // a format string for the query and let CreateAndBindKeyStatement() fill
  // it in for us.
  const char *query = "DELETE FROM storage WHERE namespace=:namespace AND %s;";

  nsCOMPtr<mozIStorageStatement> state;
  nsresult rv = CreateAndBindKeyStatement(aConn, query, aKey,
                                          getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32ByName(NS_LITERAL_CSTRING("namespace"), aNamespace);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

nsresult
StorageGetKeys(mozIStorageConnection* aConn, Namespace aNamespace,
               nsTArray<nsString>& aKeysOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  nsCOMPtr<mozIStorageStatement> state;
  nsresult rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT key FROM storage WHERE namespace=:namespace ORDER BY rowid;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32ByName(NS_LITERAL_CSTRING("namespace"), aNamespace);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool hasMoreData = false;
  while (NS_SUCCEEDED(state->ExecuteStep(&hasMoreData)) && hasMoreData) {
    nsAutoString key;
    rv = state->GetBlobAsString(0, key);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    aKeysOut.AppendElement(key);
  }

  return rv;
}

namespace {

nsresult
QueryAll(mozIStorageConnection* aConn, CacheId aCacheId,
         nsTArray<EntryId>& aEntryIdListOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  nsCOMPtr<mozIStorageStatement> state;
  nsresult rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id FROM entries WHERE cache_id=:cache_id ORDER BY id;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt64ByName(NS_LITERAL_CSTRING("cache_id"), aCacheId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool hasMoreData = false;
  while (NS_SUCCEEDED(state->ExecuteStep(&hasMoreData)) && hasMoreData) {
    EntryId entryId = INT32_MAX;
    rv = state->GetInt32(0, &entryId);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
    aEntryIdListOut.AppendElement(entryId);
  }

  return rv;
}

nsresult
QueryCache(mozIStorageConnection* aConn, CacheId aCacheId,
           const CacheRequest& aRequest,
           const CacheQueryParams& aParams,
           nsTArray<EntryId>& aEntryIdListOut,
           uint32_t aMaxResults)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);
  MOZ_ASSERT(aMaxResults > 0);

  if (!aParams.ignoreMethod() &&
      !aRequest.method().LowerCaseEqualsLiteral("get"))
  {
    return NS_OK;
  }

  nsAutoCString query(
    "SELECT id, COUNT(response_headers.name) AS vary_count "
    "FROM entries "
    "LEFT OUTER JOIN response_headers ON entries.id=response_headers.entry_id "
                                    "AND response_headers.name='vary' "
    "WHERE entries.cache_id=:cache_id "
      "AND entries.request_url_no_query_hash=:url_no_query_hash "
  );

  if (!aParams.ignoreSearch()) {
    query.AppendLiteral("AND entries.request_url_query_hash=:url_query_hash ");
  }

  query.AppendLiteral("AND entries.request_url_no_query=:url_no_query ");

  if (!aParams.ignoreSearch()) {
    query.AppendLiteral("AND entries.request_url_query=:url_query ");
  }

  query.AppendLiteral("GROUP BY entries.id ORDER BY entries.id;");

  nsCOMPtr<mozIStorageStatement> state;
  nsresult rv = aConn->CreateStatement(query, getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt64ByName(NS_LITERAL_CSTRING("cache_id"), aCacheId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsCOMPtr<nsICryptoHash> crypto =
    do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsAutoCString urlWithoutQueryHash;
  rv = HashCString(crypto, aRequest.urlWithoutQuery(), urlWithoutQueryHash);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindUTF8StringAsBlobByName(NS_LITERAL_CSTRING("url_no_query_hash"),
                                         urlWithoutQueryHash);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  if (!aParams.ignoreSearch()) {
    nsAutoCString urlQueryHash;
    rv = HashCString(crypto, aRequest.urlQuery(), urlQueryHash);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = state->BindUTF8StringAsBlobByName(NS_LITERAL_CSTRING("url_query_hash"),
                                           urlQueryHash);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  }

  rv = state->BindUTF8StringByName(NS_LITERAL_CSTRING("url_no_query"),
                                   aRequest.urlWithoutQuery());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  if (!aParams.ignoreSearch()) {
    rv = state->BindUTF8StringByName(NS_LITERAL_CSTRING("url_query"),
                                     aRequest.urlQuery());
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  }

  bool hasMoreData = false;
  while (NS_SUCCEEDED(state->ExecuteStep(&hasMoreData)) && hasMoreData) {
    // no invalid EntryId, init to least likely real value
    EntryId entryId = INT32_MAX;
    rv = state->GetInt32(0, &entryId);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    int32_t varyCount;
    rv = state->GetInt32(1, &varyCount);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    if (!aParams.ignoreVary() && varyCount > 0) {
      bool matchedByVary = false;
      rv = MatchByVaryHeader(aConn, aRequest, entryId, &matchedByVary);
      if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
      if (!matchedByVary) {
        continue;
      }
    }

    aEntryIdListOut.AppendElement(entryId);

    if (aEntryIdListOut.Length() == aMaxResults) {
      return NS_OK;
    }
  }

  return rv;
}

nsresult
MatchByVaryHeader(mozIStorageConnection* aConn,
                  const CacheRequest& aRequest,
                  EntryId entryId, bool* aSuccessOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  *aSuccessOut = false;

  nsCOMPtr<mozIStorageStatement> state;
  nsresult rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT value FROM response_headers "
    "WHERE name='vary' AND entry_id=:entry_id;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32ByName(NS_LITERAL_CSTRING("entry_id"), entryId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  AutoTArray<nsCString, 8> varyValues;

  bool hasMoreData = false;
  while (NS_SUCCEEDED(state->ExecuteStep(&hasMoreData)) && hasMoreData) {
    nsAutoCString value;
    rv = state->GetUTF8String(0, value);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
    varyValues.AppendElement(value);
  }
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Should not have called this function if this was not the case
  MOZ_ASSERT(!varyValues.IsEmpty());

  state->Reset();
  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT name, value FROM request_headers "
    "WHERE entry_id=:entry_id;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32ByName(NS_LITERAL_CSTRING("entry_id"), entryId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  RefPtr<InternalHeaders> cachedHeaders =
    new InternalHeaders(HeadersGuardEnum::None);

  while (NS_SUCCEEDED(state->ExecuteStep(&hasMoreData)) && hasMoreData) {
    nsAutoCString name;
    nsAutoCString value;
    rv = state->GetUTF8String(0, name);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
    rv = state->GetUTF8String(1, value);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    ErrorResult errorResult;

    cachedHeaders->Append(name, value, errorResult);
    if (errorResult.Failed()) { return errorResult.StealNSResult(); }
  }
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  RefPtr<InternalHeaders> queryHeaders =
    TypeUtils::ToInternalHeaders(aRequest.headers());

  // Assume the vary headers match until we find a conflict
  bool varyHeadersMatch = true;

  for (uint32_t i = 0; i < varyValues.Length(); ++i) {
    // Extract the header names inside the Vary header value.
    nsAutoCString varyValue(varyValues[i]);
    char* rawBuffer = varyValue.BeginWriting();
    char* token = nsCRT::strtok(rawBuffer, NS_HTTP_HEADER_SEPS, &rawBuffer);
    bool bailOut = false;
    for (; token;
         token = nsCRT::strtok(rawBuffer, NS_HTTP_HEADER_SEPS, &rawBuffer)) {
      nsDependentCString header(token);
      MOZ_ASSERT(!header.EqualsLiteral("*"),
                 "We should have already caught this in "
                 "TypeUtils::ToPCacheResponseWithoutBody()");

      ErrorResult errorResult;
      nsAutoCString queryValue;
      queryHeaders->Get(header, queryValue, errorResult);
      if (errorResult.Failed()) {
        errorResult.SuppressException();
        MOZ_ASSERT(queryValue.IsEmpty());
      }

      nsAutoCString cachedValue;
      cachedHeaders->Get(header, cachedValue, errorResult);
      if (errorResult.Failed()) {
        errorResult.SuppressException();
        MOZ_ASSERT(cachedValue.IsEmpty());
      }

      if (queryValue != cachedValue) {
        varyHeadersMatch = false;
        bailOut = true;
        break;
      }
    }

    if (bailOut) {
      break;
    }
  }

  *aSuccessOut = varyHeadersMatch;
  return rv;
}

nsresult
DeleteEntries(mozIStorageConnection* aConn,
              const nsTArray<EntryId>& aEntryIdList,
              nsTArray<nsID>& aDeletedBodyIdListOut,
              nsTArray<IdCount>& aDeletedSecurityIdListOut,
              uint32_t aPos, int32_t aLen)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  if (aEntryIdList.IsEmpty()) {
    return NS_OK;
  }

  MOZ_ASSERT(aPos < aEntryIdList.Length());

  if (aLen < 0) {
    aLen = aEntryIdList.Length() - aPos;
  }

  // Sqlite limits the number of entries allowed for an IN clause,
  // so split up larger operations.
  if (aLen > kMaxEntriesPerStatement) {
    uint32_t curPos = aPos;
    int32_t remaining = aLen;
    while (remaining > 0) {
      int32_t max = kMaxEntriesPerStatement;
      int32_t curLen = std::min(max, remaining);
      nsresult rv = DeleteEntries(aConn, aEntryIdList, aDeletedBodyIdListOut,
                                  aDeletedSecurityIdListOut, curPos, curLen);
      if (NS_FAILED(rv)) { return rv; }

      curPos += curLen;
      remaining -= curLen;
    }
    return NS_OK;
  }

  nsCOMPtr<mozIStorageStatement> state;
  nsAutoCString query(
    "SELECT request_body_id, response_body_id, response_security_info_id "
    "FROM entries WHERE id IN ("
  );
  AppendListParamsToQuery(query, aEntryIdList, aPos, aLen);
  query.AppendLiteral(")");

  nsresult rv = aConn->CreateStatement(query, getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = BindListParamsToQuery(state, aEntryIdList, aPos, aLen);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool hasMoreData = false;
  while (NS_SUCCEEDED(state->ExecuteStep(&hasMoreData)) && hasMoreData) {
    // extract 0 to 2 nsID structs per row
    for (uint32_t i = 0; i < 2; ++i) {
      bool isNull = false;

      rv = state->GetIsNull(i, &isNull);
      if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

      if (!isNull) {
        nsID id;
        rv = ExtractId(state, i, &id);
        if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
        aDeletedBodyIdListOut.AppendElement(id);
      }
    }

    // and then a possible third entry for the security id
    bool isNull = false;
    rv = state->GetIsNull(2, &isNull);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    if (!isNull) {
      int32_t securityId = -1;
      rv = state->GetInt32(2, &securityId);
      if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

      // First try to increment the count for this ID if we're already
      // seen it
      bool found = false;
      for (uint32_t i = 0; i < aDeletedSecurityIdListOut.Length(); ++i) {
        if (aDeletedSecurityIdListOut[i].mId == securityId) {
          found = true;
          aDeletedSecurityIdListOut[i].mCount += 1;
          break;
        }
      }

      // Otherwise add a new entry for this ID with a count of 1
      if (!found) {
        aDeletedSecurityIdListOut.AppendElement(IdCount(securityId));
      }
    }
  }

  // Dependent records removed via ON DELETE CASCADE

  query = NS_LITERAL_CSTRING(
    "DELETE FROM entries WHERE id IN ("
  );
  AppendListParamsToQuery(query, aEntryIdList, aPos, aLen);
  query.AppendLiteral(")");

  rv = aConn->CreateStatement(query, getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = BindListParamsToQuery(state, aEntryIdList, aPos, aLen);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

nsresult
InsertSecurityInfo(mozIStorageConnection* aConn, nsICryptoHash* aCrypto,
                   const nsACString& aData, int32_t *aIdOut)
{
  MOZ_ASSERT(aConn);
  MOZ_ASSERT(aCrypto);
  MOZ_ASSERT(aIdOut);
  MOZ_ASSERT(!aData.IsEmpty());

  // We want to use an index to find existing security blobs, but indexing
  // the full blob would be quite expensive.  Instead, we index a small
  // hash value.  Calculate this hash as the first 8 bytes of the SHA1 of
  // the full data.
  nsAutoCString hash;
  nsresult rv = HashCString(aCrypto, aData, hash);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Next, search for an existing entry for this blob by comparing the hash
  // value first and then the full data.  SQLite is smart enough to use
  // the index on the hash to search the table before doing the expensive
  // comparison of the large data column.  (This was verified with EXPLAIN.)
  nsCOMPtr<mozIStorageStatement> state;
  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    // Note that hash and data are blobs, but we can use = here since the
    // columns are NOT NULL.
    "SELECT id, refcount FROM security_info WHERE hash=:hash AND data=:data;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindUTF8StringAsBlobByName(NS_LITERAL_CSTRING("hash"), hash);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindUTF8StringAsBlobByName(NS_LITERAL_CSTRING("data"), aData);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool hasMoreData = false;
  rv = state->ExecuteStep(&hasMoreData);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // This security info blob is already in the database
  if (hasMoreData) {
    // get the existing security blob id to return
    rv = state->GetInt32(0, aIdOut);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    int32_t refcount = -1;
    rv = state->GetInt32(1, &refcount);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    // But first, update the refcount in the database.
    refcount += 1;

    rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
      "UPDATE security_info SET refcount=:refcount WHERE id=:id;"
    ), getter_AddRefs(state));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = state->BindInt32ByName(NS_LITERAL_CSTRING("refcount"), refcount);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = state->BindInt32ByName(NS_LITERAL_CSTRING("id"), *aIdOut);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = state->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    return NS_OK;
  }

  // This is a new security info blob.  Create a new row in the security table
  // with an initial refcount of 1.
  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO security_info (hash, data, refcount) VALUES (:hash, :data, 1);"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindUTF8StringAsBlobByName(NS_LITERAL_CSTRING("hash"), hash);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindUTF8StringAsBlobByName(NS_LITERAL_CSTRING("data"), aData);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT last_insert_rowid()"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  hasMoreData = false;
  rv = state->ExecuteStep(&hasMoreData);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->GetInt32(0, aIdOut);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return NS_OK;
}

nsresult
DeleteSecurityInfo(mozIStorageConnection* aConn, int32_t aId, int32_t aCount)
{
  // First, we need to determine the current refcount for this security blob.
  nsCOMPtr<mozIStorageStatement> state;
  nsresult rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT refcount FROM security_info WHERE id=:id;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32ByName(NS_LITERAL_CSTRING("id"), aId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool hasMoreData = false;
  rv = state->ExecuteStep(&hasMoreData);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  int32_t refcount = -1;
  rv = state->GetInt32(0, &refcount);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  MOZ_ASSERT(refcount >= aCount);

  // Next, calculate the new refcount
  int32_t newCount = refcount - aCount;

  // If the last reference to this security blob was removed we can
  // just remove the entire row.
  if (newCount == 0) {
    rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
      "DELETE FROM security_info WHERE id=:id;"
    ), getter_AddRefs(state));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = state->BindInt32ByName(NS_LITERAL_CSTRING("id"), aId);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = state->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    return NS_OK;
  }

  // Otherwise update the refcount in the table to reflect the reduced
  // number of references to the security blob.
  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE security_info SET refcount=:refcount WHERE id=:id;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32ByName(NS_LITERAL_CSTRING("refcount"), newCount);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32ByName(NS_LITERAL_CSTRING("id"), aId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return NS_OK;
}

nsresult
DeleteSecurityInfoList(mozIStorageConnection* aConn,
                        const nsTArray<IdCount>& aDeletedStorageIdList)
{
  for (uint32_t i = 0; i < aDeletedStorageIdList.Length(); ++i) {
    nsresult rv = DeleteSecurityInfo(aConn, aDeletedStorageIdList[i].mId,
                                     aDeletedStorageIdList[i].mCount);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  }

  return NS_OK;
}

nsresult
InsertEntry(mozIStorageConnection* aConn, CacheId aCacheId,
            const CacheRequest& aRequest,
            const nsID* aRequestBodyId,
            const CacheResponse& aResponse,
            const nsID* aResponseBodyId)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  nsresult rv = NS_OK;

  nsCOMPtr<nsICryptoHash> crypto =
    do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  int32_t securityId = -1;
  if (!aResponse.channelInfo().securityInfo().IsEmpty()) {
    rv = InsertSecurityInfo(aConn, crypto,
                            aResponse.channelInfo().securityInfo(),
                            &securityId);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  }

  nsCOMPtr<mozIStorageStatement> state;
  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO entries ("
      "request_method, "
      "request_url_no_query, "
      "request_url_no_query_hash, "
      "request_url_query, "
      "request_url_query_hash, "
      "request_url_fragment, "
      "request_referrer, "
      "request_referrer_policy, "
      "request_headers_guard, "
      "request_mode, "
      "request_credentials, "
      "request_contentpolicytype, "
      "request_cache, "
      "request_redirect, "
      "request_integrity, "
      "request_body_id, "
      "response_type, "
      "response_status, "
      "response_status_text, "
      "response_headers_guard, "
      "response_body_id, "
      "response_security_info_id, "
      "response_principal_info, "
      "cache_id "
    ") VALUES ("
      ":request_method, "
      ":request_url_no_query, "
      ":request_url_no_query_hash, "
      ":request_url_query, "
      ":request_url_query_hash, "
      ":request_url_fragment, "
      ":request_referrer, "
      ":request_referrer_policy, "
      ":request_headers_guard, "
      ":request_mode, "
      ":request_credentials, "
      ":request_contentpolicytype, "
      ":request_cache, "
      ":request_redirect, "
      ":request_integrity, "
      ":request_body_id, "
      ":response_type, "
      ":response_status, "
      ":response_status_text, "
      ":response_headers_guard, "
      ":response_body_id, "
      ":response_security_info_id, "
      ":response_principal_info, "
      ":cache_id "
    ");"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindUTF8StringByName(NS_LITERAL_CSTRING("request_method"),
                                   aRequest.method());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindUTF8StringByName(NS_LITERAL_CSTRING("request_url_no_query"),
                                   aRequest.urlWithoutQuery());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsAutoCString urlWithoutQueryHash;
  rv = HashCString(crypto, aRequest.urlWithoutQuery(), urlWithoutQueryHash);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindUTF8StringAsBlobByName(
    NS_LITERAL_CSTRING("request_url_no_query_hash"), urlWithoutQueryHash);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindUTF8StringByName(NS_LITERAL_CSTRING("request_url_query"),
                                   aRequest.urlQuery());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsAutoCString urlQueryHash;
  rv = HashCString(crypto, aRequest.urlQuery(), urlQueryHash);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  rv = state->BindUTF8StringAsBlobByName(
    NS_LITERAL_CSTRING("request_url_query_hash"), urlQueryHash);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  rv = state->BindUTF8StringByName(NS_LITERAL_CSTRING("request_url_fragment"),
                                   aRequest.urlFragment());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindStringByName(NS_LITERAL_CSTRING("request_referrer"),
                               aRequest.referrer());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  rv = state->BindInt32ByName(NS_LITERAL_CSTRING("request_referrer_policy"),
                              static_cast<int32_t>(aRequest.referrerPolicy()));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  rv = state->BindInt32ByName(NS_LITERAL_CSTRING("request_headers_guard"),
    static_cast<int32_t>(aRequest.headersGuard()));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32ByName(NS_LITERAL_CSTRING("request_mode"),
                              static_cast<int32_t>(aRequest.mode()));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32ByName(NS_LITERAL_CSTRING("request_credentials"),
    static_cast<int32_t>(aRequest.credentials()));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32ByName(NS_LITERAL_CSTRING("request_contentpolicytype"),
    static_cast<int32_t>(aRequest.contentPolicyType()));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32ByName(NS_LITERAL_CSTRING("request_cache"),
    static_cast<int32_t>(aRequest.requestCache()));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32ByName(NS_LITERAL_CSTRING("request_redirect"),
    static_cast<int32_t>(aRequest.requestRedirect()));

  rv = state->BindStringByName(NS_LITERAL_CSTRING("request_integrity"),
                               aRequest.integrity());

  rv = BindId(state, NS_LITERAL_CSTRING("request_body_id"), aRequestBodyId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32ByName(NS_LITERAL_CSTRING("response_type"),
                              static_cast<int32_t>(aResponse.type()));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32ByName(NS_LITERAL_CSTRING("response_status"),
                              aResponse.status());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindUTF8StringByName(NS_LITERAL_CSTRING("response_status_text"),
                                   aResponse.statusText());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32ByName(NS_LITERAL_CSTRING("response_headers_guard"),
    static_cast<int32_t>(aResponse.headersGuard()));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = BindId(state, NS_LITERAL_CSTRING("response_body_id"), aResponseBodyId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  if (aResponse.channelInfo().securityInfo().IsEmpty()) {
    rv = state->BindNullByName(NS_LITERAL_CSTRING("response_security_info_id"));
  } else {
    rv = state->BindInt32ByName(NS_LITERAL_CSTRING("response_security_info_id"),
                                securityId);
  }
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsAutoCString serializedInfo;
  // We only allow content serviceworkers right now.
  if (aResponse.principalInfo().type() == mozilla::ipc::OptionalPrincipalInfo::TPrincipalInfo) {
    const mozilla::ipc::PrincipalInfo& principalInfo =
      aResponse.principalInfo().get_PrincipalInfo();
    MOZ_ASSERT(principalInfo.type() == mozilla::ipc::PrincipalInfo::TContentPrincipalInfo);
    const mozilla::ipc::ContentPrincipalInfo& cInfo =
      principalInfo.get_ContentPrincipalInfo();

    serializedInfo.Append(cInfo.spec());

    nsAutoCString suffix;
    cInfo.attrs().CreateSuffix(suffix);
    serializedInfo.Append(suffix);
  }

  rv = state->BindUTF8StringByName(NS_LITERAL_CSTRING("response_principal_info"),
                                   serializedInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt64ByName(NS_LITERAL_CSTRING("cache_id"), aCacheId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT last_insert_rowid()"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool hasMoreData = false;
  rv = state->ExecuteStep(&hasMoreData);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  int32_t entryId;
  rv = state->GetInt32(0, &entryId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO request_headers ("
      "name, "
      "value, "
      "entry_id "
    ") VALUES (:name, :value, :entry_id)"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  const nsTArray<HeadersEntry>& requestHeaders = aRequest.headers();
  for (uint32_t i = 0; i < requestHeaders.Length(); ++i) {
    rv = state->BindUTF8StringByName(NS_LITERAL_CSTRING("name"),
                                     requestHeaders[i].name());
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = state->BindUTF8StringByName(NS_LITERAL_CSTRING("value"),
                                     requestHeaders[i].value());
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = state->BindInt32ByName(NS_LITERAL_CSTRING("entry_id"), entryId);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = state->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  }

  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO response_headers ("
      "name, "
      "value, "
      "entry_id "
    ") VALUES (:name, :value, :entry_id)"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  const nsTArray<HeadersEntry>& responseHeaders = aResponse.headers();
  for (uint32_t i = 0; i < responseHeaders.Length(); ++i) {
    rv = state->BindUTF8StringByName(NS_LITERAL_CSTRING("name"),
                                     responseHeaders[i].name());
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = state->BindUTF8StringByName(NS_LITERAL_CSTRING("value"),
                                     responseHeaders[i].value());
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = state->BindInt32ByName(NS_LITERAL_CSTRING("entry_id"), entryId);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = state->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  }

  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO response_url_list ("
      "url, "
      "entry_id "
    ") VALUES (:url, :entry_id)"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  const nsTArray<nsCString>& responseUrlList = aResponse.urlList();
  for (uint32_t i = 0; i < responseUrlList.Length(); ++i) {
    rv = state->BindUTF8StringByName(NS_LITERAL_CSTRING("url"),
                                     responseUrlList[i]);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = state->BindInt64ByName(NS_LITERAL_CSTRING("entry_id"), entryId);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = state->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  }

  return rv;
}

nsresult
ReadResponse(mozIStorageConnection* aConn, EntryId aEntryId,
             SavedResponse* aSavedResponseOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);
  MOZ_ASSERT(aSavedResponseOut);

  nsCOMPtr<mozIStorageStatement> state;
  nsresult rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT "
      "entries.response_type, "
      "entries.response_status, "
      "entries.response_status_text, "
      "entries.response_headers_guard, "
      "entries.response_body_id, "
      "entries.response_principal_info, "
      "security_info.data "
    "FROM entries "
    "LEFT OUTER JOIN security_info "
    "ON entries.response_security_info_id=security_info.id "
    "WHERE entries.id=:id;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32ByName(NS_LITERAL_CSTRING("id"), aEntryId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool hasMoreData = false;
  rv = state->ExecuteStep(&hasMoreData);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  int32_t type;
  rv = state->GetInt32(0, &type);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  aSavedResponseOut->mValue.type() = static_cast<ResponseType>(type);

  int32_t status;
  rv = state->GetInt32(1, &status);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  aSavedResponseOut->mValue.status() = status;

  rv = state->GetUTF8String(2, aSavedResponseOut->mValue.statusText());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  int32_t guard;
  rv = state->GetInt32(3, &guard);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  aSavedResponseOut->mValue.headersGuard() =
    static_cast<HeadersGuardEnum>(guard);

  bool nullBody = false;
  rv = state->GetIsNull(4, &nullBody);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  aSavedResponseOut->mHasBodyId = !nullBody;

  if (aSavedResponseOut->mHasBodyId) {
    rv = ExtractId(state, 4, &aSavedResponseOut->mBodyId);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  }

  nsAutoCString serializedInfo;
  rv = state->GetUTF8String(5, serializedInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  aSavedResponseOut->mValue.principalInfo() = void_t();
  if (!serializedInfo.IsEmpty()) {
    nsAutoCString originNoSuffix;
    PrincipalOriginAttributes attrs;
    if (!attrs.PopulateFromOrigin(serializedInfo, originNoSuffix)) {
      NS_WARNING("Something went wrong parsing a serialized principal!");
      return NS_ERROR_FAILURE;
    }

    aSavedResponseOut->mValue.principalInfo() =
      mozilla::ipc::ContentPrincipalInfo(attrs, originNoSuffix);
  }

  rv = state->GetBlobAsUTF8String(6, aSavedResponseOut->mValue.channelInfo().securityInfo());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT "
      "name, "
      "value "
    "FROM response_headers "
    "WHERE entry_id=:entry_id;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32ByName(NS_LITERAL_CSTRING("entry_id"), aEntryId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  while (NS_SUCCEEDED(state->ExecuteStep(&hasMoreData)) && hasMoreData) {
    HeadersEntry header;

    rv = state->GetUTF8String(0, header.name());
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = state->GetUTF8String(1, header.value());
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    aSavedResponseOut->mValue.headers().AppendElement(header);
  }

  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT "
      "url "
    "FROM response_url_list "
    "WHERE entry_id=:entry_id;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32ByName(NS_LITERAL_CSTRING("entry_id"), aEntryId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  while (NS_SUCCEEDED(state->ExecuteStep(&hasMoreData)) && hasMoreData) {
    nsCString url;

    rv = state->GetUTF8String(0, url);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    aSavedResponseOut->mValue.urlList().AppendElement(url);
  }

  return rv;
}

nsresult
ReadRequest(mozIStorageConnection* aConn, EntryId aEntryId,
            SavedRequest* aSavedRequestOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);
  MOZ_ASSERT(aSavedRequestOut);
  nsCOMPtr<mozIStorageStatement> state;
  nsresult rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT "
      "request_method, "
      "request_url_no_query, "
      "request_url_query, "
      "request_url_fragment, "
      "request_referrer, "
      "request_referrer_policy, "
      "request_headers_guard, "
      "request_mode, "
      "request_credentials, "
      "request_contentpolicytype, "
      "request_cache, "
      "request_redirect, "
      "request_integrity, "
      "request_body_id "
    "FROM entries "
    "WHERE id=:id;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32ByName(NS_LITERAL_CSTRING("id"), aEntryId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool hasMoreData = false;
  rv = state->ExecuteStep(&hasMoreData);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->GetUTF8String(0, aSavedRequestOut->mValue.method());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  rv = state->GetUTF8String(1, aSavedRequestOut->mValue.urlWithoutQuery());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  rv = state->GetUTF8String(2, aSavedRequestOut->mValue.urlQuery());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  rv = state->GetUTF8String(3, aSavedRequestOut->mValue.urlFragment());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  rv = state->GetString(4, aSavedRequestOut->mValue.referrer());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  int32_t referrerPolicy;
  rv = state->GetInt32(5, &referrerPolicy);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  aSavedRequestOut->mValue.referrerPolicy() =
    static_cast<ReferrerPolicy>(referrerPolicy);
  int32_t guard;
  rv = state->GetInt32(6, &guard);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  aSavedRequestOut->mValue.headersGuard() =
    static_cast<HeadersGuardEnum>(guard);
  int32_t mode;
  rv = state->GetInt32(7, &mode);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  aSavedRequestOut->mValue.mode() = static_cast<RequestMode>(mode);
  int32_t credentials;
  rv = state->GetInt32(8, &credentials);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  aSavedRequestOut->mValue.credentials() =
    static_cast<RequestCredentials>(credentials);
  int32_t requestContentPolicyType;
  rv = state->GetInt32(9, &requestContentPolicyType);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  aSavedRequestOut->mValue.contentPolicyType() =
    static_cast<nsContentPolicyType>(requestContentPolicyType);
  int32_t requestCache;
  rv = state->GetInt32(10, &requestCache);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  aSavedRequestOut->mValue.requestCache() =
    static_cast<RequestCache>(requestCache);
  int32_t requestRedirect;
  rv = state->GetInt32(11, &requestRedirect);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  aSavedRequestOut->mValue.requestRedirect() =
    static_cast<RequestRedirect>(requestRedirect);
  rv = state->GetString(12, aSavedRequestOut->mValue.integrity());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  bool nullBody = false;
  rv = state->GetIsNull(13, &nullBody);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  aSavedRequestOut->mHasBodyId = !nullBody;
  if (aSavedRequestOut->mHasBodyId) {
    rv = ExtractId(state, 13, &aSavedRequestOut->mBodyId);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  }
  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT "
      "name, "
      "value "
    "FROM request_headers "
    "WHERE entry_id=:entry_id;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32ByName(NS_LITERAL_CSTRING("entry_id"), aEntryId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  while (NS_SUCCEEDED(state->ExecuteStep(&hasMoreData)) && hasMoreData) {
    HeadersEntry header;

    rv = state->GetUTF8String(0, header.name());
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = state->GetUTF8String(1, header.value());
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    aSavedRequestOut->mValue.headers().AppendElement(header);
  }

  return rv;
}

void
AppendListParamsToQuery(nsACString& aQuery,
                        const nsTArray<EntryId>& aEntryIdList,
                        uint32_t aPos, int32_t aLen)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT((aPos + aLen) <= aEntryIdList.Length());
  for (int32_t i = aPos; i < aLen; ++i) {
    if (i == 0) {
      aQuery.AppendLiteral("?");
    } else {
      aQuery.AppendLiteral(",?");
    }
  }
}

nsresult
BindListParamsToQuery(mozIStorageStatement* aState,
                      const nsTArray<EntryId>& aEntryIdList,
                      uint32_t aPos, int32_t aLen)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT((aPos + aLen) <= aEntryIdList.Length());
  for (int32_t i = aPos; i < aLen; ++i) {
    nsresult rv = aState->BindInt32ByIndex(i, aEntryIdList[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

nsresult
BindId(mozIStorageStatement* aState, const nsACString& aName, const nsID* aId)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aState);
  nsresult rv;

  if (!aId) {
    rv = aState->BindNullByName(aName);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
    return rv;
  }

  char idBuf[NSID_LENGTH];
  aId->ToProvidedString(idBuf);
  rv = aState->BindUTF8StringByName(aName, nsDependentCString(idBuf));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

nsresult
ExtractId(mozIStorageStatement* aState, uint32_t aPos, nsID* aIdOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aState);
  MOZ_ASSERT(aIdOut);

  nsAutoCString idString;
  nsresult rv = aState->GetUTF8String(aPos, idString);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool success = aIdOut->Parse(idString.get());
  if (NS_WARN_IF(!success)) { return NS_ERROR_UNEXPECTED; }

  return rv;
}

nsresult
CreateAndBindKeyStatement(mozIStorageConnection* aConn,
                          const char* aQueryFormat,
                          const nsAString& aKey,
                          mozIStorageStatement** aStateOut)
{
  MOZ_ASSERT(aConn);
  MOZ_ASSERT(aQueryFormat);
  MOZ_ASSERT(aStateOut);

  // The key is stored as a blob to avoid encoding issues.  An empty string
  // is mapped to NULL for blobs.  Normally we would just write the query
  // as "key IS :key" to do the proper NULL checking, but that prevents
  // sqlite from using the key index.  Therefore use "IS NULL" explicitly
  // if the key is empty, otherwise use "=:key" so that sqlite uses the
  // index.
  const char* constraint = nullptr;
  if (aKey.IsEmpty()) {
    constraint = "key IS NULL";
  } else {
    constraint = "key=:key";
  }

  nsPrintfCString query(aQueryFormat, constraint);

  nsCOMPtr<mozIStorageStatement> state;
  nsresult rv = aConn->CreateStatement(query, getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  if (!aKey.IsEmpty()) {
    rv = state->BindStringAsBlobByName(NS_LITERAL_CSTRING("key"), aKey);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  }

  state.forget(aStateOut);

  return rv;
}

nsresult
HashCString(nsICryptoHash* aCrypto, const nsACString& aIn, nsACString& aOut)
{
  MOZ_ASSERT(aCrypto);

  nsresult rv = aCrypto->Init(nsICryptoHash::SHA1);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = aCrypto->Update(reinterpret_cast<const uint8_t*>(aIn.BeginReading()),
                       aIn.Length());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsAutoCString fullHash;
  rv = aCrypto->Finish(false /* based64 result */, fullHash);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  aOut = Substring(fullHash, 0, 8);
  return rv;
}

} // namespace

nsresult
IncrementalVacuum(mozIStorageConnection* aConn)
{
  // Determine how much free space is in the database.
  nsCOMPtr<mozIStorageStatement> state;
  nsresult rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "PRAGMA freelist_count;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool hasMoreData = false;
  rv = state->ExecuteStep(&hasMoreData);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  int32_t freePages = 0;
  rv = state->GetInt32(0, &freePages);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // We have a relatively small page size, so we want to be careful to avoid
  // fragmentation.  We already use a growth incremental which will cause
  // sqlite to allocate and release multiple pages at the same time.  We can
  // further reduce fragmentation by making our allocated chunks a bit
  // "sticky".  This is done by creating some hysteresis where we allocate
  // pages/chunks as soon as we need them, but we only release pages/chunks
  // when we have a large amount of free space.  This helps with the case
  // where a page is adding and remove resources causing it to dip back and
  // forth across a chunk boundary.
  //
  // So only proceed with releasing pages if we have more than our constant
  // threshold.
  if (freePages <= kMaxFreePages) {
    return NS_OK;
  }

  // Release the excess pages back to the sqlite VFS.  This may also release
  // chunks of multiple pages back to the OS.
  int32_t pagesToRelease = freePages - kMaxFreePages;

  rv = aConn->ExecuteSimpleSQL(nsPrintfCString(
    "PRAGMA incremental_vacuum(%d);", pagesToRelease
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Verify that our incremental vacuum actually did something
#ifdef DEBUG
  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "PRAGMA freelist_count;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  hasMoreData = false;
  rv = state->ExecuteStep(&hasMoreData);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  freePages = 0;
  rv = state->GetInt32(0, &freePages);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  MOZ_ASSERT(freePages <= kMaxFreePages);
#endif

  return NS_OK;
}

namespace {

#ifdef DEBUG
struct Expect
{
  // Expect exact SQL
  Expect(const char* aName, const char* aType, const char* aSql)
    : mName(aName)
    , mType(aType)
    , mSql(aSql)
    , mIgnoreSql(false)
  { }

  // Ignore SQL
  Expect(const char* aName, const char* aType)
    : mName(aName)
    , mType(aType)
    , mIgnoreSql(true)
  { }

  const nsCString mName;
  const nsCString mType;
  const nsCString mSql;
  const bool mIgnoreSql;
};
#endif

nsresult
Validate(mozIStorageConnection* aConn)
{
  int32_t schemaVersion;
  nsresult rv = aConn->GetSchemaVersion(&schemaVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  if (NS_WARN_IF(schemaVersion != kLatestSchemaVersion)) {
    return NS_ERROR_FAILURE;
  }

#ifdef DEBUG
  // This is the schema we expect the database at the latest version to
  // contain.  Update this list if you add a new table or index.
  Expect expect[] = {
    Expect("caches", "table", kTableCaches),
    Expect("sqlite_sequence", "table"), // auto-gen by sqlite
    Expect("security_info", "table", kTableSecurityInfo),
    Expect("security_info_hash_index", "index", kIndexSecurityInfoHash),
    Expect("entries", "table", kTableEntries),
    Expect("entries_request_match_index", "index", kIndexEntriesRequest),
    Expect("request_headers", "table", kTableRequestHeaders),
    Expect("response_headers", "table", kTableResponseHeaders),
    Expect("response_headers_name_index", "index", kIndexResponseHeadersName),
    Expect("response_url_list", "table", kTableResponseUrlList),
    Expect("storage", "table", kTableStorage),
    Expect("sqlite_autoindex_storage_1", "index"), // auto-gen by sqlite
  };
  const uint32_t expectLength = sizeof(expect) / sizeof(Expect);

  // Read the schema from the sqlite_master table and compare.
  nsCOMPtr<mozIStorageStatement> state;
  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT name, type, sql FROM sqlite_master;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool hasMoreData = false;
  while (NS_SUCCEEDED(state->ExecuteStep(&hasMoreData)) && hasMoreData) {
    nsAutoCString name;
    rv = state->GetUTF8String(0, name);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    nsAutoCString type;
    rv = state->GetUTF8String(1, type);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    nsAutoCString sql;
    rv = state->GetUTF8String(2, sql);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    bool foundMatch = false;
    for (uint32_t i = 0; i < expectLength; ++i) {
      if (name == expect[i].mName) {
        if (type != expect[i].mType) {
          NS_WARNING(nsPrintfCString("Unexpected type for Cache schema entry %s",
                     name.get()).get());
          return NS_ERROR_FAILURE;
        }

        if (!expect[i].mIgnoreSql && sql != expect[i].mSql) {
          NS_WARNING(nsPrintfCString("Unexpected SQL for Cache schema entry %s",
                     name.get()).get());
          return NS_ERROR_FAILURE;
        }

        foundMatch = true;
        break;
      }
    }

    if (NS_WARN_IF(!foundMatch)) {
      NS_WARNING(nsPrintfCString("Unexpected schema entry %s in Cache database",
                 name.get()).get());
      return NS_ERROR_FAILURE;
    }
  }
#endif

  return rv;
}

// -----
// Schema migration code
// -----

typedef nsresult (*MigrationFunc)(mozIStorageConnection*, bool&);
struct Migration
{
  constexpr Migration(int32_t aFromVersion, MigrationFunc aFunc)
    : mFromVersion(aFromVersion)
    , mFunc(aFunc)
  { }
  int32_t mFromVersion;
  MigrationFunc mFunc;
};

// Declare migration functions here.  Each function should upgrade
// the version by a single increment.  Don't skip versions.
nsresult MigrateFrom15To16(mozIStorageConnection* aConn, bool& aRewriteSchema);
nsresult MigrateFrom16To17(mozIStorageConnection* aConn, bool& aRewriteSchema);
nsresult MigrateFrom17To18(mozIStorageConnection* aConn, bool& aRewriteSchema);
nsresult MigrateFrom18To19(mozIStorageConnection* aConn, bool& aRewriteSchema);
nsresult MigrateFrom19To20(mozIStorageConnection* aConn, bool& aRewriteSchema);
nsresult MigrateFrom20To21(mozIStorageConnection* aConn, bool& aRewriteSchema);
nsresult MigrateFrom21To22(mozIStorageConnection* aConn, bool& aRewriteSchema);
nsresult MigrateFrom22To23(mozIStorageConnection* aConn, bool& aRewriteSchema);
nsresult MigrateFrom23To24(mozIStorageConnection* aConn, bool& aRewriteSchema);
// Configure migration functions to run for the given starting version.
Migration sMigrationList[] = {
  Migration(15, MigrateFrom15To16),
  Migration(16, MigrateFrom16To17),
  Migration(17, MigrateFrom17To18),
  Migration(18, MigrateFrom18To19),
  Migration(19, MigrateFrom19To20),
  Migration(20, MigrateFrom20To21),
  Migration(21, MigrateFrom21To22),
  Migration(22, MigrateFrom22To23),
  Migration(23, MigrateFrom23To24),
};
uint32_t sMigrationListLength = sizeof(sMigrationList) / sizeof(Migration);
nsresult
RewriteEntriesSchema(mozIStorageConnection* aConn)
{
  nsresult rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "PRAGMA writable_schema = ON"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsCOMPtr<mozIStorageStatement> state;
  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE sqlite_master SET sql=:sql WHERE name='entries'"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindUTF8StringByName(NS_LITERAL_CSTRING("sql"),
                                   nsDependentCString(kTableEntries));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "PRAGMA writable_schema = OFF"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

nsresult
Migrate(mozIStorageConnection* aConn)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  int32_t currentVersion = 0;
  nsresult rv = aConn->GetSchemaVersion(&currentVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool rewriteSchema = false;

  while (currentVersion < kLatestSchemaVersion) {
    // Wiping old databases is handled in DBAction because it requires
    // making a whole new mozIStorageConnection.  Make sure we don't
    // accidentally get here for one of those old databases.
    MOZ_ASSERT(currentVersion >= kFirstShippedSchemaVersion);

    for (uint32_t i = 0; i < sMigrationListLength; ++i) {
      if (sMigrationList[i].mFromVersion == currentVersion) {
        bool shouldRewrite = false;
        rv = sMigrationList[i].mFunc(aConn, shouldRewrite);
        if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
        if (shouldRewrite) {
          rewriteSchema = true;
        }
        break;
      }
    }

    DebugOnly<int32_t> lastVersion = currentVersion;
    rv = aConn->GetSchemaVersion(&currentVersion);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
    MOZ_ASSERT(currentVersion > lastVersion);
  }

  MOZ_ASSERT(currentVersion == kLatestSchemaVersion);

  if (rewriteSchema) {
    // Now overwrite the master SQL for the entries table to remove the column
    // default value.  This is also necessary for our Validate() method to
    // pass on this database.
    rv = RewriteEntriesSchema(aConn);
  }

  return rv;
}

nsresult MigrateFrom15To16(mozIStorageConnection* aConn, bool& aRewriteSchema)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  // Add the request_redirect column with a default value of "follow".  Note,
  // we only use a default value here because its required by ALTER TABLE and
  // we need to apply the default "follow" to existing records in the table.
  // We don't actually want to keep the default in the schema for future
  // INSERTs.
  nsresult rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "ALTER TABLE entries "
    "ADD COLUMN request_redirect INTEGER NOT NULL DEFAULT 0"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = aConn->SetSchemaVersion(16);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  aRewriteSchema = true;

  return rv;
}

nsresult
MigrateFrom16To17(mozIStorageConnection* aConn, bool& aRewriteSchema)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  // This migration path removes the response_redirected and
  // response_redirected_url columns from the entries table.  sqlite doesn't
  // support removing a column from a table using ALTER TABLE, so we need to
  // create a new table without those columns, fill it up with the existing
  // data, and then drop the original table and rename the new one to the old
  // one.

  // Create a new_entries table with the new fields as of version 17.
  nsresult rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE new_entries ("
      "id INTEGER NOT NULL PRIMARY KEY, "
      "request_method TEXT NOT NULL, "
      "request_url_no_query TEXT NOT NULL, "
      "request_url_no_query_hash BLOB NOT NULL, "
      "request_url_query TEXT NOT NULL, "
      "request_url_query_hash BLOB NOT NULL, "
      "request_referrer TEXT NOT NULL, "
      "request_headers_guard INTEGER NOT NULL, "
      "request_mode INTEGER NOT NULL, "
      "request_credentials INTEGER NOT NULL, "
      "request_contentpolicytype INTEGER NOT NULL, "
      "request_cache INTEGER NOT NULL, "
      "request_body_id TEXT NULL, "
      "response_type INTEGER NOT NULL, "
      "response_url TEXT NOT NULL, "
      "response_status INTEGER NOT NULL, "
      "response_status_text TEXT NOT NULL, "
      "response_headers_guard INTEGER NOT NULL, "
      "response_body_id TEXT NULL, "
      "response_security_info_id INTEGER NULL REFERENCES security_info(id), "
      "response_principal_info TEXT NOT NULL, "
      "cache_id INTEGER NOT NULL REFERENCES caches(id) ON DELETE CASCADE, "
      "request_redirect INTEGER NOT NULL"
    ")"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Copy all of the data to the newly created table.
  rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO new_entries ("
      "id, "
      "request_method, "
      "request_url_no_query, "
      "request_url_no_query_hash, "
      "request_url_query, "
      "request_url_query_hash, "
      "request_referrer, "
      "request_headers_guard, "
      "request_mode, "
      "request_credentials, "
      "request_contentpolicytype, "
      "request_cache, "
      "request_redirect, "
      "request_body_id, "
      "response_type, "
      "response_url, "
      "response_status, "
      "response_status_text, "
      "response_headers_guard, "
      "response_body_id, "
      "response_security_info_id, "
      "response_principal_info, "
      "cache_id "
    ") SELECT "
      "id, "
      "request_method, "
      "request_url_no_query, "
      "request_url_no_query_hash, "
      "request_url_query, "
      "request_url_query_hash, "
      "request_referrer, "
      "request_headers_guard, "
      "request_mode, "
      "request_credentials, "
      "request_contentpolicytype, "
      "request_cache, "
      "request_redirect, "
      "request_body_id, "
      "response_type, "
      "response_url, "
      "response_status, "
      "response_status_text, "
      "response_headers_guard, "
      "response_body_id, "
      "response_security_info_id, "
      "response_principal_info, "
      "cache_id "
    "FROM entries;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Remove the old table.
  rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE entries;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Rename new_entries to entries.
  rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "ALTER TABLE new_entries RENAME to entries;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Now, recreate our indices.
  rv = aConn->ExecuteSimpleSQL(nsDependentCString(kIndexEntriesRequest));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Revalidate the foreign key constraints, and ensure that there are no
  // violations.
  nsCOMPtr<mozIStorageStatement> state;
  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "PRAGMA foreign_key_check;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool hasMoreData = false;
  rv = state->ExecuteStep(&hasMoreData);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  if (NS_WARN_IF(hasMoreData)) { return NS_ERROR_FAILURE; }

  rv = aConn->SetSchemaVersion(17);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

nsresult
MigrateFrom17To18(mozIStorageConnection* aConn, bool& aRewriteSchema)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  // This migration is needed in order to remove "only-if-cached" RequestCache
  // values from the database.  This enum value was removed from the spec in
  // https://github.com/whatwg/fetch/issues/39 but we unfortunately happily
  // accepted this value in the Request constructor.
  //
  // There is no good value to upgrade this to, so we just stick to "default".

  static_assert(int(RequestCache::Default) == 0,
                "This is where the 0 below comes from!");
  nsresult rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "UPDATE entries SET request_cache = 0 "
      "WHERE request_cache = 5;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = aConn->SetSchemaVersion(18);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

nsresult
MigrateFrom18To19(mozIStorageConnection* aConn, bool& aRewriteSchema)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  // This migration is needed in order to update the RequestMode values for
  // Request objects corresponding to a navigation content policy type to
  // "navigate".

  static_assert(int(nsIContentPolicy::TYPE_DOCUMENT) == 6 &&
                int(nsIContentPolicy::TYPE_SUBDOCUMENT) == 7 &&
                int(nsIContentPolicy::TYPE_INTERNAL_FRAME) == 28 &&
                int(nsIContentPolicy::TYPE_INTERNAL_IFRAME) == 29 &&
                int(nsIContentPolicy::TYPE_REFRESH) == 8 &&
                int(RequestMode::Navigate) == 3,
                "This is where the numbers below come from!");
  nsresult rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "UPDATE entries SET request_mode = 3 "
      "WHERE request_contentpolicytype IN (6, 7, 28, 29, 8);"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = aConn->SetSchemaVersion(19);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

nsresult MigrateFrom19To20(mozIStorageConnection* aConn, bool& aRewriteSchema)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  // Add the request_referrer_policy column with a default value of
  // "no-referrer-when-downgrade".  Note, we only use a default value here
  // because its required by ALTER TABLE and we need to apply the default
  // "no-referrer-when-downgrade" to existing records in the table. We don't
  // actually want to keep the default in the schema for future INSERTs.
  nsresult rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "ALTER TABLE entries "
    "ADD COLUMN request_referrer_policy INTEGER NOT NULL DEFAULT 2"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = aConn->SetSchemaVersion(20);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  aRewriteSchema = true;

  return rv;
}

nsresult MigrateFrom20To21(mozIStorageConnection* aConn, bool& aRewriteSchema)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  // This migration creates response_url_list table to store response_url and
  // removes the response_url column from the entries table.
  // sqlite doesn't support removing a column from a table using ALTER TABLE,
  // so we need to create a new table without those columns, fill it up with the
  // existing data, and then drop the original table and rename the new one to
  // the old one.

  // Create a new_entries table with the new fields as of version 21.
  nsresult rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE new_entries ("
      "id INTEGER NOT NULL PRIMARY KEY, "
      "request_method TEXT NOT NULL, "
      "request_url_no_query TEXT NOT NULL, "
      "request_url_no_query_hash BLOB NOT NULL, "
      "request_url_query TEXT NOT NULL, "
      "request_url_query_hash BLOB NOT NULL, "
      "request_referrer TEXT NOT NULL, "
      "request_headers_guard INTEGER NOT NULL, "
      "request_mode INTEGER NOT NULL, "
      "request_credentials INTEGER NOT NULL, "
      "request_contentpolicytype INTEGER NOT NULL, "
      "request_cache INTEGER NOT NULL, "
      "request_body_id TEXT NULL, "
      "response_type INTEGER NOT NULL, "
      "response_status INTEGER NOT NULL, "
      "response_status_text TEXT NOT NULL, "
      "response_headers_guard INTEGER NOT NULL, "
      "response_body_id TEXT NULL, "
      "response_security_info_id INTEGER NULL REFERENCES security_info(id), "
      "response_principal_info TEXT NOT NULL, "
      "cache_id INTEGER NOT NULL REFERENCES caches(id) ON DELETE CASCADE, "
      "request_redirect INTEGER NOT NULL, "
      "request_referrer_policy INTEGER NOT NULL"
    ")"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Create a response_url_list table with the new fields as of version 21.
  rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE response_url_list ("
      "url TEXT NOT NULL, "
      "entry_id INTEGER NOT NULL REFERENCES entries(id) ON DELETE CASCADE"
    ")"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Copy all of the data to the newly created entries table.
  rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO new_entries ("
      "id, "
      "request_method, "
      "request_url_no_query, "
      "request_url_no_query_hash, "
      "request_url_query, "
      "request_url_query_hash, "
      "request_referrer, "
      "request_headers_guard, "
      "request_mode, "
      "request_credentials, "
      "request_contentpolicytype, "
      "request_cache, "
      "request_redirect, "
      "request_referrer_policy, "
      "request_body_id, "
      "response_type, "
      "response_status, "
      "response_status_text, "
      "response_headers_guard, "
      "response_body_id, "
      "response_security_info_id, "
      "response_principal_info, "
      "cache_id "
    ") SELECT "
      "id, "
      "request_method, "
      "request_url_no_query, "
      "request_url_no_query_hash, "
      "request_url_query, "
      "request_url_query_hash, "
      "request_referrer, "
      "request_headers_guard, "
      "request_mode, "
      "request_credentials, "
      "request_contentpolicytype, "
      "request_cache, "
      "request_redirect, "
      "request_referrer_policy, "
      "request_body_id, "
      "response_type, "
      "response_status, "
      "response_status_text, "
      "response_headers_guard, "
      "response_body_id, "
      "response_security_info_id, "
      "response_principal_info, "
      "cache_id "
    "FROM entries;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Copy reponse_url to the newly created response_url_list table.
  rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "INSERT INTO response_url_list ("
      "url, "
      "entry_id "
    ") SELECT "
      "response_url, "
      "id "
    "FROM entries;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Remove the old table.
  rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE entries;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Rename new_entries to entries.
  rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "ALTER TABLE new_entries RENAME to entries;"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Now, recreate our indices.
  rv = aConn->ExecuteSimpleSQL(nsDependentCString(kIndexEntriesRequest));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Revalidate the foreign key constraints, and ensure that there are no
  // violations.
  nsCOMPtr<mozIStorageStatement> state;
  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "PRAGMA foreign_key_check;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool hasMoreData = false;
  rv = state->ExecuteStep(&hasMoreData);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  if (NS_WARN_IF(hasMoreData)) { return NS_ERROR_FAILURE; }

  rv = aConn->SetSchemaVersion(21);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  aRewriteSchema = true;

  return rv;
}

nsresult MigrateFrom21To22(mozIStorageConnection* aConn, bool& aRewriteSchema)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  // Add the request_integrity column.
  nsresult rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "ALTER TABLE entries "
    "ADD COLUMN request_integrity TEXT NULL"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = aConn->SetSchemaVersion(22);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  aRewriteSchema = true;

  return rv;
}

nsresult MigrateFrom22To23(mozIStorageConnection* aConn, bool& aRewriteSchema)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  // The only change between 22 and 23 was a different snappy compression
  // format, but it's backwards-compatible.
  nsresult rv = aConn->SetSchemaVersion(23);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  return rv;
}
nsresult MigrateFrom23To24(mozIStorageConnection* aConn, bool& aRewriteSchema)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  // Add the request_url_fragment column.
  nsresult rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "ALTER TABLE entries "
    "ADD COLUMN request_url_fragment TEXT NOT NULL DEFAULT ''"
  ));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = aConn->SetSchemaVersion(24);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  aRewriteSchema = true;

  return rv;
}

} // anonymous namespace
} // namespace db
} // namespace cache
} // namespace dom
} // namespace mozilla
