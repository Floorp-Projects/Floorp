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
#include "nsXBLWindowKeyHandler.h"
#include "nsIXBLPrototypeBinding.h"
#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsIDOMNSUIEvent.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMEventReceiver.h"
#include "nsIPrivateDOMEvent.h"
#include "nsXBLService.h"
#include "nsIServiceManager.h"
#include "nsHTMLAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsIXBLDocumentInfo.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindowInternal.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsIDocShell.h"
#include "nsIPresShell.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMElement.h"

PRUint32 nsXBLWindowKeyHandler::gRefCnt = 0;
nsIAtom* nsXBLWindowKeyHandler::kKeyDownAtom = nsnull;
nsIAtom* nsXBLWindowKeyHandler::kKeyUpAtom = nsnull;
nsIAtom* nsXBLWindowKeyHandler::kKeyPressAtom = nsnull;

struct nsXBLSpecialDocInfo {
  nsCOMPtr<nsIXBLDocumentInfo> mHTMLBindings;
  nsCOMPtr<nsIXBLDocumentInfo> mPlatformHTMLBindings;
  PRBool mFilesPresent;

  nsXBLSpecialDocInfo() :mFilesPresent(PR_TRUE) {};
};

nsXBLWindowKeyHandler::nsXBLWindowKeyHandler(nsIDOMElement* aElement, nsIDOMEventReceiver* aReceiver)
{
  NS_INIT_ISUPPORTS();
  mElement = aElement;
  mReceiver = aReceiver;
  mXBLSpecialDocInfo = nsnull;
  gRefCnt++;
  if (gRefCnt == 1) {
    kKeyUpAtom = NS_NewAtom("keyup");
    kKeyDownAtom = NS_NewAtom("keydown");
    kKeyPressAtom = NS_NewAtom("keypress");
  }
}

nsXBLWindowKeyHandler::~nsXBLWindowKeyHandler()
{
  delete mXBLSpecialDocInfo;

  gRefCnt--;
  if (gRefCnt == 0) {
    NS_RELEASE(kKeyUpAtom);
    NS_RELEASE(kKeyDownAtom);
    NS_RELEASE(kKeyPressAtom);
  }
}

NS_IMPL_ISUPPORTS1(nsXBLWindowKeyHandler, nsIDOMKeyListener)

PRBool
nsXBLWindowKeyHandler::IsEditor()
{
  nsCOMPtr<nsPIDOMWindow> privateWindow(do_QueryInterface(mReceiver));
  nsCOMPtr<nsIDOMXULCommandDispatcher> commandDispatcher;
  privateWindow->GetRootCommandDispatcher(getter_AddRefs(commandDispatcher));
  if (!commandDispatcher) {
    NS_WARNING("********* Problem for embedding. They have no command dispatcher!!!\n");
    return PR_FALSE;
  }

  nsCOMPtr<nsIDOMWindowInternal> focusedWindow;
  commandDispatcher->GetFocusedWindow(getter_AddRefs(focusedWindow));
  if (!focusedWindow)
    return PR_FALSE;
  
  nsCOMPtr<nsIScriptGlobalObject> obj(do_QueryInterface(focusedWindow));
  nsCOMPtr<nsIDocShell> docShell;
  obj->GetDocShell(getter_AddRefs(docShell));
  nsCOMPtr<nsIPresShell> presShell;
  if (docShell)
    docShell->GetPresShell(getter_AddRefs(presShell));

  if (presShell) {
    PRBool isEditor;
    presShell->GetDisplayNonTextSelection(&isEditor);
    return isEditor;
  }

  return PR_FALSE;
}

static void GetHandlers(nsIXBLDocumentInfo* aInfo, const nsCString& aDocURI, 
                        const nsCString& aRef, nsIXBLPrototypeHandler** aResult)
{
  nsCOMPtr<nsIXBLPrototypeBinding> binding;
  aInfo->GetPrototypeBinding(aRef, getter_AddRefs(binding));
  if (!binding) {
    nsCOMPtr<nsIDocument> doc;
    aInfo->GetDocument(getter_AddRefs(doc));
    nsCOMPtr<nsIContent> root = getter_AddRefs(doc->GetRootContent());
    PRInt32 childCount;
    root->ChildCount(childCount);
    for (PRInt32 i = 0; i < childCount; i++) {
      nsCOMPtr<nsIContent> child;
      root->ChildAt(i, *getter_AddRefs(child));
      nsAutoString id;
      child->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::id, id);
      if (id.EqualsWithConversion(aRef)) {
        NS_NewXBLPrototypeBinding(aRef, child, aInfo, getter_AddRefs(binding));
        aInfo->SetPrototypeBinding(aRef, binding);
        break;
      }
    }
  }

  binding->GetPrototypeHandler(aResult); // Addref happens here.
}

NS_IMETHODIMP
nsXBLWindowKeyHandler::EnsureHandlers()
{
  if (mElement) {
    if (mHandler)
      return NS_OK;
    // Call into the XBL service.
    nsCOMPtr<nsIContent> content(do_QueryInterface(mElement));
    nsXBLService::BuildHandlerChain(content, getter_AddRefs(mHandler));
  }
  else {
    if (!mXBLSpecialDocInfo)
      mXBLSpecialDocInfo = new nsXBLSpecialDocInfo();
      
    if (!mXBLSpecialDocInfo)
      return NS_ERROR_OUT_OF_MEMORY;

    if (!mXBLSpecialDocInfo->mFilesPresent)
      return NS_OK;

    if (!mXBLSpecialDocInfo->mHTMLBindings || !mXBLSpecialDocInfo->mPlatformHTMLBindings) {
      nsresult rv;
      NS_WITH_SERVICE(nsIXBLService, xblService, "@mozilla.org/xbl;1", &rv);
      if (xblService) {
        // Obtain the two doc infos we need.
        xblService->LoadBindingDocumentInfo(nsnull, nsnull, 
                                            nsCAutoString("chrome://global/content/htmlBindings.xml"),
                                            nsCAutoString(""), PR_TRUE, 
                                            getter_AddRefs(mXBLSpecialDocInfo->mHTMLBindings));
        xblService->LoadBindingDocumentInfo(nsnull, nsnull, 
                                            nsCAutoString("chrome://global/content/platformHTMLBindings.xml"),
                                            nsCAutoString(""), PR_TRUE, 
                                            getter_AddRefs(mXBLSpecialDocInfo->mPlatformHTMLBindings));

        if (!mXBLSpecialDocInfo->mHTMLBindings || !mXBLSpecialDocInfo->mPlatformHTMLBindings) {
          mXBLSpecialDocInfo->mFilesPresent = PR_FALSE;
          return NS_OK;
        }
      }
    }

    // Now determine which handlers we should be using.
    if (IsEditor()) {
      GetHandlers(mXBLSpecialDocInfo->mPlatformHTMLBindings, 
                  nsCAutoString("chrome://global/content/platformHTMLBindings.xml"),
                  nsCAutoString("editor"), 
                  getter_AddRefs(mPlatformHandler));
      GetHandlers(mXBLSpecialDocInfo->mHTMLBindings, 
                  nsCAutoString("chrome://global/content/htmlBindings.xml"),
                  nsCAutoString("editorBase"), 
                  getter_AddRefs(mHandler));
    }
    else {
      GetHandlers(mXBLSpecialDocInfo->mPlatformHTMLBindings, 
                  nsCAutoString("chrome://global/content/platformHTMLBindings.xml"),
                  nsCAutoString("browser"), 
                  getter_AddRefs(mPlatformHandler));
      GetHandlers(mXBLSpecialDocInfo->mHTMLBindings, 
                  nsCAutoString("chrome://global/content/htmlBindings.xml"),
                  nsCAutoString("browserBase"), 
                  getter_AddRefs(mHandler));
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXBLWindowKeyHandler::WalkHandlersInternal(nsIDOMKeyEvent* aKeyEvent, nsIAtom* aEventType, 
                                            nsIXBLPrototypeHandler* aHandler)
{
  nsresult rv;
  nsCOMPtr<nsIXBLPrototypeHandler> currHandler = aHandler;
  while (currHandler) {

    PRBool stopped;
    nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aKeyEvent));
    privateEvent->IsDispatchStopped(&stopped);
    if (stopped)
      return NS_OK;
 
    PRBool matched = PR_FALSE;
    currHandler->KeyEventMatched(aEventType, aKeyEvent, &matched);
  
    if (matched) {
      // Make sure it's not disabled.
      nsAutoString disabled;
      
      nsCOMPtr<nsIContent> elt;
      currHandler->GetHandlerElement(getter_AddRefs(elt));

      /*nsAutoString id;
      elt->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::id, id);
      nsCAutoString idc; idc.AssignWithConversion(id);
   
      if (!idc.IsEmpty())
        printf("Key matched with id of: %s\n", (const char*)idc);
*/

      elt->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::disabled, disabled);
      if (!disabled.Equals(NS_LITERAL_STRING("true"))) {
        nsCOMPtr<nsIDOMEventReceiver> rec = mReceiver;
        if (mElement)
          rec = do_QueryInterface(elt);
        rv = currHandler->ExecuteHandler(rec, aKeyEvent);
        if (NS_SUCCEEDED(rv)) {
          aKeyEvent->PreventDefault();
          return NS_OK;
        }
      }
    }

    nsCOMPtr<nsIXBLPrototypeHandler> nextHandler;
    currHandler->GetNextHandler(getter_AddRefs(nextHandler));
    currHandler = nextHandler;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXBLWindowKeyHandler::WalkHandlers(nsIDOMEvent* aKeyEvent, nsIAtom* aEventType)
{
  nsCOMPtr<nsIDOMNSUIEvent> evt = do_QueryInterface(aKeyEvent);
  PRBool prevent;
  evt->GetPreventDefault(&prevent);
  if (prevent)
    return NS_OK;

  nsCOMPtr<nsIDOMKeyEvent> keyEvent(do_QueryInterface(aKeyEvent));
  if (!keyEvent)
    return NS_OK;

  EnsureHandlers();
  
  if (!mElement) {
    WalkHandlersInternal(keyEvent, aEventType, mPlatformHandler);
    evt->GetPreventDefault(&prevent);
    if (prevent)
      return NS_OK; // Handled by the platform. Our work here is done.
  }

  WalkHandlersInternal(keyEvent, aEventType, mHandler);
  
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
