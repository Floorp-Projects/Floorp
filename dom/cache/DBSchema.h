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
struct nsID;

namespace mozilla {
namespace dom {
namespace cache {

class CacheQueryParams;
class CacheRequest;
class CacheRequestOrVoid;
class CacheResponse;
struct SavedRequest;
struct SavedResponse;

namespace db {

nsresult
CreateSchema(mozIStorageConnection* aConn);

nsresult
InitializeConnection(mozIStorageConnection* aConn);

nsresult
CreateCacheId(mozIStorageConnection* aConn, CacheId* aCacheIdOut);

nsresult
DeleteCacheId(mozIStorageConnection* aConn, CacheId aCacheId,
              nsTArray<nsID>& aDeletedBodyIdListOut);

// TODO: Consider removing unused IsCacheOrphaned after writing cleanup code. (bug 1110446)
nsresult
IsCacheOrphaned(mozIStorageConnection* aConn, CacheId aCacheId,
                bool* aOrphanedOut);

nsresult
CacheMatch(mozIStorageConnection* aConn, CacheId aCacheId,
           const CacheRequest& aRequest, const CacheQueryParams& aParams,
           bool* aFoundResponseOut, SavedResponse* aSavedResponseOut);

nsresult
CacheMatchAll(mozIStorageConnection* aConn, CacheId aCacheId,
              const CacheRequestOrVoid& aRequestOrVoid,
              const CacheQueryParams& aParams,
              nsTArray<SavedResponse>& aSavedResponsesOut);

nsresult
CachePut(mozIStorageConnection* aConn, CacheId aCacheId,
         const CacheRequest& aRequest,
         const nsID* aRequestBodyId,
         const CacheResponse& aResponse,
         const nsID* aResponseBodyId,
         nsTArray<nsID>& aDeletedBodyIdListOut);

nsresult
CacheDelete(mozIStorageConnection* aConn, CacheId aCacheId,
            const CacheRequest& aRequest,
            const CacheQueryParams& aParams,
            nsTArray<nsID>& aDeletedBodyIdListOut,
            bool* aSuccessOut);

nsresult
CacheKeys(mozIStorageConnection* aConn, CacheId aCacheId,
          const CacheRequestOrVoid& aRequestOrVoid,
          const CacheQueryParams& aParams,
          nsTArray<SavedRequest>& aSavedRequestsOut);

nsresult
StorageMatch(mozIStorageConnection* aConn,
             Namespace aNamespace,
             const CacheRequest& aRequest,
             const CacheQueryParams& aParams,
             bool* aFoundResponseOut,
             SavedResponse* aSavedResponseOut);

nsresult
StorageGetCacheId(mozIStorageConnection* aConn, Namespace aNamespace,
                  const nsAString& aKey, bool* aFoundCacheOut,
                  CacheId* aCacheIdOut);

nsresult
StoragePutCache(mozIStorageConnection* aConn, Namespace aNamespace,
                const nsAString& aKey, CacheId aCacheId);

nsresult
StorageForgetCache(mozIStorageConnection* aConn, Namespace aNamespace,
                   const nsAString& aKey);

nsresult
StorageGetKeys(mozIStorageConnection* aConn, Namespace aNamespace,
               nsTArray<nsString>& aKeysOut);

// We will wipe out databases with a schema versions less than this.
extern const int32_t kMaxWipeSchemaVersion;

} // namespace db
} // namespace cache
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_DBSchema_h
