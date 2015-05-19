/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
BlobImplSnapshot::BlobImplSnapshot(const nsAString& aName,
                                   const nsAString& aContentType,
                                   MetadataParameters* aMetadataParams,
                                   nsIFile* aFile,
                                   IDBFileHandle* aFileHandle,
                                   FileInfo* aFileInfo)
  : BlobImplBase(aName,
                 aContentType,
                 aMetadataParams->Size(),
                 aMetadataParams->LastModified())
  , mFile(aFile)
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

  mFileHandle =
    do_GetWeakReference(NS_ISUPPORTS_CAST(EventTarget*, aFileHandle));
}

// Create slice
BlobImplSnapshot::BlobImplSnapshot(const BlobImplSnapshot* aOther,
                                   uint64_t aStart,
                                   uint64_t aLength,
                                   const nsAString& aContentType)
  : BlobImplBase(aContentType, aOther->mStart + aStart, aLength)
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

BlobImplSnapshot::~BlobImplSnapshot()
{
}

#ifdef DEBUG

// static
void
BlobImplSnapshot::AssertSanity()
{
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(NS_IsMainThread());
}

#endif // DEBUG

NS_IMPL_ISUPPORTS_INHERITED(BlobImplSnapshot, BlobImpl, PIBlobImplSnapshot)

void
BlobImplSnapshot::GetInternalStream(nsIInputStream** aStream,
                                    ErrorResult& aRv)
{
  AssertSanity();

  nsCOMPtr<EventTarget> et = do_QueryReferent(mFileHandle);
  nsRefPtr<IDBFileHandle> fileHandle = static_cast<IDBFileHandle*>(et.get());
  if (!fileHandle) {
    aRv.Throw(NS_ERROR_DOM_FILEHANDLE_INACTIVE_ERR);
    return;
  }

  aRv = fileHandle->OpenInputStream(mWholeFile, mStart, mLength, aStream);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

already_AddRefed<BlobImpl>
BlobImplSnapshot::CreateSlice(uint64_t aStart,
                              uint64_t aLength,
                              const nsAString& aContentType,
                              ErrorResult& aRv)
{
  AssertSanity();

  nsRefPtr<BlobImpl> impl =
    new BlobImplSnapshot(this, aStart, aLength, aContentType);

  return impl.forget();
}

void
BlobImplSnapshot::GetMozFullPathInternal(nsAString& aFilename,
                                         ErrorResult& aRv)
{
  AssertSanity();
  MOZ_ASSERT(mIsFile);

  aRv = mFile->GetPath(aFilename);
}

bool
BlobImplSnapshot::IsStoredFile() const
{
  AssertSanity();

  return true;
}

bool
BlobImplSnapshot::IsWholeFile() const
{
  AssertSanity();

  return mWholeFile;
}

bool
BlobImplSnapshot::IsSnapshot() const
{
  AssertSanity();

  return true;
}

} // namespace indexedDB
} // namespace dom
} // namespace mozilla
