/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsInlineFrame.h"
#include "nsSize.h"
#include "nsIContent.h"
#include "nsIContentDelegate.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsCSSLayout.h"
#include "nsPlaceholderFrame.h"
#include "nsReflowCommand.h"

// XXX To Do:
// 2. horizontal child margins
// 3. borders/padding and splitting
// 4. child relative positioning
// 5. absolutely positioned children
// 6. direction support
// 7. CSS line-height property

#define DEFAULT_ASCENT_LEN  10
static NS_DEFINE_IID(kStyleMoleculeSID, NS_STYLEMOLECULE_SID);
static NS_DEFINE_IID(kStyleFontSID, NS_STYLEFONT_SID);

class nsInlineState
{
public:
  nsStyleFont*      font;           // style font
  nsStyleMolecule*  mol;            // style molecule
  nsSize            availSize;      // available space in which to reflow (starts as max size minus insets)
  nsSize*           maxElementSize; // maximum element size (may be null)
  nscoord           x;              // running x-offset (starts at left inner edge)
  const nscoord     y;              // y-offset (top inner edge)
  nscoord           maxAscent;      // max child ascent
  nscoord           maxDescent;     // max child descent
  nscoord*          ascents;        // ascent information for each child
  PRBool            unconstrainedWidth;
  PRBool            unconstrainedHeight;

  // Constructor
  nsInlineState(nsStyleFont*     aStyleFont,
                nsStyleMolecule* aStyleMolecule,
                const nsSize&    aMaxSize,
                nsSize*          aMaxElementSize)
    : x(aStyleMolecule->borderPadding.left),  // determined by inner edge
      y(aStyleMolecule->borderPadding.top)    // determined by inner edge
  {
    font = aStyleFont;
    mol = aStyleMolecule;

    unconstrainedWidth = PRBool(aMaxSize.width == NS_UNCONSTRAINEDSIZE);
    unconstrainedHeight = PRBool(aMaxSize.height == NS_UNCONSTRAINEDSIZE);

    // If we're constrained adjust the available size so it excludes space
    // needed for border/padding
    availSize.width = aMaxSize.width;
    if (PR_FALSE == unconstrainedWidth) {
      availSize.width -= mol->borderPadding.left + mol->borderPadding.right;
    }
    availSize.height = aMaxSize.height;
    if (PR_FALSE == unconstrainedHeight) {
      availSize.height -= mol->borderPadding.top + mol->borderPadding.bottom;
    }

    // Initialize max element size
    maxElementSize = aMaxElementSize;
    if (nsnull != maxElementSize) {
      maxElementSize->SizeTo(0, 0);
    }

    ascents = ascentBuf;
    maxAscent = 0;
    maxDescent = 0;
  }

  void SetNumAscents(PRIntn aNumAscents) {
    // We keep around ascent information so that we can vertically align
    // children after we figure out how many children fit.
    if (aNumAscents > DEFAULT_ASCENT_LEN) {
      ascents = new nscoord[aNumAscents];
    }
  }

  // Destructor
 ~nsInlineState() {
    if (ascents != ascentBuf) {
      delete ascents;
    }
  }

private:
  nscoord   ascentBuf[DEFAULT_ASCENT_LEN];
};

/////////////////////////////////////////////////////////////////////////////
//

nsresult nsInlineFrame::NewFrame(nsIFrame**  aInstancePtrResult,
                                 nsIContent* aContent,
                                 PRInt32     aIndexInParent,
                                 nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsInlineFrame(aContent, aIndexInParent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}

nsInlineFrame::nsInlineFrame(nsIContent* aContent,
                             PRInt32     aIndexInParent,
                             nsIFrame*   aParent)
  : nsHTMLContainerFrame(aContent, aIndexInParent, aParent)
{
  NS_PRECONDITION(!IsPseudoFrame(), "can not be a pseudo frame");
}

nsInlineFrame::~nsInlineFrame()
{
}

void nsInlineFrame::PlaceChild(nsIFrame*              aChild,
                               PRInt32                aIndex,
                               nsInlineState&         aState,
                               const nsReflowMetrics& aChildSize,
                               const nsSize*          aChildMaxElementSize)
{
  // Set the child's rect
  aChild->SetRect(nsRect(aState.x, aState.y, aChildSize.width, aChildSize.height));

  // Adjust the running x-offset
  aState.x += aChildSize.width;

  // Update the array of ascents and the max ascent and descent
  aState.ascents[aIndex] = aChildSize.ascent;
  if (aChildSize.ascent > aState.maxAscent) {
    aState.maxAscent = aChildSize.ascent;
  }
  if (aChildSize.descent > aState.maxDescent) {
    aState.maxDescent = aChildSize.descent;
  }

  // If we're constrained then update the available width
  if (!aState.unconstrainedWidth) {
    aState.availSize.width -= aChildSize.width;
  }

  // Update the maximum element size
  if (nsnull != aChildMaxElementSize) {
    if (aChildMaxElementSize->width > aState.maxElementSize->width) {
      aState.maxElementSize->width = aChildMaxElementSize->width;
    }
    if (aChildMaxElementSize->height > aState.maxElementSize->height) {
      aState.maxElementSize->height = aChildMaxElementSize->height;
    }
  }
}

/**
 * Reflow the frames we've already created
 *
 * @param   aPresContext presentation context to use
 * @param   aState current inline state
 * @return  true if we successfully reflowed all the mapped children and false
 *            otherwise, e.g. we pushed children to the next in flow
 */
PRBool nsInlineFrame::ReflowMappedChildren(nsIPresContext* aPresContext,
                                           nsInlineState&  aState)
{
#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
  NS_PRECONDITION(nsnull != mFirstChild, "no children");

  PRInt32   childCount = 0;
  nsIFrame* prevKidFrame = nsnull;

  // Remember our original mLastContentIsComplete so that if we end up
  // having to push children, we have the correct value to hand to
  // PushChildren.
  PRBool    originalLastContentIsComplete = mLastContentIsComplete;

  nsSize    kidMaxElementSize;
  nsSize*   pKidMaxElementSize = (nsnull != aState.maxElementSize) ? &kidMaxElementSize : nsnull;
  PRBool    result = PR_TRUE;

  for (nsIFrame* kidFrame = mFirstChild; nsnull != kidFrame; ) {
    nsReflowMetrics kidSize;
    ReflowStatus    status;

    // Reflow the child into the available space
    status = ReflowChild(kidFrame, aPresContext, kidSize, aState.availSize,
                         pKidMaxElementSize);

    // Did the child fit?
    if ((kidSize.width > aState.availSize.width) && (kidFrame != mFirstChild)) {
      // The child is too wide to fit in the available space, and it's
      // not our first child

      // Since we are giving the next-in-flow our last child, we
      // give it our original mLastContentIsComplete too (in case we
      // are pushing into an empty next-in-flow)
      PushChildren(kidFrame, prevKidFrame, originalLastContentIsComplete);
      SetLastContentOffset(prevKidFrame);

      result = PR_FALSE;
      break;
    }

    // Place and size the child. We'll deal with vertical alignment when
    // we're all done
    PlaceChild(kidFrame, childCount, aState, kidSize, pKidMaxElementSize);
    childCount++;

    // Remember where we just were in case we end up pushing children
    prevKidFrame = kidFrame;

    // Is the child complete?
    mLastContentIsComplete = PRBool(status == frComplete);
    if (frNotComplete == status) {
      // No, the child isn't complete
      nsIFrame* kidNextInFlow;
       
      kidFrame->GetNextInFlow(kidNextInFlow);
      PRBool lastContentIsComplete = mLastContentIsComplete;
      if (nsnull == kidNextInFlow) {
        // The child doesn't have a next-in-flow so create a continuing
        // frame. This hooks the child into the flow
        nsIFrame* continuingFrame;
         
        kidFrame->CreateContinuingFrame(aPresContext, this, continuingFrame);
        NS_ASSERTION(nsnull != continuingFrame, "frame creation failed");

        // Add the continuing frame to the sibling list
        nsIFrame* nextSib;
         
        kidFrame->GetNextSibling(nextSib);
        continuingFrame->SetNextSibling(nextSib);
        kidFrame->SetNextSibling(continuingFrame);
        if (nsnull == nextSib) {
          // Assume that the continuation frame we just created is
          // complete, for now. It will get reflowed by our
          // next-in-flow (we are going to push it now)
          lastContentIsComplete = PR_TRUE;
        }
      }

      // We've used up all of our available space so push the remaining
      // children to the next-in-flow
      nsIFrame* nextSibling;
       
      kidFrame->GetNextSibling(nextSibling);
      if (nsnull != nextSibling) {
        PushChildren(nextSibling, kidFrame, lastContentIsComplete);
        SetLastContentOffset(prevKidFrame);
      }
      result = PR_FALSE;
      break;
    }

    // Get the next child frame
    kidFrame->GetNextSibling(kidFrame);

    // XXX talk with troy about checking for available space here
  }

  // Update the child count member data
  mChildCount = childCount;
#ifdef NS_DEBUG
  NS_POSTCONDITION(LengthOf(mFirstChild) == mChildCount, "bad child count");

  nsIFrame* lastChild;
  PRInt32   lastIndexInParent;

  LastChild(lastChild);
  lastChild->GetIndexInParent(lastIndexInParent);
  NS_POSTCONDITION(lastIndexInParent == mLastContentOffset, "bad last content offset");
  VerifyLastIsComplete();
#endif
  return result;
}

/**
 * Try and pull-up frames from our next-in-flow
 *
 * @param   aPresContext presentation context to use
 * @param   aState current inline state
 * @return  true if we successfully pulled-up all the children and false
 *            otherwise, e.g. child didn't fit
 */
PRBool nsInlineFrame::PullUpChildren(nsIPresContext* aPresContext,
                                     nsInlineState&  aState)
{
#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
  nsInlineFrame* nextInFlow = (nsInlineFrame*)mNextInFlow;
  nsSize         kidMaxElementSize;
  nsSize*        pKidMaxElementSize = (nsnull != aState.maxElementSize) ? &kidMaxElementSize : nsnull;
#ifdef NS_DEBUG
  PRInt32        kidIndex = NextChildOffset();
#endif
  nsIFrame*      prevKidFrame;
   
  LastChild(prevKidFrame);

  // This will hold the prevKidFrame's mLastContentIsComplete
  // status. If we have to push the frame that follows prevKidFrame
  // then this will become our mLastContentIsComplete state. Since
  // prevKidFrame is initially our last frame, it's completion status
  // is our mLastContentIsComplete value.
  PRBool        prevLastContentIsComplete = mLastContentIsComplete;

  PRBool        result = PR_TRUE;

  while (nsnull != nextInFlow) {
    nsReflowMetrics kidSize;
    ReflowStatus    status;

    // Get the next child
    nsIFrame* kidFrame = nextInFlow->mFirstChild;

    // Any more child frames?
    if (nsnull == kidFrame) {
      // No. Any frames on its overflow list?
      if (nsnull != nextInFlow->mOverflowList) {
        // Move the overflow list to become the child list
        NS_ABORT();
        nextInFlow->AppendChildren(nextInFlow->mOverflowList);
        nextInFlow->mOverflowList = nsnull;
        kidFrame = nextInFlow->mFirstChild;
      } else {
        // We've pulled up all the children, so move to the next-in-flow.
        nextInFlow = (nsInlineFrame*)nextInFlow->mNextInFlow;
        continue;
      }
    }

    // See if the child fits in the available space. If it fits or
    // it's splittable then reflow it. The reason we can't just move
    // it is that we still need ascent/descent information
    nsSize  kidFrameSize;
    PRBool  kidIsSplittable;

    kidFrame->GetSize(kidFrameSize);
    kidFrame->IsSplittable(kidIsSplittable);
    if ((kidFrameSize.width > aState.availSize.width) && !kidIsSplittable) {
      result = PR_FALSE;
      mLastContentIsComplete = prevLastContentIsComplete;
      break;
    }
    status = ReflowChild(kidFrame, aPresContext, kidSize, aState.availSize,
                         pKidMaxElementSize);

    // Did the child fit?
    if ((kidSize.width > aState.availSize.width) && (nsnull != mFirstChild)) {
      // The child is too wide to fit in the available space, and it's
      // not our first child
      result = PR_FALSE;
      mLastContentIsComplete = prevLastContentIsComplete;
      break;
    }

    // Place and size the child. We'll deal with vertical alignment when
    // we're all done
    PlaceChild(kidFrame, mChildCount, aState, kidSize, pKidMaxElementSize);

    // Remove the frame from its current parent
    kidFrame->GetNextSibling(nextInFlow->mFirstChild);
    nextInFlow->mChildCount--;
    // Update the next-in-flows first content offset
    if (nsnull != nextInFlow->mFirstChild) {
      nextInFlow->SetFirstContentOffset(nextInFlow->mFirstChild);
    }

    // Link the frame into our list of children
    kidFrame->SetGeometricParent(this);
    nsIFrame* kidContentParent;

    kidFrame->GetContentParent(kidContentParent);
    if (nextInFlow == kidContentParent) {
      kidFrame->SetContentParent(this);
    }
    if (nsnull == prevKidFrame) {
      mFirstChild = kidFrame;
      SetFirstContentOffset(kidFrame);
    } else {
      prevKidFrame->SetNextSibling(kidFrame);
    }
    kidFrame->SetNextSibling(nsnull);
    mChildCount++;

    // Remember where we just were in case we end up pushing children
    prevKidFrame = kidFrame;
    prevLastContentIsComplete = mLastContentIsComplete;

    // Is the child we just pulled up complete?
    mLastContentIsComplete = PRBool(status == frComplete);
    if (frNotComplete == status) {
      // No the child isn't complete
      nsIFrame* kidNextInFlow;
       
      kidFrame->GetNextInFlow(kidNextInFlow);
      if (nsnull == kidNextInFlow) {
        // The child doesn't have a next-in-flow so create a
        // continuing frame. The creation appends it to the flow and
        // prepares it for reflow.
        nsIFrame* continuingFrame;

        kidFrame->CreateContinuingFrame(aPresContext, this, continuingFrame);
        NS_ASSERTION(nsnull != continuingFrame, "frame creation failed");

        // Add the continuing frame to our sibling list and then push
        // it to the next-in-flow. This ensures the next-in-flow's
        // content offsets and child count are set properly. Note that
        // we can safely assume that the continuation is complete so
        // we pass PR_TRUE into PushChidren in case our next-in-flow
        // was just drained and now needs to know it's
        // mLastContentIsComplete state.
        kidFrame->SetNextSibling(continuingFrame);

        PushChildren(continuingFrame, kidFrame, PR_TRUE);

        // After we push the continuation frame we don't need to fuss
        // with mLastContentIsComplete beause the continuation frame
        // is no longer on *our* list.
      }

      // If the child isn't complete then it means that we've used up
      // all of our available space.
      result = PR_FALSE;
      break;
    }
  }

  // Update our last content offset
  if (nsnull != prevKidFrame) {
    NS_ASSERTION(IsLastChild(prevKidFrame), "bad last child");
    SetLastContentOffset(prevKidFrame);
  }

  // We need to make sure the first content offset is correct for any empty
  // next-in-flow frames (frames where we pulled up all the child frames)
  nextInFlow = (nsInlineFrame*)mNextInFlow;
  if ((nsnull != nextInFlow) && (nsnull == nextInFlow->mFirstChild)) {
    // We have at least one empty frame. Did we succesfully pull up all the
    // child frames?
    if (PR_FALSE == result) {
      // No, so we need to adjust the first content offset of all the empty
      // frames
      AdjustOffsetOfEmptyNextInFlows();
#ifdef NS_DEBUG
    } else {
      // Yes, we successfully pulled up all the child frames which means all
      // the next-in-flows must be empty. Do a sanity check
      while (nsnull != nextInFlow) {
        NS_ASSERTION(nsnull == nextInFlow->mFirstChild, "non-empty next-in-flow");
        nextInFlow = (nsInlineFrame*)nextInFlow->mNextInFlow;
      }
#endif
    }
  }

#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
  return result;
}

/**
 * Create new frames for content we haven't yet mapped
 *
 * @param   aPresContext presentation context to use
 * @param   aState current inline state
 * @return  frComplete if all content has been mapped and frNotComplete
 *            if we should be continued
 */
nsIFrame::ReflowStatus
nsInlineFrame::ReflowUnmappedChildren(nsIPresContext* aPresContext,
                                      nsInlineState&  aState)
{
#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
  nsIFrame*    kidPrevInFlow = nsnull;
  ReflowStatus result = frNotComplete;

  // If we have no children and we have a prev-in-flow then we need to pick
  // up where it left off. If we have children, e.g. we're being resized, then
  // our content offset should already be set correctly...
  if ((nsnull == mFirstChild) && (nsnull != mPrevInFlow)) {
    nsInlineFrame* prev = (nsInlineFrame*)mPrevInFlow;
    NS_ASSERTION(prev->mLastContentOffset >= prev->mFirstContentOffset, "bad prevInFlow");

    mFirstContentOffset = prev->NextChildOffset();
    if (!prev->mLastContentIsComplete) {
      // Our prev-in-flow's last child is not complete
      prev->LastChild(kidPrevInFlow);
    }
  }
  mLastContentIsComplete = PR_TRUE;

  // Place our children, one at a time until we are out of children
  nsSize    kidMaxElementSize;
  nsSize*   pKidMaxElementSize = (nsnull != aState.maxElementSize) ? &kidMaxElementSize : nsnull;
  PRInt32   kidIndex = NextChildOffset();
  nsIFrame* prevKidFrame;

  LastChild(prevKidFrame);
  for (;;) {
    // Get the next content object
    nsIContent* kid = mContent->ChildAt(kidIndex);
    if (nsnull == kid) {
      result = frComplete;
      break;
    }

    // Make sure we still have room left
    if (aState.availSize.width <= 0) {
      // Note: return status was set to frNotComplete above...
      NS_RELEASE(kid);
      break;
    }

    // Resolve style for the child
    nsIStyleContext* kidStyleContext =
      aPresContext->ResolveStyleContextFor(kid, this);

    // Figure out how we should treat the child
    nsIFrame*        kidFrame;
    nsStyleMolecule* kidMol =
      (nsStyleMolecule*)kidStyleContext->GetData(kStyleMoleculeSID);

    if (kidMol->floats != NS_STYLE_FLOAT_NONE) {
      PlaceholderFrame::NewFrame(&kidFrame, kid, kidIndex, this);
      kidFrame->SetStyleContext(kidStyleContext);

    } else if (nsnull == kidPrevInFlow) {
      nsIContentDelegate* kidDel;
      switch (kidMol->display) {
      case NS_STYLE_DISPLAY_BLOCK:
      case NS_STYLE_DISPLAY_LIST_ITEM:
        if (kidIndex != mFirstContentOffset) {
          // We don't allow block elements to be placed in us anywhere
          // other than at our left margin.
          NS_RELEASE(kidStyleContext);
          NS_RELEASE(kid);
          goto done;
        }
        // FALLTHROUGH

      case NS_STYLE_DISPLAY_INLINE:
        kidDel = kid->GetDelegate(aPresContext);
        kidFrame = kidDel->CreateFrame(aPresContext, kid, kidIndex, this);
        NS_RELEASE(kidDel);
        break;

      default:
        NS_ASSERTION(nsnull == kidPrevInFlow, "bad prev in flow");
        nsFrame::NewFrame(&kidFrame, kid, kidIndex, this);
        break;
      }
      kidFrame->SetStyleContext(kidStyleContext);
    } else {
      kidPrevInFlow->CreateContinuingFrame(aPresContext, this, kidFrame);
    }
    NS_RELEASE(kid);
    NS_RELEASE(kidStyleContext);

    // Try to reflow the child into the available space. It might not
    // fit or might need continuing.
    nsReflowMetrics kidSize;
    ReflowStatus status = ReflowChild(kidFrame,aPresContext, kidSize,
                                      aState.availSize, pKidMaxElementSize);

    // Did the child fit?
    if ((kidSize.width > aState.availSize.width) && (nsnull != mFirstChild)) {
      // The child is too wide to fit in the available space, and it's
      // not our first child. Add the frame to our overflow list
      NS_ASSERTION(nsnull == mOverflowList, "bad overflow list");
      mOverflowList = kidFrame;
      prevKidFrame->SetNextSibling(nsnull);
      break;
    }

    // Place and size the child. We'll deal with vertical alignment when
    // we're all done
    PlaceChild(kidFrame, mChildCount, aState, kidSize, pKidMaxElementSize);

    // Link child frame into the list of children
    if (nsnull != prevKidFrame) {
      prevKidFrame->SetNextSibling(kidFrame);
    } else {
      mFirstChild = kidFrame;  // our first child
      SetFirstContentOffset(kidFrame);
    }
    prevKidFrame = kidFrame;
    mChildCount++;
    kidIndex++;

    // Did the child complete?
    if (frNotComplete == status) {
      // If the child isn't complete then it means that we've used up
      // all of our available space
      mLastContentIsComplete = PR_FALSE;
      break;
    }
    kidPrevInFlow = nsnull;
  }

done:;
  // Update the content mapping
  NS_ASSERTION(IsLastChild(prevKidFrame), "bad last child");
  SetLastContentOffset(prevKidFrame);
#ifdef NS_DEBUG
  PRInt32 len = LengthOf(mFirstChild);
  NS_ASSERTION(len == mChildCount, "bad child count");
#endif
#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
  return result;
}

NS_METHOD nsInlineFrame::ResizeReflow(nsIPresContext*  aPresContext,
                                      nsReflowMetrics& aDesiredSize,
                                      const nsSize&    aMaxSize,
                                      nsSize*          aMaxElementSize,
                                      ReflowStatus&    aStatus)
{
#ifdef NS_DEBUG
  PreReflowCheck();
#endif
//XXX not now  NS_PRECONDITION((aMaxSize.width > 0) && (aMaxSize.height > 0), "unexpected max size");

  PRBool        reflowMappedOK = PR_TRUE;

  aStatus = frComplete;  // initialize out parameter

  // Get the style molecule
  nsStyleFont* styleFont =
    (nsStyleFont*)mStyleContext->GetData(kStyleFontSID);
  nsStyleMolecule* styleMolecule =
    (nsStyleMolecule*)mStyleContext->GetData(kStyleMoleculeSID);

  // Check for an overflow list
  MoveOverflowToChildList();

  // Initialize our reflow state. We must wait until after we've processed
  // the overflow list, because our first content offset might change
  nsInlineState state(styleFont, styleMolecule, aMaxSize, aMaxElementSize);
  state.SetNumAscents(mContent->ChildCount() - mFirstContentOffset);

  // Reflow any existing frames
  if (nsnull != mFirstChild) {
    reflowMappedOK = ReflowMappedChildren(aPresContext, state);

    if (PR_FALSE == reflowMappedOK) {
      aStatus = frNotComplete;
    }
  }

  // Did we successfully relow our mapped children?
  if (PR_TRUE == reflowMappedOK) {
    // Any space left?
    if (state.availSize.width <= 0) {
      // No space left. Don't try to pull-up children or reflow unmapped
      if (NextChildOffset() < mContent->ChildCount()) {
        aStatus = frNotComplete;
      }
    } else if (NextChildOffset() < mContent->ChildCount()) {
      // Try and pull-up some children from a next-in-flow
      if (PullUpChildren(aPresContext, state)) {
        // If we still have unmapped children then create some new frames
        if (NextChildOffset() < mContent->ChildCount()) {
          aStatus = ReflowUnmappedChildren(aPresContext, state);
        }
      } else {
        // We were unable to pull-up all the existing frames from the next in flow
        aStatus = frNotComplete;
      }
    }
  }

  const nsMargin&  insets = styleMolecule->borderPadding;

  // Vertically align the children
  nscoord lineHeight =
    nsCSSLayout::VerticallyAlignChildren(aPresContext, this, styleFont,
                                         insets.top, mFirstChild, mChildCount,
                                         state.ascents, state.maxAscent);

  // XXX I don't think our return size properly accounts for the lineHeight
  // (which may not == state.maxAscent + state.maxDescent)
  // Return our size and our status
  aDesiredSize.width = state.x + insets.right;
  aDesiredSize.ascent = insets.top + state.maxAscent;
  aDesiredSize.descent = state.maxDescent + insets.bottom;
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;

#ifdef NS_DEBUG
  PostReflowCheck(aStatus);
#endif
  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////

PRIntn nsInlineFrame::GetSkipSides() const
{
  PRIntn skip = 0;
  if (nsnull != mPrevInFlow) {
    skip |= 1 << NS_SIDE_LEFT;
  }
  if (nsnull != mNextInFlow) {
    skip |= 1 << NS_SIDE_RIGHT;
  }
  return skip;
}

/////////////////////////////////////////////////////////////////////////////

// Incremental reflow support

NS_METHOD nsInlineFrame::GetReflowMetrics(nsIPresContext* aPresContext,
                                          nsReflowMetrics& aMetrics)
{
  nscoord maxAscent = 0;
  nscoord maxDescent = 0;
  nsIFrame* kid = mFirstChild;
  while (nsnull != kid) {
    nsReflowMetrics kidMetrics;
    kid->GetReflowMetrics(aPresContext, kidMetrics);
    if (kidMetrics.ascent > maxAscent) maxAscent = kidMetrics.ascent;
    if (kidMetrics.descent > maxDescent) maxDescent = kidMetrics.descent;
    kid->GetNextSibling(kid);
  }

  // XXX what about border & padding
  aMetrics.width = mRect.width;
  aMetrics.height = mRect.height;
  aMetrics.ascent = maxAscent;
  aMetrics.descent = maxDescent;

  return NS_OK;
}

/**
 * Setup aState to the state it would have had we just reflowed our
 * children up to, but not including, aSkipChild. Return the index
 * of aSkipChild in our list of children.
 */
PRIntn nsInlineFrame::RecoverState(nsIPresContext* aPresContext,
                                   nsInlineState& aState,
                                   nsIFrame* aSkipChild)
{
  // Get ascent & descent info for all the children up to but not
  // including aSkipChild. Also compute the x coordinate for where
  // aSkipChild will be place after it is reflowed.
  PRIntn i = 0;
  nsIFrame* kid = mFirstChild;
  nscoord x = aState.x;
  nscoord maxAscent = 0;
  nscoord maxDescent = 0;
  while (kid != aSkipChild) {
    nsReflowMetrics kidMetrics;
    kid->GetReflowMetrics(aPresContext, kidMetrics);
    aState.ascents[i] = kidMetrics.ascent;
    if (kidMetrics.ascent > maxAscent) maxAscent = kidMetrics.ascent;
    if (kidMetrics.descent > maxDescent) maxDescent = kidMetrics.descent;

    // XXX Factor in left and right margins
    x += kidMetrics.width;

    kid->GetNextSibling(kid);
    i++;
  }
  aState.maxAscent = maxAscent;
  aState.maxDescent = maxDescent;
  aState.x = x;
  return (nsnull == aSkipChild) ? 0 : i;
}

NS_METHOD nsInlineFrame::IncrementalReflow(nsIPresContext*  aPresContext,
                                           nsReflowMetrics& aDesiredSize,
                                           const nsSize&    aMaxSize,
                                           nsReflowCommand& aReflowCommand,
                                           ReflowStatus&    aStatus)
{
  aStatus = frComplete;  // initialize out parameter

#if 0
  if (aReflowCommand.GetTarget() == this) {
    // NOTE: target frame is first in flow even when reflow commands
    // applies to some piece of content in any of the next-in-flows
    nsInlineFrame* flow = this;

    switch (aReflowCommand.GetType()) {
    case nsReflowCommand::rcContentAppended:
      // Get to last-in-flow where the append is occuring
      while (nsnull != flow->mNextInFlow) {
        flow = flow->mNextInFlow;
      }
      aStatus = flow->ReflowUnmappedChildren(...);
      break;

    case nsReflowCommand::rcContentInserted:
      // XXX where did the insertion point go?

      // Get to correct next-in-flow
      // XXX this is more complicated when dealing with pseudo children
      PRInt32 contentIndex = 0;
      while (nsnull != flow->mNextInFlow) {
        // XXX the last child we have may be continued into our
        // next-in-flow which means that endIndex is wrong???
        PRInt32 endIndex = contentIndex + mChildCount;
        if ((contentIndex <= aIndexInParent) && (aIndexInParent < endIndex)) {
          break;
        }
        flow = flow->mNextInFlow;
        contentIndex = endIndex;
      }

      // Create frame and insert it into the sibling list at the right
      // spot. Reflow it. Adjust position of siblings including
      // pushing frames that don't fit.
      flow->ReflowUnmappedChild(..., index information);
      break;

    case nsReflowCommand::rcContentDeleted:
      // XXX what about will-delete, did-delete?

      // Find the affected flow blocks and remove the child and any
      // continuations it has from the flow. Every flow block except
      // the first one will recieve a reflow command to cleanup after
      // the first flow block does it's clean up. Except for the
      // post-processing (impacting sibling frames) the code is
      // generic across container types.
      break;

    case nsReflowCommand::rcContentChanged:
      // XXX The piece of content affected may change stylistically
      // from inline to block. If it does and it's not our first kid
      // then we just push it and let our next-in-flow reflow it.

      // Either: a) delete the old frame, create a new frame, reflow -or-
      // b) ResizeReflow the affected child

      // If the reflow status is incomplete then the remaining
      // children on the line get pushed; else only push children that
      // don't fit.  If no pushing occurs then maybe we can pullup a
      // child; if we do then generate reflow commands for our
      // next-in-flows to deal with a pull event.
      break;
    }

  } else {
    // XXX Tuneup? if we save the child's continuation status and its
    // current bounding rect and then pass the command down and it
    // comes back with the same bounds in aDesiredSize and with the
    // same continuation status then we are done, right?

    // Get the next frame that the reflow command is targeted at.
    // This will be one of my children.
    nsIFrame* nextInChain = aReflowCommand.GetNext();
    NS_ASSERTION(this == nextInChain->GetGeometricParent(), "bad reflow cmd");

    // Recover the reflow state as if we had reflowed our children up
    // to but not including the child that is next in the reflow chain
    nsStyleMolecule* styleMolecule =
      (nsStyleMolecule*)mStyleContext->GetData(kStyleMoleculeSID);
    nsInlineState state(styleMolecule, aMaxSize, nsnull);
    state.SetNumAscents(mChildCount);
    PRIntn nextIndex = RecoverState(aPresContext, state, nextInChain);

    // Save away the current location and size of the child which the
    // reflow command is targeted towards. This isn't strictly
    // necessary, but our child might tamper with it's x/y
    // XXX bother?
    nsRect oldBounds;
    nextInChain->GetRect(oldBounds);

    // Now pass reflow command down to our child to handle
    nsReflowMetrics kidMetrics;
    aStatus = aReflowCommand.Next(kidMetrics, aMaxSize, kid);
    // XXX what do we do when the nextInChain's completion status changes?
    // XXX if kid == LastChild() then mLastContentIsComplete needs updating

    // Update placement information for the impacted child frame and
    // any other frames following the child frame.
    if ((oldBounds.width == kidMetrics.width) &&
        (oldBounds.height == kidMetrics.height)) {
      // The child didn't change as far as we can tell. Nobody needs
      // to be moved.

      // XXX what about a child who's shape is the same but who's
      // reflow status is frNotComplete?

      // Note: a child that used to be continued and is no longer
      // continued and whose size is unchanged: we don't care because
      // as far as we are concerned nothing happened to us. However,
      // our next-in-flow might get torched (our parent will deal with
      // that).

      // XXX figure out the right status to return
    } else {
      // Factor in ascent information from the updated child
      state.ascents[nextIndex] = kidMetrics.ascent;
      if (kidMetrics.ascent > state.maxAscent) {
        state.maxAscent = kidMetrics.ascent;
      }
      if (kidMetrics.descent > state.maxDescent) {
        state.maxDescent = kidMetrics.descent;
      }

      // Update other children that are impacted by the change in
      // nextInChain. In addition, re-apply vertical alignment and
      // relative positioning to the children on the line.
      aStatus = AdjustChildren(aPresContext, aDesiredSize, aState, kid,
                               kidMetrics, aStatus);
    }
  }
#endif

  return NS_OK;
}


// In order to execute the vertical alignment code after incremental
// reflow of the inline frame, we need to reposition any child frames
// that were relatively positioned back to their computed x origin.
// This should probably be done as a pre-alignment computation (and it
// can be avoided if there are no relatively positioned children).

/**
 * Adjust the position of all the children in this frame.
 *
 * The children after aKid in the list of children are slid over by
 * dx.
 *
 * Once the x and y coordinates have been set, then the vertical
 * alignment code is executed to place the children vertically and to
 * compute the final height of our frame.
 *
 * If one of our children spills over the end then push it to the
 * next-in-flow or to our overflow list.
 */
nsIFrame::ReflowStatus
nsInlineFrame::AdjustChildren(nsIPresContext* aPresContext,
                              nsReflowMetrics& aDesiredSize,
                              nsInlineState& aState,
                              nsIFrame* aKid,
                              nsReflowMetrics& aKidMetrics,
                              ReflowStatus aKidReflowStatus)
{
  nsStyleMolecule* mol = aState.mol;
  nscoord xr = aState.availSize.width + mol->borderPadding.left;
  nscoord remainingSpace = xr - aState.x;
  nscoord x = aState.x;

  // Slide all of the children over following aKid
  nsIFrame* kid = aKid;
  nsRect r;
  while (nsnull != kid) {
    kid->GetRect(r);
    if (r.x != x) {
      kid->MoveTo(x, r.y);
    }
    x += r.width;
    // XXX factor in left and right margins
    kid->GetNextSibling(kid);
  }

  // Vertically align the children
  const nsMargin& insets = mol->borderPadding;
  nsCSSLayout::VerticallyAlignChildren(aPresContext, this, aState.font,
                                       insets.top, mFirstChild, mChildCount,
                                       aState.ascents, aState.maxAscent);

  // XXX relative position children, if any

  // XXX adjust mLastContentOffset if we push
  // XXX if we push, generate a reflow command

  return frComplete;
}

// My container has new content at the end of it. Create frames for
// the appended content and then generate an incremental reflow
// command for ourselves.
NS_METHOD nsInlineFrame::ContentAppended(nsIPresShell* aShell,
                                         nsIPresContext* aPresContext,
                                         nsIContent* aContainer)
{
  // Get the last in flow
  nsInlineFrame* flow = (nsInlineFrame*)GetLastInFlow();

  // Get index of where the content has been appended
  PRInt32 kidIndex = flow->NextChildOffset();
  PRInt32 startIndex = kidIndex;
  nsIFrame* prevKidFrame;

  flow->LastChild(prevKidFrame);
  // Create frames for each new child
  for (;;) {
    // Get the next content object
    nsIContent* kid = mContent->ChildAt(kidIndex);
    if (nsnull == kid) {
      break;
    }

    nsIContentDelegate* del = kid->GetDelegate(aPresContext);
    nsIFrame* kidFrame = del->CreateFrame(aPresContext, kid, kidIndex, flow);
    NS_RELEASE(del);
    NS_RELEASE(kid);

    // Append kidFrame to the sibling list
    prevKidFrame->SetNextSibling(kidFrame);
    prevKidFrame = kidFrame;
    kidIndex++;
  }
  flow->SetLastContentOffset(prevKidFrame);

  // Now generate a reflow command for flow
  if (aContainer == mContent) {
    nsReflowCommand* rc =
      new nsReflowCommand(aPresContext, flow, nsReflowCommand::FrameAppended,
                          startIndex);
    aShell->AppendReflowCommand(rc);
  }
  return NS_OK;
}
