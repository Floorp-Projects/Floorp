/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Indexed Database.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "FileManager.h"

#include "mozIStorageConnection.h"
#include "mozIStorageServiceQuotaManagement.h"
#include "mozIStorageStatement.h"
#include "nsISimpleEnumerator.h"
#include "mozStorageCID.h"
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
FileManager::Init()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  NS_ENSURE_TRUE(mFileInfos.Init(), NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

nsresult
FileManager::InitDirectory(nsIFile* aDirectory,
                           mozIStorageConnection* aConnection)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

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
    } else {
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

  rv = aDirectory->GetLeafName(mDirectoryName);
  NS_ENSURE_SUCCESS(rv, rv);

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

    if (!mFileInfos.Put(id, fileInfo)) {
      NS_WARNING("Out of memory?");
      return NS_ERROR_OUT_OF_MEMORY;
    }

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
  nsCOMPtr<nsILocalFile> directory =
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

    if (!mFileInfos.Put(id, fileInfo)) {
      NS_WARNING("Out of memory?");
      return nsnull;
    }

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
