/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BlobSet.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/dom/File.h"
#include "MemoryBlobImpl.h"
#include "MultipartBlobImpl.h"
#include "StringBlobImpl.h"

namespace mozilla::dom {

nsresult BlobSet::AppendVoidPtr(const void* aData, uint32_t aLength) {
  NS_ENSURE_ARG_POINTER(aData);
  if (!aLength) {
    return NS_OK;
  }

  void* data = malloc(aLength);
  if (!data) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  memcpy((char*)data, aData, aLength);

  RefPtr<BlobImpl> blobImpl = new MemoryBlobImpl(data, aLength, u""_ns);
  return AppendBlobImpl(blobImpl);
}

nsresult BlobSet::AppendVector(Vector<uint8_t>&& aData) {
  size_t length = aData.length();
  RefPtr<BlobImpl> blobImpl =
      new MemoryBlobImpl(aData.extractOrCopyRawBuffer(), length, u""_ns);
  return AppendBlobImpl(blobImpl);
}

nsresult BlobSet::AppendUTF8String(const nsACString& aUTF8String,
                                   bool nativeEOL) {
  nsCString utf8Str;
  if (NS_WARN_IF(!utf8Str.Assign(aUTF8String, mozilla::fallible))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (nativeEOL) {
    if (utf8Str.Contains('\r')) {
      if (NS_WARN_IF(
              !utf8Str.ReplaceSubstring("\r\n", "\n", mozilla::fallible) ||
              !utf8Str.ReplaceSubstring("\r", "\n", mozilla::fallible))) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
#ifdef XP_WIN
    if (NS_WARN_IF(
            !utf8Str.ReplaceSubstring("\n", "\r\n", mozilla::fallible))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
#endif
  }

  RefPtr<StringBlobImpl> blobImpl = StringBlobImpl::Create(utf8Str, u""_ns);
  return AppendBlobImpl(blobImpl);
}

nsresult BlobSet::AppendBlobImpl(BlobImpl* aBlobImpl) {
  NS_ENSURE_ARG_POINTER(aBlobImpl);

  // If aBlobImpl is a MultipartBlobImpl, let's append the sub-blobImpls
  // instead.
  const nsTArray<RefPtr<BlobImpl>>* subBlobs = aBlobImpl->GetSubBlobImpls();
  if (subBlobs) {
    for (BlobImpl* subBlob : *subBlobs) {
      nsresult rv = AppendBlobImpl(subBlob);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    return NS_OK;
  }

  if (NS_WARN_IF(!mBlobImpls.AppendElement(aBlobImpl, fallible))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

}  // namespace mozilla::dom
