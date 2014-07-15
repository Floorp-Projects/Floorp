/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLayoutUtils.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/MemoryReporting.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsFrameList.h"
#include "nsGkAtoms.h"
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
#include "gfxPoint3D.h"
#include "gfxPrefs.h"
#include "gfxTypes.h"
#include "nsTArray.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "nsICanvasRenderingContextInternal.h"
#include "gfxPlatform.h"
#include <algorithm>
#include "mozilla/dom/HTMLVideoElement.h"
#include "mozilla/dom/HTMLImageElement.h"
#include "mozilla/dom/DOMRect.h"
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

#include "mozilla/Preferences.h"

#ifdef MOZ_XUL
#include "nsXULPopupManager.h"
#endif

#include "GeckoProfiler.h"
#include "nsAnimationManager.h"
#include "nsTransitionManager.h"
#include "RestyleManager.h"

// Additional includes used on B2G by code in GetOrMaybeCreateDisplayPort().
#ifdef MOZ_WIDGET_GONK
#include "mozilla/layers/AsyncPanZoomController.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;
using namespace mozilla::layout;
using namespace mozilla::gfx;

using mozilla::image::Angle;
using mozilla::image::Flip;
using mozilla::image::ImageOps;
using mozilla::image::Orientation;

#define GRID_ENABLED_PREF_NAME "layout.css.grid.enabled"
#define STICKY_ENABLED_PREF_NAME "layout.css.sticky.enabled"
#define TEXT_ALIGN_TRUE_ENABLED_PREF_NAME "layout.css.text-align-true-value.enabled"

#ifdef DEBUG
// TODO: remove, see bug 598468.
bool nsLayoutUtils::gPreventAssertInCompareTreePosition = false;
#endif // DEBUG

typedef FrameMetrics::ViewID ViewID;

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

static ElementAnimationCollection*
GetAnimationsOrTransitionsForCompositor(nsIContent* aContent,
                                        nsIAtom* aAnimationProperty,
                                        nsCSSProperty aProperty)
{
  ElementAnimationCollection* collection =
    static_cast<ElementAnimationCollection*>(
      aContent->GetProperty(aAnimationProperty));
  if (collection) {
    bool propertyMatches = collection->HasAnimationOfProperty(aProperty);
    if (propertyMatches &&
        collection->CanPerformOnCompositorThread(
          ElementAnimationCollection::CanAnimate_AllowPartial)) {
      return collection;
    }
  }

  return nullptr;
}

bool
nsLayoutUtils::HasAnimationsForCompositor(nsIContent* aContent,
                                          nsCSSProperty aProperty)
{
  if (!aContent->MayHaveAnimations())
    return false;
  return GetAnimationsOrTransitionsForCompositor(
           aContent, nsGkAtoms::animationsProperty, aProperty) ||
         GetAnimationsOrTransitionsForCompositor(
           aContent, nsGkAtoms::transitionsProperty, aProperty);
}

static ElementAnimationCollection*
GetAnimationsOrTransitions(nsIContent* aContent,
                           nsIAtom* aAnimationProperty,
                           nsCSSProperty aProperty)
{
  ElementAnimationCollection* collection =
    static_cast<ElementAnimationCollection*>(aContent->GetProperty(
        aAnimationProperty));
  if (collection) {
    bool propertyMatches = collection->HasAnimationOfProperty(aProperty);
    if (propertyMatches) {
      return collection;
    }
  }
  return nullptr;
}

bool
nsLayoutUtils::HasAnimations(nsIContent* aContent,
                             nsCSSProperty aProperty)
{
  if (!aContent->MayHaveAnimations())
    return false;
  return GetAnimationsOrTransitions(aContent, nsGkAtoms::animationsProperty,
                                    aProperty) ||
         GetAnimationsOrTransitions(aContent, nsGkAtoms::transitionsProperty,
                                    aProperty);
}

bool
nsLayoutUtils::HasCurrentAnimations(nsIContent* aContent,
                                    nsIAtom* aAnimationProperty,
                                    nsPresContext* aPresContext)
{
  if (!aContent->MayHaveAnimations())
    return false;

  TimeStamp now = aPresContext->RefreshDriver()->MostRecentRefresh();

  ElementAnimationCollection* collection =
    static_cast<ElementAnimationCollection*>(
      aContent->GetProperty(aAnimationProperty));
  return (collection && collection->HasCurrentAnimationsAt(now));
}

static gfxSize
GetScaleForValue(const StyleAnimationValue& aValue, nsIFrame* aFrame)
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

  if (list->mHead->mValue.GetUnit() == eCSSUnit_None) {
    // There is an animation, but no actual transform yet.
    return gfxSize();
  }

  nsRect frameBounds = aFrame->GetRect();
  bool dontCare;
  gfx3DMatrix transform = nsStyleTransformMatrix::ReadTransforms(
                            list->mHead,
                            aFrame->StyleContext(),
                            aFrame->PresContext(), dontCare, frameBounds,
                            aFrame->PresContext()->AppUnitsPerDevPixel());

  gfxMatrix transform2d;
  bool canDraw2D = transform.CanDraw2D(&transform2d);
  if (!canDraw2D) {
    return gfxSize();
  }

  return transform2d.ScaleFactors(true);
}

static float
GetSuitableScale(float aMaxScale, float aMinScale)
{
  // If the minimum scale >= 1.0f, use it; if the maximum <= 1.0f, use it;
  // otherwise use 1.0f.
  if (aMinScale >= 1.0f) {
    return aMinScale;
  }
  else if (aMaxScale <= 1.0f) {
    return aMaxScale;
  }

  return 1.0f;
}

static void
GetMinAndMaxScaleForAnimationProperty(nsIContent* aContent,
                                      nsIAtom* aAnimationProperty,
                                      gfxSize& aMaxScale,
                                      gfxSize& aMinScale)
{
  ElementAnimationCollection* collection =
    GetAnimationsOrTransitionsForCompositor(aContent, aAnimationProperty,
                                            eCSSProperty_transform);
  if (!collection)
    return;

  for (uint32_t animIdx = collection->mAnimations.Length(); animIdx-- != 0; ) {
    mozilla::ElementAnimation* anim = collection->mAnimations[animIdx];
    if (anim->IsFinishedTransition()) {
      continue;
    }
    for (uint32_t propIdx = anim->mProperties.Length(); propIdx-- != 0; ) {
      AnimationProperty& prop = anim->mProperties[propIdx];
      if (prop.mProperty == eCSSProperty_transform) {
        for (uint32_t segIdx = prop.mSegments.Length(); segIdx-- != 0; ) {
          AnimationPropertySegment& segment = prop.mSegments[segIdx];
          gfxSize from = GetScaleForValue(segment.mFromValue,
                                          aContent->GetPrimaryFrame());
          aMaxScale.width = std::max<float>(aMaxScale.width, from.width);
          aMaxScale.height = std::max<float>(aMaxScale.height, from.height);
          aMinScale.width = std::min<float>(aMinScale.width, from.width);
          aMinScale.height = std::min<float>(aMinScale.height, from.height);
          gfxSize to = GetScaleForValue(segment.mToValue,
                                        aContent->GetPrimaryFrame());
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
nsLayoutUtils::ComputeSuitableScaleForAnimation(nsIContent* aContent)
{
  gfxSize maxScale(1.0f, 1.0f);
  gfxSize minScale(1.0f, 1.0f);

  GetMinAndMaxScaleForAnimationProperty(aContent,
    nsGkAtoms::animationsProperty, maxScale, minScale);
  GetMinAndMaxScaleForAnimationProperty(aContent,
    nsGkAtoms::transitionsProperty, maxScale, minScale);

  return gfxSize(GetSuitableScale(maxScale.width, minScale.width),
                 GetSuitableScale(maxScale.height, minScale.height));
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
  NS_ABORT_IF_FALSE(aId != FrameMetrics::NULL_SCROLL_ID,
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
    isRoot = true;
  }

  nsPoint scrollPos;
  if (nsIScrollableFrame* scrollableFrame = frame->GetScrollTargetFrame()) {
    scrollPos = scrollableFrame->GetScrollPosition();
  }

  nsPresContext* presContext = frame->PresContext();
  int32_t auPerDevPixel = presContext->AppUnitsPerDevPixel();
  gfxSize res = presContext->PresShell()->GetCumulativeResolution();

  // First convert the base rect to layer pixels
  gfxSize parentRes = res;
  if (isRoot) {
    // the base rect for root scroll frames is specified in the parent document
    // coordinate space, so it doesn't include the local resolution.
    gfxSize localRes = presContext->PresShell()->GetResolution();
    parentRes.width /= localRes.width;
    parentRes.height /= localRes.height;
  }
  LayerRect layerRect(NSAppUnitsToFloatPixels(base.x, auPerDevPixel) * parentRes.width,
                      NSAppUnitsToFloatPixels(base.y, auPerDevPixel) * parentRes.height,
                      NSAppUnitsToFloatPixels(base.width, auPerDevPixel) * parentRes.width,
                      NSAppUnitsToFloatPixels(base.height, auPerDevPixel) * parentRes.height);

  // Expand the rect by the margins
  layerRect.Inflate(aMarginsData->mMargins);

  // And then align it to the requested alignment
  if (aMarginsData->mAlignmentX > 0 || aMarginsData->mAlignmentY > 0) {
    // Inflate the rectangle by 1 so that we always push to the next tile
    // boundary. This is desirable to stop from having a rectangle with a
    // moving origin occasionally being smaller when it coincidentally lines
    // up to tile boundaries.
    layerRect.Inflate(1);

    // Avoid division by zero.
    if (aMarginsData->mAlignmentX == 0) {
      aMarginsData->mAlignmentX = 1;
    }
    if (aMarginsData->mAlignmentY == 0) {
      aMarginsData->mAlignmentY = 1;
    }

    LayerPoint scrollPosLayer(NSAppUnitsToFloatPixels(scrollPos.x, auPerDevPixel) * res.width,
                              NSAppUnitsToFloatPixels(scrollPos.y, auPerDevPixel) * res.height);

    layerRect += scrollPosLayer;
    float x = aMarginsData->mAlignmentX * floor(layerRect.x / aMarginsData->mAlignmentX);
    float y = aMarginsData->mAlignmentY * floor(layerRect.y / aMarginsData->mAlignmentY);
    float w = aMarginsData->mAlignmentX * ceil(layerRect.XMost() / aMarginsData->mAlignmentX) - x;
    float h = aMarginsData->mAlignmentY * ceil(layerRect.YMost() / aMarginsData->mAlignmentY) - y;
    layerRect = LayerRect(x, y, w, h);
    layerRect -= scrollPosLayer;
  }

  // Convert the aligned rect back into app units
  nsRect result(NSFloatPixelsToAppUnits(layerRect.x / res.width, auPerDevPixel),
                NSFloatPixelsToAppUnits(layerRect.y / res.height, auPerDevPixel),
                NSFloatPixelsToAppUnits(layerRect.width / res.width, auPerDevPixel),
                NSFloatPixelsToAppUnits(layerRect.height / res.height, auPerDevPixel));

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

  if (rectData) {
    *aResult = GetDisplayPortFromRectData(aContent, rectData, aMultiplier);
  } else {
    *aResult = GetDisplayPortFromMarginsData(aContent, marginsData, aMultiplier);
  }
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

void
nsLayoutUtils::SetDisplayPortMargins(nsIContent* aContent,
                                     nsIPresShell* aPresShell,
                                     const LayerMargin& aMargins,
                                     uint32_t aAlignmentX,
                                     uint32_t aAlignmentY,
                                     uint32_t aPriority,
                                     RepaintMode aRepaintMode)
{
  DisplayPortMarginsPropertyData* currentData =
    static_cast<DisplayPortMarginsPropertyData*>(aContent->GetProperty(nsGkAtoms::DisplayPortMargins));
  if (currentData && currentData->mPriority > aPriority) {
    return;
  }

  aContent->SetProperty(nsGkAtoms::DisplayPortMargins,
                        new DisplayPortMarginsPropertyData(
                            aMargins, aAlignmentX, aAlignmentY, aPriority),
                        nsINode::DeleteProperty<DisplayPortMarginsPropertyData>);

  nsIFrame* rootScrollFrame = aPresShell->GetRootScrollFrame();
  if (rootScrollFrame && aContent == rootScrollFrame->GetContent()) {
    // We are setting a root displayport for a document.
    // The pres shell needs a special flag set.
    aPresShell->SetIgnoreViewportScrolling(true);
  }

  if (aRepaintMode == RepaintMode::Repaint) {
    nsIFrame* rootFrame = aPresShell->FrameManager()->GetRootFrame();
    if (rootFrame) {
      rootFrame->SchedulePaint();
    }
  }
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

/**
 * GetFirstChildFrame returns the first "real" child frame of a
 * given frame.  It will descend down into pseudo-frames (unless the
 * pseudo-frame is the :before generated frame).
 * @param aFrame the frame
 * @param aFrame the frame's content node
 */
static nsIFrame*
GetFirstChildFrame(nsIFrame*       aFrame,
                   nsIContent*     aContent)
{
  NS_PRECONDITION(aFrame, "NULL frame pointer");

  // Get the first child frame
  nsIFrame* childFrame = aFrame->GetFirstPrincipalChild();

  // If the child frame is a pseudo-frame, then return its first child.
  // Note that the frame we create for the generated content is also a
  // pseudo-frame and so don't drill down in that case
  if (childFrame &&
      childFrame->IsPseudoFrame(aContent) &&
      !childFrame->IsGeneratedContentFrame()) {
    return GetFirstChildFrame(childFrame, aContent);
  }

  return childFrame;
}

/**
 * GetLastChildFrame returns the last "real" child frame of a
 * given frame.  It will descend down into pseudo-frames (unless the
 * pseudo-frame is the :after generated frame).
 * @param aFrame the frame
 * @param aFrame the frame's content node
 */
static nsIFrame*
GetLastChildFrame(nsContainerFrame* aFrame,
                  nsIContent*       aContent)
{
  NS_PRECONDITION(aFrame, "NULL frame pointer");

  // Get the last continuation frame that's a parent
  nsContainerFrame* lastParentContinuation =
    nsLayoutUtils::LastContinuationWithChild(aFrame);
  nsIFrame* lastChildFrame =
    lastParentContinuation->GetLastChild(nsIFrame::kPrincipalList);
  if (lastChildFrame) {
    // Get the frame's first continuation. This matters in case the frame has
    // been continued across multiple lines or split by BiDi resolution.
    lastChildFrame = lastChildFrame->FirstContinuation();

    // If the last child frame is a pseudo-frame, then return its last child.
    // Note that the frame we create for the generated content is also a
    // pseudo-frame and so don't drill down in that case
    if (lastChildFrame &&
        lastChildFrame->IsPseudoFrame(aContent) &&
        !lastChildFrame->IsGeneratedContentFrame()) {
      return GetLastChildFrame(static_cast<nsContainerFrame*>(lastChildFrame),
                               aContent);
    }

    return lastChildFrame;
  }

  return nullptr;
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
    } else if (nsGkAtoms::tableCaptionFrame == childType) {
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

// static
nsIFrame*
nsLayoutUtils::GetBeforeFrame(nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "NULL frame pointer");
  NS_ASSERTION(!aFrame->GetPrevContinuation(),
               "aFrame must be first continuation");

  nsIFrame* cif = aFrame->GetContentInsertionFrame();
  if (!cif) {
    return nullptr;
  }
  nsIFrame* firstFrame = GetFirstChildFrame(cif, aFrame->GetContent());
  if (firstFrame && IsGeneratedContentFor(nullptr, firstFrame,
                                          nsCSSPseudoElements::before)) {
    return firstFrame;
  }

  return nullptr;
}

// static
nsIFrame*
nsLayoutUtils::GetAfterFrame(nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "NULL frame pointer");

  nsContainerFrame* cif = aFrame->GetContentInsertionFrame();
  if (!cif) {
    return nullptr;
  }
  nsIFrame* lastFrame = GetLastChildFrame(cif, aFrame->GetContent());
  if (lastFrame && IsGeneratedContentFor(nullptr, lastFrame,
                                         nsCSSPseudoElements::after)) {
    return lastFrame;
  }

  return nullptr;
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

  return (aFrame->GetContent()->Tag() == nsGkAtoms::mozgeneratedcontentbefore) ==
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
  return sf;
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

static bool
IsScrollbarThumbLayerized(nsIFrame* aThumbFrame)
{
  return reinterpret_cast<intptr_t>(aThumbFrame->Properties().Get(ScrollbarThumbLayerized()));
}

static nsIFrame*
GetAnimatedGeometryRootForFrame(nsIFrame* aFrame,
                                const nsIFrame* aStopAtAncestor)
{
  nsIFrame* f = aFrame;
  nsIFrame* stickyFrame = nullptr;
  while (f != aStopAtAncestor) {
    if (nsLayoutUtils::IsPopup(f))
      break;
    if (ActiveLayerTracker::IsOffsetOrMarginStyleAnimated(f))
      break;
    if (!f->GetParent() &&
        nsLayoutUtils::ViewportHasDisplayPort(f->PresContext())) {
      // Viewport frames in a display port need to be animated geometry roots
      // for background-attachment:fixed elements.
      break;
    }
    nsIFrame* parent = nsLayoutUtils::GetCrossDocParentFrame(f);
    if (!parent)
      break;
    nsIAtom* parentType = parent->GetType();
    // Treat the slider thumb as being as an active scrolled root when it wants
    // its own layer so that it can move without repainting.
    if (parentType == nsGkAtoms::sliderFrame && IsScrollbarThumbLayerized(f)) {
      break;
    }
    // Sticky frames are active if their nearest scrollable frame
    // is also active, just keep a record of sticky frames that we
    // encounter for now.
    if (f->StyleDisplay()->mPosition == NS_STYLE_POSITION_STICKY &&
        !stickyFrame) {
      stickyFrame = f;
    }
    if (parentType == nsGkAtoms::scrollFrame) {
      nsIScrollableFrame* sf = do_QueryFrame(parent);
      if (sf->IsScrollingActive() && sf->GetScrolledFrame() == f) {
        // If we found a sticky frame inside this active scroll frame,
        // then use that. Otherwise use the scroll frame.
        if (stickyFrame) {
          return stickyFrame;
        }
        return f;
      } else {
        stickyFrame = nullptr;
      }
    }
    // Fixed-pos frames are parented by the viewport frame, which has no parent
    if (nsLayoutUtils::IsFixedPosFrameInDisplayPort(f)) {
      return f;
    }
    f = parent;
  }
  return f;
}

nsIFrame*
nsLayoutUtils::GetAnimatedGeometryRootFor(nsDisplayItem* aItem,
                                          nsDisplayListBuilder* aBuilder)
{
  nsIFrame* f = aItem->Frame();
  if (aItem->GetType() == nsDisplayItem::TYPE_SCROLL_LAYER) {
    nsDisplayScrollLayer* scrollLayerItem =
      static_cast<nsDisplayScrollLayer*>(aItem);
    nsIFrame* scrolledFrame = scrollLayerItem->GetScrolledFrame();
    return GetAnimatedGeometryRootForFrame(scrolledFrame,
        aBuilder->FindReferenceFrameFor(scrolledFrame));
  }
  if (aItem->ShouldFixToViewport(aBuilder)) {
    // Make its active scrolled root be the active scrolled root of
    // the enclosing viewport, since it shouldn't be scrolled by scrolled
    // frames in its document. InvalidateFixedBackgroundFramesFromList in
    // nsGfxScrollFrame will not repaint this item when scrolling occurs.
    nsIFrame* viewportFrame =
      nsLayoutUtils::GetClosestFrameOfType(f, nsGkAtoms::viewportFrame);
    NS_ASSERTION(viewportFrame, "no viewport???");
    return GetAnimatedGeometryRootForFrame(viewportFrame,
        aBuilder->FindReferenceFrameFor(viewportFrame));
  }
  return GetAnimatedGeometryRootForFrame(f, aItem->ReferenceFrame());
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
      ScrollbarStyles ss = scrollableFrame->GetScrollbarStyles();
      if ((aFlags & SCROLLABLE_INCLUDE_HIDDEN) ||
          ss.mVertical != NS_STYLE_OVERFLOW_HIDDEN ||
          ss.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN)
        return scrollableFrame;
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
  if (!aEvent || (aEvent->eventStructType != NS_MOUSE_EVENT &&
                  aEvent->eventStructType != NS_MOUSE_SCROLL_EVENT &&
                  aEvent->eventStructType != NS_WHEEL_EVENT &&
                  aEvent->eventStructType != NS_DRAG_EVENT &&
                  aEvent->eventStructType != NS_SIMPLE_GESTURE_EVENT &&
                  aEvent->eventStructType != NS_POINTER_EVENT &&
                  aEvent->eventStructType != NS_GESTURENOTIFY_EVENT &&
                  aEvent->eventStructType != NS_TOUCH_EVENT &&
                  aEvent->eventStructType != NS_QUERY_CONTENT_EVENT))
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);

  return GetEventCoordinatesRelativeTo(aEvent,
           LayoutDeviceIntPoint::ToUntyped(aEvent->AsGUIEvent()->refPoint),
           aFrame);
}

nsPoint
nsLayoutUtils::GetEventCoordinatesRelativeTo(const WidgetEvent* aEvent,
                                             const nsIntPoint aPoint,
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
                                             const nsIntPoint aPoint,
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
  widgetToView = widgetToView.ConvertAppUnits(rootAPD, localAPD);

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

gfx3DMatrix
nsLayoutUtils::ChangeMatrixBasis(const gfxPoint3D &aOrigin,
                                 const gfx3DMatrix &aMatrix)
{
  gfx3DMatrix result = aMatrix;

  /* Translate to the origin before aMatrix */
  result.Translate(-aOrigin);

  /* Translate back into position after aMatrix */
  result.TranslatePost(aOrigin);

  return result; 
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

// Helper for RoundedRectIntersectsRect.
static bool
CheckCorner(nscoord aXOffset, nscoord aYOffset,
            nscoord aXRadius, nscoord aYRadius)
{
  NS_ABORT_IF_FALSE(aXOffset > 0 && aYOffset > 0,
                    "must not pass nonpositives to CheckCorner");
  NS_ABORT_IF_FALSE(aXRadius >= 0 && aYRadius >= 0,
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

gfx3DMatrix
nsLayoutUtils::GetTransformToAncestor(nsIFrame *aFrame, const nsIFrame *aAncestor)
{
  nsIFrame* parent;
  gfx3DMatrix ctm;
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

static nsIFrame*
FindNearestCommonAncestorFrame(nsIFrame* aFrame1, nsIFrame* aFrame2)
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
  gfx3DMatrix downToDest = GetTransformToAncestor(aToFrame, nearestCommonAncestor);
  if (downToDest.IsSingular()) {
    return NONINVERTIBLE_TRANSFORM;
  }
  downToDest.Invert();
  gfx3DMatrix upToAncestor = GetTransformToAncestor(aFromFrame, nearestCommonAncestor);
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
    gfxPoint toDevPixels = downToDest.ProjectPoint(
        upToAncestor.Transform(gfxPoint(devPixels.x, devPixels.y))).As2DPoint();
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
  gfx3DMatrix downToDest = GetTransformToAncestor(aToFrame, nearestCommonAncestor);
  if (downToDest.IsSingular()) {
    return NONINVERTIBLE_TRANSFORM;
  }
  downToDest.Invert();
  gfx3DMatrix upToAncestor = GetTransformToAncestor(aFromFrame, nearestCommonAncestor);

  float devPixelsPerAppUnitFromFrame =
    1.0f / aFromFrame->PresContext()->AppUnitsPerDevPixel();
  float devPixelsPerAppUnitToFrame =
    1.0f / aToFrame->PresContext()->AppUnitsPerDevPixel();
  gfxPointH3D toDevPixels = downToDest.ProjectPoint(
      upToAncestor.Transform(
        gfxPoint(aPoint.x * devPixelsPerAppUnitFromFrame,
                 aPoint.y * devPixelsPerAppUnitFromFrame)));
  if (!toDevPixels.HasPositiveWCoord()) {
    // Not strictly true, but we failed to get a valid point in this
    // coordinate space.
    return NONINVERTIBLE_TRANSFORM;
  }
  aPoint.x = toDevPixels.x / devPixelsPerAppUnitToFrame;
  aPoint.y = toDevPixels.y / devPixelsPerAppUnitToFrame;
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
  gfx3DMatrix downToDest = GetTransformToAncestor(aToFrame, nearestCommonAncestor);
  if (downToDest.IsSingular()) {
    return NONINVERTIBLE_TRANSFORM;
  }
  downToDest.Invert();
  gfx3DMatrix upToAncestor = GetTransformToAncestor(aFromFrame, nearestCommonAncestor);

  float devPixelsPerAppUnitFromFrame =
    1.0f / aFromFrame->PresContext()->AppUnitsPerDevPixel();
  float devPixelsPerAppUnitToFrame =
    1.0f / aToFrame->PresContext()->AppUnitsPerDevPixel();
  gfxRect toDevPixels = downToDest.ProjectRectBounds(
    upToAncestor.ProjectRectBounds(
      gfxRect(aRect.x * devPixelsPerAppUnitFromFrame,
              aRect.y * devPixelsPerAppUnitFromFrame,
              aRect.width * devPixelsPerAppUnitFromFrame,
              aRect.height * devPixelsPerAppUnitFromFrame)));
  aRect.x = toDevPixels.x / devPixelsPerAppUnitToFrame;
  aRect.y = toDevPixels.y / devPixelsPerAppUnitToFrame;
  aRect.width = toDevPixels.width / devPixelsPerAppUnitToFrame;
  aRect.height = toDevPixels.height / devPixelsPerAppUnitToFrame;
  return TRANSFORM_SUCCEEDED;
}

bool
nsLayoutUtils::GetLayerTransformForFrame(nsIFrame* aFrame,
                                         gfx3DMatrix* aTransform)
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
    new (&builder) nsDisplayTransform(&builder, aFrame, &list);

  *aTransform =
    item->GetTransform();
  item->~nsDisplayTransform();

  return true;
}

static bool
TransformGfxPointFromAncestor(nsIFrame *aFrame,
                              const gfxPoint &aPoint,
                              nsIFrame *aAncestor,
                              gfxPoint* aOut)
{
  gfx3DMatrix ctm = nsLayoutUtils::GetTransformToAncestor(aFrame, aAncestor);

  float factor = aFrame->PresContext()->AppUnitsPerDevPixel();
  nsRect childBounds = aFrame->GetVisualOverflowRectRelativeToSelf();
  gfxRect childGfxBounds(NSAppUnitsToFloatPixels(childBounds.x, factor),
                         NSAppUnitsToFloatPixels(childBounds.y, factor),
                         NSAppUnitsToFloatPixels(childBounds.width, factor),
                         NSAppUnitsToFloatPixels(childBounds.height, factor));
  gfxPointH3D point = ctm.Inverse().ProjectPoint(aPoint);
  if (!point.HasPositiveWCoord()) {
    return false;
  }
  *aOut = point.As2DPoint();
  return true;
}

static gfxRect
TransformGfxRectToAncestor(nsIFrame *aFrame,
                           const gfxRect &aRect,
                           const nsIFrame *aAncestor,
                           bool* aPreservesAxisAlignedRectangles = nullptr)
{
  gfx3DMatrix ctm = nsLayoutUtils::GetTransformToAncestor(aFrame, aAncestor);
  if (aPreservesAxisAlignedRectangles) {
    gfxMatrix matrix2d;
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
    gfxPoint result(NSAppUnitsToFloatPixels(aPoint.x, factor),
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
  gfxRect result;

  if (text) {
    result = text->TransformFrameRectFromTextChild(aRect, aFrame);
    result = TransformGfxRectToAncestor(text, result, aAncestor);
    // TransformFrameRectFromTextChild could involve any kind of transform, we
    // could drill down into it to get an answer out of it but we don't yet.
    if (aPreservesAxisAlignedRectangles)
      *aPreservesAxisAlignedRectangles = false;
  } else {
    result = gfxRect(NSAppUnitsToFloatPixels(aRect.x, srcAppUnitsPerDevPixel),
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

static nsIntPoint GetWidgetOffset(nsIWidget* aWidget, nsIWidget*& aRootWidget) {
  nsIntPoint offset(0, 0);
  while ((aWidget->WindowType() == eWindowType_child ||
          aWidget->WindowType() == eWindowType_plugin)) {
    nsIWidget* parent = aWidget->GetParent();
    if (!parent) {
      break;
    }
    nsIntRect bounds;
    aWidget->GetBounds(bounds);
    offset += bounds.TopLeft();
    aWidget = parent;
  }
  aRootWidget = aWidget;
  return offset;
}

nsPoint
nsLayoutUtils::TranslateWidgetToView(nsPresContext* aPresContext,
                                     nsIWidget* aWidget, nsIntPoint aPt,
                                     nsView* aView)
{
  nsPoint viewOffset;
  nsIWidget* viewWidget = aView->GetNearestWidget(&viewOffset);
  if (!viewWidget) {
    return nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  }

  nsIWidget* fromRoot;
  nsIntPoint fromOffset = GetWidgetOffset(aWidget, fromRoot);
  nsIWidget* toRoot;
  nsIntPoint toOffset = GetWidgetOffset(viewWidget, toRoot);

  nsIntPoint widgetPoint;
  if (fromRoot == toRoot) {
    widgetPoint = aPt + fromOffset - toOffset;
  } else {
    nsIntPoint screenPoint = aWidget->WidgetToScreenOffset();
    widgetPoint = aPt + screenPoint - viewWidget->WidgetToScreenOffset();
  }

  nsPoint widgetAppUnits(aPresContext->DevPixelsToAppUnits(widgetPoint.x),
                         aPresContext->DevPixelsToAppUnits(widgetPoint.y));
  return widgetAppUnits - viewOffset;
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

nsresult
nsLayoutUtils::GetRemoteContentIds(nsIFrame* aFrame,
                                   const nsRect& aTarget,
                                   nsTArray<ViewID> &aOutIDs,
                                   bool aIgnoreRootScrollFrame)
{
  nsDisplayListBuilder builder(aFrame, nsDisplayListBuilder::EVENT_DELIVERY,
                               false);
  nsDisplayList list;

  if (aIgnoreRootScrollFrame) {
    nsIFrame* rootScrollFrame =
      aFrame->PresContext()->PresShell()->GetRootScrollFrame();
    if (rootScrollFrame) {
      builder.SetIgnoreScrollFrame(rootScrollFrame);
    }
  }

  builder.EnterPresShell(aFrame, aTarget);
  aFrame->BuildDisplayListForStackingContext(&builder, aTarget, &list);
  builder.LeavePresShell(aFrame, aTarget);

  nsAutoTArray<nsIFrame*,8> outFrames;
  nsDisplayItem::HitTestState hitTestState(&aOutIDs);
  list.HitTest(&builder, aTarget, &hitTestState, &outFrames);
  list.DeleteAll();

  return NS_OK;
}

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
  nsRect target(aRect);

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

  builder.EnterPresShell(aFrame, target);
  aFrame->BuildDisplayListForStackingContext(&builder, target, &list);
  builder.LeavePresShell(aFrame, target);

#ifdef MOZ_DUMP_PAINTING
  if (gDumpEventList) {
    fprintf_stderr(stderr, "Event handling --- (%d,%d):\n", aRect.x, aRect.y);

    std::stringstream ss;
    nsFrame::PrintDisplayList(&builder, list, ss);
    fprintf_stderr(stderr, "%s", ss.str().c_str());
  }
#endif

  nsDisplayItem::HitTestState hitTestState;
  list.HitTest(&builder, target, &hitTestState, &aOutFrames);
  list.DeleteAll();
  return NS_OK;
}

// This function is only used on B2G, and some compilers complain about
// unused static functions, so we need to #ifdef it.
#ifdef MOZ_WIDGET_GONK
// aScrollFrame and aScrollFrameAsScrollable must be non-nullptr
static FrameMetrics
CalculateFrameMetricsForDisplayPort(nsIFrame* aScrollFrame,
                                    nsIScrollableFrame* aScrollFrameAsScrollable) {
  // Calculate the metrics necessary for calculating the displayport.
  // This code has a lot in common with the code in RecordFrameMetrics();
  // we may want to refactor this at some point.
  FrameMetrics metrics;
  nsPresContext* presContext = aScrollFrame->PresContext();
  nsIPresShell* presShell = presContext->PresShell();
  CSSToLayoutDeviceScale deviceScale(float(nsPresContext::AppUnitsPerCSSPixel())
                                     / presContext->AppUnitsPerDevPixel());
  ParentLayerToLayerScale resolution;
  if (aScrollFrame == presShell->GetRootScrollFrame()) {
    // Only the root scrollable frame for a given presShell should pick up
    // the presShell's resolution. All the other frames are 1.0.
    resolution = ParentLayerToLayerScale(presShell->GetXResolution(),
                                         presShell->GetYResolution());
  }
  LayoutDeviceToLayerScale cumulativeResolution(presShell->GetCumulativeResolution().width);

  metrics.mDevPixelsPerCSSPixel = deviceScale;
  metrics.mResolution = resolution;
  metrics.mCumulativeResolution = cumulativeResolution;
  metrics.SetZoom(deviceScale * cumulativeResolution * LayerToScreenScale(1));

  // Only the size of the composition bounds is relevant to the
  // displayport calculation, not its origin.
  nsSize compositionSize = nsLayoutUtils::CalculateCompositionSizeForFrame(aScrollFrame);
  metrics.mCompositionBounds
      = LayoutDeviceRect::FromAppUnits(nsRect(nsPoint(0, 0), compositionSize),
                                       presContext->AppUnitsPerDevPixel())
      * (cumulativeResolution / resolution);

  // This function is used for setting a display port for subframes, so
  // aScrollFrame will not be the root content document's root scroll frame.
  metrics.SetRootCompositionSize(
      nsLayoutUtils::CalculateRootCompositionSize(aScrollFrame, false, metrics));

  metrics.SetScrollOffset(CSSPoint::FromAppUnits(
      aScrollFrameAsScrollable->GetScrollPosition()));

  metrics.mScrollableRect = CSSRect::FromAppUnits(
      nsLayoutUtils::CalculateScrollableRectForFrame(aScrollFrameAsScrollable, nullptr));

  return metrics;
}
#endif

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

#ifdef MOZ_WIDGET_GONK
  // On B2G, we perform an optimization where we ensure that at least one
  // async-scrollable frame (i.e. one that WantsAsyncScroll()) has a displayport.
  // If that's not the case yet, and we are async-scrollable, we will get a
  // displayport.
  // Note: we only do this in processes where we do subframe scrolling to
  //       begin with (i.e., not in the parent process on B2G).
  if (aBuilder.IsPaintingToWindow() && WantSubAPZC() &&
      !aBuilder.HaveScrollableDisplayPort() &&
      scrollableFrame->WantAsyncScroll()) {

    // If we don't already have a displayport, calculate and set one.
    if (!haveDisplayPort) {
      FrameMetrics metrics = CalculateFrameMetricsForDisplayPort(aScrollFrame, scrollableFrame);
      LayerMargin displayportMargins = AsyncPanZoomController::CalculatePendingDisplayPort(
          metrics, ScreenPoint(0.0f, 0.0f), 0.0);
      nsIPresShell* presShell = aScrollFrame->PresContext()->GetPresShell();
      gfx::IntSize alignment = gfxPrefs::LayersTilesEnabled()
          ? gfx::IntSize(gfxPrefs::LayersTileWidth(), gfxPrefs::LayersTileHeight()) :
            gfx::IntSize(0, 0);
      nsLayoutUtils::SetDisplayPortMargins(
          content, presShell, displayportMargins, alignment.width,
          alignment.height, 0, nsLayoutUtils::RepaintMode::DoNotRepaint);
      haveDisplayPort = GetDisplayPort(content, aOutDisplayport);
      NS_ASSERTION(haveDisplayPort, "should have a displayport after having just set it");
    }

    // Record that the we now have a scrollable display port.
    aBuilder.SetHaveScrollableDisplayPort();
  }
#endif

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

  nsDisplayListBuilder builder(aFrame, nsDisplayListBuilder::PAINTING,
                           !(aFlags & PAINT_HIDE_CARET));

  nsIFrame* rootScrollFrame = presShell->GetRootScrollFrame();
  bool usingDisplayPort = false;
  nsRect displayport;
  if (rootScrollFrame && !aFrame->GetParent()) {
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
  // Windowed plugins aren't allowed in popups
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
        aRenderingContext->Translate(pos);
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
  builder.EnterPresShell(aFrame, dirtyRect);
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

  builder.LeavePresShell(aFrame, dirtyRect);

  if (builder.GetHadToIgnorePaintSuppression()) {
    willFlushRetainedLayers = true;
  }

#ifdef MOZ_DUMP_PAINTING
  FILE* savedDumpFile = gfxUtils::sDumpPaintFile;

  std::stringstream ss;
  if (gfxUtils::DumpPaintList() || gfxUtils::sDumpPainting) {
    if (gfxUtils::sDumpPaintingToFile) {
      nsCString string("dump-");
      string.AppendInt(gPaintCount);
      string.AppendLiteral(".html");
      gfxUtils::sDumpPaintFile = fopen(string.BeginReading(), "w");
    } else {
      gfxUtils::sDumpPaintFile = stderr;
    }
    if (gfxUtils::sDumpPaintingToFile) {
      ss << "<html><head><script>var array = {}; function ViewImage(index) { window.location = array[index]; }</script></head><body>";
    }
    ss << nsPrintfCString("Painting --- before optimization (dirty %d,%d,%d,%d):\n",
            dirtyRect.x, dirtyRect.y, dirtyRect.width, dirtyRect.height).get();
    nsFrame::PrintDisplayList(&builder, list, ss, gfxUtils::sDumpPaintingToFile);
    if (gfxUtils::sDumpPaintingToFile) {
      ss << "<script>";
    }
  }
#endif

  list.ComputeVisibilityForRoot(&builder, &visibleRegion,
                                usingDisplayPort ? rootScrollFrame : nullptr);

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

  list.PaintRoot(&builder, aRenderingContext, flags);

#ifdef MOZ_DUMP_PAINTING
  if (gfxUtils::DumpPaintList() || gfxUtils::sDumpPainting) {
    if (gfxUtils::sDumpPaintingToFile) {
      ss << "</script>";
    }
    ss << "Painting --- after optimization:\n";
    nsFrame::PrintDisplayList(&builder, list, ss, gfxUtils::sDumpPaintingToFile);

    ss << "Painting --- retained layer tree:\n";
    nsIWidget* widget = aFrame->GetNearestWidget();
    if (widget) {
      nsRefPtr<LayerManager> layerManager = widget->GetLayerManager();
      if (layerManager) {
        FrameLayerBuilder::DumpRetainedLayerTree(layerManager, ss,
                                                 gfxUtils::sDumpPaintingToFile);
      }
    }
    if (gfxUtils::sDumpPaintingToFile) {
      ss << "</body></html>";
    }

    char line[1024];
    while (!ss.eof()) {
      ss.getline(line, sizeof(line));
      if (!ss.eof() || strlen(line) > 0) {
        fprintf_stderr(gfxUtils::sDumpPaintFile, "%s\n", line);
      }
      if (ss.fail()) {
        // line was too long, skip to next newline
        ss.clear();
        ss.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      }
    }

    if (gfxUtils::sDumpPaintingToFile) {
      fclose(gfxUtils::sDumpPaintFile);
    }
    gfxUtils::sDumpPaintFile = savedDumpFile;
    gPaintCount++;
  }
#endif

  // Update the widget's opaque region information. This sets
  // glass boundaries on Windows. Also set up plugin clip regions and bounds.
  if ((aFlags & PAINT_WIDGET_LAYERS) &&
      !willFlushRetainedLayers &&
      !(aFlags & PAINT_DOCUMENT_RELATIVE)) {
    nsIWidget *widget = aFrame->GetNearestWidget();
    if (widget) {
      nsRegion excludedRegion = builder.GetExcludedGlassRegion();
      nsIntRegion windowRegion(excludedRegion.ToNearestPixels(presContext->AppUnitsPerDevPixel()));
      widget->UpdateOpaqueRegion(windowRegion);
    }
  }

  if (builder.WillComputePluginGeometry()) {
    nsRefPtr<LayerManager> layerManager;
    nsIWidget* widget = aFrame->GetNearestWidget();
    if (widget) {
      layerManager = widget->GetLayerManager();
    }

    rootPresContext->ComputePluginGeometryUpdates(aFrame, &builder, &list);

    // We're not going to get a WillPaintWindow event here if we didn't do
    // widget invalidation, so just apply the plugin geometry update here instead.
    // We could instead have the compositor send back an equivalent to WillPaintWindow,
    // but it should be close enough to now not to matter.
    if (layerManager && !layerManager->NeedsWidgetInvalidation()) {
      rootPresContext->ApplyPluginGeometryUpdates();
    }

    // We told the compositor thread not to composite when it received the transaction because
    // we wanted to update plugins first. Schedule the composite now.
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
    aTextWidth = aRendContext->GetWidth(aText, aIndex);
    return true;
  }

  int32_t inx = aStartInx + (range / 2);

  // Make sure we don't leave a dangling low surrogate
  if (NS_IS_HIGH_SURROGATE(aText[inx-1]))
    inx++;

  int32_t textWidth = aRendContext->GetWidth(aText, inx);

  int32_t fullWidth = aBaseWidth + textWidth;
  if (fullWidth == aCursorPos) {
    aTextWidth = textWidth;
    aIndex = inx;
    return true;
  } else if (aCursorPos < fullWidth) {
    aTextWidth = aBaseWidth;
    if (BinarySearchForPosition(aRendContext, aText, aBaseWidth, aBaseInx, aStartInx, inx, aCursorPos, aIndex, aTextWidth)) {
      return true;
    }
  } else {
    aTextWidth = fullWidth;
    if (BinarySearchForPosition(aRendContext, aText, aBaseWidth, aBaseInx, inx, aEndInx, aCursorPos, aIndex, aTextWidth)) {
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
    for (nsIFrame* kid = aFrame->GetFirstPrincipalChild(); kid; kid = kid->GetNextSibling()) {
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
      for (nsIFrame* kid = aFrame->GetFirstPrincipalChild(); kid; kid = kid->GetNextSibling()) {
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

  virtual void AddBox(nsIFrame* aFrame) MOZ_OVERRIDE {
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

nsLayoutUtils::FirstAndLastRectCollector::FirstAndLastRectCollector()
  : mSeenFirstRect(false)
{
}

void nsLayoutUtils::FirstAndLastRectCollector::AddRect(const nsRect& aRect) {
  if (!mSeenFirstRect) {
    mSeenFirstRect = true;
    mFirstRect = aRect;
  }

  mLastRect = aRect;
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

  nsFont font = aStyleContext->StyleFont()->mFont;
  // We need to not run font.size through floats when it's large since
  // doing so would be lossy.  Fortunately, in such cases, aInflation is
  // guaranteed to be 1.0f.
  if (aInflation != 1.0f) {
    font.size = NSToCoordRound(font.size * aInflation);
  }
  return pc->DeviceContext()->GetMetricsFor(
                  font, aStyleContext->StyleFont()->mLanguage,
                  fs, tp, *aFontMetrics);
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

static nscoord AddPercents(nsLayoutUtils::IntrinsicWidthType aType,
                           nscoord aCurrent, float aPercent)
{
  nscoord result = aCurrent;
  if (aPercent > 0.0f && aType == nsLayoutUtils::PREF_WIDTH) {
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
GetPercentHeight(const nsStyleCoord& aStyle,
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

  const nsStylePosition *pos = f->StylePosition();
  nscoord h;
  if (!GetAbsoluteCoord(pos->mHeight, h) &&
      !GetPercentHeight(pos->mHeight, f, h)) {
    NS_ASSERTION(pos->mHeight.GetUnit() == eStyleUnit_Auto ||
                 pos->mHeight.HasPercent(),
                 "unknown height unit");
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

    NS_ASSERTION(pos->mHeight.GetUnit() == eStyleUnit_Auto,
                 "Unexpected height unit for viewport or canvas or page-content");
    // For the viewport, canvas, and page-content kids, the percentage
    // basis is just the parent height.
    h = f->GetSize().height;
    if (h == NS_UNCONSTRAINEDSIZE) {
      // We don't have a percentage basis after all
      return false;
    }
  }

  nscoord maxh;
  if (GetAbsoluteCoord(pos->mMaxHeight, maxh) ||
      GetPercentHeight(pos->mMaxHeight, f, maxh)) {
    if (maxh < h)
      h = maxh;
  } else {
    NS_ASSERTION(pos->mMaxHeight.GetUnit() == eStyleUnit_None ||
                 pos->mMaxHeight.HasPercent(),
                 "unknown max-height unit");
  }

  nscoord minh;
  if (GetAbsoluteCoord(pos->mMinHeight, minh) ||
      GetPercentHeight(pos->mMinHeight, f, minh)) {
    if (minh > h)
      h = minh;
  } else {
    NS_ASSERTION(pos->mMinHeight.HasPercent(),
                 "unknown min-height unit");
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
    aResult = aFrame->GetPrefWidth(aRenderingContext);
  else
    aResult = aFrame->GetMinWidth(aRenderingContext);
  return true;
}

#undef  DEBUG_INTRINSIC_WIDTH

#ifdef DEBUG_INTRINSIC_WIDTH
static int32_t gNoiseIndent = 0;
#endif

/* static */ nscoord
nsLayoutUtils::IntrinsicForContainer(nsRenderingContext *aRenderingContext,
                                     nsIFrame *aFrame,
                                     IntrinsicWidthType aType,
                                     uint32_t aFlags)
{
  NS_PRECONDITION(aFrame, "null frame");
  NS_PRECONDITION(aType == MIN_WIDTH || aType == PREF_WIDTH, "bad type");

#ifdef DEBUG_INTRINSIC_WIDTH
  nsFrame::IndentBy(stderr, gNoiseIndent);
  static_cast<nsFrame*>(aFrame)->ListTag(stderr);
  printf_stderr(" %s intrinsic width for container:\n",
         aType == MIN_WIDTH ? "min" : "pref");
#endif

  // If aFrame is a container for font size inflation, then shrink
  // wrapping inside of it should not apply font size inflation.
  AutoMaybeDisableFontInflation an(aFrame);

  nsIFrame::IntrinsicWidthOffsetData offsets =
    aFrame->IntrinsicWidthOffsets(aRenderingContext);

  const nsStylePosition *stylePos = aFrame->StylePosition();
  uint8_t boxSizing = stylePos->mBoxSizing;
  const nsStyleCoord &styleWidth = stylePos->mWidth;
  const nsStyleCoord &styleMinWidth = stylePos->mMinWidth;
  const nsStyleCoord &styleMaxWidth = stylePos->mMaxWidth;

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

  nscoord maxw;
  bool haveFixedMaxWidth = GetAbsoluteCoord(styleMaxWidth, maxw);
  nscoord minw;
  bool haveFixedMinWidth = GetAbsoluteCoord(styleMinWidth, minw);

  // If we have a specified width (or a specified 'min-width' greater
  // than the specified 'max-width', which works out to the same thing),
  // don't even bother getting the frame's intrinsic width, because in
  // this case GetAbsoluteCoord(styleWidth, w) will always succeed, so
  // we'll never need the intrinsic dimensions.
  if (styleWidth.GetUnit() == eStyleUnit_Enumerated &&
      (styleWidth.GetIntValue() == NS_STYLE_WIDTH_MAX_CONTENT ||
       styleWidth.GetIntValue() == NS_STYLE_WIDTH_MIN_CONTENT)) {
    // -moz-fit-content and -moz-available enumerated widths compute intrinsic
    // widths just like auto.
    // For -moz-max-content and -moz-min-content, we handle them like
    // specified widths, but ignore box-sizing.
    boxSizing = NS_STYLE_BOX_SIZING_CONTENT;
  } else if (!styleWidth.ConvertsToLength() &&
             !(haveFixedMinWidth && haveFixedMaxWidth && maxw <= minw)) {
#ifdef DEBUG_INTRINSIC_WIDTH
    ++gNoiseIndent;
#endif
    if (aType == MIN_WIDTH)
      result = aFrame->GetMinWidth(aRenderingContext);
    else
      result = aFrame->GetPrefWidth(aRenderingContext);
#ifdef DEBUG_INTRINSIC_WIDTH
    --gNoiseIndent;
    nsFrame::IndentBy(stderr, gNoiseIndent);
    static_cast<nsFrame*>(aFrame)->ListTag(stderr);
    printf_stderr(" %s intrinsic width from frame is %d.\n",
           aType == MIN_WIDTH ? "min" : "pref", result);
#endif

    // Handle elements with an intrinsic ratio (or size) and a specified
    // height, min-height, or max-height.
    const nsStyleCoord &styleHeight = stylePos->mHeight;
    const nsStyleCoord &styleMinHeight = stylePos->mMinHeight;
    const nsStyleCoord &styleMaxHeight = stylePos->mMaxHeight;
    if (styleHeight.GetUnit() != eStyleUnit_Auto ||
        !(styleMinHeight.GetUnit() == eStyleUnit_Coord &&
          styleMinHeight.GetCoordValue() == 0) ||
        styleMaxHeight.GetUnit() != eStyleUnit_None) {

      nsSize ratio = aFrame->GetIntrinsicRatio();

      if (ratio.height != 0) {
        nscoord heightTakenByBoxSizing = 0;
        switch (boxSizing) {
        case NS_STYLE_BOX_SIZING_BORDER: {
          const nsStyleBorder* styleBorder = aFrame->StyleBorder();
          heightTakenByBoxSizing +=
            styleBorder->GetComputedBorder().TopBottom();
          // fall through
        }
        case NS_STYLE_BOX_SIZING_PADDING: {
          if (!(aFlags & IGNORE_PADDING)) {
            const nsStylePadding* stylePadding = aFrame->StylePadding();
            nscoord pad;
            if (GetAbsoluteCoord(stylePadding->mPadding.GetTop(), pad) ||
                GetPercentHeight(stylePadding->mPadding.GetTop(), aFrame, pad)) {
              heightTakenByBoxSizing += pad;
            }
            if (GetAbsoluteCoord(stylePadding->mPadding.GetBottom(), pad) ||
                GetPercentHeight(stylePadding->mPadding.GetBottom(), aFrame, pad)) {
              heightTakenByBoxSizing += pad;
            }
          }
          // fall through
        }
        case NS_STYLE_BOX_SIZING_CONTENT:
        default:
          break;
        }

        nscoord h;
        if (GetAbsoluteCoord(styleHeight, h) ||
            GetPercentHeight(styleHeight, aFrame, h)) {
          h = std::max(0, h - heightTakenByBoxSizing);
          result =
            NSToCoordRound(h * (float(ratio.width) / float(ratio.height)));
        }

        if (GetAbsoluteCoord(styleMaxHeight, h) ||
            GetPercentHeight(styleMaxHeight, aFrame, h)) {
          h = std::max(0, h - heightTakenByBoxSizing);
          nscoord maxWidth =
            NSToCoordRound(h * (float(ratio.width) / float(ratio.height)));
          if (maxWidth < result)
            result = maxWidth;
        }

        if (GetAbsoluteCoord(styleMinHeight, h) ||
            GetPercentHeight(styleMinHeight, aFrame, h)) {
          h = std::max(0, h - heightTakenByBoxSizing);
          nscoord minWidth =
            NSToCoordRound(h * (float(ratio.width) / float(ratio.height)));
          if (minWidth > result)
            result = minWidth;
        }
      }
    }
  }

  if (aFrame->GetType() == nsGkAtoms::tableFrame) {
    // Tables can't shrink smaller than their intrinsic minimum width,
    // no matter what.
    min = aFrame->GetMinWidth(aRenderingContext);
  }

  // We also need to track what has been added on outside of the box
  // (controlled by 'box-sizing') where 'width', 'min-width' and
  // 'max-width' are applied.  We have to account for these properties
  // after getting all the offsets (margin, border, padding) because
  // percentages do not operate linearly.
  // Doing this is ok because although percentages aren't handled
  // linearly, they are handled monotonically.
  nscoord coordOutsideWidth = 0;
  float pctOutsideWidth = 0;
  float pctTotal = 0.0f;

  if (!(aFlags & IGNORE_PADDING)) {
    coordOutsideWidth += offsets.hPadding;
    pctOutsideWidth += offsets.hPctPadding;

    if (boxSizing == NS_STYLE_BOX_SIZING_PADDING) {
      min += coordOutsideWidth;
      result = NSCoordSaturatingAdd(result, coordOutsideWidth);
      pctTotal += pctOutsideWidth;

      coordOutsideWidth = 0;
      pctOutsideWidth = 0.0f;
    }
  }

  coordOutsideWidth += offsets.hBorder;

  if (boxSizing == NS_STYLE_BOX_SIZING_BORDER) {
    min += coordOutsideWidth;
    result = NSCoordSaturatingAdd(result, coordOutsideWidth);
    pctTotal += pctOutsideWidth;

    coordOutsideWidth = 0;
    pctOutsideWidth = 0.0f;
  }

  coordOutsideWidth += offsets.hMargin;
  pctOutsideWidth += offsets.hPctMargin;

  min += coordOutsideWidth;
  result = NSCoordSaturatingAdd(result, coordOutsideWidth);
  pctTotal += pctOutsideWidth;

  nscoord w;
  if (GetAbsoluteCoord(styleWidth, w) ||
      GetIntrinsicCoord(styleWidth, aRenderingContext, aFrame,
                        PROP_WIDTH, w)) {
    result = AddPercents(aType, w + coordOutsideWidth, pctOutsideWidth);
  }
  else if (aType == MIN_WIDTH &&
           // The only cases of coord-percent-calc() units that
           // GetAbsoluteCoord didn't handle are percent and calc()s
           // containing percent.
           styleWidth.IsCoordPercentCalcUnit() &&
           aFrame->IsFrameOfType(nsIFrame::eReplaced)) {
    // A percentage width on replaced elements means they can shrink to 0.
    result = 0; // let |min| handle padding/border/margin
  }
  else {
    // NOTE: We could really do a lot better for percents and for some
    // cases of calc() containing percent (certainly including any where
    // the coefficient on the percent is positive and there are no max()
    // expressions).  However, doing better for percents wouldn't be
    // backwards compatible.
    result = AddPercents(aType, result, pctTotal);
  }

  if (haveFixedMaxWidth ||
      GetIntrinsicCoord(styleMaxWidth, aRenderingContext, aFrame,
                        PROP_MAX_WIDTH, maxw)) {
    maxw = AddPercents(aType, maxw + coordOutsideWidth, pctOutsideWidth);
    if (result > maxw)
      result = maxw;
  }

  if (haveFixedMinWidth ||
      GetIntrinsicCoord(styleMinWidth, aRenderingContext, aFrame,
                        PROP_MIN_WIDTH, minw)) {
    minw = AddPercents(aType, minw + coordOutsideWidth, pctOutsideWidth);
    if (result < minw)
      result = minw;
  }

  min = AddPercents(aType, min, pctTotal);
  if (result < min)
    result = min;

  const nsStyleDisplay *disp = aFrame->StyleDisplay();
  if (aFrame->IsThemed(disp)) {
    nsIntSize size(0, 0);
    bool canOverride = true;
    nsPresContext *presContext = aFrame->PresContext();
    presContext->GetTheme()->
      GetMinimumWidgetSize(presContext, aFrame, disp->mAppearance,
                           &size, &canOverride);

    nscoord themeWidth = presContext->DevPixelsToAppUnits(size.width);

    // GMWS() returns a border-box width
    themeWidth += offsets.hMargin;
    themeWidth = AddPercents(aType, themeWidth, offsets.hPctMargin);

    if (themeWidth > result || !canOverride)
      result = themeWidth;
  }

#ifdef DEBUG_INTRINSIC_WIDTH
  nsFrame::IndentBy(stderr, gNoiseIndent);
  static_cast<nsFrame*>(aFrame)->ListTag(stderr);
  printf_stderr(" %s intrinsic width for container is %d twips.\n",
         aType == MIN_WIDTH ? "min" : "pref", result);
#endif

  return result;
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
nsLayoutUtils::ComputeWidthValue(
                 nsRenderingContext* aRenderingContext,
                 nsIFrame*            aFrame,
                 nscoord              aContainingBlockWidth,
                 nscoord              aContentEdgeToBoxSizing,
                 nscoord              aBoxSizingToMarginEdge,
                 const nsStyleCoord&  aCoord)
{
  NS_PRECONDITION(aFrame, "non-null frame expected");
  NS_PRECONDITION(aRenderingContext, "non-null rendering context expected");
  NS_WARN_IF_FALSE(aContainingBlockWidth != NS_UNCONSTRAINEDSIZE,
                   "have unconstrained width; this should only result from "
                   "very large sizes, not attempts at intrinsic width "
                   "calculation");
  NS_PRECONDITION(aContainingBlockWidth >= 0,
                  "width less than zero");

  nscoord result;
  if (aCoord.IsCoordPercentCalcUnit()) {
    result = nsRuleNode::ComputeCoordPercentCalc(aCoord, 
                                                 aContainingBlockWidth);
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
        result = aFrame->GetPrefWidth(aRenderingContext);
        NS_ASSERTION(result >= 0, "width less than zero");
        break;
      case NS_STYLE_WIDTH_MIN_CONTENT:
        result = aFrame->GetMinWidth(aRenderingContext);
        NS_ASSERTION(result >= 0, "width less than zero");
        break;
      case NS_STYLE_WIDTH_FIT_CONTENT:
        {
          nscoord pref = aFrame->GetPrefWidth(aRenderingContext),
                   min = aFrame->GetMinWidth(aRenderingContext),
                  fill = aContainingBlockWidth -
                         (aBoxSizingToMarginEdge + aContentEdgeToBoxSizing);
          result = std::max(min, std::min(pref, fill));
          NS_ASSERTION(result >= 0, "width less than zero");
        }
        break;
      case NS_STYLE_WIDTH_AVAILABLE:
        result = aContainingBlockWidth -
                 (aBoxSizingToMarginEdge + aContentEdgeToBoxSizing);
    }
  }

  return std::max(0, result);
}

/* static */ nscoord
nsLayoutUtils::ComputeHeightDependentValue(
                 nscoord              aContainingBlockHeight,
                 const nsStyleCoord&  aCoord)
{
  // XXXldb Some callers explicitly check aContainingBlockHeight
  // against NS_AUTOHEIGHT *and* unit against eStyleUnit_Percent or
  // calc()s containing percents before calling this function.
  // However, it would be much more likely to catch problems without
  // the unit conditions.
  // XXXldb Many callers pass a non-'auto' containing block height when
  // according to CSS2.1 they should be passing 'auto'.
  NS_PRECONDITION(NS_AUTOHEIGHT != aContainingBlockHeight ||
                  !aCoord.HasPercent(),
                  "unexpected containing block height");

  if (aCoord.IsCoordPercentCalcUnit()) {
    return nsRuleNode::ComputeCoordPercentCalc(aCoord, aContainingBlockHeight);
  }

  NS_ASSERTION(aCoord.GetUnit() == eStyleUnit_None ||
               aCoord.GetUnit() == eStyleUnit_Auto,
               "unexpected height value");
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

      f->MarkIntrinsicWidthsDirty();

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

#define MULDIV(a,b,c) (nscoord(int64_t(a) * int64_t(b) / int64_t(c)))

/* static */ nsSize
nsLayoutUtils::ComputeSizeWithIntrinsicDimensions(
                   nsRenderingContext* aRenderingContext, nsIFrame* aFrame,
                   const IntrinsicSize& aIntrinsicSize,
                   nsSize aIntrinsicRatio, nsSize aCBSize,
                   nsSize aMargin, nsSize aBorder, nsSize aPadding)
{
  const nsStylePosition* stylePos = aFrame->StylePosition();

  // If we're a flex item, we'll compute our size a bit differently.
  const nsStyleCoord* widthStyleCoord = &(stylePos->mWidth);
  const nsStyleCoord* heightStyleCoord = &(stylePos->mHeight);

  bool isFlexItem = aFrame->IsFlexItem();
  bool isHorizontalFlexItem = false;

  if (isFlexItem) {
    // Flex items use their "flex-basis" property in place of their main-size
    // property (e.g. "width") for sizing purposes, *unless* they have
    // "flex-basis:auto", in which case they use their main-size property after
    // all.
    uint32_t flexDirection =
      aFrame->GetParent()->StylePosition()->mFlexDirection;
    isHorizontalFlexItem =
      flexDirection == NS_STYLE_FLEX_DIRECTION_ROW ||
      flexDirection == NS_STYLE_FLEX_DIRECTION_ROW_REVERSE;

    // NOTE: The logic here should match the similar chunk for determining
    // widthStyleCoord and heightStyleCoord in nsFrame::ComputeSize().
    const nsStyleCoord* flexBasis = &(stylePos->mFlexBasis);
    if (flexBasis->GetUnit() != eStyleUnit_Auto) {
      if (isHorizontalFlexItem) {
        widthStyleCoord = flexBasis;
      } else {
        // One caveat for vertical flex items: We don't support enumerated
        // values (e.g. "max-content") for height properties yet. So, if our
        // computed flex-basis is an enumerated value, we'll just behave as if
        // it were "auto", which means "use the main-size property after all"
        // (which is "height", in this case).
        // NOTE: Once we support intrinsic sizing keywords for "height",
        // we should remove this check.
        if (flexBasis->GetUnit() != eStyleUnit_Enumerated) {
          heightStyleCoord = flexBasis;
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

  const bool isAutoWidth = widthStyleCoord->GetUnit() == eStyleUnit_Auto;
  const bool isAutoHeight = IsAutoHeight(*heightStyleCoord, aCBSize.height);

  nsSize boxSizingAdjust(0,0);
  switch (stylePos->mBoxSizing) {
    case NS_STYLE_BOX_SIZING_BORDER:
      boxSizingAdjust += aBorder;
      // fall through
    case NS_STYLE_BOX_SIZING_PADDING:
      boxSizingAdjust += aPadding;
  }
  nscoord boxSizingToMarginEdgeWidth =
    aMargin.width + aBorder.width + aPadding.width - boxSizingAdjust.width;

  nscoord width, minWidth, maxWidth, height, minHeight, maxHeight;

  if (!isAutoWidth) {
    width = nsLayoutUtils::ComputeWidthValue(aRenderingContext,
              aFrame, aCBSize.width, boxSizingAdjust.width,
              boxSizingToMarginEdgeWidth, *widthStyleCoord);
  }

  if (stylePos->mMaxWidth.GetUnit() != eStyleUnit_None &&
      !(isFlexItem && isHorizontalFlexItem)) {
    maxWidth = nsLayoutUtils::ComputeWidthValue(aRenderingContext,
                 aFrame, aCBSize.width, boxSizingAdjust.width,
                 boxSizingToMarginEdgeWidth, stylePos->mMaxWidth);
  } else {
    // NOTE: Flex items ignore their min & max sizing properties in their
    // flex container's main-axis.  (Those properties get applied later in
    // the flexbox algorithm.)
    maxWidth = nscoord_MAX;
  }

  if (!(isFlexItem && isHorizontalFlexItem)) {
    minWidth = nsLayoutUtils::ComputeWidthValue(aRenderingContext,
                 aFrame, aCBSize.width, boxSizingAdjust.width,
                 boxSizingToMarginEdgeWidth, stylePos->mMinWidth);
  } else {
    // NOTE: Flex items ignore their min & max sizing properties in their
    // flex container's main-axis.  (Those properties get applied later in
    // the flexbox algorithm.)
    minWidth = 0;
  }

  if (!isAutoHeight) {
    height = nsLayoutUtils::ComputeHeightValue(aCBSize.height, 
                boxSizingAdjust.height, 
                *heightStyleCoord);
  }

  if (!IsAutoHeight(stylePos->mMaxHeight, aCBSize.height) &&
      !(isFlexItem && !isHorizontalFlexItem)) {
    maxHeight = nsLayoutUtils::ComputeHeightValue(aCBSize.height, 
                  boxSizingAdjust.height, 
                  stylePos->mMaxHeight);
  } else {
    maxHeight = nscoord_MAX;
  }

  if (!IsAutoHeight(stylePos->mMinHeight, aCBSize.height) &&
      !(isFlexItem && !isHorizontalFlexItem)) {
    minHeight = nsLayoutUtils::ComputeHeightValue(aCBSize.height, 
                  boxSizingAdjust.height,
                  stylePos->mMinHeight);
  } else {
    minHeight = 0;
  }

  // Resolve percentage intrinsic width/height as necessary:

  NS_ASSERTION(aCBSize.width != NS_UNCONSTRAINEDSIZE,
               "Our containing block must not have unconstrained width!");

  bool hasIntrinsicWidth, hasIntrinsicHeight;
  nscoord intrinsicWidth, intrinsicHeight;

  if (aIntrinsicSize.width.GetUnit() == eStyleUnit_Coord) {
    hasIntrinsicWidth = true;
    intrinsicWidth = aIntrinsicSize.width.GetCoordValue();
    if (intrinsicWidth < 0)
      intrinsicWidth = 0;
  } else {
    NS_ASSERTION(aIntrinsicSize.width.GetUnit() == eStyleUnit_None,
                 "unexpected unit");
    hasIntrinsicWidth = false;
    intrinsicWidth = 0;
  }

  if (aIntrinsicSize.height.GetUnit() == eStyleUnit_Coord) {
    hasIntrinsicHeight = true;
    intrinsicHeight = aIntrinsicSize.height.GetCoordValue();
    if (intrinsicHeight < 0)
      intrinsicHeight = 0;
  } else {
    NS_ASSERTION(aIntrinsicSize.height.GetUnit() == eStyleUnit_None,
                 "unexpected unit");
    hasIntrinsicHeight = false;
    intrinsicHeight = 0;
  }

  NS_ASSERTION(aIntrinsicRatio.width >= 0 && aIntrinsicRatio.height >= 0,
               "Intrinsic ratio has a negative component!");

  // Now calculate the used values for width and height:

  if (isAutoWidth) {
    if (isAutoHeight) {

      // 'auto' width, 'auto' height

      // Get tentative values - CSS 2.1 sections 10.3.2 and 10.6.2:

      nscoord tentWidth, tentHeight;

      if (hasIntrinsicWidth) {
        tentWidth = intrinsicWidth;
      } else if (hasIntrinsicHeight && aIntrinsicRatio.height > 0) {
        tentWidth = MULDIV(intrinsicHeight, aIntrinsicRatio.width, aIntrinsicRatio.height);
      } else if (aIntrinsicRatio.width > 0) {
        tentWidth = aCBSize.width - boxSizingToMarginEdgeWidth; // XXX scrollbar?
        if (tentWidth < 0) tentWidth = 0;
      } else {
        tentWidth = nsPresContext::CSSPixelsToAppUnits(300);
      }

      if (hasIntrinsicHeight) {
        tentHeight = intrinsicHeight;
      } else if (aIntrinsicRatio.width > 0) {
        tentHeight = MULDIV(tentWidth, aIntrinsicRatio.height, aIntrinsicRatio.width);
      } else {
        tentHeight = nsPresContext::CSSPixelsToAppUnits(150);
      }

      return ComputeAutoSizeWithIntrinsicDimensions(minWidth, minHeight,
                                                    maxWidth, maxHeight,
                                                    tentWidth, tentHeight);
    } else {

      // 'auto' width, non-'auto' height
      height = NS_CSS_MINMAX(height, minHeight, maxHeight);
      if (aIntrinsicRatio.height > 0) {
        width = MULDIV(height, aIntrinsicRatio.width, aIntrinsicRatio.height);
      } else if (hasIntrinsicWidth) {
        width = intrinsicWidth;
      } else {
        width = nsPresContext::CSSPixelsToAppUnits(300);
      }
      width = NS_CSS_MINMAX(width, minWidth, maxWidth);

    }
  } else {
    if (isAutoHeight) {

      // non-'auto' width, 'auto' height
      width = NS_CSS_MINMAX(width, minWidth, maxWidth);
      if (aIntrinsicRatio.width > 0) {
        height = MULDIV(width, aIntrinsicRatio.height, aIntrinsicRatio.width);
      } else if (hasIntrinsicHeight) {
        height = intrinsicHeight;
      } else {
        height = nsPresContext::CSSPixelsToAppUnits(150);
      }
      height = NS_CSS_MINMAX(height, minHeight, maxHeight);

    } else {

      // non-'auto' width, non-'auto' height
      width = NS_CSS_MINMAX(width, minWidth, maxWidth);
      height = NS_CSS_MINMAX(height, minHeight, maxHeight);

    }
  }

  return nsSize(width, height);
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
    heightAtMaxWidth = MULDIV(maxWidth, tentHeight, tentWidth);
    if (heightAtMaxWidth < minHeight)
      heightAtMaxWidth = minHeight;
    heightAtMinWidth = MULDIV(minWidth, tentHeight, tentWidth);
    if (heightAtMinWidth > maxHeight)
      heightAtMinWidth = maxHeight;
  } else {
    heightAtMaxWidth = heightAtMinWidth = NS_CSS_MINMAX(tentHeight, minHeight, maxHeight);
  }

  if (tentHeight > 0) {
    widthAtMaxHeight = MULDIV(maxHeight, tentWidth, tentHeight);
    if (widthAtMaxHeight < minWidth)
      widthAtMaxHeight = minWidth;
    widthAtMinHeight = MULDIV(minHeight, tentWidth, tentHeight);
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
nsLayoutUtils::MinWidthFromInline(nsIFrame* aFrame,
                                  nsRenderingContext* aRenderingContext)
{
  NS_ASSERTION(!aFrame->IsContainerForFontSizeInflation(),
               "should not be container for font size inflation");

  nsIFrame::InlineMinWidthData data;
  DISPLAY_MIN_WIDTH(aFrame, data.prevLines);
  aFrame->AddInlineMinWidth(aRenderingContext, &data);
  data.ForceBreak(aRenderingContext);
  return data.prevLines;
}

/* static */ nscoord
nsLayoutUtils::PrefWidthFromInline(nsIFrame* aFrame,
                                   nsRenderingContext* aRenderingContext)
{
  NS_ASSERTION(!aFrame->IsContainerForFontSizeInflation(),
               "should not be container for font size inflation");

  nsIFrame::InlinePrefWidthData data;
  DISPLAY_PREF_WIDTH(aFrame, data.prevLines);
  aFrame->AddInlinePrefWidth(aRenderingContext, &data);
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

void
nsLayoutUtils::DrawString(const nsIFrame*       aFrame,
                          nsRenderingContext*   aContext,
                          const char16_t*      aString,
                          int32_t               aLength,
                          nsPoint               aPoint,
                          nsStyleContext*       aStyleContext)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsPresContext* presContext = aFrame->PresContext();
  if (presContext->BidiEnabled()) {
    nsBidiLevel level =
      nsBidiPresUtils::BidiLevelFromStyle(aStyleContext ?
                                          aStyleContext : aFrame->StyleContext());
    rv = nsBidiPresUtils::RenderText(aString, aLength, level,
                                     presContext, *aContext, *aContext,
                                     aPoint.x, aPoint.y);
  }
  if (NS_FAILED(rv))
  {
    aContext->SetTextRunRTL(false);
    aContext->DrawString(aString, aLength, aPoint.x, aPoint.y);
  }
}

nscoord
nsLayoutUtils::GetStringWidth(const nsIFrame*      aFrame,
                              nsRenderingContext* aContext,
                              const char16_t*     aString,
                              int32_t              aLength)
{
  nsPresContext* presContext = aFrame->PresContext();
  if (presContext->BidiEnabled()) {
    nsBidiLevel level =
      nsBidiPresUtils::BidiLevelFromStyle(aFrame->StyleContext());
    return nsBidiPresUtils::MeasureTextWidth(aString, aLength,
                                             level, presContext, *aContext);
  }
  aContext->SetTextRunRTL(false);
  return aContext->GetWidth(aString, aLength);
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
    nsRefPtr<nsRenderingContext> renderingContext = new nsRenderingContext();
    renderingContext->Init(presCtx->DeviceContext(), shadowContext);

    aDestCtx->Save();
    aDestCtx->NewPath();
    aDestCtx->SetColor(gfxRGBA(shadowColor));

    // The callback will draw whatever we want to blur as a shadow.
    aCallback(renderingContext, shadowOffset, shadowColor, aCallbackData);

    contextBoxBlur.DoPaint();
    aDestCtx->Restore();
  }
}

/* static */ nscoord
nsLayoutUtils::GetCenteredFontBaseline(nsFontMetrics* aFontMetrics,
                                       nscoord         aLineHeight)
{
  nscoord fontAscent = aFontMetrics->MaxAscent();
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
    if (layer->IsPositioned() ||
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
  // A transform from either device space or user space (depending on mResetCTM)
  // to image space
  gfxMatrix mUserSpaceToImageSpace;
  // A device-space, pixel-aligned rectangle to fill
  gfxRect mFillRect;
  // A pixel rectangle in tiled image space outside of which gfx should not
  // sample (using EXTEND_PAD as necessary)
  nsIntRect mSubimage;
  // Whether there's anything to draw at all
  bool mShouldDraw;
  // true iff the CTM of the rendering context needs to be reset to the
  // identity matrix before drawing
  bool mResetCTM;

  SnappedImageDrawingParameters()
   : mShouldDraw(false)
   , mResetCTM(false)
  {}

  SnappedImageDrawingParameters(const gfxMatrix& aUserSpaceToImageSpace,
                                const gfxRect&   aFillRect,
                                const nsIntRect& aSubimage,
                                bool             aResetCTM)
   : mUserSpaceToImageSpace(aUserSpaceToImageSpace)
   , mFillRect(aFillRect)
   , mSubimage(aSubimage)
   , mShouldDraw(true)
   , mResetCTM(aResetCTM)
  {}
};

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
                                     const nsIntSize aImageSize)

{
  if (aDest.IsEmpty() || aFill.IsEmpty() || !aImageSize.width || !aImageSize.height)
    return SnappedImageDrawingParameters();

  gfxRect devPixelDest =
    nsLayoutUtils::RectToGfxRect(aDest, aAppUnitsPerDevPixel);
  gfxRect devPixelFill =
    nsLayoutUtils::RectToGfxRect(aFill, aAppUnitsPerDevPixel);
  gfxRect devPixelDirty =
    nsLayoutUtils::RectToGfxRect(aDirty, aAppUnitsPerDevPixel);

  gfxMatrix currentMatrix = aCtx->CurrentMatrix();
  gfxRect fill = devPixelFill;
  bool didSnap;
  // Snap even if we have a scale in the context. But don't snap if
  // we have something that's not translation+scale, or if the scale flips in
  // the X or Y direction, because snapped image drawing can't handle that yet.
  if (!currentMatrix.HasNonAxisAlignedTransform() &&
      currentMatrix._11 > 0.0 && currentMatrix._22 > 0.0 &&
      aCtx->UserToDevicePixelSnapped(fill, true)) {
    didSnap = true;
    if (fill.IsEmpty()) {
      return SnappedImageDrawingParameters();
    }
  } else {
    didSnap = false;
    fill = devPixelFill;
  }

  gfxSize imageSize(aImageSize.width, aImageSize.height);

  // Compute the set of pixels that would be sampled by an ideal rendering
  gfxPoint subimageTopLeft =
    MapToFloatImagePixels(imageSize, devPixelDest, devPixelFill.TopLeft());
  gfxPoint subimageBottomRight =
    MapToFloatImagePixels(imageSize, devPixelDest, devPixelFill.BottomRight());
  nsIntRect intSubimage;
  intSubimage.MoveTo(NSToIntFloor(subimageTopLeft.x),
                     NSToIntFloor(subimageTopLeft.y));
  intSubimage.SizeTo(NSToIntCeil(subimageBottomRight.x) - intSubimage.x,
                     NSToIntCeil(subimageBottomRight.y) - intSubimage.y);

  // Compute the anchor point and compute final fill rect.
  // This code assumes that pixel-based devices have one pixel per
  // device unit!
  gfxPoint anchorPoint(gfxFloat(aAnchor.x)/aAppUnitsPerDevPixel,
                       gfxFloat(aAnchor.y)/aAppUnitsPerDevPixel);
  gfxPoint imageSpaceAnchorPoint =
    MapToFloatImagePixels(imageSize, devPixelDest, anchorPoint);

  if (didSnap) {
    imageSpaceAnchorPoint.Round();
    anchorPoint = imageSpaceAnchorPoint;
    anchorPoint = MapToFloatUserPixels(imageSize, devPixelDest, anchorPoint);
    anchorPoint = currentMatrix.Transform(anchorPoint);
    anchorPoint.Round();

    // This form of Transform is safe to call since non-axis-aligned
    // transforms wouldn't be snapped.
    devPixelDirty = currentMatrix.Transform(devPixelDirty);
  }

  gfxFloat scaleX = imageSize.width*aAppUnitsPerDevPixel/aDest.width;
  gfxFloat scaleY = imageSize.height*aAppUnitsPerDevPixel/aDest.height;
  if (didSnap) {
    // We'll reset aCTX to the identity matrix before drawing, so we need to
    // adjust our scales to match.
    scaleX /= currentMatrix._11;
    scaleY /= currentMatrix._22;
  }
  gfxFloat translateX = imageSpaceAnchorPoint.x - anchorPoint.x*scaleX;
  gfxFloat translateY = imageSpaceAnchorPoint.y - anchorPoint.y*scaleY;
  gfxMatrix transform(scaleX, 0, 0, scaleY, translateX, translateY);

  gfxRect finalFillRect = fill;
  // If the user-space-to-image-space transform is not a straight
  // translation by integers, then filtering will occur, and
  // restricting the fill rect to the dirty rect would change the values
  // computed for edge pixels, which we can't allow.
  // Also, if didSnap is false then rounding out 'devPixelDirty' might not
  // produce pixel-aligned coordinates, which would also break the values
  // computed for edge pixels.
  if (didSnap && !transform.HasNonIntegerTranslation()) {
    devPixelDirty.RoundOut();
    finalFillRect = fill.Intersect(devPixelDirty);
  }
  if (finalFillRect.IsEmpty())
    return SnappedImageDrawingParameters();

  return SnappedImageDrawingParameters(transform, finalFillRect, intSubimage,
                                       didSnap);
}


static nsresult
DrawImageInternal(nsRenderingContext*    aRenderingContext,
                  nsPresContext*         aPresContext,
                  imgIContainer*         aImage,
                  GraphicsFilter         aGraphicsFilter,
                  const nsRect&          aDest,
                  const nsRect&          aFill,
                  const nsPoint&         aAnchor,
                  const nsRect&          aDirty,
                  const nsIntSize&       aImageSize,
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
  gfxContext* ctx = aRenderingContext->ThebesContext();

  SnappedImageDrawingParameters drawingParams =
    ComputeSnappedImageDrawingParameters(ctx, appUnitsPerDevPixel, aDest, aFill,
                                         aAnchor, aDirty, aImageSize);

  if (!drawingParams.mShouldDraw)
    return NS_OK;

  gfxContextMatrixAutoSaveRestore saveMatrix(ctx);
  if (drawingParams.mResetCTM) {
    ctx->IdentityMatrix();
  }

  aImage->Draw(ctx, aGraphicsFilter, drawingParams.mUserSpaceToImageSpace,
               drawingParams.mFillRect, drawingParams.mSubimage, aImageSize,
               aSVGContext, imgIContainer::FRAME_CURRENT, aImageFlags);
  return NS_OK;
}

/* static */ void
nsLayoutUtils::DrawPixelSnapped(nsRenderingContext* aRenderingContext,
                                nsPresContext*       aPresContext,
                                gfxDrawable*         aDrawable,
                                GraphicsFilter       aFilter,
                                const nsRect&        aDest,
                                const nsRect&        aFill,
                                const nsPoint&       aAnchor,
                                const nsRect&        aDirty)
{
  int32_t appUnitsPerDevPixel =
    aPresContext->AppUnitsPerDevPixel();
  gfxContext* ctx = aRenderingContext->ThebesContext();
  gfxIntSize drawableSize = aDrawable->Size();
  nsIntSize imageSize(drawableSize.width, drawableSize.height);

  SnappedImageDrawingParameters drawingParams =
    ComputeSnappedImageDrawingParameters(ctx, appUnitsPerDevPixel, aDest, aFill,
                                         aAnchor, aDirty, imageSize);

  if (!drawingParams.mShouldDraw)
    return;

  gfxContextMatrixAutoSaveRestore saveMatrix(ctx);
  if (drawingParams.mResetCTM) {
    ctx->IdentityMatrix();
  }

  gfxRect sourceRect =
    drawingParams.mUserSpaceToImageSpace.Transform(drawingParams.mFillRect);
  gfxRect imageRect(0, 0, imageSize.width, imageSize.height);
  gfxRect subimage(drawingParams.mSubimage.x, drawingParams.mSubimage.y,
                   drawingParams.mSubimage.width, drawingParams.mSubimage.height);

  NS_ASSERTION(!sourceRect.Intersect(subimage).IsEmpty(),
               "We must be allowed to sample *some* source pixels!");

  gfxUtils::DrawPixelSnapped(ctx, aDrawable,
                             drawingParams.mUserSpaceToImageSpace, subimage,
                             sourceRect, imageRect, drawingParams.mFillRect,
                             gfx::SurfaceFormat::B8G8R8A8, aFilter);
}

/* static */ nsresult
nsLayoutUtils::DrawSingleUnscaledImage(nsRenderingContext* aRenderingContext,
                                       nsPresContext*       aPresContext,
                                       imgIContainer*       aImage,
                                       GraphicsFilter       aGraphicsFilter,
                                       const nsPoint&       aDest,
                                       const nsRect*        aDirty,
                                       uint32_t             aImageFlags,
                                       const nsRect*        aSourceArea)
{
  nsIntSize imageSize;
  aImage->GetWidth(&imageSize.width);
  aImage->GetHeight(&imageSize.height);
  NS_ENSURE_TRUE(imageSize.width > 0 && imageSize.height > 0, NS_ERROR_FAILURE);

  nscoord appUnitsPerCSSPixel = nsDeviceContext::AppUnitsPerCSSPixel();
  nsSize size(imageSize.width*appUnitsPerCSSPixel,
              imageSize.height*appUnitsPerCSSPixel);

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
  return DrawImageInternal(aRenderingContext, aPresContext,
                           aImage, aGraphicsFilter,
                           dest, fill, aDest, aDirty ? *aDirty : dest,
                           imageSize, nullptr, aImageFlags);
}

/* static */ nsresult
nsLayoutUtils::DrawSingleImage(nsRenderingContext*    aRenderingContext,
                               nsPresContext*         aPresContext,
                               imgIContainer*         aImage,
                               GraphicsFilter         aGraphicsFilter,
                               const nsRect&          aDest,
                               const nsRect&          aDirty,
                               const SVGImageContext* aSVGContext,
                               uint32_t               aImageFlags,
                               const nsRect*          aSourceArea)
{
  nsIntSize imageSize;
  if (aImage->GetType() == imgIContainer::TYPE_VECTOR) {
    // We choose a size for vector images that emulates a raster image which
    // is perfectly sized for the destination rect: each pixel in the image
    // maps exactly to a single pixel on-screen.
    nscoord appUnitsPerDevPx =
      aPresContext->AppUnitsPerDevPixel();
    imageSize.width = NSAppUnitsToIntPixels(aDest.width, appUnitsPerDevPx);
    imageSize.height = NSAppUnitsToIntPixels(aDest.height, appUnitsPerDevPx);
  } else {
    // Raster images have an intrinsic size, so we just use that.
    aImage->GetWidth(&imageSize.width);
    aImage->GetHeight(&imageSize.height);
  }
  NS_ENSURE_TRUE(imageSize.width > 0 && imageSize.height > 0, NS_ERROR_FAILURE);

  nsRect source;
  if (aSourceArea) {
    source = *aSourceArea;
  } else {
    nscoord appUnitsPerCSSPixel = nsDeviceContext::AppUnitsPerCSSPixel();
    source.SizeTo(imageSize.width*appUnitsPerCSSPixel,
                  imageSize.height*appUnitsPerCSSPixel);
  }

  nsRect dest = nsLayoutUtils::GetWholeImageDestination(imageSize, source,
                                                        aDest);
  // Ensure that only a single image tile is drawn. If aSourceArea extends
  // outside the image bounds, we want to honor the aSourceArea-to-aDest
  // transform but we don't want to actually tile the image.
  nsRect fill;
  fill.IntersectRect(aDest, dest);
  return DrawImageInternal(aRenderingContext, aPresContext, aImage,
                           aGraphicsFilter, dest, fill,
                           fill.TopLeft(), aDirty, imageSize, aSVGContext, aImageFlags);
}

/* static */ void
nsLayoutUtils::ComputeSizeForDrawing(imgIContainer *aImage,
                                     nsIntSize&     aImageSize, /*outparam*/
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
    aImageSize = nsIntSize(0, 0);
    aIntrinsicRatio = nsSize(0, 0);
  }
}


/* static */ nsresult
nsLayoutUtils::DrawBackgroundImage(nsRenderingContext* aRenderingContext,
                                   nsPresContext*      aPresContext,
                                   imgIContainer*      aImage,
                                   const nsIntSize&    aImageSize,
                                   GraphicsFilter      aGraphicsFilter,
                                   const nsRect&       aDest,
                                   const nsRect&       aFill,
                                   const nsPoint&      aAnchor,
                                   const nsRect&       aDirty,
                                   uint32_t            aImageFlags)
{
  PROFILER_LABEL("nsLayoutUtils", "DrawBackgroundImage",
    js::ProfileEntry::Category::GRAPHICS);

  if (UseBackgroundNearestFiltering()) {
    aGraphicsFilter = GraphicsFilter::FILTER_NEAREST;
  }

  return DrawImageInternal(aRenderingContext, aPresContext, aImage,
                           aGraphicsFilter,
                           aDest, aFill, aAnchor, aDirty,
                           aImageSize, nullptr, aImageFlags);
}

/* static */ nsresult
nsLayoutUtils::DrawImage(nsRenderingContext* aRenderingContext,
                         nsPresContext*       aPresContext,
                         imgIContainer*       aImage,
                         GraphicsFilter       aGraphicsFilter,
                         const nsRect&        aDest,
                         const nsRect&        aFill,
                         const nsPoint&       aAnchor,
                         const nsRect&        aDirty,
                         uint32_t             aImageFlags)
{
  nsIntSize imageSize;
  nsSize imageRatio;
  bool gotHeight, gotWidth;
  ComputeSizeForDrawing(aImage, imageSize, imageRatio, gotWidth, gotHeight);

  // XXX Dimensionless images shouldn't fall back to filled-area size -- the
  //     caller should provide the image size, a la DrawBackgroundImage.
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

  if (!gotWidth) {
    imageSize.width = nsPresContext::AppUnitsToIntCSSPixels(aFill.width);
  }
  if (!gotHeight) {
    imageSize.height = nsPresContext::AppUnitsToIntCSSPixels(aFill.height);
  }

  return DrawImageInternal(aRenderingContext, aPresContext, aImage,
                           aGraphicsFilter,
                           aDest, aFill, aAnchor, aDirty,
                           imageSize, nullptr, aImageFlags);
}

/* static */ nsRect
nsLayoutUtils::GetWholeImageDestination(const nsIntSize& aWholeImageSize,
                                        const nsRect& aImageSourceArea,
                                        const nsRect& aDestArea)
{
  double scaleX = double(aDestArea.width)/aImageSourceArea.width;
  double scaleY = double(aDestArea.height)/aImageSourceArea.height;
  nscoord destOffsetX = NSToCoordRound(aImageSourceArea.x*scaleX);
  nscoord destOffsetY = NSToCoordRound(aImageSourceArea.y*scaleY);
  nscoord appUnitsPerCSSPixel = nsDeviceContext::AppUnitsPerCSSPixel();
  nscoord wholeSizeX = NSToCoordRound(aWholeImageSize.width*appUnitsPerCSSPixel*scaleX);
  nscoord wholeSizeY = NSToCoordRound(aWholeImageSize.height*appUnitsPerCSSPixel*scaleY);
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
  return result;
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

  mozilla::gfx::IntSize size;
  result.mSourceSurface = container->GetCurrentAsSourceSurface(&size);
  if (!result.mSourceSurface)
    return result;

  if (aTarget) {
    RefPtr<SourceSurface> opt = aTarget->OptimizeSourceSurface(result.mSourceSurface);
    if (opt) {
      result.mSourceSurface = opt;
    }
  }

  result.mCORSUsed = aElement->GetCORSMode() != CORS_NONE;
  result.mSize = ThebesIntSize(size);
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
  for (nsIFrame* f = aFrameList.FirstChild(); f ; f = f->GetNextSibling()) {
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

  Preferences::RegisterCallback(GridEnabledPrefChangeCallback,
                                GRID_ENABLED_PREF_NAME);
  GridEnabledPrefChangeCallback(GRID_ENABLED_PREF_NAME, nullptr);
  Preferences::RegisterCallback(StickyEnabledPrefChangeCallback,
                                STICKY_ENABLED_PREF_NAME);
  StickyEnabledPrefChangeCallback(STICKY_ENABLED_PREF_NAME, nullptr);
  Preferences::RegisterCallback(TextAlignTrueEnabledPrefChangeCallback,
                                TEXT_ALIGN_TRUE_ENABLED_PREF_NAME);
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
  nsIDocument* doc = aElement->GetCurrentDoc();
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
MinimumFontSizeFor(nsPresContext* aPresContext, nscoord aContainerWidth)
{
  nsIPresShell* presShell = aPresContext->PresShell();

  uint32_t emPerLine = presShell->FontSizeInflationEmPerLine();
  uint32_t minTwips = presShell->FontSizeInflationMinTwips();
  if (emPerLine == 0 && minTwips == 0) {
    return 0;
  }

  // Clamp the container width to the device dimensions
  nscoord iFrameWidth = aPresContext->GetVisibleArea().width;
  nscoord effectiveContainerWidth = std::min(iFrameWidth, aContainerWidth);

  nscoord byLine = 0, byInch = 0;
  if (emPerLine != 0) {
    byLine = effectiveContainerWidth / emPerLine;
  }
  if (minTwips != 0) {
    // REVIEW: Is this giving us app units and sizes *not* counting
    // viewport scaling?
    float deviceWidthInches =
      aPresContext->ScreenWidthInchesForFontInflation();
    byInch = NSToCoordRound(effectiveContainerWidth /
                            (deviceWidthInches * 1440 /
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
    // Also, if there is more than one frame corresponding to a single
    // content node, we want the outermost one.
    if (!(f->GetParent() && f->GetParent()->GetContent() == content) &&
        // ignore width/height on inlines since they don't apply
        fType != nsGkAtoms::inlineFrame &&
        // ignore width on radios and checkboxes since we enlarge them and
        // they have width/height in ua.css
        fType != nsGkAtoms::formControlFrame) {
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
  // For now, we're going to use NS_FRAME_IN_CONSTRAINED_HEIGHT, which
  // indicates whether the frame is inside something with a constrained
  // height (propagating down the tree), but the propagation stops when
  // we hit overflow-y: scroll or auto.
  const nsStyleText* styleText = aFrame->StyleText();

  return styleText->mTextSizeAdjust != NS_STYLE_TEXT_SIZE_ADJUST_NONE &&
         !(aFrame->GetStateBits() & NS_FRAME_IN_CONSTRAINED_HEIGHT) &&
         // We also want to disable font inflation for containers that have
         // preformatted text.
         styleText->WhiteSpaceCanWrap(aFrame);
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
                                data->EffectiveWidth());
    }
  }

  NS_ABORT_IF_FALSE(false, "root should always be container");

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
  
  nsRect shadows;
  int32_t A2D = aFrame->PresContext()->AppUnitsPerDevPixel();
  for (uint32_t i = 0; i < boxShadows->Length(); ++i) {
    nsRect tmpRect(nsPoint(0, 0), aFrameSize);
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

/* static */ nsSize
nsLayoutUtils::CalculateCompositionSizeForFrame(nsIFrame* aFrame)
{
  nsSize size(aFrame->GetSize());

  nsPresContext* presContext = aFrame->PresContext();
  nsIPresShell* presShell = presContext->PresShell();

  // See the comments in the code that calculates the root
  // composition bounds in RecordFrameMetrics.
  // TODO: Reuse that code here.
  bool isRootContentDocRootScrollFrame = presContext->IsRootContentDocument()
                                      && aFrame == presShell->GetRootScrollFrame();
  if (isRootContentDocRootScrollFrame) {
    if (nsIFrame* rootFrame = presShell->GetRootFrame()) {
      if (nsView* view = rootFrame->GetView()) {
        nsSize viewSize = view->GetBounds().Size();
        nsIWidget* widget =
#ifdef MOZ_WIDGET_ANDROID
            rootFrame->GetNearestWidget();
#else
            view->GetWidget();
#endif
        if (widget) {
          nsIntRect widgetBounds;
          widget->GetBounds(widgetBounds);
          int32_t auPerDevPixel = presContext->AppUnitsPerDevPixel();
          size = nsSize(widgetBounds.width * auPerDevPixel,
                        widgetBounds.height * auPerDevPixel);
#ifdef MOZ_WIDGET_ANDROID
          if (viewSize.height < size.height) {
            size.height = viewSize.height;
          }
#endif
        } else {
          size = viewSize;
        }
      }
    }
  }

  // Adjust composition bounds for the size of scroll bars.
  nsIScrollableFrame* scrollableFrame = aFrame->GetScrollTargetFrame();
  if (scrollableFrame && !LookAndFeel::GetInt(LookAndFeel::eIntID_UseOverlayScrollbars)) {
    nsMargin margins = scrollableFrame->GetActualScrollbarSizes();
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
    return ViewAs<LayerPixel>(aMetrics.mCompositionBounds.Size(),
                              PixelCastJustification::ParentLayerToLayerForRootComposition)
           / aMetrics.LayersPixelsPerCSSPixel();
  }
  nsPresContext* presContext = aFrame->PresContext();
  LayerSize rootCompositionSize;
  nsPresContext* rootPresContext =
    presContext->GetToplevelContentDocumentPresContext();
  if (!rootPresContext) {
    rootPresContext = presContext->GetRootPresContext();
  }
  nsIPresShell* rootPresShell = nullptr;
  if (rootPresContext) {
    // See the comments in the code that calculates the root
    // composition bounds in RecordFrameMetrics.
    // TODO: Reuse that code here.
    nsIPresShell* rootPresShell = rootPresContext->PresShell();
    if (nsIFrame* rootFrame = rootPresShell->GetRootFrame()) {
      if (nsView* view = rootFrame->GetView()) {
        LayoutDeviceToParentLayerScale parentResolution(
          rootPresShell->GetCumulativeResolution().width
          / rootPresShell->GetResolution().width);
        int32_t rootAUPerDevPixel = rootPresContext->AppUnitsPerDevPixel();
        nsRect viewBounds = view->GetBounds();
        LayerSize viewSize = ViewAs<LayerPixel>(
          (LayoutDeviceRect::FromAppUnits(viewBounds, rootAUPerDevPixel)
           * parentResolution).Size(), PixelCastJustification::ParentLayerToLayerForRootComposition);
        nsIWidget* widget =
#ifdef MOZ_WIDGET_ANDROID
            rootFrame->GetNearestWidget();
#else
            view->GetWidget();
#endif
        if (widget) {
          nsIntRect widgetBounds;
          widget->GetBounds(widgetBounds);
          rootCompositionSize = LayerSize(ViewAs<LayerPixel>(widgetBounds.Size()));
#ifdef MOZ_WIDGET_ANDROID
          if (viewSize.height < rootCompositionSize.height) {
            rootCompositionSize.height = viewSize.height;
          }
#endif
        } else {
          rootCompositionSize = viewSize;
        }
      }
    }
  } else {
    nsIWidget* widget = aFrame->GetNearestWidget();
    nsIntRect widgetBounds;
    widget->GetBounds(widgetBounds);
    rootCompositionSize = LayerSize(ViewAs<LayerPixel>(widgetBounds.Size()));
  }

  // Adjust composition size for the size of scroll bars.
  nsIFrame* rootRootScrollFrame = rootPresShell ? rootPresShell->GetRootScrollFrame() : nullptr;
  nsIScrollableFrame* rootScrollableFrame = nullptr;
  if (rootRootScrollFrame) {
    rootScrollableFrame = rootRootScrollFrame->GetScrollTargetFrame();
  }
  if (rootScrollableFrame && !LookAndFeel::GetInt(LookAndFeel::eIntID_UseOverlayScrollbars)) {
    CSSMargin margins = CSSMargin::FromAppUnits(rootScrollableFrame->GetActualScrollbarSizes());
    // Scrollbars are not subject to scaling, so CSS pixels = layer pixels for them.
    rootCompositionSize.width -= margins.LeftRight();
    rootCompositionSize.height -= margins.TopBottom();
  }

  return rootCompositionSize / aMetrics.LayersPixelsPerCSSPixel();
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
#ifndef MOZ_WIDGET_ANDROID
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
    gfxSize res = aFrame->PresContext()->PresShell()->GetResolution();
    compSize.width = NSToCoordRound(compSize.width / ((float) res.width));
    compSize.height = NSToCoordRound(compSize.height / ((float) res.height));
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

/* static */ bool
nsLayoutUtils::WantSubAPZC()
{
  // TODO Turn this on for inprocess OMTC on all platforms
  bool wantSubAPZC = gfxPrefs::AsyncPanZoomEnabled() &&
                     gfxPrefs::APZSubframeEnabled();
#ifdef MOZ_WIDGET_GONK
  if (XRE_GetProcessType() != GeckoProcessType_Content) {
    wantSubAPZC = false;
  }
#endif
  return wantSubAPZC;
}

/* static */ void
nsLayoutUtils::DoLogTestDataForPaint(nsIPresShell* aPresShell,
                                     ViewID aScrollId,
                                     const std::string& aKey,
                                     const std::string& aValue)
{
  nsRefPtr<LayerManager> lm = aPresShell->GetPresContext()->GetRootPresContext()
      ->GetPresShell()->GetLayerManager();
  if (lm && lm->GetBackendType() == LayersBackend::LAYERS_CLIENT) {
    static_cast<ClientLayerManager*>(lm.get())->LogTestDataForCurrentPaint(aScrollId, aKey, aValue);
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
  if (aFrame->IsContainerForFontSizeInflation()) {
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

}
}

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
