/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StreamBlobImpl.h"
#include "nsStringStream.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS_INHERITED(StringBlobImpl, BlobImpl, nsIMemoryReporter)

/* static */ already_AddRefed<StringBlobImpl>
StringBlobImpl::Create(const nsACString& aData, const nsAString& aContentType)
{
  RefPtr<StringBlobImpl> blobImpl = new StringBlobImpl(aData, aContentType);
  RegisterWeakMemoryReporter(blobImpl);
  return blobImpl.forget();
}

StringBlobImpl::StringBlobImpl(const nsACString& aData,
                               const nsAString& aContentType)
  : BaseBlobImpl(aContentType, aData.Length())
  , mData(aData)
{
}

StringBlobImpl::~StringBlobImpl()
{
  UnregisterWeakMemoryReporter(this);
}

already_AddRefed<BlobImpl>
StringBlobImpl::CreateSlice(uint64_t aStart, uint64_t aLength,
                            const nsAString& aContentType,
                            ErrorResult& aRv)
{
  RefPtr<BlobImpl> impl =
    new StringBlobImpl(Substring(mData, aStart, aLength),
                       aContentType);
  return impl.forget();
}

void
StringBlobImpl::GetInternalStream(nsIInputStream** aStream, ErrorResult& aRv)
{
  aRv = NS_NewCStringInputStream(aStream, mData);
}

NS_IMETHODIMP
StringBlobImpl::CollectReports(nsIHandleReportCallback* aHandleReport,
                               nsISupports* aData, bool aAnonymize)
{
  MOZ_COLLECT_REPORT(
    "explicit/dom/memory-file-data/string", KIND_HEAP, UNITS_BYTES,
    mData.SizeOfExcludingThisIfUnshared(MallocSizeOf),
    "Memory used to back a File/Blob based on a string.");
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
