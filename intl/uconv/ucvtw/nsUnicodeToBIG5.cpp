/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUnicodeToBIG5.h"

NS_IMPL_ADDREF(nsUnicodeToBIG5)
NS_IMPL_RELEASE(nsUnicodeToBIG5)
NS_IMPL_QUERY_INTERFACE(nsUnicodeToBIG5,
                        nsIUnicodeEncoder)

nsUnicodeToBIG5::nsUnicodeToBIG5()
 : mUtf16Lead(0)
 , mPendingTrail(0)
 , mSignal(true) // as in nsEncoderSupport
{
}

NS_IMETHODIMP
nsUnicodeToBIG5::Convert(const char16_t* aSrc,
                         int32_t* aSrcLength,
                         char* aDest,
                         int32_t * aDestLength)
{
  const char16_t* in = aSrc;
  const char16_t* inEnd = in + *aSrcLength;
  uint8_t* out = reinterpret_cast<uint8_t*>(aDest);
  uint8_t* outEnd = out + *aDestLength;

  MOZ_ASSERT(!(mPendingTrail && mUtf16Lead),
             "Can't have both pending output and pending input.");

  if (mPendingTrail) {
    if (out == outEnd) {
      *aSrcLength = 0;
      *aDestLength = 0;
      return NS_OK_UENC_MOREOUTPUT;
    }
    *out++ = mPendingTrail;
    mPendingTrail = 0;
  }
  for (;;) {
    if (in == inEnd) {
      *aSrcLength = in - aSrc;
      *aDestLength = out - reinterpret_cast<uint8_t*>(aDest);
      return mUtf16Lead ? NS_OK_UENC_MOREINPUT : NS_OK;
    }
    if (out == outEnd) {
      *aSrcLength = in - aSrc;
      *aDestLength = out - reinterpret_cast<uint8_t*>(aDest);
      return NS_OK_UENC_MOREOUTPUT;
    }
    bool isAstral; // true means Plane 2, false means BMP
    char16_t lowBits; // The low 16 bits of the code point
    char16_t codeUnit = *in++;
    size_t highBits = (codeUnit & 0xFC00);
    if (highBits == 0xD800) {
      // high surrogate
      if (mUtf16Lead) {
        // High surrogate follows another high surrogate. The
        // *previous* code unit is in error.
        if (mSignal) {
          mUtf16Lead = 0;
          // NOTE: Encode API differs from decode API!
          --in;
          *aSrcLength = in - aSrc;
          *aDestLength = out - reinterpret_cast<uint8_t*>(aDest);
          return NS_ERROR_UENC_NOMAPPING;
        }
        *out++ = '?';
      }
      mUtf16Lead = codeUnit;
      continue;
    }
    if (highBits == 0xDC00) {
      // low surrogate
      if (!mUtf16Lead) {
        // Got low surrogate without a previous high surrogate
        if (mSignal) {
          // NOTE: Encode API differs from decode API!
          *aSrcLength = in - aSrc;
          *aDestLength = out - reinterpret_cast<uint8_t*>(aDest);
          return NS_ERROR_UENC_NOMAPPING;
        }
        *out++ = '?';
        continue;
      }
      size_t codePoint = (mUtf16Lead << 10) + codeUnit -
                         (((0xD800 << 10) - 0x10000) + 0xDC00);
      mUtf16Lead = 0;
      // Plane 2 is the only astral plane that has potentially
      // Big5-encodable characters.
      if ((0xFF0000 & codePoint) != 0x20000) {
        if (mSignal) {
          // NOTE: Encode API differs from decode API!
          // nsSaveAsCharset wants us to back up on step in the case of a
          // surrogate pair.
          --in;
          *aSrcLength = in - aSrc;
          *aDestLength = out - reinterpret_cast<uint8_t*>(aDest);
          return NS_ERROR_UENC_NOMAPPING;
        }
        *out++ = '?';
        continue;
      }
      isAstral = true;
      lowBits = (char16_t)(codePoint & 0xFFFF);
    } else {
      // not a surrogate
      if (mUtf16Lead) {
        // Non-surrogate follows a high surrogate. The *previous*
        // code unit is in error.
        mUtf16Lead = 0;
        if (mSignal) {
          // NOTE: Encode API differs from decode API!
          --in;
          *aSrcLength = in - aSrc;
          *aDestLength = out - reinterpret_cast<uint8_t*>(aDest);
          return NS_ERROR_UENC_NOMAPPING;
        }
        *out++ = '?';
        // Let's unconsume this code unit and reloop in order to
        // re-check if the output buffer still has space.
        --in;
        continue;
      }
      isAstral = false;
      lowBits = codeUnit;
    }
    // isAstral now tells us if we have a Plane 2 or a BMP character.
    // lowBits tells us the low 16 bits.
    // After all the above setup to deal with UTF-16, we are now
    // finally ready to follow the spec.
    if (!isAstral && lowBits <= 0x7F) {
      *out++ = (uint8_t)lowBits;
      continue;
    }
    size_t pointer = nsBIG5Data::FindPointer(lowBits, isAstral);
    if (!pointer) {
      if (mSignal) {
        // NOTE: Encode API differs from decode API!
        if (isAstral) {
          // nsSaveAsCharset wants us to back up on step in the case of a
          // surrogate pair.
          --in;
        }
        *aSrcLength = in - aSrc;
        *aDestLength = out - reinterpret_cast<uint8_t*>(aDest);
        return NS_ERROR_UENC_NOMAPPING;
      }
      *out++ = '?';
      continue;
    }
    uint8_t lead = (uint8_t)(pointer / 157 + 0x81);
    uint8_t trail = (uint8_t)(pointer % 157);
    if (trail < 0x3F) {
      trail += 0x40;
    } else {
      trail += 0x62;
    }
    *out++ = lead;
    if (out == outEnd) {
      mPendingTrail = trail;
      *aSrcLength = in - aSrc;
      *aDestLength = out - reinterpret_cast<uint8_t*>(aDest);
      return NS_OK_UENC_MOREOUTPUT;
    }
    *out++ = trail;
    continue;
  }
}

NS_IMETHODIMP
nsUnicodeToBIG5::Finish(char* aDest,
                        int32_t* aDestLength)
{
  MOZ_ASSERT(!(mPendingTrail && mUtf16Lead),
             "Can't have both pending output and pending input.");
  uint8_t* out = reinterpret_cast<uint8_t*>(aDest);
  if (mPendingTrail) {
    if (*aDestLength < 1) {
      *aDestLength = 0;
      return NS_OK_UENC_MOREOUTPUT;
    }
    *out = mPendingTrail;
    mPendingTrail = 0;
    *aDestLength = 1;
    return NS_OK;
  }
  if (mUtf16Lead) {
    if (*aDestLength < 1) {
      *aDestLength = 0;
      return NS_OK_UENC_MOREOUTPUT;
    }
    mUtf16Lead = 0;
    if (mSignal) {
      *aDestLength = 0;
      return NS_ERROR_UENC_NOMAPPING;
    }
    *out = '?';
    *aDestLength = 1;
    return NS_OK;
  }
  *aDestLength = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsUnicodeToBIG5::GetMaxLength(const char16_t* aSrc,
                              int32_t aSrcLength,
                              int32_t* aDestLength)
{
  *aDestLength = (aSrcLength * 2) +
                 (mPendingTrail ? 1 : 0) +
                 // If the lead ends up being paired, the bytes produced
                 // are already included above.
                 // If not, it produces a single '?'.
                 (mUtf16Lead ? 1 : 0);
  return NS_OK;
}

NS_IMETHODIMP
nsUnicodeToBIG5::Reset()
{
  mUtf16Lead = 0;
  mPendingTrail = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsUnicodeToBIG5::SetOutputErrorBehavior(int32_t aBehavior,
                                        nsIUnicharEncoder* aEncoder,
                                        char16_t aChar)
{
  switch (aBehavior) {
    case kOnError_Signal:
      mSignal = true;
      break;
    case kOnError_Replace:
      mSignal = false;
      MOZ_ASSERT(aChar == '?', "Unsupported replacement.");
      break;
    case kOnError_CallBack:
      MOZ_ASSERT_UNREACHABLE("kOnError_CallBack is supposed to be unused.");
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Non-existent enum item.");
      break;
    }
    return NS_OK;
}
