/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_DBSchema_h
#define mozilla_dom_cache_DBSchema_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/cache/Types.h"
#include "nsError.h"
#include "nsString.h"
#include "nsTArrayForwardDeclare.h"

class mozIStorageConnection;
class mozIStorageStatement;
struct nsID;

namespace mozilla {
namespace dom {
namespace cache {

class PCacheQueryParams;
class PCacheRequest;
class PCacheRequestOrVoid;
class PCacheResponse;
struct SavedRequest;
struct SavedResponse;

// TODO: remove static class and use functions in cache namespace (bug 1110485)
class DBSchema final
{
public:
  static nsresult CreateSchema(mozIStorageConnection* aConn);
  static nsresult InitializeConnection(mozIStorageConnection* aConn);

  static nsresult CreateCache(mozIStorageConnection* aConn,
                              CacheId* aCacheIdOut);
  // TODO: improve naming (confusing with CacheDelete) (bug 1110485)
  static nsresult DeleteCache(mozIStorageConnection* aConn, CacheId aCacheId,
                              nsTArray<nsID>& aDeletedBodyIdListOut);

  // TODO: Consider removing unused IsCacheOrphaned after writing cleanup code. (bug 1110446)
  static nsresult IsCacheOrphaned(mozIStorageConnection* aConn,
                                  CacheId aCacheId, bool* aOrphanedOut);

  static nsresult CacheMatch(mozIStorageConnection* aConn, CacheId aCacheId,
                             const PCacheRequest& aRequest,
                             const PCacheQueryParams& aParams,
                             bool* aFoundResponseOut,
                             SavedResponse* aSavedResponseOut);
  static nsresult CacheMatchAll(mozIStorageConnection* aConn, CacheId aCacheId,
                                const PCacheRequestOrVoid& aRequestOrVoid,
                                const PCacheQueryParams& aParams,
                                nsTArray<SavedResponse>& aSavedResponsesOut);
  static nsresult CachePut(mozIStorageConnection* aConn, CacheId aCacheId,
                           const PCacheRequest& aRequest,
                           const nsID* aRequestBodyId,
                           const PCacheResponse& aResponse,
                           const nsID* aResponseBodyId,
                           nsTArray<nsID>& aDeletedBodyIdListOut);
  static nsresult CacheDelete(mozIStorageConnection* aConn, CacheId aCacheId,
                              const PCacheRequest& aRequest,
                              const PCacheQueryParams& aParams,
                              nsTArray<nsID>& aDeletedBodyIdListOut,
                              bool* aSuccessOut);
  static nsresult CacheKeys(mozIStorageConnection* aConn, CacheId aCacheId,
                            const PCacheRequestOrVoid& aRequestOrVoid,
                            const PCacheQueryParams& aParams,
                            nsTArray<SavedRequest>& aSavedRequestsOut);

  static nsresult StorageMatch(mozIStorageConnection* aConn,
                               Namespace aNamespace,
                               const PCacheRequest& aRequest,
                               const PCacheQueryParams& aParams,
                               bool* aFoundResponseOut,
                               SavedResponse* aSavedResponseOut);
  static nsresult StorageGetCacheId(mozIStorageConnection* aConn,
                                    Namespace aNamespace, const nsAString& aKey,
                                    bool* aFoundCacheOut, CacheId* aCacheIdOut);
  static nsresult StoragePutCache(mozIStorageConnection* aConn,
                                  Namespace aNamespace, const nsAString& aKey,
                                  CacheId aCacheId);
  static nsresult StorageForgetCache(mozIStorageConnection* aConn,
                                     Namespace aNamespace,
                                     const nsAString& aKey);
  static nsresult StorageGetKeys(mozIStorageConnection* aConn,
                                 Namespace aNamespace,
                                 nsTArray<nsString>& aKeysOut);

  // We will wipe out databases with a schema versions less than this.
  static const int32_t kMaxWipeSchemaVersion;

private:
  typedef int32_t EntryId;

  static nsresult QueryAll(mozIStorageConnection* aConn, CacheId aCacheId,
                           nsTArray<EntryId>& aEntryIdListOut);
  static nsresult QueryCache(mozIStorageConnection* aConn, CacheId aCacheId,
                             const PCacheRequest& aRequest,
                             const PCacheQueryParams& aParams,
                             nsTArray<EntryId>& aEntryIdListOut,
                             uint32_t aMaxResults = UINT32_MAX);
  static nsresult MatchByVaryHeader(mozIStorageConnection* aConn,
                                    const PCacheRequest& aRequest,
                                    EntryId entryId, bool* aSuccessOut);
  static nsresult DeleteEntries(mozIStorageConnection* aConn,
                                const nsTArray<EntryId>& aEntryIdList,
                                nsTArray<nsID>& aDeletedBodyIdListOut,
                                uint32_t aPos=0, int32_t aLen=-1);
  static nsresult InsertEntry(mozIStorageConnection* aConn, CacheId aCacheId,
                              const PCacheRequest& aRequest,
                              const nsID* aRequestBodyId,
                              const PCacheResponse& aResponse,
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
  static nsresult BindId(mozIStorageStatement* aState, uint32_t aPos,
                         const nsID* aId);
  static nsresult ExtractId(mozIStorageStatement* aState, uint32_t aPos,
                            nsID* aIdOut);

  DBSchema() = delete;
  ~DBSchema() = delete;

  static const int32_t kLatestSchemaVersion;
  static const int32_t kMaxEntriesPerStatement;
};

} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_DBSchema_h
