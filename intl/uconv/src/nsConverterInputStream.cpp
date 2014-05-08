/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsConverterInputStream.h"
#include "nsIInputStream.h"
#include "nsReadLine.h"
#include "nsStreamUtils.h"
#include <algorithm>
#include "mozilla/dom/EncodingUtils.h"

using mozilla::dom::EncodingUtils;

#define CONVERTER_BUFFER_SIZE 8192

NS_IMPL_ISUPPORTS(nsConverterInputStream, nsIConverterInputStream,
                  nsIUnicharInputStream, nsIUnicharLineInputStream)


NS_IMETHODIMP
nsConverterInputStream::Init(nsIInputStream* aStream,
                             const char *aCharset,
                             int32_t aBufferSize,
                             char16_t aReplacementChar)
{
    nsAutoCString label;
    if (!aCharset) {
        label.AssignLiteral("UTF-8");
    } else {
        label = aCharset;
    }

    if (aBufferSize <=0) aBufferSize=CONVERTER_BUFFER_SIZE;
    
    // get the decoder
    nsAutoCString encoding;
    if (label.EqualsLiteral("UTF-16")) {
        // Compat with old test cases. Unclear if any extensions really care.
        encoding.Assign(label);
    } else if (!EncodingUtils::FindEncodingForLabelNoReplacement(label,
                                                                 encoding)) {
      return NS_ERROR_UCONV_NOCONV;
    }
    mConverter = EncodingUtils::DecoderForEncoding(encoding);
 
    // set up our buffers
    if (!mByteData.SetCapacity(aBufferSize) ||
        !mUnicharData.SetCapacity(aBufferSize)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    mInput = aStream;
    mReplacementChar = aReplacementChar;
    if (!aReplacementChar ||
        aReplacementChar != mConverter->GetCharacterForUnMapped()) {
        mConverter->SetInputErrorBehavior(nsIUnicodeDecoder::kOnError_Signal);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsConverterInputStream::Close()
{
    nsresult rv = mInput ? mInput->Close() : NS_OK;
    mLineBuffer = nullptr;
    mInput = nullptr;
    mConverter = nullptr;
    mByteData.Clear();
    mUnicharData.Clear();
    return rv;
}

NS_IMETHODIMP
nsConverterInputStream::Read(char16_t* aBuf,
                             uint32_t aCount,
                             uint32_t *aReadCount)
{
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
                                     void* aClosure,
                                     uint32_t aCount, uint32_t *aReadCount)
{
  NS_ASSERTION(mUnicharDataLength >= mUnicharDataOffset, "unsigned madness");
  uint32_t bytesToWrite = mUnicharDataLength - mUnicharDataOffset;
  nsresult rv;
  if (0 == bytesToWrite) {
    // Fill the unichar buffer
    bytesToWrite = Fill(&rv);
    if (bytesToWrite <= 0) {
      *aReadCount = 0;
      return rv;
    }
  }
  
  if (bytesToWrite > aCount)
    bytesToWrite = aCount;
  
  uint32_t bytesWritten;
  uint32_t totalBytesWritten = 0;

  while (bytesToWrite) {
    rv = aWriter(this, aClosure,
                 mUnicharData.Elements() + mUnicharDataOffset,
                 totalBytesWritten, bytesToWrite, &bytesWritten);
    if (NS_FAILED(rv)) {
      // don't propagate errors to the caller
      break;
    }
    
    bytesToWrite -= bytesWritten;
    totalBytesWritten += bytesWritten;
    mUnicharDataOffset += bytesWritten;
    
  }

  *aReadCount = totalBytesWritten;

  return NS_OK;
}

NS_IMETHODIMP
nsConverterInputStream::ReadString(uint32_t aCount, nsAString& aString,
                                   uint32_t* aReadCount)
{
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

uint32_t
nsConverterInputStream::Fill(nsresult * aErrorCode)
{
  if (nullptr == mInput) {
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
  
  // We assume a many to one conversion and are using equal sizes for
  // the two buffers.  However if an error happens at the very start
  // of a byte buffer we may end up in a situation where n bytes lead
  // to n+1 unicode chars.  Thus we need to keep track of the leftover
  // bytes as we convert.
  
  uint32_t nb;
  *aErrorCode = NS_FillArray(mByteData, mInput, mLeftOverBytes, &nb);
  if (nb == 0 && mLeftOverBytes == 0) {
    // No more data 
    *aErrorCode = NS_OK;
    return 0;
  }

  NS_ASSERTION(uint32_t(nb) + mLeftOverBytes == mByteData.Length(),
               "mByteData is lying to us somewhere");

  // Now convert as much of the byte buffer to unicode as possible
  mUnicharDataOffset = 0;
  mUnicharDataLength = 0;
  uint32_t srcConsumed = 0;
  do {
    int32_t srcLen = mByteData.Length() - srcConsumed;
    int32_t dstLen = mUnicharData.Capacity() - mUnicharDataLength;
    *aErrorCode = mConverter->Convert(mByteData.Elements()+srcConsumed,
                                      &srcLen,
                                      mUnicharData.Elements()+mUnicharDataLength,
                                      &dstLen);
    mUnicharDataLength += dstLen;
    // XXX if srcLen is negative, we want to drop the _first_ byte in
    // the erroneous byte sequence and try again.  This is not quite
    // possible right now -- see bug 160784
    srcConsumed += srcLen;
    if (NS_FAILED(*aErrorCode) && mReplacementChar) {
      NS_ASSERTION(0 < mUnicharData.Capacity() - mUnicharDataLength,
                   "Decoder returned an error but filled the output buffer! "
                   "Should not happen.");
      mUnicharData.Elements()[mUnicharDataLength++] = mReplacementChar;
      ++srcConsumed;
      // XXX this is needed to make sure we don't underrun our buffer;
      // bug 160784 again
      srcConsumed = std::max<uint32_t>(srcConsumed, 0);
      mConverter->Reset();
    }
    NS_ASSERTION(srcConsumed <= mByteData.Length(),
                 "Whoa.  The converter should have returned NS_OK_UDEC_MOREINPUT before this point!");
  } while (mReplacementChar &&
           NS_FAILED(*aErrorCode) &&
           mUnicharData.Capacity() > mUnicharDataLength);

  mLeftOverBytes = mByteData.Length() - srcConsumed;

  return mUnicharDataLength;
}

NS_IMETHODIMP
nsConverterInputStream::ReadLine(nsAString& aLine, bool* aResult)
{
  if (!mLineBuffer) {
    mLineBuffer = new nsLineBuffer<char16_t>;
  }
  return NS_ReadLine(this, mLineBuffer.get(), aLine, aResult);
}
