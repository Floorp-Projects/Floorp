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

#include "nsGfxButtonControlFrame.h"
#include "nsHTMLAtoms.h"
#include "nsFormFrame.h"
#include "nsIFormControl.h"
#include "nsIContent.h"
#include "nsIButton.h"
#include "nsINameSpaceManager.h"


static NS_DEFINE_IID(kIButtonIID,      NS_IBUTTON_IID);

nsresult
NS_NewGfxButtonControlFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsGfxButtonControlFrame* it = new nsGfxButtonControlFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}


nsGfxButtonControlFrame::nsGfxButtonControlFrame()
{
  mRenderer.SetNameSpace(kNameSpaceID_None);
}

NS_IMETHODIMP
nsGfxButtonControlFrame::Init(nsIPresContext&  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
  nsresult  rv = Inherited::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  mRenderer.SetFrame(this,aPresContext);
  return rv;
}


//
// ReResolveStyleContext
//
// When the style context changes, make sure that all of our styles are still up to date.
//
NS_IMETHODIMP
nsGfxButtonControlFrame::ReResolveStyleContext ( nsIPresContext* aPresContext, nsIStyleContext* aParentContext,
                                              PRInt32 aParentChange, 
                                              nsStyleChangeList* aChangeList, PRInt32* aLocalChange)
{
  // this re-resolves |mStyleContext|, so it may change
  nsresult rv = Inherited::ReResolveStyleContext(aPresContext, aParentContext, 
                                                          aParentChange, aChangeList, aLocalChange); 
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (NS_COMFALSE != rv) {  // frame style changed
    if (aLocalChange) {
      aParentChange = *aLocalChange;  // tell children about or change
    }
  }
  mRenderer.ReResolveStyles(*aPresContext, aParentChange, aChangeList, aLocalChange);
  
  return rv;
  
} // ReResolveStyleContext


                                     

NS_METHOD 
nsGfxButtonControlFrame::Paint(nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect,
                            nsFramePaintLayer aWhichLayer)
{
  const nsStyleDisplay* disp = (const nsStyleDisplay*)
	mStyleContext->GetStyleData(eStyleStruct_Display);
	if (!disp->mVisible)
     return NS_OK;

    nsRect rect(0, 0, mRect.width, mRect.height);
    mRenderer.PaintButton(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer, rect);

    if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
	    nsString label;
	    nsresult result = GetValue(&label);

	    if (NS_CONTENT_ATTR_HAS_VALUE != result) {  
        GetDefaultLabel(label);
	    }
  
     nsRect content;
     mRenderer.GetButtonContentRect(rect,content);

   // paint the title 
	   const nsStyleFont* fontStyle = (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);
	   const nsStyleColor* colorStyle = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);

	   aRenderingContext.SetFont(fontStyle->mFont);
	   
     PRBool clipState;
     aRenderingContext.PushState();
     aRenderingContext.SetClipRect(rect, nsClipCombine_kIntersect, clipState);
	   // if disabled paint 
	   if (PR_TRUE == mRenderer.isDisabled())
	   {
   		 float p2t;
		   aPresContext.GetScaledPixelsToTwips(&p2t);
		   nscoord pixel = NSIntPixelsToTwips(1, p2t);

		   aRenderingContext.SetColor(NS_RGB(255,255,255));
		   aRenderingContext.DrawString(label, content.x + pixel, content.y+ pixel);
	   }

	   aRenderingContext.SetColor(colorStyle->mColor);
	   aRenderingContext.DrawString(label, content.x, content.y);
	   aRenderingContext.PopState(clipState);
    }

  return NS_OK;
}

NS_IMETHODIMP
nsGfxButtonControlFrame::AttributeChanged(nsIPresContext* aPresContext,
                                       nsIContent*     aChild,
                                       nsIAtom*        aAttribute,
                                       PRInt32         aHint)
{
  if (nsHTMLAtoms::value == aAttribute) {
   // redraw button with the changed value
    Invalidate(mRect, PR_FALSE);
  }
  return NS_OK;
}

NS_METHOD
nsGfxButtonControlFrame::Reflow(nsIPresContext&          aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus)
{

  // add ourself as an nsIFormControlFrame
  if (!mFormFrame && (eReflowReason_Initial == aReflowState.reason)) {
       nsFormFrame::AddFormControlFrame(aPresContext, *this);
  }

	if ((NS_FORMSIZE_NOTSET != mSuggestedWidth) && (
	  NS_FORMSIZE_NOTSET != mSuggestedHeight)) 
	{ 
		aDesiredSize.width   = mSuggestedWidth;
		aDesiredSize.height  = mSuggestedHeight;
		aDesiredSize.ascent  = mSuggestedHeight;
		aDesiredSize.descent = 0;
		if (aDesiredSize.maxElementSize) {
		  aDesiredSize.maxElementSize->width  = mSuggestedWidth;
		  aDesiredSize.maxElementSize->height = mSuggestedWidth;
	}

	if (nsnull != aDesiredSize.maxElementSize) {
	  aDesiredSize.maxElementSize->width = aDesiredSize.width;
	  aDesiredSize.maxElementSize->height = aDesiredSize.height;
	}

	aStatus = NS_FRAME_COMPLETE;
	return NS_OK;

	}

	nsSize ignore;
	GetDesiredSize(&aPresContext, aReflowState, aDesiredSize, ignore);

	// if our size is intrinsic. Then we need to return the size we really need
	// so include our inner borders we use for focus.
	nsMargin added = mRenderer.GetAddedButtonBorderAndPadding();
	if (aReflowState.mComputedWidth == NS_INTRINSICSIZE)
	   aDesiredSize.width += added.left + added.right;

	if (aReflowState.mComputedHeight == NS_INTRINSICSIZE)
	   aDesiredSize.height += added.top + added.bottom;

	nsMargin bp(0,0,0,0);
	AddBordersAndPadding(&aPresContext, aReflowState, aDesiredSize, bp);
	   

	if (nsnull != aDesiredSize.maxElementSize) {
	  aDesiredSize.AddBorderPaddingToMaxElementSize(bp);
	  aDesiredSize.maxElementSize->width += added.left + added.right;
	  aDesiredSize.maxElementSize->height += added.top + added.bottom;

	}
	aStatus = NS_FRAME_COMPLETE;
	return NS_OK;
}


NS_IMETHODIMP
nsGfxButtonControlFrame::HandleEvent(nsIPresContext& aPresContext, 
                                  nsGUIEvent*     aEvent,
                                  nsEventStatus&  aEventStatus)
{
  // if disabled do nothing
  if (mRenderer.isDisabled()) {
    return NS_OK;
  }

  return Inherited::HandleEvent(aPresContext, aEvent, aEventStatus);
}
