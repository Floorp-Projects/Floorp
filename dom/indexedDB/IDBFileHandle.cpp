/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBFileHandle.h"

#include "mozilla/dom/file/File.h"
#include "mozilla/dom/IDBFileHandleBinding.h"
#include "mozilla/dom/quota/FileStreams.h"

#include "IDBDatabase.h"

USING_INDEXEDDB_NAMESPACE
USING_QUOTA_NAMESPACE

namespace {

inline
already_AddRefed<nsIFile>
GetFileFor(FileInfo* aFileInfo)

{
  FileManager* fileManager = aFileInfo->Manager();
  nsCOMPtr<nsIFile> directory = fileManager->GetDirectory();
  NS_ENSURE_TRUE(directory, nullptr);

  nsCOMPtr<nsIFile> file = fileManager->GetFileForId(directory,
                                                     aFileInfo->Id());
  NS_ENSURE_TRUE(file, nullptr);

  return file.forget();
}

} // anonymous namespace

IDBFileHandle::IDBFileHandle(IDBDatabase* aOwner)
  : FileHandle(aOwner)
{
}

// virtual
JSObject*
IDBFileHandle::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return IDBFileHandleBinding::Wrap(aCx, aScope, this);
}

// static
already_AddRefed<IDBFileHandle>
IDBFileHandle::Create(IDBDatabase* aDatabase,
                      const nsAString& aName,
                      const nsAString& aType,
                      already_AddRefed<FileInfo> aFileInfo)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<FileInfo> fileInfo(aFileInfo);
  NS_ASSERTION(fileInfo, "Null pointer!");

  nsRefPtr<IDBFileHandle> newFile = new IDBFileHandle(aDatabase);

  newFile->mFileStorage = aDatabase;
  newFile->mName = aName;
  newFile->mType = aType;

  newFile->mFile = GetFileFor(fileInfo);
  NS_ENSURE_TRUE(newFile->mFile, nullptr);
  newFile->mFileName.AppendInt(fileInfo->Id());

  fileInfo.swap(newFile->mFileInfo);

  return newFile.forget();
}

already_AddRefed<nsISupports>
IDBFileHandle::CreateStream(nsIFile* aFile, bool aReadOnly)
{
  nsCOMPtr<nsIOfflineStorage> storage = do_QueryInterface(mFileStorage);
  NS_ASSERTION(storage, "This should always succeed!");

  PersistenceType persistenceType = storage->Type();
  const nsACString& group = storage->Group();
  const nsACString& origin = storage->Origin();

  nsCOMPtr<nsISupports> result;

  if (aReadOnly) {
    nsRefPtr<FileInputStream> stream =
      FileInputStream::Create(persistenceType, group, origin, aFile, -1, -1,
                              nsIFileInputStream::DEFER_OPEN);
    result = NS_ISUPPORTS_CAST(nsIFileInputStream*, stream);
  }
  else {
    nsRefPtr<FileStream> stream =
      FileStream::Create(persistenceType, group, origin, aFile, -1, -1,
                         nsIFileStream::DEFER_OPEN);
    result = NS_ISUPPORTS_CAST(nsIFileStream*, stream);
  }
  NS_ENSURE_TRUE(result, nullptr);

  return result.forget();
}

already_AddRefed<nsIDOMFile>
IDBFileHandle::CreateFileObject(mozilla::dom::file::LockedFile* aLockedFile,
                                uint32_t aFileSize)
{
  nsCOMPtr<nsIDOMFile> file = new mozilla::dom::file::File(
    mName, mType, aFileSize, mFile, aLockedFile, mFileInfo);

  return file.forget();
}

IDBDatabase*
IDBFileHandle::Database()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBDatabase* database = static_cast<IDBDatabase*>(mFileStorage.get());
  MOZ_ASSERT(database);

  return database;
}
