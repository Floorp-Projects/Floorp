/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nscore.h"
#include "nsAlgorithm.h"
#include "nsTextFragmentImpl.h"
#include <altivec.h>

namespace mozilla {
namespace altivec {

int32_t
FirstNon8Bit(const PRUnichar* str, const PRUnichar* end)
{
  const uint32_t numUnicharsPerVector = 8;
  const uint32_t numCharsPerVector = 16;

  typedef Non8BitParameters<sizeof(size_t)> p;
  const uint32_t alignMask = p::alignMask();
  const size_t mask = p::mask();
  const uint32_t numUnicharsPerWord = p::numUnicharsPerWord();

  const uint32_t len = end - str;
  uint32_t i = 0;

  // Align ourselves to a 16-byte boundary, as required by AltiVec loads.
  uint32_t alignLen =
    NS_MIN(len,
           uint32_t(((-NS_PTR_TO_UINT32(str)) & 0xf) / sizeof(PRUnichar)));

  if ((len - alignLen) >= numUnicharsPerVector) {
    for (; i < alignLen; i++) {
      if (str[i] > 255)
        return i;
    }

    register const vector unsigned short gtcompare =
      reinterpret_cast<vector unsigned short>(vec_mergel(vec_splat_s8(0),
                                                         vec_splat_s8(-1)));
    // Check one AltiVec register (16 bytes) at a time.
    // This is simpler on AltiVec and involves no mucking about with masks,
    // since the vec_any_gt intrinsic does exactly what we want.
    const uint32_t vectWalkEnd = ((len - i) / numUnicharsPerVector)
                                 * numUnicharsPerVector;
    // We use this a lot, so let's calculate it now.
    const uint32_t vectFactor = (numCharsPerVector/numUnicharsPerVector);
    uint32_t i2 = i * vectFactor;
      while (1) {
        register vector unsigned short vect;

        // This loop is manually unrolled because
        // that way performance is significantly improved
#define CheckForASCII                                                    \
        vect = vec_ld(i2, reinterpret_cast<const unsigned short*>(str)); \
        if (vec_any_gt(vect, gtcompare))                                 \
          return (i2 / vectFactor);                                      \
        i2 += numCharsPerVector;                                         \
        if (!(i2 < vectWalkEnd))                                         \
          break;

        CheckForASCII
        CheckForASCII
      }
      i = i2 / vectFactor;
  } else {
    // Align ourselves to a word boundary.
    alignLen =
      NS_MIN(len, uint32_t(((-NS_PTR_TO_UINT32(str)) & alignMask)
                          / sizeof(PRUnichar)));
    for (; i < alignLen; i++) {
      if (str[i] > 255)
        return i;
    }
  }

  // Check one word at a time.
  const uint32_t wordWalkEnd = ((len - i) / numUnicharsPerWord)
                               * numUnicharsPerWord;
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

} // namespace altivec
} // namespace mozilla
