/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Code responsible for managing style changes: tracking what style
 * changes need to happen, scheduling them, and doing them.
 */

#include "RestyleManager.h"
#include "mozilla/EventStates.h"
#include "nsLayoutUtils.h"
#include "GeckoProfiler.h"
#include "nsStyleChangeList.h"
#include "nsRuleProcessorData.h"
#include "nsStyleUtil.h"
#include "nsCSSFrameConstructor.h"
#include "nsSVGEffects.h"
#include "nsCSSRendering.h"
#include "nsAnimationManager.h"
#include "nsTransitionManager.h"
#include "nsViewManager.h"
#include "nsRenderingContext.h"
#include "nsSVGIntegrationUtils.h"
#include "nsCSSAnonBoxes.h"
#include "nsContainerFrame.h"
#include "nsPlaceholderFrame.h"
#include "nsBlockFrame.h"
#include "nsViewportFrame.h"
#include "SVGTextFrame.h"
#include "StickyScrollContainer.h"
#include "nsIRootBox.h"
#include "nsIDOMMutationEvent.h"
#include "nsContentUtils.h"
#include "nsIFrameInlines.h"
#include "ActiveLayerTracker.h"
#include "nsDisplayList.h"

#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#endif

namespace mozilla {

using namespace layers;

RestyleManager::RestyleManager(nsPresContext* aPresContext)
  : mPresContext(aPresContext)
  , mRebuildAllStyleData(false)
  , mObservingRefreshDriver(false)
  , mInStyleRefresh(false)
  , mHoverGeneration(0)
  , mRebuildAllExtraHint(nsChangeHint(0))
  , mAnimationGeneration(0)
  , mPendingRestyles(ELEMENT_HAS_PENDING_RESTYLE |
                     ELEMENT_IS_POTENTIAL_RESTYLE_ROOT)
  , mPendingAnimationRestyles(ELEMENT_HAS_PENDING_ANIMATION_RESTYLE |
                              ELEMENT_IS_POTENTIAL_ANIMATION_RESTYLE_ROOT)
{
  mPendingRestyles.Init(this);
  mPendingAnimationRestyles.Init(this);
}

void
RestyleManager::NotifyDestroyingFrame(nsIFrame* aFrame)
{
  mOverflowChangedTracker.RemoveFrame(aFrame);
}

#ifdef DEBUG
  // To ensure that the functions below are only called within
  // |ApplyRenderingChangeToTree|.
static bool gInApplyRenderingChangeToTree = false;
#endif

static void
DoApplyRenderingChangeToTree(nsIFrame* aFrame,
                             nsChangeHint aChange);

/**
 * Sync views on aFrame and all of aFrame's descendants (following placeholders),
 * if aChange has nsChangeHint_SyncFrameView.
 * Calls DoApplyRenderingChangeToTree on all aFrame's out-of-flow descendants
 * (following placeholders), if aChange has nsChangeHint_RepaintFrame.
 * aFrame should be some combination of nsChangeHint_SyncFrameView and
 * nsChangeHint_RepaintFrame and nsChangeHint_UpdateOpacityLayer, nothing else.
*/
static void
SyncViewsAndInvalidateDescendants(nsIFrame* aFrame,
                                  nsChangeHint aChange)
{
  NS_PRECONDITION(gInApplyRenderingChangeToTree,
                  "should only be called within ApplyRenderingChangeToTree");
  NS_ASSERTION(aChange == (aChange & (nsChangeHint_RepaintFrame |
                                      nsChangeHint_SyncFrameView |
                                      nsChangeHint_UpdateOpacityLayer)),
               "Invalid change flag");

  nsView* view = aFrame->GetView();
  if (view) {
    if (aChange & nsChangeHint_SyncFrameView) {
      nsContainerFrame::SyncFrameViewProperties(aFrame->PresContext(),
                                                aFrame, nullptr, view);
    }
  }

  nsIFrame::ChildListIterator lists(aFrame);
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      nsIFrame* child = childFrames.get();
      if (!(child->GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
        // only do frames that don't have placeholders
        if (nsGkAtoms::placeholderFrame == child->GetType()) {
          // do the out-of-flow frame and its continuations
          nsIFrame* outOfFlowFrame =
            nsPlaceholderFrame::GetRealFrameForPlaceholder(child);
          DoApplyRenderingChangeToTree(outOfFlowFrame, aChange);
        } else if (lists.CurrentID() == nsIFrame::kPopupList) {
          DoApplyRenderingChangeToTree(child, aChange);
        } else {  // regular frame
          SyncViewsAndInvalidateDescendants(child, aChange);
        }
      }
    }
  }
}

/**
 * To handle nsChangeHint_ChildrenOnlyTransform we must iterate over the child
 * frames of the SVG frame concerned. This helper function is used to find that
 * SVG frame when we encounter nsChangeHint_ChildrenOnlyTransform to ensure
 * that we iterate over the intended children, since sometimes we end up
 * handling that hint while processing hints for one of the SVG frame's
 * ancestor frames.
 *
 * The reason that we sometimes end up trying to process the hint for an
 * ancestor of the SVG frame that the hint is intended for is due to the way we
 * process restyle events. ApplyRenderingChangeToTree adjusts the frame from
 * the restyled element's principle frame to one of its ancestor frames based
 * on what nsCSSRendering::FindBackground returns, since the background style
 * may have been propagated up to an ancestor frame. Processing hints using an
 * ancestor frame is fine in general, but nsChangeHint_ChildrenOnlyTransform is
 * a special case since it is intended to update the children of a specific
 * frame.
 */
static nsIFrame*
GetFrameForChildrenOnlyTransformHint(nsIFrame *aFrame)
{
  if (aFrame->GetType() == nsGkAtoms::viewportFrame) {
    // This happens if the root-<svg> is fixed positioned, in which case we
    // can't use aFrame->GetContent() to find the primary frame, since
    // GetContent() returns nullptr for ViewportFrame.
    aFrame = aFrame->GetFirstPrincipalChild();
  }
  // For an nsHTMLScrollFrame, this will get the SVG frame that has the
  // children-only transforms:
  aFrame = aFrame->GetContent()->GetPrimaryFrame();
  if (aFrame->GetType() == nsGkAtoms::svgOuterSVGFrame) {
    aFrame = aFrame->GetFirstPrincipalChild();
    NS_ABORT_IF_FALSE(aFrame->GetType() == nsGkAtoms::svgOuterSVGAnonChildFrame,
                      "Where is the nsSVGOuterSVGFrame's anon child??");
  }
  NS_ABORT_IF_FALSE(aFrame->IsFrameOfType(nsIFrame::eSVG |
                                          nsIFrame::eSVGContainer),
                    "Children-only transforms only expected on SVG frames");
  return aFrame;
}

static void
DoApplyRenderingChangeToTree(nsIFrame* aFrame,
                             nsChangeHint aChange)
{
  NS_PRECONDITION(gInApplyRenderingChangeToTree,
                  "should only be called within ApplyRenderingChangeToTree");

  for ( ; aFrame; aFrame = nsLayoutUtils::GetNextContinuationOrIBSplitSibling(aFrame)) {
    // Invalidate and sync views on all descendant frames, following placeholders.
    // We don't need to update transforms in SyncViewsAndInvalidateDescendants, because
    // there can't be any out-of-flows or popups that need to be transformed;
    // all out-of-flow descendants of the transformed element must also be
    // descendants of the transformed frame.
    SyncViewsAndInvalidateDescendants(aFrame,
      nsChangeHint(aChange & (nsChangeHint_RepaintFrame |
                              nsChangeHint_SyncFrameView |
                              nsChangeHint_UpdateOpacityLayer)));
    // This must be set to true if the rendering change needs to
    // invalidate content.  If it's false, a composite-only paint
    // (empty transaction) will be scheduled.
    bool needInvalidatingPaint = false;

    // if frame has view, will already be invalidated
    if (aChange & nsChangeHint_RepaintFrame) {
      // Note that this whole block will be skipped when painting is suppressed
      // (due to our caller ApplyRendingChangeToTree() discarding the
      // nsChangeHint_RepaintFrame hint).  If you add handling for any other
      // hints within this block, be sure that they too should be ignored when
      // painting is suppressed.
      needInvalidatingPaint = true;
      aFrame->InvalidateFrameSubtree();
      if (aChange & nsChangeHint_UpdateEffects &&
          aFrame->IsFrameOfType(nsIFrame::eSVG) &&
          !(aFrame->GetStateBits() & NS_STATE_IS_OUTER_SVG)) {
        // Need to update our overflow rects:
        nsSVGUtils::ScheduleReflowSVG(aFrame);
      }
    }
    if (aChange & nsChangeHint_UpdateTextPath) {
      if (aFrame->IsSVGText()) {
        // Invalidate and reflow the entire SVGTextFrame:
        NS_ASSERTION(aFrame->GetContent()->IsSVG(nsGkAtoms::textPath),
                     "expected frame for a <textPath> element");
        nsIFrame* text = nsLayoutUtils::GetClosestFrameOfType(
                                                      aFrame,
                                                      nsGkAtoms::svgTextFrame);
        NS_ASSERTION(text, "expected to find an ancestor SVGTextFrame");
        static_cast<SVGTextFrame*>(text)->NotifyGlyphMetricsChange();
      } else {
        NS_ABORT_IF_FALSE(false, "unexpected frame got "
                                 "nsChangeHint_UpdateTextPath");
      }
    }
    if (aChange & nsChangeHint_UpdateOpacityLayer) {
      // FIXME/bug 796697: we can get away with empty transactions for
      // opacity updates in many cases.
      needInvalidatingPaint = true;

      ActiveLayerTracker::NotifyRestyle(aFrame, eCSSProperty_opacity);
      if (nsSVGIntegrationUtils::UsingEffectsForFrame(aFrame)) {
        // SVG effects paints the opacity without using
        // nsDisplayOpacity. We need to invalidate manually.
        aFrame->InvalidateFrameSubtree();
      }
    }
    if ((aChange & nsChangeHint_UpdateTransformLayer) &&
        aFrame->IsTransformed()) {
      ActiveLayerTracker::NotifyRestyle(aFrame, eCSSProperty_transform);
      // If we're not already going to do an invalidating paint, see
      // if we can get away with only updating the transform on a
      // layer for this frame, and not scheduling an invalidating
      // paint.
      if (!needInvalidatingPaint) {
        Layer* layer;
        needInvalidatingPaint |= !aFrame->TryUpdateTransformOnly(&layer);

        if (!needInvalidatingPaint) {
          // Since we're not going to paint, we need to resend animation
          // data to the layer.
          MOZ_ASSERT(layer, "this can't happen if there's no layer");
          nsDisplayListBuilder::AddAnimationsAndTransitionsToLayer(layer,
            nullptr, nullptr, aFrame, eCSSProperty_transform);
        }
      }
    }
    if (aChange & nsChangeHint_ChildrenOnlyTransform) {
      needInvalidatingPaint = true;
      nsIFrame* childFrame =
        GetFrameForChildrenOnlyTransformHint(aFrame)->GetFirstPrincipalChild();
      for ( ; childFrame; childFrame = childFrame->GetNextSibling()) {
        ActiveLayerTracker::NotifyRestyle(childFrame, eCSSProperty_transform);
      }
    }
    aFrame->SchedulePaint(needInvalidatingPaint ?
                          nsIFrame::PAINT_DEFAULT :
                          nsIFrame::PAINT_COMPOSITE_ONLY);
  }
}

static void
ApplyRenderingChangeToTree(nsPresContext* aPresContext,
                           nsIFrame* aFrame,
                           nsChangeHint aChange)
{
  // We check StyleDisplay()->HasTransform() in addition to checking
  // IsTransformed() since we can get here for some frames that don't support
  // CSS transforms.
  NS_ASSERTION(!(aChange & nsChangeHint_UpdateTransformLayer) ||
               aFrame->IsTransformed() ||
               aFrame->StyleDisplay()->HasTransformStyle(),
               "Unexpected UpdateTransformLayer hint");

  nsIPresShell *shell = aPresContext->PresShell();
  if (shell->IsPaintingSuppressed()) {
    // Don't allow synchronous rendering changes when painting is turned off.
    aChange = NS_SubtractHint(aChange, nsChangeHint_RepaintFrame);
    if (!aChange) {
      return;
    }
  }

  // If the frame's background is propagated to an ancestor, walk up to
  // that ancestor.
  nsStyleContext *bgSC;
  while (!nsCSSRendering::FindBackground(aFrame, &bgSC)) {
    aFrame = aFrame->GetParent();
    NS_ASSERTION(aFrame, "root frame must paint");
  }

  // Trigger rendering updates by damaging this frame and any
  // continuations of this frame.

  // XXX this needs to detect the need for a view due to an opacity change and deal with it...

#ifdef DEBUG
  gInApplyRenderingChangeToTree = true;
#endif
  DoApplyRenderingChangeToTree(aFrame, aChange);
#ifdef DEBUG
  gInApplyRenderingChangeToTree = false;
#endif
}

bool
RestyleManager::RecomputePosition(nsIFrame* aFrame)
{
  // Don't process position changes on table frames, since we already handle
  // the dynamic position change on the outer table frame, and the reflow-based
  // fallback code path also ignores positions on inner table frames.
  if (aFrame->GetType() == nsGkAtoms::tableFrame) {
    return true;
  }

  const nsStyleDisplay* display = aFrame->StyleDisplay();
  // Changes to the offsets of a non-positioned element can safely be ignored.
  if (display->mPosition == NS_STYLE_POSITION_STATIC) {
    return true;
  }

  // Don't process position changes on frames which have views or the ones which
  // have a view somewhere in their descendants, because the corresponding view
  // needs to be repositioned properly as well.
  if (aFrame->HasView() ||
      (aFrame->GetStateBits() & NS_FRAME_HAS_CHILD_WITH_VIEW)) {
    StyleChangeReflow(aFrame, nsChangeHint_NeedReflow);
    return false;
  }

  aFrame->SchedulePaint();

  // For relative positioning, we can simply update the frame rect
  if (display->IsRelativelyPositionedStyle()) {
    if (display->IsInnerTableStyle()) {
      // We don't currently support relative positioning of inner table
      // elements (bug 35168).  If we apply offsets to things we haven't
      // previously offset, we'll get confused.  So bail.
      return true;
    }


    // Move the frame
    if (display->mPosition == NS_STYLE_POSITION_STICKY) {
      // Update sticky positioning for an entire element at once, starting with
      // the first continuation or ib-split sibling.
      // It's rare that the frame we already have isn't already the first
      // continuation or ib-split sibling, but it can happen when styles differ
      // across continuations such as ::first-line or ::first-letter, and in
      // those cases we will generally (but maybe not always) do the work twice.
      nsIFrame *firstContinuation =
        nsLayoutUtils::FirstContinuationOrIBSplitSibling(aFrame);

      StickyScrollContainer::ComputeStickyOffsets(firstContinuation);
      StickyScrollContainer* ssc =
        StickyScrollContainer::GetStickyScrollContainerForFrame(firstContinuation);
      if (ssc) {
        ssc->PositionContinuations(firstContinuation);
      }
    } else {
      MOZ_ASSERT(NS_STYLE_POSITION_RELATIVE == display->mPosition,
                 "Unexpected type of positioning");
      for (nsIFrame *cont = aFrame; cont;
           cont = nsLayoutUtils::GetNextContinuationOrIBSplitSibling(cont)) {
        nsIFrame* cb = cont->GetContainingBlock();
        nsMargin newOffsets;
        const nsSize size = cb->GetContentRectRelativeToSelf().Size();

        nsHTMLReflowState::ComputeRelativeOffsets(
            cb->StyleVisibility()->mDirection,
            cont, size.width, size.height, newOffsets);
        NS_ASSERTION(newOffsets.left == -newOffsets.right &&
                     newOffsets.top == -newOffsets.bottom,
                     "ComputeRelativeOffsets should return valid results");

        // nsHTMLReflowState::ApplyRelativePositioning would work here, but
        // since we've already checked mPosition and aren't changing the frame's
        // normal position, go ahead and add the offsets directly.
        cont->SetPosition(cont->GetNormalPosition() +
                          nsPoint(newOffsets.left, newOffsets.top));
      }
    }

    return true;
  }

  // For the absolute positioning case, set up a fake HTML reflow state for
  // the frame, and then get the offsets and size from it. If the frame's size
  // doesn't need to change, we can simply update the frame position. Otherwise
  // we fall back to a reflow.
  nsRefPtr<nsRenderingContext> rc =
    aFrame->PresContext()->PresShell()->CreateReferenceRenderingContext();

  // Construct a bogus parent reflow state so that there's a usable
  // containing block reflow state.
  nsIFrame* parentFrame = aFrame->GetParent();
  nsSize parentSize = parentFrame->GetSize();

  nsFrameState savedState = parentFrame->GetStateBits();
  nsHTMLReflowState parentReflowState(aFrame->PresContext(), parentFrame,
                                      rc, parentSize);
  parentFrame->RemoveStateBits(~nsFrameState(0));
  parentFrame->AddStateBits(savedState);

  NS_WARN_IF_FALSE(parentSize.width != NS_INTRINSICSIZE &&
                   parentSize.height != NS_INTRINSICSIZE,
                   "parentSize should be valid");
  parentReflowState.SetComputedWidth(std::max(parentSize.width, 0));
  parentReflowState.SetComputedHeight(std::max(parentSize.height, 0));
  parentReflowState.ComputedPhysicalMargin().SizeTo(0, 0, 0, 0);

  parentReflowState.ComputedPhysicalPadding() = parentFrame->GetUsedPadding();
  parentReflowState.ComputedPhysicalBorderPadding() =
    parentFrame->GetUsedBorderAndPadding();
  nsSize availSize(parentSize.width, NS_INTRINSICSIZE);

  ViewportFrame* viewport = do_QueryFrame(parentFrame);
  nsSize cbSize = viewport ?
    viewport->AdjustReflowStateAsContainingBlock(&parentReflowState).Size()
    : aFrame->GetContainingBlock()->GetSize();
  const nsMargin& parentBorder =
    parentReflowState.mStyleBorder->GetComputedBorder();
  cbSize -= nsSize(parentBorder.LeftRight(), parentBorder.TopBottom());
  nsHTMLReflowState reflowState(aFrame->PresContext(), parentReflowState,
                                aFrame, availSize, cbSize.width,
                                cbSize.height);
  nsSize computedSize(reflowState.ComputedWidth(), reflowState.ComputedHeight());
  computedSize.width += reflowState.ComputedPhysicalBorderPadding().LeftRight();
  if (computedSize.height != NS_INTRINSICSIZE) {
    computedSize.height += reflowState.ComputedPhysicalBorderPadding().TopBottom();
  }
  nsSize size = aFrame->GetSize();
  // The RecomputePosition hint is not used if any offset changed between auto
  // and non-auto. If computedSize.height == NS_INTRINSICSIZE then the new
  // element height will be its intrinsic height, and since 'top' and 'bottom''s
  // auto-ness hasn't changed, the old height must also be its intrinsic
  // height, which we can assume hasn't changed (or reflow would have
  // been triggered).
  if (computedSize.width == size.width &&
      (computedSize.height == NS_INTRINSICSIZE || computedSize.height == size.height)) {
    // If we're solving for 'left' or 'top', then compute it here, in order to
    // match the reflow code path.
    if (NS_AUTOOFFSET == reflowState.ComputedPhysicalOffsets().left) {
      reflowState.ComputedPhysicalOffsets().left = cbSize.width -
                                          reflowState.ComputedPhysicalOffsets().right -
                                          reflowState.ComputedPhysicalMargin().right -
                                          size.width -
                                          reflowState.ComputedPhysicalMargin().left;
    }

    if (NS_AUTOOFFSET == reflowState.ComputedPhysicalOffsets().top) {
      reflowState.ComputedPhysicalOffsets().top = cbSize.height -
                                         reflowState.ComputedPhysicalOffsets().bottom -
                                         reflowState.ComputedPhysicalMargin().bottom -
                                         size.height -
                                         reflowState.ComputedPhysicalMargin().top;
    }

    // Move the frame
    nsPoint pos(parentBorder.left + reflowState.ComputedPhysicalOffsets().left +
                reflowState.ComputedPhysicalMargin().left,
                parentBorder.top + reflowState.ComputedPhysicalOffsets().top +
                reflowState.ComputedPhysicalMargin().top);
    aFrame->SetPosition(pos);

    return true;
  }

  // Fall back to a reflow
  StyleChangeReflow(aFrame, nsChangeHint_NeedReflow);
  return false;
}

void
RestyleManager::StyleChangeReflow(nsIFrame* aFrame, nsChangeHint aHint)
{
  // If the frame hasn't even received an initial reflow, then don't
  // send it a style-change reflow!
  if (aFrame->GetStateBits() & NS_FRAME_FIRST_REFLOW)
    return;

  nsIPresShell::IntrinsicDirty dirtyType;
  if (aHint & nsChangeHint_ClearDescendantIntrinsics) {
    NS_ASSERTION(aHint & nsChangeHint_ClearAncestorIntrinsics,
                 "Please read the comments in nsChangeHint.h");
    dirtyType = nsIPresShell::eStyleChange;
  } else if (aHint & nsChangeHint_ClearAncestorIntrinsics) {
    dirtyType = nsIPresShell::eTreeChange;
  } else {
    dirtyType = nsIPresShell::eResize;
  }

  nsFrameState dirtyBits;
  if (aHint & nsChangeHint_NeedDirtyReflow) {
    dirtyBits = NS_FRAME_IS_DIRTY;
  } else {
    dirtyBits = NS_FRAME_HAS_DIRTY_CHILDREN;
  }

  do {
    mPresContext->PresShell()->FrameNeedsReflow(aFrame, dirtyType, dirtyBits);
    aFrame = nsLayoutUtils::GetNextContinuationOrIBSplitSibling(aFrame);
  } while (aFrame);

  return;
}

void
RestyleManager::AddSubtreeToOverflowTracker(nsIFrame* aFrame) 
{
  mOverflowChangedTracker.AddFrame(
      aFrame,
      OverflowChangedTracker::CHILDREN_AND_PARENT_CHANGED);
  nsIFrame::ChildListIterator lists(aFrame);
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      nsIFrame* child = childFrames.get();
      AddSubtreeToOverflowTracker(child);
    }
  }
}

NS_DECLARE_FRAME_PROPERTY(ChangeListProperty, nullptr)

/**
 * Return true if aFrame's subtree has placeholders for out-of-flow content
 * whose 'position' style's bit in aPositionMask is set.
 */
static bool
FrameHasPositionedPlaceholderDescendants(nsIFrame* aFrame, uint32_t aPositionMask)
{
  const nsIFrame::ChildListIDs skip(nsIFrame::kAbsoluteList |
                                    nsIFrame::kFixedList);
  for (nsIFrame::ChildListIterator lists(aFrame); !lists.IsDone(); lists.Next()) {
    if (!skip.Contains(lists.CurrentID())) {
      for (nsFrameList::Enumerator childFrames(lists.CurrentList());
           !childFrames.AtEnd(); childFrames.Next()) {
        nsIFrame* f = childFrames.get();
        if (f->GetType() == nsGkAtoms::placeholderFrame) {
          nsIFrame* outOfFlow = nsPlaceholderFrame::GetRealFrameForPlaceholder(f);
          // If SVG text frames could appear here, they could confuse us since
          // they ignore their position style ... but they can't.
          NS_ASSERTION(!outOfFlow->IsSVGText(),
                       "SVG text frames can't be out of flow");
          if (aPositionMask & (1 << outOfFlow->StyleDisplay()->mPosition)) {
            return true;
          }
        }
        if (FrameHasPositionedPlaceholderDescendants(f, aPositionMask)) {
          return true;
        }
      }
    }
  }
  return false;
}

static bool
NeedToReframeForAddingOrRemovingTransform(nsIFrame* aFrame)
{
  static_assert(0 <= NS_STYLE_POSITION_ABSOLUTE &&
                NS_STYLE_POSITION_ABSOLUTE < 32, "Style constant out of range");
  static_assert(0 <= NS_STYLE_POSITION_FIXED &&
                NS_STYLE_POSITION_FIXED < 32, "Style constant out of range");

  uint32_t positionMask;
  // Don't call aFrame->IsPositioned here, since that returns true if
  // the frame already has a transform, and we want to ignore that here
  if (aFrame->IsAbsolutelyPositioned() ||
      aFrame->IsRelativelyPositioned()) {
    // This frame is a container for abs-pos descendants whether or not it
    // has a transform.
    // So abs-pos descendants are no problem; we only need to reframe if
    // we have fixed-pos descendants.
    positionMask = 1 << NS_STYLE_POSITION_FIXED;
  } else {
    // This frame may not be a container for abs-pos descendants already.
    // So reframe if we have abs-pos or fixed-pos descendants.
    positionMask = (1 << NS_STYLE_POSITION_FIXED) |
        (1 << NS_STYLE_POSITION_ABSOLUTE);
  }
  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetNextContinuationOrIBSplitSibling(f)) {
    if (FrameHasPositionedPlaceholderDescendants(f, positionMask)) {
      return true;
    }
  }
  return false;
}

nsresult
RestyleManager::ProcessRestyledFrames(nsStyleChangeList& aChangeList)
{
  NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
               "Someone forgot a script blocker");
  int32_t count = aChangeList.Count();
  if (!count)
    return NS_OK;

  PROFILER_LABEL("RestyleManager", "ProcessRestyledFrames",
    js::ProfileEntry::Category::CSS);

  // Make sure to not rebuild quote or counter lists while we're
  // processing restyles
  FrameConstructor()->BeginUpdate();

  FramePropertyTable* propTable = mPresContext->PropertyTable();

  // Mark frames so that we skip frames that die along the way, bug 123049.
  // A frame can be in the list multiple times with different hints. Further
  // optmization is possible if nsStyleChangeList::AppendChange could coalesce
  int32_t index = count;

  while (0 <= --index) {
    const nsStyleChangeData* changeData;
    aChangeList.ChangeAt(index, &changeData);
    if (changeData->mFrame) {
      propTable->Set(changeData->mFrame, ChangeListProperty(),
                     NS_INT32_TO_PTR(1));
    }
  }

  index = count;

  bool didUpdateCursor = false;

  while (0 <= --index) {
    nsIFrame* frame;
    nsIContent* content;
    bool didReflowThisFrame = false;
    nsChangeHint hint;
    aChangeList.ChangeAt(index, frame, content, hint);

    NS_ASSERTION(!(hint & nsChangeHint_AllReflowHints) ||
                 (hint & nsChangeHint_NeedReflow),
                 "Reflow hint bits set without actually asking for a reflow");

    // skip any frame that has been destroyed due to a ripple effect
    if (frame && !propTable->Get(frame, ChangeListProperty())) {
      continue;
    }

    if (frame && frame->GetContent() != content) {
      // XXXbz this is due to image maps messing with the primary frame of
      // <area>s.  See bug 135040.  Remove this block once that's fixed.
      frame = nullptr;
      if (!(hint & nsChangeHint_ReconstructFrame)) {
        continue;
      }
    }

    if ((hint & nsChangeHint_AddOrRemoveTransform) && frame &&
        !(hint & nsChangeHint_ReconstructFrame)) {
      if (NeedToReframeForAddingOrRemovingTransform(frame) ||
          frame->GetType() == nsGkAtoms::fieldSetFrame ||
          frame->GetContentInsertionFrame() != frame) {
        // The frame has positioned children that need to be reparented, or
        // it can't easily be converted to/from being an abs-pos container correctly.
        NS_UpdateHint(hint, nsChangeHint_ReconstructFrame);
      } else {
        for (nsIFrame *cont = frame; cont;
             cont = nsLayoutUtils::GetNextContinuationOrIBSplitSibling(cont)) {
          // Normally frame construction would set state bits as needed,
          // but we're not going to reconstruct the frame so we need to set them.
          // It's because we need to set this state on each affected frame
          // that we can't coalesce nsChangeHint_AddOrRemoveTransform hints up
          // to ancestors (i.e. it can't be an inherited change hint).
          if (cont->IsPositioned()) {
            // If a transform has been added, we'll be taking this path,
            // but we may be taking this path even if a transform has been
            // removed. It's OK to add the bit even if it's not needed.
            cont->AddStateBits(NS_FRAME_MAY_BE_TRANSFORMED);
            if (!cont->IsAbsoluteContainer() &&
                (cont->GetStateBits() & NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN)) {
              cont->MarkAsAbsoluteContainingBlock();
            }
          } else {
            // Don't remove NS_FRAME_MAY_BE_TRANSFORMED since it may still by
            // transformed by other means. It's OK to have the bit even if it's
            // not needed.
            if (cont->IsAbsoluteContainer()) {
              cont->MarkAsNotAbsoluteContainingBlock();
            }
          }
        }
      }
    }
    if (hint & nsChangeHint_ReconstructFrame) {
      // If we ever start passing true here, be careful of restyles
      // that involve a reframe and animations.  In particular, if the
      // restyle we're processing here is an animation restyle, but
      // the style resolution we will do for the frame construction
      // happens async when we're not in an animation restyle already,
      // problems could arise.
      FrameConstructor()->RecreateFramesForContent(content, false);
    } else {
      NS_ASSERTION(frame, "This shouldn't happen");

      if ((frame->GetStateBits() & NS_FRAME_SVG_LAYOUT) &&
          (frame->GetStateBits() & NS_FRAME_IS_NONDISPLAY)) {
        // frame does not maintain overflow rects, so avoid calling
        // FinishAndStoreOverflow on it:
        hint = NS_SubtractHint(hint,
                 NS_CombineHint(nsChangeHint_UpdateOverflow,
                   NS_CombineHint(nsChangeHint_ChildrenOnlyTransform,
                                  nsChangeHint_UpdatePostTransformOverflow)));
      }

      if (!(frame->GetStateBits() & NS_FRAME_MAY_BE_TRANSFORMED)) {
        // Frame can not be transformed, and thus a change in transform will
        // have no effect and we should not use the
        // nsChangeHint_UpdatePostTransformOverflow hint.
        hint = NS_SubtractHint(hint, nsChangeHint_UpdatePostTransformOverflow);
      }

      if (hint & nsChangeHint_UpdateEffects) {
        for (nsIFrame *cont = frame; cont;
             cont = nsLayoutUtils::GetNextContinuationOrIBSplitSibling(cont)) {
          nsSVGEffects::UpdateEffects(cont);
        }
      }
      if (hint & nsChangeHint_NeedReflow) {
        StyleChangeReflow(frame, hint);
        didReflowThisFrame = true;
      }

      if (hint & (nsChangeHint_RepaintFrame | nsChangeHint_SyncFrameView |
                  nsChangeHint_UpdateOpacityLayer | nsChangeHint_UpdateTransformLayer |
                  nsChangeHint_ChildrenOnlyTransform)) {
        ApplyRenderingChangeToTree(mPresContext, frame, hint);
      }
      if ((hint & nsChangeHint_RecomputePosition) && !didReflowThisFrame) {
        ActiveLayerTracker::NotifyOffsetRestyle(frame);
        // It is possible for this to fall back to a reflow
        if (!RecomputePosition(frame)) {
          didReflowThisFrame = true;
        }
      }
      NS_ASSERTION(!(hint & nsChangeHint_ChildrenOnlyTransform) ||
                   (hint & nsChangeHint_UpdateOverflow),
                   "nsChangeHint_UpdateOverflow should be passed too");
      if (!didReflowThisFrame &&
          (hint & (nsChangeHint_UpdateOverflow |
                   nsChangeHint_UpdatePostTransformOverflow |
                   nsChangeHint_UpdateSubtreeOverflow))) {
        if (hint & nsChangeHint_UpdateSubtreeOverflow) {
          AddSubtreeToOverflowTracker(frame);
        }
        OverflowChangedTracker::ChangeKind changeKind;
        if (hint & nsChangeHint_ChildrenOnlyTransform) {
          // The overflow areas of the child frames need to be updated:
          nsIFrame* hintFrame = GetFrameForChildrenOnlyTransformHint(frame);
          nsIFrame* childFrame = hintFrame->GetFirstPrincipalChild();
          NS_ASSERTION(!nsLayoutUtils::GetNextContinuationOrIBSplitSibling(frame),
                       "SVG frames should not have continuations "
                       "or ib-split siblings");
          NS_ASSERTION(!nsLayoutUtils::GetNextContinuationOrIBSplitSibling(hintFrame),
                       "SVG frames should not have continuations "
                       "or ib-split siblings");
          for ( ; childFrame; childFrame = childFrame->GetNextSibling()) {
            NS_ABORT_IF_FALSE(childFrame->IsFrameOfType(nsIFrame::eSVG),
                              "Not expecting non-SVG children");
            // If |childFrame| is dirty or has dirty children, we don't bother
            // updating overflows since that will happen when it's reflowed.
            if (!(childFrame->GetStateBits() &
                  (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN))) {
              mOverflowChangedTracker.AddFrame(childFrame,
                           OverflowChangedTracker::CHILDREN_AND_PARENT_CHANGED);
            }
            NS_ASSERTION(!nsLayoutUtils::GetNextContinuationOrIBSplitSibling(childFrame),
                         "SVG frames should not have continuations "
                         "or ib-split siblings");
            NS_ASSERTION(childFrame->GetParent() == hintFrame,
                         "SVG child frame not expected to have different parent");
          }
        }
        // If |frame| is dirty or has dirty children, we don't bother updating
        // overflows since that will happen when it's reflowed.
        if (!(frame->GetStateBits() &
              (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN))) {
          // If we have both nsChangeHint_UpdateOverflow and
          // nsChangeHint_UpdatePostTransformOverflow, CHILDREN_AND_PARENT_CHANGED
          // is selected as it is stronger.
          if (hint & (nsChangeHint_UpdateOverflow | 
                      nsChangeHint_UpdateSubtreeOverflow)) {
            changeKind = OverflowChangedTracker::CHILDREN_AND_PARENT_CHANGED;
          } else {
            changeKind = OverflowChangedTracker::TRANSFORM_CHANGED;
          }
          for (nsIFrame *cont = frame; cont; cont =
                 nsLayoutUtils::GetNextContinuationOrIBSplitSibling(cont)) {
            mOverflowChangedTracker.AddFrame(cont, changeKind);
          }
        }
      }
      if ((hint & nsChangeHint_UpdateCursor) && !didUpdateCursor) {
        mPresContext->PresShell()->SynthesizeMouseMove(false);
        didUpdateCursor = true;
      }
    }
  }

  FrameConstructor()->EndUpdate();

  // cleanup references and verify the style tree.  Note that the latter needs
  // to happen once we've processed the whole list, since until then the tree
  // is not in fact in a consistent state.
  index = count;
  while (0 <= --index) {
    const nsStyleChangeData* changeData;
    aChangeList.ChangeAt(index, &changeData);
    if (changeData->mFrame) {
      propTable->Delete(changeData->mFrame, ChangeListProperty());
    }

#ifdef DEBUG
    // reget frame from content since it may have been regenerated...
    if (changeData->mContent) {
      if (!nsAnimationManager::ContentOrAncestorHasAnimation(changeData->mContent) &&
          !nsTransitionManager::ContentOrAncestorHasTransition(changeData->mContent)) {
        nsIFrame* frame = changeData->mContent->GetPrimaryFrame();
        if (frame) {
          DebugVerifyStyleTree(frame);
        }
      }
    } else if (!changeData->mFrame ||
               changeData->mFrame->GetType() != nsGkAtoms::viewportFrame) {
      NS_WARNING("Unable to test style tree integrity -- no content node "
                 "(and not a viewport frame)");
    }
#endif
  }

  aChangeList.Clear();
  return NS_OK;
}

void
RestyleManager::RestyleElement(Element*        aElement,
                               nsIFrame*       aPrimaryFrame,
                               nsChangeHint    aMinHint,
                               RestyleTracker& aRestyleTracker,
                               nsRestyleHint   aRestyleHint)
{
  NS_ASSERTION(aPrimaryFrame == aElement->GetPrimaryFrame(),
               "frame/content mismatch");
  if (aPrimaryFrame && aPrimaryFrame->GetContent() != aElement) {
    // XXXbz this is due to image maps messing with the primary frame pointer
    // of <area>s.  See bug 135040.  We can remove this block once that's fixed.
    aPrimaryFrame = nullptr;
  }
  NS_ASSERTION(!aPrimaryFrame || aPrimaryFrame->GetContent() == aElement,
               "frame/content mismatch");

  // If we're restyling the root element and there are 'rem' units in
  // use, handle dynamic changes to the definition of a 'rem' here.
  if (mPresContext->UsesRootEMUnits() && aPrimaryFrame) {
    nsStyleContext *oldContext = aPrimaryFrame->StyleContext();
    if (!oldContext->GetParent()) { // check that we're the root element
      nsRefPtr<nsStyleContext> newContext = mPresContext->StyleSet()->
        ResolveStyleFor(aElement, nullptr /* == oldContext->GetParent() */);
      if (oldContext->StyleFont()->mFont.size !=
          newContext->StyleFont()->mFont.size) {
        // The basis for 'rem' units has changed.
        newContext = nullptr;
        DoRebuildAllStyleData(aRestyleTracker, nsChangeHint(0));
        if (aMinHint == 0) {
          return;
        }
        aPrimaryFrame = aElement->GetPrimaryFrame();
      }
    }
  }

  if (aMinHint & nsChangeHint_ReconstructFrame) {
    FrameConstructor()->RecreateFramesForContent(aElement, false);
  } else if (aPrimaryFrame) {
    nsStyleChangeList changeList;
    ComputeStyleChangeFor(aPrimaryFrame, &changeList, aMinHint,
                          aRestyleTracker, aRestyleHint);
    ProcessRestyledFrames(changeList);
  } else {
    // no frames, reconstruct for content
    FrameConstructor()->MaybeRecreateFramesForElement(aElement);
  }
}

static inline dom::Element*
ElementForStyleContext(nsIContent* aParentContent,
                       nsIFrame* aFrame,
                       nsCSSPseudoElements::Type aPseudoType);

// Forwarded nsIDocumentObserver method, to handle restyling (and
// passing the notification to the frame).
nsresult
RestyleManager::ContentStateChanged(nsIContent* aContent,
                                    EventStates aStateMask)
{
  // XXXbz it would be good if this function only took Elements, but
  // we'd have to make ESM guarantee that usefully.
  if (!aContent->IsElement()) {
    return NS_OK;
  }

  Element* aElement = aContent->AsElement();

  nsStyleSet* styleSet = mPresContext->StyleSet();
  NS_ASSERTION(styleSet, "couldn't get style set");

  nsChangeHint hint = NS_STYLE_HINT_NONE;
  // Any change to a content state that affects which frames we construct
  // must lead to a frame reconstruct here if we already have a frame.
  // Note that we never decide through non-CSS means to not create frames
  // based on content states, so if we already don't have a frame we don't
  // need to force a reframe -- if it's needed, the HasStateDependentStyle
  // call will handle things.
  nsIFrame* primaryFrame = aElement->GetPrimaryFrame();
  nsCSSPseudoElements::Type pseudoType =
    nsCSSPseudoElements::ePseudo_NotPseudoElement;
  if (primaryFrame) {
    // If it's generated content, ignore LOADING/etc state changes on it.
    if (!primaryFrame->IsGeneratedContentFrame() &&
        aStateMask.HasAtLeastOneOfStates(NS_EVENT_STATE_BROKEN |
                                         NS_EVENT_STATE_USERDISABLED |
                                         NS_EVENT_STATE_SUPPRESSED |
                                         NS_EVENT_STATE_LOADING)) {
      hint = nsChangeHint_ReconstructFrame;
    } else {
      uint8_t app = primaryFrame->StyleDisplay()->mAppearance;
      if (app) {
        nsITheme *theme = mPresContext->GetTheme();
        if (theme && theme->ThemeSupportsWidget(mPresContext,
                                                primaryFrame, app)) {
          bool repaint = false;
          theme->WidgetStateChanged(primaryFrame, app, nullptr, &repaint);
          if (repaint) {
            NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
          }
        }
      }
    }

    pseudoType = primaryFrame->StyleContext()->GetPseudoType();

    primaryFrame->ContentStatesChanged(aStateMask);
  }


  nsRestyleHint rshint;

  if (pseudoType >= nsCSSPseudoElements::ePseudo_PseudoElementCount) {
    rshint = styleSet->HasStateDependentStyle(mPresContext, aElement,
                                              aStateMask);
  } else if (nsCSSPseudoElements::PseudoElementSupportsUserActionState(
                                                                  pseudoType)) {
    // If aElement is a pseudo-element, we want to check to see whether there
    // are any state-dependent rules applying to that pseudo.
    Element* ancestor = ElementForStyleContext(nullptr, primaryFrame,
                                               pseudoType);
    rshint = styleSet->HasStateDependentStyle(mPresContext, ancestor,
                                              pseudoType, aElement,
                                              aStateMask);
  } else {
    rshint = nsRestyleHint(0);
  }

  if (aStateMask.HasState(NS_EVENT_STATE_HOVER) && rshint != 0) {
    ++mHoverGeneration;
  }

  if (aStateMask.HasState(NS_EVENT_STATE_VISITED)) {
    // Exposing information to the page about whether the link is
    // visited or not isn't really something we can worry about here.
    // FIXME: We could probably do this a bit better.
    NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
  }

  PostRestyleEvent(aElement, rshint, hint);
  return NS_OK;
}

// Forwarded nsIMutationObserver method, to handle restyling.
void
RestyleManager::AttributeWillChange(Element* aElement,
                                    int32_t aNameSpaceID,
                                    nsIAtom* aAttribute,
                                    int32_t aModType)
{
  nsRestyleHint rshint =
    mPresContext->StyleSet()->HasAttributeDependentStyle(mPresContext,
                                                         aElement,
                                                         aAttribute,
                                                         aModType,
                                                         false);
  PostRestyleEvent(aElement, rshint, NS_STYLE_HINT_NONE);
}

// Forwarded nsIMutationObserver method, to handle restyling (and
// passing the notification to the frame).
void
RestyleManager::AttributeChanged(Element* aElement,
                                 int32_t aNameSpaceID,
                                 nsIAtom* aAttribute,
                                 int32_t aModType)
{
  // Hold onto the PresShell to prevent ourselves from being destroyed.
  // XXXbz how, exactly, would this attribute change cause us to be
  // destroyed from inside this function?
  nsCOMPtr<nsIPresShell> shell = mPresContext->GetPresShell();

  // Get the frame associated with the content which is the highest in the frame tree
  nsIFrame* primaryFrame = aElement->GetPrimaryFrame();

#if 0
  NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
     ("RestyleManager::AttributeChanged: content=%p[%s] frame=%p",
      aContent, ContentTag(aElement, 0), frame));
#endif

  // the style tag has its own interpretation based on aHint
  nsChangeHint hint = aElement->GetAttributeChangeHint(aAttribute, aModType);

  bool reframe = (hint & nsChangeHint_ReconstructFrame) != 0;

#ifdef MOZ_XUL
  // The following listbox widget trap prevents offscreen listbox widget
  // content from being removed and re-inserted (which is what would
  // happen otherwise).
  if (!primaryFrame && !reframe) {
    int32_t namespaceID;
    nsIAtom* tag = mPresContext->Document()->BindingManager()->
                     ResolveTag(aElement, &namespaceID);

    if (namespaceID == kNameSpaceID_XUL &&
        (tag == nsGkAtoms::listitem ||
         tag == nsGkAtoms::listcell))
      return;
  }

  if (aAttribute == nsGkAtoms::tooltiptext ||
      aAttribute == nsGkAtoms::tooltip)
  {
    nsIRootBox* rootBox = nsIRootBox::GetRootBox(mPresContext->GetPresShell());
    if (rootBox) {
      if (aModType == nsIDOMMutationEvent::REMOVAL)
        rootBox->RemoveTooltipSupport(aElement);
      if (aModType == nsIDOMMutationEvent::ADDITION)
        rootBox->AddTooltipSupport(aElement);
    }
  }

#endif // MOZ_XUL

  if (primaryFrame) {
    // See if we have appearance information for a theme.
    const nsStyleDisplay* disp = primaryFrame->StyleDisplay();
    if (disp->mAppearance) {
      nsITheme *theme = mPresContext->GetTheme();
      if (theme && theme->ThemeSupportsWidget(mPresContext, primaryFrame, disp->mAppearance)) {
        bool repaint = false;
        theme->WidgetStateChanged(primaryFrame, disp->mAppearance, aAttribute, &repaint);
        if (repaint)
          NS_UpdateHint(hint, nsChangeHint_RepaintFrame);
      }
    }

    // let the frame deal with it now, so we don't have to deal later
    primaryFrame->AttributeChanged(aNameSpaceID, aAttribute, aModType);
    // XXXwaterson should probably check for IB split siblings
    // here, and propagate the AttributeChanged notification to
    // them, as well. Currently, inline frames don't do anything on
    // this notification, so it's not that big a deal.
  }

  // See if we can optimize away the style re-resolution -- must be called after
  // the frame's AttributeChanged() in case it does something that affects the style
  nsRestyleHint rshint =
    mPresContext->StyleSet()->HasAttributeDependentStyle(mPresContext,
                                                         aElement,
                                                         aAttribute,
                                                         aModType,
                                                         true);

  PostRestyleEvent(aElement, rshint, hint);
}

void
RestyleManager::RestyleForEmptyChange(Element* aContainer)
{
  // In some cases (:empty + E, :empty ~ E), a change if the content of
  // an element requires restyling its parent's siblings.
  nsRestyleHint hint = eRestyle_Subtree;
  nsIContent* grandparent = aContainer->GetParent();
  if (grandparent &&
      (grandparent->GetFlags() & NODE_HAS_SLOW_SELECTOR_LATER_SIBLINGS)) {
    hint = nsRestyleHint(hint | eRestyle_LaterSiblings);
  }
  PostRestyleEvent(aContainer, hint, NS_STYLE_HINT_NONE);
}

void
RestyleManager::RestyleForAppend(Element* aContainer,
                                 nsIContent* aFirstNewContent)
{
  NS_ASSERTION(aContainer, "must have container for append");
#ifdef DEBUG
  {
    for (nsIContent* cur = aFirstNewContent; cur; cur = cur->GetNextSibling()) {
      NS_ASSERTION(!cur->IsRootOfAnonymousSubtree(),
                   "anonymous nodes should not be in child lists");
    }
  }
#endif
  uint32_t selectorFlags =
    aContainer->GetFlags() & (NODE_ALL_SELECTOR_FLAGS &
                              ~NODE_HAS_SLOW_SELECTOR_LATER_SIBLINGS);
  if (selectorFlags == 0)
    return;

  if (selectorFlags & NODE_HAS_EMPTY_SELECTOR) {
    // see whether we need to restyle the container
    bool wasEmpty = true; // :empty or :-moz-only-whitespace
    for (nsIContent* cur = aContainer->GetFirstChild();
         cur != aFirstNewContent;
         cur = cur->GetNextSibling()) {
      // We don't know whether we're testing :empty or :-moz-only-whitespace,
      // so be conservative and assume :-moz-only-whitespace (i.e., make
      // IsSignificantChild less likely to be true, and thus make us more
      // likely to restyle).
      if (nsStyleUtil::IsSignificantChild(cur, true, false)) {
        wasEmpty = false;
        break;
      }
    }
    if (wasEmpty) {
      RestyleForEmptyChange(aContainer);
      return;
    }
  }

  if (selectorFlags & NODE_HAS_SLOW_SELECTOR) {
    PostRestyleEvent(aContainer, eRestyle_Subtree, NS_STYLE_HINT_NONE);
    // Restyling the container is the most we can do here, so we're done.
    return;
  }

  if (selectorFlags & NODE_HAS_EDGE_CHILD_SELECTOR) {
    // restyle the last element child before this node
    for (nsIContent* cur = aFirstNewContent->GetPreviousSibling();
         cur;
         cur = cur->GetPreviousSibling()) {
      if (cur->IsElement()) {
        PostRestyleEvent(cur->AsElement(), eRestyle_Subtree, NS_STYLE_HINT_NONE);
        break;
      }
    }
  }
}

// Needed since we can't use PostRestyleEvent on non-elements (with
// eRestyle_LaterSiblings or nsRestyleHint(eRestyle_Subtree |
// eRestyle_LaterSiblings) as appropriate).
static void
RestyleSiblingsStartingWith(RestyleManager* aRestyleManager,
                            nsIContent* aStartingSibling /* may be null */)
{
  for (nsIContent *sibling = aStartingSibling; sibling;
       sibling = sibling->GetNextSibling()) {
    if (sibling->IsElement()) {
      aRestyleManager->
        PostRestyleEvent(sibling->AsElement(),
                         nsRestyleHint(eRestyle_Subtree | eRestyle_LaterSiblings),
                         NS_STYLE_HINT_NONE);
      break;
    }
  }
}

// Restyling for a ContentInserted or CharacterDataChanged notification.
// This could be used for ContentRemoved as well if we got the
// notification before the removal happened (and sometimes
// CharacterDataChanged is more like a removal than an addition).
// The comments are written and variables are named in terms of it being
// a ContentInserted notification.
void
RestyleManager::RestyleForInsertOrChange(Element* aContainer,
                                         nsIContent* aChild)
{
  NS_ASSERTION(!aChild->IsRootOfAnonymousSubtree(),
               "anonymous nodes should not be in child lists");
  uint32_t selectorFlags =
    aContainer ? (aContainer->GetFlags() & NODE_ALL_SELECTOR_FLAGS) : 0;
  if (selectorFlags == 0)
    return;

  if (selectorFlags & NODE_HAS_EMPTY_SELECTOR) {
    // see whether we need to restyle the container
    bool wasEmpty = true; // :empty or :-moz-only-whitespace
    for (nsIContent* child = aContainer->GetFirstChild();
         child;
         child = child->GetNextSibling()) {
      if (child == aChild)
        continue;
      // We don't know whether we're testing :empty or :-moz-only-whitespace,
      // so be conservative and assume :-moz-only-whitespace (i.e., make
      // IsSignificantChild less likely to be true, and thus make us more
      // likely to restyle).
      if (nsStyleUtil::IsSignificantChild(child, true, false)) {
        wasEmpty = false;
        break;
      }
    }
    if (wasEmpty) {
      RestyleForEmptyChange(aContainer);
      return;
    }
  }

  if (selectorFlags & NODE_HAS_SLOW_SELECTOR) {
    PostRestyleEvent(aContainer, eRestyle_Subtree, NS_STYLE_HINT_NONE);
    // Restyling the container is the most we can do here, so we're done.
    return;
  }

  if (selectorFlags & NODE_HAS_SLOW_SELECTOR_LATER_SIBLINGS) {
    // Restyle all later siblings.
    RestyleSiblingsStartingWith(this, aChild->GetNextSibling());
  }

  if (selectorFlags & NODE_HAS_EDGE_CHILD_SELECTOR) {
    // restyle the previously-first element child if it is after this node
    bool passedChild = false;
    for (nsIContent* content = aContainer->GetFirstChild();
         content;
         content = content->GetNextSibling()) {
      if (content == aChild) {
        passedChild = true;
        continue;
      }
      if (content->IsElement()) {
        if (passedChild) {
          PostRestyleEvent(content->AsElement(), eRestyle_Subtree,
                           NS_STYLE_HINT_NONE);
        }
        break;
      }
    }
    // restyle the previously-last element child if it is before this node
    passedChild = false;
    for (nsIContent* content = aContainer->GetLastChild();
         content;
         content = content->GetPreviousSibling()) {
      if (content == aChild) {
        passedChild = true;
        continue;
      }
      if (content->IsElement()) {
        if (passedChild) {
          PostRestyleEvent(content->AsElement(), eRestyle_Subtree,
                           NS_STYLE_HINT_NONE);
        }
        break;
      }
    }
  }
}

void
RestyleManager::RestyleForRemove(Element* aContainer,
                                 nsIContent* aOldChild,
                                 nsIContent* aFollowingSibling)
{
  if (aOldChild->IsRootOfAnonymousSubtree()) {
    // This should be an assert, but this is called incorrectly in
    // nsHTMLEditor::DeleteRefToAnonymousNode and the assertions were clogging
    // up the logs.  Make it an assert again when that's fixed.
    NS_WARNING("anonymous nodes should not be in child lists (bug 439258)");
  }
  uint32_t selectorFlags =
    aContainer ? (aContainer->GetFlags() & NODE_ALL_SELECTOR_FLAGS) : 0;
  if (selectorFlags == 0)
    return;

  if (selectorFlags & NODE_HAS_EMPTY_SELECTOR) {
    // see whether we need to restyle the container
    bool isEmpty = true; // :empty or :-moz-only-whitespace
    for (nsIContent* child = aContainer->GetFirstChild();
         child;
         child = child->GetNextSibling()) {
      // We don't know whether we're testing :empty or :-moz-only-whitespace,
      // so be conservative and assume :-moz-only-whitespace (i.e., make
      // IsSignificantChild less likely to be true, and thus make us more
      // likely to restyle).
      if (nsStyleUtil::IsSignificantChild(child, true, false)) {
        isEmpty = false;
        break;
      }
    }
    if (isEmpty) {
      RestyleForEmptyChange(aContainer);
      return;
    }
  }

  if (selectorFlags & NODE_HAS_SLOW_SELECTOR) {
    PostRestyleEvent(aContainer, eRestyle_Subtree, NS_STYLE_HINT_NONE);
    // Restyling the container is the most we can do here, so we're done.
    return;
  }

  if (selectorFlags & NODE_HAS_SLOW_SELECTOR_LATER_SIBLINGS) {
    // Restyle all later siblings.
    RestyleSiblingsStartingWith(this, aFollowingSibling);
  }

  if (selectorFlags & NODE_HAS_EDGE_CHILD_SELECTOR) {
    // restyle the now-first element child if it was after aOldChild
    bool reachedFollowingSibling = false;
    for (nsIContent* content = aContainer->GetFirstChild();
         content;
         content = content->GetNextSibling()) {
      if (content == aFollowingSibling) {
        reachedFollowingSibling = true;
        // do NOT continue here; we might want to restyle this node
      }
      if (content->IsElement()) {
        if (reachedFollowingSibling) {
          PostRestyleEvent(content->AsElement(), eRestyle_Subtree,
                           NS_STYLE_HINT_NONE);
        }
        break;
      }
    }
    // restyle the now-last element child if it was before aOldChild
    reachedFollowingSibling = (aFollowingSibling == nullptr);
    for (nsIContent* content = aContainer->GetLastChild();
         content;
         content = content->GetPreviousSibling()) {
      if (content->IsElement()) {
        if (reachedFollowingSibling) {
          PostRestyleEvent(content->AsElement(), eRestyle_Subtree, NS_STYLE_HINT_NONE);
        }
        break;
      }
      if (content == aFollowingSibling) {
        reachedFollowingSibling = true;
      }
    }
  }
}

void
RestyleManager::RebuildAllStyleData(nsChangeHint aExtraHint)
{
  NS_ASSERTION(!(aExtraHint & nsChangeHint_ReconstructFrame),
               "Should not reconstruct the root of the frame tree.  "
               "Use ReconstructDocElementHierarchy instead.");

  mRebuildAllStyleData = false;
  NS_UpdateHint(aExtraHint, mRebuildAllExtraHint);
  mRebuildAllExtraHint = nsChangeHint(0);

  nsIPresShell* presShell = mPresContext->GetPresShell();
  if (!presShell || !presShell->GetRootFrame())
    return;

  // Make sure that the viewmanager will outlive the presshell
  nsRefPtr<nsViewManager> vm = presShell->GetViewManager();

  // Processing the style changes could cause a flush that propagates to
  // the parent frame and thus destroys the pres shell.
  nsCOMPtr<nsIPresShell> kungFuDeathGrip(presShell);

  // We may reconstruct frames below and hence process anything that is in the
  // tree. We don't want to get notified to process those items again after.
  presShell->GetDocument()->FlushPendingNotifications(Flush_ContentAndNotify);

  nsAutoScriptBlocker scriptBlocker;

  mPresContext->SetProcessingRestyles(true);

  DoRebuildAllStyleData(mPendingRestyles, aExtraHint);

  mPresContext->SetProcessingRestyles(false);

  // Make sure that we process any pending animation restyles from the
  // above style change.  Note that we can *almost* implement the above
  // by just posting a style change -- except we really need to restyle
  // the root frame rather than the root element's primary frame.
  ProcessPendingRestyles();
}

void
RestyleManager::DoRebuildAllStyleData(RestyleTracker& aRestyleTracker,
                                      nsChangeHint aExtraHint)
{
  // Tell the style set to get the old rule tree out of the way
  // so we can recalculate while maintaining rule tree immutability
  nsresult rv = mPresContext->StyleSet()->BeginReconstruct();
  if (NS_FAILED(rv)) {
    return;
  }

  // Recalculate all of the style contexts for the document
  // Note that we can ignore the return value of ComputeStyleChangeFor
  // because we never need to reframe the root frame
  // XXX This could be made faster by not rerunning rule matching
  // (but note that nsPresShell::SetPreferenceStyleRules currently depends
  // on us re-running rule matching here
  nsStyleChangeList changeList;
  // XXX Does it matter that we're passing aExtraHint to the real root
  // frame and not the root node's primary frame?
  // Note: The restyle tracker we pass in here doesn't matter.
  ComputeStyleChangeFor(mPresContext->PresShell()->GetRootFrame(),
                        &changeList, aExtraHint,
                        aRestyleTracker, eRestyle_Subtree);
  // Process the required changes
  ProcessRestyledFrames(changeList);
  FlushOverflowChangedTracker();

  // Tell the style set it's safe to destroy the old rule tree.  We
  // must do this after the ProcessRestyledFrames call in case the
  // change list has frame reconstructs in it (since frames to be
  // reconstructed will still have their old style context pointers
  // until they are destroyed).
  mPresContext->StyleSet()->EndReconstruct();
}

void
RestyleManager::ProcessPendingRestyles()
{
  NS_PRECONDITION(mPresContext->Document(), "No document?  Pshaw!");
  NS_PRECONDITION(!nsContentUtils::IsSafeToRunScript(),
                  "Missing a script blocker!");

  // First do any queued-up frame creation.  (We should really
  // merge this into the rest of the process, though; see bug 827239.)
  mPresContext->FrameConstructor()->CreateNeededFrames();

  // Process non-animation restyles...
  NS_ABORT_IF_FALSE(!mPresContext->IsProcessingRestyles(),
                    "Nesting calls to ProcessPendingRestyles?");
  mPresContext->SetProcessingRestyles(true);

  // Before we process any restyles, we need to ensure that style
  // resulting from any throttled animations (animations that we're
  // running entirely on the compositor thread) is up-to-date, so that
  // if any style changes we cause trigger transitions, we have the
  // correct old style for starting the transition.
  if (nsLayoutUtils::AreAsyncAnimationsEnabled() &&
      mPendingRestyles.Count() > 0) {
    ++mAnimationGeneration;
    mPresContext->TransitionManager()->UpdateAllThrottledStyles();
  }

  mPendingRestyles.ProcessRestyles();

#ifdef DEBUG
  uint32_t oldPendingRestyleCount = mPendingRestyles.Count();
#endif

  // ...and then process animation restyles.  This needs to happen
  // second because we need to start animations that resulted from the
  // first set of restyles (e.g., CSS transitions with negative
  // transition-delay), and because we need to immediately
  // restyle-with-animation any just-restyled elements that are
  // mid-transition (since processing the non-animation restyle ignores
  // the running transition so it can check for a new change on the same
  // property, and then posts an immediate animation style change).
  mPresContext->SetProcessingAnimationStyleChange(true);
  mPendingAnimationRestyles.ProcessRestyles();
  mPresContext->SetProcessingAnimationStyleChange(false);

  mPresContext->SetProcessingRestyles(false);
  NS_POSTCONDITION(mPendingRestyles.Count() == oldPendingRestyleCount,
                   "We should not have posted new non-animation restyles while "
                   "processing animation restyles");

  if (mRebuildAllStyleData) {
    // We probably wasted a lot of work up above, but this seems safest
    // and it should be rarely used.
    // This might add us as a refresh observer again; that's ok.
    RebuildAllStyleData(nsChangeHint(0));
  }
}

void
RestyleManager::BeginProcessingRestyles()
{
  // Make sure to not rebuild quote or counter lists while we're
  // processing restyles
  mPresContext->FrameConstructor()->BeginUpdate();

  mInStyleRefresh = true;
}

void
RestyleManager::EndProcessingRestyles()
{
  FlushOverflowChangedTracker();

  // Set mInStyleRefresh to false now, since the EndUpdate call might
  // add more restyles.
  mInStyleRefresh = false;

  mPresContext->FrameConstructor()->EndUpdate();

#ifdef DEBUG
  mPresContext->PresShell()->VerifyStyleTree();
#endif
}

void
RestyleManager::PostRestyleEventCommon(Element* aElement,
                                       nsRestyleHint aRestyleHint,
                                       nsChangeHint aMinChangeHint,
                                       bool aForAnimation)
{
  if (MOZ_UNLIKELY(mPresContext->PresShell()->IsDestroying())) {
    return;
  }

  if (aRestyleHint == 0 && !aMinChangeHint) {
    // Nothing to do here
    return;
  }

  RestyleTracker& tracker =
    aForAnimation ? mPendingAnimationRestyles : mPendingRestyles;
  tracker.AddPendingRestyle(aElement, aRestyleHint, aMinChangeHint);

  PostRestyleEventInternal(false);
}

void
RestyleManager::PostRestyleEventInternal(bool aForLazyConstruction)
{
  // Make sure we're not in a style refresh; if we are, we still have
  // a call to ProcessPendingRestyles coming and there's no need to
  // add ourselves as a refresh observer until then.
  bool inRefresh = !aForLazyConstruction && mInStyleRefresh;
  nsIPresShell* presShell = mPresContext->PresShell();
  if (!mObservingRefreshDriver && !inRefresh) {
    mObservingRefreshDriver = mPresContext->RefreshDriver()->
      AddStyleFlushObserver(presShell);
  }

  // Unconditionally flag our document as needing a flush.  The other
  // option here would be a dedicated boolean to track whether we need
  // to do so (set here and unset in ProcessPendingRestyles).
  presShell->GetDocument()->SetNeedStyleFlush();
}

void
RestyleManager::PostRebuildAllStyleDataEvent(nsChangeHint aExtraHint)
{
  NS_ASSERTION(!(aExtraHint & nsChangeHint_ReconstructFrame),
               "Should not reconstruct the root of the frame tree.  "
               "Use ReconstructDocElementHierarchy instead.");

  mRebuildAllStyleData = true;
  NS_UpdateHint(mRebuildAllExtraHint, aExtraHint);

  // Get a restyle event posted if necessary
  PostRestyleEventInternal(false);
}

#ifdef DEBUG
static void
DumpContext(nsIFrame* aFrame, nsStyleContext* aContext)
{
  if (aFrame) {
    fputs("frame: ", stdout);
    nsAutoString  name;
    aFrame->GetFrameName(name);
    fputs(NS_LossyConvertUTF16toASCII(name).get(), stdout);
    fprintf(stdout, " (%p)", static_cast<void*>(aFrame));
  }
  if (aContext) {
    fprintf(stdout, " style: %p ", static_cast<void*>(aContext));

    nsIAtom* pseudoTag = aContext->GetPseudo();
    if (pseudoTag) {
      nsAutoString  buffer;
      pseudoTag->ToString(buffer);
      fputs(NS_LossyConvertUTF16toASCII(buffer).get(), stdout);
      fputs(" ", stdout);
    }
    fputs("{}\n", stdout);
  }
}

static void
VerifySameTree(nsStyleContext* aContext1, nsStyleContext* aContext2)
{
  nsStyleContext* top1 = aContext1;
  nsStyleContext* top2 = aContext2;
  nsStyleContext* parent;
  for (;;) {
    parent = top1->GetParent();
    if (!parent)
      break;
    top1 = parent;
  }
  for (;;) {
    parent = top2->GetParent();
    if (!parent)
      break;
    top2 = parent;
  }
  NS_ASSERTION(top1 == top2,
               "Style contexts are not in the same style context tree");
}

static void
VerifyContextParent(nsPresContext* aPresContext, nsIFrame* aFrame,
                    nsStyleContext* aContext, nsStyleContext* aParentContext)
{
  // get the contexts not provided
  if (!aContext) {
    aContext = aFrame->StyleContext();
  }

  if (!aParentContext) {
    // Get the correct parent context from the frame
    //  - if the frame is a placeholder, we get the out of flow frame's context
    //    as the parent context instead of asking the frame

    // get the parent context from the frame (indirectly)
    nsIFrame* providerFrame = aFrame->GetParentStyleContextFrame();
    if (providerFrame)
      aParentContext = providerFrame->StyleContext();
    // aParentContext could still be null
  }

  NS_ASSERTION(aContext, "Failure to get required contexts");
  nsStyleContext* actualParentContext = aContext->GetParent();

  if (aParentContext) {
    if (aParentContext != actualParentContext) {
      DumpContext(aFrame, aContext);
      if (aContext == aParentContext) {
        NS_ERROR("Using parent's style context");
      }
      else {
        NS_ERROR("Wrong parent style context");
        fputs("Wrong parent style context: ", stdout);
        DumpContext(nullptr, actualParentContext);
        fputs("should be using: ", stdout);
        DumpContext(nullptr, aParentContext);
        VerifySameTree(actualParentContext, aParentContext);
        fputs("\n", stdout);
      }
    }

  }
  else {
    if (actualParentContext) {
      NS_ERROR("Have parent context and shouldn't");
      DumpContext(aFrame, aContext);
      fputs("Has parent context: ", stdout);
      DumpContext(nullptr, actualParentContext);
      fputs("Should be null\n\n", stdout);
    }
  }

  nsStyleContext* childStyleIfVisited = aContext->GetStyleIfVisited();
  // Either childStyleIfVisited has aContext->GetParent()->GetStyleIfVisited()
  // as the parent or it has a different rulenode from aContext _and_ has
  // aContext->GetParent() as the parent.
  if (childStyleIfVisited &&
      !((childStyleIfVisited->RuleNode() != aContext->RuleNode() &&
         childStyleIfVisited->GetParent() == aContext->GetParent()) ||
        childStyleIfVisited->GetParent() ==
          aContext->GetParent()->GetStyleIfVisited())) {
    NS_ERROR("Visited style has wrong parent");
    DumpContext(aFrame, aContext);
    fputs("\n", stdout);
  }
}

static void
VerifyStyleTree(nsPresContext* aPresContext, nsIFrame* aFrame,
                nsStyleContext* aParentContext)
{
  nsStyleContext*  context = aFrame->StyleContext();
  VerifyContextParent(aPresContext, aFrame, context, nullptr);

  nsIFrame::ChildListIterator lists(aFrame);
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      nsIFrame* child = childFrames.get();
      if (!(child->GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
        // only do frames that are in flow
        if (nsGkAtoms::placeholderFrame == child->GetType()) {
          // placeholder: first recurse and verify the out of flow frame,
          // then verify the placeholder's context
          nsIFrame* outOfFlowFrame =
            nsPlaceholderFrame::GetRealFrameForPlaceholder(child);

          // recurse to out of flow frame, letting the parent context get resolved
          do {
            VerifyStyleTree(aPresContext, outOfFlowFrame, nullptr);
          } while ((outOfFlowFrame = outOfFlowFrame->GetNextContinuation()));

          // verify placeholder using the parent frame's context as
          // parent context
          VerifyContextParent(aPresContext, child, nullptr, nullptr);
        }
        else { // regular frame
          VerifyStyleTree(aPresContext, child, nullptr);
        }
      }
    }
  }

  // do additional contexts
  int32_t contextIndex = 0;
  for (nsStyleContext* extraContext;
       (extraContext = aFrame->GetAdditionalStyleContext(contextIndex));
       ++contextIndex) {
    VerifyContextParent(aPresContext, aFrame, extraContext, context);
  }
}

void
RestyleManager::DebugVerifyStyleTree(nsIFrame* aFrame)
{
  if (aFrame) {
    nsStyleContext* context = aFrame->StyleContext();
    nsStyleContext* parentContext = context->GetParent();
    VerifyStyleTree(mPresContext, aFrame, parentContext);
  }
}

#endif // DEBUG

// aContent must be the content for the frame in question, which may be
// :before/:after content
static void
TryStartingTransition(nsPresContext *aPresContext, nsIContent *aContent,
                      nsStyleContext *aOldStyleContext,
                      nsRefPtr<nsStyleContext> *aNewStyleContext /* inout */)
{
  if (!aContent || !aContent->IsElement()) {
    return;
  }

  // Notify the transition manager, and if it starts a transition,
  // it will give us back a transition-covering style rule which
  // we'll use to get *another* style context.  We want to ignore
  // any already-running transitions, but cover up any that we're
  // currently starting with their start value so we don't start
  // them again for descendants that inherit that value.
  nsCOMPtr<nsIStyleRule> coverRule =
    aPresContext->TransitionManager()->StyleContextChanged(
      aContent->AsElement(), aOldStyleContext, *aNewStyleContext);
  if (coverRule) {
    nsCOMArray<nsIStyleRule> rules;
    rules.AppendObject(coverRule);
    *aNewStyleContext = aPresContext->StyleSet()->
                          ResolveStyleByAddingRules(*aNewStyleContext, rules);
  }
}

static inline dom::Element*
ElementForStyleContext(nsIContent* aParentContent,
                       nsIFrame* aFrame,
                       nsCSSPseudoElements::Type aPseudoType)
{
  // We don't expect XUL tree stuff here.
  NS_PRECONDITION(aPseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement ||
                  aPseudoType == nsCSSPseudoElements::ePseudo_AnonBox ||
                  aPseudoType < nsCSSPseudoElements::ePseudo_PseudoElementCount,
                  "Unexpected pseudo");
  // XXX see the comments about the various element confusion in
  // ElementRestyler::Restyle.
  if (aPseudoType == nsCSSPseudoElements::ePseudo_NotPseudoElement) {
    return aFrame->GetContent()->AsElement();
  }

  if (aPseudoType == nsCSSPseudoElements::ePseudo_AnonBox) {
    return nullptr;
  }

  if (aPseudoType == nsCSSPseudoElements::ePseudo_firstLetter) {
    NS_ASSERTION(aFrame->GetType() == nsGkAtoms::letterFrame,
                 "firstLetter pseudoTag without a nsFirstLetterFrame");
    nsBlockFrame* block = nsBlockFrame::GetNearestAncestorBlock(aFrame);
    return block->GetContent()->AsElement();
  }

  if (aPseudoType == nsCSSPseudoElements::ePseudo_mozColorSwatch) {
    MOZ_ASSERT(aFrame->GetParent() &&
               aFrame->GetParent()->GetParent(),
               "Color swatch frame should have a parent & grandparent");

    nsIFrame* grandparentFrame = aFrame->GetParent()->GetParent();
    MOZ_ASSERT(grandparentFrame->GetType() == nsGkAtoms::colorControlFrame,
               "Color swatch's grandparent should be nsColorControlFrame");

    return grandparentFrame->GetContent()->AsElement();
  }

  if (aPseudoType == nsCSSPseudoElements::ePseudo_mozNumberText ||
      aPseudoType == nsCSSPseudoElements::ePseudo_mozNumberWrapper ||
      aPseudoType == nsCSSPseudoElements::ePseudo_mozNumberSpinBox ||
      aPseudoType == nsCSSPseudoElements::ePseudo_mozNumberSpinUp ||
      aPseudoType == nsCSSPseudoElements::ePseudo_mozNumberSpinDown) {
    // Get content for nearest nsNumberControlFrame:
    nsIFrame* f = aFrame->GetParent();
    MOZ_ASSERT(f);
    while (f->GetType() != nsGkAtoms::numberControlFrame) {
      f = f->GetParent();
      MOZ_ASSERT(f);
    }
    return f->GetContent()->AsElement();
  }

  if (aParentContent) {
    return aParentContent->AsElement();
  }

  MOZ_ASSERT(aFrame->GetContent()->GetParent(),
             "should not have got here for the root element");
  return aFrame->GetContent()->GetParent()->AsElement();
}

/**
 * FIXME: Temporary.  Should merge with following function.
 */
static nsIFrame*
GetPrevContinuationWithPossiblySameStyle(nsIFrame* aFrame)
{
  // Account for {ib} splits when looking for "prevContinuation".  In
  // particular, for the first-continuation of a part of an {ib} split
  // we want to use the previous ib-split sibling of the previous
  // ib-split sibling of aFrame, which should have the same style
  // context as aFrame itself.  In particular, if aFrame is the first
  // continuation of an inline part of a block-in-inline split then its
  // previous ib-split sibling is a block, and the previous ib-split
  // sibling of _that_ is an inline, just like aFrame.  Similarly, if
  // aFrame is the first continuation of a block part of an
  // block-in-inline split (a block-in-inline wrapper block), then its
  // previous ib-split sibling is an inline and the previous ib-split
  // sibling of that is either another block-in-inline wrapper block box
  // or null.
  nsIFrame *prevContinuation = aFrame->GetPrevContinuation();
  if (!prevContinuation &&
      (aFrame->GetStateBits() & NS_FRAME_PART_OF_IBSPLIT)) {
    // We're the first continuation, so we can just get the frame
    // property directly
    prevContinuation = static_cast<nsIFrame*>(
      aFrame->Properties().Get(nsIFrame::IBSplitPrevSibling()));
    if (prevContinuation) {
      prevContinuation = static_cast<nsIFrame*>(
        prevContinuation->Properties().Get(nsIFrame::IBSplitPrevSibling()));
    }
  }

  NS_ASSERTION(!prevContinuation ||
               prevContinuation->GetContent() == aFrame->GetContent(),
               "unexpected content mismatch");

  return prevContinuation;
}

/**
 * Get the previous continuation or similar ib-split sibling (assuming
 * block/inline alternation), conditionally on it having the same style.
 * This assumes that we're not between resolving the two (i.e., that
 * they're both already resolved.
 */
static nsIFrame*
GetPrevContinuationWithSameStyle(nsIFrame* aFrame)
{
  nsIFrame* prevContinuation = GetPrevContinuationWithPossiblySameStyle(aFrame);
  if (!prevContinuation) {
    return nullptr;
  }

  nsStyleContext* prevStyle = prevContinuation->StyleContext();
  nsStyleContext* selfStyle = aFrame->StyleContext();
  if (prevStyle != selfStyle) {
    NS_ASSERTION(prevStyle->GetPseudo() != selfStyle->GetPseudo() ||
                 prevStyle->GetParent() != selfStyle->GetParent(),
                 "continuations should have the same style context");
    prevContinuation = nullptr;
  }
  return prevContinuation;
}

/**
 * Get the next continuation or similar ib-split sibling (assuming
 * block/inline alternation), conditionally on it having the same style.
 *
 * Since this is used when deciding to copy the new style context, it
 * takes as an argument the old style context to check if the style is
 * the same.  When it is used in other contexts (i.e., where the next
 * continuation would already have the new style context), the current
 * style context should be passed.
 */
static nsIFrame*
GetNextContinuationWithSameStyle(nsIFrame* aFrame,
                                 nsStyleContext* aOldStyleContext)
{
  // See GetPrevContinuationWithSameStyle about {ib} splits.

  nsIFrame *nextContinuation = aFrame->GetNextContinuation();
  if (!nextContinuation &&
      (aFrame->GetStateBits() & NS_FRAME_PART_OF_IBSPLIT)) {
    // We're the last continuation, so we have to hop back to the first
    // before getting the frame property
    nextContinuation = static_cast<nsIFrame*>(aFrame->FirstContinuation()->
      Properties().Get(nsIFrame::IBSplitSibling()));
    if (nextContinuation) {
      nextContinuation = static_cast<nsIFrame*>(
        nextContinuation->Properties().Get(nsIFrame::IBSplitSibling()));
    }
  }

  if (!nextContinuation) {
    return nullptr;
  }

  NS_ASSERTION(nextContinuation->GetContent() == aFrame->GetContent(),
               "unexpected content mismatch");

  nsStyleContext* nextStyle = nextContinuation->StyleContext();
  if (nextStyle != aOldStyleContext) {
    NS_ASSERTION(aOldStyleContext->GetPseudo() != nextStyle->GetPseudo() ||
                 aOldStyleContext->GetParent() != nextStyle->GetParent(),
                 "continuations should have the same style context");
    nextContinuation = nullptr;
  }
  return nextContinuation;
}

nsresult
RestyleManager::ReparentStyleContext(nsIFrame* aFrame)
{
  if (nsGkAtoms::placeholderFrame == aFrame->GetType()) {
    // Also reparent the out-of-flow and all its continuations.
    nsIFrame* outOfFlow =
      nsPlaceholderFrame::GetRealFrameForPlaceholder(aFrame);
    NS_ASSERTION(outOfFlow, "no out-of-flow frame");
    do {
      ReparentStyleContext(outOfFlow);
    } while ((outOfFlow = outOfFlow->GetNextContinuation()));
  }

  // DO NOT verify the style tree before reparenting.  The frame
  // tree has already been changed, so this check would just fail.
  nsStyleContext* oldContext = aFrame->StyleContext();

  nsRefPtr<nsStyleContext> newContext;
  nsIFrame* providerFrame = aFrame->GetParentStyleContextFrame();
  bool isChild = providerFrame && providerFrame->GetParent() == aFrame;
  nsStyleContext* newParentContext = nullptr;
  nsIFrame* providerChild = nullptr;
  if (isChild) {
    ReparentStyleContext(providerFrame);
    newParentContext = providerFrame->StyleContext();
    providerChild = providerFrame;
  } else if (providerFrame) {
    newParentContext = providerFrame->StyleContext();
  } else {
    NS_NOTREACHED("Reparenting something that has no usable parent? "
                  "Shouldn't happen!");
  }
  // XXX need to do something here to produce the correct style context for
  // an IB split whose first inline part is inside a first-line frame.
  // Currently the first IB anonymous block's style context takes the first
  // part's style context as parent, which is wrong since first-line style
  // should not apply to the anonymous block.

#ifdef DEBUG
  {
    // Check that our assumption that continuations of the same
    // pseudo-type and with the same style context parent have the
    // same style context is valid before the reresolution.  (We need
    // to check the pseudo-type and style context parent because of
    // :first-letter and :first-line, where we create styled and
    // unstyled letter/line frames distinguished by pseudo-type, and
    // then need to distinguish their descendants based on having
    // different parents.)
    nsIFrame *nextContinuation = aFrame->GetNextContinuation();
    if (nextContinuation) {
      nsStyleContext *nextContinuationContext =
        nextContinuation->StyleContext();
      NS_ASSERTION(oldContext == nextContinuationContext ||
                   oldContext->GetPseudo() !=
                     nextContinuationContext->GetPseudo() ||
                   oldContext->GetParent() !=
                     nextContinuationContext->GetParent(),
                   "continuations should have the same style context");
    }
  }
#endif

  nsIFrame *prevContinuation =
    GetPrevContinuationWithPossiblySameStyle(aFrame);
  nsStyleContext *prevContinuationContext;
  bool copyFromContinuation =
    prevContinuation &&
    (prevContinuationContext = prevContinuation->StyleContext())
      ->GetPseudo() == oldContext->GetPseudo() &&
     prevContinuationContext->GetParent() == newParentContext;
  if (copyFromContinuation) {
    // Just use the style context from the frame's previous
    // continuation (see assertion about aFrame->GetNextContinuation()
    // above, which we would have previously hit for aFrame's previous
    // continuation).
    newContext = prevContinuationContext;
  } else {
    nsIFrame* parentFrame = aFrame->GetParent();
    Element* element =
      ElementForStyleContext(parentFrame ? parentFrame->GetContent() : nullptr,
                             aFrame,
                             oldContext->GetPseudoType());
    newContext = mPresContext->StyleSet()->
                   ReparentStyleContext(oldContext, newParentContext, element);
  }

  if (newContext) {
    if (newContext != oldContext) {
      // We probably don't want to initiate transitions from
      // ReparentStyleContext, since we call it during frame
      // construction rather than in response to dynamic changes.
      // Also see the comment at the start of
      // nsTransitionManager::ConsiderStartingTransition.
#if 0
      if (!copyFromContinuation) {
        TryStartingTransition(mPresContext, aFrame->GetContent(),
                              oldContext, &newContext);
      }
#endif

      // Make sure to call CalcStyleDifference so that the new context ends
      // up resolving all the structs the old context resolved.
      if (!copyFromContinuation) {
        DebugOnly<nsChangeHint> styleChange =
          oldContext->CalcStyleDifference(newContext, nsChangeHint(0));
        // The style change is always 0 because we have the same rulenode and
        // CalcStyleDifference optimizes us away.  That's OK, though:
        // reparenting should never trigger a frame reconstruct, and whenever
        // it's happening we already plan to reflow and repaint the frames.
        NS_ASSERTION(!(styleChange & nsChangeHint_ReconstructFrame),
                     "Our frame tree is likely to be bogus!");
      }

      aFrame->SetStyleContext(newContext);

      nsIFrame::ChildListIterator lists(aFrame);
      for (; !lists.IsDone(); lists.Next()) {
        nsFrameList::Enumerator childFrames(lists.CurrentList());
        for (; !childFrames.AtEnd(); childFrames.Next()) {
          nsIFrame* child = childFrames.get();
          // only do frames that are in flow
          if (!(child->GetStateBits() & NS_FRAME_OUT_OF_FLOW) &&
              child != providerChild) {
#ifdef DEBUG
            if (nsGkAtoms::placeholderFrame == child->GetType()) {
              nsIFrame* outOfFlowFrame =
                nsPlaceholderFrame::GetRealFrameForPlaceholder(child);
              NS_ASSERTION(outOfFlowFrame, "no out-of-flow frame");

              NS_ASSERTION(outOfFlowFrame != providerChild,
                           "Out of flow provider?");
            }
#endif
            ReparentStyleContext(child);
          }
        }
      }

      // If this frame is part of an IB split, then the style context of
      // the next part of the split might be a child of our style context.
      // Reparent its style context just in case one of our ancestors
      // (split or not) hasn't done so already). It's not a problem to
      // reparent the same frame twice because the "if (newContext !=
      // oldContext)" check will prevent us from redoing work.
      if ((aFrame->GetStateBits() & NS_FRAME_PART_OF_IBSPLIT) &&
          !aFrame->GetPrevContinuation()) {
        nsIFrame* sib = static_cast<nsIFrame*>
          (aFrame->Properties().Get(nsIFrame::IBSplitSibling()));
        if (sib) {
          ReparentStyleContext(sib);
        }
      }

      // do additional contexts
      int32_t contextIndex = 0;
      for (nsStyleContext* oldExtraContext;
           (oldExtraContext = aFrame->GetAdditionalStyleContext(contextIndex));
           ++contextIndex) {
        nsRefPtr<nsStyleContext> newExtraContext;
        newExtraContext = mPresContext->StyleSet()->
                            ReparentStyleContext(oldExtraContext,
                                                 newContext, nullptr);
        if (newExtraContext) {
          if (newExtraContext != oldExtraContext) {
            // Make sure to call CalcStyleDifference so that the new
            // context ends up resolving all the structs the old context
            // resolved.
            DebugOnly<nsChangeHint> styleChange =
              oldExtraContext->CalcStyleDifference(newExtraContext,
                                                   nsChangeHint(0));
            // The style change is always 0 because we have the same
            // rulenode and CalcStyleDifference optimizes us away.  That's
            // OK, though: reparenting should never trigger a frame
            // reconstruct, and whenever it's happening we already plan to
            // reflow and repaint the frames.
            NS_ASSERTION(!(styleChange & nsChangeHint_ReconstructFrame),
                         "Our frame tree is likely to be bogus!");
          }

          aFrame->SetAdditionalStyleContext(contextIndex, newExtraContext);
        }
      }
#ifdef DEBUG
      VerifyStyleTree(mPresContext, aFrame, newParentContext);
#endif
    }
  }

  return NS_OK;
}

ElementRestyler::ElementRestyler(nsPresContext* aPresContext,
                                 nsIFrame* aFrame,
                                 nsStyleChangeList* aChangeList,
                                 nsChangeHint aHintsHandledByAncestors,
                                 RestyleTracker& aRestyleTracker,
                                 TreeMatchContext& aTreeMatchContext,
                                 nsTArray<nsIContent*>&
                                   aVisibleKidsOfHiddenElement)
  : mPresContext(aPresContext)
  , mFrame(aFrame)
  , mParentContent(nullptr)
    // XXXldb Why does it make sense to use aParentContent?  (See
    // comment above assertion at start of ElementRestyler::Restyle.)
  , mContent(mFrame->GetContent() ? mFrame->GetContent() : mParentContent)
  , mChangeList(aChangeList)
  , mHintsHandled(NS_SubtractHint(aHintsHandledByAncestors,
                  NS_HintsNotHandledForDescendantsIn(aHintsHandledByAncestors)))
  , mParentFrameHintsNotHandledForDescendants(nsChangeHint(0))
  , mHintsNotHandledForDescendants(nsChangeHint(0))
  , mRestyleTracker(aRestyleTracker)
  , mTreeMatchContext(aTreeMatchContext)
  , mResolvedChild(nullptr)
#ifdef ACCESSIBILITY
  , mDesiredA11yNotifications(eSendAllNotifications)
  , mKidsDesiredA11yNotifications(mDesiredA11yNotifications)
  , mOurA11yNotification(eDontNotify)
  , mVisibleKidsOfHiddenElement(aVisibleKidsOfHiddenElement)
#endif
{
}

ElementRestyler::ElementRestyler(const ElementRestyler& aParentRestyler,
                                 nsIFrame* aFrame,
                                 uint32_t aConstructorFlags)
  : mPresContext(aParentRestyler.mPresContext)
  , mFrame(aFrame)
  , mParentContent(aParentRestyler.mContent)
    // XXXldb Why does it make sense to use aParentContent?  (See
    // comment above assertion at start of ElementRestyler::Restyle.)
  , mContent(mFrame->GetContent() ? mFrame->GetContent() : mParentContent)
  , mChangeList(aParentRestyler.mChangeList)
  , mHintsHandled(NS_SubtractHint(aParentRestyler.mHintsHandled,
                  NS_HintsNotHandledForDescendantsIn(aParentRestyler.mHintsHandled)))
  , mParentFrameHintsNotHandledForDescendants(
      aParentRestyler.mHintsNotHandledForDescendants)
  , mHintsNotHandledForDescendants(nsChangeHint(0))
  , mRestyleTracker(aParentRestyler.mRestyleTracker)
  , mTreeMatchContext(aParentRestyler.mTreeMatchContext)
  , mResolvedChild(nullptr)
#ifdef ACCESSIBILITY
  , mDesiredA11yNotifications(aParentRestyler.mKidsDesiredA11yNotifications)
  , mKidsDesiredA11yNotifications(mDesiredA11yNotifications)
  , mOurA11yNotification(eDontNotify)
  , mVisibleKidsOfHiddenElement(aParentRestyler.mVisibleKidsOfHiddenElement)
#endif
{
  if (aConstructorFlags & FOR_OUT_OF_FLOW_CHILD) {
    // Note that the out-of-flow may not be a geometric descendant of
    // the frame where we started the reresolve.  Therefore, even if
    // mHintsHandled already includes nsChangeHint_AllReflowHints we
    // don't want to pass that on to the out-of-flow reresolve, since
    // that can lead to the out-of-flow not getting reflowed when it
    // should be (eg a reresolve starting at <body> that involves
    // reflowing the <body> would miss reflowing fixed-pos nodes that
    // also need reflow).  In the cases when the out-of-flow _is_ a
    // geometric descendant of a frame we already have a reflow hint
    // for, reflow coalescing should keep us from doing the work twice.
    mHintsHandled = NS_SubtractHint(mHintsHandled, nsChangeHint_AllReflowHints);
  }
}

ElementRestyler::ElementRestyler(ParentContextFromChildFrame,
                                 const ElementRestyler& aParentRestyler,
                                 nsIFrame* aFrame)
  : mPresContext(aParentRestyler.mPresContext)
  , mFrame(aFrame)
  , mParentContent(aParentRestyler.mParentContent)
    // XXXldb Why does it make sense to use aParentContent?  (See
    // comment above assertion at start of ElementRestyler::Restyle.)
  , mContent(mFrame->GetContent() ? mFrame->GetContent() : mParentContent)
  , mChangeList(aParentRestyler.mChangeList)
  , mHintsHandled(NS_SubtractHint(aParentRestyler.mHintsHandled,
                  NS_HintsNotHandledForDescendantsIn(aParentRestyler.mHintsHandled)))
  , mParentFrameHintsNotHandledForDescendants(
      // assume the worst
      nsChangeHint_Hints_NotHandledForDescendants)
  , mHintsNotHandledForDescendants(nsChangeHint(0))
  , mRestyleTracker(aParentRestyler.mRestyleTracker)
  , mTreeMatchContext(aParentRestyler.mTreeMatchContext)
  , mResolvedChild(nullptr)
#ifdef ACCESSIBILITY
  , mDesiredA11yNotifications(aParentRestyler.mDesiredA11yNotifications)
  , mKidsDesiredA11yNotifications(mDesiredA11yNotifications)
  , mOurA11yNotification(eDontNotify)
  , mVisibleKidsOfHiddenElement(aParentRestyler.mVisibleKidsOfHiddenElement)
#endif
{
}

void
ElementRestyler::CaptureChange(nsStyleContext* aOldContext,
                               nsStyleContext* aNewContext,
                               nsChangeHint aChangeToAssume)
{
  // Check some invariants about replacing one style context with another.
  NS_ASSERTION(aOldContext->GetPseudo() == aNewContext->GetPseudo(),
               "old and new style contexts should have the same pseudo");
  NS_ASSERTION(aOldContext->GetPseudoType() == aNewContext->GetPseudoType(),
               "old and new style contexts should have the same pseudo");

  nsChangeHint ourChange = aOldContext->CalcStyleDifference(aNewContext,
                             mParentFrameHintsNotHandledForDescendants);
  NS_ASSERTION(!(ourChange & nsChangeHint_AllReflowHints) ||
               (ourChange & nsChangeHint_NeedReflow),
               "Reflow hint bits set without actually asking for a reflow");

  // nsChangeHint_UpdateEffects is inherited, but it can be set due to changes
  // in inherited properties (fill and stroke).  Avoid propagating it into
  // text nodes.
  if ((ourChange & nsChangeHint_UpdateEffects) &&
      mContent && !mContent->IsElement()) {
    ourChange = NS_SubtractHint(ourChange, nsChangeHint_UpdateEffects);
  }

  NS_UpdateHint(ourChange, aChangeToAssume);
  if (NS_UpdateHint(mHintsHandled, ourChange)) {
    if (!(ourChange & nsChangeHint_ReconstructFrame) || mContent) {
      mChangeList->AppendChange(mFrame, mContent, ourChange);
    }
  }
  NS_UpdateHint(mHintsNotHandledForDescendants,
                NS_HintsNotHandledForDescendantsIn(ourChange));
}

/**
 * Recompute style for mFrame (which should not have a prev continuation
 * with the same style), all of its next continuations with the same
 * style, and all ib-split siblings of the same type (either block or
 * inline, skipping the intermediates of the other type) and accumulate
 * changes into mChangeList given that mHintsHandled is already accumulated
 * for an ancestor.
 * mParentContent is the content node used to resolve the parent style
 * context.  This means that, for pseudo-elements, it is the content
 * that should be used for selector matching (rather than the fake
 * content node attached to the frame).
 */
void
ElementRestyler::Restyle(nsRestyleHint aRestyleHint)
{
  // It would be nice if we could make stronger assertions here; they
  // would let us simplify the ?: expressions below setting |content|
  // and |pseudoContent| in sensible ways as well as making what
  // |content| and |pseudoContent| mean, and their relationship to
  // |mFrame->GetContent()|, make more sense.  However, we can't,
  // because of frame trees like the one in
  // https://bugzilla.mozilla.org/show_bug.cgi?id=472353#c14 .  Once we
  // fix bug 242277 we should be able to make this make more sense.
  NS_ASSERTION(mFrame->GetContent() || !mParentContent ||
               !mParentContent->GetParent(),
               "frame must have content (unless at the top of the tree)");

  NS_ASSERTION(!GetPrevContinuationWithSameStyle(mFrame),
               "should not be trying to restyle this frame separately");

  MOZ_ASSERT(!(aRestyleHint & eRestyle_LaterSiblings),
             "eRestyle_LaterSiblings must not be part of aRestyleHint");

  if (mContent && mContent->IsElement()) {
    mContent->OwnerDoc()->FlushPendingLinkUpdates();
    RestyleTracker::RestyleData restyleData;
    if (mRestyleTracker.GetRestyleData(mContent->AsElement(), &restyleData)) {
      if (NS_UpdateHint(mHintsHandled, restyleData.mChangeHint)) {
        mChangeList->AppendChange(mFrame, mContent, restyleData.mChangeHint);
      }
      aRestyleHint = nsRestyleHint(aRestyleHint | restyleData.mRestyleHint);
    }
  }

  nsRestyleHint childRestyleHint = aRestyleHint;

  if (childRestyleHint == eRestyle_Self) {
    childRestyleHint = nsRestyleHint(0);
  }

  {
    nsRefPtr<nsStyleContext> oldContext = mFrame->StyleContext();

    // TEMPORARY (until bug 918064):  Call RestyleSelf for each
    // continuation or block-in-inline sibling.

    for (nsIFrame* f = mFrame; f;
         f = GetNextContinuationWithSameStyle(f, oldContext)) {
      RestyleSelf(f, aRestyleHint);
    }
  }

  RestyleChildren(childRestyleHint);
}

void
ElementRestyler::RestyleSelf(nsIFrame* aSelf, nsRestyleHint aRestyleHint)
{
  MOZ_ASSERT(!(aRestyleHint & eRestyle_LaterSiblings),
             "eRestyle_LaterSiblings must not be part of aRestyleHint");

  // XXXldb get new context from prev-in-flow if possible, to avoid
  // duplication.  (Or should we just let |GetContext| handle that?)
  // Getting the hint would be nice too, but that's harder.

  // XXXbryner we may be able to avoid some of the refcounting goop here.
  // We do need a reference to oldContext for the lifetime of this function, and it's possible
  // that the frame has the last reference to it, so AddRef it here.

  nsChangeHint assumeDifferenceHint = NS_STYLE_HINT_NONE;
  nsRefPtr<nsStyleContext> oldContext = aSelf->StyleContext();
  nsStyleSet* styleSet = mPresContext->StyleSet();

#ifdef ACCESSIBILITY
  mWasFrameVisible = nsIPresShell::IsAccessibilityActive() ?
    oldContext->StyleVisibility()->IsVisible() : false;
#endif

  nsIAtom* const pseudoTag = oldContext->GetPseudo();
  const nsCSSPseudoElements::Type pseudoType = oldContext->GetPseudoType();

  nsStyleContext* parentContext;
  // Get the frame providing the parent style context.  If it is a
  // child, then resolve the provider first.
  nsIFrame* providerFrame = aSelf->GetParentStyleContextFrame();
  bool isChild = providerFrame && providerFrame->GetParent() == aSelf;
  if (!isChild) {
    if (providerFrame)
      parentContext = providerFrame->StyleContext();
    else
      parentContext = nullptr;
  }
  else {
    MOZ_ASSERT(providerFrame->GetContent() == aSelf->GetContent(),
               "Postcondition for GetParentStyleContextFrame() violated. "
               "That means we need to add the current element to the "
               "ancestor filter.");

    // resolve the provider here (before aSelf below).

    // assumeDifferenceHint forces the parent's change to be also
    // applied to this frame, no matter what
    // nsStyleContext::CalcStyleDifference says. CalcStyleDifference
    // can't be trusted because it assumes any changes to the parent
    // style context provider will be automatically propagated to
    // the frame(s) with child style contexts.

    ElementRestyler providerRestyler(PARENT_CONTEXT_FROM_CHILD_FRAME,
                                     *this, providerFrame);
    providerRestyler.Restyle(aRestyleHint);
    assumeDifferenceHint = providerRestyler.HintsHandledForFrame();

    // The provider's new context becomes the parent context of
    // aSelf's context.
    parentContext = providerFrame->StyleContext();
    // Set |mResolvedChild| so we don't bother resolving the
    // provider again.
    mResolvedChild = providerFrame;
  }

  if (providerFrame != aSelf->GetParent()) {
    // We don't actually know what the parent style context's
    // non-inherited hints were, so assume the worst.
    mParentFrameHintsNotHandledForDescendants =
      nsChangeHint_Hints_NotHandledForDescendants;
  }

  // do primary context
  nsRefPtr<nsStyleContext> newContext;
  nsIFrame *prevContinuation =
    GetPrevContinuationWithPossiblySameStyle(aSelf);
  nsStyleContext *prevContinuationContext;
  bool copyFromContinuation =
    prevContinuation &&
    (prevContinuationContext = prevContinuation->StyleContext())
      ->GetPseudo() == oldContext->GetPseudo() &&
     prevContinuationContext->GetParent() == parentContext;
  if (copyFromContinuation) {
    // Just use the style context from the frame's previous
    // continuation.
    newContext = prevContinuationContext;
  }
  else if (pseudoTag == nsCSSAnonBoxes::mozNonElement) {
    NS_ASSERTION(aSelf->GetContent(),
                 "non pseudo-element frame without content node");
    newContext = styleSet->ResolveStyleForNonElement(parentContext);
  }
  else if (!aRestyleHint && !prevContinuation) {
    // Unfortunately, if prevContinuation is non-null then we may have
    // already stolen the restyle tracker entry for this element while
    // processing prevContinuation.  So we don't know whether aRestyleHint
    // should really be 0 here or whether it should be eRestyle_Self.  Be
    // pessimistic and force an actual reresolve in that situation.  The good
    // news is that in the common case when prevContinuation is non-null we
    // just used prevContinuationContext anyway and aren't reaching this code
    // to start with.
    newContext =
      styleSet->ReparentStyleContext(oldContext, parentContext,
                                     ElementForStyleContext(mParentContent,
                                                            aSelf, pseudoType));
  } else if (pseudoType == nsCSSPseudoElements::ePseudo_AnonBox) {
    newContext = styleSet->ResolveAnonymousBoxStyle(pseudoTag,
                                                    parentContext);
  }
  else {
    Element* element = ElementForStyleContext(mParentContent, aSelf, pseudoType);
    if (pseudoTag) {
      if (pseudoTag == nsCSSPseudoElements::before ||
          pseudoTag == nsCSSPseudoElements::after) {
        // XXX what other pseudos do we need to treat like this?
        newContext = styleSet->ProbePseudoElementStyle(element,
                                                       pseudoType,
                                                       parentContext,
                                                       mTreeMatchContext);
        if (!newContext) {
          // This pseudo should no longer exist; gotta reframe
          NS_UpdateHint(mHintsHandled, nsChangeHint_ReconstructFrame);
          mChangeList->AppendChange(aSelf, element,
                                    nsChangeHint_ReconstructFrame);
          // We're reframing anyway; just keep the same context
          newContext = oldContext;
        }
      } else {
        // Don't expect XUL tree stuff here, since it needs a comparator and
        // all.
        NS_ASSERTION(pseudoType <
                       nsCSSPseudoElements::ePseudo_PseudoElementCount,
                     "Unexpected pseudo type");
        Element* pseudoElement =
          nsCSSPseudoElements::PseudoElementSupportsStyleAttribute(pseudoType) ||
          nsCSSPseudoElements::PseudoElementSupportsUserActionState(pseudoType) ?
            aSelf->GetContent()->AsElement() : nullptr;
        MOZ_ASSERT(element != pseudoElement);
        newContext = styleSet->ResolvePseudoElementStyle(element,
                                                         pseudoType,
                                                         parentContext,
                                                         pseudoElement);
      }
    }
    else {
      NS_ASSERTION(aSelf->GetContent(),
                   "non pseudo-element frame without content node");
      // Skip parent display based style fixup for anonymous subtrees:
      TreeMatchContext::AutoParentDisplayBasedStyleFixupSkipper
        parentDisplayBasedFixupSkipper(mTreeMatchContext,
                               element->IsRootOfNativeAnonymousSubtree());
      newContext = styleSet->ResolveStyleFor(element, parentContext,
                                             mTreeMatchContext);
    }
  }

  MOZ_ASSERT(newContext);

  if (!parentContext) {
    if (oldContext->RuleNode() == newContext->RuleNode() &&
        oldContext->IsLinkContext() == newContext->IsLinkContext() &&
        oldContext->RelevantLinkVisited() ==
          newContext->RelevantLinkVisited()) {
      // We're the root of the style context tree and the new style
      // context returned has the same rule node.  This means that
      // we can use FindChildWithRules to keep a lot of the old
      // style contexts around.  However, we need to start from the
      // same root.
      newContext = oldContext;
    }
  }

  if (newContext != oldContext) {
    if (!copyFromContinuation) {
      TryStartingTransition(mPresContext, aSelf->GetContent(),
                            oldContext, &newContext);

      CaptureChange(oldContext, newContext, assumeDifferenceHint);
    }

    if (!(mHintsHandled & nsChangeHint_ReconstructFrame)) {
      // If the frame gets regenerated, let it keep its old context,
      // which is important to maintain various invariants about
      // frame types matching their style contexts.
      // Note that this check even makes sense if we didn't call
      // CaptureChange because of copyFromContinuation being true,
      // since we'll have copied the existing context from the
      // previous continuation, so newContext == oldContext.
      aSelf->SetStyleContext(newContext);
    }
  }
  oldContext = nullptr;

  // do additional contexts
  // XXXbz might be able to avoid selector matching here in some
  // cases; won't worry about it for now.
  int32_t contextIndex = 0;
  for (nsStyleContext* oldExtraContext;
       (oldExtraContext = aSelf->GetAdditionalStyleContext(contextIndex));
       ++contextIndex) {
    nsRefPtr<nsStyleContext> newExtraContext;
    nsIAtom* const extraPseudoTag = oldExtraContext->GetPseudo();
    const nsCSSPseudoElements::Type extraPseudoType =
      oldExtraContext->GetPseudoType();
    NS_ASSERTION(extraPseudoTag &&
                 extraPseudoTag != nsCSSAnonBoxes::mozNonElement,
                 "extra style context is not pseudo element");
    if (extraPseudoType == nsCSSPseudoElements::ePseudo_AnonBox) {
      newExtraContext = styleSet->ResolveAnonymousBoxStyle(extraPseudoTag,
                                                           newContext);
    }
    else {
      // Don't expect XUL tree stuff here, since it needs a comparator and
      // all.
      NS_ASSERTION(extraPseudoType <
                     nsCSSPseudoElements::ePseudo_PseudoElementCount,
                   "Unexpected type");
      newExtraContext = styleSet->ResolvePseudoElementStyle(mContent->AsElement(),
                                                            extraPseudoType,
                                                            newContext,
                                                            nullptr);
    }

    MOZ_ASSERT(newExtraContext);

    if (oldExtraContext != newExtraContext) {
      CaptureChange(oldExtraContext, newExtraContext, assumeDifferenceHint);
      if (!(mHintsHandled & nsChangeHint_ReconstructFrame)) {
        aSelf->SetAdditionalStyleContext(contextIndex, newExtraContext);
      }
    }
  }
}

void
ElementRestyler::RestyleChildren(nsRestyleHint aChildRestyleHint)
{
  RestyleUndisplayedChildren(aChildRestyleHint);

  // Check whether we might need to create a new ::before frame.
  // There's no need to do this if we're planning to reframe already
  // or if we're not forcing restyles on kids.
  // It's also important to check mHintsHandled since we use
  // mFrame->StyleContext(), which is out of date if mHintsHandled has a
  // ReconstructFrame hint.  Using an out of date style context could
  // trigger assertions about mismatched rule trees.
  if (!(mHintsHandled & nsChangeHint_ReconstructFrame) &&
      aChildRestyleHint) {
    RestyleBeforePseudo();
  }

  // There is no need to waste time crawling into a frame's children
  // on a frame change.  The act of reconstructing frames will force
  // new style contexts to be resolved on all of this frame's
  // descendants anyway, so we want to avoid wasting time processing
  // style contexts that we're just going to throw away anyway. - dwh
  // It's also important to check mHintsHandled since reresolving the
  // kids would use mFrame->StyleContext(), which is out of date if
  // mHintsHandled has a ReconstructFrame hint; doing this could trigger
  // assertions about mismatched rule trees.
  nsIFrame *lastContinuation;
  if (!(mHintsHandled & nsChangeHint_ReconstructFrame)) {
    InitializeAccessibilityNotifications();

    for (nsIFrame* f = mFrame; f;
         f = GetNextContinuationWithSameStyle(f, f->StyleContext())) {
      lastContinuation = f;
      RestyleContentChildren(f, aChildRestyleHint);
    }

    SendAccessibilityNotifications();
  }

  // Check whether we might need to create a new ::after frame.
  // See comments above regarding :before.
  if (!(mHintsHandled & nsChangeHint_ReconstructFrame) &&
      aChildRestyleHint) {
    RestyleAfterPseudo(lastContinuation);
  }
}

void
ElementRestyler::RestyleUndisplayedChildren(nsRestyleHint aChildRestyleHint)
{
  // When the root element is display:none, we still construct *some*
  // frames that have the root element as their mContent, down to the
  // DocElementContainingBlock.
  bool checkUndisplayed;
  nsIContent* undisplayedParent;
  nsCSSFrameConstructor* frameConstructor = mPresContext->FrameConstructor();
  if (mFrame->StyleContext()->GetPseudo()) {
    checkUndisplayed = mFrame == frameConstructor->
                                   GetDocElementContainingBlock();
    undisplayedParent = nullptr;
  } else {
    checkUndisplayed = !!mFrame->GetContent();
    undisplayedParent = mFrame->GetContent();
  }
  if (checkUndisplayed &&
      // No need to do this if we're planning to reframe already.
      // It's also important to check mHintsHandled since we use
      // mFrame->StyleContext(), which is out of date if mHintsHandled
      // has a ReconstructFrame hint.  Using an out of date style
      // context could trigger assertions about mismatched rule trees.
      !(mHintsHandled & nsChangeHint_ReconstructFrame)) {
    UndisplayedNode* undisplayed =
      frameConstructor->GetAllUndisplayedContentIn(undisplayedParent);
    TreeMatchContext::AutoAncestorPusher pusher(mTreeMatchContext);
    if (undisplayed) {
      pusher.PushAncestorAndStyleScope(undisplayedParent);
    }
    for (; undisplayed; undisplayed = undisplayed->mNext) {
      NS_ASSERTION(undisplayedParent ||
                   undisplayed->mContent ==
                     mPresContext->Document()->GetRootElement(),
                   "undisplayed node child of null must be root");
      NS_ASSERTION(!undisplayed->mStyle->GetPseudo(),
                   "Shouldn't have random pseudo style contexts in the "
                   "undisplayed map");

      // Get the parent of the undisplayed content and check if it is a XBL
      // children element. Push the children element as an ancestor here because it does
      // not have a frame and would not otherwise be pushed as an ancestor.
      nsIContent* parent = undisplayed->mContent->GetParent();
      TreeMatchContext::AutoAncestorPusher insertionPointPusher(mTreeMatchContext);
      if (parent && nsContentUtils::IsContentInsertionPoint(parent)) {
        insertionPointPusher.PushAncestorAndStyleScope(parent);
      }

      nsRestyleHint thisChildHint = aChildRestyleHint;
      RestyleTracker::RestyleData undisplayedRestyleData;
      if (mRestyleTracker.GetRestyleData(undisplayed->mContent->AsElement(),
                                         &undisplayedRestyleData)) {
        thisChildHint =
          nsRestyleHint(thisChildHint | undisplayedRestyleData.mRestyleHint);
      }
      nsRefPtr<nsStyleContext> undisplayedContext;
      nsStyleSet* styleSet = mPresContext->StyleSet();
      if (thisChildHint) {
        undisplayedContext =
          styleSet->ResolveStyleFor(undisplayed->mContent->AsElement(),
                                    mFrame->StyleContext(),
                                    mTreeMatchContext);
      } else {
        undisplayedContext =
          styleSet->ReparentStyleContext(undisplayed->mStyle,
                                         mFrame->StyleContext(),
                                         undisplayed->mContent->AsElement());
      }
      const nsStyleDisplay* display = undisplayedContext->StyleDisplay();
      if (display->mDisplay != NS_STYLE_DISPLAY_NONE) {
        NS_ASSERTION(undisplayed->mContent,
                     "Must have undisplayed content");
        mChangeList->AppendChange(nullptr, undisplayed->mContent,
                                  NS_STYLE_HINT_FRAMECHANGE);
        // The node should be removed from the undisplayed map when
        // we reframe it.
      } else {
        // update the undisplayed node with the new context
        undisplayed->mStyle = undisplayedContext;
      }
    }
  }
}

void
ElementRestyler::RestyleBeforePseudo()
{
  // Make sure not to do this for pseudo-frames or frames that
  // can't have generated content.
  nsContainerFrame* cif;
  if (!mFrame->StyleContext()->GetPseudo() &&
      ((mFrame->GetStateBits() & NS_FRAME_MAY_HAVE_GENERATED_CONTENT) ||
       // Our content insertion frame might have gotten flagged
       ((cif = mFrame->GetContentInsertionFrame()) &&
        (cif->GetStateBits() & NS_FRAME_MAY_HAVE_GENERATED_CONTENT)))) {
    // Check for a new :before pseudo and an existing :before
    // frame, but only if the frame is the first continuation.
    nsIFrame* prevContinuation = mFrame->GetPrevContinuation();
    if (!prevContinuation) {
      // Checking for a :before frame is cheaper than getting the
      // :before style context.
      if (!nsLayoutUtils::GetBeforeFrame(mFrame) &&
          nsLayoutUtils::HasPseudoStyle(mFrame->GetContent(),
                                        mFrame->StyleContext(),
                                        nsCSSPseudoElements::ePseudo_before,
                                        mPresContext)) {
        // Have to create the new :before frame
        NS_UpdateHint(mHintsHandled, nsChangeHint_ReconstructFrame);
        mChangeList->AppendChange(mFrame, mContent,
                                  nsChangeHint_ReconstructFrame);
      }
    }
  }
}

/**
 * aFrame is the last continuation or block-in-inline sibling that this
 * ElementRestyler is restyling.
 */
void
ElementRestyler::RestyleAfterPseudo(nsIFrame* aFrame)
{
  // Make sure not to do this for pseudo-frames or frames that
  // can't have generated content.
  nsContainerFrame* cif;
  if (!aFrame->StyleContext()->GetPseudo() &&
      ((aFrame->GetStateBits() & NS_FRAME_MAY_HAVE_GENERATED_CONTENT) ||
       // Our content insertion frame might have gotten flagged
       ((cif = aFrame->GetContentInsertionFrame()) &&
        (cif->GetStateBits() & NS_FRAME_MAY_HAVE_GENERATED_CONTENT)))) {
    // Check for new :after content, but only if the frame is the
    // last continuation.
    nsIFrame* nextContinuation = aFrame->GetNextContinuation();

    if (!nextContinuation) {
      // Getting the :after frame is more expensive than getting the pseudo
      // context, so get the pseudo context first.
      if (nsLayoutUtils::HasPseudoStyle(aFrame->GetContent(),
                                        aFrame->StyleContext(),
                                        nsCSSPseudoElements::ePseudo_after,
                                        mPresContext) &&
          !nsLayoutUtils::GetAfterFrame(aFrame)) {
        // have to create the new :after frame
        NS_UpdateHint(mHintsHandled, nsChangeHint_ReconstructFrame);
        mChangeList->AppendChange(aFrame, mContent,
                                  nsChangeHint_ReconstructFrame);
      }
    }
  }
}

void
ElementRestyler::InitializeAccessibilityNotifications()
{
#ifdef ACCESSIBILITY
  // Notify a11y for primary frame only if it's a root frame of visibility
  // changes or its parent frame was hidden while it stays visible and
  // it is not inside a {ib} split or is the first frame of {ib} split.
  if (nsIPresShell::IsAccessibilityActive() &&
      !mFrame->GetPrevContinuation() &&
      !mFrame->FrameIsNonFirstInIBSplit()) {
    if (mDesiredA11yNotifications == eSendAllNotifications) {
      bool isFrameVisible = mFrame->StyleVisibility()->IsVisible();
      if (isFrameVisible != mWasFrameVisible) {
        if (isFrameVisible) {
          // Notify a11y the element (perhaps with its children) was shown.
          // We don't fall into this case if this element gets or stays shown
          // while its parent becomes hidden.
          mKidsDesiredA11yNotifications = eSkipNotifications;
          mOurA11yNotification = eNotifyShown;
        } else {
          // The element is being hidden; its children may stay visible, or
          // become visible after being hidden previously. If we'll find
          // visible children then we should notify a11y about that as if
          // they were inserted into tree. Notify a11y this element was
          // hidden.
          mKidsDesiredA11yNotifications = eNotifyIfShown;
          mOurA11yNotification = eNotifyHidden;
        }
      }
    } else if (mDesiredA11yNotifications == eNotifyIfShown &&
               mFrame->StyleVisibility()->IsVisible()) {
      // Notify a11y that element stayed visible while its parent was
      // hidden.
      mVisibleKidsOfHiddenElement.AppendElement(mFrame->GetContent());
      mKidsDesiredA11yNotifications = eSkipNotifications;
    }
  }
#endif
}

void
ElementRestyler::RestyleContentChildren(nsIFrame* aParent,
                                        nsRestyleHint aChildRestyleHint)
{
  nsIFrame::ChildListIterator lists(aParent);
  TreeMatchContext::AutoAncestorPusher ancestorPusher(mTreeMatchContext);
  if (!lists.IsDone()) {
    ancestorPusher.PushAncestorAndStyleScope(mContent);
  }
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      nsIFrame* child = childFrames.get();
      // Out-of-flows are reached through their placeholders.  Continuations
      // and block-in-inline splits are reached through those chains.
      if (!(child->GetStateBits() & NS_FRAME_OUT_OF_FLOW) &&
          !GetPrevContinuationWithSameStyle(child)) {
        // Get the parent of the child frame's content and check if it
        // is a XBL children element. Push the children element as an
        // ancestor here because it does not have a frame and would not
        // otherwise be pushed as an ancestor.

        // Check if the frame has a content because |child| may be a
        // nsPageFrame that does not have a content.
        nsIContent* parent = child->GetContent() ? child->GetContent()->GetParent() : nullptr;
        TreeMatchContext::AutoAncestorPusher insertionPointPusher(mTreeMatchContext);
        if (parent && nsContentUtils::IsContentInsertionPoint(parent)) {
          insertionPointPusher.PushAncestorAndStyleScope(parent);
        }

        // only do frames that are in flow
        if (nsGkAtoms::placeholderFrame == child->GetType()) { // placeholder
          // get out of flow frame and recur there
          nsIFrame* outOfFlowFrame =
            nsPlaceholderFrame::GetRealFrameForPlaceholder(child);
          NS_ASSERTION(outOfFlowFrame, "no out-of-flow frame");
          NS_ASSERTION(outOfFlowFrame != mResolvedChild,
                       "out-of-flow frame not a true descendant");

          // |nsFrame::GetParentStyleContextFrame| checks being out
          // of flow so that this works correctly.
          do {
            if (GetPrevContinuationWithSameStyle(outOfFlowFrame)) {
              // Later continuations are likely restyled as a result of
              // the restyling of the previous continuation.
              // (Currently that's always true, but it's likely to
              // change if we implement overflow:fragments or similar.)
              continue;
            }
            ElementRestyler oofRestyler(*this, outOfFlowFrame,
                                        FOR_OUT_OF_FLOW_CHILD);
            oofRestyler.Restyle(aChildRestyleHint);
          } while ((outOfFlowFrame = outOfFlowFrame->GetNextContinuation()));

          // reresolve placeholder's context under the same parent
          // as the out-of-flow frame
          ElementRestyler phRestyler(*this, child, 0);
          phRestyler.Restyle(aChildRestyleHint);
        }
        else {  // regular child frame
          if (child != mResolvedChild) {
            ElementRestyler childRestyler(*this, child, 0);
            childRestyler.Restyle(aChildRestyleHint);
          }
        }
      }
    }
  }
  // XXX need to do overflow frames???
}

void
ElementRestyler::SendAccessibilityNotifications()
{
#ifdef ACCESSIBILITY
  // Send notifications about visibility changes.
  if (mOurA11yNotification == eNotifyShown) {
    nsAccessibilityService* accService = nsIPresShell::AccService();
    if (accService) {
      nsIPresShell* presShell = mFrame->PresContext()->GetPresShell();
      nsIContent* content = mFrame->GetContent();

      accService->ContentRangeInserted(presShell, content->GetParent(),
                                       content,
                                       content->GetNextSibling());
    }
  } else if (mOurA11yNotification == eNotifyHidden) {
    nsAccessibilityService* accService = nsIPresShell::AccService();
    if (accService) {
      nsIPresShell* presShell = mFrame->PresContext()->GetPresShell();
      nsIContent* content = mFrame->GetContent();
      accService->ContentRemoved(presShell, content->GetParent(), content);

      // Process children staying shown.
      uint32_t visibleContentCount = mVisibleKidsOfHiddenElement.Length();
      for (uint32_t idx = 0; idx < visibleContentCount; idx++) {
        nsIContent* childContent = mVisibleKidsOfHiddenElement[idx];
        accService->ContentRangeInserted(presShell, childContent->GetParent(),
                                         childContent,
                                         childContent->GetNextSibling());
      }
      mVisibleKidsOfHiddenElement.Clear();
    }
  }
#endif
}

static inline nsIFrame*
GetNextBlockInInlineSibling(FramePropertyTable* aPropTable, nsIFrame* aFrame)
{
  NS_ASSERTION(!aFrame->GetPrevContinuation(),
               "must start with the first continuation");
  // Might we have ib-split siblings?
  if (!(aFrame->GetStateBits() & NS_FRAME_PART_OF_IBSPLIT)) {
    // nothing more to do here
    return nullptr;
  }

  return static_cast<nsIFrame*>
    (aPropTable->Get(aFrame, nsIFrame::IBSplitSibling()));
}

void
RestyleManager::ComputeStyleChangeFor(nsIFrame*          aFrame,
                                      nsStyleChangeList* aChangeList,
                                      nsChangeHint       aMinChange,
                                      RestyleTracker&    aRestyleTracker,
                                      nsRestyleHint      aRestyleHint)
{
  PROFILER_LABEL("RestyleManager", "ComputeStyleChangeFor",
    js::ProfileEntry::Category::CSS);

  nsIContent *content = aFrame->GetContent();
  if (aMinChange) {
    aChangeList->AppendChange(aFrame, content, aMinChange);
  }

  NS_ASSERTION(!aFrame->GetPrevContinuation(),
               "must start with the first continuation");

  // We want to start with this frame and walk all its next-in-flows,
  // as well as all its ib-split siblings and their next-in-flows,
  // reresolving style on all the frames we encounter in this walk that
  // we didn't reach already.  In the normal case, this will mean only
  // restyling the first two block-in-inline splits and no
  // continuations, and skipping everything else.  However, when we have
  // a style change targeted at an element inside a context where styles
  // vary between continuations (e.g., a style change on an element that
  // extends from inside a styled ::first-line to outside of that first
  // line), we might restyle more than that.

  FramePropertyTable* propTable = mPresContext->PropertyTable();

  TreeMatchContext treeMatchContext(true,
                                    nsRuleWalker::eRelevantLinkUnvisited,
                                    mPresContext->Document());
  Element* parent =
    content ? content->GetParentElementCrossingShadowRoot() : nullptr;
  treeMatchContext.InitAncestors(parent);
  nsTArray<nsIContent*> visibleKidsOfHiddenElement;
  for (nsIFrame* ibSibling = aFrame; ibSibling;
       ibSibling = GetNextBlockInInlineSibling(propTable, ibSibling)) {
    // Outer loop over ib-split siblings
    for (nsIFrame* cont = ibSibling; cont; cont = cont->GetNextContinuation()) {
      if (GetPrevContinuationWithSameStyle(cont)) {
        // We already handled this element when dealing with its earlier
        // continuation.
        continue;
      }

      // Inner loop over next-in-flows of the current frame
      ElementRestyler restyler(mPresContext, cont, aChangeList,
                               aMinChange, aRestyleTracker,
                               treeMatchContext,
                               visibleKidsOfHiddenElement);

      restyler.Restyle(aRestyleHint);

      if (restyler.HintsHandledForFrame() & nsChangeHint_ReconstructFrame) {
        // If it's going to cause a framechange, then don't bother
        // with the continuations or ib-split siblings since they'll be
        // clobbered by the frame reconstruct anyway.
        NS_ASSERTION(!cont->GetPrevContinuation(),
                     "continuing frame had more severe impact than first-in-flow");
        return;
      }
    }
  }
}

} // namespace mozilla
