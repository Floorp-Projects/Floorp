/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsEventListenerService.h"
#include "nsCOMArray.h"
#include "nsEventListenerManager.h"
#include "nsIVariant.h"
#include "nsIServiceManager.h"
#include "nsMemory.h"
#include "nsContentUtils.h"
#include "nsIXPConnect.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsJSUtils.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIJSContextStack.h"
#include "nsGUIEvent.h"
#include "nsEventDispatcher.h"
#include "nsIJSEventListener.h"
#ifdef MOZ_JSDEBUGGER
#include "jsdIDebuggerService.h"
#endif

NS_IMPL_CYCLE_COLLECTION_1(nsEventListenerInfo, mListener)

DOMCI_DATA(EventListenerInfo, nsEventListenerInfo)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsEventListenerInfo)
  NS_INTERFACE_MAP_ENTRY(nsIEventListenerInfo)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(EventListenerInfo)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsEventListenerInfo)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsEventListenerInfo)

NS_IMETHODIMP
nsEventListenerInfo::GetType(nsAString& aType)
{
  aType = mType;
  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerInfo::GetCapturing(bool* aCapturing)
{
  *aCapturing = mCapturing;
  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerInfo::GetAllowsUntrusted(bool* aAllowsUntrusted)
{
  *aAllowsUntrusted = mAllowsUntrusted;
  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerInfo::GetInSystemEventGroup(bool* aInSystemEventGroup)
{
  *aInSystemEventGroup = mInSystemEventGroup;
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsEventListenerService, nsIEventListenerService)

// Caller must root *aJSVal!
bool
nsEventListenerInfo::GetJSVal(JSContext* aCx, JSAutoEnterCompartment& aAc, jsval* aJSVal)
{
  *aJSVal = JSVAL_NULL;
  nsCOMPtr<nsIXPConnectWrappedJS> wrappedJS = do_QueryInterface(mListener);
  if (wrappedJS) {
    JSObject* object = nsnull;
    if (NS_FAILED(wrappedJS->GetJSObject(&object)) || !aAc.enter(aCx, object)) {
      return false;
    }
    *aJSVal = OBJECT_TO_JSVAL(object);
    return true;
  }

  nsCOMPtr<nsIJSEventListener> jsl = do_QueryInterface(mListener);
  if (jsl) {
    JSObject *handler = jsl->GetHandler();
    if (handler) {
      if (!aAc.enter(aCx, handler)) {
        return false;
      }
      *aJSVal = OBJECT_TO_JSVAL(handler);
      return true;
    }
  }
  return false;
}

NS_IMETHODIMP
nsEventListenerInfo::ToSource(nsAString& aResult)
{
  aResult.SetIsVoid(true);

  nsCOMPtr<nsIThreadJSContextStack> stack =
    nsContentUtils::ThreadJSContextStack();
  if (stack) {
    JSContext* cx = stack->GetSafeJSContext();
    if (cx && NS_SUCCEEDED(stack->Push(cx))) {
      {
        // Extra block to finish the auto request before calling pop
        JSAutoRequest ar(cx);
        JSAutoEnterCompartment ac;
        jsval v = JSVAL_NULL;
        if (GetJSVal(cx, ac, &v)) {
          JSString* str = JS_ValueToSource(cx, v);
          if (str) {
            nsDependentJSString depStr;
            if (depStr.init(cx, str)) {
              aResult.Assign(depStr);
            }
          }
        }
      }
      stack->Pop(&cx);
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerInfo::GetDebugObject(nsISupports** aRetVal)
{
  *aRetVal = nsnull;

#ifdef MOZ_JSDEBUGGER
  nsresult rv = NS_OK;
  nsCOMPtr<jsdIDebuggerService> jsd =
    do_GetService("@mozilla.org/js/jsd/debugger-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, NS_OK);
  
  bool isOn = false;
  jsd->GetIsOn(&isOn);
  NS_ENSURE_TRUE(isOn, NS_OK);

  nsCOMPtr<nsIThreadJSContextStack> stack =
    nsContentUtils::ThreadJSContextStack();
  if (stack) {
    JSContext* cx = stack->GetSafeJSContext();
    if (cx && NS_SUCCEEDED(stack->Push(cx))) {
      {
        // Extra block to finish the auto request before calling pop
        JSAutoRequest ar(cx);
        JSAutoEnterCompartment ac;
        jsval v = JSVAL_NULL;
        if (GetJSVal(cx, ac, &v)) {
          nsCOMPtr<jsdIValue> jsdValue;
          rv = jsd->WrapValue(v, getter_AddRefs(jsdValue));
          NS_ENSURE_SUCCESS(rv, rv);
          jsdValue.forget(aRetVal);
        }
      }
      stack->Pop(&cx);
    }
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerService::GetListenerInfoFor(nsIDOMEventTarget* aEventTarget,
                                           PRUint32* aCount,
                                           nsIEventListenerInfo*** aOutArray)
{
  NS_ENSURE_ARG_POINTER(aEventTarget);
  *aCount = 0;
  *aOutArray = nsnull;
  nsCOMArray<nsIEventListenerInfo> listenerInfos;
  nsEventListenerManager* elm =
    aEventTarget->GetListenerManager(false);
  if (elm) {
    elm->GetListenerInfo(&listenerInfos);
  }

  PRInt32 count = listenerInfos.Count();
  if (count == 0) {
    return NS_OK;
  }

  *aOutArray =
    static_cast<nsIEventListenerInfo**>(
      nsMemory::Alloc(sizeof(nsIEventListenerInfo*) * count));
  NS_ENSURE_TRUE(*aOutArray, NS_ERROR_OUT_OF_MEMORY);

  for (PRInt32 i = 0; i < count; ++i) {
    NS_ADDREF((*aOutArray)[i] = listenerInfos[i]);
  }
  *aCount = count;
  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerService::GetEventTargetChainFor(nsIDOMEventTarget* aEventTarget,
                                               PRUint32* aCount,
                                               nsIDOMEventTarget*** aOutArray)
{
  *aCount = 0;
  *aOutArray = nsnull;
  NS_ENSURE_ARG(aEventTarget);
  nsEvent event(true, NS_EVENT_TYPE_NULL);
  nsCOMArray<nsIDOMEventTarget> targets;
  nsresult rv = nsEventDispatcher::Dispatch(aEventTarget, nsnull, &event,
                                            nsnull, nsnull, nsnull, &targets);
  NS_ENSURE_SUCCESS(rv, rv);
  PRInt32 count = targets.Count();
  if (count == 0) {
    return NS_OK;
  }

  *aOutArray =
    static_cast<nsIDOMEventTarget**>(
      nsMemory::Alloc(sizeof(nsIDOMEventTarget*) * count));
  NS_ENSURE_TRUE(*aOutArray, NS_ERROR_OUT_OF_MEMORY);

  for (PRInt32 i = 0; i < count; ++i) {
    NS_ADDREF((*aOutArray)[i] = targets[i]);
  }
  *aCount = count;

  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerService::HasListenersFor(nsIDOMEventTarget* aEventTarget,
                                        const nsAString& aType,
                                        bool* aRetVal)
{
  nsEventListenerManager* elm = aEventTarget->GetListenerManager(false);
  *aRetVal = elm && elm->HasListenersFor(aType);
  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerService::AddSystemEventListener(nsIDOMEventTarget *aTarget,
                                               const nsAString& aType,
                                               nsIDOMEventListener* aListener,
                                               bool aUseCapture)
{
  NS_PRECONDITION(aTarget, "Missing target");
  NS_PRECONDITION(aListener, "Missing listener");

  nsEventListenerManager* manager = aTarget->GetListenerManager(true);
  NS_ENSURE_STATE(manager);

  PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE |
                                NS_EVENT_FLAG_SYSTEM_EVENT :
                                NS_EVENT_FLAG_BUBBLE |
                                NS_EVENT_FLAG_SYSTEM_EVENT;
  manager->AddEventListenerByType(aListener, aType, flags);
  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerService::RemoveSystemEventListener(nsIDOMEventTarget *aTarget,
                                                  const nsAString& aType,
                                                  nsIDOMEventListener* aListener,
                                                  bool aUseCapture)
{
  NS_PRECONDITION(aTarget, "Missing target");
  NS_PRECONDITION(aListener, "Missing listener");

  nsEventListenerManager* manager = aTarget->GetListenerManager(false);
  if (manager) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE |
                                  NS_EVENT_FLAG_SYSTEM_EVENT :
                                  NS_EVENT_FLAG_BUBBLE |
                                  NS_EVENT_FLAG_SYSTEM_EVENT;
    manager->RemoveEventListenerByType(aListener, aType, flags);
  }

  return NS_OK;
}

nsresult
NS_NewEventListenerService(nsIEventListenerService** aResult)
{
  *aResult = new nsEventListenerService();
  NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*aResult);
  return NS_OK;
}
