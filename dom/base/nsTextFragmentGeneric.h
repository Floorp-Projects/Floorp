/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTextFragmentGeneric_h__
#define nsTextFragmentGeneric_h__

#include "nsTextFragmentGenericFwd.h"

#include "nscore.h"
#include "nsTextFragmentImpl.h"
#include <algorithm>

namespace mozilla {

template <class Arch>
int32_t FirstNon8Bit(const char16_t* str, const char16_t* end) {
  const uint32_t numUnicharsPerVector = xsimd::batch<int16_t, Arch>::size;
  using p = Non8BitParameters<sizeof(size_t)>;
  const size_t mask = p::mask();
  const uint32_t numUnicharsPerWord = p::numUnicharsPerWord();
  const int32_t len = end - str;
  int32_t i = 0;

  // Align ourselves to the Arch boundary
  int32_t alignLen = std::min(
      len, int32_t(((-NS_PTR_TO_INT32(str)) & (Arch::alignment() - 1)) /
                   sizeof(char16_t)));
  for (; i < alignLen; i++) {
    if (str[i] > 255) return i;
  }

  // Check one batch at a time.
  const int32_t vectWalkEnd =
      ((len - i) / numUnicharsPerVector) * numUnicharsPerVector;
  const uint16_t shortMask = 0xff00;
  xsimd::batch<int16_t, Arch> vectmask(static_cast<int16_t>(shortMask));
  for (; i < vectWalkEnd; i += numUnicharsPerVector) {
    const auto vect = xsimd::batch<int16_t, Arch>::load_aligned(str + i);
    if (xsimd::any((vect & vectmask) != 0)) return i;
  }

  // Check one word at a time.
  const int32_t wordWalkEnd =
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

}  // namespace mozilla

#endif
