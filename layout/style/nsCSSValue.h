/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of simple property values within CSS declarations */

#ifndef nsCSSValue_h___
#define nsCSSValue_h___

#include "mozilla/Attributes.h"
#include "mozilla/CORSMode.h"
#include "mozilla/EnumTypeTraits.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ServoBindingTypes.h"
#include "mozilla/URLExtraData.h"
#include "mozilla/UniquePtr.h"

#include "nsCSSKeywords.h"
#include "nsCSSPropertyID.h"
#include "nsCoord.h"
#include "nsProxyRelease.h"
#include "nsRefPtrHashtable.h"
#include "nsString.h"
#include "nsStringBuffer.h"
#include "nsTArray.h"
#include "nsStyleConsts.h"
#include "gfxFontFamilyList.h"

#include <type_traits>

class imgRequestProxy;
class nsAtom;
class nsIContent;

class nsIPrincipal;
class nsIURI;
class nsPresContext;
template <class T>
class nsPtrHashKey;
struct RawServoCssUrlData;

namespace mozilla {
class CSSStyleSheet;
}  // namespace mozilla

// Forward declaration copied here since ServoBindings.h #includes nsCSSValue.h.
extern "C" {
mozilla::URLExtraData* Servo_CssUrlData_GetExtraData(const RawServoCssUrlData*);
bool Servo_CssUrlData_IsLocalRef(const RawServoCssUrlData* url);
}

enum nsCSSUnit : uint32_t {
  eCSSUnit_Null = 0,  // (n/a) null unit, value is not specified

  eCSSUnit_Integer = 70,     // (int) simple value
  eCSSUnit_Enumerated = 71,  // (int) value has enumerated meaning

  eCSSUnit_Percent = 100,  // (float) (1.0 == 100%) value is percentage of
                           // something
  eCSSUnit_Number = 101,   // (float) value is numeric (usually multiplier,
                           // different behavior than percent)

  // Font relative measure
  eCSSUnit_EM = 800,       // (float) == current font size
  eCSSUnit_XHeight = 801,  // (float) distance from top of lower case x to
                           // baseline
  eCSSUnit_Char = 802,     // (float) number of characters, used for width with
                           // monospace font
  eCSSUnit_RootEM = 803,   // (float) == root element font size

  // Screen relative measure
  eCSSUnit_Point = 900,       // (float) 4/3 of a CSS pixel
  eCSSUnit_Inch = 901,        // (float) 96 CSS pixels
  eCSSUnit_Millimeter = 902,  // (float) 96/25.4 CSS pixels
  eCSSUnit_Centimeter = 903,  // (float) 96/2.54 CSS pixels
  eCSSUnit_Pica = 904,        // (float) 12 points == 16 CSS pixls
  eCSSUnit_Quarter = 905,     // (float) 96/101.6 CSS pixels
  eCSSUnit_Pixel = 906,       // (float) CSS pixel unit

  // Angular units
  eCSSUnit_Degree = 1000,  // (float) 360 per circle

  // Frequency units
  eCSSUnit_Hertz = 2000,      // (float) 1/seconds
  eCSSUnit_Kilohertz = 2001,  // (float) 1000 Hertz

  // Time units
  eCSSUnit_Seconds = 3000,       // (float) Standard time
  eCSSUnit_Milliseconds = 3001,  // (float) 1/1000 second

  // Flexible fraction (CSS Grid)
  eCSSUnit_FlexFraction = 4000,  // (float) Fraction of free space
};

struct nsCSSValuePair;
struct nsCSSValuePair_heap;
struct nsCSSValueList;
struct nsCSSValueList_heap;
struct nsCSSValueSharedList;
struct nsCSSValuePairList;
struct nsCSSValuePairList_heap;

class nsCSSValue {
 public:
  explicit nsCSSValue() : mUnit(eCSSUnit_Null) {}

  nsCSSValue(int32_t aValue, nsCSSUnit aUnit);
  nsCSSValue(float aValue, nsCSSUnit aUnit);
  nsCSSValue(const nsCSSValue& aCopy);
  nsCSSValue(nsCSSValue&& aOther) : mUnit(aOther.mUnit), mValue(aOther.mValue) {
    aOther.mUnit = eCSSUnit_Null;
  }
  template <typename T,
            typename = typename std::enable_if<std::is_enum<T>::value>::type>
  explicit nsCSSValue(T aValue) : mUnit(eCSSUnit_Enumerated) {
    static_assert(mozilla::EnumTypeFitsWithin<T, int32_t>::value,
                  "aValue must be an enum that fits within mValue.mInt");
    mValue.mInt = static_cast<int32_t>(aValue);
  }

  nsCSSValue& operator=(const nsCSSValue& aCopy);
  nsCSSValue& operator=(nsCSSValue&& aCopy);
  bool operator==(const nsCSSValue& aOther) const;

  bool operator!=(const nsCSSValue& aOther) const { return !(*this == aOther); }

  nsCSSUnit GetUnit() const { return mUnit; }
  bool IsLengthUnit() const {
    return eCSSUnit_EM <= mUnit && mUnit <= eCSSUnit_Pixel;
  }
  bool IsLengthPercentUnit() const {
    return IsLengthUnit() || mUnit == eCSSUnit_Percent;
  }
  /**
   * What the spec calls relative length units is, for us, split
   * between relative length units and pixel length units.
   *
   * A "relative" length unit is a multiple of some derived metric,
   * such as a font em-size, which itself was controlled by an input CSS
   * length. Relative length units should not be scaled by zooming, since
   * the underlying CSS length would already have been scaled.
   */
  bool IsRelativeLengthUnit() const {
    return eCSSUnit_EM <= mUnit && mUnit <= eCSSUnit_RootEM;
  }
  /**
   * A "pixel" length unit is a some multiple of CSS pixels.
   */
  static bool IsPixelLengthUnit(nsCSSUnit aUnit) {
    return eCSSUnit_Point <= aUnit && aUnit <= eCSSUnit_Pixel;
  }
  bool IsPixelLengthUnit() const { return IsPixelLengthUnit(mUnit); }
  static bool IsPercentLengthUnit(nsCSSUnit aUnit) {
    return aUnit == eCSSUnit_Percent;
  }
  bool IsPercentLengthUnit() { return IsPercentLengthUnit(mUnit); }
  static bool IsFloatUnit(nsCSSUnit aUnit) { return eCSSUnit_Number <= aUnit; }
  bool IsAngularUnit() const { return eCSSUnit_Degree == mUnit; }
  bool IsFrequencyUnit() const {
    return eCSSUnit_Hertz <= mUnit && mUnit <= eCSSUnit_Kilohertz;
  }
  bool IsTimeUnit() const {
    return eCSSUnit_Seconds <= mUnit && mUnit <= eCSSUnit_Milliseconds;
  }

  int32_t GetIntValue() const {
    MOZ_ASSERT(mUnit == eCSSUnit_Integer || mUnit == eCSSUnit_Enumerated,
               "not an int value");
    return mValue.mInt;
  }

  nsCSSKeyword GetKeywordValue() const {
    MOZ_ASSERT(mUnit == eCSSUnit_Enumerated, "not a keyword value");
    return static_cast<nsCSSKeyword>(mValue.mInt);
  }

  float GetPercentValue() const {
    MOZ_ASSERT(mUnit == eCSSUnit_Percent, "not a percent value");
    return mValue.mFloat;
  }

  float GetFloatValue() const {
    MOZ_ASSERT(eCSSUnit_Number <= mUnit, "not a float value");
    MOZ_ASSERT(!mozilla::IsNaN(mValue.mFloat));
    return mValue.mFloat;
  }

  float GetAngleValue() const {
    MOZ_ASSERT(eCSSUnit_Degree == mUnit, "not an angle value");
    return mValue.mFloat;
  }

  // Converts any angle to radians.
  double GetAngleValueInRadians() const;

  // Converts any angle to degrees.
  double GetAngleValueInDegrees() const;

  nscoord GetPixelLength() const;

  void Reset() { mUnit = eCSSUnit_Null; }
  ~nsCSSValue() { Reset(); }

 public:
  void SetIntValue(int32_t aValue, nsCSSUnit aUnit);
  template <typename T,
            typename = typename std::enable_if<std::is_enum<T>::value>::type>
  void SetEnumValue(T aValue) {
    static_assert(mozilla::EnumTypeFitsWithin<T, int32_t>::value,
                  "aValue must be an enum that fits within mValue.mInt");
    SetIntValue(static_cast<int32_t>(aValue), eCSSUnit_Enumerated);
  }
  void SetPercentValue(float aValue);
  void SetFloatValue(float aValue, nsCSSUnit aUnit);
  // converts the nscoord to pixels
  void SetIntegerCoordValue(nscoord aCoord);

 protected:
  nsCSSUnit mUnit;
  union {
    int32_t mInt;
    float mFloat;
  } mValue;
};

#endif /* nsCSSValue_h___ */
