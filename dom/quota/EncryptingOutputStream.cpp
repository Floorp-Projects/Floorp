/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EncryptingOutputStream.h"
#include "EncryptingOutputStream_impl.h"

#include <type_traits>
#include "mozilla/MacroForEach.h"
#include "nsStreamUtils.h"

namespace mozilla::dom::quota {

NS_IMPL_ISUPPORTS(EncryptingOutputStreamBase, nsIOutputStream);

EncryptingOutputStreamBase::EncryptingOutputStreamBase(
    nsCOMPtr<nsIOutputStream> aBaseStream, size_t aBlockSize)
    : mBaseStream(WrapNotNull(std::move(aBaseStream))),
      mBlockSize(aBlockSize) {}

NS_IMETHODIMP EncryptingOutputStreamBase::Write(const char* aBuf,
                                                uint32_t aCount,
                                                uint32_t* aResultOut) {
  return WriteSegments(NS_CopyBufferToSegment, const_cast<char*>(aBuf), aCount,
                       aResultOut);
}

NS_IMETHODIMP EncryptingOutputStreamBase::WriteFrom(nsIInputStream*, uint32_t,
                                                    uint32_t*) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP EncryptingOutputStreamBase::IsNonBlocking(bool* aNonBlockingOut) {
  *aNonBlockingOut = false;
  return NS_OK;
}

nsresult EncryptingOutputStreamBase::WriteAll(const char* aBuf, uint32_t aCount,
                                              uint32_t* aBytesWrittenOut) {
  *aBytesWrittenOut = 0;

  if (!mBaseStream) {
    return NS_BASE_STREAM_CLOSED;
  }

  uint32_t offset = 0;
  while (aCount > 0) {
    uint32_t numWritten = 0;
    nsresult rv = (*mBaseStream)->Write(aBuf + offset, aCount, &numWritten);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    offset += numWritten;
    aCount -= numWritten;
    *aBytesWrittenOut += numWritten;
  }

  return NS_OK;
}

}  // namespace mozilla::dom::quota
