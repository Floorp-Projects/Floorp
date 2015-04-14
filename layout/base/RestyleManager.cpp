/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Code responsible for managing style changes: tracking what style
 * changes need to happen, scheduling them, and doing them.
 */

#include <algorithm> // For std::max
#include "RestyleManager.h"
#include "mozilla/EventStates.h"
#include "nsLayoutUtils.h"
#include "AnimationCommon.h" // For GetLayerAnimationInfo
#include "FrameLayerBuilder.h"
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
#include "RestyleTrackerInlines.h"
#include "nsSMILAnimationController.h"

#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#endif

namespace mozilla {

using namespace layers;

#define LOG_RESTYLE_CONTINUE(reason_, ...) \
  LOG_RESTYLE("continuing restyle since " reason_, ##__VA_ARGS__)

#ifdef RESTYLE_LOGGING
static nsCString
FrameTagToString(const nsIFrame* aFrame)
{
  nsCString result;
  aFrame->ListTag(result);
  return result;
}
#endif

RestyleManager::RestyleManager(nsPresContext* aPresContext)
  : mPresContext(aPresContext)
  , mDoRebuildAllStyleData(false)
  , mInRebuildAllStyleData(false)
  , mObservingRefreshDriver(false)
  , mInStyleRefresh(false)
  , mSkipAnimationRules(false)
  , mHavePendingNonAnimationRestyles(false)
  , mHoverGeneration(0)
  , mRebuildAllExtraHint(nsChangeHint(0))
  , mRebuildAllRestyleHint(nsRestyleHint(0))
  , mLastUpdateForThrottledAnimations(aPresContext->RefreshDriver()->
                                        MostRecentRefresh())
  , mAnimationGeneration(0)
  , mReframingStyleContexts(nullptr)
  , mPendingRestyles(ELEMENT_HAS_PENDING_RESTYLE |
                     ELEMENT_IS_POTENTIAL_RESTYLE_ROOT)
#ifdef DEBUG
  , mIsProcessingRestyles(false)
#endif
#ifdef RESTYLE_LOGGING
  , mLoggingDepth(0)
#endif
{
  mPendingRestyles.Init(this);
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

static nsIFrame*
GetNearestAncestorFrame(nsIContent* aContent)
{
  nsIFrame* ancestorFrame = nullptr;
  for (nsIContent* ancestor = aContent->GetParent();
       ancestor && !ancestorFrame;
       ancestor = ancestor->GetParent()) {
    ancestorFrame = ancestor->GetPrimaryFrame();
  }
  return ancestorFrame;
}

static void
DoApplyRenderingChangeToTree(nsIFrame* aFrame,
                             nsChangeHint aChange);

/**
 * Sync views on aFrame and all of aFrame's descendants (following placeholders),
 * if aChange has nsChangeHint_SyncFrameView.
 * Calls DoApplyRenderingChangeToTree on all aFrame's out-of-flow descendants
 * (following placeholders), if aChange has nsChangeHint_RepaintFrame.
 * aFrame should be some combination of nsChangeHint_SyncFrameView,
 * nsChangeHint_RepaintFrame, nsChangeHint_UpdateOpacityLayer and
 * nsChangeHint_SchedulePaint, nothing else.
*/
static void
SyncViewsAndInvalidateDescendants(nsIFrame* aFrame,
                                  nsChangeHint aChange)
{
  NS_PRECONDITION(gInApplyRenderingChangeToTree,
                  "should only be called within ApplyRenderingChangeToTree");
  NS_ASSERTION(aChange == (aChange & (nsChangeHint_RepaintFrame |
                                      nsChangeHint_SyncFrameView |
                                      nsChangeHint_UpdateOpacityLayer |
                                      nsChangeHint_SchedulePaint)),
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
    MOZ_ASSERT(aFrame->GetType() == nsGkAtoms::svgOuterSVGAnonChildFrame,
               "Where is the nsSVGOuterSVGFrame's anon child??");
  }
  MOZ_ASSERT(aFrame->IsFrameOfType(nsIFrame::eSVG |
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
                              nsChangeHint_UpdateOpacityLayer |
                              nsChangeHint_SchedulePaint)));
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
        NS_ASSERTION(aFrame->GetContent()->IsSVGElement(nsGkAtoms::textPath),
                     "expected frame for a <textPath> element");
        nsIFrame* text = nsLayoutUtils::GetClosestFrameOfType(
                                                      aFrame,
                                                      nsGkAtoms::svgTextFrame);
        NS_ASSERTION(text, "expected to find an ancestor SVGTextFrame");
        static_cast<SVGTextFrame*>(text)->NotifyGlyphMetricsChange();
      } else {
        MOZ_ASSERT(false, "unexpected frame got nsChangeHint_UpdateTextPath");
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
    if (aChange & nsChangeHint_SchedulePaint) {
      needInvalidatingPaint = true;
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
  // We check StyleDisplay()->HasTransformStyle() in addition to checking
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

  // Trigger rendering updates by damaging this frame and any
  // continuations of this frame.
#ifdef DEBUG
  gInApplyRenderingChangeToTree = true;
#endif
  if (aChange & nsChangeHint_RepaintFrame) {
    // If the frame's background is propagated to an ancestor, walk up to
    // that ancestor and apply the RepaintFrame change hint to it.
    nsStyleContext *bgSC;
    nsIFrame* propagatedFrame = aFrame;
    while (!nsCSSRendering::FindBackground(propagatedFrame, &bgSC)) {
      propagatedFrame = propagatedFrame->GetParent();
      NS_ASSERTION(aFrame, "root frame must paint");
    }

    if (propagatedFrame != aFrame) {
      DoApplyRenderingChangeToTree(propagatedFrame, nsChangeHint_RepaintFrame);
      aChange = NS_SubtractHint(aChange, nsChangeHint_RepaintFrame);
      if (!aChange) {
        return;
      }
    }
  }
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
    // Move the frame
    if (display->mPosition == NS_STYLE_POSITION_STICKY) {
      if (display->IsInnerTableStyle()) {
        // We don't currently support sticky positioning of inner table
        // elements (bug 975644). Bail.
        return true;
      }

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
  nsRenderingContext rc(
    aFrame->PresContext()->PresShell()->CreateReferenceRenderingContext());

  // Construct a bogus parent reflow state so that there's a usable
  // containing block reflow state.
  nsIFrame* parentFrame = aFrame->GetParent();
  WritingMode parentWM = parentFrame->GetWritingMode();
  WritingMode frameWM = aFrame->GetWritingMode();
  LogicalSize parentSize = parentFrame->GetLogicalSize();

  nsFrameState savedState = parentFrame->GetStateBits();
  nsHTMLReflowState parentReflowState(aFrame->PresContext(), parentFrame,
                                      &rc, parentSize);
  parentFrame->RemoveStateBits(~nsFrameState(0));
  parentFrame->AddStateBits(savedState);

  NS_WARN_IF_FALSE(parentSize.ISize(parentWM) != NS_INTRINSICSIZE &&
                   parentSize.BSize(parentWM) != NS_INTRINSICSIZE,
                   "parentSize should be valid");
  parentReflowState.SetComputedISize(std::max(parentSize.ISize(parentWM), 0));
  parentReflowState.SetComputedBSize(std::max(parentSize.BSize(parentWM), 0));
  parentReflowState.ComputedPhysicalMargin().SizeTo(0, 0, 0, 0);

  parentReflowState.ComputedPhysicalPadding() = parentFrame->GetUsedPadding();
  parentReflowState.ComputedPhysicalBorderPadding() =
    parentFrame->GetUsedBorderAndPadding();
  LogicalSize availSize = parentSize.ConvertTo(frameWM, parentWM);
  availSize.BSize(frameWM) = NS_INTRINSICSIZE;

  ViewportFrame* viewport = do_QueryFrame(parentFrame);
  nsSize cbSize = viewport ?
    viewport->AdjustReflowStateAsContainingBlock(&parentReflowState).Size()
    : aFrame->GetContainingBlock()->GetSize();
  const nsMargin& parentBorder =
    parentReflowState.mStyleBorder->GetComputedBorder();
  cbSize -= nsSize(parentBorder.LeftRight(), parentBorder.TopBottom());
  nsHTMLReflowState reflowState(aFrame->PresContext(), parentReflowState,
                                aFrame, availSize,
                                cbSize.width, cbSize.height);
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
  if (aFrame->GetStateBits() & NS_FRAME_FIRST_REFLOW) {
    dirtyBits = nsFrameState(0);
  } else if (aHint & nsChangeHint_NeedDirtyReflow) {
    dirtyBits = NS_FRAME_IS_DIRTY;
  } else {
    dirtyBits = NS_FRAME_HAS_DIRTY_CHILDREN;
  }

  // If we're not going to clear any intrinsic sizes on the frames, and
  // there are no dirty bits to set, then there's nothing to do.
  if (dirtyType == nsIPresShell::eResize && !dirtyBits)
    return;

  do {
    mPresContext->PresShell()->FrameNeedsReflow(aFrame, dirtyType, dirtyBits);
    aFrame = nsLayoutUtils::GetNextContinuationOrIBSplitSibling(aFrame);
  } while (aFrame);
}

void
RestyleManager::AddSubtreeToOverflowTracker(nsIFrame* aFrame) 
{
  mOverflowChangedTracker.AddFrame(
      aFrame,
      OverflowChangedTracker::CHILDREN_CHANGED);
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
          if (cont->IsAbsPosContaininingBlock()) {
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
      // We could also have problems with triggering of CSS transitions
      // on elements whose frames are reconstructed, since we depend on
      // the reconstruction happening synchronously.
      FrameConstructor()->RecreateFramesForContent(content, false,
        nsCSSFrameConstructor::REMOVE_FOR_RECONSTRUCTION, nullptr);
    } else {
      NS_ASSERTION(frame, "This shouldn't happen");

      if ((frame->GetStateBits() & NS_FRAME_SVG_LAYOUT) &&
          (frame->GetStateBits() & NS_FRAME_IS_NONDISPLAY)) {
        // frame does not maintain overflow rects, so avoid calling
        // FinishAndStoreOverflow on it:
        hint = NS_SubtractHint(hint,
                 NS_CombineHint(
                   NS_CombineHint(nsChangeHint_UpdateOverflow,
                                  nsChangeHint_ChildrenOnlyTransform),
                   NS_CombineHint(nsChangeHint_UpdatePostTransformOverflow,
                                  nsChangeHint_UpdateParentOverflow)));
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
      if (hint & nsChangeHint_InvalidateRenderingObservers) {
        nsSVGEffects::InvalidateRenderingObservers(frame);
      }
      if (hint & nsChangeHint_NeedReflow) {
        StyleChangeReflow(frame, hint);
        didReflowThisFrame = true;
      }

      if (hint & (nsChangeHint_RepaintFrame | nsChangeHint_SyncFrameView |
                  nsChangeHint_UpdateOpacityLayer | nsChangeHint_UpdateTransformLayer |
                  nsChangeHint_ChildrenOnlyTransform | nsChangeHint_SchedulePaint)) {
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
                   nsChangeHint_UpdateParentOverflow |
                   nsChangeHint_UpdateSubtreeOverflow))) {
        if (hint & nsChangeHint_UpdateSubtreeOverflow) {
          for (nsIFrame *cont = frame; cont; cont =
                 nsLayoutUtils::GetNextContinuationOrIBSplitSibling(cont)) {
            AddSubtreeToOverflowTracker(cont);
          }
          // The work we just did in AddSubtreeToOverflowTracker
          // subsumes some of the other hints:
          hint = NS_SubtractHint(hint,
                   NS_CombineHint(nsChangeHint_UpdateOverflow,
                                  nsChangeHint_UpdatePostTransformOverflow));
        }
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
            MOZ_ASSERT(childFrame->IsFrameOfType(nsIFrame::eSVG),
                       "Not expecting non-SVG children");
            // If |childFrame| is dirty or has dirty children, we don't bother
            // updating overflows since that will happen when it's reflowed.
            if (!(childFrame->GetStateBits() &
                  (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN))) {
              mOverflowChangedTracker.AddFrame(childFrame,
                           OverflowChangedTracker::CHILDREN_CHANGED);
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
          if (hint & (nsChangeHint_UpdateOverflow |
                      nsChangeHint_UpdatePostTransformOverflow)) {
            OverflowChangedTracker::ChangeKind changeKind;
            // If we have both nsChangeHint_UpdateOverflow and
            // nsChangeHint_UpdatePostTransformOverflow,
            // CHILDREN_CHANGED is selected as it is
            // strictly stronger.
            if (hint & nsChangeHint_UpdateOverflow) {
              changeKind = OverflowChangedTracker::CHILDREN_CHANGED;
            } else {
              changeKind = OverflowChangedTracker::TRANSFORM_CHANGED;
            }
            for (nsIFrame *cont = frame; cont; cont =
                   nsLayoutUtils::GetNextContinuationOrIBSplitSibling(cont)) {
              mOverflowChangedTracker.AddFrame(cont, changeKind);
            }
          }
          // UpdateParentOverflow hints need to be processed in addition
          // to the above, since if the processing of the above hints
          // yields no change, the update will not propagate to the
          // parent.
          if (hint & nsChangeHint_UpdateParentOverflow) {
            MOZ_ASSERT(frame->GetParent(),
                       "shouldn't get style hints for the root frame");
            for (nsIFrame *cont = frame; cont; cont =
                   nsLayoutUtils::GetNextContinuationOrIBSplitSibling(cont)) {
              mOverflowChangedTracker.AddFrame(cont->GetParent(),
                                   OverflowChangedTracker::CHILDREN_CHANGED);
            }
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
      if (!css::CommonAnimationManager::ContentOrAncestorHasAnimation(changeData->mContent)) {
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
  MOZ_ASSERT(mReframingStyleContexts, "should have rsc");
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
  if (mPresContext->UsesRootEMUnits() && aPrimaryFrame &&
      !mInRebuildAllStyleData) {
    nsStyleContext *oldContext = aPrimaryFrame->StyleContext();
    if (!oldContext->GetParent()) { // check that we're the root element
      nsRefPtr<nsStyleContext> newContext = mPresContext->StyleSet()->
        ResolveStyleFor(aElement, nullptr /* == oldContext->GetParent() */);
      if (oldContext->StyleFont()->mFont.size !=
          newContext->StyleFont()->mFont.size) {
        // The basis for 'rem' units has changed.
        mRebuildAllRestyleHint |= aRestyleHint;
        NS_UpdateHint(mRebuildAllExtraHint, aMinHint);
        StartRebuildAllStyleData(aRestyleTracker);
        return;
      }
    }
  }

  if (aMinHint & nsChangeHint_ReconstructFrame) {
    FrameConstructor()->RecreateFramesForContent(aElement, false,
      nsCSSFrameConstructor::REMOVE_FOR_RECONSTRUCTION, nullptr);
  } else if (aPrimaryFrame) {
    ComputeAndProcessStyleChange(aPrimaryFrame, aMinHint, aRestyleTracker,
                                 aRestyleHint);
  } else if (aRestyleHint & ~eRestyle_LaterSiblings) {
    // We're restyling an element with no frame, so we should try to
    // make one if its new style says it should have one.  But in order
    // to try to honor the restyle hint (which we'd like to do so that,
    // for example, an animation-only style flush doesn't flush other
    // buffered style changes), we only do this if the restyle hint says
    // we have *some* restyling for this frame.  This means we'll
    // potentially get ahead of ourselves in that case, but not as much
    // as we would if we didn't check the restyle hint.
    nsStyleContext* newContext =
      FrameConstructor()->MaybeRecreateFramesForElement(aElement);
    if (newContext &&
        newContext->StyleDisplay()->mDisplay == NS_STYLE_DISPLAY_CONTENTS) {
      // Style change for a display:contents node that did not recreate frames.
      ComputeAndProcessStyleChange(newContext, aElement, aMinHint,
                                   aRestyleTracker, aRestyleHint);
    }
  }
}

RestyleManager::ReframingStyleContexts::ReframingStyleContexts(
                                          RestyleManager* aRestyleManager)
  : mRestyleManager(aRestyleManager)
  , mRestorePointer(mRestyleManager->mReframingStyleContexts)
{
  MOZ_ASSERT(!mRestyleManager->mReframingStyleContexts,
             "shouldn't construct recursively");
  mRestyleManager->mReframingStyleContexts = this;
}

RestyleManager::ReframingStyleContexts::~ReframingStyleContexts()
{
  // Before we go away, we need to flush out any frame construction that
  // was enqueued, so that we start transitions.
  // Note that this is a little bit evil in that we're calling into code
  // that calls our member functions from our destructor, but it's at
  // the beginning of our destructor, so it shouldn't be too bad.
  mRestyleManager->mPresContext->FrameConstructor()->CreateNeededFrames();
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

/* static */ uint64_t
RestyleManager::GetMaxAnimationGenerationForFrame(nsIFrame* aFrame)
{
  nsIContent* content = aFrame->GetContent();
  if (!content || !content->IsElement()) {
    return 0;
  }

  nsCSSPseudoElements::Type pseudoType =
    aFrame->StyleContext()->GetPseudoType();
  AnimationPlayerCollection* transitions =
    aFrame->PresContext()->TransitionManager()->GetAnimations(
      content->AsElement(), pseudoType, false /* don't create */);
  AnimationPlayerCollection* animations =
    aFrame->PresContext()->AnimationManager()->GetAnimations(
      content->AsElement(), pseudoType, false /* don't create */);

  return std::max(transitions ? transitions->mAnimationGeneration : 0,
                  animations ? animations->mAnimationGeneration : 0);
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
RestyleManager::RebuildAllStyleData(nsChangeHint aExtraHint,
                                    nsRestyleHint aRestyleHint)
{
  NS_ASSERTION(!(aExtraHint & nsChangeHint_ReconstructFrame),
               "Should not reconstruct the root of the frame tree.  "
               "Use ReconstructDocElementHierarchy instead.");

  NS_UpdateHint(mRebuildAllExtraHint, aExtraHint);
  mRebuildAllRestyleHint |= aRestyleHint;

  // Processing the style changes could cause a flush that propagates to
  // the parent frame and thus destroys the pres shell, so we must hold
  // a reference.
  nsCOMPtr<nsIPresShell> presShell = mPresContext->GetPresShell();
  if (!presShell || !presShell->GetRootFrame()) {
    mDoRebuildAllStyleData = false;
    return;
  }

  // Make sure that the viewmanager will outlive the presshell
  nsRefPtr<nsViewManager> vm = presShell->GetViewManager();

  // We may reconstruct frames below and hence process anything that is in the
  // tree. We don't want to get notified to process those items again after.
  presShell->GetDocument()->FlushPendingNotifications(Flush_ContentAndNotify);

  nsAutoScriptBlocker scriptBlocker;

  mDoRebuildAllStyleData = true;

  ProcessPendingRestyles();
}

void
RestyleManager::StartRebuildAllStyleData(RestyleTracker& aRestyleTracker)
{
  MOZ_ASSERT(mIsProcessingRestyles);

  nsIFrame* rootFrame = mPresContext->PresShell()->GetRootFrame();
  if (!rootFrame) {
    // No need to do anything.
    return;
  }

  mInRebuildAllStyleData = true;

  // Tell the style set to get the old rule tree out of the way
  // so we can recalculate while maintaining rule tree immutability
  nsresult rv = mPresContext->StyleSet()->BeginReconstruct();
  if (NS_FAILED(rv)) {
    MOZ_CRASH("unable to rebuild style data");
  }

  nsRestyleHint restyleHint = mRebuildAllRestyleHint;
  nsChangeHint changeHint = mRebuildAllExtraHint;
  mRebuildAllExtraHint = nsChangeHint(0);
  mRebuildAllRestyleHint = nsRestyleHint(0);

  restyleHint |= eRestyle_ForceDescendants;

  if (!(restyleHint & eRestyle_Subtree) &&
      (restyleHint & ~(eRestyle_Force | eRestyle_ForceDescendants))) {
    // We want this hint to apply to the root node's primary frame
    // rather than the root frame, since it's the primary frame that has
    // the styles for the root element (rather than the ancestors of the
    // primary frame whose mContent is the root node but which have
    // different styles).  If we use up the hint for one of the
    // ancestors that we hit first, then we'll fail to do the restyling
    // we need to do.
    Element* root = mPresContext->Document()->GetRootElement();
    if (root) {
      // If the root element is gone, dropping the hint on the floor
      // should be fine.
      aRestyleTracker.AddPendingRestyle(root, restyleHint, nsChangeHint(0));
    }
    restyleHint = nsRestyleHint(0);
  }

  // Recalculate all of the style contexts for the document, from the
  // root frame.  We can't do this with a change hint, since we can't
  // post a change hint for the root frame.
  // Note that we can ignore the return value of ComputeStyleChangeFor
  // because we never need to reframe the root frame.
  // XXX Does it matter that we're passing aExtraHint to the real root
  // frame and not the root node's primary frame?  (We could do
  // roughly what we do for aRestyleHint above.)
  ComputeAndProcessStyleChange(rootFrame,
                               changeHint, aRestyleTracker, restyleHint);
}

void
RestyleManager::FinishRebuildAllStyleData()
{
  MOZ_ASSERT(mInRebuildAllStyleData, "bad caller");

  // Tell the style set it's safe to destroy the old rule tree.  We
  // must do this after the ProcessRestyledFrames call in case the
  // change list has frame reconstructs in it (since frames to be
  // reconstructed will still have their old style context pointers
  // until they are destroyed).
  mPresContext->StyleSet()->EndReconstruct();

  mInRebuildAllStyleData = false;
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
  MOZ_ASSERT(!mIsProcessingRestyles,
             "Nesting calls to ProcessPendingRestyles?");
#ifdef DEBUG
  mIsProcessingRestyles = true;
#endif

  // Before we process any restyles, we need to ensure that style
  // resulting from any animations is up-to-date, so that if any style
  // changes we cause trigger transitions, we have the correct old style
  // for starting the transition.
  bool haveNonAnimation =
    mHavePendingNonAnimationRestyles || mDoRebuildAllStyleData;
  if (haveNonAnimation) {
    IncrementAnimationGeneration();
    UpdateOnlyAnimationStyles();
  } else {
    // If we don't have non-animation style updates, then we have queued
    // up animation style updates from the refresh driver tick.  This
    // doesn't necessarily include *all* animation style updates, since
    // we might be suppressing main-thread updates for some animations,
    // so we don't want to call UpdateOnlyAnimationStyles, which updates
    // all animations.  In other words, the work that we're about to do
    // to process the pending restyles queue is a *subset* of the work
    // that UpdateOnlyAnimationStyles would do, since we're *not*
    // updating transitions that are running on the compositor thread
    // and suppressed on the main thread.
    //
    // But when we update those styles, we want to suppress updates to
    // transitions just like we do in UpdateOnlyAnimationStyles.  So we
    // want to tell the transition manager to act as though we're in
    // UpdateOnlyAnimationStyles.
    //
    // FIXME: In the future, we might want to refactor the way the
    // animation and transition manager do their refresh driver ticks so
    // that we can use UpdateOnlyAnimationStyles, with a different
    // boolean argument, for this update as well, instead of having them
    // post style updates in their WillRefresh methods.
    mPresContext->TransitionManager()->SetInAnimationOnlyStyleUpdate(true);
  }

  ProcessRestyles(mPendingRestyles);

  if (!haveNonAnimation) {
    mPresContext->TransitionManager()->SetInAnimationOnlyStyleUpdate(false);
  }

#ifdef DEBUG
  mIsProcessingRestyles = false;
#endif

  NS_ASSERTION(haveNonAnimation || !mHavePendingNonAnimationRestyles,
               "should not have added restyles");
  mHavePendingNonAnimationRestyles = false;

  if (mDoRebuildAllStyleData) {
    // We probably wasted a lot of work up above, but this seems safest
    // and it should be rarely used.
    // This might add us as a refresh observer again; that's ok.
    ProcessPendingRestyles();

    NS_ASSERTION(!mDoRebuildAllStyleData,
                 "repeatedly setting mDoRebuildAllStyleData?");
  }

  MOZ_ASSERT(!mInRebuildAllStyleData,
             "should have called FinishRebuildAllStyleData");
}

void
RestyleManager::BeginProcessingRestyles(RestyleTracker& aRestyleTracker)
{
  // Make sure to not rebuild quote or counter lists while we're
  // processing restyles
  mPresContext->FrameConstructor()->BeginUpdate();

  mInStyleRefresh = true;

  if (ShouldStartRebuildAllFor(aRestyleTracker)) {
    mDoRebuildAllStyleData = false;
    StartRebuildAllStyleData(aRestyleTracker);
  }
}

void
RestyleManager::EndProcessingRestyles()
{
  FlushOverflowChangedTracker();

  // Set mInStyleRefresh to false now, since the EndUpdate call might
  // add more restyles.
  mInStyleRefresh = false;

  if (mInRebuildAllStyleData) {
    FinishRebuildAllStyleData();
  }

  mPresContext->FrameConstructor()->EndUpdate();

#ifdef DEBUG
  mPresContext->PresShell()->VerifyStyleTree();
#endif
}

void
RestyleManager::UpdateOnlyAnimationStyles()
{
  TimeStamp now = mPresContext->RefreshDriver()->MostRecentRefresh();
  bool doCSS = mLastUpdateForThrottledAnimations != now;
  mLastUpdateForThrottledAnimations = now;

  bool doSMIL = false;
  nsIDocument* document = mPresContext->Document();
  nsSMILAnimationController* animationController = nullptr;
  if (document->HasAnimationController()) {
    animationController = document->GetAnimationController();
    // FIXME:  Ideally, we only want to do this if animation timelines
    // have advanced.  However, different SMIL animations could be
    // getting their time from different outermost SVG elements, so
    // finding all of them might be a pain.  So this could be optimized
    // to set doSMIL to true in fewer cases.
    doSMIL = true;
  }

  if (!doCSS && !doSMIL) {
    return;
  }

  nsTransitionManager* transitionManager = mPresContext->TransitionManager();
  nsAnimationManager* animationManager = mPresContext->AnimationManager();

  transitionManager->SetInAnimationOnlyStyleUpdate(true);

  RestyleTracker tracker(ELEMENT_HAS_PENDING_ANIMATION_ONLY_RESTYLE |
                         ELEMENT_IS_POTENTIAL_ANIMATION_ONLY_RESTYLE_ROOT);
  tracker.Init(this);

  if (doCSS) {
    // FIXME:  We should have the transition manager and animation manager
    // add only the elements for which animations are currently throttled
    // (i.e., animating on the compositor with main-thread style updates
    // suppressed).
    transitionManager->AddStyleUpdatesTo(tracker);
    animationManager->AddStyleUpdatesTo(tracker);
  }

  if (doSMIL) {
    animationController->AddStyleUpdatesTo(tracker);
  }

  ProcessRestyles(tracker);

  transitionManager->SetInAnimationOnlyStyleUpdate(false);
}

void
RestyleManager::PostRestyleEvent(Element* aElement,
                                 nsRestyleHint aRestyleHint,
                                 nsChangeHint aMinChangeHint)
{
  if (MOZ_UNLIKELY(!mPresContext) ||
      MOZ_UNLIKELY(mPresContext->PresShell()->IsDestroying())) {
    return;
  }

  if (aRestyleHint == 0 && !aMinChangeHint) {
    // Nothing to do here
    return;
  }

  mPendingRestyles.AddPendingRestyle(aElement, aRestyleHint, aMinChangeHint);

  // Set mHavePendingNonAnimationRestyles for any restyle that could
  // possibly contain non-animation styles (i.e., those that require us
  // to do an animation-only style flush before processing style changes
  // to ensure correct initialization of CSS transitions).
  if (aRestyleHint & ~eRestyle_AllHintsWithAnimations) {
    mHavePendingNonAnimationRestyles = true;
  }

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
RestyleManager::PostRebuildAllStyleDataEvent(nsChangeHint aExtraHint,
                                             nsRestyleHint aRestyleHint)
{
  NS_ASSERTION(!(aExtraHint & nsChangeHint_ReconstructFrame),
               "Should not reconstruct the root of the frame tree.  "
               "Use ReconstructDocElementHierarchy instead.");

  mDoRebuildAllStyleData = true;
  NS_UpdateHint(mRebuildAllExtraHint, aExtraHint);
  mRebuildAllRestyleHint |= aRestyleHint;

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
    nsIFrame* providerFrame;
    aParentContext = aFrame->GetParentStyleContext(&providerFrame);
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
/* static */ bool
RestyleManager::TryStartingTransition(nsPresContext* aPresContext,
                                      nsIContent* aContent,
                                      nsStyleContext* aOldStyleContext,
                                      nsRefPtr<nsStyleContext>*
                                        aNewStyleContext /* inout */)
{
  if (!aContent || !aContent->IsElement()) {
    return false;
  }

  // Notify the transition manager.  If it starts a transition,
  // it might modify the new style context.
  nsRefPtr<nsStyleContext> sc = *aNewStyleContext;
  aPresContext->TransitionManager()->StyleContextChanged(
    aContent->AsElement(), aOldStyleContext, aNewStyleContext);
  return *aNewStyleContext != sc;
}

static dom::Element*
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
 * Some pseudo-elements actually have a content node created for them,
 * whereas others have only a frame but not a content node.  In some
 * cases, we want to support style attributes or states on those
 * elements.  For those pseudo-elements, we need to pass the
 * anonymous pseudo-element content to selector matching processes in
 * addition to the element that the pseudo-element is for; in other
 * cases we should pass null instead.  This function returns the
 * pseudo-element content that we should pass.
 */
static dom::Element*
PseudoElementForStyleContext(nsIFrame* aFrame,
                             nsCSSPseudoElements::Type aPseudoType)
{
  if (aPseudoType >= nsCSSPseudoElements::ePseudo_PseudoElementCount) {
    return nullptr;
  }

  if (nsCSSPseudoElements::PseudoElementSupportsStyleAttribute(aPseudoType) ||
      nsCSSPseudoElements::PseudoElementSupportsUserActionState(aPseudoType)) {
    return aFrame->GetContent()->AsElement();
  }

  return nullptr;
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
                                 nsStyleContext* aOldStyleContext,
                                 bool* aHaveMoreContinuations = nullptr)
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
    if (aHaveMoreContinuations) {
      *aHaveMoreContinuations = true;
    }
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
  nsIFrame* providerFrame;
  nsStyleContext* newParentContext = aFrame->GetParentStyleContext(&providerFrame);
  bool isChild = providerFrame && providerFrame->GetParent() == aFrame;
  nsIFrame* providerChild = nullptr;
  if (isChild) {
    ReparentStyleContext(providerFrame);
    // Get the style context again after ReparentStyleContext() which might have
    // changed it.
    newParentContext = providerFrame->StyleContext();
    providerChild = providerFrame;
  }
  NS_ASSERTION(newParentContext, "Reparenting something that has no usable"
               " parent? Shouldn't happen!");
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
        uint32_t equalStructs;
        DebugOnly<nsChangeHint> styleChange =
          oldContext->CalcStyleDifference(newContext, nsChangeHint(0),
                                          &equalStructs);
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
            uint32_t equalStructs;
            DebugOnly<nsChangeHint> styleChange =
              oldExtraContext->CalcStyleDifference(newExtraContext,
                                                   nsChangeHint(0),
                                                   &equalStructs);
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
                                   aVisibleKidsOfHiddenElement,
                                 nsTArray<ContextToClear>& aContextsToClear,
                                 nsTArray<nsRefPtr<nsStyleContext>>&
                                   aSwappedStructOwners)
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
  , mContextsToClear(aContextsToClear)
  , mSwappedStructOwners(aSwappedStructOwners)
#ifdef ACCESSIBILITY
  , mDesiredA11yNotifications(eSendAllNotifications)
  , mKidsDesiredA11yNotifications(mDesiredA11yNotifications)
  , mOurA11yNotification(eDontNotify)
  , mVisibleKidsOfHiddenElement(aVisibleKidsOfHiddenElement)
#endif
#ifdef RESTYLE_LOGGING
  , mLoggingDepth(aRestyleTracker.LoggingDepth() + 1)
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
  , mContextsToClear(aParentRestyler.mContextsToClear)
  , mSwappedStructOwners(aParentRestyler.mSwappedStructOwners)
#ifdef ACCESSIBILITY
  , mDesiredA11yNotifications(aParentRestyler.mKidsDesiredA11yNotifications)
  , mKidsDesiredA11yNotifications(mDesiredA11yNotifications)
  , mOurA11yNotification(eDontNotify)
  , mVisibleKidsOfHiddenElement(aParentRestyler.mVisibleKidsOfHiddenElement)
#endif
#ifdef RESTYLE_LOGGING
  , mLoggingDepth(aParentRestyler.mLoggingDepth + 1)
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
  , mContextsToClear(aParentRestyler.mContextsToClear)
  , mSwappedStructOwners(aParentRestyler.mSwappedStructOwners)
#ifdef ACCESSIBILITY
  , mDesiredA11yNotifications(aParentRestyler.mDesiredA11yNotifications)
  , mKidsDesiredA11yNotifications(mDesiredA11yNotifications)
  , mOurA11yNotification(eDontNotify)
  , mVisibleKidsOfHiddenElement(aParentRestyler.mVisibleKidsOfHiddenElement)
#endif
#ifdef RESTYLE_LOGGING
  , mLoggingDepth(aParentRestyler.mLoggingDepth + 1)
#endif
{
}

ElementRestyler::ElementRestyler(nsPresContext* aPresContext,
                                 nsIContent* aContent,
                                 nsStyleChangeList* aChangeList,
                                 nsChangeHint aHintsHandledByAncestors,
                                 RestyleTracker& aRestyleTracker,
                                 TreeMatchContext& aTreeMatchContext,
                                 nsTArray<nsIContent*>&
                                   aVisibleKidsOfHiddenElement,
                                 nsTArray<ContextToClear>& aContextsToClear,
                                 nsTArray<nsRefPtr<nsStyleContext>>&
                                   aSwappedStructOwners)
  : mPresContext(aPresContext)
  , mFrame(nullptr)
  , mParentContent(nullptr)
  , mContent(aContent)
  , mChangeList(aChangeList)
  , mHintsHandled(NS_SubtractHint(aHintsHandledByAncestors,
                  NS_HintsNotHandledForDescendantsIn(aHintsHandledByAncestors)))
  , mParentFrameHintsNotHandledForDescendants(nsChangeHint(0))
  , mHintsNotHandledForDescendants(nsChangeHint(0))
  , mRestyleTracker(aRestyleTracker)
  , mTreeMatchContext(aTreeMatchContext)
  , mResolvedChild(nullptr)
  , mContextsToClear(aContextsToClear)
  , mSwappedStructOwners(aSwappedStructOwners)
#ifdef ACCESSIBILITY
  , mDesiredA11yNotifications(eSendAllNotifications)
  , mKidsDesiredA11yNotifications(mDesiredA11yNotifications)
  , mOurA11yNotification(eDontNotify)
  , mVisibleKidsOfHiddenElement(aVisibleKidsOfHiddenElement)
#endif
{
}

void
ElementRestyler::AddLayerChangesForAnimation()
{
  // Bug 847286 - We should have separate animation generation counters
  // on layers for transitions and animations and use != comparison below
  // rather than a > comparison.
  uint64_t frameGeneration =
    RestyleManager::GetMaxAnimationGenerationForFrame(mFrame);

  nsChangeHint hint = nsChangeHint(0);
  const auto& layerInfo = css::CommonAnimationManager::sLayerAnimationInfo;
  for (size_t i = 0; i < ArrayLength(layerInfo); i++) {
    Layer* layer =
      FrameLayerBuilder::GetDedicatedLayer(mFrame, layerInfo[i].mLayerType);
    if (layer && frameGeneration > layer->GetAnimationGeneration()) {
      // If we have a transform layer but don't have any transform style, we
      // probably just removed the transform but haven't destroyed the layer
      // yet. In this case we will add the appropriate change hint
      // (nsChangeHint_AddOrRemoveTransform) when we compare style contexts
      // so we can skip adding any change hint here. (If we *were* to add
      // nsChangeHint_UpdateTransformLayer, ApplyRenderingChangeToTree would
      // complain that we're updating a transform layer without a transform).
      if (layerInfo[i].mLayerType == nsDisplayItem::TYPE_TRANSFORM &&
          !mFrame->StyleDisplay()->HasTransformStyle()) {
        continue;
      }
      NS_UpdateHint(hint, layerInfo[i].mChangeHint);
    }
  }
  if (hint) {
    mChangeList->AppendChange(mFrame, mContent, hint);
  }
}

void
ElementRestyler::CaptureChange(nsStyleContext* aOldContext,
                               nsStyleContext* aNewContext,
                               nsChangeHint aChangeToAssume,
                               uint32_t* aEqualStructs)
{
  static_assert(nsStyleStructID_Length <= 32,
                "aEqualStructs is not big enough");

  // Check some invariants about replacing one style context with another.
  NS_ASSERTION(aOldContext->GetPseudo() == aNewContext->GetPseudo(),
               "old and new style contexts should have the same pseudo");
  NS_ASSERTION(aOldContext->GetPseudoType() == aNewContext->GetPseudoType(),
               "old and new style contexts should have the same pseudo");

  nsChangeHint ourChange =
    aOldContext->CalcStyleDifference(aNewContext,
                                     mParentFrameHintsNotHandledForDescendants,
                                     aEqualStructs);
  NS_ASSERTION(!(ourChange & nsChangeHint_AllReflowHints) ||
               (ourChange & nsChangeHint_NeedReflow),
               "Reflow hint bits set without actually asking for a reflow");

  LOG_RESTYLE("CaptureChange, ourChange = %s, aChangeToAssume = %s",
              RestyleManager::ChangeHintToString(ourChange).get(),
              RestyleManager::ChangeHintToString(aChangeToAssume).get());
  LOG_RESTYLE_INDENT();

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
      LOG_RESTYLE("appending change %s",
                  RestyleManager::ChangeHintToString(ourChange).get());
      mChangeList->AppendChange(mFrame, mContent, ourChange);
    } else {
      LOG_RESTYLE("change has already been handled");
    }
  }
  NS_UpdateHint(mHintsNotHandledForDescendants,
                NS_HintsNotHandledForDescendantsIn(ourChange));
  LOG_RESTYLE("mHintsNotHandledForDescendants = %s",
              RestyleManager::ChangeHintToString(mHintsNotHandledForDescendants).get());
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

  AutoDisplayContentsAncestorPusher adcp(mTreeMatchContext, mFrame->PresContext(),
      mFrame->GetContent() ? mFrame->GetContent()->GetParent() : nullptr);

  // List of descendant elements of mContent we know we will eventually need to
  // restyle.  Before we return from this function, we call
  // RestyleTracker::AddRestyleRootsIfAwaitingRestyle to ensure they get
  // restyled in RestyleTracker::DoProcessRestyles.
  nsTArray<nsRefPtr<Element>> descendants;

  nsRestyleHint hintToRestore = nsRestyleHint(0);
  if (mContent && mContent->IsElement() &&
      // If we're resolving from the root of the frame tree (which
      // we do when mDoRebuildAllStyleData), we need to avoid getting the
      // root's restyle data until we get to its primary frame, since
      // it's the primary frame that has the styles for the root element
      // (rather than the ancestors of the primary frame whose mContent
      // is the root node but which have different styles).  If we use
      // up the hint for one of the ancestors that we hit first, then
      // we'll fail to do the restyling we need to do.
      // Likewise, if we're restyling something with two nested frames,
      // and we post a restyle from the transition manager while
      // computing style for the outer frame (to be computed after the
      // descendants have been resolved), we don't want to consume it
      // for the inner frame.
      mContent->GetPrimaryFrame() == mFrame) {
    mContent->OwnerDoc()->FlushPendingLinkUpdates();
    nsAutoPtr<RestyleTracker::RestyleData> restyleData;
    if (mRestyleTracker.GetRestyleData(mContent->AsElement(), restyleData)) {
      if (NS_UpdateHint(mHintsHandled, restyleData->mChangeHint)) {
        mChangeList->AppendChange(mFrame, mContent, restyleData->mChangeHint);
      }
      hintToRestore = restyleData->mRestyleHint;
      aRestyleHint = nsRestyleHint(aRestyleHint | restyleData->mRestyleHint);
      descendants.SwapElements(restyleData->mDescendants);
    }
  }

  // If we are restyling this frame with eRestyle_Self or weaker hints,
  // we restyle children with nsRestyleHint(0).  But we pass the
  // eRestyle_ForceDescendants flag down too.
  nsRestyleHint childRestyleHint =
    nsRestyleHint(aRestyleHint & (eRestyle_Subtree |
                                  eRestyle_ForceDescendants));

  nsRefPtr<nsStyleContext> oldContext = mFrame->StyleContext();

  // TEMPORARY (until bug 918064):  Call RestyleSelf for each
  // continuation or block-in-inline sibling.

  // We must make a single decision on how to process this frame and
  // its descendants, yet RestyleSelf might return different RestyleResult
  // values for the different same-style continuations.  |result| is our
  // overall decision.
  RestyleResult result = RestyleResult(0);
  uint32_t swappedStructs = 0;

  nsRestyleHint thisRestyleHint = aRestyleHint;

  bool haveMoreContinuations = false;
  for (nsIFrame* f = mFrame; f; ) {
    RestyleResult thisResult = RestyleSelf(f, thisRestyleHint, &swappedStructs);

    if (thisResult != eRestyleResult_Stop) {
      // Calls to RestyleSelf for later same-style continuations must not
      // return eRestyleResult_Stop, so pass eRestyle_Force in to them.
      thisRestyleHint = nsRestyleHint(thisRestyleHint | eRestyle_Force);

      if (result == eRestyleResult_Stop) {
        // We received eRestyleResult_Stop for earlier same-style
        // continuations, and eRestyleResult_Continue(AndForceDescendants) for
        // this one; go back and force-restyle the earlier continuations.
        result = thisResult;
        f = mFrame;
        continue;
      }
    }

    if (thisResult > result) {
      // We take the highest RestyleResult value when working out what to do
      // with this frame and its descendants.  Higher RestyleResult values
      // represent a superset of the work done by lower values.
      result = thisResult;
    }

    f = GetNextContinuationWithSameStyle(f, oldContext, &haveMoreContinuations);
  }

  // Some changes to animations don't affect the computed style and yet still
  // require the layer to be updated. For example, pausing an animation via
  // the Web Animations API won't affect an element's style but still
  // requires us to pull the animation off the layer.
  //
  // Although we only expect this code path to be called when computed style
  // is not changing, we can sometimes reach this at the end of a transition
  // when the animated style is being removed. Since
  // AddLayerChangesForAnimation checks if mFrame has a transform style or not,
  // we need to call it *after* calling RestyleSelf to ensure the animated
  // transform has been removed first.
  AddLayerChangesForAnimation();

  if (haveMoreContinuations && hintToRestore) {
    // If we have more continuations with different style (e.g., because
    // we're inside a ::first-letter or ::first-line), put the restyle
    // hint back.
    mRestyleTracker.AddPendingRestyleToTable(mContent->AsElement(),
                                             hintToRestore, nsChangeHint(0));
  }

  if (result == eRestyleResult_Stop) {
    MOZ_ASSERT(mFrame->StyleContext() == oldContext,
               "frame should have been left with its old style context");

    nsIFrame* unused;
    nsStyleContext* newParent = mFrame->GetParentStyleContext(&unused);
    if (oldContext->GetParent() != newParent) {
      // If we received eRestyleResult_Stop, then the old style context was
      // left on mFrame.  Since we ended up restyling our parent, change
      // this old style context to point to its new parent.
      LOG_RESTYLE("moving style context %p from old parent %p to new parent %p",
                  oldContext.get(), oldContext->GetParent(), newParent);
      // We keep strong references to the new parent around until the end
      // of the restyle, in case:
      //   (a) we swapped structs between the old and new parent,
      //   (b) some descendants of the old parent are not getting restyled
      //       (which is the reason for the existence of
      //       ClearCachedInheritedStyleDataOnDescendants),
      //   (c) something under ProcessPendingRestyles (which notably is called
      //       *before* ClearCachedInheritedStyleDataOnDescendants is called
      //       on the old context) causes the new parent to be destroyed, thus
      //       destroying its owned structs, and
      //   (d) something under ProcessPendingRestyles then wants to use of those
      //       now destroyed structs (through the old parent's descendants).
      mSwappedStructOwners.AppendElement(newParent);
      oldContext->MoveTo(newParent);
    }

    // Send the accessibility notifications that RestyleChildren otherwise
    // would have sent.
    if (!(mHintsHandled & nsChangeHint_ReconstructFrame)) {
      InitializeAccessibilityNotifications(mFrame->StyleContext());
      SendAccessibilityNotifications();
    }

    mRestyleTracker.AddRestyleRootsIfAwaitingRestyle(descendants);
    return;
  }

  if (!swappedStructs) {
    // If we swapped any structs from the old context, then we need to keep
    // it alive until after the RestyleChildren call so that we can fix up
    // its descendants' cached structs.
    oldContext = nullptr;
  }

  if (result == eRestyleResult_ContinueAndForceDescendants) {
    childRestyleHint =
      nsRestyleHint(childRestyleHint | eRestyle_ForceDescendants);
  }

  // No need to do this if we're planning to reframe already.
  // It's also important to check mHintsHandled since we use
  // mFrame->StyleContext(), which is out of date if mHintsHandled
  // has a ReconstructFrame hint.  Using an out of date style
  // context could trigger assertions about mismatched rule trees.
  if (!(mHintsHandled & nsChangeHint_ReconstructFrame)) {
    RestyleChildren(childRestyleHint);
  }

  if (oldContext && !oldContext->HasSingleReference()) {
    // If we swapped some structs out of oldContext in the RestyleSelf call
    // and after the RestyleChildren call we still have other strong references
    // to it, we need to make ensure its descendants don't cache any of the
    // structs that were swapped out.
    //
    // Much of the time we will not get in here; we do for example when the
    // style context is shared with a later IB split sibling (which we won't
    // restyle until a bit later) or if other code is holding a strong reference
    // to the style context (as is done by nsTransformedTextRun objects, which
    // can be referenced by a text frame's mTextRun longer than the frame's
    // mStyleContext).
    ContextToClear* toClear = mContextsToClear.AppendElement();
    toClear->mStyleContext = Move(oldContext);
    toClear->mStructs = swappedStructs;
  }

  mRestyleTracker.AddRestyleRootsIfAwaitingRestyle(descendants);
}

/**
 * Depending on the details of the frame we are restyling or its old style
 * context, we may or may not be able to stop restyling after this frame if
 * we find we had no style changes.
 *
 * This function returns eRestyleResult_Stop if it does not find any
 * conditions that would preclude stopping restyling, and
 * eRestyleResult_Continue if it does.
 */
ElementRestyler::RestyleResult
ElementRestyler::ComputeRestyleResultFromFrame(nsIFrame* aSelf)
{
  // We can't handle situations where the primary style context of a frame
  // has not had any style data changes, but its additional style contexts
  // have, so we don't considering stopping if this frame has any additional
  // style contexts.
  if (aSelf->GetAdditionalStyleContext(0)) {
    LOG_RESTYLE_CONTINUE("there are additional style contexts");
    return eRestyleResult_Continue;
  }

  // Style changes might have moved children between the two nsLetterFrames
  // (the one matching ::first-letter and the one containing the rest of the
  // content).  Continue restyling to the children of the nsLetterFrame so
  // that they get the correct style context parent.  Similarly for
  // nsLineFrames.
  nsIAtom* type = aSelf->GetType();

  if (type == nsGkAtoms::letterFrame) {
    LOG_RESTYLE_CONTINUE("frame is a letter frame");
    return eRestyleResult_Continue;
  }

  if (type == nsGkAtoms::lineFrame) {
    LOG_RESTYLE_CONTINUE("frame is a line frame");
    return eRestyleResult_Continue;
  }

  // Some style computations depend not on the parent's style, but a grandparent
  // or one the grandparent's ancestors.  An example is an explicit 'inherit'
  // value for align-self, where if the parent frame's value for the property is
  // 'auto' we end up inheriting the computed value from the grandparent.  We
  // can't stop the restyling process on this frame (the one with 'auto', in
  // this example), as the grandparent's computed value might have changed
  // and we need to recompute the child's 'inherit' to that new value.
  nsStyleContext* oldContext = aSelf->StyleContext();
  if (oldContext->HasChildThatUsesGrandancestorStyle()) {
    LOG_RESTYLE_CONTINUE("the old context uses grandancestor style");
    return eRestyleResult_Continue;
  }

  // We ignore all situations that involve :visited style.
  if (oldContext->GetStyleIfVisited()) {
    LOG_RESTYLE_CONTINUE("the old style context has StyleIfVisited");
    return eRestyleResult_Continue;
  }

  nsStyleContext* parentContext = oldContext->GetParent();
  if (parentContext && parentContext->GetStyleIfVisited()) {
    LOG_RESTYLE_CONTINUE("the old style context's parent has StyleIfVisited");
    return eRestyleResult_Continue;
  }

  // We also ignore frames for pseudos, as their style contexts have
  // inheritance structures that do not match the frame inheritance
  // structure.  To avoid enumerating and checking all of the cases
  // where we have this kind of inheritance, we keep restyling past
  // pseudos.
  nsIAtom* pseudoTag = oldContext->GetPseudo();
  if (pseudoTag && pseudoTag != nsCSSAnonBoxes::mozNonElement) {
    LOG_RESTYLE_CONTINUE("the old style context is for a pseudo");
    return eRestyleResult_Continue;
  }

  nsIFrame* parent = mFrame->GetParent();

  if (parent) {
    // Also if the parent has a pseudo, as this frame's style context will
    // be inheriting from a grandparent frame's style context (or a further
    // ancestor).
    nsIAtom* parentPseudoTag = parent->StyleContext()->GetPseudo();
    if (parentPseudoTag && parentPseudoTag != nsCSSAnonBoxes::mozNonElement) {
      LOG_RESTYLE_CONTINUE("the old style context's parent is for a pseudo");
      return eRestyleResult_Continue;
    }
  }

  return eRestyleResult_Stop;
}

ElementRestyler::RestyleResult
ElementRestyler::ComputeRestyleResultFromNewContext(nsIFrame* aSelf,
                                                    nsStyleContext* aNewContext)
{
  // Keep restyling if the new style context has any style-if-visted style, so
  // that we can avoid the style context tree surgery having to deal to deal
  // with visited styles.
  if (aNewContext->GetStyleIfVisited()) {
    LOG_RESTYLE_CONTINUE("the new style context has StyleIfVisited");
    return eRestyleResult_Continue;
  }

  // If link-related information has changed, or the pseudo for the frame has
  // changed, or the new style context points to a different rule node, we can't
  // leave the old style context on the frame.
  nsStyleContext* oldContext = aSelf->StyleContext();
  if (oldContext->IsLinkContext() != aNewContext->IsLinkContext() ||
      oldContext->RelevantLinkVisited() != aNewContext->RelevantLinkVisited() ||
      oldContext->GetPseudo() != aNewContext->GetPseudo() ||
      oldContext->GetPseudoType() != aNewContext->GetPseudoType() ||
      oldContext->RuleNode() != aNewContext->RuleNode()) {
    LOG_RESTYLE_CONTINUE("the old and new style contexts have different link/"
                         "visited/pseudo/rulenodes");
    return eRestyleResult_Continue;
  }

  // If the old and new style contexts differ in their
  // NS_STYLE_HAS_TEXT_DECORATION_LINES or NS_STYLE_HAS_PSEUDO_ELEMENT_DATA
  // bits, then we must keep restyling so that those new bit values are
  // propagated.
  if (oldContext->HasTextDecorationLines() !=
        aNewContext->HasTextDecorationLines()) {
    LOG_RESTYLE_CONTINUE("NS_STYLE_HAS_TEXT_DECORATION_LINES differs between old"
                         " and new style contexts");
    return eRestyleResult_Continue;
  }

  if (oldContext->HasPseudoElementData() !=
        aNewContext->HasPseudoElementData()) {
    LOG_RESTYLE_CONTINUE("NS_STYLE_HAS_PSEUDO_ELEMENT_DATA differs between old"
                         " and new style contexts");
    return eRestyleResult_Continue;
  }

  if (oldContext->ShouldSuppressLineBreak() !=
        aNewContext->ShouldSuppressLineBreak()) {
    LOG_RESTYLE_CONTINUE("NS_STYLE_SUPPRESS_LINEBREAK differs"
                         "between old and new style contexts");
    return eRestyleResult_Continue;
  }

  if (oldContext->IsInDisplayNoneSubtree() !=
        aNewContext->IsInDisplayNoneSubtree()) {
    LOG_RESTYLE_CONTINUE("NS_STYLE_IN_DISPLAY_NONE_SUBTREE differs between old"
                         " and new style contexts");
    return eRestyleResult_Continue;
  }

  return eRestyleResult_Stop;
}

ElementRestyler::RestyleResult
ElementRestyler::RestyleSelf(nsIFrame* aSelf,
                             nsRestyleHint aRestyleHint,
                             uint32_t* aSwappedStructs)
{
  MOZ_ASSERT(!(aRestyleHint & eRestyle_LaterSiblings),
             "eRestyle_LaterSiblings must not be part of aRestyleHint");

  // XXXldb get new context from prev-in-flow if possible, to avoid
  // duplication.  (Or should we just let |GetContext| handle that?)
  // Getting the hint would be nice too, but that's harder.

  // XXXbryner we may be able to avoid some of the refcounting goop here.
  // We do need a reference to oldContext for the lifetime of this function, and it's possible
  // that the frame has the last reference to it, so AddRef it here.

  LOG_RESTYLE("RestyleSelf %s, aRestyleHint = %s",
              FrameTagToString(aSelf).get(),
              RestyleManager::RestyleHintToString(aRestyleHint).get());
  LOG_RESTYLE_INDENT();

  RestyleResult result;

  if (aRestyleHint) {
    result = eRestyleResult_Continue;
  } else {
    result = ComputeRestyleResultFromFrame(aSelf);
  }

  nsChangeHint assumeDifferenceHint = NS_STYLE_HINT_NONE;
  nsRefPtr<nsStyleContext> oldContext = aSelf->StyleContext();
  nsStyleSet* styleSet = mPresContext->StyleSet();

#ifdef ACCESSIBILITY
  mWasFrameVisible = nsIPresShell::IsAccessibilityActive() ?
    oldContext->StyleVisibility()->IsVisible() : false;
#endif

  nsIAtom* const pseudoTag = oldContext->GetPseudo();
  const nsCSSPseudoElements::Type pseudoType = oldContext->GetPseudoType();

  // Get the frame providing the parent style context.  If it is a
  // child, then resolve the provider first.
  nsIFrame* providerFrame;
  nsStyleContext* parentContext = aSelf->GetParentStyleContext(&providerFrame);
  bool isChild = providerFrame && providerFrame->GetParent() == aSelf;
  if (isChild) {
    MOZ_ASSERT(providerFrame->GetContent() == aSelf->GetContent(),
               "Postcondition for GetParentStyleContext() violated. "
               "That means we need to add the current element to the "
               "ancestor filter.");

    // resolve the provider here (before aSelf below).
    LOG_RESTYLE("resolving child provider frame");

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
    LOG_RESTYLE_CONTINUE("we had a provider frame");
    // Continue restyling past the odd style context inheritance.
    result = eRestyleResult_Continue;
  }

  if (providerFrame != aSelf->GetParent()) {
    // We don't actually know what the parent style context's
    // non-inherited hints were, so assume the worst.
    mParentFrameHintsNotHandledForDescendants =
      nsChangeHint_Hints_NotHandledForDescendants;
  }

  LOG_RESTYLE("parentContext = %p", parentContext);

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
    LOG_RESTYLE("using previous continuation's context");
    newContext = prevContinuationContext;
  }
  else if (pseudoTag == nsCSSAnonBoxes::mozNonElement) {
    NS_ASSERTION(aSelf->GetContent(),
                 "non pseudo-element frame without content node");
    newContext = styleSet->ResolveStyleForNonElement(parentContext);
  }
  else if (!(aRestyleHint & (eRestyle_Self | eRestyle_Subtree))) {
    Element* element = ElementForStyleContext(mParentContent, aSelf, pseudoType);
    if (!(aRestyleHint & ~(eRestyle_Force | eRestyle_ForceDescendants)) &&
        !styleSet->IsInRuleTreeReconstruct()) {
      LOG_RESTYLE("reparenting style context");
      newContext =
        styleSet->ReparentStyleContext(oldContext, parentContext, element);
    } else {
      // Use ResolveStyleWithReplacement either for actual replacements
      // or, with no replacements, as a substitute for
      // ReparentStyleContext that rebuilds the path in the rule tree
      // rather than reusing the rule node, as we need to do during a
      // rule tree reconstruct.
      Element* pseudoElement = PseudoElementForStyleContext(aSelf, pseudoType);
      MOZ_ASSERT(!element || element != pseudoElement,
                 "pseudo-element for selector matching should be "
                 "the anonymous content node that we create, "
                 "not the real element");
      LOG_RESTYLE("resolving style with replacement");
      newContext =
        styleSet->ResolveStyleWithReplacement(element, pseudoElement,
                                              parentContext, oldContext,
                                              aRestyleHint);
    }
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
#ifdef DEBUG
          // oldContext's parent might have had its style structs swapped out
          // with parentContext, so to avoid any assertions that might
          // otherwise trigger in oldContext's parent's destructor, we set a
          // flag on oldContext to skip it and its descendants in
          // nsStyleContext::AssertStructsNotUsedElsewhere.
          if (oldContext->GetParent() != parentContext) {
            oldContext->AddStyleBit(NS_STYLE_IS_GOING_AWAY);
          }
#endif
        }
      } else {
        // Don't expect XUL tree stuff here, since it needs a comparator and
        // all.
        NS_ASSERTION(pseudoType <
                       nsCSSPseudoElements::ePseudo_PseudoElementCount,
                     "Unexpected pseudo type");
        Element* pseudoElement =
          PseudoElementForStyleContext(aSelf, pseudoType);
        MOZ_ASSERT(element != pseudoElement,
                   "pseudo-element for selector matching should be "
                   "the anonymous content node that we create, "
                   "not the real element");
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
      LOG_RESTYLE("restyling root and keeping old context");
      LOG_RESTYLE_IF(this, result != eRestyleResult_Continue,
                     "continuing restyle since this is the root");
      newContext = oldContext;
      // Never consider stopping restyling at the root.
      result = eRestyleResult_Continue;
    }
  }

  LOG_RESTYLE("oldContext = %p, newContext = %p%s",
              oldContext.get(), newContext.get(),
              oldContext == newContext ? (const char*) " (same)" :
                                         (const char*) "");

  if (newContext != oldContext) {
    if (result == eRestyleResult_Stop) {
      if (oldContext->IsShared()) {
        // If the old style context was shared, then we can't return
        // eRestyleResult_Stop and patch its parent to point to the
        // new parent style context, as that change might not be valid
        // for the other frames sharing the style context.
        LOG_RESTYLE_CONTINUE("the old style context is shared");
        result = eRestyleResult_Continue;
      } else {
        // Look at some details of the new style context to see if it would
        // be safe to stop restyling, if we discover it has the same style
        // data as the old style context.
        result = ComputeRestyleResultFromNewContext(aSelf, newContext);
      }
    }

    uint32_t equalStructs = 0;

    if (copyFromContinuation) {
      // In theory we should know whether there was any style data difference,
      // since we would have calculated that in the previous call to
      // RestyleSelf, so until we perform only one restyling per chain-of-
      // same-style continuations (bug 918064), we need to check again here to
      // determine whether it is safe to stop restyling.
      if (result == eRestyleResult_Stop) {
        oldContext->CalcStyleDifference(newContext, nsChangeHint(0),
                                        &equalStructs);
        if (equalStructs != NS_STYLE_INHERIT_MASK) {
          // At least one struct had different data in it, so we must
          // continue restyling children.
          LOG_RESTYLE_CONTINUE("there is different style data: %s",
                      RestyleManager::StructNamesToString(
                        ~equalStructs & NS_STYLE_INHERIT_MASK).get());
          result = eRestyleResult_Continue;
        }
      }
    } else {
      bool changedStyle =
        RestyleManager::TryStartingTransition(mPresContext, aSelf->GetContent(),
                                              oldContext, &newContext);
      if (changedStyle) {
        LOG_RESTYLE_CONTINUE("TryStartingTransition changed the new style context");
        result = eRestyleResult_Continue;
      }
      CaptureChange(oldContext, newContext, assumeDifferenceHint,
                    &equalStructs);
      if (equalStructs != NS_STYLE_INHERIT_MASK) {
        // At least one struct had different data in it, so we must
        // continue restyling children.
        LOG_RESTYLE_CONTINUE("there is different style data: %s",
                    RestyleManager::StructNamesToString(
                      ~equalStructs & NS_STYLE_INHERIT_MASK).get());
        result = eRestyleResult_Continue;
      }
    }

    if (result == eRestyleResult_Stop) {
      // Since we currently have eRestyleResult_Stop, we know at this
      // point that all of our style structs are equal in terms of styles.
      // However, some of them might be different pointers.  Since our
      // descendants might share those pointers, we have to continue to
      // restyling our descendants.
      //
      // However, because of the swapping of equal structs we've done on
      // ancestors (later in this function), we've ensured that for structs
      // that cannot be stored in the rule tree, we keep the old equal structs
      // around rather than replacing them with new ones.  This means that we
      // only time we hit this deoptimization is either
      //
      // (a) when at least one of the (old or new) equal structs could be stored
      //     in the rule tree, and those structs are then inherited (by pointer
      //     sharing) to descendant style contexts; or
      //
      // (b) when we were unable to swap the structs on the parent because
      //     either or both of the old parent and new parent are shared.
      for (nsStyleStructID sid = nsStyleStructID(0);
           sid < nsStyleStructID_Length;
           sid = nsStyleStructID(sid + 1)) {
        if (oldContext->HasCachedInheritedStyleData(sid) &&
            !oldContext->HasSameCachedStyleData(newContext, sid)) {
          LOG_RESTYLE_CONTINUE("there are different struct pointers");
          result = eRestyleResult_Continue;
          break;
        }
      }
    }

    if (!(mHintsHandled & nsChangeHint_ReconstructFrame)) {
      // If the frame gets regenerated, let it keep its old context,
      // which is important to maintain various invariants about
      // frame types matching their style contexts.
      // Note that this check even makes sense if we didn't call
      // CaptureChange because of copyFromContinuation being true,
      // since we'll have copied the existing context from the
      // previous continuation, so newContext == oldContext.

      if (result != eRestyleResult_Stop) {
        if (copyFromContinuation) {
          LOG_RESTYLE("not swapping style structs, since we copied from a "
                      "continuation");
        } else if (oldContext->IsShared() && newContext->IsShared()) {
          LOG_RESTYLE("not swapping style structs, since both old and contexts "
                      "are shared");
        } else if (oldContext->IsShared()) {
          LOG_RESTYLE("not swapping style structs, since the old context is "
                      "shared");
        } else if (newContext->IsShared()) {
          LOG_RESTYLE("not swapping style structs, since the new context is "
                      "shared");
        } else {
          LOG_RESTYLE("swapping style structs between %p and %p",
                      oldContext.get(), newContext.get());
          oldContext->SwapStyleData(newContext, equalStructs);
          *aSwappedStructs |= equalStructs;
#ifdef RESTYLE_LOGGING
          uint32_t structs = RestyleManager::StructsToLog() & equalStructs;
          if (structs) {
            LOG_RESTYLE_INDENT();
            LOG_RESTYLE("old style context now has: %s",
                        oldContext->GetCachedStyleDataAsString(structs).get());
            LOG_RESTYLE("new style context now has: %s",
                        newContext->GetCachedStyleDataAsString(structs).get());
          }
#endif
        }
        LOG_RESTYLE("setting new style context");
        aSelf->SetStyleContext(newContext);
      }
    } else {
      LOG_RESTYLE("not setting new style context, since we'll reframe");
      // We need to keep the new parent alive, in case it had structs
      // swapped into it that our frame's style context still has cached.
      // This is a similar scenario to the one described in the
      // ElementRestyler::Restyle comment where we append to
      // mSwappedStructOwners.
      //
      // We really only need to do this if we did swap structs on the
      // parent, but we don't have that information here.
      mSwappedStructOwners.AppendElement(newContext->GetParent());
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
    LOG_RESTYLE("extra context %d", contextIndex);
    LOG_RESTYLE_INDENT();
    nsRefPtr<nsStyleContext> newExtraContext;
    nsIAtom* const extraPseudoTag = oldExtraContext->GetPseudo();
    const nsCSSPseudoElements::Type extraPseudoType =
      oldExtraContext->GetPseudoType();
    NS_ASSERTION(extraPseudoTag &&
                 extraPseudoTag != nsCSSAnonBoxes::mozNonElement,
                 "extra style context is not pseudo element");
    if (!(aRestyleHint & (eRestyle_Self | eRestyle_Subtree))) {
      Element* element = extraPseudoType != nsCSSPseudoElements::ePseudo_AnonBox
                           ? mContent->AsElement() : nullptr;
      if (styleSet->IsInRuleTreeReconstruct()) {
        // Use ResolveStyleWithReplacement as a substitute for
        // ReparentStyleContext that rebuilds the path in the rule tree
        // rather than reusing the rule node, as we need to do during a
        // rule tree reconstruct.
        Element* pseudoElement =
          PseudoElementForStyleContext(aSelf, extraPseudoType);
        MOZ_ASSERT(!element || element != pseudoElement,
                   "pseudo-element for selector matching should be "
                   "the anonymous content node that we create, "
                   "not the real element");
        newExtraContext =
          styleSet->ResolveStyleWithReplacement(element, pseudoElement,
                                                newContext, oldExtraContext,
                                                nsRestyleHint(0));
      } else {
        newExtraContext =
          styleSet->ReparentStyleContext(oldExtraContext, newContext, element);
      }
    } else if (extraPseudoType == nsCSSPseudoElements::ePseudo_AnonBox) {
      newExtraContext = styleSet->ResolveAnonymousBoxStyle(extraPseudoTag,
                                                           newContext);
    } else {
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

    LOG_RESTYLE("newExtraContext = %p", newExtraContext.get());

    if (oldExtraContext != newExtraContext) {
      uint32_t equalStructs;
      CaptureChange(oldExtraContext, newExtraContext, assumeDifferenceHint,
                    &equalStructs);
      if (!(mHintsHandled & nsChangeHint_ReconstructFrame)) {
        LOG_RESTYLE("setting new extra style context");
        aSelf->SetAdditionalStyleContext(contextIndex, newExtraContext);
      } else {
        LOG_RESTYLE("not setting new extra style context, since we'll reframe");
      }
    }
  }

  if (aRestyleHint & eRestyle_ForceDescendants) {
    result = eRestyleResult_ContinueAndForceDescendants;
  }

  LOG_RESTYLE("returning %s", RestyleResultToString(result).get());

  return result;
}

void
ElementRestyler::RestyleChildren(nsRestyleHint aChildRestyleHint)
{
  MOZ_ASSERT(!(mHintsHandled & nsChangeHint_ReconstructFrame),
             "No need to do this if we're planning to reframe already.");

  // We'd like style resolution to be exact in the sense that an
  // animation-only style flush flushes only the styles it requests
  // flushing and doesn't update any other styles.  This means avoiding
  // constructing new frames during such a flush.
  //
  // For a ::before or ::after, we'll do an eRestyle_Subtree due to
  // RestyleHintForOp in nsCSSRuleProcessor.cpp (via its
  // HasAttributeDependentStyle or HasStateDependentStyle), given that
  // we store pseudo-elements in selectors like they were children.
  //
  // Also, it's faster to skip the work we do on undisplayed children
  // and pseudo-elements when we can skip it.
  bool mightReframePseudos = aChildRestyleHint & eRestyle_Subtree;

  RestyleUndisplayedDescendants(aChildRestyleHint);

  // Check whether we might need to create a new ::before frame.
  // There's no need to do this if we're planning to reframe already
  // or if we're not forcing restyles on kids.
  // It's also important to check mHintsHandled since we use
  // mFrame->StyleContext(), which is out of date if mHintsHandled has a
  // ReconstructFrame hint.  Using an out of date style context could
  // trigger assertions about mismatched rule trees.
  if (!(mHintsHandled & nsChangeHint_ReconstructFrame) &&
      mightReframePseudos) {
    MaybeReframeForBeforePseudo();
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
    InitializeAccessibilityNotifications(mFrame->StyleContext());

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
      mightReframePseudos) {
    MaybeReframeForAfterPseudo(lastContinuation);
  }
}

void
ElementRestyler::RestyleChildrenOfDisplayContentsElement(
  nsIFrame*       aParentFrame,
  nsStyleContext* aNewContext,
  nsChangeHint    aMinHint,
  RestyleTracker& aRestyleTracker,
  nsRestyleHint   aRestyleHint)
{
  MOZ_ASSERT(!(mHintsHandled & nsChangeHint_ReconstructFrame), "why call me?");

  const bool mightReframePseudos = aRestyleHint & eRestyle_Subtree;
  DoRestyleUndisplayedDescendants(nsRestyleHint(0), mContent, aNewContext);
  if (!(mHintsHandled & nsChangeHint_ReconstructFrame) && mightReframePseudos) {
    MaybeReframeForBeforePseudo(aParentFrame, nullptr, mContent, aNewContext);
  }
  if (!(mHintsHandled & nsChangeHint_ReconstructFrame) && mightReframePseudos) {
    MaybeReframeForAfterPseudo(aParentFrame, nullptr, mContent, aNewContext);
  }
  if (!(mHintsHandled & nsChangeHint_ReconstructFrame)) {
    InitializeAccessibilityNotifications(aNewContext);

    // Then process child frames for content that is a descendant of mContent.
    // XXX perhaps it's better to walk child frames (before reresolving
    // XXX undisplayed contexts above) and mark those that has a stylecontext
    // XXX leading up to mContent's old context? (instead of the
    // XXX ContentIsDescendantOf check below)
    nsIFrame::ChildListIterator lists(aParentFrame);
    for ( ; !lists.IsDone(); lists.Next()) {
      nsFrameList::Enumerator childFrames(lists.CurrentList());
      for (; !childFrames.AtEnd(); childFrames.Next()) {
        nsIFrame* f = childFrames.get();
        if (nsContentUtils::ContentIsDescendantOf(f->GetContent(), mContent) &&
            !f->GetPrevContinuation()) {
          if (!(f->GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
            ComputeStyleChangeFor(f, mChangeList, aMinHint, aRestyleTracker,
                                  aRestyleHint, mContextsToClear,
                                  mSwappedStructOwners);
          }
        }
      }
    }
  }
  if (!(mHintsHandled & nsChangeHint_ReconstructFrame)) {
    SendAccessibilityNotifications();
  }
}

void
ElementRestyler::ComputeStyleChangeFor(nsIFrame*          aFrame,
                                       nsStyleChangeList* aChangeList,
                                       nsChangeHint       aMinChange,
                                       RestyleTracker&    aRestyleTracker,
                                       nsRestyleHint      aRestyleHint,
                                       nsTArray<ContextToClear>&
                                         aContextsToClear,
                                       nsTArray<nsRefPtr<nsStyleContext>>&
                                         aSwappedStructOwners)
{
  nsIContent* content = aFrame->GetContent();
  nsAutoCString idStr;
  if (profiler_is_active() && content) {
    nsIAtom* id = content->GetID();
    if (id) {
      id->ToUTF8String(idStr);
    } else {
      idStr.AssignLiteral("?");
    }
  }

  PROFILER_LABEL_PRINTF("ElementRestyler", "ComputeStyleChangeFor",
                        js::ProfileEntry::Category::CSS,
                        content ? "Element: %s" : "%s",
                        content ? idStr.get() : "");
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

  nsPresContext* presContext = aFrame->PresContext();
  FramePropertyTable* propTable = presContext->PropertyTable();

  TreeMatchContext treeMatchContext(true,
                                    nsRuleWalker::eRelevantLinkUnvisited,
                                    presContext->Document());
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
      ElementRestyler restyler(presContext, cont, aChangeList,
                               aMinChange, aRestyleTracker,
                               treeMatchContext,
                               visibleKidsOfHiddenElement,
                               aContextsToClear, aSwappedStructOwners);

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

void
ElementRestyler::RestyleUndisplayedDescendants(nsRestyleHint aChildRestyleHint)
{
  // When the root element is display:none, we still construct *some*
  // frames that have the root element as their mContent, down to the
  // DocElementContainingBlock.
  bool checkUndisplayed;
  nsIContent* undisplayedParent;
  if (mFrame->StyleContext()->GetPseudo()) {
    checkUndisplayed = mFrame == mPresContext->FrameConstructor()->
                                   GetDocElementContainingBlock();
    undisplayedParent = nullptr;
  } else {
    checkUndisplayed = !!mFrame->GetContent();
    undisplayedParent = mFrame->GetContent();
  }
  if (checkUndisplayed) {
    DoRestyleUndisplayedDescendants(aChildRestyleHint, undisplayedParent,
                                    mFrame->StyleContext());
  }
}

void
ElementRestyler::DoRestyleUndisplayedDescendants(nsRestyleHint aChildRestyleHint,
                                                 nsIContent* aParent,
                                                 nsStyleContext* aParentContext)
{
  nsCSSFrameConstructor* fc = mPresContext->FrameConstructor();
  UndisplayedNode* nodes = fc->GetAllUndisplayedContentIn(aParent);
  RestyleUndisplayedNodes(aChildRestyleHint, nodes, aParent,
                          aParentContext, NS_STYLE_DISPLAY_NONE);
  nodes = fc->GetAllDisplayContentsIn(aParent);
  RestyleUndisplayedNodes(aChildRestyleHint, nodes, aParent,
                          aParentContext, NS_STYLE_DISPLAY_CONTENTS);
}

void
ElementRestyler::RestyleUndisplayedNodes(nsRestyleHint    aChildRestyleHint,
                                         UndisplayedNode* aUndisplayed,
                                         nsIContent*      aUndisplayedParent,
                                         nsStyleContext*  aParentContext,
                                         const uint8_t    aDisplay)
{
  nsIContent* undisplayedParent = aUndisplayedParent;
  UndisplayedNode* undisplayed = aUndisplayed;
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

    LOG_RESTYLE("RestyleUndisplayedChildren: undisplayed->mContent = %p",
                undisplayed->mContent.get());

    // Get the parent of the undisplayed content and check if it is a XBL
    // children element. Push the children element as an ancestor here because it does
    // not have a frame and would not otherwise be pushed as an ancestor.
    nsIContent* parent = undisplayed->mContent->GetParent();
    TreeMatchContext::AutoAncestorPusher insertionPointPusher(mTreeMatchContext);
    if (parent && nsContentUtils::IsContentInsertionPoint(parent)) {
      insertionPointPusher.PushAncestorAndStyleScope(parent);
    }

    nsRestyleHint thisChildHint = aChildRestyleHint;
    nsAutoPtr<RestyleTracker::RestyleData> undisplayedRestyleData;
    Element* element = undisplayed->mContent->AsElement();
    if (mRestyleTracker.GetRestyleData(element,
                                       undisplayedRestyleData)) {
      thisChildHint =
        nsRestyleHint(thisChildHint | undisplayedRestyleData->mRestyleHint);
    }
    nsRefPtr<nsStyleContext> undisplayedContext;
    nsStyleSet* styleSet = mPresContext->StyleSet();
    if (thisChildHint & (eRestyle_Self | eRestyle_Subtree)) {
      undisplayedContext =
        styleSet->ResolveStyleFor(element, aParentContext, mTreeMatchContext);
    } else if (thisChildHint ||
               styleSet->IsInRuleTreeReconstruct()) {
      // Use ResolveStyleWithReplacement either for actual
      // replacements, or as a substitute for ReparentStyleContext
      // that rebuilds the path in the rule tree rather than reusing
      // the rule node, as we need to do during a rule tree
      // reconstruct.
      undisplayedContext =
        styleSet->ResolveStyleWithReplacement(element, nullptr,
                                              aParentContext,
                                              undisplayed->mStyle,
                                              thisChildHint);
    } else {
      undisplayedContext =
        styleSet->ReparentStyleContext(undisplayed->mStyle,
                                       aParentContext,
                                       element);
    }
    const nsStyleDisplay* display = undisplayedContext->StyleDisplay();
    if (display->mDisplay != aDisplay) {
      NS_ASSERTION(element, "Must have undisplayed content");
      mChangeList->AppendChange(nullptr, element,
                                NS_STYLE_HINT_FRAMECHANGE);
      // The node should be removed from the undisplayed map when
      // we reframe it.
    } else {
      // update the undisplayed node with the new context
      undisplayed->mStyle = undisplayedContext;

      if (aDisplay == NS_STYLE_DISPLAY_CONTENTS) {
        DoRestyleUndisplayedDescendants(aChildRestyleHint, element,
                                        undisplayed->mStyle);
      }
    }
  }
}

void
ElementRestyler::MaybeReframeForBeforePseudo()
{
  MaybeReframeForBeforePseudo(mFrame, mFrame, mFrame->GetContent(),
                              mFrame->StyleContext());
}

void
ElementRestyler::MaybeReframeForBeforePseudo(nsIFrame* aGenConParentFrame,
                                             nsIFrame* aFrame,
                                             nsIContent* aContent,
                                             nsStyleContext* aStyleContext)
{
  // Make sure not to do this for pseudo-frames or frames that
  // can't have generated content.
  nsContainerFrame* cif;
  if (!aStyleContext->GetPseudo() &&
      ((aGenConParentFrame->GetStateBits() & NS_FRAME_MAY_HAVE_GENERATED_CONTENT) ||
       // Our content insertion frame might have gotten flagged.
       ((cif = aGenConParentFrame->GetContentInsertionFrame()) &&
        (cif->GetStateBits() & NS_FRAME_MAY_HAVE_GENERATED_CONTENT)))) {
    // Check for a ::before pseudo style and the absence of a ::before content,
    // but only if aFrame is null or is the first continuation/ib-split.
    if (!aFrame ||
        nsLayoutUtils::IsFirstContinuationOrIBSplitSibling(aFrame)) {
      // Checking for a ::before frame is cheaper than getting the
      // ::before style context.
      if (!nsLayoutUtils::GetBeforeFrameForContent(aGenConParentFrame, aContent) &&
          nsLayoutUtils::HasPseudoStyle(aContent, aStyleContext,
                                        nsCSSPseudoElements::ePseudo_before,
                                        mPresContext)) {
        // Have to create the new ::before frame.
        LOG_RESTYLE("MaybeReframeForBeforePseudo, appending "
                    "nsChangeHint_ReconstructFrame");
        NS_UpdateHint(mHintsHandled, nsChangeHint_ReconstructFrame);
        mChangeList->AppendChange(aFrame, aContent,
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
ElementRestyler::MaybeReframeForAfterPseudo(nsIFrame* aFrame)
{
  MOZ_ASSERT(aFrame);
  MaybeReframeForAfterPseudo(aFrame, aFrame, aFrame->GetContent(),
                             aFrame->StyleContext());
}

void
ElementRestyler::MaybeReframeForAfterPseudo(nsIFrame* aGenConParentFrame,
                                            nsIFrame* aFrame,
                                            nsIContent* aContent,
                                            nsStyleContext* aStyleContext)
{
  // Make sure not to do this for pseudo-frames or frames that
  // can't have generated content.
  nsContainerFrame* cif;
  if (!aStyleContext->GetPseudo() &&
      ((aGenConParentFrame->GetStateBits() & NS_FRAME_MAY_HAVE_GENERATED_CONTENT) ||
       // Our content insertion frame might have gotten flagged.
       ((cif = aGenConParentFrame->GetContentInsertionFrame()) &&
        (cif->GetStateBits() & NS_FRAME_MAY_HAVE_GENERATED_CONTENT)))) {
    // Check for an ::after pseudo style and the absence of an ::after content,
    // but only if aFrame is null or is the last continuation/ib-split.
    if (!aFrame ||
        !nsLayoutUtils::GetNextContinuationOrIBSplitSibling(aFrame)) {
      // Checking for an ::after frame is cheaper than getting the
      // ::after style context.
      if (!nsLayoutUtils::GetAfterFrameForContent(aGenConParentFrame, aContent) &&
          nsLayoutUtils::HasPseudoStyle(aContent, aStyleContext,
                                        nsCSSPseudoElements::ePseudo_after,
                                        mPresContext)) {
        // Have to create the new ::after frame.
        LOG_RESTYLE("MaybeReframeForAfterPseudo, appending "
                    "nsChangeHint_ReconstructFrame");
        NS_UpdateHint(mHintsHandled, nsChangeHint_ReconstructFrame);
        mChangeList->AppendChange(aFrame, mContent,
                                  nsChangeHint_ReconstructFrame);
      }
    }
  }
}

void
ElementRestyler::InitializeAccessibilityNotifications(nsStyleContext* aNewContext)
{
#ifdef ACCESSIBILITY
  // Notify a11y for primary frame only if it's a root frame of visibility
  // changes or its parent frame was hidden while it stays visible and
  // it is not inside a {ib} split or is the first frame of {ib} split.
  if (nsIPresShell::IsAccessibilityActive() &&
      (!mFrame ||
       (!mFrame->GetPrevContinuation() &&
        !mFrame->FrameIsNonFirstInIBSplit()))) {
    if (mDesiredA11yNotifications == eSendAllNotifications) {
      bool isFrameVisible = aNewContext->StyleVisibility()->IsVisible();
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
               aNewContext->StyleVisibility()->IsVisible()) {
      // Notify a11y that element stayed visible while its parent was hidden.
      nsIContent* c = mFrame ? mFrame->GetContent() : mContent;
      mVisibleKidsOfHiddenElement.AppendElement(c);
      mKidsDesiredA11yNotifications = eSkipNotifications;
    }
  }
#endif
}

void
ElementRestyler::RestyleContentChildren(nsIFrame* aParent,
                                        nsRestyleHint aChildRestyleHint)
{
  LOG_RESTYLE("RestyleContentChildren");

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

          // |nsFrame::GetParentStyleContext| checks being out
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
      nsIPresShell* presShell = mPresContext->GetPresShell();
      nsIContent* content = mFrame ? mFrame->GetContent() : mContent;

      accService->ContentRangeInserted(presShell, content->GetParent(),
                                       content,
                                       content->GetNextSibling());
    }
  } else if (mOurA11yNotification == eNotifyHidden) {
    nsAccessibilityService* accService = nsIPresShell::AccService();
    if (accService) {
      nsIPresShell* presShell = mPresContext->GetPresShell();
      nsIContent* content = mFrame ? mFrame->GetContent() : mContent;
      accService->ContentRemoved(presShell, content);

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

static void
ClearCachedInheritedStyleDataOnDescendants(
    nsTArray<ElementRestyler::ContextToClear>& aContextsToClear)
{
  for (size_t i = 0; i < aContextsToClear.Length(); i++) {
    auto& entry = aContextsToClear[i];
    if (!entry.mStyleContext->HasSingleReference()) {
      entry.mStyleContext->ClearCachedInheritedStyleDataOnDescendants(
          entry.mStructs);
    }
    entry.mStyleContext = nullptr;
  }
}

void
RestyleManager::ComputeAndProcessStyleChange(nsIFrame*          aFrame,
                                             nsChangeHint       aMinChange,
                                             RestyleTracker&    aRestyleTracker,
                                             nsRestyleHint      aRestyleHint)
{
  MOZ_ASSERT(mReframingStyleContexts, "should have rsc");
  nsStyleChangeList changeList;
  nsTArray<ElementRestyler::ContextToClear> contextsToClear;

  // swappedStructOwners needs to be kept alive until after
  // ProcessRestyledFrames and ClearCachedInheritedStyleDataOnDescendants
  // calls; see comment in ElementRestyler::Restyle.
  nsTArray<nsRefPtr<nsStyleContext>> swappedStructOwners;
  ElementRestyler::ComputeStyleChangeFor(aFrame, &changeList, aMinChange,
                                         aRestyleTracker, aRestyleHint,
                                         contextsToClear, swappedStructOwners);
  ProcessRestyledFrames(changeList);
  ClearCachedInheritedStyleDataOnDescendants(contextsToClear);
}

void
RestyleManager::ComputeAndProcessStyleChange(nsStyleContext*    aNewContext,
                                             Element*           aElement,
                                             nsChangeHint       aMinChange,
                                             RestyleTracker&    aRestyleTracker,
                                             nsRestyleHint      aRestyleHint)
{
  MOZ_ASSERT(mReframingStyleContexts, "should have rsc");
  MOZ_ASSERT(aNewContext->StyleDisplay()->mDisplay == NS_STYLE_DISPLAY_CONTENTS);
  nsIFrame* frame = GetNearestAncestorFrame(aElement);
  MOZ_ASSERT(frame, "display:contents node in map although it's a "
                    "display:none descendant?");
  TreeMatchContext treeMatchContext(true,
                                    nsRuleWalker::eRelevantLinkUnvisited,
                                    frame->PresContext()->Document());
  nsIContent* parent = aElement->GetParent();
  Element* parentElement =
    parent && parent->IsElement() ? parent->AsElement() : nullptr;
  treeMatchContext.InitAncestors(parentElement);
  nsTArray<nsIContent*> visibleKidsOfHiddenElement;
  nsTArray<ElementRestyler::ContextToClear> contextsToClear;

  // swappedStructOwners needs to be kept alive until after
  // ProcessRestyledFrames and ClearCachedInheritedStyleDataOnDescendants
  // calls; see comment in ElementRestyler::Restyle.
  nsTArray<nsRefPtr<nsStyleContext>> swappedStructOwners;
  nsStyleChangeList changeList;
  ElementRestyler r(frame->PresContext(), aElement, &changeList, aMinChange,
                    aRestyleTracker, treeMatchContext,
                    visibleKidsOfHiddenElement, contextsToClear,
                    swappedStructOwners);
  r.RestyleChildrenOfDisplayContentsElement(frame, aNewContext, aMinChange,
                                            aRestyleTracker, aRestyleHint);
  ProcessRestyledFrames(changeList);
  ClearCachedInheritedStyleDataOnDescendants(contextsToClear);
}

AutoDisplayContentsAncestorPusher::AutoDisplayContentsAncestorPusher(
  TreeMatchContext& aTreeMatchContext, nsPresContext* aPresContext,
  nsIContent* aParent)
  : mTreeMatchContext(aTreeMatchContext)
  , mPresContext(aPresContext)
{
  if (aParent) {
    nsFrameManager* fm = mPresContext->FrameManager();
    // Push display:contents mAncestors onto mTreeMatchContext.
    for (nsIContent* p = aParent; p && fm->GetDisplayContentsStyleFor(p);
         p = p->GetParent()) {
      mAncestors.AppendElement(p->AsElement());
    }
    bool hasFilter = mTreeMatchContext.mAncestorFilter.HasFilter();
    nsTArray<mozilla::dom::Element*>::size_type i = mAncestors.Length();
    while (i--) {
      if (hasFilter) {
        mTreeMatchContext.mAncestorFilter.PushAncestor(mAncestors[i]);
      }
      mTreeMatchContext.PushStyleScope(mAncestors[i]);
    }
  }
}

AutoDisplayContentsAncestorPusher::~AutoDisplayContentsAncestorPusher()
{
  // Pop the ancestors we pushed in the CTOR, if any.
  typedef nsTArray<mozilla::dom::Element*>::size_type sz;
  sz len = mAncestors.Length();
  bool hasFilter = mTreeMatchContext.mAncestorFilter.HasFilter();
  for (sz i = 0; i < len; ++i) {
    if (hasFilter) {
      mTreeMatchContext.mAncestorFilter.PopAncestor();
    }
    mTreeMatchContext.PopStyleScope(mAncestors[i]);
  }
}

#ifdef RESTYLE_LOGGING
uint32_t
RestyleManager::StructsToLog()
{
  static bool initialized = false;
  static uint32_t structs;
  if (!initialized) {
    structs = 0;
    const char* value = getenv("MOZ_DEBUG_RESTYLE_STRUCTS");
    if (value) {
      nsCString s(value);
      while (!s.IsEmpty()) {
        int32_t index = s.FindChar(',');
        nsStyleStructID sid;
        bool found;
        if (index == -1) {
          found = nsStyleContext::LookupStruct(s, sid);
          s.Truncate();
        } else {
          found = nsStyleContext::LookupStruct(Substring(s, 0, index), sid);
          s = Substring(s, index + 1);
        }
        if (found) {
          structs |= nsCachedStyleData::GetBitForSID(sid);
        }
      }
    }
    initialized = true;
  }
  return structs;
}
#endif

#ifdef DEBUG
/* static */ nsCString
RestyleManager::RestyleHintToString(nsRestyleHint aHint)
{
  nsCString result;
  bool any = false;
  const char* names[] = { "Self", "Subtree", "LaterSiblings", "CSSTransitions",
                          "CSSAnimations", "SVGAttrAnimations", "StyleAttribute",
                          "Force", "ForceDescendants" };
  uint32_t hint = aHint & ((1 << ArrayLength(names)) - 1);
  uint32_t rest = aHint & ~((1 << ArrayLength(names)) - 1);
  for (uint32_t i = 0; i < ArrayLength(names); i++) {
    if (hint & (1 << i)) {
      if (any) {
        result.AppendLiteral(" | ");
      }
      result.AppendPrintf("eRestyle_%s", names[i]);
      any = true;
    }
  }
  if (rest) {
    if (any) {
      result.AppendLiteral(" | ");
    }
    result.AppendPrintf("0x%0x", rest);
  } else {
    if (!any) {
      result.AppendLiteral("0");
    }
  }
  return result;
}

/* static */ nsCString
RestyleManager::ChangeHintToString(nsChangeHint aHint)
{
  nsCString result;
  bool any = false;
  const char* names[] = {
    "RepaintFrame", "NeedReflow", "ClearAncestorIntrinsics",
    "ClearDescendantIntrinsics", "NeedDirtyReflow", "SyncFrameView",
    "UpdateCursor", "UpdateEffects", "UpdateOpacityLayer",
    "UpdateTransformLayer", "ReconstructFrame", "UpdateOverflow",
    "UpdateSubtreeOverflow", "UpdatePostTransformOverflow",
    "UpdateParentOverflow",
    "ChildrenOnlyTransform", "RecomputePosition", "AddOrRemoveTransform",
    "BorderStyleNoneChange", "UpdateTextPath", "NeutralChange",
    "InvalidateRenderingObservers"
  };
  uint32_t hint = aHint & ((1 << ArrayLength(names)) - 1);
  uint32_t rest = aHint & ~((1 << ArrayLength(names)) - 1);
  if (hint == nsChangeHint_Hints_NotHandledForDescendants) {
    result.AppendLiteral("nsChangeHint_Hints_NotHandledForDescendants");
    hint = 0;
    any = true;
  } else {
    if ((hint & NS_STYLE_HINT_FRAMECHANGE) == NS_STYLE_HINT_FRAMECHANGE) {
      result.AppendLiteral("NS_STYLE_HINT_FRAMECHANGE");
      hint = hint & ~NS_STYLE_HINT_FRAMECHANGE;
      any = true;
    } else if ((hint & NS_STYLE_HINT_REFLOW) == NS_STYLE_HINT_REFLOW) {
      result.AppendLiteral("NS_STYLE_HINT_REFLOW");
      hint = hint & ~NS_STYLE_HINT_REFLOW;
      any = true;
    } else if ((hint & nsChangeHint_AllReflowHints) == nsChangeHint_AllReflowHints) {
      result.AppendLiteral("nsChangeHint_AllReflowHints");
      hint = hint & ~nsChangeHint_AllReflowHints;
      any = true;
    } else if ((hint & NS_STYLE_HINT_VISUAL) == NS_STYLE_HINT_VISUAL) {
      result.AppendLiteral("NS_STYLE_HINT_VISUAL");
      hint = hint & ~NS_STYLE_HINT_VISUAL;
      any = true;
    }
  }
  for (uint32_t i = 0; i < ArrayLength(names); i++) {
    if (hint & (1 << i)) {
      if (any) {
        result.AppendLiteral(" | ");
      }
      result.AppendPrintf("nsChangeHint_%s", names[i]);
      any = true;
    }
  }
  if (rest) {
    if (any) {
      result.AppendLiteral(" | ");
    }
    result.AppendPrintf("0x%0x", rest);
  } else {
    if (!any) {
      result.AppendLiteral("NS_STYLE_HINT_NONE");
    }
  }
  return result;
}

/* static */ nsCString
RestyleManager::StructNamesToString(uint32_t aSIDs)
{
  nsCString result;
  bool any = false;
  for (nsStyleStructID sid = nsStyleStructID(0);
       sid < nsStyleStructID_Length;
       sid = nsStyleStructID(sid + 1)) {
    if (aSIDs & nsCachedStyleData::GetBitForSID(sid)) {
      if (any) {
        result.AppendLiteral(",");
      }
      result.AppendPrintf("%s", nsStyleContext::StructName(sid));
      any = true;
    }
  }
  return result;
}

/* static */ nsCString
ElementRestyler::RestyleResultToString(RestyleResult aRestyleResult)
{
  nsCString result;
  switch (aRestyleResult) {
    case eRestyleResult_Stop:
      result.AssignLiteral("eRestyleResult_Stop");
      break;
    case eRestyleResult_Continue:
      result.AssignLiteral("eRestyleResult_Continue");
      break;
    case eRestyleResult_ContinueAndForceDescendants:
      result.AssignLiteral("eRestyleResult_ContinueAndForceDescendants");
      break;
    default:
      result.AppendPrintf("RestyleResult(%d)", aRestyleResult);
  }
  return result;
}
#endif

} // namespace mozilla
