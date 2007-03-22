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

/* representation of simple property values within CSS declarations */

#include "nsCSSValue.h"
#include "nsString.h"
#include "nsCSSProps.h"
#include "nsReadableUtils.h"
#include "imgIRequest.h"
#include "nsIDocument.h"
#include "nsContentUtils.h"

// Paint forcing
#include "prenv.h"

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
  NS_ASSERTION((eCSSUnit_String <= aUnit) && (aUnit <= eCSSUnit_Attr), "not a string value");
  if ((eCSSUnit_String <= aUnit) && (aUnit <= eCSSUnit_Attr)) {
    mValue.mString = BufferFromString(aValue);
    if (NS_UNLIKELY(!mValue.mString)) {
      // XXXbz not much we can do here; just make sure that our promise of a
      // non-null mValue.mString holds for string units.
      mUnit = eCSSUnit_Null;
    }
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

nsCSSValue::nsCSSValue(nsCSSValue::Array* aValue, nsCSSUnit aUnit)
  : mUnit(aUnit)
{
  NS_ASSERTION(eCSSUnit_Array <= aUnit && aUnit <= eCSSUnit_Counters,
               "bad unit");
  mValue.mArray = aValue;
  mValue.mArray->AddRef();
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
  if ((eCSSUnit_String <= mUnit) && (mUnit <= eCSSUnit_Attr)) {
    mValue.mString = aCopy.mValue.mString;
    mValue.mString->AddRef();
  }
  else if ((eCSSUnit_Integer <= mUnit) && (mUnit <= eCSSUnit_Enumerated)) {
    mValue.mInt = aCopy.mValue.mInt;
  }
  else if (eCSSUnit_Color == mUnit){
    mValue.mColor = aCopy.mValue.mColor;
  }
  else if (eCSSUnit_Array <= mUnit && mUnit <= eCSSUnit_Counters) {
    mValue.mArray = aCopy.mValue.mArray;
    mValue.mArray->AddRef();
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
  if (this != &aCopy) {
    Reset();
    new (this) nsCSSValue(aCopy);
  }
  return *this;
}

PRBool nsCSSValue::operator==(const nsCSSValue& aOther) const
{
  if (mUnit == aOther.mUnit) {
    if ((eCSSUnit_String <= mUnit) && (mUnit <= eCSSUnit_Attr)) {
      return (NS_strcmp(GetBufferValue(mValue.mString),
                        GetBufferValue(aOther.mValue.mString)) == 0);
    }
    else if ((eCSSUnit_Integer <= mUnit) && (mUnit <= eCSSUnit_Enumerated)) {
      return mValue.mInt == aOther.mValue.mInt;
    }
    else if (eCSSUnit_Color == mUnit) {
      return mValue.mColor == aOther.mValue.mColor;
    }
    else if (eCSSUnit_Array <= mUnit && mUnit <= eCSSUnit_Counters) {
      return *mValue.mArray == *aOther.mValue.mArray;
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
      return NS_POINTS_TO_TWIPS(mValue.mFloat);
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

void nsCSSValue::SetStringValue(const nsString& aValue,
                                nsCSSUnit aUnit)
{
  NS_ASSERTION((eCSSUnit_String <= aUnit) && (aUnit <= eCSSUnit_Attr), "not a string unit");
  Reset();
  if ((eCSSUnit_String <= aUnit) && (aUnit <= eCSSUnit_Attr)) {
    mUnit = aUnit;
    mValue.mString = BufferFromString(aValue);
    if (NS_UNLIKELY(!mValue.mString)) {
      // XXXbz not much we can do here; just make sure that our promise of a
      // non-null mValue.mString holds for string units.
      mUnit = eCSSUnit_Null;
    }
  }
}

void nsCSSValue::SetColorValue(nscolor aValue)
{
  Reset();
  mUnit = eCSSUnit_Color;
  mValue.mColor = aValue;
}

void nsCSSValue::SetArrayValue(nsCSSValue::Array* aValue, nsCSSUnit aUnit)
{
  NS_ASSERTION(eCSSUnit_Array <= aUnit && aUnit <= eCSSUnit_Counters,
               "bad unit");
  Reset();
  mUnit = aUnit;
  mValue.mArray = aValue;
  mValue.mArray->AddRef();
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

void nsCSSValue::StartImageLoad(nsIDocument* aDocument, PRBool aIsBGImage) const
{
  NS_PRECONDITION(eCSSUnit_URL == mUnit, "Not a URL value!");
  nsCSSValue::Image* image =
    new nsCSSValue::Image(mValue.mURL->mURI,
                          mValue.mURL->mString,
                          mValue.mURL->mReferrer,
                          aDocument, aIsBGImage);
  if (image) {
    nsCSSValue* writable = NS_CONST_CAST(nsCSSValue*, this);
    writable->SetImageValue(image);
  }
}

// static
nsStringBuffer*
nsCSSValue::BufferFromString(const nsString& aValue)
{
  nsStringBuffer* buffer = nsStringBuffer::FromString(aValue);
  if (buffer) {
    buffer->AddRef();
    return buffer;
  }
  
  PRUnichar length = aValue.Length();
  buffer = nsStringBuffer::Alloc((length + 1) * sizeof(PRUnichar));
  if (NS_LIKELY(buffer != 0)) {
    PRUnichar* data = NS_STATIC_CAST(PRUnichar*, buffer->Data());
    nsCharTraits<PRUnichar>::copy(data, aValue.get(), length);
    // Null-terminate.
    data[length] = 0;
  }

  return buffer;
}

nsCSSValue::Image::Image(nsIURI* aURI, nsStringBuffer* aString,
                         nsIURI* aReferrer, nsIDocument* aDocument,
                         PRBool aIsBGImage)
  : URL(aURI, aString, aReferrer)
{
  MOZ_COUNT_CTOR(nsCSSValue::Image);

  // If the pref is enabled, force all background image loads to
  // complete before firing onload for the document.  Otherwise, background
  // image loads are special and don't block onload.
  PRInt32 loadFlag = (PRInt32)nsIRequest::LOAD_NORMAL;
  if (aIsBGImage) {
    static PRBool onloadAfterImageBackgroundLoads =
      nsContentUtils::GetBoolPref
        ("layout.fire_onload_after_image_background_loads");
    if (!onloadAfterImageBackgroundLoads) {
      loadFlag = (PRInt32)nsIRequest::LOAD_BACKGROUND;
    }
  }

  if (mURI &&
      nsContentUtils::CanLoadImage(mURI, aDocument, aDocument)) {
    nsContentUtils::LoadImage(mURI, aDocument, aReferrer, nsnull,
                              loadFlag,
                              getter_AddRefs(mRequest));
  }
}

nsCSSValue::Image::~Image()
{
  MOZ_COUNT_DTOR(nsCSSValue::Image);
}
