/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XiphExtradata.h"

namespace mozilla {

bool XiphHeadersToExtradata(MediaByteBuffer* aCodecSpecificConfig,
                            const nsTArray<const unsigned char*>& aHeaders,
                            const nsTArray<size_t>& aHeaderLens)
{
  size_t nheaders = aHeaders.Length();
  if (!nheaders || nheaders > 255) return false;
  aCodecSpecificConfig->AppendElement(nheaders - 1);
  for (size_t i = 0; i < nheaders - 1; i++) {
    size_t headerLen;
    for (headerLen = aHeaderLens[i]; headerLen >= 255; headerLen -= 255) {
      aCodecSpecificConfig->AppendElement(255);
    }
    aCodecSpecificConfig->AppendElement(headerLen);
  }
  for (size_t i = 0; i < nheaders; i++) {
    aCodecSpecificConfig->AppendElements(aHeaders[i], aHeaderLens[i]);
  }
  return true;
}

bool XiphExtradataToHeaders(nsTArray<unsigned char*>& aHeaders,
                            nsTArray<size_t>& aHeaderLens,
                            unsigned char* aData,
                            size_t aAvailable)
{
  size_t total = 0;
  if (aAvailable < 1) {
    return false;
  }
  aAvailable--;
  int nHeaders = *aData++ + 1;
  for (int i = 0; i < nHeaders - 1; i++) {
    size_t headerLen = 0;
    for (;;) {
      // After this test, we know that (aAvailable - total > headerLen) and
      // (headerLen >= 0) so (aAvailable - total > 0). The loop decrements
      // aAvailable by 1 and total remains fixed, so we know that in the next
      // iteration (aAvailable - total >= 0). Thus (aAvailable - total) can
      // never underflow.
      if (aAvailable - total <= headerLen) {
        return false;
      }
      // Since we know (aAvailable > total + headerLen), this can't overflow
      // unless total is near 0 and both aAvailable and headerLen are within
      // 255 bytes of the maximum representable size. However, that is
      // impossible, since we would have had to have gone through this loop
      // more than 255 times to make headerLen that large, and thus decremented
      // aAvailable more than 255 times.
      headerLen += *aData;
      aAvailable--;
      if (*aData++ != 255) break;
    }
    // And this check ensures updating total won't cause (aAvailable - total)
    // to underflow.
    if (aAvailable - total < headerLen) {
      return false;
    }
    aHeaderLens.AppendElement(headerLen);
    // Since we know aAvailable >= total + headerLen, this can't overflow.
    total += headerLen;
  }
  aHeaderLens.AppendElement(aAvailable - total);
  for (int i = 0; i < nHeaders; i++) {
    aHeaders.AppendElement(aData);
    aData += aHeaderLens[i];
  }
  return true;
}

} // namespace mozilla
