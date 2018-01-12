/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing values in DOM computed style */

#include "nsROCSSPrimitiveValue.h"

#include "mozilla/dom/CSSPrimitiveValueBinding.h"
#include "nsPresContext.h"
#include "nsStyleUtil.h"
#include "nsDOMCSSRGBColor.h"
#include "nsDOMCSSRect.h"
#include "nsIURI.h"
#include "nsError.h"

// There is no CSS_TURN constant on the CSSPrimitiveValue interface,
// since that unit is newer than DOM Level 2 Style, and CSS OM will
// probably expose CSS values in some other way in the future.  We
// use this value in mType for "turn"-unit angles, but we define it
// here to avoid exposing it to content.
#define CSS_TURN 30U
// Likewise we have some internal aliases for CSS_NUMBER that we don't
// want to expose.
#define CSS_NUMBER_INT32 31U
#define CSS_NUMBER_UINT32 32U

using namespace mozilla;

nsROCSSPrimitiveValue::nsROCSSPrimitiveValue()
  : CSSValue(), mType(CSSPrimitiveValueBinding::CSS_PX)
{
  mValue.mAppUnits = 0;
}


nsROCSSPrimitiveValue::~nsROCSSPrimitiveValue()
{
  Reset();
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsROCSSPrimitiveValue)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsROCSSPrimitiveValue)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsROCSSPrimitiveValue)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, CSSValue)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(nsROCSSPrimitiveValue)

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(nsROCSSPrimitiveValue)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsROCSSPrimitiveValue)
  if (tmp->mType == CSSPrimitiveValueBinding::CSS_URI) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_RAWPTR(mValue.mURI)
  } else if (tmp->mType == CSSPrimitiveValueBinding::CSS_RGBCOLOR) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_RAWPTR(mValue.mColor)
  } else if (tmp->mType == CSSPrimitiveValueBinding::CSS_RECT) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_RAWPTR(mValue.mRect)
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsROCSSPrimitiveValue)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->Reset();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

JSObject*
nsROCSSPrimitiveValue::WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto)
{
  return dom::CSSPrimitiveValueBinding::Wrap(cx, this, aGivenProto);
}

nsresult
nsROCSSPrimitiveValue::GetCssText(nsAString& aCssText)
{
  nsAutoString tmpStr;
  aCssText.Truncate();
  nsresult result = NS_OK;

  switch (mType) {
    case CSSPrimitiveValueBinding::CSS_PX:
      {
        float val = nsPresContext::AppUnitsToFloatCSSPixels(mValue.mAppUnits);
        nsStyleUtil::AppendCSSNumber(val, tmpStr);
        tmpStr.AppendLiteral("px");
        break;
      }
    case CSSPrimitiveValueBinding::CSS_IDENT:
      {
        AppendUTF8toUTF16(nsCSSKeywords::GetStringValue(mValue.mKeyword),
                          tmpStr);
        break;
      }
    case CSSPrimitiveValueBinding::CSS_STRING:
    case CSSPrimitiveValueBinding::CSS_COUNTER: /* FIXME: COUNTER should use an object */
      {
        tmpStr.Append(mValue.mString);
        break;
      }
    case CSSPrimitiveValueBinding::CSS_URI:
      {
        if (mValue.mURI) {
          nsAutoCString specUTF8;
          nsresult rv = mValue.mURI->GetSpec(specUTF8);
          NS_ENSURE_SUCCESS(rv, rv);

          tmpStr.AssignLiteral("url(");
          nsStyleUtil::AppendEscapedCSSString(NS_ConvertUTF8toUTF16(specUTF8),
                                              tmpStr);
          tmpStr.Append(')');
        } else {
          // http://dev.w3.org/csswg/css3-values/#attr defines
          // 'about:invalid' as the default value for url attributes,
          // so let's also use it here as the default computed value
          // for invalid URLs.
          tmpStr.AssignLiteral(u"url(about:invalid)");
        }
        break;
      }
    case CSSPrimitiveValueBinding::CSS_ATTR:
      {
        tmpStr.AppendLiteral("attr(");
        tmpStr.Append(mValue.mString);
        tmpStr.Append(char16_t(')'));
        break;
      }
    case CSSPrimitiveValueBinding::CSS_PERCENTAGE:
      {
        nsStyleUtil::AppendCSSNumber(mValue.mFloat * 100, tmpStr);
        tmpStr.Append(char16_t('%'));
        break;
      }
    case CSSPrimitiveValueBinding::CSS_NUMBER:
      {
        nsStyleUtil::AppendCSSNumber(mValue.mFloat, tmpStr);
        break;
      }
    case CSS_NUMBER_INT32:
      {
        tmpStr.AppendInt(mValue.mInt32);
        break;
      }
    case CSS_NUMBER_UINT32:
      {
        tmpStr.AppendInt(mValue.mUint32);
        break;
      }
    case CSSPrimitiveValueBinding::CSS_DEG:
      {
        nsStyleUtil::AppendCSSNumber(mValue.mFloat, tmpStr);
        tmpStr.AppendLiteral("deg");
        break;
      }
    case CSSPrimitiveValueBinding::CSS_GRAD:
      {
        nsStyleUtil::AppendCSSNumber(mValue.mFloat, tmpStr);
        tmpStr.AppendLiteral("grad");
        break;
      }
    case CSSPrimitiveValueBinding::CSS_RAD:
      {
        nsStyleUtil::AppendCSSNumber(mValue.mFloat, tmpStr);
        tmpStr.AppendLiteral("rad");
        break;
      }
    case CSS_TURN:
      {
        nsStyleUtil::AppendCSSNumber(mValue.mFloat, tmpStr);
        tmpStr.AppendLiteral("turn");
        break;
      }
    case CSSPrimitiveValueBinding::CSS_RECT:
      {
        NS_ASSERTION(mValue.mRect, "mValue.mRect should never be null");
        NS_NAMED_LITERAL_STRING(comma, ", ");
        nsAutoString sideValue;
        tmpStr.AssignLiteral("rect(");
        // get the top
        result = mValue.mRect->Top()->GetCssText(sideValue);
        if (NS_FAILED(result))
          break;
        tmpStr.Append(sideValue + comma);
        // get the right
        result = mValue.mRect->Right()->GetCssText(sideValue);
        if (NS_FAILED(result))
          break;
        tmpStr.Append(sideValue + comma);
        // get the bottom
        result = mValue.mRect->Bottom()->GetCssText(sideValue);
        if (NS_FAILED(result))
          break;
        tmpStr.Append(sideValue + comma);
        // get the left
        result = mValue.mRect->Left()->GetCssText(sideValue);
        if (NS_FAILED(result))
          break;
        tmpStr.Append(sideValue + NS_LITERAL_STRING(")"));
        break;
      }
    case CSSPrimitiveValueBinding::CSS_RGBCOLOR:
      {
        NS_ASSERTION(mValue.mColor, "mValue.mColor should never be null");
        ErrorResult error;
        NS_NAMED_LITERAL_STRING(comma, ", ");
        nsAutoString colorValue;
        if (mValue.mColor->HasAlpha())
          tmpStr.AssignLiteral("rgba(");
        else
          tmpStr.AssignLiteral("rgb(");

        // get the red component
        mValue.mColor->Red()->GetCssText(colorValue, error);
        if (error.Failed())
          break;
        tmpStr.Append(colorValue + comma);

        // get the green component
        mValue.mColor->Green()->GetCssText(colorValue, error);
        if (error.Failed())
          break;
        tmpStr.Append(colorValue + comma);

        // get the blue component
        mValue.mColor->Blue()->GetCssText(colorValue, error);
        if (error.Failed())
          break;
        tmpStr.Append(colorValue);

        if (mValue.mColor->HasAlpha()) {
          // get the alpha component
          mValue.mColor->Alpha()->GetCssText(colorValue, error);
          if (error.Failed())
            break;
          tmpStr.Append(comma + colorValue);
        }

        tmpStr.Append(')');

        break;
      }
    case CSSPrimitiveValueBinding::CSS_S:
      {
        nsStyleUtil::AppendCSSNumber(mValue.mFloat, tmpStr);
        tmpStr.Append('s');
        break;
      }
    case CSSPrimitiveValueBinding::CSS_CM:
    case CSSPrimitiveValueBinding::CSS_MM:
    case CSSPrimitiveValueBinding::CSS_IN:
    case CSSPrimitiveValueBinding::CSS_PT:
    case CSSPrimitiveValueBinding::CSS_PC:
    case CSSPrimitiveValueBinding::CSS_UNKNOWN:
    case CSSPrimitiveValueBinding::CSS_EMS:
    case CSSPrimitiveValueBinding::CSS_EXS:
    case CSSPrimitiveValueBinding::CSS_MS:
    case CSSPrimitiveValueBinding::CSS_HZ:
    case CSSPrimitiveValueBinding::CSS_KHZ:
    case CSSPrimitiveValueBinding::CSS_DIMENSION:
      NS_ERROR("We have a bogus value set.  This should not happen");
      return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }

  if (NS_SUCCEEDED(result)) {
    aCssText.Assign(tmpStr);
  }

  return NS_OK;
}

void
nsROCSSPrimitiveValue::GetCssText(nsString& aText, ErrorResult& aRv)
{
  aRv = GetCssText(aText);
}

void
nsROCSSPrimitiveValue::SetCssText(const nsAString& aText, ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
}

uint16_t
nsROCSSPrimitiveValue::CssValueType() const
{
  return CSSValueBinding::CSS_PRIMITIVE_VALUE;
}

void
nsROCSSPrimitiveValue::SetFloatValue(uint16_t aType, float aVal,
                                     ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
}

float
nsROCSSPrimitiveValue::GetFloatValue(uint16_t aUnitType, ErrorResult& aRv)
{
  switch(aUnitType) {
    case CSSPrimitiveValueBinding::CSS_PX:
      if (mType == CSSPrimitiveValueBinding::CSS_PX) {
        return nsPresContext::AppUnitsToFloatCSSPixels(mValue.mAppUnits);
      }

      break;
    case CSSPrimitiveValueBinding::CSS_CM:
      if (mType == CSSPrimitiveValueBinding::CSS_PX) {
        return mValue.mAppUnits * CM_PER_INCH_FLOAT /
          nsPresContext::AppUnitsPerCSSInch();
      }

      break;
    case CSSPrimitiveValueBinding::CSS_MM:
      if (mType == CSSPrimitiveValueBinding::CSS_PX) {
        return mValue.mAppUnits * MM_PER_INCH_FLOAT /
          nsPresContext::AppUnitsPerCSSInch();
      }

      break;
    case CSSPrimitiveValueBinding::CSS_IN:
      if (mType == CSSPrimitiveValueBinding::CSS_PX) {
        return mValue.mAppUnits / nsPresContext::AppUnitsPerCSSInch();
      }

      break;
    case CSSPrimitiveValueBinding::CSS_PT:
      if (mType == CSSPrimitiveValueBinding::CSS_PX) {
        return mValue.mAppUnits * POINTS_PER_INCH_FLOAT /
          nsPresContext::AppUnitsPerCSSInch();
      }

      break;
    case CSSPrimitiveValueBinding::CSS_PC:
      if (mType == CSSPrimitiveValueBinding::CSS_PX) {
        return mValue.mAppUnits * 6.0f /
          nsPresContext::AppUnitsPerCSSInch();
      }

      break;
    case CSSPrimitiveValueBinding::CSS_PERCENTAGE:
      if (mType == CSSPrimitiveValueBinding::CSS_PERCENTAGE) {
        return mValue.mFloat * 100;
      }

      break;
    case CSSPrimitiveValueBinding::CSS_NUMBER:
      if (mType == CSSPrimitiveValueBinding::CSS_NUMBER) {
        return mValue.mFloat;
      }
      if (mType == CSS_NUMBER_INT32) {
        return mValue.mInt32;
      }
      if (mType == CSS_NUMBER_UINT32) {
        return mValue.mUint32;
      }

      break;
    case CSSPrimitiveValueBinding::CSS_UNKNOWN:
    case CSSPrimitiveValueBinding::CSS_EMS:
    case CSSPrimitiveValueBinding::CSS_EXS:
    case CSSPrimitiveValueBinding::CSS_DEG:
    case CSSPrimitiveValueBinding::CSS_RAD:
    case CSSPrimitiveValueBinding::CSS_GRAD:
    case CSSPrimitiveValueBinding::CSS_MS:
    case CSSPrimitiveValueBinding::CSS_S:
    case CSSPrimitiveValueBinding::CSS_HZ:
    case CSSPrimitiveValueBinding::CSS_KHZ:
    case CSSPrimitiveValueBinding::CSS_DIMENSION:
    case CSSPrimitiveValueBinding::CSS_STRING:
    case CSSPrimitiveValueBinding::CSS_URI:
    case CSSPrimitiveValueBinding::CSS_IDENT:
    case CSSPrimitiveValueBinding::CSS_ATTR:
    case CSSPrimitiveValueBinding::CSS_COUNTER:
    case CSSPrimitiveValueBinding::CSS_RECT:
    case CSSPrimitiveValueBinding::CSS_RGBCOLOR:
      break;
  }

  aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
  return 0;
}

void
nsROCSSPrimitiveValue::SetStringValue(uint16_t aType, const nsAString& aString,
                                      mozilla::ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
}

void
nsROCSSPrimitiveValue::GetStringValue(nsString& aReturn, ErrorResult& aRv)
{
  switch (mType) {
    case CSSPrimitiveValueBinding::CSS_IDENT:
      CopyUTF8toUTF16(nsCSSKeywords::GetStringValue(mValue.mKeyword), aReturn);
      break;
    case CSSPrimitiveValueBinding::CSS_STRING:
    case CSSPrimitiveValueBinding::CSS_ATTR:
      aReturn.Assign(mValue.mString);
      break;
    case CSSPrimitiveValueBinding::CSS_URI: {
      nsAutoCString spec;
      if (mValue.mURI) {
        nsresult rv = mValue.mURI->GetSpec(spec);
        if (NS_FAILED(rv)) {
          aRv.Throw(rv);
          return;
        }
      }
      CopyUTF8toUTF16(spec, aReturn);
      break;
    }
    default:
      aReturn.Truncate();
      aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
      return;
  }
}

void
nsROCSSPrimitiveValue::GetCounterValue(ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

nsDOMCSSRect*
nsROCSSPrimitiveValue::GetRectValue(ErrorResult& aRv)
{
  if (mType != CSSPrimitiveValueBinding::CSS_RECT) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return nullptr;
  }

  NS_ASSERTION(mValue.mRect, "mValue.mRect should never be null");
  return mValue.mRect;
}

nsDOMCSSRGBColor*
nsROCSSPrimitiveValue::GetRGBColorValue(ErrorResult& aRv)
{
  if (mType != CSSPrimitiveValueBinding::CSS_RGBCOLOR) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return nullptr;
  }

  NS_ASSERTION(mValue.mColor, "mValue.mColor should never be null");
  return mValue.mColor;
}

void
nsROCSSPrimitiveValue::SetNumber(float aValue)
{
  Reset();
  mValue.mFloat = aValue;
  mType = CSSPrimitiveValueBinding::CSS_NUMBER;
}

void
nsROCSSPrimitiveValue::SetNumber(int32_t aValue)
{
  Reset();
  mValue.mInt32 = aValue;
  mType = CSS_NUMBER_INT32;
}

void
nsROCSSPrimitiveValue::SetNumber(uint32_t aValue)
{
  Reset();
  mValue.mUint32 = aValue;
  mType = CSS_NUMBER_UINT32;
}

void
nsROCSSPrimitiveValue::SetPercent(float aValue)
{
  Reset();
  mValue.mFloat = aValue;
  mType = CSSPrimitiveValueBinding::CSS_PERCENTAGE;
}

void
nsROCSSPrimitiveValue::SetDegree(float aValue)
{
  Reset();
  mValue.mFloat = aValue;
  mType = CSSPrimitiveValueBinding::CSS_DEG;
}

void
nsROCSSPrimitiveValue::SetGrad(float aValue)
{
  Reset();
  mValue.mFloat = aValue;
  mType = CSSPrimitiveValueBinding::CSS_GRAD;
}

void
nsROCSSPrimitiveValue::SetRadian(float aValue)
{
  Reset();
  mValue.mFloat = aValue;
  mType = CSSPrimitiveValueBinding::CSS_RAD;
}

void
nsROCSSPrimitiveValue::SetTurn(float aValue)
{
  Reset();
  mValue.mFloat = aValue;
  mType = CSS_TURN;
}

void
nsROCSSPrimitiveValue::SetAppUnits(nscoord aValue)
{
  Reset();
  mValue.mAppUnits = aValue;
  mType = CSSPrimitiveValueBinding::CSS_PX;
}

void
nsROCSSPrimitiveValue::SetAppUnits(float aValue)
{
  SetAppUnits(NSToCoordRound(aValue));
}

void
nsROCSSPrimitiveValue::SetIdent(nsCSSKeyword aKeyword)
{
  NS_PRECONDITION(aKeyword != eCSSKeyword_UNKNOWN &&
                  0 <= aKeyword && aKeyword < eCSSKeyword_COUNT,
                  "bad keyword");
  Reset();
  mValue.mKeyword = aKeyword;
  mType = CSSPrimitiveValueBinding::CSS_IDENT;
}

// FIXME: CSS_STRING should imply a string with "" and a need for escaping.
void
nsROCSSPrimitiveValue::SetString(const nsACString& aString, uint16_t aType)
{
  Reset();
  mValue.mString = ToNewUnicode(aString);
  if (mValue.mString) {
    mType = aType;
  } else {
    // XXXcaa We should probably let the caller know we are out of memory
    mType = CSSPrimitiveValueBinding::CSS_UNKNOWN;
  }
}

// FIXME: CSS_STRING should imply a string with "" and a need for escaping.
void
nsROCSSPrimitiveValue::SetString(const nsAString& aString, uint16_t aType)
{
  Reset();
  mValue.mString = ToNewUnicode(aString);
  if (mValue.mString) {
    mType = aType;
  } else {
    // XXXcaa We should probably let the caller know we are out of memory
    mType = CSSPrimitiveValueBinding::CSS_UNKNOWN;
  }
}

void
nsROCSSPrimitiveValue::SetURI(nsIURI *aURI)
{
  Reset();
  mValue.mURI = aURI;
  NS_IF_ADDREF(mValue.mURI);
  mType = CSSPrimitiveValueBinding::CSS_URI;
}

void
nsROCSSPrimitiveValue::SetColor(nsDOMCSSRGBColor* aColor)
{
  NS_PRECONDITION(aColor, "Null RGBColor being set!");
  Reset();
  mValue.mColor = aColor;
  if (mValue.mColor) {
    NS_ADDREF(mValue.mColor);
    mType = CSSPrimitiveValueBinding::CSS_RGBCOLOR;
  }
  else {
    mType = CSSPrimitiveValueBinding::CSS_UNKNOWN;
  }
}

void
nsROCSSPrimitiveValue::SetRect(nsDOMCSSRect* aRect)
{
  NS_PRECONDITION(aRect, "Null rect being set!");
  Reset();
  mValue.mRect = aRect;
  if (mValue.mRect) {
    NS_ADDREF(mValue.mRect);
    mType = CSSPrimitiveValueBinding::CSS_RECT;
  }
  else {
    mType = CSSPrimitiveValueBinding::CSS_UNKNOWN;
  }
}

void
nsROCSSPrimitiveValue::SetTime(float aValue)
{
  Reset();
  mValue.mFloat = aValue;
  mType = CSSPrimitiveValueBinding::CSS_S;
}

void
nsROCSSPrimitiveValue::Reset()
{
  switch (mType) {
    case CSSPrimitiveValueBinding::CSS_IDENT:
      break;
    case CSSPrimitiveValueBinding::CSS_STRING:
    case CSSPrimitiveValueBinding::CSS_ATTR:
    case CSSPrimitiveValueBinding::CSS_COUNTER: // FIXME: Counter should use an object
      NS_ASSERTION(mValue.mString, "Null string should never happen");
      free(mValue.mString);
      mValue.mString = nullptr;
      break;
    case CSSPrimitiveValueBinding::CSS_URI:
      NS_IF_RELEASE(mValue.mURI);
      break;
    case CSSPrimitiveValueBinding::CSS_RECT:
      NS_ASSERTION(mValue.mRect, "Null Rect should never happen");
      NS_RELEASE(mValue.mRect);
      break;
    case CSSPrimitiveValueBinding::CSS_RGBCOLOR:
      NS_ASSERTION(mValue.mColor, "Null RGBColor should never happen");
      NS_RELEASE(mValue.mColor);
      break;
  }

  mType = CSSPrimitiveValueBinding::CSS_UNKNOWN;
}

uint16_t
nsROCSSPrimitiveValue::PrimitiveType()
{
  // New value types were introduced but not added to CSS OM.
  // Return CSS_UNKNOWN to avoid exposing CSS_TURN to content.
  if (mType > CSSPrimitiveValueBinding::CSS_RGBCOLOR) {
    if (mType == CSS_NUMBER_INT32 ||
        mType == CSS_NUMBER_UINT32) {
      return CSSPrimitiveValueBinding::CSS_NUMBER;
    }
    return CSSPrimitiveValueBinding::CSS_UNKNOWN;
  }
  return mType;
}
