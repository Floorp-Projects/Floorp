/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of simple property values within CSS declarations */

#include "nsCSSValue.h"

#include "mozilla/CORSMode.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/ServoTypes.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/Likely.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/css/ImageLoader.h"
#include "gfxFontConstants.h"
#include "imgRequestProxy.h"
#include "mozilla/dom/Document.h"
#include "nsCSSProps.h"
#include "nsNetUtil.h"
#include "nsPresContext.h"
#include "nsStyleUtil.h"
#include "nsDeviceContext.h"
#include "nsContentUtils.h"

using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::dom;

nsCSSValue::nsCSSValue(int32_t aValue, nsCSSUnit aUnit) : mUnit(aUnit) {
  MOZ_ASSERT(aUnit == eCSSUnit_Integer || aUnit == eCSSUnit_Enumerated,
             "not an int value");
  if (aUnit == eCSSUnit_Integer || aUnit == eCSSUnit_Enumerated) {
    mValue.mInt = aValue;
  } else {
    mUnit = eCSSUnit_Null;
    mValue.mInt = 0;
  }
}

nsCSSValue::nsCSSValue(float aValue, nsCSSUnit aUnit) : mUnit(aUnit) {
  MOZ_ASSERT(eCSSUnit_Percent <= aUnit, "not a float value");
  if (eCSSUnit_Percent <= aUnit) {
    mValue.mFloat = aValue;
    MOZ_ASSERT(!mozilla::IsNaN(mValue.mFloat));
  } else {
    mUnit = eCSSUnit_Null;
    mValue.mInt = 0;
  }
}

nsCSSValue::nsCSSValue(const nsCSSValue& aCopy) : mUnit(aCopy.mUnit) {
  if (eCSSUnit_Percent <= mUnit) {
    mValue.mFloat = aCopy.mValue.mFloat;
    MOZ_ASSERT(!mozilla::IsNaN(mValue.mFloat));
  } else if (eCSSUnit_Integer <= mUnit && mUnit <= eCSSUnit_Enumerated) {
    mValue.mInt = aCopy.mValue.mInt;
  } else {
    MOZ_ASSERT_UNREACHABLE("unknown unit");
  }
}

nsCSSValue& nsCSSValue::operator=(const nsCSSValue& aCopy) {
  if (this != &aCopy) {
    this->~nsCSSValue();
    new (this) nsCSSValue(aCopy);
  }
  return *this;
}

nsCSSValue& nsCSSValue::operator=(nsCSSValue&& aOther) {
  MOZ_ASSERT(this != &aOther, "Self assigment with rvalue reference");

  Reset();
  mUnit = aOther.mUnit;
  mValue = aOther.mValue;
  aOther.mUnit = eCSSUnit_Null;

  return *this;
}

bool nsCSSValue::operator==(const nsCSSValue& aOther) const {
  if (mUnit != aOther.mUnit) {
    return false;
  }
  if ((eCSSUnit_Integer <= mUnit) && (mUnit <= eCSSUnit_Enumerated)) {
    return mValue.mInt == aOther.mValue.mInt;
  }
  return mValue.mFloat == aOther.mValue.mFloat;
}

double nsCSSValue::GetAngleValueInDegrees() const {
  // Note that this extends the value from float to double.
  return GetAngleValue();
}

double nsCSSValue::GetAngleValueInRadians() const {
  return GetAngleValueInDegrees() * M_PI / 180.0;
}

nscoord nsCSSValue::GetPixelLength() const {
  MOZ_ASSERT(IsPixelLengthUnit(), "not a fixed length unit");

  double scaleFactor;
  switch (mUnit) {
    case eCSSUnit_Pixel:
      return nsPresContext::CSSPixelsToAppUnits(mValue.mFloat);
    case eCSSUnit_Pica:
      scaleFactor = 16.0;
      break;
    case eCSSUnit_Point:
      scaleFactor = 4 / 3.0;
      break;
    case eCSSUnit_Inch:
      scaleFactor = 96.0;
      break;
    case eCSSUnit_Millimeter:
      scaleFactor = 96 / 25.4;
      break;
    case eCSSUnit_Centimeter:
      scaleFactor = 96 / 2.54;
      break;
    case eCSSUnit_Quarter:
      scaleFactor = 96 / 101.6;
      break;
    default:
      NS_ERROR("should never get here");
      return 0;
  }
  return nsPresContext::CSSPixelsToAppUnits(float(mValue.mFloat * scaleFactor));
}

void nsCSSValue::SetIntValue(int32_t aValue, nsCSSUnit aUnit) {
  MOZ_ASSERT(aUnit == eCSSUnit_Integer || aUnit == eCSSUnit_Enumerated,
             "not an int value");
  Reset();
  if (aUnit == eCSSUnit_Integer || aUnit == eCSSUnit_Enumerated) {
    mUnit = aUnit;
    mValue.mInt = aValue;
  }
}

void nsCSSValue::SetPercentValue(float aValue) {
  Reset();
  mUnit = eCSSUnit_Percent;
  mValue.mFloat = aValue;
  MOZ_ASSERT(!mozilla::IsNaN(mValue.mFloat));
}

void nsCSSValue::SetFloatValue(float aValue, nsCSSUnit aUnit) {
  MOZ_ASSERT(IsFloatUnit(aUnit), "not a float value");
  Reset();
  if (IsFloatUnit(aUnit)) {
    mUnit = aUnit;
    mValue.mFloat = aValue;
    MOZ_ASSERT(!mozilla::IsNaN(mValue.mFloat));
  }
}
