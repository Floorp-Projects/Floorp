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
#include "nsXBLMouseHandler.h"
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

PRUint32 nsXBLMouseHandler::gRefCnt = 0;
nsIAtom* nsXBLMouseHandler::kMouseDownAtom = nsnull;
nsIAtom* nsXBLMouseHandler::kMouseUpAtom = nsnull;
nsIAtom* nsXBLMouseHandler::kMouseClickAtom = nsnull;
nsIAtom* nsXBLMouseHandler::kMouseDblClickAtom = nsnull;
nsIAtom* nsXBLMouseHandler::kMouseOverAtom = nsnull;
nsIAtom* nsXBLMouseHandler::kMouseOutAtom = nsnull;

nsXBLMouseHandler::nsXBLMouseHandler(nsIDOMEventReceiver* aReceiver, nsIXBLPrototypeHandler* aHandler,
                                     nsIAtom* aEventName)
:nsXBLEventHandler(aReceiver,aHandler,aEventName)
{
  gRefCnt++;
  if (gRefCnt == 1) {
    kMouseDownAtom = NS_NewAtom("mousedown");
    kMouseUpAtom = NS_NewAtom("mouseup");
    kMouseClickAtom = NS_NewAtom("click");
    kMouseDblClickAtom = NS_NewAtom("dblclick");
    kMouseOverAtom = NS_NewAtom("mouseover");
    kMouseOutAtom = NS_NewAtom("mouseout");
  }
}

nsXBLMouseHandler::~nsXBLMouseHandler()
{
  gRefCnt--;
  if (gRefCnt == 0) {
    NS_RELEASE(kMouseUpAtom);
    NS_RELEASE(kMouseDownAtom);
    NS_RELEASE(kMouseClickAtom);
    NS_RELEASE(kMouseDblClickAtom);
    NS_RELEASE(kMouseOverAtom);
    NS_RELEASE(kMouseOutAtom);
  }
}

NS_IMPL_ISUPPORTS_INHERITED1(nsXBLMouseHandler, nsXBLEventHandler, nsIDOMMouseListener)

nsresult nsXBLMouseHandler::MouseDown(nsIDOMEvent* aMouseEvent)
{
  if (mEventName.get() != kMouseDownAtom)
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

nsresult nsXBLMouseHandler::MouseUp(nsIDOMEvent* aMouseEvent)
{
  if (mEventName.get() != kMouseUpAtom)
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

nsresult nsXBLMouseHandler::MouseClick(nsIDOMEvent* aMouseEvent)
{
  if (mEventName.get() != kMouseClickAtom)
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

nsresult nsXBLMouseHandler::MouseDblClick(nsIDOMEvent* aMouseEvent)
{
  if (mEventName.get() != kMouseDblClickAtom)
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

nsresult nsXBLMouseHandler::MouseOver(nsIDOMEvent* aMouseEvent)
{
  if (mEventName.get() != kMouseOverAtom)
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

nsresult nsXBLMouseHandler::MouseOut(nsIDOMEvent* aMouseEvent)
{
  if (mEventName.get() != kMouseOutAtom)
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
NS_NewXBLMouseHandler(nsIDOMEventReceiver* aRec, nsIXBLPrototypeHandler* aHandler, 
                    nsIAtom* aEventName,
                    nsXBLMouseHandler** aResult)
{
  *aResult = new nsXBLMouseHandler(aRec, aHandler, aEventName);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
