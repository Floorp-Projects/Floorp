/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BlobSet_h
#define mozilla_dom_BlobSet_h

#include "mozilla/RefPtr.h"

namespace mozilla {
namespace dom {

class Blob;
class BlobImpl;

class BlobSet final
{
public:
  BlobSet()
    : mData(nullptr)
    , mDataLen(0)
    , mDataBufferLen(0)
  {}

  ~BlobSet()
  {
    free(mData);
  }

  nsresult AppendVoidPtr(const void* aData, uint32_t aLength);

  nsresult AppendString(const nsAString& aString, bool nativeEOL,
                        JSContext* aCx);

  nsresult AppendBlobImpl(BlobImpl* aBlobImpl);

  nsresult AppendBlobImpls(const nsTArray<RefPtr<BlobImpl>>& aBlobImpls);

  nsTArray<RefPtr<BlobImpl>>& GetBlobImpls() { Flush(); return mBlobImpls; }

  already_AddRefed<Blob> GetBlobInternal(nsISupports* aParent,
                                         const nsACString& aContentType,
                                         ErrorResult& aRv);

private:
  bool ExpandBufferSize(uint64_t aSize);

  void Flush();

  nsTArray<RefPtr<BlobImpl>> mBlobImpls;
  void* mData;
  uint64_t mDataLen;
  uint64_t mDataBufferLen;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_BlobSet_h
