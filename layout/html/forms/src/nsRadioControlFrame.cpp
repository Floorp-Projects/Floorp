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

#include "nsRadioControlFrame.h"
#include "nsFormControlFrame.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIFrame.h"
#include "nsISupports.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsIRadioButton.h"
#include "nsWidgetsCID.h"
#include "nsSize.h"
#include "nsHTMLAtoms.h"
#include "nsIFormControl.h"
#include "nsIFormManager.h"
#include "nsIView.h"
#include "nsIStyleContext.h"
#include "nsStyleUtil.h"
#include "nsFormFrame.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsINameSpaceManager.h"
#include "nsILookAndFeel.h"
#include "nsIComponentManager.h"

static NS_DEFINE_IID(kIRadioIID, NS_IRADIOBUTTON_IID);
static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);
static NS_DEFINE_IID(kLookAndFeelCID,  NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);

#define NS_DEFAULT_RADIOBOX_SIZE  12


nsresult NS_NewRadioControlFrame(nsIFrame*& aResult);
nsresult
NS_NewRadioControlFrame(nsIFrame*& aResult)
{
  aResult = new nsRadioControlFrame;
  if (nsnull == aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsRadioControlFrame::nsRadioControlFrame()
{
   // Initialize GFX-rendered state
  mChecked = PR_FALSE;
}



const nsIID&
nsRadioControlFrame::GetIID()
{
  return kIRadioIID;
}
  
const nsIID&
nsRadioControlFrame::GetCID()
{
  static NS_DEFINE_IID(kRadioCID, NS_RADIOBUTTON_CID);
  return kRadioCID;
}

nscoord 
nsRadioControlFrame::GetRadioboxSize(float aPixToTwip) const
{
  nsILookAndFeel * lookAndFeel;
  PRInt32 radioboxSize = 0;
  if (NS_OK == nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull, kILookAndFeelIID, (void**)&lookAndFeel)) {
   lookAndFeel->GetMetric(nsILookAndFeel::eMetric_RadioboxSize,  radioboxSize);
   NS_RELEASE(lookAndFeel);
  }
 if (radioboxSize == 0)
   radioboxSize = NS_DEFAULT_RADIOBOX_SIZE;
  return NSIntPixelsToTwips(radioboxSize, aPixToTwip);
}

void 
nsRadioControlFrame::GetDesiredSize(nsIPresContext*        aPresContext,
                                  const nsHTMLReflowState& aReflowState,
                                  nsHTMLReflowMetrics&     aDesiredLayoutSize,
                                  nsSize&                  aDesiredWidgetSize)
{

  nsWidgetRendering mode;
  aPresContext->GetWidgetRenderingMode(&mode);
  if (eWidgetRendering_Gfx == mode) {
    nsFormControlFrame::GetDesiredSize(aPresContext,aReflowState,aDesiredLayoutSize,
                                    aDesiredWidgetSize);
  } else {
    float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);
    aDesiredWidgetSize.width  = GetRadioboxSize(p2t);
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
nsRadioControlFrame::PostCreateWidget(nsIPresContext* aPresContext, nscoord& aWidth, nscoord& aHeight)
{
   // set the widget to the initial state
  PRBool checked = PR_FALSE;
  nsresult result = GetDefaultCheckState(&checked);
  if (NS_CONTENT_ATTR_HAS_VALUE == result) {
    if (PR_TRUE == checked)
      SetRadioControlFrameState(NS_STRING_TRUE);
    else
      SetRadioControlFrameState(NS_STRING_FALSE);
  }

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
nsRadioControlFrame::AttributeChanged(nsIPresContext* aPresContext,
                                      nsIContent*     aChild,
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
          mFormFrame->OnRadioChecked(*this, checkedAttr);
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
nsRadioControlFrame::MouseClicked(nsIPresContext* aPresContext) 
{
  SetRadioControlFrameState(NS_STRING_TRUE);
  
  if (mFormFrame) {
     // The form frame will determine which radio button needs
     // to be turned off and will call SetChecked on the
     // nsRadioControlFrame to unset the checked state
    mFormFrame->OnRadioChecked(*this);
  }         
}

PRBool
nsRadioControlFrame::GetChecked(PRBool aGetInitialValue) 
{
  PRBool checked = PR_FALSE;
  if (PR_TRUE == aGetInitialValue) {
    GetDefaultCheckState(&checked);
  }
  else {
    GetCurrentCheckState(&checked);
  }
  return(checked);
}

void
nsRadioControlFrame::SetChecked(PRBool aValue, PRBool aSetInitialValue)
{
  if (aSetInitialValue) {
    if (aValue) {
      mContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::checked, nsAutoString("1"), PR_FALSE); // XXX should be "empty" value
    } else {
      mContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::checked, nsAutoString("0"), PR_FALSE);
    }
  }

  if (PR_TRUE == aValue) {
    SetRadioControlFrameState(NS_STRING_TRUE);
  } else {
    SetRadioControlFrameState(NS_STRING_FALSE);
  }
}


PRBool
nsRadioControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                    nsString* aValues, nsString* aNames)
{
  nsAutoString name;
  nsresult result = GetName(&name);
  if ((aMaxNumValues <= 0) || (NS_CONTENT_ATTR_HAS_VALUE != result)) {
    return PR_FALSE;
  }

  PRBool state = PR_FALSE;

  nsIRadioButton* radio = nsnull;
 	if (mWidget && (NS_OK == mWidget->QueryInterface(kIRadioIID,(void**)&radio))) {
  	radio->GetState(state);
	  NS_RELEASE(radio);
  }
  if(PR_TRUE != state) {
    return PR_FALSE;
  }

  nsAutoString value;
  result = GetValue(&value);
  
  if (NS_CONTENT_ATTR_HAS_VALUE == result) {
    aValues[0] = value;
  } else {
    aValues[0] = "on";
  }
  aNames[0]  = name;
  aNumValues = 1;

  return PR_TRUE;
}

void 
nsRadioControlFrame::Reset() 
{
  PRBool checked = PR_TRUE;
  GetDefaultCheckState(&checked);
  SetCurrentCheckState(checked);
}  


// CLASS nsRadioControlGroup

nsRadioControlGroup::nsRadioControlGroup(nsString& aName)
:mName(aName), mCheckedRadio(nsnull)
{
}

nsRadioControlGroup::~nsRadioControlGroup()
{
  mCheckedRadio = nsnull;
}
  
PRInt32 
nsRadioControlGroup::GetRadioCount() const 
{ 
  return mRadios.Count(); 
}

nsRadioControlFrame*
nsRadioControlGroup::GetRadioAt(PRInt32 aIndex) const 
{ 
  return (nsRadioControlFrame*) mRadios.ElementAt(aIndex);
}

PRBool 
nsRadioControlGroup::AddRadio(nsRadioControlFrame* aRadio) 
{ 
  return mRadios.AppendElement(aRadio);
}

PRBool 
nsRadioControlGroup::RemoveRadio(nsRadioControlFrame* aRadio) 
{ 
  return mRadios.RemoveElement(aRadio);
}

nsRadioControlFrame*
nsRadioControlGroup::GetCheckedRadio()
{
  return mCheckedRadio;
}

void    
nsRadioControlGroup::SetCheckedRadio(nsRadioControlFrame* aRadio)
{
  mCheckedRadio = aRadio;
}

void
nsRadioControlGroup::GetName(nsString& aNameResult) const
{
  aNameResult = mName;
}

NS_IMETHODIMP
nsRadioControlFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("RadioControl", aResult);
}


void
nsRadioControlFrame::PaintRadioButton(nsIPresContext& aPresContext,
                                      nsIRenderingContext& aRenderingContext,
                                      const nsRect& aDirtyRect)
{
  aRenderingContext.PushState();

  const nsStyleColor* color = (const nsStyleColor*)
     mStyleContext->GetStyleData(eStyleStruct_Color); 

  //XXX: This is backwards for now. The PaintCircular border code actually draws pie slices.

  nsFormControlHelper::PaintCircularBorder(aPresContext,aRenderingContext,
                         aDirtyRect, mStyleContext, PR_FALSE, this, mRect.width, mRect.height);
  nsFormControlHelper::PaintCircularBackground(aPresContext,aRenderingContext,
                         aDirtyRect, mStyleContext, PR_FALSE, this, mRect.width, mRect.height);

 
  PRBool checked = PR_TRUE;
  GetCurrentCheckState(&checked); // Get check state from the content model
  if (PR_TRUE == checked) {
	  // Have to do 180 degress at a time because FillArc will not correctly
	  // go from 0-360
    float p2t;
    aPresContext.GetScaledPixelsToTwips(&p2t);

    nscoord onePixel     = NSIntPixelsToTwips(1, p2t);
//XXX    nscoord twelvePixels = NSIntPixelsToTwips(12, p2t);

    nsRect outside;
    nsFormControlHelper::GetCircularRect(mRect.width, mRect.height, outside);
  
    outside.Deflate(onePixel, onePixel);
    outside.Deflate(onePixel, onePixel);
    outside.Deflate(onePixel, onePixel);
    outside.Deflate(onePixel, onePixel);
    
    aRenderingContext.SetColor(color->mColor);
    aRenderingContext.FillArc(outside, 0, 180);
    aRenderingContext.FillArc(outside, 180, 360);
  }

  PRBool clip;
  aRenderingContext.PopState(clip);
}

NS_METHOD 
nsRadioControlFrame::Paint(nsIPresContext& aPresContext,
                           nsIRenderingContext& aRenderingContext,
                           const nsRect& aDirtyRect,
                           nsFramePaintLayer aWhichLayer)
{
  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    PaintRadioButton(aPresContext, aRenderingContext, aDirtyRect);
  }
  return NS_OK;
}

void nsRadioControlFrame::GetRadioControlFrameState(nsString& aValue)
{
  if (nsnull != mWidget) {
    nsIRadioButton* radio = nsnull;
    if (NS_OK == mWidget->QueryInterface(kIRadioIID,(void**)&radio)) {
      PRBool state = PR_FALSE;
      radio->GetState(state);
      nsFormControlHelper::GetBoolString(state, aValue);
      NS_RELEASE(radio);
    }
  }
  else {   
      // Get the state for GFX-rendered widgets
    nsFormControlHelper::GetBoolString(mChecked, aValue);
  }
}         

void nsRadioControlFrame::SetRadioControlFrameState(const nsString& aValue)
{
  if (nsnull != mWidget) {
    nsIRadioButton* radio = nsnull;
    if (NS_OK == mWidget->QueryInterface(kIRadioIID,(void**)&radio)) {
      if (aValue == NS_STRING_TRUE)
        radio->SetState(PR_TRUE);
      else
        radio->SetState(PR_FALSE);

      NS_RELEASE(radio);
    }
  }
  else {    
       // Set the state for GFX-rendered widgets
    mChecked = nsFormControlHelper::GetBool(aValue);
    nsFormControlHelper::ForceDrawFrame(this);
  }
}         

NS_IMETHODIMP nsRadioControlFrame::SetProperty(nsIAtom* aName, const nsString& aValue)
{
  if (nsHTMLAtoms::checked == aName) {
    SetRadioControlFrameState(NS_STRING_TRUE);
    if (mFormFrame) {
      PRBool state = (aValue == NS_STRING_TRUE) ? PR_TRUE : PR_FALSE;
      mFormFrame->OnRadioChecked(*this, state);
    }
  }
  else {
    return nsFormControlFrame::SetProperty(aName, aValue);
  }

  return NS_OK;    
}

NS_IMETHODIMP nsRadioControlFrame::GetProperty(nsIAtom* aName, nsString& aValue)
{
  // Return the value of the property from the widget it is not null.
  // If is null, assume the widget is GFX-rendered and return a member variable instead.

  if (nsHTMLAtoms::checked == aName) {
    GetRadioControlFrameState(aValue);
  }
  else {
    return nsFormControlFrame::GetProperty(aName, aValue);
  }

  return NS_OK;    
}


nsresult nsRadioControlFrame::RequiresWidget(PRBool& aRequiresWidget)
{
  aRequiresWidget = PR_FALSE;
  return NS_OK;
}
