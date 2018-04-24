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
#include "imgIRequest.h"
#include "imgRequestProxy.h"
#include "nsIDocument.h"
#include "nsIURIMutator.h"
#include "nsCSSProps.h"
#include "nsNetUtil.h"
#include "nsPresContext.h"
#include "nsStyleUtil.h"
#include "nsDeviceContext.h"
#include "nsContentUtils.h"

using namespace mozilla;
using namespace mozilla::css;

template<class T>
static bool MightHaveRef(const T& aString)
{
  const typename T::char_type* current = aString.BeginReading();
  for (; current != aString.EndReading(); current++) {
    if (*current == '#') {
      return true;
    }
  }

  return false;
}

nsCSSValue::nsCSSValue(int32_t aValue, nsCSSUnit aUnit)
  : mUnit(aUnit)
{
  MOZ_ASSERT(aUnit == eCSSUnit_Integer || aUnit == eCSSUnit_Enumerated,
             "not an int value");
  if (aUnit == eCSSUnit_Integer || aUnit == eCSSUnit_Enumerated) {
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

nsCSSValue::nsCSSValue(mozilla::css::GridTemplateAreasValue* aValue)
  : mUnit(eCSSUnit_GridTemplateAreas)
{
  mValue.mGridTemplateAreas = aValue;
  mValue.mGridTemplateAreas->AddRef();
}

nsCSSValue::nsCSSValue(SharedFontList* aValue)
  : mUnit(eCSSUnit_FontFamilyList)
{
  mValue.mFontFamilyList = aValue;
  mValue.mFontFamilyList->AddRef();
}

nsCSSValue::nsCSSValue(FontStretch aStretch)
  : mUnit(eCSSUnit_FontStretch)
{
  mValue.mFontStretch = aStretch;
}

nsCSSValue::nsCSSValue(FontSlantStyle aStyle)
  : mUnit(eCSSUnit_FontSlantStyle)
{
  mValue.mFontSlantStyle = aStyle;
}

nsCSSValue::nsCSSValue(FontWeight aWeight)
  : mUnit(eCSSUnit_FontWeight)
{
  mValue.mFontWeight = aWeight;
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
  else if (eCSSUnit_Integer <= mUnit && mUnit <= eCSSUnit_Enumerated) {
    mValue.mInt = aCopy.mValue.mInt;
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
  else if (eCSSUnit_Pair == mUnit) {
    mValue.mPair = aCopy.mValue.mPair;
    mValue.mPair->AddRef();
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
  else if (eCSSUnit_FontStretch == mUnit) {
    mValue.mFontStretch = aCopy.mValue.mFontStretch;
  }
  else if (eCSSUnit_FontSlantStyle == mUnit) {
    mValue.mFontSlantStyle = aCopy.mValue.mFontSlantStyle;
  }
  else if (eCSSUnit_FontWeight == mUnit) {
    mValue.mFontWeight = aCopy.mValue.mFontWeight;
  }
  else if (eCSSUnit_AtomIdent == mUnit) {
    mValue.mAtom = aCopy.mValue.mAtom;
    mValue.mAtom->AddRef();
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
    else if ((eCSSUnit_Integer <= mUnit) && (mUnit <= eCSSUnit_Enumerated)) {
      return mValue.mInt == aOther.mValue.mInt;
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
    else if (eCSSUnit_Pair == mUnit) {
      return *mValue.mPair == *aOther.mValue.mPair;
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
      return mValue.mFontFamilyList->mNames ==
             aOther.mValue.mFontFamilyList->mNames;
    }
    else if (eCSSUnit_FontStretch == mUnit) {
      return mValue.mFontStretch == aOther.mValue.mFontStretch;
    }
    else if (eCSSUnit_FontSlantStyle == mUnit) {
      return mValue.mFontSlantStyle == aOther.mValue.mFontSlantStyle;
    }
    else if (eCSSUnit_FontWeight == mUnit) {
      return mValue.mFontWeight == aOther.mValue.mFontWeight;
    }
    else if (eCSSUnit_AtomIdent == mUnit) {
      return mValue.mAtom == aOther.mValue.mAtom;
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
  return nsContentUtils::GetStaticRequest(aDocument, req);
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

// Assert against resetting non-trivial CSS values from the parallel Servo
// traversal, since the refcounts aren't thread-safe.
// Note that the caller might be an OMTA thread, which is allowed to operate off
// main thread because it owns all of the corresponding nsCSSValues and any that
// they might be sharing members with. Since this can happen concurrently with
// the servo traversal, we have to use a more-precise (but slower) test.
#define DO_RELEASE(member) {                                     \
  MOZ_ASSERT(!ServoStyleSet::IsCurrentThreadInServoTraversal()); \
  mValue.member->Release();                                      \
}

void nsCSSValue::DoReset()
{
  if (UnitHasStringValue()) {
    mValue.mString->Release();
  } else if (UnitHasArrayValue()) {
    DO_RELEASE(mArray);
  } else if (eCSSUnit_URL == mUnit) {
    DO_RELEASE(mURL);
  } else if (eCSSUnit_Image == mUnit) {
    DO_RELEASE(mImage);
  } else if (eCSSUnit_Pair == mUnit) {
    DO_RELEASE(mPair);
  } else if (eCSSUnit_List == mUnit) {
    DO_RELEASE(mList);
  } else if (eCSSUnit_SharedList == mUnit) {
    DO_RELEASE(mSharedList);
  } else if (eCSSUnit_PairList == mUnit) {
    DO_RELEASE(mPairList);
  } else if (eCSSUnit_GridTemplateAreas == mUnit) {
    DO_RELEASE(mGridTemplateAreas);
  } else if (eCSSUnit_FontFamilyList == mUnit) {
    DO_RELEASE(mFontFamilyList);
  } else if (eCSSUnit_AtomIdent == mUnit) {
    DO_RELEASE(mAtom);
  }
  mUnit = eCSSUnit_Null;
}

#undef DO_RELEASE

void nsCSSValue::SetIntValue(int32_t aValue, nsCSSUnit aUnit)
{
  MOZ_ASSERT(aUnit == eCSSUnit_Integer || aUnit == eCSSUnit_Enumerated,
             "not an int value");
  Reset();
  if (aUnit == eCSSUnit_Integer || aUnit == eCSSUnit_Enumerated) {
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

void
nsCSSValue::SetAtomIdentValue(already_AddRefed<nsAtom> aValue)
{
  Reset();
  mUnit = eCSSUnit_AtomIdent;
  mValue.mAtom = aValue.take();
}

void nsCSSValue::SetIntegerCoordValue(nscoord aValue)
{
  SetFloatValue(nsPresContext::AppUnitsToFloatCSSPixels(aValue),
                eCSSUnit_Pixel);
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

void nsCSSValue::SetGridTemplateAreas(mozilla::css::GridTemplateAreasValue* aValue)
{
  Reset();
  mUnit = eCSSUnit_GridTemplateAreas;
  mValue.mGridTemplateAreas = aValue;
  mValue.mGridTemplateAreas->AddRef();
}

void nsCSSValue::SetFontFamilyListValue(already_AddRefed<SharedFontList> aValue)
{
  Reset();
  mUnit = eCSSUnit_FontFamilyList;
  mValue.mFontFamilyList = aValue.take();
}

void nsCSSValue::SetFontStretch(FontStretch aStretch)
{
  Reset();
  mUnit = eCSSUnit_FontStretch;
  mValue.mFontStretch = aStretch;
}

void nsCSSValue::SetFontSlantStyle(FontSlantStyle aStyle)
{
  Reset();
  mUnit = eCSSUnit_FontSlantStyle;
  mValue.mFontSlantStyle = aStyle;
}

void nsCSSValue::SetFontWeight(FontWeight aWeight)
{
  Reset();
  mUnit = eCSSUnit_FontWeight;
  mValue.mFontWeight = aWeight;
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

nsStyleCoord::CalcValue
nsCSSValue::GetCalcValue() const
{
  MOZ_ASSERT(mUnit == eCSSUnit_Calc,
             "The unit should be eCSSUnit_Calc");

  const nsCSSValue::Array* array = GetArrayValue();
  MOZ_ASSERT(array->Count() == 1,
             "There should be a 1-length array");

  const nsCSSValue& rootValue = array->Item(0);

  nsStyleCoord::CalcValue result;

  if (rootValue.GetUnit() == eCSSUnit_Pixel) {
    result.mLength = rootValue.GetPixelLength();
    result.mPercent = 0.0f;
    result.mHasPercent = false;
  } else {
    MOZ_ASSERT(rootValue.GetUnit() == eCSSUnit_Calc_Plus,
               "Calc unit should be eCSSUnit_Calc_Plus");

    const nsCSSValue::Array *calcPlusArray = rootValue.GetArrayValue();
    MOZ_ASSERT(calcPlusArray->Count() == 2,
               "eCSSUnit_Calc_Plus should have a 2-length array");

    const nsCSSValue& length = calcPlusArray->Item(0);
    const nsCSSValue& percent = calcPlusArray->Item(1);
    MOZ_ASSERT(length.GetUnit() == eCSSUnit_Pixel,
               "The first value should be eCSSUnit_Pixel");
    MOZ_ASSERT(percent.GetUnit() == eCSSUnit_Percent,
               "The first value should be eCSSUnit_Percent");
    result.mLength = length.GetPixelLength();
    result.mPercent = percent.GetPercentValue();
    result.mHasPercent = true;
  }

  return result;
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

void
nsCSSValue::AtomizeIdentValue()
{
  MOZ_ASSERT(mUnit == eCSSUnit_Ident);
  RefPtr<nsAtom> atom = NS_Atomize(GetStringBufferValue());
  Reset();
  mUnit = eCSSUnit_AtomIdent;
  mValue.mAtom = atom.forget().take();
}

/* static */ void
nsCSSValue::AppendAlignJustifyValueToString(int32_t aValue, nsAString& aResult)
{
  auto legacy = aValue & NS_STYLE_ALIGN_LEGACY;
  if (legacy) {
    aValue &= ~legacy;
    aResult.AppendLiteral("legacy");
    if (!aValue) {
      return;
    }
    aResult.AppendLiteral(" ");
  }
  // Don't serialize the 'unsafe' keyword; it's the default.
  auto overflowPos = aValue & (NS_STYLE_ALIGN_SAFE | NS_STYLE_ALIGN_UNSAFE);
  if (MOZ_UNLIKELY(overflowPos == NS_STYLE_ALIGN_SAFE)) {
    aResult.AppendLiteral("safe ");
  }
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
      n += mValue.mImage->SizeOfIncludingThis(aMallocSizeOf);
      break;

    // Pair
    case eCSSUnit_Pair:
      n += mValue.mPair->SizeOfIncludingThis(aMallocSizeOf);
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
      // The SharedFontList is a refcounted object, but is unique per
      // declaration. We don't measure the references from computed
      // values.
      n += mValue.mFontFamilyList->SizeOfIncludingThis(aMallocSizeOf);
      break;

    // Atom is always shared, and thus should not be counted.
    case eCSSUnit_AtomIdent:
      break;

    // Int: nothing extra to measure.
    case eCSSUnit_Integer:
    case eCSSUnit_Enumerated:
    case eCSSUnit_FontStretch:
    case eCSSUnit_FontSlantStyle:
    case eCSSUnit_FontWeight:
      break;

    // Float: nothing extra to measure.
    case eCSSUnit_Percent:
    case eCSSUnit_Number:
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

// --- nsCSSValuePair -----------------

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

css::URLValueData::URLValueData(already_AddRefed<nsIURI> aURI,
                                ServoRawOffsetArc<RustString> aString,
                                already_AddRefed<URLExtraData> aExtraData)
  : mURI(Move(aURI))
  , mExtraData(Move(aExtraData))
  , mURIResolved(true)
  , mString(aString)
{
  MOZ_ASSERT(mExtraData);
  MOZ_ASSERT(mExtraData->GetPrincipal());
}

css::URLValueData::URLValueData(ServoRawOffsetArc<RustString> aString,
                                already_AddRefed<URLExtraData> aExtraData)
  : mExtraData(Move(aExtraData))
  , mURIResolved(false)
  , mString(aString)
{
  MOZ_ASSERT(mExtraData);
  MOZ_ASSERT(mExtraData->GetPrincipal());
}

css::URLValueData::~URLValueData()
{
  Servo_ReleaseArcStringData(&mString);
}

bool
css::URLValueData::Equals(const URLValueData& aOther) const
{
  MOZ_ASSERT(NS_IsMainThread());

  bool eq;
  const URLExtraData* self = mExtraData;
  const URLExtraData* other = aOther.mExtraData;
  return GetString() == aOther.GetString() &&
          (GetURI() == aOther.GetURI() || // handles null == null
           (mURI && aOther.mURI &&
            NS_SUCCEEDED(mURI->Equals(aOther.mURI, &eq)) &&
            eq)) &&
          (self->BaseURI() == other->BaseURI() ||
           (NS_SUCCEEDED(self->BaseURI()->Equals(other->BaseURI(), &eq)) &&
            eq)) &&
          (self->GetPrincipal() == other->GetPrincipal() ||
           self->GetPrincipal()->Equals(other->GetPrincipal())) &&
          IsLocalRef() == aOther.IsLocalRef();
}

bool
css::URLValueData::DefinitelyEqualURIs(const URLValueData& aOther) const
{
  if (mExtraData->BaseURI() != aOther.mExtraData->BaseURI()) {
    return false;
  }
  return GetString() == aOther.GetString();
}

bool
css::URLValueData::DefinitelyEqualURIsAndPrincipal(
    const URLValueData& aOther) const
{
  return mExtraData->GetPrincipal() == aOther.mExtraData->GetPrincipal() &&
         DefinitelyEqualURIs(aOther);
}

nsDependentCSubstring
css::URLValueData::GetString() const
{
  const uint8_t* chars;
  uint32_t len;
  Servo_GetArcStringData(mString.mPtr, &chars, &len);
  return nsDependentCSubstring(reinterpret_cast<const char*>(chars), len);
}

nsIURI*
css::URLValueData::GetURI() const
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mURIResolved) {
    MOZ_ASSERT(!mURI);
    nsCOMPtr<nsIURI> newURI;
    NS_NewURI(getter_AddRefs(newURI),
              GetString(),
              nullptr, mExtraData->BaseURI());
    mURI = newURI.forget();
    mURIResolved = true;
  }

  return mURI;
}

bool
css::URLValueData::IsLocalRef() const
{
  if (mIsLocalRef.isNothing()) {
    // IsLocalRefURL is O(N), use it only when IsLocalRef is called.
    mIsLocalRef.emplace(nsContentUtils::IsLocalRefURL(GetString()));
  }
  return mIsLocalRef.value();
}

bool
css::URLValueData::HasRef() const
{
  bool result = false;

  if (IsLocalRef()) {
    result = true;
  } else {
    if (nsIURI* uri = GetURI()) {
      nsAutoCString ref;
      nsresult rv = uri->GetRef(ref);
      if (NS_SUCCEEDED(rv) && !ref.IsEmpty()) {
        result = true;
      }
    }
  }

  mMightHaveRef = Some(result);

  return result;
}

bool
css::URLValueData::MightHaveRef() const
{
  if (mMightHaveRef.isNothing()) {
    bool result = ::MightHaveRef(GetString());
    if (!ServoStyleSet::IsInServoTraversal()) {
      // Can only cache the result if we're not on a style worker thread.
      mMightHaveRef.emplace(result);
    }
    return result;
  }

  return mMightHaveRef.value();
}

already_AddRefed<nsIURI>
css::URLValueData::ResolveLocalRef(nsIURI* aURI) const
{
  nsCOMPtr<nsIURI> result = GetURI();

  if (result && IsLocalRef()) {
    nsCString ref;
    mURI->GetRef(ref);

    nsresult rv = NS_MutateURI(aURI)
                    .SetRef(ref)
                    .Finalize(result);
    if (NS_FAILED(rv)) {
      // If setting the ref failed, just return a clone.
      aURI->Clone(getter_AddRefs(result));
    }
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
  if (IsLocalRef()) {
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
  // Measurement of the following members may be added later if DMD finds it
  // is worthwhile:
  // - mURI
  // - mString
  // - mExtraData
  return 0;
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

css::ImageValue::ImageValue(nsIURI* aURI,
                            ServoRawOffsetArc<RustString> aString,
                            already_AddRefed<URLExtraData> aExtraData,
                            nsIDocument* aDocument,
                            CORSMode aCORSMode)
  : URLValueData(do_AddRef(aURI), aString, Move(aExtraData))
{
  mCORSMode = aCORSMode;
  Initialize(aDocument);
}

css::ImageValue::ImageValue(ServoRawOffsetArc<RustString> aString,
                            already_AddRefed<URLExtraData> aExtraData,
                            CORSMode aCORSMode)
  : URLValueData(aString, Move(aExtraData))
{
  mCORSMode = aCORSMode;
}

/*static*/ already_AddRefed<css::ImageValue>
css::ImageValue::CreateFromURLValue(URLValue* aUrl,
                                    nsIDocument* aDocument,
                                    CORSMode aCORSMode)
{
  return do_AddRef(
    new css::ImageValue(aUrl->GetURI(),
                        Servo_CloneArcStringData(&aUrl->mString),
                        do_AddRef(aUrl->mExtraData),
                        aDocument,
                        aCORSMode));
}

void
css::ImageValue::Initialize(nsIDocument* aDocument)
{
  MOZ_ASSERT(NS_IsMainThread());

  // NB: If aDocument is not the original document, we may not be able to load
  // images from aDocument.  Instead we do the image load from the original doc
  // and clone it to aDocument.
  nsIDocument* loadingDoc = aDocument->GetOriginalDocument();
  if (!loadingDoc) {
    loadingDoc = aDocument;
  }

  if (!mLoadedImage) {
    loadingDoc->StyleImageLoader()->LoadImage(GetURI(),
                                              mExtraData->GetPrincipal(),
                                              mExtraData->GetReferrer(),
                                              this,
                                              mCORSMode);

    mLoadedImage = true;
  }

  aDocument->StyleImageLoader()->MaybeRegisterCSSImage(this);
}

css::ImageValue::~ImageValue()
{
  MOZ_ASSERT(NS_IsMainThread() || mRequests.Count() == 0,
             "Destructor should run on main thread, or on non-main thread "
             "when mRequest is empty!");

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
css::ImageValue::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n += css::URLValueData::SizeOfExcludingThis(aMallocSizeOf);
  n += mRequests.ShallowSizeOfExcludingThis(aMallocSizeOf);
  return n;
}

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
