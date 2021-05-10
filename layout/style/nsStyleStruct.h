/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * structs that contain the data provided by ComputedStyle, the
 * internal API for computed style data for an element
 */

#ifndef nsStyleStruct_h___
#define nsStyleStruct_h___

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/ServoStyleConstsInlines.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/UniquePtr.h"
#include "nsColor.h"
#include "nsCoord.h"
#include "nsMargin.h"
#include "nsFont.h"
#include "nsStyleAutoArray.h"
#include "nsStyleConsts.h"
#include "nsChangeHint.h"
#include "nsTimingFunction.h"
#include "nsTArray.h"
#include "imgIContainer.h"
#include "imgRequestProxy.h"
#include "CounterStyleManager.h"
#include <cstddef>  // offsetof()
#include "X11UndefineNone.h"

class nsIFrame;
class nsIURI;
class nsTextFrame;
struct nsStyleDisplay;
struct nsStyleVisibility;
namespace mozilla {
class ComputedStyle;

}  // namespace mozilla

namespace mozilla {

using Position = StylePosition;

template <>
inline bool StylePosition::HasPercent() const {
  return horizontal.HasPercent() || vertical.HasPercent();
}

/**
 * True if the effective background image position described by this depends on
 * the size of the corresponding frame.
 */
template <>
inline bool StylePosition::DependsOnPositioningAreaSize() const {
  return HasPercent();
}

template <>
inline Position Position::FromPercentage(float aPercent) {
  return {LengthPercentage::FromPercentage(aPercent),
          LengthPercentage::FromPercentage(aPercent)};
}

}  // namespace mozilla

struct MOZ_NEEDS_MEMMOVABLE_MEMBERS nsStyleFont {
  nsStyleFont(const nsStyleFont& aStyleFont);
  explicit nsStyleFont(const mozilla::dom::Document&);
  MOZ_COUNTED_DTOR(nsStyleFont)
  static constexpr bool kHasTriggerImageLoads = false;

  nsChangeHint CalcDifference(const nsStyleFont& aNewData) const;

  /**
   * Return a given size multiplied by the current text zoom factor (in
   * aPresContext).
   *
   * The size is allowed to be negative, but the caller is expected to deal with
   * negative results.
   */
  static mozilla::Length ZoomText(const mozilla::dom::Document&,
                                  mozilla::Length);

  nsFont mFont;

  // Our "computed size". Can be different from mFont.size which is our "actual
  // size" and is enforced to be >= the user's preferred min-size.  mFont.size
  // should be used for display purposes while mSize is the value to return in
  // getComputedStyle() for example.
  mozilla::NonNegativeLength mSize;

  // In stylo these three track whether the size is keyword-derived
  // and if so if it has been modified by a factor/offset
  float mFontSizeFactor;
  mozilla::Length mFontSizeOffset;
  mozilla::StyleFontSizeKeyword mFontSizeKeyword;

  mozilla::StyleGenericFontFamily mGenericID;

  // math-depth support (used for MathML scriptlevel)
  int8_t mMathDepth;
  // MathML  mathvariant support
  uint8_t mMathVariant;
  // math-style support (used for MathML displaystyle)
  uint8_t mMathStyle;

  // allow different min font-size for certain cases
  uint8_t mMinFontSizeRatio;  // percent * 100

  // was mLanguage set based on a lang attribute in the document?
  bool mExplicitLanguage;

  // should calls to ZoomText() and UnZoomText() be made to the font
  // size on this nsStyleFont? Also used to prevent SVG text from being
  // affected by minimum font size pref.
  bool mAllowZoomAndMinSize;

  // The value mSize would have had if scriptminsize had never been applied
  mozilla::NonNegativeLength mScriptUnconstrainedSize;
  mozilla::Length mScriptMinSize;
  float mScriptSizeMultiplier;
  RefPtr<nsAtom> mLanguage;
};

// TODO(emilio, bug 1564526): Evaluate whether this is still needed.
struct CachedBorderImageData {
  ~CachedBorderImageData() { PurgeCachedImages(); }

  // Caller are expected to ensure that the value of aSize is different from the
  // cached one since the method won't do the check.
  void SetCachedSVGViewportSize(const mozilla::Maybe<nsSize>& aSize) {
    mCachedSVGViewportSize = aSize;
  }

  const mozilla::Maybe<nsSize>& GetCachedSVGViewportSize() const {
    return mCachedSVGViewportSize;
  }

  void PurgeCachedImages();

  void SetSubImage(uint8_t aIndex, imgIContainer* aSubImage) {
    mSubImages.EnsureLengthAtLeast(aIndex + 1);
    mSubImages[aIndex] = aSubImage;
  }
  imgIContainer* GetSubImage(uint8_t aIndex) {
    return mSubImages.SafeElementAt(aIndex);
  }

  // These methods are used for the caller to caches the sub images created
  // during a border-image paint operation
  void PurgeCacheForViewportChange(
      const mozilla::Maybe<nsSize>& aSVGViewportSize,
      const bool aHasIntrinsicRatio);

 private:
  // If this is a SVG border-image, we save the size of the SVG viewport that
  // we used when rasterizing any cached border-image subimages. (The viewport
  // size matters for percent-valued sizes & positions in inner SVG doc).
  mozilla::Maybe<nsSize> mCachedSVGViewportSize;
  nsTArray<RefPtr<imgIContainer>> mSubImages;
};

struct nsStyleImageLayers {
  enum class LayerType : uint8_t { Background = 0, Mask };

  explicit nsStyleImageLayers(LayerType aType);
  nsStyleImageLayers(const nsStyleImageLayers& aSource);
  MOZ_COUNTED_DTOR(nsStyleImageLayers)

  struct Repeat {
    mozilla::StyleImageLayerRepeat mXRepeat, mYRepeat;

    // Initialize nothing
    Repeat() = default;

    bool IsInitialValue() const {
      return mXRepeat == mozilla::StyleImageLayerRepeat::Repeat &&
             mYRepeat == mozilla::StyleImageLayerRepeat::Repeat;
    }

    bool DependsOnPositioningAreaSize() const {
      return mXRepeat == mozilla::StyleImageLayerRepeat::Space ||
             mYRepeat == mozilla::StyleImageLayerRepeat::Space;
    }

    // Initialize to initial values
    void SetInitialValues() {
      mXRepeat = mozilla::StyleImageLayerRepeat::Repeat;
      mYRepeat = mozilla::StyleImageLayerRepeat::Repeat;
    }

    bool operator==(const Repeat& aOther) const {
      return mXRepeat == aOther.mXRepeat && mYRepeat == aOther.mYRepeat;
    }
    bool operator!=(const Repeat& aOther) const { return !(*this == aOther); }
  };

  struct Layer {
    typedef mozilla::StyleGeometryBox StyleGeometryBox;
    typedef mozilla::StyleImageLayerAttachment StyleImageLayerAttachment;
    typedef mozilla::StyleBackgroundSize StyleBackgroundSize;

    mozilla::StyleImage mImage;
    mozilla::Position mPosition;
    StyleBackgroundSize mSize;
    StyleGeometryBox mClip;
    MOZ_INIT_OUTSIDE_CTOR StyleGeometryBox mOrigin;

    // This property is used for background layer only.
    // For a mask layer, it should always be the initial value, which is
    // StyleImageLayerAttachment::Scroll.
    StyleImageLayerAttachment mAttachment;

    // This property is used for background layer only.
    // For a mask layer, it should always be the initial value, which is
    // StyleBlend::Normal.
    mozilla::StyleBlend mBlendMode;

    // This property is used for mask layer only.
    // For a background layer, it should always be the initial value, which is
    // StyleMaskComposite::Add.
    mozilla::StyleMaskComposite mComposite;

    // mask-only property. This property is used for mask layer only. For a
    // background layer, it should always be the initial value, which is
    // StyleMaskMode::MatchSource.
    mozilla::StyleMaskMode mMaskMode;

    Repeat mRepeat;

    // This constructor does not initialize mRepeat or mOrigin and Initialize()
    // must be called to do that.
    Layer();
    ~Layer();

    // Initialize mRepeat and mOrigin by specified layer type
    void Initialize(LayerType aType);

    void ResolveImage(mozilla::dom::Document& aDocument,
                      const Layer* aOldLayer) {
      mImage.ResolveImage(aDocument, aOldLayer ? &aOldLayer->mImage : nullptr);
    }

    // True if the rendering of this layer might change when the size
    // of the background positioning area changes.  This is true for any
    // non-solid-color background whose position or size depends on
    // the size of the positioning area.  It's also true for SVG images
    // whose root <svg> node has a viewBox.
    bool RenderingMightDependOnPositioningAreaSizeChange() const;

    // Compute the change hint required by changes in just this layer.
    nsChangeHint CalcDifference(const Layer& aNewLayer) const;

    // An equality operator that compares the images using URL-equality
    // rather than pointer-equality.
    bool operator==(const Layer& aOther) const;
    bool operator!=(const Layer& aOther) const { return !(*this == aOther); }
  };

  // The (positive) number of computed values of each property, since
  // the lengths of the lists are independent.
  uint32_t mAttachmentCount;
  uint32_t mClipCount;
  uint32_t mOriginCount;
  uint32_t mRepeatCount;
  uint32_t mPositionXCount;
  uint32_t mPositionYCount;
  uint32_t mImageCount;
  uint32_t mSizeCount;
  uint32_t mMaskModeCount;
  uint32_t mBlendModeCount;
  uint32_t mCompositeCount;

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
  nsStyleAutoArray<Layer> mLayers;

  const Layer& BottomLayer() const { return mLayers[mImageCount - 1]; }

  void ResolveImages(mozilla::dom::Document& aDocument,
                     const nsStyleImageLayers* aOldLayers) {
    for (uint32_t i = 0; i < mImageCount; ++i) {
      const Layer* oldLayer = (aOldLayers && aOldLayers->mLayers.Length() > i)
                                  ? &aOldLayers->mLayers[i]
                                  : nullptr;
      mLayers[i].ResolveImage(aDocument, oldLayer);
    }
  }

  // Fill unspecified layers by cycling through their values
  // till they all are of length aMaxItemCount
  void FillAllLayers(uint32_t aMaxItemCount);

  nsChangeHint CalcDifference(const nsStyleImageLayers& aNewLayers,
                              nsStyleImageLayers::LayerType aType) const;

  nsStyleImageLayers& operator=(const nsStyleImageLayers& aOther);
  nsStyleImageLayers& operator=(nsStyleImageLayers&& aOther) = default;
  bool operator==(const nsStyleImageLayers& aOther) const;

  static const nsCSSPropertyID kBackgroundLayerTable[];
  static const nsCSSPropertyID kMaskLayerTable[];

#define NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT(var_, layers_) \
  for (uint32_t var_ = (layers_).mImageCount; var_-- != 0;)
#define NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT_WITH_RANGE(var_, layers_,  \
                                                             start_, count_) \
  NS_ASSERTION(                                                              \
      (int32_t)(start_) >= 0 && (uint32_t)(start_) < (layers_).mImageCount,  \
      "Invalid layer start!");                                               \
  NS_ASSERTION((count_) > 0 && (count_) <= (start_) + 1,                     \
               "Invalid layer range!");                                      \
  for (uint32_t var_ = (start_) + 1;                                         \
       var_-- != (uint32_t)((start_) + 1 - (count_));)
};

struct MOZ_NEEDS_MEMMOVABLE_MEMBERS nsStyleBackground {
  explicit nsStyleBackground(const mozilla::dom::Document&);
  nsStyleBackground(const nsStyleBackground& aOther);
  ~nsStyleBackground();

  // Resolves and tracks the images in mImage.  Only called with a Servo-backed
  // style system, where those images must be resolved later than the OMT
  // nsStyleBackground constructor call.
  void TriggerImageLoads(mozilla::dom::Document&, const nsStyleBackground*);
  static constexpr bool kHasTriggerImageLoads = true;

  nsChangeHint CalcDifference(const nsStyleBackground& aNewData) const;

  // Return the background color as nscolor.
  nscolor BackgroundColor(const nsIFrame* aFrame) const;
  nscolor BackgroundColor(const mozilla::ComputedStyle* aStyle) const;

  // True if this background is completely transparent.
  bool IsTransparent(const nsIFrame* aFrame) const;
  bool IsTransparent(mozilla::ComputedStyle* aStyle) const;

  // We have to take slower codepaths for fixed background attachment,
  // but we don't want to do that when there's no image.
  // Not inline because it uses an nsCOMPtr<imgIRequest>
  // FIXME: Should be in nsStyleStructInlines.h.
  bool HasFixedBackground(nsIFrame* aFrame) const;

  // Checks to see if this has a non-empty image with "local" attachment.
  // This is defined in nsStyleStructInlines.h.
  inline bool HasLocalBackground() const;

  const nsStyleImageLayers::Layer& BottomLayer() const {
    return mImage.BottomLayer();
  }

  nsStyleImageLayers mImage;
  mozilla::StyleColor mBackgroundColor;
};

struct MOZ_NEEDS_MEMMOVABLE_MEMBERS nsStyleMargin {
  explicit nsStyleMargin(const mozilla::dom::Document&);
  nsStyleMargin(const nsStyleMargin& aMargin);
  MOZ_COUNTED_DTOR(nsStyleMargin)
  static constexpr bool kHasTriggerImageLoads = false;

  nsChangeHint CalcDifference(const nsStyleMargin& aNewData) const;

  bool GetMargin(nsMargin& aMargin) const {
    bool convertsToLength = mMargin.All(
        [](const auto& aLength) { return aLength.ConvertsToLength(); });

    if (!convertsToLength) {
      return false;
    }

    for (const auto side : mozilla::AllPhysicalSides()) {
      aMargin.Side(side) = mMargin.Get(side).AsLengthPercentage().ToLength();
    }
    return true;
  }

  nsMargin GetScrollMargin() const {
    return nsMargin(mScrollMargin.Get(mozilla::eSideTop).ToAppUnits(),
                    mScrollMargin.Get(mozilla::eSideRight).ToAppUnits(),
                    mScrollMargin.Get(mozilla::eSideBottom).ToAppUnits(),
                    mScrollMargin.Get(mozilla::eSideLeft).ToAppUnits());
  }

  // Return true if either the start or end side in the axis is 'auto'.
  // (defined in WritingModes.h since we need the full WritingMode type)
  inline bool HasBlockAxisAuto(mozilla::WritingMode aWM) const;
  inline bool HasInlineAxisAuto(mozilla::WritingMode aWM) const;
  inline bool HasAuto(mozilla::LogicalAxis, mozilla::WritingMode) const;

  mozilla::StyleRect<mozilla::LengthPercentageOrAuto> mMargin;
  mozilla::StyleRect<mozilla::StyleLength> mScrollMargin;
};

struct MOZ_NEEDS_MEMMOVABLE_MEMBERS nsStylePadding {
  explicit nsStylePadding(const mozilla::dom::Document&);
  nsStylePadding(const nsStylePadding& aPadding);
  MOZ_COUNTED_DTOR(nsStylePadding)
  static constexpr bool kHasTriggerImageLoads = false;

  nsChangeHint CalcDifference(const nsStylePadding& aNewData) const;

  mozilla::StyleRect<mozilla::NonNegativeLengthPercentage> mPadding;
  mozilla::StyleRect<mozilla::NonNegativeLengthPercentageOrAuto> mScrollPadding;

  inline bool IsWidthDependent() const {
    return !mPadding.All(
        [](const auto& aLength) { return aLength.ConvertsToLength(); });
  }

  bool GetPadding(nsMargin& aPadding) const {
    if (IsWidthDependent()) {
      return false;
    }

    for (const auto side : mozilla::AllPhysicalSides()) {
      // Clamp negative calc() to 0.
      aPadding.Side(side) = std::max(mPadding.Get(side).ToLength(), 0);
    }
    return true;
  }
};

// Border widths are rounded to the nearest-below integer number of pixels,
// but values between zero and one device pixels are always rounded up to
// one device pixel.
#define NS_ROUND_BORDER_TO_PIXELS(l, tpp) \
  ((l) == 0) ? 0 : std::max((tpp), (l) / (tpp) * (tpp))

// Returns if the given border style type is visible or not
static bool IsVisibleBorderStyle(mozilla::StyleBorderStyle aStyle) {
  return (aStyle != mozilla::StyleBorderStyle::None &&
          aStyle != mozilla::StyleBorderStyle::Hidden);
}

struct MOZ_NEEDS_MEMMOVABLE_MEMBERS nsStyleBorder {
  explicit nsStyleBorder(const mozilla::dom::Document&);
  nsStyleBorder(const nsStyleBorder& aBorder);
  ~nsStyleBorder();

  // Resolves and tracks mBorderImageSource.  Only called with a Servo-backed
  // style system, where those images must be resolved later than the OMT
  // nsStyleBorder constructor call.
  void TriggerImageLoads(mozilla::dom::Document&, const nsStyleBorder*);
  static constexpr bool kHasTriggerImageLoads = true;

  nsChangeHint CalcDifference(const nsStyleBorder& aNewData) const;

  // Return whether aStyle is a visible style.  Invisible styles cause
  // the relevant computed border width to be 0.
  // Note that this does *not* consider the effects of 'border-image':
  // if border-style is none, but there is a loaded border image,
  // HasVisibleStyle will be false even though there *is* a border.
  bool HasVisibleStyle(mozilla::Side aSide) const {
    return IsVisibleBorderStyle(mBorderStyle[aSide]);
  }

  // aBorderWidth is in twips
  void SetBorderWidth(mozilla::Side aSide, nscoord aBorderWidth) {
    nscoord roundedWidth =
        NS_ROUND_BORDER_TO_PIXELS(aBorderWidth, mTwipsPerPixel);
    mBorder.Side(aSide) = roundedWidth;
    if (HasVisibleStyle(aSide)) {
      mComputedBorder.Side(aSide) = roundedWidth;
    }
  }

  // Get the computed border (plus rounding).  This does consider the
  // effects of 'border-style: none', but does not consider
  // 'border-image'.
  const nsMargin& GetComputedBorder() const { return mComputedBorder; }

  bool HasBorder() const {
    return mComputedBorder != nsMargin(0, 0, 0, 0) ||
           !mBorderImageSource.IsNone();
  }

  // Get the actual border width for a particular side, in appunits.  Note that
  // this is zero if and only if there is no border to be painted for this
  // side.  That is, this value takes into account the border style and the
  // value is rounded to the nearest device pixel by NS_ROUND_BORDER_TO_PIXELS.
  nscoord GetComputedBorderWidth(mozilla::Side aSide) const {
    return GetComputedBorder().Side(aSide);
  }

  mozilla::StyleBorderStyle GetBorderStyle(mozilla::Side aSide) const {
    NS_ASSERTION(aSide <= mozilla::eSideLeft, "bad side");
    return mBorderStyle[aSide];
  }

  void SetBorderStyle(mozilla::Side aSide, mozilla::StyleBorderStyle aStyle) {
    NS_ASSERTION(aSide <= mozilla::eSideLeft, "bad side");
    mBorderStyle[aSide] = aStyle;
    mComputedBorder.Side(aSide) =
        (HasVisibleStyle(aSide) ? mBorder.Side(aSide) : 0);
  }

  inline bool IsBorderImageSizeAvailable() const {
    return mBorderImageSource.IsSizeAvailable();
  }

  nsMargin GetImageOutset() const;

  imgIRequest* GetBorderImageRequest() const {
    return mBorderImageSource.GetImageRequest();
  }

 public:
  mozilla::StyleBorderRadius mBorderRadius;  // coord, percent
  mozilla::StyleImage mBorderImageSource;
  mozilla::StyleBorderImageWidth mBorderImageWidth;
  mozilla::StyleNonNegativeLengthOrNumberRect mBorderImageOutset;
  mozilla::StyleBorderImageSlice mBorderImageSlice;  // factor, percent
  mozilla::StyleBorderImageRepeat mBorderImageRepeatH;
  mozilla::StyleBorderImageRepeat mBorderImageRepeatV;
  mozilla::StyleFloatEdge mFloatEdge;
  mozilla::StyleBoxDecorationBreak mBoxDecorationBreak;

 protected:
  mozilla::StyleBorderStyle mBorderStyle[4];  // StyleBorderStyle::*

 public:
  // the colors to use for a simple border.
  // not used for -moz-border-colors
  mozilla::StyleColor mBorderTopColor;
  mozilla::StyleColor mBorderRightColor;
  mozilla::StyleColor mBorderBottomColor;
  mozilla::StyleColor mBorderLeftColor;

  mozilla::StyleColor& BorderColorFor(mozilla::Side aSide) {
    switch (aSide) {
      case mozilla::eSideTop:
        return mBorderTopColor;
      case mozilla::eSideRight:
        return mBorderRightColor;
      case mozilla::eSideBottom:
        return mBorderBottomColor;
      case mozilla::eSideLeft:
        return mBorderLeftColor;
    }
    MOZ_ASSERT_UNREACHABLE("Unknown side");
    return mBorderTopColor;
  }

  const mozilla::StyleColor& BorderColorFor(mozilla::Side aSide) const {
    switch (aSide) {
      case mozilla::eSideTop:
        return mBorderTopColor;
      case mozilla::eSideRight:
        return mBorderRightColor;
      case mozilla::eSideBottom:
        return mBorderBottomColor;
      case mozilla::eSideLeft:
        return mBorderLeftColor;
    }
    MOZ_ASSERT_UNREACHABLE("Unknown side");
    return mBorderTopColor;
  }

  static mozilla::StyleColor nsStyleBorder::*BorderColorFieldFor(
      mozilla::Side aSide) {
    switch (aSide) {
      case mozilla::eSideTop:
        return &nsStyleBorder::mBorderTopColor;
      case mozilla::eSideRight:
        return &nsStyleBorder::mBorderRightColor;
      case mozilla::eSideBottom:
        return &nsStyleBorder::mBorderBottomColor;
      case mozilla::eSideLeft:
        return &nsStyleBorder::mBorderLeftColor;
    }
    MOZ_ASSERT_UNREACHABLE("Unknown side");
    return nullptr;
  }

 protected:
  // mComputedBorder holds the CSS2.1 computed border-width values.
  // In particular, these widths take into account the border-style
  // for the relevant side, and the values are rounded to the nearest
  // device pixel (which is not part of the definition of computed
  // values). The presence or absence of a border-image does not
  // affect border-width values.
  nsMargin mComputedBorder;

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
  nsMargin mBorder;

 private:
  nscoord mTwipsPerPixel;

  nsStyleBorder& operator=(const nsStyleBorder& aOther) = delete;
};

struct MOZ_NEEDS_MEMMOVABLE_MEMBERS nsStyleOutline {
  explicit nsStyleOutline(const mozilla::dom::Document&);
  nsStyleOutline(const nsStyleOutline& aOutline);
  MOZ_COUNTED_DTOR(nsStyleOutline)
  static constexpr bool kHasTriggerImageLoads = false;

  nsChangeHint CalcDifference(const nsStyleOutline& aNewData) const;

  mozilla::StyleBorderRadius mOutlineRadius;

  // This is the specified value of outline-width, but with length values
  // computed to absolute.  mActualOutlineWidth stores the outline-width
  // value used by layout.  (We must store mOutlineWidth for the same
  // style struct resolution reasons that we do nsStyleBorder::mBorder;
  // see that field's comment.)
  nscoord mOutlineWidth;
  mozilla::Length mOutlineOffset;
  mozilla::StyleColor mOutlineColor;
  mozilla::StyleOutlineStyle mOutlineStyle;

  nscoord GetOutlineWidth() const { return mActualOutlineWidth; }

  bool ShouldPaintOutline() const {
    if (mOutlineStyle.IsAuto()) {
      return true;
    }
    if (GetOutlineWidth() > 0) {
      MOZ_ASSERT(
          mOutlineStyle.AsBorderStyle() != mozilla::StyleBorderStyle::None,
          "outline-style: none implies outline-width of zero");
      return true;
    }
    return false;
  }

 protected:
  // The actual value of outline-width is the computed value (an absolute
  // length, forced to zero when outline-style is none) rounded to device
  // pixels.  This is the value used by layout.
  nscoord mActualOutlineWidth;
  nscoord mTwipsPerPixel;
};

struct MOZ_NEEDS_MEMMOVABLE_MEMBERS nsStyleList {
  explicit nsStyleList(const mozilla::dom::Document&);
  nsStyleList(const nsStyleList& aStyleList);
  ~nsStyleList();

 private:
  nsStyleList& operator=(const nsStyleList& aOther) = delete;

 public:
  void TriggerImageLoads(mozilla::dom::Document&, const nsStyleList*);
  static constexpr bool kHasTriggerImageLoads = true;

  nsChangeHint CalcDifference(const nsStyleList& aNewData,
                              const nsStyleDisplay& aOldDisplay) const;

  nsRect GetImageRegion() const {
    if (!mImageRegion.IsRect()) {
      return nsRect();
    }
    return mImageRegion.AsRect().ToLayoutRect(0);
  }

  already_AddRefed<nsIURI> GetListStyleImageURI() const;

  uint8_t mListStylePosition;

  mozilla::CounterStylePtr mCounterStyle;
  mozilla::StyleQuotes mQuotes;
  mozilla::StyleImage mListStyleImage;

  // the rect to use within an image.
  mozilla::StyleClipRectOrAuto mImageRegion;
  // true in an <ol reversed> scope.
  mozilla::StyleMozListReversed mMozListReversed;
};

struct MOZ_NEEDS_MEMMOVABLE_MEMBERS nsStylePage {
  using StylePageSize = mozilla::StylePageSize;
  nsStylePage(const nsStylePage& aOther) = default;
  nsStylePage& operator=(const nsStylePage& aOther) = default;
  explicit nsStylePage(const mozilla::dom::Document&)
      : mSize(StylePageSize::Auto()) {}

  static constexpr bool kHasTriggerImageLoads = false;
  nsChangeHint CalcDifference(const nsStylePage& aNewData) const;

  // page-size property.
  StylePageSize mSize;
};

struct MOZ_NEEDS_MEMMOVABLE_MEMBERS nsStylePosition {
  using LengthPercentageOrAuto = mozilla::LengthPercentageOrAuto;
  using Position = mozilla::Position;
  template <typename T>
  using StyleRect = mozilla::StyleRect<T>;
  using StyleSize = mozilla::StyleSize;
  using StyleMaxSize = mozilla::StyleMaxSize;
  using WritingMode = mozilla::WritingMode;
  using LogicalAxis = mozilla::LogicalAxis;
  using StyleImplicitGridTracks = mozilla::StyleImplicitGridTracks;
  using ComputedStyle = mozilla::ComputedStyle;
  using StyleAlignSelf = mozilla::StyleAlignSelf;
  using StyleJustifySelf = mozilla::StyleJustifySelf;

  explicit nsStylePosition(const mozilla::dom::Document&);
  nsStylePosition(const nsStylePosition& aOther);
  ~nsStylePosition();
  static constexpr bool kHasTriggerImageLoads = false;

  nsChangeHint CalcDifference(
      const nsStylePosition& aNewData,
      const nsStyleVisibility& aOldStyleVisibility) const;

  // Returns whether we need to compute an hypothetical position if we were
  // absolutely positioned.
  bool NeedsHypotheticalPositionIfAbsPos() const {
    return (mOffset.Get(mozilla::eSideRight).IsAuto() &&
            mOffset.Get(mozilla::eSideLeft).IsAuto()) ||
           (mOffset.Get(mozilla::eSideTop).IsAuto() &&
            mOffset.Get(mozilla::eSideBottom).IsAuto());
  }

  /**
   * Return the used value for 'align-self' given our parent ComputedStyle
   * (or null for the root).
   */
  StyleAlignSelf UsedAlignSelf(const ComputedStyle*) const;

  /**
   * Return the used value for 'justify-self' given our parent ComputedStyle
   * aParent (or null for the root).
   */
  StyleJustifySelf UsedJustifySelf(const ComputedStyle*) const;

  /**
   * Return the used value for 'justify/align-self' in aAxis given our parent
   * ComputedStyle aParent (or null for the root).
   * (defined in WritingModes.h since we need the full WritingMode type)
   */
  inline mozilla::StyleAlignFlags UsedSelfAlignment(
      LogicalAxis aAxis, const mozilla::ComputedStyle* aParent) const;

  /**
   * Return the used value for 'justify/align-content' in aAxis.
   * (defined in WritingModes.h since we need the full WritingMode type)
   */
  inline mozilla::StyleContentDistribution UsedContentAlignment(
      LogicalAxis aAxis) const;

  /**
   * Return the used value for 'align-tracks'/'justify-tracks' for a track
   * in the given axis.
   * (defined in WritingModes.h since we need the full LogicalAxis type)
   */
  inline mozilla::StyleContentDistribution UsedTracksAlignment(
      LogicalAxis aAxis, uint32_t aIndex) const;

  // Each entry has the same encoding as *-content, see below.
  mozilla::StyleAlignTracks mAlignTracks;
  mozilla::StyleJustifyTracks mJustifyTracks;

  Position mObjectPosition;
  StyleRect<LengthPercentageOrAuto> mOffset;
  StyleSize mWidth;
  StyleSize mMinWidth;
  StyleMaxSize mMaxWidth;
  StyleSize mHeight;
  StyleSize mMinHeight;
  StyleMaxSize mMaxHeight;
  mozilla::StyleFlexBasis mFlexBasis;
  StyleImplicitGridTracks mGridAutoColumns;
  StyleImplicitGridTracks mGridAutoRows;
  mozilla::StyleAspectRatio mAspectRatio;
  mozilla::StyleGridAutoFlow mGridAutoFlow;
  uint8_t mMasonryAutoFlow;  // NS_STYLE_MASONRY_*

  mozilla::StyleAlignContent mAlignContent;
  mozilla::StyleAlignItems mAlignItems;
  mozilla::StyleAlignSelf mAlignSelf;
  mozilla::StyleJustifyContent mJustifyContent;
  mozilla::StyleComputedJustifyItems mJustifyItems;
  mozilla::StyleJustifySelf mJustifySelf;
  mozilla::StyleFlexDirection mFlexDirection;
  mozilla::StyleFlexWrap mFlexWrap;
  mozilla::StyleObjectFit mObjectFit;
  mozilla::StyleBoxSizing mBoxSizing;
  int32_t mOrder;
  float mFlexGrow;
  float mFlexShrink;
  mozilla::StyleZIndex mZIndex;

  mozilla::StyleGridTemplateComponent mGridTemplateColumns;
  mozilla::StyleGridTemplateComponent mGridTemplateRows;
  mozilla::StyleGridTemplateAreas mGridTemplateAreas;

  mozilla::StyleGridLine mGridColumnStart;
  mozilla::StyleGridLine mGridColumnEnd;
  mozilla::StyleGridLine mGridRowStart;
  mozilla::StyleGridLine mGridRowEnd;
  mozilla::NonNegativeLengthPercentageOrNormal mColumnGap;
  mozilla::NonNegativeLengthPercentageOrNormal mRowGap;

  // Logical-coordinate accessors for width and height properties,
  // given a WritingMode value. The definitions of these methods are
  // found in WritingModes.h (after the WritingMode class is fully
  // declared).
  inline const StyleSize& ISize(WritingMode) const;
  inline const StyleSize& MinISize(WritingMode) const;
  inline const StyleMaxSize& MaxISize(WritingMode) const;
  inline const StyleSize& BSize(WritingMode) const;
  inline const StyleSize& MinBSize(WritingMode) const;
  inline const StyleMaxSize& MaxBSize(WritingMode) const;
  inline const StyleSize& Size(LogicalAxis, WritingMode) const;
  inline const StyleSize& MinSize(LogicalAxis, WritingMode) const;
  inline const StyleMaxSize& MaxSize(LogicalAxis, WritingMode) const;
  inline bool ISizeDependsOnContainer(WritingMode) const;
  inline bool MinISizeDependsOnContainer(WritingMode) const;
  inline bool MaxISizeDependsOnContainer(WritingMode) const;
  inline bool BSizeDependsOnContainer(WritingMode) const;
  inline bool MinBSizeDependsOnContainer(WritingMode) const;
  inline bool MaxBSizeDependsOnContainer(WritingMode) const;

 private:
  template <typename SizeOrMaxSize>
  static bool ISizeCoordDependsOnContainer(const SizeOrMaxSize& aCoord) {
    if (aCoord.IsLengthPercentage()) {
      return aCoord.AsLengthPercentage().HasPercent();
    }
    return aCoord.IsMozFitContent() || aCoord.IsMozAvailable();
  }

  template <typename SizeOrMaxSize>
  static bool BSizeCoordDependsOnContainer(const SizeOrMaxSize& aCoord) {
    return aCoord.IsLengthPercentage() &&
           aCoord.AsLengthPercentage().HasPercent();
  }
};

struct MOZ_NEEDS_MEMMOVABLE_MEMBERS nsStyleTextReset {
  explicit nsStyleTextReset(const mozilla::dom::Document&);
  nsStyleTextReset(const nsStyleTextReset& aOther);
  ~nsStyleTextReset();
  static constexpr bool kHasTriggerImageLoads = false;

  // Note the difference between this and
  // ComputedStyle::HasTextDecorationLines.
  bool HasTextDecorationLines() const {
    return mTextDecorationLine != mozilla::StyleTextDecorationLine::NONE &&
           mTextDecorationLine !=
               mozilla::StyleTextDecorationLine::COLOR_OVERRIDE;
  }

  nsChangeHint CalcDifference(const nsStyleTextReset& aNewData) const;

  mozilla::StyleTextOverflow mTextOverflow;

  mozilla::StyleTextDecorationLine mTextDecorationLine;
  uint8_t mTextDecorationStyle;  // NS_STYLE_TEXT_DECORATION_STYLE_*
  uint8_t mUnicodeBidi;          // NS_STYLE_UNICODE_BIDI_*
  nscoord mInitialLetterSink;    // 0 means normal
  float mInitialLetterSize;      // 0.0f means normal
  mozilla::StyleColor mTextDecorationColor;
  mozilla::StyleTextDecorationLength mTextDecorationThickness;
};

struct MOZ_NEEDS_MEMMOVABLE_MEMBERS nsStyleText {
  explicit nsStyleText(const mozilla::dom::Document&);
  nsStyleText(const nsStyleText& aOther);
  ~nsStyleText();
  static constexpr bool kHasTriggerImageLoads = false;

  nsChangeHint CalcDifference(const nsStyleText& aNewData) const;

  mozilla::StyleRGBA mColor;
  mozilla::StyleTextTransform mTextTransform;
  mozilla::StyleTextAlign mTextAlign;
  mozilla::StyleTextAlignLast mTextAlignLast;
  mozilla::StyleTextJustify mTextJustify;
  mozilla::StyleWhiteSpace mWhiteSpace;
  mozilla::StyleLineBreak mLineBreak = mozilla::StyleLineBreak::Auto;

 private:
  mozilla::StyleWordBreak mWordBreak = mozilla::StyleWordBreak::Normal;
  mozilla::StyleOverflowWrap mOverflowWrap = mozilla::StyleOverflowWrap::Normal;

 public:
  mozilla::StyleHyphens mHyphens;
  mozilla::StyleRubyAlign mRubyAlign;
  mozilla::StyleRubyPosition mRubyPosition;
  mozilla::StyleTextSizeAdjust mTextSizeAdjust;
  uint8_t mTextCombineUpright;  // NS_STYLE_TEXT_COMBINE_UPRIGHT_*
  mozilla::StyleMozControlCharacterVisibility mMozControlCharacterVisibility;
  uint8_t mTextEmphasisPosition;  // NS_STYLE_TEXT_EMPHASIS_POSITION_*
  mozilla::StyleTextRendering mTextRendering;
  mozilla::StyleColor mTextEmphasisColor;
  mozilla::StyleColor mWebkitTextFillColor;
  mozilla::StyleColor mWebkitTextStrokeColor;

  mozilla::StyleNonNegativeLengthOrNumber mMozTabSize;
  mozilla::LengthPercentage mWordSpacing;
  mozilla::StyleLetterSpacing mLetterSpacing;
  mozilla::StyleLineHeight mLineHeight;
  mozilla::LengthPercentage mTextIndent;

  mozilla::LengthPercentageOrAuto mTextUnderlineOffset;
  mozilla::StyleTextDecorationSkipInk mTextDecorationSkipInk;
  mozilla::StyleTextUnderlinePosition mTextUnderlinePosition;

  nscoord mWebkitTextStrokeWidth;  // coord

  mozilla::StyleArcSlice<mozilla::StyleSimpleShadow> mTextShadow;
  mozilla::StyleTextEmphasisStyle mTextEmphasisStyle;

  mozilla::StyleWordBreak EffectiveWordBreak() const {
    if (mWordBreak == mozilla::StyleWordBreak::BreakWord) {
      return mozilla::StyleWordBreak::Normal;
    }
    return mWordBreak;
  }

  mozilla::StyleOverflowWrap EffectiveOverflowWrap() const {
    if (mWordBreak == mozilla::StyleWordBreak::BreakWord) {
      return mozilla::StyleOverflowWrap::Anywhere;
    }
    return mOverflowWrap;
  }

  bool WhiteSpaceIsSignificant() const {
    return mWhiteSpace == mozilla::StyleWhiteSpace::Pre ||
           mWhiteSpace == mozilla::StyleWhiteSpace::PreWrap ||
           mWhiteSpace == mozilla::StyleWhiteSpace::BreakSpaces ||
           mWhiteSpace == mozilla::StyleWhiteSpace::PreSpace;
  }

  bool WhiteSpaceCanHangOrVisuallyCollapse() const {
    // This was originally expressed in nsTextFrame in terms of:
    //   mWhiteSpace != StyleWhiteSpace::BreakSpaces &&
    //       WhiteSpaceCanWrapStyle() &&
    //       WhiteSpaceIsSignificant()
    // which simplifies to:
    return mWhiteSpace == mozilla::StyleWhiteSpace::PreWrap;
  }

  bool NewlineIsSignificantStyle() const {
    return mWhiteSpace == mozilla::StyleWhiteSpace::Pre ||
           mWhiteSpace == mozilla::StyleWhiteSpace::PreWrap ||
           mWhiteSpace == mozilla::StyleWhiteSpace::BreakSpaces ||
           mWhiteSpace == mozilla::StyleWhiteSpace::PreLine;
  }

  bool WhiteSpaceOrNewlineIsSignificant() const {
    return mWhiteSpace == mozilla::StyleWhiteSpace::Pre ||
           mWhiteSpace == mozilla::StyleWhiteSpace::PreWrap ||
           mWhiteSpace == mozilla::StyleWhiteSpace::BreakSpaces ||
           mWhiteSpace == mozilla::StyleWhiteSpace::PreLine ||
           mWhiteSpace == mozilla::StyleWhiteSpace::PreSpace;
  }

  bool TabIsSignificant() const {
    return mWhiteSpace == mozilla::StyleWhiteSpace::Pre ||
           mWhiteSpace == mozilla::StyleWhiteSpace::PreWrap ||
           mWhiteSpace == mozilla::StyleWhiteSpace::BreakSpaces;
  }

  bool WhiteSpaceCanWrapStyle() const {
    return mWhiteSpace == mozilla::StyleWhiteSpace::Normal ||
           mWhiteSpace == mozilla::StyleWhiteSpace::PreWrap ||
           mWhiteSpace == mozilla::StyleWhiteSpace::BreakSpaces ||
           mWhiteSpace == mozilla::StyleWhiteSpace::PreLine;
  }

  bool WordCanWrapStyle() const {
    if (!WhiteSpaceCanWrapStyle()) {
      return false;
    }
    auto owrap = EffectiveOverflowWrap();
    return owrap == mozilla::StyleOverflowWrap::BreakWord ||
           owrap == mozilla::StyleOverflowWrap::Anywhere;
  }

  bool HasEffectiveTextEmphasis() const {
    if (mTextEmphasisStyle.IsNone()) {
      return false;
    }
    if (mTextEmphasisStyle.IsString() &&
        mTextEmphasisStyle.AsString().AsString().IsEmpty()) {
      return false;
    }
    return true;
  }

  mozilla::StyleTextAlign TextAlignForLastLine() const {
    switch (mTextAlignLast) {
      case mozilla::StyleTextAlignLast::Auto:
        // 'text-align-last: auto' is equivalent to the value of the
        // 'text-align' property except when 'text-align' is set to 'justify',
        // in which case it is 'justify' when 'text-justify' is 'distribute' and
        // 'start' otherwise.
        //
        // XXX: the code below will have to change when we implement
        // text-justify
        if (mTextAlign == mozilla::StyleTextAlign::Justify) {
          return mozilla::StyleTextAlign::Start;
        }
        return mTextAlign;
      case mozilla::StyleTextAlignLast::Center:
        return mozilla::StyleTextAlign::Center;
      case mozilla::StyleTextAlignLast::Start:
        return mozilla::StyleTextAlign::Start;
      case mozilla::StyleTextAlignLast::End:
        return mozilla::StyleTextAlign::End;
      case mozilla::StyleTextAlignLast::Left:
        return mozilla::StyleTextAlign::Left;
      case mozilla::StyleTextAlignLast::Right:
        return mozilla::StyleTextAlign::Right;
      case mozilla::StyleTextAlignLast::Justify:
        return mozilla::StyleTextAlign::Justify;
    }
    return mozilla::StyleTextAlign::Start;
  }

  bool HasWebkitTextStroke() const { return mWebkitTextStrokeWidth > 0; }

  bool HasTextShadow() const { return !mTextShadow.IsEmpty(); }

  // The aContextFrame argument on each of these is the frame this
  // style struct is for.  If the frame is for SVG text or inside ruby,
  // the return value will be massaged to be something that makes sense
  // for those cases.
  inline bool NewlineIsSignificant(const nsTextFrame* aContextFrame) const;
  inline bool WhiteSpaceCanWrap(const nsIFrame* aContextFrame) const;
  inline bool WordCanWrap(const nsIFrame* aContextFrame) const;

  mozilla::LogicalSide TextEmphasisSide(mozilla::WritingMode aWM) const;
};

struct MOZ_NEEDS_MEMMOVABLE_MEMBERS nsStyleVisibility {
  explicit nsStyleVisibility(const mozilla::dom::Document&);
  nsStyleVisibility(const nsStyleVisibility& aVisibility);
  MOZ_COUNTED_DTOR(nsStyleVisibility)
  static constexpr bool kHasTriggerImageLoads = false;

  nsChangeHint CalcDifference(const nsStyleVisibility& aNewData) const;

  mozilla::StyleImageOrientation mImageOrientation;
  mozilla::StyleDirection mDirection;
  mozilla::StyleVisibility mVisible;
  mozilla::StyleImageRendering mImageRendering;
  mozilla::StyleWritingModeProperty mWritingMode;
  mozilla::StyleTextOrientation mTextOrientation;
  mozilla::StyleColorAdjust mColorAdjust;

  bool IsVisible() const {
    return mVisible == mozilla::StyleVisibility::Visible;
  }

  bool IsVisibleOrCollapsed() const {
    return mVisible == mozilla::StyleVisibility::Visible ||
           mVisible == mozilla::StyleVisibility::Collapse;
  }
};

namespace mozilla {

inline StyleTextTransform StyleTextTransform::None() {
  return StyleTextTransform{StyleTextTransformCase::None,
                            StyleTextTransformOther()};
}

inline bool StyleTextTransform::IsNone() const { return *this == None(); }

// Note that IsAuto() does not exclude the possibility that `left` or `right`
// is set; it refers only to behavior in horizontal typographic mode.
inline bool StyleTextUnderlinePosition::IsAuto() const {
  return !(*this & (StyleTextUnderlinePosition::FROM_FONT |
                    StyleTextUnderlinePosition::UNDER));
}
inline bool StyleTextUnderlinePosition::IsFromFont() const {
  return bool(*this & StyleTextUnderlinePosition::FROM_FONT);
}
inline bool StyleTextUnderlinePosition::IsUnder() const {
  return bool(*this & StyleTextUnderlinePosition::UNDER);
}
inline bool StyleTextUnderlinePosition::IsLeft() const {
  return bool(*this & StyleTextUnderlinePosition::LEFT);
}
inline bool StyleTextUnderlinePosition::IsRight() const {
  return bool(*this & StyleTextUnderlinePosition::RIGHT);
}

struct StyleTransition {
  StyleTransition() { /* leaves uninitialized; see also SetInitialValues */
  }
  explicit StyleTransition(const StyleTransition& aCopy);

  void SetInitialValues();

  // Delay and Duration are in milliseconds

  const nsTimingFunction& GetTimingFunction() const { return mTimingFunction; }
  float GetDelay() const { return mDelay; }
  float GetDuration() const { return mDuration; }
  nsCSSPropertyID GetProperty() const { return mProperty; }
  nsAtom* GetUnknownProperty() const { return mUnknownProperty; }

  bool operator==(const StyleTransition& aOther) const;
  bool operator!=(const StyleTransition& aOther) const {
    return !(*this == aOther);
  }

 private:
  nsTimingFunction mTimingFunction;
  float mDuration;
  float mDelay;
  nsCSSPropertyID mProperty;
  RefPtr<nsAtom> mUnknownProperty;  // used when mProperty is
                                    // eCSSProperty_UNKNOWN or
                                    // eCSSPropertyExtra_variable
};

struct StyleAnimation {
  StyleAnimation() { /* leaves uninitialized; see also SetInitialValues */
  }
  explicit StyleAnimation(const StyleAnimation& aCopy);

  void SetInitialValues();

  // Delay and Duration are in milliseconds

  const nsTimingFunction& GetTimingFunction() const { return mTimingFunction; }
  float GetDelay() const { return mDelay; }
  float GetDuration() const { return mDuration; }
  nsAtom* GetName() const { return mName; }
  dom::PlaybackDirection GetDirection() const { return mDirection; }
  dom::FillMode GetFillMode() const { return mFillMode; }
  StyleAnimationPlayState GetPlayState() const { return mPlayState; }
  float GetIterationCount() const { return mIterationCount; }

  void SetName(already_AddRefed<nsAtom> aName) { mName = aName; }
  void SetName(nsAtom* aName) { mName = aName; }

  bool operator==(const StyleAnimation& aOther) const;
  bool operator!=(const StyleAnimation& aOther) const {
    return !(*this == aOther);
  }

 private:
  nsTimingFunction mTimingFunction;
  float mDuration;
  float mDelay;
  RefPtr<nsAtom> mName;  // nsGkAtoms::_empty for 'none'
  dom::PlaybackDirection mDirection;
  dom::FillMode mFillMode;
  StyleAnimationPlayState mPlayState;
  float mIterationCount;  // mozilla::PositiveInfinity<float>() means infinite
};

}  // namespace mozilla

struct MOZ_NEEDS_MEMMOVABLE_MEMBERS nsStyleDisplay {
  typedef mozilla::StyleGeometryBox StyleGeometryBox;

  explicit nsStyleDisplay(const mozilla::dom::Document&);
  nsStyleDisplay(const nsStyleDisplay& aOther);
  ~nsStyleDisplay();

  void TriggerImageLoads(mozilla::dom::Document&, const nsStyleDisplay*);
  static constexpr bool kHasTriggerImageLoads = true;

  nsChangeHint CalcDifference(const nsStyleDisplay& aNewData,
                              const nsStylePosition& aOldPosition) const;

  nsStyleAutoArray<mozilla::StyleTransition> mTransitions;
  // The number of elements in mTransitions that are not from repeating
  // a list due to another property being longer.
  uint32_t mTransitionTimingFunctionCount;
  uint32_t mTransitionDurationCount;
  uint32_t mTransitionDelayCount;
  uint32_t mTransitionPropertyCount;
  nsStyleAutoArray<mozilla::StyleAnimation> mAnimations;
  // The number of elements in mAnimations that are not from repeating
  // a list due to another property being longer.
  uint32_t mAnimationTimingFunctionCount;
  uint32_t mAnimationDurationCount;
  uint32_t mAnimationDelayCount;
  uint32_t mAnimationNameCount;
  uint32_t mAnimationDirectionCount;
  uint32_t mAnimationFillModeCount;
  uint32_t mAnimationPlayStateCount;
  uint32_t mAnimationIterationCountCount;

  mozilla::StyleWillChange mWillChange;
  mozilla::StyleDisplay mDisplay;
  mozilla::StyleDisplay mOriginalDisplay;  // saved mDisplay for
                                           //         position:absolute/fixed
                                           //         and float:left/right;
                                           //         otherwise equal to
                                           //         mDisplay
  mozilla::StyleContain mContain;

 private:
  mozilla::StyleAppearance mAppearance;

 public:
  mozilla::StyleAppearance mDefaultAppearance;
  mozilla::StylePositionProperty mPosition;

  mozilla::StyleFloat mFloat;
  mozilla::StyleClear mBreakType;
  mozilla::StyleBreakWithin mBreakInside;
  mozilla::StyleBreakBetween mBreakBefore;
  mozilla::StyleBreakBetween mBreakAfter;
  mozilla::StyleOverflow mOverflowX;
  mozilla::StyleOverflow mOverflowY;
  mozilla::StyleOverflowClipBox mOverflowClipBoxBlock;
  mozilla::StyleOverflowClipBox mOverflowClipBoxInline;
  mozilla::StyleResize mResize;
  mozilla::StyleOrient mOrient;
  mozilla::StyleIsolation mIsolation;
  mozilla::StyleTopLayer mTopLayer;

  mozilla::StyleTouchAction mTouchAction;
  mozilla::StyleScrollBehavior mScrollBehavior;
  mozilla::StyleOverscrollBehavior mOverscrollBehaviorX;
  mozilla::StyleOverscrollBehavior mOverscrollBehaviorY;
  mozilla::StyleOverflowAnchor mOverflowAnchor;
  mozilla::StyleScrollSnapAlign mScrollSnapAlign;
  mozilla::StyleScrollSnapType mScrollSnapType;
  uint32_t mLineClamp;

  mozilla::StyleTransform mTransform;
  mozilla::StyleRotate mRotate;
  mozilla::StyleTranslate mTranslate;
  mozilla::StyleScale mScale;

  mozilla::StyleBackfaceVisibility mBackfaceVisibility;
  mozilla::StyleTransformStyle mTransformStyle;
  StyleGeometryBox mTransformBox;

  mozilla::StyleOffsetPath mOffsetPath;
  mozilla::LengthPercentage mOffsetDistance;
  mozilla::StyleOffsetRotate mOffsetRotate;
  mozilla::StylePositionOrAuto mOffsetAnchor;

  mozilla::StyleTransformOrigin mTransformOrigin;
  mozilla::StylePerspective mChildPerspective;
  mozilla::Position mPerspectiveOrigin;

  mozilla::StyleVerticalAlign mVerticalAlign;

  nsCSSPropertyID GetTransitionProperty(uint32_t aIndex) const {
    return mTransitions[aIndex % mTransitionPropertyCount].GetProperty();
  }
  float GetTransitionDelay(uint32_t aIndex) const {
    return mTransitions[aIndex % mTransitionDelayCount].GetDelay();
  }
  float GetTransitionDuration(uint32_t aIndex) const {
    return mTransitions[aIndex % mTransitionDurationCount].GetDuration();
  }
  const nsTimingFunction& GetTransitionTimingFunction(uint32_t aIndex) const {
    return mTransitions[aIndex % mTransitionTimingFunctionCount]
        .GetTimingFunction();
  }
  float GetTransitionCombinedDuration(uint32_t aIndex) const {
    // https://drafts.csswg.org/css-transitions/#transition-combined-duration
    return std::max(
               mTransitions[aIndex % mTransitionDurationCount].GetDuration(),
               0.0f) +
           mTransitions[aIndex % mTransitionDelayCount].GetDelay();
  }

  nsAtom* GetAnimationName(uint32_t aIndex) const {
    return mAnimations[aIndex % mAnimationNameCount].GetName();
  }
  float GetAnimationDelay(uint32_t aIndex) const {
    return mAnimations[aIndex % mAnimationDelayCount].GetDelay();
  }
  float GetAnimationDuration(uint32_t aIndex) const {
    return mAnimations[aIndex % mAnimationDurationCount].GetDuration();
  }
  mozilla::dom::PlaybackDirection GetAnimationDirection(uint32_t aIndex) const {
    return mAnimations[aIndex % mAnimationDirectionCount].GetDirection();
  }
  mozilla::dom::FillMode GetAnimationFillMode(uint32_t aIndex) const {
    return mAnimations[aIndex % mAnimationFillModeCount].GetFillMode();
  }
  mozilla::StyleAnimationPlayState GetAnimationPlayState(
      uint32_t aIndex) const {
    return mAnimations[aIndex % mAnimationPlayStateCount].GetPlayState();
  }
  float GetAnimationIterationCount(uint32_t aIndex) const {
    return mAnimations[aIndex % mAnimationIterationCountCount]
        .GetIterationCount();
  }
  const nsTimingFunction& GetAnimationTimingFunction(uint32_t aIndex) const {
    return mAnimations[aIndex % mAnimationTimingFunctionCount]
        .GetTimingFunction();
  }

  // The threshold used for extracting a shape from shape-outside: <image>.
  float mShapeImageThreshold = 0.0f;

  // The margin around a shape-outside: <image>.
  mozilla::NonNegativeLengthPercentage mShapeMargin;

  mozilla::StyleShapeOutside mShapeOutside;

  bool HasAppearance() const {
    return EffectiveAppearance() != mozilla::StyleAppearance::None;
  }

  mozilla::StyleAppearance EffectiveAppearance() const {
    switch (mAppearance) {
      case mozilla::StyleAppearance::Auto:
      case mozilla::StyleAppearance::Button:
      case mozilla::StyleAppearance::Searchfield:
      case mozilla::StyleAppearance::Textarea:
      case mozilla::StyleAppearance::Checkbox:
      case mozilla::StyleAppearance::Radio:
      case mozilla::StyleAppearance::Menulist:
      case mozilla::StyleAppearance::Listbox:
      case mozilla::StyleAppearance::Meter:
      case mozilla::StyleAppearance::ProgressBar:
        // These are all the values that behave like `auto`.
        return mDefaultAppearance;
      case mozilla::StyleAppearance::Textfield:
        // `appearance: textfield` should behave like `auto` on all elements
        // except <input type=search> elements, which we identify using the
        // internal -moz-default-appearance property.  (In the browser chrome
        // we have some other elements that set `-moz-default-appearance:
        // searchfield`, but not in content documents.)
        if (mDefaultAppearance == mozilla::StyleAppearance::Searchfield) {
          return mAppearance;
        }
        // We also need to support `appearance: textfield` on <input
        // type=number>, since that is the only way in Gecko to disable the
        // spinners.
        if (mDefaultAppearance == mozilla::StyleAppearance::NumberInput) {
          return mAppearance;
        }
        return mDefaultAppearance;
      case mozilla::StyleAppearance::MenulistButton:
        // `appearance: menulist-button` should behave like `auto` on all
        // elements except for drop down selects, but since we have very little
        // difference between menulist and menulist-button handling, we don't
        // bother.
        return mDefaultAppearance;
      default:
        return mAppearance;
    }
  }

  static mozilla::StyleDisplayOutside DisplayOutside(
      mozilla::StyleDisplay aDisplay) {
    return mozilla::StyleDisplayOutside(
        (uint16_t(aDisplay) >> mozilla::STYLE_DISPLAY_INSIDE_BITS) &
        uint16_t(((1 << mozilla::STYLE_DISPLAY_OUTSIDE_BITS) - 1)));
  }
  mozilla::StyleDisplayOutside DisplayOutside() const {
    return DisplayOutside(mDisplay);
  }

  static mozilla::StyleDisplayInside DisplayInside(
      mozilla::StyleDisplay aDisplay) {
    return mozilla::StyleDisplayInside(
        uint16_t(aDisplay) &
        uint16_t(((1 << mozilla::STYLE_DISPLAY_INSIDE_BITS) - 1)));
  }
  mozilla::StyleDisplayInside DisplayInside() const {
    return DisplayInside(mDisplay);
  }

  static bool IsListItem(mozilla::StyleDisplay aDisplay) {
    return !!(uint16_t(aDisplay) & mozilla::STYLE_DISPLAY_LIST_ITEM_BIT);
  }
  bool IsListItem() const { return IsListItem(mDisplay); }

  // Whether display is `inline` or `inline list-item`.
  static bool IsInlineFlow(mozilla::StyleDisplay aDisplay) {
    return DisplayOutside(aDisplay) == mozilla::StyleDisplayOutside::Inline &&
           DisplayInside(aDisplay) == mozilla::StyleDisplayInside::Flow;
  }

  bool IsInlineFlow() const { return IsInlineFlow(mDisplay); }

  bool IsInlineInsideStyle() const {
    auto inside = DisplayInside();
    return IsInlineFlow() || inside == mozilla::StyleDisplayInside::Ruby ||
           inside == mozilla::StyleDisplayInside::RubyBase ||
           inside == mozilla::StyleDisplayInside::RubyBaseContainer ||
           inside == mozilla::StyleDisplayInside::RubyText ||
           inside == mozilla::StyleDisplayInside::RubyTextContainer;
  }

  bool IsBlockOutsideStyle() const {
    return DisplayOutside() == mozilla::StyleDisplayOutside::Block;
  }

  static bool IsDisplayTypeInlineOutside(mozilla::StyleDisplay aDisplay) {
    auto outside = DisplayOutside(aDisplay);
    if (outside == mozilla::StyleDisplayOutside::Inline) {
      return true;
    }
    // just an optimization for the common case:
    if (outside == mozilla::StyleDisplayOutside::Block) {
      return false;
    }
    return mozilla::StyleDisplay::MozInlineBox == aDisplay ||
           mozilla::StyleDisplay::RubyBase == aDisplay ||
           mozilla::StyleDisplay::RubyBaseContainer == aDisplay ||
           mozilla::StyleDisplay::RubyText == aDisplay ||
           mozilla::StyleDisplay::RubyTextContainer == aDisplay;
  }

  bool IsInlineOutsideStyle() const {
    return IsDisplayTypeInlineOutside(mDisplay);
  }

  bool IsOriginalDisplayInlineOutside() const {
    return IsDisplayTypeInlineOutside(mOriginalDisplay);
  }

  bool IsInnerTableStyle() const {
    return DisplayOutside() == mozilla::StyleDisplayOutside::InternalTable;
  }

  bool IsInternalTableStyleExceptCell() const {
    return IsInnerTableStyle() && mozilla::StyleDisplay::TableCell != mDisplay;
  }

  bool IsXULDisplayStyle() const {
    // -moz-{inline-}box is XUL, unless we're emulating it with flexbox.
    if (!mozilla::StaticPrefs::layout_css_emulate_moz_box_with_flex() &&
        DisplayInside() == mozilla::StyleDisplayInside::MozBox) {
      return true;
    }

#ifdef MOZ_XUL
    return DisplayOutside() == mozilla::StyleDisplayOutside::XUL;
#else
    return false;
#endif
  }

  bool IsFloatingStyle() const { return mozilla::StyleFloat::None != mFloat; }

  bool IsPositionedStyle() const {
    return mPosition != mozilla::StylePositionProperty::Static ||
           (mWillChange.bits & mozilla::StyleWillChangeBits::POSITION);
  }

  bool IsAbsolutelyPositionedStyle() const {
    return mozilla::StylePositionProperty::Absolute == mPosition ||
           mozilla::StylePositionProperty::Fixed == mPosition;
  }

  bool IsRelativelyPositionedStyle() const {
    return mozilla::StylePositionProperty::Relative == mPosition ||
           mozilla::StylePositionProperty::Sticky == mPosition;
  }
  bool IsStickyPositionedStyle() const {
    return mozilla::StylePositionProperty::Sticky == mPosition;
  }
  bool IsPositionForcingStackingContext() const {
    return mozilla::StylePositionProperty::Sticky == mPosition ||
           mozilla::StylePositionProperty::Fixed == mPosition;
  }

  static bool IsRubyDisplayType(mozilla::StyleDisplay aDisplay) {
    return DisplayInside(aDisplay) == mozilla::StyleDisplayInside::Ruby ||
           IsInternalRubyDisplayType(aDisplay);
  }

  static bool IsInternalRubyDisplayType(mozilla::StyleDisplay aDisplay) {
    return mozilla::StyleDisplay::RubyBase == aDisplay ||
           mozilla::StyleDisplay::RubyBaseContainer == aDisplay ||
           mozilla::StyleDisplay::RubyText == aDisplay ||
           mozilla::StyleDisplay::RubyTextContainer == aDisplay;
  }

  bool IsRubyDisplayType() const { return IsRubyDisplayType(mDisplay); }

  bool IsInternalRubyDisplayType() const {
    return IsInternalRubyDisplayType(mDisplay);
  }

  bool IsOutOfFlowStyle() const {
    return (IsAbsolutelyPositionedStyle() || IsFloatingStyle());
  }

  bool IsScrollableOverflow() const {
    // Visible and Clip can be combined but not with other values,
    // so checking mOverflowX is enough.
    return mOverflowX != mozilla::StyleOverflow::Visible &&
           mOverflowX != mozilla::StyleOverflow::Clip;
  }

  bool OverflowIsVisibleInBothAxis() const {
    return mOverflowX == mozilla::StyleOverflow::Visible &&
           mOverflowY == mozilla::StyleOverflow::Visible;
  }

  bool IsContainPaint() const {
    return (mContain & mozilla::StyleContain::PAINT) &&
           !IsInternalRubyDisplayType() && !IsInternalTableStyleExceptCell();
  }

  bool IsContainLayout() const {
    // Note: The spec for layout containment says it should
    // have no effect on non-atomic, inline-level boxes. We
    // don't check for these here because we don't know
    // what type of element is involved. Callers are
    // responsible for checking if the box in question is
    // non-atomic and inline-level, and creating an
    // exemption as necessary.
    return (mContain & mozilla::StyleContain::LAYOUT) &&
           !IsInternalRubyDisplayType() && !IsInternalTableStyleExceptCell();
  }

  bool IsContainSize() const {
    // Note: The spec for size containment says it should
    // have no effect on non-atomic, inline-level boxes. We
    // don't check for these here because we don't know
    // what type of element is involved. Callers are
    // responsible for checking if the box in question is
    // non-atomic and inline-level, and creating an
    // exemption as necessary.
    return (mContain & mozilla::StyleContain::SIZE) &&
           !IsInternalRubyDisplayType() &&
           DisplayInside() != mozilla::StyleDisplayInside::Table &&
           !IsInnerTableStyle();
  }

  /* Returns whether the element has the transform property or a related
   * property. */
  bool HasTransformStyle() const {
    return HasTransformProperty() || HasIndividualTransform() ||
           mTransformStyle == mozilla::StyleTransformStyle::Preserve3d ||
           (mWillChange.bits & mozilla::StyleWillChangeBits::TRANSFORM) ||
           !mOffsetPath.IsNone();
  }

  bool HasTransformProperty() const { return !mTransform._0.IsEmpty(); }

  bool HasIndividualTransform() const {
    return !mRotate.IsNone() || !mTranslate.IsNone() || !mScale.IsNone();
  }

  bool HasPerspectiveStyle() const { return !mChildPerspective.IsNone(); }

  bool BackfaceIsHidden() const {
    return mBackfaceVisibility == mozilla::StyleBackfaceVisibility::Hidden;
  }

  // FIXME(emilio): This should be more fine-grained on each caller to
  // BreakBefore() / BreakAfter().
  static bool ShouldBreak(mozilla::StyleBreakBetween aBreak) {
    switch (aBreak) {
      case mozilla::StyleBreakBetween::Left:
      case mozilla::StyleBreakBetween::Right:
      case mozilla::StyleBreakBetween::Page:
      case mozilla::StyleBreakBetween::Always:
        return true;
      case mozilla::StyleBreakBetween::Auto:
      case mozilla::StyleBreakBetween::Avoid:
        return false;
      default:
        MOZ_ASSERT_UNREACHABLE("Unknown break kind");
        return false;
    }
  }

  bool BreakBefore() const { return ShouldBreak(mBreakBefore); }

  bool BreakAfter() const { return ShouldBreak(mBreakAfter); }

  // These are defined in nsStyleStructInlines.h.

  // The aContextFrame argument on each of these is the frame this
  // style struct is for.  If the frame is for SVG text, the return
  // value will be massaged to be something that makes sense for
  // SVG text.
  inline bool IsBlockOutside(const nsIFrame* aContextFrame) const;
  inline bool IsInlineOutside(const nsIFrame* aContextFrame) const;
  inline mozilla::StyleDisplay GetDisplay(const nsIFrame* aContextFrame) const;
  inline bool IsFloating(const nsIFrame* aContextFrame) const;
  inline bool IsRelativelyPositioned(const nsIFrame* aContextFrame) const;
  inline bool IsStickyPositioned(const nsIFrame* aContextFrame) const;
  inline bool IsAbsolutelyPositioned(const nsIFrame* aContextFrame) const;

  // These methods are defined in nsStyleStructInlines.h.

  /**
   * Returns true when the element has the transform property
   * or a related property, and supports CSS transforms.
   * aContextFrame is the frame for which this is the nsStyleDisplay.
   */
  inline bool HasTransform(const nsIFrame* aContextFrame) const;

  /**
   * Returns true when the element has the perspective property,
   * and supports CSS transforms. aContextFrame is the frame for
   * which this is the nsStyleDisplay.
   */
  inline bool HasPerspective(const nsIFrame* aContextFrame) const;

  /**
   * Returns whether the element is a containing block for its
   * absolutely positioned descendants.
   * aContextFrame is the frame for which this is the nsStyleDisplay.
   */
  inline bool IsAbsPosContainingBlock(const nsIFrame* aContextFrame) const;

  /**
   * Returns true when the element is a containing block for its fixed-pos
   * descendants.
   * aContextFrame is the frame for which this is the nsStyleDisplay.
   */
  inline bool IsFixedPosContainingBlock(const nsIFrame* aContextFrame) const;

  /**
   * Tests for only the sub-parts of IsFixedPosContainingBlock that apply
   * to:
   *  - nearly all frames, except those that are SVG text frames.
   *  - frames that support CSS contain:layout and contain:paint and are not
   *    SVG text frames.
   *  - frames that support CSS transforms and are not SVG text frames.
   *
   * This should be used only when the caller has the style but not the
   * frame (i.e., when calculating style changes).
   */
  inline bool IsFixedPosContainingBlockForNonSVGTextFrames(
      const mozilla::ComputedStyle&) const;
  inline bool
  IsFixedPosContainingBlockForContainLayoutAndPaintSupportingFrames() const;
  inline bool IsFixedPosContainingBlockForTransformSupportingFrames() const;

  void GenerateCombinedIndividualTransform();
};

struct MOZ_NEEDS_MEMMOVABLE_MEMBERS nsStyleTable {
  explicit nsStyleTable(const mozilla::dom::Document&);
  nsStyleTable(const nsStyleTable& aOther);
  ~nsStyleTable();
  static constexpr bool kHasTriggerImageLoads = false;

  nsChangeHint CalcDifference(const nsStyleTable& aNewData) const;

  mozilla::StyleTableLayout mLayoutStrategy;
  int32_t mXSpan;  // The number of columns spanned by a colgroup or col
};

struct MOZ_NEEDS_MEMMOVABLE_MEMBERS nsStyleTableBorder {
  explicit nsStyleTableBorder(const mozilla::dom::Document&);
  nsStyleTableBorder(const nsStyleTableBorder& aOther);
  ~nsStyleTableBorder();
  static constexpr bool kHasTriggerImageLoads = false;

  nsChangeHint CalcDifference(const nsStyleTableBorder& aNewData) const;

  nscoord mBorderSpacingCol;
  nscoord mBorderSpacingRow;
  mozilla::StyleBorderCollapse mBorderCollapse;
  mozilla::StyleCaptionSide mCaptionSide;
  mozilla::StyleEmptyCells mEmptyCells;
};

struct MOZ_NEEDS_MEMMOVABLE_MEMBERS nsStyleContent {
  using CounterPair = mozilla::StyleGenericCounterPair<int32_t>;

  explicit nsStyleContent(const mozilla::dom::Document&);
  nsStyleContent(const nsStyleContent& aContent);
  ~nsStyleContent();

  void TriggerImageLoads(mozilla::dom::Document&, const nsStyleContent*);
  static constexpr bool kHasTriggerImageLoads = true;

  size_t ContentCount() const {
    return mContent.IsItems() ? mContent.AsItems().Length() : 0;
  }

  const mozilla::StyleContentItem& ContentAt(size_t aIndex) const {
    return mContent.AsItems().AsSpan()[aIndex];
  }

  nsChangeHint CalcDifference(const nsStyleContent& aNewData) const;

  mozilla::StyleContent mContent;
  mozilla::StyleCounterIncrement mCounterIncrement;
  mozilla::StyleCounterSetOrReset mCounterReset;
  mozilla::StyleCounterSetOrReset mCounterSet;
};

struct MOZ_NEEDS_MEMMOVABLE_MEMBERS nsStyleUIReset {
  explicit nsStyleUIReset(const mozilla::dom::Document&);
  nsStyleUIReset(const nsStyleUIReset& aOther);
  ~nsStyleUIReset();
  static constexpr bool kHasTriggerImageLoads = false;

  nsChangeHint CalcDifference(const nsStyleUIReset& aNewData) const;

  mozilla::StyleUserSelect mUserSelect;  // [reset](selection-style)
  mozilla::StyleScrollbarWidth mScrollbarWidth;
  uint8_t mMozForceBrokenImageIcon;  // (0 if not forcing, otherwise forcing)
  mozilla::StyleImeMode mIMEMode;
  mozilla::StyleWindowDragging mWindowDragging;
  mozilla::StyleWindowShadow mWindowShadow;
  float mWindowOpacity;
  mozilla::StyleTransform mMozWindowTransform;
  mozilla::StyleTransformOrigin mWindowTransformOrigin;
};

struct MOZ_NEEDS_MEMMOVABLE_MEMBERS nsStyleUI {
  explicit nsStyleUI(const mozilla::dom::Document&);
  nsStyleUI(const nsStyleUI& aOther);
  ~nsStyleUI();

  void TriggerImageLoads(mozilla::dom::Document&, const nsStyleUI*);
  static constexpr bool kHasTriggerImageLoads = true;

  nsChangeHint CalcDifference(const nsStyleUI& aNewData) const;

  mozilla::StyleInert mInert;
  mozilla::StyleUserInput mUserInput;
  mozilla::StyleUserModify mUserModify;  // (modify-content)
  mozilla::StyleUserFocus mUserFocus;    // (auto-select)
  mozilla::StylePointerEvents mPointerEvents;

  mozilla::StyleCursor mCursor;

  mozilla::StyleColorOrAuto mAccentColor;
  mozilla::StyleCaretColor mCaretColor;
  mozilla::StyleScrollbarColor mScrollbarColor;

  inline mozilla::StylePointerEvents GetEffectivePointerEvents(
      nsIFrame* aFrame) const;

  bool HasCustomScrollbars() const { return !mScrollbarColor.IsAuto(); }
};

struct MOZ_NEEDS_MEMMOVABLE_MEMBERS nsStyleXUL {
  explicit nsStyleXUL(const mozilla::dom::Document&);
  nsStyleXUL(const nsStyleXUL& aSource);
  ~nsStyleXUL();
  static constexpr bool kHasTriggerImageLoads = false;

  nsChangeHint CalcDifference(const nsStyleXUL& aNewData) const;

  float mBoxFlex;
  int32_t mBoxOrdinal;
  mozilla::StyleBoxAlign mBoxAlign;
  mozilla::StyleBoxDirection mBoxDirection;
  mozilla::StyleBoxOrient mBoxOrient;
  mozilla::StyleBoxPack mBoxPack;
};

struct MOZ_NEEDS_MEMMOVABLE_MEMBERS nsStyleColumn {
  explicit nsStyleColumn(const mozilla::dom::Document&);
  nsStyleColumn(const nsStyleColumn& aSource);
  ~nsStyleColumn();
  static constexpr bool kHasTriggerImageLoads = false;

  nsChangeHint CalcDifference(const nsStyleColumn& aNewData) const;

  // This is the maximum number of columns we can process. It's used in
  // nsColumnSetFrame.
  static const uint32_t kMaxColumnCount = 1000;

  // This represents the value of column-count: auto.
  static const uint32_t kColumnCountAuto = 0;

  uint32_t mColumnCount = kColumnCountAuto;
  mozilla::NonNegativeLengthOrAuto mColumnWidth;

  mozilla::StyleColor mColumnRuleColor;
  mozilla::StyleBorderStyle mColumnRuleStyle;  // StyleborderStyle::*
  mozilla::StyleColumnFill mColumnFill = mozilla::StyleColumnFill::Balance;
  mozilla::StyleColumnSpan mColumnSpan = mozilla::StyleColumnSpan::None;

  nscoord GetComputedColumnRuleWidth() const {
    return (IsVisibleBorderStyle(mColumnRuleStyle) ? mColumnRuleWidth : 0);
  }

  bool IsColumnContainerStyle() const {
    return mColumnCount != kColumnCountAuto || !mColumnWidth.IsAuto();
  }

  bool IsColumnSpanStyle() const {
    return mColumnSpan == mozilla::StyleColumnSpan::All;
  }

 protected:
  nscoord mColumnRuleWidth;  // coord
  nscoord mTwipsPerPixel;
};

struct MOZ_NEEDS_MEMMOVABLE_MEMBERS nsStyleSVG {
  explicit nsStyleSVG(const mozilla::dom::Document&);
  nsStyleSVG(const nsStyleSVG& aSource);
  ~nsStyleSVG();
  static constexpr bool kHasTriggerImageLoads = false;

  nsChangeHint CalcDifference(const nsStyleSVG& aNewData) const;

  mozilla::StyleSVGPaint mFill;
  mozilla::StyleSVGPaint mStroke;
  mozilla::StyleUrlOrNone mMarkerEnd;
  mozilla::StyleUrlOrNone mMarkerMid;
  mozilla::StyleUrlOrNone mMarkerStart;
  mozilla::StyleMozContextProperties mMozContextProperties;

  mozilla::StyleSVGStrokeDashArray mStrokeDasharray;
  mozilla::StyleSVGLength mStrokeDashoffset;
  mozilla::StyleSVGWidth mStrokeWidth;

  mozilla::StyleSVGOpacity mFillOpacity;
  float mStrokeMiterlimit;
  mozilla::StyleSVGOpacity mStrokeOpacity;

  mozilla::StyleFillRule mClipRule;
  mozilla::StyleColorInterpolation mColorInterpolation;
  mozilla::StyleColorInterpolation mColorInterpolationFilters;
  mozilla::StyleFillRule mFillRule;
  mozilla::StyleSVGPaintOrder mPaintOrder;
  mozilla::StyleShapeRendering mShapeRendering;
  mozilla::StyleStrokeLinecap mStrokeLinecap;
  mozilla::StyleStrokeLinejoin mStrokeLinejoin;
  mozilla::StyleDominantBaseline mDominantBaseline;
  mozilla::StyleTextAnchor mTextAnchor;

  /// Returns true if style has been set to expose the computed values of
  /// certain properties (such as 'fill') to the contents of any linked images.
  bool ExposesContextProperties() const {
    return bool(mMozContextProperties.bits);
  }

  bool HasMarker() const {
    return mMarkerStart.IsUrl() || mMarkerMid.IsUrl() || mMarkerEnd.IsUrl();
  }

  /**
   * Returns true if the stroke is not "none" and the stroke-opacity is greater
   * than zero (or a context-dependent value).
   *
   * This ignores stroke-widths as that depends on the context.
   */
  bool HasStroke() const {
    if (mStroke.kind.IsNone()) {
      return false;
    }
    return !mStrokeOpacity.IsOpacity() || mStrokeOpacity.AsOpacity() > 0;
  }

  /**
   * Returns true if the fill is not "none" and the fill-opacity is greater
   * than zero (or a context-dependent value).
   */
  bool HasFill() const {
    if (mFill.kind.IsNone()) {
      return false;
    }
    return !mFillOpacity.IsOpacity() || mFillOpacity.AsOpacity() > 0;
  }
};

struct MOZ_NEEDS_MEMMOVABLE_MEMBERS nsStyleSVGReset {
  explicit nsStyleSVGReset(const mozilla::dom::Document&);
  nsStyleSVGReset(const nsStyleSVGReset& aSource);
  ~nsStyleSVGReset();

  // Resolves and tracks the images in mMask.  Only called with a Servo-backed
  // style system, where those images must be resolved later than the OMT
  // nsStyleSVGReset constructor call.
  void TriggerImageLoads(mozilla::dom::Document&, const nsStyleSVGReset*);
  static constexpr bool kHasTriggerImageLoads = true;

  nsChangeHint CalcDifference(const nsStyleSVGReset& aNewData) const;

  bool HasClipPath() const { return !mClipPath.IsNone(); }

  bool HasMask() const;

  bool HasNonScalingStroke() const {
    return mVectorEffect == mozilla::StyleVectorEffect::NonScalingStroke;
  }

  // geometry properties
  mozilla::LengthPercentage mX;
  mozilla::LengthPercentage mY;
  mozilla::LengthPercentage mCx;
  mozilla::LengthPercentage mCy;
  mozilla::NonNegativeLengthPercentageOrAuto mRx;
  mozilla::NonNegativeLengthPercentageOrAuto mRy;
  mozilla::NonNegativeLengthPercentage mR;

  nsStyleImageLayers mMask;
  mozilla::StyleClipPath mClipPath;
  mozilla::StyleColor mStopColor;
  mozilla::StyleColor mFloodColor;
  mozilla::StyleColor mLightingColor;

  float mStopOpacity;
  float mFloodOpacity;

  mozilla::StyleVectorEffect mVectorEffect;
  mozilla::StyleMaskType mMaskType;
};

struct MOZ_NEEDS_MEMMOVABLE_MEMBERS nsStyleEffects {
  explicit nsStyleEffects(const mozilla::dom::Document&);
  nsStyleEffects(const nsStyleEffects& aSource);
  ~nsStyleEffects();
  static constexpr bool kHasTriggerImageLoads = false;

  nsChangeHint CalcDifference(const nsStyleEffects& aNewData) const;

  bool HasFilters() const { return !mFilters.IsEmpty(); }

  bool HasBackdropFilters() const { return !mBackdropFilters.IsEmpty(); }

  bool HasBoxShadowWithInset(bool aInset) const {
    for (auto& shadow : mBoxShadow.AsSpan()) {
      if (shadow.inset == aInset) {
        return true;
      }
    }
    return false;
  }

  bool HasMixBlendMode() const {
    return mMixBlendMode != mozilla::StyleBlend::Normal;
  }

  mozilla::StyleOwnedSlice<mozilla::StyleFilter> mFilters;
  mozilla::StyleOwnedSlice<mozilla::StyleBoxShadow> mBoxShadow;
  mozilla::StyleOwnedSlice<mozilla::StyleFilter> mBackdropFilters;
  mozilla::StyleClipRectOrAuto mClip;  // offsets from UL border edge
  float mOpacity;
  mozilla::StyleBlend mMixBlendMode;
};

#define STATIC_ASSERT_TYPE_LAYOUTS_MATCH(T1, T2)           \
  static_assert(sizeof(T1) == sizeof(T2),                  \
                "Size mismatch between " #T1 " and " #T2); \
  static_assert(alignof(T1) == alignof(T2),                \
                "Align mismatch between " #T1 " and " #T2);

#define STATIC_ASSERT_FIELD_OFFSET_MATCHES(T1, T2, field)          \
  static_assert(offsetof(T1, field) == offsetof(T2, field),        \
                "Field offset mismatch of " #field " between " #T1 \
                " and " #T2);

/**
 * These *_Simple types are used to map Gecko types to layout-equivalent but
 * simpler Rust types, to aid Rust binding generation.
 *
 * If something in this types or the assertions below needs to change, ask
 * bholley, heycam or emilio before!
 *
 * <div rustbindgen="true" replaces="nsPoint">
 */
struct nsPoint_Simple {
  nscoord x, y;
};

STATIC_ASSERT_TYPE_LAYOUTS_MATCH(nsPoint, nsPoint_Simple);
STATIC_ASSERT_FIELD_OFFSET_MATCHES(nsPoint, nsPoint_Simple, x);
STATIC_ASSERT_FIELD_OFFSET_MATCHES(nsPoint, nsPoint_Simple, y);

/**
 * <div rustbindgen="true" replaces="nsMargin">
 */
struct nsMargin_Simple {
  nscoord top, right, bottom, left;
};

STATIC_ASSERT_TYPE_LAYOUTS_MATCH(nsMargin, nsMargin_Simple);
STATIC_ASSERT_FIELD_OFFSET_MATCHES(nsMargin, nsMargin_Simple, top);
STATIC_ASSERT_FIELD_OFFSET_MATCHES(nsMargin, nsMargin_Simple, right);
STATIC_ASSERT_FIELD_OFFSET_MATCHES(nsMargin, nsMargin_Simple, bottom);
STATIC_ASSERT_FIELD_OFFSET_MATCHES(nsMargin, nsMargin_Simple, left);

/**
 * <div rustbindgen="true" replaces="nsRect">
 */
struct nsRect_Simple {
  nscoord x, y, width, height;
};

STATIC_ASSERT_TYPE_LAYOUTS_MATCH(nsRect, nsRect_Simple);
STATIC_ASSERT_FIELD_OFFSET_MATCHES(nsRect, nsRect_Simple, x);
STATIC_ASSERT_FIELD_OFFSET_MATCHES(nsRect, nsRect_Simple, y);
STATIC_ASSERT_FIELD_OFFSET_MATCHES(nsRect, nsRect_Simple, width);
STATIC_ASSERT_FIELD_OFFSET_MATCHES(nsRect, nsRect_Simple, height);

/**
 * <div rustbindgen="true" replaces="nsSize">
 */
struct nsSize_Simple {
  nscoord width, height;
};

STATIC_ASSERT_TYPE_LAYOUTS_MATCH(nsSize, nsSize_Simple);
STATIC_ASSERT_FIELD_OFFSET_MATCHES(nsSize, nsSize_Simple, width);
STATIC_ASSERT_FIELD_OFFSET_MATCHES(nsSize, nsSize_Simple, height);

/**
 * <div rustbindgen="true" replaces="mozilla::UniquePtr">
 *
 * TODO(Emilio): This is a workaround and we should be able to get rid of this
 * one.
 */
template <typename T>
struct UniquePtr_Simple {
  T* mPtr;
};

STATIC_ASSERT_TYPE_LAYOUTS_MATCH(mozilla::UniquePtr<int>,
                                 UniquePtr_Simple<int>);

/**
 * <div rustbindgen replaces="nsTArray"></div>
 */
template <typename T>
class nsTArray_Simple {
 protected:
  T* mBuffer;

 public:
  ~nsTArray_Simple() {
    // The existence of a user-provided, and therefore non-trivial, destructor
    // here prevents bindgen from deriving the Clone trait via a simple memory
    // copy.
  }
};

/**
 * <div rustbindgen replaces="CopyableTArray"></div>
 */
template <typename T>
class CopyableTArray_Simple : public nsTArray_Simple<T> {};

STATIC_ASSERT_TYPE_LAYOUTS_MATCH(nsTArray<nsStyleImageLayers::Layer>,
                                 nsTArray_Simple<nsStyleImageLayers::Layer>);
STATIC_ASSERT_TYPE_LAYOUTS_MATCH(nsTArray<mozilla::StyleTransition>,
                                 nsTArray_Simple<mozilla::StyleTransition>);
STATIC_ASSERT_TYPE_LAYOUTS_MATCH(nsTArray<mozilla::StyleAnimation>,
                                 nsTArray_Simple<mozilla::StyleAnimation>);

#endif /* nsStyleStruct_h___ */
