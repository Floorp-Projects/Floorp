/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * structures that represent things to be painted (ordered in z-order),
 * used during painting and hit testing
 */

#include "nsDisplayList.h"

#include <stdint.h>
#include <algorithm>
#include <limits>

#include "gfxContext.h"
#include "gfxUtils.h"
#include "mozilla/DisplayPortUtils.h"
#include "mozilla/Likely.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/dom/RemoteBrowser.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/dom/ServiceWorkerRegistrar.h"
#include "mozilla/dom/ServiceWorkerRegistration.h"
#include "mozilla/dom/SVGElement.h"
#include "mozilla/dom/TouchEvent.h"
#include "mozilla/dom/PerformanceMainThread.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/PresShell.h"
#include "mozilla/ScrollContainerFrame.h"
#include "mozilla/ShapeUtils.h"
#include "mozilla/StaticPrefs_apz.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_layers.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/StaticPrefs_print.h"
#include "mozilla/SVGIntegrationUtils.h"
#include "mozilla/SVGUtils.h"
#include "mozilla/ViewportUtils.h"
#include "nsCSSRendering.h"
#include "nsCSSRenderingGradients.h"
#include "nsCaseTreatment.h"
#include "nsRefreshDriver.h"
#include "nsRegion.h"
#include "nsStyleStructInlines.h"
#include "nsStyleTransformMatrix.h"
#include "nsTransitionManager.h"
#include "gfxMatrix.h"
#include "nsLayoutUtils.h"
#include "nsIFrameInlines.h"
#include "nsStyleConsts.h"
#include "BorderConsts.h"
#include "mozilla/MathAlgorithms.h"

#include "imgIContainer.h"
#include "nsImageFrame.h"
#include "nsSubDocumentFrame.h"
#include "nsViewManager.h"
#include "ImageContainer.h"
#include "nsCanvasFrame.h"
#include "nsSubDocumentFrame.h"
#include "StickyScrollContainer.h"
#include "mozilla/AnimationPerformanceWarning.h"
#include "mozilla/AnimationUtils.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/EffectCompositor.h"
#include "mozilla/EffectSet.h"
#include "mozilla/glean/GleanMetrics.h"
#include "mozilla/HashTable.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/OperatorNewExtensions.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/SVGClipPathFrame.h"
#include "mozilla/SVGMaskFrame.h"
#include "mozilla/SVGObserverUtils.h"
#include "mozilla/Telemetry.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/ViewportFrame.h"
#include "mozilla/gfx/gfxVars.h"
#include "ActiveLayerTracker.h"
#include "nsEscape.h"
#include "nsPrintfCString.h"
#include "UnitTransforms.h"
#include "LayerAnimationInfo.h"
#include "mozilla/EventStateManager.h"
#include "nsCaret.h"
#include "nsDOMTokenList.h"
#include "nsCSSProps.h"
#include "nsTableCellFrame.h"
#include "nsTableColFrame.h"
#include "nsTextFrame.h"
#include "nsTextPaintStyle.h"
#include "nsSliderFrame.h"
#include "nsFocusManager.h"
#include "TextDrawTarget.h"
#include "mozilla/layers/AnimationHelper.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/InputAPZContext.h"
#include "mozilla/layers/RenderRootStateManager.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/TreeTraversal.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/layers/WebRenderMessages.h"
#include "mozilla/layers/WebRenderScrollData.h"

namespace mozilla {

using namespace dom;
using namespace gfx;
using namespace layout;
using namespace layers;
using namespace image;

LazyLogModule sContentDisplayListLog("dl.content");
LazyLogModule sParentDisplayListLog("dl.parent");

LazyLogModule& GetLoggerByProcess() {
  return XRE_IsContentProcess() ? sContentDisplayListLog
                                : sParentDisplayListLog;
}

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
void AssertUniqueItem(nsDisplayItem* aItem) {
  for (nsDisplayItem* i : aItem->Frame()->DisplayItems()) {
    if (i != aItem && !i->HasDeletedFrame() && i->Frame() == aItem->Frame() &&
        i->GetPerFrameKey() == aItem->GetPerFrameKey()) {
      if (i->IsPreProcessedItem() || i->IsPreProcessed()) {
        continue;
      }
      MOZ_DIAGNOSTIC_ASSERT(false, "Duplicate display item!");
    }
  }
}
#endif

bool ShouldBuildItemForEvents(const DisplayItemType aType) {
  return aType == DisplayItemType::TYPE_COMPOSITOR_HITTEST_INFO ||
         (GetDisplayItemFlagsForType(aType) & TYPE_IS_CONTAINER);
}

static bool ItemTypeSupportsHitTesting(const DisplayItemType aType) {
  switch (aType) {
    case DisplayItemType::TYPE_BACKGROUND:
    case DisplayItemType::TYPE_BACKGROUND_COLOR:
    case DisplayItemType::TYPE_THEMED_BACKGROUND:
      return true;
    default:
      return false;
  }
}

void InitializeHitTestInfo(nsDisplayListBuilder* aBuilder,
                           nsPaintedDisplayItem* aItem,
                           const DisplayItemType aType) {
  if (ItemTypeSupportsHitTesting(aType)) {
    aItem->InitializeHitTestInfo(aBuilder);
  }
}

/* static */
already_AddRefed<ActiveScrolledRoot> ActiveScrolledRoot::CreateASRForFrame(
    const ActiveScrolledRoot* aParent,
    ScrollContainerFrame* aScrollContainerFrame, bool aIsRetained) {
  RefPtr<ActiveScrolledRoot> asr;
  if (aIsRetained) {
    asr = aScrollContainerFrame->GetProperty(ActiveScrolledRootCache());
  }

  if (!asr) {
    asr = new ActiveScrolledRoot();

    if (aIsRetained) {
      RefPtr<ActiveScrolledRoot> ref = asr;
      aScrollContainerFrame->SetProperty(ActiveScrolledRootCache(),
                                         ref.forget().take());
    }
  }
  asr->mParent = aParent;
  asr->mScrollContainerFrame = aScrollContainerFrame;
  asr->mDepth = aParent ? aParent->mDepth + 1 : 1;
  asr->mRetained = aIsRetained;

  return asr.forget();
}

/* static */
bool ActiveScrolledRoot::IsAncestor(const ActiveScrolledRoot* aAncestor,
                                    const ActiveScrolledRoot* aDescendant) {
  if (!aAncestor) {
    // nullptr is the root
    return true;
  }
  if (Depth(aAncestor) > Depth(aDescendant)) {
    return false;
  }
  const ActiveScrolledRoot* asr = aDescendant;
  while (asr) {
    if (asr == aAncestor) {
      return true;
    }
    asr = asr->mParent;
  }
  return false;
}

/* static */
bool ActiveScrolledRoot::IsProperAncestor(
    const ActiveScrolledRoot* aAncestor,
    const ActiveScrolledRoot* aDescendant) {
  return aAncestor != aDescendant && IsAncestor(aAncestor, aDescendant);
}

/* static */
nsCString ActiveScrolledRoot::ToString(
    const ActiveScrolledRoot* aActiveScrolledRoot) {
  nsAutoCString str;
  for (const auto* asr = aActiveScrolledRoot; asr; asr = asr->mParent) {
    str.AppendPrintf("<0x%p>", asr->mScrollContainerFrame);
    if (asr->mParent) {
      str.AppendLiteral(", ");
    }
  }
  return std::move(str);
}

ScrollableLayerGuid::ViewID ActiveScrolledRoot::ComputeViewId() const {
  nsIContent* content = mScrollContainerFrame->GetScrolledFrame()->GetContent();
  return nsLayoutUtils::FindOrCreateIDFor(content);
}

ActiveScrolledRoot::~ActiveScrolledRoot() {
  if (mScrollContainerFrame && mRetained) {
    mScrollContainerFrame->RemoveProperty(ActiveScrolledRootCache());
  }
}

static uint64_t AddAnimationsForWebRender(
    nsDisplayItem* aItem, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder,
    const Maybe<LayoutDevicePoint>& aPosition = Nothing()) {
  auto* effects = EffectSet::GetForFrame(aItem->Frame(), aItem->GetType());
  if (!effects || effects->IsEmpty()) {
    // If there is no animation on the nsIFrame, that means
    //  1) we've never created any animations on this frame or
    //  2) the frame was reconstruced or
    //  3) all animations on the frame have finished
    // in such cases we don't need do anything here.
    //
    // Even if there is a WebRenderAnimationData for the display item type on
    // this frame, it's going to be discarded since it's not marked as being
    // used.
    return 0;
  }

  RefPtr<WebRenderAnimationData> animationData =
      aManager->CommandBuilder()
          .CreateOrRecycleWebRenderUserData<WebRenderAnimationData>(aItem);
  AnimationInfo& animationInfo = animationData->GetAnimationInfo();
  nsIFrame* frame = aItem->Frame();
  animationInfo.AddAnimationsForDisplayItem(
      frame, aDisplayListBuilder, aItem, aItem->GetType(),
      aManager->LayerManager(), aPosition);

  // Note that animationsId can be 0 (uninitialized in AnimationInfo) if there
  // are no active animations.
  uint64_t animationsId = animationInfo.GetCompositorAnimationsId();
  if (!animationInfo.GetAnimations().IsEmpty()) {
    OpAddCompositorAnimations anim(
        CompositorAnimations(animationInfo.GetAnimations(), animationsId));
    aManager->WrBridge()->AddWebRenderParentCommand(anim);
    aManager->AddActiveCompositorAnimationId(animationsId);
  } else if (animationsId) {
    aManager->AddCompositorAnimationsIdForDiscard(animationsId);
    animationsId = 0;
  }

  return animationsId;
}

static bool GenerateAndPushTextMask(nsIFrame* aFrame, gfxContext* aContext,
                                    const nsRect& aFillRect,
                                    nsDisplayListBuilder* aBuilder) {
  if (aBuilder->IsForGenerateGlyphMask()) {
    return false;
  }

  SVGObserverUtils::GetAndObserveBackgroundClip(aFrame);

  // The main function of enabling background-clip:text property value.
  // When a nsDisplayBackgroundImage detects "text" bg-clip style, it will call
  // this function to
  // 1. Generate a mask by all descendant text frames
  // 2. Push the generated mask into aContext.

  gfxContext* sourceCtx = aContext;
  LayoutDeviceRect bounds = LayoutDeviceRect::FromAppUnits(
      aFillRect, aFrame->PresContext()->AppUnitsPerDevPixel());

  // Create a mask surface.
  RefPtr<DrawTarget> sourceTarget = sourceCtx->GetDrawTarget();
  RefPtr<DrawTarget> maskDT = sourceTarget->CreateClippedDrawTarget(
      bounds.ToUnknownRect(), SurfaceFormat::A8);
  if (!maskDT || !maskDT->IsValid()) {
    return false;
  }
  gfxContext maskCtx(maskDT, /* aPreserveTransform */ true);
  maskCtx.Multiply(Matrix::Translation(bounds.TopLeft().ToUnknownPoint()));

  // Shade text shape into mask A8 surface.
  nsLayoutUtils::PaintFrame(
      &maskCtx, aFrame, nsRect(nsPoint(0, 0), aFrame->GetSize()),
      NS_RGB(255, 255, 255), nsDisplayListBuilderMode::GenerateGlyph);

  // Push the generated mask into aContext, so that the caller can pop and
  // blend with it.

  Matrix currentMatrix = sourceCtx->CurrentMatrix();
  Matrix invCurrentMatrix = currentMatrix;
  invCurrentMatrix.Invert();

  RefPtr<SourceSurface> maskSurface = maskDT->Snapshot();
  sourceCtx->PushGroupForBlendBack(gfxContentType::COLOR_ALPHA, 1.0,
                                   maskSurface, invCurrentMatrix);

  return true;
}

nsDisplayWrapper* nsDisplayWrapList::CreateShallowCopy(
    nsDisplayListBuilder* aBuilder) {
  const nsDisplayWrapList* wrappedItem = AsDisplayWrapList();
  MOZ_ASSERT(wrappedItem);

  // Create a new nsDisplayWrapList using a copy-constructor. This is done
  // to preserve the information about bounds.
  nsDisplayWrapper* wrapper =
      new (aBuilder) nsDisplayWrapper(aBuilder, *wrappedItem);
  wrapper->SetType(nsDisplayWrapper::ItemType());
  MOZ_ASSERT(wrapper);

  // Set the display list pointer of the new wrapper item to the display list
  // of the wrapped item.
  wrapper->mListPtr = wrappedItem->mListPtr;
  return wrapper;
}

nsDisplayWrapList* nsDisplayListBuilder::MergeItems(
    nsTArray<nsDisplayItem*>& aItems) {
  // For merging, we create a temporary item by cloning the last item of the
  // mergeable items list. This ensures that the temporary item will have the
  // correct frame and bounds.
  nsDisplayWrapList* last = aItems.PopLastElement()->AsDisplayWrapList();
  MOZ_ASSERT(last);
  nsDisplayWrapList* merged = last->Clone(this);
  MOZ_ASSERT(merged);
  AddTemporaryItem(merged);

  // Create nsDisplayWrappers that point to the internal display lists of the
  // items we are merging. These nsDisplayWrappers are added to the display list
  // of the temporary item.
  for (nsDisplayItem* item : aItems) {
    MOZ_ASSERT(item);
    MOZ_ASSERT(merged->CanMerge(item));
    merged->Merge(item);
    MOZ_ASSERT(item->AsDisplayWrapList());
    merged->GetChildren()->AppendToTop(
        static_cast<nsDisplayWrapList*>(item)->CreateShallowCopy(this));
  }

  merged->GetChildren()->AppendToTop(last->CreateShallowCopy(this));

  return merged;
}

// FIXME(emilio): This whole business should ideally not be needed at all, but
// there are a variety of hard-to-deal-with caret invalidation issues, like
// bug 1888583, and caret changes are relatively uncommon, enough that it
// probably isn't worth chasing all them down.
void nsDisplayListBuilder::InvalidateCaretFramesIfNeeded() {
  if (mPaintedCarets.IsEmpty()) {
    return;
  }
  size_t i = mPaintedCarets.Length();
  while (i--) {
    nsCaret* caret = mPaintedCarets[i];
    nsIFrame* oldCaret = caret->GetLastPaintedFrame();
    nsRect caretRect;
    nsIFrame* currentCaret = caret->GetPaintGeometry(&caretRect);
    if (oldCaret == currentCaret) {
      // Keep tracking this caret, it hasn't changed.
      continue;
    }
    if (oldCaret) {
      oldCaret->MarkNeedsDisplayItemRebuild();
    }
    if (currentCaret) {
      currentCaret->MarkNeedsDisplayItemRebuild();
    }
    // If / when we paint this caret, we'll track it again.
    caret->SetLastPaintedFrame(nullptr);
    mPaintedCarets.RemoveElementAt(i);
  }
}

void nsDisplayListBuilder::AutoCurrentActiveScrolledRootSetter::
    SetCurrentActiveScrolledRoot(
        const ActiveScrolledRoot* aActiveScrolledRoot) {
  MOZ_ASSERT(!mUsed);

  // Set the builder's mCurrentActiveScrolledRoot.
  mBuilder->mCurrentActiveScrolledRoot = aActiveScrolledRoot;

  // We also need to adjust the builder's mCurrentContainerASR.
  // mCurrentContainerASR needs to be an ASR that all the container's
  // contents have finite bounds with respect to. If aActiveScrolledRoot
  // is an ancestor ASR of mCurrentContainerASR, that means we need to
  // set mCurrentContainerASR to aActiveScrolledRoot, because otherwise
  // the items that will be created with aActiveScrolledRoot wouldn't
  // have finite bounds with respect to mCurrentContainerASR. There's one
  // exception, in the case where there's a content clip on the builder
  // that is scrolled by a descendant ASR of aActiveScrolledRoot. This
  // content clip will clip all items that are created while this
  // AutoCurrentActiveScrolledRootSetter exists. This means that the items
  // created during our lifetime will have finite bounds with respect to
  // the content clip's ASR, even if the items' actual ASR is an ancestor
  // of that. And it also means that mCurrentContainerASR only needs to be
  // set to the content clip's ASR and not all the way to aActiveScrolledRoot.
  // This case is tested by fixed-pos-scrolled-clip-opacity-layerize.html
  // and fixed-pos-scrolled-clip-opacity-inside-layerize.html.

  // finiteBoundsASR is the leafmost ASR that all items created during
  // object's lifetime have finite bounds with respect to.
  const ActiveScrolledRoot* finiteBoundsASR =
      ActiveScrolledRoot::PickDescendant(mContentClipASR, aActiveScrolledRoot);

  // mCurrentContainerASR is adjusted so that it's still an ancestor of
  // finiteBoundsASR.
  mBuilder->mCurrentContainerASR = ActiveScrolledRoot::PickAncestor(
      mBuilder->mCurrentContainerASR, finiteBoundsASR);

  // If we are entering out-of-flow content inside a CSS filter, mark
  // scroll frames wrt. which the content is fixed as containing such content.
  if (mBuilder->mFilterASR && ActiveScrolledRoot::IsAncestor(
                                  aActiveScrolledRoot, mBuilder->mFilterASR)) {
    for (const ActiveScrolledRoot* asr = mBuilder->mFilterASR;
         asr && asr != aActiveScrolledRoot; asr = asr->mParent) {
      asr->mScrollContainerFrame->SetHasOutOfFlowContentInsideFilter();
    }
  }

  mUsed = true;
}

void nsDisplayListBuilder::AutoCurrentActiveScrolledRootSetter::
    InsertScrollFrame(ScrollContainerFrame* aScrollContainerFrame) {
  MOZ_ASSERT(!mUsed);
  size_t descendantsEndIndex = mBuilder->mActiveScrolledRoots.Length();
  const ActiveScrolledRoot* parentASR = mBuilder->mCurrentActiveScrolledRoot;
  const ActiveScrolledRoot* asr =
      mBuilder->AllocateActiveScrolledRoot(parentASR, aScrollContainerFrame);
  mBuilder->mCurrentActiveScrolledRoot = asr;

  // All child ASRs of parentASR that were created while this
  // AutoCurrentActiveScrolledRootSetter object was on the stack belong to us
  // now. Reparent them to asr.
  for (size_t i = mDescendantsStartIndex; i < descendantsEndIndex; i++) {
    ActiveScrolledRoot* descendantASR = mBuilder->mActiveScrolledRoots[i];
    if (ActiveScrolledRoot::IsAncestor(parentASR, descendantASR)) {
      descendantASR->IncrementDepth();
      if (descendantASR->mParent == parentASR) {
        descendantASR->mParent = asr;
      }
    }
  }

  mUsed = true;
}

nsDisplayListBuilder::AutoContainerASRTracker::AutoContainerASRTracker(
    nsDisplayListBuilder* aBuilder)
    : mBuilder(aBuilder), mSavedContainerASR(aBuilder->mCurrentContainerASR) {
  mBuilder->mCurrentContainerASR = mBuilder->mCurrentActiveScrolledRoot;
}

nsPresContext* nsDisplayListBuilder::CurrentPresContext() {
  return CurrentPresShellState()->mPresShell->GetPresContext();
}

/* static */
nsRect nsDisplayListBuilder::OutOfFlowDisplayData::ComputeVisibleRectForFrame(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
    const nsRect& aVisibleRect, const nsRect& aDirtyRect,
    nsRect* aOutDirtyRect) {
  nsRect visible = aVisibleRect;
  nsRect dirtyRectRelativeToDirtyFrame = aDirtyRect;

  bool inPartialUpdate =
      aBuilder->IsRetainingDisplayList() && aBuilder->IsPartialUpdate();
  if (StaticPrefs::apz_allow_zooming() &&
      DisplayPortUtils::IsFixedPosFrameInDisplayPort(aFrame) &&
      aBuilder->IsPaintingToWindow() && !inPartialUpdate) {
    dirtyRectRelativeToDirtyFrame =
        nsRect(nsPoint(0, 0), aFrame->GetParent()->GetSize());

    // If there's a visual viewport size set, restrict the amount of the
    // fixed-position element we paint to the visual viewport. (In general
    // the fixed-position element can be as large as the layout viewport,
    // which at a high zoom level can cause us to paint too large of an
    // area.)
    PresShell* presShell = aFrame->PresShell();
    if (presShell->IsVisualViewportSizeSet()) {
      dirtyRectRelativeToDirtyFrame =
          nsRect(presShell->GetVisualViewportOffsetRelativeToLayoutViewport(),
                 presShell->GetVisualViewportSize());
      // But if we have a displayport, expand it to the displayport, so
      // that async-scrolling the visual viewport within the layout viewport
      // will not checkerboard.
      if (nsIFrame* rootScrollContainerFrame =
              presShell->GetRootScrollContainerFrame()) {
        nsRect displayport;
        // Note that the displayport here is already in the right coordinate
        // space: it's relative to the scroll port (= layout viewport), but
        // covers the visual viewport with some margins around it, which is
        // exactly what we want.
        if (DisplayPortUtils::GetDisplayPort(
                rootScrollContainerFrame->GetContent(), &displayport,
                DisplayPortOptions().With(ContentGeometryType::Fixed))) {
          dirtyRectRelativeToDirtyFrame = displayport;
        }
      }
    }
    visible = dirtyRectRelativeToDirtyFrame;
    if (StaticPrefs::apz_test_logging_enabled() &&
        presShell->GetDocument()->IsContentDocument()) {
      nsLayoutUtils::LogAdditionalTestData(
          aBuilder, "fixedPosDisplayport",
          ToString(CSSSize::FromAppUnits(visible)));
    }
  }

  *aOutDirtyRect = dirtyRectRelativeToDirtyFrame - aFrame->GetPosition();
  visible -= aFrame->GetPosition();

  nsRect overflowRect = aFrame->InkOverflowRect();

  if (aFrame->IsTransformed() && EffectCompositor::HasAnimationsForCompositor(
                                     aFrame, DisplayItemType::TYPE_TRANSFORM)) {
    /**
     * Add a fuzz factor to the overflow rectangle so that elements only
     * just out of view are pulled into the display list, so they can be
     * prerendered if necessary.
     */
    overflowRect.Inflate(nsPresContext::CSSPixelsToAppUnits(32));
  }

  visible.IntersectRect(visible, overflowRect);
  aOutDirtyRect->IntersectRect(*aOutDirtyRect, overflowRect);

  return visible;
}

nsDisplayListBuilder::Linkifier::Linkifier(nsDisplayListBuilder* aBuilder,
                                           nsIFrame* aFrame,
                                           nsDisplayList* aList)
    : mList(aList) {
  // Find the element that we need to check for link-ness, bailing out if
  // we can't find one.
  Element* elem = Element::FromNodeOrNull(aFrame->GetContent());
  if (!elem) {
    return;
  }

  // If the element has an id and/or name attribute, generate a destination
  // for possible internal linking.
  auto maybeGenerateDest = [&](const nsAtom* aAttr) {
    nsAutoString attrValue;
    elem->GetAttr(aAttr, attrValue);
    if (!attrValue.IsEmpty()) {
      NS_ConvertUTF16toUTF8 dest(attrValue);
      // Ensure that we only emit a given destination once, although there may
      // be multiple frames associated with a given element; we'll simply use
      // the first of them as the target of any links to it.
      // XXX(jfkthame) This prevents emitting duplicate destinations *on the
      // same page*, but does not prevent duplicates on subsequent pages, as
      // each new page is handled by a new temporary DisplayListBuilder. This
      // seems to be harmless in practice, though a bit wasteful of space. To
      // fix, we need to maintain the set of already-seen destinations globally
      // for the print job, rather than attached to the (per-page) builder.
      if (aBuilder->mDestinations.EnsureInserted(dest)) {
        auto* destination = MakeDisplayItem<nsDisplayDestination>(
            aBuilder, aFrame, dest.get(), aFrame->GetRect().TopLeft());
        mList->AppendToTop(destination);
      }
    }
  };

  if (StaticPrefs::print_save_as_pdf_internal_destinations_enabled()) {
    if (elem->HasID()) {
      maybeGenerateDest(nsGkAtoms::id);
    }
    if (elem->HasName()) {
      maybeGenerateDest(nsGkAtoms::name);
    }
  }

  // Links don't nest, so if the builder already has a destination, no need to
  // check for a link element here.
  if (!aBuilder->mLinkURI.IsEmpty() || !aBuilder->mLinkDest.IsEmpty()) {
    return;
  }

  // Check if we have actually found a link.
  if (!elem->IsLink()) {
    return;
  }

  nsCOMPtr<nsIURI> uri = elem->GetHrefURI();
  if (!uri) {
    return;
  }

  // Is it potentially a local (in-document) destination?
  bool hasRef, eqExRef;
  nsIURI* docURI;
  if (StaticPrefs::print_save_as_pdf_internal_destinations_enabled() &&
      NS_SUCCEEDED(uri->GetHasRef(&hasRef)) && hasRef &&
      (docURI = aFrame->PresContext()->Document()->GetDocumentURI()) &&
      NS_SUCCEEDED(uri->EqualsExceptRef(docURI, &eqExRef)) && eqExRef) {
    // Try to get a local destination name. If this fails, we'll leave the
    // mLinkDest string empty, but still try to set mLinkURI below.
    if (NS_FAILED(uri->GetRef(aBuilder->mLinkDest))) {
      aBuilder->mLinkDest.Truncate();
    }
    // The destination name is simply a string; we don't want URL-escaping
    // applied to it.
    if (!aBuilder->mLinkDest.IsEmpty()) {
      NS_UnescapeURL(aBuilder->mLinkDest);
    }
  }

  if (NS_FAILED(uri->GetSpec(aBuilder->mLinkURI))) {
    aBuilder->mLinkURI.Truncate();
  }

  // If we didn't get either kind of destination, we won't try to linkify at
  // this level.
  if (aBuilder->mLinkDest.IsEmpty() && aBuilder->mLinkURI.IsEmpty()) {
    return;
  }

  // Record that we need to reset the builder's state on destruction.
  mBuilderToReset = aBuilder;
}

void nsDisplayListBuilder::Linkifier::MaybeAppendLink(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame) {
  // Note that we may generate a link here even if the constructor bailed out
  // without updating aBuilder->mLinkURI/Dest, because it may have been set by
  // an ancestor that was associated with a link element.
  if (!aBuilder->mLinkURI.IsEmpty() || !aBuilder->mLinkDest.IsEmpty()) {
    auto* link = MakeDisplayItem<nsDisplayLink>(
        aBuilder, aFrame, aBuilder->mLinkDest.get(), aBuilder->mLinkURI.get(),
        aFrame->GetRect());
    mList->AppendToTop(link);
  }
}

uint32_t nsDisplayListBuilder::sPaintSequenceNumber(1);

nsDisplayListBuilder::nsDisplayListBuilder(nsIFrame* aReferenceFrame,
                                           nsDisplayListBuilderMode aMode,
                                           bool aBuildCaret,
                                           bool aRetainingDisplayList)
    : mReferenceFrame(aReferenceFrame),
      mIgnoreScrollFrame(nullptr),
      mCurrentActiveScrolledRoot(nullptr),
      mCurrentContainerASR(nullptr),
      mCurrentFrame(aReferenceFrame),
      mCurrentReferenceFrame(aReferenceFrame),
      mScrollInfoItemsForHoisting(nullptr),
      mFirstClipChainToDestroy(nullptr),
      mTableBackgroundSet(nullptr),
      mCurrentScrollParentId(ScrollableLayerGuid::NULL_SCROLL_ID),
      mCurrentScrollbarTarget(ScrollableLayerGuid::NULL_SCROLL_ID),
      mFilterASR(nullptr),
      mDirtyRect(-1, -1, -1, -1),
      mBuildingExtraPagesForPageNum(0),
      mMode(aMode),
      mContainsBlendMode(false),
      mIsBuildingScrollbar(false),
      mCurrentScrollbarWillHaveLayer(false),
      mBuildCaret(aBuildCaret),
      mRetainingDisplayList(aRetainingDisplayList),
      mPartialUpdate(false),
      mIgnoreSuppression(false),
      mIncludeAllOutOfFlows(false),
      mDescendIntoSubdocuments(true),
      mSelectedFramesOnly(false),
      mAllowMergingAndFlattening(true),
      mInTransform(false),
      mInEventsOnly(false),
      mInFilter(false),
      mInPageSequence(false),
      mIsInChromePresContext(false),
      mSyncDecodeImages(false),
      mIsPaintingToWindow(false),
      mUseHighQualityScaling(false),
      mIsPaintingForWebRender(false),
      mAncestorHasApzAwareEventHandler(false),
      mHaveScrollableDisplayPort(false),
      mWindowDraggingAllowed(false),
      mIsBuildingForPopup(nsLayoutUtils::IsPopup(aReferenceFrame)),
      mForceLayerForScrollParent(false),
      mContainsNonMinimalDisplayPort(false),
      mAsyncPanZoomEnabled(nsLayoutUtils::AsyncPanZoomEnabled(aReferenceFrame)),
      mBuildingInvisibleItems(false),
      mIsBuilding(false),
      mInInvalidSubtree(false),
      mDisablePartialUpdates(false),
      mPartialBuildFailed(false),
      mIsInActiveDocShell(false),
      mBuildAsyncZoomContainer(false),
      mIsRelativeToLayoutViewport(false),
      mUseOverlayScrollbars(false),
      mAlwaysLayerizeScrollbars(false) {
  MOZ_COUNT_CTOR(nsDisplayListBuilder);

  mBuildCompositorHitTestInfo = mAsyncPanZoomEnabled && IsForPainting();

  ShouldRebuildDisplayListDueToPrefChange();

  mUseOverlayScrollbars =
      !!LookAndFeel::GetInt(LookAndFeel::IntID::UseOverlayScrollbars);

  mAlwaysLayerizeScrollbars =
      StaticPrefs::layout_scrollbars_always_layerize_track();

  static_assert(
      static_cast<uint32_t>(DisplayItemType::TYPE_MAX) < (1 << TYPE_BITS),
      "Check TYPE_MAX should not overflow");

  mIsReusingStackingContextItems =
      mRetainingDisplayList && StaticPrefs::layout_display_list_retain_sc();
}

void nsDisplayListBuilder::BeginFrame() {
  nsCSSRendering::BeginFrameTreesLocked();

  mIsPaintingToWindow = false;
  mUseHighQualityScaling = false;
  mIgnoreSuppression = false;
  mInTransform = false;
  mInFilter = false;
  mSyncDecodeImages = false;
}

void nsDisplayListBuilder::AddEffectUpdate(dom::RemoteBrowser* aBrowser,
                                           const dom::EffectsInfo& aUpdate) {
  dom::EffectsInfo update = aUpdate;
  // For printing we create one display item for each page that an iframe
  // appears on, the proper visible rect is the union of all the visible rects
  // we get from each display item.
  nsPresContext* pc =
      mReferenceFrame ? mReferenceFrame->PresContext() : nullptr;
  if (pc && pc->Type() != nsPresContext::eContext_Galley) {
    Maybe<dom::EffectsInfo> existing = mEffectsUpdates.MaybeGet(aBrowser);
    if (existing) {
      // Only the visible rect should differ, the scales should match.
      MOZ_ASSERT(existing->mRasterScale == aUpdate.mRasterScale &&
                 existing->mTransformToAncestorScale ==
                     aUpdate.mTransformToAncestorScale);
      if (existing->mVisibleRect) {
        if (update.mVisibleRect) {
          update.mVisibleRect =
              Some(update.mVisibleRect->Union(*existing->mVisibleRect));
        } else {
          update.mVisibleRect = existing->mVisibleRect;
        }
      }
    }
  }
  mEffectsUpdates.InsertOrUpdate(aBrowser, update);
}

void nsDisplayListBuilder::EndFrame() {
  NS_ASSERTION(!mInInvalidSubtree,
               "Someone forgot to cleanup mInInvalidSubtree!");
  mCurrentContainerASR = nullptr;
  mActiveScrolledRoots.Clear();
  mEffectsUpdates.Clear();
  FreeClipChains();
  FreeTemporaryItems();
  nsCSSRendering::EndFrameTreesLocked();
}

void nsDisplayListBuilder::MarkFrameForDisplay(nsIFrame* aFrame,
                                               const nsIFrame* aStopAtFrame) {
  mFramesMarkedForDisplay.AppendElement(aFrame);
  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetParentOrPlaceholderForCrossDoc(f)) {
    if (f->HasAnyStateBits(NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO)) {
      return;
    }
    f->AddStateBits(NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO);
    if (f == aStopAtFrame) {
      // we've reached a frame that we know will be painted, so we can stop.
      break;
    }
  }
}

void nsDisplayListBuilder::AddFrameMarkedForDisplayIfVisible(nsIFrame* aFrame) {
  mFramesMarkedForDisplayIfVisible.AppendElement(aFrame);
}

static void MarkFrameForDisplayIfVisibleInternal(nsIFrame* aFrame,
                                                 const nsIFrame* aStopAtFrame) {
  for (nsIFrame* f = aFrame; f; f = nsLayoutUtils::GetDisplayListParent(f)) {
    if (f->ForceDescendIntoIfVisible()) {
      return;
    }
    f->SetForceDescendIntoIfVisible(true);

    // This condition must match the condition in
    // nsLayoutUtils::GetParentOrPlaceholderFor which is used by
    // nsLayoutUtils::GetDisplayListParent
    if (f->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW) && !f->GetPrevInFlow()) {
      nsIFrame* parent = f->GetParent();
      if (parent && !parent->ForceDescendIntoIfVisible()) {
        // If the GetDisplayListParent call is going to walk to a placeholder,
        // in rare cases the placeholder might be contained in a different
        // continuation from the oof. So we have to make sure to mark the oofs
        // parent. In the common case this doesn't make us do any extra work,
        // just changes the order in which we visit the frames since walking
        // through placeholders will walk through the parent, and we stop when
        // we find a ForceDescendIntoIfVisible bit set.
        MarkFrameForDisplayIfVisibleInternal(parent, aStopAtFrame);
      }
    }

    if (f == aStopAtFrame) {
      // we've reached a frame that we know will be painted, so we can stop.
      break;
    }
  }
}

void nsDisplayListBuilder::MarkFrameForDisplayIfVisible(
    nsIFrame* aFrame, const nsIFrame* aStopAtFrame) {
  AddFrameMarkedForDisplayIfVisible(aFrame);

  MarkFrameForDisplayIfVisibleInternal(aFrame, aStopAtFrame);
}

void nsDisplayListBuilder::SetIsRelativeToLayoutViewport() {
  mIsRelativeToLayoutViewport = true;
  UpdateShouldBuildAsyncZoomContainer();
}

void nsDisplayListBuilder::ForceLayerForScrollParent() {
  mForceLayerForScrollParent = true;
  mNumActiveScrollframesEncountered++;
}

void nsDisplayListBuilder::UpdateShouldBuildAsyncZoomContainer() {
  const Document* document = mReferenceFrame->PresContext()->Document();
  mBuildAsyncZoomContainer = !mIsRelativeToLayoutViewport &&
                             !document->Fullscreen() &&
                             nsLayoutUtils::AllowZoomingForDocument(document);
}

// Certain prefs may cause display list items to be added or removed when they
// are toggled. In those cases, we need to fully rebuild the display list.
bool nsDisplayListBuilder::ShouldRebuildDisplayListDueToPrefChange() {
  // If we transition between wrapping the RCD-RSF contents into an async
  // zoom container vs. not, we need to rebuild the display list. This only
  // happens when the zooming or container scrolling prefs are toggled
  // (manually by the user, or during test setup).
  bool didBuildAsyncZoomContainer = mBuildAsyncZoomContainer;
  UpdateShouldBuildAsyncZoomContainer();

  bool hadOverlayScrollbarsLastTime = mUseOverlayScrollbars;
  mUseOverlayScrollbars =
      !!LookAndFeel::GetInt(LookAndFeel::IntID::UseOverlayScrollbars);

  bool alwaysLayerizedScrollbarsLastTime = mAlwaysLayerizeScrollbars;
  mAlwaysLayerizeScrollbars =
      StaticPrefs::layout_scrollbars_always_layerize_track();

  if (didBuildAsyncZoomContainer != mBuildAsyncZoomContainer) {
    return true;
  }

  if (hadOverlayScrollbarsLastTime != mUseOverlayScrollbars) {
    return true;
  }

  if (alwaysLayerizedScrollbarsLastTime != mAlwaysLayerizeScrollbars) {
    return true;
  }

  return false;
}

void nsDisplayListBuilder::AddScrollContainerFrameToNotify(
    ScrollContainerFrame* aScrollContainerFrame) {
  mScrollContainerFramesToNotify.insert(aScrollContainerFrame);
}

void nsDisplayListBuilder::NotifyAndClearScrollContainerFrames() {
  for (const auto& it : mScrollContainerFramesToNotify) {
    it->NotifyApzTransaction();
  }
  mScrollContainerFramesToNotify.clear();
}

bool nsDisplayListBuilder::MarkOutOfFlowFrameForDisplay(
    nsIFrame* aDirtyFrame, nsIFrame* aFrame, const nsRect& aVisibleRect,
    const nsRect& aDirtyRect) {
  MOZ_ASSERT(aFrame->GetParent() == aDirtyFrame);
  nsRect dirty;
  nsRect visible = OutOfFlowDisplayData::ComputeVisibleRectForFrame(
      this, aFrame, aVisibleRect, aDirtyRect, &dirty);
  if (!aFrame->HasAnyStateBits(NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO) &&
      visible.IsEmpty()) {
    return false;
  }

  // Only MarkFrameForDisplay if we're dirty. If this is a nested out-of-flow
  // frame, then it will also mark any outer frames to ensure that building
  // reaches the dirty feame.
  if (!dirty.IsEmpty() || aFrame->ForceDescendIntoIfVisible()) {
    MarkFrameForDisplay(aFrame, aDirtyFrame);
  }

  return true;
}

static void UnmarkFrameForDisplay(nsIFrame* aFrame,
                                  const nsIFrame* aStopAtFrame) {
  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetParentOrPlaceholderForCrossDoc(f)) {
    if (!f->HasAnyStateBits(NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO)) {
      return;
    }
    f->RemoveStateBits(NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO);
    if (f == aStopAtFrame) {
      // we've reached a frame that we know will be painted, so we can stop.
      break;
    }
  }
}

static void UnmarkFrameForDisplayIfVisible(nsIFrame* aFrame) {
  for (nsIFrame* f = aFrame; f; f = nsLayoutUtils::GetDisplayListParent(f)) {
    if (!f->ForceDescendIntoIfVisible()) {
      return;
    }
    f->SetForceDescendIntoIfVisible(false);

    // This condition must match the condition in
    // nsLayoutUtils::GetParentOrPlaceholderFor which is used by
    // nsLayoutUtils::GetDisplayListParent
    if (f->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW) && !f->GetPrevInFlow()) {
      nsIFrame* parent = f->GetParent();
      if (parent && parent->ForceDescendIntoIfVisible()) {
        // If the GetDisplayListParent call is going to walk to a placeholder,
        // in rare cases the placeholder might be contained in a different
        // continuation from the oof. So we have to make sure to mark the oofs
        // parent. In the common case this doesn't make us do any extra work,
        // just changes the order in which we visit the frames since walking
        // through placeholders will walk through the parent, and we stop when
        // we find a ForceDescendIntoIfVisible bit set.
        UnmarkFrameForDisplayIfVisible(f);
      }
    }
  }
}

nsDisplayListBuilder::~nsDisplayListBuilder() {
  NS_ASSERTION(mFramesMarkedForDisplay.Length() == 0,
               "All frames should have been unmarked");
  NS_ASSERTION(mFramesWithOOFData.Length() == 0,
               "All OOF data should have been removed");
  NS_ASSERTION(mPresShellStates.Length() == 0,
               "All presshells should have been exited");

  DisplayItemClipChain* c = mFirstClipChainToDestroy;
  while (c) {
    DisplayItemClipChain* next = c->mNextClipChainToDestroy;
    c->DisplayItemClipChain::~DisplayItemClipChain();
    c = next;
  }

  MOZ_COUNT_DTOR(nsDisplayListBuilder);
}

uint32_t nsDisplayListBuilder::GetBackgroundPaintFlags() {
  uint32_t flags = 0;
  if (mSyncDecodeImages) {
    flags |= nsCSSRendering::PAINTBG_SYNC_DECODE_IMAGES;
  }
  if (mIsPaintingToWindow) {
    flags |= nsCSSRendering::PAINTBG_TO_WINDOW;
  }
  if (mUseHighQualityScaling) {
    flags |= nsCSSRendering::PAINTBG_HIGH_QUALITY_SCALING;
  }
  return flags;
}

// TODO(emilio): Maybe unify BackgroundPaintFlags and IamgeRendererFlags.
uint32_t nsDisplayListBuilder::GetImageRendererFlags() const {
  uint32_t flags = 0;
  if (mSyncDecodeImages) {
    flags |= nsImageRenderer::FLAG_SYNC_DECODE_IMAGES;
  }
  if (mIsPaintingToWindow) {
    flags |= nsImageRenderer::FLAG_PAINTING_TO_WINDOW;
  }
  if (mUseHighQualityScaling) {
    flags |= nsImageRenderer::FLAG_HIGH_QUALITY_SCALING;
  }
  return flags;
}

uint32_t nsDisplayListBuilder::GetImageDecodeFlags() const {
  uint32_t flags = imgIContainer::FLAG_ASYNC_NOTIFY;
  if (mSyncDecodeImages) {
    flags |= imgIContainer::FLAG_SYNC_DECODE;
  } else {
    flags |= imgIContainer::FLAG_SYNC_DECODE_IF_FAST;
  }
  if (mIsPaintingToWindow || mUseHighQualityScaling) {
    flags |= imgIContainer::FLAG_HIGH_QUALITY_SCALING;
  }
  return flags;
}

void nsDisplayListBuilder::SubtractFromVisibleRegion(nsRegion* aVisibleRegion,
                                                     const nsRegion& aRegion) {
  if (aRegion.IsEmpty()) {
    return;
  }

  nsRegion tmp;
  tmp.Sub(*aVisibleRegion, aRegion);
  // Don't let *aVisibleRegion get too complex, but don't let it fluff out
  // to its bounds either, which can be very bad (see bug 516740).
  // Do let aVisibleRegion get more complex if by doing so we reduce its
  // area by at least half.
  if (tmp.GetNumRects() <= 15 || tmp.Area() <= aVisibleRegion->Area() / 2) {
    *aVisibleRegion = tmp;
  }
}

nsCaret* nsDisplayListBuilder::GetCaret() {
  RefPtr<nsCaret> caret = CurrentPresShellState()->mPresShell->GetCaret();
  return caret;
}

void nsDisplayListBuilder::IncrementPresShellPaintCount(PresShell* aPresShell) {
  if (mIsPaintingToWindow) {
    aPresShell->IncrementPaintCount();
  }
}

void nsDisplayListBuilder::EnterPresShell(const nsIFrame* aReferenceFrame,
                                          bool aPointerEventsNoneDoc) {
  PresShellState* state = mPresShellStates.AppendElement();
  state->mPresShell = aReferenceFrame->PresShell();
  state->mFirstFrameMarkedForDisplay = mFramesMarkedForDisplay.Length();
  state->mFirstFrameWithOOFData = mFramesWithOOFData.Length();

  ScrollContainerFrame* sf = state->mPresShell->GetRootScrollContainerFrame();
  if (sf && IsInSubdocument()) {
    // We are forcing a rebuild of nsDisplayCanvasBackgroundColor to make sure
    // that the canvas background color will be set correctly, and that only one
    // unscrollable item will be created.
    // This is done to avoid, for example, a case where only scrollbar frames
    // are invalidated - we would skip creating nsDisplayCanvasBackgroundColor
    // and possibly end up with an extra nsDisplaySolidColor item.
    // We skip this for the root document, since we don't want to use
    // MarkFrameForDisplayIfVisible before ComputeRebuildRegion. We'll
    // do it manually there.
    nsCanvasFrame* canvasFrame = do_QueryFrame(sf->GetScrolledFrame());
    if (canvasFrame) {
      MarkFrameForDisplayIfVisible(canvasFrame, aReferenceFrame);
    }
  }

#ifdef DEBUG
  state->mAutoLayoutPhase.emplace(aReferenceFrame->PresContext(),
                                  nsLayoutPhase::DisplayListBuilding);
#endif

  state->mPresShell->UpdateCanvasBackground();

  bool buildCaret = mBuildCaret;
  if (mIgnoreSuppression || !state->mPresShell->IsPaintingSuppressed()) {
    state->mIsBackgroundOnly = false;
  } else {
    state->mIsBackgroundOnly = true;
    buildCaret = false;
  }

  bool pointerEventsNone = aPointerEventsNoneDoc;
  if (IsInSubdocument()) {
    pointerEventsNone |= mPresShellStates[mPresShellStates.Length() - 2]
                             .mInsidePointerEventsNoneDoc;
  }
  state->mInsidePointerEventsNoneDoc = pointerEventsNone;

  state->mPresShellIgnoreScrollFrame =
      state->mPresShell->IgnoringViewportScrolling()
          ? state->mPresShell->GetRootScrollContainerFrame()
          : nullptr;

  nsPresContext* pc = aReferenceFrame->PresContext();
  mIsInChromePresContext = pc->IsChrome();
  nsIDocShell* docShell = pc->GetDocShell();

  if (docShell) {
    docShell->GetWindowDraggingAllowed(&mWindowDraggingAllowed);
  }

  state->mTouchEventPrefEnabledDoc = dom::TouchEvent::PrefEnabled(docShell);

  if (!buildCaret) {
    return;
  }

  state->mCaretFrame = [&]() -> nsIFrame* {
    RefPtr<nsCaret> caret = state->mPresShell->GetCaret();
    nsIFrame* currentCaret = caret->GetPaintGeometry(&mCaretRect);
    if (!currentCaret) {
      return nullptr;
    }

    // Check if the display root for the caret matches the display root that
    // we're painting, and only use it if it matches. Likely we only need this
    // for carets inside popups.
    if (nsLayoutUtils::GetDisplayRootFrame(currentCaret) !=
        nsLayoutUtils::GetDisplayRootFrame(aReferenceFrame)) {
      return nullptr;
    }

    // Caret frames add visual area to their frame, but we don't update the
    // overflow area. Use flags to make sure we build display items for that
    // frame instead.
    MOZ_ASSERT(currentCaret->PresShell() == state->mPresShell);
    MarkFrameForDisplay(currentCaret, aReferenceFrame);
    caret->SetLastPaintedFrame(currentCaret);
    if (!mPaintedCarets.Contains(caret)) {
      mPaintedCarets.AppendElement(std::move(caret));
    }
    return currentCaret;
  }();
}

// A non-blank paint is a paint that does not just contain the canvas
// background.
static bool DisplayListIsNonBlank(nsDisplayList* aList) {
  for (nsDisplayItem* i : *aList) {
    switch (i->GetType()) {
      case DisplayItemType::TYPE_COMPOSITOR_HITTEST_INFO:
      case DisplayItemType::TYPE_CANVAS_BACKGROUND_COLOR:
      case DisplayItemType::TYPE_CANVAS_BACKGROUND_IMAGE:
        continue;
      case DisplayItemType::TYPE_SOLID_COLOR:
      case DisplayItemType::TYPE_BACKGROUND:
      case DisplayItemType::TYPE_BACKGROUND_COLOR:
        if (i->Frame()->IsCanvasFrame()) {
          continue;
        }
        return true;
      default:
        return true;
    }
  }
  return false;
}

// A contentful paint is a paint that does contains DOM content (text,
// images, non-blank canvases, SVG): "First Contentful Paint entry
// contains a DOMHighResTimeStamp reporting the time when the browser
// first rendered any text, image (including background images),
// non-white canvas or SVG. This excludes any content of iframes, but
// includes text with pending webfonts. This is the first time users
// could start consuming page content."
static bool DisplayListIsContentful(nsDisplayListBuilder* aBuilder,
                                    nsDisplayList* aList) {
  for (nsDisplayItem* i : *aList) {
    DisplayItemType type = i->GetType();
    nsDisplayList* children = i->GetChildren();

    switch (type) {
      case DisplayItemType::TYPE_SUBDOCUMENT:  // iframes are ignored
        break;
      // CANVASes check if they may have been modified (as a stand-in
      // actually tracking all modifications)
      default:
        if (i->IsContentful()) {
          bool dummy;
          nsRect bound = i->GetBounds(aBuilder, &dummy);
          if (!bound.IsEmpty()) {
            return true;
          }
        }
        if (children) {
          if (DisplayListIsContentful(aBuilder, children)) {
            return true;
          }
        }
        break;
    }
  }
  return false;
}

void nsDisplayListBuilder::LeavePresShell(const nsIFrame* aReferenceFrame,
                                          nsDisplayList* aPaintedContents) {
  NS_ASSERTION(
      CurrentPresShellState()->mPresShell == aReferenceFrame->PresShell(),
      "Presshell mismatch");

  if (mIsPaintingToWindow && aPaintedContents) {
    nsPresContext* pc = aReferenceFrame->PresContext();
    if (!pc->HadNonBlankPaint()) {
      if (!CurrentPresShellState()->mIsBackgroundOnly &&
          DisplayListIsNonBlank(aPaintedContents)) {
        pc->NotifyNonBlankPaint();
      }
    }
    nsRootPresContext* rootPresContext = pc->GetRootPresContext();
    if (!pc->HasStoppedGeneratingLCP() && rootPresContext) {
      if (!CurrentPresShellState()->mIsBackgroundOnly) {
        if (pc->HasEverBuiltInvisibleText() ||
            DisplayListIsContentful(this, aPaintedContents)) {
          pc->NotifyContentfulPaint();
        }
      }
    }
  }

  ResetMarkedFramesForDisplayList(aReferenceFrame);
  mPresShellStates.RemoveLastElement();

  if (!mPresShellStates.IsEmpty()) {
    nsPresContext* pc = CurrentPresContext();
    nsIDocShell* docShell = pc->GetDocShell();
    if (docShell) {
      docShell->GetWindowDraggingAllowed(&mWindowDraggingAllowed);
    }
    mIsInChromePresContext = pc->IsChrome();
  } else {
    for (uint32_t i = 0; i < mFramesMarkedForDisplayIfVisible.Length(); ++i) {
      UnmarkFrameForDisplayIfVisible(mFramesMarkedForDisplayIfVisible[i]);
    }
    mFramesMarkedForDisplayIfVisible.SetLength(0);
  }
}

void nsDisplayListBuilder::FreeClipChains() {
  // Iterate the clip chains from newest to oldest (forward
  // iteration), so that we destroy descendants first which
  // will drop the ref count on their ancestors.
  DisplayItemClipChain** indirect = &mFirstClipChainToDestroy;

  while (*indirect) {
    if (!(*indirect)->mRefCount) {
      DisplayItemClipChain* next = (*indirect)->mNextClipChainToDestroy;

      mClipDeduplicator.erase(*indirect);
      (*indirect)->DisplayItemClipChain::~DisplayItemClipChain();
      Destroy(DisplayListArenaObjectId::CLIPCHAIN, *indirect);

      *indirect = next;
    } else {
      indirect = &(*indirect)->mNextClipChainToDestroy;
    }
  }
}

void nsDisplayListBuilder::FreeTemporaryItems() {
  for (nsDisplayItem* i : mTemporaryItems) {
    // Temporary display items are not added to the frames.
    MOZ_ASSERT(i->Frame());
    i->RemoveFrame(i->Frame());
    i->Destroy(this);
  }

  mTemporaryItems.Clear();
}

void nsDisplayListBuilder::ResetMarkedFramesForDisplayList(
    const nsIFrame* aReferenceFrame) {
  // Unmark and pop off the frames marked for display in this pres shell.
  uint32_t firstFrameForShell =
      CurrentPresShellState()->mFirstFrameMarkedForDisplay;
  for (uint32_t i = firstFrameForShell; i < mFramesMarkedForDisplay.Length();
       ++i) {
    UnmarkFrameForDisplay(mFramesMarkedForDisplay[i], aReferenceFrame);
  }
  mFramesMarkedForDisplay.SetLength(firstFrameForShell);

  firstFrameForShell = CurrentPresShellState()->mFirstFrameWithOOFData;
  for (uint32_t i = firstFrameForShell; i < mFramesWithOOFData.Length(); ++i) {
    mFramesWithOOFData[i]->RemoveProperty(OutOfFlowDisplayDataProperty());
  }
  mFramesWithOOFData.SetLength(firstFrameForShell);
}

void nsDisplayListBuilder::ClearFixedBackgroundDisplayData() {
  CurrentPresShellState()->mFixedBackgroundDisplayData = Nothing();
}

void nsDisplayListBuilder::MarkFramesForDisplayList(
    nsIFrame* aDirtyFrame, const nsFrameList& aFrames) {
  nsRect visibleRect = GetVisibleRect();
  nsRect dirtyRect = GetDirtyRect();

  // If we are entering content that is fixed to the RCD-RSF, we are
  // crossing the async zoom container boundary, and need to convert from
  // visual to layout coordinates.
  if (ViewportFrame* viewportFrame = do_QueryFrame(aDirtyFrame)) {
    if (IsForEventDelivery() && ShouldBuildAsyncZoomContainer() &&
        viewportFrame->PresContext()->IsRootContentDocumentCrossProcess()) {
      if (viewportFrame->PresShell()->GetRootScrollContainerFrame()) {
#ifdef DEBUG
        for (nsIFrame* f : aFrames) {
          MOZ_ASSERT(ViewportUtils::IsZoomedContentRoot(f));
        }
#endif
        visibleRect = ViewportUtils::VisualToLayout(visibleRect,
                                                    viewportFrame->PresShell());
        dirtyRect = ViewportUtils::VisualToLayout(dirtyRect,
                                                  viewportFrame->PresShell());
      }
#ifdef DEBUG
      else {
        // This is an edge case that should only happen if we are in a
        // document with a XUL root element so that it does not have a root
        // scroll frame but it has fixed pos content and all of the frames in
        // aFrames are that fixed pos content.
        for (nsIFrame* f : aFrames) {
          MOZ_ASSERT(!ViewportUtils::IsZoomedContentRoot(f) &&
                     f->GetParent() == aDirtyFrame &&
                     f->StyleDisplay()->mPosition ==
                         StylePositionProperty::Fixed);
        }
        // There's no root scroll frame so there can't be any zooming or async
        // panning so we don't need to adjust the visible and dirty rects.
      }
#endif
    }
  }

  bool markedFrames = false;
  for (nsIFrame* e : aFrames) {
    // Skip the AccessibleCaret frame when building no caret.
    if (!IsBuildingCaret()) {
      nsIContent* content = e->GetContent();
      if (content && content->IsInNativeAnonymousSubtree() &&
          content->IsElement()) {
        const nsAttrValue* classes = content->AsElement()->GetClasses();
        if (classes &&
            classes->Contains(nsGkAtoms::mozAccessiblecaret, eCaseMatters)) {
          continue;
        }
      }
    }
    if (MarkOutOfFlowFrameForDisplay(aDirtyFrame, e, visibleRect, dirtyRect)) {
      markedFrames = true;
    }
  }

  if (markedFrames) {
    // mClipState.GetClipChainForContainingBlockDescendants can return pointers
    // to objects on the stack, so we need to clone the chain.
    const DisplayItemClipChain* clipChain =
        CopyWholeChain(mClipState.GetClipChainForContainingBlockDescendants());
    const DisplayItemClipChain* combinedClipChain =
        mClipState.GetCurrentCombinedClipChain(this);
    const ActiveScrolledRoot* asr = mCurrentActiveScrolledRoot;

    OutOfFlowDisplayData* data = new OutOfFlowDisplayData(
        clipChain, combinedClipChain, asr, this->mCurrentScrollParentId,
        visibleRect, dirtyRect);
    aDirtyFrame->SetProperty(
        nsDisplayListBuilder::OutOfFlowDisplayDataProperty(), data);
    mFramesWithOOFData.AppendElement(aDirtyFrame);
  }

  if (!aDirtyFrame->GetParent()) {
    // This is the viewport frame of aDirtyFrame's presshell.
    // Store the current display data so that it can be used for fixed
    // background images.
    NS_ASSERTION(
        CurrentPresShellState()->mPresShell == aDirtyFrame->PresShell(),
        "Presshell mismatch");
    MOZ_ASSERT(!CurrentPresShellState()->mFixedBackgroundDisplayData,
               "already traversed this presshell's root frame?");

    const DisplayItemClipChain* clipChain =
        CopyWholeChain(mClipState.GetClipChainForContainingBlockDescendants());
    const DisplayItemClipChain* combinedClipChain =
        mClipState.GetCurrentCombinedClipChain(this);
    const ActiveScrolledRoot* asr = mCurrentActiveScrolledRoot;
    CurrentPresShellState()->mFixedBackgroundDisplayData.emplace(
        clipChain, combinedClipChain, asr, this->mCurrentScrollParentId,
        GetVisibleRect(), GetDirtyRect());
  }
}

/**
 * Mark all preserve-3d children with
 * NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO to make sure
 * nsIFrame::BuildDisplayListForChild() would visit them.  Also compute
 * dirty rect for preserve-3d children.
 *
 * @param aDirtyFrame is the frame to mark children extending context.
 */
void nsDisplayListBuilder::MarkPreserve3DFramesForDisplayList(
    nsIFrame* aDirtyFrame) {
  for (const auto& childList : aDirtyFrame->ChildLists()) {
    for (nsIFrame* child : childList.mList) {
      if (child->Combines3DTransformWithAncestors()) {
        MarkFrameForDisplay(child, aDirtyFrame);
      }

      if (child->IsBlockWrapper()) {
        // Mark preserve-3d frames inside the block wrapper.
        MarkPreserve3DFramesForDisplayList(child);
      }
    }
  }
}

ActiveScrolledRoot* nsDisplayListBuilder::AllocateActiveScrolledRoot(
    const ActiveScrolledRoot* aParent,
    ScrollContainerFrame* aScrollContainerFrame) {
  RefPtr<ActiveScrolledRoot> asr = ActiveScrolledRoot::CreateASRForFrame(
      aParent, aScrollContainerFrame, IsRetainingDisplayList());
  mActiveScrolledRoots.AppendElement(asr);
  return asr;
}

const DisplayItemClipChain* nsDisplayListBuilder::AllocateDisplayItemClipChain(
    const DisplayItemClip& aClip, const ActiveScrolledRoot* aASR,
    const DisplayItemClipChain* aParent) {
  MOZ_DIAGNOSTIC_ASSERT(!(aParent && aParent->mOnStack));
  void* p = Allocate(sizeof(DisplayItemClipChain),
                     DisplayListArenaObjectId::CLIPCHAIN);
  DisplayItemClipChain* c = new (KnownNotNull, p)
      DisplayItemClipChain(aClip, aASR, aParent, mFirstClipChainToDestroy);
#if defined(DEBUG) || defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)
  c->mOnStack = false;
#endif
  auto result = mClipDeduplicator.insert(c);
  if (!result.second) {
    // An equivalent clip chain item was already created, so let's return that
    // instead. Destroy the one we just created.
    // Note that this can cause clip chains from different coordinate systems to
    // collapse into the same clip chain object, because clip chains do not keep
    // track of the reference frame that they were created in.
    c->DisplayItemClipChain::~DisplayItemClipChain();
    Destroy(DisplayListArenaObjectId::CLIPCHAIN, c);
    return *(result.first);
  }
  mFirstClipChainToDestroy = c;
  return c;
}

struct ClipChainItem {
  DisplayItemClip clip;
  const ActiveScrolledRoot* asr;
};

static const DisplayItemClipChain* FindCommonAncestorClipForIntersection(
    const DisplayItemClipChain* aOne, const DisplayItemClipChain* aTwo) {
  for (const ActiveScrolledRoot* asr =
           ActiveScrolledRoot::PickDescendant(aOne->mASR, aTwo->mASR);
       asr; asr = asr->mParent) {
    if (aOne == aTwo) {
      return aOne;
    }
    if (aOne->mASR == asr) {
      aOne = aOne->mParent;
    }
    if (aTwo->mASR == asr) {
      aTwo = aTwo->mParent;
    }
    if (!aOne) {
      return aTwo;
    }
    if (!aTwo) {
      return aOne;
    }
  }
  return nullptr;
}

const DisplayItemClipChain* nsDisplayListBuilder::CreateClipChainIntersection(
    const DisplayItemClipChain* aAncestor,
    const DisplayItemClipChain* aLeafClip1,
    const DisplayItemClipChain* aLeafClip2) {
  AutoTArray<ClipChainItem, 8> intersectedClips;

  const DisplayItemClipChain* clip1 = aLeafClip1;
  const DisplayItemClipChain* clip2 = aLeafClip2;

  const ActiveScrolledRoot* asr = ActiveScrolledRoot::PickDescendant(
      clip1 ? clip1->mASR : nullptr, clip2 ? clip2->mASR : nullptr);

  // Build up the intersection from the leaf to the root and put it into
  // intersectedClips. The loop below will convert intersectedClips into an
  // actual DisplayItemClipChain.
  // (We need to do this in two passes because we need the parent clip in order
  // to create the DisplayItemClipChain object, but the parent clip has not
  // been created at that point.)
  while (!aAncestor || asr != aAncestor->mASR) {
    if (clip1 && clip1->mASR == asr) {
      if (clip2 && clip2->mASR == asr) {
        DisplayItemClip intersection = clip1->mClip;
        intersection.IntersectWith(clip2->mClip);
        intersectedClips.AppendElement(ClipChainItem{intersection, asr});
        clip2 = clip2->mParent;
      } else {
        intersectedClips.AppendElement(ClipChainItem{clip1->mClip, asr});
      }
      clip1 = clip1->mParent;
    } else if (clip2 && clip2->mASR == asr) {
      intersectedClips.AppendElement(ClipChainItem{clip2->mClip, asr});
      clip2 = clip2->mParent;
    }
    if (!asr) {
      MOZ_ASSERT(!aAncestor, "We should have exited this loop earlier");
      break;
    }
    asr = asr->mParent;
  }

  // Convert intersectedClips into a DisplayItemClipChain.
  const DisplayItemClipChain* parentSC = aAncestor;
  for (auto& sc : Reversed(intersectedClips)) {
    parentSC = AllocateDisplayItemClipChain(sc.clip, sc.asr, parentSC);
  }
  return parentSC;
}

const DisplayItemClipChain* nsDisplayListBuilder::CreateClipChainIntersection(
    const DisplayItemClipChain* aLeafClip1,
    const DisplayItemClipChain* aLeafClip2) {
  // aLeafClip2 might be a reference to a clip on the stack. We need to make
  // sure that CreateClipChainIntersection will allocate the actual intersected
  // clip in the builder's arena, so for the aLeafClip1 == nullptr case,
  // we supply nullptr as the common ancestor so that
  // CreateClipChainIntersection clones the whole chain.
  const DisplayItemClipChain* ancestorClip =
      aLeafClip1 ? FindCommonAncestorClipForIntersection(aLeafClip1, aLeafClip2)
                 : nullptr;

  return CreateClipChainIntersection(ancestorClip, aLeafClip1, aLeafClip2);
}

const DisplayItemClipChain* nsDisplayListBuilder::CopyWholeChain(
    const DisplayItemClipChain* aClipChain) {
  return CreateClipChainIntersection(nullptr, aClipChain, nullptr);
}

const nsIFrame* nsDisplayListBuilder::FindReferenceFrameFor(
    const nsIFrame* aFrame, nsPoint* aOffset) const {
  auto MaybeApplyAdditionalOffset = [&]() {
    if (auto offset = AdditionalOffset()) {
      *aOffset += *offset;
    }
  };

  if (aFrame == mCurrentFrame) {
    if (aOffset) {
      *aOffset = mCurrentOffsetToReferenceFrame;
    }
    return mCurrentReferenceFrame;
  }

  for (const nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetCrossDocParentFrameInProcess(f)) {
    if (f == mReferenceFrame || f->IsTransformed()) {
      if (aOffset) {
        *aOffset = aFrame->GetOffsetToCrossDoc(f);
        MaybeApplyAdditionalOffset();
      }
      return f;
    }
  }

  if (aOffset) {
    *aOffset = aFrame->GetOffsetToCrossDoc(mReferenceFrame);
    MaybeApplyAdditionalOffset();
  }

  return mReferenceFrame;
}

// Sticky frames are active if their nearest scrollable frame is also active.
static bool IsStickyFrameActive(nsDisplayListBuilder* aBuilder,
                                nsIFrame* aFrame) {
  MOZ_ASSERT(aFrame->StyleDisplay()->mPosition ==
             StylePositionProperty::Sticky);

  StickyScrollContainer* stickyScrollContainer =
      StickyScrollContainer::GetStickyScrollContainerForFrame(aFrame);
  return stickyScrollContainer && stickyScrollContainer->ScrollContainer()
                                      ->IsMaybeAsynchronouslyScrolled();
}

bool nsDisplayListBuilder::IsAnimatedGeometryRoot(nsIFrame* aFrame,
                                                  nsIFrame** aParent) {
  if (aFrame == mReferenceFrame) {
    return true;
  }

  if (!IsPaintingToWindow()) {
    if (aParent) {
      *aParent = nsLayoutUtils::GetCrossDocParentFrameInProcess(aFrame);
    }
    return false;
  }

  nsIFrame* parent = nsLayoutUtils::GetCrossDocParentFrameInProcess(aFrame);
  if (!parent) {
    return true;
  }
  *aParent = parent;

  if (aFrame->StyleDisplay()->mPosition == StylePositionProperty::Sticky &&
      IsStickyFrameActive(this, aFrame)) {
    return true;
  }

  if (aFrame->IsTransformed()) {
    if (EffectCompositor::HasAnimationsForCompositor(
            aFrame, DisplayItemType::TYPE_TRANSFORM)) {
      return true;
    }
  }

  if (parent->IsScrollContainerOrSubclass()) {
    ScrollContainerFrame* sf = do_QueryFrame(parent);
    if (sf->GetScrolledFrame() == aFrame) {
      MOZ_ASSERT(!aFrame->IsTransformed());
      return sf->IsMaybeAsynchronouslyScrolled();
    }
  }

  return false;
}

nsIFrame* nsDisplayListBuilder::FindAnimatedGeometryRootFrameFor(
    nsIFrame* aFrame) {
  MOZ_ASSERT(nsLayoutUtils::IsAncestorFrameCrossDocInProcess(
      RootReferenceFrame(), aFrame));
  nsIFrame* cursor = aFrame;
  while (cursor != RootReferenceFrame()) {
    nsIFrame* next;
    if (IsAnimatedGeometryRoot(cursor, &next)) {
      return cursor;
    }
    cursor = next;
  }
  return cursor;
}

static nsRect ApplyAllClipNonRoundedIntersection(
    const DisplayItemClipChain* aClipChain, const nsRect& aRect) {
  nsRect result = aRect;
  while (aClipChain) {
    result = aClipChain->mClip.ApplyNonRoundedIntersection(result);
    aClipChain = aClipChain->mParent;
  }
  return result;
}

void nsDisplayListBuilder::AdjustWindowDraggingRegion(nsIFrame* aFrame) {
  if (!mWindowDraggingAllowed || !IsForPainting()) {
    return;
  }

  const nsStyleUIReset* styleUI = aFrame->StyleUIReset();
  if (styleUI->mWindowDragging == StyleWindowDragging::Default) {
    // This frame has the default value and doesn't influence the window
    // dragging region.
    return;
  }

  LayoutDeviceToLayoutDeviceMatrix4x4 referenceFrameToRootReferenceFrame;

  // The const_cast is for nsLayoutUtils::GetTransformToAncestor.
  nsIFrame* referenceFrame =
      const_cast<nsIFrame*>(FindReferenceFrameFor(aFrame));

  if (IsInTransform()) {
    // Only support 2d rectilinear transforms. Transform support is needed for
    // the horizontal flip transform that's applied to the urlbar textbox in
    // RTL mode - it should be able to exclude itself from the draggable region.
    referenceFrameToRootReferenceFrame =
        ViewAs<LayoutDeviceToLayoutDeviceMatrix4x4>(
            nsLayoutUtils::GetTransformToAncestor(RelativeTo{referenceFrame},
                                                  RelativeTo{mReferenceFrame})
                .GetMatrix());
    Matrix referenceFrameToRootReferenceFrame2d;
    if (!referenceFrameToRootReferenceFrame.Is2D(
            &referenceFrameToRootReferenceFrame2d) ||
        !referenceFrameToRootReferenceFrame2d.IsRectilinear()) {
      return;
    }
  } else {
    MOZ_ASSERT(referenceFrame == mReferenceFrame,
               "referenceFrameToRootReferenceFrame needs to be adjusted");
  }

  // We do some basic visibility checking on the frame's border box here.
  // We intersect it both with the current dirty rect and with the current
  // clip. Either one is just a conservative approximation on its own, but
  // their intersection luckily works well enough for our purposes, so that
  // we don't have to do full-blown visibility computations.
  // The most important case we need to handle is the scrolled-off tab:
  // If the tab bar overflows, tab parts that are clipped by the scrollbox
  // should not be allowed to interfere with the window dragging region. Using
  // just the current DisplayItemClip is not enough to cover this case
  // completely because clips are reset while building stacking context
  // contents, so for example we'd fail to clip frames that have a clip path
  // applied to them. But the current dirty rect doesn't get reset in that
  // case, so we use it to make this case work.
  nsRect borderBox = aFrame->GetRectRelativeToSelf().Intersect(mVisibleRect);
  borderBox += ToReferenceFrame(aFrame);
  const DisplayItemClipChain* clip =
      ClipState().GetCurrentCombinedClipChain(this);
  borderBox = ApplyAllClipNonRoundedIntersection(clip, borderBox);
  if (borderBox.IsEmpty()) {
    return;
  }

  LayoutDeviceRect devPixelBorderBox = LayoutDevicePixel::FromAppUnits(
      borderBox, aFrame->PresContext()->AppUnitsPerDevPixel());

  LayoutDeviceRect transformedDevPixelBorderBox =
      TransformBy(referenceFrameToRootReferenceFrame, devPixelBorderBox);
  transformedDevPixelBorderBox.Round();
  LayoutDeviceIntRect transformedDevPixelBorderBoxInt;

  if (!transformedDevPixelBorderBox.ToIntRect(
          &transformedDevPixelBorderBoxInt)) {
    return;
  }

  LayoutDeviceIntRegion& region =
      styleUI->mWindowDragging == StyleWindowDragging::Drag
          ? mWindowDraggingRegion
          : mWindowNoDraggingRegion;

  if (!IsRetainingDisplayList()) {
    region.OrWith(transformedDevPixelBorderBoxInt);
    return;
  }

  gfx::IntRect rect(transformedDevPixelBorderBoxInt.ToUnknownRect());
  if (styleUI->mWindowDragging == StyleWindowDragging::Drag) {
    mRetainedWindowDraggingRegion.Add(aFrame, rect);
  } else {
    mRetainedWindowNoDraggingRegion.Add(aFrame, rect);
  }
}

LayoutDeviceIntRegion nsDisplayListBuilder::GetWindowDraggingRegion() const {
  LayoutDeviceIntRegion result;
  if (!IsRetainingDisplayList()) {
    result.Sub(mWindowDraggingRegion, mWindowNoDraggingRegion);
    return result;
  }

  LayoutDeviceIntRegion dragRegion =
      mRetainedWindowDraggingRegion.ToLayoutDeviceIntRegion();

  LayoutDeviceIntRegion noDragRegion =
      mRetainedWindowNoDraggingRegion.ToLayoutDeviceIntRegion();

  result.Sub(dragRegion, noDragRegion);
  return result;
}

void nsDisplayTransform::AddSizeOfExcludingThis(nsWindowSizes& aSizes) const {
  nsPaintedDisplayItem::AddSizeOfExcludingThis(aSizes);
  aSizes.mLayoutRetainedDisplayListSize +=
      aSizes.mState.mMallocSizeOf(mTransformPreserves3D.get());
}

void nsDisplayListBuilder::AddSizeOfExcludingThis(nsWindowSizes& aSizes) const {
  mPool.AddSizeOfExcludingThis(aSizes, Arena::ArenaKind::DisplayList);

  size_t n = 0;
  MallocSizeOf mallocSizeOf = aSizes.mState.mMallocSizeOf;
  n += mDocumentWillChangeBudgets.ShallowSizeOfExcludingThis(mallocSizeOf);
  n += mFrameWillChangeBudgets.ShallowSizeOfExcludingThis(mallocSizeOf);
  n += mEffectsUpdates.ShallowSizeOfExcludingThis(mallocSizeOf);
  n += mRetainedWindowDraggingRegion.SizeOfExcludingThis(mallocSizeOf);
  n += mRetainedWindowNoDraggingRegion.SizeOfExcludingThis(mallocSizeOf);
  n += mRetainedWindowOpaqueRegion.SizeOfExcludingThis(mallocSizeOf);
  // XXX can't measure mClipDeduplicator since it uses std::unordered_set.

  aSizes.mLayoutRetainedDisplayListSize += n;
}

void RetainedDisplayList::AddSizeOfExcludingThis(nsWindowSizes& aSizes) const {
  for (nsDisplayItem* item : *this) {
    item->AddSizeOfExcludingThis(aSizes);
    if (RetainedDisplayList* children = item->GetChildren()) {
      children->AddSizeOfExcludingThis(aSizes);
    }
  }

  size_t n = 0;

  n += mDAG.mDirectPredecessorList.ShallowSizeOfExcludingThis(
      aSizes.mState.mMallocSizeOf);
  n += mDAG.mNodesInfo.ShallowSizeOfExcludingThis(aSizes.mState.mMallocSizeOf);
  n += mOldItems.ShallowSizeOfExcludingThis(aSizes.mState.mMallocSizeOf);

  aSizes.mLayoutRetainedDisplayListSize += n;
}

size_t nsDisplayListBuilder::WeakFrameRegion::SizeOfExcludingThis(
    MallocSizeOf aMallocSizeOf) const {
  size_t n = 0;
  n += mFrames.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (const auto& frame : mFrames) {
    const UniquePtr<WeakFrame>& weakFrame = frame.mWeakFrame;
    n += aMallocSizeOf(weakFrame.get());
  }
  n += mRects.ShallowSizeOfExcludingThis(aMallocSizeOf);
  return n;
}

/**
 * Removes modified frames and rects from this WeakFrameRegion.
 */
void nsDisplayListBuilder::WeakFrameRegion::RemoveModifiedFramesAndRects() {
  MOZ_ASSERT(mFrames.Length() == mRects.Length());

  uint32_t i = 0;
  uint32_t length = mFrames.Length();

  while (i < length) {
    auto& wrapper = mFrames[i];

    if (!wrapper.mWeakFrame->IsAlive() ||
        AnyContentAncestorModified(wrapper.mWeakFrame->GetFrame())) {
      // To avoid multiple O(n) shifts in the array, move the last element of
      // the array to the current position and decrease the array length.
      mFrameSet.Remove(wrapper.mFrame);
      mFrames[i] = std::move(mFrames[length - 1]);
      mRects[i] = std::move(mRects[length - 1]);
      length--;
    } else {
      i++;
    }
  }

  mFrames.TruncateLength(length);
  mRects.TruncateLength(length);
}

void nsDisplayListBuilder::RemoveModifiedWindowRegions() {
  mRetainedWindowDraggingRegion.RemoveModifiedFramesAndRects();
  mRetainedWindowNoDraggingRegion.RemoveModifiedFramesAndRects();
  mRetainedWindowOpaqueRegion.RemoveModifiedFramesAndRects();
}

void nsDisplayListBuilder::ClearRetainedWindowRegions() {
  mRetainedWindowDraggingRegion.Clear();
  mRetainedWindowNoDraggingRegion.Clear();
  mRetainedWindowOpaqueRegion.Clear();
}

const uint32_t gWillChangeAreaMultiplier = 3;
static uint32_t GetLayerizationCost(const nsSize& aSize) {
  // There's significant overhead for each layer created from Gecko
  // (IPC+Shared Objects) and from the backend (like an OpenGL texture).
  // Therefore we set a minimum cost threshold of a 64x64 area.
  const int minBudgetCost = 64 * 64;

  const uint32_t budgetCost = std::max(
      minBudgetCost, nsPresContext::AppUnitsToIntCSSPixels(aSize.width) *
                         nsPresContext::AppUnitsToIntCSSPixels(aSize.height));

  return budgetCost;
}

bool nsDisplayListBuilder::AddToWillChangeBudget(nsIFrame* aFrame,
                                                 const nsSize& aSize) {
  MOZ_ASSERT(IsForPainting());

  if (aFrame->MayHaveWillChangeBudget()) {
    // The frame is already in the will-change budget.
    return true;
  }

  const nsPresContext* presContext = aFrame->PresContext();
  const nsRect area = presContext->GetVisibleArea();
  const uint32_t budgetLimit =
      nsPresContext::AppUnitsToIntCSSPixels(area.width) *
      nsPresContext::AppUnitsToIntCSSPixels(area.height);
  const uint32_t cost = GetLayerizationCost(aSize);

  DocumentWillChangeBudget& documentBudget =
      mDocumentWillChangeBudgets.LookupOrInsert(presContext);

  const bool onBudget =
      (documentBudget + cost) / gWillChangeAreaMultiplier < budgetLimit;

  if (onBudget) {
    documentBudget += cost;
    mFrameWillChangeBudgets.InsertOrUpdate(
        aFrame, FrameWillChangeBudget(presContext, cost));
    aFrame->SetMayHaveWillChangeBudget(true);
  }

  return onBudget;
}

bool nsDisplayListBuilder::IsInWillChangeBudget(nsIFrame* aFrame,
                                                const nsSize& aSize) {
  if (!IsForPainting()) {
    // If this nsDisplayListBuilder is not for painting, the layerization should
    // not matter. Do the simple thing and return false.
    return false;
  }

  const bool onBudget = AddToWillChangeBudget(aFrame, aSize);
  if (onBudget) {
    return true;
  }

  auto* pc = aFrame->PresContext();
  auto* doc = pc->Document();
  if (!doc->HasWarnedAbout(Document::eIgnoringWillChangeOverBudget)) {
    AutoTArray<nsString, 2> params;
    params.AppendElement()->AppendInt(gWillChangeAreaMultiplier);

    nsRect area = pc->GetVisibleArea();
    uint32_t budgetLimit = nsPresContext::AppUnitsToIntCSSPixels(area.width) *
                           nsPresContext::AppUnitsToIntCSSPixels(area.height);
    params.AppendElement()->AppendInt(budgetLimit);

    doc->WarnOnceAbout(Document::eIgnoringWillChangeOverBudget, false, params);
  }

  return false;
}

void nsDisplayListBuilder::ClearWillChangeBudgetStatus(nsIFrame* aFrame) {
  MOZ_ASSERT(IsForPainting());

  if (!aFrame->MayHaveWillChangeBudget()) {
    return;
  }

  aFrame->SetMayHaveWillChangeBudget(false);
  RemoveFromWillChangeBudgets(aFrame);
}

void nsDisplayListBuilder::RemoveFromWillChangeBudgets(const nsIFrame* aFrame) {
  if (auto entry = mFrameWillChangeBudgets.Lookup(aFrame)) {
    const FrameWillChangeBudget& frameBudget = entry.Data();

    auto documentBudget =
        mDocumentWillChangeBudgets.Lookup(frameBudget.mPresContext);

    if (documentBudget) {
      *documentBudget -= frameBudget.mUsage;
    }

    entry.Remove();
  }
}

void nsDisplayListBuilder::ClearWillChangeBudgets() {
  mFrameWillChangeBudgets.Clear();
  mDocumentWillChangeBudgets.Clear();
}

void nsDisplayListBuilder::EnterSVGEffectsContents(
    nsIFrame* aEffectsFrame, nsDisplayList* aHoistedItemsStorage) {
  MOZ_ASSERT(aHoistedItemsStorage);
  if (mSVGEffectsFrames.IsEmpty()) {
    MOZ_ASSERT(!mScrollInfoItemsForHoisting);
    mScrollInfoItemsForHoisting = aHoistedItemsStorage;
  }
  mSVGEffectsFrames.AppendElement(aEffectsFrame);
}

void nsDisplayListBuilder::ExitSVGEffectsContents() {
  MOZ_ASSERT(!mSVGEffectsFrames.IsEmpty());
  mSVGEffectsFrames.RemoveLastElement();
  MOZ_ASSERT(mScrollInfoItemsForHoisting);
  if (mSVGEffectsFrames.IsEmpty()) {
    mScrollInfoItemsForHoisting = nullptr;
  }
}

bool nsDisplayListBuilder::ShouldBuildScrollInfoItemsForHoisting() const {
  /*
   * Note: if changing the conditions under which scroll info layers
   * are created, make a corresponding change to
   * ScrollFrameWillBuildScrollInfoLayer() in nsSliderFrame.cpp.
   */
  for (nsIFrame* frame : mSVGEffectsFrames) {
    if (SVGIntegrationUtils::UsesSVGEffectsNotSupportedInCompositor(frame)) {
      return true;
    }
  }
  return false;
}

void nsDisplayListBuilder::AppendNewScrollInfoItemForHoisting(
    nsDisplayScrollInfoLayer* aScrollInfoItem) {
  MOZ_ASSERT(ShouldBuildScrollInfoItemsForHoisting());
  MOZ_ASSERT(mScrollInfoItemsForHoisting);
  mScrollInfoItemsForHoisting->AppendToTop(aScrollInfoItem);
}

void nsDisplayListBuilder::BuildCompositorHitTestInfoIfNeeded(
    nsIFrame* aFrame, nsDisplayList* aList) {
  MOZ_ASSERT(aFrame);
  MOZ_ASSERT(aList);

  if (!BuildCompositorHitTestInfo()) {
    return;
  }

  const CompositorHitTestInfo info = aFrame->GetCompositorHitTestInfo(this);
  if (info != CompositorHitTestInvisibleToHit) {
    aList->AppendNewToTop<nsDisplayCompositorHitTestInfo>(this, aFrame);
  }
}

void nsDisplayListBuilder::AddReusableDisplayItem(nsDisplayItem* aItem) {
  mReuseableItems.Insert(aItem);
}

void nsDisplayListBuilder::RemoveReusedDisplayItem(nsDisplayItem* aItem) {
  MOZ_ASSERT(aItem->IsReusedItem());
  mReuseableItems.Remove(aItem);
}

void nsDisplayListBuilder::ClearReuseableDisplayItems() {
  const size_t total = mReuseableItems.Count();

  size_t reused = 0;
  for (auto* item : mReuseableItems) {
    if (item->IsReusedItem()) {
      reused++;
      item->SetReusable();
    } else {
      item->Destroy(this);
    }
  }

  DL_LOGI("RDL - Reused %zu of %zu SC display items", reused, total);
  mReuseableItems.Clear();
}

void nsDisplayListBuilder::ReuseDisplayItem(nsDisplayItem* aItem) {
  const auto* previous = mCurrentContainerASR;
  const auto* asr = aItem->GetActiveScrolledRoot();
  mCurrentContainerASR =
      ActiveScrolledRoot::PickAncestor(asr, mCurrentContainerASR);

  if (previous != mCurrentContainerASR) {
    DL_LOGV("RDL - Changed mCurrentContainerASR from %p to %p", previous,
            mCurrentContainerASR);
  }

  aItem->SetReusedItem();
}

void nsDisplayListSet::CopyTo(const nsDisplayListSet& aDestination) const {
  for (size_t i = 0; i < mLists.size(); ++i) {
    auto* from = mLists[i];
    auto* to = aDestination.mLists[i];

    from->CopyTo(to);
  }
}

void nsDisplayListSet::MoveTo(const nsDisplayListSet& aDestination) const {
  aDestination.BorderBackground()->AppendToTop(BorderBackground());
  aDestination.BlockBorderBackgrounds()->AppendToTop(BlockBorderBackgrounds());
  aDestination.Floats()->AppendToTop(Floats());
  aDestination.Content()->AppendToTop(Content());
  aDestination.PositionedDescendants()->AppendToTop(PositionedDescendants());
  aDestination.Outlines()->AppendToTop(Outlines());
}

nsRect nsDisplayList::GetClippedBounds(nsDisplayListBuilder* aBuilder) const {
  nsRect bounds;
  for (nsDisplayItem* i : *this) {
    bounds.UnionRect(bounds, i->GetClippedBounds(aBuilder));
  }
  return bounds;
}

nsRect nsDisplayList::GetClippedBoundsWithRespectToASR(
    nsDisplayListBuilder* aBuilder, const ActiveScrolledRoot* aASR,
    nsRect* aBuildingRect) const {
  nsRect bounds;
  for (nsDisplayItem* i : *this) {
    nsRect r = i->GetClippedBounds(aBuilder);
    if (aASR != i->GetActiveScrolledRoot() && !r.IsEmpty()) {
      if (Maybe<nsRect> clip = i->GetClipWithRespectToASR(aBuilder, aASR)) {
        r = clip.ref();
      }
    }
    if (aBuildingRect) {
      aBuildingRect->UnionRect(*aBuildingRect, i->GetBuildingRect());
    }
    bounds.UnionRect(bounds, r);
  }
  return bounds;
}

nsRect nsDisplayList::GetBuildingRect() const {
  nsRect result;
  for (nsDisplayItem* i : *this) {
    result.UnionRect(result, i->GetBuildingRect());
  }
  return result;
}

WindowRenderer* nsDisplayListBuilder::GetWidgetWindowRenderer(nsView** aView) {
  if (aView) {
    *aView = RootReferenceFrame()->GetView();
  }
  if (RootReferenceFrame() !=
      nsLayoutUtils::GetDisplayRootFrame(RootReferenceFrame())) {
    return nullptr;
  }
  nsIWidget* window = RootReferenceFrame()->GetNearestWidget();
  if (window) {
    return window->GetWindowRenderer();
  }
  return nullptr;
}

WebRenderLayerManager* nsDisplayListBuilder::GetWidgetLayerManager(
    nsView** aView) {
  WindowRenderer* renderer = GetWidgetWindowRenderer();
  if (renderer) {
    return renderer->AsWebRender();
  }
  return nullptr;
}

void nsDisplayList::Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx,
                          int32_t aAppUnitsPerDevPixel) {
  FlattenedDisplayListIterator iter(aBuilder, this);
  while (iter.HasNext()) {
    nsPaintedDisplayItem* item = iter.GetNextItem()->AsPaintedDisplayItem();
    if (!item) {
      continue;
    }

    nsRect visible = item->GetClippedBounds(aBuilder);
    visible = visible.Intersect(item->GetPaintRect(aBuilder, aCtx));
    if (visible.IsEmpty()) {
      continue;
    }

    DisplayItemClip currentClip = item->GetClip();
    if (currentClip.HasClip()) {
      aCtx->Save();
      if (currentClip.IsRectClippedByRoundedCorner(visible)) {
        currentClip.ApplyTo(aCtx, aAppUnitsPerDevPixel);
      } else {
        currentClip.ApplyRectTo(aCtx, aAppUnitsPerDevPixel);
      }
    }
    aCtx->NewPath();

    item->Paint(aBuilder, aCtx);

    if (currentClip.HasClip()) {
      aCtx->Restore();
    }
  }
}

/**
 * We paint by executing a layer manager transaction, constructing a
 * single layer representing the display list, and then making it the
 * root of the layer manager, drawing into the PaintedLayers.
 */
void nsDisplayList::PaintRoot(nsDisplayListBuilder* aBuilder, gfxContext* aCtx,
                              uint32_t aFlags,
                              Maybe<double> aDisplayListBuildTime) {
  AUTO_PROFILER_LABEL("nsDisplayList::PaintRoot", GRAPHICS);

  RefPtr<WebRenderLayerManager> layerManager;
  WindowRenderer* renderer = nullptr;
  bool widgetTransaction = false;
  bool doBeginTransaction = true;
  nsView* view = nullptr;
  if (aFlags & PAINT_USE_WIDGET_LAYERS) {
    renderer = aBuilder->GetWidgetWindowRenderer(&view);
    if (renderer) {
      // The fallback renderer doesn't retain any content, so it's
      // not meaningful to use it when drawing to an external context.
      if (aCtx && renderer->AsFallback()) {
        MOZ_ASSERT(!(aFlags & PAINT_EXISTING_TRANSACTION));
        renderer = nullptr;
      } else {
        layerManager = renderer->AsWebRender();
        doBeginTransaction = !(aFlags & PAINT_EXISTING_TRANSACTION);
        widgetTransaction = true;
      }
    }
  }

  nsIFrame* frame = aBuilder->RootReferenceFrame();
  nsPresContext* presContext = frame->PresContext();
  PresShell* presShell = presContext->PresShell();
  Document* document = presShell->GetDocument();

  ScopeExit g([&]() {
#ifdef DEBUG
    MOZ_ASSERT(!layerManager || !layerManager->GetTarget());
#endif

    // For layers-free mode, we check the invalidation state bits in the
    // EndTransaction. So we clear the invalidation state bits after
    // EndTransaction.
    if (widgetTransaction ||
        // SVG-as-an-image docs don't paint as part of the retained layer tree,
        // but they still need the invalidation state bits cleared in order for
        // invalidation for CSS/SMIL animation to work properly.
        (document && document->IsBeingUsedAsImage())) {
      DL_LOGD("Clearing invalidation state bits");
      frame->ClearInvalidationStateBits();
    }
  });

  if (!renderer) {
    if (!aCtx) {
      NS_WARNING("Nowhere to paint into");
      return;
    }
    Paint(aBuilder, aCtx, presContext->AppUnitsPerDevPixel());

    return;
  }

  if (renderer->GetBackendType() == LayersBackend::LAYERS_WR) {
    MOZ_ASSERT(layerManager);
    if (doBeginTransaction) {
      if (aCtx) {
        if (!layerManager->BeginTransactionWithTarget(aCtx, nsCString())) {
          return;
        }
      } else {
        if (!layerManager->BeginTransaction(nsCString())) {
          return;
        }
      }
    }

    layerManager->SetTransactionIdAllocator(presContext->RefreshDriver());

    bool sent = false;
    if (aFlags & PAINT_IDENTICAL_DISPLAY_LIST) {
      MOZ_ASSERT(!aCtx);
      sent = layerManager->EndEmptyTransaction();
    }

    if (!sent) {
      auto* wrManager = static_cast<WebRenderLayerManager*>(layerManager.get());

      nsIDocShell* docShell = presContext->GetDocShell();
      WrFiltersHolder wrFilters;
      gfx::Matrix5x4* colorMatrix =
          nsDocShell::Cast(docShell)->GetColorMatrix();
      if (colorMatrix) {
        wrFilters.filters.AppendElement(
            wr::FilterOp::ColorMatrix(colorMatrix->components));
      }

      wrManager->EndTransactionWithoutLayer(this, aBuilder,
                                            std::move(wrFilters), nullptr,
                                            aDisplayListBuildTime.valueOr(0.0));
    }

    if (presContext->RefreshDriver()->HasScheduleFlush()) {
      presContext->NotifyInvalidation(layerManager->GetLastTransactionId(),
                                      frame->GetRect());
    }

    return;
  }

  FallbackRenderer* fallback = renderer->AsFallback();
  MOZ_ASSERT(fallback);

  if (doBeginTransaction) {
    MOZ_ASSERT(!aCtx);
    if (!fallback->BeginTransaction()) {
      return;
    }
  }

  fallback->EndTransactionWithList(aBuilder, this,
                                   presContext->AppUnitsPerDevPixel(),
                                   WindowRenderer::END_DEFAULT);
}

void nsDisplayList::DeleteAll(nsDisplayListBuilder* aBuilder) {
  for (auto* item : TakeItems()) {
    item->Destroy(aBuilder);
  }
}

static bool IsFrameReceivingPointerEvents(nsIFrame* aFrame) {
  return aFrame->Style()->PointerEvents() != StylePointerEvents::None;
}

// A list of frames, and their z depth. Used for sorting
// the results of hit testing.
struct FramesWithDepth {
  explicit FramesWithDepth(float aDepth) : mDepth(aDepth) {}

  bool operator<(const FramesWithDepth& aOther) const {
    if (!FuzzyEqual(mDepth, aOther.mDepth, 0.1f)) {
      // We want to sort so that the shallowest item (highest depth value) is
      // first
      return mDepth > aOther.mDepth;
    }
    return false;
  }
  bool operator==(const FramesWithDepth& aOther) const {
    return this == &aOther;
  }

  float mDepth;
  nsTArray<nsIFrame*> mFrames;
};

// Sort the frames by depth and then moves all the contained frames to the
// destination
static void FlushFramesArray(nsTArray<FramesWithDepth>& aSource,
                             nsTArray<nsIFrame*>* aDest) {
  if (aSource.IsEmpty()) {
    return;
  }
  aSource.StableSort();
  uint32_t length = aSource.Length();
  for (uint32_t i = 0; i < length; i++) {
    aDest->AppendElements(std::move(aSource[i].mFrames));
  }
  aSource.Clear();
}

void nsDisplayList::HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                            nsDisplayItem::HitTestState* aState,
                            nsTArray<nsIFrame*>* aOutFrames) const {
  nsDisplayItem* item;

  if (aState->mInPreserves3D) {
    // Collect leaves of the current 3D rendering context.
    for (nsDisplayItem* item : *this) {
      auto itemType = item->GetType();
      if (itemType != DisplayItemType::TYPE_TRANSFORM ||
          !static_cast<nsDisplayTransform*>(item)->IsLeafOf3DContext()) {
        item->HitTest(aBuilder, aRect, aState, aOutFrames);
      } else {
        // One of leaves in the current 3D rendering context.
        aState->mItemBuffer.AppendElement(item);
      }
    }
    return;
  }

  int32_t itemBufferStart = aState->mItemBuffer.Length();
  for (nsDisplayItem* item : *this) {
    aState->mItemBuffer.AppendElement(item);
  }

  AutoTArray<FramesWithDepth, 16> temp;
  for (int32_t i = aState->mItemBuffer.Length() - 1; i >= itemBufferStart;
       --i) {
    // Pop element off the end of the buffer. We want to shorten the buffer
    // so that recursive calls to HitTest have more buffer space.
    item = aState->mItemBuffer[i];
    aState->mItemBuffer.SetLength(i);

    bool snap;
    nsRect r = item->GetBounds(aBuilder, &snap).Intersect(aRect);
    auto itemType = item->GetType();
    bool same3DContext =
        (itemType == DisplayItemType::TYPE_TRANSFORM &&
         static_cast<nsDisplayTransform*>(item)->IsParticipating3DContext()) ||
        (itemType == DisplayItemType::TYPE_PERSPECTIVE &&
         item->Frame()->Extend3DContext());
    if (same3DContext &&
        (itemType != DisplayItemType::TYPE_TRANSFORM ||
         !static_cast<nsDisplayTransform*>(item)->IsLeafOf3DContext())) {
      if (!item->GetClip().MayIntersect(aRect)) {
        continue;
      }
      AutoTArray<nsIFrame*, 1> neverUsed;
      // Start gathering leaves of the 3D rendering context, and
      // append leaves at the end of mItemBuffer.  Leaves are
      // processed at following iterations.
      aState->mInPreserves3D = true;
      item->HitTest(aBuilder, aRect, aState, &neverUsed);
      aState->mInPreserves3D = false;
      i = aState->mItemBuffer.Length();
      continue;
    }
    if (same3DContext || item->GetClip().MayIntersect(r)) {
      AutoTArray<nsIFrame*, 16> outFrames;
      item->HitTest(aBuilder, aRect, aState, &outFrames);

      // For 3d transforms with preserve-3d we add hit frames into the temp list
      // so we can sort them later, otherwise we add them directly to the output
      // list.
      nsTArray<nsIFrame*>* writeFrames = aOutFrames;
      if (item->GetType() == DisplayItemType::TYPE_TRANSFORM &&
          static_cast<nsDisplayTransform*>(item)->IsLeafOf3DContext()) {
        if (outFrames.Length()) {
          nsDisplayTransform* transform =
              static_cast<nsDisplayTransform*>(item);
          nsPoint point = aRect.TopLeft();
          // A 1x1 rect means a point, otherwise use the center of the rect
          if (aRect.width != 1 || aRect.height != 1) {
            point = aRect.Center();
          }
          temp.AppendElement(
              FramesWithDepth(transform->GetHitDepthAtPoint(aBuilder, point)));
          writeFrames = &temp[temp.Length() - 1].mFrames;
        }
      } else {
        // We may have just finished a run of consecutive preserve-3d
        // transforms, so flush these into the destination array before
        // processing our frame list.
        FlushFramesArray(temp, aOutFrames);
      }

      for (uint32_t j = 0; j < outFrames.Length(); j++) {
        nsIFrame* f = outFrames.ElementAt(j);
        // Filter out some frames depending on the type of hittest
        // we are doing. For visibility tests, pass through all frames.
        // For pointer tests, only pass through frames that are styled
        // to receive pointer events.
        if (aBuilder->HitTestIsForVisibility() ||
            IsFrameReceivingPointerEvents(f)) {
          writeFrames->AppendElement(f);
        }
      }

      if (aBuilder->HitTestIsForVisibility()) {
        aState->mHitOccludingItem = [&] {
          if (aState->mHitOccludingItem) {
            // We already hit something before.
            return true;
          }
          if (aState->mCurrentOpacity == 1.0f &&
              item->GetOpaqueRegion(aBuilder, &snap).Contains(aRect)) {
            // An opaque item always occludes everything. Note that we need to
            // check wrapping opacity and such as well.
            return true;
          }
          float threshold = aBuilder->VisibilityThreshold();
          if (threshold == 1.0f) {
            return false;
          }
          float itemOpacity = [&] {
            switch (item->GetType()) {
              case DisplayItemType::TYPE_OPACITY:
                return static_cast<nsDisplayOpacity*>(item)->GetOpacity();
              case DisplayItemType::TYPE_BACKGROUND_COLOR:
                return static_cast<nsDisplayBackgroundColor*>(item)
                    ->GetOpacity();
              default:
                // Be conservative and assume it won't occlude other items.
                return 0.0f;
            }
          }();
          return itemOpacity * aState->mCurrentOpacity >= threshold;
        }();

        if (aState->mHitOccludingItem) {
          // We're exiting early, so pop the remaining items off the buffer.
          aState->mItemBuffer.TruncateLength(itemBufferStart);
          break;
        }
      }
    }
  }
  // Clear any remaining preserve-3d transforms.
  FlushFramesArray(temp, aOutFrames);
  NS_ASSERTION(aState->mItemBuffer.Length() == uint32_t(itemBufferStart),
               "How did we forget to pop some elements?");
}

static nsIContent* FindContentInDocument(nsDisplayItem* aItem, Document* aDoc) {
  nsIFrame* f = aItem->Frame();
  while (f) {
    nsPresContext* pc = f->PresContext();
    if (pc->Document() == aDoc) {
      return f->GetContent();
    }
    f = nsLayoutUtils::GetCrossDocParentFrameInProcess(
        pc->PresShell()->GetRootFrame());
  }
  return nullptr;
}

struct ZSortItem {
  nsDisplayItem* item;
  int32_t zIndex;

  explicit ZSortItem(nsDisplayItem* aItem)
      : item(aItem), zIndex(aItem->ZIndex()) {}

  operator nsDisplayItem*() { return item; }
};

struct ZOrderComparator {
  bool LessThan(const ZSortItem& aLeft, const ZSortItem& aRight) const {
    return aLeft.zIndex < aRight.zIndex;
  }
};

void nsDisplayList::SortByZOrder() { Sort<ZSortItem>(ZOrderComparator()); }

struct ContentComparator {
  nsIContent* mCommonAncestor;

  explicit ContentComparator(nsIContent* aCommonAncestor)
      : mCommonAncestor(aCommonAncestor) {}

  bool LessThan(nsDisplayItem* aLeft, nsDisplayItem* aRight) const {
    // It's possible that the nsIContent for aItem1 or aItem2 is in a
    // subdocument of commonAncestor, because display items for subdocuments
    // have been mixed into the same list. Ensure that we're looking at content
    // in commonAncestor's document.
    Document* commonAncestorDoc = mCommonAncestor->OwnerDoc();
    nsIContent* content1 = FindContentInDocument(aLeft, commonAncestorDoc);
    nsIContent* content2 = FindContentInDocument(aRight, commonAncestorDoc);
    if (!content1 || !content2) {
      NS_ERROR("Document trees are mixed up!");
      // Something weird going on
      return true;
    }
    return content1 != content2 &&
           nsContentUtils::CompareTreePosition<TreeKind::Flat>(
               content1, content2, mCommonAncestor) < 0;
  }
};

void nsDisplayList::SortByContentOrder(nsIContent* aCommonAncestor) {
  Sort<nsDisplayItem*>(ContentComparator(aCommonAncestor));
}

#if !defined(DEBUG) && !defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED)
static_assert(sizeof(nsDisplayItem) <= 176, "nsDisplayItem has grown");
#endif

nsDisplayItem::nsDisplayItem(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame, aBuilder->CurrentActiveScrolledRoot()) {}

nsDisplayItem::nsDisplayItem(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                             const ActiveScrolledRoot* aActiveScrolledRoot)
    : mFrame(aFrame), mActiveScrolledRoot(aActiveScrolledRoot) {
  MOZ_COUNT_CTOR(nsDisplayItem);
  MOZ_ASSERT(mFrame);
  if (aBuilder->IsRetainingDisplayList()) {
    mFrame->AddDisplayItem(this);
  }

  aBuilder->FindReferenceFrameFor(aFrame, &mToReferenceFrame);
  NS_ASSERTION(
      aBuilder->GetVisibleRect().width >= 0 || !aBuilder->IsForPainting(),
      "visible rect not set");

  mClipChain = aBuilder->ClipState().GetCurrentCombinedClipChain(aBuilder);

  // The visible rect is for mCurrentFrame, so we have to use
  // mCurrentOffsetToReferenceFrame
  nsRect visible = aBuilder->GetVisibleRect() +
                   aBuilder->GetCurrentFrameOffsetToReferenceFrame();
  SetBuildingRect(visible);

  const nsStyleDisplay* disp = mFrame->StyleDisplay();
  if (mFrame->BackfaceIsHidden(disp)) {
    mItemFlags += ItemFlag::BackfaceHidden;
  }
  if (mFrame->Combines3DTransformWithAncestors()) {
    mItemFlags += ItemFlag::Combines3DTransformWithAncestors;
  }
}

void nsDisplayItem::SetDeletedFrame() { mItemFlags += ItemFlag::DeletedFrame; }

bool nsDisplayItem::HasDeletedFrame() const {
  bool retval = mItemFlags.contains(ItemFlag::DeletedFrame) ||
                (GetType() == DisplayItemType::TYPE_REMOTE &&
                 !static_cast<const nsDisplayRemote*>(this)->GetFrameLoader());
  MOZ_ASSERT(retval || mFrame);
  return retval;
}

/* static */
bool nsDisplayItem::ForceActiveLayers() {
  return StaticPrefs::layers_force_active();
}

int32_t nsDisplayItem::ZIndex() const { return mFrame->ZIndex().valueOr(0); }

void nsDisplayItem::SetClipChain(const DisplayItemClipChain* aClipChain,
                                 bool aStore) {
  mClipChain = aClipChain;
}

Maybe<nsRect> nsDisplayItem::GetClipWithRespectToASR(
    nsDisplayListBuilder* aBuilder, const ActiveScrolledRoot* aASR) const {
  if (const DisplayItemClip* clip =
          DisplayItemClipChain::ClipForASR(GetClipChain(), aASR)) {
    return Some(clip->GetClipRect());
  }
#ifdef DEBUG
  NS_ASSERTION(false, "item should have finite clip with respect to aASR");
#endif
  return Nothing();
}

const DisplayItemClip& nsDisplayItem::GetClip() const {
  const DisplayItemClip* clip =
      DisplayItemClipChain::ClipForASR(mClipChain, mActiveScrolledRoot);
  return clip ? *clip : DisplayItemClip::NoClip();
}

void nsDisplayItem::IntersectClip(nsDisplayListBuilder* aBuilder,
                                  const DisplayItemClipChain* aOther,
                                  bool aStore) {
  if (!aOther || mClipChain == aOther) {
    return;
  }

  // aOther might be a reference to a clip on the stack. We need to make sure
  // that CreateClipChainIntersection will allocate the actual intersected
  // clip in the builder's arena, so for the mClipChain == nullptr case,
  // we supply nullptr as the common ancestor so that
  // CreateClipChainIntersection clones the whole chain.
  const DisplayItemClipChain* ancestorClip =
      mClipChain ? FindCommonAncestorClipForIntersection(mClipChain, aOther)
                 : nullptr;

  SetClipChain(
      aBuilder->CreateClipChainIntersection(ancestorClip, mClipChain, aOther),
      aStore);
}

nsRect nsDisplayItem::GetClippedBounds(nsDisplayListBuilder* aBuilder) const {
  bool snap;
  nsRect r = GetBounds(aBuilder, &snap);
  return GetClip().ApplyNonRoundedIntersection(r);
}

nsDisplayContainer::nsDisplayContainer(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
    const ActiveScrolledRoot* aActiveScrolledRoot, nsDisplayList* aList)
    : nsDisplayItem(aBuilder, aFrame, aActiveScrolledRoot),
      mChildren(aBuilder) {
  MOZ_COUNT_CTOR(nsDisplayContainer);
  mChildren.AppendToTop(aList);
  UpdateBounds(aBuilder);

  // Clear and store the clip chain set by nsDisplayItem constructor.
  nsDisplayItem::SetClipChain(nullptr, true);
}

nsRect nsDisplayItem::GetPaintRect(nsDisplayListBuilder* aBuilder,
                                   gfxContext* aCtx) {
  bool dummy;
  nsRect result = GetBounds(aBuilder, &dummy);
  if (aCtx) {
    result.IntersectRect(result,
                         nsLayoutUtils::RoundGfxRectToAppRect(
                             aCtx->GetClipExtents(),
                             mFrame->PresContext()->AppUnitsPerDevPixel()));
  }
  return result;
}

bool nsDisplayContainer::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  aManager->CommandBuilder().CreateWebRenderCommandsFromDisplayList(
      GetChildren(), this, aDisplayListBuilder, aSc, aBuilder, aResources,
      false);
  return true;
}

nsRect nsDisplayContainer::GetBounds(nsDisplayListBuilder* aBuilder,
                                     bool* aSnap) const {
  *aSnap = false;
  return mBounds;
}

nsRect nsDisplayContainer::GetComponentAlphaBounds(
    nsDisplayListBuilder* aBuilder) const {
  return mChildren.GetComponentAlphaBounds(aBuilder);
}

static nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                nsDisplayList* aList,
                                const nsRect& aListBounds) {
  return aList->GetOpaqueRegion(aBuilder);
}

nsRegion nsDisplayContainer::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                             bool* aSnap) const {
  return ::mozilla::GetOpaqueRegion(aBuilder, GetChildren(),
                                    GetBounds(aBuilder, aSnap));
}

Maybe<nsRect> nsDisplayContainer::GetClipWithRespectToASR(
    nsDisplayListBuilder* aBuilder, const ActiveScrolledRoot* aASR) const {
  // Our children should have finite bounds with respect to |aASR|.
  if (aASR == mActiveScrolledRoot) {
    return Some(mBounds);
  }

  return Some(mChildren.GetClippedBoundsWithRespectToASR(aBuilder, aASR));
}

void nsDisplayContainer::HitTest(nsDisplayListBuilder* aBuilder,
                                 const nsRect& aRect, HitTestState* aState,
                                 nsTArray<nsIFrame*>* aOutFrames) {
  mChildren.HitTest(aBuilder, aRect, aState, aOutFrames);
}

void nsDisplayContainer::UpdateBounds(nsDisplayListBuilder* aBuilder) {
  // Container item bounds are expected to be clipped.
  mBounds =
      mChildren.GetClippedBoundsWithRespectToASR(aBuilder, mActiveScrolledRoot);
}

nsRect nsDisplaySolidColor::GetBounds(nsDisplayListBuilder* aBuilder,
                                      bool* aSnap) const {
  *aSnap = true;
  return mBounds;
}

void nsDisplaySolidColor::Paint(nsDisplayListBuilder* aBuilder,
                                gfxContext* aCtx) {
  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  DrawTarget* drawTarget = aCtx->GetDrawTarget();
  Rect rect = NSRectToSnappedRect(GetPaintRect(aBuilder, aCtx),
                                  appUnitsPerDevPixel, *drawTarget);
  drawTarget->FillRect(rect, ColorPattern(ToDeviceColor(mColor)));
}

void nsDisplaySolidColor::WriteDebugInfo(std::stringstream& aStream) {
  aStream << " (rgba " << (int)NS_GET_R(mColor) << "," << (int)NS_GET_G(mColor)
          << "," << (int)NS_GET_B(mColor) << "," << (int)NS_GET_A(mColor)
          << ")";
}

bool nsDisplaySolidColor::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  LayoutDeviceRect bounds = LayoutDeviceRect::FromAppUnits(
      mBounds, mFrame->PresContext()->AppUnitsPerDevPixel());
  wr::LayoutRect r = wr::ToLayoutRect(bounds);
  aBuilder.PushRect(r, r, !BackfaceIsHidden(), false, mIsCheckerboardBackground,
                    wr::ToColorF(ToDeviceColor(mColor)));

  return true;
}

nsRect nsDisplaySolidColorRegion::GetBounds(nsDisplayListBuilder* aBuilder,
                                            bool* aSnap) const {
  *aSnap = true;
  return mRegion.GetBounds();
}

void nsDisplaySolidColorRegion::Paint(nsDisplayListBuilder* aBuilder,
                                      gfxContext* aCtx) {
  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  DrawTarget* drawTarget = aCtx->GetDrawTarget();
  ColorPattern color(ToDeviceColor(mColor));
  for (auto iter = mRegion.RectIter(); !iter.Done(); iter.Next()) {
    Rect rect =
        NSRectToSnappedRect(iter.Get(), appUnitsPerDevPixel, *drawTarget);
    drawTarget->FillRect(rect, color);
  }
}

void nsDisplaySolidColorRegion::WriteDebugInfo(std::stringstream& aStream) {
  aStream << " (rgba " << int(mColor.r * 255) << "," << int(mColor.g * 255)
          << "," << int(mColor.b * 255) << "," << mColor.a << ")";
}

bool nsDisplaySolidColorRegion::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  for (auto iter = mRegion.RectIter(); !iter.Done(); iter.Next()) {
    nsRect rect = iter.Get();
    LayoutDeviceRect layerRects = LayoutDeviceRect::FromAppUnits(
        rect, mFrame->PresContext()->AppUnitsPerDevPixel());
    wr::LayoutRect r = wr::ToLayoutRect(layerRects);
    aBuilder.PushRect(r, r, !BackfaceIsHidden(), false, false,
                      wr::ToColorF(ToDeviceColor(mColor)));
  }

  return true;
}

static void RegisterThemeGeometry(nsDisplayListBuilder* aBuilder,
                                  nsDisplayItem* aItem, nsIFrame* aFrame,
                                  nsITheme::ThemeGeometryType aType) {
  if (aBuilder->IsInChromeDocumentOrPopup()) {
    nsIFrame* displayRoot = nsLayoutUtils::GetDisplayRootFrame(aFrame);
    bool preservesAxisAlignedRectangles = false;
    nsRect borderBox = nsLayoutUtils::TransformFrameRectToAncestor(
        aFrame, aFrame->GetRectRelativeToSelf(), displayRoot,
        &preservesAxisAlignedRectangles);
    if (preservesAxisAlignedRectangles) {
      aBuilder->RegisterThemeGeometry(
          aType, aItem,
          LayoutDeviceIntRect::FromUnknownRect(borderBox.ToNearestPixels(
              aFrame->PresContext()->AppUnitsPerDevPixel())));
    }
  }
}

// Return the bounds of the viewport relative to |aFrame|'s reference frame.
// Returns Nothing() if transforming into |aFrame|'s coordinate space fails.
static Maybe<nsRect> GetViewportRectRelativeToReferenceFrame(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame) {
  nsIFrame* rootFrame = aFrame->PresShell()->GetRootFrame();
  nsRect rootRect = rootFrame->GetRectRelativeToSelf();
  if (nsLayoutUtils::TransformRect(rootFrame, aFrame, rootRect) ==
      nsLayoutUtils::TRANSFORM_SUCCEEDED) {
    return Some(rootRect + aBuilder->ToReferenceFrame(aFrame));
  }
  return Nothing();
}

/* static */ nsDisplayBackgroundImage::InitData
nsDisplayBackgroundImage::GetInitData(nsDisplayListBuilder* aBuilder,
                                      nsIFrame* aFrame, uint16_t aLayer,
                                      const nsRect& aBackgroundRect,
                                      const ComputedStyle* aBackgroundStyle) {
  nsPresContext* presContext = aFrame->PresContext();
  uint32_t flags = aBuilder->GetBackgroundPaintFlags();
  const nsStyleImageLayers::Layer& layer =
      aBackgroundStyle->StyleBackground()->mImage.mLayers[aLayer];

  bool isTransformedFixed;
  nsBackgroundLayerState state = nsCSSRendering::PrepareImageLayer(
      presContext, aFrame, flags, aBackgroundRect, aBackgroundRect, layer,
      &isTransformedFixed);

  // background-attachment:fixed is treated as background-attachment:scroll
  // if it's affected by a transform.
  // See https://www.w3.org/Bugs/Public/show_bug.cgi?id=17521.
  bool shouldTreatAsFixed =
      layer.mAttachment == StyleImageLayerAttachment::Fixed &&
      !isTransformedFixed;

  bool shouldFixToViewport = shouldTreatAsFixed && !layer.mImage.IsNone();
  bool isRasterImage = state.mImageRenderer.IsRasterImage();
  nsCOMPtr<imgIContainer> image;
  if (isRasterImage) {
    image = state.mImageRenderer.GetImage();
  }
  return InitData{aBuilder,        aBackgroundStyle, image,
                  aBackgroundRect, state.mFillArea,  state.mDestArea,
                  aLayer,          isRasterImage,    shouldFixToViewport};
}

nsDisplayBackgroundImage::nsDisplayBackgroundImage(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, const InitData& aInitData,
    nsIFrame* aFrameForBounds)
    : nsPaintedDisplayItem(aBuilder, aFrame),
      mBackgroundStyle(aInitData.backgroundStyle),
      mImage(aInitData.image),
      mDependentFrame(nullptr),
      mBackgroundRect(aInitData.backgroundRect),
      mFillRect(aInitData.fillArea),
      mDestRect(aInitData.destArea),
      mLayer(aInitData.layer),
      mIsRasterImage(aInitData.isRasterImage),
      mShouldFixToViewport(aInitData.shouldFixToViewport) {
  MOZ_COUNT_CTOR(nsDisplayBackgroundImage);
#ifdef DEBUG
  if (mBackgroundStyle && mBackgroundStyle != mFrame->Style()) {
    // If this changes, then you also need to adjust css::ImageLoader to
    // invalidate mFrame as needed.
    MOZ_ASSERT(mFrame->IsCanvasFrame() || mFrame->IsTablePart());
  }
#endif

  mBounds = GetBoundsInternal(aInitData.builder, aFrameForBounds);
  if (mShouldFixToViewport) {
    // Expand the item's visible rect to cover the entire bounds, limited to the
    // viewport rect. This is necessary because the background's clip can move
    // asynchronously.
    if (Maybe<nsRect> viewportRect = GetViewportRectRelativeToReferenceFrame(
            aInitData.builder, mFrame)) {
      SetBuildingRect(mBounds.Intersect(*viewportRect));
    }
  }
}

nsDisplayBackgroundImage::~nsDisplayBackgroundImage() {
  MOZ_COUNT_DTOR(nsDisplayBackgroundImage);
  if (mDependentFrame) {
    mDependentFrame->RemoveDisplayItem(this);
  }
}

static void SetBackgroundClipRegion(
    DisplayListClipState::AutoSaveRestore& aClipState, nsIFrame* aFrame,
    const nsStyleImageLayers::Layer& aLayer, const nsRect& aBackgroundRect,
    bool aWillPaintBorder) {
  nsCSSRendering::ImageLayerClipState clip;
  nsCSSRendering::GetImageLayerClip(
      aLayer, aFrame, *aFrame->StyleBorder(), aBackgroundRect, aBackgroundRect,
      aWillPaintBorder, aFrame->PresContext()->AppUnitsPerDevPixel(), &clip);

  if (clip.mHasAdditionalBGClipArea) {
    aClipState.ClipContentDescendants(
        clip.mAdditionalBGClipArea, clip.mBGClipArea,
        clip.mHasRoundedCorners ? clip.mRadii : nullptr);
  } else {
    aClipState.ClipContentDescendants(
        clip.mBGClipArea, clip.mHasRoundedCorners ? clip.mRadii : nullptr);
  }
}

/**
 * This is used for the find bar highlighter overlay. It's only accessible
 * through the AnonymousContent API, so it's not exposed to general web pages.
 */
static bool SpecialCutoutRegionCase(nsDisplayListBuilder* aBuilder,
                                    nsIFrame* aFrame,
                                    const nsRect& aBackgroundRect,
                                    nsDisplayList* aList, nscolor aColor) {
  nsIContent* content = aFrame->GetContent();
  if (!content) {
    return false;
  }

  void* cutoutRegion = content->GetProperty(nsGkAtoms::cutoutregion);
  if (!cutoutRegion) {
    return false;
  }

  if (NS_GET_A(aColor) == 0) {
    return true;
  }

  nsRegion region;
  region.Sub(aBackgroundRect, *static_cast<nsRegion*>(cutoutRegion));
  region.MoveBy(aBuilder->ToReferenceFrame(aFrame));
  aList->AppendNewToTop<nsDisplaySolidColorRegion>(aBuilder, aFrame, region,
                                                   aColor);

  return true;
}

enum class TableType : uint8_t {
  Table,
  TableCol,
  TableColGroup,
  TableRow,
  TableRowGroup,
  TableCell,

  MAX,
};

enum class TableTypeBits : uint8_t { Count = 3 };

static_assert(static_cast<uint8_t>(TableType::MAX) <
                  (1 << (static_cast<uint8_t>(TableTypeBits::Count) + 1)),
              "TableType cannot fit with TableTypeBits::Count");
TableType GetTableTypeFromFrame(nsIFrame* aFrame);

static uint16_t CalculateTablePerFrameKey(const uint16_t aIndex,
                                          const TableType aType) {
  const uint32_t key = (aIndex << static_cast<uint8_t>(TableTypeBits::Count)) |
                       static_cast<uint8_t>(aType);

  return static_cast<uint16_t>(key);
}

static nsDisplayBackgroundImage* CreateBackgroundImage(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsIFrame* aSecondaryFrame,
    const nsDisplayBackgroundImage::InitData& aBgData) {
  const auto index = aBgData.layer;

  if (aSecondaryFrame) {
    const auto tableType = GetTableTypeFromFrame(aFrame);
    const uint16_t tableItemIndex = CalculateTablePerFrameKey(index, tableType);

    return MakeDisplayItemWithIndex<nsDisplayTableBackgroundImage>(
        aBuilder, aSecondaryFrame, tableItemIndex, aBgData, aFrame);
  }

  return MakeDisplayItemWithIndex<nsDisplayBackgroundImage>(aBuilder, aFrame,
                                                            index, aBgData);
}

static nsDisplayThemedBackground* CreateThemedBackground(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsIFrame* aSecondaryFrame,
    const nsRect& aBgRect) {
  if (aSecondaryFrame) {
    const uint16_t index = static_cast<uint16_t>(GetTableTypeFromFrame(aFrame));
    return MakeDisplayItemWithIndex<nsDisplayTableThemedBackground>(
        aBuilder, aSecondaryFrame, index, aBgRect, aFrame);
  }

  return MakeDisplayItem<nsDisplayThemedBackground>(aBuilder, aFrame, aBgRect);
}

static nsDisplayBackgroundColor* CreateBackgroundColor(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsIFrame* aSecondaryFrame,
    nsRect& aBgRect, const ComputedStyle* aBgSC, nscolor aColor) {
  if (aSecondaryFrame) {
    const uint16_t index = static_cast<uint16_t>(GetTableTypeFromFrame(aFrame));
    return MakeDisplayItemWithIndex<nsDisplayTableBackgroundColor>(
        aBuilder, aSecondaryFrame, index, aBgRect, aBgSC, aColor, aFrame);
  }

  return MakeDisplayItem<nsDisplayBackgroundColor>(aBuilder, aFrame, aBgRect,
                                                   aBgSC, aColor);
}

static void DealWithWindowsAppearanceHacks(nsIFrame* aFrame,
                                           nsDisplayListBuilder* aBuilder) {
  const auto& disp = *aFrame->StyleDisplay();

  // We use default appearance rather than effective appearance because we want
  // to handle when titlebar buttons that have appearance: none.
  const auto defaultAppearance = disp.mDefaultAppearance;
  if (MOZ_LIKELY(defaultAppearance == StyleAppearance::None)) {
    return;
  }

  if (auto type = disp.GetWindowButtonType()) {
    if (auto* widget = aFrame->GetNearestWidget()) {
      auto rect = LayoutDevicePixel::FromAppUnitsToNearest(
          nsRect(aBuilder->ToReferenceFrame(aFrame), aFrame->GetSize()),
          aFrame->PresContext()->AppUnitsPerDevPixel());
      widget->SetWindowButtonRect(*type, rect);
    }
  }
}

/*static*/
AppendedBackgroundType nsDisplayBackgroundImage::AppendBackgroundItemsToTop(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
    const nsRect& aBackgroundRect, nsDisplayList* aList,
    bool aAllowWillPaintBorderOptimization, const nsRect& aBackgroundOriginRect,
    nsIFrame* aSecondaryReferenceFrame,
    Maybe<nsDisplayListBuilder::AutoBuildingDisplayList>*
        aAutoBuildingDisplayList) {
  MOZ_ASSERT(!aFrame->IsCanvasFrame(),
             "We don't expect propagated canvas backgrounds here");
#ifdef DEBUG
  {
    nsIFrame* bgFrame = nsCSSRendering::FindBackgroundFrame(aFrame);
    MOZ_ASSERT(
        !bgFrame || bgFrame == aFrame,
        "Should only suppress backgrounds, never propagate to another frame");
  }
#endif

  DealWithWindowsAppearanceHacks(aFrame, aBuilder);

  const bool isThemed = aFrame->IsThemed();

  const ComputedStyle* bgSC = aFrame->Style();
  const nsStyleBackground* bg = bgSC->StyleBackground();
  const bool needsBackgroundColor =
      aBuilder->IsForEventDelivery() ||
      (EffectCompositor::HasAnimationsForCompositor(
           aFrame, DisplayItemType::TYPE_BACKGROUND_COLOR) &&
       !isThemed);
  if (!needsBackgroundColor && !isThemed && bg->IsTransparent(bgSC)) {
    return AppendedBackgroundType::None;
  }

  bool drawBackgroundColor = false;
  bool drawBackgroundImage = false;
  nscolor color = NS_RGBA(0, 0, 0, 0);
  // Don't get background color / images if we propagated our background to the
  // canvas (that is, if FindBackgroundFrame is null). But don't early return
  // yet, since we might still need a background-color item for hit-testing.
  if (!isThemed && nsCSSRendering::FindBackgroundFrame(aFrame)) {
    color = nsCSSRendering::DetermineBackgroundColor(
        aFrame->PresContext(), bgSC, aFrame, drawBackgroundImage,
        drawBackgroundColor);
  }

  if (SpecialCutoutRegionCase(aBuilder, aFrame, aBackgroundRect, aList,
                              color)) {
    return AppendedBackgroundType::None;
  }

  const nsStyleBorder& border = *aFrame->StyleBorder();
  const bool willPaintBorder =
      aAllowWillPaintBorderOptimization && !isThemed &&
      !aFrame->StyleEffects()->HasBoxShadowWithInset(true) &&
      border.HasBorder();

  auto EnsureBuildingDisplayList = [&] {
    if (!aAutoBuildingDisplayList || *aAutoBuildingDisplayList) {
      return;
    }
    nsPoint offset = aBuilder->GetCurrentFrame()->GetOffsetTo(aFrame);
    aAutoBuildingDisplayList->emplace(aBuilder, aFrame,
                                      aBuilder->GetVisibleRect() + offset,
                                      aBuilder->GetDirtyRect() + offset);
  };

  // An auxiliary list is necessary in case we have background blending; if that
  // is the case, background items need to be wrapped by a blend container to
  // isolate blending to the background
  nsDisplayList bgItemList(aBuilder);
  // Even if we don't actually have a background color to paint, we may still
  // need to create an item for hit testing and we still need to create an item
  // for background-color animations.
  if ((drawBackgroundColor && color != NS_RGBA(0, 0, 0, 0)) ||
      needsBackgroundColor) {
    EnsureBuildingDisplayList();
    Maybe<DisplayListClipState::AutoSaveRestore> clipState;
    nsRect bgColorRect = aBackgroundRect;
    if (!isThemed && !aBuilder->IsForEventDelivery()) {
      // Disable the will-paint-border optimization for background
      // colors with no border-radius. Enabling it for background colors
      // doesn't help much (there are no tiling issues) and clipping the
      // background breaks detection of the element's border-box being
      // opaque. For nonzero border-radius we still need it because we
      // want to inset the background if possible to avoid antialiasing
      // artifacts along the rounded corners.
      const bool useWillPaintBorderOptimization =
          willPaintBorder &&
          nsLayoutUtils::HasNonZeroCorner(border.mBorderRadius);

      nsCSSRendering::ImageLayerClipState clip;
      nsCSSRendering::GetImageLayerClip(
          bg->BottomLayer(), aFrame, border, aBackgroundRect, aBackgroundRect,
          useWillPaintBorderOptimization,
          aFrame->PresContext()->AppUnitsPerDevPixel(), &clip);

      bgColorRect = bgColorRect.Intersect(clip.mBGClipArea);
      if (clip.mHasAdditionalBGClipArea) {
        bgColorRect = bgColorRect.Intersect(clip.mAdditionalBGClipArea);
      }
      if (clip.mHasRoundedCorners) {
        clipState.emplace(aBuilder);
        clipState->ClipContentDescendants(clip.mBGClipArea, clip.mRadii);
      }
    }

    nsDisplayBackgroundColor* bgItem = CreateBackgroundColor(
        aBuilder, aFrame, aSecondaryReferenceFrame, bgColorRect, bgSC,
        drawBackgroundColor ? color : NS_RGBA(0, 0, 0, 0));

    if (bgItem) {
      bgItemList.AppendToTop(bgItem);
    }
  }

  if (isThemed) {
    nsDisplayThemedBackground* bgItem = CreateThemedBackground(
        aBuilder, aFrame, aSecondaryReferenceFrame, aBackgroundRect);

    if (bgItem) {
      bgItem->Init(aBuilder);
      bgItemList.AppendToTop(bgItem);
    }

    if (!bgItemList.IsEmpty()) {
      aList->AppendToTop(&bgItemList);
      return AppendedBackgroundType::ThemedBackground;
    }

    return AppendedBackgroundType::None;
  }

  if (!drawBackgroundImage) {
    if (!bgItemList.IsEmpty()) {
      aList->AppendToTop(&bgItemList);
      return AppendedBackgroundType::Background;
    }

    return AppendedBackgroundType::None;
  }

  const ActiveScrolledRoot* asr = aBuilder->CurrentActiveScrolledRoot();

  bool needBlendContainer = false;
  const nsRect& bgOriginRect =
      aBackgroundOriginRect.IsEmpty() ? aBackgroundRect : aBackgroundOriginRect;

  // Passing bg == nullptr in this macro will result in one iteration with
  // i = 0.
  NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT(i, bg->mImage) {
    if (bg->mImage.mLayers[i].mImage.IsNone()) {
      continue;
    }

    EnsureBuildingDisplayList();

    if (bg->mImage.mLayers[i].mBlendMode != StyleBlend::Normal) {
      needBlendContainer = true;
    }

    DisplayListClipState::AutoSaveRestore clipState(aBuilder);
    if (!aBuilder->IsForEventDelivery()) {
      const nsStyleImageLayers::Layer& layer = bg->mImage.mLayers[i];
      SetBackgroundClipRegion(clipState, aFrame, layer, aBackgroundRect,
                              willPaintBorder);
    }

    nsDisplayList thisItemList(aBuilder);
    nsDisplayBackgroundImage::InitData bgData =
        nsDisplayBackgroundImage::GetInitData(aBuilder, aFrame, i, bgOriginRect,
                                              bgSC);

    if (bgData.shouldFixToViewport) {
      auto* displayData = aBuilder->GetCurrentFixedBackgroundDisplayData();
      nsDisplayListBuilder::AutoBuildingDisplayList buildingDisplayList(
          aBuilder, aFrame, aBuilder->GetVisibleRect(),
          aBuilder->GetDirtyRect());

      nsDisplayListBuilder::AutoCurrentActiveScrolledRootSetter asrSetter(
          aBuilder);
      if (displayData) {
        asrSetter.SetCurrentActiveScrolledRoot(
            displayData->mContainingBlockActiveScrolledRoot);
        asrSetter.SetCurrentScrollParentId(displayData->mScrollParentId);
        if (nsLayoutUtils::UsesAsyncScrolling(aFrame)) {
          // Override the dirty rect on the builder to be the dirty rect of
          // the viewport.
          // displayData->mDirtyRect is relative to the presshell's viewport
          // frame (the root frame), and we need it to be relative to aFrame.
          nsIFrame* rootFrame =
              aBuilder->CurrentPresShellState()->mPresShell->GetRootFrame();
          // There cannot be any transforms between aFrame and rootFrame
          // because then bgData.shouldFixToViewport would have been false.
          nsRect visibleRect =
              displayData->mVisibleRect + aFrame->GetOffsetTo(rootFrame);
          aBuilder->SetVisibleRect(visibleRect);
          nsRect dirtyRect =
              displayData->mDirtyRect + aFrame->GetOffsetTo(rootFrame);
          aBuilder->SetDirtyRect(dirtyRect);
        }
      }

      nsDisplayBackgroundImage* bgItem = nullptr;
      {
        // The clip is captured by the nsDisplayFixedPosition, so clear the
        // clip for the nsDisplayBackgroundImage inside.
        DisplayListClipState::AutoSaveRestore bgImageClip(aBuilder);
        bgImageClip.Clear();
        bgItem = CreateBackgroundImage(aBuilder, aFrame,
                                       aSecondaryReferenceFrame, bgData);
      }
      if (bgItem) {
        thisItemList.AppendToTop(
            nsDisplayFixedPosition::CreateForFixedBackground(
                aBuilder, aFrame, aSecondaryReferenceFrame, bgItem, i, asr));
      }
    } else {  // bgData.shouldFixToViewport == false
      nsDisplayBackgroundImage* bgItem = CreateBackgroundImage(
          aBuilder, aFrame, aSecondaryReferenceFrame, bgData);
      if (bgItem) {
        thisItemList.AppendToTop(bgItem);
      }
    }

    if (bg->mImage.mLayers[i].mBlendMode != StyleBlend::Normal) {
      // asr is scrolled. Even if we wrap a fixed background layer, that's
      // fine, because the item will have a scrolled clip that limits the
      // item with respect to asr.
      if (aSecondaryReferenceFrame) {
        const auto tableType = GetTableTypeFromFrame(aFrame);
        const uint16_t index = CalculateTablePerFrameKey(i + 1, tableType);

        thisItemList.AppendNewToTopWithIndex<nsDisplayTableBlendMode>(
            aBuilder, aSecondaryReferenceFrame, index, &thisItemList,
            bg->mImage.mLayers[i].mBlendMode, asr, aFrame, true);
      } else {
        thisItemList.AppendNewToTopWithIndex<nsDisplayBlendMode>(
            aBuilder, aFrame, i + 1, &thisItemList,
            bg->mImage.mLayers[i].mBlendMode, asr, true);
      }
    }
    bgItemList.AppendToTop(&thisItemList);
  }

  if (needBlendContainer) {
    bgItemList.AppendToTop(
        nsDisplayBlendContainer::CreateForBackgroundBlendMode(
            aBuilder, aFrame, aSecondaryReferenceFrame, &bgItemList, asr));
  }

  if (!bgItemList.IsEmpty()) {
    aList->AppendToTop(&bgItemList);
    return AppendedBackgroundType::Background;
  }

  return AppendedBackgroundType::None;
}

// Check that the rounded border of aFrame, added to aToReferenceFrame,
// intersects aRect.  Assumes that the unrounded border has already
// been checked for intersection.
static bool RoundedBorderIntersectsRect(nsIFrame* aFrame,
                                        const nsPoint& aFrameToReferenceFrame,
                                        const nsRect& aTestRect) {
  if (!nsRect(aFrameToReferenceFrame, aFrame->GetSize())
           .Intersects(aTestRect)) {
    return false;
  }

  nscoord radii[8];
  return !aFrame->GetBorderRadii(radii) ||
         nsLayoutUtils::RoundedRectIntersectsRect(
             nsRect(aFrameToReferenceFrame, aFrame->GetSize()), radii,
             aTestRect);
}

// Returns TRUE if aContainedRect is guaranteed to be contained in
// the rounded rect defined by aRoundedRect and aRadii. Complex cases are
// handled conservatively by returning FALSE in some situations where
// a more thorough analysis could return TRUE.
//
// See also RoundedRectIntersectsRect.
static bool RoundedRectContainsRect(const nsRect& aRoundedRect,
                                    const nscoord aRadii[8],
                                    const nsRect& aContainedRect) {
  nsRegion rgn = nsLayoutUtils::RoundedRectIntersectRect(aRoundedRect, aRadii,
                                                         aContainedRect);
  return rgn.Contains(aContainedRect);
}

bool nsDisplayBackgroundImage::CanApplyOpacity(
    WebRenderLayerManager* aManager, nsDisplayListBuilder* aBuilder) const {
  return CanBuildWebRenderDisplayItems(aManager, aBuilder);
}

bool nsDisplayBackgroundImage::CanBuildWebRenderDisplayItems(
    WebRenderLayerManager* aManager, nsDisplayListBuilder* aBuilder) const {
  return mBackgroundStyle->StyleBackground()->mImage.mLayers[mLayer].mClip !=
             StyleGeometryBox::Text &&
         nsCSSRendering::CanBuildWebRenderDisplayItemsForStyleImageLayer(
             aManager, *StyleFrame()->PresContext(), StyleFrame(),
             mBackgroundStyle->StyleBackground(), mLayer,
             aBuilder->GetBackgroundPaintFlags());
}

bool nsDisplayBackgroundImage::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  if (!CanBuildWebRenderDisplayItems(aManager->LayerManager(),
                                     aDisplayListBuilder)) {
    return false;
  }

  uint32_t paintFlags = aDisplayListBuilder->GetBackgroundPaintFlags();
  bool dummy;
  nsCSSRendering::PaintBGParams params =
      nsCSSRendering::PaintBGParams::ForSingleLayer(
          *StyleFrame()->PresContext(), GetBounds(aDisplayListBuilder, &dummy),
          mBackgroundRect, StyleFrame(), paintFlags, mLayer,
          CompositionOp::OP_OVER, aBuilder.GetInheritedOpacity());
  params.bgClipRect = &mBounds;
  ImgDrawResult result =
      nsCSSRendering::BuildWebRenderDisplayItemsForStyleImageLayer(
          params, aBuilder, aResources, aSc, aManager, this);
  if (result == ImgDrawResult::NOT_SUPPORTED) {
    return false;
  }

  if (nsIContent* content = StyleFrame()->GetContent()) {
    if (imgRequestProxy* requestProxy = mBackgroundStyle->StyleBackground()
                                            ->mImage.mLayers[mLayer]
                                            .mImage.GetImageRequest()) {
      // LCP don't consider gradient backgrounds.
      LCPHelpers::FinalizeLCPEntryForImage(content->AsElement(), requestProxy,
                                           mBounds - ToReferenceFrame());
    }
  }

  return true;
}

void nsDisplayBackgroundImage::HitTest(nsDisplayListBuilder* aBuilder,
                                       const nsRect& aRect,
                                       HitTestState* aState,
                                       nsTArray<nsIFrame*>* aOutFrames) {
  if (RoundedBorderIntersectsRect(mFrame, ToReferenceFrame(), aRect)) {
    aOutFrames->AppendElement(mFrame);
  }
}

static nsRect GetInsideClipRect(const nsDisplayItem* aItem,
                                StyleGeometryBox aClip, const nsRect& aRect,
                                const nsRect& aBackgroundRect) {
  if (aRect.IsEmpty()) {
    return {};
  }

  nsIFrame* frame = aItem->Frame();

  nsRect clipRect = aBackgroundRect;
  if (frame->IsCanvasFrame()) {
    nsCanvasFrame* canvasFrame = static_cast<nsCanvasFrame*>(frame);
    clipRect = canvasFrame->CanvasArea() + aItem->ToReferenceFrame();
  } else if (aClip == StyleGeometryBox::PaddingBox ||
             aClip == StyleGeometryBox::ContentBox) {
    nsMargin border = frame->GetUsedBorder();
    if (aClip == StyleGeometryBox::ContentBox) {
      border += frame->GetUsedPadding();
    }
    border.ApplySkipSides(frame->GetSkipSides());
    clipRect.Deflate(border);
  }

  return clipRect.Intersect(aRect);
}

nsRegion nsDisplayBackgroundImage::GetOpaqueRegion(
    nsDisplayListBuilder* aBuilder, bool* aSnap) const {
  nsRegion result;
  *aSnap = false;

  if (!mBackgroundStyle) {
    return result;
  }

  *aSnap = true;

  // For StyleBoxDecorationBreak::Slice, don't try to optimize here, since
  // this could easily lead to O(N^2) behavior inside InlineBackgroundData,
  // which expects frames to be sent to it in content order, not reverse
  // content order which we'll produce here.
  // Of course, if there's only one frame in the flow, it doesn't matter.
  if (mFrame->StyleBorder()->mBoxDecorationBreak ==
          StyleBoxDecorationBreak::Clone ||
      (!mFrame->GetPrevContinuation() && !mFrame->GetNextContinuation())) {
    const nsStyleImageLayers::Layer& layer =
        mBackgroundStyle->StyleBackground()->mImage.mLayers[mLayer];
    if (layer.mImage.IsOpaque() && layer.mBlendMode == StyleBlend::Normal &&
        layer.mRepeat.mXRepeat != StyleImageLayerRepeat::Space &&
        layer.mRepeat.mYRepeat != StyleImageLayerRepeat::Space &&
        layer.mClip != StyleGeometryBox::Text) {
      result = GetInsideClipRect(this, layer.mClip, mBounds, mBackgroundRect);
    }
  }

  return result;
}

Maybe<nscolor> nsDisplayBackgroundImage::IsUniform(
    nsDisplayListBuilder* aBuilder) const {
  if (!mBackgroundStyle) {
    return Some(NS_RGBA(0, 0, 0, 0));
  }
  return Nothing();
}

nsRect nsDisplayBackgroundImage::GetPositioningArea() const {
  if (!mBackgroundStyle) {
    return nsRect();
  }
  nsIFrame* attachedToFrame;
  bool transformedFixed;
  return nsCSSRendering::ComputeImageLayerPositioningArea(
             mFrame->PresContext(), mFrame, mBackgroundRect,
             mBackgroundStyle->StyleBackground()->mImage.mLayers[mLayer],
             &attachedToFrame, &transformedFixed) +
         ToReferenceFrame();
}

bool nsDisplayBackgroundImage::RenderingMightDependOnPositioningAreaSizeChange()
    const {
  if (!mBackgroundStyle) {
    return false;
  }

  nscoord radii[8];
  if (mFrame->GetBorderRadii(radii)) {
    // A change in the size of the positioning area might change the position
    // of the rounded corners.
    return true;
  }

  const nsStyleImageLayers::Layer& layer =
      mBackgroundStyle->StyleBackground()->mImage.mLayers[mLayer];
  return layer.RenderingMightDependOnPositioningAreaSizeChange();
}

void nsDisplayBackgroundImage::Paint(nsDisplayListBuilder* aBuilder,
                                     gfxContext* aCtx) {
  PaintInternal(aBuilder, aCtx, GetPaintRect(aBuilder, aCtx), &mBounds);
}

void nsDisplayBackgroundImage::PaintInternal(nsDisplayListBuilder* aBuilder,
                                             gfxContext* aCtx,
                                             const nsRect& aBounds,
                                             nsRect* aClipRect) {
  gfxContext* ctx = aCtx;
  StyleGeometryBox clip =
      mBackgroundStyle->StyleBackground()->mImage.mLayers[mLayer].mClip;

  if (clip == StyleGeometryBox::Text) {
    if (!GenerateAndPushTextMask(StyleFrame(), aCtx, mBackgroundRect,
                                 aBuilder)) {
      return;
    }
  }

  nsCSSRendering::PaintBGParams params =
      nsCSSRendering::PaintBGParams::ForSingleLayer(
          *StyleFrame()->PresContext(), aBounds, mBackgroundRect, StyleFrame(),
          aBuilder->GetBackgroundPaintFlags(), mLayer, CompositionOp::OP_OVER,
          1.0f);
  params.bgClipRect = aClipRect;
  Unused << nsCSSRendering::PaintStyleImageLayer(params, *aCtx);

  if (clip == StyleGeometryBox::Text) {
    ctx->PopGroupAndBlend();
  }
}

void nsDisplayBackgroundImage::ComputeInvalidationRegion(
    nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
    nsRegion* aInvalidRegion) const {
  if (!mBackgroundStyle) {
    return;
  }

  const auto* geometry =
      static_cast<const nsDisplayBackgroundGeometry*>(aGeometry);

  bool snap;
  nsRect bounds = GetBounds(aBuilder, &snap);
  nsRect positioningArea = GetPositioningArea();
  if (positioningArea.TopLeft() != geometry->mPositioningArea.TopLeft() ||
      (positioningArea.Size() != geometry->mPositioningArea.Size() &&
       RenderingMightDependOnPositioningAreaSizeChange())) {
    // Positioning area changed in a way that could cause everything to change,
    // so invalidate everything (both old and new painting areas).
    aInvalidRegion->Or(bounds, geometry->mBounds);
    return;
  }
  if (!mDestRect.IsEqualInterior(geometry->mDestRect)) {
    // Dest area changed in a way that could cause everything to change,
    // so invalidate everything (both old and new painting areas).
    aInvalidRegion->Or(bounds, geometry->mBounds);
    return;
  }
  if (!bounds.IsEqualInterior(geometry->mBounds)) {
    // Positioning area is unchanged, so invalidate just the change in the
    // painting area.
    aInvalidRegion->Xor(bounds, geometry->mBounds);
  }
}

nsRect nsDisplayBackgroundImage::GetBounds(nsDisplayListBuilder* aBuilder,
                                           bool* aSnap) const {
  *aSnap = true;
  return mBounds;
}

nsRect nsDisplayBackgroundImage::GetBoundsInternal(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrameForBounds) {
  // This allows nsDisplayTableBackgroundImage to change the frame used for
  // bounds calculation.
  nsIFrame* frame = aFrameForBounds ? aFrameForBounds : mFrame;

  nsPresContext* presContext = frame->PresContext();

  if (!mBackgroundStyle) {
    return nsRect();
  }

  nsRect clipRect = mBackgroundRect;
  if (frame->IsCanvasFrame()) {
    nsCanvasFrame* canvasFrame = static_cast<nsCanvasFrame*>(frame);
    clipRect = canvasFrame->CanvasArea() + ToReferenceFrame();
  }
  const nsStyleImageLayers::Layer& layer =
      mBackgroundStyle->StyleBackground()->mImage.mLayers[mLayer];
  return nsCSSRendering::GetBackgroundLayerRect(
      presContext, frame, mBackgroundRect, clipRect, layer,
      aBuilder->GetBackgroundPaintFlags());
}

nsDisplayTableBackgroundImage::nsDisplayTableBackgroundImage(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, const InitData& aData,
    nsIFrame* aCellFrame)
    : nsDisplayBackgroundImage(aBuilder, aFrame, aData, aCellFrame),
      mStyleFrame(aCellFrame) {
  if (aBuilder->IsRetainingDisplayList()) {
    mStyleFrame->AddDisplayItem(this);
  }
}

nsDisplayTableBackgroundImage::~nsDisplayTableBackgroundImage() {
  if (mStyleFrame) {
    mStyleFrame->RemoveDisplayItem(this);
  }
}

bool nsDisplayTableBackgroundImage::IsInvalid(nsRect& aRect) const {
  bool result = mStyleFrame ? mStyleFrame->IsInvalid(aRect) : false;
  aRect += ToReferenceFrame();
  return result;
}

nsDisplayThemedBackground::nsDisplayThemedBackground(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
    const nsRect& aBackgroundRect)
    : nsPaintedDisplayItem(aBuilder, aFrame), mBackgroundRect(aBackgroundRect) {
  MOZ_COUNT_CTOR(nsDisplayThemedBackground);
}

void nsDisplayThemedBackground::Init(nsDisplayListBuilder* aBuilder) {
  const nsStyleDisplay* disp = StyleFrame()->StyleDisplay();
  mAppearance = disp->EffectiveAppearance();
  StyleFrame()->IsThemed(disp, &mThemeTransparency);

  // Perform necessary RegisterThemeGeometry
  nsITheme* theme = StyleFrame()->PresContext()->Theme();
  nsITheme::ThemeGeometryType type =
      theme->ThemeGeometryTypeForWidget(StyleFrame(), mAppearance);
  if (type != nsITheme::eThemeGeometryTypeUnknown) {
    RegisterThemeGeometry(aBuilder, this, StyleFrame(), type);
  }

  mBounds = GetBoundsInternal();
}

void nsDisplayThemedBackground::WriteDebugInfo(std::stringstream& aStream) {
  aStream << " (themed, appearance:" << (int)mAppearance << ")";
}

void nsDisplayThemedBackground::HitTest(nsDisplayListBuilder* aBuilder,
                                        const nsRect& aRect,
                                        HitTestState* aState,
                                        nsTArray<nsIFrame*>* aOutFrames) {
  // Assume that any point in our background rect is a hit.
  if (mBackgroundRect.Intersects(aRect)) {
    aOutFrames->AppendElement(mFrame);
  }
}

nsRegion nsDisplayThemedBackground::GetOpaqueRegion(
    nsDisplayListBuilder* aBuilder, bool* aSnap) const {
  nsRegion result;
  *aSnap = false;

  if (mThemeTransparency == nsITheme::eOpaque) {
    *aSnap = true;
    result = mBackgroundRect;
  }
  return result;
}

Maybe<nscolor> nsDisplayThemedBackground::IsUniform(
    nsDisplayListBuilder* aBuilder) const {
  return Nothing();
}

nsRect nsDisplayThemedBackground::GetPositioningArea() const {
  return mBackgroundRect;
}

void nsDisplayThemedBackground::Paint(nsDisplayListBuilder* aBuilder,
                                      gfxContext* aCtx) {
  PaintInternal(aBuilder, aCtx, GetPaintRect(aBuilder, aCtx), nullptr);
}

void nsDisplayThemedBackground::PaintInternal(nsDisplayListBuilder* aBuilder,
                                              gfxContext* aCtx,
                                              const nsRect& aBounds,
                                              nsRect* aClipRect) {
  // XXXzw this ignores aClipRect.
  nsPresContext* presContext = StyleFrame()->PresContext();
  nsITheme* theme = presContext->Theme();
  nsRect drawing(mBackgroundRect);
  theme->GetWidgetOverflow(presContext->DeviceContext(), StyleFrame(),
                           mAppearance, &drawing);
  drawing.IntersectRect(drawing, aBounds);
  theme->DrawWidgetBackground(aCtx, StyleFrame(), mAppearance, mBackgroundRect,
                              drawing);
}

bool nsDisplayThemedBackground::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  nsITheme* theme = StyleFrame()->PresContext()->Theme();
  return theme->CreateWebRenderCommandsForWidget(aBuilder, aResources, aSc,
                                                 aManager, StyleFrame(),
                                                 mAppearance, mBackgroundRect);
}

bool nsDisplayThemedBackground::IsWindowActive() const {
  return !mFrame->PresContext()->Document()->IsTopLevelWindowInactive();
}

void nsDisplayThemedBackground::ComputeInvalidationRegion(
    nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
    nsRegion* aInvalidRegion) const {
  const auto* geometry =
      static_cast<const nsDisplayThemedBackgroundGeometry*>(aGeometry);

  bool snap;
  nsRect bounds = GetBounds(aBuilder, &snap);
  nsRect positioningArea = GetPositioningArea();
  if (!positioningArea.IsEqualInterior(geometry->mPositioningArea)) {
    // Invalidate everything (both old and new painting areas).
    aInvalidRegion->Or(bounds, geometry->mBounds);
    return;
  }
  if (!bounds.IsEqualInterior(geometry->mBounds)) {
    // Positioning area is unchanged, so invalidate just the change in the
    // painting area.
    aInvalidRegion->Xor(bounds, geometry->mBounds);
  }
  nsITheme* theme = StyleFrame()->PresContext()->Theme();
  if (theme->WidgetAppearanceDependsOnWindowFocus(mAppearance) &&
      IsWindowActive() != geometry->mWindowIsActive) {
    aInvalidRegion->Or(*aInvalidRegion, bounds);
  }
}

nsRect nsDisplayThemedBackground::GetBounds(nsDisplayListBuilder* aBuilder,
                                            bool* aSnap) const {
  *aSnap = true;
  return mBounds;
}

nsRect nsDisplayThemedBackground::GetBoundsInternal() {
  nsPresContext* presContext = mFrame->PresContext();

  nsRect r = mBackgroundRect - ToReferenceFrame();
  presContext->Theme()->GetWidgetOverflow(
      presContext->DeviceContext(), mFrame,
      mFrame->StyleDisplay()->EffectiveAppearance(), &r);
  return r + ToReferenceFrame();
}

#if defined(MOZ_REFLOW_PERF_DSP) && defined(MOZ_REFLOW_PERF)
void nsDisplayReflowCount::Paint(nsDisplayListBuilder* aBuilder,
                                 gfxContext* aCtx) {
  mFrame->PresShell()->PaintCount(mFrameName, aCtx, mFrame->PresContext(),
                                  mFrame, ToReferenceFrame(), mColor);
}
#endif

bool nsDisplayBackgroundColor::CanApplyOpacity(
    WebRenderLayerManager* aManager, nsDisplayListBuilder* aBuilder) const {
  // Don't apply opacity if the background color is animated since the color is
  // going to be changed on the compositor.
  return !EffectCompositor::HasAnimationsForCompositor(
      mFrame, DisplayItemType::TYPE_BACKGROUND_COLOR);
}

bool nsDisplayBackgroundColor::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  gfx::sRGBColor color = mColor;
  color.a *= aBuilder.GetInheritedOpacity();

  if (color == sRGBColor() &&
      !EffectCompositor::HasAnimationsForCompositor(
          mFrame, DisplayItemType::TYPE_BACKGROUND_COLOR)) {
    return true;
  }

  if (HasBackgroundClipText()) {
    return false;
  }

  uint64_t animationsId = 0;
  // We don't support background-color animations on table elements yet.
  if (GetType() == DisplayItemType::TYPE_BACKGROUND_COLOR) {
    animationsId =
        AddAnimationsForWebRender(this, aManager, aDisplayListBuilder);
  }

  LayoutDeviceRect bounds = LayoutDeviceRect::FromAppUnits(
      mBackgroundRect, mFrame->PresContext()->AppUnitsPerDevPixel());
  wr::LayoutRect r = wr::ToLayoutRect(bounds);

  if (animationsId) {
    wr::WrAnimationProperty prop{
        wr::WrAnimationType::BackgroundColor,
        animationsId,
    };
    aBuilder.PushRectWithAnimation(r, r, !BackfaceIsHidden(),
                                   wr::ToColorF(ToDeviceColor(color)), &prop);
  } else {
    aBuilder.StartGroup(this);
    aBuilder.PushRect(r, r, !BackfaceIsHidden(), false, false,
                      wr::ToColorF(ToDeviceColor(color)));
    aBuilder.FinishGroup();
  }

  return true;
}

void nsDisplayBackgroundColor::PaintWithClip(nsDisplayListBuilder* aBuilder,
                                             gfxContext* aCtx,
                                             const DisplayItemClip& aClip) {
  MOZ_ASSERT(!HasBackgroundClipText());

  if (mColor == sRGBColor()) {
    return;
  }

  nsRect fillRect = mBackgroundRect;
  if (aClip.HasClip()) {
    fillRect.IntersectRect(fillRect, aClip.GetClipRect());
  }

  DrawTarget* dt = aCtx->GetDrawTarget();
  int32_t A2D = mFrame->PresContext()->AppUnitsPerDevPixel();
  Rect bounds = ToRect(nsLayoutUtils::RectToGfxRect(fillRect, A2D));
  MaybeSnapToDevicePixels(bounds, *dt);
  ColorPattern fill(ToDeviceColor(mColor));

  if (aClip.GetRoundedRectCount()) {
    MOZ_ASSERT(aClip.GetRoundedRectCount() == 1);

    AutoTArray<DisplayItemClip::RoundedRect, 1> roundedRect;
    aClip.AppendRoundedRects(&roundedRect);

    bool pushedClip = false;
    if (!fillRect.Contains(roundedRect[0].mRect)) {
      dt->PushClipRect(bounds);
      pushedClip = true;
    }

    RectCornerRadii pixelRadii;
    nsCSSRendering::ComputePixelRadii(roundedRect[0].mRadii, A2D, &pixelRadii);
    dt->FillRoundedRect(
        RoundedRect(NSRectToSnappedRect(roundedRect[0].mRect, A2D, *dt),
                    pixelRadii),
        fill);
    if (pushedClip) {
      dt->PopClip();
    }
  } else {
    dt->FillRect(bounds, fill);
  }
}

void nsDisplayBackgroundColor::Paint(nsDisplayListBuilder* aBuilder,
                                     gfxContext* aCtx) {
  if (mColor == sRGBColor()) {
    return;
  }

#if 0
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=1148418#c21 for why this
  // results in a precision induced rounding issue that makes the rect one
  // pixel shorter in rare cases. Disabled in favor of the old code for now.
  // Note that the pref layout.css.devPixelsPerPx needs to be set to 1 to
  // reproduce the bug.
  //
  // TODO:
  // This new path does not include support for background-clip:text; need to
  // be fixed if/when we switch to this new code path.

  DrawTarget& aDrawTarget = *aCtx->GetDrawTarget();

  Rect rect = NSRectToSnappedRect(mBackgroundRect,
                                  mFrame->PresContext()->AppUnitsPerDevPixel(),
                                  aDrawTarget);
  ColorPattern color(ToDeviceColor(mColor));
  aDrawTarget.FillRect(rect, color);
#else
  gfxContext* ctx = aCtx;
  gfxRect bounds = nsLayoutUtils::RectToGfxRect(
      mBackgroundRect, mFrame->PresContext()->AppUnitsPerDevPixel());

  if (HasBackgroundClipText()) {
    if (!GenerateAndPushTextMask(mFrame, aCtx, mBackgroundRect, aBuilder)) {
      return;
    }

    ctx->SetColor(mColor);
    ctx->NewPath();
    ctx->SnappedRectangle(bounds);
    ctx->Fill();
    ctx->PopGroupAndBlend();
    return;
  }

  ctx->SetColor(mColor);
  ctx->NewPath();
  ctx->SnappedRectangle(bounds);
  ctx->Fill();
#endif
}

nsRegion nsDisplayBackgroundColor::GetOpaqueRegion(
    nsDisplayListBuilder* aBuilder, bool* aSnap) const {
  *aSnap = false;

  if (mColor.a != 1 ||
      // Even if the current alpha channel is 1, we treat this item as if it's
      // non-opaque if there is a background-color animation since the animation
      // might change the alpha channel.
      EffectCompositor::HasAnimationsForCompositor(
          mFrame, DisplayItemType::TYPE_BACKGROUND_COLOR)) {
    return nsRegion();
  }

  if (!mHasStyle || HasBackgroundClipText()) {
    return nsRegion();
  }

  *aSnap = true;
  return GetInsideClipRect(this, mBottomLayerClip, mBackgroundRect,
                           mBackgroundRect);
}

Maybe<nscolor> nsDisplayBackgroundColor::IsUniform(
    nsDisplayListBuilder* aBuilder) const {
  return Some(mColor.ToABGR());
}

void nsDisplayBackgroundColor::HitTest(nsDisplayListBuilder* aBuilder,
                                       const nsRect& aRect,
                                       HitTestState* aState,
                                       nsTArray<nsIFrame*>* aOutFrames) {
  if (!RoundedBorderIntersectsRect(mFrame, ToReferenceFrame(), aRect)) {
    // aRect doesn't intersect our border-radius curve.
    return;
  }

  aOutFrames->AppendElement(mFrame);
}

void nsDisplayBackgroundColor::WriteDebugInfo(std::stringstream& aStream) {
  aStream << " (rgba " << mColor.r << "," << mColor.g << "," << mColor.b << ","
          << mColor.a << ")";
  aStream << " backgroundRect" << mBackgroundRect;
}

nsRect nsDisplayOutline::GetBounds(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) const {
  *aSnap = false;
  return mFrame->InkOverflowRectRelativeToSelf() + ToReferenceFrame();
}

nsRect nsDisplayOutline::GetInnerRect() const {
  if (nsRect* savedOutlineInnerRect =
          mFrame->GetProperty(nsIFrame::OutlineInnerRectProperty())) {
    return *savedOutlineInnerRect;
  }
  return mFrame->GetRectRelativeToSelf();
}

void nsDisplayOutline::Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) {
  // TODO join outlines together
  MOZ_ASSERT(mFrame->StyleOutline()->ShouldPaintOutline(),
             "Should have not created a nsDisplayOutline!");

  nsRect rect = GetInnerRect() + ToReferenceFrame();
  nsPresContext* pc = mFrame->PresContext();
  if (IsThemedOutline()) {
    rect.Inflate(mFrame->StyleOutline()->EffectiveOffsetFor(rect));
    pc->Theme()->DrawWidgetBackground(aCtx, mFrame,
                                      StyleAppearance::FocusOutline, rect,
                                      GetPaintRect(aBuilder, aCtx));
    return;
  }

  nsCSSRendering::PaintNonThemedOutline(
      pc, *aCtx, mFrame, GetPaintRect(aBuilder, aCtx), rect, mFrame->Style());
}

bool nsDisplayOutline::IsThemedOutline() const {
#ifdef DEBUG
  nsPresContext* pc = mFrame->PresContext();
  MOZ_ASSERT(
      pc->Theme()->ThemeSupportsWidget(pc, mFrame,
                                       StyleAppearance::FocusOutline),
      "All of our supported platforms have support for themed focus-outlines");
#endif
  return mFrame->StyleOutline()->mOutlineStyle.IsAuto();
}

bool nsDisplayOutline::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  nsPresContext* pc = mFrame->PresContext();
  nsRect rect = GetInnerRect() + ToReferenceFrame();
  if (IsThemedOutline()) {
    rect.Inflate(mFrame->StyleOutline()->EffectiveOffsetFor(rect));
    return pc->Theme()->CreateWebRenderCommandsForWidget(
        aBuilder, aResources, aSc, aManager, mFrame,
        StyleAppearance::FocusOutline, rect);
  }

  bool dummy;
  Maybe<nsCSSBorderRenderer> borderRenderer =
      nsCSSRendering::CreateBorderRendererForNonThemedOutline(
          pc, /* aDrawTarget = */ nullptr, mFrame,
          GetBounds(aDisplayListBuilder, &dummy), rect, mFrame->Style());

  if (!borderRenderer) {
    // No border renderer means "there is no outline".
    // Paint nothing and return success.
    return true;
  }

  borderRenderer->CreateWebRenderCommands(this, aBuilder, aResources, aSc);
  return true;
}

bool nsDisplayOutline::HasRadius() const {
  const auto& radius = mFrame->StyleBorder()->mBorderRadius;
  return !nsLayoutUtils::HasNonZeroCorner(radius);
}

bool nsDisplayOutline::IsInvisibleInRect(const nsRect& aRect) const {
  const nsStyleOutline* outline = mFrame->StyleOutline();
  nsRect borderBox(ToReferenceFrame(), mFrame->GetSize());
  if (borderBox.Contains(aRect) && !HasRadius() &&
      outline->mOutlineOffset.ToCSSPixels() >= 0.0f) {
    // aRect is entirely inside the border-rect, and the outline isn't rendered
    // inside the border-rect, so the outline is not visible.
    return true;
  }
  return false;
}

void nsDisplayEventReceiver::HitTest(nsDisplayListBuilder* aBuilder,
                                     const nsRect& aRect, HitTestState* aState,
                                     nsTArray<nsIFrame*>* aOutFrames) {
  if (!RoundedBorderIntersectsRect(mFrame, ToReferenceFrame(), aRect)) {
    // aRect doesn't intersect our border-radius curve.
    return;
  }

  aOutFrames->AppendElement(mFrame);
}

bool nsDisplayCompositorHitTestInfo::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  return true;
}

int32_t nsDisplayCompositorHitTestInfo::ZIndex() const {
  return mOverrideZIndex ? *mOverrideZIndex : nsDisplayItem::ZIndex();
}

void nsDisplayCompositorHitTestInfo::SetOverrideZIndex(int32_t aZIndex) {
  mOverrideZIndex = Some(aZIndex);
}

nsDisplayCaret::nsDisplayCaret(nsDisplayListBuilder* aBuilder,
                               nsIFrame* aCaretFrame)
    : nsPaintedDisplayItem(aBuilder, aCaretFrame),
      mCaret(aBuilder->GetCaret()),
      mBounds(aBuilder->GetCaretRect() + ToReferenceFrame()) {
  MOZ_COUNT_CTOR(nsDisplayCaret);
  // The presence of a caret doesn't change the overflow rect
  // of the owning frame, so the normal building rect might not
  // include the caret at all. We use MarkFrameForDisplay to ensure
  // we build this item, and here we override the building rect
  // to cover the pixels we're going to draw.
  SetBuildingRect(mBounds);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayCaret::~nsDisplayCaret() { MOZ_COUNT_DTOR(nsDisplayCaret); }
#endif

nsRect nsDisplayCaret::GetBounds(nsDisplayListBuilder* aBuilder,
                                 bool* aSnap) const {
  *aSnap = true;
  // The caret returns a rect in the coordinates of mFrame.
  return mBounds;
}

void nsDisplayCaret::Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) {
  // Note: Because we exist, we know that the caret is visible, so we don't
  // need to check for the caret's visibility.
  mCaret->PaintCaret(*aCtx->GetDrawTarget(), mFrame, ToReferenceFrame());
}

bool nsDisplayCaret::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  using namespace layers;
  nsRect caretRect;
  nsRect hookRect;
  nscolor caretColor;
  nsIFrame* frame =
      mCaret->GetPaintGeometry(&caretRect, &hookRect, &caretColor);
  if (NS_WARN_IF(!frame) || NS_WARN_IF(frame != mFrame)) {
    return true;
  }

  int32_t appUnitsPerDevPixel = frame->PresContext()->AppUnitsPerDevPixel();
  gfx::DeviceColor color = ToDeviceColor(caretColor);
  LayoutDeviceRect devCaretRect = LayoutDeviceRect::FromAppUnits(
      caretRect + ToReferenceFrame(), appUnitsPerDevPixel);
  LayoutDeviceRect devHookRect = LayoutDeviceRect::FromAppUnits(
      hookRect + ToReferenceFrame(), appUnitsPerDevPixel);

  wr::LayoutRect caret = wr::ToLayoutRect(devCaretRect);
  wr::LayoutRect hook = wr::ToLayoutRect(devHookRect);

  // Note, WR will pixel snap anything that is layout aligned.
  aBuilder.PushRect(caret, caret, !BackfaceIsHidden(), false, false,
                    wr::ToColorF(color));

  if (!devHookRect.IsEmpty()) {
    aBuilder.PushRect(hook, hook, !BackfaceIsHidden(), false, false,
                      wr::ToColorF(color));
  }
  return true;
}

nsDisplayBorder::nsDisplayBorder(nsDisplayListBuilder* aBuilder,
                                 nsIFrame* aFrame)
    : nsPaintedDisplayItem(aBuilder, aFrame) {
  MOZ_COUNT_CTOR(nsDisplayBorder);

  mBounds = CalculateBounds<nsRect>(*mFrame->StyleBorder());
}

bool nsDisplayBorder::IsInvisibleInRect(const nsRect& aRect) const {
  nsRect paddingRect = GetPaddingRect();
  const nsStyleBorder* styleBorder;
  if (paddingRect.Contains(aRect) &&
      !(styleBorder = mFrame->StyleBorder())->IsBorderImageSizeAvailable() &&
      !nsLayoutUtils::HasNonZeroCorner(styleBorder->mBorderRadius)) {
    // aRect is entirely inside the content rect, and no part
    // of the border is rendered inside the content rect, so we are not
    // visible
    // Skip this if there's a border-image (which draws a background
    // too) or if there is a border-radius (which makes the border draw
    // further in).
    return true;
  }

  return false;
}

nsDisplayItemGeometry* nsDisplayBorder::AllocateGeometry(
    nsDisplayListBuilder* aBuilder) {
  return new nsDisplayBorderGeometry(this, aBuilder);
}

void nsDisplayBorder::ComputeInvalidationRegion(
    nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
    nsRegion* aInvalidRegion) const {
  const auto* geometry = static_cast<const nsDisplayBorderGeometry*>(aGeometry);
  bool snap;

  if (!geometry->mBounds.IsEqualInterior(GetBounds(aBuilder, &snap))) {
    // We can probably get away with only invalidating the difference
    // between the border and padding rects, but the XUL ui at least
    // is apparently painting a background with this?
    aInvalidRegion->Or(GetBounds(aBuilder, &snap), geometry->mBounds);
  }
}

bool nsDisplayBorder::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  nsRect rect = nsRect(ToReferenceFrame(), mFrame->GetSize());

  ImgDrawResult drawResult = nsCSSRendering::CreateWebRenderCommandsForBorder(
      this, mFrame, rect, aBuilder, aResources, aSc, aManager,
      aDisplayListBuilder);

  if (drawResult == ImgDrawResult::NOT_SUPPORTED) {
    return false;
  }
  return true;
};

void nsDisplayBorder::Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) {
  nsPoint offset = ToReferenceFrame();

  PaintBorderFlags flags = aBuilder->ShouldSyncDecodeImages()
                               ? PaintBorderFlags::SyncDecodeImages
                               : PaintBorderFlags();

  Unused << nsCSSRendering::PaintBorder(
      mFrame->PresContext(), *aCtx, mFrame, GetPaintRect(aBuilder, aCtx),
      nsRect(offset, mFrame->GetSize()), mFrame->Style(), flags,
      mFrame->GetSkipSides());
}

nsRect nsDisplayBorder::GetBounds(nsDisplayListBuilder* aBuilder,
                                  bool* aSnap) const {
  *aSnap = true;
  return mBounds;
}

void nsDisplayBoxShadowOuter::Paint(nsDisplayListBuilder* aBuilder,
                                    gfxContext* aCtx) {
  nsPoint offset = ToReferenceFrame();
  nsRect borderRect = mFrame->VisualBorderRectRelativeToSelf() + offset;
  nsPresContext* presContext = mFrame->PresContext();

  AUTO_PROFILER_LABEL("nsDisplayBoxShadowOuter::Paint", GRAPHICS);

  nsCSSRendering::PaintBoxShadowOuter(presContext, *aCtx, mFrame, borderRect,
                                      GetPaintRect(aBuilder, aCtx), 1.0f);
}

nsRect nsDisplayBoxShadowOuter::GetBounds(nsDisplayListBuilder* aBuilder,
                                          bool* aSnap) const {
  *aSnap = false;
  return mBounds;
}

nsRect nsDisplayBoxShadowOuter::GetBoundsInternal() {
  return nsLayoutUtils::GetBoxShadowRectForFrame(mFrame, mFrame->GetSize()) +
         ToReferenceFrame();
}

bool nsDisplayBoxShadowOuter::IsInvisibleInRect(const nsRect& aRect) const {
  nsPoint origin = ToReferenceFrame();
  nsRect frameRect(origin, mFrame->GetSize());
  if (!frameRect.Contains(aRect)) {
    return false;
  }

  // the visible region is entirely inside the border-rect, and box shadows
  // never render within the border-rect (unless there's a border radius).
  nscoord twipsRadii[8];
  bool hasBorderRadii = mFrame->GetBorderRadii(twipsRadii);
  if (!hasBorderRadii) {
    return true;
  }

  return RoundedRectContainsRect(frameRect, twipsRadii, aRect);
}

bool nsDisplayBoxShadowOuter::CanBuildWebRenderDisplayItems() const {
  auto shadows = mFrame->StyleEffects()->mBoxShadow.AsSpan();
  if (shadows.IsEmpty()) {
    return false;
  }

  bool hasBorderRadius;
  // We don't support native themed things yet like box shadows around
  // input buttons.
  //
  // TODO(emilio): The non-native theme could provide the right rect+radius
  // instead relatively painlessly, if we find this causes performance issues or
  // what not.
  return !nsCSSRendering::HasBoxShadowNativeTheme(mFrame, hasBorderRadius);
}

bool nsDisplayBoxShadowOuter::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  if (!CanBuildWebRenderDisplayItems()) {
    return false;
  }

  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  nsPoint offset = ToReferenceFrame();
  nsRect borderRect = mFrame->VisualBorderRectRelativeToSelf() + offset;
  bool snap;
  nsRect bounds = GetBounds(aDisplayListBuilder, &snap);

  bool hasBorderRadius;
  bool nativeTheme =
      nsCSSRendering::HasBoxShadowNativeTheme(mFrame, hasBorderRadius);

  // Don't need the full size of the shadow rect like we do in
  // nsCSSRendering since WR takes care of calculations for blur
  // and spread radius.
  nsRect frameRect =
      nsCSSRendering::GetShadowRect(borderRect, nativeTheme, mFrame);

  RectCornerRadii borderRadii;
  if (hasBorderRadius) {
    hasBorderRadius = nsCSSRendering::GetBorderRadii(frameRect, borderRect,
                                                     mFrame, borderRadii);
  }

  // Everything here is in app units, change to device units.
  LayoutDeviceRect clipRect =
      LayoutDeviceRect::FromAppUnits(bounds, appUnitsPerDevPixel);
  auto shadows = mFrame->StyleEffects()->mBoxShadow.AsSpan();
  MOZ_ASSERT(!shadows.IsEmpty());

  for (const auto& shadow : Reversed(shadows)) {
    if (shadow.inset) {
      continue;
    }

    float blurRadius =
        float(shadow.base.blur.ToAppUnits()) / float(appUnitsPerDevPixel);
    gfx::sRGBColor shadowColor = nsCSSRendering::GetShadowColor(
        shadow.base, mFrame, aBuilder.GetInheritedOpacity());

    // We don't move the shadow rect here since WR does it for us
    // Now translate everything to device pixels.
    const nsRect& shadowRect = frameRect;
    LayoutDevicePoint shadowOffset = LayoutDevicePoint::FromAppUnits(
        nsPoint(shadow.base.horizontal.ToAppUnits(),
                shadow.base.vertical.ToAppUnits()),
        appUnitsPerDevPixel);

    LayoutDeviceRect deviceBox =
        LayoutDeviceRect::FromAppUnits(shadowRect, appUnitsPerDevPixel);
    wr::LayoutRect deviceBoxRect = wr::ToLayoutRect(deviceBox);
    wr::LayoutRect deviceClipRect = wr::ToLayoutRect(clipRect);

    LayoutDeviceSize zeroSize;
    wr::BorderRadius borderRadius =
        wr::ToBorderRadius(zeroSize, zeroSize, zeroSize, zeroSize);
    if (hasBorderRadius) {
      borderRadius = wr::ToBorderRadius(
          LayoutDeviceSize::FromUnknownSize(borderRadii.TopLeft()),
          LayoutDeviceSize::FromUnknownSize(borderRadii.TopRight()),
          LayoutDeviceSize::FromUnknownSize(borderRadii.BottomLeft()),
          LayoutDeviceSize::FromUnknownSize(borderRadii.BottomRight()));
    }

    float spreadRadius =
        float(shadow.spread.ToAppUnits()) / float(appUnitsPerDevPixel);

    aBuilder.PushBoxShadow(deviceBoxRect, deviceClipRect, !BackfaceIsHidden(),
                           deviceBoxRect, wr::ToLayoutVector2D(shadowOffset),
                           wr::ToColorF(ToDeviceColor(shadowColor)), blurRadius,
                           spreadRadius, borderRadius,
                           wr::BoxShadowClipMode::Outset);
  }

  return true;
}

void nsDisplayBoxShadowOuter::ComputeInvalidationRegion(
    nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
    nsRegion* aInvalidRegion) const {
  const auto* geometry =
      static_cast<const nsDisplayItemGenericGeometry*>(aGeometry);
  bool snap;
  if (!geometry->mBounds.IsEqualInterior(GetBounds(aBuilder, &snap)) ||
      !geometry->mBorderRect.IsEqualInterior(GetBorderRect())) {
    nsRegion oldShadow, newShadow;
    nscoord dontCare[8];
    bool hasBorderRadius = mFrame->GetBorderRadii(dontCare);
    if (hasBorderRadius) {
      // If we have rounded corners then we need to invalidate the frame area
      // too since we paint into it.
      oldShadow = geometry->mBounds;
      newShadow = GetBounds(aBuilder, &snap);
    } else {
      oldShadow.Sub(geometry->mBounds, geometry->mBorderRect);
      newShadow.Sub(GetBounds(aBuilder, &snap), GetBorderRect());
    }
    aInvalidRegion->Or(oldShadow, newShadow);
  }
}

void nsDisplayBoxShadowInner::Paint(nsDisplayListBuilder* aBuilder,
                                    gfxContext* aCtx) {
  nsPoint offset = ToReferenceFrame();
  nsRect borderRect = nsRect(offset, mFrame->GetSize());
  nsPresContext* presContext = mFrame->PresContext();

  AUTO_PROFILER_LABEL("nsDisplayBoxShadowInner::Paint", GRAPHICS);

  nsCSSRendering::PaintBoxShadowInner(presContext, *aCtx, mFrame, borderRect);
}

bool nsDisplayBoxShadowInner::CanCreateWebRenderCommands(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
    const nsPoint& aReferenceOffset) {
  auto shadows = aFrame->StyleEffects()->mBoxShadow.AsSpan();
  if (shadows.IsEmpty()) {
    // Means we don't have to paint anything
    return true;
  }

  bool hasBorderRadius;
  bool nativeTheme =
      nsCSSRendering::HasBoxShadowNativeTheme(aFrame, hasBorderRadius);

  // We don't support native themed things yet like box shadows around
  // input buttons.
  return !nativeTheme;
}

/* static */
void nsDisplayBoxShadowInner::CreateInsetBoxShadowWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, const StackingContextHelper& aSc,
    nsRect& aVisibleRect, nsIFrame* aFrame, const nsRect& aBorderRect) {
  if (!nsCSSRendering::ShouldPaintBoxShadowInner(aFrame)) {
    return;
  }

  int32_t appUnitsPerDevPixel = aFrame->PresContext()->AppUnitsPerDevPixel();

  auto shadows = aFrame->StyleEffects()->mBoxShadow.AsSpan();

  LayoutDeviceRect clipRect =
      LayoutDeviceRect::FromAppUnits(aVisibleRect, appUnitsPerDevPixel);

  for (const auto& shadow : Reversed(shadows)) {
    if (!shadow.inset) {
      continue;
    }

    nsRect shadowRect =
        nsCSSRendering::GetBoxShadowInnerPaddingRect(aFrame, aBorderRect);
    RectCornerRadii innerRadii;
    nsCSSRendering::GetShadowInnerRadii(aFrame, aBorderRect, innerRadii);

    // Now translate everything to device pixels.
    LayoutDeviceRect deviceBoxRect =
        LayoutDeviceRect::FromAppUnits(shadowRect, appUnitsPerDevPixel);
    wr::LayoutRect deviceClipRect = wr::ToLayoutRect(clipRect);
    sRGBColor shadowColor =
        nsCSSRendering::GetShadowColor(shadow.base, aFrame, 1.0);

    LayoutDevicePoint shadowOffset = LayoutDevicePoint::FromAppUnits(
        nsPoint(shadow.base.horizontal.ToAppUnits(),
                shadow.base.vertical.ToAppUnits()),
        appUnitsPerDevPixel);

    float blurRadius =
        float(shadow.base.blur.ToAppUnits()) / float(appUnitsPerDevPixel);

    wr::BorderRadius borderRadius = wr::ToBorderRadius(
        LayoutDeviceSize::FromUnknownSize(innerRadii.TopLeft()),
        LayoutDeviceSize::FromUnknownSize(innerRadii.TopRight()),
        LayoutDeviceSize::FromUnknownSize(innerRadii.BottomLeft()),
        LayoutDeviceSize::FromUnknownSize(innerRadii.BottomRight()));
    // NOTE: Any spread radius > 0 will render nothing. WR Bug.
    float spreadRadius =
        float(shadow.spread.ToAppUnits()) / float(appUnitsPerDevPixel);

    aBuilder.PushBoxShadow(
        wr::ToLayoutRect(deviceBoxRect), deviceClipRect,
        !aFrame->BackfaceIsHidden(), wr::ToLayoutRect(deviceBoxRect),
        wr::ToLayoutVector2D(shadowOffset),
        wr::ToColorF(ToDeviceColor(shadowColor)), blurRadius, spreadRadius,
        borderRadius, wr::BoxShadowClipMode::Inset);
  }
}

bool nsDisplayBoxShadowInner::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  if (!CanCreateWebRenderCommands(aDisplayListBuilder, mFrame,
                                  ToReferenceFrame())) {
    return false;
  }

  bool snap;
  nsRect visible = GetBounds(aDisplayListBuilder, &snap);
  nsPoint offset = ToReferenceFrame();
  nsRect borderRect = nsRect(offset, mFrame->GetSize());
  nsDisplayBoxShadowInner::CreateInsetBoxShadowWebRenderCommands(
      aBuilder, aSc, visible, mFrame, borderRect);

  return true;
}

nsDisplayWrapList::nsDisplayWrapList(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame, nsDisplayList* aList)
    : nsDisplayWrapList(aBuilder, aFrame, aList,
                        aBuilder->CurrentActiveScrolledRoot(), false) {}

nsDisplayWrapList::nsDisplayWrapList(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    const ActiveScrolledRoot* aActiveScrolledRoot, bool aClearClipChain)
    : nsPaintedDisplayItem(aBuilder, aFrame, aActiveScrolledRoot),
      mList(aBuilder),
      mFrameActiveScrolledRoot(aBuilder->CurrentActiveScrolledRoot()),
      mOverrideZIndex(0),
      mHasZIndexOverride(false),
      mClearingClipChain(aClearClipChain) {
  MOZ_COUNT_CTOR(nsDisplayWrapList);

  mBaseBuildingRect = GetBuildingRect();

  mListPtr = &mList;
  mListPtr->AppendToTop(aList);
  mOriginalClipChain = mClipChain;
  nsDisplayWrapList::UpdateBounds(aBuilder);
}

nsDisplayWrapList::nsDisplayWrapList(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame, nsDisplayItem* aItem)
    : nsPaintedDisplayItem(aBuilder, aFrame,
                           aBuilder->CurrentActiveScrolledRoot()),
      mList(aBuilder),
      mOverrideZIndex(0),
      mHasZIndexOverride(false) {
  MOZ_COUNT_CTOR(nsDisplayWrapList);

  mBaseBuildingRect = GetBuildingRect();

  mListPtr = &mList;
  mListPtr->AppendToTop(aItem);
  mOriginalClipChain = mClipChain;
  nsDisplayWrapList::UpdateBounds(aBuilder);

  if (!aFrame || !aFrame->IsTransformed()) {
    return;
  }

  // See the previous nsDisplayWrapList constructor
  if (aItem->Frame() == aFrame) {
    mToReferenceFrame = aItem->ToReferenceFrame();
  }

  nsRect visible = aBuilder->GetVisibleRect() +
                   aBuilder->GetCurrentFrameOffsetToReferenceFrame();

  SetBuildingRect(visible);
}

nsDisplayWrapList::~nsDisplayWrapList() { MOZ_COUNT_DTOR(nsDisplayWrapList); }

void nsDisplayWrapList::HitTest(nsDisplayListBuilder* aBuilder,
                                const nsRect& aRect, HitTestState* aState,
                                nsTArray<nsIFrame*>* aOutFrames) {
  mListPtr->HitTest(aBuilder, aRect, aState, aOutFrames);
}

nsRect nsDisplayWrapList::GetBounds(nsDisplayListBuilder* aBuilder,
                                    bool* aSnap) const {
  *aSnap = false;
  return mBounds;
}

nsRegion nsDisplayWrapList::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                            bool* aSnap) const {
  *aSnap = false;
  bool snap;
  return ::mozilla::GetOpaqueRegion(aBuilder, GetChildren(),
                                    GetBounds(aBuilder, &snap));
}

Maybe<nscolor> nsDisplayWrapList::IsUniform(
    nsDisplayListBuilder* aBuilder) const {
  // We could try to do something but let's conservatively just return Nothing.
  return Nothing();
}

void nsDisplayWrapper::Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) {
  NS_ERROR("nsDisplayWrapper should have been flattened away for painting");
}

nsRect nsDisplayWrapList::GetComponentAlphaBounds(
    nsDisplayListBuilder* aBuilder) const {
  return mListPtr->GetComponentAlphaBounds(aBuilder);
}

bool nsDisplayWrapList::CreateWebRenderCommandsNewClipListOption(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder, bool aNewClipList) {
  aManager->CommandBuilder().CreateWebRenderCommandsFromDisplayList(
      GetChildren(), this, aDisplayListBuilder, aSc, aBuilder, aResources,
      aNewClipList);
  return true;
}

static nsresult WrapDisplayList(nsDisplayListBuilder* aBuilder,
                                nsIFrame* aFrame, nsDisplayList* aList,
                                nsDisplayItemWrapper* aWrapper) {
  if (!aList->GetTop()) {
    return NS_OK;
  }
  nsDisplayItem* item = aWrapper->WrapList(aBuilder, aFrame, aList);
  if (!item) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  // aList was emptied
  aList->AppendToTop(item);
  return NS_OK;
}

static nsresult WrapEachDisplayItem(nsDisplayListBuilder* aBuilder,
                                    nsDisplayList* aList,
                                    nsDisplayItemWrapper* aWrapper) {
  for (nsDisplayItem* item : aList->TakeItems()) {
    item = aWrapper->WrapItem(aBuilder, item);
    if (!item) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    aList->AppendToTop(item);
  }
  // aList was emptied
  return NS_OK;
}

nsresult nsDisplayItemWrapper::WrapLists(nsDisplayListBuilder* aBuilder,
                                         nsIFrame* aFrame,
                                         const nsDisplayListSet& aIn,
                                         const nsDisplayListSet& aOut) {
  nsresult rv = WrapListsInPlace(aBuilder, aFrame, aIn);
  NS_ENSURE_SUCCESS(rv, rv);

  if (&aOut == &aIn) {
    return NS_OK;
  }
  aOut.BorderBackground()->AppendToTop(aIn.BorderBackground());
  aOut.BlockBorderBackgrounds()->AppendToTop(aIn.BlockBorderBackgrounds());
  aOut.Floats()->AppendToTop(aIn.Floats());
  aOut.Content()->AppendToTop(aIn.Content());
  aOut.PositionedDescendants()->AppendToTop(aIn.PositionedDescendants());
  aOut.Outlines()->AppendToTop(aIn.Outlines());
  return NS_OK;
}

nsresult nsDisplayItemWrapper::WrapListsInPlace(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
    const nsDisplayListSet& aLists) {
  nsresult rv;
  if (WrapBorderBackground()) {
    // Our border-backgrounds are in-flow
    rv = WrapDisplayList(aBuilder, aFrame, aLists.BorderBackground(), this);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  // Our block border-backgrounds are in-flow
  rv = WrapDisplayList(aBuilder, aFrame, aLists.BlockBorderBackgrounds(), this);
  NS_ENSURE_SUCCESS(rv, rv);
  // The floats are not in flow
  rv = WrapEachDisplayItem(aBuilder, aLists.Floats(), this);
  NS_ENSURE_SUCCESS(rv, rv);
  // Our child content is in flow
  rv = WrapDisplayList(aBuilder, aFrame, aLists.Content(), this);
  NS_ENSURE_SUCCESS(rv, rv);
  // The positioned descendants may not be in-flow
  rv = WrapEachDisplayItem(aBuilder, aLists.PositionedDescendants(), this);
  NS_ENSURE_SUCCESS(rv, rv);
  // The outlines may not be in-flow
  return WrapEachDisplayItem(aBuilder, aLists.Outlines(), this);
}

nsDisplayOpacity::nsDisplayOpacity(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    const ActiveScrolledRoot* aActiveScrolledRoot, bool aForEventsOnly,
    bool aNeedsActiveLayer, bool aWrapsBackdropFilter)
    : nsDisplayWrapList(aBuilder, aFrame, aList, aActiveScrolledRoot, true),
      mOpacity(aFrame->StyleEffects()->mOpacity),
      mForEventsOnly(aForEventsOnly),
      mNeedsActiveLayer(aNeedsActiveLayer),
      mChildOpacityState(ChildOpacityState::Unknown),
      mWrapsBackdropFilter(aWrapsBackdropFilter) {
  MOZ_COUNT_CTOR(nsDisplayOpacity);
}

void nsDisplayOpacity::HitTest(nsDisplayListBuilder* aBuilder,
                               const nsRect& aRect,
                               nsDisplayItem::HitTestState* aState,
                               nsTArray<nsIFrame*>* aOutFrames) {
  AutoRestore<float> opacity(aState->mCurrentOpacity);
  aState->mCurrentOpacity *= mOpacity;

  // TODO(emilio): special-casing zero is a bit arbitrary... Maybe we should
  // only consider fully opaque items? Or make this configurable somehow?
  if (aBuilder->HitTestIsForVisibility() && mOpacity == 0.0f) {
    return;
  }
  nsDisplayWrapList::HitTest(aBuilder, aRect, aState, aOutFrames);
}

nsRegion nsDisplayOpacity::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                           bool* aSnap) const {
  *aSnap = false;
  // The only time where mOpacity == 1.0 should be when we have will-change.
  // We could report this as opaque then but when the will-change value starts
  // animating the element would become non opaque and could cause repaints.
  return nsRegion();
}

void nsDisplayOpacity::Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) {
  if (GetOpacity() == 0.0f) {
    return;
  }

  if (GetOpacity() == 1.0f) {
    GetChildren()->Paint(aBuilder, aCtx,
                         mFrame->PresContext()->AppUnitsPerDevPixel());
    return;
  }

  // TODO: Compute a bounds rect to pass to PushLayer for a smaller
  // allocation.
  aCtx->GetDrawTarget()->PushLayer(false, GetOpacity(), nullptr, gfx::Matrix());
  GetChildren()->Paint(aBuilder, aCtx,
                       mFrame->PresContext()->AppUnitsPerDevPixel());
  aCtx->GetDrawTarget()->PopLayer();
}

/* static */
bool nsDisplayOpacity::NeedsActiveLayer(nsDisplayListBuilder* aBuilder,
                                        nsIFrame* aFrame) {
  return EffectCompositor::HasAnimationsForCompositor(
             aFrame, DisplayItemType::TYPE_OPACITY) ||
         (ActiveLayerTracker::IsStyleAnimated(
             aBuilder, aFrame, nsCSSPropertyIDSet::OpacityProperties()));
}

bool nsDisplayOpacity::CanApplyOpacity(WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aBuilder) const {
  return !EffectCompositor::HasAnimationsForCompositor(
      mFrame, DisplayItemType::TYPE_OPACITY);
}

// Only try folding our opacity down if we have at most |kOpacityMaxChildCount|
// children that don't overlap and can all apply the opacity to themselves.
static const size_t kOpacityMaxChildCount = 3;

// |kOpacityMaxListSize| defines an early exit condition for opacity items that
// are likely have more child items than |kOpacityMaxChildCount|.
static const size_t kOpacityMaxListSize = kOpacityMaxChildCount * 2;

/**
 * Recursively iterates through |aList| and collects at most
 * |kOpacityMaxChildCount| display item pointers to items that return true for
 * CanApplyOpacity(). The item pointers are added to |aArray|.
 *
 * LayerEventRegions and WrapList items are ignored.
 *
 * We need to do this recursively, because the child display items might contain
 * nested nsDisplayWrapLists.
 *
 * Returns false if there are more than |kOpacityMaxChildCount| items, or if an
 * item that returns false for CanApplyOpacity() is encountered.
 * Otherwise returns true.
 */
static bool CollectItemsWithOpacity(WebRenderLayerManager* aManager,
                                    nsDisplayListBuilder* aBuilder,
                                    nsDisplayList* aList,
                                    nsTArray<nsPaintedDisplayItem*>& aArray) {
  if (aList->Length() > kOpacityMaxListSize) {
    // Exit early, since |aList| will likely contain more than
    // |kOpacityMaxChildCount| items.
    return false;
  }

  for (nsDisplayItem* i : *aList) {
    const DisplayItemType type = i->GetType();

    if (type == DisplayItemType::TYPE_COMPOSITOR_HITTEST_INFO) {
      continue;
    }

    // Descend only into wraplists.
    if (type == DisplayItemType::TYPE_WRAP_LIST ||
        type == DisplayItemType::TYPE_CONTAINER) {
      // The current display item has children, process them first.
      if (!CollectItemsWithOpacity(aManager, aBuilder, i->GetChildren(),
                                   aArray)) {
        return false;
      }

      continue;
    }

    if (aArray.Length() == kOpacityMaxChildCount) {
      return false;
    }

    auto* item = i->AsPaintedDisplayItem();
    if (!item || !item->CanApplyOpacity(aManager, aBuilder)) {
      return false;
    }

    aArray.AppendElement(item);
  }

  return true;
}

bool nsDisplayOpacity::CanApplyToChildren(WebRenderLayerManager* aManager,
                                          nsDisplayListBuilder* aBuilder) {
  if (mChildOpacityState == ChildOpacityState::Deferred) {
    return false;
  }

  // Iterate through the child display list and copy at most
  // |kOpacityMaxChildCount| child display item pointers to a temporary list.
  AutoTArray<nsPaintedDisplayItem*, kOpacityMaxChildCount> items;
  if (!CollectItemsWithOpacity(aManager, aBuilder, &mList, items)) {
    mChildOpacityState = ChildOpacityState::Deferred;
    return false;
  }

  struct {
    nsPaintedDisplayItem* item{};
    nsRect bounds;
  } children[kOpacityMaxChildCount];

  bool snap;
  size_t childCount = 0;
  for (nsPaintedDisplayItem* item : items) {
    children[childCount].item = item;
    children[childCount].bounds = item->GetBounds(aBuilder, &snap);
    childCount++;
  }

  for (size_t i = 0; i < childCount; i++) {
    for (size_t j = i + 1; j < childCount; j++) {
      if (children[i].bounds.Intersects(children[j].bounds)) {
        mChildOpacityState = ChildOpacityState::Deferred;
        return false;
      }
    }
  }

  mChildOpacityState = ChildOpacityState::Applied;
  return true;
}

/**
 * Returns true if this nsDisplayOpacity contains only a filter or a mask item
 * that has the same frame as the opacity item, and that supports painting with
 * opacity. In this case the opacity item can be optimized away.
 */
bool nsDisplayOpacity::ApplyToMask() {
  if (mList.Length() != 1) {
    return false;
  }

  nsDisplayItem* item = mList.GetBottom();
  if (item->Frame() != mFrame) {
    // The effect item needs to have the same frame as the opacity item.
    return false;
  }

  const DisplayItemType type = item->GetType();
  if (type == DisplayItemType::TYPE_MASK) {
    return true;
  }

  return false;
}

bool nsDisplayOpacity::CanApplyOpacityToChildren(
    WebRenderLayerManager* aManager, nsDisplayListBuilder* aBuilder,
    float aInheritedOpacity) {
  if (mFrame->GetPrevContinuation() || mFrame->GetNextContinuation() ||
      mFrame->HasAnyStateBits(NS_FRAME_PART_OF_IBSPLIT)) {
    // If we've been split, then we might need to merge, so
    // don't flatten us away.
    return false;
  }

  if (mNeedsActiveLayer || mOpacity == 0.0) {
    // If our opacity is zero then we'll discard all descendant display items
    // except for layer event regions, so there's no point in doing this
    // optimization (and if we do do it, then invalidations of those descendants
    // might trigger repainting).
    return false;
  }

  if (mList.IsEmpty()) {
    return false;
  }

  // We can only flatten opacity items into a mask if we haven't
  // already flattened an earlier ancestor, since the SVG code pulls the opacity
  // from style directly, and won't know about the outer opacity value.
  if (aInheritedOpacity == 1.0f && ApplyToMask()) {
    MOZ_ASSERT(SVGIntegrationUtils::UsingEffectsForFrame(mFrame));
    mChildOpacityState = ChildOpacityState::Applied;
    return true;
  }

  // Return true if we successfully applied opacity to child items.
  return CanApplyToChildren(aManager, aBuilder);
}

void nsDisplayOpacity::ComputeInvalidationRegion(
    nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
    nsRegion* aInvalidRegion) const {
  const auto* geometry =
      static_cast<const nsDisplayOpacityGeometry*>(aGeometry);

  bool snap;
  if (mOpacity != geometry->mOpacity) {
    aInvalidRegion->Or(GetBounds(aBuilder, &snap), geometry->mBounds);
  }
}

void nsDisplayOpacity::WriteDebugInfo(std::stringstream& aStream) {
  aStream << " (opacity " << mOpacity << ", mChildOpacityState: ";
  switch (mChildOpacityState) {
    case ChildOpacityState::Unknown:
      aStream << "Unknown";
      break;
    case ChildOpacityState::Applied:
      aStream << "Applied";
      break;
    case ChildOpacityState::Deferred:
      aStream << "Deferred";
      break;
    default:
      break;
  }

  aStream << ")";
}

bool nsDisplayOpacity::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  MOZ_ASSERT(mChildOpacityState != ChildOpacityState::Applied);
  float oldOpacity = aBuilder.GetInheritedOpacity();
  const DisplayItemClipChain* oldClipChain = aBuilder.GetInheritedClipChain();
  aBuilder.SetInheritedOpacity(1.0f);
  aBuilder.SetInheritedClipChain(nullptr);
  float opacity = mOpacity * oldOpacity;
  float* opacityForSC = &opacity;

  uint64_t animationsId =
      AddAnimationsForWebRender(this, aManager, aDisplayListBuilder);
  wr::WrAnimationProperty prop{
      wr::WrAnimationType::Opacity,
      animationsId,
  };

  wr::StackingContextParams params;
  params.animation = animationsId ? &prop : nullptr;
  params.opacity = opacityForSC;
  params.clip =
      wr::WrStackingContextClip::ClipChain(aBuilder.CurrentClipChainId());
  if (mWrapsBackdropFilter) {
    params.flags |= wr::StackingContextFlags::WRAPS_BACKDROP_FILTER;
  }
  StackingContextHelper sc(aSc, GetActiveScrolledRoot(), mFrame, this, aBuilder,
                           params);

  aManager->CommandBuilder().CreateWebRenderCommandsFromDisplayList(
      &mList, this, aDisplayListBuilder, sc, aBuilder, aResources);
  aBuilder.SetInheritedOpacity(oldOpacity);
  aBuilder.SetInheritedClipChain(oldClipChain);
  return true;
}

nsDisplayBlendMode::nsDisplayBlendMode(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    StyleBlend aBlendMode, const ActiveScrolledRoot* aActiveScrolledRoot,
    const bool aIsForBackground)
    : nsDisplayWrapList(aBuilder, aFrame, aList, aActiveScrolledRoot, true),
      mBlendMode(aBlendMode),
      mIsForBackground(aIsForBackground) {
  MOZ_COUNT_CTOR(nsDisplayBlendMode);
}

nsRegion nsDisplayBlendMode::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                             bool* aSnap) const {
  *aSnap = false;
  // We are never considered opaque
  return nsRegion();
}

bool nsDisplayBlendMode::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  wr::StackingContextParams params;
  params.mix_blend_mode =
      wr::ToMixBlendMode(nsCSSRendering::GetGFXBlendMode(mBlendMode));
  params.clip =
      wr::WrStackingContextClip::ClipChain(aBuilder.CurrentClipChainId());
  StackingContextHelper sc(aSc, GetActiveScrolledRoot(), mFrame, this, aBuilder,
                           params);

  return nsDisplayWrapList::CreateWebRenderCommands(
      aBuilder, aResources, sc, aManager, aDisplayListBuilder);
}

void nsDisplayBlendMode::Paint(nsDisplayListBuilder* aBuilder,
                               gfxContext* aCtx) {
  // This should be switched to use PushLayerWithBlend, once it's
  // been implemented for all DrawTarget backends.
  DrawTarget* dt = aCtx->GetDrawTarget();
  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  Rect rect = NSRectToRect(GetPaintRect(aBuilder, aCtx), appUnitsPerDevPixel);
  rect.RoundOut();

  // Create a temporary DrawTarget that is clipped to the area that
  // we're going to draw to. This will include the same transform as
  // is currently on |dt|.
  RefPtr<DrawTarget> temp =
      dt->CreateClippedDrawTarget(rect, SurfaceFormat::B8G8R8A8);
  if (!temp) {
    return;
  }

  gfxContext ctx(temp, /* aPreserveTransform */ true);

  GetChildren()->Paint(aBuilder, &ctx,
                       mFrame->PresContext()->AppUnitsPerDevPixel());

  // Draw the temporary DT to the real destination, applying the blend mode, but
  // no transform.
  temp->Flush();
  RefPtr<SourceSurface> surface = temp->Snapshot();
  gfxContextMatrixAutoSaveRestore saveMatrix(aCtx);
  dt->SetTransform(Matrix());
  dt->DrawSurface(
      surface, Rect(surface->GetRect()), Rect(surface->GetRect()),
      DrawSurfaceOptions(),
      DrawOptions(1.0f, nsCSSRendering::GetGFXBlendMode(mBlendMode)));
}

gfx::CompositionOp nsDisplayBlendMode::BlendMode() {
  return nsCSSRendering::GetGFXBlendMode(mBlendMode);
}

bool nsDisplayBlendMode::CanMerge(const nsDisplayItem* aItem) const {
  // Items for the same content element should be merged into a single
  // compositing group.
  if (!HasDifferentFrame(aItem) || !HasSameTypeAndClip(aItem) ||
      !HasSameContent(aItem)) {
    return false;
  }

  const auto* item = static_cast<const nsDisplayBlendMode*>(aItem);
  if (mIsForBackground || item->mIsForBackground) {
    // Don't merge background-blend-mode items
    return false;
  }

  return true;
}

/* static */
nsDisplayBlendContainer* nsDisplayBlendContainer::CreateForMixBlendMode(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    const ActiveScrolledRoot* aActiveScrolledRoot) {
  return MakeDisplayItem<nsDisplayBlendContainer>(aBuilder, aFrame, aList,
                                                  aActiveScrolledRoot, false);
}

/* static */
nsDisplayBlendContainer* nsDisplayBlendContainer::CreateForBackgroundBlendMode(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsIFrame* aSecondaryFrame,
    nsDisplayList* aList, const ActiveScrolledRoot* aActiveScrolledRoot) {
  if (aSecondaryFrame) {
    auto type = GetTableTypeFromFrame(aFrame);
    auto index = static_cast<uint16_t>(type);

    return MakeDisplayItemWithIndex<nsDisplayTableBlendContainer>(
        aBuilder, aSecondaryFrame, index, aList, aActiveScrolledRoot, true,
        aFrame);
  }

  return MakeDisplayItemWithIndex<nsDisplayBlendContainer>(
      aBuilder, aFrame, 1, aList, aActiveScrolledRoot, true);
}

nsDisplayBlendContainer::nsDisplayBlendContainer(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    const ActiveScrolledRoot* aActiveScrolledRoot, bool aIsForBackground)
    : nsDisplayWrapList(aBuilder, aFrame, aList, aActiveScrolledRoot, true),
      mIsForBackground(aIsForBackground) {
  MOZ_COUNT_CTOR(nsDisplayBlendContainer);
}

void nsDisplayBlendContainer::Paint(nsDisplayListBuilder* aBuilder,
                                    gfxContext* aCtx) {
  aCtx->GetDrawTarget()->PushLayer(false, 1.0, nullptr, gfx::Matrix());
  GetChildren()->Paint(aBuilder, aCtx,
                       mFrame->PresContext()->AppUnitsPerDevPixel());
  aCtx->GetDrawTarget()->PopLayer();
}

bool nsDisplayBlendContainer::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  wr::StackingContextParams params;
  params.flags |= wr::StackingContextFlags::IS_BLEND_CONTAINER;
  params.clip =
      wr::WrStackingContextClip::ClipChain(aBuilder.CurrentClipChainId());
  StackingContextHelper sc(aSc, GetActiveScrolledRoot(), mFrame, this, aBuilder,
                           params);

  return nsDisplayWrapList::CreateWebRenderCommands(
      aBuilder, aResources, sc, aManager, aDisplayListBuilder);
}

nsDisplayOwnLayer::nsDisplayOwnLayer(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    const ActiveScrolledRoot* aActiveScrolledRoot,
    nsDisplayOwnLayerFlags aFlags, const ScrollbarData& aScrollbarData,
    bool aForceActive, bool aClearClipChain)
    : nsDisplayWrapList(aBuilder, aFrame, aList, aActiveScrolledRoot,
                        aClearClipChain),
      mFlags(aFlags),
      mScrollbarData(aScrollbarData),
      mForceActive(aForceActive),
      mWrAnimationId(0) {
  MOZ_COUNT_CTOR(nsDisplayOwnLayer);
}

bool nsDisplayOwnLayer::IsScrollThumbLayer() const {
  return mScrollbarData.mScrollbarLayerType == ScrollbarLayerType::Thumb;
}

bool nsDisplayOwnLayer::IsScrollbarContainer() const {
  return mScrollbarData.mScrollbarLayerType == ScrollbarLayerType::Container;
}

bool nsDisplayOwnLayer::IsRootScrollbarContainer() const {
  return IsScrollbarContainer() && IsScrollbarLayerForRoot();
}

bool nsDisplayOwnLayer::IsScrollbarLayerForRoot() const {
  return mFrame->PresContext()->IsRootContentDocumentCrossProcess() &&
         mScrollbarData.mTargetViewId ==
             nsLayoutUtils::ScrollIdForRootScrollFrame(mFrame->PresContext());
}

bool nsDisplayOwnLayer::IsZoomingLayer() const {
  return GetType() == DisplayItemType::TYPE_ASYNC_ZOOM;
}

bool nsDisplayOwnLayer::IsFixedPositionLayer() const {
  return GetType() == DisplayItemType::TYPE_FIXED_POSITION ||
         GetType() == DisplayItemType::TYPE_TABLE_FIXED_POSITION;
}

bool nsDisplayOwnLayer::IsStickyPositionLayer() const {
  return GetType() == DisplayItemType::TYPE_STICKY_POSITION;
}

bool nsDisplayOwnLayer::HasDynamicToolbar() const {
  if (!mFrame->PresContext()->IsRootContentDocumentCrossProcess()) {
    return false;
  }
  return mFrame->PresContext()->HasDynamicToolbar() ||
         // For tests on Android, this pref is set to simulate the dynamic
         // toolbar
         StaticPrefs::apz_fixed_margin_override_enabled();
}

bool nsDisplayOwnLayer::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  Maybe<wr::WrAnimationProperty> prop;
  bool needsProp = aManager->LayerManager()->AsyncPanZoomEnabled() &&
                   (IsScrollThumbLayer() || IsZoomingLayer() ||
                    ShouldGetFixedAnimationId() ||
                    (IsRootScrollbarContainer() && HasDynamicToolbar()));

  if (needsProp) {
    // APZ is enabled and this is a scroll thumb or zooming layer, so we need
    // to create and set an animation id. That way APZ can adjust the position/
    // zoom of this content asynchronously as needed.
    RefPtr<WebRenderAPZAnimationData> animationData =
        aManager->CommandBuilder()
            .CreateOrRecycleWebRenderUserData<WebRenderAPZAnimationData>(this);
    mWrAnimationId = animationData->GetAnimationId();

    prop.emplace();
    prop->id = mWrAnimationId;
    prop->key = wr::SpatialKey(uint64_t(mFrame), GetPerFrameKey(),
                               wr::SpatialKeyKind::APZ);
    prop->effect_type = wr::WrAnimationType::Transform;
  }

  wr::StackingContextParams params;
  params.animation = prop.ptrOr(nullptr);
  params.clip =
      wr::WrStackingContextClip::ClipChain(aBuilder.CurrentClipChainId());
  if (IsScrollbarContainer() && IsRootScrollbarContainer()) {
    params.prim_flags |= wr::PrimitiveFlags::IS_SCROLLBAR_CONTAINER;
  }
  if (IsZoomingLayer() ||
      (ShouldGetFixedAnimationId() ||
       (IsRootScrollbarContainer() && HasDynamicToolbar()))) {
    params.is_2d_scale_translation = true;
    params.should_snap = true;
  }

  StackingContextHelper sc(aSc, GetActiveScrolledRoot(), mFrame, this, aBuilder,
                           params);

  nsDisplayWrapList::CreateWebRenderCommands(aBuilder, aResources, sc, aManager,
                                             aDisplayListBuilder);
  return true;
}

bool nsDisplayOwnLayer::UpdateScrollData(WebRenderScrollData* aData,
                                         WebRenderLayerScrollData* aLayerData) {
  bool isRelevantToApz = (IsScrollThumbLayer() || IsScrollbarContainer() ||
                          IsZoomingLayer() || ShouldGetFixedAnimationId());

  if (!isRelevantToApz) {
    return false;
  }

  if (!aLayerData) {
    return true;
  }

  if (IsZoomingLayer()) {
    aLayerData->SetZoomAnimationId(mWrAnimationId);
    return true;
  }

  if (IsFixedPositionLayer() && ShouldGetFixedAnimationId()) {
    aLayerData->SetFixedPositionAnimationId(mWrAnimationId);
    return true;
  }

  MOZ_ASSERT(IsScrollbarContainer() || IsScrollThumbLayer());

  aLayerData->SetScrollbarData(mScrollbarData);

  if (IsRootScrollbarContainer() && HasDynamicToolbar()) {
    aLayerData->SetScrollbarAnimationId(mWrAnimationId);
    return true;
  }

  if (IsScrollThumbLayer()) {
    aLayerData->SetScrollbarAnimationId(mWrAnimationId);
    LayoutDeviceRect bounds = LayoutDeviceIntRect::FromAppUnits(
        mBounds, mFrame->PresContext()->AppUnitsPerDevPixel());
    // Subframe scrollbars are subject to the pinch-zoom scale,
    // but root scrollbars are not because they are outside of the
    // region that is zoomed.
    const float resolution =
        IsScrollbarLayerForRoot()
            ? 1.0f
            : mFrame->PresShell()->GetCumulativeResolution();
    LayerIntRect layerBounds =
        RoundedOut(bounds * LayoutDeviceToLayerScale(resolution));
    aLayerData->SetVisibleRect(layerBounds);
  }
  return true;
}

void nsDisplayOwnLayer::WriteDebugInfo(std::stringstream& aStream) {
  aStream << nsPrintfCString(" (flags 0x%x) (scrolltarget %" PRIu64 ")",
                             (int)mFlags, mScrollbarData.mTargetViewId)
                 .get();
}

nsDisplaySubDocument::nsDisplaySubDocument(nsDisplayListBuilder* aBuilder,
                                           nsIFrame* aFrame,
                                           nsSubDocumentFrame* aSubDocFrame,
                                           nsDisplayList* aList,
                                           nsDisplayOwnLayerFlags aFlags)
    : nsDisplayOwnLayer(aBuilder, aFrame, aList,
                        aBuilder->CurrentActiveScrolledRoot(), aFlags),
      mScrollParentId(aBuilder->GetCurrentScrollParentId()),
      mShouldFlatten(false),
      mSubDocFrame(aSubDocFrame) {
  MOZ_COUNT_CTOR(nsDisplaySubDocument);

  if (mSubDocFrame && mSubDocFrame != mFrame) {
    mSubDocFrame->AddDisplayItem(this);
  }
}

nsDisplaySubDocument::~nsDisplaySubDocument() {
  MOZ_COUNT_DTOR(nsDisplaySubDocument);
  if (mSubDocFrame) {
    mSubDocFrame->RemoveDisplayItem(this);
  }
}

nsIFrame* nsDisplaySubDocument::FrameForInvalidation() const {
  return mSubDocFrame ? mSubDocFrame : mFrame;
}

void nsDisplaySubDocument::RemoveFrame(nsIFrame* aFrame) {
  if (aFrame == mSubDocFrame) {
    mSubDocFrame = nullptr;
    SetDeletedFrame();
  }
  nsDisplayOwnLayer::RemoveFrame(aFrame);
}

static bool UseDisplayPortForViewport(nsDisplayListBuilder* aBuilder,
                                      nsIFrame* aFrame) {
  return aBuilder->IsPaintingToWindow() &&
         DisplayPortUtils::ViewportHasDisplayPort(aFrame->PresContext());
}

nsRect nsDisplaySubDocument::GetBounds(nsDisplayListBuilder* aBuilder,
                                       bool* aSnap) const {
  bool usingDisplayPort = UseDisplayPortForViewport(aBuilder, mFrame);

  if ((mFlags & nsDisplayOwnLayerFlags::GenerateScrollableLayer) &&
      usingDisplayPort) {
    *aSnap = false;
    return mFrame->GetRect() + aBuilder->ToReferenceFrame(mFrame);
  }

  return nsDisplayOwnLayer::GetBounds(aBuilder, aSnap);
}

nsRegion nsDisplaySubDocument::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                               bool* aSnap) const {
  bool usingDisplayPort = UseDisplayPortForViewport(aBuilder, mFrame);

  if ((mFlags & nsDisplayOwnLayerFlags::GenerateScrollableLayer) &&
      usingDisplayPort) {
    *aSnap = false;
    return nsRegion();
  }

  return nsDisplayOwnLayer::GetOpaqueRegion(aBuilder, aSnap);
}

/* static */
nsDisplayFixedPosition* nsDisplayFixedPosition::CreateForFixedBackground(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsIFrame* aSecondaryFrame,
    nsDisplayBackgroundImage* aImage, const uint16_t aIndex,
    const ActiveScrolledRoot* aScrollTargetASR) {
  nsDisplayList temp(aBuilder);
  temp.AppendToTop(aImage);

  if (aSecondaryFrame) {
    auto tableType = GetTableTypeFromFrame(aFrame);
    const uint16_t index = CalculateTablePerFrameKey(aIndex + 1, tableType);
    return MakeDisplayItemWithIndex<nsDisplayTableFixedPosition>(
        aBuilder, aSecondaryFrame, index, &temp, aFrame, aScrollTargetASR);
  }

  return MakeDisplayItemWithIndex<nsDisplayFixedPosition>(
      aBuilder, aFrame, aIndex + 1, &temp, aScrollTargetASR);
}

nsDisplayFixedPosition::nsDisplayFixedPosition(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    const ActiveScrolledRoot* aActiveScrolledRoot,
    const ActiveScrolledRoot* aScrollTargetASR)
    : nsDisplayOwnLayer(aBuilder, aFrame, aList, aActiveScrolledRoot),
      mScrollTargetASR(aScrollTargetASR),
      mIsFixedBackground(false) {
  MOZ_COUNT_CTOR(nsDisplayFixedPosition);
}

nsDisplayFixedPosition::nsDisplayFixedPosition(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    const ActiveScrolledRoot* aScrollTargetASR)
    : nsDisplayOwnLayer(aBuilder, aFrame, aList,
                        aBuilder->CurrentActiveScrolledRoot()),
      // For fixed backgrounds, this is the ASR for the nearest scroll frame.
      mScrollTargetASR(aScrollTargetASR),
      mIsFixedBackground(true) {
  MOZ_COUNT_CTOR(nsDisplayFixedPosition);
}

ScrollableLayerGuid::ViewID nsDisplayFixedPosition::GetScrollTargetId() const {
  if (mScrollTargetASR &&
      (mIsFixedBackground || !nsLayoutUtils::IsReallyFixedPos(mFrame))) {
    return mScrollTargetASR->GetViewId();
  }
  return nsLayoutUtils::ScrollIdForRootScrollFrame(mFrame->PresContext());
}

bool nsDisplayFixedPosition::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  SideBits sides = SideBits::eNone;
  if (!mIsFixedBackground) {
    sides = nsLayoutUtils::GetSideBitsForFixedPositionContent(mFrame);
  }

  // We install this RAII scrolltarget tracker so that any
  // nsDisplayCompositorHitTestInfo items inside this fixed-pos item (and that
  // share the same ASR as this item) use the correct scroll target. That way
  // attempts to scroll on those items will scroll the root scroll frame.
  wr::DisplayListBuilder::FixedPosScrollTargetTracker tracker(
      aBuilder, GetActiveScrolledRoot(), GetScrollTargetId(), sides);
  return nsDisplayOwnLayer::CreateWebRenderCommands(
      aBuilder, aResources, aSc, aManager, aDisplayListBuilder);
}

bool nsDisplayFixedPosition::UpdateScrollData(
    WebRenderScrollData* aData, WebRenderLayerScrollData* aLayerData) {
  if (aLayerData) {
    if (!mIsFixedBackground) {
      aLayerData->SetFixedPositionSides(
          nsLayoutUtils::GetSideBitsForFixedPositionContent(mFrame));
    }
    aLayerData->SetFixedPositionScrollContainerId(GetScrollTargetId());
  }
  nsDisplayOwnLayer::UpdateScrollData(aData, aLayerData);
  return true;
}

bool nsDisplayFixedPosition::ShouldGetFixedAnimationId() {
#if defined(MOZ_WIDGET_ANDROID)
  return mFrame->PresContext()->IsRootContentDocumentCrossProcess() &&
         nsLayoutUtils::ScrollIdForRootScrollFrame(mFrame->PresContext()) ==
             GetScrollTargetId();
#else
  return false;
#endif
}

void nsDisplayFixedPosition::WriteDebugInfo(std::stringstream& aStream) {
  aStream << nsPrintfCString(
                 " (containerASR %s) (scrolltarget %" PRIu64 ")",
                 ActiveScrolledRoot::ToString(mScrollTargetASR).get(),
                 GetScrollTargetId())
                 .get();
}

TableType GetTableTypeFromFrame(nsIFrame* aFrame) {
  if (aFrame->IsTableFrame()) {
    return TableType::Table;
  }

  if (aFrame->IsTableColFrame()) {
    return TableType::TableCol;
  }

  if (aFrame->IsTableColGroupFrame()) {
    return TableType::TableColGroup;
  }

  if (aFrame->IsTableRowFrame()) {
    return TableType::TableRow;
  }

  if (aFrame->IsTableRowGroupFrame()) {
    return TableType::TableRowGroup;
  }

  if (aFrame->IsTableCellFrame()) {
    return TableType::TableCell;
  }

  MOZ_ASSERT_UNREACHABLE("Invalid frame.");
  return TableType::Table;
}

nsDisplayTableFixedPosition::nsDisplayTableFixedPosition(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    nsIFrame* aAncestorFrame, const ActiveScrolledRoot* aScrollTargetASR)
    : nsDisplayFixedPosition(aBuilder, aFrame, aList, aScrollTargetASR),
      mAncestorFrame(aAncestorFrame) {
  if (aBuilder->IsRetainingDisplayList()) {
    mAncestorFrame->AddDisplayItem(this);
  }
}

nsDisplayStickyPosition::nsDisplayStickyPosition(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    const ActiveScrolledRoot* aActiveScrolledRoot,
    const ActiveScrolledRoot* aContainerASR, bool aClippedToDisplayPort)
    : nsDisplayOwnLayer(aBuilder, aFrame, aList, aActiveScrolledRoot),
      mContainerASR(aContainerASR),
      mClippedToDisplayPort(aClippedToDisplayPort),
      mShouldFlatten(false),
      mWrStickyAnimationId(0) {
  MOZ_COUNT_CTOR(nsDisplayStickyPosition);
}

// Returns the smallest distance from "0" to the range [min, max] where
// min <= max. Despite the name, the return value is actually a 1-D vector,
// and so may be negative if max < 0.
static nscoord DistanceToRange(nscoord min, nscoord max) {
  MOZ_ASSERT(min <= max);
  if (max < 0) {
    return max;
  }
  if (min > 0) {
    return min;
  }
  MOZ_ASSERT(min <= 0 && max >= 0);
  return 0;
}

// Returns the magnitude of the part of the range [min, max] that is greater
// than zero. The return value is always non-negative.
static nscoord PositivePart(nscoord min, nscoord max) {
  MOZ_ASSERT(min <= max);
  if (min >= 0) {
    return max - min;
  }
  if (max > 0) {
    return max;
  }
  return 0;
}

// Returns the magnitude of the part of the range [min, max] that is less
// than zero. The return value is always non-negative.
static nscoord NegativePart(nscoord min, nscoord max) {
  MOZ_ASSERT(min <= max);
  if (max <= 0) {
    return max - min;
  }
  if (min < 0) {
    return 0 - min;
  }
  return 0;
}

StickyScrollContainer* nsDisplayStickyPosition::GetStickyScrollContainer() {
  StickyScrollContainer* stickyScrollContainer =
      StickyScrollContainer::GetStickyScrollContainerForFrame(mFrame);
  if (stickyScrollContainer) {
    // If there's no ASR for the scrollframe that this sticky item is attached
    // to, then don't create a WR sticky item for it either. Trying to do so
    // will end in sadness because WR will interpret some coordinates as
    // relative to the nearest enclosing scrollframe, which will correspond
    // to the nearest ancestor ASR on the gecko side. That ASR will not be the
    // same as the scrollframe this sticky item is actually supposed to be
    // attached to, thus the sadness.
    // Not sending WR the sticky item is ok, because the enclosing scrollframe
    // will never be asynchronously scrolled. Instead we will always position
    // the sticky items correctly on the gecko side and WR will never need to
    // adjust their position itself.
    MOZ_ASSERT(stickyScrollContainer->ScrollContainer()
                   ->IsMaybeAsynchronouslyScrolled());
    if (!stickyScrollContainer->ScrollContainer()
             ->IsMaybeAsynchronouslyScrolled()) {
      stickyScrollContainer = nullptr;
    }
  }
  return stickyScrollContainer;
}

bool nsDisplayStickyPosition::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  StickyScrollContainer* stickyScrollContainer = GetStickyScrollContainer();

  Maybe<wr::SpaceAndClipChainHelper> saccHelper;

  if (stickyScrollContainer) {
    float auPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();

    bool snap;
    nsRect itemBounds = GetBounds(aDisplayListBuilder, &snap);

    Maybe<float> topMargin;
    Maybe<float> rightMargin;
    Maybe<float> bottomMargin;
    Maybe<float> leftMargin;
    wr::StickyOffsetBounds vBounds = {0.0, 0.0};
    wr::StickyOffsetBounds hBounds = {0.0, 0.0};
    nsPoint appliedOffset;

    nsRectAbsolute outer;
    nsRectAbsolute inner;
    stickyScrollContainer->GetScrollRanges(mFrame, &outer, &inner);

    nsPoint offset =
        stickyScrollContainer->ScrollContainer()->GetOffsetToCrossDoc(Frame()) +
        ToReferenceFrame();

    // Adjust the scrollPort coordinates to be relative to the reference frame,
    // so that it is in the same space as everything else.
    nsRect scrollPort =
        stickyScrollContainer->ScrollContainer()->GetScrollPortRect();
    scrollPort += offset;

    // The following computations make more sense upon understanding the
    // semantics of "inner" and "outer", which is explained in the comment on
    // SetStickyPositionData in Layers.h.

    if (outer.YMost() != inner.YMost()) {
      // Question: How far will itemBounds.y be from the top of the scrollport
      // when we have scrolled from the current scroll position of "0" to
      // reach the range [inner.YMost(), outer.YMost()] where the item gets
      // stuck?
      // Answer: the current distance is "itemBounds.y - scrollPort.y". That
      // needs to be adjusted by the distance to the range, less any other
      // sticky ranges that fall between 0 and the range. If the distance is
      // negative (i.e. inner.YMost() <= outer.YMost() < 0) then we would be
      // scrolling upwards (decreasing scroll offset) to reach that range,
      // which would increase itemBounds.y and make it farther away from the
      // top of the scrollport. So in that case the adjustment is -distance.
      // If the distance is positive (0 < inner.YMost() <= outer.YMost()) then
      // we would be scrolling downwards, itemBounds.y would decrease, and we
      // again need to adjust by -distance. If we are already in the range
      // then no adjustment is needed and distance is 0 so again using
      // -distance works. If the distance is positive, and the item has both
      // top and bottom sticky ranges, then the bottom sticky range may fall
      // (entirely[1] or partly[2]) between the current scroll position.
      // [1]: 0 <= outer.Y() <= inner.Y() < inner.YMost() <= outer.YMost()
      // [2]: outer.Y() < 0 <= inner.Y() < inner.YMost() <= outer.YMost()
      // In these cases, the item doesn't actually move for that part of the
      // distance, so we need to subtract out that bit, which can be computed
      // as the positive portion of the range [outer.Y(), inner.Y()].
      nscoord distance = DistanceToRange(inner.YMost(), outer.YMost());
      if (distance > 0) {
        distance -= PositivePart(outer.Y(), inner.Y());
      }
      topMargin = Some(NSAppUnitsToFloatPixels(
          itemBounds.y - scrollPort.y - distance, auPerDevPixel));
      // Question: What is the maximum positive ("downward") offset that WR
      // will have to apply to this item in order to prevent the item from
      // visually moving?
      // Answer: Since the item is "sticky" in the range [inner.YMost(),
      // outer.YMost()], the maximum offset will be the size of the range, which
      // is outer.YMost() - inner.YMost().
      vBounds.max =
          NSAppUnitsToFloatPixels(outer.YMost() - inner.YMost(), auPerDevPixel);
      // Question: how much of an offset has layout already applied to the item?
      // Answer: if we are
      // (a) inside the sticky range (inner.YMost() < 0 <= outer.YMost()), or
      // (b) past the sticky range (inner.YMost() < outer.YMost() < 0)
      // then layout has already applied some offset to the position of the
      // item. The amount of the adjustment is |0 - inner.YMost()| in case (a)
      // and |outer.YMost() - inner.YMost()| in case (b).
      if (inner.YMost() < 0) {
        appliedOffset.y = std::min(0, outer.YMost()) - inner.YMost();
        MOZ_ASSERT(appliedOffset.y > 0);
      }
    }
    if (outer.Y() != inner.Y()) {
      // Similar logic as in the previous section, but this time we care about
      // the distance from itemBounds.YMost() to scrollPort.YMost().
      nscoord distance = DistanceToRange(outer.Y(), inner.Y());
      if (distance < 0) {
        distance += NegativePart(inner.YMost(), outer.YMost());
      }
      bottomMargin = Some(NSAppUnitsToFloatPixels(
          scrollPort.YMost() - itemBounds.YMost() + distance, auPerDevPixel));
      // And here WR will be moving the item upwards rather than downwards so
      // again things are inverted from the previous block.
      vBounds.min =
          NSAppUnitsToFloatPixels(outer.Y() - inner.Y(), auPerDevPixel);
      // We can't have appliedOffset be both positive and negative, and the top
      // adjustment takes priority. So here we only update appliedOffset.y if
      // it wasn't set by the top-sticky case above.
      if (appliedOffset.y == 0 && inner.Y() > 0) {
        appliedOffset.y = std::max(0, outer.Y()) - inner.Y();
        MOZ_ASSERT(appliedOffset.y < 0);
      }
    }
    // Same as above, but for the x-axis
    if (outer.XMost() != inner.XMost()) {
      nscoord distance = DistanceToRange(inner.XMost(), outer.XMost());
      if (distance > 0) {
        distance -= PositivePart(outer.X(), inner.X());
      }
      leftMargin = Some(NSAppUnitsToFloatPixels(
          itemBounds.x - scrollPort.x - distance, auPerDevPixel));
      hBounds.max =
          NSAppUnitsToFloatPixels(outer.XMost() - inner.XMost(), auPerDevPixel);
      if (inner.XMost() < 0) {
        appliedOffset.x = std::min(0, outer.XMost()) - inner.XMost();
        MOZ_ASSERT(appliedOffset.x > 0);
      }
    }
    if (outer.X() != inner.X()) {
      nscoord distance = DistanceToRange(outer.X(), inner.X());
      if (distance < 0) {
        distance += NegativePart(inner.XMost(), outer.XMost());
      }
      rightMargin = Some(NSAppUnitsToFloatPixels(
          scrollPort.XMost() - itemBounds.XMost() + distance, auPerDevPixel));
      hBounds.min =
          NSAppUnitsToFloatPixels(outer.X() - inner.X(), auPerDevPixel);
      if (appliedOffset.x == 0 && inner.X() > 0) {
        appliedOffset.x = std::max(0, outer.X()) - inner.X();
        MOZ_ASSERT(appliedOffset.x < 0);
      }
    }

    LayoutDeviceRect bounds =
        LayoutDeviceRect::FromAppUnits(itemBounds, auPerDevPixel);
    wr::LayoutVector2D applied = {
        NSAppUnitsToFloatPixels(appliedOffset.x, auPerDevPixel),
        NSAppUnitsToFloatPixels(appliedOffset.y, auPerDevPixel)};
    bool needsProp = ShouldGetStickyAnimationId();
    Maybe<wr::WrAnimationProperty> prop;
    auto spatialKey = wr::SpatialKey(uint64_t(mFrame), GetPerFrameKey(),
                                     wr::SpatialKeyKind::Sticky);
    if (needsProp) {
      RefPtr<WebRenderAPZAnimationData> animationData =
          aManager->CommandBuilder()
              .CreateOrRecycleWebRenderUserData<WebRenderAPZAnimationData>(
                  this);
      mWrStickyAnimationId = animationData->GetAnimationId();

      prop.emplace();
      prop->id = mWrStickyAnimationId;
      prop->key = spatialKey;
      prop->effect_type = wr::WrAnimationType::Transform;
    }
    wr::WrSpatialId spatialId = aBuilder.DefineStickyFrame(
        wr::ToLayoutRect(bounds), topMargin.ptrOr(nullptr),
        rightMargin.ptrOr(nullptr), bottomMargin.ptrOr(nullptr),
        leftMargin.ptrOr(nullptr), vBounds, hBounds, applied, spatialKey,
        prop.ptrOr(nullptr));

    saccHelper.emplace(aBuilder, spatialId);
    aManager->CommandBuilder().PushOverrideForASR(mContainerASR, spatialId);
  }

  {
    wr::StackingContextParams params;
    params.clip =
        wr::WrStackingContextClip::ClipChain(aBuilder.CurrentClipChainId());
    StackingContextHelper sc(aSc, GetActiveScrolledRoot(), mFrame, this,
                             aBuilder, params);
    nsDisplayOwnLayer::CreateWebRenderCommands(aBuilder, aResources, sc,
                                               aManager, aDisplayListBuilder);
  }

  if (stickyScrollContainer) {
    aManager->CommandBuilder().PopOverrideForASR(mContainerASR);
  }

  return true;
}

void nsDisplayStickyPosition::CalculateLayerScrollRanges(
    StickyScrollContainer* aStickyScrollContainer, float aAppUnitsPerDevPixel,
    float aScaleX, float aScaleY, LayerRectAbsolute& aStickyOuter,
    LayerRectAbsolute& aStickyInner) {
  nsRectAbsolute outer;
  nsRectAbsolute inner;
  aStickyScrollContainer->GetScrollRanges(mFrame, &outer, &inner);
  aStickyOuter.SetBox(
      NSAppUnitsToFloatPixels(outer.X(), aAppUnitsPerDevPixel) * aScaleX,
      NSAppUnitsToFloatPixels(outer.Y(), aAppUnitsPerDevPixel) * aScaleY,
      NSAppUnitsToFloatPixels(outer.XMost(), aAppUnitsPerDevPixel) * aScaleX,
      NSAppUnitsToFloatPixels(outer.YMost(), aAppUnitsPerDevPixel) * aScaleY);
  aStickyInner.SetBox(
      NSAppUnitsToFloatPixels(inner.X(), aAppUnitsPerDevPixel) * aScaleX,
      NSAppUnitsToFloatPixels(inner.Y(), aAppUnitsPerDevPixel) * aScaleY,
      NSAppUnitsToFloatPixels(inner.XMost(), aAppUnitsPerDevPixel) * aScaleX,
      NSAppUnitsToFloatPixels(inner.YMost(), aAppUnitsPerDevPixel) * aScaleY);
}

bool nsDisplayStickyPosition::UpdateScrollData(
    WebRenderScrollData* aData, WebRenderLayerScrollData* aLayerData) {
  bool hasDynamicToolbar = HasDynamicToolbar();
  if (aLayerData && hasDynamicToolbar) {
    StickyScrollContainer* stickyScrollContainer = GetStickyScrollContainer();
    if (stickyScrollContainer) {
      float auPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
      float cumulativeResolution =
          mFrame->PresShell()->GetCumulativeResolution();
      LayerRectAbsolute stickyOuter;
      LayerRectAbsolute stickyInner;
      CalculateLayerScrollRanges(stickyScrollContainer, auPerDevPixel,
                                 cumulativeResolution, cumulativeResolution,
                                 stickyOuter, stickyInner);
      aLayerData->SetStickyScrollRangeOuter(stickyOuter);
      aLayerData->SetStickyScrollRangeInner(stickyInner);

      SideBits sides =
          nsLayoutUtils::GetSideBitsForFixedPositionContent(mFrame);
      aLayerData->SetFixedPositionSides(sides);

      ScrollableLayerGuid::ViewID scrollId = nsLayoutUtils::FindOrCreateIDFor(
          stickyScrollContainer->ScrollContainer()
              ->GetScrolledFrame()
              ->GetContent());
      aLayerData->SetStickyPositionScrollContainerId(scrollId);
    }

    if (ShouldGetStickyAnimationId()) {
      aLayerData->SetStickyPositionAnimationId(mWrStickyAnimationId);
    }
  }
  // Return true if either there is a dynamic toolbar affecting this sticky
  // item or the OwnLayer base implementation returns true for some other
  // reason.
  bool ret = hasDynamicToolbar;
  ret |= nsDisplayOwnLayer::UpdateScrollData(aData, aLayerData);
  return ret;
}

bool nsDisplayStickyPosition::ShouldGetStickyAnimationId() const {
  return HasDynamicToolbar();  // also implies being in the cross-process RCD
}

nsDisplayScrollInfoLayer::nsDisplayScrollInfoLayer(
    nsDisplayListBuilder* aBuilder, nsIFrame* aScrolledFrame,
    nsIFrame* aScrollFrame, const CompositorHitTestInfo& aHitInfo,
    const nsRect& aHitArea)
    : nsDisplayWrapList(aBuilder, aScrollFrame),
      mScrollFrame(aScrollFrame),
      mScrolledFrame(aScrolledFrame),
      mScrollParentId(aBuilder->GetCurrentScrollParentId()),
      mHitInfo(aHitInfo),
      mHitArea(aHitArea) {
#ifdef NS_BUILD_REFCNT_LOGGING
  MOZ_COUNT_CTOR(nsDisplayScrollInfoLayer);
#endif
}

UniquePtr<ScrollMetadata> nsDisplayScrollInfoLayer::ComputeScrollMetadata(
    nsDisplayListBuilder* aBuilder, WebRenderLayerManager* aLayerManager) {
  ScrollMetadata metadata = nsLayoutUtils::ComputeScrollMetadata(
      mScrolledFrame, mScrollFrame, mScrollFrame->GetContent(), Frame(),
      ToReferenceFrame(), aLayerManager, mScrollParentId,
      mScrollFrame->GetSize(), false);
  metadata.GetMetrics().SetIsScrollInfoLayer(true);
  ScrollContainerFrame* scrollContainerFrame =
      mScrollFrame->GetScrollTargetFrame();
  if (scrollContainerFrame) {
    aBuilder->AddScrollContainerFrameToNotify(scrollContainerFrame);
  }

  return UniquePtr<ScrollMetadata>(new ScrollMetadata(metadata));
}

bool nsDisplayScrollInfoLayer::UpdateScrollData(
    WebRenderScrollData* aData, WebRenderLayerScrollData* aLayerData) {
  if (aLayerData) {
    UniquePtr<ScrollMetadata> metadata =
        ComputeScrollMetadata(aData->GetBuilder(), aData->GetManager());
    MOZ_ASSERT(aData);
    MOZ_ASSERT(metadata);
    aLayerData->AppendScrollMetadata(*aData, *metadata);
  }
  return true;
}

bool nsDisplayScrollInfoLayer::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  ScrollableLayerGuid::ViewID scrollId =
      nsLayoutUtils::FindOrCreateIDFor(mScrollFrame->GetContent());

  const LayoutDeviceRect devRect = LayoutDeviceRect::FromAppUnits(
      mHitArea, mScrollFrame->PresContext()->AppUnitsPerDevPixel());

  const wr::LayoutRect rect = wr::ToLayoutRect(devRect);

  aBuilder.PushHitTest(rect, rect, !BackfaceIsHidden(), scrollId, mHitInfo,
                       SideBits::eNone);

  return true;
}

void nsDisplayScrollInfoLayer::WriteDebugInfo(std::stringstream& aStream) {
  aStream << " (scrollframe " << mScrollFrame << " scrolledFrame "
          << mScrolledFrame << ")";
}

nsDisplayZoom::nsDisplayZoom(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                             nsSubDocumentFrame* aSubDocFrame,
                             nsDisplayList* aList, int32_t aAPD,
                             int32_t aParentAPD, nsDisplayOwnLayerFlags aFlags)
    : nsDisplaySubDocument(aBuilder, aFrame, aSubDocFrame, aList, aFlags),
      mAPD(aAPD),
      mParentAPD(aParentAPD) {
  MOZ_COUNT_CTOR(nsDisplayZoom);
}

nsRect nsDisplayZoom::GetBounds(nsDisplayListBuilder* aBuilder,
                                bool* aSnap) const {
  nsRect bounds = nsDisplaySubDocument::GetBounds(aBuilder, aSnap);
  *aSnap = false;
  return bounds.ScaleToOtherAppUnitsRoundOut(mAPD, mParentAPD);
}

void nsDisplayZoom::HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                            HitTestState* aState,
                            nsTArray<nsIFrame*>* aOutFrames) {
  nsRect rect;
  // A 1x1 rect indicates we are just hit testing a point, so pass down a 1x1
  // rect as well instead of possibly rounding the width or height to zero.
  if (aRect.width == 1 && aRect.height == 1) {
    rect.MoveTo(aRect.TopLeft().ScaleToOtherAppUnits(mParentAPD, mAPD));
    rect.width = rect.height = 1;
  } else {
    rect = aRect.ScaleToOtherAppUnitsRoundOut(mParentAPD, mAPD);
  }
  mList.HitTest(aBuilder, rect, aState, aOutFrames);
}

nsDisplayAsyncZoom::nsDisplayAsyncZoom(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    const ActiveScrolledRoot* aActiveScrolledRoot, FrameMetrics::ViewID aViewID)
    : nsDisplayOwnLayer(aBuilder, aFrame, aList, aActiveScrolledRoot),
      mViewID(aViewID) {
  MOZ_COUNT_CTOR(nsDisplayAsyncZoom);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayAsyncZoom::~nsDisplayAsyncZoom() {
  MOZ_COUNT_DTOR(nsDisplayAsyncZoom);
}
#endif

void nsDisplayAsyncZoom::HitTest(nsDisplayListBuilder* aBuilder,
                                 const nsRect& aRect, HitTestState* aState,
                                 nsTArray<nsIFrame*>* aOutFrames) {
#ifdef DEBUG
  ScrollContainerFrame* scrollContainerFrame = do_QueryFrame(mFrame);
  MOZ_ASSERT(scrollContainerFrame &&
             ViewportUtils::IsZoomedContentRoot(
                 scrollContainerFrame->GetScrolledFrame()));
#endif
  nsRect rect = ViewportUtils::VisualToLayout(aRect, mFrame->PresShell());
  mList.HitTest(aBuilder, rect, aState, aOutFrames);
}

bool nsDisplayAsyncZoom::UpdateScrollData(
    WebRenderScrollData* aData, WebRenderLayerScrollData* aLayerData) {
  bool ret = nsDisplayOwnLayer::UpdateScrollData(aData, aLayerData);
  MOZ_ASSERT(ret);
  if (aLayerData) {
    aLayerData->SetAsyncZoomContainerId(mViewID);
  }
  return ret;
}

///////////////////////////////////////////////////
// nsDisplayTransform Implementation
//

#ifndef DEBUG
static_assert(sizeof(nsDisplayTransform) <= 512,
              "nsDisplayTransform has grown");
#endif

nsDisplayTransform::nsDisplayTransform(nsDisplayListBuilder* aBuilder,
                                       nsIFrame* aFrame, nsDisplayList* aList,
                                       const nsRect& aChildrenBuildingRect)
    : nsPaintedDisplayItem(aBuilder, aFrame),
      mChildren(aBuilder),
      mTransform(Some(Matrix4x4())),
      mChildrenBuildingRect(aChildrenBuildingRect),
      mPrerenderDecision(PrerenderDecision::No),
      mIsTransformSeparator(true),
      mHasTransformGetter(false),
      mHasAssociatedPerspective(false),
      mContainsASRs(false) {
  MOZ_COUNT_CTOR(nsDisplayTransform);
  MOZ_ASSERT(aFrame, "Must have a frame!");
  Init(aBuilder, aList);
}

nsDisplayTransform::nsDisplayTransform(nsDisplayListBuilder* aBuilder,
                                       nsIFrame* aFrame, nsDisplayList* aList,
                                       const nsRect& aChildrenBuildingRect,
                                       PrerenderDecision aPrerenderDecision)
    : nsPaintedDisplayItem(aBuilder, aFrame),
      mChildren(aBuilder),
      mChildrenBuildingRect(aChildrenBuildingRect),
      mPrerenderDecision(aPrerenderDecision),
      mIsTransformSeparator(false),
      mHasTransformGetter(false),
      mHasAssociatedPerspective(false),
      mContainsASRs(false) {
  MOZ_COUNT_CTOR(nsDisplayTransform);
  MOZ_ASSERT(aFrame, "Must have a frame!");
  SetReferenceFrameToAncestor(aBuilder);
  Init(aBuilder, aList);
}

nsDisplayTransform::nsDisplayTransform(nsDisplayListBuilder* aBuilder,
                                       nsIFrame* aFrame, nsDisplayList* aList,
                                       const nsRect& aChildrenBuildingRect,
                                       decltype(WithTransformGetter))
    : nsPaintedDisplayItem(aBuilder, aFrame),
      mChildren(aBuilder),
      mChildrenBuildingRect(aChildrenBuildingRect),
      mPrerenderDecision(PrerenderDecision::No),
      mIsTransformSeparator(false),
      mHasTransformGetter(true),
      mHasAssociatedPerspective(false),
      mContainsASRs(false) {
  MOZ_COUNT_CTOR(nsDisplayTransform);
  MOZ_ASSERT(aFrame, "Must have a frame!");
  MOZ_ASSERT(aFrame->GetTransformGetter());
  Init(aBuilder, aList);
}

void nsDisplayTransform::SetReferenceFrameToAncestor(
    nsDisplayListBuilder* aBuilder) {
  if (mFrame == aBuilder->RootReferenceFrame()) {
    return;
  }
  // We manually recompute mToReferenceFrame without going through the
  // builder, since this won't apply the 'additional offset'. Our
  // children will already be painting with that applied, and we don't
  // want to include it a second time in our transform. We don't recompute
  // our visible/building rects, since those should still include the additional
  // offset.
  // TODO: Are there are things computed using our ToReferenceFrame that should
  // have the additional offset applied? Should we instead just manually remove
  // the offset from our transform instead of this more general value?
  // Can we instead apply the additional offset to us and not our children, like
  // we do for all other offsets (and how reference frames are supposed to
  // work)?
  nsIFrame* outerFrame = nsLayoutUtils::GetCrossDocParentFrameInProcess(mFrame);
  const nsIFrame* referenceFrame = aBuilder->FindReferenceFrameFor(outerFrame);
  mToReferenceFrame = mFrame->GetOffsetToCrossDoc(referenceFrame);
}

void nsDisplayTransform::Init(nsDisplayListBuilder* aBuilder,
                              nsDisplayList* aChildren) {
  mChildren.AppendToTop(aChildren);
  UpdateBounds(aBuilder);
}

bool nsDisplayTransform::ShouldFlattenAway(nsDisplayListBuilder* aBuilder) {
  return false;
}

/* Returns the delta specified by the transform-origin property.
 * This is a positive delta, meaning that it indicates the direction to move
 * to get from (0, 0) of the frame to the transform origin.  This function is
 * called off the main thread.
 */
/* static */
Point3D nsDisplayTransform::GetDeltaToTransformOrigin(
    const nsIFrame* aFrame, TransformReferenceBox& aRefBox,
    float aAppUnitsPerPixel) {
  MOZ_ASSERT(aFrame, "Can't get delta for a null frame!");
  MOZ_ASSERT(aFrame->IsTransformed() || aFrame->BackfaceIsHidden() ||
                 aFrame->Combines3DTransformWithAncestors(),
             "Shouldn't get a delta for an untransformed frame!");

  if (!aFrame->IsTransformed()) {
    return Point3D();
  }

  /* For both of the coordinates, if the value of transform is a
   * percentage, it's relative to the size of the frame.  Otherwise, if it's
   * a distance, it's already computed for us!
   */
  const nsStyleDisplay* display = aFrame->StyleDisplay();

  const StyleTransformOrigin& transformOrigin = display->mTransformOrigin;
  CSSPoint origin = nsStyleTransformMatrix::Convert2DPosition(
      transformOrigin.horizontal, transformOrigin.vertical, aRefBox);

  // Note:
  // 1. SVG frames have a reference box that can be (and typically is) offset
  //    from the TopLeft() of the frame. We need to account for that here.
  // 2. If we are using transform-box:content-box in CSS layout, we have the
  //    offset from TopLeft() of the frame as well.
  origin.x += CSSPixel::FromAppUnits(aRefBox.X());
  origin.y += CSSPixel::FromAppUnits(aRefBox.Y());

  float scale = AppUnitsPerCSSPixel() / float(aAppUnitsPerPixel);
  float z = transformOrigin.depth._0;
  return Point3D(origin.x * scale, origin.y * scale, z * scale);
}

/* static */
bool nsDisplayTransform::ComputePerspectiveMatrix(const nsIFrame* aFrame,
                                                  float aAppUnitsPerPixel,
                                                  Matrix4x4& aOutMatrix) {
  MOZ_ASSERT(aFrame, "Can't get delta for a null frame!");
  MOZ_ASSERT(aFrame->IsTransformed() || aFrame->BackfaceIsHidden() ||
                 aFrame->Combines3DTransformWithAncestors(),
             "Shouldn't get a delta for an untransformed frame!");
  MOZ_ASSERT(aOutMatrix.IsIdentity(), "Must have a blank output matrix");

  if (!aFrame->IsTransformed()) {
    return false;
  }

  // TODO: Is it possible that the perspectiveFrame's bounds haven't been set
  // correctly yet (similar to the aBoundsOverride case for
  // GetResultingTransformMatrix)?
  nsIFrame* perspectiveFrame =
      aFrame->GetClosestFlattenedTreeAncestorPrimaryFrame();
  if (!perspectiveFrame) {
    return false;
  }

  /* Grab the values for perspective and perspective-origin (if present) */
  const nsStyleDisplay* perspectiveDisplay = perspectiveFrame->StyleDisplay();
  if (perspectiveDisplay->mChildPerspective.IsNone()) {
    return false;
  }

  MOZ_ASSERT(perspectiveDisplay->mChildPerspective.IsLength());
  float perspective =
      perspectiveDisplay->mChildPerspective.AsLength().ToCSSPixels();
  perspective = std::max(1.0f, perspective);
  if (perspective < std::numeric_limits<Float>::epsilon()) {
    return true;
  }

  TransformReferenceBox refBox(perspectiveFrame);

  Point perspectiveOrigin = nsStyleTransformMatrix::Convert2DPosition(
      perspectiveDisplay->mPerspectiveOrigin.horizontal,
      perspectiveDisplay->mPerspectiveOrigin.vertical, refBox,
      aAppUnitsPerPixel);

  /* GetOffsetTo computes the offset required to move from 0,0 in
   * perspectiveFrame to 0,0 in aFrame. Although we actually want the inverse of
   * this, it's faster to compute this way.
   */
  nsPoint frameToPerspectiveOffset = -aFrame->GetOffsetTo(perspectiveFrame);
  Point frameToPerspectiveGfxOffset(
      NSAppUnitsToFloatPixels(frameToPerspectiveOffset.x, aAppUnitsPerPixel),
      NSAppUnitsToFloatPixels(frameToPerspectiveOffset.y, aAppUnitsPerPixel));

  /* Move the perspective origin to be relative to aFrame, instead of relative
   * to the containing block which is how it was specified in the style system.
   */
  perspectiveOrigin += frameToPerspectiveGfxOffset;

  aOutMatrix._34 =
      -1.0 / NSAppUnitsToFloatPixels(CSSPixel::ToAppUnits(perspective),
                                     aAppUnitsPerPixel);

  aOutMatrix.ChangeBasis(Point3D(perspectiveOrigin.x, perspectiveOrigin.y, 0));
  return true;
}

nsDisplayTransform::FrameTransformProperties::FrameTransformProperties(
    const nsIFrame* aFrame, TransformReferenceBox& aRefBox,
    float aAppUnitsPerPixel)
    : mFrame(aFrame),
      mTranslate(aFrame->StyleDisplay()->mTranslate),
      mRotate(aFrame->StyleDisplay()->mRotate),
      mScale(aFrame->StyleDisplay()->mScale),
      mTransform(aFrame->StyleDisplay()->mTransform),
      mMotion(aFrame->StyleDisplay()->mOffsetPath.IsNone()
                  ? Nothing()
                  : MotionPathUtils::ResolveMotionPath(aFrame, aRefBox)),
      mToTransformOrigin(
          GetDeltaToTransformOrigin(aFrame, aRefBox, aAppUnitsPerPixel)) {}

/* Wraps up the transform matrix in a change-of-basis matrix pair that
 * translates from local coordinate space to transform coordinate space, then
 * hands it back.
 */
Matrix4x4 nsDisplayTransform::GetResultingTransformMatrix(
    const FrameTransformProperties& aProperties, TransformReferenceBox& aRefBox,
    float aAppUnitsPerPixel) {
  return GetResultingTransformMatrixInternal(aProperties, aRefBox, nsPoint(),
                                             aAppUnitsPerPixel, 0);
}

Matrix4x4 nsDisplayTransform::GetResultingTransformMatrix(
    const nsIFrame* aFrame, const nsPoint& aOrigin, float aAppUnitsPerPixel,
    uint32_t aFlags) {
  TransformReferenceBox refBox(aFrame);
  FrameTransformProperties props(aFrame, refBox, aAppUnitsPerPixel);
  return GetResultingTransformMatrixInternal(props, refBox, aOrigin,
                                             aAppUnitsPerPixel, aFlags);
}

Matrix4x4 nsDisplayTransform::GetResultingTransformMatrixInternal(
    const FrameTransformProperties& aProperties, TransformReferenceBox& aRefBox,
    const nsPoint& aOrigin, float aAppUnitsPerPixel, uint32_t aFlags) {
  const nsIFrame* frame = aProperties.mFrame;
  NS_ASSERTION(frame || !(aFlags & INCLUDE_PERSPECTIVE),
               "Must have a frame to compute perspective!");

  // IncrementScaleRestyleCountIfNeeded in ActiveLayerTracker.cpp is a
  // simplified copy of this function.

  // Get the underlying transform matrix:

  /* Get the matrix, then change its basis to factor in the origin. */
  Matrix4x4 result;
  // Call IsSVGTransformed() regardless of the value of
  // aProperties.HasTransform(), since we still need any
  // potential parentsChildrenOnlyTransform.
  Matrix svgTransform, parentsChildrenOnlyTransform;
  const bool hasSVGTransforms =
      frame && frame->HasAnyStateBits(NS_FRAME_MAY_BE_TRANSFORMED) &&
      frame->IsSVGTransformed(&svgTransform, &parentsChildrenOnlyTransform);
  bool shouldRound = nsLayoutUtils::ShouldSnapToGrid(frame);

  /* Transformed frames always have a transform, or are preserving 3d (and might
   * still have perspective!) */
  if (aProperties.HasTransform()) {
    result = nsStyleTransformMatrix::ReadTransforms(
        aProperties.mTranslate, aProperties.mRotate, aProperties.mScale,
        aProperties.mMotion.ptrOr(nullptr), aProperties.mTransform, aRefBox,
        aAppUnitsPerPixel);
  } else if (hasSVGTransforms) {
    // Correct the translation components for zoom:
    float pixelsPerCSSPx = AppUnitsPerCSSPixel() / aAppUnitsPerPixel;
    svgTransform._31 *= pixelsPerCSSPx;
    svgTransform._32 *= pixelsPerCSSPx;
    result = Matrix4x4::From2D(svgTransform);
  }

  // Apply any translation due to 'transform-origin' and/or 'transform-box':
  result.ChangeBasis(aProperties.mToTransformOrigin);

  // See the comment for SVGContainerFrame::HasChildrenOnlyTransform for
  // an explanation of what children-only transforms are.
  const bool parentHasChildrenOnlyTransform =
      hasSVGTransforms && !parentsChildrenOnlyTransform.IsIdentity();

  if (parentHasChildrenOnlyTransform) {
    float pixelsPerCSSPx = AppUnitsPerCSSPixel() / aAppUnitsPerPixel;
    parentsChildrenOnlyTransform._31 *= pixelsPerCSSPx;
    parentsChildrenOnlyTransform._32 *= pixelsPerCSSPx;

    Point3D frameOffset(
        NSAppUnitsToFloatPixels(-frame->GetPosition().x, aAppUnitsPerPixel),
        NSAppUnitsToFloatPixels(-frame->GetPosition().y, aAppUnitsPerPixel), 0);
    Matrix4x4 parentsChildrenOnlyTransform3D =
        Matrix4x4::From2D(parentsChildrenOnlyTransform)
            .ChangeBasis(frameOffset);

    result *= parentsChildrenOnlyTransform3D;
  }

  Matrix4x4 perspectiveMatrix;
  bool hasPerspective = aFlags & INCLUDE_PERSPECTIVE;
  if (hasPerspective) {
    if (ComputePerspectiveMatrix(frame, aAppUnitsPerPixel, perspectiveMatrix)) {
      result *= perspectiveMatrix;
    }
  }

  if ((aFlags & INCLUDE_PRESERVE3D_ANCESTORS) && frame &&
      frame->Combines3DTransformWithAncestors()) {
    // Include the transform set on our parent
    nsIFrame* parentFrame =
        frame->GetClosestFlattenedTreeAncestorPrimaryFrame();
    NS_ASSERTION(parentFrame && parentFrame->IsTransformed() &&
                     parentFrame->Extend3DContext(),
                 "Preserve3D mismatch!");
    TransformReferenceBox refBox(parentFrame);
    FrameTransformProperties props(parentFrame, refBox, aAppUnitsPerPixel);

    uint32_t flags =
        aFlags & (INCLUDE_PRESERVE3D_ANCESTORS | INCLUDE_PERSPECTIVE);

    // If this frame isn't transformed (but we exist for backface-visibility),
    // then we're not a reference frame so no offset to origin will be added.
    // Otherwise we need to manually translate into our parent's coordinate
    // space.
    if (frame->IsTransformed()) {
      nsLayoutUtils::PostTranslate(result, frame->GetPosition(),
                                   aAppUnitsPerPixel, shouldRound);
    }
    Matrix4x4 parent = GetResultingTransformMatrixInternal(
        props, refBox, nsPoint(0, 0), aAppUnitsPerPixel, flags);
    result = result * parent;
  }

  if (aFlags & OFFSET_BY_ORIGIN) {
    nsLayoutUtils::PostTranslate(result, aOrigin, aAppUnitsPerPixel,
                                 shouldRound);
  }

  return result;
}

bool nsDisplayOpacity::CanUseAsyncAnimations(nsDisplayListBuilder* aBuilder) {
  static constexpr nsCSSPropertyIDSet opacitySet =
      nsCSSPropertyIDSet::OpacityProperties();
  if (ActiveLayerTracker::IsStyleAnimated(aBuilder, mFrame, opacitySet)) {
    return true;
  }

  EffectCompositor::SetPerformanceWarning(
      mFrame, opacitySet,
      AnimationPerformanceWarning(
          AnimationPerformanceWarning::Type::OpacityFrameInactive));

  return false;
}

bool nsDisplayTransform::CanUseAsyncAnimations(nsDisplayListBuilder* aBuilder) {
  return mPrerenderDecision != PrerenderDecision::No;
}

bool nsDisplayBackgroundColor::CanUseAsyncAnimations(
    nsDisplayListBuilder* aBuilder) {
  return StaticPrefs::gfx_omta_background_color();
}

static bool IsInStickyPositionedSubtree(const nsIFrame* aFrame) {
  for (const nsIFrame* frame = aFrame; frame;
       frame = nsLayoutUtils::GetCrossDocParentFrameInProcess(frame)) {
    if (frame->IsStickyPositioned()) {
      return true;
    }
  }
  return false;
}

static bool ShouldUsePartialPrerender(const nsIFrame* aFrame) {
  return StaticPrefs::layout_animation_prerender_partial() &&
         // Bug 1642547: Support partial prerender for position:sticky elements.
         !IsInStickyPositionedSubtree(aFrame);
}

/* static */
auto nsDisplayTransform::ShouldPrerenderTransformedContent(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
    nsRect* aDirtyRect) -> PrerenderInfo {
  PrerenderInfo result;
  // If we are in a preserve-3d tree, and we've disallowed async animations, we
  // return No prerender decision directly.
  if ((aFrame->Extend3DContext() ||
       aFrame->Combines3DTransformWithAncestors()) &&
      !aBuilder->GetPreserves3DAllowAsyncAnimation()) {
    return result;
  }

  // Elements whose transform has been modified recently, or which
  // have a compositor-animated transform, can be prerendered. An element
  // might have only just had its transform animated in which case
  // the ActiveLayerManager may not have been notified yet.
  static constexpr nsCSSPropertyIDSet transformSet =
      nsCSSPropertyIDSet::TransformLikeProperties();
  if (!ActiveLayerTracker::IsTransformMaybeAnimated(aFrame) &&
      !EffectCompositor::HasAnimationsForCompositor(
          aFrame, DisplayItemType::TYPE_TRANSFORM)) {
    EffectCompositor::SetPerformanceWarning(
        aFrame, transformSet,
        AnimationPerformanceWarning(
            AnimationPerformanceWarning::Type::TransformFrameInactive));

    // This case happens when we're sure that the frame is not animated and its
    // preserve-3d ancestors are not, either. So we don't need to pre-render.
    // However, this decision shouldn't affect the decisions for other frames in
    // the preserve-3d context. We need this flag to determine whether we should
    // block async animations on other frames in the current preserve-3d tree.
    result.mHasAnimations = false;
    return result;
  }

  // We should not allow prerender if any ancestor container element has
  // mask/clip-path effects.
  //
  // With prerender and async transform animation, we do not need to restyle an
  // animated element to respect position changes, since that transform is done
  // by layer animation. As a result, the container element is not aware of
  // position change of that containing element and loses the chance to update
  // the content of mask/clip-path.
  //
  // Why do we need to update a mask? This is relative to how we generate a
  // mask layer in ContainerState::SetupMaskLayerForCSSMask. While creating a
  // mask layer, to reduce memory usage, we did not choose the size of the
  // masked element as mask size. Instead, we read the union of bounds of all
  // children display items by nsDisplayWrapList::GetBounds, which is smaller
  // than or equal to the masked element's boundary, and use it as the position
  // size of the mask layer. That union bounds is actually affected by the
  // geometry of the animated element. To keep the content of mask up to date,
  // forbidding of prerender is required.
  for (nsIFrame* container =
           nsLayoutUtils::GetCrossDocParentFrameInProcess(aFrame);
       container;
       container = nsLayoutUtils::GetCrossDocParentFrameInProcess(container)) {
    const nsStyleSVGReset* svgReset = container->StyleSVGReset();
    if (svgReset->HasMask() || svgReset->HasClipPath()) {
      return result;
    }
  }

  // If the incoming dirty rect already contains the entire overflow area,
  // we are already rendering the entire content.
  nsRect overflow = aFrame->InkOverflowRectRelativeToSelf();
  // UntransformRect will not touch the output rect (`&untranformedDirtyRect`)
  // in cases of non-invertible transforms, so we set `untransformedRect` to
  // `aDirtyRect` as an initial value for such cases.
  nsRect untransformedDirtyRect = *aDirtyRect;
  UntransformRect(*aDirtyRect, overflow, aFrame, &untransformedDirtyRect);
  if (untransformedDirtyRect.Contains(overflow)) {
    *aDirtyRect = untransformedDirtyRect;
    result.mDecision = PrerenderDecision::Full;
    return result;
  }

  float viewportRatio =
      StaticPrefs::layout_animation_prerender_viewport_ratio_limit();
  uint32_t absoluteLimitX =
      StaticPrefs::layout_animation_prerender_absolute_limit_x();
  uint32_t absoluteLimitY =
      StaticPrefs::layout_animation_prerender_absolute_limit_y();
  nsSize refSize = aBuilder->RootReferenceFrame()->GetSize();

  float resolution = aFrame->PresShell()->GetCumulativeResolution();
  if (resolution < 1.0f) {
    refSize.SizeTo(
        NSCoordSaturatingNonnegativeMultiply(refSize.width, 1.0f / resolution),
        NSCoordSaturatingNonnegativeMultiply(refSize.height,
                                             1.0f / resolution));
  }

  // Only prerender if the transformed frame's size is <= a multiple of the
  // reference frame size (~viewport), and less than an absolute limit.
  // Both the ratio and the absolute limit are configurable.
  nscoord maxLength = std::max(nscoord(refSize.width * viewportRatio),
                               nscoord(refSize.height * viewportRatio));
  nsSize relativeLimit(maxLength, maxLength);
  nsSize absoluteLimit(
      aFrame->PresContext()->DevPixelsToAppUnits(absoluteLimitX),
      aFrame->PresContext()->DevPixelsToAppUnits(absoluteLimitY));
  nsSize maxSize = Min(relativeLimit, absoluteLimit);

  const auto transform = nsLayoutUtils::GetTransformToAncestor(
      RelativeTo{aFrame},
      RelativeTo{nsLayoutUtils::GetDisplayRootFrame(aFrame)});
  const gfxRect transformedBounds = transform.TransformAndClipBounds(
      gfxRect(overflow.x, overflow.y, overflow.width, overflow.height),
      gfxRect::MaxIntRect());
  const nsSize frameSize =
      nsSize(transformedBounds.width, transformedBounds.height);

  uint64_t maxLimitArea = uint64_t(maxSize.width) * maxSize.height;
  uint64_t frameArea = uint64_t(frameSize.width) * frameSize.height;
  if (frameArea <= maxLimitArea && frameSize <= absoluteLimit) {
    *aDirtyRect = overflow;
    result.mDecision = PrerenderDecision::Full;
    return result;
  }

  if (ShouldUsePartialPrerender(aFrame)) {
    *aDirtyRect = nsLayoutUtils::ComputePartialPrerenderArea(
        aFrame, untransformedDirtyRect, overflow, maxSize);
    result.mDecision = PrerenderDecision::Partial;
    return result;
  }

  if (frameArea > maxLimitArea) {
    uint64_t appUnitsPerPixel = AppUnitsPerCSSPixel();
    EffectCompositor::SetPerformanceWarning(
        aFrame, transformSet,
        AnimationPerformanceWarning(
            AnimationPerformanceWarning::Type::ContentTooLargeArea,
            {
                int(frameArea / (appUnitsPerPixel * appUnitsPerPixel)),
                int(maxLimitArea / (appUnitsPerPixel * appUnitsPerPixel)),
            }));
  } else {
    EffectCompositor::SetPerformanceWarning(
        aFrame, transformSet,
        AnimationPerformanceWarning(
            AnimationPerformanceWarning::Type::ContentTooLarge,
            {
                nsPresContext::AppUnitsToIntCSSPixels(frameSize.width),
                nsPresContext::AppUnitsToIntCSSPixels(frameSize.height),
                nsPresContext::AppUnitsToIntCSSPixels(relativeLimit.width),
                nsPresContext::AppUnitsToIntCSSPixels(relativeLimit.height),
                nsPresContext::AppUnitsToIntCSSPixels(absoluteLimit.width),
                nsPresContext::AppUnitsToIntCSSPixels(absoluteLimit.height),
            }));
  }

  return result;
}

/* If the matrix is singular, or a hidden backface is shown, the frame won't be
 * visible or hit. */
static bool IsFrameVisible(nsIFrame* aFrame, const Matrix4x4& aMatrix) {
  if (aMatrix.IsSingular()) {
    return false;
  }
  if (aFrame->BackfaceIsHidden() && aMatrix.IsBackfaceVisible()) {
    return false;
  }
  return true;
}

const Matrix4x4Flagged& nsDisplayTransform::GetTransform() const {
  if (mTransform) {
    return *mTransform;
  }

  float scale = mFrame->PresContext()->AppUnitsPerDevPixel();

  if (mHasTransformGetter) {
    mTransform.emplace((mFrame->GetTransformGetter())(mFrame, scale));
    Point3D newOrigin =
        Point3D(NSAppUnitsToFloatPixels(mToReferenceFrame.x, scale),
                NSAppUnitsToFloatPixels(mToReferenceFrame.y, scale), 0.0f);
    mTransform->ChangeBasis(newOrigin.x, newOrigin.y, newOrigin.z);
  } else if (!mIsTransformSeparator) {
    DebugOnly<bool> isReference = mFrame->IsTransformed() ||
                                  mFrame->Combines3DTransformWithAncestors() ||
                                  mFrame->Extend3DContext();
    MOZ_ASSERT(isReference);
    mTransform.emplace(
        GetResultingTransformMatrix(mFrame, ToReferenceFrame(), scale,
                                    INCLUDE_PERSPECTIVE | OFFSET_BY_ORIGIN));
  } else {
    // Use identity matrix
    mTransform.emplace();
  }

  return *mTransform;
}

const Matrix4x4Flagged& nsDisplayTransform::GetInverseTransform() const {
  if (mInverseTransform) {
    return *mInverseTransform;
  }

  MOZ_ASSERT(!GetTransform().IsSingular());

  mInverseTransform.emplace(GetTransform().Inverse());

  return *mInverseTransform;
}

Matrix4x4 nsDisplayTransform::GetTransformForRendering(
    LayoutDevicePoint* aOutOrigin) const {
  if (!mFrame->HasPerspective() || mHasTransformGetter ||
      mIsTransformSeparator) {
    if (!mHasTransformGetter && !mIsTransformSeparator && aOutOrigin) {
      // If aOutOrigin is provided, put the offset to origin into it, because
      // we need to keep it separate for webrender. The combination of
      // *aOutOrigin and the returned matrix here should always be equivalent
      // to what GetTransform() would have returned.
      float scale = mFrame->PresContext()->AppUnitsPerDevPixel();
      *aOutOrigin = LayoutDevicePoint::FromAppUnits(ToReferenceFrame(), scale);

      // The rounding behavior should also be the same as GetTransform().
      if (nsLayoutUtils::ShouldSnapToGrid(mFrame)) {
        aOutOrigin->Round();
      }
      return GetResultingTransformMatrix(mFrame, nsPoint(0, 0), scale,
                                         INCLUDE_PERSPECTIVE);
    }
    return GetTransform().GetMatrix();
  }
  MOZ_ASSERT(!mHasTransformGetter);

  float scale = mFrame->PresContext()->AppUnitsPerDevPixel();
  // Don't include perspective transform, or the offset to origin, since
  // nsDisplayPerspective will handle both of those.
  return GetResultingTransformMatrix(mFrame, ToReferenceFrame(), scale, 0);
}

const Matrix4x4& nsDisplayTransform::GetAccumulatedPreserved3DTransform(
    nsDisplayListBuilder* aBuilder) {
  MOZ_ASSERT(!mFrame->Extend3DContext() || IsLeafOf3DContext());

  if (!IsLeafOf3DContext()) {
    return GetTransform().GetMatrix();
  }

  if (!mTransformPreserves3D) {
    const nsIFrame* establisher;  // Establisher of the 3D rendering context.
    for (establisher = mFrame;
         establisher && establisher->Combines3DTransformWithAncestors();
         establisher =
             establisher->GetClosestFlattenedTreeAncestorPrimaryFrame()) {
    }
    const nsIFrame* establisherReference = aBuilder->FindReferenceFrameFor(
        nsLayoutUtils::GetCrossDocParentFrameInProcess(establisher));

    nsPoint offset = establisher->GetOffsetToCrossDoc(establisherReference);
    float scale = mFrame->PresContext()->AppUnitsPerDevPixel();
    uint32_t flags =
        INCLUDE_PRESERVE3D_ANCESTORS | INCLUDE_PERSPECTIVE | OFFSET_BY_ORIGIN;
    mTransformPreserves3D = MakeUnique<Matrix4x4>(
        GetResultingTransformMatrix(mFrame, offset, scale, flags));
  }

  return *mTransformPreserves3D;
}

bool nsDisplayTransform::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  // We want to make sure we don't pollute the transform property in the WR
  // stacking context by including the position of this frame (relative to the
  // parent reference frame). We need to keep those separate; the position of
  // this frame goes into the stacking context bounds while the transform goes
  // into the transform.
  LayoutDevicePoint position;
  Matrix4x4 newTransformMatrix = GetTransformForRendering(&position);

  gfx::Matrix4x4* transformForSC = &newTransformMatrix;
  if (newTransformMatrix.IsIdentity()) {
    // If the transform is an identity transform, strip it out so that WR
    // doesn't turn this stacking context into a reference frame, as it
    // affects positioning. Bug 1345577 tracks a better fix.
    transformForSC = nullptr;

    // In ChooseScaleAndSetTransform, we round the offset from the reference
    // frame used to adjust the transform, if there is no transform, or it
    // is just a translation. We need to do the same here.
    if (nsLayoutUtils::ShouldSnapToGrid(mFrame)) {
      position.Round();
    }
  }

  auto key = wr::SpatialKey(uint64_t(mFrame), GetPerFrameKey(),
                            wr::SpatialKeyKind::Transform);

  // We don't send animations for transform separator display items.
  uint64_t animationsId =
      mIsTransformSeparator
          ? 0
          : AddAnimationsForWebRender(
                this, aManager, aDisplayListBuilder,
                IsPartialPrerender() ? Some(position) : Nothing());
  wr::WrAnimationProperty prop{wr::WrAnimationType::Transform, animationsId,
                               key};

  nsDisplayTransform* deferredTransformItem = nullptr;
  if (ShouldDeferTransform()) {
    // If it has perspective, we create a new scroll data via the
    // UpdateScrollData call because that scenario is more complex. Otherwise,
    // if we don't contain any ASRs then just stash the transform on the
    // StackingContextHelper and apply it to any scroll data that are created
    // inside this nsDisplayTransform.
    deferredTransformItem = this;
  }

  // Determine if we're possibly animated (= would need an active layer in FLB).
  bool animated = !mIsTransformSeparator &&
                  ActiveLayerTracker::IsTransformMaybeAnimated(Frame());

  wr::StackingContextParams params;
  params.mBoundTransform = &newTransformMatrix;
  params.animation = animationsId ? &prop : nullptr;

  wr::WrTransformInfo transform_info;
  if (transformForSC) {
    transform_info.transform = wr::ToLayoutTransform(newTransformMatrix);
    transform_info.key = key;
    params.mTransformPtr = &transform_info;
  } else {
    params.mTransformPtr = nullptr;
  }

  params.prim_flags = !BackfaceIsHidden()
                          ? wr::PrimitiveFlags::IS_BACKFACE_VISIBLE
                          : wr::PrimitiveFlags{0};
  params.paired_with_perspective = mHasAssociatedPerspective;
  params.mDeferredTransformItem = deferredTransformItem;
  params.mAnimated = animated;
  // Determine if we would have to rasterize any items in local raster space
  // (i.e. disable subpixel AA). We don't always need to rasterize locally even
  // if the stacking context is possibly animated (at the cost of potentially
  // some false negatives with respect to will-change handling), so we pass in
  // this determination separately to accurately match with when FLB would
  // normally disable subpixel AA.
  params.mRasterizeLocally = animated && Frame()->HasAnimationOfTransform();
  params.SetPreserve3D(mFrame->Extend3DContext() && !mIsTransformSeparator);
  params.clip =
      wr::WrStackingContextClip::ClipChain(aBuilder.CurrentClipChainId());

  LayoutDeviceSize boundsSize = LayoutDeviceSize::FromAppUnits(
      mChildBounds.Size(), mFrame->PresContext()->AppUnitsPerDevPixel());

  StackingContextHelper sc(aSc, GetActiveScrolledRoot(), mFrame, this, aBuilder,
                           params, LayoutDeviceRect(position, boundsSize));

  aManager->CommandBuilder().CreateWebRenderCommandsFromDisplayList(
      GetChildren(), this, aDisplayListBuilder, sc, aBuilder, aResources);
  return true;
}

bool nsDisplayTransform::UpdateScrollData(
    WebRenderScrollData* aData, WebRenderLayerScrollData* aLayerData) {
  if (ShouldDeferTransform()) {
    // This case is handled in CreateWebRenderCommands by stashing the transform
    // on the stacking context.
    return false;
  }
  if (aLayerData) {
    aLayerData->SetTransform(GetTransform().GetMatrix());
    aLayerData->SetTransformIsPerspective(mFrame->ChildrenHavePerspective());
  }
  return true;
}

bool nsDisplayTransform::ShouldSkipTransform(
    nsDisplayListBuilder* aBuilder) const {
  return (aBuilder->RootReferenceFrame() == mFrame) &&
         aBuilder->IsForGenerateGlyphMask();
}

void nsDisplayTransform::Collect3DTransformLeaves(
    nsDisplayListBuilder* aBuilder, nsTArray<nsDisplayTransform*>& aLeaves) {
  if (!IsParticipating3DContext() || IsLeafOf3DContext()) {
    aLeaves.AppendElement(this);
    return;
  }

  FlattenedDisplayListIterator iter(aBuilder, &mChildren);
  while (iter.HasNext()) {
    nsDisplayItem* item = iter.GetNextItem();
    if (item->GetType() == DisplayItemType::TYPE_PERSPECTIVE) {
      auto* perspective = static_cast<nsDisplayPerspective*>(item);
      if (!perspective->GetChildren()->GetTop()) {
        continue;
      }
      item = perspective->GetChildren()->GetTop();
    }
    if (item->GetType() != DisplayItemType::TYPE_TRANSFORM) {
      gfxCriticalError() << "Invalid child item within 3D transform of type: "
                         << item->Name();
      continue;
    }
    static_cast<nsDisplayTransform*>(item)->Collect3DTransformLeaves(aBuilder,
                                                                     aLeaves);
  }
}

static RefPtr<gfx::Path> BuildPathFromPolygon(const RefPtr<DrawTarget>& aDT,
                                              const gfx::Polygon& aPolygon) {
  MOZ_ASSERT(!aPolygon.IsEmpty());

  RefPtr<PathBuilder> pathBuilder = aDT->CreatePathBuilder();
  const nsTArray<Point4D>& points = aPolygon.GetPoints();

  pathBuilder->MoveTo(points[0].As2DPoint());

  for (size_t i = 1; i < points.Length(); ++i) {
    pathBuilder->LineTo(points[i].As2DPoint());
  }

  pathBuilder->Close();
  return pathBuilder->Finish();
}

void nsDisplayTransform::CollectSorted3DTransformLeaves(
    nsDisplayListBuilder* aBuilder, nsTArray<TransformPolygon>& aLeaves) {
  std::list<TransformPolygon> inputLayers;

  nsTArray<nsDisplayTransform*> leaves;
  Collect3DTransformLeaves(aBuilder, leaves);
  for (nsDisplayTransform* item : leaves) {
    auto bounds = LayoutDeviceRect::FromAppUnits(
        item->mChildBounds, item->mFrame->PresContext()->AppUnitsPerDevPixel());
    Matrix4x4 transform = item->GetAccumulatedPreserved3DTransform(aBuilder);

    if (!IsFrameVisible(item->mFrame, transform)) {
      continue;
    }
    gfx::Polygon polygon =
        gfx::Polygon::FromRect(gfx::Rect(bounds.ToUnknownRect()));

    polygon.TransformToScreenSpace(transform);

    if (polygon.GetPoints().Length() >= 3) {
      inputLayers.push_back(TransformPolygon(item, std::move(polygon)));
    }
  }

  if (inputLayers.empty()) {
    return;
  }

  BSPTree<nsDisplayTransform> tree(inputLayers);
  nsTArray<TransformPolygon> orderedLayers(tree.GetDrawOrder());

  for (TransformPolygon& polygon : orderedLayers) {
    Matrix4x4 inverse =
        polygon.data->GetAccumulatedPreserved3DTransform(aBuilder).Inverse();

    MOZ_ASSERT(polygon.geometry);
    polygon.geometry->TransformToLayerSpace(inverse);
  }

  aLeaves = std::move(orderedLayers);
}

void nsDisplayTransform::Paint(nsDisplayListBuilder* aBuilder,
                               gfxContext* aCtx) {
  Paint(aBuilder, aCtx, Nothing());
}

void nsDisplayTransform::Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx,
                               const Maybe<gfx::Polygon>& aPolygon) {
  if (IsParticipating3DContext() && !IsLeafOf3DContext()) {
    MOZ_ASSERT(!aPolygon);
    nsTArray<TransformPolygon> leaves;
    CollectSorted3DTransformLeaves(aBuilder, leaves);
    for (TransformPolygon& item : leaves) {
      item.data->Paint(aBuilder, aCtx, item.geometry);
    }
    return;
  }

  gfxContextMatrixAutoSaveRestore saveMatrix(aCtx);
  Matrix4x4 trans = ShouldSkipTransform(aBuilder)
                        ? Matrix4x4()
                        : GetAccumulatedPreserved3DTransform(aBuilder);
  if (!IsFrameVisible(mFrame, trans)) {
    return;
  }

  Matrix trans2d;
  if (trans.CanDraw2D(&trans2d)) {
    aCtx->Multiply(ThebesMatrix(trans2d));

    if (aPolygon) {
      RefPtr<gfx::Path> path =
          BuildPathFromPolygon(aCtx->GetDrawTarget(), *aPolygon);
      aCtx->GetDrawTarget()->PushClip(path);
    }

    GetChildren()->Paint(aBuilder, aCtx,
                         mFrame->PresContext()->AppUnitsPerDevPixel());

    if (aPolygon) {
      aCtx->GetDrawTarget()->PopClip();
    }
    return;
  }

  // TODO: Implement 3d transform handling, including plane splitting and
  // sorting. See BasicCompositor.
  auto pixelBounds = LayoutDeviceRect::FromAppUnitsToOutside(
      mChildBounds, mFrame->PresContext()->AppUnitsPerDevPixel());
  RefPtr<DrawTarget> untransformedDT =
      gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
          IntSize(pixelBounds.Width(), pixelBounds.Height()),
          SurfaceFormat::B8G8R8A8, true);
  if (!untransformedDT || !untransformedDT->IsValid()) {
    return;
  }
  untransformedDT->SetTransform(
      Matrix::Translation(-Point(pixelBounds.X(), pixelBounds.Y())));

  gfxContext groupTarget(untransformedDT, /* aPreserveTransform */ true);

  if (aPolygon) {
    RefPtr<gfx::Path> path =
        BuildPathFromPolygon(aCtx->GetDrawTarget(), *aPolygon);
    aCtx->GetDrawTarget()->PushClip(path);
  }

  GetChildren()->Paint(aBuilder, &groupTarget,
                       mFrame->PresContext()->AppUnitsPerDevPixel());

  if (aPolygon) {
    aCtx->GetDrawTarget()->PopClip();
  }

  RefPtr<SourceSurface> untransformedSurf = untransformedDT->Snapshot();

  trans.PreTranslate(pixelBounds.X(), pixelBounds.Y(), 0);
  aCtx->GetDrawTarget()->Draw3DTransformedSurface(untransformedSurf, trans);
}

bool nsDisplayTransform::MayBeAnimated(nsDisplayListBuilder* aBuilder) const {
  // If EffectCompositor::HasAnimationsForCompositor() is true then we can
  // completely bypass the main thread for this animation, so it is always
  // worthwhile.
  // For ActiveLayerTracker::IsTransformAnimated() cases the main thread is
  // already involved so there is less to be gained.
  // Therefore we check that the *post-transform* bounds of this item are
  // big enough to justify an active layer.
  return EffectCompositor::HasAnimationsForCompositor(
             mFrame, DisplayItemType::TYPE_TRANSFORM) ||
         (ActiveLayerTracker::IsTransformAnimated(aBuilder, mFrame));
}

nsRect nsDisplayTransform::TransformUntransformedBounds(
    nsDisplayListBuilder* aBuilder, const Matrix4x4Flagged& aMatrix) const {
  bool snap;
  const nsRect untransformedBounds = GetUntransformedBounds(aBuilder, &snap);
  // GetTransform always operates in dev pixels.
  const float factor = mFrame->PresContext()->AppUnitsPerDevPixel();
  return nsLayoutUtils::MatrixTransformRect(untransformedBounds, aMatrix,
                                            factor);
}

/**
 * Returns the bounds for this transform. The bounds are calculated during
 * display list building and merging, see |nsDisplayTransform::UpdateBounds()|.
 */
nsRect nsDisplayTransform::GetBounds(nsDisplayListBuilder* aBuilder,
                                     bool* aSnap) const {
  *aSnap = false;
  return mBounds;
}

void nsDisplayTransform::ComputeBounds(nsDisplayListBuilder* aBuilder) {
  MOZ_ASSERT(mFrame->Extend3DContext() || IsLeafOf3DContext());

  /* Some transforms can get empty bounds in 2D, but might get transformed again
   * and get non-empty bounds. A simple example of this would be a 180 degree
   * rotation getting applied twice.
   * We should not depend on transforming bounds level by level.
   *
   * This function collects the bounds of this transform and stores it in
   * nsDisplayListBuilder. If this is not a leaf of a 3D context, we recurse
   * down and include the bounds of the child transforms.
   * The bounds are transformed with the accumulated transformation matrix up to
   * the 3D context root coordinate space.
   */
  nsDisplayListBuilder::AutoAccumulateTransform accTransform(aBuilder);
  accTransform.Accumulate(GetTransform().GetMatrix());

  // Do not dive into another 3D context.
  if (!IsLeafOf3DContext()) {
    for (nsDisplayItem* i : *GetChildren()) {
      i->DoUpdateBoundsPreserves3D(aBuilder);
    }
  }

  /* The child transforms that extend 3D context further will have empty bounds,
   * so the untransformed bounds here is the bounds of all the non-preserve-3d
   * content under this transform.
   */
  const nsRect rect = TransformUntransformedBounds(
      aBuilder, accTransform.GetCurrentTransform());
  aBuilder->AccumulateRect(rect);
}

void nsDisplayTransform::DoUpdateBoundsPreserves3D(
    nsDisplayListBuilder* aBuilder) {
  MOZ_ASSERT(mFrame->Combines3DTransformWithAncestors() ||
             IsTransformSeparator());
  // Updating is not going through to child 3D context.
  ComputeBounds(aBuilder);
}

void nsDisplayTransform::UpdateBounds(nsDisplayListBuilder* aBuilder) {
  UpdateUntransformedBounds(aBuilder);

  if (IsTransformSeparator()) {
    MOZ_ASSERT(GetTransform().IsIdentity());
    mBounds = mChildBounds;
    return;
  }

  if (mFrame->Extend3DContext()) {
    if (!Combines3DTransformWithAncestors()) {
      // The transform establishes a 3D context. |UpdateBoundsFor3D()| will
      // collect the bounds from the child transforms.
      UpdateBoundsFor3D(aBuilder);
    } else {
      // With nested 3D transforms, the 2D bounds might not be useful.
      mBounds = nsRect();
    }

    return;
  }

  MOZ_ASSERT(!mFrame->Extend3DContext());

  // We would like to avoid calculating 2D bounds here for nested 3D transforms,
  // but mix-blend-mode relies on having bounds set. See bug 1556956.

  // A stand-alone transform.
  mBounds = TransformUntransformedBounds(aBuilder, GetTransform());
}

void nsDisplayTransform::UpdateBoundsFor3D(nsDisplayListBuilder* aBuilder) {
  MOZ_ASSERT(mFrame->Extend3DContext() &&
             !mFrame->Combines3DTransformWithAncestors() &&
             !IsTransformSeparator());

  // Always start updating from an establisher of a 3D rendering context.
  nsDisplayListBuilder::AutoAccumulateRect accRect(aBuilder);
  nsDisplayListBuilder::AutoAccumulateTransform accTransform(aBuilder);
  accTransform.StartRoot();
  ComputeBounds(aBuilder);
  mBounds = aBuilder->GetAccumulatedRect();
}

void nsDisplayTransform::UpdateUntransformedBounds(
    nsDisplayListBuilder* aBuilder) {
  mChildBounds = GetChildren()->GetClippedBoundsWithRespectToASR(
      aBuilder, mActiveScrolledRoot);
}

#ifdef DEBUG_HIT
#  include <time.h>
#endif

/* HitTest does some fun stuff with matrix transforms to obtain the answer. */
void nsDisplayTransform::HitTest(nsDisplayListBuilder* aBuilder,
                                 const nsRect& aRect, HitTestState* aState,
                                 nsTArray<nsIFrame*>* aOutFrames) {
  if (aState->mInPreserves3D) {
    GetChildren()->HitTest(aBuilder, aRect, aState, aOutFrames);
    return;
  }

  /* Here's how this works:
   * 1. Get the matrix.  If it's singular, abort (clearly we didn't hit
   *    anything).
   * 2. Invert the matrix.
   * 3. Use it to transform the rect into the correct space.
   * 4. Pass that rect down through to the list's version of HitTest.
   */
  // GetTransform always operates in dev pixels.
  float factor = mFrame->PresContext()->AppUnitsPerDevPixel();
  Matrix4x4 matrix = GetAccumulatedPreserved3DTransform(aBuilder);

  if (!IsFrameVisible(mFrame, matrix)) {
    return;
  }

  const bool oldHitOccludingItem = aState->mHitOccludingItem;

  /* We want to go from transformed-space to regular space.
   * Thus we have to invert the matrix, which normally does
   * the reverse operation (e.g. regular->transformed)
   */

  /* Now, apply the transform and pass it down the channel. */
  matrix.Invert();
  nsRect resultingRect;
  // Magic width/height indicating we're hit testing a point, not a rect
  const bool testingPoint = aRect.width == 1 && aRect.height == 1;
  if (testingPoint) {
    Point4D point =
        matrix.ProjectPoint(Point(NSAppUnitsToFloatPixels(aRect.x, factor),
                                  NSAppUnitsToFloatPixels(aRect.y, factor)));
    if (!point.HasPositiveWCoord()) {
      return;
    }

    Point point2d = point.As2DPoint();

    resultingRect =
        nsRect(NSFloatPixelsToAppUnits(float(point2d.x), factor),
               NSFloatPixelsToAppUnits(float(point2d.y), factor), 1, 1);

  } else {
    Rect originalRect(NSAppUnitsToFloatPixels(aRect.x, factor),
                      NSAppUnitsToFloatPixels(aRect.y, factor),
                      NSAppUnitsToFloatPixels(aRect.width, factor),
                      NSAppUnitsToFloatPixels(aRect.height, factor));

    Rect childGfxBounds(NSAppUnitsToFloatPixels(mChildBounds.x, factor),
                        NSAppUnitsToFloatPixels(mChildBounds.y, factor),
                        NSAppUnitsToFloatPixels(mChildBounds.width, factor),
                        NSAppUnitsToFloatPixels(mChildBounds.height, factor));

    Rect rect = matrix.ProjectRectBounds(originalRect, childGfxBounds);

    resultingRect =
        nsRect(NSFloatPixelsToAppUnits(float(rect.X()), factor),
               NSFloatPixelsToAppUnits(float(rect.Y()), factor),
               NSFloatPixelsToAppUnits(float(rect.Width()), factor),
               NSFloatPixelsToAppUnits(float(rect.Height()), factor));
  }

  if (resultingRect.IsEmpty()) {
    return;
  }

#ifdef DEBUG_HIT
  printf("Frame: %p\n", dynamic_cast<void*>(mFrame));
  printf("  Untransformed point: (%f, %f)\n", resultingRect.X(),
         resultingRect.Y());
  uint32_t originalFrameCount = aOutFrames.Length();
#endif

  GetChildren()->HitTest(aBuilder, resultingRect, aState, aOutFrames);

  if (aState->mHitOccludingItem && !testingPoint &&
      !mChildBounds.Contains(aRect)) {
    MOZ_ASSERT(aBuilder->HitTestIsForVisibility());
    // We're hit-testing a rect that's bigger than our child bounds, but
    // resultingRect is clipped by our bounds (in ProjectRectBounds above), so
    // we can't stop hit-testing altogether.
    //
    // FIXME(emilio): I think this means that theoretically we might include
    // some frames fully behind other transformed-but-opaque frames? Then again
    // that's our pre-existing behavior for other untransformed content that
    // doesn't fill the whole rect. To be fully correct I think we'd need proper
    // "known occluded region" tracking, but that might be overkill for our
    // purposes here.
    aState->mHitOccludingItem = oldHitOccludingItem;
  }

#ifdef DEBUG_HIT
  if (originalFrameCount != aOutFrames.Length())
    printf("  Hit! Time: %f, first frame: %p\n", static_cast<double>(clock()),
           dynamic_cast<void*>(aOutFrames.ElementAt(0)));
  printf("=== end of hit test ===\n");
#endif
}

float nsDisplayTransform::GetHitDepthAtPoint(nsDisplayListBuilder* aBuilder,
                                             const nsPoint& aPoint) {
  // GetTransform always operates in dev pixels.
  float factor = mFrame->PresContext()->AppUnitsPerDevPixel();
  Matrix4x4 matrix = GetAccumulatedPreserved3DTransform(aBuilder);

  NS_ASSERTION(IsFrameVisible(mFrame, matrix),
               "We can't have hit a frame that isn't visible!");

  Matrix4x4 inverse = matrix;
  inverse.Invert();
  Point4D point =
      inverse.ProjectPoint(Point(NSAppUnitsToFloatPixels(aPoint.x, factor),
                                 NSAppUnitsToFloatPixels(aPoint.y, factor)));

  Point point2d = point.As2DPoint();

  Point3D transformed = matrix.TransformPoint(Point3D(point2d.x, point2d.y, 0));
  return transformed.z;
}

/* The transform is opaque iff the transform consists solely of scales and
 * translations and if the underlying content is opaque.  Thus if the transform
 * is of the form
 *
 * |a c e|
 * |b d f|
 * |0 0 1|
 *
 * We need b and c to be zero.
 *
 * We also need to check whether the underlying opaque content completely fills
 * our visible rect. We use UntransformRect which expands to the axis-aligned
 * bounding rect, but that's OK since if
 * mStoredList.GetVisibleRect().Contains(untransformedVisible), then it
 * certainly contains the actual (non-axis-aligned) untransformed rect.
 */
nsRegion nsDisplayTransform::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                             bool* aSnap) const {
  *aSnap = false;

  nsRect untransformedVisible;
  if (!UntransformBuildingRect(aBuilder, &untransformedVisible)) {
    return nsRegion();
  }

  const Matrix4x4Flagged& matrix = GetTransform();
  Matrix matrix2d;
  if (!matrix.Is2D(&matrix2d) || !matrix2d.PreservesAxisAlignedRectangles()) {
    return nsRegion();
  }

  nsRegion result;

  bool tmpSnap;
  const nsRect bounds = GetUntransformedBounds(aBuilder, &tmpSnap);
  const nsRegion opaque =
      ::mozilla::GetOpaqueRegion(aBuilder, GetChildren(), bounds);

  if (opaque.Contains(untransformedVisible)) {
    result = GetBuildingRect().Intersect(GetBounds(aBuilder, &tmpSnap));
  }
  return result;
}

nsRect nsDisplayTransform::GetComponentAlphaBounds(
    nsDisplayListBuilder* aBuilder) const {
  if (GetChildren()->GetComponentAlphaBounds(aBuilder).IsEmpty()) {
    return nsRect();
  }

  bool snap;
  return GetBounds(aBuilder, &snap);
}

/* TransformRect takes in as parameters a rectangle (in app space) and returns
 * the smallest rectangle (in app space) containing the transformed image of
 * that rectangle.  That is, it takes the four corners of the rectangle,
 * transforms them according to the matrix associated with the specified frame,
 * then returns the smallest rectangle containing the four transformed points.
 *
 * @param aUntransformedBounds The rectangle (in app units) to transform.
 * @param aFrame The frame whose transformation should be applied.
 * @param aOrigin The delta from the frame origin to the coordinate space origin
 * @return The smallest rectangle containing the image of the transformed
 *         rectangle.
 */
nsRect nsDisplayTransform::TransformRect(const nsRect& aUntransformedBounds,
                                         const nsIFrame* aFrame,
                                         TransformReferenceBox& aRefBox) {
  MOZ_ASSERT(aFrame, "Can't take the transform based on a null frame!");

  float factor = aFrame->PresContext()->AppUnitsPerDevPixel();

  FrameTransformProperties props(aFrame, aRefBox, factor);
  return nsLayoutUtils::MatrixTransformRect(
      aUntransformedBounds,
      GetResultingTransformMatrixInternal(props, aRefBox, nsPoint(), factor,
                                          kTransformRectFlags),
      factor);
}

bool nsDisplayTransform::UntransformRect(const nsRect& aTransformedBounds,
                                         const nsRect& aChildBounds,
                                         const nsIFrame* aFrame,
                                         nsRect* aOutRect) {
  MOZ_ASSERT(aFrame, "Can't take the transform based on a null frame!");

  float factor = aFrame->PresContext()->AppUnitsPerDevPixel();
  Matrix4x4 transform = GetResultingTransformMatrix(aFrame, nsPoint(), factor,
                                                    kTransformRectFlags);
  return UntransformRect(aTransformedBounds, aChildBounds, transform, factor,
                         aOutRect);
}

bool nsDisplayTransform::UntransformRect(const nsRect& aTransformedBounds,
                                         const nsRect& aChildBounds,
                                         const Matrix4x4& aMatrix,
                                         float aAppUnitsPerPixel,
                                         nsRect* aOutRect) {
  if (aMatrix.IsSingular()) {
    return false;
  }

  RectDouble result(
      NSAppUnitsToFloatPixels(aTransformedBounds.x, aAppUnitsPerPixel),
      NSAppUnitsToFloatPixels(aTransformedBounds.y, aAppUnitsPerPixel),
      NSAppUnitsToFloatPixels(aTransformedBounds.width, aAppUnitsPerPixel),
      NSAppUnitsToFloatPixels(aTransformedBounds.height, aAppUnitsPerPixel));

  RectDouble childGfxBounds(
      NSAppUnitsToFloatPixels(aChildBounds.x, aAppUnitsPerPixel),
      NSAppUnitsToFloatPixels(aChildBounds.y, aAppUnitsPerPixel),
      NSAppUnitsToFloatPixels(aChildBounds.width, aAppUnitsPerPixel),
      NSAppUnitsToFloatPixels(aChildBounds.height, aAppUnitsPerPixel));

  result = aMatrix.Inverse().ProjectRectBounds(result, childGfxBounds);
  *aOutRect = nsLayoutUtils::RoundGfxRectToAppRect(ThebesRect(result),
                                                   aAppUnitsPerPixel);
  return true;
}

bool nsDisplayTransform::UntransformRect(nsDisplayListBuilder* aBuilder,
                                         const nsRect& aRect,
                                         nsRect* aOutRect) const {
  if (GetTransform().IsSingular()) {
    return false;
  }

  // GetTransform always operates in dev pixels.
  float factor = mFrame->PresContext()->AppUnitsPerDevPixel();
  RectDouble result(NSAppUnitsToFloatPixels(aRect.x, factor),
                    NSAppUnitsToFloatPixels(aRect.y, factor),
                    NSAppUnitsToFloatPixels(aRect.width, factor),
                    NSAppUnitsToFloatPixels(aRect.height, factor));

  bool snap;
  nsRect childBounds = GetUntransformedBounds(aBuilder, &snap);
  RectDouble childGfxBounds(
      NSAppUnitsToFloatPixels(childBounds.x, factor),
      NSAppUnitsToFloatPixels(childBounds.y, factor),
      NSAppUnitsToFloatPixels(childBounds.width, factor),
      NSAppUnitsToFloatPixels(childBounds.height, factor));

  /* We want to untransform the matrix, so invert the transformation first! */
  result = GetInverseTransform().ProjectRectBounds(result, childGfxBounds);

  *aOutRect = nsLayoutUtils::RoundGfxRectToAppRect(ThebesRect(result), factor);

  return true;
}

void nsDisplayTransform::WriteDebugInfo(std::stringstream& aStream) {
  aStream << GetTransform().GetMatrix();
  if (IsTransformSeparator()) {
    aStream << " transform-separator";
  }
  if (IsLeafOf3DContext()) {
    aStream << " 3d-context-leaf";
  }
  if (mFrame->Extend3DContext()) {
    aStream << " extends-3d-context";
  }
  if (mFrame->Combines3DTransformWithAncestors()) {
    aStream << " combines-3d-with-ancestors";
  }

  aStream << " prerender(";
  switch (mPrerenderDecision) {
    case PrerenderDecision::No:
      aStream << "no";
      break;
    case PrerenderDecision::Partial:
      aStream << "partial";
      break;
    case PrerenderDecision::Full:
      aStream << "full";
      break;
  }
  aStream << ")";
  aStream << " childrenBuildingRect" << mChildrenBuildingRect;
}

nsDisplayPerspective::nsDisplayPerspective(nsDisplayListBuilder* aBuilder,
                                           nsIFrame* aFrame,
                                           nsDisplayList* aList)
    : nsPaintedDisplayItem(aBuilder, aFrame), mList(aBuilder) {
  mList.AppendToTop(aList);
  MOZ_ASSERT(mList.Length() == 1);
  MOZ_ASSERT(mList.GetTop()->GetType() == DisplayItemType::TYPE_TRANSFORM);
}

void nsDisplayPerspective::Paint(nsDisplayListBuilder* aBuilder,
                                 gfxContext* aCtx) {
  // Just directly recurse into children, since we'll include the persepctive
  // value in any nsDisplayTransform children.
  GetChildren()->Paint(aBuilder, aCtx,
                       mFrame->PresContext()->AppUnitsPerDevPixel());
}

nsRegion nsDisplayPerspective::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                               bool* aSnap) const {
  if (!GetChildren()->GetTop()) {
    *aSnap = false;
    return nsRegion();
  }

  return GetChildren()->GetTop()->GetOpaqueRegion(aBuilder, aSnap);
}

bool nsDisplayPerspective::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  float appUnitsPerPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  Matrix4x4 perspectiveMatrix;
  DebugOnly<bool> hasPerspective = nsDisplayTransform::ComputePerspectiveMatrix(
      mFrame, appUnitsPerPixel, perspectiveMatrix);
  MOZ_ASSERT(hasPerspective, "Why did we create nsDisplayPerspective?");

  /*
   * ClipListToRange can remove our child after we were created.
   */
  if (!GetChildren()->GetTop()) {
    return false;
  }

  /*
   * The resulting matrix is still in the coordinate space of the transformed
   * frame. Append a translation to the reference frame coordinates.
   */
  nsDisplayTransform* transform =
      static_cast<nsDisplayTransform*>(GetChildren()->GetTop());

  Point3D newOrigin =
      Point3D(NSAppUnitsToFloatPixels(transform->ToReferenceFrame().x,
                                      appUnitsPerPixel),
              NSAppUnitsToFloatPixels(transform->ToReferenceFrame().y,
                                      appUnitsPerPixel),
              0.0f);
  Point3D roundedOrigin(NS_round(newOrigin.x), NS_round(newOrigin.y), 0);

  perspectiveMatrix.PostTranslate(roundedOrigin);

  nsIFrame* perspectiveFrame =
      mFrame->GetClosestFlattenedTreeAncestorPrimaryFrame();

  // Passing true here is always correct, since perspective always combines
  // transforms with the descendants. However that'd make WR do a lot of work
  // that it doesn't really need to do if there aren't other transforms forming
  // part of the 3D context.
  //
  // WR knows how to treat perspective in that case, so the only thing we need
  // to do is to ensure we pass true when we're involved in a 3d context in any
  // other way via the transform-style property on either the transformed frame
  // or the perspective frame in order to not confuse WR's preserve-3d code in
  // very awful ways.
  bool preserve3D =
      mFrame->Extend3DContext() || perspectiveFrame->Extend3DContext();

  wr::StackingContextParams params;

  wr::WrTransformInfo transform_info;
  transform_info.transform = wr::ToLayoutTransform(perspectiveMatrix);
  transform_info.key = wr::SpatialKey(uint64_t(mFrame), GetPerFrameKey(),
                                      wr::SpatialKeyKind::Perspective);
  params.mTransformPtr = &transform_info;

  params.reference_frame_kind = wr::WrReferenceFrameKind::Perspective;
  params.prim_flags = !BackfaceIsHidden()
                          ? wr::PrimitiveFlags::IS_BACKFACE_VISIBLE
                          : wr::PrimitiveFlags{0};
  params.SetPreserve3D(preserve3D);
  params.clip =
      wr::WrStackingContextClip::ClipChain(aBuilder.CurrentClipChainId());

  Maybe<uint64_t> scrollingRelativeTo;
  for (const auto* asr = GetActiveScrolledRoot(); asr; asr = asr->mParent) {
    // In OOP documents, the root scrollable frame of the in-process root
    // document is always active, so using IsAncestorFrameCrossDocInProcess
    // should be fine here.
    if (nsLayoutUtils::IsAncestorFrameCrossDocInProcess(
            asr->mScrollContainerFrame->GetScrolledFrame(), perspectiveFrame)) {
      scrollingRelativeTo.emplace(asr->GetViewId());
      break;
    }
  }

  // We put the perspective reference frame wrapping the transformed frame,
  // even though there may be arbitrarily nested scroll frames in between.
  //
  // We need to know how many ancestor scroll-frames are we nested in, in order
  // for the async scrolling code in WebRender to calculate the right
  // transformation for the perspective contents.
  params.scrolling_relative_to = scrollingRelativeTo.ptrOr(nullptr);

  StackingContextHelper sc(aSc, GetActiveScrolledRoot(), mFrame, this, aBuilder,
                           params);

  aManager->CommandBuilder().CreateWebRenderCommandsFromDisplayList(
      GetChildren(), this, aDisplayListBuilder, sc, aBuilder, aResources);

  return true;
}

nsDisplayText::nsDisplayText(nsDisplayListBuilder* aBuilder,
                             nsTextFrame* aFrame)
    : nsPaintedDisplayItem(aBuilder, aFrame),
      mVisIStartEdge(0),
      mVisIEndEdge(0) {
  MOZ_COUNT_CTOR(nsDisplayText);
  mBounds = mFrame->InkOverflowRectRelativeToSelf() + ToReferenceFrame();
  // Bug 748228
  mBounds.Inflate(mFrame->PresContext()->AppUnitsPerDevPixel());
  mVisibleRect = aBuilder->GetVisibleRect() +
                 aBuilder->GetCurrentFrameOffsetToReferenceFrame();
}

bool nsDisplayText::CanApplyOpacity(WebRenderLayerManager* aManager,
                                    nsDisplayListBuilder* aBuilder) const {
  auto* f = static_cast<nsTextFrame*>(mFrame);

  if (f->IsSelected()) {
    return false;
  }

  const nsStyleText* textStyle = f->StyleText();
  if (textStyle->HasTextShadow()) {
    return false;
  }

  nsTextFrame::TextDecorations decorations;
  f->GetTextDecorations(f->PresContext(), nsTextFrame::eResolvedColors,
                        decorations);
  return !decorations.HasDecorationLines();
}

void nsDisplayText::Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) {
  AUTO_PROFILER_LABEL("nsDisplayText::Paint", GRAPHICS);
  // We don't pass mVisibleRect here, since this can be called from within
  // the WebRender fallback painting path, and we don't want to issue
  // recorded commands that are dependent on the visible/building rect.
  RenderToContext(aCtx, aBuilder, GetPaintRect(aBuilder, aCtx));

  auto* textFrame = static_cast<nsTextFrame*>(mFrame);
  LCPTextFrameHelper::MaybeUnionTextFrame(textFrame,
                                          mBounds - ToReferenceFrame());
}

bool nsDisplayText::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  auto* f = static_cast<nsTextFrame*>(mFrame);
  auto appUnitsPerDevPixel = f->PresContext()->AppUnitsPerDevPixel();

  nsRect bounds = f->WebRenderBounds() + ToReferenceFrame();
  // Bug 748228
  bounds.Inflate(appUnitsPerDevPixel);

  if (bounds.IsEmpty()) {
    return true;
  }

  // For large font sizes, punt to a blob image, to avoid the blurry rendering
  // that results from WR clamping the glyph size used for rasterization.
  //
  // (See FONT_SIZE_LIMIT in webrender/src/glyph_rasterizer/mod.rs.)
  //
  // This is not strictly accurate, as final used font sizes might not be the
  // same as claimed by the fontGroup's style.size (eg: due to font-size-adjust
  // altering the used size of the font actually used).
  // It also fails to consider how transforms might affect the device-font-size
  // that webrender uses (and clamps).
  // But it should be near enough for practical purposes; the limitations just
  // mean we might sometimes end up with webrender still applying some bitmap
  // scaling, or bail out when we didn't really need to.
  constexpr float kWebRenderFontSizeLimit = 320.0;
  f->EnsureTextRun(nsTextFrame::eInflated);
  gfxTextRun* textRun = f->GetTextRun(nsTextFrame::eInflated);
  if (textRun &&
      textRun->GetFontGroup()->GetStyle()->size > kWebRenderFontSizeLimit) {
    return false;
  }

  gfx::Point deviceOffset =
      LayoutDevicePoint::FromAppUnits(bounds.TopLeft(), appUnitsPerDevPixel)
          .ToUnknownPoint();

  // Clipping the bounds to the PaintRect (factoring in what's covered by parent
  // frames) lets us early reject a bunch of things.
  nsRect visible = mVisibleRect;

  // Add the "source rect" area from which the given shadows could intersect
  // with mVisibleRect, and which therefore needs to included in the paint
  // operation, to the `visible` rect that we will use to limit the bounds of
  // what we send to the renderer.
  auto addShadowSourceToVisible = [&](Span<const StyleSimpleShadow> aShadows) {
    for (const auto& shadow : aShadows) {
      nsRect sourceRect = mVisibleRect;
      // Negate the offsets, because we're looking for the "source" rect that
      // could cast a shadow into the visible rect, rather than a "target" area
      // onto which the visible rect would cast a shadow.
      sourceRect.MoveBy(-shadow.horizontal.ToAppUnits(),
                        -shadow.vertical.ToAppUnits());
      // Inflate to account for the shadow blur.
      sourceRect.Inflate(nsContextBoxBlur::GetBlurRadiusMargin(
          shadow.blur.ToAppUnits(), appUnitsPerDevPixel));
      visible.OrWith(sourceRect);
    }
  };

  // Shadows can translate things back into view, so we enlarge the notional
  // "visible" rect to ensure we don't skip painting relevant parts that might
  // cast a shadow within the visible area.
  addShadowSourceToVisible(f->StyleText()->mTextShadow.AsSpan());

  // Similarly for shadows that may be cast by ::selection.
  if (f->IsSelected()) {
    nsTextPaintStyle textPaint(f);
    Span<const StyleSimpleShadow> shadows;
    f->GetSelectionTextShadow(SelectionType::eNormal, textPaint, &shadows);
    addShadowSourceToVisible(shadows);
  }

  // Inflate a little extra to allow for potential antialiasing "blur".
  visible.Inflate(3 * appUnitsPerDevPixel);
  bounds = bounds.Intersect(visible);

  gfxContext* textDrawer = aBuilder.GetTextContext(aResources, aSc, aManager,
                                                   this, bounds, deviceOffset);

  LCPTextFrameHelper::MaybeUnionTextFrame(f, bounds - ToReferenceFrame());

  aBuilder.StartGroup(this);

  RenderToContext(textDrawer, aDisplayListBuilder, mVisibleRect,
                  aBuilder.GetInheritedOpacity(), true);
  const bool result = textDrawer->GetTextDrawer()->Finish();

  if (result) {
    aBuilder.FinishGroup();
  } else {
    aBuilder.CancelGroup(true);
  }

  return result;
}

void nsDisplayText::RenderToContext(gfxContext* aCtx,
                                    nsDisplayListBuilder* aBuilder,
                                    const nsRect& aVisibleRect, float aOpacity,
                                    bool aIsRecording) {
  nsTextFrame* f = static_cast<nsTextFrame*>(mFrame);

  // Add 1 pixel of dirty area around mVisibleRect to allow us to paint
  // antialiased pixels beyond the measured text extents.
  // This is temporary until we do this in the actual calculation of text
  // extents.
  auto A2D = mFrame->PresContext()->AppUnitsPerDevPixel();
  LayoutDeviceRect extraVisible =
      LayoutDeviceRect::FromAppUnits(aVisibleRect, A2D);
  extraVisible.Inflate(1);

  gfxRect pixelVisible(extraVisible.x, extraVisible.y, extraVisible.width,
                       extraVisible.height);
  pixelVisible.Inflate(2);
  pixelVisible.RoundOut();

  gfxClipAutoSaveRestore autoSaveClip(aCtx);
  if (!aBuilder->IsForGenerateGlyphMask() && !aIsRecording) {
    autoSaveClip.Clip(pixelVisible);
  }

  NS_ASSERTION(mVisIStartEdge >= 0, "illegal start edge");
  NS_ASSERTION(mVisIEndEdge >= 0, "illegal end edge");

  gfxContextMatrixAutoSaveRestore matrixSR;

  nsPoint framePt = ToReferenceFrame();
  if (f->Style()->IsTextCombined()) {
    float scaleFactor = nsTextFrame::GetTextCombineScaleFactor(f);
    if (scaleFactor != 1.0f) {
      if (auto* textDrawer = aCtx->GetTextDrawer()) {
        // WebRender doesn't support scaling text like this yet
        textDrawer->FoundUnsupportedFeature();
        return;
      }
      matrixSR.SetContext(aCtx);
      // Setup matrix to compress text for text-combine-upright if
      // necessary. This is done here because we want selection be
      // compressed at the same time as text.
      gfxPoint pt = nsLayoutUtils::PointToGfxPoint(framePt, A2D);
      gfxTextRun* textRun = f->GetTextRun(nsTextFrame::eInflated);
      if (textRun && textRun->IsRightToLeft()) {
        pt.x += gfxFloat(f->GetSize().width) / A2D;
      }
      gfxMatrix mat = aCtx->CurrentMatrixDouble()
                          .PreTranslate(pt)
                          .PreScale(scaleFactor, 1.0)
                          .PreTranslate(-pt);
      aCtx->SetMatrixDouble(mat);
    }
  }
  nsTextFrame::PaintTextParams params(aCtx);
  params.framePt = gfx::Point(framePt.x, framePt.y);
  params.dirtyRect = extraVisible;

  if (aBuilder->IsForGenerateGlyphMask()) {
    params.state = nsTextFrame::PaintTextParams::GenerateTextMask;
  } else {
    params.state = nsTextFrame::PaintTextParams::PaintText;
  }

  f->PaintText(params, mVisIStartEdge, mVisIEndEdge, ToReferenceFrame(),
               f->IsSelected(), aOpacity);
}

// This could go to nsDisplayListInvalidation.h, but
// |nsTextFrame::TextDecorations| requires including of nsTextFrame.h which
// would produce circular dependencies.
class nsDisplayTextGeometry : public nsDisplayItemGenericGeometry {
 public:
  nsDisplayTextGeometry(nsDisplayText* aItem, nsDisplayListBuilder* aBuilder)
      : nsDisplayItemGenericGeometry(aItem, aBuilder),
        mVisIStartEdge(aItem->VisIStartEdge()),
        mVisIEndEdge(aItem->VisIEndEdge()) {
    nsTextFrame* f = static_cast<nsTextFrame*>(aItem->Frame());
    f->GetTextDecorations(f->PresContext(), nsTextFrame::eResolvedColors,
                          mDecorations);
  }

  /**
   * We store the computed text decorations here since they are
   * computed using style data from parent frames. Any changes to these
   * styles will only invalidate the parent frame and not this frame.
   */
  nsTextFrame::TextDecorations mDecorations;
  nscoord mVisIStartEdge;
  nscoord mVisIEndEdge;
};

nsDisplayItemGeometry* nsDisplayText::AllocateGeometry(
    nsDisplayListBuilder* aBuilder) {
  return new nsDisplayTextGeometry(this, aBuilder);
}

void nsDisplayText::ComputeInvalidationRegion(
    nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
    nsRegion* aInvalidRegion) const {
  const nsDisplayTextGeometry* geometry =
      static_cast<const nsDisplayTextGeometry*>(aGeometry);
  nsTextFrame* f = static_cast<nsTextFrame*>(mFrame);

  nsTextFrame::TextDecorations decorations;
  f->GetTextDecorations(f->PresContext(), nsTextFrame::eResolvedColors,
                        decorations);

  bool snap;
  const nsRect& newRect = geometry->mBounds;
  nsRect oldRect = GetBounds(aBuilder, &snap);
  if (decorations != geometry->mDecorations ||
      mVisIStartEdge != geometry->mVisIStartEdge ||
      mVisIEndEdge != geometry->mVisIEndEdge ||
      !oldRect.IsEqualInterior(newRect) ||
      !geometry->mBorderRect.IsEqualInterior(GetBorderRect())) {
    aInvalidRegion->Or(oldRect, newRect);
  }
}

void nsDisplayText::WriteDebugInfo(std::stringstream& aStream) {
#ifdef DEBUG
  aStream << " (\"";

  nsTextFrame* f = static_cast<nsTextFrame*>(mFrame);
  nsCString buf;
  f->ToCString(buf);

  aStream << buf.get() << "\")";
#endif
}

nsDisplayEffectsBase::nsDisplayEffectsBase(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    const ActiveScrolledRoot* aActiveScrolledRoot, bool aClearClipChain)
    : nsDisplayWrapList(aBuilder, aFrame, aList, aActiveScrolledRoot,
                        aClearClipChain) {
  MOZ_COUNT_CTOR(nsDisplayEffectsBase);
}

nsDisplayEffectsBase::nsDisplayEffectsBase(nsDisplayListBuilder* aBuilder,
                                           nsIFrame* aFrame,
                                           nsDisplayList* aList)
    : nsDisplayWrapList(aBuilder, aFrame, aList) {
  MOZ_COUNT_CTOR(nsDisplayEffectsBase);
}

nsRegion nsDisplayEffectsBase::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                               bool* aSnap) const {
  *aSnap = false;
  return nsRegion();
}

void nsDisplayEffectsBase::HitTest(nsDisplayListBuilder* aBuilder,
                                   const nsRect& aRect, HitTestState* aState,
                                   nsTArray<nsIFrame*>* aOutFrames) {
  nsPoint rectCenter(aRect.x + aRect.width / 2, aRect.y + aRect.height / 2);
  if (SVGIntegrationUtils::HitTestFrameForEffects(
          mFrame, rectCenter - ToReferenceFrame())) {
    mList.HitTest(aBuilder, aRect, aState, aOutFrames);
  }
}

gfxRect nsDisplayEffectsBase::BBoxInUserSpace() const {
  return SVGUtils::GetBBox(mFrame);
}

gfxPoint nsDisplayEffectsBase::UserSpaceOffset() const {
  return SVGUtils::FrameSpaceInCSSPxToUserSpaceOffset(mFrame);
}

void nsDisplayEffectsBase::ComputeInvalidationRegion(
    nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
    nsRegion* aInvalidRegion) const {
  const auto* geometry =
      static_cast<const nsDisplaySVGEffectGeometry*>(aGeometry);
  bool snap;
  nsRect bounds = GetBounds(aBuilder, &snap);
  if (geometry->mFrameOffsetToReferenceFrame != ToReferenceFrame() ||
      geometry->mUserSpaceOffset != UserSpaceOffset() ||
      !geometry->mBBox.IsEqualInterior(BBoxInUserSpace())) {
    // Filter and mask output can depend on the location of the frame's user
    // space and on the frame's BBox. We need to invalidate if either of these
    // change relative to the reference frame.
    // Invalidations from our inactive layer manager are not enough to catch
    // some of these cases because filters can produce output even if there's
    // nothing in the filter input.
    aInvalidRegion->Or(bounds, geometry->mBounds);
  }
}

bool nsDisplayEffectsBase::ValidateSVGFrame() {
  if (mFrame->HasAnyStateBits(NS_FRAME_SVG_LAYOUT)) {
    ISVGDisplayableFrame* svgFrame = do_QueryFrame(mFrame);
    if (!svgFrame) {
      return false;
    }
    if (auto* svgElement = SVGElement::FromNode(mFrame->GetContent())) {
      // The SVG spec says only to draw filters if the element
      // has valid dimensions.
      return svgElement->HasValidDimensions();
    }
    return false;
  }

  return true;
}

using PaintFramesParams = SVGIntegrationUtils::PaintFramesParams;

static void ComputeMaskGeometry(PaintFramesParams& aParams) {
  // Properties are added lazily and may have been removed by a restyle, so
  // make sure all applicable ones are set again.
  nsIFrame* firstFrame =
      nsLayoutUtils::FirstContinuationOrIBSplitSibling(aParams.frame);

  const nsStyleSVGReset* svgReset = firstFrame->StyleSVGReset();

  nsTArray<SVGMaskFrame*> maskFrames;
  // XXX check return value?
  SVGObserverUtils::GetAndObserveMasks(firstFrame, &maskFrames);

  if (maskFrames.Length() == 0) {
    return;
  }

  gfxContext& ctx = aParams.ctx;
  nsIFrame* frame = aParams.frame;

  nsPoint offsetToUserSpace =
      nsLayoutUtils::ComputeOffsetToUserSpace(aParams.builder, aParams.frame);

  auto cssToDevScale = frame->PresContext()->CSSToDevPixelScale();
  int32_t appUnitsPerDevPixel = frame->PresContext()->AppUnitsPerDevPixel();

  gfxPoint devPixelOffsetToUserSpace =
      nsLayoutUtils::PointToGfxPoint(offsetToUserSpace, appUnitsPerDevPixel);

  gfxContextMatrixAutoSaveRestore matSR(&ctx);
  ctx.SetMatrixDouble(
      ctx.CurrentMatrixDouble().PreTranslate(devPixelOffsetToUserSpace));

  // Convert boaderArea and dirtyRect to user space.
  nsRect userSpaceBorderArea = aParams.borderArea - offsetToUserSpace;
  nsRect userSpaceDirtyRect = aParams.dirtyRect - offsetToUserSpace;

  // Union all mask layer rectangles in user space.
  LayoutDeviceRect maskInUserSpace;
  for (size_t i = 0; i < maskFrames.Length(); i++) {
    SVGMaskFrame* maskFrame = maskFrames[i];
    LayoutDeviceRect currentMaskSurfaceRect;

    if (maskFrame) {
      auto rect = maskFrame->GetMaskArea(aParams.frame);
      currentMaskSurfaceRect =
          CSSRect::FromUnknownRect(ToRect(rect)) * cssToDevScale;
    } else {
      nsCSSRendering::ImageLayerClipState clipState;
      nsCSSRendering::GetImageLayerClip(
          svgReset->mMask.mLayers[i], frame, *frame->StyleBorder(),
          userSpaceBorderArea, userSpaceDirtyRect,
          /* aWillPaintBorder = */ false, appUnitsPerDevPixel, &clipState);
      currentMaskSurfaceRect = LayoutDeviceRect::FromUnknownRect(
          ToRect(clipState.mDirtyRectInDevPx));
    }

    maskInUserSpace = maskInUserSpace.Union(currentMaskSurfaceRect);
  }

  if (!maskInUserSpace.IsEmpty()) {
    aParams.maskRect = Some(maskInUserSpace);
  } else {
    aParams.maskRect = Nothing();
  }
}

nsDisplayMasksAndClipPaths::nsDisplayMasksAndClipPaths(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
    const ActiveScrolledRoot* aActiveScrolledRoot, bool aWrapsBackdropFilter)
    : nsDisplayEffectsBase(aBuilder, aFrame, aList, aActiveScrolledRoot, true),
      mWrapsBackdropFilter(aWrapsBackdropFilter) {
  MOZ_COUNT_CTOR(nsDisplayMasksAndClipPaths);

  nsPresContext* presContext = mFrame->PresContext();
  uint32_t flags =
      aBuilder->GetBackgroundPaintFlags() | nsCSSRendering::PAINTBG_MASK_IMAGE;
  const nsStyleSVGReset* svgReset = aFrame->StyleSVGReset();
  NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT(i, svgReset->mMask) {
    const auto& layer = svgReset->mMask.mLayers[i];
    if (!layer.mImage.IsResolved()) {
      continue;
    }
    const nsRect& borderArea = mFrame->GetRectRelativeToSelf();
    // NOTE(emilio): We only care about the dest rect so we don't bother
    // computing a clip.
    bool isTransformedFixed = false;
    nsBackgroundLayerState state = nsCSSRendering::PrepareImageLayer(
        presContext, aFrame, flags, borderArea, borderArea, layer,
        &isTransformedFixed);
    mDestRects.AppendElement(state.mDestArea);
  }
}

static bool CanMergeDisplayMaskFrame(nsIFrame* aFrame) {
  // Do not merge items for box-decoration-break:clone elements,
  // since each box should have its own mask in that case.
  if (aFrame->StyleBorder()->mBoxDecorationBreak ==
      StyleBoxDecorationBreak::Clone) {
    return false;
  }

  // Do not merge if either frame has a mask. Continuation frames should apply
  // the mask independently (just like nsDisplayBackgroundImage).
  if (aFrame->StyleSVGReset()->HasMask()) {
    return false;
  }

  return true;
}

bool nsDisplayMasksAndClipPaths::CanMerge(const nsDisplayItem* aItem) const {
  // Items for the same content element should be merged into a single
  // compositing group.
  if (!HasDifferentFrame(aItem) || !HasSameTypeAndClip(aItem) ||
      !HasSameContent(aItem)) {
    return false;
  }

  return CanMergeDisplayMaskFrame(mFrame) &&
         CanMergeDisplayMaskFrame(aItem->Frame());
}

bool nsDisplayMasksAndClipPaths::IsValidMask() {
  if (!ValidateSVGFrame()) {
    return false;
  }

  return SVGUtils::DetermineMaskUsage(mFrame, false).UsingMaskOrClipPath();
}

bool nsDisplayMasksAndClipPaths::PaintMask(nsDisplayListBuilder* aBuilder,
                                           gfxContext* aMaskContext,
                                           bool aHandleOpacity,
                                           bool* aMaskPainted) {
  MOZ_ASSERT(aMaskContext->GetDrawTarget()->GetFormat() == SurfaceFormat::A8);

  imgDrawingParams imgParams(aBuilder->GetImageDecodeFlags());
  nsRect borderArea = nsRect(ToReferenceFrame(), mFrame->GetSize());
  PaintFramesParams params(*aMaskContext, mFrame, mBounds, borderArea, aBuilder,
                           aHandleOpacity, imgParams);
  ComputeMaskGeometry(params);
  bool maskIsComplete = false;
  bool painted = SVGIntegrationUtils::PaintMask(params, maskIsComplete);
  if (aMaskPainted) {
    *aMaskPainted = painted;
  }

  return maskIsComplete &&
         (imgParams.result == ImgDrawResult::SUCCESS ||
          imgParams.result == ImgDrawResult::SUCCESS_NOT_COMPLETE ||
          imgParams.result == ImgDrawResult::WRONG_SIZE);
}

void nsDisplayMasksAndClipPaths::ComputeInvalidationRegion(
    nsDisplayListBuilder* aBuilder, const nsDisplayItemGeometry* aGeometry,
    nsRegion* aInvalidRegion) const {
  nsDisplayEffectsBase::ComputeInvalidationRegion(aBuilder, aGeometry,
                                                  aInvalidRegion);

  const auto* geometry =
      static_cast<const nsDisplayMasksAndClipPathsGeometry*>(aGeometry);
  bool snap;
  nsRect bounds = GetBounds(aBuilder, &snap);

  if (mDestRects.Length() != geometry->mDestRects.Length()) {
    aInvalidRegion->Or(bounds, geometry->mBounds);
  } else {
    for (size_t i = 0; i < mDestRects.Length(); i++) {
      if (!mDestRects[i].IsEqualInterior(geometry->mDestRects[i])) {
        aInvalidRegion->Or(bounds, geometry->mBounds);
        break;
      }
    }
  }
}

void nsDisplayMasksAndClipPaths::PaintWithContentsPaintCallback(
    nsDisplayListBuilder* aBuilder, gfxContext* aCtx,
    const std::function<void()>& aPaintChildren) {
  // Clip the drawing target by mVisibleRect, which contains the visible
  // region of the target frame and its out-of-flow and inflow descendants.
  Rect bounds = NSRectToRect(GetPaintRect(aBuilder, aCtx),
                             mFrame->PresContext()->AppUnitsPerDevPixel());
  bounds.RoundOut();
  gfxClipAutoSaveRestore autoSaveClip(aCtx);
  autoSaveClip.Clip(bounds);

  imgDrawingParams imgParams(aBuilder->GetImageDecodeFlags());
  nsRect borderArea = nsRect(ToReferenceFrame(), mFrame->GetSize());
  PaintFramesParams params(*aCtx, mFrame, GetPaintRect(aBuilder, aCtx),
                           borderArea, aBuilder, false, imgParams);

  ComputeMaskGeometry(params);

  SVGIntegrationUtils::PaintMaskAndClipPath(params, aPaintChildren);
}

void nsDisplayMasksAndClipPaths::Paint(nsDisplayListBuilder* aBuilder,
                                       gfxContext* aCtx) {
  if (!IsValidMask()) {
    return;
  }
  PaintWithContentsPaintCallback(aBuilder, aCtx, [&] {
    GetChildren()->Paint(aBuilder, aCtx,
                         mFrame->PresContext()->AppUnitsPerDevPixel());
  });
}

static Maybe<wr::WrClipChainId> CreateSimpleClipRegion(
    const nsDisplayMasksAndClipPaths& aDisplayItem,
    wr::DisplayListBuilder& aBuilder) {
  nsIFrame* frame = aDisplayItem.Frame();
  const auto* style = frame->StyleSVGReset();
  MOZ_ASSERT(style->HasClipPath() || style->HasMask());
  if (!SVGUtils::DetermineMaskUsage(frame, false).IsSimpleClipShape()) {
    return Nothing();
  }

  const auto& clipPath = style->mClipPath;
  const auto& shape = *clipPath.AsShape()._0;

  auto appUnitsPerDevPixel = frame->PresContext()->AppUnitsPerDevPixel();
  const nsRect refBox =
      nsLayoutUtils::ComputeClipPathGeometryBox(frame, clipPath.AsShape()._1);

  wr::WrClipId clipId{};

  switch (shape.tag) {
    case StyleBasicShape::Tag::Rect: {
      const nsRect rect =
          ShapeUtils::ComputeInsetRect(shape.AsRect().rect, refBox) +
          aDisplayItem.ToReferenceFrame();

      nscoord radii[8] = {0};
      if (ShapeUtils::ComputeRectRadii(shape.AsRect().round, refBox, rect,
                                       radii)) {
        clipId = aBuilder.DefineRoundedRectClip(
            Nothing(),
            wr::ToComplexClipRegion(rect, radii, appUnitsPerDevPixel));
      } else {
        clipId = aBuilder.DefineRectClip(
            Nothing(), wr::ToLayoutRect(LayoutDeviceRect::FromAppUnits(
                           rect, appUnitsPerDevPixel)));
      }

      break;
    }
    case StyleBasicShape::Tag::Ellipse:
    case StyleBasicShape::Tag::Circle: {
      nsPoint center = ShapeUtils::ComputeCircleOrEllipseCenter(shape, refBox);

      nsSize radii;
      if (shape.IsEllipse()) {
        radii = ShapeUtils::ComputeEllipseRadii(shape, center, refBox);
      } else {
        nscoord radius = ShapeUtils::ComputeCircleRadius(shape, center, refBox);
        radii = {radius, radius};
      }

      nsRect ellipseRect(aDisplayItem.ToReferenceFrame() + center -
                             nsPoint(radii.width, radii.height),
                         radii * 2);

      nscoord ellipseRadii[8];
      for (const auto corner : AllPhysicalHalfCorners()) {
        ellipseRadii[corner] =
            HalfCornerIsX(corner) ? radii.width : radii.height;
      }

      clipId = aBuilder.DefineRoundedRectClip(
          Nothing(), wr::ToComplexClipRegion(ellipseRect, ellipseRadii,
                                             appUnitsPerDevPixel));

      break;
    }
    default:
      // Please don't add more exceptions, try to find a way to define the clip
      // without using a mask image.
      //
      // And if you _really really_ need to add an exception, add it to
      // SVGUtils::DetermineMaskUsage
      MOZ_ASSERT_UNREACHABLE("Unhandled shape id?");
      return Nothing();
  }

  wr::WrClipChainId clipChainId = aBuilder.DefineClipChain({clipId}, true);

  return Some(clipChainId);
}

static void FillPolygonDataForDisplayItem(
    const nsDisplayMasksAndClipPaths& aDisplayItem,
    nsTArray<wr::LayoutPoint>& aPoints, wr::FillRule& aFillRule) {
  nsIFrame* frame = aDisplayItem.Frame();
  const auto* style = frame->StyleSVGReset();
  bool isPolygon = style->HasClipPath() && style->mClipPath.IsShape() &&
                   style->mClipPath.AsShape()._0->IsPolygon();
  if (!isPolygon) {
    return;
  }

  const auto& clipPath = style->mClipPath;
  const auto& shape = *clipPath.AsShape()._0;
  const nsRect refBox =
      nsLayoutUtils::ComputeClipPathGeometryBox(frame, clipPath.AsShape()._1);

  // We only fill polygon data for polygons that are below a complexity
  // limit.
  nsTArray<nsPoint> vertices =
      ShapeUtils::ComputePolygonVertices(shape, refBox);
  if (vertices.Length() > wr::POLYGON_CLIP_VERTEX_MAX) {
    return;
  }

  auto appUnitsPerDevPixel = frame->PresContext()->AppUnitsPerDevPixel();

  for (size_t i = 0; i < vertices.Length(); ++i) {
    wr::LayoutPoint point = wr::ToLayoutPoint(
        LayoutDevicePoint::FromAppUnits(vertices[i], appUnitsPerDevPixel));
    aPoints.AppendElement(point);
  }

  aFillRule = (shape.AsPolygon().fill == StyleFillRule::Nonzero)
                  ? wr::FillRule::Nonzero
                  : wr::FillRule::Evenodd;
}

static Maybe<wr::WrClipChainId> CreateWRClipPathAndMasks(
    nsDisplayMasksAndClipPaths* aDisplayItem, const LayoutDeviceRect& aBounds,
    wr::IpcResourceUpdateQueue& aResources, wr::DisplayListBuilder& aBuilder,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  if (auto clip = CreateSimpleClipRegion(*aDisplayItem, aBuilder)) {
    return clip;
  }

  Maybe<wr::ImageMask> mask = aManager->CommandBuilder().BuildWrMaskImage(
      aDisplayItem, aBuilder, aResources, aSc, aDisplayListBuilder, aBounds);
  if (!mask) {
    return Nothing();
  }

  // We couldn't create a simple clip region, but before we create an image
  // mask clip, see if we can get a polygon clip to add to it.
  nsTArray<wr::LayoutPoint> points;
  wr::FillRule fillRule = wr::FillRule::Nonzero;
  FillPolygonDataForDisplayItem(*aDisplayItem, points, fillRule);

  wr::WrClipId clipId =
      aBuilder.DefineImageMaskClip(mask.ref(), points, fillRule);

  wr::WrClipChainId clipChainId = aBuilder.DefineClipChain({clipId}, true);

  return Some(clipChainId);
}

bool nsDisplayMasksAndClipPaths::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  bool snap;
  auto appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  nsRect displayBounds = GetBounds(aDisplayListBuilder, &snap);
  LayoutDeviceRect bounds =
      LayoutDeviceRect::FromAppUnits(displayBounds, appUnitsPerDevPixel);

  Maybe<wr::WrClipChainId> clip = CreateWRClipPathAndMasks(
      this, bounds, aResources, aBuilder, aSc, aManager, aDisplayListBuilder);

  float oldOpacity = aBuilder.GetInheritedOpacity();

  Maybe<StackingContextHelper> layer;
  const StackingContextHelper* sc = &aSc;
  if (clip) {
    // Create a new stacking context to attach the mask to, ensuring the mask is
    // applied to the aggregate, and not the individual elements.

    // The stacking context shouldn't have any offset.
    bounds.MoveTo(0, 0);

    Maybe<float> opacity =
        (SVGUtils::DetermineMaskUsage(mFrame, false).IsSimpleClipShape() &&
         aBuilder.GetInheritedOpacity() != 1.0f)
            ? Some(aBuilder.GetInheritedOpacity())
            : Nothing();

    wr::StackingContextParams params;
    params.clip = wr::WrStackingContextClip::ClipChain(clip->id);
    params.opacity = opacity.ptrOr(nullptr);
    if (mWrapsBackdropFilter) {
      params.flags |= wr::StackingContextFlags::WRAPS_BACKDROP_FILTER;
    }
    layer.emplace(aSc, GetActiveScrolledRoot(), mFrame, this, aBuilder, params,
                  bounds);
    sc = layer.ptr();
  }

  aBuilder.SetInheritedOpacity(1.0f);
  const DisplayItemClipChain* oldClipChain = aBuilder.GetInheritedClipChain();
  aBuilder.SetInheritedClipChain(nullptr);
  CreateWebRenderCommandsNewClipListOption(aBuilder, aResources, *sc, aManager,
                                           aDisplayListBuilder, layer.isSome());
  aBuilder.SetInheritedOpacity(oldOpacity);
  aBuilder.SetInheritedClipChain(oldClipChain);

  return true;
}

Maybe<nsRect> nsDisplayMasksAndClipPaths::GetClipWithRespectToASR(
    nsDisplayListBuilder* aBuilder, const ActiveScrolledRoot* aASR) const {
  if (const DisplayItemClip* clip =
          DisplayItemClipChain::ClipForASR(GetClipChain(), aASR)) {
    return Some(clip->GetClipRect());
  }
  // This item does not have a clip with respect to |aASR|. However, we
  // might still have finite bounds with respect to |aASR|. Check our
  // children.
  nsDisplayList* childList = GetSameCoordinateSystemChildren();
  if (childList) {
    return Some(childList->GetClippedBoundsWithRespectToASR(aBuilder, aASR));
  }
#ifdef DEBUG
  NS_ASSERTION(false, "item should have finite clip with respect to aASR");
#endif
  return Nothing();
}

#ifdef MOZ_DUMP_PAINTING
void nsDisplayMasksAndClipPaths::PrintEffects(nsACString& aTo) {
  nsIFrame* firstFrame =
      nsLayoutUtils::FirstContinuationOrIBSplitSibling(mFrame);
  bool first = true;
  aTo += " effects=(";
  SVGClipPathFrame* clipPathFrame;
  // XXX Check return value?
  SVGObserverUtils::GetAndObserveClipPath(firstFrame, &clipPathFrame);
  if (clipPathFrame) {
    if (!first) {
      aTo += ", ";
    }
    aTo += nsPrintfCString(
        "clip(%s)", clipPathFrame->IsTrivial() ? "trivial" : "non-trivial");
    first = false;
  } else if (mFrame->StyleSVGReset()->HasClipPath()) {
    if (!first) {
      aTo += ", ";
    }
    aTo += "clip(basic-shape)";
    first = false;
  }

  nsTArray<SVGMaskFrame*> masks;
  // XXX check return value?
  SVGObserverUtils::GetAndObserveMasks(firstFrame, &masks);
  if (!masks.IsEmpty() && masks[0]) {
    if (!first) {
      aTo += ", ";
    }
    aTo += "mask";
  }
  aTo += ")";
}
#endif

bool nsDisplayBackdropFilters::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  WrFiltersHolder wrFilters;
  const ComputedStyle& style = mStyle ? *mStyle : *mFrame->Style();
  auto filterChain = style.StyleEffects()->mBackdropFilters.AsSpan();
  bool initialized = true;
  if (!SVGIntegrationUtils::CreateWebRenderCSSFilters(filterChain, mFrame,
                                                      wrFilters) &&
      !SVGIntegrationUtils::BuildWebRenderFilters(
          mFrame, filterChain, StyleFilterType::BackdropFilter, wrFilters,
          initialized)) {
    // TODO: If painting backdrop-filters on the content side is implemented,
    // consider returning false to fall back to that.
    wrFilters = {};
  }

  if (!initialized) {
    wrFilters = {};
  }

  nsCSSRendering::ImageLayerClipState clip;
  nsCSSRendering::GetImageLayerClip(
      style.StyleBackground()->BottomLayer(), mFrame, *style.StyleBorder(),
      mBackdropRect, mBackdropRect, false,
      mFrame->PresContext()->AppUnitsPerDevPixel(), &clip);

  LayoutDeviceRect bounds = LayoutDeviceRect::FromAppUnits(
      mBackdropRect, mFrame->PresContext()->AppUnitsPerDevPixel());

  wr::ComplexClipRegion region =
      wr::ToComplexClipRegion(clip.mBGClipArea, clip.mRadii,
                              mFrame->PresContext()->AppUnitsPerDevPixel());

  aBuilder.PushBackdropFilter(wr::ToLayoutRect(bounds), region,
                              wrFilters.filters, wrFilters.filter_datas,
                              !BackfaceIsHidden());

  wr::StackingContextParams params;
  params.clip =
      wr::WrStackingContextClip::ClipChain(aBuilder.CurrentClipChainId());
  StackingContextHelper sc(aSc, GetActiveScrolledRoot(), mFrame, this, aBuilder,
                           params);

  nsDisplayWrapList::CreateWebRenderCommands(aBuilder, aResources, sc, aManager,
                                             aDisplayListBuilder);
  return true;
}

void nsDisplayBackdropFilters::Paint(nsDisplayListBuilder* aBuilder,
                                     gfxContext* aCtx) {
  // TODO: Implement backdrop filters
  GetChildren()->Paint(aBuilder, aCtx,
                       mFrame->PresContext()->AppUnitsPerDevPixel());
}

nsRect nsDisplayBackdropFilters::GetBounds(nsDisplayListBuilder* aBuilder,
                                           bool* aSnap) const {
  nsRect childBounds = nsDisplayWrapList::GetBounds(aBuilder, aSnap);

  *aSnap = false;

  return mBackdropRect.Union(childBounds);
}

/* static */
nsDisplayFilters::nsDisplayFilters(nsDisplayListBuilder* aBuilder,
                                   nsIFrame* aFrame, nsDisplayList* aList,
                                   nsIFrame* aStyleFrame,
                                   bool aWrapsBackdropFilter)
    : nsDisplayEffectsBase(aBuilder, aFrame, aList),
      mStyle(aFrame == aStyleFrame ? nullptr : aStyleFrame->Style()),
      mEffectsBounds(aFrame->InkOverflowRectRelativeToSelf()),
      mWrapsBackdropFilter(aWrapsBackdropFilter) {
  MOZ_COUNT_CTOR(nsDisplayFilters);
  mVisibleRect = aBuilder->GetVisibleRect() +
                 aBuilder->GetCurrentFrameOffsetToReferenceFrame();
}

void nsDisplayFilters::Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) {
  PaintWithContentsPaintCallback(aBuilder, aCtx, [&](gfxContext* aContext) {
    GetChildren()->Paint(aBuilder, aContext,
                         mFrame->PresContext()->AppUnitsPerDevPixel());
  });
}

void nsDisplayFilters::PaintWithContentsPaintCallback(
    nsDisplayListBuilder* aBuilder, gfxContext* aCtx,
    const std::function<void(gfxContext* aContext)>& aPaintChildren) {
  imgDrawingParams imgParams(aBuilder->GetImageDecodeFlags());
  nsRect borderArea = nsRect(ToReferenceFrame(), mFrame->GetSize());
  PaintFramesParams params(*aCtx, mFrame, mVisibleRect, borderArea, aBuilder,
                           false, imgParams);

  gfxPoint userSpaceToFrameSpaceOffset =
      SVGIntegrationUtils::GetOffsetToUserSpaceInDevPx(mFrame, params);

  auto filterChain = mStyle ? mStyle->StyleEffects()->mFilters.AsSpan()
                            : mFrame->StyleEffects()->mFilters.AsSpan();
  SVGIntegrationUtils::PaintFilter(
      params, filterChain,
      [&](gfxContext& aContext, imgDrawingParams&, const gfxMatrix*,
          const nsIntRect*) {
        gfxContextMatrixAutoSaveRestore autoSR(&aContext);
        aContext.SetMatrixDouble(aContext.CurrentMatrixDouble().PreTranslate(
            -userSpaceToFrameSpaceOffset));
        aPaintChildren(&aContext);
      });
}

bool nsDisplayFilters::CanCreateWebRenderCommands() const {
  return SVGIntegrationUtils::CanCreateWebRenderFiltersForFrame(mFrame);
}

bool nsDisplayFilters::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  WrFiltersHolder wrFilters;
  const ComputedStyle& style = mStyle ? *mStyle : *mFrame->Style();
  auto filterChain = style.StyleEffects()->mFilters.AsSpan();
  bool initialized = true;
  if (!SVGIntegrationUtils::CreateWebRenderCSSFilters(filterChain, mFrame,
                                                      wrFilters) &&
      !SVGIntegrationUtils::BuildWebRenderFilters(mFrame, filterChain,
                                                  StyleFilterType::Filter,
                                                  wrFilters, initialized)) {
    if (mStyle) {
      // TODO(bug 1769223): Support fallback filters in the root code-path,
      // perhaps. For now treat it the same way as invalid filters.
      wrFilters = {};
    } else {
      // Draw using fallback.
      return false;
    }
  }

  if (!initialized) {
    // https://drafts.fxtf.org/filter-effects/#typedef-filter-url:
    //
    //   If the filter references a non-existent object or the referenced object
    //   is not a filter element, then the whole filter chain is ignored. No
    //   filter is applied to the object.
    //
    // Note that other engines have a weird discrepancy between SVG and HTML
    // content here, but the spec is clear.
    wrFilters = {};
  }

  uint64_t clipChainId;
  if (wrFilters.post_filters_clip) {
    auto devPxRect = LayoutDeviceRect::FromAppUnits(
        wrFilters.post_filters_clip.value() + ToReferenceFrame(),
        mFrame->PresContext()->AppUnitsPerDevPixel());
    auto clipId =
        aBuilder.DefineRectClip(Nothing(), wr::ToLayoutRect(devPxRect));
    clipChainId = aBuilder.DefineClipChain({clipId}, true).id;
  } else {
    clipChainId = aBuilder.CurrentClipChainId();
  }
  wr::WrStackingContextClip clip =
      wr::WrStackingContextClip::ClipChain(clipChainId);

  float opacity = aBuilder.GetInheritedOpacity();
  aBuilder.SetInheritedOpacity(1.0f);
  const DisplayItemClipChain* oldClipChain = aBuilder.GetInheritedClipChain();
  aBuilder.SetInheritedClipChain(nullptr);
  wr::StackingContextParams params;
  params.mFilters = std::move(wrFilters.filters);
  params.mFilterDatas = std::move(wrFilters.filter_datas);
  params.opacity = opacity != 1.0f ? &opacity : nullptr;
  params.clip = clip;
  if (mWrapsBackdropFilter) {
    params.flags |= wr::StackingContextFlags::WRAPS_BACKDROP_FILTER;
  }
  StackingContextHelper sc(aSc, GetActiveScrolledRoot(), mFrame, this, aBuilder,
                           params);

  nsDisplayEffectsBase::CreateWebRenderCommands(aBuilder, aResources, sc,
                                                aManager, aDisplayListBuilder);
  aBuilder.SetInheritedOpacity(opacity);
  aBuilder.SetInheritedClipChain(oldClipChain);

  return true;
}

#ifdef MOZ_DUMP_PAINTING
void nsDisplayFilters::PrintEffects(nsACString& aTo) {
  nsIFrame* firstFrame =
      nsLayoutUtils::FirstContinuationOrIBSplitSibling(mFrame);
  bool first = true;
  aTo += " effects=(";
  // We may exist for a mix of CSS filter functions and/or references to SVG
  // filters.  If we have invalid references to SVG filters then we paint
  // nothing, but otherwise we will apply one or more filters.
  if (SVGObserverUtils::GetAndObserveFilters(firstFrame, nullptr) !=
      SVGObserverUtils::eHasRefsSomeInvalid) {
    if (!first) {
      aTo += ", ";
    }
    aTo += "filter";
  }
  aTo += ")";
}
#endif

nsDisplaySVGWrapper::nsDisplaySVGWrapper(nsDisplayListBuilder* aBuilder,
                                         nsIFrame* aFrame, nsDisplayList* aList)
    : nsDisplayWrapList(aBuilder, aFrame, aList) {
  MOZ_COUNT_CTOR(nsDisplaySVGWrapper);
}

bool nsDisplaySVGWrapper::ShouldFlattenAway(nsDisplayListBuilder* aBuilder) {
  return !aBuilder->GetWidgetLayerManager();
}

bool nsDisplaySVGWrapper::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  return CreateWebRenderCommandsNewClipListOption(
      aBuilder, aResources, aSc, aManager, aDisplayListBuilder, false);
}

nsDisplayForeignObject::nsDisplayForeignObject(nsDisplayListBuilder* aBuilder,
                                               nsIFrame* aFrame,
                                               nsDisplayList* aList)
    : nsDisplayWrapList(aBuilder, aFrame, aList) {
  MOZ_COUNT_CTOR(nsDisplayForeignObject);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayForeignObject::~nsDisplayForeignObject() {
  MOZ_COUNT_DTOR(nsDisplayForeignObject);
}
#endif

bool nsDisplayForeignObject::ShouldFlattenAway(nsDisplayListBuilder* aBuilder) {
  return !aBuilder->GetWidgetLayerManager();
}

bool nsDisplayForeignObject::CreateWebRenderCommands(
    wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc, RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  AutoRestore<bool> restoreDoGrouping(aManager->CommandBuilder().mDoGrouping);
  aManager->CommandBuilder().mDoGrouping = false;
  return CreateWebRenderCommandsNewClipListOption(
      aBuilder, aResources, aSc, aManager, aDisplayListBuilder, false);
}

void nsDisplayLink::Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) {
  auto appPerDev = mFrame->PresContext()->AppUnitsPerDevPixel();
  aCtx->GetDrawTarget()->Link(
      mLinkURI.get(), mLinkDest.get(),
      NSRectToRect(GetPaintRect(aBuilder, aCtx), appPerDev));
}

void nsDisplayDestination::Paint(nsDisplayListBuilder* aBuilder,
                                 gfxContext* aCtx) {
  auto appPerDev = mFrame->PresContext()->AppUnitsPerDevPixel();
  aCtx->GetDrawTarget()->Destination(
      mDestinationName.get(),
      NSPointToPoint(GetPaintRect(aBuilder, aCtx).TopLeft(), appPerDev));
}

void nsDisplayListCollection::SerializeWithCorrectZOrder(
    nsDisplayList* aOutResultList, nsIContent* aContent) {
  // Sort PositionedDescendants() in CSS 'z-order' order.  The list is already
  // in content document order and SortByZOrder is a stable sort which
  // guarantees that boxes produced by the same element are placed together
  // in the sort. Consider a position:relative inline element that breaks
  // across lines and has absolutely positioned children; all the abs-pos
  // children should be z-ordered after all the boxes for the position:relative
  // element itself.
  PositionedDescendants()->SortByZOrder();

  // Now follow the rules of http://www.w3.org/TR/CSS21/zindex.html
  // 1,2: backgrounds and borders
  aOutResultList->AppendToTop(BorderBackground());
  // 3: negative z-index children.
  for (auto* item : PositionedDescendants()->TakeItems()) {
    if (item->ZIndex() < 0) {
      aOutResultList->AppendToTop(item);
    } else {
      PositionedDescendants()->AppendToTop(item);
    }
  }

  // 4: block backgrounds
  aOutResultList->AppendToTop(BlockBorderBackgrounds());
  // 5: floats
  aOutResultList->AppendToTop(Floats());
  // 7: general content
  aOutResultList->AppendToTop(Content());
  // 7.5: outlines, in content tree order. We need to sort by content order
  // because an element with outline that breaks and has children with outline
  // might have placed child outline items between its own outline items.
  // The element's outline items need to all come before any child outline
  // items.
  if (aContent) {
    Outlines()->SortByContentOrder(aContent);
  }
  aOutResultList->AppendToTop(Outlines());
  // 8, 9: non-negative z-index children
  aOutResultList->AppendToTop(PositionedDescendants());
}

uint32_t PaintTelemetry::sPaintLevel = 0;

PaintTelemetry::AutoRecordPaint::AutoRecordPaint() {
  // Don't record nested paints.
  if (sPaintLevel++ > 0) {
    return;
  }

  mStart = TimeStamp::Now();
}

PaintTelemetry::AutoRecordPaint::~AutoRecordPaint() {
  MOZ_ASSERT(sPaintLevel != 0);
  if (--sPaintLevel > 0) {
    return;
  }

  // If we're in multi-process mode, don't include paint times for the parent
  // process.
  if (gfxVars::BrowserTabsRemoteAutostart() && XRE_IsParentProcess()) {
    return;
  }

  // Record the total time.
  mozilla::glean::gfx_content::paint_time.AccumulateRawDuration(
      TimeStamp::Now() - mStart);
}

static nsIFrame* GetSelfOrPlaceholderFor(nsIFrame* aFrame) {
  if (aFrame->HasAnyStateBits(NS_FRAME_IS_PUSHED_FLOAT)) {
    return aFrame;
  }

  if (aFrame->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW) &&
      !aFrame->GetPrevInFlow()) {
    return aFrame->GetPlaceholderFrame();
  }

  return aFrame;
}

static nsIFrame* GetAncestorFor(nsIFrame* aFrame) {
  nsIFrame* f = GetSelfOrPlaceholderFor(aFrame);
  MOZ_ASSERT(f);
  return nsLayoutUtils::GetCrossDocParentFrameInProcess(f);
}

nsDisplayListBuilder::AutoBuildingDisplayList::AutoBuildingDisplayList(
    nsDisplayListBuilder* aBuilder, nsIFrame* aForChild,
    const nsRect& aVisibleRect, const nsRect& aDirtyRect,
    const bool aIsTransformed)
    : mBuilder(aBuilder),
      mPrevFrame(aBuilder->mCurrentFrame),
      mPrevReferenceFrame(aBuilder->mCurrentReferenceFrame),
      mPrevOffset(aBuilder->mCurrentOffsetToReferenceFrame),
      mPrevAdditionalOffset(aBuilder->mAdditionalOffset),
      mPrevVisibleRect(aBuilder->mVisibleRect),
      mPrevDirtyRect(aBuilder->mDirtyRect),
      mPrevCompositorHitTestInfo(aBuilder->mCompositorHitTestInfo),
      mPrevAncestorHasApzAwareEventHandler(
          aBuilder->mAncestorHasApzAwareEventHandler),
      mPrevBuildingInvisibleItems(aBuilder->mBuildingInvisibleItems),
      mPrevInInvalidSubtree(aBuilder->mInInvalidSubtree) {
  if (aIsTransformed) {
    aBuilder->mCurrentOffsetToReferenceFrame =
        aBuilder->AdditionalOffset().refOr(nsPoint());
    aBuilder->mCurrentReferenceFrame = aForChild;
  } else if (aBuilder->mCurrentFrame == aForChild->GetParent()) {
    aBuilder->mCurrentOffsetToReferenceFrame += aForChild->GetPosition();
  } else {
    aBuilder->mCurrentReferenceFrame = aBuilder->FindReferenceFrameFor(
        aForChild, &aBuilder->mCurrentOffsetToReferenceFrame);
  }

  // If aForChild is being visited from a frame other than it's ancestor frame,
  // mInInvalidSubtree will need to be recalculated the slow way.
  if (aForChild == mPrevFrame || GetAncestorFor(aForChild) == mPrevFrame) {
    aBuilder->mInInvalidSubtree =
        aBuilder->mInInvalidSubtree || aForChild->IsFrameModified();
  } else {
    aBuilder->mInInvalidSubtree = AnyContentAncestorModified(aForChild);
  }

  aBuilder->mCurrentFrame = aForChild;
  aBuilder->mVisibleRect = aVisibleRect;
  aBuilder->mDirtyRect =
      aBuilder->mInInvalidSubtree ? aVisibleRect : aDirtyRect;
}

}  // namespace mozilla
