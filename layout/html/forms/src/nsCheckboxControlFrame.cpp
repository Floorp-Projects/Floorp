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

#include "nsCheckboxControlFrame.h"
#include "nsICheckButton.h"
#include "nsNativeFormControlFrame.h"
#include "nsWidgetsCID.h"
#include "nsIContent.h"
#include "nsHTMLAtoms.h"
#include "nsCOMPtr.h"
#include "nsINameSpaceManager.h"
#include "nsFormFrame.h"
#include "nsIStatefulFrame.h"
#include "nsIPresState.h"
#include "nsISupportsPrimitives.h"
#include "nsIComponentManager.h"


//----------------------------------------------------------------------
// nsISupports
//----------------------------------------------------------------------
// Frames are not refcounted, no need to AddRef
NS_IMETHODIMP
nsCheckboxControlFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_ASSERTION(aInstancePtr, "QueryInterface requires a non-NULL destination!");
  if ( !aInstancePtr )
    return NS_ERROR_NULL_POINTER;
  if (aIID.Equals(NS_GET_IID(nsIStatefulFrame))) {
    *aInstancePtr = (void*)(nsIStatefulFrame*) this;
    return NS_OK;
  }
  return nsFormControlFrame::QueryInterface(aIID, aInstancePtr);
}

//
// GetTristateAtom [static]
//
// Use a lazily instantiated static initialization scheme to create an atom that
// represents the attribute set when this should be a tri-state checkbox.
//
// Does NOT addref!
//
nsIAtom*
nsCheckboxControlFrame :: GetTristateAtom ( )
{
  return nsHTMLAtoms::moz_tristate;
}


//
// GetTristateValueAtom [static]
//
// Use a lazily instantiated static initialization scheme to create an atom that
// represents the attribute that holds the value when the button is a tri-state (since
// we can't use "checked").
//
// Does NOT addref!
//
nsIAtom*
nsCheckboxControlFrame :: GetTristateValueAtom ( )
{
  return nsHTMLAtoms::moz_tristatevalue;
}


//
// Constructor
//
nsCheckboxControlFrame :: nsCheckboxControlFrame ( )
  : mIsTristate(PR_FALSE)
{

}


//
// Init
//
// We need to override this in order to see if we're a tristate checkbox.
//
NS_IMETHODIMP
nsCheckboxControlFrame::Init(nsIPresContext*  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
  nsNativeFormControlFrame::Init ( aPresContext, aContent, aParent, aContext, aPrevInFlow );
  
  // figure out if we're a tristate at the start. This may change later on once
  // we've been running for a while, so more code is in AttributeChanged() to pick
  // that up. Regardless, we need this check when initializing.
  nsAutoString value;
  nsresult res = mContent->GetAttribute ( kNameSpaceID_None, GetTristateAtom(), value );
  if ( res == NS_CONTENT_ATTR_HAS_VALUE )
    mIsTristate = PR_TRUE;

  // give the attribute a default value so it's always present, if we're a tristate
  if ( IsTristateCheckbox() )
    mContent->SetAttribute ( kNameSpaceID_None, GetTristateValueAtom(), "0", PR_FALSE );
  
  return NS_OK;
}

/*
 * FIXME: this ::GetIID() method has no meaning in life and should be
 * removed.
 * Pierre Phaneuf <pp@ludusdesign.com>
 */
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


//
// PostCreateWidget
//
// Set the default checked state of the checkbox.
// 
void 
nsCheckboxControlFrame::PostCreateWidget(nsIPresContext* aPresContext, nscoord& aWidth, nscoord& aHeight)
{
  PRBool checked = PR_FALSE;
  nsresult result = GetDefaultCheckState(&checked);
  if (NS_CONTENT_ATTR_HAS_VALUE == result)
    SetCheckboxState (aPresContext, checked ? eOn : eOff );
}


//
// AttributeChanged
//
// Override to check for the attribute that determines if we're a normal or a 
// tristate checkbox. If we notice a switch from one to the other, we need
// to adjust the proper attributes in the content model accordingly.
//
// Also, since the value of a tri-state is kept in a separate attribute (we
// can't use "checked" because it's a boolean), we have to notice it changing
// here.
//
NS_IMETHODIMP
nsCheckboxControlFrame::AttributeChanged(nsIPresContext* aPresContext,
                                          nsIContent*     aChild,
                                          PRInt32         aNameSpaceID,
                                          nsIAtom*        aAttribute,
                                          PRInt32         aHint)
{
  if ( aAttribute == GetTristateAtom() ) {    
    nsAutoString value;
    nsresult res = mContent->GetAttribute ( kNameSpaceID_None, GetTristateAtom(), value );
    PRBool isNowTristate = (res == NS_CONTENT_ATTR_HAS_VALUE);
    if ( isNowTristate != mIsTristate )
      SwitchModesWithEmergencyBrake(aPresContext, isNowTristate);
  }
  else if ( aAttribute == GetTristateValueAtom() ) {
    // ignore this change if we're not a tri-state checkbox
    if ( IsTristateCheckbox() ) {      
      nsAutoString value;
      nsresult res = mContent->GetAttribute ( kNameSpaceID_None, GetTristateValueAtom(), value );
      if ( res == NS_CONTENT_ATTR_HAS_VALUE )
        SetCheckboxControlFrameState(aPresContext, value);
    }
  }
  else
    return nsNativeFormControlFrame::AttributeChanged(aPresContext, aChild, aNameSpaceID, aAttribute, aHint);

  return NS_OK;
}


void 
nsCheckboxControlFrame::MouseUp(nsIPresContext* aPresContext) 
{
  if ( IsTristateCheckbox() ) {
    CheckState newState = eOn;
    switch ( GetCheckboxState() ) {
      case eOn:
        newState = eOff;
        break;
      
      case eMixed:
        newState = eOn;
        break;

      case eOff:
        newState = eMixed;
        break;
    }
    SetCheckboxState(aPresContext, newState);
    
    // Keep the tri-state stuff on the content node current. No need to force an
    // attribute changed event since we just set the state of the checkbox ourselves.
    nsAutoString value;
    CheckStateToString ( newState, value );
    mContent->SetAttribute ( kNameSpaceID_None, GetTristateValueAtom(), value, PR_FALSE );
  }
  else {
    CheckState newState = GetCheckboxState() == eOn ? eOff : eOn;
    SetCheckboxState(aPresContext, newState);
  }
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
  CheckState state = GetCheckboxState();

  nsAutoString value;
  nsresult valueResult = GetValue(&value);
   
   if (eOn != state) {
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
nsCheckboxControlFrame::Reset(nsIPresContext* aPresContext) 
{
  PRBool checked;
  GetDefaultCheckState(&checked);
  SetCheckboxState (aPresContext, checked ? eOn : eOff );
}  

NS_METHOD nsCheckboxControlFrame::HandleEvent(nsIPresContext* aPresContext, 
                                              nsGUIEvent* aEvent,
                                              nsEventStatus* aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus)
    return NS_OK;

  if (nsFormFrame::GetDisabled(this))
    return NS_OK;

  if (NS_MOUSE_LEFT_BUTTON_UP == aEvent->message) {
    MouseUp(aPresContext);
  }

  return(nsNativeFormControlFrame::HandleEvent(aPresContext, aEvent, aEventStatus));
}


void nsCheckboxControlFrame::GetCheckboxControlFrameState(nsString& aValue)
{
  CheckStateToString(GetCheckboxState(), aValue);
}       


void nsCheckboxControlFrame::SetCheckboxControlFrameState(nsIPresContext* aPresContext,
                                                          const nsString& aValue)
{
  CheckState state = StringToCheckState(aValue);
  SetCheckboxState(aPresContext, state);
}         

NS_IMETHODIMP nsCheckboxControlFrame::SetProperty(nsIPresContext* aPresContext,
                                                  nsIAtom* aName,
                                                  const nsAReadableString& aValue)
{
  if (nsHTMLAtoms::checked == aName)
    SetCheckboxControlFrameState(aPresContext, aValue);
  else
    return nsNativeFormControlFrame::SetProperty(aPresContext, aName, aValue);

  return NS_OK;     
}


NS_IMETHODIMP nsCheckboxControlFrame::GetProperty(nsIAtom* aName, nsAReadableString& aValue)
{
  if (nsHTMLAtoms::checked == aName)
    GetCheckboxControlFrameState(aValue);
  else
    return nsNativeFormControlFrame::GetProperty(aName, aValue);

  return NS_OK;     
}

nsresult nsCheckboxControlFrame::RequiresWidget(PRBool& aRequiresWidget)
{
  aRequiresWidget = PR_FALSE;
  return NS_OK;
}


//
// CheckStateToString
//
// Converts from a CheckState to a string
//
void
nsCheckboxControlFrame :: CheckStateToString ( CheckState inState, nsString& outStateAsString )
{
  switch ( inState ) {
    case eOn:
      outStateAsString = NS_STRING_TRUE;
	  break;

    case eOff:
      outStateAsString = NS_STRING_FALSE;
      break;
 
    case eMixed:
      outStateAsString = "2";
      break;
  }
} // CheckStateToString


//
// StringToCheckState
//
// Converts from a string to a CheckState enum
//
nsCheckboxControlFrame::CheckState
nsCheckboxControlFrame :: StringToCheckState ( const nsString & aStateAsString )
{
  if ( aStateAsString == NS_STRING_TRUE )
    return eOn;
  else if ( aStateAsString == NS_STRING_FALSE )
    return eOff;

  // not true and not false means mixed
  return eMixed;
  
} // StringToCheckState


//
// SwitchModesWithEmergencyBrake
//
// Since we use an attribute to decide if we're a tristate box or not, this can change
// at any time. Since we have to use separate attributes to store the values depending
// on the mode, we have to convert from one to the other.
//
void
nsCheckboxControlFrame :: SwitchModesWithEmergencyBrake ( nsIPresContext* aPresContext,
                                                          PRBool inIsNowTristate )
{
  if ( inIsNowTristate ) {
    // we were a normal checkbox, and now we're a tristate. That means that the
    // state of the checkbox was in "checked" and needs to be copied over into
    // our parallel attribute.
    nsAutoString value;
    CheckStateToString ( GetCheckboxState(), value );
    mContent->SetAttribute ( kNameSpaceID_None, GetTristateValueAtom(), value, PR_FALSE );
  }
  else {
    // we were a tri-state checkbox, and now we're a normal checkbox. The current
    // state is already up to date (because it's always up to date). We just have
    // to make sure it's not mixed. If it is, just set it to checked. Remove our
    // parallel attribute so that we're nice and HTML4 compliant.
    if ( GetCheckboxState() == eMixed )
      SetCheckboxState(aPresContext, eOn);
    mContent->UnsetAttribute ( kNameSpaceID_None, GetTristateValueAtom(), PR_FALSE );
  }

  // switch!
  mIsTristate = inIsNowTristate;
  
} // SwitchModesWithEmergencyBrake

//----------------------------------------------------------------------
// nsIStatefulFrame
//----------------------------------------------------------------------
NS_IMETHODIMP nsCheckboxControlFrame::GetStateType(nsIPresContext* aPresContext,
                                                   nsIStatefulFrame::StateType* aStateType)
{
  *aStateType=nsIStatefulFrame::eCheckboxType;
  return NS_OK;
}

NS_IMETHODIMP nsCheckboxControlFrame::SaveState(nsIPresContext* aPresContext,
                                                nsIPresState** aState)
{
  // Construct a pres state.
  NS_NewPresState(aState); // The addref happens here.
  
  // This string will hold a single item, whether or not we're checked.
  nsAutoString stateString;
  GetCheckboxControlFrameState(stateString);
  (*aState)->SetStateProperty("checked", stateString);

  return NS_OK;
}

NS_IMETHODIMP nsCheckboxControlFrame::RestoreState(nsIPresContext* aPresContext,
                                                   nsIPresState* aState)
{
  nsAutoString string;
  aState->GetStateProperty("checked", string);
  SetCheckboxControlFrameState(aPresContext, string);
  return NS_OK;
}
