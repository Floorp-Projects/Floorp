/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
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
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
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

#include "XMLHttpRequestPrivate.h"

#include "nsIDOMEvent.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMProgressEvent.h"
#include "nsIJSContextStack.h"
#include "nsIRunnable.h"
#include "nsIXMLHttpRequest.h"
#include "nsIXPConnect.h"

#include "jstypedarray.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsJSUtils.h"
#include "nsThreadUtils.h"
#include "nsXMLHttpRequest.h"

#include "Events.h"
#include "EventTarget.h"
#include "Exceptions.h"
#include "RuntimeService.h"
#include "XMLHttpRequest.h"

BEGIN_WORKERS_NAMESPACE
namespace xhr {

class Proxy : public nsIDOMEventListener
{
public:
  // Read on multiple threads.
  WorkerPrivate* mWorkerPrivate;
  XMLHttpRequestPrivate* mXMLHttpRequestPrivate;

  // Only touched on the main thread.
  nsRefPtr<nsXMLHttpRequest> mXHR;
  nsCOMPtr<nsIXMLHttpRequestUpload> mXHRUpload;
  PRUint32 mInnerChannelId;

  // Only touched on the worker thread.
  PRUint32 mOuterChannelId;
  PRUint64 mLastLoaded;
  PRUint64 mLastTotal;
  PRUint64 mLastUploadLoaded;
  PRUint64 mLastUploadTotal;
  bool mIsSyncXHR;
  bool mLastLengthComputable;
  bool mLastUploadLengthComputable;
  bool mSeenLoadStart;
  bool mSeenUploadLoadStart;

  // Only touched on the main thread.
  nsString mPreviousResponseText;
  nsCString mPreviousStatusText;
  PRUint32 mSyncQueueKey;
  PRUint32 mSyncEventResponseSyncQueueKey;
  bool mUploadEventListenersAttached;
  bool mMainThreadSeenLoadStart;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  Proxy(XMLHttpRequestPrivate* aXHRPrivate)
  : mWorkerPrivate(nsnull), mXMLHttpRequestPrivate(aXHRPrivate),
    mInnerChannelId(0), mOuterChannelId(0), mLastLoaded(0), mLastTotal(0),
    mLastUploadLoaded(0), mLastUploadTotal(0), mIsSyncXHR(false),
    mLastLengthComputable(false), mLastUploadLengthComputable(false),
    mSeenLoadStart(false), mSeenUploadLoadStart(false),
    mSyncQueueKey(PR_UINT32_MAX), mSyncEventResponseSyncQueueKey(PR_UINT32_MAX),
    mUploadEventListenersAttached(false), mMainThreadSeenLoadStart(false)
  { }

  ~Proxy()
  {
    NS_ASSERTION(!mXHR, "Still have an XHR object attached!");
    NS_ASSERTION(!mXHRUpload, "Still have an XHR upload object attached!");
  }

  bool
  Init()
  {
    AssertIsOnMainThread();
    NS_ASSERTION(mWorkerPrivate, "Must have a worker here!");

    if (!mXHR) {
      mXHR = new nsXMLHttpRequest();

      if (NS_FAILED(mXHR->Init(mWorkerPrivate->GetPrincipal(),
                               mWorkerPrivate->GetScriptContext(),
                               mWorkerPrivate->GetWindow(),
                               mWorkerPrivate->GetBaseURI()))) {
        mXHR = nsnull;
        return false;
      }

      if (NS_FAILED(mXHR->GetUpload(getter_AddRefs(mXHRUpload)))) {
        mXHR = nsnull;
        return false;
      }

      if (!AddRemoveEventListeners(false, true)) {
        mXHRUpload = nsnull;
        mXHR = nsnull;
        return false;
      }
    }

    return true;
  }

  void
  Teardown();

  bool
  AddRemoveEventListeners(bool aUpload, bool aAdd);

  void
  Reset()
  {
    AssertIsOnMainThread();

    mPreviousResponseText.Truncate();
    mPreviousStatusText.Truncate();

    if (mUploadEventListenersAttached) {
      AddRemoveEventListeners(true, false);
    }
  }

  PRUint32
  GetSyncQueueKey()
  {
    AssertIsOnMainThread();
    return mSyncEventResponseSyncQueueKey == PR_UINT32_MAX ?
           mSyncQueueKey :
           mSyncEventResponseSyncQueueKey;
  }

  bool
  EventsBypassSyncQueue()
  {
    AssertIsOnMainThread();

    return mSyncQueueKey == PR_UINT32_MAX &&
           mSyncEventResponseSyncQueueKey == PR_UINT32_MAX;
  }
};

} // namespace xhr
END_WORKERS_NAMESPACE

USING_WORKERS_NAMESPACE

using mozilla::dom::workers::xhr::XMLHttpRequestPrivate;
using mozilla::dom::workers::xhr::Proxy;
using mozilla::dom::workers::exceptions::ThrowDOMExceptionForCode;

namespace {

inline intN
GetDOMExceptionCodeFromResult(nsresult aResult)
{
  if (NS_SUCCEEDED(aResult)) {
    return 0;
  }

  if (NS_ERROR_GET_MODULE(aResult) == NS_ERROR_MODULE_DOM) {
    return NS_ERROR_GET_CODE(aResult);
  }

  NS_WARNING("Update main thread implementation for a DOM error code here!");
  return INVALID_STATE_ERR;
}

enum
{
  STRING_abort = 0,
  STRING_error,
  STRING_load,
  STRING_loadstart,
  STRING_progress,
  STRING_readystatechange,
  STRING_loadend,

  STRING_COUNT,

  STRING_LAST_XHR = STRING_loadend,
  STRING_LAST_EVENTTARGET = STRING_progress
};

JS_STATIC_ASSERT(STRING_LAST_XHR >= STRING_LAST_EVENTTARGET);
JS_STATIC_ASSERT(STRING_LAST_XHR == STRING_COUNT - 1);

const char* const sEventStrings[] = {
  // nsIXMLHttpRequestEventTarget event types, supported by both XHR and Upload.
  "abort",
  "error",
  "load",
  "loadstart",
  "progress",

  // nsIXMLHttpRequest event types, supported only by XHR.
  "readystatechange",
  "loadend",
};

JS_STATIC_ASSERT(JS_ARRAY_LENGTH(sEventStrings) == STRING_COUNT);

class MainThreadSyncRunnable : public WorkerSyncRunnable
{
public:
  MainThreadSyncRunnable(WorkerPrivate* aWorkerPrivate, PRUint32 aSyncQueueKey,
                         bool aBypassSyncEventQueue)
  : WorkerSyncRunnable(aWorkerPrivate, aSyncQueueKey, aBypassSyncEventQueue)
  {
    AssertIsOnMainThread();
  }

  bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    AssertIsOnMainThread();
    return true;
  }

  void
  PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aDispatchResult)
  {
    AssertIsOnMainThread();
  }
};

class MainThreadProxyRunnable : public MainThreadSyncRunnable
{
protected:
  nsRefPtr<Proxy> mProxy;

public:
  MainThreadProxyRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy)
  : MainThreadSyncRunnable(aWorkerPrivate, aProxy->GetSyncQueueKey(),
                           aProxy->EventsBypassSyncQueue()),
    mProxy(aProxy)
  { }
};

class TeardownRunnable : public nsRunnable
{
  Proxy* mProxy;

public:
  TeardownRunnable(nsRefPtr<Proxy>& aProxy)
  {
    aProxy.forget(&mProxy);
    NS_ASSERTION(mProxy, "Null proxy!");
  }

  NS_IMETHOD Run()
  {
    AssertIsOnMainThread();

    mProxy->Teardown();
    NS_RELEASE(mProxy);

    return NS_OK;
  }
};

class LoadStartDetectionRunnable : public nsIRunnable,
                                   public nsIDOMEventListener
{
  WorkerPrivate* mWorkerPrivate;
  nsRefPtr<Proxy> mProxy;
  nsRefPtr<nsXMLHttpRequest> mXHR;
  XMLHttpRequestPrivate* mXMLHttpRequestPrivate;
  nsString mEventType;
  bool mReceivedLoadStart;

  class ProxyCompleteRunnable : public MainThreadProxyRunnable
  {
    XMLHttpRequestPrivate* mXMLHttpRequestPrivate;

  public:
    ProxyCompleteRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                          XMLHttpRequestPrivate* aXHRPrivate)
    : MainThreadProxyRunnable(aWorkerPrivate, aProxy),
      mXMLHttpRequestPrivate(aXHRPrivate)
    { }

    bool
    PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
    {
      AssertIsOnMainThread();
      return true;
    }

    void
    PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                 bool aDispatchResult)
    {
      AssertIsOnMainThread();
    }

    bool
    WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
    {
      if (mSyncQueueKey != PR_UINT32_MAX) {
        aWorkerPrivate->StopSyncLoop(mSyncQueueKey, true);
      }

      mXMLHttpRequestPrivate->UnrootJSObject(aCx);
      aWorkerPrivate->RemoveFeature(aCx, mXMLHttpRequestPrivate);

      return true;
    }
  };

public:
  NS_DECL_ISUPPORTS

  LoadStartDetectionRunnable(Proxy* aProxy, XMLHttpRequestPrivate* aXHRPrivate)
  : mWorkerPrivate(aProxy->mWorkerPrivate), mProxy(aProxy), mXHR(aProxy->mXHR),
    mXMLHttpRequestPrivate(aXHRPrivate), mReceivedLoadStart(false)
  {
    AssertIsOnMainThread();
    mEventType.AssignWithConversion(sEventStrings[STRING_loadstart]);
  }

  ~LoadStartDetectionRunnable()
  {
    AssertIsOnMainThread();
  }

  bool
  RegisterAndDispatch()
  {
    AssertIsOnMainThread();

    if (NS_FAILED(mXHR->AddEventListener(mEventType, this, false, false, 2))) {
      NS_WARNING("Failed to add event listener!");
      return false;
    }

    return NS_SUCCEEDED(NS_DispatchToCurrentThread(this));
  }

  NS_IMETHOD
  Run()
  {
    AssertIsOnMainThread();

    if (NS_FAILED(mXHR->RemoveEventListener(mEventType, this, false))) {
      NS_WARNING("Failed to remove event listener!");
    }

    if (!mReceivedLoadStart) {
      mProxy->Reset();

      if (mProxy->mWorkerPrivate) {
        nsRefPtr<ProxyCompleteRunnable> runnable =
          new ProxyCompleteRunnable(mWorkerPrivate, mProxy,
                                    mXMLHttpRequestPrivate);
        if (!runnable->Dispatch(nsnull)) {
          NS_WARNING("Failed to dispatch ProxyCompleteRunnable!");
        }
        mProxy->mWorkerPrivate = nsnull;
      }
    }

    mProxy = nsnull;
    mXHR = nsnull;
    mXMLHttpRequestPrivate = nsnull;
    return NS_OK;
  }

  NS_IMETHOD
  HandleEvent(nsIDOMEvent* aEvent)
  {
    AssertIsOnMainThread();

#ifdef DEBUG
    {
      nsString type;
      if (NS_SUCCEEDED(aEvent->GetType(type))) {
        NS_ASSERTION(type == mEventType, "Unexpected event type!");
      }
      else {
        NS_WARNING("Failed to get event type!");
      }
    }
#endif

    mReceivedLoadStart = true;
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS2(LoadStartDetectionRunnable, nsIRunnable, nsIDOMEventListener)

class EventRunnable : public MainThreadProxyRunnable
{
  nsString mType;
  nsString mResponseText;
  nsCString mStatusText;
  PRUint64 mLoaded;
  PRUint64 mTotal;
  PRUint32 mChannelId;
  PRUint32 mStatus;
  PRUint16 mReadyState;
  bool mUploadEvent;
  bool mProgressEvent;
  bool mLengthComputable;
  bool mResponseTextException;
  bool mStatusException;
  bool mStatusTextException;
  bool mReadyStateException;

public:
  EventRunnable(Proxy* aProxy, bool aUploadEvent, const nsString& aType,
                bool aLengthComputable, PRUint64 aLoaded, PRUint64 aTotal)
  : MainThreadProxyRunnable(aProxy->mWorkerPrivate, aProxy), mType(aType),
    mLoaded(aLoaded), mTotal(aTotal), mChannelId(aProxy->mInnerChannelId),
    mStatus(0), mReadyState(0), mUploadEvent(aUploadEvent),
    mProgressEvent(true), mLengthComputable(aLengthComputable),
    mResponseTextException(false), mStatusException(false),
    mStatusTextException(false), mReadyStateException(false)
  { }

  EventRunnable(Proxy* aProxy, bool aUploadEvent, const nsString& aType)
  : MainThreadProxyRunnable(aProxy->mWorkerPrivate, aProxy), mType(aType),
    mLoaded(0), mTotal(0), mChannelId(aProxy->mInnerChannelId), mStatus(0),
    mReadyState(0), mUploadEvent(aUploadEvent), mProgressEvent(false),
    mLengthComputable(0), mResponseTextException(false),
    mStatusException(false), mStatusTextException(false),
    mReadyStateException(false)
  { }

  bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    nsRefPtr<nsXMLHttpRequest>& xhr = mProxy->mXHR;
    NS_ASSERTION(xhr, "Must have an XHR here!");

    if (NS_SUCCEEDED(xhr->GetResponseText(mResponseText))) {
      if (mResponseText == mProxy->mPreviousResponseText) {
        mResponseText.SetIsVoid(true);
      }
      else {
        mProxy->mPreviousResponseText = mResponseText;
      }
      mResponseTextException = false;
    }
    else {
      mResponseTextException = true;
    }

    mStatusException = NS_FAILED(xhr->GetStatus(&mStatus));

    if (NS_SUCCEEDED(xhr->GetStatusText(mStatusText))) {
      if (mStatusText == mProxy->mPreviousStatusText) {
        mStatusText.SetIsVoid(true);
      }
      else {
        mProxy->mPreviousStatusText = mStatusText;
      }
      mStatusTextException = false;
    }
    else {
      mStatusTextException = true;
    }

    mReadyStateException = NS_FAILED(xhr->GetReadyState(&mReadyState));
    return true;
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    if (mChannelId != mProxy->mOuterChannelId) {
      // Threads raced, this event is now obsolete.
      return true;
    }

    if (!mProxy->mXMLHttpRequestPrivate) {
      // Object was finalized, bail.
      return true;
    }

    if (mType.EqualsASCII(sEventStrings[STRING_loadstart])) {
      if (mUploadEvent) {
        mProxy->mSeenUploadLoadStart = true;
      }
      else {
        mProxy->mSeenLoadStart = true;
      }
    }
    else if (mType.EqualsASCII(sEventStrings[STRING_loadend])) {
      if (mUploadEvent) {
        mProxy->mSeenUploadLoadStart = false;
      }
      else {
        mProxy->mSeenLoadStart = false;
      }
    }

    if (mProgressEvent) {
      // Cache these for premature abort events.
      if (mUploadEvent) {
        mProxy->mLastUploadLengthComputable = mLengthComputable;
        mProxy->mLastUploadLoaded = mLoaded;
        mProxy->mLastUploadTotal = mTotal;
      }
      else {
        mProxy->mLastLengthComputable = mLengthComputable;
        mProxy->mLastLoaded = mLoaded;
        mProxy->mLastTotal = mTotal;
      }
    }

    JSObject* target = mUploadEvent ?
                       mProxy->mXMLHttpRequestPrivate->GetUploadJSObject() :
                       mProxy->mXMLHttpRequestPrivate->GetJSObject();
    if (!target) {
      NS_ASSERTION(mUploadEvent, "How else is this possible?!");
      return true;
    }

    xhr::StateData state;

    state.mResponseTextException = mResponseTextException;
    if (mResponseTextException || mResponseText.IsVoid()) {
      state.mResponseText = JSVAL_VOID;
    }
    else if (mResponseText.IsEmpty()) {
      state.mResponseText = JS_GetEmptyStringValue(aCx);
    }
    else {
      JSString* responseText = JS_NewUCStringCopyN(aCx, mResponseText.get(),
                                                   mResponseText.Length());
      if (!responseText) {
        return false;
      }
      mResponseText.Truncate();
      state.mResponseText = STRING_TO_JSVAL(responseText);
    }

    state.mStatusException = mStatusException;
    state.mStatus = mStatusException ? JSVAL_VOID : INT_TO_JSVAL(mStatus);

    state.mStatusTextException = mStatusTextException;
    if (mStatusTextException || mStatusText.IsVoid()) {
      state.mStatusText = JSVAL_VOID;
    }
    else if (mStatusText.IsEmpty()) {
      state.mStatusText = JS_GetEmptyStringValue(aCx);
    }
    else {
      JSString* statusText = JS_NewStringCopyN(aCx, mStatusText.get(),
                                               mStatusText.Length());
      if (!statusText) {
        return false;
      }
      mStatusText.Truncate();
      state.mStatusText = STRING_TO_JSVAL(statusText);
    }

    state.mReadyStateException = mReadyStateException;
    state.mReadyState = mReadyStateException ?
                        JSVAL_VOID :
                        INT_TO_JSVAL(mReadyState);

    if (!xhr::UpdateXHRState(aCx, target, state)) {
      return false;
    }

    JSString* type = JS_NewUCStringCopyN(aCx, mType.get(), mType.Length());
    if (!type) {
      return false;
    }

    JSObject* event = mProgressEvent ?
                      events::CreateProgressEvent(aCx, type, mLengthComputable,
                                                  mLoaded, mTotal) :
                      events::CreateGenericEvent(aCx, type, false, false,
                                                 false);
    if (!event) {
      return false;
    }

    bool dummy;
    if (!events::DispatchEventToTarget(aCx, target, event, &dummy)) {
      JS_ReportPendingException(aCx);
    }

    return true;
  }
};

class WorkerThreadProxySyncRunnable : public nsRunnable
{
protected:
  WorkerPrivate* mWorkerPrivate;
  nsRefPtr<Proxy> mProxy;
  PRUint32 mSyncQueueKey;

private:
  class ResponseRunnable : public MainThreadProxyRunnable
  {
    PRUint32 mSyncQueueKey;
    intN mErrorCode;

  public:
    ResponseRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                     PRUint32 aSyncQueueKey, intN aErrorCode)
    : MainThreadProxyRunnable(aWorkerPrivate, aProxy),
      mSyncQueueKey(aSyncQueueKey), mErrorCode(aErrorCode)
    {
      NS_ASSERTION(aProxy, "Don't hand me a null proxy!");
    }

    bool
    WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
    {
      if (mErrorCode) {
        ThrowDOMExceptionForCode(aCx, mErrorCode);
        aWorkerPrivate->StopSyncLoop(mSyncQueueKey, false);
      }
      else {
        aWorkerPrivate->StopSyncLoop(mSyncQueueKey, true);
      }

      return true;
    }
  };

public:
  WorkerThreadProxySyncRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy)
  : mWorkerPrivate(aWorkerPrivate), mProxy(aProxy), mSyncQueueKey(0)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
    NS_ASSERTION(aProxy, "Don't hand me a null proxy!");
    MOZ_COUNT_CTOR(mozilla::dom::workers::xhr::WorkerThreadProxySyncRunnable);
  }

  ~WorkerThreadProxySyncRunnable()
  {
    MOZ_COUNT_DTOR(mozilla::dom::workers::xhr::WorkerThreadProxySyncRunnable);
  }

  bool
  Dispatch(JSContext* aCx)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();

    mSyncQueueKey = mWorkerPrivate->CreateNewSyncLoop();

    if (NS_FAILED(NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL))) {
      JS_ReportError(aCx, "Failed to initialize XHR!");
      return false;
    }

    if (!mWorkerPrivate->RunSyncLoop(aCx, mSyncQueueKey)) {
      return false;
    }

    return true;
  }

  virtual intN
  MainThreadRun() = 0;

  NS_IMETHOD
  Run()
  {
    AssertIsOnMainThread();

    PRUint32 oldSyncQueueKey = mProxy->mSyncEventResponseSyncQueueKey;
    mProxy->mSyncEventResponseSyncQueueKey = mSyncQueueKey;

    intN rv = MainThreadRun();

    nsRefPtr<ResponseRunnable> response =
      new ResponseRunnable(mWorkerPrivate, mProxy, mSyncQueueKey, rv);
    if (!response->Dispatch(nsnull)) {
      NS_WARNING("Failed to dispatch response!");
    }

    mProxy->mSyncEventResponseSyncQueueKey = oldSyncQueueKey;

    return NS_OK;
  }
};

class SetMultipartRunnable : public WorkerThreadProxySyncRunnable
{
  bool mValue;

public:
  SetMultipartRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                       bool aValue)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy), mValue(aValue)
  { }

  intN
  MainThreadRun()
  {
    return GetDOMExceptionCodeFromResult(mProxy->mXHR->SetMultipart(mValue));
  }
};

class SetBackgroundRequestRunnable : public WorkerThreadProxySyncRunnable
{
  bool mValue;

public:
  SetBackgroundRequestRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                               bool aValue)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy), mValue(aValue)
  { }

  intN
  MainThreadRun()
  {
    nsresult rv = mProxy->mXHR->SetMozBackgroundRequest(mValue);
    return GetDOMExceptionCodeFromResult(rv);
  }
};

class SetWithCredentialsRunnable : public WorkerThreadProxySyncRunnable
{
  bool mValue;

public:
  SetWithCredentialsRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                             bool aValue)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy), mValue(aValue)
  { }

  intN
  MainThreadRun()
  {
    nsresult rv = mProxy->mXHR->SetWithCredentials(mValue);
    return GetDOMExceptionCodeFromResult(rv);
  }
};

class AbortRunnable : public WorkerThreadProxySyncRunnable
{
public:
  AbortRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy)
  { }

  intN
  MainThreadRun()
  {
    mProxy->mInnerChannelId++;

    WorkerPrivate* oldWorker = mProxy->mWorkerPrivate;
    mProxy->mWorkerPrivate = mWorkerPrivate;

    nsresult rv = mProxy->mXHR->Abort();

    mProxy->mWorkerPrivate = oldWorker;

    mProxy->Reset();

    return GetDOMExceptionCodeFromResult(rv);
  }
};

class GetAllResponseHeadersRunnable : public WorkerThreadProxySyncRunnable
{
  nsCString& mResponseHeaders;

public:
  GetAllResponseHeadersRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                                nsCString& aResponseHeaders)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy),
    mResponseHeaders(aResponseHeaders)
  { }

  intN
  MainThreadRun()
  {
    nsresult rv =
      mProxy->mXHR->GetAllResponseHeaders(getter_Copies(mResponseHeaders));
    return GetDOMExceptionCodeFromResult(rv);
  }
};

class GetResponseHeaderRunnable : public WorkerThreadProxySyncRunnable
{
  const nsCString mHeader;
  nsCString& mValue;

public:
  GetResponseHeaderRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                            const nsCString& aHeader, nsCString& aValue)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy), mHeader(aHeader),
    mValue(aValue)
  { }

  intN
  MainThreadRun()
  {
    nsresult rv = mProxy->mXHR->GetResponseHeader(mHeader, mValue);
    return GetDOMExceptionCodeFromResult(rv);
  }
};

class OpenRunnable : public WorkerThreadProxySyncRunnable
{
  nsCString mMethod;
  nsCString mURL;
  nsString mUser;
  nsString mPassword;
  bool mMultipart;
  bool mBackgroundRequest;
  bool mWithCredentials;

public:
  OpenRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
               const nsCString& aMethod, const nsCString& aURL,
               const nsString& aUser, const nsString& aPassword,
               bool aMultipart, bool aBackgroundRequest, bool aWithCredentials)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy), mMethod(aMethod),
    mURL(aURL), mUser(aUser), mPassword(aPassword), mMultipart(aMultipart),
    mBackgroundRequest(aBackgroundRequest), mWithCredentials(aWithCredentials)
  { }

  intN
  MainThreadRun()
  {
    WorkerPrivate* oldWorker = mProxy->mWorkerPrivate;
    mProxy->mWorkerPrivate = mWorkerPrivate;

    intN retval = MainThreadRunInternal();

    mProxy->mWorkerPrivate = oldWorker;
    return retval;
  }

  intN
  MainThreadRunInternal()
  {
    if (!mProxy->Init()) {
      return INVALID_STATE_ERR;
    }

    nsresult rv;

    if (mMultipart) {
      rv = mProxy->mXHR->SetMultipart(mMultipart);
      if (NS_FAILED(rv)) {
        return GetDOMExceptionCodeFromResult(rv);
      }
    }

    if (mBackgroundRequest) {
      rv = mProxy->mXHR->SetMozBackgroundRequest(mBackgroundRequest);
      if (NS_FAILED(rv)) {
        return GetDOMExceptionCodeFromResult(rv);
      }
    }

    if (mWithCredentials) {
      rv = mProxy->mXHR->SetWithCredentials(mWithCredentials);
      if (NS_FAILED(rv)) {
        return GetDOMExceptionCodeFromResult(rv);
      }
    }

    mProxy->mInnerChannelId++;
    mProxy->mPreviousResponseText.Truncate();
    mProxy->mPreviousStatusText.Truncate();

    rv = mProxy->mXHR->Open(mMethod, mURL, true, mUser, mPassword, 1);
    return GetDOMExceptionCodeFromResult(rv);
  }
};

class SendRunnable : public WorkerThreadProxySyncRunnable
{
  JSAutoStructuredCloneBuffer mBody;
  PRUint32 mSyncQueueKey;
  bool mHasUploadListeners;

public:
  SendRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
               JSAutoStructuredCloneBuffer& aBody, PRUint32 aSyncQueueKey,
               bool aHasUploadListeners)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy),
    mSyncQueueKey(aSyncQueueKey), mHasUploadListeners(aHasUploadListeners)
  {
    mBody.swap(aBody);
  }

  intN
  MainThreadRun()
  {
    NS_ASSERTION(!mProxy->mWorkerPrivate, "Should be null!");

    mProxy->mWorkerPrivate = mWorkerPrivate;

    NS_ASSERTION(mProxy->mSyncQueueKey == PR_UINT32_MAX, "Should be unset!");
    mProxy->mSyncQueueKey = mSyncQueueKey;

    nsCOMPtr<nsIVariant> variant;
    if (mBody.data()) {
      nsIXPConnect* xpc = nsContentUtils::XPConnect();
      NS_ASSERTION(xpc, "This should never be null!");

      RuntimeService::AutoSafeJSContext cx;

      intN error = 0;

      jsval body;
      if (mBody.read(cx, &body)) {
        if (NS_FAILED(xpc->JSValToVariant(cx, &body,
                                          getter_AddRefs(variant)))) {
          error = INVALID_STATE_ERR;
        }
      }
      else {
        error = DATA_CLONE_ERR;
      }

      mBody.clear();

      if (error) {
        return error;
      }
    }

    if (mHasUploadListeners) {
      NS_ASSERTION(!mProxy->mUploadEventListenersAttached, "Huh?!");
      if (!mProxy->AddRemoveEventListeners(true, true)) {
        NS_ERROR("This should never fail!");
      }
    }

    nsresult rv = mProxy->mXHR->Send(variant);

    if (NS_SUCCEEDED(rv) && !mHasUploadListeners) {
      NS_ASSERTION(!mProxy->mUploadEventListenersAttached, "Huh?!");
      if (!mProxy->AddRemoveEventListeners(true, true)) {
        NS_ERROR("This should never fail!");
      }
    }

    return GetDOMExceptionCodeFromResult(rv);
  }
};

class SendAsBinaryRunnable : public WorkerThreadProxySyncRunnable
{
  nsString mBody;
  PRUint32 mSyncQueueKey;
  bool mHasUploadListeners;

public:
  SendAsBinaryRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                       const nsString& aBody, PRUint32 aSyncQueueKey,
                       bool aHasUploadListeners)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy), mBody(aBody),
    mSyncQueueKey(aSyncQueueKey), mHasUploadListeners(aHasUploadListeners)
  { }

  intN
  MainThreadRun()
  {
    NS_ASSERTION(!mProxy->mWorkerPrivate, "Should be null!");

    mProxy->mWorkerPrivate = mWorkerPrivate;

    NS_ASSERTION(mProxy->mSyncQueueKey == PR_UINT32_MAX, "Should be unset!");
    mProxy->mSyncQueueKey = mSyncQueueKey;

    if (mHasUploadListeners) {
      NS_ASSERTION(!mProxy->mUploadEventListenersAttached, "Huh?!");
      if (!mProxy->AddRemoveEventListeners(true, true)) {
        NS_ERROR("This should never fail!");
      }
    }

    nsresult rv = mProxy->mXHR->SendAsBinary(mBody);

    if (NS_SUCCEEDED(rv) && !mHasUploadListeners) {
      NS_ASSERTION(!mProxy->mUploadEventListenersAttached, "Huh?!");
      if (!mProxy->AddRemoveEventListeners(true, true)) {
        NS_ERROR("This should never fail!");
      }
    }

    return GetDOMExceptionCodeFromResult(rv);
  }
};

class SetRequestHeaderRunnable : public WorkerThreadProxySyncRunnable
{
  nsCString mHeader;
  nsCString mValue;

public:
  SetRequestHeaderRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                           const nsCString& aHeader, const nsCString& aValue)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy), mHeader(aHeader),
    mValue(aValue)
  { }

  intN
  MainThreadRun()
  {
    nsresult rv = mProxy->mXHR->SetRequestHeader(mHeader, mValue);
    return GetDOMExceptionCodeFromResult(rv);
  }
};

class OverrideMimeTypeRunnable : public WorkerThreadProxySyncRunnable
{
  nsCString mMimeType;

public:
  OverrideMimeTypeRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                           const nsCString& aMimeType)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy), mMimeType(aMimeType)
  { }

  intN
  MainThreadRun()
  {
    nsresult rv = mProxy->mXHR->OverrideMimeType(mMimeType);
    return GetDOMExceptionCodeFromResult(rv);
  }
};

} // anonymous namespace

void
Proxy::Teardown()
{
  AssertIsOnMainThread();

  if (mXHR) {
    Reset();

    AddRemoveEventListeners(false, false);
    mXHR->Abort();

    mXHRUpload = nsnull;
    mXHR = nsnull;
  }
}

bool
Proxy::AddRemoveEventListeners(bool aUpload, bool aAdd)
{
  AssertIsOnMainThread();

  NS_ASSERTION(!aUpload ||
               (mUploadEventListenersAttached && !aAdd) ||
               (!mUploadEventListenersAttached && aAdd),
               "Messed up logic for upload listeners!");

  nsCOMPtr<nsIDOMEventTarget> target =
    aUpload ?
    do_QueryInterface(mXHRUpload) :
    do_QueryInterface(static_cast<nsIXMLHttpRequest*>(mXHR.get()));
  NS_ASSERTION(target, "This should never fail!");

  PRUint32 lastEventType = aUpload ? STRING_LAST_EVENTTARGET : STRING_LAST_XHR;

  nsAutoString eventType;
  for (PRUint32 index = 0; index <= lastEventType; index++) {
    eventType = NS_ConvertASCIItoUTF16(sEventStrings[index]);
    if (aAdd) {
      if (NS_FAILED(target->AddEventListener(eventType, this, false))) {
        return false;
      }
    }
    else if (NS_FAILED(target->RemoveEventListener(eventType, this, false))) {
      return false;
    }
  }

  if (aUpload) {
    mUploadEventListenersAttached = aAdd;
  }

  return true;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(Proxy, nsIDOMEventListener)

NS_IMETHODIMP
Proxy::HandleEvent(nsIDOMEvent* aEvent)
{
  AssertIsOnMainThread();

  if (!mWorkerPrivate || !mXMLHttpRequestPrivate) {
    NS_ERROR("Shouldn't get here!");
    return NS_OK;
  }

  nsString type;
  if (NS_FAILED(aEvent->GetType(type))) {
    NS_WARNING("Failed to get event type!");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMEventTarget> target;
  if (NS_FAILED(aEvent->GetTarget(getter_AddRefs(target)))) {
    NS_WARNING("Failed to get target!");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIXMLHttpRequestUpload> uploadTarget = do_QueryInterface(target);
  nsCOMPtr<nsIDOMProgressEvent> progressEvent = do_QueryInterface(aEvent);

  nsRefPtr<EventRunnable> runnable;

  if (progressEvent) {
    bool lengthComputable;
    PRUint64 loaded, total;
    if (NS_FAILED(progressEvent->GetLengthComputable(&lengthComputable)) ||
        NS_FAILED(progressEvent->GetLoaded(&loaded)) ||
        NS_FAILED(progressEvent->GetTotal(&total))) {
      NS_WARNING("Bad progress event!");
      return NS_ERROR_FAILURE;
    }
    runnable = new EventRunnable(this, !!uploadTarget, type, lengthComputable,
                                 loaded, total);
  }
  else {
    runnable = new EventRunnable(this, !!uploadTarget, type);
  }

  runnable->Dispatch(nsnull);

  if (!uploadTarget) {
    if (type.EqualsASCII(sEventStrings[STRING_loadstart])) {
      NS_ASSERTION(!mMainThreadSeenLoadStart, "Huh?!");
      mMainThreadSeenLoadStart = true;
    }
    else if (mMainThreadSeenLoadStart &&
             type.EqualsASCII(sEventStrings[STRING_loadend])) {
      mMainThreadSeenLoadStart = false;

      nsRefPtr<LoadStartDetectionRunnable> runnable =
        new LoadStartDetectionRunnable(this, mXMLHttpRequestPrivate);
      if (NS_FAILED(NS_DispatchToCurrentThread(runnable))) {
        NS_WARNING("Failed to dispatch LoadStartDetectionRunnable!");
      }
    }
  }

  return NS_OK;
}

XMLHttpRequestPrivate::XMLHttpRequestPrivate(JSObject* aObj,
                                             WorkerPrivate* aWorkerPrivate)
: mJSObject(aObj), mUploadJSObject(nsnull), mWorkerPrivate(aWorkerPrivate),
  mJSObjectRootCount(0), mMultipart(false), mBackgroundRequest(false),
  mWithCredentials(false), mCanceled(false)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_COUNT_CTOR(mozilla::dom::workers::xhr::XMLHttpRequestPrivate);
}

XMLHttpRequestPrivate::~XMLHttpRequestPrivate()
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  NS_ASSERTION(!mJSObjectRootCount, "Huh?!");
  MOZ_COUNT_DTOR(mozilla::dom::workers::xhr::XMLHttpRequestPrivate);
}

void
XMLHttpRequestPrivate::ReleaseProxy()
{
  // Can't assert that we're on the worker thread here because mWorkerPrivate
  // may be gone.

  if (mProxy) {
    // Don't let any more events run.
    mProxy->mOuterChannelId++;

    nsCOMPtr<nsIRunnable> runnable = new TeardownRunnable(mProxy);
    NS_ASSERTION(!mProxy, "Should have swapped!");

    if (NS_FAILED(NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL))) {
      NS_ERROR("Failed to dispatch teardown runnable!");
    }
  }
}

bool
XMLHttpRequestPrivate::RootJSObject(JSContext* aCx)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (!mJSObjectRootCount) {
    if (!JS_AddNamedObjectRoot(aCx, &mJSObject,
                               "XMLHttpRequestPrivate mJSObject")) {
      return false;
    }
  }

  mJSObjectRootCount++;
  return true;
}

void
XMLHttpRequestPrivate::UnrootJSObject(JSContext* aCx)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  NS_ASSERTION(mJSObjectRootCount, "Mismatched calls to UnrootJSObject!");
  mJSObjectRootCount--;

  if (!mJSObjectRootCount && !JS_RemoveObjectRoot(aCx, &mJSObject)) {
    NS_ERROR("JS_RemoveObjectRoot failed!");
  }
}

bool
XMLHttpRequestPrivate::Notify(JSContext* aCx, Status aStatus)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (aStatus >= Canceling && !mCanceled) {
    mCanceled = true;
    ReleaseProxy();
  }

  return true;
}

bool
XMLHttpRequestPrivate::SetMultipart(JSContext* aCx, jsval *aVp)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    return false;
  }

  JSBool multipart;
  if (!JS_ValueToBoolean(aCx, *aVp, &multipart)) {
    return false;
  }

  *aVp = multipart ? JSVAL_TRUE : JSVAL_FALSE;

  if (!mProxy) {
    mMultipart = !!multipart;
    return true;
  }

  nsRefPtr<SetMultipartRunnable> runnable =
    new SetMultipartRunnable(mWorkerPrivate, mProxy, multipart);
  if (!runnable->Dispatch(aCx)) {
    return false;
  }

  return true;
}

bool
XMLHttpRequestPrivate::SetMozBackgroundRequest(JSContext* aCx, jsval *aVp)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    return false;
  }

  JSBool backgroundRequest;
  if (!JS_ValueToBoolean(aCx, *aVp, &backgroundRequest)) {
    return false;
  }

  *aVp = backgroundRequest ? JSVAL_TRUE : JSVAL_FALSE;

  if (!mProxy) {
    mBackgroundRequest = !!backgroundRequest;
    return true;
  }

  nsRefPtr<SetBackgroundRequestRunnable> runnable =
    new SetBackgroundRequestRunnable(mWorkerPrivate, mProxy, backgroundRequest);
  if (!runnable->Dispatch(aCx)) {
    return false;
  }

  return true;
}

bool
XMLHttpRequestPrivate::SetWithCredentials(JSContext* aCx, jsval *aVp)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    return false;
  }

  JSBool withCredentials;
  if (!JS_ValueToBoolean(aCx, *aVp, &withCredentials)) {
    return false;
  }

  *aVp = withCredentials ? JSVAL_TRUE : JSVAL_FALSE;

  if (!mProxy) {
    mWithCredentials = !!withCredentials;
    return true;
  }

  nsRefPtr<SetWithCredentialsRunnable> runnable =
    new SetWithCredentialsRunnable(mWorkerPrivate, mProxy, withCredentials);
  if (!runnable->Dispatch(aCx)) {
    return false;
  }

  return true;
}

bool
XMLHttpRequestPrivate::Abort(JSContext* aCx)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    return false;
  }

  if (mProxy) {
    if (!MaybeDispatchPrematureAbortEvents(aCx, false)) {
      return false;
    }
  }
  else {
    return true;
  }

  mProxy->mOuterChannelId++;

  nsRefPtr<AbortRunnable> runnable = new AbortRunnable(mWorkerPrivate, mProxy);
  return runnable->Dispatch(aCx);
}

JSString*
XMLHttpRequestPrivate::GetAllResponseHeaders(JSContext* aCx)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    return false;
  }

  if (!mProxy) {
    ThrowDOMExceptionForCode(aCx, INVALID_STATE_ERR);
    return false;
  }

  nsCString responseHeaders;
  nsRefPtr<GetAllResponseHeadersRunnable> runnable =
    new GetAllResponseHeadersRunnable(mWorkerPrivate, mProxy, responseHeaders);
  if (!runnable->Dispatch(aCx)) {
    return false;
  }

  return JS_NewStringCopyN(aCx, responseHeaders.get(),
                           responseHeaders.Length());
}

JSString*
XMLHttpRequestPrivate::GetResponseHeader(JSContext* aCx, JSString* aHeader)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    return false;
  }

  if (!mProxy) {
    ThrowDOMExceptionForCode(aCx, INVALID_STATE_ERR);
    return false;
  }

  nsDependentJSString header;
  if (!header.init(aCx, aHeader)) {
    return false;
  }

  nsCString value;
  nsRefPtr<GetResponseHeaderRunnable> runnable =
    new GetResponseHeaderRunnable(mWorkerPrivate, mProxy,
                                  NS_ConvertUTF16toUTF8(header), value);
  if (!runnable->Dispatch(aCx)) {
    return false;
  }

  return JS_NewStringCopyN(aCx, value.get(), value.Length());
}

bool
XMLHttpRequestPrivate::Open(JSContext* aCx, JSString* aMethod, JSString* aURL,
                            bool aAsync, JSString* aUser, JSString* aPassword)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    return false;
  }

  nsDependentJSString method, url, user, password;
  if (!method.init(aCx, aMethod) || !url.init(aCx, aURL) ||
      !user.init(aCx, aUser) || !password.init(aCx, aPassword)) {
    return false;
  }

  if (mProxy) {
    if (!MaybeDispatchPrematureAbortEvents(aCx, true)) {
      return false;
    }
  }
  else {
    mProxy = new Proxy(this);
  }

  mProxy->mOuterChannelId++;

  nsRefPtr<OpenRunnable> runnable =
    new OpenRunnable(mWorkerPrivate, mProxy, NS_ConvertUTF16toUTF8(method),
                     NS_ConvertUTF16toUTF8(url), user, password, mMultipart,
                     mBackgroundRequest, mWithCredentials);

  // These were only useful before we had a proxy. From here on out changing
  // those values makes no difference.
  mMultipart = mBackgroundRequest = mWithCredentials = false;

  if (!runnable->Dispatch(aCx)) {
    ReleaseProxy();
    return false;
  }

  mProxy->mIsSyncXHR = !aAsync;
  return true;
}

bool
XMLHttpRequestPrivate::Send(JSContext* aCx, bool aHasBody, jsval aBody)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    return false;
  }

  JSAutoStructuredCloneBuffer buffer;

  if (aHasBody) {
    if (JSVAL_IS_PRIMITIVE(aBody) ||
        !js_IsArrayBuffer(JSVAL_TO_OBJECT(aBody))) {
      JSString* bodyStr = JS_ValueToString(aCx, aBody);
      if (!bodyStr) {
        return false;
      }
      aBody = STRING_TO_JSVAL(bodyStr);
    }

    if (!buffer.write(aCx, aBody)) {
      return false;
    }
  }

  if (!mProxy) {
    ThrowDOMExceptionForCode(aCx, INVALID_STATE_ERR);
    return false;
  }

  bool hasUploadListeners = false;
  if (mUploadJSObject) {
    events::EventTarget* target =
      events::EventTarget::FromJSObject(aCx, mUploadJSObject);
    NS_ASSERTION(target, "This should never be null!");
    hasUploadListeners = target->HasListeners();
  }

  if (!RootJSObject(aCx)) {
    return false;
  }

  if (!mWorkerPrivate->AddFeature(aCx, this)) {
    UnrootJSObject(aCx);
    return false;
  }

  PRUint32 syncQueueKey = PR_UINT32_MAX;
  if (mProxy->mIsSyncXHR) {
    syncQueueKey = mWorkerPrivate->CreateNewSyncLoop();
  }

  nsRefPtr<SendRunnable> runnable =
    new SendRunnable(mWorkerPrivate, mProxy, buffer, syncQueueKey,
                     hasUploadListeners);
  if (!runnable->Dispatch(aCx)) {
    UnrootJSObject(aCx);
    mWorkerPrivate->RemoveFeature(aCx, this);
    return false;
  }

  // The event loop was spun above, make sure we aren't canceled already.
  if (mCanceled) {
    return false;
  }

  return mProxy->mIsSyncXHR ?
         mWorkerPrivate->RunSyncLoop(aCx, syncQueueKey) :
         true;
}

bool
XMLHttpRequestPrivate::SendAsBinary(JSContext* aCx, JSString* aBody)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    return false;
  }

  if (!mProxy) {
    ThrowDOMExceptionForCode(aCx, INVALID_STATE_ERR);
    return false;
  }

  nsDependentJSString body;
  if (!body.init(aCx, aBody)) {
    return false;
  }

  bool hasUploadListeners = false;
  if (mUploadJSObject) {
    events::EventTarget* target =
      events::EventTarget::FromJSObject(aCx, mUploadJSObject);
    NS_ASSERTION(target, "This should never be null!");
    hasUploadListeners = target->HasListeners();
  }

  if (!RootJSObject(aCx)) {
    return false;
  }

  PRUint32 syncQueueKey = PR_UINT32_MAX;
  if (mProxy->mIsSyncXHR) {
    syncQueueKey = mWorkerPrivate->CreateNewSyncLoop();
  }

  nsRefPtr<SendAsBinaryRunnable> runnable =
    new SendAsBinaryRunnable(mWorkerPrivate, mProxy, body, syncQueueKey,
                             hasUploadListeners);
  if (!runnable->Dispatch(aCx)) {
    UnrootJSObject(aCx);
    return false;
  }

  // The event loop was spun above, make sure we aren't canceled already.
  if (mCanceled) {
    return false;
  }

  return mProxy->mIsSyncXHR ?
         mWorkerPrivate->RunSyncLoop(aCx, syncQueueKey) :
         true;
}

bool
XMLHttpRequestPrivate::SetRequestHeader(JSContext* aCx, JSString* aHeader,
                                        JSString* aValue)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    return false;
  }

  if (!mProxy) {
    ThrowDOMExceptionForCode(aCx, INVALID_STATE_ERR);
    return false;
  }

  nsDependentJSString header, value;
  if (!header.init(aCx, aHeader) || !value.init(aCx, aValue)) {
    return false;
  }

  nsRefPtr<SetRequestHeaderRunnable> runnable =
    new SetRequestHeaderRunnable(mWorkerPrivate, mProxy,
                                 NS_ConvertUTF16toUTF8(header),
                                 NS_ConvertUTF16toUTF8(value));
  return runnable->Dispatch(aCx);
}

bool
XMLHttpRequestPrivate::OverrideMimeType(JSContext* aCx, JSString* aMimeType)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    return false;
  }

  // We're supposed to throw if the state is not OPENED or HEADERS_RECEIVED. We
  // can detect OPENED really easily but we can't detect HEADERS_RECEIVED in a
  // non-racy way until the XHR state machine actually runs on this thread
  // (bug 671047). For now we're going to let this work only if the Send()
  // method has not been called.
  if (!mProxy || SendInProgress()) {
    ThrowDOMExceptionForCode(aCx, INVALID_STATE_ERR);
    return false;
  }

  nsDependentJSString mimeType;
  if (!mimeType.init(aCx, aMimeType)) {
    return false;
  }

  nsRefPtr<OverrideMimeTypeRunnable> runnable =
    new OverrideMimeTypeRunnable(mWorkerPrivate, mProxy, 
                                 NS_ConvertUTF16toUTF8(mimeType));
  return runnable->Dispatch(aCx);
}

bool
XMLHttpRequestPrivate::MaybeDispatchPrematureAbortEvents(JSContext* aCx,
                                                         bool aFromOpen)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  NS_ASSERTION(mProxy, "Must have a proxy here!");

  xhr::StateData state = {
    JSVAL_VOID, JSVAL_VOID, JSVAL_VOID, INT_TO_JSVAL(4),
    false, false, false, false
  };

  if (mProxy->mSeenUploadLoadStart) {
    JSObject* target = mProxy->mXMLHttpRequestPrivate->GetUploadJSObject();
    NS_ASSERTION(target, "Must have a target!");

    if (!xhr::UpdateXHRState(aCx, target, state) ||
        !DispatchPrematureAbortEvent(aCx, target, STRING_abort, true) ||
        !DispatchPrematureAbortEvent(aCx, target, STRING_loadend, true)) {
      return false;
    }

    mProxy->mSeenUploadLoadStart = false;
  }

  if (mProxy->mSeenLoadStart) {
    JSObject* target = mProxy->mXMLHttpRequestPrivate->GetJSObject();
    NS_ASSERTION(target, "Must have a target!");

    if (!xhr::UpdateXHRState(aCx, target, state) ||
        !DispatchPrematureAbortEvent(aCx, target, STRING_readystatechange,
                                     false)) {
      return false;
    }

    if (aFromOpen) {
      if (!DispatchPrematureAbortEvent(aCx, target, STRING_abort, false) ||
          !DispatchPrematureAbortEvent(aCx, target, STRING_loadend, false)) {
        return false;
      }
    }

    mProxy->mSeenLoadStart = false;
  }

  return true;
}

bool
XMLHttpRequestPrivate::DispatchPrematureAbortEvent(JSContext* aCx,
                                                   JSObject* aTarget,
                                                   PRUint64 aEventType,
                                                   bool aUploadTarget)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  NS_ASSERTION(mProxy, "Must have a proxy here!");
  NS_ASSERTION(aTarget, "Don't call me without a target!");
  NS_ASSERTION(aEventType <= STRING_COUNT, "Bad string index!");

  JSString* type = JS_NewStringCopyZ(aCx, sEventStrings[aEventType]);
  if (!type) {
    return false;
  }

  JSObject* event;
  if (aEventType == STRING_readystatechange) {
    event = events::CreateGenericEvent(aCx, type, false, false, false);
  }
  else {
    if (aUploadTarget) {
      event = events::CreateProgressEvent(aCx, type,
                                          mProxy->mLastUploadLengthComputable,
                                          mProxy->mLastUploadLoaded,
                                          mProxy->mLastUploadTotal);
    }
    else {
      event = events::CreateProgressEvent(aCx, type,
                                          mProxy->mLastLengthComputable,
                                          mProxy->mLastLoaded,
                                          mProxy->mLastTotal);
    }
  }
  if (!event) {
    return false;
  }

  bool dummy;
  return events::DispatchEventToTarget(aCx, aTarget, event, &dummy);
}
