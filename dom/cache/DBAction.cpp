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

  return OpenDBConnection(aQuotaInfo, dbFile, aConnOut);
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

// static
nsresult OpenDBConnection(const QuotaInfo& aQuotaInfo, nsIFile* aDBFile,
                          mozIStorageConnection** aConnOut) {
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(aQuotaInfo.mDirectoryLockId >= -1);
  MOZ_DIAGNOSTIC_ASSERT(aDBFile);
  MOZ_DIAGNOSTIC_ASSERT(aConnOut);

  // Use our default file:// protocol handler directly to construct the database
  // URL.  This avoids any problems if a plugin registers a custom file://
  // handler.  If such a custom handler used javascript, then we would have a
  // bad time running off the main thread here.
  RefPtr<nsFileProtocolHandler> handler = new nsFileProtocolHandler();
  nsresult rv = handler->Init();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIURIMutator> mutator;
  rv = handler->NewFileURIMutator(aDBFile, getter_AddRefs(mutator));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIFileURL> dbFileUrl;

  const nsCString directoryLockIdClause =
      aQuotaInfo.mDirectoryLockId >= 0
          ? "&directoryLockId="_ns + IntToCString(aQuotaInfo.mDirectoryLockId)
          : EmptyCString();

  rv = NS_MutateURI(mutator)
           .SetQuery("cache=private"_ns + directoryLockIdClause)
           .Finalize(dbFileUrl);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<mozIStorageService> ss =
      do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!ss)) {
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<mozIStorageConnection> conn;
  rv = ss->OpenDatabaseWithFileURL(dbFileUrl, ""_ns, getter_AddRefs(conn));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    NS_WARNING("Cache database corrupted. Recreating empty database.");

    conn = nullptr;

    // There is nothing else we can do to recover.  Also, this data can
    // be deleted by QuotaManager at any time anyways.
    rv = WipeDatabase(aQuotaInfo, aDBFile);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = ss->OpenDatabaseWithFileURL(dbFileUrl, ""_ns, getter_AddRefs(conn));
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Check the schema to make sure it is not too old.
  int32_t schemaVersion = 0;
  rv = conn->GetSchemaVersion(&schemaVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (schemaVersion > 0 && schemaVersion < db::kFirstShippedSchemaVersion) {
    conn = nullptr;
    rv = WipeDatabase(aQuotaInfo, aDBFile);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = ss->OpenDatabaseWithFileURL(dbFileUrl, ""_ns, getter_AddRefs(conn));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  rv = db::InitializeConnection(*conn);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  conn.forget(aConnOut);

  return rv;
}

}  // namespace mozilla::dom::cache
