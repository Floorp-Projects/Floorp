/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsCSSValue.h"
#include "nsString.h"
//#include "nsCRT.h"
#include "nsCSSProps.h"
#include "nsCSSPropIDs.h"
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

void nsCSSValue::AppendToString(nsString& aBuffer, PRInt32 aPropID) const
{
  if (eCSSUnit_Null == mUnit) {
    return;
  }

  if (-1 < aPropID) {
    aBuffer.Append(nsCSSProps::kNameTable[aPropID].name);
    aBuffer.Append(": ");
  }

  if ((eCSSUnit_String <= mUnit) && (mUnit <= eCSSUnit_Counters)) {
    switch (mUnit) {
      case eCSSUnit_URL:      aBuffer.Append("url(");       break;
      case eCSSUnit_Attr:     aBuffer.Append("attr(");      break;
      case eCSSUnit_Counter:  aBuffer.Append("counter(");   break;
      case eCSSUnit_Counters: aBuffer.Append("counters(");  break;
      default:  break;
    }
    if (nsnull != mValue.mString) {
      aBuffer.Append('"');
      aBuffer.Append(*(mValue.mString));
      aBuffer.Append('"');
    }
    else {
      aBuffer.Append("null str");
    }
  }
  else if ((eCSSUnit_Integer <= mUnit) && (mUnit <= eCSSUnit_Enumerated)) {
    aBuffer.Append(mValue.mInt, 10);
    aBuffer.Append("[0x");
    aBuffer.Append(mValue.mInt, 16);
    aBuffer.Append(']');
  }
  else if (eCSSUnit_Color == mUnit){
    aBuffer.Append("(0x");
    aBuffer.Append(NS_GET_R(mValue.mColor), 16);
    aBuffer.Append(" 0x");
    aBuffer.Append(NS_GET_G(mValue.mColor), 16);
    aBuffer.Append(" 0x");
    aBuffer.Append(NS_GET_B(mValue.mColor), 16);
    aBuffer.Append(" 0x");
    aBuffer.Append(NS_GET_A(mValue.mColor), 16);
    aBuffer.Append(')');
  }
  else if (eCSSUnit_Percent == mUnit) {
    aBuffer.Append(mValue.mFloat * 100.0f);
  }
  else if (eCSSUnit_Percent < mUnit) {
    aBuffer.Append(mValue.mFloat);
  }

  switch (mUnit) {
    case eCSSUnit_Null:         break;
    case eCSSUnit_Auto:         aBuffer.Append("auto");     break;
    case eCSSUnit_Inherit:      aBuffer.Append("inherit");  break;
    case eCSSUnit_None:         aBuffer.Append("none");     break;
    case eCSSUnit_Normal:       aBuffer.Append("normal");   break;
    case eCSSUnit_String:       break;
    case eCSSUnit_URL:
    case eCSSUnit_Attr:
    case eCSSUnit_Counter:
    case eCSSUnit_Counters:     aBuffer.Append(')');    break;
    case eCSSUnit_Integer:      aBuffer.Append("int");  break;
    case eCSSUnit_Enumerated:   aBuffer.Append("enum"); break;
    case eCSSUnit_Color:        aBuffer.Append("rbga"); break;
    case eCSSUnit_Percent:      aBuffer.Append("%");    break;
    case eCSSUnit_Number:       aBuffer.Append("#");    break;
    case eCSSUnit_Inch:         aBuffer.Append("in");   break;
    case eCSSUnit_Foot:         aBuffer.Append("ft");   break;
    case eCSSUnit_Mile:         aBuffer.Append("mi");   break;
    case eCSSUnit_Millimeter:   aBuffer.Append("mm");   break;
    case eCSSUnit_Centimeter:   aBuffer.Append("cm");   break;
    case eCSSUnit_Meter:        aBuffer.Append("m");    break;
    case eCSSUnit_Kilometer:    aBuffer.Append("km");   break;
    case eCSSUnit_Point:        aBuffer.Append("pt");   break;
    case eCSSUnit_Pica:         aBuffer.Append("pc");   break;
    case eCSSUnit_Didot:        aBuffer.Append("dt");   break;
    case eCSSUnit_Cicero:       aBuffer.Append("cc");   break;
    case eCSSUnit_EM:           aBuffer.Append("em");   break;
    case eCSSUnit_EN:           aBuffer.Append("en");   break;
    case eCSSUnit_XHeight:      aBuffer.Append("ex");   break;
    case eCSSUnit_CapHeight:    aBuffer.Append("cap");  break;
    case eCSSUnit_Pixel:        aBuffer.Append("px");   break;
    case eCSSUnit_Degree:       aBuffer.Append("deg");  break;
    case eCSSUnit_Grad:         aBuffer.Append("grad"); break;
    case eCSSUnit_Radian:       aBuffer.Append("rad");  break;
    case eCSSUnit_Hertz:        aBuffer.Append("Hz");   break;
    case eCSSUnit_Kilohertz:    aBuffer.Append("kHz");  break;
    case eCSSUnit_Seconds:      aBuffer.Append("s");    break;
    case eCSSUnit_Milliseconds: aBuffer.Append("ms");   break;
  }
  aBuffer.Append(' ');
}

void nsCSSValue::ToString(nsString& aBuffer, PRInt32 aPropID) const
{
  aBuffer.Truncate();
  AppendToString(aBuffer, aPropID);
}

