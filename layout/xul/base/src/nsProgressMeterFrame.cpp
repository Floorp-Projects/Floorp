/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

//
// Eric Vaughan
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsProgressMeterFrame.h"
#include "nsCSSRendering.h"
#include "nsIContent.h"
#include "nsIPresContext.h"
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsCOMPtr.h"
//
// NS_NewToolbarFrame
//
// Creates a new Toolbar frame and returns it in |aNewFrame|
//
nsresult
NS_NewProgressMeterFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsProgressMeterFrame* it = new (aPresShell) nsProgressMeterFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewProgressMeterFrame

//
// nsProgressMeterFrame cntr
//
// Init, if necessary
//
nsProgressMeterFrame :: nsProgressMeterFrame ( )
{
	mProgress = float(0.0);
	mHorizontal = PR_TRUE;
	mUndetermined = PR_FALSE;
}

//
// nsProgressMeterFrame dstr
//
// Cleanup, if necessary
//
nsProgressMeterFrame :: ~nsProgressMeterFrame ( )
{
}

NS_IMETHODIMP
nsProgressMeterFrame::Init(nsIPresContext*  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        aPrevInFlow)
{
 
  nsresult  rv = nsLeafFrame::Init(aPresContext, aContent, aParent, aContext,
                                   aPrevInFlow);

  // get the value
  nsAutoString value;
  if ((NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::value, value)) &&
      (value.Length() > 0)) {
	  setProgress(value);
  }


   // get the alignment
  nsAutoString align;
  mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::align, align);
  setAlignment(align); 

  // get the mode
  nsAutoString mode;
  mContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::mode, mode);
  setMode(mode); 

  return rv;
}

void
nsProgressMeterFrame::setProgress(nsAutoString progress)
{
	// convert to and integer
	PRInt32 error;
	PRInt32 v = progress.ToInteger(&error);
 
	// adjust to 0 and 100
	if (v < 0)
		v = 0;
	else if (v > 100)
		v = 100;

//	printf("ProgressMeter value=%d\n", v);
    mProgress = float(v)/float(100);
}

void
nsProgressMeterFrame::setAlignment(nsAutoString progress)
{
    if (progress.EqualsIgnoreCase("vertical"))
		  mHorizontal = PR_FALSE;
    else
		  mHorizontal = PR_TRUE;
}

void
nsProgressMeterFrame::setMode(nsAutoString mode)
{
    if (mode.EqualsIgnoreCase("undetermined"))
		  mUndetermined = PR_TRUE;
    else
		  mUndetermined = PR_FALSE;
}


//
// Paint
//
// Paint our background and border like normal frames, but before we draw the
// children, draw our grippies for each toolbar.
//
NS_IMETHODIMP
nsProgressMeterFrame :: Paint ( nsIPresContext* aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect,
                            nsFramePaintLayer aWhichLayer)
{
  const nsStyleDisplay* disp = (const nsStyleDisplay*)
  mStyleContext->GetStyleData(eStyleStruct_Display);

  // if we aren't visible then we are done.
  if (!disp->mVisible) 
	   return NS_OK;  

  // if we are visible then tell our superclass to paint
  nsLeafFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                       aWhichLayer);
  
    if (aWhichLayer == NS_FRAME_PAINT_LAYER_FOREGROUND)
    {
        if (!mUndetermined) {
            // get our border
            const nsStyleSpacing* spacing =
            (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
            nsMargin border(0,0,0,0);
            spacing->CalcBorderFor(this, border);

            const nsStyleColor* colorStyle =
            (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);

            nscolor color = colorStyle->mColor;

            // figure out our rectangle
            nsRect rect(0,0,mRect.width, mRect.height);


            //CalcSize(aPresContext,rect.width,rect.height);
            rect.x = border.left;
            rect.y = border.top;
            rect.width -= border.left + border.right;
            rect.height -= border.top + border.bottom;

            // paint the current progress in blue
            PaintBar(aPresContext, aRenderingContext, rect, mProgress, color);
        }
    }

  return NS_OK;  
} // Paint

void
nsProgressMeterFrame :: PaintBar ( nsIPresContext* aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& rect,
							float progress,
							nscolor color) {

    nsRect bar(rect);

    if (mHorizontal) 
       bar.width = (nscoord)(bar.width*progress);
    else 
       bar.height = (nscoord)(bar.height*progress);
    
    aRenderingContext.SetColor(color);
    aRenderingContext.FillRect(bar);
}

//
// Reflow
//
// Handle moving children around.
//
NS_IMETHODIMP
nsProgressMeterFrame :: Reflow ( nsIPresContext*          aPresContext,
                            nsHTMLReflowMetrics&     aDesiredSize,
                            const nsHTMLReflowState& aReflowState,
                            nsReflowStatus&          aStatus)
{	

  // handle dirty and incremental reflow
  if (eReflowReason_Incremental == aReflowState.reason) {
    nsIFrame* targetFrame;
  
    // See if it's targeted at us
    aReflowState.reflowCommand->GetTarget(targetFrame);
    if (this == targetFrame) {
      Invalidate(aPresContext, nsRect(0,0,mRect.width,mRect.height), PR_FALSE);
    }
  } else if (eReflowReason_Dirty == aReflowState.reason) {
      Invalidate(aPresContext, nsRect(0,0,mRect.width,mRect.height), PR_FALSE);
  }

  return nsLeafFrame::Reflow ( aPresContext, aDesiredSize, aReflowState, aStatus );

} // Reflow 

void
nsProgressMeterFrame::GetDesiredSize(nsIPresContext* aPresContext,
                             const nsHTMLReflowState& aReflowState,
                             nsHTMLReflowMetrics& aDesiredSize)
{

  CalcSize(aPresContext,aDesiredSize.width,aDesiredSize.height);

   // if the width is set use it
	if (NS_INTRINSICSIZE != aReflowState.mComputedWidth) 
	  aDesiredSize.width = aReflowState.mComputedWidth;

	// if the height is set use it
 	if (NS_INTRINSICSIZE != aReflowState.mComputedHeight) 
		aDesiredSize.height = aReflowState.mComputedHeight;
}


void
nsProgressMeterFrame::CalcSize(nsIPresContext* aPresContext, int& width, int& height)
{
    // set up a default size for the progress meter.
	float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);

	if (mHorizontal) {
		width = (int)(100 * p2t);
		height = (int)(16 * p2t);
	} else {
		height = (int)(100 * p2t);
		width = (int)(16 * p2t);
	}
}

NS_IMETHODIMP
nsProgressMeterFrame::GetBoxInfo(nsIPresContext* aPresContext, const nsHTMLReflowState& aReflowState, nsBoxInfo& aSize)
{
    CalcSize(aPresContext, aSize.prefSize.width, aSize.prefSize.height);
    aSize.minSize = aSize.prefSize;
    return NS_OK;
}

NS_INTERFACE_MAP_BEGIN(nsProgressMeterFrame)
  NS_INTERFACE_MAP_ENTRY(nsIBox)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIBox)
NS_INTERFACE_MAP_END_INHERITING(nsLeafFrame)



NS_IMETHODIMP
nsProgressMeterFrame::InvalidateCache(nsIFrame* aChild)
{
    // we don't have any cached children
    return NS_OK;
}

NS_IMETHODIMP
nsProgressMeterFrame::AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent* aChild,
                               PRInt32 aNameSpaceID,
                               nsIAtom* aAttribute,
                               PRInt32 aHint)
{
  nsresult rv = nsLeafFrame::AttributeChanged(aPresContext, aChild,
                                              aNameSpaceID, aAttribute, aHint);
  if (NS_OK != rv) {
    return rv;
  }

  // did the progress change?
  if (nsHTMLAtoms::value == aAttribute) {
    nsAutoString newValue;

	// get attribute and set it
    aChild->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::value, newValue);
    setProgress(newValue);

    Redraw(aPresContext);

  } else if (nsXULAtoms::mode == aAttribute) {
    nsAutoString newValue;

    aChild->GetAttribute(kNameSpaceID_None, nsXULAtoms::mode, newValue);
    setMode(newValue);

    // needs to reflow so we start the timer.
   /// if (aHint != NS_STYLE_HINT_REFLOW)
    //  Reflow(aPresContext);
    aPresContext->StopAllLoadImagesFor(this);

  } else if (nsHTMLAtoms::align == aAttribute) {
    nsAutoString newValue;

	// get attribute and set it
    aChild->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::align, newValue);
    setAlignment(newValue);

    if (aHint != NS_STYLE_HINT_REFLOW)
      Reflow(aPresContext);
  }

  return NS_OK;
}

void
nsProgressMeterFrame::Reflow(nsIPresContext* aPresContext)
{
   nsCOMPtr<nsIPresShell> shell;
   aPresContext->GetShell(getter_AddRefs(shell));

   // reflow
   mState |= NS_FRAME_IS_DIRTY;
   mParent->ReflowDirtyChild(shell, this);
}

void
nsProgressMeterFrame::Redraw(nsIPresContext* aPresContext)
{
   	nsRect frameRect;
	GetRect(frameRect);
	nsRect rect(0, 0, frameRect.width, frameRect.height);
    Invalidate(aPresContext, rect, PR_TRUE);
}






