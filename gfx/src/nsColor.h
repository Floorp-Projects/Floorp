/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsColor_h___
#define nsColor_h___

#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint8_t, uint32_t
#include "nscore.h"                     // for nsAString
#include "nsCoord.h"                    // for NSToIntRound

class nsAString;
class nsString;

// A color is a 32 bit unsigned integer with four components: R, G, B
// and A.
typedef uint32_t nscolor;

// Make a color out of r,g,b values. This assumes that the r,g,b values are
// properly constrained to 0-255. This also assumes that a is 255.
#define NS_RGB(_r,_g,_b) \
  ((nscolor) ((255 << 24) | ((_b)<<16) | ((_g)<<8) | (_r)))

// Make a color out of r,g,b,a values. This assumes that the r,g,b,a
// values are properly constrained to 0-255.
#define NS_RGBA(_r,_g,_b,_a) \
  ((nscolor) (((_a) << 24) | ((_b)<<16) | ((_g)<<8) | (_r)))

// Extract color components from nscolor
#define NS_GET_R(_rgba) ((uint8_t) ((_rgba) & 0xff))
#define NS_GET_G(_rgba) ((uint8_t) (((_rgba) >> 8) & 0xff))
#define NS_GET_B(_rgba) ((uint8_t) (((_rgba) >> 16) & 0xff))
#define NS_GET_A(_rgba) ((uint8_t) (((_rgba) >> 24) & 0xff))

namespace mozilla {

template<typename T>
inline uint8_t ClampColor(T aColor)
{
  if (aColor >= 255) {
    return 255;
  }
  if (aColor <= 0) {
    return 0;
  }
  return NSToIntRound(aColor);
}

} // namespace mozilla

// Fast approximate division by 255. It has the property that
// for all 0 <= n <= 255*255, FAST_DIVIDE_BY_255(n) == n/255.
// But it only uses two adds and two shifts instead of an 
// integer division (which is expensive on many processors).
//
// equivalent to target=v/255
#define FAST_DIVIDE_BY_255(target,v)               \
  PR_BEGIN_MACRO                                   \
    unsigned tmp_ = v;                             \
    target = ((tmp_ << 8) + tmp_ + 255) >> 16;     \
  PR_END_MACRO

enum class nsHexColorType : uint8_t {
  NoAlpha, // 3 or 6 digit hex colors only
  AllowAlpha, // 3, 4, 6, or 8 digit hex colors
};

// Translate a hex string to a color. Return true if it parses ok,
// otherwise return false.
// This accepts the number of digits specified by aType.
bool
NS_HexToRGBA(const nsAString& aBuf, nsHexColorType aType, nscolor* aResult);

// Compose one NS_RGB color onto another. The result is what
// you get if you draw aFG on top of aBG with operator OVER.
nscolor NS_ComposeColors(nscolor aBG, nscolor aFG);

namespace mozilla {

inline uint32_t RoundingDivideBy255(uint32_t n)
{
  // There is an approximate alternative: ((n << 8) + n + 32896) >> 16
  // But that is actually slower than this simple expression on a modern
  // machine with a modern compiler.
  return (n + 127) / 255;
}

// Blend one RGBA color with another based on a given ratio.
// It is a linear interpolation on each channel with alpha premultipled.
nscolor LinearBlendColors(nscolor aBg, nscolor aFg, uint_fast8_t aFgRatio);

} // namespace mozilla

// Translate a hex string to a color. Return true if it parses ok,
// otherwise return false.
// This version accepts 1 to 9 digits (missing digits are 0)
bool NS_LooseHexToRGB(const nsString& aBuf, nscolor* aResult);

// There is no function to translate a color to a hex string, because
// the hex-string syntax does not support transparency.

// Translate a color name to a color. Return true if it parses ok,
// otherwise return false.
bool NS_ColorNameToRGB(const nsAString& aBuf, nscolor* aResult);

// Returns an array of all possible color names, and sets
// *aSizeArray to the size of that array. Do NOT call |free()| on this array.
const char * const * NS_AllColorNames(size_t *aSizeArray);

// function to convert from HSL color space to RGB color space
// the float parameters are all expected to be in the range 0-1
nscolor NS_HSL2RGB(float h, float s, float l);

// Return a color name for the given nscolor.  If there is no color
// name for it, returns null.  If there are multiple possible color
// names for the given color, the first one in nsColorNameList.h
// (which is generally the first one in alphabetical order) will be
// returned.
const char* NS_RGBToColorName(nscolor aColor);

#endif /* nsColor_h___ */
