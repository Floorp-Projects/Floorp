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

#include "nsCheckboxControlFrame.h"
#include "nsICheckButton.h"
#include "nsNativeFormControlFrame.h"
#include "nsWidgetsCID.h"
#include "nsIContent.h"
#include "nsHTMLAtoms.h"
#include "nsFormFrame.h"



const nsIID&
nsCheckboxControlFrame::GetIID()
{
  static NS_DEFINE_IID(kCheckboxIID, NS_ICHECKBUTTON_IID);
  return kCheckboxIID;
}
  
const nsIID&
nsCheckboxControlFrame::GetCID()
{
  static NS_DEFINE_IID(kCheckboxCID, NS_CHECKBUTTON_CID);
  return kCheckboxCID;
}

void 
nsCheckboxControlFrame::PostCreateWidget(nsIPresContext* aPresContext, nscoord& aWidth, nscoord& aHeight)
{
   // set the widget to the initial state
  PRBool checked = PR_FALSE;
  nsresult result = GetDefaultCheckState(&checked);
  if (NS_CONTENT_ATTR_HAS_VALUE == result) {
    if (PR_TRUE == checked)
      SetProperty(nsHTMLAtoms::checked, NS_STRING_TRUE);
    else
      SetProperty(nsHTMLAtoms::checked, NS_STRING_FALSE);
  }
}

void 
nsCheckboxControlFrame::MouseClicked(nsIPresContext* aPresContext) 
{
  PRBool oldState;
  GetCurrentCheckState(&oldState);
  PRBool newState = oldState ? PR_FALSE : PR_TRUE;
  SetCurrentCheckState(newState); 
}

PRInt32 
nsCheckboxControlFrame::GetMaxNumValues()
{
  return 1;
}
  

PRBool
nsCheckboxControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                       nsString* aValues, nsString* aNames)
{
  nsAutoString name;
  nsresult nameResult = GetName(&name);
  if ((aMaxNumValues <= 0) || (NS_CONTENT_ATTR_HAS_VALUE != nameResult)) {
    return PR_FALSE;
  }

  PRBool result = PR_TRUE;
  PRBool state = GetCheckboxState();

  nsAutoString value;
  nsresult valueResult = GetValue(&value);
   
   if (PR_TRUE != state) {
      result = PR_FALSE;
   } else {
     if (NS_CONTENT_ATTR_HAS_VALUE != valueResult) {
       aValues[0] = "on";
     } else {
       aValues[0] = value;
     }
     aNames[0] = name;
     aNumValues = 1;
  }
 
  return result;
}

void 
nsCheckboxControlFrame::Reset() 
{
  PRBool checked;
  GetDefaultCheckState(&checked);
  SetCurrentCheckState(checked);
}  

NS_METHOD nsCheckboxControlFrame::HandleEvent(nsIPresContext& aPresContext, 
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


void nsCheckboxControlFrame::GetCheckboxControlFrameState(nsString& aValue)
{
	nsFormControlHelper::GetBoolString(GetCheckboxState(), aValue);
}       


void nsCheckboxControlFrame::SetCheckboxControlFrameState(const nsString& aValue)
{
  PRBool state = nsFormControlHelper::GetBool(aValue);
  SetCheckboxState(state);
}         

NS_IMETHODIMP nsCheckboxControlFrame::SetProperty(nsIAtom* aName, const nsString& aValue)
{
  if (nsHTMLAtoms::checked == aName) {
    SetCheckboxControlFrameState(aValue);
  }
  else {
    return Inherited::SetProperty(aName, aValue);
  }

  return NS_OK;     
}


NS_IMETHODIMP nsCheckboxControlFrame::GetProperty(nsIAtom* aName, nsString& aValue)
{
  // Return the value of the property from the widget it is not null.
  // If is null, assume the widget is GFX-rendered and return a member variable instead.

  if (nsHTMLAtoms::checked == aName) {
    GetCheckboxControlFrameState(aValue);
  }
  else {
    return Inherited::GetProperty(aName, aValue);
  }

  return NS_OK;     
}

nsresult nsCheckboxControlFrame::RequiresWidget(PRBool& aRequiresWidget)
{
  aRequiresWidget = PR_FALSE;
  return NS_OK;
}

