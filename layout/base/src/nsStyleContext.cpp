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

  void ResetFrom(const nsStyleFont* aParent, nsIPresContext* aPresContext);

private:  // These are not allowed
  StyleFontImpl(const StyleFontImpl& aOther);
  StyleFontImpl& operator=(const StyleFontImpl& aOther);
};

void StyleFontImpl::ResetFrom(const nsStyleFont* aParent, nsIPresContext* aPresContext)
{
  if (nsnull != aParent) {
    mFont = aParent->mFont;
    mThreeD = aParent->mThreeD;
  }
  else {
    mFont = aPresContext->GetDefaultFont();
    mThreeD = PR_FALSE;
  }
}

// --------------------
// nsStyleColor
//
nsStyleColor::nsStyleColor(void) { }
nsStyleColor::~nsStyleColor(void) { }

struct StyleColorImpl: public nsStyleColor {
  StyleColorImpl(void)  { }

  void ResetFrom(const nsStyleColor* aParent, nsIPresContext* aPresContext);

private:  // These are not allowed
  StyleColorImpl(const StyleColorImpl& aOther);
  StyleColorImpl& operator=(const StyleColorImpl& aOther);
};

void StyleColorImpl::ResetFrom(const nsStyleColor* aParent, nsIPresContext* aPresContext)
{
  if (nsnull != aParent) {
    mColor = aParent->mColor;
  }
  else {
    mColor = NS_RGB(0, 0, 0);
  }

  mBackgroundAttachment = NS_STYLE_BG_ATTACHMENT_SCROLL;
  mBackgroundFlags = NS_STYLE_BG_COLOR_TRANSPARENT;
  mBackgroundRepeat = NS_STYLE_BG_REPEAT_XY;
  mBackgroundColor = NS_RGB(192,192,192);
  mBackgroundXPosition = 0;
  mBackgroundYPosition = 0;

  mCursor = NS_STYLE_CURSOR_INHERIT;
}


// --------------------
// nsStyleSpacing
//
nsStyleSpacing::nsStyleSpacing(void) { }

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
      aFrame->GetAutoMarginSize(aSide, result);
      break;

    case eStyleUnit_Inherit:
      nsIFrame* parentFrame;
      aFrame->GetGeometricParent(parentFrame);  // XXX may not be direct parent...
      if (nsnull != parentFrame) {
        nsIStyleContext* parentContext;
        parentFrame->GetStyleContext(nsnull, parentContext);
        if (nsnull != parentContext) {
          nsStyleSpacing* parentSpacing = (nsStyleSpacing*)parentContext->GetData(eStyleStruct_Spacing);
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
      {
        nscoord baseWidth = 0;
        PRBool  isBase = PR_FALSE;
        nsIFrame* frame;
        aFrame->GetGeometricParent(frame);
        while (nsnull != frame) {
          frame->IsPercentageBase(isBase);
          if (isBase) {
            nsSize  size;
            frame->GetSize(size);
            baseWidth = size.width; // not really width, need to subtract out padding...
            break;
          }
          frame->GetGeometricParent(frame);
        }
        result = (nscoord)((float)baseWidth * aCoord.GetPercentValue());
      }
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

  void ResetFrom(const nsStyleSpacing* aParent, nsIPresContext* aPresContext);
  void RecalcData(nsIPresContext* aPresContext);
};

void StyleSpacingImpl::ResetFrom(const nsStyleSpacing* aParent, nsIPresContext* aPresContext)
{
  // spacing values not inherited
  mMargin.Reset();
  mPadding.Reset();
  mBorder.Reset();
  mBorderStyle[0] = NS_STYLE_BORDER_STYLE_NONE;
  mBorderStyle[1] = NS_STYLE_BORDER_STYLE_NONE;
  mBorderStyle[2] = NS_STYLE_BORDER_STYLE_NONE;
  mBorderStyle[3] = NS_STYLE_BORDER_STYLE_NONE;
  mBorderColor[0] = NS_RGB(0, 0, 0);
  mBorderColor[1] = NS_RGB(0, 0, 0);
  mBorderColor[2] = NS_RGB(0, 0, 0);
  mBorderColor[3] = NS_RGB(0, 0, 0);

  mHasCachedMargin = PR_FALSE;
  mHasCachedPadding = PR_FALSE;
  mHasCachedBorder = PR_FALSE;
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
  StyleListImpl(void) { }

  void ResetFrom(const nsStyleList* aParent, nsIPresContext* aPresContext);
};

void StyleListImpl::ResetFrom(const nsStyleList* aParent, nsIPresContext* aPresContext)
{
  if (nsnull != aParent) {
    mListStyleType = aParent->mListStyleType;
    mListStyleImage = aParent->mListStyleImage;
    mListStylePosition = aParent->mListStylePosition;
  }
  else {
    mListStyleType = NS_STYLE_LIST_STYLE_BASIC;
    mListStylePosition = NS_STYLE_LIST_STYLE_POSITION_OUTSIDE;
    mListStyleImage.Truncate();
  }
}

// --------------------
// nsStylePosition
//
nsStylePosition::nsStylePosition(void) { }

struct StylePositionImpl: public nsStylePosition {
  StylePositionImpl(void) { }

  void ResetFrom(const nsStylePosition* aParent, nsIPresContext* aPresContext);

private:  // These are not allowed
  StylePositionImpl(const StylePositionImpl& aOther);
  StylePositionImpl& operator=(const StylePositionImpl& aOther);
};

void StylePositionImpl::ResetFrom(const nsStylePosition* aParent, nsIPresContext* aPresContext)
{
  // positioning values not inherited
  mPosition = NS_STYLE_POSITION_NORMAL;
  mLeftOffset.SetAutoValue();
  mTopOffset.SetAutoValue();
  mWidth.SetAutoValue();
  mHeight.SetAutoValue();
  mZIndex.SetAutoValue();
}

// --------------------
// nsStyleText
//

nsStyleText::nsStyleText(void) { }

struct StyleTextImpl: public nsStyleText {
  StyleTextImpl(void) { }

  void ResetFrom(const nsStyleText* aParent, nsIPresContext* aPresContext);
};

void StyleTextImpl::ResetFrom(const nsStyleText* aParent, nsIPresContext* aPresContext)
{
  // These properties not inherited
  mTextDecoration = NS_STYLE_TEXT_DECORATION_NONE;
  mVerticalAlign.SetIntValue(NS_STYLE_VERTICAL_ALIGN_BASELINE, eStyleUnit_Enumerated);

  if (nsnull != aParent) {
    mTextAlign = aParent->mTextAlign;
    mTextTransform = aParent->mTextTransform;
    mWhiteSpace = aParent->mWhiteSpace;

    mLetterSpacing = aParent->mLetterSpacing;
    mLineHeight = aParent->mLineHeight;
    mTextIndent = aParent->mTextIndent;
    mWordSpacing = aParent->mWordSpacing;
  }
  else {
    mTextAlign = NS_STYLE_TEXT_ALIGN_LEFT;
    mTextTransform = NS_STYLE_TEXT_TRANSFORM_NONE;
    mWhiteSpace = NS_STYLE_WHITESPACE_NORMAL;

    mLetterSpacing.SetNormalValue();
    mLineHeight.SetNormalValue();
    mTextIndent.SetCoordValue(0);
    mWordSpacing.SetNormalValue();
  }

}

// --------------------
// nsStyleDisplay
//

nsStyleDisplay::nsStyleDisplay(void) { }

struct StyleDisplayImpl: public nsStyleDisplay {
  StyleDisplayImpl(void) { }

  void ResetFrom(const nsStyleDisplay* aParent, nsIPresContext* aPresContext);
};

void StyleDisplayImpl::ResetFrom(const nsStyleDisplay* aParent, nsIPresContext* aPresContext)
{
  if (nsnull != aParent) {
    mDirection = aParent->mDirection;
    mVisible = aParent->mVisible;
  }
  else {
    mDirection = NS_STYLE_DIRECTION_LTR;
    mVisible = PR_TRUE;
  }
  mDisplay = NS_STYLE_DISPLAY_INLINE;
  mFloats = NS_STYLE_FLOAT_NONE;
  mBreakType = NS_STYLE_CLEAR_NONE;
  mBreakBefore = PR_FALSE;
  mBreakAfter = PR_FALSE;
  mOverflow = NS_STYLE_OVERFLOW_VISIBLE;
  mClipFlags = NS_STYLE_CLIP_AUTO;
  mClip.SizeTo(0,0,0,0);
}

// --------------------
// nsStyleTable
//

nsStyleTable::nsStyleTable(void) { }

struct StyleTableImpl: public nsStyleTable {
  StyleTableImpl(void) { }

  void ResetFrom(const nsStyleTable* aParent, nsIPresContext* aPresContext);
};

void StyleTableImpl::ResetFrom(const nsStyleTable* aParent, nsIPresContext* aPresContext)
{
  // values not inherited
  mFrame = NS_STYLE_TABLE_FRAME_NONE;
  mRules = NS_STYLE_TABLE_RULES_NONE;
  mCellPadding.Reset();
  mCellSpacing.Reset();
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
  virtual PRInt32 GetStyleRuleCount(void) const;

  virtual nsIStyleContext* FindChildWithRules(nsISupportsArray* aRules);

  virtual PRBool    Equals(const nsIStyleContext* aOther) const;
  virtual PRUint32  HashValue(void) const;

  virtual void RemapStyle(nsIPresContext* aPresContext);

  virtual nsStyleStruct* GetData(nsStyleStructID aSID);

  virtual void RecalcAutomaticData(nsIPresContext* aPresContext);

  virtual void  List(FILE* out, PRInt32 aIndent);

protected:
  void AddChild(StyleContextImpl* aChild);

  StyleContextImpl* mParent;
  StyleContextImpl* mChild;
  StyleContextImpl* mPrev;
  StyleContextImpl* mNext;

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

#ifdef DEBUG_REFS
  PRInt32 mInstance;
#endif
};

#ifdef DEBUG_REFS
static PRInt32 gInstanceCount;
static PRInt32 gInstrument = 6;
#endif

StyleContextImpl::StyleContextImpl(nsIStyleContext* aParent,
                                   nsISupportsArray* aRules, 
                                   nsIPresContext* aPresContext)
  : mParent((StyleContextImpl*)aParent), // weak ref
    mChild(nsnull),
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

  mNext = this;
  mPrev = this;
  if (nsnull != mParent) {
    mParent->AddChild(this);
  }

  RemapStyle(aPresContext);

#ifdef DEBUG_REFS
  mInstance = ++gInstanceCount;
  fprintf(stdout, "%d of %d + StyleContext\n", mInstance, gInstanceCount);
#endif
}

StyleContextImpl::~StyleContextImpl()
{
  mParent = nsnull; // weak ref

  if (nsnull != mChild) {
    StyleContextImpl* child = mChild;
    do {
      StyleContextImpl* goner = child;
      child = child->mNext;
      NS_RELEASE(goner);
    } while (child != mChild);
    mChild = nsnull;
  }

  NS_IF_RELEASE(mRules);
  if (nsnull != mTable) {
    delete mTable;
    mTable = nsnull;
  }

#ifdef DEBUG_REFS
  fprintf(stdout, "%d of %d - StyleContext\n", mInstance, gInstanceCount);
  --gInstanceCount;
#endif
}



#ifdef DEBUG_REFS
NS_IMPL_QUERY_INTERFACE(StyleContextImpl, kIStyleContextIID)

nsrefcnt StyleContextImpl::AddRef(void)                                
{                                    
  if ((gInstrument == -1) || (mInstance == gInstrument)) {
    fprintf(stdout, "%d AddRef StyleContext %d\n", mRefCnt + 1, mInstance);
  }
  return ++mRefCnt;                                          
}

nsrefcnt StyleContextImpl::Release(void)                         
{                                                      
  if ((gInstrument == -1) || (mInstance == gInstrument)) {
    fprintf(stdout, "%d Release StyleContext %d\n", mRefCnt - 1, mInstance);
  }
  if (--mRefCnt == 0) {                                
    delete this;                                       
    return 0;                                          
  }                                                    
  return mRefCnt;                                      
}
#else
NS_IMPL_ISUPPORTS(StyleContextImpl, kIStyleContextIID)
#endif



nsIStyleContext* StyleContextImpl::GetParent(void) const
{
  NS_IF_ADDREF(mParent);
  return mParent;
}

void StyleContextImpl::AddChild(StyleContextImpl* aChild)
{
  if (nsnull == mChild) {
    mChild = aChild;
  }
  else {
    aChild->mNext = mChild;
    aChild->mPrev = mChild->mPrev;
    mChild->mPrev->mNext = aChild;
    mChild->mPrev = aChild;
  }
  NS_ADDREF(aChild);
}

nsISupportsArray* StyleContextImpl::GetStyleRules(void) const
{
  NS_IF_ADDREF(mRules);
  return mRules;
}

PRInt32 StyleContextImpl::GetStyleRuleCount(void) const
{
  if (nsnull != mRules) {
    return mRules->Count();
  }
  return 0;
}

nsIStyleContext* StyleContextImpl::FindChildWithRules(nsISupportsArray* aRules)
{
  nsIStyleContext* result = nsnull;

  if (nsnull != mChild) {
    StyleContextImpl* child = mChild;
    PRInt32 ruleCount = aRules->Count();
    do {
      if (child->GetStyleRuleCount() == ruleCount) {
        if (child->mRules->Equals(aRules)) {
          result = child;
          NS_ADDREF(result);
          break;
        }
      }
      child = child->mNext;
    } while (child != mChild);
  }
  return result;
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


nsStyleStruct* StyleContextImpl::GetData(nsStyleStructID aSID)
{
  nsStyleStruct*  result = nsnull;

  switch (aSID) {
    case eStyleStruct_Font:
      result = &mFont;
      break;
    case eStyleStruct_Color:
      result = &mColor;
      break;
    case eStyleStruct_Spacing:
      result = &mSpacing;
      break;
    case eStyleStruct_List:
      result = &mList;
      break;
    case eStyleStruct_Position:
      result = &mPosition;
      break;
    case eStyleStruct_Text:
      result = &mText;
      break;
    case eStyleStruct_Display:
      result = &mDisplay;
      break;
    case eStyleStruct_Table:  // this one gets created lazily
      if (nsnull == mTable) {
        mTable = new StyleTableImpl();
        if (nsnull != mParent) {
          if (nsnull != mParent->mTable) {
            mTable->ResetFrom(mParent->mTable, nsnull);
          }
        }
      }
      result = mTable;
      break;
    default:
      NS_ERROR("Invalid style struct id");
      break;
  }
  return result;
}

struct MapStyleData {
  MapStyleData(nsIStyleContext* aStyleContext, nsIPresContext* aPresContext)
  {
    mStyleContext = aStyleContext;
    mPresContext = aPresContext;
  }
  nsIStyleContext*  mStyleContext;
  nsIPresContext*   mPresContext;
};

PRBool MapStyleRule(nsISupports* aRule, void* aData)
{
  nsIStyleRule* rule = (nsIStyleRule*)aRule;
  MapStyleData* data = (MapStyleData*)aData;
  rule->MapStyleInto(data->mStyleContext, data->mPresContext);
  return PR_TRUE;
}

void StyleContextImpl::RemapStyle(nsIPresContext* aPresContext)
{
  if (nsnull != mParent) {
    mFont.ResetFrom(&(mParent->mFont), aPresContext);
    mColor.ResetFrom(&(mParent->mColor), aPresContext);
    mSpacing.ResetFrom(&(mParent->mSpacing), aPresContext);
    mList.ResetFrom(&(mParent->mList), aPresContext);
    mText.ResetFrom(&(mParent->mText), aPresContext);
    mPosition.ResetFrom(&(mParent->mPosition), aPresContext);
    mDisplay.ResetFrom(&(mParent->mDisplay), aPresContext);
  }
  else {
    mFont.ResetFrom(nsnull, aPresContext);
    mColor.ResetFrom(nsnull, aPresContext);
    mSpacing.ResetFrom(nsnull, aPresContext);
    mList.ResetFrom(nsnull, aPresContext);
    mText.ResetFrom(nsnull, aPresContext);
    mPosition.ResetFrom(nsnull, aPresContext);
    mDisplay.ResetFrom(nsnull, aPresContext);
  }

  if ((nsnull != mRules) && (0 < mRules->Count())) {
    MapStyleData  data(this, aPresContext);
    mRules->EnumerateBackwards(MapStyleRule, &data);
  }
}

void StyleContextImpl::RecalcAutomaticData(nsIPresContext* aPresContext)
{
  mSpacing.RecalcData(aPresContext);
}

void StyleContextImpl::List(FILE* out, PRInt32 aIndent)
{
  // Indent
  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);
  PRInt32 count = mRules->Count();
  if (0 < count) {
    fputs("{\n", out);

    for (index = 0; index < count; index++) {
      nsIStyleRule* rule = (nsIStyleRule*)mRules->ElementAt(index);
      rule->List(out, aIndent + 1);
      NS_RELEASE(rule);
    }

    for (index = aIndent; --index >= 0; ) fputs("  ", out);
    fputs("}\n", out);
  }
  else {
    fputs("{}\n", out);
  }

}

NS_LAYOUT nsresult
NS_NewStyleContext(nsIStyleContext** aInstancePtrResult,
                   nsIStyleContext* aParentContext,
                   nsISupportsArray* aRules,
                   nsIPresContext* aPresContext)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  StyleContextImpl* context = new StyleContextImpl(aParentContext, aRules, aPresContext);
  if (nsnull == context) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  context->RecalcAutomaticData(aPresContext);

  return context->QueryInterface(kIStyleContextIID, (void **) aInstancePtrResult);
}
