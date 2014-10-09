/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSnapshot.h"

#include "IDBFileHandle.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/MetadataHelper.h"

#ifdef DEBUG
#include "nsXULAppAPI.h"
#endif

namespace mozilla {
namespace dom {
namespace indexedDB {

// Create as a stored file
FileImplSnapshot::FileImplSnapshot(const nsAString& aName,
                                   const nsAString& aContentType,
                                   MetadataParameters* aMetadataParams,
                                   nsIFile* aFile,
                                   IDBFileHandle* aFileHandle,
                                   FileInfo* aFileInfo)
  : FileImplBase(aName,
                 aContentType,
                 aMetadataParams->Size(),
                 aMetadataParams->LastModified())
  , mFile(aFile)
  , mFileHandle(aFileHandle)
  , mWholeFile(true)
{
  AssertSanity();
  MOZ_ASSERT(aMetadataParams);
  MOZ_ASSERT(aMetadataParams->Size() != UINT64_MAX);
  MOZ_ASSERT(aMetadataParams->LastModified() != INT64_MAX);
  MOZ_ASSERT(aFile);
  MOZ_ASSERT(aFileHandle);
  MOZ_ASSERT(aFileInfo);

  mFileInfos.AppendElement(aFileInfo);
}

// Create slice
FileImplSnapshot::FileImplSnapshot(const FileImplSnapshot* aOther,
                                   uint64_t aStart,
                                   uint64_t aLength,
                                   const nsAString& aContentType)
  : FileImplBase(aContentType, aOther->mStart + aStart, aLength)
  , mFile(aOther->mFile)
  , mFileHandle(aOther->mFileHandle)
  , mWholeFile(false)
{
  AssertSanity();
  MOZ_ASSERT(aOther);

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

#ifdef DEBUG

// static
void
FileImplSnapshot::AssertSanity()
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(NS_IsMainThread());
}

#endif // DEBUG

NS_IMPL_ISUPPORTS_INHERITED0(FileImplSnapshot, FileImpl)

void
FileImplSnapshot::Unlink()
{
  AssertSanity();

  FileImplSnapshot* tmp = this;
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFileHandle);
}

void
FileImplSnapshot::Traverse(nsCycleCollectionTraversalCallback &cb)
{
  AssertSanity();

  FileImplSnapshot* tmp = this;
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFileHandle);
}

bool
FileImplSnapshot::IsCCed() const
{
  AssertSanity();

  return true;
}

nsresult
FileImplSnapshot::GetInternalStream(nsIInputStream** aStream)
{
  AssertSanity();

  nsresult rv = mFileHandle->OpenInputStream(mWholeFile, mStart, mLength,
                                             aStream);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

already_AddRefed<FileImpl>
FileImplSnapshot::CreateSlice(uint64_t aStart,
                              uint64_t aLength,
                              const nsAString& aContentType,
                              ErrorResult& aRv)
{
  AssertSanity();

  nsRefPtr<FileImpl> impl =
    new FileImplSnapshot(this, aStart, aLength, aContentType);

  return impl.forget();
}

void
FileImplSnapshot::GetMozFullPathInternal(nsAString& aFilename,
                                         ErrorResult& aRv)
{
  AssertSanity();
  MOZ_ASSERT(mIsFile);

  aRv = mFile->GetPath(aFilename);
}

bool
FileImplSnapshot::IsStoredFile() const
{
  AssertSanity();

  return true;
}

bool
FileImplSnapshot::IsWholeFile() const
{
  AssertSanity();

  return mWholeFile;
}

bool
FileImplSnapshot::IsSnapshot() const
{
  AssertSanity();

  return true;
}

} // namespace indexedDB
} // namespace dom
} // namespace mozilla
