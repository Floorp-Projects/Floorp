/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsConverterInputStream.h"
#include "nsIInputStream.h"
#include "nsReadLine.h"
#include "nsStreamUtils.h"

#include <algorithm>
#include <tuple>

using namespace mozilla;

#define CONVERTER_BUFFER_SIZE 8192

NS_IMPL_ISUPPORTS(nsConverterInputStream, nsIConverterInputStream,
                  nsIUnicharInputStream, nsIUnicharLineInputStream)

NS_IMETHODIMP
nsConverterInputStream::Init(nsIInputStream* aStream, const char* aCharset,
                             int32_t aBufferSize, char16_t aReplacementChar) {
  nsAutoCString label;
  if (!aCharset) {
    label.AssignLiteral("UTF-8");
  } else {
    label = aCharset;
  }

  auto encoding = Encoding::ForLabelNoReplacement(label);
  if (!encoding) {
    return NS_ERROR_UCONV_NOCONV;
  }
  // Previously, the implementation auto-switched only
  // between the two UTF-16 variants and only when
  // initialized with an endianness-unspecific label.
  mConverter = encoding->NewDecoder();

  size_t outputBufferSize;
  if (aBufferSize <= 0) {
    aBufferSize = CONVERTER_BUFFER_SIZE;
    outputBufferSize = CONVERTER_BUFFER_SIZE;
  } else {
    // NetUtil.jsm assumes that if buffer size equals
    // the input size, the whole stream will be processed
    // as one readString. This is not true with encoding_rs,
    // because encoding_rs might want to see space for a
    // surrogate pair, so let's compute a larger output
    // buffer length.
    CheckedInt<size_t> needed = mConverter->MaxUTF16BufferLength(aBufferSize);
    if (!needed.isValid()) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    outputBufferSize = needed.value();
  }

  // set up our buffers.
  if (!mByteData.SetCapacity(aBufferSize, mozilla::fallible) ||
      !mUnicharData.SetLength(outputBufferSize, mozilla::fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mInput = aStream;
  mErrorsAreFatal = !aReplacementChar;
  return NS_OK;
}

NS_IMETHODIMP
nsConverterInputStream::Close() {
  nsresult rv = mInput ? mInput->Close() : NS_OK;
  mLineBuffer = nullptr;
  mInput = nullptr;
  mConverter = nullptr;
  mByteData.Clear();
  mUnicharData.Clear();
  return rv;
}

NS_IMETHODIMP
nsConverterInputStream::Read(char16_t* aBuf, uint32_t aCount,
                             uint32_t* aReadCount) {
  NS_ASSERTION(mUnicharDataLength >= mUnicharDataOffset, "unsigned madness");
  uint32_t readCount = mUnicharDataLength - mUnicharDataOffset;
  if (0 == readCount) {
    // Fill the unichar buffer
    readCount = Fill(&mLastErrorCode);
    if (readCount == 0) {
      *aReadCount = 0;
      return mLastErrorCode;
    }
  }
  if (readCount > aCount) {
    readCount = aCount;
  }
  memcpy(aBuf, mUnicharData.Elements() + mUnicharDataOffset,
         readCount * sizeof(char16_t));
  mUnicharDataOffset += readCount;
  *aReadCount = readCount;
  return NS_OK;
}

NS_IMETHODIMP
nsConverterInputStream::ReadSegments(nsWriteUnicharSegmentFun aWriter,
                                     void* aClosure, uint32_t aCount,
                                     uint32_t* aReadCount) {
  NS_ASSERTION(mUnicharDataLength >= mUnicharDataOffset, "unsigned madness");
  uint32_t codeUnitsToWrite = mUnicharDataLength - mUnicharDataOffset;
  if (0 == codeUnitsToWrite) {
    // Fill the unichar buffer
    codeUnitsToWrite = Fill(&mLastErrorCode);
    if (codeUnitsToWrite == 0) {
      *aReadCount = 0;
      return mLastErrorCode;
    }
  }

  if (codeUnitsToWrite > aCount) {
    codeUnitsToWrite = aCount;
  }

  uint32_t codeUnitsWritten;
  uint32_t totalCodeUnitsWritten = 0;

  while (codeUnitsToWrite) {
    nsresult rv =
        aWriter(this, aClosure, mUnicharData.Elements() + mUnicharDataOffset,
                totalCodeUnitsWritten, codeUnitsToWrite, &codeUnitsWritten);
    if (NS_FAILED(rv)) {
      // don't propagate errors to the caller
      break;
    }

    codeUnitsToWrite -= codeUnitsWritten;
    totalCodeUnitsWritten += codeUnitsWritten;
    mUnicharDataOffset += codeUnitsWritten;
  }

  *aReadCount = totalCodeUnitsWritten;

  return NS_OK;
}

NS_IMETHODIMP
nsConverterInputStream::ReadString(uint32_t aCount, nsAString& aString,
                                   uint32_t* aReadCount) {
  NS_ASSERTION(mUnicharDataLength >= mUnicharDataOffset, "unsigned madness");
  uint32_t readCount = mUnicharDataLength - mUnicharDataOffset;
  if (0 == readCount) {
    // Fill the unichar buffer
    readCount = Fill(&mLastErrorCode);
    if (readCount == 0) {
      *aReadCount = 0;
      return mLastErrorCode;
    }
  }
  if (readCount > aCount) {
    readCount = aCount;
  }
  const char16_t* buf = mUnicharData.Elements() + mUnicharDataOffset;
  aString.Assign(buf, readCount);
  mUnicharDataOffset += readCount;
  *aReadCount = readCount;
  return NS_OK;
}

uint32_t nsConverterInputStream::Fill(nsresult* aErrorCode) {
  if (!mInput) {
    // We already closed the stream!
    *aErrorCode = NS_BASE_STREAM_CLOSED;
    return 0;
  }

  if (NS_FAILED(mLastErrorCode)) {
    // We failed to completely convert last time, and error-recovery
    // is disabled.  We will fare no better this time, so...
    *aErrorCode = mLastErrorCode;
    return 0;
  }

  // mUnicharData.Length() is the buffer length, not the fill status.
  // mUnicharDataLength reflects the current fill status.
  mUnicharDataLength = 0;
  // Whenever we convert, mUnicharData is logically empty.
  mUnicharDataOffset = 0;

  // Continue trying to read from the source stream until we successfully decode
  // a character or encounter an error, as returning `0` here implies that the
  // stream is complete.
  //
  // If the converter has been cleared, we've fully consumed the stream, and
  // want to report EOF.
  while (mUnicharDataLength == 0 && mConverter) {
    // We assume a many to one conversion and are using equal sizes for
    // the two buffers.  However if an error happens at the very start
    // of a byte buffer we may end up in a situation where n bytes lead
    // to n+1 unicode chars.  Thus we need to keep track of the leftover
    // bytes as we convert.

    uint32_t nb;
    *aErrorCode = NS_FillArray(mByteData, mInput, mLeftOverBytes, &nb);
    if (NS_FAILED(*aErrorCode)) {
      return 0;
    }

    NS_ASSERTION(uint32_t(nb) + mLeftOverBytes == mByteData.Length(),
                 "mByteData is lying to us somewhere");

    // If `NS_FillArray` failed to read any new bytes, this is the last read,
    // and we're at the end of the stream.
    bool last = (nb == 0);

    // Now convert as much of the byte buffer to unicode as possible
    auto src = AsBytes(Span(mByteData));
    auto dst = Span(mUnicharData);

    // Truncation from size_t to uint32_t below is OK, because the sizes
    // are bounded by the lengths of mByteData and mUnicharData.
    uint32_t result;
    size_t read;
    size_t written;
    if (mErrorsAreFatal) {
      std::tie(result, read, written) =
          mConverter->DecodeToUTF16WithoutReplacement(src, dst, last);
    } else {
      std::tie(result, read, written, std::ignore) =
          mConverter->DecodeToUTF16(src, dst, last);
    }
    mLeftOverBytes = mByteData.Length() - read;
    mUnicharDataLength = written;
    // Clear `mConverter` if we reached the end of the stream, as we can't
    // call methods on it anymore. This will also signal EOF to the caller
    // through the loop condition.
    if (last) {
      MOZ_ASSERT(mLeftOverBytes == 0,
                 "Failed to read all bytes on the last pass?");
      mConverter = nullptr;
    }
    // If we got a decode error, we're done.
    if (result != kInputEmpty && result != kOutputFull) {
      MOZ_ASSERT(mErrorsAreFatal, "How come DecodeToUTF16() reported error?");
      *aErrorCode = NS_ERROR_UDEC_ILLEGALINPUT;
      return 0;
    }
  }
  *aErrorCode = NS_OK;
  return mUnicharDataLength;
}

NS_IMETHODIMP
nsConverterInputStream::ReadLine(nsAString& aLine, bool* aResult) {
  if (!mLineBuffer) {
    mLineBuffer = MakeUnique<nsLineBuffer<char16_t>>();
  }
  return NS_ReadLine(this, mLineBuffer.get(), aLine, aResult);
}
