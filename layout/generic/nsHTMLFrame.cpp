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
#include "nsIServiceManager.h"
#include "nsHTMLParts.h"
#include "nsHTMLContainerFrame.h"
#include "nsCSSRendering.h"
#include "nsIDocument.h"
#include "nsIReflowCommand.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsViewsCID.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIWidget.h"
#include "nsHTMLIIDs.h"
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
#include "nsPIDOMWindow.h"
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

  NS_IMETHOD Init(nsIPresContext*  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow);

  NS_IMETHOD AppendFrames(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aFrameList);
  NS_IMETHOD InsertFrames(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aPrevFrame,
                          nsIFrame*       aFrameList);
  NS_IMETHOD RemoveFrame(nsIPresContext* aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame);

  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
  NS_IMETHOD HandleEvent(nsIPresContext* aPresContext, 
                         nsGUIEvent*     aEvent,
                         nsEventStatus*  aEventStatus);
  NS_IMETHOD GetFrameForPoint(nsIPresContext* aPresContext,
                              const nsPoint& aPoint, 
                              nsFramePaintLayer aWhichLayer,
                              nsIFrame**     aFrame);
  NS_IMETHOD IsPercentageBase(PRBool& aBase) const {
    aBase = PR_TRUE;
    return NS_OK;
  }

  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
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
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent*     aChild,
                              PRInt32         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              PRInt32         aModType,
                              PRInt32         aHint);
  
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
#endif
  NS_IMETHOD GetContentForEvent(nsIPresContext* aPresContext,
                                nsEvent* aEvent,
                                nsIContent** aContent);

protected:
  virtual PRIntn GetSkipSides() const;
  void DrawDottedRect(nsIRenderingContext& aRenderingContext, nsRect& aRect);

  // Data members
  PRPackedBool             mDoPaintFocus;
  nsCOMPtr<nsIPresContext> mPresContext;


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
CanvasFrame::Init(nsIPresContext*  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsHTMLContainerFrame::Init(aPresContext,aContent,aParent,aContext,aPrevInFlow);

  mPresContext = aPresContext;

  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));
  nsCOMPtr<nsIViewManager> vm;
  presShell->GetViewManager(getter_AddRefs(vm));

  nsIScrollableView* scrollingView = nsnull;
  vm->GetRootScrollableView(&scrollingView);

  if (scrollingView) {
    scrollingView->AddScrollPositionListener((nsIScrollPositionListener *)this);
  }

  return rv;

}

NS_IMETHODIMP
CanvasFrame::ScrollPositionWillChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY)
{
#ifdef DEBUG_CANVAS_FOCUS
  {
    PRBool hasFocus = PR_FALSE;
    nsCOMPtr<nsISupports> container;
    mPresContext->GetContainer(getter_AddRefs(container));
    nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container));
    if (docShell) {
      docShell->GetHasFocus(&hasFocus);
    }
    printf("SPWC: %p  HF: %s  mDoPaintFocus: %s\n", docShell.get(), hasFocus?"Y":"N", mDoPaintFocus?"Y":"N");
  }
#endif

  if (mDoPaintFocus) {
    mDoPaintFocus = PR_FALSE;
    
    nsCOMPtr<nsIPresShell> presShell;
    mPresContext->GetShell(getter_AddRefs(presShell));
    if (presShell) {
      nsCOMPtr<nsIViewManager> vm;
      presShell->GetViewManager(getter_AddRefs(vm));
      if (vm) {
        vm->UpdateAllViews(NS_VMREFRESH_NO_SYNC);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
CanvasFrame::ScrollPositionDidChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY)
{
  return NS_OK;
}

NS_IMETHODIMP
CanvasFrame::AppendFrames(nsIPresContext* aPresContext,
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
    nsIReflowCommand* reflowCmd;
    rv = NS_NewHTMLReflowCommand(&reflowCmd, this, nsIReflowCommand::ReflowDirty);
    if (NS_SUCCEEDED(rv)) {
      aPresShell.AppendReflowCommand(reflowCmd);
      NS_RELEASE(reflowCmd);
    }
  }

  return rv;
}

NS_IMETHODIMP
CanvasFrame::InsertFrames(nsIPresContext* aPresContext,
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
CanvasFrame::RemoveFrame(nsIPresContext* aPresContext,
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
    nsRect  damageRect;
    aOldFrame->GetRect(damageRect);
    Invalidate(aPresContext, damageRect, PR_FALSE);

    // Remove the frame and destroy it
    mFrames.DestroyFrame(aPresContext, aOldFrame);

    // Generate a reflow command so we get reflowed
    nsIReflowCommand* reflowCmd;
    rv = NS_NewHTMLReflowCommand(&reflowCmd, this, nsIReflowCommand::ReflowDirty);
    if (NS_SUCCEEDED(rv)) {
      aPresShell.AppendReflowCommand(reflowCmd);
      NS_RELEASE(reflowCmd);
    }

  } else {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}

void
CanvasFrame::DrawDottedRect(nsIRenderingContext& aRenderingContext, nsRect& aRect)
{
#ifdef DEBUG_CANVAS_FOCUS
  printf("** DrawDottedRect: %d,%d,%d,%d\n",aRect.x, aRect.y, aRect.width, aRect.height);
#endif

  aRenderingContext.DrawLine(aRect.x, aRect.y, 
                             aRect.x+aRect.width, aRect.y);
  aRenderingContext.DrawLine(aRect.x+aRect.width, aRect.y, 
                             aRect.x+aRect.width, aRect.y+aRect.height);
  aRenderingContext.DrawLine(aRect.x+aRect.width, aRect.y+aRect.height, 
                             aRect.x, aRect.y+aRect.height);
  aRenderingContext.DrawLine(aRect.x, aRect.y+aRect.height, 
                             aRect.x, aRect.y);
  aRenderingContext.DrawLine(aRect.x, aRect.y+aRect.height, 
                             aRect.x, aRect.y);
}
 

NS_IMETHODIMP
CanvasFrame::Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags)
{
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    SetDefaultBackgroundColor(aPresContext);
  }
  nsresult rv = nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);

  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {

#ifdef DEBUG_CANVAS_FOCUS
    nsCOMPtr<nsIContent> focusContent;
    nsCOMPtr<nsIEventStateManager> esm;
    aPresContext->GetEventStateManager(getter_AddRefs(esm));
    if (esm) {
      esm->GetFocusedContent(getter_AddRefs(focusContent));
    }

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
      aRenderingContext.PushState();
      PRBool clipEmpty;
      nsRect focusRect;
      GetRect(focusRect);
      /////////////////////
      // draw focus
      // XXX This is only temporary
      const nsStyleVisibility* vis = (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);
      // Only paint the focus if we're visible
      if (vis->IsVisible()) {
        nsCOMPtr<nsIEventStateManager> stateManager;
        nsresult rv = aPresContext->GetEventStateManager(getter_AddRefs(stateManager));
        if (NS_SUCCEEDED(rv)) {
          nsIFrame * parentFrame;
          GetParent(&parentFrame);
          nsIScrollableFrame* scrollableFrame;
          if (NS_SUCCEEDED(parentFrame->QueryInterface(NS_GET_IID(nsIScrollableFrame), (void**)&scrollableFrame))) {
            nscoord width, height;
            scrollableFrame->GetClipSize(aPresContext, &width, &height);
          }
          nsIView* parentView;
          parentFrame->GetView(aPresContext, &parentView);

          nsIScrollableView* scrollableView;
          if (NS_SUCCEEDED(parentView->QueryInterface(NS_GET_IID(nsIScrollableView), (void**)&scrollableView))) {
            nscoord width, height;
            scrollableView->GetContainerSize(&width, &height);
            const nsIView* clippedView;
            scrollableView->GetClipView(&clippedView);
            nsRect vcr;
            clippedView->GetBounds(vcr);
            focusRect.height = vcr.height;
            nscoord x,y;
            scrollableView->GetScrollPosition(x, y);
            focusRect.x += x;
            focusRect.y += y;
          }
          aRenderingContext.SetLineStyle(nsLineStyle_kDotted);
          aRenderingContext.SetColor(NS_RGB(0,0,0));

          float p2t;
          aPresContext->GetPixelsToTwips(&p2t);
          nscoord onePixel = NSIntPixelsToTwips(1, p2t);

          focusRect.width  -= onePixel;
          focusRect.height -= onePixel;

          DrawDottedRect(aRenderingContext, focusRect);

          focusRect.Deflate(onePixel, onePixel);
          DrawDottedRect(aRenderingContext, focusRect);
        }
      }
      aRenderingContext.PopState(clipEmpty);
    }
  }
  return rv;
}

NS_IMETHODIMP
CanvasFrame::Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("CanvasFrame", aReflowState.reason);
  NS_FRAME_TRACE_REFLOW_IN("CanvasFrame::Reflow");
  NS_PRECONDITION(nsnull == aDesiredSize.maxElementSize, "unexpected request");

  // Initialize OUT parameter
  aStatus = NS_FRAME_COMPLETE;

  PRBool  isStyleChange = PR_FALSE;
  PRBool  isDirtyChildReflow = PR_FALSE;

  // Check for an incremental reflow
  if (eReflowReason_Incremental == aReflowState.reason) {
    // See if we're the target frame
    nsIFrame* targetFrame;
    aReflowState.reflowCommand->GetTarget(targetFrame);
    if (this == targetFrame) {
      // Get the reflow type
      nsIReflowCommand::ReflowType  reflowType;
      aReflowState.reflowCommand->GetType(reflowType);

      switch (reflowType) {
      case nsIReflowCommand::ReflowDirty:
        isDirtyChildReflow = PR_TRUE;
        break;

      case nsIReflowCommand::StyleChanged:
        // Remember it's a style change so we can set the reflow reason below
        isStyleChange = PR_TRUE;
        break;

      default:
        NS_ASSERTION(PR_FALSE, "unexpected reflow command type");
      }

    } else {
      nsIFrame* nextFrame;
      // Get the next frame in the reflow chain
      aReflowState.reflowCommand->GetNext(nextFrame);
      NS_ASSERTION(nextFrame == mFrames.FirstChild(), "unexpected next reflow command frame");
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
    nsHTMLReflowState kidReflowState(aPresContext, aReflowState, kidFrame,
                                     nsSize(aReflowState.availableWidth,
                                            NS_UNCONSTRAINEDSIZE));
    if (isDirtyChildReflow) {
      // Note: the only reason the frame would be dirty would be if it had
      // just been inserted or appended
      kidReflowState.reason = eReflowReason_Initial;
      kidReflowState.reflowCommand = nsnull;
    } else if (isStyleChange) {
      kidReflowState.reason = eReflowReason_StyleChange;
      kidReflowState.reflowCommand = nsnull;
    }

    // Reflow the frame
    ReflowChild(kidFrame, aPresContext, kidDesiredSize, kidReflowState,
                kidReflowState.mComputedMargin.left, kidReflowState.mComputedMargin.top,
                0, aStatus);

    // The document element's background should cover the entire canvas, so
    // take into account the combined area and any space taken up by
    // absolutely positioned elements
    nsMargin      border;
    nsFrameState  kidState;

    if (!kidReflowState.mStyleBorder->GetBorder(border)) {
      NS_NOTYETIMPLEMENTED("percentage border");
    }
    kidFrame->GetFrameState(&kidState);

    // First check the combined area
    if (NS_FRAME_OUTSIDE_CHILDREN & kidState) {
      // The background covers the content area and padding area, so check
      // for children sticking outside the child frame's padding edge
      nscoord paddingEdgeX = kidDesiredSize.width - border.right;
      nscoord paddingEdgeY = kidDesiredSize.height - border.bottom;

      if (kidDesiredSize.mOverflowArea.XMost() > paddingEdgeX) {
        kidDesiredSize.width = kidDesiredSize.mOverflowArea.XMost() +
                               border.right;
      }
      if (kidDesiredSize.mOverflowArea.YMost() > paddingEdgeY) {
        kidDesiredSize.height = kidDesiredSize.mOverflowArea.YMost() +
                                border.bottom;
      }
    }

    // If our height is fixed, then make sure the child frame plus its top and
    // bottom margin is at least that high as well...
    if (NS_AUTOHEIGHT != aReflowState.mComputedHeight) {
      nscoord totalHeight = kidDesiredSize.height + kidReflowState.mComputedMargin.top +
        kidReflowState.mComputedMargin.bottom;

      if (totalHeight < aReflowState.mComputedHeight) {
        kidDesiredSize.height += aReflowState.mComputedHeight - totalHeight;
      }
    }

    // Complete the reflow and position and size the child frame
    nsRect  rect(kidReflowState.mComputedMargin.left, kidReflowState.mComputedMargin.top,
                 kidDesiredSize.width, kidDesiredSize.height);
    FinishReflowChild(kidFrame, aPresContext, kidDesiredSize, rect.x, rect.y, 0);

    // If the child frame was just inserted, then we're responsible for making sure
    // it repaints
    if (isDirtyChildReflow) {
      // Damage the area occupied by the deleted frame
      Invalidate(aPresContext, rect, PR_FALSE);
    }

    // Return our desired size
    aDesiredSize.width = kidDesiredSize.width + kidReflowState.mComputedMargin.left +
      kidReflowState.mComputedMargin.right;
    aDesiredSize.height = kidDesiredSize.height + kidReflowState.mComputedMargin.top +
      kidReflowState.mComputedMargin.bottom;
    aDesiredSize.ascent = aDesiredSize.height;
    aDesiredSize.descent = 0;
    // XXX Don't completely ignore NS_FRAME_OUTSIDE_CHILDREN for child frames
    // that stick out on the left or top edges...
  }

  NS_FRAME_TRACE_REFLOW_OUT("CanvasFrame::Reflow", aStatus);
  return NS_OK;
}

PRIntn
CanvasFrame::GetSkipSides() const
{
  return 0;
}

NS_IMETHODIMP
CanvasFrame::HandleEvent(nsIPresContext* aPresContext, 
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
    nsIFrame *firstChild;
    nsresult rv = FirstChild(aPresContext,nsnull,&firstChild);
    //canvas frame needs to pass mouse events to its area frame so that mouse movement
    //and selection code will work properly. this will still have the necessary effects
    //that would have happened if nsFrame::HandleEvent was called.
    if (NS_SUCCEEDED(rv) && firstChild)
      firstChild->HandleEvent(aPresContext, aEvent, aEventStatus);
    else
      nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
  }

  return NS_OK;
}

NS_IMETHODIMP
CanvasFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                              const nsPoint& aPoint, 
                              nsFramePaintLayer aWhichLayer,
                              nsIFrame**     aFrame)
{
  // this should act like a block, so we need to override
  return GetFrameForPointUsing(aPresContext, aPoint, nsnull, aWhichLayer, (aWhichLayer == NS_FRAME_PAINT_LAYER_BACKGROUND), aFrame);
}

NS_IMETHODIMP
CanvasFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::canvasFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}

NS_IMETHODIMP
CanvasFrame::AttributeChanged(nsIPresContext* aPresContext,
                          nsIContent*     aChild,
                          PRInt32         aNameSpaceID,
                          nsIAtom*        aAttribute,
                          PRInt32         aModType, 
                          PRInt32         aHint)
{
// if the background color or image is changing, invalidate the canvas
  if (aHint > 0){
    if (aAttribute == nsHTMLAtoms::bgcolor ||
        aAttribute == nsHTMLAtoms::background) {
      Invalidate(aPresContext,mRect,PR_FALSE);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP 
CanvasFrame::GetContentForEvent(nsIPresContext* aPresContext,
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
CanvasFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Canvas", aResult);
}

NS_IMETHODIMP
CanvasFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = sizeof(*this);
  return NS_OK;
}
#endif
