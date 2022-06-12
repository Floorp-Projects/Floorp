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
#include "mozilla/ServoStyleConsts.h"
#include "nsString.h"

/*
 * This file is separate from gfxFont.h so that layout can include it
 * without bringing in gfxFont.h and everything it includes.
 */

namespace mozilla {

using FontSlantStyle = StyleFontStyle;
using FontWeight = StyleFontWeight;
using FontStretch = StyleFontStretch;

/**
 * Convenience type to hold a <min, max> pair representing a range of values.
 *
 * The min and max are both inclusive, so when min == max the range represents
 * a single value (not an empty range).
 */
template <class T, class Derived>
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
  using ScalarType = uint32_t;

  ScalarType AsScalar() const {
    return (mValues.first.UnsignedRaw() << 16) | mValues.second.UnsignedRaw();
  }

  static Derived FromScalar(ScalarType aScalar) {
    static_assert(std::is_base_of_v<FontPropertyRange, Derived>);
    return Derived(T::FromRaw(aScalar >> 16), T::FromRaw(aScalar & 0xffff));
  }

 protected:
  std::pair<T, T> mValues;
};

class WeightRange : public FontPropertyRange<FontWeight, WeightRange> {
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
};

class StretchRange : public FontPropertyRange<FontStretch, StretchRange> {
 public:
  StretchRange(FontStretch aMin, FontStretch aMax)
      : FontPropertyRange(aMin, aMax) {}

  explicit StretchRange(FontStretch aStretch) : FontPropertyRange(aStretch) {}

  StretchRange(const StretchRange& aOther) = default;

  void ToString(nsACString& aOutString, const char* aDelim = "..") const {
    aOutString.AppendFloat(Min().ToFloat());
    if (!IsSingle()) {
      aOutString.Append(aDelim);
      aOutString.AppendFloat(Max().ToFloat());
    }
  }
};

class SlantStyleRange
    : public FontPropertyRange<FontSlantStyle, SlantStyleRange> {
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
};

}  // namespace mozilla

#endif  // GFX_FONT_PROPERTY_TYPES_H
