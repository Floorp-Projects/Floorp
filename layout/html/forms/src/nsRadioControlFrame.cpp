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
#include "nsIRadioButton.h"
#include "nsNativeFormControlFrame.h"
#include "nsWidgetsCID.h"
#include "nsIContent.h"
#include "nsHTMLAtoms.h"
#include "nsFormFrame.h"
#include "nsINameSpaceManager.h"
#include "nsFormFrame.h"


static NS_DEFINE_IID(kIRadioControlFrameIID,  NS_IRADIOCONTROLFRAME_IID);


nsresult
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
 
  return Inherited::QueryInterface(aIID, aInstancePtr);
}


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
      SetRadioControlFrameState(NS_STRING_TRUE);
    else
      SetRadioControlFrameState(NS_STRING_FALSE);
  }
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
nsRadioControlFrame::Reset() 
{
  PRBool checked = PR_TRUE;
  GetDefaultCheckState(&checked);
  SetCurrentCheckState(checked);
}  

NS_IMETHODIMP
nsRadioControlFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("RadioControl", aResult);
}


NS_IMETHODIMP
nsRadioControlFrame::SetRadioButtonFaceStyleContext(nsIStyleContext *aRadioButtonFaceStyleContext)
{
 /* for gfx widgets only */
 return NS_OK;
}


NS_METHOD 
nsRadioControlFrame::HandleEvent(nsIPresContext& aPresContext, 
                                 nsGUIEvent* aEvent,
                                 nsEventStatus& aEventStatus)
{
  if (nsEventStatus_eConsumeNoDefault == aEventStatus) {
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
          MouseClicked(&aPresContext);
        }
      }
      break;
    default:
      break;
  }

  return(Inherited::HandleEvent(aPresContext, aEvent, aEventStatus));
}

void nsRadioControlFrame::GetRadioControlFrameState(nsString& aValue)
{
	nsFormControlHelper::GetBoolString(GetRadioState(), aValue);
}         

void nsRadioControlFrame::SetRadioControlFrameState(const nsString& aValue)
{
  PRBool state = nsFormControlHelper::GetBool(aValue);
  SetRadioState(state);
}         

NS_IMETHODIMP nsRadioControlFrame::SetProperty(nsIAtom* aName, const nsString& aValue)
{
  if (nsHTMLAtoms::checked == aName) {
      // Set the current state for the radio button because
      // the mFormFrame->OnRadioChecked will not set it. 
    SetRadioControlFrameState(aValue);
    if (mFormFrame) {
      PRBool state = (aValue == NS_STRING_TRUE) ? PR_TRUE : PR_FALSE;
      mFormFrame->OnRadioChecked(*this, state);
    }
  }
  else {
    return Inherited::SetProperty(aName, aValue);
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
    return Inherited::GetProperty(aName, aValue);
  }

  return NS_OK;    
}


nsresult nsRadioControlFrame::RequiresWidget(PRBool& aRequiresWidget)
{
  aRequiresWidget = PR_FALSE;
  return NS_OK;
}
