/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/DBAction.h"

#include "mozilla/dom/cache/Connection.h"
#include "mozilla/dom/cache/DBSchema.h"
#include "mozilla/dom/cache/FileUtils.h"
#include "mozilla/dom/cache/QuotaClient.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "mozilla/net/nsFileProtocolHandler.h"
#include "mozIStorageConnection.h"
#include "mozIStorageService.h"
#include "mozStorageCID.h"
#include "nsIFile.h"
#include "nsIURI.h"
#include "nsIURIMutator.h"
#include "nsIFileURL.h"
#include "nsThreadUtils.h"

namespace mozilla::dom::cache {

using mozilla::dom::quota::AssertIsOnIOThread;
using mozilla::dom::quota::Client;
using mozilla::dom::quota::PERSISTENCE_TYPE_DEFAULT;
using mozilla::dom::quota::PersistenceType;

namespace {

nsresult WipeDatabase(const QuotaInfo& aQuotaInfo, nsIFile* aDBFile) {
  MOZ_DIAGNOSTIC_ASSERT(aDBFile);

  nsCOMPtr<nsIFile> dbDir;
  nsresult rv = aDBFile->GetParent(getter_AddRefs(dbDir));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(dbDir);

  rv = RemoveNsIFile(aQuotaInfo, aDBFile);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Note, the -wal journal file will be automatically deleted by sqlite when
  // the new database is created.  No need to explicitly delete it here.

  // Delete the morgue as well.
  rv = BodyDeleteDir(aQuotaInfo, dbDir);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = WipePaddingFile(aQuotaInfo, dbDir);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return rv;
}

}  // namespace

DBAction::DBAction(Mode aMode) : mMode(aMode) {}

DBAction::~DBAction() = default;

void DBAction::RunOnTarget(SafeRefPtr<Resolver> aResolver,
                           const QuotaInfo& aQuotaInfo, Data* aOptionalData) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(aResolver);
  MOZ_DIAGNOSTIC_ASSERT(aQuotaInfo.mDir);

  if (IsCanceled()) {
    aResolver->Resolve(NS_ERROR_ABORT);
    return;
  }

  nsCOMPtr<nsIFile> dbDir;
  nsresult rv = aQuotaInfo.mDir->Clone(getter_AddRefs(dbDir));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aResolver->Resolve(rv);
    return;
  }

  rv = dbDir->Append(u"cache"_ns);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aResolver->Resolve(rv);
    return;
  }

  nsCOMPtr<mozIStorageConnection> conn;

  // Attempt to reuse the connection opened by a previous Action.
  if (aOptionalData) {
    conn = aOptionalData->GetConnection();
  }

  // If there is no previous Action, then we must open one.
  if (!conn) {
    rv = OpenConnection(aQuotaInfo, dbDir, getter_AddRefs(conn));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aResolver->Resolve(rv);
      return;
    }
    MOZ_DIAGNOSTIC_ASSERT(conn);

    // Save this connection in the shared Data object so later Actions can
    // use it.  This avoids opening a new connection for every Action.
    if (aOptionalData) {
      // Since we know this connection will be around for as long as the
      // Cache is open, use our special wrapped connection class.  This
      // will let us perform certain operations once the Cache origin
      // is closed.
      nsCOMPtr<mozIStorageConnection> wrapped = new Connection(conn);
      aOptionalData->SetConnection(wrapped);
    }
  }

  RunWithDBOnTarget(std::move(aResolver), aQuotaInfo, dbDir, conn);
}

nsresult DBAction::OpenConnection(const QuotaInfo& aQuotaInfo, nsIFile* aDBDir,
                                  mozIStorageConnection** aConnOut) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(aQuotaInfo.mDirectoryLockId >= 0);
  MOZ_DIAGNOSTIC_ASSERT(aDBDir);
  MOZ_DIAGNOSTIC_ASSERT(aConnOut);

  bool exists;
  nsresult rv = aDBDir->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    if (NS_WARN_IF(mMode != Create)) {
      return NS_ERROR_FILE_NOT_FOUND;
    }
    rv = aDBDir->Create(nsIFile::DIRECTORY_TYPE, 0755);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  nsCOMPtr<nsIFile> dbFile;
  rv = aDBDir->Clone(getter_AddRefs(dbFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = dbFile->Append(kCachesSQLiteFilename);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  auto res = OpenDBConnection(aQuotaInfo, *dbFile);
  if (res.isErr()) {
    return res.inspectErr();
  }

  *aConnOut = res.unwrap().forget().take();
  return NS_OK;
}

SyncDBAction::SyncDBAction(Mode aMode) : DBAction(aMode) {}

SyncDBAction::~SyncDBAction() = default;

void SyncDBAction::RunWithDBOnTarget(SafeRefPtr<Resolver> aResolver,
                                     const QuotaInfo& aQuotaInfo,
                                     nsIFile* aDBDir,
                                     mozIStorageConnection* aConn) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(aResolver);
  MOZ_DIAGNOSTIC_ASSERT(aDBDir);
  MOZ_DIAGNOSTIC_ASSERT(aConn);

  nsresult rv = RunSyncWithDBOnTarget(aQuotaInfo, aDBDir, aConn);
  aResolver->Resolve(rv);
}

Result<nsCOMPtr<mozIStorageConnection>, nsresult> OpenDBConnection(
    const QuotaInfo& aQuotaInfo, nsIFile& aDBFile) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(aQuotaInfo.mDirectoryLockId >= -1);

  // Use our default file:// protocol handler directly to construct the database
  // URL.  This avoids any problems if a plugin registers a custom file://
  // handler.  If such a custom handler used javascript, then we would have a
  // bad time running off the main thread here.
  auto handler = MakeRefPtr<nsFileProtocolHandler>();
  CACHE_TRY(handler->Init());

  CACHE_TRY_INSPECT(const auto& mutator,
                    MOZ_TO_RESULT_INVOKE_TYPED(nsCOMPtr<nsIURIMutator>, handler,
                                               NewFileURIMutator, &aDBFile));

  const nsCString directoryLockIdClause =
      aQuotaInfo.mDirectoryLockId >= 0
          ? "&directoryLockId="_ns + IntToCString(aQuotaInfo.mDirectoryLockId)
          : EmptyCString();

  nsCOMPtr<nsIFileURL> dbFileUrl;
  CACHE_TRY(NS_MutateURI(mutator)
                .SetQuery("cache=private"_ns + directoryLockIdClause)
                .Finalize(dbFileUrl));

  CACHE_TRY_INSPECT(
      const auto& storageService,
      ToResultGet<nsCOMPtr<mozIStorageService>>(
          MOZ_SELECT_OVERLOAD(do_GetService), MOZ_STORAGE_SERVICE_CONTRACTID),
      Err(NS_ERROR_UNEXPECTED));

  CACHE_TRY_UNWRAP(
      auto conn,
      MOZ_TO_RESULT_INVOKE_TYPED(nsCOMPtr<mozIStorageConnection>,
                                 storageService, OpenDatabaseWithFileURL,
                                 dbFileUrl, ""_ns)
          .orElse([&aQuotaInfo, &aDBFile, &storageService,
                   &dbFileUrl](const nsresult rv)
                      -> Result<nsCOMPtr<mozIStorageConnection>, nsresult> {
            if (rv == NS_ERROR_FILE_CORRUPTED) {
              NS_WARNING(
                  "Cache database corrupted. Recreating empty database.");

              // There is nothing else we can do to recover.  Also, this data
              // can be deleted by QuotaManager at any time anyways.
              CACHE_TRY(WipeDatabase(aQuotaInfo, &aDBFile));

              CACHE_TRY_RETURN(MOZ_TO_RESULT_INVOKE_TYPED(
                  nsCOMPtr<mozIStorageConnection>, storageService,
                  OpenDatabaseWithFileURL, dbFileUrl, ""_ns));
            }
            return Err(rv);
          }));

  // Check the schema to make sure it is not too old.
  CACHE_TRY_INSPECT(const int32_t& schemaVersion,
                    MOZ_TO_RESULT_INVOKE(conn, GetSchemaVersion));
  if (schemaVersion > 0 && schemaVersion < db::kFirstShippedSchemaVersion) {
    // Close existing connection before wiping database.
    conn = nullptr;

    CACHE_TRY(WipeDatabase(aQuotaInfo, &aDBFile));

    CACHE_TRY_UNWRAP(conn, MOZ_TO_RESULT_INVOKE_TYPED(
                               nsCOMPtr<mozIStorageConnection>, storageService,
                               OpenDatabaseWithFileURL, dbFileUrl, ""_ns));
  }

  CACHE_TRY(db::InitializeConnection(*conn));

  return conn;
}

}  // namespace mozilla::dom::cache
