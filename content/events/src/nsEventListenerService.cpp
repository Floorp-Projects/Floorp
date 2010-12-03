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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Olli Pettay <Olli.Pettay@helsinki.fi> (Original Author)
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
#include "nsEventListenerService.h"
#include "nsCOMArray.h"
#include "nsEventListenerManager.h"
#include "nsPIDOMEventTarget.h"
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
#include "nsIDOMEventGroup.h"
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
nsEventListenerInfo::GetCapturing(PRBool* aCapturing)
{
  *aCapturing = mCapturing;
  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerInfo::GetAllowsUntrusted(PRBool* aAllowsUntrusted)
{
  *aAllowsUntrusted = mAllowsUntrusted;
  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerInfo::GetInSystemEventGroup(PRBool* aInSystemEventGroup)
{
  *aInSystemEventGroup = mInSystemEventGroup;
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsEventListenerService, nsIEventListenerService)

// Caller must root *aJSVal!
PRBool
nsEventListenerInfo::GetJSVal(jsval* aJSVal)
{
  *aJSVal = JSVAL_NULL;
  nsCOMPtr<nsIXPConnectWrappedJS> wrappedJS = do_QueryInterface(mListener);
  if (wrappedJS) {
    JSObject* object = nsnull;
    wrappedJS->GetJSObject(&object);
    *aJSVal = OBJECT_TO_JSVAL(object);
    return PR_TRUE;
  }

  nsCOMPtr<nsIJSEventListener> jsl = do_QueryInterface(mListener);
  if (jsl) {
    nsresult rv = jsl->GetJSVal(mType, aJSVal);
    if (NS_SUCCEEDED(rv)) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

NS_IMETHODIMP
nsEventListenerInfo::ToSource(nsAString& aResult)
{
  aResult.SetIsVoid(PR_TRUE);

  nsCOMPtr<nsIThreadJSContextStack> stack =
    nsContentUtils::ThreadJSContextStack();
  if (stack) {
    JSContext* cx = nsnull;
    stack->GetSafeJSContext(&cx);
    if (cx && NS_SUCCEEDED(stack->Push(cx))) {
      {
        // Extra block to finish the auto request before calling pop
        JSAutoRequest ar(cx);
        jsval v = JSVAL_NULL;
        if (GetJSVal(&v)) {
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
  
  PRBool isOn = PR_FALSE;
  jsd->GetIsOn(&isOn);
  NS_ENSURE_TRUE(isOn, NS_OK);

  nsCOMPtr<nsIThreadJSContextStack> stack =
    nsContentUtils::ThreadJSContextStack();
  if (stack) {
    JSContext* cx = nsnull;
    stack->GetSafeJSContext(&cx);
    if (cx && NS_SUCCEEDED(stack->Push(cx))) {
      {
        // Extra block to finish the auto request before calling pop
        JSAutoRequest ar(cx);

        jsval v = JSVAL_NULL;
        if (GetJSVal(&v)) {
          nsCOMPtr<jsdIValue> jsdValue;
          jsd->WrapJSValue(v, getter_AddRefs(jsdValue));
          *aRetVal = jsdValue.forget().get();
          return NS_OK;
        }
      }
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
  *aCount = 0;
  *aOutArray = nsnull;
  nsCOMArray<nsIEventListenerInfo> listenerInfos;
  nsCOMPtr<nsPIDOMEventTarget> target = do_QueryInterface(aEventTarget);
  if (target) {
    nsCOMPtr<nsIEventListenerManager> elm =
      target->GetListenerManager(PR_FALSE);
    if (elm) {
      elm->GetListenerInfo(&listenerInfos);
    }
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
  nsCOMPtr<nsPIDOMEventTarget> target = do_QueryInterface(aEventTarget);
  NS_ENSURE_ARG(target);
  nsEvent event(PR_TRUE, NS_EVENT_TYPE_NULL);
  nsCOMArray<nsPIDOMEventTarget> targets;
  nsresult rv = nsEventDispatcher::Dispatch(target, nsnull, &event,
                                            nsnull, nsnull, nsnull, &targets);
  NS_ENSURE_SUCCESS(rv, rv);
  PRInt32 count = targets.Count();
  if (count == 0) {
    return NS_OK;
  }

  *aOutArray =
    static_cast<nsIDOMEventTarget**>(
      nsMemory::Alloc(sizeof(nsPIDOMEventTarget*) * count));
  NS_ENSURE_TRUE(*aOutArray, NS_ERROR_OUT_OF_MEMORY);

  for (PRInt32 i = 0; i < count; ++i) {
    nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(targets[i]);
    (*aOutArray)[i] = target.forget().get();
  }
  *aCount = count;

  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerService::GetSystemEventGroup(nsIDOMEventGroup** aSystemGroup)
{
  NS_ENSURE_ARG_POINTER(aSystemGroup);
  *aSystemGroup = nsEventListenerManager::GetSystemEventGroup();
  NS_ENSURE_TRUE(*aSystemGroup, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*aSystemGroup);
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
