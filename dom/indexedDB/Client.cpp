/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Client.h"

#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/UsageRunnable.h"
#include "mozilla/dom/quota/Utilities.h"

#include "IDBDatabase.h"
#include "IndexedDatabaseManager.h"
#include "TransactionThreadPool.h"

USING_INDEXEDDB_NAMESPACE
using mozilla::dom::quota::QuotaManager;

namespace {

bool
GetDatabaseBaseFilename(const nsAString& aFilename,
                        nsAString& aDatabaseBaseFilename)
{
  NS_ASSERTION(!aFilename.IsEmpty(), "Bad argument!");

  NS_NAMED_LITERAL_STRING(sqlite, ".sqlite");

  if (!StringEndsWith(aFilename, sqlite)) {
    return false;
  }

  aDatabaseBaseFilename =
    Substring(aFilename, 0, aFilename.Length() - sqlite.Length());

  return true;
}

} // anonymous namespace

// This needs to be fully qualified to not confuse trace refcnt assertions.
NS_IMPL_ADDREF(mozilla::dom::indexedDB::Client)
NS_IMPL_RELEASE(mozilla::dom::indexedDB::Client)

nsresult
Client::InitOrigin(const nsACString& aOrigin, UsageRunnable* aUsageRunnable)
{
  nsCOMPtr<nsIFile> directory;
  nsresult rv = GetDirectory(aOrigin, getter_AddRefs(directory));
  NS_ENSURE_SUCCESS(rv, rv);

  // We need to see if there are any files in the directory already. If they
  // are database files then we need to cleanup stored files (if it's needed)
  // and also get the usage.

  nsAutoTArray<nsString, 20> subdirsToProcess;
  nsAutoTArray<nsCOMPtr<nsIFile> , 20> unknownFiles;
  nsTHashtable<nsStringHashKey> validSubdirs;
  validSubdirs.Init(20);

  nsCOMPtr<nsISimpleEnumerator> entries;
  rv = directory->GetDirectoryEntries(getter_AddRefs(entries));
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasMore;
  while (NS_SUCCEEDED((rv = entries->HasMoreElements(&hasMore))) &&
         hasMore && (!aUsageRunnable || !aUsageRunnable->Canceled())) {
    nsCOMPtr<nsISupports> entry;
    rv = entries->GetNext(getter_AddRefs(entry));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> file = do_QueryInterface(entry);
    NS_ENSURE_TRUE(file, NS_NOINTERFACE);

    nsString leafName;
    rv = file->GetLeafName(leafName);
    NS_ENSURE_SUCCESS(rv, rv);

    if (StringEndsWith(leafName, NS_LITERAL_STRING(".sqlite-journal"))) {
      continue;
    }

    bool isDirectory;
    rv = file->IsDirectory(&isDirectory);
    NS_ENSURE_SUCCESS(rv, rv);

    if (isDirectory) {
      if (!validSubdirs.GetEntry(leafName)) {
        subdirsToProcess.AppendElement(leafName);
      }
      continue;
    }

    nsString dbBaseFilename;
    if (!GetDatabaseBaseFilename(leafName, dbBaseFilename)) {
      unknownFiles.AppendElement(file);
      continue;
    }

    nsCOMPtr<nsIFile> fmDirectory;
    rv = directory->Clone(getter_AddRefs(fmDirectory));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = fmDirectory->Append(dbBaseFilename);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = FileManager::InitDirectory(fmDirectory, file, aOrigin);
    NS_ENSURE_SUCCESS(rv, rv);

    if (aUsageRunnable) {
      int64_t fileSize;
      rv = file->GetFileSize(&fileSize);
      NS_ENSURE_SUCCESS(rv, rv);

      aUsageRunnable->AppendToDatabaseUsage(uint64_t(fileSize));

      uint64_t usage;
      rv = FileManager::GetUsage(fmDirectory, &usage);
      NS_ENSURE_SUCCESS(rv, rv);

      aUsageRunnable->AppendToFileUsage(usage);
    }

    validSubdirs.PutEntry(dbBaseFilename);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  for (uint32_t i = 0; i < subdirsToProcess.Length(); i++) {
    const nsString& subdir = subdirsToProcess[i];
    if (!validSubdirs.GetEntry(subdir)) {
      NS_WARNING("Unknown subdirectory found!");
      return NS_ERROR_UNEXPECTED;
    }
  }

  for (uint32_t i = 0; i < unknownFiles.Length(); i++) {
    nsCOMPtr<nsIFile>& unknownFile = unknownFiles[i];

    // Some temporary SQLite files could disappear, so we have to check if the
    // unknown file still exists.
    bool exists;
    rv = unknownFile->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, rv);

    if (exists) {
      nsString leafName;
      unknownFile->GetLeafName(leafName);

      // The journal file may exists even after db has been correctly opened.
      if (!StringEndsWith(leafName, NS_LITERAL_STRING(".sqlite-journal"))) {
        NS_WARNING("Unknown file found!");
        return NS_ERROR_UNEXPECTED;
      }
    }
  }

  return NS_OK;
}

nsresult
Client::GetUsageForOrigin(const nsACString& aOrigin,
                          UsageRunnable* aUsageRunnable)
{
  NS_ASSERTION(aUsageRunnable, "Null pointer!");

  nsCOMPtr<nsIFile> directory;
  nsresult rv = GetDirectory(aOrigin, getter_AddRefs(directory));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = GetUsageForDirectoryInternal(directory, aUsageRunnable, true);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

bool
Client::IsTransactionServiceActivated()
{
  return !!TransactionThreadPool::Get();
}

void
Client::WaitForStoragesToComplete(nsTArray<nsIOfflineStorage*>& aStorages,
                                  nsIRunnable* aCallback)
{
  NS_ASSERTION(!aStorages.IsEmpty(), "No storages to wait on!");
  NS_ASSERTION(aCallback, "Passed null callback!");

  TransactionThreadPool* pool = TransactionThreadPool::Get();
  NS_ASSERTION(pool, "Should have checked if transaction service is active!");

  nsTArray<nsRefPtr<IDBDatabase> > databases(aStorages.Length());
  for (uint32_t index = 0; index < aStorages.Length(); index++) {
    IDBDatabase* database = IDBDatabase::FromStorage(aStorages[index]);
    if (!database) {
      MOZ_CRASH();
    }

    databases.AppendElement(database);
  }

  pool->WaitForDatabasesToComplete(databases, aCallback);
}

void
Client::AbortTransactionsForStorage(nsIOfflineStorage* aStorage)
{
  NS_ASSERTION(aStorage, "Passed null storage!");

  TransactionThreadPool* pool = TransactionThreadPool::Get();
  NS_ASSERTION(pool, "Should have checked if transaction service is active!");

  IDBDatabase* database = IDBDatabase::FromStorage(aStorage);
  NS_ASSERTION(database, "This shouldn't be null!");

  pool->AbortTransactionsForDatabase(database);
}

bool
Client::HasTransactionsForStorage(nsIOfflineStorage* aStorage)
{
  TransactionThreadPool* pool = TransactionThreadPool::Get();
  NS_ASSERTION(pool, "Should have checked if transaction service is active!");

  IDBDatabase* database = IDBDatabase::FromStorage(aStorage);
  NS_ASSERTION(database, "This shouldn't be null!");

  return pool->HasTransactionsForDatabase(database);
}

void
Client::OnOriginClearCompleted(const nsACString& aPattern)
{
  IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
  if (mgr) {
    mgr->InvalidateFileManagersForPattern(aPattern);
  }
}

void
Client::ShutdownTransactionService()
{
  TransactionThreadPool::Shutdown();
}

void
Client::OnShutdownCompleted()
{
  IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
  if (mgr) {
    mgr->InvalidateAllFileManagers();
  }
}

nsresult
Client::GetDirectory(const nsACString& aOrigin, nsIFile** aDirectory)
{
  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "This should never fail!");

  nsCOMPtr<nsIFile> directory;
  nsresult rv =
    quotaManager->GetDirectoryForOrigin(aOrigin, getter_AddRefs(directory));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(directory, "What?");

  rv = directory->Append(NS_LITERAL_STRING(IDB_DIRECTORY_NAME));
  NS_ENSURE_SUCCESS(rv, rv);

  directory.forget(aDirectory);
  return NS_OK;
}

nsresult
Client::GetUsageForDirectoryInternal(nsIFile* aDirectory,
                                     UsageRunnable* aUsageRunnable,
                                     bool aDatabaseFiles)
{
  NS_ASSERTION(aDirectory, "Null pointer!");
  NS_ASSERTION(aUsageRunnable, "Null pointer!");

  nsCOMPtr<nsISimpleEnumerator> entries;
  nsresult rv = aDirectory->GetDirectoryEntries(getter_AddRefs(entries));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!entries) {
    return NS_OK;
  }

  bool hasMore;
  while (NS_SUCCEEDED((rv = entries->HasMoreElements(&hasMore))) &&
         hasMore && !aUsageRunnable->Canceled()) {
    nsCOMPtr<nsISupports> entry;
    rv = entries->GetNext(getter_AddRefs(entry));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> file(do_QueryInterface(entry));
    NS_ASSERTION(file, "Don't know what this is!");

    bool isDirectory;
    rv = file->IsDirectory(&isDirectory);
    NS_ENSURE_SUCCESS(rv, rv);

    if (isDirectory) {
      if (aDatabaseFiles) {
        rv = GetUsageForDirectoryInternal(file, aUsageRunnable, false);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      else {
        nsString leafName;
        rv = file->GetLeafName(leafName);
        NS_ENSURE_SUCCESS(rv, rv);

        if (!leafName.EqualsLiteral(JOURNAL_DIRECTORY_NAME)) {
          NS_WARNING("Unknown directory found!");
        }
      }

      continue;
    }

    int64_t fileSize;
    rv = file->GetFileSize(&fileSize);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ASSERTION(fileSize >= 0, "Negative size?!");

    if (aDatabaseFiles) {
      aUsageRunnable->AppendToDatabaseUsage(uint64_t(fileSize));
    }
    else {
      aUsageRunnable->AppendToFileUsage(uint64_t(fileSize));
    }
  }
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}
