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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s):
 *  - Mike Pinkerton (pinkerton@netscape.com)
 */

#include "nsCOMPtr.h"
#include "nsIXBLPrototypeHandler.h"
#include "nsXBLWindowKeyHandler.h"
#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMEventReceiver.h"
#include "nsXBLService.h"
#include "nsIServiceManager.h"
#include "nsHTMLAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsIXBLDocumentInfo.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMElement.h"

PRUint32 nsXBLWindowKeyHandler::gRefCnt = 0;
nsIAtom* nsXBLWindowKeyHandler::kKeyDownAtom = nsnull;
nsIAtom* nsXBLWindowKeyHandler::kKeyUpAtom = nsnull;
nsIAtom* nsXBLWindowKeyHandler::kKeyPressAtom = nsnull;


nsXBLWindowKeyHandler::nsXBLWindowKeyHandler(nsIDOMElement* aElement, nsIDOMEventReceiver* aReceiver)
  : nsXBLWindowHandler(aElement, aReceiver)
{
  NS_INIT_ISUPPORTS();

  gRefCnt++;
  if (gRefCnt == 1) {
    kKeyUpAtom = NS_NewAtom("keyup");
    kKeyDownAtom = NS_NewAtom("keydown");
    kKeyPressAtom = NS_NewAtom("keypress");
  }
}

nsXBLWindowKeyHandler::~nsXBLWindowKeyHandler()
{
  gRefCnt--;
  if (gRefCnt == 0) {
    NS_RELEASE(kKeyUpAtom);
    NS_RELEASE(kKeyDownAtom);
    NS_RELEASE(kKeyPressAtom);
  }
}

NS_IMPL_ISUPPORTS1(nsXBLWindowKeyHandler, nsIDOMKeyListener)


//
// EnsureHandlers
//    
// Lazily load the XBL handlers. Overridden to handle being attached
// to a particular element rather than the document
//
NS_IMETHODIMP
nsXBLWindowKeyHandler::EnsureHandlers()
{
  nsresult rv = NS_ERROR_FAILURE;
  
  if (mElement) {
    if (mHandler)
      return NS_OK;
    nsCOMPtr<nsIContent> content(do_QueryInterface(mElement));
    nsCOMPtr<nsIXBLPrototypeHandler> dummy;
    rv = nsXBLService::BuildHandlerChain(content, getter_AddRefs(mHandler), getter_AddRefs(dummy));
  }
  else
    nsXBLWindowHandler::EnsureHandlers();
  
  return rv;
}


NS_IMETHODIMP
nsXBLWindowKeyHandler::WalkHandlers(nsIDOMEvent* aKeyEvent, nsIAtom* aEventType)
{
  nsCOMPtr<nsIDOMNSUIEvent> evt = do_QueryInterface(aKeyEvent);
  PRBool prevent;
  evt->GetPreventDefault(&prevent);
  if (prevent)
    return NS_OK;

  // Make sure our event is really a key event
  nsCOMPtr<nsIDOMKeyEvent> keyEvent(do_QueryInterface(aKeyEvent));
  if (!keyEvent)
    return NS_OK;

  EnsureHandlers();
  
  if (!mElement) {
    WalkHandlersInternal(aKeyEvent, aEventType, mPlatformHandler);
    evt->GetPreventDefault(&prevent);
    if (prevent)
      return NS_OK; // Handled by the platform. Our work here is done.
  }

  WalkHandlersInternal(aKeyEvent, aEventType, mHandler);
  
  return NS_OK;
}

nsresult nsXBLWindowKeyHandler::KeyUp(nsIDOMEvent* aKeyEvent)
{
  return WalkHandlers(aKeyEvent, kKeyUpAtom);
}

nsresult nsXBLWindowKeyHandler::KeyDown(nsIDOMEvent* aKeyEvent)
{
  return WalkHandlers(aKeyEvent, kKeyDownAtom);
}

nsresult nsXBLWindowKeyHandler::KeyPress(nsIDOMEvent* aKeyEvent)
{
  return WalkHandlers(aKeyEvent, kKeyPressAtom);
}


//
// EventMatched
//
// See if the given handler cares about this particular key event
//
PRBool
nsXBLWindowKeyHandler :: EventMatched ( nsIXBLPrototypeHandler* inHandler, nsIAtom* inEventType, nsIDOMEvent* inEvent )
{
  PRBool matched = PR_FALSE;
  
  nsCOMPtr<nsIDOMKeyEvent> keyEvent ( do_QueryInterface(inEvent) );
  if ( keyEvent )
    inHandler->KeyEventMatched(inEventType, keyEvent, &matched);
  
  return matched;
}

 
///////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLWindowKeyHandler(nsIDOMElement* aElement, nsIDOMEventReceiver* aReceiver, nsXBLWindowKeyHandler** aResult)
{
  *aResult = new nsXBLWindowKeyHandler(aElement, aReceiver);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
