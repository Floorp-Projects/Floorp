/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "File.h"

#include "FileHandle.h"
#include "mozilla/Assertions.h"
#include "nsDebug.h"

namespace mozilla {
namespace dom {

using indexedDB::IndexedDatabaseManager;

// Create as a file
FileImpl::FileImpl(const nsAString& aName, const nsAString& aContentType,
                   uint64_t aLength, nsIFile* aFile, FileHandle* aFileHandle)
  : DOMFileImplBase(aName, aContentType, aLength),
    mFile(aFile), mFileHandle(aFileHandle), mWholeFile(true), mStoredFile(false)
{
  MOZ_ASSERT(mFile, "Null file!");
  MOZ_ASSERT(mFileHandle, "Null file handle!");
}

// Create as a stored file
FileImpl::FileImpl(const nsAString& aName, const nsAString& aContentType,
                   uint64_t aLength, nsIFile* aFile, FileHandle* aFileHandle,
                   indexedDB::FileInfo* aFileInfo)
  : DOMFileImplBase(aName, aContentType, aLength),
    mFile(aFile), mFileHandle(aFileHandle), mWholeFile(true), mStoredFile(true)
{
  MOZ_ASSERT(mFile, "Null file!");
  MOZ_ASSERT(mFileHandle, "Null file handle!");
  mFileInfos.AppendElement(aFileInfo);
}

// Create slice
FileImpl::FileImpl(const FileImpl* aOther, uint64_t aStart, uint64_t aLength,
                   const nsAString& aContentType)
  : DOMFileImplBase(aContentType, aOther->mStart + aStart, aLength),
    mFile(aOther->mFile), mFileHandle(aOther->mFileHandle),
    mWholeFile(false), mStoredFile(aOther->mStoredFile)
{
  MOZ_ASSERT(mFile, "Null file!");
  MOZ_ASSERT(mFileHandle, "Null file handle!");

  if (mStoredFile) {
    indexedDB::FileInfo* fileInfo;

    if (IndexedDatabaseManager::IsClosed()) {
      fileInfo = aOther->GetFileInfo();
    }
    else {
      MutexAutoLock lock(IndexedDatabaseManager::FileMutex());
      fileInfo = aOther->GetFileInfo();
    }

    mFileInfos.AppendElement(fileInfo);
  }
}

FileImpl::~FileImpl()
{
}

void
FileImpl::Unlink()
{
  FileImpl* tmp = this;
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFileHandle);
}

void
FileImpl::Traverse(nsCycleCollectionTraversalCallback &cb)
{
  FileImpl* tmp = this;
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFileHandle);
}

nsresult
FileImpl::GetInternalStream(nsIInputStream** aStream)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsresult rv = mFileHandle->OpenInputStream(mWholeFile, mStart, mLength,
                                             aStream);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

already_AddRefed<nsIDOMBlob>
FileImpl::CreateSlice(uint64_t aStart, uint64_t aLength,
                      const nsAString& aContentType)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIDOMBlob> t =
    new DOMFileCC(new FileImpl(this, aStart, aLength, aContentType));

  return t.forget();
}

nsresult
FileImpl::GetMozFullPathInternal(nsAString& aFilename)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mIsFile, "Should only be called on files");

  return mFile->GetPath(aFilename);
}

} // namespace dom
} // namespace mozilla
