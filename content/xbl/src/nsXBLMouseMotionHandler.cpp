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
#include "nsXBLMouseMotionHandler.h"
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

PRUint32 nsXBLMouseMotionHandler::gRefCnt = 0;
nsIAtom* nsXBLMouseMotionHandler::kMouseMoveAtom = nsnull;

nsXBLMouseMotionHandler::nsXBLMouseMotionHandler(nsIDOMEventReceiver* aReceiver, nsIXBLPrototypeHandler* aHandler,
                                     nsIAtom* aEventName)
:nsXBLEventHandler(aReceiver,aHandler,aEventName)
{
  gRefCnt++;
  if (gRefCnt == 1) {
    kMouseMoveAtom = NS_NewAtom("mousemove");
  }
}

nsXBLMouseMotionHandler::~nsXBLMouseMotionHandler()
{
  gRefCnt--;
  if (gRefCnt == 0) {
    NS_RELEASE(kMouseMoveAtom);
  }
}

NS_IMPL_ISUPPORTS_INHERITED1(nsXBLMouseMotionHandler, nsXBLEventHandler, nsIDOMMouseMotionListener)

nsresult nsXBLMouseMotionHandler::MouseMove(nsIDOMEvent* aMouseEvent)
{
  if (mEventName.get() != kMouseMoveAtom)
    return NS_OK;

  PRBool matched = PR_FALSE;
  if (mProtoHandler) {
    nsCOMPtr<nsIDOMMouseEvent> mouse(do_QueryInterface(aMouseEvent));
    mProtoHandler->MouseEventMatched(mouse, &matched);
  }

  if (matched)
    ExecuteHandler(mEventName, aMouseEvent);
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLMouseMotionHandler(nsIDOMEventReceiver* aRec, nsIXBLPrototypeHandler* aHandler, 
                    nsIAtom* aEventName,
                    nsXBLMouseMotionHandler** aResult)
{
  *aResult = new nsXBLMouseMotionHandler(aRec, aHandler, aEventName);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
