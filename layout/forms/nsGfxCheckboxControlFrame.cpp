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
#include "nsHTMLAtoms.h"
#include "nsHTMLParts.h"
#include "nsIFormControl.h"
#include "nsIContent.h"
#include "nsIComponentManager.h"
#include "nsHTMLAtoms.h"
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
#include "nsDisplayList.h"

static void
PaintCheckMark(nsIRenderingContext& aRenderingContext,
               float aPixelsToTwips, const nsRect& aRect)
{
  const PRUint32 checkpoints = 7;
  const PRUint32 checksize   = 9; //This is value is determined by added 2 units to the end
                                  //of the 7X7 pixel rectangle below to provide some white space
                                  //around the checkmark when it is rendered.

    // Points come from the coordinates on a 7X7 pixels 
    // box with 0,0 at the lower left. 
  nscoord checkedPolygonDef[] = {0,2, 2,4, 6,0 , 6,2, 2,6, 0,4, 0,2 };
    // Location of the center point of the checkmark
  const PRUint32 centerx = 3;
  const PRUint32 centery = 3;
  
  nsPoint checkedPolygon[checkpoints];
  PRUint32 defIndex = 0;
  PRUint32 polyIndex = 0;

     // Scale the checkmark based on the smallest dimension
  PRUint32 size = aRect.width / checksize;
  if (aRect.height < aRect.width) {
   size = aRect.height / checksize;
  }
  
    // Center and offset each point in the polygon definition.
  for (defIndex = 0; defIndex < (checkpoints * 2); defIndex++) {
    checkedPolygon[polyIndex].x =
      nscoord((((checkedPolygonDef[defIndex]) - centerx) * (size)) + (aRect.width / 2) + aRect.x);
    defIndex++;
    checkedPolygon[polyIndex].y =
      nscoord((((checkedPolygonDef[defIndex]) - centery) * (size)) + (aRect.height / 2) + aRect.y);
    polyIndex++;
  }
  
  aRenderingContext.FillPolygon(checkedPolygon, checkpoints);
}

//------------------------------------------------------------
nsIFrame*
NS_NewGfxCheckboxControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsGfxCheckboxControlFrame(aContext);
}


//------------------------------------------------------------
// Initialize GFX-rendered state
nsGfxCheckboxControlFrame::nsGfxCheckboxControlFrame(nsStyleContext* aContext)
: nsFormControlFrame(aContext), mCheckButtonFaceStyle(nsnull)
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

static void PaintCheckMarkFromStyle(nsIFrame* aFrame,
     nsIRenderingContext* aCtx, const nsRect& aDirtyRect, nsPoint aPt) {
  NS_STATIC_CAST(nsGfxCheckboxControlFrame*, aFrame)
    ->PaintCheckBoxFromStyle(*aCtx, aPt, aDirtyRect);
}

class nsDisplayCheckMark : public nsDisplayItem {
public:
  nsDisplayCheckMark(nsGfxCheckboxControlFrame* aFrame)
    : nsDisplayItem(aFrame) {
    MOZ_COUNT_CTOR(nsDisplayCheckMark);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayCheckMark() {
    MOZ_COUNT_DTOR(nsDisplayCheckMark);
  }
#endif

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect);
  NS_DISPLAY_DECL_NAME("CheckMark")
};

void
nsDisplayCheckMark::Paint(nsDisplayListBuilder* aBuilder,
     nsIRenderingContext* aCtx, const nsRect& aDirtyRect) {
  NS_STATIC_CAST(nsGfxCheckboxControlFrame*, mFrame)->
    PaintCheckBox(*aCtx, aBuilder->ToReferenceFrame(mFrame), aDirtyRect);
}


//------------------------------------------------------------
void
nsGfxCheckboxControlFrame::PaintCheckBox(nsIRenderingContext& aRenderingContext,
                                         nsPoint aPt,
                                         const nsRect& aDirtyRect)
{
  // REVIEW: moved the mAppearance test out so we avoid constructing
  // a display item if it's not needed
  nsMargin borderPadding(0,0,0,0);
  CalcBorderPadding(borderPadding);

  nsRect checkRect(aPt, mRect.Size());
  checkRect.Deflate(borderPadding);

  const nsStyleColor* color = GetStyleColor();
  aRenderingContext.SetColor(color->mColor);

  ::PaintCheckMark(aRenderingContext,
                   GetPresContext()->ScaledPixelsToTwips(),
                   checkRect);
}

//------------------------------------------------------------
NS_IMETHODIMP
nsGfxCheckboxControlFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                            const nsRect&           aDirtyRect,
                                            const nsDisplayListSet& aLists)
{
  nsresult rv = nsFormControlFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // Get current checked state through content model.
  if (!GetCheckboxState() || !IsVisibleForPainting(aBuilder))
    return NS_OK;   // we're not checked or not visible, nothing to paint.
    
  if (IsThemed())
    return NS_OK; // No need to paint the checkmark. The theme will do it.

  // Paint the checkmark
  // REVIEW: this used to check !mCheckButtonFaceStyle, but that's clearly
  // wrong ... look at the blame diff, it was reversed by accident
  if (mCheckButtonFaceStyle) {
    const nsStyleBackground* myColor = mCheckButtonFaceStyle->GetStyleBackground();
    // REVIEW: I'm not sure if this is really sane. We should at least be also
    // checking for other properties too. But this is what we had.
    if (myColor->mBackgroundImage)
      return aLists.Content()->AppendNewToTop(new (aBuilder)
          nsDisplayGeneric(this, PaintCheckMarkFromStyle, "CheckMarkFromStyle"));
  }

  return aLists.Content()->AppendNewToTop(new (aBuilder) nsDisplayCheckMark(this));
  // REVIEW: I'm not really sure why we were painting the outline again
  // here ... the superclass call at the top should have painted it already.
  // Just a bug I guess.
}

void
nsGfxCheckboxControlFrame::PaintCheckBoxFromStyle(
    nsIRenderingContext& aRenderingContext, nsPoint aPt, const nsRect& aDirtyRect) {
  const nsStylePadding* myPadding = mCheckButtonFaceStyle->GetStylePadding();
  const nsStylePosition* myPosition = mCheckButtonFaceStyle->GetStylePosition();
  const nsStyleBorder* myBorder = mCheckButtonFaceStyle->GetStyleBorder();

  nscoord width = myPosition->mWidth.GetCoordValue();
  nscoord height = myPosition->mHeight.GetCoordValue();
  // Position the button centered within the radio control's rectangle.
  nscoord x = (mRect.width - width) / 2;
  nscoord y = (mRect.height - height) / 2;
  nsRect rect(aPt.x + x, aPt.y + y, width, height);

  nsCSSRendering::PaintBackground(GetPresContext(), aRenderingContext, this,
                                  aDirtyRect, rect, *myBorder, *myPadding,
                                  PR_FALSE);
  nsCSSRendering::PaintBorder(GetPresContext(), aRenderingContext, this,
                              aDirtyRect, rect, *myBorder, mCheckButtonFaceStyle, 0);
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

