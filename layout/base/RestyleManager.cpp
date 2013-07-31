/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Code responsible for managing style changes: tracking what style
 * changes need to happen, scheduling them, and doing them.
 */

#include "RestyleManager.h"
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
#include "nsSVGTextFrame2.h"
#include "nsSVGTextPathFrame.h"
#include "nsIRootBox.h"
#include "nsIDOMMutationEvent.h"

#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#endif

namespace mozilla {

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

  for ( ; aFrame; aFrame = nsLayoutUtils::GetNextContinuationOrSpecialSibling(aFrame)) {
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
      if (aFrame->GetType() == nsGkAtoms::svgTextPathFrame) {
        // Invalidate and reflow the entire nsSVGTextFrame:
        static_cast<nsSVGTextPathFrame*>(aFrame)->NotifyGlyphMetricsChange();
      } else if (aFrame->IsSVGText()) {
        // Invalidate and reflow the entire nsSVGTextFrame2:
        NS_ASSERTION(aFrame->GetContent()->IsSVG(nsGkAtoms::textPath),
                     "expected frame for a <textPath> element");
        nsIFrame* text = nsLayoutUtils::GetClosestFrameOfType(
                                                      aFrame,
                                                      nsGkAtoms::svgTextFrame2);
        NS_ASSERTION(text, "expected to find an ancestor nsSVGTextFrame2");
        static_cast<nsSVGTextFrame2*>(text)->NotifyGlyphMetricsChange();
      } else {
        NS_ABORT_IF_FALSE(false, "unexpected frame got "
                                 "nsChangeHint_UpdateTextPath");
      }
    }
    if (aChange & nsChangeHint_UpdateOpacityLayer) {
      // FIXME/bug 796697: we can get away with empty transactions for
      // opacity updates in many cases.
      needInvalidatingPaint = true;
      aFrame->MarkLayersActive(nsChangeHint_UpdateOpacityLayer);
      if (nsSVGIntegrationUtils::UsingEffectsForFrame(aFrame)) {
        // SVG effects paints the opacity without using
        // nsDisplayOpacity. We need to invalidate manually.
        aFrame->InvalidateFrameSubtree();
      }
    }
    if ((aChange & nsChangeHint_UpdateTransformLayer) &&
        aFrame->IsTransformed()) {
      aFrame->MarkLayersActive(nsChangeHint_UpdateTransformLayer);
      // If we're not already going to do an invalidating paint, see
      // if we can get away with only updating the transform on a
      // layer for this frame, and not scheduling an invalidating
      // paint.
      if (!needInvalidatingPaint) {
        needInvalidatingPaint |= !aFrame->TryUpdateTransformOnly();
      }
    }
    if (aChange & nsChangeHint_ChildrenOnlyTransform) {
      needInvalidatingPaint = true;
      nsIFrame* childFrame =
        GetFrameForChildrenOnlyTransformHint(aFrame)->GetFirstPrincipalChild();
      for ( ; childFrame; childFrame = childFrame->GetNextSibling()) {
        childFrame->MarkLayersActive(nsChangeHint_UpdateTransformLayer);
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

  // Don't process position changes on frames which have views or the ones which
  // have a view somewhere in their descendants, because the corresponding view
  // needs to be repositioned properly as well.
  if (aFrame->HasView() ||
      (aFrame->GetStateBits() & NS_FRAME_HAS_CHILD_WITH_VIEW)) {
    StyleChangeReflow(aFrame, nsChangeHint_NeedReflow);
    return false;
  }

  const nsStyleDisplay* display = aFrame->StyleDisplay();
  // Changes to the offsets of a non-positioned element can safely be ignored.
  if (display->mPosition == NS_STYLE_POSITION_STATIC) {
    return true;
  }

  aFrame->SchedulePaint();

  // For relative positioning, we can simply update the frame rect
  if (display->mPosition == NS_STYLE_POSITION_RELATIVE) {
    switch (display->mDisplay) {
      case NS_STYLE_DISPLAY_TABLE_CAPTION:
      case NS_STYLE_DISPLAY_TABLE_CELL:
      case NS_STYLE_DISPLAY_TABLE_ROW:
      case NS_STYLE_DISPLAY_TABLE_ROW_GROUP:
      case NS_STYLE_DISPLAY_TABLE_HEADER_GROUP:
      case NS_STYLE_DISPLAY_TABLE_FOOTER_GROUP:
      case NS_STYLE_DISPLAY_TABLE_COLUMN:
      case NS_STYLE_DISPLAY_TABLE_COLUMN_GROUP:
        // We don't currently support relative positioning of inner
        // table elements.  If we apply offsets to things we haven't
        // previously offset, we'll get confused.  So bail.
        return true;
      default:
        break;
    }

    nsIFrame* cb = aFrame->GetContainingBlock();
    const nsSize size = cb->GetContentRectRelativeToSelf().Size();
    const nsPoint oldOffsets = aFrame->GetRelativeOffset();
    nsMargin newOffsets;

    // Move the frame
    nsHTMLReflowState::ComputeRelativeOffsets(
        cb->StyleVisibility()->mDirection,
        aFrame, size.width, size.height, newOffsets);
    NS_ASSERTION(newOffsets.left == -newOffsets.right &&
                 newOffsets.top == -newOffsets.bottom,
                 "ComputeRelativeOffsets should return valid results");
    aFrame->SetPosition(aFrame->GetPosition() - oldOffsets +
                        nsPoint(newOffsets.left, newOffsets.top));

    return true;
  }

  // For absolute positioning, the width can potentially change if width is
  // auto and either of left or right are not.  The height can also potentially
  // change if height is auto and either of top or bottom are not.  In these
  // cases we fall back to a reflow, and in all other cases, we attempt to
  // move the frame here.
  // Note that it is possible for the dimensions to not change in the above
  // cases, so we should be a little smarter here and only fall back to reflow
  // when the dimensions will really change (bug 745485).
  const nsStylePosition* position = aFrame->StylePosition();
  if (position->mWidth.GetUnit() != eStyleUnit_Auto &&
      position->mHeight.GetUnit() != eStyleUnit_Auto) {
    // For the absolute positioning case, set up a fake HTML reflow state for
    // the frame, and then get the offsets from it.
    nsRefPtr<nsRenderingContext> rc = aFrame->PresContext()->GetPresShell()->
      GetReferenceRenderingContext();

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
    parentReflowState.mComputedMargin.SizeTo(0, 0, 0, 0);
    parentSize.height = NS_AUTOHEIGHT;

    parentReflowState.mComputedPadding = parentFrame->GetUsedPadding();
    parentReflowState.mComputedBorderPadding =
      parentFrame->GetUsedBorderAndPadding();

    nsSize availSize(parentSize.width, NS_INTRINSICSIZE);

    nsSize size = aFrame->GetSize();
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

    // If we're solving for 'left' or 'top', then compute it here, in order to
    // match the reflow code path.
    if (NS_AUTOOFFSET == reflowState.mComputedOffsets.left) {
      reflowState.mComputedOffsets.left = cbSize.width -
                                          reflowState.mComputedOffsets.right -
                                          reflowState.mComputedMargin.right -
                                          size.width -
                                          reflowState.mComputedMargin.left;
    }

    if (NS_AUTOOFFSET == reflowState.mComputedOffsets.top) {
      reflowState.mComputedOffsets.top = cbSize.height -
                                         reflowState.mComputedOffsets.bottom -
                                         reflowState.mComputedMargin.bottom -
                                         size.height -
                                         reflowState.mComputedMargin.top;
    }

    // Move the frame
    nsPoint pos(parentBorder.left + reflowState.mComputedOffsets.left +
                reflowState.mComputedMargin.left,
                parentBorder.top + reflowState.mComputedOffsets.top +
                reflowState.mComputedMargin.top);
    aFrame->SetPosition(pos);

    return true;
  }

  // Fall back to a reflow
  StyleChangeReflow(aFrame, nsChangeHint_NeedReflow);
  return false;
}

nsresult
RestyleManager::StyleChangeReflow(nsIFrame* aFrame, nsChangeHint aHint)
{
  // If the frame hasn't even received an initial reflow, then don't
  // send it a style-change reflow!
  if (aFrame->GetStateBits() & NS_FRAME_FIRST_REFLOW)
    return NS_OK;

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
    aFrame = nsLayoutUtils::GetNextContinuationOrSpecialSibling(aFrame);
  } while (aFrame);

  return NS_OK;
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
       f = nsLayoutUtils::GetNextContinuationOrSpecialSibling(f)) {
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

  PROFILER_LABEL("CSS", "ProcessRestyledFrames");

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
      if (NeedToReframeForAddingOrRemovingTransform(frame)) {
        NS_UpdateHint(hint, nsChangeHint_ReconstructFrame);
      } else {
        // Normally frame construction would set state bits as needed,
        // but we're not going to reconstruct the frame so we need to set them.
        // It's because we need to set this state on each affected frame
        // that we can't coalesce nsChangeHint_AddOrRemoveTransform hints up
        // to ancestors (i.e. it can't be an inherited change hint).
        if (frame->IsPositioned()) {
          // If a transform has been added, we'll be taking this path,
          // but we may be taking this path even if a transform has been
          // removed. It's OK to add the bit even if it's not needed.
          frame->AddStateBits(NS_FRAME_MAY_BE_TRANSFORMED);
          if (!frame->IsAbsoluteContainer() &&
              (frame->GetStateBits() & NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN)) {
            frame->MarkAsAbsoluteContainingBlock();
          }
        } else {
          // Don't remove NS_FRAME_MAY_BE_TRANSFORMED since it may still by
          // transformed by other means. It's OK to have the bit even if it's
          // not needed.
          if (frame->IsAbsoluteContainer()) {
            frame->MarkAsNotAbsoluteContainingBlock();
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
                                nsChangeHint_ChildrenOnlyTransform));
      }

      if (hint & nsChangeHint_UpdateEffects) {
        nsSVGEffects::UpdateEffects(frame);
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
        // It is possible for this to fall back to a reflow
        if (!RecomputePosition(frame)) {
          didReflowThisFrame = true;
        }
      }
      NS_ASSERTION(!(hint & nsChangeHint_ChildrenOnlyTransform) ||
                   (hint & nsChangeHint_UpdateOverflow),
                   "nsChangeHint_UpdateOverflow should be passed too");
      if ((hint & nsChangeHint_UpdateOverflow) && !didReflowThisFrame) {
        if (hint & nsChangeHint_ChildrenOnlyTransform) {
          // The overflow areas of the child frames need to be updated:
          nsIFrame* hintFrame = GetFrameForChildrenOnlyTransformHint(frame);
          nsIFrame* childFrame = hintFrame->GetFirstPrincipalChild();
          for ( ; childFrame; childFrame = childFrame->GetNextSibling()) {
            NS_ABORT_IF_FALSE(childFrame->IsFrameOfType(nsIFrame::eSVG),
                              "Not expecting non-SVG children");
            // If |childFrame| is dirty or has dirty children, we don't bother
            // updating overflows since that will happen when it's reflowed.
            if (!(childFrame->GetStateBits() &
                  (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN))) {
              mOverflowChangedTracker.AddFrame(childFrame);
            }
            NS_ASSERTION(!nsLayoutUtils::GetNextContinuationOrSpecialSibling(childFrame),
                         "SVG frames should not have continuations or special siblings");
            NS_ASSERTION(childFrame->GetParent() == hintFrame,
                         "SVG child frame not expected to have different parent");
          }
        }
        // If |frame| is dirty or has dirty children, we don't bother updating
        // overflows since that will happen when it's reflowed.
        if (!(frame->GetStateBits() &
              (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN))) {
          while (frame) {
            mOverflowChangedTracker.AddFrame(frame);

            frame =
              nsLayoutUtils::GetNextContinuationOrSpecialSibling(frame);
          }
        }
      }
      if (hint & nsChangeHint_UpdateCursor) {
        mPresContext->PresShell()->SynthesizeMouseMove(false);
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
                               bool            aRestyleDescendants)
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
                          aRestyleTracker, aRestyleDescendants);
    ProcessRestyledFrames(changeList);
  } else {
    // no frames, reconstruct for content
    FrameConstructor()->MaybeRecreateFramesForElement(aElement);
  }
}

// Forwarded nsIDocumentObserver method, to handle restyling (and
// passing the notification to the frame).
nsresult
RestyleManager::ContentStateChanged(nsIContent* aContent,
                                    nsEventStates aStateMask)
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

    primaryFrame->ContentStatesChanged(aStateMask);
  }


  nsRestyleHint rshint =
    styleSet->HasStateDependentStyle(mPresContext, aElement, aStateMask);

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
    // XXXwaterson should probably check for special IB siblings
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
                        aRestyleTracker, true);
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
  int32_t contextIndex = -1;
  while (1) {
    nsStyleContext* extraContext = aFrame->GetAdditionalStyleContext(++contextIndex);
    if (extraContext) {
      VerifyContextParent(aPresContext, aFrame, extraContext, context);
    }
    else {
      break;
    }
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

  nsIContent* content = aParentContent ? aParentContent : aFrame->GetContent();
  return content->AsElement();
}

static nsIFrame*
GetPrevContinuationWithPossiblySameStyle(nsIFrame* aFrame)
{
  // Account for {ib} splits when looking for "prevContinuation".  In
  // particular, for the first-continuation of a part of an {ib} split we
  // want to use the special prevsibling of the special prevsibling of
  // aFrame, which should have the same style context as aFrame itself.
  // In particular, if aFrame is the first continuation of an inline part
  // of an {ib} split then its special prevsibling is a block, and the
  // special prevsibling of _that_ is an inline, just like aFrame.
  // Similarly, if aFrame is the first continuation of a block part of an
  // {ib} split (an {ib} wrapper block), then its special prevsibling is
  // an inline and the special prevsibling of that is either another {ib}
  // wrapper block block or null.
  nsIFrame *prevContinuation = aFrame->GetPrevContinuation();
  if (!prevContinuation && (aFrame->GetStateBits() & NS_FRAME_IS_SPECIAL)) {
    // We're the first continuation, so we can just get the frame
    // property directly
    prevContinuation = static_cast<nsIFrame*>(
      aFrame->Properties().Get(nsIFrame::IBSplitSpecialPrevSibling()));
    if (prevContinuation) {
      prevContinuation = static_cast<nsIFrame*>(
        prevContinuation->Properties().Get(nsIFrame::IBSplitSpecialPrevSibling()));
    }
  }
  return prevContinuation;
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
  // XXXbz can oldContext really ever be null?
  if (oldContext) {
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
        DebugOnly<nsChangeHint> styleChange =
          oldContext->CalcStyleDifference(newContext, nsChangeHint(0));
        // The style change is always 0 because we have the same rulenode and
        // CalcStyleDifference optimizes us away.  That's OK, though:
        // reparenting should never trigger a frame reconstruct, and whenever
        // it's happening we already plan to reflow and repaint the frames.
        NS_ASSERTION(!(styleChange & nsChangeHint_ReconstructFrame),
                     "Our frame tree is likely to be bogus!");

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
        if ((aFrame->GetStateBits() & NS_FRAME_IS_SPECIAL) &&
            !aFrame->GetPrevContinuation()) {
          nsIFrame* sib = static_cast<nsIFrame*>
            (aFrame->Properties().Get(nsIFrame::IBSplitSpecialSibling()));
          if (sib) {
            ReparentStyleContext(sib);
          }
        }

        // do additional contexts
        int32_t contextIndex = -1;
        while (1) {
          nsStyleContext* oldExtraContext =
            aFrame->GetAdditionalStyleContext(++contextIndex);
          if (oldExtraContext) {
            nsRefPtr<nsStyleContext> newExtraContext;
            newExtraContext = mPresContext->StyleSet()->
                                ReparentStyleContext(oldExtraContext,
                                                     newContext, nullptr);
            if (newExtraContext) {
              if (newExtraContext != oldExtraContext) {
                // Make sure to call CalcStyleDifference so that the new
                // context ends up resolving all the structs the old context
                // resolved.
                styleChange =
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
          else {
            break;
          }
        }
#ifdef DEBUG
        VerifyStyleTree(mPresContext, aFrame, newParentContext);
#endif
      }
    }
  }
  return NS_OK;
}

ElementRestyler::ElementRestyler(nsPresContext* aPresContext,
                                 nsIFrame* aFrame,
                                 nsStyleChangeList* aChangeList,
                                 nsChangeHint aHintsHandledByAncestors)
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
{
}

ElementRestyler::ElementRestyler(const ElementRestyler& aParentRestyler,
                                 nsIFrame* aFrame)
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
{
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
{
}

void
ElementRestyler::CaptureChange(nsStyleContext* aOldContext,
                               nsStyleContext* aNewContext,
                               nsChangeHint aChangeToAssume)
{
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
 * Recompute style for mFrame and accumulate changes into mChangeList
 * given that mHintsHandled is already accumulated for an ancestor.
 * mParentContent is the content node used to resolve the parent style
 * context.  This means that, for pseudo-elements, it is the content
 * that should be used for selector matching (rather than the fake
 * content node attached to the frame).
 */
void
ElementRestyler::Restyle(nsRestyleHint aRestyleHint,
                         RestyleTracker&    aRestyleTracker,
                         DesiredA11yNotifications aDesiredA11yNotifications,
                         nsTArray<nsIContent*>& aVisibleKidsOfHiddenElement,
                         TreeMatchContext &aTreeMatchContext)
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
  // XXXldb get new context from prev-in-flow if possible, to avoid
  // duplication.  (Or should we just let |GetContext| handle that?)
  // Getting the hint would be nice too, but that's harder.

  // XXXbryner we may be able to avoid some of the refcounting goop here.
  // We do need a reference to oldContext for the lifetime of this function, and it's possible
  // that the frame has the last reference to it, so AddRef it here.

  nsChangeHint assumeDifferenceHint = NS_STYLE_HINT_NONE;
  // XXXbz oldContext should just be an nsRefPtr
  nsStyleContext* oldContext = mFrame->StyleContext();
  nsStyleSet* styleSet = mPresContext->StyleSet();

  // XXXbz the nsIFrame constructor takes an nsStyleContext, so how
  // could oldContext be null?
  if (oldContext) {
    oldContext->AddRef();

#ifdef ACCESSIBILITY
    bool wasFrameVisible = nsIPresShell::IsAccessibilityActive() ?
      oldContext->StyleVisibility()->IsVisible() : false;
#endif

    nsIAtom* const pseudoTag = oldContext->GetPseudo();
    const nsCSSPseudoElements::Type pseudoType = oldContext->GetPseudoType();

    if (mContent && mContent->IsElement()) {
      mContent->OwnerDoc()->FlushPendingLinkUpdates();
      RestyleTracker::RestyleData restyleData;
      if (aRestyleTracker.GetRestyleData(mContent->AsElement(), &restyleData)) {
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

    nsStyleContext* parentContext;
    nsIFrame* resolvedChild = nullptr;
    // Get the frame providing the parent style context.  If it is a
    // child, then resolve the provider first.
    nsIFrame* providerFrame = mFrame->GetParentStyleContextFrame();
    bool isChild = providerFrame && providerFrame->GetParent() == mFrame;
    if (!isChild) {
      if (providerFrame)
        parentContext = providerFrame->StyleContext();
      else
        parentContext = nullptr;
    }
    else {
      MOZ_ASSERT(providerFrame->GetContent() == mFrame->GetContent(),
                 "Postcondition for GetParentStyleContextFrame() violated. "
                 "That means we need to add the current element to the "
                 "ancestor filter.");

      // resolve the provider here (before mFrame below).

      // assumeDifferenceHint forces the parent's change to be also
      // applied to this frame, no matter what
      // nsStyleContext::CalcStyleDifference says. CalcStyleDifference
      // can't be trusted because it assumes any changes to the parent
      // style context provider will be automatically propagated to
      // the frame(s) with child style contexts.

      ElementRestyler providerRestyler(PARENT_CONTEXT_FROM_CHILD_FRAME,
                                       *this, providerFrame);
      providerRestyler.Restyle(aRestyleHint,
                                                   aRestyleTracker,
                                                   aDesiredA11yNotifications,
                                                   aVisibleKidsOfHiddenElement,
                                                   aTreeMatchContext);
      assumeDifferenceHint = providerRestyler.HintsHandledForFrame();

      // The provider's new context becomes the parent context of
      // mFrame's context.
      parentContext = providerFrame->StyleContext();
      // Set |resolvedChild| so we don't bother resolving the
      // provider again.
      resolvedChild = providerFrame;
    }

    if (providerFrame != mFrame->GetParent()) {
      // We don't actually know what the parent style context's
      // non-inherited hints were, so assume the worst.
      mParentFrameHintsNotHandledForDescendants =
        nsChangeHint_Hints_NotHandledForDescendants;
    }

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
      nsIFrame *nextContinuation = mFrame->GetNextContinuation();
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
      // And assert the same thing for {ib} splits.  See the comments in
      // GetPrevContinuationWithPossiblySameStyle for an explanation of
      // why we step two forward in the special sibling chain.
      if ((mFrame->GetStateBits() & NS_FRAME_IS_SPECIAL) &&
          !mFrame->GetPrevContinuation()) {
        nsIFrame *nextIBSibling = static_cast<nsIFrame*>(
          mFrame->Properties().Get(nsIFrame::IBSplitSpecialSibling()));
        if (nextIBSibling) {
          nextIBSibling = static_cast<nsIFrame*>(
            nextIBSibling->Properties().Get(nsIFrame::IBSplitSpecialSibling()));
        }
        if (nextIBSibling) {
          nsStyleContext *nextIBSiblingContext =
            nextIBSibling->StyleContext();
          NS_ASSERTION(oldContext == nextIBSiblingContext ||
                       oldContext->GetPseudo() !=
                         nextIBSiblingContext->GetPseudo() ||
                       oldContext->GetParent() !=
                         nextIBSiblingContext->GetParent(),
                       "continuations should have the same style context");
        }
      }
    }
#endif

    // do primary context
    nsRefPtr<nsStyleContext> newContext;
    nsIFrame *prevContinuation =
      GetPrevContinuationWithPossiblySameStyle(mFrame);
    nsStyleContext *prevContinuationContext;
    bool copyFromContinuation =
      prevContinuation &&
      (prevContinuationContext = prevContinuation->StyleContext())
        ->GetPseudo() == oldContext->GetPseudo() &&
       prevContinuationContext->GetParent() == parentContext;
    if (copyFromContinuation) {
      // Just use the style context from the frame's previous
      // continuation (see assertion about mFrame->GetNextContinuation()
      // above, which we would have previously hit for mFrame's previous
      // continuation).
      newContext = prevContinuationContext;
    }
    else if (pseudoTag == nsCSSAnonBoxes::mozNonElement) {
      NS_ASSERTION(mFrame->GetContent(),
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
                                                              mFrame,
                                                              pseudoType));
    } else if (pseudoType == nsCSSPseudoElements::ePseudo_AnonBox) {
      newContext = styleSet->ResolveAnonymousBoxStyle(pseudoTag,
                                                      parentContext);
    }
    else {
      Element* element = ElementForStyleContext(mParentContent,
                                                mFrame,
                                                pseudoType);
      if (pseudoTag) {
        if (pseudoTag == nsCSSPseudoElements::before ||
            pseudoTag == nsCSSPseudoElements::after) {
          // XXX what other pseudos do we need to treat like this?
          newContext = styleSet->ProbePseudoElementStyle(element,
                                                         pseudoType,
                                                         parentContext,
                                                         aTreeMatchContext);
          if (!newContext) {
            // This pseudo should no longer exist; gotta reframe
            NS_UpdateHint(mHintsHandled, nsChangeHint_ReconstructFrame);
            mChangeList->AppendChange(mFrame, element,
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
          newContext = styleSet->ResolvePseudoElementStyle(element,
                                                           pseudoType,
                                                           parentContext);
        }
      }
      else {
        NS_ASSERTION(mFrame->GetContent(),
                     "non pseudo-element frame without content node");
        // Skip flex-item style fixup for anonymous subtrees:
        TreeMatchContext::AutoFlexItemStyleFixupSkipper
          flexFixupSkipper(aTreeMatchContext,
                           element->IsRootOfNativeAnonymousSubtree());
        newContext = styleSet->ResolveStyleFor(element, parentContext,
                                               aTreeMatchContext);
      }
    }

    NS_ASSERTION(newContext, "failed to get new style context");
    if (newContext) {
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
          TryStartingTransition(mPresContext, mFrame->GetContent(),
                                oldContext, &newContext);
        }

        CaptureChange(oldContext, newContext, assumeDifferenceHint);
        if (!(mHintsHandled & nsChangeHint_ReconstructFrame)) {
          // if frame gets regenerated, let it keep old context
          mFrame->SetStyleContext(newContext);
        }
      }
      oldContext->Release();
    }
    else {
      NS_ERROR("resolve style context failed");
      newContext = oldContext;  // new context failed, recover...
    }

    // do additional contexts
    // XXXbz might be able to avoid selector matching here in some
    // cases; won't worry about it for now.
    int32_t contextIndex = -1;
    while (1 == 1) {
      nsStyleContext* oldExtraContext = nullptr;
      oldExtraContext = mFrame->GetAdditionalStyleContext(++contextIndex);
      if (oldExtraContext) {
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
                                                                newContext);
        }
        if (newExtraContext) {
          if (oldExtraContext != newExtraContext) {
            CaptureChange(oldExtraContext, newExtraContext, assumeDifferenceHint);
            if (!(mHintsHandled & nsChangeHint_ReconstructFrame)) {
              mFrame->SetAdditionalStyleContext(contextIndex, newExtraContext);
            }
          }
        }
      }
      else {
        break;
      }
    }

    // now look for undisplayed child content and pseudos

    // When the root element is display:none, we still construct *some*
    // frames that have the root element as their mContent, down to the
    // DocElementContainingBlock.
    bool checkUndisplayed;
    nsIContent* undisplayedParent;
    nsCSSFrameConstructor* frameConstructor = mPresContext->FrameConstructor();
    if (pseudoTag) {
      checkUndisplayed = mFrame == frameConstructor->
                                     GetDocElementContainingBlock();
      undisplayedParent = nullptr;
    } else {
      checkUndisplayed = !!mFrame->GetContent();
      undisplayedParent = mFrame->GetContent();
    }
    if (checkUndisplayed) {
      UndisplayedNode* undisplayed =
        frameConstructor->GetAllUndisplayedContentIn(undisplayedParent);
      for (TreeMatchContext::AutoAncestorPusher
             pushAncestor(undisplayed, aTreeMatchContext,
                          undisplayedParent ? undisplayedParent->AsElement()
                                            : nullptr);
           undisplayed; undisplayed = undisplayed->mNext) {
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
        bool pushInsertionPoint = parent && parent->IsActiveChildrenElement();
        TreeMatchContext::AutoAncestorPusher
          insertionPointPusher(pushInsertionPoint,
                               aTreeMatchContext,
                               parent && parent->IsElement() ? parent->AsElement() : nullptr);

        nsRestyleHint thisChildHint = childRestyleHint;
        RestyleTracker::RestyleData undisplayedRestyleData;
        if (aRestyleTracker.GetRestyleData(undisplayed->mContent->AsElement(),
                                           &undisplayedRestyleData)) {
          thisChildHint =
            nsRestyleHint(thisChildHint | undisplayedRestyleData.mRestyleHint);
        }
        nsRefPtr<nsStyleContext> undisplayedContext;
        if (thisChildHint) {
          undisplayedContext =
            styleSet->ResolveStyleFor(undisplayed->mContent->AsElement(),
                                      newContext,
                                      aTreeMatchContext);
        } else {
          undisplayedContext =
            styleSet->ReparentStyleContext(undisplayed->mStyle, newContext,
                                           undisplayed->mContent->AsElement());
        }
        if (undisplayedContext) {
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

    // Check whether we might need to create a new ::before frame.
    // There's no need to do this if we're planning to reframe already
    // or if we're not forcing restyles on kids.
    if (!(mHintsHandled & nsChangeHint_ReconstructFrame) &&
        childRestyleHint) {
      // Make sure not to do this for pseudo-frames or frames that
      // can't have generated content.
      if (!pseudoTag &&
          ((mFrame->GetStateBits() & NS_FRAME_MAY_HAVE_GENERATED_CONTENT) ||
           // Our content insertion frame might have gotten flagged
           (mFrame->GetContentInsertionFrame()->GetStateBits() &
            NS_FRAME_MAY_HAVE_GENERATED_CONTENT))) {
        // Check for a new :before pseudo and an existing :before
        // frame, but only if the frame is the first continuation.
        nsIFrame* prevContinuation = mFrame->GetPrevContinuation();
        if (!prevContinuation) {
          // Checking for a :before frame is cheaper than getting the
          // :before style context.
          if (!nsLayoutUtils::GetBeforeFrame(mFrame) &&
              nsLayoutUtils::HasPseudoStyle(mFrame->GetContent(), newContext,
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

    // Check whether we might need to create a new ::after frame.
    // There's no need to do this if we're planning to reframe already
    // or if we're not forcing restyles on kids.
    if (!(mHintsHandled & nsChangeHint_ReconstructFrame) &&
        childRestyleHint) {
      // Make sure not to do this for pseudo-frames or frames that
      // can't have generated content.
      if (!pseudoTag &&
          ((mFrame->GetStateBits() & NS_FRAME_MAY_HAVE_GENERATED_CONTENT) ||
           // Our content insertion frame might have gotten flagged
           (mFrame->GetContentInsertionFrame()->GetStateBits() &
            NS_FRAME_MAY_HAVE_GENERATED_CONTENT))) {
        // Check for new :after content, but only if the frame is the
        // last continuation.
        nsIFrame* nextContinuation = mFrame->GetNextContinuation();

        if (!nextContinuation) {
          // Getting the :after frame is more expensive than getting the pseudo
          // context, so get the pseudo context first.
          if (nsLayoutUtils::HasPseudoStyle(mFrame->GetContent(), newContext,
                                            nsCSSPseudoElements::ePseudo_after,
                                            mPresContext) &&
              !nsLayoutUtils::GetAfterFrame(mFrame)) {
            // have to create the new :after frame
            NS_UpdateHint(mHintsHandled, nsChangeHint_ReconstructFrame);
            mChangeList->AppendChange(mFrame, mContent,
                                      nsChangeHint_ReconstructFrame);
          }
        }
      }
    }

    // There is no need to waste time crawling into a frame's children
    // on a frame change.  The act of reconstructing frames will force
    // new style contexts to be resolved on all of this frame's
    // descendants anyway, so we want to avoid wasting time processing
    // style contexts that we're just going to throw away anyway. - dwh
    if (!(mHintsHandled & nsChangeHint_ReconstructFrame)) {

      DesiredA11yNotifications kidsDesiredA11yNotification =
        aDesiredA11yNotifications;
#ifdef ACCESSIBILITY
      A11yNotificationType ourA11yNotification = eDontNotify;
      // Notify a11y for primary frame only if it's a root frame of visibility
      // changes or its parent frame was hidden while it stays visible and
      // it is not inside a {ib} split or is the first frame of {ib} split.
      if (nsIPresShell::IsAccessibilityActive() &&
          !mFrame->GetPrevContinuation() &&
          !nsLayoutUtils::FrameIsNonFirstInIBSplit(mFrame)) {
        if (aDesiredA11yNotifications == eSendAllNotifications) {
          bool isFrameVisible = newContext->StyleVisibility()->IsVisible();
          if (isFrameVisible != wasFrameVisible) {
            if (isFrameVisible) {
              // Notify a11y the element (perhaps with its children) was shown.
              // We don't fall into this case if this element gets or stays shown
              // while its parent becomes hidden.
              kidsDesiredA11yNotification = eSkipNotifications;
              ourA11yNotification = eNotifyShown;
            } else {
              // The element is being hidden; its children may stay visible, or
              // become visible after being hidden previously. If we'll find
              // visible children then we should notify a11y about that as if
              // they were inserted into tree. Notify a11y this element was
              // hidden.
              kidsDesiredA11yNotification = eNotifyIfShown;
              ourA11yNotification = eNotifyHidden;
            }
          }
        } else if (aDesiredA11yNotifications == eNotifyIfShown &&
                   newContext->StyleVisibility()->IsVisible()) {
          // Notify a11y that element stayed visible while its parent was
          // hidden.
          aVisibleKidsOfHiddenElement.AppendElement(mFrame->GetContent());
          kidsDesiredA11yNotification = eSkipNotifications;
        }
      }
#endif

      // now do children
      nsIFrame::ChildListIterator lists(mFrame);
      for (TreeMatchContext::AutoAncestorPusher
             pushAncestor(!lists.IsDone(),
                          aTreeMatchContext,
                          mContent && mContent->IsElement()
                            ? mContent->AsElement() : nullptr);
           !lists.IsDone(); lists.Next()) {
        nsFrameList::Enumerator childFrames(lists.CurrentList());
        for (; !childFrames.AtEnd(); childFrames.Next()) {
          nsIFrame* child = childFrames.get();
          if (!(child->GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
            // Get the parent of the child frame's content and check if it is a XBL
            // children element. Push the children element as an ancestor here because it does
            // not have a frame and would not otherwise be pushed as an ancestor.

            // Check if the frame has a content because |child| may be a nsPageFrame that does
            // not have a content.
            nsIContent* parent = child->GetContent() ? child->GetContent()->GetParent() : nullptr;
            bool pushInsertionPoint = parent && parent->IsActiveChildrenElement();
            TreeMatchContext::AutoAncestorPusher
              insertionPointPusher(pushInsertionPoint, aTreeMatchContext,
                                   parent && parent->IsElement() ? parent->AsElement() : nullptr);

            // only do frames that are in flow
            if (nsGkAtoms::placeholderFrame == child->GetType()) { // placeholder
              // get out of flow frame and recur there
              nsIFrame* outOfFlowFrame =
                nsPlaceholderFrame::GetRealFrameForPlaceholder(child);
              NS_ASSERTION(outOfFlowFrame, "no out-of-flow frame");
              NS_ASSERTION(outOfFlowFrame != resolvedChild,
                           "out-of-flow frame not a true descendant");

              // Note that the out-of-flow may not be a geometric descendant of
              // the frame where we started the reresolve.  Therefore, even if
              // mHintsHandled already includes nsChangeHint_AllReflowHints we don't
              // want to pass that on to the out-of-flow reresolve, since that
              // can lead to the out-of-flow not getting reflowed when it should
              // be (eg a reresolve starting at <body> that involves reflowing
              // the <body> would miss reflowing fixed-pos nodes that also need
              // reflow).  In the cases when the out-of-flow _is_ a geometric
              // descendant of a frame we already have a reflow hint for,
              // reflow coalescing should keep us from doing the work twice.

              // |nsFrame::GetParentStyleContextFrame| checks being out
              // of flow so that this works correctly.
              do {
                ElementRestyler oofRestyler(*this, outOfFlowFrame);
                oofRestyler.mHintsHandled =
                  NS_SubtractHint(oofRestyler.mHintsHandled,
                                  nsChangeHint_AllReflowHints);
                oofRestyler.Restyle(childRestyleHint,
                                      aRestyleTracker,
                                      kidsDesiredA11yNotification,
                                      aVisibleKidsOfHiddenElement,
                                      aTreeMatchContext);
              } while ((outOfFlowFrame = outOfFlowFrame->GetNextContinuation()));

              // reresolve placeholder's context under the same parent
              // as the out-of-flow frame
              ElementRestyler phRestyler(*this, child);
              phRestyler.Restyle(childRestyleHint,
                                    aRestyleTracker,
                                    kidsDesiredA11yNotification,
                                    aVisibleKidsOfHiddenElement,
                                    aTreeMatchContext);
            }
            else {  // regular child frame
              if (child != resolvedChild) {
                ElementRestyler childRestyler(*this, child);
                childRestyler.Restyle(childRestyleHint,
                                      aRestyleTracker,
                                      kidsDesiredA11yNotification,
                                      aVisibleKidsOfHiddenElement,
                                      aTreeMatchContext);
              }
            }
          }
        }
      }
      // XXX need to do overflow frames???

#ifdef ACCESSIBILITY
      // Send notifications about visibility changes.
      if (ourA11yNotification == eNotifyShown) {
        nsAccessibilityService* accService = nsIPresShell::AccService();
        if (accService) {
          nsIPresShell* presShell = mFrame->PresContext()->GetPresShell();
          nsIContent* content = mFrame->GetContent();

          accService->ContentRangeInserted(presShell, content->GetParent(),
                                           content,
                                           content->GetNextSibling());
        }
      } else if (ourA11yNotification == eNotifyHidden) {
        nsAccessibilityService* accService = nsIPresShell::AccService();
        if (accService) {
          nsIPresShell* presShell = mFrame->PresContext()->GetPresShell();
          nsIContent* content = mFrame->GetContent();
          accService->ContentRemoved(presShell, content->GetParent(), content);

          // Process children staying shown.
          uint32_t visibleContentCount = aVisibleKidsOfHiddenElement.Length();
          for (uint32_t idx = 0; idx < visibleContentCount; idx++) {
            nsIContent* childContent = aVisibleKidsOfHiddenElement[idx];
            accService->ContentRangeInserted(presShell, childContent->GetParent(),
                                             childContent,
                                             childContent->GetNextSibling());
          }
          aVisibleKidsOfHiddenElement.Clear();
        }
      }
#endif
    }
  }
}

void
RestyleManager::ComputeStyleChangeFor(nsIFrame*          aFrame,
                                      nsStyleChangeList* aChangeList,
                                      nsChangeHint       aMinChange,
                                      RestyleTracker&    aRestyleTracker,
                                      bool               aRestyleDescendants)
{
  PROFILER_LABEL("CSS", "ComputeStyleChangeFor");

  nsIContent *content = aFrame->GetContent();
  if (aMinChange) {
    aChangeList->AppendChange(aFrame, content, aMinChange);
  }

  nsIFrame* frame = aFrame;
  nsIFrame* frame2 = aFrame;

  NS_ASSERTION(!frame->GetPrevContinuation(), "must start with the first in flow");

  // We want to start with this frame and walk all its next-in-flows,
  // as well as all its special siblings and their next-in-flows,
  // reresolving style on all the frames we encounter in this walk.

  FramePropertyTable* propTable = mPresContext->PropertyTable();

  TreeMatchContext treeMatchContext(true,
                                    nsRuleWalker::eRelevantLinkUnvisited,
                                    mPresContext->Document());
  nsIContent *parent = content ? content->GetParent() : nullptr;
  Element *parentElement =
    parent && parent->IsElement() ? parent->AsElement() : nullptr;
  treeMatchContext.InitAncestors(parentElement);
  nsTArray<nsIContent*> visibleKidsOfHiddenElement;
  do {
    // Outer loop over special siblings
    do {
      // Inner loop over next-in-flows of the current frame
      ElementRestyler restyler(mPresContext, frame, aChangeList,
                               aMinChange);

      restyler.Restyle(aRestyleDescendants ? eRestyle_Subtree : eRestyle_Self,
                              aRestyleTracker,
                              ElementRestyler::eSendAllNotifications,
                              visibleKidsOfHiddenElement,
                              treeMatchContext);

      if (restyler.HintsHandledForFrame() & nsChangeHint_ReconstructFrame) {
        // If it's going to cause a framechange, then don't bother
        // with the continuations or special siblings since they'll be
        // clobbered by the frame reconstruct anyway.
        NS_ASSERTION(!frame->GetPrevContinuation(),
                     "continuing frame had more severe impact than first-in-flow");
        return;
      }

      frame = frame->GetNextContinuation();
    } while (frame);

    // Might we have special siblings?
    if (!(frame2->GetStateBits() & NS_FRAME_IS_SPECIAL)) {
      // nothing more to do here
      return;
    }

    frame2 = static_cast<nsIFrame*>
      (propTable->Get(frame2, nsIFrame::IBSplitSpecialSibling()));
    frame = frame2;
  } while (frame2);
}

} // namespace mozilla
