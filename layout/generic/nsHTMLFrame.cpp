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
#include "nsIServiceManager.h"
#include "nsHTMLParts.h"
#include "nsHTMLContainerFrame.h"
#include "nsCSSRendering.h"
#include "nsIDocument.h"
#include "nsReflowPath.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsViewsCID.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"
#include "nsPageFrame.h"
#include "nsIRenderingContext.h"
#include "nsGUIEvent.h"
#include "nsIDOMEvent.h"
#include "nsStyleConsts.h"
#include "nsIViewManager.h"
#include "nsHTMLAtoms.h"
#include "nsIEventStateManager.h"
#include "nsIDeviceContext.h"
#include "nsIScrollableView.h"
#include "nsLayoutAtoms.h"
#include "nsIPresShell.h"
#include "nsIScrollPositionListener.h"

// for focus
#include "nsIDOMWindowInternal.h"
#include "nsIScriptGlobalObject.h"
#include "nsIFocusController.h"
#include "nsIScrollableFrame.h"
#include "nsIScrollableView.h"
#include "nsIDocShell.h"
#include "nsICanvasFrame.h"

#ifdef DEBUG_rods
//#define DEBUG_CANVAS_FOCUS
#endif

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
  CanvasFrame() : mDoPaintFocus(PR_FALSE) {}

   // nsISupports
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

  NS_IMETHOD Init(nsPresContext*  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsStyleContext*  aContext,
              nsIFrame*        aPrevInFlow);
  NS_IMETHOD Destroy(nsPresContext* aPresContext);

  NS_IMETHOD AppendFrames(nsPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aFrameList);
  NS_IMETHOD InsertFrames(nsPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList);
  NS_IMETHOD RemoveFrame(nsPresContext* aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);

  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
  NS_IMETHOD HandleEvent(nsPresContext* aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus*  aEventStatus);
  NS_IMETHOD GetFrameForPoint(nsPresContext* aPresContext,
                              const nsPoint& aPoint, 
                              nsFramePaintLayer aWhichLayer,
                              nsIFrame**     aFrame);
  virtual PRBool IsContainingBlock() const { return PR_TRUE; }

  NS_IMETHOD Paint(nsPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags);

  // nsIScrollPositionListener
  NS_IMETHOD ScrollPositionWillChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY);
	NS_IMETHOD ScrollPositionDidChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY);

  // nsICanvasFrame
  NS_IMETHOD SetHasFocus(PRBool aHasFocus) { mDoPaintFocus = aHasFocus; return NS_OK; }

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::canvasFrame
   */
  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif
  NS_IMETHOD GetContentForEvent(nsPresContext* aPresContext,
                                nsEvent* aEvent,
                                nsIContent** aContent);

protected:
  virtual PRIntn GetSkipSides() const;

  // Data members
  PRPackedBool             mDoPaintFocus;
  nsCOMPtr<nsIViewManager> mViewManager;
  nscoord                  mSavedChildWidth, mSavedChildHeight;

private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }
};


//----------------------------------------------------------------------

nsresult
NS_NewCanvasFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  CanvasFrame* it = new (aPresShell)CanvasFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

//--------------------------------------------------------------
// Frames are not refcounted, no need to AddRef
NS_IMETHODIMP
CanvasFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(NS_GET_IID(nsIScrollPositionListener))) {
    *aInstancePtr = (void*) ((nsIScrollPositionListener*) this);
    return NS_OK;
  } 
  
  if (aIID.Equals(NS_GET_IID(nsICanvasFrame))) {
    *aInstancePtr = (void*) ((nsICanvasFrame*) this);
    return NS_OK;
  } 
  

  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP
CanvasFrame::Init(nsPresContext*  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsStyleContext*  aContext,
              nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsHTMLContainerFrame::Init(aPresContext,aContent,aParent,aContext,aPrevInFlow);

  mViewManager = aPresContext->GetViewManager();

  nsIScrollableView* scrollingView = nsnull;
  mViewManager->GetRootScrollableView(&scrollingView);
  if (scrollingView) {
    scrollingView->AddScrollPositionListener(this);
  }

  return rv;
}

NS_IMETHODIMP
CanvasFrame::Destroy(nsPresContext* aPresContext)
{
  nsIScrollableView* scrollingView = nsnull;
  mViewManager->GetRootScrollableView(&scrollingView);
  if (scrollingView) {
    scrollingView->RemoveScrollPositionListener(this);
  }

  return nsHTMLContainerFrame::Destroy(aPresContext);
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
CanvasFrame::AppendFrames(nsPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aFrameList)
{
  nsresult  rv;

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

    // Generate a reflow command to reflow the newly inserted frame
    nsHTMLReflowCommand* reflowCmd;
    rv = NS_NewHTMLReflowCommand(&reflowCmd, this, eReflowType_ReflowDirty);
    if (NS_SUCCEEDED(rv)) {
      aPresShell.AppendReflowCommand(reflowCmd);
    }
  }

  return rv;
}

NS_IMETHODIMP
CanvasFrame::InsertFrames(nsPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList)
{
  nsresult  rv;

  // Because we only support a single child frame inserting is the same
  // as appending
  NS_PRECONDITION(!aPrevFrame, "unexpected previous sibling frame");
  if (aPrevFrame) {
    rv = NS_ERROR_UNEXPECTED;
  } else {
    rv = AppendFrames(aPresContext, aPresShell, aListName, aFrameList);
  }

  return rv;
}

NS_IMETHODIMP
CanvasFrame::RemoveFrame(nsPresContext* aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame)
{
  nsresult  rv;

  NS_ASSERTION(!aListName, "unexpected child list name");
  if (aListName) {
    // We only support the unnamed principal child list
    rv = NS_ERROR_INVALID_ARG;
  
  } else if (aOldFrame == mFrames.FirstChild()) {
    // It's our one and only child frame
    // Damage the area occupied by the deleted frame
    // The child of the canvas probably can't have an outline, but why bother
    // thinking about that?
    Invalidate(aOldFrame->GetOverflowRect() + aOldFrame->GetPosition(), PR_FALSE);

    // Remove the frame and destroy it
    mFrames.DestroyFrame(aPresContext, aOldFrame);

    // Generate a reflow command so we get reflowed
    nsHTMLReflowCommand* reflowCmd;
    rv = NS_NewHTMLReflowCommand(&reflowCmd, this, eReflowType_ReflowDirty);
    if (NS_SUCCEEDED(rv)) {
      aPresShell.AppendReflowCommand(reflowCmd);
    }

  } else {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}

NS_IMETHODIMP
CanvasFrame::Paint(nsPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags)
{
  // We are wrapping the root frame of a document. We
  // need to check the pres shell to find out if painting is locked
  // down (because we're still in the early stages of document
  // and frame construction.  If painting is locked down, then we
  // do not paint our children.  
  PRBool paintingSuppressed = PR_FALSE;
  aPresContext->PresShell()->IsPaintingSuppressed(&paintingSuppressed);
  if (paintingSuppressed) {
    if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
      PaintSelf(aPresContext, aRenderingContext, aDirtyRect);
    }
    return NS_OK;
  }

  nsresult rv = nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);

  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {

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

    if (mDoPaintFocus) {
      nsRect focusRect = GetRect();
      /////////////////////
      // draw focus
      // XXX This is only temporary
      const nsStyleVisibility* vis = (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);
      // Only paint the focus if we're visible
      if (vis->IsVisible()) {
        nsIFrame * parentFrame = GetParent();
        nsIView* parentView = parentFrame->GetView();

        nsIScrollableView* scrollableView;
        if (NS_SUCCEEDED(CallQueryInterface(parentView, &scrollableView))) {
          nscoord width, height;
          scrollableView->GetContainerSize(&width, &height);
          const nsIView* clippedView;
          scrollableView->GetClipView(&clippedView);
          nsRect vcr = clippedView->GetBounds();
          focusRect.width = vcr.width;
          focusRect.height = vcr.height;
          nscoord x,y;
          scrollableView->GetScrollPosition(x, y);
          focusRect.x += x;
          focusRect.y += y;
        }

        nsStyleOutline outlineStyle(aPresContext);
        outlineStyle.SetOutlineStyle(NS_STYLE_BORDER_STYLE_DOTTED);
        outlineStyle.SetOutlineInvert();

        float p2t = aPresContext->PixelsToTwips();
        // XXX the CSS border for links is specified as 2px, but it
        // is only drawn as 1px.  Match this here.
        nscoord onePixel = NSIntPixelsToTwips(1, p2t);

        nsRect borderInside(focusRect.x + onePixel,
                            focusRect.y + onePixel,
                            focusRect.width - 2 * onePixel,
                            focusRect.height - 2 * onePixel);

        nsCSSRendering::DrawDashedSides(0, aRenderingContext, 
                                        focusRect, nsnull,
                                        nsnull, &outlineStyle,
                                        PR_TRUE, focusRect,
                                        borderInside, 0, 
                                        nsnull);
      }
    }
  }
  return rv;
}

NS_IMETHODIMP
CanvasFrame::Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("CanvasFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  NS_FRAME_TRACE_REFLOW_IN("CanvasFrame::Reflow");
  //NS_PRECONDITION(!aDesiredSize.mComputeMEW, "unexpected request");

  // Initialize OUT parameter
  aStatus = NS_FRAME_COMPLETE;

  PRBool  isStyleChange = PR_FALSE;
  PRBool  isDirtyChildReflow = PR_FALSE;

  // Check for an incremental reflow
  if (eReflowReason_Incremental == aReflowState.reason) {
    // See if we're the target frame
    nsHTMLReflowCommand *command = aReflowState.path->mReflowCommand;
    if (command) {
      // Get the reflow type
      nsReflowType reflowType;
      command->GetType(reflowType);

      switch (reflowType) {
      case eReflowType_ReflowDirty:
        isDirtyChildReflow = PR_TRUE;
        break;

      case eReflowType_StyleChanged:
        // Remember it's a style change so we can set the reflow reason below
        isStyleChange = PR_TRUE;
        break;

      default:
        NS_ASSERTION(PR_FALSE, "unexpected reflow command type");
      }
    }
    else {
#ifdef DEBUG
      nsReflowPath::iterator iter = aReflowState.path->FirstChild();
      NS_ASSERTION(*iter == mFrames.FirstChild(), "unexpected next reflow command frame");
#endif
    }
  }

  // Reflow our one and only child frame
  nsHTMLReflowMetrics kidDesiredSize(nsnull);
  if (mFrames.IsEmpty()) {
    // We have no child frame, so return an empty size
    aDesiredSize.width = aDesiredSize.height = 0;
    aDesiredSize.ascent = aDesiredSize.descent = 0;

  } else {
    nsIFrame* kidFrame = mFrames.FirstChild();

    // We must specify an unconstrained available height, because constrained
    // is only for when we're paginated...
    nsReflowReason reason;
    if (isDirtyChildReflow) {
      // Note: the only reason the frame would be dirty would be if it had
      // just been inserted or appended
      reason = eReflowReason_Initial;
    } else if (isStyleChange) {
      reason = eReflowReason_StyleChange;
    } else {
      reason = aReflowState.reason;
    }

    nsHTMLReflowState kidReflowState(aPresContext, aReflowState, kidFrame,
                                     nsSize(aReflowState.availableWidth,
                                            NS_UNCONSTRAINEDSIZE),
                                     reason);

    if (eReflowReason_Incremental == aReflowState.reason) {
      // Restore original kid desired dimensions in case it decides to
      // reuse them during incremental reflow.
      kidFrame->SetSize(nsSize(mSavedChildWidth, mSavedChildHeight));
    }

    // Reflow the frame
    ReflowChild(kidFrame, aPresContext, kidDesiredSize, kidReflowState,
                kidReflowState.mComputedMargin.left, kidReflowState.mComputedMargin.top,
                0, aStatus);

    mSavedChildWidth = kidDesiredSize.width;
    mSavedChildHeight = kidDesiredSize.height;

    // The document element's background should cover the entire canvas, so
    // take into account the combined area and any space taken up by
    // absolutely positioned elements
    nsMargin      border;

    // Complete the reflow and position and size the child frame
    FinishReflowChild(kidFrame, aPresContext, &kidReflowState, kidDesiredSize,
                      kidReflowState.mComputedMargin.left,
                      kidReflowState.mComputedMargin.top, 0);

    // If the child frame was just inserted, then we're responsible for making sure
    // it repaints
    if (isDirtyChildReflow) {
      Invalidate(kidFrame->GetOverflowRect() + kidFrame->GetPosition(), PR_FALSE);
    }

    // Return our desired size
    // First check the combined area
    if (NS_FRAME_OUTSIDE_CHILDREN & kidFrame->GetStateBits()) {
      // Size ourselves to the size of the overflow area
      // XXXbz this ignores overflow up and left
      aDesiredSize.width = kidReflowState.mComputedMargin.left +
        PR_MAX(kidDesiredSize.mOverflowArea.XMost(),
               kidDesiredSize.width + kidReflowState.mComputedMargin.right);
      aDesiredSize.height = kidReflowState.mComputedMargin.top +
        PR_MAX(kidDesiredSize.mOverflowArea.YMost(),
               kidDesiredSize.height + kidReflowState.mComputedMargin.bottom);
    } else {
      aDesiredSize.width = kidDesiredSize.width +
        kidReflowState.mComputedMargin.left +
        kidReflowState.mComputedMargin.right;
      aDesiredSize.height = kidDesiredSize.height +
        kidReflowState.mComputedMargin.top +
        kidReflowState.mComputedMargin.bottom;
    }

    aDesiredSize.ascent = aDesiredSize.height;
    aDesiredSize.descent = 0;
    // XXX Don't completely ignore NS_FRAME_OUTSIDE_CHILDREN for child frames
    // that stick out on the left or top edges...
  }

  NS_FRAME_TRACE_REFLOW_OUT("CanvasFrame::Reflow", aStatus);
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

PRIntn
CanvasFrame::GetSkipSides() const
{
  return 0;
}

NS_IMETHODIMP
CanvasFrame::HandleEvent(nsPresContext* aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  if (aEvent->message == NS_MOUSE_LEFT_BUTTON_UP ||
      aEvent->message == NS_MOUSE_MIDDLE_BUTTON_UP ||
      aEvent->message == NS_MOUSE_RIGHT_BUTTON_UP ||
      aEvent->message == NS_MOUSE_MOVE ) {
    nsIFrame *firstChild = GetFirstChild(nsnull);
    //canvas frame needs to pass mouse events to its area frame so that mouse movement
    //and selection code will work properly. this will still have the necessary effects
    //that would have happened if nsFrame::HandleEvent was called.
    if (firstChild)
      firstChild->HandleEvent(aPresContext, aEvent, aEventStatus);
    else
      nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
  }

  return NS_OK;
}

NS_IMETHODIMP
CanvasFrame::GetFrameForPoint(nsPresContext* aPresContext,
                              const nsPoint& aPoint, 
                              nsFramePaintLayer aWhichLayer,
                              nsIFrame**     aFrame)
{
  // this should act like a block, so we need to override
  return GetFrameForPointUsing(aPresContext, aPoint, nsnull, aWhichLayer, (aWhichLayer == NS_FRAME_PAINT_LAYER_BACKGROUND), aFrame);
}

nsIAtom*
CanvasFrame::GetType() const
{
  return nsLayoutAtoms::canvasFrame;
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
