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
    nsresult valResult = GetValue(&value);
    aValues[0] = value;
    aNames[0]  = name;
    aNumValues = 1;
    return PR_TRUE;
  }
}

nsButtonControlFrame::nsButtonControlFrame()
{
   // Initialize GFX-rendered state
  mPressed = PR_FALSE;
  mGotFocus = PR_FALSE;
  mDisabled = PR_FALSE;
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
  if (NS_OK == nsRepository::CreateInstance(kLookAndFeelCID, nsnull, kILookAndFeelIID, (void**)&lookAndFeel)) {
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
  aPresContext.GetCompatibilityMode(mode);

  float   pad;
  PRInt32 padQuirks;
  PRInt32 padQuirksOffset;
  nsILookAndFeel * lookAndFeel;
  if (NS_OK == nsRepository::CreateInstance(kLookAndFeelCID, nsnull, kILookAndFeelIID, (void**)&lookAndFeel)) {
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
        nsresult result = GetValue(&value);
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
  if (eFramePaintLayer_Content == aWhichLayer) {
	  nsString label;
	  nsresult result = GetValue(&label);

	  if (NS_CONTENT_ATTR_HAS_VALUE != result) {  
		GetDefaultLabel(label);
	  }
      PaintButton(aPresContext, aRenderingContext, aDirtyRect, label);
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
        formContent->HandleDOMEvent(*aPresContext, &event, nsnull, DOM_EVENT_INIT, status);
      }
      if (nsEventStatus_eConsumeNoDefault != status) {
        mFormFrame->OnReset();
      }
      break;
    case NS_FORM_INPUT_SUBMIT:
      event.message = NS_FORM_SUBMIT;
      if (nsnull != formContent) {
        formContent->HandleDOMEvent(*aPresContext, &event, nsnull, DOM_EVENT_INIT, status); 
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

  if (NS_FORM_INPUT_IMAGE == type) {
    nsSize ignore;
    GetDesiredSize(&aPresContext, aReflowState, aDesiredSize, ignore);
    nsMargin bp;
    AddBordersAndPadding(&aPresContext, aReflowState, aDesiredSize, bp);
    if (nsnull != aDesiredSize.maxElementSize) {
      aDesiredSize.AddBorderPaddingToMaxElementSize(bp);
      aDesiredSize.maxElementSize->width = aDesiredSize.width;
      aDesiredSize.maxElementSize->height = aDesiredSize.height;
    }
    aStatus = NS_FRAME_COMPLETE;
    return NS_OK;
  }
  else {
    return nsFormControlFrame:: Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
  }
}

void 
nsButtonControlFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                   const nsHTMLReflowState& aReflowState,
                                   nsHTMLReflowMetrics& aDesiredLayoutSize,
                                   nsSize& aDesiredWidgetSize)
{
  PRInt32 type;
  GetType(&type);

  if (NS_FORM_INPUT_HIDDEN == type) { // there is no physical rep
    aDesiredLayoutSize.width  = 0;
    aDesiredLayoutSize.height = 0;
    aDesiredLayoutSize.ascent = 0;
    aDesiredLayoutSize.descent = 0;
  } else {
#ifdef NS_GFX_RENDER_FORM_ELEMENTS
	nsCOMPtr<nsIStyleContext> outlineStyle(mStyleContext);
	nsCOMPtr<nsIAtom> sbAtom (NS_NewAtom(":BUTTON-OUTLINE"));
	outlineStyle = aPresContext->ProbePseudoStyleContextFor(mContent, sbAtom, mStyleContext);
	const nsStyleSpacing* outline = (const nsStyleSpacing*)outlineStyle->GetStyleData(eStyleStruct_Spacing);
 
	nsMargin outlineBorder;
	outline->CalcBorderFor(this, outlineBorder);
#endif
    nsSize styleSize;
    GetStyleSize(*aPresContext, aReflowState, styleSize);
    // a browse button shares is style context with its parent nsInputFile
    // it uses everything from it except width
    if (NS_FORM_BROWSE == type) {
      styleSize.width = CSS_NOTSET;
    }
    nsSize size;
    PRBool widthExplicit, heightExplicit;
    PRInt32 ignore;
    nsAutoString defaultLabel;
    GetDefaultLabel(defaultLabel);
    nsInputDimensionSpec spec(nsHTMLAtoms::size, PR_TRUE, nsHTMLAtoms::value, 
                              &defaultLabel, 1, PR_FALSE, nsnull, 1);
    nsFormControlHelper::CalculateSize(aPresContext, this, styleSize, spec, size, 
                  widthExplicit, heightExplicit, ignore,
                  aReflowState.rendContext);
 #ifdef NS_GFX_RENDER_FORM_ELEMENTS
    aDesiredLayoutSize.width = size.width + outlineBorder.left + outlineBorder.right;
    aDesiredLayoutSize.height= size.height + outlineBorder.top + outlineBorder.bottom;
#else
    aDesiredLayoutSize.width = size.width;
    aDesiredLayoutSize.height= size.height;
#endif
    aDesiredLayoutSize.ascent = aDesiredLayoutSize.height;
    aDesiredLayoutSize.descent = 0;
  }

  aDesiredWidgetSize.width = aDesiredLayoutSize.width;
  aDesiredWidgetSize.height= aDesiredLayoutSize.height;
}


void 
nsButtonControlFrame::PostCreateWidget(nsIPresContext* aPresContext, nscoord& aWidth, nscoord& aHeight)
{
 	nsIButton* button = nsnull;
  if (mWidget && (NS_OK == mWidget->QueryInterface(kIButtonIID,(void**)&button))) {
    nsFont font(aPresContext->GetDefaultFixedFont()); 
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
                                  nsString& aLabel)
{
 PRBool disabled = nsFormFrame::GetDisabled(this);
  if ( disabled != mDisabled)
  {
	 if (disabled)
         mContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::kClass, "DISABLED", PR_TRUE);
	 else 
		 mContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::kClass, "", PR_TRUE);

	 mDisabled = disabled;
  }

  //nsIStyleContext* kidSC;

  nsCOMPtr<nsIStyleContext> outlineStyle(mStyleContext);
  nsCOMPtr<nsIAtom> outlineAtom (NS_NewAtom(":BUTTON-OUTLINE"));
  outlineStyle = aPresContext.ProbePseudoStyleContextFor(mContent, outlineAtom, mStyleContext);

  nsCOMPtr<nsIStyleContext> focusStyle(mStyleContext);
  nsCOMPtr<nsIAtom> focusAtom (NS_NewAtom(":BUTTON-FOCUS"));
  focusStyle = aPresContext.ProbePseudoStyleContextFor(mContent, focusAtom, mStyleContext);

  nsFormControlHelper::PaintRectangularButton(aPresContext,
                        aRenderingContext,
                        aDirtyRect, mRect.width, 
                        mRect.height,mPressed && mInside, mGotFocus, 
						nsFormFrame::GetDisabled(this),
                        mInside,
						outlineStyle,
						focusStyle,
                        mStyleContext, aLabel, this);
				
}

NS_IMETHODIMP
nsButtonControlFrame::HandleEvent(nsIPresContext& aPresContext, 
                                      nsGUIEvent* aEvent,
                                      nsEventStatus& aEventStatus)
{
 
#ifndef NS_GFX_RENDER_FORM_ELEMENTS
  return nsFormControlFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
#else

  // if disabled do nothing
  if (nsFormFrame::GetDisabled(this)) {
    return NS_OK;
  }

  // get parent with view
  nsIFrame *frame = nsnull;

  GetParentWithView(&frame);
  if (!frame)
	 return NS_OK;

  // get its view
  nsIView* view = nsnull;
  frame->GetView(&view);
  nsCOMPtr<nsIViewManager> viewMan;
  view->GetViewManager(*getter_AddRefs(viewMan));

  aEventStatus = nsEventStatus_eIgnore;
  nsresult result = NS_OK;
 
  switch (aEvent->message) {

        case NS_MOUSE_ENTER:
		  mInside = PR_TRUE;
		  if (mPressed)
			   mContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::kClass, "PRESSED", PR_TRUE);
          else
               mContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::kClass, "ROLLOVER", PR_TRUE);

	      break;
        case NS_MOUSE_LEFT_BUTTON_DOWN: 
          mGotFocus = PR_TRUE;
		  mPressed = PR_TRUE;
		  mContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::kClass, "PRESSED", PR_TRUE);

		  // grab all mouse events
		  
		  PRBool result;
		  viewMan->GrabMouseEvents(view,result);
		  break;

        case NS_MOUSE_LEFT_BUTTON_UP:
		    mPressed = PR_FALSE;
			mContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::kClass, "", PR_TRUE);

			// stop grabbing mouse events
            viewMan->GrabMouseEvents(nsnull,result);

			if (mInside)
			   MouseClicked(&aPresContext);

	        break;
        case NS_MOUSE_EXIT:
			mContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::kClass, "", PR_TRUE);
		    mInside = PR_FALSE;

		  // KLUDGE make is false when you exit because grabbing mouse events doesn't
		  // seem to work. If it did we would know when the mouse was released outside of
		  // us. And we could set this to false.
		  mPressed = PR_FALSE;
	      break;
  }

  aEventStatus = nsEventStatus_eConsumeNoDefault;

  return NS_OK;

#endif
}

void 
nsButtonControlFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
  mGotFocus = aOn;
  if (aRepaint) 
    Redraw();
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

