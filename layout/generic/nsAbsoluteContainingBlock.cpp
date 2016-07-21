/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * code for managing absolutely positioned children of a rendering
 * object that is a containing block for them
 */

#include "nsAbsoluteContainingBlock.h"

#include "nsContainerFrame.h"
#include "nsGkAtoms.h"
#include "nsIPresShell.h"
#include "mozilla/ReflowInput.h"
#include "nsPresContext.h"
#include "nsCSSFrameConstructor.h"
#include "nsGridContainerFrame.h"

#include "mozilla/Snprintf.h"

#ifdef DEBUG
#include "nsBlockFrame.h"

static void PrettyUC(nscoord aSize, char* aBuf, int aBufSize)
{
  if (NS_UNCONSTRAINEDSIZE == aSize) {
    strcpy(aBuf, "UC");
  } else {
    if((int32_t)0xdeadbeef == aSize) {
      strcpy(aBuf, "deadbeef");
    } else {
      snprintf(aBuf, aBufSize, "%d", aSize);
    }
  }
}
#endif

using namespace mozilla;

void
nsAbsoluteContainingBlock::SetInitialChildList(nsIFrame*       aDelegatingFrame,
                                               ChildListID     aListID,
                                               nsFrameList&    aChildList)
{
  NS_PRECONDITION(mChildListID == aListID, "unexpected child list name");
#ifdef DEBUG
  nsFrame::VerifyDirtyBitSet(aChildList);
#endif
  mAbsoluteFrames.SetFrames(aChildList);
}

void
nsAbsoluteContainingBlock::AppendFrames(nsIFrame*      aDelegatingFrame,
                                        ChildListID    aListID,
                                        nsFrameList&   aFrameList)
{
  NS_ASSERTION(mChildListID == aListID, "unexpected child list");

  // Append the frames to our list of absolutely positioned frames
#ifdef DEBUG
  nsFrame::VerifyDirtyBitSet(aFrameList);
#endif
  mAbsoluteFrames.AppendFrames(nullptr, aFrameList);

  // no damage to intrinsic widths, since absolutely positioned frames can't
  // change them
  aDelegatingFrame->PresContext()->PresShell()->
    FrameNeedsReflow(aDelegatingFrame, nsIPresShell::eResize,
                     NS_FRAME_HAS_DIRTY_CHILDREN);
}

void
nsAbsoluteContainingBlock::InsertFrames(nsIFrame*      aDelegatingFrame,
                                        ChildListID    aListID,
                                        nsIFrame*      aPrevFrame,
                                        nsFrameList&   aFrameList)
{
  NS_ASSERTION(mChildListID == aListID, "unexpected child list");
  NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == aDelegatingFrame,
               "inserting after sibling frame with different parent");

#ifdef DEBUG
  nsFrame::VerifyDirtyBitSet(aFrameList);
#endif
  mAbsoluteFrames.InsertFrames(nullptr, aPrevFrame, aFrameList);

  // no damage to intrinsic widths, since absolutely positioned frames can't
  // change them
  aDelegatingFrame->PresContext()->PresShell()->
    FrameNeedsReflow(aDelegatingFrame, nsIPresShell::eResize,
                     NS_FRAME_HAS_DIRTY_CHILDREN);
}

void
nsAbsoluteContainingBlock::RemoveFrame(nsIFrame*       aDelegatingFrame,
                                       ChildListID     aListID,
                                       nsIFrame*       aOldFrame)
{
  NS_ASSERTION(mChildListID == aListID, "unexpected child list");
  nsIFrame* nif = aOldFrame->GetNextInFlow();
  if (nif) {
    nif->GetParent()->DeleteNextInFlowChild(nif, false);
  }

  mAbsoluteFrames.DestroyFrame(aOldFrame);
}

void
nsAbsoluteContainingBlock::Reflow(nsContainerFrame*        aDelegatingFrame,
                                  nsPresContext*           aPresContext,
                                  const ReflowInput& aReflowInput,
                                  nsReflowStatus&          aReflowStatus,
                                  const nsRect&            aContainingBlock,
                                  AbsPosReflowFlags        aFlags,
                                  nsOverflowAreas*         aOverflowAreas)
{
  nsReflowStatus reflowStatus = NS_FRAME_COMPLETE;

  const bool reflowAll = aReflowInput.ShouldReflowAllKids();
  const bool isGrid = !!(aFlags & AbsPosReflowFlags::eIsGridContainerCB);
  nsIFrame* kidFrame;
  nsOverflowContinuationTracker tracker(aDelegatingFrame, true);
  for (kidFrame = mAbsoluteFrames.FirstChild(); kidFrame; kidFrame = kidFrame->GetNextSibling()) {
    bool kidNeedsReflow = reflowAll || NS_SUBTREE_DIRTY(kidFrame) ||
      FrameDependsOnContainer(kidFrame,
                              !!(aFlags & AbsPosReflowFlags::eCBWidthChanged),
                              !!(aFlags & AbsPosReflowFlags::eCBHeightChanged));
    if (kidNeedsReflow && !aPresContext->HasPendingInterrupt()) {
      // Reflow the frame
      nsReflowStatus  kidStatus = NS_FRAME_COMPLETE;
      const nsRect& cb = isGrid ? nsGridContainerFrame::GridItemCB(kidFrame)
                                : aContainingBlock;
      ReflowAbsoluteFrame(aDelegatingFrame, aPresContext, aReflowInput, cb,
                          aFlags, kidFrame, kidStatus, aOverflowAreas);
      nsIFrame* nextFrame = kidFrame->GetNextInFlow();
      if (!NS_FRAME_IS_FULLY_COMPLETE(kidStatus) &&
          aDelegatingFrame->IsFrameOfType(nsIFrame::eCanContainOverflowContainers)) {
        // Need a continuation
        if (!nextFrame) {
          nextFrame =
            aPresContext->PresShell()->FrameConstructor()->
              CreateContinuingFrame(aPresContext, kidFrame, aDelegatingFrame);
        }
        // Add it as an overflow container.
        //XXXfr This is a hack to fix some of our printing dataloss.
        // See bug 154892. Not sure how to do it "right" yet; probably want
        // to keep continuations within an nsAbsoluteContainingBlock eventually.
        tracker.Insert(nextFrame, kidStatus);
        NS_MergeReflowStatusInto(&reflowStatus, kidStatus);
      }
      else {
        // Delete any continuations
        if (nextFrame) {
          nsOverflowContinuationTracker::AutoFinish fini(&tracker, kidFrame);
          nextFrame->GetParent()->DeleteNextInFlowChild(nextFrame, true);
        }
      }
    }
    else {
      tracker.Skip(kidFrame, reflowStatus);
      if (aOverflowAreas) {
        aDelegatingFrame->ConsiderChildOverflow(*aOverflowAreas, kidFrame);
      }
    }

    // Make a CheckForInterrupt call, here, not just HasPendingInterrupt.  That
    // will make sure that we end up reflowing aDelegatingFrame in cases when
    // one of our kids interrupted.  Otherwise we'd set the dirty or
    // dirty-children bit on the kid in the condition below, and then when
    // reflow completes and we go to mark dirty bits on all ancestors of that
    // kid we'll immediately bail out, because the kid already has a dirty bit.
    // In particular, we won't set any dirty bits on aDelegatingFrame, so when
    // the following reflow happens we won't reflow the kid in question.  This
    // might be slightly suboptimal in cases where |kidFrame| itself did not
    // interrupt, since we'll trigger a reflow of it too when it's not strictly
    // needed.  But the logic to not do that is enough more complicated, and
    // the case enough of an edge case, that this is probably better.
    if (kidNeedsReflow && aPresContext->CheckForInterrupt(aDelegatingFrame)) {
      if (aDelegatingFrame->GetStateBits() & NS_FRAME_IS_DIRTY) {
        kidFrame->AddStateBits(NS_FRAME_IS_DIRTY);
      } else {
        kidFrame->AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
      }
    }
  }

  // Abspos frames can't cause their parent to be incomplete,
  // only overflow incomplete.
  if (NS_FRAME_IS_NOT_COMPLETE(reflowStatus))
    NS_FRAME_SET_OVERFLOW_INCOMPLETE(reflowStatus);

  NS_MergeReflowStatusInto(&aReflowStatus, reflowStatus);
}

static inline bool IsFixedPaddingSize(const nsStyleCoord& aCoord)
  { return aCoord.ConvertsToLength(); }
static inline bool IsFixedMarginSize(const nsStyleCoord& aCoord)
  { return aCoord.ConvertsToLength(); }
static inline bool IsFixedOffset(const nsStyleCoord& aCoord)
  { return aCoord.ConvertsToLength(); }

bool
nsAbsoluteContainingBlock::FrameDependsOnContainer(nsIFrame* f,
                                                   bool aCBWidthChanged,
                                                   bool aCBHeightChanged)
{
  const nsStylePosition* pos = f->StylePosition();
  // See if f's position might have changed because it depends on a
  // placeholder's position
  // This can happen in the following cases:
  // 1) Vertical positioning.  "top" must be auto and "bottom" must be auto
  //    (otherwise the vertical position is completely determined by
  //    whichever of them is not auto and the height).
  // 2) Horizontal positioning.  "left" must be auto and "right" must be auto
  //    (otherwise the horizontal position is completely determined by
  //    whichever of them is not auto and the width).
  // See ReflowInput::InitAbsoluteConstraints -- these are the
  // only cases when we call CalculateHypotheticalBox().
  if ((pos->mOffset.GetTopUnit() == eStyleUnit_Auto &&
       pos->mOffset.GetBottomUnit() == eStyleUnit_Auto) ||
      (pos->mOffset.GetLeftUnit() == eStyleUnit_Auto &&
       pos->mOffset.GetRightUnit() == eStyleUnit_Auto)) {
    return true;
  }
  if (!aCBWidthChanged && !aCBHeightChanged) {
    // skip getting style data
    return false;
  }
  const nsStylePadding* padding = f->StylePadding();
  const nsStyleMargin* margin = f->StyleMargin();
  WritingMode wm = f->GetWritingMode();
  if (wm.IsVertical() ? aCBHeightChanged : aCBWidthChanged) {
    // See if f's inline-size might have changed.
    // If margin-inline-start/end, padding-inline-start/end,
    // inline-size, min/max-inline-size are all lengths, 'none', or enumerated,
    // then our frame isize does not depend on the parent isize.
    // Note that borders never depend on the parent isize.
    // XXX All of the enumerated values except -moz-available are ok too.
    if (pos->ISizeDependsOnContainer(wm) ||
        pos->MinISizeDependsOnContainer(wm) ||
        pos->MaxISizeDependsOnContainer(wm) ||
        !IsFixedPaddingSize(padding->mPadding.GetIStart(wm)) ||
        !IsFixedPaddingSize(padding->mPadding.GetIEnd(wm))) {
      return true;
    }

    // See if f's position might have changed. If we're RTL then the
    // rules are slightly different. We'll assume percentage or auto
    // margins will always induce a dependency on the size
    if (!IsFixedMarginSize(margin->mMargin.GetIStart(wm)) ||
        !IsFixedMarginSize(margin->mMargin.GetIEnd(wm))) {
      return true;
    }
    if (!wm.IsBidiLTR()) {
      // Note that even if 'istart' is a length, our position can
      // still depend on the containing block isze, because if
      // 'iend' is also a length we will discard 'istart' and be
      // positioned relative to the containing block iend edge.
      // 'istart' length and 'iend' auto is the only combination
      // we can be sure of.
      if (!IsFixedOffset(pos->mOffset.GetIStart(wm)) ||
          pos->mOffset.GetIEndUnit(wm) != eStyleUnit_Auto) {
        return true;
      }
    } else {
      if (!IsFixedOffset(pos->mOffset.GetIStart(wm))) {
        return true;
      }
    }
  }
  if (wm.IsVertical() ? aCBWidthChanged : aCBHeightChanged) {
    // See if f's block-size might have changed.
    // If margin-block-start/end, padding-block-start/end,
    // min-block-size, and max-block-size are all lengths or 'none',
    // and bsize is a length or bsize and bend are auto and bstart is not auto,
    // then our frame bsize does not depend on the parent bsize.
    // Note that borders never depend on the parent bsize.
    if ((pos->BSizeDependsOnContainer(wm) &&
         !(pos->BSize(wm).GetUnit() == eStyleUnit_Auto &&
           pos->mOffset.GetBEndUnit(wm) == eStyleUnit_Auto &&
           pos->mOffset.GetBStartUnit(wm) != eStyleUnit_Auto)) ||
        pos->MinBSizeDependsOnContainer(wm) ||
        pos->MaxBSizeDependsOnContainer(wm) ||
        !IsFixedPaddingSize(padding->mPadding.GetBStart(wm)) ||
        !IsFixedPaddingSize(padding->mPadding.GetBEnd(wm))) {
      return true;
    }
      
    // See if f's position might have changed.
    if (!IsFixedMarginSize(margin->mMargin.GetBStart(wm)) ||
        !IsFixedMarginSize(margin->mMargin.GetBEnd(wm))) {
      return true;
    }
    if (!IsFixedOffset(pos->mOffset.GetBStart(wm))) {
      return true;
    }
  }
  return false;
}

void
nsAbsoluteContainingBlock::DestroyFrames(nsIFrame* aDelegatingFrame,
                                         nsIFrame* aDestructRoot)
{
  mAbsoluteFrames.DestroyFramesFrom(aDestructRoot);
}

void
nsAbsoluteContainingBlock::MarkSizeDependentFramesDirty()
{
  DoMarkFramesDirty(false);
}

void
nsAbsoluteContainingBlock::MarkAllFramesDirty()
{
  DoMarkFramesDirty(true);
}

void
nsAbsoluteContainingBlock::DoMarkFramesDirty(bool aMarkAllDirty)
{
  for (nsIFrame* kidFrame : mAbsoluteFrames) {
    if (aMarkAllDirty) {
      kidFrame->AddStateBits(NS_FRAME_IS_DIRTY);
    } else if (FrameDependsOnContainer(kidFrame, true, true)) {
      // Add the weakest flags that will make sure we reflow this frame later
      kidFrame->AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
    }
  }
}

// XXX Optimize the case where it's a resize reflow and the absolutely
// positioned child has the exact same size and position and skip the
// reflow...

// When bug 154892 is checked in, make sure that when 
// mChildListID == kFixedList, the height is unconstrained.
// since we don't allow replicated frames to split.

void
nsAbsoluteContainingBlock::ReflowAbsoluteFrame(nsIFrame*                aDelegatingFrame,
                                               nsPresContext*           aPresContext,
                                               const ReflowInput& aReflowInput,
                                               const nsRect&            aContainingBlock,
                                               AbsPosReflowFlags        aFlags,
                                               nsIFrame*                aKidFrame,
                                               nsReflowStatus&          aStatus,
                                               nsOverflowAreas*         aOverflowAreas)
{
#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsFrame::IndentBy(stdout,nsBlockFrame::gNoiseIndent);
    printf("abs pos ");
    nsAutoString name;
    aKidFrame->GetFrameName(name);
    printf("%s ", NS_LossyConvertUTF16toASCII(name).get());

    char width[16];
    char height[16];
    PrettyUC(aReflowInput.AvailableWidth(), width, 16);
    PrettyUC(aReflowInput.AvailableHeight(), height, 16);
    printf(" a=%s,%s ", width, height);
    PrettyUC(aReflowInput.ComputedWidth(), width, 16);
    PrettyUC(aReflowInput.ComputedHeight(), height, 16);
    printf("c=%s,%s \n", width, height);
  }
  AutoNoisyIndenter indent(nsBlockFrame::gNoisy);
#endif // DEBUG

  WritingMode wm = aKidFrame->GetWritingMode();
  LogicalSize logicalCBSize(wm, aContainingBlock.Size());
  nscoord availISize = logicalCBSize.ISize(wm);
  if (availISize == -1) {
    NS_ASSERTION(aReflowInput.ComputedSize(wm).ISize(wm) !=
                   NS_UNCONSTRAINEDSIZE,
                 "Must have a useful inline-size _somewhere_");
    availISize =
      aReflowInput.ComputedSizeWithPadding(wm).ISize(wm);
  }

  uint32_t rsFlags = 0;
  if (aFlags & AbsPosReflowFlags::eIsGridContainerCB) {
    // When a grid container generates the abs.pos. CB for a *child* then
    // the static-position is the CB origin (i.e. of the grid area rect).
    // https://drafts.csswg.org/css-grid/#static-position
    nsIFrame* placeholder =
      aPresContext->PresShell()->GetPlaceholderFrameFor(aKidFrame);
    if (placeholder && placeholder->GetParent() == aDelegatingFrame) {
      rsFlags |= ReflowInput::STATIC_POS_IS_CB_ORIGIN;
    }
  }
  ReflowInput kidReflowInput(aPresContext, aReflowInput, aKidFrame,
                                   LogicalSize(wm, availISize,
                                               NS_UNCONSTRAINEDSIZE),
                                   &logicalCBSize, rsFlags);

  // Get the border values
  WritingMode outerWM = aReflowInput.GetWritingMode();
  const LogicalMargin border(outerWM,
                             aReflowInput.mStyleBorder->GetComputedBorder());
  const LogicalMargin margin =
    kidReflowInput.ComputedLogicalMargin().ConvertTo(outerWM, wm);
  bool constrainBSize = (aReflowInput.AvailableBSize() != NS_UNCONSTRAINEDSIZE)
    && (aFlags & AbsPosReflowFlags::eConstrainHeight)
       // Don't split if told not to (e.g. for fixed frames)
    && (aDelegatingFrame->GetType() != nsGkAtoms::inlineFrame)
       //XXX we don't handle splitting frames for inline absolute containing blocks yet
    && (aKidFrame->GetLogicalRect(aContainingBlock.Size()).BStart(wm) <=
        aReflowInput.AvailableBSize());
       // Don't split things below the fold. (Ideally we shouldn't *have*
       // anything totally below the fold, but we can't position frames
       // across next-in-flow breaks yet.
  if (constrainBSize) {
    kidReflowInput.AvailableBSize() =
      aReflowInput.AvailableBSize() - border.ConvertTo(wm, outerWM).BStart(wm) -
      kidReflowInput.ComputedLogicalMargin().BStart(wm);
    if (NS_AUTOOFFSET != kidReflowInput.ComputedLogicalOffsets().BStart(wm)) {
      kidReflowInput.AvailableBSize() -=
        kidReflowInput.ComputedLogicalOffsets().BStart(wm);
    }
  }

  // Do the reflow
  ReflowOutput kidDesiredSize(kidReflowInput);
  aKidFrame->Reflow(aPresContext, kidDesiredSize, kidReflowInput, aStatus);

  const LogicalSize kidSize = kidDesiredSize.Size(wm).ConvertTo(outerWM, wm);

  LogicalMargin offsets =
    kidReflowInput.ComputedLogicalOffsets().ConvertTo(outerWM, wm);

  // If we're solving for start in either inline or block direction,
  // then compute it now that we know the dimensions.
  if ((NS_AUTOOFFSET == offsets.IStart(outerWM)) ||
      (NS_AUTOOFFSET == offsets.BStart(outerWM))) {
    if (-1 == logicalCBSize.ISize(wm)) {
      // Get the containing block width/height
      logicalCBSize =
        kidReflowInput.ComputeContainingBlockRectangle(aPresContext,
                                                       &aReflowInput);
    }

    if (NS_AUTOOFFSET == offsets.IStart(outerWM)) {
      NS_ASSERTION(NS_AUTOOFFSET != offsets.IEnd(outerWM),
                   "Can't solve for both start and end");
      offsets.IStart(outerWM) =
        logicalCBSize.ConvertTo(outerWM, wm).ISize(outerWM) -
        offsets.IEnd(outerWM) - margin.IStartEnd(outerWM) -
        kidSize.ISize(outerWM);
    }
    if (NS_AUTOOFFSET == offsets.BStart(outerWM)) {
      offsets.BStart(outerWM) =
        logicalCBSize.ConvertTo(outerWM, wm).BSize(outerWM) -
        offsets.BEnd(outerWM) - margin.BStartEnd(outerWM) -
        kidSize.BSize(outerWM);
    }
    kidReflowInput.SetComputedLogicalOffsets(offsets.ConvertTo(wm, outerWM));
  }

  // Position the child relative to our padding edge
  LogicalRect rect(outerWM,
                   border.IStart(outerWM) + offsets.IStart(outerWM) +
                     margin.IStart(outerWM),
                   border.BStart(outerWM) + offsets.BStart(outerWM) +
                     margin.BStart(outerWM),
                   kidSize.ISize(outerWM), kidSize.BSize(outerWM));
  nsRect r =
    rect.GetPhysicalRect(outerWM, logicalCBSize.GetPhysicalSize(wm) +
                         border.Size(outerWM).GetPhysicalSize(outerWM));

  // Offset the frame rect by the given origin of the absolute containing block.
  // If the frame is auto-positioned on both sides of an axis, it will be
  // positioned based on its containing block and we don't need to offset
  // (unless the caller demands it (the STATIC_POS_IS_CB_ORIGIN case)).
  if (aContainingBlock.TopLeft() != nsPoint(0, 0)) {
    const nsStyleSides& offsets = kidReflowInput.mStylePosition->mOffset;
    if (!(offsets.GetLeftUnit() == eStyleUnit_Auto &&
          offsets.GetRightUnit() == eStyleUnit_Auto) ||
        (rsFlags & ReflowInput::STATIC_POS_IS_CB_ORIGIN)) {
      r.x += aContainingBlock.x;
    }
    if (!(offsets.GetTopUnit() == eStyleUnit_Auto &&
          offsets.GetBottomUnit() == eStyleUnit_Auto) ||
        (rsFlags & ReflowInput::STATIC_POS_IS_CB_ORIGIN)) {
      r.y += aContainingBlock.y;
    }
  }

  aKidFrame->SetRect(r);

  nsView* view = aKidFrame->GetView();
  if (view) {
    // Size and position the view and set its opacity, visibility, content
    // transparency, and clip
    nsContainerFrame::SyncFrameViewAfterReflow(aPresContext, aKidFrame, view,
                                               kidDesiredSize.VisualOverflow());
  } else {
    nsContainerFrame::PositionChildViews(aKidFrame);
  }

  aKidFrame->DidReflow(aPresContext, &kidReflowInput,
                       nsDidReflowStatus::FINISHED);

#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsFrame::IndentBy(stdout,nsBlockFrame::gNoiseIndent - 1);
    printf("abs pos ");
    nsAutoString name;
    aKidFrame->GetFrameName(name);
    printf("%s ", NS_LossyConvertUTF16toASCII(name).get());
    printf("%p rect=%d,%d,%d,%d\n", static_cast<void*>(aKidFrame),
           r.x, r.y, r.width, r.height);
  }
#endif

  if (aOverflowAreas) {
    aOverflowAreas->UnionWith(kidDesiredSize.mOverflowAreas + r.TopLeft());
  }
}
