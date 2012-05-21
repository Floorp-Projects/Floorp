/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file should only be compiled if you're on x86 or x86_64.  Additionally,
// you'll need to compile this file with -msse2 if you're using gcc.

#include <emmintrin.h>
#include "nscore.h"
#include "nsAlgorithm.h"

namespace mozilla {
namespace SSE2 {

static inline bool
is_zero (__m128i x)
{
  return
    _mm_movemask_epi8(_mm_cmpeq_epi8(x, _mm_setzero_si128())) == 0xffff;
}

PRInt32
FirstNon8Bit(const PRUnichar *str, const PRUnichar *end)
{
  const PRUint32 numUnicharsPerVector = 8;

#if PR_BYTES_PER_WORD == 4
  const size_t mask = 0xff00ff00;
  const PRUint32 numUnicharsPerWord = 2;
#elif PR_BYTES_PER_WORD == 8
  const size_t mask = 0xff00ff00ff00ff00;
  const PRUint32 numUnicharsPerWord = 4;
#else
#error Unknown platform!
#endif

  const PRInt32 len = end - str;
  PRInt32 i = 0;

  // Align ourselves to a 16-byte boundary, as required by _mm_load_si128
  // (i.e. MOVDQA).
  PRInt32 alignLen =
    NS_MIN(len, PRInt32(((-NS_PTR_TO_INT32(str)) & 0xf) / sizeof(PRUnichar)));
  for (; i < alignLen; i++) {
    if (str[i] > 255)
      return i;
  }

  // Check one XMM register (16 bytes) at a time.
  const PRInt32 vectWalkEnd = ((len - i) / numUnicharsPerVector) * numUnicharsPerVector;
  __m128i vectmask = _mm_set1_epi16(0xff00);
  for(; i < vectWalkEnd; i += numUnicharsPerVector) {
    const __m128i vect = *reinterpret_cast<const __m128i*>(str + i);
    if (!is_zero(_mm_and_si128(vect, vectmask)))
      return i;
  }

  // Check one word at a time.
  const PRInt32 wordWalkEnd = ((len - i) / numUnicharsPerWord) * numUnicharsPerWord;
  for(; i < wordWalkEnd; i += numUnicharsPerWord) {
    const size_t word = *reinterpret_cast<const size_t*>(str + i);
    if (word & mask)
      return i;
  }

  // Take care of the remainder one character at a time.
  for (; i < len; i++) {
    if (str[i] > 255) {
      return i;
    }
  }

  return -1;
}

} // namespace SSE2
} // namespace mozilla
