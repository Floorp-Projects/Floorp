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
#include "nsCOMPtr.h"
#include "nsInlineFrame.h"
#include "nsBlockFrame.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLParts.h"
#include "nsStyleContext.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsAbsoluteContainingBlock.h"
#include "nsLayoutAtoms.h"
#include "nsCSSAnonBoxes.h"
#include "nsReflowPath.h"
#include "nsAutoPtr.h"
#include "nsFrameManager.h"
#ifdef ACCESSIBILITY
#include "nsIServiceManager.h"
#include "nsIAccessibilityService.h"
#endif

#ifdef DEBUG
#undef NOISY_PUSHING
#endif


NS_DEFINE_IID(kInlineFrameCID, NS_INLINE_FRAME_CID);


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
nsInlineFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Inline"), aResult);
}
#endif

nsIAtom*
nsInlineFrame::GetType() const
{
  return nsLayoutAtoms::inlineFrame;
}

inline PRBool
IsBorderZero(nsStyleUnit aUnit, nsStyleCoord &aCoord)
{
    return ((aUnit == eStyleUnit_Coord && aCoord.GetCoordValue() == 0));
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
nsInlineFrame::IsEmpty()
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
  if ((border->IsBorderSideVisible(NS_SIDE_RIGHT) &&
       !IsBorderZero(border->mBorder.GetRightUnit(),
                     border->mBorder.GetRight(coord))) ||
      (border->IsBorderSideVisible(NS_SIDE_LEFT) &&
       !IsBorderZero(border->mBorder.GetLeftUnit(),
                     border->mBorder.GetLeft(coord))) ||
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

  for (nsIFrame *kid = mFrames.FirstChild(); kid; kid = kid->GetNextSibling()) {
    if (!kid->IsEmpty())
      return PR_FALSE;
  }
  return PR_TRUE;
}

NS_IMETHODIMP
nsInlineFrame::AppendFrames(nsPresContext* aPresContext,
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
nsInlineFrame::InsertFrames(nsPresContext* aPresContext,
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
nsInlineFrame::RemoveFrame(nsPresContext* aPresContext,
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
    // If the frame we are removing is a brFrame, we need a reflow so
    // the line the brFrame was on can attempt to pull up any frames
    // that can fit from lines below it.
    PRBool generateReflowCommand =
      aOldFrame->GetType() == nsLayoutAtoms::brFrame;

    nsInlineFrame* parent = NS_STATIC_CAST(nsInlineFrame*, aOldFrame->GetParent());
    while (aOldFrame) {
#ifdef IBMBIDI
      if (nsLayoutAtoms::nextBidi != aListName) {
#endif
      // If the frame being removed has zero size then don't bother
      // generating a reflow command, otherwise make sure we do.
      nsRect bbox = aOldFrame->GetRect();
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
      parent->mFrames.DestroyFrame(aPresContext, aOldFrame);
      aOldFrame = oldFrameNextInFlow;
      if (aOldFrame) {
        parent = NS_STATIC_CAST(nsInlineFrame*, aOldFrame->GetParent());
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
nsInlineFrame::ReplaceFrame(nsPresContext* aPresContext,
                            nsIPresShell& aPresShell,
                            nsIAtom* aListName,
                            nsIFrame* aOldFrame,
                            nsIFrame* aNewFrame)
{
  if (aListName) {
    NS_ERROR("Don't have any special lists on inline frames!");
    return NS_ERROR_INVALID_ARG;
  }
  if (!aOldFrame || !aNewFrame) {
    NS_ERROR("Missing aOldFrame or aNewFrame");
    return NS_ERROR_INVALID_ARG;
  }

  PRBool retval =
    mFrames.ReplaceFrame(aPresContext, this, aOldFrame, aNewFrame, PR_TRUE);
  
  // Ask the parent frame to reflow me.
  ReflowDirtyChild(&aPresShell, nsnull);

  return retval ? NS_OK : NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsInlineFrame::Paint(nsPresContext*      aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect&        aDirtyRect,
                     nsFramePaintLayer    aWhichLayer,
                     PRUint32             aFlags)
{
  if (NS_FRAME_IS_UNFLOWABLE & mState) {
    return NS_OK;
  }

  // Paint inline element backgrounds in the foreground layer (bug 36710).
  if (aWhichLayer == NS_FRAME_PAINT_LAYER_FOREGROUND) {
    PaintSelf(aPresContext, aRenderingContext, aDirtyRect);
  }
    
  // The sole purpose of this is to trigger display of the selection
  // window for Named Anchors, which don't have any children and
  // normally don't have any size, but in Editor we use CSS to display
  // an image to represent this "hidden" element.
  if (!mFrames.FirstChild()) {
    nsFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                   aWhichLayer, aFlags);
  }

  PaintDecorationsAndChildren(aPresContext, aRenderingContext,
                              aDirtyRect, aWhichLayer, PR_FALSE,
                              aFlags);
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////
// Reflow methods

NS_IMETHODIMP
nsInlineFrame::Reflow(nsPresContext*          aPresContext,
                      nsHTMLReflowMetrics&     aMetrics,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsInlineFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aMetrics, aStatus);
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
      mFrames.AppendFrames(overflowFrames->GetParent() == this ? nsnull : this, overflowFrames);
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
nsInlineFrame::ReflowFrames(nsPresContext* aPresContext,
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
    if (NS_INLINE_IS_BREAK(aStatus) || 
        (!reflowingFirstLetter && NS_FRAME_IS_NOT_COMPLETE(aStatus))) {
      done = PR_TRUE;
      break;
    }
    irs.mPrevFrame = frame;
    frame = frame->GetNextSibling();
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
  lineLayout->EndSpan(this, size,
                    aMetrics.mComputeMEW ? &aMetrics.mMaxElementWidth : nsnull);
  if ((0 == size.height) && (0 == size.width) &&
      ((nsnull != mPrevInFlow) || (nsnull != mNextInFlow))) {
    // This is a continuation of a previous inline. Therefore make
    // sure we don't affect the line-height.
    aMetrics.width = 0;
    aMetrics.height = 0;
    aMetrics.ascent = 0;
    aMetrics.descent = 0;
    if (aMetrics.mComputeMEW) {
      aMetrics.mMaxElementWidth = 0;
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
      fm->GetMaxDescent(aMetrics.descent);
      fm->GetHeight(aMetrics.height);
    } else {
      NS_WARNING("Cannot get font metrics - defaulting sizes to 0");
      aMetrics.ascent = aMetrics.descent = aMetrics.height = 0;
    }
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
      const nsStyleFont* font = GetStyleFont();
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
  if (aMetrics.mComputeMEW) {
    printf(" maxElementWidth %d\n", aMetrics.mMaxElementWidth);
  }
#endif

  return rv;
}

static 
void SetContainsPercentAwareChild(nsIFrame *aFrame)
{
  aFrame->AddStateBits(NS_INLINE_FRAME_CONTAINS_PERCENT_AWARE_CHILD);
}

static
void MarkPercentAwareFrame(nsPresContext *aPresContext, 
                           nsInlineFrame  *aInline,
                           nsIFrame       *aFrame)
{
  if (aFrame->GetStateBits() & NS_FRAME_REPLACED_ELEMENT) 
  { // aFrame is a replaced element, check it's style
    if (nsLineLayout::IsPercentageAwareReplacedElement(aPresContext, aFrame)) {
      SetContainsPercentAwareChild(aInline);
    }
  }
  else
  {
    if (aFrame->GetFirstChild(nsnull))
    { // aFrame is an inline container frame, check my frame state
      if (aFrame->GetStateBits() & NS_INLINE_FRAME_CONTAINS_PERCENT_AWARE_CHILD) {
        SetContainsPercentAwareChild(aInline); // if a child container is effected, so am I
      }
    }
    // else frame is a leaf that we don't care about
  }   
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
  nsresult rv = lineLayout->ReflowFrame(aFrame, aStatus, nsnull, pushedFrame);
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
    if (nsLayoutAtoms::placeholderFrame == aFrame->GetType()) {
      nsBlockReflowState* blockRS = lineLayout->mBlockRS;
      blockRS->mBlock->SplitPlaceholder(*aPresContext, *aFrame);
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
    nextInFlow = (nsInlineFrame*) nextInFlow->mNextInFlow;
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
  if (nsnull != mPrevInFlow) {
    nsInlineFrame* prev = (nsInlineFrame*) mPrevInFlow;
    if (prev->mRect.height || prev->mRect.width) {
      // Prev-in-flow is not empty therefore we don't render our left
      // border edge.
      skip |= 1 << NS_SIDE_LEFT;
    }
    else {
      // If the prev-in-flow is empty, then go ahead and let our left
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

#ifdef ACCESSIBILITY
NS_IMETHODIMP nsInlineFrame::GetAccessible(nsIAccessible** aAccessible)
{
  // Broken image accessibles are created here, because layout
  // replaces the image or image control frame with an inline frame
  *aAccessible = nsnull;
  nsIAtom *tagAtom = mContent->Tag();
  if ((tagAtom == nsHTMLAtoms::img || tagAtom == nsHTMLAtoms::input || 
       tagAtom == nsHTMLAtoms::label || tagAtom == nsHTMLAtoms::hr) &&
      mContent->IsContentOfType(nsIContent::eHTML)) {
    // Only get accessibility service if we're going to use it
    nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));
    if (!accService)
      return NS_ERROR_FAILURE;
    if (tagAtom == nsHTMLAtoms::input)  // Broken <input type=image ... />
      return accService->CreateHTML4ButtonAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
    else if (tagAtom == nsHTMLAtoms::img)  // Create accessible for broken <img>
      return accService->CreateHTMLImageAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
    else if (tagAtom == nsHTMLAtoms::label)  // Creat accessible for <label>
      return accService->CreateHTMLLabelAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
    // Create accessible for <hr>
    return accService->CreateHTMLHRAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
  }

  return NS_ERROR_FAILURE;
}
#endif

//////////////////////////////////////////////////////////////////////

// nsLineFrame implementation

static void
ReParentChildListStyle(nsPresContext* aPresContext,
                       nsStyleContext* aParentStyleContext,
                       nsFrameList& aFrameList)
{
  nsFrameManager *frameManager = aPresContext->FrameManager();

  for (nsIFrame* kid = aFrameList.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    frameManager->ReParentStyleContext(kid, aParentStyleContext);
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
nsFirstLineFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Line"), aResult);
}
#endif

nsIAtom*
nsFirstLineFrame::GetType() const
{
  return nsLayoutAtoms::lineFrame;
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
  if (frame && !mPrevInFlow) {
    // We are a first-line frame. Fixup the child frames
    // style-context that we just pulled.
    aPresContext->FrameManager()->ReParentStyleContext(frame, mStyleContext);
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
  nsFirstLineFrame* prevInFlow = (nsFirstLineFrame*)mPrevInFlow;
  if (nsnull != prevInFlow) {
    nsIFrame* prevOverflowFrames = prevInFlow->GetOverflowFrames(aPresContext, PR_TRUE);
    if (prevOverflowFrames) {
      nsFrameList frames(prevOverflowFrames);
      
      mFrames.InsertFrames(this, nsnull, prevOverflowFrames);
      ReParentChildListStyle(aPresContext, mStyleContext, frames);
    }
  }

  // It's also possible that we have an overflow list for ourselves
  nsIFrame* overflowFrames = GetOverflowFrames(aPresContext, PR_TRUE);
  if (overflowFrames) {
    NS_ASSERTION(mFrames.NotEmpty(), "overflow list w/o frames");
    nsFrameList frames(overflowFrames);

    mFrames.AppendFrames(nsnull, overflowFrames);
    ReParentChildListStyle(aPresContext, mStyleContext, frames);
  }

  // Set our own reflow state (additional state above and beyond
  // aReflowState)
  InlineReflowState irs;
  irs.mPrevFrame = nsnull;
  irs.mNextInFlow = (nsInlineFrame*) mNextInFlow;

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
          SetStyleContext(aPresContext, newSC);

          // Re-resolve all children
          ReParentChildListStyle(aPresContext, mStyleContext, mFrames);
        }
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
  nsPositionedInlineFrame* it = new (aPresShell) nsPositionedInlineFrame();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

NS_IMETHODIMP
nsPositionedInlineFrame::Destroy(nsPresContext* aPresContext)
{
  mAbsoluteContainer.DestroyFrames(this, aPresContext);
  return nsInlineFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
nsPositionedInlineFrame::SetInitialChildList(nsPresContext* aPresContext,
                                             nsIAtom*        aListName,
                                             nsIFrame*       aChildList)
{
  nsresult  rv;

  if (mAbsoluteContainer.GetChildListName() == aListName) {
    rv = mAbsoluteContainer.SetInitialChildList(this, aPresContext, aListName, aChildList);
  } else {
    rv = nsInlineFrame::SetInitialChildList(aPresContext, aListName, aChildList);
  }

  return rv;
}

NS_IMETHODIMP
nsPositionedInlineFrame::AppendFrames(nsPresContext* aPresContext,
                                      nsIPresShell&   aPresShell,
                                      nsIAtom*        aListName,
                                      nsIFrame*       aFrameList)
{
  nsresult  rv;
  
  if (mAbsoluteContainer.GetChildListName() == aListName) {
    rv = mAbsoluteContainer.AppendFrames(this, aPresContext, aPresShell, aListName,
                                         aFrameList);
  } else {
    rv = nsInlineFrame::AppendFrames(aPresContext, aPresShell, aListName,
                                     aFrameList);
  }

  return rv;
}
  
NS_IMETHODIMP
nsPositionedInlineFrame::InsertFrames(nsPresContext* aPresContext,
                                      nsIPresShell&   aPresShell,
                                      nsIAtom*        aListName,
                                      nsIFrame*       aPrevFrame,
                                      nsIFrame*       aFrameList)
{
  nsresult  rv;

  if (mAbsoluteContainer.GetChildListName() == aListName) {
    rv = mAbsoluteContainer.InsertFrames(this, aPresContext, aPresShell, aListName,
                                         aPrevFrame, aFrameList);
  } else {
    rv = nsInlineFrame::InsertFrames(aPresContext, aPresShell, aListName, aPrevFrame,
                                     aFrameList);
  }

  return rv;
}
  
NS_IMETHODIMP
nsPositionedInlineFrame::RemoveFrame(nsPresContext* aPresContext,
                                     nsIPresShell&   aPresShell,
                                     nsIAtom*        aListName,
                                     nsIFrame*       aOldFrame)
{
  nsresult  rv;

  if (mAbsoluteContainer.GetChildListName() == aListName) {
    rv = mAbsoluteContainer.RemoveFrame(this, aPresContext, aPresShell, aListName, aOldFrame);
  } else {
    rv = nsInlineFrame::RemoveFrame(aPresContext, aPresShell, aListName, aOldFrame);
  }

  return rv;
}

NS_IMETHODIMP
nsPositionedInlineFrame::ReplaceFrame(nsPresContext* aPresContext,
                                      nsIPresShell&   aPresShell,
                                      nsIAtom*        aListName,
                                      nsIFrame*       aOldFrame,
                                      nsIFrame*       aNewFrame)
{
  if (mAbsoluteContainer.GetChildListName() == aListName) {
    return mAbsoluteContainer.ReplaceFrame(this, aPresContext, aPresShell,
                                           aListName, aOldFrame, aNewFrame);
  } else {
    return nsInlineFrame::ReplaceFrame(aPresContext, aPresShell, aListName,
                                       aOldFrame, aNewFrame);
  }
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
  return nsLayoutAtoms::positionedInlineFrame;
}

NS_IMETHODIMP
nsPositionedInlineFrame::Reflow(nsPresContext*          aPresContext,
                                nsHTMLReflowMetrics&     aDesiredSize,
                                const nsHTMLReflowState& aReflowState,
                                nsReflowStatus&          aStatus)
{
  nsresult  rv = NS_OK;

  nsRect oldRect(mRect);

  // See if it's an incremental reflow command
  if (mAbsoluteContainer.HasAbsoluteFrames() &&
      eReflowReason_Incremental == aReflowState.reason) {
    // Give the absolute positioning code a chance to handle it
    PRBool  handled;
    nscoord containingBlockWidth = -1;
    nscoord containingBlockHeight = -1;
    
    mAbsoluteContainer.IncrementalReflow(this, aPresContext, aReflowState,
                                         containingBlockWidth, containingBlockHeight,
                                         handled);

    // If the incremental reflow command was handled by the absolute positioning
    // code, then we're all done
    if (handled) {
      // Just return our current size as our desired size
      // XXX I don't know how to compute that without a reflow, so for the
      // time being pretend a resize reflow occured
      nsHTMLReflowState reflowState(aReflowState);
      reflowState.reason = eReflowReason_Resize;
      reflowState.path = nsnull;
      rv = nsInlineFrame::Reflow(aPresContext, aDesiredSize, reflowState, aStatus);

      // Factor the absolutely positioned child bounds into the overflow area
      nsRect childBounds;
      mAbsoluteContainer.CalculateChildBounds(aPresContext, childBounds);
      aDesiredSize.mOverflowArea.UnionRect(aDesiredSize.mOverflowArea, childBounds);

      FinishAndStoreOverflow(&aDesiredSize);
      return rv;
    }
  }

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
      mAbsoluteContainer.HasAbsoluteFrames() &&
      (eReflowReason_Incremental != aReflowState.reason ||
       aReflowState.path->mReflowCommand ||
       mRect != oldRect)) {
    // The containing block for the abs pos kids is formed by our content edge.
    nscoord containingBlockWidth = aDesiredSize.width -
      (aReflowState.mComputedBorderPadding.left +
       aReflowState.mComputedBorderPadding.right);
    nscoord containingBlockHeight = aDesiredSize.height -
      (aReflowState.mComputedBorderPadding.top +
       aReflowState.mComputedBorderPadding.bottom);
    nsRect  childBounds;

    rv = mAbsoluteContainer.Reflow(this, aPresContext, aReflowState,
                                   containingBlockWidth, containingBlockHeight,
                                   &childBounds);
    
    // Factor the absolutely positioned child bounds into the overflow area
    aDesiredSize.mOverflowArea.UnionRect(aDesiredSize.mOverflowArea, childBounds);

    FinishAndStoreOverflow(&aDesiredSize);
  }

  return rv;
}
