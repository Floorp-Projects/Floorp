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

#include "nsGfxCheckboxControlFrame.h"
#include "nsICheckButton.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLParts.h"
#include "nsFormFrame.h"
#include "nsIFormControl.h"
#include "nsIContent.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"



nsresult
NS_NewGfxCheckboxControlFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsGfxCheckboxControlFrame* it = new nsGfxCheckboxControlFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsGfxCheckboxControlFrame::nsGfxCheckboxControlFrame()
{
   // Initialize GFX-rendered state
  mMouseDownOnCheckbox = PR_FALSE;
  mChecked = PR_FALSE;
}

void 
nsGfxCheckboxControlFrame::MouseClicked(nsIPresContext* aPresContext) 
{
  mMouseDownOnCheckbox = PR_FALSE;
  Inherited::MouseClicked(aPresContext);
}

void
nsGfxCheckboxControlFrame::PaintCheckBox(nsIPresContext& aPresContext,
                  nsIRenderingContext& aRenderingContext,
                  const nsRect& aDirtyRect,
                  nsFramePaintLayer aWhichLayer)
{
  aRenderingContext.PushState();

  float p2t;
  aPresContext.GetScaledPixelsToTwips(&p2t);

  PRBool checked = PR_FALSE;

    // Get current checked state through content model.
    // XXX: This is very inefficient, but it is necessary in the case of printing.
    // During printing the Paint is called but the actual state of the checkbox
    // is in a frame in presentation shell 0.
  /*XXXnsresult result = */GetCurrentCheckState(&checked);
  if (PR_TRUE == checked) {
      // Draw check mark
    const nsStyleColor* color = (const nsStyleColor*)
      mStyleContext->GetStyleData(eStyleStruct_Color);
    aRenderingContext.SetColor(color->mColor);
    nsFormControlHelper::PaintCheckMark(aRenderingContext,
                         p2t, mRect.width, mRect.height);
   
  }
  PRBool clip;
  aRenderingContext.PopState(clip);
}


NS_METHOD 
nsGfxCheckboxControlFrame::Paint(nsIPresContext& aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              const nsRect& aDirtyRect,
                              nsFramePaintLayer aWhichLayer)
{

  const nsStyleDisplay* disp = (const nsStyleDisplay*)
	mStyleContext->GetStyleData(eStyleStruct_Display);
	if (!disp->mVisible)
		return NS_OK;

    // Paint the background
  Inherited::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
      // Paint the checkmark 
    PaintCheckBox(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  }
  return NS_OK;
}

NS_METHOD nsGfxCheckboxControlFrame::HandleEvent(nsIPresContext& aPresContext, 
                                              nsGUIEvent* aEvent,
                                              nsEventStatus& aEventStatus)
{
  if (nsEventStatus_eConsumeNoDefault == aEventStatus) {
    return NS_OK;
  }

    // Handle GFX rendered widget Mouse Down event
  PRInt32 type;
  GetType(&type);
  switch (aEvent->message) {
    case NS_MOUSE_LEFT_BUTTON_DOWN:
      mMouseDownOnCheckbox = PR_TRUE;
  //XXX: TODO render gray rectangle on mouse down  
    break;

    case NS_MOUSE_EXIT:
      mMouseDownOnCheckbox = PR_FALSE;
  //XXX: TO DO clear gray rectangle on mouse up 
    break;

    }

  return(Inherited::HandleEvent(aPresContext, aEvent, aEventStatus));
}


PRBool nsGfxCheckboxControlFrame::GetCheckboxState()
{
  return mChecked;
}

void nsGfxCheckboxControlFrame::SetCheckboxState(PRBool aValue)
{
  mChecked = aValue;
	nsFormControlHelper::ForceDrawFrame(this);
}

