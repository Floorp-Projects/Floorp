/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   Rob Arnold <robarnold@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * structs that contain the data provided by nsStyleContext, the
 * internal API for computed style data for an element
 */

#ifndef nsStyleStruct_h___
#define nsStyleStruct_h___

#include "nsColor.h"
#include "nsCoord.h"
#include "nsMargin.h"
#include "nsRect.h"
#include "nsFont.h"
#include "nsVoidArray.h"
#include "nsStyleCoord.h"
#include "nsStyleConsts.h"
#include "nsChangeHint.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIAtom.h"
#include "nsIURI.h"
#include "nsCSSValue.h"
#include "nsStyleTransformMatrix.h"

class nsIFrame;
class imgIRequest;

// Includes nsStyleStructID.
#include "nsStyleStructFwd.h"

// Bits for each struct.
// NS_STYLE_INHERIT_BIT defined in nsStyleStructFwd.h
#define NS_STYLE_INHERIT_MASK             0x00ffffff

// Additional bits for nsStyleContext's mBits:
// A bit to test whether or not we have any text decorations.
#define NS_STYLE_HAS_TEXT_DECORATIONS     0x01000000

// Additional bits for nsRuleNode's mDependentBits:
#define NS_RULE_NODE_GC_MARK              0x02000000
#define NS_RULE_NODE_IS_IMPORTANT         0x08000000
#define NS_RULE_NODE_LEVEL_MASK           0xf0000000
#define NS_RULE_NODE_LEVEL_SHIFT          28

// The lifetime of these objects is managed by the presshell's arena.

struct nsStyleFont {
  nsStyleFont(const nsFont& aFont, nsPresContext *aPresContext);
  nsStyleFont(const nsStyleFont& aStyleFont);
  nsStyleFont(nsPresContext *aPresContext);
  ~nsStyleFont(void) {}

  nsChangeHint CalcDifference(const nsStyleFont& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif
  static nsChangeHint CalcFontDifference(const nsFont& aFont1, const nsFont& aFont2);

  static nscoord ZoomText(nsPresContext* aPresContext, nscoord aSize);
  static nscoord UnZoomText(nsPresContext* aPresContext, nscoord aSize);

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW;
  void Destroy(nsPresContext* aContext);

  nsFont  mFont;        // [inherited]
  nscoord mSize;        // [inherited] Our "computed size". Can be different
                        // from mFont.size which is our "actual size" and is
                        // enforced to be >= the user's preferred min-size.
                        // mFont.size should be used for display purposes
                        // while mSize is the value to return in
                        // getComputedStyle() for example.
  PRUint8 mGenericID;   // [inherited] generic CSS font family, if any;
                        // value is a kGenericFont_* constant, see nsFont.h.

#ifdef MOZ_MATHML
  // MathML scriptlevel support
  PRInt8  mScriptLevel;          // [inherited]
  // The value mSize would have had if scriptminsize had never been applied
  nscoord mScriptUnconstrainedSize;
  nscoord mScriptMinSize;        // [inherited] length
  float   mScriptSizeMultiplier; // [inherited]
#endif
};

struct nsStyleColor {
  nsStyleColor(nsPresContext* aPresContext);
  nsStyleColor(const nsStyleColor& aOther);
  ~nsStyleColor(void) {}

  nsChangeHint CalcDifference(const nsStyleColor& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif
  
  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleColor();
    aContext->FreeToShell(sizeof(nsStyleColor), this);
  }

  // Don't add ANY members to this struct!  We can achieve caching in the rule
  // tree (rather than the style tree) by letting color stay by itself! -dwh
  nscolor mColor;                 // [inherited]
};

struct nsStyleBackground {
  nsStyleBackground();
  nsStyleBackground(const nsStyleBackground& aOther);
  ~nsStyleBackground();

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleBackground();
    aContext->FreeToShell(sizeof(nsStyleBackground), this);
  }

  nsChangeHint CalcDifference(const nsStyleBackground& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif

  PRUint8 mBackgroundFlags;        // [reset] See nsStyleConsts.h
  PRUint8 mBackgroundAttachment;   // [reset] See nsStyleConsts.h
  PRUint8 mBackgroundClip;         // [reset] See nsStyleConsts.h
  PRUint8 mBackgroundInlinePolicy; // [reset] See nsStyleConsts.h
  PRUint8 mBackgroundOrigin;       // [reset] See nsStyleConsts.h
  PRUint8 mBackgroundRepeat;       // [reset] See nsStyleConsts.h

  // Note: a member of this union is valid IFF the appropriate bit flag
  // is set in mBackgroundFlags.
  union {
    nscoord mCoord;
    float   mFloat;
  } mBackgroundXPosition,         // [reset]
    mBackgroundYPosition;         // [reset]

  nscolor mBackgroundColor;       // [reset]
  nsCOMPtr<imgIRequest> mBackgroundImage; // [reset]

  // True if this background is completely transparent.
  PRBool IsTransparent() const
  {
    return (NS_GET_A(mBackgroundColor) == 0 &&
            (mBackgroundFlags & NS_STYLE_BG_IMAGE_NONE));
  }

  // We have to take slower codepaths for fixed background attachment,
  // but we don't want to do that when there's no image.
  // Not inline because it uses an nsCOMPtr<imgIRequest>
  // FIXME: Should be in nsStyleStructInlines.h.
  PRBool HasFixedBackground() const;
};

// See https://bugzilla.mozilla.org/show_bug.cgi?id=271586#c43 for why
// this is hard to replace with 'currentColor'.
#define BORDER_COLOR_FOREGROUND   0x20
#define OUTLINE_COLOR_INITIAL     0x80
// FOREGROUND | INITIAL(OUTLINE)
#define BORDER_COLOR_SPECIAL      0xA0
#define BORDER_STYLE_MASK         0x1F

#define NS_SPACING_MARGIN   0
#define NS_SPACING_PADDING  1
#define NS_SPACING_BORDER   2


struct nsStyleMargin {
  nsStyleMargin(void);
  nsStyleMargin(const nsStyleMargin& aMargin);
  ~nsStyleMargin(void) {}

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW;
  void Destroy(nsPresContext* aContext);

  void RecalcData();
  nsChangeHint CalcDifference(const nsStyleMargin& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif

  nsStyleSides  mMargin;          // [reset] coord, percent, auto

  PRBool GetMargin(nsMargin& aMargin) const
  {
    if (mHasCachedMargin) {
      aMargin = mCachedMargin;
      return PR_TRUE;
    }
    return PR_FALSE;
  }

protected:
  PRPackedBool  mHasCachedMargin;
  nsMargin      mCachedMargin;
};


struct nsStylePadding {
  nsStylePadding(void);
  nsStylePadding(const nsStylePadding& aPadding);
  ~nsStylePadding(void) {}

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW;
  void Destroy(nsPresContext* aContext);

  void RecalcData();
  nsChangeHint CalcDifference(const nsStylePadding& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif
  
  nsStyleSides  mPadding;         // [reset] coord, percent

  PRBool GetPadding(nsMargin& aPadding) const
  {
    if (mHasCachedPadding) {
      aPadding = mCachedPadding;
      return PR_TRUE;
    }
    return PR_FALSE;
  }

protected:
  PRPackedBool  mHasCachedPadding;
  nsMargin      mCachedPadding;
};

struct nsBorderColors {
  nsBorderColors* mNext;
  nscolor mColor;

  nsBorderColors() : mNext(nsnull), mColor(NS_RGB(0,0,0)) {}
  nsBorderColors(const nscolor& aColor) : mNext(nsnull), mColor(aColor) {}
  ~nsBorderColors();

  nsBorderColors* Clone() const { return Clone(PR_TRUE); }

  static PRBool Equal(const nsBorderColors* c1,
                      const nsBorderColors* c2) {
    if (c1 == c2)
      return PR_TRUE;
    while (c1 && c2) {
      if (c1->mColor != c2->mColor)
        return PR_FALSE;
      c1 = c1->mNext;
      c2 = c2->mNext;
    }
    // both should be NULL if these are equal, otherwise one
    // has more colors than another
    return !c1 && !c2;
  }

private:
  nsBorderColors* Clone(PRBool aDeep) const;
};

struct nsCSSShadowItem {
  nscoord mXOffset;
  nscoord mYOffset;
  nscoord mRadius;
  nscoord mSpread;

  nscolor      mColor;
  PRPackedBool mHasColor; // Whether mColor should be used

  nsCSSShadowItem() : mHasColor(PR_FALSE) {
    MOZ_COUNT_CTOR(nsCSSShadowItem);
  }
  ~nsCSSShadowItem() {
    MOZ_COUNT_DTOR(nsCSSShadowItem);
  }

  PRBool operator==(const nsCSSShadowItem& aOther) {
    return (mXOffset == aOther.mXOffset &&
            mYOffset == aOther.mYOffset &&
            mRadius == aOther.mRadius &&
            mHasColor == aOther.mHasColor &&
            mSpread == aOther.mSpread &&
            (!mHasColor || mColor == aOther.mColor));
  }
  PRBool operator!=(const nsCSSShadowItem& aOther) {
    return !(*this == aOther);
  }
};

class nsCSSShadowArray {
  public:
    void* operator new(size_t aBaseSize, PRUint32 aArrayLen) {
      // We can allocate both this nsCSSShadowArray and the
      // actual array in one allocation. The amount of memory to
      // allocate is equal to the class's size + the number of bytes for all
      // but the first array item (because aBaseSize includes one
      // item, see the private declarations)
      return ::operator new(aBaseSize +
                            (aArrayLen - 1) * sizeof(nsCSSShadowItem));
    }

    nsCSSShadowArray(PRUint32 aArrayLen) :
      mLength(aArrayLen), mRefCnt(0)
    {
      MOZ_COUNT_CTOR(nsCSSShadowArray);
      for (PRUint32 i = 1; i < mLength; ++i) {
        // Make sure we call the constructors of each nsCSSShadowItem
        // (the first one is called for us because we declared it under private)
        new (&mArray[i]) nsCSSShadowItem();
      }
    }
    ~nsCSSShadowArray() {
      MOZ_COUNT_DTOR(nsCSSShadowArray);
      for (PRUint32 i = 1; i < mLength; ++i) {
        mArray[i].~nsCSSShadowItem();
      }
    }

    nsrefcnt AddRef() { return ++mRefCnt; }
    nsrefcnt Release();

    PRUint32 Length() const { return mLength; }
    nsCSSShadowItem* ShadowAt(PRUint32 i) {
      NS_ABORT_IF_FALSE(i < mLength, "Accessing too high an index in the text shadow array!");
      return &mArray[i];
    }
    const nsCSSShadowItem* ShadowAt(PRUint32 i) const {
      NS_ABORT_IF_FALSE(i < mLength, "Accessing too high an index in the text shadow array!");
      return &mArray[i];
    }

  private:
    PRUint32 mLength;
    PRUint32 mRefCnt;
    nsCSSShadowItem mArray[1]; // This MUST be the last item
};

// Border widths are rounded to the nearest-below integer number of pixels,
// but values between zero and one device pixels are always rounded up to
// one device pixel.
#define NS_ROUND_BORDER_TO_PIXELS(l,tpp) \
  ((l) == 0) ? 0 : PR_MAX((tpp), (l) / (tpp) * (tpp))
// Outline offset is rounded to the nearest integer number of pixels, but values
// between zero and one device pixels are always rounded up to one device pixel.
// Note that the offset can be negative.
#define NS_ROUND_OFFSET_TO_PIXELS(l,tpp) \
  (((l) == 0) ? 0 : \
    ((l) > 0) ? PR_MAX( (tpp), ((l) + ((tpp) / 2)) / (tpp) * (tpp)) : \
                PR_MIN(-(tpp), ((l) - ((tpp) / 2)) / (tpp) * (tpp)))

// Returns if the given border style type is visible or not
static PRBool IsVisibleBorderStyle(PRUint8 aStyle)
{
  return (aStyle != NS_STYLE_BORDER_STYLE_NONE &&
          aStyle != NS_STYLE_BORDER_STYLE_HIDDEN);
}

struct nsStyleBorder {
  nsStyleBorder(nsPresContext* aContext);
  nsStyleBorder(const nsStyleBorder& aBorder);
  ~nsStyleBorder();

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW;
  void Destroy(nsPresContext* aContext);

  nsChangeHint CalcDifference(const nsStyleBorder& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif
  PRBool ImageBorderDiffers() const;
 
  nsStyleCorners mBorderRadius;    // [reset] coord, percent
  nsStyleSides  mBorderImageSplit; // [reset] integer, percent
  PRUint8       mFloatEdge;       // [reset] see nsStyleConsts.h
  PRUint8       mBorderImageHFill; // [reset]
  PRUint8       mBorderImageVFill; // [reset]
  nsBorderColors** mBorderColors; // [reset] multiple levels of color for a border.
  nsRefPtr<nsCSSShadowArray> mBoxShadow; // [reset] NULL for 'none'
  PRBool        mHaveBorderImageWidth; // [reset]
  nsMargin      mBorderImageWidth; // [reset]
  
  void EnsureBorderColors() {
    if (!mBorderColors) {
      mBorderColors = new nsBorderColors*[4];
      if (mBorderColors)
        for (PRInt32 i = 0; i < 4; i++)
          mBorderColors[i] = nsnull;
    }
  }

  void ClearBorderColors(PRUint8 aSide) {
    if (mBorderColors && mBorderColors[aSide]) {
      delete mBorderColors[aSide];
      mBorderColors[aSide] = nsnull;
    }
  }

  // Return whether aStyle is a visible style.  Invisible styles cause
  // the relevant computed border width to be 0.
  // Note that this does *not* consider the effects of 'border-image':
  // if border-style is none, but there is a loaded border image,
  // HasVisibleStyle will be false even though there *is* a border.
  PRBool HasVisibleStyle(PRUint8 aSide)
  {
    return IsVisibleBorderStyle(GetBorderStyle(aSide));
  }

  // aBorderWidth is in twips
  void SetBorderWidth(PRUint8 aSide, nscoord aBorderWidth)
  {
    nscoord roundedWidth =
      NS_ROUND_BORDER_TO_PIXELS(aBorderWidth, mTwipsPerPixel);
    mBorder.side(aSide) = roundedWidth;
    if (HasVisibleStyle(aSide))
      mComputedBorder.side(aSide) = roundedWidth;
  }

  void SetBorderImageWidthOverride(PRUint8 aSide, nscoord aBorderWidth)
  {
    mBorderImageWidth.side(aSide) =
      NS_ROUND_BORDER_TO_PIXELS(aBorderWidth, mTwipsPerPixel);
  }

  // Get the actual border, in twips.  (If there is no border-image
  // loaded, this is the same as GetComputedBorder.  If there is a
  // border-image loaded, it uses the border-image width overrides if
  // present, and otherwise mBorder, which is GetComputedBorder without
  // considering border-style: none.)
  const nsMargin& GetActualBorder() const;
  
  // Get the computed border (plus rounding).  This does consider the
  // effects of 'border-style: none', but does not consider
  // 'border-image'.
  const nsMargin& GetComputedBorder() const
  {
    return mComputedBorder;
  }

  // Get the actual border width for a particular side, in twips.  Note that
  // this is zero if and only if there is no border to be painted for this
  // side.  That is, this value takes into account the border style and the
  // value is rounded to the nearest device pixel by NS_ROUND_BORDER_TO_PIXELS.
  nscoord GetActualBorderWidth(PRUint8 aSide) const
  {
    return GetActualBorder().side(aSide);
  }

  PRUint8 GetBorderStyle(PRUint8 aSide) const
  {
    NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side"); 
    return (mBorderStyle[aSide] & BORDER_STYLE_MASK); 
  }

  void SetBorderStyle(PRUint8 aSide, PRUint8 aStyle)
  {
    NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side"); 
    mBorderStyle[aSide] &= ~BORDER_STYLE_MASK; 
    mBorderStyle[aSide] |= (aStyle & BORDER_STYLE_MASK);
    mComputedBorder.side(aSide) =
      (HasVisibleStyle(aSide) ? mBorder.side(aSide) : 0);
  }

  // Defined in nsStyleStructInlines.h
  inline PRBool IsBorderImageLoaded() const;

  void GetBorderColor(PRUint8 aSide, nscolor& aColor,
                      PRBool& aForeground) const
  {
    aForeground = PR_FALSE;
    NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side"); 
    if ((mBorderStyle[aSide] & BORDER_COLOR_SPECIAL) == 0)
      aColor = mBorderColor[aSide]; 
    else if (mBorderStyle[aSide] & BORDER_COLOR_FOREGROUND)
      aForeground = PR_TRUE;
    else
      NS_NOTREACHED("OUTLINE_COLOR_INITIAL should not be set here");
  }

  void SetBorderColor(PRUint8 aSide, nscolor aColor) 
  {
    NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side"); 
    mBorderColor[aSide] = aColor; 
    mBorderStyle[aSide] &= ~BORDER_COLOR_SPECIAL;
  }

  // These are defined in nsStyleStructInlines.h
  inline void SetBorderImage(imgIRequest* aImage);
  inline imgIRequest* GetBorderImage() const;

  void GetCompositeColors(PRInt32 aIndex, nsBorderColors** aColors) const
  {
    if (!mBorderColors)
      *aColors = nsnull;
    else
      *aColors = mBorderColors[aIndex];
  }

  void AppendBorderColor(PRInt32 aIndex, nscolor aColor)
  {
    NS_ASSERTION(aIndex >= 0 && aIndex <= 3, "bad side for composite border color");
    nsBorderColors* colorEntry = new nsBorderColors(aColor);
    if (!mBorderColors[aIndex])
      mBorderColors[aIndex] = colorEntry;
    else {
      nsBorderColors* last = mBorderColors[aIndex];
      while (last->mNext)
        last = last->mNext;
      last->mNext = colorEntry;
    }
    mBorderStyle[aIndex] &= ~BORDER_COLOR_SPECIAL;
  }

  void SetBorderToForeground(PRUint8 aSide)
  {
    NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side"); 
    mBorderStyle[aSide] &= ~BORDER_COLOR_SPECIAL;
    mBorderStyle[aSide] |= BORDER_COLOR_FOREGROUND; 
  }

protected:
  // mComputedBorder holds the CSS2.1 computed border-width values.  In
  // particular, these widths take into account the border-style for the
  // relevant side and the values are rounded to the nearest device
  // pixel.  They are also rounded (which is not part of the definition
  // of computed values).  However, they do *not* take into account the
  // presence of border-image.  See GetActualBorder above for how to
  // really get the actual border.
  nsMargin      mComputedBorder;

  // mBorder holds the nscoord values for the border widths as they would be if
  // all the border-style values were visible (not hidden or none).  This
  // member exists so that when we create structs using the copy
  // constructor during style resolution the new structs will know what the
  // specified values of the border were in case they have more specific rules
  // setting the border style.  Note that this isn't quite the CSS specified
  // value, since this has had the enumerated border widths converted to
  // lengths, and all lengths converted to twips.  But it's not quite the
  // computed value either. The values are rounded to the nearest device pixel
  // We also use these values when we have a loaded border-image that
  // does not have width overrides.
  nsMargin      mBorder;

  PRUint8       mBorderStyle[4];  // [reset] See nsStyleConsts.h
  nscolor       mBorderColor[4];  // [reset] the colors to use for a simple border.  not used
                                  // if -moz-border-colors is specified

  nsCOMPtr<imgIRequest> mBorderImage; // [reset]

  nscoord       mTwipsPerPixel;
};


struct nsStyleOutline {
  nsStyleOutline(nsPresContext* aPresContext);
  nsStyleOutline(const nsStyleOutline& aOutline);
  ~nsStyleOutline(void) {}

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleOutline();
    aContext->FreeToShell(sizeof(nsStyleOutline), this);
  }

  void RecalcData(nsPresContext* aContext);
  nsChangeHint CalcDifference(const nsStyleOutline& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif
 
  nsStyleCorners  mOutlineRadius; // [reset] coord, percent

  // Note that this is a specified value.  You can get the actual values
  // with GetOutlineWidth.  You cannot get the computed value directly.
  nsStyleCoord  mOutlineWidth;    // [reset] coord, enum (see nsStyleConsts.h)
  nscoord       mOutlineOffset;   // [reset]

  PRBool GetOutlineWidth(nscoord& aWidth) const
  {
    if (mHasCachedOutline) {
      aWidth = mCachedOutlineWidth;
      return PR_TRUE;
    }
    return PR_FALSE;
  }

  PRUint8 GetOutlineStyle(void) const
  {
    return (mOutlineStyle & BORDER_STYLE_MASK);
  }

  void SetOutlineStyle(PRUint8 aStyle)
  {
    mOutlineStyle &= ~BORDER_STYLE_MASK;
    mOutlineStyle |= (aStyle & BORDER_STYLE_MASK);
  }

  // PR_FALSE means initial value
  PRBool GetOutlineColor(nscolor& aColor) const
  {
    if ((mOutlineStyle & BORDER_COLOR_SPECIAL) == 0) {
      aColor = mOutlineColor;
      return PR_TRUE;
    }
    return PR_FALSE;
  }

  void SetOutlineColor(nscolor aColor)
  {
    mOutlineColor = aColor;
    mOutlineStyle &= ~BORDER_COLOR_SPECIAL;
  }

  void SetOutlineInitialColor()
  {
    mOutlineStyle |= OUTLINE_COLOR_INITIAL;
  }

  PRBool GetOutlineInitialColor() const
  {
    return !!(mOutlineStyle & OUTLINE_COLOR_INITIAL);
  }

protected:
  // This value is the actual value, so it's rounded to the nearest device
  // pixel.
  nscoord       mCachedOutlineWidth;

  nscolor       mOutlineColor;    // [reset] 

  PRPackedBool  mHasCachedOutline;
  PRUint8       mOutlineStyle;    // [reset] See nsStyleConsts.h

  nscoord       mTwipsPerPixel;
};


struct nsStyleList {
  nsStyleList(void);
  nsStyleList(const nsStyleList& aStyleList);
  ~nsStyleList(void);

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleList();
    aContext->FreeToShell(sizeof(nsStyleList), this);
  }

  nsChangeHint CalcDifference(const nsStyleList& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif
  
  PRUint8   mListStyleType;             // [inherited] See nsStyleConsts.h
  PRUint8   mListStylePosition;         // [inherited] 
  nsCOMPtr<imgIRequest> mListStyleImage; // [inherited]
  nsRect        mImageRegion;           // [inherited] the rect to use within an image  
};

struct nsStylePosition {
  nsStylePosition(void);
  nsStylePosition(const nsStylePosition& aOther);
  ~nsStylePosition(void);

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStylePosition();
    aContext->FreeToShell(sizeof(nsStylePosition), this);
  }

  nsChangeHint CalcDifference(const nsStylePosition& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif
  
  nsStyleSides  mOffset;                // [reset] coord, percent, auto
  nsStyleCoord  mWidth;                 // [reset] coord, percent, auto, enum
  nsStyleCoord  mMinWidth;              // [reset] coord, percent, enum
  nsStyleCoord  mMaxWidth;              // [reset] coord, percent, null, enum
  nsStyleCoord  mHeight;                // [reset] coord, percent, auto
  nsStyleCoord  mMinHeight;             // [reset] coord, percent
  nsStyleCoord  mMaxHeight;             // [reset] coord, percent, null
  PRUint8       mBoxSizing;             // [reset] see nsStyleConsts.h
  nsStyleCoord  mZIndex;                // [reset] integer, auto
};

struct nsStyleTextReset {
  nsStyleTextReset(void);
  nsStyleTextReset(const nsStyleTextReset& aOther);
  ~nsStyleTextReset(void);

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleTextReset();
    aContext->FreeToShell(sizeof(nsStyleTextReset), this);
  }

  nsChangeHint CalcDifference(const nsStyleTextReset& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif
  
  PRUint8 mTextDecoration;              // [reset] see nsStyleConsts.h
  PRUint8 mUnicodeBidi;                 // [reset] see nsStyleConsts.h

  nsStyleCoord  mVerticalAlign;         // [reset] coord, percent, enum (see nsStyleConsts.h)
};

struct nsStyleText {
  nsStyleText(void);
  nsStyleText(const nsStyleText& aOther);
  ~nsStyleText(void);

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleText();
    aContext->FreeToShell(sizeof(nsStyleText), this);
  }

  nsChangeHint CalcDifference(const nsStyleText& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif

  PRUint8 mTextAlign;                   // [inherited] see nsStyleConsts.h
  PRUint8 mTextTransform;               // [inherited] see nsStyleConsts.h
  PRUint8 mWhiteSpace;                  // [inherited] see nsStyleConsts.h
  PRUint8 mWordWrap;                    // [inherited] see nsStyleConsts.h

  nsStyleCoord  mLetterSpacing;         // [inherited] coord, normal
  nsStyleCoord  mLineHeight;            // [inherited] coord, factor, normal
  nsStyleCoord  mTextIndent;            // [inherited] coord, percent
  nsStyleCoord  mWordSpacing;           // [inherited] coord, normal

  nsRefPtr<nsCSSShadowArray> mTextShadow; // [inherited] NULL in case of a zero-length
  
  PRBool WhiteSpaceIsSignificant() const {
    return mWhiteSpace == NS_STYLE_WHITESPACE_PRE ||
           mWhiteSpace == NS_STYLE_WHITESPACE_PRE_WRAP;
  }

  PRBool NewlineIsSignificant() const {
    return mWhiteSpace == NS_STYLE_WHITESPACE_PRE ||
           mWhiteSpace == NS_STYLE_WHITESPACE_PRE_WRAP ||
           mWhiteSpace == NS_STYLE_WHITESPACE_PRE_LINE;
  }

  PRBool WhiteSpaceCanWrap() const {
    return mWhiteSpace == NS_STYLE_WHITESPACE_NORMAL ||
           mWhiteSpace == NS_STYLE_WHITESPACE_PRE_WRAP ||
           mWhiteSpace == NS_STYLE_WHITESPACE_PRE_LINE;
  }

  PRBool WordCanWrap() const {
    return WhiteSpaceCanWrap() && mWordWrap == NS_STYLE_WORDWRAP_BREAK_WORD;
  }
};

struct nsStyleVisibility {
  nsStyleVisibility(nsPresContext* aPresContext);
  nsStyleVisibility(const nsStyleVisibility& aVisibility);
  ~nsStyleVisibility() {}

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleVisibility();
    aContext->FreeToShell(sizeof(nsStyleVisibility), this);
  }

  nsChangeHint CalcDifference(const nsStyleVisibility& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif
  
  PRUint8 mDirection;                  // [inherited] see nsStyleConsts.h NS_STYLE_DIRECTION_*
  PRUint8   mVisible;                  // [inherited]
  nsCOMPtr<nsIAtom> mLangGroup;        // [inherited]
 
  PRBool IsVisible() const {
    return (mVisible == NS_STYLE_VISIBILITY_VISIBLE);
  }

  PRBool IsVisibleOrCollapsed() const {
    return ((mVisible == NS_STYLE_VISIBILITY_VISIBLE) ||
            (mVisible == NS_STYLE_VISIBILITY_COLLAPSE));
  }
};

struct nsStyleDisplay {
  nsStyleDisplay();
  nsStyleDisplay(const nsStyleDisplay& aOther); 
  ~nsStyleDisplay() {}

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleDisplay();
    aContext->FreeToShell(sizeof(nsStyleDisplay), this);
  }

  nsChangeHint CalcDifference(const nsStyleDisplay& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif

  // We guarantee that if mBinding is non-null, so are mBinding->mURI and
  // mBinding->mOriginPrincipal.
  nsRefPtr<nsCSSValue::URL> mBinding;    // [reset]
#if 0
  // XXX This is how it is defined in the CSS2 spec, but the errata
  // changed it to be consistent with the positioning draft and how
  // Nav and IE implement it
  nsMargin  mClip;              // [reset] offsets from respective edge
#else
  nsRect    mClip;              // [reset] offsets from upper-left border edge
#endif
  float   mOpacity;             // [reset]
  PRUint8 mDisplay;             // [reset] see nsStyleConsts.h NS_STYLE_DISPLAY_*
  PRUint8 mOriginalDisplay;     // [reset] saved mDisplay for position:absolute/fixed
  PRUint8 mAppearance;          // [reset]
  PRUint8 mPosition;            // [reset] see nsStyleConsts.h
  PRUint8 mFloats;              // [reset] see nsStyleConsts.h NS_STYLE_FLOAT_*
  PRUint8 mBreakType;           // [reset] see nsStyleConsts.h NS_STYLE_CLEAR_*
  PRPackedBool mBreakBefore;    // [reset] 
  PRPackedBool mBreakAfter;     // [reset] 
  PRUint8 mOverflowX;           // [reset] see nsStyleConsts.h
  PRUint8 mOverflowY;           // [reset] see nsStyleConsts.h
  PRUint8   mClipFlags;         // [reset] see nsStyleConsts.h
  PRPackedBool mTransformPresent;  // [reset] Whether there is a -moz-transform.
  nsStyleTransformMatrix mTransform; // [reset] The stored transform matrix
  nsStyleCoord mTransformOrigin[2]; // [reset] percent, coord.

  PRBool IsBlockInside() const {
    return NS_STYLE_DISPLAY_BLOCK == mDisplay ||
           NS_STYLE_DISPLAY_LIST_ITEM == mDisplay ||
           NS_STYLE_DISPLAY_INLINE_BLOCK == mDisplay;
    // Should TABLE_CELL and TABLE_CAPTION go here?  They have
    // block frames nested inside of them.
    // (But please audit all callers before changing.)
  }

  PRBool IsBlockOutside() const {
    return NS_STYLE_DISPLAY_BLOCK == mDisplay ||
           NS_STYLE_DISPLAY_LIST_ITEM == mDisplay ||
           NS_STYLE_DISPLAY_TABLE == mDisplay;
  }

  PRBool IsInlineOutside() const {
    return NS_STYLE_DISPLAY_INLINE == mDisplay ||
           NS_STYLE_DISPLAY_INLINE_BLOCK == mDisplay ||
           NS_STYLE_DISPLAY_INLINE_TABLE == mDisplay ||
           NS_STYLE_DISPLAY_INLINE_BOX == mDisplay ||
           NS_STYLE_DISPLAY_INLINE_GRID == mDisplay ||
           NS_STYLE_DISPLAY_INLINE_STACK == mDisplay;
  }

  PRBool IsFloating() const {
    return NS_STYLE_FLOAT_NONE != mFloats;
  }

  PRBool IsAbsolutelyPositioned() const {return (NS_STYLE_POSITION_ABSOLUTE == mPosition) ||
                                                (NS_STYLE_POSITION_FIXED == mPosition);}

  /* Returns true if we're positioned or there's a transform in effect. */
  PRBool IsPositioned() const {
    return IsAbsolutelyPositioned() ||
      NS_STYLE_POSITION_RELATIVE == mPosition || mTransformPresent;
  }

  PRBool IsScrollableOverflow() const {
    // mOverflowX and mOverflowY always match when one of them is
    // NS_STYLE_OVERFLOW_VISIBLE or NS_STYLE_OVERFLOW_CLIP.
    return mOverflowX != NS_STYLE_OVERFLOW_VISIBLE &&
           mOverflowX != NS_STYLE_OVERFLOW_CLIP;
  }

  // For table elements that don't support scroll frame creation, we
  // support 'overflow: hidden' to mean 'overflow: -moz-hidden-unscrollable'.
  PRBool IsTableClip() const {
    return mOverflowX == NS_STYLE_OVERFLOW_CLIP ||
           (mOverflowX == NS_STYLE_OVERFLOW_HIDDEN &&
            mOverflowY == NS_STYLE_OVERFLOW_HIDDEN);
  }

  /* Returns whether the element has the -moz-transform property. */
  PRBool HasTransform() const {
    return mTransformPresent;
  }
};

struct nsStyleTable {
  nsStyleTable(void);
  nsStyleTable(const nsStyleTable& aOther);
  ~nsStyleTable(void);

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleTable();
    aContext->FreeToShell(sizeof(nsStyleTable), this);
  }

  nsChangeHint CalcDifference(const nsStyleTable& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif
  
  PRUint8       mLayoutStrategy;// [reset] see nsStyleConsts.h NS_STYLE_TABLE_LAYOUT_*
  PRUint8       mFrame;         // [reset] see nsStyleConsts.h NS_STYLE_TABLE_FRAME_*
  PRUint8       mRules;         // [reset] see nsStyleConsts.h NS_STYLE_TABLE_RULES_*
  PRInt32       mCols;          // [reset] an integer if set, or see nsStyleConsts.h NS_STYLE_TABLE_COLS_*
  PRInt32       mSpan;          // [reset] the number of columns spanned by a colgroup or col
};

struct nsStyleTableBorder {
  nsStyleTableBorder(nsPresContext* aContext);
  nsStyleTableBorder(const nsStyleTableBorder& aOther);
  ~nsStyleTableBorder(void);

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleTableBorder();
    aContext->FreeToShell(sizeof(nsStyleTableBorder), this);
  }

  nsChangeHint CalcDifference(const nsStyleTableBorder& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif
  
  nscoord       mBorderSpacingX;// [inherited]
  nscoord       mBorderSpacingY;// [inherited]
  PRUint8       mBorderCollapse;// [inherited]
  PRUint8       mCaptionSide;   // [inherited]
  PRUint8       mEmptyCells;    // [inherited]
};

enum nsStyleContentType {
  eStyleContentType_String        = 1,
  eStyleContentType_Image         = 10,
  eStyleContentType_Attr          = 20,
  eStyleContentType_Counter       = 30,
  eStyleContentType_Counters      = 31,
  eStyleContentType_OpenQuote     = 40,
  eStyleContentType_CloseQuote    = 41,
  eStyleContentType_NoOpenQuote   = 42,
  eStyleContentType_NoCloseQuote  = 43,
  eStyleContentType_AltContent    = 50
};

struct nsStyleContentData {
  nsStyleContentType  mType;
  union {
    PRUnichar *mString;
    imgIRequest *mImage;
    nsCSSValue::Array* mCounters;
  } mContent;

  nsStyleContentData() : mType(nsStyleContentType(0)) { mContent.mString = nsnull; }
  ~nsStyleContentData();
  nsStyleContentData& operator=(const nsStyleContentData& aOther);
  PRBool operator==(const nsStyleContentData& aOther) const;

  PRBool operator!=(const nsStyleContentData& aOther) const {
    return !(*this == aOther);
  }
private:
  nsStyleContentData(const nsStyleContentData&); // not to be implemented
};

struct nsStyleCounterData {
  nsString  mCounter;
  PRInt32   mValue;
};


#define DELETE_ARRAY_IF(array)  if (array) { delete[] array; array = nsnull; }

struct nsStyleQuotes {
  nsStyleQuotes();
  nsStyleQuotes(const nsStyleQuotes& aQuotes);
  ~nsStyleQuotes();

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleQuotes();
    aContext->FreeToShell(sizeof(nsStyleQuotes), this);
  }

  void SetInitial();
  void CopyFrom(const nsStyleQuotes& aSource);

  nsChangeHint CalcDifference(const nsStyleQuotes& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif
  
  PRUint32  QuotesCount(void) const { return mQuotesCount; } // [inherited]

  const nsString* OpenQuoteAt(PRUint32 aIndex) const
  {
    NS_ASSERTION(aIndex < mQuotesCount, "out of range");
    return mQuotes + (aIndex * 2);
  }
  const nsString* CloseQuoteAt(PRUint32 aIndex) const
  {
    NS_ASSERTION(aIndex < mQuotesCount, "out of range");
    return mQuotes + (aIndex * 2 + 1);
  }
  nsresult  GetQuotesAt(PRUint32 aIndex, nsString& aOpen, nsString& aClose) const {
    if (aIndex < mQuotesCount) {
      aIndex *= 2;
      aOpen = mQuotes[aIndex];
      aClose = mQuotes[++aIndex];
      return NS_OK;
    }
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsresult  AllocateQuotes(PRUint32 aCount) {
    if (aCount != mQuotesCount) {
      DELETE_ARRAY_IF(mQuotes);
      if (aCount) {
        mQuotes = new nsString[aCount * 2];
        if (! mQuotes) {
          mQuotesCount = 0;
          return NS_ERROR_OUT_OF_MEMORY;
        }
      }
      mQuotesCount = aCount;
    }
    return NS_OK;
  }

  nsresult  SetQuotesAt(PRUint32 aIndex, const nsString& aOpen, const nsString& aClose) {
    if (aIndex < mQuotesCount) {
      aIndex *= 2;
      mQuotes[aIndex] = aOpen;
      mQuotes[++aIndex] = aClose;
      return NS_OK;
    }
    return NS_ERROR_ILLEGAL_VALUE;
  }

protected:
  PRUint32            mQuotesCount;
  nsString*           mQuotes;
};

struct nsStyleContent {
  nsStyleContent(void);
  nsStyleContent(const nsStyleContent& aContent);
  ~nsStyleContent(void);

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleContent();
    aContext->FreeToShell(sizeof(nsStyleContent), this);
  }

  nsChangeHint CalcDifference(const nsStyleContent& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif

  PRUint32  ContentCount(void) const  { return mContentCount; } // [reset]

  const nsStyleContentData& ContentAt(PRUint32 aIndex) const {
    NS_ASSERTION(aIndex < mContentCount, "out of range");
    return mContents[aIndex];
  }

  nsStyleContentData& ContentAt(PRUint32 aIndex) {
    NS_ASSERTION(aIndex < mContentCount, "out of range");
    return mContents[aIndex];
  }

  nsresult AllocateContents(PRUint32 aCount);

  PRUint32  CounterIncrementCount(void) const { return mIncrementCount; }  // [reset]
  const nsStyleCounterData* GetCounterIncrementAt(PRUint32 aIndex) const {
    NS_ASSERTION(aIndex < mIncrementCount, "out of range");
    return &mIncrements[aIndex];
  }

  nsresult  AllocateCounterIncrements(PRUint32 aCount) {
    if (aCount != mIncrementCount) {
      DELETE_ARRAY_IF(mIncrements);
      if (aCount) {
        mIncrements = new nsStyleCounterData[aCount];
        if (! mIncrements) {
          mIncrementCount = 0;
          return NS_ERROR_OUT_OF_MEMORY;
        }
      }
      mIncrementCount = aCount;
    }
    return NS_OK;
  }

  nsresult  SetCounterIncrementAt(PRUint32 aIndex, const nsString& aCounter, PRInt32 aIncrement) {
    if (aIndex < mIncrementCount) {
      mIncrements[aIndex].mCounter = aCounter;
      mIncrements[aIndex].mValue = aIncrement;
      return NS_OK;
    }
    return NS_ERROR_ILLEGAL_VALUE;
  }

  PRUint32  CounterResetCount(void) const { return mResetCount; }  // [reset]
  const nsStyleCounterData* GetCounterResetAt(PRUint32 aIndex) const {
    NS_ASSERTION(aIndex < mResetCount, "out of range");
    return &mResets[aIndex];
  }

  nsresult  AllocateCounterResets(PRUint32 aCount) {
    if (aCount != mResetCount) {
      DELETE_ARRAY_IF(mResets);
      if (aCount) {
        mResets = new nsStyleCounterData[aCount];
        if (! mResets) {
          mResetCount = 0;
          return NS_ERROR_OUT_OF_MEMORY;
        }
      }
      mResetCount = aCount;
    }
    return NS_OK;
  }

  nsresult  SetCounterResetAt(PRUint32 aIndex, const nsString& aCounter, PRInt32 aValue) {
    if (aIndex < mResetCount) {
      mResets[aIndex].mCounter = aCounter;
      mResets[aIndex].mValue = aValue;
      return NS_OK;
    }
    return NS_ERROR_ILLEGAL_VALUE;
  }

  nsStyleCoord  mMarkerOffset;  // [reset] coord, auto

protected:
  PRUint32            mContentCount;
  nsStyleContentData* mContents;

  PRUint32            mIncrementCount;
  nsStyleCounterData* mIncrements;

  PRUint32            mResetCount;
  nsStyleCounterData* mResets;
};

struct nsStyleUIReset {
  nsStyleUIReset(void);
  nsStyleUIReset(const nsStyleUIReset& aOther);
  ~nsStyleUIReset(void);

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleUIReset();
    aContext->FreeToShell(sizeof(nsStyleUIReset), this);
  }

  nsChangeHint CalcDifference(const nsStyleUIReset& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif

  PRUint8   mUserSelect;      // [reset] (selection-style)
  PRUint8   mForceBrokenImageIcon; // [reset]  (0 if not forcing, otherwise forcing)
  PRUint8   mIMEMode;         // [reset]
  PRUint8   mWindowShadow;    // [reset]
};

struct nsCursorImage {
  nsCOMPtr<imgIRequest> mImage;
  PRBool mHaveHotspot;
  float mHotspotX, mHotspotY;

  nsCursorImage();
};

struct nsStyleUserInterface {
  nsStyleUserInterface(void);
  nsStyleUserInterface(const nsStyleUserInterface& aOther);
  ~nsStyleUserInterface(void);

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleUserInterface();
    aContext->FreeToShell(sizeof(nsStyleUserInterface), this);
  }

  nsChangeHint CalcDifference(const nsStyleUserInterface& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif

  PRUint8   mUserInput;       // [inherited]
  PRUint8   mUserModify;      // [inherited] (modify-content)
  PRUint8   mUserFocus;       // [inherited] (auto-select)
  
  PRUint8   mCursor;          // [inherited] See nsStyleConsts.h

  PRUint32 mCursorArrayLength;
  nsCursorImage *mCursorArray;// [inherited] The specified URL values
                              //   and coordinates.  Takes precedence over
                              //   mCursor.  Zero-length array is represented
                              //   by null pointer.

  // Does not free mCursorArray; the caller is responsible for calling
  // |delete [] mCursorArray| first if it is needed.
  void CopyCursorArrayFrom(const nsStyleUserInterface& aSource);
};

struct nsStyleXUL {
  nsStyleXUL();
  nsStyleXUL(const nsStyleXUL& aSource);
  ~nsStyleXUL();

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleXUL();
    aContext->FreeToShell(sizeof(nsStyleXUL), this);
  }

  nsChangeHint CalcDifference(const nsStyleXUL& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif
  
  float         mBoxFlex;               // [reset] see nsStyleConsts.h
  PRUint32      mBoxOrdinal;            // [reset] see nsStyleConsts.h
  PRUint8       mBoxAlign;              // [reset] see nsStyleConsts.h
  PRUint8       mBoxDirection;          // [reset] see nsStyleConsts.h
  PRUint8       mBoxOrient;             // [reset] see nsStyleConsts.h
  PRUint8       mBoxPack;               // [reset] see nsStyleConsts.h
  PRPackedBool  mStretchStack;          // [reset] see nsStyleConsts.h
};

struct nsStyleColumn {
  nsStyleColumn(nsPresContext* aPresContext);
  nsStyleColumn(const nsStyleColumn& aSource);
  ~nsStyleColumn();

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleColumn();
    aContext->FreeToShell(sizeof(nsStyleColumn), this);
  }

  nsChangeHint CalcDifference(const nsStyleColumn& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif

  PRUint32     mColumnCount; // [reset] see nsStyleConsts.h
  nsStyleCoord mColumnWidth; // [reset] coord, auto
  nsStyleCoord mColumnGap;   // [reset] coord, percent, normal

  nscolor      mColumnRuleColor;  // [reset]
  PRUint8      mColumnRuleStyle;  // [reset]
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=271586#c43 for why
  // this is hard to replace with 'currentColor'.
  PRPackedBool mColumnRuleColorIsForeground;

  void SetColumnRuleWidth(nscoord aWidth) {
    mColumnRuleWidth = NS_ROUND_BORDER_TO_PIXELS(aWidth, mTwipsPerPixel);
  }

  nscoord GetComputedColumnRuleWidth() const {
    return (IsVisibleBorderStyle(mColumnRuleStyle) ? mColumnRuleWidth : 0);
  }

protected:
  nscoord mColumnRuleWidth;  // [reset] coord
  nscoord mTwipsPerPixel;
};

#ifdef MOZ_SVG
enum nsStyleSVGPaintType {
  eStyleSVGPaintType_None = 1,
  eStyleSVGPaintType_Color,
  eStyleSVGPaintType_Server
};

struct nsStyleSVGPaint
{
  nsStyleSVGPaintType mType;
  union {
    nscolor mColor;
    nsIURI *mPaintServer;
  } mPaint;
  nscolor mFallbackColor;

  nsStyleSVGPaint() : mType(nsStyleSVGPaintType(0)) { mPaint.mPaintServer = nsnull; }
  ~nsStyleSVGPaint();
  void SetType(nsStyleSVGPaintType aType);
  nsStyleSVGPaint& operator=(const nsStyleSVGPaint& aOther);
  PRBool operator==(const nsStyleSVGPaint& aOther) const; 

  PRBool operator!=(const nsStyleSVGPaint& aOther) const {
    return !(*this == aOther);
  }
};

struct nsStyleSVG {
  nsStyleSVG();
  nsStyleSVG(const nsStyleSVG& aSource);
  ~nsStyleSVG();

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleSVG();
    aContext->FreeToShell(sizeof(nsStyleSVG), this);
  }

  nsChangeHint CalcDifference(const nsStyleSVG& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif

  nsStyleSVGPaint  mFill;             // [inherited]
  nsStyleSVGPaint  mStroke;           // [inherited]
  nsCOMPtr<nsIURI> mMarkerEnd;        // [inherited]
  nsCOMPtr<nsIURI> mMarkerMid;        // [inherited]
  nsCOMPtr<nsIURI> mMarkerStart;      // [inherited]
  nsStyleCoord    *mStrokeDasharray;  // [inherited] coord, percent, factor

  nsStyleCoord     mStrokeDashoffset; // [inherited] coord, percent, factor
  nsStyleCoord     mStrokeWidth;      // [inherited] coord, percent, factor

  float            mFillOpacity;      // [inherited]
  float            mStrokeMiterlimit; // [inherited]
  float            mStrokeOpacity;    // [inherited]

  PRUint32         mStrokeDasharrayLength;
  PRUint8          mClipRule;         // [inherited]
  PRUint8          mColorInterpolation; // [inherited] see nsStyleConsts.h
  PRUint8          mColorInterpolationFilters; // [inherited] see nsStyleConsts.h
  PRUint8          mFillRule;         // [inherited] see nsStyleConsts.h
  PRUint8          mPointerEvents;    // [inherited] see nsStyleConsts.h
  PRUint8          mShapeRendering;   // [inherited] see nsStyleConsts.h
  PRUint8          mStrokeLinecap;    // [inherited] see nsStyleConsts.h
  PRUint8          mStrokeLinejoin;   // [inherited] see nsStyleConsts.h
  PRUint8          mTextAnchor;       // [inherited] see nsStyleConsts.h
  PRUint8          mTextRendering;    // [inherited] see nsStyleConsts.h
};

struct nsStyleSVGReset {
  nsStyleSVGReset();
  nsStyleSVGReset(const nsStyleSVGReset& aSource);
  ~nsStyleSVGReset();

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW {
    return aContext->AllocateFromShell(sz);
  }
  void Destroy(nsPresContext* aContext) {
    this->~nsStyleSVGReset();
    aContext->FreeToShell(sizeof(nsStyleSVGReset), this);
  }

  nsChangeHint CalcDifference(const nsStyleSVGReset& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif

  nscolor          mStopColor;        // [reset]
  nscolor          mFloodColor;       // [reset]
  nscolor          mLightingColor;    // [reset]
  nsCOMPtr<nsIURI> mClipPath;         // [reset]
  nsCOMPtr<nsIURI> mFilter;           // [reset]
  nsCOMPtr<nsIURI> mMask;             // [reset]

  float            mStopOpacity;      // [reset]
  float            mFloodOpacity;     // [reset]

  PRUint8          mDominantBaseline; // [reset] see nsStyleConsts.h
};
#endif

#endif /* nsStyleStruct_h___ */
