/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS display:inline objects */

#include "nsInlineFrame.h"

#include "gfxContext.h"
#include "mozilla/ComputedStyle.h"
#include "mozilla/Likely.h"
#include "mozilla/PresShell.h"
#include "mozilla/RestyleManager.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/SVGTextFrame.h"
#include "mozilla/SVGUtils.h"
#include "nsLineLayout.h"
#include "nsBlockFrame.h"
#include "nsLayoutUtils.h"
#include "nsPlaceholderFrame.h"
#include "nsGkAtoms.h"
#include "nsPresContext.h"
#include "nsPresContextInlines.h"
#include "nsCSSAnonBoxes.h"
#include "nsDisplayList.h"
#include "nsStyleChangeList.h"

#ifdef DEBUG
#  undef NOISY_PUSHING
#endif

using namespace mozilla;
using namespace mozilla::layout;

//////////////////////////////////////////////////////////////////////

// Basic nsInlineFrame methods

nsInlineFrame* NS_NewInlineFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell) nsInlineFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsInlineFrame)

NS_QUERYFRAME_HEAD(nsInlineFrame)
  NS_QUERYFRAME_ENTRY(nsInlineFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

#ifdef DEBUG_FRAME_DUMP
nsresult nsInlineFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(u"Inline"_ns, aResult);
}
#endif

void nsInlineFrame::InvalidateFrame(uint32_t aDisplayItemKey,
                                    bool aRebuildDisplayItems) {
  if (SVGUtils::IsInSVGTextSubtree(this)) {
    nsIFrame* svgTextFrame = nsLayoutUtils::GetClosestFrameOfType(
        GetParent(), LayoutFrameType::SVGText);
    svgTextFrame->InvalidateFrame();
    return;
  }
  nsContainerFrame::InvalidateFrame(aDisplayItemKey, aRebuildDisplayItems);
}

void nsInlineFrame::InvalidateFrameWithRect(const nsRect& aRect,
                                            uint32_t aDisplayItemKey,
                                            bool aRebuildDisplayItems) {
  if (SVGUtils::IsInSVGTextSubtree(this)) {
    nsIFrame* svgTextFrame = nsLayoutUtils::GetClosestFrameOfType(
        GetParent(), LayoutFrameType::SVGText);
    svgTextFrame->InvalidateFrame();
    return;
  }
  nsContainerFrame::InvalidateFrameWithRect(aRect, aDisplayItemKey,
                                            aRebuildDisplayItems);
}

static inline bool IsMarginZero(const LengthPercentageOrAuto& aLength) {
  return aLength.IsAuto() ||
         nsLayoutUtils::IsMarginZero(aLength.AsLengthPercentage());
}

/* virtual */
bool nsInlineFrame::IsSelfEmpty() {
#if 0
  // I used to think inline frames worked this way, but it seems they
  // don't.  At least not in our codebase.
  if (GetPresContext()->CompatibilityMode() == eCompatibility_FullStandards) {
    return false;
  }
#endif
  const nsStyleMargin* margin = StyleMargin();
  const nsStyleBorder* border = StyleBorder();
  const nsStylePadding* padding = StylePadding();
  // Block-start and -end ignored, since they shouldn't affect things, but this
  // doesn't really match with nsLineLayout.cpp's setting of
  // ZeroEffectiveSpanBox, anymore, so what should this really be?
  WritingMode wm = GetWritingMode();
  bool haveStart, haveEnd;

  auto HaveSide = [&](mozilla::Side aSide) -> bool {
    return border->GetComputedBorderWidth(aSide) != 0 ||
           !nsLayoutUtils::IsPaddingZero(padding->mPadding.Get(aSide)) ||
           !IsMarginZero(margin->mMargin.Get(aSide));
  };
  // Initially set up haveStart and haveEnd in terms of visual (LTR/TTB)
  // coordinates; we'll exchange them later if bidi-RTL is in effect to
  // get logical start and end flags.
  if (wm.IsVertical()) {
    haveStart = HaveSide(eSideTop);
    haveEnd = HaveSide(eSideBottom);
  } else {
    haveStart = HaveSide(eSideLeft);
    haveEnd = HaveSide(eSideRight);
  }
  if (haveStart || haveEnd) {
    // We skip this block and return false for box-decoration-break:clone since
    // in that case all the continuations will have the border/padding/margin.
    if (HasAnyStateBits(NS_FRAME_PART_OF_IBSPLIT) &&
        StyleBorder()->mBoxDecorationBreak == StyleBoxDecorationBreak::Slice) {
      // When direction=rtl, we need to consider logical rather than visual
      // start and end, so swap the flags.
      if (wm.IsBidiRTL()) {
        std::swap(haveStart, haveEnd);
      }
      // For ib-split frames, ignore things we know we'll skip in GetSkipSides.
      // XXXbz should we be doing this for non-ib-split frames too, in a more
      // general way?

      // Get the first continuation eagerly, as a performance optimization, to
      // avoid having to get it twice..
      nsIFrame* firstCont = FirstContinuation();
      return (!haveStart || firstCont->FrameIsNonFirstInIBSplit()) &&
             (!haveEnd || firstCont->FrameIsNonLastInIBSplit());
    }
    return false;
  }
  return true;
}

bool nsInlineFrame::IsEmpty() {
  if (!IsSelfEmpty()) {
    return false;
  }

  for (nsIFrame* kid : mFrames) {
    if (!kid->IsEmpty()) return false;
  }

  return true;
}

nsIFrame::FrameSearchResult nsInlineFrame::PeekOffsetCharacter(
    bool aForward, int32_t* aOffset, PeekOffsetCharacterOptions aOptions) {
  // Override the implementation in nsFrame, to skip empty inline frames
  NS_ASSERTION(aOffset && *aOffset <= 1, "aOffset out of range");
  int32_t startOffset = *aOffset;
  if (startOffset < 0) startOffset = 1;
  if (aForward == (startOffset == 0)) {
    // We're before the frame and moving forward, or after it and moving
    // backwards: skip to the other side, but keep going.
    *aOffset = 1 - startOffset;
  }
  return CONTINUE;
}

void nsInlineFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                PostDestroyData& aPostDestroyData) {
  nsFrameList* overflowFrames = GetOverflowFrames();
  if (overflowFrames) {
    // Fixup the parent pointers for any child frames on the OverflowList.
    // nsIFrame::DestroyFrom depends on that to find the sticky scroll
    // container (an ancestor).
    overflowFrames->ApplySetParent(this);
  }
  nsContainerFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

void nsInlineFrame::StealFrame(nsIFrame* aChild) {
  if (MaybeStealOverflowContainerFrame(aChild)) {
    return;
  }

  nsInlineFrame* parent = this;
  do {
    if (parent->mFrames.StartRemoveFrame(aChild)) {
      return;
    }

    // We didn't find the child in our principal child list.
    // Maybe it's on the overflow list?
    nsFrameList* frameList = parent->GetOverflowFrames();
    if (frameList && frameList->ContinueRemoveFrame(aChild)) {
      if (frameList->IsEmpty()) {
        parent->DestroyOverflowList();
      }
      return;
    }

    // Due to our "lazy reparenting" optimization 'aChild' might not actually
    // be on any of our child lists, but instead in one of our next-in-flows.
    parent = static_cast<nsInlineFrame*>(parent->GetNextInFlow());
  } while (parent);

  MOZ_ASSERT_UNREACHABLE("nsInlineFrame::StealFrame: can't find aChild");
}

void nsInlineFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                     const nsDisplayListSet& aLists) {
  BuildDisplayListForInline(aBuilder, aLists);

  // The sole purpose of this is to trigger display of the selection
  // window for Named Anchors, which don't have any children and
  // normally don't have any size, but in Editor we use CSS to display
  // an image to represent this "hidden" element.
  if (!mFrames.FirstChild()) {
    DisplaySelectionOverlay(aBuilder, aLists.Content());
  }
}

//////////////////////////////////////////////////////////////////////
// Reflow methods

/* virtual */
void nsInlineFrame::AddInlineMinISize(gfxContext* aRenderingContext,
                                      nsIFrame::InlineMinISizeData* aData) {
  DoInlineMinISize(aRenderingContext, aData);
}

/* virtual */
void nsInlineFrame::AddInlinePrefISize(gfxContext* aRenderingContext,
                                       nsIFrame::InlinePrefISizeData* aData) {
  DoInlinePrefISize(aRenderingContext, aData);
}

/* virtual */
nsIFrame::SizeComputationResult nsInlineFrame::ComputeSize(
    gfxContext* aRenderingContext, WritingMode aWM, const LogicalSize& aCBSize,
    nscoord aAvailableISize, const LogicalSize& aMargin,
    const LogicalSize& aBorderPadding, const StyleSizeOverrides& aSizeOverrides,
    ComputeSizeFlags aFlags) {
  // Inlines and text don't compute size before reflow.
  return {LogicalSize(aWM, NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE),
          AspectRatioUsage::None};
}

nsRect nsInlineFrame::ComputeTightBounds(DrawTarget* aDrawTarget) const {
  // be conservative
  if (Style()->HasTextDecorationLines()) {
    return InkOverflowRect();
  }
  return ComputeSimpleTightBounds(aDrawTarget);
}

static void ReparentChildListStyle(nsPresContext* aPresContext,
                                   const nsFrameList::Slice& aFrames,
                                   nsIFrame* aParentFrame) {
  RestyleManager* restyleManager = aPresContext->RestyleManager();

  for (nsFrameList::Enumerator e(aFrames); !e.AtEnd(); e.Next()) {
    NS_ASSERTION(e.get()->GetParent() == aParentFrame, "Bogus parentage");
    restyleManager->ReparentComputedStyleForFirstLine(e.get());
    nsLayoutUtils::MarkDescendantsDirty(e.get());
  }
}

void nsInlineFrame::Reflow(nsPresContext* aPresContext, ReflowOutput& aMetrics,
                           const ReflowInput& aReflowInput,
                           nsReflowStatus& aStatus) {
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsInlineFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aMetrics, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  if (nullptr == aReflowInput.mLineLayout) {
    NS_ERROR("must have non-null aReflowInput.mLineLayout");
    return;
  }
  if (IsFrameTreeTooDeep(aReflowInput, aMetrics, aStatus)) {
    return;
  }

  bool lazilySetParentPointer = false;

  // Check for an overflow list with our prev-in-flow
  nsInlineFrame* prevInFlow = (nsInlineFrame*)GetPrevInFlow();
  if (prevInFlow) {
    AutoFrameListPtr prevOverflowFrames(aPresContext,
                                        prevInFlow->StealOverflowFrames());
    if (prevOverflowFrames) {
      // When pushing and pulling frames we need to check for whether any
      // views need to be reparented.
      nsContainerFrame::ReparentFrameViewList(*prevOverflowFrames, prevInFlow,
                                              this);

      // Check if we should do the lazilySetParentPointer optimization.
      // Only do it in simple cases where we're being reflowed for the
      // first time, nothing (e.g. bidi resolution) has already given
      // us children, and there's no next-in-flow, so all our frames
      // will be taken from prevOverflowFrames.
      if (HasAnyStateBits(NS_FRAME_FIRST_REFLOW) && mFrames.IsEmpty() &&
          !GetNextInFlow()) {
        // If our child list is empty, just put the new frames into it.
        // Note that we don't set the parent pointer for the new frames. Instead
        // wait to do this until we actually reflow the frame. If the overflow
        // list contains thousands of frames this is a big performance issue
        // (see bug #5588)
        mFrames.SetFrames(*prevOverflowFrames);
        lazilySetParentPointer = true;
      } else {
        // Insert the new frames at the beginning of the child list
        // and set their parent pointer
        const nsFrameList::Slice& newFrames =
            mFrames.InsertFrames(this, nullptr, *prevOverflowFrames);
        // If our prev in flow was under the first continuation of a first-line
        // frame then we need to reparent the ComputedStyles to remove the
        // the special first-line styling. In the lazilySetParentPointer case
        // we reparent the ComputedStyles when we set their parents in
        // nsInlineFrame::ReflowFrames and nsInlineFrame::ReflowInlineFrame.
        if (aReflowInput.mLineLayout->GetInFirstLine()) {
          ReparentChildListStyle(aPresContext, newFrames, this);
        }
      }
    }
  }

  // It's also possible that we have an overflow list for ourselves
#ifdef DEBUG
  if (HasAnyStateBits(NS_FRAME_FIRST_REFLOW)) {
    // If it's our initial reflow, then we should not have an overflow list.
    // However, add an assertion in case we get reflowed more than once with
    // the initial reflow reason
    nsFrameList* overflowFrames = GetOverflowFrames();
    NS_ASSERTION(!overflowFrames || overflowFrames->IsEmpty(),
                 "overflow list is not empty for initial reflow");
  }
#endif
  if (!HasAnyStateBits(NS_FRAME_FIRST_REFLOW)) {
    DrainSelfOverflowListInternal(aReflowInput.mLineLayout->GetInFirstLine());
  }

  // Set our own reflow input (additional state above and beyond aReflowInput).
  InlineReflowInput irs;
  irs.mPrevFrame = nullptr;
  irs.mLineContainer = aReflowInput.mLineLayout->LineContainerFrame();
  irs.mLineLayout = aReflowInput.mLineLayout;
  irs.mNextInFlow = (nsInlineFrame*)GetNextInFlow();
  irs.mSetParentPointer = lazilySetParentPointer;

  if (mFrames.IsEmpty()) {
    // Try to pull over one frame before starting so that we know
    // whether we have an anonymous block or not.
    Unused << PullOneFrame(aPresContext, irs);
  }

  ReflowFrames(aPresContext, aReflowInput, irs, aMetrics, aStatus);

  ReflowAbsoluteFrames(aPresContext, aMetrics, aReflowInput, aStatus);

  // Note: the line layout code will properly compute our
  // overflow-rect state for us.

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aMetrics);
}

nsresult nsInlineFrame::AttributeChanged(int32_t aNameSpaceID,
                                         nsAtom* aAttribute, int32_t aModType) {
  nsresult rv =
      nsContainerFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);

  if (NS_FAILED(rv)) {
    return rv;
  }

  if (SVGUtils::IsInSVGTextSubtree(this)) {
    SVGTextFrame* f = static_cast<SVGTextFrame*>(
        nsLayoutUtils::GetClosestFrameOfType(this, LayoutFrameType::SVGText));
    f->HandleAttributeChangeInDescendant(mContent->AsElement(), aNameSpaceID,
                                         aAttribute);
  }

  return NS_OK;
}

bool nsInlineFrame::DrainSelfOverflowListInternal(bool aInFirstLine) {
  AutoFrameListPtr overflowFrames(PresContext(), StealOverflowFrames());
  if (!overflowFrames || overflowFrames->IsEmpty()) {
    return false;
  }

  // The frames on our own overflowlist may have been pushed by a
  // previous lazilySetParentPointer Reflow so we need to ensure the
  // correct parent pointer.  This is sometimes skipped by Reflow.
  nsIFrame* firstChild = overflowFrames->FirstChild();
  RestyleManager* restyleManager = PresContext()->RestyleManager();
  for (nsIFrame* f = firstChild; f; f = f->GetNextSibling()) {
    f->SetParent(this);
    if (MOZ_UNLIKELY(aInFirstLine)) {
      restyleManager->ReparentComputedStyleForFirstLine(f);
      nsLayoutUtils::MarkDescendantsDirty(f);
    }
  }
  mFrames.AppendFrames(nullptr, *overflowFrames);
  return true;
}

/* virtual */
bool nsInlineFrame::DrainSelfOverflowList() {
  nsIFrame* lineContainer = nsLayoutUtils::FindNearestBlockAncestor(this);
  // Add the eInFirstLine flag if we have a ::first-line ancestor frame.
  // No need to look further than the nearest line container though.
  bool inFirstLine = false;
  for (nsIFrame* p = GetParent(); p != lineContainer; p = p->GetParent()) {
    if (p->IsLineFrame()) {
      inFirstLine = true;
      break;
    }
  }
  return DrainSelfOverflowListInternal(inFirstLine);
}

/* virtual */
bool nsInlineFrame::CanContinueTextRun() const {
  // We can continue a text run through an inline frame
  return true;
}

/* virtual */
void nsInlineFrame::PullOverflowsFromPrevInFlow() {
  nsInlineFrame* prevInFlow = static_cast<nsInlineFrame*>(GetPrevInFlow());
  if (prevInFlow) {
    nsPresContext* presContext = PresContext();
    AutoFrameListPtr prevOverflowFrames(presContext,
                                        prevInFlow->StealOverflowFrames());
    if (prevOverflowFrames) {
      // Assume that our prev-in-flow has the same line container that we do.
      nsContainerFrame::ReparentFrameViewList(*prevOverflowFrames, prevInFlow,
                                              this);
      mFrames.InsertFrames(this, nullptr, *prevOverflowFrames);
    }
  }
}

void nsInlineFrame::ReflowFrames(nsPresContext* aPresContext,
                                 const ReflowInput& aReflowInput,
                                 InlineReflowInput& irs, ReflowOutput& aMetrics,
                                 nsReflowStatus& aStatus) {
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  nsLineLayout* lineLayout = aReflowInput.mLineLayout;
  bool inFirstLine = aReflowInput.mLineLayout->GetInFirstLine();
  RestyleManager* restyleManager = aPresContext->RestyleManager();
  WritingMode frameWM = aReflowInput.GetWritingMode();
  WritingMode lineWM = aReflowInput.mLineLayout->mRootSpan->mWritingMode;
  LogicalMargin framePadding =
      aReflowInput.ComputedLogicalBorderPadding(frameWM);
  nscoord startEdge = 0;
  const bool boxDecorationBreakClone = MOZ_UNLIKELY(
      StyleBorder()->mBoxDecorationBreak == StyleBoxDecorationBreak::Clone);
  // Don't offset by our start borderpadding if we have a prev continuation or
  // if we're in a part of an {ib} split other than the first one. For
  // box-decoration-break:clone we always offset our start since all
  // continuations have border/padding.
  if ((!GetPrevContinuation() && !FrameIsNonFirstInIBSplit()) ||
      boxDecorationBreakClone) {
    startEdge = framePadding.IStart(frameWM);
  }
  nscoord availableISize = aReflowInput.AvailableISize();
  NS_ASSERTION(availableISize != NS_UNCONSTRAINEDSIZE,
               "should no longer use available widths");
  // Subtract off inline axis border+padding from availableISize
  availableISize -= startEdge;
  availableISize -= framePadding.IEnd(frameWM);
  lineLayout->BeginSpan(this, &aReflowInput, startEdge,
                        startEdge + availableISize, &mBaseline);

  // First reflow our principal children.
  nsIFrame* frame = mFrames.FirstChild();
  bool done = false;
  while (frame) {
    // Check if we should lazily set the child frame's parent pointer.
    if (irs.mSetParentPointer) {
      nsIFrame* child = frame;
      do {
        child->SetParent(this);
        if (inFirstLine) {
          restyleManager->ReparentComputedStyleForFirstLine(child);
          nsLayoutUtils::MarkDescendantsDirty(child);
        }
        // We also need to do the same for |frame|'s next-in-flows that are in
        // the sibling list. Otherwise, if we reflow |frame| and it's complete
        // we'll crash when trying to delete its next-in-flow.
        // This scenario doesn't happen often, but it can happen.
        nsIFrame* nextSibling = child->GetNextSibling();
        child = child->GetNextInFlow();
        if (MOZ_UNLIKELY(child)) {
          while (child != nextSibling && nextSibling) {
            nextSibling = nextSibling->GetNextSibling();
          }
          if (!nextSibling) {
            child = nullptr;
          }
        }
        MOZ_ASSERT(!child || mFrames.ContainsFrame(child));
      } while (child);

      // Fix the parent pointer for ::first-letter child frame next-in-flows,
      // so nsFirstLetterFrame::Reflow can destroy them safely (bug 401042).
      nsIFrame* realFrame = nsPlaceholderFrame::GetRealFrameFor(frame);
      if (realFrame->IsLetterFrame()) {
        nsIFrame* child = realFrame->PrincipalChildList().FirstChild();
        if (child) {
          NS_ASSERTION(child->IsTextFrame(), "unexpected frame type");
          nsIFrame* nextInFlow = child->GetNextInFlow();
          for (; nextInFlow; nextInFlow = nextInFlow->GetNextInFlow()) {
            NS_ASSERTION(nextInFlow->IsTextFrame(), "unexpected frame type");
            if (mFrames.ContainsFrame(nextInFlow)) {
              nextInFlow->SetParent(this);
              if (inFirstLine) {
                restyleManager->ReparentComputedStyleForFirstLine(nextInFlow);
                nsLayoutUtils::MarkDescendantsDirty(nextInFlow);
              }
            } else {
#ifdef DEBUG
              // Once we find a next-in-flow that isn't ours none of the
              // remaining next-in-flows should be either.
              for (; nextInFlow; nextInFlow = nextInFlow->GetNextInFlow()) {
                NS_ASSERTION(!mFrames.ContainsFrame(nextInFlow),
                             "unexpected letter frame flow");
              }
#endif
              break;
            }
          }
        }
      }
    }
    MOZ_ASSERT(frame->GetParent() == this);

    if (!done) {
      bool reflowingFirstLetter = lineLayout->GetFirstLetterStyleOK();
      ReflowInlineFrame(aPresContext, aReflowInput, irs, frame, aStatus);
      done = aStatus.IsInlineBreak() ||
             (!reflowingFirstLetter && aStatus.IsIncomplete());
      if (done) {
        if (!irs.mSetParentPointer) {
          break;
        }
        // Keep reparenting the remaining siblings, but don't reflow them.
        nsFrameList* pushedFrames = GetOverflowFrames();
        if (pushedFrames && pushedFrames->FirstChild() == frame) {
          // Don't bother if |frame| was pushed to our overflow list.
          break;
        }
      } else {
        irs.mPrevFrame = frame;
      }
    }
    frame = frame->GetNextSibling();
  }

  // Attempt to pull frames from our next-in-flow until we can't
  if (!done && GetNextInFlow()) {
    while (true) {
      bool reflowingFirstLetter = lineLayout->GetFirstLetterStyleOK();
      if (!frame) {  // Could be non-null if we pulled a first-letter frame and
                     // it created a continuation, since we don't push those.
        frame = PullOneFrame(aPresContext, irs);
      }
#ifdef NOISY_PUSHING
      printf("%p pulled up %p\n", this, frame);
#endif
      if (!frame) {
        break;
      }
      ReflowInlineFrame(aPresContext, aReflowInput, irs, frame, aStatus);
      if (aStatus.IsInlineBreak() ||
          (!reflowingFirstLetter && aStatus.IsIncomplete())) {
        break;
      }
      irs.mPrevFrame = frame;
      frame = frame->GetNextSibling();
    }
  }

  NS_ASSERTION(!aStatus.IsComplete() || !GetOverflowFrames(),
               "We can't be complete AND have overflow frames!");

  // If after reflowing our children they take up no area then make
  // sure that we don't either.
  //
  // Note: CSS demands that empty inline elements still affect the
  // line-height calculations. However, continuations of an inline
  // that are empty we force to empty so that things like collapsed
  // whitespace in an inline element don't affect the line-height.
  aMetrics.ISize(lineWM) = lineLayout->EndSpan(this);

  // Compute final width.

  // XXX Note that that the padding start and end are in the frame's
  //     writing mode, but the metrics' inline-size is in the line's
  //     writing mode. This makes sense if the line and frame are both
  //     vertical or both horizontal, but what should happen with
  //     orthogonal inlines?

  // Make sure to not include our start border and padding if we have a prev
  // continuation or if we're in a part of an {ib} split other than the first
  // one.  For box-decoration-break:clone we always include our start border
  // and padding since all continuations have them.
  if ((!GetPrevContinuation() && !FrameIsNonFirstInIBSplit()) ||
      boxDecorationBreakClone) {
    aMetrics.ISize(lineWM) += framePadding.IStart(frameWM);
  }

  /*
   * We want to only apply the end border and padding if we're the last
   * continuation and either not in an {ib} split or the last part of it.  To
   * be the last continuation we have to be complete (so that we won't get a
   * next-in-flow) and have no non-fluid continuations on our continuation
   * chain.  For box-decoration-break:clone we always apply the end border and
   * padding since all continuations have them.
   */
  if ((aStatus.IsComplete() && !LastInFlow()->GetNextContinuation() &&
       !FrameIsNonLastInIBSplit()) ||
      boxDecorationBreakClone) {
    aMetrics.ISize(lineWM) += framePadding.IEnd(frameWM);
  }

  nsLayoutUtils::SetBSizeFromFontMetrics(this, aMetrics, framePadding, lineWM,
                                         frameWM);

  // For now our overflow area is zero. The real value will be
  // computed in |nsLineLayout::RelativePositionFrames|.
  aMetrics.mOverflowAreas.Clear();

#ifdef NOISY_FINAL_SIZE
  ListTag(stdout);
  printf(": metrics=%d,%d ascent=%d\n", aMetrics.Width(), aMetrics.Height(),
         aMetrics.TopAscent());
#endif
}

// Returns whether there's any remaining frame to pull.
/* static */
bool nsInlineFrame::HasFramesToPull(nsInlineFrame* aNextInFlow) {
  while (aNextInFlow) {
    if (!aNextInFlow->mFrames.IsEmpty()) {
      return true;
    }
    if (const nsFrameList* overflow = aNextInFlow->GetOverflowFrames()) {
      if (!overflow->IsEmpty()) {
        return true;
      }
    }
    aNextInFlow = static_cast<nsInlineFrame*>(aNextInFlow->GetNextInFlow());
  }
  return false;
}

void nsInlineFrame::ReflowInlineFrame(nsPresContext* aPresContext,
                                      const ReflowInput& aReflowInput,
                                      InlineReflowInput& irs, nsIFrame* aFrame,
                                      nsReflowStatus& aStatus) {
  nsLineLayout* lineLayout = aReflowInput.mLineLayout;
  bool reflowingFirstLetter = lineLayout->GetFirstLetterStyleOK();
  bool pushedFrame;
  aStatus.Reset();
  lineLayout->ReflowFrame(aFrame, aStatus, nullptr, pushedFrame);

  if (aStatus.IsInlineBreakBefore()) {
    if (aFrame != mFrames.FirstChild()) {
      // Change break-before status into break-after since we have
      // already placed at least one child frame. This preserves the
      // break-type so that it can be propagated upward.
      StyleClear oldBreakType = aStatus.BreakType();
      aStatus.Reset();
      aStatus.SetIncomplete();
      aStatus.SetInlineLineBreakAfter(oldBreakType);
      PushFrames(aPresContext, aFrame, irs.mPrevFrame, irs);
    } else {
      // Preserve reflow status when breaking-before our first child
      // and propagate it upward without modification.
    }
    return;
  }

  // Create a next-in-flow if needed.
  if (!aStatus.IsFullyComplete()) {
    CreateNextInFlow(aFrame);
  }

  if (aStatus.IsInlineBreakAfter()) {
    nsIFrame* nextFrame = aFrame->GetNextSibling();
    if (nextFrame) {
      aStatus.SetIncomplete();
      PushFrames(aPresContext, nextFrame, aFrame, irs);
    } else {
      // We must return an incomplete status if there are more child
      // frames remaining in a next-in-flow that follows this frame.
      if (HasFramesToPull(static_cast<nsInlineFrame*>(GetNextInFlow()))) {
        aStatus.SetIncomplete();
      }
    }
    return;
  }

  if (!aStatus.IsFullyComplete() && !reflowingFirstLetter) {
    nsIFrame* nextFrame = aFrame->GetNextSibling();
    if (nextFrame) {
      PushFrames(aPresContext, nextFrame, aFrame, irs);
    }
  }
}

nsIFrame* nsInlineFrame::PullOneFrame(nsPresContext* aPresContext,
                                      InlineReflowInput& irs) {
  nsIFrame* frame = nullptr;
  nsInlineFrame* nextInFlow = irs.mNextInFlow;

#ifdef DEBUG
  bool willPull = HasFramesToPull(nextInFlow);
#endif

  while (nextInFlow) {
    frame = nextInFlow->mFrames.FirstChild();
    if (!frame) {
      // The nextInFlow's principal list has no frames, try its overflow list.
      nsFrameList* overflowFrames = nextInFlow->GetOverflowFrames();
      if (overflowFrames) {
        frame = overflowFrames->RemoveFirstChild();
        if (overflowFrames->IsEmpty()) {
          // We're stealing the only frame - delete the overflow list.
          nextInFlow->DestroyOverflowList();
        } else {
          // We leave the remaining frames on the overflow list (rather than
          // putting them on nextInFlow's principal list) so we don't have to
          // set up the parent for them.
        }
        // ReparentFloatsForInlineChild needs it to be on a child list -
        // we remove it again below.
        nextInFlow->mFrames.SetFrames(frame);
      }
    }

    if (frame) {
      // If our block has no next continuation, then any floats belonging to
      // the pulled frame must belong to our block already. This check ensures
      // we do no extra work in the common non-vertical-breaking case.
      if (irs.mLineContainer && irs.mLineContainer->GetNextContinuation()) {
        // The blockChildren.ContainsFrame check performed by
        // ReparentFloatsForInlineChild will be fast because frame's ancestor
        // will be the first child of its containing block.
        ReparentFloatsForInlineChild(irs.mLineContainer, frame, false);
      }
      nextInFlow->mFrames.RemoveFirstChild();
      // nsFirstLineFrame::PullOneFrame calls ReparentComputedStyle.

      mFrames.InsertFrame(this, irs.mPrevFrame, frame);
      if (irs.mLineLayout) {
        irs.mLineLayout->SetDirtyNextLine();
      }
      nsContainerFrame::ReparentFrameView(frame, nextInFlow, this);
      break;
    }
    nextInFlow = static_cast<nsInlineFrame*>(nextInFlow->GetNextInFlow());
    irs.mNextInFlow = nextInFlow;
  }

  MOZ_ASSERT(!!frame == willPull);
  return frame;
}

void nsInlineFrame::PushFrames(nsPresContext* aPresContext,
                               nsIFrame* aFromChild, nsIFrame* aPrevSibling,
                               InlineReflowInput& aState) {
#ifdef NOISY_PUSHING
  printf("%p pushing aFromChild %p, disconnecting from prev sib %p\n", this,
         aFromChild, aPrevSibling);
#endif

  PushChildrenToOverflow(aFromChild, aPrevSibling);
  if (aState.mLineLayout) {
    aState.mLineLayout->SetDirtyNextLine();
  }
}

//////////////////////////////////////////////////////////////////////

LogicalSides nsInlineFrame::GetLogicalSkipSides() const {
  LogicalSides skip(mWritingMode);
  if (MOZ_UNLIKELY(StyleBorder()->mBoxDecorationBreak ==
                   StyleBoxDecorationBreak::Clone)) {
    return skip;
  }

  if (!IsFirst()) {
    nsInlineFrame* prev = (nsInlineFrame*)GetPrevContinuation();
    if (HasAnyStateBits(NS_INLINE_FRAME_BIDI_VISUAL_STATE_IS_SET) ||
        (prev && (prev->mRect.height || prev->mRect.width))) {
      // Prev continuation is not empty therefore we don't render our start
      // border edge.
      skip |= eLogicalSideBitsIStart;
    } else {
      // If the prev continuation is empty, then go ahead and let our start
      // edge border render.
    }
  }
  if (!IsLast()) {
    nsInlineFrame* next = (nsInlineFrame*)GetNextContinuation();
    if (HasAnyStateBits(NS_INLINE_FRAME_BIDI_VISUAL_STATE_IS_SET) ||
        (next && (next->mRect.height || next->mRect.width))) {
      // Next continuation is not empty therefore we don't render our end
      // border edge.
      skip |= eLogicalSideBitsIEnd;
    } else {
      // If the next continuation is empty, then go ahead and let our end
      // edge border render.
    }
  }

  if (HasAnyStateBits(NS_FRAME_PART_OF_IBSPLIT)) {
    // All but the last part of an {ib} split should skip the "end" side (as
    // determined by this frame's direction) and all but the first part of such
    // a split should skip the "start" side.  But figuring out which part of
    // the split we are involves getting our first continuation, which might be
    // expensive.  So don't bother if we already have the relevant bits set.
    if (skip != LogicalSides(mWritingMode, eLogicalSideBitsIBoth)) {
      // We're missing one of the skip bits, so check whether we need to set it.
      // Only get the first continuation once, as an optimization.
      nsIFrame* firstContinuation = FirstContinuation();
      if (firstContinuation->FrameIsNonLastInIBSplit()) {
        skip |= eLogicalSideBitsIEnd;
      }
      if (firstContinuation->FrameIsNonFirstInIBSplit()) {
        skip |= eLogicalSideBitsIStart;
      }
    }
  }

  return skip;
}

nscoord nsInlineFrame::GetLogicalBaseline(
    mozilla::WritingMode aWritingMode) const {
  return mBaseline;
}

#ifdef ACCESSIBILITY
a11y::AccType nsInlineFrame::AccessibleType() {
  // FIXME(emilio): This is broken, if the image has its default `display` value
  // overridden. Should be somewhere else.
  if (mContent->IsHTMLElement(
          nsGkAtoms::img))  // Create accessible for broken <img>
    return a11y::eHyperTextType;

  return a11y::eNoType;
}
#endif

void nsInlineFrame::UpdateStyleOfOwnedAnonBoxesForIBSplit(
    ServoRestyleState& aRestyleState) {
  MOZ_ASSERT(HasAnyStateBits(NS_FRAME_OWNS_ANON_BOXES),
             "Why did we get called?");
  MOZ_ASSERT(HasAnyStateBits(NS_FRAME_PART_OF_IBSPLIT),
             "Why did we have the NS_FRAME_OWNS_ANON_BOXES bit set?");
  // Note: this assert _looks_ expensive, but it's cheap in all the cases when
  // it passes!
  MOZ_ASSERT(nsLayoutUtils::FirstContinuationOrIBSplitSibling(this) == this,
             "Only the primary frame of the inline in a block-inside-inline "
             "split should have NS_FRAME_OWNS_ANON_BOXES");
  MOZ_ASSERT(mContent->GetPrimaryFrame() == this,
             "We should be the primary frame for our element");

  nsIFrame* blockFrame = GetProperty(nsIFrame::IBSplitSibling());
  MOZ_ASSERT(blockFrame, "Why did we have an IB split?");

  // The later inlines need to get our style.
  ComputedStyle* ourStyle = Style();

  // The anonymous block's style inherits from ours, and we already have our new
  // ComputedStyle.
  RefPtr<ComputedStyle> newContext =
      aRestyleState.StyleSet().ResolveInheritingAnonymousBoxStyle(
          PseudoStyleType::mozBlockInsideInlineWrapper, ourStyle);

  // We're guaranteed that newContext only differs from the old ComputedStyle on
  // the block in things they might inherit from us.  And changehint processing
  // guarantees walking the continuation and ib-sibling chains, so our existing
  // changehint being in aChangeList is good enough.  So we don't need to touch
  // aChangeList at all here.

  while (blockFrame) {
    MOZ_ASSERT(!blockFrame->GetPrevContinuation(),
               "Must be first continuation");

    MOZ_ASSERT(blockFrame->Style()->GetPseudoType() ==
                   PseudoStyleType::mozBlockInsideInlineWrapper,
               "Unexpected kind of ComputedStyle");

    for (nsIFrame* cont = blockFrame; cont;
         cont = cont->GetNextContinuation()) {
      cont->SetComputedStyle(newContext);
    }

    nsIFrame* nextInline = blockFrame->GetProperty(nsIFrame::IBSplitSibling());

    // This check is here due to bug 1431232.  Please remove it once
    // that bug is fixed.
    if (MOZ_UNLIKELY(!nextInline)) {
      break;
    }

    MOZ_ASSERT(nextInline, "There is always a trailing inline in an IB split");

    for (nsIFrame* cont = nextInline; cont;
         cont = cont->GetNextContinuation()) {
      cont->SetComputedStyle(ourStyle);
    }
    blockFrame = nextInline->GetProperty(nsIFrame::IBSplitSibling());
  }
}

//////////////////////////////////////////////////////////////////////

// nsLineFrame implementation

nsFirstLineFrame* NS_NewFirstLineFrame(PresShell* aPresShell,
                                       ComputedStyle* aStyle) {
  return new (aPresShell)
      nsFirstLineFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsFirstLineFrame)

void nsFirstLineFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                            nsIFrame* aPrevInFlow) {
  nsInlineFrame::Init(aContent, aParent, aPrevInFlow);
  if (!aPrevInFlow) {
    MOZ_ASSERT(Style()->GetPseudoType() == PseudoStyleType::firstLine);
    return;
  }

  // This frame is a continuation - fixup the computed style if aPrevInFlow
  // is the first-in-flow (the only one with a ::first-line pseudo).
  if (aPrevInFlow->Style()->GetPseudoType() == PseudoStyleType::firstLine) {
    MOZ_ASSERT(FirstInFlow() == aPrevInFlow);
    // Create a new ComputedStyle that is a child of the parent
    // ComputedStyle thus removing the ::first-line style. This way
    // we behave as if an anonymous (unstyled) span was the child
    // of the parent frame.
    ComputedStyle* parentContext = aParent->Style();
    RefPtr<ComputedStyle> newSC =
        PresContext()->StyleSet()->ResolveInheritingAnonymousBoxStyle(
            PseudoStyleType::mozLineFrame, parentContext);
    SetComputedStyle(newSC);
  } else {
    MOZ_ASSERT(FirstInFlow() != aPrevInFlow);
    MOZ_ASSERT(aPrevInFlow->Style()->GetPseudoType() ==
               PseudoStyleType::mozLineFrame);
  }
}

#ifdef DEBUG_FRAME_DUMP
nsresult nsFirstLineFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(u"Line"_ns, aResult);
}
#endif

nsIFrame* nsFirstLineFrame::PullOneFrame(nsPresContext* aPresContext,
                                         InlineReflowInput& irs) {
  nsIFrame* frame = nsInlineFrame::PullOneFrame(aPresContext, irs);
  if (frame && !GetPrevInFlow()) {
    // We are a first-line frame. Fixup the child frames
    // style-context that we just pulled.
    NS_ASSERTION(frame->GetParent() == this, "Incorrect parent?");
    aPresContext->RestyleManager()->ReparentComputedStyleForFirstLine(frame);
    nsLayoutUtils::MarkDescendantsDirty(frame);
  }
  return frame;
}

void nsFirstLineFrame::Reflow(nsPresContext* aPresContext,
                              ReflowOutput& aMetrics,
                              const ReflowInput& aReflowInput,
                              nsReflowStatus& aStatus) {
  MarkInReflow();
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  if (nullptr == aReflowInput.mLineLayout) {
    return;  // XXX does this happen? why?
  }

  // Check for an overflow list with our prev-in-flow
  nsFirstLineFrame* prevInFlow = (nsFirstLineFrame*)GetPrevInFlow();
  if (prevInFlow) {
    AutoFrameListPtr prevOverflowFrames(aPresContext,
                                        prevInFlow->StealOverflowFrames());
    if (prevOverflowFrames) {
      // Reparent the new frames and their ComputedStyles.
      const nsFrameList::Slice& newFrames =
          mFrames.InsertFrames(this, nullptr, *prevOverflowFrames);
      ReparentChildListStyle(aPresContext, newFrames, this);
    }
  }

  // It's also possible that we have an overflow list for ourselves.
  DrainSelfOverflowList();

  // Set our own reflow input (additional state above and beyond aReflowInput).
  InlineReflowInput irs;
  irs.mPrevFrame = nullptr;
  irs.mLineContainer = aReflowInput.mLineLayout->LineContainerFrame();
  irs.mLineLayout = aReflowInput.mLineLayout;
  irs.mNextInFlow = (nsInlineFrame*)GetNextInFlow();

  bool wasEmpty = mFrames.IsEmpty();
  if (wasEmpty) {
    // Try to pull over one frame before starting so that we know
    // whether we have an anonymous block or not.
    PullOneFrame(aPresContext, irs);
  }

  if (nullptr == GetPrevInFlow()) {
    // XXX This is pretty sick, but what we do here is to pull-up, in
    // advance, all of the next-in-flows children. We re-resolve their
    // style while we are at at it so that when we reflow they have
    // the right style.
    //
    // All of this is so that text-runs reflow properly.
    irs.mPrevFrame = mFrames.LastChild();
    for (;;) {
      nsIFrame* frame = PullOneFrame(aPresContext, irs);
      if (!frame) {
        break;
      }
      irs.mPrevFrame = frame;
    }
    irs.mPrevFrame = nullptr;
  }

  NS_ASSERTION(!aReflowInput.mLineLayout->GetInFirstLine(),
               "Nested first-line frames? BOGUS");
  aReflowInput.mLineLayout->SetInFirstLine(true);
  ReflowFrames(aPresContext, aReflowInput, irs, aMetrics, aStatus);
  aReflowInput.mLineLayout->SetInFirstLine(false);

  ReflowAbsoluteFrames(aPresContext, aMetrics, aReflowInput, aStatus);

  // Note: the line layout code will properly compute our overflow state for us
}

/* virtual */
void nsFirstLineFrame::PullOverflowsFromPrevInFlow() {
  nsFirstLineFrame* prevInFlow =
      static_cast<nsFirstLineFrame*>(GetPrevInFlow());
  if (prevInFlow) {
    nsPresContext* presContext = PresContext();
    AutoFrameListPtr prevOverflowFrames(presContext,
                                        prevInFlow->StealOverflowFrames());
    if (prevOverflowFrames) {
      // Assume that our prev-in-flow has the same line container that we do.
      const nsFrameList::Slice& newFrames =
          mFrames.InsertFrames(this, nullptr, *prevOverflowFrames);
      ReparentChildListStyle(presContext, newFrames, this);
    }
  }
}

/* virtual */
bool nsFirstLineFrame::DrainSelfOverflowList() {
  AutoFrameListPtr overflowFrames(PresContext(), StealOverflowFrames());
  if (overflowFrames) {
    bool result = !overflowFrames->IsEmpty();
    const nsFrameList::Slice& newFrames =
        mFrames.AppendFrames(nullptr, *overflowFrames);
    ReparentChildListStyle(PresContext(), newFrames, this);
    return result;
  }
  return false;
}
