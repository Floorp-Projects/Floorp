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

// pinkerton - this should be removed when the onload handler is called at
//  the correct time so that changes to content in there notify the frames.
#define ONLOAD_CALLED_TOO_EARLY 1

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
// GetDepressAtom [static]
//
// Use a lazily instantiated static initialization scheme to create an atom that
// represents the attribute set when the button is depressed.
//
void
nsTriStateCheckboxFrame :: GetDepressAtom ( nsCOMPtr<nsIAtom>* outAtom )
{
  static nsCOMPtr<nsIAtom> depressAtom = dont_QueryInterface(NS_NewAtom("depress"));
  *outAtom = depressAtom;
}


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
  : mMouseDownOnCheckbox(PR_FALSE), mHasOnceBeenInMixedState(PR_FALSE)
{

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

#if ONLOAD_CALLED_TOO_EARLY
// this code really belongs in AttributeChanged, but is needed here because
// setting the value in onload doesn't trip the AttributeChanged method on the frame
  if ( outState == eMixed )
    mHasOnceBeenInMixedState = PR_TRUE;
#endif
   
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
  mContent->SetAttribute(kNameSpaceID_None, nsHTMLAtoms::value, valueAsString, PR_TRUE);
  
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
  mMouseDownOnCheckbox = PR_FALSE;
  CheckState oldState = GetCurrentCheckState();
  CheckState newState = eOn;
  switch ( oldState ) {
    case eOn:
      newState = eOff;
      break;
      
    case eMixed:
      newState = eOn;
      break;
    
    case eOff:
      newState = mHasOnceBeenInMixedState ? eMixed: eOn;
  }
  SetCurrentCheckState(newState);
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
  CheckState checked = GetCurrentCheckState();
  switch ( checked ) {   
    case eOn:
    {
      const nsStyleColor* color = (const nsStyleColor*)
                                      mStyleContext->GetStyleData(eStyleStruct_Color);
      aRenderingContext.SetColor(color->mColor);
      nsFormControlHelper::PaintCheckMark(aRenderingContext, p2t, mRect.width, mRect.height);
      break;
    }
    
    case eMixed:
    {
      const nsStyleColor* color = (const nsStyleColor*)
                                      mStyleContext->GetStyleData(eStyleStruct_Color);
      aRenderingContext.SetColor(color->mColor);
      PaintMixedMark(aRenderingContext, p2t, mRect.width, mRect.height);
      break;
    }
  
  } // case of value of checkbox
  
  PRBool clip;
  aRenderingContext.PopState(clip);
}


//
// PaintMixedMark
//
// Like nsFormControlHelper::PaintCheckMark(), but paints the horizontal "mixed"
// bar inside the box.
//
void
nsTriStateCheckboxFrame::PaintMixedMark(nsIRenderingContext& aRenderingContext,
                         float aPixelsToTwips, PRUint32 aWidth, PRUint32 aHeight)
{
  const PRUint32 checkpoints = 4;
  const PRUint32 checksize   = 6; //This is value is determined by added 2 units to the end
                                //of the 7X& pixel rectangle below to provide some white space
                                //around the checkmark when it is rendered.

  // Points come from the coordinates on a 7X7 pixels 
  // box with 0,0 at the lower left. 
  nscoord checkedPolygonDef[] = { 1,2,  5,2,  5,4, 1,4 };
  // Location of the center point of the checkmark
  const PRUint32 centerx = 3;
  const PRUint32 centery = 3;
  
  nsPoint checkedPolygon[checkpoints];
  PRUint32 defIndex = 0;
  PRUint32 polyIndex = 0;

   // Scale the checkmark based on the smallest dimension
  PRUint32 size = aWidth / checksize;
  if (aHeight < aWidth)
   size = aHeight / checksize;
  
  // Center and offset each point in the polygon definition.
  for (defIndex = 0; defIndex < (checkpoints * 2); defIndex++) {
    checkedPolygon[polyIndex].x = nscoord((((checkedPolygonDef[defIndex]) - centerx) * (size)) + (aWidth / 2));
    defIndex++;
    checkedPolygon[polyIndex].y = nscoord((((checkedPolygonDef[defIndex]) - centery) * (size)) + (aHeight / 2));
    polyIndex++;
  }
  
  aRenderingContext.FillPolygon(checkedPolygon, checkpoints);

} // PaintMixedMark


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
    {
      // set "depressed" state so CSS redraws us
      DisplayDepressed();
      mMouseDownOnCheckbox = PR_TRUE;
      break;
    }

    case NS_MOUSE_EXIT:
    {
      // clear "depressed" state so css redraws us
      if ( mMouseDownOnCheckbox )
        DisplayNormal();
      mMouseDownOnCheckbox = PR_FALSE;
      break;
    }
    
    case NS_MOUSE_ENTER:
    {
      // if the mouse is down, reset the depressed attribute so CSS redraws.
      if ( mMouseDownOnCheckbox )
        DisplayDepressed();
      break;
    }

    case NS_MOUSE_LEFT_CLICK:
    case NS_MOUSE_LEFT_BUTTON_UP:
      if ( mMouseDownOnCheckbox )
        MouseClicked(aPresContext);
      DisplayNormal();
      break;
      
    default:
      retVal = nsLeafFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
  }
 
  aEventStatus = nsEventStatus_eConsumeNoDefault;  //XXX ???
  
  return retVal;
}


//
// DisplayDepressed
//
// Tickle the right attributes so that CSS draws us in a depressed state. Used
// when doing mouse tracking
//
void
nsTriStateCheckboxFrame :: DisplayDepressed ( )
{
  nsCOMPtr<nsIAtom> depressAtom;
  GetDepressAtom(&depressAtom);
  mContent->SetAttribute(kNameSpaceID_None, depressAtom, NS_STRING_TRUE, PR_TRUE);

} // DisplayDepressed


//
// DisplayNormal
//
// Tickle the right attributes so that CSS draws us in a normal state. Used
// when doing mouse tracking to reset us when the mouse leaves or at the end.
//
void
nsTriStateCheckboxFrame :: DisplayNormal ( )
{
  nsCOMPtr<nsIAtom> depressAtom;
  GetDepressAtom(&depressAtom);
  mContent->UnsetAttribute(kNameSpaceID_None, depressAtom, PR_TRUE);

} // DisplayNormal


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


//
// AttributeChanged
//
// We only want to show the mixed state if the button has ever been in that 
// state in the past. That means that we need to trap all changes to the "value"
// attribute and see if we ever get set to "mixed"
//
NS_IMETHODIMP
nsTriStateCheckboxFrame::AttributeChanged(nsIPresContext* aPresContext,
                                           nsIContent*     aChild,
                                           nsIAtom*        aAttribute,
                                           PRInt32         aHint)
{
  nsresult result = NS_OK;
#if !ONLOAD_CALLED_TOO_EARLY
// onload handlers are called to early, so we have to do this code
// elsewhere. It really belongs HERE.
  if ( aAttribute == nsHTMLAtoms::value ) {
    CheckState newState = GetCurrentCheckState();
    if ( newState == eMixed ) {
      mHasOnceBeenInMixedState = PR_TRUE;
    }
  }
#endif

  // process normally regardless.
  result = nsLeafFrame::AttributeChanged(aPresContext, aChild, aAttribute, aHint);
    
  return result;
  
} // AttributeChanged
