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

#include "nsGfxRadioControlFrame.h"
#include "nsIContent.h"
#include "nsCOMPtr.h"
#include "nsCSSRendering.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif
#include "nsIServiceManager.h"
#include "nsITheme.h"
#include "nsDisplayList.h"

nsIFrame*
NS_NewGfxRadioControlFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsGfxRadioControlFrame(aContext);
}

nsGfxRadioControlFrame::nsGfxRadioControlFrame(nsStyleContext* aContext):
  nsFormControlFrame(aContext)
{
}

nsGfxRadioControlFrame::~nsGfxRadioControlFrame()
{
}

// Frames are not refcounted, no need to AddRef
NS_IMETHODIMP
nsGfxRadioControlFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsIRadioControlFrame))) {
    *aInstancePtr = (void*) ((nsIRadioControlFrame*) this);
    return NS_OK;
  }

  return nsFormControlFrame::QueryInterface(aIID, aInstancePtr);
}

#ifdef ACCESSIBILITY
NS_IMETHODIMP nsGfxRadioControlFrame::GetAccessible(nsIAccessible** aAccessible)
{
  nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");

  if (accService) {
    return accService->CreateHTMLRadioButtonAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
  }

  return NS_ERROR_FAILURE;
}
#endif

//--------------------------------------------------------------
nsStyleContext*
nsGfxRadioControlFrame::GetAdditionalStyleContext(PRInt32 aIndex) const
{
  switch (aIndex) {
  case NS_GFX_RADIO_CONTROL_FRAME_FACE_CONTEXT_INDEX:
    return mRadioButtonFaceStyle;
    break;
  default:
    return nsnull;
  }
}

//--------------------------------------------------------------
void
nsGfxRadioControlFrame::SetAdditionalStyleContext(PRInt32 aIndex, 
                                                  nsStyleContext* aStyleContext)
{
  switch (aIndex) {
  case NS_GFX_RADIO_CONTROL_FRAME_FACE_CONTEXT_INDEX:
    mRadioButtonFaceStyle = aStyleContext;
    break;
  }
}

//--------------------------------------------------------------
NS_IMETHODIMP
nsGfxRadioControlFrame::SetRadioButtonFaceStyleContext(nsStyleContext *aRadioButtonFaceStyleContext)
{
  mRadioButtonFaceStyle = aRadioButtonFaceStyleContext;
  return NS_OK;
}

//--------------------------------------------------------------
void
nsGfxRadioControlFrame::PaintRadioButtonFromStyle(
    nsIRenderingContext& aRenderingContext, nsPoint aPt, const nsRect& aDirtyRect)
{
  const nsStyleBorder* myBorder = mRadioButtonFaceStyle->GetStyleBorder();
  // Paint the button for the radio button using CSS background rendering code
  const nsStyleBackground* myColor = mRadioButtonFaceStyle->GetStyleBackground();
  const nsStyleColor* color = mRadioButtonFaceStyle->GetStyleColor();
  const nsStylePadding* myPadding = mRadioButtonFaceStyle->GetStylePadding();
  const nsStylePosition* myPosition = mRadioButtonFaceStyle->GetStylePosition();

  nscoord width = myPosition->mWidth.GetCoordValue();
  nscoord height = myPosition->mHeight.GetCoordValue();
  // Position the button centered within the radio control's rectangle.
  nscoord x = (mRect.width - width) / 2;
  nscoord y = (mRect.height - height) / 2;
  nsRect rect = nsRect(x, y, width, height) + aPt;

  // So we will use PaintBackgroundWithSC to paint the dot, 
  // but it uses the mBackgroundColor for painting and we need to use the mColor
  // so create a temporary style color struct and set it up appropriately
  // XXXldb It would make more sense to use
  // |aRenderingContext.FillEllipse| here, but on at least GTK that
  // doesn't draw a round enough circle.
  nsStyleBackground tmpColor     = *myColor;
  tmpColor.mBackgroundColor = color->mColor;
  nsPresContext* pc = GetPresContext();
  nsCSSRendering::PaintBackgroundWithSC(pc, aRenderingContext,
                                        this, aDirtyRect, rect,
                                        tmpColor, *myBorder, *myPadding, PR_FALSE);
  nsCSSRendering::PaintBorder(pc, aRenderingContext, this,
                              aDirtyRect, rect, *myBorder, mRadioButtonFaceStyle, 0);
}

class nsDisplayRadioButtonFromStyle : public nsDisplayItem {
public:
  nsDisplayRadioButtonFromStyle(nsGfxRadioControlFrame* aFrame)
    : nsDisplayItem(aFrame) {
    MOZ_COUNT_CTOR(nsDisplayRadioButtonFromStyle);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayRadioButtonFromStyle() {
    MOZ_COUNT_DTOR(nsDisplayRadioButtonFromStyle);
  }
#endif
  
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect);
  NS_DISPLAY_DECL_NAME("RadioButton")
};

void
nsDisplayRadioButtonFromStyle::Paint(nsDisplayListBuilder* aBuilder,
     nsIRenderingContext* aCtx, const nsRect& aDirtyRect) {
  NS_STATIC_CAST(nsGfxRadioControlFrame*, mFrame)->
    PaintRadioButtonFromStyle(*aCtx, aBuilder->ToReferenceFrame(mFrame), aDirtyRect);
}

//--------------------------------------------------------------
NS_IMETHODIMP
nsGfxRadioControlFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                         const nsRect&           aDirtyRect,
                                         const nsDisplayListSet& aLists)
{
  nsresult rv = nsFormControlFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (!IsVisibleForPainting(aBuilder))
    return NS_OK;
  
  if (IsThemed())
    return NS_OK; // No need to paint the radio button. The theme will do it.

  if (!mRadioButtonFaceStyle)
    return NS_OK;
  
  PRBool checked = PR_TRUE;
  GetCurrentCheckState(&checked); // Get check state from the content model
  if (!checked)
    return NS_OK;
    
  return aLists.Content()->AppendNewToTop(new (aBuilder)
      nsDisplayRadioButtonFromStyle(this));
}


//--------------------------------------------------------------
NS_IMETHODIMP
nsGfxRadioControlFrame::OnChecked(nsPresContext* aPresContext,
                                  PRBool aChecked)
{
  Invalidate(GetOverflowRect(), PR_FALSE);
  return NS_OK;
}

