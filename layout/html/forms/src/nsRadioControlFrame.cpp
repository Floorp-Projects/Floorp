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

#include "nsRadioControlFrame.h"
#include "nsIRadioButton.h"
#include "nsNativeFormControlFrame.h"
#include "nsWidgetsCID.h"
#include "nsIContent.h"
#include "nsHTMLAtoms.h"
#include "nsFormFrame.h"
#include "nsINameSpaceManager.h"
#include "nsFormFrame.h"
#include "nsIStatefulFrame.h"
#include "nsISupportsPrimitives.h"
#include "nsIComponentManager.h"

static NS_DEFINE_IID(kIRadioControlFrameIID,  NS_IRADIOCONTROLFRAME_IID);

// Frames are not refcounted, no need to AddRef
NS_IMETHODIMP
nsRadioControlFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIRadioControlFrameIID)) {
    *aInstancePtr = (void*) ((nsIRadioControlFrame*) this);
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIStatefulFrame))) {
    *aInstancePtr = (void*) ((nsIStatefulFrame*) this);
    return NS_OK;
  }
 
  return nsNativeFormControlFrame::QueryInterface(aIID, aInstancePtr);
}

/*
 * FIXME: this ::GetIID() method has no meaning in life and should be
 * removed.
 * Pierre Phaneuf <pp@ludusdesign.com>
 */
const nsIID&
nsRadioControlFrame::GetIID()
{
	static NS_DEFINE_IID(kIRadioIID, NS_IRADIOBUTTON_IID);
  return kIRadioIID;
}
  
const nsIID&
nsRadioControlFrame::GetCID()
{
  static NS_DEFINE_IID(kRadioCID, NS_RADIOBUTTON_CID);
  return kRadioCID;
}


void 
nsRadioControlFrame::PostCreateWidget(nsIPresContext* aPresContext, nscoord& aWidth, nscoord& aHeight)
{
   // set the widget to the initial state
  PRBool checked = PR_FALSE;
  nsresult result = GetDefaultCheckState(&checked);
  if (NS_CONTENT_ATTR_HAS_VALUE == result) {
    if (PR_TRUE == checked)
      SetRadioControlFrameState(aPresContext, NS_STRING_TRUE);
    else
      SetRadioControlFrameState(aPresContext, NS_STRING_FALSE);
  }
}

void 
nsRadioControlFrame::MouseUp(nsIPresContext* aPresContext) 
{
  SetRadioControlFrameState(aPresContext, NS_STRING_TRUE);
  
  if (mFormFrame) {
     // The form frame will determine which radio button needs
     // to be turned off and will call SetChecked on the
     // nsRadioControlFrame to unset the checked state
    mFormFrame->OnRadioChecked(aPresContext, *this);
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
nsRadioControlFrame::SetChecked(nsIPresContext* aPresContext, PRBool aValue, PRBool aSetInitialValue)
{
  if (aSetInitialValue) {
    if (aValue) {
      mContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::checked, nsAutoString("1"), PR_FALSE); // XXX should be "empty" value
    } else {
      mContent->SetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::checked, nsAutoString("0"), PR_FALSE);
    }
  }

  if (PR_TRUE == aValue) {
    SetRadioControlFrameState(aPresContext, NS_STRING_TRUE);
  } else {
    SetRadioControlFrameState(aPresContext, NS_STRING_FALSE);
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

  PRBool state = GetRadioState();

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
nsRadioControlFrame::Reset(nsIPresContext* aPresContext) 
{
  PRBool checked = PR_TRUE;
  GetDefaultCheckState(&checked);
  SetCurrentCheckState(checked);
}  

#ifdef DEBUG
NS_IMETHODIMP
nsRadioControlFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("RadioControl", aResult);
}
#endif

NS_IMETHODIMP
nsRadioControlFrame::SetRadioButtonFaceStyleContext(nsIStyleContext *aRadioButtonFaceStyleContext)
{
 /* for gfx widgets only */
 return NS_OK;
}


NS_METHOD 
nsRadioControlFrame::HandleEvent(nsIPresContext* aPresContext, 
                                 nsGUIEvent* aEvent,
                                 nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  if (nsFormFrame::GetDisabled(this)) { 
    return NS_OK;
  }

  switch(aEvent->message) {
    case NS_KEY_PRESS:
      if (NS_KEY_EVENT == aEvent->eventStructType) {
        nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
        if (NS_VK_SPACE == keyEvent->keyCode || NS_VK_RETURN == keyEvent->keyCode) {
          MouseClicked(aPresContext);
        }
      }
      break;
    case NS_MOUSE_LEFT_BUTTON_UP:
       MouseUp(aPresContext);
     break;

    
    default:
      break;
  }

  return(nsNativeFormControlFrame::HandleEvent(aPresContext, aEvent, aEventStatus));
}

void nsRadioControlFrame::GetRadioControlFrameState(nsString& aValue)
{
	nsFormControlHelper::GetBoolString(GetRadioState(), aValue);
}         

void nsRadioControlFrame::SetRadioControlFrameState(nsIPresContext* aPresContext,
                                                    const nsString& aValue)
{
  PRBool state = nsFormControlHelper::GetBool(aValue);
  SetRadioState(aPresContext, state);
}         

NS_IMETHODIMP nsRadioControlFrame::SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsAReadableString& aValue)
{
  if (nsHTMLAtoms::checked == aName) {
      // Set the current state for the radio button because
      // the mFormFrame->OnRadioChecked will not set it. 
    SetRadioControlFrameState(aPresContext, aValue);
    if (mFormFrame) {
      PRBool state = (aValue == NS_STRING_TRUE) ? PR_TRUE : PR_FALSE;
      mFormFrame->OnRadioChecked(aPresContext, *this, state);
    }
  }
  else {
    return nsNativeFormControlFrame::SetProperty(aPresContext, aName, aValue);
  }

  return NS_OK;    
}

NS_IMETHODIMP nsRadioControlFrame::GetProperty(nsIAtom* aName, nsAWritableString& aValue)
{
  // Return the value of the property from the widget it is not null.
  // If is null, assume the widget is GFX-rendered and return a member variable instead.

  if (nsHTMLAtoms::checked == aName) {
    GetRadioControlFrameState(aValue);
  }
  else {
    return nsNativeFormControlFrame::GetProperty(aName, aValue);
  }

  return NS_OK;    
}


nsresult nsRadioControlFrame::RequiresWidget(PRBool& aRequiresWidget)
{
  aRequiresWidget = PR_FALSE;
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIStatefulFrame
//----------------------------------------------------------------------
NS_IMETHODIMP
nsRadioControlFrame::GetStateType(nsIPresContext* aPresContext, nsIStatefulFrame::StateType* aStateType)
{
  *aStateType = nsIStatefulFrame::eRadioType;
  return NS_OK;
}

NS_IMETHODIMP
nsRadioControlFrame::SaveState(nsIPresContext* aPresContext, nsIPresState** aState)
{
  // Construct a pres state.
  NS_NewPresState(aState); // The addref happens here.
  
  // This string will hold a single item, whether or not we're checked.
  nsAutoString stateString;
  GetRadioControlFrameState(stateString);
  (*aState)->SetStateProperty("checked", stateString);

  return NS_OK;
}

NS_IMETHODIMP
nsRadioControlFrame::RestoreState(nsIPresContext* aPresContext, nsIPresState* aState)
{
  nsAutoString string;
  aState->GetStateProperty("checked", string);
  SetRadioControlFrameState(aPresContext, string);
  return NS_OK;
}
