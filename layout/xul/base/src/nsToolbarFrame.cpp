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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsIViewManager.h"
#include "nsXULAtoms.h"
#include "nsINameSpaceManager.h"

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
NS_IMPL_QUERY_INTERFACE2(nsTEMPDragGestureEater, nsIDOMEventListener, nsIDOMDragListener)


nsTEMPDragGestureEater :: nsTEMPDragGestureEater (  )
{
  NS_INIT_REFCNT();
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
NS_NewToolbarFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsToolbarFrame* it = new (aPresShell) nsToolbarFrame (aPresShell);
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
nsToolbarFrame :: nsToolbarFrame (nsIPresShell* aShell):nsBoxFrame(aShell),
   mXDropLoc ( -1 )
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
  nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(content));

  // NOTE: the last Remove will delete the drag listener
  if ( receiver ) {
    receiver->RemoveEventListener("dragover", mDragListener, PR_TRUE);
    receiver->RemoveEventListener("dragexit", mDragListener, PR_TRUE);
  }
}


//
// Init
//
// Setup event capturers for drag and drop. Our frame's lifetime is bounded by the
// lifetime of the content model, so we're guaranteed that the content node won't go away on us. As
// a result, our drag capturer can't go away before the frame is deleted. Since the content
// node holds owning references to our drag capturer, which we tear down in the dtor, there is no 
// need to hold an owning ref to it ourselves.
//
NS_IMETHODIMP
nsToolbarFrame::Init ( nsIPresContext*  aPresContext, nsIContent* aContent,
                        nsIFrame* aParent, nsIStyleContext* aContext,
                        nsIFrame* aPrevInFlow)
{
  nsresult  rv = nsBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  nsCOMPtr<nsIContent> content;
  GetContent(getter_AddRefs(content));
  nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(content));

  // register our drag over and exit capturers. These annotate the content object
  // with enough info to determine where the drop would happen so that JS down the 
  // line can do the right thing.
  mDragListener = new nsToolbarDragListener(this, aPresContext);
  receiver->AddEventListener("dragover", mDragListener, PR_TRUE);
  receiver->AddEventListener("dragexit", mDragListener, PR_TRUE);

#if 0 //TEMP_HACK_FOR_BUG_11291
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
        receiver = do_QueryInterface(node);
        if ( receiver )
          receiver->AddEventListenerByIID(new nsTEMPDragGestureEater, NS_GET_IID(nsIDOMDragListener));
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
// Used to draw the drop feedback based on attributes set by the drag event capturer
//
NS_IMETHODIMP
nsToolbarFrame :: Paint ( nsIPresContext* aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect,
                            nsFramePaintLayer aWhichLayer)
{
  nsresult res =  nsBoxFrame::Paint ( aPresContext, aRenderingContext, aDirtyRect, aWhichLayer );

  if (mXDropLoc != -1) {
    // go looking for the psuedo-style that describes the drop feedback marker. If we don't
    // have it yet, go looking for it.
    if (!mMarkerStyle) {
      nsCOMPtr<nsIAtom> atom ( getter_AddRefs(NS_NewAtom(":-moz-drop-marker")) );
      aPresContext->ProbePseudoStyleContextFor(mContent, atom, mStyleContext,
                                                PR_FALSE, getter_AddRefs(mMarkerStyle));
    }
    
    nscolor color;
    if (mMarkerStyle) {
      const nsStyleColor* styleColor = 
                 NS_STATIC_CAST(const nsStyleColor*, mMarkerStyle->GetStyleData(eStyleStruct_Color));
      color = styleColor->mColor;
    } else {
      color = NS_RGB(0,0,0);
    }

    // draw different drop feedback depending on if we have subitems or not
    int numChildren = 0;
    mContent->ChildCount(numChildren);
    if ( numChildren ) {
      aRenderingContext.SetColor(color);
      nsRect dividingLine ( mXDropLoc, 0, 40, mRect.height );
      aRenderingContext.FillRect(dividingLine);
    }
    else
      aRenderingContext.DrawRect ( mRect );
  }

  return res;
  
} // Paint


// 
// HandleEvent 
// 
// Process events that come to this frame. If they end up here, they are
// almost certainly drag and drop events.
//
NS_IMETHODIMP 
nsToolbarFrame :: HandleEvent ( nsIPresContext* aPresContext, 
                                   nsGUIEvent*     aEvent, 
                                   nsEventStatus*  aEventStatus) 
{ 
  if ( !aEvent ) 
    return nsEventStatus_eIgnore; 

  //XXX this needs to change when I am really handling the D&D events 
  return nsBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus); 
  
} // HandleEvent


#if NOT_YET_NEEDED
/**
 * Call this when styles change
 */
void 
nsToolbarFrame::ReResolveStyles(nsIPresContext* aPresContext,
                                       PRInt32 aParentChange,
                                       nsStyleChangeList* aChangeList,
                                       PRInt32* aLocalChange)
{

  // style that draw an Marker around the button

  // see if the Marker has changed.
  /*nsCOMPtr<nsIStyleContext> oldMarker = mMarkerStyle;

	nsCOMPtr<nsIAtom> atom ( getter_AddRefs(NS_NewAtom(":-moz-marker")) );
	aPresContext->ProbePseudoStyleContextFor(mContent, atom, mStyleContext,
										  PR_FALSE,
										  getter_AddRefs(mMarkerStyle));
  if ((mMarkerStyle && oldMarker) && (mMarkerStyle != oldMarker)) {
    nsFrame::CaptureStyleChangeFor(this, oldMarker, mMarkerStyle, 
                              aParentChange, aChangeList, aLocalChange);
  }*/

}
#endif


////////////////////////////////////////////////////////////////////////
// This is temporary until the bubling of event for CSS actions work
////////////////////////////////////////////////////////////////////////
static void ForceDrawFrame(nsIPresContext* aPresContext, nsIFrame * aFrame);
static void ForceDrawFrame(nsIPresContext* aPresContext, nsIFrame * aFrame)
{
  if (aFrame == nsnull) {
    return;
  }
  nsRect    rect;
  nsIView * view;
  nsPoint   pnt;
  aFrame->GetOffsetFromView(aPresContext, pnt, &view);
  aFrame->GetRect(rect);
  rect.x = pnt.x;
  rect.y = pnt.y;
  if (view) {
    nsCOMPtr<nsIViewManager> viewMgr;
    view->GetViewManager(*getter_AddRefs(viewMgr));
    if (viewMgr)
      viewMgr->UpdateView(view, rect, NS_VMREFRESH_AUTO_DOUBLE_BUFFER | NS_VMREFRESH_IMMEDIATE);
  }

}


//
// AttributeChanged
//
// Track several attributes set by the d&d drop feedback tracking mechanism. The first
// is the "tb-triggerrepaint" attribute so JS can trigger a repaint when it
// needs up update the drop feedback. The second is the x (or y, if bar is vertical) 
// coordinate of where the drop feedback bar should be drawn.
//
NS_IMETHODIMP
nsToolbarFrame :: AttributeChanged ( nsIPresContext* aPresContext, nsIContent* aChild,
                                     PRInt32 aNameSpaceID, nsIAtom* aAttribute, PRInt32 aHint)
{
  nsresult rv = NS_OK;
  
  if ( aAttribute == nsXULAtoms::ddTriggerRepaint )
    ForceDrawFrame ( aPresContext, this );
  else if ( aAttribute == nsXULAtoms::ddDropLocationCoord ) {
    nsAutoString attribute;
    aChild->GetAttribute ( kNameSpaceID_None, aAttribute, attribute );
    char* iHateNSString = attribute.ToNewCString();
    mXDropLoc = atoi( iHateNSString );
    nsAllocator::Free ( iHateNSString );
  }
  else
    rv = nsBoxFrame::AttributeChanged ( aPresContext, aChild, aNameSpaceID, aAttribute, aHint );

  return rv;
  
} // AttributeChanged
