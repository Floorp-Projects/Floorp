/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileManager.h"

#include "mozIStorageConnection.h"
#include "mozIStorageServiceQuotaManagement.h"
#include "mozIStorageStatement.h"
#include "nsISimpleEnumerator.h"

#include "mozStorageCID.h"
#include "mozStorageHelper.h"
#include "nsContentUtils.h"

#include "FileInfo.h"
#include "IndexedDatabaseManager.h"

USING_INDEXEDDB_NAMESPACE

namespace {

PLDHashOperator
EnumerateToTArray(const PRUint64& aKey,
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

} // anonymous namespace

nsresult
FileManager::Init(nsIFile* aDirectory,
                  mozIStorageConnection* aConnection)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  mFileInfos.Init();

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

  mozStorageTransaction transaction(aConnection, false);

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE VIRTUAL TABLE fs USING filesystem;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageStatement> stmt;
  rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT name, (name IN (SELECT id FROM file)) FROM fs "
    "WHERE path = :path"
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  nsString path;
  rv = aDirectory->GetPath(path);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("path"), path);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<mozIStorageServiceQuotaManagement> ss =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(ss, NS_ERROR_FAILURE);

  bool hasResult;
  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    nsString name;
    rv = stmt->GetString(0, name);
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 flag = stmt->AsInt32(1);

    nsCOMPtr<nsIFile> file;
    rv = aDirectory->Clone(getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = file->Append(name);
    NS_ENSURE_SUCCESS(rv, rv);

    if (flag) {
      rv = ss->UpdateQuotaInformationForFile(file);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      rv = file->Remove(false);
      if (NS_FAILED(rv)) {
        NS_WARNING("Failed to remove orphaned file!");
      }
    }
  }

  rv = aConnection->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "DROP TABLE fs;"
  ));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aDirectory->GetPath(mDirectoryPath);
  NS_ENSURE_SUCCESS(rv, rv);

  transaction.Commit();
  return NS_OK;
}

nsresult
FileManager::Load(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id, refcount "
    "FROM file"
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasResult;
  while (NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && hasResult) {
    PRInt64 id;
    rv = stmt->GetInt64(0, &id);
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 refcount;
    rv = stmt->GetInt32(1, &refcount);
    NS_ENSURE_SUCCESS(rv, rv);

    NS_ASSERTION(refcount, "This shouldn't happen!");

    nsRefPtr<FileInfo> fileInfo = FileInfo::Create(this, id);
    fileInfo->mDBRefCnt = refcount;

    mFileInfos.Put(id, fileInfo);

    mLastFileId = NS_MAX(id, mLastFileId);
  }

  mLoaded = true;

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

  for (PRUint32 i = 0; i < fileInfos.Length(); i++) {
    FileInfo* fileInfo = fileInfos.ElementAt(i);
    fileInfo->ClearDBRefs();
  }

  return NS_OK;
}

already_AddRefed<nsIFile>
FileManager::GetDirectory()
{
  nsCOMPtr<nsIFile> directory =
    do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
  NS_ENSURE_TRUE(directory, nsnull);

  nsresult rv = directory->InitWithPath(mDirectoryPath);
  NS_ENSURE_SUCCESS(rv, nsnull);

  return directory.forget();
}

already_AddRefed<FileInfo>
FileManager::GetFileInfo(PRInt64 aId)
{
  if (IndexedDatabaseManager::IsClosed()) {
    NS_ERROR("Shouldn't be called after shutdown!");
    return nsnull;
  }

  FileInfo* fileInfo = nsnull;
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
    return nsnull;
  }

  nsAutoPtr<FileInfo> fileInfo;

  {
    MutexAutoLock lock(IndexedDatabaseManager::FileMutex());

    PRInt64 id = mLastFileId + 1;

    fileInfo = FileInfo::Create(this, id);

    mFileInfos.Put(id, fileInfo);

    mLastFileId = id;
  }

  nsRefPtr<FileInfo> result = fileInfo.forget();
  return result.forget();
}

already_AddRefed<nsIFile>
FileManager::GetFileForId(nsIFile* aDirectory, PRInt64 aId)
{
  NS_ASSERTION(aDirectory, "Null pointer!");

  nsAutoString id;
  id.AppendInt(aId);

  nsCOMPtr<nsIFile> file;
  nsresult rv = aDirectory->Clone(getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, nsnull);

  rv = file->Append(id);
  NS_ENSURE_SUCCESS(rv, nsnull);

  return file.forget();
}
