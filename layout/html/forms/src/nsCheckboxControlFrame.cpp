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

#include "nsICheckButton.h"
#include "nsFormControlFrame.h"
#include "nsFormFrame.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIFrame.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsWidgetsCID.h"
#include "nsIView.h"
#include "nsHTMLAtoms.h"
#include "nsIStyleContext.h"
#include "nsStyleUtil.h"
#include "nsIFormControl.h"

static NS_DEFINE_IID(kICheckButtonIID, NS_ICHECKBUTTON_IID);
//static NS_DEFINE_IID(kIHTMLContentIID, NS_IHTMLCONTENT_IID);

class nsCheckboxControlFrame : public nsFormControlFrame {
public:
  virtual void PostCreateWidget(nsIPresContext* aPresContext,
                                nscoord& aWidth,
                                nscoord& aHeight);

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                              nsIContent*     aChild,
                              nsIAtom*        aAttribute,
                              PRInt32         aHint);

  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("CheckboxControl", aResult);
  }

  virtual const nsIID& GetCID();

  virtual const nsIID& GetIID();

  virtual void MouseClicked(nsIPresContext* aPresContext);

  virtual PRInt32 GetMaxNumValues();

  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames);

  virtual void Reset();

  NS_IMETHOD GetChecked(PRBool* aResult);

protected:
  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize);
};

nsresult
NS_NewCheckboxControlFrame(nsIFrame*& aResult)
{
  aResult = new nsCheckboxControlFrame;
  if (nsnull == aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

const nsIID&
nsCheckboxControlFrame::GetIID()
{
  static NS_DEFINE_IID(kCheckboxIID, NS_ICHECKBUTTON_IID);
  return kCheckboxIID;
}
  
const nsIID&
nsCheckboxControlFrame::GetCID()
{
  static NS_DEFINE_IID(kCheckboxCID, NS_CHECKBUTTON_CID);
  return kCheckboxCID;
}

void
nsCheckboxControlFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                            const nsHTMLReflowState& aReflowState,
                                            nsHTMLReflowMetrics& aDesiredLayoutSize,
                                            nsSize& aDesiredWidgetSize)
{
  float p2t;
  aPresContext->GetScaledPixelsToTwips(p2t);
#ifdef XP_PC
  aDesiredWidgetSize.width  = NSIntPixelsToTwips(12, p2t);
  aDesiredWidgetSize.height = NSIntPixelsToTwips(12, p2t);
#endif
#ifdef XP_PC
  aDesiredWidgetSize.width  = NSIntPixelsToTwips(20, p2t);
  aDesiredWidgetSize.height = NSIntPixelsToTwips(20, p2t);
#endif
  aDesiredLayoutSize.width  = aDesiredWidgetSize.width;
  aDesiredLayoutSize.height = aDesiredWidgetSize.height;
  aDesiredLayoutSize.ascent = aDesiredLayoutSize.height;
  aDesiredLayoutSize.descent = 0;
}

NS_IMETHODIMP
nsCheckboxControlFrame::GetChecked(PRBool* aResult)
{
  nsresult result = NS_FORM_NOTOK;
  *aResult = PR_FALSE;
  if (mContent) {
    nsIHTMLContent* iContent = nsnull;
    result = mContent->QueryInterface(kIHTMLContentIID, (void**)&iContent);
    if ((NS_OK == result) && iContent) {
      nsHTMLValue value;
      result = iContent->GetAttribute(nsHTMLAtoms::checked, value);
      if (NS_CONTENT_ATTR_HAS_VALUE == result) {
        *aResult = PR_TRUE;
      }
      NS_RELEASE(iContent);
    }
  }
  return result;
}

void 
nsCheckboxControlFrame::PostCreateWidget(nsIPresContext* aPresContext, nscoord& aWidth, nscoord& aHeight)
{
  if (!mWidget) {
    return;
  }

  SetColors(*aPresContext);
  mWidget->Enable(!nsFormFrame::GetDisabled(this));

  // set the widget to the initial state
  nsICheckButton* checkbox = nsnull;
  if (NS_OK == mWidget->QueryInterface(GetIID(),(void**)&checkbox)) {
    PRBool checked;
    nsresult result = GetChecked(&checked);
    if (NS_CONTENT_ATTR_HAS_VALUE == result) {
	    checkbox->SetState(checked);
    }
    NS_RELEASE(checkbox);
  }
}

NS_IMETHODIMP
nsCheckboxControlFrame::AttributeChanged(nsIPresContext* aPresContext,
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
        GetChecked(&checkedAttr);
        PRBool checkedPrevState;
        button->GetState(checkedPrevState);
        if (checkedAttr != checkedPrevState) {
          button->SetState(checkedAttr);
        }
        NS_RELEASE(button);
      }
    }   
  }
  return result;
}

void 
nsCheckboxControlFrame::MouseClicked(nsIPresContext* aPresContext) 
{
  nsICheckButton* checkbox = nsnull;
  if (mWidget && (NS_OK == mWidget->QueryInterface(GetIID(),(void**)&checkbox))) {
  	PRBool oldState;
  	checkbox->GetState(oldState);
		PRBool newState = oldState ? PR_FALSE : PR_TRUE;
		checkbox->SetState(newState);
    NS_IF_RELEASE(checkbox);
  }
}

PRInt32 
nsCheckboxControlFrame::GetMaxNumValues()
{
  return 1;
}
  

PRBool
nsCheckboxControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                       nsString* aValues, nsString* aNames)
{
  nsAutoString name;
  nsresult nameResult = GetName(&name);
  if ((aMaxNumValues <= 0) || (NS_CONTENT_ATTR_HAS_VALUE != nameResult)) {
    return PR_FALSE;
  }

  PRBool result = PR_TRUE;

  nsAutoString value;
  nsresult valueResult = GetValue(&value);

  nsICheckButton* checkBox = nsnull;
  if ((nsnull != mWidget) && 
      (NS_OK == mWidget->QueryInterface(kICheckButtonIID,(void**)&checkBox))) {
    PRBool state = PR_FALSE;
    checkBox->GetState(state);
    if (PR_TRUE != state) {
      result = PR_FALSE;
    } else {
      if (NS_CONTENT_ATTR_HAS_VALUE == valueResult) {
        aValues[0] = "on";
      } else {
        aValues[0] = value;
      }
      aNames[0] = name;
      aNumValues = 1;
    }
    NS_RELEASE(checkBox);
  }

  return result;
}

void 
nsCheckboxControlFrame::Reset() 
{ 
  nsICheckButton*  checkBox = nsnull;
  if ((mWidget != nsnull) && 
      (NS_OK == mWidget->QueryInterface(kICheckButtonIID,(void**)&checkBox))) {
    PRBool checked;
    GetChecked(&checked);
    checkBox->SetState(checked);
    NS_RELEASE(checkBox);
  }
}  

