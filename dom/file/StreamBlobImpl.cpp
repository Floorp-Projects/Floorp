/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EmptyBlobImpl.h"
#include "mozilla/SlicedInputStream.h"
#include "StreamBlobImpl.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "nsICloneableInputStream.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS_INHERITED(StreamBlobImpl, BlobImpl, nsIMemoryReporter)

/* static */ already_AddRefed<StreamBlobImpl>
StreamBlobImpl::Create(nsIInputStream* aInputStream,
                       const nsAString& aContentType,
                       uint64_t aLength)
{
  RefPtr<StreamBlobImpl> blobImplStream =
    new StreamBlobImpl(aInputStream, aContentType, aLength);
  blobImplStream->MaybeRegisterMemoryReporter();
  return blobImplStream.forget();
}

/* static */ already_AddRefed<StreamBlobImpl>
StreamBlobImpl::Create(nsIInputStream* aInputStream,
                       const nsAString& aName,
                       const nsAString& aContentType,
                       int64_t aLastModifiedDate,
                       uint64_t aLength)
{
  RefPtr<StreamBlobImpl> blobImplStream =
    new StreamBlobImpl(aInputStream, aName, aContentType, aLastModifiedDate,
                       aLength);
  blobImplStream->MaybeRegisterMemoryReporter();
  return blobImplStream.forget();
}

StreamBlobImpl::StreamBlobImpl(nsIInputStream* aInputStream,
                               const nsAString& aContentType,
                               uint64_t aLength)
  : BaseBlobImpl(aContentType, aLength)
  , mInputStream(aInputStream)
  , mIsDirectory(false)
  , mFileId(-1)
{
  mImmutable = true;
}

StreamBlobImpl::StreamBlobImpl(nsIInputStream* aInputStream,
                               const nsAString& aName,
                               const nsAString& aContentType,
                               int64_t aLastModifiedDate,
                               uint64_t aLength)
  : BaseBlobImpl(aName, aContentType, aLength, aLastModifiedDate)
  , mInputStream(aInputStream)
  , mIsDirectory(false)
  , mFileId(-1)
{
  mImmutable = true;
}

StreamBlobImpl::~StreamBlobImpl()
{
  UnregisterWeakMemoryReporter(this);
}

void
StreamBlobImpl::CreateInputStream(nsIInputStream** aStream, ErrorResult& aRv)
{
  nsCOMPtr<nsIInputStream> clonedStream;
  nsCOMPtr<nsIInputStream> replacementStream;

  aRv = NS_CloneInputStream(mInputStream, getter_AddRefs(clonedStream),
                            getter_AddRefs(replacementStream));
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (replacementStream) {
    mInputStream = replacementStream.forget();
  }

  clonedStream.forget(aStream);
}

already_AddRefed<BlobImpl>
StreamBlobImpl::CreateSlice(uint64_t aStart, uint64_t aLength,
                            const nsAString& aContentType, ErrorResult& aRv)
{
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

  RefPtr<BlobImpl> impl =
    new StreamBlobImpl(clonedStream, aContentType, aLength);
  return impl.forget();
}

void
StreamBlobImpl::MaybeRegisterMemoryReporter()
{
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
                               nsISupports* aData, bool aAnonymize)
{
  nsCOMPtr<nsIStringInputStream> stringInputStream =
    do_QueryInterface(mInputStream);
  if (!stringInputStream) {
    return NS_OK;
  }

  MOZ_COLLECT_REPORT(
    "explicit/dom/memory-file-data/stream", KIND_HEAP, UNITS_BYTES,
    stringInputStream->SizeOfIncludingThis(MallocSizeOf),
    "Memory used to back a File/Blob based on an input stream.");

  return NS_OK;
}

size_t
StreamBlobImpl::GetAllocationSize() const
{
  nsCOMPtr<nsIStringInputStream> stringInputStream =
    do_QueryInterface(mInputStream);
  if (!stringInputStream) {
    return 0;
  }

  return stringInputStream->SizeOfIncludingThis(MallocSizeOf);
}

} // namespace dom
} // namespace mozilla
