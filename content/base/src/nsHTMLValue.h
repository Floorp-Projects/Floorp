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
#ifndef nsHTMLValue_h___
#define nsHTMLValue_h___

#include "nscore.h"
#include "nsColor.h"
#include "nsString.h"
#include "nsISupports.h"

enum nsHTMLUnit {
  eHTMLUnit_Null          = 0,      // (n/a) null unit, value is not specified
  eHTMLUnit_Empty         = 1,      // (n/a) empty unit, value is not specified
  eHTMLUnit_String        = 10,     // (nsString) a string value
  eHTMLUnit_ISupports     = 20,     // (nsISupports*) a ref counted interface
  eHTMLUnit_Integer       = 50,     // (int) simple value
  eHTMLUnit_Enumerated    = 51,     // (int) value has enumerated meaning
  eHTMLUnit_Proportional  = 52,     // (int) value is a relative proportion of some whole
  eHTMLUnit_Color         = 80,     // (color) an RGBA value
  eHTMLUnit_ColorName     = 81,     // (nsString/color) a color name value
  eHTMLUnit_Percent       = 90,     // (float) 1.0 == 100%) value is percentage of something

  // Screen relative measure
  eHTMLUnit_Pixel      = 600    // (int) screen pixels
};

class nsHTMLValue {
public:
  nsHTMLValue(nsHTMLUnit aUnit = eHTMLUnit_Null);
  nsHTMLValue(PRInt32 aValue, nsHTMLUnit aUnit);
  nsHTMLValue(float aValue);
  nsHTMLValue(const nsAReadableString& aValue, nsHTMLUnit aUnit = eHTMLUnit_String);
  nsHTMLValue(nsISupports* aValue);
  nsHTMLValue(nscolor aValue);
  nsHTMLValue(const nsHTMLValue& aCopy);
  ~nsHTMLValue(void);

  nsHTMLValue&  operator=(const nsHTMLValue& aCopy);
  PRBool        operator==(const nsHTMLValue& aOther) const;
  PRBool        operator!=(const nsHTMLValue& aOther) const;
  PRUint32      HashValue(void) const;

  nsHTMLUnit  GetUnit(void) const { return mUnit; }
  PRInt32     GetIntValue(void) const;
  PRInt32     GetPixelValue(void) const;
  float       GetPercentValue(void) const;
  nsAWritableString&   GetStringValue(nsAWritableString& aBuffer) const;
  nsISupports*  GetISupportsValue(void) const;
  nscolor     GetColorValue(void) const;

  void  Reset(void);
  void  SetIntValue(PRInt32 aValue, nsHTMLUnit aUnit);
  void  SetPixelValue(PRInt32 aValue);
  void  SetPercentValue(float aValue);
  void  SetStringValue(const nsAReadableString& aValue, nsHTMLUnit aUnit = eHTMLUnit_String);
  void  SetISupportsValue(nsISupports* aValue);
  void  SetColorValue(nscolor aValue);
  void  SetEmptyValue(void);

#ifdef DEBUG
  void  AppendToString(nsAWritableString& aBuffer) const;
#endif

protected:
  nsHTMLUnit  mUnit;
  union {
    PRInt32       mInt;
    float         mFloat;
    PRUnichar*    mString;
    nsISupports*  mISupports;
    nscolor       mColor;
  }           mValue;
};

inline PRInt32 nsHTMLValue::GetIntValue(void) const
{
  NS_ASSERTION((mUnit == eHTMLUnit_String) ||
               (mUnit == eHTMLUnit_Integer) || 
               (mUnit == eHTMLUnit_Enumerated) ||
               (mUnit == eHTMLUnit_Proportional), "not an int value");
  if ((mUnit == eHTMLUnit_Integer) || 
      (mUnit == eHTMLUnit_Enumerated) ||
      (mUnit == eHTMLUnit_Proportional)) {
    return mValue.mInt;
  }
  else if (mUnit == eHTMLUnit_String) {
    if (mValue.mString) {
      PRInt32 err=0;
      nsAutoString str(mValue.mString); // XXX copy. new string APIs will make this better, right?
      return str.ToInteger(&err);
    }
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

inline nsAWritableString& nsHTMLValue::GetStringValue(nsAWritableString& aBuffer) const
{
  NS_ASSERTION((mUnit == eHTMLUnit_String) || (mUnit == eHTMLUnit_ColorName) ||
               (mUnit == eHTMLUnit_Null), "not a string value");
  aBuffer.SetLength(0);
  if (((mUnit == eHTMLUnit_String) || (mUnit == eHTMLUnit_ColorName)) && 
      (nsnull != mValue.mString)) {
    aBuffer.Append(mValue.mString);
  }
  return aBuffer;
}

inline nsISupports* nsHTMLValue::GetISupportsValue(void) const
{
  NS_ASSERTION(mUnit == eHTMLUnit_ISupports, "not an ISupports value");
  if (mUnit == eHTMLUnit_ISupports) {
    nsISupports *result = mValue.mISupports;
    NS_IF_ADDREF(result);
    return result;
  }
  return nsnull;
}

inline nscolor nsHTMLValue::GetColorValue(void) const 
{
  NS_ASSERTION((mUnit == eHTMLUnit_Color) || (mUnit == eHTMLUnit_ColorName), 
               "not a color value");
  if (mUnit == eHTMLUnit_Color) {
    return mValue.mColor;
  }
  if ((mUnit == eHTMLUnit_ColorName) && (mValue.mString)) {
    nscolor color;
    if (NS_ColorNameToRGB(nsAutoString(mValue.mString), &color)) {
      return color;
    }
  }
  return NS_RGB(0,0,0);
}

inline PRBool nsHTMLValue::operator!=(const nsHTMLValue& aOther) const
{
  return PRBool(! ((*this) == aOther));
}

#endif /* nsHTMLValue_h___ */

