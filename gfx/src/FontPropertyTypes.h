/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* font specific types shared by both thebes and layout */

#ifndef GFX_FONT_PROPERTY_TYPES_H
#define GFX_FONT_PROPERTY_TYPES_H

#include <cstdint>

/*
 * This file is separate from gfxFont.h so that layout can include it
 * without bringing in gfxFont.h and everything it includes.
 */

namespace mozilla {

/**
 * A type that will in future encode a value as fixed point.
 */
class FontFixedPointValue
{
public:
  // Ugh. We need a default constructor to allow this type to be used in the
  // union in nsCSSValue.  Furthermore we need the default and copy
  // constructors to be "trivial" (i.e. the compiler implemented defaults that
  // do no initialization).
  // Annoyingly it seems we can't make the default implementations constexpr
  // (at least in clang).  We'd like to do that to allow Thin() et. al. below
  // to also be constexpr. :/
  FontFixedPointValue() = default;
  FontFixedPointValue(const FontFixedPointValue& aOther) = default;

  // Not currently encoded, but it will be in future
  explicit FontFixedPointValue(int16_t aValue)
    : mEncoded(aValue)
  {}

  explicit FontFixedPointValue(int32_t aValue)
    : mEncoded(aValue)
  {}

  explicit FontFixedPointValue(float aValue)
    : mEncoded(int16_t(aValue))
  {}

  float ToFloat() const {
    return float(mEncoded);
  }

  int16_t ToIntRounded() const {
    return mEncoded;
  }

  bool operator==(const FontFixedPointValue& aOther) const {
    return mEncoded == aOther.mEncoded;
  }

  bool operator!=(const FontFixedPointValue& aOther) const {
    return mEncoded != aOther.mEncoded;
  }

  bool operator<(const FontFixedPointValue& aOther) const {
    return mEncoded < aOther.mEncoded;
  }

  bool operator<=(const FontFixedPointValue& aOther) const {
    return mEncoded <= aOther.mEncoded;
  }

  bool operator>=(const FontFixedPointValue& aOther) const {
    return mEncoded >= aOther.mEncoded;
  }

  bool operator>(const FontFixedPointValue& aOther) const {
    return mEncoded > aOther.mEncoded;
  }

  int16_t ForHash() const {
    return mEncoded;
  }

protected:
  int16_t mEncoded;
};

class FontWeight : public FontFixedPointValue
{
public:
  // Ugh, to get union in nsCSSValue compiling
  FontWeight() = default;
  FontWeight(const FontWeight& aOther) = default;

    // Not currently encoded, but it will be in future
  explicit FontWeight(int16_t aValue)
    : FontFixedPointValue(aValue)
  {}

  explicit FontWeight(int32_t aValue)
    : FontFixedPointValue(aValue)
  {}

  explicit FontWeight(float aValue)
    : FontFixedPointValue(int16_t(aValue))
  {}

  bool operator==(const FontWeight& aOther) const
  {
    return mEncoded == aOther.mEncoded;
  }

  /// The "distance" between two font weights
  float operator-(const FontWeight& aOther) const {
    return this->ToFloat() - aOther.ToFloat();
  }

  static FontWeight Thin() {
    return FontWeight{100};
  }
  static FontWeight Normal() {
    return FontWeight{400};
  }
  static FontWeight Bold() {
    return FontWeight{700};
  }
};

} // namespace mozilla

#endif // GFX_FONT_PROPERTY_TYPES_H

