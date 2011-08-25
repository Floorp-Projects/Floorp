/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Novell code.
 *
 * The Initial Developer of the Original Code is Novell Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *     robert@ocallahan.org
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
 * ***** END LICENSE BLOCK *****
 */

/*
 * structures that represent things to be painted (ordered in z-order),
 * used during painting and hit testing
 */

#include "nsDisplayList.h"

#include "nsCSSRendering.h"
#include "nsRenderingContext.h"
#include "nsISelectionController.h"
#include "nsIPresShell.h"
#include "nsRegion.h"
#include "nsFrameManager.h"
#include "gfxContext.h"
#include "nsStyleStructInlines.h"
#include "nsStyleTransformMatrix.h"
#include "gfxMatrix.h"
#include "nsSVGIntegrationUtils.h"
#include "nsLayoutUtils.h"
#include "nsIScrollableFrame.h"
#include "nsThemeConstants.h"

#include "imgIContainer.h"
#include "nsIInterfaceRequestorUtils.h"
#include "BasicLayers.h"
#include "nsBoxFrame.h"
#include "nsViewportFrame.h"

using namespace mozilla;
using namespace mozilla::layers;
typedef FrameMetrics::ViewID ViewID;

nsDisplayListBuilder::nsDisplayListBuilder(nsIFrame* aReferenceFrame,
    Mode aMode, PRBool aBuildCaret)
    : mReferenceFrame(aReferenceFrame),
      mIgnoreScrollFrame(nsnull),
      mCurrentTableItem(nsnull),
      mFinalTransparentRegion(nsnull),
      mMode(aMode),
      mBuildCaret(aBuildCaret),
      mIgnoreSuppression(PR_FALSE),
      mHadToIgnoreSuppression(PR_FALSE),
      mIsAtRootOfPseudoStackingContext(PR_FALSE),
      mIncludeAllOutOfFlows(PR_FALSE),
      mSelectedFramesOnly(PR_FALSE),
      mAccurateVisibleRegions(PR_FALSE),
      mInTransform(PR_FALSE),
      mSyncDecodeImages(PR_FALSE),
      mIsPaintingToWindow(PR_FALSE),
      mSnappingEnabled(mMode != EVENT_DELIVERY),
      mHasDisplayPort(PR_FALSE),
      mHasFixedItems(PR_FALSE)
{
  MOZ_COUNT_CTOR(nsDisplayListBuilder);
  PL_InitArenaPool(&mPool, "displayListArena", 1024,
                   NS_MAX(NS_ALIGNMENT_OF(void*),NS_ALIGNMENT_OF(double))-1);

  nsPresContext* pc = aReferenceFrame->PresContext();
  nsIPresShell *shell = pc->PresShell();
  if (pc->IsRenderingOnlySelection()) {
    nsCOMPtr<nsISelectionController> selcon(do_QueryInterface(shell));
    if (selcon) {
      selcon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                           getter_AddRefs(mBoundingSelection));
    }
  }

  if(mReferenceFrame->GetType() == nsGkAtoms::viewportFrame) {
    ViewportFrame* viewportFrame = static_cast<ViewportFrame*>(mReferenceFrame);
    if (!viewportFrame->GetChildList(nsIFrame::kFixedList).IsEmpty()) {
      mHasFixedItems = PR_TRUE;
    }
  }

  LayerBuilder()->Init(this);

  PR_STATIC_ASSERT(nsDisplayItem::TYPE_MAX < (1 << nsDisplayItem::TYPE_BITS));
}

static void MarkFrameForDisplay(nsIFrame* aFrame, nsIFrame* aStopAtFrame) {
  nsFrameManager* frameManager = aFrame->PresContext()->PresShell()->FrameManager();

  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetParentOrPlaceholderFor(frameManager, f)) {
    if (f->GetStateBits() & NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO)
      return;
    f->AddStateBits(NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO);
    if (f == aStopAtFrame) {
      // we've reached a frame that we know will be painted, so we can stop.
      break;
    }
  }
}

static PRBool IsFixedFrame(nsIFrame* aFrame)
{
  return aFrame && aFrame->GetParent() && !aFrame->GetParent()->GetParent();
}

static PRBool IsFixedItem(nsDisplayItem *aItem, nsDisplayListBuilder* aBuilder,
                          PRBool* aIsFixedBackground)
{
  nsIFrame* activeScrolledRoot =
    nsLayoutUtils::GetActiveScrolledRootFor(aItem, aBuilder, aIsFixedBackground);
  return activeScrolledRoot &&
         !nsLayoutUtils::ScrolledByViewportScrolling(activeScrolledRoot,
                                                     aBuilder);
}

static PRBool ForceVisiblityForFixedItem(nsDisplayListBuilder* aBuilder,
                                         nsDisplayItem* aItem,
                                         PRBool* aIsFixedBackground)
{
  return aBuilder->GetDisplayPort() && aBuilder->GetHasFixedItems() &&
         IsFixedItem(aItem, aBuilder, aIsFixedBackground);
}

void nsDisplayListBuilder::SetDisplayPort(const nsRect& aDisplayPort)
{
    static bool fixedPositionLayersEnabled = getenv("MOZ_ENABLE_FIXED_POSITION_LAYERS") != 0;
    if (fixedPositionLayersEnabled) {
      mHasDisplayPort = PR_TRUE;
      mDisplayPort = aDisplayPort;
    }
}

void nsDisplayListBuilder::MarkOutOfFlowFrameForDisplay(nsIFrame* aDirtyFrame,
                                                        nsIFrame* aFrame,
                                                        const nsRect& aDirtyRect)
{
  nsRect dirty = aDirtyRect - aFrame->GetOffsetTo(aDirtyFrame);
  nsRect overflowRect = aFrame->GetVisualOverflowRect();

  if (mHasDisplayPort && IsFixedFrame(aFrame)) {
    dirty = overflowRect;
  }

  if (!dirty.IntersectRect(dirty, overflowRect))
    return;
  aFrame->Properties().Set(nsDisplayListBuilder::OutOfFlowDirtyRectProperty(),
                           new nsRect(dirty));

  MarkFrameForDisplay(aFrame, aDirtyFrame);
}

static void UnmarkFrameForDisplay(nsIFrame* aFrame) {
  nsPresContext* presContext = aFrame->PresContext();
  presContext->PropertyTable()->
    Delete(aFrame, nsDisplayListBuilder::OutOfFlowDirtyRectProperty());

  nsFrameManager* frameManager = presContext->PresShell()->FrameManager();

  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetParentOrPlaceholderFor(frameManager, f)) {
    if (!(f->GetStateBits() & NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO))
      return;
    f->RemoveStateBits(NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO);
  }
}

static void RecordFrameMetrics(nsIFrame* aForFrame,
                               nsIFrame* aScrollFrame,
                               ContainerLayer* aRoot,
                               const nsRect& aVisibleRect,
                               const nsRect& aViewport,
                               nsRect* aDisplayPort,
                               ViewID aScrollId,
                               const nsDisplayItem::ContainerParameters& aContainerParameters) {
  nsPresContext* presContext = aForFrame->PresContext();
  PRInt32 auPerDevPixel = presContext->AppUnitsPerDevPixel();

  nsIntRect visible = aVisibleRect.ScaleToNearestPixels(
    aContainerParameters.mXScale, aContainerParameters.mYScale, auPerDevPixel);
  aRoot->SetVisibleRegion(nsIntRegion(visible));

  FrameMetrics metrics;

  metrics.mViewport = aViewport.ScaleToNearestPixels(
    aContainerParameters.mXScale, aContainerParameters.mYScale, auPerDevPixel);

  if (aDisplayPort) {
    metrics.mDisplayPort = aDisplayPort->ScaleToNearestPixels(
      aContainerParameters.mXScale, aContainerParameters.mYScale, auPerDevPixel);
  }

  nsIScrollableFrame* scrollableFrame = nsnull;
  if (aScrollFrame)
    scrollableFrame = aScrollFrame->GetScrollTargetFrame();

  if (scrollableFrame) {
    nsSize contentSize =
      scrollableFrame->GetScrollRange().Size() +
      scrollableFrame->GetScrollPortRect().Size();
    metrics.mContentSize = contentSize.ScaleToNearestPixels(
      aContainerParameters.mXScale, aContainerParameters.mYScale, auPerDevPixel);
    metrics.mViewportScrollOffset = scrollableFrame->GetScrollPosition().ScaleToNearestPixels(
      aContainerParameters.mXScale, aContainerParameters.mYScale, auPerDevPixel);
  }
  else {
    nsSize contentSize = aForFrame->GetSize();
    metrics.mContentSize = contentSize.ScaleToNearestPixels(
      aContainerParameters.mXScale, aContainerParameters.mYScale, auPerDevPixel);
  }

  metrics.mScrollId = aScrollId;
  aRoot->SetFrameMetrics(metrics);
}

nsDisplayListBuilder::~nsDisplayListBuilder() {
  NS_ASSERTION(mFramesMarkedForDisplay.Length() == 0,
               "All frames should have been unmarked");
  NS_ASSERTION(mPresShellStates.Length() == 0,
               "All presshells should have been exited");
  NS_ASSERTION(!mCurrentTableItem, "No table item should be active");

  PL_FreeArenaPool(&mPool);
  PL_FinishArenaPool(&mPool);
  MOZ_COUNT_DTOR(nsDisplayListBuilder);
}

PRUint32
nsDisplayListBuilder::GetBackgroundPaintFlags() {
  PRUint32 flags = 0;
  if (mSyncDecodeImages) {
    flags |= nsCSSRendering::PAINTBG_SYNC_DECODE_IMAGES;
  }
  if (mIsPaintingToWindow) {
    flags |= nsCSSRendering::PAINTBG_TO_WINDOW;
  }
  return flags;
}

static PRUint64 RegionArea(const nsRegion& aRegion)
{
  PRUint64 area = 0;
  nsRegionRectIterator iter(aRegion);
  const nsRect* r;
  while ((r = iter.Next()) != nsnull) {
    area += PRUint64(r->width)*r->height;
  }
  return area;
}

void
nsDisplayListBuilder::SubtractFromVisibleRegion(nsRegion* aVisibleRegion,
                                                const nsRegion& aRegion)
{
  if (aRegion.IsEmpty())
    return;

  nsRegion tmp;
  tmp.Sub(*aVisibleRegion, aRegion);
  // Don't let *aVisibleRegion get too complex, but don't let it fluff out
  // to its bounds either, which can be very bad (see bug 516740).
  // Do let aVisibleRegion get more complex if by doing so we reduce its
  // area by at least half.
  if (GetAccurateVisibleRegions() || tmp.GetNumRects() <= 15 ||
      RegionArea(tmp) <= RegionArea(*aVisibleRegion)/2) {
    *aVisibleRegion = tmp;
  }
}

nsCaret *
nsDisplayListBuilder::GetCaret() {
  nsRefPtr<nsCaret> caret = CurrentPresShellState()->mPresShell->GetCaret();
  return caret;
}

void
nsDisplayListBuilder::EnterPresShell(nsIFrame* aReferenceFrame,
                                     const nsRect& aDirtyRect) {
  PresShellState* state = mPresShellStates.AppendElement();
  if (!state)
    return;
  state->mPresShell = aReferenceFrame->PresContext()->PresShell();
  state->mCaretFrame = nsnull;
  state->mFirstFrameMarkedForDisplay = mFramesMarkedForDisplay.Length();

  state->mPresShell->UpdateCanvasBackground();

  if (mIsPaintingToWindow) {
    mReferenceFrame->AddPaintedPresShell(state->mPresShell);
    
    state->mPresShell->IncrementPaintCount();
  }

  PRBool buildCaret = mBuildCaret;
  if (mIgnoreSuppression || !state->mPresShell->IsPaintingSuppressed()) {
    if (state->mPresShell->IsPaintingSuppressed()) {
      mHadToIgnoreSuppression = PR_TRUE;
    }
    state->mIsBackgroundOnly = PR_FALSE;
  } else {
    state->mIsBackgroundOnly = PR_TRUE;
    buildCaret = PR_FALSE;
  }

  if (!buildCaret)
    return;

  nsRefPtr<nsCaret> caret = state->mPresShell->GetCaret();
  state->mCaretFrame = caret->GetCaretFrame();

  if (state->mCaretFrame) {
    // Check if the dirty rect intersects with the caret's dirty rect.
    nsRect caretRect =
      caret->GetCaretRect() + state->mCaretFrame->GetOffsetTo(aReferenceFrame);
    if (caretRect.Intersects(aDirtyRect)) {
      // Okay, our rects intersect, let's mark the frame and all of its ancestors.
      mFramesMarkedForDisplay.AppendElement(state->mCaretFrame);
      MarkFrameForDisplay(state->mCaretFrame, nsnull);
    }
  }
}

void
nsDisplayListBuilder::LeavePresShell(nsIFrame* aReferenceFrame,
                                     const nsRect& aDirtyRect) {
  if (CurrentPresShellState()->mPresShell != aReferenceFrame->PresContext()->PresShell()) {
    // Must have not allocated a state for this presshell, presumably due
    // to OOM.
    return;
  }

  // Unmark and pop off the frames marked for display in this pres shell.
  PRUint32 firstFrameForShell = CurrentPresShellState()->mFirstFrameMarkedForDisplay;
  for (PRUint32 i = firstFrameForShell;
       i < mFramesMarkedForDisplay.Length(); ++i) {
    UnmarkFrameForDisplay(mFramesMarkedForDisplay[i]);
  }
  mFramesMarkedForDisplay.SetLength(firstFrameForShell);
  mPresShellStates.SetLength(mPresShellStates.Length() - 1);
}

void
nsDisplayListBuilder::MarkFramesForDisplayList(nsIFrame* aDirtyFrame,
                                               const nsFrameList& aFrames,
                                               const nsRect& aDirtyRect) {
  for (nsFrameList::Enumerator e(aFrames); !e.AtEnd(); e.Next()) {
    mFramesMarkedForDisplay.AppendElement(e.get());
    MarkOutOfFlowFrameForDisplay(aDirtyFrame, e.get(), aDirtyRect);
  }
}

void*
nsDisplayListBuilder::Allocate(size_t aSize) {
  void *tmp;
  PL_ARENA_ALLOCATE(tmp, &mPool, aSize);
  return tmp;
}

void nsDisplayListSet::MoveTo(const nsDisplayListSet& aDestination) const
{
  aDestination.BorderBackground()->AppendToTop(BorderBackground());
  aDestination.BlockBorderBackgrounds()->AppendToTop(BlockBorderBackgrounds());
  aDestination.Floats()->AppendToTop(Floats());
  aDestination.Content()->AppendToTop(Content());
  aDestination.PositionedDescendants()->AppendToTop(PositionedDescendants());
  aDestination.Outlines()->AppendToTop(Outlines());
}

void
nsDisplayList::FlattenTo(nsTArray<nsDisplayItem*>* aElements) {
  nsDisplayItem* item;
  while ((item = RemoveBottom()) != nsnull) {
    if (item->GetType() == nsDisplayItem::TYPE_WRAP_LIST) {
      item->GetList()->FlattenTo(aElements);
      item->~nsDisplayItem();
    } else {
      aElements->AppendElement(item);
    }
  }
}

nsRect
nsDisplayList::GetBounds(nsDisplayListBuilder* aBuilder) const {
  nsRect bounds;
  for (nsDisplayItem* i = GetBottom(); i != nsnull; i = i->GetAbove()) {
    bounds.UnionRect(bounds, i->GetBounds(aBuilder));
  }
  return bounds;
}

PRBool
nsDisplayList::ComputeVisibilityForRoot(nsDisplayListBuilder* aBuilder,
                                        nsRegion* aVisibleRegion) {
  nsRegion r;
  r.And(*aVisibleRegion, GetBounds(aBuilder));
  return ComputeVisibilityForSublist(aBuilder, aVisibleRegion, r.GetBounds(), r.GetBounds());
}

static nsRegion
TreatAsOpaque(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder,
              PRBool* aTransparentBackground)
{
  nsRegion opaque = aItem->GetOpaqueRegion(aBuilder, aTransparentBackground);
  if (aBuilder->IsForPluginGeometry()) {
    // Treat all chrome items as opaque, unless their frames are opacity:0.
    // Since opacity:0 frames generate an nsDisplayOpacity, that item will
    // not be treated as opaque here, so opacity:0 chrome content will be
    // effectively ignored, as it should be.
    nsIFrame* f = aItem->GetUnderlyingFrame();
    if (f && f->PresContext()->IsChrome() && f->GetStyleDisplay()->mOpacity != 0.0) {
      opaque = aItem->GetBounds(aBuilder);
    }
  }
  return opaque;
}

static nsRect GetDisplayPortBounds(nsDisplayListBuilder* aBuilder,
                                   nsDisplayItem* aItem,
                                   PRBool aIgnoreTransform)
{
  nsIFrame* frame = aItem->GetUnderlyingFrame();
  const nsRect* displayport = aBuilder->GetDisplayPort();

  if (aIgnoreTransform) {
    return *displayport;
  }

  return nsLayoutUtils::TransformRectToBoundsInAncestor(
           frame,
           nsRect(0, 0, displayport->width, displayport->height),
           aBuilder->ReferenceFrame());
}

PRBool
nsDisplayList::ComputeVisibilityForSublist(nsDisplayListBuilder* aBuilder,
                                           nsRegion* aVisibleRegion,
                                           const nsRect& aListVisibleBounds,
                                           const nsRect& aAllowVisibleRegionExpansion) {
#ifdef DEBUG
  nsRegion r;
  r.And(*aVisibleRegion, GetBounds(aBuilder));
  NS_ASSERTION(r.GetBounds().IsEqualInterior(aListVisibleBounds),
               "bad aListVisibleBounds");
#endif
  mVisibleRect = aListVisibleBounds;
  PRBool anyVisible = PR_FALSE;

  nsAutoTArray<nsDisplayItem*, 512> elements;
  FlattenTo(&elements);

  PRBool forceTransparentSurface = PR_FALSE;

  for (PRInt32 i = elements.Length() - 1; i >= 0; --i) {
    nsDisplayItem* item = elements[i];
    nsDisplayItem* belowItem = i < 1 ? nsnull : elements[i - 1];

    if (belowItem && item->TryMerge(aBuilder, belowItem)) {
      belowItem->~nsDisplayItem();
      elements.ReplaceElementsAt(i - 1, 1, item);
      continue;
    }

    nsDisplayList* list = item->GetList();
    if (list && item->ShouldFlattenAway(aBuilder)) {
      // The elements on the list >= i no longer serve any use.
      elements.SetLength(i);
      list->FlattenTo(&elements);
      i = elements.Length();
      item->~nsDisplayItem();
      continue;
    }

    nsRect bounds = item->GetBounds(aBuilder);

    nsRegion itemVisible;
    PRBool isFixedBackground;
    if (ForceVisiblityForFixedItem(aBuilder, item, &isFixedBackground)) {
      itemVisible.And(GetDisplayPortBounds(aBuilder, item, isFixedBackground), bounds);
    } else {
      itemVisible.And(*aVisibleRegion, bounds);
    }
    item->mVisibleRect = itemVisible.GetBounds();

    if (item->ComputeVisibility(aBuilder, aVisibleRegion, aAllowVisibleRegionExpansion)) {
      anyVisible = PR_TRUE;
      PRBool transparentBackground = PR_FALSE;
      nsRegion opaque = TreatAsOpaque(item, aBuilder, &transparentBackground);
      // Subtract opaque item from the visible region
      aBuilder->SubtractFromVisibleRegion(aVisibleRegion, opaque);
      forceTransparentSurface = forceTransparentSurface || transparentBackground;
    }
    AppendToBottom(item);
  }

  mIsOpaque = !aVisibleRegion->Intersects(mVisibleRect);
  mForceTransparentSurface = forceTransparentSurface;
#ifdef DEBUG
  mDidComputeVisibility = PR_TRUE;
#endif
  return anyVisible;
}

void nsDisplayList::PaintRoot(nsDisplayListBuilder* aBuilder,
                              nsRenderingContext* aCtx,
                              PRUint32 aFlags) const {
  PaintForFrame(aBuilder, aCtx, aBuilder->ReferenceFrame(), aFlags);
}

/**
 * We paint by executing a layer manager transaction, constructing a
 * single layer representing the display list, and then making it the
 * root of the layer manager, drawing into the ThebesLayers.
 */
void nsDisplayList::PaintForFrame(nsDisplayListBuilder* aBuilder,
                                  nsRenderingContext* aCtx,
                                  nsIFrame* aForFrame,
                                  PRUint32 aFlags) const {
  NS_ASSERTION(mDidComputeVisibility,
               "Must call ComputeVisibility before calling Paint");

  nsRefPtr<LayerManager> layerManager;
  bool allowRetaining = false;
  bool doBeginTransaction = true;
  if (aFlags & PAINT_USE_WIDGET_LAYERS) {
    nsIFrame* referenceFrame = aBuilder->ReferenceFrame();
    NS_ASSERTION(referenceFrame == nsLayoutUtils::GetDisplayRootFrame(referenceFrame),
                 "Reference frame must be a display root for us to use the layer manager");
    nsIWidget* window = referenceFrame->GetNearestWidget();
    if (window) {
      layerManager = window->GetLayerManager(&allowRetaining);
      if (layerManager) {
        doBeginTransaction = !(aFlags & PAINT_EXISTING_TRANSACTION);
      }
    }
  }
  if (!layerManager) {
    if (!aCtx) {
      NS_WARNING("Nowhere to paint into");
      return;
    }
    layerManager = new BasicLayerManager();
  }

  if (aFlags & PAINT_FLUSH_LAYERS) {
    FrameLayerBuilder::InvalidateAllLayers(layerManager);
  }

  if (doBeginTransaction) {
    if (aCtx) {
      layerManager->BeginTransactionWithTarget(aCtx->ThebesContext());
    } else {
      layerManager->BeginTransaction();
    }
  }
  if (allowRetaining) {
    aBuilder->LayerBuilder()->DidBeginRetainedLayerTransaction(layerManager);
  }

  nsPresContext* presContext = aForFrame->PresContext();
  nsIPresShell* presShell = presContext->GetPresShell();

  nsDisplayItem::ContainerParameters containerParameters
    (presShell->GetXResolution(), presShell->GetYResolution());
  nsRefPtr<ContainerLayer> root = aBuilder->LayerBuilder()->
    BuildContainerLayerFor(aBuilder, layerManager, aForFrame, nsnull, *this,
                           containerParameters, nsnull);
  if (!root)
    return;
  // Root is being scaled up by the X/Y resolution. Scale it back down.
  gfx3DMatrix rootTransform = root->GetTransform()*
    gfx3DMatrix::Scale(1.0f/containerParameters.mXScale,
                       1.0f/containerParameters.mYScale, 1.0f);
  root->SetTransform(rootTransform);

  ViewID id = presContext->IsRootContentDocument() ? FrameMetrics::ROOT_SCROLL_ID
                                                   : FrameMetrics::NULL_SCROLL_ID;

  nsIFrame* rootScrollFrame = presShell->GetRootScrollFrame();
  nsRect displayport;
  bool usingDisplayport = false;
  if (rootScrollFrame) {
    nsIContent* content = rootScrollFrame->GetContent();
    if (content) {
      usingDisplayport = nsLayoutUtils::GetDisplayPort(content, &displayport);
    }
  }
  RecordFrameMetrics(aForFrame, rootScrollFrame,
                     root, mVisibleRect, mVisibleRect,
                     (usingDisplayport ? &displayport : nsnull), id,
                     containerParameters);

  layerManager->SetRoot(root);
  aBuilder->LayerBuilder()->WillEndTransaction(layerManager);
  layerManager->EndTransaction(FrameLayerBuilder::DrawThebesLayer,
                               aBuilder);
  aBuilder->LayerBuilder()->DidEndTransaction(layerManager);

  if (aFlags & PAINT_FLUSH_LAYERS) {
    FrameLayerBuilder::InvalidateAllLayers(layerManager);
  }

  nsCSSRendering::DidPaint();
}

PRUint32 nsDisplayList::Count() const {
  PRUint32 count = 0;
  for (nsDisplayItem* i = GetBottom(); i; i = i->GetAbove()) {
    ++count;
  }
  return count;
}

nsDisplayItem* nsDisplayList::RemoveBottom() {
  nsDisplayItem* item = mSentinel.mAbove;
  if (!item)
    return nsnull;
  mSentinel.mAbove = item->mAbove;
  if (item == mTop) {
    // must have been the only item
    mTop = &mSentinel;
  }
  item->mAbove = nsnull;
  return item;
}

void nsDisplayList::DeleteAll() {
  nsDisplayItem* item;
  while ((item = RemoveBottom()) != nsnull) {
    item->~nsDisplayItem();
  }
}

static PRBool
GetMouseThrough(const nsIFrame* aFrame)
{
  if (!aFrame->IsBoxFrame())
    return PR_FALSE;

  const nsIFrame* frame = aFrame;
  while (frame) {
    if (frame->GetStateBits() & NS_FRAME_MOUSE_THROUGH_ALWAYS) {
      return PR_TRUE;
    } else if (frame->GetStateBits() & NS_FRAME_MOUSE_THROUGH_NEVER) {
      return PR_FALSE;
    }
    frame = frame->GetParentBox();
  }
  return PR_FALSE;
}

void nsDisplayList::HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                            nsDisplayItem::HitTestState* aState,
                            nsTArray<nsIFrame*> *aOutFrames) const {
  PRInt32 itemBufferStart = aState->mItemBuffer.Length();
  nsDisplayItem* item;
  for (item = GetBottom(); item; item = item->GetAbove()) {
    aState->mItemBuffer.AppendElement(item);
  }
  for (PRInt32 i = aState->mItemBuffer.Length() - 1; i >= itemBufferStart; --i) {
    // Pop element off the end of the buffer. We want to shorten the buffer
    // so that recursive calls to HitTest have more buffer space.
    item = aState->mItemBuffer[i];
    aState->mItemBuffer.SetLength(i);

    if (aRect.Intersects(item->GetBounds(aBuilder))) {
      nsTArray<nsIFrame*> outFrames;
      item->HitTest(aBuilder, aRect, aState, &outFrames);

      for (PRUint32 j = 0; j < outFrames.Length(); j++) {
        nsIFrame *f = outFrames.ElementAt(j);
        // Handle the XUL 'mousethrough' feature and 'pointer-events'.
        if (!GetMouseThrough(f) &&
            f->GetStyleVisibility()->mPointerEvents != NS_STYLE_POINTER_EVENTS_NONE) {
          aOutFrames->AppendElement(f);
        }
      }

    }
  }
  NS_ASSERTION(aState->mItemBuffer.Length() == PRUint32(itemBufferStart),
               "How did we forget to pop some elements?");
}

static void Sort(nsDisplayList* aList, PRInt32 aCount, nsDisplayList::SortLEQ aCmp,
                 void* aClosure) {
  if (aCount < 2)
    return;

  nsDisplayList list1;
  nsDisplayList list2;
  int i;
  PRInt32 half = aCount/2;
  PRBool sorted = PR_TRUE;
  nsDisplayItem* prev = nsnull;
  for (i = 0; i < aCount; ++i) {
    nsDisplayItem* item = aList->RemoveBottom();
    (i < half ? &list1 : &list2)->AppendToTop(item);
    if (sorted && prev && !aCmp(prev, item, aClosure)) {
      sorted = PR_FALSE;
    }
    prev = item;
  }
  if (sorted) {
    aList->AppendToTop(&list1);
    aList->AppendToTop(&list2);
    return;
  }
  
  Sort(&list1, half, aCmp, aClosure);
  Sort(&list2, aCount - half, aCmp, aClosure);

  for (i = 0; i < aCount; ++i) {
    if (list1.GetBottom() &&
        (!list2.GetBottom() ||
         aCmp(list1.GetBottom(), list2.GetBottom(), aClosure))) {
      aList->AppendToTop(list1.RemoveBottom());
    } else {
      aList->AppendToTop(list2.RemoveBottom());
    }
  }
}

static PRBool IsContentLEQ(nsDisplayItem* aItem1, nsDisplayItem* aItem2,
                           void* aClosure) {
  // These GetUnderlyingFrame calls return non-null because we're only used
  // in sorting
  return nsLayoutUtils::CompareTreePosition(
      aItem1->GetUnderlyingFrame()->GetContent(),
      aItem2->GetUnderlyingFrame()->GetContent(),
      static_cast<nsIContent*>(aClosure)) <= 0;
}

static PRBool IsZOrderLEQ(nsDisplayItem* aItem1, nsDisplayItem* aItem2,
                          void* aClosure) {
  // These GetUnderlyingFrame calls return non-null because we're only used
  // in sorting.  Note that we can't just take the difference of the two
  // z-indices here, because that might overflow a 32-bit int.
  PRInt32 index1 = nsLayoutUtils::GetZIndex(aItem1->GetUnderlyingFrame());
  PRInt32 index2 = nsLayoutUtils::GetZIndex(aItem2->GetUnderlyingFrame());
  if (index1 == index2)
    return IsContentLEQ(aItem1, aItem2, aClosure);
  return index1 < index2;
}

void nsDisplayList::ExplodeAnonymousChildLists(nsDisplayListBuilder* aBuilder) {
  // See if there's anything to do
  PRBool anyAnonymousItems = PR_FALSE;
  nsDisplayItem* i;
  for (i = GetBottom(); i != nsnull; i = i->GetAbove()) {
    if (!i->GetUnderlyingFrame()) {
      anyAnonymousItems = PR_TRUE;
      break;
    }
  }
  if (!anyAnonymousItems)
    return;

  nsDisplayList tmp;
  while ((i = RemoveBottom()) != nsnull) {
    if (i->GetUnderlyingFrame()) {
      tmp.AppendToTop(i);
    } else {
      nsDisplayList* list = i->GetList();
      NS_ASSERTION(list, "leaf items can't be anonymous");
      list->ExplodeAnonymousChildLists(aBuilder);
      nsDisplayItem* j;
      while ((j = list->RemoveBottom()) != nsnull) {
        tmp.AppendToTop(static_cast<nsDisplayWrapList*>(i)->
            WrapWithClone(aBuilder, j));
      }
      i->~nsDisplayItem();
    }
  }
  
  AppendToTop(&tmp);
}

void nsDisplayList::SortByZOrder(nsDisplayListBuilder* aBuilder,
                                 nsIContent* aCommonAncestor) {
  Sort(aBuilder, IsZOrderLEQ, aCommonAncestor);
}

void nsDisplayList::SortByContentOrder(nsDisplayListBuilder* aBuilder,
                                       nsIContent* aCommonAncestor) {
  Sort(aBuilder, IsContentLEQ, aCommonAncestor);
}

void nsDisplayList::Sort(nsDisplayListBuilder* aBuilder,
                         SortLEQ aCmp, void* aClosure) {
  ExplodeAnonymousChildLists(aBuilder);
  ::Sort(this, Count(), aCmp, aClosure);
}

PRBool nsDisplayItem::RecomputeVisibility(nsDisplayListBuilder* aBuilder,
                                          nsRegion* aVisibleRegion) {
  nsRect bounds = GetBounds(aBuilder);

  nsRegion itemVisible;
  PRBool isFixedBackground;
  if (ForceVisiblityForFixedItem(aBuilder, this, &isFixedBackground)) {
    itemVisible.And(GetDisplayPortBounds(aBuilder, this, isFixedBackground), bounds);
  } else {
    itemVisible.And(*aVisibleRegion, bounds);
  }
  mVisibleRect = itemVisible.GetBounds();

  // When we recompute visibility within layers we don't need to
  // expand the visible region for content behind plugins (the plugin
  // is not in the layer).
  if (!ComputeVisibility(aBuilder, aVisibleRegion, nsRect()))
    return PR_FALSE;

  PRBool forceTransparentBackground;
  nsRegion opaque = TreatAsOpaque(this, aBuilder, &forceTransparentBackground);
  aBuilder->SubtractFromVisibleRegion(aVisibleRegion, opaque);
  return PR_TRUE;
}

// Note that even if the rectangle we draw and snap is smaller than aRect,
// it's OK to call this to get a bounding rect for what we'll draw, because
// snapping a rectangle which is contained in R always gives you a
// rectangle which is contained in the snapped R.
static nsRect
SnapBounds(PRBool aSnappingEnabled, nsPresContext* aPresContext,
           const nsRect& aRect) {
  nsRect r = aRect;
  if (aSnappingEnabled) {
    nscoord appUnitsPerDevPixel = aPresContext->AppUnitsPerDevPixel();
    r = r.ToNearestPixels(appUnitsPerDevPixel).ToAppUnits(appUnitsPerDevPixel);
  }
  return r;
}

nsRect
nsDisplaySolidColor::GetBounds(nsDisplayListBuilder* aBuilder)
{
  nsPresContext* presContext = mFrame->PresContext();
  return SnapBounds(mSnappingEnabled, presContext, mBounds);
}

void
nsDisplaySolidColor::Paint(nsDisplayListBuilder* aBuilder,
                           nsRenderingContext* aCtx)
{
  aCtx->SetColor(mColor);
  aCtx->FillRect(mVisibleRect);
}

static void
RegisterThemeGeometry(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
{
  nsIFrame* displayRoot = nsLayoutUtils::GetDisplayRootFrame(aFrame);

  for (nsIFrame* f = aFrame; f; f = f->GetParent()) {
    // Bail out if we're in a transformed subtree
    if (f->IsTransformed())
      return;
    // Bail out if we're not in the displayRoot's document
    if (!f->GetParent() && f != displayRoot)
      return;
  }

  nsRect borderBox(aFrame->GetOffsetTo(displayRoot), aFrame->GetSize());
  aBuilder->RegisterThemeGeometry(aFrame->GetStyleDisplay()->mAppearance,
      borderBox.ToNearestPixels(aFrame->PresContext()->AppUnitsPerDevPixel()));
}

nsDisplayBackground::nsDisplayBackground(nsDisplayListBuilder* aBuilder,
                                         nsIFrame* aFrame)
  : nsDisplayItem(aBuilder, aFrame),
    mSnappingEnabled(aBuilder->IsSnappingEnabled() && !aBuilder->IsInTransform())
{
  MOZ_COUNT_CTOR(nsDisplayBackground);
  const nsStyleDisplay* disp = mFrame->GetStyleDisplay();
  mIsThemed = mFrame->IsThemed(disp, &mThemeTransparency);

  if (mIsThemed) {
    // Perform necessary RegisterThemeGeometry
    if (disp->mAppearance == NS_THEME_MOZ_MAC_UNIFIED_TOOLBAR ||
        disp->mAppearance == NS_THEME_TOOLBAR) {
      RegisterThemeGeometry(aBuilder, aFrame);
    }
  } else {
    // Set HasFixedItems if we construct a background-attachment:fixed item
    nsPresContext* presContext = mFrame->PresContext();
    nsStyleContext* bgSC;
    PRBool hasBG = nsCSSRendering::FindBackground(presContext, mFrame, &bgSC);
    if (hasBG && bgSC->GetStyleBackground()->HasFixedBackground()) {
      aBuilder->SetHasFixedItems();
    }
  }
}

// Helper for RoundedRectIntersectsRect.
static PRBool
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
    return PR_TRUE;

  // Convert coordinates to a unit circle with (0,0) as the center of
  // curvature, and see if we're inside the circle or outside.
  float scaledX = float(aXRadius - aXOffset) / float(aXRadius);
  float scaledY = float(aYRadius - aYOffset) / float(aYRadius);
  return scaledX * scaledX + scaledY * scaledY < 1.0f;
}


/**
 * Return whether any part of aTestRect is inside of the rounded
 * rectangle formed by aBounds and aRadii (which are indexed by the
 * NS_CORNER_* constants in nsStyleConsts.h).
 *
 * See also RoundedRectContainsRect.
 */
static PRBool
RoundedRectIntersectsRect(const nsRect& aRoundedRect, nscoord aRadii[8],
                          const nsRect& aTestRect)
{
  NS_ABORT_IF_FALSE(aTestRect.Intersects(aRoundedRect),
                    "we should already have tested basic rect intersection");

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

// Check that the rounded border of aFrame, added to aToReferenceFrame,
// intersects aRect.  Assumes that the unrounded border has already
// been checked for intersection.
static PRBool
RoundedBorderIntersectsRect(nsIFrame* aFrame,
                            const nsPoint& aFrameToReferenceFrame,
                            const nsRect& aTestRect)
{
  if (!nsRect(aFrameToReferenceFrame, aFrame->GetSize()).Intersects(aTestRect))
    return PR_FALSE;

  nscoord radii[8];
  return !aFrame->GetBorderRadii(radii) ||
         RoundedRectIntersectsRect(nsRect(aFrameToReferenceFrame,
                                          aFrame->GetSize()),
                                   radii, aTestRect);
}

// Returns TRUE if aContainedRect is guaranteed to be contained in
// the rounded rect defined by aRoundedRect and aRadii. Complex cases are
// handled conservatively by returning FALSE in some situations where
// a more thorough analysis could return TRUE.
//
// See also RoundedRectIntersectsRect.
static PRBool RoundedRectContainsRect(const nsRect& aRoundedRect,
                                      const nscoord aRadii[8],
                                      const nsRect& aContainedRect) {
  nsRegion rgn = nsLayoutUtils::RoundedRectIntersectRect(aRoundedRect, aRadii, aContainedRect);
  return rgn.Contains(aContainedRect);
}

void
nsDisplayBackground::HitTest(nsDisplayListBuilder* aBuilder,
                             const nsRect& aRect,
                             HitTestState* aState,
                             nsTArray<nsIFrame*> *aOutFrames)
{
  if (mIsThemed) {
    // For theme backgrounds, assume that any point in our border rect is a hit.
    if (!nsRect(ToReferenceFrame(), mFrame->GetSize()).Intersects(aRect))
      return;
  } else {
    if (!RoundedBorderIntersectsRect(mFrame, ToReferenceFrame(), aRect)) {
      // aRect doesn't intersect our border-radius curve.
      return;
    }
  }

  aOutFrames->AppendElement(mFrame);
}

PRBool
nsDisplayBackground::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                       nsRegion* aVisibleRegion,
                                       const nsRect& aAllowVisibleRegionExpansion)
{
  if (!nsDisplayItem::ComputeVisibility(aBuilder, aVisibleRegion,
                                        aAllowVisibleRegionExpansion)) {
    return PR_FALSE;
  }

  // Return false if the background was propagated away from this
  // frame. We don't want this display item to show up and confuse
  // anything.
  nsStyleContext* bgSC;
  return mIsThemed ||
    nsCSSRendering::FindBackground(mFrame->PresContext(), mFrame, &bgSC);
}

nsRegion
nsDisplayBackground::GetInsideClipRegion(nsPresContext* aPresContext,
                                         PRUint8 aClip, const nsRect& aRect)
{
  nsRegion result;
  if (aRect.IsEmpty())
    return result;

  nscoord radii[8];
  nsRect clipRect;
  PRBool haveRadii;
  switch (aClip) {
  case NS_STYLE_BG_CLIP_BORDER:
    haveRadii = mFrame->GetBorderRadii(radii);
    clipRect = nsRect(ToReferenceFrame(), mFrame->GetSize());
    break;
  case NS_STYLE_BG_CLIP_PADDING:
    haveRadii = mFrame->GetPaddingBoxBorderRadii(radii);
    clipRect = mFrame->GetPaddingRect() - mFrame->GetPosition() + ToReferenceFrame();
    break;
  case NS_STYLE_BG_CLIP_CONTENT:
    haveRadii = mFrame->GetContentBoxBorderRadii(radii);
    clipRect = mFrame->GetContentRect() - mFrame->GetPosition() + ToReferenceFrame();
    break;
  default:
    NS_NOTREACHED("Unknown clip type");
    return result;
  }

  nsRect inputRect = SnapBounds(mSnappingEnabled, aPresContext, aRect);
  clipRect = SnapBounds(mSnappingEnabled, aPresContext, clipRect);

  if (haveRadii) {
    result = nsLayoutUtils::RoundedRectIntersectRect(clipRect, radii, inputRect);
  } else {
    nsRect r;
    r.IntersectRect(clipRect, inputRect);
    result = r;
  }
  return result;
}

nsRegion
nsDisplayBackground::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                     PRBool* aForceTransparentSurface) {
  nsRegion result;
  if (aForceTransparentSurface) {
    *aForceTransparentSurface = PR_FALSE;
  }
  // theme background overrides any other background
  if (mIsThemed) {
    if (aForceTransparentSurface) {
      const nsStyleDisplay* disp = mFrame->GetStyleDisplay();
      *aForceTransparentSurface = disp->mAppearance == NS_THEME_WIN_BORDERLESS_GLASS ||
                                  disp->mAppearance == NS_THEME_WIN_GLASS;
    }
    if (mThemeTransparency == nsITheme::eOpaque) {
      result = GetBounds(aBuilder);
    }
    return result;
  }

  nsStyleContext* bgSC;
  nsPresContext* presContext = mFrame->PresContext();
  if (!nsCSSRendering::FindBackground(mFrame->PresContext(), mFrame, &bgSC))
    return result;
  const nsStyleBackground* bg = bgSC->GetStyleBackground();
  const nsStyleBackground::Layer& bottomLayer = bg->BottomLayer();

  nsRect borderBox = nsRect(ToReferenceFrame(), mFrame->GetSize());
  if (NS_GET_A(bg->mBackgroundColor) == 255 &&
      !nsCSSRendering::IsCanvasFrame(mFrame)) {
    result = GetInsideClipRegion(presContext, bottomLayer.mClip, borderBox);
  }

  // For policies other than EACH_BOX, don't try to optimize here, since
  // this could easily lead to O(N^2) behavior inside InlineBackgroundData,
  // which expects frames to be sent to it in content order, not reverse
  // content order which we'll produce here.
  // Of course, if there's only one frame in the flow, it doesn't matter.
  if (bg->mBackgroundInlinePolicy == NS_STYLE_BG_INLINE_POLICY_EACH_BOX ||
      (!mFrame->GetPrevContinuation() && !mFrame->GetNextContinuation())) {
    NS_FOR_VISIBLE_BACKGROUND_LAYERS_BACK_TO_FRONT(i, bg) {
      const nsStyleBackground::Layer& layer = bg->mLayers[i];
      if (layer.mImage.IsOpaque()) {
        nsRect r = nsCSSRendering::GetBackgroundLayerRect(presContext, mFrame,
            borderBox, *bg, layer);
        result.Or(result, GetInsideClipRegion(presContext, layer.mClip, r));
      }
    }
  }

  return result;
}

PRBool
nsDisplayBackground::IsUniform(nsDisplayListBuilder* aBuilder, nscolor* aColor) {
  // theme background overrides any other background
  if (mIsThemed) {
    const nsStyleDisplay* disp = mFrame->GetStyleDisplay();
    if (disp->mAppearance == NS_THEME_WIN_BORDERLESS_GLASS ||
        disp->mAppearance == NS_THEME_WIN_GLASS) {
      *aColor = NS_RGBA(0,0,0,0);
      return PR_TRUE;
    }
    return PR_FALSE;
  }

  nsStyleContext *bgSC;
  PRBool hasBG =
    nsCSSRendering::FindBackground(mFrame->PresContext(), mFrame, &bgSC);
  if (!hasBG) {
    *aColor = NS_RGBA(0,0,0,0);
    return PR_TRUE;
  }
  const nsStyleBackground* bg = bgSC->GetStyleBackground();
  if (bg->BottomLayer().mImage.IsEmpty() &&
      bg->mImageCount == 1 &&
      !nsLayoutUtils::HasNonZeroCorner(mFrame->GetStyleBorder()->mBorderRadius) &&
      bg->BottomLayer().mClip == NS_STYLE_BG_CLIP_BORDER) {
    // Canvas frames don't actually render their background color, since that
    // gets propagated to the solid color of the viewport
    // (see nsCSSRendering::PaintBackgroundWithSC)
    *aColor = nsCSSRendering::IsCanvasFrame(mFrame) ? NS_RGBA(0,0,0,0)
        : bg->mBackgroundColor;
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRBool
nsDisplayBackground::IsVaryingRelativeToMovingFrame(nsDisplayListBuilder* aBuilder,
                                                    nsIFrame* aFrame)
{
  // theme background overrides any other background and is never fixed
  if (mIsThemed)
    return PR_FALSE;

  nsPresContext* presContext = mFrame->PresContext();
  nsStyleContext *bgSC;
  PRBool hasBG =
    nsCSSRendering::FindBackground(presContext, mFrame, &bgSC);
  if (!hasBG)
    return PR_FALSE;
  const nsStyleBackground* bg = bgSC->GetStyleBackground();
  if (!bg->HasFixedBackground())
    return PR_FALSE;

  // If aFrame is mFrame or an ancestor in this document, and aFrame is
  // not the viewport frame, then moving aFrame will move mFrame
  // relative to the viewport, so our fixed-pos background will change.
  return aFrame->GetParent() &&
    (aFrame == mFrame ||
     nsLayoutUtils::IsProperAncestorFrame(aFrame, mFrame));
}

PRBool
nsDisplayBackground::ShouldFixToViewport(nsDisplayListBuilder* aBuilder)
{
  if (mIsThemed)
    return PR_FALSE;

  nsPresContext* presContext = mFrame->PresContext();
  nsStyleContext* bgSC;
  PRBool hasBG =
    nsCSSRendering::FindBackground(presContext, mFrame, &bgSC);
  if (!hasBG)
    return PR_FALSE;

  const nsStyleBackground* bg = bgSC->GetStyleBackground();
  if (!bg->HasFixedBackground())
    return PR_FALSE;

  NS_FOR_VISIBLE_BACKGROUND_LAYERS_BACK_TO_FRONT(i, bg) {
    const nsStyleBackground::Layer& layer = bg->mLayers[i];
    if (layer.mAttachment != NS_STYLE_BG_ATTACHMENT_FIXED &&
        !layer.mImage.IsEmpty()) {
      return PR_FALSE;
    }
    if (layer.mClip != NS_STYLE_BG_CLIP_BORDER)
      return PR_FALSE;
  }

  if (nsLayoutUtils::HasNonZeroCorner(mFrame->GetStyleBorder()->mBorderRadius))
    return PR_FALSE;

  nsRect bounds = GetBounds(aBuilder);
  nsIFrame* rootScrollFrame = presContext->PresShell()->GetRootScrollFrame();
  if (!rootScrollFrame)
    return PR_FALSE;
  nsIScrollableFrame* scrollable = do_QueryFrame(rootScrollFrame);
  nsRect scrollport = scrollable->GetScrollPortRect() +
    aBuilder->ToReferenceFrame(rootScrollFrame);
  return bounds.Contains(scrollport);
}

void
nsDisplayBackground::Paint(nsDisplayListBuilder* aBuilder,
                           nsRenderingContext* aCtx) {
  nsPoint offset = ToReferenceFrame();
  PRUint32 flags = aBuilder->GetBackgroundPaintFlags();
  nsDisplayItem* nextItem = GetAbove();
  if (nextItem && nextItem->GetUnderlyingFrame() == mFrame &&
      nextItem->GetType() == TYPE_BORDER) {
    flags |= nsCSSRendering::PAINTBG_WILL_PAINT_BORDER;
  }
  nsCSSRendering::PaintBackground(mFrame->PresContext(), *aCtx, mFrame,
                                  mVisibleRect,
                                  nsRect(offset, mFrame->GetSize()),
                                  flags);
}

nsRect
nsDisplayBackground::GetBounds(nsDisplayListBuilder* aBuilder) {
  nsRect r(nsPoint(0,0), mFrame->GetSize());
  nsPresContext* presContext = mFrame->PresContext();

  if (mIsThemed) {
    presContext->GetTheme()->
        GetWidgetOverflow(presContext->DeviceContext(), mFrame,
                          mFrame->GetStyleDisplay()->mAppearance, &r);
  }

  return SnapBounds(mSnappingEnabled, presContext, r + ToReferenceFrame());
}

nsRect
nsDisplayOutline::GetBounds(nsDisplayListBuilder* aBuilder) {
  return mFrame->GetVisualOverflowRectRelativeToSelf() + ToReferenceFrame();
}

void
nsDisplayOutline::Paint(nsDisplayListBuilder* aBuilder,
                        nsRenderingContext* aCtx) {
  // TODO join outlines together
  nsPoint offset = ToReferenceFrame();
  nsCSSRendering::PaintOutline(mFrame->PresContext(), *aCtx, mFrame,
                               mVisibleRect,
                               nsRect(offset, mFrame->GetSize()),
                               mFrame->GetStyleContext());
}

PRBool
nsDisplayOutline::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                    nsRegion* aVisibleRegion,
                                    const nsRect& aAllowVisibleRegionExpansion) {
  if (!nsDisplayItem::ComputeVisibility(aBuilder, aVisibleRegion,
                                        aAllowVisibleRegionExpansion)) {
    return PR_FALSE;
  }

  const nsStyleOutline* outline = mFrame->GetStyleOutline();
  nsRect borderBox(ToReferenceFrame(), mFrame->GetSize());
  if (borderBox.Contains(aVisibleRegion->GetBounds()) &&
      !nsLayoutUtils::HasNonZeroCorner(outline->mOutlineRadius)) {
    if (outline->mOutlineOffset >= 0) {
      // the visible region is entirely inside the border-rect, and the outline
      // isn't rendered inside the border-rect, so the outline is not visible
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}

void
nsDisplayEventReceiver::HitTest(nsDisplayListBuilder* aBuilder,
                                const nsRect& aRect,
                                HitTestState* aState,
                                nsTArray<nsIFrame*> *aOutFrames)
{
  if (!RoundedBorderIntersectsRect(mFrame, ToReferenceFrame(), aRect)) {
    // aRect doesn't intersect our border-radius curve.
    return;
  }

  aOutFrames->AppendElement(mFrame);
}

void
nsDisplayCaret::Paint(nsDisplayListBuilder* aBuilder,
                      nsRenderingContext* aCtx) {
  // Note: Because we exist, we know that the caret is visible, so we don't
  // need to check for the caret's visibility.
  mCaret->PaintCaret(aBuilder, aCtx, mFrame, ToReferenceFrame());
}

PRBool
nsDisplayBorder::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion,
                                   const nsRect& aAllowVisibleRegionExpansion) {
  if (!nsDisplayItem::ComputeVisibility(aBuilder, aVisibleRegion,
                                        aAllowVisibleRegionExpansion)) {
    return PR_FALSE;
  }

  nsRect paddingRect = mFrame->GetPaddingRect() - mFrame->GetPosition() +
    ToReferenceFrame();
  const nsStyleBorder *styleBorder;
  if (paddingRect.Contains(aVisibleRegion->GetBounds()) &&
      !(styleBorder = mFrame->GetStyleBorder())->IsBorderImageLoaded() &&
      !nsLayoutUtils::HasNonZeroCorner(styleBorder->mBorderRadius)) {
    // the visible region is entirely inside the content rect, and no part
    // of the border is rendered inside the content rect, so we are not
    // visible
    // Skip this if there's a border-image (which draws a background
    // too) or if there is a border-radius (which makes the border draw
    // further in).
    return PR_FALSE;
  }

  return PR_TRUE;
}

void
nsDisplayBorder::Paint(nsDisplayListBuilder* aBuilder,
                       nsRenderingContext* aCtx) {
  nsPoint offset = ToReferenceFrame();
  nsCSSRendering::PaintBorder(mFrame->PresContext(), *aCtx, mFrame,
                              mVisibleRect,
                              nsRect(offset, mFrame->GetSize()),
                              mFrame->GetStyleContext(),
                              mFrame->GetSkipSides());
}

nsRect
nsDisplayBorder::GetBounds(nsDisplayListBuilder* aBuilder)
{
  return SnapBounds(mSnappingEnabled, mFrame->PresContext(),
                    nsRect(ToReferenceFrame(), mFrame->GetSize()));
}

// Given a region, compute a conservative approximation to it as a list
// of rectangles that aren't vertically adjacent (i.e., vertically
// adjacent or overlapping rectangles are combined).
// Right now this is only approximate, some vertically overlapping rectangles
// aren't guaranteed to be combined.
static void
ComputeDisjointRectangles(const nsRegion& aRegion,
                          nsTArray<nsRect>* aRects) {
  nscoord accumulationMargin = nsPresContext::CSSPixelsToAppUnits(25);
  nsRect accumulated;
  nsRegionRectIterator iter(aRegion);
  while (PR_TRUE) {
    const nsRect* r = iter.Next();
    if (r && !accumulated.IsEmpty() &&
        accumulated.YMost() >= r->y - accumulationMargin) {
      accumulated.UnionRect(accumulated, *r);
      continue;
    }

    if (!accumulated.IsEmpty()) {
      aRects->AppendElement(accumulated);
      accumulated.SetEmpty();
    }

    if (!r)
      break;

    accumulated = *r;
  }
}

void
nsDisplayBoxShadowOuter::Paint(nsDisplayListBuilder* aBuilder,
                               nsRenderingContext* aCtx) {
  nsPoint offset = ToReferenceFrame();
  nsRect borderRect = nsRect(offset, mFrame->GetSize());
  nsPresContext* presContext = mFrame->PresContext();
  nsAutoTArray<nsRect,10> rects;
  ComputeDisjointRectangles(mVisibleRegion, &rects);

  for (PRUint32 i = 0; i < rects.Length(); ++i) {
    aCtx->PushState();
    aCtx->IntersectClip(rects[i]);
    nsCSSRendering::PaintBoxShadowOuter(presContext, *aCtx, mFrame,
                                        borderRect, rects[i]);
    aCtx->PopState();
  }
}

nsRect
nsDisplayBoxShadowOuter::GetBounds(nsDisplayListBuilder* aBuilder) {
  return mFrame->GetVisualOverflowRectRelativeToSelf() + ToReferenceFrame();
}

PRBool
nsDisplayBoxShadowOuter::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                           nsRegion* aVisibleRegion,
                                           const nsRect& aAllowVisibleRegionExpansion) {
  if (!nsDisplayItem::ComputeVisibility(aBuilder, aVisibleRegion,
                                        aAllowVisibleRegionExpansion)) {
    return PR_FALSE;
  }

  // Store the actual visible region
  mVisibleRegion.And(*aVisibleRegion, mVisibleRect);

  nsPoint origin = ToReferenceFrame();
  nsRect visibleBounds = aVisibleRegion->GetBounds();
  nsRect frameRect(origin, mFrame->GetSize());
  if (!frameRect.Contains(visibleBounds))
    return PR_TRUE;

  // the visible region is entirely inside the border-rect, and box shadows
  // never render within the border-rect (unless there's a border radius).
  nscoord twipsRadii[8];
  PRBool hasBorderRadii = mFrame->GetBorderRadii(twipsRadii);
  if (!hasBorderRadii)
    return PR_FALSE;

  return !RoundedRectContainsRect(frameRect, twipsRadii, visibleBounds);
}

void
nsDisplayBoxShadowInner::Paint(nsDisplayListBuilder* aBuilder,
                               nsRenderingContext* aCtx) {
  nsPoint offset = ToReferenceFrame();
  nsRect borderRect = nsRect(offset, mFrame->GetSize());
  nsPresContext* presContext = mFrame->PresContext();
  nsAutoTArray<nsRect,10> rects;
  ComputeDisjointRectangles(mVisibleRegion, &rects);

  for (PRUint32 i = 0; i < rects.Length(); ++i) {
    aCtx->PushState();
    aCtx->IntersectClip(rects[i]);
    nsCSSRendering::PaintBoxShadowInner(presContext, *aCtx, mFrame,
                                        borderRect, rects[i]);
    aCtx->PopState();
  }
}

PRBool
nsDisplayBoxShadowInner::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                           nsRegion* aVisibleRegion,
                                           const nsRect& aAllowVisibleRegionExpansion) {
  if (!nsDisplayItem::ComputeVisibility(aBuilder, aVisibleRegion,
                                        aAllowVisibleRegionExpansion)) {
    return PR_FALSE;
  }

  // Store the actual visible region
  mVisibleRegion.And(*aVisibleRegion, mVisibleRect);
  return PR_TRUE;
}

nsDisplayWrapList::nsDisplayWrapList(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame, nsDisplayList* aList)
  : nsDisplayItem(aBuilder, aFrame) {
  mList.AppendToTop(aList);
}

nsDisplayWrapList::nsDisplayWrapList(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame, nsDisplayItem* aItem)
  : nsDisplayItem(aBuilder, aFrame) {
  mList.AppendToTop(aItem);
}

nsDisplayWrapList::nsDisplayWrapList(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame)
  : nsDisplayItem(aBuilder, aFrame) {
}

nsDisplayWrapList::~nsDisplayWrapList() {
  mList.DeleteAll();
}

void
nsDisplayWrapList::HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                           HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) {
  mList.HitTest(aBuilder, aRect, aState, aOutFrames);
}

nsRect
nsDisplayWrapList::GetBounds(nsDisplayListBuilder* aBuilder) {
  return mList.GetBounds(aBuilder);
}

PRBool
nsDisplayWrapList::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                     nsRegion* aVisibleRegion,
                                     const nsRect& aAllowVisibleRegionExpansion) {
  return mList.ComputeVisibilityForSublist(aBuilder, aVisibleRegion,
                                           mVisibleRect,
                                           aAllowVisibleRegionExpansion);
}

nsRegion
nsDisplayWrapList::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   PRBool* aForceTransparentSurface) {
  if (aForceTransparentSurface) {
    *aForceTransparentSurface = PR_FALSE;
  }
  nsRegion result;
  if (mList.IsOpaque()) {
    result = GetBounds(aBuilder);
  }
  return result;
}

PRBool nsDisplayWrapList::IsUniform(nsDisplayListBuilder* aBuilder, nscolor* aColor) {
  // We could try to do something but let's conservatively just return PR_FALSE.
  return PR_FALSE;
}

PRBool nsDisplayWrapList::IsVaryingRelativeToMovingFrame(nsDisplayListBuilder* aBuilder,
                                                         nsIFrame* aFrame) {
  NS_WARNING("nsDisplayWrapList::IsVaryingRelativeToMovingFrame called unexpectedly");
  // We could try to do something but let's conservatively just return PR_TRUE.
  return PR_TRUE;
}

void nsDisplayWrapList::Paint(nsDisplayListBuilder* aBuilder,
                              nsRenderingContext* aCtx) {
  NS_ERROR("nsDisplayWrapList should have been flattened away for painting");
}

PRBool nsDisplayWrapList::ChildrenCanBeInactive(nsDisplayListBuilder* aBuilder,
                                                LayerManager* aManager,
                                                const nsDisplayList& aList,
                                                nsIFrame* aActiveScrolledRoot) {
  for (nsDisplayItem* i = aList.GetBottom(); i; i = i->GetAbove()) {
    nsIFrame* f = i->GetUnderlyingFrame();
    if (f) {
      nsIFrame* activeScrolledRoot =
        nsLayoutUtils::GetActiveScrolledRootFor(f, nsnull);
      if (activeScrolledRoot != aActiveScrolledRoot)
        return PR_FALSE;
    }

    LayerState state = i->GetLayerState(aBuilder, aManager);
    if (state == LAYER_ACTIVE)
      return PR_FALSE;
    if (state == LAYER_NONE) {
      nsDisplayList* list = i->GetList();
      if (list && !ChildrenCanBeInactive(aBuilder, aManager, *list, aActiveScrolledRoot))
        return PR_FALSE;
    }
  }
  return PR_TRUE;
}

nsRect nsDisplayWrapList::GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder)
{
  nsRect bounds;
  for (nsDisplayItem* i = mList.GetBottom(); i; i = i->GetAbove()) {
    bounds.UnionRect(bounds, i->GetComponentAlphaBounds(aBuilder));
  }
  return bounds;
}

static nsresult
WrapDisplayList(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                nsDisplayList* aList, nsDisplayWrapper* aWrapper) {
  if (!aList->GetTop())
    return NS_OK;
  nsDisplayItem* item = aWrapper->WrapList(aBuilder, aFrame, aList);
  if (!item)
    return NS_ERROR_OUT_OF_MEMORY;
  // aList was emptied
  aList->AppendToTop(item);
  return NS_OK;
}

static nsresult
WrapEachDisplayItem(nsDisplayListBuilder* aBuilder,
                    nsDisplayList* aList, nsDisplayWrapper* aWrapper) {
  nsDisplayList newList;
  nsDisplayItem* item;
  while ((item = aList->RemoveBottom())) {
    item = aWrapper->WrapItem(aBuilder, item);
    if (!item)
      return NS_ERROR_OUT_OF_MEMORY;
    newList.AppendToTop(item);
  }
  // aList was emptied
  aList->AppendToTop(&newList);
  return NS_OK;
}

nsresult nsDisplayWrapper::WrapLists(nsDisplayListBuilder* aBuilder,
    nsIFrame* aFrame, const nsDisplayListSet& aIn, const nsDisplayListSet& aOut)
{
  nsresult rv = WrapListsInPlace(aBuilder, aFrame, aIn);
  NS_ENSURE_SUCCESS(rv, rv);

  if (&aOut == &aIn)
    return NS_OK;
  aOut.BorderBackground()->AppendToTop(aIn.BorderBackground());
  aOut.BlockBorderBackgrounds()->AppendToTop(aIn.BlockBorderBackgrounds());
  aOut.Floats()->AppendToTop(aIn.Floats());
  aOut.Content()->AppendToTop(aIn.Content());
  aOut.PositionedDescendants()->AppendToTop(aIn.PositionedDescendants());
  aOut.Outlines()->AppendToTop(aIn.Outlines());
  return NS_OK;
}

nsresult nsDisplayWrapper::WrapListsInPlace(nsDisplayListBuilder* aBuilder,
    nsIFrame* aFrame, const nsDisplayListSet& aLists)
{
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

nsDisplayOpacity::nsDisplayOpacity(nsDisplayListBuilder* aBuilder,
                                   nsIFrame* aFrame, nsDisplayList* aList)
    : nsDisplayWrapList(aBuilder, aFrame, aList) {
  MOZ_COUNT_CTOR(nsDisplayOpacity);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayOpacity::~nsDisplayOpacity() {
  MOZ_COUNT_DTOR(nsDisplayOpacity);
}
#endif

nsRegion nsDisplayOpacity::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                           PRBool* aForceTransparentSurface) {
  if (aForceTransparentSurface) {
    *aForceTransparentSurface = PR_FALSE;
  }
  // We are never opaque, if our opacity was < 1 then we wouldn't have
  // been created.
  return nsRegion();
}

// nsDisplayOpacity uses layers for rendering
already_AddRefed<Layer>
nsDisplayOpacity::BuildLayer(nsDisplayListBuilder* aBuilder,
                             LayerManager* aManager,
                             const ContainerParameters& aContainerParameters) {
  nsRefPtr<Layer> layer = aBuilder->LayerBuilder()->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, mList,
                           aContainerParameters, nsnull);
  if (!layer)
    return nsnull;

  layer->SetOpacity(mFrame->GetStyleDisplay()->mOpacity);
  return layer.forget();
}

nsDisplayItem::LayerState
nsDisplayOpacity::GetLayerState(nsDisplayListBuilder* aBuilder,
                                LayerManager* aManager) {
  if (mFrame->AreLayersMarkedActive(nsChangeHint_UpdateOpacityLayer))
    return LAYER_ACTIVE;
  nsIFrame* activeScrolledRoot =
    nsLayoutUtils::GetActiveScrolledRootFor(mFrame, nsnull);
  return !ChildrenCanBeInactive(aBuilder, aManager, mList, activeScrolledRoot)
      ? LAYER_ACTIVE : LAYER_INACTIVE;
}

PRBool
nsDisplayOpacity::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                    nsRegion* aVisibleRegion,
                                    const nsRect& aAllowVisibleRegionExpansion) {
  // Our children are translucent so we should not allow them to subtract
  // area from aVisibleRegion. We do need to find out what is visible under
  // our children in the temporary compositing buffer, because if our children
  // paint our entire bounds opaquely then we don't need an alpha channel in
  // the temporary compositing buffer.
  nsRect bounds = GetBounds(aBuilder);
  nsRegion visibleUnderChildren;
  visibleUnderChildren.And(*aVisibleRegion, bounds);
  nsRect allowExpansion;
  allowExpansion.IntersectRect(bounds, aAllowVisibleRegionExpansion);
  return
    nsDisplayWrapList::ComputeVisibility(aBuilder, &visibleUnderChildren,
                                         allowExpansion);
}

PRBool nsDisplayOpacity::TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem) {
  if (aItem->GetType() != TYPE_OPACITY)
    return PR_FALSE;
  // items for the same content element should be merged into a single
  // compositing group
  // aItem->GetUnderlyingFrame() returns non-null because it's nsDisplayOpacity
  if (aItem->GetUnderlyingFrame()->GetContent() != mFrame->GetContent())
    return PR_FALSE;
  mList.AppendToBottom(&static_cast<nsDisplayOpacity*>(aItem)->mList);
  return PR_TRUE;
}

nsDisplayOwnLayer::nsDisplayOwnLayer(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame, nsDisplayList* aList)
    : nsDisplayWrapList(aBuilder, aFrame, aList) {
  MOZ_COUNT_CTOR(nsDisplayOwnLayer);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayOwnLayer::~nsDisplayOwnLayer() {
  MOZ_COUNT_DTOR(nsDisplayOwnLayer);
}
#endif

// nsDisplayOpacity uses layers for rendering
already_AddRefed<Layer>
nsDisplayOwnLayer::BuildLayer(nsDisplayListBuilder* aBuilder,
                              LayerManager* aManager,
                              const ContainerParameters& aContainerParameters) {
  nsRefPtr<Layer> layer = aBuilder->LayerBuilder()->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, mList,
                           aContainerParameters, nsnull);
  return layer.forget();
}

nsDisplayScrollLayer::nsDisplayScrollLayer(nsDisplayListBuilder* aBuilder,
                                           nsDisplayList* aList,
                                           nsIFrame* aForFrame,
                                           nsIFrame* aScrolledFrame,
                                           nsIFrame* aScrollFrame)
  : nsDisplayWrapList(aBuilder, aForFrame, aList)
  , mScrollFrame(aScrollFrame)
  , mScrolledFrame(aScrolledFrame)
{
#ifdef NS_BUILD_REFCNT_LOGGING
  MOZ_COUNT_CTOR(nsDisplayScrollLayer);
#endif

  NS_ASSERTION(mScrolledFrame && mScrolledFrame->GetContent(),
               "Need a child frame with content");
}

nsDisplayScrollLayer::nsDisplayScrollLayer(nsDisplayListBuilder* aBuilder,
                                           nsDisplayItem* aItem,
                                           nsIFrame* aForFrame,
                                           nsIFrame* aScrolledFrame,
                                           nsIFrame* aScrollFrame)
  : nsDisplayWrapList(aBuilder, aForFrame, aItem)
  , mScrollFrame(aScrollFrame)
  , mScrolledFrame(aScrolledFrame)
{
#ifdef NS_BUILD_REFCNT_LOGGING
  MOZ_COUNT_CTOR(nsDisplayScrollLayer);
#endif

  NS_ASSERTION(mScrolledFrame && mScrolledFrame->GetContent(),
               "Need a child frame with content");
}

nsDisplayScrollLayer::nsDisplayScrollLayer(nsDisplayListBuilder* aBuilder,
                                           nsIFrame* aForFrame,
                                           nsIFrame* aScrolledFrame,
                                           nsIFrame* aScrollFrame)
  : nsDisplayWrapList(aBuilder, aForFrame)
  , mScrollFrame(aScrollFrame)
  , mScrolledFrame(aScrolledFrame)
{
#ifdef NS_BUILD_REFCNT_LOGGING
  MOZ_COUNT_CTOR(nsDisplayScrollLayer);
#endif

  NS_ASSERTION(mScrolledFrame && mScrolledFrame->GetContent(),
               "Need a child frame with content");
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayScrollLayer::~nsDisplayScrollLayer()
{
  MOZ_COUNT_DTOR(nsDisplayScrollLayer);
}
#endif

already_AddRefed<Layer>
nsDisplayScrollLayer::BuildLayer(nsDisplayListBuilder* aBuilder,
                                 LayerManager* aManager,
                                 const ContainerParameters& aContainerParameters) {
  nsRefPtr<ContainerLayer> layer = aBuilder->LayerBuilder()->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, mList,
                           aContainerParameters, nsnull);

  // Get the already set unique ID for scrolling this content remotely.
  // Or, if not set, generate a new ID.
  nsIContent* content = mScrolledFrame->GetContent();
  ViewID scrollId = nsLayoutUtils::FindIDFor(content);

  nsRect viewport = mScrollFrame->GetRect() -
                    mScrollFrame->GetPosition() +
                    aBuilder->ToReferenceFrame(mScrollFrame);

  bool usingDisplayport = false;
  nsRect displayport;
  if (content) {
    usingDisplayport = nsLayoutUtils::GetDisplayPort(content, &displayport);
  }
  RecordFrameMetrics(mScrolledFrame, mScrollFrame, layer, mVisibleRect, viewport,
                     (usingDisplayport ? &displayport : nsnull), scrollId,
                     aContainerParameters);

  return layer.forget();
}

PRBool
nsDisplayScrollLayer::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                        nsRegion* aVisibleRegion,
                                        const nsRect& aAllowVisibleRegionExpansion)
{
  nsRect displayport;
  if (nsLayoutUtils::GetDisplayPort(mScrolledFrame->GetContent(), &displayport)) {
    // The visible region for the children may be much bigger than the hole we
    // are viewing the children from, so that the compositor process has enough
    // content to asynchronously pan while content is being refreshed.

    nsRegion childVisibleRegion = displayport + aBuilder->ToReferenceFrame(mScrollFrame);

    nsRect boundedRect;
    boundedRect.IntersectRect(childVisibleRegion.GetBounds(), mList.GetBounds(aBuilder));
    nsRect allowExpansion;
    allowExpansion.IntersectRect(allowExpansion, boundedRect);
    PRBool visible = mList.ComputeVisibilityForSublist(
      aBuilder, &childVisibleRegion, boundedRect, allowExpansion);
    mVisibleRect = boundedRect;

    return visible;

  } else {
    return nsDisplayWrapList::ComputeVisibility(aBuilder, aVisibleRegion,
                                                aAllowVisibleRegionExpansion);
  }
}

LayerState
nsDisplayScrollLayer::GetLayerState(nsDisplayListBuilder* aBuilder,
                                    LayerManager* aManager)
{
  // Force this as a layer so we can scroll asynchronously.
  // This causes incorrect rendering for rounded clips!
  return LAYER_ACTIVE_FORCE;
}

PRBool
nsDisplayScrollLayer::TryMerge(nsDisplayListBuilder* aBuilder,
                               nsDisplayItem* aItem)
{
  if (aItem->GetType() != TYPE_SCROLL_LAYER) {
    return PR_FALSE;
  }

  nsDisplayScrollLayer* other = static_cast<nsDisplayScrollLayer*>(aItem);
  if (other->mScrolledFrame != this->mScrolledFrame) {
    return PR_FALSE;
  }

  FrameProperties props = mScrolledFrame->Properties();
  props.Set(nsIFrame::ScrollLayerCount(),
    reinterpret_cast<void*>(GetScrollLayerCount() - 1));

  mList.AppendToBottom(&other->mList);
  return PR_TRUE;
}

PRBool
nsDisplayScrollLayer::ShouldFlattenAway(nsDisplayListBuilder* aBuilder)
{
  return GetScrollLayerCount() > 1;
}

PRWord
nsDisplayScrollLayer::GetScrollLayerCount()
{
  FrameProperties props = mScrolledFrame->Properties();
#ifdef DEBUG
  PRBool hasCount = PR_FALSE;
  PRWord result = reinterpret_cast<PRWord>(
    props.Get(nsIFrame::ScrollLayerCount(), &hasCount));
  // If this aborts, then the property was either not added before scroll
  // layers were created or the property was deleted to early. If the latter,
  // make sure that nsDisplayScrollInfoLayer is on the bottom of the list so
  // that it is processed last.
  NS_ABORT_IF_FALSE(hasCount, "nsDisplayScrollLayer should always be defined");
  return result;
#else
  return reinterpret_cast<PRWord>(props.Get(nsIFrame::ScrollLayerCount()));
#endif
}

PRWord
nsDisplayScrollLayer::RemoveScrollLayerCount()
{
  PRWord result = GetScrollLayerCount();
  FrameProperties props = mScrolledFrame->Properties();
  props.Remove(nsIFrame::ScrollLayerCount());
  return result;
}


nsDisplayScrollInfoLayer::nsDisplayScrollInfoLayer(
  nsDisplayListBuilder* aBuilder,
  nsIFrame* aScrolledFrame,
  nsIFrame* aScrollFrame)
  : nsDisplayScrollLayer(aBuilder, aScrolledFrame, aScrolledFrame, aScrollFrame)
{
#ifdef NS_BUILD_REFCNT_LOGGING
  MOZ_COUNT_CTOR(nsDisplayScrollInfoLayer);
#endif
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayScrollInfoLayer::~nsDisplayScrollInfoLayer()
{
  MOZ_COUNT_DTOR(nsDisplayScrollInfoLayer);
}
#endif

LayerState
nsDisplayScrollInfoLayer::GetLayerState(nsDisplayListBuilder* aBuilder,
                                        LayerManager* aManager)
{
  return LAYER_ACTIVE_EMPTY;
}

PRBool
nsDisplayScrollInfoLayer::TryMerge(nsDisplayListBuilder* aBuilder,
                                   nsDisplayItem* aItem)
{
  return PR_FALSE;
}

PRBool
nsDisplayScrollInfoLayer::ShouldFlattenAway(nsDisplayListBuilder* aBuilder)
{
  // Layer metadata for a particular scroll frame needs to be unique. Only
  // one nsDisplayScrollLayer (with rendered content) or one
  // nsDisplayScrollInfoLayer (with only the metadata) should survive the
  // visibility computation. 
  return RemoveScrollLayerCount() == 1;
}

nsDisplayClip::nsDisplayClip(nsDisplayListBuilder* aBuilder,
                             nsIFrame* aFrame, nsDisplayItem* aItem,
                             const nsRect& aRect)
   : nsDisplayWrapList(aBuilder, aFrame, aItem) {
  MOZ_COUNT_CTOR(nsDisplayClip);
  mClip = SnapBounds(aBuilder->IsSnappingEnabled() && !aBuilder->IsInTransform(),
                     aBuilder->CurrentPresContext(), aRect);
}

nsDisplayClip::nsDisplayClip(nsDisplayListBuilder* aBuilder,
                             nsIFrame* aFrame, nsDisplayList* aList,
                             const nsRect& aRect)
   : nsDisplayWrapList(aBuilder, aFrame, aList) {
  MOZ_COUNT_CTOR(nsDisplayClip);
  mClip = SnapBounds(aBuilder->IsSnappingEnabled() && !aBuilder->IsInTransform(),
                     aBuilder->CurrentPresContext(), aRect);
}

nsRect nsDisplayClip::GetBounds(nsDisplayListBuilder* aBuilder) {
  nsRect r = nsDisplayWrapList::GetBounds(aBuilder);
  r.IntersectRect(mClip, r);
  return r;
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayClip::~nsDisplayClip() {
  MOZ_COUNT_DTOR(nsDisplayClip);
}
#endif

void nsDisplayClip::Paint(nsDisplayListBuilder* aBuilder,
                          nsRenderingContext* aCtx) {
  NS_ERROR("nsDisplayClip should have been flattened away for painting");
}

PRBool nsDisplayClip::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                        nsRegion* aVisibleRegion,
                                        const nsRect& aAllowVisibleRegionExpansion) {
  nsRegion clipped;
  clipped.And(*aVisibleRegion, mClip);

  nsRegion finalClipped(clipped);
  nsRect allowExpansion;
  allowExpansion.IntersectRect(mClip, aAllowVisibleRegionExpansion);
  PRBool anyVisible =
    nsDisplayWrapList::ComputeVisibility(aBuilder, &finalClipped,
                                         allowExpansion);

  nsRegion removed;
  removed.Sub(clipped, finalClipped);
  aBuilder->SubtractFromVisibleRegion(aVisibleRegion, removed);

  return anyVisible;
}

PRBool nsDisplayClip::TryMerge(nsDisplayListBuilder* aBuilder,
                               nsDisplayItem* aItem) {
  if (aItem->GetType() != TYPE_CLIP)
    return PR_FALSE;
  nsDisplayClip* other = static_cast<nsDisplayClip*>(aItem);
  if (!other->mClip.IsEqualInterior(mClip))
    return PR_FALSE;
  mList.AppendToBottom(&other->mList);
  return PR_TRUE;
}

nsDisplayWrapList* nsDisplayClip::WrapWithClone(nsDisplayListBuilder* aBuilder,
                                                nsDisplayItem* aItem) {
  return new (aBuilder)
    nsDisplayClip(aBuilder, aItem->GetUnderlyingFrame(), aItem, mClip);
}

nsDisplayClipRoundedRect::nsDisplayClipRoundedRect(
                             nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                             nsDisplayItem* aItem,
                             const nsRect& aRect, nscoord aRadii[8])
    : nsDisplayClip(aBuilder, aFrame, aItem, aRect)
{
  MOZ_COUNT_CTOR(nsDisplayClipRoundedRect);
  memcpy(mRadii, aRadii, sizeof(mRadii));
}

nsDisplayClipRoundedRect::nsDisplayClipRoundedRect(
                             nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                             nsDisplayList* aList,
                             const nsRect& aRect, nscoord aRadii[8])
    : nsDisplayClip(aBuilder, aFrame, aList, aRect)
{
  MOZ_COUNT_CTOR(nsDisplayClipRoundedRect);
  memcpy(mRadii, aRadii, sizeof(mRadii));
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayClipRoundedRect::~nsDisplayClipRoundedRect()
{
  MOZ_COUNT_DTOR(nsDisplayClipRoundedRect);
}
#endif

nsRegion
nsDisplayClipRoundedRect::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                          PRBool* aForceTransparentSurface)
{
  if (aForceTransparentSurface) {
    *aForceTransparentSurface = PR_FALSE;
  }
  return nsRegion();
}

void
nsDisplayClipRoundedRect::HitTest(nsDisplayListBuilder* aBuilder,
                                  const nsRect& aRect, HitTestState* aState,
                                  nsTArray<nsIFrame*> *aOutFrames)
{
  if (!RoundedRectIntersectsRect(mClip, mRadii, aRect)) {
    // aRect doesn't intersect our border-radius curve.

    // FIXME: This isn't quite sufficient for aRect having nontrivial
    // size (which is the unusual case here), since it's possible that
    // the part of aRect that intersects the the rounded rect isn't the
    // part that intersects the items in mList.
    return;
  }

  mList.HitTest(aBuilder, aRect, aState, aOutFrames);
}

nsDisplayWrapList*
nsDisplayClipRoundedRect::WrapWithClone(nsDisplayListBuilder* aBuilder,
                                        nsDisplayItem* aItem) {
  return new (aBuilder)
    nsDisplayClipRoundedRect(aBuilder, aItem->GetUnderlyingFrame(), aItem,
                             mClip, mRadii);
}

PRBool nsDisplayClipRoundedRect::ComputeVisibility(
                                    nsDisplayListBuilder* aBuilder,
                                    nsRegion* aVisibleRegion,
                                    const nsRect& aAllowVisibleRegionExpansion)
{
  nsRegion clipped;
  clipped.And(*aVisibleRegion, mClip);

  return nsDisplayWrapList::ComputeVisibility(aBuilder, &clipped, nsRect());
  // FIXME: Remove a *conservative* opaque region from aVisibleRegion
  // (like in nsDisplayClip::ComputeVisibility).
}

PRBool nsDisplayClipRoundedRect::TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem)
{
  if (aItem->GetType() != TYPE_CLIP_ROUNDED_RECT)
    return PR_FALSE;
  nsDisplayClipRoundedRect* other =
    static_cast<nsDisplayClipRoundedRect*>(aItem);
  if (!mClip.IsEqualInterior(other->mClip) ||
      memcmp(mRadii, other->mRadii, sizeof(mRadii)) != 0)
    return PR_FALSE;
  mList.AppendToBottom(&other->mList);
  return PR_TRUE;
}

nsDisplayZoom::nsDisplayZoom(nsDisplayListBuilder* aBuilder,
                             nsIFrame* aFrame, nsDisplayList* aList,
                             PRInt32 aAPD, PRInt32 aParentAPD)
    : nsDisplayOwnLayer(aBuilder, aFrame, aList), mAPD(aAPD),
      mParentAPD(aParentAPD) {
  MOZ_COUNT_CTOR(nsDisplayZoom);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayZoom::~nsDisplayZoom() {
  MOZ_COUNT_DTOR(nsDisplayZoom);
}
#endif

nsRect nsDisplayZoom::GetBounds(nsDisplayListBuilder* aBuilder)
{
  nsRect bounds = nsDisplayWrapList::GetBounds(aBuilder);
  return bounds.ConvertAppUnitsRoundOut(mAPD, mParentAPD);
}

void nsDisplayZoom::HitTest(nsDisplayListBuilder *aBuilder,
                            const nsRect& aRect,
                            HitTestState *aState,
                            nsTArray<nsIFrame*> *aOutFrames)
{
  nsRect rect;
  // A 1x1 rect indicates we are just hit testing a point, so pass down a 1x1
  // rect as well instead of possibly rounding the width or height to zero.
  if (aRect.width == 1 && aRect.height == 1) {
    rect.MoveTo(aRect.TopLeft().ConvertAppUnits(mParentAPD, mAPD));
    rect.width = rect.height = 1;
  } else {
    rect = aRect.ConvertAppUnitsRoundOut(mParentAPD, mAPD);
  }
  mList.HitTest(aBuilder, rect, aState, aOutFrames);
}

void nsDisplayZoom::Paint(nsDisplayListBuilder* aBuilder,
                          nsRenderingContext* aCtx)
{
  mList.PaintForFrame(aBuilder, aCtx, mFrame, nsDisplayList::PAINT_DEFAULT);
}

PRBool nsDisplayZoom::ComputeVisibility(nsDisplayListBuilder *aBuilder,
                                        nsRegion *aVisibleRegion,
                                        const nsRect& aAllowVisibleRegionExpansion)
{
  // Convert the passed in visible region to our appunits.
  nsRegion visibleRegion =
    aVisibleRegion->ConvertAppUnitsRoundOut(mParentAPD, mAPD);
  nsRegion originalVisibleRegion = visibleRegion;

  nsRect transformedVisibleRect =
    mVisibleRect.ConvertAppUnitsRoundOut(mParentAPD, mAPD);
  nsRect allowExpansion =
    aAllowVisibleRegionExpansion.ConvertAppUnitsRoundIn(mParentAPD, mAPD);
  PRBool retval =
    mList.ComputeVisibilityForSublist(aBuilder, &visibleRegion,
                                      transformedVisibleRect,
                                      allowExpansion);

  nsRegion removed;
  // removed = originalVisibleRegion - visibleRegion
  removed.Sub(originalVisibleRegion, visibleRegion);
  // Convert removed region to parent appunits.
  removed = removed.ConvertAppUnitsRoundIn(mAPD, mParentAPD);
  // aVisibleRegion = aVisibleRegion - removed (modulo any simplifications
  // SubtractFromVisibleRegion does)
  aBuilder->SubtractFromVisibleRegion(aVisibleRegion, removed);

  return retval;
}

///////////////////////////////////////////////////
// nsDisplayTransform Implementation
//

// Write #define UNIFIED_CONTINUATIONS here to have the transform property try
// to transform content with continuations as one unified block instead of
// several smaller ones.  This is currently disabled because it doesn't work
// correctly, since when the frames are initially being reflowed, their
// continuations all compute their bounding rects independently of each other
// and consequently get the wrong value.  Write #define DEBUG_HIT here to have
// the nsDisplayTransform class dump out a bunch of information about hit
// detection.
#undef  UNIFIED_CONTINUATIONS
#undef  DEBUG_HIT

/* Returns the bounds of a frame as defined for transforms.  If
 * UNIFIED_CONTINUATIONS is not defined, this is simply the frame's bounding
 * rectangle, translated to the origin. Otherwise, returns the smallest
 * rectangle containing a frame and all of its continuations.  For example, if
 * there is a <span> element with several continuations split over several
 * lines, this function will return the rectangle containing all of those
 * continuations.  This rectangle is relative to the origin of the frame's local
 * coordinate space.
 */
#ifndef UNIFIED_CONTINUATIONS

nsRect
nsDisplayTransform::GetFrameBoundsForTransform(const nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "Can't get the bounds of a nonexistent frame!");
  return nsRect(nsPoint(0, 0), aFrame->GetSize());
}

#else

nsRect
nsDisplayTransform::GetFrameBoundsForTransform(const nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "Can't get the bounds of a nonexistent frame!");

  nsRect result;
  
  /* Iterate through the continuation list, unioning together all the
   * bounding rects.
   */
  for (const nsIFrame *currFrame = aFrame->GetFirstContinuation();
       currFrame != nsnull;
       currFrame = currFrame->GetNextContinuation())
    {
      /* Get the frame rect in local coordinates, then translate back to the
       * original coordinates.
       */
      result.UnionRect(result, nsRect(currFrame->GetOffsetTo(aFrame),
                                      currFrame->GetSize()));
    }

  return result;
}

#endif

/* Returns the delta specified by the -moz-transform-origin property.
 * This is a positive delta, meaning that it indicates the direction to move
 * to get from (0, 0) of the frame to the transform origin.
 */
static
gfxPoint3D GetDeltaToMozTransformOrigin(const nsIFrame* aFrame,
                                        float aFactor,
                                        const nsRect* aBoundsOverride)
{
  NS_PRECONDITION(aFrame, "Can't get delta for a null frame!");
  NS_PRECONDITION(aFrame->GetStyleDisplay()->HasTransform(),
                  "Can't get a delta for an untransformed frame!");

  /* For both of the coordinates, if the value of -moz-transform is a
   * percentage, it's relative to the size of the frame.  Otherwise, if it's
   * a distance, it's already computed for us!
   */
  const nsStyleDisplay* display = aFrame->GetStyleDisplay();
  nsRect boundingRect = (aBoundsOverride ? *aBoundsOverride :
                         nsDisplayTransform::GetFrameBoundsForTransform(aFrame));

  /* Allows us to access named variables by index. */
  gfxPoint3D result;
  gfxFloat* coords[3] = {&result.x, &result.y, &result.z};
  const nscoord* dimensions[2] =
    {&boundingRect.width, &boundingRect.height};

  for (PRUint8 index = 0; index < 2; ++index) {
    /* If the -moz-transform-origin specifies a percentage, take the percentage
     * of the size of the box.
     */
    const nsStyleCoord &coord = display->mTransformOrigin[index];
    if (coord.GetUnit() == eStyleUnit_Calc) {
      const nsStyleCoord::Calc *calc = coord.GetCalcValue();
      *coords[index] = NSAppUnitsToFloatPixels(*dimensions[index], aFactor) *
                         calc->mPercent +
                       NSAppUnitsToFloatPixels(calc->mLength, aFactor);
    } else if (coord.GetUnit() == eStyleUnit_Percent) {
      *coords[index] = NSAppUnitsToFloatPixels(*dimensions[index], aFactor) *
        coord.GetPercentValue();
    } else {
      NS_ABORT_IF_FALSE(coord.GetUnit() == eStyleUnit_Coord, "unexpected unit");
      *coords[index] = NSAppUnitsToFloatPixels(coord.GetCoordValue(), aFactor);
    }
  }

  *coords[2] = NSAppUnitsToFloatPixels(display->mTransformOrigin[2].GetCoordValue(), aFactor);
  
  /* Adjust based on the origin of the rectangle. */
  result.x += NSAppUnitsToFloatPixels(boundingRect.x, aFactor);
  result.y += NSAppUnitsToFloatPixels(boundingRect.y, aFactor);

  return result;
}

/* Returns the delta specified by the -moz-perspective-origin property.
 * This is a positive delta, meaning that it indicates the direction to move
 * to get from (0, 0) of the frame to the perspective origin.
 */
static
gfxPoint3D GetDeltaToMozPerspectiveOrigin(const nsIFrame* aFrame,
                                          float aFactor,
                                          const nsRect* aBoundsOverride)
{
  NS_PRECONDITION(aFrame, "Can't get delta for a null frame!");
  NS_PRECONDITION(aFrame->GetStyleDisplay()->HasTransform(),
                  "Can't get a delta for an untransformed frame!");

  /* For both of the coordinates, if the value of -moz-perspective-origin is a
   * percentage, it's relative to the size of the frame.  Otherwise, if it's
   * a distance, it's already computed for us!
   */

  //TODO: Should this be using our bounds or the parent's bounds?
  // How do we handle aBoundsOverride in the latter case?
  const nsStyleDisplay* display = aFrame->GetParent()->GetStyleDisplay();
  nsRect boundingRect = (aBoundsOverride ? *aBoundsOverride :
                         nsDisplayTransform::GetFrameBoundsForTransform(aFrame));

  /* Allows us to access named variables by index. */
  gfxPoint3D result;
  result.z = 0.0f;
  gfxFloat* coords[2] = {&result.x, &result.y};
  const nscoord* dimensions[2] =
    {&boundingRect.width, &boundingRect.height};

  for (PRUint8 index = 0; index < 2; ++index) {
    /* If the -moz-transform-origin specifies a percentage, take the percentage
     * of the size of the box.
     */
    const nsStyleCoord &coord = display->mPerspectiveOrigin[index];
    if (coord.GetUnit() == eStyleUnit_Calc) {
      const nsStyleCoord::Calc *calc = coord.GetCalcValue();
      *coords[index] = NSAppUnitsToFloatPixels(*dimensions[index], aFactor) *
                         calc->mPercent +
                       NSAppUnitsToFloatPixels(calc->mLength, aFactor);
    } else if (coord.GetUnit() == eStyleUnit_Percent) {
      *coords[index] = NSAppUnitsToFloatPixels(*dimensions[index], aFactor) *
        coord.GetPercentValue();
    } else {
      NS_ABORT_IF_FALSE(coord.GetUnit() == eStyleUnit_Coord, "unexpected unit");
      *coords[index] = NSAppUnitsToFloatPixels(coord.GetCoordValue(), aFactor);
    }
  }
  
  /**
   * An offset of (0,0) results in the perspective-origin being at the centre of the element,
   * so include a shift of the centre point to (0,0).
   */
  result.x -= NSAppUnitsToFloatPixels(boundingRect.width, aFactor)/2;
  result.y -= NSAppUnitsToFloatPixels(boundingRect.height, aFactor)/2;

  return result;
}

/* Wraps up the -moz-transform matrix in a change-of-basis matrix pair that
 * translates from local coordinate space to transform coordinate space, then
 * hands it back.
 */
gfx3DMatrix
nsDisplayTransform::GetResultingTransformMatrix(const nsIFrame* aFrame,
                                                const nsPoint &aOrigin,
                                                float aFactor,
                                                const nsRect* aBoundsOverride)
{
  NS_PRECONDITION(aFrame, "Cannot get transform matrix for a null frame!");
  NS_PRECONDITION(aFrame->GetStyleDisplay()->HasTransform(),
                  "Cannot get transform matrix if frame isn't transformed!");

  /* Account for the -moz-transform-origin property by translating the
   * coordinate space to the new origin.
   */
  gfxPoint3D toMozOrigin = GetDeltaToMozTransformOrigin(aFrame, aFactor, aBoundsOverride);
  gfxPoint3D toPerspectiveOrigin = GetDeltaToMozPerspectiveOrigin(aFrame, aFactor, aBoundsOverride);
  gfxPoint3D newOrigin = gfxPoint3D(NSAppUnitsToFloatPixels(aOrigin.x, aFactor),
                                    NSAppUnitsToFloatPixels(aOrigin.y, aFactor),
                                    0.0f);

  /* Get the underlying transform matrix.  This requires us to get the
   * bounds of the frame.
   */
  const nsStyleDisplay* disp = aFrame->GetStyleDisplay();
  nsRect bounds = (aBoundsOverride ? *aBoundsOverride :
                   nsDisplayTransform::GetFrameBoundsForTransform(aFrame));

  /* Get the matrix, then change its basis to factor in the origin. */
  PRBool dummy;
  gfx3DMatrix result =
    nsStyleTransformMatrix::ReadTransforms(disp->mSpecifiedTransform,
                                           aFrame->GetStyleContext(),
                                           aFrame->PresContext(),
                                           dummy, bounds, aFactor);

  const nsStyleDisplay* parentDisp = nsnull;
  if (aFrame->GetParent()) {
    parentDisp = aFrame->GetParent()->GetStyleDisplay();
  }
  if (nsLayoutUtils::Are3DTransformsEnabled() &&
      parentDisp && parentDisp->mChildPerspective.GetUnit() == eStyleUnit_Coord &&
      parentDisp->mChildPerspective.GetCoordValue() > 0.0) {
    gfx3DMatrix perspective;
    perspective._34 =
      -1.0 / NSAppUnitsToFloatPixels(parentDisp->mChildPerspective.GetCoordValue(),
                                     aFactor);
    result = result * nsLayoutUtils::ChangeMatrixBasis(toPerspectiveOrigin, perspective);
  }
  return nsLayoutUtils::ChangeMatrixBasis
    (newOrigin + toMozOrigin, result);
}

const gfx3DMatrix&
nsDisplayTransform::GetTransform(float aFactor)
{
  if (mTransform.IsIdentity() || mCachedFactor != aFactor) {
    mTransform =
      GetResultingTransformMatrix(mFrame, ToReferenceFrame(),
                                  aFactor,
                                  nsnull);
    mCachedFactor = aFactor;
  }
  return mTransform;
}

already_AddRefed<Layer> nsDisplayTransform::BuildLayer(nsDisplayListBuilder *aBuilder,
                                                       LayerManager *aManager,
                                                       const ContainerParameters& aContainerParameters)
{
  const gfx3DMatrix& newTransformMatrix = 
    GetTransform(mFrame->PresContext()->AppUnitsPerDevPixel());

  if (newTransformMatrix.IsSingular() ||
      (mFrame->GetStyleDisplay()->mBackfaceVisibility == NS_STYLE_BACKFACE_VISIBILITY_HIDDEN &&
       newTransformMatrix.GetNormalVector().z <= 0.0)) {
    return nsnull;
  }

  return aBuilder->LayerBuilder()->
    BuildContainerLayerFor(aBuilder, aManager, mFrame, this, *mStoredList.GetList(),
                           aContainerParameters, &newTransformMatrix);
}

nsDisplayItem::LayerState
nsDisplayTransform::GetLayerState(nsDisplayListBuilder* aBuilder,
                                  LayerManager* aManager) {
  if (mFrame->AreLayersMarkedActive(nsChangeHint_UpdateTransformLayer))
    return LAYER_ACTIVE;
  if (!GetTransform(mFrame->PresContext()->AppUnitsPerDevPixel()).Is2D())
    return LAYER_ACTIVE;
  nsIFrame* activeScrolledRoot =
    nsLayoutUtils::GetActiveScrolledRootFor(mFrame, nsnull);
  return !mStoredList.ChildrenCanBeInactive(aBuilder, 
                                             aManager, 
                                             *mStoredList.GetList(), 
                                             activeScrolledRoot)
      ? LAYER_ACTIVE : LAYER_INACTIVE;
}

PRBool nsDisplayTransform::ComputeVisibility(nsDisplayListBuilder *aBuilder,
                                             nsRegion *aVisibleRegion,
                                             const nsRect& aAllowVisibleRegionExpansion)
{
  /* As we do this, we need to be sure to
   * untransform the visible rect, since we want everything that's painting to
   * think that it's painting in its original rectangular coordinate space.
   * If we can't untransform, take the entire overflow rect */
  nsRect untransformedVisibleRect;
  if (!UntransformRect(mVisibleRect, 
                       mFrame, 
                       aBuilder->ToReferenceFrame(mFrame), 
                       &untransformedVisibleRect)) 
  {
    untransformedVisibleRect = mFrame->GetVisualOverflowRectRelativeToSelf() +  
                               aBuilder->ToReferenceFrame(mFrame);
  }
  nsRegion untransformedVisible = untransformedVisibleRect;
  // Call RecomputeVisiblity instead of ComputeVisibilty since
  // nsDisplayItem::ComputeVisibility should only be called from
  // nsDisplayList::ComputeVisibility (which sets mVisibleRect on the item)
  mStoredList.RecomputeVisibility(aBuilder, &untransformedVisible);
  return PR_TRUE;
}

#ifdef DEBUG_HIT
#include <time.h>
#endif

/* HitTest does some fun stuff with matrix transforms to obtain the answer. */
void nsDisplayTransform::HitTest(nsDisplayListBuilder *aBuilder,
                                 const nsRect& aRect,
                                 HitTestState *aState,
                                 nsTArray<nsIFrame*> *aOutFrames)
{
  /* Here's how this works:
   * 1. Get the matrix.  If it's singular, abort (clearly we didn't hit
   *    anything).
   * 2. Invert the matrix.
   * 3. Use it to transform the rect into the correct space.
   * 4. Pass that rect down through to the list's version of HitTest.
   */
  float factor = nsPresContext::AppUnitsPerCSSPixel();
  gfx3DMatrix matrix = GetTransform(factor);

  /* If the matrix is singular, or a hidden backface is shown, we didn't hit anything. */
  if (matrix.IsSingular() ||
      (mFrame->GetStyleDisplay()->mBackfaceVisibility == NS_STYLE_BACKFACE_VISIBILITY_HIDDEN &&
       matrix.GetNormalVector().z <= 0.0)) {
          return;
  }

  /* We want to go from transformed-space to regular space.
   * Thus we have to invert the matrix, which normally does
   * the reverse operation (e.g. regular->transformed)
   */

  /* Now, apply the transform and pass it down the channel. */
  nsRect resultingRect;
  if (aRect.width == 1 && aRect.height == 1) {
    gfxPoint point = matrix.Inverse().ProjectPoint(
                       gfxPoint(NSAppUnitsToFloatPixels(aRect.x, factor),
                                NSAppUnitsToFloatPixels(aRect.y, factor)));

    resultingRect = nsRect(NSFloatPixelsToAppUnits(float(point.x), factor),
                           NSFloatPixelsToAppUnits(float(point.y), factor),
                           1, 1);

  } else {
    gfxRect originalRect(NSAppUnitsToFloatPixels(aRect.x, factor),
                         NSAppUnitsToFloatPixels(aRect.y, factor),
                         NSAppUnitsToFloatPixels(aRect.width, factor),
                         NSAppUnitsToFloatPixels(aRect.height, factor));

    gfxRect rect = matrix.Inverse().ProjectRectBounds(originalRect);;

    resultingRect = nsRect(NSFloatPixelsToAppUnits(float(rect.X()), factor),
                           NSFloatPixelsToAppUnits(float(rect.Y()), factor),
                           NSFloatPixelsToAppUnits(float(rect.Width()), factor),
                           NSFloatPixelsToAppUnits(float(rect.Height()), factor));
  }
  

#ifdef DEBUG_HIT
  printf("Frame: %p\n", dynamic_cast<void *>(mFrame));
  printf("  Untransformed point: (%f, %f)\n", resultingRect.X(), resultingRect.Y());
  PRUint32 originalFrameCount = aOutFrames.Length();
#endif

  mStoredList.HitTest(aBuilder, resultingRect, aState, aOutFrames);

#ifdef DEBUG_HIT
  if (originalFrameCount != aOutFrames.Length())
    printf("  Hit! Time: %f, first frame: %p\n", static_cast<double>(clock()),
           dynamic_cast<void *>(aOutFrames.ElementAt(0)));
  printf("=== end of hit test ===\n");
#endif

}

/* The bounding rectangle for the object is the overflow rectangle translated
 * by the reference point.
 */
nsRect nsDisplayTransform::GetBounds(nsDisplayListBuilder *aBuilder)
{
  return TransformRect(mStoredList.GetBounds(aBuilder), mFrame, ToReferenceFrame());
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
nsRegion nsDisplayTransform::GetOpaqueRegion(nsDisplayListBuilder *aBuilder,
                                             PRBool* aForceTransparentSurface)
{
  if (aForceTransparentSurface) {
    *aForceTransparentSurface = PR_FALSE;
  }
  nsRect untransformedVisible;
  if (!UntransformRect(mVisibleRect, mFrame, ToReferenceFrame(), &untransformedVisible)) {
      return nsRegion();
  }
  
  const gfx3DMatrix& matrix = GetTransform(nsPresContext::AppUnitsPerCSSPixel());
                
  nsRegion result;
  gfxMatrix matrix2d;
  if (matrix.Is2D(&matrix2d) &&
      matrix2d.PreservesAxisAlignedRectangles() &&
      mStoredList.GetOpaqueRegion(aBuilder).Contains(untransformedVisible)) {
    result = mVisibleRect;
  }
  return result;
}

/* The transform is uniform if it fills the entire bounding rect and the
 * wrapped list is uniform.  See GetOpaqueRegion for discussion of why this
 * works.
 */
PRBool nsDisplayTransform::IsUniform(nsDisplayListBuilder *aBuilder, nscolor* aColor)
{
  nsRect untransformedVisible;
  if (!UntransformRect(mVisibleRect, mFrame, ToReferenceFrame(), &untransformedVisible)) {
    return PR_FALSE;
  }
  const gfx3DMatrix& matrix = GetTransform(nsPresContext::AppUnitsPerCSSPixel());

  gfxMatrix matrix2d;
  return matrix.Is2D(&matrix2d) &&
         matrix2d.PreservesAxisAlignedRectangles() &&
         mStoredList.GetVisibleRect().Contains(untransformedVisible) &&
         mStoredList.IsUniform(aBuilder, aColor);
}

/* If UNIFIED_CONTINUATIONS is defined, we can merge two display lists that
 * share the same underlying content.  Otherwise, doing so results in graphical
 * glitches.
 */
#ifndef UNIFIED_CONTINUATIONS

PRBool
nsDisplayTransform::TryMerge(nsDisplayListBuilder *aBuilder,
                             nsDisplayItem *aItem)
{
  return PR_FALSE;
}

#else

PRBool
nsDisplayTransform::TryMerge(nsDisplayListBuilder *aBuilder,
                             nsDisplayItem *aItem)
{
  NS_PRECONDITION(aItem, "Why did you try merging with a null item?");
  NS_PRECONDITION(aBuilder, "Why did you try merging with a null builder?");

  /* Make sure that we're dealing with two transforms. */
  if (aItem->GetType() != TYPE_TRANSFORM)
    return PR_FALSE;

  /* Check to see that both frames are part of the same content. */
  if (aItem->GetUnderlyingFrame()->GetContent() != mFrame->GetContent())
    return PR_FALSE;

  /* Now, move everything over to this frame and signal that
   * we merged things!
   */
  mStoredList.GetList()->
    AppendToBottom(&static_cast<nsDisplayTransform *>(aItem)->mStoredList);
  return PR_TRUE;
}

#endif

/* TransformRect takes in as parameters a rectangle (in app space) and returns
 * the smallest rectangle (in app space) containing the transformed image of
 * that rectangle.  That is, it takes the four corners of the rectangle,
 * transforms them according to the matrix associated with the specified frame,
 * then returns the smallest rectangle containing the four transformed points.
 *
 * @param aUntransformedBounds The rectangle (in app units) to transform.
 * @param aFrame The frame whose transformation should be applied.
 * @param aOrigin The delta from the frame origin to the coordinate space origin
 * @param aBoundsOverride (optional) Force the frame bounds to be the
 *        specified bounds.
 * @return The smallest rectangle containing the image of the transformed
 *         rectangle.
 */
nsRect nsDisplayTransform::TransformRect(const nsRect &aUntransformedBounds,
                                         const nsIFrame* aFrame,
                                         const nsPoint &aOrigin,
                                         const nsRect* aBoundsOverride)
{
  NS_PRECONDITION(aFrame, "Can't take the transform based on a null frame!");
  NS_PRECONDITION(aFrame->GetStyleDisplay()->HasTransform(),
                  "Cannot transform a rectangle if there's no transformation!");

  float factor = nsPresContext::AppUnitsPerCSSPixel();
  return nsLayoutUtils::MatrixTransformRect
    (aUntransformedBounds,
     GetResultingTransformMatrix(aFrame, aOrigin, factor, aBoundsOverride),
     factor);
}

nsRect nsDisplayTransform::TransformRectOut(const nsRect &aUntransformedBounds,
                                            const nsIFrame* aFrame,
                                            const nsPoint &aOrigin,
                                            const nsRect* aBoundsOverride)
{
  NS_PRECONDITION(aFrame, "Can't take the transform based on a null frame!");
  NS_PRECONDITION(aFrame->GetStyleDisplay()->HasTransform(),
                  "Cannot transform a rectangle if there's no transformation!");

  float factor = nsPresContext::AppUnitsPerCSSPixel();
  return nsLayoutUtils::MatrixTransformRectOut
    (aUntransformedBounds,
     GetResultingTransformMatrix(aFrame, aOrigin, factor, aBoundsOverride),
     factor);
}

PRBool nsDisplayTransform::UntransformRect(const nsRect &aUntransformedBounds,
                                           const nsIFrame* aFrame,
                                           const nsPoint &aOrigin,
                                           nsRect* aOutRect)
{
  NS_PRECONDITION(aFrame, "Can't take the transform based on a null frame!");
  NS_PRECONDITION(aFrame->GetStyleDisplay()->HasTransform(),
                  "Cannot transform a rectangle if there's no transformation!");


  /* Grab the matrix.  If the transform is degenerate, just hand back the
   * empty rect.
   */
  float factor = nsPresContext::AppUnitsPerCSSPixel();
  gfx3DMatrix matrix = GetResultingTransformMatrix(aFrame, aOrigin, factor, nsnull);
  if (matrix.IsSingular())
    return PR_FALSE;

  gfxRect result(NSAppUnitsToFloatPixels(aUntransformedBounds.x, factor),
                 NSAppUnitsToFloatPixels(aUntransformedBounds.y, factor),
                 NSAppUnitsToFloatPixels(aUntransformedBounds.width, factor),
                 NSAppUnitsToFloatPixels(aUntransformedBounds.height, factor));

  /* We want to untransform the matrix, so invert the transformation first! */
  result = matrix.Inverse().ProjectRectBounds(result);

  *aOutRect = nsLayoutUtils::RoundGfxRectToAppRect(result, factor);

  return PR_TRUE;
}

nsDisplaySVGEffects::nsDisplaySVGEffects(nsDisplayListBuilder* aBuilder,
                                         nsIFrame* aFrame, nsDisplayList* aList)
    : nsDisplayWrapList(aBuilder, aFrame, aList), mEffectsFrame(aFrame),
      mBounds(aFrame->GetVisualOverflowRectRelativeToSelf())
{
  MOZ_COUNT_CTOR(nsDisplaySVGEffects);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplaySVGEffects::~nsDisplaySVGEffects()
{
  MOZ_COUNT_DTOR(nsDisplaySVGEffects);
}
#endif

nsRegion nsDisplaySVGEffects::GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                              PRBool* aForceTransparentSurface)
{
  if (aForceTransparentSurface) {
    *aForceTransparentSurface = PR_FALSE;
  }
  return nsRegion();
}

void
nsDisplaySVGEffects::HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                             HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames)
{
  nsPoint rectCenter(aRect.x + aRect.width / 2, aRect.y + aRect.height / 2);
  if (nsSVGIntegrationUtils::HitTestFrameForEffects(mEffectsFrame,
      rectCenter - aBuilder->ToReferenceFrame(mEffectsFrame))) {
    mList.HitTest(aBuilder, aRect, aState, aOutFrames);
  }
}

void nsDisplaySVGEffects::Paint(nsDisplayListBuilder* aBuilder,
                                nsRenderingContext* aCtx)
{
  nsSVGIntegrationUtils::PaintFramesWithEffects(aCtx,
          mEffectsFrame, mVisibleRect, aBuilder, &mList);
}

PRBool nsDisplaySVGEffects::ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                              nsRegion* aVisibleRegion,
                                              const nsRect& aAllowVisibleRegionExpansion) {
  nsPoint offset = aBuilder->ToReferenceFrame(mEffectsFrame);
  nsRect dirtyRect =
    nsSVGIntegrationUtils::GetRequiredSourceForInvalidArea(mEffectsFrame,
                                                           mVisibleRect - offset) +
    offset;

  // Our children may be made translucent or arbitrarily deformed so we should
  // not allow them to subtract area from aVisibleRegion.
  nsRegion childrenVisible(dirtyRect);
  nsRect r;
  r.IntersectRect(dirtyRect, mList.GetBounds(aBuilder));
  mList.ComputeVisibilityForSublist(aBuilder, &childrenVisible, r, nsRect());
  return PR_TRUE;
}

PRBool nsDisplaySVGEffects::TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem)
{
  if (aItem->GetType() != TYPE_SVG_EFFECTS)
    return PR_FALSE;
  // items for the same content element should be merged into a single
  // compositing group
  // aItem->GetUnderlyingFrame() returns non-null because it's nsDisplaySVGEffects
  if (aItem->GetUnderlyingFrame()->GetContent() != mFrame->GetContent())
    return PR_FALSE;
  nsDisplaySVGEffects* other = static_cast<nsDisplaySVGEffects*>(aItem);
  mList.AppendToBottom(&other->mList);
  mBounds.UnionRect(mBounds,
    other->mBounds + other->mEffectsFrame->GetOffsetTo(mEffectsFrame));
  return PR_TRUE;
}

nsDisplayForcePaintOnScroll::nsDisplayForcePaintOnScroll(
    nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
  : nsDisplayItem(aBuilder, aFrame) {
  MOZ_COUNT_CTOR(nsDisplayForcePaintOnScroll);
}

#ifdef NS_BUILD_REFCNT_LOGGING
nsDisplayForcePaintOnScroll::~nsDisplayForcePaintOnScroll() {
  MOZ_COUNT_DTOR(nsDisplayForcePaintOnScroll);
}
#endif

PRBool nsDisplayForcePaintOnScroll::IsVaryingRelativeToMovingFrame(
         nsDisplayListBuilder* aBuilder, nsIFrame* aFrame) {
  return PR_TRUE;
}
