/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

//
// Mike Pinkerton
// Netscape Communications
//
// See documentation in associated header file
//

#include "nsToolbarFrame.h"
#include "nsCSSRendering.h"

#include "nsToolbarDragListener.h"
#include "nsIDOMDragListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMEventReceiver.h"
#include "nsIContent.h"
#include "nsIPresContext.h"
//
// NS_NewToolbarFrame
//
// Creates a new Toolbar frame and returns it in |aNewFrame|
//
nsresult
NS_NewToolbarFrame ( nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsToolbarFrame* it = new nsToolbarFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  // it->SetFlags(aFlags);
  * aNewFrame = it;
  return NS_OK;
  
} // NS_NewToolbarFrame


//
// nsToolbarFrame cntr
//
// Init, if necessary
//
nsToolbarFrame :: nsToolbarFrame ( )
{
	//*** anything?
  mXDropLoc = -1;
}


//
// nsToolbarFrame dstr
//
// Cleanup, if necessary
//
nsToolbarFrame :: ~nsToolbarFrame ( )
{
#ifdef NS_DEBUG
  printf("Deleting toolbar frame\n");
#endif
}

NS_IMETHODIMP
nsToolbarFrame::Init(nsIPresContext&  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  //nsCOMPtr<nsIDOMEventReceiver> reciever(do_QueryInterface(mContent));
  nsCOMPtr<nsIContent> content;
  GetContent(getter_AddRefs(content));

  nsCOMPtr<nsIDOMEventReceiver> reciever(do_QueryInterface(content));

  mDragListener = new nsToolbarDragListener();
  mDragListener->SetToolbar(this);

  if (NS_OK == reciever->AddEventListenerByIID((nsIDOMDragListener *)mDragListener, nsIDOMDragListener::GetIID())) {
    //printf("Toolbar registered as Drag Listener\n");
  }

  if (NS_OK == reciever->AddEventListenerByIID((nsIDOMMouseListener *)mDragListener, nsIDOMMouseListener::GetIID())) {
    //printf("Toolbar registered as Mouse Listener\n");
  }

  if (NS_OK == reciever->AddEventListenerByIID((nsIDOMMouseMotionListener *)mDragListener, nsIDOMMouseMotionListener::GetIID())) {
    //printf("Toolbar registered as MouseMotion Listener\n");
  }

  //      nsCOMPtr<nsIDOMEventListener> eventListener = do_QueryInterface(popupListener);
  //      AddEventListener("mousedown", eventListener, PR_FALSE);  

  return rv;
}

//
// Paint
//
// Paint our background and border like normal frames, but before we draw the
// children, draw our grippies for each toolbar.
//
NS_IMETHODIMP
nsToolbarFrame :: Paint ( nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect,
                            nsFramePaintLayer aWhichLayer)
{
#if 0
  if (eFramePaintLayer_Underlay == aWhichLayer) {
    const nsStyleDisplay* disp = (const nsStyleDisplay*)
      mStyleContext->GetStyleData(eStyleStruct_Display);
    if (disp->mVisible && mRect.width && mRect.height) {
      // Paint our background and border
      PRIntn skipSides = GetSkipSides();
      const nsStyleColor* color = (const nsStyleColor*)
        mStyleContext->GetStyleData(eStyleStruct_Color);
      const nsStyleSpacing* spacing = (const nsStyleSpacing*)
        mStyleContext->GetStyleData(eStyleStruct_Spacing);

      nsRect  rect(0, 0, mRect.width, mRect.height);
      nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                      aDirtyRect, rect, *color, *spacing, 0, 0);
      nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                  aDirtyRect, rect, *spacing, mStyleContext, skipSides);
    }
  }

  //*** anything toolbar specific here???
    
  // Now paint the items. Note that child elements have the opportunity to
  // override the visibility property and display even if their parent is
  // hidden
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);


  return NS_OK;

#endif

  nsresult res =  nsBoxFrame::Paint ( aPresContext, aRenderingContext, aDirtyRect, aWhichLayer );

  if (mXDropLoc != -1) {
    //printf("mXDropLoc: %d\n", mXDropLoc);
    // XXX this is temporary
    if (!mMarkerStyle) {
      nsCOMPtr<nsIAtom> atom ( getter_AddRefs(NS_NewAtom(":-moz-drop-marker")) );
      aPresContext.ProbePseudoStyleContextFor(mContent, atom, mStyleContext,
										    PR_FALSE,
										    getter_AddRefs(mMarkerStyle));
    }
    nscolor color;
    if (mMarkerStyle) {
      const nsStyleColor* styleColor = (const nsStyleColor*)mMarkerStyle->GetStyleData(eStyleStruct_Color);
      color = styleColor->mColor;
    } else {
      color = NS_RGB(0,0,0);
    }
    //printf("paint %d\n", mXDropLoc);
    aRenderingContext.SetColor(color);
    aRenderingContext.DrawLine(mXDropLoc, 0, mXDropLoc, mRect.height);
  }

  return res;
  
} // Paint


//
// Reflow
//
// Handle moving children around.
//
NS_IMETHODIMP
nsToolbarFrame :: Reflow ( nsIPresContext&          aPresContext,
                            nsHTMLReflowMetrics&     aDesiredSize,
                            const nsHTMLReflowState& aReflowState,
                            nsReflowStatus&          aStatus)
{
  return nsBoxFrame::Reflow ( aPresContext, aDesiredSize, aReflowState, aStatus );

} // Reflow 


//
// GetFrameForPoint
//
// Override to process events in our own frame
//
NS_IMETHODIMP
nsToolbarFrame :: GetFrameForPoint(const nsPoint& aPoint, 
                                  nsIFrame**     aFrame)
{
  nsresult retVal = nsHTMLContainerFrame::GetFrameForPoint(aPoint, aFrame);

  // returning NS_OK means that we tell the frame finding code that we have something
  // and to stop looking elsewhere for a frame.
  if ( aFrame && *aFrame == this )
    retVal = NS_OK;
  else if ( retVal != NS_OK ) {
    *aFrame = this;
    retVal = NS_OK;
  }
     
  return retVal;
  
} // GetFrameForPoint


// 
// HandleEvent 
// 
// Process events that come to this frame. If they end up here, they are
// almost certainly drag and drop events.
//
NS_IMETHODIMP 
nsToolbarFrame :: HandleEvent ( nsIPresContext& aPresContext, 
                                   nsGUIEvent*     aEvent, 
                                   nsEventStatus&  aEventStatus) 
{ 
  if(mDragListener) {
    mDragListener->SetPresContext(&aPresContext); // not ref counted
  }

  if ( !aEvent ) 
    return nsEventStatus_eIgnore; 

  switch (aEvent->message) { 
    case NS_DRAGDROP_ENTER: 

      if (!mMarkerStyle) {
        nsCOMPtr<nsIAtom> atom ( getter_AddRefs(NS_NewAtom(":-moz-drop-marker")) );
        aPresContext.ProbePseudoStyleContextFor(mContent, atom, mStyleContext,
										      PR_FALSE,
										      getter_AddRefs(mMarkerStyle));
      }
      break; 

    case NS_DRAGDROP_OVER: 
      break; 

    case NS_DRAGDROP_EXIT: 
      mMarkerStyle = do_QueryInterface(nsnull);
      // remove drop feedback 
      break; 

    case NS_DRAGDROP_DROP: 
      // do drop coolness stuff 
      break; 
  } 

  //XXX this needs to change when I am really handling the D&D events 
  return nsBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus); 
  
} // HandleEvent

/**
 * Call this when styles change
 */
void 
nsToolbarFrame::ReResolveStyles(nsIPresContext& aPresContext,
                                       PRInt32 aParentChange,
                                       nsStyleChangeList* aChangeList,
                                       PRInt32* aLocalChange)
{

  // style that draw an Marker around the button

  // see if the Marker has changed.
  /*nsCOMPtr<nsIStyleContext> oldMarker = mMarkerStyle;

	nsCOMPtr<nsIAtom> atom ( getter_AddRefs(NS_NewAtom(":-moz-marker")) );
	aPresContext.ProbePseudoStyleContextFor(mContent, atom, mStyleContext,
										  PR_FALSE,
										  getter_AddRefs(mMarkerStyle));
  if ((mMarkerStyle && oldMarker) && (mMarkerStyle != oldMarker)) {
    nsFrame::CaptureStyleChangeFor(this, oldMarker, mMarkerStyle, 
                              aParentChange, aChangeList, aLocalChange);
  }*/

}
