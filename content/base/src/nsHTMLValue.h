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
#ifndef nsHTMLValue_h___
#define nsHTMLValue_h___

#include "nscore.h"
#include "nsColor.h"
#include "nsString.h"
#include "nsISupports.h"


enum nsHTMLUnit {
  eHTMLUnit_Null       = 0,      // (n/a) null unit, value is not specified
  eHTMLUnit_Empty      = 1,      // (n/a) empty unit, value is not specified
  eHTMLUnit_String     = 10,     // (nsString) a string value
  eHTMLUnit_ISupports  = 20,     // (nsISupports*) a ref counted interface
  eHTMLUnit_Integer    = 50,     // (int) simple value
  eHTMLUnit_Enumerated = 51,     // (int) value has enumerated meaning
  eHTMLUnit_Color      = 80,     // (color) an RGBA value
  eHTMLUnit_Percent    = 90,     // (float) 1.0 == 100%) value is percentage of something

  // Screen relative measure
  eHTMLUnit_Pixel      = 600,    // (int) screen pixels
};

class nsHTMLValue {
public:
  nsHTMLValue(nsHTMLUnit aUnit = eHTMLUnit_Null);
  nsHTMLValue(PRInt32 aValue, nsHTMLUnit aUnit);
  nsHTMLValue(float aValue);
  nsHTMLValue(const nsString& aValue);
  nsHTMLValue(nsISupports* aValue);
  nsHTMLValue(nscolor aValue);
  nsHTMLValue(const nsHTMLValue& aCopy);
  ~nsHTMLValue(void);

  nsHTMLValue&  operator=(const nsHTMLValue& aCopy);
  PRBool        operator==(const nsHTMLValue& aOther) const;

  nsHTMLUnit  GetUnit(void) const { return mUnit; }
  PRInt32     GetIntValue(void) const;
  PRInt32     GetPixelValue(void) const;
  float       GetPercentValue(void) const;
  nsString&   GetStringValue(nsString& aBuffer) const;
  nsISupports*  GetISupportsValue(void) const;
  nscolor     GetColorValue(void) const;

  void  Reset(void);
  void  SetIntValue(PRInt32 aValue, nsHTMLUnit aUnit);
  void  SetPixelValue(PRInt32 aValue);
  void  SetPercentValue(float aValue);
  void  SetStringValue(const nsString& aValue);
  void  SetSupportsValue(nsISupports* aValue);
  void  SetColorValue(nscolor aValue);
  void  SetEmptyValue(void);

  void  AppendToString(nsString& aBuffer) const;
  void  ToString(nsString& aBuffer) const;

protected:
  nsHTMLUnit  mUnit;
  union {
    PRInt32       mInt;
    float         mFloat;
    nsString*     mString;
    nsISupports*  mISupports;
    nscolor       mColor;
  }           mValue;

public:
  static const nsHTMLValue kNull;
};

inline PRInt32 nsHTMLValue::GetIntValue(void) const
{
  NS_ASSERTION((mUnit == eHTMLUnit_Integer) || 
               (mUnit == eHTMLUnit_Enumerated), "not an int value");
  if ((mUnit == eHTMLUnit_Integer) || 
      (mUnit == eHTMLUnit_Enumerated)) {
    return mValue.mInt;
  }
  return 0;
}

inline PRInt32 nsHTMLValue::GetPixelValue(void) const
{
  NS_ASSERTION((mUnit == eHTMLUnit_Pixel), "not a pixel value");
  if (mUnit == eHTMLUnit_Pixel) {
    return mValue.mInt;
  }
  return 0;
}

inline float nsHTMLValue::GetPercentValue(void) const
{
  NS_ASSERTION(mUnit == eHTMLUnit_Percent, "not a percent value");
  if (mUnit == eHTMLUnit_Percent) {
    return mValue.mFloat;
  }
  return 0.0f;
}

inline nsString& nsHTMLValue::GetStringValue(nsString& aBuffer) const
{
  NS_ASSERTION((mUnit == eHTMLUnit_String) || (mUnit == eHTMLUnit_Null), "not a string value");
  aBuffer.SetLength(0);
  if ((mUnit == eHTMLUnit_String) && (nsnull != mValue.mString)) {
    aBuffer.Append(*(mValue.mString));
  }
  return aBuffer;
}

inline nsISupports* nsHTMLValue::GetISupportsValue(void) const
{
  NS_ASSERTION(mUnit == eHTMLUnit_ISupports, "not an ISupports value");
  if (mUnit == eHTMLUnit_ISupports) {
    NS_IF_ADDREF(mValue.mISupports);
    return mValue.mISupports;
  }
  return nsnull;
}

inline nscolor nsHTMLValue::GetColorValue(void) const 
{
  NS_ASSERTION(mUnit == eHTMLUnit_Color, "not a color value");
  if (mUnit == eHTMLUnit_Color) {
    return mValue.mColor;
  }
  return NS_RGB(0,0,0);
}


#endif /* nsHTMLValue_h___ */

