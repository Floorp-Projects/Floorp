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
// Significant portions of the collapse/expanding code donated by Chris Lattner
// (sabre@skylab.org). Thanks Chris!
//
// See documentation in associated header file
//

#include "nsToolboxFrame.h"
#include "nsToolbarFrame.h" // needed for MIME definitions

#include "nsIStyleContext.h"
#include "nsCSSRendering.h"
#include "nsIReflowCommand.h"
#include "nsHTMLIIDs.h"
#include "nsIPresContext.h"
#include "nsIWidget.h"
#include "nsINameSpaceManager.h"
#include "nsBoxLayoutState.h"

#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "nsITransferable.h"
#include "nsIFormatConverter.h"
#include "nsCOMPtr.h"
#include "nsIDOMUIEvent.h"
#include "nsIDOMDragListener.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMEventListener.h"
#include "nsISupportsPrimitives.h"
#include "nsISupportsArray.h"
#include "nsHTMLAtoms.h"


// Drag & Drop, Clipboard Support
static NS_DEFINE_CID(kCDragServiceCID,         NS_DRAGSERVICE_CID);
static NS_DEFINE_CID(kCTransferableCID,        NS_TRANSFERABLE_CID);
static NS_DEFINE_IID(kCDataFlavorCID,          NS_DATAFLAVOR_CID);
static NS_DEFINE_IID(kCXIFFormatConverterCID,  NS_XIFFORMATCONVERTER_CID);


NS_IMPL_ADDREF(nsToolboxFrame::DragListenerDelegate);
NS_IMPL_RELEASE(nsToolboxFrame::DragListenerDelegate);
NS_IMPL_QUERY_INTERFACE2(nsToolboxFrame::DragListenerDelegate, nsIDOMDragListener, nsIDOMEventListener)


//
// NS_NewToolboxFrame
//
// Creates a new toolbox frame and returns it in |aNewFrame|
//
nsresult
NS_NewToolboxFrame ( nsIPresShell* aPresShell, nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsToolboxFrame* it = new (aPresShell) nsToolboxFrame (aPresShell);
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  //it->SetFlags(aFlags);
  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewToolboxFrame


//
// nsToolboxFrame cntr
//
// Init, if necessary
//
nsToolboxFrame :: nsToolboxFrame (nsIPresShell* aShell):nsBoxFrame(aShell)
  , mDragListenerDelegate(nsnull)
{
}

//
// nsToolboxFrame dstr
//
// Cleanup, as necessary
//
nsToolboxFrame :: ~nsToolboxFrame ( )
{
  if (mDragListenerDelegate) {
    mDragListenerDelegate->NotifyFrameDestroyed();
    NS_RELEASE(mDragListenerDelegate);
  }
}



NS_IMETHODIMP
nsToolboxFrame::Init(nsIPresContext*  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsIStyleContext* aContext,
              nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  
  // Register the delegate as a drag listener.
  mDragListenerDelegate = new DragListenerDelegate(this);
  if (! mDragListenerDelegate)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(mDragListenerDelegate);

  nsCOMPtr<nsIContent> content;
  rv = GetContent(getter_AddRefs(content));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDOMEventReceiver> reciever(do_QueryInterface(content));

  reciever->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMDragListener*, mDragListenerDelegate), NS_GET_IID(nsIDOMDragListener));

  return rv;
}

////////////////////////////////////////////////////////////////////////
nsresult
nsToolboxFrame::HandleEvent(nsIDOMEvent* aEvent)
{
  //printf("nsToolbarDragListener::HandleEvent\n");
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
nsresult
nsToolboxFrame::DragEnter(nsIDOMEvent* aDragEvent)
{
  nsIDragService* dragService;
  nsresult rv = nsServiceManager::GetService(kCDragServiceCID,
                                             NS_GET_IID(nsIDragService),
                                             (nsISupports **)&dragService);
  if ( NS_SUCCEEDED(rv) ) {
    nsCOMPtr<nsIDragSession> dragSession(do_QueryInterface(dragService));

    if ( dragSession ) {
      PRBool flavorSupported = PR_FALSE;
      dragSession->IsDataFlavorSupported(TOOLBAR_MIME, &flavorSupported);
      if ( flavorSupported ) {
        dragSession->SetCanDrop(PR_TRUE);
        rv = NS_ERROR_BASE; // consume event
      }
    }
    
    nsServiceManager::ReleaseService(kCDragServiceCID, dragService);
  } else {
    rv = NS_OK;
  }
  return rv; 
}


////////////////////////////////////////////////////////////////////////
nsresult
nsToolboxFrame::DragOver(nsIDOMEvent* aDragEvent)
{
  // now tell the drag session whether we can drop here
  nsIDragService* dragService;
  nsresult rv = nsServiceManager::GetService(kCDragServiceCID, NS_GET_IID(nsIDragService),
                                              (nsISupports **)&dragService);
  if ( NS_SUCCEEDED(rv) ) {
    nsCOMPtr<nsIDragSession> dragSession(do_QueryInterface(dragService));

    if ( dragSession ) {
      PRBool flavorSupported = PR_FALSE;
      dragSession->IsDataFlavorSupported(TOOLBAR_MIME, &flavorSupported);
      if ( flavorSupported ) {
        // Right here you need to figure out where the mouse is 
        // and whether you can drop here

        dragSession->SetCanDrop(PR_TRUE);
        rv = NS_ERROR_BASE; // consume event
      }
    }
    
    nsServiceManager::ReleaseService(kCDragServiceCID, dragService);
  }

  // NS_OK means event is NOT consumed
  return rv; 
}


////////////////////////////////////////////////////////////////////////
nsresult
nsToolboxFrame::DragExit(nsIDOMEvent* aDragEvent)
{
  return NS_ERROR_BASE; // consumes event
}



////////////////////////////////////////////////////////////////////////
nsresult
nsToolboxFrame::DragDrop(nsIDOMEvent* aMouseEvent)
{
  // Create drag service for getting state of drag
  nsIDragService* dragService;
  nsresult rv = nsServiceManager::GetService(kCDragServiceCID,
                                             NS_GET_IID(nsIDragService),
                                             (nsISupports **)&dragService);
  if (NS_OK == rv) {
    nsCOMPtr<nsIDragSession> dragSession(do_QueryInterface(dragService));
  
    if (dragSession) {

      // Create transferable for getting the drag data
      nsCOMPtr<nsITransferable> trans;
      rv = nsComponentManager::CreateInstance(kCTransferableCID, nsnull, 
                                              NS_GET_IID(nsITransferable), 
                                              (void**) getter_AddRefs(trans));
      if ( NS_SUCCEEDED(rv) && trans ) {
        // Add the toolbar Flavor to the transferable, because that is the only type of data we are
        // looking for at the moment.
        trans->AddDataFlavor(TOOLBAR_MIME);

        // Fill the transferable with data for each drag item in succession
        PRUint32 numItems = 0; 
        if (NS_SUCCEEDED(dragSession->GetNumDropItems(&numItems))) { 

          //printf("Num Drop Items %d\n", numItems); 

          PRUint32 i; 
          for (i=0;i<numItems;++i) {
            if (NS_SUCCEEDED(dragSession->GetData(trans, i))) { 
 
              // Get the string data out of the transferable as a nsISupportsString.
              nsCOMPtr<nsISupports> data;
              PRUint32 len;
              char* whichFlavor = nsnull;
              trans->GetAnyTransferData(&whichFlavor, getter_AddRefs(data), &len);
              nsCOMPtr<nsISupportsString> dataAsString ( do_QueryInterface(data) );

              // If the string was not empty then make it so.
              if ( dataAsString ) {
                char* stuffToPaste;
                dataAsString->ToString ( &stuffToPaste );
                printf("Dropped: %s\n", stuffToPaste);
                dragSession->SetCanDrop(PR_TRUE);
              }
              
              nsAllocator::Free ( whichFlavor );
            }
          } // foreach drag item
        }
      } // if valid transferable
    } // if valid drag session
    nsServiceManager::ReleaseService(kCDragServiceCID, dragService);
  } // if valid drag service

  return NS_ERROR_BASE; // consume the event;
}


