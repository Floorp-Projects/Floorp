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

#include "nsSVGContainerFrame.h"
#include "nsIStyleContext.h"
#include "nsIPresContext.h"
#include "nsCOMPtr.h"
#include "nsHTMLIIDs.h"
#include "nsUnitConversion.h"
#include "nsINameSpaceManager.h"
#include "nsHTMLAtoms.h"
#include "nsSVGAtoms.h"
#include "nsIReflowCommand.h"
#include "nsIContent.h"
#include "nsHTMLParts.h"
#include "nsIViewManager.h"
#include "nsIView.h"
#include "nsIPresShell.h"
#include "nsCSSRendering.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsISVGFrame.h"


nsresult
NS_NewSVGContainerFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRBool aIsRoot)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsSVGContainerFrame* it = new (aPresShell) nsSVGContainerFrame(aPresShell, aIsRoot);
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewSVGContainerFrame

nsSVGContainerFrame::nsSVGContainerFrame(nsIPresShell* aPresShell, PRBool aIsRoot)
{
}


nsSVGContainerFrame::~nsSVGContainerFrame()
{
}


NS_IMETHODIMP
nsSVGContainerFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                              nsIAtom*        aListName,
                                              nsIFrame*       aChildList)
{

  nsresult r = nsHTMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);

  return r;
}


/**
 * Initialize us. 
 */
NS_IMETHODIMP
nsSVGContainerFrame::Init(nsIPresContext*  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsHTMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

 

  return rv;
}


NS_IMETHODIMP
nsSVGContainerFrame::ReflowDirtyChild(nsIPresShell* aPresShell, nsIFrame* aChild)
{
    // if we are not dirty mark ourselves dirty and tell our parent we are dirty too.
    if (!(mState & NS_FRAME_IS_DIRTY)) {      
      // Mark yourself as dirty
      mState |= NS_FRAME_IS_DIRTY;
      return mParent->ReflowDirtyChild(aPresShell, this);
    }

    return NS_OK;
}





/**
 * Ok what we want to do here is get all the children, figure out
 * their flexibility, preferred, min, max sizes and then stretch or
 * shrink them to fit in the given space.
 *
 * So we will have 3 passes. 
 * 1) get our min,max,preferred size.
 * 2) flow all our children to fit into the size we are given layout in
 * 3) move all the children to the right locations.
 */
NS_IMETHODIMP
nsSVGContainerFrame::Reflow(nsIPresContext*   aPresContext,
                     nsHTMLReflowMetrics&     aDesiredSize,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  aStatus = NS_FRAME_COMPLETE;
  if (eReflowReason_Incremental == aReflowState.reason) {
    nsIFrame* targetFrame;
    aReflowState.reflowCommand->GetTarget(targetFrame);
    // Check to see if we are the target of the Incremental Reflow
    if (targetFrame == this) {
      NS_ASSERTION(0, "Incremental reflow on nsSVGContainerFrame");
    } else {
      nsIFrame * incrementalChild;
      aReflowState.reflowCommand->GetNext(incrementalChild);
      nscoord maxWidth  = 0;
      nscoord maxHeight = 0;

      nsSize availSize(aReflowState.availableWidth, aReflowState.availableHeight);
      nsHTMLReflowMetrics kidSize(&availSize);
      nsHTMLReflowState   kidReflowState(aPresContext, aReflowState, incrementalChild, availSize);

      incrementalChild->WillReflow(aPresContext);
      incrementalChild->MoveTo(aPresContext, aReflowState.mComputedBorderPadding.left, aReflowState.mComputedBorderPadding.top);
      nsIView*  view;
      incrementalChild->GetView(aPresContext, &view);
      if (view) {
        //nsHTMLContainerFrame::PositionFrameView(aPresContext, child, view);
      }
      nsReflowStatus status;
      nsresult rv = incrementalChild->Reflow(aPresContext, kidSize, kidReflowState, status);

      nsRect rect;
      incrementalChild->GetRect(rect);
      nsCOMPtr<nsISVGFrame> svgFrame = do_QueryInterface(incrementalChild);
      if (svgFrame) {
        svgFrame->GetXY(&rect.x, &rect.y);
      }
      rect.width  = kidSize.width;
      rect.height = kidSize.height;
      maxWidth = PR_MAX(maxWidth, rect.x+rect.width);
      maxHeight = PR_MAX(maxHeight, rect.y+rect.height);

      incrementalChild->SetRect(aPresContext, rect);
      if (NS_FAILED(rv)) return rv;
      rv = incrementalChild->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
      if (NS_FAILED(rv)) return rv;

      nsIFrame * child = mFrames.FirstChild();
      while (child != nsnull) {
        child->GetRect(rect);
        nsCOMPtr<nsISVGFrame> svgFrame = do_QueryInterface(child);
        if (svgFrame) {
          svgFrame->GetXY(&rect.x, &rect.y);
        }
        maxWidth = PR_MAX(maxWidth, rect.x+rect.width);
        maxHeight = PR_MAX(maxHeight, rect.y+rect.height);
        child->GetNextSibling(&child);
      }

      aDesiredSize.width  = maxWidth;
      aDesiredSize.height = maxHeight;
      aDesiredSize.ascent = aDesiredSize.height;
      aDesiredSize.descent = 0;
    }
  } else {

    nscoord maxWidth  = 0;
    nscoord maxHeight = 0;

    nsIFrame * child = mFrames.FirstChild();

    while (child != nsnull) {

      nsSize availSize(aReflowState.availableWidth, aReflowState.availableHeight);
      nsHTMLReflowMetrics kidSize(&availSize);
      nsHTMLReflowState   kidReflowState(aPresContext, aReflowState, child, availSize);

      child->WillReflow(aPresContext);
      child->MoveTo(aPresContext, aReflowState.mComputedBorderPadding.left, aReflowState.mComputedBorderPadding.top);
      nsIView*  view;
      child->GetView(aPresContext, &view);
      if (view) {
        //nsHTMLContainerFrame::PositionFrameView(aPresContext, child, view);
      }
      nsReflowStatus status;
      nsresult rv = child->Reflow(aPresContext, kidSize, kidReflowState, status);

      nsRect rect;
      child->GetRect(rect);
      nsCOMPtr<nsISVGFrame> svgFrame = do_QueryInterface(child);
      if (svgFrame) {
        svgFrame->GetXY(&rect.x, &rect.y);
      }
      rect.width  = kidSize.width;
      rect.height = kidSize.height;
      maxWidth = PR_MAX(maxWidth, rect.x+rect.width);
      maxHeight = PR_MAX(maxHeight, rect.y+rect.height);

      child->SetRect(aPresContext, rect);
      if (NS_FAILED(rv)) return rv;
      rv = child->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
      if (NS_FAILED(rv)) return rv;
      child->GetNextSibling(&child);
    }


    aDesiredSize.width  = maxWidth;
    aDesiredSize.height = maxHeight;
    aDesiredSize.ascent = aDesiredSize.height;
    aDesiredSize.descent = 0;
  }

  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width  = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsSVGContainerFrame::DidReflow(nsIPresContext* aPresContext,
                      nsDidReflowStatus aStatus)
{
  nsresult rv = nsHTMLContainerFrame::DidReflow(aPresContext, aStatus);
  NS_ASSERTION(rv == NS_OK,"DidReflow failed");

  return rv;
}


// Marks the frame as dirty and generates an incremental reflow
// command targeted at this frame
nsresult
nsSVGContainerFrame::GenerateDirtyReflowCommand(nsIPresContext* aPresContext,
                                       nsIPresShell&   aPresShell)
{
  if (mState & NS_FRAME_IS_DIRTY)      
       return NS_OK;

  // ask out parent to dirty things.
  mState |= NS_FRAME_IS_DIRTY;
  return mParent->ReflowDirtyChild(&aPresShell, this);
}

NS_IMETHODIMP
nsSVGContainerFrame::RemoveFrame(nsIPresContext* aPresContext,
                           nsIPresShell& aPresShell,
                           nsIAtom* aListName,
                           nsIFrame* aOldFrame)
{

    // remove the child frame
    mFrames.DestroyFrame(aPresContext, aOldFrame);

    // mark us dirty and generate a reflow command
    return GenerateDirtyReflowCommand(aPresContext, aPresShell);
}

NS_IMETHODIMP
nsSVGContainerFrame::Destroy(nsIPresContext* aPresContext)
{

  return nsHTMLContainerFrame::Destroy(aPresContext);
} 


NS_IMETHODIMP
nsSVGContainerFrame::InsertFrames(nsIPresContext* aPresContext,
                            nsIPresShell& aPresShell,
                            nsIAtom* aListName,
                            nsIFrame* aPrevFrame,
                            nsIFrame* aFrameList)
{
   // insert the frames in out regular frame list
   mFrames.InsertFrames(this, aPrevFrame, aFrameList);

   // mark us dirty and generate a reflow command
   return GenerateDirtyReflowCommand(aPresContext, aPresShell);
   
}


NS_IMETHODIMP
nsSVGContainerFrame::AppendFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList)
{
   // append in regular frames
   mFrames.AppendFrames(this, aFrameList); 
   
   // mark us dirty and generate a reflow command
   return GenerateDirtyReflowCommand(aPresContext, aPresShell);
}



NS_IMETHODIMP
nsSVGContainerFrame::AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent* aChild,
                               PRInt32 aNameSpaceID,
                               nsIAtom* aAttribute,
                               PRInt32 aModType, 
                               PRInt32 aHint)
{
    nsresult rv = nsHTMLContainerFrame::AttributeChanged(aPresContext, aChild,
                                              aNameSpaceID, aAttribute, aModType, aHint);
#if 0
    if (aAttribute == nsHTMLAtoms::width ||
        aAttribute == nsHTMLAtoms::height ||
        aAttribute == nsHTMLAtoms::align  ||
        aAttribute == nsHTMLAtoms::valign ||
        aAttribute == nsXULAtoms::flex ||
        aAttribute == nsXULAtoms::orient) {

        if (aAttribute == nsXULAtoms::orient || aAttribute == nsXULAtoms::debug || aAttribute == nsHTMLAtoms::align || aAttribute == nsHTMLAtoms::valign) {
          mInner->mValign = nsSVGContainerFrame::vAlign_Top;
          mInner->mHalign = nsSVGContainerFrame::hAlign_Left;

          GetInitialVAlignment(mInner->mValign);
          GetInitialHAlignment(mInner->mHalign);
  
          PRBool orient = mState & NS_STATE_IS_HORIZONTAL;
          GetInitialOrientation(orient); 
          if (orient)
                mState |= NS_STATE_IS_HORIZONTAL;
            else
                mState &= ~NS_STATE_IS_HORIZONTAL;
   
          PRBool debug = mState & NS_STATE_SET_TO_DEBUG;
          PRBool debugSet = mInner->GetInitialDebug(debug); 
          if (debugSet) {
                mState |= NS_STATE_DEBUG_WAS_SET;
                if (debug)
                    mState |= NS_STATE_SET_TO_DEBUG;
                else
                    mState &= ~NS_STATE_SET_TO_DEBUG;
          } else {
                mState &= ~NS_STATE_DEBUG_WAS_SET;
          }


          PRBool autostretch = mState & NS_STATE_AUTO_STRETCH;
          GetInitialAutoStretch(autostretch);
          if (autostretch)
                mState |= NS_STATE_AUTO_STRETCH;
             else
                mState &= ~NS_STATE_AUTO_STRETCH;
        }

        nsCOMPtr<nsIPresShell> shell;
        aPresContext->GetShell(getter_AddRefs(shell));
        GenerateDirtyReflowCommand(aPresContext, *shell);
    }
#endif

  return rv;
}


NS_IMETHODIMP
nsSVGContainerFrame::Paint (nsIPresContext* aPresContext,
                      nsIRenderingContext&  aRenderingContext,
                      const nsRect&         aDirtyRect,
                      nsFramePaintLayer     aWhichLayer,
                      PRUint32              aFlags)
{
  const nsStyleVisibility* visib = (const nsStyleVisibility*)
  mStyleContext->GetStyleData(eStyleStruct_Visibility);

  // if we aren't visible then we are done.
  if (!visib->IsVisibleOrCollapsed()) 
	   return NS_OK;  
  //printf("nsSVGContainerFrame::Paint Start\n");
  // if we are visible then tell our superclass to paint
  nsresult r = nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                       aWhichLayer);
  //printf("nsSVGContainerFrame::Paint End\n");

  return r;
}

// Paint one child frame
void
nsSVGContainerFrame::PaintChild(nsIPresContext*   aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect&        aDirtyRect,
                             nsIFrame*            aFrame,
                             nsFramePaintLayer    aWhichLayer,
                             PRUint32             aFlags)
{
      const nsStyleVisibility* visib;
      aFrame->GetStyleData(eStyleStruct_Visibility, ((const nsStyleStruct *&)visib));

      // if collapsed don't paint the child.
      if (visib->mVisible == NS_STYLE_VISIBILITY_COLLAPSE) 
         return;

      nsHTMLContainerFrame::PaintChild(aPresContext, aRenderingContext, aDirtyRect, aFrame, aWhichLayer);
}

void
nsSVGContainerFrame::PaintChildren(nsIPresContext*   aPresContext,
                                nsIRenderingContext& aRenderingContext,
                                const nsRect&        aDirtyRect,
                                nsFramePaintLayer    aWhichLayer,
                                PRUint32             aFlags)
{
  const nsStyleDisplay* disp = (const nsStyleDisplay*)
    mStyleContext->GetStyleData(eStyleStruct_Display);

  // Child elements have the opportunity to override the visibility property
  // of their parent and display even if the parent is hidden
  PRBool clipState;

  nsRect r(0,0,mRect.width, mRect.height);
  PRBool hasClipped = PR_FALSE;
  
  // If overflow is hidden then set the clip rect so that children
  // don't leak out of us
  if (NS_STYLE_OVERFLOW_HIDDEN == disp->mOverflow) {
    //nsMargin dm(0,0,0,0);
    //mInner->GetDebugInset(dm);
    nsMargin im(0,0,0,0);
    GetInset(im);
    nsMargin border(0,0,0,0);
    nsStyleBorderPadding borderpadding;
    mStyleContext->GetBorderPaddingFor(borderpadding);
    borderpadding.GetBorderPadding(border);
    r.Deflate(im);
    //r.Deflate(dm);
    r.Deflate(border);    
  }

  nsIFrame* kid = mFrames.FirstChild();
  while (nsnull != kid) {
    if (!hasClipped && NS_STYLE_OVERFLOW_HIDDEN == disp->mOverflow) {
        // if we haven't already clipped and we should
        // check to see if the child is in out bounds. If not then
        // we begin clipping.
        nsRect cr(0,0,0,0);
        kid->GetRect(cr);
    
        // if our rect does not contain the childs then begin clipping
        if (!r.Contains(cr)) {
            aRenderingContext.PushState();
            aRenderingContext.SetClipRect(r,
                                          nsClipCombine_kIntersect, clipState);
            hasClipped = PR_TRUE;
        }
    }

    PaintChild(aPresContext, aRenderingContext, aDirtyRect, kid, aWhichLayer);
    kid->GetNextSibling(&kid);
  }

  if (hasClipped) {
    aRenderingContext.PopState(clipState);
  }
}



NS_IMETHODIMP_(nsrefcnt) 
nsSVGContainerFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsSVGContainerFrame::Release(void)
{
    return NS_OK;
}

NS_IMETHODIMP
nsSVGContainerFrame::GetFrameName(nsString& aResult) const
{
	aResult = NS_ConvertASCIItoUCS2("nsSVGContainerFrame");
	return NS_OK;
}


NS_IMETHODIMP  
nsSVGContainerFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                             const nsPoint& aPoint, 
                             nsFramePaintLayer aWhichLayer,
                             nsIFrame**     aFrame)
{   
  // this should act like a block, so we need to override
  return GetFrameForPointUsing(aPresContext, aPoint, nsnull, aWhichLayer, (aWhichLayer == NS_FRAME_PAINT_LAYER_BACKGROUND), aFrame);

}




NS_IMETHODIMP
nsSVGContainerFrame::GetCursor(nsIPresContext* aPresContext,
                           nsPoint&        aPoint,
                           PRInt32&        aCursor)
{
  
  return nsHTMLContainerFrame::GetCursor(aPresContext, aPoint, aCursor);
}

void 
nsSVGContainerFrame::GetInset(nsMargin& margin)
{
  margin.top = 0;
  margin.left = 0;
  margin.right = 0;
  margin.bottom = 0;
}

