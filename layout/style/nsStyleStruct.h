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
 *   Mats Palmgren <matspal@gmail.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   Rob Arnold <robarnold@mozilla.com>
 *   Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>, Collabora Ltd.
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation
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

#include "mozilla/Attributes.h"

#include "nsColor.h"
#include "nsCoord.h"
#include "nsMargin.h"
#include "nsRect.h"
#include "nsFont.h"
#include "nsStyleCoord.h"
#include "nsStyleConsts.h"
#include "nsChangeHint.h"
#include "nsPresContext.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsTArray.h"
#include "nsIAtom.h"
#include "nsIURI.h"
#include "nsCSSValue.h"
#include "nsStyleTransformMatrix.h"
#include "nsAlgorithm.h"
#include "imgIRequest.h"
#include "gfxRect.h"

class nsIFrame;
class imgIRequest;
class imgIContainer;
struct nsCSSValueList;

// Includes nsStyleStructID.
#include "nsStyleStructFwd.h"

// Bits for each struct.
// NS_STYLE_INHERIT_BIT defined in nsStyleStructFwd.h
#define NS_STYLE_INHERIT_MASK             0x007fffff

// Additional bits for nsStyleContext's mBits:
// See nsStyleContext::HasTextDecorationLines
#define NS_STYLE_HAS_TEXT_DECORATION_LINES 0x00800000
// See nsStyleContext::HasPseudoElementData.
#define NS_STYLE_HAS_PSEUDO_ELEMENT_DATA  0x01000000
// See nsStyleContext::RelevantLinkIsVisited
#define NS_STYLE_RELEVANT_LINK_VISITED    0x02000000
// See nsStyleContext::IsStyleIfVisited
#define NS_STYLE_IS_STYLE_IF_VISITED      0x04000000
// See nsStyleContext::GetPseudoEnum
#define NS_STYLE_CONTEXT_TYPE_MASK        0xf8000000
#define NS_STYLE_CONTEXT_TYPE_SHIFT       27

// Additional bits for nsRuleNode's mDependentBits:
#define NS_RULE_NODE_GC_MARK              0x02000000
#define NS_RULE_NODE_IS_IMPORTANT         0x08000000
#define NS_RULE_NODE_LEVEL_MASK           0xf0000000
#define NS_RULE_NODE_LEVEL_SHIFT          28

// The lifetime of these objects is managed by the presshell's arena.

// Each struct must implement a static ForceCompare() function returning a
// boolean.  Structs that can return a hint that doesn't guarantee that the
// change will be applied to all descendants must return true from
// ForceCompare(), so that we will make sure to compare those structs in
// nsStyleContext::CalcStyleDifference.

struct nsStyleFont {
  nsStyleFont(const nsFont& aFont, nsPresContext *aPresContext);
  nsStyleFont(const nsStyleFont& aStyleFont);
  nsStyleFont(nsPresContext *aPresContext);
private:
  void Init(nsPresContext *aPresContext);
public:
  ~nsStyleFont(void) {
    MOZ_COUNT_DTOR(nsStyleFont);
  }

  nsChangeHint CalcDifference(const nsStyleFont& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif
  static bool ForceCompare() { return false; }
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

  // MathML scriptlevel support
  PRInt8  mScriptLevel;          // [inherited]
  // The value mSize would have had if scriptminsize had never been applied
  nscoord mScriptUnconstrainedSize;
  nscoord mScriptMinSize;        // [inherited] length
  float   mScriptSizeMultiplier; // [inherited]
  nsCOMPtr<nsIAtom> mLanguage;   // [inherited]
};

struct nsStyleGradientStop {
  nsStyleCoord mLocation; // percent, coord, none
  nscolor mColor;
};

class nsStyleGradient {
public:
  nsStyleGradient();
  PRUint8 mShape;  // NS_STYLE_GRADIENT_SHAPE_*
  PRUint8 mSize;   // NS_STYLE_GRADIENT_SIZE_*;
                   // not used (must be FARTHEST_CORNER) for linear shape
  bool mRepeating;
  bool mToCorner;

  nsStyleCoord mBgPosX; // percent, coord, calc, none
  nsStyleCoord mBgPosY; // percent, coord, calc, none
  nsStyleCoord mAngle;  // none, angle

  // stops are in the order specified in the stylesheet
  nsTArray<nsStyleGradientStop> mStops;

  bool operator==(const nsStyleGradient& aOther) const;
  bool operator!=(const nsStyleGradient& aOther) const {
    return !(*this == aOther);
  };

  bool IsOpaque();

  NS_INLINE_DECL_REFCOUNTING(nsStyleGradient)

private:
  ~nsStyleGradient() {}

  nsStyleGradient(const nsStyleGradient& aOther) MOZ_DELETE;
  nsStyleGradient& operator=(const nsStyleGradient& aOther) MOZ_DELETE;
};

enum nsStyleImageType {
  eStyleImageType_Null,
  eStyleImageType_Image,
  eStyleImageType_Gradient,
  eStyleImageType_Element
};

/**
 * Represents a paintable image of one of the following types.
 * (1) A real image loaded from an external source.
 * (2) A CSS linear or radial gradient.
 * (3) An element within a document, or an <img>, <video>, or <canvas> element
 *     not in a document.
 * (*) Optionally a crop rect can be set to paint a partial (rectangular)
 * region of an image. (Currently, this feature is only supported with an
 * image of type (1)).
 *
 * This struct is currently used only for 'background-image', but it may be
 * used by other CSS properties such as 'border-image', 'list-style-image', and
 * 'content' in the future (bug 507052).
 */
struct nsStyleImage {
  nsStyleImage();
  ~nsStyleImage();
  nsStyleImage(const nsStyleImage& aOther);
  nsStyleImage& operator=(const nsStyleImage& aOther);

  void SetNull();
  void SetImageData(imgIRequest* aImage);
  void TrackImage(nsPresContext* aContext);
  void UntrackImage(nsPresContext* aContext);
  void SetGradientData(nsStyleGradient* aGradient);
  void SetElementId(const PRUnichar* aElementId);
  void SetCropRect(nsStyleSides* aCropRect);

  nsStyleImageType GetType() const {
    return mType;
  }
  imgIRequest* GetImageData() const {
    NS_ABORT_IF_FALSE(mType == eStyleImageType_Image, "Data is not an image!");
    NS_ABORT_IF_FALSE(mImageTracked,
                      "Should be tracking any image we're going to use!");
    return mImage;
  }
  nsStyleGradient* GetGradientData() const {
    NS_ASSERTION(mType == eStyleImageType_Gradient, "Data is not a gradient!");
    return mGradient;
  }
  const PRUnichar* GetElementId() const {
    NS_ASSERTION(mType == eStyleImageType_Element, "Data is not an element!");
    return mElementId;
  }
  nsStyleSides* GetCropRect() const {
    NS_ASSERTION(mType == eStyleImageType_Image,
                 "Only image data can have a crop rect");
    return mCropRect;
  }

  /**
   * Compute the actual crop rect in pixels, using the source image bounds.
   * The computation involves converting percentage unit to pixel unit and
   * clamping each side value to fit in the source image bounds.
   * @param aActualCropRect the computed actual crop rect.
   * @param aIsEntireImage true iff |aActualCropRect| is identical to the
   * source image bounds.
   * @return true iff |aActualCropRect| holds a meaningful value.
   */
  bool ComputeActualCropRect(nsIntRect& aActualCropRect,
                               bool* aIsEntireImage = nsnull) const;

  /**
   * Requests a decode on the image.
   */
  nsresult RequestDecode() const;
  /**
   * @return true if the item is definitely opaque --- i.e., paints every
   * pixel within its bounds opaquely, and the bounds contains at least a pixel.
   */
  bool IsOpaque() const;
  /**
   * @return true if this image is fully loaded, and its size is calculated;
   * always returns true if |mType| is |eStyleImageType_Gradient| or
   * |eStyleImageType_Element|.
   */
  bool IsComplete() const;
  /**
   * @return true if it is 100% confident that this image contains no pixel
   * to draw.
   */
  bool IsEmpty() const {
    // There are some other cases when the image will be empty, for example
    // when the crop rect is empty. However, checking the emptiness of crop
    // rect is non-trivial since each side value can be specified with
    // percentage unit, which can not be evaluated until the source image size
    // is available. Therefore, we currently postpone the evaluation of crop
    // rect until the actual rendering time --- alternatively until GetOpaqueRegion()
    // is called.
    return mType == eStyleImageType_Null;
  }

  bool operator==(const nsStyleImage& aOther) const;
  bool operator!=(const nsStyleImage& aOther) const {
    return !(*this == aOther);
  }

private:
  void DoCopy(const nsStyleImage& aOther);

  nsStyleImageType mType;
  union {
    imgIRequest* mImage;
    nsStyleGradient* mGradient;
    PRUnichar* mElementId;
  };
  // This is _currently_ used only in conjunction with eStyleImageType_Image.
  nsAutoPtr<nsStyleSides> mCropRect;
#ifdef DEBUG
  bool mImageTracked;
#endif
};

struct nsStyleColor {
  nsStyleColor(nsPresContext* aPresContext);
  nsStyleColor(const nsStyleColor& aOther);
  ~nsStyleColor(void) {
    MOZ_COUNT_DTOR(nsStyleColor);
  }

  nsChangeHint CalcDifference(const nsStyleColor& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif
  static bool ForceCompare() { return false; }

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
  void Destroy(nsPresContext* aContext);

  nsChangeHint CalcDifference(const nsStyleBackground& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif
  static bool ForceCompare() { return false; }

  struct Position;
  friend struct Position;
  struct Position {
    typedef nsStyleCoord::Calc PositionCoord;
    PositionCoord mXPosition, mYPosition;

    // Initialize nothing
    Position() {}

    // Initialize to initial values
    void SetInitialValues();

    // True if the effective background image position described by this depends
    // on the size of the corresponding frame.
    bool DependsOnFrameSize() const {
      return mXPosition.mPercent != 0.0f || mYPosition.mPercent != 0.0f;
    }

    bool operator==(const Position& aOther) const {
      return mXPosition == aOther.mXPosition &&
             mYPosition == aOther.mYPosition;
    }
    bool operator!=(const Position& aOther) const {
      return !(*this == aOther);
    }
  };

  struct Size;
  friend struct Size;
  struct Size {
    struct Dimension : public nsStyleCoord::Calc {
      nscoord ResolveLengthPercentage(nscoord aAvailable) const {
        double d = double(mPercent) * double(aAvailable) + double(mLength);
        if (d < 0.0)
          return 0;
        return NSToCoordRoundWithClamp(float(d));
      }
    };
    Dimension mWidth, mHeight;

    nscoord ResolveWidthLengthPercentage(const nsSize& aBgPositioningArea) const {
      NS_ABORT_IF_FALSE(mWidthType == eLengthPercentage,
                        "resolving non-length/percent dimension!");
      return mWidth.ResolveLengthPercentage(aBgPositioningArea.width);
    }

    nscoord ResolveHeightLengthPercentage(const nsSize& aBgPositioningArea) const {
      NS_ABORT_IF_FALSE(mHeightType == eLengthPercentage,
                        "resolving non-length/percent dimension!");
      return mHeight.ResolveLengthPercentage(aBgPositioningArea.height);
    }

    // Except for eLengthPercentage, Dimension types which might change
    // how a layer is painted when the corresponding frame's dimensions
    // change *must* precede all dimension types which are agnostic to
    // frame size; see DependsOnFrameSize.
    enum DimensionType {
      // If one of mWidth and mHeight is eContain or eCover, then both are.
      // Also, these two values must equal the corresponding values in
      // kBackgroundSizeKTable.
      eContain, eCover,

      eAuto,
      eLengthPercentage,
      eDimensionType_COUNT
    };
    PRUint8 mWidthType, mHeightType;

    // True if the effective image size described by this depends on the size of
    // the corresponding frame, when aImage (which must not have null type) is
    // the background image.
    bool DependsOnFrameSize(const nsStyleImage& aImage) const;

    // Initialize nothing
    Size() {}

    // Initialize to initial values
    void SetInitialValues();

    bool operator==(const Size& aOther) const;
    bool operator!=(const Size& aOther) const {
      return !(*this == aOther);
    }
  };
  
  struct Repeat;
  friend struct Repeat;
  struct Repeat {
    PRUint8 mXRepeat, mYRepeat;
    
    // Initialize nothing
    Repeat() {}

    // Initialize to initial values
    void SetInitialValues();

    bool operator==(const Repeat& aOther) const {
      return mXRepeat == aOther.mXRepeat &&
             mYRepeat == aOther.mYRepeat;
    }
    bool operator!=(const Repeat& aOther) const {
      return !(*this == aOther);
    }
  };

  struct Layer;
  friend struct Layer;
  struct Layer {
    PRUint8 mAttachment;                // [reset] See nsStyleConsts.h
    PRUint8 mClip;                      // [reset] See nsStyleConsts.h
    PRUint8 mOrigin;                    // [reset] See nsStyleConsts.h
    Repeat mRepeat;                     // [reset] See nsStyleConsts.h
    Position mPosition;                 // [reset]
    nsStyleImage mImage;                // [reset]
    Size mSize;                         // [reset]

    // Initializes only mImage
    Layer();
    ~Layer();

    // Register/unregister images with the document. We do this only
    // after the dust has settled in ComputeBackgroundData.
    void TrackImages(nsPresContext* aContext) {
      if (mImage.GetType() == eStyleImageType_Image)
        mImage.TrackImage(aContext);
    }
    void UntrackImages(nsPresContext* aContext) {
      if (mImage.GetType() == eStyleImageType_Image)
        mImage.UntrackImage(aContext);
    }

    void SetInitialValues();

    // True if the rendering of this layer might change when the size
    // of the corresponding frame changes.  This is true for any
    // non-solid-color background whose position or size depends on
    // the frame size.  It's also true for SVG images whose root <svg>
    // node has a viewBox.
    bool RenderingMightDependOnFrameSize() const;

    // An equality operator that compares the images using URL-equality
    // rather than pointer-equality.
    bool operator==(const Layer& aOther) const;
    bool operator!=(const Layer& aOther) const {
      return !(*this == aOther);
    }
  };

  // The (positive) number of computed values of each property, since
  // the lengths of the lists are independent.
  PRUint32 mAttachmentCount,
           mClipCount,
           mOriginCount,
           mRepeatCount,
           mPositionCount,
           mImageCount,
           mSizeCount;
  // Layers are stored in an array, matching the top-to-bottom order in
  // which they are specified in CSS.  The number of layers to be used
  // should come from the background-image property.  We create
  // additional |Layer| objects for *any* property, not just
  // background-image.  This means that the bottommost layer that
  // callers in layout care about (which is also the one whose
  // background-clip applies to the background-color) may not be last
  // layer.  In layers below the bottom layer, properties will be
  // uninitialized unless their count, above, indicates that they are
  // present.
  nsAutoTArray<Layer, 1> mLayers;

  const Layer& BottomLayer() const { return mLayers[mImageCount - 1]; }

  #define NS_FOR_VISIBLE_BACKGROUND_LAYERS_BACK_TO_FRONT(var_, stylebg_) \
    for (PRUint32 var_ = (stylebg_)->mImageCount; var_-- != 0; )

  nscolor mBackgroundColor;       // [reset]

  // FIXME: This (now background-break in css3-background) should
  // probably move into a different struct so that everything in
  // nsStyleBackground is set by the background shorthand.
  PRUint8 mBackgroundInlinePolicy; // [reset] See nsStyleConsts.h

  // True if this background is completely transparent.
  bool IsTransparent() const;

  // We have to take slower codepaths for fixed background attachment,
  // but we don't want to do that when there's no image.
  // Not inline because it uses an nsCOMPtr<imgIRequest>
  // FIXME: Should be in nsStyleStructInlines.h.
  bool HasFixedBackground() const;
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
  ~nsStyleMargin(void) {
    MOZ_COUNT_DTOR(nsStyleMargin);
  }

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW;
  void Destroy(nsPresContext* aContext);

  void RecalcData();
  nsChangeHint CalcDifference(const nsStyleMargin& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif
  static bool ForceCompare() { return true; }

  nsStyleSides  mMargin;          // [reset] coord, percent, calc, auto

  bool IsWidthDependent() const { return !mHasCachedMargin; }
  bool GetMargin(nsMargin& aMargin) const
  {
    if (mHasCachedMargin) {
      aMargin = mCachedMargin;
      return true;
    }
    return false;
  }

protected:
  bool          mHasCachedMargin;
  nsMargin      mCachedMargin;
};


struct nsStylePadding {
  nsStylePadding(void);
  nsStylePadding(const nsStylePadding& aPadding);
  ~nsStylePadding(void) {
    MOZ_COUNT_DTOR(nsStylePadding);
  }

  void* operator new(size_t sz, nsPresContext* aContext) CPP_THROW_NEW;
  void Destroy(nsPresContext* aContext);

  void RecalcData();
  nsChangeHint CalcDifference(const nsStylePadding& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif
  static bool ForceCompare() { return true; }

  nsStyleSides  mPadding;         // [reset] coord, percent, calc

  bool IsWidthDependent() const { return !mHasCachedPadding; }
  bool GetPadding(nsMargin& aPadding) const
  {
    if (mHasCachedPadding) {
      aPadding = mCachedPadding;
      return true;
    }
    return false;
  }

protected:
  bool          mHasCachedPadding;
  nsMargin      mCachedPadding;
};

struct nsBorderColors {
  nsBorderColors* mNext;
  nscolor mColor;

  nsBorderColors() : mNext(nsnull), mColor(NS_RGB(0,0,0)) {}
  nsBorderColors(const nscolor& aColor) : mNext(nsnull), mColor(aColor) {}
  ~nsBorderColors();

  nsBorderColors* Clone() const { return Clone(true); }

  static bool Equal(const nsBorderColors* c1,
                      const nsBorderColors* c2) {
    if (c1 == c2)
      return true;
    while (c1 && c2) {
      if (c1->mColor != c2->mColor)
        return false;
      c1 = c1->mNext;
      c2 = c2->mNext;
    }
    // both should be NULL if these are equal, otherwise one
    // has more colors than another
    return !c1 && !c2;
  }

private:
  nsBorderColors* Clone(bool aDeep) const;
};

struct nsCSSShadowItem {
  nscoord mXOffset;
  nscoord mYOffset;
  nscoord mRadius;
  nscoord mSpread;

  nscolor      mColor;
  bool mHasColor; // Whether mColor should be used
  bool mInset;

  nsCSSShadowItem() : mHasColor(false) {
    MOZ_COUNT_CTOR(nsCSSShadowItem);
  }
  ~nsCSSShadowItem() {
    MOZ_COUNT_DTOR(nsCSSShadowItem);
  }

  bool operator==(const nsCSSShadowItem& aOther) {
    return (mXOffset == aOther.mXOffset &&
            mYOffset == aOther.mYOffset &&
            mRadius == aOther.mRadius &&
            mHasColor == aOther.mHasColor &&
            mSpread == aOther.mSpread &&
            mInset == aOther.mInset &&
            (!mHasColor || mColor == aOther.mColor));
  }
  bool operator!=(const nsCSSShadowItem& aOther) {
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
      mLength(aArrayLen)
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

    PRUint32 Length() const { return mLength; }
    nsCSSShadowItem* ShadowAt(PRUint32 i) {
      NS_ABORT_IF_FALSE(i < mLength, "Accessing too high an index in the text shadow array!");
      return &mArray[i];
    }
    const nsCSSShadowItem* ShadowAt(PRUint32 i) const {
      NS_ABORT_IF_FALSE(i < mLength, "Accessing too high an index in the text shadow array!");
      return &mArray[i];
    }

    bool HasShadowWithInset(bool aInset) {
      for (PRUint32 i = 0; i < mLength; ++i) {
        if (mArray[i].mInset == aInset)
          return true;
      }
      return false;
    }

    NS_INLINE_DECL_REFCOUNTING(nsCSSShadowArray)

  private:
    PRUint32 mLength;
    nsCSSShadowItem mArray[1]; // This MUST be the last item
};

// Border widths are rounded to the nearest-below integer number of pixels,
// but values between zero and one device pixels are always rounded up to
// one device pixel.
#define NS_ROUND_BORDER_TO_PIXELS(l,tpp) \
  ((l) == 0) ? 0 : NS_MAX((tpp), (l) / (tpp) * (tpp))
// Outline offset is rounded to the nearest integer number of pixels, but values
// between zero and one device pixels are always rounded up to one device pixel.
// Note that the offset can be negative.
#define NS_ROUND_OFFSET_TO_PIXELS(l,tpp) \
  (((l) == 0) ? 0 : \
    ((l) > 0) ? NS_MAX( (tpp), ((l) + ((tpp) / 2)) / (tpp) * (tpp)) : \
                NS_MIN(-(tpp), ((l) - ((tpp) / 2)) / (tpp) * (tpp)))

// Returns if the given border style type is visible or not
static bool IsVisibleBorderStyle(PRUint8 aStyle)
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
  // ForceCompare is true, because a change to our border-style might
  // change border-width on descendants (requiring reflow of those)
  // but not our own border-width (thus not requiring us to reflow).
  static bool ForceCompare() { return true; }

  void EnsureBorderColors() {
    if (!mBorderColors) {
      mBorderColors = new nsBorderColors*[4];
      if (mBorderColors)
        for (PRInt32 i = 0; i < 4; i++)
          mBorderColors[i] = nsnull;
    }
  }

  void ClearBorderColors(mozilla::css::Side aSide) {
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
  bool HasVisibleStyle(mozilla::css::Side aSide)
  {
    return IsVisibleBorderStyle(GetBorderStyle(aSide));
  }

  // aBorderWidth is in twips
  void SetBorderWidth(mozilla::css::Side aSide, nscoord aBorderWidth)
  {
    nscoord roundedWidth =
      NS_ROUND_BORDER_TO_PIXELS(aBorderWidth, mTwipsPerPixel);
    mBorder.Side(aSide) = roundedWidth;
    if (HasVisibleStyle(aSide))
      mComputedBorder.Side(aSide) = roundedWidth;
  }

  // Returns the computed border.
  inline const nsMargin& GetActualBorder() const
  {
    return mComputedBorder;
  }

  // Get the computed border (plus rounding).  This does consider the
  // effects of 'border-style: none', but does not consider
  // 'border-image'.
  const nsMargin& GetComputedBorder() const
  {
    return mComputedBorder;
  }

  bool HasBorder() const
  {
    return mComputedBorder != nsMargin(0,0,0,0) || mBorderImageSource;
  }

  // Get the actual border width for a particular side, in appunits.  Note that
  // this is zero if and only if there is no border to be painted for this
  // side.  That is, this value takes into account the border style and the
  // value is rounded to the nearest device pixel by NS_ROUND_BORDER_TO_PIXELS.
  nscoord GetActualBorderWidth(mozilla::css::Side aSide) const
  {
    return GetActualBorder().Side(aSide);
  }

  PRUint8 GetBorderStyle(mozilla::css::Side aSide) const
  {
    NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side");
    return (mBorderStyle[aSide] & BORDER_STYLE_MASK);
  }

  void SetBorderStyle(mozilla::css::Side aSide, PRUint8 aStyle)
  {
    NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side");
    mBorderStyle[aSide] &= ~BORDER_STYLE_MASK;
    mBorderStyle[aSide] |= (aStyle & BORDER_STYLE_MASK);
    mComputedBorder.Side(aSide) =
      (HasVisibleStyle(aSide) ? mBorder.Side(aSide) : 0);
  }

  // Defined in nsStyleStructInlines.h
  inline bool IsBorderImageLoaded() const;
  inline nsresult RequestDecode();

  void GetBorderColor(mozilla::css::Side aSide, nscolor& aColor,
                      bool& aForeground) const
  {
    aForeground = false;
    NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side");
    if ((mBorderStyle[aSide] & BORDER_COLOR_SPECIAL) == 0)
      aColor = mBorderColor[aSide];
    else if (mBorderStyle[aSide] & BORDER_COLOR_FOREGROUND)
      aForeground = true;
    else
      NS_NOTREACHED("OUTLINE_COLOR_INITIAL should not be set here");
  }

  void SetBorderColor(mozilla::css::Side aSide, nscolor aColor)
  {
    NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side");
    mBorderColor[aSide] = aColor;
    mBorderStyle[aSide] &= ~BORDER_COLOR_SPECIAL;
  }

  // These are defined in nsStyleStructInlines.h
  inline void SetBorderImage(imgIRequest* aImage);
  inline imgIRequest* GetBorderImage() const;

  bool HasBorderImage() {return !!mBorderImageSource;}

  void TrackImage(nsPresContext* aContext);
  void UntrackImage(nsPresContext* aContext);

  nsMargin GetImageOutset() const;

  // These methods are used for the caller to caches the sub images created during
  // a border-image paint operation
  inline void SetSubImage(PRUint8 aIndex, imgIContainer* aSubImage) const;
  inline imgIContainer* GetSubImage(PRUint8 aIndex) const;

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

  void SetBorderToForeground(mozilla::css::Side aSide)
  {
    NS_ASSERTION(aSide <= NS_SIDE_LEFT, "bad side");
    mBorderStyle[aSide] &= ~BORDER_COLOR_SPECIAL;
    mBorderStyle[aSide] |= BORDER_COLOR_FOREGROUND;
  }

public:
  nsBorderColors** mBorderColors;        // [reset] composite (stripe) colors
  nsRefPtr<nsCSSShadowArray> mBoxShadow; // [reset] NULL for 'none'

#ifdef DEBUG
  bool mImageTracked;
#endif

protected:
  nsCOMPtr<imgIRequest> mBorderImageSource; // [reset]

public:
  nsStyleCorners mBorderRadius;       // [reset] coord, percent
  nsStyleSides   mBorderImageSlice;   // [reset] factor, percent
  nsStyleSides   mBorderImageWidth;   // [reset] length, factor, percent, auto
  nsStyleSides   mBorderImageOutset;  // [reset] length, factor

  PRUint8        mBorderImageFill;    // [reset]
  PRUint8        mBorderImageRepeatH; // [reset] see nsStyleConsts.h
  PRUint8        mBorderImageRepeatV; // [reset]
  PRUint8        mFloatEdge;          // [reset]

protected:
  // mComputedBorder holds the CSS2.1 computed border-width values.
  // In particular, these widths take into account the border-style
  // for the relevant side, and the values are rounded to the nearest
  // device pixel (which is not part of the definition of computed
  // values). The presence or absence of a border-image does not
  // affect border-width values.
  nsMargin      mComputedBorder;

  // mBorder holds the nscoord values for the border widths as they
  // would be if all the border-style values were visible (not hidden
  // or none).  This member exists so that when we create structs
  // using the copy constructor during style resolution the new
  // structs will know what the specified values of the border were in
  // case they have more specific rules setting the border style.
  //
  // Note that this isn't quite the CSS specified value, since this
  // has had the enumerated border widths converted to lengths, and
  // all lengths converted to twips.  But it's not quite the computed
  // value either. The values are rounded to the nearest device pixel.
  nsMargin      mBorder;

  PRUint8       mBorderStyle[4];  // [reset] See nsStyleConsts.h
  nscolor       mBorderColor[4];  // [reset] the colors to use for a simple
                                  // border.  not used for -moz-border-colors
private:
  // Cache used by callers for border-image painting
  nsCOMArray<imgIContainer> mSubImages;

  nscoord       mTwipsPerPixel;

  nsStyleBorder& operator=(const nsStyleBorder& aOther) MOZ_DELETE;
};


struct nsStyleOutline {
  nsStyleOutline(nsPresContext* aPresContext);
  nsStyleOutline(const nsStyleOutline& aOutline);
  ~nsStyleOutline(void) {
    MOZ_COUNT_DTOR(nsStyleOutline);
  }

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
  static bool ForceCompare() { return false; }

  nsStyleCorners  mOutlineRadius; // [reset] coord, percent, calc

  // Note that this is a specified value.  You can get the actual values
  // with GetOutlineWidth.  You cannot get the computed value directly.
  nsStyleCoord  mOutlineWidth;    // [reset] coord, enum (see nsStyleConsts.h)
  nscoord       mOutlineOffset;   // [reset]

  bool GetOutlineWidth(nscoord& aWidth) const
  {
    if (mHasCachedOutline) {
      aWidth = mCachedOutlineWidth;
      return true;
    }
    return false;
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

  // false means initial value
  bool GetOutlineColor(nscolor& aColor) const
  {
    if ((mOutlineStyle & BORDER_COLOR_SPECIAL) == 0) {
      aColor = mOutlineColor;
      return true;
    }
    return false;
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

  bool GetOutlineInitialColor() const
  {
    return !!(mOutlineStyle & OUTLINE_COLOR_INITIAL);
  }

protected:
  // This value is the actual value, so it's rounded to the nearest device
  // pixel.
  nscoord       mCachedOutlineWidth;

  nscolor       mOutlineColor;    // [reset]

  bool          mHasCachedOutline;
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
  static bool ForceCompare() { return false; }

  imgIRequest* GetListStyleImage() const { return mListStyleImage; }
  void SetListStyleImage(imgIRequest* aReq)
  {
    if (mListStyleImage)
      mListStyleImage->UnlockImage();
    mListStyleImage = aReq;
    if (mListStyleImage)
      mListStyleImage->LockImage();
  }

  PRUint8   mListStyleType;             // [inherited] See nsStyleConsts.h
  PRUint8   mListStylePosition;         // [inherited]
private:
  nsCOMPtr<imgIRequest> mListStyleImage; // [inherited]
  nsStyleList& operator=(const nsStyleList& aOther) MOZ_DELETE;
public:
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
  static bool ForceCompare() { return true; }

  nsStyleSides  mOffset;                // [reset] coord, percent, calc, auto
  nsStyleCoord  mWidth;                 // [reset] coord, percent, enum, calc, auto
  nsStyleCoord  mMinWidth;              // [reset] coord, percent, enum, calc
  nsStyleCoord  mMaxWidth;              // [reset] coord, percent, enum, calc, none
  nsStyleCoord  mHeight;                // [reset] coord, percent, calc, auto
  nsStyleCoord  mMinHeight;             // [reset] coord, percent, calc
  nsStyleCoord  mMaxHeight;             // [reset] coord, percent, calc, none
  PRUint8       mBoxSizing;             // [reset] see nsStyleConsts.h
  nsStyleCoord  mZIndex;                // [reset] integer, auto

  bool WidthDependsOnContainer() const
    { return WidthCoordDependsOnContainer(mWidth); }
  bool MinWidthDependsOnContainer() const
    { return WidthCoordDependsOnContainer(mMinWidth); }
  bool MaxWidthDependsOnContainer() const
    { return WidthCoordDependsOnContainer(mMaxWidth); }

  // Note that these functions count 'auto' as depending on the
  // container since that's the case for absolutely positioned elements.
  // However, some callers do not care about this case and should check
  // for it, since it is the most common case.
  // FIXME: We should probably change the assumption to be the other way
  // around.
  bool HeightDependsOnContainer() const
    { return HeightCoordDependsOnContainer(mHeight); }
  bool MinHeightDependsOnContainer() const
    { return HeightCoordDependsOnContainer(mMinHeight); }
  bool MaxHeightDependsOnContainer() const
    { return HeightCoordDependsOnContainer(mMaxHeight); }

  bool OffsetHasPercent(mozilla::css::Side aSide) const
  {
    return mOffset.Get(aSide).HasPercent();
  }

private:
  static bool WidthCoordDependsOnContainer(const nsStyleCoord &aCoord);
  static bool HeightCoordDependsOnContainer(const nsStyleCoord &aCoord)
  {
    return aCoord.GetUnit() == eStyleUnit_Auto || // CSS 2.1, 10.6.4, item (5)
           aCoord.HasPercent();
  }
};

struct nsStyleTextOverflowSide {
  nsStyleTextOverflowSide() : mType(NS_STYLE_TEXT_OVERFLOW_CLIP) {}

  bool operator==(const nsStyleTextOverflowSide& aOther) const {
    return mType == aOther.mType &&
           (mType != NS_STYLE_TEXT_OVERFLOW_STRING ||
            mString == aOther.mString);
  }
  bool operator!=(const nsStyleTextOverflowSide& aOther) const {
    return !(*this == aOther);
  }

  nsString mString;
  PRUint8  mType;
};

struct nsStyleTextOverflow {
  nsStyleTextOverflow() : mLogicalDirections(true) {}
  bool operator==(const nsStyleTextOverflow& aOther) const {
    return mLeft == aOther.mLeft && mRight == aOther.mRight;
  }
  bool operator!=(const nsStyleTextOverflow& aOther) const {
    return !(*this == aOther);
  }

  // Returns the value to apply on the left side.
  const nsStyleTextOverflowSide& GetLeft(PRUint8 aDirection) const {
    NS_ASSERTION(aDirection == NS_STYLE_DIRECTION_LTR ||
                 aDirection == NS_STYLE_DIRECTION_RTL, "bad direction");
    return !mLogicalDirections || aDirection == NS_STYLE_DIRECTION_LTR ?
             mLeft : mRight;
  }

  // Returns the value to apply on the right side.
  const nsStyleTextOverflowSide& GetRight(PRUint8 aDirection) const {
    NS_ASSERTION(aDirection == NS_STYLE_DIRECTION_LTR ||
                 aDirection == NS_STYLE_DIRECTION_RTL, "bad direction");
    return !mLogicalDirections || aDirection == NS_STYLE_DIRECTION_LTR ?
             mRight : mLeft;
  }

  // Returns the first value that was specified.
  const nsStyleTextOverflowSide* GetFirstValue() const {
    return mLogicalDirections ? &mRight : &mLeft;
  }

  // Returns the second value, or null if there was only one value specified.
  const nsStyleTextOverflowSide* GetSecondValue() const {
    return mLogicalDirections ? nsnull : &mRight;
  }

  nsStyleTextOverflowSide mLeft;  // start side when mLogicalDirections is true
  nsStyleTextOverflowSide mRight; // end side when mLogicalDirections is true
  bool mLogicalDirections;  // true when only one value was specified
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

  PRUint8 GetDecorationStyle() const
  {
    return (mTextDecorationStyle & BORDER_STYLE_MASK);
  }

  void SetDecorationStyle(PRUint8 aStyle)
  {
    NS_ABORT_IF_FALSE((aStyle & BORDER_STYLE_MASK) == aStyle,
                      "style doesn't fit");
    mTextDecorationStyle &= ~BORDER_STYLE_MASK;
    mTextDecorationStyle |= (aStyle & BORDER_STYLE_MASK);
  }

  void GetDecorationColor(nscolor& aColor, bool& aForeground) const
  {
    aForeground = false;
    if ((mTextDecorationStyle & BORDER_COLOR_SPECIAL) == 0) {
      aColor = mTextDecorationColor;
    } else if (mTextDecorationStyle & BORDER_COLOR_FOREGROUND) {
      aForeground = true;
    } else {
      NS_NOTREACHED("OUTLINE_COLOR_INITIAL should not be set here");
    }
  }

  void SetDecorationColor(nscolor aColor)
  {
    mTextDecorationColor = aColor;
    mTextDecorationStyle &= ~BORDER_COLOR_SPECIAL;
  }

  void SetDecorationColorToForeground()
  {
    mTextDecorationStyle &= ~BORDER_COLOR_SPECIAL;
    mTextDecorationStyle |= BORDER_COLOR_FOREGROUND;
  }

  nsChangeHint CalcDifference(const nsStyleTextReset& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif
  static bool ForceCompare() { return false; }

  nsStyleCoord  mVerticalAlign;         // [reset] coord, percent, calc, enum (see nsStyleConsts.h)
  nsStyleTextOverflow mTextOverflow;    // [reset] enum, string

  PRUint8 mTextBlink;                   // [reset] see nsStyleConsts.h
  PRUint8 mTextDecorationLine;          // [reset] see nsStyleConsts.h
  PRUint8 mUnicodeBidi;                 // [reset] see nsStyleConsts.h
protected:
  PRUint8 mTextDecorationStyle;         // [reset] see nsStyleConsts.h

  nscolor mTextDecorationColor;         // [reset] the colors to use for a decoration lines, not used at currentColor
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
  static bool ForceCompare() { return false; }

  PRUint8 mTextAlign;                   // [inherited] see nsStyleConsts.h
  PRUint8 mTextAlignLast;               // [inherited] see nsStyleConsts.h
  PRUint8 mTextTransform;               // [inherited] see nsStyleConsts.h
  PRUint8 mWhiteSpace;                  // [inherited] see nsStyleConsts.h
  PRUint8 mWordWrap;                    // [inherited] see nsStyleConsts.h
  PRUint8 mHyphens;                     // [inherited] see nsStyleConsts.h
  PRUint8 mTextSizeAdjust;              // [inherited] see nsStyleConsts.h
  PRInt32 mTabSize;                     // [inherited] see nsStyleConsts.h

  nscoord mWordSpacing;                 // [inherited]
  nsStyleCoord  mLetterSpacing;         // [inherited] coord, normal
  nsStyleCoord  mLineHeight;            // [inherited] coord, factor, normal
  nsStyleCoord  mTextIndent;            // [inherited] coord, percent, calc

  nsRefPtr<nsCSSShadowArray> mTextShadow; // [inherited] NULL in case of a zero-length

  bool WhiteSpaceIsSignificant() const {
    return mWhiteSpace == NS_STYLE_WHITESPACE_PRE ||
           mWhiteSpace == NS_STYLE_WHITESPACE_PRE_WRAP;
  }

  bool NewlineIsSignificant() const {
    return mWhiteSpace == NS_STYLE_WHITESPACE_PRE ||
           mWhiteSpace == NS_STYLE_WHITESPACE_PRE_WRAP ||
           mWhiteSpace == NS_STYLE_WHITESPACE_PRE_LINE;
  }

  bool WhiteSpaceCanWrap() const {
    return mWhiteSpace == NS_STYLE_WHITESPACE_NORMAL ||
           mWhiteSpace == NS_STYLE_WHITESPACE_PRE_WRAP ||
           mWhiteSpace == NS_STYLE_WHITESPACE_PRE_LINE;
  }

  bool WordCanWrap() const {
    return WhiteSpaceCanWrap() && mWordWrap == NS_STYLE_WORDWRAP_BREAK_WORD;
  }
};

struct nsStyleVisibility {
  nsStyleVisibility(nsPresContext* aPresContext);
  nsStyleVisibility(const nsStyleVisibility& aVisibility);
  ~nsStyleVisibility() {
    MOZ_COUNT_DTOR(nsStyleVisibility);
  }

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
  static bool ForceCompare() { return false; }

  PRUint8 mDirection;                  // [inherited] see nsStyleConsts.h NS_STYLE_DIRECTION_*
  PRUint8   mVisible;                  // [inherited]
  PRUint8 mPointerEvents;              // [inherited] see nsStyleConsts.h

  bool IsVisible() const {
    return (mVisible == NS_STYLE_VISIBILITY_VISIBLE);
  }

  bool IsVisibleOrCollapsed() const {
    return ((mVisible == NS_STYLE_VISIBILITY_VISIBLE) ||
            (mVisible == NS_STYLE_VISIBILITY_COLLAPSE));
  }
};

struct nsTimingFunction {
  enum Type { Function, StepStart, StepEnd };

  explicit nsTimingFunction(PRInt32 aTimingFunctionType
                              = NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE)
  {
    AssignFromKeyword(aTimingFunctionType);
  }

  nsTimingFunction(float x1, float y1, float x2, float y2)
    : mType(Function)
  {
    mFunc.mX1 = x1;
    mFunc.mY1 = y1;
    mFunc.mX2 = x2;
    mFunc.mY2 = y2;
  }

  nsTimingFunction(Type aType, PRUint32 aSteps)
    : mType(aType)
  {
    NS_ABORT_IF_FALSE(mType == StepStart || mType == StepEnd, "wrong type");
    mSteps = aSteps;
  }

  nsTimingFunction(const nsTimingFunction& aOther)
  {
    *this = aOther;
  }

  Type mType;
  union {
    struct {
      float mX1;
      float mY1;
      float mX2;
      float mY2;
    } mFunc;
    PRUint32 mSteps;
  };

  nsTimingFunction&
  operator=(const nsTimingFunction& aOther)
  {
    if (&aOther == this)
      return *this;

    mType = aOther.mType;

    if (mType == Function) {
      mFunc.mX1 = aOther.mFunc.mX1;
      mFunc.mY1 = aOther.mFunc.mY1;
      mFunc.mX2 = aOther.mFunc.mX2;
      mFunc.mY2 = aOther.mFunc.mY2;
    } else {
      mSteps = aOther.mSteps;
    }

    return *this;
  }

  bool operator==(const nsTimingFunction& aOther) const
  {
    if (mType != aOther.mType) {
      return false;
    }
    if (mType == Function) {
      return mFunc.mX1 == aOther.mFunc.mX1 && mFunc.mY1 == aOther.mFunc.mY1 &&
             mFunc.mX2 == aOther.mFunc.mX2 && mFunc.mY2 == aOther.mFunc.mY2;
    }
    return mSteps == aOther.mSteps;
  }

  bool operator!=(const nsTimingFunction& aOther) const
  {
    return !(*this == aOther);
  }

private:
  void AssignFromKeyword(PRInt32 aTimingFunctionType);
};

struct nsTransition {
  nsTransition() { /* leaves uninitialized; see also SetInitialValues */ }
  explicit nsTransition(const nsTransition& aCopy);

  void SetInitialValues();

  // Delay and Duration are in milliseconds

  const nsTimingFunction& GetTimingFunction() const { return mTimingFunction; }
  float GetDelay() const { return mDelay; }
  float GetDuration() const { return mDuration; }
  nsCSSProperty GetProperty() const { return mProperty; }
  nsIAtom* GetUnknownProperty() const { return mUnknownProperty; }

  void SetTimingFunction(const nsTimingFunction& aTimingFunction)
    { mTimingFunction = aTimingFunction; }
  void SetDelay(float aDelay) { mDelay = aDelay; }
  void SetDuration(float aDuration) { mDuration = aDuration; }
  void SetProperty(nsCSSProperty aProperty)
    {
      NS_ASSERTION(aProperty != eCSSProperty_UNKNOWN, "invalid property");
      mProperty = aProperty;
    }
  void SetUnknownProperty(const nsAString& aUnknownProperty);
  void CopyPropertyFrom(const nsTransition& aOther)
    {
      mProperty = aOther.mProperty;
      mUnknownProperty = aOther.mUnknownProperty;
    }

  nsTimingFunction& TimingFunctionSlot() { return mTimingFunction; }

private:
  nsTimingFunction mTimingFunction;
  float mDuration;
  float mDelay;
  nsCSSProperty mProperty;
  nsCOMPtr<nsIAtom> mUnknownProperty; // used when mProperty is
                                      // eCSSProperty_UNKNOWN
};

struct nsAnimation {
  nsAnimation() { /* leaves uninitialized; see also SetInitialValues */ }
  explicit nsAnimation(const nsAnimation& aCopy);

  void SetInitialValues();

  // Delay and Duration are in milliseconds

  const nsTimingFunction& GetTimingFunction() const { return mTimingFunction; }
  float GetDelay() const { return mDelay; }
  float GetDuration() const { return mDuration; }
  const nsString& GetName() const { return mName; }
  PRUint8 GetDirection() const { return mDirection; }
  PRUint8 GetFillMode() const { return mFillMode; }
  PRUint8 GetPlayState() const { return mPlayState; }
  float GetIterationCount() const { return mIterationCount; }

  void SetTimingFunction(const nsTimingFunction& aTimingFunction)
    { mTimingFunction = aTimingFunction; }
  void SetDelay(float aDelay) { mDelay = aDelay; }
  void SetDuration(float aDuration) { mDuration = aDuration; }
  void SetName(const nsSubstring& aName) { mName = aName; }
  void SetDirection(PRUint8 aDirection) { mDirection = aDirection; }
  void SetFillMode(PRUint8 aFillMode) { mFillMode = aFillMode; }
  void SetPlayState(PRUint8 aPlayState) { mPlayState = aPlayState; }
  void SetIterationCount(float aIterationCount)
    { mIterationCount = aIterationCount; }

  nsTimingFunction& TimingFunctionSlot() { return mTimingFunction; }

private:
  nsTimingFunction mTimingFunction;
  float mDuration;
  float mDelay;
  nsString mName; // empty string for 'none'
  PRUint8 mDirection;
  PRUint8 mFillMode;
  PRUint8 mPlayState;
  float mIterationCount; // NS_IEEEPositiveInfinity() means infinite
};

struct nsStyleDisplay {
  nsStyleDisplay();
  nsStyleDisplay(const nsStyleDisplay& aOther);
  ~nsStyleDisplay() {
    MOZ_COUNT_DTOR(nsStyleDisplay);
  }

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
  static bool ForceCompare() { return true; }

  // We guarantee that if mBinding is non-null, so are mBinding->GetURI() and
  // mBinding->mOriginPrincipal.
  nsRefPtr<nsCSSValue::URL> mBinding;    // [reset]
  nsRect  mClip;                // [reset] offsets from upper-left border edge
  float   mOpacity;             // [reset]
  PRUint8 mDisplay;             // [reset] see nsStyleConsts.h NS_STYLE_DISPLAY_*
  PRUint8 mOriginalDisplay;     // [reset] saved mDisplay for position:absolute/fixed
                                //         and float:left/right; otherwise equal
                                //         to mDisplay
  PRUint8 mAppearance;          // [reset]
  PRUint8 mPosition;            // [reset] see nsStyleConsts.h
  PRUint8 mFloats;              // [reset] see nsStyleConsts.h NS_STYLE_FLOAT_*
  PRUint8 mOriginalFloats;      // [reset] saved mFloats for position:absolute/fixed;
                                //         otherwise equal to mFloats
  PRUint8 mBreakType;           // [reset] see nsStyleConsts.h NS_STYLE_CLEAR_*
  bool mBreakBefore;    // [reset]
  bool mBreakAfter;     // [reset]
  PRUint8 mOverflowX;           // [reset] see nsStyleConsts.h
  PRUint8 mOverflowY;           // [reset] see nsStyleConsts.h
  PRUint8 mResize;              // [reset] see nsStyleConsts.h
  PRUint8 mClipFlags;           // [reset] see nsStyleConsts.h
  PRUint8 mOrient;              // [reset] see nsStyleConsts.h

  // mSpecifiedTransform is the list of transform functions as
  // specified, or null to indicate there is no transform.  (inherit or
  // initial are replaced by an actual list of transform functions, or
  // null, as appropriate.) (owned by the style rule)
  PRUint8 mBackfaceVisibility;
  PRUint8 mTransformStyle;
  const nsCSSValueList *mSpecifiedTransform; // [reset]
  nsStyleCoord mTransformOrigin[3]; // [reset] percent, coord, calc, 3rd param is coord, calc only
  nsStyleCoord mChildPerspective; // [reset] coord
  nsStyleCoord mPerspectiveOrigin[2]; // [reset] percent, coord, calc

  nsAutoTArray<nsTransition, 1> mTransitions; // [reset]
  // The number of elements in mTransitions that are not from repeating
  // a list due to another property being longer.
  PRUint32 mTransitionTimingFunctionCount,
           mTransitionDurationCount,
           mTransitionDelayCount,
           mTransitionPropertyCount;

  nsAutoTArray<nsAnimation, 1> mAnimations; // [reset]
  // The number of elements in mAnimations that are not from repeating
  // a list due to another property being longer.
  PRUint32 mAnimationTimingFunctionCount,
           mAnimationDurationCount,
           mAnimationDelayCount,
           mAnimationNameCount,
           mAnimationDirectionCount,
           mAnimationFillModeCount,
           mAnimationPlayStateCount,
           mAnimationIterationCountCount;

  bool IsBlockInside() const {
    return NS_STYLE_DISPLAY_BLOCK == mDisplay ||
           NS_STYLE_DISPLAY_LIST_ITEM == mDisplay ||
           NS_STYLE_DISPLAY_INLINE_BLOCK == mDisplay;
    // Should TABLE_CELL and TABLE_CAPTION go here?  They have
    // block frames nested inside of them.
    // (But please audit all callers before changing.)
  }

  bool IsBlockOutside() const {
    return NS_STYLE_DISPLAY_BLOCK == mDisplay ||
           NS_STYLE_DISPLAY_LIST_ITEM == mDisplay ||
           NS_STYLE_DISPLAY_TABLE == mDisplay;
  }

  static bool IsDisplayTypeInlineOutside(PRUint8 aDisplay) {
    return NS_STYLE_DISPLAY_INLINE == aDisplay ||
           NS_STYLE_DISPLAY_INLINE_BLOCK == aDisplay ||
           NS_STYLE_DISPLAY_INLINE_TABLE == aDisplay ||
           NS_STYLE_DISPLAY_INLINE_BOX == aDisplay ||
           NS_STYLE_DISPLAY_INLINE_GRID == aDisplay ||
           NS_STYLE_DISPLAY_INLINE_STACK == aDisplay;
  }

  bool IsInlineOutside() const {
    return IsDisplayTypeInlineOutside(mDisplay);
  }

  bool IsOriginalDisplayInlineOutside() const {
    return IsDisplayTypeInlineOutside(mOriginalDisplay);
  }

  bool IsFloating() const {
    return NS_STYLE_FLOAT_NONE != mFloats;
  }

  bool IsAbsolutelyPositioned() const {return (NS_STYLE_POSITION_ABSOLUTE == mPosition) ||
                                                (NS_STYLE_POSITION_FIXED == mPosition);}

  /* Returns true if we're positioned or there's a transform in effect. */
  bool IsPositioned() const {
    return IsAbsolutelyPositioned() ||
      NS_STYLE_POSITION_RELATIVE == mPosition || HasTransform();
  }

  bool IsScrollableOverflow() const {
    // mOverflowX and mOverflowY always match when one of them is
    // NS_STYLE_OVERFLOW_VISIBLE or NS_STYLE_OVERFLOW_CLIP.
    return mOverflowX != NS_STYLE_OVERFLOW_VISIBLE &&
           mOverflowX != NS_STYLE_OVERFLOW_CLIP;
  }

  /* Returns whether the element has the -moz-transform property. */
  bool HasTransform() const {
    return mSpecifiedTransform != nsnull || 
           mTransformStyle == NS_STYLE_TRANSFORM_STYLE_PRESERVE_3D ||
           mBackfaceVisibility == NS_STYLE_BACKFACE_VISIBILITY_HIDDEN;
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
  static bool ForceCompare() { return false; }

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
  static bool ForceCompare() { return false; }

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
#ifdef DEBUG
  bool mImageTracked;
#endif

  nsStyleContentData()
    : mType(nsStyleContentType(0))
#ifdef DEBUG
    , mImageTracked(false)
#endif
  { mContent.mString = nsnull; }

  ~nsStyleContentData();
  nsStyleContentData& operator=(const nsStyleContentData& aOther);
  bool operator==(const nsStyleContentData& aOther) const;

  bool operator!=(const nsStyleContentData& aOther) const {
    return !(*this == aOther);
  }

  void TrackImage(nsPresContext* aContext);
  void UntrackImage(nsPresContext* aContext);

  void SetImage(imgIRequest* aRequest)
  {
    NS_ABORT_IF_FALSE(!mImageTracked,
                      "Setting a new image without untracking the old one!");
    NS_ABORT_IF_FALSE(mType == eStyleContentType_Image, "Wrong type!");
    NS_IF_ADDREF(mContent.mImage = aRequest);
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
  static bool ForceCompare() { return false; }

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
  void Destroy(nsPresContext* aContext);

  nsChangeHint CalcDifference(const nsStyleContent& aOther) const;
#ifdef DEBUG
  static nsChangeHint MaxDifference();
#endif
  static bool ForceCompare() { return false; }

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
  nsStyleContentData* mContents;
  nsStyleCounterData* mIncrements;
  nsStyleCounterData* mResets;

  PRUint32            mContentCount;
  PRUint32            mIncrementCount;
  PRUint32            mResetCount;
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
  static bool ForceCompare() { return false; }

  PRUint8   mUserSelect;      // [reset] (selection-style)
  PRUint8   mForceBrokenImageIcon; // [reset]  (0 if not forcing, otherwise forcing)
  PRUint8   mIMEMode;         // [reset]
  PRUint8   mWindowShadow;    // [reset]
};

struct nsCursorImage {
  bool mHaveHotspot;
  float mHotspotX, mHotspotY;

  nsCursorImage();
  nsCursorImage(const nsCursorImage& aOther);
  ~nsCursorImage();

  nsCursorImage& operator=(const nsCursorImage& aOther);
  /*
   * We hide mImage and force access through the getter and setter so that we
   * can lock the images we use. Cursor images are likely to be small, so we
   * don't care about discarding them. See bug 512260.
   * */
  void SetImage(imgIRequest *aImage) {
    if (mImage)
      mImage->UnlockImage();
    mImage = aImage;
    if (mImage)
      mImage->LockImage();
  }
  imgIRequest* GetImage() const {
    return mImage;
  }

private:
  nsCOMPtr<imgIRequest> mImage;
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
  static bool ForceCompare() { return false; }

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
  static bool ForceCompare() { return false; }

  float         mBoxFlex;               // [reset] see nsStyleConsts.h
  PRUint32      mBoxOrdinal;            // [reset] see nsStyleConsts.h
  PRUint8       mBoxAlign;              // [reset] see nsStyleConsts.h
  PRUint8       mBoxDirection;          // [reset] see nsStyleConsts.h
  PRUint8       mBoxOrient;             // [reset] see nsStyleConsts.h
  PRUint8       mBoxPack;               // [reset] see nsStyleConsts.h
  bool          mStretchStack;          // [reset] see nsStyleConsts.h
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
  static bool ForceCompare() { return false; }

  PRUint32     mColumnCount; // [reset] see nsStyleConsts.h
  nsStyleCoord mColumnWidth; // [reset] coord, auto
  nsStyleCoord mColumnGap;   // [reset] coord, normal

  nscolor      mColumnRuleColor;  // [reset]
  PRUint8      mColumnRuleStyle;  // [reset]
  PRUint8      mColumnFill;  // [reset] see nsStyleConsts.h

  // See https://bugzilla.mozilla.org/show_bug.cgi?id=271586#c43 for why
  // this is hard to replace with 'currentColor'.
  bool mColumnRuleColorIsForeground;

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

enum nsStyleSVGPaintType {
  eStyleSVGPaintType_None = 1,
  eStyleSVGPaintType_Color,
  eStyleSVGPaintType_Server
};

struct nsStyleSVGPaint
{
  union {
    nscolor mColor;
    nsIURI *mPaintServer;
  } mPaint;
  nsStyleSVGPaintType mType;
  nscolor mFallbackColor;

  nsStyleSVGPaint() : mType(nsStyleSVGPaintType(0)) { mPaint.mPaintServer = nsnull; }
  ~nsStyleSVGPaint();
  void SetType(nsStyleSVGPaintType aType);
  nsStyleSVGPaint& operator=(const nsStyleSVGPaint& aOther);
  bool operator==(const nsStyleSVGPaint& aOther) const;

  bool operator!=(const nsStyleSVGPaint& aOther) const {
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
  static bool ForceCompare() { return false; }

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
  PRUint8          mImageRendering;   // [inherited] see nsStyleConsts.h
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
  static bool ForceCompare() { return false; }

  nsCOMPtr<nsIURI> mClipPath;         // [reset]
  nsCOMPtr<nsIURI> mFilter;           // [reset]
  nsCOMPtr<nsIURI> mMask;             // [reset]
  nscolor          mStopColor;        // [reset]
  nscolor          mFloodColor;       // [reset]
  nscolor          mLightingColor;    // [reset]

  float            mStopOpacity;      // [reset]
  float            mFloodOpacity;     // [reset]

  PRUint8          mDominantBaseline; // [reset] see nsStyleConsts.h
};

#endif /* nsStyleStruct_h___ */
