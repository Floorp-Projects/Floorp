/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLayoutUtils.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/Likely.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "nsCharTraits.h"
#include "nsFontMetrics.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsFrameList.h"
#include "nsGkAtoms.h"
#include "nsHtml5Atoms.h"
#include "nsIAtom.h"
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
#include "nsRenderingContext.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsCSSRendering.h"
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
#include "mozilla/dom/HTMLVideoElement.h"
#include "mozilla/dom/HTMLImageElement.h"
#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/KeyframeEffect.h"
#include "imgIRequest.h"
#include "nsIImageLoadingContent.h"
#include "nsCOMPtr.h"
#include "nsCSSProps.h"
#include "nsListControlFrame.h"
#include "mozilla/dom/Element.h"
#include "nsCanvasFrame.h"
#include "gfxDrawable.h"
#include "gfxUtils.h"
#include "nsDataHashtable.h"
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
#include "mozilla/Telemetry.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/RuleNodeCacheConditions.h"

#ifdef MOZ_XUL
#include "nsXULPopupManager.h"
#endif

#include "GeckoProfiler.h"
#include "nsAnimationManager.h"
#include "nsTransitionManager.h"
#include "RestyleManager.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::image;
using namespace mozilla::layers;
using namespace mozilla::layout;
using namespace mozilla::gfx;

#define GRID_ENABLED_PREF_NAME "layout.css.grid.enabled"
#define RUBY_ENABLED_PREF_NAME "layout.css.ruby.enabled"
#define STICKY_ENABLED_PREF_NAME "layout.css.sticky.enabled"
#define DISPLAY_CONTENTS_ENABLED_PREF_NAME "layout.css.display-contents.enabled"
#define TEXT_ALIGN_TRUE_ENABLED_PREF_NAME "layout.css.text-align-true-value.enabled"

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
/* static */ bool nsLayoutUtils::sInvalidationDebuggingIsEnabled;
/* static */ bool nsLayoutUtils::sCSSVariablesEnabled;
/* static */ bool nsLayoutUtils::sInterruptibleReflowEnabled;
/* static */ bool nsLayoutUtils::sSVGTransformOriginEnabled;

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
    nsCSSProps::kDisplayKTable[sIndexOfGridInDisplayTable] =
      isGridEnabled ? eCSSKeyword_grid : eCSSKeyword_UNKNOWN;
  }
  if (sIndexOfInlineGridInDisplayTable >= 0) {
    nsCSSProps::kDisplayKTable[sIndexOfInlineGridInDisplayTable] =
      isGridEnabled ? eCSSKeyword_inline_grid : eCSSKeyword_UNKNOWN;
  }
}

static void
RubyEnabledPrefChangeCallback(const char* aPrefName, void* aClosure)
{
  MOZ_ASSERT(strncmp(aPrefName, RUBY_ENABLED_PREF_NAME,
                     ArrayLength(RUBY_ENABLED_PREF_NAME)) == 0,
             "We only registered this callback for a single pref, so it "
             "should only be called for that pref");

  static int32_t sIndexOfRubyInDisplayTable;
  static int32_t sIndexOfRubyBaseInDisplayTable;
  static int32_t sIndexOfRubyBaseContainerInDisplayTable;
  static int32_t sIndexOfRubyTextInDisplayTable;
  static int32_t sIndexOfRubyTextContainerInDisplayTable;
  static bool sAreRubyKeywordIndicesInitialized; // initialized to false

  bool isRubyEnabled =
    Preferences::GetBool(RUBY_ENABLED_PREF_NAME, false);
  if (!sAreRubyKeywordIndicesInitialized) {
    // First run: find the position of the ruby display values in
    // kDisplayKTable.
    sIndexOfRubyInDisplayTable =
      nsCSSProps::FindIndexOfKeyword(eCSSKeyword_ruby,
                                     nsCSSProps::kDisplayKTable);
    MOZ_ASSERT(sIndexOfRubyInDisplayTable >= 0,
               "Couldn't find ruby in kDisplayKTable");
    sIndexOfRubyBaseInDisplayTable =
      nsCSSProps::FindIndexOfKeyword(eCSSKeyword_ruby_base,
                                     nsCSSProps::kDisplayKTable);
    MOZ_ASSERT(sIndexOfRubyBaseInDisplayTable >= 0,
               "Couldn't find ruby-base in kDisplayKTable");
    sIndexOfRubyBaseContainerInDisplayTable =
      nsCSSProps::FindIndexOfKeyword(eCSSKeyword_ruby_base_container,
                                     nsCSSProps::kDisplayKTable);
    MOZ_ASSERT(sIndexOfRubyBaseContainerInDisplayTable >= 0,
               "Couldn't find ruby-base-container in kDisplayKTable");
    sIndexOfRubyTextInDisplayTable =
      nsCSSProps::FindIndexOfKeyword(eCSSKeyword_ruby_text,
                                     nsCSSProps::kDisplayKTable);
    MOZ_ASSERT(sIndexOfRubyTextInDisplayTable >= 0,
               "Couldn't find ruby-text in kDisplayKTable");
    sIndexOfRubyTextContainerInDisplayTable =
      nsCSSProps::FindIndexOfKeyword(eCSSKeyword_ruby_text_container,
                                     nsCSSProps::kDisplayKTable);
    MOZ_ASSERT(sIndexOfRubyTextContainerInDisplayTable >= 0,
               "Couldn't find ruby-text-container in kDisplayKTable");
    sAreRubyKeywordIndicesInitialized = true;
  }

  // OK -- now, stomp on or restore the "ruby" entries in kDisplayKTable,
  // depending on whether the ruby pref is enabled vs. disabled.
  if (sIndexOfRubyInDisplayTable >= 0) {
    nsCSSProps::kDisplayKTable[sIndexOfRubyInDisplayTable] =
      isRubyEnabled ? eCSSKeyword_ruby : eCSSKeyword_UNKNOWN;
  }
  if (sIndexOfRubyBaseInDisplayTable >= 0) {
    nsCSSProps::kDisplayKTable[sIndexOfRubyBaseInDisplayTable] =
      isRubyEnabled ? eCSSKeyword_ruby_base : eCSSKeyword_UNKNOWN;
  }
  if (sIndexOfRubyBaseContainerInDisplayTable >= 0) {
    nsCSSProps::kDisplayKTable[sIndexOfRubyBaseContainerInDisplayTable] =
      isRubyEnabled ? eCSSKeyword_ruby_base_container : eCSSKeyword_UNKNOWN;
  }
  if (sIndexOfRubyTextInDisplayTable >= 0) {
    nsCSSProps::kDisplayKTable[sIndexOfRubyTextInDisplayTable] =
      isRubyEnabled ? eCSSKeyword_ruby_text : eCSSKeyword_UNKNOWN;
  }
  if (sIndexOfRubyTextContainerInDisplayTable >= 0) {
    nsCSSProps::kDisplayKTable[sIndexOfRubyTextContainerInDisplayTable] =
      isRubyEnabled ? eCSSKeyword_ruby_text_container : eCSSKeyword_UNKNOWN;
  }
}

// When the pref "layout.css.sticky.enabled" changes, this function is invoked
// to let us update kPositionKTable, to selectively disable or restore the
// entry for "sticky" in that table.
static void
StickyEnabledPrefChangeCallback(const char* aPrefName, void* aClosure)
{
  MOZ_ASSERT(strncmp(aPrefName, STICKY_ENABLED_PREF_NAME,
                     ArrayLength(STICKY_ENABLED_PREF_NAME)) == 0,
             "We only registered this callback for a single pref, so it "
             "should only be called for that pref");

  static int32_t sIndexOfStickyInPositionTable;
  static bool sIsStickyKeywordIndexInitialized; // initialized to false

  bool isStickyEnabled =
    Preferences::GetBool(STICKY_ENABLED_PREF_NAME, false);

  if (!sIsStickyKeywordIndexInitialized) {
    // First run: find the position of "sticky" in kPositionKTable.
    sIndexOfStickyInPositionTable =
      nsCSSProps::FindIndexOfKeyword(eCSSKeyword_sticky,
                                     nsCSSProps::kPositionKTable);
    MOZ_ASSERT(sIndexOfStickyInPositionTable >= 0,
               "Couldn't find sticky in kPositionKTable");
    sIsStickyKeywordIndexInitialized = true;
  }

  // OK -- now, stomp on or restore the "sticky" entry in kPositionKTable,
  // depending on whether the sticky pref is enabled vs. disabled.
  nsCSSProps::kPositionKTable[sIndexOfStickyInPositionTable] =
    isStickyEnabled ? eCSSKeyword_sticky : eCSSKeyword_UNKNOWN;
}

// When the pref "layout.css.display-contents.enabled" changes, this function is
// invoked to let us update kDisplayKTable, to selectively disable or restore
// the entries for "contents" in that table.
static void
DisplayContentsEnabledPrefChangeCallback(const char* aPrefName, void* aClosure)
{
  NS_ASSERTION(strcmp(aPrefName, DISPLAY_CONTENTS_ENABLED_PREF_NAME) == 0,
               "Did you misspell " DISPLAY_CONTENTS_ENABLED_PREF_NAME " ?");

  static bool sIsDisplayContentsKeywordIndexInitialized;
  static int32_t sIndexOfContentsInDisplayTable;
  bool isDisplayContentsEnabled =
    Preferences::GetBool(DISPLAY_CONTENTS_ENABLED_PREF_NAME, false);

  if (!sIsDisplayContentsKeywordIndexInitialized) {
    // First run: find the position of "contents" in kDisplayKTable.
    sIndexOfContentsInDisplayTable =
      nsCSSProps::FindIndexOfKeyword(eCSSKeyword_contents,
                                     nsCSSProps::kDisplayKTable);
    sIsDisplayContentsKeywordIndexInitialized = true;
  }

  // OK -- now, stomp on or restore the "contents" entry in kDisplayKTable,
  // depending on whether the pref is enabled vs. disabled.
  if (sIndexOfContentsInDisplayTable >= 0) {
    nsCSSProps::kDisplayKTable[sIndexOfContentsInDisplayTable] =
      isDisplayContentsEnabled ? eCSSKeyword_contents : eCSSKeyword_UNKNOWN;
  }
}

// When the pref "layout.css.text-align-true-value.enabled" changes, this
// function is called to let us update kTextAlignKTable & kTextAlignLastKTable,
// to selectively disable or restore the entries for "true" in those tables.
static void
TextAlignTrueEnabledPrefChangeCallback(const char* aPrefName, void* aClosure)
{
  NS_ASSERTION(strcmp(aPrefName, TEXT_ALIGN_TRUE_ENABLED_PREF_NAME) == 0,
               "Did you misspell " TEXT_ALIGN_TRUE_ENABLED_PREF_NAME " ?");

  static bool sIsInitialized;
  static int32_t sIndexOfTrueInTextAlignTable;
  static int32_t sIndexOfTrueInTextAlignLastTable;
  bool isTextAlignTrueEnabled =
    Preferences::GetBool(TEXT_ALIGN_TRUE_ENABLED_PREF_NAME, false);

  if (!sIsInitialized) {
    // First run: find the position of "true" in kTextAlignKTable.
    sIndexOfTrueInTextAlignTable =
      nsCSSProps::FindIndexOfKeyword(eCSSKeyword_true,
                                     nsCSSProps::kTextAlignKTable);
    // First run: find the position of "true" in kTextAlignLastKTable.
    sIndexOfTrueInTextAlignLastTable =
      nsCSSProps::FindIndexOfKeyword(eCSSKeyword_true,
                                     nsCSSProps::kTextAlignLastKTable);
    sIsInitialized = true;
  }

  // OK -- now, stomp on or restore the "true" entry in the keyword tables,
  // depending on whether the pref is enabled vs. disabled.
  MOZ_ASSERT(sIndexOfTrueInTextAlignTable >= 0);
  nsCSSProps::kTextAlignKTable[sIndexOfTrueInTextAlignTable] =
    isTextAlignTrueEnabled ? eCSSKeyword_true : eCSSKeyword_UNKNOWN;
  MOZ_ASSERT(sIndexOfTrueInTextAlignLastTable >= 0);
  nsCSSProps::kTextAlignLastKTable[sIndexOfTrueInTextAlignLastTable] =
    isTextAlignTrueEnabled ? eCSSKeyword_true : eCSSKeyword_UNKNOWN;
}

bool
nsLayoutUtils::HasAnimationsForCompositor(const nsIFrame* aFrame,
                                          nsCSSProperty aProperty)
{
  nsPresContext* presContext = aFrame->PresContext();
  return presContext->AnimationManager()->GetAnimationsForCompositor(aFrame, aProperty) ||
         presContext->TransitionManager()->GetAnimationsForCompositor(aFrame, aProperty);
}

bool
nsLayoutUtils::HasAnimations(const nsIFrame* aFrame,
                             nsCSSProperty aProperty)
{
  nsPresContext* presContext = aFrame->PresContext();
  AnimationCollection* collection =
    presContext->AnimationManager()->GetAnimationCollection(aFrame);
  if (collection &&
      collection->HasAnimationOfProperty(aProperty)) {
    return true;
  }
  collection =
    presContext->TransitionManager()->GetAnimationCollection(aFrame);
  if (collection &&
      collection->HasAnimationOfProperty(aProperty)) {
    return true;
  }
  return false;
}

bool
nsLayoutUtils::HasCurrentAnimations(const nsIFrame* aFrame)
{
  nsPresContext* presContext = aFrame->PresContext();
  AnimationCollection* collection =
    presContext->AnimationManager()->GetAnimationCollection(aFrame);
  return collection &&
         collection->HasCurrentAnimations();
}

bool
nsLayoutUtils::HasCurrentTransitions(const nsIFrame* aFrame)
{
  nsPresContext* presContext = aFrame->PresContext();
  AnimationCollection* collection =
    presContext->TransitionManager()->GetAnimationCollection(aFrame);
  return collection &&
         collection->HasCurrentAnimations();
}

bool
nsLayoutUtils::HasCurrentAnimationsForProperties(const nsIFrame* aFrame,
                                                 const nsCSSProperty* aProperties,
                                                 size_t aPropertyCount)
{
  nsPresContext* presContext = aFrame->PresContext();
  AnimationCollection* collection =
    presContext->AnimationManager()->GetAnimationCollection(aFrame);
  if (collection &&
      collection->HasCurrentAnimationsForProperties(aProperties,
                                                    aPropertyCount)) {
    return true;
  }
  collection =
    presContext->TransitionManager()->GetAnimationCollection(aFrame);
  if (collection &&
      collection->HasCurrentAnimationsForProperties(aProperties,
                                                    aPropertyCount)) {
    return true;
  }
  return false;
}

static gfxSize
GetScaleForValue(const StyleAnimationValue& aValue, const nsIFrame* aFrame)
{
  if (!aFrame) {
    NS_WARNING("No frame.");
    return gfxSize();
  }
  if (aValue.GetUnit() != StyleAnimationValue::eUnit_Transform) {
    NS_WARNING("Expected a transform.");
    return gfxSize();
  }

  nsCSSValueSharedList* list = aValue.GetCSSValueSharedListValue();
  MOZ_ASSERT(list->mHead);

  RuleNodeCacheConditions dontCare;
  TransformReferenceBox refBox(aFrame);
  gfx3DMatrix transform = nsStyleTransformMatrix::ReadTransforms(
                            list->mHead,
                            aFrame->StyleContext(),
                            aFrame->PresContext(), dontCare, refBox,
                            aFrame->PresContext()->AppUnitsPerDevPixel());

  gfxMatrix transform2d;
  bool canDraw2D = transform.CanDraw2D(&transform2d);
  if (!canDraw2D) {
    return gfxSize();
  }

  return transform2d.ScaleFactors(true);
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
  return std::max(std::min(aMaxScale, displayVisibleRatio), aMinScale);
}

static void
GetMinAndMaxScaleForAnimationProperty(const nsIFrame* aFrame,
                                      AnimationCollection* aAnimations,
                                      gfxSize& aMaxScale,
                                      gfxSize& aMinScale)
{
  for (size_t animIdx = aAnimations->mAnimations.Length(); animIdx-- != 0; ) {
    dom::Animation* anim = aAnimations->mAnimations[animIdx];
    if (!anim->GetEffect() || anim->GetEffect()->IsFinishedTransition()) {
      continue;
    }
    dom::KeyframeEffectReadOnly* effect = anim->GetEffect();
    for (size_t propIdx = effect->Properties().Length(); propIdx-- != 0; ) {
      AnimationProperty& prop = effect->Properties()[propIdx];
      if (prop.mProperty == eCSSProperty_transform) {
        for (uint32_t segIdx = prop.mSegments.Length(); segIdx-- != 0; ) {
          AnimationPropertySegment& segment = prop.mSegments[segIdx];
          gfxSize from = GetScaleForValue(segment.mFromValue, aFrame);
          aMaxScale.width = std::max<float>(aMaxScale.width, from.width);
          aMaxScale.height = std::max<float>(aMaxScale.height, from.height);
          aMinScale.width = std::min<float>(aMinScale.width, from.width);
          aMinScale.height = std::min<float>(aMinScale.height, from.height);
          gfxSize to = GetScaleForValue(segment.mToValue, aFrame);
          aMaxScale.width = std::max<float>(aMaxScale.width, to.width);
          aMaxScale.height = std::max<float>(aMaxScale.height, to.height);
          aMinScale.width = std::min<float>(aMinScale.width, to.width);
          aMinScale.height = std::min<float>(aMinScale.height, to.height);
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
  nsPresContext* presContext = aFrame->PresContext();

  AnimationCollection* animations =
    presContext->AnimationManager()->GetAnimationsForCompositor(
      aFrame, eCSSProperty_transform);
  if (animations) {
    GetMinAndMaxScaleForAnimationProperty(aFrame, animations,
                                          maxScale, minScale);
  }

  animations =
    presContext->TransitionManager()->GetAnimationsForCompositor(
      aFrame, eCSSProperty_transform);
  if (animations) {
    GetMinAndMaxScaleForAnimationProperty(aFrame, animations,
                                          maxScale, minScale);
  }

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
nsLayoutUtils::UseBackgroundNearestFiltering()
{
  static bool sUseBackgroundNearestFilteringEnabled;
  static bool sUseBackgroundNearestFilteringPrefInitialised = false;

  if (!sUseBackgroundNearestFilteringPrefInitialised) {
    sUseBackgroundNearestFilteringPrefInitialised = true;
    sUseBackgroundNearestFilteringEnabled =
      Preferences::GetBool("gfx.filter.nearest.force-enabled", false);
  }

  return sUseBackgroundNearestFilteringEnabled;
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
nsLayoutUtils::IsTextAlignTrueValueEnabled()
{
  static bool sTextAlignTrueValueEnabled;
  static bool sTextAlignTrueValueEnabledPrefCached = false;

  if (!sTextAlignTrueValueEnabledPrefCached) {
    sTextAlignTrueValueEnabledPrefCached = true;
    Preferences::AddBoolVarCache(&sTextAlignTrueValueEnabled,
                                 TEXT_ALIGN_TRUE_ENABLED_PREF_NAME,
                                 false);
  }

  return sTextAlignTrueValueEnabled;
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

nsIScrollableFrame*
nsLayoutUtils::FindScrollableFrameFor(ViewID aId)
{
  nsIContent* content = FindContentFor(aId);
  if (!content) {
    return nullptr;
  }

  nsIFrame* scrolledFrame = content->GetPrimaryFrame();
  if (scrolledFrame && content->OwnerDoc()->GetRootElement() == content) {
    // The content is the root element of a subdocument, so return the root scrollable
    // for the subdocument.
    scrolledFrame = scrolledFrame->PresContext()->PresShell()->GetRootScrollFrame();
  }
  return scrolledFrame ? scrolledFrame->GetScrollTargetFrame() : nullptr;
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
  if (!gfxPrefs::AsyncPanZoomEnabledDoNotUseDirectly()) {
    return false;
  }

  nsIFrame *frame = nsLayoutUtils::GetDisplayRootFrame(aFrame);
  nsIWidget* widget = frame->GetNearestWidget();
  if (!widget) {
    return false;
  }
  return widget->AsyncPanZoomEnabled();
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
    NS_WARNING("Attempting to get a margins-based displayport with no base data!");
  }

  nsIFrame* frame = aContent->GetPrimaryFrame();
  if (!frame) {
    // Turns out we can't really compute it. Oops. We still should return
    // something sane. Note that although we can apply the multiplier on the
    // base rect here, we can't tile-align or clamp the rect without a frame.
    NS_WARNING("Attempting to get a displayport from a content with no primary frame!");
    return ApplyRectMultiplier(base, aMultiplier);
  }

  bool isRoot = false;
  if (aContent->OwnerDoc()->GetRootElement() == aContent) {
    // We want the scroll frame, the root scroll frame differs from all
    // others in that the primary frame is not the scroll frame.
    frame = frame->PresContext()->PresShell()->GetRootScrollFrame();
    if (!frame) {
      // If there is no root scrollframe, just exit.
      return ApplyRectMultiplier(base, aMultiplier);
    }

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

  if (gfxPrefs::LayersTilesEnabled()) {
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
    int alignmentX = gfxPlatform::GetPlatform()->GetTileWidth();
    int alignmentY = gfxPlatform::GetPlatform()->GetTileHeight();

    // Expand the rect by the margins
    screenRect.Inflate(aMarginsData->mMargins);

    // Inflate the rectangle by 1 so that we always push to the next tile
    // boundary. This is desirable to stop from having a rectangle with a
    // moving origin occasionally being smaller when it coincidentally lines
    // up to tile boundaries.
    screenRect.Inflate(1);

    // Avoid division by zero.
    if (alignmentX == 0) {
      alignmentX = 1;
    }
    if (alignmentY == 0) {
      alignmentY = 1;
    }

    ScreenPoint scrollPosScreen = LayoutDevicePoint::FromAppUnits(scrollPos, auPerDevPixel)
                                * res;

    screenRect += scrollPosScreen;
    float x = alignmentX * floor(screenRect.x / alignmentX);
    float y = alignmentY * floor(screenRect.y / alignmentY);
    float w = alignmentX * ceil(screenRect.XMost() / alignmentX) - x;
    float h = alignmentY * ceil(screenRect.YMost() / alignmentY) - y;
    screenRect = ScreenRect(x, y, w, h);
    screenRect -= scrollPosScreen;
  } else {
    nscoord maxSizeInAppUnits = GetMaxDisplayPortSize(aContent);
    if (maxSizeInAppUnits == nscoord_MAX) {
      // Pick a safe maximum displayport size for sanity purposes. This is the
      // lowest maximum texture size on tileless-platforms (Windows, D3D10).
      maxSizeInAppUnits = presContext->DevPixelsToAppUnits(8192);
    }

    // Find the maximum size in screen pixels.
    int32_t maxSizeInDevPixels = presContext->AppUnitsToDevPixels(maxSizeInAppUnits);
    int32_t maxWidthInScreenPixels = floor(double(maxSizeInDevPixels) * res.xScale);
    int32_t maxHeightInScreenPixels = floor(double(maxSizeInDevPixels) * res.yScale);

    // For each axis, inflate the margins up to the maximum size.
    const ScreenMargin& margins = aMarginsData->mMargins;
    if (screenRect.height < maxHeightInScreenPixels) {
      int32_t budget = maxHeightInScreenPixels - screenRect.height;

      int32_t top = std::min(int32_t(margins.top), budget);
      screenRect.y -= top;
      screenRect.height += top + std::min(int32_t(margins.bottom), budget - top);
    }
    if (screenRect.width < maxWidthInScreenPixels) {
      int32_t budget = maxWidthInScreenPixels - screenRect.width;

      int32_t left = std::min(int32_t(margins.left), budget);
      screenRect.x -= left;
      screenRect.width += left + std::min(int32_t(margins.right), budget - left);
    }
  }

  // Convert the aligned rect back into app units.
  nsRect result = LayoutDeviceRect::ToAppUnits(screenRect / res, auPerDevPixel);

  // Expand it for the low-res buffer if needed
  result = ApplyRectMultiplier(result, aMultiplier);

  // Finally, clamp it to the expanded scrollable rect.
  nsRect expandedScrollableRect = nsLayoutUtils::CalculateExpandedScrollableRect(frame);
  result = expandedScrollableRect.Intersect(result + scrollPos) - scrollPos;

  return result;
}

static bool
GetDisplayPortImpl(nsIContent* aContent, nsRect *aResult, float aMultiplier)
{
  DisplayPortPropertyData* rectData =
    static_cast<DisplayPortPropertyData*>(aContent->GetProperty(nsGkAtoms::DisplayPort));
  DisplayPortMarginsPropertyData* marginsData =
    static_cast<DisplayPortMarginsPropertyData*>(aContent->GetProperty(nsGkAtoms::DisplayPortMargins));

  if (!rectData && !marginsData) {
    // This content element has no displayport data at all
    return false;
  }

  if (!aResult) {
    // We have displayport data, but the caller doesn't want the actual
    // rect, so we don't need to actually compute it.
    return true;
  }

  if (rectData && marginsData) {
    // choose margins if equal priority
    if (rectData->mPriority > marginsData->mPriority) {
      marginsData = nullptr;
    } else {
      rectData = nullptr;
    }
  }

  NS_ASSERTION((rectData == nullptr) != (marginsData == nullptr),
               "Only one of rectData or marginsData should be set!");

  nsRect result;
  if (rectData) {
    result = GetDisplayPortFromRectData(aContent, rectData, aMultiplier);
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

bool
nsLayoutUtils::GetDisplayPort(nsIContent* aContent, nsRect *aResult)
{
  if (gfxPrefs::UseLowPrecisionBuffer()) {
    return GetDisplayPortImpl(aContent, aResult, 1.0f / gfxPrefs::LowPrecisionResolution());
  }
  return GetDisplayPortImpl(aContent, aResult, 1.0f);
}

/* static */ bool
nsLayoutUtils::GetDisplayPortForVisibilityTesting(nsIContent* aContent,
                                                  nsRect* aResult)
{
  return GetDisplayPortImpl(aContent, aResult, 1.0f);
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

  aContent->SetProperty(nsGkAtoms::DisplayPortMargins,
                        new DisplayPortMarginsPropertyData(
                            aMargins, aPriority),
                        nsINode::DeleteProperty<DisplayPortMarginsPropertyData>);

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

  if (aRepaintMode == RepaintMode::Repaint) {
    nsIFrame* rootFrame = aPresShell->FrameManager()->GetRootFrame();
    if (rootFrame) {
      rootFrame->SchedulePaint();
    }
  }

  // Display port margins changing means that the set of visible images may
  // have drastically changed. Schedule an update.
  aPresShell->ScheduleImageVisibilityUpdate();

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

nsContainerFrame*
nsLayoutUtils::LastContinuationWithChild(nsContainerFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "NULL frame pointer");
  nsIFrame* f = aFrame->LastContinuation();
  while (!f->GetFirstPrincipalChild() && f->GetPrevContinuation()) {
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
    } else if (NS_STYLE_DISPLAY_POPUP == disp->mDisplay) {
      // Out-of-flows that are DISPLAY_POPUP must be kids of the root popup set
#ifdef DEBUG
      nsIFrame* parent = aChildFrame->GetParent();
      NS_ASSERTION(parent && parent->GetType() == nsGkAtoms::popupSetFrame,
                   "Unexpected parent");
#endif // DEBUG

      id = nsIFrame::kPopupList;
#endif // MOZ_XUL
    } else {
      NS_ASSERTION(aChildFrame->IsFloating(), "not a floated frame");
      id = nsIFrame::kFloatList;
    }

  } else {
    nsIAtom* childType = aChildFrame->GetType();
    if (nsGkAtoms::menuPopupFrame == childType) {
      nsIFrame* parent = aChildFrame->GetParent();
      MOZ_ASSERT(parent, "nsMenuPopupFrame can't be the root frame");
      if (parent) {
        if (parent->GetType() == nsGkAtoms::popupSetFrame) {
          id = nsIFrame::kPopupList;
        } else {
          nsIFrame* firstPopup = parent->GetFirstChild(nsIFrame::kPopupList);
          MOZ_ASSERT(!firstPopup || !firstPopup->GetNextSibling(),
                     "We assume popupList only has one child, but it has more.");
          id = firstPopup == aChildFrame
                 ? nsIFrame::kPopupList
                 : nsIFrame::kPrincipalList;
        }
      } else {
        id = nsIFrame::kPrincipalList;
      }
    } else if (nsGkAtoms::tableColGroupFrame == childType) {
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

/*static*/ nsIFrame*
nsLayoutUtils::GetBeforeFrameForContent(nsIFrame* aFrame,
                                        nsIContent* aContent)
{
  // We need to call GetGenConPseudos() on the first continuation/ib-split.
  // Find it, for symmetry with GetAfterFrameForContent.
  nsContainerFrame* genConParentFrame =
    FirstContinuationOrIBSplitSibling(aFrame)->GetContentInsertionFrame();
  if (!genConParentFrame) {
    return nullptr;
  }
  nsTArray<nsIContent*>* prop = genConParentFrame->GetGenConPseudos();
  if (prop) {
    const nsTArray<nsIContent*>& pseudos(*prop);
    for (uint32_t i = 0; i < pseudos.Length(); ++i) {
      if (pseudos[i]->GetParent() == aContent &&
          pseudos[i]->NodeInfo()->NameAtom() == nsGkAtoms::mozgeneratedcontentbefore) {
        return pseudos[i]->GetPrimaryFrame();
      }
    }
  }
  // If the first child frame is a pseudo-frame, then try that.
  // Note that the frame we create for the generated content is also a
  // pseudo-frame and so don't drill down in that case.
  nsIFrame* childFrame = genConParentFrame->GetFirstPrincipalChild();
  if (childFrame &&
      childFrame->IsPseudoFrame(aContent) &&
      !childFrame->IsGeneratedContentFrame()) {
    return GetBeforeFrameForContent(childFrame, aContent);
  }
  return nullptr;
}

/*static*/ nsIFrame*
nsLayoutUtils::GetBeforeFrame(nsIFrame* aFrame)
{
  return GetBeforeFrameForContent(aFrame, aFrame->GetContent());
}

/*static*/ nsIFrame*
nsLayoutUtils::GetAfterFrameForContent(nsIFrame* aFrame,
                                       nsIContent* aContent)
{
  // We need to call GetGenConPseudos() on the first continuation,
  // but callers are likely to pass the last.
  nsContainerFrame* genConParentFrame =
    FirstContinuationOrIBSplitSibling(aFrame)->GetContentInsertionFrame();
  if (!genConParentFrame) {
    return nullptr;
  }
  nsTArray<nsIContent*>* prop = genConParentFrame->GetGenConPseudos();
  if (prop) {
    const nsTArray<nsIContent*>& pseudos(*prop);
    for (uint32_t i = 0; i < pseudos.Length(); ++i) {
      if (pseudos[i]->GetParent() == aContent &&
          pseudos[i]->NodeInfo()->NameAtom() == nsGkAtoms::mozgeneratedcontentafter) {
        return pseudos[i]->GetPrimaryFrame();
      }
    }
  }
  // If the last child frame is a pseudo-frame, then try that.
  // Note that the frame we create for the generated content is also a
  // pseudo-frame and so don't drill down in that case.
  genConParentFrame = aFrame->GetContentInsertionFrame();
  if (!genConParentFrame) {
    return nullptr;
  }
  nsIFrame* lastParentContinuation =
    LastContinuationWithChild(static_cast<nsContainerFrame*>(
      LastContinuationOrIBSplitSibling(genConParentFrame)));
  nsIFrame* childFrame =
    lastParentContinuation->GetLastChild(nsIFrame::kPrincipalList);
  if (childFrame &&
      childFrame->IsPseudoFrame(aContent) &&
      !childFrame->IsGeneratedContentFrame()) {
    return GetAfterFrameForContent(childFrame->FirstContinuation(), aContent);
  }
  return nullptr;
}

/*static*/ nsIFrame*
nsLayoutUtils::GetAfterFrame(nsIFrame* aFrame)
{
  return GetAfterFrameForContent(aFrame, aFrame->GetContent());
}

// static
nsIFrame*
nsLayoutUtils::GetClosestFrameOfType(nsIFrame* aFrame, nsIAtom* aFrameType)
{
  for (nsIFrame* frame = aFrame; frame; frame = frame->GetParent()) {
    if (frame->GetType() == aFrameType) {
      return frame;
    }
  }
  return nullptr;
}

// static
nsIFrame*
nsLayoutUtils::GetStyleFrame(nsIFrame* aFrame)
{
  if (aFrame->GetType() == nsGkAtoms::tableOuterFrame) {
    nsIFrame* inner = aFrame->GetFirstPrincipalChild();
    NS_ASSERTION(inner, "Outer table must have an inner");
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

nsIFrame*
nsLayoutUtils::GetFloatFromPlaceholder(nsIFrame* aFrame) {
  NS_ASSERTION(nsGkAtoms::placeholderFrame == aFrame->GetType(),
               "Must have a placeholder here");
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

  nsAutoTArray<nsINode*, 32> content1Ancestors;
  nsINode* c1;
  for (c1 = aContent1; c1 && c1 != aCommonAncestor; c1 = c1->GetParentNode()) {
    content1Ancestors.AppendElement(c1);
  }
  if (!c1 && aCommonAncestor) {
    // So, it turns out aCommonAncestor was not an ancestor of c1. Oops.
    // Never mind. We can continue as if aCommonAncestor was null.
    aCommonAncestor = nullptr;
  }

  nsAutoTArray<nsINode*, 32> content2Ancestors;
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

  nsAutoTArray<nsIFrame*,20> frame2Ancestors;
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

  nsAutoTArray<nsIFrame*,20> frame1Ancestors;
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
  // Make sure the layer is aware of any fixed position margins that have
  // been set.
  nsMargin fixedMargins = aPresContext->PresShell()->GetContentDocumentFixedPositionMargins();
  LayerMargin fixedLayerMargins(NSAppUnitsToFloatPixels(fixedMargins.top, factor) *
                                  aContainerParameters.mYScale,
                                NSAppUnitsToFloatPixels(fixedMargins.right, factor) *
                                  aContainerParameters.mXScale,
                                NSAppUnitsToFloatPixels(fixedMargins.bottom, factor) *
                                  aContainerParameters.mYScale,
                                NSAppUnitsToFloatPixels(fixedMargins.left, factor) *
                                  aContainerParameters.mXScale);

  if (aFixedPosFrame != aViewportFrame) {
    const nsStylePosition* position = aFixedPosFrame->StylePosition();
    if (position->mOffset.GetRightUnit() != eStyleUnit_Auto) {
      if (position->mOffset.GetLeftUnit() != eStyleUnit_Auto) {
        anchor.x = anchorRect.x + anchorRect.width / 2.f;
      } else {
        anchor.x = anchorRect.XMost();
      }
    }
    if (position->mOffset.GetBottomUnit() != eStyleUnit_Auto) {
      if (position->mOffset.GetTopUnit() != eStyleUnit_Auto) {
        anchor.y = anchorRect.y + anchorRect.height / 2.f;
      } else {
        anchor.y = anchorRect.YMost();
      }
    }

    // If the frame is auto-positioned on either axis, set the top/left layer
    // margins to -1, to indicate to the compositor that this layer is
    // unaffected by fixed margins.
    if (position->mOffset.GetLeftUnit() == eStyleUnit_Auto &&
        position->mOffset.GetRightUnit() == eStyleUnit_Auto) {
      fixedLayerMargins.left = -1;
    }
    if (position->mOffset.GetTopUnit() == eStyleUnit_Auto &&
        position->mOffset.GetBottomUnit() == eStyleUnit_Auto) {
      fixedLayerMargins.top = -1;
    }
  }

  aLayer->SetFixedPositionAnchor(anchor);
  aLayer->SetFixedPositionMargins(fixedLayerMargins);
}

bool
nsLayoutUtils::ViewportHasDisplayPort(nsPresContext* aPresContext, nsRect* aDisplayPort)
{
  nsIFrame* rootScrollFrame =
    aPresContext->PresShell()->GetRootScrollFrame();
  return rootScrollFrame &&
    nsLayoutUtils::GetDisplayPort(rootScrollFrame->GetContent(), aDisplayPort);
}

bool
nsLayoutUtils::IsFixedPosFrameInDisplayPort(const nsIFrame* aFrame, nsRect* aDisplayPort)
{
  // Fixed-pos frames are parented by the viewport frame or the page content frame.
  // We'll assume that printing/print preview don't have displayports for their
  // pages!
  nsIFrame* parent = aFrame->GetParent();
  if (!parent || parent->GetParent() ||
      aFrame->StyleDisplay()->mPosition != NS_STYLE_POSITION_FIXED) {
    return false;
  }
  return ViewportHasDisplayPort(aFrame->PresContext(), aDisplayPort);
}

NS_DECLARE_FRAME_PROPERTY(ScrollbarThumbLayerized, nullptr)

/* static */ void
nsLayoutUtils::SetScrollbarThumbLayerization(nsIFrame* aThumbFrame, bool aLayerize)
{
  aThumbFrame->Properties().Set(ScrollbarThumbLayerized(),
    reinterpret_cast<void*>(intptr_t(aLayerize)));
}

bool
nsLayoutUtils::IsScrollbarThumbLayerized(nsIFrame* aThumbFrame)
{
  return reinterpret_cast<intptr_t>(aThumbFrame->Properties().Get(ScrollbarThumbLayerized()));
}

nsIFrame*
nsLayoutUtils::GetAnimatedGeometryRootForFrame(nsDisplayListBuilder* aBuilder,
                                               nsIFrame* aFrame,
                                               const nsIFrame* aStopAtAncestor)
{
  return aBuilder->FindAnimatedGeometryRootFor(aFrame, aStopAtAncestor);
}

nsIFrame*
nsLayoutUtils::GetAnimatedGeometryRootFor(nsDisplayItem* aItem,
                                          nsDisplayListBuilder* aBuilder,
                                          LayerManager* aManager)
{
  nsIFrame* f = aItem->Frame();
  if (aItem->ShouldFixToViewport(aManager)) {
    // Make its active scrolled root be the active scrolled root of
    // the enclosing viewport, since it shouldn't be scrolled by scrolled
    // frames in its document. InvalidateFixedBackgroundFramesFromList in
    // nsGfxScrollFrame will not repaint this item when scrolling occurs.
    nsIFrame* viewportFrame =
      nsLayoutUtils::GetClosestFrameOfType(f, nsGkAtoms::viewportFrame);
    NS_ASSERTION(viewportFrame, "no viewport???");
    return GetAnimatedGeometryRootForFrame(aBuilder, viewportFrame,
        aBuilder->FindReferenceFrameFor(viewportFrame));
  }
  return GetAnimatedGeometryRootForFrame(aBuilder, f, aItem->ReferenceFrame());
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
        continue;
      }
      ScrollbarStyles ss = scrollableFrame->GetScrollbarStyles();
      if ((aFlags & SCROLLABLE_INCLUDE_HIDDEN) ||
          ss.mVertical != NS_STYLE_OVERFLOW_HIDDEN ||
          ss.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN)
        return scrollableFrame;
    }
    if (aFlags & SCROLLABLE_ALWAYS_MATCH_ROOT) {
      nsIPresShell* ps = f->PresContext()->PresShell();
      if (ps->GetDocument() && ps->GetDocument()->IsRootDisplayDocument() &&
          ps->GetRootFrame() == f) {
        return ps->GetRootScrollFrameAsScrollable();
      }
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
  nscoord x1 = aScrolledFrameOverflowArea.x,
          x2 = aScrolledFrameOverflowArea.XMost(),
          y1 = aScrolledFrameOverflowArea.y,
          y2 = aScrolledFrameOverflowArea.YMost();
  if (y1 < 0) {
    y1 = 0;
  }
  if (aDirection != NS_STYLE_DIRECTION_RTL) {
    if (x1 < 0) {
      x1 = 0;
    }
  } else {
    if (x2 > aScrollPortSize.width) {
      x2 = aScrollPortSize.width;
    }
    // When the scrolled frame chooses a size larger than its available width (because
    // its padding alone is larger than the available width), we need to keep the
    // start-edge of the scroll frame anchored to the start-edge of the scrollport.
    // When the scrolled frame is RTL, this means moving it in our left-based
    // coordinate system, so we need to compensate for its extra width here by
    // effectively repositioning the frame.
    nscoord extraWidth = std::max(0, aScrolledFrame->GetSize().width - aScrollPortSize.width);
    x2 += extraWidth;
  }
  return nsRect(x1, y1, x2 - x1, y2 - y1);
}

//static
bool
nsLayoutUtils::HasPseudoStyle(nsIContent* aContent,
                              nsStyleContext* aStyleContext,
                              nsCSSPseudoElements::Type aPseudoElement,
                              nsPresContext* aPresContext)
{
  NS_PRECONDITION(aPresContext, "Must have a prescontext");

  nsRefPtr<nsStyleContext> pseudoContext;
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
  WidgetEvent* event = aDOMEvent->GetInternalNSEvent();
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
           aEvent->AsGUIEvent()->refPoint,
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

  nsIWidget* widget = aEvent->AsGUIEvent()->widget;
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
      return pt - view->ViewToWidgetOffset();
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

  /* If we encountered a transform, we can't do simple arithmetic to figure
   * out how to convert back to aFrame's coordinates and must use the CTM.
   */
  if (transformFound || aFrame->IsSVGText()) {
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
  nscoord xDiff = std::max(aRadii[NS_CORNER_TOP_LEFT_X], aRadii[NS_CORNER_BOTTOM_LEFT_X]);
  rectFullHeight.x += xDiff;
  rectFullHeight.width -= std::max(aRadii[NS_CORNER_TOP_RIGHT_X],
                                 aRadii[NS_CORNER_BOTTOM_RIGHT_X]) + xDiff;
  nsRect r1;
  r1.IntersectRect(rectFullHeight, aContainedRect);

  nsRect rectFullWidth = aRoundedRect;
  nscoord yDiff = std::max(aRadii[NS_CORNER_TOP_LEFT_Y], aRadii[NS_CORNER_TOP_RIGHT_Y]);
  rectFullWidth.y += yDiff;
  rectFullWidth.height -= std::max(aRadii[NS_CORNER_BOTTOM_LEFT_Y],
                                 aRadii[NS_CORNER_BOTTOM_RIGHT_Y]) + yDiff;
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
                     aRadii[NS_CORNER_TOP_LEFT_X],
                     aRadii[NS_CORNER_TOP_LEFT_Y]) &&
         CheckCorner(insets.right, insets.top,
                     aRadii[NS_CORNER_TOP_RIGHT_X],
                     aRadii[NS_CORNER_TOP_RIGHT_Y]) &&
         CheckCorner(insets.right, insets.bottom,
                     aRadii[NS_CORNER_BOTTOM_RIGHT_X],
                     aRadii[NS_CORNER_BOTTOM_RIGHT_Y]) &&
         CheckCorner(insets.left, insets.bottom,
                     aRadii[NS_CORNER_BOTTOM_LEFT_X],
                     aRadii[NS_CORNER_BOTTOM_LEFT_Y]);
}

nsRect
nsLayoutUtils::MatrixTransformRectOut(const nsRect &aBounds,
                                      const gfx3DMatrix &aMatrix, float aFactor)
{
  nsRect outside = aBounds;
  outside.ScaleRoundOut(1/aFactor);
  gfxRect image = aMatrix.TransformBounds(gfxRect(outside.x,
                                                  outside.y,
                                                  outside.width,
                                                  outside.height));
  return RoundGfxRectToAppRect(image, aFactor);
}

nsRect
nsLayoutUtils::MatrixTransformRect(const nsRect &aBounds,
                                   const gfx3DMatrix &aMatrix, float aFactor)
{
  gfxRect image = aMatrix.TransformBounds(gfxRect(NSAppUnitsToDoublePixels(aBounds.x, aFactor),
                                                  NSAppUnitsToDoublePixels(aBounds.y, aFactor),
                                                  NSAppUnitsToDoublePixels(aBounds.width, aFactor),
                                                  NSAppUnitsToDoublePixels(aBounds.height, aFactor)));

  return RoundGfxRectToAppRect(image, aFactor);
}

nsPoint
nsLayoutUtils::MatrixTransformPoint(const nsPoint &aPoint,
                                    const gfx3DMatrix &aMatrix, float aFactor)
{
  gfxPoint image = aMatrix.Transform(gfxPoint(NSAppUnitsToFloatPixels(aPoint.x, aFactor),
                                              NSAppUnitsToFloatPixels(aPoint.y, aFactor)));
  return nsPoint(NSFloatPixelsToAppUnits(float(image.x), aFactor),
                 NSFloatPixelsToAppUnits(float(image.y), aFactor));
}

Matrix4x4
nsLayoutUtils::GetTransformToAncestor(nsIFrame *aFrame, const nsIFrame *aAncestor)
{
  nsIFrame* parent;
  Matrix4x4 ctm;
  if (aFrame == aAncestor) {
    return ctm;
  }
  ctm = aFrame->GetTransformMatrix(aAncestor, &parent);
  while (parent && parent != aAncestor) {
    if (!parent->Preserves3DChildren()) {
      ctm.ProjectTo2D();
    }
    ctm = ctm * parent->GetTransformMatrix(aAncestor, &parent);
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


nsIFrame*
nsLayoutUtils::FindNearestCommonAncestorFrame(nsIFrame* aFrame1, nsIFrame* aFrame2)
{
  nsAutoTArray<nsIFrame*,100> ancestors1;
  nsAutoTArray<nsIFrame*,100> ancestors2;
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
  CSSToLayoutDeviceScale devPixelsPerCSSPixelFromFrame(
    double(nsPresContext::AppUnitsPerCSSPixel())/
      aFromFrame->PresContext()->AppUnitsPerDevPixel());
  CSSToLayoutDeviceScale devPixelsPerCSSPixelToFrame(
    double(nsPresContext::AppUnitsPerCSSPixel())/
      aToFrame->PresContext()->AppUnitsPerDevPixel());
  for (uint32_t i = 0; i < aPointCount; ++i) {
    LayoutDevicePoint devPixels = aPoints[i] * devPixelsPerCSSPixelFromFrame;
    // What should the behaviour be if some of the points aren't invertible
    // and others are? Just assume all points are for now.
    Point toDevPixels = downToDest.ProjectPoint(
        (upToAncestor * Point(devPixels.x, devPixels.y))).As2DPoint();
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
      upToAncestor * Point(aPoint.x * devPixelsPerAppUnitFromFrame,
                           aPoint.y * devPixelsPerAppUnitFromFrame));
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
  aRect.x = toDevPixels.x / devPixelsPerAppUnitToFrame;
  aRect.y = toDevPixels.y / devPixelsPerAppUnitToFrame;
  aRect.width = toDevPixels.width / devPixelsPerAppUnitToFrame;
  aRect.height = toDevPixels.height / devPixelsPerAppUnitToFrame;
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

bool
nsLayoutUtils::IsRectVisibleInScrollFrames(nsIFrame* aFrame, const nsRect& aRect)
{
  return !ClampRectToScrollFrames(aFrame, aRect).IsEmpty();
}

nsRect
nsLayoutUtils::ClampRectToScrollFrames(nsIFrame* aFrame, const nsRect& aRect)
{
  nsIFrame* closestScrollFrame =
    nsLayoutUtils::GetClosestFrameOfType(aFrame, nsGkAtoms::scrollFrame);

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
    closestScrollFrame =
      nsLayoutUtils::GetClosestFrameOfType(closestScrollFrame->GetParent(),
                                           nsGkAtoms::scrollFrame);
  }

  return resultRect;
}

bool
nsLayoutUtils::GetLayerTransformForFrame(nsIFrame* aFrame,
                                         Matrix4x4* aTransform)
{
  // FIXME/bug 796690: we can sometimes compute a transform in these
  // cases, it just increases complexity considerably.  Punt for now.
  if (aFrame->Preserves3DChildren() || aFrame->HasTransformGetter()) {
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

  nsDisplayListBuilder builder(root, nsDisplayListBuilder::OTHER,
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
                           bool* aPreservesAxisAlignedRectangles = nullptr)
{
  Matrix4x4 ctm = nsLayoutUtils::GetTransformToAncestor(aFrame, aAncestor);
  if (aPreservesAxisAlignedRectangles) {
    Matrix matrix2d;
    *aPreservesAxisAlignedRectangles =
      ctm.Is2D(&matrix2d) && matrix2d.PreservesAxisAlignedRectangles();
  }
  return ctm.TransformBounds(aRect);
}

static SVGTextFrame*
GetContainingSVGTextFrame(nsIFrame* aFrame)
{
  if (!aFrame->IsSVGText()) {
    return nullptr;
  }

  return static_cast<SVGTextFrame*>
    (nsLayoutUtils::GetClosestFrameOfType(aFrame->GetParent(),
                                          nsGkAtoms::svgTextFrame));
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
                                            bool* aPreservesAxisAlignedRectangles /* = nullptr */)
{
  SVGTextFrame* text = GetContainingSVGTextFrame(aFrame);

  float srcAppUnitsPerDevPixel = aFrame->PresContext()->AppUnitsPerDevPixel();
  Rect result;

  if (text) {
    result = ToRect(text->TransformFrameRectFromTextChild(aRect, aFrame));
    result = TransformGfxRectToAncestor(text, result, aAncestor);
    // TransformFrameRectFromTextChild could involve any kind of transform, we
    // could drill down into it to get an answer out of it but we don't yet.
    if (aPreservesAxisAlignedRectangles)
      *aPreservesAxisAlignedRectangles = false;
  } else {
    result = Rect(NSAppUnitsToFloatPixels(aRect.x, srcAppUnitsPerDevPixel),
                  NSAppUnitsToFloatPixels(aRect.y, srcAppUnitsPerDevPixel),
                  NSAppUnitsToFloatPixels(aRect.width, srcAppUnitsPerDevPixel),
                  NSAppUnitsToFloatPixels(aRect.height, srcAppUnitsPerDevPixel));
    result = TransformGfxRectToAncestor(aFrame, result, aAncestor, aPreservesAxisAlignedRectangles);
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
    nsIntRect bounds;
    aWidget->GetBounds(bounds);
    offset += LayoutDeviceIntPoint::FromUntyped(bounds.TopLeft());
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

  LayoutDeviceIntPoint relativeToViewWidget(aPresContext->AppUnitsToDevPixels(aPt.x + viewOffset.x),
                                            aPresContext->AppUnitsToDevPixels(aPt.y + viewOffset.y));
  return relativeToViewWidget + WidgetToWidgetOffset(viewWidget, aWidget);
}

// Combine aNewBreakType with aOrigBreakType, but limit the break types
// to NS_STYLE_CLEAR_LEFT, RIGHT, BOTH.
uint8_t
nsLayoutUtils::CombineBreakType(uint8_t aOrigBreakType,
                                uint8_t aNewBreakType)
{
  uint8_t breakType = aOrigBreakType;
  switch(breakType) {
  case NS_STYLE_CLEAR_LEFT:
    if (NS_STYLE_CLEAR_RIGHT == aNewBreakType ||
        NS_STYLE_CLEAR_BOTH == aNewBreakType) {
      breakType = NS_STYLE_CLEAR_BOTH;
    }
    break;
  case NS_STYLE_CLEAR_RIGHT:
    if (NS_STYLE_CLEAR_LEFT == aNewBreakType ||
        NS_STYLE_CLEAR_BOTH == aNewBreakType) {
      breakType = NS_STYLE_CLEAR_BOTH;
    }
    break;
  case NS_STYLE_CLEAR_NONE:
    if (NS_STYLE_CLEAR_LEFT == aNewBreakType ||
        NS_STYLE_CLEAR_RIGHT == aNewBreakType ||
        NS_STYLE_CLEAR_BOTH == aNewBreakType) {
      breakType = aNewBreakType;
    }
  }
  return breakType;
}

#ifdef MOZ_DUMP_PAINTING
#include <stdio.h>

static bool gDumpEventList = false;
int gPaintCount = 0;
#endif

nsIFrame*
nsLayoutUtils::GetFrameForPoint(nsIFrame* aFrame, nsPoint aPt, uint32_t aFlags)
{
  PROFILER_LABEL("nsLayoutUtils", "GetFrameForPoint",
    js::ProfileEntry::Category::GRAPHICS);

  nsresult rv;
  nsAutoTArray<nsIFrame*,8> outFrames;
  rv = GetFramesForArea(aFrame, nsRect(aPt, nsSize(1, 1)), outFrames, aFlags);
  NS_ENSURE_SUCCESS(rv, nullptr);
  return outFrames.Length() ? outFrames.ElementAt(0) : nullptr;
}

nsresult
nsLayoutUtils::GetFramesForArea(nsIFrame* aFrame, const nsRect& aRect,
                                nsTArray<nsIFrame*> &aOutFrames,
                                uint32_t aFlags)
{
  PROFILER_LABEL("nsLayoutUtils", "GetFramesForArea",
    js::ProfileEntry::Category::GRAPHICS);

  nsDisplayListBuilder builder(aFrame, nsDisplayListBuilder::EVENT_DELIVERY,
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
  builder.LeavePresShell(aFrame);

#ifdef MOZ_DUMP_PAINTING
  if (gDumpEventList) {
    fprintf_stderr(stderr, "Event handling --- (%d,%d):\n", aRect.x, aRect.y);

    std::stringstream ss;
    nsFrame::PrintDisplayList(&builder, list, ss);
    print_stderr(ss);
  }
#endif

  nsDisplayItem::HitTestState hitTestState;
  list.HitTest(&builder, aRect, &hitTestState, &aOutFrames);
  list.DeleteAll();
  return NS_OK;
}

// aScrollFrameAsScrollable must be non-nullptr and queryable to an nsIFrame
static FrameMetrics
CalculateFrameMetricsForDisplayPort(nsIScrollableFrame* aScrollFrame) {
  nsIFrame* frame = do_QueryFrame(aScrollFrame);
  MOZ_ASSERT(frame);

  // Calculate the metrics necessary for calculating the displayport.
  // This code has a lot in common with the code in ComputeFrameMetrics();
  // we may want to refactor this at some point.
  FrameMetrics metrics;
  nsPresContext* presContext = frame->PresContext();
  nsIPresShell* presShell = presContext->PresShell();
  CSSToLayoutDeviceScale deviceScale(float(nsPresContext::AppUnitsPerCSSPixel())
                                     / presContext->AppUnitsPerDevPixel());
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

  FrameMetrics metrics = CalculateFrameMetricsForDisplayPort(aScrollFrame);
  ScreenMargin displayportMargins = APZCTreeManager::CalculatePendingDisplayPort(
      metrics, ParentLayerPoint(0.0f, 0.0f), 0.0);
  nsIPresShell* presShell = frame->PresContext()->GetPresShell();
  return nsLayoutUtils::SetDisplayPortMargins(
      content, presShell, displayportMargins, 0, aRepaintMode);
}

bool
nsLayoutUtils::GetOrMaybeCreateDisplayPort(nsDisplayListBuilder& aBuilder,
                                           nsIFrame* aScrollFrame,
                                           nsRect aDisplayPortBase,
                                           nsRect* aOutDisplayport) {
  nsIContent* content = aScrollFrame->GetContent();
  nsIScrollableFrame* scrollableFrame = do_QueryFrame(aScrollFrame);
  if (!content || !scrollableFrame) {
    return false;
  }

  // Set the base rect. Note that this will not influence 'haveDisplayPort',
  // which is based on either the whole rect or margins being set, but it
  // will affect what is returned in 'aOutDisplayPort' if margins are set.
  SetDisplayPortBase(content, aDisplayPortBase);

  bool haveDisplayPort = GetDisplayPort(content, aOutDisplayport);

  // We perform an optimization where we ensure that at least one
  // async-scrollable frame (i.e. one that WantsAsyncScroll()) has a displayport.
  // If that's not the case yet, and we are async-scrollable, we will get a
  // displayport.
  // Note: we only do this in processes where we do subframe scrolling to
  //       begin with (i.e., not in the parent process on B2G).
  if (aBuilder.IsPaintingToWindow() &&
      nsLayoutUtils::AsyncPanZoomEnabled(aScrollFrame) &&
      !aBuilder.HaveScrollableDisplayPort() &&
      scrollableFrame->WantAsyncScroll()) {

    // If we don't already have a displayport, calculate and set one.
    if (!haveDisplayPort) {
      CalculateAndSetDisplayPortMargins(scrollableFrame, nsLayoutUtils::RepaintMode::DoNotRepaint);
      haveDisplayPort = GetDisplayPort(content, aOutDisplayport);
      NS_ASSERTION(haveDisplayPort, "should have a displayport after having just set it");
    }

    // Record that the we now have a scrollable display port.
    aBuilder.SetHaveScrollableDisplayPort();
  }

  return haveDisplayPort;
}

nsresult
nsLayoutUtils::PaintFrame(nsRenderingContext* aRenderingContext, nsIFrame* aFrame,
                          const nsRegion& aDirtyRegion, nscolor aBackstop,
                          uint32_t aFlags)
{
  PROFILER_LABEL("nsLayoutUtils", "PaintFrame",
    js::ProfileEntry::Category::GRAPHICS);

  if (aFlags & PAINT_WIDGET_LAYERS) {
    nsView* view = aFrame->GetView();
    if (!(view && view->GetWidget() && GetDisplayRootFrame(aFrame) == aFrame)) {
      aFlags &= ~PAINT_WIDGET_LAYERS;
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
  nsDisplayListBuilder builder(aFrame, nsDisplayListBuilder::PAINTING,
                           !(aFlags & PAINT_HIDE_CARET));
  if (aFlags & PAINT_IN_TRANSFORM) {
    builder.SetInTransform(true);
  }
  if (aFlags & PAINT_SYNC_DECODE_IMAGES) {
    builder.SetSyncDecodeImages(true);
  }
  if (aFlags & (PAINT_WIDGET_LAYERS | PAINT_TO_WINDOW)) {
    builder.SetPaintingToWindow(true);
  }
  if (aFlags & PAINT_IGNORE_SUPPRESSION) {
    builder.IgnorePaintSuppression();
  }

  nsIFrame* rootScrollFrame = presShell->GetRootScrollFrame();
  bool usingDisplayPort = false;
  nsRect displayport;
  if (rootScrollFrame && !aFrame->GetParent() &&
      (aFlags & (PAINT_WIDGET_LAYERS | PAINT_TO_WINDOW)) &&
      gfxPrefs::LayoutUseContainersForRootFrames()) {
    nsRect displayportBase(
        nsPoint(0,0),
        nsLayoutUtils::CalculateCompositionSizeForFrame(rootScrollFrame));
    usingDisplayPort = nsLayoutUtils::GetOrMaybeCreateDisplayPort(
        builder, rootScrollFrame, displayportBase, &displayport);
  }

  nsRegion visibleRegion;
  if (aFlags & PAINT_WIDGET_LAYERS) {
    // This layer tree will be reused, so we'll need to calculate it
    // for the whole "visible" area of the window
    //
    // |ignoreViewportScrolling| and |usingDisplayPort| are persistent
    // document-rendering state.  We rely on PresShell to flush
    // retained layers as needed when that persistent state changes.
    if (!usingDisplayPort) {
      visibleRegion = aFrame->GetVisualOverflowRectRelativeToSelf();
    } else {
      visibleRegion = displayport;
    }
  } else {
    visibleRegion = aDirtyRegion;
  }

  // If we're going to display something different from what we'd normally
  // paint in a window then we will flush out any retained layer trees before
  // *and after* we draw.
  bool willFlushRetainedLayers = (aFlags & PAINT_HIDE_CARET) != 0;

  nsDisplayList list;

  // If the root has embedded plugins, flag the builder so we know we'll need
  // to update plugin geometry after painting.
  if ((aFlags & PAINT_WIDGET_LAYERS) &&
      !willFlushRetainedLayers &&
      !(aFlags & PAINT_DOCUMENT_RELATIVE) &&
      rootPresContext->NeedToComputePluginGeometryUpdates()) {
    builder.SetWillComputePluginGeometry(true);
  }

  nsRect canvasArea(nsPoint(0, 0), aFrame->GetSize());
  bool ignoreViewportScrolling =
    aFrame->GetParent() ? false : presShell->IgnoringViewportScrolling();
  if (ignoreViewportScrolling && rootScrollFrame) {
    nsIScrollableFrame* rootScrollableFrame =
      presShell->GetRootScrollFrameAsScrollable();
    if (aFlags & PAINT_DOCUMENT_RELATIVE) {
      // Make visibleRegion and aRenderingContext relative to the
      // scrolled frame instead of the root frame.
      nsPoint pos = rootScrollableFrame->GetScrollPosition();
      visibleRegion.MoveBy(-pos);
      if (aRenderingContext) {
        gfxPoint devPixelOffset =
          nsLayoutUtils::PointToGfxPoint(pos,
                                         presContext->AppUnitsPerDevPixel());
        aRenderingContext->ThebesContext()->SetMatrix(
          aRenderingContext->ThebesContext()->CurrentMatrix().Translate(devPixelOffset));
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

  builder.EnterPresShell(aFrame);
  nsRect dirtyRect = visibleRegion.GetBounds();
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
#if !defined(MOZ_WIDGET_ANDROID) || defined(MOZ_ANDROID_APZ)
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
#endif

    nsDisplayListBuilder::AutoCurrentScrollParentIdSetter idSetter(&builder, id);

    PROFILER_LABEL("nsLayoutUtils", "PaintFrame::BuildDisplayList",
      js::ProfileEntry::Category::GRAPHICS);

    aFrame->BuildDisplayListForStackingContext(&builder, dirtyRect, &list);
  }
  const bool paintAllContinuations = aFlags & PAINT_ALL_CONTINUATIONS;
  NS_ASSERTION(!paintAllContinuations || !aFrame->GetPrevContinuation(),
               "If painting all continuations, the frame must be "
               "first-continuation");

  nsIAtom* frameType = aFrame->GetType();

  if (paintAllContinuations) {
    nsIFrame* currentFrame = aFrame;
    while ((currentFrame = currentFrame->GetNextContinuation()) != nullptr) {
      PROFILER_LABEL("nsLayoutUtils", "PaintFrame::ContinuationsBuildDisplayList",
        js::ProfileEntry::Category::GRAPHICS);

      nsRect frameDirty = dirtyRect - builder.ToReferenceFrame(currentFrame);
      currentFrame->BuildDisplayListForStackingContext(&builder,
                                                       frameDirty, &list);
    }
  }

  // For the viewport frame in print preview/page layout we want to paint
  // the grey background behind the page, not the canvas color.
  if (frameType == nsGkAtoms::viewportFrame && 
      nsLayoutUtils::NeedsPrintPreviewBackground(presContext)) {
    nsRect bounds = nsRect(builder.ToReferenceFrame(aFrame),
                           aFrame->GetSize());
    nsDisplayListBuilder::AutoBuildingDisplayList
      buildingDisplayList(&builder, aFrame, bounds, false);
    presShell->AddPrintPreviewBackgroundItem(builder, list, aFrame, bounds);
  } else if (frameType != nsGkAtoms::pageFrame) {
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

    // If the passed in backstop color makes us draw something different from
    // normal, we need to flush layers.
    if ((aFlags & PAINT_WIDGET_LAYERS) && !willFlushRetainedLayers) {
      nsView* view = aFrame->GetView();
      if (view) {
        nscolor backstop = presShell->ComputeBackstopColor(view);
        // The PresShell's canvas background color doesn't get updated until
        // EnterPresShell, so this check has to be done after that.
        nscolor canvasColor = presShell->GetCanvasBackground();
        if (NS_ComposeColors(aBackstop, canvasColor) !=
            NS_ComposeColors(backstop, canvasColor)) {
          willFlushRetainedLayers = true;
        }
      }
    }
  }

  builder.LeavePresShell(aFrame);
  Telemetry::AccumulateTimeDelta(Telemetry::PAINT_BUILD_DISPLAYLIST_TIME,
                                 startBuildDisplayList);

  if (builder.GetHadToIgnorePaintSuppression()) {
    willFlushRetainedLayers = true;
  }


  bool profilerNeedsDisplayList = profiler_feature_active("displaylistdump");
  bool consoleNeedsDisplayList = gfxUtils::DumpDisplayList() || gfxUtils::sDumpPainting;
#ifdef MOZ_DUMP_PAINTING
  FILE* savedDumpFile = gfxUtils::sDumpPaintFile;
#endif

  UniquePtr<std::stringstream> ss;
  if (consoleNeedsDisplayList || profilerNeedsDisplayList) {
    ss = MakeUnique<std::stringstream>();
#ifdef MOZ_DUMP_PAINTING
    if (gfxUtils::sDumpPaintingToFile) {
      nsCString string("dump-");
      string.AppendInt(gPaintCount);
      string.AppendLiteral(".html");
      gfxUtils::sDumpPaintFile = fopen(string.BeginReading(), "w");
    } else {
      gfxUtils::sDumpPaintFile = stderr;
    }
    if (gfxUtils::sDumpPaintingToFile) {
      *ss << "<html><head><script>var array = {}; function ViewImage(index) { window.location = array[index]; }</script></head><body>";
    }
#endif
    *ss << nsPrintfCString("Painting --- before optimization (dirty %d,%d,%d,%d):\n",
            dirtyRect.x, dirtyRect.y, dirtyRect.width, dirtyRect.height).get();
    nsFrame::PrintDisplayList(&builder, list, *ss, gfxUtils::sDumpPaintingToFile);
    if (gfxUtils::sDumpPaintingToFile) {
      *ss << "<script>";
    } else {
      // Flush stream now to avoid reordering dump output relative to
      // messages dumped by PaintRoot below.
      if (profilerNeedsDisplayList && !consoleNeedsDisplayList) {
        profiler_log(ss->str().c_str());
      } else {
        // Send to the console which will send to the profiler if required.
        fprint_stderr(gfxUtils::sDumpPaintFile, *ss);
      }
      ss = MakeUnique<std::stringstream>();
    }
  }

  uint32_t flags = nsDisplayList::PAINT_DEFAULT;
  if (aFlags & PAINT_WIDGET_LAYERS) {
    flags |= nsDisplayList::PAINT_USE_WIDGET_LAYERS;
    if (willFlushRetainedLayers) {
      // The caller wanted to paint from retained layers, but set up
      // the paint in such a way that we can't use them.  We're going
      // to display something different from what we'd normally paint
      // in a window, so make sure we flush out any retained layer
      // trees before *and after* we draw.  Callers should be fixed to
      // not do this.
      NS_WARNING("Flushing retained layers!");
      flags |= nsDisplayList::PAINT_FLUSH_LAYERS;
    } else if (!(aFlags & PAINT_DOCUMENT_RELATIVE)) {
      nsIWidget *widget = aFrame->GetNearestWidget();
      if (widget) {
        // If we're finished building display list items for painting of the outermost
        // pres shell, notify the widget about any toolbars we've encountered.
        widget->UpdateThemeGeometries(builder.GetThemeGeometries());
      }
    }
  }
  if (aFlags & PAINT_EXISTING_TRANSACTION) {
    flags |= nsDisplayList::PAINT_EXISTING_TRANSACTION;
  }
  if (aFlags & PAINT_NO_COMPOSITE) {
    flags |= nsDisplayList::PAINT_NO_COMPOSITE;
  }
  if (aFlags & PAINT_COMPRESSED) {
    flags |= nsDisplayList::PAINT_COMPRESSED;
  }

  TimeStamp paintStart = TimeStamp::Now();
  nsRefPtr<LayerManager> layerManager =
    list.PaintRoot(&builder, aRenderingContext, flags);
  Telemetry::AccumulateTimeDelta(Telemetry::PAINT_RASTERIZE_TIME,
                                 paintStart);

  if (consoleNeedsDisplayList || profilerNeedsDisplayList) {
#ifdef MOZ_DUMP_PAINTING
    if (gfxUtils::sDumpPaintingToFile) {
      *ss << "</script>";
    }
#endif
    *ss << "Painting --- after optimization:\n";
    nsFrame::PrintDisplayList(&builder, list, *ss, gfxUtils::sDumpPaintingToFile);

    *ss << "Painting --- layer tree:\n";
    if (layerManager) {
      FrameLayerBuilder::DumpRetainedLayerTree(layerManager, *ss,
                                               gfxUtils::sDumpPaintingToFile);
    }

    if (profilerNeedsDisplayList && !consoleNeedsDisplayList) {
      profiler_log(ss->str().c_str());
    } else {
      // Send to the console which will send to the profiler if required.
      fprint_stderr(gfxUtils::sDumpPaintFile, *ss);
    }

#ifdef MOZ_DUMP_PAINTING
    if (gfxUtils::sDumpPaintingToFile) {
      *ss << "</body></html>";
    }
    if (gfxUtils::sDumpPaintingToFile) {
      fclose(gfxUtils::sDumpPaintFile);
    }
    gfxUtils::sDumpPaintFile = savedDumpFile;
    gPaintCount++;
#endif
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
  if ((aFlags & PAINT_WIDGET_LAYERS) &&
      !willFlushRetainedLayers &&
      !(aFlags & PAINT_DOCUMENT_RELATIVE)) {
    nsIWidget *widget = aFrame->GetNearestWidget();
    if (widget) {
      nsRegion opaqueRegion;
      opaqueRegion.And(builder.GetWindowExcludeGlassRegion(), builder.GetWindowOpaqueRegion());
      widget->UpdateOpaqueRegion(
        opaqueRegion.ToNearestPixels(presContext->AppUnitsPerDevPixel()));

      const nsIntRegion& draggingRegion = builder.GetWindowDraggingRegion();
      widget->UpdateWindowDraggingRegion(draggingRegion);
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
      layerManager->Composite();
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
nsLayoutUtils::BinarySearchForPosition(nsRenderingContext* aRendContext,
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
                                                     aFontMetrics,
                                                     *aRendContext);
    return true;
  }

  int32_t inx = aStartInx + (range / 2);

  // Make sure we don't leave a dangling low surrogate
  if (NS_IS_HIGH_SURROGATE(aText[inx-1]))
    inx++;

  int32_t textWidth = nsLayoutUtils::AppUnitWidthOfString(aText, inx,
                                                          aFontMetrics,
                                                          *aRendContext);

  int32_t fullWidth = aBaseWidth + textWidth;
  if (fullWidth == aCursorPos) {
    aTextWidth = textWidth;
    aIndex = inx;
    return true;
  } else if (aCursorPos < fullWidth) {
    aTextWidth = aBaseWidth;
    if (BinarySearchForPosition(aRendContext, aFontMetrics, aText, aBaseWidth,
                                aBaseInx, aStartInx, inx, aCursorPos, aIndex,
                                aTextWidth)) {
      return true;
    }
  } else {
    aTextWidth = fullWidth;
    if (BinarySearchForPosition(aRendContext, aFontMetrics, aText, aBaseWidth,
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

  if (pseudoType == nsCSSAnonBoxes::tableOuter) {
    AddBoxesForFrame(aFrame->GetFirstPrincipalChild(), aCallback);
    nsIFrame* kid = aFrame->GetFirstChild(nsIFrame::kCaptionList);
    if (kid) {
      AddBoxesForFrame(kid, aCallback);
    }
  } else if (pseudoType == nsCSSAnonBoxes::mozAnonymousBlock ||
             pseudoType == nsCSSAnonBoxes::mozAnonymousPositionedBlock ||
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

    if (pseudoType == nsCSSAnonBoxes::tableOuter) {
      nsIFrame* f = GetFirstNonAnonymousFrame(aFrame->GetFirstPrincipalChild());
      if (f) {
        return f;
      }
      nsIFrame* kid = aFrame->GetFirstChild(nsIFrame::kCaptionList);
      if (kid) {
        f = GetFirstNonAnonymousFrame(kid);
        if (f) {
          return f;
        }
      }
    } else if (pseudoType == nsCSSAnonBoxes::mozAnonymousBlock ||
               pseudoType == nsCSSAnonBoxes::mozAnonymousPositionedBlock ||
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

void
nsLayoutUtils::GetAllInFlowRects(nsIFrame* aFrame, nsIFrame* aRelativeTo,
                                 RectCallback* aCallback, uint32_t aFlags)
{
  BoxToRect converter(aRelativeTo, aCallback, aFlags);
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
  nsRefPtr<DOMRect> rect = new DOMRect(mRectList);

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
typedef nsStyleBackground::Position::PositionCoord PositionCoord;
static bool
IsCoord50Pct(const PositionCoord& aCoord)
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
  const nsStyleBackground::Position& objectPos = aStylePos->mObjectPosition;

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

nsresult
nsLayoutUtils::GetFontMetricsForFrame(const nsIFrame* aFrame,
                                      nsFontMetrics** aFontMetrics,
                                      float aInflation)
{
  return nsLayoutUtils::GetFontMetricsForStyleContext(aFrame->StyleContext(),
                                                      aFontMetrics,
                                                      aInflation);
}

nsresult
nsLayoutUtils::GetFontMetricsForStyleContext(nsStyleContext* aStyleContext,
                                             nsFontMetrics** aFontMetrics,
                                             float aInflation)
{
  // pass the user font set object into the device context to pass along to CreateFontGroup
  nsPresContext* pc = aStyleContext->PresContext();
  gfxUserFontSet* fs = pc->GetUserFontSet();
  gfxTextPerfMetrics* tp = pc->GetTextPerfMetrics();

  WritingMode wm(aStyleContext);
  gfxFont::Orientation orientation =
    wm.IsVertical() && !wm.IsSideways() ? gfxFont::eVertical
                                        : gfxFont::eHorizontal;

  const nsStyleFont* styleFont = aStyleContext->StyleFont();

  // When aInflation is 1.0, avoid making a local copy of the nsFont.
  // This also avoids running font.size through floats when it is large,
  // which would be lossy.  Fortunately, in such cases, aInflation is
  // guaranteed to be 1.0f.
  if (aInflation == 1.0f) {
    return pc->DeviceContext()->GetMetricsFor(styleFont->mFont,
                                              styleFont->mLanguage,
                                              styleFont->mExplicitLanguage,
                                              orientation, fs, tp,
                                              *aFontMetrics);
  }

  nsFont font = styleFont->mFont;
  font.size = NSToCoordRound(font.size * aInflation);
  return pc->DeviceContext()->GetMetricsFor(font, styleFont->mLanguage,
                                            styleFont->mExplicitLanguage,
                                            orientation, fs, tp,
                                            *aFontMetrics);
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
    return aFrame->PresContext()->PresShell()->FrameManager()->
      GetPlaceholderFrameFor(aFrame);
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

    void* value = aFrame->Properties().Get(nsIFrame::IBSplitSibling());
    return static_cast<nsIFrame*>(value);
  }

  return nullptr;
}

nsIFrame*
nsLayoutUtils::FirstContinuationOrIBSplitSibling(nsIFrame *aFrame)
{
  nsIFrame *result = aFrame->FirstContinuation();
  if (result->GetStateBits() & NS_FRAME_PART_OF_IBSPLIT) {
    while (true) {
      nsIFrame *f = static_cast<nsIFrame*>
        (result->Properties().Get(nsIFrame::IBSplitPrevSibling()));
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
      nsIFrame *f = static_cast<nsIFrame*>
        (result->Properties().Get(nsIFrame::IBSplitSibling()));
      if (!f)
        break;
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
      aFrame->Properties().Get(nsIFrame::IBSplitPrevSibling())) {
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

static nscoord AddPercents(nsLayoutUtils::IntrinsicISizeType aType,
                           nscoord aCurrent, float aPercent)
{
  nscoord result = aCurrent;
  if (aPercent > 0.0f && aType == nsLayoutUtils::PREF_ISIZE) {
    // XXX Should we also consider percentages for min widths, up to a
    // limit?
    if (aPercent >= 1.0f)
      result = nscoord_MAX;
    else
      result = NSToCoordRound(float(result) / (1.0f - aPercent));
  }
  return result;
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

// Only call on style coords for which GetAbsoluteCoord returned false.
static bool
GetPercentBSize(const nsStyleCoord& aStyle,
                 nsIFrame* aFrame,
                 nscoord& aResult)
{
  if (eStyleUnit_Percent != aStyle.GetUnit() &&
      !aStyle.IsCalcUnit())
    return false;

  MOZ_ASSERT(!aStyle.IsCalcUnit() || aStyle.CalcHasPercent(),
             "GetAbsoluteCoord should have handled this");

  nsIFrame *f = aFrame->GetContainingBlock();
  if (!f) {
    NS_NOTREACHED("top of frame tree not a containing block");
    return false;
  }

  // During reflow, nsHTMLScrollFrame::ReflowScrolledFrame uses
  // SetComputedHeight on the reflow state for its child to propagate its
  // computed height to the scrolled content. So here we skip to the scroll
  // frame that contains this scrolled content in order to get the same
  // behavior as layout when computing percentage heights.
  if (f->StyleContext()->GetPseudo() == nsCSSAnonBoxes::scrolledContent) {
    f = f->GetParent();
  }

  WritingMode wm = f->GetWritingMode();

  const nsStylePosition *pos = f->StylePosition();
  const nsStyleCoord& bSizeCoord = pos->BSize(wm);
  nscoord h;
  if (!GetAbsoluteCoord(bSizeCoord, h) &&
      !GetPercentBSize(bSizeCoord, f, h)) {
    NS_ASSERTION(bSizeCoord.GetUnit() == eStyleUnit_Auto ||
                 bSizeCoord.HasPercent(),
                 "unknown block-size unit");
    nsIAtom* fType = f->GetType();
    if (fType != nsGkAtoms::viewportFrame && fType != nsGkAtoms::canvasFrame &&
        fType != nsGkAtoms::pageContentFrame) {
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
      GetPercentBSize(maxBSizeCoord, f, maxh)) {
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
      GetPercentBSize(minBSizeCoord, f, minh)) {
    if (minh > h)
      h = minh;
  } else {
    NS_ASSERTION(minBSizeCoord.HasPercent() ||
                 minBSizeCoord.GetUnit() == eStyleUnit_Auto,
                 "unknown min block-size unit");
  }

  if (aStyle.IsCalcUnit()) {
    aResult = std::max(nsRuleNode::ComputeComputedCalc(aStyle, h), 0);
    return true;
  }

  aResult = NSToCoordRound(aStyle.GetPercentValue() * h);
  return true;
}

// Handles only -moz-max-content and -moz-min-content, and
// -moz-fit-content for min-width and max-width, since the others
// (-moz-fit-content for width, and -moz-available) have no effect on
// intrinsic widths.
enum eWidthProperty { PROP_WIDTH, PROP_MAX_WIDTH, PROP_MIN_WIDTH };
static bool
GetIntrinsicCoord(const nsStyleCoord& aStyle,
                  nsRenderingContext* aRenderingContext,
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
AddIntrinsicSizeOffset(nsRenderingContext* aRenderingContext,
                       nsIFrame* aFrame,
                       const nsIFrame::IntrinsicISizeOffsetData& aOffsets,
                       nsLayoutUtils::IntrinsicISizeType aType,
                       uint8_t aBoxSizing,
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

    if (aBoxSizing == NS_STYLE_BOX_SIZING_PADDING) {
      min += coordOutsideSize;
      result = NSCoordSaturatingAdd(result, coordOutsideSize);
      pctTotal += pctOutsideSize;

      coordOutsideSize = 0;
      pctOutsideSize = 0.0f;
    }
  }

  coordOutsideSize += aOffsets.hBorder;

  if (aBoxSizing == NS_STYLE_BOX_SIZING_BORDER) {
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

  nscoord size;
  if (GetAbsoluteCoord(aStyleSize, size) ||
      GetIntrinsicCoord(aStyleSize, aRenderingContext, aFrame,
                        PROP_WIDTH, size)) {
    result = AddPercents(aType, size + coordOutsideSize, pctOutsideSize);
  } else if (aType == nsLayoutUtils::MIN_ISIZE &&
             // The only cases of coord-percent-calc() units that
             // GetAbsoluteCoord didn't handle are percent and calc()s
             // containing percent.
             aStyleSize.IsCoordPercentCalcUnit() &&
             aFrame->IsFrameOfType(nsIFrame::eReplaced)) {
    // A percentage width on replaced elements means they can shrink to 0.
    result = 0; // let |min| handle padding/border/margin
  } else {
    // NOTE: We could really do a lot better for percents and for some
    // cases of calc() containing percent (certainly including any where
    // the coefficient on the percent is positive and there are no max()
    // expressions).  However, doing better for percents wouldn't be
    // backwards compatible.
    result = AddPercents(aType, result, pctTotal);
  }

  nscoord maxSize = aFixedMaxSize ? *aFixedMaxSize : 0;
  if (aFixedMaxSize ||
      GetIntrinsicCoord(aStyleMaxSize, aRenderingContext, aFrame,
                        PROP_MAX_WIDTH, maxSize)) {
    maxSize = AddPercents(aType, maxSize + coordOutsideSize, pctOutsideSize);
    if (result > maxSize) {
      result = maxSize;
    }
  }

  nscoord minSize = aFixedMinSize ? *aFixedMinSize : 0;
  if (aFixedMinSize ||
      GetIntrinsicCoord(aStyleMinSize, aRenderingContext, aFrame,
                        PROP_MIN_WIDTH, minSize)) {
    minSize = AddPercents(aType, minSize + coordOutsideSize, pctOutsideSize);
    if (result < minSize) {
      result = minSize;
    }
  }

  min = AddPercents(aType, min, pctTotal);
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
    themeSize = AddPercents(aType, themeSize, aOffsets.hPctMargin);

    if (themeSize > result || !canOverride) {
      result = themeSize;
    }
  }
  return result;
}

/* static */ nscoord
nsLayoutUtils::IntrinsicForAxis(PhysicalAxis        aAxis,
                                nsRenderingContext* aRenderingContext,
                                nsIFrame*           aFrame,
                                IntrinsicISizeType  aType,
                                uint32_t            aFlags)
{
  NS_PRECONDITION(aFrame, "null frame");
  NS_PRECONDITION(aFrame->GetParent(),
                  "IntrinsicForAxis called on frame not in tree");
  NS_PRECONDITION(aType == MIN_ISIZE || aType == PREF_ISIZE, "bad type");

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
  uint8_t boxSizing = stylePos->mBoxSizing;

  const nsStyleCoord& styleISize =
    horizontalAxis ? stylePos->mWidth : stylePos->mHeight;
  const nsStyleCoord& styleMinISize =
    horizontalAxis ? stylePos->mMinWidth : stylePos->mMinHeight;
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
    boxSizing = NS_STYLE_BOX_SIZING_CONTENT;
  } else if (!styleISize.ConvertsToLength() &&
             !(haveFixedMinISize && haveFixedMaxISize && maxISize <= minISize)) {
#ifdef DEBUG_INTRINSIC_WIDTH
    ++gNoiseIndent;
#endif
    if (MOZ_UNLIKELY(aAxis != ourInlineAxis)) {
      // We need aFrame's block-dir size.
      // XXX Unfortunately, we probably don't know this yet, so this is wrong...
      // but it's not clear what we should do. If aFrame's inline size hasn't
      // been determined yet, we can't necessarily figure out its block size
      // either. For now, authors who put orthogonal elements into things like
      // buttons or table cells may have to explicitly provide sizes rather
      // than expecting intrinsic sizing to work "perfectly" in underspecified
      // cases.
      result = aFrame->BSize();
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
        nscoord bSizeTakenByBoxSizing = 0;
        switch (boxSizing) {
        case NS_STYLE_BOX_SIZING_BORDER: {
          const nsStyleBorder* styleBorder = aFrame->StyleBorder();
          bSizeTakenByBoxSizing +=
            horizontalAxis ? styleBorder->GetComputedBorder().TopBottom()
                           : styleBorder->GetComputedBorder().LeftRight();
          // fall through
        }
        case NS_STYLE_BOX_SIZING_PADDING: {
          if (!(aFlags & IGNORE_PADDING)) {
            const nsStyleSides& stylePadding =
              aFrame->StylePadding()->mPadding;
            const nsStyleCoord& paddingStart =
              stylePadding.Get(horizontalAxis ? NS_SIDE_TOP : NS_SIDE_LEFT);
            const nsStyleCoord& paddingEnd =
              stylePadding.Get(horizontalAxis ? NS_SIDE_BOTTOM : NS_SIDE_RIGHT);
            nscoord pad;
            if (GetAbsoluteCoord(paddingStart, pad) ||
                GetPercentBSize(paddingStart, aFrame, pad)) {
              bSizeTakenByBoxSizing += pad;
            }
            if (GetAbsoluteCoord(paddingEnd, pad) ||
                GetPercentBSize(paddingEnd, aFrame, pad)) {
              bSizeTakenByBoxSizing += pad;
            }
          }
          // fall through
        }
        case NS_STYLE_BOX_SIZING_CONTENT:
        default:
          break;
        }

        nscoord h;
        if (GetAbsoluteCoord(styleBSize, h) ||
            GetPercentBSize(styleBSize, aFrame, h)) {
          h = std::max(0, h - bSizeTakenByBoxSizing);
          result = NSCoordMulDiv(h, ratioISize, ratioBSize);
        }

        if (GetAbsoluteCoord(styleMaxBSize, h) ||
            GetPercentBSize(styleMaxBSize, aFrame, h)) {
          h = std::max(0, h - bSizeTakenByBoxSizing);
          nscoord maxISize = NSCoordMulDiv(h, ratioISize, ratioBSize);
          if (maxISize < result)
            result = maxISize;
        }

        if (GetAbsoluteCoord(styleMinBSize, h) ||
            GetPercentBSize(styleMinBSize, aFrame, h)) {
          h = std::max(0, h - bSizeTakenByBoxSizing);
          nscoord minISize = NSCoordMulDiv(h, ratioISize, ratioBSize);
          if (minISize > result)
            result = minISize;
        }
      }
    }
  }

  if (aFrame->GetType() == nsGkAtoms::tableFrame) {
    // Tables can't shrink smaller than their intrinsic minimum width,
    // no matter what.
    min = aFrame->GetMinISize(aRenderingContext);
  }

  nsIFrame::IntrinsicISizeOffsetData offsets =
    MOZ_LIKELY(aAxis == ourInlineAxis) ? aFrame->IntrinsicISizeOffsets()
                                       : aFrame->IntrinsicBSizeOffsets();
  result = AddIntrinsicSizeOffset(aRenderingContext, aFrame, offsets, aType,
                                  boxSizing, result, min, styleISize,
                                  haveFixedMinISize ? &minISize : nullptr,
                                  styleMinISize,
                                  haveFixedMaxISize ? &maxISize : nullptr,
                                  styleMaxISize,
                                  aFlags, aAxis);

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
nsLayoutUtils::IntrinsicForContainer(nsRenderingContext* aRenderingContext,
                                     nsIFrame* aFrame,
                                     IntrinsicISizeType aType,
                                     uint32_t aFlags)
{
  MOZ_ASSERT(aFrame && aFrame->GetParent());
  // We want the size aFrame will contribute to its parent's inline-size.
  PhysicalAxis axis =
    aFrame->GetParent()->GetWritingMode().PhysicalAxis(eLogicalAxisInline);
  return IntrinsicForAxis(axis, aRenderingContext, aFrame, aType, aFlags);
}

/* static */ nscoord
nsLayoutUtils::ComputeCBDependentValue(nscoord aPercentBasis,
                                       const nsStyleCoord& aCoord)
{
  NS_WARN_IF_FALSE(aPercentBasis != NS_UNCONSTRAINEDSIZE,
                   "have unconstrained width or height; this should only "
                   "result from very large sizes, not attempts at intrinsic "
                   "size calculation");

  if (aCoord.IsCoordPercentCalcUnit()) {
    return nsRuleNode::ComputeCoordPercentCalc(aCoord, aPercentBasis);
  }
  NS_ASSERTION(aCoord.GetUnit() == eStyleUnit_None ||
               aCoord.GetUnit() == eStyleUnit_Auto,
               "unexpected width value");
  return 0;
}

/* static */ nscoord
nsLayoutUtils::ComputeISizeValue(
                 nsRenderingContext* aRenderingContext,
                 nsIFrame*            aFrame,
                 nscoord              aContainingBlockISize,
                 nscoord              aContentEdgeToBoxSizing,
                 nscoord              aBoxSizingToMarginEdge,
                 const nsStyleCoord&  aCoord)
{
  NS_PRECONDITION(aFrame, "non-null frame expected");
  NS_PRECONDITION(aRenderingContext, "non-null rendering context expected");
  NS_WARN_IF_FALSE(aContainingBlockISize != NS_UNCONSTRAINEDSIZE,
                   "have unconstrained inline-size; this should only result from "
                   "very large sizes, not attempts at intrinsic inline-size "
                   "calculation");
  NS_PRECONDITION(aContainingBlockISize >= 0,
                  "inline-size less than zero");

  nscoord result;
  if (aCoord.IsCoordPercentCalcUnit()) {
    result = nsRuleNode::ComputeCoordPercentCalc(aCoord, 
                                                 aContainingBlockISize);
    // The result of a calc() expression might be less than 0; we
    // should clamp at runtime (below).  (Percentages and coords that
    // are less than 0 have already been dropped by the parser.)
    result -= aContentEdgeToBoxSizing;
  } else {
    MOZ_ASSERT(eStyleUnit_Enumerated == aCoord.GetUnit());
    // If aFrame is a container for font size inflation, then shrink
    // wrapping inside of it should not apply font size inflation.
    AutoMaybeDisableFontInflation an(aFrame);

    int32_t val = aCoord.GetIntValue();
    switch (val) {
      case NS_STYLE_WIDTH_MAX_CONTENT:
        result = aFrame->GetPrefISize(aRenderingContext);
        NS_ASSERTION(result >= 0, "inline-size less than zero");
        break;
      case NS_STYLE_WIDTH_MIN_CONTENT:
        result = aFrame->GetMinISize(aRenderingContext);
        NS_ASSERTION(result >= 0, "inline-size less than zero");
        break;
      case NS_STYLE_WIDTH_FIT_CONTENT:
        {
          nscoord pref = aFrame->GetPrefISize(aRenderingContext),
                   min = aFrame->GetMinISize(aRenderingContext),
                  fill = aContainingBlockISize -
                         (aBoxSizingToMarginEdge + aContentEdgeToBoxSizing);
          result = std::max(min, std::min(pref, fill));
          NS_ASSERTION(result >= 0, "inline-size less than zero");
        }
        break;
      case NS_STYLE_WIDTH_AVAILABLE:
        result = aContainingBlockISize -
                 (aBoxSizingToMarginEdge + aContentEdgeToBoxSizing);
    }
  }

  return std::max(0, result);
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
  nsAutoTArray<nsIFrame*, 4> subtrees;
  subtrees.AppendElement(aSubtreeRoot);

  // dirty descendants, iterating over subtrees that may include
  // additional subtrees associated with placeholders
  do {
    nsIFrame *subtreeRoot = subtrees.ElementAt(subtrees.Length() - 1);
    subtrees.RemoveElementAt(subtrees.Length() - 1);

    // Mark all descendants dirty (using an nsTArray stack rather than
    // recursion).
    // Note that nsHTMLReflowState::InitResizeFlags has some similar
    // code; see comments there for how and why it differs.
    nsAutoTArray<nsIFrame*, 32> stack;
    stack.AppendElement(subtreeRoot);

    do {
      nsIFrame *f = stack.ElementAt(stack.Length() - 1);
      stack.RemoveElementAt(stack.Length() - 1);

      f->MarkIntrinsicISizesDirty();

      if (f->GetType() == nsGkAtoms::placeholderFrame) {
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
LogicalSize
nsLayoutUtils::ComputeSizeWithIntrinsicDimensions(WritingMode aWM,
                   nsRenderingContext* aRenderingContext, nsIFrame* aFrame,
                   const IntrinsicSize& aIntrinsicSize,
                   nsSize aIntrinsicRatio,
                   const mozilla::LogicalSize& aCBSize,
                   const mozilla::LogicalSize& aMargin,
                   const mozilla::LogicalSize& aBorder,
                   const mozilla::LogicalSize& aPadding)
{
  const nsStylePosition* stylePos = aFrame->StylePosition();

  // If we're a flex item, we'll compute our size a bit differently.
  bool isVertical = aWM.IsVertical();
  const nsStyleCoord* inlineStyleCoord = &stylePos->ISize(aWM);
  const nsStyleCoord* blockStyleCoord = &stylePos->BSize(aWM);

  bool isFlexItem = aFrame->IsFlexItem();
  bool isInlineFlexItem = false;

  if (isFlexItem) {
    // Flex items use their "flex-basis" property in place of their main-size
    // property (e.g. "width") for sizing purposes, *unless* they have
    // "flex-basis:auto", in which case they use their main-size property after
    // all.
    uint32_t flexDirection =
      aFrame->GetParent()->StylePosition()->mFlexDirection;
    isInlineFlexItem =
      flexDirection == NS_STYLE_FLEX_DIRECTION_ROW ||
      flexDirection == NS_STYLE_FLEX_DIRECTION_ROW_REVERSE;

    // NOTE: The logic here should match the similar chunk for determining
    // inlineStyleCoord and blockStyleCoord in nsFrame::ComputeSize().
    const nsStyleCoord* flexBasis = &(stylePos->mFlexBasis);
    if (flexBasis->GetUnit() != eStyleUnit_Auto) {
      if (isInlineFlexItem) {
        inlineStyleCoord = flexBasis;
      } else {
        // One caveat for vertical flex items: We don't support enumerated
        // values (e.g. "max-content") for height properties yet. So, if our
        // computed flex-basis is an enumerated value, we'll just behave as if
        // it were "auto", which means "use the main-size property after all"
        // (which is "height", in this case).
        // NOTE: Once we support intrinsic sizing keywords for "height",
        // we should remove this check.
        if (flexBasis->GetUnit() != eStyleUnit_Enumerated) {
          blockStyleCoord = flexBasis;
        }
      }
    }
  }

  // Handle intrinsic sizes and their interaction with
  // {min-,max-,}{width,height} according to the rules in
  // http://www.w3.org/TR/CSS21/visudet.html#min-max-widths

  // Note: throughout the following section of the function, I avoid
  // a * (b / c) because of its reduced accuracy relative to a * b / c
  // or (a * b) / c (which are equivalent).

  const bool isAutoISize = inlineStyleCoord->GetUnit() == eStyleUnit_Auto;
  const bool isAutoBSize = IsAutoBSize(*blockStyleCoord, aCBSize.BSize(aWM));

  LogicalSize boxSizingAdjust(aWM);
  switch (stylePos->mBoxSizing) {
    case NS_STYLE_BOX_SIZING_BORDER:
      boxSizingAdjust += aBorder;
      // fall through
    case NS_STYLE_BOX_SIZING_PADDING:
      boxSizingAdjust += aPadding;
  }
  nscoord boxSizingToMarginEdgeISize =
    aMargin.ISize(aWM) + aBorder.ISize(aWM) + aPadding.ISize(aWM) -
      boxSizingAdjust.ISize(aWM);

  nscoord iSize, minISize, maxISize, bSize, minBSize, maxBSize;

  if (!isAutoISize) {
    iSize = nsLayoutUtils::ComputeISizeValue(aRenderingContext,
              aFrame, aCBSize.ISize(aWM), boxSizingAdjust.ISize(aWM),
              boxSizingToMarginEdgeISize, *inlineStyleCoord);
  }

  const nsStyleCoord& maxISizeCoord = stylePos->MaxISize(aWM);

  if (maxISizeCoord.GetUnit() != eStyleUnit_None &&
      !(isFlexItem && isInlineFlexItem)) {
    maxISize = nsLayoutUtils::ComputeISizeValue(aRenderingContext,
                 aFrame, aCBSize.ISize(aWM), boxSizingAdjust.ISize(aWM),
                 boxSizingToMarginEdgeISize, maxISizeCoord);
  } else {
    maxISize = nscoord_MAX;
  }

  // NOTE: Flex items ignore their min & max sizing properties in their
  // flex container's main-axis.  (Those properties get applied later in
  // the flexbox algorithm.)

  const nsStyleCoord& minISizeCoord = stylePos->MinISize(aWM);

  if (minISizeCoord.GetUnit() != eStyleUnit_Auto &&
      !(isFlexItem && isInlineFlexItem)) {
    minISize = nsLayoutUtils::ComputeISizeValue(aRenderingContext,
                 aFrame, aCBSize.ISize(aWM), boxSizingAdjust.ISize(aWM),
                 boxSizingToMarginEdgeISize, minISizeCoord);
  } else {
    // Treat "min-width: auto" as 0.
    // NOTE: Technically, "auto" is supposed to behave like "min-content" on
    // flex items. However, we don't need to worry about that here, because
    // flex items' min-sizes are intentionally ignored until the flex
    // container explicitly considers them during space distribution.
    minISize = 0;
  }

  if (!isAutoBSize) {
    bSize = nsLayoutUtils::ComputeBSizeValue(aCBSize.BSize(aWM),
                boxSizingAdjust.BSize(aWM),
                *blockStyleCoord);
  }

  const nsStyleCoord& maxBSizeCoord = stylePos->MaxBSize(aWM);

  if (!IsAutoBSize(maxBSizeCoord, aCBSize.BSize(aWM)) &&
      !(isFlexItem && !isInlineFlexItem)) {
    maxBSize = nsLayoutUtils::ComputeBSizeValue(aCBSize.BSize(aWM),
                  boxSizingAdjust.BSize(aWM), maxBSizeCoord);
  } else {
    maxBSize = nscoord_MAX;
  }

  const nsStyleCoord& minBSizeCoord = stylePos->MinBSize(aWM);

  if (!IsAutoBSize(minBSizeCoord, aCBSize.BSize(aWM)) &&
      !(isFlexItem && !isInlineFlexItem)) {
    minBSize = nsLayoutUtils::ComputeBSizeValue(aCBSize.BSize(aWM),
                  boxSizingAdjust.BSize(aWM), minBSizeCoord);
  } else {
    minBSize = 0;
  }

  // Resolve percentage intrinsic iSize/bSize as necessary:

  NS_ASSERTION(aCBSize.ISize(aWM) != NS_UNCONSTRAINEDSIZE,
               "Our containing block must not have unconstrained inline-size!");

  const nsStyleCoord& isizeCoord =
    isVertical ? aIntrinsicSize.height : aIntrinsicSize.width;
  const nsStyleCoord& bsizeCoord =
    isVertical ? aIntrinsicSize.width : aIntrinsicSize.height;

  bool hasIntrinsicISize, hasIntrinsicBSize;
  nscoord intrinsicISize, intrinsicBSize;

  if (isizeCoord.GetUnit() == eStyleUnit_Coord) {
    hasIntrinsicISize = true;
    intrinsicISize = isizeCoord.GetCoordValue();
    if (intrinsicISize < 0)
      intrinsicISize = 0;
  } else {
    NS_ASSERTION(isizeCoord.GetUnit() == eStyleUnit_None,
                 "unexpected unit");
    hasIntrinsicISize = false;
    intrinsicISize = 0;
  }

  if (bsizeCoord.GetUnit() == eStyleUnit_Coord) {
    hasIntrinsicBSize = true;
    intrinsicBSize = bsizeCoord.GetCoordValue();
    if (intrinsicBSize < 0)
      intrinsicBSize = 0;
  } else {
    NS_ASSERTION(bsizeCoord.GetUnit() == eStyleUnit_None,
                 "unexpected unit");
    hasIntrinsicBSize = false;
    intrinsicBSize = 0;
  }

  NS_ASSERTION(aIntrinsicRatio.width >= 0 && aIntrinsicRatio.height >= 0,
               "Intrinsic ratio has a negative component!");
  LogicalSize logicalRatio(aWM, aIntrinsicRatio);

  // Now calculate the used values for iSize and bSize:

  if (isAutoISize) {
    if (isAutoBSize) {

      // 'auto' iSize, 'auto' bSize

      // Get tentative values - CSS 2.1 sections 10.3.2 and 10.6.2:

      nscoord tentISize, tentBSize;

      if (hasIntrinsicISize) {
        tentISize = intrinsicISize;
      } else if (hasIntrinsicBSize && logicalRatio.BSize(aWM) > 0) {
        tentISize = NSCoordMulDiv(intrinsicBSize, logicalRatio.ISize(aWM), logicalRatio.BSize(aWM));
      } else if (logicalRatio.ISize(aWM) > 0) {
        tentISize = aCBSize.ISize(aWM) - boxSizingToMarginEdgeISize; // XXX scrollbar?
        if (tentISize < 0) tentISize = 0;
      } else {
        tentISize = nsPresContext::CSSPixelsToAppUnits(300);
      }

      if (hasIntrinsicBSize) {
        tentBSize = intrinsicBSize;
      } else if (logicalRatio.ISize(aWM) > 0) {
        tentBSize = NSCoordMulDiv(tentISize, logicalRatio.BSize(aWM), logicalRatio.ISize(aWM));
      } else {
        tentBSize = nsPresContext::CSSPixelsToAppUnits(150);
      }

      if (aIntrinsicRatio != nsSize(0, 0)) {
        nsSize autoSize =
          ComputeAutoSizeWithIntrinsicDimensions(minISize, minBSize,
                                                 maxISize, maxBSize,
                                                 tentISize, tentBSize);
        // The nsSize that ComputeAutoSizeWithIntrinsicDimensions returns will
        // actually contain logical values if the parameters passed to it were
        // logical coordinates, so we do NOT perform a physical-to-logical
        // conversion here, but just assign the fields directly to our result.
        iSize = autoSize.width;
        bSize = autoSize.height;
      } else {
        // No intrinsic ratio, so just clamp the dimensions
        // independently without calling
        // ComputeAutoSizeWithIntrinsicDimensions, which deals with
        // propagating these changes to the other dimension (and would
        // be incorrect when there is no intrinsic ratio).
        iSize = NS_CSS_MINMAX(tentISize, minISize, maxISize);
        bSize = NS_CSS_MINMAX(tentBSize, minBSize, maxBSize);
      }
    } else {

      // 'auto' iSize, non-'auto' bSize
      bSize = NS_CSS_MINMAX(bSize, minBSize, maxBSize);
      if (logicalRatio.BSize(aWM) > 0) {
        iSize = NSCoordMulDiv(bSize, logicalRatio.ISize(aWM), logicalRatio.BSize(aWM));
      } else if (hasIntrinsicISize) {
        iSize = intrinsicISize;
      } else {
        iSize = nsPresContext::CSSPixelsToAppUnits(300);
      }
      iSize = NS_CSS_MINMAX(iSize, minISize, maxISize);

    }
  } else {
    if (isAutoBSize) {

      // non-'auto' iSize, 'auto' bSize
      iSize = NS_CSS_MINMAX(iSize, minISize, maxISize);
      if (logicalRatio.ISize(aWM) > 0) {
        bSize = NSCoordMulDiv(iSize, logicalRatio.BSize(aWM), logicalRatio.ISize(aWM));
      } else if (hasIntrinsicBSize) {
        bSize = intrinsicBSize;
      } else {
        bSize = nsPresContext::CSSPixelsToAppUnits(150);
      }
      bSize = NS_CSS_MINMAX(bSize, minBSize, maxBSize);

    } else {

      // non-'auto' iSize, non-'auto' bSize
      iSize = NS_CSS_MINMAX(iSize, minISize, maxISize);
      bSize = NS_CSS_MINMAX(bSize, minBSize, maxBSize);

    }
  }

  return LogicalSize(aWM, iSize, bSize);
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
                                  nsRenderingContext* aRenderingContext)
{
  NS_ASSERTION(!aFrame->IsContainerForFontSizeInflation(),
               "should not be container for font size inflation");

  nsIFrame::InlineMinISizeData data;
  DISPLAY_MIN_WIDTH(aFrame, data.prevLines);
  aFrame->AddInlineMinISize(aRenderingContext, &data);
  data.ForceBreak(aRenderingContext);
  return data.prevLines;
}

/* static */ nscoord
nsLayoutUtils::PrefISizeFromInline(nsIFrame* aFrame,
                                   nsRenderingContext* aRenderingContext)
{
  NS_ASSERTION(!aFrame->IsContainerForFontSizeInflation(),
               "should not be container for font size inflation");

  nsIFrame::InlinePrefISizeData data;
  DISPLAY_PREF_WIDTH(aFrame, data.prevLines);
  aFrame->AddInlinePrefISize(aRenderingContext, &data);
  data.ForceBreak(aRenderingContext);
  return data.prevLines;
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
nsLayoutUtils::GetColor(nsIFrame* aFrame, nsCSSProperty aProperty)
{
  nscolor color = aFrame->GetVisitedDependentColor(aProperty);
  if (ShouldDarkenColors(aFrame->PresContext())) {
    color = DarkenColor(color);
  }
  return color;
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
                                    nsRenderingContext& aContext)
{
  uint32_t maxChunkLength = GetMaxChunkLength(aFontMetrics);
  nscoord width = 0;
  while (aLength > 0) {
    int32_t len = FindSafeLength(aString, aLength, maxChunkLength);
    width += aFontMetrics.GetWidth(aString, len, &aContext);
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
                                        nsRenderingContext& aContext)
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
                                             aContext);
}

bool
nsLayoutUtils::StringWidthIsGreaterThan(const nsString& aString,
                                        nsFontMetrics& aFontMetrics,
                                        nsRenderingContext& aContext,
                                        nscoord aWidth)
{
  const char16_t *string = aString.get();
  uint32_t length = aString.Length();
  uint32_t maxChunkLength = GetMaxChunkLength(aFontMetrics);
  nscoord width = 0;
  while (length > 0) {
    int32_t len = FindSafeLength(string, length, maxChunkLength);
    width += aFontMetrics.GetWidth(string, len, &aContext);
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
                                     nsRenderingContext& aContext)
{
  uint32_t maxChunkLength = GetMaxChunkLength(aFontMetrics);
  int32_t len = FindSafeLength(aString, aLength, maxChunkLength);
  // Assign directly in the first iteration. This ensures that
  // negative ascent/descent can be returned and the left bearing
  // is properly initialized.
  nsBoundingMetrics totalMetrics =
    aFontMetrics.GetBoundingMetrics(aString, len, &aContext);
  aLength -= len;
  aString += len;

  while (aLength > 0) {
    len = FindSafeLength(aString, aLength, maxChunkLength);
    nsBoundingMetrics metrics =
      aFontMetrics.GetBoundingMetrics(aString, len, &aContext);
    totalMetrics += metrics;
    aLength -= len;
    aString += len;
  }
  return totalMetrics;
}

void
nsLayoutUtils::DrawString(const nsIFrame*       aFrame,
                          nsFontMetrics&        aFontMetrics,
                          nsRenderingContext*   aContext,
                          const char16_t*      aString,
                          int32_t               aLength,
                          nsPoint               aPoint,
                          nsStyleContext*       aStyleContext)
{
  nsresult rv = NS_ERROR_FAILURE;

  // If caller didn't pass a style context, use the frame's.
  if (!aStyleContext) {
    aStyleContext = aFrame->StyleContext();
  }
  aFontMetrics.SetVertical(WritingMode(aStyleContext).IsVertical());
  aFontMetrics.SetTextOrientation(
    aStyleContext->StyleVisibility()->mTextOrientation);

  nsPresContext* presContext = aFrame->PresContext();
  if (presContext->BidiEnabled()) {
    nsBidiLevel level =
      nsBidiPresUtils::BidiLevelFromStyle(aStyleContext);
    rv = nsBidiPresUtils::RenderText(aString, aLength, level,
                                     presContext, *aContext, *aContext,
                                     aFontMetrics,
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
                                nsRenderingContext& aContext)
{
  nscoord x = aPoint.x;
  nscoord y = aPoint.y;

  uint32_t maxChunkLength = GetMaxChunkLength(aFontMetrics);
  if (aLength <= maxChunkLength) {
    aFontMetrics.DrawString(aString, aLength, x, y, &aContext, &aContext);
    return;
  }

  bool isRTL = aFontMetrics.GetTextRunRTL();

  // If we're drawing right to left, we must start at the end.
  if (isRTL) {
    x += nsLayoutUtils::AppUnitWidthOfString(aString, aLength, aFontMetrics,
                                             aContext);
  }

  while (aLength > 0) {
    int32_t len = FindSafeLength(aString, aLength, maxChunkLength);
    nscoord width = aFontMetrics.GetWidth(aString, len, &aContext);
    if (isRTL) {
      x -= width;
    }
    aFontMetrics.DrawString(aString, len, x, y, &aContext, &aContext);
    if (!isRTL) {
      x += width;
    }
    aLength -= len;
    aString += len;
  }
}

/* static */ void
nsLayoutUtils::PaintTextShadow(const nsIFrame* aFrame,
                               nsRenderingContext* aContext,
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
  gfxContext* aDestCtx = aContext->ThebesContext();
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

    // Conjure an nsRenderingContext from a gfxContext for drawing the text
    // to blur.
    nsRenderingContext renderingContext(shadowContext);

    aDestCtx->Save();
    aDestCtx->NewPath();
    aDestCtx->SetColor(gfxRGBA(shadowColor));

    // The callback will draw whatever we want to blur as a shadow.
    aCallback(&renderingContext, shadowOffset, shadowColor, aCallbackData);

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
    nsIAtom* fType = aFrame->GetType();
    if (fType == nsGkAtoms::tableOuterFrame) {
      aResult->mBStart = 0;
      aResult->mBaseline = aFrame->GetLogicalBaseline(aWM);
      // This is what we want for the list bullet caller; not sure if
      // other future callers will want the same.
      aResult->mBEnd = aFrame->BSize(aWM);
      return true;
    }

    // For first-line baselines, we have to consider scroll frames.
    if (fType == nsGkAtoms::scrollFrame) {
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

    if (fType == nsGkAtoms::fieldSetFrame) {
      LinePosition kidPosition;
      nsIFrame* kid = aFrame->GetFirstPrincipalChild();
      // kid might be a legend frame here, but that's ok.
      if (GetFirstLinePosition(aWM, kid, &kidPosition)) {
        *aResult = kidPosition +
          kid->GetLogicalNormalPosition(aWM, aFrame->GetSize().width).B(aWM);
        return true;
      }
      return false;
    }

    // No baseline.
    return false;
  }

  for (nsBlockFrame::const_line_iterator line = block->begin_lines(),
                                     line_end = block->end_lines();
       line != line_end; ++line) {
    if (line->IsBlock()) {
      nsIFrame *kid = line->mFirstChild;
      LinePosition kidPosition;
      if (GetFirstLinePosition(aWM, kid, &kidPosition)) {
        //XXX Not sure if this is the correct value to use for container
        //    width here. It will only be used in vertical-rl layout,
        //    which we don't have full support and testing for yet.
        nscoord containerWidth = line->mContainerWidth;
        *aResult = kidPosition +
                   kid->GetLogicalNormalPosition(aWM, containerWidth).B(aWM);
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

  for (nsBlockFrame::const_reverse_line_iterator line = block->rbegin_lines(),
                                             line_end = block->rend_lines();
       line != line_end; ++line) {
    if (line->IsBlock()) {
      nsIFrame *kid = line->mFirstChild;
      nscoord kidBaseline;
      nscoord containerWidth = line->mContainerWidth;
      if (GetLastLineBaseline(aWM, kid, &kidBaseline)) {
        // Ignore relative positioning for baseline calculations
        *aResult = kidBaseline +
          kid->GetLogicalNormalPosition(aWM, containerWidth).B(aWM);
        return true;
      } else if (kid->GetType() == nsGkAtoms::scrollFrame) {
        // Use the bottom of the scroll frame.
        // XXX CSS2.1 really doesn't say what to do here.
        *aResult = kid->GetLogicalNormalPosition(aWM, containerWidth).B(aWM) +
                   kid->BSize(aWM);
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

  for (nsBlockFrame::line_iterator line = aFrame->begin_lines(),
                                   line_end = aFrame->end_lines();
       line != line_end; ++line) {
    if (line->IsBlock()) {
      nsIFrame* child = line->mFirstChild;
      nscoord containerWidth = line->mContainerWidth;
      nscoord offset =
        child->GetLogicalNormalPosition(aWM, containerWidth).B(aWM);
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
                                            aFrame->GetSize().width).B(aWM);
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
    if (layer->IsAbsPosContaininingBlock() ||
        (layer->GetParent() &&
          layer->GetParent()->GetType() == nsGkAtoms::scrollFrame))
      break;
  }
  if (layer)
    return layer;
  return aFrame->PresContext()->PresShell()->FrameManager()->GetRootFrame();
}

GraphicsFilter
nsLayoutUtils::GetGraphicsFilterForFrame(nsIFrame* aForFrame)
{
  GraphicsFilter defaultFilter = GraphicsFilter::FILTER_GOOD;
  nsStyleContext *sc;
  if (nsCSSRendering::IsCanvasFrame(aForFrame)) {
    nsCSSRendering::FindBackground(aForFrame, &sc);
  } else {
    sc = aForFrame->StyleContext();
  }

  switch (sc->StyleSVG()->mImageRendering) {
  case NS_STYLE_IMAGE_RENDERING_OPTIMIZESPEED:
    return GraphicsFilter::FILTER_FAST;
  case NS_STYLE_IMAGE_RENDERING_OPTIMIZEQUALITY:
    return GraphicsFilter::FILTER_BEST;
  case NS_STYLE_IMAGE_RENDERING_CRISPEDGES:
    return GraphicsFilter::FILTER_NEAREST;
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
                                     GraphicsFilter  aGraphicsFilter,
                                     uint32_t        aImageFlags)
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
                                    aGraphicsFilter, aImageFlags);
  gfxSize imageSize(intImageSize.width, intImageSize.height);

  // XXX(seth): May be buggy; see bug 1151016.
  CSSIntSize svgViewportSize = currentMatrix.IsIdentity()
    ? CSSIntSize(intImageSize.width, intImageSize.height)
    : CSSIntSize(devPixelDest.width, devPixelDest.height);

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

  ImageRegion region =
    ImageRegion::CreateWithSamplingRestriction(imageSpaceFill, subimage);

  return SnappedImageDrawingParameters(transform, intImageSize,
                                       region, svgViewportSize);
}


static DrawResult
DrawImageInternal(gfxContext&            aContext,
                  nsPresContext*         aPresContext,
                  imgIContainer*         aImage,
                  GraphicsFilter         aGraphicsFilter,
                  const nsRect&          aDest,
                  const nsRect&          aFill,
                  const nsPoint&         aAnchor,
                  const nsRect&          aDirty,
                  const SVGImageContext* aSVGContext,
                  uint32_t               aImageFlags)
{
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
                                         aGraphicsFilter, aImageFlags);

  if (!params.shouldDraw) {
    return DrawResult::SUCCESS;
  }

  gfxContextMatrixAutoSaveRestore contextMatrixRestorer(&aContext);
  aContext.SetMatrix(params.imageSpaceToDeviceSpace);

  Maybe<SVGImageContext> svgContext = ToMaybe(aSVGContext);
  if (!svgContext) {
    // Use the default viewport.
    svgContext = Some(SVGImageContext(params.svgViewportSize, Nothing()));
  }

  return aImage->Draw(&aContext, params.size, params.region,
                      imgIContainer::FRAME_CURRENT, aGraphicsFilter,
                      svgContext, aImageFlags);
}

/* static */ DrawResult
nsLayoutUtils::DrawSingleUnscaledImage(gfxContext&          aContext,
                                       nsPresContext*       aPresContext,
                                       imgIContainer*       aImage,
                                       GraphicsFilter       aGraphicsFilter,
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
                           aImage, aGraphicsFilter,
                           dest, fill, aDest, aDirty ? *aDirty : dest,
                           nullptr, aImageFlags);
}

/* static */ DrawResult
nsLayoutUtils::DrawSingleImage(gfxContext&            aContext,
                               nsPresContext*         aPresContext,
                               imgIContainer*         aImage,
                               GraphicsFilter         aGraphicsFilter,
                               const nsRect&          aDest,
                               const nsRect&          aDirty,
                               const SVGImageContext* aSVGContext,
                               uint32_t               aImageFlags,
                               const nsPoint*         aAnchorPoint,
                               const nsRect*          aSourceArea)
{
  nscoord appUnitsPerCSSPixel = nsDeviceContext::AppUnitsPerCSSPixel();
  CSSIntSize pixelImageSize(ComputeSizeForDrawingWithFallback(aImage, aDest.Size()));
  if (pixelImageSize.width < 1 || pixelImageSize.height < 1) {
    NS_WARNING("Image width or height is non-positive");
    return DrawResult::TEMPORARY_ERROR;
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
                           aGraphicsFilter, dest, fill,
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

/* static */ DrawResult
nsLayoutUtils::DrawBackgroundImage(gfxContext&         aContext,
                                   nsPresContext*      aPresContext,
                                   imgIContainer*      aImage,
                                   const CSSIntSize&   aImageSize,
                                   GraphicsFilter      aGraphicsFilter,
                                   const nsRect&       aDest,
                                   const nsRect&       aFill,
                                   const nsPoint&      aAnchor,
                                   const nsRect&       aDirty,
                                   uint32_t            aImageFlags)
{
  PROFILER_LABEL("layout", "nsLayoutUtils::DrawBackgroundImage",
                 js::ProfileEntry::Category::GRAPHICS);

  if (UseBackgroundNearestFiltering()) {
    aGraphicsFilter = GraphicsFilter::FILTER_NEAREST;
  }

  SVGImageContext svgContext(aImageSize, Nothing());

  return DrawImageInternal(aContext, aPresContext, aImage,
                           aGraphicsFilter, aDest, aFill, aAnchor,
                           aDirty, &svgContext, aImageFlags);
}

/* static */ DrawResult
nsLayoutUtils::DrawImage(gfxContext&         aContext,
                         nsPresContext*      aPresContext,
                         imgIContainer*      aImage,
                         GraphicsFilter      aGraphicsFilter,
                         const nsRect&       aDest,
                         const nsRect&       aFill,
                         const nsPoint&      aAnchor,
                         const nsRect&       aDirty,
                         uint32_t            aImageFlags)
{
  return DrawImageInternal(aContext, aPresContext, aImage,
                           aGraphicsFilter, aDest, aFill, aAnchor,
                           aDirty, nullptr, aImageFlags);
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

// aCorner is a "full corner" value, i.e. NS_CORNER_TOP_LEFT etc
static bool IsCornerAdjacentToSide(uint8_t aCorner, css::Side aSide)
{
  PR_STATIC_ASSERT((int)NS_SIDE_TOP == NS_CORNER_TOP_LEFT);
  PR_STATIC_ASSERT((int)NS_SIDE_RIGHT == NS_CORNER_TOP_RIGHT);
  PR_STATIC_ASSERT((int)NS_SIDE_BOTTOM == NS_CORNER_BOTTOM_RIGHT);
  PR_STATIC_ASSERT((int)NS_SIDE_LEFT == NS_CORNER_BOTTOM_LEFT);
  PR_STATIC_ASSERT((int)NS_SIDE_TOP == ((NS_CORNER_TOP_RIGHT - 1)&3));
  PR_STATIC_ASSERT((int)NS_SIDE_RIGHT == ((NS_CORNER_BOTTOM_RIGHT - 1)&3));
  PR_STATIC_ASSERT((int)NS_SIDE_BOTTOM == ((NS_CORNER_BOTTOM_LEFT - 1)&3));
  PR_STATIC_ASSERT((int)NS_SIDE_LEFT == ((NS_CORNER_TOP_LEFT - 1)&3));

  return aSide == aCorner || aSide == ((aCorner - 1)&3);
}

/* static */ bool
nsLayoutUtils::HasNonZeroCornerOnSide(const nsStyleCorners& aCorners,
                                      css::Side aSide)
{
  PR_STATIC_ASSERT(NS_CORNER_TOP_LEFT_X/2 == NS_CORNER_TOP_LEFT);
  PR_STATIC_ASSERT(NS_CORNER_TOP_LEFT_Y/2 == NS_CORNER_TOP_LEFT);
  PR_STATIC_ASSERT(NS_CORNER_TOP_RIGHT_X/2 == NS_CORNER_TOP_RIGHT);
  PR_STATIC_ASSERT(NS_CORNER_TOP_RIGHT_Y/2 == NS_CORNER_TOP_RIGHT);
  PR_STATIC_ASSERT(NS_CORNER_BOTTOM_RIGHT_X/2 == NS_CORNER_BOTTOM_RIGHT);
  PR_STATIC_ASSERT(NS_CORNER_BOTTOM_RIGHT_Y/2 == NS_CORNER_BOTTOM_RIGHT);
  PR_STATIC_ASSERT(NS_CORNER_BOTTOM_LEFT_X/2 == NS_CORNER_BOTTOM_LEFT);
  PR_STATIC_ASSERT(NS_CORNER_BOTTOM_LEFT_Y/2 == NS_CORNER_BOTTOM_LEFT);

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
  if (aCSSRootFrame->StyleDisplay()->mOpacity < 1.0f)
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
  if (aBackgroundFrame->GetType() == nsGkAtoms::viewportFrame &&
      !aBackgroundFrame->GetFirstPrincipalChild()) {
    return eTransparencyOpaque;
  }

  nsStyleContext* bgSC;
  if (!nsCSSRendering::FindBackground(aBackgroundFrame, &bgSC)) {
    return eTransparencyTransparent;
  }
  const nsStyleBackground* bg = bgSC->StyleBackground();
  if (NS_GET_A(bg->mBackgroundColor) < 255 ||
      // bottom layer's clip is used for the color
      bg->BottomLayer().mClip != NS_STYLE_BG_CLIP_BORDER)
    return eTransparencyTransparent;
  return eTransparencyOpaque;
}

static bool IsPopupFrame(nsIFrame* aFrame)
{
  // aFrame is a popup it's the list control frame dropdown for a combobox.
  nsIAtom* frameType = aFrame->GetType();
  if (frameType == nsGkAtoms::listControlFrame) {
    nsListControlFrame* lcf = static_cast<nsListControlFrame*>(aFrame);
    return lcf->IsInDropDownMode();
  }

  // ... or if it's a XUL menupopup frame.
  return frameType == nsGkAtoms::menuPopupFrame;
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
    if (f->IsTransformed() || IsPopup(f)) {
      return f;
    }
    nsIFrame* parent = GetCrossDocParentFrame(f);
    if (!parent) {
      return f;
    }
    f = parent;
  }
}

/* static */ nsIFrame*
nsLayoutUtils::GetTransformRootFrame(nsIFrame* aFrame)
{
  nsIFrame *parent = nsLayoutUtils::GetCrossDocParentFrame(aFrame);
  while (parent && parent->Preserves3DChildren()) {
    parent = nsLayoutUtils::GetCrossDocParentFrame(parent);
  }
  return parent;
}

/* static */ uint32_t
nsLayoutUtils::GetTextRunFlagsForStyle(nsStyleContext* aStyleContext,
                                       const nsStyleFont* aStyleFont,
                                       const nsStyleText* aStyleText,
                                       nscoord aLetterSpacing)
{
  uint32_t result = 0;
  if (aLetterSpacing != 0) {
    result |= gfxTextRunFactory::TEXT_DISABLE_OPTIONAL_LIGATURES;
  }
  if (aStyleText->mControlCharacterVisibility == NS_STYLE_CONTROL_CHARACTER_VISIBILITY_HIDDEN) {
    result |= gfxTextRunFactory::TEXT_HIDE_CONTROL_CHARACTERS;
  }
  switch (aStyleContext->StyleSVG()->mTextRendering) {
  case NS_STYLE_TEXT_RENDERING_OPTIMIZESPEED:
    result |= gfxTextRunFactory::TEXT_OPTIMIZE_SPEED;
    break;
  case NS_STYLE_TEXT_RENDERING_AUTO:
    if (aStyleFont->mFont.size <
        aStyleContext->PresContext()->GetAutoQualityMinFontSize()) {
      result |= gfxTextRunFactory::TEXT_OPTIMIZE_SPEED;
    }
    break;
  default:
    break;
  }
  return result | GetTextRunOrientFlagsForStyle(aStyleContext);
}

/* static */ uint32_t
nsLayoutUtils::GetTextRunOrientFlagsForStyle(nsStyleContext* aStyleContext)
{
  WritingMode wm(aStyleContext);
  if (wm.IsVertical()) {
    switch (aStyleContext->StyleVisibility()->mTextOrientation) {
    case NS_STYLE_TEXT_ORIENTATION_MIXED:
      return gfxTextRunFactory::TEXT_ORIENT_VERTICAL_MIXED;
    case NS_STYLE_TEXT_ORIENTATION_UPRIGHT:
      return gfxTextRunFactory::TEXT_ORIENT_VERTICAL_UPRIGHT;
    case NS_STYLE_TEXT_ORIENTATION_SIDEWAYS:
      // This should depend on writing mode vertical-lr vs vertical-rl,
      // but until we support SIDEWAYS_LEFT, we'll treat this the same
      // as SIDEWAYS_RIGHT and simply fall through.
      /*
      if (wm.IsVerticalLR()) {
        return gfxTextRunFactory::TEXT_ORIENT_VERTICAL_SIDEWAYS_LEFT;
      } else {
        return gfxTextRunFactory::TEXT_ORIENT_VERTICAL_SIDEWAYS_RIGHT;
      }
      */
    case NS_STYLE_TEXT_ORIENTATION_SIDEWAYS_RIGHT:
      return gfxTextRunFactory::TEXT_ORIENT_VERTICAL_SIDEWAYS_RIGHT;
    case NS_STYLE_TEXT_ORIENTATION_SIDEWAYS_LEFT:
      // Not yet supported, so fall through to the default (error) case.
      /*
      return gfxTextRunFactory::TEXT_ORIENT_VERTICAL_SIDEWAYS_LEFT;
      */
    default:
      NS_NOTREACHED("unknown text-orientation");
    }
  }
  return 0;
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
nsLayoutUtils::GetDeviceContextForScreenInfo(nsPIDOMWindow* aWindow)
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
    nsCOMPtr<nsPIDOMWindow> win = docShell->GetWindow();
    if (!win) {
      // No reason to go on
      return nullptr;
    }

    win->EnsureSizeUpToDate();

    nsRefPtr<nsPresContext> presContext;
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

  nsIAtom *parentType = aFrame->GetParent()->GetType();
  return parentType == nsGkAtoms::viewportFrame ||
         parentType == nsGkAtoms::pageContentFrame;
}

nsLayoutUtils::SurfaceFromElementResult
nsLayoutUtils::SurfaceFromElement(nsIImageLoadingContent* aElement,
                                  uint32_t aSurfaceFlags,
                                  DrawTarget* aTarget)
{
  SurfaceFromElementResult result;
  nsresult rv;

  nsCOMPtr<imgIRequest> imgRequest;
  rv = aElement->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                            getter_AddRefs(imgRequest));
  if (NS_FAILED(rv) || !imgRequest)
    return result;

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
  if (NS_FAILED(rv))
    return result;

  nsCOMPtr<imgIContainer> imgContainer;
  rv = imgRequest->GetImage(getter_AddRefs(imgContainer));
  if (NS_FAILED(rv))
    return result;

  uint32_t noRasterize = aSurfaceFlags & SFE_NO_RASTERIZING_VECTORS;

  uint32_t whichFrame = (aSurfaceFlags & SFE_WANT_FIRST_FRAME)
                        ? (uint32_t) imgIContainer::FRAME_FIRST
                        : (uint32_t) imgIContainer::FRAME_CURRENT;
  uint32_t frameFlags = imgIContainer::FLAG_SYNC_DECODE;
  if (aSurfaceFlags & SFE_NO_COLORSPACE_CONVERSION)
    frameFlags |= imgIContainer::FLAG_DECODE_NO_COLORSPACE_CONVERSION;
  if (aSurfaceFlags & SFE_PREFER_NO_PREMULTIPLY_ALPHA) {
    frameFlags |= imgIContainer::FLAG_DECODE_NO_PREMULTIPLY_ALPHA;
    result.mIsPremultiplied = false;
  }

  int32_t imgWidth, imgHeight;
  rv = imgContainer->GetWidth(&imgWidth);
  nsresult rv2 = imgContainer->GetHeight(&imgHeight);
  if (NS_FAILED(rv) || NS_FAILED(rv2))
    return result;

  if (!noRasterize || imgContainer->GetType() == imgIContainer::TYPE_RASTER) {
    if (aSurfaceFlags & SFE_WANT_IMAGE_SURFACE) {
      frameFlags |= imgIContainer::FLAG_WANT_DATA_SURFACE;
    }
    result.mSourceSurface = imgContainer->GetFrame(whichFrame, frameFlags);
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
  } else {
    result.mDrawInfo.mImgContainer = imgContainer;
    result.mDrawInfo.mWhichFrame = whichFrame;
    result.mDrawInfo.mDrawingFlags = frameFlags;
  }

  int32_t corsmode;
  if (NS_SUCCEEDED(imgRequest->GetCORSMode(&corsmode))) {
    result.mCORSUsed = (corsmode != imgIRequest::CORS_NONE);
  }

  result.mSize = gfxIntSize(imgWidth, imgHeight);
  result.mPrincipal = principal.forget();
  // no images, including SVG images, can load content from another domain.
  result.mIsWriteOnly = false;
  result.mImageRequest = imgRequest.forget();

  return result;
}

nsLayoutUtils::SurfaceFromElementResult
nsLayoutUtils::SurfaceFromElement(HTMLImageElement *aElement,
                                  uint32_t aSurfaceFlags,
                                  DrawTarget* aTarget)
{
  return SurfaceFromElement(static_cast<nsIImageLoadingContent*>(aElement),
                            aSurfaceFlags, aTarget);
}

nsLayoutUtils::SurfaceFromElementResult
nsLayoutUtils::SurfaceFromElement(HTMLCanvasElement* aElement,
                                  uint32_t aSurfaceFlags,
                                  DrawTarget* aTarget)
{
  SurfaceFromElementResult result;

  bool* isPremultiplied = nullptr;
  if (aSurfaceFlags & SFE_PREFER_NO_PREMULTIPLY_ALPHA) {
    isPremultiplied = &result.mIsPremultiplied;
  }

  gfxIntSize size = aElement->GetSize();

  result.mSourceSurface = aElement->GetSurfaceSnapshot(isPremultiplied);
  if (!result.mSourceSurface) {
     // If the element doesn't have a context then we won't get a snapshot. The canvas spec wants us to not error and just
     // draw nothing, so return an empty surface.
     DrawTarget *ref = aTarget ? aTarget : gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget();
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
                                  DrawTarget* aTarget)
{
  SurfaceFromElementResult result;

  NS_WARN_IF_FALSE((aSurfaceFlags & SFE_PREFER_NO_PREMULTIPLY_ALPHA) == 0, "We can't support non-premultiplied alpha for video!");

#ifdef MOZ_EME
  if (aElement->ContainsRestrictedContent()) {
    return result;
  }
#endif

  uint16_t readyState;
  if (NS_SUCCEEDED(aElement->GetReadyState(&readyState)) &&
      (readyState == nsIDOMHTMLMediaElement::HAVE_NOTHING ||
       readyState == nsIDOMHTMLMediaElement::HAVE_METADATA)) {
    result.mIsStillLoading = true;
    return result;
  }

  // If it doesn't have a principal, just bail
  nsCOMPtr<nsIPrincipal> principal = aElement->GetCurrentPrincipal();
  if (!principal)
    return result;

  ImageContainer *container = aElement->GetImageContainer();
  if (!container)
    return result;

  AutoLockImage lockImage(container);
  layers::Image* image = lockImage.GetImage();
  if (!image) {
    return result;
  }
  result.mSourceSurface = image->GetAsSourceSurface();
  if (!result.mSourceSurface)
    return result;

  if (aTarget) {
    RefPtr<SourceSurface> opt = aTarget->OptimizeSourceSurface(result.mSourceSurface);
    if (opt) {
      result.mSourceSurface = opt;
    }
  }

  result.mCORSUsed = aElement->GetCORSMode() != CORS_NONE;
  result.mHasSize = true;
  result.mSize = image->GetSize();
  result.mPrincipal = principal.forget();
  result.mIsWriteOnly = false;

  return result;
}

nsLayoutUtils::SurfaceFromElementResult
nsLayoutUtils::SurfaceFromElement(dom::Element* aElement,
                                  uint32_t aSurfaceFlags,
                                  DrawTarget* aTarget)
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
  //       additional work for some cases and nsEditor uses them.
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
    if (f->GetType() == nsGkAtoms::letterFrame) {
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

  if (aFrame->GetType() == nsGkAtoms::textFrame) {
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

  if (aFrame->GetType() != nsGkAtoms::textFrame) {
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

  if (aFrame->GetType() == nsGkAtoms::textFrame) {
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

  nsAutoTArray<nsIFrame::ChildList,4> childListArray;
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
  Preferences::AddBoolVarCache(&sInvalidationDebuggingIsEnabled,
                               "nglayout.debug.invalidation");
  Preferences::AddBoolVarCache(&sCSSVariablesEnabled,
                               "layout.css.variables.enabled");
  Preferences::AddBoolVarCache(&sInterruptibleReflowEnabled,
                               "layout.interruptible-reflow.enabled");
  Preferences::AddBoolVarCache(&sSVGTransformOriginEnabled,
                               "svg.transform-origin.enabled");

  Preferences::RegisterCallback(GridEnabledPrefChangeCallback,
                                GRID_ENABLED_PREF_NAME);
  GridEnabledPrefChangeCallback(GRID_ENABLED_PREF_NAME, nullptr);
  Preferences::RegisterCallback(RubyEnabledPrefChangeCallback,
                                RUBY_ENABLED_PREF_NAME);
  RubyEnabledPrefChangeCallback(RUBY_ENABLED_PREF_NAME, nullptr);
  Preferences::RegisterCallback(StickyEnabledPrefChangeCallback,
                                STICKY_ENABLED_PREF_NAME);
  StickyEnabledPrefChangeCallback(STICKY_ENABLED_PREF_NAME, nullptr);
  Preferences::RegisterCallback(TextAlignTrueEnabledPrefChangeCallback,
                                TEXT_ALIGN_TRUE_ENABLED_PREF_NAME);
  Preferences::RegisterCallback(DisplayContentsEnabledPrefChangeCallback,
                                DISPLAY_CONTENTS_ENABLED_PREF_NAME);
  DisplayContentsEnabledPrefChangeCallback(DISPLAY_CONTENTS_ENABLED_PREF_NAME,
                                           nullptr);
  TextAlignTrueEnabledPrefChangeCallback(TEXT_ALIGN_TRUE_ENABLED_PREF_NAME,
                                         nullptr);

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

  Preferences::UnregisterCallback(GridEnabledPrefChangeCallback,
                                  GRID_ENABLED_PREF_NAME);
  Preferences::UnregisterCallback(RubyEnabledPrefChangeCallback,
                                  RUBY_ENABLED_PREF_NAME);
  Preferences::UnregisterCallback(StickyEnabledPrefChangeCallback,
                                  STICKY_ENABLED_PREF_NAME);

  nsComputedDOMStyle::UnregisterPrefChangeCallbacks();
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
    nsIAtom* fType = f->GetType();
    nsIFrame* parent = f->GetParent();
    // Also, if there is more than one frame corresponding to a single
    // content node, we want the outermost one.
    if (!(parent && parent->GetContent() == content) &&
        // ignore width/height on inlines since they don't apply
        fType != nsGkAtoms::inlineFrame &&
        // ignore width on radios and checkboxes since we enlarge them and
        // they have width/height in ua.css
        fType != nsGkAtoms::formControlFrame) {
      // ruby annotations should have the same inflation as its
      // grandparent, which is the ruby frame contains the annotation.
      if (fType == nsGkAtoms::rubyTextFrame) {
        MOZ_ASSERT(parent &&
                   parent->GetType() == nsGkAtoms::rubyTextContainerFrame);
        nsIFrame* grandparent = parent->GetParent();
        MOZ_ASSERT(grandparent &&
                   grandparent->GetType() == nsGkAtoms::rubyFrame);
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
  if (aFrame->IsSVGText()) {
    const nsIFrame* container = aFrame;
    while (container->GetType() != nsGkAtoms::svgTextFrame) {
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
  nsCSSShadowArray* boxShadows = aFrame->StyleBorder()->mBoxShadow;
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

/* static */ void
nsLayoutUtils::UpdateImageVisibilityForFrame(nsIFrame* aImageFrame)
{
#ifdef DEBUG
  nsIAtom* type = aImageFrame->GetType();
  MOZ_ASSERT(type == nsGkAtoms::imageFrame ||
             type == nsGkAtoms::imageControlFrame ||
             type == nsGkAtoms::svgImageFrame, "wrong type of frame");
#endif

  nsCOMPtr<nsIImageLoadingContent> content = do_QueryInterface(aImageFrame->GetContent());
  if (!content) {
    return;
  }

  nsIPresShell* presShell = aImageFrame->PresContext()->PresShell();
  if (presShell->AssumeAllImagesVisible()) {
    presShell->EnsureImageInVisibleList(content);
    return;
  }

  bool visible = true;
  nsIFrame* f = aImageFrame->GetParent();
  nsRect rect = aImageFrame->GetContentRectRelativeToSelf();
  nsIFrame* rectFrame = aImageFrame;
  while (f) {
    nsIScrollableFrame* sf = do_QueryFrame(f);
    if (sf) {
      nsRect transformedRect =
        nsLayoutUtils::TransformFrameRectToAncestor(rectFrame, rect, f);
      if (!sf->IsRectNearlyVisible(transformedRect)) {
        visible = false;
        break;
      }
      // Move transformedRect to be contained in the scrollport as best we can
      // (it might not fit) to pretend that it was scrolled into view.
      nsRect scrollPort = sf->GetScrollPortRect();
      if (transformedRect.XMost() > scrollPort.XMost()) {
        transformedRect.x -= transformedRect.XMost() - scrollPort.XMost();
      }
      if (transformedRect.x < scrollPort.x) {
        transformedRect.x = scrollPort.x;
      }
      if (transformedRect.YMost() > scrollPort.YMost()) {
        transformedRect.y -= transformedRect.YMost() - scrollPort.YMost();
      }
      if (transformedRect.y < scrollPort.y) {
        transformedRect.y = scrollPort.y;
      }
      transformedRect.width = std::min(transformedRect.width, scrollPort.width);
      transformedRect.height = std::min(transformedRect.height, scrollPort.height);
      rect = transformedRect;
      rectFrame = f;
    }
    nsIFrame* parent = f->GetParent();
    if (!parent) {
      parent = nsLayoutUtils::GetCrossDocParentFrame(f);
      if (parent && parent->PresContext()->IsChrome()) {
        break;
      }
    }
    f = parent;
  }

  if (visible) {
    presShell->EnsureImageInVisibleList(content);
  } else {
    presShell->RemoveImageFromVisibleList(content);
  }
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
  aOutSize = LayoutDeviceIntRect::FromUntyped(bounds).Size();
  return true;
}

static bool
UpdateCompositionBoundsForRCDRSF(ParentLayerRect& aCompBounds,
                                 nsPresContext* aPresContext,
                                 const nsRect& aFrameBounds,
                                 bool aScaleContentViewerSize,
                                 const LayoutDeviceToLayerScale2D& aCumulativeResolution)
{
  nsIFrame* rootFrame = aPresContext->PresShell()->GetRootFrame();
  if (!rootFrame) {
    return false;
  }

  // On Android, we need to do things a bit differently to get things
  // right (see bug 983208, bug 988882). We use the bounds of the nearest
  // widget, but clamp the height to the frame bounds height. This clamping
  // is done to get correct results for a page where the page is sized to
  // the screen and thus the dynamic toolbar never disappears. In such a
  // case, we want the composition bounds to exclude the toolbar height,
  // but the widget bounds includes it. We don't currently have a good way
  // of knowing about the toolbar height, but clamping to the frame bounds
  // height gives the correct answer in the cases we care about.
#ifdef MOZ_WIDGET_ANDROID
  nsIWidget* widget = rootFrame->GetNearestWidget();
#else
  nsView* view = rootFrame->GetView();
  nsIWidget* widget = view ? view->GetWidget() : nullptr;
#endif

  if (widget) {
    nsIntRect widgetBounds;
    widget->GetBounds(widgetBounds);
    widgetBounds.MoveTo(0, 0);
    aCompBounds = ParentLayerRect(ViewAs<ParentLayerPixel>(widgetBounds));
#ifdef MOZ_WIDGET_ANDROID
    ParentLayerRect frameBounds =
          LayoutDeviceRect::FromAppUnits(aFrameBounds, aPresContext->AppUnitsPerDevPixel())
        * aCumulativeResolution
        * LayerToParentLayerScale(1.0);
    if (frameBounds.height < aCompBounds.height) {
      aCompBounds.height = frameBounds.height;
    }
#endif
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
    LayoutDeviceToLayerScale2D cumulativeResolution(
        presShell->GetCumulativeResolution()
      * nsLayoutUtils::GetTransformToAncestorScale(aFrame));
    if (UpdateCompositionBoundsForRCDRSF(compBounds, presContext, rect,
        false, cumulativeResolution)) {
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
      if (UpdateCompositionBoundsForRCDRSF(compBounds, rootPresContext,
          rootFrame->GetRect(), true, cumulativeResolution)) {
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
    nsIntRect widgetBounds;
    widget->GetBounds(widgetBounds);
    rootCompositionSize = ScreenSize(ViewAs<ScreenPixel>(widgetBounds.Size()));
  }

  // Adjust composition size for the size of scroll bars.
  nsIFrame* rootRootScrollFrame = rootPresShell ? rootPresShell->GetRootScrollFrame() : nullptr;
  nsMargin scrollbarMargins = ScrollbarAreaToExcludeFromCompositionBoundsFor(rootRootScrollFrame);
  CSSMargin margins = CSSMargin::FromAppUnits(scrollbarMargins);
  // Scrollbars are not subject to scaling, so CSS pixels = layer pixels for them.
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

    // We ifndef the below code for Fennec because it requires special behaviour
    // on the APZC side. Because Fennec has it's own PZC implementation which doesn't
    // provide the special behaviour, this code will cause it to break. We can remove
    // the ifndef once Fennec switches over to APZ or if we add the special handling
    // to Fennec
#if !defined(MOZ_WIDGET_ANDROID) || defined(MOZ_ANDROID_APZ)
    nsPoint scrollPosition = aScrollableFrame->GetScrollPosition();
    if (aScrollableFrame->GetScrollbarStyles().mVertical == NS_STYLE_OVERFLOW_HIDDEN) {
      contentBounds.y = scrollPosition.y;
      contentBounds.height = 0;
    }
    if (aScrollableFrame->GetScrollbarStyles().mHorizontal == NS_STYLE_OVERFLOW_HIDDEN) {
      contentBounds.x = scrollPosition.x;
      contentBounds.width = 0;
    }
#endif

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
  if (aManager->GetBackendType() == LayersBackend::LAYERS_CLIENT) {
    static_cast<ClientLayerManager*>(aManager)->LogTestDataForCurrentPaint(aScrollId, aKey, aValue);
  }
}

/* static */ bool
nsLayoutUtils::IsAPZTestLoggingEnabled()
{
  return gfxPrefs::APZTestLoggingEnabled();
}

nsLayoutUtils::SurfaceFromElementResult::SurfaceFromElementResult()
  // Use safe default values here
  : mIsWriteOnly(true)
  , mIsStillLoading(false)
  , mHasSize(false)
  , mCORSUsed(false)
  , mIsPremultiplied(true)
{
}

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
MaybeSetupTransactionIdAllocator(layers::LayerManager* aManager, nsView* aView)
{
  if (aManager->GetBackendType() == layers::LayersBackend::LAYERS_CLIENT) {
    layers::ClientLayerManager *manager = static_cast<layers::ClientLayerManager*>(aManager);
    nsRefreshDriver *refresh = aView->GetViewManager()->GetPresShell()->GetPresContext()->RefreshDriver();
    manager->SetTransactionIdAllocator(refresh);
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
                                       nsHTMLReflowMetrics& aMetrics,
                                       const LogicalMargin& aFramePadding,
                                       WritingMode aLineWM,
                                       WritingMode aFrameWM)
{
  nsRefPtr<nsFontMetrics> fm;
  float inflation = nsLayoutUtils::FontSizeInflationFor(aFrame);
  nsLayoutUtils::GetFontMetricsForFrame(aFrame, getter_AddRefs(fm), inflation);

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
nsLayoutUtils::HasApzAwareListeners(EventListenerManager* aElm)
{
  if (!aElm) {
    return false;
  }
  return aElm->HasListenersFor(nsGkAtoms::ontouchstart) ||
         aElm->HasListenersFor(nsGkAtoms::ontouchmove) ||
         aElm->HasListenersFor(nsGkAtoms::onwheel) ||
         aElm->HasListenersFor(nsGkAtoms::onDOMMouseScroll) ||
         aElm->HasListenersFor(nsHtml5Atoms::onmousewheel) ||
         aElm->HasListenersFor(nsGkAtoms::onMozMousePixelScroll);
}

/* static */ bool
nsLayoutUtils::HasDocumentLevelListenersForApzAwareEvents(nsIPresShell* aShell)
{
  if (nsIDocument* doc = aShell->GetDocument()) {
    WidgetEvent event(true, NS_EVENT_NULL);
    nsTArray<EventTarget*> targets;
    nsresult rv = EventDispatcher::Dispatch(doc, nullptr, &event, nullptr,
        nullptr, nullptr, &targets);
    NS_ENSURE_SUCCESS(rv, false);
    for (size_t i = 0; i < targets.Length(); i++) {
      if (HasApzAwareListeners(targets[i]->GetExistingListenerManager())) {
        return true;
      }
    }
  }
  return false;
}

/* static */ float
nsLayoutUtils::GetResolution(nsIPresShell* aPresShell)
{
  nsIScrollableFrame* sf = aPresShell->GetRootScrollFrameAsScrollable();
  if (sf) {
    return sf->GetResolution();
  }
  return aPresShell->GetResolution();
}

/* static */ void
nsLayoutUtils::SetResolutionAndScaleTo(nsIPresShell* aPresShell, float aResolution)
{
  nsIScrollableFrame* sf = aPresShell->GetRootScrollFrameAsScrollable();
  if (sf) {
    sf->SetResolutionAndScaleTo(aResolution);
    aPresShell->SetResolutionAndScaleTo(aResolution);
  }
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

/* static */ void
nsLayoutUtils::SetCSSViewport(nsIPresShell* aPresShell, CSSSize aSize)
{
  MOZ_ASSERT(aSize.width >= 0.0 && aSize.height >= 0.0);

  nscoord width = nsPresContext::CSSPixelsToAppUnits(aSize.width);
  nscoord height = nsPresContext::CSSPixelsToAppUnits(aSize.height);

  aPresShell->ResizeReflowOverride(width, height);
}

/* static */ FrameMetrics
nsLayoutUtils::ComputeFrameMetrics(nsIFrame* aForFrame,
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
  FrameMetrics metrics;
  metrics.SetViewport(CSSRect::FromAppUnits(aViewport));

  ViewID scrollId = FrameMetrics::NULL_SCROLL_ID;
  if (aContent) {
    scrollId = nsLayoutUtils::FindOrCreateIDFor(aContent);
    nsRect dp;
    if (nsLayoutUtils::GetDisplayPort(aContent, &dp)) {
      metrics.SetDisplayPort(CSSRect::FromAppUnits(dp));
      nsLayoutUtils::LogTestDataForPaint(aLayer->Manager(), scrollId, "displayport",
          metrics.GetDisplayPort());
    }
    if (nsLayoutUtils::GetCriticalDisplayPort(aContent, &dp)) {
      metrics.SetCriticalDisplayPort(CSSRect::FromAppUnits(dp));
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

    // If the frame was scrolled since the last layers update, and by
    // something other than the APZ code, we want to tell the APZ to update
    // its scroll offset.
    nsIAtom* lastScrollOrigin = scrollableFrame->LastScrollOrigin();
    if (lastScrollOrigin && lastScrollOrigin != nsGkAtoms::apz) {
      metrics.SetScrollOffsetUpdated(scrollableFrame->CurrentScrollGeneration());
    }
    nsIAtom* lastSmoothScrollOrigin = scrollableFrame->LastSmoothScrollOrigin();
    if (lastSmoothScrollOrigin) {
      metrics.SetSmoothScrollOffsetUpdated(scrollableFrame->CurrentScrollGeneration());
    }

    nsSize lineScrollAmount = scrollableFrame->GetLineScrollAmount();
    LayoutDeviceIntSize lineScrollAmountInDevPixels =
      LayoutDeviceIntSize::FromAppUnitsRounded(lineScrollAmount, presContext->AppUnitsPerDevPixel());
    metrics.SetLineScrollAmount(lineScrollAmountInDevPixels);

    nsSize pageScrollAmount = scrollableFrame->GetPageScrollAmount();
    LayoutDeviceIntSize pageScrollAmountInDevPixels =
      LayoutDeviceIntSize::FromAppUnitsRounded(pageScrollAmount, presContext->AppUnitsPerDevPixel());
    metrics.SetPageScrollAmount(pageScrollAmountInDevPixels);

    if (!aScrollFrame->GetParent() ||
        EventStateManager::CanVerticallyScrollFrameWithWheel(aScrollFrame->GetParent()))
    {
      metrics.SetAllowVerticalScrollWithWheel();
    }

    metrics.SetUsesContainerScrolling(scrollableFrame->UsesContainerScrolling());
  }

  // If we have the scrollparent being the same as the scroll id, the
  // compositor-side code could get into an infinite loop while building the
  // overscroll handoff chain.
  MOZ_ASSERT(aScrollParentId == FrameMetrics::NULL_SCROLL_ID || scrollId != aScrollParentId);
  metrics.SetScrollId(scrollId);
  metrics.SetIsRootContent(aIsRootContent);
  metrics.SetScrollParentId(aScrollParentId);

  if (scrollId != FrameMetrics::NULL_SCROLL_ID && !presContext->GetParentPresContext()) {
    if ((aScrollFrame && (aScrollFrame == presShell->GetRootScrollFrame())) ||
        aContent == presShell->GetDocument()->GetDocumentElement()) {
      metrics.SetIsLayersIdRoot(true);
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

  metrics.SetDevPixelsPerCSSPixel(CSSToLayoutDeviceScale(
    (float)nsPresContext::AppUnitsPerCSSPixel() / auPerDevPixel));

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
    metrics.SetClipRect(Some(RoundedToInt(rect)));
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
    UpdateCompositionBoundsForRCDRSF(frameBounds, presContext,
      compositionBounds, true, metrics.GetCumulativeResolution());
  }

  nsMargin sizes = ScrollbarAreaToExcludeFromCompositionBoundsFor(aScrollFrame);
  // Scrollbars are not subject to scaling, so CSS pixels = layer pixels for them.
  ParentLayerMargin boundMargins = CSSMargin::FromAppUnits(sizes) * CSSToParentLayerScale(1.0f);
  frameBounds.Deflate(boundMargins);

  metrics.SetCompositionBounds(frameBounds);

  metrics.SetRootCompositionSize(
    nsLayoutUtils::CalculateRootCompositionSize(aScrollFrame ? aScrollFrame : aForFrame,
                                                isRootContentDocRootScrollFrame, metrics));

  if (gfxPrefs::APZPrintTree() || gfxPrefs::APZTestLoggingEnabled()) {
    if (nsIContent* content = frameForCompositionBoundsCalculation->GetContent()) {
      nsAutoString contentDescription;
      content->Describe(contentDescription);
      metrics.SetContentDescription(NS_LossyConvertUTF16toASCII(contentDescription));
      nsLayoutUtils::LogTestDataForPaint(aLayer->Manager(), scrollId, "contentDescription",
          metrics.GetContentDescription().get());
    }
  }

  metrics.SetPresShellId(presShell->GetPresShellId());

  // If the scroll frame's content is marked 'scrollgrab', record this
  // in the FrameMetrics so APZ knows to provide the scroll grabbing
  // behaviour.
  if (aScrollFrame && nsContentUtils::HasScrollgrab(aScrollFrame->GetContent())) {
    metrics.SetHasScrollgrab(true);
  }

  // Also compute and set the background color.
  // This is needed for APZ overscrolling support.
  if (aScrollFrame) {
    if (isRootScrollFrame) {
      metrics.SetBackgroundColor(presShell->GetCanvasBackground());
    } else {
      nsStyleContext* backgroundStyle;
      if (nsCSSRendering::FindBackground(aScrollFrame, &backgroundStyle)) {
        metrics.SetBackgroundColor(backgroundStyle->StyleBackground()->mBackgroundColor);
      }
    }
  }

  return metrics;
}

/* static */ bool
nsLayoutUtils::ContainsMetricsWithId(const Layer* aLayer, const ViewID& aScrollId)
{
  for (uint32_t i = aLayer->GetFrameMetricsCount(); i > 0; i--) {
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
    disp->mDisplay != NS_STYLE_DISPLAY_TABLE_CELL &&
    disp->mDisplay != NS_STYLE_DISPLAY_TABLE_CAPTION;
  if (isTableElement) {
    return NS_STYLE_TOUCH_ACTION_AUTO;
  }

  return disp->mTouchAction;
}

/* static */  void
nsLayoutUtils::TransformToAncestorAndCombineRegions(
  const nsRect& aBounds,
  nsIFrame* aFrame,
  const nsIFrame* aAncestorFrame,
  nsRegion* aPreciseTargetDest,
  nsRegion* aImpreciseTargetDest)
{
  if (aBounds.IsEmpty()) {
    return;
  }
  Matrix4x4 matrix = GetTransformToAncestor(aFrame, aAncestorFrame);
  Matrix matrix2D;
  bool isPrecise = (matrix.Is2D(&matrix2D)
    && !matrix2D.HasNonAxisAlignedTransform());
  nsRect transformed = TransformFrameRectToAncestor(
    aFrame, aBounds, aAncestorFrame);
  nsRegion* dest = isPrecise ? aPreciseTargetDest : aImpreciseTargetDest;
  dest->OrWith(transformed);
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
