/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUCSupport.h"
#include "nsUTF8ToUnicode.h"
#include "mozilla/SSE.h"
#include "nsCharTraits.h"
#include <algorithm>

#define UNICODE_BYTE_ORDER_MARK    0xFEFF

static PRUnichar* EmitSurrogatePair(uint32_t ucs4, PRUnichar* aDest)
{
  NS_ASSERTION(ucs4 > 0xFFFF, "Should be a supplementary character");
  ucs4 -= 0x00010000;
  *aDest++ = 0xD800 | (0x000003FF & (ucs4 >> 10));
  *aDest++ = 0xDC00 | (0x000003FF & ucs4);
  return aDest;
}

//----------------------------------------------------------------------
// Class nsUTF8ToUnicode [implementation]

nsUTF8ToUnicode::nsUTF8ToUnicode()
: nsBasicDecoderSupport()
{
  Reset();
}

//----------------------------------------------------------------------
// Subclassing of nsTableDecoderSupport class [implementation]

/**
 * Normally the maximum length of the output of the UTF8 decoder in UTF16
 *  code units is the same as the length of the input in UTF8 code units,
 *  since 1-byte, 2-byte and 3-byte UTF-8 sequences decode to a single
 *  UTF-16 character, and 4-byte UTF-8 sequences decode to a surrogate pair.
 *
 * However, there is an edge case where the output can be longer than the
 *  input: if the previous buffer ended with an incomplete multi-byte
 *  sequence and this buffer does not begin with a valid continuation
 *  byte, we will return NS_ERROR_ILLEGAL_INPUT and the caller may insert a
 *  replacement character in the output buffer which corresponds to no
 *  character in the input buffer. So in the worst case the destination
 *  will need to be one code unit longer than the source.
 *  See bug 301797.
 */
NS_IMETHODIMP nsUTF8ToUnicode::GetMaxLength(const char * aSrc,
                                            int32_t aSrcLength,
                                            int32_t * aDestLength)
{
  *aDestLength = aSrcLength + 1;
  return NS_OK;
}


//----------------------------------------------------------------------
// Subclassing of nsBasicDecoderSupport class [implementation]

NS_IMETHODIMP nsUTF8ToUnicode::Reset()
{

  mUcs4  = 0;     // cached Unicode character
  mState = 0;     // cached expected number of octets after the current octet
                  // until the beginning of the next UTF8 character sequence
  mBytes = 1;     // cached expected number of octets in the current sequence
  mFirst = true;

  return NS_OK;

}

//----------------------------------------------------------------------
// Subclassing of nsBasicDecoderSupport class [implementation]

// Fast ASCII -> UTF16 inner loop implementations
//
// Convert_ascii_run will update src and dst to the new values, and
// len must be the maximum number ascii chars that it would be valid
// to take from src and place into dst.  (That is, the minimum of the
// number of bytes left in src and the number of unichars available in
// dst.)

#if defined(__arm__) || defined(_M_ARM)

// on ARM, do extra work to avoid byte/halfword reads/writes by
// reading/writing a word at a time for as long as we can
static inline void
Convert_ascii_run (const char *&src,
                   PRUnichar *&dst,
                   int32_t len)
{
  const uint32_t *src32;
  uint32_t *dst32;

  // with some alignments, we'd never actually break out of the slow loop, so
  // check and do the faster slow loop
  if ((((NS_PTR_TO_UINT32(dst) & 3) == 0) && ((NS_PTR_TO_UINT32(src) & 1) == 0)) ||
      (((NS_PTR_TO_UINT32(dst) & 3) == 2) && ((NS_PTR_TO_UINT32(src) & 1) == 1)))
  {
    while (((NS_PTR_TO_UINT32(src) & 3) ||
            (NS_PTR_TO_UINT32(dst) & 3)) &&
           len > 0)
    {
      if (*src & 0x80U)
        return;
      *dst++ = (PRUnichar) *src++;
      len--;
    }
  } else {
    goto finish;
  }

  // then go 4 bytes at a time
  src32 = (const uint32_t*) src;
  dst32 = (uint32_t*) dst;

  while (len > 4) {
    uint32_t in = *src32++;

    if (in & 0x80808080U) {
      src32--;
      break;
    }

    *dst32++ = ((in & 0x000000ff) >>  0) | ((in & 0x0000ff00) << 8);
    *dst32++ = ((in & 0x00ff0000) >> 16) | ((in & 0xff000000) >> 8);

    len -= 4;
  }

  src = (const char *) src32;
  dst = (PRUnichar *) dst32;

finish:
  while (len-- > 0 && (*src & 0x80U) == 0) {
    *dst++ = (PRUnichar) *src++;
  }
}

#else

#ifdef MOZILLA_MAY_SUPPORT_SSE2
namespace mozilla {
namespace SSE2 {

void Convert_ascii_run(const char *&src, PRUnichar *&dst, int32_t len);

}
}
#endif

static inline void
Convert_ascii_run (const char *&src,
                   PRUnichar *&dst,
                   int32_t len)
{
#ifdef MOZILLA_MAY_SUPPORT_SSE2
  if (mozilla::supports_sse2()) {
    mozilla::SSE2::Convert_ascii_run(src, dst, len);
    return;
  }
#endif

  while (len-- > 0 && (*src & 0x80U) == 0) {
    *dst++ = (PRUnichar) *src++;
  }
}

#endif

NS_IMETHODIMP nsUTF8ToUnicode::Convert(const char * aSrc,
                                       int32_t * aSrcLength,
                                       PRUnichar * aDest,
                                       int32_t * aDestLength)
{
  uint32_t aSrcLen   = (uint32_t) (*aSrcLength);
  uint32_t aDestLen = (uint32_t) (*aDestLength);

  const char *in, *inend;
  inend = aSrc + aSrcLen;

  PRUnichar *out, *outend;
  outend = aDest + aDestLen;

  nsresult res = NS_OK; // conversion result

  out = aDest;
  if (mState == 0xFF) {
    // Emit supplementary character left over from previous iteration. It is
    // caller's responsibility to keep a sufficient buffer.
    if (aDestLen < 2) {
      *aSrcLength = *aDestLength = 0;
      return NS_OK_UDEC_MOREOUTPUT;
    }
    out = EmitSurrogatePair(mUcs4, out);
    mUcs4 = 0;
    mState = 0;
    mBytes = 1;
    mFirst = false;
  }

  // alias these locally for speed
  int32_t mUcs4 = this->mUcs4;
  uint8_t mState = this->mState;
  uint8_t mBytes = this->mBytes;
  bool mFirst = this->mFirst;

  // Set mFirst to false now so we don't have to every time through the ASCII
  // branch within the loop.
  if (mFirst && aSrcLen && (0 == (0x80 & (*aSrc))))
    mFirst = false;

  for (in = aSrc; ((in < inend) && (out < outend)); ++in) {
    uint8_t c = *in;
    if (0 == mState) {
      // When mState is zero we expect either a US-ASCII character or a
      // multi-octet sequence.
      if (c < 0x80) {  // 00..7F
        int32_t max_loops = std::min(inend - in, outend - out);
        Convert_ascii_run(in, out, max_loops);
        --in; // match the rest of the cases
        mBytes = 1;
      } else if (c < 0xC2) {  // C0/C1
        // Overlong 2 octet sequence
        if (mErrBehavior == kOnError_Signal) {
          res = NS_ERROR_ILLEGAL_INPUT;
          break;
        }
        *out++ = UCS2_REPLACEMENT_CHAR;
        mFirst = false;
      } else if (c < 0xE0) {  // C2..DF
        // First octet of 2 octet sequence
        mUcs4 = c;
        mUcs4 = (mUcs4 & 0x1F) << 6;
        mState = 1;
        mBytes = 2;
      } else if (c < 0xF0) {  // E0..EF
        // First octet of 3 octet sequence
        mUcs4 = c;
        mUcs4 = (mUcs4 & 0x0F) << 12;
        mState = 2;
        mBytes = 3;
      } else if (c < 0xF5) {  // F0..F4
        // First octet of 4 octet sequence
        mUcs4 = c;
        mUcs4 = (mUcs4 & 0x07) << 18;
        mState = 3;
        mBytes = 4;
      } else {  // F5..FF
        /* Current octet is neither in the US-ASCII range nor a legal first
         * octet of a multi-octet sequence.
         */
        if (mErrBehavior == kOnError_Signal) {
          /* Return an error condition. Caller is responsible for flushing and
           * refilling the buffer and resetting state.
           */
          res = NS_ERROR_ILLEGAL_INPUT;
          break;
        }
        *out++ = UCS2_REPLACEMENT_CHAR;
        mFirst = false;
      }
    } else {
      // When mState is non-zero, we expect a continuation of the multi-octet
      // sequence
      if (0x80 == (0xC0 & c)) {
        if (mState > 1) {
          // If we are here, all possibilities are:
          // mState == 2 && mBytes == 3 ||
          // mState == 2 && mBytes == 4 ||
          // mState == 3 && mBytes == 4
          if ((mBytes == 3 && ((!mUcs4 && c < 0xA0) ||             // E0 80..9F
                               (mUcs4 == 0xD000 && c > 0x9F))) ||  // ED A0..BF
              (mState == 3 && ((!mUcs4 && c < 0x90) ||             // F0 80..8F
                               (mUcs4 == 0x100000 && c > 0x8F)))) {// F4 90..BF
            // illegal sequences or sequences converted into illegal ranges.
            in--;
            if (mErrBehavior == kOnError_Signal) {
              res = NS_ERROR_ILLEGAL_INPUT;
              break;
            }
            *out++ = UCS2_REPLACEMENT_CHAR;
            mState = 0;
            mFirst = false;
            continue;
          }
        }

        // Legal continuation.
        uint32_t shift = (mState - 1) * 6;
        uint32_t tmp = c;
        tmp = (tmp & 0x0000003FL) << shift;
        mUcs4 |= tmp;

        if (0 == --mState) {
          /* End of the multi-octet sequence. mUcs4 now contains the final
           * Unicode codepoint to be output
           */

          if (mUcs4 > 0xFFFF) {
            // mUcs4 is in the range 0x10000 - 0x10FFFF. Output a UTF-16 pair
            if (out + 2 > outend) {
              // insufficient space left in the buffer. Keep mUcs4 for the
              // next iteration.
              mState = 0xFF;
              ++in;
              res = NS_OK_UDEC_MOREOUTPUT;
              break;
            }
            out = EmitSurrogatePair(mUcs4, out);
          } else if (UNICODE_BYTE_ORDER_MARK != mUcs4 || !mFirst) {
            // Don't output the BOM only if it is the first character
            *out++ = mUcs4;
          }
          //initialize UTF8 cache
          mUcs4  = 0;
          mState = 0;
          mBytes = 1;
          mFirst = false;
        }
      } else {
        /* ((0xC0 & c != 0x80) && (mState != 0))
         *
         * Incomplete multi-octet sequence. Unconsume this
         * octet and return an error condition. Caller is responsible
         * for flushing and refilling the buffer and resetting state.
         */
        in--;
        if (mErrBehavior == kOnError_Signal) {
          res = NS_ERROR_ILLEGAL_INPUT;
          break;
        }
        *out++ = UCS2_REPLACEMENT_CHAR;
        mState = 0;
        mFirst = false;
      }
    }
  }

  // output not finished, output buffer too short
  if ((NS_OK == res) && (in < inend) && (out >= outend))
    res = NS_OK_UDEC_MOREOUTPUT;

  // last UCS4 is incomplete, make sure the caller
  // returns with properly aligned continuation of the buffer
  if ((NS_OK == res) && (mState != 0))
    res = NS_OK_UDEC_MOREINPUT;

  *aSrcLength = in - aSrc;
  *aDestLength = out - aDest;

  this->mUcs4 = mUcs4;
  this->mState = mState;
  this->mBytes = mBytes;
  this->mFirst = mFirst;

  return(res);
}
