/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *  - David W. Hyatt (hyatt@netscape.com)
 *  - Mike Pinkerton (pinkerton@netscape.com)
 *  - Akkana Peck (akkana@netscape.com)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
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
#include "nsIDOMDocument.h"
#ifdef INCLUDE_XUL
#include "nsXULAtoms.h"
#endif

class nsXBLSpecialDocInfo
{
public:
  nsCOMPtr<nsIXBLDocumentInfo> mHTMLBindings;
  nsCOMPtr<nsIXBLDocumentInfo> mPlatformHTMLBindings;
  nsCOMPtr<nsIXBLDocumentInfo> mUserHTMLBindings;

  nsCString mHTMLBindingStr;
  nsCString mPlatformHTMLBindingStr;
  nsCString mUserHTMLBindingStr;

  static const char sHTMLBindingStr[];
  static const char sPlatformHTMLBindingStr[];
  static const char sUserHTMLBindingStr[];

  PRBool mInitialized;

public:
  void LoadDocInfo();
  void GetAllHandlers(const char* aType,
                      nsIXBLPrototypeHandler** handler,
                      nsIXBLPrototypeHandler** platformHandler,
                      nsIXBLPrototypeHandler** userHandler);
  void GetHandlers(nsIXBLDocumentInfo* aInfo,
                   const nsAReadableCString& aRef,
                   nsIXBLPrototypeHandler** aResult);

  nsXBLSpecialDocInfo() : mInitialized(PR_FALSE) {};
};

const char nsXBLSpecialDocInfo::sHTMLBindingStr[] = "resource:///res/builtin/htmlBindings.xml";
const char nsXBLSpecialDocInfo::sPlatformHTMLBindingStr[] = "resource:///res/builtin/platformHTMLBindings.xml";
// Allow for a userHTMLBindings.xml.
// XXX Should be in the user profile directory, when we have a urlspec for that
const char nsXBLSpecialDocInfo::sUserHTMLBindingStr[] = "resource:///res/builtin/userHTMLBindings.xml";

void nsXBLSpecialDocInfo::LoadDocInfo()
{
  if (mInitialized)
    return;
  mInitialized = PR_TRUE;

  mHTMLBindingStr = sHTMLBindingStr;
  mPlatformHTMLBindingStr = sPlatformHTMLBindingStr;
  mUserHTMLBindingStr = sUserHTMLBindingStr;

  if (mHTMLBindings && mPlatformHTMLBindings && mUserHTMLBindings)
    return;

  nsresult rv;
  nsCOMPtr<nsIXBLService> xblService = 
           do_GetService("@mozilla.org/xbl;1", &rv);
  if (NS_FAILED(rv) || !xblService)
    return;

  // Obtain the XP and platform doc infos
  xblService->LoadBindingDocumentInfo(nsnull, nsnull, 
                                      mHTMLBindingStr,
                                      nsCAutoString(""), PR_TRUE, 
                                      getter_AddRefs(mHTMLBindings));
  xblService->LoadBindingDocumentInfo(nsnull, nsnull,
                                      mPlatformHTMLBindingStr,
                                      nsCAutoString(""), PR_TRUE, 
                                      getter_AddRefs(mPlatformHTMLBindings));
  xblService->LoadBindingDocumentInfo(nsnull, nsnull,
                                      mUserHTMLBindingStr,
                                      nsCAutoString(""), PR_TRUE, 
                                      getter_AddRefs(mUserHTMLBindings));
}

//
// GetHandlers
//
// 
void
nsXBLSpecialDocInfo::GetHandlers(nsIXBLDocumentInfo* aInfo,
                                 const nsAReadableCString& aRef,
                                 nsIXBLPrototypeHandler** aResult)
{
  nsCOMPtr<nsIXBLPrototypeBinding> binding;
  aInfo->GetPrototypeBinding(aRef, getter_AddRefs(binding));
  
  NS_ASSERTION(binding, "No binding found for the XBL window key handler.");
  if (!binding)
    return;

  binding->GetPrototypeHandlers(aResult); // Addref happens here.
} // GetHandlers

void
nsXBLSpecialDocInfo::GetAllHandlers(const char* aType,
                                    nsIXBLPrototypeHandler** aHandler,
                                    nsIXBLPrototypeHandler** aPlatformHandler,
                                    nsIXBLPrototypeHandler** aUserHandler)
{
  if (mUserHTMLBindings) {
    nsCAutoString type(aType);
    type.Append("User");
    GetHandlers(mUserHTMLBindings, type, aUserHandler);
  }
  if (mPlatformHTMLBindings) {
    nsCAutoString type(aType);
    GetHandlers(mPlatformHTMLBindings, type, aPlatformHandler);
  }
  if (mHTMLBindings) {
    nsCAutoString type(aType);
    type.Append("Base");
    GetHandlers(mHTMLBindings, type, aHandler);
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
      // ...but don't execute if it is disabled.
      nsAutoString disabled;
      
      nsCOMPtr<nsIContent> elt;
      currHandler->GetHandlerElement(getter_AddRefs(elt));

      nsCOMPtr<nsIDOMElement> commandElt(do_QueryInterface(elt));

      // See if we're in a XUL doc.
      if (mElement) {
        // We are.  Obtain our command attribute.
        nsAutoString command;
        elt->GetAttr(kNameSpaceID_None, nsXULAtoms::command, command);
        if (!command.IsEmpty()) {
          // Locate the command element in question.
          nsCOMPtr<nsIDocument> doc;
          elt->GetDocument(*getter_AddRefs(doc));
          nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(doc));
          if (domDoc)
            domDoc->GetElementById(command, getter_AddRefs(commandElt));

          if (!commandElt) {
            NS_ASSERTION(PR_FALSE, "A XUL <key> is observing a command that doesn't exist. Unable to execute key binding!\n");
            return NS_OK;
          }
        }
      }

      if (commandElt)
        commandElt->GetAttribute(NS_LITERAL_STRING("disabled"), disabled);
      if (!disabled.Equals(NS_LITERAL_STRING("true"))) {
        nsCOMPtr<nsIDOMEventReceiver> rec = mReceiver;
        if (mElement)
          rec = do_QueryInterface(commandElt);
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
  sXBLSpecialDocInfo->LoadDocInfo();

  // Now determine which handlers we should be using.
  if (IsEditor()) {
    sXBLSpecialDocInfo->GetAllHandlers("editor",
                                       getter_AddRefs(mHandler),
                                       getter_AddRefs(mPlatformHandler),
                                       getter_AddRefs(mUserHandler));
  }
  else {
    sXBLSpecialDocInfo->GetAllHandlers("browser",
                                       getter_AddRefs(mHandler),
                                       getter_AddRefs(mPlatformHandler),
                                       getter_AddRefs(mUserHandler));
  }

  return NS_OK;
  
} // EnsureHandlers

