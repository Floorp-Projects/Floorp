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
#ifndef nsICSSDeclaration_h___
#define nsICSSDeclaration_h___

#include "nslayout.h"
#include "nsISupports.h"
#include "nsColor.h"
#include "stdio.h"
#include "nsString.h"
#include "nsCoord.h"


enum nsCSSUnit {
  eCSSUnit_Null       = 0,      // (n/a) null unit, value is not specified
  eCSSUnit_Auto       = 1,      // (n/a) value is algorithmic
  eCSSUnit_Inherit    = 2,      // (n/a) value is inherited
  eCSSUnit_None       = 3,      // (n/a) value is none
  eCSSUnit_Normal     = 4,      // (n/a) value is normal (algorithmic, different than auto)
  eCSSUnit_String     = 10,     // (nsString) a string value
  eCSSUnit_Integer    = 50,     // (int) simple value
  eCSSUnit_Enumerated = 51,     // (int) value has enumerated meaning
  eCSSUnit_Color      = 80,     // (color) an RGBA value
  eCSSUnit_Percent    = 90,     // (float) 1.0 == 100%) value is percentage of something
  eCSSUnit_Number     = 91,     // (float) value is numeric (usually multiplier, different behavior that percent)

  // US English
  eCSSUnit_Inch       = 100,    // (float) Standard length
  eCSSUnit_Foot       = 101,    // (float) 12 inches
  eCSSUnit_Mile       = 102,    // (float) 5280 feet

  // Metric
  eCSSUnit_Millimeter = 207,    // (float) 1/1000 meter
  eCSSUnit_Centimeter = 208,    // (float) 1/100 meter
  eCSSUnit_Meter      = 210,    // (float) Standard length
  eCSSUnit_Kilometer  = 213,    // (float) 1000 meters

  // US Typographic
  eCSSUnit_Point      = 300,    // (float) 1/72 inch
  eCSSUnit_Pica       = 301,    // (float) 12 points == 1/6 inch

  // European Typographic
  eCSSUnit_Didot      = 400,    // (float) 15 didots == 16 points
  eCSSUnit_Cicero     = 401,    // (float) 12 didots

  // relative units
  // Font relative measure
  eCSSUnit_EM         = 800,    // (float) == current font size
  eCSSUnit_EN         = 801,    // (float) .5 em
  eCSSUnit_XHeight    = 802,    // (float) distance from top of lower case x to baseline
  eCSSUnit_CapHeight  = 803,    // (float) distance from top of uppercase case H to baseline

  // Screen relative measure
  eCSSUnit_Pixel      = 900     // (float)
};

struct nsCSSStruct {
  virtual const nsID& GetID(void) = 0;
};

class nsCSSValue {
public:
  nsCSSValue(nsCSSUnit aUnit = eCSSUnit_Null);  // for valueless units only (null, auto, inherit, none, normal)
  nsCSSValue(PRInt32 aValue, nsCSSUnit aUnit);
  nsCSSValue(float aValue, nsCSSUnit aUnit);
  nsCSSValue(const nsString& aValue);
  nsCSSValue(nscolor aValue);
  nsCSSValue(const nsCSSValue& aCopy);
  ~nsCSSValue(void);

  nsCSSValue&  operator=(const nsCSSValue& aCopy);
  PRBool      operator==(const nsCSSValue& aOther) const;

  nsCSSUnit GetUnit(void) const { return mUnit; };
  PRBool    IsLengthUnit(void) const  
    { return PRBool(eCSSUnit_Inch <= mUnit); }
  PRBool    IsFixedLengthUnit(void) const  
    { return PRBool((eCSSUnit_Inch <= mUnit) && (mUnit < eCSSUnit_EM)); }
  PRBool    IsRelativeLengthUnit(void) const  
    { return PRBool(eCSSUnit_EM <= mUnit); }

  PRInt32   GetIntValue(void) const;
  float     GetPercentValue(void) const;
  float     GetFloatValue(void) const;
  nsString& GetStringValue(nsString& aBuffer) const;
  nscolor   GetColorValue(void) const;
  nscoord   GetLengthTwips(void) const;

  void  Reset(void);  // sets to null
  void  SetIntValue(PRInt32 aValue, nsCSSUnit aUnit);
  void  SetPercentValue(float aValue);
  void  SetFloatValue(float aValue, nsCSSUnit aUnit);
  void  SetStringValue(const nsString& aValue);
  void  SetColorValue(nscolor aValue);
  void  SetAutoValue(void);
  void  SetInheritValue(void);
  void  SetNoneValue(void);
  void  SetNormalValue(void);

  void  AppendToString(nsString& aBuffer, PRInt32 aPropID = -1) const;
  void  ToString(nsString& aBuffer, PRInt32 aPropID = -1) const;

protected:
  nsCSSUnit mUnit;
  union {
    PRInt32   mInt;
    float     mFloat;
    nsString* mString;
    nscolor   mColor;
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
  NS_ASSERTION((mUnit >= eCSSUnit_Number), "not a float value");
  if ((mUnit >= eCSSUnit_Number)) {
    return mValue.mFloat;
  }
  return 0.0f;
}

inline nsString& nsCSSValue::GetStringValue(nsString& aBuffer) const
{
  NS_ASSERTION((mUnit == eCSSUnit_String), "not a string value");
  aBuffer.Truncate();
  if ((mUnit == eCSSUnit_String) && (nsnull != mValue.mString)) {
    aBuffer.Append(*(mValue.mString));
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

// SID for the nsCSSFont struct {f645dbf8-b48a-11d1-9ca5-0060088f9ff7}
#define NS_CSS_FONT_SID   \
{0xf645dbf8, 0xb48a, 0x11d1, {0x9c, 0xa5, 0x00, 0x60, 0x08, 0x8f, 0x9f, 0xf7}}

// SID for the nsCSSColor struct {fd825f22-b48a-11d1-9ca5-0060088f9ff7}
#define NS_CSS_COLOR_SID   \
{0xfd825f22, 0xb48a, 0x11d1, {0x9c, 0xa5, 0x00, 0x60, 0x08, 0x8f, 0x9f, 0xf7}}

// SID for the nsCSSText struct {fe13ce94-b48a-11d1-9ca5-0060088f9ff7}
#define NS_CSS_TEXT_SID   \
{0xfe13ce94, 0xb48a, 0x11d1, {0x9c, 0xa5, 0x00, 0x60, 0x08, 0x8f, 0x9f, 0xf7}}

// SID for the nsCSSDisplay struct {fe13ce95-b48a-11d1-9ca5-0060088f9ff7}
#define NS_CSS_DISPLAY_SID   \
{0xfe13ce95, 0xb48a, 0x11d1, {0x9c, 0xa5, 0x00, 0x60, 0x08, 0x8f, 0x9f, 0xf7}}

// SID for the nsCSSMargin struct {fe6019d4-b48a-11d1-9ca5-0060088f9ff7}
#define NS_CSS_MARGIN_SID   \
{0xfe6019d4, 0xb48a, 0x11d1, {0x9c, 0xa5, 0x00, 0x60, 0x08, 0x8f, 0x9f, 0xf7}}

// SID for the nsCSSPosition struct {fee33b2a-b48a-11d1-9ca5-0060088f9ff7}
#define NS_CSS_POSITION_SID   \
{0xfee33b2a, 0xb48a, 0x11d1, {0x9c, 0xa5, 0x00, 0x60, 0x08, 0x8f, 0x9f, 0xf7}}

// SID for the nsCSSList struct {603f8266-b48b-11d1-9ca5-0060088f9ff7}
#define NS_CSS_LIST_SID   \
{0x603f8266, 0xb48b, 0x11d1, {0x9c, 0xa5, 0x00, 0x60, 0x08, 0x8f, 0x9f, 0xf7}}

// IID for the nsICSSDeclaration interface {7b36b9ac-b48d-11d1-9ca5-0060088f9ff7}
#define NS_ICSS_DECLARATION_IID   \
{0x7b36b9ac, 0xb48d, 0x11d1, {0x9c, 0xa5, 0x00, 0x60, 0x08, 0x8f, 0x9f, 0xf7}}

struct nsCSSFont : public nsCSSStruct {
  const nsID& GetID(void);
  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nsCSSValue mFamily;
  nsCSSValue mStyle;
  nsCSSValue mVariant;
  nsCSSValue mWeight;
  nsCSSValue mSize;
};

struct nsCSSColor : public nsCSSStruct  {
  const nsID& GetID(void);
  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nsCSSValue mColor;
  nsCSSValue mBackColor;
  nsCSSValue mBackImage;
  nsCSSValue mBackRepeat;
  nsCSSValue mBackAttachment;
  nsCSSValue mBackPositionX;
  nsCSSValue mBackPositionY;
  nsCSSValue mBackFilter;
  nsCSSValue mCursor;
  nsCSSValue mCursorImage;
  nsCSSValue mOpacity;
};

struct nsCSSText : public nsCSSStruct  {
  const nsID& GetID(void);
  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nsCSSValue mWordSpacing;
  nsCSSValue mLetterSpacing;
  nsCSSValue mDecoration;
  nsCSSValue mVerticalAlign;
  nsCSSValue mTextTransform;
  nsCSSValue mTextAlign;
  nsCSSValue mTextIndent;
  nsCSSValue mLineHeight;
  nsCSSValue mWhiteSpace;
};

struct nsCSSRect {
  void List(FILE* out = 0, PRInt32 aPropID = -1, PRInt32 aIndent = 0) const;

  nsCSSValue mTop;
  nsCSSValue mRight;
  nsCSSValue mBottom;
  nsCSSValue mLeft;
};

struct nsCSSDisplay : public nsCSSStruct  {
  nsCSSDisplay(void);
  ~nsCSSDisplay(void);
  const nsID& GetID(void);
  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nsCSSValue mDirection;
  nsCSSValue mDisplay;
  nsCSSValue mFloat;
  nsCSSValue mClear;
  nsCSSRect* mClip;
  nsCSSValue mOverflow;
  nsCSSValue mVisibility;
  nsCSSValue mFilter;
};

struct nsCSSMargin : public nsCSSStruct  {
  nsCSSMargin(void);
  ~nsCSSMargin(void);
  const nsID& GetID(void);
  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nsCSSRect*  mMargin;
  nsCSSRect*  mPadding;
  nsCSSRect*  mBorder;
  nsCSSRect*  mColor;
  nsCSSRect*  mStyle;
};

struct nsCSSPosition : public nsCSSStruct  {
  const nsID& GetID(void);
  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nsCSSValue  mPosition;
  nsCSSValue  mWidth;
  nsCSSValue  mHeight;
  nsCSSValue  mLeft;
  nsCSSValue  mTop;
  nsCSSValue  mZIndex;
};

struct nsCSSList : public nsCSSStruct  {
  const nsID& GetID(void);
  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  nsCSSValue mType;
  nsCSSValue mImage;
  nsCSSValue mPosition;
};


class nsICSSDeclaration : public nsISupports {
public:
  virtual nsresult GetData(const nsID& aIID, nsCSSStruct** aData) = 0;
  virtual nsresult EnsureData(const nsID& aSID, nsCSSStruct** aData) = 0;

  virtual nsresult AddValue(const char* aProperty, const nsCSSValue& aValue) = 0;
  virtual nsresult AddValue(PRInt32 aProperty, const nsCSSValue& aValue) = 0;
// XXX make nscolor a struct to avoid type conflicts
  virtual nsresult GetValue(const char* aProperty, nsCSSValue& aValue) = 0;
  virtual nsresult GetValue(PRInt32 aProperty, nsCSSValue& aValue) = 0;

  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const = 0;
};

extern NS_HTML nsresult
  NS_NewCSSDeclaration(nsICSSDeclaration** aInstancePtrResult);

#endif /* nsICSSDeclaration_h___ */

