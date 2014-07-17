/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSnapshot.h"

#include "IDBFileHandle.h"
#include "mozilla/Assertions.h"
#include "nsDebug.h"

namespace mozilla {
namespace dom {
namespace indexedDB {

NS_IMPL_ISUPPORTS_INHERITED0(FileImplSnapshot, DOMFileImpl)

// Create as a stored file
FileImplSnapshot::FileImplSnapshot(const nsAString& aName,
                                   const nsAString& aContentType,
                                   uint64_t aLength, nsIFile* aFile,
                                   IDBFileHandle* aFileHandle,
                                   FileInfo* aFileInfo)
  : DOMFileImplBase(aName, aContentType, aLength),
    mFile(aFile), mFileHandle(aFileHandle), mWholeFile(true)
{
  MOZ_ASSERT(mFile, "Null file!");
  MOZ_ASSERT(mFileHandle, "Null file handle!");
  mFileInfos.AppendElement(aFileInfo);
}

// Create slice
FileImplSnapshot::FileImplSnapshot(const FileImplSnapshot* aOther,
                                   uint64_t aStart, uint64_t aLength,
                                   const nsAString& aContentType)
  : DOMFileImplBase(aContentType, aOther->mStart + aStart, aLength),
    mFile(aOther->mFile), mFileHandle(aOther->mFileHandle),
    mWholeFile(false)
{
  MOZ_ASSERT(mFile, "Null file!");
  MOZ_ASSERT(mFileHandle, "Null file handle!");

  FileInfo* fileInfo;

  if (IndexedDatabaseManager::IsClosed()) {
    fileInfo = aOther->GetFileInfo();
  } else {
    MutexAutoLock lock(IndexedDatabaseManager::FileMutex());
    fileInfo = aOther->GetFileInfo();
  }

  mFileInfos.AppendElement(fileInfo);
}

FileImplSnapshot::~FileImplSnapshot()
{
}

void
FileImplSnapshot::Unlink()
{
  FileImplSnapshot* tmp = this;
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFileHandle);
}

void
FileImplSnapshot::Traverse(nsCycleCollectionTraversalCallback &cb)
{
  FileImplSnapshot* tmp = this;
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFileHandle);
}

nsresult
FileImplSnapshot::GetInternalStream(nsIInputStream** aStream)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsresult rv = mFileHandle->OpenInputStream(mWholeFile, mStart, mLength,
                                             aStream);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

already_AddRefed<nsIDOMBlob>
FileImplSnapshot::CreateSlice(uint64_t aStart, uint64_t aLength,
                              const nsAString& aContentType)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIDOMBlob> t =
    new DOMFile(new FileImplSnapshot(this, aStart, aLength, aContentType));

  return t.forget();
}

nsresult
FileImplSnapshot::GetMozFullPathInternal(nsAString& aFilename)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mIsFile, "Should only be called on files");

  return mFile->GetPath(aFilename);
}

} // namespace indexedDB
} // namespace dom
} // namespace mozilla
