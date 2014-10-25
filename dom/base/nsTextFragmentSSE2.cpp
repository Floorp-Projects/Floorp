/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file should only be compiled if you're on x86 or x86_64.  Additionally,
// you'll need to compile this file with -msse2 if you're using gcc.

#include <emmintrin.h>
#include "nscore.h"
#include "nsAlgorithm.h"
#include "nsTextFragmentImpl.h"
#include <algorithm>

namespace mozilla {
namespace SSE2 {

static inline bool
is_zero (__m128i x)
{
  return
    _mm_movemask_epi8(_mm_cmpeq_epi8(x, _mm_setzero_si128())) == 0xffff;
}

int32_t
FirstNon8Bit(const char16_t *str, const char16_t *end)
{
  const uint32_t numUnicharsPerVector = 8;
  typedef Non8BitParameters<sizeof(size_t)> p;
  const size_t mask = p::mask();
  const uint32_t numUnicharsPerWord = p::numUnicharsPerWord();
  const int32_t len = end - str;
  int32_t i = 0;

  // Align ourselves to a 16-byte boundary, as required by _mm_load_si128
  // (i.e. MOVDQA).
  int32_t alignLen =
    std::min(len, int32_t(((-NS_PTR_TO_INT32(str)) & 0xf) / sizeof(char16_t)));
  for (; i < alignLen; i++) {
    if (str[i] > 255)
      return i;
  }

  // Check one XMM register (16 bytes) at a time.
  const int32_t vectWalkEnd = ((len - i) / numUnicharsPerVector) * numUnicharsPerVector;
  const uint16_t shortMask = 0xff00;
  __m128i vectmask = _mm_set1_epi16(static_cast<int16_t>(shortMask));
  for(; i < vectWalkEnd; i += numUnicharsPerVector) {
    const __m128i vect = *reinterpret_cast<const __m128i*>(str + i);
    if (!is_zero(_mm_and_si128(vect, vectmask)))
      return i;
  }

  // Check one word at a time.
  const int32_t wordWalkEnd = ((len - i) / numUnicharsPerWord) * numUnicharsPerWord;
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
