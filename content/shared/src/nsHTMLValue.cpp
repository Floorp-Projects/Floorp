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

#include "nsHTMLValue.h"
#include "nsString.h"
#include "nsCRT.h"

const nsHTMLValue nsHTMLValue::kNull;

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
               (eHTMLUnit_Pixel == aUnit), "not an integer value");
  if ((eHTMLUnit_Integer == aUnit) ||
      (eHTMLUnit_Enumerated == aUnit) ||
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

nsHTMLValue::nsHTMLValue(const nsString& aValue)
  : mUnit(eHTMLUnit_String)
{
  mValue.mString = aValue.ToNewString();
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
  if (eHTMLUnit_String == mUnit) {
    if (nsnull != aCopy.mValue.mString) {
      mValue.mString = aCopy.mValue.mString->ToNewString();
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
  if (eHTMLUnit_String == mUnit) {
    if (nsnull != aCopy.mValue.mString) {
      mValue.mString = aCopy.mValue.mString->ToNewString();
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
    if (eHTMLUnit_String == mUnit) {
      if (nsnull == mValue.mString) {
        if (nsnull == aOther.mValue.mString) {
          return PR_TRUE;
        }
      }
      else if (nsnull != aOther.mValue.mString) {
        return mValue.mString->Equals(*(aOther.mValue.mString));
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

void nsHTMLValue::Reset(void)
{
  if (eHTMLUnit_String == mUnit) {
    if (nsnull != mValue.mString) {
      delete mValue.mString;
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
               (eHTMLUnit_Enumerated == aUnit), "not an int value");
  if ((eHTMLUnit_Integer == aUnit) ||
      (eHTMLUnit_Enumerated == aUnit)) {
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

void nsHTMLValue::SetStringValue(const nsString& aValue)
{
  Reset();
  mUnit = eHTMLUnit_String;
  mValue.mString = aValue.ToNewString();
}

void nsHTMLValue::SetSupportsValue(nsISupports* aValue)
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

void nsHTMLValue::AppendToString(nsString& aBuffer) const
{
  if (eHTMLUnit_Null == mUnit) {
    return;
  }

  if (eHTMLUnit_Empty == mUnit) {
  }
  else if (eHTMLUnit_String == mUnit) {
    if (nsnull != mValue.mString) {
      aBuffer.Append('"');
      aBuffer.Append(*(mValue.mString));
      aBuffer.Append('"');
    }
    else {
      aBuffer.Append("null str");
    }
  }
  else if (eHTMLUnit_ISupports == mUnit) {
    aBuffer.Append("0x");
    aBuffer.Append((PRInt32)mValue.mISupports, 16);
  }
  else if (eHTMLUnit_Color == mUnit){
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
  else if (eHTMLUnit_Percent == mUnit) {
    aBuffer.Append(mValue.mFloat);
  }
  else {
    aBuffer.Append(mValue.mInt, 10);
    aBuffer.Append("[0x");
    aBuffer.Append(mValue.mInt, 16);
    aBuffer.Append(']');
  }

  switch (mUnit) {
    case eHTMLUnit_Null:       break;
    case eHTMLUnit_Empty:      break;
    case eHTMLUnit_String:     break;
    case eHTMLUnit_ISupports:  aBuffer.Append("ptr");  break;
    case eHTMLUnit_Integer:    break;
    case eHTMLUnit_Enumerated: aBuffer.Append("enum"); break;
    case eHTMLUnit_Color:      aBuffer.Append("rbga"); break;
    case eHTMLUnit_Percent:    aBuffer.Append("%");    break;
    case eHTMLUnit_Pixel:      aBuffer.Append("px");   break;
  }
  aBuffer.Append(' ');
}

void nsHTMLValue::ToString(nsString& aBuffer) const
{
  aBuffer.Truncate();
  AppendToString(aBuffer);
}

