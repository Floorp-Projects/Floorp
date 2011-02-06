/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 *
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// This file should only be compiled if you're on x86 or x86_64.  Additionally,
// you'll need to compile this file with -msse2 if you're using gcc.

#include <emmintrin.h>
#include "nscore.h"

namespace mozilla {
namespace SSE2 {

void
Convert_ascii_run(const char *&src,
                  PRUnichar  *&dst,
                  PRInt32      len)
{
  if (len > 15) {
    __m128i in, out1, out2;
    __m128d *outp1, *outp2;
    __m128i zeroes;
    PRUint32 offset;

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
