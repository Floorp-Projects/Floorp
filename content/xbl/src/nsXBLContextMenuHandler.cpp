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
 */

#include "nsCOMPtr.h"
#include "nsIXBLPrototypeHandler.h"
#include "nsXBLContextMenuHandler.h"
#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsIDOMMouseEvent.h"
#include "nsINameSpaceManager.h"
#include "nsIScriptContext.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIJSEventListener.h"
#include "nsIController.h"
#include "nsIControllers.h"
#include "nsIDOMXULElement.h"
#include "nsIDOMNSHTMLTextAreaElement.h"
#include "nsIDOMNSHTMLInputElement.h"
#include "nsIDOMText.h"
#include "nsIEventListenerManager.h"
#include "nsIDOMEventReceiver.h"
#include "nsXBLBinding.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMWindowInternal.h"
#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsIURI.h"
#include "nsXPIDLString.h"

PRUint32 nsXBLContextMenuHandler::gRefCnt = 0;
nsIAtom* nsXBLContextMenuHandler::kContextMenuAtom = nsnull;

nsXBLContextMenuHandler::nsXBLContextMenuHandler(nsIDOMEventReceiver* aReceiver, nsIXBLPrototypeHandler* aHandler)
:nsXBLEventHandler(aReceiver,aHandler)
{
  gRefCnt++;
  if (gRefCnt == 1) {
    kContextMenuAtom = NS_NewAtom("contextmenu");
  }
}

nsXBLContextMenuHandler::~nsXBLContextMenuHandler()
{
  gRefCnt--;
  if (gRefCnt == 0) {
    NS_RELEASE(kContextMenuAtom);
  }
}

NS_IMPL_ISUPPORTS_INHERITED1(nsXBLContextMenuHandler, nsXBLEventHandler, nsIDOMContextMenuListener)

static inline nsresult DoContextMenu(nsIAtom* aEventType, nsIXBLPrototypeHandler* aHandler, 
                                     nsIDOMEvent* aContextMenuEvent,
                                     nsIDOMEventReceiver* aReceiver)
{
  if (!aHandler)
    return NS_OK;

  PRBool matched = PR_FALSE;
  nsCOMPtr<nsIDOMMouseEvent> contextMenu(do_QueryInterface(aContextMenuEvent));
  aHandler->MouseEventMatched(aEventType, contextMenu, &matched);

  if (matched)
    aHandler->ExecuteHandler(aReceiver, aContextMenuEvent);
  
  return NS_OK;
}

nsresult nsXBLContextMenuHandler::ContextMenu(nsIDOMEvent* aContextMenuEvent)
{
  return DoContextMenu(kContextMenuAtom, mProtoHandler, aContextMenuEvent, mEventReceiver);
}

///////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLContextMenuHandler(nsIDOMEventReceiver* aRec, nsIXBLPrototypeHandler* aHandler,
                      nsXBLContextMenuHandler** aResult)
{
  *aResult = new nsXBLContextMenuHandler(aRec, aHandler);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
