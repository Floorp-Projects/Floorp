/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsCOMPtr.h"
#include "nsInlineFrame.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLParts.h"
#include "nsIStyleContext.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsAbsoluteContainingBlock.h"
#include "nsLayoutAtoms.h"

#ifdef DEBUG
#undef NOISY_PUSHING
#endif

nsIID nsInlineFrame::kInlineFrameCID = NS_INLINE_FRAME_CID;





//////////////////////////////////////////////////////////////////////

// Basic nsInlineFrame methods

nsresult
NS_NewInlineFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsInlineFrame* it = new (aPresShell) nsInlineFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsInlineFrame::nsInlineFrame()
{
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

#ifdef DEBUG
NS_IMETHODIMP
nsInlineFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Inline", aResult);
}
#endif

NS_IMETHODIMP
nsInlineFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::inlineFrame;
  NS_ADDREF(*aType);
  return NS_OK;
}

NS_IMETHODIMP
nsInlineFrame::AppendFrames(nsIPresContext* aPresContext,
                            nsIPresShell& aPresShell,
                            nsIAtom* aListName,
                            nsIFrame* aFrameList)
{
  if (nsnull != aListName) {
    return NS_ERROR_INVALID_ARG;
  }
  if (aFrameList) {
    mFrames.AppendFrames(this, aFrameList);

    // Ask the parent frame to reflow me.
    ReflowDirtyChild(&aPresShell, nsnull);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsInlineFrame::InsertFrames(nsIPresContext* aPresContext,
                            nsIPresShell& aPresShell,
                            nsIAtom* aListName,
                            nsIFrame* aPrevFrame,
                            nsIFrame* aFrameList)
{
  if (nsnull != aListName) {
#ifdef IBMBIDI
    if (aListName != nsLayoutAtoms::nextBidi)
#endif
    return NS_ERROR_INVALID_ARG;
  }
  if (aFrameList) {
    // Insert frames after aPrevFrame
    mFrames.InsertFrames(this, aPrevFrame, aFrameList);

#ifdef IBMBIDI
    if (nsnull == aListName)
#endif
    // Ask the parent frame to reflow me.
    ReflowDirtyChild(&aPresShell, nsnull);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsInlineFrame::RemoveFrame(nsIPresContext* aPresContext,
                           nsIPresShell& aPresShell,
                           nsIAtom* aListName,
                           nsIFrame* aOldFrame)
{
  if (nsnull != aListName) {
#ifdef IBMBIDI
    if (nsLayoutAtoms::nextBidi != aListName)
#endif
    return NS_ERROR_INVALID_ARG;
  }

  if (aOldFrame) {
    // Loop and destroy the frame and all of its continuations.
    PRBool generateReflowCommand = PR_FALSE;
    nsIFrame* oldFrameParent;
    aOldFrame->GetParent(&oldFrameParent);
    nsInlineFrame* parent = (nsInlineFrame*) oldFrameParent;
    while (nsnull != aOldFrame) {
#ifdef IBMBIDI
      if (nsLayoutAtoms::nextBidi != aListName) {
#endif
      // If the frame being removed has zero size then don't bother
      // generating a reflow command, otherwise make sure we do.
      nsRect bbox;
      aOldFrame->GetRect(bbox);
      if (bbox.width || bbox.height) {
        generateReflowCommand = PR_TRUE;
      }
#ifdef IBMBIDI
      }
#endif

      // When the parent is an inline frame we have a simple task - just
      // remove the frame from its parents list and generate a reflow
      // command.
      nsIFrame* oldFrameNextInFlow;
      aOldFrame->GetNextInFlow(&oldFrameNextInFlow);
      nsSplittableType st;
      aOldFrame->IsSplittable(st);
      if (NS_FRAME_NOT_SPLITTABLE != st) {
        nsSplittableFrame::RemoveFromFlow(aOldFrame);
      }
      parent->mFrames.DestroyFrame(aPresContext, aOldFrame);
      aOldFrame = oldFrameNextInFlow;
      if (nsnull != aOldFrame) {
        aOldFrame->GetParent((nsIFrame**) &parent);
      }
    }

    if (generateReflowCommand) {
      // Ask the parent frame to reflow me.
      ReflowDirtyChild(&aPresShell, nsnull);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsInlineFrame::ReplaceFrame(nsIPresContext* aPresContext,
                            nsIPresShell& aPresShell,
                            nsIAtom* aListName,
                            nsIFrame* aOldFrame,
                            nsIFrame* aNewFrame)
{
  if (nsnull != aListName) {
    return NS_ERROR_INVALID_ARG;
  }
  if (!aOldFrame || !aNewFrame) {
    return NS_ERROR_INVALID_ARG;
  }

  // Replace the old frame with the new frame in the list, then remove the old frame
  mFrames.ReplaceFrame(this, aOldFrame, aNewFrame);
  aOldFrame->Destroy(aPresContext);

  // Ask the parent frame to reflow me.
  ReflowDirtyChild(&aPresShell, nsnull);

  return NS_OK;
}


//////////////////////////////////////////////////////////////////////
// Reflow methods

NS_IMETHODIMP
nsInlineFrame::Reflow(nsIPresContext*          aPresContext,
                      nsHTMLReflowMetrics&     aMetrics,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsInlineFrame", aReflowState.reason);
  if (nsnull == aReflowState.mLineLayout) {
    return NS_ERROR_INVALID_ARG;
  }

  PRBool  lazilySetParentPointer = PR_FALSE;

  // Check for an overflow list with our prev-in-flow
  nsInlineFrame* prevInFlow = (nsInlineFrame*)mPrevInFlow;
  if (nsnull != prevInFlow) {
    nsIFrame* prevOverflowFrames = prevInFlow->GetOverflowFrames(aPresContext, PR_TRUE);

    if (prevOverflowFrames) {
      // When pushing and pulling frames we need to check for whether any
      // views need to be reparented.
      nsHTMLContainerFrame::ReparentFrameViewList(aPresContext, prevOverflowFrames,
                                                  prevInFlow, this);

      if (aReflowState.reason == eReflowReason_Initial) {
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
  if (aReflowState.reason == eReflowReason_Initial) {
    // If it's our initial reflow, then we should not have an overflow list.
    // However, add an assertion in case we get reflowed more than once with
    // the initial reflow reason
    nsIFrame* overflowFrames = GetOverflowFrames(aPresContext, PR_FALSE);
    NS_ASSERTION(!overflowFrames, "overflow list is not empty for initial reflow");
  }
#endif
  if (aReflowState.reason != eReflowReason_Initial) {
    nsIFrame* overflowFrames = GetOverflowFrames(aPresContext, PR_TRUE);
    if (overflowFrames) {
      NS_ASSERTION(mFrames.NotEmpty(), "overflow list w/o frames");

      // Because we lazily set the parent pointer of child frames we get from
      // our prev-in-flow's overflow list, it's possible that we have not set
      // the parent pointer for these frames. Check the first frame to see, and
      // if we haven't set the parent pointer then set it now
      nsIFrame* parent;
      overflowFrames->GetParent(&parent);
      mFrames.AppendFrames(parent == this ? nsnull : this, overflowFrames);
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
  irs.mNextInFlow = (nsInlineFrame*) mNextInFlow;
  irs.mNextRCFrame = nsnull;
  irs.mSetParentPointer = lazilySetParentPointer;
  if (eReflowReason_Incremental == aReflowState.reason) {
    // Peel off the next frame in the path if this is an incremental
    // reflow aimed at one of the children.
    nsIFrame* target;
    aReflowState.reflowCommand->GetTarget(target);
    if (this != target) {
      aReflowState.reflowCommand->GetNext(irs.mNextRCFrame);
    }
  }

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

  return rv;
}

NS_IMETHODIMP
nsInlineFrame::CanContinueTextRun(PRBool& aContinueTextRun) const
{
  // We can continue a text run through an inline frame
  aContinueTextRun = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsInlineFrame::ReflowDirtyChild(nsIPresShell* aPresShell, nsIFrame* aChild)
{
  // The inline container frame does not handle the reflow
  // request.  It passes it up to its parent container.

  // If you don't already have dirty children,
  if (!(mState & NS_FRAME_HAS_DIRTY_CHILDREN)) {
    if (mParent) {
      // Record that you are dirty and have dirty children
      mState |= NS_FRAME_IS_DIRTY;
      mState |= NS_FRAME_HAS_DIRTY_CHILDREN; 

      // Pass the reflow request up to the parent
      mParent->ReflowDirtyChild(aPresShell, this);
    }
    else {
      NS_ERROR("No parent to pass the reflow request up to.");
    }
  }

  return NS_OK;
}

nsresult
nsInlineFrame::ReflowFrames(nsIPresContext* aPresContext,
                            const nsHTMLReflowState& aReflowState,
                            InlineReflowState& irs,
                            nsHTMLReflowMetrics& aMetrics,
                            nsReflowStatus& aStatus)
{
  nsresult rv = NS_OK;
  aStatus = NS_FRAME_COMPLETE;

  nsLineLayout* lineLayout = aReflowState.mLineLayout;
  nscoord leftEdge = 0;
  if (nsnull == mPrevInFlow) {
    leftEdge = aReflowState.mComputedBorderPadding.left;
  }
  nscoord availableWidth = aReflowState.availableWidth;
  if (NS_UNCONSTRAINEDSIZE != availableWidth) {
    // Subtract off left and right border+padding from availableWidth
    availableWidth -= leftEdge;
    availableWidth -= aReflowState.mComputedBorderPadding.right;
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

      // We also need to check if frame has a next-in-flow. It it does, then set
      // its parent frame pointer, too. Otherwise, if we reflow frame and it's
      // complete we'll fail when deleting its next-in-flow which is no longer
      // needed. This scenario doesn't happen often, but it can happen
      nsIFrame* nextInFlow;
      frame->GetNextInFlow(&nextInFlow);
      while (nextInFlow) {
        // Since we only do lazy setting of parent pointers for the frame's
        // initial reflow, this frame can't have a next-in-flow. That means
        // the continuing child frame must be in our child list as well. If
        // not, then something is wrong
        NS_ASSERTION(mFrames.ContainsFrame(nextInFlow), "unexpected flow");
        nextInFlow->SetParent(this);
        nextInFlow->GetNextInFlow(&nextInFlow);
      }
    }
    rv = ReflowInlineFrame(aPresContext, aReflowState, irs, frame, aStatus);
    if (NS_FAILED(rv)) {
      done = PR_TRUE;
      break;
    }
    if (NS_FRAME_COMPLETE != aStatus) {
      if (!reflowingFirstLetter || NS_INLINE_IS_BREAK(aStatus)) {
        done = PR_TRUE;
        break;
      }
    }
    irs.mPrevFrame = frame;
    frame->GetNextSibling(&frame);
  }

  // Attempt to pull frames from our next-in-flow until we can't
  if (!done && (nsnull != mNextInFlow)) {
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
      if (NS_FRAME_COMPLETE != aStatus) {
        if (!reflowingFirstLetter || NS_INLINE_IS_BREAK(aStatus)) {
          done = PR_TRUE;
          break;
        }
      }
      irs.mPrevFrame = frame;
    }
  }
#ifdef DEBUG
  if (NS_FRAME_COMPLETE == aStatus) {
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
  lineLayout->EndSpan(this, size, aMetrics.maxElementSize);
  if ((0 == size.height) && (0 == size.width) &&
      ((nsnull != mPrevInFlow) || (nsnull != mNextInFlow))) {
    // This is a continuation of a previous inline. Therefore make
    // sure we don't affect the line-height.
    aMetrics.width = 0;
    aMetrics.height = 0;
    aMetrics.ascent = 0;
    aMetrics.descent = 0;
    if (nsnull != aMetrics.maxElementSize) {
      aMetrics.maxElementSize->width = 0;
      aMetrics.maxElementSize->height = 0;
    }
  }
  else {
    // Compute final width
    aMetrics.width = size.width;
    if (nsnull == mPrevInFlow) {
      aMetrics.width += aReflowState.mComputedBorderPadding.left;
    }
    if (NS_FRAME_IS_COMPLETE(aStatus)) {
      aMetrics.width += aReflowState.mComputedBorderPadding.right;
    }

    const nsStyleFont* font;
    GetStyleData(eStyleStruct_Font, (const nsStyleStruct*&)font);
    aReflowState.rendContext->SetFont(font->mFont);
    nsCOMPtr<nsIFontMetrics> fm;
    aReflowState.rendContext->GetFontMetrics(*getter_AddRefs(fm));

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
    fm->GetMaxDescent(aMetrics.descent);
    fm->GetHeight(aMetrics.height);
    aMetrics.ascent += aReflowState.mComputedBorderPadding.top;
    aMetrics.descent += aReflowState.mComputedBorderPadding.bottom;
    aMetrics.height += aReflowState.mComputedBorderPadding.top +
      aReflowState.mComputedBorderPadding.bottom;

    // Note: we normally use the actual font height for computing the
    // line-height raw value from the style context. On systems where
    // they disagree the actual font height is more appropriate. This
    // little hack lets us override that behavior to allow for more
    // precise layout in the face of imprecise fonts.
    if (nsHTMLReflowState::UseComputedHeight()) {
      aMetrics.height = font->mFont.size +
        aReflowState.mComputedBorderPadding.top +
        aReflowState.mComputedBorderPadding.bottom;
    }
  }

  // For now our overflow area is zero. The real value will be
  // computed during vertical alignment of the line we are on.
  aMetrics.mOverflowArea.x = 0;
  aMetrics.mOverflowArea.y = 0;
  aMetrics.mOverflowArea.width = aMetrics.width;
  aMetrics.mOverflowArea.height = aMetrics.height;

#ifdef NOISY_FINAL_SIZE
  ListTag(stdout);
  printf(": metrics=%d,%d ascent=%d descent=%d\n",
         aMetrics.width, aMetrics.height, aMetrics.ascent, aMetrics.descent);
  if (nsnull != aMetrics.maxElementSize) {
    printf(" maxElementSize %d,%d\n", 
      aMetrics.maxElementSize->width, aMetrics.maxElementSize->height);
  }
#endif

  return rv;
}

static 
void SetContainsPercentAwareChild(nsIFrame *aFrame)
{
  nsFrameState myFrameState;
  aFrame->GetFrameState(&myFrameState);
  aFrame->SetFrameState(myFrameState | NS_INLINE_FRAME_CONTAINS_PERCENT_AWARE_CHILD);
}

static
void MarkPercentAwareFrame(nsIPresContext *aPresContext, 
                           nsInlineFrame  *aInline,
                           nsIFrame       *aFrame)
{
  nsFrameState childFrameState;
  aFrame->GetFrameState(&childFrameState);
  if (childFrameState & NS_FRAME_REPLACED_ELEMENT) 
  { // aFrame is a replaced element, check it's style
    if (nsLineLayout::IsPercentageAwareReplacedElement(aPresContext, aFrame)) {
      SetContainsPercentAwareChild(aInline);
    }
  }
  else
  {
    nsIFrame *child;
    aFrame->FirstChild(aPresContext, nsnull, &child);
    if (child)
    { // aFrame is an inline container frame, check my frame state
      if (childFrameState & NS_INLINE_FRAME_CONTAINS_PERCENT_AWARE_CHILD) {
        SetContainsPercentAwareChild(aInline); // if a child container is effected, so am I
      }
    }
    // else frame is a leaf that we don't care about
  }   
}

nsresult
nsInlineFrame::ReflowInlineFrame(nsIPresContext* aPresContext,
                                 const nsHTMLReflowState& aReflowState,
                                 InlineReflowState& irs,
                                 nsIFrame* aFrame,
                                 nsReflowStatus& aStatus)
{
  nsLineLayout* lineLayout = aReflowState.mLineLayout;
  PRBool reflowingFirstLetter = lineLayout->GetFirstLetterStyleOK();
  PRBool pushedFrame;
  nsresult rv = lineLayout->ReflowFrame(aFrame, &irs.mNextRCFrame, aStatus,
                                        nsnull, pushedFrame);
  /* This next block is for bug 28811
     Test the child frame for %-awareness, 
     and mark this frame with a bit if it is %-aware.
     Don't bother if this frame is already marked
  */
  if (!(mState & NS_INLINE_FRAME_CONTAINS_PERCENT_AWARE_CHILD)) {  
    MarkPercentAwareFrame(aPresContext, this, aFrame);
  }

  if (NS_FAILED(rv)) {
    return rv;
  }
  if (NS_INLINE_IS_BREAK(aStatus)) {
    if (NS_INLINE_IS_BREAK_BEFORE(aStatus)) {
      if (aFrame != mFrames.FirstChild()) {
        // Change break-before status into break-after since we have
        // already placed at least one child frame. This preserves the
        // break-type so that it can be propogated upward.
        aStatus = NS_FRAME_NOT_COMPLETE |
          NS_INLINE_BREAK | NS_INLINE_BREAK_AFTER |
          (aStatus & NS_INLINE_BREAK_TYPE_MASK);
        PushFrames(aPresContext, aFrame, irs.mPrevFrame);
      }
      else {
        // Preserve reflow status when breaking-before our first child
        // and propogate it upward without modification.
        // Note: if we're lazily setting the frame pointer for our child 
        // frames, then we need to set it now. Don't return and leave the
        // remaining child frames in our child list with the wrong parent
        // frame pointer...
        if (irs.mSetParentPointer) {
          nsIFrame* f;
          aFrame->GetNextSibling(&f);
          while (f) {
            f->SetParent(this);
            f->GetNextSibling(&f);
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
      nsIFrame* nextFrame;
      aFrame->GetNextSibling(&nextFrame);
      if (nsnull != nextFrame) {
        aStatus |= NS_FRAME_NOT_COMPLETE;
        PushFrames(aPresContext, nextFrame, aFrame);
      }
      else if (nsnull != mNextInFlow) {
        // We must return an incomplete status if there are more child
        // frames remaining in a next-in-flow that follows this frame.
        nsInlineFrame* nextInFlow = (nsInlineFrame*) mNextInFlow;
        while (nsnull != nextInFlow) {
          if (nextInFlow->mFrames.NotEmpty()) {
            aStatus |= NS_FRAME_NOT_COMPLETE;
            break;
          }
          nextInFlow = (nsInlineFrame*) nextInFlow->mNextInFlow;
        }
      }
    }
  }
  else if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
    nsIFrame* newFrame;
    rv = CreateNextInFlow(aPresContext, this, aFrame, newFrame);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (!reflowingFirstLetter) {
      nsIFrame* nextFrame;
      aFrame->GetNextSibling(&nextFrame);
      if (nsnull != nextFrame) {
        PushFrames(aPresContext, nextFrame, aFrame);
      }
    }
  }
  return rv;
}

nsIFrame*
nsInlineFrame::PullOneFrame(nsIPresContext* aPresContext,
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
    nextInFlow = (nsInlineFrame*) nextInFlow->mNextInFlow;
    irs.mNextInFlow = nextInFlow;
  }

  *aIsComplete = isComplete;
  return frame;
}

void
nsInlineFrame::PushFrames(nsIPresContext* aPresContext,
                          nsIFrame* aFromChild,
                          nsIFrame* aPrevSibling)
{
  NS_PRECONDITION(nsnull != aFromChild, "null pointer");
  NS_PRECONDITION(nsnull != aPrevSibling, "pushing first child");
#ifdef DEBUG
  nsIFrame* prevNextSibling;
  aPrevSibling->GetNextSibling(&prevNextSibling);
  NS_PRECONDITION(prevNextSibling == aFromChild, "bad prev sibling");
#endif

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
  if (nsnull != mPrevInFlow) {
    nsInlineFrame* prev = (nsInlineFrame*) mPrevInFlow;
    if (prev->mRect.height || prev->mRect.width) {
      // Prev-in-flow is not empty therefore we don't render our left
      // border edge.
      skip |= 1 << NS_SIDE_LEFT;
    }
    else {
      // If the prev-in-flow is empty, then go ahead and let our right
      // edge border render.
    }
  }
  if (nsnull != mNextInFlow) {
    nsInlineFrame* next = (nsInlineFrame*) mNextInFlow;
    if (next->mRect.height || next->mRect.width) {
      // Next-in-flow is not empty therefore we don't render our right
      // border edge.
      skip |= 1 << NS_SIDE_RIGHT;
    }
    else {
      // If the next-in-flow is empty, then go ahead and let our right
      // edge border render.
    }
  }
  return skip;
}

//////////////////////////////////////////////////////////////////////

// nsLineFrame implementation

static void
ReParentChildListStyle(nsIPresContext* aPresContext,
                       nsIStyleContext* aParentStyleContext,
                       nsFrameList& aFrameList)
{
  nsIFrame* kid = aFrameList.FirstChild();
  while (nsnull != kid) {
    aPresContext->ReParentStyleContext(kid, aParentStyleContext);
    kid->GetNextSibling(&kid);
  }
}

nsresult
NS_NewFirstLineFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(nsnull != aNewFrame, "null ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsInlineFrame* it = new (aPresShell) nsFirstLineFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsFirstLineFrame::nsFirstLineFrame()
{
}

#ifdef DEBUG
NS_IMETHODIMP
nsFirstLineFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Line", aResult);
}
#endif

NS_IMETHODIMP
nsFirstLineFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::lineFrame;
  NS_ADDREF(*aType);
  return NS_OK;
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
nsFirstLineFrame::PullOneFrame(nsIPresContext* aPresContext, InlineReflowState& irs, PRBool* aIsComplete)
{
  nsIFrame* frame = nsInlineFrame::PullOneFrame(aPresContext, irs, aIsComplete);
  if (frame && !mPrevInFlow) {
    // We are a first-line frame. Fixup the child frames
    // style-context that we just pulled.
    aPresContext->ReParentStyleContext(frame, mStyleContext);
  }
  return frame;
}

NS_IMETHODIMP
nsFirstLineFrame::Reflow(nsIPresContext* aPresContext,
                         nsHTMLReflowMetrics& aMetrics,
                         const nsHTMLReflowState& aReflowState,
                         nsReflowStatus& aStatus)
{
  if (nsnull == aReflowState.mLineLayout) {
    return NS_ERROR_INVALID_ARG;
  }

  // Check for an overflow list with our prev-in-flow
  nsFirstLineFrame* prevInFlow = (nsFirstLineFrame*)mPrevInFlow;
  if (nsnull != prevInFlow) {
    nsIFrame* prevOverflowFrames = prevInFlow->GetOverflowFrames(aPresContext, PR_TRUE);
    if (prevOverflowFrames) {
      nsFrameList frames(prevOverflowFrames);
      
      ReParentChildListStyle(aPresContext, mStyleContext, frames);
      mFrames.InsertFrames(this, nsnull, prevOverflowFrames);
    }
  }

  // It's also possible that we have an overflow list for ourselves
  nsIFrame* overflowFrames = GetOverflowFrames(aPresContext, PR_TRUE);
  if (overflowFrames) {
    NS_ASSERTION(mFrames.NotEmpty(), "overflow list w/o frames");
    nsFrameList frames(overflowFrames);

    ReParentChildListStyle(aPresContext, mStyleContext, frames);
    mFrames.AppendFrames(nsnull, overflowFrames);
  }

  // Set our own reflow state (additional state above and beyond
  // aReflowState)
  InlineReflowState irs;
  irs.mPrevFrame = nsnull;
  irs.mNextInFlow = (nsInlineFrame*) mNextInFlow;
  irs.mNextRCFrame = nsnull;
  if (eReflowReason_Incremental == aReflowState.reason) {
    // Peel off the next frame in the path if this is an incremental
    // reflow aimed at one of the children.
    nsIFrame* target;
    aReflowState.reflowCommand->GetTarget(target);
    if (this != target) {
      aReflowState.reflowCommand->GetNext(irs.mNextRCFrame);
    }
  }

  nsresult rv;
  PRBool wasEmpty = mFrames.IsEmpty();
  if (wasEmpty) {
    // Try to pull over one frame before starting so that we know
    // whether we have an anonymous block or not.
    PRBool complete;
    PullOneFrame(aPresContext, irs, &complete);
  }

  if (nsnull == mPrevInFlow) {
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
      nsIFrame* parentFrame;
      first->GetParent(&parentFrame);
      nsIStyleContext* parentContext;
      parentFrame->GetStyleContext(&parentContext);
      if (parentContext) {
        // Create a new style context that is a child of the parent
        // style context thus removing the :first-line style. This way
        // we behave as if an anonymous (unstyled) span was the child
        // of the parent frame.
        nsIStyleContext* newSC;
        aPresContext->ResolvePseudoStyleContextFor(mContent,
                                                  nsHTMLAtoms::mozLineFrame,
                                                  parentContext,
                                                  PR_FALSE, &newSC);
        if (newSC) {
          // Switch to the new style context.
          SetStyleContext(aPresContext, newSC);

          // Re-resolve all children
          ReParentChildListStyle(aPresContext, mStyleContext, mFrames);

          NS_RELEASE(newSC);
        }
        NS_RELEASE(parentContext);
      }
    }
  }

  rv = ReflowFrames(aPresContext, aReflowState, irs, aMetrics, aStatus);

  // Note: the line layout code will properly compute our
  // NS_FRAME_OUTSIDE_CHILDREN state for us.

  return rv;
}

//////////////////////////////////////////////////////////////////////

nsresult
NS_NewPositionedInlineFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsPositionedInlineFrame* it = new (aPresShell) nsPositionedInlineFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

NS_IMETHODIMP
nsPositionedInlineFrame::Destroy(nsIPresContext* aPresContext)
{
  mAbsoluteContainer.DestroyFrames(this, aPresContext);
  return nsInlineFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
nsPositionedInlineFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                             nsIAtom*        aListName,
                                             nsIFrame*       aChildList)
{
  nsresult  rv;

  if (nsLayoutAtoms::absoluteList == aListName) {
    rv = mAbsoluteContainer.SetInitialChildList(this, aPresContext, aListName, aChildList);
  } else {
    rv = nsInlineFrame::SetInitialChildList(aPresContext, aListName, aChildList);
  }

  return rv;
}

NS_IMETHODIMP
nsPositionedInlineFrame::AppendFrames(nsIPresContext* aPresContext,
                                      nsIPresShell&   aPresShell,
                                      nsIAtom*        aListName,
                                      nsIFrame*       aFrameList)
{
  nsresult  rv;
  
  if (nsLayoutAtoms::absoluteList == aListName) {
    rv = mAbsoluteContainer.AppendFrames(this, aPresContext, aPresShell, aListName,
                                         aFrameList);
  } else {
    rv = nsInlineFrame::AppendFrames(aPresContext, aPresShell, aListName,
                                     aFrameList);
  }

  return rv;
}
  
NS_IMETHODIMP
nsPositionedInlineFrame::InsertFrames(nsIPresContext* aPresContext,
                                      nsIPresShell&   aPresShell,
                                      nsIAtom*        aListName,
                                      nsIFrame*       aPrevFrame,
                                      nsIFrame*       aFrameList)
{
  nsresult  rv;

  if (nsLayoutAtoms::absoluteList == aListName) {
    rv = mAbsoluteContainer.InsertFrames(this, aPresContext, aPresShell, aListName,
                                         aPrevFrame, aFrameList);
  } else {
    rv = nsInlineFrame::InsertFrames(aPresContext, aPresShell, aListName, aPrevFrame,
                                     aFrameList);
  }

  return rv;
}
  
NS_IMETHODIMP
nsPositionedInlineFrame::RemoveFrame(nsIPresContext* aPresContext,
                                     nsIPresShell&   aPresShell,
                                     nsIAtom*        aListName,
                                     nsIFrame*       aOldFrame)
{
  nsresult  rv;

  if (nsLayoutAtoms::absoluteList == aListName) {
    rv = mAbsoluteContainer.RemoveFrame(this, aPresContext, aPresShell, aListName, aOldFrame);
  } else {
    rv = nsInlineFrame::RemoveFrame(aPresContext, aPresShell, aListName, aOldFrame);
  }

  return rv;
}

NS_IMETHODIMP
nsPositionedInlineFrame::GetAdditionalChildListName(PRInt32   aIndex,
                                                    nsIAtom** aListName) const
{
  NS_PRECONDITION(nsnull != aListName, "null OUT parameter pointer");
  *aListName = nsnull;
  if (0 == aIndex) {
    *aListName = nsLayoutAtoms::absoluteList;
    NS_ADDREF(*aListName);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPositionedInlineFrame::FirstChild(nsIPresContext* aPresContext,
                                    nsIAtom*        aListName,
                                    nsIFrame**      aFirstChild) const
{
  NS_PRECONDITION(nsnull != aFirstChild, "null OUT parameter pointer");
  if (aListName == nsLayoutAtoms::absoluteList) {
    return mAbsoluteContainer.FirstChild(this, aListName, aFirstChild);
  }

  return nsInlineFrame::FirstChild(aPresContext, aListName, aFirstChild);
}

NS_IMETHODIMP
nsPositionedInlineFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::positionedInlineFrame;
  NS_ADDREF(*aType);
  return NS_OK;
}

NS_IMETHODIMP
nsPositionedInlineFrame::Reflow(nsIPresContext*          aPresContext,
                                nsHTMLReflowMetrics&     aDesiredSize,
                                const nsHTMLReflowState& aReflowState,
                                nsReflowStatus&          aStatus)
{
  nsresult  rv = NS_OK;

  // See if it's an incremental reflow command
  if (eReflowReason_Incremental == aReflowState.reason) {
    // Give the absolute positioning code a chance to handle it
    PRBool  handled;
    nsRect  childBounds;
    nscoord containingBlockWidth = -1;
    nscoord containingBlockHeight = -1;
    
    mAbsoluteContainer.IncrementalReflow(this, aPresContext, aReflowState,
                                         containingBlockWidth, containingBlockHeight,
                                         handled, childBounds);

    // If the incremental reflow command was handled by the absolute positioning
    // code, then we're all done
    if (handled) {
      // Just return our current size as our desired size
      // XXX I don't know how to compute that without a reflow, so for the
      // time being pretend a resize reflow occured
      nsHTMLReflowState reflowState(aReflowState);
      reflowState.reason = eReflowReason_Resize;
      reflowState.reflowCommand = nsnull;
      rv = nsInlineFrame::Reflow(aPresContext, aDesiredSize, reflowState, aStatus);

      // XXX Although this seems like the correct thing to do the line layout
      // code seems to reset the NS_FRAME_OUTSIDE_CHILDREN and so it is ignored
#if 0
      // Factor the absolutely positioned child bounds into the overflow area
      aDesiredSize.mOverflowArea.UnionRect(aDesiredSize.mOverflowArea, childBounds);

      // Make sure the NS_FRAME_OUTSIDE_CHILDREN flag is set correctly
      if ((aDesiredSize.mOverflowArea.x < 0) ||
          (aDesiredSize.mOverflowArea.y < 0) ||
          (aDesiredSize.mOverflowArea.XMost() > aDesiredSize.width) ||
          (aDesiredSize.mOverflowArea.YMost() > aDesiredSize.height)) {
        mState |= NS_FRAME_OUTSIDE_CHILDREN;
      } else {
        mState &= ~NS_FRAME_OUTSIDE_CHILDREN;
      }
#endif
      return rv;
    }
  }

  // Let the inline frame do its reflow first
  rv = nsInlineFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

  // Let the absolutely positioned container reflow any absolutely positioned
  // child frames that need to be reflowed
  if (NS_SUCCEEDED(rv)) {
    nscoord containingBlockWidth = -1;
    nscoord containingBlockHeight = -1;
    nsRect  childBounds;

    rv = mAbsoluteContainer.Reflow(this, aPresContext, aReflowState,
                                   containingBlockWidth, containingBlockHeight,
                                   childBounds);
    
    // XXX Although this seems like the correct thing to do the line layout
    // code seems to reset the NS_FRAME_OUTSIDE_CHILDREN and so it is ignored
#if 0
    // Factor the absolutely positioned child bounds into the overflow area
    aDesiredSize.mOverflowArea.UnionRect(aDesiredSize.mOverflowArea, childBounds);

    // Make sure the NS_FRAME_OUTSIDE_CHILDREN flag is set correctly
    if ((aDesiredSize.mOverflowArea.x < 0) ||
        (aDesiredSize.mOverflowArea.y < 0) ||
        (aDesiredSize.mOverflowArea.XMost() > aDesiredSize.width) ||
        (aDesiredSize.mOverflowArea.YMost() > aDesiredSize.height)) {
      mState |= NS_FRAME_OUTSIDE_CHILDREN;
    } else {
      mState &= ~NS_FRAME_OUTSIDE_CHILDREN;
    }
#endif
  }

  return rv;
}

#ifdef DEBUG
NS_IMETHODIMP
nsPositionedInlineFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = sizeof(*this);
  return NS_OK;
}
#endif
