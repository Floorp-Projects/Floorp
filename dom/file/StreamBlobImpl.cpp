/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmptyBlobImpl.h"
#include "mozilla/InputStreamLengthWrapper.h"
#include "mozilla/SlicedInputStream.h"
#include "StreamBlobImpl.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsICloneableInputStream.h"
#include "nsIEventTarget.h"
#include "nsIPipe.h"
#include "js/GCAPI.h"

namespace mozilla::dom {

NS_IMPL_ISUPPORTS_INHERITED(StreamBlobImpl, BlobImpl, nsIMemoryReporter)

static already_AddRefed<nsICloneableInputStream> EnsureCloneableStream(
    nsIInputStream* aInputStream, uint64_t aLength) {
  nsCOMPtr<nsICloneableInputStream> cloneable = do_QueryInterface(aInputStream);
  if (cloneable && cloneable->GetCloneable()) {
    return cloneable.forget();
  }

  // If the stream we're copying is known to be small, specify the size of the
  // pipe's segments precisely to limit wasted space. An extra byte above length
  // is included to avoid allocating an extra segment immediately before reading
  // EOF from the source stream.  Otherwise, allocate 64k buffers rather than
  // the default of 4k buffers to reduce segment counts for very large payloads.
  static constexpr uint32_t kBaseSegmentSize = 64 * 1024;
  uint32_t segmentSize = kBaseSegmentSize;
  if (aLength + 1 <= kBaseSegmentSize * 4) {
    segmentSize = aLength + 1;
  }

  // NOTE: We specify unlimited segments to eagerly build a complete copy of the
  // source stream locally without waiting for the blob to be consumed.
  nsCOMPtr<nsIAsyncInputStream> reader;
  nsCOMPtr<nsIAsyncOutputStream> writer;
  NS_NewPipe2(getter_AddRefs(reader), getter_AddRefs(writer), true, true,
              segmentSize, UINT32_MAX);

  nsresult rv;
  nsCOMPtr<nsIEventTarget> target =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  rv = NS_AsyncCopy(aInputStream, writer, target,
                    NS_ASYNCCOPY_VIA_WRITESEGMENTS, segmentSize);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  cloneable = do_QueryInterface(reader);
  MOZ_ASSERT(cloneable && cloneable->GetCloneable());
  return cloneable.forget();
}

/* static */
already_AddRefed<StreamBlobImpl> StreamBlobImpl::Create(
    already_AddRefed<nsIInputStream> aInputStream,
    const nsAString& aContentType, uint64_t aLength,
    const nsAString& aBlobImplType) {
  nsCOMPtr<nsIInputStream> inputStream = std::move(aInputStream);
  nsCOMPtr<nsICloneableInputStream> cloneable =
      EnsureCloneableStream(inputStream, aLength);

  RefPtr<StreamBlobImpl> blobImplStream = new StreamBlobImpl(
      cloneable.forget(), aContentType, aLength, aBlobImplType);
  blobImplStream->MaybeRegisterMemoryReporter();
  return blobImplStream.forget();
}

/* static */
already_AddRefed<StreamBlobImpl> StreamBlobImpl::Create(
    already_AddRefed<nsIInputStream> aInputStream, const nsAString& aName,
    const nsAString& aContentType, int64_t aLastModifiedDate, uint64_t aLength,
    const nsAString& aBlobImplType) {
  nsCOMPtr<nsIInputStream> inputStream = std::move(aInputStream);
  nsCOMPtr<nsICloneableInputStream> cloneable =
      EnsureCloneableStream(inputStream, aLength);

  RefPtr<StreamBlobImpl> blobImplStream =
      new StreamBlobImpl(cloneable.forget(), aName, aContentType,
                         aLastModifiedDate, aLength, aBlobImplType);
  blobImplStream->MaybeRegisterMemoryReporter();
  return blobImplStream.forget();
}

StreamBlobImpl::StreamBlobImpl(
    already_AddRefed<nsICloneableInputStream> aInputStream,
    const nsAString& aContentType, uint64_t aLength,
    const nsAString& aBlobImplType)
    : BaseBlobImpl(aContentType, aLength),
      mInputStream(std::move(aInputStream)),
      mBlobImplType(aBlobImplType),
      mIsDirectory(false),
      mFileId(-1) {}

StreamBlobImpl::StreamBlobImpl(
    already_AddRefed<nsICloneableInputStream> aInputStream,
    const nsAString& aName, const nsAString& aContentType,
    int64_t aLastModifiedDate, uint64_t aLength, const nsAString& aBlobImplType)
    : BaseBlobImpl(aName, aContentType, aLength, aLastModifiedDate),
      mInputStream(std::move(aInputStream)),
      mBlobImplType(aBlobImplType),
      mIsDirectory(false),
      mFileId(-1) {}

StreamBlobImpl::~StreamBlobImpl() {
  if (mInputStream) {
    nsCOMPtr<nsIInputStream> stream = do_QueryInterface(mInputStream);
    stream->Close();
  }
  UnregisterWeakMemoryReporter(this);
}

void StreamBlobImpl::CreateInputStream(nsIInputStream** aStream,
                                       ErrorResult& aRv) const {
  if (!mInputStream) {
    // We failed to clone the input stream in EnsureCloneableStream.
    *aStream = nullptr;
    aRv.ThrowUnknownError("failed to read blob data");
    return;
  }

  nsCOMPtr<nsIInputStream> clonedStream;
  aRv = mInputStream->Clone(getter_AddRefs(clonedStream));
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsCOMPtr<nsIInputStream> wrappedStream =
      InputStreamLengthWrapper::MaybeWrap(clonedStream.forget(), mLength);

  wrappedStream.forget(aStream);
}

already_AddRefed<BlobImpl> StreamBlobImpl::CreateSlice(
    uint64_t aStart, uint64_t aLength, const nsAString& aContentType,
    ErrorResult& aRv) const {
  if (!aLength) {
    RefPtr<BlobImpl> impl = new EmptyBlobImpl(aContentType);
    return impl.forget();
  }

  nsCOMPtr<nsIInputStream> clonedStream;

  nsCOMPtr<nsICloneableInputStreamWithRange> stream =
      do_QueryInterface(mInputStream);
  if (stream) {
    aRv = stream->CloneWithRange(aStart, aLength, getter_AddRefs(clonedStream));
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  } else {
    CreateInputStream(getter_AddRefs(clonedStream), aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    clonedStream =
        new SlicedInputStream(clonedStream.forget(), aStart, aLength);
  }

  MOZ_ASSERT(clonedStream);

  return StreamBlobImpl::Create(clonedStream.forget(), aContentType, aLength,
                                mBlobImplType);
}

void StreamBlobImpl::MaybeRegisterMemoryReporter() {
  // We report only stringInputStream.
  nsCOMPtr<nsIStringInputStream> stringInputStream =
      do_QueryInterface(mInputStream);
  if (!stringInputStream) {
    return;
  }

  RegisterWeakMemoryReporter(this);
}

NS_IMETHODIMP
StreamBlobImpl::CollectReports(nsIHandleReportCallback* aHandleReport,
                               nsISupports* aData, bool aAnonymize) {
  nsCOMPtr<nsIStringInputStream> stringInputStream =
      do_QueryInterface(mInputStream);
  if (!stringInputStream) {
    return NS_OK;
  }

  MOZ_COLLECT_REPORT(
      "explicit/dom/memory-file-data/stream", KIND_HEAP, UNITS_BYTES,
      stringInputStream->SizeOfIncludingThisIfUnshared(MallocSizeOf),
      "Memory used to back a File/Blob based on an input stream.");

  return NS_OK;
}

size_t StreamBlobImpl::GetAllocationSize() const {
  // do_QueryInterface may run GC if the object is implemented in JS, but
  // mInputStream is nsICloneableInputStream which doesn't have a JS
  // implementation.
  JS::AutoSuppressGCAnalysis nogc;

  nsCOMPtr<nsIStringInputStream> stringInputStream =
      do_QueryInterface(mInputStream);
  if (!stringInputStream) {
    return 0;
  }

  return stringInputStream->SizeOfIncludingThisEvenIfShared(MallocSizeOf);
}

void StreamBlobImpl::GetBlobImplType(nsAString& aBlobImplType) const {
  aBlobImplType.AssignLiteral("StreamBlobImpl[");
  aBlobImplType.Append(mBlobImplType);
  aBlobImplType.AppendLiteral("]");
}

}  // namespace mozilla::dom
