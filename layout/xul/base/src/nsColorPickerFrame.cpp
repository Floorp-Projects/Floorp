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


#include "nsColorPickerFrame.h"

#include "nsIDOMElement.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsHTMLAtoms.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsCSSRendering.h"
#include "nsINameSpaceManager.h"
#include "nsColor.h"
#include "nsIServiceManager.h"
#include "nsStdColorPicker.h"
#include "nsColorPickerCID.h"
//
// NS_NewColorPickerFrame
//
// Wrapper for creating a new color picker
//
nsresult
NS_NewColorPickerFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsColorPickerFrame* it = new nsColorPickerFrame;
  if ( !it )
    return NS_ERROR_OUT_OF_MEMORY;
  *aNewFrame = it;
  return NS_OK;
}

static NS_DEFINE_IID(kDefColorPickerCID, NS_DEFCOLORPICKER_CID);

//
// nsColorPickerFrame cntr
//
nsColorPickerFrame::nsColorPickerFrame()
{

}

nsColorPickerFrame::~nsColorPickerFrame()
{
  delete mColorPicker;
}


NS_IMETHODIMP
nsColorPickerFrame::Init(nsIPresContext&  aPresContext,
                         nsIContent*      aContent,
                         nsIFrame*        aParent,
                         nsIStyleContext* aContext,
                         nsIFrame*        aPrevInFlow)
{
 
  nsresult rv = nsLeafFrame::Init(aPresContext, aContent, aParent, aContext,
                                  aPrevInFlow);


  nsAutoString type;
  mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::type, type);

  if (type.EqualsIgnoreCase("swatch") || type.Equals(""))
  {
    mColorPicker = new nsStdColorPicker();
    mColorPicker->Init(mContent);
  }

  return rv;
}



NS_IMETHODIMP
nsColorPickerFrame::HandleEvent(nsIPresContext& aPresContext, 
                                nsGUIEvent*     aEvent,
                                nsEventStatus&  aEventStatus)
{
  aEventStatus = nsEventStatus_eConsumeDoDefault;
	if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN)
		HandleMouseDownEvent(aPresContext, aEvent, aEventStatus);

  return NS_OK;
}

nsresult
nsColorPickerFrame::HandleMouseDownEvent(nsIPresContext& aPresContext, 
                                         nsGUIEvent*     aEvent,
                                         nsEventStatus&  aEventStatus)
{
  int x,y;
  char *color;
  // figure out what color we just picked
#ifdef DEBUG_pavlov
  printf("got mouse down.. x = %i, y = %i\n", aEvent->refPoint.x, aEvent->refPoint.y);
#endif
  x = aEvent->refPoint.x;
  y = aEvent->refPoint.y;

  nsCOMPtr<nsIDOMElement> node( do_QueryInterface(mContent) );

  nsresult rv = mColorPicker->GetColor(x, y, &color);

  if (NS_FAILED(rv))
    node->RemoveAttribute("color");
  else
    node->SetAttribute("color", color);

  return NS_OK;
}

//
// Paint
//
//
NS_METHOD 
nsColorPickerFrame::Paint(nsIPresContext& aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect& aDirtyRect,
                          nsFramePaintLayer aWhichLayer)
{
  float p2t;

  aPresContext.GetScaledPixelsToTwips(&p2t);

  const nsStyleDisplay* disp = (const nsStyleDisplay*)
  mStyleContext->GetStyleData(eStyleStruct_Display);

  // if we aren't visible then we are done.
  if (!disp->mVisible) 
	   return NS_OK;  

  // if we are visible then tell our superclass to paint
  nsLeafFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                     aWhichLayer);

  // get our border
	const nsStyleSpacing* spacing = (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
	nsMargin border(0,0,0,0);
	spacing->CalcBorderFor(this, border);

  /*
    const nsStyleColor* colorStyle = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
    nscolor color = colorStyle->mColor;
  */

  aRenderingContext.PushState();

  // set the clip region
  PRInt32 width, height;
  mColorPicker->GetSize(&width, &height);
  nsRect rect(0, 0, width*p2t, height*p2t);

  PRBool clipState;

  // Clip so we don't render outside the inner rect
  aRenderingContext.SetClipRect(rect, nsClipCombine_kIntersect, clipState);

  // call the color picker's paint method
  mColorPicker->Paint(&aPresContext, &aRenderingContext);


  aRenderingContext.PopState(clipState);

  return NS_OK;
}


//
// GetDesiredSize
//
// For now, be as big as CSS wants us to be, or some small default size.
//
void
nsColorPickerFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                   const nsHTMLReflowState& aReflowState,
                                   nsHTMLReflowMetrics& aDesiredSize)
{
  float p2t;

  aPresContext->GetScaledPixelsToTwips(&p2t);


   // if the width is set use it
	if (NS_INTRINSICSIZE != aReflowState.mComputedWidth) 
	  aDesiredSize.width = aReflowState.mComputedWidth;
  else
    aDesiredSize.width = -1;

	// if the height is set use it
 	if (NS_INTRINSICSIZE != aReflowState.mComputedHeight) 
		aDesiredSize.height = aReflowState.mComputedHeight;
  else
    aDesiredSize.height = -1;

  mColorPicker->SetSize((aDesiredSize.width == -1) ? -1 : aDesiredSize.width /p2t,
                        (aDesiredSize.height == -1) ? -1 : aDesiredSize.height/p2t);


  int width, height;
  
  mColorPicker->GetSize(&width, &height);
  
  aDesiredSize.width = nscoord(width * p2t);
  aDesiredSize.height = nscoord(height * p2t);
  aDesiredSize.ascent = nscoord(height * p2t);
  aDesiredSize.descent = 0;

} // GetDesiredSize
