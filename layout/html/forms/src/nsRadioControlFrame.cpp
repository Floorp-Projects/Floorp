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

static NS_DEFINE_IID(kIRadioIID, NS_IRADIOBUTTON_IID);
static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);

#define NS_DESIRED_RADIOBOX_SIZE  12


nsresult
NS_NewRadioControlFrame(nsIFrame*& aResult)
{
  aResult = new nsRadioControlFrame;
  if (nsnull == aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
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

void 
nsRadioControlFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                  const nsHTMLReflowState& aReflowState,
                                  nsHTMLReflowMetrics& aDesiredLayoutSize,
                                  nsSize& aDesiredWidgetSize)
{
  float p2t;
  aPresContext->GetScaledPixelsToTwips(p2t);
  aDesiredWidgetSize.width  = NSIntPixelsToTwips(NS_DESIRED_RADIOBOX_SIZE, p2t);
  aDesiredWidgetSize.height = NSIntPixelsToTwips(NS_DESIRED_RADIOBOX_SIZE, p2t);
  aDesiredLayoutSize.width  = aDesiredWidgetSize.width;
  aDesiredLayoutSize.height = aDesiredWidgetSize.height;
  aDesiredLayoutSize.ascent = aDesiredLayoutSize.height;
  aDesiredLayoutSize.descent = 0;
}

void 
nsRadioControlFrame::PostCreateWidget(nsIPresContext* aPresContext, nscoord& aWidth, nscoord& aHeight)
{
  nsIRadioButton* radio = nsnull;
  if (mWidget && (NS_OK == mWidget->QueryInterface(GetIID(),(void**)&radio))) {
		radio->SetState(GetChecked(PR_TRUE));

	  const nsStyleColor* color = 
	    nsStyleUtil::FindNonTransparentBackground(mStyleContext);

	  if (nsnull != color) {
	    mWidget->SetBackgroundColor(color->mBackgroundColor);
	  } else {
	    mWidget->SetBackgroundColor(NS_RGB(0xFF, 0xFF, 0xFF));
	  }
	  NS_RELEASE(radio);
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
        PRBool checkedAttr  = GetChecked(PR_TRUE);
        PRBool checkedPrevState = GetChecked(PR_FALSE);
        if (checkedAttr != checkedPrevState) {
          button->SetState(checkedAttr);
          mFormFrame->OnRadioChecked(*this, checkedAttr);
        }
        NS_RELEASE(button);
      }
    }   
  }
  return result;
}

void 
nsRadioControlFrame::MouseClicked(nsIPresContext* aPresContext) 
{
  nsIRadioButton* radioWidget;
  if (mWidget && (NS_OK == mWidget->QueryInterface(kIRadioIID, (void**)&radioWidget))) {
	  radioWidget->SetState(PR_TRUE);
    NS_RELEASE(radioWidget);
    if (mFormFrame) {
      mFormFrame->OnRadioChecked(*this);
    } 
  }
}

PRBool
nsRadioControlFrame::GetChecked(PRBool aGetInitialValue) 
{
  if (aGetInitialValue) {
    if (mContent) {
      nsIHTMLContent* iContent = nsnull;
      nsresult result = mContent->QueryInterface(kIHTMLContentIID, (void**)&iContent);
      if ((NS_OK == result) && iContent) {
        nsHTMLValue value;
        result = iContent->GetAttribute(nsHTMLAtoms::checked, value);
        if (NS_CONTENT_ATTR_HAS_VALUE == result) {
          return PR_TRUE;
        }
        NS_RELEASE(iContent);
      }
    }
    return PR_FALSE;
  }

  PRBool result = PR_FALSE;
  nsIRadioButton* radio = nsnull;
  if (mWidget && (NS_OK == mWidget->QueryInterface(kIRadioIID,(void**)&radio))) {
    radio->GetState(result);
    NS_RELEASE(radio);
  } else {
    result = GetChecked(PR_TRUE);
  }
  return result;
}

void
nsRadioControlFrame::SetChecked(PRBool aValue, PRBool aSetInitialValue)
{
  if (aSetInitialValue) {
    if (aValue) {
      mContent->SetAttribute("checked", "1", PR_FALSE);
    } else {
      mContent->SetAttribute("checked", "0", PR_FALSE);
    }
  }
  nsIRadioButton* radio = nsnull;
  if (mWidget && (NS_OK ==  mWidget->QueryInterface(kIRadioIID,(void**)&radio))) {
    radio->SetState(aValue);
    NS_RELEASE(radio);
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
  nsIRadioButton* radio = nsnull;
  if (mWidget && (NS_OK == mWidget->QueryInterface(kIRadioIID,(void**)&radio))) {
    radio->SetState(GetChecked(PR_TRUE));
    NS_RELEASE(radio);
  }
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

//
// XXX: The following paint code is TEMPORARY. It is being used to get printing working
// under windows. Later it may be used to GFX-render the controls to the display. 
// Expect this code to repackaged and moved to a new location in the future.
//

void nsRadioControlFrame::GetCurrentRadioState(PRBool *aState)
{
  nsIDOMHTMLInputElement* inputElement;
  if (NS_OK == mContent->QueryInterface(kIDOMHTMLInputElementIID, (void**)&inputElement)) {
    inputElement->GetChecked(aState);
    NS_RELEASE(inputElement);
  }
}

void
nsRadioControlFrame::PaintRadioButton(nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect)
{
#ifdef XP_PC
  aRenderingContext.PushState();

  float p2t;
  aPresContext.GetScaledPixelsToTwips(p2t);
 
    //XXX:??? Offset for rendering, Not sure why we need this??? When rendering the 
    //radiobox to the screen this is not needed. But it is needed when Printing. Looks
    // Like it offsets from the middle of the control during Printing, but not when rendered
    // to the screen?
  const int printOffsetX = (NS_DESIRED_RADIOBOX_SIZE / 2);
  const int printOffsetY = (NS_DESIRED_RADIOBOX_SIZE / 2);
  aRenderingContext.Translate(NSIntPixelsToTwips(printOffsetX, p2t), 
                              NSIntPixelsToTwips(printOffsetY, p2t));

  nsFormControlFrame::Paint(aPresContext, aRenderingContext, aDirtyRect);

  const nsStyleSpacing* spacing = (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin border;
  spacing->CalcBorderFor(this, border);

  nscoord onePixel     = NSIntPixelsToTwips(1, p2t);
  nscoord twelvePixels = NSIntPixelsToTwips(12, p2t);

  nsRect outside(0, 0, mRect.width, mRect.height);

  aRenderingContext.SetColor(NS_RGB(192,192,192));
  aRenderingContext.FillRect(outside);

  PRBool standardSize = PR_FALSE;
  if (standardSize) {
    outside.SetRect(0, 0, twelvePixels, twelvePixels);
    aRenderingContext.SetColor(NS_RGB(255,255,255));
    aRenderingContext.FillArc(outside, 0, 180);
    aRenderingContext.FillArc(outside, 180, 360);

    if (mLastMouseState == eMouseDown) {
      outside.Deflate(onePixel, onePixel);
      outside.Deflate(onePixel, onePixel);
      aRenderingContext.SetColor(NS_RGB(192,192,192));
      aRenderingContext.FillArc(outside, 0, 180);
      aRenderingContext.FillArc(outside, 180, 360);
      outside.SetRect(0, 0, twelvePixels, twelvePixels);
    }

    // DrakGray
    aRenderingContext.SetColor(NS_RGB(128,128,128));
    DrawLine(aRenderingContext, 4, 0, 7, 0, PR_TRUE, 1, onePixel);
    DrawLine(aRenderingContext, 2, 1, 3, 1, PR_TRUE, 1, onePixel);
    DrawLine(aRenderingContext, 8, 1, 9, 1, PR_TRUE, 1, onePixel);

    DrawLine(aRenderingContext, 1, 2, 1, 3, PR_FALSE, 1, onePixel);
    DrawLine(aRenderingContext, 0, 4, 0, 7, PR_FALSE, 1, onePixel);
    DrawLine(aRenderingContext, 1, 8, 1, 9, PR_FALSE, 1, onePixel);

    // Black
    aRenderingContext.SetColor(NS_RGB(0,0,0));
    DrawLine(aRenderingContext, 4, 1, 7, 1, PR_TRUE, 1, onePixel);
    DrawLine(aRenderingContext, 2, 2, 3, 2, PR_TRUE, 1, onePixel);
    DrawLine(aRenderingContext, 8, 2, 9, 2, PR_TRUE, 1, onePixel);

    DrawLine(aRenderingContext, 2, 2, 2, 3, PR_FALSE, 1, onePixel);
    DrawLine(aRenderingContext, 1, 4, 1, 7, PR_FALSE, 1, onePixel);
    DrawLine(aRenderingContext, 2, 8, 2, 8, PR_FALSE, 1, onePixel);

    // Gray
    aRenderingContext.SetColor(NS_RGB(192, 192, 192));
    DrawLine(aRenderingContext, 2, 9, 3, 9, PR_TRUE, 1, onePixel);
    DrawLine(aRenderingContext, 8, 9, 9, 9, PR_TRUE, 1, onePixel);
    DrawLine(aRenderingContext, 4, 10, 7, 10, PR_TRUE, 1, onePixel);

    DrawLine(aRenderingContext, 9, 3, 9, 3, PR_FALSE, 1, onePixel);
    DrawLine(aRenderingContext, 10, 4, 10, 7, PR_FALSE, 1, onePixel);
    DrawLine(aRenderingContext, 9, 8, 9, 9, PR_FALSE, 1, onePixel);

    outside.Deflate(onePixel, onePixel);
    outside.Deflate(onePixel, onePixel);
    outside.Deflate(onePixel, onePixel);
    outside.Deflate(onePixel, onePixel);
  } else {
    outside.SetRect(0, 0, twelvePixels, twelvePixels);

    aRenderingContext.SetColor(NS_RGB(128,128,128));
    aRenderingContext.FillArc(outside, 46, 225);
    aRenderingContext.SetColor(NS_RGB(255,255,255));
    aRenderingContext.FillArc(outside, 225, 360);
    aRenderingContext.FillArc(outside, 0, 44);

    outside.Deflate(onePixel, onePixel);
    aRenderingContext.SetColor(NS_RGB(0,0,0));
    aRenderingContext.FillArc(outside, 46, 225);
    aRenderingContext.SetColor(NS_RGB(192,192,192));
    aRenderingContext.FillArc(outside, 225, 360);
    aRenderingContext.FillArc(outside, 0, 44);

    outside.Deflate(onePixel, onePixel);
    aRenderingContext.SetColor(NS_RGB(255,255,255));
    aRenderingContext.FillArc(outside, 0, 180);
    aRenderingContext.FillArc(outside, 180, 360);
    outside.Deflate(onePixel, onePixel);
    outside.Deflate(onePixel, onePixel);
  }

  PRBool checked = PR_TRUE;
  GetCurrentRadioState(&checked); // Get check state from the content model
  if (PR_TRUE == checked) {
	  // Have to do 180 degress at a time because FillArc will not correctly
	  // go from 0-360
    aRenderingContext.SetColor(NS_RGB(0,0,0));
    aRenderingContext.FillArc(outside, 0, 180);
    aRenderingContext.FillArc(outside, 180, 360);
  }

  PRBool clip;
  aRenderingContext.PopState(clip);
#endif
}

NS_METHOD 
nsRadioControlFrame::Paint(nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect)
{
#ifdef XP_PC
  PaintRadioButton(aPresContext, aRenderingContext, aDirtyRect);
#endif
  return NS_OK;
}
