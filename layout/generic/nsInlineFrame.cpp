/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
 * ***** END LICENSE BLOCK ***** */

/* rendering object for CSS display:inline objects */

#include "nsCOMPtr.h"
#include "nsInlineFrame.h"
#include "nsBlockFrame.h"
#include "nsGkAtoms.h"
#include "nsHTMLParts.h"
#include "nsStyleContext.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsAbsoluteContainingBlock.h"
#include "nsCSSAnonBoxes.h"
#include "nsAutoPtr.h"
#include "nsFrameManager.h"
#ifdef ACCESSIBILITY
#include "nsIServiceManager.h"
#include "nsIAccessibilityService.h"
#endif
#include "nsDisplayList.h"

#ifdef DEBUG
#undef NOISY_PUSHING
#endif


NS_DEFINE_IID(kInlineFrameCID, NS_INLINE_FRAME_CID);


//////////////////////////////////////////////////////////////////////

// Basic nsInlineFrame methods

nsIFrame*
NS_NewInlineFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsInlineFrame(aContext);
}

NS_IMETHODIMP
nsInlineFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kInlineFrameCID)) {
    nsInlineFrame* tmp = this;
    *aInstancePtr = (void*) tmp;
    return NS_OK;
  }
  return nsInlineFrameSuper::QueryInterface(aIID, aInstancePtr);
}

void
nsInlineFrame::Destroy()
{
  if (mState & NS_FRAME_GENERATED_CONTENT) {
    // Make sure all the content nodes for the generated content inside
    // this frame know it's going away.
    // This is duplicated in nsBlockFrame::Destroy.
    // See also nsCSSFrameConstructor::CreateGeneratedContentFrame which
    // created this frame.

    // XXXbz would this be better done via a global structure in
    // nsCSSFrameConstructor that could key off of
    // GeneratedContentFrameRemoved or something?  The problem is that
    // our kids are gone by the time that's called.
    nsContainerFrame::CleanupGeneratedContentIn(mContent, this);
  }
  nsInlineFrameSuper::Destroy();
}

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

inline PRBool
IsPaddingZero(nsStyleUnit aUnit, nsStyleCoord &aCoord)
{
    return (aUnit == eStyleUnit_Null ||
            (aUnit == eStyleUnit_Coord && aCoord.GetCoordValue() == 0) ||
            (aUnit == eStyleUnit_Percent && aCoord.GetPercentValue() == 0.0));
}

inline PRBool
IsMarginZero(nsStyleUnit aUnit, nsStyleCoord &aCoord)
{
    return (aUnit == eStyleUnit_Null ||
            aUnit == eStyleUnit_Auto ||
            (aUnit == eStyleUnit_Coord && aCoord.GetCoordValue() == 0) ||
            (aUnit == eStyleUnit_Percent && aCoord.GetPercentValue() == 0.0));
}

/* virtual */ PRBool
nsInlineFrame::IsSelfEmpty()
{
#if 0
  // I used to think inline frames worked this way, but it seems they
  // don't.  At least not in our codebase.
  if (GetPresContext()->CompatibilityMode() == eCompatibility_FullStandards) {
    return PR_FALSE;
  }
#endif
  const nsStyleMargin* margin = GetStyleMargin();
  const nsStyleBorder* border = GetStyleBorder();
  const nsStylePadding* padding = GetStylePadding();
  nsStyleCoord coord;
  // XXX Top and bottom removed, since they shouldn't affect things, but this
  // doesn't really match with nsLineLayout.cpp's setting of
  // ZeroEffectiveSpanBox, anymore, so what should this really be?
  if (border->GetBorderWidth(NS_SIDE_RIGHT) != 0 ||
      border->GetBorderWidth(NS_SIDE_LEFT) != 0 ||
      !IsPaddingZero(padding->mPadding.GetRightUnit(),
                     padding->mPadding.GetRight(coord)) ||
      !IsPaddingZero(padding->mPadding.GetLeftUnit(),
                     padding->mPadding.GetLeft(coord)) ||
      !IsMarginZero(margin->mMargin.GetRightUnit(),
                    margin->mMargin.GetRight(coord)) ||
      !IsMarginZero(margin->mMargin.GetLeftUnit(),
                    margin->mMargin.GetLeft(coord))) {
    return PR_FALSE;
  }
  return PR_TRUE;
}

PRBool
nsInlineFrame::IsEmpty()
{
  if (!IsSelfEmpty()) {
    return PR_FALSE;
  }

  for (nsIFrame *kid = mFrames.FirstChild(); kid; kid = kid->GetNextSibling()) {
    if (!kid->IsEmpty())
      return PR_FALSE;
  }

  return PR_TRUE;
}

PRBool
nsInlineFrame::PeekOffsetCharacter(PRBool aForward, PRInt32* aOffset)
{
  // Override the implementation in nsFrame, to skip empty inline frames
  NS_ASSERTION (aOffset && *aOffset <= 1, "aOffset out of range");
  PRInt32 startOffset = *aOffset;
  if (startOffset < 0)
    startOffset = 1;
  if (aForward == (startOffset == 0)) {
    // We're before the frame and moving forward, or after it and moving backwards:
    // skip to the other side, but keep going.
    *aOffset = 1 - startOffset;
  }
  return PR_FALSE;
}

NS_IMETHODIMP
nsInlineFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists)
{
  nsresult rv = nsHTMLContainerFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // The sole purpose of this is to trigger display of the selection
  // window for Named Anchors, which don't have any children and
  // normally don't have any size, but in Editor we use CSS to display
  // an image to represent this "hidden" element.
  if (!mFrames.FirstChild()) {
    rv = DisplaySelectionOverlay(aBuilder, aLists);
  }
  return rv;
}

//////////////////////////////////////////////////////////////////////
// Reflow methods

/* virtual */ void
nsInlineFrame::AddInlineMinWidth(nsIRenderingContext *aRenderingContext,
                                 nsIFrame::InlineMinWidthData *aData)
{
  DoInlineIntrinsicWidth(aRenderingContext, aData, nsLayoutUtils::MIN_WIDTH);
}

/* virtual */ void
nsInlineFrame::AddInlinePrefWidth(nsIRenderingContext *aRenderingContext,
                                  nsIFrame::InlinePrefWidthData *aData)
{
  DoInlineIntrinsicWidth(aRenderingContext, aData, nsLayoutUtils::PREF_WIDTH);
}

/* virtual */ nsSize
nsInlineFrame::ComputeSize(nsIRenderingContext *aRenderingContext,
                           nsSize aCBSize, nscoord aAvailableWidth,
                           nsSize aMargin, nsSize aBorder, nsSize aPadding,
                           PRBool aShrinkWrap)
{
  // Inlines and text don't compute size before reflow.
  return nsSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
}

NS_IMETHODIMP
nsInlineFrame::Reflow(nsPresContext*          aPresContext,
                      nsHTMLReflowMetrics&     aMetrics,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsInlineFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aMetrics, aStatus);
  if (nsnull == aReflowState.mLineLayout) {
    return NS_ERROR_INVALID_ARG;
  }

  PRBool  lazilySetParentPointer = PR_FALSE;

  // Check for an overflow list with our prev-in-flow
  nsInlineFrame* prevInFlow = (nsInlineFrame*)GetPrevInFlow();
  if (nsnull != prevInFlow) {
    nsIFrame* prevOverflowFrames = prevInFlow->GetOverflowFrames(aPresContext, PR_TRUE);

    if (prevOverflowFrames) {
      // When pushing and pulling frames we need to check for whether any
      // views need to be reparented.
      nsHTMLContainerFrame::ReparentFrameViewList(aPresContext, prevOverflowFrames,
                                                  prevInFlow, this);

      if (GetStateBits() & NS_FRAME_FIRST_REFLOW) {
        // If it's the initial reflow, then our child list must be empty, so
        // just set the child list rather than calling InsertFrame(). This avoids
        // having to get the last child frame in the list.
        // Note that we don't set the parent pointer for the new frames. Instead wait
        // to do this until we actually reflow the frame. If the overflow list contains
        // thousands of frames this is a big performance issue (see bug #5588)
        NS_ASSERTION(mFrames.IsEmpty(), "child list is not empty for initial reflow");
        mFrames.SetFrames(prevOverflowFrames);
        lazilySetParentPointer = PR_TRUE;

      } else {
        // Insert the new frames at the beginning of the child list
        // and set their parent pointer
        mFrames.InsertFrames(this, nsnull, prevOverflowFrames);
      }
    }
  }

  // It's also possible that we have an overflow list for ourselves
#ifdef DEBUG
  if (GetStateBits() & NS_FRAME_FIRST_REFLOW) {
    // If it's our initial reflow, then we should not have an overflow list.
    // However, add an assertion in case we get reflowed more than once with
    // the initial reflow reason
    nsIFrame* overflowFrames = GetOverflowFrames(aPresContext, PR_FALSE);
    NS_ASSERTION(!overflowFrames, "overflow list is not empty for initial reflow");
  }
#endif
  if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    nsIFrame* overflowFrames = GetOverflowFrames(aPresContext, PR_TRUE);
    if (overflowFrames) {
      NS_ASSERTION(mFrames.NotEmpty(), "overflow list w/o frames");

      // Because we lazily set the parent pointer of child frames we get from
      // our prev-in-flow's overflow list, it's possible that we have not set
      // the parent pointer for these frames.
      mFrames.AppendFrames(this, overflowFrames);
    }
  }

  if (IsFrameTreeTooDeep(aReflowState, aMetrics)) {
#ifdef DEBUG_kipp
    {
      extern char* nsPresShell_ReflowStackPointerTop;
      char marker;
      char* newsp = (char*) &marker;
      printf("XXX: frame tree is too deep; approx stack size = %d\n",
             nsPresShell_ReflowStackPointerTop - newsp);
    }
#endif
    aStatus = NS_FRAME_COMPLETE;
    return NS_OK;
  }

  // Set our own reflow state (additional state above and beyond
  // aReflowState)
  InlineReflowState irs;
  irs.mPrevFrame = nsnull;
  irs.mNextInFlow = (nsInlineFrame*) GetNextInFlow();
  irs.mSetParentPointer = lazilySetParentPointer;

  nsresult rv;
  if (mFrames.IsEmpty()) {
    // Try to pull over one frame before starting so that we know
    // whether we have an anonymous block or not.
    PRBool complete;
    (void) PullOneFrame(aPresContext, irs, &complete);
  }

  rv = ReflowFrames(aPresContext, aReflowState, irs, aMetrics, aStatus);
  
  // Note: the line layout code will properly compute our
  // NS_FRAME_OUTSIDE_CHILDREN state for us.

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aMetrics);
  return rv;
}

/* virtual */ PRBool
nsInlineFrame::CanContinueTextRun() const
{
  // We can continue a text run through an inline frame
  return PR_TRUE;
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
  PRBool ltr = (NS_STYLE_DIRECTION_LTR == aReflowState.mStyleVisibility->mDirection);
  nscoord leftEdge = 0;
  if (nsnull == GetPrevContinuation()) {
    leftEdge = ltr ? aReflowState.mComputedBorderPadding.left
                   : aReflowState.mComputedBorderPadding.right;
  }
  nscoord availableWidth = aReflowState.availableWidth;
  if (NS_UNCONSTRAINEDSIZE != availableWidth) {
    // Subtract off left and right border+padding from availableWidth
    availableWidth -= leftEdge;
    availableWidth -= ltr ? aReflowState.mComputedBorderPadding.right
                          : aReflowState.mComputedBorderPadding.left;
    availableWidth = PR_MAX(0, availableWidth);
  }
  lineLayout->BeginSpan(this, &aReflowState, leftEdge, leftEdge + availableWidth);

  // First reflow our current children
  nsIFrame* frame = mFrames.FirstChild();
  PRBool done = PR_FALSE;
  while (nsnull != frame) {
    PRBool reflowingFirstLetter = lineLayout->GetFirstLetterStyleOK();

    // Check if we should lazily set the child frame's parent pointer
    if (irs.mSetParentPointer) {
      frame->SetParent(this);

      // We also need to check if frame has a next-in-flow. If it does, then set
      // its parent frame pointer, too. Otherwise, if we reflow frame and it's
      // complete we'll fail when deleting its next-in-flow which is no longer
      // needed. This scenario doesn't happen often, but it can happen
      nsIFrame* nextInFlow = frame->GetNextInFlow();
      while (nextInFlow) {
        // Since we only do lazy setting of parent pointers for the frame's
        // initial reflow, this frame can't have a next-in-flow. That means
        // the continuing child frame must be in our child list as well. If
        // not, then something is wrong
        NS_ASSERTION(mFrames.ContainsFrame(nextInFlow), "unexpected flow");
        nextInFlow->SetParent(this);
        nextInFlow = nextInFlow->GetNextInFlow();
      }
    }
    rv = ReflowInlineFrame(aPresContext, aReflowState, irs, frame, aStatus);
    if (NS_FAILED(rv)) {
      done = PR_TRUE;
      break;
    }
    if (NS_INLINE_IS_BREAK(aStatus) || 
        (!reflowingFirstLetter && NS_FRAME_IS_NOT_COMPLETE(aStatus))) {
      done = PR_TRUE;
      break;
    }
    irs.mPrevFrame = frame;
    frame = frame->GetNextSibling();
  }

  // Attempt to pull frames from our next-in-flow until we can't
  if (!done && (nsnull != GetNextInFlow())) {
    while (!done) {
      PRBool reflowingFirstLetter = lineLayout->GetFirstLetterStyleOK();
      PRBool isComplete;
      frame = PullOneFrame(aPresContext, irs, &isComplete);
#ifdef NOISY_PUSHING
      printf("%p pulled up %p\n", this, frame);
#endif
      if (nsnull == frame) {
        if (!isComplete) {
          aStatus = NS_FRAME_NOT_COMPLETE;
        }
        break;
      }
      rv = ReflowInlineFrame(aPresContext, aReflowState, irs, frame, aStatus);
      if (NS_FAILED(rv)) {
        done = PR_TRUE;
        break;
      }
      if (NS_INLINE_IS_BREAK(aStatus) || 
          (!reflowingFirstLetter && NS_FRAME_IS_NOT_COMPLETE(aStatus))) {
        done = PR_TRUE;
        break;
      }
      irs.mPrevFrame = frame;
    }
  }
#ifdef DEBUG
  if (NS_FRAME_IS_COMPLETE(aStatus)) {
    // We can't be complete AND have overflow frames!
    nsIFrame* overflowFrames = GetOverflowFrames(aPresContext, PR_FALSE);
    NS_ASSERTION(!overflowFrames, "whoops");
  }
#endif

  // If after reflowing our children they take up no area then make
  // sure that we don't either.
  //
  // Note: CSS demands that empty inline elements still affect the
  // line-height calculations. However, continuations of an inline
  // that are empty we force to empty so that things like collapsed
  // whitespace in an inline element don't affect the line-height.
  nsSize size;
  lineLayout->EndSpan(this, size);
  if ((0 == size.height) && (0 == size.width) &&
      ((nsnull != GetPrevInFlow()) || (nsnull != GetNextInFlow()))) {
    // This is a continuation of a previous inline. Therefore make
    // sure we don't affect the line-height.
    aMetrics.width = 0;
    aMetrics.height = 0;
    aMetrics.ascent = 0;
  }
  else {
    // Compute final width
    aMetrics.width = size.width;
    if (nsnull == GetPrevContinuation()) {
      aMetrics.width += ltr ? aReflowState.mComputedBorderPadding.left
                            : aReflowState.mComputedBorderPadding.right;
    }
    if (NS_FRAME_IS_COMPLETE(aStatus) && (!GetNextContinuation() || GetNextInFlow())) {
      aMetrics.width += ltr ? aReflowState.mComputedBorderPadding.right
                            : aReflowState.mComputedBorderPadding.left;
    }

    SetFontFromStyle(aReflowState.rendContext, mStyleContext);
    nsCOMPtr<nsIFontMetrics> fm;
    aReflowState.rendContext->GetFontMetrics(*getter_AddRefs(fm));

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
      fm->GetMaxAscent(aMetrics.ascent);
      fm->GetHeight(aMetrics.height);
    } else {
      NS_WARNING("Cannot get font metrics - defaulting sizes to 0");
      aMetrics.ascent = aMetrics.height = 0;
    }
    aMetrics.ascent += aReflowState.mComputedBorderPadding.top;
    aMetrics.height += aReflowState.mComputedBorderPadding.top +
      aReflowState.mComputedBorderPadding.bottom;
  }

  // For now our overflow area is zero. The real value will be
  // computed during vertical alignment of the line we are on.
  aMetrics.mOverflowArea.SetRect(0, 0, 0, 0);

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
  PRBool reflowingFirstLetter = lineLayout->GetFirstLetterStyleOK();
  PRBool pushedFrame;
  nsresult rv =
    lineLayout->ReflowFrame(aFrame, aStatus, nsnull, pushedFrame);
  
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (NS_INLINE_IS_BREAK(aStatus)) {
    if (NS_INLINE_IS_BREAK_BEFORE(aStatus)) {
      if (aFrame != mFrames.FirstChild()) {
        // Change break-before status into break-after since we have
        // already placed at least one child frame. This preserves the
        // break-type so that it can be propagated upward.
        aStatus = NS_FRAME_NOT_COMPLETE |
          NS_INLINE_BREAK | NS_INLINE_BREAK_AFTER |
          (aStatus & NS_INLINE_BREAK_TYPE_MASK);
        PushFrames(aPresContext, aFrame, irs.mPrevFrame);
      }
      else {
        // Preserve reflow status when breaking-before our first child
        // and propagate it upward without modification.
        // Note: if we're lazily setting the frame pointer for our child 
        // frames, then we need to set it now. Don't return and leave the
        // remaining child frames in our child list with the wrong parent
        // frame pointer...
        if (irs.mSetParentPointer) {
          for (nsIFrame* f = aFrame->GetNextSibling(); f; f = f->GetNextSibling()) {
            f->SetParent(this);
          }
        }
      }
    }
    else {
      // Break-after
      if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
        nsIFrame* newFrame;
        rv = CreateNextInFlow(aPresContext, this, aFrame, newFrame);
        if (NS_FAILED(rv)) {
          return rv;
        }
      }
      nsIFrame* nextFrame = aFrame->GetNextSibling();
      if (nextFrame) {
        aStatus |= NS_FRAME_NOT_COMPLETE;
        PushFrames(aPresContext, nextFrame, aFrame);
      }
      else if (nsnull != GetNextInFlow()) {
        // We must return an incomplete status if there are more child
        // frames remaining in a next-in-flow that follows this frame.
        nsInlineFrame* nextInFlow = (nsInlineFrame*) GetNextInFlow();
        while (nsnull != nextInFlow) {
          if (nextInFlow->mFrames.NotEmpty()) {
            aStatus |= NS_FRAME_NOT_COMPLETE;
            break;
          }
          nextInFlow = (nsInlineFrame*) nextInFlow->GetNextInFlow();
        }
      }
    }
  }
  else if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
    if (nsGkAtoms::placeholderFrame == aFrame->GetType()) {
      nsBlockReflowState* blockRS = lineLayout->mBlockRS;
      blockRS->mBlock->SplitPlaceholder(*blockRS, aFrame);
      // Allow the parent to continue reflowing
      aStatus = NS_FRAME_COMPLETE;
    }
    else {
      nsIFrame* newFrame;
      rv = CreateNextInFlow(aPresContext, this, aFrame, newFrame);
      if (NS_FAILED(rv)) {
        return rv;
      }
      if (!reflowingFirstLetter) {
        nsIFrame* nextFrame = aFrame->GetNextSibling();
        if (nextFrame) {
          PushFrames(aPresContext, nextFrame, aFrame);
        }
      }
    }
  }
  return rv;
}

nsIFrame*
nsInlineFrame::PullOneFrame(nsPresContext* aPresContext,
                            InlineReflowState& irs,
                            PRBool* aIsComplete)
{
  PRBool isComplete = PR_TRUE;

  nsIFrame* frame = nsnull;
  nsInlineFrame* nextInFlow = irs.mNextInFlow;
  while (nsnull != nextInFlow) {
    frame = mFrames.PullFrame(this, irs.mPrevFrame, nextInFlow->mFrames);
    if (nsnull != frame) {
      isComplete = PR_FALSE;
      nsHTMLContainerFrame::ReparentFrameView(aPresContext, frame, nextInFlow, this);
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
                          nsIFrame* aPrevSibling)
{
  NS_PRECONDITION(nsnull != aFromChild, "null pointer");
  NS_PRECONDITION(nsnull != aPrevSibling, "pushing first child");
  NS_PRECONDITION(aPrevSibling->GetNextSibling() == aFromChild, "bad prev sibling");

#ifdef NOISY_PUSHING
      printf("%p pushing aFromChild %p, disconnecting from prev sib %p\n", 
             this, aFromChild, aPrevSibling);
#endif
  // Disconnect aFromChild from its previous sibling
  aPrevSibling->SetNextSibling(nsnull);

  // Add the frames to our overflow list (let our next in flow drain
  // our overflow list when it is ready)
  SetOverflowFrames(aPresContext, aFromChild);
}


//////////////////////////////////////////////////////////////////////

PRIntn
nsInlineFrame::GetSkipSides() const
{
  PRIntn skip = 0;
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
  return skip;
}

#ifdef ACCESSIBILITY
NS_IMETHODIMP nsInlineFrame::GetAccessible(nsIAccessible** aAccessible)
{
  // Broken image accessibles are created here, because layout
  // replaces the image or image control frame with an inline frame
  *aAccessible = nsnull;
  nsIAtom *tagAtom = mContent->Tag();
  if ((tagAtom == nsGkAtoms::img || tagAtom == nsGkAtoms::input || 
       tagAtom == nsGkAtoms::label) && mContent->IsNodeOfType(nsINode::eHTML)) {
    // Only get accessibility service if we're going to use it
    nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));
    if (!accService)
      return NS_ERROR_FAILURE;
    if (tagAtom == nsGkAtoms::input)  // Broken <input type=image ... />
      return accService->CreateHTMLButtonAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
    else if (tagAtom == nsGkAtoms::img)  // Create accessible for broken <img>
      return accService->CreateHTMLImageAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
    else if (tagAtom == nsGkAtoms::label)  // Creat accessible for <label>
      return accService->CreateHTMLLabelAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
  }

  return NS_ERROR_FAILURE;
}
#endif

//////////////////////////////////////////////////////////////////////

// nsLineFrame implementation

static void
ReParentChildListStyle(nsPresContext* aPresContext,
                       nsFrameList& aFrameList,
                       nsIFrame* aParentFrame)
{
  nsFrameManager *frameManager = aPresContext->FrameManager();

  for (nsIFrame* kid = aFrameList.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    NS_ASSERTION(kid->GetParent() == aParentFrame, "Bogus parentage");
    frameManager->ReParentStyleContext(kid);
  }
}

nsIFrame*
NS_NewFirstLineFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsFirstLineFrame(aContext);
}

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

void
nsFirstLineFrame::StealFramesFrom(nsIFrame* aFrame)
{
  nsIFrame* prevFrame = mFrames.GetPrevSiblingFor(aFrame);
  if (prevFrame) {
    prevFrame->SetNextSibling(nsnull);
  }
  else {
    mFrames.SetFrames(nsnull);
  }
}

nsIFrame*
nsFirstLineFrame::PullOneFrame(nsPresContext* aPresContext, InlineReflowState& irs, PRBool* aIsComplete)
{
  nsIFrame* frame = nsInlineFrame::PullOneFrame(aPresContext, irs, aIsComplete);
  if (frame && !GetPrevInFlow()) {
    // We are a first-line frame. Fixup the child frames
    // style-context that we just pulled.
    NS_ASSERTION(frame->GetParent() == this, "Incorrect parent?");
    aPresContext->FrameManager()->ReParentStyleContext(frame);
  }
  return frame;
}

NS_IMETHODIMP
nsFirstLineFrame::Reflow(nsPresContext* aPresContext,
                         nsHTMLReflowMetrics& aMetrics,
                         const nsHTMLReflowState& aReflowState,
                         nsReflowStatus& aStatus)
{
  if (nsnull == aReflowState.mLineLayout) {
    return NS_ERROR_INVALID_ARG;
  }

  // Check for an overflow list with our prev-in-flow
  nsFirstLineFrame* prevInFlow = (nsFirstLineFrame*)GetPrevInFlow();
  if (nsnull != prevInFlow) {
    nsIFrame* prevOverflowFrames = prevInFlow->GetOverflowFrames(aPresContext, PR_TRUE);
    if (prevOverflowFrames) {
      nsFrameList frames(prevOverflowFrames);
      
      mFrames.InsertFrames(this, nsnull, prevOverflowFrames);
      ReParentChildListStyle(aPresContext, frames, this);
    }
  }

  // It's also possible that we have an overflow list for ourselves
  nsIFrame* overflowFrames = GetOverflowFrames(aPresContext, PR_TRUE);
  if (overflowFrames) {
    NS_ASSERTION(mFrames.NotEmpty(), "overflow list w/o frames");
    nsFrameList frames(overflowFrames);

    mFrames.AppendFrames(nsnull, overflowFrames);
    ReParentChildListStyle(aPresContext, frames, this);
  }

  // Set our own reflow state (additional state above and beyond
  // aReflowState)
  InlineReflowState irs;
  irs.mPrevFrame = nsnull;
  irs.mNextInFlow = (nsInlineFrame*) GetNextInFlow();

  nsresult rv;
  PRBool wasEmpty = mFrames.IsEmpty();
  if (wasEmpty) {
    // Try to pull over one frame before starting so that we know
    // whether we have an anonymous block or not.
    PRBool complete;
    PullOneFrame(aPresContext, irs, &complete);
  }

  if (nsnull == GetPrevInFlow()) {
    // XXX This is pretty sick, but what we do here is to pull-up, in
    // advance, all of the next-in-flows children. We re-resolve their
    // style while we are at at it so that when we reflow they have
    // the right style.
    //
    // All of this is so that text-runs reflow properly.
    irs.mPrevFrame = mFrames.LastChild();
    for (;;) {
      PRBool complete;
      nsIFrame* frame = PullOneFrame(aPresContext, irs, &complete);
      if (!frame) {
        break;
      }
      irs.mPrevFrame = frame;
    }
    irs.mPrevFrame = nsnull;
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
          ResolvePseudoStyleFor(nsnull,
                                nsCSSAnonBoxes::mozLineFrame, parentContext);
        if (newSC) {
          // Switch to the new style context.
          SetStyleContext(newSC);

          // Re-resolve all children
          ReParentChildListStyle(aPresContext, mFrames, this);
        }
      }
    }
  }

  NS_ASSERTION(!aReflowState.mLineLayout->GetInFirstLine(),
               "Nested first-line frames? BOGUS");
  aReflowState.mLineLayout->SetInFirstLine(PR_TRUE);
  rv = ReflowFrames(aPresContext, aReflowState, irs, aMetrics, aStatus);
  aReflowState.mLineLayout->SetInFirstLine(PR_FALSE);

  // Note: the line layout code will properly compute our overflow state for us

  return rv;
}

//////////////////////////////////////////////////////////////////////

nsIFrame*
NS_NewPositionedInlineFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsPositionedInlineFrame(aContext);
}

void
nsPositionedInlineFrame::Destroy()
{
  mAbsoluteContainer.DestroyFrames(this);
  nsInlineFrame::Destroy();
}

NS_IMETHODIMP
nsPositionedInlineFrame::SetInitialChildList(nsIAtom*        aListName,
                                             nsIFrame*       aChildList)
{
  nsresult  rv;

  if (mAbsoluteContainer.GetChildListName() == aListName) {
    rv = mAbsoluteContainer.SetInitialChildList(this, aListName, aChildList);
  } else {
    rv = nsInlineFrame::SetInitialChildList(aListName, aChildList);
  }

  return rv;
}

NS_IMETHODIMP
nsPositionedInlineFrame::AppendFrames(nsIAtom*        aListName,
                                      nsIFrame*       aFrameList)
{
  nsresult  rv;
  
  if (mAbsoluteContainer.GetChildListName() == aListName) {
    rv = mAbsoluteContainer.AppendFrames(this, aListName, aFrameList);
  } else {
    rv = nsInlineFrame::AppendFrames(aListName, aFrameList);
  }

  return rv;
}
  
NS_IMETHODIMP
nsPositionedInlineFrame::InsertFrames(nsIAtom*        aListName,
                                      nsIFrame*       aPrevFrame,
                                      nsIFrame*       aFrameList)
{
  nsresult  rv;

  if (mAbsoluteContainer.GetChildListName() == aListName) {
    rv = mAbsoluteContainer.InsertFrames(this, aListName, aPrevFrame,
                                         aFrameList);
  } else {
    rv = nsInlineFrame::InsertFrames(aListName, aPrevFrame, aFrameList);
  }

  return rv;
}
  
NS_IMETHODIMP
nsPositionedInlineFrame::RemoveFrame(nsIAtom*        aListName,
                                     nsIFrame*       aOldFrame)
{
  nsresult  rv;

  if (mAbsoluteContainer.GetChildListName() == aListName) {
    rv = mAbsoluteContainer.RemoveFrame(this, aListName, aOldFrame);
  } else {
    rv = nsInlineFrame::RemoveFrame(aListName, aOldFrame);
  }

  return rv;
}

NS_IMETHODIMP
nsPositionedInlineFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                          const nsRect&           aDirtyRect,
                                          const nsDisplayListSet& aLists)
{
  aBuilder->MarkFramesForDisplayList(this, mAbsoluteContainer.GetFirstChild(), aDirtyRect);
  return nsHTMLContainerFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);
}

nsIAtom*
nsPositionedInlineFrame::GetAdditionalChildListName(PRInt32 aIndex) const
{
  if (0 == aIndex) {
    return mAbsoluteContainer.GetChildListName();
  }
  return nsnull;
}

nsIFrame*
nsPositionedInlineFrame::GetFirstChild(nsIAtom* aListName) const
{
  if (mAbsoluteContainer.GetChildListName() == aListName) {
    nsIFrame* result = nsnull;
    mAbsoluteContainer.FirstChild(this, aListName, &result);
    return result;
  }

  return nsInlineFrame::GetFirstChild(aListName);
}

nsIAtom*
nsPositionedInlineFrame::GetType() const
{
  return nsGkAtoms::positionedInlineFrame;
}

NS_IMETHODIMP
nsPositionedInlineFrame::Reflow(nsPresContext*          aPresContext,
                                nsHTMLReflowMetrics&     aDesiredSize,
                                const nsHTMLReflowState& aReflowState,
                                nsReflowStatus&          aStatus)
{
  nsresult  rv = NS_OK;

  // Don't bother optimizing for fast incremental reflow of absolute
  // children of an inline

  // Let the inline frame do its reflow first
  rv = nsInlineFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

  // Let the absolutely positioned container reflow any absolutely positioned
  // child frames that need to be reflowed
  // We want to do this under either of two conditions:
  //  1. If we didn't do the incremental reflow above.
  //  2. If our size changed.
  // Even though it's the padding edge that's the containing block, we
  // can use our rect (the border edge) since if the border style
  // changed, the reflow would have been targeted at us so we'd satisfy
  // condition 1.
  if (NS_SUCCEEDED(rv) &&
      mAbsoluteContainer.HasAbsoluteFrames()) {
    // The containing block for the abs pos kids is formed by our padding edge.
    nsMargin computedBorder =
      aReflowState.mComputedBorderPadding - aReflowState.mComputedPadding;
    nscoord containingBlockWidth =
      aDesiredSize.width - computedBorder.LeftRight();
    nscoord containingBlockHeight =
      aDesiredSize.height - computedBorder.TopBottom();

    // Factor the absolutely positioned child bounds into the overflow area
    // Don't include this frame's bounds, nor its inline descendants' bounds,
    // and don't store the overflow property.
    // That will all be done by nsLineLayout::RelativePositionFrames.
    rv = mAbsoluteContainer.Reflow(this, aPresContext, aReflowState,
                                   containingBlockWidth, containingBlockHeight,
                                   PR_TRUE, PR_TRUE, // XXX could be optimized
                                   &aDesiredSize.mOverflowArea);
  }

  return rv;
}
