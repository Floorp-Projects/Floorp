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
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
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
#include "nsXBLWindowKeyHandler.h"
#include "nsXBLAtoms.h"
#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMNSEvent.h"
#include "nsXBLService.h"
#include "nsIServiceManager.h"
#include "nsHTMLAtoms.h"
#include "nsIXBLDocumentInfo.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMElement.h"
#include "nsXBLAtoms.h"
#include "nsINativeKeyBindings.h"
#include "nsIController.h"
#include "nsIControllers.h"
#include "nsIDOMWindowInternal.h"
#include "nsIFocusController.h"
#include "nsPIWindowRoot.h"

static nsINativeKeyBindings *sNativeEditorBindings = nsnull;

nsXBLWindowKeyHandler::nsXBLWindowKeyHandler(nsIDOMElement* aElement, nsIDOMEventReceiver* aReceiver)
  : nsXBLWindowHandler(aElement, aReceiver)
{
}

nsXBLWindowKeyHandler::~nsXBLWindowKeyHandler()
{
  // If mElement is non-null, we created a prototype handler.
  if (mElement)
    delete mHandler;
}

NS_IMPL_ISUPPORTS1(nsXBLWindowKeyHandler, nsIDOMKeyListener)

static void
BuildHandlerChain(nsIContent* aContent, nsXBLPrototypeHandler** aResult)
{
  nsXBLPrototypeHandler *firstHandler = nsnull, *currHandler = nsnull;

  PRUint32 handlerCount = aContent->GetChildCount();
  for (PRUint32 j = 0; j < handlerCount; j++) {
    nsIContent *handler = aContent->GetChildAt(j);

    nsXBLPrototypeHandler* newHandler = new nsXBLPrototypeHandler(handler);

    if (newHandler) {
      if (currHandler)
        currHandler->SetNextHandler(newHandler);
      else firstHandler = newHandler;
      currHandler = newHandler;
    }
  }

  *aResult = firstHandler;
}

//
// EnsureHandlers
//    
// Lazily load the XBL handlers. Overridden to handle being attached
// to a particular element rather than the document
//
nsresult
nsXBLWindowKeyHandler::EnsureHandlers(PRBool *aIsEditor)
{
  if (mElement) {
    // We are actually a XUL <keyset>.
    if (aIsEditor)
      *aIsEditor = PR_FALSE;

    if (mHandler)
      return NS_OK;

    nsCOMPtr<nsIContent> content(do_QueryInterface(mElement));
    BuildHandlerChain(content, &mHandler);
  }
  else // We are an XBL file of handlers.
    nsXBLWindowHandler::EnsureHandlers(aIsEditor);
  
  return NS_OK;
}

static nsINativeKeyBindings*
GetEditorKeyBindings()
{
  static PRBool noBindings = PR_FALSE;
  if (!sNativeEditorBindings && !noBindings) {
    CallGetService(NS_NATIVEKEYBINDINGS_CONTRACTID_PREFIX "editor",
                   &sNativeEditorBindings);

    if (!sNativeEditorBindings) {
      noBindings = PR_TRUE;
    }
  }

  return sNativeEditorBindings;
}

static void
DoCommandCallback(const char *aCommand, void *aData)
{
  nsIControllers *controllers = NS_STATIC_CAST(nsIControllers*, aData);
  if (controllers) {
    nsCOMPtr<nsIController> controller;
    controllers->GetControllerForCommand(aCommand, getter_AddRefs(controller));
    if (controller) {
      controller->DoCommand(aCommand);
    }
  }
}

NS_IMETHODIMP
nsXBLWindowKeyHandler::WalkHandlers(nsIDOMEvent* aKeyEvent, nsIAtom* aEventType)
{
  nsCOMPtr<nsIDOMNSUIEvent> evt = do_QueryInterface(aKeyEvent);
  PRBool prevent;
  evt->GetPreventDefault(&prevent);
  if (prevent)
    return NS_OK;

  nsCOMPtr<nsIDOMNSEvent> domNSEvent = do_QueryInterface(aKeyEvent);
  PRBool trustedEvent = PR_FALSE;

  if (domNSEvent) {
    //Don't process the event if it was not dispatched from a trusted source
    domNSEvent->GetIsTrusted(&trustedEvent);
  }

  if (!trustedEvent)
    return NS_OK;

  // Make sure our event is really a key event
  nsCOMPtr<nsIDOMKeyEvent> keyEvent(do_QueryInterface(aKeyEvent));
  if (!keyEvent)
    return NS_OK;

  PRBool isEditor;
  EnsureHandlers(&isEditor);
  
  if (!mElement) {
    if (mUserHandler) {
      WalkHandlersInternal(aKeyEvent, aEventType, mUserHandler);
      evt->GetPreventDefault(&prevent);
      if (prevent)
        return NS_OK; // Handled by the user bindings. Our work here is done.
    }
  }

  WalkHandlersInternal(aKeyEvent, aEventType, mHandler);

  nsINativeKeyBindings *nativeBindings;
  if (isEditor && (nativeBindings = GetEditorKeyBindings())) {
    nsNativeKeyEvent nativeEvent;
    keyEvent->GetCharCode(&nativeEvent.charCode);
    keyEvent->GetKeyCode(&nativeEvent.keyCode);
    keyEvent->GetAltKey(&nativeEvent.altKey);
    keyEvent->GetCtrlKey(&nativeEvent.ctrlKey);
    keyEvent->GetShiftKey(&nativeEvent.shiftKey);
    keyEvent->GetMetaKey(&nativeEvent.metaKey);

    // get the DOM window we're attached to
    nsCOMPtr<nsIControllers> controllers;
    nsCOMPtr<nsPIWindowRoot> root = do_QueryInterface(mReceiver);
    if (root) {
      nsCOMPtr<nsIFocusController> fc;
      root->GetFocusController(getter_AddRefs(fc));
      if (fc) {
        fc->GetControllers(getter_AddRefs(controllers));
      }
    }

    PRBool handled;
    if (aEventType == nsXBLAtoms::keyup) {
      handled = sNativeEditorBindings->KeyUp(nativeEvent,
                                             DoCommandCallback, controllers);
    } else if (aEventType == nsXBLAtoms::keypress) {
      handled = sNativeEditorBindings->KeyPress(nativeEvent,
                                                DoCommandCallback, controllers);
    } else {
      handled = sNativeEditorBindings->KeyDown(nativeEvent,
                                               DoCommandCallback, controllers);
    }

    if (handled)
      aKeyEvent->PreventDefault();

  }
  
  return NS_OK;
}

nsresult nsXBLWindowKeyHandler::KeyUp(nsIDOMEvent* aKeyEvent)
{
  return WalkHandlers(aKeyEvent, nsXBLAtoms::keyup);
}

nsresult nsXBLWindowKeyHandler::KeyDown(nsIDOMEvent* aKeyEvent)
{
  return WalkHandlers(aKeyEvent, nsXBLAtoms::keydown);
}

nsresult nsXBLWindowKeyHandler::KeyPress(nsIDOMEvent* aKeyEvent)
{
  return WalkHandlers(aKeyEvent, nsXBLAtoms::keypress);
}


//
// EventMatched
//
// See if the given handler cares about this particular key event
//
PRBool
nsXBLWindowKeyHandler::EventMatched(nsXBLPrototypeHandler* inHandler,
                                    nsIAtom* inEventType, nsIDOMEvent* inEvent)
{
  nsCOMPtr<nsIDOMKeyEvent> keyEvent(do_QueryInterface(inEvent));
  if (keyEvent)
    return inHandler->KeyEventMatched(inEventType, keyEvent);

  return PR_FALSE;
}

/* static */ void
nsXBLWindowKeyHandler::ShutDown()
{
  NS_IF_RELEASE(sNativeEditorBindings);
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
