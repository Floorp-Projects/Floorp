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
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsString.h"
#include "nsUnitConversion.h"
#include "nsIContent.h"
#include "nsIPresContext.h"
#include "nsIStyleRule.h"
#include "nsISupportsArray.h"
#include "nsCRT.h"

#include "nsIFrame.h"

//#define DEBUG_REFS

#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
#else
static const PRBool gsDebug = PR_FALSE;
#endif

static NS_DEFINE_IID(kStyleFontSID, NS_STYLEFONT_SID);
static NS_DEFINE_IID(kStyleColorSID, NS_STYLECOLOR_SID);
static NS_DEFINE_IID(kStyleSpacingSID, NS_STYLESPACING_SID);
static NS_DEFINE_IID(kStyleBorderSID, NS_STYLEBORDER_SID);
static NS_DEFINE_IID(kStyleListSID, NS_STYLELIST_SID);
static NS_DEFINE_IID(kStylePositionSID, NS_STYLEPOSITION_SID);
static NS_DEFINE_IID(kStyleTextSID, NS_STYLETEXT_SID);
static NS_DEFINE_IID(kStyleDisplaySID, NS_STYLEDISPLAY_SID);

static NS_DEFINE_IID(kIStyleContextIID, NS_ISTYLECONTEXT_IID);


// --------------------
// nsStyleFont
//
nsStyleFont::nsStyleFont(const nsFont& aFont)
  : mFont(aFont)
{ }

nsStyleFont::~nsStyleFont(void) { }

struct StyleFontImpl : public nsStyleFont {
  StyleFontImpl(const nsFont& aFont)
    : nsStyleFont(aFont)
  {}

  virtual const nsID& GetID(void)
  { return kStyleFontSID; }

  virtual void InheritFrom(const nsStyleFont& aCopy);

private:  // These are not allowed
  StyleFontImpl(const StyleFontImpl& aOther);
  StyleFontImpl& operator=(const StyleFontImpl& aOther);
};

void StyleFontImpl::InheritFrom(const nsStyleFont& aCopy)
{
  mFont = aCopy.mFont;
  mThreeD = aCopy.mThreeD;
}

// --------------------
// nsStyleColor
//
nsStyleColor::nsStyleColor() { }
nsStyleColor::~nsStyleColor() { }

struct StyleColorImpl: public nsStyleColor {
  StyleColorImpl(void)
  {
    mBackgroundAttachment = NS_STYLE_BG_ATTACHMENT_SCROLL;
    mBackgroundFlags = NS_STYLE_BG_COLOR_TRANSPARENT;
    mBackgroundRepeat = NS_STYLE_BG_REPEAT_OFF;

    mBackgroundColor = NS_RGB(192,192,192);
    mCursor = NS_STYLE_CURSOR_INHERIT;
  }

  virtual const nsID& GetID(void)
  { return kStyleColorSID;  }

  virtual void InheritFrom(const nsStyleColor& aCopy);

private:  // These are not allowed
  StyleColorImpl(const StyleColorImpl& aOther);
  StyleColorImpl& operator=(const StyleColorImpl& aOther);
};

void StyleColorImpl::InheritFrom(const nsStyleColor& aCopy)
{
  mColor = aCopy.mColor;
}


// --------------------
// nsStyleSpacing
//
nsStyleSpacing::nsStyleSpacing(void)
  : mMargin(0, 0, 0, 0),
    mPadding(0, 0, 0, 0),
    mBorderPadding(0, 0, 0, 0)
{
}

struct StyleSpacingImpl: public nsStyleSpacing {
  StyleSpacingImpl(void)
    : nsStyleSpacing()
  {}

  virtual const nsID& GetID(void)
  { return kStyleSpacingSID;  }

  virtual void InheritFrom(const nsStyleSpacing& aCopy);
};

void StyleSpacingImpl::InheritFrom(const nsStyleSpacing& aCopy)
{
  // spacing values not inherited
}

// --------------------
// nsStyleBorder
//

nsStyleBorder::nsStyleBorder(void)
  : mSize(0, 0, 0, 0)
{
  mSizeFlag[0] = mSizeFlag[1] = mSizeFlag[2] = mSizeFlag[3] = NS_STYLE_BORDER_WIDTH_LENGTH_VALUE;
  mStyle[0] = mStyle[1] = mStyle[2] = mStyle[3] = NS_STYLE_BORDER_STYLE_NONE;
  mColor[0] = mColor[1] = mColor[2] = mColor[3] = NS_RGB(0, 0, 0);
}

struct StyleBorderImpl: public nsStyleBorder {
  StyleBorderImpl(void)
    : nsStyleBorder()
  {}

  virtual const nsID& GetID(void)
  { return kStyleBorderSID;  }

  virtual void InheritFrom(const nsStyleBorder& aCopy);
};

void StyleBorderImpl::InheritFrom(const nsStyleBorder& aCopy)
{
  // border values not inherited
}

// --------------------
// nsStyleList
//
nsStyleList::nsStyleList() { }
nsStyleList::~nsStyleList() { }

struct StyleListImpl: public nsStyleList {
  StyleListImpl(void)
  {
    mListStyleType = NS_STYLE_LIST_STYLE_BASIC;
    mListStylePosition = NS_STYLE_LIST_STYLE_POSITION_OUTSIDE;
  }

  virtual const nsID& GetID(void)
  { return kStyleListSID; }

  virtual void InheritFrom(const nsStyleList& aCopy);
};

void StyleListImpl::InheritFrom(const nsStyleList& aCopy)
{
  mListStyleType = aCopy.mListStyleType;
  mListStyleImage = aCopy.mListStyleImage;
  mListStylePosition = aCopy.mListStylePosition;
}

// --------------------
// nsStylePosition
//
nsStylePosition::nsStylePosition() { }

struct StylePositionImpl: public nsStylePosition {
  StylePositionImpl(void)
  {
    mPosition = NS_STYLE_POSITION_STATIC;
    mOverflow = NS_STYLE_OVERFLOW_VISIBLE;
    mLeftOffsetFlags = NS_STYLE_POSITION_VALUE_AUTO;
    mLeftOffset = 0;
    mTopOffsetFlags = NS_STYLE_POSITION_VALUE_AUTO;
    mTopOffset = 0;
    mWidthFlags = NS_STYLE_POSITION_VALUE_AUTO;
    mWidth = 0;
    mHeightFlags = NS_STYLE_POSITION_VALUE_AUTO;
    mHeight = 0;
    mZIndex = 0;
    mClipFlags = NS_STYLE_CLIP_AUTO;
    mClip.SizeTo(0,0,0,0);
  }

  virtual const nsID& GetID(void)
  { return kStylePositionSID;  }

  virtual void InheritFrom(const nsStylePosition& aCopy);

private:  // These are not allowed
  StylePositionImpl(const StylePositionImpl& aOther);
  StylePositionImpl& operator=(const StylePositionImpl& aOther);
};

void StylePositionImpl::InheritFrom(const nsStylePosition& aCopy)
{
  // positioning values not inherited
}

// --------------------
// nsStyleText
//

nsStyleText::nsStyleText() { }

struct StyleTextImpl: public nsStyleText {
  StyleTextImpl() {
    mTextAlign = NS_STYLE_TEXT_ALIGN_LEFT;
    mTextDecoration = NS_STYLE_TEXT_DECORATION_NONE;
    mTextTransform = NS_STYLE_TEXT_TRANSFORM_NONE;
    mWhiteSpace = NS_STYLE_WHITESPACE_NORMAL;

    mLetterSpacing.SetNormal();
    mLineHeight.SetNormal();
    mTextIndent.Set(0);
    mWordSpacing.SetNormal();
    mVerticalAlign.Set(NS_STYLE_VERTICAL_ALIGN_BASELINE, eStyleUnit_Enumerated);
  }

  virtual const nsID& GetID() {
    return kStyleTextSID;
  }

  virtual void InheritFrom(const nsStyleText& aCopy);
};

void StyleTextImpl::InheritFrom(const nsStyleText& aCopy)
{
  // Properties not listed here are not inherited: mVerticalAlign,
  // mTextDecoration
  mTextAlign = aCopy.mTextAlign;
  mTextTransform = aCopy.mTextTransform;
  mWhiteSpace = aCopy.mWhiteSpace;

  mLetterSpacing = aCopy.mLetterSpacing;
  mLineHeight = aCopy.mLineHeight;
  mTextIndent = aCopy.mTextIndent;
  mWordSpacing = aCopy.mWordSpacing;
}

// --------------------
// nsStyleDisplay
//

nsStyleDisplay::nsStyleDisplay() { }

struct StyleDisplayImpl: public nsStyleDisplay {
  StyleDisplayImpl() {
    mDirection = NS_STYLE_DIRECTION_LTR;
    mDisplay = NS_STYLE_DISPLAY_INLINE;
    mFloats = NS_STYLE_FLOAT_NONE;
    mBreakType = NS_STYLE_CLEAR_NONE;
    mBreakBefore = PR_FALSE;
    mBreakAfter = PR_FALSE;
  }

  virtual const nsID& GetID() {
    return kStyleDisplaySID;
  }

  virtual void InheritFrom(const nsStyleDisplay& aCopy);
};

void StyleDisplayImpl::InheritFrom(const nsStyleDisplay& aCopy)
{
  mDirection = aCopy.mDirection;
}

//----------------------------------------------------------------------

class StyleContextImpl : public nsIStyleContext {
public:
  StyleContextImpl(nsIStyleContext* aParent, nsISupportsArray* aRules, nsIPresContext* aPresContext);
  ~StyleContextImpl();

  void* operator new(size_t sz) {
    void* rv = new char[sz];
    nsCRT::zero(rv, sz);
    return rv;
  }

  NS_DECL_ISUPPORTS

  virtual nsIStyleContext*  GetParent(void) const;
  virtual nsISupportsArray* GetStyleRules(void) const;

  virtual PRBool    Equals(const nsIStyleContext* aOther) const;
  virtual PRUint32  HashValue(void) const;

  virtual nsStyleStruct* GetData(const nsIID& aSID);

  virtual void InheritFrom(const StyleContextImpl& aParent);
  virtual void RecalcAutomaticData(void);

  nsIStyleContext*  mParent;
  PRUint32          mHashValid: 1;
  PRUint32          mHashValue: 31;
  nsISupportsArray* mRules;

  // the style data...
  StyleFontImpl     mFont;
  StyleColorImpl    mColor;
  StyleSpacingImpl  mSpacing;
  StyleBorderImpl   mBorder;
  StyleListImpl     mList;
  StylePositionImpl mPosition;
  StyleTextImpl     mText;
  StyleDisplayImpl  mDisplay;
};

#ifdef DEBUG_REFS
static PRInt32 gInstanceCount;
#endif

StyleContextImpl::StyleContextImpl(nsIStyleContext* aParent,
                                   nsISupportsArray* aRules, 
                                   nsIPresContext* aPresContext)
  : mParent(aParent), // weak ref
    mRules(aRules),
    mFont(aPresContext->GetDefaultFont()),
    mColor(),
    mSpacing(),
    mBorder(),
    mList(),
    mPosition(),
    mText(),
    mDisplay()
{
  NS_INIT_REFCNT();
  NS_IF_ADDREF(mRules);

  if (nsnull != aParent) {
    InheritFrom((StyleContextImpl&)*aParent);
  }

  if (nsnull != mRules) {
    PRInt32 index = mRules->Count();
    while (0 < index) {
      nsIStyleRule* rule = (nsIStyleRule*)mRules->ElementAt(--index);
      rule->MapStyleInto(this, aPresContext);
      NS_RELEASE(rule);
    }
  }
#ifdef DEBUG_REFS
  ++gInstanceCount;
  fprintf(stdout, "%d + StyleContext\n", gInstanceCount);
#endif
}

StyleContextImpl::~StyleContextImpl()
{
  mParent = nsnull; // weak ref
  NS_IF_RELEASE(mRules);
#ifdef DEBUG_REFS
  --gInstanceCount;
  fprintf(stdout, "%d - StyleContext\n", gInstanceCount);
#endif
}

NS_IMPL_ISUPPORTS(StyleContextImpl, kIStyleContextIID)

nsIStyleContext* StyleContextImpl::GetParent(void) const
{
  NS_IF_ADDREF(mParent);
  return mParent;
}

nsISupportsArray* StyleContextImpl::GetStyleRules(void) const
{
  NS_IF_ADDREF(mRules);
  return mRules;
}


PRBool StyleContextImpl::Equals(const nsIStyleContext* aOther) const
{
  PRBool  result = PR_TRUE;
  const StyleContextImpl* other = (StyleContextImpl*)aOther;

  if (other != this) {
    if (mParent != other->mParent) {
      result = PR_FALSE;
    }
    else {
      if ((nsnull != mRules) && (nsnull != other->mRules)) {
        result = mRules->Equals(other->mRules);
      }
      else {
        result = PRBool((nsnull == mRules) && (nsnull == other->mRules));
      }
    }
  }
  return result;
}

PRUint32 StyleContextImpl::HashValue(void) const
{
  if (0 == mHashValid) {
    ((StyleContextImpl*)this)->mHashValue = ((nsnull != mParent) ? mParent->HashValue() : 0);
    if (nsnull != mRules) {
      PRInt32 index = mRules->Count();
      while (0 <= --index) {
        nsIStyleRule* rule = (nsIStyleRule*)mRules->ElementAt(index);
        PRUint32 hash = rule->HashValue();
        ((StyleContextImpl*)this)->mHashValue ^= (hash & 0x7FFFFFFF);
        NS_RELEASE(rule);
      }
    }
    ((StyleContextImpl*)this)->mHashValid = 1;
  }
  return mHashValue;
}


nsStyleStruct* StyleContextImpl::GetData(const nsIID& aSID)
{
  if (aSID.Equals(kStyleFontSID)) {
    return &mFont;
  }
  if (aSID.Equals(kStyleColorSID)) {
    return &mColor;
  }
  if (aSID.Equals(kStyleSpacingSID)) {
    return &mSpacing;
  }
  if (aSID.Equals(kStyleBorderSID)) {
    return &mBorder;
  }
  if (aSID.Equals(kStyleListSID)) {
    return &mList;
  }
  if (aSID.Equals(kStylePositionSID)) {
    return &mPosition;
  }
  if (aSID.Equals(kStyleTextSID)) {
    return &mText;
  }
  if (aSID.Equals(kStyleDisplaySID)) {
    return &mDisplay;
  }
  return nsnull;
}

void StyleContextImpl::InheritFrom(const StyleContextImpl& aParent)
{
  mFont.InheritFrom(aParent.mFont);
  mColor.InheritFrom(aParent.mColor);
  mSpacing.InheritFrom(aParent.mSpacing);
  mBorder.InheritFrom(aParent.mBorder);
  mList.InheritFrom(aParent.mList);
  mText.InheritFrom(aParent.mText);
  mPosition.InheritFrom(aParent.mPosition);
  mDisplay.InheritFrom(aParent.mDisplay);
}

static void CalcBorderSize(nscoord& aSize, const nsStyleBorder& aBorder, PRUint8 aSide)
{
  static const nscoord kBorderSize[3] = // XXX need real numbers here (this is probably wrong anyway)
      { NS_POINTS_TO_TWIPS_INT(1), 
        NS_POINTS_TO_TWIPS_INT(3), 
        NS_POINTS_TO_TWIPS_INT(5) };
  PRUint8 style = aBorder.mStyle[aSide];
  if (NS_STYLE_BORDER_STYLE_NONE == style) {
    aSize = 0;
  }
  else {
    PRUint8 flag = aBorder.mSizeFlag[aSide];
    if (flag < NS_STYLE_BORDER_WIDTH_LENGTH_VALUE) {
      aSize = kBorderSize[flag];
    }
  }
}

void StyleContextImpl::RecalcAutomaticData(void)
{
  CalcBorderSize(mBorder.mSize.top, mBorder, NS_SIDE_TOP);
  CalcBorderSize(mBorder.mSize.right, mBorder, NS_SIDE_RIGHT);
  CalcBorderSize(mBorder.mSize.bottom, mBorder, NS_SIDE_BOTTOM);
  CalcBorderSize(mBorder.mSize.left, mBorder, NS_SIDE_LEFT);

  // XXX fixup missing border colors

  mSpacing.mBorder = mBorder.mSize;
  mSpacing.mBorderPadding = mSpacing.mPadding;
  mSpacing.mBorderPadding += mBorder.mSize;
}

NS_LAYOUT nsresult
NS_NewStyleContext(nsIStyleContext** aInstancePtrResult,
                   nsISupportsArray* aRules,
                   nsIPresContext* aPresContext,
                   nsIContent* aContent,
                   nsIFrame* aParentFrame)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIStyleContext* parent = nsnull;
  if (nsnull != aParentFrame) {
    aParentFrame->GetStyleContext(aPresContext, parent);
    NS_ASSERTION(nsnull != parent, "parent frame must have style context");
  }

  StyleContextImpl* context = new StyleContextImpl(parent, aRules, aPresContext);
  NS_IF_RELEASE(parent);
  if (nsnull == context) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  context->RecalcAutomaticData();

  return context->QueryInterface(kIStyleContextIID, (void **) aInstancePtrResult);
}
