/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "File.h"

#include "LockedFile.h"

USING_FILE_NAMESPACE
using mozilla::dom::indexedDB::IndexedDatabaseManager;

// Create slice
File::File(const File* aOther, uint64_t aStart, uint64_t aLength,
           const nsAString& aContentType)
: nsDOMFileCC(aContentType, aOther->mStart + aStart, aLength),
  mFile(aOther->mFile), mLockedFile(aOther->mLockedFile),
  mWholeFile(false), mStoredFile(aOther->mStoredFile)
{
  NS_ASSERTION(mFile, "Null file!");
  NS_ASSERTION(mLockedFile, "Null locked file!");

  if (mStoredFile) {
    FileInfo* fileInfo;

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

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(File, nsDOMFileCC,
                                     mLockedFile)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(File)
NS_INTERFACE_MAP_END_INHERITING(nsDOMFileCC)

NS_IMPL_ADDREF_INHERITED(File, nsDOMFileCC)
NS_IMPL_RELEASE_INHERITED(File, nsDOMFileCC)

NS_IMETHODIMP
File::GetInternalStream(nsIInputStream **aStream)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsresult rv = mLockedFile->OpenInputStream(mWholeFile, mStart, mLength,
                                             aStream);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

already_AddRefed<nsIDOMBlob>
File::CreateSlice(uint64_t aStart, uint64_t aLength,
                  const nsAString& aContentType)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIDOMBlob> t =
    new File(this, aStart, aLength, aContentType);
  return t.forget();
}

NS_IMETHODIMP
File::GetMozFullPathInternal(nsAString &aFilename)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(mIsFile, "Should only be called on files");

  return mFile->GetPath(aFilename);
}
