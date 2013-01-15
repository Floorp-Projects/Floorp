/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "pratom.h"
#include "nsAlgorithm.h"
#include "nsIComponentManager.h"
#include "nsUCSupport.h"
#include "nsUnicodeDecodeHelper.h"
#include "nsUnicodeEncodeHelper.h"
#include <algorithm>

#define DEFAULT_BUFFER_CAPACITY 16

// XXX review the buffer growth limitation code

//----------------------------------------------------------------------
// Class nsBasicDecoderSupport [implementation]

nsBasicDecoderSupport::nsBasicDecoderSupport()
  : mErrBehavior(kOnError_Recover)
{
}

nsBasicDecoderSupport::~nsBasicDecoderSupport()
{
}

//----------------------------------------------------------------------
// Interface nsISupports [implementation]

#ifdef DEBUG
NS_IMPL_THREADSAFE_ISUPPORTS2(nsBasicDecoderSupport,
                              nsIUnicodeDecoder,
                              nsIBasicDecoder)
#else
NS_IMPL_THREADSAFE_ISUPPORTS1(nsBasicDecoderSupport, nsIUnicodeDecoder)
#endif

//----------------------------------------------------------------------
// Interface nsIUnicodeDecoder [implementation]

void
nsBasicDecoderSupport::SetInputErrorBehavior(int32_t aBehavior)
{
  NS_ABORT_IF_FALSE(aBehavior == kOnError_Recover || aBehavior == kOnError_Signal,
                    "Unknown behavior for SetInputErrorBehavior");
  mErrBehavior = aBehavior;
}

PRUnichar
nsBasicDecoderSupport::GetCharacterForUnMapped()
{
  return PRUnichar(0xfffd); // Unicode REPLACEMENT CHARACTER
}

//----------------------------------------------------------------------
// Class nsBufferDecoderSupport [implementation]

nsBufferDecoderSupport::nsBufferDecoderSupport(uint32_t aMaxLengthFactor)
  : nsBasicDecoderSupport(),
    mMaxLengthFactor(aMaxLengthFactor)
{
  mBufferCapacity = DEFAULT_BUFFER_CAPACITY;
  mBuffer = new char[mBufferCapacity];

  Reset();
}

nsBufferDecoderSupport::~nsBufferDecoderSupport()
{
  delete [] mBuffer;
}

void nsBufferDecoderSupport::FillBuffer(const char ** aSrc, int32_t aSrcLength)
{
  int32_t bcr = std::min(mBufferCapacity - mBufferLength, aSrcLength);
  memcpy(mBuffer + mBufferLength, *aSrc, bcr);
  mBufferLength += bcr;
  (*aSrc) += bcr;
}

//----------------------------------------------------------------------
// Subclassing of nsBasicDecoderSupport class [implementation]

NS_IMETHODIMP nsBufferDecoderSupport::Convert(const char * aSrc,
                                              int32_t * aSrcLength,
                                              PRUnichar * aDest,
                                              int32_t * aDestLength)
{
  // we do all operations using pointers internally
  const char * src = aSrc;
  const char * srcEnd = aSrc + *aSrcLength;
  PRUnichar * dest = aDest;
  PRUnichar * destEnd = aDest + *aDestLength;

  int32_t bcr, bcw; // byte counts for read & write;
  nsresult res = NS_OK;

  // do we have some residual data from the last conversion?
  if (mBufferLength > 0) {
    if (dest == destEnd) {
      res = NS_OK_UDEC_MOREOUTPUT;
    } else {
      for (;;) {
        // we need new data to add to the buffer
        if (src == srcEnd) {
          res = NS_OK_UDEC_MOREINPUT;
          break;
        }

        // fill that buffer
        int32_t buffLen = mBufferLength;  // initial buffer length
        FillBuffer(&src, srcEnd - src);

        // convert that buffer
        bcr = mBufferLength;
        bcw = destEnd - dest;
        res = ConvertNoBuff(mBuffer, &bcr, dest, &bcw);
        dest += bcw;

        // Detect invalid input character
        if (res == NS_ERROR_ILLEGAL_INPUT && mErrBehavior == kOnError_Signal) {
          break;
        }

        if ((res == NS_OK_UDEC_MOREINPUT) && (bcw == 0)) {
          res = NS_ERROR_UNEXPECTED;
#if defined(DEBUG_yokoyama) || defined(DEBUG_ftang)
          NS_ERROR("This should not happen. Internal buffer may be corrupted.");
#endif
          break;
        } else {
          if (bcr < buffLen) {
            // we didn't convert that residual data - unfill the buffer
            src -= mBufferLength - buffLen;
            mBufferLength = buffLen;
#if defined(DEBUG_yokoyama) || defined(DEBUG_ftang)
            NS_ERROR("This should not happen. Internal buffer may be corrupted.");
#endif
          } else {
            // the buffer and some extra data was converted - unget the rest
            src -= mBufferLength - bcr;
            mBufferLength = 0;
            res = NS_OK;
          }
          break;
        }
      }
    }
  }

  if (res == NS_OK) {
    bcr = srcEnd - src;
    bcw = destEnd - dest;
    res = ConvertNoBuff(src, &bcr, dest, &bcw);
    src += bcr;
    dest += bcw;

    // if we have partial input, store it in our internal buffer.
    if (res == NS_OK_UDEC_MOREINPUT) {
      bcr = srcEnd - src;
      // make sure buffer is large enough
      if (bcr > mBufferCapacity) {
          // somehow we got into an error state and the buffer is growing out
          // of control
          res = NS_ERROR_UNEXPECTED;
      } else {
          FillBuffer(&src, bcr);
      }
    }
  }

  *aSrcLength   -= srcEnd - src;
  *aDestLength  -= destEnd - dest;
  return res;
}

NS_IMETHODIMP nsBufferDecoderSupport::Reset()
{
  mBufferLength = 0;
  return NS_OK;
}

NS_IMETHODIMP nsBufferDecoderSupport::GetMaxLength(const char* aSrc,
                                                   int32_t aSrcLength,
                                                   int32_t* aDestLength)
{
  NS_ASSERTION(mMaxLengthFactor != 0, "Must override GetMaxLength!");
  *aDestLength = aSrcLength * mMaxLengthFactor;
  return NS_OK;
}

//----------------------------------------------------------------------
// Class nsTableDecoderSupport [implementation]

nsTableDecoderSupport::nsTableDecoderSupport(uScanClassID aScanClass,
                                             uShiftInTable * aShiftInTable,
                                             uMappingTable  * aMappingTable,
                                             uint32_t aMaxLengthFactor)
: nsBufferDecoderSupport(aMaxLengthFactor)
{
  mScanClass = aScanClass;
  mShiftInTable = aShiftInTable;
  mMappingTable = aMappingTable;
}

nsTableDecoderSupport::~nsTableDecoderSupport()
{
}

//----------------------------------------------------------------------
// Subclassing of nsBufferDecoderSupport class [implementation]

NS_IMETHODIMP nsTableDecoderSupport::ConvertNoBuff(const char * aSrc,
                                                   int32_t * aSrcLength,
                                                   PRUnichar * aDest,
                                                   int32_t * aDestLength)
{
  return nsUnicodeDecodeHelper::ConvertByTable(aSrc, aSrcLength,
                                               aDest, aDestLength,
                                               mScanClass,
                                               mShiftInTable, mMappingTable,
                                               mErrBehavior == kOnError_Signal);
}

//----------------------------------------------------------------------
// Class nsMultiTableDecoderSupport [implementation]

nsMultiTableDecoderSupport::nsMultiTableDecoderSupport(
                            int32_t aTableCount,
                            const uRange * aRangeArray,
                            uScanClassID * aScanClassArray,
                            uMappingTable ** aMappingTable,
                            uint32_t aMaxLengthFactor)
: nsBufferDecoderSupport(aMaxLengthFactor)
{
  mTableCount = aTableCount;
  mRangeArray = aRangeArray;
  mScanClassArray = aScanClassArray;
  mMappingTable = aMappingTable;
}

nsMultiTableDecoderSupport::~nsMultiTableDecoderSupport()
{
}

//----------------------------------------------------------------------
// Subclassing of nsBufferDecoderSupport class [implementation]

NS_IMETHODIMP nsMultiTableDecoderSupport::ConvertNoBuff(const char * aSrc,
                                                        int32_t * aSrcLength,
                                                        PRUnichar * aDest,
                                                        int32_t * aDestLength)
{
  return nsUnicodeDecodeHelper::ConvertByMultiTable(aSrc, aSrcLength,
                                                    aDest, aDestLength,
                                                    mTableCount, mRangeArray,
                                                    mScanClassArray,
                                                    mMappingTable,
                                                    mErrBehavior == kOnError_Signal);
}

//----------------------------------------------------------------------
// Class nsOneByteDecoderSupport [implementation]

nsOneByteDecoderSupport::nsOneByteDecoderSupport(
                         uMappingTable  * aMappingTable)
  : nsBasicDecoderSupport()
  , mMappingTable(aMappingTable)
  , mFastTableCreated(false)
  , mFastTableMutex("nsOneByteDecoderSupport mFastTableMutex")
{
}

nsOneByteDecoderSupport::~nsOneByteDecoderSupport()
{
}

//----------------------------------------------------------------------
// Subclassing of nsBasicDecoderSupport class [implementation]

NS_IMETHODIMP nsOneByteDecoderSupport::Convert(const char * aSrc,
                                              int32_t * aSrcLength,
                                              PRUnichar * aDest,
                                              int32_t * aDestLength)
{
  if (!mFastTableCreated) {
    // Probably better to make this non-lazy and get rid of the mutex
    mozilla::MutexAutoLock autoLock(mFastTableMutex);
    if (!mFastTableCreated) {
      nsresult res = nsUnicodeDecodeHelper::CreateFastTable(
                         mMappingTable, mFastTable, ONE_BYTE_TABLE_SIZE);
      if (NS_FAILED(res)) return res;
      mFastTableCreated = true;
    }
  }

  return nsUnicodeDecodeHelper::ConvertByFastTable(aSrc, aSrcLength,
                                                   aDest, aDestLength,
                                                   mFastTable,
                                                   ONE_BYTE_TABLE_SIZE,
                                                   mErrBehavior == kOnError_Signal);
}

NS_IMETHODIMP nsOneByteDecoderSupport::GetMaxLength(const char * aSrc,
                                                    int32_t aSrcLength,
                                                    int32_t * aDestLength)
{
  // single byte to Unicode converter
  *aDestLength = aSrcLength;
  return NS_OK_UDEC_EXACTLENGTH;
}

NS_IMETHODIMP nsOneByteDecoderSupport::Reset()
{
  // nothing to reset, no internal state in this case
  return NS_OK;
}

//----------------------------------------------------------------------
// Class nsBasicEncoder [implementation]
nsBasicEncoder::nsBasicEncoder()
{
}

nsBasicEncoder::~nsBasicEncoder()
{
}

//----------------------------------------------------------------------
// Interface nsISupports [implementation]

NS_IMPL_ADDREF(nsBasicEncoder)
NS_IMPL_RELEASE(nsBasicEncoder)
#ifdef DEBUG
NS_IMPL_QUERY_INTERFACE2(nsBasicEncoder,
                         nsIUnicodeEncoder,
                         nsIBasicEncoder)
#else
NS_IMPL_QUERY_INTERFACE1(nsBasicEncoder,
                         nsIUnicodeEncoder)
#endif
//----------------------------------------------------------------------
// Class nsEncoderSupport [implementation]

nsEncoderSupport::nsEncoderSupport(uint32_t aMaxLengthFactor) :
  mMaxLengthFactor(aMaxLengthFactor)
{
  mBufferCapacity = DEFAULT_BUFFER_CAPACITY;
  mBuffer = new char[mBufferCapacity];

  mErrBehavior = kOnError_Signal;
  mErrChar = 0;

  Reset();
}

nsEncoderSupport::~nsEncoderSupport()
{
  delete [] mBuffer;
}

NS_IMETHODIMP nsEncoderSupport::ConvertNoBuff(const PRUnichar * aSrc,
                                              int32_t * aSrcLength,
                                              char * aDest,
                                              int32_t * aDestLength)
{
  // we do all operations using pointers internally
  const PRUnichar * src = aSrc;
  const PRUnichar * srcEnd = aSrc + *aSrcLength;
  char * dest = aDest;
  char * destEnd = aDest + *aDestLength;

  int32_t bcr, bcw; // byte counts for read & write;
  nsresult res;

  for (;;) {
    bcr = srcEnd - src;
    bcw = destEnd - dest;
    res = ConvertNoBuffNoErr(src, &bcr, dest, &bcw);
    src += bcr;
    dest += bcw;

    if (res == NS_ERROR_UENC_NOMAPPING) {
      if (mErrBehavior == kOnError_Replace) {
        const PRUnichar buff[] = {mErrChar};
        bcr = 1;
        bcw = destEnd - dest;
        src--; // back the input: maybe the guy won't consume consume anything.
        res = ConvertNoBuffNoErr(buff, &bcr, dest, &bcw);
        src += bcr;
        dest += bcw;
        if (res != NS_OK) break;
      } else if (mErrBehavior == kOnError_CallBack) {
        bcw = destEnd - dest;
        src--;
        res = mErrEncoder->Convert(*src, dest, &bcw);
        dest += bcw;
        // if enought output space then the last char was used
        if (res != NS_OK_UENC_MOREOUTPUT) src++;
        if (res != NS_OK) break;
      } else break;
    }
    else break;
  }

  *aSrcLength   -= srcEnd - src;
  *aDestLength  -= destEnd - dest;
  return res;
}

NS_IMETHODIMP nsEncoderSupport::FinishNoBuff(char * aDest,
                                             int32_t * aDestLength)
{
  *aDestLength = 0;
  return NS_OK;
}

nsresult nsEncoderSupport::FlushBuffer(char ** aDest, const char * aDestEnd)
{
  int32_t bcr, bcw; // byte counts for read & write;
  nsresult res = NS_OK;
  char * dest = *aDest;

  if (mBufferStart < mBufferEnd) {
    bcr = mBufferEnd - mBufferStart;
    bcw = aDestEnd - dest;
    if (bcw < bcr) bcr = bcw;
    memcpy(dest, mBufferStart, bcr);
    dest += bcr;
    mBufferStart += bcr;

    if (mBufferStart < mBufferEnd) res = NS_OK_UENC_MOREOUTPUT;
  }

  *aDest = dest;
  return res;
}


//----------------------------------------------------------------------
// Interface nsIUnicodeEncoder [implementation]

NS_IMETHODIMP nsEncoderSupport::Convert(const PRUnichar * aSrc,
                                        int32_t * aSrcLength,
                                        char * aDest,
                                        int32_t * aDestLength)
{
  // we do all operations using pointers internally
  const PRUnichar * src = aSrc;
  const PRUnichar * srcEnd = aSrc + *aSrcLength;
  char * dest = aDest;
  char * destEnd = aDest + *aDestLength;

  int32_t bcr, bcw; // byte counts for read & write;
  nsresult res;

  res = FlushBuffer(&dest, destEnd);
  if (res == NS_OK_UENC_MOREOUTPUT) goto final;

  bcr = srcEnd - src;
  bcw = destEnd - dest;
  res = ConvertNoBuff(src, &bcr, dest, &bcw);
  src += bcr;
  dest += bcw;
  if ((res == NS_OK_UENC_MOREOUTPUT) && (dest < destEnd)) {
    // convert exactly one character into the internal buffer
    // at this point, there should be at least a char in the input
    for (;;) {
      bcr = 1;
      bcw = mBufferCapacity;
      res = ConvertNoBuff(src, &bcr, mBuffer, &bcw);

      if (res == NS_OK_UENC_MOREOUTPUT) {
        delete [] mBuffer;
        mBufferCapacity *= 2;
        mBuffer = new char [mBufferCapacity];
      } else {
        src += bcr;
        mBufferStart = mBufferEnd = mBuffer;
        mBufferEnd += bcw;
        break;
      }
    }

    res = FlushBuffer(&dest, destEnd);
  }

final:
  *aSrcLength   -= srcEnd - src;
  *aDestLength  -= destEnd - dest;
  return res;
}

NS_IMETHODIMP nsEncoderSupport::Finish(char * aDest, int32_t * aDestLength)
{
  // we do all operations using pointers internally
  char * dest = aDest;
  char * destEnd = aDest + *aDestLength;

  int32_t bcw; // byte count for write;
  nsresult res;

  res = FlushBuffer(&dest, destEnd);
  if (res == NS_OK_UENC_MOREOUTPUT) goto final;

  // do the finish into the internal buffer.
  for (;;) {
    bcw = mBufferCapacity;
    res = FinishNoBuff(mBuffer, &bcw);

    if (res == NS_OK_UENC_MOREOUTPUT) {
      delete [] mBuffer;
      mBufferCapacity *= 2;
      mBuffer = new char [mBufferCapacity];
    } else {
      mBufferStart = mBufferEnd = mBuffer;
      mBufferEnd += bcw;
      break;
    }
  }

  res = FlushBuffer(&dest, destEnd);

final:
  *aDestLength  -= destEnd - dest;
  return res;
}

NS_IMETHODIMP nsEncoderSupport::Reset()
{
  mBufferStart = mBufferEnd = mBuffer;
  return NS_OK;
}

NS_IMETHODIMP nsEncoderSupport::SetOutputErrorBehavior(
                                int32_t aBehavior,
                                nsIUnicharEncoder * aEncoder,
                                PRUnichar aChar)
{
  if (aBehavior == kOnError_CallBack && !aEncoder)
    return NS_ERROR_NULL_POINTER;

  mErrEncoder = aEncoder;
  mErrBehavior = aBehavior;
  mErrChar = aChar;
  return NS_OK;
}

NS_IMETHODIMP
nsEncoderSupport::GetMaxLength(const PRUnichar * aSrc,
                               int32_t aSrcLength,
                               int32_t * aDestLength)
{
  *aDestLength = aSrcLength * mMaxLengthFactor;
  return NS_OK;
}


//----------------------------------------------------------------------
// Class nsTableEncoderSupport [implementation]

nsTableEncoderSupport::nsTableEncoderSupport(uScanClassID aScanClass,
                                             uShiftOutTable * aShiftOutTable,
                                             uMappingTable  * aMappingTable,
                                             uint32_t aMaxLengthFactor)
: nsEncoderSupport(aMaxLengthFactor)
{
  mScanClass = aScanClass;
  mShiftOutTable = aShiftOutTable,
  mMappingTable = aMappingTable;
}

nsTableEncoderSupport::nsTableEncoderSupport(uScanClassID aScanClass,
                                             uMappingTable  * aMappingTable,
                                             uint32_t aMaxLengthFactor)
: nsEncoderSupport(aMaxLengthFactor)
{
  mScanClass = aScanClass;
  mShiftOutTable = nullptr;
  mMappingTable = aMappingTable;
}

nsTableEncoderSupport::~nsTableEncoderSupport()
{
}

//----------------------------------------------------------------------
// Subclassing of nsEncoderSupport class [implementation]

NS_IMETHODIMP nsTableEncoderSupport::ConvertNoBuffNoErr(
                                     const PRUnichar * aSrc,
                                     int32_t * aSrcLength,
                                     char * aDest,
                                     int32_t * aDestLength)
{
  return nsUnicodeEncodeHelper::ConvertByTable(aSrc, aSrcLength,
                                               aDest, aDestLength,
                                               mScanClass,
                                               mShiftOutTable, mMappingTable);
}

//----------------------------------------------------------------------
// Class nsMultiTableEncoderSupport [implementation]

nsMultiTableEncoderSupport::nsMultiTableEncoderSupport(
                            int32_t aTableCount,
                            uScanClassID * aScanClassArray,
                            uShiftOutTable ** aShiftOutTable,
                            uMappingTable  ** aMappingTable,
                            uint32_t aMaxLengthFactor)
: nsEncoderSupport(aMaxLengthFactor)
{
  mTableCount = aTableCount;
  mScanClassArray = aScanClassArray;
  mShiftOutTable = aShiftOutTable;
  mMappingTable = aMappingTable;
}

nsMultiTableEncoderSupport::~nsMultiTableEncoderSupport()
{
}

//----------------------------------------------------------------------
// Subclassing of nsEncoderSupport class [implementation]

NS_IMETHODIMP nsMultiTableEncoderSupport::ConvertNoBuffNoErr(
                                          const PRUnichar * aSrc,
                                          int32_t * aSrcLength,
                                          char * aDest,
                                          int32_t * aDestLength)
{
  return nsUnicodeEncodeHelper::ConvertByMultiTable(aSrc, aSrcLength,
                                                    aDest, aDestLength,
                                                    mTableCount,
                                                    mScanClassArray,
                                                    mShiftOutTable,
                                                    mMappingTable);
}
