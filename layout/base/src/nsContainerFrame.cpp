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
#include "nsContainerFrame.h"
#include "nsIContent.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsIRunaround.h"
#include "nsISpaceManager.h"
#include "nsIStyleContext.h"
#include "nsIContentDelegate.h"
#include "nsRect.h"
#include "nsPoint.h"
#include "nsGUIEvent.h"
#include "nsStyleConsts.h"
#include "nsIView.h"

#ifdef NS_DEBUG
#undef NOISY
#else
#undef NOISY
#endif

static NS_DEFINE_IID(kIRunaroundIID, NS_IRUNAROUND_IID);

nsContainerFrame::nsContainerFrame(nsIContent* aContent,
                                   PRInt32     aIndexInParent,
                                   nsIFrame*   aParent)
  : nsSplittableFrame(aContent, aIndexInParent, aParent),
    mLastContentIsComplete(PR_TRUE)
{
}

nsContainerFrame::~nsContainerFrame()
{
  // Delete our child frames before doing anything else. In particular
  // we do all of this before our base class releases it's hold on the
  // view.
  for (nsIFrame* child = mFirstChild; child; ) {
    nsIFrame* nextChild;
     
    child->GetNextSibling(nextChild);
    child->DeleteFrame();
    child = nextChild;
  }
}

void nsContainerFrame::PrepareContinuingFrame(nsIPresContext* aPresContext,
                                              nsIFrame* aParent,
                                              nsContainerFrame* aContFrame)
{
  // Append the continuing frame to the flow
  aContFrame->AppendToFlow(this);

  // Initialize it's content offsets. Note that we assume for now that
  // the continuingFrame will map the remainder of the content and
  // that therefore mLastContentIsComplete will be true.
  PRInt32 nextOffset = NextChildOffset();
  aContFrame->mFirstContentOffset = nextOffset;
  aContFrame->mLastContentOffset = nextOffset;
  aContFrame->mLastContentIsComplete = PR_TRUE;

  // Resolve style for the continuing frame and set its style context.
  // XXX presumptive
  nsIStyleContext* styleContext =
    aPresContext->ResolveStyleContextFor(mContent, aParent);
  aContFrame->SetStyleContext(styleContext);
  NS_RELEASE(styleContext);
}

NS_METHOD nsContainerFrame::CreateContinuingFrame(nsIPresContext* aPresContext,
                                                  nsIFrame*       aParent,
                                                  nsIFrame*&      aContinuingFrame)
{
  nsIContentDelegate* contentDelegate = mContent->GetDelegate(aPresContext);

  aContinuingFrame = contentDelegate->CreateFrame(aPresContext, mContent,
                                                  mIndexInParent, aParent);
  NS_RELEASE(contentDelegate);

  PrepareContinuingFrame(aPresContext, aParent, (nsContainerFrame*)aContinuingFrame);
  return NS_OK;
}


/////////////////////////////////////////////////////////////////////////////
// Child frame enumeration

NS_METHOD nsContainerFrame::ChildCount(PRInt32& aChildCount) const
{
  aChildCount = mChildCount;
  return NS_OK;
}

NS_METHOD nsContainerFrame::ChildAt(PRInt32 aIndex, nsIFrame*& aFrame) const
{
  // Check that the index is in range
  if ((aIndex < 0) || (aIndex >= mChildCount)) {
    aFrame = nsnull;
    return NS_OK;
  }

  aFrame = mFirstChild;
  while ((aIndex-- > 0) && (aFrame != nsnull)) {
    aFrame->GetNextSibling(aFrame);
  }
  return NS_OK;
}

NS_METHOD nsContainerFrame::IndexOf(const nsIFrame* aChild, PRInt32& aIndex) const
{
  aIndex = -1;  // initialize out parameter

  for (nsIFrame* f = mFirstChild; f != nsnull; f->GetNextSibling(f)) {
    aIndex++;

    if (f == aChild)
      break;
  }

  return NS_OK;
}

NS_METHOD nsContainerFrame::FirstChild(nsIFrame*& aFirstChild) const
{
  aFirstChild = mFirstChild;
  return NS_OK;
}

NS_METHOD nsContainerFrame::NextChild(const nsIFrame* aChild, nsIFrame*& aNextChild) const
{
  NS_PRECONDITION(aChild != nsnull, "null pointer");
  aChild->GetNextSibling(aNextChild);
  return NS_OK;
}

NS_METHOD nsContainerFrame::PrevChild(const nsIFrame* aChild, nsIFrame*& aPrevChild) const
{
  NS_PRECONDITION(aChild != nsnull, "null pointer");

  if (mFirstChild == aChild) {
    aPrevChild = nsnull;
  } else {
    aPrevChild = mFirstChild;

    while ((aPrevChild != nsnull)) {
      nsIFrame* nextChild;
         
      aPrevChild->GetNextSibling(nextChild);
      if (nextChild == aChild) {
        break;
      }

      aPrevChild = nextChild;
    }
  }

  return NS_OK;
}

NS_METHOD nsContainerFrame::LastChild(nsIFrame*& aLastChild) const
{
  aLastChild = mFirstChild;

  if (nsnull != aLastChild) {
    nsIFrame* nextChild;

    aLastChild->GetNextSibling(nextChild);
    while (nextChild != nsnull) {
      aLastChild = nextChild;
      aLastChild->GetNextSibling(nextChild);
    }
  }

  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////
// Painting

NS_METHOD nsContainerFrame::Paint(nsIPresContext&      aPresContext,
                                  nsIRenderingContext& aRenderingContext,
                                  const nsRect&        aDirtyRect)
{
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect);
  return NS_OK;
}

// aDirtyRect is in our coordinate system
// child rect's are also in our coordinate system
void nsContainerFrame::PaintChildren(nsIPresContext&      aPresContext,
                                     nsIRenderingContext& aRenderingContext,
                                     const nsRect&        aDirtyRect)
{
  nsIFrame* kid = mFirstChild;
  while (nsnull != kid) {
    nsIView *pView;
     
    kid->GetView(pView);
    if (nsnull == pView) {
      nsRect kidRect;
      kid->GetRect(kidRect);
      nsRect damageArea;
      PRBool overlap = damageArea.IntersectRect(aDirtyRect, kidRect);
      if (overlap) {
        // Translate damage area into kid's coordinate system
        nsRect kidDamageArea(damageArea.x - kidRect.x, damageArea.y - kidRect.y,
                             damageArea.width, damageArea.height);
        aRenderingContext.PushState();
        aRenderingContext.Translate(kidRect.x, kidRect.y);
        kid->Paint(aPresContext, aRenderingContext, kidDamageArea);
        if (nsIFrame::GetShowFrameBorders()) {
          aRenderingContext.SetColor(NS_RGB(255,0,0));
          aRenderingContext.DrawRect(0, 0, kidRect.width, kidRect.height);
        }
        aRenderingContext.PopState();
      }
    }
    else {
      NS_RELEASE(pView);
    }
    kid->GetNextSibling(kid);
  }
}

/////////////////////////////////////////////////////////////////////////////
// Events

NS_METHOD nsContainerFrame::HandleEvent(nsIPresContext& aPresContext, 
                                        nsGUIEvent*     aEvent,
                                        nsEventStatus&  aEventStatus)
{
  aEventStatus = nsEventStatus_eIgnore;

  nsIFrame* kid = mFirstChild;
  while (nsnull != kid) {
    nsRect kidRect;
    kid->GetRect(kidRect);
    if (kidRect.Contains(aEvent->point)) {
      aEvent->point.MoveBy(-kidRect.x, -kidRect.y);
      kid->HandleEvent(aPresContext, aEvent, aEventStatus);
      aEvent->point.MoveBy(kidRect.x, kidRect.y);
      break;
    }
    kid->GetNextSibling(kid);
  }

  return NS_OK;
}

NS_METHOD nsContainerFrame::GetCursorAt(nsIPresContext& aPresContext,
                                        const nsPoint&  aPoint,
                                        nsIFrame**      aFrame,
                                        PRInt32&        aCursor)
{
  aCursor = NS_STYLE_CURSOR_INHERIT;

  nsIFrame* kid = mFirstChild;
  nsPoint tmp;
  while (nsnull != kid) {
    nsRect kidRect;
    kid->GetRect(kidRect);
    if (kidRect.Contains(aPoint)) {
      tmp.MoveTo(aPoint.x - kidRect.x, aPoint.y - kidRect.y);
      kid->GetCursorAt(aPresContext, tmp, aFrame, aCursor);
      break;
    }
    kid->GetNextSibling(kid);
  }
  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////
// Managing content mapping

// Get the offset for the next child content, i.e. the child after the
// last child that fit in us
PRInt32 nsContainerFrame::NextChildOffset() const
{
  PRInt32 result;

  if (mChildCount > 0) {
    result = mLastContentOffset;

#ifdef NS_DEBUG
    VerifyLastIsComplete();
#endif
    if (mLastContentIsComplete) {
      result++;
    }
  } else {
    result = mFirstContentOffset;
  }
  return result;
}

/**
 * Sets the first content offset based on the first child frame.
 */
void nsContainerFrame::SetFirstContentOffset(const nsIFrame* aFirstChild)
{
  NS_PRECONDITION(nsnull != aFirstChild, "bad argument");
  NS_PRECONDITION(IsChild(aFirstChild), "bad geometric parent");

  if (ChildIsPseudoFrame(aFirstChild)) {
    nsContainerFrame* pseudoFrame = (nsContainerFrame*)aFirstChild;
    mFirstContentOffset = pseudoFrame->mFirstContentOffset;
  } else {
    aFirstChild->GetIndexInParent(mFirstContentOffset);
  }
}

/**
 * Sets the last content offset based on the last child frame. If the last
 * child is a pseudo frame then it sets mLastContentIsComplete to be the same
 * as the last child's mLastContentIsComplete
 */
void nsContainerFrame::SetLastContentOffset(const nsIFrame* aLastChild)
{
  NS_PRECONDITION(nsnull != aLastChild, "bad argument");
  NS_PRECONDITION(IsChild(aLastChild), "bad geometric parent");

  if (ChildIsPseudoFrame(aLastChild)) {
    nsContainerFrame* pseudoFrame = (nsContainerFrame*)aLastChild;
    mLastContentOffset = pseudoFrame->mLastContentOffset;
#ifdef NS_DEBUG
    pseudoFrame->VerifyLastIsComplete();
#endif
    mLastContentIsComplete = pseudoFrame->mLastContentIsComplete;
  } else {
    aLastChild->GetIndexInParent(mLastContentOffset);
  }
#ifdef NS_DEBUG
  if (mLastContentOffset < mFirstContentOffset) {
    DumpTree();
  }
#endif
  NS_ASSERTION(mLastContentOffset >= mFirstContentOffset, "unexpected content mapping");
}

/////////////////////////////////////////////////////////////////////////////
// Pseudo frame related

// Returns true if this frame is being used a pseudo frame
PRBool nsContainerFrame::IsPseudoFrame() const
{
  PRBool  result = PR_FALSE;

  if (nsnull != mGeometricParent) {
    nsIContent* parentContent;
     
    mGeometricParent->GetContent(parentContent);
    if (parentContent == mContent) {
      result = PR_TRUE;
    }
    NS_RELEASE(parentContent);
  }

  return result;
}

// Returns true if aChild is being used as a pseudo frame
PRBool nsContainerFrame::ChildIsPseudoFrame(const nsIFrame* aChild) const
{
  NS_PRECONDITION(IsChild(aChild), "bad geometric parent");

  nsIContent* childContent;
  PRBool      result;
   
  aChild->GetContent(childContent);
  result = PRBool(childContent == mContent);
  NS_RELEASE(childContent);
  return result;
}

/////////////////////////////////////////////////////////////////////////////
// Helper member functions

/**
 * Reflow a child frame and return the status of the reflow. If the
 * child is complete and it has next-in-flows (it was a splittable child)
 * then delete the next-in-flows.
 */
nsIFrame::ReflowStatus
nsContainerFrame::ReflowChild(nsIFrame*        aKidFrame,
                              nsIPresContext*  aPresContext,
                              nsReflowMetrics& aDesiredSize,
                              const nsSize&    aMaxSize,
                              nsSize*          aMaxElementSize)
{
  ReflowStatus status;
                                                  
  aKidFrame->ResizeReflow(aPresContext, aDesiredSize, aMaxSize, aMaxElementSize, status);

  if (frComplete == status) {
    nsIFrame* kidNextInFlow;
     
    aKidFrame->GetNextInFlow(kidNextInFlow);
    if (nsnull != kidNextInFlow) {
      // Remove all of the childs next-in-flows. Make sure that we ask
      // the right parent to do the removal (it's possible that the
      // parent is not this because we are executing pullup code)
      nsIFrame* parent;
       
      aKidFrame->GetGeometricParent(parent);
      ((nsContainerFrame*)parent)->DeleteChildsNextInFlow(aKidFrame);
    }
  }
  return status;
}

/**
 * Reflow a child frame and return the status of the reflow. If the child
 * is complete and it has next-in-flows, then delete the next-in-flows.
 *
 * Use this version if you have a space manager. Checks if the child frame
 * supports interface nsIRunaround. If it does, interface nsIRunaround is
 * used to reflow the child; otherwise interface nsIFrame is used. If the
 * child is splittable then  runaround is done using continuing frames.
 */
nsIFrame::ReflowStatus
nsContainerFrame::ReflowChild(nsIFrame*        aKidFrame,
                              nsIPresContext*  aPresContext,
                              nsStyleMolecule* aKidMol,
                              nsISpaceManager* aSpaceManager,
                              const nsSize&    aMaxSize,
                              nsRect&          aDesiredRect,
                              nsSize*          aMaxElementSize)
{
  nsIRunaround* reflowRunaround;
  ReflowStatus  status;

  // Get the band for this y-offset and see whether there are any floaters
  // that have changed the left/right edges.
  //
  // XXX In order to do this efficiently we should move all this code to
  // nsBlockFrame since it already has band data, and it's probably the only
  // one who calls this routine anyway
  nsBandData        bandData;
  nsBandTrapezoid   trapezoids[7];
  nsBandTrapezoid*  availSpace = trapezoids;
  nsSize            availSize = aMaxSize;

  bandData.trapezoids = trapezoids;
  bandData.size = 7;
  aSpaceManager->GetBandData(0, aMaxSize, bandData);

  // If there's more than one trapezoid then figure out which one determines the
  // available reflow area
  if (bandData.count == 2) {
    nsRect  r0, r1;

    trapezoids[0].GetRect(r0);
    trapezoids[1].GetRect(r1);

    // Either a left or right floater. Use whichever space is larger
    if (r1.width > r0.width) {
      availSpace = &trapezoids[1];
    }
  } else if (bandData.count == 3) {
    // Assume there are floaters on the left and right, and use the middle rect
    availSpace = &trapezoids[1];
  }

  // Does the child frame want to do continuation-based runaround?
  SplittableType  isSplittable;

  aKidFrame->IsSplittable(isSplittable);
  if (isSplittable == frSplittableNonRectangular) {
    // Reduce the max height to the band height.
    availSize.height = availSpace->yBottom - availSpace->yTop;
  }

  // Get the bounding rect of the available trapezoid
  nsRect  rect;

  availSpace->GetRect(rect);
  if (aMaxSize.width != NS_UNCONSTRAINEDSIZE) {
    if ((rect.x > 0) || (rect.XMost() < aMaxSize.width)) {
      // There are left/right floaters.
      availSize.width = rect.width;
    }
  
    // Reduce the available width by the kid's left/right margin
    availSize.width -= aKidMol->margin.left + aKidMol->margin.right;
  }

  // Does the child frame support interface nsIRunaround?
  if (NS_OK == aKidFrame->QueryInterface(kIRunaroundIID, (void**)&reflowRunaround)) {
    // Yes, the child frame wants to interact directly with the space manager.
    nscoord tx = rect.x + aKidMol->margin.left;

    // Translate the local coordinate space to the current left edge plus any
    // left margin the child wants
    aSpaceManager->Translate(tx, 0);

    reflowRunaround->ResizeReflow(aPresContext, aSpaceManager, availSize,
                                  aDesiredRect, aMaxElementSize, status);
    aDesiredRect.x += rect.x;

    // Translate back
    aSpaceManager->Translate(-tx, 0);

  } else {
    // No, use interface nsIFrame instead.
    nsReflowMetrics desiredSize;

    aKidFrame->ResizeReflow(aPresContext, desiredSize, availSize,
                            aMaxElementSize, status);

    // Return the desired rect
    aDesiredRect.x = rect.x;
    aDesiredRect.y = 0;
    aDesiredRect.width = desiredSize.width;
    aDesiredRect.height = desiredSize.height;
  }

  if (frComplete == status) {
    nsIFrame* kidNextInFlow;
     
    aKidFrame->GetNextInFlow(kidNextInFlow);
    if (nsnull != kidNextInFlow) {
      // Remove all of the childs next-in-flows. Make sure that we ask
      // the right parent to do the removal (it's possible that the
      // parent is not this because we are executing pullup code)
      nsIFrame* parent;
       
      aKidFrame->GetGeometricParent(parent);
      ((nsContainerFrame*)parent)->DeleteChildsNextInFlow(aKidFrame);
    }
  }
  return status;
}

/**
 * Remove and delete aChild's next-in-flow(s). Updates the sibling and flow
 * pointers
 *
 * Updates the child count and content offsets of all containers that are
 * affected
 *
 * @param   aChild child this child's next-in-flow
 * @return  PR_TRUE if successful and PR_FALSE otherwise
 */
PRBool nsContainerFrame::DeleteChildsNextInFlow(nsIFrame* aChild)
{
  NS_PRECONDITION(IsChild(aChild), "bad geometric parent");

  nsIFrame*         nextInFlow;
  nsContainerFrame* parent;
   
  aChild->GetNextInFlow(nextInFlow);
  NS_PRECONDITION(nsnull != nextInFlow, "null next-in-flow");
  nextInFlow->GetGeometricParent((nsIFrame*&)parent);

  // If the next-in-flow has a next-in-flow then delete it, too (and
  // delete it first).
  nsIFrame* nextNextInFlow;

  nextInFlow->GetNextInFlow(nextNextInFlow);
  if (nsnull != nextNextInFlow) {
    ((nsContainerFrame*)parent)->DeleteChildsNextInFlow(nextInFlow);
  }

#ifdef NS_DEBUG
  PRInt32   childCount;
  nsIFrame* firstChild;

  nextInFlow->ChildCount(childCount);
  nextInFlow->FirstChild(firstChild);

  NS_ASSERTION(childCount == 0, "deleting !empty next-in-flow");

  NS_ASSERTION((0 == childCount) && (nsnull == firstChild), "deleting !empty next-in-flow");
#endif

  // Disconnect the next-in-flow from the flow list
  nextInFlow->BreakFromPrevFlow();

  // Take the next-in-flow out of the parent's child list
  if (parent->mFirstChild == nextInFlow) {
    nextInFlow->GetNextSibling(parent->mFirstChild);
    if (nsnull != parent->mFirstChild) {
      parent->SetFirstContentOffset(parent->mFirstChild);
      if (parent->IsPseudoFrame()) {
        parent->PropagateContentOffsets();
      }
    }

    // When a parent loses it's last child and that last child is a
    // pseudo-frame then the parent's content offsets are now wrong.
    // However, we know that the parent will eventually be reflowed
    // in one of two ways: it will either get a chance to pullup
    // children or it will be deleted because it's prev-in-flow
    // (e.g. this) is complete. In either case, the content offsets
    // will be repaired.

  } else {
    nsIFrame* nextSibling;

    // Because the next-in-flow is not the first child of the parent
    // we know that it shares a parent with aChild. Therefore, we need
    // to capture the next-in-flow's next sibling (in case the
    // next-in-flow is the last next-in-flow for aChild AND the
    // next-in-flow is not the last child in parent)
    NS_ASSERTION(((nsContainerFrame*)parent)->IsChild(aChild), "screwy flow");
    aChild->GetNextSibling(nextSibling);
    NS_ASSERTION(nextSibling == nextInFlow, "unexpected sibling");

    nextInFlow->GetNextSibling(nextSibling);
    aChild->SetNextSibling(nextSibling);
  }

  // Delete the next-in-flow frame and adjust it's parent's child count
  nextInFlow->DeleteFrame();
  parent->mChildCount--;
#ifdef NS_DEBUG
  if (0 != parent->mChildCount) {
    parent->CheckContentOffsets();
  }

  aChild->GetNextInFlow(nextInFlow);
  NS_POSTCONDITION(nsnull == nextInFlow, "non null next-in-flow");
#endif

  return PR_TRUE;
}

void nsContainerFrame::PropagateContentOffsets()
{
  NS_PRECONDITION(IsPseudoFrame(), "not a pseudo frame");
  nsContainerFrame* parent = (nsContainerFrame*)mGeometricParent;

  if (nsnull != parent) {
    // If we're the first child frame then update our parent's content offset
    if (parent->mFirstChild == this) {
      parent->mFirstContentOffset = mFirstContentOffset;
    }

    // If we're the last child frame then update our parent's content offset
    if (nsnull == mNextSibling) {
      parent->mLastContentOffset = mLastContentOffset;
      parent->mLastContentIsComplete = mLastContentIsComplete;
    }

    // If the parent is being used as a pseudo frame then we need to propagate
    // the content offsets upwards to its parent frame
    if (parent->IsPseudoFrame()) {
      parent->PropagateContentOffsets();
    }
  }
}

/**
 * Push aFromChild and its next siblings to the next-in-flow. Change the
 * geometric parent of each frame that's pushed. If there is no next-in-flow
 * the frames are placed on the overflow list (and the geometric parent is
 * left unchanged).
 *
 * Updates the next-in-flow's child count and content offsets. Does
 * <b>not</b> update the pusher's child count or last content offset.
 *
 * @param   aFromChild the first child frame to push. It is disconnected from
 *            aPrevSibling
 * @param   aPrevSibling aFromChild's previous sibling. Must not be null. It's
 *            an error to push a parent's first child frame
 * @param   aNextInFlowsLastChildIsComplete the next-in-flow's
 *            mLastContentIsComplete flag. This is used when refilling an
 *            empty next-in-flow that was drained by the caller.
 *
 */
// Note: we cannot VerifyLastIsComplete here because the caller is
// responsible for setting it.
void nsContainerFrame::PushChildren(nsIFrame* aFromChild,
                                    nsIFrame* aPrevSibling,
                                    PRBool aNextInFlowsLastContentIsComplete)
{
  NS_PRECONDITION(nsnull != aFromChild, "null pointer");
  NS_PRECONDITION(nsnull != aPrevSibling, "pushing first child");
#ifdef NS_DEBUG
  nsIFrame* prevNextSibling;

  aPrevSibling->GetNextSibling(prevNextSibling);
  NS_PRECONDITION(prevNextSibling == aFromChild, "bad prev sibling");
#endif

  // Disconnect aFromChild from its previous sibling
  aPrevSibling->SetNextSibling(nsnull);

  // Do we have a next-in-flow?
  nsContainerFrame* nextInFlow = (nsContainerFrame*)mNextInFlow;
  if (nsnull != nextInFlow) {
    PRInt32   numChildren = 0;
    nsIFrame* lastChild = nsnull;

#ifdef NOISY
    ListTag(stdout);
    printf(": pushing kids (childCount=%d)\n", mChildCount);
    {
      nsContainerFrame* flow = (nsContainerFrame*) mNextInFlow;
      while (flow != 0) {
        printf("  %p: [%d,%d,%c]\n",
               flow, flow->mFirstContentOffset, flow->mLastContentOffset,
               (flow->mLastContentIsComplete ? 'T' : 'F'));
        flow = (nsContainerFrame*) flow->mNextInFlow;
      }
    }
#endif
    // Compute the number of children being pushed, and for each child change
    // its geometric parent. Remember the last child
    for (nsIFrame* f = aFromChild; nsnull != f; f->GetNextSibling(f)) {
      numChildren++;
#ifdef NOISY
      printf("  ");
      ((nsFrame*)f)->ListTag(stdout);
      printf("\n");
#endif
      lastChild = f;
      f->SetGeometricParent(nextInFlow);

      nsIFrame* contentParent;

      f->GetContentParent(contentParent);
      if (this == contentParent) {
        f->SetContentParent(nextInFlow);
      }
    }
    NS_ASSERTION(numChildren > 0, "no children to push");

    // Prepend the frames to our next-in-flow's child list
    lastChild->SetNextSibling(nextInFlow->mFirstChild);
    nextInFlow->mFirstChild = aFromChild;

    // Update our next-in-flow's first content offset and child count
    nextInFlow->SetFirstContentOffset(aFromChild);
    if (0 == nextInFlow->mChildCount) {
      nextInFlow->SetLastContentOffset(lastChild);
      // If the child is pseudo-frame then SetLastContentOffset will
      // have updated the next-in-flow's mLastContentIsComplete flag,
      // otherwise we have to do it.
      if (!nextInFlow->ChildIsPseudoFrame(lastChild)) {
        nsIFrame* lastChildNextInFlow;

        lastChild->GetNextInFlow(lastChildNextInFlow);
        nextInFlow->mLastContentIsComplete = PRBool(nsnull == lastChildNextInFlow);
      }
    }
    nextInFlow->mChildCount += numChildren;
#ifdef NS_DEBUG
    nextInFlow->VerifyLastIsComplete();
#endif

    // If the next-in-flow is being used as a pseudo frame then we need
    // to propagate the content offsets upwards to its parent frame
    if (nextInFlow->IsPseudoFrame()) {
      nextInFlow->PropagateContentOffsets();
    }

#ifdef NOISY
    ListTag(stdout);
    printf(": push kids done (childCount=%d)\n", mChildCount);
    {
      nsContainerFrame* flow = (nsContainerFrame*) mNextInFlow;
      while (flow != 0) {
        printf("  %p: [%d,%d,%c]\n",
               flow, flow->mFirstContentOffset, flow->mLastContentOffset,
               (flow->mLastContentIsComplete ? 'T' : 'F'));
        flow = (nsContainerFrame*) flow->mNextInFlow;
      }
    }
#endif
  } else {
    // Add the frames to our overflow list
    NS_ASSERTION(nsnull == mOverflowList, "bad overflow list");
#ifdef NOISY
    ListTag(stdout);
    printf(": pushing kids to my overflow list\n");
#endif
    mOverflowList = aFromChild;
  }
}

// Note: mLastContentIsComplete is not set correctly by this routine
// (we don't always know the correct value at this time)
nsIFrame* nsContainerFrame::PullUpOneChild(nsContainerFrame* aNextInFlow,
                                           nsIFrame*         aLastChild)
{
  NS_PRECONDITION(nsnull != aNextInFlow, "null ptr");
#ifdef NS_DEBUG
  if (nsnull != aLastChild) {
    NS_PRECONDITION(IsLastChild(aLastChild), "bad last child");
  }
#endif

  // Get first available frame from the next-in-flow
  NS_ASSERTION(nsnull == aNextInFlow->mOverflowList, "bad next in flow");
  nsIFrame* kidFrame = aNextInFlow->mFirstChild;
  if (nsnull == kidFrame) {
    return nsnull;
  }

#ifdef NOISY
    ListTag(stdout);
    printf(": pulled ");
    kidFrame->ListTag(stdout);
    printf("\n");
#endif

  // Take the frame away from the next-in-flow. Update it's first
  // content offset and propagate upward the offset if the
  // next-in-flow is a pseudo-frame.
  kidFrame->GetNextSibling(aNextInFlow->mFirstChild);
  aNextInFlow->mChildCount--;
  if (nsnull != aNextInFlow->mFirstChild) {
    aNextInFlow->SetFirstContentOffset(aNextInFlow->mFirstChild);
    if (aNextInFlow->IsPseudoFrame()) {
      aNextInFlow->PropagateContentOffsets();
    }
  } else {
#ifdef NOISY
    ListTag(stdout);
    printf(": drained next-in-flow %p\n", aNextInFlow);
#endif
  }

  // Now give the frame to this container
  kidFrame->SetGeometricParent(this);

  nsIFrame* contentParent;

  kidFrame->GetContentParent(contentParent);
  if (aNextInFlow == contentParent) {
    kidFrame->SetContentParent(this);
  }
  if (nsnull == aLastChild) {
    mFirstChild = kidFrame;
    SetFirstContentOffset(kidFrame);
    // If we are a pseudo-frame propagate our first offset upward
    if (IsPseudoFrame()) {
      PropagateContentOffsets();
    }
  } else {
    aLastChild->SetNextSibling(kidFrame);
  }
  kidFrame->SetNextSibling(nsnull);
  mChildCount++;

  return kidFrame;
}

/**
 * Moves any frames on the overflwo lists (the prev-in-flow's overflow list and
 * the receiver's overflow list) to the child list.
 *
 * Updates this frame's child count and content mapping.
 *
 * @return  PR_TRUE if any frames were moved and PR_FALSE otherwise
 */
PRBool nsContainerFrame::MoveOverflowToChildList()
{
  PRBool  result = PR_FALSE;

  // Check for an overflow list with our prev-in-flow
  nsContainerFrame* prevInFlow = (nsContainerFrame*)mPrevInFlow;
  if (nsnull != prevInFlow) {
    if (nsnull != prevInFlow->mOverflowList) {
      NS_ASSERTION(nsnull == mFirstChild, "bad overflow list");
      AppendChildren(prevInFlow->mOverflowList);
      prevInFlow->mOverflowList = nsnull;
      result = PR_TRUE;
    }
  }

  // It's also possible that we have an overflow list for ourselves
  if (nsnull != mOverflowList) {
    NS_ASSERTION(nsnull != mFirstChild, "overflow list but no mapped children");
    AppendChildren(mOverflowList, PR_FALSE);
    mOverflowList = nsnull;
    result = PR_TRUE;
  }

  return result;
}

/**
 * Append child list starting at aChild to this frame's child list. Used for
 * processing of the overflow list.
 *
 * Updates this frame's child count and content mapping.
 *
 * @param   aChild the beginning of the child list
 * @param   aSetParent if true each child's geometric (and content parent if
 *            they're the same) parent is set to this frame.
 */
void nsContainerFrame::AppendChildren(nsIFrame* aChild, PRBool aSetParent)
{
  // Link the frames into our child frame list
  if (nsnull == mFirstChild) {
    // We have no children so aChild becomes the first child
    mFirstChild = aChild;
  } else {
    nsIFrame* lastChild;

    LastChild(lastChild);
    lastChild->SetNextSibling(aChild);
  }

  // Update our child count and last content offset
  nsIFrame* lastChild;
  for (nsIFrame* f = aChild; nsnull != f; f->GetNextSibling(f)) {
    lastChild = f;
    mChildCount++;

    // Reset the geometric parent if requested
    if (aSetParent) {
      nsIFrame* geometricParent;
      nsIFrame* contentParent;

      f->GetGeometricParent(geometricParent);
      f->GetContentParent(contentParent);
      if (contentParent == geometricParent) {
        f->SetContentParent(this);
      }
      f->SetGeometricParent(this);
    }
  }

  // Update our content mapping
  if (mFirstChild == aChild) {
    SetFirstContentOffset(mFirstChild);
  }
  SetLastContentOffset(lastChild);
  if (!ChildIsPseudoFrame(lastChild)) {
    nsIFrame* nextInFlow;

    lastChild->GetNextInFlow(nextInFlow);
    mLastContentIsComplete = PRBool(nsnull == nextInFlow);
  }

#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
}

/**
 * Called after pulling-up children from the next-in-flow. Adjusts the first
 * content offset of all the empty next-in-flows
 *
 * It's an error to call this function if all of the next-in-flow frames
 * are empty.
 *
 * @return PR_TRUE if successful and PR_FALSE if all the next-in-flows are
 *           empty
 */
void nsContainerFrame::AdjustOffsetOfEmptyNextInFlows()
{
  // If the first next-in-flow is not empty then the caller drained
  // more than one next-in-flow and then pushed back into it's
  // immediate next-in-flow.
  nsContainerFrame* nextInFlow = (nsContainerFrame*)mNextInFlow;
  PRInt32 nextOffset;
  if ((nsnull != nextInFlow) && (nsnull != nextInFlow->mFirstChild)) {
    // Use the offset from this frame's first next-in-flow (because
    // it's not empty) to propagate into the next next-in-flow (and
    // onward if necessary)
    nextOffset = nextInFlow->NextChildOffset();

    // Use next next-in-flow
    nextInFlow = (nsContainerFrame*) nextInFlow->mNextInFlow;

    // When this happens, there *must* be a next next-in-flow and the
    // next next-in-flow must be empty.
    NS_ASSERTION(nsnull != nextInFlow, "bad adjust offsets");
    NS_ASSERTION(nsnull == nextInFlow->mFirstChild, "bad adjust offsets");
  } else {
    // Use our offset (since our next-in-flow is empty) to propagate
    // into our empty next-in-flows
    nextOffset = NextChildOffset();
  }

  while (nsnull != nextInFlow) {
    if (nsnull == nextInFlow->mFirstChild) {
      NS_ASSERTION(nextInFlow->IsEmpty(), "bad state");
      nextInFlow->mFirstContentOffset = nextOffset;
      // If the next-in-flow is a pseudo-frame then we need to have it
      // update it's parents offsets too.
      if (nextInFlow->IsPseudoFrame()) {
        nextInFlow->PropagateContentOffsets();
      }
    } else {
      // We found a non-empty frame. Verify that its first content offset
      // is correct
      NS_ASSERTION(nextInFlow->mFirstContentOffset == nextOffset, "bad first content offset");
      return;
    }

    nextInFlow = (nsContainerFrame*)nextInFlow->mNextInFlow;
  }

  // Make sure that all the next-in-flows weren't empty
  NS_ASSERTION(nsnull != ((nsContainerFrame*)mNextInFlow)->mFirstChild,
               "Every next-in-flow is empty!");
}

/////////////////////////////////////////////////////////////////////////////
// Debugging

NS_METHOD nsContainerFrame::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);

  // Output the tag
  ListTag(out);

  // Output the first/last content offset
  fprintf(out, "[%d,%d,%c] pif=%p nif=%p",
          mFirstContentOffset, mLastContentOffset,
          (mLastContentIsComplete ? 'T' : 'F'),
          mPrevInFlow, mNextInFlow);

  // Output the rect
  out << mRect;

  // Output the children
  if (mChildCount > 0) {
    if (!mLastContentIsComplete) {
      fputs(", complete=>false ", out);
    }

    fputs("<\n", out);
    for (nsIFrame* child = mFirstChild; child; NextChild(child, child)) {
      child->List(out, aIndent + 1);
    }
    for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);
    fputs(">\n", out);
  } else {
    fputs("<>\n", out);
  }

  return NS_OK;
}

NS_METHOD nsContainerFrame::ListTag(FILE* out) const
{
  if ((nsnull != mGeometricParent) && IsPseudoFrame()) {
    fputs("*", out);
  }
  nsFrame::ListTag(out);
  return NS_OK;
}

#define VERIFY_ASSERT(_expr, _msg)                \
  if (!(_expr)) {                                 \
    DumpTree();                       \
  }                                               \
  NS_ASSERTION(_expr, _msg)

NS_METHOD nsContainerFrame::VerifyTree() const
{
#ifdef NS_DEBUG
  // Check our child count
  PRInt32 len = LengthOf(mFirstChild);
  VERIFY_ASSERT(len == mChildCount, "bad child count");

  nsIFrame* lastChild;

  LastChild(lastChild);
  if (len != 0) {
    VERIFY_ASSERT(nsnull != lastChild, "bad last child");
  }
  VERIFY_ASSERT(nsnull == mOverflowList, "bad overflow list");

  // Make sure our content offsets are sane
  VERIFY_ASSERT(mFirstContentOffset <= mLastContentOffset, "bad offsets");

  // Verify child content offsets and index-in-parents
  PRInt32 offset = mFirstContentOffset;
  nsIFrame* child = mFirstChild;
  while (nsnull != child) {
    // Make sure that the child's tree is valid
    child->VerifyTree();

    // Make sure child's index-in-parent is correct
    if (ChildIsPseudoFrame(child)) {
      nsContainerFrame* pseudo = (nsContainerFrame*) child;
      if (pseudo == mFirstChild) {
        VERIFY_ASSERT(pseudo->mFirstContentOffset == offset,
                      "bad pseudo first content offset");
      }
      if (pseudo == lastChild) {
        VERIFY_ASSERT(pseudo->mFirstContentOffset == offset,
                      "bad pseudo first content offset");
        VERIFY_ASSERT(pseudo->mLastContentOffset == mLastContentOffset,
                      "bad pseudo last content offset");
        VERIFY_ASSERT(pseudo->mLastContentIsComplete == mLastContentIsComplete,
                      "bad pseudo last content is complete");
      }

      offset = pseudo->mLastContentOffset;
      if (pseudo->mLastContentIsComplete) {
        offset++;
      }
    } else {
      PRInt32 indexInParent;

      child->GetIndexInParent(indexInParent);
      VERIFY_ASSERT(offset == indexInParent, "bad child offset");

      nsIFrame* nextInFlow;
      child->GetNextInFlow(nextInFlow);
      if (nsnull == nextInFlow) {
        offset++;
      }
    }

    child->GetNextSibling(child);
  }

  // Verify that our last content offset is correct
  if (0 != mChildCount) {
    if (mLastContentIsComplete) {
      VERIFY_ASSERT(offset == mLastContentOffset + 1, "bad last offset");
    } else {
      VERIFY_ASSERT(offset == mLastContentOffset, "bad last offset");
    }
  }

  // Make sure that our flow blocks offsets are all correct
  if (nsnull == mPrevInFlow) {
    PRInt32 nextOffset = NextChildOffset();
    nsContainerFrame* nextInFlow = (nsContainerFrame*) mNextInFlow;
    while (nsnull != nextInFlow) {
      VERIFY_ASSERT(0 != nextInFlow->mChildCount, "empty next-in-flow");
      VERIFY_ASSERT(nextInFlow->GetFirstContentOffset() == nextOffset,
                    "bad next-in-flow first offset");
      nextOffset = nextInFlow->NextChildOffset();
      nextInFlow = (nsContainerFrame*) nextInFlow->mNextInFlow;
    }
  }
#endif
  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////

#ifdef NS_DEBUG
PRInt32 nsContainerFrame::LengthOf(nsIFrame* aFrame)
{
  PRInt32 result = 0;

  while (nsnull != aFrame) {
    result++;
    aFrame->GetNextSibling(aFrame);
  }

  return result;
}

PRBool nsContainerFrame::IsChild(const nsIFrame* aChild) const
{
  // Check the geometric parent
  nsIFrame* parent;

  aChild->GetGeometricParent(parent);
  if (parent != (nsIFrame*)this) {
    return PR_FALSE;
  }

  // Check that aChild is in our sibling list
  PRInt32 index;

  IndexOf(aChild, index);
  if (-1 == index) {
    return PR_FALSE;
  }

  return PR_TRUE;
}

PRBool nsContainerFrame::IsLastChild(const nsIFrame* aChild) const
{
  // Check the geometric parent
  nsIFrame* parent;

  aChild->GetGeometricParent(parent);
  if (parent != (nsIFrame*)this) {
    return PR_FALSE;
  }

  // Check that aChild is in our sibling list
  nsIFrame* lastChild;

  LastChild(lastChild);
  if (lastChild != aChild) {
    return PR_FALSE;
  }

  return PR_TRUE;
}

void nsContainerFrame::DumpTree() const
{
  nsIFrame* root = (nsIFrame*)this;
  nsIFrame* parent = mGeometricParent;

  while (nsnull != parent) {
    root = parent;
    parent->GetGeometricParent(parent);
  }

  root->List();
}

void nsContainerFrame::CheckContentOffsets()
{
  NS_PRECONDITION(nsnull != mFirstChild, "null first child");

  // Verify that our first content offset is correct
  if (ChildIsPseudoFrame(mFirstChild)) {
    nsContainerFrame* pseudoFrame = (nsContainerFrame*)mFirstChild;

    if (pseudoFrame->GetFirstContentOffset() != mFirstContentOffset) {
      DumpTree();
    }
    NS_ASSERTION(pseudoFrame->GetFirstContentOffset() == mFirstContentOffset,
                 "bad first content offset");
  } else {
    PRInt32 indexInParent;

    mFirstChild->GetIndexInParent(indexInParent);
    if (indexInParent != mFirstContentOffset) {
      DumpTree();
    }

    NS_ASSERTION(indexInParent == mFirstContentOffset, "bad first content offset");
  }

  // Verify that our last content offset is correct
  nsIFrame* lastChild;
   
  LastChild(lastChild);
  if (ChildIsPseudoFrame(lastChild)) {
    nsContainerFrame* pseudoFrame = (nsContainerFrame*)lastChild;

    NS_ASSERTION(pseudoFrame->GetLastContentOffset() == mLastContentOffset,
                 "bad last content offset");

  } else {
    PRInt32 indexInParent;

    lastChild->GetIndexInParent(indexInParent);
    NS_ASSERTION(indexInParent == mLastContentOffset, "bad last content offset");
  }
}

void nsContainerFrame::PreReflowCheck()
{
  PRInt32 len = LengthOf(mFirstChild);
  NS_ASSERTION(len == mChildCount, "bad child count");

  if (0 == mChildCount) {
    NS_ASSERTION(nsnull == mFirstChild, "bad child-count/first-child");
  } else {
    NS_ASSERTION(nsnull != mFirstChild, "bad child-count/first-child");
    CheckContentOffsets();
  }
  VerifyLastIsComplete();
}

void nsContainerFrame::PostReflowCheck(ReflowStatus aStatus)
{
  PRInt32 len = LengthOf(mFirstChild) ;
  NS_ASSERTION(len == mChildCount, "bad child count");

  if (0 == mChildCount) {
    NS_ASSERTION(nsnull == mFirstChild, "bad child-count/first-child");
  } else {
    NS_ASSERTION(nsnull != mFirstChild, "bad child-count/first-child");
    CheckContentOffsets();
  }
  VerifyLastIsComplete();
}

/**
 * A container is empty if it has no children, or it has exactly one
 * child and that child is a pseudo-frame and it's empty (recursively
 * applied if that child contains one child which is a
 * pseudo-frame...)
 */
PRBool nsContainerFrame::IsEmpty()
{
  if (0 == mChildCount) {
    return PR_TRUE;
  }
  if (mChildCount > 1) {
    return PR_FALSE;
  }
  if (ChildIsPseudoFrame(mFirstChild)) {
    return ((nsContainerFrame*)mFirstChild)->IsEmpty();
  }
  return PR_FALSE;
}

void nsContainerFrame::CheckNextInFlowOffsets()
{
  // We have to be in a safe state before we can even consider
  // checking our next-in-flows state.
  if (!SafeToCheckLastContentOffset(this)) {
    return;
  }

  PRInt32           nextOffset = NextChildOffset();
  nsContainerFrame* nextInFlow = (nsContainerFrame*)mNextInFlow;
  while (nsnull != nextInFlow) {
    if (!SafeToCheckLastContentOffset(nextInFlow)) {
      return;
    }
#ifdef NS_DEBUG
    if (nextInFlow->GetFirstContentOffset() != nextOffset) {
      DumpTree();
    }
#endif
    NS_ASSERTION(nextInFlow->GetFirstContentOffset() == nextOffset,
                 "bad next-in-flow first offset");
    // Verify the content offsets are in sync with the first/last child
    nextInFlow->CheckContentOffsets();
    nextOffset = nextInFlow->NextChildOffset();

    // Move to the next-in-flow
    nextInFlow = (nsContainerFrame*)nextInFlow->mNextInFlow;
  }
}

PRBool
nsContainerFrame::SafeToCheckLastContentOffset(nsContainerFrame* aContainer)
{
  if (0 == aContainer->mChildCount) {
    // We can't use the last content offset on an empty frame
    return PR_FALSE;
  }

  if (nsnull != aContainer->mOverflowList) {
    // If a frame has an overflow list then it's last content offset
    // cannot be used for assertion checks (because it's not done
    // being reflowed).
    return PR_FALSE;
  }

  nsIFrame* lastChild;
   
  aContainer->LastChild(lastChild);
  if (aContainer->ChildIsPseudoFrame(lastChild)) {
    // If the containers last child is a pseudo-frame then the
    // containers last content offset is determined by the child. Ask
    // the child if it's safe to use the last content offset.
    return SafeToCheckLastContentOffset((nsContainerFrame*)lastChild);
  }

  return PR_TRUE;
}

void nsContainerFrame::VerifyLastIsComplete() const
{
  if (nsnull == mFirstChild) {
    // If we have no children, our mLastContentIsComplete doesn't mean
    // anything
    return;
  }
  if (nsnull != mOverflowList) {
    // If we can an overflow list then our mLastContentIsComplete and
    // mLastContentOffset are un-verifyable because a reflow is in
    // process.
    return;
  }

  nsIFrame* lastKid;

  LastChild(lastKid);
  if (ChildIsPseudoFrame(lastKid)) {
    // When my last child is a pseudo-frame it means that our
    // mLastContentIsComplete is a copy of it's.
    nsContainerFrame* pseudoFrame = (nsContainerFrame*) lastKid;
    NS_ASSERTION(mLastContentIsComplete == pseudoFrame->mLastContentIsComplete,
                 "bad mLastContentIsComplete");
  } else {
    // If my last child has a next-in-flow then our
    // mLastContentIsComplete must be false (because our last child is
    // obviously not complete)
    nsIFrame* lastKidNextInFlow;
     
    lastKid->GetNextInFlow(lastKidNextInFlow);
    if (nsnull != lastKidNextInFlow) {
      if (mLastContentIsComplete) {
        DumpTree();
      }
      NS_ASSERTION(mLastContentIsComplete == PR_FALSE, "bad mLastContentIsComplete");
    } else {
      // We don't know what state our mLastContentIsComplete should be in.
    }
  }
}
#endif
