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


#define NS_DEFAULT_CHECKBOX_SIZE 12

static NS_DEFINE_IID(kICheckButtonIID, NS_ICHECKBUTTON_IID);
static NS_DEFINE_IID(kLookAndFeelCID,  NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);


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
nsNativeCheckboxControlFrame::GetCheckboxSize(float aPixToTwip) const
{
  nsILookAndFeel * lookAndFeel;
  PRInt32 checkboxSize = 0;
  if (NS_OK == nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull, kILookAndFeelIID, (void**)&lookAndFeel)) {
   lookAndFeel->GetMetric(nsILookAndFeel::eMetric_CheckboxSize,  checkboxSize);
   NS_RELEASE(lookAndFeel);
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

  aDesiredWidgetSize.width  = GetCheckboxSize(p2t);
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
    SetColors(*aPresContext);
    mWidget->Enable(!nsFormFrame::GetDisabled(this));
  }              
}

NS_IMETHODIMP
nsNativeCheckboxControlFrame::AttributeChanged(nsIPresContext* aPresContext,
                                         nsIContent*     aChild,
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
      result = Inherited::AttributeChanged(aPresContext, aChild, aAttribute, aHint);
    }
  }
  return result;
}

PRBool nsNativeCheckboxControlFrame::GetCheckboxState()
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
  return state;
}


void nsNativeCheckboxControlFrame::SetCheckboxState(PRBool aValue)
{
  if (nsnull != mWidget) {
    nsICheckButton* checkBox = nsnull;
    if (NS_OK == mWidget->QueryInterface(kICheckButtonIID,(void**)&checkBox)) {
      checkBox->SetState(aValue);
      NS_RELEASE(checkBox);
    }
  }
}
