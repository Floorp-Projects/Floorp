/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * code for managing absolutely positioned children of a rendering
 * object that is a containing block for them
 */

#include "nsAbsoluteContainingBlock.h"

#include "nsAtomicContainerFrame.h"
#include "nsContainerFrame.h"
#include "nsGkAtoms.h"
#include "mozilla/CSSAlignUtils.h"
#include "mozilla/PresShell.h"
#include "mozilla/ReflowInput.h"
#include "nsPlaceholderFrame.h"
#include "nsPresContext.h"
#include "nsCSSFrameConstructor.h"
#include "nsGridContainerFrame.h"

#include "mozilla/Sprintf.h"

#ifdef DEBUG
#  include "nsBlockFrame.h"

static void PrettyUC(nscoord aSize, char* aBuf, int aBufSize) {
  if (NS_UNCONSTRAINEDSIZE == aSize) {
    strcpy(aBuf, "UC");
  } else {
    if ((int32_t)0xdeadbeef == aSize) {
      strcpy(aBuf, "deadbeef");
    } else {
      snprintf(aBuf, aBufSize, "%d", aSize);
    }
  }
}
#endif

using namespace mozilla;

typedef mozilla::CSSAlignUtils::AlignJustifyFlags AlignJustifyFlags;

void nsAbsoluteContainingBlock::SetInitialChildList(nsIFrame* aDelegatingFrame,
                                                    FrameChildListID aListID,
                                                    nsFrameList&& aChildList) {
  MOZ_ASSERT(mChildListID == aListID, "unexpected child list name");
#ifdef DEBUG
  nsIFrame::VerifyDirtyBitSet(aChildList);
  for (nsIFrame* f : aChildList) {
    MOZ_ASSERT(f->GetParent() == aDelegatingFrame, "Unexpected parent");
  }
#endif
  mAbsoluteFrames = std::move(aChildList);
}

void nsAbsoluteContainingBlock::AppendFrames(nsIFrame* aDelegatingFrame,
                                             FrameChildListID aListID,
                                             nsFrameList&& aFrameList) {
  NS_ASSERTION(mChildListID == aListID, "unexpected child list");

  // Append the frames to our list of absolutely positioned frames
#ifdef DEBUG
  nsIFrame::VerifyDirtyBitSet(aFrameList);
#endif
  mAbsoluteFrames.AppendFrames(nullptr, std::move(aFrameList));

  // no damage to intrinsic widths, since absolutely positioned frames can't
  // change them
  aDelegatingFrame->PresShell()->FrameNeedsReflow(
      aDelegatingFrame, IntrinsicDirty::None, NS_FRAME_HAS_DIRTY_CHILDREN);
}

void nsAbsoluteContainingBlock::InsertFrames(nsIFrame* aDelegatingFrame,
                                             FrameChildListID aListID,
                                             nsIFrame* aPrevFrame,
                                             nsFrameList&& aFrameList) {
  NS_ASSERTION(mChildListID == aListID, "unexpected child list");
  NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == aDelegatingFrame,
               "inserting after sibling frame with different parent");

#ifdef DEBUG
  nsIFrame::VerifyDirtyBitSet(aFrameList);
#endif
  mAbsoluteFrames.InsertFrames(nullptr, aPrevFrame, std::move(aFrameList));

  // no damage to intrinsic widths, since absolutely positioned frames can't
  // change them
  aDelegatingFrame->PresShell()->FrameNeedsReflow(
      aDelegatingFrame, IntrinsicDirty::None, NS_FRAME_HAS_DIRTY_CHILDREN);
}

void nsAbsoluteContainingBlock::RemoveFrame(FrameDestroyContext& aContext,
                                            FrameChildListID aListID,
                                            nsIFrame* aOldFrame) {
  NS_ASSERTION(mChildListID == aListID, "unexpected child list");
  if (nsIFrame* nif = aOldFrame->GetNextInFlow()) {
    nif->GetParent()->DeleteNextInFlowChild(aContext, nif, false);
  }
  mAbsoluteFrames.DestroyFrame(aContext, aOldFrame);
}

static void MaybeMarkAncestorsAsHavingDescendantDependentOnItsStaticPos(
    nsIFrame* aFrame, nsIFrame* aContainingBlockFrame) {
  MOZ_ASSERT(aFrame->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW));
  if (!aFrame->StylePosition()->NeedsHypotheticalPositionIfAbsPos()) {
    return;
  }
  // We should have set the bit when reflowing the previous continuations
  // already.
  if (aFrame->GetPrevContinuation()) {
    return;
  }

  auto* placeholder = aFrame->GetPlaceholderFrame();
  MOZ_ASSERT(placeholder);

  // Only fixed-pos frames can escape their containing block.
  if (!placeholder->HasAnyStateBits(PLACEHOLDER_FOR_FIXEDPOS)) {
    return;
  }

  for (nsIFrame* ancestor = placeholder->GetParent(); ancestor;
       ancestor = ancestor->GetParent()) {
    // Walk towards the ancestor's first continuation. That's the only one that
    // really matters, since it's the only one restyling will look at. We also
    // flag the following continuations just so it's caught on the first
    // early-return ones just to avoid walking them over and over.
    do {
      if (ancestor->DescendantMayDependOnItsStaticPosition()) {
        return;
      }
      // Moving the containing block or anything above it would move our static
      // position as well, so no need to flag it or any of its ancestors.
      if (aFrame == aContainingBlockFrame) {
        return;
      }
      ancestor->SetDescendantMayDependOnItsStaticPosition(true);
      nsIFrame* prev = ancestor->GetPrevContinuation();
      if (!prev) {
        break;
      }
      ancestor = prev;
    } while (true);
  }
}

void nsAbsoluteContainingBlock::Reflow(nsContainerFrame* aDelegatingFrame,
                                       nsPresContext* aPresContext,
                                       const ReflowInput& aReflowInput,
                                       nsReflowStatus& aReflowStatus,
                                       const nsRect& aContainingBlock,
                                       AbsPosReflowFlags aFlags,
                                       OverflowAreas* aOverflowAreas) {
  // PageContentFrame replicates fixed pos children so we really don't want
  // them contributing to overflow areas because that means we'll create new
  // pages ad infinitum if one of them overflows the page.
  if (aDelegatingFrame->IsPageContentFrame()) {
    MOZ_ASSERT(mChildListID == FrameChildListID::Fixed);
    aOverflowAreas = nullptr;
  }

  nsReflowStatus reflowStatus;
  const bool reflowAll = aReflowInput.ShouldReflowAllKids();
  const bool isGrid = !!(aFlags & AbsPosReflowFlags::IsGridContainerCB);
  nsIFrame* kidFrame;
  nsOverflowContinuationTracker tracker(aDelegatingFrame, true);
  for (kidFrame = mAbsoluteFrames.FirstChild(); kidFrame;
       kidFrame = kidFrame->GetNextSibling()) {
    bool kidNeedsReflow =
        reflowAll || kidFrame->IsSubtreeDirty() ||
        FrameDependsOnContainer(
            kidFrame, !!(aFlags & AbsPosReflowFlags::CBWidthChanged),
            !!(aFlags & AbsPosReflowFlags::CBHeightChanged));

    if (kidFrame->IsSubtreeDirty()) {
      MaybeMarkAncestorsAsHavingDescendantDependentOnItsStaticPos(
          kidFrame, aDelegatingFrame);
    }

    nscoord availBSize = aReflowInput.AvailableBSize();
    const nsRect& cb =
        isGrid ? nsGridContainerFrame::GridItemCB(kidFrame) : aContainingBlock;
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
                        kidFrame->ScrollableOverflowRectRelativeToSelf() +
                            kidFrame->GetPosition(),
                        aContainingBlock.Size())
                .BEnd(containerWM);
        NS_ASSERTION(kidOverflowBEnd >= kidBEnd,
                     "overflow area should be at least as large as frame rect");
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
      MOZ_ASSERT(!kidStatus.IsInlineBreakBefore(),
                 "ShouldAvoidBreakInside should prevent this from happening");
      nsIFrame* nextFrame = kidFrame->GetNextInFlow();
      if (!kidStatus.IsFullyComplete() &&
          aDelegatingFrame->IsFrameOfType(
              nsIFrame::eCanContainOverflowContainers)) {
        // Need a continuation
        if (!nextFrame) {
          nextFrame = aPresContext->PresShell()
                          ->FrameConstructor()
                          ->CreateContinuingFrame(kidFrame, aDelegatingFrame);
        }
        // Add it as an overflow container.
        // XXXfr This is a hack to fix some of our printing dataloss.
        // See bug 154892. Not sure how to do it "right" yet; probably want
        // to keep continuations within an nsAbsoluteContainingBlock eventually.
        tracker.Insert(nextFrame, kidStatus);
        reflowStatus.MergeCompletionStatusFrom(kidStatus);
      } else if (nextFrame) {
        // Delete any continuations
        nsOverflowContinuationTracker::AutoFinish fini(&tracker, kidFrame);
        FrameDestroyContext context(aPresContext->PresShell());
        nextFrame->GetParent()->DeleteNextInFlowChild(context, nextFrame, true);
      }
    } else {
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
      if (aDelegatingFrame->HasAnyStateBits(NS_FRAME_IS_DIRTY)) {
        kidFrame->MarkSubtreeDirty();
      } else {
        kidFrame->AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
      }
    }
  }

  // Abspos frames can't cause their parent to be incomplete,
  // only overflow incomplete.
  if (reflowStatus.IsIncomplete()) {
    reflowStatus.SetOverflowIncomplete();
    reflowStatus.SetNextInFlowNeedsReflow();
  }

  aReflowStatus.MergeCompletionStatusFrom(reflowStatus);
}

static inline bool IsFixedPaddingSize(const LengthPercentage& aCoord) {
  return aCoord.ConvertsToLength();
}
static inline bool IsFixedMarginSize(const LengthPercentageOrAuto& aCoord) {
  return aCoord.ConvertsToLength();
}
static inline bool IsFixedOffset(const LengthPercentageOrAuto& aCoord) {
  return aCoord.ConvertsToLength();
}

bool nsAbsoluteContainingBlock::FrameDependsOnContainer(nsIFrame* f,
                                                        bool aCBWidthChanged,
                                                        bool aCBHeightChanged) {
  const nsStylePosition* pos = f->StylePosition();
  // See if f's position might have changed because it depends on a
  // placeholder's position.
  if (pos->NeedsHypotheticalPositionIfAbsPos()) {
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
    //
    // FIXME(emilio): Should the BSize(wm).IsAuto() check also for the extremum
    // lengths?
    if ((pos->BSizeDependsOnContainer(wm) &&
         !(pos->BSize(wm).IsAuto() && pos->mOffset.GetBEnd(wm).IsAuto() &&
           !pos->mOffset.GetBStart(wm).IsAuto())) ||
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
    if (!IsFixedOffset(pos->mOffset.Get(eSideLeft))) {
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
        !pos->mOffset.Get(eSideRight).IsAuto()) {
      return true;
    }
  }
  if (aCBHeightChanged) {
    if (!IsFixedOffset(pos->mOffset.Get(eSideTop))) {
      return true;
    }
    // See comment above for width changes.
    if (wm.GetInlineDir() == WritingMode::eInlineBTT &&
        !pos->mOffset.Get(eSideBottom).IsAuto()) {
      return true;
    }
  }

  return false;
}

void nsAbsoluteContainingBlock::DestroyFrames(DestroyContext& aContext) {
  mAbsoluteFrames.DestroyFrames(aContext);
}

void nsAbsoluteContainingBlock::MarkSizeDependentFramesDirty() {
  DoMarkFramesDirty(false);
}

void nsAbsoluteContainingBlock::MarkAllFramesDirty() {
  DoMarkFramesDirty(true);
}

void nsAbsoluteContainingBlock::DoMarkFramesDirty(bool aMarkAllDirty) {
  for (nsIFrame* kidFrame : mAbsoluteFrames) {
    if (aMarkAllDirty) {
      kidFrame->MarkSubtreeDirty();
    } else if (FrameDependsOnContainer(kidFrame, true, true)) {
      // Add the weakest flags that will make sure we reflow this frame later
      kidFrame->AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
    }
  }
}

// Given an out-of-flow frame, this method returns the parent frame of its
// placeholder frame or null if it doesn't have a placeholder for some reason.
static nsContainerFrame* GetPlaceholderContainer(nsIFrame* aPositionedFrame) {
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
static nscoord OffsetToAlignedStaticPos(const ReflowInput& aKidReflowInput,
                                        const LogicalSize& aKidSizeInAbsPosCBWM,
                                        const LogicalSize& aAbsPosCBSize,
                                        nsContainerFrame* aPlaceholderContainer,
                                        WritingMode aAbsPosCBWM,
                                        LogicalAxis aAbsPosCBAxis) {
  if (!aPlaceholderContainer) {
    // (The placeholder container should be the thing that kicks this whole
    // process off, by setting PLACEHOLDER_STATICPOS_NEEDS_CSSALIGN.  So it
    // should exist... but bail gracefully if it doesn't.)
    NS_ERROR(
        "Missing placeholder-container when computing a "
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
  LogicalAxis pcAxis =
      (pcWM.IsOrthogonalTo(aAbsPosCBWM) ? GetOrthogonalAxis(aAbsPosCBAxis)
                                        : aAbsPosCBAxis);

  const bool placeholderContainerIsContainingBlock =
      aPlaceholderContainer == aKidReflowInput.mCBReflowInput->mFrame;

  LayoutFrameType parentType = aPlaceholderContainer->Type();
  LogicalSize alignAreaSize(pcWM);
  if (parentType == LayoutFrameType::FlexContainer) {
    // We store the frame rect in FinishAndStoreOverflow, which runs _after_
    // reflowing the absolute frames, so handle the special case of the frame
    // being the actual containing block here, by getting the size from
    // aAbsPosCBSize.
    //
    // The alignment container is the flex container's content box.
    if (placeholderContainerIsContainingBlock) {
      alignAreaSize = aAbsPosCBSize.ConvertTo(pcWM, aAbsPosCBWM);
      // aAbsPosCBSize is the padding-box, so substract the padding to get the
      // content box.
      alignAreaSize -=
          aPlaceholderContainer->GetLogicalUsedPadding(pcWM).Size(pcWM);
    } else {
      alignAreaSize = aPlaceholderContainer->GetLogicalSize(pcWM);
      LogicalMargin pcBorderPadding =
          aPlaceholderContainer->GetLogicalUsedBorderAndPadding(pcWM);
      alignAreaSize -= pcBorderPadding.Size(pcWM);
    }
  } else if (parentType == LayoutFrameType::GridContainer) {
    // This abspos elem's parent is a grid container. Per CSS Grid 10.1 & 10.2:
    //  - If the grid container *also* generates the abspos containing block (a
    // grid area) for this abspos child, we use that abspos containing block as
    // the alignment container, too. (And its size is aAbsPosCBSize.)
    //  - Otherwise, we use the grid's padding box as the alignment container.
    // https://drafts.csswg.org/css-grid/#static-position
    if (placeholderContainerIsContainingBlock) {
      // The alignment container is the grid area that we're using as the
      // absolute containing block.
      alignAreaSize = aAbsPosCBSize.ConvertTo(pcWM, aAbsPosCBWM);
    } else {
      // The alignment container is a the grid container's content box (which
      // we can get by subtracting away its border & padding from frame's size):
      alignAreaSize = aPlaceholderContainer->GetLogicalSize(pcWM);
      LogicalMargin pcBorderPadding =
          aPlaceholderContainer->GetLogicalUsedBorderAndPadding(pcWM);
      alignAreaSize -= pcBorderPadding.Size(pcWM);
    }
  } else {
    NS_ERROR("Unsupported container for abpsos CSS Box Alignment");
    return 0;  // (leave the child at the start of its alignment container)
  }

  nscoord alignAreaSizeInAxis = (pcAxis == eLogicalAxisInline)
                                    ? alignAreaSize.ISize(pcWM)
                                    : alignAreaSize.BSize(pcWM);

  AlignJustifyFlags flags = AlignJustifyFlags::IgnoreAutoMargins;
  StyleAlignFlags alignConst =
      aPlaceholderContainer->CSSAlignmentForAbsPosChild(aKidReflowInput,
                                                        pcAxis);
  // If the safe bit in alignConst is set, set the safe flag in |flags|.
  // Note: If no <overflow-position> is specified, we behave as 'unsafe'.
  // This doesn't quite match the css-align spec, which has an [at-risk]
  // "smart default" behavior with some extra nuance about scroll containers.
  if (alignConst & StyleAlignFlags::SAFE) {
    flags |= AlignJustifyFlags::OverflowSafe;
  }
  alignConst &= ~StyleAlignFlags::FLAG_BITS;

  // Find out if placeholder-container & the OOF child have the same start-sides
  // in the placeholder-container's pcAxis.
  WritingMode kidWM = aKidReflowInput.GetWritingMode();
  if (pcWM.ParallelAxisStartsOnSameSide(pcAxis, kidWM)) {
    flags |= AlignJustifyFlags::SameSide;
  }

  // (baselineAdjust is unused. CSSAlignmentForAbsPosChild() should've
  // converted 'baseline'/'last baseline' enums to their fallback values.)
  const nscoord baselineAdjust = nscoord(0);

  // AlignJustifySelf operates in the kid's writing mode, so we need to
  // represent the child's size and the desired axis in that writing mode:
  LogicalSize kidSizeInOwnWM =
      aKidSizeInAbsPosCBWM.ConvertTo(kidWM, aAbsPosCBWM);
  LogicalAxis kidAxis =
      (kidWM.IsOrthogonalTo(aAbsPosCBWM) ? GetOrthogonalAxis(aAbsPosCBAxis)
                                         : aAbsPosCBAxis);

  nscoord offset = CSSAlignUtils::AlignJustifySelf(
      alignConst, kidAxis, flags, baselineAdjust, alignAreaSizeInAxis,
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

void nsAbsoluteContainingBlock::ResolveSizeDependentOffsets(
    nsPresContext* aPresContext, ReflowInput& aKidReflowInput,
    const LogicalSize& aKidSize, const LogicalMargin& aMargin,
    LogicalMargin* aOffsets, LogicalSize* aLogicalCBSize) {
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
      *aLogicalCBSize = aKidReflowInput.ComputeContainingBlockRectangle(
          aPresContext, parentRI);
    }

    const LogicalSize logicalCBSizeOuterWM =
        aLogicalCBSize->ConvertTo(outerWM, wm);

    // placeholderContainer is used in each of the m{I,B}OffsetsNeedCSSAlign
    // clauses. We declare it at this scope so we can avoid having to look
    // it up twice (and only look it up if it's needed).
    nsContainerFrame* placeholderContainer = nullptr;

    if (NS_AUTOOFFSET == aOffsets->IStart(outerWM)) {
      NS_ASSERTION(NS_AUTOOFFSET != aOffsets->IEnd(outerWM),
                   "Can't solve for both start and end");
      aOffsets->IStart(outerWM) =
          logicalCBSizeOuterWM.ISize(outerWM) - aOffsets->IEnd(outerWM) -
          aMargin.IStartEnd(outerWM) - aKidSize.ISize(outerWM);
    } else if (aKidReflowInput.mFlags.mIOffsetsNeedCSSAlign) {
      placeholderContainer = GetPlaceholderContainer(aKidReflowInput.mFrame);
      nscoord offset = OffsetToAlignedStaticPos(
          aKidReflowInput, aKidSize, logicalCBSizeOuterWM, placeholderContainer,
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
          logicalCBSizeOuterWM.BSize(outerWM) - aOffsets->BEnd(outerWM) -
          aMargin.BStartEnd(outerWM) - aKidSize.BSize(outerWM);
    } else if (aKidReflowInput.mFlags.mBOffsetsNeedCSSAlign) {
      if (!placeholderContainer) {
        placeholderContainer = GetPlaceholderContainer(aKidReflowInput.mFrame);
      }
      nscoord offset = OffsetToAlignedStaticPos(
          aKidReflowInput, aKidSize, logicalCBSizeOuterWM, placeholderContainer,
          outerWM, eLogicalAxisBlock);
      // Shift BStart from its current position (at start corner of the
      // alignment container) by the returned offset.  And set BEnd to the
      // distance between the kid's end edge to containing block's end edge.
      aOffsets->BStart(outerWM) += offset;
      aOffsets->BEnd(outerWM) =
          logicalCBSizeOuterWM.BSize(outerWM) -
          (aOffsets->BStart(outerWM) + aKidSize.BSize(outerWM));
    }
    aKidReflowInput.SetComputedLogicalOffsets(outerWM, *aOffsets);
  }
}

void nsAbsoluteContainingBlock::ResolveAutoMarginsAfterLayout(
    ReflowInput& aKidReflowInput, const LogicalSize* aLogicalCBSize,
    const LogicalSize& aKidSize, LogicalMargin& aMargin,
    LogicalMargin& aOffsets) {
  MOZ_ASSERT(aKidReflowInput.mFrame->HasIntrinsicKeywordForBSize());

  WritingMode wm = aKidReflowInput.GetWritingMode();
  WritingMode outerWM = aKidReflowInput.mParentReflowInput->GetWritingMode();

  const LogicalSize kidSizeInWM = aKidSize.ConvertTo(wm, outerWM);
  LogicalMargin marginInWM = aMargin.ConvertTo(wm, outerWM);
  LogicalMargin offsetsInWM = aOffsets.ConvertTo(wm, outerWM);

  // No need to substract border sizes because aKidSize has it included
  // already. Also, if any offset is auto, the auto margin resolves to zero.
  // https://drafts.csswg.org/css-position-3/#abspos-margins
  const bool autoOffset = offsetsInWM.BEnd(wm) == NS_AUTOOFFSET ||
                          offsetsInWM.BStart(wm) == NS_AUTOOFFSET;
  nscoord availMarginSpace =
      autoOffset ? 0
                 : aLogicalCBSize->BSize(wm) - kidSizeInWM.BSize(wm) -
                       offsetsInWM.BStartEnd(wm) - marginInWM.BStartEnd(wm);

  const auto& styleMargin = aKidReflowInput.mStyleMargin;
  if (wm.IsOrthogonalTo(outerWM)) {
    ReflowInput::ComputeAbsPosInlineAutoMargin(
        availMarginSpace, outerWM,
        styleMargin->mMargin.GetIStart(outerWM).IsAuto(),
        styleMargin->mMargin.GetIEnd(outerWM).IsAuto(), aMargin, aOffsets);
  } else {
    ReflowInput::ComputeAbsPosBlockAutoMargin(
        availMarginSpace, outerWM,
        styleMargin->mMargin.GetBStart(outerWM).IsAuto(),
        styleMargin->mMargin.GetBEnd(outerWM).IsAuto(), aMargin, aOffsets);
  }

  aKidReflowInput.SetComputedLogicalMargin(outerWM, aMargin);
  aKidReflowInput.SetComputedLogicalOffsets(outerWM, aOffsets);

  nsMargin* propValue =
      aKidReflowInput.mFrame->GetProperty(nsIFrame::UsedMarginProperty());
  // InitOffsets should've created a UsedMarginProperty for us, if any margin is
  // auto.
  MOZ_ASSERT_IF(styleMargin->HasInlineAxisAuto(outerWM) ||
                    styleMargin->HasBlockAxisAuto(outerWM),
                propValue);
  if (propValue) {
    *propValue = aMargin.GetPhysicalMargin(outerWM);
  }
}

// XXX Optimize the case where it's a resize reflow and the absolutely
// positioned child has the exact same size and position and skip the
// reflow...

// When bug 154892 is checked in, make sure that when
// mChildListID == FrameChildListID::Fixed, the height is unconstrained.
// since we don't allow replicated frames to split.

void nsAbsoluteContainingBlock::ReflowAbsoluteFrame(
    nsIFrame* aDelegatingFrame, nsPresContext* aPresContext,
    const ReflowInput& aReflowInput, const nsRect& aContainingBlock,
    AbsPosReflowFlags aFlags, nsIFrame* aKidFrame, nsReflowStatus& aStatus,
    OverflowAreas* aOverflowAreas) {
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsIFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent);
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
#endif  // DEBUG

  WritingMode wm = aKidFrame->GetWritingMode();
  LogicalSize logicalCBSize(wm, aContainingBlock.Size());
  nscoord availISize = logicalCBSize.ISize(wm);
  if (availISize == -1) {
    NS_ASSERTION(
        aReflowInput.ComputedSize(wm).ISize(wm) != NS_UNCONSTRAINEDSIZE,
        "Must have a useful inline-size _somewhere_");
    availISize = aReflowInput.ComputedSizeWithPadding(wm).ISize(wm);
  }

  ReflowInput::InitFlags initFlags;
  if (aFlags & AbsPosReflowFlags::IsGridContainerCB) {
    // When a grid container generates the abs.pos. CB for a *child* then
    // the static position is determined via CSS Box Alignment within the
    // abs.pos. CB (a grid area, i.e. a piece of the grid). In this scenario,
    // due to the multiple coordinate spaces in play, we use a convenience flag
    // to simply have the child's ReflowInput give it a static position at its
    // abs.pos. CB origin, and then we'll align & offset it from there.
    nsIFrame* placeholder = aKidFrame->GetPlaceholderFrame();
    if (placeholder && placeholder->GetParent() == aDelegatingFrame) {
      initFlags += ReflowInput::InitFlag::StaticPosIsCBOrigin;
    }
  }

  bool constrainBSize =
      (aReflowInput.AvailableBSize() != NS_UNCONSTRAINEDSIZE) &&

      // Don't split if told not to (e.g. for fixed frames)
      (aFlags & AbsPosReflowFlags::ConstrainHeight) &&

      // XXX we don't handle splitting frames for inline absolute containing
      // blocks yet
      !aDelegatingFrame->IsInlineFrame() &&

      // Bug 1588623: Support splitting absolute positioned multicol containers.
      !aKidFrame->IsColumnSetWrapperFrame() &&

      // Don't split things below the fold. (Ideally we shouldn't *have*
      // anything totally below the fold, but we can't position frames
      // across next-in-flow breaks yet.
      (aKidFrame->GetLogicalRect(aContainingBlock.Size()).BStart(wm) <=
       aReflowInput.AvailableBSize());

  // Get the border values
  const WritingMode outerWM = aReflowInput.GetWritingMode();
  const LogicalMargin border = aDelegatingFrame->GetLogicalUsedBorder(outerWM);

  const nscoord availBSize = constrainBSize
                                 ? aReflowInput.AvailableBSize() -
                                       border.ConvertTo(wm, outerWM).BStart(wm)
                                 : NS_UNCONSTRAINEDSIZE;

  ReflowInput kidReflowInput(aPresContext, aReflowInput, aKidFrame,
                             LogicalSize(wm, availISize, availBSize),
                             Some(logicalCBSize), initFlags);

  if (nscoord kidAvailBSize = kidReflowInput.AvailableBSize();
      kidAvailBSize != NS_UNCONSTRAINEDSIZE) {
    // Shrink available block-size if it's constrained.
    kidAvailBSize -= kidReflowInput.ComputedLogicalMargin(wm).BStart(wm);
    const nscoord kidOffsetBStart =
        kidReflowInput.ComputedLogicalOffsets(wm).BStart(wm);
    if (NS_AUTOOFFSET != kidOffsetBStart) {
      kidAvailBSize -= kidOffsetBStart;
    }
    kidReflowInput.SetAvailableBSize(kidAvailBSize);
  }

  // Do the reflow
  ReflowOutput kidDesiredSize(kidReflowInput);
  aKidFrame->Reflow(aPresContext, kidDesiredSize, kidReflowInput, aStatus);

  // Position the child relative to our padding edge. Don't do this for popups,
  // which handle their own positioning.
  if (!aKidFrame->IsMenuPopupFrame()) {
    const LogicalSize kidSize = kidDesiredSize.Size(outerWM);

    LogicalMargin offsets = kidReflowInput.ComputedLogicalOffsets(outerWM);
    LogicalMargin margin = kidReflowInput.ComputedLogicalMargin(outerWM);

    // If we're doing CSS Box Alignment in either axis, that will apply the
    // margin for us in that axis (since the thing that's aligned is the margin
    // box).  So, we clear out the margin here to avoid applying it twice.
    if (kidReflowInput.mFlags.mIOffsetsNeedCSSAlign) {
      margin.IStart(outerWM) = margin.IEnd(outerWM) = 0;
    }
    if (kidReflowInput.mFlags.mBOffsetsNeedCSSAlign) {
      margin.BStart(outerWM) = margin.BEnd(outerWM) = 0;
    }

    // If we're solving for start in either inline or block direction,
    // then compute it now that we know the dimensions.
    ResolveSizeDependentOffsets(aPresContext, kidReflowInput, kidSize, margin,
                                &offsets, &logicalCBSize);

    if (kidReflowInput.mFrame->HasIntrinsicKeywordForBSize()) {
      ResolveAutoMarginsAfterLayout(kidReflowInput, &logicalCBSize, kidSize,
                                    margin, offsets);
    }

    LogicalRect rect(outerWM,
                     border.StartOffset(outerWM) +
                         offsets.StartOffset(outerWM) +
                         margin.StartOffset(outerWM),
                     kidSize);
    nsRect r = rect.GetPhysicalRect(
        outerWM, logicalCBSize.GetPhysicalSize(wm) +
                     border.Size(outerWM).GetPhysicalSize(outerWM));

    // Offset the frame rect by the given origin of the absolute containing
    // block.
    r.x += aContainingBlock.x;
    r.y += aContainingBlock.y;

    aKidFrame->SetRect(r);

    nsView* view = aKidFrame->GetView();
    if (view) {
      // Size and position the view and set its opacity, visibility, content
      // transparency, and clip
      nsContainerFrame::SyncFrameViewAfterReflow(aPresContext, aKidFrame, view,
                                                 kidDesiredSize.InkOverflow());
    } else {
      nsContainerFrame::PositionChildViews(aKidFrame);
    }
  }

  aKidFrame->DidReflow(aPresContext, &kidReflowInput);

  const nsRect r = aKidFrame->GetRect();
#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsIFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent - 1);
    printf("abs pos ");
    nsAutoString name;
    aKidFrame->GetFrameName(name);
    printf("%s ", NS_LossyConvertUTF16toASCII(name).get());
    printf("%p rect=%d,%d,%d,%d\n", static_cast<void*>(aKidFrame), r.x, r.y,
           r.width, r.height);
  }
#endif

  if (aOverflowAreas) {
    aOverflowAreas->UnionWith(kidDesiredSize.mOverflowAreas + r.TopLeft());
  }
}
