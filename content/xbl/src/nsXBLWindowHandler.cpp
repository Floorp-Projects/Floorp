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
 * Contributor(s):
 *  - David W. Hyatt (hyatt@netscape.com)
 *  - Mike Pinkerton (pinkerton@netscape.com)
 */


#include "nsXBLWindowHandler.h"

#include "nsCOMPtr.h"
#include "nsPIWindowRoot.h"
#include "nsIDOMWindowInternal.h"
#include "nsIFocusController.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocShell.h"
#include "nsIPresShell.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIXBLPrototypeHandler.h"
#include "nsIXBLPrototypeBinding.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMEvent.h"
#include "nsIContent.h"
#include "nsHTMLAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsIXBLDocumentInfo.h"
#include "nsIDocument.h"
#include "nsIXBLService.h"
#include "nsIServiceManager.h"


struct nsXBLSpecialDocInfo {
  nsCOMPtr<nsIXBLDocumentInfo> mHTMLBindings;
  nsCOMPtr<nsIXBLDocumentInfo> mPlatformHTMLBindings;
  PRBool mFilesPresent;

  nsXBLSpecialDocInfo() :mFilesPresent(PR_TRUE) {};
};


// Init statics
nsXBLSpecialDocInfo* nsXBLWindowHandler::sXBLSpecialDocInfo = nsnull;
PRUint32 nsXBLWindowHandler::sRefCnt = 0;


//
// nsXBLWindowHandler ctor
//
// Increment the refcount
//
nsXBLWindowHandler :: nsXBLWindowHandler (nsIDOMElement* aElement, nsIDOMEventReceiver* aReceiver)
  : mElement(aElement), mReceiver(aReceiver)
{
  ++sRefCnt;
}


//
// nsXBLWindowHandler dtor
//
// Decrement the refcount. If we get to zero, get rid of the static XBL doc
// info.
//
nsXBLWindowHandler :: ~nsXBLWindowHandler ( )
{
  --sRefCnt;
  if ( !sRefCnt ) {
    delete sXBLSpecialDocInfo;
    sXBLSpecialDocInfo = nsnull;
  }
}


//
// IsEditor
//
// Determine if the document we're working with is Editor or Browser
//
PRBool
nsXBLWindowHandler :: IsEditor()
{
  nsCOMPtr<nsPIWindowRoot> windowRoot(do_QueryInterface(mReceiver));
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
} // IsEditor


//
// WalkHandlersInternal
//
// Given a particular DOM event and a pointer to the first handler in the list,
// scan through the list to find something to handle the event and then make it
// so.
//
nsresult
nsXBLWindowHandler::WalkHandlersInternal(nsIDOMEvent* aEvent, nsIAtom* aEventType, 
                                            nsIXBLPrototypeHandler* aHandler)
{
  nsresult rv;
  nsCOMPtr<nsIXBLPrototypeHandler> currHandler = aHandler;
  while (currHandler) {

    PRBool stopped;
    nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aEvent));
    privateEvent->IsDispatchStopped(&stopped);
    if (stopped)
      return NS_OK;
 
    // if the handler says it wants the event, execute it
    if ( EventMatched(currHandler, aEventType, aEvent) ) {
      // ...but don't exectute if it is disabled.
      nsAutoString disabled;
      
      nsCOMPtr<nsIContent> elt;
      currHandler->GetHandlerElement(getter_AddRefs(elt));

      elt->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::disabled, disabled);
      if (!disabled.Equals(NS_LITERAL_STRING("true"))) {
        nsCOMPtr<nsIDOMEventReceiver> rec = mReceiver;
        if (mElement)
          rec = do_QueryInterface(elt);
        rv = currHandler->ExecuteHandler(rec, aEvent);
        if (NS_SUCCEEDED(rv))
          return NS_OK;
      }
    }

    // the current handler didn't want it, try the next one.
    nsCOMPtr<nsIXBLPrototypeHandler> nextHandler;
    currHandler->GetNextHandler(getter_AddRefs(nextHandler));
    currHandler = nextHandler;
  }

  return NS_OK;
} // WalkHandlersInternal


//
// GetHandlers
//
// 
void
nsXBLWindowHandler::GetHandlers(nsIXBLDocumentInfo* aInfo, const nsAReadableCString& aDocURI, 
                                    const nsAReadableCString& aRef, nsIXBLPrototypeHandler** aResult)
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
      if (id.EqualsWithConversion(nsPromiseFlatCString(aRef))) {
        NS_NewXBLPrototypeBinding(aRef, child, aInfo, getter_AddRefs(binding));
        aInfo->SetPrototypeBinding(aRef, binding);
        break;
      }
    }
  }

  nsCOMPtr<nsIXBLPrototypeHandler> dummy;
  binding->GetPrototypeHandlers(aResult, getter_AddRefs(dummy)); // Addref happens here.
} // GetHandlers


//
// EnsureHandlers
//
// Lazily load the XP and platform-specific bindings
//
nsresult
nsXBLWindowHandler::EnsureHandlers()
{
  if (!sXBLSpecialDocInfo)
    sXBLSpecialDocInfo = new nsXBLSpecialDocInfo();    
  if (!sXBLSpecialDocInfo)
    return NS_ERROR_OUT_OF_MEMORY;

  if (!sXBLSpecialDocInfo->mFilesPresent)
    return NS_OK;

  if (!sXBLSpecialDocInfo->mHTMLBindings || !sXBLSpecialDocInfo->mPlatformHTMLBindings) {
    nsresult rv;
    NS_WITH_SERVICE(nsIXBLService, xblService, "@mozilla.org/xbl;1", &rv);
    if (xblService) {
      // Obtain the two doc infos we need.
      xblService->LoadBindingDocumentInfo(nsnull, nsnull, 
                                          nsCAutoString("resource:///res/builtin/htmlBindings.xml"),
                                          nsCAutoString(""), PR_TRUE, 
                                          getter_AddRefs(sXBLSpecialDocInfo->mHTMLBindings));
      xblService->LoadBindingDocumentInfo(nsnull, nsnull, 
                                          nsCAutoString("resource:///res/builtin/platformHTMLBindings.xml"),
                                          nsCAutoString(""), PR_TRUE, 
                                          getter_AddRefs(sXBLSpecialDocInfo->mPlatformHTMLBindings));

      if (!sXBLSpecialDocInfo->mHTMLBindings || !sXBLSpecialDocInfo->mPlatformHTMLBindings) {
        sXBLSpecialDocInfo->mFilesPresent = PR_FALSE;
        return NS_OK;
      }
    }
  }

  // Now determine which handlers we should be using.
  if (IsEditor()) {
    GetHandlers(sXBLSpecialDocInfo->mPlatformHTMLBindings, 
                nsCAutoString("resource:///res/builtin/platformHTMLBindings.xml"),
                nsCAutoString("editor"), 
                getter_AddRefs(mPlatformHandler));
    GetHandlers(sXBLSpecialDocInfo->mHTMLBindings, 
                nsCAutoString("resource:///res/builtin/htmlBindings.xml"),
                nsCAutoString("editorBase"), 
                getter_AddRefs(mHandler));
  }
  else {
    GetHandlers(sXBLSpecialDocInfo->mPlatformHTMLBindings, 
                nsCAutoString("resource:///res/builtin/platformHTMLBindings.xml"),
                nsCAutoString("browser"), 
                getter_AddRefs(mPlatformHandler));
    GetHandlers(sXBLSpecialDocInfo->mHTMLBindings, 
                nsCAutoString("resource:///res/builtin/htmlBindings.xml"),
                nsCAutoString("browserBase"), 
                getter_AddRefs(mHandler));
  }

  return NS_OK;
  
} // EnsureHandlers

