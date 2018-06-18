/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MultipartBlobImpl.h"
#include "jsfriendapi.h"
#include "mozilla/dom/BlobSet.h"
#include "mozilla/dom/FileBinding.h"
#include "mozilla/dom/UnionTypes.h"
#include "nsIMultiplexInputStream.h"
#include "nsRFPService.h"
#include "nsStringStream.h"
#include "nsTArray.h"
#include "nsJSUtils.h"
#include "nsContentUtils.h"
#include "nsIScriptError.h"
#include "nsIXPConnect.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::dom;

/* static */ already_AddRefed<MultipartBlobImpl>
MultipartBlobImpl::Create(nsTArray<RefPtr<BlobImpl>>&& aBlobImpls,
                          const nsAString& aName,
                          const nsAString& aContentType,
                          ErrorResult& aRv)
{
  RefPtr<MultipartBlobImpl> blobImpl =
    new MultipartBlobImpl(std::move(aBlobImpls), aName, aContentType);
  blobImpl->SetLengthAndModifiedDate(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return blobImpl.forget();
}

/* static */ already_AddRefed<MultipartBlobImpl>
MultipartBlobImpl::Create(nsTArray<RefPtr<BlobImpl>>&& aBlobImpls,
                          const nsAString& aContentType,
                          ErrorResult& aRv)
{
  RefPtr<MultipartBlobImpl> blobImpl =
    new MultipartBlobImpl(std::move(aBlobImpls), aContentType);
  blobImpl->SetLengthAndModifiedDate(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return blobImpl.forget();
}

void
MultipartBlobImpl::CreateInputStream(nsIInputStream** aStream,
                                     ErrorResult& aRv)
{
  *aStream = nullptr;

  uint32_t length = mBlobImpls.Length();
  if (length == 0) {
    aRv = NS_NewCStringInputStream(aStream, EmptyCString());
    return;
  }

  if (length == 1) {
    BlobImpl* blobImpl = mBlobImpls.ElementAt(0);
    blobImpl->CreateInputStream(aStream, aRv);
    return;
  }

  nsCOMPtr<nsIMultiplexInputStream> stream =
    do_CreateInstance("@mozilla.org/io/multiplex-input-stream;1");
  if (NS_WARN_IF(!stream)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  uint32_t i;
  for (i = 0; i < length; i++) {
    nsCOMPtr<nsIInputStream> scratchStream;
    BlobImpl* blobImpl = mBlobImpls.ElementAt(i).get();

    blobImpl->CreateInputStream(getter_AddRefs(scratchStream), aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    aRv = stream->AppendStream(scratchStream);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }
  }

  CallQueryInterface(stream, aStream);
}

already_AddRefed<BlobImpl>
MultipartBlobImpl::CreateSlice(uint64_t aStart, uint64_t aLength,
                               const nsAString& aContentType,
                               ErrorResult& aRv)
{
  // If we clamped to nothing we create an empty blob
  nsTArray<RefPtr<BlobImpl>> blobImpls;

  uint64_t length = aLength;
  uint64_t skipStart = aStart;

  // Prune the list of blobs if we can
  uint32_t i;
  for (i = 0; length && skipStart && i < mBlobImpls.Length(); i++) {
    BlobImpl* blobImpl = mBlobImpls[i].get();

    uint64_t l = blobImpl->GetSize(aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    if (skipStart < l) {
      uint64_t upperBound = std::min<uint64_t>(l - skipStart, length);

      RefPtr<BlobImpl> firstBlobImpl =
        blobImpl->CreateSlice(skipStart, upperBound,
                              aContentType, aRv);
      if (NS_WARN_IF(aRv.Failed())) {
        return nullptr;
      }

      // Avoid wrapping a single blob inside an MultipartBlobImpl
      if (length == upperBound) {
        return firstBlobImpl.forget();
      }

      blobImpls.AppendElement(firstBlobImpl);
      length -= upperBound;
      i++;
      break;
    }
    skipStart -= l;
  }

  // Now append enough blobs until we're done
  for (; length && i < mBlobImpls.Length(); i++) {
    BlobImpl* blobImpl = mBlobImpls[i].get();

    uint64_t l = blobImpl->GetSize(aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    if (length < l) {
      RefPtr<BlobImpl> lastBlobImpl =
        blobImpl->CreateSlice(0, length, aContentType, aRv);
      if (NS_WARN_IF(aRv.Failed())) {
        return nullptr;
      }

      blobImpls.AppendElement(lastBlobImpl);
    } else {
      blobImpls.AppendElement(blobImpl);
    }
    length -= std::min<uint64_t>(l, length);
  }

  // we can create our blob now
  RefPtr<BlobImpl> impl = Create(std::move(blobImpls), aContentType, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return impl.forget();
}

void
MultipartBlobImpl::InitializeBlob(ErrorResult& aRv)
{
  SetLengthAndModifiedDate(aRv);
  NS_WARNING_ASSERTION(!aRv.Failed(), "SetLengthAndModifiedDate failed");
}

void
MultipartBlobImpl::InitializeBlob(const Sequence<Blob::BlobPart>& aData,
                                  const nsAString& aContentType,
                                  bool aNativeEOL,
                                  ErrorResult& aRv)
{
  mContentType = aContentType;
  BlobSet blobSet;

  for (uint32_t i = 0, len = aData.Length(); i < len; ++i) {
    const Blob::BlobPart& data = aData[i];

    if (data.IsBlob()) {
      RefPtr<Blob> blob = data.GetAsBlob().get();
      aRv = blobSet.AppendBlobImpl(blob->Impl());
      if (aRv.Failed()) {
        return;
      }
    }

    else if (data.IsUSVString()) {
      aRv = blobSet.AppendString(data.GetAsUSVString(), aNativeEOL);
      if (aRv.Failed()) {
        return;
      }
    }

    else if (data.IsArrayBuffer()) {
      const ArrayBuffer& buffer = data.GetAsArrayBuffer();
      buffer.ComputeLengthAndData();
      aRv = blobSet.AppendVoidPtr(buffer.Data(), buffer.Length());
      if (aRv.Failed()) {
        return;
      }
    }

    else if (data.IsArrayBufferView()) {
      const ArrayBufferView& buffer = data.GetAsArrayBufferView();
      buffer.ComputeLengthAndData();
      aRv = blobSet.AppendVoidPtr(buffer.Data(), buffer.Length());
      if (aRv.Failed()) {
        return;
      }
    }

    else {
      MOZ_CRASH("Impossible blob data type.");
    }
  }


  mBlobImpls = blobSet.GetBlobImpls();
  SetLengthAndModifiedDate(aRv);
  NS_WARNING_ASSERTION(!aRv.Failed(), "SetLengthAndModifiedDate failed");
}

void
MultipartBlobImpl::SetLengthAndModifiedDate(ErrorResult& aRv)
{
  MOZ_ASSERT(mLength == UINT64_MAX);
  MOZ_ASSERT(mLastModificationDate == INT64_MAX);

  uint64_t totalLength = 0;
  int64_t lastModified = 0;
  bool lastModifiedSet = false;

  for (uint32_t index = 0, count = mBlobImpls.Length(); index < count; index++) {
    RefPtr<BlobImpl>& blob = mBlobImpls[index];

#ifdef DEBUG
    MOZ_ASSERT(!blob->IsSizeUnknown());
    MOZ_ASSERT(!blob->IsDateUnknown());
#endif

    uint64_t subBlobLength = blob->GetSize(aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    MOZ_ASSERT(UINT64_MAX - subBlobLength >= totalLength);
    totalLength += subBlobLength;

    if (blob->IsFile()) {
      int64_t partLastModified = blob->GetLastModified(aRv);
      if (NS_WARN_IF(aRv.Failed())) {
        return;
      }

      if (lastModified < partLastModified) {
        lastModified = partLastModified;
        lastModifiedSet = true;
      }
    }
  }

  mLength = totalLength;

  if (mIsFile) {
    // We cannot use PR_Now() because bug 493756 and, for this reason:
    //   var x = new Date(); var f = new File(...);
    //   x.getTime() < f.dateModified.getTime()
    // could fail.
    mLastModificationDate = nsRFPService::ReduceTimePrecisionAsUSecs(
      lastModifiedSet ? lastModified * PR_USEC_PER_MSEC : JS_Now(), 0);
    // mLastModificationDate is an absolute timestamp so we supply a zero context mix-in
  }
}

void
MultipartBlobImpl::GetMozFullPathInternal(nsAString& aFilename,
                                          ErrorResult& aRv) const
{
  if (!mIsFromNsIFile || mBlobImpls.Length() == 0) {
    BaseBlobImpl::GetMozFullPathInternal(aFilename, aRv);
    return;
  }

  BlobImpl* blobImpl = mBlobImpls.ElementAt(0).get();
  if (!blobImpl) {
    BaseBlobImpl::GetMozFullPathInternal(aFilename, aRv);
    return;
  }

  blobImpl->GetMozFullPathInternal(aFilename, aRv);
}

nsresult
MultipartBlobImpl::SetMutable(bool aMutable)
{
  nsresult rv;

  // This looks a little sketchy since BlobImpl objects are supposed to be
  // threadsafe. However, we try to enforce that all BlobImpl objects must be
  // set to immutable *before* being passed to another thread, so this should
  // be safe.
  if (!aMutable && !mImmutable && !mBlobImpls.IsEmpty()) {
    for (uint32_t index = 0, count = mBlobImpls.Length();
         index < count;
         index++) {
      rv = mBlobImpls[index]->SetMutable(aMutable);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  rv = BaseBlobImpl::SetMutable(aMutable);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT_IF(!aMutable, mImmutable);

  return NS_OK;
}

nsresult
MultipartBlobImpl::InitializeChromeFile(nsIFile* aFile,
                                        const nsAString& aType,
                                        const nsAString& aName,
                                        bool aLastModifiedPassed,
                                        int64_t aLastModified,
                                        bool aIsFromNsIFile)
{
  MOZ_ASSERT(!mImmutable, "Something went wrong ...");
  if (mImmutable) {
    return NS_ERROR_UNEXPECTED;
  }

  mName = aName;
  mContentType = aType;
  mIsFromNsIFile = aIsFromNsIFile;

  bool exists;
  nsresult rv= aFile->Exists(&exists);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!exists) {
    return NS_ERROR_FILE_NOT_FOUND;
  }

  bool isDir;
  rv = aFile->IsDirectory(&isDir);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (isDir) {
    return NS_ERROR_FILE_IS_DIRECTORY;
  }

  if (mName.IsEmpty()) {
    aFile->GetLeafName(mName);
  }

  RefPtr<File> blob = File::CreateFromFile(nullptr, aFile);

  // Pre-cache size.
  ErrorResult error;
  blob->GetSize(error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  // Pre-cache modified date.
  blob->GetLastModified(error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  // XXXkhuey this is terrible
  if (mContentType.IsEmpty()) {
    blob->GetType(mContentType);
  }

  BlobSet blobSet;
  rv = blobSet.AppendBlobImpl(static_cast<File*>(blob.get())->Impl());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mBlobImpls = blobSet.GetBlobImpls();

  SetLengthAndModifiedDate(error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  if (aLastModifiedPassed) {
    SetLastModified(aLastModified);
  }

  return NS_OK;
}

bool
MultipartBlobImpl::MayBeClonedToOtherThreads() const
{
  for (uint32_t i = 0; i < mBlobImpls.Length(); ++i) {
    if (!mBlobImpls[i]->MayBeClonedToOtherThreads()) {
      return false;
    }
  }

  return true;
}

size_t MultipartBlobImpl::GetAllocationSize() const
{
  FallibleTArray<BlobImpl*> visitedBlobs;

  // We want to report the unique blob allocation, avoiding duplicated blobs in
  // the multipart blob tree.
  size_t total = 0;
  for (uint32_t i = 0; i < mBlobImpls.Length(); ++i) {
    total += mBlobImpls[i]->GetAllocationSize(visitedBlobs);
  }

  return total;
}

size_t MultipartBlobImpl::GetAllocationSize(FallibleTArray<BlobImpl*>& aVisitedBlobs) const
{
  FallibleTArray<BlobImpl*> visitedBlobs;

  size_t total = 0;
  for (BlobImpl* blobImpl : mBlobImpls) {
    if (!aVisitedBlobs.Contains(blobImpl)) {
      if (NS_WARN_IF(!aVisitedBlobs.AppendElement(blobImpl, fallible))) {
        return 0;
      }
      total += blobImpl->GetAllocationSize(aVisitedBlobs);
    }
  }

  return total;
}
