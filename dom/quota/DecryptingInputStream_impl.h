/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_DecryptingInputStream_impl_h
#define mozilla_dom_quota_DecryptingInputStream_impl_h

#include "DecryptingInputStream.h"

#include "CipherStrategy.h"

#include "nsIAsyncInputStream.h"
#include "nsStreamUtils.h"

namespace mozilla::dom::quota {

template <typename CipherStrategy>
DecryptingInputStream<CipherStrategy>::DecryptingInputStream(
    MovingNotNull<nsCOMPtr<nsIInputStream>> aBaseStream, size_t aBlockSize,
    CipherStrategy aCipherStrategy, typename CipherStrategy::KeyType aKey)
    : DecryptingInputStreamBase(std::move(aBaseStream), aBlockSize),
      mCipherStrategy(std::move(aCipherStrategy)),
      mKey(aKey) {
  // This implementation only supports sync base streams.  Verify this in debug
  // builds.
#ifdef DEBUG
  bool baseNonBlocking;
  nsresult rv = (*mBaseStream)->IsNonBlocking(&baseNonBlocking);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  MOZ_ASSERT(!baseNonBlocking);
#endif
}

template <typename CipherStrategy>
DecryptingInputStream<CipherStrategy>::~DecryptingInputStream() {
  Close();
}

template <typename CipherStrategy>
NS_IMETHODIMP DecryptingInputStream<CipherStrategy>::Close() {
  if (!mBaseStream) {
    return NS_OK;
  }

  (*mBaseStream)->Close();
  mBaseStream.destroy();

  mPlainBuffer.Clear();
  mEncryptedBlock.reset();

  return NS_OK;
}

template <typename CipherStrategy>
NS_IMETHODIMP DecryptingInputStream<CipherStrategy>::Available(
    uint64_t* aLengthOut) {
  if (!mBaseStream) {
    return NS_BASE_STREAM_CLOSED;
  }

  // If we have plain bytes, then we are done.
  *aLengthOut = PlainLength();
  if (*aLengthOut > 0) {
    return NS_OK;
  }

  // Otherwise, attempt to decrypt bytes until we get something or the
  // underlying stream is drained.  We loop here because some chunks can
  // be StreamIdentifiers, padding, etc with no data.
  uint32_t bytesRead;
  do {
    nsresult rv = ParseNextChunk(&bytesRead);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    *aLengthOut = PlainLength();
  } while (*aLengthOut == 0 && bytesRead);

  return NS_OK;
}

template <typename CipherStrategy>
NS_IMETHODIMP DecryptingInputStream<CipherStrategy>::ReadSegments(
    nsWriteSegmentFun aWriter, void* aClosure, uint32_t aCount,
    uint32_t* aBytesReadOut) {
  *aBytesReadOut = 0;

  if (!mBaseStream) {
    return NS_BASE_STREAM_CLOSED;
  }

  nsresult rv;

  // Do not try to use the base stream's ReadSegments here.  Its very
  // unlikely we will get a single buffer that contains all of the encrypted
  // data and therefore would have to copy into our own buffer anyways.
  // Instead, focus on making efficient use of the Read() interface.

  while (aCount > 0) {
    // We have some decrypted data in our buffer.  Provide it to the callers
    // writer function.
    if (mPlainBytes > 0) {
      MOZ_ASSERT(!mPlainBuffer.IsEmpty());
      uint32_t remaining = PlainLength();
      uint32_t numToWrite = std::min(aCount, remaining);
      uint32_t numWritten;
      rv = aWriter(this, aClosure,
                   reinterpret_cast<const char*>(&mPlainBuffer[mNextByte]),
                   *aBytesReadOut, numToWrite, &numWritten);

      // As defined in nsIInputputStream.idl, do not pass writer func errors.
      if (NS_FAILED(rv)) {
        return NS_OK;
      }

      // End-of-file
      if (numWritten == 0) {
        return NS_OK;
      }

      *aBytesReadOut += numWritten;
      mNextByte += numWritten;
      MOZ_ASSERT(mNextByte <= mPlainBytes);

      if (mNextByte == mPlainBytes) {
        mNextByte = 0;
        mPlainBytes = 0;
      }

      aCount -= numWritten;

      continue;
    }

    // Otherwise decrypt the next chunk and loop.  Any resulting data
    // will set mPlainBytes which we check at the top of the loop.
    uint32_t bytesRead;
    rv = ParseNextChunk(&bytesRead);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // If we couldn't read anything and there is no more data to provide
    // to the caller, then this is eof.
    if (bytesRead == 0 && mPlainBytes == 0) {
      return NS_OK;
    }

    mPlainBytes += bytesRead;
  }

  return NS_OK;
}

template <typename CipherStrategy>
nsresult DecryptingInputStream<CipherStrategy>::ParseNextChunk(
    uint32_t* const aBytesReadOut) {
  // There must not be any plain data already in mPlainBuffer.
  MOZ_ASSERT(mPlainBytes == 0);
  MOZ_ASSERT(mNextByte == 0);

  *aBytesReadOut = 0;

  if (!EnsureBuffers()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Read the data to our internal encrypted buffer.
  auto wholeBlock = mEncryptedBlock->MutableWholeBlock();
  nsresult rv =
      ReadAll(AsWritableChars(wholeBlock).Elements(), wholeBlock.Length(),
              wholeBlock.Length(), aBytesReadOut);
  if (NS_WARN_IF(NS_FAILED(rv)) || *aBytesReadOut == 0) {
    return rv;
  }

  // XXX Do we need to know the actual decrypted size?
  rv = mCipherStrategy.Cipher(
      CipherMode::Decrypt, mKey, mEncryptedBlock->MutableCipherPrefix(),
      mEncryptedBlock->Payload(), AsWritableBytes(Span{mPlainBuffer}));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  *aBytesReadOut = mEncryptedBlock->ActualPayloadLength();

  return NS_OK;
}

template <typename CipherStrategy>
nsresult DecryptingInputStream<CipherStrategy>::ReadAll(
    char* aBuf, uint32_t aCount, uint32_t aMinValidCount,
    uint32_t* aBytesReadOut) {
  MOZ_ASSERT(aCount >= aMinValidCount);
  MOZ_ASSERT(mBaseStream);

  *aBytesReadOut = 0;

  uint32_t offset = 0;
  while (aCount > 0) {
    uint32_t bytesRead = 0;
    nsresult rv = (*mBaseStream)->Read(aBuf + offset, aCount, &bytesRead);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    // EOF, but don't immediately return.  We need to validate min read bytes
    // below.
    if (bytesRead == 0) {
      break;
    }

    *aBytesReadOut += bytesRead;
    offset += bytesRead;
    aCount -= bytesRead;
  }

  // Reading zero bytes is not an error.  Its the expected EOF condition.
  // Only compare to the minimum valid count if we read at least one byte.
  if (*aBytesReadOut != 0 && *aBytesReadOut < aMinValidCount) {
    return NS_ERROR_CORRUPTED_CONTENT;
  }

  return NS_OK;
}

template <typename CipherStrategy>
bool DecryptingInputStream<CipherStrategy>::EnsureBuffers() {
  // Lazily create our two buffers so we can report OOM during stream
  // operation.  These allocations only happens once.  The buffers are reused
  // until the stream is closed.
  if (!mEncryptedBlock) {
    // XXX Do we need to do this fallible (as the comment above suggests)?
    mEncryptedBlock.emplace(mBlockSize);

    MOZ_ASSERT(mPlainBuffer.IsEmpty());
    if (NS_WARN_IF(!mPlainBuffer.SetLength(mEncryptedBlock->MaxPayloadLength(),
                                           fallible))) {
      return false;
    }
  }

  return true;
}

}  // namespace mozilla::dom::quota

#endif
