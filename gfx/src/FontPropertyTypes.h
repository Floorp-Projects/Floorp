/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* font specific types shared by both thebes and layout */

#ifndef GFX_FONT_PROPERTY_TYPES_H
#define GFX_FONT_PROPERTY_TYPES_H

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <utility>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "mozilla/Assertions.h"
#include "mozilla/TextUtils.h"
#include "nsString.h"

/*
 * This file is separate from gfxFont.h so that layout can include it
 * without bringing in gfxFont.h and everything it includes.
 */

namespace mozilla {

/**
 * Generic template for font property type classes that use a fixed-point
 * internal representation.
 * Template parameters:
 *   InternalType - the integer type to use as the internal representation (e.g.
 *                  uint16_t)
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
template <class InternalType, unsigned FractionBits, int Min, int Max>
class FontPropertyValue {
 public:
  // Initialize to the minimum value by default.
  constexpr FontPropertyValue() : FontPropertyValue(Min) {}

  explicit FontPropertyValue(const FontPropertyValue& aOther) = default;
  FontPropertyValue& operator=(const FontPropertyValue& aOther) = default;

  bool operator==(const FontPropertyValue& aOther) const {
    return mValue == aOther.mValue;
  }
  bool operator!=(const FontPropertyValue& aOther) const {
    return mValue != aOther.mValue;
  }
  bool operator<(const FontPropertyValue& aOther) const {
    return mValue < aOther.mValue;
  }
  bool operator>(const FontPropertyValue& aOther) const {
    return mValue > aOther.mValue;
  }
  bool operator<=(const FontPropertyValue& aOther) const {
    return mValue <= aOther.mValue;
  }
  bool operator>=(const FontPropertyValue& aOther) const {
    return mValue >= aOther.mValue;
  }

  // The difference between two values, returned as a raw floating-point number
  // (which might not be a valid property value in its own right).
  float operator-(const FontPropertyValue& aOther) const {
    return (mValue - aOther.mValue) * kInverseScale;
  }

  /// Return the raw internal representation, for purposes of hashing.
  /// (Do not try to interpret the numeric value of this.)
  uint16_t ForHash() const { return uint16_t(mValue); }

  static constexpr const float kMin = float(Min);
  static constexpr const float kMax = float(Max);

 protected:
  // Construct from a floating-point or integer value, checking that it is
  // within the allowed range and converting to fixed-point representation.
  explicit constexpr FontPropertyValue(float aValue)
      : mValue(std::round(aValue * kScale)) {
    MOZ_ASSERT(aValue >= kMin && aValue <= kMax);
  }
  explicit constexpr FontPropertyValue(int aValue)
      : mValue(static_cast<InternalType>(aValue * kScale)) {
    MOZ_ASSERT(aValue >= Min && aValue <= Max);
  }

  // Construct directly from a fixed-point value of type T, with no check;
  // note that there may be special "flag" values that are outside the normal
  // min/max range (e.g. for font-style:italic, distinct from oblique angle).
  explicit constexpr FontPropertyValue(InternalType aValue) : mValue(aValue) {}

  // This is protected as it may not be the most appropriate accessor for a
  // given instance to expose. It's up to each individual property to provide
  // public accessors that forward to this as required.
  float ToFloat() const { return mValue * kInverseScale; }
  int ToIntRounded() const { return (mValue + kPointFive) >> FractionBits; }

  static constexpr int kScale = 1 << FractionBits;
  static constexpr float kInverseScale = 1.0f / kScale;
  static const unsigned kFractionBits = FractionBits;

  // Constant representing 0.5 in the internal representation (note this
  // assumes that kFractionBits is greater than zero!)
  static const InternalType kPointFive = 1u << (kFractionBits - 1);

  InternalType mValue;
};

/**
 * font-weight: range 1..1000, fractional values permitted; keywords
 * 'normal', 'bold' aliased to 400, 700 respectively; relative keywords
 * 'lighter', 'bolder' (not currently handled here).
 *
 * We use an unsigned 10.6 fixed-point value (range 0.0 - 1023.984375)
 */
class FontWeight final : public FontPropertyValue<uint16_t, 6, 1, 1000> {
 public:
  constexpr FontWeight() = default;

  explicit constexpr FontWeight(float aValue) : FontPropertyValue(aValue) {}

  /**
   * CSS font weights can have fractional values, but this constructor exists
   * for convenience when writing constants such as FontWeight(700) in code.
   */
  explicit constexpr FontWeight(int aValue) : FontPropertyValue(aValue) {}

  static constexpr FontWeight Normal() { return FontWeight(kNormal); }

  static constexpr FontWeight Thin() { return FontWeight(kThin); }

  static constexpr FontWeight Bold() { return FontWeight(kBold); }

  bool IsNormal() const { return mValue == kNormal; }
  bool IsBold() const { return mValue >= kBoldThreshold; }

  float ToFloat() const { return FontPropertyValue::ToFloat(); }
  int ToIntRounded() const { return FontPropertyValue::ToIntRounded(); }

  typedef uint16_t InternalType;

 private:
  friend class WeightRange;

  explicit constexpr FontWeight(InternalType aValue)
      : FontPropertyValue(aValue) {}

  static const InternalType kNormal = 400u << kFractionBits;
  static const InternalType kBold = 700u << kFractionBits;
  static const InternalType kBoldThreshold = 600u << kFractionBits;
  static const InternalType kThin = 100u << kFractionBits;
  static const InternalType kExtraBold = 900u << kFractionBits;
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
class FontStretch final : public FontPropertyValue<uint16_t, 6, 0, 1000> {
 public:
  constexpr FontStretch() = default;

  explicit constexpr FontStretch(float aPercent)
      : FontPropertyValue(aPercent) {}

  static constexpr FontStretch Normal() { return FontStretch(kNormal); }
  static constexpr FontStretch UltraCondensed() {
    return FontStretch(kUltraCondensed);
  }
  static constexpr FontStretch ExtraCondensed() {
    return FontStretch(kExtraCondensed);
  }
  static constexpr FontStretch Condensed() { return FontStretch(kCondensed); }
  static constexpr FontStretch SemiCondensed() {
    return FontStretch(kSemiCondensed);
  }
  static constexpr FontStretch SemiExpanded() {
    return FontStretch(kSemiExpanded);
  }
  static constexpr FontStretch Expanded() { return FontStretch(kExpanded); }
  static constexpr FontStretch ExtraExpanded() {
    return FontStretch(kExtraExpanded);
  }
  static constexpr FontStretch UltraExpanded() {
    return FontStretch(kUltraExpanded);
  }

  // The style system represents percentages in the 0.0..1.0 range, and
  // FontStretch does it in the 0.0..100.0 range.
  //
  // TODO(emilio): We should consider changing this class to deal with the same
  // range as the style system.
  static FontStretch FromStyle(float aStylePercentage) {
    return FontStretch(std::min(aStylePercentage * 100.0f, float(kMax)));
  }

  bool IsNormal() const { return mValue == kNormal; }
  float Percentage() const { return ToFloat(); }

  typedef uint16_t InternalType;

 private:
  friend class StretchRange;

  explicit constexpr FontStretch(InternalType aValue)
      : FontPropertyValue(aValue) {}

  static const InternalType kUltraCondensed = 50u << kFractionBits;
  static const InternalType kExtraCondensed =
      (62u << kFractionBits) + kPointFive;
  static const InternalType kCondensed = 75u << kFractionBits;
  static const InternalType kSemiCondensed =
      (87u << kFractionBits) + kPointFive;
  static const InternalType kNormal = 100u << kFractionBits;
  static const InternalType kSemiExpanded =
      (112u << kFractionBits) + kPointFive;
  static const InternalType kExpanded = 125u << kFractionBits;
  static const InternalType kExtraExpanded = 150u << kFractionBits;
  static const InternalType kUltraExpanded = 200u << kFractionBits;
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
class FontSlantStyle final : public FontPropertyValue<int16_t, 8, -90, 90> {
 public:
  const static constexpr float kDefaultAngle = 14.0;

  constexpr FontSlantStyle() = default;

  static constexpr FontSlantStyle Normal() { return FontSlantStyle(kNormal); }

  static constexpr FontSlantStyle Italic() { return FontSlantStyle(kItalic); }

  static constexpr FontSlantStyle Oblique(float aAngle = kDefaultAngle) {
    return FontSlantStyle(aAngle);
  }

  // Create from a string as generated by ToString. This is for internal use
  // when serializing/deserializing entries for the startupcache, and is not
  // intended to parse arbitrary (untrusted) strings.
  static FontSlantStyle FromString(const char* aString) {
    if (strcmp(aString, "normal") == 0) {
      return Normal();
    }
    if (strcmp(aString, "italic") == 0) {
      return Italic();
    }
    if (mozilla::IsAsciiDigit(aString[0]) && strstr(aString, "deg")) {
      float angle = strtof(aString, nullptr);
      return Oblique(angle);
    }
    // Not recognized as an oblique angle; maybe it's from a startup-cache
    // created by an older version. The style field there used a simple 0/1
    // for normal/italic respectively.
    return aString[0] == '0' ? Normal() : Italic();
  }

  bool IsNormal() const { return mValue == kNormal; }
  bool IsItalic() const { return mValue == kItalic; }
  bool IsOblique() const { return mValue != kItalic && mValue != kNormal; }

  float ObliqueAngle() const {
    // It's not meaningful to get the oblique angle from a style that is
    // actually 'normal' or 'italic'.
    MOZ_ASSERT(IsOblique());
    return ToFloat();
  }

  /**
   * Write a string representation of the value to aOutString.
   *
   * NOTE that this APPENDS to the output string, it does not replace
   * any existing contents.
   */
  void ToString(nsACString& aOutString) const {
    if (IsNormal()) {
      aOutString.Append("normal");
    } else if (IsItalic()) {
      aOutString.Append("italic");
    } else {
      aOutString.AppendPrintf("%gdeg", ObliqueAngle());
    }
  }

  typedef int16_t InternalType;

 private:
  friend class SlantStyleRange;

  explicit constexpr FontSlantStyle(InternalType aConstant)
      : FontPropertyValue(aConstant) {}

  explicit constexpr FontSlantStyle(float aAngle) : FontPropertyValue(aAngle) {}

  static const InternalType kNormal = INT16_MIN;
  static const InternalType kItalic = INT16_MAX;
};

/**
 * Convenience type to hold a <min, max> pair representing a range of values.
 *
 * The min and max are both inclusive, so when min == max the range represents
 * a single value (not an empty range).
 */
template <class T>
class FontPropertyRange {
  // This implementation assumes the underlying property type is a 16-bit value
  // (see FromScalar and AsScalar below).
  static_assert(sizeof(T) == 2, "FontPropertyValue should be a 16-bit type!");

 public:
  /**
   * Construct a range from given minimum and maximum values (inclusive).
   */
  FontPropertyRange(T aMin, T aMax) : mValues(aMin, aMax) {
    MOZ_ASSERT(aMin <= aMax);
  }

  /**
   * Construct a range representing a single value (min==max).
   */
  explicit FontPropertyRange(T aValue) : mValues(aValue, aValue) {}

  explicit FontPropertyRange(const FontPropertyRange& aOther) = default;
  FontPropertyRange& operator=(const FontPropertyRange& aOther) = default;

  T Min() const { return mValues.first; }
  T Max() const { return mValues.second; }

  /**
   * Clamp the given value to this range.
   *
   * (We can't use mozilla::Clamp here because it only accepts integral types.)
   */
  T Clamp(T aValue) const {
    return aValue <= Min() ? Min() : (aValue >= Max() ? Max() : aValue);
  }

  /**
   * Return whether the range consists of a single unique value.
   */
  bool IsSingle() const { return Min() == Max(); }

  bool operator==(const FontPropertyRange& aOther) const {
    return mValues == aOther.mValues;
  }
  bool operator!=(const FontPropertyRange& aOther) const {
    return mValues != aOther.mValues;
  }

  /**
   * Conversion of the property range to/from a single 32-bit scalar value,
   * suitable for IPC serialization, hashing, caching.
   *
   * No assumptions should be made about the numeric value of the scalar.
   *
   * This depends on the underlying property type being a 16-bit value!
   */
  typedef uint32_t ScalarType;

  ScalarType AsScalar() const {
    return (mValues.first.ForHash() << 16) | mValues.second.ForHash();
  }

  /*
   * FIXME:
   * FromScalar is defined in each individual subclass, because I can't
   * persuade the compiler to accept a definition here in the template. :\
   *
  static FontPropertyRange FromScalar(ScalarType aScalar)
  {
    return FontPropertyRange(T(typename T::InternalType(aScalar >> 16)),
                             T(typename T::InternalType(aScalar & 0xffff)));
  }
   */

 protected:
  std::pair<T, T> mValues;
};

class WeightRange : public FontPropertyRange<FontWeight> {
 public:
  WeightRange(FontWeight aMin, FontWeight aMax)
      : FontPropertyRange(aMin, aMax) {}

  explicit WeightRange(FontWeight aWeight) : FontPropertyRange(aWeight) {}

  WeightRange(const WeightRange& aOther) = default;

  void ToString(nsACString& aOutString, const char* aDelim = "..") const {
    aOutString.AppendFloat(Min().ToFloat());
    if (!IsSingle()) {
      aOutString.Append(aDelim);
      aOutString.AppendFloat(Max().ToFloat());
    }
  }

  static WeightRange FromScalar(ScalarType aScalar) {
    return WeightRange(FontWeight(FontWeight::InternalType(aScalar >> 16)),
                       FontWeight(FontWeight::InternalType(aScalar & 0xffff)));
  }
};

class StretchRange : public FontPropertyRange<FontStretch> {
 public:
  StretchRange(FontStretch aMin, FontStretch aMax)
      : FontPropertyRange(aMin, aMax) {}

  explicit StretchRange(FontStretch aStretch) : FontPropertyRange(aStretch) {}

  StretchRange(const StretchRange& aOther) = default;

  void ToString(nsACString& aOutString, const char* aDelim = "..") const {
    aOutString.AppendFloat(Min().Percentage());
    if (!IsSingle()) {
      aOutString.Append(aDelim);
      aOutString.AppendFloat(Max().Percentage());
    }
  }

  static StretchRange FromScalar(ScalarType aScalar) {
    return StretchRange(
        FontStretch(FontStretch::InternalType(aScalar >> 16)),
        FontStretch(FontStretch::InternalType(aScalar & 0xffff)));
  }
};

class SlantStyleRange : public FontPropertyRange<FontSlantStyle> {
 public:
  SlantStyleRange(FontSlantStyle aMin, FontSlantStyle aMax)
      : FontPropertyRange(aMin, aMax) {}

  explicit SlantStyleRange(FontSlantStyle aStyle) : FontPropertyRange(aStyle) {}

  SlantStyleRange(const SlantStyleRange& aOther) = default;

  void ToString(nsACString& aOutString, const char* aDelim = "..") const {
    Min().ToString(aOutString);
    if (!IsSingle()) {
      aOutString.Append(aDelim);
      Max().ToString(aOutString);
    }
  }

  static SlantStyleRange FromScalar(ScalarType aScalar) {
    return SlantStyleRange(
        FontSlantStyle(FontSlantStyle::InternalType(aScalar >> 16)),
        FontSlantStyle(FontSlantStyle::InternalType(aScalar & 0xffff)));
  }
};

}  // namespace mozilla

#endif  // GFX_FONT_PROPERTY_TYPES_H
