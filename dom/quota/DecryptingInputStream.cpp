/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecryptingInputStream.h"
#include "DecryptingInputStream_impl.h"

namespace mozilla::dom::quota {

NS_IMPL_ADDREF(DecryptingInputStreamBase);
NS_IMPL_RELEASE(DecryptingInputStreamBase);

NS_INTERFACE_MAP_BEGIN(DecryptingInputStreamBase)
  NS_INTERFACE_MAP_ENTRY(nsIInputStream)
  NS_INTERFACE_MAP_ENTRY(nsISeekableStream)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsICloneableInputStream,
                                     mBaseCloneableInputStream || !mBaseStream)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInputStream)
NS_INTERFACE_MAP_END

DecryptingInputStreamBase::DecryptingInputStreamBase(
    MovingNotNull<nsCOMPtr<nsIInputStream>> aBaseStream, size_t aBlockSize)
    : mBaseStream(std::move(aBaseStream)), mBlockSize(aBlockSize) {
  const nsCOMPtr<nsISeekableStream> seekableStream =
      do_QueryInterface(mBaseStream->get());
  MOZ_ASSERT(seekableStream &&
             SameCOMIdentity(mBaseStream->get(), seekableStream));
  mBaseSeekableStream.init(WrapNotNullUnchecked(seekableStream));

  const nsCOMPtr<nsICloneableInputStream> cloneableInputStream =
      do_QueryInterface(mBaseStream->get());
  if (cloneableInputStream &&
      SameCOMIdentity(mBaseStream->get(), cloneableInputStream)) {
    mBaseCloneableInputStream.init(WrapNotNullUnchecked(cloneableInputStream));
  }
}

NS_IMETHODIMP DecryptingInputStreamBase::Read(char* aBuf, uint32_t aCount,
                                              uint32_t* aBytesReadOut) {
  return ReadSegments(NS_CopySegmentToBuffer, aBuf, aCount, aBytesReadOut);
}

NS_IMETHODIMP DecryptingInputStreamBase::IsNonBlocking(bool* aNonBlockingOut) {
  *aNonBlockingOut = false;
  return NS_OK;
}

NS_IMETHODIMP DecryptingInputStreamBase::SetEOF() {
  // Cannot truncate a read-only stream.

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DecryptingInputStreamBase::GetCloneable(bool* aCloneable) {
  *aCloneable = true;
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
