/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   - Mike Pinkerton (pinkerton@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsXBLPrototypeHandler.h"
#include "nsXBLWindowDragHandler.h"
#include "nsXBLAtoms.h"
#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIDOMMouseEvent.h"
#include "nsIDOMEventReceiver.h"
#include "nsXBLService.h"
#include "nsIServiceManager.h"
#include "nsHTMLAtoms.h"
#include "nsIXBLDocumentInfo.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMElement.h"

nsXBLWindowDragHandler::nsXBLWindowDragHandler(nsIDOMEventReceiver* aReceiver)
  : nsXBLWindowHandler(nsnull, aReceiver)
{
}

nsXBLWindowDragHandler::~nsXBLWindowDragHandler()
{
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

  EnsureHandlers(nsnull);
  
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
  return WalkHandlers(aDragEvent, nsXBLAtoms::draggesture);
}

nsresult 
nsXBLWindowDragHandler::DragEnter(nsIDOMEvent* aDragEvent)
{
  return WalkHandlers(aDragEvent, nsXBLAtoms::dragenter);
}

nsresult 
nsXBLWindowDragHandler::DragExit(nsIDOMEvent* aDragEvent)
{
  return WalkHandlers(aDragEvent, nsXBLAtoms::dragexit);
}

nsresult 
nsXBLWindowDragHandler::DragOver(nsIDOMEvent* aDragEvent)
{
  return WalkHandlers(aDragEvent, nsXBLAtoms::dragevent);
}

nsresult 
nsXBLWindowDragHandler::DragDrop(nsIDOMEvent* aDragEvent)
{
  return WalkHandlers(aDragEvent, nsXBLAtoms::dragdrop);
}


//
// EventMatched
//
// See if the given handler cares about this particular key event
//
PRBool
nsXBLWindowDragHandler::EventMatched (nsXBLPrototypeHandler* inHandler,
                                      nsIAtom* inEventType,
                                      nsIDOMEvent* inEvent)
{
  nsCOMPtr<nsIDOMMouseEvent> dragEvent ( do_QueryInterface(inEvent) );
  if ( dragEvent )
    return inHandler->MouseEventMatched(inEventType, dragEvent);
  
  return PR_FALSE;
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
