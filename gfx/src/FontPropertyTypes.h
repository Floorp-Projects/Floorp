/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* font specific types shared by both thebes and layout */

#ifndef GFX_FONT_PROPERTY_TYPES_H
#define GFX_FONT_PROPERTY_TYPES_H

#include <cstdint>
#include <cmath>
#include "mozilla/Assertions.h"

/*
 * This file is separate from gfxFont.h so that layout can include it
 * without bringing in gfxFont.h and everything it includes.
 */

namespace mozilla {

/**
 * Generic template for font property type classes that use a fixed-point
 * internal representation.
 * Template parameters:
 *   T - the integer type to use as the internal representation (e.g. uint16_t)
 *       * NOTE that T must NOT be plain /int/, as that would result in
 *         ambiguity between constructors from /int/ and /T/, which mean
 *         different things.
 *   FractionBits - number of bits to use for the fractional part
 *   Min, Max - [inclusive] limits to the range of values that may be stored
 * Values are constructed from and exposed as floating-point, but stored
 * internally as fixed point, so there will be a quantization effect on
 * fractional values, depending on the number of fractional bits used.
 * Using (16-bit) fixed-point types rather than floats for these style
 * attributes reduces the memory footprint of gfxFontEntry and gfxFontStyle;
 * it will also tend to reduce the number of distinct font instances that
 * get created, particularly when styles are animated or set to arbitrary
 * values (e.g. by sliders in the UI), which should reduce pressure on
 * graphics resources and improve cache hit rates.
 */
template<class T,unsigned FractionBits,int Min,int Max>
class FontPropertyValue
{
public:
  // Ugh. We need a default constructor to allow this type to be used in the
  // union in nsCSSValue. Furthermore we need the default and copy
  // constructors to be "trivial" (i.e. the compiler implemented defaults that
  // do no initialization).
  // Annoyingly we can't make the default implementations constexpr (at least
  // in clang). That would be nice to do in order to allow the methods of
  // subclasses that always return the same value (e.g., FontWeight::Thin())
  // to also be constexpr. :/
  FontPropertyValue() = default;
  explicit FontPropertyValue(const FontPropertyValue& aOther) = default;
  FontPropertyValue& operator= (const FontPropertyValue& aOther) = default;

  bool operator==(const FontPropertyValue& aOther) const
  {
    return mValue == aOther.mValue;
  }
  bool operator!=(const FontPropertyValue& aOther) const
  {
    return mValue != aOther.mValue;
  }
  bool operator<(const FontPropertyValue& aOther) const
  {
    return mValue < aOther.mValue;
  }
  bool operator>(const FontPropertyValue& aOther) const
  {
    return mValue > aOther.mValue;
  }
  bool operator<=(const FontPropertyValue& aOther) const
  {
    return mValue <= aOther.mValue;
  }
  bool operator>=(const FontPropertyValue& aOther) const
  {
    return mValue >= aOther.mValue;
  }

  // The difference between two values, returned as a raw floating-point number
  // (which might not be a valid property value in its own right).
  float operator-(const FontPropertyValue& aOther) const
  {
    return (mValue - aOther.mValue) * kInverseScale;
  }

  /// Return the raw internal representation, for purposes of hashing.
  T ForHash() const
  {
    return mValue;
  }

protected:
  typedef T internal_type;

  // Construct from a floating-point or integer value, checking that it is
  // within the allowed range and converting to fixed-point representation.
  explicit FontPropertyValue(float aValue)
    : mValue(std::round(aValue * kScale))
  {
    MOZ_ASSERT(aValue >= kMin && aValue <= kMax);
  }
  explicit FontPropertyValue(int aValue)
    : mValue(aValue << kFractionBits)
  {
    MOZ_ASSERT(aValue >= Min && aValue <= Max);
  }

  // Construct directly from a fixed-point value of type T, with no check;
  // note that there may be special "flag" values that are outside the normal
  // min/max range (e.g. for font-style:italic, distinct from oblique angle).
  explicit FontPropertyValue(T aValue)
    : mValue(aValue)
  {
  }

  // This is protected as it may not be the most appropriate accessor for a
  // given instance to expose. It's up to each individual property to provide
  // public accessors that forward to this as required.
  float ToFloat() const { return mValue * kInverseScale; }
  int ToIntRounded() const { return (mValue + kPointFive) >> FractionBits; }

  static constexpr float kScale = float(1u << FractionBits);
  static constexpr float kInverseScale = 1.0f / kScale;
  static constexpr float kMin = float(Min);
  static constexpr float kMax = float(Max);
  static const unsigned kFractionBits = FractionBits;

  // Constant representing 0.5 in the internal representation (note this
  // assumes that kFractionBits is greater than zero!)
  static const T kPointFive = 1u << (kFractionBits - 1);

  T mValue;
};

/**
 * font-weight: range 1..1000, fractional values permitted; keywords
 * 'normal', 'bold' aliased to 400, 700 respectively; relative keywords
 * 'lighter', 'bolder' (not currently handled here).
 *
 * We use an unsigned 10.6 fixed-point value (range 0.0 - 1023.984375)
 */
class FontWeight final : public FontPropertyValue<uint16_t,6,1,1000>
{
public:
  // See comment in FontPropertyValue regarding requirement for a trivial
  // default constructor.
  FontWeight() = default;

  explicit FontWeight(float aValue)
    : FontPropertyValue(aValue)
  {
  }

  /**
   * CSS font weights can have fractional values, but this constructor exists
   * for convenience when writing constants such as FontWeight(700) in code.
   */
  explicit FontWeight(int aValue)
    : FontPropertyValue(aValue)
  {
  }

  static FontWeight Normal()
  {
    return FontWeight(kNormal);
  }

  static FontWeight Thin()
  {
    return FontWeight(kThin);
  }

  static FontWeight Bold()
  {
    return FontWeight(kBold);
  }

  bool IsNormal() const { return mValue == kNormal; }
  bool IsBold() const { return mValue >= kBoldThreshold; }

  float ToFloat() const { return FontPropertyValue::ToFloat(); }
  int ToIntRounded() const { return FontPropertyValue::ToIntRounded(); }

private:
  explicit FontWeight(internal_type aValue)
    : FontPropertyValue(aValue)
  {
  }

  static const internal_type kNormal        = 400u << kFractionBits;
  static const internal_type kBold          = 700u << kFractionBits;
  static const internal_type kBoldThreshold = 600u << kFractionBits;
  static const internal_type kThin          = 100u << kFractionBits;
  static const internal_type kExtraBold     = 900u << kFractionBits;
};

/**
 * font-stretch is represented as a percentage relative to 'normal'.
 *
 * css-fonts says the value must be >= 0%, and normal is 100%. Keywords
 * from ultra-condensed to ultra-expanded are aliased to percentages
 * from 50% to 200%; values outside that range are unlikely to be common,
 * but could occur.
 *
 * Like font-weight, we use an unsigned 10.6 fixed-point value (range
 * 0.0 - 1023.984375).
 *
 * We arbitrarily limit here to 1000%. (If that becomes a problem, we
 * could reduce the number of fractional bits and increase the limit.)
 */
class FontStretch final : public FontPropertyValue<uint16_t,6,0,1000>
{
public:
  // See comment in FontPropertyValue regarding requirement for a trivial
  // default constructor.
  FontStretch() = default;

  explicit FontStretch(float aPercent)
    : FontPropertyValue(aPercent)
  {
  }

  static FontStretch Normal()
  {
    return FontStretch(kNormal);
  }
  static FontStretch UltraCondensed()
  {
    return FontStretch(kUltraCondensed);
  }
  static FontStretch ExtraCondensed()
  {
    return FontStretch(kExtraCondensed);
  }
  static FontStretch Condensed()
  {
    return FontStretch(kCondensed);
  }
  static FontStretch SemiCondensed()
  {
    return FontStretch(kSemiCondensed);
  }
  static FontStretch SemiExpanded()
  {
    return FontStretch(kSemiExpanded);
  }
  static FontStretch Expanded()
  {
    return FontStretch(kExpanded);
  }
  static FontStretch ExtraExpanded()
  {
    return FontStretch(kExtraExpanded);
  }
  static FontStretch UltraExpanded()
  {
    return FontStretch(kUltraExpanded);
  }

  bool IsNormal() const { return mValue == kNormal; }
  float Percentage() const { return ToFloat(); }

private:
  static const internal_type kUltraCondensed =  50u << kFractionBits;
  static const internal_type kExtraCondensed = (62u << kFractionBits) + kPointFive;
  static const internal_type kCondensed      =  75u << kFractionBits;
  static const internal_type kSemiCondensed  = (87u << kFractionBits) + kPointFive;
  static const internal_type kNormal         = 100u << kFractionBits;
  static const internal_type kSemiExpanded  = (112u << kFractionBits) + kPointFive;
  static const internal_type kExpanded       = 125u << kFractionBits;
  static const internal_type kExtraExpanded  = 150u << kFractionBits;
  static const internal_type kUltraExpanded  = 200u << kFractionBits;
};

/**
 * font-style: normal | italic | oblique <angle>?
 * values of <angle> below -90 or above 90 not permitted
 * - Use a signed 8.8 fixed-point value
 *   (representable range -128.0 - 127.99609375)
 * - Define min value (-128.0) as meaning 'normal'
 * - Define max value (127.99609375) as 'italic'
 * - Other values represent 'oblique <angle>'
 * - Note that 'oblique 0deg' is distinct from 'normal' (should it be?)
 */
class FontStyle final : public FontPropertyValue<int16_t,8,-90,90>
{
public:
  // See comment in FontPropertyValue regarding requirement for a trivial
  // default constructor.
  FontStyle() = default;

  static FontStyle Normal()
  {
    return FontStyle(kNormal);
  }

  static FontStyle Italic()
  {
    return FontStyle(kItalic);
  }

  static FontStyle Oblique(float aAngle = 14.0f)
  {
    return FontStyle(aAngle);
  }

  bool IsNormal() const { return mValue == kNormal; }
  bool IsItalic() const { return mValue == kItalic; }
  bool IsOblique() const { return mValue != kItalic && mValue != kNormal; }

  float ObliqueAngle() const
  {
    // It's not meaningful to get the oblique angle from a style that is
    // actually 'normal' or 'italic'.
    MOZ_ASSERT(!IsItalic() && !IsNormal());
    return ToFloat();
  }

private:
  explicit FontStyle(float aAngle)
    : FontPropertyValue(aAngle)
  {
  }

  static const int16_t kNormal = INT16_MIN;
  static const int16_t kItalic = INT16_MAX;
};

} // namespace mozilla

#endif // GFX_FONT_PROPERTY_TYPES_H

