/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_EncryptingOutputStream_impl_h
#define mozilla_dom_quota_EncryptingOutputStream_impl_h

#include "EncryptingOutputStream.h"

#include <algorithm>
#include <utility>
#include "CipherStrategy.h"
#include "mozilla/Assertions.h"
#include "mozilla/Span.h"
#include "mozilla/fallible.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIAsyncOutputStream.h"
#include "nsIRandomGenerator.h"
#include "nsServiceManagerUtils.h"

namespace mozilla::dom::quota {
template <typename CipherStrategy>
EncryptingOutputStream<CipherStrategy>::EncryptingOutputStream(
    nsCOMPtr<nsIOutputStream> aBaseStream, size_t aBlockSize,
    typename CipherStrategy::KeyType aKey)
    : EncryptingOutputStreamBase(std::move(aBaseStream), aBlockSize) {
  // XXX Move this to a fallible init function.
  MOZ_ALWAYS_SUCCEEDS(mCipherStrategy.Init(CipherMode::Encrypt,
                                           CipherStrategy::SerializeKey(aKey),
                                           CipherStrategy::MakeBlockPrefix()));

  MOZ_ASSERT(mBlockSize > 0);
  MOZ_ASSERT(mBlockSize % CipherStrategy::BasicBlockSize == 0);
  static_assert(
      CipherStrategy::BlockPrefixLength % CipherStrategy::BasicBlockSize == 0);

  // This implementation only supports sync base streams.  Verify this in debug
  // builds.  Note, this is a bit complicated because the streams we support
  // advertise different capabilities:
  //  - nsFileOutputStream - blocking and sync
  //  - FixedBufferOutputStream - non-blocking and sync
  //  - nsPipeOutputStream - can be blocking, but provides async interface
#ifdef DEBUG
  bool baseNonBlocking;
  nsresult rv = (*mBaseStream)->IsNonBlocking(&baseNonBlocking);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  if (baseNonBlocking) {
    nsCOMPtr<nsIAsyncOutputStream> async =
        do_QueryInterface((*mBaseStream).get());
    MOZ_ASSERT(!async);
  }
#endif
}

template <typename CipherStrategy>
EncryptingOutputStream<CipherStrategy>::~EncryptingOutputStream() {
  Close();
}

template <typename CipherStrategy>
NS_IMETHODIMP EncryptingOutputStream<CipherStrategy>::Close() {
  if (!mBaseStream) {
    return NS_OK;
  }

  // When closing, flush to the base stream unconditionally, i.e. even if the
  // buffer is not completely full.
  nsresult rv = FlushToBaseStream();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // XXX Maybe this Flush call can be removed, since the base stream is closed
  // afterwards anyway.
  rv = (*mBaseStream)->Flush();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // XXX What if closing the base stream failed? Fail this method, or at least
  // log a warning?
  (*mBaseStream)->Close();
  mBaseStream.destroy();

  mBuffer.Clear();
  mEncryptedBlock.reset();

  return NS_OK;
}

template <typename CipherStrategy>
NS_IMETHODIMP EncryptingOutputStream<CipherStrategy>::Flush() {
  if (!mBaseStream) {
    return NS_BASE_STREAM_CLOSED;
  }

  if (!EnsureBuffers()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // We cannot call FlushBaseStream() here if the buffer is not completely
  // full, we would write an incomplete page, which might be read sequentially,
  // but we want to support random accesses in DecryptingInputStream, which
  // would no longer be feasible.
  if (mNextByte && mNextByte == mEncryptedBlock->MaxPayloadLength()) {
    nsresult rv = FlushToBaseStream();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return (*mBaseStream)->Flush();
}

template <typename CipherStrategy>
NS_IMETHODIMP EncryptingOutputStream<CipherStrategy>::StreamStatus() {
  if (!mBaseStream) {
    return NS_BASE_STREAM_CLOSED;
  }
  return (*mBaseStream)->StreamStatus();
}

template <typename CipherStrategy>
NS_IMETHODIMP EncryptingOutputStream<CipherStrategy>::WriteSegments(
    nsReadSegmentFun aReader, void* aClosure, uint32_t aCount,
    uint32_t* aBytesWrittenOut) {
  *aBytesWrittenOut = 0;

  if (!mBaseStream) {
    return NS_BASE_STREAM_CLOSED;
  }

  if (!EnsureBuffers()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  const size_t plainBufferSize = mEncryptedBlock->MaxPayloadLength();

  while (aCount > 0) {
    // Determine how much space is left in our flat, plain buffer.
    MOZ_ASSERT(mNextByte <= plainBufferSize);
    uint32_t remaining = plainBufferSize - mNextByte;

    // If it is full, then encrypt and flush the data to the base stream.
    if (remaining == 0) {
      nsresult rv = FlushToBaseStream();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      // Now the entire buffer should be available for copying.
      MOZ_ASSERT(!mNextByte);
      remaining = plainBufferSize;
    }

    uint32_t numToRead = std::min(remaining, aCount);
    uint32_t numRead = 0;

    nsresult rv =
        aReader(this, aClosure, reinterpret_cast<char*>(&mBuffer[mNextByte]),
                *aBytesWrittenOut, numToRead, &numRead);

    // As defined in nsIOutputStream.idl, do not pass reader func errors.
    if (NS_FAILED(rv)) {
      return NS_OK;
    }

    // End-of-file
    if (numRead == 0) {
      return NS_OK;
    }

    mNextByte += numRead;
    *aBytesWrittenOut += numRead;
    aCount -= numRead;
  }

  return NS_OK;
}

template <typename CipherStrategy>
bool EncryptingOutputStream<CipherStrategy>::EnsureBuffers() {
  // Lazily create the encrypted buffer on our first flush.  This
  // allows us to report OOM during stream operation.  This buffer
  // will then get re-used until the stream is closed.
  if (!mEncryptedBlock) {
    // XXX Do we need to do this fallible (as the comment above suggests)?
    mEncryptedBlock.emplace(mBlockSize);
    MOZ_ASSERT(mBuffer.IsEmpty());

    if (NS_WARN_IF(!mBuffer.SetLength(mEncryptedBlock->MaxPayloadLength(),
                                      fallible))) {
      return false;
    }
  }

  return true;
}

template <typename CipherStrategy>
nsresult EncryptingOutputStream<CipherStrategy>::FlushToBaseStream() {
  MOZ_ASSERT(mBaseStream);

  if (!mNextByte) {
    // Nothing to do.
    return NS_OK;
  }

  if (mNextByte < mEncryptedBlock->MaxPayloadLength()) {
    if (!mRandomGenerator) {
      mRandomGenerator =
          do_GetService("@mozilla.org/security/random-generator;1");
      if (NS_WARN_IF(!mRandomGenerator)) {
        return NS_ERROR_FAILURE;
      }
    }

    const auto payload = mEncryptedBlock->MutablePayload();

    const auto unusedPayload = payload.From(mNextByte);

    nsresult rv = mRandomGenerator->GenerateRandomBytesInto(
        unusedPayload.Elements(), unusedPayload.Length());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // XXX The compressing stream implementation this was based on wrote a stream
  // identifier, containing e.g. the block size. Should we do something like
  // that as well? At the moment, we don't need it, but maybe this were
  // convenient if we use this for persistent files in the future across version
  // updates, which might change such parameters.

  const auto iv = mCipherStrategy.MakeBlockPrefix();
  static_assert(iv.size() * sizeof(decltype(*iv.begin())) ==
                CipherStrategy::BlockPrefixLength);
  std::copy(iv.cbegin(), iv.cend(),
            mEncryptedBlock->MutableCipherPrefix().begin());

  // Encrypt the data to our internal encrypted buffer.
  // XXX Do we need to know the actual encrypted size?
  nsresult rv = mCipherStrategy.Cipher(
      mEncryptedBlock->MutableCipherPrefix(),
      mozilla::Span(reinterpret_cast<uint8_t*>(mBuffer.Elements()),
                    ((mNextByte + (CipherStrategy::BasicBlockSize - 1)) /
                     CipherStrategy::BasicBlockSize) *
                        CipherStrategy::BasicBlockSize),
      mEncryptedBlock->MutablePayload());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mEncryptedBlock->SetActualPayloadLength(mNextByte);

  mNextByte = 0;

  // Write the encrypted buffer out to the base stream.
  uint32_t numWritten = 0;
  const auto& wholeBlock = mEncryptedBlock->WholeBlock();
  rv = WriteAll(AsChars(wholeBlock).Elements(), wholeBlock.Length(),
                &numWritten);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  MOZ_ASSERT(wholeBlock.Length() == numWritten);

  return NS_OK;
}

}  // namespace mozilla::dom::quota

#endif
