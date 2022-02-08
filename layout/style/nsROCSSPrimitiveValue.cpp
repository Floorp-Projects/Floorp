/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing values in DOM computed style */

#include "nsROCSSPrimitiveValue.h"

#include "mozilla/ErrorResult.h"
#include "nsPresContext.h"
#include "nsStyleUtil.h"
#include "nsIURI.h"
#include "nsError.h"

using namespace mozilla;
using namespace mozilla::dom;

nsROCSSPrimitiveValue::nsROCSSPrimitiveValue() : CSSValue(), mType(CSS_PX) {
  mValue.mFloat = 0.0f;
}

nsROCSSPrimitiveValue::~nsROCSSPrimitiveValue() { Reset(); }

void nsROCSSPrimitiveValue::GetCssText(nsString& aCssText, ErrorResult& aRv) {
  nsAutoString tmpStr;
  aCssText.Truncate();

  switch (mType) {
    case CSS_PX: {
      nsStyleUtil::AppendCSSNumber(mValue.mFloat, tmpStr);
      tmpStr.AppendLiteral("px");
      break;
    }
    case CSS_STRING: {
      tmpStr.Append(mValue.mString);
      break;
    }
    case CSS_URI: {
      if (mValue.mURI) {
        nsAutoCString specUTF8;
        nsresult rv = mValue.mURI->GetSpec(specUTF8);
        if (NS_FAILED(rv)) {
          aRv.ThrowInvalidStateError("Can't get URL string for url()");
          return;
        }

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
    case CSS_PERCENTAGE: {
      nsStyleUtil::AppendCSSNumber(mValue.mFloat * 100, tmpStr);
      tmpStr.Append(char16_t('%'));
      break;
    }
    case CSS_NUMBER: {
      nsStyleUtil::AppendCSSNumber(mValue.mFloat, tmpStr);
      break;
    }
    case CSS_NUMBER_INT32: {
      tmpStr.AppendInt(mValue.mInt32);
      break;
    }
    case CSS_NUMBER_UINT32: {
      tmpStr.AppendInt(mValue.mUint32);
      break;
    }
    case CSS_DEG: {
      nsStyleUtil::AppendCSSNumber(mValue.mFloat, tmpStr);
      tmpStr.AppendLiteral("deg");
      break;
    }
    case CSS_S: {
      nsStyleUtil::AppendCSSNumber(mValue.mFloat, tmpStr);
      tmpStr.Append('s');
      break;
    }
    case CSS_CM:
    case CSS_MM:
    case CSS_IN:
    case CSS_PT:
    case CSS_PC:
    case CSS_UNKNOWN:
    case CSS_EMS:
    case CSS_EXS:
    case CSS_MS:
    case CSS_HZ:
    case CSS_KHZ:
    case CSS_DIMENSION:
      NS_ERROR("We have a bogus value set.  This should not happen");
      aRv.ThrowInvalidAccessError("Unexpected value in computed style");
      return;
  }

  aCssText.Assign(tmpStr);
}

uint16_t nsROCSSPrimitiveValue::CssValueType() const {
  return CSSValue::CSS_PRIMITIVE_VALUE;
}

void nsROCSSPrimitiveValue::SetNumber(float aValue) {
  Reset();
  mValue.mFloat = aValue;
  mType = CSS_NUMBER;
}

void nsROCSSPrimitiveValue::SetNumber(int32_t aValue) {
  Reset();
  mValue.mInt32 = aValue;
  mType = CSS_NUMBER_INT32;
}

void nsROCSSPrimitiveValue::SetNumber(uint32_t aValue) {
  Reset();
  mValue.mUint32 = aValue;
  mType = CSS_NUMBER_UINT32;
}

void nsROCSSPrimitiveValue::SetPercent(float aValue) {
  Reset();
  mValue.mFloat = aValue;
  mType = CSS_PERCENTAGE;
}

void nsROCSSPrimitiveValue::SetDegree(float aValue) {
  Reset();
  mValue.mFloat = aValue;
  mType = CSS_DEG;
}

void nsROCSSPrimitiveValue::SetAppUnits(nscoord aValue) {
  SetPixels(nsPresContext::AppUnitsToFloatCSSPixels(aValue));
}

void nsROCSSPrimitiveValue::SetPixels(float aValue) {
  Reset();
  mValue.mFloat = aValue;
  mType = CSS_PX;
}

void nsROCSSPrimitiveValue::SetAppUnits(float aValue) {
  SetAppUnits(NSToCoordRound(aValue));
}

void nsROCSSPrimitiveValue::SetString(const nsACString& aString) {
  Reset();
  mValue.mString = ToNewUnicode(aString, mozilla::fallible);
  if (mValue.mString) {
    mType = CSS_STRING;
  } else {
    // XXXcaa We should probably let the caller know we are out of memory
    mType = CSS_UNKNOWN;
  }
}

void nsROCSSPrimitiveValue::SetString(const nsAString& aString) {
  Reset();
  mValue.mString = ToNewUnicode(aString, mozilla::fallible);
  if (mValue.mString) {
    mType = CSS_STRING;
  } else {
    // XXXcaa We should probably let the caller know we are out of memory
    mType = CSS_UNKNOWN;
  }
}

void nsROCSSPrimitiveValue::SetURI(nsIURI* aURI) {
  Reset();
  mValue.mURI = aURI;
  NS_IF_ADDREF(mValue.mURI);
  mType = CSS_URI;
}

void nsROCSSPrimitiveValue::SetTime(float aValue) {
  Reset();
  mValue.mFloat = aValue;
  mType = CSS_S;
}

void nsROCSSPrimitiveValue::Reset() {
  switch (mType) {
    case CSS_STRING:
      NS_ASSERTION(mValue.mString, "Null string should never happen");
      free(mValue.mString);
      mValue.mString = nullptr;
      break;
    case CSS_URI:
      NS_IF_RELEASE(mValue.mURI);
      break;
  }

  mType = CSS_UNKNOWN;
}

uint16_t nsROCSSPrimitiveValue::PrimitiveType() {
  // New value types were introduced but not added to CSS OM.
  // Return CSS_UNKNOWN to avoid exposing CSS_TURN to content.
  if (mType > CSS_RGBCOLOR) {
    if (mType == CSS_NUMBER_INT32 || mType == CSS_NUMBER_UINT32) {
      return CSS_NUMBER;
    }
    return CSS_UNKNOWN;
  }
  return mType;
}
