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
static NS_DEFINE_IID(kStyleListSID, NS_STYLELIST_SID);
static NS_DEFINE_IID(kStylePositionSID, NS_STYLEPOSITION_SID);
static NS_DEFINE_IID(kStyleTextSID, NS_STYLETEXT_SID);
static NS_DEFINE_IID(kStyleDisplaySID, NS_STYLEDISPLAY_SID);
static NS_DEFINE_IID(kStyleTableSID, NS_STYLETABLE_SID);

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

  void InheritFrom(const nsStyleFont& aCopy);

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
nsStyleColor::nsStyleColor(void) { }
nsStyleColor::~nsStyleColor(void) { }

struct StyleColorImpl: public nsStyleColor {
  StyleColorImpl(void)
  {
    mBackgroundAttachment = NS_STYLE_BG_ATTACHMENT_SCROLL;
    mBackgroundFlags = NS_STYLE_BG_COLOR_TRANSPARENT;
    mBackgroundRepeat = NS_STYLE_BG_REPEAT_XY;

    mBackgroundColor = NS_RGB(192,192,192);
    mCursor = NS_STYLE_CURSOR_INHERIT;
  }

  void InheritFrom(const nsStyleColor& aCopy);

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
  : mMargin(),
    mPadding(),
    mBorder(),
    mHasCachedMargin(PR_FALSE),
    mHasCachedPadding(PR_FALSE),
    mHasCachedBorder(PR_FALSE)
{
  mBorderStyle[0] = NS_STYLE_BORDER_STYLE_NONE;
  mBorderStyle[1] = NS_STYLE_BORDER_STYLE_NONE;
  mBorderStyle[2] = NS_STYLE_BORDER_STYLE_NONE;
  mBorderStyle[3] = NS_STYLE_BORDER_STYLE_NONE;
  mBorderColor[0] = NS_RGB(0, 0, 0);
  mBorderColor[1] = NS_RGB(0, 0, 0);
  mBorderColor[2] = NS_RGB(0, 0, 0);
  mBorderColor[3] = NS_RGB(0, 0, 0);
}

#define NS_SPACING_MARGIN   0
#define NS_SPACING_PADDING  1
#define NS_SPACING_BORDER   2

static nscoord CalcSideFor(const nsIFrame* aFrame, const nsStyleCoord& aCoord, 
                           PRUint8 aSpacing, PRUint8 aSide,
                           const nscoord* aEnumTable, PRInt32 aNumEnums)
{
  nscoord result = 0;

  switch (aCoord.GetUnit()) {
    case eStyleUnit_Auto:
      // XXX need to call back to frame to compute
      NS_NOTYETIMPLEMENTED("auto value");
      break;

    case eStyleUnit_Inherit:
      nsIFrame* parentFrame;
      aFrame->GetGeometricParent(parentFrame);  // XXX may not be direct parent...
      if (nsnull != parentFrame) {
        nsIStyleContext* parentContext;
        parentFrame->GetStyleContext(nsnull, parentContext);
        if (nsnull != parentContext) {
          nsStyleSpacing* parentSpacing = (nsStyleSpacing*)parentContext->GetData(kStyleSpacingSID);
          nsMargin  parentMargin;
          switch (aSpacing) {
            case NS_SPACING_MARGIN:   parentSpacing->CalcMarginFor(parentFrame, parentMargin);  
              break;
            case NS_SPACING_PADDING:  parentSpacing->CalcPaddingFor(parentFrame, parentMargin);  
              break;
            case NS_SPACING_BORDER:   parentSpacing->CalcBorderFor(parentFrame, parentMargin);  
              break;
          }
          switch (aSide) {
            case NS_SIDE_LEFT:    result = parentMargin.left;   break;
            case NS_SIDE_TOP:     result = parentMargin.top;    break;
            case NS_SIDE_RIGHT:   result = parentMargin.right;  break;
            case NS_SIDE_BOTTOM:  result = parentMargin.bottom; break;
          }
          NS_RELEASE(parentContext);
        }
      }

    case eStyleUnit_Percent:
      // XXX call frame to get percent base, do the math
      NS_NOTYETIMPLEMENTED("percent value");
      break;

    case eStyleUnit_Coord:
      result = aCoord.GetCoordValue();
      break;

    case eStyleUnit_Enumerated:
      if (nsnull != aEnumTable) {
        PRInt32 value = aCoord.GetIntValue();
        if ((0 <= value) && (value < aNumEnums)) {
          return aEnumTable[aCoord.GetIntValue()];
        }
      }
      break;

    case eStyleUnit_Null:
    case eStyleUnit_Normal:
    case eStyleUnit_Integer:
    case eStyleUnit_Proportional:
    default:
      result = 0;
      break;
  }
  return result;
}

static void CalcSidesFor(const nsIFrame* aFrame, const nsStyleSides& aSides, 
                         PRUint8 aSpacing, 
                         const nscoord* aEnumTable, PRInt32 aNumEnums,
                         nsMargin& aResult)
{
  nsStyleCoord  coord;

  aResult.left = CalcSideFor(aFrame, aSides.GetLeft(coord), aSpacing, NS_SIDE_LEFT,
                             aEnumTable, aNumEnums);
  aResult.top = CalcSideFor(aFrame, aSides.GetTop(coord), aSpacing, NS_SIDE_TOP,
                            aEnumTable, aNumEnums);
  aResult.right = CalcSideFor(aFrame, aSides.GetRight(coord), aSpacing, NS_SIDE_RIGHT,
                              aEnumTable, aNumEnums);
  aResult.bottom = CalcSideFor(aFrame, aSides.GetBottom(coord), aSpacing, NS_SIDE_BOTTOM,
                               aEnumTable, aNumEnums);
}

void nsStyleSpacing::CalcMarginFor(const nsIFrame* aFrame, nsMargin& aMargin) const
{
  if (mHasCachedMargin) {
    aMargin = mCachedMargin;
  }
  else {
    CalcSidesFor(aFrame, mMargin, NS_SPACING_MARGIN, nsnull, 0, aMargin);
  }
}

void nsStyleSpacing::CalcPaddingFor(const nsIFrame* aFrame, nsMargin& aPadding) const
{
  if (mHasCachedPadding) {
    aPadding = mCachedPadding;
  }
  else {
    CalcSidesFor(aFrame, mPadding, NS_SPACING_PADDING, nsnull, 0, aPadding);
  }
}

static const nscoord kBorderWidths[3] = 
  { NS_POINTS_TO_TWIPS_INT(1), 
    NS_POINTS_TO_TWIPS_INT(3), 
    NS_POINTS_TO_TWIPS_INT(5) };

void nsStyleSpacing::CalcBorderFor(const nsIFrame* aFrame, nsMargin& aBorder) const
{
  if (mHasCachedBorder) {
    aBorder = mCachedBorder;
  }
  else {
    CalcSidesFor(aFrame, mBorder, NS_SPACING_BORDER, kBorderWidths, 3, aBorder);
  }
}

void nsStyleSpacing::CalcBorderPaddingFor(const nsIFrame* aFrame, nsMargin& aBorderPadding) const
{
  if (mHasCachedPadding && mHasCachedBorder) {
    aBorderPadding = mCachedBorderPadding;
  }
  else {
    nsMargin border;
    CalcBorderFor(aFrame, border);
    CalcPaddingFor(aFrame, aBorderPadding);
    aBorderPadding += border;
  }
}


struct StyleSpacingImpl: public nsStyleSpacing {
  StyleSpacingImpl(void)
    : nsStyleSpacing()
  {}

  void InheritFrom(const nsStyleSpacing& aCopy);
  void RecalcData(nsIPresContext* aPresContext);
};

void StyleSpacingImpl::InheritFrom(const nsStyleSpacing& aCopy)
{
  // spacing values not inherited
}

inline PRBool IsFixedUnit(nsStyleUnit aUnit, PRBool aEnumOK)
{
  return PRBool((aUnit == eStyleUnit_Null) || 
                (aUnit == eStyleUnit_Coord) || 
                (aEnumOK && (aUnit == eStyleUnit_Enumerated)));
}

static PRBool IsFixedData(const nsStyleSides& aSides, PRBool aEnumOK)
{
  return PRBool(IsFixedUnit(aSides.GetLeftUnit(), aEnumOK) &&
                IsFixedUnit(aSides.GetTopUnit(), aEnumOK) &&
                IsFixedUnit(aSides.GetRightUnit(), aEnumOK) &&
                IsFixedUnit(aSides.GetBottomUnit(), aEnumOK));
}

static nscoord CalcCoord(const nsStyleCoord& aCoord, 
                         const nscoord* aEnumTable, 
                         PRInt32 aNumEnums)
{
  switch (aCoord.GetUnit()) {
    case eStyleUnit_Null:
      return 0;
    case eStyleUnit_Coord:
      return aCoord.GetCoordValue();
    case eStyleUnit_Enumerated:
      if (nsnull != aEnumTable) {
        PRInt32 value = aCoord.GetIntValue();
        if ((0 <= value) && (value < aNumEnums)) {
          return aEnumTable[aCoord.GetIntValue()];
        }
      }
      break;
    default:
      NS_ERROR("bad unit type");
      break;
  }
  return 0;
}

void StyleSpacingImpl::RecalcData(nsIPresContext* aPresContext)
{
  if (IsFixedData(mMargin, PR_FALSE)) {
    nsStyleCoord  coord;
    mCachedMargin.left = CalcCoord(mMargin.GetLeft(coord), nsnull, 0);
    mCachedMargin.top = CalcCoord(mMargin.GetTop(coord), nsnull, 0);
    mCachedMargin.right = CalcCoord(mMargin.GetRight(coord), nsnull, 0);
    mCachedMargin.bottom = CalcCoord(mMargin.GetBottom(coord), nsnull, 0);

    mHasCachedMargin = PR_TRUE;
  }
  else {
    mHasCachedMargin = PR_FALSE;
  }

  if (IsFixedData(mPadding, PR_FALSE)) {
    nsStyleCoord  coord;
    mCachedPadding.left = CalcCoord(mPadding.GetLeft(coord), nsnull, 0);
    mCachedPadding.top = CalcCoord(mPadding.GetTop(coord), nsnull, 0);
    mCachedPadding.right = CalcCoord(mPadding.GetRight(coord), nsnull, 0);
    mCachedPadding.bottom = CalcCoord(mPadding.GetBottom(coord), nsnull, 0);

    mHasCachedPadding = PR_TRUE;
  }
  else {
    mHasCachedPadding = PR_FALSE;
  }

  if (IsFixedData(mBorder, PR_TRUE)) {
    nsStyleCoord  coord;
    mCachedBorder.left = CalcCoord(mBorder.GetLeft(coord), kBorderWidths, 3);
    mCachedBorder.top = CalcCoord(mBorder.GetTop(coord), kBorderWidths, 3);
    mCachedBorder.right = CalcCoord(mBorder.GetRight(coord), kBorderWidths, 3);
    mCachedBorder.bottom = CalcCoord(mBorder.GetBottom(coord), kBorderWidths, 3);

    mHasCachedPadding = PR_TRUE;
  }
  else {
    mHasCachedPadding = PR_FALSE;
  }

  if (mHasCachedBorder && mHasCachedPadding) {
    mCachedBorderPadding = mCachedPadding;
    mCachedBorderPadding += mCachedBorder;
  }


  // XXX fixup missing border colors

}


// --------------------
// nsStyleList
//
nsStyleList::nsStyleList(void) { }
nsStyleList::~nsStyleList(void) { }

struct StyleListImpl: public nsStyleList {
  StyleListImpl(void)
  {
    mListStyleType = NS_STYLE_LIST_STYLE_BASIC;
    mListStylePosition = NS_STYLE_LIST_STYLE_POSITION_OUTSIDE;
  }

  void InheritFrom(const nsStyleList& aCopy);
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
nsStylePosition::nsStylePosition(void) { }

struct StylePositionImpl: public nsStylePosition {
  StylePositionImpl(void)
  {
    mPosition = NS_STYLE_POSITION_STATIC;
    mOverflow = NS_STYLE_OVERFLOW_VISIBLE;
    mLeftOffset.SetAutoValue();
    mTopOffset.SetAutoValue();
    mWidth.SetAutoValue();
    mHeight.SetAutoValue();
    mZIndex.SetAutoValue();
    mClipFlags = NS_STYLE_CLIP_AUTO;
    mClip.SizeTo(0,0,0,0);
  }

  void InheritFrom(const nsStylePosition& aCopy);

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

nsStyleText::nsStyleText(void) { }

struct StyleTextImpl: public nsStyleText {
  StyleTextImpl(void) {
    mTextAlign = NS_STYLE_TEXT_ALIGN_LEFT;
    mTextDecoration = NS_STYLE_TEXT_DECORATION_NONE;
    mTextTransform = NS_STYLE_TEXT_TRANSFORM_NONE;
    mWhiteSpace = NS_STYLE_WHITESPACE_NORMAL;

    mLetterSpacing.SetNormalValue();
    mLineHeight.SetNormalValue();
    mTextIndent.SetCoordValue(0);
    mWordSpacing.SetNormalValue();
    mVerticalAlign.SetIntValue(NS_STYLE_VERTICAL_ALIGN_BASELINE, eStyleUnit_Enumerated);
  }

  void InheritFrom(const nsStyleText& aCopy);
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

nsStyleDisplay::nsStyleDisplay(void) { }

struct StyleDisplayImpl: public nsStyleDisplay {
  StyleDisplayImpl(void) {
    mDirection = NS_STYLE_DIRECTION_LTR;
    mDisplay = NS_STYLE_DISPLAY_INLINE;
    mFloats = NS_STYLE_FLOAT_NONE;
    mBreakType = NS_STYLE_CLEAR_NONE;
    mBreakBefore = PR_FALSE;
    mBreakAfter = PR_FALSE;
  }

  void InheritFrom(const nsStyleDisplay& aCopy);
};

void StyleDisplayImpl::InheritFrom(const nsStyleDisplay& aCopy)
{
  mDirection = aCopy.mDirection;
}

// --------------------
// nsStyleTable
//

nsStyleTable::nsStyleTable(void) { }

struct StyleTableImpl: public nsStyleTable {
  StyleTableImpl(void) {
    mFrame = NS_STYLE_TABLE_FRAME_NONE;
    mRules = NS_STYLE_TABLE_RULES_NONE;
    mCellPadding.Reset();
    mCellSpacing.Reset();
  }

  void InheritFrom(const nsStyleTable& aCopy);
};

void StyleTableImpl::InheritFrom(const nsStyleTable& aCopy)
{
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
  virtual void RecalcAutomaticData(nsIPresContext* aPresContext);

  nsIStyleContext*  mParent;
  PRUint32          mHashValid: 1;
  PRUint32          mHashValue: 31;
  nsISupportsArray* mRules;

  // the style data...
  StyleFontImpl     mFont;
  StyleColorImpl    mColor;
  StyleSpacingImpl  mSpacing;
  StyleListImpl     mList;
  StylePositionImpl mPosition;
  StyleTextImpl     mText;
  StyleDisplayImpl  mDisplay;
  StyleTableImpl*   mTable;
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
    mList(),
    mPosition(),
    mText(),
    mDisplay(),
    mTable(nsnull)
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
  if (nsnull != mTable) {
    delete mTable;
    mTable = nsnull;
  }

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
  if (aSID.Equals(kStyleTableSID)) {  // this one gets created lazily
    if (nsnull == mTable) {
      mTable = new StyleTableImpl();
      if (nsnull != mParent) {
        StyleContextImpl* parent = (StyleContextImpl*)mParent;
        if (nsnull != parent->mTable) {
          mTable->InheritFrom(*(parent->mTable));
        }
      }
    }
    return mTable;
  }
  return nsnull;
}

void StyleContextImpl::InheritFrom(const StyleContextImpl& aParent)
{
  mFont.InheritFrom(aParent.mFont);
  mColor.InheritFrom(aParent.mColor);
  mSpacing.InheritFrom(aParent.mSpacing);
  mList.InheritFrom(aParent.mList);
  mText.InheritFrom(aParent.mText);
  mPosition.InheritFrom(aParent.mPosition);
  mDisplay.InheritFrom(aParent.mDisplay);
}

void StyleContextImpl::RecalcAutomaticData(nsIPresContext* aPresContext)
{
  mSpacing.RecalcData(aPresContext);
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
  context->RecalcAutomaticData(aPresContext);

  return context->QueryInterface(kIStyleContextIID, (void **) aInstancePtrResult);
}
