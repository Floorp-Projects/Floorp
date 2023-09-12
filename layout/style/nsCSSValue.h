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
#include "mozilla/MemoryReporting.h"
#include "mozilla/ServoBindingTypes.h"
#include "mozilla/URLExtraData.h"
#include "mozilla/UniquePtr.h"

#include "nsCoord.h"
#include "nsTArray.h"

#include <type_traits>

// XXX Avoid including this here by moving function bodies to the cpp file
#include "mozilla/FloatingPoint.h"

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

  eCSSUnit_Percent = 100,  // (1.0 == 100%) value is percentage of
                           // something
  eCSSUnit_Number = 101,   // value is numeric (usually multiplier,
                           // different behavior than percent)

  // Font relative measure
  eCSSUnit_EM = 800,           // == current font size
  eCSSUnit_XHeight = 801,      // distance from top of lower case x to
                               // baseline
  eCSSUnit_Char = 802,         // number of characters, used for width with
                               // monospace font
  eCSSUnit_RootEM = 803,       // == root element font size
  eCSSUnit_Ideographic = 804,  // == CJK water ideograph width
  eCSSUnit_CapHeight = 805,    // == Capital letter height

  // Screen relative measure
  eCSSUnit_Point = 900,       // 4/3 of a CSS pixel
  eCSSUnit_Inch = 901,        // 96 CSS pixels
  eCSSUnit_Millimeter = 902,  // 96/25.4 CSS pixels
  eCSSUnit_Centimeter = 903,  // 96/2.54 CSS pixels
  eCSSUnit_Pica = 904,        // 12 points == 16 CSS pixls
  eCSSUnit_Quarter = 905,     // 96/101.6 CSS pixels
  eCSSUnit_Pixel = 906,       // CSS pixel unit

  // Viewport percentage lengths
  eCSSUnit_VW = 950,
  eCSSUnit_VH = 951,
  eCSSUnit_VMin = 952,
  eCSSUnit_VMax = 953,
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

  nsCSSValue(float aValue, nsCSSUnit aUnit);
  nsCSSValue(const nsCSSValue& aCopy);
  nsCSSValue(nsCSSValue&& aOther) : mUnit(aOther.mUnit), mValue(aOther.mValue) {
    aOther.mUnit = eCSSUnit_Null;
  }

  nsCSSValue& operator=(const nsCSSValue& aCopy);
  nsCSSValue& operator=(nsCSSValue&& aCopy);
  bool operator==(const nsCSSValue& aOther) const;

  bool operator!=(const nsCSSValue& aOther) const { return !(*this == aOther); }

  nsCSSUnit GetUnit() const { return mUnit; }
  bool IsLengthUnit() const {
    return eCSSUnit_EM <= mUnit && mUnit <= eCSSUnit_Pixel;
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
  static bool IsFloatUnit(nsCSSUnit aUnit) { return eCSSUnit_Number <= aUnit; }

  float GetPercentValue() const {
    MOZ_ASSERT(mUnit == eCSSUnit_Percent, "not a percent value");
    return mValue;
  }

  float GetFloatValue() const {
    MOZ_ASSERT(eCSSUnit_Number <= mUnit, "not a float value");
    MOZ_ASSERT(!std::isnan(mValue));
    return mValue;
  }

  nscoord GetPixelLength() const;

  void Reset() { mUnit = eCSSUnit_Null; }
  ~nsCSSValue() { Reset(); }

 public:
  void SetPercentValue(float aValue);
  void SetFloatValue(float aValue, nsCSSUnit aUnit);

 protected:
  nsCSSUnit mUnit;
  float mValue;
};

#endif /* nsCSSValue_h___ */
