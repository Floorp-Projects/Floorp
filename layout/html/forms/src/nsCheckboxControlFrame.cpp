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
#include "nsCSSRendering.h"
#include "nsILookAndFeel.h"
#include "nsIComponentManager.h"


#define NS_DEFAULT_CHECKBOX_SIZE 12

static NS_DEFINE_IID(kICheckButtonIID, NS_ICHECKBUTTON_IID);
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);
static NS_DEFINE_IID(kLookAndFeelCID,  NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);

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

    // nsIFormControlFrame
  NS_IMETHOD SetProperty(nsIAtom* aName, const nsString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsString& aValue); 

   // Utility methods for implementing SetProperty/GetProperty
  void SetCheckboxControlFrameState(const nsString& aValue);
  void GetCheckboxControlFrameState(nsString& aValue);  

   // nsFormControlFrame overrides
  nsresult RequiresWidget(PRBool &aHasWidget);

  //
  // Methods used to GFX-render the checkbox
  // 

  virtual void PaintCheckBox(nsIPresContext& aPresContext,
                             nsIRenderingContext& aRenderingContext,
                             const nsRect& aDirtyRect,
                             nsFramePaintLayer aWhichLayer);

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                         nsGUIEvent* aEvent,
                         nsEventStatus& aEventStatus);

  //End of GFX-rendering methods
  
protected:
  virtual nscoord GetCheckboxSize(float aPixToTwip) const;
  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize);

    //GFX-rendered state variables
  PRBool mMouseDownOnCheckbox;
  PRBool mChecked;
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
   // Initialize GFX-rendered state
  mMouseDownOnCheckbox = PR_FALSE;
  mChecked = PR_FALSE;
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

nscoord 
nsCheckboxControlFrame::GetCheckboxSize(float aPixToTwip) const
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
nsCheckboxControlFrame::GetDesiredSize(nsIPresContext*          aPresContext,
                                       const nsHTMLReflowState& aReflowState,
                                       nsHTMLReflowMetrics&     aDesiredLayoutSize,
                                       nsSize&                  aDesiredWidgetSize)
{
  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);

  nsWidgetRendering mode;
  aPresContext->GetWidgetRenderingMode(&mode);
  if (eWidgetRendering_Gfx == mode) {
    nsFormControlFrame::GetDesiredSize(aPresContext, aReflowState, 
    aDesiredLayoutSize, aDesiredWidgetSize);
  } else {
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
}


void 
nsCheckboxControlFrame::PostCreateWidget(nsIPresContext* aPresContext, nscoord& aWidth, nscoord& aHeight)
{
   // set the widget to the initial state
  PRBool checked = PR_FALSE;
  nsresult result = GetDefaultCheckState(&checked);
  if (NS_CONTENT_ATTR_HAS_VALUE == result) {
    if (PR_TRUE == checked)
      SetProperty(nsHTMLAtoms::checked, NS_STRING_TRUE);
    else
      SetProperty(nsHTMLAtoms::checked, NS_STRING_FALSE);
  }

  if (mWidget != nsnull) {
    SetColors(*aPresContext);
    mWidget->Enable(!nsFormFrame::GetDisabled(this));
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
      if (NS_CONTENT_ATTR_HAS_VALUE != valueResult) {
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
                  const nsRect& aDirtyRect,
                  nsFramePaintLayer aWhichLayer)
{
  aRenderingContext.PushState();

  float p2t;
  aPresContext.GetScaledPixelsToTwips(&p2t);

  PRBool checked = PR_FALSE;

    // Get current checked state through content model.
    // XXX: This is very inefficient, but it is necessary in the case of printing.
    // During printing the Paint is called but the actual state of the checkbox
    // is in a frame in presentation shell 0.
  /*XXXnsresult result = */GetCurrentCheckState(&checked);
  if (PR_TRUE == checked) {
      // Draw check mark
    const nsStyleColor* color = (const nsStyleColor*)
      mStyleContext->GetStyleData(eStyleStruct_Color);
    aRenderingContext.SetColor(color->mColor);
    nsFormControlHelper::PaintCheckMark(aRenderingContext,
                         p2t, mRect.width, mRect.height);
   
  }
  PRBool clip;
  aRenderingContext.PopState(clip);
}


NS_METHOD 
nsCheckboxControlFrame::Paint(nsIPresContext& aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              const nsRect& aDirtyRect,
                              nsFramePaintLayer aWhichLayer)
{
    // Paint the background
  nsFormControlFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
      // Paint the checkmark 
    PaintCheckBox(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
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
    GetType(&type);
    switch (aEvent->message) {
      case NS_MOUSE_LEFT_BUTTON_DOWN:
        mMouseDownOnCheckbox = PR_TRUE;
    //XXX: TODO render gray rectangle on mouse down  
      break;

      case NS_MOUSE_EXIT:
        mMouseDownOnCheckbox = PR_FALSE;
    //XXX: TO DO clear gray rectangle on mouse up 
      break;

    }
  }

  return(nsFormControlFrame::HandleEvent(aPresContext, aEvent, aEventStatus));
}


void nsCheckboxControlFrame::GetCheckboxControlFrameState(nsString& aValue)
{
  if (nsnull != mWidget) {
    nsICheckButton* checkBox = nsnull;
    if (NS_OK == mWidget->QueryInterface(kICheckButtonIID,(void**)&checkBox)) {
      PRBool state = PR_FALSE;
      checkBox->GetState(state);
      nsFormControlHelper::GetBoolString(state, aValue);
      NS_RELEASE(checkBox);
    }
  }
  else {   
      // Get the state for GFX-rendered widgets
    nsFormControlHelper::GetBoolString(mChecked, aValue);
  }
}       


void nsCheckboxControlFrame::SetCheckboxControlFrameState(const nsString& aValue)
{
  if (nsnull != mWidget) {
    nsICheckButton* checkBox = nsnull;
    if (NS_OK == mWidget->QueryInterface(kICheckButtonIID,(void**)&checkBox)) {
      PRBool state = nsFormControlHelper::GetBool(aValue);
      checkBox->SetState(state);
      NS_RELEASE(checkBox);
    }
  }
  else {
      // Set the state for GFX-rendered widgets
    mChecked = nsFormControlHelper::GetBool(aValue);
    nsFormControlHelper::ForceDrawFrame(this);
  }
}         

NS_IMETHODIMP nsCheckboxControlFrame::SetProperty(nsIAtom* aName, const nsString& aValue)
{
  if (nsHTMLAtoms::checked == aName) {
    SetCheckboxControlFrameState(aValue);
  }
  else {
    return nsFormControlFrame::SetProperty(aName, aValue);
  }

  return NS_OK;     
}


NS_IMETHODIMP nsCheckboxControlFrame::GetProperty(nsIAtom* aName, nsString& aValue)
{
  // Return the value of the property from the widget it is not null.
  // If is null, assume the widget is GFX-rendered and return a member variable instead.

  if (nsHTMLAtoms::checked == aName) {
    GetCheckboxControlFrameState(aValue);
  }
  else {
    return nsFormControlFrame::GetProperty(aName, aValue);
  }

  return NS_OK;     
}

nsresult nsCheckboxControlFrame::RequiresWidget(PRBool& aRequiresWidget)
{
  aRequiresWidget = PR_FALSE;
  return NS_OK;
}

