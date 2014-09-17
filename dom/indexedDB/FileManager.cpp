/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileManager.h"

#include "mozIStorageConnection.h"
#include "mozIStorageStatement.h"
#include "nsIInputStream.h"
#include "nsISimpleEnumerator.h"

#include "mozilla/dom/quota/Utilities.h"
#include "mozStorageCID.h"
#include "mozStorageHelper.h"

#include "Client.h"
#include "FileInfo.h"
#include "IndexedDatabaseManager.h"
#include "OpenDatabaseHelper.h"

#include "IndexedDatabaseInlines.h"
#include <algorithm>

USING_INDEXEDDB_NAMESPACE
using mozilla::dom::quota::AssertIsOnIOThread;

namespace {

PLDHashOperator
EnumerateToTArray(const uint64_t& aKey,
                  FileInfo* aValue,
                  void* aUserArg)
{
  NS_ASSERTION(aValue, "Null pointer!");
  NS_ASSERTION(aUserArg, "Null pointer!");

  nsTArray<FileInfo*>* array =
    static_cast<nsTArray<FileInfo*>*>(aUserArg);

  array->AppendElement(aValue);

  return PL_DHASH_NEXT;
}

already_AddRefed<nsIFile>
GetDirectoryFor(const nsAString& aDirectoryPath)
{
  nsCOMPtr<nsIFile> directory =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
  NS_ENSURE_TRUE(directory, nullptr);

  nsresult rv = directory->InitWithPath(aDirectoryPath);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return directory.forget();
}

} // anonymous namespace

nsresult
FileManager::Init(nsIFile* aDirectory,
                  mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  NS_ASSERTION(aDirectory, "Null directory!");
  NS_ASSERTION(aConnection, "Null connection!");

  bool exists;
  nsresult rv = aDirectory->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {
    bool isDirectory;
    rv = aDirectory->IsDirectory(&isDirectory);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(isDirectory, NS_ERROR_FAILURE);
  }
  else {
    rv = aDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = aDirectory->GetPath(mDirectoryPath);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIFile> journalDirectory;
  rv = aDirectory->Clone(getter_AddRefs(journalDirectory));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = journalDirectory->Append(NS_LITERAL_STRING(JOURNAL_DIRECTORY_NAME));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = journalDirectory->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {
    bool isDirectory;
    rv = journalDirectory->IsDirectory(&isDirectory);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(isDirectory, NS_ERROR_FAILURE);
  }

  rv = journalDirectory->GetPath(mJournalDirectoryPath);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageStatement> stmt;
  rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id, refcount "
    "FROM file"
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasResult;
  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    int64_t id;
    rv = stmt->GetInt64(0, &id);
    NS_ENSURE_SUCCESS(rv, rv);

    int32_t refcount;
    rv = stmt->GetInt32(1, &refcount);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ASSERTION(refcount, "This shouldn't happen!");

    nsRefPtr<FileInfo> fileInfo = FileInfo::Create(this, id);
    fileInfo->mDBRefCnt = refcount;

    mFileInfos.Put(id, fileInfo);

    mLastFileId = std::max(id, mLastFileId);
  }

  return NS_OK;
}

nsresult
FileManager::Invalidate()
{
  if (IndexedDatabaseManager::IsClosed()) {
    NS_ERROR("Shouldn't be called after shutdown!");
    return NS_ERROR_UNEXPECTED;
  }

  nsTArray<FileInfo*> fileInfos;
  {
    MutexAutoLock lock(IndexedDatabaseManager::FileMutex());

    NS_ASSERTION(!mInvalidated, "Invalidate more than once?!");
    mInvalidated = true;

    fileInfos.SetCapacity(mFileInfos.Count());
    mFileInfos.EnumerateRead(EnumerateToTArray, &fileInfos);
  }

  for (uint32_t i = 0; i < fileInfos.Length(); i++) {
    FileInfo* fileInfo = fileInfos.ElementAt(i);
    fileInfo->ClearDBRefs();
  }

  return NS_OK;
}

already_AddRefed<nsIFile>
FileManager::GetDirectory()
{
  return GetDirectoryFor(mDirectoryPath);
}

already_AddRefed<nsIFile>
FileManager::GetJournalDirectory()
{
  return GetDirectoryFor(mJournalDirectoryPath);
}

already_AddRefed<nsIFile>
FileManager::EnsureJournalDirectory()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIFile> journalDirectory = GetDirectoryFor(mJournalDirectoryPath);
  NS_ENSURE_TRUE(journalDirectory, nullptr);

  bool exists;
  nsresult rv = journalDirectory->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, nullptr);

  if (exists) {
    bool isDirectory;
    rv = journalDirectory->IsDirectory(&isDirectory);
    NS_ENSURE_SUCCESS(rv, nullptr);
    NS_ENSURE_TRUE(isDirectory, nullptr);
  }
  else {
    rv = journalDirectory->Create(nsIFile::DIRECTORY_TYPE, 0755);
    NS_ENSURE_SUCCESS(rv, nullptr);
  }

  return journalDirectory.forget();
}

already_AddRefed<FileInfo>
FileManager::GetFileInfo(int64_t aId)
{
  if (IndexedDatabaseManager::IsClosed()) {
    NS_ERROR("Shouldn't be called after shutdown!");
    return nullptr;
  }

  FileInfo* fileInfo = nullptr;
  {
    MutexAutoLock lock(IndexedDatabaseManager::FileMutex());
    fileInfo = mFileInfos.Get(aId);
  }
  nsRefPtr<FileInfo> result = fileInfo;
  return result.forget();
}

already_AddRefed<FileInfo>
FileManager::GetNewFileInfo()
{
  if (IndexedDatabaseManager::IsClosed()) {
    NS_ERROR("Shouldn't be called after shutdown!");
    return nullptr;
  }

  nsAutoPtr<FileInfo> fileInfo;

  {
    MutexAutoLock lock(IndexedDatabaseManager::FileMutex());

    int64_t id = mLastFileId + 1;

    fileInfo = FileInfo::Create(this, id);

    mFileInfos.Put(id, fileInfo);

    mLastFileId = id;
  }

  nsRefPtr<FileInfo> result = fileInfo.forget();
  return result.forget();
}

// static
already_AddRefed<nsIFile>
FileManager::GetFileForId(nsIFile* aDirectory, int64_t aId)
{
  NS_ASSERTION(aDirectory, "Null pointer!");

  nsAutoString id;
  id.AppendInt(aId);

  nsCOMPtr<nsIFile> file;
  nsresult rv = aDirectory->Clone(getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, nullptr);

  rv = file->Append(id);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return file.forget();
}

// static
nsresult
FileManager::InitDirectory(nsIFile* aDirectory,
                           nsIFile* aDatabaseFile,
                           PersistenceType aPersistenceType,
                           const nsACString& aGroup,
                           const nsACString& aOrigin)
{
  AssertIsOnIOThread();
  NS_ASSERTION(aDirectory, "Null directory!");
  NS_ASSERTION(aDatabaseFile, "Null database file!");

  bool exists;
  nsresult rv = aDirectory->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!exists) {
    return NS_OK;
  }

  bool isDirectory;
  rv = aDirectory->IsDirectory(&isDirectory);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(isDirectory, NS_ERROR_FAILURE);

  nsCOMPtr<nsIFile> journalDirectory;
  rv = aDirectory->Clone(getter_AddRefs(journalDirectory));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = journalDirectory->Append(NS_LITERAL_STRING(JOURNAL_DIRECTORY_NAME));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = journalDirectory->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (exists) {
    rv = journalDirectory->IsDirectory(&isDirectory);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(isDirectory, NS_ERROR_FAILURE);

    nsCOMPtr<nsISimpleEnumerator> entries;
    rv = journalDirectory->GetDirectoryEntries(getter_AddRefs(entries));
    NS_ENSURE_SUCCESS(rv, rv);

    bool hasElements;
    rv = entries->HasMoreElements(&hasElements);
    NS_ENSURE_SUCCESS(rv, rv);

    if (hasElements) {
      nsCOMPtr<mozIStorageConnection> connection;
      rv = OpenDatabaseHelper::CreateDatabaseConnection(aDatabaseFile,
        aDirectory, NullString(), aPersistenceType, aGroup, aOrigin,
        getter_AddRefs(connection));
      NS_ENSURE_SUCCESS(rv, rv);

      mozStorageTransaction transaction(connection, false);

      rv = connection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "CREATE VIRTUAL TABLE fs USING filesystem;"
      ));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<mozIStorageStatement> stmt;
      rv = connection->CreateStatement(NS_LITERAL_CSTRING(
        "SELECT name, (name IN (SELECT id FROM file)) FROM fs "
        "WHERE path = :path"
      ), getter_AddRefs(stmt));
      NS_ENSURE_SUCCESS(rv, rv);

      nsString path;
      rv = journalDirectory->GetPath(path);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = stmt->BindStringByName(NS_LITERAL_CSTRING("path"), path);
      NS_ENSURE_SUCCESS(rv, rv);

      bool hasResult;
      while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
        nsString name;
        rv = stmt->GetString(0, name);
        NS_ENSURE_SUCCESS(rv, rv);

        int32_t flag = stmt->AsInt32(1);

        if (!flag) {
          nsCOMPtr<nsIFile> file;
          rv = aDirectory->Clone(getter_AddRefs(file));
          NS_ENSURE_SUCCESS(rv, rv);

          rv = file->Append(name);
          NS_ENSURE_SUCCESS(rv, rv);

          if (NS_FAILED(file->Remove(false))) {
            NS_WARNING("Failed to remove orphaned file!");
          }
        }

        nsCOMPtr<nsIFile> journalFile;
        rv = journalDirectory->Clone(getter_AddRefs(journalFile));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = journalFile->Append(name);
        NS_ENSURE_SUCCESS(rv, rv);

        if (NS_FAILED(journalFile->Remove(false))) {
          NS_WARNING("Failed to remove journal file!");
        }
      }

      rv = connection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "DROP TABLE fs;"
      ));
      NS_ENSURE_SUCCESS(rv, rv);

      transaction.Commit();
    }
  }

  return NS_OK;
}

// static
nsresult
FileManager::GetUsage(nsIFile* aDirectory, uint64_t* aUsage)
{
  AssertIsOnIOThread();

  bool exists;
  nsresult rv = aDirectory->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!exists) {
    *aUsage = 0;
    return NS_OK;
  }

  nsCOMPtr<nsISimpleEnumerator> entries;
  rv = aDirectory->GetDirectoryEntries(getter_AddRefs(entries));
  NS_ENSURE_SUCCESS(rv, rv);

  uint64_t usage = 0;

  bool hasMore;
  while (NS_SUCCEEDED((rv = entries->HasMoreElements(&hasMore))) && hasMore) {
    nsCOMPtr<nsISupports> entry;
    rv = entries->GetNext(getter_AddRefs(entry));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> file = do_QueryInterface(entry);
    NS_ENSURE_TRUE(file, NS_NOINTERFACE);

    nsString leafName;
    rv = file->GetLeafName(leafName);
    NS_ENSURE_SUCCESS(rv, rv);

    if (leafName.EqualsLiteral(JOURNAL_DIRECTORY_NAME)) {
      continue;
    }

    int64_t fileSize;
    rv = file->GetFileSize(&fileSize);
    NS_ENSURE_SUCCESS(rv, rv);

    quota::IncrementUsage(&usage, uint64_t(fileSize));
  }

  *aUsage = usage;
  return NS_OK;
}
