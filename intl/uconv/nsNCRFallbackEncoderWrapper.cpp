/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNCRFallbackEncoderWrapper.h"

#include "mozilla/dom/EncodingUtils.h"

nsNCRFallbackEncoderWrapper::nsNCRFallbackEncoderWrapper(const nsACString& aEncoding)
 : mEncoder(mozilla::dom::EncodingUtils::EncoderForEncoding(aEncoding))
{
}

nsNCRFallbackEncoderWrapper::~nsNCRFallbackEncoderWrapper()
{
}

bool
nsNCRFallbackEncoderWrapper::WriteNCR(nsACString& aBytes,
                                      uint32_t& aDstWritten,
                                      int32_t aUnmappable)
{
  // To avoid potentially shrinking aBytes and then growing it back, use
  // another string for number formatting.
  nsAutoCString ncr("&#");
  ncr.AppendInt(aUnmappable);
  ncr.Append(';');
  uint32_t ncrLen = ncr.Length();
  uint32_t needed = aDstWritten + ncrLen;
  if (needed > INT32_MAX) {
    return false;
  }
  if (needed > aBytes.Length() && !aBytes.SetLength(needed,
                                                    mozilla::fallible_t())) {
    return false;
  }
  memcpy(aBytes.BeginWriting() + aDstWritten,
         ncr.BeginReading(),
         ncrLen);
  aDstWritten += ncrLen;
  return true;
}

bool
nsNCRFallbackEncoderWrapper::Encode(const nsAString& aUtf16,
                                    nsACString& aBytes)
{
  // nsIUnicodeEncoder uses int32_t for sizes :-(
  if (aUtf16.Length() > INT32_MAX) {
    return false;
  }
  const char16_t* src = aUtf16.BeginReading();
  const char16_t* srcEnd = aUtf16.EndReading();
  uint32_t dstWritten = 0;
  for (;;) {
    int32_t srcLen = srcEnd - src;
    int32_t dstLen = 0;
    nsresult rv = mEncoder->GetMaxLength(src, srcLen, &dstLen);
    if (NS_FAILED(rv)) {
      return false;
    }
    uint32_t needed = dstWritten + dstLen;
    if (needed > INT32_MAX) {
      return false;
    }
    // Behind the scenes SetLength() makes the underlying allocation not have
    // slop, so we don't need to round up here.
    if (needed > aBytes.Length() && !aBytes.SetLength(needed,
                                                      mozilla::fallible_t())) {
      return false;
    }
    // We need to re-obtain the destination pointer on every iteration, because
    // SetLength() invalidates it.
    char* dst = aBytes.BeginWriting() + dstWritten;
    dstLen = aBytes.Length() - dstWritten;
    mEncoder->Reset();
    rv = mEncoder->Convert(src, &srcLen, dst, &dstLen);
    // Update state tracking
    src += srcLen;
    dstWritten += dstLen;
    if (rv == NS_OK_UENC_MOREOUTPUT) {
      MOZ_ASSERT_UNREACHABLE("GetMaxLength must have returned a bogus length.");
      return false;
    }
    if (rv == NS_ERROR_UENC_NOMAPPING) {
      int32_t unmappable;
      // The unmappable code unit or the first half of an unmappable surrogate
      // pair is consumed by the encoder.
      MOZ_ASSERT(srcLen > 0, "Encoder should have consumed some input.");
      char16_t codeUnit = src[-1];
      // Let's see if it is a surrogate
      size_t highBits = (codeUnit & 0xFC00);
      if (highBits == 0xD800) {
        // high surrogate
        // Let's see if we actually have a surrogate pair.
        char16_t next;
        if (src < srcEnd && NS_IS_LOW_SURROGATE((next = *src))) {
          src++; // consume the low surrogate
          unmappable = SURROGATE_TO_UCS4(codeUnit, next);
        } else {
          // unpaired surrogate.
          unmappable = 0xFFFD;
        }
      } else if (highBits == 0xDC00) {
        // low surrogate
        // This must be an unpaired surrogate.
        unmappable = 0xFFFD;
      } else {
        // not a surrogate
        unmappable = codeUnit;
      }
      // If we are encoding to ISO-2022-JP, we need to let the encoder to
      // generate a transition to the ASCII state if not already there.
      dst = aBytes.BeginWriting() + dstWritten;
      dstLen = aBytes.Length() - dstWritten;
      rv = mEncoder->Finish(dst, &dstLen);
      dstWritten += dstLen;
      if (rv != NS_OK) {
        // Failures should be impossible if GetMaxLength works. Big5 is the
        // only case where Finish() may return NS_ERROR_UENC_NOMAPPING but
        // that should never happen right after Convert() has returned it.
        MOZ_ASSERT_UNREACHABLE("Broken encoder.");
        return false;
      }
      if (!WriteNCR(aBytes, dstWritten, unmappable)) {
        return false;
      }
      continue;
    }
    if (!(rv == NS_OK || rv == NS_OK_UENC_MOREINPUT)) {
      return false;
    }
    MOZ_ASSERT(src == srcEnd, "Converter did not consume all input.");
    dst = aBytes.BeginWriting() + dstWritten;
    dstLen = aBytes.Length() - dstWritten;
    rv = mEncoder->Finish(dst, &dstLen);
    dstWritten += dstLen;
    if (rv == NS_OK_UENC_MOREOUTPUT) {
      MOZ_ASSERT_UNREACHABLE("GetMaxLength must have returned a bogus length.");
      return false;
    }
    if (rv == NS_ERROR_UENC_NOMAPPING) {
      // Big5
      if (!WriteNCR(aBytes, dstWritten, 0xFFFD)) {
        return false;
      }
    }
    return aBytes.SetLength(dstWritten, mozilla::fallible_t());
  }
}

