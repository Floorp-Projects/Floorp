/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   David W. Hyatt <hyatt@netscape.com> (Original Author)
 */

#include "nsCOMPtr.h"
#include "nsWindowRoot.h"
#include "nsIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIEventListenerManager.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsLayoutCID.h"
#include "nsContentCID.h"
#include "nsIEventStateManager.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMWindowInternal.h"
#include "nsFocusController.h"
#include "nsString.h"

static NS_DEFINE_CID(kEventListenerManagerCID,    NS_EVENTLISTENERMANAGER_CID);

nsWindowRoot::nsWindowRoot(nsIDOMWindow* aWindow)
{
  NS_INIT_ISUPPORTS();
  mWindow = aWindow;

  // Create and init our focus controller.
  nsFocusController::Create(getter_AddRefs(mFocusController));

  nsCOMPtr<nsIDOMFocusListener> focusListener(do_QueryInterface(mFocusController));
  AddEventListener(NS_LITERAL_STRING("focus"), focusListener, PR_TRUE);
  AddEventListener(NS_LITERAL_STRING("blur"), focusListener, PR_TRUE);
}

nsWindowRoot::~nsWindowRoot()
{
}

NS_IMPL_ISUPPORTS4(nsWindowRoot, nsIDOMEventReceiver, nsIChromeEventHandler, nsPIWindowRoot, nsIDOMEventTarget)

NS_IMETHODIMP
nsWindowRoot::AddEventListener(const nsAReadableString& aType, nsIDOMEventListener* aListener, PRBool aUseCapture)
{
  nsCOMPtr<nsIEventListenerManager> manager;
  GetListenerManager(getter_AddRefs(manager));
  if (manager) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;
    manager->AddEventListenerByType(aListener, aType, flags);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowRoot::RemoveEventListener(const nsAReadableString& aType, nsIDOMEventListener* aListener, PRBool aUseCapture)
{
  nsCOMPtr<nsIEventListenerManager> manager;
  GetListenerManager(getter_AddRefs(manager));
  if (manager) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;
    manager->RemoveEventListenerByType(aListener, aType, flags);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowRoot::DispatchEvent(nsIDOMEvent* aEvt, PRBool *_retval)
{
  // Obtain a presentation context
  nsCOMPtr<nsIDOMDocument> domDoc;
  mWindow->GetDocument(getter_AddRefs(domDoc));
  if (!domDoc)
    return NS_OK;
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  
  PRInt32 count = doc->GetNumberOfShells();
  if (count == 0)
    return NS_OK;

  nsCOMPtr<nsIPresShell> shell;
  doc->GetShellAt(0, getter_AddRefs(shell));
  
  // Retrieve the context
  nsCOMPtr<nsIPresContext> aPresContext;
  shell->GetPresContext(getter_AddRefs(aPresContext));

  nsCOMPtr<nsIEventStateManager> esm;
  if (NS_SUCCEEDED(aPresContext->GetEventStateManager(getter_AddRefs(esm)))) {
    return esm->DispatchNewEvent(NS_STATIC_CAST(nsIDOMEventReceiver*,this), aEvt, _retval);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowRoot::AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID)
{
  nsCOMPtr<nsIEventListenerManager> manager;
  GetListenerManager(getter_AddRefs(manager));
  if (manager) {
    manager->AddEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}
  
NS_IMETHODIMP
nsWindowRoot::RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID)
{
  nsCOMPtr<nsIEventListenerManager> manager;
  GetListenerManager(getter_AddRefs(manager));
  if (manager) {
    manager->RemoveEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowRoot::GetListenerManager(nsIEventListenerManager** aResult)
{
  if (!mListenerManager) {
    nsresult rv;
    mListenerManager = do_CreateInstance(kEventListenerManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;
  }

  *aResult = mListenerManager;
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowRoot::HandleEvent(nsIDOMEvent *aEvent)
{
  PRBool noDefault;
  return DispatchEvent(aEvent, &noDefault);
}

NS_IMETHODIMP nsWindowRoot::HandleChromeEvent(nsIPresContext* aPresContext,
   nsEvent* aEvent, nsIDOMEvent** aDOMEvent, PRUint32 aFlags, 
   nsEventStatus* aEventStatus)
{
  // Prevent the world from going
  // away until after we've finished handling the event.
  nsCOMPtr<nsIDOMWindow> kungFuDeathGrip(mWindow);

  nsresult ret = NS_OK;
  nsIDOMEvent* domEvent = nsnull;

  // We're at the top, so there's no bubbling or capturing here.
  if (NS_EVENT_FLAG_INIT & aFlags) {
    aDOMEvent = &domEvent;
    aEvent->flags = aFlags;
    aFlags &= ~(NS_EVENT_FLAG_CANT_BUBBLE | NS_EVENT_FLAG_CANT_CANCEL);
  }

  //Local handling stage
  if (mListenerManager && !(aEvent->flags & NS_EVENT_FLAG_STOP_DISPATCH)) {
    aEvent->flags |= aFlags;
    mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent, this, aFlags, aEventStatus);
    aEvent->flags &= ~aFlags;
  }

  if (NS_EVENT_FLAG_INIT & aFlags) {
    // We're leaving the DOM event loop so if we created a DOM event, release here.
    if (nsnull != *aDOMEvent) {
      nsrefcnt rc;
      NS_RELEASE2(*aDOMEvent, rc);
      if (0 != rc) {
      //Okay, so someone in the DOM loop (a listener, JS object) still has a ref to the DOM Event but
      //the internal data hasn't been malloc'd.  Force a copy of the data here so the DOM Event is still valid.
        nsIPrivateDOMEvent *privateEvent;
        if (NS_OK == (*aDOMEvent)->QueryInterface(NS_GET_IID(nsIPrivateDOMEvent), (void**)&privateEvent)) {
          privateEvent->DuplicatePrivateData();
          NS_RELEASE(privateEvent);
        }
      }
    }
    aDOMEvent = nsnull;
  }

  return ret;
}

NS_IMETHODIMP
nsWindowRoot::GetFocusController(nsIFocusController** aResult)
{
  *aResult = mFocusController;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewWindowRoot(nsIDOMWindow* aWindow, nsIChromeEventHandler** aResult)
{
  *aResult = new nsWindowRoot(aWindow);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
