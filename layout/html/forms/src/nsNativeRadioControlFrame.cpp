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

#include "nsNativeRadioControlFrame.h"
#include "nsIRadioButton.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLParts.h"
#include "nsFormFrame.h"
#include "nsIFormControl.h"
#include "nsIContent.h"
#include "nsILookAndFeel.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"
#include "nsStyleUtil.h"
#include "nsCOMPtr.h"


static NS_DEFINE_IID(kIRadioIID, NS_IRADIOBUTTON_IID);

#define NS_DEFAULT_RADIOBOX_SIZE  12

nsresult
NS_NewNativeRadioControlFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsNativeRadioControlFrame* it = new nsNativeRadioControlFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

//--------------------------------------------------------------


nscoord 
nsNativeRadioControlFrame::GetRadioboxSize(nsIPresContext* aPresContext, float aPixToTwip) const
{
  PRInt32 radioboxSize = 0;
  nsCOMPtr<nsILookAndFeel> lookAndFeel;
  if (NS_SUCCEEDED(aPresContext->GetLookAndFeel(getter_AddRefs(lookAndFeel)))) {
   lookAndFeel->GetMetric(nsILookAndFeel::eMetric_RadioboxSize,  radioboxSize);
  }
 if (radioboxSize == 0)
   radioboxSize = NS_DEFAULT_RADIOBOX_SIZE;
  return NSIntPixelsToTwips(radioboxSize, aPixToTwip);
}

void 
nsNativeRadioControlFrame::GetDesiredSize(nsIPresContext*        aPresContext,
                                          const nsHTMLReflowState& aReflowState,
                                          nsHTMLReflowMetrics&     aDesiredLayoutSize,
                                          nsSize&                  aDesiredWidgetSize)
{

  nsWidgetRendering mode;
  aPresContext->GetWidgetRenderingMode(&mode);
  if ((eWidgetRendering_Gfx == mode) || (eWidgetRendering_PartialGfx == mode)) {
    Inherited::GetDesiredSize(aPresContext,aReflowState,aDesiredLayoutSize,
                                    aDesiredWidgetSize);
  } else {
    float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);
    aDesiredWidgetSize.width  = GetRadioboxSize(aPresContext, p2t);
    aDesiredWidgetSize.height = aDesiredWidgetSize.width;

    aDesiredLayoutSize.width  = aDesiredWidgetSize.width;
    aDesiredLayoutSize.height = aDesiredWidgetSize.height;
    aDesiredLayoutSize.ascent = aDesiredLayoutSize.height;
    aDesiredLayoutSize.descent = 0;
    if (aDesiredLayoutSize.maxElementSize) {
      aDesiredLayoutSize.maxElementSize->width  = aDesiredLayoutSize.width;
      aDesiredLayoutSize.maxElementSize->height = aDesiredLayoutSize.height;
    }
  }
}

void 
nsNativeRadioControlFrame::PostCreateWidget(nsIPresContext* aPresContext, nscoord& aWidth, nscoord& aHeight)
{
	Inherited::PostCreateWidget(aPresContext, aWidth, aHeight);

  if (mWidget != nsnull) {
    const nsStyleColor* color =
    nsStyleUtil::FindNonTransparentBackground(mStyleContext);

    if (nsnull != color) {
      mWidget->SetBackgroundColor(color->mBackgroundColor);
    } else {
      mWidget->SetBackgroundColor(NS_RGB(0xFF, 0xFF, 0xFF));
    }
    mWidget->Enable(!nsFormFrame::GetDisabled(this));
  }           

}


NS_IMETHODIMP
nsNativeRadioControlFrame::AttributeChanged(nsIPresContext* aPresContext,
                                      nsIContent*     aChild,
                                      PRInt32         aNameSpaceID,
                                      nsIAtom*        aAttribute,
                                      PRInt32         aHint)
{
  nsresult result = NS_OK;
  if (mWidget) {
    if (nsHTMLAtoms::checked == aAttribute) {
      nsIRadioButton* button = nsnull;
      result = mWidget->QueryInterface(kIRadioIID, (void**)&button);
      if ((NS_SUCCEEDED(result)) && (nsnull != button)) {
        PRBool checkedAttr = PR_TRUE;
        GetCurrentCheckState(&checkedAttr);
        PRBool checkedPrevState = PR_TRUE;
        button->GetState(checkedPrevState);
        if (checkedAttr != checkedPrevState) {
          button->SetState(checkedAttr);
          mFormFrame->OnRadioChecked(aPresContext, *this, checkedAttr);
        }
        NS_RELEASE(button);
      }
    }   
    // Allow the base class to handle common attributes supported
    // by all form elements... 
    else {
      result = Inherited::AttributeChanged(aPresContext, aChild, aNameSpaceID, aAttribute, aHint);
    }
  }
  return result;
}

PRBool nsNativeRadioControlFrame::GetRadioState()
{
	PRBool state = PR_FALSE;

  if (nsnull != mWidget) {
	  nsIRadioButton* radio = nsnull;
    if (NS_SUCCEEDED(mWidget->QueryInterface(kIRadioIID,(void**)&radio))) {
      radio->GetState(state);
  		NS_RELEASE(radio);
    }
  }
  return state;
}


void nsNativeRadioControlFrame::SetRadioState(nsIPresContext* aPresContext, PRBool aValue)
{
  if (nsnull != mWidget) {
    nsIRadioButton* radio = nsnull;
    if (NS_OK == mWidget->QueryInterface(kIRadioIID,(void**)&radio)) {
      radio->SetState(aValue);
      NS_RELEASE(radio);
    }
  }
}
