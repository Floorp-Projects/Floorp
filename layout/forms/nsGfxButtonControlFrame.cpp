/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsGfxButtonControlFrame.h"
#include "nsIButton.h"
#include "nsWidgetsCID.h"
#include "nsIFontMetrics.h"

static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kIButtonIID,      NS_IBUTTON_IID);

const nscoord kSuggestedNotSet = -1;

nsGfxButtonControlFrame::nsGfxButtonControlFrame()
{
  mRenderer.SetNameSpace(kNameSpaceID_None);
  mSuggestedWidth  = kSuggestedNotSet;
  mSuggestedHeight = kSuggestedNotSet;
}

PRBool
nsGfxButtonControlFrame::IsSuccessful(nsIFormControlFrame* aSubmitter)
{
  PRInt32 type;
  GetType(&type);
  if ((NS_FORM_INPUT_HIDDEN == type) || (this == aSubmitter)) {
     // Can not use the nsHTMLButtonControlFrame::IsSuccessful because
     // it will fail it's test of (this==aSubmitter)
    nsAutoString name;
    return (NS_CONTENT_ATTR_HAS_VALUE == GetName(&name));
  } else {
    return PR_FALSE;
  }
}

PRInt32
nsGfxButtonControlFrame::GetMaxNumValues() 
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
nsGfxButtonControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
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
    GetValue(&value);
    aValues[0] = value;
    aNames[0]  = name;
    aNumValues = 1;
    return PR_TRUE;
  }
}

nsresult
NS_NewGfxButtonControlFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsGfxButtonControlFrame* it = new nsGfxButtonControlFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}
      

PRBool
nsGfxButtonControlFrame::IsReset(PRInt32 type)
{
  if (NS_FORM_INPUT_RESET == type) {
    return PR_TRUE;
  } else {
    return PR_FALSE;
  }
}

PRBool
nsGfxButtonControlFrame::IsSubmit(PRInt32 type)
{
  if (NS_FORM_INPUT_SUBMIT == type) {
    return PR_TRUE;
  } else {
    return PR_FALSE;
  }
}
                              
const nsIID&
nsGfxButtonControlFrame::GetIID()
{
  static NS_DEFINE_IID(kButtonIID, NS_IBUTTON_IID);
  return kButtonIID;
}
  
const nsIID&
nsGfxButtonControlFrame::GetCID()
{
  static NS_DEFINE_IID(kButtonCID, NS_BUTTON_CID);
  return kButtonCID;
}

#ifdef DEBUG
NS_IMETHODIMP
nsGfxButtonControlFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("ButtonControl", aResult);
}
#endif

NS_IMETHODIMP 
nsGfxButtonControlFrame::AddComputedBorderPaddingToDesiredSize(nsHTMLReflowMetrics& aDesiredSize,
                                                               const nsHTMLReflowState& aSuggestedReflowState)
{
  if (kSuggestedNotSet == mSuggestedWidth) {
    aDesiredSize.width  += aSuggestedReflowState.mComputedBorderPadding.left + aSuggestedReflowState.mComputedBorderPadding.right;
  }

  if (kSuggestedNotSet == mSuggestedHeight) {
    aDesiredSize.height += aSuggestedReflowState.mComputedBorderPadding.top + aSuggestedReflowState.mComputedBorderPadding.bottom;
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsGfxButtonControlFrame::DoNavQuirksReflow(nsIPresContext*          aPresContext, 
                                           nsHTMLReflowMetrics&     aDesiredSize,
                                           const nsHTMLReflowState& aReflowState, 
                                           nsReflowStatus&          aStatus)
{
  nsIFrame* firstKid = mFrames.FirstChild();

  nsCOMPtr<nsIFontMetrics> fontMet;
  nsFormControlHelper::GetFrameFontFM(aPresContext, (nsIFormControlFrame *)this, getter_AddRefs(fontMet));
  nsSize desiredSize;
  if (fontMet) {
    aReflowState.rendContext->SetFont(fontMet);

    // Get the text from the "value" attribute 
    // for measuring the height, width of the text
    // if there is no value attr or its length is 0
    // then go to the generated content to get it
    nsAutoString value;
    GetValue(&value);

    // if there was no value specified 
    // then go get the content of the generated content
    // we can't make any assumption as to what the default would be
    // because of the value is specified in the html.css and for
    // for non-english platforms it might not be the string 
    // "Reset" or "Submit Query"
    if (value.Length() == 0) {
      value = "  ";
      // The child frame will br the generated content
      nsIFrame* fKid;
      firstKid->FirstChild(nsnull, &fKid);
      if (fKid) {
        const nsStyleContent* content;
        fKid->GetStyleData(eStyleStruct_Content,  (const nsStyleStruct *&)content);
        PRUint32 count = content->ContentCount();
        // XXX not sure if I need to get more than the first index?
        if (count > 0) {
          nsStyleContentType contentType = eStyleContentType_String;
          content->GetContentAt(0, contentType, value);
        }
      }
    }

    nsInputDimensionSpec btnSpec(NULL, PR_FALSE, nsnull, 
                                  &value,0, 
                                  PR_FALSE, NULL, 1);
    nsFormControlHelper::CalcNavQuirkSizing(aPresContext, aReflowState.rendContext, 
                                            fontMet, (nsIFormControlFrame*)this, 
                                            btnSpec, desiredSize);

    // This calculates the reflow size
    // get the css size and let the frame use or override it
    nsSize styleSize;
    nsFormControlFrame::GetStyleSize(aPresContext, aReflowState, styleSize);

    if (CSS_NOTSET != styleSize.width) {  // css provides width
      NS_ASSERTION(styleSize.width >= 0, "form control's computed width is < 0"); 
      if (NS_INTRINSICSIZE != styleSize.width) {
        desiredSize.width = styleSize.width;
        desiredSize.width  += aReflowState.mComputedBorderPadding.top + aReflowState.mComputedBorderPadding.bottom;
      }
    }

    if (CSS_NOTSET != styleSize.height) {  // css provides height
      NS_ASSERTION(styleSize.height > 0, "form control's computed height is <= 0"); 
      if (NS_INTRINSICSIZE != styleSize.height) {
        desiredSize.height = styleSize.height;
        desiredSize.height += aReflowState.mComputedBorderPadding.left + aReflowState.mComputedBorderPadding.right;
      }
    }

    aDesiredSize.width   = desiredSize.width;
    aDesiredSize.height  = desiredSize.height;
    aDesiredSize.ascent  = aDesiredSize.height;
    aDesiredSize.descent = 0;
  } else {
    // XXX ASSERT HERE
    desiredSize.width = 0;
    desiredSize.height = 0;
  }

  // get border and padding
  /*const nsStyleSpacing* spacing;
  GetStyleData(eStyleStruct_Spacing,  (const nsStyleStruct *&)spacing);
  nsMargin borderPadding;
  borderPadding.SizeTo(0, 0, 0, 0);
  spacing->CalcBorderPaddingFor(this, borderPadding);
*/
  // remove it from the the desired size
  // because the content need to fit inside of it
  desiredSize.width  -= (aReflowState.mComputedBorderPadding.left + aReflowState.mComputedBorderPadding.right);
  desiredSize.height -= (aReflowState.mComputedBorderPadding.top + aReflowState.mComputedBorderPadding.bottom);

  // now reflow the first child (genertaed content)
  nsHTMLReflowState reflowState(aPresContext, aReflowState, firstKid, desiredSize);
  reflowState.mComputedWidth  = desiredSize.width;
  reflowState.mComputedHeight = desiredSize.height;
  // XXX Proper handling of incremental reflow...
  if (eReflowReason_Incremental == aReflowState.reason) {
    nsIFrame* targetFrame;

    // See if it's targeted at us
    aReflowState.reflowCommand->GetTarget(targetFrame);
    if (this == targetFrame) {
      Invalidate(aPresContext, nsRect(0,0,mRect.width,mRect.height), PR_FALSE);

      nsIReflowCommand::ReflowType  reflowType;
      aReflowState.reflowCommand->GetType(reflowType);
      if (nsIReflowCommand::StyleChanged == reflowType) {
        reflowState.reason = eReflowReason_StyleChange;
      }
      else {
        reflowState.reason = eReflowReason_Resize;
      }
    } else {
      nsIFrame* nextFrame;

      // Remove the next frame from the reflow path
      aReflowState.reflowCommand->GetNext(nextFrame);  
    }
  }

  nsHTMLReflowMetrics childReflowMetrics(aDesiredSize);
  nsRect kidRect;
  firstKid->GetRect(kidRect);
  ReflowChild(firstKid, aPresContext, childReflowMetrics, reflowState, kidRect.x, kidRect.y, 0, aStatus);

  // Now do the reverse calculation of the 
  // NavQuirks button to get the size of the text
  nscoord textWidth  = (2 * aDesiredSize.width) / 3;
  nscoord textHeight = (2 * aDesiredSize.height) / 3;

  // Center the child and add back in the border and badding
  nsRect rect = nsRect(((desiredSize.width - textWidth)/2)+ aReflowState.mComputedBorderPadding.left, 
                       ((desiredSize.height - textHeight)/2) + aReflowState.mComputedBorderPadding.top, 
                       desiredSize.width, 
                       desiredSize.height);
  firstKid->SetRect(aPresContext, rect);

  return NS_OK;
}

NS_IMETHODIMP 
nsGfxButtonControlFrame::Reflow(nsIPresContext*          aPresContext, 
                                nsHTMLReflowMetrics&     aDesiredSize,
                                const nsHTMLReflowState& aReflowState, 
                                nsReflowStatus&          aStatus)
{
   // The mFormFrame is set in the initial reflow within nsHTMLButtonControlFrame
  nsresult rv = NS_OK;
  if (!mFormFrame && (eReflowReason_Initial == aReflowState.reason)) {
    nsFormFrame::AddFormControlFrame(aPresContext, *NS_STATIC_CAST(nsIFrame*, this));
  }

  if ((kSuggestedNotSet != mSuggestedWidth) || 
      (kSuggestedNotSet != mSuggestedHeight)) {
    nsHTMLReflowState suggestedReflowState(aReflowState);

      // Honor the suggested width and/or height.
    if (kSuggestedNotSet != mSuggestedWidth) {
      suggestedReflowState.mComputedWidth = mSuggestedWidth;
    }

    if (kSuggestedNotSet != mSuggestedHeight) {
      suggestedReflowState.mComputedHeight = mSuggestedHeight;
    }

    rv = nsHTMLButtonControlFrame::Reflow(aPresContext, aDesiredSize, suggestedReflowState, aStatus);

  } else { // Normal reflow.

    nsCompatibility mode;
    aPresContext->GetCompatibilityMode(&mode);

    if (mode == eCompatibility_NavQuirks) {
      // Do NavQuirks Sizing and layout
      rv = DoNavQuirksReflow(aPresContext, aDesiredSize, aReflowState, aStatus);   
    } else {
      // Do Standard mode sizing and layout
      rv = nsHTMLButtonControlFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);
    }
  }

#ifdef DEBUG_rodsXXXX
  COMPARE_QUIRK_SIZE("nsGfxButtonControlFrame", 84, 24) // with the text "Press Me" in it
#endif
  aStatus = NS_FRAME_COMPLETE;

  return rv;
}

NS_IMETHODIMP 
nsGfxButtonControlFrame::SetSuggestedSize(nscoord aWidth, nscoord aHeight)
{
  mSuggestedWidth = aWidth;
  mSuggestedHeight = aHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsGfxButtonControlFrame::HandleEvent(nsIPresContext* aPresContext, 
                                      nsGUIEvent*     aEvent,
                                      nsEventStatus*  aEventStatus)
{
  // Override the HandleEvent to prevent the nsFrame::HandleEvent
  // from being called. The nsFrame::HandleEvent causes the button label
  // to be selected (Drawn with an XOR rectangle over the label)
 
  // if disabled do nothing
  if (mRenderer.isDisabled()) {
    return NS_OK;
  }

   // lets see if the button was clicked
  switch (aEvent->message) {
     case NS_MOUSE_LEFT_CLICK:
        MouseClicked(aPresContext);
     break;
  }

  return NS_OK;

}


