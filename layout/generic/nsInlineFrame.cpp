/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS display:inline objects */

#include "nsCOMPtr.h"
#include "nsInlineFrame.h"
#include "nsBlockFrame.h"
#include "nsPlaceholderFrame.h"
#include "nsGkAtoms.h"
#include "nsHTMLParts.h"
#include "nsStyleContext.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsRenderingContext.h"
#include "nsCSSAnonBoxes.h"
#include "nsAutoPtr.h"
#include "nsFrameManager.h"
#include "nsDisplayList.h"
#include "mozilla/Likely.h"

#ifdef DEBUG
#undef NOISY_PUSHING
#endif

using namespace mozilla;


//////////////////////////////////////////////////////////////////////

// Basic nsInlineFrame methods

nsIFrame*
NS_NewInlineFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsInlineFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsInlineFrame)

NS_QUERYFRAME_HEAD(nsInlineFrame)
  NS_QUERYFRAME_ENTRY(nsInlineFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

#ifdef DEBUG
NS_IMETHODIMP
nsInlineFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Inline"), aResult);
}
#endif

nsIAtom*
nsInlineFrame::GetType() const
{
  return nsGkAtoms::inlineFrame;
}

static inline bool
IsMarginZero(const nsStyleCoord &aCoord)
{
  return aCoord.GetUnit() == eStyleUnit_Auto ||
         nsLayoutUtils::IsMarginZero(aCoord);
}

/* virtual */ bool
nsInlineFrame::IsSelfEmpty()
{
#if 0
  // I used to think inline frames worked this way, but it seems they
  // don't.  At least not in our codebase.
  if (GetPresContext()->CompatibilityMode() == eCompatibility_FullStandards) {
    return false;
  }
#endif
  const nsStyleMargin* margin = GetStyleMargin();
  const nsStyleBorder* border = GetStyleBorder();
  const nsStylePadding* padding = GetStylePadding();
  // XXX Top and bottom removed, since they shouldn't affect things, but this
  // doesn't really match with nsLineLayout.cpp's setting of
  // ZeroEffectiveSpanBox, anymore, so what should this really be?
  bool haveRight =
    border->GetComputedBorderWidth(NS_SIDE_RIGHT) != 0 ||
    !nsLayoutUtils::IsPaddingZero(padding->mPadding.GetRight()) ||
    !IsMarginZero(margin->mMargin.GetRight());
  bool haveLeft =
    border->GetComputedBorderWidth(NS_SIDE_LEFT) != 0 ||
    !nsLayoutUtils::IsPaddingZero(padding->mPadding.GetLeft()) ||
    !IsMarginZero(margin->mMargin.GetLeft());
  if (haveLeft || haveRight) {
    if (GetStateBits() & NS_FRAME_IS_SPECIAL) {
      bool haveStart, haveEnd;
      if (NS_STYLE_DIRECTION_LTR == GetStyleVisibility()->mDirection) {
        haveStart = haveLeft;
        haveEnd = haveRight;
      } else {
        haveStart = haveRight;
        haveEnd = haveLeft;
      }
      // For special frames, ignore things we know we'll skip in GetSkipSides.
      // XXXbz should we be doing this for non-special frames too, in a more
      // general way?

      // Get the first continuation eagerly, as a performance optimization, to
      // avoid having to get it twice..
      nsIFrame* firstCont = GetFirstContinuation();
      return
        (!haveStart || nsLayoutUtils::FrameIsNonFirstInIBSplit(firstCont)) &&
        (!haveEnd || nsLayoutUtils::FrameIsNonLastInIBSplit(firstCont));
    }
    return false;
  }
  return true;
}

bool
nsInlineFrame::IsEmpty()
{
  if (!IsSelfEmpty()) {
    return false;
  }

  for (nsIFrame *kid = mFrames.FirstChild(); kid; kid = kid->GetNextSibling()) {
    if (!kid->IsEmpty())
      return false;
  }

  return true;
}

bool
nsInlineFrame::PeekOffsetCharacter(bool aForward, int32_t* aOffset,
                                   bool aRespectClusters)
{
  // Override the implementation in nsFrame, to skip empty inline frames
  NS_ASSERTION (aOffset && *aOffset <= 1, "aOffset out of range");
  int32_t startOffset = *aOffset;
  if (startOffset < 0)
    startOffset = 1;
  if (aForward == (startOffset == 0)) {
    // We're before the frame and moving forward, or after it and moving backwards:
    // skip to the other side, but keep going.
    *aOffset = 1 - startOffset;
  }
  return false;
}

NS_IMETHODIMP
nsInlineFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists)
{
  nsresult rv = BuildDisplayListForInline(aBuilder, aDirtyRect, aLists);
  NS_ENSURE_SUCCESS(rv, rv);

  // The sole purpose of this is to trigger display of the selection
  // window for Named Anchors, which don't have any children and
  // normally don't have any size, but in Editor we use CSS to display
  // an image to represent this "hidden" element.
  if (!mFrames.FirstChild()) {
    rv = DisplaySelectionOverlay(aBuilder, aLists.Content());
  }
  return rv;
}

//////////////////////////////////////////////////////////////////////
// Reflow methods

/* virtual */ void
nsInlineFrame::AddInlineMinWidth(nsRenderingContext *aRenderingContext,
                                 nsIFrame::InlineMinWidthData *aData)
{
  DoInlineIntrinsicWidth(aRenderingContext, aData, nsLayoutUtils::MIN_WIDTH);
}

/* virtual */ void
nsInlineFrame::AddInlinePrefWidth(nsRenderingContext *aRenderingContext,
                                  nsIFrame::InlinePrefWidthData *aData)
{
  DoInlineIntrinsicWidth(aRenderingContext, aData, nsLayoutUtils::PREF_WIDTH);
}

/* virtual */ nsSize
nsInlineFrame::ComputeSize(nsRenderingContext *aRenderingContext,
                           nsSize aCBSize, nscoord aAvailableWidth,
                           nsSize aMargin, nsSize aBorder, nsSize aPadding,
                           uint32_t aFlags)
{
  // Inlines and text don't compute size before reflow.
  return nsSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
}

nsRect
nsInlineFrame::ComputeTightBounds(gfxContext* aContext) const
{
  // be conservative
  if (GetStyleContext()->HasTextDecorationLines()) {
    return GetVisualOverflowRect();
  }
  return ComputeSimpleTightBounds(aContext);
}

void
nsInlineFrame::ReparentFloatsForInlineChild(nsIFrame* aOurLineContainer,
                                            nsIFrame* aFrame,
                                            bool aReparentSiblings)
{
  // XXXbz this would be better if it took a nsFrameList or a frame
  // list slice....
  NS_ASSERTION(aOurLineContainer->GetNextContinuation() ||
               aOurLineContainer->GetPrevContinuation(),
               "Don't call this when we have no continuation, it's a waste");
  if (!aFrame) {
    NS_ASSERTION(aReparentSiblings, "Why did we get called?");
    return;
  }

  nsIFrame* ancestor = aFrame;
  nsIFrame* ancestorBlockChild;
  do {
    ancestorBlockChild = ancestor;
    ancestor = ancestor->GetParent();
    if (!ancestor)
      return;
  } while (!ancestor->IsFloatContainingBlock());

  if (ancestor == aOurLineContainer)
    return;

  nsBlockFrame* ourBlock = nsLayoutUtils::GetAsBlock(aOurLineContainer);
  NS_ASSERTION(ourBlock, "Not a block, but broke vertically?");
  nsBlockFrame* frameBlock = nsLayoutUtils::GetAsBlock(ancestor);
  NS_ASSERTION(frameBlock, "ancestor not a block");

  const nsFrameList& blockChildren(ancestor->PrincipalChildList());
  bool isOverflow = !blockChildren.ContainsFrame(ancestorBlockChild);

  while (true) {
    ourBlock->ReparentFloats(aFrame, frameBlock, isOverflow, false);

    if (!aReparentSiblings)
      return;
    nsIFrame* next = aFrame->GetNextSibling();
    if (!next)
      return;
    if (next->GetParent() == aFrame->GetParent()) {
      aFrame = next;
      continue;
    }
    // This is paranoid and will hardly ever get hit ... but we can't actually
    // trust that the frames in the sibling chain all have the same parent,
    // because lazy reparenting may be going on. If we find a different
    // parent we need to redo our analysis.
    ReparentFloatsForInlineChild(aOurLineContainer, next, aReparentSiblings);
    return;
  }
}

static void
ReparentChildListStyle(nsPresContext* aPresContext,
                       const nsFrameList::Slice& aFrames,
                       nsIFrame* aParentFrame)
{
  nsFrameManager *frameManager = aPresContext->FrameManager();

  for (nsFrameList::Enumerator e(aFrames); !e.AtEnd(); e.Next()) {
    NS_ASSERTION(e.get()->GetParent() == aParentFrame, "Bogus parentage");
    frameManager->ReparentStyleContext(e.get());
  }
}

NS_IMETHODIMP
nsInlineFrame::Reflow(nsPresContext*          aPresContext,
                      nsHTMLReflowMetrics&     aMetrics,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsInlineFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aMetrics, aStatus);
  if (nullptr == aReflowState.mLineLayout) {
    return NS_ERROR_INVALID_ARG;
  }
  if (IsFrameTreeTooDeep(aReflowState, aMetrics, aStatus)) {
    return NS_OK;
  }

  bool    lazilySetParentPointer = false;

  nsIFrame* lineContainer = aReflowState.mLineLayout->GetLineContainerFrame();

   // Check for an overflow list with our prev-in-flow
  nsInlineFrame* prevInFlow = (nsInlineFrame*)GetPrevInFlow();
  if (nullptr != prevInFlow) {
    nsAutoPtr<nsFrameList> prevOverflowFrames(prevInFlow->StealOverflowFrames());

    if (prevOverflowFrames) {
      // When pushing and pulling frames we need to check for whether any
      // views need to be reparented.
      nsContainerFrame::ReparentFrameViewList(aPresContext,
                                              *prevOverflowFrames,
                                              prevInFlow, this);

      // Check if we should do the lazilySetParentPointer optimization.
      // Only do it in simple cases where we're being reflowed for the
      // first time, nothing (e.g. bidi resolution) has already given
      // us children, and there's no next-in-flow, so all our frames
      // will be taken from prevOverflowFrames.
      if ((GetStateBits() & NS_FRAME_FIRST_REFLOW) && mFrames.IsEmpty() &&
          !GetNextInFlow()) {
        // If our child list is empty, just put the new frames into it.
        // Note that we don't set the parent pointer for the new frames. Instead wait
        // to do this until we actually reflow the frame. If the overflow list contains
        // thousands of frames this is a big performance issue (see bug #5588)
        mFrames.SetFrames(*prevOverflowFrames);
        lazilySetParentPointer = true;
      } else {
        // Assign all floats to our block if necessary
        if (lineContainer && lineContainer->GetPrevContinuation()) {
          ReparentFloatsForInlineChild(lineContainer,
                                       prevOverflowFrames->FirstChild(),
                                       true);
        }
        // Insert the new frames at the beginning of the child list
        // and set their parent pointer
        const nsFrameList::Slice& newFrames =
          mFrames.InsertFrames(this, nullptr, *prevOverflowFrames);
        // If our prev in flow was under the first continuation of a first-line
        // frame then we need to reparent the style contexts to remove the
        // the special first-line styling. In the lazilySetParentPointer case
        // we reparent the style contexts when we set their parents in
        // nsInlineFrame::ReflowFrames and nsInlineFrame::ReflowInlineFrame.
        if (aReflowState.mLineLayout->GetInFirstLine()) {
          ReparentChildListStyle(aPresContext, newFrames, this);
        }
      }
    }
  }

  // It's also possible that we have an overflow list for ourselves
#ifdef DEBUG
  if (GetStateBits() & NS_FRAME_FIRST_REFLOW) {
    // If it's our initial reflow, then we should not have an overflow list.
    // However, add an assertion in case we get reflowed more than once with
    // the initial reflow reason
    nsFrameList* overflowFrames = GetOverflowFrames();
    NS_ASSERTION(!overflowFrames || overflowFrames->IsEmpty(),
                 "overflow list is not empty for initial reflow");
  }
#endif
  if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    nsAutoPtr<nsFrameList> overflowFrames(StealOverflowFrames());
    if (overflowFrames) {
      NS_ASSERTION(mFrames.NotEmpty(), "overflow list w/o frames");
      if (!lazilySetParentPointer) {
        // The frames on our own overflowlist may have been pushed by a
        // previous lazilySetParentPointer Reflow so we need to ensure
        // the correct parent pointer now since we're not setting it
        // lazily in this Reflow.
        nsIFrame* firstChild = overflowFrames->FirstChild();
        if (lineContainer && lineContainer->GetPrevContinuation()) {
          ReparentFloatsForInlineChild(lineContainer, firstChild, true);
        }
        const bool inFirstLine = aReflowState.mLineLayout->GetInFirstLine();
        nsFrameManager* fm = PresContext()->FrameManager();
        for (nsIFrame* f = firstChild; f; f = f->GetNextSibling()) {
          f->SetParent(this);
          if (inFirstLine) {
            fm->ReparentStyleContext(f);
          }
        }
      }
      mFrames.AppendFrames(nullptr, *overflowFrames);
    }
  }

  // Set our own reflow state (additional state above and beyond
  // aReflowState)
  InlineReflowState irs;
  irs.mPrevFrame = nullptr;
  irs.mLineContainer = lineContainer;
  irs.mLineLayout = aReflowState.mLineLayout;
  irs.mNextInFlow = (nsInlineFrame*) GetNextInFlow();
  irs.mSetParentPointer = lazilySetParentPointer;

  nsresult rv;
  if (mFrames.IsEmpty()) {
    // Try to pull over one frame before starting so that we know
    // whether we have an anonymous block or not.
    bool complete;
    (void) PullOneFrame(aPresContext, irs, &complete);
  }

  rv = ReflowFrames(aPresContext, aReflowState, irs, aMetrics, aStatus);

  ReflowAbsoluteFrames(aPresContext, aMetrics, aReflowState, aStatus);

  // Note: the line layout code will properly compute our
  // overflow-rect state for us.

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aMetrics);
  return rv;
}

/* virtual */ bool
nsInlineFrame::CanContinueTextRun() const
{
  // We can continue a text run through an inline frame
  return true;
}

/* virtual */ void
nsInlineFrame::PullOverflowsFromPrevInFlow()
{
  nsInlineFrame* prevInFlow = static_cast<nsInlineFrame*>(GetPrevInFlow());
  if (prevInFlow) {
    nsAutoPtr<nsFrameList> prevOverflowFrames(prevInFlow->StealOverflowFrames());
    if (prevOverflowFrames) {
      // Assume that our prev-in-flow has the same line container that we do.
      nsContainerFrame::ReparentFrameViewList(PresContext(),
                                              *prevOverflowFrames,
                                              prevInFlow, this);
      mFrames.InsertFrames(this, nullptr, *prevOverflowFrames);
    }
  }
}

nsresult
nsInlineFrame::ReflowFrames(nsPresContext* aPresContext,
                            const nsHTMLReflowState& aReflowState,
                            InlineReflowState& irs,
                            nsHTMLReflowMetrics& aMetrics,
                            nsReflowStatus& aStatus)
{
  nsresult rv = NS_OK;
  aStatus = NS_FRAME_COMPLETE;

  nsLineLayout* lineLayout = aReflowState.mLineLayout;
  bool inFirstLine = aReflowState.mLineLayout->GetInFirstLine();
  nsFrameManager* frameManager = aPresContext->FrameManager();
  bool ltr = (NS_STYLE_DIRECTION_LTR == aReflowState.mStyleVisibility->mDirection);
  nscoord leftEdge = 0;
  // Don't offset by our start borderpadding if we have a prev continuation or
  // if we're in a part of an {ib} split other than the first one.
  if (!GetPrevContinuation() &&
      !nsLayoutUtils::FrameIsNonFirstInIBSplit(this)) {
    leftEdge = ltr ? aReflowState.mComputedBorderPadding.left
                   : aReflowState.mComputedBorderPadding.right;
  }
  nscoord availableWidth = aReflowState.availableWidth;
  NS_ASSERTION(availableWidth != NS_UNCONSTRAINEDSIZE,
               "should no longer use available widths");
  // Subtract off left and right border+padding from availableWidth
  availableWidth -= leftEdge;
  availableWidth -= ltr ? aReflowState.mComputedBorderPadding.right
                        : aReflowState.mComputedBorderPadding.left;
  lineLayout->BeginSpan(this, &aReflowState, leftEdge,
                        leftEdge + availableWidth, &mBaseline);

  // First reflow our current children
  nsIFrame* frame = mFrames.FirstChild();
  bool done = false;
  while (nullptr != frame) {
    bool reflowingFirstLetter = lineLayout->GetFirstLetterStyleOK();

    // Check if we should lazily set the child frame's parent pointer
    if (irs.mSetParentPointer) {
      bool havePrevBlock =
        irs.mLineContainer && irs.mLineContainer->GetPrevContinuation();
      nsIFrame* child = frame;
      do {
        // If our block is the first in flow, then any floats under the pulled
        // frame must already belong to our block.
        if (havePrevBlock) {
          // This has to happen before we update frame's parent; we need to
          // know frame's ancestry under its old block.
          // The blockChildren.ContainsFrame check performed by
          // ReparentFloatsForInlineChild here may be slow, but we can't
          // easily avoid it because we don't know where 'frame' originally
          // came from. If we really really have to optimize this we could
          // cache whether frame->GetParent() is under its containing blocks
          // overflowList or not.
          ReparentFloatsForInlineChild(irs.mLineContainer, child, false);
        }
        child->SetParent(this);
        if (inFirstLine) {
          frameManager->ReparentStyleContext(child);
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
      if (realFrame->GetType() == nsGkAtoms::letterFrame) {
        nsIFrame* child = realFrame->GetFirstPrincipalChild();
        if (child) {
          NS_ASSERTION(child->GetType() == nsGkAtoms::textFrame,
                       "unexpected frame type");
          nsIFrame* nextInFlow = child->GetNextInFlow();
          for ( ; nextInFlow; nextInFlow = nextInFlow->GetNextInFlow()) {
            NS_ASSERTION(nextInFlow->GetType() == nsGkAtoms::textFrame,
                         "unexpected frame type");
            if (mFrames.ContainsFrame(nextInFlow)) {
              nextInFlow->SetParent(this);
              if (inFirstLine) {
                frameManager->ReparentStyleContext(nextInFlow);
              }
            }
            else {
#ifdef DEBUG              
              // Once we find a next-in-flow that isn't ours none of the
              // remaining next-in-flows should be either.
              for ( ; nextInFlow; nextInFlow = nextInFlow->GetNextInFlow()) {
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
      rv = ReflowInlineFrame(aPresContext, aReflowState, irs, frame, aStatus);
      done = NS_FAILED(rv) ||
             NS_INLINE_IS_BREAK(aStatus) || 
             (!reflowingFirstLetter && NS_FRAME_IS_NOT_COMPLETE(aStatus));
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
      bool isComplete;
      if (!frame) { // Could be non-null if we pulled a first-letter frame and
                    // it created a continuation, since we don't push those.
        frame = PullOneFrame(aPresContext, irs, &isComplete);
      }
#ifdef NOISY_PUSHING
      printf("%p pulled up %p\n", this, frame);
#endif
      if (nullptr == frame) {
        if (!isComplete) {
          aStatus = NS_FRAME_NOT_COMPLETE;
        }
        break;
      }
      rv = ReflowInlineFrame(aPresContext, aReflowState, irs, frame, aStatus);
      if (NS_FAILED(rv) ||
          NS_INLINE_IS_BREAK(aStatus) || 
          (!reflowingFirstLetter && NS_FRAME_IS_NOT_COMPLETE(aStatus))) {
        break;
      }
      irs.mPrevFrame = frame;
      frame = frame->GetNextSibling();
    }
  }

  NS_ASSERTION(!NS_FRAME_IS_COMPLETE(aStatus) || !GetOverflowFrames(),
               "We can't be complete AND have overflow frames!");

  // If after reflowing our children they take up no area then make
  // sure that we don't either.
  //
  // Note: CSS demands that empty inline elements still affect the
  // line-height calculations. However, continuations of an inline
  // that are empty we force to empty so that things like collapsed
  // whitespace in an inline element don't affect the line-height.
  aMetrics.width = lineLayout->EndSpan(this);

  // Compute final width.

  // Make sure to not include our start border and padding if we have a prev
  // continuation or if we're in a part of an {ib} split other than the first
  // one.
  if (!GetPrevContinuation() &&
      !nsLayoutUtils::FrameIsNonFirstInIBSplit(this)) {
    aMetrics.width += ltr ? aReflowState.mComputedBorderPadding.left
                          : aReflowState.mComputedBorderPadding.right;
  }

  /*
   * We want to only apply the end border and padding if we're the last
   * continuation and either not in an {ib} split or the last part of it.  To
   * be the last continuation we have to be complete (so that we won't get a
   * next-in-flow) and have no non-fluid continuations on our continuation
   * chain.
   */
  if (NS_FRAME_IS_COMPLETE(aStatus) &&
      !GetLastInFlow()->GetNextContinuation() &&
      !nsLayoutUtils::FrameIsNonLastInIBSplit(this)) {
    aMetrics.width += ltr ? aReflowState.mComputedBorderPadding.right
                          : aReflowState.mComputedBorderPadding.left;
  }

  nsRefPtr<nsFontMetrics> fm;
  float inflation = nsLayoutUtils::FontSizeInflationFor(this);
  nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fm), inflation);
  aReflowState.rendContext->SetFont(fm);

  if (fm) {
    // Compute final height of the frame.
    //
    // Do things the standard css2 way -- though it's hard to find it
    // in the css2 spec! It's actually found in the css1 spec section
    // 4.4 (you will have to read between the lines to really see
    // it).
    //
    // The height of our box is the sum of our font size plus the top
    // and bottom border and padding. The height of children do not
    // affect our height.
    aMetrics.ascent = fm->MaxAscent();
    aMetrics.height = fm->MaxHeight();
  } else {
    NS_WARNING("Cannot get font metrics - defaulting sizes to 0");
    aMetrics.ascent = aMetrics.height = 0;
  }
  aMetrics.ascent += aReflowState.mComputedBorderPadding.top;
  aMetrics.height += aReflowState.mComputedBorderPadding.top +
    aReflowState.mComputedBorderPadding.bottom;

  // For now our overflow area is zero. The real value will be
  // computed in |nsLineLayout::RelativePositionFrames|.
  aMetrics.mOverflowAreas.Clear();

#ifdef NOISY_FINAL_SIZE
  ListTag(stdout);
  printf(": metrics=%d,%d ascent=%d\n",
         aMetrics.width, aMetrics.height, aMetrics.ascent);
#endif

  return rv;
}

nsresult
nsInlineFrame::ReflowInlineFrame(nsPresContext* aPresContext,
                                 const nsHTMLReflowState& aReflowState,
                                 InlineReflowState& irs,
                                 nsIFrame* aFrame,
                                 nsReflowStatus& aStatus)
{
  nsLineLayout* lineLayout = aReflowState.mLineLayout;
  bool reflowingFirstLetter = lineLayout->GetFirstLetterStyleOK();
  bool pushedFrame;
  nsresult rv =
    lineLayout->ReflowFrame(aFrame, aStatus, nullptr, pushedFrame);
  
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (NS_INLINE_IS_BREAK_BEFORE(aStatus)) {
    if (aFrame != mFrames.FirstChild()) {
      // Change break-before status into break-after since we have
      // already placed at least one child frame. This preserves the
      // break-type so that it can be propagated upward.
      aStatus = NS_FRAME_NOT_COMPLETE |
        NS_INLINE_BREAK | NS_INLINE_BREAK_AFTER |
        (aStatus & NS_INLINE_BREAK_TYPE_MASK);
      PushFrames(aPresContext, aFrame, irs.mPrevFrame, irs);
    }
    else {
      // Preserve reflow status when breaking-before our first child
      // and propagate it upward without modification.
    }
    return NS_OK;
  }

  // Create a next-in-flow if needed.
  if (!NS_FRAME_IS_FULLY_COMPLETE(aStatus)) {
    nsIFrame* newFrame;
    rv = CreateNextInFlow(aPresContext, aFrame, newFrame);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  if (NS_INLINE_IS_BREAK_AFTER(aStatus)) {
    nsIFrame* nextFrame = aFrame->GetNextSibling();
    if (nextFrame) {
      NS_FRAME_SET_INCOMPLETE(aStatus);
      PushFrames(aPresContext, nextFrame, aFrame, irs);
    }
    else {
      // We must return an incomplete status if there are more child
      // frames remaining in a next-in-flow that follows this frame.
      nsInlineFrame* nextInFlow = static_cast<nsInlineFrame*>(GetNextInFlow());
      while (nextInFlow) {
        if (nextInFlow->mFrames.NotEmpty()) {
          NS_FRAME_SET_INCOMPLETE(aStatus);
          break;
        }
        nextInFlow = static_cast<nsInlineFrame*>(nextInFlow->GetNextInFlow());
      }
    }
    return NS_OK;
  }

  if (!NS_FRAME_IS_FULLY_COMPLETE(aStatus) && !reflowingFirstLetter) {
    nsIFrame* nextFrame = aFrame->GetNextSibling();
    if (nextFrame) {
      PushFrames(aPresContext, nextFrame, aFrame, irs);
    }
  }
  return NS_OK;
}

nsIFrame*
nsInlineFrame::PullOneFrame(nsPresContext* aPresContext,
                            InlineReflowState& irs,
                            bool* aIsComplete)
{
  bool isComplete = true;

  nsIFrame* frame = nullptr;
  nsInlineFrame* nextInFlow = irs.mNextInFlow;
  while (nullptr != nextInFlow) {
    frame = nextInFlow->mFrames.FirstChild();
    if (!frame) {
      // The nextInFlow's principal list has no frames, try its overflow list.
      nsFrameList* overflowFrames = nextInFlow->GetOverflowFrames();
      if (overflowFrames) {
        frame = overflowFrames->FirstChild();
        if (!frame->GetNextSibling()) {
          // We're stealing the only frame - delete the overflow list.
          delete nextInFlow->StealOverflowFrames();
        } else {
          // We leave the remaining frames on the overflow list (rather than
          // putting them on nextInFlow's principal list) so we don't have to
          // set up the parent for them.
          overflowFrames->RemoveFirstChild();
        }
        // ReparentFloatsForInlineChild needs it to be on a child list -
        // we remove it again below.
        nextInFlow->mFrames.SetFrames(frame);
      }
    }

    if (nullptr != frame) {
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
      // nsFirstLineFrame::PullOneFrame calls ReparentStyleContext.

      mFrames.InsertFrame(this, irs.mPrevFrame, frame);
      isComplete = false;
      if (irs.mLineLayout) {
        irs.mLineLayout->SetDirtyNextLine();
      }
      nsContainerFrame::ReparentFrameView(aPresContext, frame, nextInFlow, this);
      break;
    }
    nextInFlow = (nsInlineFrame*) nextInFlow->GetNextInFlow();
    irs.mNextInFlow = nextInFlow;
  }

  *aIsComplete = isComplete;
  return frame;
}

void
nsInlineFrame::PushFrames(nsPresContext* aPresContext,
                          nsIFrame* aFromChild,
                          nsIFrame* aPrevSibling,
                          InlineReflowState& aState)
{
  NS_PRECONDITION(aFromChild, "null pointer");
  NS_PRECONDITION(aPrevSibling, "pushing first child");
  NS_PRECONDITION(aPrevSibling->GetNextSibling() == aFromChild, "bad prev sibling");

#ifdef NOISY_PUSHING
  printf("%p pushing aFromChild %p, disconnecting from prev sib %p\n", 
         this, aFromChild, aPrevSibling);
#endif

  // Add the frames to our overflow list (let our next in flow drain
  // our overflow list when it is ready)
  SetOverflowFrames(aPresContext, mFrames.RemoveFramesAfter(aPrevSibling));
  if (aState.mLineLayout) {
    aState.mLineLayout->SetDirtyNextLine();
  }
}


//////////////////////////////////////////////////////////////////////

int
nsInlineFrame::GetSkipSides() const
{
  int skip = 0;
  if (!IsLeftMost()) {
    nsInlineFrame* prev = (nsInlineFrame*) GetPrevContinuation();
    if ((GetStateBits() & NS_INLINE_FRAME_BIDI_VISUAL_STATE_IS_SET) ||
        (prev && (prev->mRect.height || prev->mRect.width))) {
      // Prev continuation is not empty therefore we don't render our left
      // border edge.
      skip |= 1 << NS_SIDE_LEFT;
    }
    else {
      // If the prev continuation is empty, then go ahead and let our left
      // edge border render.
    }
  }
  if (!IsRightMost()) {
    nsInlineFrame* next = (nsInlineFrame*) GetNextContinuation();
    if ((GetStateBits() & NS_INLINE_FRAME_BIDI_VISUAL_STATE_IS_SET) ||
        (next && (next->mRect.height || next->mRect.width))) {
      // Next continuation is not empty therefore we don't render our right
      // border edge.
      skip |= 1 << NS_SIDE_RIGHT;
    }
    else {
      // If the next continuation is empty, then go ahead and let our right
      // edge border render.
    }
  }

  if (GetStateBits() & NS_FRAME_IS_SPECIAL) {
    // All but the last part of an {ib} split should skip the "end" side (as
    // determined by this frame's direction) and all but the first part of such
    // a split should skip the "start" side.  But figuring out which part of
    // the split we are involves getting our first continuation, which might be
    // expensive.  So don't bother if we already have the relevant bits set.
    bool ltr = (NS_STYLE_DIRECTION_LTR == GetStyleVisibility()->mDirection);
    int startBit = (1 << (ltr ? NS_SIDE_LEFT : NS_SIDE_RIGHT));
    int endBit = (1 << (ltr ? NS_SIDE_RIGHT : NS_SIDE_LEFT));
    if (((startBit | endBit) & skip) != (startBit | endBit)) {
      // We're missing one of the skip bits, so check whether we need to set it.
      // Only get the first continuation once, as an optimization.
      nsIFrame* firstContinuation = GetFirstContinuation();
      if (nsLayoutUtils::FrameIsNonLastInIBSplit(firstContinuation)) {
        skip |= endBit;
      }
      if (nsLayoutUtils::FrameIsNonFirstInIBSplit(firstContinuation)) {
        skip |= startBit;
      }
    }
  }

  return skip;
}

nscoord
nsInlineFrame::GetBaseline() const
{
  return mBaseline;
}

#ifdef ACCESSIBILITY
a11y::AccType
nsInlineFrame::AccessibleType()
{
  // Broken image accessibles are created here, because layout
  // replaces the image or image control frame with an inline frame
  nsIAtom *tagAtom = mContent->Tag();
  if (tagAtom == nsGkAtoms::input)  // Broken <input type=image ... />
    return a11y::eHTMLButtonAccessible;
  if (tagAtom == nsGkAtoms::img)  // Create accessible for broken <img>
    return a11y::eImageAccessible;
  if (tagAtom == nsGkAtoms::label)  // Creat accessible for <label>
    return a11y::eHTMLLabelAccessible;

  return a11y::eNoAccessible;
}
#endif

//////////////////////////////////////////////////////////////////////

// nsLineFrame implementation

nsIFrame*
NS_NewFirstLineFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsFirstLineFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsFirstLineFrame)

#ifdef DEBUG
NS_IMETHODIMP
nsFirstLineFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Line"), aResult);
}
#endif

nsIAtom*
nsFirstLineFrame::GetType() const
{
  return nsGkAtoms::lineFrame;
}

nsIFrame*
nsFirstLineFrame::PullOneFrame(nsPresContext* aPresContext, InlineReflowState& irs,
                               bool* aIsComplete)
{
  nsIFrame* frame = nsInlineFrame::PullOneFrame(aPresContext, irs, aIsComplete);
  if (frame && !GetPrevInFlow()) {
    // We are a first-line frame. Fixup the child frames
    // style-context that we just pulled.
    NS_ASSERTION(frame->GetParent() == this, "Incorrect parent?");
    aPresContext->FrameManager()->ReparentStyleContext(frame);
  }
  return frame;
}

NS_IMETHODIMP
nsFirstLineFrame::Reflow(nsPresContext* aPresContext,
                         nsHTMLReflowMetrics& aMetrics,
                         const nsHTMLReflowState& aReflowState,
                         nsReflowStatus& aStatus)
{
  if (nullptr == aReflowState.mLineLayout) {
    return NS_ERROR_INVALID_ARG;
  }

  nsIFrame* lineContainer = aReflowState.mLineLayout->GetLineContainerFrame();

  // Check for an overflow list with our prev-in-flow
  nsFirstLineFrame* prevInFlow = (nsFirstLineFrame*)GetPrevInFlow();
  if (nullptr != prevInFlow) {
    nsAutoPtr<nsFrameList> prevOverflowFrames(prevInFlow->StealOverflowFrames());
    if (prevOverflowFrames) {
      // Assign all floats to our block if necessary
      if (lineContainer && lineContainer->GetPrevContinuation()) {
        ReparentFloatsForInlineChild(lineContainer,
                                     prevOverflowFrames->FirstChild(),
                                     true);
      }
      const nsFrameList::Slice& newFrames =
        mFrames.InsertFrames(this, nullptr, *prevOverflowFrames);
      ReparentChildListStyle(aPresContext, newFrames, this);
    }
  }

  // It's also possible that we have an overflow list for ourselves
  nsAutoPtr<nsFrameList> overflowFrames(StealOverflowFrames());
  if (overflowFrames) {
    NS_ASSERTION(mFrames.NotEmpty(), "overflow list w/o frames");

    const nsFrameList::Slice& newFrames =
      mFrames.AppendFrames(nullptr, *overflowFrames);
    ReparentChildListStyle(aPresContext, newFrames, this);
  }

  // Set our own reflow state (additional state above and beyond
  // aReflowState)
  InlineReflowState irs;
  irs.mPrevFrame = nullptr;
  irs.mLineContainer = lineContainer;
  irs.mLineLayout = aReflowState.mLineLayout;
  irs.mNextInFlow = (nsInlineFrame*) GetNextInFlow();

  nsresult rv;
  bool wasEmpty = mFrames.IsEmpty();
  if (wasEmpty) {
    // Try to pull over one frame before starting so that we know
    // whether we have an anonymous block or not.
    bool complete;
    PullOneFrame(aPresContext, irs, &complete);
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
      bool complete;
      nsIFrame* frame = PullOneFrame(aPresContext, irs, &complete);
      if (!frame) {
        break;
      }
      irs.mPrevFrame = frame;
    }
    irs.mPrevFrame = nullptr;
  }
  else {
// XXX do this in the Init method instead
    // For continuations, we need to check and see if our style
    // context is right. If its the same as the first-in-flow, then
    // we need to fix it up (that way :first-line style doesn't leak
    // into this continuation since we aren't the first line).
    nsFirstLineFrame* first = (nsFirstLineFrame*) GetFirstInFlow();
    if (mStyleContext == first->mStyleContext) {
      // Fixup our style context and our children. First get the
      // proper parent context.
      nsStyleContext* parentContext = first->GetParent()->GetStyleContext();
      if (parentContext) {
        // Create a new style context that is a child of the parent
        // style context thus removing the :first-line style. This way
        // we behave as if an anonymous (unstyled) span was the child
        // of the parent frame.
        nsRefPtr<nsStyleContext> newSC;
        newSC = aPresContext->StyleSet()->
          ResolveAnonymousBoxStyle(nsCSSAnonBoxes::mozLineFrame, parentContext);
        if (newSC) {
          // Switch to the new style context.
          SetStyleContext(newSC);

          // Re-resolve all children
          ReparentChildListStyle(aPresContext, mFrames, this);
        }
      }
    }
  }

  NS_ASSERTION(!aReflowState.mLineLayout->GetInFirstLine(),
               "Nested first-line frames? BOGUS");
  aReflowState.mLineLayout->SetInFirstLine(true);
  rv = ReflowFrames(aPresContext, aReflowState, irs, aMetrics, aStatus);
  aReflowState.mLineLayout->SetInFirstLine(false);

  ReflowAbsoluteFrames(aPresContext, aMetrics, aReflowState, aStatus);

  // Note: the line layout code will properly compute our overflow state for us

  return rv;
}

/* virtual */ void
nsFirstLineFrame::PullOverflowsFromPrevInFlow()
{
  nsFirstLineFrame* prevInFlow = static_cast<nsFirstLineFrame*>(GetPrevInFlow());
  if (prevInFlow) {
    nsAutoPtr<nsFrameList> prevOverflowFrames(prevInFlow->StealOverflowFrames());
    if (prevOverflowFrames) {
      // Assume that our prev-in-flow has the same line container that we do.
      const nsFrameList::Slice& newFrames =
        mFrames.InsertFrames(this, nullptr, *prevOverflowFrames);
      ReparentChildListStyle(PresContext(), newFrames, this);
    }
  }
}

