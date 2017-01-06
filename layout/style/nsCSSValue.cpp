/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of simple property values within CSS declarations */

#include "nsCSSValue.h"

#include "mozilla/StyleSheetInlines.h"
#include "mozilla/Likely.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Move.h"
#include "mozilla/css/ImageLoader.h"
#include "CSSCalc.h"
#include "gfxFontConstants.h"
#include "imgIRequest.h"
#include "imgRequestProxy.h"
#include "nsIDocument.h"
#include "nsIPrincipal.h"
#include "nsCSSProps.h"
#include "nsNetUtil.h"
#include "nsPresContext.h"
#include "nsStyleUtil.h"
#include "nsDeviceContext.h"
#include "nsStyleSet.h"
#include "nsContentUtils.h"

using namespace mozilla;
using namespace mozilla::css;

static bool
IsLocalRefURL(nsStringBuffer* aString)
{
  // Find the first non-"C0 controls + space" character.
  char16_t* current = static_cast<char16_t*>(aString->Data());
  for (; *current != '\0'; current++) {
    if (*current > 0x20) {
      // if the first non-"C0 controls + space" character is '#', this is a
      // local-ref URL.
      return *current == '#';
    }
  }

  return false;
}

nsCSSValue::nsCSSValue(int32_t aValue, nsCSSUnit aUnit)
  : mUnit(aUnit)
{
  MOZ_ASSERT(aUnit == eCSSUnit_Integer || aUnit == eCSSUnit_Enumerated ||
             aUnit == eCSSUnit_EnumColor,
             "not an int value");
  if (aUnit == eCSSUnit_Integer || aUnit == eCSSUnit_Enumerated ||
      aUnit == eCSSUnit_EnumColor) {
    mValue.mInt = aValue;
  }
  else {
    mUnit = eCSSUnit_Null;
    mValue.mInt = 0;
  }
}

nsCSSValue::nsCSSValue(float aValue, nsCSSUnit aUnit)
  : mUnit(aUnit)
{
  MOZ_ASSERT(eCSSUnit_Percent <= aUnit, "not a float value");
  if (eCSSUnit_Percent <= aUnit) {
    mValue.mFloat = aValue;
    MOZ_ASSERT(!mozilla::IsNaN(mValue.mFloat));
  }
  else {
    mUnit = eCSSUnit_Null;
    mValue.mInt = 0;
  }
}

nsCSSValue::nsCSSValue(const nsString& aValue, nsCSSUnit aUnit)
  : mUnit(aUnit)
{
  MOZ_ASSERT(UnitHasStringValue(), "not a string value");
  if (UnitHasStringValue()) {
    mValue.mString = BufferFromString(aValue).take();
  }
  else {
    mUnit = eCSSUnit_Null;
    mValue.mInt = 0;
  }
}

nsCSSValue::nsCSSValue(nsCSSValue::Array* aValue, nsCSSUnit aUnit)
  : mUnit(aUnit)
{
  MOZ_ASSERT(UnitHasArrayValue(), "bad unit");
  mValue.mArray = aValue;
  mValue.mArray->AddRef();
}

nsCSSValue::nsCSSValue(mozilla::css::URLValue* aValue)
  : mUnit(eCSSUnit_URL)
{
  mValue.mURL = aValue;
  mValue.mURL->AddRef();
}

nsCSSValue::nsCSSValue(mozilla::css::ImageValue* aValue)
  : mUnit(eCSSUnit_Image)
{
  mValue.mImage = aValue;
  mValue.mImage->AddRef();
}

nsCSSValue::nsCSSValue(nsCSSValueGradient* aValue)
  : mUnit(eCSSUnit_Gradient)
{
  mValue.mGradient = aValue;
  mValue.mGradient->AddRef();
}

nsCSSValue::nsCSSValue(nsCSSValueTokenStream* aValue)
  : mUnit(eCSSUnit_TokenStream)
{
  mValue.mTokenStream = aValue;
  mValue.mTokenStream->AddRef();
}

nsCSSValue::nsCSSValue(mozilla::css::GridTemplateAreasValue* aValue)
  : mUnit(eCSSUnit_GridTemplateAreas)
{
  mValue.mGridTemplateAreas = aValue;
  mValue.mGridTemplateAreas->AddRef();
}

nsCSSValue::nsCSSValue(css::FontFamilyListRefCnt* aValue)
  : mUnit(eCSSUnit_FontFamilyList)
{
  mValue.mFontFamilyList = aValue;
  mValue.mFontFamilyList->AddRef();
}

nsCSSValue::nsCSSValue(const nsCSSValue& aCopy)
  : mUnit(aCopy.mUnit)
{
  if (mUnit <= eCSSUnit_DummyInherit) {
    // nothing to do, but put this important case first
  }
  else if (eCSSUnit_Percent <= mUnit) {
    mValue.mFloat = aCopy.mValue.mFloat;
    MOZ_ASSERT(!mozilla::IsNaN(mValue.mFloat));
  }
  else if (UnitHasStringValue()) {
    mValue.mString = aCopy.mValue.mString;
    mValue.mString->AddRef();
  }
  else if (eCSSUnit_Integer <= mUnit && mUnit <= eCSSUnit_EnumColor) {
    mValue.mInt = aCopy.mValue.mInt;
  }
  else if (IsIntegerColorUnit()) {
    mValue.mColor = aCopy.mValue.mColor;
  }
  else if (IsFloatColorUnit()) {
    mValue.mFloatColor = aCopy.mValue.mFloatColor;
    mValue.mFloatColor->AddRef();
  }
  else if (eCSSUnit_ComplexColor == mUnit) {
    mValue.mComplexColor = aCopy.mValue.mComplexColor;
    mValue.mComplexColor->AddRef();
  }
  else if (UnitHasArrayValue()) {
    mValue.mArray = aCopy.mValue.mArray;
    mValue.mArray->AddRef();
  }
  else if (eCSSUnit_URL == mUnit) {
    mValue.mURL = aCopy.mValue.mURL;
    mValue.mURL->AddRef();
  }
  else if (eCSSUnit_Image == mUnit) {
    mValue.mImage = aCopy.mValue.mImage;
    mValue.mImage->AddRef();
  }
  else if (eCSSUnit_Gradient == mUnit) {
    mValue.mGradient = aCopy.mValue.mGradient;
    mValue.mGradient->AddRef();
  }
  else if (eCSSUnit_TokenStream == mUnit) {
    mValue.mTokenStream = aCopy.mValue.mTokenStream;
    mValue.mTokenStream->AddRef();
  }
  else if (eCSSUnit_Pair == mUnit) {
    mValue.mPair = aCopy.mValue.mPair;
    mValue.mPair->AddRef();
  }
  else if (eCSSUnit_Triplet == mUnit) {
    mValue.mTriplet = aCopy.mValue.mTriplet;
    mValue.mTriplet->AddRef();
  }
  else if (eCSSUnit_Rect == mUnit) {
    mValue.mRect = aCopy.mValue.mRect;
    mValue.mRect->AddRef();
  }
  else if (eCSSUnit_List == mUnit) {
    mValue.mList = aCopy.mValue.mList;
    mValue.mList->AddRef();
  }
  else if (eCSSUnit_ListDep == mUnit) {
    mValue.mListDependent = aCopy.mValue.mListDependent;
  }
  else if (eCSSUnit_SharedList == mUnit) {
    mValue.mSharedList = aCopy.mValue.mSharedList;
    mValue.mSharedList->AddRef();
  }
  else if (eCSSUnit_PairList == mUnit) {
    mValue.mPairList = aCopy.mValue.mPairList;
    mValue.mPairList->AddRef();
  }
  else if (eCSSUnit_PairListDep == mUnit) {
    mValue.mPairListDependent = aCopy.mValue.mPairListDependent;
  }
  else if (eCSSUnit_GridTemplateAreas == mUnit) {
    mValue.mGridTemplateAreas = aCopy.mValue.mGridTemplateAreas;
    mValue.mGridTemplateAreas->AddRef();
  }
  else if (eCSSUnit_FontFamilyList == mUnit) {
    mValue.mFontFamilyList = aCopy.mValue.mFontFamilyList;
    mValue.mFontFamilyList->AddRef();
  }
  else {
    MOZ_ASSERT(false, "unknown unit");
  }
}

nsCSSValue& nsCSSValue::operator=(const nsCSSValue& aCopy)
{
  if (this != &aCopy) {
    Reset();
    new (this) nsCSSValue(aCopy);
  }
  return *this;
}

nsCSSValue&
nsCSSValue::operator=(nsCSSValue&& aOther)
{
  MOZ_ASSERT(this != &aOther, "Self assigment with rvalue reference");

  Reset();
  mUnit = aOther.mUnit;
  mValue = aOther.mValue;
  aOther.mUnit = eCSSUnit_Null;

  return *this;
}

bool nsCSSValue::operator==(const nsCSSValue& aOther) const
{
  MOZ_ASSERT(mUnit != eCSSUnit_ListDep &&
             aOther.mUnit != eCSSUnit_ListDep &&
             mUnit != eCSSUnit_PairListDep &&
             aOther.mUnit != eCSSUnit_PairListDep,
             "don't use operator== with dependent lists");

  if (mUnit == aOther.mUnit) {
    if (mUnit <= eCSSUnit_DummyInherit) {
      return true;
    }
    else if (UnitHasStringValue()) {
      return (NS_strcmp(GetBufferValue(mValue.mString),
                        GetBufferValue(aOther.mValue.mString)) == 0);
    }
    else if ((eCSSUnit_Integer <= mUnit) && (mUnit <= eCSSUnit_EnumColor)) {
      return mValue.mInt == aOther.mValue.mInt;
    }
    else if (IsIntegerColorUnit()) {
      return mValue.mColor == aOther.mValue.mColor;
    }
    else if (IsFloatColorUnit()) {
      return *mValue.mFloatColor == *aOther.mValue.mFloatColor;
    }
    else if (eCSSUnit_ComplexColor == mUnit) {
      return *mValue.mComplexColor == *aOther.mValue.mComplexColor;
    }
    else if (UnitHasArrayValue()) {
      return *mValue.mArray == *aOther.mValue.mArray;
    }
    else if (eCSSUnit_URL == mUnit) {
      return mValue.mURL->Equals(*aOther.mValue.mURL);
    }
    else if (eCSSUnit_Image == mUnit) {
      return mValue.mImage->Equals(*aOther.mValue.mImage);
    }
    else if (eCSSUnit_Gradient == mUnit) {
      return *mValue.mGradient == *aOther.mValue.mGradient;
    }
    else if (eCSSUnit_TokenStream == mUnit) {
      return *mValue.mTokenStream == *aOther.mValue.mTokenStream;
    }
    else if (eCSSUnit_Pair == mUnit) {
      return *mValue.mPair == *aOther.mValue.mPair;
    }
    else if (eCSSUnit_Triplet == mUnit) {
      return *mValue.mTriplet == *aOther.mValue.mTriplet;
    }
    else if (eCSSUnit_Rect == mUnit) {
      return *mValue.mRect == *aOther.mValue.mRect;
    }
    else if (eCSSUnit_List == mUnit) {
      return nsCSSValueList::Equal(mValue.mList, aOther.mValue.mList);
    }
    else if (eCSSUnit_SharedList == mUnit) {
      return *mValue.mSharedList == *aOther.mValue.mSharedList;
    }
    else if (eCSSUnit_PairList == mUnit) {
      return nsCSSValuePairList::Equal(mValue.mPairList,
                                       aOther.mValue.mPairList);
    }
    else if (eCSSUnit_GridTemplateAreas == mUnit) {
      return *mValue.mGridTemplateAreas == *aOther.mValue.mGridTemplateAreas;
    }
    else if (eCSSUnit_FontFamilyList == mUnit) {
      return *mValue.mFontFamilyList == *aOther.mValue.mFontFamilyList;
    }
    else {
      return mValue.mFloat == aOther.mValue.mFloat;
    }
  }
  return false;
}

double
nsCSSValue::GetAngleValueInRadians() const
{
  double angle = GetFloatValue();

  switch (GetUnit()) {
    case eCSSUnit_Radian: return angle;
    case eCSSUnit_Turn:   return angle * 2 * M_PI;
    case eCSSUnit_Degree: return angle * M_PI / 180.0;
    case eCSSUnit_Grad:   return angle * M_PI / 200.0;

    default:
      MOZ_ASSERT(false, "unrecognized angular unit");
      return 0.0;
  }
}

double
nsCSSValue::GetAngleValueInDegrees() const
{
  double angle = GetFloatValue();

  switch (GetUnit()) {
    case eCSSUnit_Degree: return angle;
    case eCSSUnit_Grad:   return angle * 0.9; // grad / 400 * 360
    case eCSSUnit_Radian: return angle * 180.0 / M_PI;  // rad / 2pi * 360
    case eCSSUnit_Turn:   return angle * 360.0;

    default:
      MOZ_ASSERT(false, "unrecognized angular unit");
      return 0.0;
  }
}

imgRequestProxy* nsCSSValue::GetImageValue(nsIDocument* aDocument) const
{
  MOZ_ASSERT(mUnit == eCSSUnit_Image, "not an Image value");
  return mValue.mImage->mRequests.GetWeak(aDocument);
}

already_AddRefed<imgRequestProxy>
nsCSSValue::GetPossiblyStaticImageValue(nsIDocument* aDocument,
                                        nsPresContext* aPresContext) const
{
  imgRequestProxy* req = GetImageValue(aDocument);
  if (aPresContext->IsDynamic()) {
    return do_AddRef(req);
  }
  return nsContentUtils::GetStaticRequest(req);
}

nscoord nsCSSValue::GetFixedLength(nsPresContext* aPresContext) const
{
  MOZ_ASSERT(mUnit == eCSSUnit_PhysicalMillimeter,
             "not a fixed length unit");

  float inches = mValue.mFloat / MM_PER_INCH_FLOAT;
  return NSToCoordFloorClamped(inches *
    float(aPresContext->DeviceContext()->AppUnitsPerPhysicalInch()));
}

nscoord nsCSSValue::GetPixelLength() const
{
  MOZ_ASSERT(IsPixelLengthUnit(), "not a fixed length unit");

  double scaleFactor;
  switch (mUnit) {
  case eCSSUnit_Pixel: return nsPresContext::CSSPixelsToAppUnits(mValue.mFloat);
  case eCSSUnit_Pica: scaleFactor = 16.0; break;
  case eCSSUnit_Point: scaleFactor = 4/3.0; break;
  case eCSSUnit_Inch: scaleFactor = 96.0; break;
  case eCSSUnit_Millimeter: scaleFactor = 96/25.4; break;
  case eCSSUnit_Centimeter: scaleFactor = 96/2.54; break;
  case eCSSUnit_Quarter: scaleFactor = 96/101.6; break;
  default:
    NS_ERROR("should never get here");
    return 0;
  }
  return nsPresContext::CSSPixelsToAppUnits(float(mValue.mFloat*scaleFactor));
}

void nsCSSValue::DoReset()
{
  if (UnitHasStringValue()) {
    mValue.mString->Release();
  } else if (IsFloatColorUnit()) {
    mValue.mFloatColor->Release();
  } else if (eCSSUnit_ComplexColor == mUnit) {
    mValue.mComplexColor->Release();
  } else if (UnitHasArrayValue()) {
    mValue.mArray->Release();
  } else if (eCSSUnit_URL == mUnit) {
    mValue.mURL->Release();
  } else if (eCSSUnit_Image == mUnit) {
    mValue.mImage->Release();
  } else if (eCSSUnit_Gradient == mUnit) {
    mValue.mGradient->Release();
  } else if (eCSSUnit_TokenStream == mUnit) {
    mValue.mTokenStream->Release();
  } else if (eCSSUnit_Pair == mUnit) {
    mValue.mPair->Release();
  } else if (eCSSUnit_Triplet == mUnit) {
    mValue.mTriplet->Release();
  } else if (eCSSUnit_Rect == mUnit) {
    mValue.mRect->Release();
  } else if (eCSSUnit_List == mUnit) {
    mValue.mList->Release();
  } else if (eCSSUnit_SharedList == mUnit) {
    mValue.mSharedList->Release();
  } else if (eCSSUnit_PairList == mUnit) {
    mValue.mPairList->Release();
  } else if (eCSSUnit_GridTemplateAreas == mUnit) {
    mValue.mGridTemplateAreas->Release();
  } else if (eCSSUnit_FontFamilyList == mUnit) {
    mValue.mFontFamilyList->Release();
  }
  mUnit = eCSSUnit_Null;
}

void nsCSSValue::SetIntValue(int32_t aValue, nsCSSUnit aUnit)
{
  MOZ_ASSERT(aUnit == eCSSUnit_Integer || aUnit == eCSSUnit_Enumerated ||
             aUnit == eCSSUnit_EnumColor,
             "not an int value");
  Reset();
  if (aUnit == eCSSUnit_Integer || aUnit == eCSSUnit_Enumerated ||
      aUnit == eCSSUnit_EnumColor) {
    mUnit = aUnit;
    mValue.mInt = aValue;
  }
}

void nsCSSValue::SetPercentValue(float aValue)
{
  Reset();
  mUnit = eCSSUnit_Percent;
  mValue.mFloat = aValue;
  MOZ_ASSERT(!mozilla::IsNaN(mValue.mFloat));
}

void nsCSSValue::SetFloatValue(float aValue, nsCSSUnit aUnit)
{
  MOZ_ASSERT(IsFloatUnit(aUnit), "not a float value");
  Reset();
  if (IsFloatUnit(aUnit)) {
    mUnit = aUnit;
    mValue.mFloat = aValue;
    MOZ_ASSERT(!mozilla::IsNaN(mValue.mFloat));
  }
}

void nsCSSValue::SetStringValue(const nsString& aValue,
                                nsCSSUnit aUnit)
{
  Reset();
  mUnit = aUnit;
  MOZ_ASSERT(UnitHasStringValue(), "not a string unit");
  if (UnitHasStringValue()) {
    mValue.mString = BufferFromString(aValue).take();
  } else
    mUnit = eCSSUnit_Null;
}

void nsCSSValue::SetColorValue(nscolor aValue)
{
  SetIntegerColorValue(aValue, eCSSUnit_RGBAColor);
}



void nsCSSValue::SetIntegerColorValue(nscolor aValue, nsCSSUnit aUnit)
{
  Reset();
  mUnit = aUnit;
  MOZ_ASSERT(IsIntegerColorUnit(), "bad unit");
  mValue.mColor = aValue;
}

void nsCSSValue::SetIntegerCoordValue(nscoord aValue)
{
  SetFloatValue(nsPresContext::AppUnitsToFloatCSSPixels(aValue),
                eCSSUnit_Pixel);
}

void nsCSSValue::SetFloatColorValue(float aComponent1,
                                    float aComponent2,
                                    float aComponent3,
                                    float aAlpha,
                                    nsCSSUnit aUnit)
{
  Reset();
  mUnit = aUnit;
  MOZ_ASSERT(IsFloatColorUnit(), "bad unit");
  mValue.mFloatColor =
    new nsCSSValueFloatColor(aComponent1, aComponent2, aComponent3, aAlpha);
  mValue.mFloatColor->AddRef();
}

void
nsCSSValue::SetRGBAColorValue(const RGBAColorData& aValue)
{
  SetFloatColorValue(aValue.mR, aValue.mG, aValue.mB,
                     aValue.mA, eCSSUnit_PercentageRGBAColor);
}

void
nsCSSValue::SetComplexColorValue(already_AddRefed<ComplexColorValue> aValue)
{
  Reset();
  mUnit = eCSSUnit_ComplexColor;
  mValue.mComplexColor = aValue.take();
}

void nsCSSValue::SetArrayValue(nsCSSValue::Array* aValue, nsCSSUnit aUnit)
{
  Reset();
  mUnit = aUnit;
  MOZ_ASSERT(UnitHasArrayValue(), "bad unit");
  mValue.mArray = aValue;
  mValue.mArray->AddRef();
}

void nsCSSValue::SetURLValue(mozilla::css::URLValue* aValue)
{
  Reset();
  mUnit = eCSSUnit_URL;
  mValue.mURL = aValue;
  mValue.mURL->AddRef();
}

void nsCSSValue::SetImageValue(mozilla::css::ImageValue* aValue)
{
  Reset();
  mUnit = eCSSUnit_Image;
  mValue.mImage = aValue;
  mValue.mImage->AddRef();
}

void nsCSSValue::SetGradientValue(nsCSSValueGradient* aValue)
{
  Reset();
  mUnit = eCSSUnit_Gradient;
  mValue.mGradient = aValue;
  mValue.mGradient->AddRef();
}

void nsCSSValue::SetTokenStreamValue(nsCSSValueTokenStream* aValue)
{
  Reset();
  mUnit = eCSSUnit_TokenStream;
  mValue.mTokenStream = aValue;
  mValue.mTokenStream->AddRef();
}

void nsCSSValue::SetGridTemplateAreas(mozilla::css::GridTemplateAreasValue* aValue)
{
  Reset();
  mUnit = eCSSUnit_GridTemplateAreas;
  mValue.mGridTemplateAreas = aValue;
  mValue.mGridTemplateAreas->AddRef();
}

void nsCSSValue::SetFontFamilyListValue(css::FontFamilyListRefCnt* aValue)
{
  Reset();
  mUnit = eCSSUnit_FontFamilyList;
  mValue.mFontFamilyList = aValue;
  mValue.mFontFamilyList->AddRef();
}

void nsCSSValue::SetPairValue(const nsCSSValuePair* aValue)
{
  // pairs should not be used for null/inherit/initial values
  MOZ_ASSERT(aValue &&
             aValue->mXValue.GetUnit() != eCSSUnit_Null &&
             aValue->mYValue.GetUnit() != eCSSUnit_Null &&
             aValue->mXValue.GetUnit() != eCSSUnit_Inherit &&
             aValue->mYValue.GetUnit() != eCSSUnit_Inherit &&
             aValue->mXValue.GetUnit() != eCSSUnit_Initial &&
             aValue->mYValue.GetUnit() != eCSSUnit_Initial &&
             aValue->mXValue.GetUnit() != eCSSUnit_Unset &&
             aValue->mYValue.GetUnit() != eCSSUnit_Unset,
             "missing or inappropriate pair value");
  Reset();
  mUnit = eCSSUnit_Pair;
  mValue.mPair = new nsCSSValuePair_heap(aValue->mXValue, aValue->mYValue);
  mValue.mPair->AddRef();
}

void nsCSSValue::SetPairValue(const nsCSSValue& xValue,
                              const nsCSSValue& yValue)
{
  MOZ_ASSERT(xValue.GetUnit() != eCSSUnit_Null &&
             yValue.GetUnit() != eCSSUnit_Null &&
             xValue.GetUnit() != eCSSUnit_Inherit &&
             yValue.GetUnit() != eCSSUnit_Inherit &&
             xValue.GetUnit() != eCSSUnit_Initial &&
             yValue.GetUnit() != eCSSUnit_Initial &&
             xValue.GetUnit() != eCSSUnit_Unset &&
             yValue.GetUnit() != eCSSUnit_Unset,
             "inappropriate pair value");
  Reset();
  mUnit = eCSSUnit_Pair;
  mValue.mPair = new nsCSSValuePair_heap(xValue, yValue);
  mValue.mPair->AddRef();
}

void nsCSSValue::SetTripletValue(const nsCSSValueTriplet* aValue)
{
  // triplet should not be used for null/inherit/initial values
  MOZ_ASSERT(aValue &&
             aValue->mXValue.GetUnit() != eCSSUnit_Null &&
             aValue->mYValue.GetUnit() != eCSSUnit_Null &&
             aValue->mZValue.GetUnit() != eCSSUnit_Null &&
             aValue->mXValue.GetUnit() != eCSSUnit_Inherit &&
             aValue->mYValue.GetUnit() != eCSSUnit_Inherit &&
             aValue->mZValue.GetUnit() != eCSSUnit_Inherit &&
             aValue->mXValue.GetUnit() != eCSSUnit_Initial &&
             aValue->mYValue.GetUnit() != eCSSUnit_Initial &&
             aValue->mZValue.GetUnit() != eCSSUnit_Initial &&
             aValue->mXValue.GetUnit() != eCSSUnit_Unset &&
             aValue->mYValue.GetUnit() != eCSSUnit_Unset &&
             aValue->mZValue.GetUnit() != eCSSUnit_Unset,
             "missing or inappropriate triplet value");
  Reset();
  mUnit = eCSSUnit_Triplet;
  mValue.mTriplet = new nsCSSValueTriplet_heap(aValue->mXValue, aValue->mYValue, aValue->mZValue);
  mValue.mTriplet->AddRef();
}

void nsCSSValue::SetTripletValue(const nsCSSValue& xValue,
                                 const nsCSSValue& yValue,
                                 const nsCSSValue& zValue)
{
  // Only allow Null for the z component
  MOZ_ASSERT(xValue.GetUnit() != eCSSUnit_Null &&
             yValue.GetUnit() != eCSSUnit_Null &&
             xValue.GetUnit() != eCSSUnit_Inherit &&
             yValue.GetUnit() != eCSSUnit_Inherit &&
             zValue.GetUnit() != eCSSUnit_Inherit &&
             xValue.GetUnit() != eCSSUnit_Initial &&
             yValue.GetUnit() != eCSSUnit_Initial &&
             zValue.GetUnit() != eCSSUnit_Initial &&
             xValue.GetUnit() != eCSSUnit_Unset &&
             yValue.GetUnit() != eCSSUnit_Unset &&
             zValue.GetUnit() != eCSSUnit_Unset,
             "inappropriate triplet value");
  Reset();
  mUnit = eCSSUnit_Triplet;
  mValue.mTriplet = new nsCSSValueTriplet_heap(xValue, yValue, zValue);
  mValue.mTriplet->AddRef();
}

nsCSSRect& nsCSSValue::SetRectValue()
{
  Reset();
  mUnit = eCSSUnit_Rect;
  mValue.mRect = new nsCSSRect_heap;
  mValue.mRect->AddRef();
  return *mValue.mRect;
}

nsCSSValueList* nsCSSValue::SetListValue()
{
  Reset();
  mUnit = eCSSUnit_List;
  mValue.mList = new nsCSSValueList_heap;
  mValue.mList->AddRef();
  return mValue.mList;
}

void nsCSSValue::SetSharedListValue(nsCSSValueSharedList* aList)
{
  Reset();
  mUnit = eCSSUnit_SharedList;
  mValue.mSharedList = aList;
  mValue.mSharedList->AddRef();
}

void nsCSSValue::SetDependentListValue(nsCSSValueList* aList)
{
  Reset();
  if (aList) {
    mUnit = eCSSUnit_ListDep;
    mValue.mListDependent = aList;
  }
}

void
nsCSSValue::AdoptListValue(UniquePtr<nsCSSValueList> aValue)
{
  // We have to copy the first element since for owned lists the first
  // element should be an nsCSSValueList_heap object.
  SetListValue();
  mValue.mList->mValue = Move(aValue->mValue);
  mValue.mList->mNext  = aValue->mNext;
  aValue->mNext = nullptr;
  aValue.reset();
}

nsCSSValuePairList* nsCSSValue::SetPairListValue()
{
  Reset();
  mUnit = eCSSUnit_PairList;
  mValue.mPairList = new nsCSSValuePairList_heap;
  mValue.mPairList->AddRef();
  return mValue.mPairList;
}

void nsCSSValue::SetDependentPairListValue(nsCSSValuePairList* aList)
{
  Reset();
  if (aList) {
    mUnit = eCSSUnit_PairListDep;
    mValue.mPairListDependent = aList;
  }
}

void
nsCSSValue::AdoptPairListValue(UniquePtr<nsCSSValuePairList> aValue)
{
  // We have to copy the first element, since for owned pair lists, the first
  // element should be an nsCSSValuePairList_heap object.
  SetPairListValue();
  mValue.mPairList->mXValue = Move(aValue->mXValue);
  mValue.mPairList->mYValue = Move(aValue->mYValue);
  mValue.mPairList->mNext   = aValue->mNext;
  aValue->mNext = nullptr;
  aValue.reset();
}

void nsCSSValue::SetAutoValue()
{
  Reset();
  mUnit = eCSSUnit_Auto;
}

void nsCSSValue::SetInheritValue()
{
  Reset();
  mUnit = eCSSUnit_Inherit;
}

void nsCSSValue::SetInitialValue()
{
  Reset();
  mUnit = eCSSUnit_Initial;
}

void nsCSSValue::SetUnsetValue()
{
  Reset();
  mUnit = eCSSUnit_Unset;
}

void nsCSSValue::SetNoneValue()
{
  Reset();
  mUnit = eCSSUnit_None;
}

void nsCSSValue::SetAllValue()
{
  Reset();
  mUnit = eCSSUnit_All;
}

void nsCSSValue::SetNormalValue()
{
  Reset();
  mUnit = eCSSUnit_Normal;
}

void nsCSSValue::SetSystemFontValue()
{
  Reset();
  mUnit = eCSSUnit_System_Font;
}

void nsCSSValue::SetDummyValue()
{
  Reset();
  mUnit = eCSSUnit_Dummy;
}

void nsCSSValue::SetDummyInheritValue()
{
  Reset();
  mUnit = eCSSUnit_DummyInherit;
}

void nsCSSValue::SetCalcValue(const nsStyleCoord::CalcValue* aCalc)
{
  RefPtr<nsCSSValue::Array> arr = nsCSSValue::Array::Create(1);
  if (!aCalc->mHasPercent) {
    arr->Item(0).SetIntegerCoordValue(aCalc->mLength);
  } else {
    nsCSSValue::Array *arr2 = nsCSSValue::Array::Create(2);
    arr->Item(0).SetArrayValue(arr2, eCSSUnit_Calc_Plus);
    arr2->Item(0).SetIntegerCoordValue(aCalc->mLength);
    arr2->Item(1).SetPercentValue(aCalc->mPercent);
  }

  SetArrayValue(arr, eCSSUnit_Calc);
}

void nsCSSValue::StartImageLoad(nsIDocument* aDocument) const
{
  MOZ_ASSERT(eCSSUnit_URL == mUnit, "Not a URL value!");
  mozilla::css::ImageValue* image =
    new mozilla::css::ImageValue(mValue.mURL->GetURI(),
                                 mValue.mURL->mString,
                                 mValue.mURL->mBaseURI,
                                 mValue.mURL->mReferrer,
                                 mValue.mURL->mOriginPrincipal,
                                 aDocument);

  nsCSSValue* writable = const_cast<nsCSSValue*>(this);
  writable->SetImageValue(image);
}

nscolor nsCSSValue::GetColorValue() const
{
  MOZ_ASSERT(IsNumericColorUnit(), "not a color value");
  if (IsFloatColorUnit()) {
    return mValue.mFloatColor->GetColorValue(mUnit);
  }
  return mValue.mColor;
}

bool nsCSSValue::IsNonTransparentColor() const
{
  // We have the value in the form it was specified in at this point, so we
  // have to look for both the keyword 'transparent' and its equivalent in
  // rgba notation.
  nsDependentString buf;
  return
    (IsIntegerColorUnit() && NS_GET_A(GetColorValue()) > 0) ||
    (IsFloatColorUnit() && mValue.mFloatColor->IsNonTransparentColor()) ||
    (mUnit == eCSSUnit_Ident &&
     !nsGkAtoms::transparent->Equals(GetStringValue(buf))) ||
    (mUnit == eCSSUnit_EnumColor);
}

nsCSSValue::Array*
nsCSSValue::InitFunction(nsCSSKeyword aFunctionId, uint32_t aNumArgs)
{
  RefPtr<nsCSSValue::Array> func = Array::Create(aNumArgs + 1);
  func->Item(0).SetIntValue(aFunctionId, eCSSUnit_Enumerated);
  SetArrayValue(func, eCSSUnit_Function);
  return func;
}

bool
nsCSSValue::EqualsFunction(nsCSSKeyword aFunctionId) const
{
  if (mUnit != eCSSUnit_Function) {
    return false;
  }

  nsCSSValue::Array* func = mValue.mArray;
  MOZ_ASSERT(func && func->Count() >= 1 &&
             func->Item(0).GetUnit() == eCSSUnit_Enumerated,
             "illegally structured function value");

  nsCSSKeyword thisFunctionId = func->Item(0).GetKeywordValue();
  return thisFunctionId == aFunctionId;
}

// static
already_AddRefed<nsStringBuffer>
nsCSSValue::BufferFromString(const nsString& aValue)
{
  RefPtr<nsStringBuffer> buffer = nsStringBuffer::FromString(aValue);
  if (buffer) {
    return buffer.forget();
  }

  nsString::size_type length = aValue.Length();

  // NOTE: Alloc prouduces a new, already-addref'd (refcnt = 1) buffer.
  // NOTE: String buffer allocation is currently fallible.
  size_t sz = (length + 1) * sizeof(char16_t);
  buffer = nsStringBuffer::Alloc(sz);
  if (MOZ_UNLIKELY(!buffer)) {
    NS_ABORT_OOM(sz);
  }

  char16_t* data = static_cast<char16_t*>(buffer->Data());
  nsCharTraits<char16_t>::copy(data, aValue.get(), length);
  // Null-terminate.
  data[length] = 0;
  return buffer.forget();
}

namespace {

struct CSSValueSerializeCalcOps {
  CSSValueSerializeCalcOps(nsCSSPropertyID aProperty, nsAString& aResult,
                           nsCSSValue::Serialization aSerialization)
    : mProperty(aProperty),
      mResult(aResult),
      mValueSerialization(aSerialization)
  {
  }

  typedef nsCSSValue input_type;
  typedef nsCSSValue::Array input_array_type;

  static nsCSSUnit GetUnit(const input_type& aValue) {
    return aValue.GetUnit();
  }

  void Append(const char* aString)
  {
    mResult.AppendASCII(aString);
  }

  void AppendLeafValue(const input_type& aValue)
  {
    MOZ_ASSERT(aValue.GetUnit() == eCSSUnit_Percent ||
               aValue.IsLengthUnit() ||
               aValue.GetUnit() == eCSSUnit_Number,
               "unexpected unit");
    aValue.AppendToString(mProperty, mResult, mValueSerialization);
  }

  void AppendCoefficient(const input_type& aValue)
  {
    MOZ_ASSERT(aValue.GetUnit() == eCSSUnit_Number, "unexpected unit");
    aValue.AppendToString(mProperty, mResult, mValueSerialization);
  }

private:
  nsCSSPropertyID mProperty;
  nsAString &mResult;
  nsCSSValue::Serialization mValueSerialization;
};

} // namespace

void
nsCSSValue::AppendPolygonToString(nsCSSPropertyID aProperty, nsAString& aResult,
                                  Serialization aSerialization) const
{
  const nsCSSValue::Array* array = GetArrayValue();
  MOZ_ASSERT(array->Count() > 1 && array->Count() <= 3,
             "Polygons must have name and at least one more value.");
  // When the array has 2 elements, the item on index 1 is the coordinate
  // pair list.
  // When the array has 3 elements, the item on index 1 is a fill-rule
  // and item on index 2 is the coordinate pair list.
  size_t index = 1;
  if (array->Count() == 3) {
    const nsCSSValue& fillRuleValue = array->Item(index);
    MOZ_ASSERT(fillRuleValue.GetUnit() == eCSSUnit_Enumerated,
               "Expected polygon fill rule.");
    int32_t fillRule = fillRuleValue.GetIntValue();
    AppendASCIItoUTF16(nsCSSProps::ValueToKeyword(fillRule,
                                                  nsCSSProps::kFillRuleKTable),
                       aResult);
    aResult.AppendLiteral(", ");
    ++index;
  }
  array->Item(index).AppendToString(aProperty, aResult, aSerialization);
}

inline void
nsCSSValue::AppendPositionCoordinateToString(
                const nsCSSValue& aValue, nsCSSPropertyID aProperty,
                nsAString& aResult, Serialization aSerialization) const
{
  if (aValue.GetUnit() == eCSSUnit_Enumerated) {
    int32_t intValue = aValue.GetIntValue();
    AppendASCIItoUTF16(nsCSSProps::ValueToKeyword(intValue,
                          nsCSSProps::kShapeRadiusKTable), aResult);
  } else {
    aValue.AppendToString(aProperty, aResult, aSerialization);
  }
}

void
nsCSSValue::AppendCircleOrEllipseToString(nsCSSKeyword aFunctionId,
                                          nsCSSPropertyID aProperty,
                                          nsAString& aResult,
                                          Serialization aSerialization) const
{
  const nsCSSValue::Array* array = GetArrayValue();
  size_t count = aFunctionId == eCSSKeyword_circle ? 2 : 3;
  MOZ_ASSERT(array->Count() == count + 1, "wrong number of arguments");

  bool hasRadii = array->Item(1).GetUnit() != eCSSUnit_Null;

  // closest-side is the default, so we don't need to
  // output it if all values are closest-side.
  if (array->Item(1).GetUnit() == eCSSUnit_Enumerated &&
      StyleShapeRadius(array->Item(1).GetIntValue()) == StyleShapeRadius::ClosestSide &&
      (aFunctionId == eCSSKeyword_circle ||
       (array->Item(2).GetUnit() == eCSSUnit_Enumerated &&
        StyleShapeRadius(array->Item(2).GetIntValue()) == StyleShapeRadius::ClosestSide))) {
    hasRadii = false;
  } else {
    AppendPositionCoordinateToString(array->Item(1), aProperty,
                                     aResult, aSerialization);

    if (hasRadii && aFunctionId == eCSSKeyword_ellipse) {
      aResult.Append(' ');
      AppendPositionCoordinateToString(array->Item(2), aProperty,
                                       aResult, aSerialization);
    }
  }

  if (hasRadii) {
    aResult.Append(' ');
  }

  // Any position specified?
  if (array->Item(count).GetUnit() != eCSSUnit_Array) {
    MOZ_ASSERT(array->Item(count).GetUnit() == eCSSUnit_Null,
               "unexpected value");
    // We only serialize to the 2 or 4 value form
    // |circle()| is valid, but should be expanded
    // to |circle(at 50% 50%)|
    aResult.AppendLiteral("at 50% 50%");
    return;
  }

  aResult.AppendLiteral("at ");
  array->Item(count).AppendBasicShapePositionToString(aResult, aSerialization);
}

// https://drafts.csswg.org/css-shapes/#basic-shape-serialization
// basic-shape asks us to omit a lot of redundant things whilst serializing
// position values. Other specs are not clear about this
// (https://github.com/w3c/csswg-drafts/issues/368), so for now we special-case
// basic shapes only
void
nsCSSValue::AppendBasicShapePositionToString(nsAString& aResult,
                                             Serialization aSerialization) const
{
  const nsCSSValue::Array* array = GetArrayValue();
  // We always parse these into an array of four elements
  MOZ_ASSERT(array->Count() == 4,
             "basic-shape position value doesn't have enough elements");

  const nsCSSValue &xEdge   = array->Item(0);
  const nsCSSValue &xOffset = array->Item(1);
  const nsCSSValue &yEdge   = array->Item(2);
  const nsCSSValue &yOffset = array->Item(3);

  MOZ_ASSERT(xEdge.GetUnit() == eCSSUnit_Enumerated &&
             yEdge.GetUnit() == eCSSUnit_Enumerated &&
             xOffset.IsLengthPercentCalcUnit() &&
             yOffset.IsLengthPercentCalcUnit() &&
             xEdge.GetIntValue() != NS_STYLE_IMAGELAYER_POSITION_CENTER &&
             yEdge.GetIntValue() != NS_STYLE_IMAGELAYER_POSITION_CENTER,
             "Ensure invariants from ParsePositionValueBasicShape "
             "haven't been modified");
  if (xEdge.GetIntValue() == NS_STYLE_IMAGELAYER_POSITION_LEFT &&
      yEdge.GetIntValue() == NS_STYLE_IMAGELAYER_POSITION_TOP) {
    // We can omit these defaults
    xOffset.AppendToString(eCSSProperty_UNKNOWN, aResult, aSerialization);
    aResult.Append(' ');
    yOffset.AppendToString(eCSSProperty_UNKNOWN, aResult, aSerialization);
  } else {
    // We only serialize to the two or four valued form
    xEdge.AppendToString(eCSSProperty_object_position, aResult, aSerialization);
    aResult.Append(' ');
    xOffset.AppendToString(eCSSProperty_UNKNOWN, aResult, aSerialization);
    aResult.Append(' ');
    yEdge.AppendToString(eCSSProperty_object_position, aResult, aSerialization);
    aResult.Append(' ');
    yOffset.AppendToString(eCSSProperty_UNKNOWN, aResult, aSerialization);
  }
}

// Helper to append |aString| with the shorthand sides notation used in e.g.
// 'padding'. |aProperties| and |aValues| are expected to have 4 elements.
/*static*/ void
nsCSSValue::AppendSidesShorthandToString(const nsCSSPropertyID aProperties[],
                                         const nsCSSValue* aValues[],
                                         nsAString& aString,
                                         nsCSSValue::Serialization
                                            aSerialization)
{
  const nsCSSValue& value1 = *aValues[0];
  const nsCSSValue& value2 = *aValues[1];
  const nsCSSValue& value3 = *aValues[2];
  const nsCSSValue& value4 = *aValues[3];

  MOZ_ASSERT(value1.GetUnit() != eCSSUnit_Null, "null value 1");
  value1.AppendToString(aProperties[0], aString, aSerialization);
  if (value1 != value2 || value1 != value3 || value1 != value4) {
    aString.Append(char16_t(' '));
    MOZ_ASSERT(value2.GetUnit() != eCSSUnit_Null, "null value 2");
    value2.AppendToString(aProperties[1], aString, aSerialization);
    if (value1 != value3 || value2 != value4) {
      aString.Append(char16_t(' '));
      MOZ_ASSERT(value3.GetUnit() != eCSSUnit_Null, "null value 3");
      value3.AppendToString(aProperties[2], aString, aSerialization);
      if (value2 != value4) {
        aString.Append(char16_t(' '));
        MOZ_ASSERT(value4.GetUnit() != eCSSUnit_Null, "null value 4");
        value4.AppendToString(aProperties[3], aString, aSerialization);
      }
    }
  }
}

/*static*/ void
nsCSSValue::AppendBasicShapeRadiusToString(const nsCSSPropertyID aProperties[],
                                           const nsCSSValue* aValues[],
                                           nsAString& aResult,
                                           Serialization aSerialization)
{
  bool needY = false;
  const nsCSSValue* xVals[4];
  const nsCSSValue* yVals[4];
  for (int i = 0; i < 4; i++) {
    if (aValues[i]->GetUnit() == eCSSUnit_Pair) {
      needY = true;
      xVals[i] = &aValues[i]->GetPairValue().mXValue;
      yVals[i] = &aValues[i]->GetPairValue().mYValue;
    } else {
      xVals[i] = yVals[i] = aValues[i];
    }
  }

  AppendSidesShorthandToString(aProperties, xVals, aResult, aSerialization);
  if (needY) {
    aResult.AppendLiteral(" / ");
    AppendSidesShorthandToString(aProperties, yVals, aResult, aSerialization);
  }
}

void
nsCSSValue::AppendInsetToString(nsCSSPropertyID aProperty, nsAString& aResult,
                                Serialization aSerialization) const
{
  const nsCSSValue::Array* array = GetArrayValue();
  MOZ_ASSERT(array->Count() == 6,
             "inset function has wrong number of arguments");
  if (array->Item(1).GetUnit() != eCSSUnit_Null) {
    array->Item(1).AppendToString(aProperty, aResult, aSerialization);
    if (array->Item(2).GetUnit() != eCSSUnit_Null) {
      aResult.Append(' ');
      array->Item(2).AppendToString(aProperty, aResult, aSerialization);
      if (array->Item(3).GetUnit() != eCSSUnit_Null) {
        aResult.Append(' ');
        array->Item(3).AppendToString(aProperty, aResult, aSerialization);
        if (array->Item(4).GetUnit() != eCSSUnit_Null) {
          aResult.Append(' ');
          array->Item(4).AppendToString(aProperty, aResult, aSerialization);
        }
      }
    }
  }

  if (array->Item(5).GetUnit() == eCSSUnit_Array) {
    const nsCSSPropertyID* subprops =
      nsCSSProps::SubpropertyEntryFor(eCSSProperty_border_radius);
    const nsCSSValue::Array* radius = array->Item(5).GetArrayValue();
    MOZ_ASSERT(radius->Count() == 4, "expected 4 radii values");
    const nsCSSValue* vals[4] = {
      &(radius->Item(0)),
      &(radius->Item(1)),
      &(radius->Item(2)),
      &(radius->Item(3))
    };
    aResult.AppendLiteral(" round ");
    AppendBasicShapeRadiusToString(subprops, vals, aResult,
                                   aSerialization);
  } else {
    MOZ_ASSERT(array->Item(5).GetUnit() == eCSSUnit_Null,
               "unexpected value");
  }
}

/* static */ void
nsCSSValue::AppendAlignJustifyValueToString(int32_t aValue, nsAString& aResult)
{
  auto legacy = aValue & NS_STYLE_ALIGN_LEGACY;
  if (legacy) {
    aValue &= ~legacy;
    aResult.AppendLiteral("legacy ");
  }
  auto overflowPos = aValue & (NS_STYLE_ALIGN_SAFE | NS_STYLE_ALIGN_UNSAFE);
  aValue &= ~overflowPos;
  MOZ_ASSERT(!(aValue & NS_STYLE_ALIGN_FLAG_BITS),
             "unknown bits in align/justify value");
  MOZ_ASSERT((aValue != NS_STYLE_ALIGN_AUTO &&
              aValue != NS_STYLE_ALIGN_NORMAL &&
              aValue != NS_STYLE_ALIGN_BASELINE &&
              aValue != NS_STYLE_ALIGN_LAST_BASELINE) ||
             (!legacy && !overflowPos),
             "auto/normal/baseline/'last baseline' never have any flags");
  MOZ_ASSERT(legacy == 0 || overflowPos == 0,
             "'legacy' together with <overflow-position>");
  if (aValue == NS_STYLE_ALIGN_LAST_BASELINE) {
    aResult.AppendLiteral("last ");
    aValue = NS_STYLE_ALIGN_BASELINE;
  }
  const auto& kwtable(nsCSSProps::kAlignAllKeywords);
  AppendASCIItoUTF16(nsCSSProps::ValueToKeyword(aValue, kwtable), aResult);
  // Don't serialize the 'unsafe' keyword; it's the default.
  if (MOZ_UNLIKELY(overflowPos == NS_STYLE_ALIGN_SAFE)) {
    aResult.Append(' ');
    AppendASCIItoUTF16(nsCSSProps::ValueToKeyword(overflowPos, kwtable),
                       aResult);
  }
}

void
nsCSSValue::AppendToString(nsCSSPropertyID aProperty, nsAString& aResult,
                           Serialization aSerialization) const
{
  // eCSSProperty_UNKNOWN gets used for some recursive calls below.
  MOZ_ASSERT((0 <= aProperty &&
              aProperty <= eCSSProperty_COUNT_no_shorthands) ||
             aProperty == eCSSProperty_UNKNOWN ||
             aProperty == eCSSProperty_DOM,
             "property ID out of range");

  nsCSSUnit unit = GetUnit();
  if (unit == eCSSUnit_Null) {
    return;
  }

  if (eCSSUnit_String <= unit && unit <= eCSSUnit_Attr) {
    if (unit == eCSSUnit_Attr) {
      aResult.AppendLiteral("attr(");
    }
    nsAutoString  buffer;
    GetStringValue(buffer);
    if (unit == eCSSUnit_String) {
      nsStyleUtil::AppendEscapedCSSString(buffer, aResult);
    } else {
      nsStyleUtil::AppendEscapedCSSIdent(buffer, aResult);
    }
  }
  else if (eCSSUnit_Array <= unit && unit <= eCSSUnit_Symbols) {
    switch (unit) {
      case eCSSUnit_Counter:  aResult.AppendLiteral("counter(");  break;
      case eCSSUnit_Counters: aResult.AppendLiteral("counters("); break;
      case eCSSUnit_Cubic_Bezier: aResult.AppendLiteral("cubic-bezier("); break;
      case eCSSUnit_Steps: aResult.AppendLiteral("steps("); break;
      case eCSSUnit_Symbols: aResult.AppendLiteral("symbols("); break;
      default: break;
    }

    nsCSSValue::Array *array = GetArrayValue();
    bool mark = false;
    for (size_t i = 0, i_end = array->Count(); i < i_end; ++i) {
      if (mark && array->Item(i).GetUnit() != eCSSUnit_Null) {
        if ((unit == eCSSUnit_Array &&
             eCSSProperty_transition_timing_function != aProperty) ||
            unit == eCSSUnit_Symbols)
          aResult.Append(' ');
        else if (unit != eCSSUnit_Steps)
          aResult.AppendLiteral(", ");
      }
      if (unit == eCSSUnit_Steps && i == 1) {
        MOZ_ASSERT(array->Item(i).GetUnit() == eCSSUnit_Enumerated,
                   "unexpected value");
        int32_t side = array->Item(i).GetIntValue();
        MOZ_ASSERT(side == NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_START ||
                   side == NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_END ||
                   side == -1,
                   "unexpected value");
        if (side == NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_START) {
          aResult.AppendLiteral(", start");
        } else if (side == NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_END) {
          aResult.AppendLiteral(", end");
        }
        continue;
      }
      if (unit == eCSSUnit_Symbols && i == 0) {
        MOZ_ASSERT(array->Item(i).GetUnit() == eCSSUnit_Enumerated,
                   "unexpected value");
        int32_t system = array->Item(i).GetIntValue();
        if (system != NS_STYLE_COUNTER_SYSTEM_SYMBOLIC) {
          AppendASCIItoUTF16(nsCSSProps::ValueToKeyword(
                  system, nsCSSProps::kCounterSystemKTable), aResult);
          mark = true;
        }
        continue;
      }
      nsCSSPropertyID prop =
        ((eCSSUnit_Counter <= unit && unit <= eCSSUnit_Counters) &&
         i == array->Count() - 1)
        ? eCSSProperty_list_style_type : aProperty;
      if (array->Item(i).GetUnit() != eCSSUnit_Null) {
        array->Item(i).AppendToString(prop, aResult, aSerialization);
        mark = true;
      }
    }
    if (eCSSUnit_Array == unit &&
        aProperty == eCSSProperty_transition_timing_function) {
      aResult.Append(')');
    }
  }
  /* Although Function is backed by an Array, we'll handle it separately
   * because it's a bit quirky.
   */
  else if (eCSSUnit_Function == unit) {
    const nsCSSValue::Array* array = GetArrayValue();
    MOZ_ASSERT(array->Count() >= 1,
               "Functions must have at least one element for the name.");

    const nsCSSValue& functionName = array->Item(0);
    MOZ_ASSERT(functionName.GetUnit() == eCSSUnit_Enumerated,
               "Functions must have an enumerated name.");
    // The first argument is always of nsCSSKeyword type.
    const nsCSSKeyword functionId = functionName.GetKeywordValue();

    // minmax(auto, <flex>) is equivalent to (and is our internal representation
    // of) <flex>, and both are serialized as <flex>
    if (functionId == eCSSKeyword_minmax &&
        array->Count() == 3 &&
        array->Item(1).GetUnit() == eCSSUnit_Auto &&
        array->Item(2).GetUnit() == eCSSUnit_FlexFraction) {
      array->Item(2).AppendToString(aProperty, aResult, aSerialization);
      MOZ_ASSERT(aProperty == eCSSProperty_grid_template_columns ||
                 aProperty == eCSSProperty_grid_template_rows ||
                 aProperty == eCSSProperty_grid_auto_columns ||
                 aProperty == eCSSProperty_grid_auto_rows);
      return;
    }

    /* Append the function name. */
    NS_ConvertASCIItoUTF16 ident(nsCSSKeywords::GetStringValue(functionId));
    // Bug 721136: Normalize the identifier to lowercase, except that things
    // like scaleX should have the last character capitalized.  This matches
    // what other browsers do.
    switch (functionId) {
      case eCSSKeyword_rotatex:
      case eCSSKeyword_scalex:
      case eCSSKeyword_skewx:
      case eCSSKeyword_translatex:
        ident.Replace(ident.Length() - 1, 1, char16_t('X'));
        break;

      case eCSSKeyword_rotatey:
      case eCSSKeyword_scaley:
      case eCSSKeyword_skewy:
      case eCSSKeyword_translatey:
        ident.Replace(ident.Length() - 1, 1, char16_t('Y'));
        break;

      case eCSSKeyword_rotatez:
      case eCSSKeyword_scalez:
      case eCSSKeyword_translatez:
        ident.Replace(ident.Length() - 1, 1, char16_t('Z'));
        break;

      default:
        break;
    }
    nsStyleUtil::AppendEscapedCSSIdent(ident, aResult);
    aResult.Append('(');

    switch (functionId) {
      case eCSSKeyword_polygon:
        AppendPolygonToString(aProperty, aResult, aSerialization);
        break;

      case eCSSKeyword_circle:
      case eCSSKeyword_ellipse:
        AppendCircleOrEllipseToString(functionId, aProperty, aResult,
                                      aSerialization);
        break;

      case eCSSKeyword_inset:
        AppendInsetToString(aProperty, aResult, aSerialization);
        break;

      default: {
        // Now, step through the function contents, writing each of
        // them as we go.
        for (size_t index = 1; index < array->Count(); ++index) {
          array->Item(index).AppendToString(aProperty, aResult,
                                            aSerialization);

          /* If we're not at the final element, append a comma. */
          if (index + 1 != array->Count())
            aResult.AppendLiteral(", ");
        }
      }
    }

    /* Finally, append the closing parenthesis. */
    aResult.Append(')');
  }
  else if (IsCalcUnit()) {
    MOZ_ASSERT(GetUnit() == eCSSUnit_Calc, "unexpected unit");
    CSSValueSerializeCalcOps ops(aProperty, aResult, aSerialization);
    css::SerializeCalc(*this, ops);
  }
  else if (eCSSUnit_Integer == unit) {
    aResult.AppendInt(GetIntValue(), 10);
  }
  else if (eCSSUnit_Enumerated == unit) {
    int32_t intValue = GetIntValue();
    switch(aProperty) {


    case eCSSProperty_text_combine_upright:
      if (intValue <= NS_STYLE_TEXT_COMBINE_UPRIGHT_ALL) {
        AppendASCIItoUTF16(nsCSSProps::LookupPropertyValue(aProperty, intValue),
                           aResult);
      } else if (intValue == NS_STYLE_TEXT_COMBINE_UPRIGHT_DIGITS_2) {
        aResult.AppendLiteral("digits 2");
      } else if (intValue == NS_STYLE_TEXT_COMBINE_UPRIGHT_DIGITS_3) {
        aResult.AppendLiteral("digits 3");
      } else {
        aResult.AppendLiteral("digits 4");
      }
      break;

    case eCSSProperty_text_decoration_line:
      if (NS_STYLE_TEXT_DECORATION_LINE_NONE == intValue) {
        AppendASCIItoUTF16(nsCSSProps::LookupPropertyValue(aProperty, intValue),
                           aResult);
      } else {
        // Ignore the "override all" internal value.
        // (It doesn't have a string representation.)
        intValue &= ~NS_STYLE_TEXT_DECORATION_LINE_OVERRIDE_ALL;
        nsStyleUtil::AppendBitmaskCSSValue(
          aProperty, intValue,
          NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE,
          NS_STYLE_TEXT_DECORATION_LINE_PREF_ANCHORS,
          aResult);
      }
      break;

    case eCSSProperty_paint_order:
      static_assert
        (NS_STYLE_PAINT_ORDER_BITWIDTH * NS_STYLE_PAINT_ORDER_LAST_VALUE <= 8,
         "SVGStyleStruct::mPaintOrder and the following cast not big enough");
      nsStyleUtil::AppendPaintOrderValue(static_cast<uint8_t>(GetIntValue()),
                                         aResult);
      break;

    case eCSSProperty_font_synthesis:
      nsStyleUtil::AppendBitmaskCSSValue(aProperty, intValue,
                                         NS_FONT_SYNTHESIS_WEIGHT,
                                         NS_FONT_SYNTHESIS_STYLE,
                                         aResult);
      break;

    case eCSSProperty_font_variant_east_asian:
      nsStyleUtil::AppendBitmaskCSSValue(aProperty, intValue,
                                         NS_FONT_VARIANT_EAST_ASIAN_JIS78,
                                         NS_FONT_VARIANT_EAST_ASIAN_RUBY,
                                         aResult);
      break;

    case eCSSProperty_font_variant_ligatures:
      nsStyleUtil::AppendBitmaskCSSValue(aProperty, intValue,
                                         NS_FONT_VARIANT_LIGATURES_NONE,
                                         NS_FONT_VARIANT_LIGATURES_NO_CONTEXTUAL,
                                         aResult);
      break;

    case eCSSProperty_font_variant_numeric:
      nsStyleUtil::AppendBitmaskCSSValue(aProperty, intValue,
                                         NS_FONT_VARIANT_NUMERIC_LINING,
                                         NS_FONT_VARIANT_NUMERIC_ORDINAL,
                                         aResult);
      break;

    case eCSSProperty_grid_auto_flow:
      nsStyleUtil::AppendBitmaskCSSValue(aProperty, intValue,
                                         NS_STYLE_GRID_AUTO_FLOW_ROW,
                                         NS_STYLE_GRID_AUTO_FLOW_DENSE,
                                         aResult);
      break;

    case eCSSProperty_grid_column_start:
    case eCSSProperty_grid_column_end:
    case eCSSProperty_grid_row_start:
    case eCSSProperty_grid_row_end:
      // "span" is the only enumerated-unit value for these properties
      aResult.AppendLiteral("span");
      break;

    case eCSSProperty_touch_action:
      nsStyleUtil::AppendBitmaskCSSValue(aProperty, intValue,
                                         NS_STYLE_TOUCH_ACTION_NONE,
                                         NS_STYLE_TOUCH_ACTION_MANIPULATION,
                                         aResult);
      break;

    case eCSSProperty_clip_path:
      AppendASCIItoUTF16(nsCSSProps::ValueToKeyword(intValue,
                            nsCSSProps::kClipPathGeometryBoxKTable),
                         aResult);
      break;

    case eCSSProperty_shape_outside:
      AppendASCIItoUTF16(nsCSSProps::ValueToKeyword(intValue,
                            nsCSSProps::kShapeOutsideShapeBoxKTable),
                         aResult);
      break;

    case eCSSProperty_contain:
      if (intValue & NS_STYLE_CONTAIN_STRICT) {
        NS_ASSERTION(intValue == (NS_STYLE_CONTAIN_STRICT | NS_STYLE_CONTAIN_ALL_BITS),
                     "contain: strict should imply contain: layout style paint");
        // Only output strict.
        intValue = NS_STYLE_CONTAIN_STRICT;
      }
      nsStyleUtil::AppendBitmaskCSSValue(aProperty,
                                         intValue,
                                         NS_STYLE_CONTAIN_STRICT,
                                         NS_STYLE_CONTAIN_PAINT,
                                         aResult);
      break;

    case eCSSProperty_align_content:
    case eCSSProperty_justify_content: {
      AppendAlignJustifyValueToString(intValue & NS_STYLE_ALIGN_ALL_BITS, aResult);
      auto fallback = intValue >> NS_STYLE_ALIGN_ALL_SHIFT;
      if (fallback) {
        MOZ_ASSERT(nsCSSProps::ValueToKeywordEnum(fallback & ~NS_STYLE_ALIGN_FLAG_BITS,
                                                  nsCSSProps::kAlignSelfPosition)
                   != eCSSKeyword_UNKNOWN, "unknown fallback value");
        aResult.Append(' ');
        AppendAlignJustifyValueToString(fallback, aResult);
      }
      break;
    }

    case eCSSProperty_align_items:
    case eCSSProperty_align_self:
    case eCSSProperty_justify_items:
    case eCSSProperty_justify_self:
      AppendAlignJustifyValueToString(intValue, aResult);
      break;

    case eCSSProperty_text_emphasis_position: {
      nsStyleUtil::AppendBitmaskCSSValue(aProperty, intValue,
                                         NS_STYLE_TEXT_EMPHASIS_POSITION_OVER,
                                         NS_STYLE_TEXT_EMPHASIS_POSITION_RIGHT,
                                         aResult);
      break;
    }

    case eCSSProperty_text_emphasis_style: {
      auto fill = intValue & NS_STYLE_TEXT_EMPHASIS_STYLE_FILL_MASK;
      AppendASCIItoUTF16(nsCSSProps::ValueToKeyword(
        fill, nsCSSProps::kTextEmphasisStyleFillKTable), aResult);
      aResult.Append(' ');
      auto shape = intValue & NS_STYLE_TEXT_EMPHASIS_STYLE_SHAPE_MASK;
      AppendASCIItoUTF16(nsCSSProps::ValueToKeyword(
        shape, nsCSSProps::kTextEmphasisStyleShapeKTable), aResult);
      break;
    }

    default:
      const nsAFlatCString& name = nsCSSProps::LookupPropertyValue(aProperty, intValue);
      AppendASCIItoUTF16(name, aResult);
      break;
    }
  }
  else if (eCSSUnit_EnumColor == unit) {
    // we can lookup the property in the ColorTable and then
    // get a string mapping the name
    nsAutoCString str;
    if (nsCSSProps::GetColorName(GetIntValue(), str)){
      AppendASCIItoUTF16(str, aResult);
    } else {
      MOZ_ASSERT(false, "bad color value");
    }
  }
  else if (IsNumericColorUnit(unit)) {
    if (aSerialization == eNormalized ||
        unit == eCSSUnit_RGBColor ||
        unit == eCSSUnit_RGBAColor) {
      nscolor color = GetColorValue();
      if (aSerialization == eNormalized &&
          color == NS_RGBA(0, 0, 0, 0)) {
        // Use the strictest match for 'transparent' so we do correct
        // round-tripping of all other rgba() values.
        aResult.AppendLiteral("transparent");
      } else {
        // For brevity, we omit the alpha component if it's equal to 255 (full
        // opaque). Also, we try to preserve the author-specified function name,
        // unless it's rgba() and we're omitting the alpha component - then we
        // use rgb().
        uint8_t a = NS_GET_A(color);
        bool showAlpha = (a != 255);

        if (unit == eCSSUnit_RGBAColor && showAlpha) {
          aResult.AppendLiteral("rgba(");
        } else {
          aResult.AppendLiteral("rgb(");
        }

        NS_NAMED_LITERAL_STRING(comma, ", ");

        aResult.AppendInt(NS_GET_R(color), 10);
        aResult.Append(comma);
        aResult.AppendInt(NS_GET_G(color), 10);
        aResult.Append(comma);
        aResult.AppendInt(NS_GET_B(color), 10);
        if (showAlpha) {
          aResult.Append(comma);
          aResult.AppendFloat(nsStyleUtil::ColorComponentToFloat(a));
        }
        aResult.Append(char16_t(')'));
      }
    } else if (eCSSUnit_HexColor == unit ||
               eCSSUnit_HexColorAlpha == unit) {
      nscolor color = GetColorValue();
      aResult.Append('#');
      aResult.AppendPrintf("%02x", NS_GET_R(color));
      aResult.AppendPrintf("%02x", NS_GET_G(color));
      aResult.AppendPrintf("%02x", NS_GET_B(color));
      if (eCSSUnit_HexColorAlpha == unit) {
        aResult.AppendPrintf("%02x", NS_GET_A(color));
      }
    } else if (eCSSUnit_ShortHexColor == unit ||
               eCSSUnit_ShortHexColorAlpha == unit) {
      nscolor color = GetColorValue();
      aResult.Append('#');
      aResult.AppendInt(NS_GET_R(color) / 0x11, 16);
      aResult.AppendInt(NS_GET_G(color) / 0x11, 16);
      aResult.AppendInt(NS_GET_B(color) / 0x11, 16);
      if (eCSSUnit_ShortHexColorAlpha == unit) {
        aResult.AppendInt(NS_GET_A(color) / 0x11, 16);
      }
    } else {
      MOZ_ASSERT(IsFloatColorUnit());
      mValue.mFloatColor->AppendToString(unit, aResult);
    }
  }
  else if (eCSSUnit_ComplexColor == unit) {
    StyleComplexColor color = GetStyleComplexColorValue();
    nsCSSValue serializable;
    if (color.IsCurrentColor()) {
      serializable.SetIntValue(NS_COLOR_CURRENTCOLOR, eCSSUnit_EnumColor);
    } else if (color.IsNumericColor()) {
      serializable.SetColorValue(color.mColor);
    } else {
      MOZ_ASSERT_UNREACHABLE("Cannot serialize a complex color");
    }
    serializable.AppendToString(aProperty, aResult, aSerialization);
  }
  else if (eCSSUnit_URL == unit || eCSSUnit_Image == unit) {
    aResult.AppendLiteral("url(");
    nsStyleUtil::AppendEscapedCSSString(
      nsDependentString(GetOriginalURLValue()), aResult);
    aResult.Append(')');
  }
  else if (eCSSUnit_Element == unit) {
    aResult.AppendLiteral("-moz-element(#");
    nsAutoString tmpStr;
    GetStringValue(tmpStr);
    nsStyleUtil::AppendEscapedCSSIdent(tmpStr, aResult);
    aResult.Append(')');
  }
  else if (eCSSUnit_Percent == unit) {
    aResult.AppendFloat(GetPercentValue() * 100.0f);
  }
  else if (eCSSUnit_Percent < unit) {  // length unit
    aResult.AppendFloat(GetFloatValue());
  }
  else if (eCSSUnit_Gradient == unit) {
    nsCSSValueGradient* gradient = GetGradientValue();

    if (gradient->mIsLegacySyntax) {
      aResult.AppendLiteral("-moz-");
    }
    if (gradient->mIsRepeating) {
      aResult.AppendLiteral("repeating-");
    }
    if (gradient->mIsRadial) {
      aResult.AppendLiteral("radial-gradient(");
    } else {
      aResult.AppendLiteral("linear-gradient(");
    }

    bool needSep = false;
    if (gradient->mIsRadial && !gradient->mIsLegacySyntax) {
      if (!gradient->mIsExplicitSize) {
        if (gradient->GetRadialShape().GetUnit() != eCSSUnit_None) {
          MOZ_ASSERT(gradient->GetRadialShape().GetUnit() ==
                     eCSSUnit_Enumerated,
                     "bad unit for radial gradient shape");
          int32_t intValue = gradient->GetRadialShape().GetIntValue();
          MOZ_ASSERT(intValue != NS_STYLE_GRADIENT_SHAPE_LINEAR,
                     "radial gradient with linear shape?!");
          AppendASCIItoUTF16(nsCSSProps::ValueToKeyword(intValue,
                                 nsCSSProps::kRadialGradientShapeKTable),
                             aResult);
          needSep = true;
        }

        if (gradient->GetRadialSize().GetUnit() != eCSSUnit_None) {
          if (needSep) {
            aResult.Append(' ');
          }
          MOZ_ASSERT(gradient->GetRadialSize().GetUnit() == eCSSUnit_Enumerated,
                     "bad unit for radial gradient size");
          int32_t intValue = gradient->GetRadialSize().GetIntValue();
          AppendASCIItoUTF16(nsCSSProps::ValueToKeyword(intValue,
                                 nsCSSProps::kRadialGradientSizeKTable),
                             aResult);
          needSep = true;
        }
      } else {
        MOZ_ASSERT(gradient->GetRadiusX().GetUnit() != eCSSUnit_None,
                   "bad unit for radial gradient explicit size");
        gradient->GetRadiusX().AppendToString(aProperty, aResult,
                                              aSerialization);
        if (gradient->GetRadiusY().GetUnit() != eCSSUnit_None) {
          aResult.Append(' ');
          gradient->GetRadiusY().AppendToString(aProperty, aResult,
                                                aSerialization);
        }
        needSep = true;
      }
    }
    if (!gradient->mIsRadial && !gradient->mIsLegacySyntax) {
      if (gradient->mBgPos.mXValue.GetUnit() != eCSSUnit_None ||
          gradient->mBgPos.mYValue.GetUnit() != eCSSUnit_None) {
        MOZ_ASSERT(gradient->mAngle.GetUnit() == eCSSUnit_None);
        MOZ_ASSERT(gradient->mBgPos.mXValue.GetUnit() == eCSSUnit_Enumerated &&
                   gradient->mBgPos.mYValue.GetUnit() == eCSSUnit_Enumerated,
                   "unexpected unit");
        aResult.AppendLiteral("to");
        if (!(gradient->mBgPos.mXValue.GetIntValue() & NS_STYLE_IMAGELAYER_POSITION_CENTER)) {
          aResult.Append(' ');
          gradient->mBgPos.mXValue.AppendToString(eCSSProperty_background_position_x,
                                                  aResult, aSerialization);
        }
        if (!(gradient->mBgPos.mYValue.GetIntValue() & NS_STYLE_IMAGELAYER_POSITION_CENTER)) {
          aResult.Append(' ');
          gradient->mBgPos.mYValue.AppendToString(eCSSProperty_background_position_y,
                                                  aResult, aSerialization);
        }
        needSep = true;
      } else if (gradient->mAngle.GetUnit() != eCSSUnit_None) {
        gradient->mAngle.AppendToString(aProperty, aResult, aSerialization);
        needSep = true;
      }
    } else if (gradient->mBgPos.mXValue.GetUnit() != eCSSUnit_None ||
        gradient->mBgPos.mYValue.GetUnit() != eCSSUnit_None ||
        gradient->mAngle.GetUnit() != eCSSUnit_None) {
      if (needSep) {
        aResult.Append(' ');
      }
      if (gradient->mIsRadial && !gradient->mIsLegacySyntax) {
        aResult.AppendLiteral("at ");
      }
      if (gradient->mBgPos.mXValue.GetUnit() != eCSSUnit_None) {
        gradient->mBgPos.mXValue.AppendToString(eCSSProperty_background_position_x,
                                                aResult, aSerialization);
        aResult.Append(' ');
      }
      if (gradient->mBgPos.mYValue.GetUnit() != eCSSUnit_None) {
        gradient->mBgPos.mYValue.AppendToString(eCSSProperty_background_position_y,
                                                aResult, aSerialization);
        aResult.Append(' ');
      }
      if (gradient->mAngle.GetUnit() != eCSSUnit_None) {
        MOZ_ASSERT(gradient->mIsLegacySyntax,
                   "angle is allowed only for legacy syntax");
        gradient->mAngle.AppendToString(aProperty, aResult, aSerialization);
      }
      needSep = true;
    }

    if (gradient->mIsRadial && gradient->mIsLegacySyntax &&
        (gradient->GetRadialShape().GetUnit() != eCSSUnit_None ||
         gradient->GetRadialSize().GetUnit() != eCSSUnit_None)) {
      MOZ_ASSERT(!gradient->mIsExplicitSize);
      if (needSep) {
        aResult.AppendLiteral(", ");
      }
      if (gradient->GetRadialShape().GetUnit() != eCSSUnit_None) {
        MOZ_ASSERT(gradient->GetRadialShape().GetUnit() == eCSSUnit_Enumerated,
                   "bad unit for radial gradient shape");
        int32_t intValue = gradient->GetRadialShape().GetIntValue();
        MOZ_ASSERT(intValue != NS_STYLE_GRADIENT_SHAPE_LINEAR,
                   "radial gradient with linear shape?!");
        AppendASCIItoUTF16(nsCSSProps::ValueToKeyword(intValue,
                               nsCSSProps::kRadialGradientShapeKTable),
                           aResult);
        aResult.Append(' ');
      }

      if (gradient->GetRadialSize().GetUnit() != eCSSUnit_None) {
        MOZ_ASSERT(gradient->GetRadialSize().GetUnit() == eCSSUnit_Enumerated,
                   "bad unit for radial gradient size");
        int32_t intValue = gradient->GetRadialSize().GetIntValue();
        AppendASCIItoUTF16(nsCSSProps::ValueToKeyword(intValue,
                               nsCSSProps::kRadialGradientSizeKTable),
                           aResult);
      }
      needSep = true;
    }
    if (needSep) {
      aResult.AppendLiteral(", ");
    }

    for (uint32_t i = 0 ;;) {
      bool isInterpolationHint = gradient->mStops[i].mIsInterpolationHint;
      if (!isInterpolationHint) {
        gradient->mStops[i].mColor.AppendToString(aProperty, aResult,
                                                  aSerialization);
      }
      if (gradient->mStops[i].mLocation.GetUnit() != eCSSUnit_None) {
        if (!isInterpolationHint) {
          aResult.Append(' ');
        }
        gradient->mStops[i].mLocation.AppendToString(aProperty, aResult,
                                                     aSerialization);
      }
      if (++i == gradient->mStops.Length()) {
        break;
      }
      aResult.AppendLiteral(", ");
    }

    aResult.Append(')');
  } else if (eCSSUnit_TokenStream == unit) {
    nsCSSPropertyID shorthand = mValue.mTokenStream->mShorthandPropertyID;
    if (shorthand == eCSSProperty_UNKNOWN ||
        nsCSSProps::PropHasFlags(shorthand, CSS_PROPERTY_IS_ALIAS) ||
        aProperty == eCSSProperty__x_system_font) {
      // We treat serialization of aliases like '-moz-transform' as a special
      // case, since it really wants to be serialized as if it were a longhand
      // even though it is implemented as a shorthand. We also need to
      // serialize -x-system-font's token stream value, even though the
      // value is set through the font shorthand.  This serialization
      // of -x-system-font is needed when we need to output the
      // 'font' shorthand followed by a number of overriding font
      // longhand components.
      aResult.Append(mValue.mTokenStream->mTokenStream);
    }
  } else if (eCSSUnit_Pair == unit) {
    if (eCSSProperty_font_variant_alternates == aProperty) {
      int32_t intValue = GetPairValue().mXValue.GetIntValue();
      nsAutoString out;

      // simple, enumerated values
      nsStyleUtil::AppendBitmaskCSSValue(aProperty,
          intValue & NS_FONT_VARIANT_ALTERNATES_ENUMERATED_MASK,
          NS_FONT_VARIANT_ALTERNATES_HISTORICAL,
          NS_FONT_VARIANT_ALTERNATES_HISTORICAL,
          out);

      // functional values
      const nsCSSValueList *list = GetPairValue().mYValue.GetListValue();
      AutoTArray<gfxAlternateValue,8> altValues;

      nsStyleUtil::ComputeFunctionalAlternates(list, altValues);
      nsStyleUtil::SerializeFunctionalAlternates(altValues, out);
      aResult.Append(out);
    } else {
      GetPairValue().AppendToString(aProperty, aResult, aSerialization);
    }
  } else if (eCSSUnit_Triplet == unit) {
    GetTripletValue().AppendToString(aProperty, aResult, aSerialization);
  } else if (eCSSUnit_Rect == unit) {
    GetRectValue().AppendToString(aProperty, aResult, aSerialization);
  } else if (eCSSUnit_List == unit || eCSSUnit_ListDep == unit) {
    GetListValue()->AppendToString(aProperty, aResult, aSerialization);
  } else if (eCSSUnit_SharedList == unit) {
    GetSharedListValue()->AppendToString(aProperty, aResult, aSerialization);
  } else if (eCSSUnit_PairList == unit || eCSSUnit_PairListDep == unit) {
    switch (aProperty) {
      case eCSSProperty_font_feature_settings:
        nsStyleUtil::AppendFontFeatureSettings(*this, aResult);
        break;
      default:
        GetPairListValue()->AppendToString(aProperty, aResult, aSerialization);
        break;
    }
  } else if (eCSSUnit_GridTemplateAreas == unit) {
    const mozilla::css::GridTemplateAreasValue* areas = GetGridTemplateAreas();
    MOZ_ASSERT(!areas->mTemplates.IsEmpty(),
               "Unexpected empty array in GridTemplateAreasValue");
    nsStyleUtil::AppendEscapedCSSString(areas->mTemplates[0], aResult);
    for (uint32_t i = 1; i < areas->mTemplates.Length(); i++) {
      aResult.Append(char16_t(' '));
      nsStyleUtil::AppendEscapedCSSString(areas->mTemplates[i], aResult);
    }
  } else if (eCSSUnit_FontFamilyList == unit) {
    nsStyleUtil::AppendEscapedCSSFontFamilyList(*mValue.mFontFamilyList,
                                                aResult);
  }

  switch (unit) {
    case eCSSUnit_Null:         break;
    case eCSSUnit_Auto:         aResult.AppendLiteral("auto");     break;
    case eCSSUnit_Inherit:      aResult.AppendLiteral("inherit");  break;
    case eCSSUnit_Initial:      aResult.AppendLiteral("initial");  break;
    case eCSSUnit_Unset:        aResult.AppendLiteral("unset");    break;
    case eCSSUnit_None:         aResult.AppendLiteral("none");     break;
    case eCSSUnit_Normal:       aResult.AppendLiteral("normal");   break;
    case eCSSUnit_System_Font:  aResult.AppendLiteral("-moz-use-system-font"); break;
    case eCSSUnit_All:          aResult.AppendLiteral("all"); break;
    case eCSSUnit_Dummy:
    case eCSSUnit_DummyInherit:
      MOZ_ASSERT(false, "should never serialize");
      break;

    case eCSSUnit_FontFamilyList: break;
    case eCSSUnit_String:       break;
    case eCSSUnit_Ident:        break;
    case eCSSUnit_URL:          break;
    case eCSSUnit_Image:        break;
    case eCSSUnit_Element:      break;
    case eCSSUnit_Array:        break;
    case eCSSUnit_Attr:
    case eCSSUnit_Cubic_Bezier:
    case eCSSUnit_Steps:
    case eCSSUnit_Symbols:
    case eCSSUnit_Counter:
    case eCSSUnit_Counters:     aResult.Append(char16_t(')'));    break;
    case eCSSUnit_Local_Font:   break;
    case eCSSUnit_Font_Format:  break;
    case eCSSUnit_Function:     break;
    case eCSSUnit_Calc:         break;
    case eCSSUnit_Calc_Plus:    break;
    case eCSSUnit_Calc_Minus:   break;
    case eCSSUnit_Calc_Times_L: break;
    case eCSSUnit_Calc_Times_R: break;
    case eCSSUnit_Calc_Divided: break;
    case eCSSUnit_Integer:      break;
    case eCSSUnit_Enumerated:   break;
    case eCSSUnit_EnumColor:             break;
    case eCSSUnit_RGBColor:              break;
    case eCSSUnit_RGBAColor:             break;
    case eCSSUnit_HexColor:              break;
    case eCSSUnit_ShortHexColor:         break;
    case eCSSUnit_HexColorAlpha:         break;
    case eCSSUnit_ShortHexColorAlpha:    break;
    case eCSSUnit_PercentageRGBColor:    break;
    case eCSSUnit_PercentageRGBAColor:   break;
    case eCSSUnit_HSLColor:              break;
    case eCSSUnit_HSLAColor:             break;
    case eCSSUnit_ComplexColor:          break;
    case eCSSUnit_Percent:      aResult.Append(char16_t('%'));    break;
    case eCSSUnit_Number:       break;
    case eCSSUnit_Gradient:     break;
    case eCSSUnit_TokenStream:  break;
    case eCSSUnit_Pair:         break;
    case eCSSUnit_Triplet:      break;
    case eCSSUnit_Rect:         break;
    case eCSSUnit_List:         break;
    case eCSSUnit_ListDep:      break;
    case eCSSUnit_SharedList:   break;
    case eCSSUnit_PairList:     break;
    case eCSSUnit_PairListDep:  break;
    case eCSSUnit_GridTemplateAreas:     break;

    case eCSSUnit_Inch:         aResult.AppendLiteral("in");   break;
    case eCSSUnit_Millimeter:   aResult.AppendLiteral("mm");   break;
    case eCSSUnit_PhysicalMillimeter: aResult.AppendLiteral("mozmm");   break;
    case eCSSUnit_Centimeter:   aResult.AppendLiteral("cm");   break;
    case eCSSUnit_Point:        aResult.AppendLiteral("pt");   break;
    case eCSSUnit_Pica:         aResult.AppendLiteral("pc");   break;
    case eCSSUnit_Quarter:      aResult.AppendLiteral("q");    break;

    case eCSSUnit_ViewportWidth:  aResult.AppendLiteral("vw");   break;
    case eCSSUnit_ViewportHeight: aResult.AppendLiteral("vh");   break;
    case eCSSUnit_ViewportMin:    aResult.AppendLiteral("vmin"); break;
    case eCSSUnit_ViewportMax:    aResult.AppendLiteral("vmax"); break;

    case eCSSUnit_EM:           aResult.AppendLiteral("em");   break;
    case eCSSUnit_XHeight:      aResult.AppendLiteral("ex");   break;
    case eCSSUnit_Char:         aResult.AppendLiteral("ch");   break;
    case eCSSUnit_RootEM:       aResult.AppendLiteral("rem");  break;

    case eCSSUnit_Pixel:        aResult.AppendLiteral("px");   break;

    case eCSSUnit_Degree:       aResult.AppendLiteral("deg");  break;
    case eCSSUnit_Grad:         aResult.AppendLiteral("grad"); break;
    case eCSSUnit_Radian:       aResult.AppendLiteral("rad");  break;
    case eCSSUnit_Turn:         aResult.AppendLiteral("turn");  break;

    case eCSSUnit_Hertz:        aResult.AppendLiteral("Hz");   break;
    case eCSSUnit_Kilohertz:    aResult.AppendLiteral("kHz");  break;

    case eCSSUnit_Seconds:      aResult.Append(char16_t('s'));    break;
    case eCSSUnit_Milliseconds: aResult.AppendLiteral("ms");   break;

    case eCSSUnit_FlexFraction: aResult.AppendLiteral("fr");   break;
  }
}

size_t
nsCSSValue::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;

  switch (GetUnit()) {
    // No value: nothing extra to measure.
    case eCSSUnit_Null:
    case eCSSUnit_Auto:
    case eCSSUnit_Inherit:
    case eCSSUnit_Initial:
    case eCSSUnit_Unset:
    case eCSSUnit_None:
    case eCSSUnit_Normal:
    case eCSSUnit_System_Font:
    case eCSSUnit_All:
    case eCSSUnit_Dummy:
    case eCSSUnit_DummyInherit:
      break;

    // String
    case eCSSUnit_String:
    case eCSSUnit_Ident:
    case eCSSUnit_Attr:
    case eCSSUnit_Local_Font:
    case eCSSUnit_Font_Format:
    case eCSSUnit_Element:
      n += mValue.mString->SizeOfIncludingThisIfUnshared(aMallocSizeOf);
      break;

    // Array
    case eCSSUnit_Array:
    case eCSSUnit_Counter:
    case eCSSUnit_Counters:
    case eCSSUnit_Cubic_Bezier:
    case eCSSUnit_Steps:
    case eCSSUnit_Symbols:
    case eCSSUnit_Function:
    case eCSSUnit_Calc:
    case eCSSUnit_Calc_Plus:
    case eCSSUnit_Calc_Minus:
    case eCSSUnit_Calc_Times_L:
    case eCSSUnit_Calc_Times_R:
    case eCSSUnit_Calc_Divided:
      break;

    // URL
    case eCSSUnit_URL:
      n += mValue.mURL->SizeOfIncludingThis(aMallocSizeOf);
      break;

    // Image
    case eCSSUnit_Image:
      // Not yet measured.  Measurement may be added later if DMD finds it
      // worthwhile.
      break;

    // Gradient
    case eCSSUnit_Gradient:
      n += mValue.mGradient->SizeOfIncludingThis(aMallocSizeOf);
      break;

    // TokenStream
    case eCSSUnit_TokenStream:
      n += mValue.mTokenStream->SizeOfIncludingThis(aMallocSizeOf);
      break;

    // Pair
    case eCSSUnit_Pair:
      n += mValue.mPair->SizeOfIncludingThis(aMallocSizeOf);
      break;

    // Triplet
    case eCSSUnit_Triplet:
      n += mValue.mTriplet->SizeOfIncludingThis(aMallocSizeOf);
      break;

    // Rect
    case eCSSUnit_Rect:
      n += mValue.mRect->SizeOfIncludingThis(aMallocSizeOf);
      break;

    // List
    case eCSSUnit_List:
      n += mValue.mList->SizeOfIncludingThis(aMallocSizeOf);
      break;

    // ListDep: not measured because it's non-owning.
    case eCSSUnit_ListDep:
      break;

    // SharedList
    case eCSSUnit_SharedList:
      // Makes more sense not to measure, since it most cases the list
      // will be shared.
      break;

    // PairList
    case eCSSUnit_PairList:
      n += mValue.mPairList->SizeOfIncludingThis(aMallocSizeOf);
      break;

    // PairListDep: not measured because it's non-owning.
    case eCSSUnit_PairListDep:
      break;

    // GridTemplateAreas
    case eCSSUnit_GridTemplateAreas:
      n += mValue.mGridTemplateAreas->SizeOfIncludingThis(aMallocSizeOf);
      break;

    case eCSSUnit_FontFamilyList:
      n += mValue.mFontFamilyList->SizeOfIncludingThis(aMallocSizeOf);
      break;

    // Int: nothing extra to measure.
    case eCSSUnit_Integer:
    case eCSSUnit_Enumerated:
    case eCSSUnit_EnumColor:
      break;

    // Integer Color: nothing extra to measure.
    case eCSSUnit_RGBColor:
    case eCSSUnit_RGBAColor:
    case eCSSUnit_HexColor:
    case eCSSUnit_ShortHexColor:
    case eCSSUnit_HexColorAlpha:
    case eCSSUnit_ShortHexColorAlpha:
      break;

    // Float Color
    case eCSSUnit_PercentageRGBColor:
    case eCSSUnit_PercentageRGBAColor:
    case eCSSUnit_HSLColor:
    case eCSSUnit_HSLAColor:
      n += mValue.mFloatColor->SizeOfIncludingThis(aMallocSizeOf);
      break;

    // Complex Color
    case eCSSUnit_ComplexColor:
      n += mValue.mComplexColor->SizeOfIncludingThis(aMallocSizeOf);
      break;

    // Float: nothing extra to measure.
    case eCSSUnit_Percent:
    case eCSSUnit_Number:
    case eCSSUnit_PhysicalMillimeter:
    case eCSSUnit_ViewportWidth:
    case eCSSUnit_ViewportHeight:
    case eCSSUnit_ViewportMin:
    case eCSSUnit_ViewportMax:
    case eCSSUnit_EM:
    case eCSSUnit_XHeight:
    case eCSSUnit_Char:
    case eCSSUnit_RootEM:
    case eCSSUnit_Point:
    case eCSSUnit_Inch:
    case eCSSUnit_Millimeter:
    case eCSSUnit_Centimeter:
    case eCSSUnit_Pica:
    case eCSSUnit_Pixel:
    case eCSSUnit_Quarter:
    case eCSSUnit_Degree:
    case eCSSUnit_Grad:
    case eCSSUnit_Turn:
    case eCSSUnit_Radian:
    case eCSSUnit_Hertz:
    case eCSSUnit_Kilohertz:
    case eCSSUnit_Seconds:
    case eCSSUnit_Milliseconds:
    case eCSSUnit_FlexFraction:
      break;

    default:
      MOZ_ASSERT(false, "bad nsCSSUnit");
      break;
  }

  return n;
}

// --- nsCSSValueList -----------------

nsCSSValueList::~nsCSSValueList()
{
  MOZ_COUNT_DTOR(nsCSSValueList);
  NS_CSS_DELETE_LIST_MEMBER(nsCSSValueList, this, mNext);
}

nsCSSValueList*
nsCSSValueList::Clone() const
{
  nsCSSValueList* result = new nsCSSValueList(*this);
  nsCSSValueList* dest = result;
  const nsCSSValueList* src = this->mNext;
  while (src) {
    dest->mNext = new nsCSSValueList(*src);
    dest = dest->mNext;
    src = src->mNext;
  }

  MOZ_ASSERT(result, "shouldn't return null; supposed to be infallible");
  return result;
}

void
nsCSSValueList::CloneInto(nsCSSValueList* aList) const
{
  NS_ASSERTION(!aList->mNext, "Must be an empty list!");
  aList->mValue = mValue;
  aList->mNext = mNext ? mNext->Clone() : nullptr;
}

static void
AppendValueListToString(const nsCSSValueList* val,
                        nsCSSPropertyID aProperty, nsAString& aResult,
                        nsCSSValue::Serialization aSerialization)
{
  for (;;) {
    val->mValue.AppendToString(aProperty, aResult, aSerialization);
    val = val->mNext;
    if (!val)
      break;

    if (nsCSSProps::PropHasFlags(aProperty,
                                 CSS_PROPERTY_VALUE_LIST_USES_COMMAS))
      aResult.Append(char16_t(','));
    aResult.Append(char16_t(' '));
  }
}

static void
AppendGridTemplateToString(const nsCSSValueList* val,
                           nsCSSPropertyID aProperty, nsAString& aResult,
                           nsCSSValue::Serialization aSerialization)
{
  // This is called for the "list" that's the top-level value of the property.
  bool isSubgrid = false;
  for (;;) {
    bool addSpaceSeparator = true;
    nsCSSUnit unit = val->mValue.GetUnit();

    if (unit == eCSSUnit_Enumerated &&
        val->mValue.GetIntValue() == NS_STYLE_GRID_TEMPLATE_SUBGRID) {
      MOZ_ASSERT(!isSubgrid, "saw subgrid once already");
      isSubgrid = true;
      aResult.AppendLiteral("subgrid");

    } else if (unit == eCSSUnit_Pair) {
      // This is a repeat 'auto-fill' / 'auto-fit'.
      const nsCSSValuePair& pair = val->mValue.GetPairValue();
      switch (pair.mXValue.GetIntValue()) {
        case NS_STYLE_GRID_REPEAT_AUTO_FILL:
          aResult.AppendLiteral("repeat(auto-fill, ");
          break;
        case NS_STYLE_GRID_REPEAT_AUTO_FIT:
          aResult.AppendLiteral("repeat(auto-fit, ");
          break;
        default:
          MOZ_ASSERT_UNREACHABLE("unexpected enum value");
      }
      const nsCSSValueList* repeatList = pair.mYValue.GetListValue();
      if (repeatList->mValue.GetUnit() != eCSSUnit_Null) {
        aResult.Append('[');
        AppendValueListToString(repeatList->mValue.GetListValue(), aProperty,
                                aResult, aSerialization);
        aResult.Append(']');
        if (!isSubgrid) {
          aResult.Append(' ');
        }
      } else if (isSubgrid) {
        aResult.AppendLiteral("[]");
      }
      if (!isSubgrid) {
        repeatList = repeatList->mNext;
        repeatList->mValue.AppendToString(aProperty, aResult, aSerialization);
        repeatList = repeatList->mNext;
        if (repeatList->mValue.GetUnit() != eCSSUnit_Null) {
          aResult.AppendLiteral(" [");
          AppendValueListToString(repeatList->mValue.GetListValue(), aProperty,
                                  aResult, aSerialization);
          aResult.Append(']');
        }
      }
      aResult.Append(')');

    } else if (unit == eCSSUnit_Null) {
      // Empty or omitted <line-names>.
      if (isSubgrid) {
        aResult.AppendLiteral("[]");
      } else {
        // Serializes to nothing.
        addSpaceSeparator = false;  // Avoid a double space.
      }

    } else if (unit == eCSSUnit_List || unit == eCSSUnit_ListDep) {
      // Non-empty <line-names>
      aResult.Append('[');
      AppendValueListToString(val->mValue.GetListValue(), aProperty,
                              aResult, aSerialization);
      aResult.Append(']');

    } else {
      // <track-size>
      val->mValue.AppendToString(aProperty, aResult, aSerialization);
      if (!isSubgrid &&
          val->mNext &&
          val->mNext->mValue.GetUnit() == eCSSUnit_Null &&
          !val->mNext->mNext) {
        // Break out of the loop early to avoid a trailing space.
        break;
      }
    }

    val = val->mNext;
    if (!val) {
      break;
    }

    if (addSpaceSeparator) {
      aResult.Append(char16_t(' '));
    }
  }
}

void
nsCSSValueList::AppendToString(nsCSSPropertyID aProperty, nsAString& aResult,
                               nsCSSValue::Serialization aSerialization) const
{
  if (aProperty == eCSSProperty_grid_template_columns ||
      aProperty == eCSSProperty_grid_template_rows) {
    AppendGridTemplateToString(this, aProperty, aResult, aSerialization);
  } else {
    AppendValueListToString(this, aProperty, aResult, aSerialization);
  }
}

/* static */ bool
nsCSSValueList::Equal(const nsCSSValueList* aList1,
                      const nsCSSValueList* aList2)
{
  if (aList1 == aList2) {
    return true;
  }

  const nsCSSValueList *p1 = aList1, *p2 = aList2;
  for ( ; p1 && p2; p1 = p1->mNext, p2 = p2->mNext) {
    if (p1->mValue != p2->mValue)
      return false;
  }
  return !p1 && !p2; // true if same length, false otherwise
}

size_t
nsCSSValueList::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;
  const nsCSSValueList* v = this;
  while (v) {
    n += aMallocSizeOf(v);
    n += v->mValue.SizeOfExcludingThis(aMallocSizeOf);
    v = v->mNext;
  }
  return n;
}

size_t
nsCSSValueList_heap::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  // Only measure it if it's unshared, to avoid double-counting.
  size_t n = 0;
  if (mRefCnt <= 1) {
    n += aMallocSizeOf(this);
    n += mValue.SizeOfExcludingThis(aMallocSizeOf);
    n += mNext ? mNext->SizeOfIncludingThis(aMallocSizeOf) : 0;
  }
  return n;
}

// --- nsCSSValueSharedList -----------------

nsCSSValueSharedList::~nsCSSValueSharedList()
{
  if (mHead) {
    NS_CSS_DELETE_LIST_MEMBER(nsCSSValueList, mHead, mNext);
    delete mHead;
  }
}

void
nsCSSValueSharedList::AppendToString(nsCSSPropertyID aProperty, nsAString& aResult,
                                     nsCSSValue::Serialization aSerialization) const
{
  if (mHead) {
    mHead->AppendToString(aProperty, aResult, aSerialization);
  }
}

bool
nsCSSValueSharedList::operator==(const nsCSSValueSharedList& aOther) const
{
  return nsCSSValueList::Equal(mHead, aOther.mHead);
}

size_t
nsCSSValueSharedList::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  // Only measure it if it's unshared, to avoid double-counting.
  size_t n = 0;
  if (mRefCnt <= 1) {
    n += aMallocSizeOf(this);
    n += mHead->SizeOfIncludingThis(aMallocSizeOf);
  }
  return n;
}

// --- nsCSSRect -----------------

nsCSSRect::nsCSSRect(void)
{
  MOZ_COUNT_CTOR(nsCSSRect);
}

nsCSSRect::nsCSSRect(const nsCSSRect& aCopy)
  : mTop(aCopy.mTop),
    mRight(aCopy.mRight),
    mBottom(aCopy.mBottom),
    mLeft(aCopy.mLeft)
{
  MOZ_COUNT_CTOR(nsCSSRect);
}

nsCSSRect::~nsCSSRect()
{
  MOZ_COUNT_DTOR(nsCSSRect);
}

void
nsCSSRect::AppendToString(nsCSSPropertyID aProperty, nsAString& aResult,
                          nsCSSValue::Serialization aSerialization) const
{
  MOZ_ASSERT(mTop.GetUnit() != eCSSUnit_Null &&
             mTop.GetUnit() != eCSSUnit_Inherit &&
             mTop.GetUnit() != eCSSUnit_Initial &&
             mTop.GetUnit() != eCSSUnit_Unset,
             "parser should have used a bare value");

  if (eCSSProperty_border_image_slice == aProperty ||
      eCSSProperty_border_image_width == aProperty ||
      eCSSProperty_border_image_outset == aProperty ||
      eCSSProperty_DOM == aProperty) {
    NS_NAMED_LITERAL_STRING(space, " ");

    mTop.AppendToString(aProperty, aResult, aSerialization);
    aResult.Append(space);
    mRight.AppendToString(aProperty, aResult, aSerialization);
    aResult.Append(space);
    mBottom.AppendToString(aProperty, aResult, aSerialization);
    aResult.Append(space);
    mLeft.AppendToString(aProperty, aResult, aSerialization);
  } else {
    NS_NAMED_LITERAL_STRING(comma, ", ");

    aResult.AppendLiteral("rect(");
    mTop.AppendToString(aProperty, aResult, aSerialization);
    aResult.Append(comma);
    mRight.AppendToString(aProperty, aResult, aSerialization);
    aResult.Append(comma);
    mBottom.AppendToString(aProperty, aResult, aSerialization);
    aResult.Append(comma);
    mLeft.AppendToString(aProperty, aResult, aSerialization);
    aResult.Append(char16_t(')'));
  }
}

void nsCSSRect::SetAllSidesTo(const nsCSSValue& aValue)
{
  mTop = aValue;
  mRight = aValue;
  mBottom = aValue;
  mLeft = aValue;
}

size_t
nsCSSRect_heap::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  // Only measure it if it's unshared, to avoid double-counting.
  size_t n = 0;
  if (mRefCnt <= 1) {
    n += aMallocSizeOf(this);
    n += mTop   .SizeOfExcludingThis(aMallocSizeOf);
    n += mRight .SizeOfExcludingThis(aMallocSizeOf);
    n += mBottom.SizeOfExcludingThis(aMallocSizeOf);
    n += mLeft  .SizeOfExcludingThis(aMallocSizeOf);
  }
  return n;
}

static_assert(eSideTop == 0 && eSideRight == 1 &&
              eSideBottom == 2 && eSideLeft == 3,
              "box side constants not top/right/bottom/left == 0/1/2/3");

/* static */ const nsCSSRect::side_type nsCSSRect::sides[4] = {
  &nsCSSRect::mTop,
  &nsCSSRect::mRight,
  &nsCSSRect::mBottom,
  &nsCSSRect::mLeft,
};

// --- nsCSSValuePair -----------------

void
nsCSSValuePair::AppendToString(nsCSSPropertyID aProperty,
                               nsAString& aResult,
                               nsCSSValue::Serialization aSerialization) const
{
  mXValue.AppendToString(aProperty, aResult, aSerialization);
  if (mYValue.GetUnit() != eCSSUnit_Null) {
    aResult.Append(char16_t(' '));
    mYValue.AppendToString(aProperty, aResult, aSerialization);
  }
}

size_t
nsCSSValuePair::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;
  n += mXValue.SizeOfExcludingThis(aMallocSizeOf);
  n += mYValue.SizeOfExcludingThis(aMallocSizeOf);
  return n;
}

size_t
nsCSSValuePair_heap::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  // Only measure it if it's unshared, to avoid double-counting.
  size_t n = 0;
  if (mRefCnt <= 1) {
    n += aMallocSizeOf(this);
    n += mXValue.SizeOfExcludingThis(aMallocSizeOf);
    n += mYValue.SizeOfExcludingThis(aMallocSizeOf);
  }
  return n;
}

// --- nsCSSValueTriplet -----------------

void
nsCSSValueTriplet::AppendToString(nsCSSPropertyID aProperty,
                                  nsAString& aResult,
                                  nsCSSValue::Serialization aSerialization) const
{
  mXValue.AppendToString(aProperty, aResult, aSerialization);
  if (mYValue.GetUnit() != eCSSUnit_Null) {
    aResult.Append(char16_t(' '));
    mYValue.AppendToString(aProperty, aResult, aSerialization);
    if (mZValue.GetUnit() != eCSSUnit_Null) {
      aResult.Append(char16_t(' '));
      mZValue.AppendToString(aProperty, aResult, aSerialization);
    }
  }
}

size_t
nsCSSValueTriplet_heap::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  // Only measure it if it's unshared, to avoid double-counting.
  size_t n = 0;
  if (mRefCnt <= 1) {
    n += aMallocSizeOf(this);
    n += mXValue.SizeOfExcludingThis(aMallocSizeOf);
    n += mYValue.SizeOfExcludingThis(aMallocSizeOf);
    n += mZValue.SizeOfExcludingThis(aMallocSizeOf);
  }
  return n;
}

// --- nsCSSValuePairList -----------------

nsCSSValuePairList::~nsCSSValuePairList()
{
  MOZ_COUNT_DTOR(nsCSSValuePairList);
  NS_CSS_DELETE_LIST_MEMBER(nsCSSValuePairList, this, mNext);
}

nsCSSValuePairList*
nsCSSValuePairList::Clone() const
{
  nsCSSValuePairList* result = new nsCSSValuePairList(*this);
  nsCSSValuePairList* dest = result;
  const nsCSSValuePairList* src = this->mNext;
  while (src) {
    dest->mNext = new nsCSSValuePairList(*src);
    dest = dest->mNext;
    src = src->mNext;
  }

  MOZ_ASSERT(result, "shouldn't return null; supposed to be infallible");
  return result;
}

void
nsCSSValuePairList::AppendToString(nsCSSPropertyID aProperty,
                                   nsAString& aResult,
                                   nsCSSValue::Serialization aSerialization) const
{
  const nsCSSValuePairList* item = this;
  for (;;) {
    MOZ_ASSERT(item->mXValue.GetUnit() != eCSSUnit_Null,
               "unexpected null unit");
    item->mXValue.AppendToString(aProperty, aResult, aSerialization);
    if (item->mXValue.GetUnit() != eCSSUnit_Inherit &&
        item->mXValue.GetUnit() != eCSSUnit_Initial &&
        item->mXValue.GetUnit() != eCSSUnit_Unset &&
        item->mYValue.GetUnit() != eCSSUnit_Null) {
      aResult.Append(char16_t(' '));
      item->mYValue.AppendToString(aProperty, aResult, aSerialization);
    }
    item = item->mNext;
    if (!item)
      break;

    if (nsCSSProps::PropHasFlags(aProperty,
                                 CSS_PROPERTY_VALUE_LIST_USES_COMMAS) ||
        aProperty == eCSSProperty_clip_path ||
        aProperty == eCSSProperty_shape_outside)
      aResult.Append(char16_t(','));
    aResult.Append(char16_t(' '));
  }
}

/* static */ bool
nsCSSValuePairList::Equal(const nsCSSValuePairList* aList1,
                          const nsCSSValuePairList* aList2)
{
  if (aList1 == aList2) {
    return true;
  }

  const nsCSSValuePairList *p1 = aList1, *p2 = aList2;
  for ( ; p1 && p2; p1 = p1->mNext, p2 = p2->mNext) {
    if (p1->mXValue != p2->mXValue ||
        p1->mYValue != p2->mYValue)
      return false;
  }
  return !p1 && !p2; // true if same length, false otherwise
}

size_t
nsCSSValuePairList::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;
  const nsCSSValuePairList* v = this;
  while (v) {
    n += aMallocSizeOf(v);
    n += v->mXValue.SizeOfExcludingThis(aMallocSizeOf);
    n += v->mYValue.SizeOfExcludingThis(aMallocSizeOf);
    v = v->mNext;
  }
  return n;
}

size_t
nsCSSValuePairList_heap::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  // Only measure it if it's unshared, to avoid double-counting.
  size_t n = 0;
  if (mRefCnt <= 1) {
    n += aMallocSizeOf(this);
    n += mXValue.SizeOfExcludingThis(aMallocSizeOf);
    n += mYValue.SizeOfExcludingThis(aMallocSizeOf);
    n += mNext ? mNext->SizeOfIncludingThis(aMallocSizeOf) : 0;
  }
  return n;
}

size_t
nsCSSValue::Array::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  for (size_t i = 0; i < mCount; i++) {
    n += mArray[i].SizeOfExcludingThis(aMallocSizeOf);
  }
  return n;
}

css::URLValueData::URLValueData(already_AddRefed<PtrHolder<nsIURI>> aURI,
                                nsStringBuffer* aString,
                                already_AddRefed<PtrHolder<nsIURI>> aBaseURI,
                                already_AddRefed<PtrHolder<nsIURI>> aReferrer,
                                already_AddRefed<PtrHolder<nsIPrincipal>>
                                  aOriginPrincipal)
  : mURI(Move(aURI))
  , mBaseURI(Move(aBaseURI))
  , mString(aString)
  , mReferrer(Move(aReferrer))
  , mOriginPrincipal(Move(aOriginPrincipal))
  , mURIResolved(true)
  , mIsLocalRef(IsLocalRefURL(aString))
{
  MOZ_ASSERT(mString);
  MOZ_ASSERT(mBaseURI);
  MOZ_ASSERT(mOriginPrincipal);
}

css::URLValueData::URLValueData(nsStringBuffer* aString,
                                already_AddRefed<PtrHolder<nsIURI>> aBaseURI,
                                already_AddRefed<PtrHolder<nsIURI>> aReferrer,
                                already_AddRefed<PtrHolder<nsIPrincipal>>
                                  aOriginPrincipal)
  : mBaseURI(Move(aBaseURI))
  , mString(aString)
  , mReferrer(Move(aReferrer))
  , mOriginPrincipal(Move(aOriginPrincipal))
  , mURIResolved(false)
  , mIsLocalRef(IsLocalRefURL(aString))
{
  MOZ_ASSERT(aString);
  MOZ_ASSERT(mBaseURI);
  MOZ_ASSERT(mOriginPrincipal);
}

bool
css::URLValueData::Equals(const URLValueData& aOther) const
{
  MOZ_ASSERT(NS_IsMainThread());

  bool eq;
  // Cast away const so we can call nsIPrincipal::Equals.
  auto& self = *const_cast<URLValueData*>(this);
  auto& other = const_cast<URLValueData&>(aOther);
  return NS_strcmp(nsCSSValue::GetBufferValue(mString),
                   nsCSSValue::GetBufferValue(aOther.mString)) == 0 &&
          (GetURI() == aOther.GetURI() || // handles null == null
           (mURI && aOther.mURI &&
            NS_SUCCEEDED(mURI->Equals(aOther.mURI, &eq)) &&
            eq)) &&
          (mBaseURI == aOther.mBaseURI ||
           (NS_SUCCEEDED(self.mBaseURI.get()->Equals(other.mBaseURI.get(), &eq)) &&
            eq)) &&
          (mOriginPrincipal == aOther.mOriginPrincipal ||
           self.mOriginPrincipal.get()->Equals(other.mOriginPrincipal.get())) &&
          mIsLocalRef == aOther.mIsLocalRef;
}

bool
css::URLValueData::DefinitelyEqualURIs(const URLValueData& aOther) const
{
  return mBaseURI == aOther.mBaseURI &&
         (mString == aOther.mString ||
          NS_strcmp(nsCSSValue::GetBufferValue(mString),
                    nsCSSValue::GetBufferValue(aOther.mString)) == 0);
}

bool
css::URLValueData::DefinitelyEqualURIsAndPrincipal(
    const URLValueData& aOther) const
{
  return mOriginPrincipal == aOther.mOriginPrincipal &&
         DefinitelyEqualURIs(aOther);
}

nsIURI*
css::URLValueData::GetURI() const
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mURIResolved) {
    MOZ_ASSERT(!mURI);
    nsCOMPtr<nsIURI> newURI;
    NS_NewURI(getter_AddRefs(newURI),
              NS_ConvertUTF16toUTF8(nsCSSValue::GetBufferValue(mString)),
              nullptr, const_cast<nsIURI*>(mBaseURI.get()));
    mURI = new PtrHolder<nsIURI>(newURI.forget());
    mURIResolved = true;
  }

  return mURI;
}

already_AddRefed<nsIURI>
css::URLValueData::ResolveLocalRef(nsIURI* aURI) const
{
  nsCOMPtr<nsIURI> result = GetURI();

  if (result && mIsLocalRef) {
    nsCString ref;
    mURI->GetRef(ref);

    aURI->Clone(getter_AddRefs(result));
    result->SetRef(ref);
  }

  return result.forget();
}

already_AddRefed<nsIURI>
css::URLValueData::ResolveLocalRef(nsIContent* aContent) const
{
  nsCOMPtr<nsIURI> url = aContent->GetBaseURI();
  return ResolveLocalRef(url);
}

void
css::URLValueData::GetSourceString(nsString& aRef) const
{
  nsIURI* uri = GetURI();
  if (!uri) {
    aRef.Truncate();
    return;
  }

  nsCString cref;
  if (mIsLocalRef) {
    // XXXheycam It's possible we can just return mString in this case, since
    // it should be the "#fragment" string the URLValueData was created with.
    uri->GetRef(cref);
    cref.Insert('#', 0);
  } else {
    // It's not entirely clear how to best handle failure here. Ensuring the
    // string is empty seems safest.
    nsresult rv = uri->GetSpec(cref);
    if (NS_FAILED(rv)) {
      cref.Truncate();
    }
  }

  aRef = NS_ConvertUTF8toUTF16(cref);
}

bool
css::URLValueData::EqualsExceptRef(nsIURI* aURI) const
{
  nsIURI* uri = GetURI();
  if (!uri) {
    return false;
  }

  bool ret = false;
  uri->EqualsExceptRef(aURI, &ret);
  return ret;
}

size_t
css::URLValueData::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;
  n += mString->SizeOfIncludingThisIfUnshared(aMallocSizeOf);

  // Measurement of the following members may be added later if DMD finds it
  // is worthwhile:
  // - mURI
  // - mReferrer
  // - mOriginPrincipal
  return n;
}

URLValue::URLValue(nsStringBuffer* aString, nsIURI* aBaseURI, nsIURI* aReferrer,
                   nsIPrincipal* aOriginPrincipal)
  : URLValueData(aString,
                 do_AddRef(new PtrHolder<nsIURI>(aBaseURI)),
                 do_AddRef(new PtrHolder<nsIURI>(aReferrer)),
                 do_AddRef(new PtrHolder<nsIPrincipal>(aOriginPrincipal)))
{
  MOZ_ASSERT(NS_IsMainThread());
}

URLValue::URLValue(nsIURI* aURI, nsStringBuffer* aString, nsIURI* aBaseURI,
                   nsIURI* aReferrer, nsIPrincipal* aOriginPrincipal)
  : URLValueData(do_AddRef(new PtrHolder<nsIURI>(aURI)),
                 aString,
                 do_AddRef(new PtrHolder<nsIURI>(aBaseURI)),
                 do_AddRef(new PtrHolder<nsIURI>(aReferrer)),
                 do_AddRef(new PtrHolder<nsIPrincipal>(aOriginPrincipal)))
{
  MOZ_ASSERT(NS_IsMainThread());
}

size_t
css::URLValue::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  // Only measure it if it's unshared, to avoid double-counting.
  size_t n = 0;
  if (mRefCnt <= 1) {
    n += aMallocSizeOf(this);
    n += URLValueData::SizeOfExcludingThis(aMallocSizeOf);
  }
  return n;
}

css::ImageValue::ImageValue(nsIURI* aURI, nsStringBuffer* aString,
                            nsIURI* aBaseURI, nsIURI* aReferrer,
                            nsIPrincipal* aOriginPrincipal,
                            nsIDocument* aDocument)
  : URLValueData(do_AddRef(new PtrHolder<nsIURI>(aURI)),
                 aString,
                 do_AddRef(new PtrHolder<nsIURI>(aBaseURI, false)),
                 do_AddRef(new PtrHolder<nsIURI>(aReferrer)),
                 do_AddRef(new PtrHolder<nsIPrincipal>(aOriginPrincipal)))
{
  Initialize(aDocument);
}

css::ImageValue::ImageValue(
    nsStringBuffer* aString,
    already_AddRefed<PtrHolder<nsIURI>> aBaseURI,
    already_AddRefed<PtrHolder<nsIURI>> aReferrer,
    already_AddRefed<PtrHolder<nsIPrincipal>> aOriginPrincipal)
  : URLValueData(aString, Move(aBaseURI), Move(aReferrer),
                 Move(aOriginPrincipal))
{
}

void
css::ImageValue::Initialize(nsIDocument* aDocument)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mInitialized);

  // NB: If aDocument is not the original document, we may not be able to load
  // images from aDocument.  Instead we do the image load from the original doc
  // and clone it to aDocument.
  nsIDocument* loadingDoc = aDocument->GetOriginalDocument();
  if (!loadingDoc) {
    loadingDoc = aDocument;
  }

  loadingDoc->StyleImageLoader()->LoadImage(GetURI(), mOriginPrincipal,
                                            mReferrer, this);

  if (loadingDoc != aDocument) {
    aDocument->StyleImageLoader()->MaybeRegisterCSSImage(this);
  }

#ifdef DEBUG
  mInitialized = true;
#endif
}

css::ImageValue::~ImageValue()
{
  MOZ_ASSERT(NS_IsMainThread());

  for (auto iter = mRequests.Iter(); !iter.Done(); iter.Next()) {
    nsIDocument* doc = iter.Key();
    RefPtr<imgRequestProxy>& proxy = iter.Data();

    if (doc) {
      doc->StyleImageLoader()->DeregisterCSSImage(this);
    }

    if (proxy) {
      proxy->CancelAndForgetObserver(NS_BINDING_ABORTED);
    }

    iter.Remove();
  }
}

size_t
css::ComplexColorValue::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  // Only measure it if it's unshared, to avoid double-counting.
  size_t n = 0;
  if (mRefCnt <= 1) {
    n += aMallocSizeOf(this);
  }
  return n;
}

nsCSSValueGradientStop::nsCSSValueGradientStop()
  : mLocation(eCSSUnit_None),
    mColor(eCSSUnit_Null),
    mIsInterpolationHint(false)
{
  MOZ_COUNT_CTOR(nsCSSValueGradientStop);
}

nsCSSValueGradientStop::nsCSSValueGradientStop(const nsCSSValueGradientStop& aOther)
  : mLocation(aOther.mLocation),
    mColor(aOther.mColor),
    mIsInterpolationHint(aOther.mIsInterpolationHint)
{
  MOZ_COUNT_CTOR(nsCSSValueGradientStop);
}

nsCSSValueGradientStop::~nsCSSValueGradientStop()
{
  MOZ_COUNT_DTOR(nsCSSValueGradientStop);
}

size_t
nsCSSValueGradientStop::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;
  n += mLocation.SizeOfExcludingThis(aMallocSizeOf);
  n += mColor   .SizeOfExcludingThis(aMallocSizeOf);
  return n;
}

nsCSSValueGradient::nsCSSValueGradient(bool aIsRadial,
                                       bool aIsRepeating)
  : mIsRadial(aIsRadial),
    mIsRepeating(aIsRepeating),
    mIsLegacySyntax(false),
    mIsExplicitSize(false),
    mBgPos(eCSSUnit_None),
    mAngle(eCSSUnit_None)
{
  mRadialValues[0].SetNoneValue();
  mRadialValues[1].SetNoneValue();
}

size_t
nsCSSValueGradient::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  // Only measure it if it's unshared, to avoid double-counting.
  size_t n = 0;
  if (mRefCnt <= 1) {
    n += aMallocSizeOf(this);
    n += mBgPos.SizeOfExcludingThis(aMallocSizeOf);
    n += mAngle.SizeOfExcludingThis(aMallocSizeOf);
    n += mRadialValues[0].SizeOfExcludingThis(aMallocSizeOf);
    n += mRadialValues[1].SizeOfExcludingThis(aMallocSizeOf);
    n += mStops.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (uint32_t i = 0; i < mStops.Length(); i++) {
      n += mStops[i].SizeOfExcludingThis(aMallocSizeOf);
    }
  }
  return n;
}

// --- nsCSSValueTokenStream ------------

nsCSSValueTokenStream::nsCSSValueTokenStream()
  : mPropertyID(eCSSProperty_UNKNOWN)
  , mShorthandPropertyID(eCSSProperty_UNKNOWN)
  , mLevel(SheetType::Count)
{}

nsCSSValueTokenStream::~nsCSSValueTokenStream()
{}

size_t
nsCSSValueTokenStream::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  // Only measure it if it's unshared, to avoid double-counting.
  size_t n = 0;
  if (mRefCnt <= 1) {
    n += aMallocSizeOf(this);
    n += mTokenStream.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }
  return n;
}

// --- nsCSSValueFloatColor -------------

bool
nsCSSValueFloatColor::operator==(nsCSSValueFloatColor& aOther) const
{
  return mComponent1 == aOther.mComponent1 &&
         mComponent2 == aOther.mComponent2 &&
         mComponent3 == aOther.mComponent3 &&
         mAlpha == aOther.mAlpha;
}

nscolor
nsCSSValueFloatColor::GetColorValue(nsCSSUnit aUnit) const
{
  MOZ_ASSERT(nsCSSValue::IsFloatColorUnit(aUnit), "unexpected unit");

  // We should clamp each component value since eCSSUnit_PercentageRGBColor
  // and eCSSUnit_PercentageRGBAColor may store values greater than 1.0.
  if (aUnit == eCSSUnit_PercentageRGBColor ||
      aUnit == eCSSUnit_PercentageRGBAColor) {
    return NS_RGBA(
      // We need to clamp before multiplying by 255.0f to avoid overflow.
      NSToIntRound(mozilla::clamped(mComponent1, 0.0f, 1.0f) * 255.0f),
      NSToIntRound(mozilla::clamped(mComponent2, 0.0f, 1.0f) * 255.0f),
      NSToIntRound(mozilla::clamped(mComponent3, 0.0f, 1.0f) * 255.0f),
      NSToIntRound(mozilla::clamped(mAlpha, 0.0f, 1.0f) * 255.0f));
  }

  // HSL color
  MOZ_ASSERT(aUnit == eCSSUnit_HSLColor ||
             aUnit == eCSSUnit_HSLAColor);
  nscolor hsl = NS_HSL2RGB(mComponent1, mComponent2, mComponent3);
  return NS_RGBA(NS_GET_R(hsl),
                 NS_GET_G(hsl),
                 NS_GET_B(hsl),
                 NSToIntRound(mAlpha * 255.0f));
}

bool
nsCSSValueFloatColor::IsNonTransparentColor() const
{
  return mAlpha > 0.0f;
}

void
nsCSSValueFloatColor::AppendToString(nsCSSUnit aUnit, nsAString& aResult) const
{
  // Similar to the rgb()/rgba() case in nsCSSValue::AppendToString. We omit the
  // alpha component if it's equal to 1.0f (full opaque). Also, we try to
  // preserve the author-specified function name, unless it's rgba()/hsla() and
  // we're omitting the alpha component - then we use rgb()/hsl().
  MOZ_ASSERT(nsCSSValue::IsFloatColorUnit(aUnit), "unexpected unit");

  bool showAlpha = (mAlpha != 1.0f);
  bool isHSL = (aUnit == eCSSUnit_HSLColor ||
                aUnit == eCSSUnit_HSLAColor);

  if (isHSL) {
    aResult.AppendLiteral("hsl");
  } else {
    aResult.AppendLiteral("rgb");
  }
  if (showAlpha && (aUnit == eCSSUnit_HSLAColor || aUnit == eCSSUnit_PercentageRGBAColor)) {
    aResult.AppendLiteral("a(");
  } else {
    aResult.Append('(');
  }
  if (isHSL) {
    aResult.AppendFloat(mComponent1 * 360.0f);
    aResult.AppendLiteral(", ");
  } else {
    aResult.AppendFloat(mComponent1 * 100.0f);
    aResult.AppendLiteral("%, ");
  }
  aResult.AppendFloat(mComponent2 * 100.0f);
  aResult.AppendLiteral("%, ");
  aResult.AppendFloat(mComponent3 * 100.0f);
  if (showAlpha) {
    aResult.AppendLiteral("%, ");
    aResult.AppendFloat(mAlpha);
    aResult.Append(')');
  } else {
    aResult.AppendLiteral("%)");
  }
}

size_t
nsCSSValueFloatColor::SizeOfIncludingThis(
                                      mozilla::MallocSizeOf aMallocSizeOf) const
{
  // Only measure it if it's unshared, to avoid double-counting.
  size_t n = 0;
  if (mRefCnt <= 1) {
    n += aMallocSizeOf(this);
  }
  return n;
}

// --- nsCSSCornerSizes -----------------

nsCSSCornerSizes::nsCSSCornerSizes(void)
{
  MOZ_COUNT_CTOR(nsCSSCornerSizes);
}

nsCSSCornerSizes::nsCSSCornerSizes(const nsCSSCornerSizes& aCopy)
  : mTopLeft(aCopy.mTopLeft),
    mTopRight(aCopy.mTopRight),
    mBottomRight(aCopy.mBottomRight),
    mBottomLeft(aCopy.mBottomLeft)
{
  MOZ_COUNT_CTOR(nsCSSCornerSizes);
}

nsCSSCornerSizes::~nsCSSCornerSizes()
{
  MOZ_COUNT_DTOR(nsCSSCornerSizes);
}

void
nsCSSCornerSizes::Reset()
{
  NS_FOR_CSS_FULL_CORNERS(corner) {
    this->GetCorner(corner).Reset();
  }
}

static_assert(eCornerTopLeft == 0 && eCornerTopRight == 1 &&
              eCornerBottomRight == 2 && eCornerBottomLeft == 3,
              "box corner constants not tl/tr/br/bl == 0/1/2/3");

/* static */ const nsCSSCornerSizes::corner_type
nsCSSCornerSizes::corners[4] = {
  &nsCSSCornerSizes::mTopLeft,
  &nsCSSCornerSizes::mTopRight,
  &nsCSSCornerSizes::mBottomRight,
  &nsCSSCornerSizes::mBottomLeft,
};

size_t
mozilla::css::GridTemplateAreasValue::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  // Only measure it if it's unshared, to avoid double-counting.
  size_t n = 0;
  if (mRefCnt <= 1) {
    n += aMallocSizeOf(this);
    n += mNamedAreas.ShallowSizeOfExcludingThis(aMallocSizeOf);
    n += mTemplates.ShallowSizeOfExcludingThis(aMallocSizeOf);
  }
  return n;
}
