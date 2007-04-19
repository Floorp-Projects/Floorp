/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David W. Hyatt <hyatt@netscape.com> (Original Author)
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
#include "nsWindowRoot.h"
#include "nsIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIEventListenerManager.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsLayoutCID.h"
#include "nsContentCID.h"
#include "nsIEventStateManager.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMWindowInternal.h"
#include "nsFocusController.h"
#include "nsString.h"
#include "nsEventDispatcher.h"
#include "nsIProgrammingLanguage.h"

#include "nsCycleCollectionParticipant.h"

static NS_DEFINE_CID(kEventListenerManagerCID,    NS_EVENTLISTENERMANAGER_CID);

nsWindowRoot::nsWindowRoot(nsIDOMWindow* aWindow)
{
  mWindow = aWindow;

  // Create and init our focus controller.
  nsFocusController::Create(getter_AddRefs(mFocusController));

  nsCOMPtr<nsIDOMFocusListener> focusListener(do_QueryInterface(mFocusController));
  mRefCnt.incr(NS_STATIC_CAST(nsIDOMEventReceiver*, this));
  AddEventListener(NS_LITERAL_STRING("focus"), focusListener, PR_TRUE);
  AddEventListener(NS_LITERAL_STRING("blur"), focusListener, PR_TRUE);
  mRefCnt.decr(NS_STATIC_CAST(nsIDOMEventReceiver*, this));
}

nsWindowRoot::~nsWindowRoot()
{
  if (mListenerManager) {
    mListenerManager->Disconnect();
  }
}

NS_IMPL_CYCLE_COLLECTION_2(nsWindowRoot, mListenerManager, mFocusController)

NS_INTERFACE_MAP_BEGIN(nsWindowRoot)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMEventReceiver)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventReceiver)
  NS_INTERFACE_MAP_ENTRY(nsPIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsPIWindowRoot)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIDOM3EventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSEventTarget)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsWindowRoot)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsWindowRoot, nsIDOMEventReceiver)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsWindowRoot, nsIDOMEventReceiver)

NS_IMETHODIMP
nsWindowRoot::AddEventListener(const nsAString& aType, nsIDOMEventListener* aListener, PRBool aUseCapture)
{
  return AddGroupedEventListener(aType, aListener, aUseCapture, nsnull);
}

NS_IMETHODIMP
nsWindowRoot::RemoveEventListener(const nsAString& aType, nsIDOMEventListener* aListener, PRBool aUseCapture)
{
  return RemoveGroupedEventListener(aType, aListener, aUseCapture, nsnull);
}

NS_IMETHODIMP
nsWindowRoot::DispatchEvent(nsIDOMEvent* aEvt, PRBool *_retval)
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv =  nsEventDispatcher::DispatchDOMEvent(
    NS_STATIC_CAST(nsPIDOMEventTarget*, this), nsnull, aEvt, nsnull, &status);
  *_retval = (status != nsEventStatus_eConsumeNoDefault);
  return rv;
}

nsresult
nsWindowRoot::DispatchDOMEvent(nsEvent* aEvent,
                               nsIDOMEvent* aDOMEvent,
                               nsPresContext* aPresContext,
                               nsEventStatus* aEventStatus)
{
  return nsEventDispatcher::DispatchDOMEvent(NS_STATIC_CAST(nsPIDOMEventTarget*, this),
                                             aEvent, aDOMEvent,
                                             aPresContext, aEventStatus);
}

NS_IMETHODIMP
nsWindowRoot::AddGroupedEventListener(const nsAString & aType, nsIDOMEventListener *aListener, 
                                          PRBool aUseCapture, nsIDOMEventGroup *aEvtGrp)
{
  nsCOMPtr<nsIEventListenerManager> manager;

  if (NS_SUCCEEDED(GetListenerManager(PR_TRUE, getter_AddRefs(manager)))) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;
    manager->AddEventListenerByType(aListener, aType, flags, aEvtGrp);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowRoot::RemoveGroupedEventListener(const nsAString & aType, nsIDOMEventListener *aListener, 
                                             PRBool aUseCapture, nsIDOMEventGroup *aEvtGrp)
{
  if (mListenerManager) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;
    mListenerManager->RemoveEventListenerByType(aListener, aType, flags, aEvtGrp);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowRoot::CanTrigger(const nsAString & type, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindowRoot::IsRegisteredHere(const nsAString & type, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindowRoot::AddEventListener(const nsAString& aType,
                               nsIDOMEventListener *aListener,
                               PRBool aUseCapture, PRBool aWantsUntrusted)
{
  nsCOMPtr<nsIEventListenerManager> manager;
  nsresult rv = GetListenerManager(PR_TRUE, getter_AddRefs(manager));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

  if (aWantsUntrusted) {
    flags |= NS_PRIV_EVENT_UNTRUSTED_PERMITTED;
  }

  return manager->AddEventListenerByType(aListener, aType, flags, nsnull);
}

NS_IMETHODIMP
nsWindowRoot::AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID)
{
  nsCOMPtr<nsIEventListenerManager> manager;
  GetListenerManager(PR_TRUE, getter_AddRefs(manager));
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
  GetListenerManager(PR_TRUE, getter_AddRefs(manager));
  if (manager) {
    manager->RemoveEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindowRoot::GetListenerManager(PRBool aCreateIfNotFound,
                                 nsIEventListenerManager** aResult)
{
  if (!mListenerManager) {
    if (!aCreateIfNotFound) {
      *aResult = nsnull;
      return NS_OK;
    }
    nsresult rv;
    mListenerManager = do_CreateInstance(kEventListenerManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;
    mListenerManager->SetListenerTarget(
      NS_STATIC_CAST(nsIDOMEventReceiver*, this));
  }

  *aResult = mListenerManager;
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowRoot::GetSystemEventGroup(nsIDOMEventGroup **aGroup)
{
  nsCOMPtr<nsIEventListenerManager> manager;
  if (NS_SUCCEEDED(GetListenerManager(PR_TRUE, getter_AddRefs(manager))) &&
    manager) {
    return manager->GetSystemEventGroupLM(aGroup);
  }
  return NS_ERROR_FAILURE;
}


nsresult
nsWindowRoot::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  aVisitor.mCanHandle = PR_TRUE;
  aVisitor.mForceContentDispatch = PR_TRUE; //FIXME! Bug 329119
  // To keep mWindow alive
  aVisitor.mItemData = mWindow;
  return NS_OK;
}

nsresult
nsWindowRoot::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  return NS_OK;
}

NS_IMETHODIMP
nsWindowRoot::GetFocusController(nsIFocusController** aResult)
{
  *aResult = mFocusController;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsWindowRoot::GetScriptTypeID(PRUint32 *aScriptType)
{
    NS_ERROR("No default script type here - ask some element");
    return nsIProgrammingLanguage::UNKNOWN;
}

NS_IMETHODIMP
nsWindowRoot::SetScriptTypeID(PRUint32 aScriptType)
{
    NS_ERROR("Can't change default script type for a document");
    return NS_ERROR_NOT_IMPLEMENTED;
}

///////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewWindowRoot(nsIDOMWindow* aWindow, nsPIDOMEventTarget** aResult)
{
  *aResult = new nsWindowRoot(aWindow);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
