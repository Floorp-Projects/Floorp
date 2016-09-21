/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BlobSet.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/dom/File.h"
#include "MultipartBlobImpl.h"

namespace mozilla {
namespace dom {

already_AddRefed<Blob>
BlobSet::GetBlobInternal(nsISupports* aParent,
                         const nsACString& aContentType,
                         ErrorResult& aRv)
{
  nsTArray<RefPtr<BlobImpl>> subImpls(GetBlobImpls());
  RefPtr<BlobImpl> blobImpl =
    MultipartBlobImpl::Create(Move(subImpls),
                              NS_ConvertASCIItoUTF16(aContentType),
                              aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<Blob> blob = Blob::Create(aParent, blobImpl);
  return blob.forget();
}

nsresult
BlobSet::AppendVoidPtr(const void* aData, uint32_t aLength)
{
  NS_ENSURE_ARG_POINTER(aData);
  if (!aLength) {
    return NS_OK;
  }

  uint64_t offset = mDataLen;

  if (!ExpandBufferSize(aLength)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  memcpy((char*)mData + offset, aData, aLength);
  return NS_OK;
}

nsresult
BlobSet::AppendString(const nsAString& aString, bool nativeEOL, JSContext* aCx)
{
  nsCString utf8Str = NS_ConvertUTF16toUTF8(aString);

  if (nativeEOL) {
    if (utf8Str.Contains('\r')) {
      utf8Str.ReplaceSubstring("\r\n", "\n");
      utf8Str.ReplaceSubstring("\r", "\n");
    }
#ifdef XP_WIN
    utf8Str.ReplaceSubstring("\n", "\r\n");
#endif
  }

  return AppendVoidPtr((void*)utf8Str.Data(),
                       utf8Str.Length());
}

nsresult
BlobSet::AppendBlobImpl(BlobImpl* aBlobImpl)
{
  NS_ENSURE_ARG_POINTER(aBlobImpl);

  Flush();
  mBlobImpls.AppendElement(aBlobImpl);

  return NS_OK;
}

nsresult
BlobSet::AppendBlobImpls(const nsTArray<RefPtr<BlobImpl>>& aBlobImpls)
{
  Flush();
  mBlobImpls.AppendElements(aBlobImpls);

  return NS_OK;
}

bool
BlobSet::ExpandBufferSize(uint64_t aSize)
{
  if (mDataBufferLen >= mDataLen + aSize) {
    mDataLen += aSize;
    return true;
  }

  // Start at 1 or we'll loop forever.
  CheckedUint32 bufferLen =
    std::max<uint32_t>(static_cast<uint32_t>(mDataBufferLen), 1);
  while (bufferLen.isValid() && bufferLen.value() < mDataLen + aSize) {
    bufferLen *= 2;
  }

  if (!bufferLen.isValid()) {
    return false;
  }

  void* data = realloc(mData, bufferLen.value());
  if (!data) {
    return false;
  }

  mData = data;
  mDataBufferLen = bufferLen.value();
  mDataLen += aSize;
  return true;
}

void
BlobSet::Flush()
{
  if (mData) {
    // If we have some data, create a blob for it
    // and put it on the stack

    RefPtr<BlobImpl> blobImpl =
      new BlobImplMemory(mData, mDataLen, EmptyString());
    mBlobImpls.AppendElement(blobImpl);

    mData = nullptr; // The nsDOMMemoryFile takes ownership of the buffer
    mDataLen = 0;
    mDataBufferLen = 0;
    return;
  }
}

} // dom namespace
} // mozilla namespace
