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

#include "nsFileControlFrame.h"
#include "nsFormControlHelper.h"
#include "nsButtonControlFrame.h"
#include "nsHTMLParts.h"
#include "nsIRenderingContext.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIStyleContext.h"
#include "nsLeafFrame.h"
#include "nsCSSRendering.h"
#include "nsHTMLIIDs.h"
#include "nsIButton.h"
#include "nsIViewManager.h"
#include "nsISupports.h"
#include "nsHTMLAtoms.h"
#include "nsIButton.h"
#include "nsIView.h"
#include "nsViewsCID.h"
#include "nsWidgetsCID.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nsIFormControl.h"
#include "nsStyleUtil.h"
#include "nsDOMEvent.h"
#include "nsStyleConsts.h"
#include "nsIHTMLAttributes.h"
#include "nsGenericHTMLElement.h"
#include "nsFormFrame.h"
#include "nsILookAndFeel.h"
#include "nsCOMPtr.h"
#include "nsDOMEvent.h"
#include "nsIViewManager.h"

static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kIButtonIID,      NS_IBUTTON_IID);
static NS_DEFINE_IID(kLookAndFeelCID,  NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);

nsButtonControlFrame::nsButtonControlFrame()
{
  mRenderer.SetNameSpace(kNameSpaceID_None);
}

NS_IMETHODIMP
nsButtonControlFrame::Init(nsIPresContext&  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsFormControlFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  mRenderer.SetFrame(this,aPresContext);
  return rv;
}

void
nsButtonControlFrame::GetDefaultLabel(nsString& aString) 
{
  PRInt32 type;
  GetType(&type);
  if (NS_FORM_INPUT_RESET == type) {
    aString = "Reset";
  } 
  else if (NS_FORM_INPUT_SUBMIT == type) {
    aString = "Submit Query";
  } 
  else if (NS_FORM_BROWSE == type) {
    aString = "Browse...";
  } 
  else {
    aString = " ";
  }
}

PRBool
nsButtonControlFrame::IsSuccessful(nsIFormControlFrame* aSubmitter)
{
  PRInt32 type;
  GetType(&type);
  if ((NS_FORM_INPUT_HIDDEN == type) || (this == aSubmitter)) {
    return nsFormControlFrame::IsSuccessful(aSubmitter);
  } else {
    return PR_FALSE;
  }
}

PRInt32
nsButtonControlFrame::GetMaxNumValues() 
{
  PRInt32 type;
  GetType(&type);
  if ((NS_FORM_INPUT_SUBMIT == type) || (NS_FORM_INPUT_HIDDEN == type)) {
    return 1;
  } else {
    return 0;
  }
}


PRBool
nsButtonControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                     nsString* aValues, nsString* aNames)
{
  nsAutoString name;
  nsresult result = GetName(&name);
  if ((aMaxNumValues <= 0) || (NS_CONTENT_ATTR_HAS_VALUE != result)) {
    return PR_FALSE;
  }

  PRInt32 type;
  GetType(&type);

  if (NS_FORM_INPUT_RESET == type) {
    aNumValues = 0;
    return PR_FALSE;
  } else {
    nsAutoString value;
    /*XXX nsresult valResult = */GetValue(&value);
    aValues[0] = value;
    aNames[0]  = name;
    aNumValues = 1;
    return PR_TRUE;
  }
}

nsresult
NS_NewButtonControlFrame(nsIFrame*& aResult)
{
  aResult = new nsButtonControlFrame;
  if (nsnull == aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nscoord 
nsButtonControlFrame::GetVerticalBorderWidth(float aPixToTwip) const
{
   return NSIntPixelsToTwips(4, aPixToTwip);
}

nscoord 
nsButtonControlFrame::GetHorizontalBorderWidth(float aPixToTwip) const
{
  return GetVerticalBorderWidth(aPixToTwip);
}

nscoord 
nsButtonControlFrame::GetVerticalInsidePadding(float aPixToTwip, 
                                                     nscoord aInnerHeight) const
{
  float pad;
  nsILookAndFeel * lookAndFeel;
  if (NS_OK == nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull, kILookAndFeelIID, (void**)&lookAndFeel)) {
   lookAndFeel->GetMetric(nsILookAndFeel::eMetricFloat_ButtonVerticalInsidePadding,  pad);
   NS_RELEASE(lookAndFeel);
  }
  return (nscoord)NSToIntRound((float)aInnerHeight * pad);
}

nscoord 
nsButtonControlFrame::GetHorizontalInsidePadding(nsIPresContext& aPresContext,
                                                 float aPixToTwip, 
                                                 nscoord aInnerWidth,
                                                 nscoord aCharWidth) const
{
  nsCompatibility mode;
  aPresContext.GetCompatibilityMode(&mode);

  float   pad;
  PRInt32 padQuirks;
  PRInt32 padQuirksOffset;
  nsILookAndFeel * lookAndFeel;
  if (NS_OK == nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull, kILookAndFeelIID, (void**)&lookAndFeel)) {
    lookAndFeel->GetMetric(nsILookAndFeel::eMetricFloat_ButtonHorizontalInsidePadding,  pad);
    lookAndFeel->GetMetric(nsILookAndFeel::eMetric_ButtonHorizontalInsidePaddingNavQuirks,  padQuirks);
    lookAndFeel->GetMetric(nsILookAndFeel::eMetric_ButtonHorizontalInsidePaddingOffsetNavQuirks,  padQuirksOffset);
    NS_RELEASE(lookAndFeel);
  }
  if (eCompatibility_NavQuirks == mode) {
    return (nscoord)NSToIntRound(float(aInnerWidth) * pad);
  } else {
    return NSIntPixelsToTwips(padQuirks, aPixToTwip) + padQuirksOffset;
  }
}

//
// ReResolveStyleContext
//
// When the style context changes, make sure that all of our styles are still up to date.
//
NS_IMETHODIMP
nsButtonControlFrame::ReResolveStyleContext ( nsIPresContext* aPresContext, nsIStyleContext* aParentContext,
                                              PRInt32 aParentChange, 
                                              nsStyleChangeList* aChangeList, PRInt32* aLocalChange)
{
  // this re-resolves |mStyleContext|, so it may change
  nsresult rv = nsFormControlFrame::ReResolveStyleContext(aPresContext, aParentContext, 
                                                          aParentChange, aChangeList, aLocalChange); 
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (NS_COMFALSE != rv) {
    mRenderer.ReResolveStyles(*aPresContext);
  }
  
  return rv;
  
} // ReResolveStyleContext



NS_IMETHODIMP
nsButtonControlFrame::AttributeChanged(nsIPresContext* aPresContext,
                                       nsIContent*     aChild,
                                       nsIAtom*        aAttribute,
                                       PRInt32         aHint)
{
  nsresult result = NS_OK;
  if (mWidget) {
    if (nsHTMLAtoms::value == aAttribute) {
      nsIButton* button = nsnull;
      result = mWidget->QueryInterface(kIButtonIID, (void**)&button);
      if ((NS_SUCCEEDED(result)) && (nsnull != button)) {
        nsString value;
        /*XXXnsresult result = */GetValue(&value);
        button->SetLabel(value);
        NS_RELEASE(button);
        nsFormFrame::StyleChangeReflow(aPresContext, this);
      }
    } else if (nsHTMLAtoms::size == aAttribute) {
      nsFormFrame::StyleChangeReflow(aPresContext, this);
    }
    // Allow the base class to handle common attributes supported
    // by all form elements... 
    else {
      result = nsFormControlFrame::AttributeChanged(aPresContext, aChild, aAttribute, aHint);
    }
  }
  return result;
}
                                     

NS_METHOD 
nsButtonControlFrame::Paint(nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect,
                            nsFramePaintLayer aWhichLayer)
{
    nsRect rect(0, 0, mRect.width, mRect.height);
    mRenderer.PaintButton(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer, rect);

    if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
	    nsString label;
	    nsresult result = GetValue(&label);

	    if (NS_CONTENT_ATTR_HAS_VALUE != result) {  
        GetDefaultLabel(label);
	    }
  
     nsRect content;
     mRenderer.GetButtonContentRect(rect,content);

   // paint the title 
	   const nsStyleFont* fontStyle = (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);
	   const nsStyleColor* colorStyle = (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);

	   aRenderingContext.SetFont(fontStyle->mFont);
	   
	   // if disabled paint 
	   if (PR_TRUE == mRenderer.isDisabled())
	   {
   		 float p2t;
		   aPresContext.GetScaledPixelsToTwips(&p2t);
		   nscoord pixel = NSIntPixelsToTwips(1, p2t);

		   aRenderingContext.SetColor(NS_RGB(255,255,255));
		   aRenderingContext.DrawString(label, content.x + pixel, content.y+ pixel);
	   }

	   aRenderingContext.SetColor(colorStyle->mColor);
	   aRenderingContext.DrawString(label, content.x, content.y);
    }

  return NS_OK;
}


void
nsButtonControlFrame::MouseClicked(nsIPresContext* aPresContext) 
{
  PRInt32 type;
  GetType(&type);

  if ((nsnull != mFormFrame) && !nsFormFrame::GetDisabled(this)) {
    nsEventStatus status = nsEventStatus_eIgnore;
    nsEvent event;
    event.eventStructType = NS_EVENT;
    nsIContent *formContent = nsnull;
    mFormFrame->GetContent(&formContent);

    switch(type) {
    case NS_FORM_INPUT_RESET:
      event.message = NS_FORM_RESET;
      if (nsnull != formContent) {
        formContent->HandleDOMEvent(*aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, status);
      }
      if (nsEventStatus_eConsumeNoDefault != status) {
        mFormFrame->OnReset();
      }
      break;
    case NS_FORM_INPUT_SUBMIT:
      event.message = NS_FORM_SUBMIT;
      if (nsnull != formContent) {
        formContent->HandleDOMEvent(*aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, status); 
      }
      if (nsEventStatus_eConsumeNoDefault != status) {
        mFormFrame->OnSubmit(aPresContext, this);
      }
    }
    NS_IF_RELEASE(formContent);
  } 
  //else if ((NS_FORM_BROWSE == type) && mMouseListener) {
  else if (nsnull != mMouseListener) {
    mMouseListener->MouseClicked(aPresContext);
  }
}

NS_METHOD
nsButtonControlFrame::Reflow(nsIPresContext&          aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus)
{
  PRInt32 type;
  GetType(&type);

  nsWidgetRendering mode;
  aPresContext.GetWidgetRenderingMode(&mode);

  if (eWidgetRendering_Gfx == mode || NS_FORM_INPUT_IMAGE == type) {
    nsSize ignore;
    GetDesiredSize(&aPresContext, aReflowState, aDesiredSize, ignore);
    nsMargin bp(0,0,0,0);
    AddBordersAndPadding(&aPresContext, aReflowState, aDesiredSize, bp);
    mRenderer.AddFocusBordersAndPadding(aPresContext, aReflowState, aDesiredSize, bp);

    if (nsnull != aDesiredSize.maxElementSize) {
      aDesiredSize.AddBorderPaddingToMaxElementSize(bp);
    }
    aStatus = NS_FRAME_COMPLETE;
    return NS_OK;
  }
  else {
    return nsFormControlFrame:: Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
  }
}

void 
nsButtonControlFrame::GetDesiredSize(nsIPresContext*          aPresContext,
                                     const nsHTMLReflowState& aReflowState,
                                     nsHTMLReflowMetrics&     aDesiredLayoutSize,
                                     nsSize&                  aDesiredWidgetSize)
{
  PRInt32 type;
  GetType(&type);

  if (NS_FORM_INPUT_HIDDEN == type) { // there is no physical rep
    aDesiredLayoutSize.width   = 0;
    aDesiredLayoutSize.height  = 0;
    aDesiredLayoutSize.ascent  = 0;
    aDesiredLayoutSize.descent = 0;
    if (aDesiredLayoutSize.maxElementSize) {
      aDesiredLayoutSize.maxElementSize->width  = 0;
      aDesiredLayoutSize.maxElementSize->height = 0;
    }
  } else {
    nsSize styleSize;
    GetStyleSize(*aPresContext, aReflowState, styleSize);
    // a browse button shares its style context with its parent nsInputFile
    // it uses everything from it except width
    if (NS_FORM_BROWSE == type) {
      styleSize.width = CSS_NOTSET;
    }
    nsSize desiredSize;
    nsSize minSize;
    PRBool widthExplicit, heightExplicit;
    PRInt32 ignore;
    nsAutoString defaultLabel;
    GetDefaultLabel(defaultLabel);
    nsInputDimensionSpec spec(nsHTMLAtoms::size, PR_TRUE, nsHTMLAtoms::value, 
                              &defaultLabel, 1, PR_FALSE, nsnull, 1);
    nsFormControlHelper::CalculateSize(aPresContext, aReflowState.rendContext, this, styleSize, 
                                       spec, desiredSize, minSize, widthExplicit, heightExplicit, ignore);   

    // set desired size, max element size
    aDesiredLayoutSize.width = desiredSize.width;
    aDesiredLayoutSize.height= desiredSize.height;
    if (aDesiredLayoutSize.maxElementSize) {
      aDesiredLayoutSize.maxElementSize->width  = minSize.width;
      aDesiredLayoutSize.maxElementSize->height = minSize.height;
    }
  }

  aDesiredWidgetSize.width = aDesiredLayoutSize.width;
  aDesiredWidgetSize.height= aDesiredLayoutSize.height;
}


void 
nsButtonControlFrame::PostCreateWidget(nsIPresContext* aPresContext, nscoord& aWidth, nscoord& aHeight)
{
 	nsIButton* button = nsnull;
  if (mWidget && (NS_OK == mWidget->QueryInterface(kIButtonIID,(void**)&button))) {
    nsFont font(aPresContext->GetDefaultFixedFontDeprecated()); 
    GetFont(aPresContext, font);
    mWidget->SetFont(font);
    SetColors(*aPresContext);

    nsAutoString value;
    nsresult result = GetValue(&value);

    if (NS_CONTENT_ATTR_HAS_VALUE != result) {  
      GetDefaultLabel(value);
    }
    button->SetLabel(value);
    NS_RELEASE(button);

    mWidget->Enable(!nsFormFrame::GetDisabled(this));
  }
}

const nsIID&
nsButtonControlFrame::GetIID()
{
  static NS_DEFINE_IID(kButtonIID, NS_IBUTTON_IID);
  return kButtonIID;
}
  
const nsIID&
nsButtonControlFrame::GetCID()
{
  static NS_DEFINE_IID(kButtonCID, NS_BUTTON_CID);
  return kButtonCID;
}

NS_IMETHODIMP
nsButtonControlFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("ButtonControl", aResult);
}


void 
nsButtonControlFrame::PaintButton(nsIPresContext& aPresContext,
                                  nsIRenderingContext& aRenderingContext,
                                  const nsRect& aDirtyRect,
                                  nsString& aLabel, const nsRect& aRect)
{
   
}

NS_IMETHODIMP
nsButtonControlFrame::HandleEvent(nsIPresContext& aPresContext, 
                                  nsGUIEvent*     aEvent,
                                  nsEventStatus&  aEventStatus)
{
  nsWidgetRendering mode;
  aPresContext.GetWidgetRenderingMode(&mode);

  if (eWidgetRendering_Native == mode) {
    return nsFormControlFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
  } 

  // if disabled do nothing
  if (mRenderer.isDisabled()) {
    return NS_OK;
  }

  nsresult result = mRenderer.HandleEvent(aPresContext, aEvent, aEventStatus);
  if (NS_OK != result) {
    return result;
  }
    
  aEventStatus = nsEventStatus_eIgnore;
 
  switch (aEvent->message) {

    case NS_MOUSE_ENTER:
	   break;
 
    case NS_MOUSE_LEFT_BUTTON_DOWN:
      mRenderer.SetFocus(PR_TRUE, PR_TRUE);         
	    break;

    case NS_MOUSE_LEFT_BUTTON_UP:
      if (mRenderer.isHover()) {
			  MouseClicked(&aPresContext);
      }
	    break;

    case NS_KEY_DOWN:
      if (NS_KEY_EVENT == aEvent->eventStructType) {
        nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
        if (NS_VK_SPACE == keyEvent->keyCode || NS_VK_RETURN == keyEvent->keyCode) {
          MouseClicked(&aPresContext);
        }
      }

    case NS_MOUSE_EXIT:
	    break;
  }

  return NS_OK;

}

void 
nsButtonControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
  mRenderer.SetFocus(aOn, aRepaint);
}

void
nsButtonControlFrame::Redraw()
{	nsRect rect(0, 0, mRect.width, mRect.height);
    Invalidate(rect, PR_TRUE);

}


NS_IMETHODIMP nsButtonControlFrame::SetProperty(nsIAtom* aName, const nsString& aValue)
{
  return NS_OK;
}

NS_IMETHODIMP nsButtonControlFrame::GetProperty(nsIAtom* aName, nsString& aValue)
{
  return NS_OK;
}

nsresult nsButtonControlFrame::RequiresWidget(PRBool& aRequiresWidget)
{
  aRequiresWidget = PR_FALSE;
  return NS_OK;
}

