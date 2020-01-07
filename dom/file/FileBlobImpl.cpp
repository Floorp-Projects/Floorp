/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileBlobImpl.h"
#include "mozilla/SlicedInputStream.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "nsCExternalHandlerService.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsIMIMEService.h"
#include "nsNetUtil.h"
#include "nsStreamUtils.h"

namespace mozilla {
namespace dom {

FileBlobImpl::FileBlobImpl(nsIFile* aFile)
    : BaseBlobImpl(NS_LITERAL_STRING("FileBlobImpl"), EmptyString(),
                   EmptyString(), UINT64_MAX, INT64_MAX),
      mFile(aFile),
      mFileId(-1),
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
    : BaseBlobImpl(NS_LITERAL_STRING("FileBlobImpl"), aName, aContentType,
                   aLength, UINT64_MAX),
      mFile(aFile),
      mFileId(-1),
      mWholeFile(true) {
  MOZ_ASSERT(mFile, "must have file");
  MOZ_ASSERT(XRE_IsParentProcess());
  mMozFullPath.SetIsVoid(true);
}

FileBlobImpl::FileBlobImpl(const nsAString& aName,
                           const nsAString& aContentType, uint64_t aLength,
                           nsIFile* aFile, int64_t aLastModificationDate)
    : BaseBlobImpl(NS_LITERAL_STRING("FileBlobImpl"), aName, aContentType,
                   aLength, aLastModificationDate),
      mFile(aFile),
      mFileId(-1),
      mWholeFile(true) {
  MOZ_ASSERT(mFile, "must have file");
  MOZ_ASSERT(XRE_IsParentProcess());
  mMozFullPath.SetIsVoid(true);
}

FileBlobImpl::FileBlobImpl(nsIFile* aFile, const nsAString& aName,
                           const nsAString& aContentType,
                           const nsAString& aBlobImplType)
    : BaseBlobImpl(aBlobImplType, aName, aContentType, UINT64_MAX, INT64_MAX),
      mFile(aFile),
      mFileId(-1),
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
    : BaseBlobImpl(NS_LITERAL_STRING("FileBlobImpl"), aContentType,
                   aOther->mStart + aStart, aLength),
      mFile(aOther->mFile),
      mFileId(-1),
      mWholeFile(false) {
  MOZ_ASSERT(mFile, "must have file");
  MOZ_ASSERT(XRE_IsParentProcess());
  mImmutable = aOther->mImmutable;
  mMozFullPath = aOther->mMozFullPath;
}

already_AddRefed<BlobImpl> FileBlobImpl::CreateSlice(
    uint64_t aStart, uint64_t aLength, const nsAString& aContentType,
    ErrorResult& aRv) {
  RefPtr<FileBlobImpl> impl =
      new FileBlobImpl(this, aStart, aLength, aContentType);
  return impl.forget();
}

void FileBlobImpl::GetMozFullPathInternal(nsAString& aFilename,
                                          ErrorResult& aRv) {
  MOZ_ASSERT(mIsFile, "Should only be called on files");

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
  if (BaseBlobImpl::IsSizeUnknown()) {
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

    mLength = fileSize;
  }

  return mLength;
}

namespace {

class GetTypeRunnable final : public WorkerMainThreadRunnable {
 public:
  GetTypeRunnable(WorkerPrivate* aWorkerPrivate, BlobImpl* aBlobImpl)
      : WorkerMainThreadRunnable(aWorkerPrivate,
                                 NS_LITERAL_CSTRING("FileBlobImpl :: GetType")),
        mBlobImpl(aBlobImpl) {
    MOZ_ASSERT(aBlobImpl);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool MainThreadRun() override {
    MOZ_ASSERT(NS_IsMainThread());

    nsAutoString type;
    mBlobImpl->GetType(type);
    return true;
  }

 private:
  ~GetTypeRunnable() = default;

  RefPtr<BlobImpl> mBlobImpl;
};

}  // anonymous namespace

void FileBlobImpl::GetType(nsAString& aType) {
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
          new GetTypeRunnable(workerPrivate, this);

      ErrorResult rv;
      runnable->Dispatch(Canceling, rv);
      if (NS_WARN_IF(rv.Failed())) {
        rv.SuppressException();
      }
      return;
    }

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

  aType = mContentType;
}

int64_t FileBlobImpl::GetLastModified(ErrorResult& aRv) {
  MOZ_ASSERT(mIsFile, "Should only be called on files");
  if (BaseBlobImpl::IsDateUnknown()) {
    PRTime msecs;
    aRv = mFile->GetLastModifiedTime(&msecs);
    if (NS_WARN_IF(aRv.Failed())) {
      return 0;
    }

    mLastModificationDate = msecs;
  }

  return mLastModificationDate;
}

const uint32_t sFileStreamFlags =
    nsIFileInputStream::CLOSE_ON_EOF | nsIFileInputStream::REOPEN_ON_REWIND |
    nsIFileInputStream::DEFER_OPEN | nsIFileInputStream::SHARE_DELETE;

void FileBlobImpl::CreateInputStream(nsIInputStream** aStream,
                                     ErrorResult& aRv) {
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

  RefPtr<SlicedInputStream> slicedInputStream =
      new SlicedInputStream(stream.forget(), mStart, mLength);
  slicedInputStream.forget(aStream);
}

bool FileBlobImpl::IsDirectory() const {
  bool isDirectory = false;
  if (mFile) {
    mFile->IsDirectory(&isDirectory);
  }
  return isDirectory;
}

}  // namespace dom
}  // namespace mozilla
