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
#include "nsISpaceManager.h"
#include "nsIStyleContext.h"
#include "nsRect.h"
#include "nsPoint.h"
#include "nsGUIEvent.h"
#include "nsStyleConsts.h"
#include "nsIView.h"
#include "nsVoidArray.h"
#include "nsISizeOfHandler.h"
#include "nsIReflowCommand.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLContainerFrame.h"

#ifdef NS_DEBUG
#undef NOISY
#else
#undef NOISY
#endif

nsContainerFrame::nsContainerFrame()
{
}

nsContainerFrame::~nsContainerFrame()
{
}

NS_IMETHODIMP
nsContainerFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                      nsIAtom*        aListName,
                                      nsIFrame*       aChildList)
{
//  NS_PRECONDITION(mFrames.IsEmpty(), "already initialized");

  nsresult  result;
  if (!mFrames.IsEmpty()) {
    // We already have child frames which means we've already been
    // initialized
    result = NS_ERROR_UNEXPECTED;
  } else if (aListName) {
    // All we know about is the unnamed principal child list
    result = NS_ERROR_INVALID_ARG;
  } else {
#ifdef NS_DEBUG
    nsFrame::VerifyDirtyBitSet(aChildList);
#endif
    mFrames.SetFrames(aChildList);
    result = NS_OK;
  }
  return result;
}

NS_IMETHODIMP
nsContainerFrame::Destroy(nsIPresContext& aPresContext)
{
  // Prevent event dispatch during destruction
  nsIView* view;
  GetView(&view);
  if (nsnull != view) {
    view->SetClientData(nsnull);
  }

  // Delete the primary child list
  mFrames.DestroyFrames(aPresContext);

  // Base class will destroy the frame
  return nsFrame::Destroy(aPresContext);
}

NS_IMETHODIMP
nsContainerFrame::DidReflow(nsIPresContext& aPresContext,
                            nsDidReflowStatus aStatus)
{
  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
                     ("enter nsContainerFrame::DidReflow: status=%d",
                      aStatus));
  // Let nsFrame position and size our view (if we have one), and clear
  // the NS_FRAME_IN_REFLOW bit
  nsresult  result = nsFrame::DidReflow(aPresContext, aStatus);

  if (NS_FRAME_REFLOW_FINISHED == aStatus) {
    // Apply DidReflow to each and every list that this frame implements
    nsIAtom* listName = nsnull;
    PRInt32 listIndex = 0;
    do {
      nsIFrame* kid;
      FirstChild(listName, &kid);
      while (nsnull != kid) {
        nsIHTMLReflow* htmlReflow;
        nsresult rv;
        rv = kid->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow);
        if (NS_SUCCEEDED(rv)) {
          htmlReflow->DidReflow(aPresContext, aStatus);
        }
        kid->GetNextSibling(&kid);
      }
      NS_IF_RELEASE(listName);
      GetAdditionalChildListName(listIndex++, &listName);
    } while(nsnull != listName);
  }

  NS_FRAME_TRACE_OUT("nsContainerFrame::DidReflow");
  return result;
}

/////////////////////////////////////////////////////////////////////////////
// Child frame enumeration

NS_IMETHODIMP
nsContainerFrame::FirstChild(nsIAtom* aListName, nsIFrame** aFirstChild) const
{
  NS_PRECONDITION(nsnull != aFirstChild, "null OUT parameter pointer");
  // We only know about the unnamed principal child list
  if (nsnull == aListName) {
    *aFirstChild = mFrames.FirstChild();
    return NS_OK;
  } else {
    *aFirstChild = nsnull;
    return NS_ERROR_INVALID_ARG;
  }
}

/////////////////////////////////////////////////////////////////////////////
// Style support

NS_IMETHODIMP
nsContainerFrame::ReResolveStyleContext(nsIPresContext* aPresContext,
                                        nsIStyleContext* aParentContext,
                                        PRInt32 aParentChange,
                                        nsStyleChangeList* aChangeList,
                                        PRInt32* aLocalChange)
{
  PRInt32 ourChange = aParentChange;
  nsresult result = nsFrame::ReResolveStyleContext(aPresContext, aParentContext, 
                                                   ourChange, aChangeList, &ourChange);
  if (NS_FAILED(result)) {
    return result;
  }

  if (aLocalChange) {
    *aLocalChange = ourChange;
  }

  if (NS_COMFALSE != result) {
    // Update all children since our context changed
    PRInt32 listIndex = 0;
    nsIAtom* childList = nsnull;
    PRInt32 childChange;
    nsIFrame* child;

    do {
      child = nsnull;
      result = FirstChild(childList, &child);
      while ((NS_SUCCEEDED(result)) && (child)) {
        result = child->ReResolveStyleContext(aPresContext, mStyleContext, 
                                              ourChange, aChangeList, &childChange);
        child->GetNextSibling(&child);
      }

      NS_IF_RELEASE(childList);
      GetAdditionalChildListName(listIndex++, &childList);
    } while (childList);

    // Update overflow list too (XXX this is not just another child list?)
    child = mOverflowFrames.FirstChild();
    while ((NS_SUCCEEDED(result)) && (nsnull != child)) {
      result = child->ReResolveStyleContext(aPresContext, mStyleContext, 
                                            ourChange, aChangeList, &childChange);
      child->GetNextSibling(&child);
    }

    // And just to be complete, update our prev-in-flows overflow list
    // too (since in theory, those frames will become our frames)
    // XXX Eeek, this is potentially re-resolving these frames twice, can this be optimized?
    //     Better yet, this should just go away, overflow push/pull code should re-resolve
    //     as needed for parent changes
    if (nsnull != mPrevInFlow) {
      child = ((nsContainerFrame*)mPrevInFlow)->mOverflowFrames.FirstChild();
      while ((NS_SUCCEEDED(result)) && (nsnull != child)) {
        result = child->ReResolveStyleContext(aPresContext, mStyleContext,
                                              ourChange, aChangeList, &childChange);
        child->GetNextSibling(&child);
      }
    }

  }
  return result;
}

/////////////////////////////////////////////////////////////////////////////
// Painting/Events

NS_IMETHODIMP
nsContainerFrame::Paint(nsIPresContext&      aPresContext,
                        nsIRenderingContext& aRenderingContext,
                        const nsRect&        aDirtyRect,
                        nsFramePaintLayer    aWhichLayer)
{
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  return NS_OK;
}

// Paint the children of a container, assuming nothing about the
// childrens spatial arrangement. Given relative positioning, negative
// margins, etc, that's probably a good thing.
//
// Note: aDirtyRect is in our coordinate system (and of course, child
// rect's are also in our coordinate system)
void
nsContainerFrame::PaintChildren(nsIPresContext&      aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect&        aDirtyRect,
                                nsFramePaintLayer    aWhichLayer)
{
  const nsStyleDisplay* disp = (const nsStyleDisplay*)
    mStyleContext->GetStyleData(eStyleStruct_Display);

  // Child elements have the opportunity to override the visibility property
  // of their parent and display even if the parent is hidden
  PRBool clipState;

  // If overflow is hidden then set the clip rect so that children
  // don't leak out of us
  if (NS_STYLE_OVERFLOW_HIDDEN == disp->mOverflow) {
    aRenderingContext.PushState();
    aRenderingContext.SetClipRect(nsRect(0, 0, mRect.width, mRect.height),
                                  nsClipCombine_kIntersect, clipState);
  }

  nsIFrame* kid = mFrames.FirstChild();
  while (nsnull != kid) {
    PaintChild(aPresContext, aRenderingContext, aDirtyRect, kid, aWhichLayer);
    kid->GetNextSibling(&kid);
  }

  if (NS_STYLE_OVERFLOW_HIDDEN == disp->mOverflow) {
    aRenderingContext.PopState(clipState);
  }
}

// Paint one child frame
void
nsContainerFrame::PaintChild(nsIPresContext&      aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect,
                             nsIFrame*            aFrame,
                             nsFramePaintLayer    aWhichLayer)
{
  nsIView *pView;
  aFrame->GetView(&pView);
  if (nsnull == pView) {
    nsRect kidRect;
    aFrame->GetRect(kidRect);
    nsFrameState state;
    aFrame->GetFrameState(&state);

    // Compute the constrained damage area; set the overlap flag to
    // PR_TRUE if any portion of the child frame intersects the
    // dirty rect.
    nsRect damageArea;
    PRBool overlap;
    if (NS_FRAME_OUTSIDE_CHILDREN & state) {
      // If the child frame has children that leak out of our box
      // then we don't constrain the damageArea to just the childs
      // bounding rect.
      damageArea = aDirtyRect;
      overlap = PR_TRUE;
    }
    else {
      // Compute the intersection of the dirty rect and the childs
      // rect (both are in our coordinate space). This limits the
      // damageArea to just the portion that intersects the childs
      // rect.
      overlap = damageArea.IntersectRect(aDirtyRect, kidRect);
#ifdef NS_DEBUG
      if (!overlap && (0 == kidRect.width) && (0 == kidRect.height)) {
        overlap = PR_TRUE;
      }
#endif
    }

    if (overlap) {
      // Translate damage area into the kids coordinate
      // system. Translate rendering context into the kids
      // coordinate system.
      damageArea.x -= kidRect.x;
      damageArea.y -= kidRect.y;
      aRenderingContext.PushState();
      aRenderingContext.Translate(kidRect.x, kidRect.y);

      // Paint the kid
      aFrame->Paint(aPresContext, aRenderingContext, damageArea, aWhichLayer);
      PRBool clipState;
      aRenderingContext.PopState(clipState);

#ifdef NS_DEBUG
      // Draw a border around the child
      if (nsIFrame::GetShowFrameBorders() && !kidRect.IsEmpty()) {
        aRenderingContext.SetColor(NS_RGB(255,0,0));
        aRenderingContext.DrawRect(kidRect);
      }
#endif
    }
  }
}

NS_IMETHODIMP
nsContainerFrame::GetFrameForPoint(const nsPoint& aPoint, 
                                   nsIFrame**     aFrame)
{
  return GetFrameForPointUsing(aPoint, nsnull, aFrame);
}

nsresult
nsContainerFrame::GetFrameForPointUsing(const nsPoint& aPoint,
                                        nsIAtom*       aList,
                                        nsIFrame**     aFrame)
{
  nsIFrame* kid;
  nsRect kidRect;
  nsPoint tmp;
  *aFrame = this;

  FirstChild(aList, &kid);
  while (nsnull != kid) {
    kid->GetRect(kidRect);
    if (kidRect.Contains(aPoint)) {
      tmp.MoveTo(aPoint.x - kidRect.x, aPoint.y - kidRect.y);
      return kid->GetFrameForPoint(tmp, aFrame);
    }
    kid->GetNextSibling(&kid);
  }

  FirstChild(aList, &kid);
  while (nsnull != kid) {
    nsFrameState state;
    kid->GetFrameState(&state);
    if (NS_FRAME_OUTSIDE_CHILDREN & state) {
      kid->GetRect(kidRect);
      tmp.MoveTo(aPoint.x - kidRect.x, aPoint.y - kidRect.y);
      if (NS_OK == kid->GetFrameForPoint(tmp, aFrame)) {
        return NS_OK;
      }
      else {
        *aFrame = this;
      }
    }
    kid->GetNextSibling(&kid);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsContainerFrame::ReplaceFrame(nsIPresContext& aPresContext,
                               nsIPresShell&   aPresShell,
                               nsIAtom*        aListName,
                               nsIFrame*       aOldFrame,
                               nsIFrame*       aNewFrame)
{
  nsIFrame* prevFrame;
  nsIFrame* firstChild;
  nsresult  rv;

  // Get the old frame's previous sibling frame
  FirstChild(aListName, &firstChild);
  nsFrameList frames(firstChild);
  NS_ASSERTION(frames.ContainsFrame(aOldFrame), "frame is not a valid child frame");
  prevFrame = frames.GetPrevSiblingFor(aOldFrame);

  // Default implementation treats it like two separate operations
  rv = RemoveFrame(aPresContext, aPresShell, aListName, aOldFrame);
  if (NS_SUCCEEDED(rv)) {
    rv = InsertFrames(aPresContext, aPresShell, aListName, prevFrame, aNewFrame);
  }

  return rv;
}

/////////////////////////////////////////////////////////////////////////////
// Helper member functions

/**
 * Queries the child frame for the nsIHTMLReflow interface and if it's
 * supported invokes the WillReflow() and Reflow() member functions. If
 * the reflow succeeds and the child frame is complete, deletes any
 * next-in-flows using DeleteChildsNextInFlow()
 */
nsresult
nsContainerFrame::ReflowChild(nsIFrame*                aKidFrame,
                              nsIPresContext&          aPresContext,
                              nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nsReflowStatus&          aStatus)
{
  NS_PRECONDITION(aReflowState.frame == aKidFrame, "bad reflow state");

  // Query for the nsIHTMLReflow interface
  nsIHTMLReflow*  htmlReflow;
  nsresult        result;
  result = aKidFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow);
  if (NS_FAILED(result)) {
    return result;
  }

#ifdef DEBUG
  nsSize* saveMaxElementSize = aDesiredSize.maxElementSize;
#ifdef REALLY_NOISY_MAX_ELEMENT_SIZE
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = nscoord(0xdeadbeef);
    aDesiredSize.maxElementSize->height = nscoord(0xdeadbeef);
  }
#endif
#endif

  // Send the WillReflow notification, and reflow the child frame
  htmlReflow->WillReflow(aPresContext);
  result = htmlReflow->Reflow(aPresContext, aDesiredSize, aReflowState,
                              aStatus);

#ifdef DEBUG
  if (saveMaxElementSize != aDesiredSize.maxElementSize) {
    printf("nsContainerFrame: ");
    nsFrame::ListTag(stdout, aKidFrame);
    printf(" changed the maxElementSize *pointer* (baaaad boy!)\n");
  }
#ifdef REALLY_NOISY_MAX_ELEMENT_SIZE
  if ((nsnull != aDesiredSize.maxElementSize) &&
      ((nscoord(0xdeadbeef) == aDesiredSize.maxElementSize->width) ||
       (nscoord(0xdeadbeef) == aDesiredSize.maxElementSize->height))) {
    printf("nsContainerFrame: ");
    nsFrame::ListTag(stdout, aKidFrame);
    printf(" didn't set max-element-size!\n");
    aDesiredSize.maxElementSize->width = 0;
    aDesiredSize.maxElementSize->height = 0;
  }
#endif
#endif

  // If the reflow was successful and the child frame is complete, delete any
  // next-in-flows
  if (NS_SUCCEEDED(result) && NS_FRAME_IS_COMPLETE(aStatus)) {
    nsIFrame* kidNextInFlow;
    aKidFrame->GetNextInFlow(&kidNextInFlow);
    if (nsnull != kidNextInFlow) {
      // Remove all of the childs next-in-flows. Make sure that we ask
      // the right parent to do the removal (it's possible that the
      // parent is not this because we are executing pullup code)
      nsIFrame* parent;
      aKidFrame->GetParent(&parent);
      ((nsContainerFrame*)parent)->DeleteChildsNextInFlow(aPresContext,
                                                          aKidFrame);
    }
  }
  return result;
}

/**
 * Remove and delete aChild's next-in-flow(s). Updates the sibling and flow
 * pointers
 *
 * @param   aChild child this child's next-in-flow
 * @return  PR_TRUE if successful and PR_FALSE otherwise
 */
void
nsContainerFrame::DeleteChildsNextInFlow(nsIPresContext& aPresContext,
                                         nsIFrame* aChild)
{
  NS_PRECONDITION(mFrames.ContainsFrame(aChild), "bad geometric parent");

  nsIFrame*         nextInFlow;
  nsContainerFrame* parent;
   
  aChild->GetNextInFlow(&nextInFlow);
  NS_PRECONDITION(nsnull != nextInFlow, "null next-in-flow");
  nextInFlow->GetParent((nsIFrame**)&parent);

  // If the next-in-flow has a next-in-flow then delete it, too (and
  // delete it first).
  nsIFrame* nextNextInFlow;

  nextInFlow->GetNextInFlow(&nextNextInFlow);
  if (nsnull != nextNextInFlow) {
    parent->DeleteChildsNextInFlow(aPresContext, nextInFlow);
  }

  // Disconnect the next-in-flow from the flow list
  nextInFlow->BreakFromPrevFlow();

  // Take the next-in-flow out of the parent's child list
  PRBool  result = parent->mFrames.RemoveFrame(nextInFlow);
  NS_ASSERTION(result, "failed to remove frame");

  // Delete the next-in-flow frame
  nextInFlow->Destroy(aPresContext);

#ifdef NS_DEBUG
  aChild->GetNextInFlow(&nextInFlow);
  NS_POSTCONDITION(nsnull == nextInFlow, "non null next-in-flow");
#endif
}

/**
 * Push aFromChild and its next siblings to the next-in-flow. Change the
 * geometric parent of each frame that's pushed. If there is no next-in-flow
 * the frames are placed on the overflow list (and the geometric parent is
 * left unchanged).
 *
 * Updates the next-in-flow's child count. Does <b>not</b> update the
 * pusher's child count.
 *
 * @param   aFromChild the first child frame to push. It is disconnected from
 *            aPrevSibling
 * @param   aPrevSibling aFromChild's previous sibling. Must not be null. It's
 *            an error to push a parent's first child frame
 */
void
nsContainerFrame::PushChildren(nsIFrame* aFromChild, nsIFrame* aPrevSibling)
{
  NS_PRECONDITION(nsnull != aFromChild, "null pointer");
  NS_PRECONDITION(nsnull != aPrevSibling, "pushing first child");
#ifdef NS_DEBUG
  nsIFrame* prevNextSibling;
  aPrevSibling->GetNextSibling(&prevNextSibling);
  NS_PRECONDITION(prevNextSibling == aFromChild, "bad prev sibling");
#endif

  // Disconnect aFromChild from its previous sibling
  aPrevSibling->SetNextSibling(nsnull);

  if (nsnull != mNextInFlow) {
    // XXX This is not a very good thing to do. If it gets removed
    // then remove the copy of this routine that doesn't do this from
    // nsInlineFrame.
    nsContainerFrame* nextInFlow = (nsContainerFrame*)mNextInFlow;
    // When pushing and pulling frames we need to check for whether any
    // views need to be reparented.
    for (nsIFrame* f = aFromChild; f; f->GetNextSibling(&f)) {
      nsHTMLContainerFrame::ReparentFrameView(f, this, mNextInFlow);
    }
    nextInFlow->mFrames.InsertFrames(mNextInFlow, nsnull, aFromChild);
  }
  else {
    // Add the frames to our overflow list
    NS_ASSERTION(mOverflowFrames.IsEmpty(), "bad overflow list");
    mOverflowFrames.SetFrames(aFromChild);
  }
}

/**
 * Moves any frames on the overflwo lists (the prev-in-flow's overflow list and
 * the receiver's overflow list) to the child list.
 *
 * Updates this frame's child count and content mapping.
 *
 * @return  PR_TRUE if any frames were moved and PR_FALSE otherwise
 */
PRBool
nsContainerFrame::MoveOverflowToChildList()
{
  PRBool result = PR_FALSE;

  // Check for an overflow list with our prev-in-flow
  nsContainerFrame* prevInFlow = (nsContainerFrame*)mPrevInFlow;
  if (nsnull != prevInFlow) {
    if (prevInFlow->mOverflowFrames.NotEmpty()) {
      NS_ASSERTION(mFrames.IsEmpty(), "bad overflow list");
      // When pushing and pulling frames we need to check for whether any
      // views need to be reparented.
      for (nsIFrame* f = prevInFlow->mOverflowFrames.FirstChild(); f; f->GetNextSibling(&f)) {
        nsHTMLContainerFrame::ReparentFrameView(f, prevInFlow, this);
      }
      mFrames.InsertFrames(this, nsnull, prevInFlow->mOverflowFrames);
      result = PR_TRUE;
    }
  }

  // It's also possible that we have an overflow list for ourselves
  if (mOverflowFrames.NotEmpty()) {
    NS_ASSERTION(mFrames.NotEmpty(), "overflow list w/o frames");
    mFrames.AppendFrames(nsnull, mOverflowFrames);
    result = PR_TRUE;
  }
  return result;
}

/////////////////////////////////////////////////////////////////////////////
// Debugging

NS_IMETHODIMP
nsContainerFrame::List(FILE* out, PRInt32 aIndent) const
{
  IndentBy(out, aIndent);
  ListTag(out);
  nsIView* view;
  GetView(&view);
  if (nsnull != view) {
    fprintf(out, " [view=%p]", view);
  }
  if (nsnull != mNextSibling) {
    fprintf(out, " next=%p", mNextSibling);
  }
  if (nsnull != mPrevInFlow) {
    fprintf(out, " prev-in-flow=%p", mPrevInFlow);
  }
  if (nsnull != mNextInFlow) {
    fprintf(out, " next-in-flow=%p", mNextInFlow);
  }
  fprintf(out, " {%d,%d,%d,%d}", mRect.x, mRect.y, mRect.width, mRect.height);
  if (0 != mState) {
    fprintf(out, " [state=%08x]", mState);
  }
  fprintf(out, " sc=%p", mStyleContext);

  // Output the children
  nsIAtom* listName = nsnull;
  PRInt32 listIndex = 0;
  PRBool outputOneList = PR_FALSE;
  do {
    nsIFrame* kid;
    FirstChild(listName, &kid);
    if (nsnull != kid) {
      if (outputOneList) {
        IndentBy(out, aIndent);
      }
      outputOneList = PR_TRUE;
      nsAutoString tmp;
      if (nsnull != listName) {
        listName->ToString(tmp);
        fputs(tmp, out);
      }
      fputs("<\n", out);
      while (nsnull != kid) {
        kid->List(out, aIndent + 1);
        kid->GetNextSibling(&kid);
      }
      IndentBy(out, aIndent);
      fputs(">\n", out);
    }
    NS_IF_RELEASE(listName);
    GetAdditionalChildListName(listIndex++, &listName);
  } while(nsnull != listName);

  if (!outputOneList) {
    fputs("<>\n", out);
  }

  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
nsContainerFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = sizeof(*this);
  return NS_OK;
}
#endif
