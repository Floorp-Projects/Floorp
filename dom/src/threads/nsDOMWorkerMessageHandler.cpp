/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*- */
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
 * The Original Code is Web Workers.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com> (Original Author)
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

#include "nsDOMWorkerMessageHandler.h"

#include "nsIDOMEvent.h"
#include "nsIXPConnect.h"

#include "nsContentUtils.h"

#include "nsDOMThreadService.h"
#include "nsDOMWorkerEvents.h"

NS_IMPL_THREADSAFE_ADDREF(nsDOMWorkerEventListenerBase)
NS_IMPL_THREADSAFE_RELEASE(nsDOMWorkerEventListenerBase)

nsresult
nsDOMWorkerWeakEventListener::Init(nsIDOMEventListener* aListener)
{
  NS_ENSURE_ARG_POINTER(aListener);

  nsCOMPtr<nsIXPConnectWrappedJS> wrappedJS(do_QueryInterface(aListener));
  NS_ENSURE_TRUE(wrappedJS, NS_NOINTERFACE);

  JSObject* obj;
  nsresult rv = wrappedJS->GetJSObject(&obj);
  NS_ENSURE_SUCCESS(rv, rv);

  mObj = obj;

  return NS_OK;
}

already_AddRefed<nsIDOMEventListener>
nsDOMWorkerWeakEventListener::GetListener()
{
  JSContext* cx = nsDOMThreadService::GetCurrentContext();
  NS_ENSURE_TRUE(cx, nsnull);

  nsIXPConnect* xpc = nsContentUtils::XPConnect();

  nsCOMPtr<nsIDOMEventListener> listener;
  nsresult rv = xpc->WrapJS(cx, mObj, NS_GET_IID(nsIDOMEventListener),
                            getter_AddRefs(listener));
  NS_ENSURE_SUCCESS(rv, nsnull);

  return listener.forget();
}

nsDOMWorkerWrappedWeakEventListener::
nsDOMWorkerWrappedWeakEventListener(nsDOMWorkerWeakEventListener* aInner)
: mInner(aInner)
{
  NS_ASSERTION(aInner, "Null pointer!");
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsDOMWorkerMessageHandler,
                              nsIDOMEventTarget,
                              nsIClassInfo)

NS_IMPL_CI_INTERFACE_GETTER1(nsDOMWorkerMessageHandler,
                             nsIDOMEventTarget)

NS_IMPL_THREADSAFE_DOM_CI(nsDOMWorkerMessageHandler)

const nsDOMWorkerMessageHandler::ListenerCollection*
nsDOMWorkerMessageHandler::GetListenerCollection(const nsAString& aType) const
{
  PRUint32 count = mCollections.Length();
  for (PRUint32 index = 0; index < count; index++) {
    const ListenerCollection& collection = mCollections[index];
    if (collection.type.Equals(aType)) {
      return &collection;
    }
  }
  return nsnull;
}

void
nsDOMWorkerMessageHandler::GetListenersForType(const nsAString& aType,
                                               ListenerArray& _retval) const
{
  _retval.Clear();

  const ListenerCollection* collection = GetListenerCollection(aType);
  if (collection) {
    PRUint32 count = collection->listeners.Length();

    if (!_retval.SetLength(count)) {
      NS_WARNING("Out of memory!");
      return;
    }

    for (PRUint32 index = 0; index < count; index++) {
      nsCOMPtr<nsIDOMEventListener> listener =
        collection->listeners[index]->GetListener();
      _retval[index].swap(listener);
    }
  }
}

nsresult
nsDOMWorkerMessageHandler::SetOnXListener(const nsAString& aType,
                                          nsIDOMEventListener* aListener)
{
  nsRefPtr<nsDOMWorkerWrappedWeakEventListener> wrappedListener;

  ListenerCollection* collection =
    const_cast<ListenerCollection*>(GetListenerCollection(aType));

#ifdef DEBUG
  PRBool removed;
#endif

  if (collection) {
    wrappedListener.swap(collection->onXListener);
    if (wrappedListener) {
#ifdef DEBUG
      removed =
#endif
      collection->listeners.RemoveElement(wrappedListener);
      NS_ASSERTION(removed, "Element wasn't in the list!");
    }
  }

  if (!aListener) {
    if (collection && !collection->listeners.Length()) {
#ifdef DEBUG
      removed =
#endif
      mCollections.RemoveElement(*collection);
      NS_ASSERTION(removed, "Element wasn't in the list!");
    }
    return NS_OK;
  }

  nsRefPtr<nsDOMWorkerWeakEventListener> weakListener =
    new nsDOMWorkerWeakEventListener();
  NS_ENSURE_TRUE(weakListener, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = weakListener->Init(aListener);
  NS_ENSURE_SUCCESS(rv, rv);

  wrappedListener = new nsDOMWorkerWrappedWeakEventListener(weakListener);
  NS_ENSURE_TRUE(wrappedListener, NS_ERROR_OUT_OF_MEMORY);

  if (!collection) {
    collection = mCollections.AppendElement(aType);
    NS_ENSURE_TRUE(collection, NS_ERROR_OUT_OF_MEMORY);
  }

  WeakListener* newListener =
    collection->listeners.AppendElement(wrappedListener);
  NS_ENSURE_TRUE(newListener, NS_ERROR_OUT_OF_MEMORY);

  wrappedListener.swap(collection->onXListener);
  return NS_OK;
}

already_AddRefed<nsIDOMEventListener>
nsDOMWorkerMessageHandler::GetOnXListener(const nsAString& aType) const
{
  const ListenerCollection* collection = GetListenerCollection(aType);
  if (collection && collection->onXListener) {
    return collection->onXListener->GetListener();
  }

  return nsnull;
}

void
nsDOMWorkerMessageHandler::ClearListeners(const nsAString& aType)
{
  PRUint32 count = mCollections.Length();
  for (PRUint32 index = 0; index < count; index++) {
    if (mCollections[index].type.Equals(aType)) {
      mCollections.RemoveElementAt(index);
      return;
    }
  }
}

PRBool
nsDOMWorkerMessageHandler::HasListeners(const nsAString& aType)
{
  const ListenerCollection* collection = GetListenerCollection(aType);
  return collection && collection->listeners.Length();
}

void
nsDOMWorkerMessageHandler::ClearAllListeners()
{
  mCollections.Clear();
}

void
nsDOMWorkerMessageHandler::Trace(JSTracer* aTracer)
{
  PRUint32 cCount = mCollections.Length();
  for (PRUint32 cIndex = 0; cIndex < cCount; cIndex++) {
    const ListenerCollection& collection = mCollections[cIndex];
    PRUint32 lCount = collection.listeners.Length();
    for (PRUint32 lIndex = 0; lIndex < lCount; lIndex++) {
      JSObject* obj = collection.listeners[lIndex]->GetJSObject();
      NS_ASSERTION(obj, "Null object!");
      JS_SET_TRACING_DETAILS(aTracer, nsnull, this, 0);
      JS_CallTracer(aTracer, obj, JSTRACE_OBJECT);
    }
  }
}

/**
 * See nsIDOMEventTarget
 */
NS_IMETHODIMP
nsDOMWorkerMessageHandler::RemoveEventListener(const nsAString& aType,
                                               nsIDOMEventListener* aListener,
                                               PRBool aUseCapture)
{
  ListenerCollection* collection =
    const_cast<ListenerCollection*>(GetListenerCollection(aType));

  if (collection) {
    PRUint32 count = collection->listeners.Length();
    for (PRUint32 index = 0; index < count; index++) {
      WeakListener& weakListener = collection->listeners[index];
      if (weakListener == collection->onXListener) {
        continue;
      }
      nsCOMPtr<nsIDOMEventListener> listener = weakListener->GetListener();
      if (listener == aListener) {
        collection->listeners.RemoveElementAt(index);
        break;
      }
    }

    if (!collection->listeners.Length()) {
#ifdef DEBUG
      PRBool removed =
#endif
      mCollections.RemoveElement(*collection);
      NS_ASSERTION(removed, "Somehow this wasn't in the list!");
    }
  }

  return NS_OK;
}

/**
 * See nsIDOMEventTarget
 */
NS_IMETHODIMP
nsDOMWorkerMessageHandler::DispatchEvent(nsIDOMEvent* aEvent,
                                         PRBool* _retval)
{
  NS_ENSURE_ARG_POINTER(aEvent);

  nsCOMPtr<nsIDOMWorkerPrivateEvent> event;

  if (_retval) {
    event = do_QueryInterface(aEvent);
    if (!event) {
      event = new nsDOMWorkerPrivateEvent(aEvent);
      NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);
    }
    aEvent = event;
  }

  nsAutoString type;
  nsresult rv = aEvent->GetType(type);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoTArray<Listener, 10>  listeners;
  GetListenersForType(type, listeners);

  PRUint32 count = listeners.Length();
  for (PRUint32 index = 0; index < count; index++) {
    const Listener& listener = listeners[index];
    NS_ASSERTION(listener, "Null listener in array!");

    listener->HandleEvent(aEvent);
  }

  if (_retval) {
    *_retval = event->PreventDefaultCalled();
  }

  return NS_OK;
}

/**
 * See nsIDOMEventTarget
 */
NS_IMETHODIMP
nsDOMWorkerMessageHandler::AddEventListener(const nsAString& aType,
                                            nsIDOMEventListener* aListener,
                                            PRBool aUseCapture,
                                            PRBool aWantsUntrusted,
                                            PRUint8 aOptionalArgc)
{
  // We don't support aWantsUntrusted yet.
  NS_ENSURE_TRUE(aOptionalArgc < 2, NS_ERROR_NOT_IMPLEMENTED);

  ListenerCollection* collection =
    const_cast<ListenerCollection*>(GetListenerCollection(aType));

  if (!collection) {
    collection = mCollections.AppendElement(aType);
    NS_ENSURE_TRUE(collection, NS_ERROR_OUT_OF_MEMORY);
  }

  nsRefPtr<nsDOMWorkerWeakEventListener> weakListener =
    new nsDOMWorkerWeakEventListener();
  NS_ENSURE_TRUE(weakListener, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = weakListener->Init(aListener);
  NS_ENSURE_SUCCESS(rv, rv);

  WeakListener* newListener = collection->listeners.AppendElement(weakListener);
  NS_ENSURE_TRUE(newListener, NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

nsIDOMEventTarget *
nsDOMWorkerMessageHandler::GetTargetForDOMEvent()
{
  NS_ERROR("Should not be called");
  return nsnull;
}

nsIDOMEventTarget *
nsDOMWorkerMessageHandler::GetTargetForEventTargetChain()
{
  NS_ERROR("Should not be called");
  return nsnull;
}

nsresult
nsDOMWorkerMessageHandler::PreHandleEvent(nsEventChainPreVisitor & aVisitor)
{
  NS_ERROR("Should not be called");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsDOMWorkerMessageHandler::WillHandleEvent(nsEventChainPostVisitor & aVisitor)
{
  NS_ERROR("Should not be called");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsDOMWorkerMessageHandler::PostHandleEvent(nsEventChainPostVisitor & aVisitor)
{
  NS_ERROR("Should not be called");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsDOMWorkerMessageHandler::DispatchDOMEvent(nsEvent *aEvent, nsIDOMEvent *aDOMEvent,
                                            nsPresContext *aPresContext,
                                            nsEventStatus *aEventStatus)
{
  NS_ERROR("Should not be called");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsEventListenerManager*
nsDOMWorkerMessageHandler::GetListenerManager(PRBool aMayCreate)
{
  NS_ERROR("Should not be called");
  return nsnull;
}

nsresult
nsDOMWorkerMessageHandler::AddEventListenerByIID(nsIDOMEventListener *aListener,
                                                 const nsIID & aIID)
{
  NS_ERROR("Should not be called");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsDOMWorkerMessageHandler::RemoveEventListenerByIID(nsIDOMEventListener *aListener,
                                                    const nsIID & aIID)
{
  NS_ERROR("Should not be called");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsIScriptContext*
nsDOMWorkerMessageHandler::GetContextForEventHandlers(nsresult *aRv)
{
  NS_ERROR("Should not be called");
  *aRv = NS_ERROR_NOT_IMPLEMENTED;
  return nsnull;
}

JSContext*
nsDOMWorkerMessageHandler::GetJSContextForEventHandlers()
{
  NS_ERROR("Should not be called");
  return nsnull;
}
