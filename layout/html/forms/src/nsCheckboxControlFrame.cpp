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
#include "nsFormControlHelper.h"
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
#include "nsIDOMHTMLInputElement.h"


#define NS_DESIRED_CHECKBOX_SIZE 20
#define NS_ABSOLUTE_CHECKBOX_SIZE 12


static NS_DEFINE_IID(kICheckButtonIID, NS_ICHECKBUTTON_IID);
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);

class nsCheckboxControlFrame : public nsFormControlFrame {
public:
  nsCheckboxControlFrame();
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

    // nsIFormControLFrame
  NS_IMETHOD SetProperty(nsIAtom* aName, const nsString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsString& aValue); 

  //
  // Methods used to GFX-render the checkbox
  // 

  virtual void PaintCheckBox(nsIPresContext& aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect& aDirtyRect);

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus& aEventStatus);

  //End of GFX-rendering methods
  
protected:
  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize);
  PRBool mMouseDownOnCheckbox;
};

nsresult NS_NewCheckboxControlFrame(nsIFrame*& aResult);
nsresult
NS_NewCheckboxControlFrame(nsIFrame*& aResult)
{
  aResult = new nsCheckboxControlFrame;
  if (nsnull == aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsCheckboxControlFrame::nsCheckboxControlFrame()
{
  mMouseDownOnCheckbox = PR_FALSE;
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
   aDesiredWidgetSize.width  = NSIntPixelsToTwips(NS_DESIRED_CHECKBOX_SIZE, p2t);
   aDesiredWidgetSize.height = NSIntPixelsToTwips(NS_DESIRED_CHECKBOX_SIZE, p2t);
#endif
  aDesiredLayoutSize.width  = aDesiredWidgetSize.width;
  aDesiredLayoutSize.height = aDesiredWidgetSize.height;
  aDesiredLayoutSize.ascent = aDesiredLayoutSize.height;
  aDesiredLayoutSize.descent = 0;
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
    nsresult result = GetCurrentCheckState(&checked);
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
      result = nsFormControlFrame::AttributeChanged(aPresContext, aChild, aAttribute, aHint);
    }
  }
  return result;
}

void 
nsCheckboxControlFrame::MouseClicked(nsIPresContext* aPresContext) 
{
  mMouseDownOnCheckbox = PR_FALSE;
  PRBool oldState;
  GetCurrentCheckState(&oldState);
  PRBool newState = oldState ? PR_FALSE : PR_TRUE;
  SetCurrentCheckState(newState); 
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
  PRBool checked;
  GetDefaultCheckState(&checked);
  SetCurrentCheckState(checked);
}  

void
nsCheckboxControlFrame::PaintCheckBox(nsIPresContext& aPresContext,
                  nsIRenderingContext& aRenderingContext,
                  const nsRect& aDirtyRect)
{
  //XXX: Resolution of styles should be cached. This is very
  // inefficient to do this each time the checkbox is cliced on.
  /**
   * Resolve style for a pseudo frame within the given aParentContent & aParentContext.
   * The tag should be uppercase and inclue the colon.
   * ie: NS_NewAtom(":FIRST-LINE");
   */
  nsIStyleContext* style = nsnull;
  if (PR_TRUE == mMouseDownOnCheckbox) {
    nsIAtom * sbAtom = NS_NewAtom(":CHECKBOX-LOOK");
    style = aPresContext.ResolvePseudoStyleContextFor(mContent, sbAtom, mStyleContext);
    NS_RELEASE(sbAtom);
  }
  else {
    nsIAtom * sbAtom = NS_NewAtom(":CHECKBOX-SELECT-LOOK");
    style = aPresContext.ResolvePseudoStyleContextFor(mContent, sbAtom, mStyleContext);
    NS_RELEASE(sbAtom);
  }

  const nsStyleColor* color = (const nsStyleColor*)style->GetStyleData(eStyleStruct_Color);

  aRenderingContext.PushState();
 
  float p2t;
  aPresContext.GetScaledPixelsToTwips(p2t);
 
    //Offset fixed size checkbox in to the middle of the area reserved for the checkbox
  const int printOffsetX = (NS_DESIRED_CHECKBOX_SIZE - NS_ABSOLUTE_CHECKBOX_SIZE);
  const int printOffsetY = (NS_DESIRED_CHECKBOX_SIZE - NS_ABSOLUTE_CHECKBOX_SIZE);
  aRenderingContext.Translate(NSIntPixelsToTwips(printOffsetX, p2t), 
                              NSIntPixelsToTwips(printOffsetY, p2t));

    // Draw's background + border
  nsFormControlHelper::PaintFixedSizeCheckMarkBorder(aRenderingContext, p2t, *color);
 
  PRBool checked = PR_TRUE;

    // Get current checked state from content model
  nsresult result = GetCurrentCheckState(&checked);
  if (PR_TRUE == checked) {
    nsFormControlHelper::PaintFixedSizeCheckMark(aRenderingContext, p2t);

  }
  PRBool clip;
  aRenderingContext.PopState(clip);
  NS_IF_RELEASE(style);
}


NS_METHOD 
nsCheckboxControlFrame::Paint(nsIPresContext& aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              const nsRect& aDirtyRect,
                              nsFramePaintLayer aWhichLayer)
{
  if (eFramePaintLayer_Content == aWhichLayer) {
    PaintCheckBox(aPresContext, aRenderingContext, aDirtyRect);
  }
  return NS_OK;
}

NS_METHOD nsCheckboxControlFrame::HandleEvent(nsIPresContext& aPresContext, 
                                          nsGUIEvent* aEvent,
                                          nsEventStatus& aEventStatus)
{
  if (nsEventStatus_eConsumeNoDefault == aEventStatus) {
    return NS_OK;
  }

  if (nsnull == mWidget) {
      // Handle GFX rendered widget Mouse Down event
    PRInt32 type;
    PRBool checked;
    GetType(&type);
    switch (aEvent->message) {
      case NS_MOUSE_LEFT_BUTTON_DOWN:
         mMouseDownOnCheckbox = PR_TRUE;
        // XXX: Hack, force refresh by changing attribute to current
        GetCurrentCheckState(&checked);
        SetCurrentCheckState(checked);
     
      break;

      case NS_MOUSE_EXIT:
        mMouseDownOnCheckbox = PR_FALSE;
        // XXX: Hack, force refresh by changing attribute to current
        GetCurrentCheckState(&checked);
        SetCurrentCheckState(checked);
      break;

    }
  }

  return(nsFormControlFrame::HandleEvent(aPresContext, aEvent, aEventStatus));
}

NS_IMETHODIMP nsCheckboxControlFrame::SetProperty(nsIAtom* aName, const nsString& aValue)
{
  return NS_OK;
}

NS_IMETHODIMP nsCheckboxControlFrame::GetProperty(nsIAtom* aName, nsString& aValue)
{
  return NS_OK;
}




