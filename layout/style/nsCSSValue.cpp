/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of simple property values within CSS declarations */

#include "nsCSSValue.h"

#include "imgIRequest.h"
#include "nsIDocument.h"
#include "nsIPrincipal.h"
#include "nsCSSProps.h"
#include "nsStyleUtil.h"
#include "CSSCalc.h"
#include "nsNetUtil.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/css/ImageLoader.h"
#include "mozilla/Likely.h"
#include "gfxFontConstants.h"
#include "nsPresContext.h"
#include "imgRequestProxy.h"
#include "nsDeviceContext.h"

using namespace mozilla;

nsCSSValue::nsCSSValue(int32_t aValue, nsCSSUnit aUnit)
  : mUnit(aUnit)
{
  NS_ABORT_IF_FALSE(aUnit == eCSSUnit_Integer || aUnit == eCSSUnit_Enumerated ||
                    aUnit == eCSSUnit_EnumColor, "not an int value");
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
  NS_ABORT_IF_FALSE(eCSSUnit_Percent <= aUnit, "not a float value");
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
  NS_ABORT_IF_FALSE(UnitHasStringValue(), "not a string value");
  if (UnitHasStringValue()) {
    mValue.mString = BufferFromString(aValue).get();
  }
  else {
    mUnit = eCSSUnit_Null;
    mValue.mInt = 0;
  }
}

nsCSSValue::nsCSSValue(nsCSSValue::Array* aValue, nsCSSUnit aUnit)
  : mUnit(aUnit)
{
  NS_ABORT_IF_FALSE(UnitHasArrayValue(), "bad unit");
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
  else if (eCSSUnit_Color == mUnit) {
    mValue.mColor = aCopy.mValue.mColor;
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
  else if (eCSSUnit_PairList == mUnit) {
    mValue.mPairList = aCopy.mValue.mPairList;
    mValue.mPairList->AddRef();
  }
  else if (eCSSUnit_PairListDep == mUnit) {
    mValue.mPairListDependent = aCopy.mValue.mPairListDependent;
  }
  else {
    NS_ABORT_IF_FALSE(false, "unknown unit");
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

bool nsCSSValue::operator==(const nsCSSValue& aOther) const
{
  NS_ABORT_IF_FALSE(mUnit != eCSSUnit_ListDep &&
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
    else if (eCSSUnit_Color == mUnit) {
      return mValue.mColor == aOther.mValue.mColor;
    }
    else if (UnitHasArrayValue()) {
      return *mValue.mArray == *aOther.mValue.mArray;
    }
    else if (eCSSUnit_URL == mUnit) {
      return *mValue.mURL == *aOther.mValue.mURL;
    }
    else if (eCSSUnit_Image == mUnit) {
      return *mValue.mImage == *aOther.mValue.mImage;
    }
    else if (eCSSUnit_Gradient == mUnit) {
      return *mValue.mGradient == *aOther.mValue.mGradient;
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
      return *mValue.mList == *aOther.mValue.mList;
    }
    else if (eCSSUnit_PairList == mUnit) {
      return *mValue.mPairList == *aOther.mValue.mPairList;
    }
    else {
      return mValue.mFloat == aOther.mValue.mFloat;
    }
  }
  return false;
}

double nsCSSValue::GetAngleValueInRadians() const
{
  double angle = GetFloatValue();

  switch (GetUnit()) {
  case eCSSUnit_Radian: return angle;
  case eCSSUnit_Turn:   return angle * 2 * M_PI;
  case eCSSUnit_Degree: return angle * M_PI / 180.0;
  case eCSSUnit_Grad:   return angle * M_PI / 200.0;

  default:
    NS_ABORT_IF_FALSE(false, "unrecognized angular unit");
    return 0.0;
  }
}

imgRequestProxy* nsCSSValue::GetImageValue(nsIDocument* aDocument) const
{
  NS_ABORT_IF_FALSE(mUnit == eCSSUnit_Image, "not an Image value");
  return mValue.mImage->mRequests.GetWeak(aDocument);
}

nscoord nsCSSValue::GetFixedLength(nsPresContext* aPresContext) const
{
  NS_ABORT_IF_FALSE(mUnit == eCSSUnit_PhysicalMillimeter,
                    "not a fixed length unit");

  float inches = mValue.mFloat / MM_PER_INCH_FLOAT;
  return NSToCoordFloorClamped(inches *
    float(aPresContext->DeviceContext()->AppUnitsPerPhysicalInch()));
}

nscoord nsCSSValue::GetPixelLength() const
{
  NS_ABORT_IF_FALSE(IsPixelLengthUnit(), "not a fixed length unit");

  double scaleFactor;
  switch (mUnit) {
  case eCSSUnit_Pixel: return nsPresContext::CSSPixelsToAppUnits(mValue.mFloat);
  case eCSSUnit_Pica: scaleFactor = 16.0; break;
  case eCSSUnit_Point: scaleFactor = 4/3.0; break;
  case eCSSUnit_Inch: scaleFactor = 96.0; break;
  case eCSSUnit_Millimeter: scaleFactor = 96/25.4; break;
  case eCSSUnit_Centimeter: scaleFactor = 96/2.54; break;
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
  } else if (UnitHasArrayValue()) {
    mValue.mArray->Release();
  } else if (eCSSUnit_URL == mUnit) {
    mValue.mURL->Release();
  } else if (eCSSUnit_Image == mUnit) {
    mValue.mImage->Release();
  } else if (eCSSUnit_Gradient == mUnit) {
    mValue.mGradient->Release();
  } else if (eCSSUnit_Pair == mUnit) {
    mValue.mPair->Release();
  } else if (eCSSUnit_Triplet == mUnit) {
    mValue.mTriplet->Release();
  } else if (eCSSUnit_Rect == mUnit) {
    mValue.mRect->Release();
  } else if (eCSSUnit_List == mUnit) {
    mValue.mList->Release();
  } else if (eCSSUnit_PairList == mUnit) {
    mValue.mPairList->Release();
  }
  mUnit = eCSSUnit_Null;
}

void nsCSSValue::SetIntValue(int32_t aValue, nsCSSUnit aUnit)
{
  NS_ABORT_IF_FALSE(aUnit == eCSSUnit_Integer || aUnit == eCSSUnit_Enumerated ||
                    aUnit == eCSSUnit_EnumColor, "not an int value");
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
  NS_ABORT_IF_FALSE(eCSSUnit_Number <= aUnit, "not a float value");
  Reset();
  if (eCSSUnit_Number <= aUnit) {
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
  NS_ABORT_IF_FALSE(UnitHasStringValue(), "not a string unit");
  if (UnitHasStringValue()) {
    mValue.mString = BufferFromString(aValue).get();
  } else
    mUnit = eCSSUnit_Null;
}

void nsCSSValue::SetColorValue(nscolor aValue)
{
  Reset();
  mUnit = eCSSUnit_Color;
  mValue.mColor = aValue;
}

void nsCSSValue::SetArrayValue(nsCSSValue::Array* aValue, nsCSSUnit aUnit)
{
  Reset();
  mUnit = aUnit;
  NS_ABORT_IF_FALSE(UnitHasArrayValue(), "bad unit");
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

void nsCSSValue::SetPairValue(const nsCSSValuePair* aValue)
{
  // pairs should not be used for null/inherit/initial values
  NS_ABORT_IF_FALSE(aValue &&
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
  NS_ABORT_IF_FALSE(xValue.GetUnit() != eCSSUnit_Null &&
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
    NS_ABORT_IF_FALSE(aValue &&
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
    NS_ABORT_IF_FALSE(xValue.GetUnit() != eCSSUnit_Null &&
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

void nsCSSValue::SetDependentListValue(nsCSSValueList* aList)
{
  Reset();
  if (aList) {
    mUnit = eCSSUnit_ListDep;
    mValue.mListDependent = aList;
  }
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

void nsCSSValue::StartImageLoad(nsIDocument* aDocument) const
{
  NS_ABORT_IF_FALSE(eCSSUnit_URL == mUnit, "Not a URL value!");
  mozilla::css::ImageValue* image =
    new mozilla::css::ImageValue(mValue.mURL->GetURI(),
                                 mValue.mURL->mString,
                                 mValue.mURL->mReferrer,
                                 mValue.mURL->mOriginPrincipal,
                                 aDocument);
  if (image) {
    nsCSSValue* writable = const_cast<nsCSSValue*>(this);
    writable->SetImageValue(image);
  }
}

bool nsCSSValue::IsNonTransparentColor() const
{
  // We have the value in the form it was specified in at this point, so we
  // have to look for both the keyword 'transparent' and its equivalent in
  // rgba notation.
  nsDependentString buf;
  return
    (mUnit == eCSSUnit_Color && NS_GET_A(GetColorValue()) > 0) ||
    (mUnit == eCSSUnit_Ident &&
     !nsGkAtoms::transparent->Equals(GetStringValue(buf))) ||
    (mUnit == eCSSUnit_EnumColor);
}

nsCSSValue::Array*
nsCSSValue::InitFunction(nsCSSKeyword aFunctionId, uint32_t aNumArgs)
{
  nsRefPtr<nsCSSValue::Array> func = Array::Create(aNumArgs + 1);
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
  NS_ABORT_IF_FALSE(func && func->Count() >= 1 &&
                    func->Item(0).GetUnit() == eCSSUnit_Enumerated,
                    "illegally structured function value");

  nsCSSKeyword thisFunctionId = func->Item(0).GetKeywordValue();
  return thisFunctionId == aFunctionId;
}

// static
already_AddRefed<nsStringBuffer>
nsCSSValue::BufferFromString(const nsString& aValue)
{
  nsRefPtr<nsStringBuffer> buffer = nsStringBuffer::FromString(aValue);
  if (buffer) {
    return buffer.forget();
  }

  nsString::size_type length = aValue.Length();

  // NOTE: Alloc prouduces a new, already-addref'd (refcnt = 1) buffer.
  // NOTE: String buffer allocation is currently fallible.
  buffer = nsStringBuffer::Alloc((length + 1) * sizeof(PRUnichar));
  if (MOZ_UNLIKELY(!buffer)) {
    NS_RUNTIMEABORT("out of memory");
  }

  PRUnichar* data = static_cast<PRUnichar*>(buffer->Data());
  nsCharTraits<PRUnichar>::copy(data, aValue.get(), length);
  // Null-terminate.
  data[length] = 0;
  return buffer.forget();
}

namespace {

struct CSSValueSerializeCalcOps {
  CSSValueSerializeCalcOps(nsCSSProperty aProperty, nsAString& aResult)
    : mProperty(aProperty),
      mResult(aResult)
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
    NS_ABORT_IF_FALSE(aValue.GetUnit() == eCSSUnit_Percent ||
                      aValue.IsLengthUnit(), "unexpected unit");
    aValue.AppendToString(mProperty, mResult);
  }

  void AppendNumber(const input_type& aValue)
  {
    NS_ABORT_IF_FALSE(aValue.GetUnit() == eCSSUnit_Number, "unexpected unit");
    aValue.AppendToString(mProperty, mResult);
  }

private:
  nsCSSProperty mProperty;
  nsAString &mResult;
};

} // anonymous namespace

void
nsCSSValue::AppendToString(nsCSSProperty aProperty, nsAString& aResult) const
{
  // eCSSProperty_UNKNOWN gets used for some recursive calls below.
  NS_ABORT_IF_FALSE((0 <= aProperty &&
                     aProperty <= eCSSProperty_COUNT_no_shorthands) ||
                    aProperty == eCSSProperty_UNKNOWN,
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
    } else if (unit == eCSSUnit_Families) {
      // XXX We really need to do *some* escaping.
      aResult.Append(buffer);
    } else {
      nsStyleUtil::AppendEscapedCSSIdent(buffer, aResult);
    }
  }
  else if (eCSSUnit_Array <= unit && unit <= eCSSUnit_Steps) {
    switch (unit) {
      case eCSSUnit_Counter:  aResult.AppendLiteral("counter(");  break;
      case eCSSUnit_Counters: aResult.AppendLiteral("counters("); break;
      case eCSSUnit_Cubic_Bezier: aResult.AppendLiteral("cubic-bezier("); break;
      case eCSSUnit_Steps: aResult.AppendLiteral("steps("); break;
      default: break;
    }

    nsCSSValue::Array *array = GetArrayValue();
    bool mark = false;
    for (size_t i = 0, i_end = array->Count(); i < i_end; ++i) {
      if (mark && array->Item(i).GetUnit() != eCSSUnit_Null) {
        if (unit == eCSSUnit_Array &&
            eCSSProperty_transition_timing_function != aProperty)
          aResult.AppendLiteral(" ");
        else
          aResult.AppendLiteral(", ");
      }
      if (unit == eCSSUnit_Steps && i == 1) {
        NS_ABORT_IF_FALSE(array->Item(i).GetUnit() == eCSSUnit_Enumerated &&
                          (array->Item(i).GetIntValue() ==
                            NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_START ||
                           array->Item(i).GetIntValue() ==
                            NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_END),
                          "unexpected value");
        if (array->Item(i).GetIntValue() ==
              NS_STYLE_TRANSITION_TIMING_FUNCTION_STEP_START) {
          aResult.AppendLiteral("start");
        } else {
          aResult.AppendLiteral("end");
        }
        continue;
      }
      nsCSSProperty prop =
        ((eCSSUnit_Counter <= unit && unit <= eCSSUnit_Counters) &&
         i == array->Count() - 1)
        ? eCSSProperty_list_style_type : aProperty;
      if (array->Item(i).GetUnit() != eCSSUnit_Null) {
        array->Item(i).AppendToString(prop, aResult);
        mark = true;
      }
    }
    if (eCSSUnit_Array == unit &&
        aProperty == eCSSProperty_transition_timing_function) {
      aResult.AppendLiteral(")");
    }
  }
  /* Although Function is backed by an Array, we'll handle it separately
   * because it's a bit quirky.
   */
  else if (eCSSUnit_Function == unit) {
    const nsCSSValue::Array* array = GetArrayValue();
    NS_ABORT_IF_FALSE(array->Count() >= 1,
                      "Functions must have at least one element for the name.");

    /* Append the function name. */
    const nsCSSValue& functionName = array->Item(0);
    if (functionName.GetUnit() == eCSSUnit_Enumerated) {
      // We assume that the first argument is always of nsCSSKeyword type.
      const nsCSSKeyword functionId = functionName.GetKeywordValue();
      NS_ConvertASCIItoUTF16 ident(nsCSSKeywords::GetStringValue(functionId));
      // Bug 721136: Normalize the identifier to lowercase, except that things
      // like scaleX should have the last character capitalized.  This matches
      // what other browsers do.
      switch (functionId) {
        case eCSSKeyword_rotatex:
        case eCSSKeyword_scalex:
        case eCSSKeyword_skewx:
        case eCSSKeyword_translatex:
          ident.Replace(ident.Length() - 1, 1, PRUnichar('X'));
          break;

        case eCSSKeyword_rotatey:
        case eCSSKeyword_scaley:
        case eCSSKeyword_skewy:
        case eCSSKeyword_translatey:
          ident.Replace(ident.Length() - 1, 1, PRUnichar('Y'));
          break;

        case eCSSKeyword_rotatez:
        case eCSSKeyword_scalez:
        case eCSSKeyword_translatez:
          ident.Replace(ident.Length() - 1, 1, PRUnichar('Z'));
          break;

        default:
          break;
      }
      nsStyleUtil::AppendEscapedCSSIdent(ident, aResult);
    } else {
      MOZ_ASSERT(false, "should no longer have non-enumerated functions");
    }
    aResult.AppendLiteral("(");

    /* Now, step through the function contents, writing each of them as we go. */
    for (size_t index = 1; index < array->Count(); ++index) {
      array->Item(index).AppendToString(aProperty, aResult);

      /* If we're not at the final element, append a comma. */
      if (index + 1 != array->Count())
        aResult.AppendLiteral(", ");
    }

    /* Finally, append the closing parenthesis. */
    aResult.AppendLiteral(")");
  }
  else if (IsCalcUnit()) {
    NS_ABORT_IF_FALSE(GetUnit() == eCSSUnit_Calc, "unexpected unit");
    CSSValueSerializeCalcOps ops(aProperty, aResult);
    css::SerializeCalc(*this, ops);
  }
  else if (eCSSUnit_Integer == unit) {
    aResult.AppendInt(GetIntValue(), 10);
  }
  else if (eCSSUnit_Enumerated == unit) {
    int32_t intValue = GetIntValue();
    switch(aProperty) {


    case eCSSProperty_text_combine_horizontal:
      if (intValue <= NS_STYLE_TEXT_COMBINE_HORIZ_ALL) {
        AppendASCIItoUTF16(nsCSSProps::LookupPropertyValue(aProperty, intValue),
                           aResult);
      } else if (intValue == NS_STYLE_TEXT_COMBINE_HORIZ_DIGITS_2) {
        aResult.AppendLiteral("digits 2");
      } else if (intValue == NS_STYLE_TEXT_COMBINE_HORIZ_DIGITS_3) {
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

    case eCSSProperty_marks:
      if (intValue == NS_STYLE_PAGE_MARKS_NONE) {
        AppendASCIItoUTF16(nsCSSProps::LookupPropertyValue(aProperty, intValue),
                           aResult);
      } else {
        nsStyleUtil::AppendBitmaskCSSValue(aProperty, intValue,
                                           NS_STYLE_PAGE_MARKS_CROP,
                                           NS_STYLE_PAGE_MARKS_REGISTER,
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
      NS_ABORT_IF_FALSE(false, "bad color value");
    }
  }
  else if (eCSSUnit_Color == unit) {
    nscolor color = GetColorValue();
    if (color == NS_RGBA(0, 0, 0, 0)) {
      // Use the strictest match for 'transparent' so we do correct
      // round-tripping of all other rgba() values.
      aResult.AppendLiteral("transparent");
    } else {
      uint8_t a = NS_GET_A(color);
      if (a < 255) {
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
      if (a < 255) {
        aResult.Append(comma);
        aResult.AppendFloat(nsStyleUtil::ColorComponentToFloat(a));
      }
      aResult.Append(PRUnichar(')'));
    }
  }
  else if (eCSSUnit_URL == unit || eCSSUnit_Image == unit) {
    aResult.Append(NS_LITERAL_STRING("url("));
    nsStyleUtil::AppendEscapedCSSString(
      nsDependentString(GetOriginalURLValue()), aResult);
    aResult.Append(NS_LITERAL_STRING(")"));
  }
  else if (eCSSUnit_Element == unit) {
    aResult.Append(NS_LITERAL_STRING("-moz-element(#"));
    nsAutoString tmpStr;
    GetStringValue(tmpStr);
    nsStyleUtil::AppendEscapedCSSIdent(tmpStr, aResult);
    aResult.Append(NS_LITERAL_STRING(")"));
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
          NS_ABORT_IF_FALSE(gradient->GetRadialShape().GetUnit() ==
                            eCSSUnit_Enumerated,
                            "bad unit for radial gradient shape");
          int32_t intValue = gradient->GetRadialShape().GetIntValue();
          NS_ABORT_IF_FALSE(intValue != NS_STYLE_GRADIENT_SHAPE_LINEAR,
                            "radial gradient with linear shape?!");
          AppendASCIItoUTF16(nsCSSProps::ValueToKeyword(intValue,
                                 nsCSSProps::kRadialGradientShapeKTable),
                             aResult);
          needSep = true;
        }

        if (gradient->GetRadialSize().GetUnit() != eCSSUnit_None) {
          if (needSep) {
            aResult.AppendLiteral(" ");
          }
          NS_ABORT_IF_FALSE(gradient->GetRadialSize().GetUnit() ==
                            eCSSUnit_Enumerated,
                            "bad unit for radial gradient size");
          int32_t intValue = gradient->GetRadialSize().GetIntValue();
          AppendASCIItoUTF16(nsCSSProps::ValueToKeyword(intValue,
                                 nsCSSProps::kRadialGradientSizeKTable),
                             aResult);
          needSep = true;
        }
      } else {
        NS_ABORT_IF_FALSE(gradient->GetRadiusX().GetUnit() != eCSSUnit_None,
                          "bad unit for radial gradient explicit size");
        gradient->GetRadiusX().AppendToString(aProperty, aResult);
        if (gradient->GetRadiusY().GetUnit() != eCSSUnit_None) {
          aResult.AppendLiteral(" ");
          gradient->GetRadiusY().AppendToString(aProperty, aResult);
        }
        needSep = true;
      }
    }
    if (!gradient->mIsRadial && !gradient->mIsLegacySyntax) {
      if (gradient->mBgPos.mXValue.GetUnit() != eCSSUnit_None ||
          gradient->mBgPos.mYValue.GetUnit() != eCSSUnit_None) {
        MOZ_ASSERT(gradient->mAngle.GetUnit() == eCSSUnit_None);
        NS_ABORT_IF_FALSE(gradient->mBgPos.mXValue.GetUnit() == eCSSUnit_Enumerated &&
                          gradient->mBgPos.mYValue.GetUnit() == eCSSUnit_Enumerated,
                          "unexpected unit");
        aResult.AppendLiteral("to");
        if (!(gradient->mBgPos.mXValue.GetIntValue() & NS_STYLE_BG_POSITION_CENTER)) {
          aResult.AppendLiteral(" ");
          gradient->mBgPos.mXValue.AppendToString(eCSSProperty_background_position,
                                                  aResult);
        }
        if (!(gradient->mBgPos.mYValue.GetIntValue() & NS_STYLE_BG_POSITION_CENTER)) {
          aResult.AppendLiteral(" ");
          gradient->mBgPos.mYValue.AppendToString(eCSSProperty_background_position,
                                                  aResult);
        }
        needSep = true;
      } else if (gradient->mAngle.GetUnit() != eCSSUnit_None) {
        gradient->mAngle.AppendToString(aProperty, aResult);
        needSep = true;
      }
    } else if (gradient->mBgPos.mXValue.GetUnit() != eCSSUnit_None ||
        gradient->mBgPos.mYValue.GetUnit() != eCSSUnit_None ||
        gradient->mAngle.GetUnit() != eCSSUnit_None) {
      if (needSep) {
        aResult.AppendLiteral(" ");
      }
      if (gradient->mIsRadial && !gradient->mIsLegacySyntax) {
        aResult.AppendLiteral("at ");
      }
      if (gradient->mBgPos.mXValue.GetUnit() != eCSSUnit_None) {
        gradient->mBgPos.mXValue.AppendToString(eCSSProperty_background_position,
                                                aResult);
        aResult.AppendLiteral(" ");
      }
      if (gradient->mBgPos.mXValue.GetUnit() != eCSSUnit_None) {
        gradient->mBgPos.mYValue.AppendToString(eCSSProperty_background_position,
                                                aResult);
        aResult.AppendLiteral(" ");
      }
      if (gradient->mAngle.GetUnit() != eCSSUnit_None) {
        NS_ABORT_IF_FALSE(gradient->mIsLegacySyntax,
                          "angle is allowed only for legacy syntax");
        gradient->mAngle.AppendToString(aProperty, aResult);
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
        NS_ABORT_IF_FALSE(gradient->GetRadialShape().GetUnit() ==
                          eCSSUnit_Enumerated,
                          "bad unit for radial gradient shape");
        int32_t intValue = gradient->GetRadialShape().GetIntValue();
        NS_ABORT_IF_FALSE(intValue != NS_STYLE_GRADIENT_SHAPE_LINEAR,
                          "radial gradient with linear shape?!");
        AppendASCIItoUTF16(nsCSSProps::ValueToKeyword(intValue,
                               nsCSSProps::kRadialGradientShapeKTable),
                           aResult);
        aResult.AppendLiteral(" ");
      }

      if (gradient->GetRadialSize().GetUnit() != eCSSUnit_None) {
        NS_ABORT_IF_FALSE(gradient->GetRadialSize().GetUnit() ==
                          eCSSUnit_Enumerated,
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
      gradient->mStops[i].mColor.AppendToString(aProperty, aResult);
      if (gradient->mStops[i].mLocation.GetUnit() != eCSSUnit_None) {
        aResult.AppendLiteral(" ");
        gradient->mStops[i].mLocation.AppendToString(aProperty, aResult);
      }
      if (++i == gradient->mStops.Length()) {
        break;
      }
      aResult.AppendLiteral(", ");
    }

    aResult.AppendLiteral(")");
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
      nsAutoTArray<gfxAlternateValue,8> altValues;

      nsStyleUtil::ComputeFunctionalAlternates(list, altValues);
      nsStyleUtil::SerializeFunctionalAlternates(altValues, out);
      aResult.Append(out);
    } else {
      GetPairValue().AppendToString(aProperty, aResult);
    }
  } else if (eCSSUnit_Triplet == unit) {
    GetTripletValue().AppendToString(aProperty, aResult);
  } else if (eCSSUnit_Rect == unit) {
    GetRectValue().AppendToString(aProperty, aResult);
  } else if (eCSSUnit_List == unit || eCSSUnit_ListDep == unit) {
    GetListValue()->AppendToString(aProperty, aResult);
  } else if (eCSSUnit_PairList == unit || eCSSUnit_PairListDep == unit) {
    switch (aProperty) {
      case eCSSProperty_font_feature_settings:
        nsStyleUtil::AppendFontFeatureSettings(*this, aResult);
        break;
      default:
        GetPairListValue()->AppendToString(aProperty, aResult);
        break;
    }
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
      NS_ABORT_IF_FALSE(false, "should never serialize");
      break;

    case eCSSUnit_String:       break;
    case eCSSUnit_Ident:        break;
    case eCSSUnit_Families:     break;
    case eCSSUnit_URL:          break;
    case eCSSUnit_Image:        break;
    case eCSSUnit_Element:      break;
    case eCSSUnit_Array:        break;
    case eCSSUnit_Attr:
    case eCSSUnit_Cubic_Bezier:
    case eCSSUnit_Steps:
    case eCSSUnit_Counter:
    case eCSSUnit_Counters:     aResult.Append(PRUnichar(')'));    break;
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
    case eCSSUnit_EnumColor:    break;
    case eCSSUnit_Color:        break;
    case eCSSUnit_Percent:      aResult.Append(PRUnichar('%'));    break;
    case eCSSUnit_Number:       break;
    case eCSSUnit_Gradient:     break;
    case eCSSUnit_Pair:         break;
    case eCSSUnit_Triplet:      break;
    case eCSSUnit_Rect:         break;
    case eCSSUnit_List:         break;
    case eCSSUnit_ListDep:      break;
    case eCSSUnit_PairList:     break;
    case eCSSUnit_PairListDep:  break;

    case eCSSUnit_Inch:         aResult.AppendLiteral("in");   break;
    case eCSSUnit_Millimeter:   aResult.AppendLiteral("mm");   break;
    case eCSSUnit_PhysicalMillimeter: aResult.AppendLiteral("mozmm");   break;
    case eCSSUnit_Centimeter:   aResult.AppendLiteral("cm");   break;
    case eCSSUnit_Point:        aResult.AppendLiteral("pt");   break;
    case eCSSUnit_Pica:         aResult.AppendLiteral("pc");   break;

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

    case eCSSUnit_Seconds:      aResult.Append(PRUnichar('s'));    break;
    case eCSSUnit_Milliseconds: aResult.AppendLiteral("ms");   break;
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
    case eCSSUnit_Families:
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

    // PairList
    case eCSSUnit_PairList:
      n += mValue.mPairList->SizeOfIncludingThis(aMallocSizeOf);
      break;

    // PairListDep: not measured because it's non-owning.
    case eCSSUnit_PairListDep:
      break;

    // Int: nothing extra to measure.
    case eCSSUnit_Integer:
    case eCSSUnit_Enumerated:
    case eCSSUnit_EnumColor:
      break;

    // Color: nothing extra to measure.
    case eCSSUnit_Color:
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
    case eCSSUnit_Degree:
    case eCSSUnit_Grad:
    case eCSSUnit_Turn:
    case eCSSUnit_Radian:
    case eCSSUnit_Hertz:
    case eCSSUnit_Kilohertz:
    case eCSSUnit_Seconds:
    case eCSSUnit_Milliseconds:
      break;

    default:
      NS_ABORT_IF_FALSE(false, "bad nsCSSUnit");
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
  return result;
}

void
nsCSSValueList::CloneInto(nsCSSValueList* aList) const
{
    NS_ASSERTION(!aList->mNext, "Must be an empty list!");
    aList->mValue = mValue;
    aList->mNext = mNext ? mNext->Clone() : nullptr;
}

void
nsCSSValueList::AppendToString(nsCSSProperty aProperty, nsAString& aResult) const
{
  const nsCSSValueList* val = this;
  for (;;) {
    val->mValue.AppendToString(aProperty, aResult);
    val = val->mNext;
    if (!val)
      break;

    if (nsCSSProps::PropHasFlags(aProperty,
                                 CSS_PROPERTY_VALUE_LIST_USES_COMMAS))
      aResult.Append(PRUnichar(','));
    aResult.Append(PRUnichar(' '));
  }
}

bool
nsCSSValueList::operator==(const nsCSSValueList& aOther) const
{
  if (this == &aOther)
    return true;

  const nsCSSValueList *p1 = this, *p2 = &aOther;
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
  size_t n = aMallocSizeOf(this);
  n += mValue.SizeOfExcludingThis(aMallocSizeOf);
  n += mNext ? mNext->SizeOfIncludingThis(aMallocSizeOf) : 0;
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
nsCSSRect::AppendToString(nsCSSProperty aProperty, nsAString& aResult) const
{
  NS_ABORT_IF_FALSE(mTop.GetUnit() != eCSSUnit_Null &&
                    mTop.GetUnit() != eCSSUnit_Inherit &&
                    mTop.GetUnit() != eCSSUnit_Initial &&
                    mTop.GetUnit() != eCSSUnit_Unset,
                    "parser should have used a bare value");

  if (eCSSProperty_border_image_slice == aProperty ||
      eCSSProperty_border_image_width == aProperty ||
      eCSSProperty_border_image_outset == aProperty) {
    NS_NAMED_LITERAL_STRING(space, " ");

    mTop.AppendToString(aProperty, aResult);
    aResult.Append(space);
    mRight.AppendToString(aProperty, aResult);
    aResult.Append(space);
    mBottom.AppendToString(aProperty, aResult);
    aResult.Append(space);
    mLeft.AppendToString(aProperty, aResult);
  } else {
    NS_NAMED_LITERAL_STRING(comma, ", ");

    aResult.AppendLiteral("rect(");
    mTop.AppendToString(aProperty, aResult);
    aResult.Append(comma);
    mRight.AppendToString(aProperty, aResult);
    aResult.Append(comma);
    mBottom.AppendToString(aProperty, aResult);
    aResult.Append(comma);
    mLeft.AppendToString(aProperty, aResult);
    aResult.Append(PRUnichar(')'));
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
  size_t n = aMallocSizeOf(this);
  n += mTop   .SizeOfExcludingThis(aMallocSizeOf);
  n += mRight .SizeOfExcludingThis(aMallocSizeOf);
  n += mBottom.SizeOfExcludingThis(aMallocSizeOf);
  n += mLeft  .SizeOfExcludingThis(aMallocSizeOf);
  return n;
}

static_assert(NS_SIDE_TOP == 0 && NS_SIDE_RIGHT == 1 &&
              NS_SIDE_BOTTOM == 2 && NS_SIDE_LEFT == 3,
              "box side constants not top/right/bottom/left == 0/1/2/3");

/* static */ const nsCSSRect::side_type nsCSSRect::sides[4] = {
  &nsCSSRect::mTop,
  &nsCSSRect::mRight,
  &nsCSSRect::mBottom,
  &nsCSSRect::mLeft,
};

// --- nsCSSValuePair -----------------

void
nsCSSValuePair::AppendToString(nsCSSProperty aProperty,
                               nsAString& aResult) const
{
  mXValue.AppendToString(aProperty, aResult);
  if (mYValue.GetUnit() != eCSSUnit_Null) {
    aResult.Append(PRUnichar(' '));
    mYValue.AppendToString(aProperty, aResult);
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
  size_t n = aMallocSizeOf(this);
  n += mXValue.SizeOfExcludingThis(aMallocSizeOf);
  n += mYValue.SizeOfExcludingThis(aMallocSizeOf);
  return n;
}

// --- nsCSSValueTriplet -----------------

void
nsCSSValueTriplet::AppendToString(nsCSSProperty aProperty,
                               nsAString& aResult) const
{
    mXValue.AppendToString(aProperty, aResult);
    if (mYValue.GetUnit() != eCSSUnit_Null) {
        aResult.Append(PRUnichar(' '));
        mYValue.AppendToString(aProperty, aResult);
        if (mZValue.GetUnit() != eCSSUnit_Null) {
            aResult.Append(PRUnichar(' '));
            mZValue.AppendToString(aProperty, aResult);
        }
    }
}

size_t
nsCSSValueTriplet_heap::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n += mXValue.SizeOfExcludingThis(aMallocSizeOf);
  n += mYValue.SizeOfExcludingThis(aMallocSizeOf);
  n += mZValue.SizeOfExcludingThis(aMallocSizeOf);
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
  return result;
}

void
nsCSSValuePairList::AppendToString(nsCSSProperty aProperty,
                                   nsAString& aResult) const
{
  const nsCSSValuePairList* item = this;
  for (;;) {
    NS_ABORT_IF_FALSE(item->mXValue.GetUnit() != eCSSUnit_Null,
                      "unexpected null unit");
    item->mXValue.AppendToString(aProperty, aResult);
    if (item->mXValue.GetUnit() != eCSSUnit_Inherit &&
        item->mXValue.GetUnit() != eCSSUnit_Initial &&
        item->mXValue.GetUnit() != eCSSUnit_Unset &&
        item->mYValue.GetUnit() != eCSSUnit_Null) {
      aResult.Append(PRUnichar(' '));
      item->mYValue.AppendToString(aProperty, aResult);
    }
    item = item->mNext;
    if (!item)
      break;

    if (nsCSSProps::PropHasFlags(aProperty,
                                 CSS_PROPERTY_VALUE_LIST_USES_COMMAS))
      aResult.Append(PRUnichar(','));
    aResult.Append(PRUnichar(' '));
  }
}

bool
nsCSSValuePairList::operator==(const nsCSSValuePairList& aOther) const
{
  if (this == &aOther)
    return true;

  const nsCSSValuePairList *p1 = this, *p2 = &aOther;
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
  size_t n = aMallocSizeOf(this);
  n += mXValue.SizeOfExcludingThis(aMallocSizeOf);
  n += mYValue.SizeOfExcludingThis(aMallocSizeOf);
  n += mNext ? mNext->SizeOfIncludingThis(aMallocSizeOf) : 0;
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

css::URLValue::URLValue(nsIURI* aURI, nsStringBuffer* aString,
                        nsIURI* aReferrer, nsIPrincipal* aOriginPrincipal)
  : mURI(aURI),
    mString(aString),
    mReferrer(aReferrer),
    mOriginPrincipal(aOriginPrincipal),
    mURIResolved(true)
{
  NS_ABORT_IF_FALSE(aOriginPrincipal, "Must have an origin principal");
  mString->AddRef();
}

css::URLValue::URLValue(nsStringBuffer* aString, nsIURI* aBaseURI,
                        nsIURI* aReferrer, nsIPrincipal* aOriginPrincipal)
  : mURI(aBaseURI),
    mString(aString),
    mReferrer(aReferrer),
    mOriginPrincipal(aOriginPrincipal),
    mURIResolved(false)
{
  NS_ABORT_IF_FALSE(aOriginPrincipal, "Must have an origin principal");
  mString->AddRef();
}

css::URLValue::~URLValue()
{
  mString->Release();
}

bool
css::URLValue::operator==(const URLValue& aOther) const
{
  bool eq;
  return NS_strcmp(nsCSSValue::GetBufferValue(mString),
                   nsCSSValue::GetBufferValue(aOther.mString)) == 0 &&
          (GetURI() == aOther.GetURI() || // handles null == null
           (mURI && aOther.mURI &&
            NS_SUCCEEDED(mURI->Equals(aOther.mURI, &eq)) &&
            eq)) &&
          (mOriginPrincipal == aOther.mOriginPrincipal ||
           (NS_SUCCEEDED(mOriginPrincipal->Equals(aOther.mOriginPrincipal,
                                                  &eq)) && eq));
}

bool
css::URLValue::URIEquals(const URLValue& aOther) const
{
  NS_ABORT_IF_FALSE(mURIResolved && aOther.mURIResolved,
                    "How do you know the URIs aren't null?");
  bool eq;
  // Worth comparing GetURI() to aOther.GetURI() and mOriginPrincipal to
  // aOther.mOriginPrincipal, because in the (probably common) case when this
  // value was one of the ones that in fact did not change this will be our
  // fast path to equality
  return (mURI == aOther.mURI ||
          (NS_SUCCEEDED(mURI->Equals(aOther.mURI, &eq)) && eq)) &&
         (mOriginPrincipal == aOther.mOriginPrincipal ||
          (NS_SUCCEEDED(mOriginPrincipal->Equals(aOther.mOriginPrincipal,
                                                 &eq)) && eq));
}

nsIURI*
css::URLValue::GetURI() const
{
  if (!mURIResolved) {
    mURIResolved = true;
    // Be careful to not null out mURI before we've passed it as the base URI
    nsCOMPtr<nsIURI> newURI;
    NS_NewURI(getter_AddRefs(newURI),
              NS_ConvertUTF16toUTF8(nsCSSValue::GetBufferValue(mString)),
              nullptr, mURI);
    newURI.swap(mURI);
  }

  return mURI;
}

size_t
css::URLValue::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);

  // This string is unshared.
  n += mString->SizeOfIncludingThisMustBeUnshared(aMallocSizeOf);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mURI
  // - mReferrer
  // - mOriginPrincipal

  return n;
}


css::ImageValue::ImageValue(nsIURI* aURI, nsStringBuffer* aString,
                            nsIURI* aReferrer, nsIPrincipal* aOriginPrincipal,
                            nsIDocument* aDocument)
  : URLValue(aURI, aString, aReferrer, aOriginPrincipal)
{
  // NB: If aDocument is not the original document, we may not be able to load
  // images from aDocument.  Instead we do the image load from the original doc
  // and clone it to aDocument.
  nsIDocument* loadingDoc = aDocument->GetOriginalDocument();
  if (!loadingDoc) {
    loadingDoc = aDocument;
  }

  loadingDoc->StyleImageLoader()->LoadImage(aURI, aOriginPrincipal, aReferrer,
                                            this);

  if (loadingDoc != aDocument) {
    aDocument->StyleImageLoader()->MaybeRegisterCSSImage(this);
  }
}

static PLDHashOperator
ClearRequestHashtable(nsISupports* aKey, nsRefPtr<imgRequestProxy>& aValue,
                      void* aClosure)
{
  mozilla::css::ImageValue* image =
    static_cast<mozilla::css::ImageValue*>(aClosure);
  nsIDocument* doc = static_cast<nsIDocument*>(aKey);

#ifdef DEBUG
  {
    nsCOMPtr<nsIDocument> slowDoc = do_QueryInterface(aKey);
    MOZ_ASSERT(slowDoc == doc);
  }
#endif

  if (doc) {
    doc->StyleImageLoader()->DeregisterCSSImage(image);
  }

  if (aValue) {
    aValue->CancelAndForgetObserver(NS_BINDING_ABORTED);
  }

  return PL_DHASH_REMOVE;
}

css::ImageValue::~ImageValue()
{
  mRequests.Enumerate(&ClearRequestHashtable, this);
}

nsCSSValueGradientStop::nsCSSValueGradientStop()
  : mLocation(eCSSUnit_None),
    mColor(eCSSUnit_Null)
{
  MOZ_COUNT_CTOR(nsCSSValueGradientStop);
}

nsCSSValueGradientStop::nsCSSValueGradientStop(const nsCSSValueGradientStop& aOther)
  : mLocation(aOther.mLocation),
    mColor(aOther.mColor)
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
  size_t n = aMallocSizeOf(this);
  n += mBgPos.SizeOfExcludingThis(aMallocSizeOf);
  n += mAngle.SizeOfExcludingThis(aMallocSizeOf);
  n += mRadialValues[0].SizeOfExcludingThis(aMallocSizeOf);
  n += mRadialValues[1].SizeOfExcludingThis(aMallocSizeOf);
  n += mStops.SizeOfExcludingThis(aMallocSizeOf);
  for (uint32_t i = 0; i < mStops.Length(); i++) {
    n += mStops[i].SizeOfExcludingThis(aMallocSizeOf);
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

static_assert(NS_CORNER_TOP_LEFT == 0 && NS_CORNER_TOP_RIGHT == 1 &&
              NS_CORNER_BOTTOM_RIGHT == 2 && NS_CORNER_BOTTOM_LEFT == 3,
              "box corner constants not tl/tr/br/bl == 0/1/2/3");

/* static */ const nsCSSCornerSizes::corner_type
nsCSSCornerSizes::corners[4] = {
  &nsCSSCornerSizes::mTopLeft,
  &nsCSSCornerSizes::mTopRight,
  &nsCSSCornerSizes::mBottomRight,
  &nsCSSCornerSizes::mBottomLeft,
};

