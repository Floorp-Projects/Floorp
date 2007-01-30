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
#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMNSEvent.h"
#include "nsXBLService.h"
#include "nsIServiceManager.h"
#include "nsGkAtoms.h"
#include "nsIXBLDocumentInfo.h"
#include "nsIDOMElement.h"
#include "nsINativeKeyBindings.h"
#include "nsIController.h"
#include "nsIControllers.h"
#include "nsIDOMWindowInternal.h"
#include "nsIFocusController.h"
#include "nsPIWindowRoot.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsContentUtils.h"
#include "nsXBLPrototypeBinding.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNSDocument.h"
#include "nsPIWindowRoot.h"
#include "nsPIDOMWindow.h"
#include "nsIFocusController.h"
#include "nsIDocShell.h"
#include "nsIPresShell.h"
#include "nsIPrivateDOMEvent.h"
#include "nsISelectionController.h"

static nsINativeKeyBindings *sNativeEditorBindings = nsnull;

class nsXBLSpecialDocInfo
{
public:
  nsCOMPtr<nsIXBLDocumentInfo> mHTMLBindings;
  nsCOMPtr<nsIXBLDocumentInfo> mUserHTMLBindings;

  static const char sHTMLBindingStr[];
  static const char sUserHTMLBindingStr[];

  PRBool mInitialized;

public:
  void LoadDocInfo();
  void GetAllHandlers(const char* aType,
                      nsXBLPrototypeHandler** handler,
                      nsXBLPrototypeHandler** userHandler);
  void GetHandlers(nsIXBLDocumentInfo* aInfo,
                   const nsACString& aRef,
                   nsXBLPrototypeHandler** aResult);

  nsXBLSpecialDocInfo() : mInitialized(PR_FALSE) {};
};

const char nsXBLSpecialDocInfo::sHTMLBindingStr[] =
  "chrome://global/content/platformHTMLBindings.xml";

void nsXBLSpecialDocInfo::LoadDocInfo()
{
  if (mInitialized)
    return;
  mInitialized = PR_TRUE;

  nsresult rv;
  nsCOMPtr<nsIXBLService> xblService = 
           do_GetService("@mozilla.org/xbl;1", &rv);
  if (NS_FAILED(rv) || !xblService)
    return;

  // Obtain the platform doc info
  nsCOMPtr<nsIURI> bindingURI;
  NS_NewURI(getter_AddRefs(bindingURI), sHTMLBindingStr);
  if (!bindingURI) {
    return;
  }
  xblService->LoadBindingDocumentInfo(nsnull, nsnull,
                                      bindingURI,
                                      PR_TRUE, 
                                      getter_AddRefs(mHTMLBindings));

  const nsAdoptingCString& userHTMLBindingStr =
    nsContentUtils::GetCharPref("dom.userHTMLBindings.uri");
  if (!userHTMLBindingStr.IsEmpty()) {
    NS_NewURI(getter_AddRefs(bindingURI), userHTMLBindingStr);
    if (!bindingURI) {
      return;
    }

    xblService->LoadBindingDocumentInfo(nsnull, nsnull,
                                        bindingURI,
                                        PR_TRUE, 
                                        getter_AddRefs(mUserHTMLBindings));
  }
}

//
// GetHandlers
//
// 
void
nsXBLSpecialDocInfo::GetHandlers(nsIXBLDocumentInfo* aInfo,
                                 const nsACString& aRef,
                                 nsXBLPrototypeHandler** aResult)
{
  nsXBLPrototypeBinding* binding;
  aInfo->GetPrototypeBinding(aRef, &binding);
  
  NS_ASSERTION(binding, "No binding found for the XBL window key handler.");
  if (!binding)
    return;

  *aResult = binding->GetPrototypeHandlers();
}

void
nsXBLSpecialDocInfo::GetAllHandlers(const char* aType,
                                    nsXBLPrototypeHandler** aHandler,
                                    nsXBLPrototypeHandler** aUserHandler)
{
  if (mUserHTMLBindings) {
    nsCAutoString type(aType);
    type.Append("User");
    GetHandlers(mUserHTMLBindings, type, aUserHandler);
  }
  if (mHTMLBindings) {
    GetHandlers(mHTMLBindings, nsDependentCString(aType), aHandler);
  }
}

// Init statics
nsXBLSpecialDocInfo* nsXBLWindowKeyHandler::sXBLSpecialDocInfo = nsnull;
PRUint32 nsXBLWindowKeyHandler::sRefCnt = 0;

nsXBLWindowKeyHandler::nsXBLWindowKeyHandler(nsIDOMElement* aElement, nsIDOMEventReceiver* aReceiver)
  : mReceiver(aReceiver),
    mHandler(nsnull),
    mUserHandler(nsnull)
{
  mWeakPtrForElement = do_GetWeakReference(aElement);
  ++sRefCnt;
}

nsXBLWindowKeyHandler::~nsXBLWindowKeyHandler()
{
  // If mWeakPtrForElement is non-null, we created a prototype handler.
  if (mWeakPtrForElement)
    delete mHandler;

  --sRefCnt;
  if (!sRefCnt) {
    delete sXBLSpecialDocInfo;
    sXBLSpecialDocInfo = nsnull;
  }
}

NS_IMPL_ISUPPORTS2(nsXBLWindowKeyHandler,
                   nsIDOMKeyListener,
                   nsIDOMEventListener)

static void
BuildHandlerChain(nsIContent* aContent, nsXBLPrototypeHandler** aResult)
{
  *aResult = nsnull;

  // Since we chain each handler onto the next handler,
  // we'll enumerate them here in reverse so that when we
  // walk the chain they'll come out in the original order
  for (PRUint32 j = aContent->GetChildCount(); j--; ) {
    nsIContent *key = aContent->GetChildAt(j);

    if (key->NodeInfo()->Equals(nsGkAtoms::key, kNameSpaceID_XUL)) {
      nsXBLPrototypeHandler* handler = new nsXBLPrototypeHandler(key);

      if (!handler)
        return;

      handler->SetNextHandler(*aResult);
      *aResult = handler;
    }
  }
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
  nsCOMPtr<nsIDOMElement> el = GetElement();
  NS_ENSURE_STATE(!mWeakPtrForElement || el);
  if (el) {
    // We are actually a XUL <keyset>.
    if (aIsEditor)
      *aIsEditor = PR_FALSE;

    if (mHandler)
      return NS_OK;

    nsCOMPtr<nsIContent> content(do_QueryInterface(el));
    BuildHandlerChain(content, &mHandler);
  } else { // We are an XBL file of handlers.
    if (!sXBLSpecialDocInfo)
      sXBLSpecialDocInfo = new nsXBLSpecialDocInfo();
    if (!sXBLSpecialDocInfo) {
      if (aIsEditor) {
        *aIsEditor = PR_FALSE;
      }
      return NS_ERROR_OUT_OF_MEMORY;
    }
    sXBLSpecialDocInfo->LoadDocInfo();

    // Now determine which handlers we should be using.
    PRBool isEditor = IsEditor();
    if (isEditor) {
      sXBLSpecialDocInfo->GetAllHandlers("editor", &mHandler, &mUserHandler);
    }
    else {
      sXBLSpecialDocInfo->GetAllHandlers("browser", &mHandler, &mUserHandler);
    }

    if (aIsEditor)
      *aIsEditor = isEditor;
  }

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

nsresult
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
  nsresult rv = EnsureHandlers(&isEditor);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIDOMElement> el = GetElement();
  if (!el) {
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
    // Some key events have no useful charCode
    nativeEvent.charCode = 0;
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
    if (aEventType == nsGkAtoms::keypress) {
      keyEvent->GetCharCode(&nativeEvent.charCode);
      handled = sNativeEditorBindings->KeyPress(nativeEvent,
                                                DoCommandCallback, controllers);
    } else if (aEventType == nsGkAtoms::keyup) {
      handled = sNativeEditorBindings->KeyUp(nativeEvent,
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
  return WalkHandlers(aKeyEvent, nsGkAtoms::keyup);
}

nsresult nsXBLWindowKeyHandler::KeyDown(nsIDOMEvent* aKeyEvent)
{
  return WalkHandlers(aKeyEvent, nsGkAtoms::keydown);
}

nsresult nsXBLWindowKeyHandler::KeyPress(nsIDOMEvent* aKeyEvent)
{
  return WalkHandlers(aKeyEvent, nsGkAtoms::keypress);
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

//
// IsEditor
//
// Determine if the document we're working with is Editor or Browser
//
PRBool
nsXBLWindowKeyHandler::IsEditor()
{
  nsCOMPtr<nsPIWindowRoot> windowRoot(do_QueryInterface(mReceiver));
  NS_ENSURE_TRUE(windowRoot, PR_FALSE);
  nsCOMPtr<nsIFocusController> focusController;
  windowRoot->GetFocusController(getter_AddRefs(focusController));
  if (!focusController) {
    NS_WARNING("********* Something went wrong! No focus controller on the root!!!\n");
    return PR_FALSE;
  }

  nsCOMPtr<nsIDOMWindowInternal> focusedWindow;
  focusController->GetFocusedWindow(getter_AddRefs(focusedWindow));
  if (!focusedWindow)
    return PR_FALSE;
  
  nsCOMPtr<nsPIDOMWindow> piwin(do_QueryInterface(focusedWindow));
  nsIDocShell *docShell = piwin->GetDocShell();
  nsCOMPtr<nsIPresShell> presShell;
  if (docShell)
    docShell->GetPresShell(getter_AddRefs(presShell));

  if (presShell) {
    PRInt16 isEditor;
    presShell->GetSelectionFlags(&isEditor);
    return isEditor == nsISelectionDisplay::DISPLAY_ALL;
  }

  return PR_FALSE;
}

//
// WalkHandlersInternal
//
// Given a particular DOM event and a pointer to the first handler in the list,
// scan through the list to find something to handle the event and then make it
// so.
//
nsresult
nsXBLWindowKeyHandler::WalkHandlersInternal(nsIDOMEvent* aEvent,
                                            nsIAtom* aEventType, 
                                            nsXBLPrototypeHandler* aHandler)
{
  nsresult rv;
  nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aEvent));
  
  // Try all of the handlers until we find one that matches the event.
  for (nsXBLPrototypeHandler *currHandler = aHandler; currHandler;
       currHandler = currHandler->GetNextHandler()) {
    PRBool stopped;
    privateEvent->IsDispatchStopped(&stopped);
    if (stopped) {
      // The event is finished, don't execute any more handlers
      return NS_OK;
    }

    if (!EventMatched(currHandler, aEventType, aEvent))
      continue;  // try the next one

    // Before executing this handler, check that it's not disabled,
    // and that it has something to do (oncommand of the <key> or its
    // <command> is non-empty).
    nsCOMPtr<nsIContent> elt = currHandler->GetHandlerElement();
    nsCOMPtr<nsIDOMElement> commandElt;

    // See if we're in a XUL doc.
    nsCOMPtr<nsIDOMElement> el = GetElement();
    if (el && elt) {
      // We are.  Obtain our command attribute.
      nsAutoString command;
      elt->GetAttr(kNameSpaceID_None, nsGkAtoms::command, command);
      if (!command.IsEmpty()) {
        // Locate the command element in question.  Note that we
        // know "elt" is in a doc if we're dealing with it here.
        NS_ASSERTION(elt->IsInDoc(), "elt must be in document");
        nsCOMPtr<nsIDOMDocument> domDoc(
           do_QueryInterface(elt->GetCurrentDoc()));
        if (domDoc)
          domDoc->GetElementById(command, getter_AddRefs(commandElt));

        if (!commandElt) {
          NS_ERROR("A XUL <key> is observing a command that doesn't exist. Unable to execute key binding!\n");
          continue;
        }
      }
    }

    if (!commandElt) {
      commandElt = do_QueryInterface(elt);
    }

    if (commandElt) {
      nsAutoString value;
      commandElt->GetAttribute(NS_LITERAL_STRING("disabled"), value);
      if (value.EqualsLiteral("true")) {
        continue;  // this handler is disabled, try the next one
      }

      // Check that there is an oncommand handler
      commandElt->GetAttribute(NS_LITERAL_STRING("oncommand"), value);
      if (value.IsEmpty()) {
        continue;  // nothing to do
      }
    }

    nsCOMPtr<nsIDOMEventReceiver> rec;
    nsCOMPtr<nsIDOMElement> element = GetElement();
    if (element) {
      rec = do_QueryInterface(commandElt);
    } else {
      rec = mReceiver;
    }

    rv = currHandler->ExecuteHandler(rec, aEvent);
    if (NS_SUCCEEDED(rv)) {
      return NS_OK;
    }
  }

  return NS_OK;
}

already_AddRefed<nsIDOMElement>
nsXBLWindowKeyHandler::GetElement()
{
  nsCOMPtr<nsIDOMElement> element = do_QueryReferent(mWeakPtrForElement);
  nsIDOMElement* el = nsnull;
  element.swap(el);
  return el;
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
