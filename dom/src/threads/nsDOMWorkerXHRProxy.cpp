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
 * The Original Code is worker threads.
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

#include "nsDOMWorkerXHRProxy.h"

// Interfaces
#include "nsIThread.h"
#include "nsIVariant.h"
#include "nsIXMLHttpRequest.h"

// Other includes
#include "nsAutoLock.h"
#include "nsComponentManagerUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"
#include "nsXMLHttpRequest.h"
#include "prinrval.h"
#include "prthread.h"

// DOMWorker includes
#include "nsDOMWorkerPool.h"
#include "nsDOMWorkerThread.h"
#include "nsDOMWorkerXHRProxiedFunctions.h"

#define MAX_XHR_LISTENER_TYPE nsDOMWorkerXHREventTarget::sMaxXHREventTypes
#define MAX_UPLOAD_LISTENER_TYPE nsDOMWorkerXHREventTarget::sMaxUploadEventTypes

using namespace nsDOMWorkerProxiedXHRFunctions;

class nsResultReturningRunnable : public nsRunnable
{
public:
  nsResultReturningRunnable(nsIEventTarget* aTarget, nsIRunnable* aRunnable,
                            nsDOMWorkerThread* aWorker)
  : mTarget(aTarget), mRunnable(aRunnable), mWorker(aWorker),
    mResult(NS_OK), mDone(PR_FALSE) { }

  nsresult Dispatch() {
    if (!mWorker) {
      // Must have been canceled, bail out.
      return NS_ERROR_ABORT;
    }

    nsIThread* currentThread = NS_GetCurrentThread();
    NS_ASSERTION(currentThread, "This should never be null!");

    nsresult rv = mTarget->Dispatch(this, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);

    // Complicated logic: Spin events while we haven't been canceled (if a
    // cancel boolean was supplied) and while we're not done.
    while (!mWorker->IsCanceled() && !mDone) {
      // Process a single event or yield if no event is pending.
      if (!NS_ProcessNextEvent(currentThread, PR_FALSE)) {
        PR_Sleep(PR_INTERVAL_NO_WAIT);
      }
    }

    if (mWorker->IsCanceled()) {
      mResult = NS_ERROR_ABORT;
    }

    return mResult;
  }

  NS_IMETHOD Run() {
#ifdef DEBUG
    PRBool rightThread = PR_FALSE;
    mTarget->IsOnCurrentThread(&rightThread);
    NS_ASSERTION(rightThread, "Run called on the wrong thread!");
#endif

    mResult = mWorker->IsCanceled() ? NS_ERROR_ABORT : mRunnable->Run();
    mDone = PR_TRUE;

    return mResult;
  }

private:
  nsCOMPtr<nsIEventTarget> mTarget;
  nsCOMPtr<nsIRunnable> mRunnable;
  nsRefPtr<nsDOMWorkerThread> mWorker;
  nsresult mResult;
  volatile PRBool mDone;
};

class nsDOMWorkerXHREvent : public nsRunnable,
                            public nsIDOMEvent,
                            public nsIClassInfo
{
  friend class nsDOMWorkerXHRProxy;
  friend class nsDOMWorkerXHREventTargetProxy;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIDOMEVENT
  NS_DECL_NSICLASSINFO

  nsDOMWorkerXHREvent(nsDOMWorkerXHRProxy* aXHRProxy);

  nsresult Init(nsIDOMEvent* aEvent);
  nsresult Init(nsIXMLHttpRequest* aXHR);

  void EventHandled();

protected:
  nsRefPtr<nsDOMWorkerXHRProxy> mXHRProxy;
  nsCOMPtr<nsIDOMEventTarget> mTarget;
  nsString mTypeString;
  PRUint32 mType;
  PRUint16 mEventPhase;
  DOMTimeStamp mTimeStamp;
  nsString mResponseText;
  nsCString mStatusText;
  nsresult mStatus;
  PRInt32 mReadyState;
  PRPackedBool mBubbles;
  PRPackedBool mCancelable;
  PRPackedBool mUploadEvent;
};

nsDOMWorkerXHREvent::nsDOMWorkerXHREvent(nsDOMWorkerXHRProxy* aXHRProxy)
: mXHRProxy(aXHRProxy),
  mType(PR_UINT32_MAX),
  mEventPhase(0),
  mTimeStamp(0),
  mStatus(NS_OK),
  mReadyState(0),
  mBubbles(PR_FALSE),
  mCancelable(PR_FALSE),
  mUploadEvent(PR_FALSE)
{
  NS_ASSERTION(aXHRProxy, "Can't be null!");

  nsIDOMEventTarget* target =
    static_cast<nsIDOMEventTarget*>(aXHRProxy->mWorkerXHR);
  mTarget = do_QueryInterface(target);
  NS_ASSERTION(mTarget, "Must support nsIDOMEventTarget!");
}

NS_IMPL_ISUPPORTS_INHERITED2(nsDOMWorkerXHREvent, nsRunnable,
                                                  nsIDOMEvent,
                                                  nsIClassInfo)

NS_IMPL_CI_INTERFACE_GETTER1(nsDOMWorkerXHREvent, nsIDOMEvent)

NS_IMPL_THREADSAFE_DOM_CI(nsDOMWorkerXHREvent)

nsresult
nsDOMWorkerXHREvent::Init(nsIDOMEvent* aEvent)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aEvent, "Don't pass null here!");

  nsresult rv = aEvent->GetType(mTypeString);
  NS_ENSURE_SUCCESS(rv, rv);

  mType = nsDOMWorkerXHREventTarget::GetListenerTypeFromString(mTypeString);
  if (mType >= MAX_XHR_LISTENER_TYPE) {
    NS_ERROR("Shouldn't get this type of event!");
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIDOMEventTarget> target;
  rv = aEvent->GetTarget(getter_AddRefs(target));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIXMLHttpRequestUpload> upload(do_QueryInterface(target));
  mUploadEvent = !!upload;

  PRBool boolVal;
  rv = aEvent->GetBubbles(&boolVal);
  NS_ENSURE_SUCCESS(rv, rv);
  mBubbles = boolVal ? PR_TRUE : PR_FALSE;

  rv = aEvent->GetCancelable(&boolVal);
  NS_ENSURE_SUCCESS(rv, rv);
  mCancelable = boolVal ? PR_TRUE : PR_FALSE;

  rv = aEvent->GetTimeStamp(&mTimeStamp);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = Init(mXHRProxy->mXHR);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


nsresult
nsDOMWorkerXHREvent::Init(nsIXMLHttpRequest* aXHR)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aXHR, "Don't pass null here!");

  nsresult rv = aXHR->GetResponseText(mResponseText);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aXHR->GetStatusText(mStatusText);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aXHR->GetStatus(&mStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aXHR->GetReadyState(&mReadyState);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
nsDOMWorkerXHREvent::EventHandled()
{
  mXHRProxy = nsnull;
}

NS_IMETHODIMP
nsDOMWorkerXHREvent::Run()
{
  nsresult rv = mXHRProxy->HandleWorkerEvent(this, mUploadEvent);

  EventHandled();

  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREvent::GetType(nsAString& aType)
{
  aType.Assign(mTypeString);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREvent::GetTarget(nsIDOMEventTarget** aTarget)
{
  NS_ENSURE_ARG_POINTER(aTarget);
  NS_ADDREF(*aTarget = mTarget);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREvent::GetCurrentTarget(nsIDOMEventTarget** aCurrentTarget)
{
  NS_ENSURE_ARG_POINTER(aCurrentTarget);
  NS_ADDREF(*aCurrentTarget = mTarget);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREvent::GetEventPhase(PRUint16* aEventPhase)
{
  NS_ENSURE_ARG_POINTER(aEventPhase);
  *aEventPhase = mEventPhase;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREvent::GetBubbles(PRBool* aBubbles)
{
  NS_ENSURE_ARG_POINTER(aBubbles);
  *aBubbles = mBubbles;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREvent::GetCancelable(PRBool* aCancelable)
{
  NS_ENSURE_ARG_POINTER(aCancelable);
  *aCancelable = mCancelable;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREvent::GetTimeStamp(DOMTimeStamp* aTimeStamp)
{
  NS_ENSURE_ARG_POINTER(aTimeStamp);
  *aTimeStamp = mTimeStamp;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREvent::StopPropagation()
{
  NS_WARNING("StopPropagation doesn't do anything here!");
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREvent::PreventDefault()
{
  NS_WARNING("PreventDefault doesn't do anything yet!");
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREvent::InitEvent(const nsAString& aEventTypeArg,
                               PRBool aCanBubbleArg,
                               PRBool aCancelableArg)
{
  NS_WARNING("InitEvent doesn't do anything here!");
  return NS_OK;
}

class nsDOMWorkerXHRWrappedListener : public nsIDOMEventListener
{
public:
  NS_DECL_ISUPPORTS

  nsDOMWorkerXHRWrappedListener(nsIDOMEventListener* aInner)
  : mInner(aInner) {
    NS_ASSERTION(aInner, "Null pointer!");
  }

  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) {
    return mInner->HandleEvent(aEvent);
  }

  nsIDOMEventListener* Inner() {
    return mInner;
  }

private:
  nsCOMPtr<nsIDOMEventListener> mInner;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDOMWorkerXHRWrappedListener,
                              nsIDOMEventListener)

nsDOMWorkerXHRProxy::nsDOMWorkerXHRProxy(nsDOMWorkerXHR* aWorkerXHR)
: mWorkerXHR(aWorkerXHR),
  mXHR(nsnull),
  mConcreteXHR(nsnull),
  mUpload(nsnull),
  mOwnedByXHR(PR_FALSE),
  mMultipart(PR_FALSE),
  mCanceled(PR_FALSE)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(MAX_XHR_LISTENER_TYPE >= MAX_UPLOAD_LISTENER_TYPE,
               "Upload should support a subset of XHR's event types!");
}

nsDOMWorkerXHRProxy::~nsDOMWorkerXHRProxy()
{
  if (mOwnedByXHR) {
    mWorkerXHR->Release();
  }
  else if (mUpload) {
    nsCOMPtr<nsIThread> mainThread;
    NS_GetMainThread(getter_AddRefs(mainThread));
    NS_ASSERTION(mainThread, "This isn't supposed to fail!");

    // This will release immediately if we're on the main thread.
    NS_ProxyRelease(mainThread, mUpload);
    mUpload = nsnull;
  }
}

NS_IMPL_ISUPPORTS_INHERITED2(nsDOMWorkerXHRProxy, nsRunnable,
                                                  nsIDOMEventListener,
                                                  nsIRequestObserver)

nsresult
nsDOMWorkerXHRProxy::Init()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  NS_ENSURE_FALSE(mXHR, NS_ERROR_ALREADY_INITIALIZED);

  mXHRListeners.SetLength(MAX_XHR_LISTENER_TYPE);
  mXHROnXListeners.SetLength(MAX_XHR_LISTENER_TYPE);

  mUploadListeners.SetLength(MAX_UPLOAD_LISTENER_TYPE);
  mUploadOnXListeners.SetLength(MAX_UPLOAD_LISTENER_TYPE);

  mMainThread = do_GetMainThread();
  NS_ENSURE_TRUE(mMainThread, NS_ERROR_UNEXPECTED);

  nsRefPtr<nsResultReturningRunnable> runnable =
    new nsResultReturningRunnable(mMainThread, this, mWorkerXHR->mWorker);
  NS_ENSURE_TRUE(runnable, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = runnable->Dispatch();
  if (NS_FAILED(rv)) {
    // Only warn if we didn't get canceled.
    NS_WARN_IF_FALSE(rv == NS_ERROR_ABORT, "Dispatch failed!");
    return rv;
  }

  return NS_OK;
}

nsIXMLHttpRequest*
nsDOMWorkerXHRProxy::GetXMLHttpRequest()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  return mCanceled ? nsnull : mXHR;
}

nsresult
nsDOMWorkerXHRProxy::Destroy()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  mCanceled = PR_TRUE;

  ClearEventListeners();
  mLastXHREvent = nsnull;

  if (mUpload) {
    DestroyInternal();
  }

  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::InitInternal()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!mXHR, "InitInternal shouldn't be called twice!");

  nsDOMWorkerThread* worker = mWorkerXHR->mWorker;
  nsRefPtr<nsDOMWorkerPool> pool = worker->Pool();

  if (worker->IsCanceled()) {
    return NS_ERROR_ABORT;
  }

  nsIPrincipal* nodePrincipal = pool->ParentDocument()->NodePrincipal();
  nsIScriptContext* scriptContext = pool->ScriptContext();
  NS_ASSERTION(nodePrincipal && scriptContext, "Shouldn't be null!");

  nsRefPtr<nsXMLHttpRequest> xhrConcrete = new nsXMLHttpRequest();
  NS_ENSURE_TRUE(xhrConcrete, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = xhrConcrete->Init(nodePrincipal, scriptContext, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  // Call QI manually here to avoid keeping up with the cast madness of
  // nsXMLHttpRequest.
  nsCOMPtr<nsIXMLHttpRequest> xhr =
    do_QueryInterface(static_cast<nsIXMLHttpRequest*>(xhrConcrete));
  NS_ENSURE_TRUE(xhr, NS_ERROR_NO_INTERFACE);

  nsCOMPtr<nsIXMLHttpRequestUpload> upload;
  rv = xhr->GetUpload(getter_AddRefs(upload));
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<nsDOMWorkerXHREvent> nullEvent = new nsDOMWorkerXHREvent(this);
  NS_ENSURE_TRUE(nullEvent, NS_ERROR_OUT_OF_MEMORY);

  rv = nullEvent->Init(xhr);
  NS_ENSURE_SUCCESS(rv, rv);

  nullEvent->EventHandled();
  mLastXHREvent.swap(nullEvent);

  // We now own upload and it owns xhr.
  upload.swap(mUpload);

  // Weak.
  mXHR = xhr;

  xhrConcrete->SetRequestObserver(this);

  // Weak.
  mConcreteXHR = xhrConcrete;

  return NS_OK;
}

void
nsDOMWorkerXHRProxy::DestroyInternal()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<nsDOMWorkerXHRProxy> kungFuDeathGrip;

  if (mOwnedByXHR) {
    kungFuDeathGrip = this;
    mXHR->Abort();
  }

  // mUpload could be null if Init fails.
  if (mUpload) {
    mConcreteXHR->SetRequestObserver(nsnull);
    mUpload->Release();
    mUpload = nsnull;
    mXHR = nsnull;
  }
}

void
nsDOMWorkerXHRProxy::FlipOwnership()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // Flip!
  mOwnedByXHR = !mOwnedByXHR;

  nsCOMPtr<nsIDOMEventTarget> xhrTarget(do_QueryInterface(mXHR));
  NS_ASSERTION(xhrTarget, "This shouldn't fail!");

  nsCOMPtr<nsIDOMEventTarget> uploadTarget(do_QueryInterface(mUpload));
  NS_ASSERTION(uploadTarget, "This shouldn't fail!");

  // If mWorkerXHR has no outstanding refs from JS then we are about to die.
  // Hold an extra ref here to make sure that we live through this call.
  nsRefPtr<nsDOMWorkerXHRProxy> kungFuDeathGrip(this);

  AddRemoveFunction addRemoveEventListener = mOwnedByXHR ?
    &nsIDOMEventTarget::AddEventListener :
    &nsIDOMEventTarget::RemoveEventListener;

  nsAutoString eventName;
  PRUint32 index = 0;

  for (; index < MAX_UPLOAD_LISTENER_TYPE; index++) {
    eventName.AssignASCII(nsDOMWorkerXHREventTarget::sListenerTypes[index]);
    (xhrTarget->*addRemoveEventListener)(eventName, this, PR_FALSE);
    (uploadTarget->*addRemoveEventListener)(eventName, this, PR_FALSE);
  }

  for (; index < MAX_XHR_LISTENER_TYPE; index++) {
    eventName.AssignASCII(nsDOMWorkerXHREventTarget::sListenerTypes[index]);
    (xhrTarget->*addRemoveEventListener)(eventName, this, PR_FALSE);
  }

  if (mOwnedByXHR) {
    mWorkerXHR->AddRef();
    mUpload->Release();
  }
  else {
    mUpload->AddRef();
    mWorkerXHR->Release();
  }
}

nsresult
nsDOMWorkerXHRProxy::AddEventListener(PRUint32 aType,
                                      nsIDOMEventListener* aListener,
                                      PRBool aOnXListener,
                                      PRBool aUploadListener)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  if ((aUploadListener && aType >= MAX_UPLOAD_LISTENER_TYPE) ||
      aType >= MAX_XHR_LISTENER_TYPE) {
    // Silently fail on junk events.
    return NS_OK;
  }

  ListenerArray& listeners = aUploadListener ? mUploadListeners[aType] :
                                               mXHRListeners[aType];
  WrappedListener& onXListener = aUploadListener ? mUploadOnXListeners[aType] :
                                                   mXHROnXListeners[aType];

  nsAutoLock lock(mWorkerXHR->Lock());

#ifdef DEBUG
  if (!aListener) {
    NS_ASSERTION(aOnXListener, "Shouldn't pass a null listener!");
  }
#endif

  if (aOnXListener) {
    // Remove the old one from the array if it exists.
    if (onXListener) {
#ifdef DEBUG
      PRBool removed =
#endif
      listeners.RemoveElement(onXListener);
      NS_ASSERTION(removed, "Should still be in the array!");
    }

    if (!aListener) {
      onXListener = nsnull;
      return NS_OK;
    }

    onXListener = new nsDOMWorkerXHRWrappedListener(aListener);
    NS_ENSURE_TRUE(onXListener, NS_ERROR_OUT_OF_MEMORY);

    aListener = onXListener;
  }

  Listener* added = listeners.AppendElement(aListener);
  NS_ENSURE_TRUE(added, NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::RemoveEventListener(PRUint32 aType,
                                         nsIDOMEventListener* aListener,
                                         PRBool aUploadListener)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aListener, "Null pointer!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  if ((aUploadListener && aType >= MAX_UPLOAD_LISTENER_TYPE) ||
      aType >= MAX_XHR_LISTENER_TYPE) {
    // Silently fail on junk events.
    return NS_OK;
  }

  ListenerArray& listeners = aUploadListener ? mUploadListeners[aType] :
                                               mXHRListeners[aType];

  nsAutoLock lock(mWorkerXHR->Lock());

  listeners.RemoveElement(aListener);

  return NS_OK;
}

already_AddRefed<nsIDOMEventListener>
nsDOMWorkerXHRProxy::GetOnXListener(PRUint32 aType,
                                    PRBool aUploadListener)
{
  if (mCanceled) {
    return nsnull;
  }

  if ((aUploadListener && aType >= MAX_UPLOAD_LISTENER_TYPE) ||
      aType >= MAX_XHR_LISTENER_TYPE) {
    // Silently fail on junk events.
    return nsnull;
  }

  WrappedListener& onXListener = aUploadListener ? mUploadOnXListeners[aType] :
                                                   mXHROnXListeners[aType];

  nsAutoLock lock(mWorkerXHR->Lock());

  nsCOMPtr<nsIDOMEventListener> listener = onXListener->Inner();
  return listener.forget();
}

nsresult
nsDOMWorkerXHRProxy::HandleWorkerEvent(nsDOMWorkerXHREvent* aEvent,
                                       PRBool aUploadEvent)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aEvent, "Should not be null!");

  mLastXHREvent = aEvent;

  nsresult rv = HandleEventInternal(aEvent->mType, aEvent, aUploadEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::HandleWorkerEvent(nsIDOMEvent* aEvent,
                                       PRBool aUploadEvent)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aEvent, "Should not be null!");

  nsString typeString;
  nsresult rv = aEvent->GetType(typeString);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 maxType = aUploadEvent ? MAX_UPLOAD_LISTENER_TYPE :
                                    MAX_XHR_LISTENER_TYPE;

  PRUint32 type =
    nsDOMWorkerXHREventTarget::GetListenerTypeFromString(typeString);
  if (type >= maxType) {
    // Silently fail on junk events.
    return NS_OK;
  }

  rv = HandleEventInternal(type, aEvent, aUploadEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::HandleEventInternal(PRUint32 aType,
                                         nsIDOMEvent* aEvent,
                                         PRBool aUploadListener)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aEvent, "Should not be null!");

#ifdef DEBUG
  if (aUploadListener) {
    NS_ASSERTION(aType < MAX_UPLOAD_LISTENER_TYPE, "Bad type!");
  }
  else {
    NS_ASSERTION(aType < MAX_XHR_LISTENER_TYPE, "Bad type!");
  }
#endif

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  ListenerArray& listeners = aUploadListener ? mUploadListeners[aType] :
                                               mXHRListeners[aType];

  nsAutoTArray<Listener, 10> listenerCopy;
  PRUint32 count;

  {
    nsAutoLock lock(mWorkerXHR->Lock());

    count = listeners.Length();
    if (!count) {
      return NS_OK;
    }

    Listener* copied = listenerCopy.AppendElements(listeners);
    NS_ENSURE_TRUE(copied, NS_ERROR_OUT_OF_MEMORY);
  }

  for (PRUint32 index = 0; index < count; index++) {
    NS_ASSERTION(listenerCopy[index], "Null listener?!");
    listenerCopy[index]->HandleEvent(aEvent);
  }

  return NS_OK;
}

void
nsDOMWorkerXHRProxy::ClearEventListeners()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsTArray<ListenerArray> doomedArrays;
  PRBool ok = doomedArrays.SetLength(MAX_XHR_LISTENER_TYPE +
                                     MAX_UPLOAD_LISTENER_TYPE);
  NS_ENSURE_TRUE(ok,);

  nsTArray<WrappedListener> doomedListeners;
  ok = doomedListeners.SetLength(MAX_XHR_LISTENER_TYPE +
                                 MAX_UPLOAD_LISTENER_TYPE);
  NS_ENSURE_TRUE(ok,);

  {
    PRUint32 listenerIndex, doomedIndex;

    nsAutoLock lock(mWorkerXHR->Lock());

    for (listenerIndex = 0, doomedIndex = 0;
         listenerIndex < MAX_UPLOAD_LISTENER_TYPE;
         listenerIndex++, doomedIndex++) {
      mUploadListeners[listenerIndex].
        SwapElements(doomedArrays[doomedIndex]);
      mUploadOnXListeners[listenerIndex].
        swap(doomedListeners[doomedIndex]);
    }

    for (listenerIndex = 0;
         listenerIndex < MAX_XHR_LISTENER_TYPE;
         listenerIndex++, doomedIndex++) {
      mXHRListeners[listenerIndex].
        SwapElements(doomedArrays[doomedIndex]);
      mXHROnXListeners[listenerIndex].
        swap(doomedListeners[doomedIndex]);
    }
  }

  // Destructors for the nsTArrays actually kill the listeners outside of the
  // lock.
}

// nsIDOMEventListener
NS_IMETHODIMP
nsDOMWorkerXHRProxy::HandleEvent(nsIDOMEvent* aEvent)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  NS_ENSURE_ARG_POINTER(aEvent);

  nsRefPtr<nsDOMWorkerXHREvent> newEvent = new nsDOMWorkerXHREvent(this);
  NS_ENSURE_TRUE(newEvent, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = newEvent->Init(aEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nsDOMThreadService::get()->Dispatch(mWorkerXHR->mWorker, newEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::Abort()
{
  if (NS_IsMainThread()) {
    if (mCanceled) {
      return NS_ERROR_ABORT;
    }

    nsCOMPtr<nsIXMLHttpRequest> xhr = mXHR;
    if (mOwnedByXHR) {
      FlipOwnership();
    }

    return xhr->Abort();
  }

  RUN_PROXIED_FUNCTION(Abort, (this));
  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::GetAllResponseHeaders(char** _retval)
{
  RUN_PROXIED_FUNCTION(GetAllResponseHeaders, (this, _retval));
  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::GetResponseHeader(const nsACString& aHeader,
                                       nsACString& _retval)
{
  RUN_PROXIED_FUNCTION(GetResponseHeader, (this, aHeader, _retval));
  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::OpenRequest(const nsACString& aMethod,
                                 const nsACString& aUrl,
                                 PRBool aAsync,
                                 const nsAString& aUser,
                                 const nsAString& aPassword)
{
  RUN_PROXIED_FUNCTION(OpenRequest, (this, aMethod, aUrl, aAsync, aUser,
                                     aPassword));
  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::Send(nsIVariant* aBody)
{
  RUN_PROXIED_FUNCTION(Send, (this, aBody));
  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::SendAsBinary(const nsAString& aBody)
{
  RUN_PROXIED_FUNCTION(SendAsBinary, (this, aBody));
  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::SetRequestHeader(const nsACString& aHeader,
                                      const nsACString& aValue)
{
  RUN_PROXIED_FUNCTION(SetRequestHeader, (this, aHeader, aValue));
  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::OverrideMimeType(const nsACString& aMimetype)
{
  RUN_PROXIED_FUNCTION(OverrideMimeType, (this, aMimetype));
  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::GetMultipart(PRBool* aMultipart)
{
  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  *aMultipart = mMultipart;
  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::SetMultipart(PRBool aMultipart)
{
  RUN_PROXIED_FUNCTION(SetMultipart, (this, aMultipart));
  mMultipart = aMultipart;
  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::GetResponseText(nsAString& _retval)
{
  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  _retval.Assign(mLastXHREvent->mResponseText);
  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::GetStatusText(nsACString& _retval)
{
  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  _retval.Assign(mLastXHREvent->mStatusText);
  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::GetStatus(nsresult* _retval)
{
  NS_ASSERTION(_retval, "Null pointer!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  *_retval = mLastXHREvent->mStatus;
  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::GetReadyState(PRInt32* _retval)
{
  NS_ASSERTION(_retval, "Null pointer!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  *_retval = mLastXHREvent->mReadyState;
  return NS_OK;
}

// nsIRunnable
NS_IMETHODIMP
nsDOMWorkerXHRProxy::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!mUpload, "Run twice?!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  nsresult rv = InitInternal();
  if (NS_FAILED(rv)) {
    NS_WARNING("InitInternal failed!");
    DestroyInternal();
    return rv;
  }

  return NS_OK;
}

// nsIRequestObserver
NS_IMETHODIMP
nsDOMWorkerXHRProxy::OnStartRequest(nsIRequest* /* aRequest */,
                                    nsISupports* /* aContext */)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_OK;
  }

  NS_ASSERTION(!mOwnedByXHR, "Inconsistent state!");

  FlipOwnership();

  return NS_OK;
}

// nsIRequestObserver
NS_IMETHODIMP
nsDOMWorkerXHRProxy::OnStopRequest(nsIRequest* /* aRequest */,
                                   nsISupports* /* aContext */,
                                   nsresult /* aStatus */)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_OK;
  }

  NS_ASSERTION(mOwnedByXHR, "Inconsistent state!");

  FlipOwnership();

  return NS_OK;
}
