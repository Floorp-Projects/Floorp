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
#include "nsMemory.h"

nsHTMLValue::nsHTMLValue(nsHTMLUnit aUnit)
  : mUnit(aUnit)
{
  NS_ASSERTION(GetUnitClass() == HTMLUNIT_NOSTORE, "not a valueless unit");
  if (GetUnitClass() != HTMLUNIT_NOSTORE) {
    mUnit = eHTMLUnit_Null;
  }
  mValue.mString = nsnull;
}

nsHTMLValue::nsHTMLValue(PRInt32 aValue, nsHTMLUnit aUnit)
  : mUnit(aUnit)
{
  NS_ASSERTION(GetUnitClass() == HTMLUNIT_INTEGER ||
               GetUnitClass() == HTMLUNIT_PIXEL, "unit not an integer unit");
  if (GetUnitClass() == HTMLUNIT_INTEGER ||
      GetUnitClass() == HTMLUNIT_PIXEL) {
    mValue.mInt = aValue;
  } else {
    mUnit = eHTMLUnit_Null;
    mValue.mString = nsnull;
  }
}

nsHTMLValue::nsHTMLValue(float aValue)
  : mUnit(eHTMLUnit_Percent)
{
  mValue.mFloat = aValue;
}

nsHTMLValue::nsHTMLValue(const nsAString& aValue, nsHTMLUnit aUnit)
  : mUnit(aUnit)
{
  SetStringValueInternal(aValue, aUnit);
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
{
  InitializeFrom(aCopy);
}

nsHTMLValue::~nsHTMLValue(void)
{
  Reset();
}

nsHTMLValue& nsHTMLValue::operator=(const nsHTMLValue& aCopy)
{
  Reset();
  InitializeFrom(aCopy);
  return *this;
}

PRBool nsHTMLValue::operator==(const nsHTMLValue& aOther) const
{
  if (mUnit != aOther.mUnit) {
    return PR_FALSE;
  }
  // Call GetUnitClass() so that we turn StringWithLength into String
  PRUint32 unitClass = GetUnitClass();
  switch (unitClass) {
    case HTMLUNIT_NOSTORE:
      return PR_TRUE;

    case HTMLUNIT_STRING:
      if (mValue.mString && aOther.mValue.mString) {
        return GetDependentString().Equals(aOther.GetDependentString(),
                                           nsCaseInsensitiveStringComparator());
      }
      // One of them is null.  An == check will see if they are both null.
      return mValue.mString == aOther.mValue.mString;

    case HTMLUNIT_INTEGER:
    case HTMLUNIT_PIXEL:
      return mValue.mInt == aOther.mValue.mInt;

    case HTMLUNIT_COLOR:
      return mValue.mColor == aOther.mValue.mColor;

    case HTMLUNIT_ISUPPORTS:
      return mValue.mISupports == aOther.mValue.mISupports;

    case HTMLUNIT_PERCENT:
      return mValue.mFloat == aOther.mValue.mFloat;

    default:
      NS_WARNING("Unknown unit");
      return PR_TRUE;
  }
}

PRUint32 nsHTMLValue::HashValue(void) const
{
  PRUint32 retval;
  if (GetUnitClass() == HTMLUNIT_STRING) {
    retval = mValue.mString ? nsCheapStringBufferUtils::HashCode(mValue.mString)
                            : 0;
  } else {
    retval = mValue.mInt;
  }
  return retval ^ PRUint32(mUnit);
}


void nsHTMLValue::Reset(void)
{
  if (GetUnitClass() == HTMLUNIT_STRING) {
    if (mValue.mString) {
      nsCheapStringBufferUtils::Free(mValue.mString);
    }
  }
  else if (mUnit == eHTMLUnit_ISupports) {
    NS_IF_RELEASE(mValue.mISupports);
  }
  mUnit = eHTMLUnit_Null;
  mValue.mString = nsnull;
}


void nsHTMLValue::SetIntValue(PRInt32 aValue, nsHTMLUnit aUnit)
{
  Reset();
  mUnit = aUnit;
  NS_ASSERTION(GetUnitClass() == HTMLUNIT_INTEGER, "not an int value");
  if (GetUnitClass() == HTMLUNIT_INTEGER) {
    mValue.mInt = aValue;
  } else {
    mUnit = eHTMLUnit_Null;
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

void nsHTMLValue::SetStringValueInternal(const nsAString& aValue,
                                         nsHTMLUnit aUnit)
{
  NS_ASSERTION(GetUnitClass() == HTMLUNIT_STRING, "unit not a string unit!");
  if (GetUnitClass() == HTMLUNIT_STRING) {
    // Remember the length of the string if necessary
    if (aValue.IsEmpty()) {
      mValue.mString = nsnull;
    } else {
      nsCheapStringBufferUtils::CopyToBuffer(mValue.mString, aValue);
    }
  } else {
    mUnit = eHTMLUnit_Null;
    mValue.mString = nsnull;
  }
}

void nsHTMLValue::SetStringValue(const nsAString& aValue,
                                 nsHTMLUnit aUnit)
{
  Reset();
  mUnit = aUnit;
  SetStringValueInternal(aValue, aUnit);
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
void nsHTMLValue::AppendToString(nsAString& aBuffer) const
{
  switch (GetUnitClass()) {
    case HTMLUNIT_NOSTORE:
      break;
    case HTMLUNIT_STRING:
      aBuffer.Append(PRUnichar('"'));
      aBuffer.Append(GetDependentString());
      aBuffer.Append(PRUnichar('"'));
      break;
    case HTMLUNIT_INTEGER:
    case HTMLUNIT_PIXEL:
      {
        nsAutoString intStr;
        intStr.AppendInt(mValue.mInt, 10);
        intStr.Append(NS_LITERAL_STRING("[0x"));
        intStr.AppendInt(mValue.mInt, 16);
        intStr.Append(PRUnichar(']'));

        aBuffer.Append(intStr);
      }
      break;
    case HTMLUNIT_COLOR:
      {
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
    break;
    case HTMLUNIT_ISUPPORTS:
      {
        aBuffer.Append(NS_LITERAL_STRING("0x"));
        nsAutoString intStr;
        intStr.AppendInt(NS_PTR_TO_INT32(mValue.mISupports), 16);
        aBuffer.Append(intStr);
      }
      break;
    case HTMLUNIT_PERCENT:
      {
        nsAutoString floatStr;
        floatStr.AppendFloat(mValue.mFloat * 100.0f);
        aBuffer.Append(floatStr);
      }
      break;
    default:
      NS_ERROR("Unknown HTMLValue type!");
  }

  //
  // Append the type name for types that are ambiguous
  //
  switch (mUnit) {
    case eHTMLUnit_Null:
      aBuffer.Append(NS_LITERAL_STRING("null"));
      break;
    case eHTMLUnit_Empty:
      aBuffer.Append(NS_LITERAL_STRING("empty"));
      break;
    case eHTMLUnit_ISupports:
      aBuffer.Append(NS_LITERAL_STRING("ptr"));
      break;
    case eHTMLUnit_Enumerated:
      aBuffer.Append(NS_LITERAL_STRING("enum"));
      break;
    case eHTMLUnit_Proportional:
      aBuffer.Append(NS_LITERAL_STRING("*"));
      break;
    case eHTMLUnit_Color:
      aBuffer.Append(NS_LITERAL_STRING("rbga"));
      break;
    case eHTMLUnit_Percent:
      aBuffer.Append(NS_LITERAL_STRING("%"));
      break;
    case eHTMLUnit_Pixel:
      aBuffer.Append(NS_LITERAL_STRING("px"));
      break;
  }
  aBuffer.Append(PRUnichar(' '));
}
#endif // DEBUG

void
nsHTMLValue::InitializeFrom(const nsHTMLValue& aCopy)
{
  mUnit = aCopy.mUnit;
  switch (GetUnitClass()) {
    case HTMLUNIT_NOSTORE:
      mValue.mString = nsnull;
      break;

    case HTMLUNIT_STRING:
      if (aCopy.mValue.mString) {
        nsCheapStringBufferUtils::Clone(mValue.mString, aCopy.mValue.mString);
      } else {
        mValue.mString = nsnull;
      }
      break;

    case HTMLUNIT_INTEGER:
    case HTMLUNIT_PIXEL:
      mValue.mInt = aCopy.mValue.mInt;
      break;

    case HTMLUNIT_COLOR:
      mValue.mColor = aCopy.mValue.mColor;
      break;

    case HTMLUNIT_ISUPPORTS:
      mValue.mISupports = aCopy.mValue.mISupports;
      NS_IF_ADDREF(mValue.mISupports);
      break;

    case HTMLUNIT_PERCENT:
      mValue.mFloat = aCopy.mValue.mFloat;
      break;

    default:
      NS_ERROR("Unknown HTMLValue type!");
  }
}
