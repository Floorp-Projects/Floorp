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
#include "nsGUIEvent.h"

#include "nsCycleCollectionParticipant.h"

static NS_DEFINE_CID(kEventListenerManagerCID,    NS_EVENTLISTENERMANAGER_CID);

nsWindowRoot::nsWindowRoot(nsIDOMWindow* aWindow)
{
  mWindow = aWindow;

  // Create and init our focus controller.
  nsFocusController::Create(getter_AddRefs(mFocusController));
}

nsWindowRoot::~nsWindowRoot()
{
  if (mListenerManager) {
    mListenerManager->Disconnect();
  }
}

NS_IMPL_CYCLE_COLLECTION_2(nsWindowRoot, mListenerManager, mFocusController)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsWindowRoot)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsPIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsPIWindowRoot)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIDOM3EventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSEventTarget)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsWindowRoot, nsIDOMEventTarget)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsWindowRoot, nsIDOMEventTarget)

NS_IMETHODIMP
nsWindowRoot::AddEventListener(const nsAString& aType, nsIDOMEventListener* aListener, PRBool aUseCapture)
{
  return AddEventListener(aType, aListener, aUseCapture, PR_FALSE, 0);
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
    static_cast<nsPIDOMEventTarget*>(this), nsnull, aEvt, nsnull, &status);
  *_retval = (status != nsEventStatus_eConsumeNoDefault);
  return rv;
}

nsresult
nsWindowRoot::DispatchDOMEvent(nsEvent* aEvent,
                               nsIDOMEvent* aDOMEvent,
                               nsPresContext* aPresContext,
                               nsEventStatus* aEventStatus)
{
  return nsEventDispatcher::DispatchDOMEvent(static_cast<nsPIDOMEventTarget*>(this),
                                             aEvent, aDOMEvent,
                                             aPresContext, aEventStatus);
}

NS_IMETHODIMP
nsWindowRoot::AddGroupedEventListener(const nsAString & aType, nsIDOMEventListener *aListener, 
                                          PRBool aUseCapture, nsIDOMEventGroup *aEvtGrp)
{
  nsCOMPtr<nsIEventListenerManager> manager = GetListenerManager(PR_TRUE);
  NS_ENSURE_STATE(manager);
  PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;
  return manager->AddEventListenerByType(aListener, aType, flags, aEvtGrp);
}

NS_IMETHODIMP
nsWindowRoot::RemoveGroupedEventListener(const nsAString & aType, nsIDOMEventListener *aListener, 
                                             PRBool aUseCapture, nsIDOMEventGroup *aEvtGrp)
{
  if (mListenerManager) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;
    return mListenerManager->RemoveEventListenerByType(aListener, aType, flags,
                                                       aEvtGrp);
  }
  return NS_OK;
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
                               PRBool aUseCapture, PRBool aWantsUntrusted,
                               PRUint8 optional_argc)
{
  NS_ASSERTION(!aWantsUntrusted || optional_argc > 0,
               "Won't check if this is chrome, you want to set "
               "aWantsUntrusted to PR_FALSE or make the aWantsUntrusted "
               "explicit by making optional_argc non-zero.");

  nsIEventListenerManager* manager = GetListenerManager(PR_TRUE);
  NS_ENSURE_STATE(manager);

  PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

  if (aWantsUntrusted) {
    flags |= NS_PRIV_EVENT_UNTRUSTED_PERMITTED;
  }

  return manager->AddEventListenerByType(aListener, aType, flags, nsnull);
}

nsresult
nsWindowRoot::AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID)
{
  nsIEventListenerManager* manager = GetListenerManager(PR_TRUE);
  NS_ENSURE_STATE(manager);
  return manager->AddEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
}
  
nsresult
nsWindowRoot::RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID)
{
  nsIEventListenerManager* manager = GetListenerManager(PR_TRUE);
  if (manager) {
    return manager->RemoveEventListenerByIID(aListener, aIID,
                                             NS_EVENT_FLAG_BUBBLE);
  }
  return NS_OK;
}

nsIEventListenerManager*
nsWindowRoot::GetListenerManager(PRBool aCreateIfNotFound)
{
  if (!mListenerManager) {
    if (!aCreateIfNotFound) {
      return nsnull;
    }

    mListenerManager = do_CreateInstance(kEventListenerManagerCID);
    if (mListenerManager) {
      mListenerManager->SetListenerTarget(
        static_cast<nsPIDOMEventTarget*>(this));
    }
  }

  return mListenerManager;
}

nsresult
nsWindowRoot::GetSystemEventGroup(nsIDOMEventGroup **aGroup)
{
  nsIEventListenerManager* manager = GetListenerManager(PR_TRUE);
  NS_ENSURE_STATE(manager);
  return manager->GetSystemEventGroupLM(aGroup);
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

nsIDOMWindow*
nsWindowRoot::GetWindow()
{
  return mWindow;
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
