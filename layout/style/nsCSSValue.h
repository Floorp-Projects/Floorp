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
#ifndef nsCSSValue_h___
#define nsCSSValue_h___

#include "nslayout.h"
#include "nsColor.h"
#include "nsString.h"
#include "nsCoord.h"
#include "nsCSSProps.h"
#include "nsUnitConversion.h"


enum nsCSSUnit {
  eCSSUnit_Null         = 0,      // (n/a) null unit, value is not specified
  eCSSUnit_Auto         = 1,      // (n/a) value is algorithmic
  eCSSUnit_Inherit      = 2,      // (n/a) value is inherited
  eCSSUnit_Initial      = 3,      // (n/a) value is default UA value
  eCSSUnit_None         = 4,      // (n/a) value is none
  eCSSUnit_Normal       = 5,      // (n/a) value is normal (algorithmic, different than auto)
  eCSSUnit_String       = 10,     // (nsString) a string value
  eCSSUnit_URL          = 11,     // (nsString) a URL value
  eCSSUnit_Attr         = 12,     // (nsString) a attr(string) value
  eCSSUnit_Counter      = 13,     // (nsString) a counter(string,[string]) value
  eCSSUnit_Counters     = 14,     // (nsString) a counters(string,string[,string]) value
  eCSSUnit_Integer      = 50,     // (int) simple value
  eCSSUnit_Enumerated   = 51,     // (int) value has enumerated meaning
  eCSSUnit_Color        = 80,     // (color) an RGBA value
  eCSSUnit_Percent      = 90,     // (float) 1.0 == 100%) value is percentage of something
  eCSSUnit_Number       = 91,     // (float) value is numeric (usually multiplier, different behavior that percent)

  // Length units - fixed
  // US English
  eCSSUnit_Inch         = 100,    // (float) Standard length
  eCSSUnit_Foot         = 101,    // (float) 12 inches
  eCSSUnit_Mile         = 102,    // (float) 5280 feet

  // Metric
  eCSSUnit_Millimeter   = 207,    // (float) 1/1000 meter
  eCSSUnit_Centimeter   = 208,    // (float) 1/100 meter
  eCSSUnit_Meter        = 210,    // (float) Standard length
  eCSSUnit_Kilometer    = 213,    // (float) 1000 meters

  // US Typographic
  eCSSUnit_Point        = 300,    // (float) 1/72 inch
  eCSSUnit_Pica         = 301,    // (float) 12 points == 1/6 inch

  // European Typographic
  eCSSUnit_Didot        = 400,    // (float) 15 didots == 16 points
  eCSSUnit_Cicero       = 401,    // (float) 12 didots

  // Length units - relative
  // Font relative measure
  eCSSUnit_EM           = 800,    // (float) == current font size
  eCSSUnit_EN           = 801,    // (float) .5 em
  eCSSUnit_XHeight      = 802,    // (float) distance from top of lower case x to baseline
  eCSSUnit_CapHeight    = 803,    // (float) distance from top of uppercase case H to baseline
  eCSSUnit_Char         = 804,    // (float) number of characters, used for width with monospace font

  // Screen relative measure
  eCSSUnit_Pixel        = 900,    // (float)

  // Proportional Unit (for columns in tables)
  eCSSUnit_Proportional = 950, 

  // Angular units
  eCSSUnit_Degree       = 1000,    // (float) 360 per circle
  eCSSUnit_Grad         = 1001,    // (float) 400 per circle
  eCSSUnit_Radian       = 1002,    // (float) 2pi per circle

  // Frequency units
  eCSSUnit_Hertz        = 2000,    // (float)
  eCSSUnit_Kilohertz    = 2001,    // (float)

  // Time units
  eCSSUnit_Seconds      = 3000,    // (float)
  eCSSUnit_Milliseconds = 3001     // (float)
};

class nsCSSValue {
public:
  // for valueless units only (null, auto, inherit, none, normal)
  nsCSSValue(nsCSSUnit aUnit = eCSSUnit_Null)
    : mUnit(aUnit)
  {
    NS_ASSERTION(aUnit <= eCSSUnit_Normal, "not a valueless unit");
    if (aUnit > eCSSUnit_Normal) {
      mUnit = eCSSUnit_Null;
    }
    mValue.mInt = 0;
  }

  nsCSSValue(PRInt32 aValue, nsCSSUnit aUnit);
  nsCSSValue(float aValue, nsCSSUnit aUnit);
  nsCSSValue(const nsAReadableString& aValue, nsCSSUnit aUnit);
  nsCSSValue(nscolor aValue);
  nsCSSValue(const nsCSSValue& aCopy);
  ~nsCSSValue(void)
  {
    Reset();
  }


  nsCSSValue&  operator=(const nsCSSValue& aCopy);
  PRBool      operator==(const nsCSSValue& aOther) const;
  PRBool      operator!=(const nsCSSValue& aOther) const;

  nsCSSUnit GetUnit(void) const { return mUnit; };
  PRBool    IsLengthUnit(void) const
    { return PRBool((eCSSUnit_Inch <= mUnit) && (mUnit <= eCSSUnit_Proportional)); }
  PRBool    IsFixedLengthUnit(void) const  
    { return PRBool((eCSSUnit_Inch <= mUnit) && (mUnit <= eCSSUnit_Cicero)); }
  PRBool    IsRelativeLengthUnit(void) const  
    { return PRBool((eCSSUnit_EM <= mUnit) && (mUnit <= eCSSUnit_Proportional)); }
  PRBool    IsAngularUnit(void) const  
    { return PRBool((eCSSUnit_Degree <= mUnit) && (mUnit <= eCSSUnit_Radian)); }
  PRBool    IsFrequencyUnit(void) const  
    { return PRBool((eCSSUnit_Hertz <= mUnit) && (mUnit <= eCSSUnit_Kilohertz)); }
  PRBool    IsTimeUnit(void) const  
    { return PRBool((eCSSUnit_Seconds <= mUnit) && (mUnit <= eCSSUnit_Milliseconds)); }

  PRInt32   GetIntValue(void) const;
  float     GetPercentValue(void) const;
  float     GetFloatValue(void) const;
  nsAWritableString& GetStringValue(nsAWritableString& aBuffer) const;
  nscolor   GetColorValue(void) const;
  nscoord   GetLengthTwips(void) const
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

  void  Reset(void)  // sets to null
  {
    if ((eCSSUnit_String <= mUnit) && (mUnit <= eCSSUnit_Counters) &&
        (nsnull != mValue.mString)) {
      nsCRT::free(mValue.mString);
    }
    mUnit = eCSSUnit_Null;
    mValue.mInt = 0;
  }

  void  SetIntValue(PRInt32 aValue, nsCSSUnit aUnit);
  void  SetPercentValue(float aValue)
  {
    Reset();
    mUnit = eCSSUnit_Percent;
    mValue.mFloat = aValue;
  }

  void  SetFloatValue(float aValue, nsCSSUnit aUnit)
  {
    NS_ASSERTION(eCSSUnit_Number <= aUnit, "not a float value");
    Reset();
    if (eCSSUnit_Number <= aUnit) {
      mUnit = aUnit;
      mValue.mFloat = aValue;
    }
  }

  void  SetStringValue(const nsAReadableString& aValue, nsCSSUnit aUnit);
  void  SetColorValue(nscolor aValue);
  void  SetAutoValue(void);
  void  SetInheritValue(void);
  void  SetInitialValue(void);
  void  SetNoneValue(void);
  void  SetNormalValue(void);

  // debugging methods only
  void  AppendToString(nsAWritableString& aBuffer, nsCSSProperty aPropID = eCSSProperty_UNKNOWN) const;
  void  ToString(nsAWritableString& aBuffer, nsCSSProperty aPropID = eCSSProperty_UNKNOWN) const;

protected:
  nsCSSUnit mUnit;
  union {
    PRInt32    mInt;
    float      mFloat;
    PRUnichar* mString;
    nscolor    mColor;
  }         mValue;
};

inline PRInt32 nsCSSValue::GetIntValue(void) const
{
  NS_ASSERTION((mUnit == eCSSUnit_Integer) ||
               (mUnit == eCSSUnit_Enumerated), "not an int value");
  if ((mUnit == eCSSUnit_Integer) ||
      (mUnit == eCSSUnit_Enumerated)) {
    return mValue.mInt;
  }
  return 0;
}

inline float nsCSSValue::GetPercentValue(void) const
{
  NS_ASSERTION((mUnit == eCSSUnit_Percent), "not a percent value");
  if ((mUnit == eCSSUnit_Percent)) {
    return mValue.mFloat;
  }
  return 0.0f;
}

inline float nsCSSValue::GetFloatValue(void) const
{
  NS_ASSERTION((eCSSUnit_Number <= mUnit), "not a float value");
  if ((mUnit >= eCSSUnit_Number)) {
    return mValue.mFloat;
  }
  return 0.0f;
}

inline nsAWritableString& nsCSSValue::GetStringValue(nsAWritableString& aBuffer) const
{
  NS_ASSERTION((eCSSUnit_String <= mUnit) && (mUnit <= eCSSUnit_Counters), "not a string value");
  aBuffer.Truncate();
  if ((eCSSUnit_String <= mUnit) && (mUnit <= eCSSUnit_Counters) && 
      (nsnull != mValue.mString)) {
    aBuffer.Append(mValue.mString);
  }
  return aBuffer;
}

inline nscolor nsCSSValue::GetColorValue(void) const
{
  NS_ASSERTION((mUnit == eCSSUnit_Color), "not a color value");
  if (mUnit == eCSSUnit_Color) {
    return mValue.mColor;
  }
  return NS_RGB(0,0,0);
}

inline PRBool nsCSSValue::operator!=(const nsCSSValue& aOther) const
{
  return PRBool(! ((*this) == aOther));
}



#endif /* nsCSSValue_h___ */

