/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCSSValue.h"
#include "nsString.h"
#include "nsCSSProps.h"
#include "nsReadableUtils.h"

//#include "nsStyleConsts.h"


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

nsCSSValue::nsCSSValue(const nsAReadableString& aValue, nsCSSUnit aUnit)
  : mUnit(aUnit)
{
  NS_ASSERTION((eCSSUnit_String <= aUnit) && (aUnit <= eCSSUnit_Counters), "not a string value");
  if ((eCSSUnit_String <= aUnit) && (aUnit <= eCSSUnit_Counters)) {
    mValue.mString = ToNewUnicode(aValue);
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
      mValue.mString = nsCRT::strdup(aCopy.mValue.mString);
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

nsCSSValue& nsCSSValue::operator=(const nsCSSValue& aCopy)
{
  Reset();
  mUnit = aCopy.mUnit;
  if ((eCSSUnit_String <= mUnit) && (mUnit <= eCSSUnit_Counters)) {
    if (nsnull != aCopy.mValue.mString) {
      mValue.mString = nsCRT::strdup(aCopy.mValue.mString);
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
        return (nsCRT::strcmp(mValue.mString, aOther.mValue.mString) == 0);
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

void nsCSSValue::SetStringValue(const nsAReadableString& aValue,
                                nsCSSUnit aUnit)
{
  NS_ASSERTION((eCSSUnit_String <= aUnit) && (aUnit <= eCSSUnit_Counters), "not a string unit");
  Reset();
  if ((eCSSUnit_String <= aUnit) && (aUnit <= eCSSUnit_Counters)) {
    mUnit = aUnit;
    mValue.mString = ToNewUnicode(aValue);
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

void nsCSSValue::SetInitialValue(void)
{
  Reset();
  mUnit = eCSSUnit_Initial;
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

void nsCSSValue::AppendToString(nsAWritableString& aBuffer,
                                nsCSSProperty aPropID) const
{
  if (eCSSUnit_Null == mUnit) {
    return;
  }

  if (-1 < aPropID) {
    aBuffer.Append(NS_ConvertASCIItoUCS2(nsCSSProps::GetStringValue(aPropID)));
    aBuffer.Append(NS_LITERAL_STRING(": "));
  }

  if ((eCSSUnit_String <= mUnit) && (mUnit <= eCSSUnit_Counters)) {
    switch (mUnit) {
      case eCSSUnit_URL:      aBuffer.Append(NS_LITERAL_STRING("url("));       break;
      case eCSSUnit_Attr:     aBuffer.Append(NS_LITERAL_STRING("attr("));      break;
      case eCSSUnit_Counter:  aBuffer.Append(NS_LITERAL_STRING("counter("));   break;
      case eCSSUnit_Counters: aBuffer.Append(NS_LITERAL_STRING("counters("));  break;
      default:  break;
    }
    if (nsnull != mValue.mString) {
      aBuffer.Append(PRUnichar('"'));
      aBuffer.Append(mValue.mString);
      aBuffer.Append(PRUnichar('"'));
    }
    else {
      aBuffer.Append(NS_LITERAL_STRING("null str"));
    }
  }
  else if ((eCSSUnit_Integer <= mUnit) && (mUnit <= eCSSUnit_Enumerated)) {
    nsAutoString intStr;
    intStr.AppendInt(mValue.mInt, 10);
    aBuffer.Append(intStr);

    aBuffer.Append(NS_LITERAL_STRING("[0x"));

    intStr.Truncate();
    intStr.AppendInt(mValue.mInt, 16);
    aBuffer.Append(intStr);

    aBuffer.Append(PRUnichar(']'));
  }
  else if (eCSSUnit_Color == mUnit){
    aBuffer.Append(NS_LITERAL_STRING("(0x"));

    nsAutoString intStr;
    intStr.AppendInt(NS_GET_R(mValue.mColor), 16);
    aBuffer.Append(intStr);

    aBuffer.Append(NS_LITERAL_STRING(" 0x"));

    intStr.Truncate();
    intStr.AppendInt(NS_GET_G(mValue.mColor), 16);
    aBuffer.Append(intStr);

    aBuffer.Append(NS_LITERAL_STRING(" 0x"));

    intStr.Truncate();
    intStr.AppendInt(NS_GET_B(mValue.mColor), 16);
    aBuffer.Append(intStr);

    aBuffer.Append(NS_LITERAL_STRING(" 0x"));

    intStr.Truncate();
    intStr.AppendInt(NS_GET_A(mValue.mColor), 16);
    aBuffer.Append(intStr);

    aBuffer.Append(PRUnichar(')'));
  }
  else if (eCSSUnit_Percent == mUnit) {
    nsAutoString floatString;
    floatString.AppendFloat(mValue.mFloat * 100.0f);
    aBuffer.Append(floatString);
  }
  else if (eCSSUnit_Percent < mUnit) {
    nsAutoString floatString;
    floatString.AppendFloat(mValue.mFloat);
    aBuffer.Append(floatString);
  }

  switch (mUnit) {
    case eCSSUnit_Null:         break;
    case eCSSUnit_Auto:         aBuffer.Append(NS_LITERAL_STRING("auto"));     break;
    case eCSSUnit_Inherit:      aBuffer.Append(NS_LITERAL_STRING("inherit"));  break;
    case eCSSUnit_Initial:      aBuffer.Append(NS_LITERAL_STRING("initial"));  break;
    case eCSSUnit_None:         aBuffer.Append(NS_LITERAL_STRING("none"));     break;
    case eCSSUnit_Normal:       aBuffer.Append(NS_LITERAL_STRING("normal"));   break;
    case eCSSUnit_String:       break;
    case eCSSUnit_URL:
    case eCSSUnit_Attr:
    case eCSSUnit_Counter:
    case eCSSUnit_Counters:     aBuffer.Append(NS_LITERAL_STRING(")"));    break;
    case eCSSUnit_Integer:      aBuffer.Append(NS_LITERAL_STRING("int"));  break;
    case eCSSUnit_Enumerated:   aBuffer.Append(NS_LITERAL_STRING("enum")); break;
    case eCSSUnit_Color:        aBuffer.Append(NS_LITERAL_STRING("rbga")); break;
    case eCSSUnit_Percent:      aBuffer.Append(NS_LITERAL_STRING("%"));    break;
    case eCSSUnit_Number:       aBuffer.Append(NS_LITERAL_STRING("#"));    break;
    case eCSSUnit_Inch:         aBuffer.Append(NS_LITERAL_STRING("in"));   break;
    case eCSSUnit_Foot:         aBuffer.Append(NS_LITERAL_STRING("ft"));   break;
    case eCSSUnit_Mile:         aBuffer.Append(NS_LITERAL_STRING("mi"));   break;
    case eCSSUnit_Millimeter:   aBuffer.Append(NS_LITERAL_STRING("mm"));   break;
    case eCSSUnit_Centimeter:   aBuffer.Append(NS_LITERAL_STRING("cm"));   break;
    case eCSSUnit_Meter:        aBuffer.Append(NS_LITERAL_STRING("m"));    break;
    case eCSSUnit_Kilometer:    aBuffer.Append(NS_LITERAL_STRING("km"));   break;
    case eCSSUnit_Point:        aBuffer.Append(NS_LITERAL_STRING("pt"));   break;
    case eCSSUnit_Pica:         aBuffer.Append(NS_LITERAL_STRING("pc"));   break;
    case eCSSUnit_Didot:        aBuffer.Append(NS_LITERAL_STRING("dt"));   break;
    case eCSSUnit_Cicero:       aBuffer.Append(NS_LITERAL_STRING("cc"));   break;
    case eCSSUnit_EM:           aBuffer.Append(NS_LITERAL_STRING("em"));   break;
    case eCSSUnit_EN:           aBuffer.Append(NS_LITERAL_STRING("en"));   break;
    case eCSSUnit_XHeight:      aBuffer.Append(NS_LITERAL_STRING("ex"));   break;
    case eCSSUnit_CapHeight:    aBuffer.Append(NS_LITERAL_STRING("cap"));  break;
    case eCSSUnit_Char:         aBuffer.Append(NS_LITERAL_STRING("ch"));   break;
    case eCSSUnit_Pixel:        aBuffer.Append(NS_LITERAL_STRING("px"));   break;
    case eCSSUnit_Proportional: aBuffer.Append(NS_LITERAL_STRING("*"));    break;
    case eCSSUnit_Degree:       aBuffer.Append(NS_LITERAL_STRING("deg"));  break;
    case eCSSUnit_Grad:         aBuffer.Append(NS_LITERAL_STRING("grad")); break;
    case eCSSUnit_Radian:       aBuffer.Append(NS_LITERAL_STRING("rad"));  break;
    case eCSSUnit_Hertz:        aBuffer.Append(NS_LITERAL_STRING("Hz"));   break;
    case eCSSUnit_Kilohertz:    aBuffer.Append(NS_LITERAL_STRING("kHz"));  break;
    case eCSSUnit_Seconds:      aBuffer.Append(NS_LITERAL_STRING("s"));    break;
    case eCSSUnit_Milliseconds: aBuffer.Append(NS_LITERAL_STRING("ms"));   break;
  }
  aBuffer.Append(NS_LITERAL_STRING(" "));
}

void nsCSSValue::ToString(nsAWritableString& aBuffer,
                          nsCSSProperty aPropID) const
{
  aBuffer.Truncate();
  AppendToString(aBuffer, aPropID);
}

