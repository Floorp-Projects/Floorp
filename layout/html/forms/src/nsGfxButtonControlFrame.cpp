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
#include "nsFormControlFrame.h"
#include "nsISupportsArray.h"

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
  PRBool successful = PR_TRUE;
  if ((NS_FORM_INPUT_HIDDEN == type) || (this == aSubmitter)) {
     // Can not use the nsHTMLButtonControlFrame::IsSuccessful because
     // it will fail it's test of (this==aSubmitter)
    nsAutoString name;
    PRBool disabled = PR_FALSE;
    nsFormControlHelper::GetDisabled(mContent, &disabled);
    successful = !disabled && (NS_CONTENT_ATTR_HAS_VALUE == GetName(&name));
  } else {
    successful = PR_FALSE;
  }
  return successful;
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
NS_NewGfxButtonControlFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsGfxButtonControlFrame* it = new (aPresShell) nsGfxButtonControlFrame;
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
                              
// Special check for the browse button of a file input.
//
// Since this is actually type "NS_FORM_INPUT_BUTTON", we
// can't tell if it is part of a file input by the type.
// We'll return PR_TRUE if either:
// (a) type is NS_FORM_BROWSE or
// (b) type is NS_FORM_INPUT_BUTTON and our parent is a file input
PRBool
nsGfxButtonControlFrame::IsBrowse(PRInt32 type)
{
  PRBool rv = PR_FALSE;
  if (NS_FORM_BROWSE == type) {
    rv = PR_TRUE;
  }
  else if (NS_FORM_INPUT_BUTTON == type) {
  
    // Check to see if parent is a file input
    nsresult result;
    nsCOMPtr<nsIContent> parentContent;
    result = mContent->GetParent(*getter_AddRefs(parentContent));
    if (NS_SUCCEEDED(result) && parentContent) {
      nsCOMPtr<nsIAtom> atom;
      result = parentContent->GetTag(*getter_AddRefs(atom));
      if (NS_SUCCEEDED(result) && atom) {
        if (atom.get() == nsHTMLAtoms::input) {

          // It's an input, is it a file input?
          nsAutoString value;
          if (NS_CONTENT_ATTR_HAS_VALUE == parentContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::type, value)) {
            if (value.EqualsIgnoreCase("file")) {
              rv = PR_TRUE;
            }
          }
        }
      }
    }
  }
  return rv;
}

/*
 * FIXME: this ::GetIID() method has no meaning in life and should be
 * removed.
 * Pierre Phaneuf <pp@ludusdesign.com>
 */
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
  nsresult res = nsFormControlHelper::GetFrameFontFM(aPresContext, (nsIFormControlFrame *)this, getter_AddRefs(fontMet));
  nsSize desiredSize;
  if (NS_SUCCEEDED(res) && fontMet) {
    aReflowState.rendContext->SetFont(fontMet);

    // Get the text from the "value" attribute 
    // for measuring the height, width of the text
    nsAutoString value;
    GetValue(&value);

    if (value.Length() == 0) {
      // Generate localized label.
      // We can't make any assumption as to what the default would be
      // because the value is localized for non-english platforms, thus
      // it might not be the string "Reset", "Submit Query", or "Browse..."
      res = GetDefaultLabel(value);
      if (NS_FAILED(res)) {
        return res;
      }
    }

    const nsStyleText* textStyle;
    GetStyleData(eStyleStruct_Text,  (const nsStyleStruct *&)textStyle);
    if (!textStyle->WhiteSpaceIsSignificant()) {
      value.CompressWhitespace();
      if (value.Length() == 0) {
        value.AssignWithConversion("  ");
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
  //nscoord textWidth  = (2 * aDesiredSize.width) / 3;
  nscoord textHeight = (2 * aDesiredSize.height) / 3;

  // Center the child and add back in the border and badding
  // our inner area frame is already doing centering so we only need to center vertically.
  nsRect rect = nsRect(aReflowState.mComputedBorderPadding.left, 
                       ((desiredSize.height - textHeight)/2) + aReflowState.mComputedBorderPadding.top, 
                       desiredSize.width, 
                       desiredSize.height);
  firstKid->SetRect(aPresContext, rect);

  return NS_OK;
}

// Create the text content used as label for the button.
// The frame will be generated by the frame constructor.
NS_IMETHODIMP
nsGfxButtonControlFrame::CreateAnonymousContent(nsIPresContext* aPresContext,
                                                nsISupportsArray& aChildList)
{
  nsresult result;

  // Get the text from the "value" attribute.
  // If it is zero length, set it to a default value (localized)
  nsAutoString initvalue, value;
  GetValue(&initvalue);
  value = initvalue;
  if (value.Length() == 0) {
    // Generate localized label.
    // We can't make any assumption as to what the default would be
    // because the value is localized for non-english platforms, thus
    // it might not be the string "Reset", "Submit Query", or "Browse..."
    result = GetDefaultLabel(value);
    if (NS_FAILED(result)) {
      return result;
    }
  }

  // Compress whitespace out of label if needed.
  const nsStyleText* textStyle;
  GetStyleData(eStyleStruct_Text,  (const nsStyleStruct *&)textStyle);
  if (!textStyle->WhiteSpaceIsSignificant()) {
    value.CompressWhitespace();
    if (value.Length() == 0) {
      value.AssignWithConversion("  ");
    }
  }

  // Add a child text content node for the label
  nsCOMPtr<nsIContent> labelContent;
  result = NS_NewTextNode(getter_AddRefs(labelContent));
  if (NS_SUCCEEDED(result) && labelContent) {
    // set the value of the text node and add it to the child list
    mTextContent = do_QueryInterface(labelContent, &result);
    if (NS_SUCCEEDED(result) && mTextContent) {
      mTextContent->SetText(value.GetUnicode(), value.Length(), PR_TRUE);
      aChildList.AppendElement(mTextContent);
    }
  }
  return result;
}

// Create the text content used as label for the button.
// The frame will be generated by the frame constructor.
NS_IMETHODIMP
nsGfxButtonControlFrame::CreateFrameFor(nsIPresContext*   aPresContext,
                                        nsIContent *      aContent,
                                        nsIFrame**        aFrame)
{
  nsIFrame * newFrame = nsnull;
  nsresult rv = NS_ERROR_FAILURE;

  if (aFrame) 
    *aFrame = nsnull; 

  nsCOMPtr<nsIContent> content(do_QueryInterface(mTextContent));
  if (aContent == content.get()) {
    nsCOMPtr<nsIPresShell> shell;
    aPresContext->GetShell(getter_AddRefs(shell));

    nsIFrame * parentFrame = mFrames.FirstChild();
    nsCOMPtr<nsIStyleContext> styleContext;
    parentFrame->GetStyleContext(getter_AddRefs(styleContext));

    rv = NS_NewTextFrame(shell, &newFrame);
    if (NS_FAILED(rv)) { return rv; }
    if (!newFrame)   { return NS_ERROR_NULL_POINTER; }
    nsCOMPtr<nsIStyleContext> textStyleContext;
    rv = aPresContext->ResolvePseudoStyleContextFor(content, 
                                                    nsHTMLAtoms::textPseudo,
                                                    styleContext,
                                                    PR_FALSE,
                                                    getter_AddRefs(textStyleContext));
    if (NS_FAILED(rv))     { return rv; }
    if (!textStyleContext) { return NS_ERROR_NULL_POINTER; }

    if (styleContext) {
      // initialize the text frame
      newFrame->Init(aPresContext, content, parentFrame, textStyleContext, nsnull);
      newFrame->SetInitialChildList(aPresContext, nsnull, nsnull);
      rv = NS_OK;
    }
  }

  if (aFrame) {
    *aFrame = newFrame;
  }
  return rv;
}

// Frames are not refcounted, no need to AddRef
NS_IMETHODIMP
nsGfxButtonControlFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  } else if (aIID.Equals(NS_GET_IID(nsIAnonymousContentCreator))) {
    *aInstancePtr = (void*)(nsIAnonymousContentCreator*) this;
    return NS_OK;
  }
  return nsHTMLButtonControlFrame::QueryInterface(aIID, aInstancePtr);
}

// Initially we hardcoded the default strings here.
// Next, we used html.css to store the default label for various types
// of buttons. (nsGfxButtonControlFrame::DoNavQuirksReflow rev 1.20)
// However, since html.css is not internationalized, we now grab the default
// label from a string bundle as is done for all other UI strings.
// See bug 16999 for further details.
NS_IMETHODIMP
nsGfxButtonControlFrame::GetDefaultLabel(nsString& aString) 
{
  nsresult rv = NS_OK;
  PRInt32 type;
  GetType(&type);
  if (IsReset(type)) {
    rv = nsFormControlHelper::GetLocalizedString("Reset", aString);
  } 
  else if (IsSubmit(type)) {
    rv = nsFormControlHelper::GetLocalizedString("Submit", aString);
  } 
  else if (IsBrowse(type)) {
    rv = nsFormControlHelper::GetLocalizedString("Browse", aString);
  }
  else {
    aString.AssignWithConversion("  ");
    rv = NS_OK;
  }
  return rv;
}

NS_IMETHODIMP
nsGfxButtonControlFrame::AttributeChanged(nsIPresContext* aPresContext,
                                       nsIContent*     aChild,
                                       PRInt32         aNameSpaceID,
                                       nsIAtom*        aAttribute,
                                       PRInt32         aHint)
{
  nsresult rv = NS_OK;

  // If the value attribute is set, update the text of the label
  if (nsHTMLAtoms::value == aAttribute) {
    nsAutoString value;
    if (mTextContent && mContent) {
      if (NS_CONTENT_ATTR_HAS_VALUE != mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::value, value)) {
        value.SetLength(0);
      }
      rv = mTextContent->SetText(value.GetUnicode(), value.Length(), PR_TRUE);
    } else {
      rv = NS_ERROR_UNEXPECTED;
    }

  // defer to HTMLButtonControlFrame
  } else {
    rv = nsHTMLButtonControlFrame::AttributeChanged(aPresContext, aChild, aNameSpaceID, aAttribute, aHint);
  }
  return rv;
}

NS_IMETHODIMP 
nsGfxButtonControlFrame::Reflow(nsIPresContext*          aPresContext, 
                                nsHTMLReflowMetrics&     aDesiredSize,
                                const nsHTMLReflowState& aReflowState, 
                                nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsGfxButtonControlFrame", aReflowState.reason);

   // The mFormFrame is set in the initial reflow within nsHTMLButtonControlFrame
  nsresult rv = NS_OK;
  if (eReflowReason_Initial == aReflowState.reason) {
    if (!mFormFrame) {
      nsFormFrame::AddFormControlFrame(aPresContext, *NS_STATIC_CAST(nsIFrame*, this));
    }
  }

#if 0
  nsresult skiprv = nsFormControlFrame::SkipResizeReflow(mCacheSize, mCachedMaxElementSize, aPresContext, 
                                                         aDesiredSize, aReflowState, aStatus);

  if (NS_SUCCEEDED(skiprv)) {
    return skiprv;
  }
#endif

  PRInt32 type;
  GetType(&type);
  // hidden inputs are zero width/height and are finished here
  if (NS_FORM_INPUT_HIDDEN == type) {
    aDesiredSize.width = aDesiredSize.height = aDesiredSize.ascent = aDesiredSize.descent = 0;
    if (aDesiredSize.maxElementSize) {
      aDesiredSize.maxElementSize->width = aDesiredSize.maxElementSize->height = 0;
    }
    // just in case the hidden input is the target of an incremental reflow 
    if (eReflowReason_Incremental == aReflowState.reason) {
      nsIFrame* targetFrame;
      aReflowState.reflowCommand->GetTarget(targetFrame);
      if (this == targetFrame) {
        nsIFrame* nextFrame;
        // Remove the next frame from the reflow path
        aReflowState.reflowCommand->GetNext(nextFrame); 
      }
    }
    aStatus = NS_FRAME_COMPLETE;   
    return rv;
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
      // nsHTMLButtonControlFrame::Reflow registers it for Standard Mode
      // and sets up mPresContext
      if (eReflowReason_Initial == aReflowState.reason) {
        nsFormControlFrame::RegUnRegAccessKey(aPresContext, NS_STATIC_CAST(nsIFrame*, this), PR_TRUE);
      }
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

  nsFormControlFrame::SetupCachedSizes(mCacheSize, mCachedMaxElementSize, aDesiredSize);

  if (aDesiredSize.maxElementSize != nsnull) {
    aDesiredSize.maxElementSize->width  = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }


  return rv;
}

NS_IMETHODIMP 
nsGfxButtonControlFrame::SetSuggestedSize(nscoord aWidth, nscoord aHeight)
{
  mSuggestedWidth = aWidth;
  mSuggestedHeight = aHeight;
  //mState |= NS_FRAME_IS_DIRTY;
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
 
  // Do nothing here, nsHTMLInputElement::HandleDOMEvent
  // takes cares of calling MouseClicked for us.

  // do we have user-input style?
  const nsStyleUserInterface* uiStyle;
  GetStyleData(eStyleStruct_UserInterface,  (const nsStyleStruct *&)uiStyle);
  if (uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE || uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED)
    return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
  
  return NS_OK;
}


