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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
#include "nsBoxLayoutState.h"
//
// NS_NewColorPickerFrame
//
// Wrapper for creating a new color picker
//
nsresult
NS_NewColorPickerFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsColorPickerFrame* it = new (aPresShell) nsColorPickerFrame (aPresShell);
  if ( !it )
    return NS_ERROR_OUT_OF_MEMORY;
  *aNewFrame = it;
  return NS_OK;
}

// static NS_DEFINE_IID(kDefColorPickerCID, NS_DEFCOLORPICKER_CID);

//
// nsColorPickerFrame cntr
//

nsColorPickerFrame::~nsColorPickerFrame()
{
  delete mColorPicker;
}


NS_IMETHODIMP
nsColorPickerFrame::Init(nsIPresContext*  aPresContext,
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
nsColorPickerFrame::HandleEvent(nsIPresContext* aPresContext, 
                                nsGUIEvent*     aEvent,
                                nsEventStatus*  aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  *aEventStatus = nsEventStatus_eConsumeDoDefault;
	if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN)
		HandleMouseDownEvent(aPresContext, aEvent, aEventStatus);

  return NS_OK;
}

nsresult
nsColorPickerFrame::HandleMouseDownEvent(nsIPresContext* aPresContext, 
                                         nsGUIEvent*     aEvent,
                                         nsEventStatus*  aEventStatus)
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
nsColorPickerFrame::Paint(nsIPresContext* aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect& aDirtyRect,
                          nsFramePaintLayer aWhichLayer)
{
  float p2t;

  aPresContext->GetScaledPixelsToTwips(&p2t);

  const nsStyleDisplay* disp = (const nsStyleDisplay*)
  mStyleContext->GetStyleData(eStyleStruct_Display);

  // if we aren't visible then we are done.
  if (!disp->IsVisibleOrCollapsed()) 
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
  nsRect rect(0, 0, PRInt32(width*p2t), PRInt32(height*p2t));

  PRBool clipState;

  // Clip so we don't render outside the inner rect
  aRenderingContext.SetClipRect(rect, nsClipCombine_kIntersect, clipState);

  // call the color picker's paint method
  mColorPicker->Paint(aPresContext, &aRenderingContext);

  aRenderingContext.PopState(clipState);

  return NS_OK;
}


NS_IMETHODIMP
nsColorPickerFrame::GetPrefSize(nsBoxLayoutState& aBoxLayoutState, nsSize& aSize)
{
  float p2t;

  nsIPresContext* presContext = aBoxLayoutState.GetPresContext();

  presContext->GetScaledPixelsToTwips(&p2t);

  nsSize pixelSize(0,0);
  mColorPicker->GetSize(&pixelSize.width, &pixelSize.height);
  
  aSize.width = nscoord(pixelSize.width * p2t);
  aSize.height = nscoord(pixelSize.height * p2t);

  AddBorderAndPadding(aSize);
  AddInset(aSize);      

  return NS_OK;
}

/*
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

} // GetDesiredSize
*/
