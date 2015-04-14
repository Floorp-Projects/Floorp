/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/DBSchema.h"

#include "ipc/IPCMessageUtils.h"
#include "mozilla/dom/InternalHeaders.h"
#include "mozilla/dom/cache/CacheTypes.h"
#include "mozilla/dom/cache/SavedTypes.h"
#include "mozilla/dom/cache/Types.h"
#include "mozilla/dom/cache/TypeUtils.h"
#include "mozIStorageConnection.h"
#include "mozIStorageStatement.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsCRT.h"
#include "nsHttp.h"
#include "mozilla/dom/HeadersBinding.h"
#include "mozilla/dom/RequestBinding.h"
#include "mozilla/dom/ResponseBinding.h"
#include "nsIContentPolicy.h"

namespace mozilla {
namespace dom {
namespace cache {

const int32_t DBSchema::kMaxWipeSchemaVersion = 6;
const int32_t DBSchema::kLatestSchemaVersion = 6;
const int32_t DBSchema::kMaxEntriesPerStatement = 255;

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
static_assert(int(RequestMode::Same_origin) == 0 &&
              int(RequestMode::No_cors) == 1 &&
              int(RequestMode::Cors) == 2 &&
              int(RequestMode::Cors_with_forced_preflight) == 3 &&
              int(RequestMode::EndGuard_) == 4,
              "RequestMode values are as expected");
static_assert(int(RequestCredentials::Omit) == 0 &&
              int(RequestCredentials::Same_origin) == 1 &&
              int(RequestCredentials::Include) == 2 &&
              int(RequestCredentials::EndGuard_) == 3,
              "RequestCredentials values are as expected");
static_assert(int(RequestContext::Audio) == 0 &&
              int(RequestContext::Beacon) == 1 &&
              int(RequestContext::Cspreport) == 2 &&
              int(RequestContext::Download) == 3 &&
              int(RequestContext::Embed) == 4 &&
              int(RequestContext::Eventsource) == 5 &&
              int(RequestContext::Favicon) == 6 &&
              int(RequestContext::Fetch) == 7 &&
              int(RequestContext::Font) == 8 &&
              int(RequestContext::Form) == 9 &&
              int(RequestContext::Frame) == 10 &&
              int(RequestContext::Hyperlink) == 11 &&
              int(RequestContext::Iframe) == 12 &&
              int(RequestContext::Image) == 13 &&
              int(RequestContext::Imageset) == 14 &&
              int(RequestContext::Import) == 15 &&
              int(RequestContext::Internal) == 16 &&
              int(RequestContext::Location) == 17 &&
              int(RequestContext::Manifest) == 18 &&
              int(RequestContext::Object) == 19 &&
              int(RequestContext::Ping) == 20 &&
              int(RequestContext::Plugin) == 21 &&
              int(RequestContext::Prefetch) == 22 &&
              int(RequestContext::Script) == 23 &&
              int(RequestContext::Serviceworker) == 24 &&
              int(RequestContext::Sharedworker) == 25 &&
              int(RequestContext::Subresource) == 26 &&
              int(RequestContext::Style) == 27 &&
              int(RequestContext::Track) == 28 &&
              int(RequestContext::Video) == 29 &&
              int(RequestContext::Worker) == 30 &&
              int(RequestContext::Xmlhttprequest) == 31 &&
              int(RequestContext::Xslt) == 32,
              "RequestContext values are as expected");
static_assert(int(RequestCache::Default) == 0 &&
              int(RequestCache::No_store) == 1 &&
              int(RequestCache::Reload) == 2 &&
              int(RequestCache::No_cache) == 3 &&
              int(RequestCache::Force_cache) == 4 &&
              int(RequestCache::Only_if_cached) == 5 &&
              int(RequestCache::EndGuard_) == 6,
              "RequestCache values are as expected");
static_assert(int(ResponseType::Basic) == 0 &&
              int(ResponseType::Cors) == 1 &&
              int(ResponseType::Default) == 2 &&
              int(ResponseType::Error) == 3 &&
              int(ResponseType::Opaque) == 4 &&
              int(ResponseType::EndGuard_) == 5,
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
              nsIContentPolicy::TYPE_IMAGESET == 21,
              "nsContentPolicytType values are as expected");

using mozilla::void_t;

// static
nsresult
DBSchema::CreateSchema(mozIStorageConnection* aConn)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  nsAutoCString pragmas(
    // Enable auto-vaccum but in incremental mode in order to avoid doing a lot
    // of work at the end of each transaction.
    "PRAGMA auto_vacuum = INCREMENTAL; "
  );

  nsresult rv = aConn->ExecuteSimpleSQL(pragmas);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  int32_t schemaVersion;
  rv = aConn->GetSchemaVersion(&schemaVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  if (schemaVersion == kLatestSchemaVersion) {
    // We already have the correct schema, so just do an incremental vaccum and
    // get started.
    rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "PRAGMA incremental_vacuum;"));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    return rv;
  }

  if (!schemaVersion) {
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
    rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE caches ("
        "id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT "
      ");"
    ));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE entries ("
        "id INTEGER NOT NULL PRIMARY KEY, "
        "request_method TEXT NOT NULL, "
        "request_url TEXT NOT NULL, "
        "request_url_no_query TEXT NOT NULL, "
        "request_referrer TEXT NOT NULL, "
        "request_headers_guard INTEGER NOT NULL, "
        "request_mode INTEGER NOT NULL, "
        "request_credentials INTEGER NOT NULL, "
        "request_contentpolicytype INTEGER NOT NULL, "
        "request_context INTEGER NOT NULL, "
        "request_cache INTEGER NOT NULL, "
        "request_body_id TEXT NULL, "
        "response_type INTEGER NOT NULL, "
        "response_url TEXT NOT NULL, "
        "response_status INTEGER NOT NULL, "
        "response_status_text TEXT NOT NULL, "
        "response_headers_guard INTEGER NOT NULL, "
        "response_body_id TEXT NULL, "
        "response_security_info BLOB NULL, "
        "cache_id INTEGER NOT NULL REFERENCES caches(id) ON DELETE CASCADE"
      ");"
    ));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    // TODO: see if we can remove these indices on TEXT columns (bug 1110458)
    rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE INDEX entries_request_url_index "
                "ON entries (request_url);"
    ));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE INDEX entries_request_url_no_query_index "
                "ON entries (request_url_no_query);"
    ));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE request_headers ("
        "name TEXT NOT NULL, "
        "value TEXT NOT NULL, "
        "entry_id INTEGER NOT NULL REFERENCES entries(id) ON DELETE CASCADE"
      ");"
    ));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE response_headers ("
        "name TEXT NOT NULL, "
        "value TEXT NOT NULL, "
        "entry_id INTEGER NOT NULL REFERENCES entries(id) ON DELETE CASCADE"
      ");"
    ));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    // We need an index on response_headers, but not on request_headers,
    // because we quickly need to determine if a VARY header is present.
    rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE INDEX response_headers_name_index "
                "ON response_headers (name);"
    ));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = aConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE storage ("
        "namespace INTEGER NOT NULL, "
        "key TEXT NOT NULL, "
        "cache_id INTEGER NOT NULL REFERENCES caches(id), "
        "PRIMARY KEY(namespace, key) "
      ");"
    ));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = aConn->SetSchemaVersion(kLatestSchemaVersion);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = aConn->GetSchemaVersion(&schemaVersion);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  }

  if (schemaVersion != kLatestSchemaVersion) {
    return NS_ERROR_FAILURE;
  }

  return rv;
}

// static
nsresult
DBSchema::InitializeConnection(mozIStorageConnection* aConn)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  // This function needs to perform per-connection initialization tasks that
  // need to happen regardless of the schema.

  nsAutoCString pragmas(
#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_GONK)
    // Switch the journaling mode to TRUNCATE to avoid changing the directory
    // structure at the conclusion of every transaction for devices with slower
    // file systems.
    "PRAGMA journal_mode = TRUNCATE; "
#endif
    "PRAGMA foreign_keys = ON; "

    // Note, the default encoding of UTF-8 is preferred.  mozStorage does all
    // the work necessary to convert UTF-16 nsString values for us.  We don't
    // need ordering and the binary equality operations are correct.  So, do
    // NOT set PRAGMA encoding to UTF-16.
  );

  nsresult rv = aConn->ExecuteSimpleSQL(pragmas);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return NS_OK;
}

// static
nsresult
DBSchema::CreateCache(mozIStorageConnection* aConn, CacheId* aCacheIdOut)
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

// static
nsresult
DBSchema::DeleteCache(mozIStorageConnection* aConn, CacheId aCacheId,
                      nsTArray<nsID>& aDeletedBodyIdListOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  // Delete the bodies explicitly as we need to read out the body IDs
  // anyway.  These body IDs must be deleted one-by-one as content may
  // still be referencing them invidivually.
  nsAutoTArray<EntryId, 256> matches;
  nsresult rv = QueryAll(aConn, aCacheId, matches);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = DeleteEntries(aConn, matches, aDeletedBodyIdListOut);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Delete the remainder of the cache using cascade semantics.
  nsCOMPtr<mozIStorageStatement> state;
  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM caches WHERE id=?1;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt64Parameter(0, aCacheId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

// static
nsresult
DBSchema::IsCacheOrphaned(mozIStorageConnection* aConn,
                          CacheId aCacheId, bool* aOrphanedOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);
  MOZ_ASSERT(aOrphanedOut);

  // err on the side of not deleting user data
  *aOrphanedOut = false;

  nsCOMPtr<mozIStorageStatement> state;
  nsresult rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT COUNT(*) FROM storage WHERE cache_id=?1;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt64Parameter(0, aCacheId);
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

// static
nsresult
DBSchema::CacheMatch(mozIStorageConnection* aConn, CacheId aCacheId,
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

  nsAutoTArray<EntryId, 1> matches;
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

// static
nsresult
DBSchema::CacheMatchAll(mozIStorageConnection* aConn, CacheId aCacheId,
                        const CacheRequestOrVoid& aRequestOrVoid,
                        const CacheQueryParams& aParams,
                        nsTArray<SavedResponse>& aSavedResponsesOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);
  nsresult rv;

  nsAutoTArray<EntryId, 256> matches;
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

// static
nsresult
DBSchema::CachePut(mozIStorageConnection* aConn, CacheId aCacheId,
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
  nsAutoTArray<EntryId, 256> matches;
  nsresult rv = QueryCache(aConn, aCacheId, aRequest, params, matches);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = DeleteEntries(aConn, matches, aDeletedBodyIdListOut);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = InsertEntry(aConn, aCacheId, aRequest, aRequestBodyId, aResponse,
                   aResponseBodyId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

// static
nsresult
DBSchema::CacheDelete(mozIStorageConnection* aConn, CacheId aCacheId,
                      const CacheRequest& aRequest,
                      const CacheQueryParams& aParams,
                      nsTArray<nsID>& aDeletedBodyIdListOut, bool* aSuccessOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);
  MOZ_ASSERT(aSuccessOut);

  *aSuccessOut = false;

  nsAutoTArray<EntryId, 256> matches;
  nsresult rv = QueryCache(aConn, aCacheId, aRequest, aParams, matches);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  if (matches.IsEmpty()) {
    return rv;
  }

  rv = DeleteEntries(aConn, matches, aDeletedBodyIdListOut);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  *aSuccessOut = true;

  return rv;
}

// static
nsresult
DBSchema::CacheKeys(mozIStorageConnection* aConn, CacheId aCacheId,
                    const CacheRequestOrVoid& aRequestOrVoid,
                    const CacheQueryParams& aParams,
                    nsTArray<SavedRequest>& aSavedRequestsOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);
  nsresult rv;

  nsAutoTArray<EntryId, 256> matches;
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

// static
nsresult
DBSchema::StorageMatch(mozIStorageConnection* aConn,
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
    if (!foundCache) { return NS_ERROR_DOM_NOT_FOUND_ERR; }

    rv = CacheMatch(aConn, cacheId, aRequest, aParams, aFoundResponseOut,
                    aSavedResponseOut);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    return rv;
  }

  // Otherwise we need to get a list of all the cache IDs in this namespace.

  nsCOMPtr<mozIStorageStatement> state;
  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT cache_id FROM storage WHERE namespace=?1 ORDER BY rowid;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32Parameter(0, aNamespace);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsAutoTArray<CacheId, 32> cacheIdList;

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

// static
nsresult
DBSchema::StorageGetCacheId(mozIStorageConnection* aConn, Namespace aNamespace,
                            const nsAString& aKey, bool* aFoundCacheOut,
                            CacheId* aCacheIdOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);
  MOZ_ASSERT(aFoundCacheOut);
  MOZ_ASSERT(aCacheIdOut);

  *aFoundCacheOut = false;

  nsCOMPtr<mozIStorageStatement> state;
  nsresult rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT cache_id FROM storage WHERE namespace=?1 AND key=?2 ORDER BY rowid;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32Parameter(0, aNamespace);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindStringParameter(1, aKey);
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

// static
nsresult
DBSchema::StoragePutCache(mozIStorageConnection* aConn, Namespace aNamespace,
                          const nsAString& aKey, CacheId aCacheId)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  nsCOMPtr<mozIStorageStatement> state;
  nsresult rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO storage (namespace, key, cache_id) VALUES(?1, ?2, ?3);"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32Parameter(0, aNamespace);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindStringParameter(1, aKey);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt64Parameter(2, aCacheId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

// static
nsresult
DBSchema::StorageForgetCache(mozIStorageConnection* aConn, Namespace aNamespace,
                             const nsAString& aKey)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  nsCOMPtr<mozIStorageStatement> state;
  nsresult rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM storage WHERE namespace=?1 AND key=?2;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32Parameter(0, aNamespace);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindStringParameter(1, aKey);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->Execute();
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

// static
nsresult
DBSchema::StorageGetKeys(mozIStorageConnection* aConn, Namespace aNamespace,
                         nsTArray<nsString>& aKeysOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  nsCOMPtr<mozIStorageStatement> state;
  nsresult rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT key FROM storage WHERE namespace=?1 ORDER BY rowid;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32Parameter(0, aNamespace);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool hasMoreData = false;
  while (NS_SUCCEEDED(state->ExecuteStep(&hasMoreData)) && hasMoreData) {
    nsAutoString key;
    rv = state->GetString(0, key);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
    aKeysOut.AppendElement(key);
  }

  return rv;
}

// static
nsresult
DBSchema::QueryAll(mozIStorageConnection* aConn, CacheId aCacheId,
                   nsTArray<EntryId>& aEntryIdListOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  nsCOMPtr<mozIStorageStatement> state;
  nsresult rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id FROM entries WHERE cache_id=?1 ORDER BY id;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt64Parameter(0, aCacheId);
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

// static
nsresult
DBSchema::QueryCache(mozIStorageConnection* aConn, CacheId aCacheId,
                     const CacheRequest& aRequest,
                     const CacheQueryParams& aParams,
                     nsTArray<EntryId>& aEntryIdListOut,
                     uint32_t aMaxResults)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);
  MOZ_ASSERT(aMaxResults > 0);

  if (!aParams.ignoreMethod() && !aRequest.method().LowerCaseEqualsLiteral("get")
                              && !aRequest.method().LowerCaseEqualsLiteral("head"))
  {
    return NS_OK;
  }

  nsAutoCString query(
    "SELECT id, COUNT(response_headers.name) AS vary_count "
    "FROM entries "
    "LEFT OUTER JOIN response_headers ON entries.id=response_headers.entry_id "
                                    "AND response_headers.name='vary' "
    "WHERE entries.cache_id=?1 "
      "AND entries."
  );

  nsAutoString urlToMatch;
  if (aParams.ignoreSearch()) {
    urlToMatch = aRequest.urlWithoutQuery();
    query.AppendLiteral("request_url_no_query");
  } else {
    urlToMatch = aRequest.url();
    query.AppendLiteral("request_url");
  }

  query.AppendLiteral("=?2 GROUP BY entries.id ORDER BY entries.id;");

  nsCOMPtr<mozIStorageStatement> state;
  nsresult rv = aConn->CreateStatement(query, getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt64Parameter(0, aCacheId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindStringParameter(1, urlToMatch);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

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

// static
nsresult
DBSchema::MatchByVaryHeader(mozIStorageConnection* aConn,
                            const CacheRequest& aRequest,
                            EntryId entryId, bool* aSuccessOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  *aSuccessOut = false;

  nsCOMPtr<mozIStorageStatement> state;
  nsresult rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT value FROM response_headers "
    "WHERE name='vary' AND entry_id=?1;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32Parameter(0, entryId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsAutoTArray<nsCString, 8> varyValues;

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
    "WHERE entry_id=?1;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32Parameter(0, entryId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsRefPtr<InternalHeaders> cachedHeaders =
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
    if (errorResult.Failed()) { return errorResult.ErrorCode(); };
  }
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsRefPtr<InternalHeaders> queryHeaders =
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
      if (header.EqualsLiteral("*")) {
        continue;
      }

      ErrorResult errorResult;
      nsAutoCString queryValue;
      queryHeaders->Get(header, queryValue, errorResult);
      if (errorResult.Failed()) {
        errorResult.ClearMessage();
        MOZ_ASSERT(queryValue.IsEmpty());
      }

      nsAutoCString cachedValue;
      cachedHeaders->Get(header, cachedValue, errorResult);
      if (errorResult.Failed()) {
        errorResult.ClearMessage();
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

// static
nsresult
DBSchema::DeleteEntries(mozIStorageConnection* aConn,
                        const nsTArray<EntryId>& aEntryIdList,
                        nsTArray<nsID>& aDeletedBodyIdListOut,
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
                                  curPos, curLen);
      if (NS_FAILED(rv)) { return rv; }

      curPos += curLen;
      remaining -= curLen;
    }
    return NS_OK;
  }

  nsCOMPtr<mozIStorageStatement> state;
  nsAutoCString query(
    "SELECT request_body_id, response_body_id FROM entries WHERE id IN ("
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

// static
nsresult
DBSchema::InsertEntry(mozIStorageConnection* aConn, CacheId aCacheId,
                      const CacheRequest& aRequest,
                      const nsID* aRequestBodyId,
                      const CacheResponse& aResponse,
                      const nsID* aResponseBodyId)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);

  nsCOMPtr<mozIStorageStatement> state;
  nsresult rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO entries ("
      "request_method, "
      "request_url, "
      "request_url_no_query, "
      "request_referrer, "
      "request_headers_guard, "
      "request_mode, "
      "request_credentials, "
      "request_contentpolicytype, "
      "request_context, "
      "request_cache, "
      "request_body_id, "
      "response_type, "
      "response_url, "
      "response_status, "
      "response_status_text, "
      "response_headers_guard, "
      "response_body_id, "
      "response_security_info, "
      "cache_id "
    ") VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17, ?18, ?19)"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindUTF8StringParameter(0, aRequest.method());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindStringParameter(1, aRequest.url());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindStringParameter(2, aRequest.urlWithoutQuery());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindStringParameter(3, aRequest.referrer());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32Parameter(4,
    static_cast<int32_t>(aRequest.headersGuard()));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32Parameter(5, static_cast<int32_t>(aRequest.mode()));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32Parameter(6,
    static_cast<int32_t>(aRequest.credentials()));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32Parameter(7,
    static_cast<int32_t>(aRequest.contentPolicyType()));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32Parameter(8,
    static_cast<int32_t>(aRequest.context()));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32Parameter(9,
    static_cast<int32_t>(aRequest.requestCache()));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = BindId(state, 10, aRequestBodyId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32Parameter(11, static_cast<int32_t>(aResponse.type()));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindStringParameter(12, aResponse.url());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32Parameter(13, aResponse.status());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindUTF8StringParameter(14, aResponse.statusText());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32Parameter(15,
    static_cast<int32_t>(aResponse.headersGuard()));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = BindId(state, 16, aResponseBodyId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindBlobParameter(17, reinterpret_cast<const uint8_t*>
                                  (aResponse.securityInfo().get()),
                                aResponse.securityInfo().Length());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt64Parameter(18, aCacheId);
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
    ") VALUES (?1, ?2, ?3)"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  const nsTArray<HeadersEntry>& requestHeaders = aRequest.headers();
  for (uint32_t i = 0; i < requestHeaders.Length(); ++i) {
    rv = state->BindUTF8StringParameter(0, requestHeaders[i].name());
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = state->BindUTF8StringParameter(1, requestHeaders[i].value());
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = state->BindInt32Parameter(2, entryId);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = state->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  }

  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO response_headers ("
      "name, "
      "value, "
      "entry_id "
    ") VALUES (?1, ?2, ?3)"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  const nsTArray<HeadersEntry>& responseHeaders = aResponse.headers();
  for (uint32_t i = 0; i < responseHeaders.Length(); ++i) {
    rv = state->BindUTF8StringParameter(0, responseHeaders[i].name());
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = state->BindUTF8StringParameter(1, responseHeaders[i].value());
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = state->BindInt32Parameter(2, entryId);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = state->Execute();
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  }

  return rv;
}

// static
nsresult
DBSchema::ReadResponse(mozIStorageConnection* aConn, EntryId aEntryId,
                       SavedResponse* aSavedResponseOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);
  MOZ_ASSERT(aSavedResponseOut);

  nsCOMPtr<mozIStorageStatement> state;
  nsresult rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT "
      "response_type, "
      "response_url, "
      "response_status, "
      "response_status_text, "
      "response_headers_guard, "
      "response_body_id, "
      "response_security_info "
    "FROM entries "
    "WHERE id=?1;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32Parameter(0, aEntryId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool hasMoreData = false;
  rv = state->ExecuteStep(&hasMoreData);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  int32_t type;
  rv = state->GetInt32(0, &type);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  aSavedResponseOut->mValue.type() = static_cast<ResponseType>(type);

  rv = state->GetString(1, aSavedResponseOut->mValue.url());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  int32_t status;
  rv = state->GetInt32(2, &status);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  aSavedResponseOut->mValue.status() = status;

  rv = state->GetUTF8String(3, aSavedResponseOut->mValue.statusText());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  int32_t guard;
  rv = state->GetInt32(4, &guard);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  aSavedResponseOut->mValue.headersGuard() =
    static_cast<HeadersGuardEnum>(guard);

  bool nullBody = false;
  rv = state->GetIsNull(5, &nullBody);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  aSavedResponseOut->mHasBodyId = !nullBody;

  if (aSavedResponseOut->mHasBodyId) {
    rv = ExtractId(state, 5, &aSavedResponseOut->mBodyId);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  }

  uint8_t* data = nullptr;
  uint32_t dataLength = 0;
  rv = state->GetBlob(6, &dataLength, &data);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  aSavedResponseOut->mValue.securityInfo().Adopt(
    reinterpret_cast<char*>(data), dataLength);

  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT "
      "name, "
      "value "
    "FROM response_headers "
    "WHERE entry_id=?1;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32Parameter(0, aEntryId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  while (NS_SUCCEEDED(state->ExecuteStep(&hasMoreData)) && hasMoreData) {
    HeadersEntry header;

    rv = state->GetUTF8String(0, header.name());
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = state->GetUTF8String(1, header.value());
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    aSavedResponseOut->mValue.headers().AppendElement(header);
  }

  return rv;
}

// static
nsresult
DBSchema::ReadRequest(mozIStorageConnection* aConn, EntryId aEntryId,
                      SavedRequest* aSavedRequestOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aConn);
  MOZ_ASSERT(aSavedRequestOut);

  nsCOMPtr<mozIStorageStatement> state;
  nsresult rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT "
      "request_method, "
      "request_url, "
      "request_url_no_query, "
      "request_referrer, "
      "request_headers_guard, "
      "request_mode, "
      "request_credentials, "
      "request_contentpolicytype, "
      "request_context, "
      "request_cache, "
      "request_body_id "
    "FROM entries "
    "WHERE id=?1;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32Parameter(0, aEntryId);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool hasMoreData = false;
  rv = state->ExecuteStep(&hasMoreData);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->GetUTF8String(0, aSavedRequestOut->mValue.method());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->GetString(1, aSavedRequestOut->mValue.url());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->GetString(2, aSavedRequestOut->mValue.urlWithoutQuery());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->GetString(3, aSavedRequestOut->mValue.referrer());
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  int32_t guard;
  rv = state->GetInt32(4, &guard);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  aSavedRequestOut->mValue.headersGuard() =
    static_cast<HeadersGuardEnum>(guard);

  int32_t mode;
  rv = state->GetInt32(5, &mode);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  aSavedRequestOut->mValue.mode() = static_cast<RequestMode>(mode);

  int32_t credentials;
  rv = state->GetInt32(6, &credentials);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  aSavedRequestOut->mValue.credentials() =
    static_cast<RequestCredentials>(credentials);

  int32_t requestContentPolicyType;
  rv = state->GetInt32(7, &requestContentPolicyType);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  aSavedRequestOut->mValue.contentPolicyType() =
    static_cast<nsContentPolicyType>(requestContentPolicyType);

  int32_t requestContext;
  rv = state->GetInt32(8, &requestContext);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  aSavedRequestOut->mValue.context() =
    static_cast<RequestContext>(requestContext);

  int32_t requestCache;
  rv = state->GetInt32(9, &requestCache);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  aSavedRequestOut->mValue.requestCache() =
    static_cast<RequestCache>(requestCache);

  bool nullBody = false;
  rv = state->GetIsNull(10, &nullBody);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  aSavedRequestOut->mHasBodyId = !nullBody;

  if (aSavedRequestOut->mHasBodyId) {
    rv = ExtractId(state, 10, &aSavedRequestOut->mBodyId);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  }

  rv = aConn->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT "
      "name, "
      "value "
    "FROM request_headers "
    "WHERE entry_id=?1;"
  ), getter_AddRefs(state));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = state->BindInt32Parameter(0, aEntryId);
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

// static
void
DBSchema::AppendListParamsToQuery(nsACString& aQuery,
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

// static
nsresult
DBSchema::BindListParamsToQuery(mozIStorageStatement* aState,
                                const nsTArray<EntryId>& aEntryIdList,
                                uint32_t aPos, int32_t aLen)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT((aPos + aLen) <= aEntryIdList.Length());
  for (int32_t i = aPos; i < aLen; ++i) {
    nsresult rv = aState->BindInt32Parameter(i, aEntryIdList[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

// static
nsresult
DBSchema::BindId(mozIStorageStatement* aState, uint32_t aPos, const nsID* aId)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aState);
  nsresult rv;

  if (!aId) {
    rv = aState->BindNullParameter(aPos);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
    return rv;
  }

  char idBuf[NSID_LENGTH];
  aId->ToProvidedString(idBuf);
  rv = aState->BindUTF8StringParameter(aPos, nsAutoCString(idBuf));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

// static
nsresult
DBSchema::ExtractId(mozIStorageStatement* aState, uint32_t aPos, nsID* aIdOut)
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

} // namespace cache
} // namespace dom
} // namespace mozilla
