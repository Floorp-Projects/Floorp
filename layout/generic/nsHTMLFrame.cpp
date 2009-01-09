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
 * The Original Code is mozilla.org code.
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

/* rendering object that goes directly inside the document's scrollbars */

#include "nsIServiceManager.h"
#include "nsHTMLParts.h"
#include "nsHTMLContainerFrame.h"
#include "nsCSSRendering.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIRenderingContext.h"
#include "nsGUIEvent.h"
#include "nsStyleConsts.h"
#include "nsGkAtoms.h"
#include "nsIEventStateManager.h"
#include "nsIDeviceContext.h"
#include "nsIPresShell.h"
#include "nsIScrollPositionListener.h"
#include "nsDisplayList.h"
#include "nsAbsoluteContainingBlock.h"

// for focus
#include "nsIDOMWindowInternal.h"
#include "nsIFocusController.h"
#include "nsIScrollableFrame.h"
#include "nsIScrollableView.h"
#include "nsIDocShell.h"
#include "nsICanvasFrame.h"

#ifdef DEBUG_rods
//#define DEBUG_CANVAS_FOCUS
#endif

#define CANVAS_ABS_POS_CHILD_LIST NS_CONTAINER_LIST_COUNT_INCL_OC

// Interface IDs

/**
 * Root frame class.
 *
 * The root frame is the parent frame for the document element's frame.
 * It only supports having a single child frame which must be an area
 * frame
 */
class CanvasFrame : public nsHTMLContainerFrame, 
                    public nsIScrollPositionListener, 
                    public nsICanvasFrame {
public:
  CanvasFrame(nsStyleContext* aContext)
  : nsHTMLContainerFrame(aContext), mDoPaintFocus(PR_FALSE),
    mAbsoluteContainer(nsGkAtoms::absoluteList) {}

   // nsISupports
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);
  virtual void Destroy();

  NS_IMETHOD SetInitialChildList(nsIAtom*        aListName,
                                 nsIFrame*       aChildList);
  NS_IMETHOD AppendFrames(nsIAtom*        aListName,
                          nsIFrame*       aFrameList);
  NS_IMETHOD InsertFrames(nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList);
  NS_IMETHOD RemoveFrame(nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);

  virtual nsIAtom* GetAdditionalChildListName(PRInt32 aIndex) const;
  virtual nsIFrame* GetFirstChild(nsIAtom* aListName) const;

  virtual nscoord GetMinWidth(nsIRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsIRenderingContext *aRenderingContext);
  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
  virtual PRBool IsContainingBlock() const { return PR_TRUE; }
  virtual PRBool IsFrameOfType(PRUint32 aFlags) const
  {
    return nsHTMLContainerFrame::IsFrameOfType(aFlags &
             ~(nsIFrame::eCanContainOverflowContainers));
  }

  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  void PaintFocus(nsIRenderingContext& aRenderingContext, nsPoint aPt);

  // nsIScrollPositionListener
  NS_IMETHOD ScrollPositionWillChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY);
  virtual void ViewPositionDidChange(nsIScrollableView* aScrollable) {}
  NS_IMETHOD ScrollPositionDidChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY);

  // nsICanvasFrame
  NS_IMETHOD SetHasFocus(PRBool aHasFocus);

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::canvasFrame
   */
  virtual nsIAtom* GetType() const;

  virtual nsresult StealFrame(nsPresContext* aPresContext,
                              nsIFrame*      aChild,
                              PRBool         aForceNormal)
  {
    NS_ASSERTION(!aForceNormal, "No-one should be passing this in here");

    // CanvasFrame keeps overflow container continuations of its child
    // frame in main child list
    nsresult rv = nsContainerFrame::StealFrame(aPresContext, aChild, PR_TRUE);
    if (NS_FAILED(rv)) {
      rv = nsContainerFrame::StealFrame(aPresContext, aChild);
    }
    return rv;
  }

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif
  NS_IMETHOD GetContentForEvent(nsPresContext* aPresContext,
                                nsEvent* aEvent,
                                nsIContent** aContent);

  nsRect CanvasArea() const;

protected:
  virtual PRIntn GetSkipSides() const;

  // Data members
  PRPackedBool              mDoPaintFocus;
  nsCOMPtr<nsIViewManager>  mViewManager;
  nsAbsoluteContainingBlock mAbsoluteContainer;

private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }
};


//----------------------------------------------------------------------

nsIFrame*
NS_NewCanvasFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell)CanvasFrame(aContext);
}

//--------------------------------------------------------------
// Frames are not refcounted, no need to AddRef
NS_IMETHODIMP
CanvasFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(aInstancePtr, "null out param");

  if (aIID.Equals(NS_GET_IID(nsIScrollPositionListener))) {
    *aInstancePtr = static_cast<nsIScrollPositionListener*>(this);
    return NS_OK;
  } 
  if (aIID.Equals(NS_GET_IID(nsICanvasFrame))) {
    *aInstancePtr = static_cast<nsICanvasFrame*>(this);
    return NS_OK;
  } 

  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP
CanvasFrame::Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsHTMLContainerFrame::Init(aContent, aParent, aPrevInFlow);

  mViewManager = PresContext()->GetViewManager();

  nsIScrollableView* scrollingView = nsnull;
  mViewManager->GetRootScrollableView(&scrollingView);
  if (scrollingView) {
    scrollingView->AddScrollPositionListener(this);
  }

  return rv;
}

void
CanvasFrame::Destroy()
{
  mAbsoluteContainer.DestroyFrames(this);

  nsIScrollableView* scrollingView = nsnull;
  mViewManager->GetRootScrollableView(&scrollingView);
  if (scrollingView) {
    scrollingView->RemoveScrollPositionListener(this);
  }

  nsHTMLContainerFrame::Destroy();
}

NS_IMETHODIMP
CanvasFrame::ScrollPositionWillChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY)
{
#ifdef DEBUG_CANVAS_FOCUS
  {
    PRBool hasFocus = PR_FALSE;
    nsCOMPtr<nsIViewObserver> observer;
    mViewManager->GetViewObserver(*getter_AddRefs(observer));
    nsCOMPtr<nsIPresShell> shell = do_QueryInterface(observer);
    nsCOMPtr<nsPresContext> context;
    shell->GetPresContext(getter_AddRefs(context));
    nsCOMPtr<nsISupports> container;
    context->GetContainer(getter_AddRefs(container));
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container));
    if (docShell) {
      docShell->GetHasFocus(&hasFocus);
    }
    printf("SPWC: %p  HF: %s  mDoPaintFocus: %s\n", docShell.get(), hasFocus?"Y":"N", mDoPaintFocus?"Y":"N");
  }
#endif

  if (mDoPaintFocus) {
    mDoPaintFocus = PR_FALSE;
    mViewManager->UpdateAllViews(NS_VMREFRESH_NO_SYNC);
  }
  return NS_OK;
}

NS_IMETHODIMP
CanvasFrame::ScrollPositionDidChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY)
{
  return NS_OK;
}

NS_IMETHODIMP
CanvasFrame::SetHasFocus(PRBool aHasFocus)
{
  if (mDoPaintFocus != aHasFocus) {
    mDoPaintFocus = aHasFocus;
    mViewManager->UpdateAllViews(NS_VMREFRESH_NO_SYNC);
  }
  return NS_OK;
}

NS_IMETHODIMP
CanvasFrame::SetInitialChildList(nsIAtom*        aListName,
                                 nsIFrame*       aChildList)
{
  if (nsGkAtoms::absoluteList == aListName)
    return mAbsoluteContainer.SetInitialChildList(this, aListName, aChildList);

  NS_ASSERTION(aListName || !aChildList || !aChildList->GetNextSibling(),
               "Primary child list can have at most one frame in it");
  return nsHTMLContainerFrame::SetInitialChildList(aListName, aChildList);
}

NS_IMETHODIMP
CanvasFrame::AppendFrames(nsIAtom*        aListName,
                          nsIFrame*       aFrameList)
{
  nsresult  rv;

  if (nsGkAtoms::absoluteList == aListName)
    return mAbsoluteContainer.AppendFrames(this, aListName, aFrameList);

  NS_ASSERTION(!aListName, "unexpected child list name");
  NS_PRECONDITION(mFrames.IsEmpty(), "already have a child frame");
  if (aListName) {
    // We only support unnamed principal child list
    rv = NS_ERROR_INVALID_ARG;

  } else if (!mFrames.IsEmpty()) {
    // We only allow a single child frame
    rv = NS_ERROR_FAILURE;

  } else {
    // Insert the new frames
#ifdef NS_DEBUG
    nsFrame::VerifyDirtyBitSet(aFrameList);
#endif
    mFrames.AppendFrame(nsnull, aFrameList);

    rv = PresContext()->PresShell()->
           FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                            NS_FRAME_HAS_DIRTY_CHILDREN);
  }

  return rv;
}

NS_IMETHODIMP
CanvasFrame::InsertFrames(nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList)
{
  nsresult  rv;

  if (nsGkAtoms::absoluteList == aListName)
    return mAbsoluteContainer.InsertFrames(this, aListName, aPrevFrame, aFrameList);

  // Because we only support a single child frame inserting is the same
  // as appending
  NS_PRECONDITION(!aPrevFrame, "unexpected previous sibling frame");
  if (aPrevFrame) {
    rv = NS_ERROR_UNEXPECTED;
  } else {
    rv = AppendFrames(aListName, aFrameList);
  }

  return rv;
}

NS_IMETHODIMP
CanvasFrame::RemoveFrame(nsIAtom*        aListName,
                         nsIFrame*       aOldFrame)
{
  nsresult  rv;

  if (nsGkAtoms::absoluteList == aListName)
    return mAbsoluteContainer.RemoveFrame(this, aListName, aOldFrame);

  NS_ASSERTION(!aListName, "unexpected child list name");
  if (aListName) {
    // We only support the unnamed principal child list
    rv = NS_ERROR_INVALID_ARG;
  
  } else if (aOldFrame == mFrames.FirstChild()) {
    // It's our one and only child frame
    // Damage the area occupied by the deleted frame
    // The child of the canvas probably can't have an outline, but why bother
    // thinking about that?
    Invalidate(aOldFrame->GetOverflowRect() + aOldFrame->GetPosition());

    // Remove the frame and destroy it
    mFrames.DestroyFrame(aOldFrame);

    rv = PresContext()->PresShell()->
           FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                            NS_FRAME_HAS_DIRTY_CHILDREN);
  } else {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}

nsIAtom*
CanvasFrame::GetAdditionalChildListName(PRInt32 aIndex) const
{
  if (CANVAS_ABS_POS_CHILD_LIST == aIndex)
    return nsGkAtoms::absoluteList;

  return nsHTMLContainerFrame::GetAdditionalChildListName(aIndex);
}

nsIFrame*
CanvasFrame::GetFirstChild(nsIAtom* aListName) const
{
  if (nsGkAtoms::absoluteList == aListName)
    return mAbsoluteContainer.GetFirstChild();

  return nsHTMLContainerFrame::GetFirstChild(aListName);
}

nsRect CanvasFrame::CanvasArea() const
{
  nsRect result(GetOverflowRect());

  nsIScrollableFrame *scrollableFrame;
  CallQueryInterface(GetParent(), &scrollableFrame);
  if (scrollableFrame) {
    nsIScrollableView* scrollableView = scrollableFrame->GetScrollableView();
    nsRect vcr = scrollableView->View()->GetBounds();
    result.UnionRect(result, nsRect(nsPoint(0, 0), vcr.Size()));
  }
  return result;
}

/*
 * Override nsDisplayBackground methods so that we pass aBGClipRect to
 * PaintBackground, covering the whole overflow area.
 */
class nsDisplayCanvasBackground : public nsDisplayBackground {
public:
  nsDisplayCanvasBackground(nsIFrame *aFrame)
    : nsDisplayBackground(aFrame)
  {
  }

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder)
  {
    CanvasFrame* frame = static_cast<CanvasFrame*>(mFrame);
    return frame->CanvasArea() + aBuilder->ToReferenceFrame(mFrame);
  }

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsIRenderingContext* aCtx, const nsRect& aDirtyRect)
  {
    CanvasFrame* frame = static_cast<CanvasFrame*>(mFrame);
    nsPoint offset = aBuilder->ToReferenceFrame(mFrame);
    nsRect bgClipRect = frame->CanvasArea() + offset;
    // XXXzw This is the only use of the bgClipRect argument.  Does this
    // path need the propagation-of-root-background-to-viewport logic?
    nsCSSRendering::PaintBackground(mFrame->PresContext(), *aCtx, mFrame,
                                    aDirtyRect,
                                    nsRect(offset, mFrame->GetSize()),
                                    mFrame->HonorPrintBackgroundSettings(),
                                    &bgClipRect);
  }

  NS_DISPLAY_DECL_NAME("CanvasBackground")
};

/**
 * A display item to paint the focus ring for the document.
 *
 * The only reason this can't use nsDisplayGeneric is overriding GetBounds.
 */
class nsDisplayCanvasFocus : public nsDisplayItem {
public:
  nsDisplayCanvasFocus(CanvasFrame *aFrame)
    : nsDisplayItem(aFrame)
  {
  }

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder)
  {
    // This is an overestimate, but that's not a problem.
    CanvasFrame* frame = static_cast<CanvasFrame*>(mFrame);
    return frame->CanvasArea() + aBuilder->ToReferenceFrame(mFrame);
  }

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsIRenderingContext* aCtx, const nsRect& aDirtyRect)
  {
    CanvasFrame* frame = static_cast<CanvasFrame*>(mFrame);
    frame->PaintFocus(*aCtx, aBuilder->ToReferenceFrame(mFrame));
  }

  NS_DISPLAY_DECL_NAME("CanvasFocus")
};

NS_IMETHODIMP
CanvasFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists)
{
  nsresult rv;

  if (GetPrevInFlow()) {
    DisplayOverflowContainers(aBuilder, aDirtyRect, aLists);
  }

  aBuilder->MarkFramesForDisplayList(this, mAbsoluteContainer.GetFirstChild(), aDirtyRect);
  
  // Force a background to be shown. We may have a background propagated to us,
  // in which case GetStyleBackground wouldn't have the right background
  // and the code in nsFrame::DisplayBorderBackgroundOutline might not give us
  // a background.
  // We don't have any border or outline, and our background draws over
  // the overflow area, so just add nsDisplayCanvasBackground instead of
  // calling DisplayBorderBackgroundOutline.
  if (IsVisibleForPainting(aBuilder)) { 
    rv = aLists.BorderBackground()->AppendNewToTop(new (aBuilder)
           nsDisplayCanvasBackground(this));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsIFrame* kid;
  for (kid = GetFirstChild(nsnull); kid; kid = kid->GetNextSibling()) {
    // Put our child into its own pseudo-stack.
    rv = BuildDisplayListForChild(aBuilder, kid, aDirtyRect, aLists,
                                  DISPLAY_CHILD_FORCE_PSEUDO_STACKING_CONTEXT);
    NS_ENSURE_SUCCESS(rv, rv);
  }

#ifdef DEBUG_CANVAS_FOCUS
  nsCOMPtr<nsIContent> focusContent;
  aPresContext->EventStateManager()->
    GetFocusedContent(getter_AddRefs(focusContent));

  PRBool hasFocus = PR_FALSE;
  nsCOMPtr<nsISupports> container;
  aPresContext->GetContainer(getter_AddRefs(container));
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container));
  if (docShell) {
    docShell->GetHasFocus(&hasFocus);
    printf("%p - CanvasFrame::Paint R:%d,%d,%d,%d  DR: %d,%d,%d,%d\n", this, 
            mRect.x, mRect.y, mRect.width, mRect.height,
            aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
  }
  printf("%p - Focus: %s   c: %p  DoPaint:%s\n", docShell.get(), hasFocus?"Y":"N", 
         focusContent.get(), mDoPaintFocus?"Y":"N");
#endif

  if (!mDoPaintFocus)
    return NS_OK;
  // Only paint the focus if we're visible
  if (!GetStyleVisibility()->IsVisible())
    return NS_OK;
  
  return aLists.Outlines()->AppendNewToTop(new (aBuilder)
      nsDisplayCanvasFocus(this));
}

void
CanvasFrame::PaintFocus(nsIRenderingContext& aRenderingContext, nsPoint aPt)
{
  nsRect focusRect(aPt, GetSize());

  nsIScrollableFrame *scrollableFrame;
  CallQueryInterface(GetParent(), &scrollableFrame);

  if (scrollableFrame) {
    nsIScrollableView* scrollableView = scrollableFrame->GetScrollableView();
    nsRect vcr = scrollableView->View()->GetBounds();
    focusRect.width = vcr.width;
    focusRect.height = vcr.height;
    nscoord x,y;
    scrollableView->GetScrollPosition(x, y);
    focusRect.x += x;
    focusRect.y += y;
  }

 // XXX use the root frame foreground color, but should we find BODY frame
 // for HTML documents?
  nsIFrame* root = mFrames.FirstChild();
  const nsStyleColor* color =
    root ? root->GetStyleContext()->GetStyleColor() :
           mStyleContext->GetStyleColor();
  if (!color) {
    NS_ERROR("current color cannot be found");
    return;
  }

  nsCSSRendering::PaintFocus(PresContext(), aRenderingContext,
                             focusRect, color->mColor);
}

/* virtual */ nscoord
CanvasFrame::GetMinWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);
  if (mFrames.IsEmpty())
    result = 0;
  else
    result = mFrames.FirstChild()->GetMinWidth(aRenderingContext);
  return result;
}

/* virtual */ nscoord
CanvasFrame::GetPrefWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);
  if (mFrames.IsEmpty())
    result = 0;
  else
    result = mFrames.FirstChild()->GetPrefWidth(aRenderingContext);
  return result;
}

NS_IMETHODIMP
CanvasFrame::Reflow(nsPresContext*           aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("CanvasFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  NS_FRAME_TRACE_REFLOW_IN("CanvasFrame::Reflow");

  // Initialize OUT parameter
  aStatus = NS_FRAME_COMPLETE;

  CanvasFrame* prevCanvasFrame = static_cast<CanvasFrame*>
                                               (GetPrevInFlow());
  if (prevCanvasFrame) {
    nsIFrame* overflow = prevCanvasFrame->GetOverflowFrames(aPresContext, PR_TRUE);
    if (overflow) {
      NS_ASSERTION(!overflow->GetNextSibling(),
                   "must have doc root as canvas frame's only child");
      nsHTMLContainerFrame::ReparentFrameView(aPresContext, overflow, prevCanvasFrame, this);
      // Prepend overflow to the our child list. There may already be
      // children placeholders for fixed-pos elements, which don't get
      // reflowed but must not be lost until the canvas frame is destroyed.
      mFrames.InsertFrames(this, nsnull, overflow);
    }
  }

  // Reflow our one and only normal child frame. It's either the root
  // element's frame or a placeholder for that frame, if the root element
  // is abs-pos or fixed-pos. We may have additional children which
  // are placeholders for continuations of fixed-pos content, but those
  // don't need to be reflowed. The normal child is always comes before
  // the fixed-pos placeholders, because we insert it at the start
  // of the child list, above.
  nsHTMLReflowMetrics kidDesiredSize;
  if (mFrames.IsEmpty()) {
    // We have no child frame, so return an empty size
    aDesiredSize.width = aDesiredSize.height = 0;
  } else {
    nsIFrame* kidFrame = mFrames.FirstChild();
    PRBool kidDirty = (kidFrame->GetStateBits() & NS_FRAME_IS_DIRTY) != 0;

    nsHTMLReflowState kidReflowState(aPresContext, aReflowState, kidFrame,
                                     nsSize(aReflowState.availableWidth,
                                            aReflowState.availableHeight));

    if (aReflowState.mFlags.mVResize &&
        (kidFrame->GetStateBits() & NS_FRAME_CONTAINS_RELATIVE_HEIGHT)) {
      // Tell our kid it's being vertically resized too.  Bit of a
      // hack for framesets.
      kidReflowState.mFlags.mVResize = PR_TRUE;
    }

    nsPoint kidPt(kidReflowState.mComputedMargin.left,
                  kidReflowState.mComputedMargin.top);
    // Apply CSS relative positioning
    const nsStyleDisplay* styleDisp = kidFrame->GetStyleDisplay();
    if (NS_STYLE_POSITION_RELATIVE == styleDisp->mPosition) {
      kidPt += nsPoint(kidReflowState.mComputedOffsets.left,
                       kidReflowState.mComputedOffsets.top);
    }

    // Reflow the frame
    ReflowChild(kidFrame, aPresContext, kidDesiredSize, kidReflowState,
                kidPt.x, kidPt.y, 0, aStatus);

    // Complete the reflow and position and size the child frame
    FinishReflowChild(kidFrame, aPresContext, &kidReflowState, kidDesiredSize,
                      kidPt.x, kidPt.y, 0);

    if (!NS_FRAME_IS_FULLY_COMPLETE(aStatus)) {
      nsIFrame* nextFrame = kidFrame->GetNextInFlow();
      NS_ASSERTION(nextFrame || aStatus & NS_FRAME_REFLOW_NEXTINFLOW,
        "If it's incomplete and has no nif yet, it must flag a nif reflow.");
      if (!nextFrame) {
        nsresult rv = nsHTMLContainerFrame::CreateNextInFlow(aPresContext,
                                              this, kidFrame, nextFrame);
        NS_ENSURE_SUCCESS(rv, rv);
        kidFrame->SetNextSibling(nextFrame->GetNextSibling());
        nextFrame->SetNextSibling(nsnull);
        SetOverflowFrames(aPresContext, nextFrame);
        // Root overflow containers will be normal children of
        // the canvas frame, but that's ok because there
        // aren't any other frames we need to isolate them from
        // during reflow.
      }
      if (NS_FRAME_OVERFLOW_IS_INCOMPLETE(aStatus)) {
        nextFrame->AddStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER);
      }
    }

    // If the child frame was just inserted, then we're responsible for making sure
    // it repaints
    if (kidDirty) {
      // But we have a new child, which will affect our background, so
      // invalidate our whole rect.
      // Note: Even though we request to be sized to our child's size, our
      // scroll frame ensures that we are always the size of the viewport.
      // Also note: GetPosition() on a CanvasFrame is always going to return
      // (0, 0). We only want to invalidate GetRect() since GetOverflowRect()
      // could also include overflow to our top and left (out of the viewport)
      // which doesn't need to be painted.
      nsIFrame* viewport = PresContext()->GetPresShell()->GetRootFrame();
      viewport->Invalidate(nsRect(nsPoint(0, 0), viewport->GetSize()));
    }
    
    // Return our desired size. Normally it's what we're told, but
    // sometimes we can be given an unconstrained height (when a window
    // is sizing-to-content), and we should compute our desired height.
    aDesiredSize.width = aReflowState.ComputedWidth();
    if (aReflowState.ComputedHeight() == NS_UNCONSTRAINEDSIZE) {
      aDesiredSize.height = kidFrame->GetRect().height +
        kidReflowState.mComputedMargin.TopBottom();
    } else {
      aDesiredSize.height = aReflowState.ComputedHeight();
    }

    aDesiredSize.mOverflowArea.UnionRect(
      nsRect(0, 0, aDesiredSize.width, aDesiredSize.height),
      kidDesiredSize.mOverflowArea + kidPt);

    if (mAbsoluteContainer.HasAbsoluteFrames()) {
      PRBool widthChanged = aDesiredSize.width != mRect.width;
      PRBool heightChanged = aDesiredSize.height != mRect.height;
      nsRect absPosBounds;
      mAbsoluteContainer.Reflow(this, aPresContext, aReflowState, aStatus,
                                aDesiredSize.width, aDesiredSize.height,
                                PR_TRUE, widthChanged, heightChanged,
                                &absPosBounds);
      aDesiredSize.mOverflowArea.UnionRect(aDesiredSize.mOverflowArea, absPosBounds);
    }
  }

  if (prevCanvasFrame) {
    ReflowOverflowContainerChildren(aPresContext, aReflowState,
                                    aDesiredSize.mOverflowArea, 0,
                                    aStatus);
  }

  FinishAndStoreOverflow(&aDesiredSize);

  NS_FRAME_TRACE_REFLOW_OUT("CanvasFrame::Reflow", aStatus);
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

PRIntn
CanvasFrame::GetSkipSides() const
{
  return 0;
}

nsIAtom*
CanvasFrame::GetType() const
{
  return nsGkAtoms::canvasFrame;
}

NS_IMETHODIMP 
CanvasFrame::GetContentForEvent(nsPresContext* aPresContext,
                                nsEvent* aEvent,
                                nsIContent** aContent)
{
  NS_ENSURE_ARG_POINTER(aContent);
  nsresult rv = nsFrame::GetContentForEvent(aPresContext,
                                            aEvent,
                                            aContent);
  if (NS_FAILED(rv) || !*aContent) {
    nsIFrame* kid = mFrames.FirstChild();
    if (kid) {
      rv = kid->GetContentForEvent(aPresContext,
                                   aEvent,
                                   aContent);
    }
  }

  return rv;
}

#ifdef DEBUG
NS_IMETHODIMP
CanvasFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Canvas"), aResult);
}
#endif
