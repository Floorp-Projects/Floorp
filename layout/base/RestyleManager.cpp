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
#include "nsStyleUtil.h"
#include "nsCSSFrameConstructor.h"
#include "nsSVGEffects.h"
#include "nsCSSRendering.h"
#include "nsAnimationManager.h"
#include "nsTransitionManager.h"
#include "nsViewManager.h"
#include "nsRenderingContext.h"
#include "nsSVGIntegrationUtils.h"
#include "nsContainerFrame.h"
#include "nsPlaceholderFrame.h"
#include "nsViewportFrame.h"
#include "nsSVGTextFrame2.h"
#include "nsSVGTextPathFrame.h"
#include "nsIRootBox.h"
#include "nsIDOMMutationEvent.h"

namespace mozilla {

RestyleManager::RestyleManager(nsPresContext* aPresContext)
  : mPresContext(aPresContext)
  , mRebuildAllStyleData(false)
  , mObservingRefreshDriver(false)
  , mInStyleRefresh(false)
  , mPromoteReflowsToReframeRoot(false)
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
                             nsFrameManager* aFrameManager,
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
                                  nsFrameManager* aFrameManager,
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
          DoApplyRenderingChangeToTree(outOfFlowFrame, aFrameManager,
                                       aChange);
        } else if (lists.CurrentID() == nsIFrame::kPopupList) {
          DoApplyRenderingChangeToTree(child, aFrameManager,
                                       aChange);
        } else {  // regular frame
          SyncViewsAndInvalidateDescendants(child, aFrameManager, aChange);
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
                             nsFrameManager* aFrameManager,
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
    SyncViewsAndInvalidateDescendants(aFrame, aFrameManager,
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
  DoApplyRenderingChangeToTree(aFrame, shell->FrameManager(), aChange);
#ifdef DEBUG
  gInApplyRenderingChangeToTree = false;
#endif
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
  MOZ_STATIC_ASSERT(0 <= NS_STYLE_POSITION_ABSOLUTE &&
                    NS_STYLE_POSITION_ABSOLUTE < 32, "Style constant out of range");
  MOZ_STATIC_ASSERT(0 <= NS_STYLE_POSITION_FIXED &&
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

static nsIFrame*
FindReflowRootFor(nsIFrame* aFrame)
{
  for (nsIFrame* f = aFrame; f; f = f->GetParent()) {
    if (f->GetStateBits() & NS_FRAME_REFLOW_ROOT) {
      return f;
    }
  }
  return nullptr;
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

    if (mPromoteReflowsToReframeRoot &&
        (hint & (nsChangeHint_ReconstructFrame | nsChangeHint_NeedReflow))) {
      nsIFrame* reflowRoot = FindReflowRootFor(frame);
      if (!reflowRoot) {
        // Reflow root is the viewport. Better reframe the document.
        // We don't do this for elements which are inside a reflow root --- they
        // should be OK.
        nsIContent* root = mPresContext->Document()->GetRootElement();
        if (root) {
          NS_UpdateHint(hint, nsChangeHint_ReconstructFrame);
          content = root;
        }
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
          FrameConstructor()->DebugVerifyStyleTree(frame);
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
    FrameConstructor()-> // NOTE: removed later in patch series
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
  FrameConstructor()-> // NOTE: removed later in patch series
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
    const nsSize size = cb->GetSize();
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

} // namespace mozilla
