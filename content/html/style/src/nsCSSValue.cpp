/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCSSValue.h"
#include "nsString.h"
#include "nsCSSProps.h"
#include "nsReadableUtils.h"
#include "imgIRequest.h"
#include "nsIDocument.h"
#include "nsContentUtils.h"

// Paint forcing
#include "prenv.h"

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

nsCSSValue::nsCSSValue(const nsAString& aValue, nsCSSUnit aUnit)
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

nsCSSValue::nsCSSValue(nsCSSValue::URL* aValue)
  : mUnit(eCSSUnit_URL)
{
  mValue.mURL = aValue;
  mValue.mURL->AddRef();
}

nsCSSValue::nsCSSValue(nsCSSValue::Image* aValue)
  : mUnit(eCSSUnit_Image)
{
  mValue.mImage = aValue;
  mValue.mImage->AddRef();
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
  else if (eCSSUnit_URL == mUnit){
    mValue.mURL = aCopy.mValue.mURL;
    mValue.mURL->AddRef();
  }
  else if (eCSSUnit_Image == mUnit){
    mValue.mImage = aCopy.mValue.mImage;
    mValue.mImage->AddRef();
  }
  else {
    mValue.mFloat = aCopy.mValue.mFloat;
  }
}

nsCSSValue::~nsCSSValue()
{
  Reset();
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
  else if (eCSSUnit_URL == mUnit){
    mValue.mURL = aCopy.mValue.mURL;
    mValue.mURL->AddRef();
  }
  else if (eCSSUnit_Image == mUnit){
    mValue.mImage = aCopy.mValue.mImage;
    mValue.mImage->AddRef();
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
      return mValue.mInt == aOther.mValue.mInt;
    }
    else if (eCSSUnit_Color == mUnit) {
      return mValue.mColor == aOther.mValue.mColor;
    }
    else if (eCSSUnit_URL == mUnit) {
      return *mValue.mURL == *aOther.mValue.mURL;
    }
    else if (eCSSUnit_Image == mUnit) {
      return *mValue.mImage == *aOther.mValue.mImage;
    }
    else {
      return mValue.mFloat == aOther.mValue.mFloat;
    }
  }
  return PR_FALSE;
}

imgIRequest* nsCSSValue::GetImageValue() const
{
  NS_ASSERTION(mUnit == eCSSUnit_Image, "not an Image value");
  return mValue.mImage->mRequest;
}

nscoord nsCSSValue::GetLengthTwips() const
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

void nsCSSValue::SetStringValue(const nsAString& aValue,
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

void nsCSSValue::SetURLValue(nsCSSValue::URL* aValue)
{
  Reset();
  mUnit = eCSSUnit_URL;
  mValue.mURL = aValue;
  mValue.mURL->AddRef();
}

void nsCSSValue::SetImageValue(nsCSSValue::Image* aValue)
{
  Reset();
  mUnit = eCSSUnit_Image;
  mValue.mImage = aValue;
  mValue.mImage->AddRef();
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

void nsCSSValue::SetNoneValue()
{
  Reset();
  mUnit = eCSSUnit_None;
}

void nsCSSValue::SetNormalValue()
{
  Reset();
  mUnit = eCSSUnit_Normal;
}

void nsCSSValue::StartImageLoad(nsIDocument* aDocument) const
{
  NS_PRECONDITION(eCSSUnit_URL == mUnit, "Not a URL value!");
  nsCSSValue::Image* image =
    new nsCSSValue::Image(mValue.mURL->mURI,
                          mValue.mURL->mString,
                          aDocument);
  if (image) {
    if (image->mString) {
      nsCSSValue* writable = NS_CONST_CAST(nsCSSValue*, this);
      writable->SetImageValue(image);
    } else {
      delete image;
    }
  }
}

nsCSSValue::Image::Image(nsIURI* aURI, const PRUnichar* aString,
                         nsIDocument* aDocument)
  : URL(aURI, aString)
{
  MOZ_COUNT_CTOR(nsCSSValue::Image);

  // Check for failed mString allocation first
  if (!mString)
    return;

  // If Paint Forcing is enabled, then force all background image loads to
  // complete before firing onload for the document
  static PRInt32 loadFlag = PR_GetEnv("MOZ_FORCE_PAINT_AFTER_ONLOAD")
    ? (PRInt32)nsIRequest::LOAD_NORMAL
    : (PRInt32)nsIRequest::LOAD_BACKGROUND;

  if (mURI &&
      nsContentUtils::CanLoadImage(mURI, aDocument, aDocument)) {
    nsContentUtils::LoadImage(mURI, aDocument, nsnull,
                              loadFlag,
                              getter_AddRefs(mRequest));
  }
}

nsCSSValue::Image::~Image()
{
  MOZ_COUNT_DTOR(nsCSSValue::Image);
}

#ifdef DEBUG

void nsCSSValue::AppendToString(nsAString& aBuffer,
                                nsCSSProperty aPropID) const
{
  if (eCSSUnit_Null == mUnit) {
    return;
  }

  if (-1 < aPropID) {
    AppendASCIItoUTF16(nsCSSProps::GetStringValue(aPropID), aBuffer);
    aBuffer.AppendLiteral(": ");
  }

  switch (mUnit) {
    case eCSSUnit_Image:
    case eCSSUnit_URL:      aBuffer.AppendLiteral("url(");       break;
    case eCSSUnit_Attr:     aBuffer.AppendLiteral("attr(");      break;
    case eCSSUnit_Counter:  aBuffer.AppendLiteral("counter(");   break;
    case eCSSUnit_Counters: aBuffer.AppendLiteral("counters(");  break;
    default:  break;
  }
  if ((eCSSUnit_String <= mUnit) && (mUnit <= eCSSUnit_Counters)) {
    if (nsnull != mValue.mString) {
      aBuffer.Append(PRUnichar('"'));
      aBuffer.Append(mValue.mString);
      aBuffer.Append(PRUnichar('"'));
    }
    else {
      aBuffer.AppendLiteral("null str");
    }
  }
  else if ((eCSSUnit_Integer <= mUnit) && (mUnit <= eCSSUnit_Enumerated)) {
    nsAutoString intStr;
    intStr.AppendInt(mValue.mInt, 10);
    aBuffer.Append(intStr);

    aBuffer.AppendLiteral("[0x");

    intStr.Truncate();
    intStr.AppendInt(mValue.mInt, 16);
    aBuffer.Append(intStr);

    aBuffer.Append(PRUnichar(']'));
  }
  else if (eCSSUnit_Color == mUnit) {
    aBuffer.AppendLiteral("(0x");

    nsAutoString intStr;
    intStr.AppendInt(NS_GET_R(mValue.mColor), 16);
    aBuffer.Append(intStr);

    aBuffer.AppendLiteral(" 0x");

    intStr.Truncate();
    intStr.AppendInt(NS_GET_G(mValue.mColor), 16);
    aBuffer.Append(intStr);

    aBuffer.AppendLiteral(" 0x");

    intStr.Truncate();
    intStr.AppendInt(NS_GET_B(mValue.mColor), 16);
    aBuffer.Append(intStr);

    aBuffer.AppendLiteral(" 0x");

    intStr.Truncate();
    intStr.AppendInt(NS_GET_A(mValue.mColor), 16);
    aBuffer.Append(intStr);

    aBuffer.Append(PRUnichar(')'));
  }
  else if (eCSSUnit_URL == mUnit) {
    aBuffer.Append(mValue.mURL->mString);
  }
  else if (eCSSUnit_Image == mUnit) {
    aBuffer.Append(mValue.mImage->mString);
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
    case eCSSUnit_Auto:         aBuffer.AppendLiteral("auto");     break;
    case eCSSUnit_Inherit:      aBuffer.AppendLiteral("inherit");  break;
    case eCSSUnit_Initial:      aBuffer.AppendLiteral("initial");  break;
    case eCSSUnit_None:         aBuffer.AppendLiteral("none");     break;
    case eCSSUnit_Normal:       aBuffer.AppendLiteral("normal");   break;
    case eCSSUnit_String:       break;
    case eCSSUnit_URL:
    case eCSSUnit_Image:
    case eCSSUnit_Attr:
    case eCSSUnit_Counter:
    case eCSSUnit_Counters:     aBuffer.Append(NS_LITERAL_STRING(")"));    break;
    case eCSSUnit_Integer:      aBuffer.AppendLiteral("int");  break;
    case eCSSUnit_Enumerated:   aBuffer.AppendLiteral("enum"); break;
    case eCSSUnit_Color:        aBuffer.AppendLiteral("rbga"); break;
    case eCSSUnit_Percent:      aBuffer.AppendLiteral("%");    break;
    case eCSSUnit_Number:       aBuffer.AppendLiteral("#");    break;
    case eCSSUnit_Inch:         aBuffer.AppendLiteral("in");   break;
    case eCSSUnit_Foot:         aBuffer.AppendLiteral("ft");   break;
    case eCSSUnit_Mile:         aBuffer.AppendLiteral("mi");   break;
    case eCSSUnit_Millimeter:   aBuffer.AppendLiteral("mm");   break;
    case eCSSUnit_Centimeter:   aBuffer.AppendLiteral("cm");   break;
    case eCSSUnit_Meter:        aBuffer.AppendLiteral("m");    break;
    case eCSSUnit_Kilometer:    aBuffer.AppendLiteral("km");   break;
    case eCSSUnit_Point:        aBuffer.AppendLiteral("pt");   break;
    case eCSSUnit_Pica:         aBuffer.AppendLiteral("pc");   break;
    case eCSSUnit_Didot:        aBuffer.AppendLiteral("dt");   break;
    case eCSSUnit_Cicero:       aBuffer.AppendLiteral("cc");   break;
    case eCSSUnit_EM:           aBuffer.AppendLiteral("em");   break;
    case eCSSUnit_EN:           aBuffer.AppendLiteral("en");   break;
    case eCSSUnit_XHeight:      aBuffer.AppendLiteral("ex");   break;
    case eCSSUnit_CapHeight:    aBuffer.AppendLiteral("cap");  break;
    case eCSSUnit_Char:         aBuffer.AppendLiteral("ch");   break;
    case eCSSUnit_Pixel:        aBuffer.AppendLiteral("px");   break;
    case eCSSUnit_Proportional: aBuffer.AppendLiteral("*");    break;
    case eCSSUnit_Degree:       aBuffer.AppendLiteral("deg");  break;
    case eCSSUnit_Grad:         aBuffer.AppendLiteral("grad"); break;
    case eCSSUnit_Radian:       aBuffer.AppendLiteral("rad");  break;
    case eCSSUnit_Hertz:        aBuffer.AppendLiteral("Hz");   break;
    case eCSSUnit_Kilohertz:    aBuffer.AppendLiteral("kHz");  break;
    case eCSSUnit_Seconds:      aBuffer.AppendLiteral("s");    break;
    case eCSSUnit_Milliseconds: aBuffer.AppendLiteral("ms");   break;
  }
  aBuffer.AppendLiteral(" ");
}

void nsCSSValue::ToString(nsAString& aBuffer,
                          nsCSSProperty aPropID) const
{
  aBuffer.Truncate();
  AppendToString(aBuffer, aPropID);
}

#endif /* defined(DEBUG) */
