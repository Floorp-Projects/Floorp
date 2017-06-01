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
#include "mozilla/CSSAlignUtils.h"
#include "mozilla/ReflowInput.h"
#include "nsPresContext.h"
#include "nsCSSFrameConstructor.h"
#include "nsGridContainerFrame.h"

#include "mozilla/Sprintf.h"

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

typedef mozilla::CSSAlignUtils::AlignJustifyFlags AlignJustifyFlags;

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
                                  const ReflowInput&       aReflowInput,
                                  nsReflowStatus&          aReflowStatus,
                                  const nsRect&            aContainingBlock,
                                  AbsPosReflowFlags        aFlags,
                                  nsOverflowAreas*         aOverflowAreas)
{
  nsReflowStatus reflowStatus;

  const bool reflowAll = aReflowInput.ShouldReflowAllKids();
  const bool isGrid = !!(aFlags & AbsPosReflowFlags::eIsGridContainerCB);
  nsIFrame* kidFrame;
  nsOverflowContinuationTracker tracker(aDelegatingFrame, true);
  for (kidFrame = mAbsoluteFrames.FirstChild(); kidFrame; kidFrame = kidFrame->GetNextSibling()) {
    bool kidNeedsReflow = reflowAll || NS_SUBTREE_DIRTY(kidFrame) ||
      FrameDependsOnContainer(kidFrame,
                              !!(aFlags & AbsPosReflowFlags::eCBWidthChanged),
                              !!(aFlags & AbsPosReflowFlags::eCBHeightChanged));
    nscoord availBSize = aReflowInput.AvailableBSize();
    const nsRect& cb = isGrid ? nsGridContainerFrame::GridItemCB(kidFrame)
                              : aContainingBlock;
    WritingMode containerWM = aReflowInput.GetWritingMode();
    if (!kidNeedsReflow && availBSize != NS_UNCONSTRAINEDSIZE) {
      // If we need to redo pagination on the kid, we need to reflow it.
      // This can happen either if the available height shrunk and the
      // kid (or its overflow that creates overflow containers) is now
      // too large to fit in the available height, or if the available
      // height has increased and the kid has a next-in-flow that we
      // might need to pull from.
      WritingMode kidWM = kidFrame->GetWritingMode();
      if (containerWM.GetBlockDir() != kidWM.GetBlockDir()) {
        // Not sure what the right test would be here.
        kidNeedsReflow = true;
      } else {
        nscoord kidBEnd = kidFrame->GetLogicalRect(cb.Size()).BEnd(kidWM);
        nscoord kidOverflowBEnd =
          LogicalRect(containerWM,
                      // Use ...RelativeToSelf to ignore transforms
                      kidFrame->GetScrollableOverflowRectRelativeToSelf() +
                        kidFrame->GetPosition(),
                      aContainingBlock.Size()).BEnd(containerWM);
        MOZ_ASSERT(kidOverflowBEnd >= kidBEnd);
        if (kidOverflowBEnd > availBSize ||
            (kidBEnd < availBSize && kidFrame->GetNextInFlow())) {
          kidNeedsReflow = true;
        }
      }
    }
    if (kidNeedsReflow && !aPresContext->HasPendingInterrupt()) {
      // Reflow the frame
      nsReflowStatus kidStatus;
      ReflowAbsoluteFrame(aDelegatingFrame, aPresContext, aReflowInput, cb,
                          aFlags, kidFrame, kidStatus, aOverflowAreas);
      nsIFrame* nextFrame = kidFrame->GetNextInFlow();
      if (!kidStatus.IsFullyComplete() &&
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
        reflowStatus.MergeCompletionStatusFrom(kidStatus);
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
  if (reflowStatus.IsIncomplete())
    reflowStatus.SetOverflowIncomplete();

  aReflowStatus.MergeCompletionStatusFrom(reflowStatus);
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
  }

  // Since we store coordinates relative to top and left, the position
  // of a frame depends on that of its container if it is fixed relative
  // to the right or bottom, or if it is positioned using percentages
  // relative to the left or top.  Because of the dependency on the
  // sides (left and top) that we use to store coordinates, these tests
  // are easier to do using physical coordinates rather than logical.
  if (aCBWidthChanged) {
    if (!IsFixedOffset(pos->mOffset.GetLeft())) {
      return true;
    }
    // Note that even if 'left' is a length, our position can still
    // depend on the containing block width, because if our direction or
    // writing-mode moves from right to left (in either block or inline
    // progression) and 'right' is not 'auto', we will discard 'left'
    // and be positioned relative to the containing block right edge.
    // 'left' length and 'right' auto is the only combination we can be
    // sure of.
    if ((wm.GetInlineDir() == WritingMode::eInlineRTL ||
         wm.GetBlockDir() == WritingMode::eBlockRL) &&
        pos->mOffset.GetRightUnit() != eStyleUnit_Auto) {
      return true;
    }
  }
  if (aCBHeightChanged) {
    if (!IsFixedOffset(pos->mOffset.GetTop())) {
      return true;
    }
    // See comment above for width changes.
    if (wm.GetInlineDir() == WritingMode::eInlineBTT &&
        pos->mOffset.GetBottomUnit() != eStyleUnit_Auto) {
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

// Given an out-of-flow frame, this method returns the parent frame of its
// placeholder frame or null if it doesn't have a placeholder for some reason.
static nsContainerFrame*
GetPlaceholderContainer(nsIFrame* aPositionedFrame)
{
  nsIFrame* placeholder = aPositionedFrame->GetPlaceholderFrame();
  return placeholder ? placeholder->GetParent() : nullptr;
}

/**
 * This function returns the offset of an abs/fixed-pos child's static
 * position, with respect to the "start" corner of its alignment container,
 * according to CSS Box Alignment.  This function only operates in a single
 * axis at a time -- callers can choose which axis via the |aAbsPosCBAxis|
 * parameter.
 *
 * @param aKidReflowInput The ReflowInput for the to-be-aligned abspos child.
 * @param aKidSizeInAbsPosCBWM The child frame's size (after it's been given
 *                             the opportunity to reflow), in terms of
 *                             aAbsPosCBWM.
 * @param aAbsPosCBSize The abspos CB size, in terms of aAbsPosCBWM.
 * @param aPlaceholderContainer The parent of the child frame's corresponding
 *                              placeholder frame, cast to a nsContainerFrame.
 *                              (This will help us choose which alignment enum
 *                              we should use for the child.)
 * @param aAbsPosCBWM The child frame's containing block's WritingMode.
 * @param aAbsPosCBAxis The axis (of the containing block) that we should
 *                      be doing this computation for.
 */
static nscoord
OffsetToAlignedStaticPos(const ReflowInput& aKidReflowInput,
                         const LogicalSize& aKidSizeInAbsPosCBWM,
                         const LogicalSize& aAbsPosCBSize,
                         nsContainerFrame* aPlaceholderContainer,
                         WritingMode aAbsPosCBWM,
                         LogicalAxis aAbsPosCBAxis)
{
  if (!aPlaceholderContainer) {
    // (The placeholder container should be the thing that kicks this whole
    // process off, by setting PLACEHOLDER_STATICPOS_NEEDS_CSSALIGN.  So it
    // should exist... but bail gracefully if it doesn't.)
    NS_ERROR("Missing placeholder-container when computing a "
             "CSS Box Alignment static position");
    return 0;
  }

  // (Most of this function is simply preparing args that we'll pass to
  // AlignJustifySelf at the end.)

  // NOTE: Our alignment container is aPlaceholderContainer's content-box
  // (or an area within it, if aPlaceholderContainer is a grid). So, we'll
  // perform most of our arithmetic/alignment in aPlaceholderContainer's
  // WritingMode. For brevity, we use the abbreviation "pc" for "placeholder
  // container" in variables below.
  WritingMode pcWM = aPlaceholderContainer->GetWritingMode();

  // Find what axis aAbsPosCBAxis corresponds to, in placeholder's parent's
  // writing-mode.
  LogicalAxis pcAxis = (pcWM.IsOrthogonalTo(aAbsPosCBWM)
                        ? GetOrthogonalAxis(aAbsPosCBAxis)
                        : aAbsPosCBAxis);

  LayoutFrameType parentType = aPlaceholderContainer->Type();
  LogicalSize alignAreaSize(pcWM);
  if (parentType == LayoutFrameType::FlexContainer) {
    // The alignment container is the flex container's content box:
    alignAreaSize = aPlaceholderContainer->GetLogicalSize(pcWM);
    LogicalMargin pcBorderPadding =
      aPlaceholderContainer->GetLogicalUsedBorderAndPadding(pcWM);
    alignAreaSize -= pcBorderPadding.Size(pcWM);
  } else if (parentType == LayoutFrameType::GridContainer) {
    // This abspos elem's parent is a grid container. Per CSS Grid 10.1 & 10.2:
    //  - If the grid container *also* generates the abspos containing block (a
    // grid area) for this abspos child, we use that abspos containing block as
    // the alignment container, too. (And its size is aAbsPosCBSize.)
    //  - Otherwise, we use the grid's padding box as the alignment container.
    // https://drafts.csswg.org/css-grid/#static-position
    if (aPlaceholderContainer == aKidReflowInput.mCBReflowInput->mFrame) {
      // The alignment container is the grid area that we're using as the
      // absolute containing block.
      alignAreaSize = aAbsPosCBSize.ConvertTo(pcWM, aAbsPosCBWM);
    } else {
      // The alignment container is a the grid container's padding box (which
      // we can get by subtracting away its border from frame's size):
      alignAreaSize = aPlaceholderContainer->GetLogicalSize(pcWM);
      LogicalMargin pcBorder =
        aPlaceholderContainer->GetLogicalUsedBorder(pcWM);
      alignAreaSize -= pcBorder.Size(pcWM);
    }
  } else {
    NS_ERROR("Unsupported container for abpsos CSS Box Alignment");
    return 0; // (leave the child at the start of its alignment container)
  }

  nscoord alignAreaSizeInAxis = (pcAxis == eLogicalAxisInline)
    ? alignAreaSize.ISize(pcWM)
    : alignAreaSize.BSize(pcWM);

  AlignJustifyFlags flags = AlignJustifyFlags::eIgnoreAutoMargins;
  uint16_t alignConst =
    aPlaceholderContainer->CSSAlignmentForAbsPosChild(aKidReflowInput, pcAxis);
  // XXXdholbert: Handle <overflow-position> in bug 1311892 (by conditionally
  // setting AlignJustifyFlags::eOverflowSafe in |flags|.)  For now, we behave
  // as if "unsafe" was the specified value (which is basically equivalent to
  // the default behavior, when no value is specified -- though the default
  // behavior also has some [at-risk] extra nuance about scroll containers...)
  // For now we ignore & strip off <overflow-position> bits, until bug 1311892.
  alignConst &= ~NS_STYLE_ALIGN_FLAG_BITS;

  // Find out if placeholder-container & the OOF child have the same start-sides
  // in the placeholder-container's pcAxis.
  WritingMode kidWM = aKidReflowInput.GetWritingMode();
  if (pcWM.ParallelAxisStartsOnSameSide(pcAxis, kidWM)) {
    flags |= AlignJustifyFlags::eSameSide;
  }

  // (baselineAdjust is unused. CSSAlignmentForAbsPosChild() should've
  // converted 'baseline'/'last baseline' enums to their fallback values.)
  const nscoord baselineAdjust = nscoord(0);

  // AlignJustifySelf operates in the kid's writing mode, so we need to
  // represent the child's size and the desired axis in that writing mode:
  LogicalSize kidSizeInOwnWM = aKidSizeInAbsPosCBWM.ConvertTo(kidWM,
                                                              aAbsPosCBWM);
  LogicalAxis kidAxis = (kidWM.IsOrthogonalTo(aAbsPosCBWM)
                         ? GetOrthogonalAxis(aAbsPosCBAxis)
                         : aAbsPosCBAxis);

  nscoord offset =
    CSSAlignUtils::AlignJustifySelf(alignConst, kidAxis, flags,
                                    baselineAdjust, alignAreaSizeInAxis,
                                    aKidReflowInput, kidSizeInOwnWM);

  // "offset" is in terms of the CSS Box Alignment container (i.e. it's in
  // terms of pcWM). But our return value needs to in terms of the containing
  // block's writing mode, which might have the opposite directionality in the
  // given axis. In that case, we just need to negate "offset" when returning,
  // to make it have the right effect as an offset for coordinates in the
  // containing block's writing mode.
  if (!pcWM.ParallelAxisStartsOnSameSide(pcAxis, aAbsPosCBWM)) {
    return -offset;
  }
  return offset;
}

void
nsAbsoluteContainingBlock::ResolveSizeDependentOffsets(
  nsPresContext* aPresContext,
  ReflowInput& aKidReflowInput,
  const LogicalSize& aKidSize,
  const LogicalMargin& aMargin,
  LogicalMargin* aOffsets,
  LogicalSize* aLogicalCBSize)
{
  WritingMode wm = aKidReflowInput.GetWritingMode();
  WritingMode outerWM = aKidReflowInput.mParentReflowInput->GetWritingMode();

  // Now that we know the child's size, we resolve any sentinel values in its
  // IStart/BStart offset coordinates that depend on that size.
  //  * NS_AUTOOFFSET indicates that the child's position in the given axis
  // is determined by its end-wards offset property, combined with its size and
  // available space. e.g.: "top: auto; height: auto; bottom: 50px"
  //  * m{I,B}OffsetsResolvedAfterSize indicate that the child is using its
  // static position in that axis, *and* its static position is determined by
  // the axis-appropriate css-align property (which may require the child's
  // size, e.g. to center it within the parent).
  if ((NS_AUTOOFFSET == aOffsets->IStart(outerWM)) ||
      (NS_AUTOOFFSET == aOffsets->BStart(outerWM)) ||
      aKidReflowInput.mFlags.mIOffsetsNeedCSSAlign ||
      aKidReflowInput.mFlags.mBOffsetsNeedCSSAlign) {
    if (-1 == aLogicalCBSize->ISize(wm)) {
      // Get the containing block width/height
      const ReflowInput* parentRI = aKidReflowInput.mParentReflowInput;
      *aLogicalCBSize =
        aKidReflowInput.ComputeContainingBlockRectangle(aPresContext,
                                                        parentRI);
    }

    const LogicalSize logicalCBSizeOuterWM = aLogicalCBSize->ConvertTo(outerWM,
                                                                       wm);

    // placeholderContainer is used in each of the m{I,B}OffsetsNeedCSSAlign
    // clauses. We declare it at this scope so we can avoid having to look
    // it up twice (and only look it up if it's needed).
    nsContainerFrame* placeholderContainer = nullptr;

    if (NS_AUTOOFFSET == aOffsets->IStart(outerWM)) {
      NS_ASSERTION(NS_AUTOOFFSET != aOffsets->IEnd(outerWM),
                   "Can't solve for both start and end");
      aOffsets->IStart(outerWM) =
        logicalCBSizeOuterWM.ISize(outerWM) -
        aOffsets->IEnd(outerWM) - aMargin.IStartEnd(outerWM) -
        aKidSize.ISize(outerWM);
    } else if (aKidReflowInput.mFlags.mIOffsetsNeedCSSAlign) {
      placeholderContainer = GetPlaceholderContainer(aKidReflowInput.mFrame);
      nscoord offset = OffsetToAlignedStaticPos(aKidReflowInput, aKidSize,
                                                logicalCBSizeOuterWM,
                                                placeholderContainer,
                                                outerWM, eLogicalAxisInline);
      // Shift IStart from its current position (at start corner of the
      // alignment container) by the returned offset.  And set IEnd to the
      // distance between the kid's end edge to containing block's end edge.
      aOffsets->IStart(outerWM) += offset;
      aOffsets->IEnd(outerWM) =
        logicalCBSizeOuterWM.ISize(outerWM) -
        (aOffsets->IStart(outerWM) + aKidSize.ISize(outerWM));
    }

    if (NS_AUTOOFFSET == aOffsets->BStart(outerWM)) {
      aOffsets->BStart(outerWM) =
        logicalCBSizeOuterWM.BSize(outerWM) -
        aOffsets->BEnd(outerWM) - aMargin.BStartEnd(outerWM) -
        aKidSize.BSize(outerWM);
    } else if (aKidReflowInput.mFlags.mBOffsetsNeedCSSAlign) {
      if (!placeholderContainer) {
        placeholderContainer = GetPlaceholderContainer(aKidReflowInput.mFrame);
      }
      nscoord offset = OffsetToAlignedStaticPos(aKidReflowInput, aKidSize,
                                                logicalCBSizeOuterWM,
                                                placeholderContainer,
                                                outerWM, eLogicalAxisBlock);
      // Shift BStart from its current position (at start corner of the
      // alignment container) by the returned offset.  And set BEnd to the
      // distance between the kid's end edge to containing block's end edge.
      aOffsets->BStart(outerWM) += offset;
      aOffsets->BEnd(outerWM) =
        logicalCBSizeOuterWM.BSize(outerWM) -
        (aOffsets->BStart(outerWM) + aKidSize.BSize(outerWM));
    }
    aKidReflowInput.SetComputedLogicalOffsets(aOffsets->ConvertTo(wm, outerWM));
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
    // the static position is determined via CSS Box Alignment within the
    // abs.pos. CB (a grid area, i.e. a piece of the grid). In this scenario,
    // due to the multiple coordinate spaces in play, we use a convenience flag
    // to simply have the child's ReflowInput give it a static position at its
    // abs.pos. CB origin, and then we'll align & offset it from there.
    nsIFrame* placeholder = aKidFrame->GetPlaceholderFrame();
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
  LogicalMargin margin =
    kidReflowInput.ComputedLogicalMargin().ConvertTo(outerWM, wm);

  // If we're doing CSS Box Alignment in either axis, that will apply the
  // margin for us in that axis (since the thing that's aligned is the margin
  // box).  So, we clear out the margin here to avoid applying it twice.
  if (kidReflowInput.mFlags.mIOffsetsNeedCSSAlign) {
    margin.IStart(outerWM) = margin.IEnd(outerWM) = 0;
  }
  if (kidReflowInput.mFlags.mBOffsetsNeedCSSAlign) {
    margin.BStart(outerWM) = margin.BEnd(outerWM) = 0;
  }

  bool constrainBSize = (aReflowInput.AvailableBSize() != NS_UNCONSTRAINEDSIZE)
    && (aFlags & AbsPosReflowFlags::eConstrainHeight)
       // Don't split if told not to (e.g. for fixed frames)
    && !aDelegatingFrame->IsInlineFrame()
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
  ResolveSizeDependentOffsets(aPresContext, kidReflowInput, kidSize, margin,
                              &offsets, &logicalCBSize);

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
