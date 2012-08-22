/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file should only be compiled if you're on x86 or x86_64.  Additionally,
// you'll need to compile this file with -msse2 if you're using gcc.

#include <emmintrin.h>
#include "nscore.h"

namespace mozilla {
namespace SSE2 {

void
Convert_ascii_run(const char *&src,
                  PRUnichar  *&dst,
                  int32_t      len)
{
  if (len > 15) {
    __m128i in, out1, out2;
    __m128d *outp1, *outp2;
    __m128i zeroes;
    uint32_t offset;

    // align input to 16 bytes
    while ((NS_PTR_TO_UINT32(src) & 15) && len > 0) {
      if (*src & 0x80U)
        return;
      *dst++ = (PRUnichar) *src++;
      len--;
    }

    zeroes = _mm_setzero_si128();

    offset = NS_PTR_TO_UINT32(dst) & 15;

    // Note: all these inner loops have to break, not return; we need
    // to let the single-char loop below catch any leftover
    // byte-at-a-time ASCII chars, since this function must consume
    // all available ASCII chars before it returns

    if (offset == 0) {
      while (len > 15) {
        in = _mm_load_si128((__m128i *) src);
        if (_mm_movemask_epi8(in))
          break;
        out1 = _mm_unpacklo_epi8(in, zeroes);
        out2 = _mm_unpackhi_epi8(in, zeroes);
        _mm_stream_si128((__m128i *) dst, out1);
        _mm_stream_si128((__m128i *) (dst + 8), out2);
        dst += 16;
        src += 16;
        len -= 16;
      }
    } else if (offset == 8) {
      outp1 = (__m128d *) &out1;
      outp2 = (__m128d *) &out2;
      while (len > 15) {
        in = _mm_load_si128((__m128i *) src);
        if (_mm_movemask_epi8(in))
          break;
        out1 = _mm_unpacklo_epi8(in, zeroes);
        out2 = _mm_unpackhi_epi8(in, zeroes);
        _mm_storel_epi64((__m128i *) dst, out1);
        _mm_storel_epi64((__m128i *) (dst + 8), out2);
        _mm_storeh_pd((double *) (dst + 4), *outp1);
        _mm_storeh_pd((double *) (dst + 12), *outp2);
        src += 16;
        dst += 16;
        len -= 16;
      }
    } else {
      while (len > 15) {
        in = _mm_load_si128((__m128i *) src);
        if (_mm_movemask_epi8(in))
          break;
        out1 = _mm_unpacklo_epi8(in, zeroes);
        out2 = _mm_unpackhi_epi8(in, zeroes);
        _mm_storeu_si128((__m128i *) dst, out1);
        _mm_storeu_si128((__m128i *) (dst + 8), out2);
        src += 16;
        dst += 16;
        len -= 16;
      }
    }
  }

  // finish off a byte at a time

  while (len-- > 0 && (*src & 0x80U) == 0) {
    *dst++ = (PRUnichar) *src++;
  }
}

} // namespace SSE2
} // namespace mozilla
