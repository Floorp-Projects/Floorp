/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBMutableFile.h"

#include "mozilla/dom/File.h"
#include "mozilla/dom/IDBMutableFileBinding.h"
#include "mozilla/dom/quota/FileStreams.h"
#include "mozilla/dom/quota/QuotaManager.h"

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

IDBMutableFile::IDBMutableFile(IDBDatabase* aOwner)
  : MutableFile(aOwner)
{
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(IDBMutableFile, MutableFile, mDatabase)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBMutableFile)
NS_INTERFACE_MAP_END_INHERITING(MutableFile)

NS_IMPL_ADDREF_INHERITED(IDBMutableFile, MutableFile)
NS_IMPL_RELEASE_INHERITED(IDBMutableFile, MutableFile)

// static
already_AddRefed<IDBMutableFile>
IDBMutableFile::Create(const nsAString& aName,
                       const nsAString& aType,
                       IDBDatabase* aDatabase,
                       already_AddRefed<FileInfo> aFileInfo)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<FileInfo> fileInfo(aFileInfo);
  NS_ASSERTION(fileInfo, "Null pointer!");

  nsRefPtr<IDBMutableFile> newFile = new IDBMutableFile(aDatabase);

  newFile->mName = aName;
  newFile->mType = aType;

  newFile->mFile = GetFileFor(fileInfo);
  NS_ENSURE_TRUE(newFile->mFile, nullptr);

  newFile->mStorageId = aDatabase->Id();
  newFile->mFileName.AppendInt(fileInfo->Id());

  newFile->mDatabase = aDatabase;
  fileInfo.swap(newFile->mFileInfo);

  return newFile.forget();
}

bool
IDBMutableFile::IsShuttingDown()
{
  return QuotaManager::IsShuttingDown() || MutableFile::IsShuttingDown();
}

bool
IDBMutableFile::IsInvalid()
{
  return mDatabase->IsInvalidated();
}

nsIOfflineStorage*
IDBMutableFile::Storage()
{
  return mDatabase;
}

already_AddRefed<nsISupports>
IDBMutableFile::CreateStream(nsIFile* aFile, bool aReadOnly)
{
  PersistenceType persistenceType = mDatabase->Type();
  const nsACString& group = mDatabase->Group();
  const nsACString& origin = mDatabase->Origin();

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

void
IDBMutableFile::SetThreadLocals()
{
  MOZ_ASSERT(mDatabase->GetOwner(), "Should have owner!");
  QuotaManager::SetCurrentWindow(mDatabase->GetOwner());
}

void
IDBMutableFile::UnsetThreadLocals()
{
  QuotaManager::SetCurrentWindow(nullptr);
}

already_AddRefed<nsIDOMFile>
IDBMutableFile::CreateFileObject(mozilla::dom::FileHandle* aFileHandle,
                                uint32_t aFileSize)
{
  nsCOMPtr<nsIDOMFile> file = new DOMFile(
    new FileImpl(mName, mType, aFileSize, mFile, aFileHandle, mFileInfo));

  return file.forget();
}

// virtual
JSObject*
IDBMutableFile::WrapObject(JSContext* aCx)
{
  return IDBMutableFileBinding::Wrap(aCx, this);
}
