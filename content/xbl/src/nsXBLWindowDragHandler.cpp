/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  - Mike Pinkerton (pinkerton@netscape.com)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsIXBLPrototypeHandler.h"
#include "nsXBLWindowDragHandler.h"
#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMEventReceiver.h"
#include "nsXBLService.h"
#include "nsIServiceManager.h"
#include "nsHTMLAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsIXBLDocumentInfo.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMElement.h"

PRUint32 nsXBLWindowDragHandler::gRefCnt = 0;
nsIAtom* nsXBLWindowDragHandler::kDragGestureAtom = nsnull;
nsIAtom* nsXBLWindowDragHandler::kDragOverAtom = nsnull;
nsIAtom* nsXBLWindowDragHandler::kDragEnterAtom = nsnull;
nsIAtom* nsXBLWindowDragHandler::kDragExitAtom = nsnull;
nsIAtom* nsXBLWindowDragHandler::kDragDropAtom = nsnull;

nsXBLWindowDragHandler::nsXBLWindowDragHandler(nsIDOMEventReceiver* aReceiver)
  : nsXBLWindowHandler(nsnull, aReceiver)
{
  NS_INIT_ISUPPORTS();

  gRefCnt++;
  if (gRefCnt == 1) {
    kDragEnterAtom = NS_NewAtom("dragenter");
    kDragOverAtom = NS_NewAtom("dragover");
    kDragExitAtom = NS_NewAtom("dragexit");
    kDragDropAtom = NS_NewAtom("dragdrop");
    kDragGestureAtom = NS_NewAtom("draggesture");
  }
}

nsXBLWindowDragHandler::~nsXBLWindowDragHandler()
{
  gRefCnt--;
  if (gRefCnt == 0) {
    NS_RELEASE(kDragEnterAtom);
    NS_RELEASE(kDragOverAtom);
    NS_RELEASE(kDragExitAtom);
    NS_RELEASE(kDragDropAtom);
    NS_RELEASE(kDragGestureAtom);
  }
}

NS_IMPL_ISUPPORTS1(nsXBLWindowDragHandler, nsIDOMDragListener)


NS_IMETHODIMP
nsXBLWindowDragHandler::WalkHandlers(nsIDOMEvent* aDragEvent, nsIAtom* aEventType)
{
  nsCOMPtr<nsIDOMNSUIEvent> evt = do_QueryInterface(aDragEvent);
  PRBool prevent;
  evt->GetPreventDefault(&prevent);
  if (prevent)
    return NS_OK;

  // Make sure our event is really a mouse event
  nsCOMPtr<nsIDOMMouseEvent> dragEvent(do_QueryInterface(aDragEvent));
  if (!dragEvent)
    return NS_OK;

  EnsureHandlers();
  
  if (!mElement) {
    WalkHandlersInternal(aDragEvent, aEventType, mPlatformHandler);
    evt->GetPreventDefault(&prevent);
    if (prevent)
      return NS_OK; // Handled by the platform. Our work here is done.
  }

  WalkHandlersInternal(aDragEvent, aEventType, mHandler);
  
  return NS_OK;
}


//
// DragGesture
// DragEnter
// DragExit
// DragOver
// DragDrop
//
// On the particular event, walk the handlers we loaded in to
// do something with the event.
//
nsresult 
nsXBLWindowDragHandler::DragGesture(nsIDOMEvent* aDragEvent)
{
  return WalkHandlers(aDragEvent, kDragGestureAtom);
}

nsresult 
nsXBLWindowDragHandler::DragEnter(nsIDOMEvent* aDragEvent)
{
  return WalkHandlers(aDragEvent, kDragEnterAtom);
}

nsresult 
nsXBLWindowDragHandler::DragExit(nsIDOMEvent* aDragEvent)
{
  return WalkHandlers(aDragEvent, kDragExitAtom);
}

nsresult 
nsXBLWindowDragHandler::DragOver(nsIDOMEvent* aDragEvent)
{
  return WalkHandlers(aDragEvent, kDragOverAtom);
}

nsresult 
nsXBLWindowDragHandler::DragDrop(nsIDOMEvent* aDragEvent)
{
  return WalkHandlers(aDragEvent, kDragDropAtom);
}


//
// EventMatched
//
// See if the given handler cares about this particular key event
//
PRBool
nsXBLWindowDragHandler :: EventMatched ( nsIXBLPrototypeHandler* inHandler, nsIAtom* inEventType, nsIDOMEvent* inEvent )
{
  PRBool matched = PR_FALSE;
  
  nsCOMPtr<nsIDOMMouseEvent> dragEvent ( do_QueryInterface(inEvent) );
  if ( dragEvent )
    inHandler->MouseEventMatched(inEventType, dragEvent, &matched);
  
  return matched;
}

 
///////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLWindowDragHandler(nsIDOMEventReceiver* aReceiver, nsXBLWindowDragHandler** aResult)
{
  *aResult = new nsXBLWindowDragHandler(aReceiver);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
