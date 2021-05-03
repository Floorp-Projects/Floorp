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
using mozilla::dom::quota::CloneFileAndAppend;
using mozilla::dom::quota::IsDatabaseCorruptionError;
using mozilla::dom::quota::PERSISTENCE_TYPE_DEFAULT;
using mozilla::dom::quota::PersistenceType;

namespace {

nsresult WipeDatabase(const QuotaInfo& aQuotaInfo, nsIFile& aDBFile) {
  QM_TRY_INSPECT(const auto& dbDir, MOZ_TO_RESULT_INVOKE_TYPED(
                                        nsCOMPtr<nsIFile>, aDBFile, GetParent));

  QM_TRY(RemoveNsIFile(aQuotaInfo, aDBFile));

  // Note, the -wal journal file will be automatically deleted by sqlite when
  // the new database is created.  No need to explicitly delete it here.

  // Delete the morgue as well.
  QM_TRY(BodyDeleteDir(aQuotaInfo, *dbDir));

  QM_TRY(WipePaddingFile(aQuotaInfo, dbDir));

  return NS_OK;
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

  const auto resolveErr = [&aResolver](const nsresult rv) {
    aResolver->Resolve(rv);
  };

  QM_TRY_INSPECT(const auto& dbDir,
                 CloneFileAndAppend(*aQuotaInfo.mDir, u"cache"_ns), QM_VOID,
                 resolveErr);

  nsCOMPtr<mozIStorageConnection> conn;

  // Attempt to reuse the connection opened by a previous Action.
  if (aOptionalData) {
    conn = aOptionalData->GetConnection();
  }

  // If there is no previous Action, then we must open one.
  if (!conn) {
    QM_TRY_UNWRAP(conn, OpenConnection(aQuotaInfo, *dbDir), QM_VOID,
                  resolveErr);
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

Result<nsCOMPtr<mozIStorageConnection>, nsresult> DBAction::OpenConnection(
    const QuotaInfo& aQuotaInfo, nsIFile& aDBDir) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(aQuotaInfo.mDirectoryLockId >= 0);

  QM_TRY_INSPECT(const bool& exists, MOZ_TO_RESULT_INVOKE(aDBDir, Exists));

  if (!exists) {
    QM_TRY(OkIf(mMode == Create), Err(NS_ERROR_FILE_NOT_FOUND));
    QM_TRY(aDBDir.Create(nsIFile::DIRECTORY_TYPE, 0755));
  }

  QM_TRY_INSPECT(const auto& dbFile,
                 CloneFileAndAppend(aDBDir, kCachesSQLiteFilename));

  QM_TRY_RETURN(OpenDBConnection(aQuotaInfo, *dbFile));
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
  QM_TRY(handler->Init());

  QM_TRY_INSPECT(const auto& mutator,
                 MOZ_TO_RESULT_INVOKE_TYPED(nsCOMPtr<nsIURIMutator>, handler,
                                            NewFileURIMutator, &aDBFile));

  const nsCString directoryLockIdClause =
      aQuotaInfo.mDirectoryLockId >= 0
          ? "&directoryLockId="_ns + IntToCString(aQuotaInfo.mDirectoryLockId)
          : EmptyCString();

  nsCOMPtr<nsIFileURL> dbFileUrl;
  QM_TRY(NS_MutateURI(mutator)
             .SetQuery("cache=private"_ns + directoryLockIdClause)
             .Finalize(dbFileUrl));

  QM_TRY_INSPECT(
      const auto& storageService,
      ToResultGet<nsCOMPtr<mozIStorageService>>(
          MOZ_SELECT_OVERLOAD(do_GetService), MOZ_STORAGE_SERVICE_CONTRACTID),
      Err(NS_ERROR_UNEXPECTED));

  QM_TRY_UNWRAP(
      auto conn,
      QM_OR_ELSE_WARN(
          MOZ_TO_RESULT_INVOKE_TYPED(nsCOMPtr<mozIStorageConnection>,
                                     storageService, OpenDatabaseWithFileURL,
                                     dbFileUrl, ""_ns),
          ([&aQuotaInfo, &aDBFile, &storageService,
            &dbFileUrl](const nsresult rv)
               -> Result<nsCOMPtr<mozIStorageConnection>, nsresult> {
            if (IsDatabaseCorruptionError(rv)) {
              NS_WARNING(
                  "Cache database corrupted. Recreating empty database.");

              // There is nothing else we can do to recover.  Also, this data
              // can be deleted by QuotaManager at any time anyways.
              QM_TRY(WipeDatabase(aQuotaInfo, aDBFile));

              QM_TRY_RETURN(MOZ_TO_RESULT_INVOKE_TYPED(
                  nsCOMPtr<mozIStorageConnection>, storageService,
                  OpenDatabaseWithFileURL, dbFileUrl, ""_ns));
            }
            return Err(rv);
          })));

  // Check the schema to make sure it is not too old.
  QM_TRY_INSPECT(const int32_t& schemaVersion,
                 MOZ_TO_RESULT_INVOKE(conn, GetSchemaVersion));
  if (schemaVersion > 0 && schemaVersion < db::kFirstShippedSchemaVersion) {
    // Close existing connection before wiping database.
    conn = nullptr;

    QM_TRY(WipeDatabase(aQuotaInfo, aDBFile));

    QM_TRY_UNWRAP(conn, MOZ_TO_RESULT_INVOKE_TYPED(
                            nsCOMPtr<mozIStorageConnection>, storageService,
                            OpenDatabaseWithFileURL, dbFileUrl, ""_ns));
  }

  QM_TRY(db::InitializeConnection(*conn));

  return conn;
}

}  // namespace mozilla::dom::cache
