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

#include "nsToolbarItemFrame.h"
#include "nsCOMPtr.h"

#include "nsWidgetsCID.h"

// Drag & Drop, Clipboard Support
static NS_DEFINE_CID(kCDragServiceCID,         NS_DRAGSERVICE_CID);
static NS_DEFINE_CID(kCTransferableCID,        NS_TRANSFERABLE_CID);
static NS_DEFINE_IID(kCDataFlavorCID,          NS_DATAFLAVOR_CID);


//
// NS_NewToolbarItemFrame (friend)
//
// Creates a new toolbar item frame and returns it in |aNewFrame|
//
nsresult
NS_NewToolbarItemFrame ( nsIFrame** aNewFrame )
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if ( !aNewFrame )
    return NS_ERROR_NULL_POINTER;

  nsToolbarItemFrame* it = new nsToolbarItemFrame;
  if ( !it )
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewToolbarItemFrame


//
// nsToolbarItemFrame ctor and dtor
//
nsToolbarItemFrame::nsToolbarItemFrame()
{
}

nsToolbarItemFrame::~nsToolbarItemFrame()
{
}


//
// Init
//
// Ummm, just forwards for now.
//
NS_IMETHODIMP
nsToolbarItemFrame::Init(nsIPresContext&  aPresContext,
                          nsIContent*      aContent,
                          nsIFrame*        aParent,
                          nsIStyleContext* aContext,
                          nsIFrame*        aPrevInFlow)
{
  nsresult  rv = nsBoxFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  return rv;
}


//
// Init
//
// Ummm, just forwards for now. Most of this code is now in the drag listener's
// mouseMoved event.
//
// еее remove all this.
//
NS_IMETHODIMP
nsToolbarItemFrame::HandleEvent(nsIPresContext& aPresContext, 
                                      nsGUIEvent* aEvent,
                                      nsEventStatus& aEventStatus)
{
  // if disabled do nothing
  /*if (PR_TRUE == mRenderer.isDisabled()) {
    return NS_OK;
  }

    switch (aEvent->message) {
    case NS_KEY_PRESS:
      if (NS_KEY_EVENT == aEvent->eventStructType) {
        nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
        if (NS_VK_SPACE == keyEvent->keyCode || NS_VK_RETURN == keyEvent->keyCode) {
          MouseClicked(aPresContext);
        }
      }
      break;

    case NS_MOUSE_LEFT_CLICK:
          MouseClicked(aPresContext);
    break;
  }
*/

/* // Start Drag
  nsIDragService* dragService; 
  nsresult rv = nsServiceManager::GetService(kCDragServiceCID, 
                                             nsIDragService::GetIID(), 
                                             (nsISupports **)&dragService); 
  if (NS_OK == rv) { 
    nsCOMPtr<nsITransferable> trans; 
    rv = nsComponentManager::CreateInstance(kCTransferableCID, nsnull, 
                                              nsITransferable::GetIID(), getter_AddRefs(trans)); 
    nsCOMPtr<nsITransferable> trans2; 
    rv = nsComponentManager::CreateInstance(kCTransferableCID, nsnull, 
                                              nsITransferable::GetIID(), getter_AddRefs(trans2)); 
    if ( trans && trans2 ) {
      nsString textPlainFlavor ( "text/plain" );
      trans->AddDataFlavor(&textPlainFlavor);
      nsString dragText = "Drag Text";
      PRUint32 len = 9; 
      trans->SetTransferData(&textPlainFlavor, dragText.ToNewCString(), len);   // transferable consumes the data

      trans2->AddDataFlavor(&textPlainFlavor);
      nsString dragText2 = "More Drag Text";
      len = 14; 
      trans2->SetTransferData(&textPlainFlavor, dragText2.ToNewCString(), len);   // transferable consumes the data

      nsCOMPtr<nsISupportsArray> items;
      NS_NewISupportsArray(getter_AddRefs(items));
      if ( items ) {
        items->AppendElement(trans);
        items->AppendElement(trans2);
        dragService->InvokeDragSession(items, nsnull, nsIDragService::DRAGDROP_ACTION_COPY | nsIDragService::DRAGDROP_ACTION_MOVE);
      }
    } 
    nsServiceManager::ReleaseService(kCDragServiceCID, dragService); 
  } */
  printf("ToolbarItem %d\n", aEvent->message);
  return nsBoxFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}


/*
 * We are a frame and we do not maintain a ref count
 */
NS_IMETHODIMP_(nsrefcnt) 
nsToolbarItemFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt) 
nsToolbarItemFrame::Release(void)
{
    return NS_OK;
}



