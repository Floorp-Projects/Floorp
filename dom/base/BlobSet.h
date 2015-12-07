/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BlobSet_h
#define mozilla_dom_BlobSet_h

#include "mozilla/CheckedInt.h"
#include "mozilla/dom/File.h"

namespace mozilla {
namespace dom {

class BlobSet {
public:
  BlobSet()
    : mData(nullptr), mDataLen(0), mDataBufferLen(0)
  {}

  ~BlobSet()
  {
    free(mData);
  }

  nsresult AppendVoidPtr(const void* aData, uint32_t aLength);
  nsresult AppendString(const nsAString& aString, bool nativeEOL, JSContext* aCx);
  nsresult AppendBlobImpl(BlobImpl* aBlobImpl);
  nsresult AppendBlobImpls(const nsTArray<RefPtr<BlobImpl>>& aBlobImpls);

  nsTArray<RefPtr<BlobImpl>>& GetBlobImpls() { Flush(); return mBlobImpls; }

  already_AddRefed<Blob> GetBlobInternal(nsISupports* aParent,
                                         const nsACString& aContentType,
                                         ErrorResult& aRv);

protected:
  bool ExpandBufferSize(uint64_t aSize)
  {
    using mozilla::CheckedUint32;

    if (mDataBufferLen >= mDataLen + aSize) {
      mDataLen += aSize;
      return true;
    }

    // Start at 1 or we'll loop forever.
    CheckedUint32 bufferLen =
      std::max<uint32_t>(static_cast<uint32_t>(mDataBufferLen), 1);
    while (bufferLen.isValid() && bufferLen.value() < mDataLen + aSize)
      bufferLen *= 2;

    if (!bufferLen.isValid())
      return false;

    void* data = realloc(mData, bufferLen.value());
    if (!data)
      return false;

    mData = data;
    mDataBufferLen = bufferLen.value();
    mDataLen += aSize;
    return true;
  }

  void Flush() {
    if (mData) {
      // If we have some data, create a blob for it
      // and put it on the stack

      RefPtr<BlobImpl> blobImpl =
        new BlobImplMemory(mData, mDataLen, EmptyString());
      mBlobImpls.AppendElement(blobImpl);
      mData = nullptr; // The nsDOMMemoryFile takes ownership of the buffer
      mDataLen = 0;
      mDataBufferLen = 0;
    }
  }

  nsTArray<RefPtr<BlobImpl>> mBlobImpls;
  void* mData;
  uint64_t mDataLen;
  uint64_t mDataBufferLen;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_BlobSet_h
