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

#include "nsGfxRadioControlFrame.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLParts.h"
#include "nsFormFrame.h"
#include "nsIFormControl.h"
#include "nsIContent.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsCSSRendering.h"
#include "nsIPresState.h"
#include "nsINameSpaceManager.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif
#include "nsIServiceManager.h"
#include "nsIDOMNode.h"


nsresult
NS_NewGfxRadioControlFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsGfxRadioControlFrame* it = new (aPresShell) nsGfxRadioControlFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsGfxRadioControlFrame::nsGfxRadioControlFrame()
{
   // Initialize GFX-rendered state
  mChecked = PR_FALSE;
  mRadioButtonFaceStyle = nsnull;
}

nsGfxRadioControlFrame::~nsGfxRadioControlFrame()
{
  NS_IF_RELEASE(mRadioButtonFaceStyle);
}

// Frames are not refcounted, no need to AddRef
NS_IMETHODIMP
nsGfxRadioControlFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsIRadioControlFrame))) {
    *aInstancePtr = (void*) ((nsIRadioControlFrame*) this);
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIStatefulFrame))) {
    *aInstancePtr = (void*) ((nsIStatefulFrame*) this);
    return NS_OK;
  }

  return nsFormControlFrame::QueryInterface(aIID, aInstancePtr);
}

#ifdef ACCESSIBILITY
NS_IMETHODIMP nsGfxRadioControlFrame::GetAccessible(nsIAccessible** aAccessible)
{
  nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");

  if (accService) {
    nsIAccessible* acc = nsnull;
    return accService->CreateHTMLRadioButtonAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
  }

  return NS_ERROR_FAILURE;
}
#endif

//--------------------------------------------------------------
NS_IMETHODIMP
nsGfxRadioControlFrame::GetAdditionalStyleContext(PRInt32 aIndex, 
                                                  nsIStyleContext** aStyleContext) const
{
  NS_PRECONDITION(nsnull != aStyleContext, "null OUT parameter pointer");
  if (aIndex < 0) {
    return NS_ERROR_INVALID_ARG;
  }
  *aStyleContext = nsnull;
  switch (aIndex) {
  case NS_GFX_RADIO_CONTROL_FRAME_FACE_CONTEXT_INDEX:
    *aStyleContext = mRadioButtonFaceStyle;
    NS_IF_ADDREF(*aStyleContext);
    break;
  default:
    return NS_ERROR_INVALID_ARG;
  }
  return NS_OK;
}

//--------------------------------------------------------------
NS_IMETHODIMP
nsGfxRadioControlFrame::SetAdditionalStyleContext(PRInt32 aIndex, 
                                                  nsIStyleContext* aStyleContext)
{
  if (aIndex < 0) {
    return NS_ERROR_INVALID_ARG;
  }
  switch (aIndex) {
  case NS_GFX_RADIO_CONTROL_FRAME_FACE_CONTEXT_INDEX:
    NS_IF_RELEASE(mRadioButtonFaceStyle);
    mRadioButtonFaceStyle = aStyleContext;
    NS_IF_ADDREF(aStyleContext);
    break;
  }
  return NS_OK;
}

//--------------------------------------------------------------
NS_IMETHODIMP nsGfxRadioControlFrame::SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsAReadableString& aValue)
{
  if (nsHTMLAtoms::checked == aName) {
    PRBool state = (aValue.Equals(NS_STRING_TRUE)) ? PR_TRUE : PR_FALSE;


    // if there is no form than the radiobtn is an orphan
    if (mFormFrame) {
      // if this failed then it didn't have a named radio group
      if (NS_FAILED(mFormFrame->OnRadioChecked(aPresContext, *this, state))) {
        // The line below is commented out to duplicate NavQuirks behavior
        //SetRadioState(aPresContext, state);
      }
    } else {
      SetRadioState(aPresContext, state);
    }
  }
  else {
    return nsFormControlFrame::SetProperty(aPresContext, aName, aValue);
  }

  return NS_OK;    
}

//--------------------------------------------------------------
NS_IMETHODIMP nsGfxRadioControlFrame::GetProperty(nsIAtom* aName, nsAWritableString& aValue)
{
  // Return the value of the property from the widget it is not null.
  // If is null, assume the widget is GFX-rendered and return a member variable instead.

  if (nsHTMLAtoms::checked == aName) {
	  nsFormControlHelper::GetBoolString(GetRadioState(), aValue);
  } else {
    return nsFormControlFrame::GetProperty(aName, aValue);
  }

  return NS_OK;    
}

//--------------------------------------------------------------
PRBool
nsGfxRadioControlFrame::GetChecked() 
{
  PRBool checked = PR_FALSE;
  GetCurrentCheckState(&checked);
  return(checked);
}

//--------------------------------------------------------------
PRBool
nsGfxRadioControlFrame::GetDefaultChecked() 
{
  PRBool checked = PR_FALSE;
  GetDefaultCheckState(&checked);
  return(checked);
}

//--------------------------------------------------------------
void
nsGfxRadioControlFrame::SetChecked(nsIPresContext* aPresContext, PRBool aValue, PRBool aSetInitialValue)
{
  if (aSetInitialValue) {
    if (aValue) {
      mContent->SetAttr(kNameSpaceID_HTML, nsHTMLAtoms::checked, NS_ConvertASCIItoUCS2("1"), PR_FALSE); // XXX should be "empty" value
    } else {
      mContent->SetAttr(kNameSpaceID_HTML, nsHTMLAtoms::checked, NS_ConvertASCIItoUCS2("0"), PR_FALSE);
    }
  }

  SetRadioState(aPresContext, aValue);
}

//--------------------------------------------------------------
NS_IMETHODIMP
nsGfxRadioControlFrame::SetRadioButtonFaceStyleContext(nsIStyleContext *aRadioButtonFaceStyleContext)
{
  mRadioButtonFaceStyle = aRadioButtonFaceStyleContext;
  NS_ADDREF(mRadioButtonFaceStyle);
  return NS_OK;
}

//--------------------------------------------------------------
NS_IMETHODIMP
nsGfxRadioControlFrame::GetRadioGroupSelectedContent(nsIContent ** aRadioBtn)
{
  NS_ENSURE_ARG_POINTER(aRadioBtn);
  nsFormFrame::GetRadioGroupSelectedContent(this, aRadioBtn);
  return NS_OK;
}


//--------------------------------------------------------------
PRBool
nsGfxRadioControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                    nsString* aValues, nsString* aNames)
{
  nsAutoString name;
  nsresult result = GetName(&name);
  if ((aMaxNumValues <= 0) || (NS_CONTENT_ATTR_HAS_VALUE != result)) {
    return PR_FALSE;
  }

  PRBool state = GetRadioState();

  if(PR_TRUE != state) {
    return PR_FALSE;
  }

  nsAutoString value;
  result = GetValue(&value);
  
  if (NS_CONTENT_ATTR_HAS_VALUE == result) {
    aValues[0] = value;
  } else {
    aValues[0].AssignWithConversion("on");
  }
  aNames[0]  = name;
  aNumValues = 1;

  return PR_TRUE;
}

//--------------------------------------------------------------
void 
nsGfxRadioControlFrame::Reset(nsIPresContext* aPresContext) 
{
  PRBool checked = PR_TRUE;
  GetDefaultCheckState(&checked);
  SetCurrentCheckState(checked);
}  

NS_IMETHODIMP
nsGfxRadioControlFrame::HandleEvent(nsIPresContext* aPresContext, 
                                          nsGUIEvent* aEvent,
                                          nsEventStatus* aEventStatus)
{
  // Check for user-input:none style
  const nsStyleUserInterface* uiStyle;
  GetStyleData(eStyleStruct_UserInterface,  (const nsStyleStruct *&)uiStyle);
  if (uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE || uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED)
    return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);

  // otherwise, do nothing. Events are handled by the DOM.
  return NS_OK;
}

//--------------------------------------------------------------
void
nsGfxRadioControlFrame::PaintRadioButton(nsIPresContext* aPresContext,
                                      nsIRenderingContext& aRenderingContext,
                                      const nsRect& aDirtyRect)
{
   
  PRBool checked = PR_TRUE;
  GetCurrentCheckState(&checked); // Get check state from the content model
  if (PR_TRUE == checked) {
    // Paint the button for the radio button using CSS background rendering code
   if (nsnull != mRadioButtonFaceStyle) {
     const nsStyleBackground* myColor = (const nsStyleBackground*)
          mRadioButtonFaceStyle->GetStyleData(eStyleStruct_Background);

     const nsStyleColor* color = (const nsStyleColor*)
          mRadioButtonFaceStyle->GetStyleData(eStyleStruct_Color);

     const nsStyleBorder* myBorder = (const nsStyleBorder*)
          mRadioButtonFaceStyle->GetStyleData(eStyleStruct_Border);
     const nsStylePosition* myPosition = (const nsStylePosition*)
          mRadioButtonFaceStyle->GetStyleData(eStyleStruct_Position);

     nscoord width = myPosition->mWidth.GetCoordValue();
     nscoord height = myPosition->mHeight.GetCoordValue();
       // Position the button centered within the radio control's rectangle.
     nscoord x = (mRect.width - width) / 2;
     nscoord y = (mRect.height - height) / 2;
     nsRect rect(x, y, width, height); 

     // So we will use the PaintBackground to paint the dot, 
     // but it uses the mBackgroundColor for painting and we need to use the mColor
     // so create a temporary style color struct and set it up appropriately
     nsStyleBackground tmpColor     = *myColor;
     tmpColor.mBackgroundColor = color->mColor;
     nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                        aDirtyRect, rect, tmpColor, *myBorder, 0, 0);
     nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                  aDirtyRect, rect, *myBorder, mRadioButtonFaceStyle, 0);
   }
  }
}

//--------------------------------------------------------------
NS_METHOD 
nsGfxRadioControlFrame::Paint(nsIPresContext*   aPresContext,
                           nsIRenderingContext& aRenderingContext,
                           const nsRect&        aDirtyRect,
                           nsFramePaintLayer    aWhichLayer,
                           PRUint32             aFlags)
{
  PRBool isVisible;
  if (NS_SUCCEEDED(IsVisibleForPainting(aPresContext, aRenderingContext, PR_TRUE, &isVisible)) && !isVisible) {
    return NS_OK;
  }
     // Paint the background
  nsFormControlFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);

  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    PaintRadioButton(aPresContext, aRenderingContext, aDirtyRect);
  }

  // Call to the base class to draw selection borders when appropriate
  return nsFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
}


//--------------------------------------------------------------
PRBool nsGfxRadioControlFrame::GetRadioState()
{
  return mChecked;
}

//--------------------------------------------------------------
void nsGfxRadioControlFrame::SetRadioState(nsIPresContext* aPresContext, PRBool aValue)
{
  mChecked = aValue;
	nsFormControlHelper::ForceDrawFrame(aPresContext, this);
}

void 
nsGfxRadioControlFrame::InitializeControl(nsIPresContext* aPresContext)
{
  nsFormControlFrame::InitializeControl(aPresContext);

   // set the widget to the initial state
  PRBool checked = PR_FALSE;
  nsresult result = GetDefaultCheckState(&checked);
  if (NS_CONTENT_ATTR_HAS_VALUE == result) {
    SetRadioState(aPresContext, checked);
  }
}

//----------------------------------------------------------------------
// nsIStatefulFrame
//----------------------------------------------------------------------
NS_IMETHODIMP
nsGfxRadioControlFrame::SaveState(nsIPresContext* aPresContext, nsIPresState** aState)
{
  NS_ENSURE_ARG_POINTER(aState);

  // Don't save state before we are initialized
  if (!mDidInit) {
    return NS_OK;
  }

  nsresult res = NS_OK;
  PRBool stateBool = GetRadioState();
  PRBool defaultStateBool = GetDefaultChecked();

  // Compare to default value, and only save if needed (Bug 62713)
  if (stateBool != defaultStateBool) {

    // Get the value string
    nsAutoString stateString;
    nsFormControlHelper::GetBoolString(stateBool, stateString);

    // Construct a pres state and store value in it.
    res = NS_NewPresState(aState);
    NS_ENSURE_SUCCESS(res, res);
    res = (*aState)->SetStateProperty(NS_LITERAL_STRING("checked"), stateString);
  }

  return res;
}
        


//----------------------------------------------------------------------
NS_IMETHODIMP
nsGfxRadioControlFrame::RestoreState(nsIPresContext* aPresContext, nsIPresState* aState)
{
  NS_ENSURE_ARG_POINTER(aState);

  if (!mDidInit) {
    mPresContext = aPresContext;
    InitializeControl(aPresContext);
    mDidInit = PR_TRUE;
  }

  mIsRestored = PR_TRUE;
  nsAutoString stateString;
  nsresult res = aState->GetStateProperty(NS_LITERAL_STRING("checked"), stateString);
  NS_ENSURE_SUCCESS(res, res);

  res = SetProperty(aPresContext, nsHTMLAtoms::checked, stateString);
  NS_ENSURE_SUCCESS(res, res);

  mRestoredChecked = mChecked;

  return NS_OK;
}


//----------------------------------------------------------------------
// Extra Debug Helper Methods
//----------------------------------------------------------------------
#ifdef DEBUG_rodsXXX
NS_IMETHODIMP 
nsGfxRadioControlFrame::Reflow(nsIPresContext*          aPresContext, 
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState, 
                               nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsGfxRadioControlFrame", aReflowState.reason);

  nsresult rv = nsNativeFormControlFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

  COMPARE_QUIRK_SIZE("nsGfxRadioControlFrame", 12, 11) 
  return rv;
}
#endif
