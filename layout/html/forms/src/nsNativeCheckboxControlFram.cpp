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

#include "nsNativeCheckboxControlFram.h"
#include "nsICheckButton.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLParts.h"
#include "nsFormFrame.h"
#include "nsIFormControl.h"
#include "nsIContent.h"
#include "nsILookAndFeel.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"


#define NS_DEFAULT_CHECKBOX_SIZE 12

static NS_DEFINE_IID(kICheckButtonIID, NS_ICHECKBUTTON_IID);


nsresult
NS_NewNativeCheckboxControlFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsNativeCheckboxControlFrame* it = new nsNativeCheckboxControlFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}


nscoord 
nsNativeCheckboxControlFrame::GetCheckboxSize(nsIPresContext* aPresContext, float aPixToTwip) const
{
  PRInt32 checkboxSize = 0;
  nsCOMPtr<nsILookAndFeel> lookAndFeel;
  if (NS_SUCCEEDED(aPresContext->GetLookAndFeel(getter_AddRefs(lookAndFeel)))) {
   lookAndFeel->GetMetric(nsILookAndFeel::eMetric_CheckboxSize,  checkboxSize);
  }
 if (checkboxSize == 0)
   checkboxSize = NS_DEFAULT_CHECKBOX_SIZE;
  return NSIntPixelsToTwips(checkboxSize, aPixToTwip);
}

void
nsNativeCheckboxControlFrame::GetDesiredSize(nsIPresContext*          aPresContext,
                                       const nsHTMLReflowState& aReflowState,
                                       nsHTMLReflowMetrics&     aDesiredLayoutSize,
                                       nsSize&                  aDesiredWidgetSize)
{
  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);

  aDesiredWidgetSize.width  = GetCheckboxSize(aPresContext, p2t);
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


void 
nsNativeCheckboxControlFrame::PostCreateWidget(nsIPresContext* aPresContext, nscoord& aWidth, nscoord& aHeight)
{
	Inherited::PostCreateWidget(aPresContext, aWidth, aHeight);

  if (mWidget != nsnull) {
    SetColors(aPresContext);
    mWidget->Enable(!nsFormFrame::GetDisabled(this));
  }              
}

NS_IMETHODIMP
nsNativeCheckboxControlFrame::AttributeChanged(nsIPresContext* aPresContext,
                                         nsIContent*     aChild,
                                         PRInt32         aNameSpaceID,
                                         nsIAtom*        aAttribute,
                                         PRInt32         aHint)
{
  nsresult result = NS_OK;
  if (mWidget) {
    if (nsHTMLAtoms::checked == aAttribute) {
      nsICheckButton* button = nsnull;
      result = mWidget->QueryInterface(GetIID(), (void**)&button);
      if ((NS_SUCCEEDED(result)) && (nsnull != button)) {
        PRBool checkedAttr;
        GetCurrentCheckState(&checkedAttr);
        PRBool checkedPrevState;
        button->GetState(checkedPrevState);
        if (checkedAttr != checkedPrevState) {
          button->SetState(checkedAttr);
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

nsCheckboxControlFrame::CheckState
nsNativeCheckboxControlFrame::GetCheckboxState()
{
	PRBool state = PR_FALSE;

  nsICheckButton* checkBox = nsnull;
  if (nsnull != mWidget) {
     // native-widget
      if (NS_SUCCEEDED(mWidget->QueryInterface(kICheckButtonIID,(void**)&checkBox))) {
        checkBox->GetState(state);
       NS_RELEASE(checkBox);
      }
  }
  return state ? eOn : eOff;
}


void nsNativeCheckboxControlFrame::SetCheckboxState(nsIPresContext* aPresContext, CheckState aValue)
{
  if (nsnull != mWidget) {
    nsICheckButton* checkBox = nsnull;
    if (NS_OK == mWidget->QueryInterface(kICheckButtonIID,(void**)&checkBox)) {
      checkBox->SetState(aValue == eOn);
      NS_RELEASE(checkBox);
    }
  }
}
