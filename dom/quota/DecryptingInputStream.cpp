/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecryptingInputStream.h"
#include "DecryptingInputStream_impl.h"

namespace mozilla::dom::quota {

NS_IMPL_ISUPPORTS(DecryptingInputStreamBase, nsIInputStream);

DecryptingInputStreamBase::DecryptingInputStreamBase(
    MovingNotNull<nsCOMPtr<nsIInputStream>> aBaseStream, size_t aBlockSize)
    : mBaseStream(std::move(aBaseStream)), mBlockSize(aBlockSize) {}

NS_IMETHODIMP DecryptingInputStreamBase::Read(char* aBuf, uint32_t aCount,
                                              uint32_t* aBytesReadOut) {
  return ReadSegments(NS_CopySegmentToBuffer, aBuf, aCount, aBytesReadOut);
}

NS_IMETHODIMP DecryptingInputStreamBase::IsNonBlocking(bool* aNonBlockingOut) {
  *aNonBlockingOut = false;
  return NS_OK;
}

size_t DecryptingInputStreamBase::PlainLength() const {
  MOZ_ASSERT(mNextByte <= mPlainBytes);
  return mPlainBytes - mNextByte;
}

size_t DecryptingInputStreamBase::EncryptedBufferLength() const {
  return mBlockSize;
}

}  // namespace mozilla::dom::quota
