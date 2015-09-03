/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBIG5ToUnicode.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/ArrayUtils.h"
#include "nsBIG5Data.h"

nsBIG5ToUnicode::nsBIG5ToUnicode()
 : mPendingTrail(0)
 , mBig5Lead(0)
{
}

NS_IMETHODIMP
nsBIG5ToUnicode::Convert(const char* aSrc,
                         int32_t* aSrcLength,
                         char16_t* aDest,
                         int32_t* aDestLength)
{
  // We'll be doing comparisons as unsigned.
  const uint8_t* in = reinterpret_cast<const uint8_t*>(aSrc);
  const uint8_t* inEnd = in + *aSrcLength;
  char16_t* out = aDest;
  char16_t* outEnd = out + *aDestLength;

  if (mPendingTrail) {
    if (out == outEnd) {
      *aSrcLength = 0;
      *aDestLength = 0;
      return NS_OK_UDEC_MOREOUTPUT;
    }
    *out++ = mPendingTrail;
    mPendingTrail = 0;
  }
  for (;;) {
    if (in == inEnd) {
      *aSrcLength = in - reinterpret_cast<const uint8_t*>(aSrc);
      *aDestLength = out - aDest;
      return mBig5Lead ? NS_OK_UDEC_MOREINPUT : NS_OK;
    }
    if (out == outEnd) {
      *aSrcLength = in - reinterpret_cast<const uint8_t*>(aSrc);
      *aDestLength = out - aDest;
      return NS_OK_UDEC_MOREOUTPUT;
    }
    uint8_t b = *in++;
    if (!mBig5Lead) {
      if (b <= 0x7F) {
        *out++ = (char16_t)b;
        continue;
      }
      if (b >= 0x81 && b <= 0xFE) {
        mBig5Lead = b;
        continue;
      }
      if (mErrBehavior == kOnError_Signal) {
        --in;
        *aSrcLength = in - reinterpret_cast<const uint8_t*>(aSrc);
        *aDestLength = out - aDest;
        return NS_ERROR_ILLEGAL_INPUT;
      }
      *out++ = 0xFFFD;
      continue;
    }
    size_t lead = mBig5Lead;
    mBig5Lead = 0;
    size_t offset = (b < 0x7F) ? 0x40 : 0x62;
    if ((b >= 0x40 && b <= 0x7E) || (b >= 0xA1 && b <= 0xFE)) {
      size_t pointer = (lead - 0x81) * 157L + (b - offset);
      char16_t outTrail;
      switch (pointer) {
        case 1133:
          *out++ = 0x00CA;
          outTrail = 0x0304;
          break;
        case 1135:
          *out++ = 0x00CA;
          outTrail = 0x030C;
          break;
        case 1164:
          *out++ = 0x00EA;
          outTrail = 0x0304;
          break;
        case 1166:
          *out++ = 0x00EA;
          outTrail = 0x030C;
          break;
        default:
          char16_t lowBits = nsBIG5Data::LowBits(pointer);
          if (!lowBits) {
            if (b <= 0x7F) {
              // prepend byte to stream
              // Always legal, since we've always just read a byte
              // if we come here.
              --in;
            }
            if (mErrBehavior == kOnError_Signal) {
              --in;
              *aSrcLength = in - reinterpret_cast<const uint8_t*>(aSrc);
              *aDestLength = out - aDest;
              return NS_ERROR_ILLEGAL_INPUT;
            }
            *out++ = 0xFFFD;
            continue;
          }
          if (nsBIG5Data::IsAstral(pointer)) {
            uint32_t codePoint = uint32_t(lowBits) | 0x20000;
            *out++ = char16_t(0xD7C0 + (codePoint >> 10));
            outTrail = char16_t(0xDC00 + (codePoint & 0x3FF));
            break;
          }
          *out++ = lowBits;
          continue;
      }
      if (out == outEnd) {
        mPendingTrail = outTrail;
        *aSrcLength = in - reinterpret_cast<const uint8_t*>(aSrc);
        *aDestLength = out - aDest;
        return NS_OK_UDEC_MOREOUTPUT;
      }
      *out++ = outTrail;
      continue;
    }
    // pointer is null
    if (b <= 0x7F) {
      // prepend byte to stream
      // Always legal, since we've always just read a byte
      // if we come here.
      --in;
    }
    if (mErrBehavior == kOnError_Signal) {
      // Moving in one past the start of aSrc is actually OK per API contract,
      // since assigning -1 to aSrcLength means that we want the caller to
      // record one U+FFFD and repush the same input buffer.
      --in;
      *aSrcLength = in - reinterpret_cast<const uint8_t*>(aSrc);
      *aDestLength = out - aDest;
      return NS_ERROR_ILLEGAL_INPUT;
    }
    *out++ = 0xFFFD;
    continue;
  }
}

NS_IMETHODIMP
nsBIG5ToUnicode::GetMaxLength(const char* aSrc,
                              int32_t aSrcLength,
                              int32_t* aDestLength)
{
  // The length of the output in UTF-16 code units never exceeds the length
  // of the input in bytes.
  *aDestLength = aSrcLength + (mPendingTrail ? 1 : 0) + (mBig5Lead ? 1 : 0);
  return NS_OK;
}

NS_IMETHODIMP
nsBIG5ToUnicode::Reset()
{
  mPendingTrail = 0;
  mBig5Lead = 0;
  return NS_OK;
}
