/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/DBAction.h"

#include "mozilla/dom/cache/Connection.h"
#include "mozilla/dom/cache/DBSchema.h"
#include "mozilla/dom/cache/FileUtils.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "mozilla/net/nsFileProtocolHandler.h"
#include "mozIStorageConnection.h"
#include "mozIStorageService.h"
#include "mozStorageCID.h"
#include "nsIFile.h"
#include "nsIURI.h"
#include "nsIFileURL.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {
namespace cache {

using mozilla::dom::quota::PERSISTENCE_TYPE_DEFAULT;
using mozilla::dom::quota::PersistenceType;

DBAction::DBAction(Mode aMode)
  : mMode(aMode)
{
}

DBAction::~DBAction()
{
}

void
DBAction::RunOnTarget(Resolver* aResolver, const QuotaInfo& aQuotaInfo,
                      Data* aOptionalData)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aResolver);
  MOZ_ASSERT(aQuotaInfo.mDir);

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

  rv = dbDir->Append(NS_LITERAL_STRING("cache"));
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
    MOZ_ASSERT(conn);

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

  RunWithDBOnTarget(aResolver, aQuotaInfo, dbDir, conn);
}

nsresult
DBAction::OpenConnection(const QuotaInfo& aQuotaInfo, nsIFile* aDBDir,
                         mozIStorageConnection** aConnOut)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aDBDir);
  MOZ_ASSERT(aConnOut);

  nsCOMPtr<mozIStorageConnection> conn;

  bool exists;
  nsresult rv = aDBDir->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  if (!exists) {
    if (NS_WARN_IF(mMode != Create)) {  return NS_ERROR_FILE_NOT_FOUND; }
    rv = aDBDir->Create(nsIFile::DIRECTORY_TYPE, 0755);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  }

  nsCOMPtr<nsIFile> dbFile;
  rv = aDBDir->Clone(getter_AddRefs(dbFile));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = dbFile->Append(NS_LITERAL_STRING("caches.sqlite"));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  rv = dbFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Use our default file:// protocol handler directly to construct the database
  // URL.  This avoids any problems if a plugin registers a custom file://
  // handler.  If such a custom handler used javascript, then we would have a
  // bad time running off the main thread here.
  nsRefPtr<nsFileProtocolHandler> handler = new nsFileProtocolHandler();
  rv = handler->Init();
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsCOMPtr<nsIURI> uri;
  rv = handler->NewFileURI(dbFile, getter_AddRefs(uri));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsCOMPtr<nsIFileURL> dbFileUrl = do_QueryInterface(uri);
  if (NS_WARN_IF(!dbFileUrl)) { return NS_ERROR_UNEXPECTED; }

  nsAutoCString type;
  PersistenceTypeToText(PERSISTENCE_TYPE_DEFAULT, type);

  rv = dbFileUrl->SetQuery(
    NS_LITERAL_CSTRING("persistenceType=") + type +
    NS_LITERAL_CSTRING("&group=") + aQuotaInfo.mGroup +
    NS_LITERAL_CSTRING("&origin=") + aQuotaInfo.mOrigin +
    NS_LITERAL_CSTRING("&cache=private"));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  nsCOMPtr<mozIStorageService> ss =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!ss)) { return NS_ERROR_UNEXPECTED; }

  rv = ss->OpenDatabaseWithFileURL(dbFileUrl, getter_AddRefs(conn));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    NS_WARNING("Cache database corrupted. Recreating empty database.");

    conn = nullptr;

    // There is nothing else we can do to recover.  Also, this data can
    // be deleted by QuotaManager at any time anyways.
    rv = WipeDatabase(dbFile, aDBDir);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = ss->OpenDatabaseWithFileURL(dbFileUrl, getter_AddRefs(conn));
  }
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Check the schema to make sure it is not too old.
  int32_t schemaVersion = 0;
  rv = conn->GetSchemaVersion(&schemaVersion);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  if (schemaVersion > 0 && schemaVersion < db::kFirstShippedSchemaVersion) {
    conn = nullptr;
    rv = WipeDatabase(dbFile, aDBDir);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = ss->OpenDatabaseWithFileURL(dbFileUrl, getter_AddRefs(conn));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
  }

  rv = db::InitializeConnection(conn);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  conn.forget(aConnOut);

  return rv;
}

nsresult
DBAction::WipeDatabase(nsIFile* aDBFile, nsIFile* aDBDir)
{
  nsresult rv = aDBFile->Remove(false);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  // Note, the -wal journal file will be automatically deleted by sqlite when
  // the new database is created.  No need to explicitly delete it here.

  // Delete the morgue as well.
  rv = BodyDeleteDir(aDBDir);
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  return rv;
}

SyncDBAction::SyncDBAction(Mode aMode)
  : DBAction(aMode)
{
}

SyncDBAction::~SyncDBAction()
{
}

void
SyncDBAction::RunWithDBOnTarget(Resolver* aResolver,
                                const QuotaInfo& aQuotaInfo, nsIFile* aDBDir,
                                mozIStorageConnection* aConn)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aResolver);
  MOZ_ASSERT(aDBDir);
  MOZ_ASSERT(aConn);

  nsresult rv = RunSyncWithDBOnTarget(aQuotaInfo, aDBDir, aConn);
  aResolver->Resolve(rv);
}

} // namespace cache
} // namespace dom
} // namespace mozilla
