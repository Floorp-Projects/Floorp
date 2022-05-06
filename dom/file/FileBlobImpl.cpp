/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileBlobImpl.h"
#include "BaseBlobImpl.h"
#include "mozilla/SlicedInputStream.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "nsCExternalHandlerService.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsIMIMEService.h"
#include "nsNetUtil.h"
#include "nsStreamUtils.h"

namespace mozilla::dom {

FileBlobImpl::FileBlobImpl(nsIFile* aFile)
    : mMutex("FileBlobImpl::mMutex"),
      mFile(aFile),
      mSerialNumber(BaseBlobImpl::NextSerialNumber()),
      mStart(0),
      mFileId(-1),
      mIsFile(true),
      mWholeFile(true) {
  MOZ_ASSERT(mFile, "must have file");
  MOZ_ASSERT(XRE_IsParentProcess());
  // Lazily get the content type and size
  mContentType.SetIsVoid(true);
  mMozFullPath.SetIsVoid(true);
  mFile->GetLeafName(mName);
}

FileBlobImpl::FileBlobImpl(const nsAString& aName,
                           const nsAString& aContentType, uint64_t aLength,
                           nsIFile* aFile)
    : mMutex("FileBlobImpl::mMutex"),
      mFile(aFile),
      mContentType(aContentType),
      mName(aName),
      mSerialNumber(BaseBlobImpl::NextSerialNumber()),
      mStart(0),
      mFileId(-1),
      mLength(Some(aLength)),
      mIsFile(true),
      mWholeFile(true) {
  MOZ_ASSERT(mFile, "must have file");
  MOZ_ASSERT(XRE_IsParentProcess());
  mMozFullPath.SetIsVoid(true);
}

FileBlobImpl::FileBlobImpl(const nsAString& aName,
                           const nsAString& aContentType, uint64_t aLength,
                           nsIFile* aFile, int64_t aLastModificationDate)
    : mMutex("FileBlobImpl::mMutex"),
      mFile(aFile),
      mContentType(aContentType),
      mName(aName),
      mSerialNumber(BaseBlobImpl::NextSerialNumber()),
      mStart(0),
      mFileId(-1),
      mLength(Some(aLength)),
      mLastModified(Some(aLastModificationDate)),
      mIsFile(true),
      mWholeFile(true) {
  MOZ_ASSERT(mFile, "must have file");
  MOZ_ASSERT(XRE_IsParentProcess());
  mMozFullPath.SetIsVoid(true);
}

FileBlobImpl::FileBlobImpl(nsIFile* aFile, const nsAString& aName,
                           const nsAString& aContentType)
    : mMutex("FileBlobImpl::mMutex"),
      mFile(aFile),
      mContentType(aContentType),
      mName(aName),
      mSerialNumber(BaseBlobImpl::NextSerialNumber()),
      mStart(0),
      mFileId(-1),
      mIsFile(true),
      mWholeFile(true) {
  MOZ_ASSERT(mFile, "must have file");
  MOZ_ASSERT(XRE_IsParentProcess());
  if (aContentType.IsEmpty()) {
    // Lazily get the content type and size
    mContentType.SetIsVoid(true);
  }

  mMozFullPath.SetIsVoid(true);
}

FileBlobImpl::FileBlobImpl(const FileBlobImpl* aOther, uint64_t aStart,
                           uint64_t aLength, const nsAString& aContentType)
    : mMutex("FileBlobImpl::mMutex"),
      mFile(aOther->mFile),
      mContentType(aContentType),
      mSerialNumber(BaseBlobImpl::NextSerialNumber()),
      mStart(aOther->mStart + aStart),
      mFileId(-1),
      mLength(Some(aLength)),
      mIsFile(false),
      mWholeFile(false) {
  MOZ_ASSERT(mFile, "must have file");
  MOZ_ASSERT(XRE_IsParentProcess());
  mMozFullPath = aOther->mMozFullPath;
}

already_AddRefed<BlobImpl> FileBlobImpl::CreateSlice(
    uint64_t aStart, uint64_t aLength, const nsAString& aContentType,
    ErrorResult& aRv) const {
  RefPtr<FileBlobImpl> impl =
      new FileBlobImpl(this, aStart, aLength, aContentType);
  return impl.forget();
}

void FileBlobImpl::GetMozFullPathInternal(nsAString& aFilename,
                                          ErrorResult& aRv) {
  MOZ_ASSERT(mIsFile, "Should only be called on files");

  MutexAutoLock lock(mMutex);

  if (!mMozFullPath.IsVoid()) {
    aFilename = mMozFullPath;
    return;
  }

  aRv = mFile->GetPath(aFilename);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  mMozFullPath = aFilename;
}

uint64_t FileBlobImpl::GetSize(ErrorResult& aRv) {
  MutexAutoLock lock(mMutex);

  if (mLength.isNothing()) {
    MOZ_ASSERT(mWholeFile,
               "Should only use lazy size when using the whole file");
    int64_t fileSize;
    aRv = mFile->GetFileSize(&fileSize);
    if (NS_WARN_IF(aRv.Failed())) {
      return 0;
    }

    if (fileSize < 0) {
      aRv.Throw(NS_ERROR_FAILURE);
      return 0;
    }

    mLength.emplace(fileSize);
  }

  return mLength.value();
}

class FileBlobImpl::GetTypeRunnable final : public WorkerMainThreadRunnable {
 public:
  GetTypeRunnable(WorkerPrivate* aWorkerPrivate, FileBlobImpl* aBlobImpl,
                  const MutexAutoLock& aProofOfLock)
      : WorkerMainThreadRunnable(aWorkerPrivate, "FileBlobImpl :: GetType"_ns),
        mBlobImpl(aBlobImpl),
        mProofOfLock(aProofOfLock) {
    MOZ_ASSERT(aBlobImpl);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool MainThreadRun() override {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoString type;
    mBlobImpl->GetTypeInternal(type, mProofOfLock);
    return true;
  }

 private:
  ~GetTypeRunnable() override = default;

  RefPtr<FileBlobImpl> mBlobImpl;
  const MutexAutoLock& mProofOfLock;
};

void FileBlobImpl::GetType(nsAString& aType) {
  MutexAutoLock lock(mMutex);
  GetTypeInternal(aType, lock);
}

void FileBlobImpl::GetTypeInternal(nsAString& aType,
                                   const MutexAutoLock& aProofOfLock) {
  aType.Truncate();

  if (mContentType.IsVoid()) {
    MOZ_ASSERT(mWholeFile,
               "Should only use lazy ContentType when using the whole file");

    if (!NS_IsMainThread()) {
      WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
      if (!workerPrivate) {
        // I have no idea in which thread this method is called. We cannot
        // return any valid value.
        return;
      }

      RefPtr<GetTypeRunnable> runnable =
          new GetTypeRunnable(workerPrivate, this, aProofOfLock);

      ErrorResult rv;
      runnable->Dispatch(Canceling, rv);
      if (NS_WARN_IF(rv.Failed())) {
        rv.SuppressException();
        return;
      }
    } else {
      nsresult rv;
      nsCOMPtr<nsIMIMEService> mimeService =
          do_GetService(NS_MIMESERVICE_CONTRACTID, &rv);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return;
      }

      nsAutoCString mimeType;
      rv = mimeService->GetTypeFromFile(mFile, mimeType);
      if (NS_FAILED(rv)) {
        mimeType.Truncate();
      }

      AppendUTF8toUTF16(mimeType, mContentType);
      mContentType.SetIsVoid(false);
    }
  }

  aType = mContentType;
}

void FileBlobImpl::GetBlobImplType(nsAString& aBlobImplType) const {
  aBlobImplType = u"FileBlobImpl"_ns;
}

int64_t FileBlobImpl::GetLastModified(ErrorResult& aRv) {
  MOZ_ASSERT(mIsFile, "Should only be called on files");

  MutexAutoLock lock(mMutex);

  if (mLastModified.isNothing()) {
    PRTime msecs;
    aRv = mFile->GetLastModifiedTime(&msecs);
    if (NS_WARN_IF(aRv.Failed())) {
      return 0;
    }

    mLastModified.emplace(int64_t(msecs));
  }

  return mLastModified.value();
}

const uint32_t sFileStreamFlags =
    nsIFileInputStream::CLOSE_ON_EOF | nsIFileInputStream::REOPEN_ON_REWIND |
    nsIFileInputStream::DEFER_OPEN | nsIFileInputStream::SHARE_DELETE;

void FileBlobImpl::CreateInputStream(nsIInputStream** aStream,
                                     ErrorResult& aRv) const {
  nsCOMPtr<nsIInputStream> stream;
  aRv = NS_NewLocalFileInputStream(getter_AddRefs(stream), mFile, -1, -1,
                                   sFileStreamFlags);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (mWholeFile) {
    stream.forget(aStream);
    return;
  }

  MOZ_ASSERT(mLength.isSome());

  RefPtr<SlicedInputStream> slicedInputStream =
      new SlicedInputStream(stream.forget(), mStart, mLength.value());
  slicedInputStream.forget(aStream);
}

bool FileBlobImpl::IsDirectory() const {
  bool isDirectory = false;
  if (mFile) {
    mFile->IsDirectory(&isDirectory);
  }
  return isDirectory;
}

}  // namespace mozilla::dom
