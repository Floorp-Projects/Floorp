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
#include "nsStyleUtil.h"
#include "nsDOMEvent.h"
#include "nsStyleConsts.h"
#include "nsIHTMLAttributes.h"
#include "nsGenericHTMLElement.h"
#include "nsFormFrame.h"

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


NS_IMETHODIMP
nsButtonControlFrame::AttributeChanged(nsIPresContext* aPresContext,
                                       nsIContent*     aChild,
                                       nsIAtom*        aAttribute,
                                       PRInt32         aHint)
{
  nsresult result = NS_OK;
  PRInt32 type;
  GetType(&type);
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
  }
  return result;
}
                                     

NS_METHOD 
nsButtonControlFrame::Paint(nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect)
{
#ifdef XP_PC
  PaintButton(aPresContext, aRenderingContext, aDirtyRect);
#else
  PRInt32 type;
  GetType(&type);

  const nsStyleDisplay* disp =
    (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);

  if (disp->mVisible) {
    return nsFormControlFrame::Paint(aPresContext, aRenderingContext, aDirtyRect);
  }
#endif
 
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
    mFormFrame->GetContent(formContent);

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
  else if ((NS_FORM_BROWSE == type) && mFileControlFrame) {
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
    nsMargin bp;
    AddBordersAndPadding(&aPresContext, aReflowState, aDesiredSize, bp);
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

NS_IMETHODIMP
nsButtonControlFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("ButtonControl", aResult);
}

//
// XXX: The following paint code is TEMPORARY. It is being used to get printing working
// under windows. Later it may be used to GFX-render the controls to the display. 
// Expect this code to repackaged and moved to a new location in the future.
//

void 
nsButtonControlFrame::PaintButton(nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect)
{
#ifdef XP_PC
    aRenderingContext.PushState();
    nsFormControlFrame::Paint(aPresContext, aRenderingContext, aDirtyRect);

    nsString label;
    nsresult result = GetValue(&label);

    const nsStyleSpacing* spacing =
      (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
    nsMargin border;
    spacing->CalcBorderFor(this, border);

    float p2t;
    aPresContext.GetScaledPixelsToTwips(p2t);
    nscoord onePixel = NSIntPixelsToTwips(1, p2t);

    nsRect outside(0, 0, mRect.width, mRect.height);
    outside.Deflate(border);
    outside.Deflate(onePixel, onePixel);

    nsRect inside(outside);
    inside.Deflate(onePixel, onePixel);

    //XXX: This will be used later, when rendering to the screen. For printing we don't bother
    //printing the focus dashed line for now.
#if 0
  if (mGotFocus) { // draw dashed line to indicate selection, XXX don't calc rect every time
    PRUint8 borderStyles[4];
    nscolor borderColors[4];
    nscolor black = NS_RGB(0,0,0);
    for (PRInt32 i = 0; i < 4; i++) {
      borderStyles[i] = NS_STYLE_BORDER_STYLE_DOTTED;
      borderColors[i] = black;
    }
    nsCSSRendering::DrawDashedSides(0, aRenderingContext, borderStyles, borderColors, outside,
                                    inside, PR_FALSE, nsnull);
  }
#endif

  float appUnits;
  float devUnits;
  float scale;
  nsIDeviceContext * context;
  aRenderingContext.GetDeviceContext(context);

  context->GetCanonicalPixelScale(scale);
  context->GetAppUnitsToDevUnits(devUnits);
  context->GetDevUnitsToAppUnits(appUnits);

  //aRenderingContext.SetColor(NS_RGB(192,192,192));
  //aRenderingContext.FillRect(inside);

  aRenderingContext.SetColor(NS_RGB(0,0,0));
 
  nsFont font(aPresContext.GetDefaultFixedFont()); 
  GetFont(&aPresContext, font);

  aRenderingContext.SetFont(font);

  nscoord textWidth;
  nscoord textHeight;
  aRenderingContext.GetWidth(label, textWidth);

  nsIFontMetrics* metrics;
  context->GetMetricsFor(font, metrics);
  metrics->GetMaxAscent(textHeight);

  nscoord x = ((inside.width  - textWidth) / 2)  + inside.x;
  nscoord y = ((inside.height - textHeight) / 2) + inside.y;
  if (mLastMouseState == eMouseDown) {
    x += onePixel;
    y += onePixel;
  }

  aRenderingContext.DrawString(label, x, y, 0);
  NS_RELEASE(context);
  PRBool clipEmpty;
  aRenderingContext.PopState(clipEmpty);

#endif
}

