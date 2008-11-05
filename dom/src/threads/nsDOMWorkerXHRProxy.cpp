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
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMProgressEvent.h"
#include "nsILoadGroup.h"
#include "nsIRequest.h"
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

#define RUN_PROXIED_FUNCTION(_name, _args)                                     \
  PR_BEGIN_MACRO                                                               \
    NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");                         \
                                                                               \
    if (mCanceled) {                                                           \
      return NS_ERROR_ABORT;                                                   \
    }                                                                          \
                                                                               \
    SyncEventQueue queue;                                                      \
                                                                               \
    nsRefPtr< :: _name> method = new :: _name _args;                           \
    NS_ENSURE_TRUE(method, NS_ERROR_OUT_OF_MEMORY);                            \
                                                                               \
    method->Init(this, &queue);                                                \
                                                                               \
    nsRefPtr<nsResultReturningRunnable> runnable =                             \
      new nsResultReturningRunnable(mMainThread, method, mWorkerXHR->mWorker); \
    NS_ENSURE_TRUE(runnable, NS_ERROR_OUT_OF_MEMORY);                          \
                                                                               \
    nsresult _rv = runnable->Dispatch();                                       \
                                                                               \
    if (mCanceled) {                                                           \
      return NS_ERROR_ABORT;                                                   \
    }                                                                          \
                                                                               \
    PRUint32 queueLength = queue.Length();                                     \
    for (PRUint32 index = 0; index < queueLength; index++) {                   \
      queue[index]->Run();                                                     \
    }                                                                          \
                                                                               \
    if (NS_FAILED(_rv)) {                                                      \
      return _rv;                                                              \
    }                                                                          \
  PR_END_MACRO

using namespace nsDOMWorkerProxiedXHRFunctions;

class nsResultReturningRunnable : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS

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

NS_IMPL_THREADSAFE_ISUPPORTS1(nsResultReturningRunnable, nsIRunnable)

class nsDOMWorkerXHREvent : public nsIRunnable,
                            public nsIDOMProgressEvent,
                            public nsIClassInfo
{
  friend class nsDOMWorkerXHRProxy;
  friend class nsDOMWorkerXHREventTargetProxy;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIDOMEVENT
  NS_DECL_NSIDOMPROGRESSEVENT
  NS_DECL_NSICLASSINFO

  nsDOMWorkerXHREvent(nsDOMWorkerXHRProxy* aXHRProxy);

  nsresult Init(PRUint32 aType,
                const nsAString& aTypeString,
                nsIDOMEvent* aEvent);

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
  PRUint64 mLoaded;
  PRUint64 mTotal;
  PRInt32 mChannelID;
  PRPackedBool mBubbles;
  PRPackedBool mCancelable;
  PRPackedBool mUploadEvent;
  PRPackedBool mProgressEvent;
  PRPackedBool mLengthComputable;
};

nsDOMWorkerXHREvent::nsDOMWorkerXHREvent(nsDOMWorkerXHRProxy* aXHRProxy)
: mXHRProxy(aXHRProxy),
  mType(PR_UINT32_MAX),
  mEventPhase(0),
  mTimeStamp(0),
  mStatus(NS_OK),
  mReadyState(0),
  mLoaded(0),
  mTotal(0),
  mChannelID(-1),
  mBubbles(PR_FALSE),
  mCancelable(PR_FALSE),
  mUploadEvent(PR_FALSE),
  mProgressEvent(PR_FALSE),
  mLengthComputable(PR_FALSE)
{
  NS_ASSERTION(aXHRProxy, "Can't be null!");
}

NS_IMPL_THREADSAFE_ADDREF(nsDOMWorkerXHREvent)
NS_IMPL_THREADSAFE_RELEASE(nsDOMWorkerXHREvent)

NS_INTERFACE_MAP_BEGIN(nsDOMWorkerXHREvent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMEvent)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEvent)
  NS_INTERFACE_MAP_ENTRY(nsIClassInfo)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIDOMProgressEvent, mProgressEvent)
NS_INTERFACE_MAP_END

NS_IMPL_CI_INTERFACE_GETTER1(nsDOMWorkerXHREvent, nsIDOMEvent)
NS_IMETHODIMP
nsDOMWorkerXHREvent::GetInterfaces(PRUint32* aCount,
                                   nsIID*** aArray)
{
  PRUint32 count = *aCount = mProgressEvent ? 2 : 1;

  *aArray = (nsIID**)nsMemory::Alloc(sizeof(nsIID*) * count);

  if (mProgressEvent) {
    (*aArray)[--count] =
      (nsIID*)nsMemory::Clone(&NS_GET_IID(nsIDOMProgressEvent), sizeof(nsIID));
  }

  (*aArray)[--count] =
    (nsIID *)nsMemory::Clone(&NS_GET_IID(nsIDOMEvent), sizeof(nsIID));

  NS_ASSERTION(!count, "Bad math!");
  return NS_OK;
}

NS_IMPL_THREADSAFE_DOM_CI_ALL_THE_REST(nsDOMWorkerXHREvent)

nsresult
nsDOMWorkerXHREvent::Init(PRUint32 aType,
                          const nsAString& aTypeString,
                          nsIDOMEvent* aEvent)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aEvent, "Don't pass null here!");

  nsresult rv;

  mType = aType;
  mTypeString.Assign(aTypeString);

  mChannelID = mXHRProxy->ChannelID();

  nsCOMPtr<nsIDOMProgressEvent> progressEvent(do_QueryInterface(aEvent));
  if (progressEvent) {
    mProgressEvent = PR_TRUE;

    PRBool lengthComputable;
    rv = progressEvent->GetLengthComputable(&lengthComputable);
    NS_ENSURE_SUCCESS(rv, rv);

    mLengthComputable = lengthComputable ? PR_TRUE : PR_FALSE;

    rv = progressEvent->GetLoaded(&mLoaded);
    NS_ENSURE_SUCCESS(rv, rv); 

    rv = progressEvent->GetTotal(&mTotal);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIDOMEventTarget> mainThreadTarget;
  rv = aEvent->GetTarget(getter_AddRefs(mainThreadTarget));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIXMLHttpRequestUpload> upload(do_QueryInterface(mainThreadTarget));
  if (upload) {
    mUploadEvent = PR_TRUE;
    mTarget =
      static_cast<nsDOMWorkerXHREventTarget*>(mXHRProxy->mWorkerXHR->mUpload);
  }
  else {
    mUploadEvent = PR_FALSE;
    mTarget = mXHRProxy->mWorkerXHR;
  }

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
  // Prevent reference cycles by releasing these here.
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

NS_IMETHODIMP
nsDOMWorkerXHREvent::GetLengthComputable(PRBool* aLengthComputable)
{
  NS_ENSURE_ARG_POINTER(aLengthComputable);
  *aLengthComputable = mLengthComputable;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREvent::GetLoaded(PRUint64* aLoaded)
{
  NS_ENSURE_ARG_POINTER(aLoaded);
  *aLoaded = mLoaded;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREvent::GetTotal(PRUint64* aTotal)
{
  NS_ENSURE_ARG_POINTER(aTotal);
  *aTotal = mTotal;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREvent::InitProgressEvent(const nsAString_internal& aTypeArg,
                                       PRBool aCanBubbleArg,
                                       PRBool aCancelableArg,
                                       PRBool aLengthComputableArg,
                                       PRUint64 aLoadedArg,
                                       PRUint64 aTotalArg)
{
  NS_WARNING("InitProgressEvent doesn't do anything here!");
  return NS_OK;
}

class nsDOMWorkerXHRLastProgressOrLoadEvent : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS

  nsDOMWorkerXHRLastProgressOrLoadEvent(nsDOMWorkerXHRProxy* aProxy)
  : mProxy(aProxy) {
    NS_ASSERTION(aProxy, "Null pointer!");
  }

  NS_IMETHOD Run() {
    nsRefPtr<nsDOMWorkerXHREvent> lastProgressOrLoadEvent;

    if (!mProxy->mCanceled) {
      nsAutoLock lock(mProxy->mWorkerXHR->Lock());
      mProxy->mLastProgressOrLoadEvent.swap(lastProgressOrLoadEvent);
    }

    if (lastProgressOrLoadEvent) {
      return lastProgressOrLoadEvent->Run();
    }

    return NS_OK;
  }

private:
  nsRefPtr<nsDOMWorkerXHRProxy> mProxy;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDOMWorkerXHRLastProgressOrLoadEvent,
                              nsIRunnable)

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

class nsDOMWorkerXHRAttachUploadListenersRunnable : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS

  nsDOMWorkerXHRAttachUploadListenersRunnable(nsDOMWorkerXHRProxy* aProxy)
  : mProxy(aProxy) {
    NS_ASSERTION(aProxy, "Null pointer!");
  }

  NS_IMETHOD Run() {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

    NS_ASSERTION(!mProxy->mWantUploadListeners, "Huh?!");

    nsCOMPtr<nsIDOMEventTarget> upload(do_QueryInterface(mProxy->mUpload));
    NS_ASSERTION(upload, "This shouldn't fail!");

    nsAutoString eventName;
    for (PRUint32 index = 0; index < MAX_UPLOAD_LISTENER_TYPE; index++) {
      eventName.AssignASCII(nsDOMWorkerXHREventTarget::sListenerTypes[index]);
      upload->AddEventListener(eventName, mProxy, PR_FALSE);
    }

    mProxy->mWantUploadListeners = PR_TRUE;
    return NS_OK;
  }

private:
  nsRefPtr<nsDOMWorkerXHRProxy> mProxy;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDOMWorkerXHRAttachUploadListenersRunnable,
                              nsIRunnable)

class nsDOMWorkerXHRFinishSyncXHRRunnable : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS

  nsDOMWorkerXHRFinishSyncXHRRunnable(nsDOMWorkerXHRProxy* aProxy,
                                      nsIThread* aTarget)
  : mProxy(aProxy), mTarget(aTarget) {
    NS_ASSERTION(aProxy, "Null pointer!");
    NS_ASSERTION(aTarget, "Null pointer!");
  }

  NS_IMETHOD Run() {
    nsCOMPtr<nsIThread> thread;
    mProxy->mSyncXHRThread.swap(thread);
    mProxy = nsnull;

    NS_ASSERTION(thread, "Should have a thread here!");
    NS_ASSERTION(!NS_IsMainThread() && thread == NS_GetCurrentThread(),
                 "Wrong thread?!");

    return NS_ProcessPendingEvents(thread);
  }

  nsresult Dispatch() {
    nsresult rv = mTarget->Dispatch(this, NS_DISPATCH_NORMAL);
    mTarget = nsnull;
    return rv;
  }

private:
  nsRefPtr<nsDOMWorkerXHRProxy> mProxy;
  nsCOMPtr<nsIThread> mTarget;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDOMWorkerXHRFinishSyncXHRRunnable, nsIRunnable)

nsDOMWorkerXHRProxy::nsDOMWorkerXHRProxy(nsDOMWorkerXHR* aWorkerXHR)
: mWorkerXHR(aWorkerXHR),
  mXHR(nsnull),
  mConcreteXHR(nsnull),
  mUpload(nsnull),
  mSyncEventQueue(nsnull),
  mChannelID(-1),
  mOwnedByXHR(PR_FALSE),
  mWantUploadListeners(PR_FALSE),
  mCanceled(PR_FALSE),
  mSyncRequest(PR_FALSE)
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
  else if (mXHR) {
    nsCOMPtr<nsIThread> mainThread;
    NS_GetMainThread(getter_AddRefs(mainThread));
    NS_ASSERTION(mainThread, "This isn't supposed to fail!");

    // This will release immediately if we're on the main thread.
    NS_ProxyRelease(mainThread, mXHR);
  }
}

NS_IMPL_THREADSAFE_ISUPPORTS3(nsDOMWorkerXHRProxy, nsIRunnable,
                                                   nsIDOMEventListener,
                                                   nsIRequestObserver)

nsresult
nsDOMWorkerXHRProxy::Init()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  NS_ENSURE_FALSE(mXHR, NS_ERROR_ALREADY_INITIALIZED);

  PRBool success = mXHRListeners.SetLength(MAX_XHR_LISTENER_TYPE);
  NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);

  success = mXHROnXListeners.SetLength(MAX_XHR_LISTENER_TYPE);
  NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);

  success = mUploadListeners.SetLength(MAX_UPLOAD_LISTENER_TYPE);
  NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);

  success = mUploadOnXListeners.SetLength(MAX_UPLOAD_LISTENER_TYPE);
  NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);

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

  {
    nsAutoLock lock(mWorkerXHR->Lock());
    mLastProgressOrLoadEvent = nsnull;
  }

  if (mXHR) {
    DestroyInternal();
  }

  mLastXHREvent = nsnull;

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
  mLastXHREvent.swap(nullEvent);

  mLastXHREvent->EventHandled();

  xhrConcrete->SetRequestObserver(this);

  // We now own mXHR and it owns upload.
  xhr.swap(mXHR);

  // Weak refs.
  mUpload = upload;
  mConcreteXHR = xhrConcrete;

  AddRemoveXHRListeners(PR_TRUE);

  return NS_OK;
}

void
nsDOMWorkerXHRProxy::DestroyInternal()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<nsDOMWorkerXHRProxy> kungFuDeathGrip(this);

  if (mConcreteXHR) {
    mConcreteXHR->SetRequestObserver(nsnull);
  }

  if (mOwnedByXHR) {
    mXHR->Abort();
  }
  else {
    // There's a slight chance that Send was called yet we've canceled before
    // necko has fired its OnStartRequest notification. Guard against that here.
    nsRefPtr<nsDOMWorkerXHRFinishSyncXHRRunnable> syncFinishedRunnable;
    {
      nsAutoLock lock(mWorkerXHR->Lock());
      mSyncFinishedRunnable.swap(syncFinishedRunnable);
    }

    if (syncFinishedRunnable) {
      syncFinishedRunnable->Dispatch();
    }
  }

  NS_ASSERTION(!mOwnedByXHR, "Should have flipped already!");
  NS_ASSERTION(!mSyncFinishedRunnable, "Should have fired this already!");
  NS_ASSERTION(!mLastProgressOrLoadEvent, "Should have killed this already!");

  // mXHR could be null if Init fails.
  if (mXHR) {
    AddRemoveXHRListeners(PR_FALSE);

    mXHR->Release();
    mXHR = nsnull;

    mUpload = nsnull;
  }
}

void
nsDOMWorkerXHRProxy::AddRemoveXHRListeners(PRBool aAdd)
{
  nsCOMPtr<nsIDOMEventTarget> xhrTarget(do_QueryInterface(mXHR));
  NS_ASSERTION(xhrTarget, "This shouldn't fail!");

  EventListenerFunction function = aAdd ?
                                   &nsIDOMEventTarget::AddEventListener :
                                   &nsIDOMEventTarget::RemoveEventListener;

  nsAutoString eventName;
  PRUint32 index = 0;

  if (mWantUploadListeners) {
    nsCOMPtr<nsIDOMEventTarget> uploadTarget(do_QueryInterface(mUpload));
    NS_ASSERTION(uploadTarget, "This shouldn't fail!");

    for (; index < MAX_UPLOAD_LISTENER_TYPE; index++) {
      eventName.AssignASCII(nsDOMWorkerXHREventTarget::sListenerTypes[index]);
      (xhrTarget.get()->*function)(eventName, this, PR_FALSE);
      (uploadTarget.get()->*function)(eventName, this, PR_FALSE);
    }
  }

  for (; index < MAX_XHR_LISTENER_TYPE; index++) {
    eventName.AssignASCII(nsDOMWorkerXHREventTarget::sListenerTypes[index]);
    (xhrTarget.get()->*function)(eventName, this, PR_FALSE);
  }
}

void
nsDOMWorkerXHRProxy::FlipOwnership()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // Flip!
  mOwnedByXHR = !mOwnedByXHR;

  // If mWorkerXHR has no outstanding refs from JS then we are about to die.
  // Hold an extra ref here to make sure that we live through this call.
  nsRefPtr<nsDOMWorkerXHRProxy> kungFuDeathGrip(this);

  if (mOwnedByXHR) {
    mWorkerXHR->AddRef();
    mXHR->Release();
  }
  else {
    mXHR->AddRef();
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

  if ((aUploadListener && aType >= MAX_UPLOAD_LISTENER_TYPE) ||
      aType >= MAX_XHR_LISTENER_TYPE) {
    // Silently fail on junk events.
    return NS_OK;
  }

  ListenerArray& listeners = aUploadListener ? mUploadListeners[aType] :
                                               mXHRListeners[aType];
  WrappedListener& onXListener = aUploadListener ? mUploadOnXListeners[aType] :
                                                   mXHROnXListeners[aType];

  {
    nsAutoLock lock(mWorkerXHR->Lock());

    if (mCanceled) {
      return NS_ERROR_ABORT;
    }

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
  }

  // If this is the first time we're setting an upload listener then we have to
  // hit the main thread to attach the upload listeners.
  if (aUploadListener && aListener && !mWantUploadListeners) {
    nsRefPtr<nsDOMWorkerXHRAttachUploadListenersRunnable> attachRunnable =
      new nsDOMWorkerXHRAttachUploadListenersRunnable(this);
    NS_ENSURE_TRUE(attachRunnable, NS_ERROR_OUT_OF_MEMORY);

    nsRefPtr<nsResultReturningRunnable> runnable =
      new nsResultReturningRunnable(mMainThread, attachRunnable,
                                    mWorkerXHR->mWorker);
    NS_ENSURE_TRUE(runnable, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = runnable->Dispatch();
    if (NS_FAILED(rv)) {
      return rv;
    }

    NS_ASSERTION(mWantUploadListeners, "Should have set this!");
  }

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

  if (mCanceled ||
      (aEvent->mChannelID != -1 && aEvent->mChannelID != mChannelID)) {
    return NS_OK;
  }

  mLastXHREvent = aEvent;

  return HandleEventInternal(aEvent->mType, aEvent, aUploadEvent);
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

  return HandleEventInternal(type, aEvent, aUploadEvent);
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

PRBool
nsDOMWorkerXHRProxy::HasListenersForType(PRUint32 aType,
                                         nsIDOMEvent* aEvent)
{
  NS_ASSERTION(aType < MAX_XHR_LISTENER_TYPE, "Bad type!");

  if (mXHRListeners[aType].Length()) {
    return PR_TRUE;
  }

  PRBool checkUploadListeners = PR_FALSE;
  if (aEvent) {
    nsCOMPtr<nsIDOMEventTarget> target;
    if (NS_SUCCEEDED(aEvent->GetTarget(getter_AddRefs(target)))) {
      nsCOMPtr<nsIXMLHttpRequestUpload> upload(do_QueryInterface(target));
      checkUploadListeners = !!upload;
    }
  }
  else {
    checkUploadListeners = PR_TRUE;
  }

  if (checkUploadListeners && mUploadListeners[aType].Length()) {
    return PR_TRUE;
  }

  return PR_FALSE;
}

// nsIDOMEventListener
NS_IMETHODIMP
nsDOMWorkerXHRProxy::HandleEvent(nsIDOMEvent* aEvent)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  NS_ENSURE_ARG_POINTER(aEvent);

  nsAutoString typeString;
  nsresult rv = aEvent->GetType(typeString);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 type =
    nsDOMWorkerXHREventTarget::GetListenerTypeFromString(typeString);
  if (type >= MAX_XHR_LISTENER_TYPE) {
    return NS_OK;
  }

  // When Abort is called on nsXMLHttpRequest (either from a proxied Abort call
  // or from DestroyInternal) the OnStopRequest call is not run synchronously.
  // Thankfully an abort event *is* fired synchronously so we can flip our
  // ownership around and fire the sync finished runnable if we're running in
  // sync mode.
  if (type == LISTENER_TYPE_ABORT && mCanceled) {
    OnStopRequest(nsnull, nsnull, NS_ERROR_ABORT);
  }

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  if (!HasListenersForType(type, aEvent)) {
    return NS_OK;
  }

  nsRefPtr<nsDOMWorkerXHREvent> newEvent = new nsDOMWorkerXHREvent(this);
  NS_ENSURE_TRUE(newEvent, NS_ERROR_OUT_OF_MEMORY);

  rv = newEvent->Init(type, typeString, aEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRunnable> runnable(newEvent);

  if (type == LISTENER_TYPE_LOAD || type == LISTENER_TYPE_PROGRESS) {
    runnable = new nsDOMWorkerXHRLastProgressOrLoadEvent(this);
    NS_ENSURE_TRUE(runnable, NS_ERROR_OUT_OF_MEMORY);

    {
      nsAutoLock lock(mWorkerXHR->Lock());
      mLastProgressOrLoadEvent.swap(newEvent);

      if (newEvent) {
        // Already had a saved progress/load event so no need to generate
        // another. Bail out rather than dispatching runnable.
        return NS_OK;
      }
    }
  }

  if (mSyncEventQueue) {
    // If we're supposed to be capturing events for synchronous execution then
    // place this event in the queue.
    nsCOMPtr<nsIRunnable>* newElement =
      mSyncEventQueue->AppendElement(runnable);
    NS_ENSURE_TRUE(newElement, NS_ERROR_OUT_OF_MEMORY);
  }
  else if (mSyncXHRThread) {
    // If we're running a sync XHR then schedule the event immediately for the
    // worker's thread.
    rv = mSyncXHRThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    // Otherwise schedule it for the worker via the thread service.
    rv = nsDOMThreadService::get()->Dispatch(mWorkerXHR->mWorker, runnable);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::OpenRequest(const nsACString& aMethod,
                                 const nsACString& aUrl,
                                 PRBool aAsync,
                                 const nsAString& aUser,
                                 const nsAString& aPassword)
{
  if (!NS_IsMainThread()) {
    mSyncRequest = !aAsync;

    // Always do async behind the scenes!
    RUN_PROXIED_FUNCTION(OpenRequest,
                         (aMethod, aUrl, PR_TRUE, aUser, aPassword));
    return NS_OK;
  }

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  nsresult rv = mXHR->OpenRequest(aMethod, aUrl, aAsync, aUser, aPassword);
  NS_ENSURE_SUCCESS(rv, rv);

  // Do this after OpenRequest is called so that we will continue to run events
  // from the old channel if OpenRequest fails. Any events generated by the
  // OpenRequest method will always run regardless of channel ID.
  mChannelID++;

  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::Abort()
{
  if (!NS_IsMainThread()) {
    RUN_PROXIED_FUNCTION(Abort, ());
    return NS_OK;
  }

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  nsCOMPtr<nsIXMLHttpRequest> xhr = mXHR;

  FlipOwnership();

  nsresult rv = xhr->Abort();
  NS_ENSURE_SUCCESS(rv, rv);

  // Don't allow further events from this channel.
  mChannelID++;

  return NS_OK;
}


nsDOMWorkerXHRProxy::SyncEventQueue*
nsDOMWorkerXHRProxy::SetSyncEventQueue(SyncEventQueue* aQueue)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  SyncEventQueue* oldQueue = mSyncEventQueue;
  mSyncEventQueue = aQueue;
  return oldQueue;
}

nsresult
nsDOMWorkerXHRProxy::GetAllResponseHeaders(char** _retval)
{
  RUN_PROXIED_FUNCTION(GetAllResponseHeaders, (_retval));
  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::GetResponseHeader(const nsACString& aHeader,
                                       nsACString& _retval)
{
  RUN_PROXIED_FUNCTION(GetResponseHeader, (aHeader, _retval));
  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::Send(nsIVariant* aBody)
{
  NS_ASSERTION(!mSyncXHRThread, "Shouldn't reenter here!");

  if (mSyncRequest) {

    mSyncXHRThread = NS_GetCurrentThread();
    NS_ENSURE_TRUE(mSyncXHRThread, NS_ERROR_FAILURE);

    nsAutoLock lock(mWorkerXHR->Lock());

    if (mCanceled) {
      return NS_ERROR_ABORT;
    }

    mSyncFinishedRunnable =
      new nsDOMWorkerXHRFinishSyncXHRRunnable(this, mSyncXHRThread);
    NS_ENSURE_TRUE(mSyncFinishedRunnable, NS_ERROR_FAILURE);
  }

  RUN_PROXIED_FUNCTION(Send, (aBody));

  return RunSyncEventLoop();
}

nsresult
nsDOMWorkerXHRProxy::SendAsBinary(const nsAString& aBody)
{
  NS_ASSERTION(!mSyncXHRThread, "Shouldn't reenter here!");

  if (mSyncRequest) {
    mSyncXHRThread = NS_GetCurrentThread();
    NS_ENSURE_TRUE(mSyncXHRThread, NS_ERROR_FAILURE);

    nsAutoLock lock(mWorkerXHR->Lock());

    if (mCanceled) {
      return NS_ERROR_ABORT;
    }

    mSyncFinishedRunnable =
      new nsDOMWorkerXHRFinishSyncXHRRunnable(this, mSyncXHRThread);
    NS_ENSURE_TRUE(mSyncFinishedRunnable, NS_ERROR_FAILURE);
  }

  RUN_PROXIED_FUNCTION(SendAsBinary, (aBody));

  return RunSyncEventLoop();
}

nsresult
nsDOMWorkerXHRProxy::SetRequestHeader(const nsACString& aHeader,
                                      const nsACString& aValue)
{
  RUN_PROXIED_FUNCTION(SetRequestHeader, (aHeader, aValue));
  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::OverrideMimeType(const nsACString& aMimetype)
{
  RUN_PROXIED_FUNCTION(OverrideMimeType, (aMimetype));
  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::GetMultipart(PRBool* aMultipart)
{
  NS_ASSERTION(aMultipart, "Null pointer!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  RUN_PROXIED_FUNCTION(GetMultipart, (aMultipart));
  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::SetMultipart(PRBool aMultipart)
{
  RUN_PROXIED_FUNCTION(SetMultipart, (aMultipart));
  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::GetWithCredentials(PRBool* aWithCredentials)
{
  RUN_PROXIED_FUNCTION(GetWithCredentials, (aWithCredentials));
  return NS_OK;
}

nsresult
nsDOMWorkerXHRProxy::SetWithCredentials(PRBool aWithCredentials)
{
  RUN_PROXIED_FUNCTION(SetWithCredentials, (aWithCredentials));
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

nsresult
nsDOMWorkerXHRProxy::RunSyncEventLoop()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (!mSyncRequest) {
    return NS_OK;
  }

  NS_ASSERTION(mSyncXHRThread == NS_GetCurrentThread(), "Wrong thread!");

  while (mSyncXHRThread) {
    if (NS_UNLIKELY(!NS_ProcessNextEvent(mSyncXHRThread))) {
      NS_ERROR("Something wrong here, this shouldn't fail!");
      return NS_ERROR_UNEXPECTED;
    }
  }

  NS_ASSERTION(!NS_HasPendingEvents(NS_GetCurrentThread()),
               "Unprocessed events remaining!");

  return NS_OK;
}

// nsIRunnable
NS_IMETHODIMP
nsDOMWorkerXHRProxy::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!mXHR, "Run twice?!");

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

  NS_ASSERTION(!mOwnedByXHR, "Inconsistent state!");

  if (mCanceled) {
    return NS_OK;
  }

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

  NS_ASSERTION(mOwnedByXHR, "Inconsistent state!");

  FlipOwnership();

  nsRefPtr<nsDOMWorkerXHRFinishSyncXHRRunnable> syncFinishedRunnable;
  {
    nsAutoLock lock(mWorkerXHR->Lock());
    mSyncFinishedRunnable.swap(syncFinishedRunnable);
  }

  if (syncFinishedRunnable) {
    nsresult rv = syncFinishedRunnable->Dispatch();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}
