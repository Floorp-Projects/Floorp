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
#include "nsIDOMDragListener.h"
#include "nsIDOMEventReceiver.h"

// Drag & Drop, Clipboard
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsIClipboard.h"
#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "nsITransferable.h"
#include "nsIFormatConverter.h"
#include "nsCOMPtr.h"
#include "nsIDOMUIEvent.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsXULAtoms.h"

#include "nsIViewManager.h"
#include "nsIView.h"

// Drag & Drop, Clipboard Support
static NS_DEFINE_CID(kCDragServiceCID,         NS_DRAGSERVICE_CID);
static NS_DEFINE_CID(kCTransferableCID,        NS_TRANSFERABLE_CID);
static NS_DEFINE_IID(kCDataFlavorCID,          NS_DATAFLAVOR_CID);
static NS_DEFINE_IID(kCXIFFormatConverterCID,  NS_XIFFORMATCONVERTER_CID);


/*
 * nsToolbarDragListener implementation
 */

NS_IMPL_ADDREF(nsToolbarDragListener)

NS_IMPL_RELEASE(nsToolbarDragListener)


nsToolbarDragListener::nsToolbarDragListener() 
{
  NS_INIT_REFCNT();
}

nsToolbarDragListener::~nsToolbarDragListener() 
{
}

nsresult
nsToolbarDragListener::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(nsISupports::GetIID())) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIDOMEventListener::GetIID())) {
    *aInstancePtr = (void*)(nsIDOMEventListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIDOMDragListener::GetIID())) {
    *aInstancePtr = (void*)(nsIDOMDragListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

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
      viewMgr->UpdateView(view, rect, NS_VMREFRESH_AUTO_DOUBLE_BUFFER);
  }

}



nsresult
nsToolbarDragListener::HandleEvent(nsIDOMEvent* aEvent)
{
  //printf("nsToolbarDragListener::HandleEvent\n");
  return NS_OK;
}



nsresult
nsToolbarDragListener::DragEnter(nsIDOMEvent* aDragEvent)
{
  //printf("nsToolbarDragListener::DragEnter\n");
  nsIDragService* dragService;
  nsresult rv = nsServiceManager::GetService(kCDragServiceCID,
                                             nsIDragService::GetIID(),
                                             (nsISupports **)&dragService);
  if (NS_OK == rv) {
    nsCOMPtr<nsIDragSession> dragSession(do_QueryInterface(dragService));

    nsAutoString textFlavor(kTextMime);
    if (dragSession && 
        (NS_OK == dragSession->IsDataFlavorSupported(&textFlavor))) {
      dragSession->SetCanDrop(PR_TRUE);
    }
    
    nsServiceManager::ReleaseService(kCDragServiceCID, dragService);
  }
  //DragOver(aDragEvent);
  return NS_OK;
}


nsresult
nsToolbarDragListener::DragOver(nsIDOMEvent* aDragEvent)
{

  nsCOMPtr<nsIDOMUIEvent> uiEvent(do_QueryInterface(aDragEvent));
  PRInt32 x,y = 0;
  uiEvent->GetClientX(&x);
  uiEvent->GetClientY(&y);
  /*{
      nsCOMPtr<nsIContent> content;
      nsresult result = mToolbar->GetContent(getter_AddRefs(content));
      if (NS_OK == result) {
        nsCOMPtr<nsIDOMElement> element;
        element = do_QueryInterface(content);
        if (nsnull != element ) {
          //printf("===================================================Got element\n");
          nsAutoString value;
          element->GetAttribute(nsAutoString("naturalorder"), value);
          printf("---------------------------> [%s]\n", value.ToNewCString());
        }
      }
  }*/

  nsCOMPtr<nsIDOMNode> contentDOMNode;
  aDragEvent->GetTarget(getter_AddRefs(contentDOMNode));
  if (contentDOMNode) {
    //printf("===================================================Got node\n");
  }
             
  float p2t;
  mPresContext->GetScaledPixelsToTwips(&p2t);
  nscoord xp = NSIntPixelsToTwips(x, p2t);
  nscoord yp = NSIntPixelsToTwips(y, p2t);
  nsPoint pnt(xp,yp);
  //printf("nsToolbarDragListener::DragOver  %d,%d   %d,%d\n", xp,yp, x, y);

  nsRect r;
  mToolbar->GetRect(r);
  //printf("TB: %d %d %d %d\n", r.x, r.y, r.width, r.height);

  nscoord passes = 0; 
  nscoord changedIndex = -1;
  nscoord count = 0;
  nsIFrame* childFrame;
  mToolbar->FirstChild(nsnull, &childFrame); 
  nsRect rect;
  nsRect prevRect;
  PRBool found = PR_FALSE;

  nscoord lastChildX = -1;

  while (nsnull != childFrame) {    
    prevRect = rect;
    childFrame->GetRect(rect);
    rect.MoveBy(r.x, r.y);
    //printf("%d %d %d %d\n", rect.x, rect.y, rect.width, rect.height);
    if (pnt.x < rect.x && lastChildX == -1) {
      lastChildX = rect.x;
    }
    if (rect.Contains(pnt)) {
      //printf("**************** Over child %d\n", count);
      nsCOMPtr<nsIContent> content;
      nsresult result = childFrame->GetContent(getter_AddRefs(content));
      if (NS_OK == result) {
        nsCOMPtr<nsIAtom> tag;
        content->GetTag(*getter_AddRefs(tag));
        if (tag.get() == nsXULAtoms::titledbutton) {
          //printf("Got element\n");
          found = PR_TRUE;
          if (count == 0) {
            mToolbar->SetDropfeedbackLocation(rect.x-r.x);
          } else {
            mToolbar->SetDropfeedbackLocation(rect.x-r.x);
          }
          ForceDrawFrame(mToolbar);
          break;
        }

        /*nsCOMPtr<nsIDOMNode> domNode;
        domNode = do_QueryInterface(content);
        if (nsnull != domNode ) {
          nsCOMPtr<nsIAtom> tag;
          mContent->GetTag(*getter_AddRefs(tag));
          if (tag.get() == nsXULAtoms::titlebutton) {
            printf("===================================================Got element\n");
            break;
          }
          //nsAutoString value;
          //element->GetAttribute(nsAutoString("naturalorder"), value);
          //printf("-----> [%s]\n", value.ToNewCString());

        }*/
      }

    }

    if (!found) {
      mToolbar->SetDropfeedbackLocation(lastChildX-r.x);
      ForceDrawFrame(mToolbar);
    }

    nsresult rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(rv == NS_OK,"failed to get next child");
    count++;
  }





  nsIDragService* dragService;
  nsresult rv = nsServiceManager::GetService(kCDragServiceCID,
                                           nsIDragService::GetIID(),
                                           (nsISupports **)&dragService);
  if (NS_OK == rv) {
    nsCOMPtr<nsIDragSession> dragSession(do_QueryInterface(dragService));
    nsAutoString textFlavor(kTextMime);
    if (dragSession && NS_OK == dragSession->IsDataFlavorSupported(&textFlavor)) {
      dragSession->SetCanDrop(PR_TRUE);
    } 
    
    nsServiceManager::ReleaseService(kCDragServiceCID, dragService);
  }

  return NS_OK;
}


nsresult
nsToolbarDragListener::DragExit(nsIDOMEvent* aDragEvent)
{
  mToolbar->SetDropfeedbackLocation(-1); // clears drawing of marker
  ForceDrawFrame(mToolbar);
  //printf("nsToolbarDragListener::DragExit\n");
  return NS_OK;
}



nsresult
nsToolbarDragListener::DragDrop(nsIDOMEvent* aMouseEvent)
{
  mToolbar->SetDropfeedbackLocation(-1); // clears drawing of marker
  ForceDrawFrame(mToolbar);
  //printf("nsToolbarDragListener::DragDrop\n");
  // String for doing paste
  nsString stuffToPaste;

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
        // Add the text Flavor to the transferable, 
        // because that is the only type of data we are
        // looking for at the moment.
        nsAutoString textMime (kTextMime);
        trans->AddDataFlavor(&textMime);
        //trans->AddDataFlavor(mImageDataFlavor);

        // Fill the transferable with data for each drag item in succession
        PRUint32 numItems = 0; 
        if (NS_SUCCEEDED(dragSession->GetNumDropItems(&numItems))) { 

          //printf("Num Drop Items %d\n", numItems); 

          PRUint32 i; 
          for (i=0;i<numItems;++i) {
            if (NS_SUCCEEDED(dragSession->GetData(trans, i))) { 
 
              // Get the string data out of the transferable
              // Note: the transferable owns the pointer to the data
              char *str = 0;
              PRUint32 len;
              trans->GetAnyTransferData(&textMime, (void **)&str, &len);

              // If the string was not empty then paste it in
              if (str) {
                printf("Dropped: %s\n", str);
                stuffToPaste.SetString(str, len);
                //mEditor->InsertText(stuffToPaste);
                dragSession->SetCanDrop(PR_TRUE);
              }

              // XXX This is where image support might go
              //void * data;
              //trans->GetTransferData(mImageDataFlavor, (void **)&data, &len);
            }
          } // foreach drag item
        }
      } // if valid transferable
    } // if valid drag session
    nsServiceManager::ReleaseService(kCDragServiceCID, dragService);
  } // if valid drag service

  return NS_OK;
}

