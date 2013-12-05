/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XMLHttpRequest.h"

#include "nsIDOMEvent.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMProgressEvent.h"
#include "nsIRunnable.h"
#include "nsIVariant.h"
#include "nsIXMLHttpRequest.h"
#include "nsIXPConnect.h"

#include "jsfriendapi.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsEventDispatcher.h"
#include "nsJSUtils.h"
#include "nsThreadUtils.h"

#include "mozilla/dom/Exceptions.h"
#include "File.h"
#include "RuntimeService.h"
#include "WorkerPrivate.h"
#include "XMLHttpRequestUpload.h"

#include "mozilla/Attributes.h"
#include "nsComponentManagerUtils.h"

using namespace mozilla;

using namespace mozilla::dom;
USING_WORKERS_NAMESPACE

// XXX Need to figure this out...
#define UNCATCHABLE_EXCEPTION NS_ERROR_OUT_OF_MEMORY

/**
 *  XMLHttpRequest in workers
 *
 *  XHR in workers is implemented by proxying calls/events/etc between the
 *  worker thread and an nsXMLHttpRequest on the main thread.  The glue
 *  object here is the Proxy, which lives on both threads.  All other objects
 *  live on either the main thread (the nsXMLHttpRequest) or the worker thread
 *  (the worker and XHR private objects).
 *
 *  The main thread XHR is always operated in async mode, even for sync XHR
 *  in workers.  Calls made on the worker thread are proxied to the main thread
 *  synchronously (meaning the worker thread is blocked until the call
 *  returns).  Each proxied call spins up a sync queue, which captures any
 *  synchronously dispatched events and ensures that they run synchronously
 *  on the worker as well.  Asynchronously dispatched events are posted to the
 *  worker thread to run asynchronously.  Some of the XHR state is mirrored on
 *  the worker thread to avoid needing a cross-thread call on every property
 *  access.
 *
 *  The XHR private is stored in the private slot of the XHR JSObject on the
 *  worker thread.  It is destroyed when that JSObject is GCd.  The private
 *  roots its JSObject while network activity is in progress.  It also
 *  adds itself as a feature to the worker to give itself a chance to clean up
 *  if the worker goes away during an XHR call.  It is important that the
 *  rooting and feature registration (collectively called pinning) happens at
 *  the proper times.  If we pin for too long we can cause memory leaks or even
 *  shutdown hangs.  If we don't pin for long enough we introduce a GC hazard.
 *
 *  The XHR is pinned from the time Send is called to roughly the time loadend
 *  is received.  There are some complications involved with Abort and XHR
 *  reuse.  We maintain a counter on the main thread of how many times Send was
 *  called on this XHR, and we decrement the counter every time we receive a
 *  loadend event.  When the counter reaches zero we dispatch a runnable to the
 *  worker thread to unpin the XHR.  We only decrement the counter if the 
 *  dispatch was successful, because the worker may no longer be accepting
 *  regular runnables.  In the event that we reach Proxy::Teardown and there
 *  the outstanding Send count is still non-zero, we dispatch a control
 *  runnable which is guaranteed to run.
 *
 *  NB: Some of this could probably be simplified now that we have the
 *  inner/outer channel ids.
 */

BEGIN_WORKERS_NAMESPACE

class Proxy MOZ_FINAL : public nsIDOMEventListener
{
public:
  // Read on multiple threads.
  WorkerPrivate* mWorkerPrivate;
  XMLHttpRequest* mXMLHttpRequestPrivate;

  // XHR Params:
  bool mMozAnon;
  bool mMozSystem;

  // Only touched on the main thread.
  nsRefPtr<nsXMLHttpRequest> mXHR;
  nsCOMPtr<nsIXMLHttpRequestUpload> mXHRUpload;
  uint32_t mInnerEventStreamId;
  uint32_t mInnerChannelId;
  uint32_t mOutstandingSendCount;

  // Only touched on the worker thread.
  uint32_t mOuterEventStreamId;
  uint32_t mOuterChannelId;
  uint64_t mLastLoaded;
  uint64_t mLastTotal;
  uint64_t mLastUploadLoaded;
  uint64_t mLastUploadTotal;
  bool mIsSyncXHR;
  bool mLastLengthComputable;
  bool mLastUploadLengthComputable;
  bool mSeenLoadStart;
  bool mSeenUploadLoadStart;

  // Only touched on the main thread.
  uint32_t mSyncQueueKey;
  uint32_t mSyncEventResponseSyncQueueKey;
  bool mUploadEventListenersAttached;
  bool mMainThreadSeenLoadStart;
  bool mInOpen;

public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  Proxy(XMLHttpRequest* aXHRPrivate, bool aMozAnon, bool aMozSystem)
  : mWorkerPrivate(nullptr), mXMLHttpRequestPrivate(aXHRPrivate),
    mMozAnon(aMozAnon), mMozSystem(aMozSystem),
    mInnerEventStreamId(0), mInnerChannelId(0), mOutstandingSendCount(0),
    mOuterEventStreamId(0), mOuterChannelId(0), mLastLoaded(0), mLastTotal(0),
    mLastUploadLoaded(0), mLastUploadTotal(0), mIsSyncXHR(false),
    mLastLengthComputable(false), mLastUploadLengthComputable(false),
    mSeenLoadStart(false), mSeenUploadLoadStart(false),
    mSyncQueueKey(UINT32_MAX),
    mSyncEventResponseSyncQueueKey(UINT32_MAX),
    mUploadEventListenersAttached(false), mMainThreadSeenLoadStart(false),
    mInOpen(false)
  { }

  ~Proxy()
  {
    NS_ASSERTION(!mXHR, "Still have an XHR object attached!");
    NS_ASSERTION(!mXHRUpload, "Still have an XHR upload object attached!");
    NS_ASSERTION(!mOutstandingSendCount, "We're dying too early!");
  }

  bool
  Init()
  {
    AssertIsOnMainThread();
    NS_ASSERTION(mWorkerPrivate, "Must have a worker here!");

    if (!mXHR) {
      nsPIDOMWindow* ownerWindow = mWorkerPrivate->GetWindow();
      if (ownerWindow) {
        ownerWindow = ownerWindow->GetOuterWindow();
        if (!ownerWindow) {
          NS_ERROR("No outer window?!");
          return false;
        }

        nsPIDOMWindow* innerWindow = ownerWindow->GetCurrentInnerWindow();
        if (mWorkerPrivate->GetWindow() != innerWindow) {
          NS_WARNING("Window has navigated, cannot create XHR here.");
          return false;
        }
      }

      mXHR = new nsXMLHttpRequest();

      nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(ownerWindow);
      if (NS_FAILED(mXHR->Init(mWorkerPrivate->GetPrincipal(),
                               mWorkerPrivate->GetScriptContext(),
                               global, mWorkerPrivate->GetBaseURI()))) {
        mXHR = nullptr;
        return false;
      }

      mXHR->SetParameters(mMozAnon, mMozSystem);

      if (NS_FAILED(mXHR->GetUpload(getter_AddRefs(mXHRUpload)))) {
        mXHR = nullptr;
        return false;
      }

      if (!AddRemoveEventListeners(false, true)) {
        mXHRUpload = nullptr;
        mXHR = nullptr;
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

    if (mUploadEventListenersAttached) {
      AddRemoveEventListeners(true, false);
    }
  }

  uint32_t
  GetSyncQueueKey()
  {
    AssertIsOnMainThread();
    return mSyncEventResponseSyncQueueKey == UINT32_MAX ?
           mSyncQueueKey :
           mSyncEventResponseSyncQueueKey;
  }

  bool
  EventsBypassSyncQueue()
  {
    AssertIsOnMainThread();

    return mSyncQueueKey == UINT32_MAX &&
           mSyncEventResponseSyncQueueKey == UINT32_MAX;
  }
};

END_WORKERS_NAMESPACE


namespace {

inline void
ConvertResponseTypeToString(XMLHttpRequestResponseType aType,
                            nsString& aString)
{
  using namespace
    mozilla::dom::XMLHttpRequestResponseTypeValues;

  size_t index = static_cast<size_t>(aType);
  MOZ_ASSERT(index < ArrayLength(strings), "Codegen gave us a bad value!");

  aString.AssignASCII(strings[index].value, strings[index].length);
}

inline XMLHttpRequestResponseType
ConvertStringToResponseType(const nsAString& aString)
{
  using namespace
    mozilla::dom::XMLHttpRequestResponseTypeValues;

  for (size_t index = 0; index < ArrayLength(strings) - 1; index++) {
    if (aString.EqualsASCII(strings[index].value, strings[index].length)) {
      return static_cast<XMLHttpRequestResponseType>(index);
    }
  }

  MOZ_CRASH("Don't know anything about this response type!");
}

enum
{
  STRING_abort = 0,
  STRING_error,
  STRING_load,
  STRING_loadstart,
  STRING_progress,
  STRING_timeout,
  STRING_readystatechange,
  STRING_loadend,

  STRING_COUNT,

  STRING_LAST_XHR = STRING_loadend,
  STRING_LAST_EVENTTARGET = STRING_timeout
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
  "timeout",

  // nsIXMLHttpRequest event types, supported only by XHR.
  "readystatechange",
  "loadend",
};

JS_STATIC_ASSERT(JS_ARRAY_LENGTH(sEventStrings) == STRING_COUNT);

class MainThreadProxyRunnable : public MainThreadSyncRunnable
{
protected:
  nsRefPtr<Proxy> mProxy;

public:
  MainThreadProxyRunnable(WorkerPrivate* aWorkerPrivate,
                          ClearingBehavior aClearingBehavior, Proxy* aProxy)
  : MainThreadSyncRunnable(aWorkerPrivate, aClearingBehavior,
                           aProxy->GetSyncQueueKey(),
                           aProxy->EventsBypassSyncQueue()),
    mProxy(aProxy)
  { }
};

class XHRUnpinRunnable : public WorkerControlRunnable
{
  XMLHttpRequest* mXMLHttpRequestPrivate;

public:
  XHRUnpinRunnable(WorkerPrivate* aWorkerPrivate,
                   XMLHttpRequest* aXHRPrivate)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThread, UnchangedBusyCount),
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
    mXMLHttpRequestPrivate->Unpin();

    return true;
  }
};

class AsyncTeardownRunnable : public nsRunnable
{
  nsRefPtr<Proxy> mProxy;

public:
  AsyncTeardownRunnable(Proxy* aProxy)
  {
    mProxy = aProxy;
    NS_ASSERTION(mProxy, "Null proxy!");
  }

  NS_IMETHOD Run()
  {
    AssertIsOnMainThread();

    mProxy->Teardown();
    mProxy = nullptr;

    return NS_OK;
  }
};

class LoadStartDetectionRunnable MOZ_FINAL : public nsIRunnable,
                                             public nsIDOMEventListener
{
  WorkerPrivate* mWorkerPrivate;
  nsRefPtr<Proxy> mProxy;
  nsRefPtr<nsXMLHttpRequest> mXHR;
  XMLHttpRequest* mXMLHttpRequestPrivate;
  nsString mEventType;
  bool mReceivedLoadStart;
  uint32_t mChannelId;

  class ProxyCompleteRunnable : public MainThreadProxyRunnable
  {
    XMLHttpRequest* mXMLHttpRequestPrivate;
    uint32_t mChannelId;

  public:
    ProxyCompleteRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                          XMLHttpRequest* aXHRPrivate, uint32_t aChannelId)
    : MainThreadProxyRunnable(aWorkerPrivate, RunWhenClearing, aProxy),
      mXMLHttpRequestPrivate(aXHRPrivate), mChannelId(aChannelId)
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
      if (mChannelId != mProxy->mOuterChannelId) {
        // Threads raced, this event is now obsolete.
        return true;
      }

      if (mSyncQueueKey != UINT32_MAX) {
        aWorkerPrivate->StopSyncLoop(mSyncQueueKey, true);
      }

      mXMLHttpRequestPrivate->Unpin();

      return true;
    }
  };

public:
  NS_DECL_ISUPPORTS

  LoadStartDetectionRunnable(Proxy* aProxy, XMLHttpRequest* aXHRPrivate)
  : mWorkerPrivate(aProxy->mWorkerPrivate), mProxy(aProxy), mXHR(aProxy->mXHR),
    mXMLHttpRequestPrivate(aXHRPrivate), mReceivedLoadStart(false),
    mChannelId(mProxy->mInnerChannelId)
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
      if (mProxy->mOutstandingSendCount > 1) {
        mProxy->mOutstandingSendCount--;
      } else if (mProxy->mOutstandingSendCount == 1) {
        mProxy->Reset();

        nsRefPtr<ProxyCompleteRunnable> runnable =
          new ProxyCompleteRunnable(mWorkerPrivate, mProxy,
                                    mXMLHttpRequestPrivate,
                                    mChannelId);
        if (runnable->Dispatch(nullptr)) {
          mProxy->mWorkerPrivate = nullptr;
          mProxy->mOutstandingSendCount--;
        }
      }
    }

    mProxy = nullptr;
    mXHR = nullptr;
    mXMLHttpRequestPrivate = nullptr;
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
  nsString mResponseType;
  JSAutoStructuredCloneBuffer mResponseBuffer;
  nsTArray<nsCOMPtr<nsISupports> > mClonedObjects;
  JS::Heap<JS::Value> mResponse;
  nsString mResponseText;
  nsCString mStatusText;
  uint64_t mLoaded;
  uint64_t mTotal;
  uint32_t mEventStreamId;
  uint32_t mStatus;
  uint16_t mReadyState;
  bool mUploadEvent;
  bool mProgressEvent;
  bool mLengthComputable;
  nsresult mResponseTextResult;
  nsresult mStatusResult;
  nsresult mResponseResult;

public:
  EventRunnable(Proxy* aProxy, bool aUploadEvent, const nsString& aType,
                bool aLengthComputable, uint64_t aLoaded, uint64_t aTotal)
  : MainThreadProxyRunnable(aProxy->mWorkerPrivate, SkipWhenClearing, aProxy),
    mType(aType), mResponse(JSVAL_VOID), mLoaded(aLoaded), mTotal(aTotal),
    mEventStreamId(aProxy->mInnerEventStreamId), mStatus(0), mReadyState(0),
    mUploadEvent(aUploadEvent), mProgressEvent(true),
    mLengthComputable(aLengthComputable), mResponseTextResult(NS_OK),
    mStatusResult(NS_OK), mResponseResult(NS_OK)
  { }

  EventRunnable(Proxy* aProxy, bool aUploadEvent, const nsString& aType)
  : MainThreadProxyRunnable(aProxy->mWorkerPrivate, SkipWhenClearing, aProxy),
    mType(aType), mResponse(JSVAL_VOID), mLoaded(0), mTotal(0),
    mEventStreamId(aProxy->mInnerEventStreamId), mStatus(0), mReadyState(0),
    mUploadEvent(aUploadEvent), mProgressEvent(false), mLengthComputable(0),
    mResponseTextResult(NS_OK), mStatusResult(NS_OK), mResponseResult(NS_OK)
  { }

  bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    nsRefPtr<nsXMLHttpRequest>& xhr = mProxy->mXHR;
    NS_ASSERTION(xhr, "Must have an XHR here!");

    if (NS_FAILED(xhr->GetResponseType(mResponseType))) {
      NS_ERROR("This should never fail!");
    }

    mResponseTextResult = xhr->GetResponseText(mResponseText);
    if (NS_SUCCEEDED(mResponseTextResult)) {
      mResponseResult = mResponseTextResult;
      if (mResponseText.IsVoid()) {
        mResponse = JSVAL_NULL;
      }
    }
    else {
      JS::Rooted<JS::Value> response(aCx);
      mResponseResult = xhr->GetResponse(aCx, response.address());
      if (NS_SUCCEEDED(mResponseResult)) {
        if (JSVAL_IS_UNIVERSAL(response)) {
          mResponse = response;
        }
        else {
          // Anything subject to GC must be cloned.
          JSStructuredCloneCallbacks* callbacks =
            aWorkerPrivate->IsChromeWorker() ?
            ChromeWorkerStructuredCloneCallbacks(true) :
            WorkerStructuredCloneCallbacks(true);

          nsTArray<nsCOMPtr<nsISupports> > clonedObjects;

          if (mResponseBuffer.write(aCx, response, callbacks, &clonedObjects)) {
            mClonedObjects.SwapElements(clonedObjects);
          }
          else {
            NS_WARNING("Failed to clone response!");
            mResponseResult = NS_ERROR_DOM_DATA_CLONE_ERR;
          }
        }
      }
    }

    mStatusResult = xhr->GetStatus(&mStatus);

    xhr->GetStatusText(mStatusText);

    mReadyState = xhr->ReadyState();

    return true;
  }

  class StateDataAutoRooter : private JS::CustomAutoRooter
  {
  public:
    explicit StateDataAutoRooter(JSContext* aCx, XMLHttpRequest::StateData* aData
                                 MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : CustomAutoRooter(aCx), mStateData(aData), mSkip(aCx, mStateData)
    {
      MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

  private:
    virtual void trace(JSTracer* aTrc)
    {
      JS_CallHeapValueTracer(aTrc, &mStateData->mResponse,
                             "XMLHttpRequest::StateData::mResponse");
    }

    XMLHttpRequest::StateData* mStateData;
    js::SkipRoot mSkip;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  };

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    if (mEventStreamId != mProxy->mOuterEventStreamId) {
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
    else if (mType.EqualsASCII(sEventStrings[STRING_abort])) {
      if ((mUploadEvent && !mProxy->mSeenUploadLoadStart) ||
          (!mUploadEvent && !mProxy->mSeenLoadStart)) {
        // We've already dispatched premature abort events.
        return true;
      }
    }
    else if (mType.EqualsASCII(sEventStrings[STRING_readystatechange])) {
      if (mReadyState == 4 && !mUploadEvent && !mProxy->mSeenLoadStart) {
        // We've already dispatched premature abort events.
        return true;
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

    nsAutoPtr<XMLHttpRequest::StateData> state(new XMLHttpRequest::StateData());
    StateDataAutoRooter rooter(aCx, state);

    state->mResponseTextResult = mResponseTextResult;
    state->mResponseText = mResponseText;

    if (NS_SUCCEEDED(mResponseTextResult)) {
      MOZ_ASSERT(JSVAL_IS_VOID(mResponse) || JSVAL_IS_NULL(mResponse));
      state->mResponseResult = mResponseTextResult;
      state->mResponse = mResponse;
    }
    else {
      state->mResponseResult = mResponseResult;

      if (NS_SUCCEEDED(mResponseResult)) {
        if (mResponseBuffer.data()) {
          MOZ_ASSERT(JSVAL_IS_VOID(mResponse));

          JSAutoStructuredCloneBuffer responseBuffer;
          mResponseBuffer.swap(responseBuffer);

          JSStructuredCloneCallbacks* callbacks =
            aWorkerPrivate->IsChromeWorker() ?
            ChromeWorkerStructuredCloneCallbacks(false) :
            WorkerStructuredCloneCallbacks(false);

          nsTArray<nsCOMPtr<nsISupports> > clonedObjects;
          clonedObjects.SwapElements(mClonedObjects);

          JS::Rooted<JS::Value> response(aCx);
          if (!responseBuffer.read(aCx, &response, callbacks, &clonedObjects)) {
            return false;
          }

          state->mResponse = response;
        }
        else {
          state->mResponse = mResponse;
        }
      }
    }

    state->mStatusResult = mStatusResult;
    state->mStatus = mStatus;

    state->mStatusText = mStatusText;

    state->mReadyState = mReadyState;

    XMLHttpRequest* xhr = mProxy->mXMLHttpRequestPrivate;
    xhr->UpdateState(*state);

    if (mUploadEvent && !xhr->GetUploadObjectNoCreate()) {
      return true;
    }

    JS::Rooted<JSString*> type(aCx, JS_NewUCStringCopyN(aCx, mType.get(), mType.Length()));
    if (!type) {
      return false;
    }

    nsXHREventTarget* target;
    if (mUploadEvent) {
      target = xhr->GetUploadObjectNoCreate();
    }
    else {
      target = xhr;
    }

    MOZ_ASSERT(target);

    nsCOMPtr<nsIDOMEvent> event;
    if (mProgressEvent) {
      NS_NewDOMProgressEvent(getter_AddRefs(event), target, nullptr, nullptr);
      nsCOMPtr<nsIDOMProgressEvent> progress = do_QueryInterface(event);

      if (progress) {
        progress->InitProgressEvent(mType, false, false, mLengthComputable,
                                    mLoaded, mTotal);
      }
    }
    else {
      NS_NewDOMEvent(getter_AddRefs(event), target, nullptr, nullptr);

      if (event) {
        event->InitEvent(mType, false, false);
      }
    }

    if (!event) {
      return false;
    }

    event->SetTrusted(true);

    target->DispatchDOMEvent(nullptr, event, nullptr, nullptr);

    // After firing the event set mResponse to JSVAL_NULL for chunked response
    // types.
    if (StringBeginsWith(mResponseType, NS_LITERAL_STRING("moz-chunked-"))) {
      xhr->NullResponseText();
    }

    return true;
  }
};

class WorkerThreadProxySyncRunnable : public nsRunnable
{
protected:
  WorkerPrivate* mWorkerPrivate;
  nsRefPtr<Proxy> mProxy;
  uint32_t mSyncQueueKey;

private:
  class ResponseRunnable : public MainThreadProxyRunnable
  {
    uint32_t mSyncQueueKey;
    nsresult mErrorCode;

  public:
    ResponseRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                     uint32_t aSyncQueueKey, nsresult aErrorCode)
    : MainThreadProxyRunnable(aWorkerPrivate, SkipWhenClearing, aProxy),
      mSyncQueueKey(aSyncQueueKey), mErrorCode(aErrorCode)
    {
      NS_ASSERTION(aProxy, "Don't hand me a null proxy!");
    }

    bool
    WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
    {
      if (NS_FAILED(mErrorCode)) {
        Throw(aCx, mErrorCode);
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
  }

  bool
  Dispatch(JSContext* aCx)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();

    AutoSyncLoopHolder syncLoop(mWorkerPrivate);
    mSyncQueueKey = syncLoop.SyncQueueKey();

    if (NS_FAILED(NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL))) {
      JS_ReportError(aCx, "Failed to dispatch to main thread!");
      return false;
    }

    return syncLoop.RunAndForget(aCx);
  }

  virtual nsresult
  MainThreadRun() = 0;

  NS_IMETHOD
  Run()
  {
    AssertIsOnMainThread();

    uint32_t oldSyncQueueKey = mProxy->mSyncEventResponseSyncQueueKey;
    mProxy->mSyncEventResponseSyncQueueKey = mSyncQueueKey;

    nsresult rv = MainThreadRun();

    nsRefPtr<ResponseRunnable> response =
      new ResponseRunnable(mWorkerPrivate, mProxy, mSyncQueueKey, rv);
    if (!response->Dispatch(nullptr)) {
      NS_WARNING("Failed to dispatch response!");
    }

    mProxy->mSyncEventResponseSyncQueueKey = oldSyncQueueKey;

    return NS_OK;
  }
};

class SyncTeardownRunnable : public WorkerThreadProxySyncRunnable
{
public:
  SyncTeardownRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy)
  {
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aProxy);
  }

  virtual nsresult
  MainThreadRun()
  {
    AssertIsOnMainThread();

    mProxy->Teardown();

    return NS_OK;
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

  nsresult
  MainThreadRun()
  {
    return mProxy->mXHR->SetMozBackgroundRequest(mValue);
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

  nsresult
  MainThreadRun()
  {
    return mProxy->mXHR->SetWithCredentials(mValue);
  }
};

class SetResponseTypeRunnable : public WorkerThreadProxySyncRunnable
{
  nsString mResponseType;

public:
  SetResponseTypeRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                          const nsAString& aResponseType)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy),
    mResponseType(aResponseType)
  { }

  nsresult
  MainThreadRun()
  {
    nsresult rv = mProxy->mXHR->SetResponseType(mResponseType);
    mResponseType.Truncate();
    if (NS_SUCCEEDED(rv)) {
      rv = mProxy->mXHR->GetResponseType(mResponseType);
    }
    return rv;
  }

  void
  GetResponseType(nsAString& aResponseType) {
    aResponseType.Assign(mResponseType);
  }
};

class SetTimeoutRunnable : public WorkerThreadProxySyncRunnable
{
  uint32_t mTimeout;

public:
  SetTimeoutRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                     uint32_t aTimeout)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy),
    mTimeout(aTimeout)
  { }

  nsresult
  MainThreadRun()
  {
    return mProxy->mXHR->SetTimeout(mTimeout);
  }
};

class AbortRunnable : public WorkerThreadProxySyncRunnable
{
public:
  AbortRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy)
  { }

  nsresult
  MainThreadRun()
  {
    mProxy->mInnerEventStreamId++;

    WorkerPrivate* oldWorker = mProxy->mWorkerPrivate;
    mProxy->mWorkerPrivate = mWorkerPrivate;

    mProxy->mXHR->Abort();

    mProxy->mWorkerPrivate = oldWorker;

    mProxy->Reset();

    return NS_OK;
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

  nsresult
  MainThreadRun()
  {
    mProxy->mXHR->GetAllResponseHeaders(mResponseHeaders);
    return NS_OK;
  }
};

class GetResponseHeaderRunnable : public WorkerThreadProxySyncRunnable
{
  const nsCString mHeader;
  nsCString& mValue;

public:
  GetResponseHeaderRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                            const nsACString& aHeader, nsCString& aValue)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy), mHeader(aHeader),
    mValue(aValue)
  { }

  nsresult
  MainThreadRun()
  {
    return mProxy->mXHR->GetResponseHeader(mHeader, mValue);
  }
};

class OpenRunnable : public WorkerThreadProxySyncRunnable
{
  nsCString mMethod;
  nsString mURL;
  Optional<nsAString> mUser;
  nsString mUserStr;
  Optional<nsAString> mPassword;
  nsString mPasswordStr;
  bool mBackgroundRequest;
  bool mWithCredentials;
  uint32_t mTimeout;

public:
  OpenRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
               const nsACString& aMethod, const nsAString& aURL,
               const Optional<nsAString>& aUser,
               const Optional<nsAString>& aPassword,
               bool aBackgroundRequest, bool aWithCredentials,
               uint32_t aTimeout)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy), mMethod(aMethod),
    mURL(aURL),
    mBackgroundRequest(aBackgroundRequest), mWithCredentials(aWithCredentials),
    mTimeout(aTimeout)
  {
    if (aUser.WasPassed()) {
      mUserStr = aUser.Value();
      mUser = &mUserStr;
    }
    if (aPassword.WasPassed()) {
      mPasswordStr = aPassword.Value();
      mPassword = &mPasswordStr;
    }
  }

  nsresult
  MainThreadRun()
  {
    WorkerPrivate* oldWorker = mProxy->mWorkerPrivate;
    mProxy->mWorkerPrivate = mWorkerPrivate;

    nsresult rv = MainThreadRunInternal();

    mProxy->mWorkerPrivate = oldWorker;
    return rv;
  }

  nsresult
  MainThreadRunInternal()
  {
    if (!mProxy->Init()) {
      return NS_ERROR_DOM_INVALID_STATE_ERR;
    }

    nsresult rv;

    if (mBackgroundRequest) {
      rv = mProxy->mXHR->SetMozBackgroundRequest(mBackgroundRequest);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (mWithCredentials) {
      rv = mProxy->mXHR->SetWithCredentials(mWithCredentials);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (mTimeout) {
      rv = mProxy->mXHR->SetTimeout(mTimeout);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    NS_ASSERTION(!mProxy->mInOpen, "Reentrancy is bad!");
    mProxy->mInOpen = true;

    ErrorResult rv2;
    mProxy->mXHR->Open(mMethod, mURL, true, mUser, mPassword, rv2);

    NS_ASSERTION(mProxy->mInOpen, "Reentrancy is bad!");
    mProxy->mInOpen = false;

    if (rv2.Failed()) {
      return rv2.ErrorCode();
    }

    rv = mProxy->mXHR->SetResponseType(NS_LITERAL_STRING("text"));

    return rv;
  }
};

class SendRunnable : public WorkerThreadProxySyncRunnable
{
  nsString mStringBody;
  JSAutoStructuredCloneBuffer mBody;
  nsTArray<nsCOMPtr<nsISupports> > mClonedObjects;
  uint32_t mSyncQueueKey;
  bool mHasUploadListeners;

public:
  SendRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
               const nsAString& aStringBody, JSAutoStructuredCloneBuffer& aBody,
               nsTArray<nsCOMPtr<nsISupports> >& aClonedObjects,
               uint32_t aSyncQueueKey, bool aHasUploadListeners)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy),
    mStringBody(aStringBody), mSyncQueueKey(aSyncQueueKey),
    mHasUploadListeners(aHasUploadListeners)
  {
    mBody.swap(aBody);
    mClonedObjects.SwapElements(aClonedObjects);
  }

  nsresult
  MainThreadRun()
  {
    nsCOMPtr<nsIVariant> variant;

    if (mBody.data()) {
      AutoSafeJSContext cx;
      JSAutoRequest ar(cx);
      nsIXPConnect* xpc = nsContentUtils::XPConnect();
      NS_ASSERTION(xpc, "This should never be null!");

      nsresult rv = NS_OK;

      JSStructuredCloneCallbacks* callbacks =
        mWorkerPrivate->IsChromeWorker() ?
        ChromeWorkerStructuredCloneCallbacks(true) :
        WorkerStructuredCloneCallbacks(true);

      JS::Rooted<JS::Value> body(cx);
      if (mBody.read(cx, &body, callbacks, &mClonedObjects)) {
        if (NS_FAILED(xpc->JSValToVariant(cx, body.address(),
                                          getter_AddRefs(variant)))) {
          rv = NS_ERROR_DOM_INVALID_STATE_ERR;
        }
      }
      else {
        rv = NS_ERROR_DOM_DATA_CLONE_ERR;
      }

      mBody.clear();
      mClonedObjects.Clear();

      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      nsCOMPtr<nsIWritableVariant> wvariant =
        do_CreateInstance(NS_VARIANT_CONTRACTID);
      NS_ENSURE_TRUE(wvariant, NS_ERROR_UNEXPECTED);

      if (NS_FAILED(wvariant->SetAsAString(mStringBody))) {
        NS_ERROR("This should never fail!");
      }

      variant = wvariant;
    }

    NS_ASSERTION(!mProxy->mWorkerPrivate, "Should be null!");
    mProxy->mWorkerPrivate = mWorkerPrivate;

    NS_ASSERTION(mProxy->mSyncQueueKey == UINT32_MAX, "Should be unset!");
    mProxy->mSyncQueueKey = mSyncQueueKey;

    if (mHasUploadListeners) {
      NS_ASSERTION(!mProxy->mUploadEventListenersAttached, "Huh?!");
      if (!mProxy->AddRemoveEventListeners(true, true)) {
        NS_ERROR("This should never fail!");
      }
    }

    mProxy->mInnerChannelId++;

    nsresult rv = mProxy->mXHR->Send(variant);

    if (NS_SUCCEEDED(rv)) {
      mProxy->mOutstandingSendCount++;

      if (!mHasUploadListeners) {
        NS_ASSERTION(!mProxy->mUploadEventListenersAttached, "Huh?!");
        if (!mProxy->AddRemoveEventListeners(true, true)) {
          NS_ERROR("This should never fail!");
        }
      }
    }

    return rv;
  }
};

class SetRequestHeaderRunnable : public WorkerThreadProxySyncRunnable
{
  nsCString mHeader;
  nsCString mValue;

public:
  SetRequestHeaderRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                           const nsACString& aHeader, const nsACString& aValue)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy), mHeader(aHeader),
    mValue(aValue)
  { }

  nsresult
  MainThreadRun()
  {
    return mProxy->mXHR->SetRequestHeader(mHeader, mValue);
  }
};

class OverrideMimeTypeRunnable : public WorkerThreadProxySyncRunnable
{
  nsString mMimeType;

public:
  OverrideMimeTypeRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                           const nsAString& aMimeType)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy), mMimeType(aMimeType)
  { }

  nsresult
  MainThreadRun()
  {
    mProxy->mXHR->OverrideMimeType(mMimeType);
    return NS_OK;
  }
};

class AutoUnpinXHR
{
public:
  AutoUnpinXHR(XMLHttpRequest* aXMLHttpRequestPrivate)
  : mXMLHttpRequestPrivate(aXMLHttpRequestPrivate)
  {
    MOZ_ASSERT(aXMLHttpRequestPrivate);
  }

  ~AutoUnpinXHR()
  {
    if (mXMLHttpRequestPrivate) {
      mXMLHttpRequestPrivate->Unpin();
    }
  }

  void Clear()
  {
    mXMLHttpRequestPrivate = nullptr;
  }

private:
  XMLHttpRequest* mXMLHttpRequestPrivate;
};

} // anonymous namespace

void
Proxy::Teardown()
{
  AssertIsOnMainThread();

  if (mXHR) {
    Reset();

    // NB: We are intentionally dropping events coming from xhr.abort on the
    // floor.
    AddRemoveEventListeners(false, false);
    mXHR->Abort();

    if (mOutstandingSendCount) {
      nsRefPtr<XHRUnpinRunnable> runnable =
        new XHRUnpinRunnable(mWorkerPrivate, mXMLHttpRequestPrivate);
      if (!runnable->Dispatch(nullptr)) {
        NS_RUNTIMEABORT("We're going to hang at shutdown anyways.");
      }

      mWorkerPrivate = nullptr;
      mOutstandingSendCount = 0;
    }

    mXHRUpload = nullptr;
    mXHR = nullptr;
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

  uint32_t lastEventType = aUpload ? STRING_LAST_EVENTTARGET : STRING_LAST_XHR;

  nsAutoString eventType;
  for (uint32_t index = 0; index <= lastEventType; index++) {
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

NS_IMPL_ISUPPORTS1(Proxy, nsIDOMEventListener)

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

  if (mInOpen && type.EqualsASCII(sEventStrings[STRING_readystatechange])) {
    uint16_t readyState = 0;
    if (NS_SUCCEEDED(mXHR->GetReadyState(&readyState)) &&
        readyState == nsIXMLHttpRequest::OPENED) {
      mInnerEventStreamId++;
    }
  }

  if (progressEvent) {
    bool lengthComputable;
    uint64_t loaded, total;
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

  {
    AutoSafeJSContext cx;
    JSAutoRequest ar(cx);
    runnable->Dispatch(cx);
  }

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

XMLHttpRequest::XMLHttpRequest(WorkerPrivate* aWorkerPrivate)
: mWorkerPrivate(aWorkerPrivate),
  mResponseType(XMLHttpRequestResponseType::Text), mTimeout(0),
  mRooted(false), mBackgroundRequest(false), mWithCredentials(false),
  mCanceled(false), mMozAnon(false), mMozSystem(false)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  SetIsDOMBinding();
}

XMLHttpRequest::~XMLHttpRequest()
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  ReleaseProxy(XHRIsGoingAway);

  MOZ_ASSERT(!mRooted);

  mozilla::DropJSObjects(this);
}

NS_IMPL_ADDREF_INHERITED(XMLHttpRequest, nsXHREventTarget)
NS_IMPL_RELEASE_INHERITED(XMLHttpRequest, nsXHREventTarget)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(XMLHttpRequest)
NS_INTERFACE_MAP_END_INHERITING(nsXHREventTarget)

NS_IMPL_CYCLE_COLLECTION_CLASS(XMLHttpRequest)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(XMLHttpRequest,
                                                  nsXHREventTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mUpload)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(XMLHttpRequest,
                                                nsXHREventTarget)
  tmp->ReleaseProxy(XHRIsGoingAway);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mUpload)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(XMLHttpRequest,
                                               nsXHREventTarget)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mStateData.mResponse)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

JSObject*
XMLHttpRequest::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return XMLHttpRequestBinding_workers::Wrap(aCx, aScope, this);
}

// static
already_AddRefed<XMLHttpRequest>
XMLHttpRequest::Constructor(const GlobalObject& aGlobal,
                            const MozXMLHttpRequestParameters& aParams,
                            ErrorResult& aRv)
{
  JSContext* cx = aGlobal.GetContext();
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);
  MOZ_ASSERT(workerPrivate);

  nsRefPtr<XMLHttpRequest> xhr = new XMLHttpRequest(workerPrivate);

  if (workerPrivate->XHRParamsAllowed()) {
    if (aParams.mMozSystem)
      xhr->mMozAnon = true;
    else
      xhr->mMozAnon = aParams.mMozAnon;
    xhr->mMozSystem = aParams.mMozSystem;
  }

  return xhr.forget();
}

void
XMLHttpRequest::ReleaseProxy(ReleaseType aType)
{
  // Can't assert that we're on the worker thread here because mWorkerPrivate
  // may be gone.

  if (mProxy) {
    if (aType == XHRIsGoingAway) {
      // We're in a GC finalizer, so we can't do a sync call here (and we don't
      // need to).
      nsRefPtr<AsyncTeardownRunnable> runnable =
        new AsyncTeardownRunnable(mProxy);
      mProxy = nullptr;

      if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
        NS_ERROR("Failed to dispatch teardown runnable!");
      }
    } else {
      // This isn't necessary if the worker is going away or the XHR is going
      // away.
      if (aType == Default) {
        // Don't let any more events run.
        mProxy->mOuterEventStreamId++;
      }

      // We need to make a sync call here.
      nsRefPtr<SyncTeardownRunnable> runnable =
        new SyncTeardownRunnable(mWorkerPrivate, mProxy);
      mProxy = nullptr;

      if (!runnable->Dispatch(nullptr)) {
        NS_ERROR("Failed to dispatch teardown runnable!");
      }
    }
  }
}

void
XMLHttpRequest::MaybePin(ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mRooted) {
    return;
  }

  JSContext* cx = GetCurrentThreadJSContext();

  if (!mWorkerPrivate->AddFeature(cx, this)) {
    return;
  }

  NS_ADDREF_THIS();

  mRooted = true;
}

void
XMLHttpRequest::MaybeDispatchPrematureAbortEvents(ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mProxy);

  mStateData.mReadyState = 4;

  if (mProxy->mSeenUploadLoadStart) {
    MOZ_ASSERT(mUpload);

    DispatchPrematureAbortEvent(mUpload, NS_LITERAL_STRING("abort"), true,
                                aRv);
    if (aRv.Failed()) {
      return;
    }

    DispatchPrematureAbortEvent(mUpload, NS_LITERAL_STRING("loadend"), true,
                                aRv);
    if (aRv.Failed()) {
      return;
    }

    mProxy->mSeenUploadLoadStart = false;
  }

  if (mProxy->mSeenLoadStart) {
    DispatchPrematureAbortEvent(this, NS_LITERAL_STRING("readystatechange"),
                                false, aRv);
    if (aRv.Failed()) {
      return;
    }

    DispatchPrematureAbortEvent(this, NS_LITERAL_STRING("abort"), false, aRv);
    if (aRv.Failed()) {
      return;
    }

    DispatchPrematureAbortEvent(this, NS_LITERAL_STRING("loadend"), false,
                                aRv);
    if (aRv.Failed()) {
      return;
    }

    mProxy->mSeenLoadStart = false;
  }
}

void
XMLHttpRequest::DispatchPrematureAbortEvent(EventTarget* aTarget,
                                            const nsAString& aEventType,
                                            bool aUploadTarget,
                                            ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(aTarget);

  if (!mProxy) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsCOMPtr<nsIDOMEvent> event;
  if (aEventType.EqualsLiteral("readystatechange")) {
    NS_NewDOMEvent(getter_AddRefs(event), aTarget, nullptr, nullptr);

    if (event) {
      event->InitEvent(aEventType, false, false);
    }
  }
  else {
    NS_NewDOMProgressEvent(getter_AddRefs(event), aTarget, nullptr, nullptr);

    nsCOMPtr<nsIDOMProgressEvent> progress = do_QueryInterface(event);
    if (progress) {
      if (aUploadTarget) {
        progress->InitProgressEvent(aEventType, false, false,
                                    mProxy->mLastUploadLengthComputable,
                                    mProxy->mLastUploadLoaded,
                                    mProxy->mLastUploadTotal);
      }
      else {
        progress->InitProgressEvent(aEventType, false, false,
                                    mProxy->mLastLengthComputable,
                                    mProxy->mLastLoaded,
                                    mProxy->mLastTotal);
      }
    }
  }

  if (!event) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  event->SetTrusted(true);

  aTarget->DispatchDOMEvent(nullptr, event, nullptr, nullptr);
}

void
XMLHttpRequest::Unpin()
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  MOZ_ASSERT(mRooted, "Mismatched calls to Unpin!");

  JSContext* cx = GetCurrentThreadJSContext();

  mWorkerPrivate->RemoveFeature(cx, this);

  mRooted = false;

  NS_RELEASE_THIS();
}

void
XMLHttpRequest::SendInternal(const nsAString& aStringBody,
                             JSAutoStructuredCloneBuffer& aBody,
                             nsTArray<nsCOMPtr<nsISupports> >& aClonedObjects,
                             ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  bool hasUploadListeners = mUpload ? mUpload->HasListeners() : false;

  MaybePin(aRv);
  if (aRv.Failed()) {
    return;
  }

  AutoUnpinXHR autoUnpin(this);
  Maybe<AutoSyncLoopHolder> autoSyncLoop;

  uint32_t syncQueueKey = UINT32_MAX;
  bool isSyncXHR = mProxy->mIsSyncXHR;
  if (isSyncXHR) {
    autoSyncLoop.construct(mWorkerPrivate);
    syncQueueKey = autoSyncLoop.ref().SyncQueueKey();
  }

  mProxy->mOuterChannelId++;

  JSContext* cx = mWorkerPrivate->GetJSContext();

  nsRefPtr<SendRunnable> runnable =
    new SendRunnable(mWorkerPrivate, mProxy, aStringBody, aBody,
                     aClonedObjects, syncQueueKey, hasUploadListeners);
  if (!runnable->Dispatch(cx)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (!isSyncXHR)  {
    autoUnpin.Clear();
    MOZ_ASSERT(autoSyncLoop.empty());
    return;
  }

  // If our sync XHR was canceled during the send call the worker is going
  // away.  We have no idea how far through the send call we got.  There may
  // be a ProxyCompleteRunnable in the sync loop, but rather than run the loop
  // to get it we just let our RAII helpers clean up.
  if (mCanceled) {
    return;
  }

  autoUnpin.Clear();

  if (!autoSyncLoop.ref().RunAndForget(cx)) {
    aRv.Throw(NS_ERROR_FAILURE);
  }
}

bool
XMLHttpRequest::Notify(JSContext* aCx, Status aStatus)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mWorkerPrivate->GetJSContext() == aCx);

  if (aStatus >= Canceling && !mCanceled) {
    mCanceled = true;
    ReleaseProxy(WorkerIsGoingAway);
  }

  return true;
}

void
XMLHttpRequest::Open(const nsACString& aMethod, const nsAString& aUrl,
                     bool aAsync, const Optional<nsAString>& aUser,
                     const Optional<nsAString>& aPassword, ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    aRv.Throw(UNCATCHABLE_EXCEPTION);
    return;
  }

  if (mProxy) {
    MaybeDispatchPrematureAbortEvents(aRv);
    if (aRv.Failed()) {
      return;
    }
  }
  else {
    mProxy = new Proxy(this, mMozAnon, mMozSystem);
  }

  mProxy->mOuterEventStreamId++;

  nsRefPtr<OpenRunnable> runnable =
    new OpenRunnable(mWorkerPrivate, mProxy, aMethod, aUrl, aUser, aPassword,
                     mBackgroundRequest, mWithCredentials,
                     mTimeout);

  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    ReleaseProxy();
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  mProxy->mIsSyncXHR = !aAsync;
}

void
XMLHttpRequest::SetRequestHeader(const nsACString& aHeader,
                                 const nsACString& aValue, ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    aRv.Throw(UNCATCHABLE_EXCEPTION);
    return;
  }

  if (!mProxy) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  nsRefPtr<SetRequestHeaderRunnable> runnable =
    new SetRequestHeaderRunnable(mWorkerPrivate, mProxy, aHeader, aValue);
  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
}

void
XMLHttpRequest::SetTimeout(uint32_t aTimeout, ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    aRv.Throw(UNCATCHABLE_EXCEPTION);
    return;
  }

  mTimeout = aTimeout;

  if (!mProxy) {
    // Open may not have been called yet, in which case we'll handle the
    // timeout in OpenRunnable.
    return;
  }

  nsRefPtr<SetTimeoutRunnable> runnable =
    new SetTimeoutRunnable(mWorkerPrivate, mProxy, aTimeout);
  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
}

void
XMLHttpRequest::SetWithCredentials(bool aWithCredentials, ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    aRv.Throw(UNCATCHABLE_EXCEPTION);
    return;
  }

  mWithCredentials = aWithCredentials;

  if (!mProxy) {
    // Open may not have been called yet, in which case we'll handle the
    // credentials in OpenRunnable.
    return;
  }

  nsRefPtr<SetWithCredentialsRunnable> runnable =
    new SetWithCredentialsRunnable(mWorkerPrivate, mProxy, aWithCredentials);
  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
}

void
XMLHttpRequest::SetMozBackgroundRequest(bool aBackgroundRequest,
                                        ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    aRv.Throw(UNCATCHABLE_EXCEPTION);
    return;
  }

  mBackgroundRequest = aBackgroundRequest;

  if (!mProxy) {
    // Open may not have been called yet, in which case we'll handle the
    // background request in OpenRunnable.
    return;
  }

  nsRefPtr<SetBackgroundRequestRunnable> runnable =
    new SetBackgroundRequestRunnable(mWorkerPrivate, mProxy,
                                     aBackgroundRequest);
  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
}

XMLHttpRequestUpload*
XMLHttpRequest::GetUpload(ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    aRv.Throw(UNCATCHABLE_EXCEPTION);
    return nullptr;
  }

  if (!mUpload) {
    mUpload = XMLHttpRequestUpload::Create(this);

    if (!mUpload) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
  }

  return mUpload;
}

void
XMLHttpRequest::Send(ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    aRv.Throw(UNCATCHABLE_EXCEPTION);
    return;
  }

  if (!mProxy) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  // Nothing to clone.
  JSAutoStructuredCloneBuffer buffer;
  nsTArray<nsCOMPtr<nsISupports> > clonedObjects;

  SendInternal(NullString(), buffer, clonedObjects, aRv);
}

void
XMLHttpRequest::Send(const nsAString& aBody, ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    aRv.Throw(UNCATCHABLE_EXCEPTION);
    return;
  }

  if (!mProxy) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  // Nothing to clone.
  JSAutoStructuredCloneBuffer buffer;
  nsTArray<nsCOMPtr<nsISupports> > clonedObjects;

  SendInternal(aBody, buffer, clonedObjects, aRv);
}

void
XMLHttpRequest::Send(JSObject* aBody, ErrorResult& aRv)
{
  JSContext* cx = mWorkerPrivate->GetJSContext();

  MOZ_ASSERT(aBody);
  JS::Rooted<JSObject*> body(cx, aBody);

  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    aRv.Throw(UNCATCHABLE_EXCEPTION);
    return;
  }

  if (!mProxy) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  JS::Rooted<JS::Value> valToClone(cx);
  if (JS_IsArrayBufferObject(body) || JS_IsArrayBufferViewObject(body) ||
      file::GetDOMBlobFromJSObject(body)) {
    valToClone.setObject(*body);
  }
  else {
    JS::Rooted<JS::Value> obj(cx, JS::ObjectValue(*body));
    JSString* bodyStr = JS::ToString(cx, obj);
    if (!bodyStr) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    valToClone.setString(bodyStr);
  }

  JSStructuredCloneCallbacks* callbacks =
    mWorkerPrivate->IsChromeWorker() ?
    ChromeWorkerStructuredCloneCallbacks(false) :
    WorkerStructuredCloneCallbacks(false);

  nsTArray<nsCOMPtr<nsISupports> > clonedObjects;

  JSAutoStructuredCloneBuffer buffer;
  if (!buffer.write(cx, valToClone, callbacks, &clonedObjects)) {
    aRv.Throw(NS_ERROR_DOM_DATA_CLONE_ERR);
    return;
  }

  SendInternal(EmptyString(), buffer, clonedObjects, aRv);
}

void
XMLHttpRequest::SendAsBinary(const nsAString& aBody, ErrorResult& aRv)
{
  NS_NOTYETIMPLEMENTED("Implement me!");
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return;
}

void
XMLHttpRequest::Abort(ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    aRv.Throw(UNCATCHABLE_EXCEPTION);
  }

  if (!mProxy) {
    return;
  }

  MaybeDispatchPrematureAbortEvents(aRv);
  if (aRv.Failed()) {
    return;
  }

  mProxy->mOuterEventStreamId++;

  nsRefPtr<AbortRunnable> runnable = new AbortRunnable(mWorkerPrivate, mProxy);
  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
}

void
XMLHttpRequest::GetResponseHeader(const nsACString& aHeader,
                                  nsACString& aResponseHeader, ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    aRv.Throw(UNCATCHABLE_EXCEPTION);
    return;
  }

  if (!mProxy) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  nsCString responseHeader;
  nsRefPtr<GetResponseHeaderRunnable> runnable =
    new GetResponseHeaderRunnable(mWorkerPrivate, mProxy, aHeader,
                                  responseHeader);
  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  aResponseHeader = responseHeader;
}

void
XMLHttpRequest::GetAllResponseHeaders(nsACString& aResponseHeaders,
                                      ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    aRv.Throw(UNCATCHABLE_EXCEPTION);
    return;
  }

  if (!mProxy) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  nsCString responseHeaders;
  nsRefPtr<GetAllResponseHeadersRunnable> runnable =
    new GetAllResponseHeadersRunnable(mWorkerPrivate, mProxy, responseHeaders);
  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  aResponseHeaders = responseHeaders;
}

void
XMLHttpRequest::OverrideMimeType(const nsAString& aMimeType, ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    aRv.Throw(UNCATCHABLE_EXCEPTION);
    return;
  }

  // We're supposed to throw if the state is not OPENED or HEADERS_RECEIVED. We
  // can detect OPENED really easily but we can't detect HEADERS_RECEIVED in a
  // non-racy way until the XHR state machine actually runs on this thread
  // (bug 671047). For now we're going to let this work only if the Send()
  // method has not been called.
  if (!mProxy || SendInProgress()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  nsRefPtr<OverrideMimeTypeRunnable> runnable =
    new OverrideMimeTypeRunnable(mWorkerPrivate, mProxy, aMimeType);
  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
}

void
XMLHttpRequest::SetResponseType(XMLHttpRequestResponseType aResponseType,
                                ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    aRv.Throw(UNCATCHABLE_EXCEPTION);
    return;
  }

  if (!mProxy || SendInProgress()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  // "document" is fine for the main thread but not for a worker. Short-circuit
  // that here.
  if (aResponseType == XMLHttpRequestResponseType::Document) {
    return;
  }

  nsString responseType;
  ConvertResponseTypeToString(aResponseType, responseType);

  nsRefPtr<SetResponseTypeRunnable> runnable =
    new SetResponseTypeRunnable(mWorkerPrivate, mProxy, responseType);
  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsString acceptedResponseTypeString;
  runnable->GetResponseType(acceptedResponseTypeString);

  mResponseType = ConvertStringToResponseType(acceptedResponseTypeString);
}

jsval
XMLHttpRequest::GetResponse(JSContext* /* unused */, ErrorResult& aRv)
{
  if (NS_SUCCEEDED(mStateData.mResponseTextResult) &&
      JSVAL_IS_VOID(mStateData.mResponse)) {
    MOZ_ASSERT(mStateData.mResponseText.Length());
    MOZ_ASSERT(NS_SUCCEEDED(mStateData.mResponseResult));

    JSString* str =
      JS_NewUCStringCopyN(mWorkerPrivate->GetJSContext(),
                          mStateData.mResponseText.get(),
                          mStateData.mResponseText.Length());
    if (!str) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return JSVAL_VOID;
    }

    mStateData.mResponse = STRING_TO_JSVAL(str);
  }

  aRv = mStateData.mResponseResult;
  return mStateData.mResponse;
}

void
XMLHttpRequest::GetResponseText(nsAString& aResponseText, ErrorResult& aRv)
{
  aRv = mStateData.mResponseTextResult;
  aResponseText = mStateData.mResponseText;
}

void
XMLHttpRequest::UpdateState(const StateData& aStateData)
{
  mStateData = aStateData;
  if (JSVAL_IS_GCTHING(mStateData.mResponse)) {
    mozilla::HoldJSObjects(this);
  }
}
