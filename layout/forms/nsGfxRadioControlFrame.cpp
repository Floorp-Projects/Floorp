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

#include "nsGfxRadioControlFrame.h"
#include "nsIRadioButton.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLParts.h"
#include "nsFormFrame.h"
#include "nsIFormControl.h"
#include "nsIContent.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsCSSRendering.h"


nsresult
NS_NewGfxRadioControlFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsGfxRadioControlFrame* it = new nsGfxRadioControlFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsGfxRadioControlFrame::nsGfxRadioControlFrame()
{
   // Initialize GFX-rendered state
  mChecked = PR_FALSE;
  mRadioButtonFaceStyle = nsnull;
}

nsGfxRadioControlFrame::~nsGfxRadioControlFrame()
{
 NS_IF_RELEASE(mRadioButtonFaceStyle);
}

//--------------------------------------------------------------

NS_IMETHODIMP
nsGfxRadioControlFrame::GetAdditionalStyleContext(PRInt32 aIndex, 
                                                  nsIStyleContext** aStyleContext) const
{
  NS_PRECONDITION(nsnull != aStyleContext, "null OUT parameter pointer");
  if (aIndex < 0) {
    return NS_ERROR_INVALID_ARG;
  }
  *aStyleContext = nsnull;
  switch (aIndex) {
  case NS_GFX_RADIO_CONTROL_FRAME_FACE_CONTEXT_INDEX:
    *aStyleContext = mRadioButtonFaceStyle;
    NS_ADDREF(*aStyleContext);
    break;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsGfxRadioControlFrame::SetAdditionalStyleContext(PRInt32 aIndex, 
                                                  nsIStyleContext* aStyleContext)
{
  if (aIndex < 0) {
    return NS_ERROR_INVALID_ARG;
  }
  switch (aIndex) {
  case NS_GFX_RADIO_CONTROL_FRAME_FACE_CONTEXT_INDEX:
    NS_IF_RELEASE(mRadioButtonFaceStyle);
    mRadioButtonFaceStyle = aStyleContext;
    NS_IF_ADDREF(aStyleContext);
    break;
  }
  return NS_OK;
}


NS_IMETHODIMP
nsGfxRadioControlFrame::ReResolveStyleContext(nsIPresContext* aPresContext,
                                              nsIStyleContext* aParentContext,
                                              PRInt32 aParentChange,
                                              nsStyleChangeList* aChangeList,
                                              PRInt32* aLocalChange)
{
 // this re-resolves |mStyleContext|, so it may change
  nsresult rv = Inherited::ReResolveStyleContext(aPresContext, aParentContext, aParentChange,
                                               aChangeList, aLocalChange); 
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (NS_COMFALSE != rv) {  // frame style changed
    if (aLocalChange) {
      aParentChange = *aLocalChange;  // tell children about or change
    }
  }

 // see if the outline has changed.
  nsCOMPtr<nsIStyleContext> oldRadioButtonFaceStyle = mRadioButtonFaceStyle;
	aPresContext->ProbePseudoStyleContextFor(mContent, nsHTMLAtoms::radioPseudo, mStyleContext,
										  PR_FALSE,
										  &mRadioButtonFaceStyle);

  if ((mRadioButtonFaceStyle && oldRadioButtonFaceStyle.get()) && (mRadioButtonFaceStyle != oldRadioButtonFaceStyle.get())) {
    nsRadioControlFrame::CaptureStyleChangeFor(this, oldRadioButtonFaceStyle, mRadioButtonFaceStyle, 
                              aParentChange, aChangeList, aLocalChange);
  }

  return rv;
}


NS_IMETHODIMP
nsGfxRadioControlFrame::SetRadioButtonFaceStyleContext(nsIStyleContext *aRadioButtonFaceStyleContext)
{
  mRadioButtonFaceStyle = aRadioButtonFaceStyleContext;
  NS_ADDREF(mRadioButtonFaceStyle);
  return NS_OK;
}

void
nsGfxRadioControlFrame::PaintRadioButton(nsIPresContext& aPresContext,
                                      nsIRenderingContext& aRenderingContext,
                                      const nsRect& aDirtyRect)
{
   
  PRBool checked = PR_TRUE;
  GetCurrentCheckState(&checked); // Get check state from the content model
  if (PR_TRUE == checked) {
    // Paint the button for the radio button using CSS background rendering code
   if (nsnull != mRadioButtonFaceStyle) {
     const nsStyleColor* myColor = (const nsStyleColor*)
          mRadioButtonFaceStyle->GetStyleData(eStyleStruct_Color);
     const nsStyleSpacing* mySpacing = (const nsStyleSpacing*)
          mRadioButtonFaceStyle->GetStyleData(eStyleStruct_Spacing);
     const nsStylePosition* myPosition = (const nsStylePosition*)
          mRadioButtonFaceStyle->GetStyleData(eStyleStruct_Position);

     nscoord width = myPosition->mWidth.GetCoordValue();
     nscoord height = myPosition->mHeight.GetCoordValue();
       // Position the button centered within the radio control's rectangle.
     nscoord x = (mRect.width - width) / 2;
     nscoord y = (mRect.height - height) / 2;
     nsRect rect(x, y, width, height); 

     nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                        aDirtyRect, rect, *myColor, *mySpacing, 0, 0);
     nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                  aDirtyRect, rect, *mySpacing, mRadioButtonFaceStyle, 0);
   }
  }
}

NS_METHOD 
nsGfxRadioControlFrame::Paint(nsIPresContext& aPresContext,
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
    PaintRadioButton(aPresContext, aRenderingContext, aDirtyRect);
  }
  return NS_OK;
}


PRBool nsGfxRadioControlFrame::GetRadioState()
{
  return mChecked;
}

void nsGfxRadioControlFrame::SetRadioState(PRBool aValue)
{
  mChecked = aValue;
	nsFormControlHelper::ForceDrawFrame(this);
}
