/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLayoutUtils.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/EffectCompositor.h"
#include "mozilla/EffectSet.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/layers/PAPZ.h"
#include "mozilla/Likely.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/Unused.h"
#include "nsCharTraits.h"
#include "nsDocument.h"
#include "nsFontMetrics.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsFrameList.h"
#include "nsGkAtoms.h"
#include "nsIAtom.h"
#include "nsCaret.h"
#include "nsCSSPseudoElements.h"
#include "nsCSSAnonBoxes.h"
#include "nsCSSColorUtils.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsPlaceholderFrame.h"
#include "nsIScrollableFrame.h"
#include "nsIDOMEvent.h"
#include "nsDisplayList.h"
#include "nsRegion.h"
#include "nsFrameManager.h"
#include "nsBlockFrame.h"
#include "nsBidiPresUtils.h"
#include "imgIContainer.h"
#include "ImageOps.h"
#include "ImageRegion.h"
#include "gfxRect.h"
#include "gfxContext.h"
#include "gfxContext.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsCSSRendering.h"
#include "nsTextFragment.h"
#include "nsThemeConstants.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIWidget.h"
#include "gfxMatrix.h"
#include "gfxPrefs.h"
#include "gfxTypes.h"
#include "nsTArray.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "nsICanvasRenderingContextInternal.h"
#include "gfxPlatform.h"
#include <algorithm>
#include <limits>
#include "mozilla/dom/AnonymousContent.h"
#include "mozilla/dom/HTMLVideoElement.h"
#include "mozilla/dom/HTMLImageElement.h"
#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/DOMStringList.h"
#include "mozilla/dom/KeyframeEffectReadOnly.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "imgIRequest.h"
#include "nsIImageLoadingContent.h"
#include "nsCOMPtr.h"
#include "nsCSSProps.h"
#include "nsListControlFrame.h"
#include "mozilla/dom/Element.h"
#include "nsCanvasFrame.h"
#include "gfxDrawable.h"
#include "gfxEnv.h"
#include "gfxUtils.h"
#include "nsDataHashtable.h"
#include "nsTableWrapperFrame.h"
#include "nsTextFrame.h"
#include "nsFontFaceList.h"
#include "nsFontInflationData.h"
#include "nsSVGUtils.h"
#include "SVGImageContext.h"
#include "SVGTextFrame.h"
#include "nsStyleStructInlines.h"
#include "nsStyleTransformMatrix.h"
#include "nsIFrameInlines.h"
#include "ImageContainer.h"
#include "nsComputedDOMStyle.h"
#include "ActiveLayerTracker.h"
#include "mozilla/gfx/2D.h"
#include "gfx2DGlue.h"
#include "mozilla/LookAndFeel.h"
#include "UnitTransforms.h"
#include "TiledLayerBuffer.h" // For TILEDLAYERBUFFER_TILE_SIZE
#include "ClientLayerManager.h"
#include "nsRefreshDriver.h"
#include "nsIContentViewer.h"
#include "LayersLogging.h"
#include "mozilla/Preferences.h"
#include "nsFrameSelection.h"
#include "FrameLayerBuilder.h"
#include "mozilla/layers/APZCTreeManager.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/Telemetry.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/RuleNodeCacheConditions.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/StyleSetHandle.h"
#include "mozilla/StyleSetHandleInlines.h"
#include "RegionBuilder.h"
#include "SVGSVGElement.h"
#include "DisplayItemClip.h"
#include "mozilla/layers/WebRenderLayerManager.h"

#ifdef MOZ_XUL
#include "nsXULPopupManager.h"
#endif

#include "GeckoProfiler.h"
#include "nsAnimationManager.h"
#include "nsTransitionManager.h"
#include "mozilla/RestyleManager.h"
#include "mozilla/RestyleManagerInlines.h"
#include "LayoutLogging.h"

// Make sure getpid() works.
#ifdef XP_WIN
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::image;
using namespace mozilla::layers;
using namespace mozilla::layout;
using namespace mozilla::gfx;

#define GRID_ENABLED_PREF_NAME "layout.css.grid.enabled"
#define GRID_TEMPLATE_SUBGRID_ENABLED_PREF_NAME "layout.css.grid-template-subgrid-value.enabled"
#define WEBKIT_PREFIXES_ENABLED_PREF_NAME "layout.css.prefixes.webkit"
#define TEXT_ALIGN_UNSAFE_ENABLED_PREF_NAME "layout.css.text-align-unsafe-value.enabled"
#define FLOAT_LOGICAL_VALUES_ENABLED_PREF_NAME "layout.css.float-logical-values.enabled"

// The time in number of frames that we estimate for a refresh driver
// to be quiescent
#define DEFAULT_QUIESCENT_FRAMES 2
// The time (milliseconds) we estimate is needed between the end of an
// idle time and the next Tick.
#define DEFAULT_IDLE_PERIOD_TIME_LIMIT 1.0f

#ifdef DEBUG
// TODO: remove, see bug 598468.
bool nsLayoutUtils::gPreventAssertInCompareTreePosition = false;
#endif // DEBUG

typedef FrameMetrics::ViewID ViewID;
typedef nsStyleTransformMatrix::TransformReferenceBox TransformReferenceBox;

/* static */ uint32_t nsLayoutUtils::sFontSizeInflationEmPerLine;
/* static */ uint32_t nsLayoutUtils::sFontSizeInflationMinTwips;
/* static */ uint32_t nsLayoutUtils::sFontSizeInflationLineThreshold;
/* static */ int32_t  nsLayoutUtils::sFontSizeInflationMappingIntercept;
/* static */ uint32_t nsLayoutUtils::sFontSizeInflationMaxRatio;
/* static */ bool nsLayoutUtils::sFontSizeInflationForceEnabled;
/* static */ bool nsLayoutUtils::sFontSizeInflationDisabledInMasterProcess;
/* static */ uint32_t nsLayoutUtils::sSystemFontScale;
/* static */ uint32_t nsLayoutUtils::sZoomMaxPercent;
/* static */ uint32_t nsLayoutUtils::sZoomMinPercent;
/* static */ bool nsLayoutUtils::sInvalidationDebuggingIsEnabled;
/* static */ bool nsLayoutUtils::sInterruptibleReflowEnabled;
/* static */ bool nsLayoutUtils::sSVGTransformBoxEnabled;
/* static */ bool nsLayoutUtils::sTextCombineUprightDigitsEnabled;
#ifdef MOZ_STYLO
/* static */ bool nsLayoutUtils::sStyloEnabled;
#endif
/* static */ bool nsLayoutUtils::sStyleAttrWithXMLBaseDisabled;
/* static */ uint32_t nsLayoutUtils::sIdlePeriodDeadlineLimit;
/* static */ uint32_t nsLayoutUtils::sQuiescentFramesBeforeIdlePeriod;

static ViewID sScrollIdCounter = FrameMetrics::START_SCROLL_ID;

typedef nsDataHashtable<nsUint64HashKey, nsIContent*> ContentMap;
static ContentMap* sContentMap = nullptr;
static ContentMap& GetContentMap() {
  if (!sContentMap) {
    sContentMap = new ContentMap();
  }
  return *sContentMap;
}

// When the pref "layout.css.grid.enabled" changes, this function is invoked
// to let us update kDisplayKTable, to selectively disable or restore the
// entries for "grid" and "inline-grid" in that table.
static void
GridEnabledPrefChangeCallback(const char* aPrefName, void* aClosure)
{
  MOZ_ASSERT(strncmp(aPrefName, GRID_ENABLED_PREF_NAME,
                     ArrayLength(GRID_ENABLED_PREF_NAME)) == 0,
             "We only registered this callback for a single pref, so it "
             "should only be called for that pref");

  static int32_t sIndexOfGridInDisplayTable;
  static int32_t sIndexOfInlineGridInDisplayTable;
  static bool sAreGridKeywordIndicesInitialized; // initialized to false

  bool isGridEnabled =
    Preferences::GetBool(GRID_ENABLED_PREF_NAME, false);
  if (!sAreGridKeywordIndicesInitialized) {
    // First run: find the position of "grid" and "inline-grid" in
    // kDisplayKTable.
    sIndexOfGridInDisplayTable =
      nsCSSProps::FindIndexOfKeyword(eCSSKeyword_grid,
                                     nsCSSProps::kDisplayKTable);
    MOZ_ASSERT(sIndexOfGridInDisplayTable >= 0,
               "Couldn't find grid in kDisplayKTable");
    sIndexOfInlineGridInDisplayTable =
      nsCSSProps::FindIndexOfKeyword(eCSSKeyword_inline_grid,
                                     nsCSSProps::kDisplayKTable);
    MOZ_ASSERT(sIndexOfInlineGridInDisplayTable >= 0,
               "Couldn't find inline-grid in kDisplayKTable");
    sAreGridKeywordIndicesInitialized = true;
  }

  // OK -- now, stomp on or restore the "grid" entries in kDisplayKTable,
  // depending on whether the grid pref is enabled vs. disabled.
  if (sIndexOfGridInDisplayTable >= 0) {
    nsCSSProps::kDisplayKTable[sIndexOfGridInDisplayTable].mKeyword =
      isGridEnabled ? eCSSKeyword_grid : eCSSKeyword_UNKNOWN;
  }
  if (sIndexOfInlineGridInDisplayTable >= 0) {
    nsCSSProps::kDisplayKTable[sIndexOfInlineGridInDisplayTable].mKeyword =
      isGridEnabled ? eCSSKeyword_inline_grid : eCSSKeyword_UNKNOWN;
  }
}

// When the pref "layout.css.prefixes.webkit" changes, this function is invoked
// to let us update kDisplayKTable, to selectively disable or restore the
// entries for "-webkit-box" and "-webkit-inline-box" in that table.
static void
WebkitPrefixEnabledPrefChangeCallback(const char* aPrefName, void* aClosure)
{
  MOZ_ASSERT(strncmp(aPrefName, WEBKIT_PREFIXES_ENABLED_PREF_NAME,
                     ArrayLength(WEBKIT_PREFIXES_ENABLED_PREF_NAME)) == 0,
             "We only registered this callback for a single pref, so it "
             "should only be called for that pref");

  static int32_t sIndexOfWebkitBoxInDisplayTable;
  static int32_t sIndexOfWebkitInlineBoxInDisplayTable;
  static int32_t sIndexOfWebkitFlexInDisplayTable;
  static int32_t sIndexOfWebkitInlineFlexInDisplayTable;

  static bool sAreKeywordIndicesInitialized; // initialized to false

  bool isWebkitPrefixSupportEnabled =
    Preferences::GetBool(WEBKIT_PREFIXES_ENABLED_PREF_NAME, false);
  if (!sAreKeywordIndicesInitialized) {
    // First run: find the position of the keywords in kDisplayKTable.
    sIndexOfWebkitBoxInDisplayTable =
      nsCSSProps::FindIndexOfKeyword(eCSSKeyword__webkit_box,
                                     nsCSSProps::kDisplayKTable);
    MOZ_ASSERT(sIndexOfWebkitBoxInDisplayTable >= 0,
               "Couldn't find -webkit-box in kDisplayKTable");
    sIndexOfWebkitInlineBoxInDisplayTable =
      nsCSSProps::FindIndexOfKeyword(eCSSKeyword__webkit_inline_box,
                                     nsCSSProps::kDisplayKTable);
    MOZ_ASSERT(sIndexOfWebkitInlineBoxInDisplayTable >= 0,
               "Couldn't find -webkit-inline-box in kDisplayKTable");

    sIndexOfWebkitFlexInDisplayTable =
      nsCSSProps::FindIndexOfKeyword(eCSSKeyword__webkit_flex,
                                     nsCSSProps::kDisplayKTable);
    MOZ_ASSERT(sIndexOfWebkitFlexInDisplayTable >= 0,
               "Couldn't find -webkit-flex in kDisplayKTable");
    sIndexOfWebkitInlineFlexInDisplayTable =
      nsCSSProps::FindIndexOfKeyword(eCSSKeyword__webkit_inline_flex,
                                     nsCSSProps::kDisplayKTable);
    MOZ_ASSERT(sIndexOfWebkitInlineFlexInDisplayTable >= 0,
               "Couldn't find -webkit-inline-flex in kDisplayKTable");
    sAreKeywordIndicesInitialized = true;
  }

  // OK -- now, stomp on or restore the "-webkit-{box|flex}" entries in
  // kDisplayKTable, depending on whether the webkit prefix pref is enabled
  // vs. disabled.
  if (sIndexOfWebkitBoxInDisplayTable >= 0) {
    nsCSSProps::kDisplayKTable[sIndexOfWebkitBoxInDisplayTable].mKeyword =
      isWebkitPrefixSupportEnabled ?
      eCSSKeyword__webkit_box : eCSSKeyword_UNKNOWN;
  }
  if (sIndexOfWebkitInlineBoxInDisplayTable >= 0) {
    nsCSSProps::kDisplayKTable[sIndexOfWebkitInlineBoxInDisplayTable].mKeyword =
      isWebkitPrefixSupportEnabled ?
      eCSSKeyword__webkit_inline_box : eCSSKeyword_UNKNOWN;
  }
  if (sIndexOfWebkitFlexInDisplayTable >= 0) {
    nsCSSProps::kDisplayKTable[sIndexOfWebkitFlexInDisplayTable].mKeyword =
      isWebkitPrefixSupportEnabled ?
      eCSSKeyword__webkit_flex : eCSSKeyword_UNKNOWN;
  }
  if (sIndexOfWebkitInlineFlexInDisplayTable >= 0) {
    nsCSSProps::kDisplayKTable[sIndexOfWebkitInlineFlexInDisplayTable].mKeyword =
      isWebkitPrefixSupportEnabled ?
      eCSSKeyword__webkit_inline_flex : eCSSKeyword_UNKNOWN;
  }
}

// When the pref "layout.css.text-align-unsafe-value.enabled" changes, this
// function is called to let us update kTextAlignKTable & kTextAlignLastKTable,
// to selectively disable or restore the entries for "unsafe" in those tables.
static void
TextAlignUnsafeEnabledPrefChangeCallback(const char* aPrefName, void* aClosure)
{
  NS_ASSERTION(strcmp(aPrefName, TEXT_ALIGN_UNSAFE_ENABLED_PREF_NAME) == 0,
               "Did you misspell " TEXT_ALIGN_UNSAFE_ENABLED_PREF_NAME " ?");

  static bool sIsInitialized;
  static int32_t sIndexOfUnsafeInTextAlignTable;
  static int32_t sIndexOfUnsafeInTextAlignLastTable;
  bool isTextAlignUnsafeEnabled =
    Preferences::GetBool(TEXT_ALIGN_UNSAFE_ENABLED_PREF_NAME, false);

  if (!sIsInitialized) {
    // First run: find the position of "unsafe" in kTextAlignKTable.
    sIndexOfUnsafeInTextAlignTable =
      nsCSSProps::FindIndexOfKeyword(eCSSKeyword_unsafe,
                                     nsCSSProps::kTextAlignKTable);
    // First run: find the position of "unsafe" in kTextAlignLastKTable.
    sIndexOfUnsafeInTextAlignLastTable =
      nsCSSProps::FindIndexOfKeyword(eCSSKeyword_unsafe,
                                     nsCSSProps::kTextAlignLastKTable);
    sIsInitialized = true;
  }

  // OK -- now, stomp on or restore the "unsafe" entry in the keyword tables,
  // depending on whether the pref is enabled vs. disabled.
  MOZ_ASSERT(sIndexOfUnsafeInTextAlignTable >= 0);
  nsCSSProps::kTextAlignKTable[sIndexOfUnsafeInTextAlignTable].mKeyword =
    isTextAlignUnsafeEnabled ? eCSSKeyword_unsafe : eCSSKeyword_UNKNOWN;
  MOZ_ASSERT(sIndexOfUnsafeInTextAlignLastTable >= 0);
  nsCSSProps::kTextAlignLastKTable[sIndexOfUnsafeInTextAlignLastTable].mKeyword =
    isTextAlignUnsafeEnabled ? eCSSKeyword_unsafe : eCSSKeyword_UNKNOWN;
}

// When the pref "layout.css.float-logical-values.enabled" changes, this
// function is called to let us update kFloatKTable & kClearKTable,
// to selectively disable or restore the entries for logical values
// (inline-start and inline-end) in those tables.
static void
FloatLogicalValuesEnabledPrefChangeCallback(const char* aPrefName,
                                            void* aClosure)
{
  NS_ASSERTION(strcmp(aPrefName, FLOAT_LOGICAL_VALUES_ENABLED_PREF_NAME) == 0,
               "Did you misspell " FLOAT_LOGICAL_VALUES_ENABLED_PREF_NAME " ?");

  static bool sIsInitialized;
  static int32_t sIndexOfInlineStartInFloatTable;
  static int32_t sIndexOfInlineEndInFloatTable;
  static int32_t sIndexOfInlineStartInClearTable;
  static int32_t sIndexOfInlineEndInClearTable;
  bool isFloatLogicalValuesEnabled =
    Preferences::GetBool(FLOAT_LOGICAL_VALUES_ENABLED_PREF_NAME, false);

  if (!sIsInitialized) {
    // First run: find the position of "inline-start" in kFloatKTable.
    sIndexOfInlineStartInFloatTable =
      nsCSSProps::FindIndexOfKeyword(eCSSKeyword_inline_start,
                                     nsCSSProps::kFloatKTable);
    // First run: find the position of "inline-end" in kFloatKTable.
    sIndexOfInlineEndInFloatTable =
      nsCSSProps::FindIndexOfKeyword(eCSSKeyword_inline_end,
                                     nsCSSProps::kFloatKTable);
    // First run: find the position of "inline-start" in kClearKTable.
    sIndexOfInlineStartInClearTable =
      nsCSSProps::FindIndexOfKeyword(eCSSKeyword_inline_start,
                                     nsCSSProps::kClearKTable);
    // First run: find the position of "inline-end" in kClearKTable.
    sIndexOfInlineEndInClearTable =
      nsCSSProps::FindIndexOfKeyword(eCSSKeyword_inline_end,
                                     nsCSSProps::kClearKTable);
    sIsInitialized = true;
  }

  // OK -- now, stomp on or restore the logical entries in the keyword tables,
  // depending on whether the pref is enabled vs. disabled.
  MOZ_ASSERT(sIndexOfInlineStartInFloatTable >= 0);
  nsCSSProps::kFloatKTable[sIndexOfInlineStartInFloatTable].mKeyword =
    isFloatLogicalValuesEnabled ? eCSSKeyword_inline_start : eCSSKeyword_UNKNOWN;
  MOZ_ASSERT(sIndexOfInlineEndInFloatTable >= 0);
  nsCSSProps::kFloatKTable[sIndexOfInlineEndInFloatTable].mKeyword =
    isFloatLogicalValuesEnabled ? eCSSKeyword_inline_end : eCSSKeyword_UNKNOWN;
  MOZ_ASSERT(sIndexOfInlineStartInClearTable >= 0);
  nsCSSProps::kClearKTable[sIndexOfInlineStartInClearTable].mKeyword =
    isFloatLogicalValuesEnabled ? eCSSKeyword_inline_start : eCSSKeyword_UNKNOWN;
  MOZ_ASSERT(sIndexOfInlineEndInClearTable >= 0);
  nsCSSProps::kClearKTable[sIndexOfInlineEndInClearTable].mKeyword =
    isFloatLogicalValuesEnabled ? eCSSKeyword_inline_end : eCSSKeyword_UNKNOWN;
}

template<typename TestType>
static bool
HasMatchingAnimations(EffectSet* aEffects, TestType&& aTest)
{
  for (KeyframeEffectReadOnly* effect : *aEffects) {
    if (aTest(*effect)) {
      return true;
    }
  }

  return false;
}

template<typename TestType>
static bool
HasMatchingAnimations(const nsIFrame* aFrame, TestType&& aTest)
{
  EffectSet* effects = EffectSet::GetEffectSet(aFrame);
  if (!effects) {
    return false;
  }

  return HasMatchingAnimations(effects, aTest);
}

bool
nsLayoutUtils::HasCurrentTransitions(const nsIFrame* aFrame)
{
  return HasMatchingAnimations(aFrame,
    [](KeyframeEffectReadOnly& aEffect)
    {
      // Since |aEffect| is current, it must have an associated Animation
      // so we don't need to null-check the result of GetAnimation().
      return aEffect.IsCurrent() && aEffect.GetAnimation()->AsCSSTransition();
    }
  );
}

static bool
MayHaveAnimationOfProperty(EffectSet* effects, nsCSSPropertyID aProperty)
{
  MOZ_ASSERT(effects);

  if (aProperty == eCSSProperty_transform &&
      !effects->MayHaveTransformAnimation()) {
    return false;
  }
  if (aProperty == eCSSProperty_opacity &&
      !effects->MayHaveOpacityAnimation()) {
    return false;
  }

  return true;
}

bool
nsLayoutUtils::HasAnimationOfProperty(EffectSet* aEffectSet,
                                      nsCSSPropertyID aProperty)
{
  if (!aEffectSet || !MayHaveAnimationOfProperty(aEffectSet, aProperty)) {
    return false;
  }

  return HasMatchingAnimations(aEffectSet,
    [&aProperty](KeyframeEffectReadOnly& aEffect)
    {
      return (aEffect.IsInEffect() || aEffect.IsCurrent()) &&
             aEffect.HasAnimationOfProperty(aProperty);
    }
  );
}

bool
nsLayoutUtils::HasAnimationOfProperty(const nsIFrame* aFrame,
                                      nsCSSPropertyID aProperty)
{
  return HasAnimationOfProperty(EffectSet::GetEffectSet(aFrame), aProperty);
}

bool
nsLayoutUtils::HasEffectiveAnimation(const nsIFrame* aFrame,
                                     nsCSSPropertyID aProperty)
{
  EffectSet* effects = EffectSet::GetEffectSet(aFrame);
  if (!effects || !MayHaveAnimationOfProperty(effects, aProperty)) {
    return false;
  }


  return HasMatchingAnimations(effects,
    [&aProperty](KeyframeEffectReadOnly& aEffect)
    {
      return (aEffect.IsInEffect() || aEffect.IsCurrent()) &&
             aEffect.HasEffectiveAnimationOfProperty(aProperty);
    }
  );
}

static float
GetSuitableScale(float aMaxScale, float aMinScale,
                 nscoord aVisibleDimension, nscoord aDisplayDimension)
{
  float displayVisibleRatio = float(aDisplayDimension) /
                              float(aVisibleDimension);
  // We want to rasterize based on the largest scale used during the
  // transform animation, unless that would make us rasterize something
  // larger than the screen.  But we never want to go smaller than the
  // minimum scale over the animation.
  if (FuzzyEqualsMultiplicative(displayVisibleRatio, aMaxScale, .01f)) {
    // Using aMaxScale may make us rasterize something a fraction larger than
    // the screen. However, if aMaxScale happens to be the final scale of a
    // transform animation it is better to use aMaxScale so that for the
    // fraction of a second before we delayerize the composited texture it has
    // a better chance of being pixel aligned and composited without resampling
    // (avoiding visually clunky delayerization).
    return aMaxScale;
  }
  return std::max(std::min(aMaxScale, displayVisibleRatio), aMinScale);
}

static inline void
UpdateMinMaxScale(const nsIFrame* aFrame,
                  const AnimationValue& aValue,
                  gfxSize& aMinScale,
                  gfxSize& aMaxScale)
{
  gfxSize size = aValue.GetScaleValue(aFrame);
  aMaxScale.width = std::max<float>(aMaxScale.width, size.width);
  aMaxScale.height = std::max<float>(aMaxScale.height, size.height);
  aMinScale.width = std::min<float>(aMinScale.width, size.width);
  aMinScale.height = std::min<float>(aMinScale.height, size.height);
}

static void
GetMinAndMaxScaleForAnimationProperty(const nsIFrame* aFrame,
                                      nsTArray<RefPtr<dom::Animation>>&
                                        aAnimations,
                                      gfxSize& aMaxScale,
                                      gfxSize& aMinScale)
{
  for (dom::Animation* anim : aAnimations) {
    // This method is only expected to be passed animations that are running on
    // the compositor and we only pass playing animations to the compositor,
    // which are, by definition, "relevant" animations (animations that are
    // not yet finished or which are filling forwards).
    MOZ_ASSERT(anim->IsRelevant());

    dom::KeyframeEffectReadOnly* effect =
      anim->GetEffect() ? anim->GetEffect()->AsKeyframeEffect() : nullptr;
    MOZ_ASSERT(effect, "A playing animation should have a keyframe effect");
    for (size_t propIdx = effect->Properties().Length(); propIdx-- != 0; ) {
      const AnimationProperty& prop = effect->Properties()[propIdx];
      if (prop.mProperty != eCSSProperty_transform) {
        continue;
      }

      // We need to factor in the scale of the base style if the base style
      // will be used on the compositor.
      AnimationValue baseStyle = effect->BaseStyle(prop.mProperty);
      if (!baseStyle.IsNull()) {
        UpdateMinMaxScale(aFrame, baseStyle, aMinScale, aMaxScale);
      }

      for (const AnimationPropertySegment& segment : prop.mSegments) {
        // In case of add or accumulate composite, StyleAnimationValue does
        // not have a valid value.
        if (segment.HasReplaceableFromValue()) {
          UpdateMinMaxScale(aFrame, segment.mFromValue, aMinScale, aMaxScale);
        }
        if (segment.HasReplaceableToValue()) {
          UpdateMinMaxScale(aFrame, segment.mToValue, aMinScale, aMaxScale);
        }
      }
    }
  }
}

gfxSize
nsLayoutUtils::ComputeSuitableScaleForAnimation(const nsIFrame* aFrame,
                                                const nsSize& aVisibleSize,
                                                const nsSize& aDisplaySize)
{
  gfxSize maxScale(std::numeric_limits<gfxFloat>::min(),
                   std::numeric_limits<gfxFloat>::min());
  gfxSize minScale(std::numeric_limits<gfxFloat>::max(),
                   std::numeric_limits<gfxFloat>::max());

  nsTArray<RefPtr<dom::Animation>> compositorAnimations =
    EffectCompositor::GetAnimationsForCompositor(aFrame,
                                                 eCSSProperty_transform);
  GetMinAndMaxScaleForAnimationProperty(aFrame, compositorAnimations,
                                        maxScale, minScale);

  if (maxScale.width == std::numeric_limits<gfxFloat>::min()) {
    // We didn't encounter a transform
    return gfxSize(1.0, 1.0);
  }

  return gfxSize(GetSuitableScale(maxScale.width, minScale.width,
                                  aVisibleSize.width, aDisplaySize.width),
                 GetSuitableScale(maxScale.height, minScale.height,
                                  aVisibleSize.height, aDisplaySize.height));
}

bool
nsLayoutUtils::AreAsyncAnimationsEnabled()
{
  static bool sAreAsyncAnimationsEnabled;
  static bool sAsyncPrefCached = false;

  if (!sAsyncPrefCached) {
    sAsyncPrefCached = true;
    Preferences::AddBoolVarCache(&sAreAsyncAnimationsEnabled,
                                 "layers.offmainthreadcomposition.async-animations");
  }

  return sAreAsyncAnimationsEnabled &&
    gfxPlatform::OffMainThreadCompositingEnabled();
}

bool
nsLayoutUtils::IsAnimationLoggingEnabled()
{
  static bool sShouldLog;
  static bool sShouldLogPrefCached;

  if (!sShouldLogPrefCached) {
    sShouldLogPrefCached = true;
    Preferences::AddBoolVarCache(&sShouldLog,
                                 "layers.offmainthreadcomposition.log-animations");
  }

  return sShouldLog;
}

bool
nsLayoutUtils::GPUImageScalingEnabled()
{
  static bool sGPUImageScalingEnabled;
  static bool sGPUImageScalingPrefInitialised = false;

  if (!sGPUImageScalingPrefInitialised) {
    sGPUImageScalingPrefInitialised = true;
    sGPUImageScalingEnabled =
      Preferences::GetBool("layout.gpu-image-scaling.enabled", false);
  }

  return sGPUImageScalingEnabled;
}

bool
nsLayoutUtils::AnimatedImageLayersEnabled()
{
  static bool sAnimatedImageLayersEnabled;
  static bool sAnimatedImageLayersPrefCached = false;

  if (!sAnimatedImageLayersPrefCached) {
    sAnimatedImageLayersPrefCached = true;
    Preferences::AddBoolVarCache(&sAnimatedImageLayersEnabled,
                                 "layout.animated-image-layers.enabled",
                                 false);
  }

  return sAnimatedImageLayersEnabled;
}

bool
nsLayoutUtils::CSSFiltersEnabled()
{
  static bool sCSSFiltersEnabled;
  static bool sCSSFiltersPrefCached = false;

  if (!sCSSFiltersPrefCached) {
    sCSSFiltersPrefCached = true;
    Preferences::AddBoolVarCache(&sCSSFiltersEnabled,
                                 "layout.css.filters.enabled",
                                 false);
  }

  return sCSSFiltersEnabled;
}

bool
nsLayoutUtils::CSSClipPathShapesEnabled()
{
  static bool sCSSClipPathShapesEnabled;
  static bool sCSSClipPathShapesPrefCached = false;

  if (!sCSSClipPathShapesPrefCached) {
   sCSSClipPathShapesPrefCached = true;
   Preferences::AddBoolVarCache(&sCSSClipPathShapesEnabled,
                                "layout.css.clip-path-shapes.enabled",
                                false);
  }

  return sCSSClipPathShapesEnabled;
}

bool
nsLayoutUtils::UnsetValueEnabled()
{
  static bool sUnsetValueEnabled;
  static bool sUnsetValuePrefCached = false;

  if (!sUnsetValuePrefCached) {
    sUnsetValuePrefCached = true;
    Preferences::AddBoolVarCache(&sUnsetValueEnabled,
                                 "layout.css.unset-value.enabled",
                                 false);
  }

  return sUnsetValueEnabled;
}

bool
nsLayoutUtils::IsGridTemplateSubgridValueEnabled()
{
  static bool sGridTemplateSubgridValueEnabled;
  static bool sGridTemplateSubgridValueEnabledPrefCached = false;

  if (!sGridTemplateSubgridValueEnabledPrefCached) {
    sGridTemplateSubgridValueEnabledPrefCached = true;
    Preferences::AddBoolVarCache(&sGridTemplateSubgridValueEnabled,
                                 GRID_TEMPLATE_SUBGRID_ENABLED_PREF_NAME,
                                 false);
  }

  return sGridTemplateSubgridValueEnabled;
}

bool
nsLayoutUtils::IsTextAlignUnsafeValueEnabled()
{
  static bool sTextAlignUnsafeValueEnabled;
  static bool sTextAlignUnsafeValueEnabledPrefCached = false;

  if (!sTextAlignUnsafeValueEnabledPrefCached) {
    sTextAlignUnsafeValueEnabledPrefCached = true;
    Preferences::AddBoolVarCache(&sTextAlignUnsafeValueEnabled,
                                 TEXT_ALIGN_UNSAFE_ENABLED_PREF_NAME,
                                 false);
  }

  return sTextAlignUnsafeValueEnabled;
}

void
nsLayoutUtils::UnionChildOverflow(nsIFrame* aFrame,
                                  nsOverflowAreas& aOverflowAreas,
                                  FrameChildListIDs aSkipChildLists)
{
  // Iterate over all children except pop-ups.
  FrameChildListIDs skip = aSkipChildLists |
      nsIFrame::kSelectPopupList | nsIFrame::kPopupList;
  for (nsIFrame::ChildListIterator childLists(aFrame);
       !childLists.IsDone(); childLists.Next()) {
    if (skip.Contains(childLists.CurrentID())) {
      continue;
    }

    nsFrameList children = childLists.CurrentList();
    for (nsFrameList::Enumerator e(children); !e.AtEnd(); e.Next()) {
      nsIFrame* child = e.get();
      nsOverflowAreas childOverflow =
        child->GetOverflowAreas() + child->GetPosition();
      aOverflowAreas.UnionWith(childOverflow);
    }
  }
}

static void DestroyViewID(void* aObject, nsIAtom* aPropertyName,
                          void* aPropertyValue, void* aData)
{
  ViewID* id = static_cast<ViewID*>(aPropertyValue);
  GetContentMap().Remove(*id);
  delete id;
}

/**
 * A namespace class for static layout utilities.
 */

bool
nsLayoutUtils::FindIDFor(const nsIContent* aContent, ViewID* aOutViewId)
{
  void* scrollIdProperty = aContent->GetProperty(nsGkAtoms::RemoteId);
  if (scrollIdProperty) {
    *aOutViewId = *static_cast<ViewID*>(scrollIdProperty);
    return true;
  }
  return false;
}

ViewID
nsLayoutUtils::FindOrCreateIDFor(nsIContent* aContent)
{
  ViewID scrollId;

  if (!FindIDFor(aContent, &scrollId)) {
    scrollId = sScrollIdCounter++;
    aContent->SetProperty(nsGkAtoms::RemoteId, new ViewID(scrollId),
                          DestroyViewID);
    GetContentMap().Put(scrollId, aContent);
  }

  return scrollId;
}

nsIContent*
nsLayoutUtils::FindContentFor(ViewID aId)
{
  MOZ_ASSERT(aId != FrameMetrics::NULL_SCROLL_ID,
             "Cannot find a content element in map for null IDs.");
  nsIContent* content;
  bool exists = GetContentMap().Get(aId, &content);

  if (exists) {
    return content;
  } else {
    return nullptr;
  }
}

nsIFrame*
GetScrollFrameFromContent(nsIContent* aContent)
{
  nsIFrame* frame = aContent->GetPrimaryFrame();
  if (aContent->OwnerDoc()->GetRootElement() == aContent) {
    nsIPresShell* presShell = frame ? frame->PresContext()->PresShell() : nullptr;
    if (!presShell) {
      presShell = aContent->OwnerDoc()->GetShell();
    }
    // We want the scroll frame, the root scroll frame differs from all
    // others in that the primary frame is not the scroll frame.
    nsIFrame* rootScrollFrame = presShell ? presShell->GetRootScrollFrame() : nullptr;
    if (rootScrollFrame) {
      frame = rootScrollFrame;
    }
  }
  return frame;
}

nsIScrollableFrame*
nsLayoutUtils::FindScrollableFrameFor(ViewID aId)
{
  nsIContent* content = FindContentFor(aId);
  if (!content) {
    return nullptr;
  }

  nsIFrame* scrollFrame = GetScrollFrameFromContent(content);
  return scrollFrame ? scrollFrame->GetScrollTargetFrame() : nullptr;
}

static nsRect
ApplyRectMultiplier(nsRect aRect, float aMultiplier)
{
  if (aMultiplier == 1.0f) {
    return aRect;
  }
  float newWidth = aRect.width * aMultiplier;
  float newHeight = aRect.height * aMultiplier;
  float newX = aRect.x - ((newWidth - aRect.width) / 2.0f);
  float newY = aRect.y - ((newHeight - aRect.height) / 2.0f);
  // Rounding doesn't matter too much here, do a round-in
  return nsRect(ceil(newX), ceil(newY), floor(newWidth), floor(newHeight));
}

bool
nsLayoutUtils::UsesAsyncScrolling(nsIFrame* aFrame)
{
#ifdef MOZ_WIDGET_ANDROID
  // We always have async scrolling for android
  return true;
#endif

  return AsyncPanZoomEnabled(aFrame);
}

bool
nsLayoutUtils::AsyncPanZoomEnabled(nsIFrame* aFrame)
{
  // We use this as a shortcut, since if the compositor will never use APZ,
  // no widget will either.
  if (!gfxPlatform::AsyncPanZoomEnabled()) {
    return false;
  }

  nsIFrame *frame = nsLayoutUtils::GetDisplayRootFrame(aFrame);
  nsIWidget* widget = frame->GetNearestWidget();
  if (!widget) {
    return false;
  }
  return widget->AsyncPanZoomEnabled();
}

float
nsLayoutUtils::GetCurrentAPZResolutionScale(nsIPresShell* aShell) {
  return aShell ? aShell->GetCumulativeNonRootScaleResolution() : 1.0;
}

// Return the maximum displayport size, based on the LayerManager's maximum
// supported texture size. The result is in app units.
static nscoord
GetMaxDisplayPortSize(nsIContent* aContent)
{
  MOZ_ASSERT(!gfxPrefs::LayersTilesEnabled(), "Do not clamp displayports if tiling is enabled");

  nsIFrame* frame = aContent->GetPrimaryFrame();
  if (!frame) {
    return nscoord_MAX;
  }
  frame = nsLayoutUtils::GetDisplayRootFrame(frame);

  nsIWidget* widget = frame->GetNearestWidget();
  if (!widget) {
    return nscoord_MAX;
  }
  LayerManager* lm = widget->GetLayerManager();
  if (!lm) {
    return nscoord_MAX;
  }
  nsPresContext* presContext = frame->PresContext();

  int32_t maxSizeInDevPixels = lm->GetMaxTextureSize();
  if (maxSizeInDevPixels < 0 || maxSizeInDevPixels == INT_MAX) {
    return nscoord_MAX;
  }
  return presContext->DevPixelsToAppUnits(maxSizeInDevPixels);
}

static nsRect
GetDisplayPortFromRectData(nsIContent* aContent,
                           DisplayPortPropertyData* aRectData,
                           float aMultiplier)
{
  // In the case where the displayport is set as a rect, we assume it is
  // already aligned and clamped as necessary. The burden to do that is
  // on the setter of the displayport. In practice very few places set the
  // displayport directly as a rect (mostly tests). We still do need to
  // expand it by the multiplier though.
  return ApplyRectMultiplier(aRectData->mRect, aMultiplier);
}

static nsRect
GetDisplayPortFromMarginsData(nsIContent* aContent,
                              DisplayPortMarginsPropertyData* aMarginsData,
                              float aMultiplier)
{
  // In the case where the displayport is set via margins, we apply the margins
  // to a base rect. Then we align the expanded rect based on the alignment
  // requested, further expand the rect by the multiplier, and finally, clamp it
  // to the size of the scrollable rect.

  nsRect base;
  if (nsRect* baseData = static_cast<nsRect*>(aContent->GetProperty(nsGkAtoms::DisplayPortBase))) {
    base = *baseData;
  } else {
    // In theory we shouldn't get here, but we do sometimes (see bug 1212136).
    // Fall through for graceful handling.
  }

  nsIFrame* frame = GetScrollFrameFromContent(aContent);
  if (!frame) {
    // Turns out we can't really compute it. Oops. We still should return
    // something sane. Note that since we can't clamp the rect without a
    // frame, we don't apply the multiplier either as it can cause the result
    // to leak outside the scrollable area.
    NS_WARNING("Attempting to get a displayport from a content with no primary frame!");
    return base;
  }

  bool isRoot = false;
  if (aContent->OwnerDoc()->GetRootElement() == aContent) {
    isRoot = true;
  }

  nsPoint scrollPos;
  if (nsIScrollableFrame* scrollableFrame = frame->GetScrollTargetFrame()) {
    scrollPos = scrollableFrame->GetScrollPosition();
  }

  nsPresContext* presContext = frame->PresContext();
  int32_t auPerDevPixel = presContext->AppUnitsPerDevPixel();

  LayoutDeviceToScreenScale2D res(presContext->PresShell()->GetCumulativeResolution()
                                * nsLayoutUtils::GetTransformToAncestorScale(frame));

  // Calculate the expanded scrollable rect, which we'll be clamping the
  // displayport to.
  nsRect expandedScrollableRect =
    nsLayoutUtils::CalculateExpandedScrollableRect(frame);

  // GetTransformToAncestorScale() can return 0. In this case, just return the
  // base rect (clamped to the expanded scrollable rect), as other calculations
  // would run into divisions by zero.
  if (res == LayoutDeviceToScreenScale2D(0, 0)) {
    // Make sure the displayport remains within the scrollable rect.
    return base.MoveInsideAndClamp(expandedScrollableRect - scrollPos);
  }

  // First convert the base rect to screen pixels
  LayoutDeviceToScreenScale2D parentRes = res;
  if (isRoot) {
    // the base rect for root scroll frames is specified in the parent document
    // coordinate space, so it doesn't include the local resolution.
    float localRes = presContext->PresShell()->GetResolution();
    parentRes.xScale /= localRes;
    parentRes.yScale /= localRes;
  }
  ScreenRect screenRect = LayoutDeviceRect::FromAppUnits(base, auPerDevPixel)
                        * parentRes;

  // Note on the correctness of applying the alignment in Screen space:
  //   The correct space to apply the alignment in would be Layer space, but
  //   we don't necessarily know the scale to convert to Layer space at this
  //   point because Layout may not yet have chosen the resolution at which to
  //   render (it chooses that in FrameLayerBuilder, but this can be called
  //   during display list building). Therefore, we perform the alignment in
  //   Screen space, which basically assumes that Layout chose to render at
  //   screen resolution; since this is what Layout does most of the time,
  //   this is a good approximation. A proper solution would involve moving
  //   the choosing of the resolution to display-list building time.
  ScreenSize alignment;

  if (APZCCallbackHelper::IsDisplayportSuppressed()) {
    alignment = ScreenSize(1, 1);
  } else if (gfxPrefs::LayersTilesEnabled()) {
    // Don't align to tiles if they are too large, because we could expand
    // the displayport by a lot which can take more paint time. It's a tradeoff
    // though because if we don't align to tiles we have more waste on upload.
    IntSize tileSize = gfxVars::TileSize();
    alignment = ScreenSize(std::min(256, tileSize.width), std::min(256, tileSize.height));
  } else {
    // If we're not drawing with tiles then we need to be careful about not
    // hitting the max texture size and we only need 1 draw call per layer
    // so we can align to a smaller multiple.
    alignment = ScreenSize(128, 128);
  }

  // Avoid division by zero.
  if (alignment.width == 0) {
    alignment.width = 128;
  }
  if (alignment.height == 0) {
    alignment.height = 128;
  }

  if (gfxPrefs::LayersTilesEnabled()) {
    // Expand the rect by the margins
    screenRect.Inflate(aMarginsData->mMargins);
  } else {
    // Calculate the displayport to make sure we fit within the max texture size
    // when not tiling.
    nscoord maxSizeAppUnits = GetMaxDisplayPortSize(aContent);
    if (maxSizeAppUnits == nscoord_MAX) {
      // Pick a safe maximum displayport size for sanity purposes. This is the
      // lowest maximum texture size on tileless-platforms (Windows, D3D10).
      maxSizeAppUnits = presContext->DevPixelsToAppUnits(8192);
    }

    // The alignment code can round up to 3 tiles, we want to make sure
    // that the displayport can grow by up to 3 tiles without going
    // over the max texture size.
    const int MAX_ALIGN_ROUNDING = 3;

    // Find the maximum size in screen pixels.
    int32_t maxSizeDevPx = presContext->AppUnitsToDevPixels(maxSizeAppUnits);
    int32_t maxWidthScreenPx = floor(double(maxSizeDevPx) * res.xScale) -
      MAX_ALIGN_ROUNDING * alignment.width;
    int32_t maxHeightScreenPx = floor(double(maxSizeDevPx) * res.yScale) -
      MAX_ALIGN_ROUNDING * alignment.height;

    // For each axis, inflate the margins up to the maximum size.
    const ScreenMargin& margins = aMarginsData->mMargins;
    if (screenRect.height < maxHeightScreenPx) {
      int32_t budget = maxHeightScreenPx - screenRect.height;
      // Scale the margins down to fit into the budget if necessary, maintaining
      // their relative ratio.
      float scale = std::min(1.0f, float(budget) / margins.TopBottom());
      float top = margins.top * scale;
      float bottom = margins.bottom * scale;
      screenRect.y -= top;
      screenRect.height += top + bottom;
    }
    if (screenRect.width < maxWidthScreenPx) {
      int32_t budget = maxWidthScreenPx - screenRect.width;
      float scale = std::min(1.0f, float(budget) / margins.LeftRight());
      float left = margins.left * scale;
      float right = margins.right * scale;
      screenRect.x -= left;
      screenRect.width += left + right;
    }
  }

  ScreenPoint scrollPosScreen = LayoutDevicePoint::FromAppUnits(scrollPos, auPerDevPixel)
                              * res;

  // Round-out the display port to the nearest alignment (tiles)
  screenRect += scrollPosScreen;
  float x = alignment.width * floor(screenRect.x / alignment.width);
  float y = alignment.height * floor(screenRect.y / alignment.height);
  float w = alignment.width * ceil(screenRect.width / alignment.width + 1);
  float h = alignment.height * ceil(screenRect.height / alignment.height + 1);
  screenRect = ScreenRect(x, y, w, h);
  screenRect -= scrollPosScreen;

  // Convert the aligned rect back into app units.
  nsRect result = LayoutDeviceRect::ToAppUnits(screenRect / res, auPerDevPixel);

  // If we have non-zero margins, expand the displayport for the low-res buffer
  // if that's what we're drawing. If we have zero margins, we want the
  // displayport to reflect the scrollport.
  if (aMarginsData->mMargins != ScreenMargin()) {
    result = ApplyRectMultiplier(result, aMultiplier);
  }

  // Make sure the displayport remains within the scrollable rect.
  result = result.MoveInsideAndClamp(expandedScrollableRect - scrollPos);

  return result;
}

static bool
HasVisibleAnonymousContents(nsIDocument* aDoc)
{
  for (RefPtr<AnonymousContent>& ac : aDoc->GetAnonymousContents()) {
    Element* elem = ac->GetContentNode();
    // We check to see if the anonymous content node has a frame. If it doesn't,
    // that means that's not visible to the user because e.g. it's display:none.
    // For now we assume that if it has a frame, it is visible. We might be able
    // to refine this further by adding complexity if it turns out this condition
    // results in a lot of false positives.
    if (elem && elem->GetPrimaryFrame()) {
      return true;
    }
  }
  return false;
}

bool
nsLayoutUtils::ShouldDisableApzForElement(nsIContent* aContent)
{
  if (!aContent) {
    return false;
  }

  nsIDocument* doc = aContent->GetComposedDoc();
  nsIPresShell* rootShell = APZCCallbackHelper::GetRootContentDocumentPresShellForContent(aContent);
  if (rootShell) {
    if (nsIDocument* rootDoc = rootShell->GetDocument()) {
      nsIContent* rootContent = rootShell->GetRootScrollFrame()
          ? rootShell->GetRootScrollFrame()->GetContent()
          : rootDoc->GetDocumentElement();
      // For the AccessibleCaret: disable APZ on any scrollable subframes that
      // are not the root scrollframe of a document, if the document has any
      // visible anonymous contents.
      // If we find this is triggering in too many scenarios then we might
      // want to tighten this check further. The main use cases for which we want
      // to disable APZ as of this writing are listed in bug 1316318.
      if (aContent != rootContent && HasVisibleAnonymousContents(rootDoc)) {
        return true;
      }
    }
  }

  if (!doc) {
    return false;
  }
  return gfxPrefs::APZDisableForScrollLinkedEffects() &&
         doc->HasScrollLinkedEffect();
}

static bool
GetDisplayPortData(nsIContent* aContent,
                   DisplayPortPropertyData** aOutRectData,
                   DisplayPortMarginsPropertyData** aOutMarginsData)
{
  MOZ_ASSERT(aOutRectData && aOutMarginsData);

  *aOutRectData =
    static_cast<DisplayPortPropertyData*>(aContent->GetProperty(nsGkAtoms::DisplayPort));
  *aOutMarginsData =
    static_cast<DisplayPortMarginsPropertyData*>(aContent->GetProperty(nsGkAtoms::DisplayPortMargins));

  if (!*aOutRectData && !*aOutMarginsData) {
    // This content element has no displayport data at all
    return false;
  }

  if (*aOutRectData && *aOutMarginsData) {
    // choose margins if equal priority
    if ((*aOutRectData)->mPriority > (*aOutMarginsData)->mPriority) {
      *aOutMarginsData = nullptr;
    } else {
      *aOutRectData = nullptr;
    }
  }

  NS_ASSERTION((*aOutRectData == nullptr) != (*aOutMarginsData == nullptr),
               "Only one of aOutRectData or aOutMarginsData should be set!");

  return true;
}

bool
nsLayoutUtils::IsMissingDisplayPortBaseRect(nsIContent* aContent)
{
  DisplayPortPropertyData* rectData = nullptr;
  DisplayPortMarginsPropertyData* marginsData = nullptr;

  if (GetDisplayPortData(aContent, &rectData, &marginsData) && marginsData) {
    return !aContent->GetProperty(nsGkAtoms::DisplayPortBase);
  }

  return false;
}

static bool
GetDisplayPortImpl(nsIContent* aContent, nsRect* aResult, float aMultiplier)
{
  DisplayPortPropertyData* rectData = nullptr;
  DisplayPortMarginsPropertyData* marginsData = nullptr;

  if (!GetDisplayPortData(aContent, &rectData, &marginsData)) {
    return false;
  }

  if (!aResult) {
    // We have displayport data, but the caller doesn't want the actual
    // rect, so we don't need to actually compute it.
    return true;
  }

  nsRect result;
  if (rectData) {
    result = GetDisplayPortFromRectData(aContent, rectData, aMultiplier);
  } else if (APZCCallbackHelper::IsDisplayportSuppressed() ||
      nsLayoutUtils::ShouldDisableApzForElement(aContent)) {
    DisplayPortMarginsPropertyData noMargins(ScreenMargin(), 1);
    result = GetDisplayPortFromMarginsData(aContent, &noMargins, aMultiplier);
  } else {
    result = GetDisplayPortFromMarginsData(aContent, marginsData, aMultiplier);
  }

  if (!gfxPrefs::LayersTilesEnabled()) {
    // Either we should have gotten a valid rect directly from the displayport
    // base, or we should have computed a valid rect from the margins.
    NS_ASSERTION(result.width <= GetMaxDisplayPortSize(aContent),
                 "Displayport must be a valid texture size");
    NS_ASSERTION(result.height <= GetMaxDisplayPortSize(aContent),
                 "Displayport must be a valid texture size");
  }

  *aResult = result;
  return true;
}

void
TranslateFromScrollPortToScrollFrame(nsIContent* aContent, nsRect* aRect)
{
  MOZ_ASSERT(aRect);
  nsIFrame* frame = GetScrollFrameFromContent(aContent);
  nsIScrollableFrame* scrollableFrame = frame ? frame->GetScrollTargetFrame() : nullptr;
  if (scrollableFrame) {
    *aRect += scrollableFrame->GetScrollPortRect().TopLeft();
  }
}

bool
nsLayoutUtils::GetDisplayPort(nsIContent* aContent, nsRect *aResult,
  RelativeTo aRelativeTo /* = RelativeTo::ScrollPort */)
{
  float multiplier =
    gfxPrefs::UseLowPrecisionBuffer() ? 1.0f / gfxPrefs::LowPrecisionResolution() : 1.0f;
  bool usingDisplayPort = GetDisplayPortImpl(aContent, aResult, multiplier);
  if (aResult && usingDisplayPort && aRelativeTo == RelativeTo::ScrollFrame) {
    TranslateFromScrollPortToScrollFrame(aContent, aResult);
  }
  return usingDisplayPort;
}

bool
nsLayoutUtils::HasDisplayPort(nsIContent* aContent) {
  return GetDisplayPort(aContent, nullptr);
}

/* static */ bool
nsLayoutUtils::GetDisplayPortForVisibilityTesting(
  nsIContent* aContent,
  nsRect* aResult,
  RelativeTo aRelativeTo /* = RelativeTo::ScrollPort */)
{
  MOZ_ASSERT(aResult);
  bool usingDisplayPort = GetDisplayPortImpl(aContent, aResult, 1.0f);
  if (usingDisplayPort && aRelativeTo == RelativeTo::ScrollFrame) {
    TranslateFromScrollPortToScrollFrame(aContent, aResult);
  }
  return usingDisplayPort;
}

bool
nsLayoutUtils::SetDisplayPortMargins(nsIContent* aContent,
                                     nsIPresShell* aPresShell,
                                     const ScreenMargin& aMargins,
                                     uint32_t aPriority,
                                     RepaintMode aRepaintMode)
{
  MOZ_ASSERT(aContent);
  MOZ_ASSERT(aContent->GetComposedDoc() == aPresShell->GetDocument());

  DisplayPortMarginsPropertyData* currentData =
    static_cast<DisplayPortMarginsPropertyData*>(aContent->GetProperty(nsGkAtoms::DisplayPortMargins));
  if (currentData && currentData->mPriority > aPriority) {
    return false;
  }

  nsRect oldDisplayPort;
  bool hadDisplayPort = GetHighResolutionDisplayPort(aContent, &oldDisplayPort);

  aContent->SetProperty(nsGkAtoms::DisplayPortMargins,
                        new DisplayPortMarginsPropertyData(
                            aMargins, aPriority),
                        nsINode::DeleteProperty<DisplayPortMarginsPropertyData>);

  nsRect newDisplayPort;
  DebugOnly<bool> hasDisplayPort = GetHighResolutionDisplayPort(aContent, &newDisplayPort);
  MOZ_ASSERT(hasDisplayPort);

  bool changed = !hadDisplayPort ||
        !oldDisplayPort.IsEqualEdges(newDisplayPort);

  if (gfxPrefs::LayoutUseContainersForRootFrames()) {
    nsIFrame* rootScrollFrame = aPresShell->GetRootScrollFrame();
    if (rootScrollFrame &&
        aContent == rootScrollFrame->GetContent() &&
        nsLayoutUtils::UsesAsyncScrolling(rootScrollFrame))
    {
      // We are setting a root displayport for a document.
      // If we have APZ, then set a special flag on the pres shell so
      // that we don't get scrollbars drawn.
      aPresShell->SetIgnoreViewportScrolling(true);
    }
  }

  if (changed && aRepaintMode == RepaintMode::Repaint) {
    nsIFrame* frame = aContent->GetPrimaryFrame();
    if (frame) {
      frame->SchedulePaint();
    }
  }

  nsIFrame* frame = GetScrollFrameFromContent(aContent);
  nsIScrollableFrame* scrollableFrame = frame ? frame->GetScrollTargetFrame() : nullptr;
  if (!scrollableFrame) {
    return true;
  }

  scrollableFrame->TriggerDisplayPortExpiration();

  // Display port margins changing means that the set of visible frames may
  // have drastically changed. Check if we should schedule an update.
  hadDisplayPort =
    scrollableFrame->GetDisplayPortAtLastApproximateFrameVisibilityUpdate(&oldDisplayPort);

  bool needVisibilityUpdate = !hadDisplayPort;
  // Check if the total size has changed by a large factor.
  if (!needVisibilityUpdate) {
    if ((newDisplayPort.width > 2 * oldDisplayPort.width) ||
        (oldDisplayPort.width > 2 * newDisplayPort.width) ||
        (newDisplayPort.height > 2 * oldDisplayPort.height) ||
        (oldDisplayPort.height > 2 * newDisplayPort.height)) {
      needVisibilityUpdate = true;
    }
  }
  // Check if it's moved by a significant amount.
  if (!needVisibilityUpdate) {
    if (nsRect* baseData = static_cast<nsRect*>(aContent->GetProperty(nsGkAtoms::DisplayPortBase))) {
      nsRect base = *baseData;
      if ((std::abs(newDisplayPort.X() - oldDisplayPort.X()) > base.width) ||
          (std::abs(newDisplayPort.XMost() - oldDisplayPort.XMost()) > base.width) ||
          (std::abs(newDisplayPort.Y() - oldDisplayPort.Y()) > base.height) ||
          (std::abs(newDisplayPort.YMost() - oldDisplayPort.YMost()) > base.height)) {
        needVisibilityUpdate = true;
      }
    }
  }
  if (needVisibilityUpdate) {
    aPresShell->ScheduleApproximateFrameVisibilityUpdateNow();
  }

  return true;
}

void
nsLayoutUtils::SetDisplayPortBase(nsIContent* aContent, const nsRect& aBase)
{
  aContent->SetProperty(nsGkAtoms::DisplayPortBase, new nsRect(aBase),
                        nsINode::DeleteProperty<nsRect>);
}

void
nsLayoutUtils::SetDisplayPortBaseIfNotSet(nsIContent* aContent, const nsRect& aBase)
{
  if (!aContent->GetProperty(nsGkAtoms::DisplayPortBase)) {
    SetDisplayPortBase(aContent, aBase);
  }
}

bool
nsLayoutUtils::GetCriticalDisplayPort(nsIContent* aContent, nsRect* aResult)
{
  if (gfxPrefs::UseLowPrecisionBuffer()) {
    return GetDisplayPortImpl(aContent, aResult, 1.0f);
  }
  return false;
}

bool
nsLayoutUtils::HasCriticalDisplayPort(nsIContent* aContent)
{
  return GetCriticalDisplayPort(aContent, nullptr);
}

bool
nsLayoutUtils::GetHighResolutionDisplayPort(nsIContent* aContent, nsRect* aResult)
{
  if (gfxPrefs::UseLowPrecisionBuffer()) {
    return GetCriticalDisplayPort(aContent, aResult);
  }
  return GetDisplayPort(aContent, aResult);
}

void
nsLayoutUtils::RemoveDisplayPort(nsIContent* aContent)
{
  aContent->DeleteProperty(nsGkAtoms::DisplayPort);
  aContent->DeleteProperty(nsGkAtoms::DisplayPortMargins);
}

nsContainerFrame*
nsLayoutUtils::LastContinuationWithChild(nsContainerFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "NULL frame pointer");
  nsIFrame* f = aFrame->LastContinuation();
  while (!f->PrincipalChildList().FirstChild() && f->GetPrevContinuation()) {
    f = f->GetPrevContinuation();
  }
  return static_cast<nsContainerFrame*>(f);
}

//static
FrameChildListID
nsLayoutUtils::GetChildListNameFor(nsIFrame* aChildFrame)
{
  nsIFrame::ChildListID id = nsIFrame::kPrincipalList;

  if (aChildFrame->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER) {
    nsIFrame* pif = aChildFrame->GetPrevInFlow();
    if (pif->GetParent() == aChildFrame->GetParent()) {
      id = nsIFrame::kExcessOverflowContainersList;
    }
    else {
      id = nsIFrame::kOverflowContainersList;
    }
  }
  // See if the frame is moved out of the flow
  else if (aChildFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
    // Look at the style information to tell
    const nsStyleDisplay* disp = aChildFrame->StyleDisplay();

    if (NS_STYLE_POSITION_ABSOLUTE == disp->mPosition) {
      id = nsIFrame::kAbsoluteList;
    } else if (NS_STYLE_POSITION_FIXED == disp->mPosition) {
      if (nsLayoutUtils::IsReallyFixedPos(aChildFrame)) {
        id = nsIFrame::kFixedList;
      } else {
        id = nsIFrame::kAbsoluteList;
      }
#ifdef MOZ_XUL
    } else if (StyleDisplay::MozPopup == disp->mDisplay) {
      // Out-of-flows that are DISPLAY_POPUP must be kids of the root popup set
#ifdef DEBUG
      nsIFrame* parent = aChildFrame->GetParent();
      NS_ASSERTION(parent && parent->IsPopupSetFrame(), "Unexpected parent");
#endif // DEBUG

      id = nsIFrame::kPopupList;
#endif // MOZ_XUL
    } else {
      NS_ASSERTION(aChildFrame->IsFloating(), "not a floated frame");
      id = nsIFrame::kFloatList;
    }

  } else {
    LayoutFrameType childType = aChildFrame->Type();
    if (LayoutFrameType::MenuPopup == childType) {
      nsIFrame* parent = aChildFrame->GetParent();
      MOZ_ASSERT(parent, "nsMenuPopupFrame can't be the root frame");
      if (parent) {
        if (parent->IsPopupSetFrame()) {
          id = nsIFrame::kPopupList;
        } else {
          nsIFrame* firstPopup = parent->GetChildList(nsIFrame::kPopupList).FirstChild();
          MOZ_ASSERT(!firstPopup || !firstPopup->GetNextSibling(),
                     "We assume popupList only has one child, but it has more.");
          id = firstPopup == aChildFrame
                 ? nsIFrame::kPopupList
                 : nsIFrame::kPrincipalList;
        }
      } else {
        id = nsIFrame::kPrincipalList;
      }
    } else if (LayoutFrameType::TableColGroup == childType) {
      id = nsIFrame::kColGroupList;
    } else if (aChildFrame->IsTableCaption()) {
      id = nsIFrame::kCaptionList;
    } else {
      id = nsIFrame::kPrincipalList;
    }
  }

#ifdef DEBUG
  // Verify that the frame is actually in that child list or in the
  // corresponding overflow list.
  nsContainerFrame* parent = aChildFrame->GetParent();
  bool found = parent->GetChildList(id).ContainsFrame(aChildFrame);
  if (!found) {
    if (!(aChildFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
      found = parent->GetChildList(nsIFrame::kOverflowList)
                .ContainsFrame(aChildFrame);
    }
    else if (aChildFrame->IsFloating()) {
      found = parent->GetChildList(nsIFrame::kOverflowOutOfFlowList)
                .ContainsFrame(aChildFrame);
      if (!found) {
        found = parent->GetChildList(nsIFrame::kPushedFloatsList)
                  .ContainsFrame(aChildFrame);
      }
    }
    // else it's positioned and should have been on the 'id' child list.
    NS_POSTCONDITION(found, "not in child list");
  }
#endif

  return id;
}

static Element*
GetPseudo(const nsIContent* aContent, nsIAtom* aPseudoProperty)
{
  MOZ_ASSERT(aPseudoProperty == nsGkAtoms::beforePseudoProperty ||
             aPseudoProperty == nsGkAtoms::afterPseudoProperty);
  return static_cast<Element*>(aContent->GetProperty(aPseudoProperty));
}

/*static*/ Element*
nsLayoutUtils::GetBeforePseudo(const nsIContent* aContent)
{
  return GetPseudo(aContent, nsGkAtoms::beforePseudoProperty);
}

/*static*/ nsIFrame*
nsLayoutUtils::GetBeforeFrame(const nsIContent* aContent)
{
  Element* pseudo = GetBeforePseudo(aContent);
  return pseudo ? pseudo->GetPrimaryFrame() : nullptr;
}

/*static*/ Element*
nsLayoutUtils::GetAfterPseudo(const nsIContent* aContent)
{
  return GetPseudo(aContent, nsGkAtoms::afterPseudoProperty);
}

/*static*/ nsIFrame*
nsLayoutUtils::GetAfterFrame(const nsIContent* aContent)
{
  Element* pseudo = GetAfterPseudo(aContent);
  return pseudo ? pseudo->GetPrimaryFrame() : nullptr;
}

// static
nsIFrame*
nsLayoutUtils::GetClosestFrameOfType(nsIFrame* aFrame,
                                     LayoutFrameType aFrameType,
                                     nsIFrame* aStopAt)
{
  for (nsIFrame* frame = aFrame; frame; frame = frame->GetParent()) {
    if (frame->Type() == aFrameType) {
      return frame;
    }
    if (frame == aStopAt) {
      break;
    }
  }
  return nullptr;
}

/* static */ nsIFrame*
nsLayoutUtils::GetPageFrame(nsIFrame* aFrame)
{
  return GetClosestFrameOfType(aFrame, LayoutFrameType::Page);
}

// static
nsIFrame*
nsLayoutUtils::GetStyleFrame(nsIFrame* aFrame)
{
  if (aFrame->IsTableWrapperFrame()) {
    nsIFrame* inner = aFrame->PrincipalChildList().FirstChild();
    // inner may be null, if aFrame is mid-destruction
    return inner;
  }

  return aFrame;
}

nsIFrame*
nsLayoutUtils::GetStyleFrame(const nsIContent* aContent)
{
  nsIFrame *frame = aContent->GetPrimaryFrame();
  if (!frame) {
    return nullptr;
  }

  return nsLayoutUtils::GetStyleFrame(frame);
}

/* static */ nsIFrame*
nsLayoutUtils::GetRealPrimaryFrameFor(const nsIContent* aContent)
{
  nsIFrame *frame = aContent->GetPrimaryFrame();
  if (!frame) {
    return nullptr;
  }

  return nsPlaceholderFrame::GetRealFrameFor(frame);
}

nsIFrame*
nsLayoutUtils::GetFloatFromPlaceholder(nsIFrame* aFrame) {
  NS_ASSERTION(aFrame->IsPlaceholderFrame(), "Must have a placeholder here");
  if (aFrame->GetStateBits() & PLACEHOLDER_FOR_FLOAT) {
    nsIFrame *outOfFlowFrame =
      nsPlaceholderFrame::GetRealFrameForPlaceholder(aFrame);
    NS_ASSERTION(outOfFlowFrame->IsFloating(),
                 "How did that happen?");
    return outOfFlowFrame;
  }

  return nullptr;
}

// static
bool
nsLayoutUtils::IsGeneratedContentFor(nsIContent* aContent,
                                     nsIFrame* aFrame,
                                     nsIAtom* aPseudoElement)
{
  NS_PRECONDITION(aFrame, "Must have a frame");
  NS_PRECONDITION(aPseudoElement, "Must have a pseudo name");

  if (!aFrame->IsGeneratedContentFrame()) {
    return false;
  }
  nsIFrame* parent = aFrame->GetParent();
  NS_ASSERTION(parent, "Generated content can't be root frame");
  if (parent->IsGeneratedContentFrame()) {
    // Not the root of the generated content
    return false;
  }

  if (aContent && parent->GetContent() != aContent) {
    return false;
  }

  return (aFrame->GetContent()->NodeInfo()->NameAtom() == nsGkAtoms::mozgeneratedcontentbefore) ==
    (aPseudoElement == nsCSSPseudoElements::before);
}

// static
nsIFrame*
nsLayoutUtils::GetCrossDocParentFrame(const nsIFrame* aFrame,
                                      nsPoint* aExtraOffset)
{
  nsIFrame* p = aFrame->GetParent();
  if (p)
    return p;

  nsView* v = aFrame->GetView();
  if (!v)
    return nullptr;
  v = v->GetParent(); // anonymous inner view
  if (!v)
    return nullptr;
  if (aExtraOffset) {
    *aExtraOffset += v->GetPosition();
  }
  v = v->GetParent(); // subdocumentframe's view
  return v ? v->GetFrame() : nullptr;
}

// static
bool
nsLayoutUtils::IsProperAncestorFrameCrossDoc(nsIFrame* aAncestorFrame, nsIFrame* aFrame,
                                             nsIFrame* aCommonAncestor)
{
  if (aFrame == aAncestorFrame)
    return false;
  return IsAncestorFrameCrossDoc(aAncestorFrame, aFrame, aCommonAncestor);
}

// static
bool
nsLayoutUtils::IsAncestorFrameCrossDoc(const nsIFrame* aAncestorFrame, const nsIFrame* aFrame,
                                       const nsIFrame* aCommonAncestor)
{
  for (const nsIFrame* f = aFrame; f != aCommonAncestor;
       f = GetCrossDocParentFrame(f)) {
    if (f == aAncestorFrame)
      return true;
  }
  return aCommonAncestor == aAncestorFrame;
}

// static
bool
nsLayoutUtils::IsProperAncestorFrame(nsIFrame* aAncestorFrame, nsIFrame* aFrame,
                                     nsIFrame* aCommonAncestor)
{
  if (aFrame == aAncestorFrame)
    return false;
  for (nsIFrame* f = aFrame; f != aCommonAncestor; f = f->GetParent()) {
    if (f == aAncestorFrame)
      return true;
  }
  return aCommonAncestor == aAncestorFrame;
}

// static
int32_t
nsLayoutUtils::DoCompareTreePosition(nsIContent* aContent1,
                                     nsIContent* aContent2,
                                     int32_t aIf1Ancestor,
                                     int32_t aIf2Ancestor,
                                     const nsIContent* aCommonAncestor)
{
  NS_PRECONDITION(aContent1, "aContent1 must not be null");
  NS_PRECONDITION(aContent2, "aContent2 must not be null");

  AutoTArray<nsINode*, 32> content1Ancestors;
  nsINode* c1;
  for (c1 = aContent1; c1 && c1 != aCommonAncestor; c1 = c1->GetParentNode()) {
    content1Ancestors.AppendElement(c1);
  }
  if (!c1 && aCommonAncestor) {
    // So, it turns out aCommonAncestor was not an ancestor of c1. Oops.
    // Never mind. We can continue as if aCommonAncestor was null.
    aCommonAncestor = nullptr;
  }

  AutoTArray<nsINode*, 32> content2Ancestors;
  nsINode* c2;
  for (c2 = aContent2; c2 && c2 != aCommonAncestor; c2 = c2->GetParentNode()) {
    content2Ancestors.AppendElement(c2);
  }
  if (!c2 && aCommonAncestor) {
    // So, it turns out aCommonAncestor was not an ancestor of c2.
    // We need to retry with no common ancestor hint.
    return DoCompareTreePosition(aContent1, aContent2,
                                 aIf1Ancestor, aIf2Ancestor, nullptr);
  }

  int last1 = content1Ancestors.Length() - 1;
  int last2 = content2Ancestors.Length() - 1;
  nsINode* content1Ancestor = nullptr;
  nsINode* content2Ancestor = nullptr;
  while (last1 >= 0 && last2 >= 0
         && ((content1Ancestor = content1Ancestors.ElementAt(last1)) ==
             (content2Ancestor = content2Ancestors.ElementAt(last2)))) {
    last1--;
    last2--;
  }

  if (last1 < 0) {
    if (last2 < 0) {
      NS_ASSERTION(aContent1 == aContent2, "internal error?");
      return 0;
    }
    // aContent1 is an ancestor of aContent2
    return aIf1Ancestor;
  }

  if (last2 < 0) {
    // aContent2 is an ancestor of aContent1
    return aIf2Ancestor;
  }

  // content1Ancestor != content2Ancestor, so they must be siblings with the same parent
  nsINode* parent = content1Ancestor->GetParentNode();
#ifdef DEBUG
  // TODO: remove the uglyness, see bug 598468.
  NS_ASSERTION(gPreventAssertInCompareTreePosition || parent,
               "no common ancestor at all???");
#endif // DEBUG
  if (!parent) { // different documents??
    return 0;
  }

  int32_t index1 = parent->IndexOf(content1Ancestor);
  int32_t index2 = parent->IndexOf(content2Ancestor);
  if (index1 < 0 || index2 < 0) {
    // one of them must be anonymous; we can't determine the order
    return 0;
  }

  return index1 - index2;
}

// static
nsIFrame*
nsLayoutUtils::FillAncestors(nsIFrame* aFrame,
                             nsIFrame* aStopAtAncestor,
                             nsTArray<nsIFrame*>* aAncestors)
{
  while (aFrame && aFrame != aStopAtAncestor) {
    aAncestors->AppendElement(aFrame);
    aFrame = nsLayoutUtils::GetParentOrPlaceholderFor(aFrame);
  }
  return aFrame;
}

// Return true if aFrame1 is after aFrame2
static bool IsFrameAfter(nsIFrame* aFrame1, nsIFrame* aFrame2)
{
  nsIFrame* f = aFrame2;
  do {
    f = f->GetNextSibling();
    if (f == aFrame1)
      return true;
  } while (f);
  return false;
}

// static
int32_t
nsLayoutUtils::DoCompareTreePosition(nsIFrame* aFrame1,
                                     nsIFrame* aFrame2,
                                     int32_t aIf1Ancestor,
                                     int32_t aIf2Ancestor,
                                     nsIFrame* aCommonAncestor)
{
  NS_PRECONDITION(aFrame1, "aFrame1 must not be null");
  NS_PRECONDITION(aFrame2, "aFrame2 must not be null");

  AutoTArray<nsIFrame*,20> frame2Ancestors;
  nsIFrame* nonCommonAncestor =
    FillAncestors(aFrame2, aCommonAncestor, &frame2Ancestors);

  return DoCompareTreePosition(aFrame1, aFrame2, frame2Ancestors,
                               aIf1Ancestor, aIf2Ancestor,
                               nonCommonAncestor ? aCommonAncestor : nullptr);
}

// static
int32_t
nsLayoutUtils::DoCompareTreePosition(nsIFrame* aFrame1,
                                     nsIFrame* aFrame2,
                                     nsTArray<nsIFrame*>& aFrame2Ancestors,
                                     int32_t aIf1Ancestor,
                                     int32_t aIf2Ancestor,
                                     nsIFrame* aCommonAncestor)
{
  NS_PRECONDITION(aFrame1, "aFrame1 must not be null");
  NS_PRECONDITION(aFrame2, "aFrame2 must not be null");

  nsPresContext* presContext = aFrame1->PresContext();
  if (presContext != aFrame2->PresContext()) {
    NS_ERROR("no common ancestor at all, different documents");
    return 0;
  }

  AutoTArray<nsIFrame*,20> frame1Ancestors;
  if (aCommonAncestor &&
      !FillAncestors(aFrame1, aCommonAncestor, &frame1Ancestors)) {
    // We reached the root of the frame tree ... if aCommonAncestor was set,
    // it is wrong
    return DoCompareTreePosition(aFrame1, aFrame2,
                                 aIf1Ancestor, aIf2Ancestor, nullptr);
  }

  int32_t last1 = int32_t(frame1Ancestors.Length()) - 1;
  int32_t last2 = int32_t(aFrame2Ancestors.Length()) - 1;
  while (last1 >= 0 && last2 >= 0 &&
         frame1Ancestors[last1] == aFrame2Ancestors[last2]) {
    last1--;
    last2--;
  }

  if (last1 < 0) {
    if (last2 < 0) {
      NS_ASSERTION(aFrame1 == aFrame2, "internal error?");
      return 0;
    }
    // aFrame1 is an ancestor of aFrame2
    return aIf1Ancestor;
  }

  if (last2 < 0) {
    // aFrame2 is an ancestor of aFrame1
    return aIf2Ancestor;
  }

  nsIFrame* ancestor1 = frame1Ancestors[last1];
  nsIFrame* ancestor2 = aFrame2Ancestors[last2];
  // Now we should be able to walk sibling chains to find which one is first
  if (IsFrameAfter(ancestor2, ancestor1))
    return -1;
  if (IsFrameAfter(ancestor1, ancestor2))
    return 1;
  NS_WARNING("Frames were in different child lists???");
  return 0;
}

// static
nsIFrame* nsLayoutUtils::GetLastSibling(nsIFrame* aFrame) {
  if (!aFrame) {
    return nullptr;
  }

  nsIFrame* next;
  while ((next = aFrame->GetNextSibling()) != nullptr) {
    aFrame = next;
  }
  return aFrame;
}

// static
nsView*
nsLayoutUtils::FindSiblingViewFor(nsView* aParentView, nsIFrame* aFrame) {
  nsIFrame* parentViewFrame = aParentView->GetFrame();
  nsIContent* parentViewContent = parentViewFrame ? parentViewFrame->GetContent() : nullptr;
  for (nsView* insertBefore = aParentView->GetFirstChild(); insertBefore;
       insertBefore = insertBefore->GetNextSibling()) {
    nsIFrame* f = insertBefore->GetFrame();
    if (!f) {
      // this view could be some anonymous view attached to a meaningful parent
      for (nsView* searchView = insertBefore->GetParent(); searchView;
           searchView = searchView->GetParent()) {
        f = searchView->GetFrame();
        if (f) {
          break;
        }
      }
      NS_ASSERTION(f, "Can't find a frame anywhere!");
    }
    if (!f || !aFrame->GetContent() || !f->GetContent() ||
        CompareTreePosition(aFrame->GetContent(), f->GetContent(), parentViewContent) > 0) {
      // aFrame's content is after f's content (or we just don't know),
      // so put our view before f's view
      return insertBefore;
    }
  }
  return nullptr;
}

//static
nsIScrollableFrame*
nsLayoutUtils::GetScrollableFrameFor(const nsIFrame *aScrolledFrame)
{
  nsIFrame *frame = aScrolledFrame->GetParent();
  nsIScrollableFrame *sf = do_QueryFrame(frame);
  return (sf && sf->GetScrolledFrame() == aScrolledFrame) ? sf : nullptr;
}

/* static */ void
nsLayoutUtils::SetFixedPositionLayerData(Layer* aLayer,
                                         const nsIFrame* aViewportFrame,
                                         const nsRect& aAnchorRect,
                                         const nsIFrame* aFixedPosFrame,
                                         nsPresContext* aPresContext,
                                         const ContainerLayerParameters& aContainerParameters) {
  // Find out the rect of the viewport frame relative to the reference frame.
  // This, in conjunction with the container scale, will correspond to the
  // coordinate-space of the built layer.
  float factor = aPresContext->AppUnitsPerDevPixel();
  Rect anchorRect(NSAppUnitsToFloatPixels(aAnchorRect.x, factor) *
                    aContainerParameters.mXScale,
                  NSAppUnitsToFloatPixels(aAnchorRect.y, factor) *
                    aContainerParameters.mYScale,
                  NSAppUnitsToFloatPixels(aAnchorRect.width, factor) *
                    aContainerParameters.mXScale,
                  NSAppUnitsToFloatPixels(aAnchorRect.height, factor) *
                    aContainerParameters.mYScale);
  // Need to transform anchorRect from the container layer's coordinate system
  // into aLayer's coordinate system.
  Matrix transform2d;
  if (aLayer->GetTransform().Is2D(&transform2d)) {
    transform2d.Invert();
    anchorRect = transform2d.TransformBounds(anchorRect);
  } else {
    NS_ERROR("3D transform found between fixedpos content and its viewport (should never happen)");
    anchorRect = Rect(0,0,0,0);
  }

  // Work out the anchor point for this fixed position layer. We assume that
  // any positioning set (left/top/right/bottom) indicates that the
  // corresponding side of its container should be the anchor point,
  // defaulting to top-left.
  LayerPoint anchor(anchorRect.x, anchorRect.y);

  int32_t sides = eSideBitsNone;
  if (aFixedPosFrame != aViewportFrame) {
    const nsStylePosition* position = aFixedPosFrame->StylePosition();
    if (position->mOffset.GetRightUnit() != eStyleUnit_Auto) {
      sides |= eSideBitsRight;
      if (position->mOffset.GetLeftUnit() != eStyleUnit_Auto) {
        sides |= eSideBitsLeft;
        anchor.x = anchorRect.x + anchorRect.width / 2.f;
      } else {
        anchor.x = anchorRect.XMost();
      }
    } else if (position->mOffset.GetLeftUnit() != eStyleUnit_Auto) {
      sides |= eSideBitsLeft;
    }
    if (position->mOffset.GetBottomUnit() != eStyleUnit_Auto) {
      sides |= eSideBitsBottom;
      if (position->mOffset.GetTopUnit() != eStyleUnit_Auto) {
        sides |= eSideBitsTop;
        anchor.y = anchorRect.y + anchorRect.height / 2.f;
      } else {
        anchor.y = anchorRect.YMost();
      }
    } else if (position->mOffset.GetTopUnit() != eStyleUnit_Auto) {
      sides |= eSideBitsTop;
    }
  }

  ViewID id = FrameMetrics::NULL_SCROLL_ID;
  if (nsIFrame* rootScrollFrame = aPresContext->PresShell()->GetRootScrollFrame()) {
    if (nsIContent* content = rootScrollFrame->GetContent()) {
      id = FindOrCreateIDFor(content);
    }
  }
  aLayer->SetFixedPositionData(id, anchor, sides);
}

bool
nsLayoutUtils::ViewportHasDisplayPort(nsPresContext* aPresContext)
{
  nsIFrame* rootScrollFrame =
    aPresContext->PresShell()->GetRootScrollFrame();
  return rootScrollFrame &&
    nsLayoutUtils::HasDisplayPort(rootScrollFrame->GetContent());
}

bool
nsLayoutUtils::IsFixedPosFrameInDisplayPort(const nsIFrame* aFrame)
{
  // Fixed-pos frames are parented by the viewport frame or the page content frame.
  // We'll assume that printing/print preview don't have displayports for their
  // pages!
  nsIFrame* parent = aFrame->GetParent();
  if (!parent || parent->GetParent() ||
      aFrame->StyleDisplay()->mPosition != NS_STYLE_POSITION_FIXED) {
    return false;
  }
  return ViewportHasDisplayPort(aFrame->PresContext());
}

NS_DECLARE_FRAME_PROPERTY_SMALL_VALUE(ScrollbarThumbLayerized, bool)

/* static */ void
nsLayoutUtils::SetScrollbarThumbLayerization(nsIFrame* aThumbFrame, bool aLayerize)
{
  aThumbFrame->SetProperty(ScrollbarThumbLayerized(), aLayerize);
}

bool
nsLayoutUtils::IsScrollbarThumbLayerized(nsIFrame* aThumbFrame)
{
  return aThumbFrame->GetProperty(ScrollbarThumbLayerized());
}

// static
nsIScrollableFrame*
nsLayoutUtils::GetNearestScrollableFrameForDirection(nsIFrame* aFrame,
                                                     Direction aDirection)
{
  NS_ASSERTION(aFrame, "GetNearestScrollableFrameForDirection expects a non-null frame");
  for (nsIFrame* f = aFrame; f; f = nsLayoutUtils::GetCrossDocParentFrame(f)) {
    nsIScrollableFrame* scrollableFrame = do_QueryFrame(f);
    if (scrollableFrame) {
      ScrollbarStyles ss = scrollableFrame->GetScrollbarStyles();
      uint32_t directions = scrollableFrame->GetPerceivedScrollingDirections();
      if (aDirection == eVertical ?
          (ss.mVertical != NS_STYLE_OVERFLOW_HIDDEN &&
           (directions & nsIScrollableFrame::VERTICAL)) :
          (ss.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN &&
           (directions & nsIScrollableFrame::HORIZONTAL)))
        return scrollableFrame;
    }
  }
  return nullptr;
}

// static
nsIScrollableFrame*
nsLayoutUtils::GetNearestScrollableFrame(nsIFrame* aFrame, uint32_t aFlags)
{
  NS_ASSERTION(aFrame, "GetNearestScrollableFrame expects a non-null frame");
  for (nsIFrame* f = aFrame; f; f = (aFlags & SCROLLABLE_SAME_DOC) ?
       f->GetParent() : nsLayoutUtils::GetCrossDocParentFrame(f)) {
    nsIScrollableFrame* scrollableFrame = do_QueryFrame(f);
    if (scrollableFrame) {
      if (aFlags & SCROLLABLE_ONLY_ASYNC_SCROLLABLE) {
        if (scrollableFrame->WantAsyncScroll()) {
          return scrollableFrame;
        }
      } else {
        ScrollbarStyles ss = scrollableFrame->GetScrollbarStyles();
        if ((aFlags & SCROLLABLE_INCLUDE_HIDDEN) ||
            ss.mVertical != NS_STYLE_OVERFLOW_HIDDEN ||
            ss.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN) {
          return scrollableFrame;
        }
      }
      if (aFlags & SCROLLABLE_ALWAYS_MATCH_ROOT) {
        nsIPresShell* ps = f->PresContext()->PresShell();
        if (ps->GetRootScrollFrame() == f &&
            ps->GetDocument() && ps->GetDocument()->IsRootDisplayDocument()) {
          return scrollableFrame;
        }
      }
    }
    if ((aFlags & SCROLLABLE_FIXEDPOS_FINDS_ROOT) &&
        f->StyleDisplay()->mPosition == NS_STYLE_POSITION_FIXED &&
        nsLayoutUtils::IsReallyFixedPos(f)) {
      return f->PresContext()->PresShell()->GetRootScrollFrameAsScrollable();
    }
  }
  return nullptr;
}

// static
nsRect
nsLayoutUtils::GetScrolledRect(nsIFrame* aScrolledFrame,
                               const nsRect& aScrolledFrameOverflowArea,
                               const nsSize& aScrollPortSize,
                               uint8_t aDirection)
{
  WritingMode wm = aScrolledFrame->GetWritingMode();
  // Potentially override the frame's direction to use the direction found
  // by ScrollFrameHelper::GetScrolledFrameDir()
  wm.SetDirectionFromBidiLevel(aDirection == NS_STYLE_DIRECTION_RTL ? 1 : 0);

  nscoord x1 = aScrolledFrameOverflowArea.x,
          x2 = aScrolledFrameOverflowArea.XMost(),
          y1 = aScrolledFrameOverflowArea.y,
          y2 = aScrolledFrameOverflowArea.YMost();

  bool horizontal = !wm.IsVertical();

  // Clamp the horizontal start-edge (x1 or x2, depending whether the logical
  // axis that corresponds to horizontal progresses from L-R or R-L).
  // In horizontal writing mode, we need to check IsInlineReversed() to see
  // which side to clamp; in vertical mode, it depends on the block direction.
  if ((horizontal && !wm.IsInlineReversed()) || wm.IsVerticalLR()) {
    if (x1 < 0) {
      x1 = 0;
    }
  } else {
    if (x2 > aScrollPortSize.width) {
      x2 = aScrollPortSize.width;
    }
    // When the scrolled frame chooses a size larger than its available width
    // (because its padding alone is larger than the available width), we need
    // to keep the start-edge of the scroll frame anchored to the start-edge of
    // the scrollport.
    // When the scrolled frame is RTL, this means moving it in our left-based
    // coordinate system, so we need to compensate for its extra width here by
    // effectively repositioning the frame.
    nscoord extraWidth =
      std::max(0, aScrolledFrame->GetSize().width - aScrollPortSize.width);
    x2 += extraWidth;
  }

  // Similarly, clamp the vertical start-edge.
  // In horizontal writing mode, the block direction is always top-to-bottom;
  // in vertical writing mode, we need to check IsInlineReversed().
  if (horizontal || !wm.IsInlineReversed()) {
    if (y1 < 0) {
      y1 = 0;
    }
  } else {
    if (y2 > aScrollPortSize.height) {
      y2 = aScrollPortSize.height;
    }
    nscoord extraHeight =
      std::max(0, aScrolledFrame->GetSize().height - aScrollPortSize.height);
    y2 += extraHeight;
  }

  return nsRect(x1, y1, x2 - x1, y2 - y1);
}

//static
bool
nsLayoutUtils::HasPseudoStyle(nsIContent* aContent,
                              nsStyleContext* aStyleContext,
                              CSSPseudoElementType aPseudoElement,
                              nsPresContext* aPresContext)
{
  NS_PRECONDITION(aPresContext, "Must have a prescontext");

  RefPtr<nsStyleContext> pseudoContext;
  if (aContent) {
    pseudoContext = aPresContext->StyleSet()->
      ProbePseudoElementStyle(aContent->AsElement(), aPseudoElement,
                              aStyleContext);
  }
  return pseudoContext != nullptr;
}

nsPoint
nsLayoutUtils::GetDOMEventCoordinatesRelativeTo(nsIDOMEvent* aDOMEvent, nsIFrame* aFrame)
{
  if (!aDOMEvent)
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  WidgetEvent* event = aDOMEvent->WidgetEventPtr();
  if (!event)
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  return GetEventCoordinatesRelativeTo(event, aFrame);
}

nsPoint
nsLayoutUtils::GetEventCoordinatesRelativeTo(const WidgetEvent* aEvent,
                                             nsIFrame* aFrame)
{
  if (!aEvent || (aEvent->mClass != eMouseEventClass &&
                  aEvent->mClass != eMouseScrollEventClass &&
                  aEvent->mClass != eWheelEventClass &&
                  aEvent->mClass != eDragEventClass &&
                  aEvent->mClass != eSimpleGestureEventClass &&
                  aEvent->mClass != ePointerEventClass &&
                  aEvent->mClass != eGestureNotifyEventClass &&
                  aEvent->mClass != eTouchEventClass &&
                  aEvent->mClass != eQueryContentEventClass))
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);

  return GetEventCoordinatesRelativeTo(aEvent,
           aEvent->AsGUIEvent()->mRefPoint,
           aFrame);
}

nsPoint
nsLayoutUtils::GetEventCoordinatesRelativeTo(const WidgetEvent* aEvent,
                                             const LayoutDeviceIntPoint& aPoint,
                                             nsIFrame* aFrame)
{
  if (!aFrame) {
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  }

  nsIWidget* widget = aEvent->AsGUIEvent()->mWidget;
  if (!widget) {
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  }

  return GetEventCoordinatesRelativeTo(widget, aPoint, aFrame);
}

nsPoint
nsLayoutUtils::GetEventCoordinatesRelativeTo(nsIWidget* aWidget,
                                             const LayoutDeviceIntPoint& aPoint,
                                             nsIFrame* aFrame)
{
  if (!aFrame || !aWidget) {
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  }

  nsView* view = aFrame->GetView();
  if (view) {
    nsIWidget* frameWidget = view->GetWidget();
    if (frameWidget && frameWidget == aWidget) {
      // Special case this cause it happens a lot.
      // This also fixes bug 664707, events in the extra-special case of select
      // dropdown popups that are transformed.
      nsPresContext* presContext = aFrame->PresContext();
      nsPoint pt(presContext->DevPixelsToAppUnits(aPoint.x),
                 presContext->DevPixelsToAppUnits(aPoint.y));
      pt = pt - view->ViewToWidgetOffset();
      pt = pt.RemoveResolution(GetCurrentAPZResolutionScale(presContext->PresShell()));
      return pt;
    }
  }

  /* If we walk up the frame tree and discover that any of the frames are
   * transformed, we need to do extra work to convert from the global
   * space to the local space.
   */
  nsIFrame* rootFrame = aFrame;
  bool transformFound = false;
  for (nsIFrame* f = aFrame; f; f = GetCrossDocParentFrame(f)) {
    if (f->IsTransformed()) {
      transformFound = true;
    }

    rootFrame = f;
  }

  nsView* rootView = rootFrame->GetView();
  if (!rootView) {
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  }

  nsPoint widgetToView = TranslateWidgetToView(rootFrame->PresContext(),
                                               aWidget, aPoint, rootView);

  if (widgetToView == nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE)) {
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  }

  // Convert from root document app units to app units of the document aFrame
  // is in.
  int32_t rootAPD = rootFrame->PresContext()->AppUnitsPerDevPixel();
  int32_t localAPD = aFrame->PresContext()->AppUnitsPerDevPixel();
  widgetToView = widgetToView.ScaleToOtherAppUnits(rootAPD, localAPD);
  nsIPresShell* shell = aFrame->PresContext()->PresShell();

  // XXX Bug 1224748 - Update nsLayoutUtils functions to correctly handle nsPresShell resolution
  widgetToView = widgetToView.RemoveResolution(GetCurrentAPZResolutionScale(shell));

  /* If we encountered a transform, we can't do simple arithmetic to figure
   * out how to convert back to aFrame's coordinates and must use the CTM.
   */
  if (transformFound || nsSVGUtils::IsInSVGTextSubtree(aFrame)) {
    return TransformRootPointToFrame(aFrame, widgetToView);
  }

  /* Otherwise, all coordinate systems are translations of one another,
   * so we can just subtract out the difference.
   */
  return widgetToView - aFrame->GetOffsetToCrossDoc(rootFrame);
}

nsIFrame*
nsLayoutUtils::GetPopupFrameForEventCoordinates(nsPresContext* aPresContext,
                                                const WidgetEvent* aEvent)
{
#ifdef MOZ_XUL
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (!pm) {
    return nullptr;
  }
  nsTArray<nsIFrame*> popups;
  pm->GetVisiblePopups(popups);
  uint32_t i;
  // Search from top to bottom
  for (i = 0; i < popups.Length(); i++) {
    nsIFrame* popup = popups[i];
    if (popup->PresContext()->GetRootPresContext() == aPresContext &&
        popup->GetScrollableOverflowRect().Contains(
          GetEventCoordinatesRelativeTo(aEvent, popup))) {
      return popup;
    }
  }
#endif
  return nullptr;
}

static void ConstrainToCoordValues(float& aStart, float& aSize)
{
  MOZ_ASSERT(aSize >= 0);

  // Here we try to make sure that the resulting nsRect will continue to cover
  // as much of the area that was covered by the original gfx Rect as possible.

  // We clamp the bounds of the rect to {nscoord_MIN,nscoord_MAX} since
  // nsRect::X/Y() and nsRect::XMost/YMost() can't return values outwith this
  // range:
  float end = aStart + aSize;
  aStart = clamped(aStart, float(nscoord_MIN), float(nscoord_MAX));
  end = clamped(end, float(nscoord_MIN), float(nscoord_MAX));

  aSize = end - aStart;

  // We must also clamp aSize to {0,nscoord_MAX} since nsRect::Width/Height()
  // can't return a value greater than nscoord_MAX. If aSize is greater than
  // nscoord_MAX then we reduce it to nscoord_MAX while keeping the rect
  // centered:
  if (aSize > nscoord_MAX) {
    float excess = aSize - nscoord_MAX;
    excess /= 2;
    aStart += excess;
    aSize = nscoord_MAX;
  }
}

/**
 * Given a gfxFloat, constrains its value to be between nscoord_MIN and nscoord_MAX.
 *
 * @param aVal The value to constrain (in/out)
 */
static void ConstrainToCoordValues(gfxFloat& aVal)
{
  if (aVal <= nscoord_MIN)
    aVal = nscoord_MIN;
  else if (aVal >= nscoord_MAX)
    aVal = nscoord_MAX;
}

static void ConstrainToCoordValues(gfxFloat& aStart, gfxFloat& aSize)
{
  gfxFloat max = aStart + aSize;

  // Clamp the end points to within nscoord range
  ConstrainToCoordValues(aStart);
  ConstrainToCoordValues(max);

  aSize = max - aStart;
  // If the width if still greater than the max nscoord, then bring both
  // endpoints in by the same amount until it fits.
  if (aSize > nscoord_MAX) {
    gfxFloat excess = aSize - nscoord_MAX;
    excess /= 2;

    aStart += excess;
    aSize = nscoord_MAX;
  } else if (aSize < nscoord_MIN) {
    gfxFloat excess = aSize - nscoord_MIN;
    excess /= 2;

    aStart -= excess;
    aSize = nscoord_MIN;
  }
}

nsRect
nsLayoutUtils::RoundGfxRectToAppRect(const Rect &aRect, float aFactor)
{
  /* Get a new Rect whose units are app units by scaling by the specified factor. */
  Rect scaledRect = aRect;
  scaledRect.ScaleRoundOut(aFactor);

  /* We now need to constrain our results to the max and min values for coords. */
  ConstrainToCoordValues(scaledRect.x, scaledRect.width);
  ConstrainToCoordValues(scaledRect.y, scaledRect.height);

  /* Now typecast everything back.  This is guaranteed to be safe. */
  return nsRect(nscoord(scaledRect.X()), nscoord(scaledRect.Y()),
                nscoord(scaledRect.Width()), nscoord(scaledRect.Height()));
}

nsRect
nsLayoutUtils::RoundGfxRectToAppRect(const gfxRect &aRect, float aFactor)
{
  /* Get a new gfxRect whose units are app units by scaling by the specified factor. */
  gfxRect scaledRect = aRect;
  scaledRect.ScaleRoundOut(aFactor);

  /* We now need to constrain our results to the max and min values for coords. */
  ConstrainToCoordValues(scaledRect.x, scaledRect.width);
  ConstrainToCoordValues(scaledRect.y, scaledRect.height);

  /* Now typecast everything back.  This is guaranteed to be safe. */
  return nsRect(nscoord(scaledRect.X()), nscoord(scaledRect.Y()),
                nscoord(scaledRect.Width()), nscoord(scaledRect.Height()));
}


nsRegion
nsLayoutUtils::RoundedRectIntersectRect(const nsRect& aRoundedRect,
                                        const nscoord aRadii[8],
                                        const nsRect& aContainedRect)
{
  // rectFullHeight and rectFullWidth together will approximately contain
  // the total area of the frame minus the rounded corners.
  nsRect rectFullHeight = aRoundedRect;
  nscoord xDiff = std::max(aRadii[eCornerTopLeftX], aRadii[eCornerBottomLeftX]);
  rectFullHeight.x += xDiff;
  rectFullHeight.width -= std::max(aRadii[eCornerTopRightX],
                                   aRadii[eCornerBottomRightX]) + xDiff;
  nsRect r1;
  r1.IntersectRect(rectFullHeight, aContainedRect);

  nsRect rectFullWidth = aRoundedRect;
  nscoord yDiff = std::max(aRadii[eCornerTopLeftY], aRadii[eCornerTopRightY]);
  rectFullWidth.y += yDiff;
  rectFullWidth.height -= std::max(aRadii[eCornerBottomLeftY],
                                   aRadii[eCornerBottomRightY]) + yDiff;
  nsRect r2;
  r2.IntersectRect(rectFullWidth, aContainedRect);

  nsRegion result;
  result.Or(r1, r2);
  return result;
}

nsIntRegion
nsLayoutUtils::RoundedRectIntersectIntRect(const nsIntRect& aRoundedRect,
                                           const RectCornerRadii& aCornerRadii,
                                           const nsIntRect& aContainedRect)
{
  // rectFullHeight and rectFullWidth together will approximately contain
  // the total area of the frame minus the rounded corners.
  nsIntRect rectFullHeight = aRoundedRect;
  uint32_t xDiff = std::max(aCornerRadii.TopLeft().width,
                            aCornerRadii.BottomLeft().width);
  rectFullHeight.x += xDiff;
  rectFullHeight.width -= std::max(aCornerRadii.TopRight().width,
                                   aCornerRadii.BottomRight().width) + xDiff;
  nsIntRect r1;
  r1.IntersectRect(rectFullHeight, aContainedRect);

  nsIntRect rectFullWidth = aRoundedRect;
  uint32_t yDiff = std::max(aCornerRadii.TopLeft().height,
                            aCornerRadii.TopRight().height);
  rectFullWidth.y += yDiff;
  rectFullWidth.height -= std::max(aCornerRadii.BottomLeft().height,
                                   aCornerRadii.BottomRight().height) + yDiff;
  nsIntRect r2;
  r2.IntersectRect(rectFullWidth, aContainedRect);

  nsIntRegion result;
  result.Or(r1, r2);
  return result;
}

// Helper for RoundedRectIntersectsRect.
static bool
CheckCorner(nscoord aXOffset, nscoord aYOffset,
            nscoord aXRadius, nscoord aYRadius)
{
  MOZ_ASSERT(aXOffset > 0 && aYOffset > 0,
             "must not pass nonpositives to CheckCorner");
  MOZ_ASSERT(aXRadius >= 0 && aYRadius >= 0,
             "must not pass negatives to CheckCorner");

  // Avoid floating point math unless we're either (1) within the
  // quarter-ellipse area at the rounded corner or (2) outside the
  // rounding.
  if (aXOffset >= aXRadius || aYOffset >= aYRadius)
    return true;

  // Convert coordinates to a unit circle with (0,0) as the center of
  // curvature, and see if we're inside the circle or outside.
  float scaledX = float(aXRadius - aXOffset) / float(aXRadius);
  float scaledY = float(aYRadius - aYOffset) / float(aYRadius);
  return scaledX * scaledX + scaledY * scaledY < 1.0f;
}

bool
nsLayoutUtils::RoundedRectIntersectsRect(const nsRect& aRoundedRect,
                                         const nscoord aRadii[8],
                                         const nsRect& aTestRect)
{
  if (!aTestRect.Intersects(aRoundedRect))
    return false;

  // distances from this edge of aRoundedRect to opposite edge of aTestRect,
  // which we know are positive due to the Intersects check above.
  nsMargin insets;
  insets.top = aTestRect.YMost() - aRoundedRect.y;
  insets.right = aRoundedRect.XMost() - aTestRect.x;
  insets.bottom = aRoundedRect.YMost() - aTestRect.y;
  insets.left = aTestRect.XMost() - aRoundedRect.x;

  // Check whether the bottom-right corner of aTestRect is inside the
  // top left corner of aBounds when rounded by aRadii, etc.  If any
  // corner is not, then fail; otherwise succeed.
  return CheckCorner(insets.left, insets.top,
                     aRadii[eCornerTopLeftX],
                     aRadii[eCornerTopLeftY]) &&
         CheckCorner(insets.right, insets.top,
                     aRadii[eCornerTopRightX],
                     aRadii[eCornerTopRightY]) &&
         CheckCorner(insets.right, insets.bottom,
                     aRadii[eCornerBottomRightX],
                     aRadii[eCornerBottomRightY]) &&
         CheckCorner(insets.left, insets.bottom,
                     aRadii[eCornerBottomLeftX],
                     aRadii[eCornerBottomLeftY]);
}

nsRect
nsLayoutUtils::MatrixTransformRect(const nsRect &aBounds,
                                   const Matrix4x4 &aMatrix, float aFactor)
{
  RectDouble image = RectDouble(NSAppUnitsToDoublePixels(aBounds.x, aFactor),
                                NSAppUnitsToDoublePixels(aBounds.y, aFactor),
                                NSAppUnitsToDoublePixels(aBounds.width, aFactor),
                                NSAppUnitsToDoublePixels(aBounds.height, aFactor));

  RectDouble maxBounds = RectDouble(double(nscoord_MIN) / aFactor * 0.5,
                                    double(nscoord_MIN) / aFactor * 0.5,
                                    double(nscoord_MAX) / aFactor,
                                    double(nscoord_MAX) / aFactor);

  image = aMatrix.TransformAndClipBounds(image, maxBounds);

  return RoundGfxRectToAppRect(ThebesRect(image), aFactor);
}

nsPoint
nsLayoutUtils::MatrixTransformPoint(const nsPoint &aPoint,
                                    const Matrix4x4 &aMatrix, float aFactor)
{
  gfxPoint image = gfxPoint(NSAppUnitsToFloatPixels(aPoint.x, aFactor),
                            NSAppUnitsToFloatPixels(aPoint.y, aFactor));
  image.Transform(aMatrix);
  return nsPoint(NSFloatPixelsToAppUnits(float(image.x), aFactor),
                 NSFloatPixelsToAppUnits(float(image.y), aFactor));
}

void
nsLayoutUtils::PostTranslate(Matrix4x4& aTransform, const nsPoint& aOrigin, float aAppUnitsPerPixel, bool aRounded)
{
  Point3D gfxOrigin =
    Point3D(NSAppUnitsToFloatPixels(aOrigin.x, aAppUnitsPerPixel),
            NSAppUnitsToFloatPixels(aOrigin.y, aAppUnitsPerPixel),
            0.0f);
  if (aRounded) {
    gfxOrigin.x = NS_round(gfxOrigin.x);
    gfxOrigin.y = NS_round(gfxOrigin.y);
  }
  aTransform.PostTranslate(gfxOrigin);
}

Matrix4x4
nsLayoutUtils::GetTransformToAncestor(nsIFrame *aFrame,
                                      const nsIFrame *aAncestor,
                                      bool aInCSSUnits)
{
  nsIFrame* parent;
  Matrix4x4 ctm;
  if (aFrame == aAncestor) {
    return ctm;
  }
  ctm = aFrame->GetTransformMatrix(aAncestor, &parent, aInCSSUnits);
  while (parent && parent != aAncestor) {
    if (!parent->Extend3DContext()) {
      ctm.ProjectTo2D();
    }
    ctm = ctm * parent->GetTransformMatrix(aAncestor, &parent, aInCSSUnits);
  }
  return ctm;
}

gfxSize
nsLayoutUtils::GetTransformToAncestorScale(nsIFrame* aFrame)
{
  Matrix4x4 transform = GetTransformToAncestor(aFrame,
      nsLayoutUtils::GetDisplayRootFrame(aFrame));
  Matrix transform2D;
  if (transform.Is2D(&transform2D)) {
    return ThebesMatrix(transform2D).ScaleFactors(true);
  }
  return gfxSize(1, 1);
}

static Matrix4x4
GetTransformToAncestorExcludingAnimated(nsIFrame* aFrame,
                                        const nsIFrame* aAncestor)
{
  nsIFrame* parent;
  Matrix4x4 ctm;
  if (aFrame == aAncestor) {
    return ctm;
  }
  if (ActiveLayerTracker::IsScaleSubjectToAnimation(aFrame)) {
    return ctm;
  }
  ctm = aFrame->GetTransformMatrix(aAncestor, &parent);
  while (parent && parent != aAncestor) {
    if (ActiveLayerTracker::IsScaleSubjectToAnimation(parent)) {
      return Matrix4x4();
    }
    if (!parent->Extend3DContext()) {
      ctm.ProjectTo2D();
    }
    ctm = ctm * parent->GetTransformMatrix(aAncestor, &parent);
  }
  return ctm;
}

gfxSize
nsLayoutUtils::GetTransformToAncestorScaleExcludingAnimated(nsIFrame* aFrame)
{
  Matrix4x4 transform = GetTransformToAncestorExcludingAnimated(aFrame,
      nsLayoutUtils::GetDisplayRootFrame(aFrame));
  Matrix transform2D;
  if (transform.Is2D(&transform2D)) {
    return ThebesMatrix(transform2D).ScaleFactors(true);
  }
  return gfxSize(1, 1);
}

nsIFrame*
nsLayoutUtils::FindNearestCommonAncestorFrame(nsIFrame* aFrame1, nsIFrame* aFrame2)
{
  AutoTArray<nsIFrame*,100> ancestors1;
  AutoTArray<nsIFrame*,100> ancestors2;
  nsIFrame* commonAncestor = nullptr;
  if (aFrame1->PresContext() == aFrame2->PresContext()) {
    commonAncestor = aFrame1->PresContext()->PresShell()->GetRootFrame();
  }
  for (nsIFrame* f = aFrame1; f != commonAncestor;
       f = nsLayoutUtils::GetCrossDocParentFrame(f)) {
    ancestors1.AppendElement(f);
  }
  for (nsIFrame* f = aFrame2; f != commonAncestor;
       f = nsLayoutUtils::GetCrossDocParentFrame(f)) {
    ancestors2.AppendElement(f);
  }
  uint32_t minLengths = std::min(ancestors1.Length(), ancestors2.Length());
  for (uint32_t i = 1; i <= minLengths; ++i) {
    if (ancestors1[ancestors1.Length() - i] == ancestors2[ancestors2.Length() - i]) {
      commonAncestor = ancestors1[ancestors1.Length() - i];
    } else {
      break;
    }
  }
  return commonAncestor;
}

nsLayoutUtils::TransformResult
nsLayoutUtils::TransformPoints(nsIFrame* aFromFrame, nsIFrame* aToFrame,
                               uint32_t aPointCount, CSSPoint* aPoints)
{
  nsIFrame* nearestCommonAncestor = FindNearestCommonAncestorFrame(aFromFrame, aToFrame);
  if (!nearestCommonAncestor) {
    return NO_COMMON_ANCESTOR;
  }
  Matrix4x4 downToDest = GetTransformToAncestor(aToFrame, nearestCommonAncestor);
  if (downToDest.IsSingular()) {
    return NONINVERTIBLE_TRANSFORM;
  }
  downToDest.Invert();
  Matrix4x4 upToAncestor = GetTransformToAncestor(aFromFrame, nearestCommonAncestor);
  CSSToLayoutDeviceScale devPixelsPerCSSPixelFromFrame =
      aFromFrame->PresContext()->CSSToDevPixelScale();
  CSSToLayoutDeviceScale devPixelsPerCSSPixelToFrame =
      aToFrame->PresContext()->CSSToDevPixelScale();
  for (uint32_t i = 0; i < aPointCount; ++i) {
    LayoutDevicePoint devPixels = aPoints[i] * devPixelsPerCSSPixelFromFrame;
    // What should the behaviour be if some of the points aren't invertible
    // and others are? Just assume all points are for now.
    Point toDevPixels = downToDest.ProjectPoint(
        (upToAncestor.TransformPoint(Point(devPixels.x, devPixels.y)))).As2DPoint();
    // Divide here so that when the devPixelsPerCSSPixels are the same, we get the correct
    // answer instead of some inaccuracy multiplying a number by its reciprocal.
    aPoints[i] = LayoutDevicePoint(toDevPixels.x, toDevPixels.y) /
        devPixelsPerCSSPixelToFrame;
  }
  return TRANSFORM_SUCCEEDED;
}

nsLayoutUtils::TransformResult
nsLayoutUtils::TransformPoint(nsIFrame* aFromFrame, nsIFrame* aToFrame,
                              nsPoint& aPoint)
{
  nsIFrame* nearestCommonAncestor = FindNearestCommonAncestorFrame(aFromFrame, aToFrame);
  if (!nearestCommonAncestor) {
    return NO_COMMON_ANCESTOR;
  }
  Matrix4x4 downToDest = GetTransformToAncestor(aToFrame, nearestCommonAncestor);
  if (downToDest.IsSingular()) {
    return NONINVERTIBLE_TRANSFORM;
  }
  downToDest.Invert();
  Matrix4x4 upToAncestor = GetTransformToAncestor(aFromFrame, nearestCommonAncestor);

  float devPixelsPerAppUnitFromFrame =
    1.0f / aFromFrame->PresContext()->AppUnitsPerDevPixel();
  float devPixelsPerAppUnitToFrame =
    1.0f / aToFrame->PresContext()->AppUnitsPerDevPixel();
  Point4D toDevPixels = downToDest.ProjectPoint(
      upToAncestor.TransformPoint(Point(aPoint.x * devPixelsPerAppUnitFromFrame,
                                        aPoint.y * devPixelsPerAppUnitFromFrame)));
  if (!toDevPixels.HasPositiveWCoord()) {
    // Not strictly true, but we failed to get a valid point in this
    // coordinate space.
    return NONINVERTIBLE_TRANSFORM;
  }
  aPoint.x = NSToCoordRound(toDevPixels.x / devPixelsPerAppUnitToFrame);
  aPoint.y = NSToCoordRound(toDevPixels.y / devPixelsPerAppUnitToFrame);
  return TRANSFORM_SUCCEEDED;
}

nsLayoutUtils::TransformResult
nsLayoutUtils::TransformRect(nsIFrame* aFromFrame, nsIFrame* aToFrame,
                             nsRect& aRect)
{
  nsIFrame* nearestCommonAncestor = FindNearestCommonAncestorFrame(aFromFrame, aToFrame);
  if (!nearestCommonAncestor) {
    return NO_COMMON_ANCESTOR;
  }
  Matrix4x4 downToDest = GetTransformToAncestor(aToFrame, nearestCommonAncestor);
  if (downToDest.IsSingular()) {
    return NONINVERTIBLE_TRANSFORM;
  }
  downToDest.Invert();
  Matrix4x4 upToAncestor = GetTransformToAncestor(aFromFrame, nearestCommonAncestor);

  float devPixelsPerAppUnitFromFrame =
    1.0f / aFromFrame->PresContext()->AppUnitsPerDevPixel();
  float devPixelsPerAppUnitToFrame =
    1.0f / aToFrame->PresContext()->AppUnitsPerDevPixel();
  gfx::Rect toDevPixels = downToDest.ProjectRectBounds(
    upToAncestor.ProjectRectBounds(
      gfx::Rect(aRect.x * devPixelsPerAppUnitFromFrame,
                aRect.y * devPixelsPerAppUnitFromFrame,
                aRect.width * devPixelsPerAppUnitFromFrame,
                aRect.height * devPixelsPerAppUnitFromFrame),
      Rect(-std::numeric_limits<Float>::max() * 0.5f,
           -std::numeric_limits<Float>::max() * 0.5f,
           std::numeric_limits<Float>::max(),
           std::numeric_limits<Float>::max())),
    Rect(-std::numeric_limits<Float>::max() * devPixelsPerAppUnitFromFrame * 0.5f,
         -std::numeric_limits<Float>::max() * devPixelsPerAppUnitFromFrame * 0.5f,
         std::numeric_limits<Float>::max() * devPixelsPerAppUnitFromFrame,
         std::numeric_limits<Float>::max() * devPixelsPerAppUnitFromFrame));
  aRect.x = NSToCoordRound(toDevPixels.x / devPixelsPerAppUnitToFrame);
  aRect.y = NSToCoordRound(toDevPixels.y / devPixelsPerAppUnitToFrame);
  aRect.width = NSToCoordRound(toDevPixels.width / devPixelsPerAppUnitToFrame);
  aRect.height = NSToCoordRound(toDevPixels.height / devPixelsPerAppUnitToFrame);
  return TRANSFORM_SUCCEEDED;
}

nsRect
nsLayoutUtils::GetRectRelativeToFrame(Element* aElement, nsIFrame* aFrame)
{
  if (!aElement || !aFrame) {
    return nsRect();
  }

  nsIFrame* frame = aElement->GetPrimaryFrame();
  if (!frame) {
    return nsRect();
  }

  nsRect rect = frame->GetRectRelativeToSelf();
  nsLayoutUtils::TransformResult rv =
    nsLayoutUtils::TransformRect(frame, aFrame, rect);
  if (rv != nsLayoutUtils::TRANSFORM_SUCCEEDED) {
    return nsRect();
  }

  return rect;
}

bool
nsLayoutUtils::ContainsPoint(const nsRect& aRect, const nsPoint& aPoint,
                             nscoord aInflateSize)
{
  nsRect rect = aRect;
  rect.Inflate(aInflateSize);
  return rect.Contains(aPoint);
}

nsRect
nsLayoutUtils::ClampRectToScrollFrames(nsIFrame* aFrame, const nsRect& aRect)
{
  nsIFrame* closestScrollFrame =
    nsLayoutUtils::GetClosestFrameOfType(aFrame, LayoutFrameType::Scroll);

  nsRect resultRect = aRect;

  while (closestScrollFrame) {
    nsIScrollableFrame* sf = do_QueryFrame(closestScrollFrame);

    nsRect scrollPortRect = sf->GetScrollPortRect();
    nsLayoutUtils::TransformRect(closestScrollFrame, aFrame, scrollPortRect);

    resultRect = resultRect.Intersect(scrollPortRect);

    // Check whether aRect is visible in the scroll frame or not.
    if (resultRect.IsEmpty()) {
      break;
    }

    // Get next ancestor scroll frame.
    closestScrollFrame = nsLayoutUtils::GetClosestFrameOfType(
      closestScrollFrame->GetParent(), LayoutFrameType::Scroll);
  }

  return resultRect;
}

bool
nsLayoutUtils::GetLayerTransformForFrame(nsIFrame* aFrame,
                                         Matrix4x4* aTransform)
{
  // FIXME/bug 796690: we can sometimes compute a transform in these
  // cases, it just increases complexity considerably.  Punt for now.
  if (aFrame->Extend3DContext() || aFrame->HasTransformGetter()) {
    return false;
  }

  nsIFrame* root = nsLayoutUtils::GetDisplayRootFrame(aFrame);
  if (root->HasAnyStateBits(NS_FRAME_UPDATE_LAYER_TREE)) {
    // Content may have been invalidated, so we can't reliably compute
    // the "layer transform" in general.
    return false;
  }
  // If the caller doesn't care about the value, early-return to skip
  // overhead below.
  if (!aTransform) {
    return true;
  }

  nsDisplayListBuilder builder(root,
                               nsDisplayListBuilderMode::TRANSFORM_COMPUTATION,
                               false/*don't build caret*/);
  nsDisplayList list;
  nsDisplayTransform* item =
    new (&builder) nsDisplayTransform(&builder, aFrame, &list, nsRect());

  *aTransform = item->GetTransform();
  item->~nsDisplayTransform();

  return true;
}

static bool
TransformGfxPointFromAncestor(nsIFrame *aFrame,
                              const Point &aPoint,
                              nsIFrame *aAncestor,
                              Point* aOut)
{
  Matrix4x4 ctm = nsLayoutUtils::GetTransformToAncestor(aFrame, aAncestor);
  ctm.Invert();
  Point4D point = ctm.ProjectPoint(aPoint);
  if (!point.HasPositiveWCoord()) {
    return false;
  }
  *aOut = point.As2DPoint();
  return true;
}

static Rect
TransformGfxRectToAncestor(nsIFrame *aFrame,
                           const Rect &aRect,
                           const nsIFrame *aAncestor,
                           bool* aPreservesAxisAlignedRectangles = nullptr,
                           Maybe<Matrix4x4>* aMatrixCache = nullptr)
{
  Matrix4x4 ctm;
  if (aMatrixCache && *aMatrixCache) {
    // We are given a matrix to use, so use it
    ctm = aMatrixCache->value();
  } else {
    // Else, compute it
    ctm = nsLayoutUtils::GetTransformToAncestor(aFrame, aAncestor);
    if (aMatrixCache) {
      // and put it in the cache, if provided
      *aMatrixCache = Some(ctm);
    }
  }
  // Fill out the axis-alignment flag
  if (aPreservesAxisAlignedRectangles) {
    Matrix matrix2d;
    *aPreservesAxisAlignedRectangles =
      ctm.Is2D(&matrix2d) && matrix2d.PreservesAxisAlignedRectangles();
  }
  Rect maxBounds = Rect(-std::numeric_limits<float>::max() * 0.5,
                        -std::numeric_limits<float>::max() * 0.5,
                        std::numeric_limits<float>::max(),
                        std::numeric_limits<float>::max());
  return ctm.TransformAndClipBounds(aRect, maxBounds);
}

static SVGTextFrame*
GetContainingSVGTextFrame(nsIFrame* aFrame)
{
  if (!nsSVGUtils::IsInSVGTextSubtree(aFrame)) {
    return nullptr;
  }

  return static_cast<SVGTextFrame*>(nsLayoutUtils::GetClosestFrameOfType(
    aFrame->GetParent(), LayoutFrameType::SVGText));
}

nsPoint
nsLayoutUtils::TransformAncestorPointToFrame(nsIFrame* aFrame,
                                             const nsPoint& aPoint,
                                             nsIFrame* aAncestor)
{
    SVGTextFrame* text = GetContainingSVGTextFrame(aFrame);

    float factor = aFrame->PresContext()->AppUnitsPerDevPixel();
    Point result(NSAppUnitsToFloatPixels(aPoint.x, factor),
                 NSAppUnitsToFloatPixels(aPoint.y, factor));

    if (text) {
        if (!TransformGfxPointFromAncestor(text, result, aAncestor, &result)) {
            return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
        }
        result = text->TransformFramePointToTextChild(result, aFrame);
    } else {
        if (!TransformGfxPointFromAncestor(aFrame, result, nullptr, &result)) {
            return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
        }
    }

    return nsPoint(NSFloatPixelsToAppUnits(float(result.x), factor),
                   NSFloatPixelsToAppUnits(float(result.y), factor));
}

nsRect
nsLayoutUtils::TransformFrameRectToAncestor(nsIFrame* aFrame,
                                            const nsRect& aRect,
                                            const nsIFrame* aAncestor,
                                            bool* aPreservesAxisAlignedRectangles /* = nullptr */,
                                            Maybe<Matrix4x4>* aMatrixCache /* = nullptr */)
{
  SVGTextFrame* text = GetContainingSVGTextFrame(aFrame);

  float srcAppUnitsPerDevPixel = aFrame->PresContext()->AppUnitsPerDevPixel();
  Rect result;

  if (text) {
    result = ToRect(text->TransformFrameRectFromTextChild(aRect, aFrame));
    result = TransformGfxRectToAncestor(text, result, aAncestor, nullptr, aMatrixCache);
    // TransformFrameRectFromTextChild could involve any kind of transform, we
    // could drill down into it to get an answer out of it but we don't yet.
    if (aPreservesAxisAlignedRectangles)
      *aPreservesAxisAlignedRectangles = false;
  } else {
    result = Rect(NSAppUnitsToFloatPixels(aRect.x, srcAppUnitsPerDevPixel),
                  NSAppUnitsToFloatPixels(aRect.y, srcAppUnitsPerDevPixel),
                  NSAppUnitsToFloatPixels(aRect.width, srcAppUnitsPerDevPixel),
                  NSAppUnitsToFloatPixels(aRect.height, srcAppUnitsPerDevPixel));
    result = TransformGfxRectToAncestor(aFrame, result, aAncestor, aPreservesAxisAlignedRectangles, aMatrixCache);
  }

  float destAppUnitsPerDevPixel = aAncestor->PresContext()->AppUnitsPerDevPixel();
  return nsRect(NSFloatPixelsToAppUnits(float(result.x), destAppUnitsPerDevPixel),
                NSFloatPixelsToAppUnits(float(result.y), destAppUnitsPerDevPixel),
                NSFloatPixelsToAppUnits(float(result.width), destAppUnitsPerDevPixel),
                NSFloatPixelsToAppUnits(float(result.height), destAppUnitsPerDevPixel));
}

static LayoutDeviceIntPoint GetWidgetOffset(nsIWidget* aWidget, nsIWidget*& aRootWidget) {
  LayoutDeviceIntPoint offset(0, 0);
  while ((aWidget->WindowType() == eWindowType_child ||
          aWidget->IsPlugin())) {
    nsIWidget* parent = aWidget->GetParent();
    if (!parent) {
      break;
    }
    LayoutDeviceIntRect bounds = aWidget->GetBounds();
    offset += bounds.TopLeft();
    aWidget = parent;
  }
  aRootWidget = aWidget;
  return offset;
}

static LayoutDeviceIntPoint
WidgetToWidgetOffset(nsIWidget* aFrom, nsIWidget* aTo) {
  nsIWidget* fromRoot;
  LayoutDeviceIntPoint fromOffset = GetWidgetOffset(aFrom, fromRoot);
  nsIWidget* toRoot;
  LayoutDeviceIntPoint toOffset = GetWidgetOffset(aTo, toRoot);

  if (fromRoot == toRoot) {
    return fromOffset - toOffset;
  }
  return aFrom->WidgetToScreenOffset() - aTo->WidgetToScreenOffset();
}

nsPoint
nsLayoutUtils::TranslateWidgetToView(nsPresContext* aPresContext,
                                     nsIWidget* aWidget, const LayoutDeviceIntPoint& aPt,
                                     nsView* aView)
{
  nsPoint viewOffset;
  nsIWidget* viewWidget = aView->GetNearestWidget(&viewOffset);
  if (!viewWidget) {
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  }

  LayoutDeviceIntPoint widgetPoint = aPt + WidgetToWidgetOffset(aWidget, viewWidget);
  nsPoint widgetAppUnits(aPresContext->DevPixelsToAppUnits(widgetPoint.x),
                         aPresContext->DevPixelsToAppUnits(widgetPoint.y));
  return widgetAppUnits - viewOffset;
}

LayoutDeviceIntPoint
nsLayoutUtils::TranslateViewToWidget(nsPresContext* aPresContext,
                                     nsView* aView, nsPoint aPt,
                                     nsIWidget* aWidget)
{
  nsPoint viewOffset;
  nsIWidget* viewWidget = aView->GetNearestWidget(&viewOffset);
  if (!viewWidget) {
    return LayoutDeviceIntPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  }

  nsPoint pt = (aPt +
  viewOffset).ApplyResolution(GetCurrentAPZResolutionScale(aPresContext->PresShell()));
  LayoutDeviceIntPoint relativeToViewWidget(aPresContext->AppUnitsToDevPixels(pt.x),
                                            aPresContext->AppUnitsToDevPixels(pt.y));
  return relativeToViewWidget + WidgetToWidgetOffset(viewWidget, aWidget);
}

// Combine aNewBreakType with aOrigBreakType, but limit the break types
// to StyleClear::Left, Right, Both.
StyleClear
nsLayoutUtils::CombineBreakType(StyleClear aOrigBreakType,
                                StyleClear aNewBreakType)
{
  StyleClear breakType = aOrigBreakType;
  switch(breakType) {
    case StyleClear::Left:
      if (StyleClear::Right == aNewBreakType ||
          StyleClear::Both == aNewBreakType) {
        breakType = StyleClear::Both;
      }
      break;
    case StyleClear::Right:
      if (StyleClear::Left == aNewBreakType ||
          StyleClear::Both == aNewBreakType) {
        breakType = StyleClear::Both;
      }
      break;
    case StyleClear::None:
      if (StyleClear::Left == aNewBreakType ||
          StyleClear::Right == aNewBreakType ||
          StyleClear::Both == aNewBreakType) {
        breakType = aNewBreakType;
      }
      break;
    default:
      break;
  }
  return breakType;
}

#ifdef MOZ_DUMP_PAINTING
#include <stdio.h>

static bool gDumpEventList = false;

// nsLayoutUtils::PaintFrame() can call itself recursively, so rather than
// maintaining a single paint count, we need a stack.
StaticAutoPtr<nsTArray<int>> gPaintCountStack;

struct AutoNestedPaintCount {
  AutoNestedPaintCount() {
    gPaintCountStack->AppendElement(0);
  }
  ~AutoNestedPaintCount() {
    gPaintCountStack->RemoveElementAt(gPaintCountStack->Length() - 1);
  }
};

#endif

nsIFrame*
nsLayoutUtils::GetFrameForPoint(nsIFrame* aFrame, nsPoint aPt, uint32_t aFlags)
{
  AUTO_PROFILER_LABEL("nsLayoutUtils::GetFrameForPoint", GRAPHICS);

  nsresult rv;
  AutoTArray<nsIFrame*,8> outFrames;
  rv = GetFramesForArea(aFrame, nsRect(aPt, nsSize(1, 1)), outFrames, aFlags);
  NS_ENSURE_SUCCESS(rv, nullptr);
  return outFrames.Length() ? outFrames.ElementAt(0) : nullptr;
}

nsresult
nsLayoutUtils::GetFramesForArea(nsIFrame* aFrame, const nsRect& aRect,
                                nsTArray<nsIFrame*> &aOutFrames,
                                uint32_t aFlags)
{
  AUTO_PROFILER_LABEL("nsLayoutUtils::GetFramesForArea", GRAPHICS);

  nsDisplayListBuilder builder(aFrame,
                               nsDisplayListBuilderMode::EVENT_DELIVERY,
                               false);
  nsDisplayList list;

  if (aFlags & IGNORE_PAINT_SUPPRESSION) {
    builder.IgnorePaintSuppression();
  }

  if (aFlags & IGNORE_ROOT_SCROLL_FRAME) {
    nsIFrame* rootScrollFrame =
      aFrame->PresContext()->PresShell()->GetRootScrollFrame();
    if (rootScrollFrame) {
      builder.SetIgnoreScrollFrame(rootScrollFrame);
    }
  }
  if (aFlags & IGNORE_CROSS_DOC) {
    builder.SetDescendIntoSubdocuments(false);
  }

  builder.EnterPresShell(aFrame);
  aFrame->BuildDisplayListForStackingContext(&builder, aRect, &list);
  builder.LeavePresShell(aFrame, nullptr);

#ifdef MOZ_DUMP_PAINTING
  if (gDumpEventList) {
    fprintf_stderr(stderr, "Event handling --- (%d,%d):\n", aRect.x, aRect.y);

    std::stringstream ss;
    nsFrame::PrintDisplayList(&builder, list, ss);
    print_stderr(ss);
  }
#endif

  nsDisplayItem::HitTestState hitTestState;
  builder.SetHitTestShouldStopAtFirstOpaque(aFlags & ONLY_VISIBLE);
  list.HitTest(&builder, aRect, &hitTestState, &aOutFrames);
  list.DeleteAll();
  return NS_OK;
}

// aScrollFrameAsScrollable must be non-nullptr and queryable to an nsIFrame
FrameMetrics
nsLayoutUtils::CalculateBasicFrameMetrics(nsIScrollableFrame* aScrollFrame) {
  nsIFrame* frame = do_QueryFrame(aScrollFrame);
  MOZ_ASSERT(frame);

  // Calculate the metrics necessary for calculating the displayport.
  // This code has a lot in common with the code in ComputeFrameMetrics();
  // we may want to refactor this at some point.
  FrameMetrics metrics;
  nsPresContext* presContext = frame->PresContext();
  nsIPresShell* presShell = presContext->PresShell();
  CSSToLayoutDeviceScale deviceScale = presContext->CSSToDevPixelScale();
  float resolution = 1.0f;
  if (frame == presShell->GetRootScrollFrame()) {
    // Only the root scrollable frame for a given presShell should pick up
    // the presShell's resolution. All the other frames are 1.0.
    resolution = presShell->GetResolution();
  }
  // Note: unlike in ComputeFrameMetrics(), we don't know the full cumulative
  // resolution including FrameMetrics::mExtraResolution, because layout hasn't
  // chosen a resolution to paint at yet. However, the display port calculation
  // divides out mExtraResolution anyways, so we get the correct result by
  // setting the mCumulativeResolution to everything except the extra resolution
  // and leaving mExtraResolution at 1.
  LayoutDeviceToLayerScale2D cumulativeResolution(
      presShell->GetCumulativeResolution()
    * nsLayoutUtils::GetTransformToAncestorScale(frame));

  LayerToParentLayerScale layerToParentLayerScale(1.0f);
  metrics.SetDevPixelsPerCSSPixel(deviceScale);
  metrics.SetPresShellResolution(resolution);
  metrics.SetCumulativeResolution(cumulativeResolution);
  metrics.SetZoom(deviceScale * cumulativeResolution * layerToParentLayerScale);

  // Only the size of the composition bounds is relevant to the
  // displayport calculation, not its origin.
  nsSize compositionSize = nsLayoutUtils::CalculateCompositionSizeForFrame(frame);
  LayoutDeviceToParentLayerScale2D compBoundsScale;
  if (frame == presShell->GetRootScrollFrame() && presContext->IsRootContentDocument()) {
    if (presContext->GetParentPresContext()) {
      float res = presContext->GetParentPresContext()->PresShell()->GetCumulativeResolution();
      compBoundsScale = LayoutDeviceToParentLayerScale2D(
          LayoutDeviceToParentLayerScale(res));
    }
  } else {
    compBoundsScale = cumulativeResolution * layerToParentLayerScale;
  }
  metrics.SetCompositionBounds(
      LayoutDeviceRect::FromAppUnits(nsRect(nsPoint(0, 0), compositionSize),
                                       presContext->AppUnitsPerDevPixel())
      * compBoundsScale);

  metrics.SetRootCompositionSize(
      nsLayoutUtils::CalculateRootCompositionSize(frame, false, metrics));

  metrics.SetScrollOffset(CSSPoint::FromAppUnits(
      aScrollFrame->GetScrollPosition()));

  metrics.SetScrollableRect(CSSRect::FromAppUnits(
      nsLayoutUtils::CalculateScrollableRectForFrame(aScrollFrame, nullptr)));

  return metrics;
}

bool
nsLayoutUtils::CalculateAndSetDisplayPortMargins(nsIScrollableFrame* aScrollFrame,
                                                 RepaintMode aRepaintMode) {
  nsIFrame* frame = do_QueryFrame(aScrollFrame);
  MOZ_ASSERT(frame);
  nsIContent* content = frame->GetContent();
  MOZ_ASSERT(content);

  FrameMetrics metrics = CalculateBasicFrameMetrics(aScrollFrame);
  ScreenMargin displayportMargins = APZCTreeManager::CalculatePendingDisplayPort(
      metrics, ParentLayerPoint(0.0f, 0.0f));
  nsIPresShell* presShell = frame->PresContext()->GetPresShell();
  return nsLayoutUtils::SetDisplayPortMargins(
      content, presShell, displayportMargins, 0, aRepaintMode);
}

void
nsLayoutUtils::MaybeCreateDisplayPort(nsDisplayListBuilder& aBuilder,
                                      nsIFrame* aScrollFrame) {
  nsIContent* content = aScrollFrame->GetContent();
  nsIScrollableFrame* scrollableFrame = do_QueryFrame(aScrollFrame);
  if (!content || !scrollableFrame) {
    return;
  }

  bool haveDisplayPort = HasDisplayPort(content);

  // We perform an optimization where we ensure that at least one
  // async-scrollable frame (i.e. one that WantsAsyncScroll()) has a displayport.
  // If that's not the case yet, and we are async-scrollable, we will get a
  // displayport.
  if (aBuilder.IsPaintingToWindow() &&
      nsLayoutUtils::AsyncPanZoomEnabled(aScrollFrame) &&
      !aBuilder.HaveScrollableDisplayPort() &&
      scrollableFrame->WantAsyncScroll()) {

    // If we don't already have a displayport, calculate and set one.
    if (!haveDisplayPort) {
      CalculateAndSetDisplayPortMargins(scrollableFrame, nsLayoutUtils::RepaintMode::DoNotRepaint);
#ifdef DEBUG
      haveDisplayPort = HasDisplayPort(content);
      MOZ_ASSERT(haveDisplayPort, "should have a displayport after having just set it");
#endif
    }

    // Record that the we now have a scrollable display port.
    aBuilder.SetHaveScrollableDisplayPort();
  }
}

nsIScrollableFrame*
nsLayoutUtils::GetAsyncScrollableAncestorFrame(nsIFrame* aTarget)
{
  uint32_t flags = nsLayoutUtils::SCROLLABLE_ALWAYS_MATCH_ROOT
                 | nsLayoutUtils::SCROLLABLE_ONLY_ASYNC_SCROLLABLE
                 | nsLayoutUtils::SCROLLABLE_FIXEDPOS_FINDS_ROOT;
  return nsLayoutUtils::GetNearestScrollableFrame(aTarget, flags);
}

void
nsLayoutUtils::SetZeroMarginDisplayPortOnAsyncScrollableAncestors(nsIFrame* aFrame,
                                                                  RepaintMode aRepaintMode)
{
  nsIFrame* frame = aFrame;
  while (frame) {
    frame = nsLayoutUtils::GetCrossDocParentFrame(frame);
    if (!frame) {
      break;
    }
    nsIScrollableFrame* scrollAncestor = GetAsyncScrollableAncestorFrame(frame);
    if (!scrollAncestor) {
      break;
    }
    frame = do_QueryFrame(scrollAncestor);
    MOZ_ASSERT(frame);
    MOZ_ASSERT(scrollAncestor->WantAsyncScroll() ||
      frame->PresContext()->PresShell()->GetRootScrollFrame() == frame);
    if (nsLayoutUtils::AsyncPanZoomEnabled(frame) &&
        !nsLayoutUtils::HasDisplayPort(frame->GetContent())) {
      nsLayoutUtils::SetDisplayPortMargins(
        frame->GetContent(), frame->PresContext()->PresShell(), ScreenMargin(), 0,
        aRepaintMode);
    }
  }
}

void
nsLayoutUtils::ExpireDisplayPortOnAsyncScrollableAncestor(nsIFrame* aFrame)
{
  nsIFrame* frame = aFrame;
  while (frame) {
    frame = nsLayoutUtils::GetCrossDocParentFrame(frame);
    if (!frame) {
      break;
    }
    nsIScrollableFrame* scrollAncestor = GetAsyncScrollableAncestorFrame(frame);
    if (!scrollAncestor) {
      break;
    }
    frame = do_QueryFrame(scrollAncestor);
    MOZ_ASSERT(frame);
    if (!frame) {
      break;
    }
    MOZ_ASSERT(scrollAncestor->WantAsyncScroll() ||
      frame->PresContext()->PresShell()->GetRootScrollFrame() == frame);
    if (nsLayoutUtils::HasDisplayPort(frame->GetContent())) {
      scrollAncestor->TriggerDisplayPortExpiration();
      // Stop after the first trigger. If it failed, there's no point in
      // continuing because all the rest of the frames we encounter are going
      // to be ancestors of |scrollAncestor| which will keep its displayport.
      // If the trigger succeeded, we stop because when the trigger executes
      // it will call this function again to trigger the next ancestor up the
      // chain.
      break;
    }
  }
}

nsresult
nsLayoutUtils::PaintFrame(gfxContext* aRenderingContext, nsIFrame* aFrame,
                          const nsRegion& aDirtyRegion, nscolor aBackstop,
                          nsDisplayListBuilderMode aBuilderMode,
                          PaintFrameFlags aFlags)
{
  AUTO_PROFILER_LABEL("nsLayoutUtils::PaintFrame", GRAPHICS);

#ifdef MOZ_DUMP_PAINTING
  if (!gPaintCountStack) {
    gPaintCountStack = new nsTArray<int>();
    ClearOnShutdown(&gPaintCountStack);

    gPaintCountStack->AppendElement(0);
  }
  ++gPaintCountStack->LastElement();
  AutoNestedPaintCount nestedPaintCount;
#endif

  if (aFlags & PaintFrameFlags::PAINT_WIDGET_LAYERS) {
    nsView* view = aFrame->GetView();
    if (!(view && view->GetWidget() && GetDisplayRootFrame(aFrame) == aFrame)) {
      aFlags &= ~PaintFrameFlags::PAINT_WIDGET_LAYERS;
      NS_ASSERTION(aRenderingContext, "need a rendering context");
    }
  }

  nsPresContext* presContext = aFrame->PresContext();
  nsIPresShell* presShell = presContext->PresShell();
  nsRootPresContext* rootPresContext = presContext->GetRootPresContext();
  if (!rootPresContext) {
    return NS_OK;
  }

  TimeStamp startBuildDisplayList = TimeStamp::Now();
  nsDisplayListBuilder builder(aFrame, aBuilderMode,
                               !(aFlags & PaintFrameFlags::PAINT_HIDE_CARET));
  if (aFlags & PaintFrameFlags::PAINT_IN_TRANSFORM) {
    builder.SetInTransform(true);
  }
  if (aFlags & PaintFrameFlags::PAINT_SYNC_DECODE_IMAGES) {
    builder.SetSyncDecodeImages(true);
  }
  if (aFlags & (PaintFrameFlags::PAINT_WIDGET_LAYERS |
                PaintFrameFlags::PAINT_TO_WINDOW)) {
    builder.SetPaintingToWindow(true);
  }
  if (aFlags & PaintFrameFlags::PAINT_IGNORE_SUPPRESSION) {
    builder.IgnorePaintSuppression();
  }

  nsIFrame* rootScrollFrame = presShell->GetRootScrollFrame();
  if (rootScrollFrame && !aFrame->GetParent()) {
    nsIScrollableFrame* rootScrollableFrame = presShell->GetRootScrollFrameAsScrollable();
    MOZ_ASSERT(rootScrollableFrame);
    nsRect displayPortBase = aFrame->GetVisualOverflowRectRelativeToSelf();
    Unused << rootScrollableFrame->DecideScrollableLayer(&builder, &displayPortBase,
                /* aAllowCreateDisplayPort = */ true);
  }

  nsRegion visibleRegion;
  if (aFlags & PaintFrameFlags::PAINT_WIDGET_LAYERS) {
    // This layer tree will be reused, so we'll need to calculate it
    // for the whole "visible" area of the window
    //
    // |ignoreViewportScrolling| and |usingDisplayPort| are persistent
    // document-rendering state.  We rely on PresShell to flush
    // retained layers as needed when that persistent state changes.
    visibleRegion = aFrame->GetVisualOverflowRectRelativeToSelf();
  } else {
    visibleRegion = aDirtyRegion;
  }

  nsDisplayList list;

  // If the root has embedded plugins, flag the builder so we know we'll need
  // to update plugin geometry after painting.
  if ((aFlags & PaintFrameFlags::PAINT_WIDGET_LAYERS) &&
      !(aFlags & PaintFrameFlags::PAINT_DOCUMENT_RELATIVE) &&
      rootPresContext->NeedToComputePluginGeometryUpdates()) {
    builder.SetWillComputePluginGeometry(true);
  }

  nsRect canvasArea(nsPoint(0, 0), aFrame->GetSize());
  bool ignoreViewportScrolling =
    aFrame->GetParent() ? false : presShell->IgnoringViewportScrolling();
  if (ignoreViewportScrolling && rootScrollFrame) {
    nsIScrollableFrame* rootScrollableFrame =
      presShell->GetRootScrollFrameAsScrollable();
    if (aFlags & PaintFrameFlags::PAINT_DOCUMENT_RELATIVE) {
      // Make visibleRegion and aRenderingContext relative to the
      // scrolled frame instead of the root frame.
      nsPoint pos = rootScrollableFrame->GetScrollPosition();
      visibleRegion.MoveBy(-pos);
      if (aRenderingContext) {
        gfxPoint devPixelOffset =
          nsLayoutUtils::PointToGfxPoint(pos,
                                         presContext->AppUnitsPerDevPixel());
        aRenderingContext->SetMatrix(
          aRenderingContext->CurrentMatrix().Translate(devPixelOffset));
      }
    }
    builder.SetIgnoreScrollFrame(rootScrollFrame);

    nsCanvasFrame* canvasFrame =
      do_QueryFrame(rootScrollableFrame->GetScrolledFrame());
    if (canvasFrame) {
      // Use UnionRect here to ensure that areas where the scrollbars
      // were are still filled with the background color.
      canvasArea.UnionRect(canvasArea,
        canvasFrame->CanvasArea() + builder.ToReferenceFrame(canvasFrame));
    }
  }

  nsRect dirtyRect = visibleRegion.GetBounds();

  {
    AUTO_PROFILER_LABEL("nsLayoutUtils::PaintFrame:BuildDisplayList",
                        GRAPHICS);
    AutoProfilerTracing tracing("Paint", "DisplayList");

    PaintTelemetry::AutoRecord record(PaintTelemetry::Metric::DisplayList);

    builder.EnterPresShell(aFrame);
    {
      // If a scrollable container layer is created in nsDisplayList::PaintForFrame,
      // it will be the scroll parent for display items that are built in the
      // BuildDisplayListForStackingContext call below. We need to set the scroll
      // parent on the display list builder while we build those items, so that they
      // can pick up their scroll parent's id.
      ViewID id = FrameMetrics::NULL_SCROLL_ID;
      if (ignoreViewportScrolling && presContext->IsRootContentDocument()) {
        if (nsIFrame* rootScrollFrame = presShell->GetRootScrollFrame()) {
          if (nsIContent* content = rootScrollFrame->GetContent()) {
            id = nsLayoutUtils::FindOrCreateIDFor(content);
          }
        }
      }
      else if (presShell->GetDocument() && presShell->GetDocument()->IsRootDisplayDocument()
          && !presShell->GetRootScrollFrame()) {
        // In cases where the root document is a XUL document, we want to take
        // the ViewID from the root element, as that will be the ViewID of the
        // root APZC in the tree. Skip doing this in cases where we know
        // nsGfxScrollFrame::BuilDisplayList will do it instead.
        if (dom::Element* element = presShell->GetDocument()->GetDocumentElement()) {
          id = nsLayoutUtils::FindOrCreateIDFor(element);
        }
      }

      nsDisplayListBuilder::AutoCurrentScrollParentIdSetter idSetter(&builder, id);

      aFrame->BuildDisplayListForStackingContext(&builder, dirtyRect, &list);
    }

    LayoutFrameType frameType = aFrame->Type();

    // For the viewport frame in print preview/page layout we want to paint
    // the grey background behind the page, not the canvas color.
    if (frameType == LayoutFrameType::Viewport &&
        nsLayoutUtils::NeedsPrintPreviewBackground(presContext)) {
      nsRect bounds = nsRect(builder.ToReferenceFrame(aFrame),
                             aFrame->GetSize());
      nsDisplayListBuilder::AutoBuildingDisplayList
        buildingDisplayList(&builder, aFrame, bounds, false);
      presShell->AddPrintPreviewBackgroundItem(builder, list, aFrame, bounds);
    } else if (frameType != LayoutFrameType::Page) {
      // For printing, this function is first called on an nsPageFrame, which
      // creates a display list with a PageContent item. The PageContent item's
      // paint function calls this function on the nsPageFrame's child which is
      // an nsPageContentFrame. We only want to add the canvas background color
      // item once, for the nsPageContentFrame.

      // Add the canvas background color to the bottom of the list. This
      // happens after we've built the list so that AddCanvasBackgroundColorItem
      // can monkey with the contents if necessary.
      canvasArea.IntersectRect(canvasArea, visibleRegion.GetBounds());
      nsDisplayListBuilder::AutoBuildingDisplayList
        buildingDisplayList(&builder, aFrame, canvasArea, false);
      presShell->AddCanvasBackgroundColorItem(
             builder, list, aFrame, canvasArea, aBackstop);
    }

    builder.LeavePresShell(aFrame, &list);

    if (!record.GetStart().IsNull() && gfxPrefs::LayersDrawFPS()) {
      if (RefPtr<LayerManager> lm = builder.GetWidgetLayerManager()) {
        if (PaintTiming* pt = ClientLayerManager::MaybeGetPaintTiming(lm)) {
          pt->dlMs() = (TimeStamp::Now() - record.GetStart()).ToMilliseconds();
        }
      }
    }
  }

  Telemetry::AccumulateTimeDelta(Telemetry::PAINT_BUILD_DISPLAYLIST_TIME,
                                 startBuildDisplayList);

  bool profilerNeedsDisplayList =
    profiler_feature_active(ProfilerFeature::DisplayListDump);
  bool consoleNeedsDisplayList = gfxUtils::DumpDisplayList() || gfxEnv::DumpPaint();
#ifdef MOZ_DUMP_PAINTING
  FILE* savedDumpFile = gfxUtils::sDumpPaintFile;
#endif

  UniquePtr<std::stringstream> ss;
  if (consoleNeedsDisplayList || profilerNeedsDisplayList) {
    ss = MakeUnique<std::stringstream>();
#ifdef MOZ_DUMP_PAINTING
    if (gfxEnv::DumpPaintToFile()) {
      nsCString string("dump-");
      // Include the process ID in the dump file name, to make sure that in an
      // e10s setup different processes don't clobber each other's dump files.
      string.AppendInt(getpid());
      for (int paintCount : *gPaintCountStack) {
        string.AppendLiteral("-");
        string.AppendInt(paintCount);
      }
      string.AppendLiteral(".html");
      gfxUtils::sDumpPaintFile = fopen(string.BeginReading(), "w");
    } else {
      gfxUtils::sDumpPaintFile = stderr;
    }
    if (gfxEnv::DumpPaintToFile()) {
      *ss << "<html><head><script>\n"
             "var array = {};\n"
             "function ViewImage(index) { \n"
             "  var image = document.getElementById(index);\n"
             "  if (image.src) {\n"
             "    image.removeAttribute('src');\n"
             "  } else {\n"
             "    image.src = array[index];\n"
             "  }\n"
             "}</script></head><body>";
    }
#endif
    *ss << nsPrintfCString("Painting --- before optimization (dirty %d,%d,%d,%d):\n",
            dirtyRect.x, dirtyRect.y, dirtyRect.width, dirtyRect.height).get();
    nsFrame::PrintDisplayList(&builder, list, *ss, gfxEnv::DumpPaintToFile());

    if (gfxEnv::DumpPaint() || gfxEnv::DumpPaintItems()) {
      // Flush stream now to avoid reordering dump output relative to
      // messages dumped by PaintRoot below.
      if (profilerNeedsDisplayList && !consoleNeedsDisplayList) {
        profiler_tracing("log", ss->str().c_str());
      } else {
        fprint_stderr(gfxUtils::sDumpPaintFile, *ss);
      }
      ss = MakeUnique<std::stringstream>();
    }
  }

  uint32_t flags = nsDisplayList::PAINT_DEFAULT;
  if (aFlags & PaintFrameFlags::PAINT_WIDGET_LAYERS) {
    flags |= nsDisplayList::PAINT_USE_WIDGET_LAYERS;
    if (!(aFlags & PaintFrameFlags::PAINT_DOCUMENT_RELATIVE)) {
      nsIWidget *widget = aFrame->GetNearestWidget();
      if (widget) {
        // If we're finished building display list items for painting of the outermost
        // pres shell, notify the widget about any toolbars we've encountered.
        widget->UpdateThemeGeometries(builder.GetThemeGeometries());
      }
    }
  }
  if (aFlags & PaintFrameFlags::PAINT_EXISTING_TRANSACTION) {
    flags |= nsDisplayList::PAINT_EXISTING_TRANSACTION;
  }
  if (aFlags & PaintFrameFlags::PAINT_NO_COMPOSITE) {
    flags |= nsDisplayList::PAINT_NO_COMPOSITE;
  }
  if (aFlags & PaintFrameFlags::PAINT_COMPRESSED) {
    flags |= nsDisplayList::PAINT_COMPRESSED;
  }

  TimeStamp paintStart = TimeStamp::Now();
  RefPtr<LayerManager> layerManager
    = list.PaintRoot(&builder, aRenderingContext, flags);
  Telemetry::AccumulateTimeDelta(Telemetry::PAINT_RASTERIZE_TIME,
                                 paintStart);

  if (gfxPrefs::GfxLoggingPaintedPixelCountEnabled()) {
    TimeStamp now = TimeStamp::Now();
    float rasterizeTime = (now - paintStart).ToMilliseconds();
    uint32_t pixelCount = layerManager->GetAndClearPaintedPixelCount();
    static std::vector<std::pair<TimeStamp, uint32_t>> history;
    if (pixelCount) {
      history.push_back(std::make_pair(now, pixelCount));
    }
    uint32_t paintedInLastSecond = 0;
    for (auto i = history.begin(); i != history.end(); i++) {
      if ((now - i->first).ToMilliseconds() > 1000.0f) {
        // more than 1000ms ago, don't count it
        continue;
      }
      if (paintedInLastSecond == 0) {
        // This is the first one in the last 1000ms, so drop everything earlier
        history.erase(history.begin(), i);
        i = history.begin();
      }
      paintedInLastSecond += i->second;
      MOZ_ASSERT(paintedInLastSecond); // all historical pixel counts are > 0
    }
    printf_stderr("Painted %u pixels in %fms (%u in the last 1000ms)\n",
        pixelCount, rasterizeTime, paintedInLastSecond);
  }

  if (consoleNeedsDisplayList || profilerNeedsDisplayList) {
    *ss << "Painting --- after optimization:\n";
    nsFrame::PrintDisplayList(&builder, list, *ss, gfxEnv::DumpPaintToFile());

    *ss << "Painting --- layer tree:\n";
    if (layerManager) {
      FrameLayerBuilder::DumpRetainedLayerTree(layerManager, *ss,
                                               gfxEnv::DumpPaintToFile());
    }

    if (profilerNeedsDisplayList && !consoleNeedsDisplayList) {
      profiler_tracing("log", ss->str().c_str());
    } else {
      fprint_stderr(gfxUtils::sDumpPaintFile, *ss);
    }

#ifdef MOZ_DUMP_PAINTING
    if (gfxEnv::DumpPaintToFile()) {
      *ss << "</body></html>";
    }
    if (gfxEnv::DumpPaintToFile()) {
      fclose(gfxUtils::sDumpPaintFile);
    }
    gfxUtils::sDumpPaintFile = savedDumpFile;
#endif

    std::stringstream lsStream;
    nsFrame::PrintDisplayList(&builder, list, lsStream);
    layerManager->GetRoot()->SetDisplayListLog(lsStream.str().c_str());
  }

#ifdef MOZ_DUMP_PAINTING
  if (gfxPrefs::DumpClientLayers()) {
    std::stringstream ss;
    FrameLayerBuilder::DumpRetainedLayerTree(layerManager, ss, false);
    print_stderr(ss);
  }
#endif

  // Update the widget's opaque region information. This sets
  // glass boundaries on Windows. Also set up the window dragging region
  // and plugin clip regions and bounds.
  if ((aFlags & PaintFrameFlags::PAINT_WIDGET_LAYERS) &&
      !(aFlags & PaintFrameFlags::PAINT_DOCUMENT_RELATIVE)) {
    nsIWidget *widget = aFrame->GetNearestWidget();
    if (widget) {
      nsRegion opaqueRegion;
      opaqueRegion.And(builder.GetWindowExcludeGlassRegion(), builder.GetWindowOpaqueRegion());
      widget->UpdateOpaqueRegion(
        LayoutDeviceIntRegion::FromUnknownRegion(
          opaqueRegion.ToNearestPixels(presContext->AppUnitsPerDevPixel())));

      widget->UpdateWindowDraggingRegion(builder.GetWindowDraggingRegion());
    }
  }

  if (builder.WillComputePluginGeometry()) {
    // For single process compute and apply plugin geometry updates to plugin
    // windows, then request composition. For content processes skip eveything
    // except requesting composition. Geometry updates were calculated and
    // shipped to the chrome process in nsDisplayList when the layer
    // transaction completed.
    if (XRE_IsParentProcess()) {
      rootPresContext->ComputePluginGeometryUpdates(aFrame, &builder, &list);
      // We're not going to get a WillPaintWindow event here if we didn't do
      // widget invalidation, so just apply the plugin geometry update here
      // instead. We could instead have the compositor send back an equivalent
      // to WillPaintWindow, but it should be close enough to now not to matter.
      if (layerManager && !layerManager->NeedsWidgetInvalidation()) {
        rootPresContext->ApplyPluginGeometryUpdates();
      }
    }

    // We told the compositor thread not to composite when it received the
    // transaction because we wanted to update plugins first. Schedule the
    // composite now.
    if (layerManager) {
      layerManager->ScheduleComposite();
    }
  }


  // Flush the list so we don't trigger the IsEmpty-on-destruction assertion
  list.DeleteAll();
  return NS_OK;
}

/**
 * Uses a binary search for find where the cursor falls in the line of text
 * It also keeps track of the part of the string that has already been measured
 * so it doesn't have to keep measuring the same text over and over
 *
 * @param "aBaseWidth" contains the width in twips of the portion
 * of the text that has already been measured, and aBaseInx contains
 * the index of the text that has already been measured.
 *
 * @param aTextWidth returns the (in twips) the length of the text that falls
 * before the cursor aIndex contains the index of the text where the cursor falls
 */
bool
nsLayoutUtils::BinarySearchForPosition(DrawTarget* aDrawTarget,
                                       nsFontMetrics& aFontMetrics,
                        const char16_t* aText,
                        int32_t    aBaseWidth,
                        int32_t    aBaseInx,
                        int32_t    aStartInx,
                        int32_t    aEndInx,
                        int32_t    aCursorPos,
                        int32_t&   aIndex,
                        int32_t&   aTextWidth)
{
  int32_t range = aEndInx - aStartInx;
  if ((range == 1) || (range == 2 && NS_IS_HIGH_SURROGATE(aText[aStartInx]))) {
    aIndex   = aStartInx + aBaseInx;
    aTextWidth = nsLayoutUtils::AppUnitWidthOfString(aText, aIndex,
                                                     aFontMetrics, aDrawTarget);
    return true;
  }

  int32_t inx = aStartInx + (range / 2);

  // Make sure we don't leave a dangling low surrogate
  if (NS_IS_HIGH_SURROGATE(aText[inx-1]))
    inx++;

  int32_t textWidth = nsLayoutUtils::AppUnitWidthOfString(aText, inx,
                                                          aFontMetrics,
                                                          aDrawTarget);

  int32_t fullWidth = aBaseWidth + textWidth;
  if (fullWidth == aCursorPos) {
    aTextWidth = textWidth;
    aIndex = inx;
    return true;
  } else if (aCursorPos < fullWidth) {
    aTextWidth = aBaseWidth;
    if (BinarySearchForPosition(aDrawTarget, aFontMetrics, aText, aBaseWidth,
                                aBaseInx, aStartInx, inx, aCursorPos, aIndex,
                                aTextWidth)) {
      return true;
    }
  } else {
    aTextWidth = fullWidth;
    if (BinarySearchForPosition(aDrawTarget, aFontMetrics, aText, aBaseWidth,
                                aBaseInx, inx, aEndInx, aCursorPos, aIndex,
                                aTextWidth)) {
      return true;
    }
  }
  return false;
}

static void
AddBoxesForFrame(nsIFrame* aFrame,
                 nsLayoutUtils::BoxCallback* aCallback)
{
  nsIAtom* pseudoType = aFrame->StyleContext()->GetPseudo();

  if (pseudoType == nsCSSAnonBoxes::tableWrapper) {
    AddBoxesForFrame(aFrame->PrincipalChildList().FirstChild(), aCallback);
    if (aCallback->mIncludeCaptionBoxForTable) {
      nsIFrame* kid = aFrame->GetChildList(nsIFrame::kCaptionList).FirstChild();
      if (kid) {
        AddBoxesForFrame(kid, aCallback);
      }
    }
  } else if (pseudoType == nsCSSAnonBoxes::mozBlockInsideInlineWrapper ||
             pseudoType == nsCSSAnonBoxes::mozMathMLAnonymousBlock ||
             pseudoType == nsCSSAnonBoxes::mozXULAnonymousBlock) {
    for (nsIFrame* kid : aFrame->PrincipalChildList()) {
      AddBoxesForFrame(kid, aCallback);
    }
  } else {
    aCallback->AddBox(aFrame);
  }
}

void
nsLayoutUtils::GetAllInFlowBoxes(nsIFrame* aFrame, BoxCallback* aCallback)
{
  while (aFrame) {
    AddBoxesForFrame(aFrame, aCallback);
    aFrame = nsLayoutUtils::GetNextContinuationOrIBSplitSibling(aFrame);
  }
}

nsIFrame*
nsLayoutUtils::GetFirstNonAnonymousFrame(nsIFrame* aFrame)
{
  while (aFrame) {
    nsIAtom* pseudoType = aFrame->StyleContext()->GetPseudo();

    if (pseudoType == nsCSSAnonBoxes::tableWrapper) {
      nsIFrame* f = GetFirstNonAnonymousFrame(aFrame->PrincipalChildList().FirstChild());
      if (f) {
        return f;
      }
      nsIFrame* kid = aFrame->GetChildList(nsIFrame::kCaptionList).FirstChild();
      if (kid) {
        f = GetFirstNonAnonymousFrame(kid);
        if (f) {
          return f;
        }
      }
    } else if (pseudoType == nsCSSAnonBoxes::mozBlockInsideInlineWrapper ||
               pseudoType == nsCSSAnonBoxes::mozMathMLAnonymousBlock ||
               pseudoType == nsCSSAnonBoxes::mozXULAnonymousBlock) {
      for (nsIFrame* kid : aFrame->PrincipalChildList()) {
        nsIFrame* f = GetFirstNonAnonymousFrame(kid);
        if (f) {
          return f;
        }
      }
    } else {
      return aFrame;
    }

    aFrame = nsLayoutUtils::GetNextContinuationOrIBSplitSibling(aFrame);
  }
  return nullptr;
}

struct BoxToRect : public nsLayoutUtils::BoxCallback {
  nsIFrame* mRelativeTo;
  nsLayoutUtils::RectCallback* mCallback;
  uint32_t mFlags;

  BoxToRect(nsIFrame* aRelativeTo, nsLayoutUtils::RectCallback* aCallback,
            uint32_t aFlags)
    : mRelativeTo(aRelativeTo), mCallback(aCallback), mFlags(aFlags) {}

  virtual void AddBox(nsIFrame* aFrame) override {
    nsRect r;
    nsIFrame* outer = nsSVGUtils::GetOuterSVGFrameAndCoveredRegion(aFrame, &r);
    if (!outer) {
      outer = aFrame;
      switch (mFlags & nsLayoutUtils::RECTS_WHICH_BOX_MASK) {
        case nsLayoutUtils::RECTS_USE_CONTENT_BOX:
          r = aFrame->GetContentRectRelativeToSelf();
          break;
        case nsLayoutUtils::RECTS_USE_PADDING_BOX:
          r = aFrame->GetPaddingRectRelativeToSelf();
          break;
        case nsLayoutUtils::RECTS_USE_MARGIN_BOX:
          r = aFrame->GetMarginRectRelativeToSelf();
          break;
        default: // Use the border box
          r = aFrame->GetRectRelativeToSelf();
      }
    }
    if (mFlags & nsLayoutUtils::RECTS_ACCOUNT_FOR_TRANSFORMS) {
      r = nsLayoutUtils::TransformFrameRectToAncestor(outer, r, mRelativeTo);
    } else {
      r += outer->GetOffsetTo(mRelativeTo);
    }
    mCallback->AddRect(r);
  }
};

struct MOZ_RAII BoxToRectAndText : public BoxToRect {
  Sequence<nsString>* mTextList;

  BoxToRectAndText(nsIFrame* aRelativeTo, nsLayoutUtils::RectCallback* aCallback,
                   Sequence<nsString>* aTextList, uint32_t aFlags)
    : BoxToRect(aRelativeTo, aCallback, aFlags), mTextList(aTextList) {}

  static void AccumulateText(nsIFrame* aFrame, nsAString& aResult) {
    MOZ_ASSERT(aFrame);

    // Get all the text in aFrame and child frames, while respecting
    // the content offsets in each of the nsTextFrames.
    if (aFrame->IsTextFrame()) {
      nsTextFrame* textFrame = static_cast<nsTextFrame*>(aFrame);

      nsIFrame::RenderedText renderedText = textFrame->GetRenderedText(
        textFrame->GetContentOffset(),
        textFrame->GetContentOffset() + textFrame->GetContentLength(),
        nsIFrame::TextOffsetType::OFFSETS_IN_CONTENT_TEXT,
        nsIFrame::TrailingWhitespace::DONT_TRIM_TRAILING_WHITESPACE);

      aResult.Append(renderedText.mString);
    }

    for (nsIFrame* child = aFrame->PrincipalChildList().FirstChild();
         child;
         child = child->GetNextSibling()) {
      AccumulateText(child, aResult);
    }
  }

  virtual void AddBox(nsIFrame* aFrame) override {
    BoxToRect::AddBox(aFrame);
    if (mTextList) {
      nsString* textForFrame = mTextList->AppendElement(fallible);
      if (textForFrame) {
        AccumulateText(aFrame, *textForFrame);
      }
    }
  }
};

void
nsLayoutUtils::GetAllInFlowRects(nsIFrame* aFrame, nsIFrame* aRelativeTo,
                                 RectCallback* aCallback, uint32_t aFlags)
{
  BoxToRect converter(aRelativeTo, aCallback, aFlags);
  GetAllInFlowBoxes(aFrame, &converter);
}

void
nsLayoutUtils::GetAllInFlowRectsAndTexts(nsIFrame* aFrame, nsIFrame* aRelativeTo,
                                         RectCallback* aCallback,
                                         Sequence<nsString>* aTextList,
                                         uint32_t aFlags)
{
  BoxToRectAndText converter(aRelativeTo, aCallback, aTextList, aFlags);
  GetAllInFlowBoxes(aFrame, &converter);
}

nsLayoutUtils::RectAccumulator::RectAccumulator() : mSeenFirstRect(false) {}

void nsLayoutUtils::RectAccumulator::AddRect(const nsRect& aRect) {
  mResultRect.UnionRect(mResultRect, aRect);
  if (!mSeenFirstRect) {
    mSeenFirstRect = true;
    mFirstRect = aRect;
  }
}

nsLayoutUtils::RectListBuilder::RectListBuilder(DOMRectList* aList)
  : mRectList(aList)
{
}

void nsLayoutUtils::RectListBuilder::AddRect(const nsRect& aRect) {
  RefPtr<DOMRect> rect = new DOMRect(mRectList);

  rect->SetLayoutRect(aRect);
  mRectList->Append(rect);
}

nsIFrame* nsLayoutUtils::GetContainingBlockForClientRect(nsIFrame* aFrame)
{
  return aFrame->PresContext()->PresShell()->GetRootFrame();
}

nsRect
nsLayoutUtils::GetAllInFlowRectsUnion(nsIFrame* aFrame, nsIFrame* aRelativeTo,
                                      uint32_t aFlags) {
  RectAccumulator accumulator;
  GetAllInFlowRects(aFrame, aRelativeTo, &accumulator, aFlags);
  return accumulator.mResultRect.IsEmpty() ? accumulator.mFirstRect
          : accumulator.mResultRect;
}

nsRect
nsLayoutUtils::GetTextShadowRectsUnion(const nsRect& aTextAndDecorationsRect,
                                       nsIFrame* aFrame,
                                       uint32_t aFlags)
{
  const nsStyleText* textStyle = aFrame->StyleText();
  if (!textStyle->HasTextShadow())
    return aTextAndDecorationsRect;

  nsRect resultRect = aTextAndDecorationsRect;
  int32_t A2D = aFrame->PresContext()->AppUnitsPerDevPixel();
  for (uint32_t i = 0; i < textStyle->mTextShadow->Length(); ++i) {
    nsCSSShadowItem* shadow = textStyle->mTextShadow->ShadowAt(i);
    nsMargin blur = nsContextBoxBlur::GetBlurRadiusMargin(shadow->mRadius, A2D);
    if ((aFlags & EXCLUDE_BLUR_SHADOWS) && blur != nsMargin(0, 0, 0, 0))
      continue;

    nsRect tmpRect(aTextAndDecorationsRect);

    tmpRect.MoveBy(nsPoint(shadow->mXOffset, shadow->mYOffset));
    tmpRect.Inflate(blur);

    resultRect.UnionRect(resultRect, tmpRect);
  }
  return resultRect;
}

enum ObjectDimensionType { eWidth, eHeight };
static nscoord
ComputeMissingDimension(const nsSize& aDefaultObjectSize,
                        const nsSize& aIntrinsicRatio,
                        const Maybe<nscoord>& aSpecifiedWidth,
                        const Maybe<nscoord>& aSpecifiedHeight,
                        ObjectDimensionType aDimensionToCompute)
{
  // The "default sizing algorithm" computes the missing dimension as follows:
  // (source: http://dev.w3.org/csswg/css-images-3/#default-sizing )

  // 1. "If the object has an intrinsic aspect ratio, the missing dimension of
  //     the concrete object size is calculated using the intrinsic aspect
  //     ratio and the present dimension."
  if (aIntrinsicRatio.width > 0 && aIntrinsicRatio.height > 0) {
    // Fill in the missing dimension using the intrinsic aspect ratio.
    nscoord knownDimensionSize;
    float ratio;
    if (aDimensionToCompute == eWidth) {
      knownDimensionSize = *aSpecifiedHeight;
      ratio = aIntrinsicRatio.width / aIntrinsicRatio.height;
    } else {
      knownDimensionSize = *aSpecifiedWidth;
      ratio = aIntrinsicRatio.height / aIntrinsicRatio.width;
    }
    return NSCoordSaturatingNonnegativeMultiply(knownDimensionSize, ratio);
  }

  // 2. "Otherwise, if the missing dimension is present in the object’s
  //     intrinsic dimensions, [...]"
  // NOTE: *Skipping* this case, because we already know it's not true -- we're
  // in this function because the missing dimension is *not* present in
  // the object's intrinsic dimensions.

  // 3. "Otherwise, the missing dimension of the concrete object size is taken
  //     from the default object size. "
  return (aDimensionToCompute == eWidth) ?
    aDefaultObjectSize.width : aDefaultObjectSize.height;
}

/*
 * This computes & returns the concrete object size of replaced content, if
 * that content were to be rendered with "object-fit: none".  (Or, if the
 * element has neither an intrinsic height nor width, this method returns an
 * empty Maybe<> object.)
 *
 * As specced...
 *   http://dev.w3.org/csswg/css-images-3/#valdef-object-fit-none
 * ..we use "the default sizing algorithm with no specified size,
 * and a default object size equal to the replaced element's used width and
 * height."
 *
 * The default sizing algorithm is described here:
 *   http://dev.w3.org/csswg/css-images-3/#default-sizing
 * Quotes in the function-impl are taken from that ^ spec-text.
 *
 * Per its final bulleted section: since there's no specified size,
 * we run the default sizing algorithm using the object's intrinsic size in
 * place of the specified size. But if the object has neither an intrinsic
 * height nor an intrinsic width, then we instead return without populating our
 * outparam, and we let the caller figure out the size (using a contain
 * constraint).
 */
static Maybe<nsSize>
MaybeComputeObjectFitNoneSize(const nsSize& aDefaultObjectSize,
                              const IntrinsicSize& aIntrinsicSize,
                              const nsSize& aIntrinsicRatio)
{
  // "If the object has an intrinsic height or width, its size is resolved as
  // if its intrinsic dimensions were given as the specified size."
  //
  // So, first we check if we have an intrinsic height and/or width:
  Maybe<nscoord> specifiedWidth;
  if (aIntrinsicSize.width.GetUnit() == eStyleUnit_Coord) {
    specifiedWidth.emplace(aIntrinsicSize.width.GetCoordValue());
  }

  Maybe<nscoord> specifiedHeight;
  if (aIntrinsicSize.height.GetUnit() == eStyleUnit_Coord) {
    specifiedHeight.emplace(aIntrinsicSize.height.GetCoordValue());
  }

  Maybe<nsSize> noneSize; // (the value we'll return)
  if (specifiedWidth || specifiedHeight) {
    // We have at least one specified dimension; use whichever dimension is
    // specified, and compute the other one using our intrinsic ratio, or (if
    // no valid ratio) using the default object size.
    noneSize.emplace();

    noneSize->width = specifiedWidth ?
      *specifiedWidth :
      ComputeMissingDimension(aDefaultObjectSize, aIntrinsicRatio,
                              specifiedWidth, specifiedHeight,
                              eWidth);

    noneSize->height = specifiedHeight ?
      *specifiedHeight :
      ComputeMissingDimension(aDefaultObjectSize, aIntrinsicRatio,
                              specifiedWidth, specifiedHeight,
                              eHeight);
  }
  // [else:] "Otherwise [if there's neither an intrinsic height nor width], its
  // size is resolved as a contain constraint against the default object size."
  // We'll let our caller do that, to share code & avoid redundant
  // computations; so, we return w/out populating noneSize.
  return noneSize;
}

// Computes the concrete object size to render into, as described at
// http://dev.w3.org/csswg/css-images-3/#concrete-size-resolution
static nsSize
ComputeConcreteObjectSize(const nsSize& aConstraintSize,
                          const IntrinsicSize& aIntrinsicSize,
                          const nsSize& aIntrinsicRatio,
                          uint8_t aObjectFit)
{
  // Handle default behavior (filling the container) w/ fast early return.
  // (Also: if there's no valid intrinsic ratio, then we have the "fill"
  // behavior & just use the constraint size.)
  if (MOZ_LIKELY(aObjectFit == NS_STYLE_OBJECT_FIT_FILL) ||
      aIntrinsicRatio.width == 0 ||
      aIntrinsicRatio.height == 0) {
    return aConstraintSize;
  }

  // The type of constraint to compute (cover/contain), if needed:
  Maybe<nsImageRenderer::FitType> fitType;

  Maybe<nsSize> noneSize;
  if (aObjectFit == NS_STYLE_OBJECT_FIT_NONE ||
      aObjectFit == NS_STYLE_OBJECT_FIT_SCALE_DOWN) {
    noneSize = MaybeComputeObjectFitNoneSize(aConstraintSize, aIntrinsicSize,
                                             aIntrinsicRatio);
    if (!noneSize || aObjectFit == NS_STYLE_OBJECT_FIT_SCALE_DOWN) {
      // Need to compute a 'CONTAIN' constraint (either for the 'none' size
      // itself, or for comparison w/ the 'none' size to resolve 'scale-down'.)
      fitType.emplace(nsImageRenderer::CONTAIN);
    }
  } else if (aObjectFit == NS_STYLE_OBJECT_FIT_COVER) {
    fitType.emplace(nsImageRenderer::COVER);
  } else if (aObjectFit == NS_STYLE_OBJECT_FIT_CONTAIN) {
    fitType.emplace(nsImageRenderer::CONTAIN);
  }

  Maybe<nsSize> constrainedSize;
  if (fitType) {
    constrainedSize.emplace(
      nsImageRenderer::ComputeConstrainedSize(aConstraintSize,
                                              aIntrinsicRatio,
                                              *fitType));
  }

  // Now, we should have all the sizing information that we need.
  switch (aObjectFit) {
    // skipping NS_STYLE_OBJECT_FIT_FILL; we handled it w/ early-return.
    case NS_STYLE_OBJECT_FIT_CONTAIN:
    case NS_STYLE_OBJECT_FIT_COVER:
      MOZ_ASSERT(constrainedSize);
      return *constrainedSize;

    case NS_STYLE_OBJECT_FIT_NONE:
      if (noneSize) {
        return *noneSize;
      }
      MOZ_ASSERT(constrainedSize);
      return *constrainedSize;

    case NS_STYLE_OBJECT_FIT_SCALE_DOWN:
      MOZ_ASSERT(constrainedSize);
      if (noneSize) {
        constrainedSize->width =
          std::min(constrainedSize->width, noneSize->width);
        constrainedSize->height =
          std::min(constrainedSize->height, noneSize->height);
      }
      return *constrainedSize;

    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected enum value for 'object-fit'");
      return aConstraintSize; // fall back to (default) 'fill' behavior
  }
}

// (Helper for HasInitialObjectFitAndPosition, to check
// each "object-position" coord.)
static bool
IsCoord50Pct(const mozilla::Position::Coord& aCoord)
{
  return (aCoord.mLength == 0 &&
          aCoord.mHasPercent &&
          aCoord.mPercent == 0.5f);
}

// Indicates whether the given nsStylePosition has the initial values
// for the "object-fit" and "object-position" properties.
static bool
HasInitialObjectFitAndPosition(const nsStylePosition* aStylePos)
{
  const mozilla::Position& objectPos = aStylePos->mObjectPosition;

  return aStylePos->mObjectFit == NS_STYLE_OBJECT_FIT_FILL &&
    IsCoord50Pct(objectPos.mXPosition) &&
    IsCoord50Pct(objectPos.mYPosition);
}

/* static */ nsRect
nsLayoutUtils::ComputeObjectDestRect(const nsRect& aConstraintRect,
                                     const IntrinsicSize& aIntrinsicSize,
                                     const nsSize& aIntrinsicRatio,
                                     const nsStylePosition* aStylePos,
                                     nsPoint* aAnchorPoint)
{
  // Step 1: Figure out our "concrete object size"
  // (the size of the region we'll actually draw our image's pixels into).
  nsSize concreteObjectSize =
    ComputeConcreteObjectSize(aConstraintRect.Size(), aIntrinsicSize,
                              aIntrinsicRatio, aStylePos->mObjectFit);

  // Step 2: Figure out how to align that region in the element's content-box.
  nsPoint imageTopLeftPt, imageAnchorPt;
  nsImageRenderer::ComputeObjectAnchorPoint(aStylePos->mObjectPosition,
                                            aConstraintRect.Size(),
                                            concreteObjectSize,
                                            &imageTopLeftPt, &imageAnchorPt);
  // Right now, we're with respect to aConstraintRect's top-left point.  We add
  // that point here, to convert to the same broader coordinate space that
  // aConstraintRect is in.
  imageTopLeftPt += aConstraintRect.TopLeft();
  imageAnchorPt += aConstraintRect.TopLeft();

  if (aAnchorPoint) {
    // Special-case: if our "object-fit" and "object-position" properties have
    // their default values ("object-fit: fill; object-position:50% 50%"), then
    // we'll override the calculated imageAnchorPt, and instead use the
    // object's top-left corner.
    //
    // This special case is partly for backwards compatibility (since
    // traditionally we've pixel-aligned the top-left corner of e.g. <img>
    // elements), and partly because ComputeSnappedDrawingParameters produces
    // less error if the anchor point is at the top-left corner. So, all other
    // things being equal, we prefer that code path with less error.
    if (HasInitialObjectFitAndPosition(aStylePos)) {
      *aAnchorPoint = imageTopLeftPt;
    } else {
      *aAnchorPoint = imageAnchorPt;
    }
  }
  return nsRect(imageTopLeftPt, concreteObjectSize);
}

already_AddRefed<nsFontMetrics>
nsLayoutUtils::GetFontMetricsForFrame(const nsIFrame* aFrame, float aInflation)
{
  nsStyleContext* styleContext = aFrame->StyleContext();
  uint8_t variantWidth = NS_FONT_VARIANT_WIDTH_NORMAL;
  if (styleContext->IsTextCombined()) {
    MOZ_ASSERT(aFrame->IsTextFrame());
    auto textFrame = static_cast<const nsTextFrame*>(aFrame);
    auto clusters = textFrame->CountGraphemeClusters();
    if (clusters == 2) {
      variantWidth = NS_FONT_VARIANT_WIDTH_HALF;
    } else if (clusters == 3) {
      variantWidth = NS_FONT_VARIANT_WIDTH_THIRD;
    } else if (clusters == 4) {
      variantWidth = NS_FONT_VARIANT_WIDTH_QUARTER;
    }
  }
  return GetFontMetricsForStyleContext(styleContext, aInflation, variantWidth);
}

already_AddRefed<nsFontMetrics>
nsLayoutUtils::GetFontMetricsForStyleContext(nsStyleContext* aStyleContext,
                                             float aInflation,
                                             uint8_t aVariantWidth)
{
  nsPresContext* pc = aStyleContext->PresContext();

  WritingMode wm(aStyleContext);
  const nsStyleFont* styleFont = aStyleContext->StyleFont();
  nsFontMetrics::Params params;
  params.language = styleFont->mLanguage;
  params.explicitLanguage = styleFont->mExplicitLanguage;
  params.orientation =
    wm.IsVertical() && !wm.IsSideways() ? gfxFont::eVertical
                                        : gfxFont::eHorizontal;
  // pass the user font set object into the device context to
  // pass along to CreateFontGroup
  params.userFontSet = pc->GetUserFontSet();
  params.textPerf = pc->GetTextPerfMetrics();

  // When aInflation is 1.0 and we don't require width variant, avoid
  // making a local copy of the nsFont.
  // This also avoids running font.size through floats when it is large,
  // which would be lossy.  Fortunately, in such cases, aInflation is
  // guaranteed to be 1.0f.
  if (aInflation == 1.0f && aVariantWidth == NS_FONT_VARIANT_WIDTH_NORMAL) {
    return pc->DeviceContext()->GetMetricsFor(styleFont->mFont, params);
  }

  nsFont font = styleFont->mFont;
  font.size = NSToCoordRound(font.size * aInflation);
  font.variantWidth = aVariantWidth;
  return pc->DeviceContext()->GetMetricsFor(font, params);
}

nsIFrame*
nsLayoutUtils::FindChildContainingDescendant(nsIFrame* aParent, nsIFrame* aDescendantFrame)
{
  nsIFrame* result = aDescendantFrame;

  while (result) {
    nsIFrame* parent = result->GetParent();
    if (parent == aParent) {
      break;
    }

    // The frame is not an immediate child of aParent so walk up another level
    result = parent;
  }

  return result;
}

nsBlockFrame*
nsLayoutUtils::GetAsBlock(nsIFrame* aFrame)
{
  nsBlockFrame* block = do_QueryFrame(aFrame);
  return block;
}

nsBlockFrame*
nsLayoutUtils::FindNearestBlockAncestor(nsIFrame* aFrame)
{
  nsIFrame* nextAncestor;
  for (nextAncestor = aFrame->GetParent(); nextAncestor;
       nextAncestor = nextAncestor->GetParent()) {
    nsBlockFrame* block = GetAsBlock(nextAncestor);
    if (block)
      return block;
  }
  return nullptr;
}

nsIFrame*
nsLayoutUtils::GetNonGeneratedAncestor(nsIFrame* aFrame)
{
  if (!(aFrame->GetStateBits() & NS_FRAME_GENERATED_CONTENT))
    return aFrame;

  nsIFrame* f = aFrame;
  do {
    f = GetParentOrPlaceholderFor(f);
  } while (f->GetStateBits() & NS_FRAME_GENERATED_CONTENT);
  return f;
}

nsIFrame*
nsLayoutUtils::GetParentOrPlaceholderFor(nsIFrame* aFrame)
{
  if ((aFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW)
      && !aFrame->GetPrevInFlow()) {
    return aFrame->GetProperty(nsIFrame::PlaceholderFrameProperty());
  }
  return aFrame->GetParent();
}

nsIFrame*
nsLayoutUtils::GetParentOrPlaceholderForCrossDoc(nsIFrame* aFrame)
{
  nsIFrame* f = GetParentOrPlaceholderFor(aFrame);
  if (f)
    return f;
  return GetCrossDocParentFrame(aFrame);
}

nsIFrame*
nsLayoutUtils::GetNextContinuationOrIBSplitSibling(nsIFrame *aFrame)
{
  nsIFrame *result = aFrame->GetNextContinuation();
  if (result)
    return result;

  if ((aFrame->GetStateBits() & NS_FRAME_PART_OF_IBSPLIT) != 0) {
    // We only store the ib-split sibling annotation with the first
    // frame in the continuation chain. Walk back to find that frame now.
    aFrame = aFrame->FirstContinuation();

    return aFrame->GetProperty(nsIFrame::IBSplitSibling());
  }

  return nullptr;
}

nsIFrame*
nsLayoutUtils::FirstContinuationOrIBSplitSibling(nsIFrame *aFrame)
{
  nsIFrame *result = aFrame->FirstContinuation();
  if (result->GetStateBits() & NS_FRAME_PART_OF_IBSPLIT) {
    while (true) {
      nsIFrame* f =
        result->GetProperty(nsIFrame::IBSplitPrevSibling());
      if (!f)
        break;
      result = f;
    }
  }

  return result;
}

nsIFrame*
nsLayoutUtils::LastContinuationOrIBSplitSibling(nsIFrame *aFrame)
{
  nsIFrame *result = aFrame->FirstContinuation();
  if (result->GetStateBits() & NS_FRAME_PART_OF_IBSPLIT) {
    while (true) {
      nsIFrame* f = result->GetProperty(nsIFrame::IBSplitSibling());
      if (!f) {
        break;
      }
      result = f;
    }
  }

  result = result->LastContinuation();

  return result;
}

bool
nsLayoutUtils::IsFirstContinuationOrIBSplitSibling(nsIFrame *aFrame)
{
  if (aFrame->GetPrevContinuation()) {
    return false;
  }
  if ((aFrame->GetStateBits() & NS_FRAME_PART_OF_IBSPLIT) &&
      aFrame->GetProperty(nsIFrame::IBSplitPrevSibling())) {
    return false;
  }

  return true;
}

bool
nsLayoutUtils::IsViewportScrollbarFrame(nsIFrame* aFrame)
{
  if (!aFrame)
    return false;

  nsIFrame* rootScrollFrame =
    aFrame->PresContext()->PresShell()->GetRootScrollFrame();
  if (!rootScrollFrame)
    return false;

  nsIScrollableFrame* rootScrollableFrame = do_QueryFrame(rootScrollFrame);
  NS_ASSERTION(rootScrollableFrame, "The root scorollable frame is null");

  if (!IsProperAncestorFrame(rootScrollFrame, aFrame))
    return false;

  nsIFrame* rootScrolledFrame = rootScrollableFrame->GetScrolledFrame();
  return !(rootScrolledFrame == aFrame ||
           IsProperAncestorFrame(rootScrolledFrame, aFrame));
}

// Use only for widths/heights (or their min/max), since it clamps
// negative calc() results to 0.
static bool GetAbsoluteCoord(const nsStyleCoord& aStyle, nscoord& aResult)
{
  if (aStyle.IsCalcUnit()) {
    if (aStyle.CalcHasPercent()) {
      return false;
    }
    // If it has no percents, we can pass 0 for the percentage basis.
    aResult = nsRuleNode::ComputeComputedCalc(aStyle, 0);
    if (aResult < 0)
      aResult = 0;
    return true;
  }

  if (eStyleUnit_Coord != aStyle.GetUnit())
    return false;

  aResult = aStyle.GetCoordValue();
  NS_ASSERTION(aResult >= 0, "negative widths not allowed");
  return true;
}

static nscoord
GetBSizeTakenByBoxSizing(StyleBoxSizing aBoxSizing,
                         nsIFrame* aFrame,
                         bool aHorizontalAxis,
                         bool aIgnorePadding);

// Only call on style coords for which GetAbsoluteCoord returned false.
static bool
GetPercentBSize(const nsStyleCoord& aStyle,
                nsIFrame* aFrame,
                bool aHorizontalAxis,
                nscoord& aResult)
{
  if (eStyleUnit_Percent != aStyle.GetUnit() &&
      !aStyle.IsCalcUnit())
    return false;

  MOZ_ASSERT(!aStyle.IsCalcUnit() || aStyle.CalcHasPercent(),
             "GetAbsoluteCoord should have handled this");

  // During reflow, nsHTMLScrollFrame::ReflowScrolledFrame uses
  // SetComputedHeight on the reflow state for its child to propagate its
  // computed height to the scrolled content. So here we skip to the scroll
  // frame that contains this scrolled content in order to get the same
  // behavior as layout when computing percentage heights.
  nsIFrame *f = aFrame->GetContainingBlock(nsIFrame::SKIP_SCROLLED_FRAME);
  if (!f) {
    NS_NOTREACHED("top of frame tree not a containing block");
    return false;
  }

  WritingMode wm = f->GetWritingMode();

  const nsStylePosition *pos = f->StylePosition();
  const nsStyleCoord& bSizeCoord = pos->BSize(wm);
  nscoord h;
  if (!GetAbsoluteCoord(bSizeCoord, h) &&
      !GetPercentBSize(bSizeCoord, f, aHorizontalAxis, h)) {
    NS_ASSERTION(bSizeCoord.GetUnit() == eStyleUnit_Auto ||
                 bSizeCoord.HasPercent(),
                 "unknown block-size unit");
    LayoutFrameType fType = f->Type();
    if (fType != LayoutFrameType::Viewport &&
        fType != LayoutFrameType::Canvas &&
        fType != LayoutFrameType::PageContent) {
      // There's no basis for the percentage height, so it acts like auto.
      // Should we consider a max-height < min-height pair a basis for
      // percentage heights?  The spec is somewhat unclear, and not doing
      // so is simpler and avoids troubling discontinuities in behavior,
      // so I'll choose not to. -LDB
      return false;
    }

    NS_ASSERTION(bSizeCoord.GetUnit() == eStyleUnit_Auto,
                 "Unexpected block-size unit for viewport or canvas or page-content");
    // For the viewport, canvas, and page-content kids, the percentage
    // basis is just the parent block-size.
    h = f->BSize(wm);
    if (h == NS_UNCONSTRAINEDSIZE) {
      // We don't have a percentage basis after all
      return false;
    }
  }

  const nsStyleCoord& maxBSizeCoord = pos->MaxBSize(wm);

  nscoord maxh;
  if (GetAbsoluteCoord(maxBSizeCoord, maxh) ||
      GetPercentBSize(maxBSizeCoord, f, aHorizontalAxis, maxh)) {
    if (maxh < h)
      h = maxh;
  } else {
    NS_ASSERTION(maxBSizeCoord.GetUnit() == eStyleUnit_None ||
                 maxBSizeCoord.HasPercent(),
                 "unknown max block-size unit");
  }

  const nsStyleCoord& minBSizeCoord = pos->MinBSize(wm);

  nscoord minh;
  if (GetAbsoluteCoord(minBSizeCoord, minh) ||
      GetPercentBSize(minBSizeCoord, f, aHorizontalAxis, minh)) {
    if (minh > h)
      h = minh;
  } else {
    NS_ASSERTION(minBSizeCoord.HasPercent() ||
                 minBSizeCoord.GetUnit() == eStyleUnit_Auto,
                 "unknown min block-size unit");
  }

  // Now adjust h for box-sizing styles on the parent.  We never ignore padding
  // here.  That could conceivably cause some problems with fieldsets (which are
  // the one place that wants to ignore padding), but solving that here without
  // hardcoding a check for f being a fieldset-content frame is a bit of a pain.
  nscoord bSizeTakenByBoxSizing =
    GetBSizeTakenByBoxSizing(pos->mBoxSizing, f, aHorizontalAxis, false);
  h = std::max(0, h - bSizeTakenByBoxSizing);

  if (aStyle.IsCalcUnit()) {
    aResult = std::max(nsRuleNode::ComputeComputedCalc(aStyle, h), 0);
    return true;
  }

  aResult = NSToCoordRound(aStyle.GetPercentValue() * h);
  return true;
}

// Return true if aStyle can be resolved to a definite value and if so
// return that value in aResult.
static bool
GetDefiniteSize(const nsStyleCoord&       aStyle,
                nsIFrame*                 aFrame,
                bool                      aIsInlineAxis,
                const Maybe<LogicalSize>& aPercentageBasis,
                nscoord*                  aResult)
{
  switch (aStyle.GetUnit()) {
    case eStyleUnit_Coord:
      *aResult = aStyle.GetCoordValue();
      return true;
    case eStyleUnit_Percent: {
      if (aPercentageBasis.isNothing()) {
        return false;
      }
      auto wm = aFrame->GetWritingMode();
      nscoord pb = aIsInlineAxis ? aPercentageBasis.value().ISize(wm)
                                 : aPercentageBasis.value().BSize(wm);
      if (pb != NS_UNCONSTRAINEDSIZE) {
        nscoord p = NSToCoordFloorClamped(pb * aStyle.GetPercentValue());
        *aResult = std::max(nscoord(0), p);
        return true;
      }
      return false;
    }
    case eStyleUnit_Calc: {
      nsStyleCoord::Calc* calc = aStyle.GetCalcValue();
      if (calc->mPercent != 0.0f) {
        if (aPercentageBasis.isNothing()) {
          return false;
        }
        auto wm = aFrame->GetWritingMode();
        nscoord pb = aIsInlineAxis ? aPercentageBasis.value().ISize(wm)
                                   : aPercentageBasis.value().BSize(wm);
        if (pb == NS_UNCONSTRAINEDSIZE) {
          // XXXmats given that we're calculating an intrinsic size here,
          // maybe we should back-compute the calc-size using AddPercents?
          return false;
        }
        *aResult = std::max(0, calc->mLength +
                               NSToCoordFloorClamped(pb * calc->mPercent));
      } else {
        *aResult = std::max(0, calc->mLength);
      }
      return true;
    }
    default:
      return false;
  }
}

//
// NOTE: this function will be replaced by GetDefiniteSizeTakenByBoxSizing (bug 1363918).
// Please do not add new uses of this function.
//
// Get the amount of vertical space taken out of aFrame's content area due to
// its borders and paddings given the box-sizing value in aBoxSizing.  We don't
// get aBoxSizing from the frame because some callers want to compute this for
// specific box-sizing values.  aHorizontalAxis is true if our inline direction
// is horisontal and our block direction is vertical.  aIgnorePadding is true if
// padding should be ignored.
static nscoord
GetBSizeTakenByBoxSizing(StyleBoxSizing aBoxSizing,
                         nsIFrame* aFrame,
                         bool aHorizontalAxis,
                         bool aIgnorePadding)
{
  nscoord bSizeTakenByBoxSizing = 0;
  if (aBoxSizing == StyleBoxSizing::Border) {
    const nsStyleBorder* styleBorder = aFrame->StyleBorder();
    bSizeTakenByBoxSizing +=
      aHorizontalAxis ? styleBorder->GetComputedBorder().TopBottom()
                      : styleBorder->GetComputedBorder().LeftRight();
    if (!aIgnorePadding) {
      const nsStyleSides& stylePadding =
        aFrame->StylePadding()->mPadding;
      const nsStyleCoord& paddingStart =
        stylePadding.Get(aHorizontalAxis ? eSideTop : eSideLeft);
      const nsStyleCoord& paddingEnd =
        stylePadding.Get(aHorizontalAxis ? eSideBottom : eSideRight);
      nscoord pad;
      // XXXbz Calling GetPercentBSize on padding values looks bogus, since
      // percent padding is always a percentage of the inline-size of the
      // containing block.  We should perhaps just treat non-absolute paddings
      // here as 0 instead, except that in some cases the width may in fact be
      // known.  See bug 1231059.
      if (GetAbsoluteCoord(paddingStart, pad) ||
          GetPercentBSize(paddingStart, aFrame, aHorizontalAxis, pad)) {
        bSizeTakenByBoxSizing += pad;
      }
      if (GetAbsoluteCoord(paddingEnd, pad) ||
          GetPercentBSize(paddingEnd, aFrame, aHorizontalAxis, pad)) {
        bSizeTakenByBoxSizing += pad;
      }
    }
  }
  return bSizeTakenByBoxSizing;
}

// Get the amount of space taken out of aFrame's content area due to its
// borders and paddings given the box-sizing value in aBoxSizing.  We don't
// get aBoxSizing from the frame because some callers want to compute this for
// specific box-sizing values.
// aIsInlineAxis is true if we're computing for aFrame's inline axis.
// aIgnorePadding is true if padding should be ignored.
static nscoord
GetDefiniteSizeTakenByBoxSizing(StyleBoxSizing aBoxSizing,
                                nsIFrame* aFrame,
                                bool aIsInlineAxis,
                                bool aIgnorePadding,
                                const Maybe<LogicalSize>& aPercentageBasis)
{
  nscoord sizeTakenByBoxSizing = 0;
  if (MOZ_UNLIKELY(aBoxSizing == StyleBoxSizing::Border)) {
    const bool isHorizontalAxis =
      aIsInlineAxis == !aFrame->GetWritingMode().IsVertical();
    const nsStyleBorder* styleBorder = aFrame->StyleBorder();
    sizeTakenByBoxSizing =
      isHorizontalAxis ? styleBorder->GetComputedBorder().LeftRight()
                       : styleBorder->GetComputedBorder().TopBottom();
    if (!aIgnorePadding) {
      const nsStyleSides& stylePadding = aFrame->StylePadding()->mPadding;
      const nsStyleCoord& pStart =
        stylePadding.Get(isHorizontalAxis ? eSideLeft : eSideTop);
      const nsStyleCoord& pEnd =
        stylePadding.Get(isHorizontalAxis ? eSideRight : eSideBottom);
      nscoord pad;
      // XXXbz Calling GetPercentBSize on padding values looks bogus, since
      // percent padding is always a percentage of the inline-size of the
      // containing block.  We should perhaps just treat non-absolute paddings
      // here as 0 instead, except that in some cases the width may in fact be
      // known.  See bug 1231059.
      if (GetDefiniteSize(pStart, aFrame, aIsInlineAxis, aPercentageBasis, &pad) ||
          (aPercentageBasis.isNothing() &&
           GetPercentBSize(pStart, aFrame, isHorizontalAxis, pad))) {
        sizeTakenByBoxSizing += pad;
      }
      if (GetDefiniteSize(pEnd, aFrame, aIsInlineAxis, aPercentageBasis, &pad) ||
          (aPercentageBasis.isNothing() &&
           GetPercentBSize(pEnd, aFrame, isHorizontalAxis, pad))) {
        sizeTakenByBoxSizing += pad;
      }
    }
  }
  return sizeTakenByBoxSizing;
}

// Handles only -moz-max-content and -moz-min-content, and
// -moz-fit-content for min-width and max-width, since the others
// (-moz-fit-content for width, and -moz-available) have no effect on
// intrinsic widths.
enum eWidthProperty { PROP_WIDTH, PROP_MAX_WIDTH, PROP_MIN_WIDTH };
static bool
GetIntrinsicCoord(const nsStyleCoord& aStyle,
                  gfxContext* aRenderingContext,
                  nsIFrame* aFrame,
                  eWidthProperty aProperty,
                  nscoord& aResult)
{
  NS_PRECONDITION(aProperty == PROP_WIDTH || aProperty == PROP_MAX_WIDTH ||
                  aProperty == PROP_MIN_WIDTH, "unexpected property");
  if (aStyle.GetUnit() != eStyleUnit_Enumerated)
    return false;
  int32_t val = aStyle.GetIntValue();
  NS_ASSERTION(val == NS_STYLE_WIDTH_MAX_CONTENT ||
               val == NS_STYLE_WIDTH_MIN_CONTENT ||
               val == NS_STYLE_WIDTH_FIT_CONTENT ||
               val == NS_STYLE_WIDTH_AVAILABLE,
               "unexpected enumerated value for width property");
  if (val == NS_STYLE_WIDTH_AVAILABLE)
    return false;
  if (val == NS_STYLE_WIDTH_FIT_CONTENT) {
    if (aProperty == PROP_WIDTH)
      return false; // handle like 'width: auto'
    if (aProperty == PROP_MAX_WIDTH)
      // constrain large 'width' values down to -moz-max-content
      val = NS_STYLE_WIDTH_MAX_CONTENT;
    else
      // constrain small 'width' or 'max-width' values up to -moz-min-content
      val = NS_STYLE_WIDTH_MIN_CONTENT;
  }

  NS_ASSERTION(val == NS_STYLE_WIDTH_MAX_CONTENT ||
               val == NS_STYLE_WIDTH_MIN_CONTENT,
               "should have reduced everything remaining to one of these");

  // If aFrame is a container for font size inflation, then shrink
  // wrapping inside of it should not apply font size inflation.
  AutoMaybeDisableFontInflation an(aFrame);

  if (val == NS_STYLE_WIDTH_MAX_CONTENT)
    aResult = aFrame->GetPrefISize(aRenderingContext);
  else
    aResult = aFrame->GetMinISize(aRenderingContext);
  return true;
}

#undef  DEBUG_INTRINSIC_WIDTH

#ifdef DEBUG_INTRINSIC_WIDTH
static int32_t gNoiseIndent = 0;
#endif

// Return true for form controls whose minimum intrinsic inline-size
// shrinks to 0 when they have a percentage inline-size (but not
// percentage max-inline-size).  (Proper replaced elements, whose
// intrinsic minimium inline-size shrinks to 0 for both percentage
// inline-size and percentage max-inline-size, are handled elsewhere.)
inline static bool
FormControlShrinksForPercentISize(nsIFrame* aFrame)
{
  if (!aFrame->IsFrameOfType(nsIFrame::eReplaced)) {
    // Quick test to reject most frames.
    return false;
  }

  LayoutFrameType fType = aFrame->Type();
  if (fType == LayoutFrameType::Meter || fType == LayoutFrameType::Progress) {
    // progress and meter do have this shrinking behavior
    // FIXME: Maybe these should be nsIFormControlFrame?
    return true;
  }

  if (!static_cast<nsIFormControlFrame*>(do_QueryFrame(aFrame))) {
    // Not a form control.  This includes fieldsets, which do not
    // shrink.
    return false;
  }

  if (fType == LayoutFrameType::GfxButtonControl ||
      fType == LayoutFrameType::HTMLButtonControl) {
    // Buttons don't have this shrinking behavior.  (Note that color
    // inputs do, even though they inherit from button, so we can't use
    // do_QueryFrame here.)
    return false;
  }

  return true;
}

/**
 * Add aOffsets which describes what to add on outside of the content box
 * aContentSize (controlled by 'box-sizing') and apply min/max properties.
 * We have to account for these properties after getting all the offsets
 * (margin, border, padding) because percentages do not operate linearly.
 * Doing this is ok because although percentages aren't handled linearly,
 * they are handled monotonically.
 *
 * @param aContentSize the content size calculated so far
                       (@see IntrinsicForContainer)
 * @param aContentMinSize ditto min content size
 * @param aStyleSize a 'width' or 'height' property value
 * @param aFixedMinSize if aStyleMinSize is a definite size then this points to
 *                      the value, otherwise nullptr
 * @param aStyleMinSize a 'min-width' or 'min-height' property value
 * @param aFixedMaxSize if aStyleMaxSize is a definite size then this points to
 *                      the value, otherwise nullptr
 * @param aStyleMaxSize a 'max-width' or 'max-height' property value
 * @param aFlags same as for IntrinsicForContainer
 * @param aContainerWM the container's WM
 */
static nscoord
AddIntrinsicSizeOffset(gfxContext* aRenderingContext,
                       nsIFrame* aFrame,
                       const nsIFrame::IntrinsicISizeOffsetData& aOffsets,
                       nsLayoutUtils::IntrinsicISizeType aType,
                       StyleBoxSizing aBoxSizing,
                       nscoord aContentSize,
                       nscoord aContentMinSize,
                       const nsStyleCoord& aStyleSize,
                       const nscoord* aFixedMinSize,
                       const nsStyleCoord& aStyleMinSize,
                       const nscoord* aFixedMaxSize,
                       const nsStyleCoord& aStyleMaxSize,
                       uint32_t aFlags,
                       PhysicalAxis aAxis)
{
  nscoord result = aContentSize;
  nscoord min = aContentMinSize;
  nscoord coordOutsideSize = 0;
  float pctOutsideSize = 0;
  float pctTotal = 0.0f;

  if (!(aFlags & nsLayoutUtils::IGNORE_PADDING)) {
    coordOutsideSize += aOffsets.hPadding;
    pctOutsideSize += aOffsets.hPctPadding;
  }

  coordOutsideSize += aOffsets.hBorder;

  if (aBoxSizing == StyleBoxSizing::Border) {
    min += coordOutsideSize;
    result = NSCoordSaturatingAdd(result, coordOutsideSize);
    pctTotal += pctOutsideSize;

    coordOutsideSize = 0;
    pctOutsideSize = 0.0f;
  }

  coordOutsideSize += aOffsets.hMargin;
  pctOutsideSize += aOffsets.hPctMargin;

  min += coordOutsideSize;
  result = NSCoordSaturatingAdd(result, coordOutsideSize);
  pctTotal += pctOutsideSize;

  const bool shouldAddPercent = aType == nsLayoutUtils::PREF_ISIZE ||
                                (aFlags & nsLayoutUtils::ADD_PERCENTS);
  nscoord size;
  if (aType == nsLayoutUtils::MIN_ISIZE &&
      (((aStyleSize.HasPercent() || aStyleMaxSize.HasPercent()) &&
        aFrame->IsFrameOfType(nsIFrame::eReplacedSizing)) ||
       (aStyleSize.HasPercent() &&
        FormControlShrinksForPercentISize(aFrame)))) {
    // A percentage width or max-width on replaced elements means they
    // can shrink to 0.
    // This is also true for percentage widths (but not max-widths) on
    // text inputs.
    // Note that if this is max-width, this overrides the fixed-width
    // rule in the next condition.
    result = 0; // let |min| handle padding/border/margin
  } else if (GetAbsoluteCoord(aStyleSize, size) ||
             GetIntrinsicCoord(aStyleSize, aRenderingContext, aFrame,
                               PROP_WIDTH, size)) {
    result = size + coordOutsideSize;
    if (shouldAddPercent) {
      result = nsLayoutUtils::AddPercents(result, pctOutsideSize);
    }
  } else {
    // NOTE: We could really do a lot better for percents and for some
    // cases of calc() containing percent (certainly including any where
    // the coefficient on the percent is positive and there are no max()
    // expressions).  However, doing better for percents wouldn't be
    // backwards compatible.
    if (shouldAddPercent) {
      result = nsLayoutUtils::AddPercents(result, pctTotal);
    }
  }

  nscoord maxSize = aFixedMaxSize ? *aFixedMaxSize : 0;
  if (aFixedMaxSize ||
      GetIntrinsicCoord(aStyleMaxSize, aRenderingContext, aFrame,
                        PROP_MAX_WIDTH, maxSize)) {
    maxSize += coordOutsideSize;
    if (shouldAddPercent) {
      maxSize = nsLayoutUtils::AddPercents(maxSize, pctOutsideSize);
    }
    if (result > maxSize) {
      result = maxSize;
    }
  }

  nscoord minSize = aFixedMinSize ? *aFixedMinSize : 0;
  if (aFixedMinSize ||
      GetIntrinsicCoord(aStyleMinSize, aRenderingContext, aFrame,
                        PROP_MIN_WIDTH, minSize)) {
    minSize += coordOutsideSize;
    if (shouldAddPercent) {
      minSize = nsLayoutUtils::AddPercents(minSize, pctOutsideSize);
    }
    if (result < minSize) {
      result = minSize;
    }
  }

  if (shouldAddPercent) {
    min = nsLayoutUtils::AddPercents(min, pctTotal);
  }
  if (result < min) {
    result = min;
  }

  const nsStyleDisplay* disp = aFrame->StyleDisplay();
  if (aFrame->IsThemed(disp)) {
    LayoutDeviceIntSize devSize;
    bool canOverride = true;
    nsPresContext* pc = aFrame->PresContext();
    pc->GetTheme()->GetMinimumWidgetSize(pc, aFrame, disp->mAppearance,
                                         &devSize, &canOverride);
    nscoord themeSize =
      pc->DevPixelsToAppUnits(aAxis == eAxisVertical ? devSize.height
                                                     : devSize.width);
    // GetMinimumWidgetSize() returns a border-box width.
    themeSize += aOffsets.hMargin;
    if (shouldAddPercent) {
      themeSize = nsLayoutUtils::AddPercents(themeSize, aOffsets.hPctMargin);
    }
    if (themeSize > result || !canOverride) {
      result = themeSize;
    }
  }
  return result;
}

static void
AddStateBitToAncestors(nsIFrame* aFrame, nsFrameState aBit)
{
  for (nsIFrame* f = aFrame; f; f = f->GetParent()) {
    if (f->HasAnyStateBits(aBit)) {
      break;
    }
    f->AddStateBits(aBit);
  }
}

/* static */ nscoord
nsLayoutUtils::IntrinsicForAxis(PhysicalAxis              aAxis,
                                gfxContext*               aRenderingContext,
                                nsIFrame*                 aFrame,
                                IntrinsicISizeType        aType,
                                const Maybe<LogicalSize>& aPercentageBasis,
                                uint32_t                  aFlags,
                                nscoord                   aMarginBoxMinSizeClamp)
{
  NS_PRECONDITION(aFrame, "null frame");
  NS_PRECONDITION(aFrame->GetParent(),
                  "IntrinsicForAxis called on frame not in tree");
  NS_PRECONDITION(aType == MIN_ISIZE || aType == PREF_ISIZE, "bad type");
  MOZ_ASSERT(aFrame->GetParent()->Type() != LayoutFrameType::GridContainer ||
             aPercentageBasis.isSome(),
             "grid layout should always pass a percentage basis");

  const bool horizontalAxis = MOZ_LIKELY(aAxis == eAxisHorizontal);
#ifdef DEBUG_INTRINSIC_WIDTH
  nsFrame::IndentBy(stderr, gNoiseIndent);
  static_cast<nsFrame*>(aFrame)->ListTag(stderr);
  printf_stderr(" %s %s intrinsic size for container:\n",
                aType == MIN_ISIZE ? "min" : "pref",
                horizontalAxis ? "horizontal" : "vertical");
#endif

  // If aFrame is a container for font size inflation, then shrink
  // wrapping inside of it should not apply font size inflation.
  AutoMaybeDisableFontInflation an(aFrame);

  // We want the size this frame will contribute to the parent's inline-size,
  // so we work in the parent's writing mode; but if aFrame is orthogonal to
  // its parent, we'll need to look at its BSize instead of min/pref-ISize.
  const nsStylePosition* stylePos = aFrame->StylePosition();
  StyleBoxSizing boxSizing = stylePos->mBoxSizing;

  const nsStyleCoord& styleMinISize =
    horizontalAxis ? stylePos->mMinWidth : stylePos->mMinHeight;
  const nsStyleCoord& styleISize =
    (aFlags & MIN_INTRINSIC_ISIZE) ? styleMinISize :
    (horizontalAxis ? stylePos->mWidth : stylePos->mHeight);
  MOZ_ASSERT(!(aFlags & MIN_INTRINSIC_ISIZE) ||
             styleISize.GetUnit() == eStyleUnit_Auto ||
             styleISize.GetUnit() == eStyleUnit_Enumerated,
             "should only use MIN_INTRINSIC_ISIZE for intrinsic values");
  const nsStyleCoord& styleMaxISize =
    horizontalAxis ? stylePos->mMaxWidth : stylePos->mMaxHeight;

  // We build up two values starting with the content box, and then
  // adding padding, border and margin.  The result is normally
  // |result|.  Then, when we handle 'width', 'min-width', and
  // 'max-width', we use the results we've been building in |min| as a
  // minimum, overriding 'min-width'.  This ensures two things:
  //   * that we don't let a value of 'box-sizing' specifying a width
  //     smaller than the padding/border inside the box-sizing box give
  //     a content width less than zero
  //   * that we prevent tables from becoming smaller than their
  //     intrinsic minimum width
  nscoord result = 0, min = 0;

  nscoord maxISize;
  bool haveFixedMaxISize = GetAbsoluteCoord(styleMaxISize, maxISize);
  nscoord minISize;

  // Treat "min-width: auto" as 0.
  bool haveFixedMinISize;
  if (eStyleUnit_Auto == styleMinISize.GetUnit()) {
    // NOTE: Technically, "auto" is supposed to behave like "min-content" on
    // flex items. However, we don't need to worry about that here, because
    // flex items' min-sizes are intentionally ignored until the flex
    // container explicitly considers them during space distribution.
    minISize = 0;
    haveFixedMinISize = true;
  } else {
    haveFixedMinISize = GetAbsoluteCoord(styleMinISize, minISize);
  }

  PhysicalAxis ourInlineAxis =
    aFrame->GetWritingMode().PhysicalAxis(eLogicalAxisInline);
  const bool isInlineAxis = aAxis == ourInlineAxis;
  // If we have a specified width (or a specified 'min-width' greater
  // than the specified 'max-width', which works out to the same thing),
  // don't even bother getting the frame's intrinsic width, because in
  // this case GetAbsoluteCoord(styleISize, w) will always succeed, so
  // we'll never need the intrinsic dimensions.
  if (styleISize.GetUnit() == eStyleUnit_Enumerated &&
      (styleISize.GetIntValue() == NS_STYLE_WIDTH_MAX_CONTENT ||
       styleISize.GetIntValue() == NS_STYLE_WIDTH_MIN_CONTENT)) {
    // -moz-fit-content and -moz-available enumerated widths compute intrinsic
    // widths just like auto.
    // For -moz-max-content and -moz-min-content, we handle them like
    // specified widths, but ignore box-sizing.
    boxSizing = StyleBoxSizing::Content;
    if (aMarginBoxMinSizeClamp != NS_MAXSIZE &&
        styleISize.GetIntValue() == NS_STYLE_WIDTH_MIN_CONTENT) {
      // We need |result| to be the 'min-content size' for the clamping below.
      result = aFrame->GetMinISize(aRenderingContext);
    }
  } else if (!styleISize.ConvertsToLength() &&
             !(haveFixedMinISize && haveFixedMaxISize && maxISize <= minISize)) {
#ifdef DEBUG_INTRINSIC_WIDTH
    ++gNoiseIndent;
#endif
    if (aType != MIN_ISIZE) {
      // At this point, |styleISize| is auto/-moz-fit-content/-moz-available or
      // has a percentage.  The intrinisic size for those under a max-content
      // constraint is the max-content contribution which we shouldn't clamp.
      aMarginBoxMinSizeClamp = NS_MAXSIZE;
    }
    if (MOZ_UNLIKELY(!isInlineAxis)) {
      IntrinsicSize intrinsicSize = aFrame->GetIntrinsicSize();
      const nsStyleCoord intrinsicBCoord =
        horizontalAxis ? intrinsicSize.width : intrinsicSize.height;
      if (intrinsicBCoord.GetUnit() == eStyleUnit_Coord) {
        result = intrinsicBCoord.GetCoordValue();
      } else {
        // We don't have an intrinsic bsize and we need aFrame's block-dir size.
        if (aFlags & BAIL_IF_REFLOW_NEEDED) {
          return NS_INTRINSIC_WIDTH_UNKNOWN;
        }
        // XXX Unfortunately, we probably don't know this yet, so this is wrong...
        // but it's not clear what we should do. If aFrame's inline size hasn't
        // been determined yet, we can't necessarily figure out its block size
        // either. For now, authors who put orthogonal elements into things like
        // buttons or table cells may have to explicitly provide sizes rather
        // than expecting intrinsic sizing to work "perfectly" in underspecified
        // cases.
        result = aFrame->BSize();
      }
    } else {
      result = aType == MIN_ISIZE
               ? aFrame->GetMinISize(aRenderingContext)
               : aFrame->GetPrefISize(aRenderingContext);
    }
#ifdef DEBUG_INTRINSIC_WIDTH
    --gNoiseIndent;
    nsFrame::IndentBy(stderr, gNoiseIndent);
    static_cast<nsFrame*>(aFrame)->ListTag(stderr);
    printf_stderr(" %s %s intrinsic size from frame is %d.\n",
                  aType == MIN_ISIZE ? "min" : "pref",
                  horizontalAxis ? "horizontal" : "vertical",
                  result);
#endif

    // Handle elements with an intrinsic ratio (or size) and a specified
    // height, min-height, or max-height.
    // NOTE: We treat "min-height:auto" as "0" for the purpose of this code,
    // since that's what it means in all cases except for on flex items -- and
    // even there, we're supposed to ignore it (i.e. treat it as 0) until the
    // flex container explicitly considers it.
    const nsStyleCoord& styleBSize =
      horizontalAxis ? stylePos->mHeight : stylePos->mWidth;
    const nsStyleCoord& styleMinBSize =
      horizontalAxis ? stylePos->mMinHeight : stylePos->mMinWidth;
    const nsStyleCoord& styleMaxBSize =
      horizontalAxis ? stylePos->mMaxHeight : stylePos->mMaxWidth;

    if (styleBSize.GetUnit() != eStyleUnit_Auto ||
        !(styleMinBSize.GetUnit() == eStyleUnit_Auto ||
          (styleMinBSize.GetUnit() == eStyleUnit_Coord &&
           styleMinBSize.GetCoordValue() == 0)) ||
        styleMaxBSize.GetUnit() != eStyleUnit_None) {

      nsSize ratio(aFrame->GetIntrinsicRatio());
      nscoord ratioISize = (horizontalAxis ? ratio.width  : ratio.height);
      nscoord ratioBSize = (horizontalAxis ? ratio.height : ratio.width);
      if (ratioBSize != 0) {
        AddStateBitToAncestors(aFrame,
            NS_FRAME_DESCENDANT_INTRINSIC_ISIZE_DEPENDS_ON_BSIZE);

        nscoord bSizeTakenByBoxSizing =
          GetDefiniteSizeTakenByBoxSizing(boxSizing, aFrame, !isInlineAxis,
                                          aFlags & IGNORE_PADDING,
                                          aPercentageBasis);
        // NOTE: This is only the minContentSize if we've been passed MIN_INTRINSIC_ISIZE
        // (which is fine, because this should only be used inside a check for that flag).
        nscoord minContentSize = result;
        nscoord h;
        if (GetDefiniteSize(styleBSize, aFrame, !isInlineAxis, aPercentageBasis, &h) ||
            (aPercentageBasis.isNothing() &&
             GetPercentBSize(styleBSize, aFrame, horizontalAxis, h))) {
          h = std::max(0, h - bSizeTakenByBoxSizing);
          result = NSCoordMulDiv(h, ratioISize, ratioBSize);
        }

        if (GetDefiniteSize(styleMaxBSize, aFrame, !isInlineAxis, aPercentageBasis, &h) ||
            (aPercentageBasis.isNothing() &&
             GetPercentBSize(styleMaxBSize, aFrame, horizontalAxis, h))) {
          h = std::max(0, h - bSizeTakenByBoxSizing);
          nscoord maxISize = NSCoordMulDiv(h, ratioISize, ratioBSize);
          if (maxISize < result) {
            result = maxISize;
          }
          if (maxISize < minContentSize) {
            minContentSize = maxISize;
          }
        }

        if (GetDefiniteSize(styleMinBSize, aFrame, !isInlineAxis, aPercentageBasis, &h) ||
            (aPercentageBasis.isNothing() &&
             GetPercentBSize(styleMinBSize, aFrame, horizontalAxis, h))) {
          h = std::max(0, h - bSizeTakenByBoxSizing);
          nscoord minISize = NSCoordMulDiv(h, ratioISize, ratioBSize);
          if (minISize > result) {
            result = minISize;
          }
          if (minISize > minContentSize) {
            minContentSize = minISize;
          }
        }
        if (MOZ_UNLIKELY(aFlags & nsLayoutUtils::MIN_INTRINSIC_ISIZE)) {
          // This is the 'min-width/height:auto' "transferred size" piece of:
          // https://www.w3.org/TR/css-flexbox-1/#min-width-automatic-minimum-size
          // https://drafts.csswg.org/css-grid/#min-size-auto
          result = std::min(result, minContentSize);
        }
      }
    }
  }

  if (aFrame->IsTableFrame()) {
    // Tables can't shrink smaller than their intrinsic minimum width,
    // no matter what.
    min = aFrame->GetMinISize(aRenderingContext);
  }

  nsIFrame::IntrinsicISizeOffsetData offsets =
    MOZ_LIKELY(isInlineAxis) ? aFrame->IntrinsicISizeOffsets()
                             : aFrame->IntrinsicBSizeOffsets();
  nscoord contentBoxSize = result;
  result = AddIntrinsicSizeOffset(aRenderingContext, aFrame, offsets, aType,
                                  boxSizing, result, min, styleISize,
                                  haveFixedMinISize ? &minISize : nullptr,
                                  styleMinISize,
                                  haveFixedMaxISize ? &maxISize : nullptr,
                                  styleMaxISize,
                                  aFlags, aAxis);
  nscoord overflow = result - aMarginBoxMinSizeClamp;
  if (MOZ_UNLIKELY(overflow > 0)) {
    nscoord newContentBoxSize = std::max(nscoord(0), contentBoxSize - overflow);
    result -= contentBoxSize - newContentBoxSize;
  }

#ifdef DEBUG_INTRINSIC_WIDTH
  nsFrame::IndentBy(stderr, gNoiseIndent);
  static_cast<nsFrame*>(aFrame)->ListTag(stderr);
  printf_stderr(" %s %s intrinsic size for container is %d twips.\n",
                aType == MIN_ISIZE ? "min" : "pref",
                horizontalAxis ? "horizontal" : "vertical",
                result);
#endif

  return result;
}

/* static */ nscoord
nsLayoutUtils::IntrinsicForContainer(gfxContext* aRenderingContext,
                                     nsIFrame* aFrame,
                                     IntrinsicISizeType aType,
                                     uint32_t aFlags)
{
  MOZ_ASSERT(aFrame && aFrame->GetParent());
  // We want the size aFrame will contribute to its parent's inline-size.
  PhysicalAxis axis =
    aFrame->GetParent()->GetWritingMode().PhysicalAxis(eLogicalAxisInline);
  return IntrinsicForAxis(axis, aRenderingContext, aFrame, aType, Nothing(), aFlags);
}

/* static */ nscoord
nsLayoutUtils::MinSizeContributionForAxis(PhysicalAxis        aAxis,
                                          gfxContext*         aRC,
                                          nsIFrame*           aFrame,
                                          IntrinsicISizeType  aType,
                                          uint32_t            aFlags)
{
  MOZ_ASSERT(aFrame);
  MOZ_ASSERT(aFrame->IsFlexOrGridItem(),
             "only grid/flex items have this behavior currently");

#ifdef DEBUG_INTRINSIC_WIDTH
  nsFrame::IndentBy(stderr, gNoiseIndent);
  static_cast<nsFrame*>(aFrame)->ListTag(stderr);
  printf_stderr(" %s min-isize for %s WM:\n",
                aType == MIN_ISIZE ? "min" : "pref",
                aWM.IsVertical() ? "vertical" : "horizontal");
#endif

  // Note: this method is only meant for grid/flex items which always
  // include percentages in their intrinsic size.
  aFlags |= nsLayoutUtils::ADD_PERCENTS;
  const nsStylePosition* const stylePos = aFrame->StylePosition();
  const nsStyleCoord* style = aAxis == eAxisHorizontal ? &stylePos->mMinWidth
                                                       : &stylePos->mMinHeight;
  nscoord minSize;
  nscoord* fixedMinSize = nullptr;
  auto minSizeUnit = style->GetUnit();
  if (minSizeUnit == eStyleUnit_Auto) {
    if (aFrame->StyleDisplay()->mOverflowX == NS_STYLE_OVERFLOW_VISIBLE) {
      style = aAxis == eAxisHorizontal ? &stylePos->mWidth
                                       : &stylePos->mHeight;
      if (GetAbsoluteCoord(*style, minSize)) {
        // We have a definite width/height.  This is the "specified size" in:
        // https://drafts.csswg.org/css-grid/#min-size-auto
        fixedMinSize = &minSize;
      }
      // fall through - the caller will have to deal with "transferred size"
    } else {
      // min-[width|height]:auto with overflow != visible computes to zero.
      minSize = 0;
      fixedMinSize = &minSize;
    }
  } else if (GetAbsoluteCoord(*style, minSize)) {
    fixedMinSize = &minSize;
  } else if (minSizeUnit != eStyleUnit_Enumerated) {
    MOZ_ASSERT(style->HasPercent());
    minSize = 0;
    fixedMinSize = &minSize;
  }

  if (!fixedMinSize) {
    // Let the caller deal with the "content size" cases.
#ifdef DEBUG_INTRINSIC_WIDTH
    nsFrame::IndentBy(stderr, gNoiseIndent);
    static_cast<nsFrame*>(aFrame)->ListTag(stderr);
    printf_stderr(" %s min-isize is indefinite.\n",
                  aType == MIN_ISIZE ? "min" : "pref");
#endif
    return NS_UNCONSTRAINEDSIZE;
  }

  // If aFrame is a container for font size inflation, then shrink
  // wrapping inside of it should not apply font size inflation.
  AutoMaybeDisableFontInflation an(aFrame);

  PhysicalAxis ourInlineAxis =
    aFrame->GetWritingMode().PhysicalAxis(eLogicalAxisInline);
  nsIFrame::IntrinsicISizeOffsetData offsets =
    ourInlineAxis == aAxis ? aFrame->IntrinsicISizeOffsets()
                           : aFrame->IntrinsicBSizeOffsets();
  nscoord result = 0;
  nscoord min = 0;

  const nsStyleCoord& maxISize =
    aAxis == eAxisHorizontal ? stylePos->mMaxWidth : stylePos->mMaxHeight;
  result = AddIntrinsicSizeOffset(aRC, aFrame, offsets, aType,
                                  stylePos->mBoxSizing,
                                  result, min, *style, fixedMinSize,
                                  *style, nullptr, maxISize, aFlags, aAxis);

#ifdef DEBUG_INTRINSIC_WIDTH
  nsFrame::IndentBy(stderr, gNoiseIndent);
  static_cast<nsFrame*>(aFrame)->ListTag(stderr);
  printf_stderr(" %s min-isize is %d twips.\n",
         aType == MIN_ISIZE ? "min" : "pref", result);
#endif

  return result;
}

/* static */ nscoord
nsLayoutUtils::ComputeCBDependentValue(nscoord aPercentBasis,
                                       const nsStyleCoord& aCoord)
{
  NS_WARNING_ASSERTION(
    aPercentBasis != NS_UNCONSTRAINEDSIZE,
    "have unconstrained width or height; this should only result from very "
    "large sizes, not attempts at intrinsic size calculation");

  if (aCoord.IsCoordPercentCalcUnit()) {
    return nsRuleNode::ComputeCoordPercentCalc(aCoord, aPercentBasis);
  }
  NS_ASSERTION(aCoord.GetUnit() == eStyleUnit_None ||
               aCoord.GetUnit() == eStyleUnit_Auto,
               "unexpected width value");
  return 0;
}

/* static */ nscoord
nsLayoutUtils::ComputeBSizeDependentValue(
                 nscoord              aContainingBlockBSize,
                 const nsStyleCoord&  aCoord)
{
  // XXXldb Some callers explicitly check aContainingBlockBSize
  // against NS_AUTOHEIGHT *and* unit against eStyleUnit_Percent or
  // calc()s containing percents before calling this function.
  // However, it would be much more likely to catch problems without
  // the unit conditions.
  // XXXldb Many callers pass a non-'auto' containing block height when
  // according to CSS2.1 they should be passing 'auto'.
  NS_PRECONDITION(NS_AUTOHEIGHT != aContainingBlockBSize ||
                  !aCoord.HasPercent(),
                  "unexpected containing block block-size");

  if (aCoord.IsCoordPercentCalcUnit()) {
    return nsRuleNode::ComputeCoordPercentCalc(aCoord, aContainingBlockBSize);
  }

  NS_ASSERTION(aCoord.GetUnit() == eStyleUnit_None ||
               aCoord.GetUnit() == eStyleUnit_Auto,
               "unexpected block-size value");
  return 0;
}

/* static */ void
nsLayoutUtils::MarkDescendantsDirty(nsIFrame *aSubtreeRoot)
{
  AutoTArray<nsIFrame*, 4> subtrees;
  subtrees.AppendElement(aSubtreeRoot);

  // dirty descendants, iterating over subtrees that may include
  // additional subtrees associated with placeholders
  do {
    nsIFrame *subtreeRoot = subtrees.ElementAt(subtrees.Length() - 1);
    subtrees.RemoveElementAt(subtrees.Length() - 1);

    // Mark all descendants dirty (using an nsTArray stack rather than
    // recursion).
    // Note that ReflowInput::InitResizeFlags has some similar
    // code; see comments there for how and why it differs.
    AutoTArray<nsIFrame*, 32> stack;
    stack.AppendElement(subtreeRoot);

    do {
      nsIFrame *f = stack.ElementAt(stack.Length() - 1);
      stack.RemoveElementAt(stack.Length() - 1);

      f->MarkIntrinsicISizesDirty();

      if (f->IsPlaceholderFrame()) {
        nsIFrame *oof = nsPlaceholderFrame::GetRealFrameForPlaceholder(f);
        if (!nsLayoutUtils::IsProperAncestorFrame(subtreeRoot, oof)) {
          // We have another distinct subtree we need to mark.
          subtrees.AppendElement(oof);
        }
      }

      nsIFrame::ChildListIterator lists(f);
      for (; !lists.IsDone(); lists.Next()) {
        nsFrameList::Enumerator childFrames(lists.CurrentList());
        for (; !childFrames.AtEnd(); childFrames.Next()) {
          nsIFrame* kid = childFrames.get();
          stack.AppendElement(kid);
        }
      }
    } while (stack.Length() != 0);
  } while (subtrees.Length() != 0);
}

/* static */
void
nsLayoutUtils::MarkIntrinsicISizesDirtyIfDependentOnBSize(nsIFrame* aFrame)
{
  AutoTArray<nsIFrame*, 32> stack;
  stack.AppendElement(aFrame);

  do {
    nsIFrame* f = stack.ElementAt(stack.Length() - 1);
    stack.RemoveElementAt(stack.Length() - 1);

    if (!f->HasAnyStateBits(
        NS_FRAME_DESCENDANT_INTRINSIC_ISIZE_DEPENDS_ON_BSIZE)) {
      continue;
    }
    f->MarkIntrinsicISizesDirty();

    for (nsIFrame::ChildListIterator lists(f); !lists.IsDone(); lists.Next()) {
      for (nsIFrame* kid : lists.CurrentList()) {
        stack.AppendElement(kid);
      }
    }
  } while (stack.Length() != 0);
}

nsSize
nsLayoutUtils::ComputeAutoSizeWithIntrinsicDimensions(nscoord minWidth, nscoord minHeight,
                                                      nscoord maxWidth, nscoord maxHeight,
                                                      nscoord tentWidth, nscoord tentHeight)
{
  // Now apply min/max-width/height - CSS 2.1 sections 10.4 and 10.7:

  if (minWidth > maxWidth)
    maxWidth = minWidth;
  if (minHeight > maxHeight)
    maxHeight = minHeight;

  nscoord heightAtMaxWidth, heightAtMinWidth,
          widthAtMaxHeight, widthAtMinHeight;

  if (tentWidth > 0) {
    heightAtMaxWidth = NSCoordMulDiv(maxWidth, tentHeight, tentWidth);
    if (heightAtMaxWidth < minHeight)
      heightAtMaxWidth = minHeight;
    heightAtMinWidth = NSCoordMulDiv(minWidth, tentHeight, tentWidth);
    if (heightAtMinWidth > maxHeight)
      heightAtMinWidth = maxHeight;
  } else {
    heightAtMaxWidth = heightAtMinWidth = NS_CSS_MINMAX(tentHeight, minHeight, maxHeight);
  }

  if (tentHeight > 0) {
    widthAtMaxHeight = NSCoordMulDiv(maxHeight, tentWidth, tentHeight);
    if (widthAtMaxHeight < minWidth)
      widthAtMaxHeight = minWidth;
    widthAtMinHeight = NSCoordMulDiv(minHeight, tentWidth, tentHeight);
    if (widthAtMinHeight > maxWidth)
      widthAtMinHeight = maxWidth;
  } else {
    widthAtMaxHeight = widthAtMinHeight = NS_CSS_MINMAX(tentWidth, minWidth, maxWidth);
  }

  // The table at http://www.w3.org/TR/CSS21/visudet.html#min-max-widths :

  nscoord width, height;

  if (tentWidth > maxWidth) {
    if (tentHeight > maxHeight) {
      if (int64_t(maxWidth) * int64_t(tentHeight) <=
          int64_t(maxHeight) * int64_t(tentWidth)) {
        width = maxWidth;
        height = heightAtMaxWidth;
      } else {
        width = widthAtMaxHeight;
        height = maxHeight;
      }
    } else {
      // This also covers "(w > max-width) and (h < min-height)" since in
      // that case (max-width/w < 1), and with (h < min-height):
      //   max(max-width * h/w, min-height) == min-height
      width = maxWidth;
      height = heightAtMaxWidth;
    }
  } else if (tentWidth < minWidth) {
    if (tentHeight < minHeight) {
      if (int64_t(minWidth) * int64_t(tentHeight) <=
          int64_t(minHeight) * int64_t(tentWidth)) {
        width = widthAtMinHeight;
        height = minHeight;
      } else {
        width = minWidth;
        height = heightAtMinWidth;
      }
    } else {
      // This also covers "(w < min-width) and (h > max-height)" since in
      // that case (min-width/w > 1), and with (h > max-height):
      //   min(min-width * h/w, max-height) == max-height
      width = minWidth;
      height = heightAtMinWidth;
    }
  } else {
    if (tentHeight > maxHeight) {
      width = widthAtMaxHeight;
      height = maxHeight;
    } else if (tentHeight < minHeight) {
      width = widthAtMinHeight;
      height = minHeight;
    } else {
      width = tentWidth;
      height = tentHeight;
    }
  }

  return nsSize(width, height);
}

/* static */ nscoord
nsLayoutUtils::MinISizeFromInline(nsIFrame* aFrame,
                                  gfxContext* aRenderingContext)
{
  NS_ASSERTION(!aFrame->IsContainerForFontSizeInflation(),
               "should not be container for font size inflation");

  nsIFrame::InlineMinISizeData data;
  DISPLAY_MIN_WIDTH(aFrame, data.mPrevLines);
  aFrame->AddInlineMinISize(aRenderingContext, &data);
  data.ForceBreak();
  return data.mPrevLines;
}

/* static */ nscoord
nsLayoutUtils::PrefISizeFromInline(nsIFrame* aFrame,
                                   gfxContext* aRenderingContext)
{
  NS_ASSERTION(!aFrame->IsContainerForFontSizeInflation(),
               "should not be container for font size inflation");

  nsIFrame::InlinePrefISizeData data;
  DISPLAY_PREF_WIDTH(aFrame, data.mPrevLines);
  aFrame->AddInlinePrefISize(aRenderingContext, &data);
  data.ForceBreak();
  return data.mPrevLines;
}

static nscolor
DarkenColor(nscolor aColor)
{
  uint16_t  hue, sat, value;
  uint8_t alpha;

  // convert the RBG to HSV so we can get the lightness (which is the v)
  NS_RGB2HSV(aColor, hue, sat, value, alpha);

  // The goal here is to send white to black while letting colored
  // stuff stay colored... So we adopt the following approach.
  // Something with sat = 0 should end up with value = 0.  Something
  // with a high sat can end up with a high value and it's ok.... At
  // the same time, we don't want to make things lighter.  Do
  // something simple, since it seems to work.
  if (value > sat) {
    value = sat;
    // convert this color back into the RGB color space.
    NS_HSV2RGB(aColor, hue, sat, value, alpha);
  }
  return aColor;
}

// Check whether we should darken text/decoration colors. We need to do this if
// background images and colors are being suppressed, because that means
// light text will not be visible against the (presumed light-colored) background.
static bool
ShouldDarkenColors(nsPresContext* aPresContext)
{
  return !aPresContext->GetBackgroundColorDraw() &&
         !aPresContext->GetBackgroundImageDraw();
}

nscolor
nsLayoutUtils::DarkenColorIfNeeded(nsIFrame* aFrame, nscolor aColor)
{
  if (ShouldDarkenColors(aFrame->PresContext())) {
    return DarkenColor(aColor);
  }
  return aColor;
}

gfxFloat
nsLayoutUtils::GetSnappedBaselineY(nsIFrame* aFrame, gfxContext* aContext,
                                   nscoord aY, nscoord aAscent)
{
  gfxFloat appUnitsPerDevUnit = aFrame->PresContext()->AppUnitsPerDevPixel();
  gfxFloat baseline = gfxFloat(aY) + aAscent;
  gfxRect putativeRect(0, baseline/appUnitsPerDevUnit, 1, 1);
  if (!aContext->UserToDevicePixelSnapped(putativeRect, true))
    return baseline;
  return aContext->DeviceToUser(putativeRect.TopLeft()).y * appUnitsPerDevUnit;
}

gfxFloat
nsLayoutUtils::GetSnappedBaselineX(nsIFrame* aFrame, gfxContext* aContext,
                                   nscoord aX, nscoord aAscent)
{
  gfxFloat appUnitsPerDevUnit = aFrame->PresContext()->AppUnitsPerDevPixel();
  gfxFloat baseline = gfxFloat(aX) + aAscent;
  gfxRect putativeRect(baseline / appUnitsPerDevUnit, 0, 1, 1);
  if (!aContext->UserToDevicePixelSnapped(putativeRect, true)) {
    return baseline;
  }
  return aContext->DeviceToUser(putativeRect.TopLeft()).x * appUnitsPerDevUnit;
}

// Hard limit substring lengths to 8000 characters ... this lets us statically
// size the cluster buffer array in FindSafeLength
#define MAX_GFX_TEXT_BUF_SIZE 8000

static int32_t FindSafeLength(const char16_t *aString, uint32_t aLength,
                              uint32_t aMaxChunkLength)
{
  if (aLength <= aMaxChunkLength)
    return aLength;

  int32_t len = aMaxChunkLength;

  // Ensure that we don't break inside a surrogate pair
  while (len > 0 && NS_IS_LOW_SURROGATE(aString[len])) {
    len--;
  }
  if (len == 0) {
    // We don't want our caller to go into an infinite loop, so don't
    // return zero. It's hard to imagine how we could actually get here
    // unless there are languages that allow clusters of arbitrary size.
    // If there are and someone feeds us a 500+ character cluster, too
    // bad.
    return aMaxChunkLength;
  }
  return len;
}

static int32_t GetMaxChunkLength(nsFontMetrics& aFontMetrics)
{
  return std::min(aFontMetrics.GetMaxStringLength(), MAX_GFX_TEXT_BUF_SIZE);
}

nscoord
nsLayoutUtils::AppUnitWidthOfString(const char16_t *aString,
                                    uint32_t aLength,
                                    nsFontMetrics& aFontMetrics,
                                    DrawTarget* aDrawTarget)
{
  uint32_t maxChunkLength = GetMaxChunkLength(aFontMetrics);
  nscoord width = 0;
  while (aLength > 0) {
    int32_t len = FindSafeLength(aString, aLength, maxChunkLength);
    width += aFontMetrics.GetWidth(aString, len, aDrawTarget);
    aLength -= len;
    aString += len;
  }
  return width;
}

nscoord
nsLayoutUtils::AppUnitWidthOfStringBidi(const char16_t* aString,
                                        uint32_t aLength,
                                        const nsIFrame* aFrame,
                                        nsFontMetrics& aFontMetrics,
                                        gfxContext& aContext)
{
  nsPresContext* presContext = aFrame->PresContext();
  if (presContext->BidiEnabled()) {
    nsBidiLevel level =
      nsBidiPresUtils::BidiLevelFromStyle(aFrame->StyleContext());
    return nsBidiPresUtils::MeasureTextWidth(aString, aLength, level,
                                             presContext, aContext,
                                             aFontMetrics);
  }
  aFontMetrics.SetTextRunRTL(false);
  aFontMetrics.SetVertical(aFrame->GetWritingMode().IsVertical());
  aFontMetrics.SetTextOrientation(aFrame->StyleVisibility()->mTextOrientation);
  return nsLayoutUtils::AppUnitWidthOfString(aString, aLength, aFontMetrics,
                                             aContext.GetDrawTarget());
}

bool
nsLayoutUtils::StringWidthIsGreaterThan(const nsString& aString,
                                        nsFontMetrics& aFontMetrics,
                                        DrawTarget* aDrawTarget,
                                        nscoord aWidth)
{
  const char16_t *string = aString.get();
  uint32_t length = aString.Length();
  uint32_t maxChunkLength = GetMaxChunkLength(aFontMetrics);
  nscoord width = 0;
  while (length > 0) {
    int32_t len = FindSafeLength(string, length, maxChunkLength);
    width += aFontMetrics.GetWidth(string, len, aDrawTarget);
    if (width > aWidth) {
      return true;
    }
    length -= len;
    string += len;
  }
  return false;
}

nsBoundingMetrics
nsLayoutUtils::AppUnitBoundsOfString(const char16_t* aString,
                                     uint32_t aLength,
                                     nsFontMetrics& aFontMetrics,
                                     DrawTarget* aDrawTarget)
{
  uint32_t maxChunkLength = GetMaxChunkLength(aFontMetrics);
  int32_t len = FindSafeLength(aString, aLength, maxChunkLength);
  // Assign directly in the first iteration. This ensures that
  // negative ascent/descent can be returned and the left bearing
  // is properly initialized.
  nsBoundingMetrics totalMetrics =
    aFontMetrics.GetBoundingMetrics(aString, len, aDrawTarget);
  aLength -= len;
  aString += len;

  while (aLength > 0) {
    len = FindSafeLength(aString, aLength, maxChunkLength);
    nsBoundingMetrics metrics =
      aFontMetrics.GetBoundingMetrics(aString, len, aDrawTarget);
    totalMetrics += metrics;
    aLength -= len;
    aString += len;
  }
  return totalMetrics;
}

void
nsLayoutUtils::DrawString(const nsIFrame*     aFrame,
                          nsFontMetrics&      aFontMetrics,
                          gfxContext* aContext,
                          const char16_t*     aString,
                          int32_t             aLength,
                          nsPoint             aPoint,
                          nsStyleContext*     aStyleContext,
                          DrawStringFlags     aFlags)
{
  nsresult rv = NS_ERROR_FAILURE;

  // If caller didn't pass a style context, use the frame's.
  if (!aStyleContext) {
    aStyleContext = aFrame->StyleContext();
  }

  if (aFlags & DrawStringFlags::eForceHorizontal) {
    aFontMetrics.SetVertical(false);
  } else {
    aFontMetrics.SetVertical(WritingMode(aStyleContext).IsVertical());
  }

  aFontMetrics.SetTextOrientation(
    aStyleContext->StyleVisibility()->mTextOrientation);

  nsPresContext* presContext = aFrame->PresContext();
  if (presContext->BidiEnabled()) {
    nsBidiLevel level =
      nsBidiPresUtils::BidiLevelFromStyle(aStyleContext);
    rv = nsBidiPresUtils::RenderText(aString, aLength, level,
                                     presContext, *aContext,
                                     aContext->GetDrawTarget(), aFontMetrics,
                                     aPoint.x, aPoint.y);
  }
  if (NS_FAILED(rv))
  {
    aFontMetrics.SetTextRunRTL(false);
    DrawUniDirString(aString, aLength, aPoint, aFontMetrics, *aContext);
  }
}

void
nsLayoutUtils::DrawUniDirString(const char16_t* aString,
                                uint32_t aLength,
                                nsPoint aPoint,
                                nsFontMetrics& aFontMetrics,
                                gfxContext& aContext)
{
  nscoord x = aPoint.x;
  nscoord y = aPoint.y;

  uint32_t maxChunkLength = GetMaxChunkLength(aFontMetrics);
  if (aLength <= maxChunkLength) {
    aFontMetrics.DrawString(aString, aLength, x, y, &aContext,
                            aContext.GetDrawTarget());
    return;
  }

  bool isRTL = aFontMetrics.GetTextRunRTL();

  // If we're drawing right to left, we must start at the end.
  if (isRTL) {
    x += nsLayoutUtils::AppUnitWidthOfString(aString, aLength, aFontMetrics,
                                             aContext.GetDrawTarget());
  }

  while (aLength > 0) {
    int32_t len = FindSafeLength(aString, aLength, maxChunkLength);
    nscoord width = aFontMetrics.GetWidth(aString, len, aContext.GetDrawTarget());
    if (isRTL) {
      x -= width;
    }
    aFontMetrics.DrawString(aString, len, x, y, &aContext,
                            aContext.GetDrawTarget());
    if (!isRTL) {
      x += width;
    }
    aLength -= len;
    aString += len;
  }
}

/* static */ void
nsLayoutUtils::PaintTextShadow(const nsIFrame* aFrame,
                               gfxContext* aContext,
                               const nsRect& aTextRect,
                               const nsRect& aDirtyRect,
                               const nscolor& aForegroundColor,
                               TextShadowCallback aCallback,
                               void* aCallbackData)
{
  const nsStyleText* textStyle = aFrame->StyleText();
  if (!textStyle->HasTextShadow())
    return;

  // Text shadow happens with the last value being painted at the back,
  // ie. it is painted first.
  gfxContext* aDestCtx = aContext;
  for (uint32_t i = textStyle->mTextShadow->Length(); i > 0; --i) {
    nsCSSShadowItem* shadowDetails = textStyle->mTextShadow->ShadowAt(i - 1);
    nsPoint shadowOffset(shadowDetails->mXOffset,
                         shadowDetails->mYOffset);
    nscoord blurRadius = std::max(shadowDetails->mRadius, 0);

    nsRect shadowRect(aTextRect);
    shadowRect.MoveBy(shadowOffset);

    nsPresContext* presCtx = aFrame->PresContext();
    nsContextBoxBlur contextBoxBlur;
    gfxContext* shadowContext = contextBoxBlur.Init(shadowRect, 0, blurRadius,
                                                    presCtx->AppUnitsPerDevPixel(),
                                                    aDestCtx, aDirtyRect, nullptr);
    if (!shadowContext)
      continue;

    nscolor shadowColor;
    if (shadowDetails->mHasColor)
      shadowColor = shadowDetails->mColor;
    else
      shadowColor = aForegroundColor;

    aDestCtx->Save();
    aDestCtx->NewPath();
    aDestCtx->SetColor(Color::FromABGR(shadowColor));

    // The callback will draw whatever we want to blur as a shadow.
    aCallback(shadowContext, shadowOffset, shadowColor, aCallbackData);

    contextBoxBlur.DoPaint();
    aDestCtx->Restore();
  }
}

/* static */ nscoord
nsLayoutUtils::GetCenteredFontBaseline(nsFontMetrics* aFontMetrics,
                                       nscoord        aLineHeight,
                                       bool           aIsInverted)
{
  nscoord fontAscent = aIsInverted ? aFontMetrics->MaxDescent()
                                   : aFontMetrics->MaxAscent();
  nscoord fontHeight = aFontMetrics->MaxHeight();

  nscoord leading = aLineHeight - fontHeight;
  return fontAscent + leading/2;
}


/* static */ bool
nsLayoutUtils::GetFirstLineBaseline(WritingMode aWritingMode,
                                    const nsIFrame* aFrame, nscoord* aResult)
{
  LinePosition position;
  if (!GetFirstLinePosition(aWritingMode, aFrame, &position))
    return false;
  *aResult = position.mBaseline;
  return true;
}

/* static */ bool
nsLayoutUtils::GetFirstLinePosition(WritingMode aWM,
                                    const nsIFrame* aFrame,
                                    LinePosition* aResult)
{
  const nsBlockFrame* block = nsLayoutUtils::GetAsBlock(const_cast<nsIFrame*>(aFrame));
  if (!block) {
    // For the first-line baseline we also have to check for a table, and if
    // so, use the baseline of its first row.
    LayoutFrameType fType = aFrame->Type();
    if (fType == LayoutFrameType::TableWrapper ||
        fType == LayoutFrameType::FlexContainer ||
        fType == LayoutFrameType::GridContainer) {
      if ((fType == LayoutFrameType::GridContainer &&
           aFrame->HasAnyStateBits(NS_STATE_GRID_SYNTHESIZE_BASELINE)) ||
          (fType == LayoutFrameType::FlexContainer &&
           aFrame->HasAnyStateBits(NS_STATE_FLEX_SYNTHESIZE_BASELINE)) ||
          (fType == LayoutFrameType::TableWrapper &&
           static_cast<const nsTableWrapperFrame*>(aFrame)->GetRowCount() == 0)) {
        // empty grid/flex/table container
        aResult->mBStart = 0;
        aResult->mBaseline = aFrame->SynthesizeBaselineBOffsetFromBorderBox(aWM,
                                       BaselineSharingGroup::eFirst);
        aResult->mBEnd = aFrame->BSize(aWM);
        return true;
      }
      aResult->mBStart = 0;
      aResult->mBaseline = aFrame->GetLogicalBaseline(aWM);
      // This is what we want for the list bullet caller; not sure if
      // other future callers will want the same.
      aResult->mBEnd = aFrame->BSize(aWM);
      return true;
    }

    // For first-line baselines, we have to consider scroll frames.
    if (fType == LayoutFrameType::Scroll) {
      nsIScrollableFrame *sFrame = do_QueryFrame(const_cast<nsIFrame*>(aFrame));
      if (!sFrame) {
        NS_NOTREACHED("not scroll frame");
      }
      LinePosition kidPosition;
      if (GetFirstLinePosition(aWM,
                               sFrame->GetScrolledFrame(), &kidPosition)) {
        // Consider only the border and padding that contributes to the
        // kid's position, not the scrolling, so we get the initial
        // position.
        *aResult = kidPosition +
          aFrame->GetLogicalUsedBorderAndPadding(aWM).BStart(aWM);
        return true;
      }
      return false;
    }

    if (fType == LayoutFrameType::FieldSet) {
      LinePosition kidPosition;
      nsIFrame* kid = aFrame->PrincipalChildList().FirstChild();
      // kid might be a legend frame here, but that's ok.
      if (GetFirstLinePosition(aWM, kid, &kidPosition)) {
        *aResult = kidPosition +
          kid->GetLogicalNormalPosition(aWM, aFrame->GetSize()).B(aWM);
        return true;
      }
      return false;
    }

    // No baseline.
    return false;
  }

  for (nsBlockFrame::ConstLineIterator line = block->LinesBegin(),
                                       line_end = block->LinesEnd();
       line != line_end; ++line) {
    if (line->IsBlock()) {
      nsIFrame *kid = line->mFirstChild;
      LinePosition kidPosition;
      if (GetFirstLinePosition(aWM, kid, &kidPosition)) {
        //XXX Not sure if this is the correct value to use for container
        //    width here. It will only be used in vertical-rl layout,
        //    which we don't have full support and testing for yet.
        const nsSize& containerSize = line->mContainerSize;
        *aResult = kidPosition +
                   kid->GetLogicalNormalPosition(aWM, containerSize).B(aWM);
        return true;
      }
    } else {
      // XXX Is this the right test?  We have some bogus empty lines
      // floating around, but IsEmpty is perhaps too weak.
      if (line->BSize() != 0 || !line->IsEmpty()) {
        nscoord bStart = line->BStart();
        aResult->mBStart = bStart;
        aResult->mBaseline = bStart + line->GetLogicalAscent();
        aResult->mBEnd = bStart + line->BSize();
        return true;
      }
    }
  }
  return false;
}

/* static */ bool
nsLayoutUtils::GetLastLineBaseline(WritingMode aWM,
                                   const nsIFrame* aFrame, nscoord* aResult)
{
  const nsBlockFrame* block = nsLayoutUtils::GetAsBlock(const_cast<nsIFrame*>(aFrame));
  if (!block)
    // No baseline.  (We intentionally don't descend into scroll frames.)
    return false;

  for (nsBlockFrame::ConstReverseLineIterator line = block->LinesRBegin(),
                                              line_end = block->LinesREnd();
       line != line_end; ++line) {
    if (line->IsBlock()) {
      nsIFrame *kid = line->mFirstChild;
      nscoord kidBaseline;
      const nsSize& containerSize = line->mContainerSize;
      if (GetLastLineBaseline(aWM, kid, &kidBaseline)) {
        // Ignore relative positioning for baseline calculations
        *aResult = kidBaseline +
          kid->GetLogicalNormalPosition(aWM, containerSize).B(aWM);
        return true;
      } else if (kid->IsScrollFrame()) {
        // Defer to nsFrame::GetLogicalBaseline (which synthesizes a baseline
        // from the margin-box).
        kidBaseline = kid->GetLogicalBaseline(aWM);
        *aResult = kidBaseline +
          kid->GetLogicalNormalPosition(aWM, containerSize).B(aWM);
        return true;
      }
    } else {
      // XXX Is this the right test?  We have some bogus empty lines
      // floating around, but IsEmpty is perhaps too weak.
      if (line->BSize() != 0 || !line->IsEmpty()) {
        *aResult = line->BStart() + line->GetLogicalAscent();
        return true;
      }
    }
  }
  return false;
}

static nscoord
CalculateBlockContentBEnd(WritingMode aWM, nsBlockFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "null ptr");

  nscoord contentBEnd = 0;

  for (nsBlockFrame::LineIterator line = aFrame->LinesBegin(),
                                  line_end = aFrame->LinesEnd();
       line != line_end; ++line) {
    if (line->IsBlock()) {
      nsIFrame* child = line->mFirstChild;
      const nsSize& containerSize = line->mContainerSize;
      nscoord offset =
        child->GetLogicalNormalPosition(aWM, containerSize).B(aWM);
      contentBEnd =
        std::max(contentBEnd,
                 nsLayoutUtils::CalculateContentBEnd(aWM, child) + offset);
    }
    else {
      contentBEnd = std::max(contentBEnd, line->BEnd());
    }
  }
  return contentBEnd;
}

/* static */ nscoord
nsLayoutUtils::CalculateContentBEnd(WritingMode aWM, nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "null ptr");

  nscoord contentBEnd = aFrame->BSize(aWM);

  // We want scrollable overflow rather than visual because this
  // calculation is intended to affect layout.
  LogicalSize overflowSize(aWM, aFrame->GetScrollableOverflowRect().Size());
  if (overflowSize.BSize(aWM) > contentBEnd) {
    nsIFrame::ChildListIDs skip(nsIFrame::kOverflowList |
                                nsIFrame::kExcessOverflowContainersList |
                                nsIFrame::kOverflowOutOfFlowList);
    nsBlockFrame* blockFrame = GetAsBlock(aFrame);
    if (blockFrame) {
      contentBEnd =
        std::max(contentBEnd, CalculateBlockContentBEnd(aWM, blockFrame));
      skip |= nsIFrame::kPrincipalList;
    }
    nsIFrame::ChildListIterator lists(aFrame);
    for (; !lists.IsDone(); lists.Next()) {
      if (!skip.Contains(lists.CurrentID())) {
        nsFrameList::Enumerator childFrames(lists.CurrentList());
        for (; !childFrames.AtEnd(); childFrames.Next()) {
          nsIFrame* child = childFrames.get();
          nscoord offset =
            child->GetLogicalNormalPosition(aWM,
                                            aFrame->GetSize()).B(aWM);
          contentBEnd = std::max(contentBEnd,
                                 CalculateContentBEnd(aWM, child) + offset);
        }
      }
    }
  }
  return contentBEnd;
}

/* static */ nsIFrame*
nsLayoutUtils::GetClosestLayer(nsIFrame* aFrame)
{
  nsIFrame* layer;
  for (layer = aFrame; layer; layer = layer->GetParent()) {
    if (layer->IsAbsPosContainingBlock() ||
        (layer->GetParent() && layer->GetParent()->IsScrollFrame()))
      break;
  }
  if (layer)
    return layer;
  return aFrame->PresContext()->PresShell()->FrameManager()->GetRootFrame();
}

SamplingFilter
nsLayoutUtils::GetSamplingFilterForFrame(nsIFrame* aForFrame)
{
  SamplingFilter defaultFilter = SamplingFilter::GOOD;
  nsStyleContext *sc;
  if (nsCSSRendering::IsCanvasFrame(aForFrame)) {
    nsCSSRendering::FindBackground(aForFrame, &sc);
  } else {
    sc = aForFrame->StyleContext();
  }

  switch (sc->StyleVisibility()->mImageRendering) {
  case NS_STYLE_IMAGE_RENDERING_OPTIMIZESPEED:
    return SamplingFilter::POINT;
  case NS_STYLE_IMAGE_RENDERING_OPTIMIZEQUALITY:
    return SamplingFilter::LINEAR;
  case NS_STYLE_IMAGE_RENDERING_CRISPEDGES:
    return SamplingFilter::POINT;
  default:
    return defaultFilter;
  }
}

/**
 * Given an image being drawn into an appunit coordinate system, and
 * a point in that coordinate system, map the point back into image
 * pixel space.
 * @param aSize the size of the image, in pixels
 * @param aDest the rectangle that the image is being mapped into
 * @param aPt a point in the same coordinate system as the rectangle
 */
static gfxPoint
MapToFloatImagePixels(const gfxSize& aSize,
                      const gfxRect& aDest, const gfxPoint& aPt)
{
  return gfxPoint(((aPt.x - aDest.X())*aSize.width)/aDest.Width(),
                  ((aPt.y - aDest.Y())*aSize.height)/aDest.Height());
}

/**
 * Given an image being drawn into an pixel-based coordinate system, and
 * a point in image space, map the point into the pixel-based coordinate
 * system.
 * @param aSize the size of the image, in pixels
 * @param aDest the rectangle that the image is being mapped into
 * @param aPt a point in image space
 */
static gfxPoint
MapToFloatUserPixels(const gfxSize& aSize,
                     const gfxRect& aDest, const gfxPoint& aPt)
{
  return gfxPoint(aPt.x*aDest.Width()/aSize.width + aDest.X(),
                  aPt.y*aDest.Height()/aSize.height + aDest.Y());
}

/* static */ gfxRect
nsLayoutUtils::RectToGfxRect(const nsRect& aRect, int32_t aAppUnitsPerDevPixel)
{
  return gfxRect(gfxFloat(aRect.x) / aAppUnitsPerDevPixel,
                 gfxFloat(aRect.y) / aAppUnitsPerDevPixel,
                 gfxFloat(aRect.width) / aAppUnitsPerDevPixel,
                 gfxFloat(aRect.height) / aAppUnitsPerDevPixel);
}

struct SnappedImageDrawingParameters {
  // A transform from image space to device space.
  gfxMatrix imageSpaceToDeviceSpace;
  // The size at which the image should be drawn (which may not be its
  // intrinsic size due to, for example, HQ scaling).
  nsIntSize size;
  // The region in tiled image space which will be drawn, with an associated
  // region to which sampling should be restricted.
  ImageRegion region;
  // The default viewport size for SVG images, which we use unless a different
  // one has been explicitly specified. This is the same as |size| except that
  // it does not take into account any transformation on the gfxContext we're
  // drawing to - for example, CSS transforms are not taken into account.
  CSSIntSize svgViewportSize;
  // Whether there's anything to draw at all.
  bool shouldDraw;

  SnappedImageDrawingParameters()
   : region(ImageRegion::Empty())
   , shouldDraw(false)
  {}

  SnappedImageDrawingParameters(const gfxMatrix&   aImageSpaceToDeviceSpace,
                                const nsIntSize&   aSize,
                                const ImageRegion& aRegion,
                                const CSSIntSize&  aSVGViewportSize)
   : imageSpaceToDeviceSpace(aImageSpaceToDeviceSpace)
   , size(aSize)
   , region(aRegion)
   , svgViewportSize(aSVGViewportSize)
   , shouldDraw(true)
  {}
};

/**
 * Given two axis-aligned rectangles, returns the transformation that maps the
 * first onto the second.
 *
 * @param aFrom The rect to be transformed.
 * @param aTo The rect that aFrom should be mapped onto by the transformation.
 */
static gfxMatrix
TransformBetweenRects(const gfxRect& aFrom, const gfxRect& aTo)
{
  gfxSize scale(aTo.width / aFrom.width,
                aTo.height / aFrom.height);
  gfxPoint translation(aTo.x - aFrom.x * scale.width,
                       aTo.y - aFrom.y * scale.height);
  return gfxMatrix(scale.width, 0, 0, scale.height,
                   translation.x, translation.y);
}

static nsRect
TileNearRect(const nsRect& aAnyTile, const nsRect& aTargetRect)
{
  nsPoint distance = aTargetRect.TopLeft() - aAnyTile.TopLeft();
  return aAnyTile + nsPoint(distance.x / aAnyTile.width * aAnyTile.width,
                            distance.y / aAnyTile.height * aAnyTile.height);
}

static gfxFloat
StableRound(gfxFloat aValue)
{
  // Values slightly less than 0.5 should round up like 0.5 would; we're
  // assuming they were meant to be 0.5.
  return floor(aValue + 0.5001);
}

static gfxPoint
StableRound(const gfxPoint& aPoint)
{
  return gfxPoint(StableRound(aPoint.x), StableRound(aPoint.y));
}

/**
 * Given a set of input parameters, compute certain output parameters
 * for drawing an image with the image snapping algorithm.
 * See https://wiki.mozilla.org/Gecko:Image_Snapping_and_Rendering
 *
 *  @see nsLayoutUtils::DrawImage() for the descriptions of input parameters
 */
static SnappedImageDrawingParameters
ComputeSnappedImageDrawingParameters(gfxContext*     aCtx,
                                     int32_t         aAppUnitsPerDevPixel,
                                     const nsRect    aDest,
                                     const nsRect    aFill,
                                     const nsPoint   aAnchor,
                                     const nsRect    aDirty,
                                     imgIContainer*  aImage,
                                     const SamplingFilter aSamplingFilter,
                                     uint32_t        aImageFlags,
                                     ExtendMode      aExtendMode)
{
  if (aDest.IsEmpty() || aFill.IsEmpty())
    return SnappedImageDrawingParameters();

  // Avoid unnecessarily large offsets.
  bool doTile = !aDest.Contains(aFill);
  nsRect appUnitDest = doTile ? TileNearRect(aDest, aFill.Intersect(aDirty))
                              : aDest;
  nsPoint anchor = aAnchor + (appUnitDest.TopLeft() - aDest.TopLeft());

  gfxRect devPixelDest =
    nsLayoutUtils::RectToGfxRect(appUnitDest, aAppUnitsPerDevPixel);
  gfxRect devPixelFill =
    nsLayoutUtils::RectToGfxRect(aFill, aAppUnitsPerDevPixel);
  gfxRect devPixelDirty =
    nsLayoutUtils::RectToGfxRect(aDirty, aAppUnitsPerDevPixel);

  gfxMatrix currentMatrix = aCtx->CurrentMatrix();
  gfxRect fill = devPixelFill;
  gfxRect dest = devPixelDest;
  bool didSnap;
  // Snap even if we have a scale in the context. But don't snap if
  // we have something that's not translation+scale, or if the scale flips in
  // the X or Y direction, because snapped image drawing can't handle that yet.
  if (!currentMatrix.HasNonAxisAlignedTransform() &&
      currentMatrix._11 > 0.0 && currentMatrix._22 > 0.0 &&
      aCtx->UserToDevicePixelSnapped(fill, true) &&
      aCtx->UserToDevicePixelSnapped(dest, true)) {
    // We snapped. On this code path, |fill| and |dest| take into account
    // currentMatrix's transform.
    didSnap = true;
  } else {
    // We didn't snap. On this code path, |fill| and |dest| do not take into
    // account currentMatrix's transform.
    didSnap = false;
    fill = devPixelFill;
    dest = devPixelDest;
  }

  // If we snapped above, |dest| already takes into account |currentMatrix|'s scale
  // and has integer coordinates. If not, we need these properties to compute
  // the optimal drawn image size, so compute |snappedDestSize| here.
  gfxSize snappedDestSize = dest.Size();
  if (!didSnap) {
    gfxSize scaleFactors = currentMatrix.ScaleFactors(true);
    snappedDestSize.Scale(scaleFactors.width, scaleFactors.height);
    snappedDestSize.width = NS_round(snappedDestSize.width);
    snappedDestSize.height = NS_round(snappedDestSize.height);
  }

  // We need to be sure that this is at least one pixel in width and height,
  // or we'll end up drawing nothing even if we have a nonempty fill.
  snappedDestSize.width = std::max(snappedDestSize.width, 1.0);
  snappedDestSize.height = std::max(snappedDestSize.height, 1.0);

  // Bail if we're not going to end up drawing anything.
  if (fill.IsEmpty() || snappedDestSize.IsEmpty()) {
    return SnappedImageDrawingParameters();
  }

  nsIntSize intImageSize =
    aImage->OptimalImageSizeForDest(snappedDestSize,
                                    imgIContainer::FRAME_CURRENT,
                                    aSamplingFilter, aImageFlags);
  gfxSize imageSize(intImageSize.width, intImageSize.height);

  // XXX(seth): May be buggy; see bug 1151016.
  CSSIntSize svgViewportSize = currentMatrix.IsIdentity()
    ? CSSIntSize(intImageSize.width, intImageSize.height)
    : CSSIntSize::Truncate(devPixelDest.width, devPixelDest.height);

  // Compute the set of pixels that would be sampled by an ideal rendering
  gfxPoint subimageTopLeft =
    MapToFloatImagePixels(imageSize, devPixelDest, devPixelFill.TopLeft());
  gfxPoint subimageBottomRight =
    MapToFloatImagePixels(imageSize, devPixelDest, devPixelFill.BottomRight());
  gfxRect subimage;
  subimage.MoveTo(NSToIntFloor(subimageTopLeft.x),
                  NSToIntFloor(subimageTopLeft.y));
  subimage.SizeTo(NSToIntCeil(subimageBottomRight.x) - subimage.x,
                  NSToIntCeil(subimageBottomRight.y) - subimage.y);

  if (subimage.IsEmpty()) {
    // Bail if the subimage is empty (we're not going to be drawing anything).
    return SnappedImageDrawingParameters();
  }

  gfxMatrix transform;
  gfxMatrix invTransform;

  bool anchorAtUpperLeft = anchor.x == appUnitDest.x &&
                           anchor.y == appUnitDest.y;
  bool exactlyOneImageCopy = aFill.IsEqualEdges(appUnitDest);
  if (anchorAtUpperLeft && exactlyOneImageCopy) {
    // The simple case: we can ignore the anchor point and compute the
    // transformation from the sampled region (the subimage) to the fill rect.
    // This approach is preferable when it works since it tends to produce
    // less numerical error.
    transform = TransformBetweenRects(subimage, fill);
    invTransform = TransformBetweenRects(fill, subimage);
  } else {
    // The more complicated case: we compute the transformation from the
    // image rect positioned at the image space anchor point to the dest rect
    // positioned at the device space anchor point.

    // Compute the anchor point in both device space and image space.  This
    // code assumes that pixel-based devices have one pixel per device unit!
    gfxPoint anchorPoint(gfxFloat(anchor.x)/aAppUnitsPerDevPixel,
                         gfxFloat(anchor.y)/aAppUnitsPerDevPixel);
    gfxPoint imageSpaceAnchorPoint =
      MapToFloatImagePixels(imageSize, devPixelDest, anchorPoint);

    if (didSnap) {
      imageSpaceAnchorPoint = StableRound(imageSpaceAnchorPoint);
      anchorPoint = imageSpaceAnchorPoint;
      anchorPoint = MapToFloatUserPixels(imageSize, devPixelDest, anchorPoint);
      anchorPoint = currentMatrix.Transform(anchorPoint);
      anchorPoint = StableRound(anchorPoint);
    }

    // Compute an unsnapped version of the dest rect's size. We continue to
    // follow the pattern that we take |currentMatrix| into account only if
    // |didSnap| is true.
    gfxSize unsnappedDestSize
      = didSnap ? devPixelDest.Size() * currentMatrix.ScaleFactors(true)
                : devPixelDest.Size();

    gfxRect anchoredDestRect(anchorPoint, unsnappedDestSize);
    gfxRect anchoredImageRect(imageSpaceAnchorPoint, imageSize);

    // Calculate anchoredDestRect with snapped fill rect when the devPixelFill rect
    // corresponds to just a single tile in that direction
    if (fill.Width() != devPixelFill.Width() &&
        devPixelDest.x == devPixelFill.x &&
        devPixelDest.XMost() == devPixelFill.XMost()) {
      anchoredDestRect.width = fill.width;
    }
    if (fill.Height() != devPixelFill.Height() &&
        devPixelDest.y == devPixelFill.y &&
        devPixelDest.YMost() == devPixelFill.YMost()) {
      anchoredDestRect.height = fill.height;
    }

    transform = TransformBetweenRects(anchoredImageRect, anchoredDestRect);
    invTransform = TransformBetweenRects(anchoredDestRect, anchoredImageRect);
  }

  // If the transform is not a straight translation by integers, then
  // filtering will occur, and restricting the fill rect to the dirty rect
  // would change the values computed for edge pixels, which we can't allow.
  // Also, if 'didSnap' is false then rounding out 'devPixelDirty' might not
  // produce pixel-aligned coordinates, which would also break the values
  // computed for edge pixels.
  if (didSnap && !invTransform.HasNonIntegerTranslation()) {
    // This form of Transform is safe to call since non-axis-aligned
    // transforms wouldn't be snapped.
    devPixelDirty = currentMatrix.Transform(devPixelDirty);
    devPixelDirty.RoundOut();
    fill = fill.Intersect(devPixelDirty);
  }
  if (fill.IsEmpty())
    return SnappedImageDrawingParameters();

  gfxRect imageSpaceFill(didSnap ? invTransform.Transform(fill)
                                 : invTransform.TransformBounds(fill));

  // If we didn't snap, we need to post-multiply the matrix on the context to
  // get the final matrix we'll draw with, because we didn't take it into
  // account when computing the matrices above.
  if (!didSnap) {
    transform = transform * currentMatrix;
  }

  ExtendMode extendMode = (aImageFlags & imgIContainer::FLAG_CLAMP)
                          ? ExtendMode::CLAMP
                          : aExtendMode;
  // We were passed in the default extend mode but need to tile.
  if (extendMode == ExtendMode::CLAMP && doTile) {
    MOZ_ASSERT(!(aImageFlags & imgIContainer::FLAG_CLAMP));
    extendMode = ExtendMode::REPEAT;
  }

  ImageRegion region =
    ImageRegion::CreateWithSamplingRestriction(imageSpaceFill, subimage, extendMode);

  return SnappedImageDrawingParameters(transform, intImageSize,
                                       region, svgViewportSize);
}

static DrawResult
DrawImageInternal(gfxContext&            aContext,
                  nsPresContext*         aPresContext,
                  imgIContainer*         aImage,
                  const SamplingFilter   aSamplingFilter,
                  const nsRect&          aDest,
                  const nsRect&          aFill,
                  const nsPoint&         aAnchor,
                  const nsRect&          aDirty,
                  const Maybe<SVGImageContext>& aSVGContext,
                  uint32_t               aImageFlags,
                  ExtendMode             aExtendMode = ExtendMode::CLAMP,
                  float                  aOpacity = 1.0)
{
  DrawResult result = DrawResult::SUCCESS;

  aImageFlags |= imgIContainer::FLAG_ASYNC_NOTIFY;

  if (aPresContext->Type() == nsPresContext::eContext_Print) {
    // We want vector images to be passed on as vector commands, not a raster
    // image.
    aImageFlags |= imgIContainer::FLAG_BYPASS_SURFACE_CACHE;
  }
  if (aDest.Contains(aFill)) {
    aImageFlags |= imgIContainer::FLAG_CLAMP;
  }
  int32_t appUnitsPerDevPixel =
   aPresContext->AppUnitsPerDevPixel();

  SnappedImageDrawingParameters params =
    ComputeSnappedImageDrawingParameters(&aContext, appUnitsPerDevPixel, aDest,
                                         aFill, aAnchor, aDirty, aImage,
                                         aSamplingFilter, aImageFlags, aExtendMode);

  if (!params.shouldDraw) {
    return result;
  }

  {
    gfxContextMatrixAutoSaveRestore contextMatrixRestorer(&aContext);

    RefPtr<gfxContext> destCtx = &aContext;

    destCtx->SetMatrix(params.imageSpaceToDeviceSpace);

    Maybe<SVGImageContext> fallbackContext;
    if (!aSVGContext) {
      // Use the default viewport.
      fallbackContext.emplace(Some(params.svgViewportSize));
    }

    result = aImage->Draw(destCtx, params.size, params.region,
                          imgIContainer::FRAME_CURRENT, aSamplingFilter,
                          aSVGContext ? aSVGContext : fallbackContext,
                          aImageFlags, aOpacity);

  }

  return result;
}

/* static */ DrawResult
nsLayoutUtils::DrawSingleUnscaledImage(gfxContext&          aContext,
                                       nsPresContext*       aPresContext,
                                       imgIContainer*       aImage,
                                       const SamplingFilter aSamplingFilter,
                                       const nsPoint&       aDest,
                                       const nsRect*        aDirty,
                                       uint32_t             aImageFlags,
                                       const nsRect*        aSourceArea)
{
  CSSIntSize imageSize;
  aImage->GetWidth(&imageSize.width);
  aImage->GetHeight(&imageSize.height);
  if (imageSize.width < 1 || imageSize.height < 1) {
    NS_WARNING("Image width or height is non-positive");
    return DrawResult::TEMPORARY_ERROR;
  }

  nsSize size(CSSPixel::ToAppUnits(imageSize));
  nsRect source;
  if (aSourceArea) {
    source = *aSourceArea;
  } else {
    source.SizeTo(size);
  }

  nsRect dest(aDest - source.TopLeft(), size);
  nsRect fill(aDest, source.Size());
  // Ensure that only a single image tile is drawn. If aSourceArea extends
  // outside the image bounds, we want to honor the aSourceArea-to-aDest
  // translation but we don't want to actually tile the image.
  fill.IntersectRect(fill, dest);
  return DrawImageInternal(aContext, aPresContext,
                           aImage, aSamplingFilter,
                           dest, fill, aDest, aDirty ? *aDirty : dest,
                           /* no SVGImageContext */ Nothing(), aImageFlags);
}

/* static */ DrawResult
nsLayoutUtils::DrawSingleImage(gfxContext&            aContext,
                               nsPresContext*         aPresContext,
                               imgIContainer*         aImage,
                               const SamplingFilter   aSamplingFilter,
                               const nsRect&          aDest,
                               const nsRect&          aDirty,
                               const Maybe<SVGImageContext>& aSVGContext,
                               uint32_t               aImageFlags,
                               const nsPoint*         aAnchorPoint,
                               const nsRect*          aSourceArea)
{
  nscoord appUnitsPerCSSPixel = nsDeviceContext::AppUnitsPerCSSPixel();
  CSSIntSize pixelImageSize(ComputeSizeForDrawingWithFallback(aImage, aDest.Size()));
  if (pixelImageSize.width < 1 || pixelImageSize.height < 1) {
    NS_ASSERTION(pixelImageSize.width >= 0 && pixelImageSize.height >= 0,
                 "Image width or height is negative");
    return DrawResult::SUCCESS;  // no point in drawing a zero size image
  }

  nsSize imageSize(CSSPixel::ToAppUnits(pixelImageSize));
  nsRect source;
  nsCOMPtr<imgIContainer> image;
  if (aSourceArea) {
    source = *aSourceArea;
    nsIntRect subRect(source.x, source.y, source.width, source.height);
    subRect.ScaleInverseRoundOut(appUnitsPerCSSPixel);
    image = ImageOps::Clip(aImage, subRect);

    nsRect imageRect;
    imageRect.SizeTo(imageSize);
    nsRect clippedSource = imageRect.Intersect(source);

    source -= clippedSource.TopLeft();
    imageSize = clippedSource.Size();
  } else {
    source.SizeTo(imageSize);
    image = aImage;
  }

  nsRect dest = GetWholeImageDestination(imageSize, source, aDest);

  // Ensure that only a single image tile is drawn. If aSourceArea extends
  // outside the image bounds, we want to honor the aSourceArea-to-aDest
  // transform but we don't want to actually tile the image.
  nsRect fill;
  fill.IntersectRect(aDest, dest);
  return DrawImageInternal(aContext, aPresContext, image,
                           aSamplingFilter, dest, fill,
                           aAnchorPoint ? *aAnchorPoint : fill.TopLeft(),
                           aDirty, aSVGContext, aImageFlags);
}

/* static */ void
nsLayoutUtils::ComputeSizeForDrawing(imgIContainer *aImage,
                                     CSSIntSize&    aImageSize, /*outparam*/
                                     nsSize&        aIntrinsicRatio, /*outparam*/
                                     bool&          aGotWidth,  /*outparam*/
                                     bool&          aGotHeight  /*outparam*/)
{
  aGotWidth  = NS_SUCCEEDED(aImage->GetWidth(&aImageSize.width));
  aGotHeight = NS_SUCCEEDED(aImage->GetHeight(&aImageSize.height));
  bool gotRatio = NS_SUCCEEDED(aImage->GetIntrinsicRatio(&aIntrinsicRatio));

  if (!(aGotWidth && aGotHeight) && !gotRatio) {
    // We hit an error (say, because the image failed to load or couldn't be
    // decoded) and should return zero size.
    aGotWidth = aGotHeight = true;
    aImageSize = CSSIntSize(0, 0);
    aIntrinsicRatio = nsSize(0, 0);
  }
}

/* static */ CSSIntSize
nsLayoutUtils::ComputeSizeForDrawingWithFallback(imgIContainer* aImage,
                                                 const nsSize&  aFallbackSize)
{
  CSSIntSize imageSize;
  nsSize imageRatio;
  bool gotHeight, gotWidth;
  ComputeSizeForDrawing(aImage, imageSize, imageRatio, gotWidth, gotHeight);

  // If we didn't get both width and height, try to compute them using the
  // intrinsic ratio of the image.
  if (gotWidth != gotHeight) {
    if (!gotWidth) {
      if (imageRatio.height != 0) {
        imageSize.width =
          NSCoordSaturatingNonnegativeMultiply(imageSize.height,
                                               float(imageRatio.width) /
                                               float(imageRatio.height));
        gotWidth = true;
      }
    } else {
      if (imageRatio.width != 0) {
        imageSize.height =
          NSCoordSaturatingNonnegativeMultiply(imageSize.width,
                                               float(imageRatio.height) /
                                               float(imageRatio.width));
        gotHeight = true;
      }
    }
  }

  // If we still don't have a width or height, just use the fallback size the
  // caller provided.
  if (!gotWidth) {
    imageSize.width = nsPresContext::AppUnitsToIntCSSPixels(aFallbackSize.width);
  }
  if (!gotHeight) {
    imageSize.height = nsPresContext::AppUnitsToIntCSSPixels(aFallbackSize.height);
  }

  return imageSize;
}

/* static */ nsPoint
nsLayoutUtils::GetBackgroundFirstTilePos(const nsPoint& aDest,
                                         const nsPoint& aFill,
                                         const nsSize& aRepeatSize)
{
  return nsPoint(NSToIntFloor(float(aFill.x - aDest.x) / aRepeatSize.width) * aRepeatSize.width,
                 NSToIntFloor(float(aFill.y - aDest.y) / aRepeatSize.height) * aRepeatSize.height) +
         aDest;
}

/* static */ DrawResult
nsLayoutUtils::DrawBackgroundImage(gfxContext&         aContext,
                                   nsIFrame*           aForFrame,
                                   nsPresContext*      aPresContext,
                                   imgIContainer*      aImage,
                                   const CSSIntSize&   aImageSize,
                                   SamplingFilter      aSamplingFilter,
                                   const nsRect&       aDest,
                                   const nsRect&       aFill,
                                   const nsSize&       aRepeatSize,
                                   const nsPoint&      aAnchor,
                                   const nsRect&       aDirty,
                                   uint32_t            aImageFlags,
                                   ExtendMode          aExtendMode,
                                   float               aOpacity)
{
  AUTO_PROFILER_LABEL("nsLayoutUtils::DrawBackgroundImage", GRAPHICS);

  Maybe<SVGImageContext> svgContext(Some(SVGImageContext(Some(aImageSize))));
  SVGImageContext::MaybeStoreContextPaint(svgContext, aForFrame, aImage);

  /* Fast path when there is no need for image spacing */
  if (aRepeatSize.width == aDest.width && aRepeatSize.height == aDest.height) {
    return DrawImageInternal(aContext, aPresContext, aImage,
                             aSamplingFilter, aDest, aFill, aAnchor,
                             aDirty, svgContext, aImageFlags, aExtendMode,
                             aOpacity);
  }

  nsPoint firstTilePos = GetBackgroundFirstTilePos(aDest.TopLeft(), aFill.TopLeft(), aRepeatSize);
  for (int32_t i = firstTilePos.x; i < aFill.XMost(); i += aRepeatSize.width) {
    for (int32_t j = firstTilePos.y; j < aFill.YMost(); j += aRepeatSize.height) {
      nsRect dest(i, j, aDest.width, aDest.height);
      DrawResult result = DrawImageInternal(aContext, aPresContext, aImage, aSamplingFilter,
                                            dest, dest, aAnchor, aDirty, svgContext,
                                            aImageFlags, ExtendMode::CLAMP,
                                            aOpacity);
      if (result != DrawResult::SUCCESS) {
        return result;
      }
    }
  }

  return DrawResult::SUCCESS;
}

/* static */ DrawResult
nsLayoutUtils::DrawImage(gfxContext&         aContext,
                         nsStyleContext*     aStyleContext,
                         nsPresContext*      aPresContext,
                         imgIContainer*      aImage,
                         const SamplingFilter aSamplingFilter,
                         const nsRect&       aDest,
                         const nsRect&       aFill,
                         const nsPoint&      aAnchor,
                         const nsRect&       aDirty,
                         uint32_t            aImageFlags,
                         float               aOpacity)
{
  Maybe<SVGImageContext> svgContext;
  SVGImageContext::MaybeStoreContextPaint(svgContext, aStyleContext, aImage);

  return DrawImageInternal(aContext, aPresContext, aImage,
                           aSamplingFilter, aDest, aFill, aAnchor,
                           aDirty,
                           svgContext,
                           aImageFlags, ExtendMode::CLAMP,
                           aOpacity);
}

/* static */ nsRect
nsLayoutUtils::GetWholeImageDestination(const nsSize& aWholeImageSize,
                                        const nsRect& aImageSourceArea,
                                        const nsRect& aDestArea)
{
  double scaleX = double(aDestArea.width)/aImageSourceArea.width;
  double scaleY = double(aDestArea.height)/aImageSourceArea.height;
  nscoord destOffsetX = NSToCoordRound(aImageSourceArea.x*scaleX);
  nscoord destOffsetY = NSToCoordRound(aImageSourceArea.y*scaleY);
  nscoord wholeSizeX = NSToCoordRound(aWholeImageSize.width*scaleX);
  nscoord wholeSizeY = NSToCoordRound(aWholeImageSize.height*scaleY);
  return nsRect(aDestArea.TopLeft() - nsPoint(destOffsetX, destOffsetY),
                nsSize(wholeSizeX, wholeSizeY));
}

/* static */ already_AddRefed<imgIContainer>
nsLayoutUtils::OrientImage(imgIContainer* aContainer,
                           const nsStyleImageOrientation& aOrientation)
{
  MOZ_ASSERT(aContainer, "Should have an image container");
  nsCOMPtr<imgIContainer> img(aContainer);

  if (aOrientation.IsFromImage()) {
    img = ImageOps::Orient(img, img->GetOrientation());
  } else if (!aOrientation.IsDefault()) {
    Angle angle = aOrientation.Angle();
    Flip flip  = aOrientation.IsFlipped() ? Flip::Horizontal
                                          : Flip::Unflipped;
    img = ImageOps::Orient(img, Orientation(angle, flip));
  }

  return img.forget();
}

static bool NonZeroStyleCoord(const nsStyleCoord& aCoord)
{
  if (aCoord.IsCoordPercentCalcUnit()) {
    // Since negative results are clamped to 0, check > 0.
    return nsRuleNode::ComputeCoordPercentCalc(aCoord, nscoord_MAX) > 0 ||
           nsRuleNode::ComputeCoordPercentCalc(aCoord, 0) > 0;
  }

  return true;
}

/* static */ bool
nsLayoutUtils::HasNonZeroCorner(const nsStyleCorners& aCorners)
{
  NS_FOR_CSS_HALF_CORNERS(corner) {
    if (NonZeroStyleCoord(aCorners.Get(corner)))
      return true;
  }
  return false;
}

// aCorner is a "full corner" value, i.e. eCornerTopLeft etc.
static bool IsCornerAdjacentToSide(uint8_t aCorner, Side aSide)
{
  static_assert((int)eSideTop == eCornerTopLeft, "Check for Full Corner");
  static_assert((int)eSideRight == eCornerTopRight, "Check for Full Corner");
  static_assert((int)eSideBottom == eCornerBottomRight, "Check for Full Corner");
  static_assert((int)eSideLeft == eCornerBottomLeft, "Check for Full Corner");
  static_assert((int)eSideTop == ((eCornerTopRight - 1)&3), "Check for Full Corner");
  static_assert((int)eSideRight == ((eCornerBottomRight - 1)&3), "Check for Full Corner");
  static_assert((int)eSideBottom == ((eCornerBottomLeft - 1)&3), "Check for Full Corner");
  static_assert((int)eSideLeft == ((eCornerTopLeft - 1)&3), "Check for Full Corner");

  return aSide == aCorner || aSide == ((aCorner - 1)&3);
}

/* static */ bool
nsLayoutUtils::HasNonZeroCornerOnSide(const nsStyleCorners& aCorners,
                                      Side aSide)
{
  static_assert(eCornerTopLeftX/2 == eCornerTopLeft, "Check for Non Zero on side");
  static_assert(eCornerTopLeftY/2 == eCornerTopLeft, "Check for Non Zero on side");
  static_assert(eCornerTopRightX/2 == eCornerTopRight, "Check for Non Zero on side");
  static_assert(eCornerTopRightY/2 == eCornerTopRight, "Check for Non Zero on side");
  static_assert(eCornerBottomRightX/2 == eCornerBottomRight, "Check for Non Zero on side");
  static_assert(eCornerBottomRightY/2 == eCornerBottomRight, "Check for Non Zero on side");
  static_assert(eCornerBottomLeftX/2 == eCornerBottomLeft, "Check for Non Zero on side");
  static_assert(eCornerBottomLeftY/2 == eCornerBottomLeft, "Check for Non Zero on side");

  NS_FOR_CSS_HALF_CORNERS(corner) {
    // corner is a "half corner" value, so dividing by two gives us a
    // "full corner" value.
    if (NonZeroStyleCoord(aCorners.Get(corner)) &&
        IsCornerAdjacentToSide(corner/2, aSide))
      return true;
  }
  return false;
}

/* static */ nsTransparencyMode
nsLayoutUtils::GetFrameTransparency(nsIFrame* aBackgroundFrame,
                                    nsIFrame* aCSSRootFrame) {
  if (aCSSRootFrame->StyleEffects()->mOpacity < 1.0f)
    return eTransparencyTransparent;

  if (HasNonZeroCorner(aCSSRootFrame->StyleBorder()->mBorderRadius))
    return eTransparencyTransparent;

  if (aCSSRootFrame->StyleDisplay()->mAppearance == NS_THEME_WIN_GLASS)
    return eTransparencyGlass;

  if (aCSSRootFrame->StyleDisplay()->mAppearance == NS_THEME_WIN_BORDERLESS_GLASS)
    return eTransparencyBorderlessGlass;

  nsITheme::Transparency transparency;
  if (aCSSRootFrame->IsThemed(&transparency))
    return transparency == nsITheme::eTransparent
         ? eTransparencyTransparent
         : eTransparencyOpaque;

  // We need an uninitialized window to be treated as opaque because
  // doing otherwise breaks window display effects on some platforms,
  // specifically Vista. (bug 450322)
  if (aBackgroundFrame->IsViewportFrame() &&
      !aBackgroundFrame->PrincipalChildList().FirstChild()) {
    return eTransparencyOpaque;
  }

  nsStyleContext* bgSC;
  if (!nsCSSRendering::FindBackground(aBackgroundFrame, &bgSC)) {
    return eTransparencyTransparent;
  }
  const nsStyleBackground* bg = bgSC->StyleBackground();
  if (NS_GET_A(bg->BackgroundColor(bgSC)) < 255 ||
      // bottom layer's clip is used for the color
      bg->BottomLayer().mClip != StyleGeometryBox::BorderBox)
    return eTransparencyTransparent;
  return eTransparencyOpaque;
}

static bool IsPopupFrame(nsIFrame* aFrame)
{
  // aFrame is a popup it's the list control frame dropdown for a combobox.
  LayoutFrameType frameType = aFrame->Type();
  if (frameType == LayoutFrameType::ListControl) {
    nsListControlFrame* lcf = static_cast<nsListControlFrame*>(aFrame);
    return lcf->IsInDropDownMode();
  }

  // ... or if it's a XUL menupopup frame.
  return frameType == LayoutFrameType::MenuPopup;
}

/* static */ bool
nsLayoutUtils::IsPopup(nsIFrame* aFrame)
{
  // Optimization: the frame can't possibly be a popup if it has no view.
  if (!aFrame->HasView()) {
    NS_ASSERTION(!IsPopupFrame(aFrame), "popup frame must have a view");
    return false;
  }
  return IsPopupFrame(aFrame);
}

/* static */ nsIFrame*
nsLayoutUtils::GetDisplayRootFrame(nsIFrame* aFrame)
{
  // We could use GetRootPresContext() here if the
  // NS_FRAME_IN_POPUP frame bit is set.
  nsIFrame* f = aFrame;
  for (;;) {
    if (!f->HasAnyStateBits(NS_FRAME_IN_POPUP)) {
      f = f->PresContext()->FrameManager()->GetRootFrame();
    } else if (IsPopup(f)) {
      return f;
    }
    nsIFrame* parent = GetCrossDocParentFrame(f);
    if (!parent)
      return f;
    f = parent;
  }
}

/* static */ nsIFrame*
nsLayoutUtils::GetReferenceFrame(nsIFrame* aFrame)
{
  nsIFrame *f = aFrame;
  for (;;) {
    const nsStyleDisplay* disp = f->StyleDisplay();
    if (f->IsTransformed(disp) || f->IsPreserve3DLeaf(disp) || IsPopup(f)) {
      return f;
    }
    nsIFrame* parent = GetCrossDocParentFrame(f);
    if (!parent) {
      return f;
    }
    f = parent;
  }
}

/* static */ gfx::ShapedTextFlags
nsLayoutUtils::GetTextRunFlagsForStyle(nsStyleContext* aStyleContext,
                                       const nsStyleFont* aStyleFont,
                                       const nsStyleText* aStyleText,
                                       nscoord aLetterSpacing)
{
  gfx::ShapedTextFlags result = gfx::ShapedTextFlags();
  if (aLetterSpacing != 0 ||
      aStyleText->mTextJustify == StyleTextJustify::InterCharacter) {
    result |= gfx::ShapedTextFlags::TEXT_DISABLE_OPTIONAL_LIGATURES;
  }
  if (aStyleText->mControlCharacterVisibility == NS_STYLE_CONTROL_CHARACTER_VISIBILITY_HIDDEN) {
    result |= gfx::ShapedTextFlags::TEXT_HIDE_CONTROL_CHARACTERS;
  }
  switch (aStyleContext->StyleText()->mTextRendering) {
  case NS_STYLE_TEXT_RENDERING_OPTIMIZESPEED:
    result |= gfx::ShapedTextFlags::TEXT_OPTIMIZE_SPEED;
    break;
  case NS_STYLE_TEXT_RENDERING_AUTO:
    if (aStyleFont->mFont.size <
        aStyleContext->PresContext()->GetAutoQualityMinFontSize()) {
      result |= gfx::ShapedTextFlags::TEXT_OPTIMIZE_SPEED;
    }
    break;
  default:
    break;
  }
  return result | GetTextRunOrientFlagsForStyle(aStyleContext);
}

/* static */ gfx::ShapedTextFlags
nsLayoutUtils::GetTextRunOrientFlagsForStyle(nsStyleContext* aStyleContext)
{
  uint8_t writingMode = aStyleContext->StyleVisibility()->mWritingMode;
  switch (writingMode) {
  case NS_STYLE_WRITING_MODE_HORIZONTAL_TB:
    return gfx::ShapedTextFlags::TEXT_ORIENT_HORIZONTAL;

  case NS_STYLE_WRITING_MODE_VERTICAL_LR:
  case NS_STYLE_WRITING_MODE_VERTICAL_RL:
    switch (aStyleContext->StyleVisibility()->mTextOrientation) {
    case NS_STYLE_TEXT_ORIENTATION_MIXED:
      return gfx::ShapedTextFlags::TEXT_ORIENT_VERTICAL_MIXED;
    case NS_STYLE_TEXT_ORIENTATION_UPRIGHT:
      return gfx::ShapedTextFlags::TEXT_ORIENT_VERTICAL_UPRIGHT;
    case NS_STYLE_TEXT_ORIENTATION_SIDEWAYS:
      return gfx::ShapedTextFlags::TEXT_ORIENT_VERTICAL_SIDEWAYS_RIGHT;
    default:
      NS_NOTREACHED("unknown text-orientation");
      return gfx::ShapedTextFlags();
    }

  case NS_STYLE_WRITING_MODE_SIDEWAYS_LR:
    return gfx::ShapedTextFlags::TEXT_ORIENT_VERTICAL_SIDEWAYS_LEFT;

  case NS_STYLE_WRITING_MODE_SIDEWAYS_RL:
    return gfx::ShapedTextFlags::TEXT_ORIENT_VERTICAL_SIDEWAYS_RIGHT;

  default:
    NS_NOTREACHED("unknown writing-mode");
    return gfx::ShapedTextFlags();
  }
}

/* static */ void
nsLayoutUtils::GetRectDifferenceStrips(const nsRect& aR1, const nsRect& aR2,
                                       nsRect* aHStrip, nsRect* aVStrip) {
  NS_ASSERTION(aR1.TopLeft() == aR2.TopLeft(),
               "expected rects at the same position");
  nsRect unionRect(aR1.x, aR1.y, std::max(aR1.width, aR2.width),
                   std::max(aR1.height, aR2.height));
  nscoord VStripStart = std::min(aR1.width, aR2.width);
  nscoord HStripStart = std::min(aR1.height, aR2.height);
  *aVStrip = unionRect;
  aVStrip->x += VStripStart;
  aVStrip->width -= VStripStart;
  *aHStrip = unionRect;
  aHStrip->y += HStripStart;
  aHStrip->height -= HStripStart;
}

nsDeviceContext*
nsLayoutUtils::GetDeviceContextForScreenInfo(nsPIDOMWindowOuter* aWindow)
{
  if (!aWindow) {
    return nullptr;
  }

  nsCOMPtr<nsIDocShell> docShell = aWindow->GetDocShell();
  while (docShell) {
    // Now make sure our size is up to date.  That will mean that the device
    // context does the right thing on multi-monitor systems when we return it to
    // the caller.  It will also make sure that our prescontext has been created,
    // if we're supposed to have one.
    nsCOMPtr<nsPIDOMWindowOuter> win = docShell->GetWindow();
    if (!win) {
      // No reason to go on
      return nullptr;
    }

    win->EnsureSizeAndPositionUpToDate();

    RefPtr<nsPresContext> presContext;
    docShell->GetPresContext(getter_AddRefs(presContext));
    if (presContext) {
      nsDeviceContext* context = presContext->DeviceContext();
      if (context) {
        return context;
      }
    }

    nsCOMPtr<nsIDocShellTreeItem> parentItem;
    docShell->GetParent(getter_AddRefs(parentItem));
    docShell = do_QueryInterface(parentItem);
  }

  return nullptr;
}

/* static */ bool
nsLayoutUtils::IsReallyFixedPos(nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame->GetParent(),
                  "IsReallyFixedPos called on frame not in tree");
  NS_PRECONDITION(aFrame->StyleDisplay()->mPosition ==
                    NS_STYLE_POSITION_FIXED,
                  "IsReallyFixedPos called on non-'position:fixed' frame");

  LayoutFrameType parentType = aFrame->GetParent()->Type();
  return parentType == LayoutFrameType::Viewport ||
         parentType == LayoutFrameType::PageContent;
}

nsLayoutUtils::SurfaceFromElementResult
nsLayoutUtils::SurfaceFromOffscreenCanvas(OffscreenCanvas* aOffscreenCanvas,
                                          uint32_t aSurfaceFlags,
                                          RefPtr<DrawTarget>& aTarget)
{
  SurfaceFromElementResult result;

  nsIntSize size = aOffscreenCanvas->GetWidthHeight();

  result.mSourceSurface = aOffscreenCanvas->GetSurfaceSnapshot(&result.mAlphaType);
  if (!result.mSourceSurface) {
    // If the element doesn't have a context then we won't get a snapshot. The canvas spec wants us to not error and just
    // draw nothing, so return an empty surface.
    result.mAlphaType = gfxAlphaType::Opaque;
    DrawTarget *ref = aTarget ? aTarget.get() : gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget();
    RefPtr<DrawTarget> dt = ref->CreateSimilarDrawTarget(IntSize(size.width, size.height),
                                                         SurfaceFormat::B8G8R8A8);
    if (dt) {
      result.mSourceSurface = dt->Snapshot();
    }
  } else if (aTarget) {
    RefPtr<SourceSurface> opt = aTarget->OptimizeSourceSurface(result.mSourceSurface);
    if (opt) {
      result.mSourceSurface = opt;
    }
  }

  result.mHasSize = true;
  result.mSize = size;
  result.mIsWriteOnly = aOffscreenCanvas->IsWriteOnly();

  return result;
}

nsLayoutUtils::SurfaceFromElementResult
nsLayoutUtils::SurfaceFromElement(nsIImageLoadingContent* aElement,
                                  uint32_t aSurfaceFlags,
                                  RefPtr<DrawTarget>& aTarget)
{
  SurfaceFromElementResult result;
  nsresult rv;

  nsCOMPtr<imgIRequest> imgRequest;
  rv = aElement->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                            getter_AddRefs(imgRequest));
  if (NS_FAILED(rv)) {
    return result;
  }

  if (!imgRequest) {
    // There's no image request. This is either because a request for
    // a non-empty URI failed, or the URI is the empty string.
    nsCOMPtr<nsIURI> currentURI;
    aElement->GetCurrentURI(getter_AddRefs(currentURI));
    if (!currentURI) {
      // Treat the empty URI as available instead of broken state.
      result.mHasSize = true;
    }
    return result;
  }

  uint32_t status;
  imgRequest->GetImageStatus(&status);
  result.mHasSize = status & imgIRequest::STATUS_SIZE_AVAILABLE;
  if ((status & imgIRequest::STATUS_LOAD_COMPLETE) == 0) {
    // Spec says to use GetComplete, but that only works on
    // nsIDOMHTMLImageElement, and we support all sorts of other stuff
    // here.  Do this for now pending spec clarification.
    result.mIsStillLoading = (status & imgIRequest::STATUS_ERROR) == 0;
    return result;
  }

  nsCOMPtr<nsIPrincipal> principal;
  rv = imgRequest->GetImagePrincipal(getter_AddRefs(principal));
  if (NS_FAILED(rv)) {
    return result;
  }

  nsCOMPtr<imgIContainer> imgContainer;
  rv = imgRequest->GetImage(getter_AddRefs(imgContainer));
  if (NS_FAILED(rv)) {
    return result;
  }

  uint32_t noRasterize = aSurfaceFlags & SFE_NO_RASTERIZING_VECTORS;

  uint32_t whichFrame = (aSurfaceFlags & SFE_WANT_FIRST_FRAME_IF_IMAGE)
                        ? (uint32_t) imgIContainer::FRAME_FIRST
                        : (uint32_t) imgIContainer::FRAME_CURRENT;
  uint32_t frameFlags = imgIContainer::FLAG_SYNC_DECODE
                      | imgIContainer::FLAG_ASYNC_NOTIFY;
  if (aSurfaceFlags & SFE_NO_COLORSPACE_CONVERSION)
    frameFlags |= imgIContainer::FLAG_DECODE_NO_COLORSPACE_CONVERSION;
  if (aSurfaceFlags & SFE_PREFER_NO_PREMULTIPLY_ALPHA) {
    frameFlags |= imgIContainer::FLAG_DECODE_NO_PREMULTIPLY_ALPHA;
  }

  int32_t imgWidth, imgHeight;
  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  HTMLImageElement* element = HTMLImageElement::FromContentOrNull(content);
  if (aSurfaceFlags & SFE_USE_ELEMENT_SIZE_IF_VECTOR &&
      element &&
      imgContainer->GetType() == imgIContainer::TYPE_VECTOR) {
    imgWidth = element->Width();
    imgHeight = element->Height();
  } else {
    rv = imgContainer->GetWidth(&imgWidth);
    nsresult rv2 = imgContainer->GetHeight(&imgHeight);
    if (NS_FAILED(rv) || NS_FAILED(rv2))
      return result;
  }
  result.mSize = IntSize(imgWidth, imgHeight);

  if (!noRasterize || imgContainer->GetType() == imgIContainer::TYPE_RASTER) {
    if (aSurfaceFlags & SFE_WANT_IMAGE_SURFACE) {
      frameFlags |= imgIContainer::FLAG_WANT_DATA_SURFACE;
    }
    result.mSourceSurface = imgContainer->GetFrameAtSize(result.mSize, whichFrame, frameFlags);
    if (!result.mSourceSurface) {
      return result;
    }
    // The surface we return is likely to be cached. We don't want to have to
    // convert to a surface that's compatible with aTarget each time it's used
    // (that would result in terrible performance), so we convert once here
    // upfront if aTarget is specified.
    if (aTarget) {
      RefPtr<SourceSurface> optSurface =
        aTarget->OptimizeSourceSurface(result.mSourceSurface);
      if (optSurface) {
        result.mSourceSurface = optSurface;
      }
    }

    const auto& format = result.mSourceSurface->GetFormat();
    if (IsOpaque(format)) {
      result.mAlphaType = gfxAlphaType::Opaque;
    } else if (frameFlags & imgIContainer::FLAG_DECODE_NO_PREMULTIPLY_ALPHA) {
      result.mAlphaType = gfxAlphaType::NonPremult;
    } else {
      result.mAlphaType = gfxAlphaType::Premult;
    }
  } else {
    result.mDrawInfo.mImgContainer = imgContainer;
    result.mDrawInfo.mWhichFrame = whichFrame;
    result.mDrawInfo.mDrawingFlags = frameFlags;
  }

  int32_t corsmode;
  if (NS_SUCCEEDED(imgRequest->GetCORSMode(&corsmode))) {
    result.mCORSUsed = (corsmode != imgIRequest::CORS_NONE);
  }

  result.mPrincipal = principal.forget();
  // no images, including SVG images, can load content from another domain.
  result.mIsWriteOnly = false;
  result.mImageRequest = imgRequest.forget();
  return result;
}

nsLayoutUtils::SurfaceFromElementResult
nsLayoutUtils::SurfaceFromElement(HTMLImageElement *aElement,
                                  uint32_t aSurfaceFlags,
                                  RefPtr<DrawTarget>& aTarget)
{
  return SurfaceFromElement(static_cast<nsIImageLoadingContent*>(aElement),
                            aSurfaceFlags, aTarget);
}

nsLayoutUtils::SurfaceFromElementResult
nsLayoutUtils::SurfaceFromElement(HTMLCanvasElement* aElement,
                                  uint32_t aSurfaceFlags,
                                  RefPtr<DrawTarget>& aTarget)
{
  SurfaceFromElementResult result;

  IntSize size = aElement->GetSize();

  result.mSourceSurface = aElement->GetSurfaceSnapshot(&result.mAlphaType);
  if (!result.mSourceSurface) {
    // If the element doesn't have a context then we won't get a snapshot. The canvas spec wants us to not error and just
    // draw nothing, so return an empty surface.
    result.mAlphaType = gfxAlphaType::Opaque;
    DrawTarget *ref = aTarget ? aTarget.get() : gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget();
    RefPtr<DrawTarget> dt = ref->CreateSimilarDrawTarget(IntSize(size.width, size.height),
                                                        SurfaceFormat::B8G8R8A8);
    if (dt) {
      result.mSourceSurface = dt->Snapshot();
    }
  } else if (aTarget) {
    RefPtr<SourceSurface> opt = aTarget->OptimizeSourceSurface(result.mSourceSurface);
    if (opt) {
      result.mSourceSurface = opt;
    }
  }

  // Ensure that any future changes to the canvas trigger proper invalidation,
  // in case this is being used by -moz-element()
  aElement->MarkContextClean();

  result.mHasSize = true;
  result.mSize = size;
  result.mPrincipal = aElement->NodePrincipal();
  result.mIsWriteOnly = aElement->IsWriteOnly();

  return result;
}

nsLayoutUtils::SurfaceFromElementResult
nsLayoutUtils::SurfaceFromElement(HTMLVideoElement* aElement,
                                  uint32_t aSurfaceFlags,
                                  RefPtr<DrawTarget>& aTarget)
{
  SurfaceFromElementResult result;
  result.mAlphaType = gfxAlphaType::Opaque; // Assume opaque.

  if (aElement->ContainsRestrictedContent()) {
    return result;
  }

  uint16_t readyState;
  if (NS_SUCCEEDED(aElement->GetReadyState(&readyState)) &&
      (readyState == nsIDOMHTMLMediaElement::HAVE_NOTHING ||
       readyState == nsIDOMHTMLMediaElement::HAVE_METADATA)) {
    result.mIsStillLoading = true;
    return result;
  }

  // If it doesn't have a principal, just bail
  nsCOMPtr<nsIPrincipal> principal = aElement->GetCurrentVideoPrincipal();
  if (!principal)
    return result;

  result.mLayersImage = aElement->GetCurrentImage();
  if (!result.mLayersImage)
    return result;

  if (aTarget) {
    // They gave us a DrawTarget to optimize for, so even though we have a layers::Image,
    // we should unconditionally grab a SourceSurface and try to optimize it.
    result.mSourceSurface = result.mLayersImage->GetAsSourceSurface();
    if (!result.mSourceSurface)
      return result;

    RefPtr<SourceSurface> opt = aTarget->OptimizeSourceSurface(result.mSourceSurface);
    if (opt) {
      result.mSourceSurface = opt;
    }
  }

  result.mCORSUsed = aElement->GetCORSMode() != CORS_NONE;
  result.mHasSize = true;
  result.mSize = result.mLayersImage->GetSize();
  result.mPrincipal = principal.forget();
  result.mIsWriteOnly = false;

  return result;
}

nsLayoutUtils::SurfaceFromElementResult
nsLayoutUtils::SurfaceFromElement(dom::Element* aElement,
                                  uint32_t aSurfaceFlags,
                                  RefPtr<DrawTarget>& aTarget)
{
  // If it's a <canvas>, we may be able to just grab its internal surface
  if (HTMLCanvasElement* canvas =
        HTMLCanvasElement::FromContentOrNull(aElement)) {
    return SurfaceFromElement(canvas, aSurfaceFlags, aTarget);
  }

  // Maybe it's <video>?
  if (HTMLVideoElement* video =
        HTMLVideoElement::FromContentOrNull(aElement)) {
    return SurfaceFromElement(video, aSurfaceFlags, aTarget);
  }

  // Finally, check if it's a normal image
  nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(aElement);

  if (!imageLoader) {
    return SurfaceFromElementResult();
  }

  return SurfaceFromElement(imageLoader, aSurfaceFlags, aTarget);
}

/* static */
nsIContent*
nsLayoutUtils::GetEditableRootContentByContentEditable(nsIDocument* aDocument)
{
  // If the document is in designMode we should return nullptr.
  if (!aDocument || aDocument->HasFlag(NODE_IS_EDITABLE)) {
    return nullptr;
  }

  // contenteditable only works with HTML document.
  // Note: Use nsIDOMHTMLDocument rather than nsIHTMLDocument for getting the
  //       body node because nsIDOMHTMLDocument::GetBody() does something
  //       additional work for some cases and EditorBase uses them.
  nsCOMPtr<nsIDOMHTMLDocument> domHTMLDoc = do_QueryInterface(aDocument);
  if (!domHTMLDoc) {
    return nullptr;
  }

  Element* rootElement = aDocument->GetRootElement();
  if (rootElement && rootElement->IsEditable()) {
    return rootElement;
  }

  // If there are no editable root element, check its <body> element.
  // Note that the body element could be <frameset> element.
  nsCOMPtr<nsIDOMHTMLElement> body;
  nsresult rv = domHTMLDoc->GetBody(getter_AddRefs(body));
  nsCOMPtr<nsIContent> content = do_QueryInterface(body);
  if (NS_SUCCEEDED(rv) && content && content->IsEditable()) {
    return content;
  }
  return nullptr;
}

#ifdef DEBUG
/* static */ void
nsLayoutUtils::AssertNoDuplicateContinuations(nsIFrame* aContainer,
                                              const nsFrameList& aFrameList)
{
  for (nsIFrame* f : aFrameList) {
    // Check only later continuations of f; we deal with checking the
    // earlier continuations when we hit those earlier continuations in
    // the frame list.
    for (nsIFrame *c = f; (c = c->GetNextInFlow());) {
      NS_ASSERTION(c->GetParent() != aContainer ||
                   !aFrameList.ContainsFrame(c),
                   "Two continuations of the same frame in the same "
                   "frame list");
    }
  }
}

// Is one of aFrame's ancestors a letter frame?
static bool
IsInLetterFrame(nsIFrame *aFrame)
{
  for (nsIFrame *f = aFrame->GetParent(); f; f = f->GetParent()) {
    if (f->IsLetterFrame()) {
      return true;
    }
  }
  return false;
}

/* static */ void
nsLayoutUtils::AssertTreeOnlyEmptyNextInFlows(nsIFrame *aSubtreeRoot)
{
  NS_ASSERTION(aSubtreeRoot->GetPrevInFlow(),
               "frame tree not empty, but caller reported complete status");

  // Also assert that text frames map no text.
  int32_t start, end;
  nsresult rv = aSubtreeRoot->GetOffsets(start, end);
  NS_ASSERTION(NS_SUCCEEDED(rv), "GetOffsets failed");
  // In some cases involving :first-letter, we'll partially unlink a
  // continuation in the middle of a continuation chain from its
  // previous and next continuations before destroying it, presumably so
  // that we don't also destroy the later continuations.  Once we've
  // done this, GetOffsets returns incorrect values.
  // For examples, see list of tests in
  // https://bugzilla.mozilla.org/show_bug.cgi?id=619021#c29
  NS_ASSERTION(start == end || IsInLetterFrame(aSubtreeRoot),
               "frame tree not empty, but caller reported complete status");

  nsIFrame::ChildListIterator lists(aSubtreeRoot);
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      nsLayoutUtils::AssertTreeOnlyEmptyNextInFlows(childFrames.get());
    }
  }
}
#endif

static void
GetFontFacesForFramesInner(nsIFrame* aFrame, nsFontFaceList* aFontFaceList)
{
  NS_PRECONDITION(aFrame, "NULL frame pointer");

  if (aFrame->IsTextFrame()) {
    if (!aFrame->GetPrevContinuation()) {
      nsLayoutUtils::GetFontFacesForText(aFrame, 0, INT32_MAX, true,
                                         aFontFaceList);
    }
    return;
  }

  nsIFrame::ChildListID childLists[] = { nsIFrame::kPrincipalList,
                                         nsIFrame::kPopupList };
  for (size_t i = 0; i < ArrayLength(childLists); ++i) {
    nsFrameList children(aFrame->GetChildList(childLists[i]));
    for (nsFrameList::Enumerator e(children); !e.AtEnd(); e.Next()) {
      nsIFrame* child = e.get();
      child = nsPlaceholderFrame::GetRealFrameFor(child);
      GetFontFacesForFramesInner(child, aFontFaceList);
    }
  }
}

/* static */
nsresult
nsLayoutUtils::GetFontFacesForFrames(nsIFrame* aFrame,
                                     nsFontFaceList* aFontFaceList)
{
  NS_PRECONDITION(aFrame, "NULL frame pointer");

  while (aFrame) {
    GetFontFacesForFramesInner(aFrame, aFontFaceList);
    aFrame = GetNextContinuationOrIBSplitSibling(aFrame);
  }

  return NS_OK;
}

/* static */
nsresult
nsLayoutUtils::GetFontFacesForText(nsIFrame* aFrame,
                                   int32_t aStartOffset, int32_t aEndOffset,
                                   bool aFollowContinuations,
                                   nsFontFaceList* aFontFaceList)
{
  NS_PRECONDITION(aFrame, "NULL frame pointer");

  if (!aFrame->IsTextFrame()) {
    return NS_OK;
  }

  nsTextFrame* curr = static_cast<nsTextFrame*>(aFrame);
  do {
    int32_t fstart = std::max(curr->GetContentOffset(), aStartOffset);
    int32_t fend = std::min(curr->GetContentEnd(), aEndOffset);
    if (fstart >= fend) {
      curr = static_cast<nsTextFrame*>(curr->GetNextContinuation());
      continue;
    }

    // curr is overlapping with the offset we want
    gfxSkipCharsIterator iter = curr->EnsureTextRun(nsTextFrame::eInflated);
    gfxTextRun* textRun = curr->GetTextRun(nsTextFrame::eInflated);
    NS_ENSURE_TRUE(textRun, NS_ERROR_OUT_OF_MEMORY);

    // include continuations in the range that share the same textrun
    nsTextFrame* next = nullptr;
    if (aFollowContinuations && fend < aEndOffset) {
      next = static_cast<nsTextFrame*>(curr->GetNextContinuation());
      while (next && next->GetTextRun(nsTextFrame::eInflated) == textRun) {
        fend = std::min(next->GetContentEnd(), aEndOffset);
        next = fend < aEndOffset ?
          static_cast<nsTextFrame*>(next->GetNextContinuation()) : nullptr;
      }
    }

    uint32_t skipStart = iter.ConvertOriginalToSkipped(fstart);
    uint32_t skipEnd = iter.ConvertOriginalToSkipped(fend);
    aFontFaceList->AddFontsFromTextRun(textRun, skipStart, skipEnd - skipStart);
    curr = next;
  } while (aFollowContinuations && curr);

  return NS_OK;
}

/* static */
size_t
nsLayoutUtils::SizeOfTextRunsForFrames(nsIFrame* aFrame,
                                       MallocSizeOf aMallocSizeOf,
                                       bool clear)
{
  NS_PRECONDITION(aFrame, "NULL frame pointer");

  size_t total = 0;

  if (aFrame->IsTextFrame()) {
    nsTextFrame* textFrame = static_cast<nsTextFrame*>(aFrame);
    for (uint32_t i = 0; i < 2; ++i) {
      gfxTextRun *run = textFrame->GetTextRun(
        (i != 0) ? nsTextFrame::eInflated : nsTextFrame::eNotInflated);
      if (run) {
        if (clear) {
          run->ResetSizeOfAccountingFlags();
        } else {
          total += run->MaybeSizeOfIncludingThis(aMallocSizeOf);
        }
      }
    }
    return total;
  }

  AutoTArray<nsIFrame::ChildList,4> childListArray;
  aFrame->GetChildLists(&childListArray);

  for (nsIFrame::ChildListArrayIterator childLists(childListArray);
       !childLists.IsDone(); childLists.Next()) {
    for (nsFrameList::Enumerator e(childLists.CurrentList());
         !e.AtEnd(); e.Next()) {
      total += SizeOfTextRunsForFrames(e.get(), aMallocSizeOf, clear);
    }
  }
  return total;
}

struct PrefCallbacks
{
  const char* name;
  PrefChangedFunc func;
};
static const PrefCallbacks kPrefCallbacks[] = {
  { GRID_ENABLED_PREF_NAME,
    GridEnabledPrefChangeCallback },
  { WEBKIT_PREFIXES_ENABLED_PREF_NAME,
    WebkitPrefixEnabledPrefChangeCallback },
  { TEXT_ALIGN_UNSAFE_ENABLED_PREF_NAME,
    TextAlignUnsafeEnabledPrefChangeCallback },
  { FLOAT_LOGICAL_VALUES_ENABLED_PREF_NAME,
    FloatLogicalValuesEnabledPrefChangeCallback },
};

/* static */
void
nsLayoutUtils::Initialize()
{
  Preferences::AddUintVarCache(&sFontSizeInflationMaxRatio,
                               "font.size.inflation.maxRatio");
  Preferences::AddUintVarCache(&sFontSizeInflationEmPerLine,
                               "font.size.inflation.emPerLine");
  Preferences::AddUintVarCache(&sFontSizeInflationMinTwips,
                               "font.size.inflation.minTwips");
  Preferences::AddUintVarCache(&sFontSizeInflationLineThreshold,
                               "font.size.inflation.lineThreshold");
  Preferences::AddIntVarCache(&sFontSizeInflationMappingIntercept,
                              "font.size.inflation.mappingIntercept");
  Preferences::AddBoolVarCache(&sFontSizeInflationForceEnabled,
                               "font.size.inflation.forceEnabled");
  Preferences::AddBoolVarCache(&sFontSizeInflationDisabledInMasterProcess,
                               "font.size.inflation.disabledInMasterProcess");
  Preferences::AddUintVarCache(&sSystemFontScale,
                               "font.size.systemFontScale", 100);
  Preferences::AddUintVarCache(&sZoomMaxPercent,
                               "zoom.maxPercent", 300);
  Preferences::AddUintVarCache(&sZoomMinPercent,
                               "zoom.minPercent", 30);
  Preferences::AddBoolVarCache(&sInvalidationDebuggingIsEnabled,
                               "nglayout.debug.invalidation");
  Preferences::AddBoolVarCache(&sInterruptibleReflowEnabled,
                               "layout.interruptible-reflow.enabled");
  Preferences::AddBoolVarCache(&sSVGTransformBoxEnabled,
                               "svg.transform-box.enabled");
  Preferences::AddBoolVarCache(&sTextCombineUprightDigitsEnabled,
                               "layout.css.text-combine-upright-digits.enabled");
#ifdef MOZ_STYLO
  Preferences::AddBoolVarCache(&sStyloEnabled,
                               "layout.css.servo.enabled");
#endif
  Preferences::AddBoolVarCache(&sStyleAttrWithXMLBaseDisabled,
                               "layout.css.style-attr-with-xml-base.disabled");
  Preferences::AddUintVarCache(&sIdlePeriodDeadlineLimit,
                               "layout.idle_period.time_limit",
                               DEFAULT_IDLE_PERIOD_TIME_LIMIT);
  Preferences::AddUintVarCache(&sQuiescentFramesBeforeIdlePeriod,
                               "layout.idle_period.required_quiescent_frames",
                               DEFAULT_QUIESCENT_FRAMES);

  for (auto& callback : kPrefCallbacks) {
    Preferences::RegisterCallbackAndCall(callback.func, callback.name);
  }
  nsComputedDOMStyle::RegisterPrefChangeCallbacks();
}

/* static */
void
nsLayoutUtils::Shutdown()
{
  if (sContentMap) {
    delete sContentMap;
    sContentMap = nullptr;
  }

  for (auto& callback : kPrefCallbacks) {
    Preferences::UnregisterCallback(callback.func, callback.name);
  }
  nsComputedDOMStyle::UnregisterPrefChangeCallbacks();

  // so the cached initial quotes array doesn't appear to be a leak
  nsStyleList::Shutdown();
}

/* static */
void
nsLayoutUtils::RegisterImageRequest(nsPresContext* aPresContext,
                                    imgIRequest* aRequest,
                                    bool* aRequestRegistered)
{
  if (!aPresContext) {
    return;
  }

  if (aRequestRegistered && *aRequestRegistered) {
    // Our request is already registered with the refresh driver, so
    // no need to register it again.
    return;
  }

  if (aRequest) {
    if (!aPresContext->RefreshDriver()->AddImageRequest(aRequest)) {
      NS_WARNING("Unable to add image request");
      return;
    }

    if (aRequestRegistered) {
      *aRequestRegistered = true;
    }
  }
}

/* static */
void
nsLayoutUtils::RegisterImageRequestIfAnimated(nsPresContext* aPresContext,
                                              imgIRequest* aRequest,
                                              bool* aRequestRegistered)
{
  if (!aPresContext) {
    return;
  }

  if (aRequestRegistered && *aRequestRegistered) {
    // Our request is already registered with the refresh driver, so
    // no need to register it again.
    return;
  }

  if (aRequest) {
    nsCOMPtr<imgIContainer> image;
    if (NS_SUCCEEDED(aRequest->GetImage(getter_AddRefs(image)))) {
      // Check to verify that the image is animated. If so, then add it to the
      // list of images tracked by the refresh driver.
      bool isAnimated = false;
      nsresult rv = image->GetAnimated(&isAnimated);
      if (NS_SUCCEEDED(rv) && isAnimated) {
        if (!aPresContext->RefreshDriver()->AddImageRequest(aRequest)) {
          NS_WARNING("Unable to add image request");
          return;
        }

        if (aRequestRegistered) {
          *aRequestRegistered = true;
        }
      }
    }
  }
}

/* static */
void
nsLayoutUtils::DeregisterImageRequest(nsPresContext* aPresContext,
                                      imgIRequest* aRequest,
                                      bool* aRequestRegistered)
{
  if (!aPresContext) {
    return;
  }

  // Deregister our imgIRequest with the refresh driver to
  // complete tear-down, but only if it has been registered
  if (aRequestRegistered && !*aRequestRegistered) {
    return;
  }

  if (aRequest) {
    nsCOMPtr<imgIContainer> image;
    if (NS_SUCCEEDED(aRequest->GetImage(getter_AddRefs(image)))) {
      aPresContext->RefreshDriver()->RemoveImageRequest(aRequest);

      if (aRequestRegistered) {
        *aRequestRegistered = false;
      }
    }
  }
}

/* static */
void
nsLayoutUtils::PostRestyleEvent(Element* aElement,
                                nsRestyleHint aRestyleHint,
                                nsChangeHint aMinChangeHint)
{
  nsIDocument* doc = aElement->GetComposedDoc();
  if (doc) {
    nsCOMPtr<nsIPresShell> presShell = doc->GetShell();
    if (presShell) {
      presShell->GetPresContext()->RestyleManager()->PostRestyleEvent(
        aElement, aRestyleHint, aMinChangeHint);
    }
  }
}

nsSetAttrRunnable::nsSetAttrRunnable(nsIContent* aContent, nsIAtom* aAttrName,
                                     const nsAString& aValue)
  : mContent(aContent),
    mAttrName(aAttrName),
    mValue(aValue)
{
  NS_ASSERTION(aContent && aAttrName, "Missing stuff, prepare to crash");
}

nsSetAttrRunnable::nsSetAttrRunnable(nsIContent* aContent, nsIAtom* aAttrName,
                                     int32_t aValue)
  : mContent(aContent),
    mAttrName(aAttrName)
{
  NS_ASSERTION(aContent && aAttrName, "Missing stuff, prepare to crash");
  mValue.AppendInt(aValue);
}

NS_IMETHODIMP
nsSetAttrRunnable::Run()
{
  return mContent->SetAttr(kNameSpaceID_None, mAttrName, mValue, true);
}

nsUnsetAttrRunnable::nsUnsetAttrRunnable(nsIContent* aContent,
                                         nsIAtom* aAttrName)
  : mContent(aContent),
    mAttrName(aAttrName)
{
  NS_ASSERTION(aContent && aAttrName, "Missing stuff, prepare to crash");
}

NS_IMETHODIMP
nsUnsetAttrRunnable::Run()
{
  return mContent->UnsetAttr(kNameSpaceID_None, mAttrName, true);
}

/**
 * Compute the minimum font size inside of a container with the given
 * width, such that **when the user zooms the container to fill the full
 * width of the device**, the fonts satisfy our minima.
 */
static nscoord
MinimumFontSizeFor(nsPresContext* aPresContext, WritingMode aWritingMode,
                   nscoord aContainerISize)
{
  nsIPresShell* presShell = aPresContext->PresShell();

  uint32_t emPerLine = presShell->FontSizeInflationEmPerLine();
  uint32_t minTwips = presShell->FontSizeInflationMinTwips();
  if (emPerLine == 0 && minTwips == 0) {
    return 0;
  }

  // Clamp the container width to the device dimensions
  nscoord iFrameISize = aWritingMode.IsVertical()
    ? aPresContext->GetVisibleArea().height
    : aPresContext->GetVisibleArea().width;
  nscoord effectiveContainerISize = std::min(iFrameISize, aContainerISize);

  nscoord byLine = 0, byInch = 0;
  if (emPerLine != 0) {
    byLine = effectiveContainerISize / emPerLine;
  }
  if (minTwips != 0) {
    // REVIEW: Is this giving us app units and sizes *not* counting
    // viewport scaling?
    gfxSize screenSize = aPresContext->ScreenSizeInchesForFontInflation();
    float deviceISizeInches = aWritingMode.IsVertical()
      ? screenSize.height : screenSize.width;
    byInch = NSToCoordRound(effectiveContainerISize /
                            (deviceISizeInches * 1440 /
                             minTwips ));
  }
  return std::max(byLine, byInch);
}

/* static */ float
nsLayoutUtils::FontSizeInflationInner(const nsIFrame *aFrame,
                                      nscoord aMinFontSize)
{
  // Note that line heights should be inflated by the same ratio as the
  // font size of the same text; thus we operate only on the font size
  // even when we're scaling a line height.
  nscoord styleFontSize = aFrame->StyleFont()->mFont.size;
  if (styleFontSize <= 0) {
    // Never scale zero font size.
    return 1.0;
  }

  if (aMinFontSize <= 0) {
    // No need to scale.
    return 1.0;
  }

  // If between this current frame and its font inflation container there is a
  // non-inline element with fixed width or height, then we should not inflate
  // fonts for this frame.
  for (const nsIFrame* f = aFrame;
       f && !f->IsContainerForFontSizeInflation();
       f = f->GetParent()) {
    nsIContent* content = f->GetContent();
    LayoutFrameType fType = f->Type();
    nsIFrame* parent = f->GetParent();
    // Also, if there is more than one frame corresponding to a single
    // content node, we want the outermost one.
    if (!(parent && parent->GetContent() == content) &&
        // ignore width/height on inlines since they don't apply
        fType != LayoutFrameType::Inline &&
        // ignore width on radios and checkboxes since we enlarge them and
        // they have width/height in ua.css
        fType != LayoutFrameType::FormControl) {
      // ruby annotations should have the same inflation as its
      // grandparent, which is the ruby frame contains the annotation.
      if (fType == LayoutFrameType::RubyText) {
        MOZ_ASSERT(parent && parent->IsRubyTextContainerFrame());
        nsIFrame* grandparent = parent->GetParent();
        MOZ_ASSERT(grandparent && grandparent->IsRubyFrame());
        return FontSizeInflationFor(grandparent);
      }
      nsStyleCoord stylePosWidth = f->StylePosition()->mWidth;
      nsStyleCoord stylePosHeight = f->StylePosition()->mHeight;
      if (stylePosWidth.GetUnit() != eStyleUnit_Auto ||
          stylePosHeight.GetUnit() != eStyleUnit_Auto) {

        return 1.0;
      }
    }
  }

  int32_t interceptParam = nsLayoutUtils::FontSizeInflationMappingIntercept();
  float maxRatio = (float)nsLayoutUtils::FontSizeInflationMaxRatio() / 100.0f;

  float ratio = float(styleFontSize) / float(aMinFontSize);
  float inflationRatio;

  // Given a minimum inflated font size m, a specified font size s, we want to
  // find the inflated font size i and then return the ratio of i to s (i/s).
  if (interceptParam >= 0) {
    // Since the mapping intercept parameter P is greater than zero, we use it
    // to determine the point where our mapping function intersects the i=s
    // line. This means that we have an equation of the form:
    //
    // i = m + s·(P/2)/(1 + P/2), if s <= (1 + P/2)·m
    // i = s, if s >= (1 + P/2)·m

    float intercept = 1 + float(interceptParam)/2.0f;
    if (ratio >= intercept) {
      // If we're already at 1+P/2 or more times the minimum, don't scale.
      return 1.0;
    }

    // The point (intercept, intercept) is where the part of the i vs. s graph
    // that's not slope 1 meets the i=s line.  (This part of the
    // graph is a line from (0, m), to that point). We calculate the
    // intersection point to be ((1+P/2)m, (1+P/2)m), where P is the
    // intercept parameter above. We then need to return i/s.
    inflationRatio = (1.0f + (ratio * (intercept - 1) / intercept)) / ratio;
  } else {
    // This is the case where P is negative. We essentially want to implement
    // the case for P=infinity here, so we make i = s + m, which means that
    // i/s = s/s + m/s = 1 + 1/ratio
    inflationRatio = 1 + 1.0f / ratio;
  }

  if (maxRatio > 1.0 && inflationRatio > maxRatio) {
    return maxRatio;
  } else {
    return inflationRatio;
  }
}

static bool
ShouldInflateFontsForContainer(const nsIFrame *aFrame)
{
  // We only want to inflate fonts for text that is in a place
  // with room to expand.  The question is what the best heuristic for
  // that is...
  // For now, we're going to use NS_FRAME_IN_CONSTRAINED_BSIZE, which
  // indicates whether the frame is inside something with a constrained
  // block-size (propagating down the tree), but the propagation stops when
  // we hit overflow-y [or -x, for vertical mode]: scroll or auto.
  const nsStyleText* styleText = aFrame->StyleText();

  return styleText->mTextSizeAdjust != NS_STYLE_TEXT_SIZE_ADJUST_NONE &&
         !(aFrame->GetStateBits() & NS_FRAME_IN_CONSTRAINED_BSIZE) &&
         // We also want to disable font inflation for containers that have
         // preformatted text.
         // MathML cells need special treatment. See bug 1002526 comment 56.
         (styleText->WhiteSpaceCanWrap(aFrame) ||
          aFrame->IsFrameOfType(nsIFrame::eMathML));
}

nscoord
nsLayoutUtils::InflationMinFontSizeFor(const nsIFrame *aFrame)
{
  nsPresContext *presContext = aFrame->PresContext();
  if (!FontSizeInflationEnabled(presContext) ||
      presContext->mInflationDisabledForShrinkWrap) {
    return 0;
  }

  for (const nsIFrame *f = aFrame; f; f = f->GetParent()) {
    if (f->IsContainerForFontSizeInflation()) {
      if (!ShouldInflateFontsForContainer(f)) {
        return 0;
      }

      nsFontInflationData *data =
        nsFontInflationData::FindFontInflationDataFor(aFrame);
      // FIXME: The need to null-check here is sort of a bug, and might
      // lead to incorrect results.
      if (!data || !data->InflationEnabled()) {
        return 0;
      }

      return MinimumFontSizeFor(aFrame->PresContext(),
                                aFrame->GetWritingMode(),
                                data->EffectiveISize());
    }
  }

  MOZ_ASSERT(false, "root should always be container");

  return 0;
}

float
nsLayoutUtils::FontSizeInflationFor(const nsIFrame *aFrame)
{
  if (nsSVGUtils::IsInSVGTextSubtree(aFrame)) {
    const nsIFrame* container = aFrame;
    while (!container->IsSVGTextFrame()) {
      container = container->GetParent();
    }
    NS_ASSERTION(container, "expected to find an ancestor SVGTextFrame");
    return
      static_cast<const SVGTextFrame*>(container)->GetFontSizeScaleFactor();
  }

  if (!FontSizeInflationEnabled(aFrame->PresContext())) {
    return 1.0f;
  }

  return FontSizeInflationInner(aFrame, InflationMinFontSizeFor(aFrame));
}

/* static */ bool
nsLayoutUtils::FontSizeInflationEnabled(nsPresContext *aPresContext)
{
  nsIPresShell* presShell = aPresContext->GetPresShell();

  if (!presShell) {
    return false;
  }

  return presShell->FontSizeInflationEnabled();
}

/* static */ nsRect
nsLayoutUtils::GetBoxShadowRectForFrame(nsIFrame* aFrame,
                                        const nsSize& aFrameSize)
{
  nsCSSShadowArray* boxShadows = aFrame->StyleEffects()->mBoxShadow;
  if (!boxShadows) {
    return nsRect();
  }

  bool nativeTheme;
  const nsStyleDisplay* styleDisplay = aFrame->StyleDisplay();
  nsITheme::Transparency transparency;
  if (aFrame->IsThemed(styleDisplay, &transparency)) {
    // For opaque (rectangular) theme widgets we can take the generic
    // border-box path with border-radius disabled.
    nativeTheme = transparency != nsITheme::eOpaque;
  } else {
    nativeTheme = false;
  }

  nsRect frameRect = nativeTheme ?
    aFrame->GetVisualOverflowRectRelativeToSelf() :
    nsRect(nsPoint(0, 0), aFrameSize);

  nsRect shadows;
  int32_t A2D = aFrame->PresContext()->AppUnitsPerDevPixel();
  for (uint32_t i = 0; i < boxShadows->Length(); ++i) {
    nsRect tmpRect = frameRect;
    nsCSSShadowItem* shadow = boxShadows->ShadowAt(i);

    // inset shadows are never painted outside the frame
    if (shadow->mInset)
      continue;

    tmpRect.MoveBy(nsPoint(shadow->mXOffset, shadow->mYOffset));
    tmpRect.Inflate(shadow->mSpread);
    tmpRect.Inflate(
      nsContextBoxBlur::GetBlurRadiusMargin(shadow->mRadius, A2D));
    shadows.UnionRect(shadows, tmpRect);
  }
  return shadows;
}

/* static */ bool
nsLayoutUtils::GetContentViewerSize(nsPresContext* aPresContext,
                                    LayoutDeviceIntSize& aOutSize)
{
  nsCOMPtr<nsIDocShell> docShell = aPresContext->GetDocShell();
  if (!docShell) {
    return false;
  }

  nsCOMPtr<nsIContentViewer> cv;
  docShell->GetContentViewer(getter_AddRefs(cv));
  if (!cv) {
    return false;
  }

  nsIntRect bounds;
  cv->GetBounds(bounds);
  aOutSize = LayoutDeviceIntRect::FromUnknownRect(bounds).Size();
  return true;
}

static bool
UpdateCompositionBoundsForRCDRSF(ParentLayerRect& aCompBounds,
                                 nsPresContext* aPresContext,
                                 bool aScaleContentViewerSize)
{
  nsIFrame* rootFrame = aPresContext->PresShell()->GetRootFrame();
  if (!rootFrame) {
    return false;
  }

#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_UIKIT)
  nsIWidget* widget = rootFrame->GetNearestWidget();
#else
  nsView* view = rootFrame->GetView();
  nsIWidget* widget = view ? view->GetWidget() : nullptr;
#endif

  if (widget) {
    LayoutDeviceIntRect widgetBounds = widget->GetBounds();
    widgetBounds.MoveTo(0, 0);
    aCompBounds = ParentLayerRect(
      ViewAs<ParentLayerPixel>(
        widgetBounds,
        PixelCastJustification::LayoutDeviceIsParentLayerForRCDRSF));
    return true;
  }

  LayoutDeviceIntSize contentSize;
  if (nsLayoutUtils::GetContentViewerSize(aPresContext, contentSize)) {
    LayoutDeviceToParentLayerScale scale;
    if (aScaleContentViewerSize && aPresContext->GetParentPresContext()) {
      scale = LayoutDeviceToParentLayerScale(
        aPresContext->GetParentPresContext()->PresShell()->GetCumulativeResolution());
    }
    aCompBounds.SizeTo(contentSize * scale);
    return true;
  }

  return false;
}

/* static */ nsMargin
nsLayoutUtils::ScrollbarAreaToExcludeFromCompositionBoundsFor(nsIFrame* aScrollFrame)
{
  if (!aScrollFrame || !aScrollFrame->GetScrollTargetFrame()) {
    return nsMargin();
  }
  nsPresContext* presContext = aScrollFrame->PresContext();
  nsIPresShell* presShell = presContext->GetPresShell();
  if (!presShell) {
    return nsMargin();
  }
  bool isRootScrollFrame = aScrollFrame == presShell->GetRootScrollFrame();
  bool isRootContentDocRootScrollFrame = isRootScrollFrame
                                      && presContext->IsRootContentDocument();
  if (!isRootContentDocRootScrollFrame) {
    return nsMargin();
  }
  if (LookAndFeel::GetInt(LookAndFeel::eIntID_UseOverlayScrollbars)) {
    return nsMargin();
  }
  nsIScrollableFrame* scrollableFrame = aScrollFrame->GetScrollTargetFrame();
  if (!scrollableFrame) {
    return nsMargin();
  }
  return scrollableFrame->GetActualScrollbarSizes();
}

/* static */ nsSize
nsLayoutUtils::CalculateCompositionSizeForFrame(nsIFrame* aFrame, bool aSubtractScrollbars)
{
  // If we have a scrollable frame, restrict the composition bounds to its
  // scroll port. The scroll port excludes the frame borders and the scroll
  // bars, which we don't want to be part of the composition bounds.
  nsIScrollableFrame* scrollableFrame = aFrame->GetScrollTargetFrame();
  nsRect rect = scrollableFrame ? scrollableFrame->GetScrollPortRect() : aFrame->GetRect();
  nsSize size = rect.Size();

  nsPresContext* presContext = aFrame->PresContext();
  nsIPresShell* presShell = presContext->PresShell();

  bool isRootContentDocRootScrollFrame = presContext->IsRootContentDocument()
                                      && aFrame == presShell->GetRootScrollFrame();
  if (isRootContentDocRootScrollFrame) {
    ParentLayerRect compBounds;
    if (UpdateCompositionBoundsForRCDRSF(compBounds, presContext, false)) {
      int32_t auPerDevPixel = presContext->AppUnitsPerDevPixel();
      size = nsSize(compBounds.width * auPerDevPixel, compBounds.height * auPerDevPixel);
    }
  }

  if (aSubtractScrollbars) {
    nsMargin margins = ScrollbarAreaToExcludeFromCompositionBoundsFor(aFrame);
    size.width -= margins.LeftRight();
    size.height -= margins.TopBottom();
  }

  return size;
}

/* static */ CSSSize
nsLayoutUtils::CalculateRootCompositionSize(nsIFrame* aFrame,
                                            bool aIsRootContentDocRootScrollFrame,
                                            const FrameMetrics& aMetrics)
{

  if (aIsRootContentDocRootScrollFrame) {
    return ViewAs<LayerPixel>(aMetrics.GetCompositionBounds().Size(),
                              PixelCastJustification::ParentLayerToLayerForRootComposition)
           * LayerToScreenScale(1.0f)
           / aMetrics.DisplayportPixelsPerCSSPixel();
  }
  nsPresContext* presContext = aFrame->PresContext();
  ScreenSize rootCompositionSize;
  nsPresContext* rootPresContext =
    presContext->GetToplevelContentDocumentPresContext();
  if (!rootPresContext) {
    rootPresContext = presContext->GetRootPresContext();
  }
  nsIPresShell* rootPresShell = nullptr;
  if (rootPresContext) {
    rootPresShell = rootPresContext->PresShell();
    if (nsIFrame* rootFrame = rootPresShell->GetRootFrame()) {
      LayoutDeviceToLayerScale2D cumulativeResolution(
        rootPresShell->GetCumulativeResolution()
      * nsLayoutUtils::GetTransformToAncestorScale(rootFrame));
      ParentLayerRect compBounds;
      if (UpdateCompositionBoundsForRCDRSF(compBounds, rootPresContext, true)) {
        rootCompositionSize = ViewAs<ScreenPixel>(compBounds.Size(),
            PixelCastJustification::ScreenIsParentLayerForRoot);
      } else {
        int32_t rootAUPerDevPixel = rootPresContext->AppUnitsPerDevPixel();
        LayerSize frameSize =
          (LayoutDeviceRect::FromAppUnits(rootFrame->GetRect(), rootAUPerDevPixel)
           * cumulativeResolution).Size();
        rootCompositionSize = frameSize * LayerToScreenScale(1.0f);
      }
    }
  } else {
    nsIWidget* widget = aFrame->GetNearestWidget();
    LayoutDeviceIntRect widgetBounds = widget->GetBounds();
    rootCompositionSize = ScreenSize(
      ViewAs<ScreenPixel>(widgetBounds.Size(),
                          PixelCastJustification::LayoutDeviceIsScreenForBounds));
  }

  // Adjust composition size for the size of scroll bars.
  nsIFrame* rootRootScrollFrame = rootPresShell ? rootPresShell->GetRootScrollFrame() : nullptr;
  nsMargin scrollbarMargins = ScrollbarAreaToExcludeFromCompositionBoundsFor(rootRootScrollFrame);
  LayoutDeviceMargin margins = LayoutDeviceMargin::FromAppUnits(scrollbarMargins,
    rootPresContext->AppUnitsPerDevPixel());
  // Scrollbars are not subject to resolution scaling, so LD pixels = layer pixels for them.
  rootCompositionSize.width -= margins.LeftRight();
  rootCompositionSize.height -= margins.TopBottom();

  return rootCompositionSize / aMetrics.DisplayportPixelsPerCSSPixel();
}

/* static */ nsRect
nsLayoutUtils::CalculateScrollableRectForFrame(nsIScrollableFrame* aScrollableFrame, nsIFrame* aRootFrame)
{
  nsRect contentBounds;
  if (aScrollableFrame) {
    contentBounds = aScrollableFrame->GetScrollRange();

    nsPoint scrollPosition = aScrollableFrame->GetScrollPosition();
    if (aScrollableFrame->GetScrollbarStyles().mVertical == NS_STYLE_OVERFLOW_HIDDEN) {
      contentBounds.y = scrollPosition.y;
      contentBounds.height = 0;
    }
    if (aScrollableFrame->GetScrollbarStyles().mHorizontal == NS_STYLE_OVERFLOW_HIDDEN) {
      contentBounds.x = scrollPosition.x;
      contentBounds.width = 0;
    }

    contentBounds.width += aScrollableFrame->GetScrollPortRect().width;
    contentBounds.height += aScrollableFrame->GetScrollPortRect().height;
  } else {
    contentBounds = aRootFrame->GetRect();
  }
  return contentBounds;
}

/* static */ nsRect
nsLayoutUtils::CalculateExpandedScrollableRect(nsIFrame* aFrame)
{
  nsRect scrollableRect =
    CalculateScrollableRectForFrame(aFrame->GetScrollTargetFrame(),
                                    aFrame->PresContext()->PresShell()->GetRootFrame());
  nsSize compSize = CalculateCompositionSizeForFrame(aFrame);

  if (aFrame == aFrame->PresContext()->PresShell()->GetRootScrollFrame()) {
    // the composition size for the root scroll frame does not include the
    // local resolution, so we adjust.
    float res = aFrame->PresContext()->PresShell()->GetResolution();
    compSize.width = NSToCoordRound(compSize.width / res);
    compSize.height = NSToCoordRound(compSize.height / res);
  }

  if (scrollableRect.width < compSize.width) {
    scrollableRect.x = std::max(0,
                                scrollableRect.x - (compSize.width - scrollableRect.width));
    scrollableRect.width = compSize.width;
  }

  if (scrollableRect.height < compSize.height) {
    scrollableRect.y = std::max(0,
                                scrollableRect.y - (compSize.height - scrollableRect.height));
    scrollableRect.height = compSize.height;
  }
  return scrollableRect;
}

/* static */ void
nsLayoutUtils::DoLogTestDataForPaint(LayerManager* aManager,
                                     ViewID aScrollId,
                                     const std::string& aKey,
                                     const std::string& aValue)
{
  if (ClientLayerManager* mgr = aManager->AsClientLayerManager()) {
    mgr->LogTestDataForCurrentPaint(aScrollId, aKey, aValue);
  } else if (WebRenderLayerManager* wrlm = aManager->AsWebRenderLayerManager()) {
    wrlm->LogTestDataForCurrentPaint(aScrollId, aKey, aValue);
  }
}

/* static */ bool
nsLayoutUtils::IsAPZTestLoggingEnabled()
{
  return gfxPrefs::APZTestLoggingEnabled();
}

////////////////////////////////////////
// SurfaceFromElementResult

nsLayoutUtils::SurfaceFromElementResult::SurfaceFromElementResult()
  // Use safe default values here
  : mIsWriteOnly(true)
  , mIsStillLoading(false)
  , mHasSize(false)
  , mCORSUsed(false)
  , mAlphaType(gfxAlphaType::Opaque)
{
}

const RefPtr<mozilla::gfx::SourceSurface>&
nsLayoutUtils::SurfaceFromElementResult::GetSourceSurface()
{
  if (!mSourceSurface && mLayersImage) {
    mSourceSurface = mLayersImage->GetAsSourceSurface();
  }

  return mSourceSurface;
}

////////////////////////////////////////

bool
nsLayoutUtils::IsNonWrapperBlock(nsIFrame* aFrame)
{
  return GetAsBlock(aFrame) && !aFrame->IsBlockWrapper();
}

bool
nsLayoutUtils::NeedsPrintPreviewBackground(nsPresContext* aPresContext)
{
  return aPresContext->IsRootPaginatedDocument() &&
    (aPresContext->Type() == nsPresContext::eContext_PrintPreview ||
     aPresContext->Type() == nsPresContext::eContext_PageLayout);
}

AutoMaybeDisableFontInflation::AutoMaybeDisableFontInflation(nsIFrame *aFrame)
{
  // FIXME: Now that inflation calculations are based on the flow
  // root's NCA's (nearest common ancestor of its inflatable
  // descendants) width, we could probably disable inflation in
  // fewer cases than we currently do.
  // MathML cells need special treatment. See bug 1002526 comment 56.
  if (aFrame->IsContainerForFontSizeInflation() &&
      !aFrame->IsFrameOfType(nsIFrame::eMathML)) {
    mPresContext = aFrame->PresContext();
    mOldValue = mPresContext->mInflationDisabledForShrinkWrap;
    mPresContext->mInflationDisabledForShrinkWrap = true;
  } else {
    // indicate we have nothing to restore
    mPresContext = nullptr;
  }
}

AutoMaybeDisableFontInflation::~AutoMaybeDisableFontInflation()
{
  if (mPresContext) {
    mPresContext->mInflationDisabledForShrinkWrap = mOldValue;
  }
}

namespace mozilla {

Rect NSRectToRect(const nsRect& aRect, double aAppUnitsPerPixel)
{
  // Note that by making aAppUnitsPerPixel a double we're doing floating-point
  // division using a larger type and avoiding rounding error.
  return Rect(Float(aRect.x / aAppUnitsPerPixel),
              Float(aRect.y / aAppUnitsPerPixel),
              Float(aRect.width / aAppUnitsPerPixel),
              Float(aRect.height / aAppUnitsPerPixel));
}

Rect NSRectToSnappedRect(const nsRect& aRect, double aAppUnitsPerPixel,
                         const gfx::DrawTarget& aSnapDT)
{
  // Note that by making aAppUnitsPerPixel a double we're doing floating-point
  // division using a larger type and avoiding rounding error.
  Rect rect(Float(aRect.x / aAppUnitsPerPixel),
            Float(aRect.y / aAppUnitsPerPixel),
            Float(aRect.width / aAppUnitsPerPixel),
            Float(aRect.height / aAppUnitsPerPixel));
  MaybeSnapToDevicePixels(rect, aSnapDT, true);
  return rect;
}
// Similar to a snapped rect, except an axis is left unsnapped if the snapping
// process results in a length of 0.
Rect NSRectToNonEmptySnappedRect(const nsRect& aRect, double aAppUnitsPerPixel,
                                 const gfx::DrawTarget& aSnapDT)
{
  // Note that by making aAppUnitsPerPixel a double we're doing floating-point
  // division using a larger type and avoiding rounding error.
  Rect rect(Float(aRect.x / aAppUnitsPerPixel),
            Float(aRect.y / aAppUnitsPerPixel),
            Float(aRect.width / aAppUnitsPerPixel),
            Float(aRect.height / aAppUnitsPerPixel));
  MaybeSnapToDevicePixels(rect, aSnapDT, true, false);
  return rect;
}

void StrokeLineWithSnapping(const nsPoint& aP1, const nsPoint& aP2,
                            int32_t aAppUnitsPerDevPixel,
                            DrawTarget& aDrawTarget,
                            const Pattern& aPattern,
                            const StrokeOptions& aStrokeOptions,
                            const DrawOptions& aDrawOptions)
{
  Point p1 = NSPointToPoint(aP1, aAppUnitsPerDevPixel);
  Point p2 = NSPointToPoint(aP2, aAppUnitsPerDevPixel);
  SnapLineToDevicePixelsForStroking(p1, p2, aDrawTarget,
                                    aStrokeOptions.mLineWidth);
  aDrawTarget.StrokeLine(p1, p2, aPattern, aStrokeOptions, aDrawOptions);
}

namespace layout {

void
MaybeSetupTransactionIdAllocator(layers::LayerManager* aManager,
                                 nsPresContext* aPresContext)
{
  auto backendType = aManager->GetBackendType();
  if (backendType == LayersBackend::LAYERS_CLIENT ||
      backendType == LayersBackend::LAYERS_WR) {
    aManager->SetTransactionIdAllocator(aPresContext->RefreshDriver());
  }
}

} // namespace layout
} // namespace mozilla

/* static */ bool
nsLayoutUtils::IsOutlineStyleAutoEnabled()
{
  static bool sOutlineStyleAutoEnabled;
  static bool sOutlineStyleAutoPrefCached = false;

  if (!sOutlineStyleAutoPrefCached) {
    sOutlineStyleAutoPrefCached = true;
    Preferences::AddBoolVarCache(&sOutlineStyleAutoEnabled,
                                 "layout.css.outline-style-auto.enabled",
                                 false);
  }
  return sOutlineStyleAutoEnabled;
}

/* static */ void
nsLayoutUtils::SetBSizeFromFontMetrics(const nsIFrame* aFrame,
                                       ReflowOutput& aMetrics,
                                       const LogicalMargin& aFramePadding,
                                       WritingMode aLineWM,
                                       WritingMode aFrameWM)
{
  RefPtr<nsFontMetrics> fm =
    nsLayoutUtils::GetInflatedFontMetricsForFrame(aFrame);

  if (fm) {
    // Compute final height of the frame.
    //
    // Do things the standard css2 way -- though it's hard to find it
    // in the css2 spec! It's actually found in the css1 spec section
    // 4.4 (you will have to read between the lines to really see
    // it).
    //
    // The height of our box is the sum of our font size plus the top
    // and bottom border and padding. The height of children do not
    // affect our height.
    aMetrics.SetBlockStartAscent(aLineWM.IsLineInverted() ? fm->MaxDescent()
                                                          : fm->MaxAscent());
    aMetrics.BSize(aLineWM) = fm->MaxHeight();
  } else {
    NS_WARNING("Cannot get font metrics - defaulting sizes to 0");
    aMetrics.SetBlockStartAscent(aMetrics.BSize(aLineWM) = 0);
  }
  aMetrics.SetBlockStartAscent(aMetrics.BlockStartAscent() +
                               aFramePadding.BStart(aFrameWM));
  aMetrics.BSize(aLineWM) += aFramePadding.BStartEnd(aFrameWM);
}

/* static */ bool
nsLayoutUtils::HasDocumentLevelListenersForApzAwareEvents(nsIPresShell* aShell)
{
  if (nsIDocument* doc = aShell->GetDocument()) {
    WidgetEvent event(true, eVoidEvent);
    nsTArray<EventTarget*> targets;
    nsresult rv = EventDispatcher::Dispatch(doc, nullptr, &event, nullptr,
        nullptr, nullptr, &targets);
    NS_ENSURE_SUCCESS(rv, false);
    for (size_t i = 0; i < targets.Length(); i++) {
      if (targets[i]->IsApzAware()) {
        return true;
      }
    }
  }
  return false;
}

static void
MaybeReflowForInflationScreenSizeChange(nsPresContext *aPresContext)
{
  if (aPresContext) {
    nsIPresShell* presShell = aPresContext->GetPresShell();
    bool fontInflationWasEnabled = presShell->FontSizeInflationEnabled();
    presShell->NotifyFontSizeInflationEnabledIsDirty();
    bool changed = false;
    if (presShell && presShell->FontSizeInflationEnabled() &&
        presShell->FontSizeInflationMinTwips() != 0) {
      aPresContext->ScreenSizeInchesForFontInflation(&changed);
    }

    changed = changed ||
      (fontInflationWasEnabled != presShell->FontSizeInflationEnabled());
    if (changed) {
      nsCOMPtr<nsIDocShell> docShell = aPresContext->GetDocShell();
      if (docShell) {
        nsCOMPtr<nsIContentViewer> cv;
        docShell->GetContentViewer(getter_AddRefs(cv));
        if (cv) {
          nsTArray<nsCOMPtr<nsIContentViewer> > array;
          cv->AppendSubtree(array);
          for (uint32_t i = 0, iEnd = array.Length(); i < iEnd; ++i) {
            nsCOMPtr<nsIPresShell> shell;
            nsCOMPtr<nsIContentViewer> cv = array[i];
            cv->GetPresShell(getter_AddRefs(shell));
            if (shell) {
              nsIFrame *rootFrame = shell->GetRootFrame();
              if (rootFrame) {
                shell->FrameNeedsReflow(rootFrame,
                                        nsIPresShell::eStyleChange,
                                        NS_FRAME_IS_DIRTY);
              }
            }
          }
        }
      }
    }
  }
}

/* static */ void
nsLayoutUtils::SetScrollPositionClampingScrollPortSize(nsIPresShell* aPresShell, CSSSize aSize)
{
  MOZ_ASSERT(aSize.width >= 0.0 && aSize.height >= 0.0);

  aPresShell->SetScrollPositionClampingScrollPortSize(
    nsPresContext::CSSPixelsToAppUnits(aSize.width),
    nsPresContext::CSSPixelsToAppUnits(aSize.height));

  // When the "font.size.inflation.minTwips" preference is set, the
  // layout depends on the size of the screen.  Since when the size
  // of the screen changes, the scroll position clamping scroll port
  // size also changes, we hook in the needed updates here rather
  // than adding a separate notification just for this change.
  nsPresContext* presContext = aPresShell->GetPresContext();
  MaybeReflowForInflationScreenSizeChange(presContext);
}

/* static */ bool
nsLayoutUtils::CanScrollOriginClobberApz(nsIAtom* aScrollOrigin)
{
  return aScrollOrigin != nullptr
      && aScrollOrigin != nsGkAtoms::apz
      && aScrollOrigin != nsGkAtoms::restore;
}

/* static */ ScrollMetadata
nsLayoutUtils::ComputeScrollMetadata(nsIFrame* aForFrame,
                                     nsIFrame* aScrollFrame,
                                     nsIContent* aContent,
                                     const nsIFrame* aReferenceFrame,
                                     Layer* aLayer,
                                     ViewID aScrollParentId,
                                     const nsRect& aViewport,
                                     const Maybe<nsRect>& aClipRect,
                                     bool aIsRootContent,
                                     const ContainerLayerParameters& aContainerParameters)
{
  nsPresContext* presContext = aForFrame->PresContext();
  int32_t auPerDevPixel = presContext->AppUnitsPerDevPixel();

  nsIPresShell* presShell = presContext->GetPresShell();
  ScrollMetadata metadata;
  FrameMetrics& metrics = metadata.GetMetrics();
  metrics.SetViewport(CSSRect::FromAppUnits(aViewport));

  ViewID scrollId = FrameMetrics::NULL_SCROLL_ID;
  if (aContent) {
    if (void* paintRequestTime = aContent->GetProperty(nsGkAtoms::paintRequestTime)) {
      metrics.SetPaintRequestTime(*static_cast<TimeStamp*>(paintRequestTime));
      aContent->DeleteProperty(nsGkAtoms::paintRequestTime);
    }
    scrollId = nsLayoutUtils::FindOrCreateIDFor(aContent);
    nsRect dp;
    if (nsLayoutUtils::GetDisplayPort(aContent, &dp)) {
      metrics.SetDisplayPort(CSSRect::FromAppUnits(dp));
      nsLayoutUtils::LogTestDataForPaint(aLayer->Manager(), scrollId, "displayport",
          metrics.GetDisplayPort());
    }
    if (nsLayoutUtils::GetCriticalDisplayPort(aContent, &dp)) {
      metrics.SetCriticalDisplayPort(CSSRect::FromAppUnits(dp));
      nsLayoutUtils::LogTestDataForPaint(aLayer->Manager(), scrollId,
          "criticalDisplayport", metrics.GetCriticalDisplayPort());
    }
    DisplayPortMarginsPropertyData* marginsData =
        static_cast<DisplayPortMarginsPropertyData*>(aContent->GetProperty(nsGkAtoms::DisplayPortMargins));
    if (marginsData) {
      metrics.SetDisplayPortMargins(marginsData->mMargins);
    }
  }

  nsIScrollableFrame* scrollableFrame = nullptr;
  if (aScrollFrame)
    scrollableFrame = aScrollFrame->GetScrollTargetFrame();

  metrics.SetScrollableRect(CSSRect::FromAppUnits(
    nsLayoutUtils::CalculateScrollableRectForFrame(scrollableFrame, aForFrame)));

  if (scrollableFrame) {
    nsPoint scrollPosition = scrollableFrame->GetScrollPosition();
    metrics.SetScrollOffset(CSSPoint::FromAppUnits(scrollPosition));

    nsPoint smoothScrollPosition = scrollableFrame->LastScrollDestination();
    metrics.SetSmoothScrollOffset(CSSPoint::FromAppUnits(smoothScrollPosition));

    // If the frame was scrolled since the last layers update, and by something
    // that is higher priority than APZ, we want to tell the APZ to update
    // its scroll offset. We want to distinguish the case where the scroll offset
    // was "restored" because in that case the restored scroll position should
    // not overwrite a user-driven scroll.
    if (scrollableFrame->LastScrollOrigin() == nsGkAtoms::restore) {
      metrics.SetScrollOffsetRestored(scrollableFrame->CurrentScrollGeneration());
    } else if (CanScrollOriginClobberApz(scrollableFrame->LastScrollOrigin())) {
      metrics.SetScrollOffsetUpdated(scrollableFrame->CurrentScrollGeneration());
    }
    scrollableFrame->AllowScrollOriginDowngrade();

    nsIAtom* lastSmoothScrollOrigin = scrollableFrame->LastSmoothScrollOrigin();
    if (lastSmoothScrollOrigin) {
      metrics.SetSmoothScrollOffsetUpdated(scrollableFrame->CurrentScrollGeneration());
    }

    nsSize lineScrollAmount = scrollableFrame->GetLineScrollAmount();
    LayoutDeviceIntSize lineScrollAmountInDevPixels =
      LayoutDeviceIntSize::FromAppUnitsRounded(lineScrollAmount, presContext->AppUnitsPerDevPixel());
    metadata.SetLineScrollAmount(lineScrollAmountInDevPixels);

    nsSize pageScrollAmount = scrollableFrame->GetPageScrollAmount();
    LayoutDeviceIntSize pageScrollAmountInDevPixels =
      LayoutDeviceIntSize::FromAppUnitsRounded(pageScrollAmount, presContext->AppUnitsPerDevPixel());
    metadata.SetPageScrollAmount(pageScrollAmountInDevPixels);

    if (!aScrollFrame->GetParent() ||
        EventStateManager::CanVerticallyScrollFrameWithWheel(aScrollFrame->GetParent()))
    {
      metadata.SetAllowVerticalScrollWithWheel(true);
    }

    metadata.SetUsesContainerScrolling(scrollableFrame->UsesContainerScrolling());

    metadata.SetSnapInfo(scrollableFrame->GetScrollSnapInfo());
  }

  // If we have the scrollparent being the same as the scroll id, the
  // compositor-side code could get into an infinite loop while building the
  // overscroll handoff chain.
  MOZ_ASSERT(aScrollParentId == FrameMetrics::NULL_SCROLL_ID || scrollId != aScrollParentId);
  metrics.SetScrollId(scrollId);
  metrics.SetIsRootContent(aIsRootContent);
  metadata.SetScrollParentId(aScrollParentId);

  if (scrollId != FrameMetrics::NULL_SCROLL_ID && !presContext->GetParentPresContext()) {
    if ((aScrollFrame && (aScrollFrame == presShell->GetRootScrollFrame())) ||
        aContent == presShell->GetDocument()->GetDocumentElement()) {
      metadata.SetIsLayersIdRoot(true);
    }
  }

  // Only the root scrollable frame for a given presShell should pick up
  // the presShell's resolution. All the other frames are 1.0.
  if (aScrollFrame == presShell->GetRootScrollFrame()) {
    metrics.SetPresShellResolution(presShell->GetResolution());
  } else {
    metrics.SetPresShellResolution(1.0f);
  }
  // The cumulative resolution is the resolution at which the scroll frame's
  // content is actually rendered. It includes the pres shell resolutions of
  // all the pres shells from here up to the root, as well as any css-driven
  // resolution. We don't need to compute it as it's already stored in the
  // container parameters.
  metrics.SetCumulativeResolution(aContainerParameters.Scale());

  LayoutDeviceToScreenScale2D resolutionToScreen(
      presShell->GetCumulativeResolution()
    * nsLayoutUtils::GetTransformToAncestorScale(aScrollFrame ? aScrollFrame : aForFrame));
  metrics.SetExtraResolution(metrics.GetCumulativeResolution() / resolutionToScreen);

  metrics.SetDevPixelsPerCSSPixel(presContext->CSSToDevPixelScale());

  // Initially, AsyncPanZoomController should render the content to the screen
  // at the painted resolution.
  const LayerToParentLayerScale layerToParentLayerScale(1.0f);
  metrics.SetZoom(metrics.GetCumulativeResolution() * metrics.GetDevPixelsPerCSSPixel()
                  * layerToParentLayerScale);

  // Calculate the composition bounds as the size of the scroll frame and
  // its origin relative to the reference frame.
  // If aScrollFrame is null, we are in a document without a root scroll frame,
  // so it's a xul document. In this case, use the size of the viewport frame.
  nsIFrame* frameForCompositionBoundsCalculation = aScrollFrame ? aScrollFrame : aForFrame;
  nsRect compositionBounds(frameForCompositionBoundsCalculation->GetOffsetToCrossDoc(aReferenceFrame),
                           frameForCompositionBoundsCalculation->GetSize());
  if (scrollableFrame) {
    // If we have a scrollable frame, restrict the composition bounds to its
    // scroll port. The scroll port excludes the frame borders and the scroll
    // bars, which we don't want to be part of the composition bounds.
    nsRect scrollPort = scrollableFrame->GetScrollPortRect();
    compositionBounds = nsRect(compositionBounds.TopLeft() + scrollPort.TopLeft(),
                               scrollPort.Size());
  }
  ParentLayerRect frameBounds = LayoutDeviceRect::FromAppUnits(compositionBounds, auPerDevPixel)
                              * metrics.GetCumulativeResolution()
                              * layerToParentLayerScale;

  if (aClipRect) {
    ParentLayerRect rect = LayoutDeviceRect::FromAppUnits(*aClipRect, auPerDevPixel)
                         * metrics.GetCumulativeResolution()
                         * layerToParentLayerScale;
    metadata.SetScrollClip(Some(LayerClip(RoundedToInt(rect))));
  }

  // For the root scroll frame of the root content document (RCD-RSF), the above calculation
  // will yield the size of the viewport frame as the composition bounds, which
  // doesn't actually correspond to what is visible when
  // nsIDOMWindowUtils::setCSSViewport has been called to modify the visible area of
  // the prescontext that the viewport frame is reflowed into. In that case if our
  // document has a widget then the widget's bounds will correspond to what is
  // visible. If we don't have a widget the root view's bounds correspond to what
  // would be visible because they don't get modified by setCSSViewport.
  bool isRootScrollFrame = aScrollFrame == presShell->GetRootScrollFrame();
  bool isRootContentDocRootScrollFrame = isRootScrollFrame
                                      && presContext->IsRootContentDocument();
  if (isRootContentDocRootScrollFrame) {
    UpdateCompositionBoundsForRCDRSF(frameBounds, presContext, true);
  }

  nsMargin sizes = ScrollbarAreaToExcludeFromCompositionBoundsFor(aScrollFrame);
  // Scrollbars are not subject to resolution scaling, so LD pixels = layer pixels for them.
  ParentLayerMargin boundMargins = LayoutDeviceMargin::FromAppUnits(sizes, auPerDevPixel)
    * LayoutDeviceToParentLayerScale(1.0f);
  frameBounds.Deflate(boundMargins);

  metrics.SetCompositionBounds(frameBounds);

  metrics.SetRootCompositionSize(
    nsLayoutUtils::CalculateRootCompositionSize(aScrollFrame ? aScrollFrame : aForFrame,
                                                isRootContentDocRootScrollFrame, metrics));

  if (gfxPrefs::APZPrintTree() || gfxPrefs::APZTestLoggingEnabled()) {
    if (nsIContent* content = frameForCompositionBoundsCalculation->GetContent()) {
      nsAutoString contentDescription;
      content->Describe(contentDescription);
      metadata.SetContentDescription(NS_LossyConvertUTF16toASCII(contentDescription));
      nsLayoutUtils::LogTestDataForPaint(aLayer->Manager(), scrollId, "contentDescription",
          metadata.GetContentDescription().get());
    }
  }

  metrics.SetPresShellId(presShell->GetPresShellId());

  // If the scroll frame's content is marked 'scrollgrab', record this
  // in the FrameMetrics so APZ knows to provide the scroll grabbing
  // behaviour.
  if (aScrollFrame && nsContentUtils::HasScrollgrab(aScrollFrame->GetContent())) {
    metadata.SetHasScrollgrab(true);
  }

  // Also compute and set the background color.
  // This is needed for APZ overscrolling support.
  if (aScrollFrame) {
    if (isRootScrollFrame) {
      metadata.SetBackgroundColor(Color::FromABGR(
        presShell->GetCanvasBackground()));
    } else {
      nsStyleContext* backgroundStyle;
      if (nsCSSRendering::FindBackground(aScrollFrame, &backgroundStyle)) {
        nscolor backgroundColor = backgroundStyle->
          StyleBackground()->BackgroundColor(backgroundStyle);
        metadata.SetBackgroundColor(Color::FromABGR(backgroundColor));
      }
    }
  }

  if (ShouldDisableApzForElement(aContent)) {
    metadata.SetForceDisableApz(true);
  }

  return metadata;
}

/* static */ bool
nsLayoutUtils::ContainsMetricsWithId(const Layer* aLayer, const ViewID& aScrollId)
{
  for (uint32_t i = aLayer->GetScrollMetadataCount(); i > 0; i--) {
    if (aLayer->GetFrameMetrics(i-1).GetScrollId() == aScrollId) {
      return true;
    }
  }
  for (Layer* child = aLayer->GetFirstChild(); child; child = child->GetNextSibling()) {
    if (ContainsMetricsWithId(child, aScrollId)) {
      return true;
    }
  }
  return false;
}

/* static */ uint32_t
nsLayoutUtils::GetTouchActionFromFrame(nsIFrame* aFrame)
{
  // If aFrame is null then return default value
  if (!aFrame) {
    return NS_STYLE_TOUCH_ACTION_AUTO;
  }

  // The touch-action CSS property applies to: all elements except:
  // non-replaced inline elements, table rows, row groups, table columns, and column groups
  bool isNonReplacedInlineElement = aFrame->IsFrameOfType(nsIFrame::eLineParticipant);
  if (isNonReplacedInlineElement) {
    return NS_STYLE_TOUCH_ACTION_AUTO;
  }

  const nsStyleDisplay* disp = aFrame->StyleDisplay();
  bool isTableElement = disp->IsInnerTableStyle() &&
    disp->mDisplay != StyleDisplay::TableCell &&
    disp->mDisplay != StyleDisplay::TableCaption;
  if (isTableElement) {
    return NS_STYLE_TOUCH_ACTION_AUTO;
  }

  return disp->mTouchAction;
}

/* static */  void
nsLayoutUtils::TransformToAncestorAndCombineRegions(
  const nsRegion& aRegion,
  nsIFrame* aFrame,
  const nsIFrame* aAncestorFrame,
  nsRegion* aPreciseTargetDest,
  nsRegion* aImpreciseTargetDest,
  Maybe<Matrix4x4>* aMatrixCache,
  const DisplayItemClip* aClip)
{
  if (aRegion.IsEmpty()) {
    return;
  }
  bool isPrecise;
  RegionBuilder<nsRegion> transformedRegion;
  for (nsRegion::RectIterator it = aRegion.RectIter(); !it.Done(); it.Next()) {
    nsRect transformed = TransformFrameRectToAncestor(
      aFrame, it.Get(), aAncestorFrame, &isPrecise, aMatrixCache);
    if (aClip) {
      transformed = aClip->ApplyNonRoundedIntersection(transformed);
      if (aClip->GetRoundedRectCount() > 0) {
        isPrecise = false;
      }
    }
    transformedRegion.OrWith(transformed);
  }
  nsRegion* dest = isPrecise ? aPreciseTargetDest : aImpreciseTargetDest;
  dest->OrWith(transformedRegion.ToRegion());
}

/* static */ bool
nsLayoutUtils::ShouldUseNoScriptSheet(nsIDocument* aDocument)
{
  // also handle the case where print is done from print preview
  // see bug #342439 for more details
  if (aDocument->IsStaticDocument()) {
    aDocument = aDocument->GetOriginalDocument();
  }
  return aDocument->IsScriptEnabled();
}

/* static */ bool
nsLayoutUtils::ShouldUseNoFramesSheet(nsIDocument* aDocument)
{
  bool allowSubframes = true;
  nsIDocShell* docShell = aDocument->GetDocShell();
  if (docShell) {
    docShell->GetAllowSubframes(&allowSubframes);
  }
  return !allowSubframes;
}

/* static */ void
nsLayoutUtils::GetFrameTextContent(nsIFrame* aFrame, nsAString& aResult)
{
  aResult.Truncate();
  AppendFrameTextContent(aFrame, aResult);
}

/* static */ void
nsLayoutUtils::AppendFrameTextContent(nsIFrame* aFrame, nsAString& aResult)
{
  if (aFrame->IsTextFrame()) {
    auto textFrame = static_cast<nsTextFrame*>(aFrame);
    auto offset = textFrame->GetContentOffset();
    auto length = textFrame->GetContentLength();
    textFrame->GetContent()->
      GetText()->AppendTo(aResult, offset, length);
  } else {
    for (nsIFrame* child : aFrame->PrincipalChildList()) {
      AppendFrameTextContent(child, aResult);
    }
  }
}

/* static */
nsRect
nsLayoutUtils::GetSelectionBoundingRect(Selection* aSel)
{
  nsRect res;
  // Bounding client rect may be empty after calling GetBoundingClientRect
  // when range is collapsed. So we get caret's rect when range is
  // collapsed.
  if (aSel->IsCollapsed()) {
    nsIFrame* frame = nsCaret::GetGeometry(aSel, &res);
    if (frame) {
      nsIFrame* relativeTo = GetContainingBlockForClientRect(frame);
      res = TransformFrameRectToAncestor(frame, res, relativeTo);
    }
  } else {
    int32_t rangeCount = aSel->RangeCount();
    RectAccumulator accumulator;
    for (int32_t idx = 0; idx < rangeCount; ++idx) {
      nsRange* range = aSel->GetRangeAt(idx);
      nsRange::CollectClientRectsAndText(&accumulator, nullptr, range,
                                  range->GetStartParent(), range->StartOffset(),
                                  range->GetEndParent(), range->EndOffset(),
                                  true, false);
    }
    res = accumulator.mResultRect.IsEmpty() ? accumulator.mFirstRect :
      accumulator.mResultRect;
  }

  return res;
}

/* static */ nsBlockFrame*
nsLayoutUtils::GetFloatContainingBlock(nsIFrame* aFrame)
{
  nsIFrame* ancestor = aFrame->GetParent();
  while (ancestor && !ancestor->IsFloatContainingBlock()) {
    ancestor = ancestor->GetParent();
  }
  MOZ_ASSERT(!ancestor || GetAsBlock(ancestor),
             "Float containing block can only be block frame");
  return static_cast<nsBlockFrame*>(ancestor);
}

// The implementation of this calculation is adapted from
// Element::GetBoundingClientRect().
/* static */ CSSRect
nsLayoutUtils::GetBoundingContentRect(const nsIContent* aContent,
                                      const nsIScrollableFrame* aRootScrollFrame) {
  CSSRect result;
  if (nsIFrame* frame = aContent->GetPrimaryFrame()) {
    nsIFrame* relativeTo = aRootScrollFrame->GetScrolledFrame();
    result = CSSRect::FromAppUnits(
        nsLayoutUtils::GetAllInFlowRectsUnion(
            frame,
            relativeTo,
            nsLayoutUtils::RECTS_ACCOUNT_FOR_TRANSFORMS));

    // If the element is contained in a scrollable frame that is not
    // the root scroll frame, make sure to clip the result so that it is
    // not larger than the containing scrollable frame's bounds.
    nsIScrollableFrame* scrollFrame = nsLayoutUtils::GetNearestScrollableFrame(frame);
    if (scrollFrame && scrollFrame != aRootScrollFrame) {
      nsIFrame* subFrame = do_QueryFrame(scrollFrame);
      MOZ_ASSERT(subFrame);
      // Get the bounds of the scroll frame in the same coordinate space
      // as |result|.
      CSSRect subFrameRect = CSSRect::FromAppUnits(
          nsLayoutUtils::TransformFrameRectToAncestor(
              subFrame,
              subFrame->GetRectRelativeToSelf(),
              relativeTo));

      result = subFrameRect.Intersect(result);
    }
  }
  return result;
}

static already_AddRefed<nsIPresShell>
GetPresShell(const nsIContent* aContent)
{
  nsCOMPtr<nsIPresShell> result;
  if (nsIDocument* doc = aContent->GetComposedDoc()) {
    result = doc->GetShell();
  }
  return result.forget();
}

static void UpdateDisplayPortMarginsForPendingMetrics(FrameMetrics& aMetrics) {
  nsIContent* content = nsLayoutUtils::FindContentFor(aMetrics.GetScrollId());
  if (!content) {
    return;
  }

  nsCOMPtr<nsIPresShell> shell = GetPresShell(content);
  if (!shell) {
    return;
  }

  MOZ_ASSERT(aMetrics.GetUseDisplayPortMargins());

  if (gfxPrefs::APZAllowZooming() && aMetrics.IsRootContent()) {
    // See APZCCallbackHelper::UpdateRootFrame for details.
    float presShellResolution = shell->GetResolution();
    if (presShellResolution != aMetrics.GetPresShellResolution()) {
      return;
    }
  }

  nsIScrollableFrame* frame = nsLayoutUtils::FindScrollableFrameFor(aMetrics.GetScrollId());

  if (!frame) {
    return;
  }

  if (APZCCallbackHelper::IsScrollInProgress(frame)) {
    // If these conditions are true, then the UpdateFrame
    // message may be ignored by the main-thread, so we
    // shouldn't update the displayport based on it.
    return;
  }

  DisplayPortMarginsPropertyData* currentData =
    static_cast<DisplayPortMarginsPropertyData*>(content->GetProperty(nsGkAtoms::DisplayPortMargins));
  if (!currentData) {
    return;
  }

  CSSPoint frameScrollOffset = CSSPoint::FromAppUnits(frame->GetScrollPosition());
  APZCCallbackHelper::AdjustDisplayPortForScrollDelta(aMetrics, frameScrollOffset);

  nsLayoutUtils::SetDisplayPortMargins(content, shell,
                                       aMetrics.GetDisplayPortMargins(), 0);
}

/* static */ void
nsLayoutUtils::UpdateDisplayPortMarginsFromPendingMessages()
{
  if (mozilla::dom::ContentChild::GetSingleton() &&
      mozilla::dom::ContentChild::GetSingleton()->GetIPCChannel()) {
    CompositorBridgeChild::Get()->GetIPCChannel()->PeekMessages(
      [](const IPC::Message& aMsg) -> bool {
        if (aMsg.type() == mozilla::layers::PAPZ::Msg_RequestContentRepaint__ID) {
          PickleIterator iter(aMsg);
          FrameMetrics frame;
          if (!IPC::ReadParam(&aMsg, &iter, &frame)) {
            MOZ_ASSERT(false);
            return true;
          }

          UpdateDisplayPortMarginsForPendingMetrics(frame);
        }
        return true;
      });
  }
}

/* static */ bool
nsLayoutUtils::IsTransformed(nsIFrame* aForFrame, nsIFrame* aTopFrame)
{
  for (nsIFrame* f = aForFrame; f != aTopFrame; f = f->GetParent()) {
    if (f->IsTransformed()) {
      return true;
    }
  }
  return false;
}

/*static*/ CSSPoint
nsLayoutUtils::GetCumulativeApzCallbackTransform(nsIFrame* aFrame)
{
  CSSPoint delta;
  if (!aFrame) {
    return delta;
  }
  nsIFrame* frame = aFrame;
  nsCOMPtr<nsIContent> content = frame->GetContent();
  nsCOMPtr<nsIContent> lastContent;
  while (frame) {
    if (content && (content != lastContent)) {
      void* property = content->GetProperty(nsGkAtoms::apzCallbackTransform);
      if (property) {
        delta += *static_cast<CSSPoint*>(property);
      }
    }
    frame = GetCrossDocParentFrame(frame);
    lastContent = content;
    content = frame ? frame->GetContent() : nullptr;
  }
  return delta;
}

/* static */ nsRect
nsLayoutUtils::ComputePartialPrerenderArea(const nsRect& aDirtyRect,
                                           const nsRect& aOverflow,
                                           const nsSize& aPrerenderSize)
{
  // Simple calculation for now: center the pre-render area on the dirty rect,
  // and clamp to the overflow area. Later we can do more advanced things like
  // redistributing from one axis to another, or from one side to another.
  nscoord xExcess = aPrerenderSize.width - aDirtyRect.width;
  nscoord yExcess = aPrerenderSize.height - aDirtyRect.height;
  nsRect result = aDirtyRect;
  result.Inflate(xExcess / 2, yExcess / 2);
  return result.MoveInsideAndClamp(aOverflow);
}

static
bool
LineHasNonEmptyContentWorker(nsIFrame* aFrame)
{
  // Look for non-empty frames, but ignore inline and br frames.
  // For inline frames, descend into the children, if any.
  if (aFrame->IsInlineFrame()) {
    for (nsIFrame* child : aFrame->PrincipalChildList()) {
      if (LineHasNonEmptyContentWorker(child)) {
        return true;
      }
    }
  } else {
    if (!aFrame->IsBrFrame() && !aFrame->IsEmpty()) {
      return true;
    }
  }
  return false;
}

static
bool
LineHasNonEmptyContent(nsLineBox* aLine)
{
  int32_t count = aLine->GetChildCount();
  for (nsIFrame* frame = aLine->mFirstChild; count > 0;
       --count, frame = frame->GetNextSibling()) {
    if (LineHasNonEmptyContentWorker(frame)) {
      return true;
    }
  }
  return false;
}

/* static */ bool
nsLayoutUtils::IsInvisibleBreak(nsINode* aNode, nsIFrame** aNextLineFrame)
{
  if (aNextLineFrame) {
    *aNextLineFrame = nullptr;
  }

  if (!aNode->IsElement() || !aNode->IsEditable()) {
    return false;
  }
  nsIFrame* frame = aNode->AsElement()->GetPrimaryFrame();
  if (!frame || !frame->IsBrFrame()) {
    return false;
  }

  nsContainerFrame* f = frame->GetParent();
  while (f && f->IsFrameOfType(nsBox::eLineParticipant)) {
    f = f->GetParent();
  }
  nsBlockFrame* blockAncestor = do_QueryFrame(f);
  if (!blockAncestor) {
    // The container frame doesn't support line breaking.
    return false;
  }

  bool valid = false;
  nsBlockInFlowLineIterator iter(blockAncestor, frame, &valid);
  if (!valid) {
    return false;
  }

  bool lineNonEmpty = LineHasNonEmptyContent(iter.GetLine());
  if (!lineNonEmpty) {
    return false;
  }

  while (iter.Next()) {
    auto currentLine = iter.GetLine();
    // Completely skip empty lines.
    if (!currentLine->IsEmpty()) {
      // If we come across an inline line, the BR has caused a visible line break.
      if (currentLine->IsInline()) {
        if (aNextLineFrame) {
          *aNextLineFrame = currentLine->mFirstChild;
        }
        return false;
      }
      break;
    }
  }

  return lineNonEmpty;
}

static nsRect
ComputeSVGReferenceRect(nsIFrame* aFrame,
                        StyleGeometryBox aGeometryBox)
{
  MOZ_ASSERT(aFrame->GetContent()->IsSVGElement());
  nsRect r;

  // For SVG elements without associated CSS layout box, the used value for
  // content-box, padding-box, border-box and margin-box is fill-box.
  switch (aGeometryBox) {
    case StyleGeometryBox::StrokeBox: {
      // XXX Bug 1299876
      // The size of srtoke-box is not correct if this graphic element has
      // specific stroke-linejoin or stroke-linecap.
      gfxRect bbox = nsSVGUtils::GetBBox(aFrame,
                nsSVGUtils::eBBoxIncludeFill | nsSVGUtils::eBBoxIncludeStroke);
      r = nsLayoutUtils::RoundGfxRectToAppRect(bbox,
                                         nsPresContext::AppUnitsPerCSSPixel());
      break;
    }
    case StyleGeometryBox::ViewBox: {
      nsIContent* content = aFrame->GetContent();
      nsSVGElement* element = static_cast<nsSVGElement*>(content);
      SVGSVGElement* svgElement = element->GetCtx();
      MOZ_ASSERT(svgElement);

      if (svgElement && svgElement->HasViewBoxRect()) {
        // If a ‘viewBox‘ attribute is specified for the SVG viewport creating
        // element:
        // 1. The reference box is positioned at the origin of the coordinate
        //    system established by the ‘viewBox‘ attribute.
        // 2. The dimension of the reference box is set to the width and height
        //    values of the ‘viewBox‘ attribute.
        nsSVGViewBox* viewBox = svgElement->GetViewBox();
        const nsSVGViewBoxRect& value = viewBox->GetAnimValue();
        r = nsRect(nsPresContext::CSSPixelsToAppUnits(value.x),
                   nsPresContext::CSSPixelsToAppUnits(value.y),
                   nsPresContext::CSSPixelsToAppUnits(value.width),
                   nsPresContext::CSSPixelsToAppUnits(value.height));
      } else {
        // No viewBox is specified, uses the nearest SVG viewport as reference
        // box.
        svgFloatSize viewportSize = svgElement->GetViewportSize();
        r = nsRect(0, 0,
                   nsPresContext::CSSPixelsToAppUnits(viewportSize.width),
                   nsPresContext::CSSPixelsToAppUnits(viewportSize.height));
      }

      break;
    }
    case StyleGeometryBox::NoBox:
    case StyleGeometryBox::BorderBox:
    case StyleGeometryBox::ContentBox:
    case StyleGeometryBox::PaddingBox:
    case StyleGeometryBox::MarginBox:
    case StyleGeometryBox::FillBox: {
      gfxRect bbox = nsSVGUtils::GetBBox(aFrame,
                                         nsSVGUtils::eBBoxIncludeFill);
      r = nsLayoutUtils::RoundGfxRectToAppRect(bbox,
                                         nsPresContext::AppUnitsPerCSSPixel());
      break;
    }
    default:{
      MOZ_ASSERT_UNREACHABLE("unknown StyleGeometryBox type");
      gfxRect bbox = nsSVGUtils::GetBBox(aFrame,
                                         nsSVGUtils::eBBoxIncludeFill);
      r = nsLayoutUtils::RoundGfxRectToAppRect(bbox,
                                         nsPresContext::AppUnitsPerCSSPixel());
      break;
    }
  }

  return r;
}

static nsRect
ComputeHTMLReferenceRect(nsIFrame* aFrame,
                         StyleGeometryBox aGeometryBox)
{
  nsRect r;

  // For elements with associated CSS layout box, the used value for fill-box,
  // stroke-box and view-box is border-box.
  switch (aGeometryBox) {
    case StyleGeometryBox::ContentBox:
      r = aFrame->GetContentRectRelativeToSelf();
      break;
    case StyleGeometryBox::PaddingBox:
      r = aFrame->GetPaddingRectRelativeToSelf();
      break;
    case StyleGeometryBox::MarginBox:
      r = aFrame->GetMarginRectRelativeToSelf();
      break;
    case StyleGeometryBox::NoBox:
    case StyleGeometryBox::BorderBox:
    case StyleGeometryBox::FillBox:
    case StyleGeometryBox::StrokeBox:
    case StyleGeometryBox::ViewBox:
      r = aFrame->GetRectRelativeToSelf();
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("unknown StyleGeometryBox type");
      r = aFrame->GetRectRelativeToSelf();
      break;
  }

  return r;
}

/* static */ nsRect
nsLayoutUtils::ComputeGeometryBox(nsIFrame* aFrame,
                                  StyleGeometryBox aGeometryBox)
{
  // We use ComputeSVGReferenceRect for all SVG elements, except <svg>
  // element, which does have an associated CSS layout box. In this case we
  // should still use ComputeHTMLReferenceRect for region computing.
  nsRect r = (aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT)
             ? ComputeSVGReferenceRect(aFrame, aGeometryBox)
             : ComputeHTMLReferenceRect(aFrame, aGeometryBox);

  return r;
}
