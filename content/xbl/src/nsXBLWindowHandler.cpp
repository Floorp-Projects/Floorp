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
 *   - David W. Hyatt (hyatt@netscape.com)
 *   - Mike Pinkerton (pinkerton@netscape.com)
 *   - Akkana Peck (akkana@netscape.com)
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
#include "nsXBLPrototypeHandler.h"
#include "nsXBLPrototypeBinding.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMEvent.h"
#include "nsIContent.h"
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsIXBLDocumentInfo.h"
#include "nsIDocument.h"
#include "nsIXBLService.h"
#include "nsIServiceManager.h"
#include "nsIDOMDocument.h"
#include "nsISelectionController.h"
#include "nsXULAtoms.h"
#include "nsIURI.h"
#include "nsNetUtil.h"

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

const char nsXBLSpecialDocInfo::sHTMLBindingStr[] = "resource://gre/res/builtin/platformHTMLBindings.xml";
// Allow for a userHTMLBindings.xml.
// XXXbsmedberg Should be in the profile chrome directory, when we have a resource mapping for that
const char nsXBLSpecialDocInfo::sUserHTMLBindingStr[] = "resource://gre/res/builtin/userHTMLBindings.xml";

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

  rv = bindingURI->SetSpec(NS_LITERAL_CSTRING(sUserHTMLBindingStr));
  if (NS_FAILED(rv)) {
    NS_ERROR("Shouldn't fail to set spec here");
    return;
  }
  
  xblService->LoadBindingDocumentInfo(nsnull, nsnull,
                                      bindingURI,
                                      PR_TRUE, 
                                      getter_AddRefs(mUserHTMLBindings));
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
} // GetHandlers

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
nsXBLSpecialDocInfo* nsXBLWindowHandler::sXBLSpecialDocInfo = nsnull;
PRUint32 nsXBLWindowHandler::sRefCnt = 0;


//
// nsXBLWindowHandler ctor
//
// Increment the refcount
//
nsXBLWindowHandler::nsXBLWindowHandler(nsIDOMElement* aElement,
                                       nsIDOMEventReceiver* aReceiver)
  : mElement(aElement),
    mReceiver(aReceiver),
    mHandler(nsnull),
    mUserHandler(nsnull)
{
  ++sRefCnt;
}


//
// nsXBLWindowHandler dtor
//
// Decrement the refcount. If we get to zero, get rid of the static XBL doc
// info.
//
nsXBLWindowHandler::~nsXBLWindowHandler()
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
  nsIDocShell *docShell = obj->GetDocShell();
  nsCOMPtr<nsIPresShell> presShell;
  if (docShell)
    docShell->GetPresShell(getter_AddRefs(presShell));

  if (presShell) {
    PRInt16 isEditor;
    presShell->GetSelectionFlags(&isEditor);
    return isEditor == nsISelectionDisplay::DISPLAY_ALL;
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
nsXBLWindowHandler::WalkHandlersInternal(nsIDOMEvent* aEvent,
                                         nsIAtom* aEventType, 
                                         nsXBLPrototypeHandler* aHandler)
{
  nsresult rv;
  nsXBLPrototypeHandler* currHandler = aHandler;
  nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aEvent));
  
  while (currHandler) {

    PRBool stopped;
    privateEvent->IsDispatchStopped(&stopped);
    if (stopped)
      return NS_OK;
 
    // if the handler says it wants the event, execute it
    if ( EventMatched(currHandler, aEventType, aEvent) ) {
      // ...but don't execute if it is disabled.
      nsAutoString disabled;
      
      nsCOMPtr<nsIContent> elt = currHandler->GetHandlerElement();
      nsCOMPtr<nsIDOMElement> commandElt;

      // See if we're in a XUL doc.
      if (mElement) {
        // We are.  Obtain our command attribute.
        nsAutoString command;
        elt->GetAttr(kNameSpaceID_None, nsXULAtoms::command, command);
        if (!command.IsEmpty()) {
          // Locate the command element in question.
          nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(elt->GetDocument()));
          if (domDoc)
            domDoc->GetElementById(command, getter_AddRefs(commandElt));

          if (!commandElt) {
            NS_ERROR("A XUL <key> is observing a command that doesn't exist. Unable to execute key binding!\n");
            return NS_OK;
          }
        }
      }

      if (!commandElt) {
        commandElt = do_QueryInterface(elt);
      }

      if (commandElt)
        commandElt->GetAttribute(NS_LITERAL_STRING("disabled"), disabled);
      if (!disabled.EqualsLiteral("true")) {
        nsCOMPtr<nsIDOMEventReceiver> rec = mReceiver;
        if (mElement)
          rec = do_QueryInterface(commandElt);
        rv = currHandler->ExecuteHandler(rec, aEvent);
        if (NS_SUCCEEDED(rv))
          return NS_OK;
      }
    }

    // the current handler didn't want it, try the next one.
    currHandler = currHandler->GetNextHandler();
  }

  return NS_OK;
} // WalkHandlersInternal



//
// EnsureHandlers
//
// Lazily load the platform and user bindings
//
nsresult
nsXBLWindowHandler::EnsureHandlers(PRBool *aIsEditor)
{
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

  return NS_OK;
  
} // EnsureHandlers

