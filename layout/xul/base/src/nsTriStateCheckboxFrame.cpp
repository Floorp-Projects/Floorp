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

#include "nsTriStateCheckboxFrame.h"

#include "nsFormControlHelper.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIFrame.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsHTMLAtoms.h"
#include "nsIStyleContext.h"
#include "nsStyleUtil.h"
#include "nsIFormControl.h"
#include "nsCSSRendering.h"
#include "nsINameSpaceManager.h"


//
// NS_NewTriStateCheckboxFrame
//
// Wrapper for creating a new tristate checkbox
//
nsresult
NS_NewTriStateCheckboxFrame(nsIFrame*& aResult)
{
  aResult = new nsTriStateCheckboxFrame;
  if ( !aResult )
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}


//
// nsTriStateCheckboxFrame cntr
//
nsTriStateCheckboxFrame::nsTriStateCheckboxFrame()
  : mMouseDownOnCheckbox(PR_FALSE)
{
}


//
// GetCurrentCheckState
//
// Looks in the DOM to find out what the value is. 0 is off, 1 is on, 2 is mixed.
// This will default to "off" if no value is set in the DOM.
//
nsresult
nsTriStateCheckboxFrame::GetCurrentCheckState(CheckState *outState)
{
  nsString value;
  nsresult res = mContent->GetAttribute ( kNameSpaceID_None, nsHTMLAtoms::value, value );
  if ( res == NS_CONTENT_ATTR_HAS_VALUE )
    *outState = StringToCheckState(value);  
  else
    *outState = eOff;

printf("getting value, it is %s\n", value.ToNewCString());
  return NS_OK;
}


//
// SetCurrentCheckState
//
// Sets the value in the DOM. 0 is off, 1 is on, 2 is mixed.
//
nsresult 
nsTriStateCheckboxFrame::SetCurrentCheckState(CheckState aState)
{
  nsString valueAsString;
  CheckStateToString ( aState, valueAsString );
printf("setting value, it is %s\n", valueAsString.ToNewCString());
  return mContent->SetAttribute ( kNameSpaceID_None, nsHTMLAtoms::value, valueAsString, PR_TRUE );

} // SetCurrentCheckState


//
// MouseClicked
//
// handle when the mouse is clicked in the box. If the check is on or off, toggle it.
// If the state is mixed, then set it to off. You can't ever get back to mixed.
//
void 
nsTriStateCheckboxFrame::MouseClicked(nsIPresContext* aPresContext) 
{
  mMouseDownOnCheckbox = PR_FALSE;
  CheckState oldState;
  GetCurrentCheckState(&oldState);
  CheckState newState = eOn;
  switch ( oldState ) {
    case eOn:
    case eMixed:
      newState = eOff;
      break;
  }
  SetCurrentCheckState(newState); 
}


// 
// GetMaxNumValues
//
// Because we have a mixed state, we go up to two.
//
PRInt32 
nsTriStateCheckboxFrame::GetMaxNumValues()
{
  return 2;
}
  

//
// PaintCheckBox
//
// Paint the checkbox depending on the mode
//
void
nsTriStateCheckboxFrame::PaintCheckBox(nsIPresContext& aPresContext,
                  nsIRenderingContext& aRenderingContext,
                  const nsRect& aDirtyRect,
                  nsFramePaintLayer aWhichLayer)
{
  aRenderingContext.PushState();

  float p2t;
  aPresContext.GetScaledPixelsToTwips(&p2t);

  CheckState checked = eOff;

    // Get current checked state through content model.
    // XXX: This is very inefficient, but it is necessary in the case of printing.
    // During printing the Paint is called but the actual state of the checkbox
    // is in a frame in presentation shell 0.
  /*XXXnsresult result = */GetCurrentCheckState(&checked);
  if ( checked == eOn ) {
      // Draw check mark
    const nsStyleColor* color = (const nsStyleColor*)
      mStyleContext->GetStyleData(eStyleStruct_Color);
    aRenderingContext.SetColor(color->mColor);
printf("painting checkbox\n");
    nsFormControlHelper::PaintCheckMark(aRenderingContext,
                         p2t, mRect.width, mRect.height);
   
  }
  PRBool clip;
  aRenderingContext.PopState(clip);
}


//
// Paint
//
// Overidden to handle drawing the checkmark in addition to everything else
//
NS_METHOD 
nsTriStateCheckboxFrame::Paint(nsIPresContext& aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              const nsRect& aDirtyRect,
                              nsFramePaintLayer aWhichLayer)
{
printf("painting checkbox\n");
    // Paint the background
  nsFormControlFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
      // Paint the checkmark 
    PaintCheckBox(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  }
  return NS_OK;
}


//
// HandleEvent
//
// Track the mouse and handle clicks
//
NS_METHOD
nsTriStateCheckboxFrame::HandleEvent(nsIPresContext& aPresContext, 
                                     nsGUIEvent* aEvent,
                                     nsEventStatus& aEventStatus)
{
  if (nsEventStatus_eConsumeNoDefault == aEventStatus) {
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

      // Handle GFX rendered widget Mouse Down event
    switch (aEvent->message) {
      case NS_MOUSE_LEFT_BUTTON_DOWN:
        mMouseDownOnCheckbox = PR_TRUE;
    //XXX: TODO render gray rectangle on mouse down  
      break;

      case NS_MOUSE_EXIT:
        mMouseDownOnCheckbox = PR_FALSE;
    //XXX: TO DO clear gray rectangle on mouse up 
      break;

    }

  return(nsFormControlFrame::HandleEvent(aPresContext, aEvent, aEventStatus));
}


//
// RequiresWidget
//
// Tell callers that we are GFX rendered through and through
//
nsresult
nsTriStateCheckboxFrame::RequiresWidget(PRBool& aRequiresWidget)
{
  aRequiresWidget = PR_FALSE;
  return NS_OK;
}


//
// StringToCheckState
//
// Converts from a string to a CheckState enum
//
nsTriStateCheckboxFrame::CheckState
nsTriStateCheckboxFrame :: StringToCheckState ( const nsString & aStateAsString )
{
  if ( aStateAsString == NS_STRING_TRUE )
    return eOn;
  else if ( aStateAsString == NS_STRING_FALSE )
    return eOff;

  // not true and not false means mixed
  return eMixed;
  
} // StringToCheckState


//
// CheckStateToString
//
// Converts from a CheckState to a string
//
void
nsTriStateCheckboxFrame :: CheckStateToString ( CheckState inState, nsString& outStateAsString )
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
