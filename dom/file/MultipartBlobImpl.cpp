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
#include <algorithm>

using namespace mozilla;
using namespace mozilla::dom;

/* static */
already_AddRefed<MultipartBlobImpl> MultipartBlobImpl::Create(
    nsTArray<RefPtr<BlobImpl>>&& aBlobImpls, const nsAString& aName,
    const nsAString& aContentType, bool aCrossOriginIsolated,
    ErrorResult& aRv) {
  RefPtr<MultipartBlobImpl> blobImpl =
      new MultipartBlobImpl(std::move(aBlobImpls), aName, aContentType);
  blobImpl->SetLengthAndModifiedDate(Some(aCrossOriginIsolated), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return blobImpl.forget();
}

/* static */
already_AddRefed<MultipartBlobImpl> MultipartBlobImpl::Create(
    nsTArray<RefPtr<BlobImpl>>&& aBlobImpls, const nsAString& aContentType,
    ErrorResult& aRv) {
  RefPtr<MultipartBlobImpl> blobImpl =
      new MultipartBlobImpl(std::move(aBlobImpls), aContentType);
  blobImpl->SetLengthAndModifiedDate(/* aCrossOriginIsolated */ Nothing(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return blobImpl.forget();
}

void MultipartBlobImpl::CreateInputStream(nsIInputStream** aStream,
                                          ErrorResult& aRv) {
  *aStream = nullptr;

  uint32_t length = mBlobImpls.Length();
  if (length == 0 || mLength == 0) {
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

    // nsIMultiplexInputStream doesn't work well with empty sub streams. Let's
    // skip the empty blobs.
    uint32_t size = blobImpl->GetSize(aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    if (size == 0) {
      continue;
    }

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

already_AddRefed<BlobImpl> MultipartBlobImpl::CreateSlice(
    uint64_t aStart, uint64_t aLength, const nsAString& aContentType,
    ErrorResult& aRv) {
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
          blobImpl->CreateSlice(skipStart, upperBound, aContentType, aRv);
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

void MultipartBlobImpl::InitializeBlob(bool aCrossOriginIsolated,
                                       ErrorResult& aRv) {
  SetLengthAndModifiedDate(Some(aCrossOriginIsolated), aRv);
  NS_WARNING_ASSERTION(!aRv.Failed(), "SetLengthAndModifiedDate failed");
}

void MultipartBlobImpl::InitializeBlob(const Sequence<Blob::BlobPart>& aData,
                                       const nsAString& aContentType,
                                       bool aNativeEOL,
                                       bool aCrossOriginIsolated,
                                       ErrorResult& aRv) {
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
      buffer.ComputeState();
      aRv = blobSet.AppendVoidPtr(buffer.Data(), buffer.Length());
      if (aRv.Failed()) {
        return;
      }
    }

    else if (data.IsArrayBufferView()) {
      const ArrayBufferView& buffer = data.GetAsArrayBufferView();
      buffer.ComputeState();
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
  SetLengthAndModifiedDate(Some(aCrossOriginIsolated), aRv);
  NS_WARNING_ASSERTION(!aRv.Failed(), "SetLengthAndModifiedDate failed");
}

void MultipartBlobImpl::SetLengthAndModifiedDate(
    const Maybe<bool>& aCrossOriginIsolated, ErrorResult& aRv) {
  MOZ_ASSERT(mLength == MULTIPARTBLOBIMPL_UNKNOWN_LENGTH);
  MOZ_ASSERT_IF(mIsFile, IsLastModificationDateUnset());

  uint64_t totalLength = 0;
  int64_t lastModified = 0;
  bool lastModifiedSet = false;

  for (uint32_t index = 0, count = mBlobImpls.Length(); index < count;
       index++) {
    RefPtr<BlobImpl>& blob = mBlobImpls[index];

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
        lastModified = partLastModified * PR_USEC_PER_MSEC;
        lastModifiedSet = true;
      }
    }
  }

  mLength = totalLength;

  if (mIsFile) {
    if (lastModifiedSet) {
      SetLastModificationDatePrecisely(lastModified);
    } else {
      MOZ_ASSERT(aCrossOriginIsolated.isSome());

      // We cannot use PR_Now() because bug 493756 and, for this reason:
      //   var x = new Date(); var f = new File(...);
      //   x.getTime() < f.dateModified.getTime()
      // could fail.
      SetLastModificationDate(aCrossOriginIsolated.value(), JS_Now());
    }
  }
}

size_t MultipartBlobImpl::GetAllocationSize() const {
  FallibleTArray<BlobImpl*> visitedBlobs;

  // We want to report the unique blob allocation, avoiding duplicated blobs in
  // the multipart blob tree.
  size_t total = 0;
  for (uint32_t i = 0; i < mBlobImpls.Length(); ++i) {
    total += mBlobImpls[i]->GetAllocationSize(visitedBlobs);
  }

  return total;
}

size_t MultipartBlobImpl::GetAllocationSize(
    FallibleTArray<BlobImpl*>& aVisitedBlobs) const {
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

void MultipartBlobImpl::GetBlobImplType(nsAString& aBlobImplType) const {
  aBlobImplType.AssignLiteral("MultipartBlobImpl[");

  for (uint32_t i = 0; i < mBlobImpls.Length(); ++i) {
    if (i != 0) {
      aBlobImplType.AppendLiteral(", ");
    }

    nsAutoString blobImplType;
    mBlobImpls[i]->GetBlobImplType(blobImplType);

    aBlobImplType.Append(blobImplType);
  }

  aBlobImplType.AppendLiteral("]");
}

void MultipartBlobImpl::SetLastModified(int64_t aLastModified) {
  SetLastModificationDatePrecisely(aLastModified * PR_USEC_PER_MSEC);
}
