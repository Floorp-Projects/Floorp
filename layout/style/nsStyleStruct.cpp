/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * structs that contain the data provided by ComputedStyle, the
 * internal API for computed style data for an element
 */

#include "nsStyleStruct.h"
#include "nsStyleStructInlines.h"
#include "nsStyleConsts.h"
#include "nsString.h"
#include "nsPresContext.h"
#include "nsIWidget.h"
#include "nsCRTGlue.h"
#include "nsCSSProps.h"
#include "nsDeviceContext.h"
#include "nsStyleUtil.h"
#include "nsIURIMutator.h"

#include "nsCOMPtr.h"

#include "nsBidiUtils.h"
#include "nsLayoutUtils.h"

#include "imgIRequest.h"
#include "imgIContainer.h"
#include "CounterStyleManager.h"

#include "mozilla/dom/AnimationEffectBinding.h"  // for PlaybackDirection
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/ImageTracker.h"
#include "mozilla/CORSMode.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/GeckoBindings.h"
#include "mozilla/PreferenceSheet.h"
#include "mozilla/StaticPresData.h"
#include "mozilla/Likely.h"
#include "nsIURI.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include <algorithm>
#include "ImageLoader.h"
#include "mozilla/StaticPrefs_layout.h"

using namespace mozilla;
using namespace mozilla::dom;

static const nscoord kMediumBorderWidth = nsPresContext::CSSPixelsToAppUnits(3);

// We set the size limit of style structs to 504 bytes so that when they
// are allocated by Servo side with Arc, the total size doesn't exceed
// 512 bytes, which minimizes allocator slop.
static constexpr size_t kStyleStructSizeLimit = 504;

template <typename Struct, size_t Actual, size_t Limit>
struct AssertSizeIsLessThan {
  static_assert(Actual == sizeof(Struct), "Bogus invocation");
  static_assert(Actual <= Limit,
                "Style struct became larger than the size limit");
  static constexpr bool instantiate = true;
};

#define STYLE_STRUCT(name_)                                                  \
  static_assert(AssertSizeIsLessThan<nsStyle##name_, sizeof(nsStyle##name_), \
                                     kStyleStructSizeLimit>::instantiate,    \
                "");
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

bool StyleCssUrlData::operator==(const StyleCssUrlData& aOther) const {
  // This very intentionally avoids comparing LoadData and such.
  const auto& extra = extra_data.get();
  const auto& otherExtra = aOther.extra_data.get();
  if (extra.BaseURI() != otherExtra.BaseURI() ||
      extra.Principal() != otherExtra.Principal() ||
      cors_mode != aOther.cors_mode) {
    // NOTE(emilio): This does pointer comparison, but it's what URLValue used
    // to do. That's ok though since this is only used for style struct diffing.
    return false;
  }
  return serialization == aOther.serialization;
}

StyleLoadData::~StyleLoadData() { Gecko_LoadData_Drop(this); }

already_AddRefed<nsIURI> StyleComputedUrl::ResolveLocalRef(nsIURI* aURI) const {
  nsCOMPtr<nsIURI> result = GetURI();
  if (result && IsLocalRef()) {
    nsCString ref;
    result->GetRef(ref);

    nsresult rv = NS_MutateURI(aURI).SetRef(ref).Finalize(result);

    if (NS_FAILED(rv)) {
      // If setting the ref failed, just return the original URI.
      result = aURI;
    }
  }
  return result.forget();
}

already_AddRefed<nsIURI> StyleComputedUrl::ResolveLocalRef(
    const nsIContent* aContent) const {
  return ResolveLocalRef(aContent->GetBaseURI());
}

void StyleComputedUrl::ResolveImage(Document& aDocument,
                                    const StyleComputedUrl* aOldImage) {
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread());

  StyleLoadData& data = LoadData();

  MOZ_ASSERT(!(data.flags & StyleLoadDataFlags::TRIED_TO_RESOLVE_IMAGE));

  data.flags |= StyleLoadDataFlags::TRIED_TO_RESOLVE_IMAGE;

  nsIURI* docURI = aDocument.GetDocumentURI();
  if (HasRef()) {
    bool isEqualExceptRef = false;
    nsIURI* imageURI = GetURI();
    if (!imageURI) {
      return;
    }

    if (NS_SUCCEEDED(imageURI->EqualsExceptRef(docURI, &isEqualExceptRef)) &&
        isEqualExceptRef) {
      // Prevent loading an internal resource.
      return;
    }
  }

  MOZ_ASSERT(NS_IsMainThread());

  // TODO(emilio, bug 1440442): This is a hackaround to avoid flickering due the
  // lack of non-http image caching in imagelib (bug 1406134), which causes
  // stuff like bug 1439285. Cleanest fix if that doesn't get fixed is bug
  // 1440305, but that seems too risky, and a lot of work to do before 60.
  //
  // Once that's fixed, the "old style" argument to TriggerImageLoads can go
  // away, and same for mSharedCount in the image loader and so on.
  const bool reuseProxy = nsContentUtils::IsChromeDoc(&aDocument) &&
                          aOldImage && aOldImage->IsImageResolved() &&
                          *this == *aOldImage;

  RefPtr<imgRequestProxy> request;
  if (reuseProxy) {
    request = aOldImage->LoadData().resolved_image;
    if (request) {
      css::ImageLoader::NoteSharedLoad(request);
    }
  } else {
    request = css::ImageLoader::LoadImage(*this, aDocument);
  }

  if (!request) {
    return;
  }

  data.resolved_image = request.forget().take();

  // Boost priority now that we know the image is present in the ComputedStyle
  // of some frame.
  data.resolved_image->BoostPriority(imgIRequest::CATEGORY_FRAME_STYLE);
}

/**
 * Runnable to release the image request's mRequestProxy
 * and mImageTracker on the main thread, and to perform
 * any necessary unlocking and untracking of the image.
 */
class StyleImageRequestCleanupTask final : public mozilla::Runnable {
 public:
  explicit StyleImageRequestCleanupTask(StyleLoadData& aData)
      : mozilla::Runnable("StyleImageRequestCleanupTask"),
        mRequestProxy(dont_AddRef(aData.resolved_image)) {
    MOZ_ASSERT(mRequestProxy);
    aData.resolved_image = nullptr;
  }

  NS_IMETHOD Run() final {
    MOZ_ASSERT(NS_IsMainThread());
    css::ImageLoader::UnloadImage(mRequestProxy);
    return NS_OK;
  }

 protected:
  virtual ~StyleImageRequestCleanupTask() {
    MOZ_ASSERT(!mRequestProxy || NS_IsMainThread(),
               "mRequestProxy destructor need to run on the main thread!");
  }

 private:
  // Since we always dispatch this runnable to the main thread, these will be
  // released on the main thread when the runnable itself is released.
  RefPtr<imgRequestProxy> mRequestProxy;
};

// This is defined here for parallelism with LoadURI.
void Gecko_LoadData_Drop(StyleLoadData* aData) {
  if (aData->resolved_image) {
    auto task = MakeRefPtr<StyleImageRequestCleanupTask>(*aData);
    if (NS_IsMainThread()) {
      task->Run();
    } else {
      // if Resolve was not called at some point, mDocGroup is not set.
      SystemGroup::Dispatch(TaskCategory::Other, task.forget());
    }
  }

  // URIs are safe to refcount from any thread.
  NS_IF_RELEASE(aData->resolved_uri);
}

// --------------------
// nsStyleFont
//
nsStyleFont::nsStyleFont(const nsStyleFont& aSrc)
    : mFont(aSrc.mFont),
      mSize(aSrc.mSize),
      mFontSizeFactor(aSrc.mFontSizeFactor),
      mFontSizeOffset(aSrc.mFontSizeOffset),
      mFontSizeKeyword(aSrc.mFontSizeKeyword),
      mGenericID(aSrc.mGenericID),
      mScriptLevel(aSrc.mScriptLevel),
      mMathVariant(aSrc.mMathVariant),
      mMathDisplay(aSrc.mMathDisplay),
      mMinFontSizeRatio(aSrc.mMinFontSizeRatio),
      mExplicitLanguage(aSrc.mExplicitLanguage),
      mAllowZoom(aSrc.mAllowZoom),
      mScriptUnconstrainedSize(aSrc.mScriptUnconstrainedSize),
      mScriptMinSize(aSrc.mScriptMinSize),
      mScriptSizeMultiplier(aSrc.mScriptSizeMultiplier),
      mLanguage(aSrc.mLanguage) {
  MOZ_COUNT_CTOR(nsStyleFont);
}

nsStyleFont::nsStyleFont(const Document& aDocument)
    : mFont(*aDocument.GetFontPrefsForLang(nullptr)->GetDefaultFont(
          StyleGenericFontFamily::None)),
      mSize(ZoomText(aDocument, mFont.size)),
      mFontSizeFactor(1.0),
      mFontSizeOffset(0),
      mFontSizeKeyword(NS_STYLE_FONT_SIZE_MEDIUM),
      mGenericID(StyleGenericFontFamily::None),
      mScriptLevel(0),
      mMathVariant(NS_MATHML_MATHVARIANT_NONE),
      mMathDisplay(NS_MATHML_DISPLAYSTYLE_INLINE),
      mMinFontSizeRatio(100),  // 100%
      mExplicitLanguage(false),
      mAllowZoom(true),
      mScriptUnconstrainedSize(mSize),
      mScriptMinSize(nsPresContext::CSSTwipsToAppUnits(
          NS_POINTS_TO_TWIPS(NS_MATHML_DEFAULT_SCRIPT_MIN_SIZE_PT))),
      mScriptSizeMultiplier(NS_MATHML_DEFAULT_SCRIPT_SIZE_MULTIPLIER),
      mLanguage(aDocument.GetLanguageForStyle()) {
  MOZ_COUNT_CTOR(nsStyleFont);
  MOZ_ASSERT(NS_IsMainThread());
  mFont.size = mSize;
  if (!nsContentUtils::IsChromeDoc(&aDocument)) {
    nscoord minimumFontSize =
        aDocument.GetFontPrefsForLang(mLanguage)->mMinimumFontSize;
    mFont.size = std::max(mSize, minimumFontSize);
  }
}

nsChangeHint nsStyleFont::CalcDifference(const nsStyleFont& aNewData) const {
  MOZ_ASSERT(mAllowZoom == aNewData.mAllowZoom,
             "expected mAllowZoom to be the same on both nsStyleFonts");
  if (mSize != aNewData.mSize || mLanguage != aNewData.mLanguage ||
      mExplicitLanguage != aNewData.mExplicitLanguage ||
      mMathVariant != aNewData.mMathVariant ||
      mMathDisplay != aNewData.mMathDisplay ||
      mMinFontSizeRatio != aNewData.mMinFontSizeRatio) {
    return NS_STYLE_HINT_REFLOW;
  }

  switch (mFont.CalcDifference(aNewData.mFont)) {
    case nsFont::MaxDifference::eLayoutAffecting:
      return NS_STYLE_HINT_REFLOW;

    case nsFont::MaxDifference::eVisual:
      return NS_STYLE_HINT_VISUAL;

    case nsFont::MaxDifference::eNone:
      break;
  }

  // XXX Should any of these cause a non-nsChangeHint_NeutralChange change?
  if (mGenericID != aNewData.mGenericID ||
      mScriptLevel != aNewData.mScriptLevel ||
      mScriptUnconstrainedSize != aNewData.mScriptUnconstrainedSize ||
      mScriptMinSize != aNewData.mScriptMinSize ||
      mScriptSizeMultiplier != aNewData.mScriptSizeMultiplier) {
    return nsChangeHint_NeutralChange;
  }

  return nsChangeHint(0);
}

nscoord nsStyleFont::ZoomText(const Document& aDocument, nscoord aSize) {
  float textZoom = 1.0;
  if (auto* pc = aDocument.GetPresContext()) {
    textZoom = pc->EffectiveTextZoom();
  }
  // aSize can be negative (e.g.: calc(-1px)) so we can't assert that here.
  // The caller is expected deal with that.
  return NSToCoordTruncClamped(float(aSize) * textZoom);
}

template <typename T>
static StyleRect<T> StyleRectWithAllSides(const T& aSide) {
  return {aSide, aSide, aSide, aSide};
}

nsStyleMargin::nsStyleMargin(const Document& aDocument)
    : mMargin(StyleRectWithAllSides(
          LengthPercentageOrAuto::LengthPercentage(LengthPercentage::Zero()))),
      mScrollMargin(StyleRectWithAllSides(StyleLength{0.})) {
  MOZ_COUNT_CTOR(nsStyleMargin);
}

nsStyleMargin::nsStyleMargin(const nsStyleMargin& aSrc)
    : mMargin(aSrc.mMargin), mScrollMargin(aSrc.mScrollMargin) {
  MOZ_COUNT_CTOR(nsStyleMargin);
}

nsChangeHint nsStyleMargin::CalcDifference(
    const nsStyleMargin& aNewData) const {
  nsChangeHint hint = nsChangeHint(0);

  if (mMargin != aNewData.mMargin) {
    // Margin differences can't affect descendant intrinsic sizes and
    // don't need to force children to reflow.
    hint |= nsChangeHint_NeedReflow | nsChangeHint_ReflowChangesSizeOrPosition |
            nsChangeHint_ClearAncestorIntrinsics;
  }

  if (mScrollMargin != aNewData.mScrollMargin) {
    // FIXME: Bug 1530253 Support re-snapping when scroll-margin changes.
    hint |= nsChangeHint_NeutralChange;
  }

  return hint;
}

nsStylePadding::nsStylePadding(const Document& aDocument)
    : mPadding(StyleRectWithAllSides(LengthPercentage::Zero())),
      mScrollPadding(StyleRectWithAllSides(LengthPercentageOrAuto::Auto())) {
  MOZ_COUNT_CTOR(nsStylePadding);
}

nsStylePadding::nsStylePadding(const nsStylePadding& aSrc)
    : mPadding(aSrc.mPadding), mScrollPadding(aSrc.mScrollPadding) {
  MOZ_COUNT_CTOR(nsStylePadding);
}

nsChangeHint nsStylePadding::CalcDifference(
    const nsStylePadding& aNewData) const {
  nsChangeHint hint = nsChangeHint(0);

  if (mPadding != aNewData.mPadding) {
    // Padding differences can't affect descendant intrinsic sizes, but do need
    // to force children to reflow so that we can reposition them, since their
    // offsets are from our frame bounds but our content rect's position within
    // those bounds is moving.
    // FIXME: It would be good to return a weaker hint here that doesn't
    // force reflow of all descendants, but the hint would need to force
    // reflow of the frame's children (see how
    // ReflowInput::InitResizeFlags initializes the inline-resize flag).
    hint |= NS_STYLE_HINT_REFLOW & ~nsChangeHint_ClearDescendantIntrinsics;
  }

  if (mScrollPadding != aNewData.mScrollPadding) {
    // FIXME: Bug 1530253 Support re-snapping when scroll-padding changes.
    hint |= nsChangeHint_NeutralChange;
  }

  return hint;
}

static nscoord TwipsPerPixel(const Document& aDocument) {
  auto* pc = aDocument.GetPresContext();
  return pc ? pc->AppUnitsPerDevPixel() : mozilla::AppUnitsPerCSSPixel();
}

static inline BorderRadius ZeroBorderRadius() {
  auto zero = LengthPercentage::Zero();
  return {{{zero, zero}}, {{zero, zero}}, {{zero, zero}}, {{zero, zero}}};
}

nsStyleBorder::nsStyleBorder(const Document& aDocument)
    : mBorderRadius(ZeroBorderRadius()),
      mBorderImageSource(StyleImage::None()),
      mBorderImageWidth(
          StyleRectWithAllSides(StyleBorderImageSideWidth::Number(1.))),
      mBorderImageOutset(
          StyleRectWithAllSides(StyleNonNegativeLengthOrNumber::Number(0.))),
      mBorderImageSlice(
          {StyleRectWithAllSides(StyleNumberOrPercentage::Percentage({1.})),
           false}),
      mBorderImageRepeatH(StyleBorderImageRepeat::Stretch),
      mBorderImageRepeatV(StyleBorderImageRepeat::Stretch),
      mFloatEdge(StyleFloatEdge::ContentBox),
      mBoxDecorationBreak(StyleBoxDecorationBreak::Slice),
      mBorderTopColor(StyleColor::CurrentColor()),
      mBorderRightColor(StyleColor::CurrentColor()),
      mBorderBottomColor(StyleColor::CurrentColor()),
      mBorderLeftColor(StyleColor::CurrentColor()),
      mComputedBorder(0, 0, 0, 0),
      mTwipsPerPixel(TwipsPerPixel(aDocument)) {
  MOZ_COUNT_CTOR(nsStyleBorder);

  nscoord medium = kMediumBorderWidth;
  for (const auto side : mozilla::AllPhysicalSides()) {
    mBorder.Side(side) = medium;
    mBorderStyle[side] = StyleBorderStyle::None;
  }
}

nsStyleBorder::nsStyleBorder(const nsStyleBorder& aSrc)
    : mBorderRadius(aSrc.mBorderRadius),
      mBorderImageSource(aSrc.mBorderImageSource),
      mBorderImageWidth(aSrc.mBorderImageWidth),
      mBorderImageOutset(aSrc.mBorderImageOutset),
      mBorderImageSlice(aSrc.mBorderImageSlice),
      mBorderImageRepeatH(aSrc.mBorderImageRepeatH),
      mBorderImageRepeatV(aSrc.mBorderImageRepeatV),
      mFloatEdge(aSrc.mFloatEdge),
      mBoxDecorationBreak(aSrc.mBoxDecorationBreak),
      mBorderTopColor(aSrc.mBorderTopColor),
      mBorderRightColor(aSrc.mBorderRightColor),
      mBorderBottomColor(aSrc.mBorderBottomColor),
      mBorderLeftColor(aSrc.mBorderLeftColor),
      mComputedBorder(aSrc.mComputedBorder),
      mBorder(aSrc.mBorder),
      mTwipsPerPixel(aSrc.mTwipsPerPixel) {
  MOZ_COUNT_CTOR(nsStyleBorder);
  for (const auto side : mozilla::AllPhysicalSides()) {
    mBorderStyle[side] = aSrc.mBorderStyle[side];
  }
}

nsStyleBorder::~nsStyleBorder() { MOZ_COUNT_DTOR(nsStyleBorder); }

void nsStyleBorder::TriggerImageLoads(Document& aDocument,
                                      const nsStyleBorder* aOldStyle) {
  MOZ_ASSERT(NS_IsMainThread());

  mBorderImageSource.ResolveImage(
      aDocument, aOldStyle ? &aOldStyle->mBorderImageSource : nullptr);
}

nsMargin nsStyleBorder::GetImageOutset() const {
  // We don't check whether there is a border-image (which is OK since
  // the initial values yields 0 outset) so that we don't have to
  // reflow to update overflow areas when an image loads.
  nsMargin outset;
  for (const auto s : mozilla::AllPhysicalSides()) {
    const auto& coord = mBorderImageOutset.Get(s);
    nscoord value;
    if (coord.IsLength()) {
      value = coord.AsLength().ToAppUnits();
    } else {
      MOZ_ASSERT(coord.IsNumber());
      value = coord.AsNumber() * mComputedBorder.Side(s);
    }
    outset.Side(s) = value;
  }
  return outset;
}

nsChangeHint nsStyleBorder::CalcDifference(
    const nsStyleBorder& aNewData) const {
  // FIXME: XXXbz: As in nsStylePadding::CalcDifference, many of these
  // differences should not need to clear descendant intrinsics.
  // FIXME: It would be good to return a weaker hint for the
  // GetComputedBorder() differences (and perhaps others) that doesn't
  // force reflow of all descendants, but the hint would need to force
  // reflow of the frame's children (see how
  // ReflowInput::InitResizeFlags initializes the inline-resize flag).
  if (mTwipsPerPixel != aNewData.mTwipsPerPixel ||
      GetComputedBorder() != aNewData.GetComputedBorder() ||
      mFloatEdge != aNewData.mFloatEdge ||
      mBorderImageOutset != aNewData.mBorderImageOutset ||
      mBoxDecorationBreak != aNewData.mBoxDecorationBreak) {
    return NS_STYLE_HINT_REFLOW;
  }

  for (const auto ix : mozilla::AllPhysicalSides()) {
    // See the explanation in nsChangeHint.h of
    // nsChangeHint_BorderStyleNoneChange .
    // Furthermore, even though we know *this* side is 0 width, just
    // assume a repaint hint for some other change rather than bother
    // tracking this result through the rest of the function.
    if (HasVisibleStyle(ix) != aNewData.HasVisibleStyle(ix)) {
      return nsChangeHint_RepaintFrame | nsChangeHint_BorderStyleNoneChange;
    }
  }

  // Note that mBorderStyle stores not only the border style but also
  // color-related flags.  Given that we've already done an mComputedBorder
  // comparison, border-style differences can only lead to a repaint hint.  So
  // it's OK to just compare the values directly -- if either the actual
  // style or the color flags differ we want to repaint.
  for (const auto ix : mozilla::AllPhysicalSides()) {
    if (mBorderStyle[ix] != aNewData.mBorderStyle[ix] ||
        BorderColorFor(ix) != aNewData.BorderColorFor(ix)) {
      return nsChangeHint_RepaintFrame;
    }
  }

  if (mBorderRadius != aNewData.mBorderRadius) {
    return nsChangeHint_RepaintFrame;
  }

  // Loading status of the border image can be accessed in main thread only
  // while CalcDifference might be executed on a background thread. As a
  // result, we have to check mBorderImage* fields even before border image was
  // actually loaded.
  if (!mBorderImageSource.IsNone() || !aNewData.mBorderImageSource.IsNone()) {
    if (mBorderImageSource != aNewData.mBorderImageSource ||
        mBorderImageRepeatH != aNewData.mBorderImageRepeatH ||
        mBorderImageRepeatV != aNewData.mBorderImageRepeatV ||
        mBorderImageSlice != aNewData.mBorderImageSlice ||
        mBorderImageWidth != aNewData.mBorderImageWidth) {
      return nsChangeHint_RepaintFrame;
    }
  }

  // mBorder is the specified border value.  Changes to this don't
  // need any change processing, since we operate on the computed
  // border values instead.
  if (mBorder != aNewData.mBorder) {
    return nsChangeHint_NeutralChange;
  }

  // mBorderImage* fields are checked only when border-image is not 'none'.
  if (mBorderImageSource != aNewData.mBorderImageSource ||
      mBorderImageRepeatH != aNewData.mBorderImageRepeatH ||
      mBorderImageRepeatV != aNewData.mBorderImageRepeatV ||
      mBorderImageSlice != aNewData.mBorderImageSlice ||
      mBorderImageWidth != aNewData.mBorderImageWidth) {
    return nsChangeHint_NeutralChange;
  }

  return nsChangeHint(0);
}

nsStyleOutline::nsStyleOutline(const Document& aDocument)
    : mOutlineRadius(ZeroBorderRadius()),
      mOutlineWidth(kMediumBorderWidth),
      mOutlineOffset({0.0f}),
      mOutlineColor(StyleColor::CurrentColor()),
      mOutlineStyle(StyleOutlineStyle::BorderStyle(StyleBorderStyle::None)),
      mActualOutlineWidth(0),
      mTwipsPerPixel(TwipsPerPixel(aDocument)) {
  MOZ_COUNT_CTOR(nsStyleOutline);
}

nsStyleOutline::nsStyleOutline(const nsStyleOutline& aSrc)
    : mOutlineRadius(aSrc.mOutlineRadius),
      mOutlineWidth(aSrc.mOutlineWidth),
      mOutlineOffset(aSrc.mOutlineOffset),
      mOutlineColor(aSrc.mOutlineColor),
      mOutlineStyle(aSrc.mOutlineStyle),
      mActualOutlineWidth(aSrc.mActualOutlineWidth),
      mTwipsPerPixel(aSrc.mTwipsPerPixel) {
  MOZ_COUNT_CTOR(nsStyleOutline);
}

nsChangeHint nsStyleOutline::CalcDifference(
    const nsStyleOutline& aNewData) const {
  if (mActualOutlineWidth != aNewData.mActualOutlineWidth ||
      (mActualOutlineWidth > 0 && mOutlineOffset != aNewData.mOutlineOffset)) {
    return nsChangeHint_UpdateOverflow | nsChangeHint_SchedulePaint |
           nsChangeHint_RepaintFrame;
  }

  if (mOutlineStyle != aNewData.mOutlineStyle ||
      mOutlineColor != aNewData.mOutlineColor ||
      mOutlineRadius != aNewData.mOutlineRadius) {
    if (mActualOutlineWidth > 0) {
      return nsChangeHint_RepaintFrame;
    }
    return nsChangeHint_NeutralChange;
  }

  if (mOutlineWidth != aNewData.mOutlineWidth ||
      mOutlineOffset != aNewData.mOutlineOffset ||
      mTwipsPerPixel != aNewData.mTwipsPerPixel) {
    return nsChangeHint_NeutralChange;
  }

  return nsChangeHint(0);
}

// --------------------
// nsStyleList
//
nsStyleList::nsStyleList(const Document& aDocument)
    : mListStylePosition(NS_STYLE_LIST_STYLE_POSITION_OUTSIDE),
      mQuotes(StyleQuotes::Auto()),
      mListStyleImage(StyleImageUrlOrNone::None()),
      mImageRegion(StyleClipRectOrAuto::Auto()),
      mMozListReversed(StyleMozListReversed::False) {
  MOZ_COUNT_CTOR(nsStyleList);
  MOZ_ASSERT(NS_IsMainThread());

  mCounterStyle = nsGkAtoms::disc;
}

nsStyleList::~nsStyleList() { MOZ_COUNT_DTOR(nsStyleList); }

nsStyleList::nsStyleList(const nsStyleList& aSource)
    : mListStylePosition(aSource.mListStylePosition),
      mCounterStyle(aSource.mCounterStyle),
      mQuotes(aSource.mQuotes),
      mListStyleImage(aSource.mListStyleImage),
      mImageRegion(aSource.mImageRegion),
      mMozListReversed(aSource.mMozListReversed) {
  MOZ_COUNT_CTOR(nsStyleList);
}

void nsStyleList::TriggerImageLoads(Document& aDocument,
                                    const nsStyleList* aOldStyle) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mListStyleImage.IsUrl() && !mListStyleImage.AsUrl().IsImageResolved()) {
    auto* oldUrl = aOldStyle && aOldStyle->mListStyleImage.IsUrl()
                       ? &aOldStyle->mListStyleImage.AsUrl()
                       : nullptr;
    const_cast<StyleComputedImageUrl&>(mListStyleImage.AsUrl())
        .ResolveImage(aDocument, oldUrl);
  }
}

nsChangeHint nsStyleList::CalcDifference(
    const nsStyleList& aNewData, const nsStyleDisplay& aOldDisplay) const {
  // If the quotes implementation is ever going to change we might not need
  // a framechange here and a reflow should be sufficient.  See bug 35768.
  if (mQuotes != aNewData.mQuotes) {
    return nsChangeHint_ReconstructFrame;
  }
  nsChangeHint hint = nsChangeHint(0);
  // Only elements whose display value is list-item can be affected by
  // list-style-position and list-style-type. If the old display struct
  // doesn't exist, assume it isn't affected by display value at all,
  // and thus these properties should not affect it either. This also
  // relies on that when the display value changes from something else
  // to list-item, that change itself would cause ReconstructFrame.
  if (aOldDisplay.IsListItem()) {
    if (mListStylePosition != aNewData.mListStylePosition) {
      return nsChangeHint_ReconstructFrame;
    }
    if (mCounterStyle != aNewData.mCounterStyle) {
      return NS_STYLE_HINT_REFLOW;
    }
  } else if (mListStylePosition != aNewData.mListStylePosition ||
             mCounterStyle != aNewData.mCounterStyle) {
    hint = nsChangeHint_NeutralChange;
  }
  // This is an internal UA-sheet property that is true only for <ol reversed>
  // so hopefully it changes rarely.
  if (mMozListReversed != aNewData.mMozListReversed) {
    return NS_STYLE_HINT_REFLOW;
  }
  // list-style-image and -moz-image-region may affect some XUL elements
  // regardless of display value, so we still need to check them.
  if (mListStyleImage != aNewData.mListStyleImage) {
    return NS_STYLE_HINT_REFLOW;
  }
  if (mImageRegion != aNewData.mImageRegion) {
    nsRect region = GetImageRegion();
    nsRect newRegion = aNewData.GetImageRegion();
    if (region.width != newRegion.width || region.height != newRegion.height) {
      return NS_STYLE_HINT_REFLOW;
    }
    return NS_STYLE_HINT_VISUAL;
  }
  return hint;
}

already_AddRefed<nsIURI> nsStyleList::GetListStyleImageURI() const {
  if (!mListStyleImage.IsUrl()) {
    return nullptr;
  }

  nsCOMPtr<nsIURI> uri = mListStyleImage.AsUrl().GetURI();
  return uri.forget();
}

// --------------------
// nsStyleXUL
//
nsStyleXUL::nsStyleXUL(const Document& aDocument)
    : mBoxFlex(0.0f),
      mBoxOrdinal(1),
      mBoxAlign(StyleBoxAlign::Stretch),
      mBoxDirection(StyleBoxDirection::Normal),
      mBoxOrient(StyleBoxOrient::Horizontal),
      mBoxPack(StyleBoxPack::Start) {
  MOZ_COUNT_CTOR(nsStyleXUL);
}

nsStyleXUL::~nsStyleXUL() { MOZ_COUNT_DTOR(nsStyleXUL); }

nsStyleXUL::nsStyleXUL(const nsStyleXUL& aSource)
    : mBoxFlex(aSource.mBoxFlex),
      mBoxOrdinal(aSource.mBoxOrdinal),
      mBoxAlign(aSource.mBoxAlign),
      mBoxDirection(aSource.mBoxDirection),
      mBoxOrient(aSource.mBoxOrient),
      mBoxPack(aSource.mBoxPack) {
  MOZ_COUNT_CTOR(nsStyleXUL);
}

nsChangeHint nsStyleXUL::CalcDifference(const nsStyleXUL& aNewData) const {
  if (mBoxAlign == aNewData.mBoxAlign &&
      mBoxDirection == aNewData.mBoxDirection &&
      mBoxFlex == aNewData.mBoxFlex && mBoxOrient == aNewData.mBoxOrient &&
      mBoxPack == aNewData.mBoxPack && mBoxOrdinal == aNewData.mBoxOrdinal) {
    return nsChangeHint(0);
  }
  if (mBoxOrdinal != aNewData.mBoxOrdinal) {
    return nsChangeHint_ReconstructFrame;
  }
  return NS_STYLE_HINT_REFLOW;
}

// --------------------
// nsStyleColumn
//
/* static */ const uint32_t nsStyleColumn::kMaxColumnCount;
/* static */ const uint32_t nsStyleColumn::kColumnCountAuto;

nsStyleColumn::nsStyleColumn(const Document& aDocument)
    : mColumnWidth(LengthOrAuto::Auto()),
      mColumnRuleColor(StyleColor::CurrentColor()),
      mColumnRuleStyle(StyleBorderStyle::None),
      mColumnRuleWidth(kMediumBorderWidth),
      mTwipsPerPixel(TwipsPerPixel(aDocument)) {
  MOZ_COUNT_CTOR(nsStyleColumn);
}

nsStyleColumn::~nsStyleColumn() { MOZ_COUNT_DTOR(nsStyleColumn); }

nsStyleColumn::nsStyleColumn(const nsStyleColumn& aSource)
    : mColumnCount(aSource.mColumnCount),
      mColumnWidth(aSource.mColumnWidth),
      mColumnRuleColor(aSource.mColumnRuleColor),
      mColumnRuleStyle(aSource.mColumnRuleStyle),
      mColumnFill(aSource.mColumnFill),
      mColumnSpan(aSource.mColumnSpan),
      mColumnRuleWidth(aSource.mColumnRuleWidth),
      mTwipsPerPixel(aSource.mTwipsPerPixel) {
  MOZ_COUNT_CTOR(nsStyleColumn);
}

nsChangeHint nsStyleColumn::CalcDifference(
    const nsStyleColumn& aNewData) const {
  if (mColumnWidth.IsAuto() != aNewData.mColumnWidth.IsAuto() ||
      mColumnCount != aNewData.mColumnCount ||
      mColumnSpan != aNewData.mColumnSpan) {
    // We force column count changes to do a reframe, because it's tricky to
    // handle some edge cases where the column count gets smaller and content
    // overflows.
    // XXX not ideal
    return nsChangeHint_ReconstructFrame;
  }

  if (mColumnWidth != aNewData.mColumnWidth ||
      mColumnFill != aNewData.mColumnFill) {
    return NS_STYLE_HINT_REFLOW;
  }

  if (GetComputedColumnRuleWidth() != aNewData.GetComputedColumnRuleWidth() ||
      mColumnRuleStyle != aNewData.mColumnRuleStyle ||
      mColumnRuleColor != aNewData.mColumnRuleColor) {
    return NS_STYLE_HINT_VISUAL;
  }

  // XXX Is it right that we never check mTwipsPerPixel to return a
  // non-nsChangeHint_NeutralChange hint?
  if (mColumnRuleWidth != aNewData.mColumnRuleWidth ||
      mTwipsPerPixel != aNewData.mTwipsPerPixel) {
    return nsChangeHint_NeutralChange;
  }

  return nsChangeHint(0);
}

using SVGPaintFallback = StyleGenericSVGPaintFallback<StyleColor>;

// --------------------
// nsStyleSVG
//
nsStyleSVG::nsStyleSVG(const Document& aDocument)
    : mFill{StyleSVGPaintKind::Color(StyleColor::Black()),
            SVGPaintFallback::Unset()},
      mStroke{StyleSVGPaintKind::None(), SVGPaintFallback::Unset()},
      mMarkerEnd(StyleUrlOrNone::None()),
      mMarkerMid(StyleUrlOrNone::None()),
      mMarkerStart(StyleUrlOrNone::None()),
      mMozContextProperties{{}, {0}},
      mStrokeDasharray(StyleSVGStrokeDashArray::Values({})),
      mStrokeDashoffset(
          StyleSVGLength::LengthPercentage(LengthPercentage::Zero())),
      mStrokeWidth(
          StyleSVGWidth::LengthPercentage(LengthPercentage::FromPixels(1.0f))),
      mFillOpacity(StyleSVGOpacity::Opacity(1.0f)),
      mStrokeMiterlimit(4.0f),
      mStrokeOpacity(StyleSVGOpacity::Opacity(1.0f)),
      mClipRule(StyleFillRule::Nonzero),
      mColorInterpolation(StyleColorInterpolation::Srgb),
      mColorInterpolationFilters(StyleColorInterpolation::Linearrgb),
      mFillRule(StyleFillRule::Nonzero),
      mPaintOrder(0),
      mShapeRendering(StyleShapeRendering::Auto),
      mStrokeLinecap(StyleStrokeLinecap::Butt),
      mStrokeLinejoin(StyleStrokeLinejoin::Miter),
      mDominantBaseline(StyleDominantBaseline::Auto),
      mTextAnchor(StyleTextAnchor::Start) {
  MOZ_COUNT_CTOR(nsStyleSVG);
}

nsStyleSVG::~nsStyleSVG() { MOZ_COUNT_DTOR(nsStyleSVG); }

nsStyleSVG::nsStyleSVG(const nsStyleSVG& aSource)
    : mFill(aSource.mFill),
      mStroke(aSource.mStroke),
      mMarkerEnd(aSource.mMarkerEnd),
      mMarkerMid(aSource.mMarkerMid),
      mMarkerStart(aSource.mMarkerStart),
      mMozContextProperties(aSource.mMozContextProperties),
      mStrokeDasharray(aSource.mStrokeDasharray),
      mStrokeDashoffset(aSource.mStrokeDashoffset),
      mStrokeWidth(aSource.mStrokeWidth),
      mFillOpacity(aSource.mFillOpacity),
      mStrokeMiterlimit(aSource.mStrokeMiterlimit),
      mStrokeOpacity(aSource.mStrokeOpacity),
      mClipRule(aSource.mClipRule),
      mColorInterpolation(aSource.mColorInterpolation),
      mColorInterpolationFilters(aSource.mColorInterpolationFilters),
      mFillRule(aSource.mFillRule),
      mPaintOrder(aSource.mPaintOrder),
      mShapeRendering(aSource.mShapeRendering),
      mStrokeLinecap(aSource.mStrokeLinecap),
      mStrokeLinejoin(aSource.mStrokeLinejoin),
      mDominantBaseline(aSource.mDominantBaseline),
      mTextAnchor(aSource.mTextAnchor) {
  MOZ_COUNT_CTOR(nsStyleSVG);
}

static bool PaintURIChanged(const StyleSVGPaint& aPaint1,
                            const StyleSVGPaint& aPaint2) {
  if (aPaint1.kind.IsPaintServer() != aPaint2.kind.IsPaintServer()) {
    return true;
  }
  return aPaint1.kind.IsPaintServer() &&
         aPaint1.kind.AsPaintServer() != aPaint2.kind.AsPaintServer();
}

nsChangeHint nsStyleSVG::CalcDifference(const nsStyleSVG& aNewData) const {
  nsChangeHint hint = nsChangeHint(0);

  if (mMarkerEnd != aNewData.mMarkerEnd || mMarkerMid != aNewData.mMarkerMid ||
      mMarkerStart != aNewData.mMarkerStart) {
    // Markers currently contribute to SVGGeometryFrame::mRect,
    // so we need a reflow as well as a repaint. No intrinsic sizes need
    // to change, so nsChangeHint_NeedReflow is sufficient.
    return nsChangeHint_UpdateEffects | nsChangeHint_NeedReflow |
           nsChangeHint_NeedDirtyReflow |  // XXX remove me: bug 876085
           nsChangeHint_RepaintFrame;
  }

  if (mFill != aNewData.mFill || mStroke != aNewData.mStroke ||
      mFillOpacity != aNewData.mFillOpacity ||
      mStrokeOpacity != aNewData.mStrokeOpacity) {
    hint |= nsChangeHint_RepaintFrame;
    if (HasStroke() != aNewData.HasStroke() ||
        (!HasStroke() && HasFill() != aNewData.HasFill())) {
      // Frame bounds and overflow rects depend on whether we "have" fill or
      // stroke. Whether we have stroke or not just changed, or else we have no
      // stroke (in which case whether we have fill or not is significant to
      // frame bounds) and whether we have fill or not just changed. In either
      // case we need to reflow so the frame rect is updated.
      // XXXperf this is a waste on non SVGGeometryFrames.
      hint |= nsChangeHint_NeedReflow |
              nsChangeHint_NeedDirtyReflow;  // XXX remove me: bug 876085
    }
    if (PaintURIChanged(mFill, aNewData.mFill) ||
        PaintURIChanged(mStroke, aNewData.mStroke)) {
      hint |= nsChangeHint_UpdateEffects;
    }
  }

  // Stroke currently contributes to SVGGeometryFrame::mRect, so
  // we need a reflow here. No intrinsic sizes need to change, so
  // nsChangeHint_NeedReflow is sufficient.
  // Note that stroke-dashoffset does not affect SVGGeometryFrame::mRect.
  // text-anchor and dominant-baseline changes also require a reflow since
  // they change frames' rects.
  if (mStrokeWidth != aNewData.mStrokeWidth ||
      mStrokeMiterlimit != aNewData.mStrokeMiterlimit ||
      mStrokeLinecap != aNewData.mStrokeLinecap ||
      mStrokeLinejoin != aNewData.mStrokeLinejoin ||
      mDominantBaseline != aNewData.mDominantBaseline ||
      mTextAnchor != aNewData.mTextAnchor) {
    return hint | nsChangeHint_NeedReflow |
           nsChangeHint_NeedDirtyReflow |  // XXX remove me: bug 876085
           nsChangeHint_RepaintFrame;
  }

  if (hint & nsChangeHint_RepaintFrame) {
    return hint;  // we don't add anything else below
  }

  if (mStrokeDashoffset != aNewData.mStrokeDashoffset ||
      mClipRule != aNewData.mClipRule ||
      mColorInterpolation != aNewData.mColorInterpolation ||
      mColorInterpolationFilters != aNewData.mColorInterpolationFilters ||
      mFillRule != aNewData.mFillRule || mPaintOrder != aNewData.mPaintOrder ||
      mShapeRendering != aNewData.mShapeRendering ||
      mStrokeDasharray != aNewData.mStrokeDasharray ||
      mMozContextProperties.bits != aNewData.mMozContextProperties.bits) {
    return hint | nsChangeHint_RepaintFrame;
  }

  if (!hint) {
    if (mMozContextProperties.idents != aNewData.mMozContextProperties.idents) {
      hint = nsChangeHint_NeutralChange;
    }
  }

  return hint;
}

// --------------------
// nsStyleSVGReset
//
nsStyleSVGReset::nsStyleSVGReset(const Document& aDocument)
    : mX(LengthPercentage::Zero()),
      mY(LengthPercentage::Zero()),
      mCx(LengthPercentage::Zero()),
      mCy(LengthPercentage::Zero()),
      mRx(NonNegativeLengthPercentageOrAuto::Auto()),
      mRy(NonNegativeLengthPercentageOrAuto::Auto()),
      mR(NonNegativeLengthPercentage::Zero()),
      mMask(nsStyleImageLayers::LayerType::Mask),
      mClipPath(StyleClipPath::None()),
      mStopColor(StyleColor::Black()),
      mFloodColor(StyleColor::Black()),
      mLightingColor(StyleColor::White()),
      mStopOpacity(1.0f),
      mFloodOpacity(1.0f),
      mVectorEffect(StyleVectorEffect::None),
      mMaskType(StyleMaskType::Luminance) {
  MOZ_COUNT_CTOR(nsStyleSVGReset);
}

nsStyleSVGReset::~nsStyleSVGReset() { MOZ_COUNT_DTOR(nsStyleSVGReset); }

nsStyleSVGReset::nsStyleSVGReset(const nsStyleSVGReset& aSource)
    : mX(aSource.mX),
      mY(aSource.mY),
      mCx(aSource.mCx),
      mCy(aSource.mCy),
      mRx(aSource.mRx),
      mRy(aSource.mRy),
      mR(aSource.mR),
      mMask(aSource.mMask),
      mClipPath(aSource.mClipPath),
      mStopColor(aSource.mStopColor),
      mFloodColor(aSource.mFloodColor),
      mLightingColor(aSource.mLightingColor),
      mStopOpacity(aSource.mStopOpacity),
      mFloodOpacity(aSource.mFloodOpacity),
      mVectorEffect(aSource.mVectorEffect),
      mMaskType(aSource.mMaskType) {
  MOZ_COUNT_CTOR(nsStyleSVGReset);
}

void nsStyleSVGReset::TriggerImageLoads(Document& aDocument,
                                        const nsStyleSVGReset* aOldStyle) {
  MOZ_ASSERT(NS_IsMainThread());
  // NOTE(emilio): we intentionally don't call TriggerImageLoads for clip-path.

  NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT(i, mMask) {
    auto& image = mMask.mLayers[i].mImage;
    if (!image.IsImageRequestType()) {
      continue;
    }
    const auto* url = image.GetImageRequestURLValue();
    // If the url is a local ref, it must be a <mask-resource>, so we don't
    // need to resolve the style image.
    if (url->IsLocalRef()) {
      continue;
    }
#if 0
    // XXX The old style system also checks whether this is a reference to
    // the current document with reference, but it doesn't seem to be a
    // behavior mentioned anywhere, so we comment out the code for now.
    nsIURI* docURI = aPresContext->Document()->GetDocumentURI();
    if (url->EqualsExceptRef(docURI)) {
      continue;
    }
#endif
    // Otherwise, we may need the image even if it has a reference, in case
    // the referenced element isn't a valid SVG <mask> element.
    const auto* oldImage = (aOldStyle && aOldStyle->mMask.mLayers.Length() > i)
                               ? &aOldStyle->mMask.mLayers[i].mImage
                               : nullptr;

    image.ResolveImage(aDocument, oldImage);
  }
}

nsChangeHint nsStyleSVGReset::CalcDifference(
    const nsStyleSVGReset& aNewData) const {
  nsChangeHint hint = nsChangeHint(0);

  if (mX != aNewData.mX || mY != aNewData.mY || mCx != aNewData.mCx ||
      mCy != aNewData.mCy || mR != aNewData.mR || mRx != aNewData.mRx ||
      mRy != aNewData.mRy) {
    hint |= nsChangeHint_InvalidateRenderingObservers | nsChangeHint_NeedReflow;
  }

  if (mClipPath != aNewData.mClipPath) {
    hint |= nsChangeHint_UpdateEffects | nsChangeHint_RepaintFrame;
  }

  if (mVectorEffect != aNewData.mVectorEffect) {
    // Stroke currently affects SVGGeometryFrame::mRect, and
    // vector-effect affect stroke. As a result we need to reflow if
    // vector-effect changes in order to have SVGGeometryFrame::
    // ReflowSVG called to update its mRect. No intrinsic sizes need
    // to change so nsChangeHint_NeedReflow is sufficient.
    hint |= nsChangeHint_NeedReflow |
            nsChangeHint_NeedDirtyReflow |  // XXX remove me: bug 876085
            nsChangeHint_RepaintFrame;
  } else if (mStopColor != aNewData.mStopColor ||
             mFloodColor != aNewData.mFloodColor ||
             mLightingColor != aNewData.mLightingColor ||
             mStopOpacity != aNewData.mStopOpacity ||
             mFloodOpacity != aNewData.mFloodOpacity ||
             mMaskType != aNewData.mMaskType) {
    hint |= nsChangeHint_RepaintFrame;
  }

  hint |=
      mMask.CalcDifference(aNewData.mMask, nsStyleImageLayers::LayerType::Mask);

  return hint;
}

bool nsStyleSVGReset::HasMask() const {
  for (uint32_t i = 0; i < mMask.mImageCount; i++) {
    if (!mMask.mLayers[i].mImage.IsNone()) {
      return true;
    }
  }

  return false;
}

// --------------------
// nsStylePosition
//
nsStylePosition::nsStylePosition(const Document& aDocument)
    : mObjectPosition(Position::FromPercentage(0.5f)),
      mOffset(StyleRectWithAllSides(LengthPercentageOrAuto::Auto())),
      mWidth(StyleSize::Auto()),
      mMinWidth(StyleSize::Auto()),
      mMaxWidth(StyleMaxSize::None()),
      mHeight(StyleSize::Auto()),
      mMinHeight(StyleSize::Auto()),
      mMaxHeight(StyleMaxSize::None()),
      mFlexBasis(StyleFlexBasis::Size(StyleSize::Auto())),
      mAspectRatio(0.0f),
      mGridAutoFlow(StyleGridAutoFlow::ROW),
      mBoxSizing(StyleBoxSizing::Content),
      mAlignContent({StyleAlignFlags::NORMAL}),
      mAlignItems({StyleAlignFlags::NORMAL}),
      mAlignSelf({StyleAlignFlags::AUTO}),
      mJustifyContent({StyleAlignFlags::NORMAL}),
      mJustifyItems({{StyleAlignFlags::LEGACY}, {StyleAlignFlags::NORMAL}}),
      mJustifySelf({StyleAlignFlags::AUTO}),
      mFlexDirection(StyleFlexDirection::Row),
      mFlexWrap(StyleFlexWrap::Nowrap),
      mObjectFit(StyleObjectFit::Fill),
      mOrder(NS_STYLE_ORDER_INITIAL),
      mFlexGrow(0.0f),
      mFlexShrink(1.0f),
      mZIndex(StyleZIndex::Auto()),
      mGridTemplateColumns(StyleGridTemplateComponent::None()),
      mGridTemplateRows(StyleGridTemplateComponent::None()),
      mGridTemplateAreas(StyleGridTemplateAreas::None()),
      mColumnGap(NonNegativeLengthPercentageOrNormal::Normal()),
      mRowGap(NonNegativeLengthPercentageOrNormal::Normal()) {
  MOZ_COUNT_CTOR(nsStylePosition);

  // The initial value of grid-auto-columns and grid-auto-rows is 'auto',
  // which computes to 'minmax(auto, auto)'.

  // Other members get their default constructors
  // which initialize them to representations of their respective initial value.
  // mGridTemplate{Rows,Columns}: false and empty arrays for 'none'
  // mGrid{Column,Row}{Start,End}: false/0/empty values for 'auto'
}

nsStylePosition::~nsStylePosition() { MOZ_COUNT_DTOR(nsStylePosition); }

nsStylePosition::nsStylePosition(const nsStylePosition& aSource)
    : mObjectPosition(aSource.mObjectPosition),
      mOffset(aSource.mOffset),
      mWidth(aSource.mWidth),
      mMinWidth(aSource.mMinWidth),
      mMaxWidth(aSource.mMaxWidth),
      mHeight(aSource.mHeight),
      mMinHeight(aSource.mMinHeight),
      mMaxHeight(aSource.mMaxHeight),
      mFlexBasis(aSource.mFlexBasis),
      mGridAutoColumns(aSource.mGridAutoColumns),
      mGridAutoRows(aSource.mGridAutoRows),
      mAspectRatio(aSource.mAspectRatio),
      mGridAutoFlow(aSource.mGridAutoFlow),
      mBoxSizing(aSource.mBoxSizing),
      mAlignContent(aSource.mAlignContent),
      mAlignItems(aSource.mAlignItems),
      mAlignSelf(aSource.mAlignSelf),
      mJustifyContent(aSource.mJustifyContent),
      mJustifyItems(aSource.mJustifyItems),
      mJustifySelf(aSource.mJustifySelf),
      mFlexDirection(aSource.mFlexDirection),
      mFlexWrap(aSource.mFlexWrap),
      mObjectFit(aSource.mObjectFit),
      mOrder(aSource.mOrder),
      mFlexGrow(aSource.mFlexGrow),
      mFlexShrink(aSource.mFlexShrink),
      mZIndex(aSource.mZIndex),
      mGridTemplateColumns(aSource.mGridTemplateColumns),
      mGridTemplateRows(aSource.mGridTemplateRows),
      mGridTemplateAreas(aSource.mGridTemplateAreas),
      mGridColumnStart(aSource.mGridColumnStart),
      mGridColumnEnd(aSource.mGridColumnEnd),
      mGridRowStart(aSource.mGridRowStart),
      mGridRowEnd(aSource.mGridRowEnd),
      mColumnGap(aSource.mColumnGap),
      mRowGap(aSource.mRowGap) {
  MOZ_COUNT_CTOR(nsStylePosition);
}

static bool IsAutonessEqual(const StyleRect<LengthPercentageOrAuto>& aSides1,
                            const StyleRect<LengthPercentageOrAuto>& aSides2) {
  for (const auto side : mozilla::AllPhysicalSides()) {
    if (aSides1.Get(side).IsAuto() != aSides2.Get(side).IsAuto()) {
      return false;
    }
  }
  return true;
}

nsChangeHint nsStylePosition::CalcDifference(
    const nsStylePosition& aNewData,
    const nsStyleVisibility& aOldStyleVisibility) const {
  nsChangeHint hint = nsChangeHint(0);

  // Changes to "z-index" require a repaint.
  if (mZIndex != aNewData.mZIndex) {
    hint |= nsChangeHint_RepaintFrame;
  }

  // Changes to "object-fit" & "object-position" require a repaint.  They
  // may also require a reflow, if we have a nsSubDocumentFrame, so that we
  // can adjust the size & position of the subdocument.
  if (mObjectFit != aNewData.mObjectFit ||
      mObjectPosition != aNewData.mObjectPosition) {
    hint |= nsChangeHint_RepaintFrame | nsChangeHint_NeedReflow;
  }

  if (mOrder != aNewData.mOrder) {
    // "order" impacts both layout order and stacking order, so we need both a
    // reflow and a repaint when it changes.  (Technically, we only need a
    // reflow if we're in a multi-line flexbox (which we can't be sure about,
    // since that's determined by styling on our parent) -- there, "order" can
    // affect which flex line we end up on, & hence can affect our sizing by
    // changing the group of flex items we're competing with for space.)
    return hint | nsChangeHint_RepaintFrame | nsChangeHint_AllReflowHints;
  }

  if (mBoxSizing != aNewData.mBoxSizing) {
    // Can affect both widths and heights; just a bad scene.
    return hint | nsChangeHint_AllReflowHints;
  }

  // Properties that apply to flex items:
  // XXXdholbert These should probably be more targeted (bug 819536)
  if (mAlignSelf != aNewData.mAlignSelf || mFlexBasis != aNewData.mFlexBasis ||
      mFlexGrow != aNewData.mFlexGrow || mFlexShrink != aNewData.mFlexShrink) {
    return hint | nsChangeHint_AllReflowHints;
  }

  // Properties that apply to flex containers:
  // - flex-direction can swap a flex container between vertical & horizontal.
  // - align-items can change the sizing of a flex container & the positioning
  //   of its children.
  // - flex-wrap changes whether a flex container's children are wrapped, which
  //   impacts their sizing/positioning and hence impacts the container's size.
  if (mAlignItems != aNewData.mAlignItems ||
      mFlexDirection != aNewData.mFlexDirection ||
      mFlexWrap != aNewData.mFlexWrap) {
    return hint | nsChangeHint_AllReflowHints;
  }

  // Properties that apply to grid containers:
  // FIXME: only for grid containers
  // (ie. 'display: grid' or 'display: inline-grid')
  if (mGridTemplateColumns != aNewData.mGridTemplateColumns ||
      mGridTemplateRows != aNewData.mGridTemplateRows ||
      mGridTemplateAreas != aNewData.mGridTemplateAreas ||
      mGridAutoColumns != aNewData.mGridAutoColumns ||
      mGridAutoRows != aNewData.mGridAutoRows ||
      mGridAutoFlow != aNewData.mGridAutoFlow) {
    return hint | nsChangeHint_AllReflowHints;
  }

  // Properties that apply to grid items:
  // FIXME: only for grid items
  // (ie. parent frame is 'display: grid' or 'display: inline-grid')
  if (mGridColumnStart != aNewData.mGridColumnStart ||
      mGridColumnEnd != aNewData.mGridColumnEnd ||
      mGridRowStart != aNewData.mGridRowStart ||
      mGridRowEnd != aNewData.mGridRowEnd ||
      mColumnGap != aNewData.mColumnGap || mRowGap != aNewData.mRowGap) {
    return hint | nsChangeHint_AllReflowHints;
  }

  // Changing 'justify-content/items/self' might affect the positioning,
  // but it won't affect any sizing.
  if (mJustifyContent != aNewData.mJustifyContent ||
      mJustifyItems.computed != aNewData.mJustifyItems.computed ||
      mJustifySelf != aNewData.mJustifySelf) {
    hint |= nsChangeHint_NeedReflow;
  }

  // No need to do anything if specified justify-items changes, as long as the
  // computed one (tested above) is unchanged.
  if (mJustifyItems.specified != aNewData.mJustifyItems.specified) {
    hint |= nsChangeHint_NeutralChange;
  }

  // 'align-content' doesn't apply to a single-line flexbox but we don't know
  // if we're a flex container at this point so we can't optimize for that.
  if (mAlignContent != aNewData.mAlignContent) {
    hint |= nsChangeHint_NeedReflow;
  }

  bool widthChanged = mWidth != aNewData.mWidth ||
                      mMinWidth != aNewData.mMinWidth ||
                      mMaxWidth != aNewData.mMaxWidth;
  bool heightChanged = mHeight != aNewData.mHeight ||
                       mMinHeight != aNewData.mMinHeight ||
                       mMaxHeight != aNewData.mMaxHeight;

  // It doesn't matter whether we're looking at the old or new visibility
  // struct, since a change between vertical and horizontal writing-mode will
  // cause a reframe.
  bool isVertical =
      aOldStyleVisibility.mWritingMode != NS_STYLE_WRITING_MODE_HORIZONTAL_TB;
  if (isVertical ? widthChanged : heightChanged) {
    hint |= nsChangeHint_ReflowHintsForBSizeChange;
  }

  if (isVertical ? heightChanged : widthChanged) {
    hint |= nsChangeHint_ReflowHintsForISizeChange;
  }

  if (mAspectRatio != aNewData.mAspectRatio) {
    hint |= nsChangeHint_ReflowHintsForISizeChange |
            nsChangeHint_ReflowHintsForBSizeChange;
  }

  // If any of the offsets have changed, then return the respective hints
  // so that we would hopefully be able to avoid reflowing.
  // Note that it is possible that we'll need to reflow when processing
  // restyles, but we don't have enough information to make a good decision
  // right now.
  // Don't try to handle changes between "auto" and non-auto efficiently;
  // that's tricky to do and will hardly ever be able to avoid a reflow.
  if (mOffset != aNewData.mOffset) {
    if (IsAutonessEqual(mOffset, aNewData.mOffset)) {
      hint |=
          nsChangeHint_RecomputePosition | nsChangeHint_UpdateParentOverflow;
    } else {
      hint |=
          nsChangeHint_NeedReflow | nsChangeHint_ReflowChangesSizeOrPosition;
    }
  }
  return hint;
}

StyleAlignSelf nsStylePosition::UsedAlignSelf(
    const ComputedStyle* aParent) const {
  if (mAlignSelf._0 != StyleAlignFlags::AUTO) {
    return mAlignSelf;
  }
  if (MOZ_LIKELY(aParent)) {
    auto parentAlignItems = aParent->StylePosition()->mAlignItems;
    MOZ_ASSERT(!(parentAlignItems._0 & StyleAlignFlags::LEGACY),
               "align-items can't have 'legacy'");
    return {parentAlignItems._0};
  }
  return {StyleAlignFlags::NORMAL};
}

StyleJustifySelf nsStylePosition::UsedJustifySelf(
    const ComputedStyle* aParent) const {
  if (mJustifySelf._0 != StyleAlignFlags::AUTO) {
    return mJustifySelf;
  }
  if (MOZ_LIKELY(aParent)) {
    const auto& inheritedJustifyItems =
        aParent->StylePosition()->mJustifyItems.computed;
    return {inheritedJustifyItems._0 & ~StyleAlignFlags::LEGACY};
  }
  return {StyleAlignFlags::NORMAL};
}

// --------------------
// nsStyleTable
//

nsStyleTable::nsStyleTable(const Document& aDocument)
    : mLayoutStrategy(StyleTableLayout::Auto), mXSpan(1) {
  MOZ_COUNT_CTOR(nsStyleTable);
}

nsStyleTable::~nsStyleTable() { MOZ_COUNT_DTOR(nsStyleTable); }

nsStyleTable::nsStyleTable(const nsStyleTable& aSource)
    : mLayoutStrategy(aSource.mLayoutStrategy), mXSpan(aSource.mXSpan) {
  MOZ_COUNT_CTOR(nsStyleTable);
}

nsChangeHint nsStyleTable::CalcDifference(const nsStyleTable& aNewData) const {
  if (mXSpan != aNewData.mXSpan ||
      mLayoutStrategy != aNewData.mLayoutStrategy) {
    return nsChangeHint_ReconstructFrame;
  }
  return nsChangeHint(0);
}

// -----------------------
// nsStyleTableBorder

nsStyleTableBorder::nsStyleTableBorder(const Document& aDocument)
    : mBorderSpacingCol(0),
      mBorderSpacingRow(0),
      mBorderCollapse(StyleBorderCollapse::Separate),
      mCaptionSide(NS_STYLE_CAPTION_SIDE_TOP),
      mEmptyCells(StyleEmptyCells::Show) {
  MOZ_COUNT_CTOR(nsStyleTableBorder);
}

nsStyleTableBorder::~nsStyleTableBorder() {
  MOZ_COUNT_DTOR(nsStyleTableBorder);
}

nsStyleTableBorder::nsStyleTableBorder(const nsStyleTableBorder& aSource)
    : mBorderSpacingCol(aSource.mBorderSpacingCol),
      mBorderSpacingRow(aSource.mBorderSpacingRow),
      mBorderCollapse(aSource.mBorderCollapse),
      mCaptionSide(aSource.mCaptionSide),
      mEmptyCells(aSource.mEmptyCells) {
  MOZ_COUNT_CTOR(nsStyleTableBorder);
}

nsChangeHint nsStyleTableBorder::CalcDifference(
    const nsStyleTableBorder& aNewData) const {
  // Border-collapse changes need a reframe, because we use a different frame
  // class for table cells in the collapsed border model.  This is used to
  // conserve memory when using the separated border model (collapsed borders
  // require extra state to be stored).
  if (mBorderCollapse != aNewData.mBorderCollapse) {
    return nsChangeHint_ReconstructFrame;
  }

  if ((mCaptionSide == aNewData.mCaptionSide) &&
      (mBorderSpacingCol == aNewData.mBorderSpacingCol) &&
      (mBorderSpacingRow == aNewData.mBorderSpacingRow)) {
    if (mEmptyCells == aNewData.mEmptyCells) {
      return nsChangeHint(0);
    }
    return NS_STYLE_HINT_VISUAL;
  } else {
    return NS_STYLE_HINT_REFLOW;
  }
}

template <typename T>
static bool GradientItemsAreOpaque(
    Span<const StyleGenericGradientItem<StyleColor, T>> aItems) {
  for (auto& stop : aItems) {
    if (stop.IsInterpolationHint()) {
      continue;
    }

    auto& color = stop.IsSimpleColorStop() ? stop.AsSimpleColorStop()
                                           : stop.AsComplexColorStop().color;
    if (color.MaybeTransparent()) {
      // We don't know the foreground color here, so if it's being used
      // we must assume it might be transparent.
      return false;
    }
  }

  return true;
}

template <>
bool StyleGradient::IsOpaque() const {
  if (IsLinear()) {
    return GradientItemsAreOpaque(AsLinear().items.AsSpan());
  }
  if (IsRadial()) {
    return GradientItemsAreOpaque(AsRadial().items.AsSpan());
  }
  return GradientItemsAreOpaque(AsConic().items.AsSpan());
}

// --------------------
// CachedBorderImageData

void CachedBorderImageData::PurgeCachedImages() {
  MOZ_ASSERT(!ServoStyleSet::IsInServoTraversal());
  MOZ_ASSERT(NS_IsMainThread());
  mSubImages.Clear();
}

void CachedBorderImageData::PurgeCacheForViewportChange(
    const Maybe<nsSize>& aSize, const bool aHasIntrinsicRatio) {
  // If we're redrawing with a different viewport-size than we used for our
  // cached subimages, then we can't trust that our subimages are valid;
  // any percent sizes/positions in our SVG doc may be different now. Purge!
  // (We don't have to purge if the SVG document has an intrinsic ratio,
  // though, because the actual size of elements in SVG documant's coordinate
  // axis are fixed in this case.)
  if (aSize != mCachedSVGViewportSize && !aHasIntrinsicRatio) {
    PurgeCachedImages();
    SetCachedSVGViewportSize(aSize);
  }
}

static int32_t ConvertToPixelCoord(const StyleNumberOrPercentage& aCoord,
                                   int32_t aPercentScale) {
  double pixelValue;
  if (aCoord.IsNumber()) {
    pixelValue = aCoord.AsNumber();
  } else {
    MOZ_ASSERT(aCoord.IsPercentage());
    pixelValue = aCoord.AsPercentage()._0 * aPercentScale;
  }
  MOZ_ASSERT(pixelValue >= 0, "we ensured non-negative while parsing");
  pixelValue = std::min(pixelValue, double(INT32_MAX));  // avoid overflow
  return NS_lround(pixelValue);
}

template <>
Maybe<StyleImage::ActualCropRect> StyleImage::ComputeActualCropRect() const {
  MOZ_ASSERT(IsRect(),
             "This function is designed to be used only image-rect images");

  imgRequestProxy* req = GetImageRequest();
  if (!req) {
    return Nothing();
  }

  nsCOMPtr<imgIContainer> imageContainer;
  req->GetImage(getter_AddRefs(imageContainer));
  if (!imageContainer) {
    return Nothing();
  }

  nsIntSize imageSize;
  imageContainer->GetWidth(&imageSize.width);
  imageContainer->GetHeight(&imageSize.height);
  if (imageSize.width <= 0 || imageSize.height <= 0) {
    return Nothing();
  }

  auto& rect = AsRect();

  int32_t left = ConvertToPixelCoord(rect->left, imageSize.width);
  int32_t top = ConvertToPixelCoord(rect->top, imageSize.height);
  int32_t right = ConvertToPixelCoord(rect->right, imageSize.width);
  int32_t bottom = ConvertToPixelCoord(rect->bottom, imageSize.height);

  // IntersectRect() returns an empty rect if we get negative width or height
  nsIntRect cropRect(left, top, right - left, bottom - top);
  nsIntRect imageRect(nsIntPoint(0, 0), imageSize);
  auto finalRect = imageRect.Intersect(cropRect);
  bool isEntireImage = finalRect.IsEqualInterior(imageRect);
  return Some(ActualCropRect{finalRect, isEntireImage});
}

template <>
bool StyleImage::StartDecoding() const {
  if (IsImageRequestType()) {
    imgRequestProxy* req = GetImageRequest();
    return req &&
           req->StartDecodingWithResult(imgIContainer::FLAG_ASYNC_NOTIFY);
  }
  // None always returns false from IsComplete, so we do the same here.
  return !IsNone();
}

template <>
bool StyleImage::IsOpaque() const {
  if (!IsComplete()) {
    return false;
  }

  if (IsGradient()) {
    return AsGradient()->IsOpaque();
  }

  if (IsElement()) {
    return false;
  }

  MOZ_ASSERT(IsImageRequestType(), "unexpected image type");
  MOZ_ASSERT(GetImageRequest(), "should've returned earlier above");

  nsCOMPtr<imgIContainer> imageContainer;
  GetImageRequest()->GetImage(getter_AddRefs(imageContainer));
  MOZ_ASSERT(imageContainer, "IsComplete() said image container is ready");

  // Check if the crop region of the image is opaque.
  if (imageContainer->WillDrawOpaqueNow()) {
    if (!IsRect()) {
      return true;
    }

    // Must make sure if the crop rect contains at least a pixel.
    // XXX Is this optimization worth it? Maybe I should just return false.
    auto croprect = ComputeActualCropRect();
    return croprect && !croprect->mRect.IsEmpty();
  }

  return false;
}

template <>
bool StyleImage::IsComplete() const {
  switch (tag) {
    case Tag::None:
      return false;
    case Tag::Gradient:
    case Tag::Element:
      return true;
    case Tag::Url:
    case Tag::Rect: {
      if (!IsResolved()) {
        return false;
      }
      imgRequestProxy* req = GetImageRequest();
      if (!req) {
        return false;
      }
      uint32_t status = imgIRequest::STATUS_ERROR;
      return NS_SUCCEEDED(req->GetImageStatus(&status)) &&
             (status & imgIRequest::STATUS_SIZE_AVAILABLE) &&
             (status & imgIRequest::STATUS_FRAME_COMPLETE);
    }
    default:
      MOZ_ASSERT_UNREACHABLE("unexpected image type");
      return false;
  }
}

template <>
bool StyleImage::IsSizeAvailable() const {
  switch (tag) {
    case Tag::None:
      return false;
    case Tag::Gradient:
    case Tag::Element:
      return true;
    case Tag::Url:
    case Tag::Rect: {
      imgRequestProxy* req = GetImageRequest();
      if (!req) {
        return false;
      }
      uint32_t status = imgIRequest::STATUS_ERROR;
      return NS_SUCCEEDED(req->GetImageStatus(&status)) &&
             !(status & imgIRequest::STATUS_ERROR) &&
             (status & imgIRequest::STATUS_SIZE_AVAILABLE);
    }
    default:
      MOZ_ASSERT_UNREACHABLE("unexpected image type");
      return false;
  }
}

template <>
void StyleImage::ResolveImage(Document& aDoc, const StyleImage* aOld) {
  if (IsResolved()) {
    return;
  }
  auto* old = aOld ? aOld->GetImageRequestURLValue() : nullptr;
  auto* url = GetImageRequestURLValue();
  // We could avoid this const_cast generating more code but it's not really
  // worth it.
  const_cast<StyleComputedImageUrl*>(url)->ResolveImage(aDoc, old);
}

// --------------------
// nsStyleImageLayers
//

const nsCSSPropertyID nsStyleImageLayers::kBackgroundLayerTable[] = {
    eCSSProperty_background,             // shorthand
    eCSSProperty_background_color,       // color
    eCSSProperty_background_image,       // image
    eCSSProperty_background_repeat,      // repeat
    eCSSProperty_background_position_x,  // positionX
    eCSSProperty_background_position_y,  // positionY
    eCSSProperty_background_clip,        // clip
    eCSSProperty_background_origin,      // origin
    eCSSProperty_background_size,        // size
    eCSSProperty_background_attachment,  // attachment
    eCSSProperty_UNKNOWN,                // maskMode
    eCSSProperty_UNKNOWN                 // composite
};

const nsCSSPropertyID nsStyleImageLayers::kMaskLayerTable[] = {
    eCSSProperty_mask,             // shorthand
    eCSSProperty_UNKNOWN,          // color
    eCSSProperty_mask_image,       // image
    eCSSProperty_mask_repeat,      // repeat
    eCSSProperty_mask_position_x,  // positionX
    eCSSProperty_mask_position_y,  // positionY
    eCSSProperty_mask_clip,        // clip
    eCSSProperty_mask_origin,      // origin
    eCSSProperty_mask_size,        // size
    eCSSProperty_UNKNOWN,          // attachment
    eCSSProperty_mask_mode,        // maskMode
    eCSSProperty_mask_composite    // composite
};

nsStyleImageLayers::nsStyleImageLayers(nsStyleImageLayers::LayerType aType)
    : mAttachmentCount(1),
      mClipCount(1),
      mOriginCount(1),
      mRepeatCount(1),
      mPositionXCount(1),
      mPositionYCount(1),
      mImageCount(1),
      mSizeCount(1),
      mMaskModeCount(1),
      mBlendModeCount(1),
      mCompositeCount(1),
      mLayers(nsStyleAutoArray<Layer>::WITH_SINGLE_INITIAL_ELEMENT) {
  MOZ_COUNT_CTOR(nsStyleImageLayers);

  // Ensure first layer is initialized as specified layer type
  mLayers[0].Initialize(aType);
}

nsStyleImageLayers::nsStyleImageLayers(const nsStyleImageLayers& aSource)
    : mAttachmentCount(aSource.mAttachmentCount),
      mClipCount(aSource.mClipCount),
      mOriginCount(aSource.mOriginCount),
      mRepeatCount(aSource.mRepeatCount),
      mPositionXCount(aSource.mPositionXCount),
      mPositionYCount(aSource.mPositionYCount),
      mImageCount(aSource.mImageCount),
      mSizeCount(aSource.mSizeCount),
      mMaskModeCount(aSource.mMaskModeCount),
      mBlendModeCount(aSource.mBlendModeCount),
      mCompositeCount(aSource.mCompositeCount),
      mLayers(aSource.mLayers)  // deep copy
{
  MOZ_COUNT_CTOR(nsStyleImageLayers);
  // If the deep copy of mLayers failed, truncate the counts.
  uint32_t count = mLayers.Length();
  if (count != aSource.mLayers.Length()) {
    NS_WARNING("truncating counts due to out-of-memory");
    mAttachmentCount = std::max(mAttachmentCount, count);
    mClipCount = std::max(mClipCount, count);
    mOriginCount = std::max(mOriginCount, count);
    mRepeatCount = std::max(mRepeatCount, count);
    mPositionXCount = std::max(mPositionXCount, count);
    mPositionYCount = std::max(mPositionYCount, count);
    mImageCount = std::max(mImageCount, count);
    mSizeCount = std::max(mSizeCount, count);
    mMaskModeCount = std::max(mMaskModeCount, count);
    mBlendModeCount = std::max(mBlendModeCount, count);
    mCompositeCount = std::max(mCompositeCount, count);
  }
}

static bool AnyLayerIsElementImage(const nsStyleImageLayers& aLayers) {
  NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT(i, aLayers) {
    if (aLayers.mLayers[i].mImage.IsElement()) {
      return true;
    }
  }
  return false;
}

nsChangeHint nsStyleImageLayers::CalcDifference(
    const nsStyleImageLayers& aNewLayers, LayerType aType) const {
  nsChangeHint hint = nsChangeHint(0);

  // If the number of visible images changes, then it's easy-peasy.
  if (mImageCount != aNewLayers.mImageCount) {
    hint |= nsChangeHint_RepaintFrame;
    if (aType == nsStyleImageLayers::LayerType::Mask ||
        AnyLayerIsElementImage(*this) || AnyLayerIsElementImage(aNewLayers)) {
      hint |= nsChangeHint_UpdateEffects;
    }
    return hint;
  }

  const nsStyleImageLayers& moreLayers =
      mLayers.Length() > aNewLayers.mLayers.Length() ? *this : aNewLayers;
  const nsStyleImageLayers& lessLayers =
      mLayers.Length() > aNewLayers.mLayers.Length() ? aNewLayers : *this;

  for (size_t i = 0; i < moreLayers.mLayers.Length(); ++i) {
    const Layer& moreLayersLayer = moreLayers.mLayers[i];
    if (i < moreLayers.mImageCount) {
      // This is a visible image we're diffing, we may need to repaint.
      const Layer& lessLayersLayer = lessLayers.mLayers[i];
      nsChangeHint layerDifference =
          moreLayersLayer.CalcDifference(lessLayersLayer);
      if (layerDifference && (moreLayersLayer.mImage.IsElement() ||
                              lessLayersLayer.mImage.IsElement())) {
        layerDifference |=
            nsChangeHint_UpdateEffects | nsChangeHint_RepaintFrame;
      }
      hint |= layerDifference;
      continue;
    }
    if (hint) {
      // If they're different by now, we're done.
      return hint;
    }
    if (i >= lessLayers.mLayers.Length()) {
      // The layer count differs, we know some property has changed, but if we
      // got here we know it won't affect rendering.
      return nsChangeHint_NeutralChange;
    }

    const Layer& lessLayersLayer = lessLayers.mLayers[i];
    MOZ_ASSERT(moreLayersLayer.mImage.IsNone());
    MOZ_ASSERT(lessLayersLayer.mImage.IsNone());
    if (moreLayersLayer.CalcDifference(lessLayersLayer)) {
      // We don't care about the difference returned, we know it's not visible,
      // but if something changed, then we need to return the neutral change.
      return nsChangeHint_NeutralChange;
    }
  }

  if (hint) {
    // If they're different by now, we're done.
    return hint;
  }

  // We could have same layers and values, but different count still.
  if (mAttachmentCount != aNewLayers.mAttachmentCount ||
      mBlendModeCount != aNewLayers.mBlendModeCount ||
      mClipCount != aNewLayers.mClipCount ||
      mCompositeCount != aNewLayers.mCompositeCount ||
      mMaskModeCount != aNewLayers.mMaskModeCount ||
      mOriginCount != aNewLayers.mOriginCount ||
      mRepeatCount != aNewLayers.mRepeatCount ||
      mPositionXCount != aNewLayers.mPositionXCount ||
      mPositionYCount != aNewLayers.mPositionYCount ||
      mSizeCount != aNewLayers.mSizeCount) {
    hint |= nsChangeHint_NeutralChange;
  }

  return hint;
}

nsStyleImageLayers& nsStyleImageLayers::operator=(
    const nsStyleImageLayers& aOther) {
  mAttachmentCount = aOther.mAttachmentCount;
  mClipCount = aOther.mClipCount;
  mOriginCount = aOther.mOriginCount;
  mRepeatCount = aOther.mRepeatCount;
  mPositionXCount = aOther.mPositionXCount;
  mPositionYCount = aOther.mPositionYCount;
  mImageCount = aOther.mImageCount;
  mSizeCount = aOther.mSizeCount;
  mMaskModeCount = aOther.mMaskModeCount;
  mBlendModeCount = aOther.mBlendModeCount;
  mCompositeCount = aOther.mCompositeCount;
  mLayers = aOther.mLayers;

  uint32_t count = mLayers.Length();
  if (count != aOther.mLayers.Length()) {
    NS_WARNING("truncating counts due to out-of-memory");
    mAttachmentCount = std::max(mAttachmentCount, count);
    mClipCount = std::max(mClipCount, count);
    mOriginCount = std::max(mOriginCount, count);
    mRepeatCount = std::max(mRepeatCount, count);
    mPositionXCount = std::max(mPositionXCount, count);
    mPositionYCount = std::max(mPositionYCount, count);
    mImageCount = std::max(mImageCount, count);
    mSizeCount = std::max(mSizeCount, count);
    mMaskModeCount = std::max(mMaskModeCount, count);
    mBlendModeCount = std::max(mBlendModeCount, count);
    mCompositeCount = std::max(mCompositeCount, count);
  }

  return *this;
}

nsStyleImageLayers& nsStyleImageLayers::operator=(nsStyleImageLayers&& aOther) {
  mAttachmentCount = aOther.mAttachmentCount;
  mClipCount = aOther.mClipCount;
  mOriginCount = aOther.mOriginCount;
  mRepeatCount = aOther.mRepeatCount;
  mPositionXCount = aOther.mPositionXCount;
  mPositionYCount = aOther.mPositionYCount;
  mImageCount = aOther.mImageCount;
  mSizeCount = aOther.mSizeCount;
  mMaskModeCount = aOther.mMaskModeCount;
  mBlendModeCount = aOther.mBlendModeCount;
  mCompositeCount = aOther.mCompositeCount;
  mLayers = std::move(aOther.mLayers);

  uint32_t count = mLayers.Length();
  if (count != aOther.mLayers.Length()) {
    NS_WARNING("truncating counts due to out-of-memory");
    mAttachmentCount = std::max(mAttachmentCount, count);
    mClipCount = std::max(mClipCount, count);
    mOriginCount = std::max(mOriginCount, count);
    mRepeatCount = std::max(mRepeatCount, count);
    mPositionXCount = std::max(mPositionXCount, count);
    mPositionYCount = std::max(mPositionYCount, count);
    mImageCount = std::max(mImageCount, count);
    mSizeCount = std::max(mSizeCount, count);
    mMaskModeCount = std::max(mMaskModeCount, count);
    mBlendModeCount = std::max(mBlendModeCount, count);
    mCompositeCount = std::max(mCompositeCount, count);
  }

  return *this;
}

bool nsStyleImageLayers::operator==(const nsStyleImageLayers& aOther) const {
  if (mAttachmentCount != aOther.mAttachmentCount ||
      mClipCount != aOther.mClipCount || mOriginCount != aOther.mOriginCount ||
      mRepeatCount != aOther.mRepeatCount ||
      mPositionXCount != aOther.mPositionXCount ||
      mPositionYCount != aOther.mPositionYCount ||
      mImageCount != aOther.mImageCount || mSizeCount != aOther.mSizeCount ||
      mMaskModeCount != aOther.mMaskModeCount ||
      mBlendModeCount != aOther.mBlendModeCount) {
    return false;
  }

  if (mLayers.Length() != aOther.mLayers.Length()) {
    return false;
  }

  for (uint32_t i = 0; i < mLayers.Length(); i++) {
    if (mLayers[i].mPosition != aOther.mLayers[i].mPosition ||
        mLayers[i].mImage != aOther.mLayers[i].mImage ||
        mLayers[i].mSize != aOther.mLayers[i].mSize ||
        mLayers[i].mClip != aOther.mLayers[i].mClip ||
        mLayers[i].mOrigin != aOther.mLayers[i].mOrigin ||
        mLayers[i].mAttachment != aOther.mLayers[i].mAttachment ||
        mLayers[i].mBlendMode != aOther.mLayers[i].mBlendMode ||
        mLayers[i].mComposite != aOther.mLayers[i].mComposite ||
        mLayers[i].mMaskMode != aOther.mLayers[i].mMaskMode ||
        mLayers[i].mRepeat != aOther.mLayers[i].mRepeat) {
      return false;
    }
  }

  return true;
}

static bool SizeDependsOnPositioningAreaSize(const StyleBackgroundSize& aSize,
                                             const StyleImage& aImage) {
  MOZ_ASSERT(!aImage.IsNone(), "caller should have handled this");

  // Contain and cover straightforwardly depend on frame size.
  if (aSize.IsCover() || aSize.IsContain()) {
    return true;
  }

  MOZ_ASSERT(aSize.IsExplicitSize());
  auto& size = aSize.explicit_size;

  // If either dimension contains a non-zero percentage, rendering for that
  // dimension straightforwardly depends on frame size.
  if (size.width.HasPercent() || size.height.HasPercent()) {
    return true;
  }

  // If both dimensions are fixed lengths, there's no dependency.
  if (!size.width.IsAuto() && !size.height.IsAuto()) {
    return false;
  }

  // Gradient rendering depends on frame size when auto is involved because
  // gradients have no intrinsic ratio or dimensions, and therefore the relevant
  // dimension is "treat[ed] as 100%".
  if (aImage.IsGradient()) {
    return true;
  }

  // XXX Element rendering for auto or fixed length doesn't depend on frame size
  //     according to the spec.  However, we don't implement the spec yet, so
  //     for now we bail and say element() plus auto affects ultimate size.
  if (aImage.IsElement()) {
    return true;
  }

  MOZ_ASSERT(aImage.IsImageRequestType(), "Missed some image");
  if (auto* request = aImage.GetImageRequest()) {
    nsCOMPtr<imgIContainer> imgContainer;
    request->GetImage(getter_AddRefs(imgContainer));
    if (imgContainer) {
      CSSIntSize imageSize;
      AspectRatio imageRatio;
      bool hasWidth, hasHeight;
      nsLayoutUtils::ComputeSizeForDrawing(imgContainer, imageSize, imageRatio,
                                           hasWidth, hasHeight);

      // If the image has a fixed width and height, rendering never depends on
      // the frame size.
      if (hasWidth && hasHeight) {
        return false;
      }

      // If the image has an intrinsic ratio, rendering will depend on frame
      // size when background-size is all auto.
      if (imageRatio) {
        return size.width.IsAuto() == size.height.IsAuto();
      }

      // Otherwise, rendering depends on frame size when the image dimensions
      // and background-size don't complement each other.
      return !(hasWidth && size.width.IsLengthPercentage()) &&
             !(hasHeight && size.height.IsLengthPercentage());
    }
  }

  // Passed the gauntlet: no dependency.
  return false;
}

nsStyleImageLayers::Layer::Layer()
    : mImage(StyleImage::None()),
      mSize(StyleBackgroundSize::ExplicitSize(LengthPercentageOrAuto::Auto(),
                                              LengthPercentageOrAuto::Auto())),

      mClip(StyleGeometryBox::BorderBox),
      mAttachment(StyleImageLayerAttachment::Scroll),
      mBlendMode(StyleBlend::Normal),
      mComposite(StyleMaskComposite::Add),
      mMaskMode(StyleMaskMode::MatchSource) {}

nsStyleImageLayers::Layer::~Layer() = default;

void nsStyleImageLayers::Layer::Initialize(
    nsStyleImageLayers::LayerType aType) {
  mRepeat.SetInitialValues();

  mPosition = Position::FromPercentage(0.);

  if (aType == LayerType::Background) {
    mOrigin = StyleGeometryBox::PaddingBox;
  } else {
    MOZ_ASSERT(aType == LayerType::Mask, "unsupported layer type.");
    mOrigin = StyleGeometryBox::BorderBox;
  }
}

bool nsStyleImageLayers::Layer::
    RenderingMightDependOnPositioningAreaSizeChange() const {
  // Do we even have an image?
  if (mImage.IsNone()) {
    return false;
  }

  return mPosition.DependsOnPositioningAreaSize() ||
         SizeDependsOnPositioningAreaSize(mSize, mImage) ||
         mRepeat.DependsOnPositioningAreaSize();
}

bool nsStyleImageLayers::Layer::operator==(const Layer& aOther) const {
  return mAttachment == aOther.mAttachment && mClip == aOther.mClip &&
         mOrigin == aOther.mOrigin && mRepeat == aOther.mRepeat &&
         mBlendMode == aOther.mBlendMode && mPosition == aOther.mPosition &&
         mSize == aOther.mSize && mImage == aOther.mImage &&
         mMaskMode == aOther.mMaskMode && mComposite == aOther.mComposite;
}

template <class ComputedValueItem>
static void FillImageLayerList(
    nsStyleAutoArray<nsStyleImageLayers::Layer>& aLayers,
    ComputedValueItem nsStyleImageLayers::Layer::*aResultLocation,
    uint32_t aItemCount, uint32_t aFillCount) {
  MOZ_ASSERT(aFillCount <= aLayers.Length(), "unexpected array length");
  for (uint32_t sourceLayer = 0, destLayer = aItemCount; destLayer < aFillCount;
       ++sourceLayer, ++destLayer) {
    aLayers[destLayer].*aResultLocation = aLayers[sourceLayer].*aResultLocation;
  }
}

// The same as FillImageLayerList, but for values stored in
// layer.mPosition.*aResultLocation instead of layer.*aResultLocation.
static void FillImageLayerPositionCoordList(
    nsStyleAutoArray<nsStyleImageLayers::Layer>& aLayers,
    LengthPercentage Position::*aResultLocation, uint32_t aItemCount,
    uint32_t aFillCount) {
  MOZ_ASSERT(aFillCount <= aLayers.Length(), "unexpected array length");
  for (uint32_t sourceLayer = 0, destLayer = aItemCount; destLayer < aFillCount;
       ++sourceLayer, ++destLayer) {
    aLayers[destLayer].mPosition.*aResultLocation =
        aLayers[sourceLayer].mPosition.*aResultLocation;
  }
}

void nsStyleImageLayers::FillAllLayers(uint32_t aMaxItemCount) {
  // Delete any extra items.  We need to keep layers in which any
  // property was specified.
  mLayers.TruncateLengthNonZero(aMaxItemCount);

  uint32_t fillCount = mImageCount;
  FillImageLayerList(mLayers, &Layer::mImage, mImageCount, fillCount);
  FillImageLayerList(mLayers, &Layer::mRepeat, mRepeatCount, fillCount);
  FillImageLayerList(mLayers, &Layer::mAttachment, mAttachmentCount, fillCount);
  FillImageLayerList(mLayers, &Layer::mClip, mClipCount, fillCount);
  FillImageLayerList(mLayers, &Layer::mBlendMode, mBlendModeCount, fillCount);
  FillImageLayerList(mLayers, &Layer::mOrigin, mOriginCount, fillCount);
  FillImageLayerPositionCoordList(mLayers, &Position::horizontal,
                                  mPositionXCount, fillCount);
  FillImageLayerPositionCoordList(mLayers, &Position::vertical, mPositionYCount,
                                  fillCount);
  FillImageLayerList(mLayers, &Layer::mSize, mSizeCount, fillCount);
  FillImageLayerList(mLayers, &Layer::mMaskMode, mMaskModeCount, fillCount);
  FillImageLayerList(mLayers, &Layer::mComposite, mCompositeCount, fillCount);
}

static bool UrlValuesEqual(const StyleImage& aImage,
                           const StyleImage& aOtherImage) {
  auto* url = aImage.GetImageRequestURLValue();
  auto* other = aOtherImage.GetImageRequestURLValue();
  return url == other || (url && other && *url == *other);
}

nsChangeHint nsStyleImageLayers::Layer::CalcDifference(
    const nsStyleImageLayers::Layer& aNewLayer) const {
  nsChangeHint hint = nsChangeHint(0);
  if (!UrlValuesEqual(mImage, aNewLayer.mImage)) {
    hint |= nsChangeHint_RepaintFrame | nsChangeHint_UpdateEffects;
  } else if (mAttachment != aNewLayer.mAttachment || mClip != aNewLayer.mClip ||
             mOrigin != aNewLayer.mOrigin || mRepeat != aNewLayer.mRepeat ||
             mBlendMode != aNewLayer.mBlendMode || mSize != aNewLayer.mSize ||
             mImage != aNewLayer.mImage || mMaskMode != aNewLayer.mMaskMode ||
             mComposite != aNewLayer.mComposite) {
    hint |= nsChangeHint_RepaintFrame;
  }

  if (mPosition != aNewLayer.mPosition) {
    hint |= nsChangeHint_UpdateBackgroundPosition;
  }

  return hint;
}

// --------------------
// nsStyleBackground
//

nsStyleBackground::nsStyleBackground(const Document& aDocument)
    : mImage(nsStyleImageLayers::LayerType::Background),
      mBackgroundColor(StyleColor::Transparent()) {
  MOZ_COUNT_CTOR(nsStyleBackground);
}

nsStyleBackground::nsStyleBackground(const nsStyleBackground& aSource)
    : mImage(aSource.mImage), mBackgroundColor(aSource.mBackgroundColor) {
  MOZ_COUNT_CTOR(nsStyleBackground);
}

nsStyleBackground::~nsStyleBackground() { MOZ_COUNT_DTOR(nsStyleBackground); }

void nsStyleBackground::TriggerImageLoads(Document& aDocument,
                                          const nsStyleBackground* aOldStyle) {
  MOZ_ASSERT(NS_IsMainThread());
  mImage.ResolveImages(aDocument, aOldStyle ? &aOldStyle->mImage : nullptr);
}

nsChangeHint nsStyleBackground::CalcDifference(
    const nsStyleBackground& aNewData) const {
  nsChangeHint hint = nsChangeHint(0);
  if (mBackgroundColor != aNewData.mBackgroundColor) {
    hint |= nsChangeHint_RepaintFrame;
  }

  hint |= mImage.CalcDifference(aNewData.mImage,
                                nsStyleImageLayers::LayerType::Background);

  return hint;
}

bool nsStyleBackground::HasFixedBackground(nsIFrame* aFrame) const {
  NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT(i, mImage) {
    const nsStyleImageLayers::Layer& layer = mImage.mLayers[i];
    if (layer.mAttachment == StyleImageLayerAttachment::Fixed &&
        !layer.mImage.IsNone() && !nsLayoutUtils::IsTransformed(aFrame)) {
      return true;
    }
  }
  return false;
}

nscolor nsStyleBackground::BackgroundColor(const nsIFrame* aFrame) const {
  return mBackgroundColor.CalcColor(aFrame);
}

nscolor nsStyleBackground::BackgroundColor(ComputedStyle* aStyle) const {
  return mBackgroundColor.CalcColor(*aStyle);
}

bool nsStyleBackground::IsTransparent(const nsIFrame* aFrame) const {
  return IsTransparent(aFrame->Style());
}

bool nsStyleBackground::IsTransparent(mozilla::ComputedStyle* aStyle) const {
  return BottomLayer().mImage.IsNone() && mImage.mImageCount == 1 &&
         NS_GET_A(BackgroundColor(aStyle)) == 0;
}

StyleTransition::StyleTransition(const StyleTransition& aCopy) = default;

void StyleTransition::SetInitialValues() {
  mTimingFunction = nsTimingFunction(StyleTimingKeyword::Ease);
  mDuration = 0.0;
  mDelay = 0.0;
  mProperty = eCSSPropertyExtra_all_properties;
}

bool StyleTransition::operator==(const StyleTransition& aOther) const {
  return mTimingFunction == aOther.mTimingFunction &&
         mDuration == aOther.mDuration && mDelay == aOther.mDelay &&
         mProperty == aOther.mProperty &&
         (mProperty != eCSSProperty_UNKNOWN ||
          mUnknownProperty == aOther.mUnknownProperty);
}

StyleAnimation::StyleAnimation(const StyleAnimation& aCopy) = default;

void StyleAnimation::SetInitialValues() {
  mTimingFunction = nsTimingFunction(StyleTimingKeyword::Ease);
  mDuration = 0.0;
  mDelay = 0.0;
  mName = nsGkAtoms::_empty;
  mDirection = dom::PlaybackDirection::Normal;
  mFillMode = dom::FillMode::None;
  mPlayState = StyleAnimationPlayState::Running;
  mIterationCount = 1.0f;
}

bool StyleAnimation::operator==(const StyleAnimation& aOther) const {
  return mTimingFunction == aOther.mTimingFunction &&
         mDuration == aOther.mDuration && mDelay == aOther.mDelay &&
         mName == aOther.mName && mDirection == aOther.mDirection &&
         mFillMode == aOther.mFillMode && mPlayState == aOther.mPlayState &&
         mIterationCount == aOther.mIterationCount;
}

// --------------------
// nsStyleDisplay
//
nsStyleDisplay::nsStyleDisplay(const Document& aDocument)
    : mTransitions(
          nsStyleAutoArray<StyleTransition>::WITH_SINGLE_INITIAL_ELEMENT),
      mTransitionTimingFunctionCount(1),
      mTransitionDurationCount(1),
      mTransitionDelayCount(1),
      mTransitionPropertyCount(1),
      mAnimations(
          nsStyleAutoArray<StyleAnimation>::WITH_SINGLE_INITIAL_ELEMENT),
      mAnimationTimingFunctionCount(1),
      mAnimationDurationCount(1),
      mAnimationDelayCount(1),
      mAnimationNameCount(1),
      mAnimationDirectionCount(1),
      mAnimationFillModeCount(1),
      mAnimationPlayStateCount(1),
      mAnimationIterationCountCount(1),
      mWillChange{{}, {0}},
      mDisplay(StyleDisplay::Inline),
      mOriginalDisplay(StyleDisplay::Inline),
      mContain(StyleContain::NONE),
      mAppearance(StyleAppearance::None),
      mPosition(StylePositionProperty::Static),
      mFloat(StyleFloat::None),
      mBreakType(StyleClear::None),
      mBreakInside(StyleBreakWithin::Auto),
      mBreakBefore(StyleBreakBetween::Auto),
      mBreakAfter(StyleBreakBetween::Auto),
      mOverflowX(StyleOverflow::Visible),
      mOverflowY(StyleOverflow::Visible),
      mOverflowClipBoxBlock(StyleOverflowClipBox::PaddingBox),
      mOverflowClipBoxInline(StyleOverflowClipBox::PaddingBox),
      mResize(StyleResize::None),
      mOrient(StyleOrient::Inline),
      mIsolation(StyleIsolation::Auto),
      mTopLayer(StyleTopLayer::None),
      mTouchAction(StyleTouchAction::AUTO),
      mScrollBehavior(StyleScrollBehavior::Auto),
      mOverscrollBehaviorX(StyleOverscrollBehavior::Auto),
      mOverscrollBehaviorY(StyleOverscrollBehavior::Auto),
      mOverflowAnchor(StyleOverflowAnchor::Auto),
      mScrollSnapAlign{StyleScrollSnapAlignKeyword::None,
                       StyleScrollSnapAlignKeyword::None},
      mScrollSnapType{StyleScrollSnapAxis::Both,
                      StyleScrollSnapStrictness::None},
      mLineClamp(0),
      mRotate(StyleRotate::None()),
      mTranslate(StyleTranslate::None()),
      mScale(StyleScale::None()),
      mBackfaceVisibility(StyleBackfaceVisibility::Visible),
      mTransformStyle(StyleTransformStyle::Flat),
      mTransformBox(StyleGeometryBox::BorderBox),
      mOffsetPath(StyleOffsetPath::None()),
      mOffsetDistance(LengthPercentage::Zero()),
      mOffsetRotate{true, StyleAngle{0.0}},
      mOffsetAnchor(StylePositionOrAuto::Auto()),
      mTransformOrigin{LengthPercentage::FromPercentage(0.5),
                       LengthPercentage::FromPercentage(0.5),
                       {0.}},
      mChildPerspective(StylePerspective::None()),
      mPerspectiveOrigin(Position::FromPercentage(0.5f)),
      mVerticalAlign(
          StyleVerticalAlign::Keyword(StyleVerticalAlignKeyword::Baseline)),
      mShapeMargin(LengthPercentage::Zero()),
      mShapeOutside(StyleShapeOutside::None()) {
  MOZ_COUNT_CTOR(nsStyleDisplay);

  mTransitions[0].SetInitialValues();
  mAnimations[0].SetInitialValues();
}

nsStyleDisplay::nsStyleDisplay(const nsStyleDisplay& aSource)
    : mTransitions(aSource.mTransitions),
      mTransitionTimingFunctionCount(aSource.mTransitionTimingFunctionCount),
      mTransitionDurationCount(aSource.mTransitionDurationCount),
      mTransitionDelayCount(aSource.mTransitionDelayCount),
      mTransitionPropertyCount(aSource.mTransitionPropertyCount),
      mAnimations(aSource.mAnimations),
      mAnimationTimingFunctionCount(aSource.mAnimationTimingFunctionCount),
      mAnimationDurationCount(aSource.mAnimationDurationCount),
      mAnimationDelayCount(aSource.mAnimationDelayCount),
      mAnimationNameCount(aSource.mAnimationNameCount),
      mAnimationDirectionCount(aSource.mAnimationDirectionCount),
      mAnimationFillModeCount(aSource.mAnimationFillModeCount),
      mAnimationPlayStateCount(aSource.mAnimationPlayStateCount),
      mAnimationIterationCountCount(aSource.mAnimationIterationCountCount),
      mWillChange(aSource.mWillChange),
      mDisplay(aSource.mDisplay),
      mOriginalDisplay(aSource.mOriginalDisplay),
      mContain(aSource.mContain),
      mAppearance(aSource.mAppearance),
      mPosition(aSource.mPosition),
      mFloat(aSource.mFloat),
      mBreakType(aSource.mBreakType),
      mBreakInside(aSource.mBreakInside),
      mBreakBefore(aSource.mBreakBefore),
      mBreakAfter(aSource.mBreakAfter),
      mOverflowX(aSource.mOverflowX),
      mOverflowY(aSource.mOverflowY),
      mOverflowClipBoxBlock(aSource.mOverflowClipBoxBlock),
      mOverflowClipBoxInline(aSource.mOverflowClipBoxInline),
      mResize(aSource.mResize),
      mOrient(aSource.mOrient),
      mIsolation(aSource.mIsolation),
      mTopLayer(aSource.mTopLayer),
      mTouchAction(aSource.mTouchAction),
      mScrollBehavior(aSource.mScrollBehavior),
      mOverscrollBehaviorX(aSource.mOverscrollBehaviorX),
      mOverscrollBehaviorY(aSource.mOverscrollBehaviorY),
      mOverflowAnchor(aSource.mOverflowAnchor),
      mScrollSnapAlign(aSource.mScrollSnapAlign),
      mScrollSnapType(aSource.mScrollSnapType),
      mLineClamp(aSource.mLineClamp),
      mTransform(aSource.mTransform),
      mRotate(aSource.mRotate),
      mTranslate(aSource.mTranslate),
      mScale(aSource.mScale),
      mBackfaceVisibility(aSource.mBackfaceVisibility),
      mTransformStyle(aSource.mTransformStyle),
      mTransformBox(aSource.mTransformBox),
      mOffsetPath(aSource.mOffsetPath),
      mOffsetDistance(aSource.mOffsetDistance),
      mOffsetRotate(aSource.mOffsetRotate),
      mOffsetAnchor(aSource.mOffsetAnchor),
      mTransformOrigin(aSource.mTransformOrigin),
      mChildPerspective(aSource.mChildPerspective),
      mPerspectiveOrigin(aSource.mPerspectiveOrigin),
      mVerticalAlign(aSource.mVerticalAlign),
      mShapeImageThreshold(aSource.mShapeImageThreshold),
      mShapeMargin(aSource.mShapeMargin),
      mShapeOutside(aSource.mShapeOutside) {
  MOZ_COUNT_CTOR(nsStyleDisplay);
}

nsStyleDisplay::~nsStyleDisplay() { MOZ_COUNT_DTOR(nsStyleDisplay); }

void nsStyleDisplay::TriggerImageLoads(Document& aDocument,
                                       const nsStyleDisplay* aOldStyle) {
  MOZ_ASSERT(NS_IsMainThread());

  if (mShapeOutside.IsImage()) {
    auto* old = aOldStyle && aOldStyle->mShapeOutside.IsImage()
                    ? &aOldStyle->mShapeOutside.AsImage()
                    : nullptr;
    // Const-cast is ugly but legit, we could avoid it by generating mut-casts
    // with cbindgen.
    const_cast<StyleImage&>(mShapeOutside.AsImage())
        .ResolveImage(aDocument, old);
  }
}

template <typename TransformLike>
static inline nsChangeHint CompareTransformValues(
    const TransformLike& aOldTransform, const TransformLike& aNewTransform) {
  nsChangeHint result = nsChangeHint(0);

  // Note: If we add a new change hint for transform changes here, we have to
  // modify KeyframeEffect::CalculateCumulativeChangeHint too!
  if (aOldTransform != aNewTransform) {
    result |= nsChangeHint_UpdateTransformLayer;
    if (!aOldTransform.IsNone() && !aNewTransform.IsNone()) {
      result |= nsChangeHint_UpdatePostTransformOverflow;
    } else {
      result |= nsChangeHint_UpdateOverflow;
    }
  }

  return result;
}

static inline nsChangeHint CompareMotionValues(
    const nsStyleDisplay& aDisplay, const nsStyleDisplay& aNewDisplay) {
  if (aDisplay.mOffsetPath == aNewDisplay.mOffsetPath) {
    if (aDisplay.mOffsetDistance == aNewDisplay.mOffsetDistance &&
        aDisplay.mOffsetRotate == aNewDisplay.mOffsetRotate &&
        aDisplay.mOffsetAnchor == aNewDisplay.mOffsetAnchor) {
      return nsChangeHint(0);
    }

    if (aDisplay.mOffsetPath.IsNone()) {
      return nsChangeHint_NeutralChange;
    }
  }

  // TODO: Bug 1482737: This probably doesn't need to UpdateOverflow
  // (or UpdateTransformLayer) if there's already a transform.
  // Set the same hints as what we use for transform because motion path is
  // a kind of transform and will be combined with other transforms.
  nsChangeHint result = nsChangeHint_UpdateTransformLayer;
  if (!aDisplay.mOffsetPath.IsNone() && !aNewDisplay.mOffsetPath.IsNone()) {
    result |= nsChangeHint_UpdatePostTransformOverflow;
  } else {
    result |= nsChangeHint_UpdateOverflow;
  }
  return result;
}

static bool ScrollbarGenerationChanged(const nsStyleDisplay& aOld,
                                       const nsStyleDisplay& aNew) {
  auto changed = [](StyleOverflow aOld, StyleOverflow aNew) {
    return aOld != aNew &&
           (aOld == StyleOverflow::Hidden || aNew == StyleOverflow::Hidden);
  };
  return changed(aOld.mOverflowX, aNew.mOverflowX) ||
         changed(aOld.mOverflowY, aNew.mOverflowY);
}

nsChangeHint nsStyleDisplay::CalcDifference(
    const nsStyleDisplay& aNewData, const nsStylePosition& aOldPosition) const {
  if (mDisplay != aNewData.mDisplay || mContain != aNewData.mContain ||
      (mFloat == StyleFloat::None) != (aNewData.mFloat == StyleFloat::None) ||
      mScrollBehavior != aNewData.mScrollBehavior ||
      mScrollSnapType != aNewData.mScrollSnapType ||
      mTopLayer != aNewData.mTopLayer || mResize != aNewData.mResize) {
    return nsChangeHint_ReconstructFrame;
  }

  if ((mAppearance == StyleAppearance::Textfield &&
       aNewData.mAppearance != StyleAppearance::Textfield) ||
      (mAppearance != StyleAppearance::Textfield &&
       aNewData.mAppearance == StyleAppearance::Textfield)) {
    // This is for <input type=number> where we allow authors to specify a
    // |-moz-appearance:textfield| to get a control without a spinner. (The
    // spinner is present for |-moz-appearance:number-input| but also other
    // values such as 'none'.) We need to reframe since we want to use
    // nsTextControlFrame instead of nsNumberControlFrame if the author
    // specifies 'textfield'.
    return nsChangeHint_ReconstructFrame;
  }

  auto hint = nsChangeHint(0);
  if (mPosition != aNewData.mPosition) {
    if (IsAbsolutelyPositionedStyle() ||
        aNewData.IsAbsolutelyPositionedStyle()) {
      // This changes our parent relationship on the frame tree and / or needs
      // to create a placeholder, so gotta reframe. There are some cases (when
      // switching from fixed to absolute or viceversa, if our containing block
      // happens to remain the same, i.e., if it has a transform or such) where
      // this wouldn't really be needed (though we'd still need to move the
      // frame from one child list to another). In any case we don't have a hand
      // to that information from here, and it doesn't seem like a case worth
      // optimizing for.
      return nsChangeHint_ReconstructFrame;
    }
    // We start or stop being a containing block for abspos descendants. This
    // also causes painting to change, as we'd become a pseudo-stacking context.
    if (IsRelativelyPositionedStyle() !=
        aNewData.IsRelativelyPositionedStyle()) {
      hint |= nsChangeHint_UpdateContainingBlock | nsChangeHint_RepaintFrame;
    }
    if (IsPositionForcingStackingContext() !=
        aNewData.IsPositionForcingStackingContext()) {
      hint |= nsChangeHint_RepaintFrame;
    }
    // On top of that: if the above ends up not reframing, we need a reflow to
    // compute our relative, static or sticky position.
    hint |= nsChangeHint_NeedReflow | nsChangeHint_ReflowChangesSizeOrPosition;
  }

  if (mScrollSnapAlign != aNewData.mScrollSnapAlign) {
    // FIXME: Bug 1530253 Support re-snapping when scroll-snap-align changes.
    hint |= nsChangeHint_NeutralChange;
  }

  if (mOverflowX != aNewData.mOverflowX || mOverflowY != aNewData.mOverflowY) {
    const bool isScrollable = IsScrollableOverflow();
    if (isScrollable != aNewData.IsScrollableOverflow()) {
      // We may need to construct or destroy a scroll frame as a result of this
      // change.
      hint |= nsChangeHint_ScrollbarChange;
    } else if (isScrollable) {
      if (ScrollbarGenerationChanged(*this, aNewData)) {
        // We need to reframe in the case of hidden -> non-hidden case though,
        // since ScrollFrameHelper::CreateAnonymousContent avoids creating
        // scrollbars altogether for overflow: hidden. That seems it could
        // create some interesting perf cliffs...
        //
        // We reframe when non-hidden -> hidden too, for now.
        //
        // FIXME(bug 1590247): Seems we could avoid reframing once we've created
        // scrollbars, which should get us the optimization for elements that
        // have toggled scrollbars, but would prevent the cliff of toggling
        // overflow causing jank.
        hint |= nsChangeHint_ScrollbarChange;
      } else {
        // Otherwise, for changes where both overflow values are scrollable,
        // means that scrollbars may appear or disappear. We need to reflow,
        // since reflow is what determines which scrollbars if any are visible.
        hint |= nsChangeHint_ReflowHintsForScrollbarChange;
      }
    } else {
      // Otherwise this is a change between visible and
      // -moz-hidden-unscrollable. Here only whether we have a clip changes, so
      // just repaint and update our overflow areas in that case.
      hint |= nsChangeHint_UpdateOverflow | nsChangeHint_RepaintFrame;
    }
  }

  /* Note: When mScrollBehavior or mScrollSnapType are changed,
   * nsChangeHint_NeutralChange is not sufficient to enter
   * nsCSSFrameConstructor::PropagateScrollToViewport. By using the same hint as
   * used when the overflow css property changes, nsChangeHint_ReconstructFrame,
   * PropagateScrollToViewport will be called.
   *
   * The scroll-behavior css property is not expected to change often (the
   * CSSOM-View DOM methods are likely to be used in those cases); however,
   * if this does become common perhaps a faster-path might be worth while.
   *
   * FIXME(emilio): Can we do what we do for overflow changes?
   *
   * FIXME(emilio): These properties no longer propagate from the body to the
   * viewport.
   */

  if (mFloat != aNewData.mFloat) {
    // Changing which side we're floating on (float:none was handled above).
    hint |= nsChangeHint_ReflowHintsForFloatAreaChange;
  }

  if (mShapeOutside != aNewData.mShapeOutside ||
      mShapeMargin != aNewData.mShapeMargin ||
      mShapeImageThreshold != aNewData.mShapeImageThreshold) {
    if (aNewData.mFloat != StyleFloat::None) {
      // If we are floating, and our shape-outside, shape-margin, or
      // shape-image-threshold are changed, our descendants are not impacted,
      // but our ancestor and siblings are.
      hint |= nsChangeHint_ReflowHintsForFloatAreaChange;
    } else {
      // shape-outside or shape-margin or shape-image-threshold changed,
      // but we don't need to reflow because we're not floating.
      hint |= nsChangeHint_NeutralChange;
    }
  }

  if (mLineClamp != aNewData.mLineClamp) {
    hint |= NS_STYLE_HINT_REFLOW;
  }

  if (mVerticalAlign != aNewData.mVerticalAlign) {
    // XXX Can this just be AllReflowHints + RepaintFrame, and be included in
    // the block below?
    hint |= NS_STYLE_HINT_REFLOW;
  }

  // XXX the following is conservative, for now: changing float breaking
  // shouldn't necessarily require a repaint, reflow should suffice.
  //
  // FIXME(emilio): We definitely change the frame tree in nsCSSFrameConstructor
  // based on break-before / break-after... Shouldn't that reframe?
  if (mBreakType != aNewData.mBreakType ||
      mBreakInside != aNewData.mBreakInside ||
      mBreakBefore != aNewData.mBreakBefore ||
      mBreakAfter != aNewData.mBreakAfter ||
      mAppearance != aNewData.mAppearance || mOrient != aNewData.mOrient ||
      mOverflowClipBoxBlock != aNewData.mOverflowClipBoxBlock ||
      mOverflowClipBoxInline != aNewData.mOverflowClipBoxInline) {
    hint |= nsChangeHint_AllReflowHints | nsChangeHint_RepaintFrame;
  }

  if (mIsolation != aNewData.mIsolation) {
    hint |= nsChangeHint_RepaintFrame;
  }

  /* If we've added or removed the transform property, we need to reconstruct
   * the frame to add or remove the view object, and also to handle abs-pos and
   * fixed-pos containers.
   */
  if (HasTransformStyle() != aNewData.HasTransformStyle()) {
    hint |= nsChangeHint_ComprehensiveAddOrRemoveTransform;
  } else {
    /* Otherwise, if we've kept the property lying around and we already had a
     * transform, we need to see whether or not we've changed the transform.
     * If so, we need to recompute its overflow rect (which probably changed
     * if the transform changed) and to redraw within the bounds of that new
     * overflow rect.
     *
     * If the property isn't present in either style struct, we still do the
     * comparisons but turn all the resulting change hints into
     * nsChangeHint_NeutralChange.
     */
    nsChangeHint transformHint = nsChangeHint(0);

    transformHint |= CompareTransformValues(mTransform, aNewData.mTransform);
    transformHint |= CompareTransformValues(mRotate, aNewData.mRotate);
    transformHint |= CompareTransformValues(mTranslate, aNewData.mTranslate);
    transformHint |= CompareTransformValues(mScale, aNewData.mScale);
    transformHint |= CompareMotionValues(*this, aNewData);

    if (mTransformOrigin != aNewData.mTransformOrigin) {
      transformHint |= nsChangeHint_UpdateTransformLayer |
                       nsChangeHint_UpdatePostTransformOverflow;
    }

    if (mPerspectiveOrigin != aNewData.mPerspectiveOrigin ||
        mTransformStyle != aNewData.mTransformStyle ||
        mTransformBox != aNewData.mTransformBox) {
      transformHint |= nsChangeHint_UpdateOverflow | nsChangeHint_RepaintFrame;
    }

    if (mBackfaceVisibility != aNewData.mBackfaceVisibility) {
      transformHint |= nsChangeHint_RepaintFrame;
    }

    if (transformHint) {
      if (HasTransformStyle()) {
        hint |= transformHint;
      } else {
        hint |= nsChangeHint_NeutralChange;
      }
    }
  }

  if (HasPerspectiveStyle() != aNewData.HasPerspectiveStyle()) {
    // A change from/to being a containing block for position:fixed.
    hint |= nsChangeHint_UpdateContainingBlock | nsChangeHint_UpdateOverflow |
            nsChangeHint_RepaintFrame;
  } else if (mChildPerspective != aNewData.mChildPerspective) {
    hint |= nsChangeHint_UpdateOverflow | nsChangeHint_RepaintFrame;
  }

  // Note that the HasTransformStyle() != aNewData.HasTransformStyle()
  // test above handles relevant changes in the StyleWillChangeBit_TRANSFORM
  // bit, which in turn handles frame reconstruction for changes in the
  // containing block of fixed-positioned elements.
  auto willChangeBitsChanged = mWillChange.bits ^ aNewData.mWillChange.bits;

  if (willChangeBitsChanged &
      (StyleWillChangeBits::STACKING_CONTEXT | StyleWillChangeBits::SCROLL |
       StyleWillChangeBits::OPACITY)) {
    hint |= nsChangeHint_RepaintFrame;
  }

  if (willChangeBitsChanged &
      (StyleWillChangeBits::FIXPOS_CB | StyleWillChangeBits::ABSPOS_CB)) {
    hint |= nsChangeHint_UpdateContainingBlock;
  }

  // If touch-action is changed, we need to regenerate the event regions on
  // the layers and send it over to the compositor for APZ to handle.
  if (mTouchAction != aNewData.mTouchAction) {
    hint |= nsChangeHint_RepaintFrame;
  }

  // If overscroll-behavior has changed, the changes are picked up
  // during a repaint.
  if (mOverscrollBehaviorX != aNewData.mOverscrollBehaviorX ||
      mOverscrollBehaviorY != aNewData.mOverscrollBehaviorY) {
    hint |= nsChangeHint_SchedulePaint;
  }

  if (mOriginalDisplay != aNewData.mOriginalDisplay) {
    // Our hypothetical box position may have changed.
    //
    // Note that it doesn't matter if we look at the old or the new struct,
    // since a change on whether we need a hypothetical position would trigger
    // reflow anyway.
    if (IsAbsolutelyPositionedStyle() &&
        aOldPosition.NeedsHypotheticalPositionIfAbsPos()) {
      hint |=
          nsChangeHint_NeedReflow | nsChangeHint_ReflowChangesSizeOrPosition;
    } else {
      hint |= nsChangeHint_NeutralChange;
    }
  }

  // Note:  Our current behavior for handling changes to the
  // transition-duration, transition-delay, and transition-timing-function
  // properties is to do nothing.  In other words, the transition
  // property that matters is what it is when the transition begins, and
  // we don't stop a transition later because the transition property
  // changed.
  // We do handle changes to transition-property, but we don't need to
  // bother with anything here, since the transition manager is notified
  // of any ComputedStyle change anyway.

  // Note: Likewise, for animation-*, the animation manager gets
  // notified about every new ComputedStyle constructed, and it uses
  // that opportunity to handle dynamic changes appropriately.

  // But we still need to return nsChangeHint_NeutralChange for these
  // properties, since some data did change in the style struct.

  if (!hint && (mTransitions != aNewData.mTransitions ||
                mTransitionTimingFunctionCount !=
                    aNewData.mTransitionTimingFunctionCount ||
                mTransitionDurationCount != aNewData.mTransitionDurationCount ||
                mTransitionDelayCount != aNewData.mTransitionDelayCount ||
                mTransitionPropertyCount != aNewData.mTransitionPropertyCount ||
                mAnimations != aNewData.mAnimations ||
                mAnimationTimingFunctionCount !=
                    aNewData.mAnimationTimingFunctionCount ||
                mAnimationDurationCount != aNewData.mAnimationDurationCount ||
                mAnimationDelayCount != aNewData.mAnimationDelayCount ||
                mAnimationNameCount != aNewData.mAnimationNameCount ||
                mAnimationDirectionCount != aNewData.mAnimationDirectionCount ||
                mAnimationFillModeCount != aNewData.mAnimationFillModeCount ||
                mAnimationPlayStateCount != aNewData.mAnimationPlayStateCount ||
                mAnimationIterationCountCount !=
                    aNewData.mAnimationIterationCountCount ||
                mWillChange != aNewData.mWillChange ||
                mOverflowAnchor != aNewData.mOverflowAnchor)) {
    hint |= nsChangeHint_NeutralChange;
  }

  return hint;
}

// --------------------
// nsStyleVisibility
//

nsStyleVisibility::nsStyleVisibility(const Document& aDocument)
    : mImageOrientation(
          StaticPrefs::layout_css_image_orientation_initial_from_image()
              ? StyleImageOrientation::FromImage
              : StyleImageOrientation::None),
      mDirection(aDocument.GetBidiOptions() == IBMBIDI_TEXTDIRECTION_RTL
                     ? StyleDirection::Rtl
                     : StyleDirection::Ltr),
      mVisible(StyleVisibility::Visible),
      mImageRendering(StyleImageRendering::Auto),
      mWritingMode(NS_STYLE_WRITING_MODE_HORIZONTAL_TB),
      mTextOrientation(StyleTextOrientation::Mixed),
      mColorAdjust(StyleColorAdjust::Economy) {
  MOZ_COUNT_CTOR(nsStyleVisibility);
}

nsStyleVisibility::nsStyleVisibility(const nsStyleVisibility& aSource)
    : mImageOrientation(aSource.mImageOrientation),
      mDirection(aSource.mDirection),
      mVisible(aSource.mVisible),
      mImageRendering(aSource.mImageRendering),
      mWritingMode(aSource.mWritingMode),
      mTextOrientation(aSource.mTextOrientation),
      mColorAdjust(aSource.mColorAdjust) {
  MOZ_COUNT_CTOR(nsStyleVisibility);
}

nsChangeHint nsStyleVisibility::CalcDifference(
    const nsStyleVisibility& aNewData) const {
  nsChangeHint hint = nsChangeHint(0);

  if (mDirection != aNewData.mDirection ||
      mWritingMode != aNewData.mWritingMode) {
    // It's important that a change in mWritingMode results in frame
    // reconstruction, because it may affect intrinsic size (see
    // nsSubDocumentFrame::GetIntrinsicISize/BSize).
    // Also, the used writing-mode value is now a field on nsIFrame and some
    // classes (e.g. table rows/cells) copy their value from an ancestor.
    hint |= nsChangeHint_ReconstructFrame;
  } else {
    if ((mImageOrientation != aNewData.mImageOrientation)) {
      hint |= nsChangeHint_AllReflowHints | nsChangeHint_RepaintFrame;
    }
    if (mVisible != aNewData.mVisible) {
      if (mVisible == StyleVisibility::Visible ||
          aNewData.mVisible == StyleVisibility::Visible) {
        hint |= nsChangeHint_VisibilityChange;
      }
      if (StyleVisibility::Collapse == mVisible ||
          StyleVisibility::Collapse == aNewData.mVisible) {
        hint |= NS_STYLE_HINT_REFLOW;
      } else {
        hint |= NS_STYLE_HINT_VISUAL;
      }
    }
    if (mTextOrientation != aNewData.mTextOrientation) {
      hint |= NS_STYLE_HINT_REFLOW;
    }
    if (mImageRendering != aNewData.mImageRendering) {
      hint |= nsChangeHint_RepaintFrame;
    }
    if (mColorAdjust != aNewData.mColorAdjust) {
      // color-adjust only affects media where dynamic changes can't happen.
      hint |= nsChangeHint_NeutralChange;
    }
  }
  return hint;
}

//-----------------------
// nsStyleContent
//

nsStyleContent::nsStyleContent(const Document& aDocument)
    : mContent(StyleContent::Normal()) {
  MOZ_COUNT_CTOR(nsStyleContent);
}

nsStyleContent::~nsStyleContent() { MOZ_COUNT_DTOR(nsStyleContent); }

nsStyleContent::nsStyleContent(const nsStyleContent& aSource)
    : mContent(aSource.mContent),
      mCounterIncrement(aSource.mCounterIncrement),
      mCounterReset(aSource.mCounterReset),
      mCounterSet(aSource.mCounterSet) {
  MOZ_COUNT_CTOR(nsStyleContent);
}

nsChangeHint nsStyleContent::CalcDifference(
    const nsStyleContent& aNewData) const {
  // Unfortunately we need to reframe even if the content lengths are the same;
  // a simple reflow will not pick up different text or different image URLs,
  // since we set all that up in the CSSFrameConstructor
  if (mContent != aNewData.mContent ||
      mCounterIncrement != aNewData.mCounterIncrement ||
      mCounterReset != aNewData.mCounterReset ||
      mCounterSet != aNewData.mCounterSet) {
    return nsChangeHint_ReconstructFrame;
  }

  return nsChangeHint(0);
}

void nsStyleContent::TriggerImageLoads(Document& aDoc,
                                       const nsStyleContent* aOld) {
  if (!mContent.IsItems()) {
    return;
  }

  Span<const StyleContentItem> oldItems;
  if (aOld && aOld->mContent.IsItems()) {
    oldItems = aOld->mContent.AsItems().AsSpan();
  }

  auto items = mContent.AsItems().AsSpan();

  for (size_t i = 0; i < items.Length(); ++i) {
    auto& item = items[i];
    if (!item.IsUrl()) {
      continue;
    }
    auto& url = item.AsUrl();
    if (url.IsImageResolved()) {
      continue;
    }
    auto* oldUrl = i < oldItems.Length() && oldItems[i].IsUrl()
                       ? &oldItems[i].AsUrl()
                       : nullptr;
    const_cast<StyleComputedImageUrl&>(url).ResolveImage(aDoc, oldUrl);
  }
}

// --------------------
// nsStyleTextReset
//

nsStyleTextReset::nsStyleTextReset(const Document& aDocument)
    : mTextOverflow(),
      mTextDecorationLine(StyleTextDecorationLine::NONE),
      mTextDecorationStyle(NS_STYLE_TEXT_DECORATION_STYLE_SOLID),
      mUnicodeBidi(NS_STYLE_UNICODE_BIDI_NORMAL),
      mInitialLetterSink(0),
      mInitialLetterSize(0.0f),
      mTextDecorationColor(StyleColor::CurrentColor()),
      mTextDecorationThickness(StyleTextDecorationLength::Auto()) {
  MOZ_COUNT_CTOR(nsStyleTextReset);
}

nsStyleTextReset::nsStyleTextReset(const nsStyleTextReset& aSource)
    : mTextOverflow(aSource.mTextOverflow),
      mTextDecorationLine(aSource.mTextDecorationLine),
      mTextDecorationStyle(aSource.mTextDecorationStyle),
      mUnicodeBidi(aSource.mUnicodeBidi),
      mInitialLetterSink(aSource.mInitialLetterSink),
      mInitialLetterSize(aSource.mInitialLetterSize),
      mTextDecorationColor(aSource.mTextDecorationColor),
      mTextDecorationThickness(aSource.mTextDecorationThickness) {
  MOZ_COUNT_CTOR(nsStyleTextReset);
}

nsStyleTextReset::~nsStyleTextReset() { MOZ_COUNT_DTOR(nsStyleTextReset); }

nsChangeHint nsStyleTextReset::CalcDifference(
    const nsStyleTextReset& aNewData) const {
  if (mUnicodeBidi != aNewData.mUnicodeBidi ||
      mInitialLetterSink != aNewData.mInitialLetterSink ||
      mInitialLetterSize != aNewData.mInitialLetterSize) {
    return NS_STYLE_HINT_REFLOW;
  }

  if (mTextDecorationLine != aNewData.mTextDecorationLine ||
      mTextDecorationStyle != aNewData.mTextDecorationStyle ||
      mTextDecorationThickness != aNewData.mTextDecorationThickness) {
    // Changes to our text-decoration line can impact our overflow area &
    // also our descendants' overflow areas (particularly for text-frame
    // descendants).  So, we update those areas & trigger a repaint.
    return nsChangeHint_RepaintFrame | nsChangeHint_UpdateSubtreeOverflow |
           nsChangeHint_SchedulePaint;
  }

  // Repaint for decoration color changes
  if (mTextDecorationColor != aNewData.mTextDecorationColor) {
    return nsChangeHint_RepaintFrame;
  }

  if (mTextOverflow != aNewData.mTextOverflow) {
    return nsChangeHint_RepaintFrame;
  }

  return nsChangeHint(0);
}

// --------------------
// nsStyleText
//

static StyleRGBA DefaultColor(const Document& aDocument) {
  return StyleRGBA::FromColor(
      PreferenceSheet::PrefsFor(aDocument).mDefaultColor);
}

nsStyleText::nsStyleText(const Document& aDocument)
    : mColor(DefaultColor(aDocument)),
      mTextTransform(StyleTextTransform::None()),
      mTextAlign(StyleTextAlign::Start),
      mTextAlignLast(StyleTextAlignLast::Auto),
      mTextJustify(StyleTextJustify::Auto),
      mWhiteSpace(StyleWhiteSpace::Normal),
      mHyphens(StyleHyphens::Manual),
      mRubyAlign(StyleRubyAlign::SpaceAround),
      mRubyPosition(StyleRubyPosition::Over),
      mTextSizeAdjust(StyleTextSizeAdjust::Auto),
      mTextCombineUpright(NS_STYLE_TEXT_COMBINE_UPRIGHT_NONE),
      mControlCharacterVisibility(
          nsLayoutUtils::ControlCharVisibilityDefault()),
      mTextRendering(StyleTextRendering::Auto),
      mTextEmphasisColor(StyleColor::CurrentColor()),
      mWebkitTextFillColor(StyleColor::CurrentColor()),
      mWebkitTextStrokeColor(StyleColor::CurrentColor()),
      mMozTabSize(
          StyleNonNegativeLengthOrNumber::Number(NS_STYLE_TABSIZE_INITIAL)),
      mWordSpacing(LengthPercentage::Zero()),
      mLetterSpacing({0.}),
      mLineHeight(StyleLineHeight::Normal()),
      mTextIndent(LengthPercentage::Zero()),
      mTextUnderlineOffset(LengthPercentageOrAuto::Auto()),
      mTextDecorationSkipInk(StyleTextDecorationSkipInk::Auto),
      mTextUnderlinePosition(StyleTextUnderlinePosition::AUTO),
      mWebkitTextStrokeWidth(0),
      mTextEmphasisStyle(StyleTextEmphasisStyle::None()) {
  MOZ_COUNT_CTOR(nsStyleText);
  RefPtr<nsAtom> language = aDocument.GetContentLanguageAsAtomForStyle();
  mTextEmphasisPosition =
      language && nsStyleUtil::MatchesLanguagePrefix(language, u"zh")
          ? NS_STYLE_TEXT_EMPHASIS_POSITION_DEFAULT_ZH
          : NS_STYLE_TEXT_EMPHASIS_POSITION_DEFAULT;
}

nsStyleText::nsStyleText(const nsStyleText& aSource)
    : mColor(aSource.mColor),
      mTextTransform(aSource.mTextTransform),
      mTextAlign(aSource.mTextAlign),
      mTextAlignLast(aSource.mTextAlignLast),
      mTextJustify(aSource.mTextJustify),
      mWhiteSpace(aSource.mWhiteSpace),
      mLineBreak(aSource.mLineBreak),
      mWordBreak(aSource.mWordBreak),
      mOverflowWrap(aSource.mOverflowWrap),
      mHyphens(aSource.mHyphens),
      mRubyAlign(aSource.mRubyAlign),
      mRubyPosition(aSource.mRubyPosition),
      mTextSizeAdjust(aSource.mTextSizeAdjust),
      mTextCombineUpright(aSource.mTextCombineUpright),
      mControlCharacterVisibility(aSource.mControlCharacterVisibility),
      mTextEmphasisPosition(aSource.mTextEmphasisPosition),
      mTextRendering(aSource.mTextRendering),
      mTextEmphasisColor(aSource.mTextEmphasisColor),
      mWebkitTextFillColor(aSource.mWebkitTextFillColor),
      mWebkitTextStrokeColor(aSource.mWebkitTextStrokeColor),
      mMozTabSize(aSource.mMozTabSize),
      mWordSpacing(aSource.mWordSpacing),
      mLetterSpacing(aSource.mLetterSpacing),
      mLineHeight(aSource.mLineHeight),
      mTextIndent(aSource.mTextIndent),
      mTextUnderlineOffset(aSource.mTextUnderlineOffset),
      mTextDecorationSkipInk(aSource.mTextDecorationSkipInk),
      mTextUnderlinePosition(aSource.mTextUnderlinePosition),
      mWebkitTextStrokeWidth(aSource.mWebkitTextStrokeWidth),
      mTextShadow(aSource.mTextShadow),
      mTextEmphasisStyle(aSource.mTextEmphasisStyle) {
  MOZ_COUNT_CTOR(nsStyleText);
}

nsStyleText::~nsStyleText() { MOZ_COUNT_DTOR(nsStyleText); }

nsChangeHint nsStyleText::CalcDifference(const nsStyleText& aNewData) const {
  if (WhiteSpaceOrNewlineIsSignificant() !=
      aNewData.WhiteSpaceOrNewlineIsSignificant()) {
    // This may require construction of suppressed text frames
    return nsChangeHint_ReconstructFrame;
  }

  if (mTextCombineUpright != aNewData.mTextCombineUpright ||
      mControlCharacterVisibility != aNewData.mControlCharacterVisibility) {
    return nsChangeHint_ReconstructFrame;
  }

  if ((mTextAlign != aNewData.mTextAlign) ||
      (mTextAlignLast != aNewData.mTextAlignLast) ||
      (mTextTransform != aNewData.mTextTransform) ||
      (mWhiteSpace != aNewData.mWhiteSpace) ||
      (mLineBreak != aNewData.mLineBreak) ||
      (mWordBreak != aNewData.mWordBreak) ||
      (mOverflowWrap != aNewData.mOverflowWrap) ||
      (mHyphens != aNewData.mHyphens) || (mRubyAlign != aNewData.mRubyAlign) ||
      (mRubyPosition != aNewData.mRubyPosition) ||
      (mTextSizeAdjust != aNewData.mTextSizeAdjust) ||
      (mLetterSpacing != aNewData.mLetterSpacing) ||
      (mLineHeight != aNewData.mLineHeight) ||
      (mTextIndent != aNewData.mTextIndent) ||
      (mTextJustify != aNewData.mTextJustify) ||
      (mWordSpacing != aNewData.mWordSpacing) ||
      (mMozTabSize != aNewData.mMozTabSize)) {
    return NS_STYLE_HINT_REFLOW;
  }

  if (HasEffectiveTextEmphasis() != aNewData.HasEffectiveTextEmphasis() ||
      (HasEffectiveTextEmphasis() &&
       mTextEmphasisPosition != aNewData.mTextEmphasisPosition)) {
    // Text emphasis position change could affect line height calculation.
    return nsChangeHint_AllReflowHints | nsChangeHint_RepaintFrame;
  }

  nsChangeHint hint = nsChangeHint(0);

  // text-rendering changes require a reflow since they change SVG
  // frames' rects.
  if (mTextRendering != aNewData.mTextRendering) {
    hint |= nsChangeHint_NeedReflow |
            nsChangeHint_NeedDirtyReflow |  // XXX remove me: bug 876085
            nsChangeHint_RepaintFrame;
  }

  if (mTextShadow != aNewData.mTextShadow ||
      mTextEmphasisStyle != aNewData.mTextEmphasisStyle ||
      mWebkitTextStrokeWidth != aNewData.mWebkitTextStrokeWidth ||
      mTextUnderlineOffset != aNewData.mTextUnderlineOffset ||
      mTextDecorationSkipInk != aNewData.mTextDecorationSkipInk ||
      mTextUnderlinePosition != aNewData.mTextUnderlinePosition) {
    hint |= nsChangeHint_UpdateSubtreeOverflow | nsChangeHint_SchedulePaint |
            nsChangeHint_RepaintFrame;

    // We don't add any other hints below.
    return hint;
  }

  if (mColor != aNewData.mColor) {
    hint |= nsChangeHint_RepaintFrame;
  }

  if (mTextEmphasisColor != aNewData.mTextEmphasisColor ||
      mWebkitTextFillColor != aNewData.mWebkitTextFillColor ||
      mWebkitTextStrokeColor != aNewData.mWebkitTextStrokeColor) {
    hint |= nsChangeHint_SchedulePaint | nsChangeHint_RepaintFrame;
  }

  if (hint) {
    return hint;
  }

  if (mTextEmphasisPosition != aNewData.mTextEmphasisPosition) {
    return nsChangeHint_NeutralChange;
  }

  return nsChangeHint(0);
}

LogicalSide nsStyleText::TextEmphasisSide(WritingMode aWM) const {
  MOZ_ASSERT(
      (!(mTextEmphasisPosition & NS_STYLE_TEXT_EMPHASIS_POSITION_LEFT) !=
       !(mTextEmphasisPosition & NS_STYLE_TEXT_EMPHASIS_POSITION_RIGHT)) &&
      (!(mTextEmphasisPosition & NS_STYLE_TEXT_EMPHASIS_POSITION_OVER) !=
       !(mTextEmphasisPosition & NS_STYLE_TEXT_EMPHASIS_POSITION_UNDER)));
  mozilla::Side side =
      aWM.IsVertical()
          ? (mTextEmphasisPosition & NS_STYLE_TEXT_EMPHASIS_POSITION_LEFT
                 ? eSideLeft
                 : eSideRight)
          : (mTextEmphasisPosition & NS_STYLE_TEXT_EMPHASIS_POSITION_OVER
                 ? eSideTop
                 : eSideBottom);
  LogicalSide result = aWM.LogicalSideForPhysicalSide(side);
  MOZ_ASSERT(IsBlock(result));
  return result;
}

//-----------------------
// nsStyleUI
//

nsStyleUI::nsStyleUI(const Document& aDocument)
    : mUserInput(StyleUserInput::Auto),
      mUserModify(StyleUserModify::ReadOnly),
      mUserFocus(StyleUserFocus::None),
      mPointerEvents(StylePointerEvents::Auto),
      mCursor{{}, StyleCursorKind::Auto},
      mCaretColor(StyleColorOrAuto::Auto()),
      mScrollbarColor(StyleScrollbarColor::Auto()) {
  MOZ_COUNT_CTOR(nsStyleUI);
}

nsStyleUI::nsStyleUI(const nsStyleUI& aSource)
    : mUserInput(aSource.mUserInput),
      mUserModify(aSource.mUserModify),
      mUserFocus(aSource.mUserFocus),
      mPointerEvents(aSource.mPointerEvents),
      mCursor(aSource.mCursor),
      mCaretColor(aSource.mCaretColor),
      mScrollbarColor(aSource.mScrollbarColor) {
  MOZ_COUNT_CTOR(nsStyleUI);
}

nsStyleUI::~nsStyleUI() { MOZ_COUNT_DTOR(nsStyleUI); }

void nsStyleUI::TriggerImageLoads(Document& aDocument,
                                  const nsStyleUI* aOldStyle) {
  MOZ_ASSERT(NS_IsMainThread());

  auto cursorImages = mCursor.images.AsSpan();
  auto oldCursorImages = aOldStyle ? aOldStyle->mCursor.images.AsSpan()
                                   : Span<const StyleCursorImage>();
  for (size_t i = 0; i < cursorImages.Length(); ++i) {
    auto& cursor = cursorImages[i];

    if (!cursor.url.IsImageResolved()) {
      const auto* oldCursor =
          oldCursorImages.Length() > i ? &oldCursorImages[i] : nullptr;
      const_cast<StyleComputedImageUrl&>(cursor.url)
          .ResolveImage(aDocument, oldCursor ? &oldCursor->url : nullptr);
    }
  }
}

nsChangeHint nsStyleUI::CalcDifference(const nsStyleUI& aNewData) const {
  nsChangeHint hint = nsChangeHint(0);
  if (mCursor != aNewData.mCursor) {
    hint |= nsChangeHint_UpdateCursor;
  }

  if (mPointerEvents != aNewData.mPointerEvents) {
    // SVGGeometryFrame's mRect depends on stroke _and_ on the value
    // of pointer-events. See SVGGeometryFrame::ReflowSVG's use of
    // GetHitTestFlags. (Only a reflow, no visual change.)
    hint |= nsChangeHint_NeedReflow |
            nsChangeHint_NeedDirtyReflow |  // XXX remove me: bug 876085
            nsChangeHint_SchedulePaint;     // pointer-events changes can change
                                            // event regions overrides on layers
                                            // and so needs a repaint.
  }

  if (mUserModify != aNewData.mUserModify) {
    hint |= NS_STYLE_HINT_VISUAL;
  }

  if (mUserInput != aNewData.mUserInput) {
    if (StyleUserInput::None == mUserInput ||
        StyleUserInput::None == aNewData.mUserInput) {
      hint |= nsChangeHint_ReconstructFrame;
    } else {
      hint |= nsChangeHint_NeutralChange;
    }
  }

  if (mUserFocus != aNewData.mUserFocus) {
    hint |= nsChangeHint_NeutralChange;
  }

  if (mCaretColor != aNewData.mCaretColor ||
      mScrollbarColor != aNewData.mScrollbarColor) {
    hint |= nsChangeHint_RepaintFrame;
  }

  return hint;
}

//-----------------------
// nsStyleUIReset
//

nsStyleUIReset::nsStyleUIReset(const Document& aDocument)
    : mUserSelect(StyleUserSelect::Auto),
      mScrollbarWidth(StyleScrollbarWidth::Auto),
      mMozForceBrokenImageIcon(0),
      mIMEMode(StyleImeMode::Auto),
      mWindowDragging(StyleWindowDragging::Default),
      mWindowShadow(StyleWindowShadow::Default),
      mWindowOpacity(1.0),
      mWindowTransformOrigin{LengthPercentage::FromPercentage(0.5),
                             LengthPercentage::FromPercentage(0.5),
                             {0.}} {
  MOZ_COUNT_CTOR(nsStyleUIReset);
}

nsStyleUIReset::nsStyleUIReset(const nsStyleUIReset& aSource)
    : mUserSelect(aSource.mUserSelect),
      mScrollbarWidth(aSource.mScrollbarWidth),
      mMozForceBrokenImageIcon(aSource.mMozForceBrokenImageIcon),
      mIMEMode(aSource.mIMEMode),
      mWindowDragging(aSource.mWindowDragging),
      mWindowShadow(aSource.mWindowShadow),
      mWindowOpacity(aSource.mWindowOpacity),
      mMozWindowTransform(aSource.mMozWindowTransform),
      mWindowTransformOrigin(aSource.mWindowTransformOrigin) {
  MOZ_COUNT_CTOR(nsStyleUIReset);
}

nsStyleUIReset::~nsStyleUIReset() { MOZ_COUNT_DTOR(nsStyleUIReset); }

nsChangeHint nsStyleUIReset::CalcDifference(
    const nsStyleUIReset& aNewData) const {
  nsChangeHint hint = nsChangeHint(0);

  if (mMozForceBrokenImageIcon != aNewData.mMozForceBrokenImageIcon) {
    hint |= nsChangeHint_ReconstructFrame;
  }
  if (mScrollbarWidth != aNewData.mScrollbarWidth) {
    // For scrollbar-width change, we need some special handling similar
    // to overflow properties. Specifically, we may need to reconstruct
    // the scrollbar or force reflow of the viewport scrollbar.
    hint |= nsChangeHint_ScrollbarChange;
  }
  if (mWindowShadow != aNewData.mWindowShadow) {
    // We really need just an nsChangeHint_SyncFrameView, except
    // on an ancestor of the frame, so we get that by doing a
    // reflow.
    hint |= NS_STYLE_HINT_REFLOW;
  }
  if (mUserSelect != aNewData.mUserSelect) {
    hint |= NS_STYLE_HINT_VISUAL;
  }

  if (mWindowDragging != aNewData.mWindowDragging) {
    hint |= nsChangeHint_SchedulePaint;
  }

  if (!hint && (mIMEMode != aNewData.mIMEMode ||
                mWindowOpacity != aNewData.mWindowOpacity ||
                mMozWindowTransform != aNewData.mMozWindowTransform)) {
    hint |= nsChangeHint_NeutralChange;
  }

  return hint;
}

//-----------------------
// nsStyleEffects
//

nsStyleEffects::nsStyleEffects(const Document&)
    : mClip(StyleClipRectOrAuto::Auto()),
      mOpacity(1.0f),
      mMixBlendMode(StyleBlend::Normal) {
  MOZ_COUNT_CTOR(nsStyleEffects);
}

nsStyleEffects::nsStyleEffects(const nsStyleEffects& aSource)
    : mFilters(aSource.mFilters),
      mBoxShadow(aSource.mBoxShadow),
      mBackdropFilters(aSource.mBackdropFilters),
      mClip(aSource.mClip),
      mOpacity(aSource.mOpacity),
      mMixBlendMode(aSource.mMixBlendMode) {
  MOZ_COUNT_CTOR(nsStyleEffects);
}

nsStyleEffects::~nsStyleEffects() { MOZ_COUNT_DTOR(nsStyleEffects); }

static bool AnyAutonessChanged(const StyleClipRectOrAuto& aOld,
                               const StyleClipRectOrAuto& aNew) {
  if (aOld.IsAuto() != aNew.IsAuto()) {
    return true;
  }
  if (aOld.IsAuto()) {
    return false;
  }
  auto& oldRect = aOld.AsRect();
  auto& newRect = aNew.AsRect();
  return oldRect.top.IsAuto() != newRect.top.IsAuto() ||
         oldRect.right.IsAuto() != newRect.right.IsAuto() ||
         oldRect.bottom.IsAuto() != newRect.bottom.IsAuto() ||
         oldRect.left.IsAuto() != newRect.left.IsAuto();
}

nsChangeHint nsStyleEffects::CalcDifference(
    const nsStyleEffects& aNewData) const {
  nsChangeHint hint = nsChangeHint(0);

  if (mBoxShadow != aNewData.mBoxShadow) {
    // Update overflow regions & trigger DLBI to be sure it's noticed.
    // Also request a repaint, since it's possible that only the color
    // of the shadow is changing (and UpdateOverflow/SchedulePaint won't
    // repaint for that, since they won't know what needs invalidating.)
    hint |= nsChangeHint_UpdateOverflow | nsChangeHint_SchedulePaint |
            nsChangeHint_RepaintFrame;
  }

  if (AnyAutonessChanged(mClip, aNewData.mClip)) {
    hint |= nsChangeHint_AllReflowHints | nsChangeHint_RepaintFrame;
  } else if (mClip != aNewData.mClip) {
    // If the clip has changed, we just need to update overflow areas. DLBI
    // will handle the invalidation.
    hint |= nsChangeHint_UpdateOverflow | nsChangeHint_SchedulePaint;
  }

  if (mOpacity != aNewData.mOpacity) {
    hint |= nsChangeHint_UpdateOpacityLayer;

    // If we're going from the optimized >=0.99 opacity value to 1.0 or back,
    // then repaint the frame because DLBI will not catch the invalidation.
    // Otherwise, just update the opacity layer.
    if ((mOpacity >= 0.99f && mOpacity < 1.0f && aNewData.mOpacity == 1.0f) ||
        (aNewData.mOpacity >= 0.99f && aNewData.mOpacity < 1.0f &&
         mOpacity == 1.0f)) {
      hint |= nsChangeHint_RepaintFrame;
    } else {
      if ((mOpacity == 1.0f) != (aNewData.mOpacity == 1.0f)) {
        hint |= nsChangeHint_UpdateUsesOpacity;
      }
    }
  }

  if (HasFilters() != aNewData.HasFilters()) {
    // A change from/to being a containing block for position:fixed.
    hint |= nsChangeHint_UpdateContainingBlock;
  }

  if (mFilters != aNewData.mFilters) {
    hint |= nsChangeHint_UpdateEffects | nsChangeHint_RepaintFrame |
            nsChangeHint_UpdateOverflow;
  }

  if (mMixBlendMode != aNewData.mMixBlendMode) {
    hint |= nsChangeHint_RepaintFrame;
  }

  if (HasBackdropFilters() != aNewData.HasBackdropFilters()) {
    // A change from/to being a containing block for position:fixed.
    hint |= nsChangeHint_UpdateContainingBlock;
  }

  if (mBackdropFilters != aNewData.mBackdropFilters) {
    hint |= nsChangeHint_UpdateEffects | nsChangeHint_RepaintFrame;
  }

  return hint;
}

static bool TransformOperationHasPercent(const StyleTransformOperation& aOp) {
  switch (aOp.tag) {
    case StyleTransformOperation::Tag::TranslateX:
      return aOp.AsTranslateX().HasPercent();
    case StyleTransformOperation::Tag::TranslateY:
      return aOp.AsTranslateY().HasPercent();
    case StyleTransformOperation::Tag::TranslateZ:
      return false;
    case StyleTransformOperation::Tag::Translate3D: {
      auto& translate = aOp.AsTranslate3D();
      // NOTE(emilio): z translation is a `<length>`, so can't have percentages.
      return translate._0.HasPercent() || translate._1.HasPercent();
    }
    case StyleTransformOperation::Tag::Translate: {
      auto& translate = aOp.AsTranslate();
      return translate._0.HasPercent() || translate._1.HasPercent();
    }
    case StyleTransformOperation::Tag::AccumulateMatrix: {
      auto& accum = aOp.AsAccumulateMatrix();
      return accum.from_list.HasPercent() || accum.to_list.HasPercent();
    }
    case StyleTransformOperation::Tag::InterpolateMatrix: {
      auto& interpolate = aOp.AsInterpolateMatrix();
      return interpolate.from_list.HasPercent() ||
             interpolate.to_list.HasPercent();
    }
    case StyleTransformOperation::Tag::Perspective:
    case StyleTransformOperation::Tag::RotateX:
    case StyleTransformOperation::Tag::RotateY:
    case StyleTransformOperation::Tag::RotateZ:
    case StyleTransformOperation::Tag::Rotate:
    case StyleTransformOperation::Tag::Rotate3D:
    case StyleTransformOperation::Tag::SkewX:
    case StyleTransformOperation::Tag::SkewY:
    case StyleTransformOperation::Tag::Skew:
    case StyleTransformOperation::Tag::ScaleX:
    case StyleTransformOperation::Tag::ScaleY:
    case StyleTransformOperation::Tag::ScaleZ:
    case StyleTransformOperation::Tag::Scale:
    case StyleTransformOperation::Tag::Scale3D:
    case StyleTransformOperation::Tag::Matrix:
    case StyleTransformOperation::Tag::Matrix3D:
      return false;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown transform operation");
      return false;
  }
}

template <>
bool StyleTransform::HasPercent() const {
  for (const auto& op : Operations()) {
    if (TransformOperationHasPercent(op)) {
      return true;
    }
  }
  return false;
}

template <>
void StyleCalcNode::ScaleLengthsBy(float aScale) {
  auto ScaleNode = [aScale](const StyleCalcNode& aNode) {
    // This const_cast could be removed by generating more mut-casts, if
    // needed.
    const_cast<StyleCalcNode&>(aNode).ScaleLengthsBy(aScale);
  };

  switch (tag) {
    case Tag::Leaf: {
      auto& leaf = AsLeaf();
      if (leaf.IsLength()) {
        // This const_cast could be removed by generating more mut-casts, if
        // needed.
        const_cast<Length&>(leaf.AsLength()).ScaleBy(aScale);
      }
      break;
    }
    case Tag::Clamp: {
      auto& clamp = AsClamp();
      ScaleNode(*clamp.min);
      ScaleNode(*clamp.center);
      ScaleNode(*clamp.max);
      break;
    }
    case Tag::MinMax: {
      for (auto& child : AsMinMax()._0.AsSpan()) {
        ScaleNode(child);
      }
      break;
    }
    case Tag::Sum: {
      for (auto& child : AsSum().AsSpan()) {
        ScaleNode(child);
      }
      break;
    }
  }
}

template <>
template <typename ResultT, typename PercentageConverter>
ResultT StyleCalcNode::ResolveInternal(ResultT aPercentageBasis,
                                       PercentageConverter aConverter) const {
  static_assert(std::is_same_v<decltype(aConverter(1.0f)), ResultT>);
  static_assert(std::is_same_v<ResultT, nscoord> ||
                std::is_same_v<ResultT, CSSCoord>);

  switch (tag) {
    case Tag::Leaf: {
      auto& leaf = AsLeaf();
      if (leaf.IsPercentage()) {
        return aConverter(leaf.AsPercentage()._0 * aPercentageBasis);
      }
      if constexpr (std::is_same_v<ResultT, nscoord>) {
        return leaf.AsLength().ToAppUnits();
      } else {
        return leaf.AsLength().ToCSSPixels();
      }
    }
    case Tag::Clamp: {
      auto& clamp = AsClamp();
      auto min = clamp.min->ResolveInternal(aPercentageBasis, aConverter);
      auto center = clamp.center->ResolveInternal(aPercentageBasis, aConverter);
      auto max = clamp.max->ResolveInternal(aPercentageBasis, aConverter);
      return std::max(min, std::min(center, max));
    }
    case Tag::MinMax: {
      auto children = AsMinMax()._0.AsSpan();
      StyleMinMaxOp op = AsMinMax()._1;

      ResultT result =
          children[0].ResolveInternal(aPercentageBasis, aConverter);
      for (auto& child : children.From(1)) {
        ResultT candidate = child.ResolveInternal(aPercentageBasis, aConverter);
        if (op == StyleMinMaxOp::Max) {
          result = std::max(result, candidate);
        } else {
          result = std::min(result, candidate);
        }
      }
      return result;
    }
    case Tag::Sum: {
      ResultT result = 0;
      for (auto& child : AsSum().AsSpan()) {
        result += child.ResolveInternal(aPercentageBasis, aConverter);
      }
      return result;
    }
  }

  MOZ_ASSERT_UNREACHABLE("Unknown calc node");
  return 0;
}

template <>
CSSCoord StyleCalcNode::ResolveToCSSPixels(CSSCoord aBasis) const {
  return ResolveInternal(aBasis, [](CSSCoord aPercent) { return aPercent; });
}

template <>
nscoord StyleCalcNode::Resolve(nscoord aBasis,
                               CoordPercentageRounder aRounder) const {
  return ResolveInternal(aBasis, aRounder);
}
