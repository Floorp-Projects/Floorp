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
#include "nsICSSDeclaration.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsCSSProps.h"
#include "nsCSSPropIDs.h"
#include "nsUnitConversion.h"

//#define DEBUG_REFS

static NS_DEFINE_IID(kCSSFontSID, NS_CSS_FONT_SID);
static NS_DEFINE_IID(kCSSColorSID, NS_CSS_COLOR_SID);
static NS_DEFINE_IID(kCSSDisplaySID, NS_CSS_DISPLAY_SID);
static NS_DEFINE_IID(kCSSTextSID, NS_CSS_TEXT_SID);
static NS_DEFINE_IID(kCSSMarginSID, NS_CSS_MARGIN_SID);
static NS_DEFINE_IID(kCSSPositionSID, NS_CSS_POSITION_SID);
static NS_DEFINE_IID(kCSSListSID, NS_CSS_LIST_SID);
static NS_DEFINE_IID(kICSSDeclarationIID, NS_ICSS_DECLARATION_IID);


nsCSSValue::nsCSSValue(nsCSSUnit aUnit)
  : mUnit(aUnit)
{
  NS_ASSERTION(mUnit <= eCSSUnit_Normal, "not a valueless unit");
  if (aUnit > eCSSUnit_Normal) {
    mUnit = eCSSUnit_Null;
  }
  mValue.mInt = 0;
}

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

nsCSSValue::nsCSSValue(const nsString& aValue)
  : mUnit(eCSSUnit_String)
{
  mValue.mString = aValue.ToNewString();
}

nsCSSValue::nsCSSValue(nscolor aValue)
  : mUnit(eCSSUnit_Color)
{
  mValue.mColor = aValue;
}

nsCSSValue::nsCSSValue(const nsCSSValue& aCopy)
  : mUnit(aCopy.mUnit)
{
  if (eCSSUnit_String == mUnit) {
    if (nsnull != aCopy.mValue.mString) {
      mValue.mString = (aCopy.mValue.mString)->ToNewString();
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
  else {
    mValue.mFloat = aCopy.mValue.mFloat;
  }
}

nsCSSValue::~nsCSSValue(void)
{
  Reset();
}

nsCSSValue& nsCSSValue::operator=(const nsCSSValue& aCopy)
{
  Reset();
  mUnit = aCopy.mUnit;
  if (eCSSUnit_String == mUnit) {
    if (nsnull != aCopy.mValue.mString) {
      mValue.mString = (aCopy.mValue.mString)->ToNewString();
    }
  }
  else if ((eCSSUnit_Integer <= mUnit) && (mUnit <= eCSSUnit_Enumerated)) {
    mValue.mInt = aCopy.mValue.mInt;
  }
  else if (eCSSUnit_Color == mUnit){
    mValue.mColor = aCopy.mValue.mColor;
  }
  else {
    mValue.mFloat = aCopy.mValue.mFloat;
  }
  return *this;
}

PRBool nsCSSValue::operator==(const nsCSSValue& aOther) const
{
  if (mUnit == aOther.mUnit) {
    if (eCSSUnit_String == mUnit) {
      if (nsnull == mValue.mString) {
        if (nsnull == aOther.mValue.mString) {
          return PR_TRUE;
        }
      }
      else if (nsnull != aOther.mValue.mString) {
        return mValue.mString->Equals(*(aOther.mValue.mString));
      }
    }
    else if ((eCSSUnit_Integer <= mUnit) && (mUnit <= eCSSUnit_Enumerated)) {
      return PRBool(mValue.mInt == aOther.mValue.mInt);
    }
    else if (eCSSUnit_Color == mUnit){
      return PRBool(mValue.mColor == aOther.mValue.mColor);
    }
    else {
      return PRBool(mValue.mFloat == aOther.mValue.mFloat);
    }
  }
  return PR_FALSE;
}

nscoord nsCSSValue::GetLengthTwips(void) const
{
  NS_ASSERTION(IsFixedLengthUnit(), "not a fixed length unit");

  if (IsFixedLengthUnit()) {
    switch (mUnit) {
    case eCSSUnit_Inch:        
      return (nscoord)NS_INCHES_TO_TWIPS(mValue.mFloat);
    case eCSSUnit_Foot:        
      return (nscoord)NS_FEET_TO_TWIPS(mValue.mFloat);
    case eCSSUnit_Mile:        
      return (nscoord)NS_MILES_TO_TWIPS(mValue.mFloat);

    case eCSSUnit_Millimeter:
      return (nscoord)NS_MILLIMETERS_TO_TWIPS(mValue.mFloat);
    case eCSSUnit_Centimeter:
      return (nscoord)NS_CENTIMETERS_TO_TWIPS(mValue.mFloat);
    case eCSSUnit_Meter:
      return (nscoord)NS_METERS_TO_TWIPS(mValue.mFloat);
    case eCSSUnit_Kilometer:
      return (nscoord)NS_KILOMETERS_TO_TWIPS(mValue.mFloat);

    case eCSSUnit_Point:
      return (nscoord)NS_POINTS_TO_TWIPS_FLOAT(mValue.mFloat);
    case eCSSUnit_Pica:
      return (nscoord)NS_PICAS_TO_TWIPS(mValue.mFloat);
    case eCSSUnit_Didot:
      return (nscoord)NS_DIDOTS_TO_TWIPS(mValue.mFloat);
    case eCSSUnit_Cicero:
      return (nscoord)NS_CICEROS_TO_TWIPS(mValue.mFloat);
    }
  }
  return 0;
}

void nsCSSValue::Reset(void)
{
  if ((eCSSUnit_String == mUnit) && (nsnull != mValue.mString)) {
    delete mValue.mString;
  }
  mUnit = eCSSUnit_Null;
  mValue.mInt = 0;
};

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

void nsCSSValue::SetStringValue(const nsString& aValue)
{
  Reset();
  mUnit = eCSSUnit_String;
  mValue.mString = aValue.ToNewString();
}

void nsCSSValue::SetColorValue(nscolor aValue)
{
  Reset();
  mUnit = eCSSUnit_Color;
  mValue.mColor = aValue;
}

void nsCSSValue::SetAutoValue(void)
{
  Reset();
  mUnit = eCSSUnit_Auto;
}

void nsCSSValue::SetInheritValue(void)
{
  Reset();
  mUnit = eCSSUnit_Inherit;
}

void nsCSSValue::SetNoneValue(void)
{
  Reset();
  mUnit = eCSSUnit_None;
}

void nsCSSValue::SetNormalValue(void)
{
  Reset();
  mUnit = eCSSUnit_Normal;
}

void nsCSSValue::AppendToString(nsString& aBuffer, PRInt32 aPropID) const
{
  if (eCSSUnit_Null == mUnit) {
    return;
  }

  if (-1 < aPropID) {
    aBuffer.Append(nsCSSProps::kNameTable[aPropID].name);
    aBuffer.Append(": ");
  }

  if (eCSSUnit_String == mUnit) {
    if (nsnull != mValue.mString) {
      aBuffer.Append('"');
      aBuffer.Append(*(mValue.mString));
      aBuffer.Append('"');
    }
    else {
      aBuffer.Append("null str");
    }
  }
  else if ((eCSSUnit_Integer <= mUnit) && (mUnit <= eCSSUnit_Enumerated)) {
    aBuffer.Append(mValue.mInt, 10);
    aBuffer.Append("[0x");
    aBuffer.Append(mValue.mInt, 16);
    aBuffer.Append(']');
  }
  else if (eCSSUnit_Color == mUnit){
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
  else if (eCSSUnit_Percent <= mUnit) {
    aBuffer.Append(mValue.mFloat);
  }

  switch (mUnit) {
    case eCSSUnit_Null:       break;
    case eCSSUnit_Auto:       aBuffer.Append("auto"); break;
    case eCSSUnit_Inherit:    aBuffer.Append("inherit"); break;
    case eCSSUnit_None:       aBuffer.Append("none"); break;
    case eCSSUnit_String:     break;
    case eCSSUnit_Integer:    aBuffer.Append("int");  break;
    case eCSSUnit_Enumerated: aBuffer.Append("enum"); break;
    case eCSSUnit_Color:      aBuffer.Append("rbga"); break;
    case eCSSUnit_Percent:    aBuffer.Append("%");    break;
    case eCSSUnit_Number:     aBuffer.Append("#");    break;
    case eCSSUnit_Inch:       aBuffer.Append("in");   break;
    case eCSSUnit_Foot:       aBuffer.Append("ft");   break;
    case eCSSUnit_Mile:       aBuffer.Append("mi");   break;
    case eCSSUnit_Millimeter: aBuffer.Append("mm");   break;
    case eCSSUnit_Centimeter: aBuffer.Append("cm");   break;
    case eCSSUnit_Meter:      aBuffer.Append("m");    break;
    case eCSSUnit_Kilometer:  aBuffer.Append("km");   break;
    case eCSSUnit_Point:      aBuffer.Append("pt");   break;
    case eCSSUnit_Pica:       aBuffer.Append("pc");   break;
    case eCSSUnit_Didot:      aBuffer.Append("dt");   break;
    case eCSSUnit_Cicero:     aBuffer.Append("cc");   break;
    case eCSSUnit_EM:         aBuffer.Append("em");   break;
    case eCSSUnit_EN:         aBuffer.Append("en");   break;
    case eCSSUnit_XHeight:    aBuffer.Append("ex");   break;
    case eCSSUnit_CapHeight:  aBuffer.Append("cap");  break;
    case eCSSUnit_Pixel:      aBuffer.Append("px");   break;
  }
  aBuffer.Append(' ');
}

void nsCSSValue::ToString(nsString& aBuffer, PRInt32 aPropID) const
{
  aBuffer.Truncate();
  AppendToString(aBuffer, aPropID);
}

const nsID& nsCSSFont::GetID(void)
{
  return kCSSFontSID;
}

void nsCSSFont::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mFamily.AppendToString(buffer, PROP_FONT_FAMILY);
  mStyle.AppendToString(buffer, PROP_FONT_STYLE);
  mVariant.AppendToString(buffer, PROP_FONT_VARIANT);
  mWeight.AppendToString(buffer, PROP_FONT_WEIGHT);
  mSize.AppendToString(buffer, PROP_FONT_SIZE);
  fputs(buffer, out);
}


const nsID& nsCSSColor::GetID(void)
{
  return kCSSColorSID;
}

void nsCSSColor::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mColor.AppendToString(buffer, PROP_COLOR);
  mBackColor.AppendToString(buffer, PROP_BACKGROUND_COLOR);
  mBackImage.AppendToString(buffer, PROP_BACKGROUND_IMAGE);
  mBackRepeat.AppendToString(buffer, PROP_BACKGROUND_REPEAT);
  mBackAttachment.AppendToString(buffer, PROP_BACKGROUND_ATTACHMENT);
  mBackPositionX.AppendToString(buffer, PROP_BACKGROUND_X_POSITION);
  mBackPositionY.AppendToString(buffer, PROP_BACKGROUND_Y_POSITION);
  mBackFilter.AppendToString(buffer, PROP_BACKGROUND_FILTER);
  mCursor.AppendToString(buffer, PROP_CURSOR);
  mCursorImage.AppendToString(buffer, PROP_CURSOR_IMAGE);
  fputs(buffer, out);
}

const nsID& nsCSSText::GetID(void)
{
  return kCSSTextSID;
}

void nsCSSText::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mWordSpacing.AppendToString(buffer, PROP_WORD_SPACING);
  mLetterSpacing.AppendToString(buffer, PROP_LETTER_SPACING);
  mDecoration.AppendToString(buffer, PROP_TEXT_DECORATION);
  mVerticalAlign.AppendToString(buffer, PROP_VERTICAL_ALIGN);
  mTextTransform.AppendToString(buffer, PROP_TEXT_TRANSFORM);
  mTextAlign.AppendToString(buffer, PROP_TEXT_ALIGN);
  mTextIndent.AppendToString(buffer, PROP_TEXT_INDENT);
  mLineHeight.AppendToString(buffer, PROP_LINE_HEIGHT);
  mWhiteSpace.AppendToString(buffer, PROP_WHITE_SPACE);
  fputs(buffer, out);
}

const nsID& nsCSSDisplay::GetID(void)
{
  return kCSSDisplaySID;
}

void nsCSSDisplay::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mDirection.AppendToString(buffer, PROP_DIRECTION);
  mDisplay.AppendToString(buffer, PROP_DISPLAY);
  mFloat.AppendToString(buffer, PROP_FLOAT);
  mClear.AppendToString(buffer, PROP_CLEAR);
  fputs(buffer, out);
}

void nsCSSRect::List(FILE* out, PRInt32 aPropID, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  if (-1 < aPropID) {
    buffer.Append(nsCSSProps::kNameTable[aPropID].name);
    buffer.Append(": ");
  }

  mTop.AppendToString(buffer);
  mRight.AppendToString(buffer);
  mBottom.AppendToString(buffer); 
  mLeft.AppendToString(buffer);
  fputs(buffer, out);
}

nsCSSMargin::nsCSSMargin(void)
  : mMargin(nsnull), mPadding(nsnull), mBorder(nsnull), mColor(nsnull), mStyle(nsnull)
{
}

nsCSSMargin::~nsCSSMargin(void)
{
  if (nsnull != mMargin) {
    delete mMargin;
  }
  if (nsnull != mPadding) {
    delete mPadding;
  }
  if (nsnull != mBorder) {
    delete mBorder;
  }
  if (nsnull != mColor) {
    delete mColor;
  }
  if (nsnull != mStyle) {
    delete mStyle;
  }
}

const nsID& nsCSSMargin::GetID(void)
{
  return kCSSMarginSID;
}

void nsCSSMargin::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  if (nsnull != mMargin) {
    mMargin->List(out, PROP_MARGIN, aIndent);
  }
  if (nsnull != mPadding) {
    mPadding->List(out, PROP_PADDING, aIndent);
  }
  if (nsnull != mBorder) {
    mBorder->List(out, PROP_BORDER_WIDTH, aIndent);
  }
  if (nsnull != mColor) {
    mColor->List(out, PROP_BORDER_COLOR, aIndent);
  }
  if (nsnull != mStyle) {
    mStyle->List(out, PROP_BORDER_STYLE, aIndent);
  }
}

nsCSSPosition::nsCSSPosition(void)
  : mClip(nsnull)
{
}

nsCSSPosition::~nsCSSPosition(void)
{
  if (nsnull != mClip) {
    delete mClip;
  }
}

const nsID& nsCSSPosition::GetID(void)
{
  return kCSSPositionSID;
}

void nsCSSPosition::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mPosition.AppendToString(buffer, PROP_POSITION);
  mWidth.AppendToString(buffer, PROP_WIDTH);
  mHeight.AppendToString(buffer, PROP_HEIGHT);
  mLeft.AppendToString(buffer, PROP_LEFT);
  mTop.AppendToString(buffer, PROP_TOP);
  fputs(buffer, out);
  if (nsnull != mClip) {
    mClip->List(out, PROP_CLIP);
  }
  buffer.SetLength(0);
  mOverflow.AppendToString(buffer, PROP_OVERFLOW);
  mZIndex.AppendToString(buffer, PROP_Z_INDEX);
  mVisibility.AppendToString(buffer, PROP_VISIBILITY);
  mFilter.AppendToString(buffer, PROP_FILTER);
  fputs(buffer, out);
}

const nsID& nsCSSList::GetID(void)
{
  return kCSSListSID;
}

void nsCSSList::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buffer;

  mType.AppendToString(buffer, PROP_LIST_STYLE_TYPE);
  mImage.AppendToString(buffer, PROP_LIST_STYLE_IMAGE);
  mPosition.AppendToString(buffer, PROP_LIST_STYLE_POSITION);
  fputs(buffer, out);
}



class CSSDeclarationImpl : public nsICSSDeclaration {
public:
  void* operator new(size_t size);

  CSSDeclarationImpl(void);
  ~CSSDeclarationImpl(void);

  NS_DECL_ISUPPORTS

  nsresult  GetData(const nsID& aSID, nsCSSStruct** aData);
  nsresult  EnsureData(const nsID& aSID, nsCSSStruct** aData);

  nsresult AddValue(const char* aProperty, const nsCSSValue& aValue);
  nsresult AddValue(PRInt32 aProperty, const nsCSSValue& aValue);
  nsresult GetValue(const char* aProperty, nsCSSValue& aValue);
  nsresult GetValue(PRInt32 aProperty, nsCSSValue& aValue);

  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;

private:
  CSSDeclarationImpl(const CSSDeclarationImpl& aCopy);
  CSSDeclarationImpl& operator=(const CSSDeclarationImpl& aCopy);
  PRBool operator==(const CSSDeclarationImpl& aCopy) const;

protected:
  nsCSSFont*      mFont;
  nsCSSColor*     mColor;
  nsCSSText*      mText;
  nsCSSMargin*    mMargin;
  nsCSSPosition*  mPosition;
  nsCSSList*      mList;
  nsCSSDisplay*   mDisplay;
};

#ifdef DEBUG_REFS
static PRInt32 gInstanceCount;
#endif


void* CSSDeclarationImpl::operator new(size_t size)
{
  void* result = new char[size];

  nsCRT::zero(result, size);
  return result;
}

CSSDeclarationImpl::CSSDeclarationImpl(void)
{
  NS_INIT_REFCNT();
#ifdef DEBUG_REFS
  ++gInstanceCount;
  fprintf(stdout, "%d + CSSDeclaration\n", gInstanceCount);
#endif
}

CSSDeclarationImpl::~CSSDeclarationImpl(void)
{
  if (nsnull != mFont) {
    delete mFont;
  }
  if (nsnull != mColor) {
    delete mColor;
  }
  if (nsnull != mText) {
    delete mText;
  }
  if (nsnull != mMargin) {
    delete mMargin;
  }
  if (nsnull != mPosition) {
    delete mPosition;
  }
  if (nsnull != mList) {
    delete mList;
  }
  if (nsnull != mDisplay) {
    delete mDisplay;
  }
#ifdef DEBUG_REFS
  --gInstanceCount;
  fprintf(stdout, "%d - CSSDeclaration\n", gInstanceCount);
#endif
}

NS_IMPL_ISUPPORTS(CSSDeclarationImpl, kICSSDeclarationIID);

nsresult CSSDeclarationImpl::GetData(const nsID& aSID, nsCSSStruct** aDataPtr)
{
  if (nsnull == aDataPtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aSID.Equals(kCSSFontSID)) {
    *aDataPtr = mFont;
  }
  else if (aSID.Equals(kCSSColorSID)) {
    *aDataPtr = mColor;
  }
  else if (aSID.Equals(kCSSDisplaySID)) {
    *aDataPtr = mDisplay;
  }
  else if (aSID.Equals(kCSSTextSID)) {
    *aDataPtr = mText;
  }
  else if (aSID.Equals(kCSSMarginSID)) {
    *aDataPtr = mMargin;
  }
  else if (aSID.Equals(kCSSPositionSID)) {
    *aDataPtr = mPosition;
  }
  else if (aSID.Equals(kCSSListSID)) {
    *aDataPtr = mList;
  }
  else {
    return NS_NOINTERFACE;
  }
  return NS_OK;
}

nsresult CSSDeclarationImpl::EnsureData(const nsID& aSID, nsCSSStruct** aDataPtr)
{
  if (nsnull == aDataPtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aSID.Equals(kCSSFontSID)) {
    if (nsnull == mFont) {
      mFont = new nsCSSFont();
    }
    *aDataPtr = mFont;
  }
  else if (aSID.Equals(kCSSColorSID)) {
    if (nsnull == mColor) {
      mColor = new nsCSSColor();
    }
    *aDataPtr = mColor;
  }
  else if (aSID.Equals(kCSSDisplaySID)) {
    if (nsnull == mDisplay) {
      mDisplay = new nsCSSDisplay();
    }
    *aDataPtr = mColor;
  }
  else if (aSID.Equals(kCSSTextSID)) {
    if (nsnull == mText) {
      mText = new nsCSSText();
    }
    *aDataPtr = mText;
  }
  else if (aSID.Equals(kCSSMarginSID)) {
    if (nsnull == mMargin) {
      mMargin = new nsCSSMargin();
    }
    *aDataPtr = mMargin;
  }
  else if (aSID.Equals(kCSSPositionSID)) {
    if (nsnull == mPosition) {
      mPosition = new nsCSSPosition();
    }
    *aDataPtr = mPosition;
  }
  else if (aSID.Equals(kCSSListSID)) {
    if (nsnull == mList) {
      mList = new nsCSSList();
    }
    *aDataPtr = mList;
  }
  else {
    return NS_NOINTERFACE;
  }
  if (nsnull == *aDataPtr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsresult CSSDeclarationImpl::AddValue(const char* aProperty, const nsCSSValue& aValue)
{
  return AddValue(nsCSSProps::LookupName(aProperty), aValue);
}

nsresult CSSDeclarationImpl::AddValue(PRInt32 aProperty, const nsCSSValue& aValue)
{
  nsresult result = NS_OK;

  switch (aProperty) {
    // nsCSSFont
    case PROP_FONT_FAMILY:
    case PROP_FONT_STYLE:
    case PROP_FONT_VARIANT:
    case PROP_FONT_WEIGHT:
    case PROP_FONT_SIZE:
      if (nsnull == mFont) {
        mFont = new nsCSSFont();
      }
      if (nsnull != mFont) {
        switch (aProperty) {
          case PROP_FONT_FAMILY:  mFont->mFamily = aValue;   break;
          case PROP_FONT_STYLE:   mFont->mStyle = aValue;    break;
          case PROP_FONT_VARIANT: mFont->mVariant = aValue;  break;
          case PROP_FONT_WEIGHT:  mFont->mWeight = aValue;   break;
          case PROP_FONT_SIZE:    mFont->mSize = aValue;     break;
        }
      }
      else {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
      break;

    // nsCSSColor
    case PROP_COLOR:
    case PROP_BACKGROUND_COLOR:
    case PROP_BACKGROUND_IMAGE:
    case PROP_BACKGROUND_REPEAT:
    case PROP_BACKGROUND_ATTACHMENT:
    case PROP_BACKGROUND_X_POSITION:
    case PROP_BACKGROUND_Y_POSITION:
    case PROP_BACKGROUND_FILTER:
    case PROP_CURSOR:
    case PROP_CURSOR_IMAGE:
      if (nsnull == mColor) {
        mColor = new nsCSSColor();
      }
      if (nsnull != mColor) {
        switch (aProperty) {
          case PROP_COLOR:                  mColor->mColor = aValue;           break;
          case PROP_BACKGROUND_COLOR:       mColor->mBackColor = aValue;       break;
          case PROP_BACKGROUND_IMAGE:       mColor->mBackImage = aValue;       break;
          case PROP_BACKGROUND_REPEAT:      mColor->mBackRepeat = aValue;      break;
          case PROP_BACKGROUND_ATTACHMENT:  mColor->mBackAttachment = aValue;  break;
          case PROP_BACKGROUND_X_POSITION:  mColor->mBackPositionX = aValue;   break;
          case PROP_BACKGROUND_Y_POSITION:  mColor->mBackPositionY = aValue;   break;
          case PROP_BACKGROUND_FILTER:      mColor->mBackFilter = aValue;      break;
          case PROP_CURSOR:                 mColor->mCursor = aValue;          break;
          case PROP_CURSOR_IMAGE:           mColor->mCursorImage = aValue;     break;
        }
      }
      else {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
      break;

    // nsCSSText
    case PROP_WORD_SPACING:
    case PROP_LETTER_SPACING:
    case PROP_TEXT_DECORATION:
    case PROP_VERTICAL_ALIGN:
    case PROP_TEXT_TRANSFORM:
    case PROP_TEXT_ALIGN:
    case PROP_TEXT_INDENT:
    case PROP_LINE_HEIGHT:
    case PROP_WHITE_SPACE:
      if (nsnull == mText) {
        mText = new nsCSSText();
      }
      if (nsnull != mText) {
        switch (aProperty) {
          case PROP_WORD_SPACING:     mText->mWordSpacing = aValue;    break;
          case PROP_LETTER_SPACING:   mText->mLetterSpacing = aValue;  break;
          case PROP_TEXT_DECORATION:  mText->mDecoration = aValue;     break;
          case PROP_VERTICAL_ALIGN:   mText->mVerticalAlign = aValue;  break;
          case PROP_TEXT_TRANSFORM:   mText->mTextTransform = aValue;  break;
          case PROP_TEXT_ALIGN:       mText->mTextAlign = aValue;      break;
          case PROP_TEXT_INDENT:      mText->mTextIndent = aValue;     break;
          case PROP_LINE_HEIGHT:      mText->mLineHeight = aValue;     break;
          case PROP_WHITE_SPACE:      mText->mWhiteSpace = aValue;     break;
        }
      }
      else {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
      break;

    // nsCSSMargin
    case PROP_MARGIN_TOP:
    case PROP_MARGIN_RIGHT:
    case PROP_MARGIN_BOTTOM:
    case PROP_MARGIN_LEFT:
      if (nsnull == mMargin) {
        mMargin = new nsCSSMargin();
      }
      if (nsnull != mMargin) {
        if (nsnull == mMargin->mMargin) {
          mMargin->mMargin = new nsCSSRect();
        }
        if (nsnull != mMargin->mMargin) {
          switch (aProperty) {
            case PROP_MARGIN_TOP:     mMargin->mMargin->mTop = aValue;     break;
            case PROP_MARGIN_RIGHT:   mMargin->mMargin->mRight = aValue;   break;
            case PROP_MARGIN_BOTTOM:  mMargin->mMargin->mBottom = aValue;  break;
            case PROP_MARGIN_LEFT:    mMargin->mMargin->mLeft = aValue;    break;
          }
        }
        else {
          result = NS_ERROR_OUT_OF_MEMORY;
        }
      }
      else {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
      break;

    case PROP_PADDING_TOP:
    case PROP_PADDING_RIGHT:
    case PROP_PADDING_BOTTOM:
    case PROP_PADDING_LEFT:
      if (nsnull == mMargin) {
        mMargin = new nsCSSMargin();
      }
      if (nsnull != mMargin) {
        if (nsnull == mMargin->mPadding) {
          mMargin->mPadding = new nsCSSRect();
        }
        if (nsnull != mMargin->mPadding) {
          switch (aProperty) {
            case PROP_PADDING_TOP:    mMargin->mPadding->mTop = aValue;    break;
            case PROP_PADDING_RIGHT:  mMargin->mPadding->mRight = aValue;  break;
            case PROP_PADDING_BOTTOM: mMargin->mPadding->mBottom = aValue; break;
            case PROP_PADDING_LEFT:   mMargin->mPadding->mLeft = aValue;   break;
          }
        }
        else {
          result = NS_ERROR_OUT_OF_MEMORY;
        }
      }
      else {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
      break;

    case PROP_BORDER_TOP_WIDTH:
    case PROP_BORDER_RIGHT_WIDTH:
    case PROP_BORDER_BOTTOM_WIDTH:
    case PROP_BORDER_LEFT_WIDTH:
      if (nsnull == mMargin) {
        mMargin = new nsCSSMargin();
      }
      if (nsnull != mMargin) {
        if (nsnull == mMargin->mBorder) {
          mMargin->mBorder = new nsCSSRect();
        }
        if (nsnull != mMargin->mBorder) {
          switch (aProperty) {
            case PROP_BORDER_TOP_WIDTH:     mMargin->mBorder->mTop = aValue;     break;
            case PROP_BORDER_RIGHT_WIDTH:   mMargin->mBorder->mRight = aValue;   break;
            case PROP_BORDER_BOTTOM_WIDTH:  mMargin->mBorder->mBottom = aValue;  break;
            case PROP_BORDER_LEFT_WIDTH:    mMargin->mBorder->mLeft = aValue;    break;
          }
        }
        else {
          result = NS_ERROR_OUT_OF_MEMORY;
        }
      }
      else {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
      break;

    case PROP_BORDER_TOP_COLOR:
    case PROP_BORDER_RIGHT_COLOR:
    case PROP_BORDER_BOTTOM_COLOR:
    case PROP_BORDER_LEFT_COLOR:
      if (nsnull == mMargin) {
        mMargin = new nsCSSMargin();
      }
      if (nsnull != mMargin) {
        if (nsnull == mMargin->mColor) {
          mMargin->mColor = new nsCSSRect();
        }
        if (nsnull != mMargin->mColor) {
          switch (aProperty) {
            case PROP_BORDER_TOP_COLOR:     mMargin->mColor->mTop = aValue;    break;
            case PROP_BORDER_RIGHT_COLOR:   mMargin->mColor->mRight = aValue;  break;
            case PROP_BORDER_BOTTOM_COLOR:  mMargin->mColor->mBottom = aValue; break;
            case PROP_BORDER_LEFT_COLOR:    mMargin->mColor->mLeft = aValue;   break;
          }
        }
        else {
          result = NS_ERROR_OUT_OF_MEMORY;
        }
      }
      else {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
      break;

    case PROP_BORDER_TOP_STYLE:
    case PROP_BORDER_RIGHT_STYLE:
    case PROP_BORDER_BOTTOM_STYLE:
    case PROP_BORDER_LEFT_STYLE:
      if (nsnull == mMargin) {
        mMargin = new nsCSSMargin();
      }
      if (nsnull != mMargin) {
        if (nsnull == mMargin->mStyle) {
          mMargin->mStyle = new nsCSSRect();
        }
        if (nsnull != mMargin->mStyle) {
          switch (aProperty) {
            case PROP_BORDER_TOP_STYLE:     mMargin->mStyle->mTop = aValue;    break;
            case PROP_BORDER_RIGHT_STYLE:   mMargin->mStyle->mRight = aValue;  break;
            case PROP_BORDER_BOTTOM_STYLE:  mMargin->mStyle->mBottom = aValue; break;
            case PROP_BORDER_LEFT_STYLE:    mMargin->mStyle->mLeft = aValue;   break;
          }
        }
        else {
          result = NS_ERROR_OUT_OF_MEMORY;
        }
      }
      else {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
      break;

    // nsCSSPosition
    case PROP_POSITION:
    case PROP_WIDTH:
    case PROP_HEIGHT:
    case PROP_LEFT:
    case PROP_TOP:
    case PROP_OVERFLOW:
    case PROP_Z_INDEX:
    case PROP_VISIBILITY:
    case PROP_FILTER:
      if (nsnull == mPosition) {
        mPosition = new nsCSSPosition();
      }
      if (nsnull != mPosition) {
        switch (aProperty) {
          case PROP_POSITION:   mPosition->mPosition = aValue;   break;
          case PROP_WIDTH:      mPosition->mWidth = aValue;      break;
          case PROP_HEIGHT:     mPosition->mHeight = aValue;     break;
          case PROP_LEFT:       mPosition->mLeft = aValue;       break;
          case PROP_TOP:        mPosition->mTop = aValue;        break;
          case PROP_OVERFLOW:   mPosition->mOverflow = aValue;   break;
          case PROP_Z_INDEX:    mPosition->mZIndex = aValue;     break;
          case PROP_VISIBILITY: mPosition->mVisibility = aValue; break;
          case PROP_FILTER:     mPosition->mFilter = aValue;     break;
        }
      }
      else {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
      break;

    case PROP_CLIP_TOP:
    case PROP_CLIP_RIGHT:
    case PROP_CLIP_BOTTOM:
    case PROP_CLIP_LEFT:
      if (nsnull == mPosition) {
        mPosition = new nsCSSPosition();
      }
      if (nsnull != mPosition) {
        if (nsnull == mPosition->mClip) {
          mPosition->mClip = new nsCSSRect();
        }
        if (nsnull != mPosition->mClip) {
          switch(aProperty) {
            case PROP_CLIP_TOP:     mPosition->mClip->mTop = aValue;     break;
            case PROP_CLIP_RIGHT:   mPosition->mClip->mRight = aValue;   break;
            case PROP_CLIP_BOTTOM:  mPosition->mClip->mBottom = aValue;  break;
            case PROP_CLIP_LEFT:    mPosition->mClip->mLeft = aValue;    break;
          }
        }
        else {
          result = NS_ERROR_OUT_OF_MEMORY;
        }
      }
      else {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
      break;

      // nsCSSList
    case PROP_LIST_STYLE_TYPE:
    case PROP_LIST_STYLE_IMAGE:
    case PROP_LIST_STYLE_POSITION:
      if (nsnull == mList) {
        mList = new nsCSSList();
      }
      if (nsnull != mList) {
        switch (aProperty) {
          case PROP_LIST_STYLE_TYPE:      mList->mType = aValue;     break;
          case PROP_LIST_STYLE_IMAGE:     mList->mImage = aValue;    break;
          case PROP_LIST_STYLE_POSITION:  mList->mPosition = aValue; break;
        }
      }
      else {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
      break;

      // nsCSSDisplay
    case PROP_FLOAT:
    case PROP_CLEAR:
    case PROP_DISPLAY:
    case PROP_DIRECTION:
      if (nsnull == mDisplay) {
        mDisplay = new nsCSSDisplay();
      }
      if (nsnull != mDisplay) {
        switch (aProperty) {
          case PROP_FLOAT:      mDisplay->mFloat = aValue;      break;
          case PROP_CLEAR:      mDisplay->mClear = aValue;      break;
          case PROP_DISPLAY:    mDisplay->mDisplay = aValue;    break;
          case PROP_DIRECTION:  mDisplay->mDirection = aValue;    break;
        }
      }
      else {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
      break;

    case PROP_BACKGROUND:
    case PROP_BORDER:
    case PROP_CLIP:
    case PROP_FONT:
    case PROP_LIST_STYLE:
    case PROP_MARGIN:
    case PROP_PADDING:
    case PROP_BACKGROUND_POSITION:
    case PROP_BORDER_TOP:
    case PROP_BORDER_RIGHT:
    case PROP_BORDER_BOTTOM:
    case PROP_BORDER_LEFT:
    case PROP_BORDER_COLOR:
    case PROP_BORDER_STYLE:
    case PROP_BORDER_WIDTH:
      NS_ERROR("can't query for shorthand properties");
    default:
      result = NS_ERROR_ILLEGAL_VALUE;
      break;
  }
  return result;
}

nsresult CSSDeclarationImpl::GetValue(const char* aProperty, nsCSSValue& aValue)
{
  return GetValue(nsCSSProps::LookupName(aProperty), aValue);
}

nsresult CSSDeclarationImpl::GetValue(PRInt32 aProperty, nsCSSValue& aValue)
{
  nsresult result = NS_OK;

  switch (aProperty) {
    // nsCSSFont
    case PROP_FONT_FAMILY:
    case PROP_FONT_STYLE:
    case PROP_FONT_VARIANT:
    case PROP_FONT_WEIGHT:
    case PROP_FONT_SIZE:
      if (nsnull != mFont) {
        switch (aProperty) {
          case PROP_FONT_FAMILY:  aValue = mFont->mFamily;   break;
          case PROP_FONT_STYLE:   aValue = mFont->mStyle;    break;
          case PROP_FONT_VARIANT: aValue = mFont->mVariant;  break;
          case PROP_FONT_WEIGHT:  aValue = mFont->mWeight;   break;
          case PROP_FONT_SIZE:    aValue = mFont->mSize;     break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    // nsCSSColor
    case PROP_COLOR:
    case PROP_BACKGROUND_COLOR:
    case PROP_BACKGROUND_IMAGE:
    case PROP_BACKGROUND_REPEAT:
    case PROP_BACKGROUND_ATTACHMENT:
    case PROP_BACKGROUND_X_POSITION:
    case PROP_BACKGROUND_Y_POSITION:
    case PROP_BACKGROUND_FILTER:
    case PROP_CURSOR:
    case PROP_CURSOR_IMAGE:
      if (nsnull != mColor) {
        switch (aProperty) {
          case PROP_COLOR:                  aValue = mColor->mColor;           break;
          case PROP_BACKGROUND_COLOR:       aValue = mColor->mBackColor;       break;
          case PROP_BACKGROUND_IMAGE:       aValue = mColor->mBackImage;       break;
          case PROP_BACKGROUND_REPEAT:      aValue = mColor->mBackRepeat;      break;
          case PROP_BACKGROUND_ATTACHMENT:  aValue = mColor->mBackAttachment;  break;
          case PROP_BACKGROUND_X_POSITION:  aValue = mColor->mBackPositionX;   break;
          case PROP_BACKGROUND_Y_POSITION:  aValue = mColor->mBackPositionY;   break;
          case PROP_BACKGROUND_FILTER:      aValue = mColor->mBackFilter;      break;
          case PROP_CURSOR:                 aValue = mColor->mCursor;          break;
          case PROP_CURSOR_IMAGE:           aValue = mColor->mCursorImage;     break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    // nsCSSText
    case PROP_WORD_SPACING:
    case PROP_LETTER_SPACING:
    case PROP_TEXT_DECORATION:
    case PROP_VERTICAL_ALIGN:
    case PROP_TEXT_TRANSFORM:
    case PROP_TEXT_ALIGN:
    case PROP_TEXT_INDENT:
    case PROP_LINE_HEIGHT:
    case PROP_WHITE_SPACE:
      if (nsnull != mText) {
        switch (aProperty) {
          case PROP_WORD_SPACING:     aValue = mText->mWordSpacing;    break;
          case PROP_LETTER_SPACING:   aValue = mText->mLetterSpacing;  break;
          case PROP_TEXT_DECORATION:  aValue = mText->mDecoration;     break;
          case PROP_VERTICAL_ALIGN:   aValue = mText->mVerticalAlign;  break;
          case PROP_TEXT_TRANSFORM:   aValue = mText->mTextTransform;  break;
          case PROP_TEXT_ALIGN:       aValue = mText->mTextAlign;      break;
          case PROP_TEXT_INDENT:      aValue = mText->mTextIndent;     break;
          case PROP_LINE_HEIGHT:      aValue = mText->mLineHeight;     break;
          case PROP_WHITE_SPACE:      aValue = mText->mWhiteSpace;     break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    // nsCSSMargin
    case PROP_MARGIN_TOP:
    case PROP_MARGIN_RIGHT:
    case PROP_MARGIN_BOTTOM:
    case PROP_MARGIN_LEFT:
      if ((nsnull != mMargin) && (nsnull != mMargin->mMargin)) {
        switch (aProperty) {
          case PROP_MARGIN_TOP:     aValue = mMargin->mMargin->mTop;     break;
          case PROP_MARGIN_RIGHT:   aValue = mMargin->mMargin->mRight;   break;
          case PROP_MARGIN_BOTTOM:  aValue = mMargin->mMargin->mBottom;  break;
          case PROP_MARGIN_LEFT:    aValue = mMargin->mMargin->mLeft;    break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case PROP_PADDING_TOP:
    case PROP_PADDING_RIGHT:
    case PROP_PADDING_BOTTOM:
    case PROP_PADDING_LEFT:
      if ((nsnull != mMargin) && (nsnull != mMargin->mPadding)) {
        switch (aProperty) {
          case PROP_PADDING_TOP:    aValue = mMargin->mPadding->mTop;    break;
          case PROP_PADDING_RIGHT:  aValue = mMargin->mPadding->mRight;  break;
          case PROP_PADDING_BOTTOM: aValue = mMargin->mPadding->mBottom; break;
          case PROP_PADDING_LEFT:   aValue = mMargin->mPadding->mLeft;   break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case PROP_BORDER_TOP_WIDTH:
    case PROP_BORDER_RIGHT_WIDTH:
    case PROP_BORDER_BOTTOM_WIDTH:
    case PROP_BORDER_LEFT_WIDTH:
      if ((nsnull != mMargin) && (nsnull != mMargin->mBorder)) {
        switch (aProperty) {
          case PROP_BORDER_TOP_WIDTH:     aValue = mMargin->mBorder->mTop;     break;
          case PROP_BORDER_RIGHT_WIDTH:   aValue = mMargin->mBorder->mRight;   break;
          case PROP_BORDER_BOTTOM_WIDTH:  aValue = mMargin->mBorder->mBottom;  break;
          case PROP_BORDER_LEFT_WIDTH:    aValue = mMargin->mBorder->mLeft;    break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case PROP_BORDER_TOP_COLOR:
    case PROP_BORDER_RIGHT_COLOR:
    case PROP_BORDER_BOTTOM_COLOR:
    case PROP_BORDER_LEFT_COLOR:
      if ((nsnull != mMargin) && (nsnull != mMargin->mColor)) {
        switch (aProperty) {
          case PROP_BORDER_TOP_COLOR:     aValue = mMargin->mColor->mTop;    break;
          case PROP_BORDER_RIGHT_COLOR:   aValue = mMargin->mColor->mRight;  break;
          case PROP_BORDER_BOTTOM_COLOR:  aValue = mMargin->mColor->mBottom; break;
          case PROP_BORDER_LEFT_COLOR:    aValue = mMargin->mColor->mLeft;   break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case PROP_BORDER_TOP_STYLE:
    case PROP_BORDER_RIGHT_STYLE:
    case PROP_BORDER_BOTTOM_STYLE:
    case PROP_BORDER_LEFT_STYLE:
      if ((nsnull != mMargin) && (nsnull != mMargin->mStyle)) {
        switch (aProperty) {
          case PROP_BORDER_TOP_STYLE:     aValue = mMargin->mStyle->mTop;    break;
          case PROP_BORDER_RIGHT_STYLE:   aValue = mMargin->mStyle->mRight;  break;
          case PROP_BORDER_BOTTOM_STYLE:  aValue = mMargin->mStyle->mBottom; break;
          case PROP_BORDER_LEFT_STYLE:    aValue = mMargin->mStyle->mLeft;   break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    // nsCSSPosition
    case PROP_POSITION:
    case PROP_WIDTH:
    case PROP_HEIGHT:
    case PROP_LEFT:
    case PROP_TOP:
    case PROP_OVERFLOW:
    case PROP_Z_INDEX:
    case PROP_VISIBILITY:
    case PROP_FILTER:
      if (nsnull != mPosition) {
        switch (aProperty) {
          case PROP_POSITION:   aValue = mPosition->mPosition;   break;
          case PROP_WIDTH:      aValue = mPosition->mWidth;      break;
          case PROP_HEIGHT:     aValue = mPosition->mHeight;     break;
          case PROP_LEFT:       aValue = mPosition->mLeft;       break;
          case PROP_TOP:        aValue = mPosition->mTop;        break;
          case PROP_OVERFLOW:   aValue = mPosition->mOverflow;   break;
          case PROP_Z_INDEX:    aValue = mPosition->mZIndex;     break;
          case PROP_VISIBILITY: aValue = mPosition->mVisibility; break;
          case PROP_FILTER:     aValue = mPosition->mFilter;     break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case PROP_CLIP_TOP:
    case PROP_CLIP_RIGHT:
    case PROP_CLIP_BOTTOM:
    case PROP_CLIP_LEFT:
      if ((nsnull != mPosition) && (nsnull != mPosition->mClip)) {
        switch(aProperty) {
          case PROP_CLIP_TOP:     aValue = mPosition->mClip->mTop;     break;
          case PROP_CLIP_RIGHT:   aValue = mPosition->mClip->mRight;   break;
          case PROP_CLIP_BOTTOM:  aValue = mPosition->mClip->mBottom;  break;
          case PROP_CLIP_LEFT:    aValue = mPosition->mClip->mLeft;    break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

      // nsCSSList
    case PROP_LIST_STYLE_TYPE:
    case PROP_LIST_STYLE_IMAGE:
    case PROP_LIST_STYLE_POSITION:
      if (nsnull != mList) {
        switch (aProperty) {
          case PROP_LIST_STYLE_TYPE:      aValue = mList->mType;     break;
          case PROP_LIST_STYLE_IMAGE:     aValue = mList->mImage;    break;
          case PROP_LIST_STYLE_POSITION:  aValue = mList->mPosition; break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

      // nsCSSDisplay
    case PROP_FLOAT:
    case PROP_CLEAR:
    case PROP_DISPLAY:
    case PROP_DIRECTION:
      if (nsnull != mDisplay) {
        switch (aProperty) {
          case PROP_FLOAT:      aValue = mDisplay->mFloat;      break;
          case PROP_CLEAR:      aValue = mDisplay->mClear;      break;
          case PROP_DISPLAY:    aValue = mDisplay->mDisplay;    break;
          case PROP_DIRECTION:  aValue = mDisplay->mDirection;  break;
        }
      }
      else {
        aValue.Reset();
      }
      break;

    case PROP_BACKGROUND:
    case PROP_BORDER:
    case PROP_CLIP:
    case PROP_FONT:
    case PROP_LIST_STYLE:
    case PROP_MARGIN:
    case PROP_PADDING:
    case PROP_BACKGROUND_POSITION:
    case PROP_BORDER_TOP:
    case PROP_BORDER_RIGHT:
    case PROP_BORDER_BOTTOM:
    case PROP_BORDER_LEFT:
    case PROP_BORDER_COLOR:
    case PROP_BORDER_STYLE:
    case PROP_BORDER_WIDTH:
      NS_ERROR("can't query for shorthand properties");
    default:
      result = NS_ERROR_ILLEGAL_VALUE;
      break;
  }
  return result;
}

void CSSDeclarationImpl::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 index = aIndent; --index >= 0; ) fputs("  ", out);

  fputs("{ ", out);

  if (nsnull != mFont) {
    mFont->List(out);
  }
  if (nsnull != mColor) {
    mColor->List(out);
  }
  if (nsnull != mText) {
    mText->List(out);
  }
  if (nsnull != mMargin) {
    mMargin->List(out);
  }
  if (nsnull != mPosition) {
    mPosition->List(out);
  }
  if (nsnull != mList) {
    mList->List(out);
  }
  if (nsnull != mDisplay) {
    mDisplay->List(out);
  }

  fputs("}", out);
}

NS_HTML nsresult
  NS_NewCSSDeclaration(nsICSSDeclaration** aInstancePtrResult)
{
  if (aInstancePtrResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  CSSDeclarationImpl  *it = new CSSDeclarationImpl();

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kICSSDeclarationIID, (void **) aInstancePtrResult);
}


/*
font 
===========
font-family: string (list)
font-style: enum
font-variant: enum (ie: small caps)
font-weight: enum
font-size: abs, pct, enum, +-1


color/background
=============
color: color
background-color: color
background-image: url(string)
background-repeat: enum 
background-attachment: enum
background-position-x -y: abs, pct, enum (left/top center right/bottom (pct?))
cursor: enum
cursor-image: url(string)


text
=======
word-spacing: abs, "normal"
letter-spacing: abs, "normal"
text-decoration: enum
vertical-align: enum, pct
text-transform: enum
text-align: enum
text-indent: abs, pct
line-height: "normal", abs, pct, number-factor
white-space: enum

margin
=======
margin-top -right -bottom -left: "auto", abs, pct
padding-top -right -bottom -left: abs, pct
border-top -right -bottom -left-width: enum, abs
border-top -right -bottom -left-color: color
border-top -right -bottom -left-style: enum

size
=======
position: enum
width: abs, pct, "auto"
height: abs, pct, "auto"
left: abs, pct, "auto"
top: abs, pct, "auto"
clip: shape, "auto" (shape: rect - abs, auto)
overflow: enum
z-index: int, auto
visibity: enum

display
=======
float: enum
clear: enum
direction: enum
display: enum

filter: string

list
========
list-style-type: enum
list-style-image: url, "none"
list-style-position: enum (bool? in/out)

*/



