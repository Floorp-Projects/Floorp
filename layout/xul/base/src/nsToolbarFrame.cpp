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
#include "nsIDOMEventReceiver.h"
#include "nsIDOMDragListener.h"
#include "nsIContent.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"

#define TEMP_HACK_FOR_BUG_11291 1
#if TEMP_HACK_FOR_BUG_11291

// for temp fix of bug 11291. This should really be in JavaScript, I think.
#include "nsIDOMNodeList.h"
#include "nsIDOMElement.h"


class nsTEMPDragGestureEater : public nsIDOMDragListener
{
public:

    // default ctor and dtor
  nsTEMPDragGestureEater ( ) ;
  virtual ~nsTEMPDragGestureEater() { };

    // interfaces for addref and release and queryinterface
  NS_DECL_ISUPPORTS

    // nsIDOMMouseListener
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent);
  virtual nsresult DragEnter(nsIDOMEvent* aMouseEvent);
  virtual nsresult DragOver(nsIDOMEvent* aMouseEvent);
  virtual nsresult DragExit(nsIDOMEvent* aMouseEvent);
  virtual nsresult DragDrop(nsIDOMEvent* aMouseEvent);
  virtual nsresult DragGesture(nsIDOMEvent* aMouseEvent);

}; // class nsTEMPDragGestureEater

NS_IMPL_ADDREF(nsTEMPDragGestureEater)
NS_IMPL_RELEASE(nsTEMPDragGestureEater)

nsTEMPDragGestureEater :: nsTEMPDragGestureEater (  )
{
  NS_INIT_REFCNT();
}

//
// QueryInterface
//
// Modeled after scc's reference implementation
//   http://www.mozilla.org/projects/xpcom/QI.html
//
nsresult
nsTEMPDragGestureEater::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if ( !aInstancePtr)
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(nsCOMTypeInfo<nsIDOMEventListener>::GetIID()))
    *aInstancePtr = NS_STATIC_CAST(nsIDOMEventListener*, this);
  else if (aIID.Equals(nsCOMTypeInfo<nsIDOMDragListener>::GetIID()))
    *aInstancePtr = NS_STATIC_CAST(nsIDOMDragListener*, this);
  else if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))                                   
    *aInstancePtr = NS_STATIC_CAST(nsISupports*, NS_STATIC_CAST(nsIDOMDragListener*, this));
  else
    *aInstancePtr = 0;
  
  nsresult status;
  if ( !*aInstancePtr )
    status = NS_NOINTERFACE;
  else {
    NS_ADDREF( NS_REINTERPRET_CAST(nsISupports*, *aInstancePtr) );
    status = NS_OK;
  }
  return status;
}

nsresult
nsTEMPDragGestureEater :: HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

nsresult
nsTEMPDragGestureEater::DragGesture(nsIDOMEvent* aMouseEvent)
{
  // we want the text widget to see this event, but not anyone above us that
  // might be registered as a listener for drags. Therefore, don't
  // allow this event to bubble.
  aMouseEvent->PreventBubble();
  return NS_ERROR_BASE; // means I AM consuming event
}

nsresult
nsTEMPDragGestureEater::DragEnter(nsIDOMEvent* aMouseEvent)
{
  return NS_OK; // means I am NOT consuming event
}

nsresult
nsTEMPDragGestureEater::DragOver(nsIDOMEvent* aMouseEvent)
{
  return NS_OK; // means I am NOT consuming event
}

nsresult
nsTEMPDragGestureEater::DragExit(nsIDOMEvent* aMouseEvent)
{
  return NS_OK; // means I am NOT consuming event
}

nsresult
nsTEMPDragGestureEater::DragDrop(nsIDOMEvent* aMouseEvent)
{
  return NS_OK; // means I am NOT consuming event
}

#endif /* TEMP_HACK_FOR_BUG_11291 */


#ifdef XP_MAC
#pragma mark -
#endif


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
// nsToolbarFrame ctor
//
// Most of the work need to be delayed until Init(). Lame!
//
nsToolbarFrame :: nsToolbarFrame ( )
  : mXDropLoc ( -1 )
{
}


//
// nsToolbarFrame dtor
//
// Cleanup. Remove our registered event listener from the content model.
//
nsToolbarFrame :: ~nsToolbarFrame ( )
{
  nsCOMPtr<nsIContent> content;
  GetContent(getter_AddRefs(content));
  nsCOMPtr<nsIDOMEventReceiver> reciever(do_QueryInterface(content));

  // NOTE: the Remove will delete the drag listener
  reciever->RemoveEventListenerByIID((nsIDOMDragListener *)mDragListener, nsIDOMDragListener::GetIID());
}


//
// Init
//
// Setup event listeners for drag and drop. Our frame's lifetime is bounded by the
// lifetime of the content model, so we're guaranteed that the content node won't go away on us. As
// a result, our drag listener can't go away before the frame is deleted. Since the content
// node holds owning references to our drag listener, which we tear down in the dtor, there is no 
// need to hold an owning ref to it ourselves.
//
NS_IMETHODIMP
nsToolbarFrame::Init ( nsIPresContext&  aPresContext, nsIContent* aContent,
                        nsIFrame* aParent, nsIStyleContext* aContext,
                        nsIFrame* aPrevInFlow)
{
  nsresult  rv = nsBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  nsCOMPtr<nsIContent> content;
  GetContent(getter_AddRefs(content));
  nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(content));

  mDragListener = new nsToolbarDragListener(this, &aPresContext);
  receiver->AddEventListenerByIID((nsIDOMDragListener *)mDragListener, nsIDOMDragListener::GetIID());

#if TEMP_HACK_FOR_BUG_11291
  // Ok, this is a hack until Ender lands. We need to have a mouse listener on text widgets
  // in order to make sure that mouseDowns within the text widget don't bubble up to the toolbar
  // listener. This would cause problems where selecting text and moving the mouse outside the text
  // widget and into the toolbar would start a drag (bug #11291)
  nsCOMPtr<nsIDOMElement> element ( do_QueryInterface(content) );
  if ( element ) {
    nsCOMPtr<nsIDOMNodeList> inputList;
    element->GetElementsByTagName("INPUT", getter_AddRefs(inputList));
    if ( inputList ) {
      PRUint32 length = 0;
      inputList->GetLength(&length);
      for ( PRUint32 i = 0; i < length; ++i ) {
        nsCOMPtr<nsIDOMNode> node;
        inputList->Item(i, getter_AddRefs(node));
        nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(node));
        if ( receiver )
          receiver->AddEventListenerByIID(new nsTEMPDragGestureEater, nsIDOMDragListener::GetIID());
        // yes, i know this will leak. That's ok, i don't care because this code will go away
      }   
    }
  }
#endif

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
// GetFrameForPoint
//
// Override to process events in our own frame
//
NS_IMETHODIMP
nsToolbarFrame :: GetFrameForPoint ( const nsPoint& aPoint, nsIFrame** aFrame)
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

  } 

  //XXX this needs to change when I am really handling the D&D events 
  return nsBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus); 
  
} // HandleEvent


#if NOT_YET_NEEDED
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
#endif

