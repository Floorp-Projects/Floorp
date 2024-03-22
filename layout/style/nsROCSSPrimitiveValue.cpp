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
#include "nsError.h"

using namespace mozilla;
using namespace mozilla::dom;

nsROCSSPrimitiveValue::nsROCSSPrimitiveValue() : CSSValue(), mType(CSS_PX) {
  mValue.mFloat = 0.0f;
}

nsROCSSPrimitiveValue::~nsROCSSPrimitiveValue() { Reset(); }

void nsROCSSPrimitiveValue::GetCssText(nsAString& aCssText) {
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

void nsROCSSPrimitiveValue::SetPixels(float aValue) {
  Reset();
  mValue.mFloat = aValue;
  mType = CSS_PX;
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
