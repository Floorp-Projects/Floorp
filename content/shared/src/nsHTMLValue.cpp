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

#include "nscore.h"
#include "nsHTMLValue.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"

nsHTMLValue::nsHTMLValue(nsHTMLUnit aUnit)
  : mUnit(aUnit)
{
  NS_ASSERTION((aUnit <= eHTMLUnit_Empty), "not a valueless unit");
  if (aUnit > eHTMLUnit_Empty) {
    mUnit = eHTMLUnit_Null;
  }
  mValue.mString = nsnull;
}

nsHTMLValue::nsHTMLValue(PRInt32 aValue, nsHTMLUnit aUnit)
  : mUnit(aUnit)
{
  NS_ASSERTION((eHTMLUnit_Integer == aUnit) ||
               (eHTMLUnit_Enumerated == aUnit) ||
               (eHTMLUnit_Proportional == aUnit) ||
               (eHTMLUnit_Pixel == aUnit), "not an integer value");
  if ((eHTMLUnit_Integer == aUnit) ||
      (eHTMLUnit_Enumerated == aUnit) ||
      (eHTMLUnit_Proportional == aUnit) ||
      (eHTMLUnit_Pixel == aUnit)) {
    mValue.mInt = aValue;
  }
  else {
    mUnit = eHTMLUnit_Null;
    mValue.mInt = 0;
  }
}

nsHTMLValue::nsHTMLValue(float aValue)
  : mUnit(eHTMLUnit_Percent)
{
  mValue.mFloat = aValue;
}

nsHTMLValue::nsHTMLValue(const nsAReadableString& aValue, nsHTMLUnit aUnit)
  : mUnit(aUnit)
{
  NS_ASSERTION((eHTMLUnit_String == aUnit) ||
               (eHTMLUnit_ColorName == aUnit), "not a string value");
  if ((eHTMLUnit_String == aUnit) ||
      (eHTMLUnit_ColorName == aUnit)) {
    mValue.mString = ToNewUnicode(aValue);
  }
  else {
    mUnit = eHTMLUnit_Null;
    mValue.mInt = 0;
  }
}

nsHTMLValue::nsHTMLValue(nsISupports* aValue)
  : mUnit(eHTMLUnit_ISupports)
{
  mValue.mISupports = aValue;
  NS_IF_ADDREF(mValue.mISupports);
}

nsHTMLValue::nsHTMLValue(nscolor aValue)
  : mUnit(eHTMLUnit_Color)
{
  mValue.mColor = aValue;
}

nsHTMLValue::nsHTMLValue(const nsHTMLValue& aCopy)
  : mUnit(aCopy.mUnit)
{
  if ((eHTMLUnit_String == mUnit) || (eHTMLUnit_ColorName == mUnit)) {
    if (nsnull != aCopy.mValue.mString) {
      mValue.mString = nsCRT::strdup(aCopy.mValue.mString);
    }
    else {
      mValue.mString = nsnull;
    }
  }
  else if (eHTMLUnit_ISupports == mUnit) {
    mValue.mISupports = aCopy.mValue.mISupports;
    NS_IF_ADDREF(mValue.mISupports);
  }
  else if (eHTMLUnit_Color == mUnit){
    mValue.mColor = aCopy.mValue.mColor;
  }
  else if (eHTMLUnit_Percent == mUnit) {
    mValue.mFloat = aCopy.mValue.mFloat;
  }
  else {
    mValue.mInt = aCopy.mValue.mInt;
  }
}

nsHTMLValue::~nsHTMLValue(void)
{
  Reset();
}

nsHTMLValue& nsHTMLValue::operator=(const nsHTMLValue& aCopy)
{
  Reset();
  mUnit = aCopy.mUnit;
  if ((eHTMLUnit_String == mUnit) || (eHTMLUnit_ColorName == mUnit)) {
    if (nsnull != aCopy.mValue.mString) {
      mValue.mString = nsCRT::strdup(aCopy.mValue.mString);
    }
  }
  else if (eHTMLUnit_ISupports == mUnit) {
    mValue.mISupports = aCopy.mValue.mISupports;
    NS_IF_ADDREF(mValue.mISupports);
  }
  else if (eHTMLUnit_Color == mUnit){
    mValue.mColor = aCopy.mValue.mColor;
  }
  else if (eHTMLUnit_Percent == mUnit) {
    mValue.mFloat = aCopy.mValue.mFloat;
  }
  else {
    mValue.mInt = aCopy.mValue.mInt;
  }
  return *this;
}

PRBool nsHTMLValue::operator==(const nsHTMLValue& aOther) const
{
  if (mUnit == aOther.mUnit) {
    if ((eHTMLUnit_String == mUnit) || (eHTMLUnit_ColorName == mUnit)) {
      if (nsnull == mValue.mString) {
        if (nsnull == aOther.mValue.mString) {
          return PR_TRUE;
        }
      }
      else if (nsnull != aOther.mValue.mString) {
        return 0 == Compare(nsDependentString(mValue.mString),
                            nsDependentString(aOther.mValue.mString),
                            nsCaseInsensitiveStringComparator());
      }
    }
    else if (eHTMLUnit_ISupports == mUnit) {
      return PRBool(mValue.mISupports == aOther.mValue.mISupports);
    }
    else if (eHTMLUnit_Color == mUnit){
      return PRBool(mValue.mColor == aOther.mValue.mColor);
    }
    else if (eHTMLUnit_Percent == mUnit) {
      return PRBool(mValue.mFloat == aOther.mValue.mFloat);
    }
    else {
      return PRBool(mValue.mInt == aOther.mValue.mInt);
    }
  }
  return PR_FALSE;
}

PRUint32 nsHTMLValue::HashValue(void) const
{
  return PRUint32(mUnit) ^ 
         ((((eHTMLUnit_String == mUnit) || (eHTMLUnit_ColorName == mUnit)) && 
           (nsnull != mValue.mString)) ? 
          nsCRT::HashCode(mValue.mString) : 
          mValue.mInt);
}


void nsHTMLValue::Reset(void)
{
  if ((eHTMLUnit_String == mUnit) || (eHTMLUnit_ColorName == mUnit)) {
    if (nsnull != mValue.mString) {
      nsCRT::free(mValue.mString);
    }
  }
  else if (eHTMLUnit_ISupports == mUnit) {
    NS_IF_RELEASE(mValue.mISupports);
  }
  mUnit = eHTMLUnit_Null;
  mValue.mString = nsnull;
}


void nsHTMLValue::SetIntValue(PRInt32 aValue, nsHTMLUnit aUnit)
{
  Reset();
  NS_ASSERTION((eHTMLUnit_Integer == aUnit) ||
               (eHTMLUnit_Enumerated == aUnit) ||
               (eHTMLUnit_Proportional == aUnit), "not an int value");
  if ((eHTMLUnit_Integer == aUnit) ||
      (eHTMLUnit_Enumerated == aUnit) ||
      (eHTMLUnit_Proportional == aUnit)) {
    mUnit = aUnit;
    mValue.mInt = aValue;
  }
}

void nsHTMLValue::SetPixelValue(PRInt32 aValue)
{
  Reset();
  mUnit = eHTMLUnit_Pixel;
  mValue.mInt = aValue;
}

void nsHTMLValue::SetPercentValue(float aValue)
{
  Reset();
  mUnit = eHTMLUnit_Percent;
  mValue.mFloat = aValue;
}

void nsHTMLValue::SetStringValue(const nsAReadableString& aValue,
                                 nsHTMLUnit aUnit)
{
  Reset();
  if ((eHTMLUnit_String == aUnit) || (eHTMLUnit_ColorName == aUnit)) {
    mUnit = aUnit;
    mValue.mString = ToNewUnicode(aValue);
  }
}

void nsHTMLValue::SetISupportsValue(nsISupports* aValue)
{
  Reset();
  mUnit = eHTMLUnit_ISupports;
  mValue.mISupports = aValue;
  NS_IF_ADDREF(mValue.mISupports);
}

void nsHTMLValue::SetColorValue(nscolor aValue)
{
  Reset();
  mUnit = eHTMLUnit_Color;
  mValue.mColor = aValue;
}

void nsHTMLValue::SetEmptyValue(void)
{
  Reset();
  mUnit = eHTMLUnit_Empty;
}

#ifdef DEBUG
void nsHTMLValue::AppendToString(nsAWritableString& aBuffer) const
{
  if (eHTMLUnit_Null == mUnit) {
    return;
  }

  if (eHTMLUnit_Empty == mUnit) {
  }
  else if ((eHTMLUnit_String == mUnit) || (eHTMLUnit_ColorName == mUnit)) {
    if (nsnull != mValue.mString) {
      aBuffer.Append(PRUnichar('"'));
      aBuffer.Append(mValue.mString);
      aBuffer.Append(PRUnichar('"'));
    }
    else {
      aBuffer.Append(NS_LITERAL_STRING("null str"));
    }
  }
  else if (eHTMLUnit_ISupports == mUnit) {
    aBuffer.Append(NS_LITERAL_STRING("0x"));
    nsAutoString intStr;
    intStr.AppendInt(NS_PTR_TO_INT32(mValue.mISupports), 16);
    aBuffer.Append(intStr);
  }
  else if (eHTMLUnit_Color == mUnit){
    nsAutoString intStr;
    intStr.Append(NS_LITERAL_STRING("(0x"));
    intStr.AppendInt(NS_GET_R(mValue.mColor), 16);
    intStr.Append(NS_LITERAL_STRING(" 0x"));
    intStr.AppendInt(NS_GET_G(mValue.mColor), 16);
    intStr.Append(NS_LITERAL_STRING(" 0x"));
    intStr.AppendInt(NS_GET_B(mValue.mColor), 16);
    intStr.Append(NS_LITERAL_STRING(" 0x"));
    intStr.AppendInt(NS_GET_A(mValue.mColor), 16);
    intStr.Append(PRUnichar(')'));

    aBuffer.Append(intStr);
  }
  else if (eHTMLUnit_Percent == mUnit) {
    nsAutoString floatStr;
    floatStr.AppendFloat(mValue.mFloat * 100.0f);
    aBuffer.Append(floatStr);
  }
  else {
    nsAutoString intStr;
    intStr.AppendInt(mValue.mInt, 10);
    intStr.Append(NS_LITERAL_STRING("[0x"));
    intStr.AppendInt(mValue.mInt, 16);
    intStr.Append(PRUnichar(']'));

    aBuffer.Append(intStr);
  }

  switch (mUnit) {
    case eHTMLUnit_Null:       break;
    case eHTMLUnit_Empty:      break;
    case eHTMLUnit_String:     break;
    case eHTMLUnit_ColorName:  break;
    case eHTMLUnit_ISupports:  aBuffer.Append(NS_LITERAL_STRING("ptr"));  break;
    case eHTMLUnit_Integer:    break;
    case eHTMLUnit_Enumerated: aBuffer.Append(NS_LITERAL_STRING("enum")); break;
    case eHTMLUnit_Proportional:  aBuffer.Append(NS_LITERAL_STRING("*")); break;
    case eHTMLUnit_Color:      aBuffer.Append(NS_LITERAL_STRING("rbga")); break;
    case eHTMLUnit_Percent:    aBuffer.Append(NS_LITERAL_STRING("%"));    break;
    case eHTMLUnit_Pixel:      aBuffer.Append(NS_LITERAL_STRING("px"));   break;
  }
  aBuffer.Append(PRUnichar(' '));
}
#endif // DEBUG
