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
#include "nsRect.h"
#include "nsPoint.h"
#include "nsGUIEvent.h"
#include "nsStyleConsts.h"
#include "nsIView.h"
#include "nsVoidArray.h"
#include "nsISizeOfHandler.h"

#ifdef NS_DEBUG
#undef NOISY
#else
#undef NOISY
#endif

nsContainerFrame::nsContainerFrame(nsIContent* aContent, nsIFrame* aParent)
  : nsSplittableFrame(aContent, aParent)
{
}

nsContainerFrame::~nsContainerFrame()
{
}

NS_IMETHODIMP
nsContainerFrame::SizeOf(nsISizeOfHandler* aHandler) const
{
  aHandler->Add(sizeof(*this));
  nsContainerFrame::SizeOfWithoutThis(aHandler);
  return NS_OK;
}

NS_IMETHODIMP
nsContainerFrame::Init(nsIPresContext& aPresContext, nsIFrame* aChildList)
{
  NS_PRECONDITION(nsnull == mFirstChild, "already initialized");

  mFirstChild = aChildList;
  mChildCount = LengthOf(mFirstChild);
  return NS_OK;
}

NS_IMETHODIMP
nsContainerFrame::DeleteFrame(nsIPresContext& aPresContext)
{
  // Delete our child frames before doing anything else. In particular
  // we do all of this before our base class releases it's hold on the
  // view.
  for (nsIFrame* child = mFirstChild; child; ) {
    mFirstChild = nsnull;  // XXX hack until HandleEvent is not called until after destruction
    nsIFrame* nextChild;
     
    child->GetNextSibling(nextChild);
    child->DeleteFrame(aPresContext);
    child = nextChild;
  }

  nsFrame::DeleteFrame(aPresContext);

  return NS_OK;
}

void
nsContainerFrame::SizeOfWithoutThis(nsISizeOfHandler* aHandler) const
{
  nsSplittableFrame::SizeOfWithoutThis(aHandler);
  for (nsIFrame* child = mFirstChild; child; ) {
    child->SizeOf(aHandler);
    child->GetNextSibling(child);
  }
}

void
nsContainerFrame::PrepareContinuingFrame(nsIPresContext&   aPresContext,
                                         nsIFrame*         aParent,
                                         nsIStyleContext*  aStyleContext,
                                         nsContainerFrame* aContFrame)
{
  // Append the continuing frame to the flow
  aContFrame->AppendToFlow(this);
  aContFrame->SetStyleContext(&aPresContext, aStyleContext);
}

NS_METHOD
nsContainerFrame::DidReflow(nsIPresContext& aPresContext,
                            nsDidReflowStatus aStatus)
{
  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
                     ("enter nsContainerFrame::DidReflow: status=%d",
                      aStatus));
  if (NS_FRAME_REFLOW_FINISHED == aStatus) {
    nsIFrame* kid;
    FirstChild(kid);
    while (nsnull != kid) {
      kid->DidReflow(aPresContext, aStatus);
      kid->GetNextSibling(kid);
    }
  }

  NS_FRAME_TRACE_OUT("nsContainerFrame::DidReflow");

  // Let nsFrame position and size our view (if we have one), and clear
  // the NS_FRAME_IN_REFLOW bit
  return nsFrame::DidReflow(aPresContext, aStatus);
}

/////////////////////////////////////////////////////////////////////////////
// Child frame enumeration

NS_METHOD nsContainerFrame::FirstChild(nsIFrame*& aFirstChild) const
{
  aFirstChild = mFirstChild;
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
  // Set clip rect so that children don't leak out of us
  const nsStyleDisplay* disp =
    (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);
  PRBool hidden = PR_FALSE;
  if (NS_STYLE_OVERFLOW_HIDDEN == disp->mOverflow) {
    aRenderingContext.PushState();
    aRenderingContext.SetClipRect(nsRect(0, 0, mRect.width, mRect.height),
                                  nsClipCombine_kIntersect);
    hidden = PR_TRUE;
  }

  // XXX reminder: use the coordinates in the dirty rect to figure out
  // which set of children are impacted and only do the intersection
  // work for them. In addition, stop when we no longer overlap.

  nsIFrame* kid = mFirstChild;
  while (nsnull != kid) {
    nsIView *pView;
     
    kid->GetView(pView);
    if (nsnull == pView) {
      nsRect kidRect;
      kid->GetRect(kidRect);
      nsRect damageArea;
#ifdef NS_DEBUG
      PRBool overlap = PR_FALSE;
      if (nsIFrame::GetShowFrameBorders() &&
          ((kidRect.width == 0) || (kidRect.height == 0))) {
        nscoord xmost = aDirtyRect.XMost();
        nscoord ymost = aDirtyRect.YMost();
        if ((aDirtyRect.x <= kidRect.x) && (kidRect.x < xmost) &&
            (aDirtyRect.y <= kidRect.y) && (kidRect.y < ymost)) {
          overlap = PR_TRUE;
        }
      }
      else {
        overlap = damageArea.IntersectRect(aDirtyRect, kidRect);
      }
#else
      PRBool overlap = damageArea.IntersectRect(aDirtyRect, kidRect);
#endif
      if (overlap) {
        // Translate damage area into kid's coordinate system
        nsRect kidDamageArea(damageArea.x - kidRect.x,
                             damageArea.y - kidRect.y,
                             damageArea.width, damageArea.height);
        aRenderingContext.PushState();
        aRenderingContext.Translate(kidRect.x, kidRect.y);
        kid->Paint(aPresContext, aRenderingContext, kidDamageArea);
#ifdef NS_DEBUG
        if (nsIFrame::GetShowFrameBorders() &&
            (0 != kidRect.width) && (0 != kidRect.height)) {
          nsIView* view;
          GetView(view);
          if (nsnull != view) {
            aRenderingContext.SetColor(NS_RGB(0,0,255));
          }
          else {
            aRenderingContext.SetColor(NS_RGB(255,0,0));
          }
          aRenderingContext.DrawRect(0, 0, kidRect.width, kidRect.height);
        }
#endif
        aRenderingContext.PopState();
      }
    }
    kid->GetNextSibling(kid);
  }

  if (hidden) {
    aRenderingContext.PopState();
  }
}

/////////////////////////////////////////////////////////////////////////////
// Events

NS_METHOD nsContainerFrame::HandleEvent(nsIPresContext& aPresContext, 
                                        nsGUIEvent*     aEvent,
                                        nsEventStatus&  aEventStatus)
{
  aEventStatus = nsEventStatus_eIgnore;

  nsIFrame* kid;
  FirstChild(kid);
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

NS_METHOD nsContainerFrame::GetCursorAndContentAt(nsIPresContext& aPresContext,
                                        const nsPoint&  aPoint,
                                        nsIFrame**      aFrame,
                                        nsIContent**    aContent,
                                        PRInt32&        aCursor)
{
  aCursor = NS_STYLE_CURSOR_INHERIT;
  *aContent = mContent;

  nsIFrame* kid;
  FirstChild(kid);
  nsPoint tmp;
  while (nsnull != kid) {
    nsRect kidRect;
    kid->GetRect(kidRect);
    if (kidRect.Contains(aPoint)) {
      tmp.MoveTo(aPoint.x - kidRect.x, aPoint.y - kidRect.y);
      kid->GetCursorAndContentAt(aPresContext, tmp, aFrame, aContent, aCursor);
      break;
    }
    kid->GetNextSibling(kid);
  }
  return NS_OK;
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
  nsIContent* childContent;
  PRBool      result;
   
  aChild->GetContent(childContent);
  result = childContent == mContent;
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
nsReflowStatus nsContainerFrame::ReflowChild(nsIFrame*            aKidFrame,
                                             nsIPresContext*      aPresContext,
                                             nsReflowMetrics&     aDesiredSize,
                                             const nsReflowState& aReflowState)
{
  nsReflowStatus status;
                                                  
  NS_PRECONDITION(aReflowState.frame == aKidFrame, "bad reflow state");
#ifdef NS_DEBUG
  nsFrameState  kidFrameState;

  aKidFrame->GetFrameState(kidFrameState);
  NS_ASSERTION(kidFrameState & NS_FRAME_IN_REFLOW, "kid frame is not in reflow");
#endif
  aKidFrame->Reflow(*aPresContext, aDesiredSize, aReflowState, status);

  if (NS_FRAME_IS_COMPLETE(status)) {
    nsIFrame* kidNextInFlow;
     
    aKidFrame->GetNextInFlow(kidNextInFlow);
    if (nsnull != kidNextInFlow) {
      // Remove all of the childs next-in-flows. Make sure that we ask
      // the right parent to do the removal (it's possible that the
      // parent is not this because we are executing pullup code)
      nsIFrame* parent;
      aKidFrame->GetGeometricParent(parent);
      ((nsContainerFrame*)parent)->DeleteChildsNextInFlow(*aPresContext, aKidFrame);
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
PRBool
nsContainerFrame::DeleteChildsNextInFlow(nsIPresContext& aPresContext, nsIFrame* aChild)
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
    parent->DeleteChildsNextInFlow(aPresContext, nextInFlow);
  }

#ifdef NS_DEBUG
  PRInt32   childCount;
  nsIFrame* firstChild;

  nextInFlow->FirstChild(firstChild);
  childCount = LengthOf(firstChild);

  if ((0 != childCount) || (nsnull != firstChild)) {
    nsIFrame* top = nextInFlow;
    for (;;) {
      nsIFrame* parent;
      top->GetGeometricParent(parent);
      if (nsnull == parent) {
        break;
      }
      top = parent;
    }
    top->List();
  }
  NS_ASSERTION((0 == childCount) && (nsnull == firstChild),
               "deleting !empty next-in-flow");
#endif

  // Disconnect the next-in-flow from the flow list
  nextInFlow->BreakFromPrevFlow();

  // Take the next-in-flow out of the parent's child list
  if (parent->mFirstChild == nextInFlow) {
    nextInFlow->GetNextSibling(parent->mFirstChild);

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

  // Delete the next-in-flow frame and adjust its parents child count
  WillDeleteNextInFlowFrame(nextInFlow);
  nextInFlow->DeleteFrame(aPresContext);
  parent->mChildCount--;

#ifdef NS_DEBUG
  aChild->GetNextInFlow(nextInFlow);
  NS_POSTCONDITION(nsnull == nextInFlow, "non null next-in-flow");
#endif
  return PR_TRUE;
}

void nsContainerFrame::WillDeleteNextInFlowFrame(nsIFrame* aNextInFlow)
{
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
    nextInFlow->mChildCount += numChildren;

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
    nsIFrame* lastChild = LastFrame(mFirstChild);
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
}

/////////////////////////////////////////////////////////////////////////////
// Debugging

NS_METHOD nsContainerFrame::List(FILE* out, PRInt32 aIndent, nsIListFilter *aFilter) const
{
  // if a filter is present, only output this frame if the filter says we should
  nsIAtom* tag;
  nsAutoString tagString;
  mContent->GetTag(tag);
  if (tag != nsnull) 
    tag->ToString(tagString);
  PRBool outputMe = (PRBool)((nsnull==aFilter) || ((PR_TRUE==aFilter->OutputTag(&tagString)) && (!IsPseudoFrame())));
  if (PR_TRUE==outputMe)
  {
    // Indent
    for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);

    // Output the tag
    ListTag(out);
    nsIView* view;
    GetView(view);
    if (nsnull != view) {
      fprintf(out, " [view=%p]", view);
    }

    if (nsnull != mPrevInFlow) {
      fprintf(out, "prev-in-flow=%p ", mPrevInFlow);
    }
    if (nsnull != mNextInFlow) {
      fprintf(out, "next-in-flow=%p ", mNextInFlow);
    }

    // Output the rect
    out << mRect;
  }

  // Output the children
  if (nsnull != mFirstChild) {
    if (PR_TRUE==outputMe)
    {
      if (0 != mState) {
        fprintf(out, " [state=%08x]", mState);
      }
      fputs("<\n", out);
    }
    for (nsIFrame* child = mFirstChild; child; child->GetNextSibling(child)) {
      child->List(out, aIndent + 1, aFilter);
    }
    if (PR_TRUE==outputMe)
    {
      for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);
      fputs(">\n", out);
    }
  } else {
    if (PR_TRUE==outputMe)
    {
      if (0 != mState) {
        fprintf(out, " [state=%08x]", mState);
      }
      fputs("<>\n", out);
    }
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
  NS_ASSERTION(0 == (mState & NS_FRAME_IN_REFLOW), "frame is in reflow");

  // Check our child count
  PRInt32 len = LengthOf(mFirstChild);
  VERIFY_ASSERT(len == mChildCount, "bad child count");

  nsIFrame* lastChild = LastFrame(mFirstChild);
  if (len != 0) {
    VERIFY_ASSERT(nsnull != lastChild, "bad last child");
  }
  VERIFY_ASSERT(nsnull == mOverflowList, "bad overflow list");

#endif
  return NS_OK;
}

PRInt32 nsContainerFrame::LengthOf(nsIFrame* aFrame)
{
  PRInt32 result = 0;

  while (nsnull != aFrame) {
    result++;
    aFrame->GetNextSibling(aFrame);
  }

  return result;
}

nsIFrame* nsContainerFrame::LastFrame(nsIFrame* aFrame)
{
  nsIFrame* lastChild = nsnull;

  while (nsnull != aFrame) {
    lastChild = aFrame;
    aFrame->GetNextSibling(aFrame);
  }

  return lastChild;
}

nsIFrame* nsContainerFrame::FrameAt(nsIFrame* aFrame, PRInt32 aIndex)
{
  while ((aIndex-- > 0) && (aFrame != nsnull)) {
    aFrame->GetNextSibling(aFrame);
  }
  return aFrame;
}

/////////////////////////////////////////////////////////////////////////////

#ifdef NS_DEBUG

PRBool nsContainerFrame::IsChild(const nsIFrame* aChild) const
{
  // Check the geometric parent
  nsIFrame* parent;

  aChild->GetGeometricParent(parent);
  if (parent != (nsIFrame*)this) {
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
  nsIFrame* lastChild = LastFrame(mFirstChild);
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

/**
 * A container is empty if it has no children, or it has exactly one
 * child and that child is a pseudo-frame and it's empty (recursively
 * applied if that child contains one child which is a
 * pseudo-frame...)
 */
PRBool nsContainerFrame::IsEmpty()
{
  if (nsnull == mFirstChild) {
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

#endif
