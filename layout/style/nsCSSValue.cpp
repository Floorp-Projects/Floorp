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

nsCSSValue::nsCSSValue(float aValue, nsCSSUnit aUnit) : mUnit(aUnit) {
  MOZ_ASSERT(eCSSUnit_Null == aUnit, "not a float value");
  mValue = aValue;
  MOZ_ASSERT(!std::isnan(mValue));
}

nsCSSValue::nsCSSValue(const nsCSSValue& aCopy) : mUnit(aCopy.mUnit) {
  if (eCSSUnit_Null != mUnit) {
    mValue = aCopy.mValue;
    MOZ_ASSERT(!std::isnan(mValue));
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
  return mValue == aOther.mValue;
}

nscoord nsCSSValue::GetPixelLength() const {
  MOZ_ASSERT(IsPixelLengthUnit(), "not a fixed length unit");

  double scaleFactor;
  switch (mUnit) {
    case eCSSUnit_Pixel:
      return nsPresContext::CSSPixelsToAppUnits(mValue);
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
  return nsPresContext::CSSPixelsToAppUnits(float(mValue * scaleFactor));
}

void nsCSSValue::SetPercentValue(float aValue) {
  Reset();
  mUnit = eCSSUnit_Percent;
  mValue = aValue;
  MOZ_ASSERT(!std::isnan(mValue));
}

void nsCSSValue::SetFloatValue(float aValue, nsCSSUnit aUnit) {
  MOZ_ASSERT(IsFloatUnit(aUnit), "not a float value");
  Reset();
  if (IsFloatUnit(aUnit)) {
    mUnit = aUnit;
    mValue = aValue;
    MOZ_ASSERT(!std::isnan(mValue));
  }
}
