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

#include "nsToolbarDragListener.h"
#include "nsToolbarFrame.h"

#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "nsITransferable.h"
#include "nsCOMPtr.h"
#include "nsIDOMUIEvent.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsXULAtoms.h"
#include "nsIEventStateManager.h"
#include "nsISupportsPrimitives.h"

#include "nsISupportsArray.h"
#include "nsIViewManager.h"
#include "nsIView.h"


// Drag & Drop, Clipboard Support
static NS_DEFINE_CID(kCDragServiceCID,         NS_DRAGSERVICE_CID);
static NS_DEFINE_CID(kCTransferableCID,        NS_TRANSFERABLE_CID);
static NS_DEFINE_IID(kCDataFlavorCID,          NS_DATAFLAVOR_CID);


NS_IMPL_ADDREF(nsToolbarDragListener)
NS_IMPL_RELEASE(nsToolbarDragListener)


//
// nsToolbarDragListener ctor
//
// Not much to do besides init member variables
//
nsToolbarDragListener :: nsToolbarDragListener ( nsToolbarFrame* inToolbar, nsIPresContext* inPresContext )
  : mToolbar(inToolbar), mPresContext(inPresContext), mCurrentDropLoc(-1)
{
  NS_INIT_REFCNT();
}


//
// nsToolbarDragListener dtor
//
// Cleanup.
//
nsToolbarDragListener::~nsToolbarDragListener() 
{
}


//
// QueryInterface
//
// Modeled after scc's reference implementation
//   http://www.mozilla.org/projects/xpcom/QI.html
//
nsresult
nsToolbarDragListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if ( !aInstancePtr)
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(nsCOMTypeInfo<nsIDOMEventListener>::GetIID()))
    *aInstancePtr = NS_STATIC_CAST(nsIDOMEventListener*, NS_STATIC_CAST(nsIDOMDragListener*, this));
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


////////////////////////////////////////////////////////////////////////
// This is temporary until the bubling of event for CSS actions work
////////////////////////////////////////////////////////////////////////
static void ForceDrawFrame(nsIFrame * aFrame)
{
  if (aFrame == nsnull) {
    return;
  }
  nsRect    rect;
  nsIView * view;
  nsPoint   pnt;
  aFrame->GetOffsetFromView(pnt, &view);
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

////////////////////////////////////////////////////////////////////////
nsresult
nsToolbarDragListener::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
nsresult
nsToolbarDragListener::DragGesture(nsIDOMEvent* aDragEvent)
{
  // this code should all be in JS.
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
nsresult
nsToolbarDragListener::DragEnter(nsIDOMEvent* aDragEvent)
{
  mCurrentDropLoc = -1;

  nsresult rv;
  NS_WITH_SERVICE ( nsIDragService, dragService, kCDragServiceCID, &rv );
  if ( NS_SUCCEEDED(rv) ) {
    nsCOMPtr<nsIDragSession> dragSession(do_QueryInterface(dragService));

    if ( dragSession ) {
      PRBool flavorSupported = PR_FALSE;
      dragSession->IsDataFlavorSupported ( TOOLBARITEM_MIME, &flavorSupported );
      if ( flavorSupported ) {
        dragSession->SetCanDrop(PR_TRUE);
        rv = NS_ERROR_BASE;   // consume event
      }
      else
        rv = NS_OK;  // don't consume event
    }
  }
  else 
    rv = NS_OK;

  return rv;
}


////////////////////////////////////////////////////////////////////////
PRBool
nsToolbarDragListener::IsOnToolbarItem(nsIDOMEvent* aDragEvent, nscoord& aXLoc, PRBool& aIsLegalChild)
{
  aIsLegalChild = PR_FALSE;

  nsCOMPtr<nsIDOMUIEvent> uiEvent(do_QueryInterface(aDragEvent));
  PRInt32 x,y = 0;
  uiEvent->GetClientX(&x);
  uiEvent->GetClientY(&y);

  // This is kind of hooky but its the easiest thing to do
  // The mPresContext is set into this class from the nsToolbarFrame
  // It's needed here for figuring out twips & 
  // resetting the active state in the event manager after the drop takes place.
  if ( !mPresContext ) 
    return NS_OK;

  // translate the mouse coords into twips
  float p2t;
  mPresContext->GetScaledPixelsToTwips(&p2t);
  nscoord onePixel = NSIntPixelsToTwips(1, p2t);
  nscoord xp       = NSIntPixelsToTwips(x, p2t);
  nscoord yp       = NSIntPixelsToTwips(y, p2t);
  nsPoint pnt(xp, yp);

  // get the toolbar's rect
  nsRect tbRect;
  mToolbar->GetRect(tbRect);

  nscoord   count = 0;
  nsIFrame* childFrame;
  nsRect    rect;             // child frame's rect
  nsRect    prevRect(-1, -1, 0, 0);
  PRBool    found = PR_FALSE;

  // Now lop through the child and see if the mouse is over a child
  mToolbar->FirstChild(nsnull, &childFrame); 
  while (nsnull != childFrame) {    

    // The mouse coords are in the toolbar's domain
    // Get child's rect and adjust to the toolbar's domain
    childFrame->GetRect(rect);
    rect.MoveBy(tbRect.x, tbRect.y);

    // remember the previous child x location
    if (pnt.x < rect.x && prevRect.x == -1) {
      prevRect = rect;
    }

    // now check to see if the mouse inside an items bounds
    if (rect.Contains(pnt)) {
      nsCOMPtr<nsIContent> content;
      nsresult result = childFrame->GetContent(getter_AddRefs(content));
      if (NS_OK == result) {
        nsCOMPtr<nsIAtom> tag;
        content->GetTag(*getter_AddRefs(tag));

        // for now I am checking for both titlebutton and toolbar items
        // XXX but the check for titlebutton should be removed in the future
        if (tag.get() == nsXULAtoms::titledbutton || tag.get() == nsXULAtoms::toolbaritem) {

          // now check for natural order
          PRBool naturalOrder = PR_FALSE;
          nsCOMPtr<nsIDOMElement> domElement;
          domElement = do_QueryInterface(content);
          if (nsnull != domElement ) {
            nsAutoString value;
            // maybe naturalorder needs to be an atom
            domElement->GetAttribute(nsAutoString("naturalorder"), value);
            naturalOrder = value.Equals("true");

          } else {
            printf("Not a DOM element\n");
          }
          
          found = PR_TRUE;

          // if naturalorder than figure out if it is in the 
          // left, middle, or right hand side of the item
          PRInt32 xc = -1;
          if (naturalOrder) {
            if (pnt.x <= (rect.x + (rect.width / 4))) {
              xc = rect.x-tbRect.x;
            } else if (pnt.x >= (rect.x + PRInt32(float(rect.width) *0.75))) {
              xc = rect.x-tbRect.x+rect.width-onePixel;
            } else {
              //printf("no-op\n");
            }
          } else {
            xc = rect.x-tbRect.x;
          }
          aXLoc = xc;
          aIsLegalChild = PR_TRUE;
        }
        return PR_TRUE;
      }
    }

    nsresult rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(rv == NS_OK,"failed to get next child");
    count++;
  }

  if (!found) {
    aXLoc = prevRect.x - tbRect.x;
  }

  return PR_FALSE;

}


//
// DragOver
//
// The mouse has moved over the toolbar. Update the drop feedback
//
//еее rewrite to not re-load the service on each mouse moved event. That
//еее seriously blows.
nsresult
nsToolbarDragListener::DragOver(nsIDOMEvent* aDragEvent)
{
  // now tell the drag session whether we can drop here
  nsIDragService* dragService;
  nsresult rv = nsServiceManager::GetService(kCDragServiceCID,
                                           nsIDragService::GetIID(),
                                           (nsISupports **)&dragService);
  if ( NS_SUCCEEDED(rv) ) {
    nsCOMPtr<nsIDragSession> dragSession(do_QueryInterface(dragService));
    if ( dragSession ) {
      PRBool flavorSupported = PR_FALSE;
      dragSession->IsDataFlavorSupported(TOOLBARITEM_MIME, &flavorSupported);
      if ( flavorSupported ) {
        dragSession->SetCanDrop(PR_TRUE);

        // Check to see if the mouse is over an item
        nscoord xLoc;
        PRBool isLegalChild;
        IsOnToolbarItem(aDragEvent, xLoc, isLegalChild);

        if (xLoc != mCurrentDropLoc) {
          mToolbar->SetDropfeedbackLocation(xLoc);

          // force the toolbar frame to redraw
          ForceDrawFrame(mToolbar);

          // cache the current drop location
          mCurrentDropLoc = xLoc;

          rv = NS_ERROR_BASE; // means I am consuming the event
        }
      } 
    } 
    nsServiceManager::ReleaseService(kCDragServiceCID, dragService);
  } 
  else
    rv = NS_OK; // don't consume event

  // NS_OK means event is NOT consumed
  return rv; 
}


////////////////////////////////////////////////////////////////////////
nsresult
nsToolbarDragListener::DragExit(nsIDOMEvent* aDragEvent)
{
  // now tell the drag session whether we can drop here
  nsresult rv;
  NS_WITH_SERVICE ( nsIDragService, dragService, kCDragServiceCID, &rv );
  if ( NS_SUCCEEDED(rv) ) {
    nsCOMPtr<nsIDragSession> dragSession(do_QueryInterface(dragService));
    if ( dragSession ) {
      PRBool flavorSupported = PR_FALSE;
      dragSession->IsDataFlavorSupported(TOOLBARITEM_MIME, &flavorSupported);
      if ( flavorSupported ) {
        mToolbar->SetDropfeedbackLocation(-1); // clears drawing of marker
        ForceDrawFrame(mToolbar);
        rv = NS_ERROR_BASE; // consume event
      }
    }    
  }
  else
    rv = NS_OK; // don't consume event

  return rv;
}



////////////////////////////////////////////////////////////////////////
nsresult
nsToolbarDragListener::DragDrop(nsIDOMEvent* aMouseEvent)
{
  mToolbar->SetDropfeedbackLocation(-1); // clears drawing of marker

  // XXX At the moment toolbar drags contain "text"
  // in the future they will probably contain some form of content
  // that will be translated into some RDF form
  ForceDrawFrame(mToolbar);

  // Create drag service for getting state of drag
  nsIDragService* dragService;
  nsresult rv = nsServiceManager::GetService(kCDragServiceCID,
                                             nsIDragService::GetIID(),
                                             (nsISupports **)&dragService);
  if (NS_OK == rv) {
    nsCOMPtr<nsIDragSession> dragSession(do_QueryInterface(dragService));
  
    if (dragSession) {

      // Create transferable for getting the drag data
      nsCOMPtr<nsITransferable> trans;
      rv = nsComponentManager::CreateInstance(kCTransferableCID, nsnull, 
                                              nsITransferable::GetIID(), 
                                              (void**) getter_AddRefs(trans));
      if ( NS_SUCCEEDED(rv) && trans ) {
        // Add the toolbar item flavor to the transferable, 
        // because that is the only type of data we are looking for at the moment.
        trans->AddDataFlavor(TOOLBARITEM_MIME);
 
        // Fill the transferable with data for each drag item in succession
        PRUint32 numItems = 0; 
        if (NS_SUCCEEDED(dragSession->GetNumDropItems(&numItems))) { 

          //printf("Num Drop Items %d\n", numItems); 

          PRUint32 i; 
          for (i=0;i<numItems;++i) {
            if (NS_SUCCEEDED(dragSession->GetData(trans, i))) { 
 
              // Get the string data out of the transferable. This obviously needs to be rewritten ;)
              PRUint32 len;
              char* toolbarItemMime;
              nsCOMPtr<nsISupports> genericDataObj;
              trans->GetAnyTransferData(&toolbarItemMime, getter_AddRefs(genericDataObj), &len);
              if ( genericDataObj ) {
                nsCOMPtr<nsISupportsWString> dataObj ( do_QueryInterface(genericDataObj) );
                if ( dataObj ) {               
                  // If the string was not empty then paste it in
                  PRUnichar* buf;
                  dataObj->ToString(&buf);
                  if ( buf ) {
                    nsAutoString converter(buf);
                    char* lame = converter.ToNewCString();
                    printf("Dropped Data: %s\n", lame);
                    //XXX do real stuff here
                    dragSession->SetCanDrop(PR_TRUE);
                    delete [] lame;
                  }
                  delete [] buf;
                }
              }
              delete [] toolbarItemMime;
            }
          } // foreach drag item

          // this clear the active state of the button that was pressed to do the drag
          // if the drag came from outside the app then make this call should have no effect
          nsIEventStateManager *stateManager;
          if (NS_OK == mPresContext->GetEventStateManager(&stateManager)) {
            //stateManager->SetContentState(nsnull, NS_EVENT_STATE_DRAGOVER);
            stateManager->SetContentState(nsnull, NS_EVENT_STATE_ACTIVE);
            NS_RELEASE(stateManager);
          }

        }
      } // if valid transferable
    } // if valid drag session
    nsServiceManager::ReleaseService(kCDragServiceCID, dragService);
  } // if valid drag service

  return NS_ERROR_BASE; // consumes the event
}

