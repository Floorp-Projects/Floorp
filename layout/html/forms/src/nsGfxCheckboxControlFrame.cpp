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

#include "nsGfxCheckboxControlFrame.h"
#include "nsICheckButton.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLParts.h"
#include "nsIFormControl.h"
#include "nsIContent.h"
#include "nsIComponentManager.h"
#include "nsHTMLAtoms.h"
#include "nsIPresState.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsCSSRendering.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif
#include "nsIServiceManager.h"
#include "nsIDOMNode.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsITheme.h"
#include "imgIRequest.h"


//------------------------------------------------------------
nsresult
NS_NewGfxCheckboxControlFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsGfxCheckboxControlFrame* it = new (aPresShell) nsGfxCheckboxControlFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}


//------------------------------------------------------------
// Initialize GFX-rendered state
nsGfxCheckboxControlFrame::nsGfxCheckboxControlFrame()
: mCheckButtonFaceStyle(nsnull)
{
}

nsGfxCheckboxControlFrame::~nsGfxCheckboxControlFrame()
{
  if (mCheckButtonFaceStyle)
    mCheckButtonFaceStyle->Release();
}


//----------------------------------------------------------------------
// nsISupports
//----------------------------------------------------------------------
// Frames are not refcounted, no need to AddRef
NS_IMETHODIMP
nsGfxCheckboxControlFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_ASSERTION(aInstancePtr, "QueryInterface requires a non-NULL destination!");
  if ( !aInstancePtr )
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(NS_GET_IID(nsICheckboxControlFrame))) {
    *aInstancePtr = (void*) ((nsICheckboxControlFrame*) this);
    return NS_OK;
  }

  return nsFormControlFrame::QueryInterface(aIID, aInstancePtr);
}

#ifdef ACCESSIBILITY
NS_IMETHODIMP nsGfxCheckboxControlFrame::GetAccessible(nsIAccessible** aAccessible)
{
  nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");

  if (accService) {
    return accService->CreateHTMLCheckboxAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
  }

  return NS_ERROR_FAILURE;
}
#endif

//--------------------------------------------------------------
NS_IMETHODIMP
nsGfxCheckboxControlFrame::SetCheckboxFaceStyleContext(nsStyleContext *aCheckboxFaceStyleContext)
{
  mCheckButtonFaceStyle = aCheckboxFaceStyleContext;
  mCheckButtonFaceStyle->AddRef();
  return NS_OK;
}


//--------------------------------------------------------------
nsStyleContext*
nsGfxCheckboxControlFrame::GetAdditionalStyleContext(PRInt32 aIndex) const
{
  switch (aIndex) {
  case NS_GFX_CHECKBOX_CONTROL_FRAME_FACE_CONTEXT_INDEX:
    return mCheckButtonFaceStyle;
    break;
  default:
    return nsnull;
  }
}



//--------------------------------------------------------------
void
nsGfxCheckboxControlFrame::SetAdditionalStyleContext(PRInt32 aIndex, 
                                                     nsStyleContext* aStyleContext)
{
  switch (aIndex) {
  case NS_GFX_CHECKBOX_CONTROL_FRAME_FACE_CONTEXT_INDEX:
    if (mCheckButtonFaceStyle)
      mCheckButtonFaceStyle->Release();
    mCheckButtonFaceStyle = aStyleContext;
    if (aStyleContext)
      aStyleContext->AddRef();
    break;
  }
}


//------------------------------------------------------------
NS_IMETHODIMP
nsGfxCheckboxControlFrame::OnChecked(nsPresContext* aPresContext,
                                     PRBool aChecked)
{
  Invalidate(GetOverflowRect(), PR_FALSE);
  return aChecked;
}


//------------------------------------------------------------
void
nsGfxCheckboxControlFrame::PaintCheckBox(nsPresContext* aPresContext,
                                         nsIRenderingContext& aRenderingContext,
                                         const nsRect& aDirtyRect,
                                         nsFramePaintLayer aWhichLayer)
{
  const nsStyleDisplay* disp = GetStyleDisplay();
  if (disp->mAppearance) {
    nsITheme *theme = aPresContext->GetTheme();
    if (theme && theme->ThemeSupportsWidget(aPresContext, this, disp->mAppearance))
      return; // No need to paint the checkbox. The theme will do it.
  }

  // Get current checked state through content model.
  if (!GetCheckboxState())
    return;   // we're not checked, nothing to paint.

  aRenderingContext.PushState();

  nsMargin borderPadding(0,0,0,0);
  CalcBorderPadding(borderPadding);

  nsRect checkRect(0,0, mRect.width, mRect.height);
  checkRect.Deflate(borderPadding);

  const nsStyleColor* color = GetStyleColor();
  aRenderingContext.SetColor(color->mColor);

  nsFormControlHelper::PaintCheckMark(aRenderingContext,
                                      aPresContext->ScaledPixelsToTwips(),
                                      checkRect);

  aRenderingContext.PopState();
}


//------------------------------------------------------------
NS_METHOD 
nsGfxCheckboxControlFrame::Paint(nsPresContext*   aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              const nsRect&        aDirtyRect,
                              nsFramePaintLayer    aWhichLayer,
                              PRUint32             aFlags)
{
  PRBool isVisible;
  if (NS_SUCCEEDED(IsVisibleForPainting(aPresContext, aRenderingContext, PR_TRUE, &isVisible)) && !isVisible) {
    return NS_OK;
  }

  // Paint the background
  nsresult rv = nsFormControlFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    PRBool doDefaultPainting = PR_TRUE;
    // Paint the checkmark
    if (!mCheckButtonFaceStyle && GetCheckboxState()) {
      const nsStyleBackground* myColor = mCheckButtonFaceStyle->GetStyleBackground();

      if (myColor->mBackgroundImage) {
        const nsStyleBorder* myBorder = mCheckButtonFaceStyle->GetStyleBorder();
        const nsStylePadding* myPadding = mCheckButtonFaceStyle->GetStylePadding();
        const nsStylePosition* myPosition = mCheckButtonFaceStyle->GetStylePosition();

        nscoord width = myPosition->mWidth.GetCoordValue();
        nscoord height = myPosition->mHeight.GetCoordValue();
         // Position the button centered within the radio control's rectangle.
        nscoord x = (mRect.width - width) / 2;
        nscoord y = (mRect.height - height) / 2;
        nsRect rect(x, y, width, height); 

        nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                        aDirtyRect, rect, *myBorder, *myPadding,
                                        PR_FALSE);
        nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                    aDirtyRect, rect, *myBorder, mCheckButtonFaceStyle, 0);
        doDefaultPainting = PR_FALSE;
      }
    } 

    if (doDefaultPainting) {
      PaintCheckBox(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
    }
  }
  return rv;
}

//------------------------------------------------------------
PRBool
nsGfxCheckboxControlFrame::GetCheckboxState ( )
{
  nsCOMPtr<nsIDOMHTMLInputElement> elem(do_QueryInterface(mContent));
  PRBool retval = PR_FALSE;
  elem->GetChecked(&retval);
  return retval;
}

//------------------------------------------------------------
// Extra Debug Methods
//------------------------------------------------------------
#ifdef DEBUG_rodsXXX
NS_IMETHODIMP
nsGfxCheckboxControlFrame::Reflow(nsPresContext*          aPresContext, 
                                  nsHTMLReflowMetrics&     aDesiredSize,
                                  const nsHTMLReflowState& aReflowState, 
                                  nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsGfxCheckboxControlFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  nsresult rv = nsFormControlFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

  COMPARE_QUIRK_SIZE("nsGfxCheckboxControlFrame", 13, 13) 
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return rv;
}
#endif

NS_IMETHODIMP
nsGfxCheckboxControlFrame::OnContentReset()
{
  return NS_OK;
}
