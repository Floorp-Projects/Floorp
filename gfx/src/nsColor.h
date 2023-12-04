/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsColor_h___
#define nsColor_h___

#include <stdint.h>   // for uint8_t, uint32_t
#include "nsCoord.h"  // for NSToIntRound
#include "nsStringFwd.h"

// A color is a 32 bit unsigned integer with four components: R, G, B
// and A.
typedef uint32_t nscolor;

// Make a color out of r,g,b values. This assumes that the r,g,b values are
// properly constrained to 0-255. This also assumes that a is 255.
#define NS_RGB(_r, _g, _b) \
  ((nscolor)((255 << 24) | ((_b) << 16) | ((_g) << 8) | (_r)))

// Make a color out of r,g,b,a values. This assumes that the r,g,b,a
// values are properly constrained to 0-255.
#define NS_RGBA(_r, _g, _b, _a) \
  ((nscolor)(((_a) << 24) | ((_b) << 16) | ((_g) << 8) | (_r)))

// Extract color components from nscolor
#define NS_GET_R(_rgba) ((uint8_t)((_rgba) & 0xff))
#define NS_GET_G(_rgba) ((uint8_t)(((_rgba) >> 8) & 0xff))
#define NS_GET_B(_rgba) ((uint8_t)(((_rgba) >> 16) & 0xff))
#define NS_GET_A(_rgba) ((uint8_t)(((_rgba) >> 24) & 0xff))

namespace mozilla {

template <typename T>
inline uint8_t ClampColor(T aColor) {
  if (aColor >= 255) {
    return 255;
  }
  if (aColor <= 0) {
    return 0;
  }
  return NSToIntRound(aColor);
}

}  // namespace mozilla

enum class nsHexColorType : uint8_t {
  NoAlpha,     // 3 or 6 digit hex colors only
  AllowAlpha,  // 3, 4, 6, or 8 digit hex colors
};

// Translate a hex string to a color. Return true if it parses ok,
// otherwise return false.
// This accepts the number of digits specified by aType.
bool NS_HexToRGBA(const nsAString& aBuf, nsHexColorType aType,
                  nscolor* aResult);

// Compose one NS_RGB color onto another. The result is what
// you get if you draw aFG on top of aBG with operator OVER.
nscolor NS_ComposeColors(nscolor aBG, nscolor aFG);

namespace mozilla {

inline uint32_t RoundingDivideBy255(uint32_t n) {
  // There is an approximate alternative: ((n << 8) + n + 32896) >> 16
  // But that is actually slower than this simple expression on a modern
  // machine with a modern compiler.
  return (n + 127) / 255;
}

}  // namespace mozilla

// Translate a hex string to a color. Return true if it parses ok,
// otherwise return false.
// This version accepts 1 to 9 digits (missing digits are 0)
bool NS_LooseHexToRGB(const nsString& aBuf, nscolor* aResult);

// There is no function to translate a color to a hex string, because
// the hex-string syntax does not support transparency.

#endif /* nsColor_h___ */
