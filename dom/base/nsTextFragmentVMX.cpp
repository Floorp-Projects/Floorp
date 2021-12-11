/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file should only be compiled if you're on Power ISA.

#include "nscore.h"
#include "nsAlgorithm.h"
#include "nsTextFragmentImpl.h"
#include <algorithm>
#include <altivec.h>

namespace mozilla {
namespace VMX {

int32_t FirstNon8Bit(const char16_t* str, const char16_t* end) {
  const uint32_t numUnicharsPerVector = 8;
  const uint32_t numCharsPerVector = 16;
  // Paranoia. If this assertion is wrong, change the vector loop below.
  MOZ_ASSERT((numCharsPerVector / numUnicharsPerVector) == sizeof(char16_t));

  typedef Non8BitParameters<sizeof(size_t)> p;
  const uint32_t alignMask = p::alignMask();
  const size_t mask = p::mask();
  const uint32_t numUnicharsPerWord = p::numUnicharsPerWord();

  const uint32_t len = end - str;

  // i shall count the index in unichars; i2 shall count the index in chars.
  uint32_t i = 0;
  uint32_t i2 = 0;

  // Align ourselves to a 16-byte boundary, as required by VMX loads.
  uint32_t alignLen = std::min(
      len, uint32_t(((-NS_PTR_TO_UINT32(str)) & 0xf) / sizeof(char16_t)));

  if ((len - alignLen) >= numUnicharsPerVector) {
    for (; i < alignLen; i++) {
      if (str[i] > 255) return i;
    }

    // Construct a vector of shorts.
#if __LITTLE_ENDIAN__
    const vector unsigned short gtcompare =
        reinterpret_cast<vector unsigned short>(
            vec_mergel(vec_splat_s8(-1), vec_splat_s8(0)));
#else
    const vector unsigned short gtcompare =
        reinterpret_cast<vector unsigned short>(
            vec_mergel(vec_splat_s8(0), vec_splat_s8(-1)));
#endif
    const uint32_t vectWalkEnd =
        ((len - i) / numUnicharsPerVector) * numUnicharsPerVector;
    i2 = i * sizeof(char16_t);

    while (1) {
      vector unsigned short vect;

      // Check one VMX register (8 unichars) at a time. The vec_any_gt
      // intrinsic does exactly what we want. This loop is manually unrolled;
      // it yields notable performance improvements this way.
#define CheckForASCII                                              \
  vect = vec_ld(i2, reinterpret_cast<const unsigned short*>(str)); \
  if (vec_any_gt(vect, gtcompare)) return i;                       \
  i += numUnicharsPerVector;                                       \
  if (!(i < vectWalkEnd)) break;                                   \
  i2 += numCharsPerVector;

      CheckForASCII CheckForASCII

#undef CheckForASCII
    }
  } else {
    // Align ourselves to a word boundary.
    alignLen = std::min(len, uint32_t(((-NS_PTR_TO_UINT32(str)) & alignMask) /
                                      sizeof(char16_t)));
    for (; i < alignLen; i++) {
      if (str[i] > 255) return i;
    }
  }

  // Check one word at a time.
  const uint32_t wordWalkEnd =
      ((len - i) / numUnicharsPerWord) * numUnicharsPerWord;
  for (; i < wordWalkEnd; i += numUnicharsPerWord) {
    const size_t word = *reinterpret_cast<const size_t*>(str + i);
    if (word & mask) return i;
  }

  // Take care of the remainder one character at a time.
  for (; i < len; i++) {
    if (str[i] > 255) {
      return i;
    }
  }

  return -1;
}

}  // namespace VMX
}  // namespace mozilla
