/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsCSSValue.h"
#include "nsString.h"
#include "nsCSSProps.h"
#include "nsUnitConversion.h"

//#include "nsStyleConsts.h"


nsCSSValue::nsCSSValue(nsCSSUnit aUnit)
  : mUnit(aUnit)
{
  NS_ASSERTION(aUnit <= eCSSUnit_Normal, "not a valueless unit");
  if (aUnit > eCSSUnit_Normal) {
    mUnit = eCSSUnit_Null;
  }
  mValue.mInt = 0;
}

nsCSSValue::nsCSSValue(PRInt32 aValue, nsCSSUnit aUnit)
  : mUnit(aUnit)
{
  NS_ASSERTION((eCSSUnit_Integer == aUnit) ||
               (eCSSUnit_Enumerated == aUnit), "not an int value");
  if ((eCSSUnit_Integer == aUnit) ||
      (eCSSUnit_Enumerated == aUnit)) {
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
  NS_ASSERTION(eCSSUnit_Percent <= aUnit, "not a float value");
  if (eCSSUnit_Percent <= aUnit) {
    mValue.mFloat = aValue;
  }
  else {
    mUnit = eCSSUnit_Null;
    mValue.mInt = 0;
  }
}

nsCSSValue::nsCSSValue(const nsString& aValue, nsCSSUnit aUnit)
  : mUnit(aUnit)
{
  NS_ASSERTION((eCSSUnit_String <= aUnit) && (aUnit <= eCSSUnit_Counters), "not a string value");
  if ((eCSSUnit_String <= aUnit) && (aUnit <= eCSSUnit_Counters)) {
    mValue.mString = aValue.ToNewString();
  }
  else {
    mUnit = eCSSUnit_Null;
    mValue.mInt = 0;
  }
}

nsCSSValue::nsCSSValue(nscolor aValue)
  : mUnit(eCSSUnit_Color)
{
  mValue.mColor = aValue;
}

nsCSSValue::nsCSSValue(const nsCSSValue& aCopy)
  : mUnit(aCopy.mUnit)
{
  if ((eCSSUnit_String <= mUnit) && (mUnit <= eCSSUnit_Counters)) {
    if (nsnull != aCopy.mValue.mString) {
      mValue.mString = (aCopy.mValue.mString)->ToNewString();
    }
    else {
      mValue.mString = nsnull;
    }
  }
  else if ((eCSSUnit_Integer <= mUnit) && (mUnit <= eCSSUnit_Enumerated)) {
    mValue.mInt = aCopy.mValue.mInt;
  }
  else if (eCSSUnit_Color == mUnit){
    mValue.mColor = aCopy.mValue.mColor;
  }
  else {
    mValue.mFloat = aCopy.mValue.mFloat;
  }
}

nsCSSValue::~nsCSSValue(void)
{
  Reset();
}

nsCSSValue& nsCSSValue::operator=(const nsCSSValue& aCopy)
{
  Reset();
  mUnit = aCopy.mUnit;
  if ((eCSSUnit_String <= mUnit) && (mUnit <= eCSSUnit_Counters)) {
    if (nsnull != aCopy.mValue.mString) {
      mValue.mString = (aCopy.mValue.mString)->ToNewString();
    }
  }
  else if ((eCSSUnit_Integer <= mUnit) && (mUnit <= eCSSUnit_Enumerated)) {
    mValue.mInt = aCopy.mValue.mInt;
  }
  else if (eCSSUnit_Color == mUnit){
    mValue.mColor = aCopy.mValue.mColor;
  }
  else {
    mValue.mFloat = aCopy.mValue.mFloat;
  }
  return *this;
}

PRBool nsCSSValue::operator==(const nsCSSValue& aOther) const
{
  if (mUnit == aOther.mUnit) {
    if ((eCSSUnit_String <= mUnit) && (mUnit <= eCSSUnit_Counters)) {
      if (nsnull == mValue.mString) {
        if (nsnull == aOther.mValue.mString) {
          return PR_TRUE;
        }
      }
      else if (nsnull != aOther.mValue.mString) {
        return mValue.mString->Equals(*(aOther.mValue.mString));
      }
    }
    else if ((eCSSUnit_Integer <= mUnit) && (mUnit <= eCSSUnit_Enumerated)) {
      return PRBool(mValue.mInt == aOther.mValue.mInt);
    }
    else if (eCSSUnit_Color == mUnit){
      return PRBool(mValue.mColor == aOther.mValue.mColor);
    }
    else {
      return PRBool(mValue.mFloat == aOther.mValue.mFloat);
    }
  }
  return PR_FALSE;
}

nscoord nsCSSValue::GetLengthTwips(void) const
{
  NS_ASSERTION(IsFixedLengthUnit(), "not a fixed length unit");

  if (IsFixedLengthUnit()) {
    switch (mUnit) {
    case eCSSUnit_Inch:        
      return NS_INCHES_TO_TWIPS(mValue.mFloat);
    case eCSSUnit_Foot:        
      return NS_FEET_TO_TWIPS(mValue.mFloat);
    case eCSSUnit_Mile:        
      return NS_MILES_TO_TWIPS(mValue.mFloat);

    case eCSSUnit_Millimeter:
      return NS_MILLIMETERS_TO_TWIPS(mValue.mFloat);
    case eCSSUnit_Centimeter:
      return NS_CENTIMETERS_TO_TWIPS(mValue.mFloat);
    case eCSSUnit_Meter:
      return NS_METERS_TO_TWIPS(mValue.mFloat);
    case eCSSUnit_Kilometer:
      return NS_KILOMETERS_TO_TWIPS(mValue.mFloat);

    case eCSSUnit_Point:
      return NSFloatPointsToTwips(mValue.mFloat);
    case eCSSUnit_Pica:
      return NS_PICAS_TO_TWIPS(mValue.mFloat);
    case eCSSUnit_Didot:
      return NS_DIDOTS_TO_TWIPS(mValue.mFloat);
    case eCSSUnit_Cicero:
      return NS_CICEROS_TO_TWIPS(mValue.mFloat);
    default:
      NS_ERROR("should never get here");
      break;
    }
  }
  return 0;
}

void nsCSSValue::Reset(void)
{
  if ((eCSSUnit_String <= mUnit) && (mUnit <= eCSSUnit_Counters) &&
      (nsnull != mValue.mString)) {
    delete mValue.mString;
  }
  mUnit = eCSSUnit_Null;
  mValue.mInt = 0;
};

void nsCSSValue::SetIntValue(PRInt32 aValue, nsCSSUnit aUnit)
{
  NS_ASSERTION((eCSSUnit_Integer == aUnit) ||
               (eCSSUnit_Enumerated == aUnit), "not an int value");
  Reset();
  if ((eCSSUnit_Integer == aUnit) ||
      (eCSSUnit_Enumerated == aUnit)) {
    mUnit = aUnit;
    mValue.mInt = aValue;
  }
}

void nsCSSValue::SetPercentValue(float aValue)
{
  Reset();
  mUnit = eCSSUnit_Percent;
  mValue.mFloat = aValue;
}

void nsCSSValue::SetFloatValue(float aValue, nsCSSUnit aUnit)
{
  NS_ASSERTION(eCSSUnit_Number <= aUnit, "not a float value");
  Reset();
  if (eCSSUnit_Number <= aUnit) {
    mUnit = aUnit;
    mValue.mFloat = aValue;
  }
}

void nsCSSValue::SetStringValue(const nsString& aValue, nsCSSUnit aUnit)
{
  NS_ASSERTION((eCSSUnit_String <= aUnit) && (aUnit <= eCSSUnit_Counters), "not a string unit");
  Reset();
  if ((eCSSUnit_String <= aUnit) && (aUnit <= eCSSUnit_Counters)) {
    mUnit = aUnit;
    mValue.mString = aValue.ToNewString();
  }
}

void nsCSSValue::SetColorValue(nscolor aValue)
{
  Reset();
  mUnit = eCSSUnit_Color;
  mValue.mColor = aValue;
}

void nsCSSValue::SetAutoValue(void)
{
  Reset();
  mUnit = eCSSUnit_Auto;
}

void nsCSSValue::SetInheritValue(void)
{
  Reset();
  mUnit = eCSSUnit_Inherit;
}

void nsCSSValue::SetNoneValue(void)
{
  Reset();
  mUnit = eCSSUnit_None;
}

void nsCSSValue::SetNormalValue(void)
{
  Reset();
  mUnit = eCSSUnit_Normal;
}

void nsCSSValue::AppendToString(nsString& aBuffer, nsCSSProperty aPropID) const
{
  if (eCSSUnit_Null == mUnit) {
    return;
  }

  if (-1 < aPropID) {
    aBuffer.AppendWithConversion(nsCSSProps::GetStringValue(aPropID));
    aBuffer.AppendWithConversion(": ");
  }

  if ((eCSSUnit_String <= mUnit) && (mUnit <= eCSSUnit_Counters)) {
    switch (mUnit) {
      case eCSSUnit_URL:      aBuffer.AppendWithConversion("url(");       break;
      case eCSSUnit_Attr:     aBuffer.AppendWithConversion("attr(");      break;
      case eCSSUnit_Counter:  aBuffer.AppendWithConversion("counter(");   break;
      case eCSSUnit_Counters: aBuffer.AppendWithConversion("counters(");  break;
      default:  break;
    }
    if (nsnull != mValue.mString) {
      aBuffer.AppendWithConversion('"');
      aBuffer.Append(*(mValue.mString));
      aBuffer.AppendWithConversion('"');
    }
    else {
      aBuffer.AppendWithConversion("null str");
    }
  }
  else if ((eCSSUnit_Integer <= mUnit) && (mUnit <= eCSSUnit_Enumerated)) {
    aBuffer.AppendInt(mValue.mInt, 10);
    aBuffer.AppendWithConversion("[0x");
    aBuffer.AppendInt(mValue.mInt, 16);
    aBuffer.AppendWithConversion(']');
  }
  else if (eCSSUnit_Color == mUnit){
    aBuffer.AppendWithConversion("(0x");
    aBuffer.AppendInt(NS_GET_R(mValue.mColor), 16);
    aBuffer.AppendWithConversion(" 0x");
    aBuffer.AppendInt(NS_GET_G(mValue.mColor), 16);
    aBuffer.AppendWithConversion(" 0x");
    aBuffer.AppendInt(NS_GET_B(mValue.mColor), 16);
    aBuffer.AppendWithConversion(" 0x");
    aBuffer.AppendInt(NS_GET_A(mValue.mColor), 16);
    aBuffer.AppendWithConversion(')');
  }
  else if (eCSSUnit_Percent == mUnit) {
    aBuffer.AppendFloat(mValue.mFloat * 100.0f);
  }
  else if (eCSSUnit_Percent < mUnit) {
    aBuffer.AppendFloat(mValue.mFloat);
  }

  switch (mUnit) {
    case eCSSUnit_Null:         break;
    case eCSSUnit_Auto:         aBuffer.AppendWithConversion("auto");     break;
    case eCSSUnit_Inherit:      aBuffer.AppendWithConversion("inherit");  break;
    case eCSSUnit_None:         aBuffer.AppendWithConversion("none");     break;
    case eCSSUnit_Normal:       aBuffer.AppendWithConversion("normal");   break;
    case eCSSUnit_String:       break;
    case eCSSUnit_URL:
    case eCSSUnit_Attr:
    case eCSSUnit_Counter:
    case eCSSUnit_Counters:     aBuffer.AppendWithConversion(')');    break;
    case eCSSUnit_Integer:      aBuffer.AppendWithConversion("int");  break;
    case eCSSUnit_Enumerated:   aBuffer.AppendWithConversion("enum"); break;
    case eCSSUnit_Color:        aBuffer.AppendWithConversion("rbga"); break;
    case eCSSUnit_Percent:      aBuffer.AppendWithConversion("%");    break;
    case eCSSUnit_Number:       aBuffer.AppendWithConversion("#");    break;
    case eCSSUnit_Inch:         aBuffer.AppendWithConversion("in");   break;
    case eCSSUnit_Foot:         aBuffer.AppendWithConversion("ft");   break;
    case eCSSUnit_Mile:         aBuffer.AppendWithConversion("mi");   break;
    case eCSSUnit_Millimeter:   aBuffer.AppendWithConversion("mm");   break;
    case eCSSUnit_Centimeter:   aBuffer.AppendWithConversion("cm");   break;
    case eCSSUnit_Meter:        aBuffer.AppendWithConversion("m");    break;
    case eCSSUnit_Kilometer:    aBuffer.AppendWithConversion("km");   break;
    case eCSSUnit_Point:        aBuffer.AppendWithConversion("pt");   break;
    case eCSSUnit_Pica:         aBuffer.AppendWithConversion("pc");   break;
    case eCSSUnit_Didot:        aBuffer.AppendWithConversion("dt");   break;
    case eCSSUnit_Cicero:       aBuffer.AppendWithConversion("cc");   break;
    case eCSSUnit_EM:           aBuffer.AppendWithConversion("em");   break;
    case eCSSUnit_EN:           aBuffer.AppendWithConversion("en");   break;
    case eCSSUnit_XHeight:      aBuffer.AppendWithConversion("ex");   break;
    case eCSSUnit_CapHeight:    aBuffer.AppendWithConversion("cap");  break;
    case eCSSUnit_Char:         aBuffer.AppendWithConversion("ch");   break;
    case eCSSUnit_Pixel:        aBuffer.AppendWithConversion("px");   break;
    case eCSSUnit_Degree:       aBuffer.AppendWithConversion("deg");  break;
    case eCSSUnit_Grad:         aBuffer.AppendWithConversion("grad"); break;
    case eCSSUnit_Radian:       aBuffer.AppendWithConversion("rad");  break;
    case eCSSUnit_Hertz:        aBuffer.AppendWithConversion("Hz");   break;
    case eCSSUnit_Kilohertz:    aBuffer.AppendWithConversion("kHz");  break;
    case eCSSUnit_Seconds:      aBuffer.AppendWithConversion("s");    break;
    case eCSSUnit_Milliseconds: aBuffer.AppendWithConversion("ms");   break;
  }
  aBuffer.AppendWithConversion(' ');
}

void nsCSSValue::ToString(nsString& aBuffer, nsCSSProperty aPropID) const
{
  aBuffer.Truncate();
  AppendToString(aBuffer, aPropID);
}

