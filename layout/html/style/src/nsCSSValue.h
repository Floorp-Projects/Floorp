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
#ifndef nsCSSValue_h___
#define nsCSSValue_h___

#include "nslayout.h"
#include "nsColor.h"
#include "nsString.h"
#include "nsCoord.h"
#include "nsCSSProps.h"


enum nsCSSUnit {
  eCSSUnit_Null         = 0,      // (n/a) null unit, value is not specified
  eCSSUnit_Auto         = 1,      // (n/a) value is algorithmic
  eCSSUnit_Inherit      = 2,      // (n/a) value is inherited
  eCSSUnit_None         = 3,      // (n/a) value is none
  eCSSUnit_Normal       = 4,      // (n/a) value is normal (algorithmic, different than auto)
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
  nsCSSValue(nsCSSUnit aUnit = eCSSUnit_Null);  // for valueless units only (null, auto, inherit, none, normal)
  nsCSSValue(PRInt32 aValue, nsCSSUnit aUnit);
  nsCSSValue(float aValue, nsCSSUnit aUnit);
  nsCSSValue(const nsAReadableString& aValue, nsCSSUnit aUnit);
  nsCSSValue(nscolor aValue);
  nsCSSValue(const nsCSSValue& aCopy);
  ~nsCSSValue(void);

  nsCSSValue&  operator=(const nsCSSValue& aCopy);
  PRBool      operator==(const nsCSSValue& aOther) const;
  PRBool      operator!=(const nsCSSValue& aOther) const;

  nsCSSUnit GetUnit(void) const { return mUnit; };
  PRBool    IsLengthUnit(void) const  
    { return PRBool((eCSSUnit_Inch <= mUnit) && (mUnit <= eCSSUnit_Pixel)); }
  PRBool    IsFixedLengthUnit(void) const  
    { return PRBool((eCSSUnit_Inch <= mUnit) && (mUnit <= eCSSUnit_Cicero)); }
  PRBool    IsRelativeLengthUnit(void) const  
    { return PRBool((eCSSUnit_EM <= mUnit) && (mUnit <= eCSSUnit_Pixel)); }
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
  nscoord   GetLengthTwips(void) const;

  void  Reset(void);  // sets to null
  void  SetIntValue(PRInt32 aValue, nsCSSUnit aUnit);
  void  SetPercentValue(float aValue);
  void  SetFloatValue(float aValue, nsCSSUnit aUnit);
  void  SetStringValue(const nsAReadableString& aValue, nsCSSUnit aUnit);
  void  SetColorValue(nscolor aValue);
  void  SetAutoValue(void);
  void  SetInheritValue(void);
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

