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
#include "nsIImage.h"
#include "nsHTMLImage.h"
#include "nsStyleUtil.h"
#include "nsDOMEvent.h"
#include "nsStyleConsts.h"
#include "nsIHTMLAttributes.h"
#include "nsGenericHTMLElement.h"
#include "nsFormFrame.h"
#include "nsIDocument.h"

static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kIButtonIID,      NS_IBUTTON_IID);

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
  if (this == (aSubmitter)) {
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
  } else if (NS_FORM_INPUT_IMAGE == type) {
    return 2;
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
  nsAutoString value;
  nsresult valResult = GetValue(&value);

  if ((NS_FORM_INPUT_IMAGE == type) || (NS_FORM_INPUT_BUTTON == type)) {
    char buf[20];
    aNumValues = 2;

    aValues[0].SetLength(0);
    sprintf(&buf[0], "%d", mLastClickPoint.x);
    aValues[0].Append(&buf[0]);

    aNames[0] = name;
    aNames[0].Append(".x");

    aValues[1].SetLength(0);
    sprintf(&buf[0], "%d", mLastClickPoint.y);
    aValues[1].Append(&buf[0]);

    aNames[1] = name;
    aNames[1].Append(".y");

    return PR_TRUE;
  }
  else if (((NS_FORM_INPUT_SUBMIT == type) || (NS_FORM_INPUT_HIDDEN == type)) &&
            (NS_CONTENT_ATTR_HAS_VALUE == valResult)) {
    aValues[0] = value;
    aNames[0]  = name;
    aNumValues = 1;
    return PR_TRUE;
  } else {
    aNumValues = 0;
    return PR_FALSE;
  }
}


nsresult
NS_NewButtonControlFrame(nsIContent* aContent,
                         nsIFrame*   aParent,
                         nsIFrame*&  aResult)
{
  aResult = new nsButtonControlFrame(aContent, aParent);
  if (nsnull == aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsButtonControlFrame::nsButtonControlFrame(nsIContent* aContent,
                                           nsIFrame* aParentFrame)
  : nsFormControlFrame(aContent, aParentFrame)
{
}

nsButtonControlFrame::~nsButtonControlFrame()
{
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
  //return NSIntPixelsToTwips(4, aPixToTwip);
#ifdef XP_PC
  return (nscoord)NSToIntRound((float)aInnerHeight * 0.25f);
#endif
#ifdef XP_UNIX
  return (nscoord)NSToIntRound((float)aInnerHeight * 0.50f);
#endif
}

nscoord 
nsButtonControlFrame::GetHorizontalInsidePadding(nsIPresContext& aPresContext,
                                                 float aPixToTwip, 
                                                 nscoord aInnerWidth,
                                                 nscoord aCharWidth) const
{
  nsCompatibility mode;
  aPresContext.GetCompatibilityMode(mode);

#ifdef XP_PC
  if (eCompatibility_NavQuirks == mode) {
    return (nscoord)NSToIntRound(float(aInnerWidth) * 0.25f);
  } else {
    return NSIntPixelsToTwips(10, aPixToTwip) + 8;
  }
#endif
#ifdef XP_UNIX
  if (eCompatibility_NavQuirks == mode) {
    return (nscoord)NSToIntRound(float(aInnerWidth) * 0.5f);
  } else {
    return NSIntPixelsToTwips(20, aPixToTwip);
  }
#endif
}

NS_METHOD 
nsButtonControlFrame::Paint(nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect)
{
  PRInt32 type;
  GetType(&type);

  const nsStyleDisplay* disp =
    (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);

  if (disp->mVisible) {
    // let super do processing if there is no image
    if (NS_FORM_INPUT_IMAGE != type) {
      return nsFormControlFrame::Paint(aPresContext, aRenderingContext, aDirtyRect);
    }

    // First paint background and borders
    nsFormControlFrame::Paint(aPresContext, aRenderingContext, aDirtyRect);

    nsIImage* image = mImageLoader.GetImage();
    if (nsnull == image) {
      // No image yet
      return NS_OK;
    }

    // Now render the image into our inner area (the area without the
    nsRect inner;
    GetInnerArea(&aPresContext, inner);
    aRenderingContext.DrawImage(image, inner);
  }

  return NS_OK;
}

void
nsButtonControlFrame::MouseClicked(nsIPresContext* aPresContext) 
{
  PRInt32 type;
  GetType(&type);
  if (nsnull != mFormFrame) {
    if (NS_FORM_INPUT_RESET == type) {
      //Send DOM event
      nsEventStatus mStatus;
      nsEvent mEvent;
      mEvent.eventStructType = NS_EVENT;
      mEvent.message = NS_FORM_RESET;
      mContent->HandleDOMEvent(*aPresContext, &mEvent, nsnull, DOM_EVENT_INIT, mStatus); 

      mFormFrame->OnReset();
    } 
    else if ((NS_FORM_INPUT_SUBMIT == type) || 
             ((NS_FORM_INPUT_IMAGE == type) && !nsFormFrame::GetDisabled(this))) {
      //Send DOM event
      nsEventStatus mStatus;
      nsEvent mEvent;
      mEvent.eventStructType = NS_EVENT;
      mEvent.message = NS_FORM_SUBMIT;
      mContent->HandleDOMEvent(*aPresContext, &mEvent, nsnull, DOM_EVENT_INIT, mStatus); 

      mFormFrame->OnSubmit(aPresContext, this);
    }
  } else if ((NS_FORM_BROWSE == type) && mFileControlFrame) {
    mFileControlFrame->MouseClicked(aPresContext);
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
    AddBordersAndPadding(&aPresContext, aDesiredSize);
    if (nsnull != aDesiredSize.maxElementSize) {
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

// XXX This is a copy of the code in nsImageFrame.cpp
static nsresult
UpdateImageFrame(nsIPresContext& aPresContext, nsIFrame* aFrame,
                 PRIntn aStatus)
{
  if (NS_IMAGE_LOAD_STATUS_SIZE_AVAILABLE & aStatus) {
    // Now that the size is available, trigger a content-changed reflow
    nsIContent* content = nsnull;
    aFrame->GetContent(content);
    if (nsnull != content) {
      nsIDocument* document = nsnull;
      content->GetDocument(document);
      if (nsnull != document) {
        document->ContentChanged(content, nsnull);
        NS_RELEASE(document);
      }
      NS_RELEASE(content);
    }
  }
  return NS_OK;
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
    if (NS_FORM_INPUT_IMAGE == type) { // there is an image
      // Setup url before starting the image load
      nsAutoString src;
      if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute("SRC", src)) {
        mImageLoader.SetURL(src);
      }
      mImageLoader.GetDesiredSize(aPresContext, aReflowState, this, UpdateImageFrame,
                                  aDesiredLayoutSize);
    } else {  // there is a widget
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
      CalculateSize(aPresContext, this, styleSize, spec, size, 
                    widthExplicit, heightExplicit, ignore,
                    aReflowState.rendContext);
      aDesiredLayoutSize.width = size.width;
      aDesiredLayoutSize.height= size.height;
    }
    aDesiredLayoutSize.ascent = aDesiredLayoutSize.height;
    aDesiredLayoutSize.descent = 0;
  }

  aDesiredWidgetSize.width = aDesiredLayoutSize.width;
  aDesiredWidgetSize.height= aDesiredLayoutSize.height;
}


void 
nsButtonControlFrame::PostCreateWidget(nsIPresContext* aPresContext, nscoord& aWidth, nscoord& aHeight)
{
  PRInt32 type;
  GetType(&type);

 	nsIButton* button = nsnull;
  if (mWidget && (NS_OK == mWidget->QueryInterface(kIButtonIID,(void**)&button))) {
    nsFont font(aPresContext->GetDefaultFixedFont()); 
    GetFont(aPresContext, font);
    mWidget->SetFont(font);
    SetColors(*aPresContext);

    nsString value;
    nsresult result = GetValue(&value);
  
    if (button != nsnull) {
	    if (NS_CONTENT_ATTR_HAS_VALUE == result) {  
	      button->SetLabel(value);
	    } else {
	      nsAutoString label;
	      GetDefaultLabel(label);
	      button->SetLabel(label);
	    }
	  }
    mWidget->Enable(!nsFormFrame::GetDisabled(this));
    NS_RELEASE(button);
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

