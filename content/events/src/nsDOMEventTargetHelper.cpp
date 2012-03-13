/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Olli Pettay <Olli.Pettay@helsinki.fi>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsDOMEventTargetHelper.h"
#include "nsContentUtils.h"
#include "nsEventDispatcher.h"
#include "nsGUIEvent.h"
#include "nsIDocument.h"
#include "nsIJSContextStack.h"
#include "nsDOMJSUtils.h"
#include "prprf.h"
#include "nsGlobalWindow.h"

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMEventListenerWrapper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMEventListenerWrapper)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
NS_INTERFACE_MAP_END_AGGREGATED(mListener)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMEventListenerWrapper)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMEventListenerWrapper)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMEventListenerWrapper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mListener)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsDOMEventListenerWrapper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mListener)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMETHODIMP
nsDOMEventListenerWrapper::HandleEvent(nsIDOMEvent* aEvent)
{
  return mListener->HandleEvent(aEvent);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(nsDOMEventTargetHelper)
  if (NS_UNLIKELY(cb.WantDebugInfo())) {
    char name[512];
    nsAutoString uri;
    if (tmp->mOwner && tmp->mOwner->GetExtantDocument()) {
      tmp->mOwner->GetExtantDocument()->GetDocumentURI(uri);
    }
    PR_snprintf(name, sizeof(name), "nsDOMEventTargetHelper %s",
                NS_ConvertUTF16toUTF8(uri).get());
    cb.DescribeRefCountedNode(tmp->mRefCnt.get(), sizeof(nsDOMEventTargetHelper),
                              name);
  } else {
    NS_IMPL_CYCLE_COLLECTION_DESCRIBE(nsDOMEventTargetHelper, tmp->mRefCnt.get())
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_MEMBER(mListenerManager,
                                                  nsEventListenerManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mListenerManager)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMEventTargetHelper)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMEventTargetHelper)

NS_IMPL_DOMTARGET_DEFAULTS(nsDOMEventTargetHelper);

nsDOMEventTargetHelper::~nsDOMEventTargetHelper()
{
  if (mOwner) {
    static_cast<nsGlobalWindow*>(mOwner)->RemoveEventTargetObject(this);
  }
  if (mListenerManager) {
    mListenerManager->Disconnect();
  }
  nsContentUtils::ReleaseWrapper(this, this);
}

void
nsDOMEventTargetHelper::BindToOwner(nsPIDOMWindow* aOwner)
{
  if (mOwner) {
    static_cast<nsGlobalWindow*>(mOwner)->RemoveEventTargetObject(this);
    mOwner = nsnull;
    mHasOrHasHadOwner = false;
  }
  if (aOwner) {
    mOwner = aOwner;
    mHasOrHasHadOwner = true;
    static_cast<nsGlobalWindow*>(mOwner)->AddEventTargetObject(this);
  }
}

void
nsDOMEventTargetHelper::BindToOwner(nsDOMEventTargetHelper* aOther)
{
  if (mOwner) {
    static_cast<nsGlobalWindow*>(mOwner)->RemoveEventTargetObject(this);
    mOwner = nsnull;
    mHasOrHasHadOwner = false;
  }
  if (aOther) {
    mHasOrHasHadOwner = aOther->HasOrHasHadOwner();
    if (aOther->GetOwner()) {
      mOwner = aOther->GetOwner();
      mHasOrHasHadOwner = true;
      static_cast<nsGlobalWindow*>(mOwner)->AddEventTargetObject(this);
    }
  }
}

void
nsDOMEventTargetHelper::DisconnectFromOwner()
{
  mOwner = nsnull;
  // Event listeners can't be handled anymore, so we can release them here.
  if (mListenerManager) {
    mListenerManager->Disconnect();
    mListenerManager = nsnull;
  }
}

NS_IMETHODIMP
nsDOMEventTargetHelper::RemoveEventListener(const nsAString& aType,
                                            nsIDOMEventListener* aListener,
                                            bool aUseCapture)
{
  nsEventListenerManager* elm = GetListenerManager(false);
  if (elm) {
    elm->RemoveEventListener(aType, aListener, aUseCapture);
  }

  return NS_OK;
}

NS_IMPL_REMOVE_SYSTEM_EVENT_LISTENER(nsDOMEventTargetHelper)

NS_IMETHODIMP
nsDOMEventTargetHelper::AddEventListener(const nsAString& aType,
                                         nsIDOMEventListener *aListener,
                                         bool aUseCapture,
                                         bool aWantsUntrusted,
                                         PRUint8 aOptionalArgc)
{
  NS_ASSERTION(!aWantsUntrusted || aOptionalArgc > 1,
               "Won't check if this is chrome, you want to set "
               "aWantsUntrusted to false or make the aWantsUntrusted "
               "explicit by making aOptionalArgc non-zero.");

  if (aOptionalArgc < 2) {
    nsresult rv;
    nsIScriptContext* context = GetContextForEventHandlers(&rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIDocument> doc =
      nsContentUtils::GetDocumentFromScriptContext(context);
    aWantsUntrusted = doc && !nsContentUtils::IsChromeDoc(doc);
  }

  nsEventListenerManager* elm = GetListenerManager(true);
  NS_ENSURE_STATE(elm);
  elm->AddEventListener(aType, aListener, aUseCapture, aWantsUntrusted);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMEventTargetHelper::AddSystemEventListener(const nsAString& aType,
                                               nsIDOMEventListener *aListener,
                                               bool aUseCapture,
                                               bool aWantsUntrusted,
                                               PRUint8 aOptionalArgc)
{
  NS_ASSERTION(!aWantsUntrusted || aOptionalArgc > 1,
               "Won't check if this is chrome, you want to set "
               "aWantsUntrusted to false or make the aWantsUntrusted "
               "explicit by making aOptionalArgc non-zero.");

  if (aOptionalArgc < 2) {
    nsresult rv;
    nsIScriptContext* context = GetContextForEventHandlers(&rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIDocument> doc =
      nsContentUtils::GetDocumentFromScriptContext(context);
    aWantsUntrusted = doc && !nsContentUtils::IsChromeDoc(doc);
  }

  return NS_AddSystemEventListener(this, aType, aListener, aUseCapture,
                                   aWantsUntrusted);
}

NS_IMETHODIMP
nsDOMEventTargetHelper::DispatchEvent(nsIDOMEvent* aEvent, bool* aRetVal)
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv =
    nsEventDispatcher::DispatchDOMEvent(this, nsnull, aEvent, nsnull, &status);

  *aRetVal = (status != nsEventStatus_eConsumeNoDefault);
  return rv;
}

nsresult
nsDOMEventTargetHelper::RemoveAddEventListener(const nsAString& aType,
                                               nsRefPtr<nsDOMEventListenerWrapper>& aCurrent,
                                               nsIDOMEventListener* aNew)
{
  if (aCurrent) {
    RemoveEventListener(aType, aCurrent, false);
    aCurrent = nsnull;
  }
  if (aNew) {
    aCurrent = new nsDOMEventListenerWrapper(aNew);
    NS_ENSURE_TRUE(aCurrent, NS_ERROR_OUT_OF_MEMORY);
    nsIDOMEventTarget::AddEventListener(aType, aCurrent, false);
  }
  return NS_OK;
}

nsresult
nsDOMEventTargetHelper::GetInnerEventListener(nsRefPtr<nsDOMEventListenerWrapper>& aWrapper,
                                              nsIDOMEventListener** aListener)
{
  NS_ENSURE_ARG_POINTER(aListener);
  if (aWrapper) {
    NS_ADDREF(*aListener = aWrapper->GetInner());
  } else {
    *aListener = nsnull;
  }
  return NS_OK;
}


nsresult
nsDOMEventTargetHelper::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  aVisitor.mCanHandle = true;
  aVisitor.mParentTarget = nsnull;
  return NS_OK;
}

nsresult
nsDOMEventTargetHelper::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  return NS_OK;
}

nsresult
nsDOMEventTargetHelper::DispatchDOMEvent(nsEvent* aEvent,
                                         nsIDOMEvent* aDOMEvent,
                                         nsPresContext* aPresContext,
                                         nsEventStatus* aEventStatus)
{
  return
    nsEventDispatcher::DispatchDOMEvent(this, aEvent, aDOMEvent, aPresContext,
                                        aEventStatus);
}

nsEventListenerManager*
nsDOMEventTargetHelper::GetListenerManager(bool aCreateIfNotFound)
{
  if (!mListenerManager && aCreateIfNotFound) {
    mListenerManager = new nsEventListenerManager(this);
  }

  return mListenerManager;
}

nsIScriptContext*
nsDOMEventTargetHelper::GetContextForEventHandlers(nsresult* aRv)
{
  *aRv = CheckInnerWindowCorrectness();
  if (NS_FAILED(*aRv)) {
    return nsnull;
  }
  return mOwner ? static_cast<nsGlobalWindow*>(mOwner)->GetContextInternal()
                : nsnull;
}

void
nsDOMEventTargetHelper::Init(JSContext* aCx)
{
  JSContext* cx = aCx;
  if (!cx) {
    nsIJSContextStack* stack = nsContentUtils::ThreadJSContextStack();

    if (!stack)
      return;

    if (NS_FAILED(stack->Peek(&cx)) || !cx)
      return;
  }

  NS_ASSERTION(cx, "Should have returned earlier ...");
  nsIScriptContext* context = GetScriptContextFromJSContext(cx);
  if (context) {
    nsCOMPtr<nsPIDOMWindow> window =
      do_QueryInterface(context->GetGlobalObject());
    if (window) {
      BindToOwner(window->GetCurrentInnerWindow());
    }
  }
}
