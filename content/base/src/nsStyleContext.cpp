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
static NS_DEFINE_IID(kStyleMoleculeSID, NS_STYLEMOLECULE_SID);

static NS_DEFINE_IID(kIStyleContextIID, NS_ISTYLECONTEXT_IID);


// --------------------
// nsStyleFont
//
nsStyleFont::nsStyleFont(const nsFont& aFont)
  : mFont(aFont)
{
}

nsStyleFont::~nsStyleFont(void)
{
}

struct StyleFontImpl : public nsStyleFont {
  StyleFontImpl(const nsFont& aFont)
    : nsStyleFont(aFont)
  {}
  ~StyleFontImpl()  
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
struct StyleColorImpl: public nsStyleColor {
  StyleColorImpl(void)
  {
    mBackgroundAttachment = NS_STYLE_BG_ATTACHMENT_SCROLL;
    mBackgroundFlags = NS_STYLE_BG_COLOR_TRANSPARENT;
    mBackgroundRepeat = NS_STYLE_BG_REPEAT_OFF;

    mBackgroundColor = NS_RGB(192,192,192);
  }

  ~StyleColorImpl(void)
  {}

  virtual const nsID& GetID(void)
  { return kStyleColorSID;  }

  virtual void InheritFrom(const nsStyleColor& aCopy);

private:  // These are not allowed
  StyleColorImpl(const StyleColorImpl& aOther);
  StyleColorImpl& operator=(const StyleColorImpl& aOther);
};

void StyleColorImpl::InheritFrom(const nsStyleColor& aCopy)
{
  mColor  = aCopy.mColor;

  mBackgroundFlags = NS_STYLE_BG_COLOR_TRANSPARENT;
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

nsStyleSpacing::~nsStyleSpacing(void)
{
}

struct StyleSpacingImpl: public nsStyleSpacing {
  StyleSpacingImpl(void)
    : nsStyleSpacing()
  {}

  ~StyleSpacingImpl(void)
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

nsStyleBorder::~nsStyleBorder(void)
{
}

struct StyleBorderImpl: public nsStyleBorder {
  StyleBorderImpl(void)
    : nsStyleBorder()
  {}

  ~StyleBorderImpl(void)
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
struct StyleListImpl: public nsStyleList {
  StyleListImpl(void)
  {
    mListStyleType = NS_STYLE_LIST_STYLE_BASIC;
    mListStylePosition = NS_STYLE_LIST_STYLE_POSITION_OUTSIDE;
  }

  ~StyleListImpl(void)
  {
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
// nsStyleMolecule
//
nsStyleMolecule::nsStyleMolecule()
{
}

nsStyleMolecule::~nsStyleMolecule()
{
}

struct StyleMoleculeImpl : public nsStyleMolecule {
  StyleMoleculeImpl(void)
  {}
  ~StyleMoleculeImpl(void)
  {}

  virtual const nsID& GetID(void)
  { return kStyleMoleculeSID;  }

  virtual void InheritFrom(const nsStyleMolecule& aCopy);

private:  // These are not allowed
  StyleMoleculeImpl(const StyleMoleculeImpl& aOther);
  StyleMoleculeImpl& operator=(const StyleMoleculeImpl& aOther);
};

void StyleMoleculeImpl::InheritFrom(const nsStyleMolecule& aCopy)
{
  cursor            = aCopy.cursor;
  direction         = aCopy.direction;

  textDecoration    = aCopy.textDecoration;

  textAlign         = aCopy.textAlign;
  whiteSpace        = aCopy.whiteSpace;

//  lineHeight        = aCopy.lineHeight;
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
  virtual void PostProcessData(void);

  virtual void HackStyleFor(nsIPresContext* aPresContext,
                            nsIContent* aContent,
                            nsIFrame* aFrame);

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
// xxx backward support hack
  StyleMoleculeImpl mMolecule;
};

#ifdef DEBUG_REFS
static PRInt32 gInstanceCount;
#endif

StyleContextImpl::StyleContextImpl(nsIStyleContext* aParent, nsISupportsArray* aRules, 
                                   nsIPresContext* aPresContext)
  : mParent(aParent), // weak ref
    mRules(aRules),
    mFont(aPresContext->GetDefaultFont()),
    mColor(),
    mSpacing(),
    mBorder(),
    mList(),
    mMolecule()
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
  if (aSID.Equals(kStyleMoleculeSID)) {
    return &mMolecule;
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
  mMolecule.InheritFrom(aParent.mMolecule);
}

static void CalcBorderSize(nscoord& aSize, PRUint8 aFlag)
{
  static const nscoord kBorderSize[3] = // XXX need real numbers here (this is probably wrong anyway)
      { NS_POINTS_TO_TWIPS_INT(1), 
        NS_POINTS_TO_TWIPS_INT(3), 
        NS_POINTS_TO_TWIPS_INT(5) };
  if (aFlag < NS_STYLE_BORDER_WIDTH_LENGTH_VALUE) {
    aSize = kBorderSize[aFlag];
  }
}

void StyleContextImpl::PostProcessData(void)
{
  CalcBorderSize(mBorder.mSize.top, mBorder.mSizeFlag[NS_SIDE_TOP]);
  CalcBorderSize(mBorder.mSize.right, mBorder.mSizeFlag[NS_SIDE_RIGHT]);
  CalcBorderSize(mBorder.mSize.bottom, mBorder.mSizeFlag[NS_SIDE_BOTTOM]);
  CalcBorderSize(mBorder.mSize.left, mBorder.mSizeFlag[NS_SIDE_LEFT]);

  mSpacing.mBorder = mBorder.mSize;
  mSpacing.mBorderPadding = mSpacing.mPadding;
  mSpacing.mBorderPadding += mBorder.mSize;

  // XXX hack fill in molecule
  mMolecule.border = mBorder.mSize;
  mMolecule.margin = mSpacing.mMargin;
  mMolecule.borderPadding = mSpacing.mBorderPadding;
}

void StyleContextImpl::HackStyleFor(nsIPresContext* aPresContext,
                                    nsIContent* aContent,
                                    nsIFrame* aParentFrame)
{

  mMolecule.display = NS_STYLE_DISPLAY_BLOCK;
  mMolecule.whiteSpace = NS_STYLE_WHITESPACE_NORMAL;
  mMolecule.verticalAlign = NS_STYLE_VERTICAL_ALIGN_BASELINE;
  mMolecule.textAlign = NS_STYLE_TEXT_ALIGN_LEFT;

  mMolecule.positionFlags = NS_STYLE_POSITION_STATIC;
  mMolecule.floats = 0;

  // XXX If it's a B guy then make it inline
  nsIAtom* tag = aContent->GetTag();
  nsAutoString buf;
  if (tag != nsnull) {
    tag->ToString(buf);
    NS_RELEASE(tag);
    if (buf.EqualsIgnoreCase("B")) {
//      float p2t = aPresContext->GetPixelsToTwips();
      mMolecule.display = NS_STYLE_DISPLAY_INLINE;
    } else if (buf.EqualsIgnoreCase("A")) {
      mMolecule.display = NS_STYLE_DISPLAY_INLINE;
      mMolecule.cursor = NS_STYLE_CURSOR_HAND;
    } else if (buf.EqualsIgnoreCase("BR")) {
      mMolecule.display = NS_STYLE_DISPLAY_INLINE;
      nsString  align("CLEAR");
      nsString  value;
      if (eContentAttr_HasValue == aContent->GetAttribute(align, value)) {
        if (0 == value.Compare("left", PR_TRUE)) {
          mMolecule.clear = NS_STYLE_CLEAR_LEFT;
        } else if (0 == value.Compare("right", PR_TRUE)) {
          mMolecule.clear = NS_STYLE_CLEAR_RIGHT;
        } else if (0 == value.Compare("all", PR_TRUE)) {
          mMolecule.clear = NS_STYLE_CLEAR_BOTH;
        }
      }
    } else if (buf.EqualsIgnoreCase("SPACER")) {
      mMolecule.display = NS_STYLE_DISPLAY_INLINE;
    } else if (buf.EqualsIgnoreCase("WBR")) {
      mMolecule.display = NS_STYLE_DISPLAY_INLINE;
    } else if (buf.EqualsIgnoreCase("INPUT")) {
      mMolecule.display = NS_STYLE_DISPLAY_INLINE;
    } else if (buf.EqualsIgnoreCase("I")) {
      mMolecule.display = NS_STYLE_DISPLAY_INLINE;
    } else if (buf.EqualsIgnoreCase("S")) {
      mMolecule.display = NS_STYLE_DISPLAY_INLINE;
    } else if (buf.EqualsIgnoreCase("PRE")) {
      mMolecule.whiteSpace = NS_STYLE_WHITESPACE_PRE;
      mSpacing.mMargin.top = NS_POINTS_TO_TWIPS_INT(3);
      mSpacing.mMargin.bottom = NS_POINTS_TO_TWIPS_INT(3);
    } else if (buf.EqualsIgnoreCase("U")) {
      mMolecule.display = NS_STYLE_DISPLAY_INLINE;
    } else if (buf.EqualsIgnoreCase("FONT")) {
      mMolecule.display = NS_STYLE_DISPLAY_INLINE;
    } else if (buf.EqualsIgnoreCase("THREED")) {
      mMolecule.display = NS_STYLE_DISPLAY_INLINE;
      mFont.mThreeD = 1;
    } else if (buf.EqualsIgnoreCase("TT")) {
      mMolecule.display = NS_STYLE_DISPLAY_INLINE;
    } else if (buf.EqualsIgnoreCase("IMG")) {
      float p2t = aPresContext->GetPixelsToTwips();
      mMolecule.display = NS_STYLE_DISPLAY_INLINE;
      mSpacing.mPadding.top = nscoord(2 * p2t);
      mSpacing.mPadding.right = nscoord(2 * p2t);
      mSpacing.mPadding.bottom = nscoord(2 * p2t);
      mSpacing.mPadding.left = nscoord(2 * p2t);
      nsString  align("ALIGN");
      nsString  value;
      if (eContentAttr_HasValue == aContent->GetAttribute(align, value)) {
        if (0 == value.Compare("left", PR_TRUE)) {
          mMolecule.floats = NS_STYLE_FLOAT_LEFT;
        } else if (0 == value.Compare("right", PR_TRUE)) {
          mMolecule.floats = NS_STYLE_FLOAT_RIGHT;
        }
      }
    } else if (buf.EqualsIgnoreCase("P")) {
      nsString align("align");
      nsString value;
      if (eContentAttr_NotThere != aContent->GetAttribute(align, value)) {
        if (0 == value.Compare("center", PR_TRUE)) {
          mMolecule.textAlign = NS_STYLE_TEXT_ALIGN_CENTER;
        }
        if (0 == value.Compare("right", PR_TRUE)) {
          mMolecule.textAlign = NS_STYLE_TEXT_ALIGN_RIGHT;
        }
      }
//      mSpacing.mMargin.top = NS_POINTS_TO_TWIPS_INT(2);
//      mSpacing.mMargin.bottom = NS_POINTS_TO_TWIPS_INT(2);
    } else if (buf.EqualsIgnoreCase("BODY")) {
      float p2t = aPresContext->GetPixelsToTwips();
      mSpacing.mPadding.top = nscoord(5 * p2t);
      mSpacing.mPadding.right = nscoord(5 * p2t);
      mSpacing.mPadding.bottom = nscoord(5 * p2t);
      mSpacing.mPadding.left = nscoord(5 * p2t);
      mBorder.mSize.top = nscoord(1 * p2t);
      mBorder.mSize.right = nscoord(1 * p2t);
      mBorder.mSize.bottom = nscoord(1 * p2t);
      mBorder.mSize.left = nscoord(1 * p2t);
      for (int i = 0; i < 4; i++) {
        mBorder.mStyle[i] = NS_STYLE_BORDER_STYLE_SOLID;
        mBorder.mColor[i] = NS_RGB(0, 255, 0);
      }
    } else if (buf.EqualsIgnoreCase("LI")) {
      mMolecule.display = NS_STYLE_DISPLAY_LIST_ITEM;
    } else if (buf.EqualsIgnoreCase("UL") || buf.EqualsIgnoreCase("OL")) {
      float p2t = aPresContext->GetPixelsToTwips();
      mSpacing.mPadding.left = nscoord(40 * p2t);
      mSpacing.mMargin.top = NS_POINTS_TO_TWIPS_INT(5);
      mSpacing.mMargin.bottom = NS_POINTS_TO_TWIPS_INT(5);
    } else if (buf.EqualsIgnoreCase("TABLE")) {                     // TABLE
      mBorder.mSize.top = NS_POINTS_TO_TWIPS_INT(1);
      mBorder.mSize.bottom = NS_POINTS_TO_TWIPS_INT(1);
      mBorder.mSize.right = NS_POINTS_TO_TWIPS_INT(1);
      mBorder.mSize.left = NS_POINTS_TO_TWIPS_INT(1);
      mBorder.mStyle[0] = mBorder.mStyle[1] =
      mBorder.mStyle[2] = mBorder.mStyle[3] = NS_STYLE_BORDER_STYLE_SOLID;
      mMolecule.fixedWidth = -1;
      mMolecule.proportionalWidth = -1;
      nsString  align("ALIGN");
      nsString  value;
      if (eContentAttr_HasValue == aContent->GetAttribute(align, value)) {
        if (0 == value.Compare("left", PR_TRUE)) {
          mMolecule.floats = NS_STYLE_FLOAT_LEFT;
        } else if (0 == value.Compare("right", PR_TRUE)) {
          mMolecule.floats = NS_STYLE_FLOAT_RIGHT;
        }
      }
    } else if (buf.EqualsIgnoreCase("CAPTION")) {                   // CAPTION
      mMolecule.verticalAlign = NS_STYLE_VERTICAL_ALIGN_TOP;
    } else if (buf.EqualsIgnoreCase("TBODY")) {                     // TBODY
      mSpacing.mPadding.top = NS_POINTS_TO_TWIPS_INT(0);
      mSpacing.mPadding.bottom = NS_POINTS_TO_TWIPS_INT(0);
      mSpacing.mPadding.right = NS_POINTS_TO_TWIPS_INT(0);
      mSpacing.mPadding.left = NS_POINTS_TO_TWIPS_INT(0);
    } else if (buf.EqualsIgnoreCase("TR")) {                        // TROW
      mSpacing.mPadding.top = NS_POINTS_TO_TWIPS_INT(0);
      mSpacing.mPadding.bottom = NS_POINTS_TO_TWIPS_INT(0);
      mSpacing.mPadding.right = NS_POINTS_TO_TWIPS_INT(0);
      mSpacing.mPadding.left = NS_POINTS_TO_TWIPS_INT(0);
    } else if (buf.EqualsIgnoreCase("TD")) {                        // TD
      float p2t = aPresContext->GetPixelsToTwips();
      
      
      // Set padding to twenty for testing purposes
      int cellPadding = 1;
      if (gsDebug==PR_TRUE)
        cellPadding = 20;
      mMolecule.verticalAlign = NS_STYLE_VERTICAL_ALIGN_MIDDLE;
        
      mSpacing.mPadding.top = NS_POINTS_TO_TWIPS_INT(cellPadding);
      mSpacing.mPadding.bottom = NS_POINTS_TO_TWIPS_INT(cellPadding);
      mSpacing.mPadding.right = NS_POINTS_TO_TWIPS_INT(cellPadding);
      mSpacing.mPadding.left = NS_POINTS_TO_TWIPS_INT(cellPadding);
      mBorder.mSize.top = nscoord(1 * p2t);
      mBorder.mSize.right = nscoord(1 * p2t);
      mBorder.mSize.bottom = nscoord(1 * p2t);
      mBorder.mSize.left = nscoord(1 * p2t);
      for (int i = 0; i < 4; i++) {
        mBorder.mStyle[i] = NS_STYLE_BORDER_STYLE_SOLID;
        mBorder.mColor[i] = NS_RGB(128, 128, 128);
      }
      mMolecule.fixedWidth = -1;
      mMolecule.proportionalWidth = -1;
      mMolecule.fixedHeight = -1;
      mMolecule.proportionalHeight = -1;

    } else if (buf.EqualsIgnoreCase("COL")) {
      mMolecule.fixedWidth = -1;
      mMolecule.proportionalWidth = -1;
      mMolecule.fixedHeight = -1;
      mMolecule.proportionalHeight = -1;
    }
  } else {
    // It's text (!)
    mMolecule.display = NS_STYLE_DISPLAY_INLINE;
    mMolecule.cursor = NS_STYLE_CURSOR_IBEAM;
    nsIContent* content;
     
    aParentFrame->GetContent(content);
    nsIAtom* parentTag = content->GetTag();
    parentTag->ToString(buf);
    NS_RELEASE(content);
    NS_RELEASE(parentTag);
    if (buf.EqualsIgnoreCase("B")) {
    } else if (buf.EqualsIgnoreCase("A")) {
      // This simulates a <PRE><A>text inheritance rule
      // Check the parent of the A
      nsIFrame* parentParentFrame;
       
      aParentFrame->GetGeometricParent(parentParentFrame);
      if (nsnull != parentParentFrame) {
        nsIContent* parentParentContent;
         
        parentParentFrame->GetContent(parentParentContent);
        nsIAtom* parentParentTag = parentParentContent->GetTag();
        parentParentTag->ToString(buf);
        NS_RELEASE(parentParentTag);
        NS_RELEASE(parentParentContent);
        if (buf.EqualsIgnoreCase("PRE")) {
          mMolecule.whiteSpace = NS_STYLE_WHITESPACE_PRE;
        }
      }
    } else if (buf.EqualsIgnoreCase("THREED")) {
      mFont.mThreeD = 1;
    } else if (buf.EqualsIgnoreCase("I")) {
    } else if (buf.EqualsIgnoreCase("BLINK")) {
    } else if (buf.EqualsIgnoreCase("TT")) {
    } else if (buf.EqualsIgnoreCase("S")) {
    } else if (buf.EqualsIgnoreCase("U")) {
    } else if (buf.EqualsIgnoreCase("PRE")) {
      mMolecule.whiteSpace = NS_STYLE_WHITESPACE_PRE;
    }
  }

#if 0
  if ((NS_STYLE_DISPLAY_BLOCK == mMolecule.display) ||
      (NS_STYLE_DISPLAY_LIST_ITEM == mMolecule.display)) {
    // Always justify text (take that ie)
    mMolecule.textAlign = NS_STYLE_TEXT_ALIGN_JUSTIFY;
  }
#endif


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
  context->HackStyleFor(aPresContext, aContent, aParentFrame);
  context->PostProcessData();

  return context->QueryInterface(kIStyleContextIID, (void **) aInstancePtrResult);
}
