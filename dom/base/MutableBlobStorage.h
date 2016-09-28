/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MutableBlobStorage_h
#define mozilla_dom_MutableBlobStorage_h

#include "mozilla/RefPtr.h"

namespace mozilla {
namespace dom {

class Blob;
class BlobImpl;

class MutableBlobStorage final
{
public:
  MutableBlobStorage()
    : mData(nullptr)
    , mDataLen(0)
    , mDataBufferLen(0)
  {}

  ~MutableBlobStorage()
  {
    free(mData);
  }

  nsresult Append(const void* aData, uint32_t aLength);

  already_AddRefed<Blob> GetBlob(nsISupports* aParent,
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

#endif // mozilla_dom_MutableBlobStorage_h
