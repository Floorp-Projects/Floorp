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


// Initialize GFX-rendered state
nsGfxCheckboxControlFrame::nsGfxCheckboxControlFrame()
  : mChecked(eOff)
{
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

  const nsStyleSpacing* spacing =
    (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);

  nsRect checkRect(0,0, mRect.width, mRect.height);

  nsMargin borderPadding;
  spacing->CalcBorderPaddingFor(this, borderPadding);
  checkRect.Deflate(borderPadding);

  const nsStyleColor* color = (const nsStyleColor*)
                                  mStyleContext->GetStyleData(eStyleStruct_Color);
  aRenderingContext.SetColor(color->mColor);

  if ( IsTristateCheckbox() ) {
    // Get current checked state through content model.
    // XXX this won't work under printing. does that matter???
    CheckState checked = GetCheckboxState();
    switch ( checked ) {   
      case eOn:
        nsFormControlHelper::PaintCheckMark(aRenderingContext, p2t, checkRect);
        break;

      case eMixed:
        PaintMixedMark(aRenderingContext, p2t, checkRect);
        break;
    } // case of value of checkbox
  } else {
    // Get current checked state through content model.
    // XXX: This is very inefficient, but it is necessary in the case of printing.
    // During printing the Paint is called but the actual state of the checkbox
    // is in a frame in presentation shell 0.
    PRBool checked = PR_FALSE;
    GetCurrentCheckState(&checked);
    if ( checked ) {
      nsFormControlHelper::PaintCheckMark(aRenderingContext, p2t, checkRect);
    }
  }
  
  PRBool clip;
  aRenderingContext.PopState(clip);
}


//
// PaintMixedMark
//
// Like nsFormControlHelper::PaintCheckMark(), but paints the horizontal "mixed"
// bar inside the box. Only used for tri-state checkboxes.
//
void
nsGfxCheckboxControlFrame::PaintMixedMark ( nsIRenderingContext& aRenderingContext,
                                             float aPixelsToTwips, const nsRect& aRect)
{
  const PRUint32 checkpoints = 4;
  const PRUint32 checksize   = 6; //This is value is determined by added 2 units to the end
                                //of the 7X& pixel rectangle below to provide some white space
                                //around the checkmark when it is rendered.

  // Points come from the coordinates on a 7X7 pixels 
  // box with 0,0 at the lower left. 
  nscoord checkedPolygonDef[] = { 1,2,  5,2,  5,4, 1,4 };
  // Location of the center point of the checkmark
  const PRUint32 centerx = 3;
  const PRUint32 centery = 3;
  
  nsPoint checkedPolygon[checkpoints];
  PRUint32 defIndex = 0;
  PRUint32 polyIndex = 0;

  // Scale the checkmark based on the smallest dimension
  PRUint32 size = aRect.width / checksize;
  if (aRect.height < aRect.width)
   size = aRect.height / checksize;
  
  // Center and offset each point in the polygon definition.
  for (defIndex = 0; defIndex < (checkpoints * 2); defIndex++) {
    checkedPolygon[polyIndex].x = nscoord((((checkedPolygonDef[defIndex]) - centerx) * (size)) + (aRect.width / 2) + aRect.x);
    defIndex++;
    checkedPolygon[polyIndex].y = nscoord((((checkedPolygonDef[defIndex]) - centery) * (size)) + (aRect.height / 2) + aRect.y);
    polyIndex++;
  }
  
  aRenderingContext.FillPolygon(checkedPolygon, checkpoints);

} // PaintMixedMark


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


nsCheckboxControlFrame::CheckState
nsGfxCheckboxControlFrame :: GetCheckboxState ( )
{
  return mChecked;
}


void 
nsGfxCheckboxControlFrame :: SetCheckboxState ( nsCheckboxControlFrame::CheckState aValue )
{
  mChecked = aValue;
  nsFormControlHelper::ForceDrawFrame(this);
}

