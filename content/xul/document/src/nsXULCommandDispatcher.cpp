/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*

  This file provides the implementation for the XUL Command Dispatcher.

 */

#include "nsIContent.h"
#include "nsIControllers.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMNSHTMLInputElement.h"
#include "nsIDOMNSHTMLTextAreaElement.h"
#include "nsIDOMUIEvent.h"
#include "nsIDOMWindow.h"
#include "nsIDOMXULElement.h"
#include "nsIDocument.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIScriptGlobalObject.h"
#include "nsPIDOMWindow.h"
#include "nsRDFCID.h"
#include "nsXULCommandDispatcher.h"
#include "prlog.h"

#ifdef PR_LOGGING
static PRLogModuleInfo* gLog;
#endif

////////////////////////////////////////////////////////////////////////

nsXULCommandDispatcher::nsXULCommandDispatcher(void)
    : mScriptObject(nsnull), mCurrentNode(nsnull), mUpdaters(nsnull)
{
	NS_INIT_REFCNT();

#ifdef PR_LOGGING
    if (! gLog)
        gLog = PR_NewLogModule("nsXULCommandDispatcher");
#endif
}

nsXULCommandDispatcher::~nsXULCommandDispatcher(void)
{
    while (mUpdaters) {
        Updater* doomed = mUpdaters;
        mUpdaters = mUpdaters->mNext;
        delete doomed;
    }
}

NS_IMPL_ADDREF(nsXULCommandDispatcher)
NS_IMPL_RELEASE(nsXULCommandDispatcher)

NS_IMETHODIMP
nsXULCommandDispatcher::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(NS_GET_IID(nsISupports)) ||
        iid.Equals(NS_GET_IID(nsIDOMXULCommandDispatcher))) {
        *result = NS_STATIC_CAST(nsIDOMXULCommandDispatcher*, this);
    }
    else if (iid.Equals(NS_GET_IID(nsIDOMFocusListener)) ||
             iid.Equals(NS_GET_IID(nsIDOMEventListener))) {
        *result = NS_STATIC_CAST(nsIDOMFocusListener*, this);
    }
    else if (iid.Equals(NS_GET_IID(nsIScriptObjectOwner))) {
        *result = NS_STATIC_CAST(nsIScriptObjectOwner*, this);
    }
    else if (iid.Equals(NS_GET_IID(nsISupportsWeakReference))) {
        *result = NS_STATIC_CAST(nsISupportsWeakReference*, this);
    }
    else {
        return NS_NOINTERFACE;
    }

    NS_ADDREF_THIS();
    return NS_OK;
}


NS_IMETHODIMP
nsXULCommandDispatcher::Create(nsIDOMXULCommandDispatcher** aResult)
{
    nsXULCommandDispatcher* dispatcher = new nsXULCommandDispatcher();
    if (! dispatcher)
        return NS_ERROR_OUT_OF_MEMORY;

    *aResult = dispatcher;
    NS_ADDREF(*aResult);
    return NS_OK;
}


////////////////////////////////////////////////////////////////
// nsIDOMXULTracker Interface

NS_IMETHODIMP
nsXULCommandDispatcher::GetFocusedNode(nsIDOMNode** aNode)
{
  *aNode = mCurrentNode;
  NS_IF_ADDREF(*aNode);
  return NS_OK;
}

NS_IMETHODIMP
nsXULCommandDispatcher::SetFocusedNode(nsIDOMNode* aNode)
{
  // XXX On a blur, will need to fire an updatecommands (focus) on the
  // parent window.
  mCurrentNode = aNode;
  if (mCurrentNode)
    UpdateCommands(nsAutoString("focus"));
  return NS_OK;
}

NS_IMETHODIMP
nsXULCommandDispatcher::AddCommandUpdater(nsIDOMElement* aElement,
                                            const nsString& aEvents,
                                            const nsString& aTargets)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    Updater* updater = mUpdaters;
    Updater** link = &mUpdaters;

    while (updater) {
        if (updater->mElement == aElement) {
            PR_LOG(gLog, PR_LOG_ALWAYS,
                   ("xulcmd[%p] replace %p(events=%s targets=%s) with (events=%s targets=%s)",
                    this, aElement,
                    (const char*) nsCAutoString(updater->mEvents),
                    (const char*) nsCAutoString(updater->mTargets),
                    (const char*) nsCAutoString(aEvents),
                    (const char*) nsCAutoString(aTargets)));

            // If the updater was already in the list, then replace
            // (?) the 'events' and 'targets' filters with the new
            // specification.
            updater->mEvents  = aEvents;
            updater->mTargets = aTargets;
            return NS_OK;
        }

        link = &(updater->mNext);
        updater = updater->mNext;
    }

    PR_LOG(gLog, PR_LOG_ALWAYS,
           ("xulcmd[%p] add     %p(events=%s targets=%s)",
            this, aElement,
            (const char*) nsCAutoString(aEvents),
            (const char*) nsCAutoString(aTargets)));

    // If we get here, this is a new updater. Append it to the list.
    updater = new Updater(aElement, aEvents, aTargets);
    if (! updater)
        return NS_ERROR_OUT_OF_MEMORY;

    *link = updater;
    return NS_OK;
}

NS_IMETHODIMP
nsXULCommandDispatcher::RemoveCommandUpdater(nsIDOMElement* aElement)
{
    NS_PRECONDITION(aElement != nsnull, "null ptr");
    if (! aElement)
        return NS_ERROR_NULL_POINTER;

    Updater* updater = mUpdaters;
    Updater** link = &mUpdaters;

    while (updater) {
        if (updater->mElement == aElement) {
            PR_LOG(gLog, PR_LOG_ALWAYS,
                   ("xulcmd[%p] remove  %p(events=%s targets=%s)",
                    this, aElement,
                    (const char*) nsCAutoString(updater->mEvents),
                    (const char*) nsCAutoString(updater->mTargets)));

            *link = updater->mNext;
            delete updater;
            return NS_OK;
        }

        link = &(updater->mNext);
        updater = updater->mNext;
    }

    // Hmm. Not found. Oh well.
    return NS_OK;
}

NS_IMETHODIMP
nsXULCommandDispatcher::UpdateCommands(const nsString& aEventName)
{
    nsresult rv;

    nsAutoString id;
    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(mCurrentNode);
    if (element) {
        rv = element->GetAttribute(nsAutoString("id"), id);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get element's id");
        if (NS_FAILED(rv)) return rv;
    }

    for (Updater* updater = mUpdaters; updater != nsnull; updater = updater->mNext) {
        // Skip any nodes that don't match our 'events' or 'targets'
        // filters.
        if (! Matches(updater->mEvents, aEventName))
            continue;

        if (! Matches(updater->mTargets, id))
            continue;

        nsCOMPtr<nsIContent> content = do_QueryInterface(updater->mElement);
        NS_ASSERTION(content != nsnull, "not an nsIContent");
        if (! content)
            return NS_ERROR_UNEXPECTED;

        nsCOMPtr<nsIDocument> document;
        rv = content->GetDocument(*getter_AddRefs(document));
        NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get document");
        if (NS_FAILED(rv)) return rv;

        NS_ASSERTION(document != nsnull, "element has no document");
        if (! document)
            continue;

        PR_LOG(gLog, PR_LOG_ALWAYS,
               ("xulcmd[%p] update %p event=%s",
                this, updater->mElement,
                (const char*) nsCAutoString(aEventName)));

        PRInt32 count = document->GetNumberOfShells();
        for (PRInt32 i = 0; i < count; i++) {
            nsCOMPtr<nsIPresShell> shell = dont_AddRef(document->GetShellAt(i));
            if (! shell)
                continue;

            // Retrieve the context in which our DOM event will fire.
            nsCOMPtr<nsIPresContext> context;
            rv = shell->GetPresContext(getter_AddRefs(context));
            if (NS_FAILED(rv)) return rv;

            // Handle the DOM event
            nsEventStatus status = nsEventStatus_eIgnore;
            nsEvent event;
            event.eventStructType = NS_EVENT;
            event.message = NS_XUL_COMMAND_UPDATE; 
            content->HandleDOMEvent(context, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXULCommandDispatcher::GetControllers(nsIControllers** aResult)
{
  //XXX: we should fix this so there's a generic interface that describes controllers, 
  //     so this code would have no special knowledge of what object might have controllers.
  if (mCurrentNode) {
    nsCOMPtr<nsIDOMXULElement> xulElement = do_QueryInterface(mCurrentNode);
    if (xulElement)
      return xulElement->GetControllers(aResult);

    nsCOMPtr<nsIDOMNSHTMLTextAreaElement> htmlTextArea = do_QueryInterface(mCurrentNode);
    if (htmlTextArea)
      return htmlTextArea->GetControllers(aResult);

    nsCOMPtr<nsIDOMNSHTMLInputElement> htmlInputElement = do_QueryInterface(mCurrentNode);
    if (htmlInputElement)
      return htmlInputElement->GetControllers(aResult);

    nsCOMPtr<nsIDOMWindow> domWindow = do_QueryInterface(mCurrentNode);
    if (domWindow)
      return domWindow->GetControllers(aResult);
  }

  *aResult = nsnull;
  return NS_OK;
}

/////
// nsIDOMFocusListener
/////

nsresult 
nsXULCommandDispatcher::Focus(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMNode> t;
  aEvent->GetTarget(getter_AddRefs(t));
  
  // XXX: Bad fix
  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(t);
  if(element) {
    SetFocusedNode(t);
  }

  return NS_OK;
}

nsresult 
nsXULCommandDispatcher::Blur(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMNode> t;
  aEvent->GetTarget(getter_AddRefs(t));
  if( t == mCurrentNode ) {
    SetFocusedNode(nsnull);
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsIScriptObjectOwner interface
NS_IMETHODIMP
nsXULCommandDispatcher::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
    nsresult res = NS_OK;
    nsIScriptGlobalObject *global = aContext->GetGlobalObject();

    if (nsnull == mScriptObject) {
        res = NS_NewScriptXULCommandDispatcher(aContext, (nsISupports *)(nsIDOMXULCommandDispatcher*)this, global, (void**)&mScriptObject);
    }
    *aScriptObject = mScriptObject;

    NS_RELEASE(global);
    return res;
}


NS_IMETHODIMP
nsXULCommandDispatcher::SetScriptObject(void *aScriptObject)
{
    mScriptObject = aScriptObject;
    return NS_OK;
}


PRBool
nsXULCommandDispatcher::Matches(const nsString& aList, const nsString& aElement)
{
    if (aList == "*")
        return PR_TRUE; // match _everything_!

    PRInt32 indx = aList.Find(aElement);
    if (indx == -1)
        return PR_FALSE; // not in the list at all

    // okay, now make sure it's not a substring snafu; e.g., 'ur'
    // found inside of 'blur'.
    if (indx > 0) {
        PRUnichar ch = aList[indx - 1];
        if (! nsString::IsSpace(ch) && ch != PRUnichar(','))
            return PR_FALSE;
    }

    if (indx + aElement.Length() < aList.Length()) {
        PRUnichar ch = aList[indx + aElement.Length()];
        if (! nsString::IsSpace(ch) && ch != PRUnichar(','))
            return PR_FALSE;
    }

    return PR_TRUE;
}


NS_IMETHODIMP
nsXULCommandDispatcher::GetParentWindowFromElement(nsIDOMElement* aElement, nsPIDOMWindow** aPWindow)
{
    nsCOMPtr<nsIDOMDocument> document;
    aElement->GetOwnerDocument(getter_AddRefs(document));
    if(!document) return NS_OK;

    nsCOMPtr<nsIDocument> objectOwner = do_QueryInterface(document);
    if(!objectOwner) return NS_OK;

    nsCOMPtr<nsIScriptGlobalObject> globalObject;
    objectOwner->GetScriptGlobalObject(getter_AddRefs(globalObject));
    if(!globalObject) return NS_OK;

    nsCOMPtr<nsPIDOMWindow> privateDOMWindow = do_QueryInterface(globalObject);
    if(!privateDOMWindow) return NS_OK;

    privateDOMWindow->GetPrivateParent(aPWindow);

    return NS_OK;
}

NS_IMETHODIMP
nsXULCommandDispatcher::GetControllerForCommand(const nsString& command, nsIController** _retval)
{
    *_retval = nsnull;

    nsCOMPtr<nsIControllers> controllers;
    GetControllers(getter_AddRefs(controllers));
    if(controllers) {
        nsCOMPtr<nsIController> controller;
        controllers->GetControllerForCommand(command.GetUnicode(), getter_AddRefs(controller));
        if(controller) {
            *_retval = controller;
            NS_ADDREF(*_retval);
            return NS_OK;
        }
    }
     
    if(!mCurrentNode) return NS_OK;

    nsCOMPtr<nsPIDOMWindow> currentWindow;

    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(mCurrentNode);
    if(element) {
        GetParentWindowFromElement(element, getter_AddRefs(currentWindow));
    } else {
        nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(mCurrentNode);
        if(!window) return NS_OK;

        window->GetPrivateParent(getter_AddRefs(currentWindow));
    }

    while(currentWindow) {
        nsCOMPtr<nsIDOMWindow> domWindow = do_QueryInterface(currentWindow);
        if(domWindow) {
            nsCOMPtr<nsIControllers> controllers2;
            domWindow->GetControllers(getter_AddRefs(controllers2));
            if(controllers2) {
                nsCOMPtr<nsIController> controller;
                controllers2->GetControllerForCommand(command.GetUnicode(), getter_AddRefs(controller));
                if(controller) {
                    *_retval = controller;
                    NS_ADDREF(*_retval);
                    return NS_OK;
                }
            }
        } 
        nsCOMPtr<nsPIDOMWindow> parentPWindow = currentWindow;
        parentPWindow->GetPrivateParent(getter_AddRefs(currentWindow));
    }
    
    return NS_OK;
}

