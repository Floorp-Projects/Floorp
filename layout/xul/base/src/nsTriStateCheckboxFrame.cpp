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
  : mMouseDownOnCheckbox(PR_FALSE), nsLeafFrame()
{
  // create an atom for the "depress" attribute if it hasn't yet been created.
//  if ( !sDepressAtom )
//    sDepressAtom = dont_QueryInterface(NS_NewAtom("depress"));

} // cntr


//
// GetCurrentCheckState
//
// Looks in the DOM to find out what the value is. 0 is off, 1 is on, 2 is mixed.
// This will default to "off" if no value is set in the DOM.
//
nsTriStateCheckboxFrame::CheckState
nsTriStateCheckboxFrame::GetCurrentCheckState()
{
  nsString value;
  CheckState outState = eOff;
  nsresult res = mContent->GetAttribute ( kNameSpaceID_None, nsHTMLAtoms::value, value );
  if ( res == NS_CONTENT_ATTR_HAS_VALUE )
    outState = StringToCheckState(value);  

printf("getting value, it is %s\n", value.ToNewCString());
  return outState;
} // GetCurrentCheckState


//
// SetCurrentCheckState
//
// Sets the value in the DOM. 0 is off, 1 is on, 2 is mixed.
//
void 
nsTriStateCheckboxFrame::SetCurrentCheckState(CheckState aState)
{
  nsString valueAsString;
  CheckStateToString ( aState, valueAsString );
printf("setting value, it is %s\n", valueAsString.ToNewCString());
  if ( NS_SUCCEEDED(mContent->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::value, valueAsString, PR_TRUE)) )
    Invalidate(nsRect(0, 0, mRect.width, mRect.height), PR_TRUE);
  #ifdef NS_DEBUG
  else
    printf("nsTriStateCheckboxFrame::SetCurrentCheckState -- SetAttribute failed\n");
  #endif
  
} // SetCurrentCheckState


//
// MouseClicked
//
// handle when the mouse is clicked in the box. If the check is on or off, toggle it.
// If the state is mixed, then set it to off. You can't ever get back to mixed.
//
void 
nsTriStateCheckboxFrame::MouseClicked ( const nsIPresContext & aPresContext) 
{
printf("MouseClicked\n");
  mMouseDownOnCheckbox = PR_FALSE;
  CheckState oldState = GetCurrentCheckState();
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

    // Get current checked state through content model.
    // XXX: This is very inefficient, but it is necessary in the case of printing.
    // During printing the Paint is called but the actual state of the checkbox
    // is in a frame in presentation shell 0.
  CheckState checked = GetCurrentCheckState();
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
    // Paint the background
  nsLeafFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
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
  if (aEventStatus == nsEventStatus_eConsumeNoDefault )
    return NS_OK;

  nsresult retVal = NS_OK;
  switch (aEvent->message) {
    case NS_KEY_PRESS:
      if (NS_KEY_EVENT == aEvent->eventStructType) {
        nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
        if (NS_VK_SPACE == keyEvent->keyCode || NS_VK_RETURN == keyEvent->keyCode) {
          MouseClicked(aPresContext);
        }
      }
      break;

    case NS_MOUSE_LEFT_BUTTON_DOWN:
      // set "depressed" state so CSS redraws us
//      if ( NS_SUCCEEDED(mContent->SetAttribute(kNameSpaceID_None, sDepressAtom, NS_STRING_TRUE, PR_TRUE)) )
//        Invalidate(nsRect(0, 0, mRect.width, mRect.height), PR_TRUE);
      mMouseDownOnCheckbox = PR_TRUE;
      break;

    case NS_MOUSE_EXIT:
      // clear "depressed" state so css redraws us
//      if ( NS_SUCCEEDED(mContent->UnsetAttribute(kNameSpaceID_None, sDepressAtom, PR_TRUE)) )
//        Invalidate(nsRect(0, 0, mRect.width, mRect.height), PR_TRUE);
      mMouseDownOnCheckbox = PR_FALSE;
      break;

    case NS_MOUSE_LEFT_CLICK:
    case NS_MOUSE_LEFT_BUTTON_UP:
      if ( mMouseDownOnCheckbox )
        MouseClicked(aPresContext);
      break;
      
    default:
      retVal = nsLeafFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
  }
 
  aEventStatus = nsEventStatus_eConsumeNoDefault;  //XXX ???
  
  return retVal;
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


void
nsTriStateCheckboxFrame :: GetDesiredSize(nsIPresContext* aPresContext,
                                           const nsHTMLReflowState& aReflowState,
                                           nsHTMLReflowMetrics& aDesiredLayoutSize)
{
  nsSize styleSize;
  if (aReflowState.HaveFixedContentWidth()) {
    styleSize.width = aReflowState.computedWidth;
  }
  else {
    styleSize.width = CSS_NOTSET;
  }
  if (aReflowState.HaveFixedContentHeight()) {
    styleSize.height = aReflowState.computedHeight;
  }
  else {
    styleSize.height = CSS_NOTSET;
  }

  // subclasses should always override this method, but if not and no css, make it small
  aDesiredLayoutSize.width  = (styleSize.width  > CSS_NOTSET) ? styleSize.width  : 200;
  aDesiredLayoutSize.height = (styleSize.height > CSS_NOTSET) ? styleSize.height : 200;
  aDesiredLayoutSize.ascent = aDesiredLayoutSize.height;
  aDesiredLayoutSize.descent = 0;
  if (aDesiredLayoutSize.maxElementSize) {
    aDesiredLayoutSize.maxElementSize->width  = aDesiredLayoutSize.width;
    aDesiredLayoutSize.maxElementSize->height = aDesiredLayoutSize.height;
  }

} // GetDesiredSize

