/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XMLHttpRequest.h"

#include "nsIDOMEvent.h"
#include "nsIDOMEventListener.h"
#include "nsIRunnable.h"
#include "nsIVariant.h"
#include "nsIXMLHttpRequest.h"
#include "nsIXPConnect.h"

#include "jsfriendapi.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/ProgressEvent.h"
#include "mozilla/dom/StructuredCloneHelper.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsFormData.h"
#include "nsJSUtils.h"
#include "nsThreadUtils.h"

#include "RuntimeService.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "XMLHttpRequestUpload.h"

using namespace mozilla;

using namespace mozilla::dom;
USING_WORKERS_NAMESPACE

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

class Proxy final : public nsIDOMEventListener
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
  nsCOMPtr<nsIEventTarget> mSyncLoopTarget;
  nsCOMPtr<nsIEventTarget> mSyncEventResponseTarget;
  uint32_t mInnerEventStreamId;
  uint32_t mInnerChannelId;
  uint32_t mOutstandingSendCount;

  // Only touched on the worker thread.
  uint32_t mOuterEventStreamId;
  uint32_t mOuterChannelId;
  uint32_t mOpenCount;
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
  bool mUploadEventListenersAttached;
  bool mMainThreadSeenLoadStart;
  bool mInOpen;
  bool mArrayBufferResponseWasTransferred;

public:
  Proxy(XMLHttpRequest* aXHRPrivate, bool aMozAnon, bool aMozSystem)
  : mWorkerPrivate(nullptr), mXMLHttpRequestPrivate(aXHRPrivate),
    mMozAnon(aMozAnon), mMozSystem(aMozSystem),
    mInnerEventStreamId(0), mInnerChannelId(0), mOutstandingSendCount(0),
    mOuterEventStreamId(0), mOuterChannelId(0), mOpenCount(0), mLastLoaded(0),
    mLastTotal(0), mLastUploadLoaded(0), mLastUploadTotal(0), mIsSyncXHR(false),
    mLastLengthComputable(false), mLastUploadLengthComputable(false),
    mSeenLoadStart(false), mSeenUploadLoadStart(false),
    mUploadEventListenersAttached(false), mMainThreadSeenLoadStart(false),
    mInOpen(false), mArrayBufferResponseWasTransferred(false)
  { }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  bool
  Init();

  void
  Teardown(bool aSendUnpin);

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

  already_AddRefed<nsIEventTarget>
  GetEventTarget()
  {
    AssertIsOnMainThread();

    nsCOMPtr<nsIEventTarget> target = mSyncEventResponseTarget ?
                                      mSyncEventResponseTarget :
                                      mSyncLoopTarget;
    return target.forget();
  }

private:
  ~Proxy()
  {
    MOZ_ASSERT(!mXHR);
    MOZ_ASSERT(!mXHRUpload);
    MOZ_ASSERT(!mOutstandingSendCount);
  }
};

class WorkerThreadProxySyncRunnable : public nsRunnable
{
protected:
  WorkerPrivate* mWorkerPrivate;
  nsRefPtr<Proxy> mProxy;
  nsCOMPtr<nsIEventTarget> mSyncLoopTarget;

private:
  class ResponseRunnable final: public MainThreadStopSyncLoopRunnable
  {
    nsRefPtr<Proxy> mProxy;
    nsresult mErrorCode;

  public:
    ResponseRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                     nsresult aErrorCode)
    : MainThreadStopSyncLoopRunnable(aWorkerPrivate, aProxy->GetEventTarget(),
                                     NS_SUCCEEDED(aErrorCode)),
      mProxy(aProxy), mErrorCode(aErrorCode)
    {
      MOZ_ASSERT(aProxy);
    }

  private:
    ~ResponseRunnable()
    { }

    virtual void
    MaybeSetException(JSContext* aCx) override
    {
      MOZ_ASSERT(NS_FAILED(mErrorCode));

      Throw(aCx, mErrorCode);
    }
  };

public:
  WorkerThreadProxySyncRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy)
  : mWorkerPrivate(aWorkerPrivate), mProxy(aProxy)
  {
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aProxy);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  NS_DECL_ISUPPORTS_INHERITED

  bool
  Dispatch(JSContext* aCx)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();

    AutoSyncLoopHolder syncLoop(mWorkerPrivate);
    mSyncLoopTarget = syncLoop.EventTarget();

    if (NS_FAILED(NS_DispatchToMainThread(this))) {
      JS_ReportError(aCx, "Failed to dispatch to main thread!");
      return false;
    }

    return syncLoop.Run();
  }

protected:
  virtual ~WorkerThreadProxySyncRunnable()
  { }

  virtual nsresult
  MainThreadRun() = 0;

private:
  NS_DECL_NSIRUNNABLE
};

class SendRunnable final
  : public WorkerThreadProxySyncRunnable
  , public StructuredCloneHelper
{
  nsString mStringBody;
  nsCOMPtr<nsIEventTarget> mSyncLoopTarget;
  bool mHasUploadListeners;

public:
  SendRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
               const nsAString& aStringBody)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy)
  , StructuredCloneHelper(CloningSupported, TransferringNotSupported)
  , mStringBody(aStringBody)
  , mHasUploadListeners(false)
  {
  }

  void SetHaveUploadListeners(bool aHasUploadListeners)
  {
    mHasUploadListeners = aHasUploadListeners;
  }

  void SetSyncLoopTarget(nsIEventTarget* aSyncLoopTarget)
  {
    mSyncLoopTarget = aSyncLoopTarget;
  }

private:
  ~SendRunnable()
  { }

  virtual nsresult
  MainThreadRun() override;
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

static_assert(STRING_LAST_XHR >= STRING_LAST_EVENTTARGET, "Bad string setup!");
static_assert(STRING_LAST_XHR == STRING_COUNT - 1, "Bad string setup!");

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

static_assert(MOZ_ARRAY_LENGTH(sEventStrings) == STRING_COUNT,
              "Bad string count!");

class MainThreadProxyRunnable : public MainThreadWorkerSyncRunnable
{
protected:
  nsRefPtr<Proxy> mProxy;

  MainThreadProxyRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy)
  : MainThreadWorkerSyncRunnable(aWorkerPrivate, aProxy->GetEventTarget()),
    mProxy(aProxy)
  {
    MOZ_ASSERT(aProxy);
  }

  virtual ~MainThreadProxyRunnable()
  { }
};

class XHRUnpinRunnable final : public MainThreadWorkerControlRunnable
{
  XMLHttpRequest* mXMLHttpRequestPrivate;

public:
  XHRUnpinRunnable(WorkerPrivate* aWorkerPrivate,
                   XMLHttpRequest* aXHRPrivate)
  : MainThreadWorkerControlRunnable(aWorkerPrivate),
    mXMLHttpRequestPrivate(aXHRPrivate)
  {
    MOZ_ASSERT(aXHRPrivate);
  }

private:
  ~XHRUnpinRunnable()
  { }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    if (mXMLHttpRequestPrivate->SendInProgress()) {
      mXMLHttpRequestPrivate->Unpin();
    }

    return true;
  }
};

class AsyncTeardownRunnable final : public nsRunnable
{
  nsRefPtr<Proxy> mProxy;

public:
  explicit AsyncTeardownRunnable(Proxy* aProxy)
  : mProxy(aProxy)
  {
    MOZ_ASSERT(aProxy);
  }

  NS_DECL_ISUPPORTS_INHERITED

private:
  ~AsyncTeardownRunnable()
  { }

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();

    // This means the XHR was GC'd, so we can't be pinned, and we don't need to
    // try to unpin.
    mProxy->Teardown(/* aSendUnpin */ false);
    mProxy = nullptr;

    return NS_OK;
  }
};

class LoadStartDetectionRunnable final : public nsRunnable,
                                         public nsIDOMEventListener
{
  WorkerPrivate* mWorkerPrivate;
  nsRefPtr<Proxy> mProxy;
  nsRefPtr<nsXMLHttpRequest> mXHR;
  XMLHttpRequest* mXMLHttpRequestPrivate;
  nsString mEventType;
  uint32_t mChannelId;
  bool mReceivedLoadStart;

  class ProxyCompleteRunnable final : public MainThreadProxyRunnable
  {
    XMLHttpRequest* mXMLHttpRequestPrivate;
    uint32_t mChannelId;

  public:
    ProxyCompleteRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                          XMLHttpRequest* aXHRPrivate, uint32_t aChannelId)
    : MainThreadProxyRunnable(aWorkerPrivate, aProxy),
      mXMLHttpRequestPrivate(aXHRPrivate), mChannelId(aChannelId)
    { }

  private:
    ~ProxyCompleteRunnable()
    { }

    virtual bool
    WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
    {
      if (mChannelId != mProxy->mOuterChannelId) {
        // Threads raced, this event is now obsolete.
        return true;
      }

      if (mSyncLoopTarget) {
        aWorkerPrivate->StopSyncLoop(mSyncLoopTarget, true);
      }

      if (mXMLHttpRequestPrivate->SendInProgress()) {
        mXMLHttpRequestPrivate->Unpin();
      }

      return true;
    }

    NS_IMETHOD
    Cancel() override
    {
      // This must run!
      nsresult rv = MainThreadProxyRunnable::Cancel();
      nsresult rv2 = Run();
      return NS_FAILED(rv) ? rv : rv2;
    }
  };

public:
  LoadStartDetectionRunnable(Proxy* aProxy, XMLHttpRequest* aXHRPrivate)
  : mWorkerPrivate(aProxy->mWorkerPrivate), mProxy(aProxy), mXHR(aProxy->mXHR),
    mXMLHttpRequestPrivate(aXHRPrivate), mChannelId(mProxy->mInnerChannelId),
    mReceivedLoadStart(false)
  {
    AssertIsOnMainThread();
    mEventType.AssignWithConversion(sEventStrings[STRING_loadstart]);
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIDOMEVENTLISTENER

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

private:
  ~LoadStartDetectionRunnable()
  {
    AssertIsOnMainThread();
    }
};

class EventRunnable final : public MainThreadProxyRunnable
                          , public StructuredCloneHelper
{
  nsString mType;
  nsString mResponseType;
  JS::Heap<JS::Value> mResponse;
  nsString mResponseText;
  nsString mResponseURL;
  nsCString mStatusText;
  uint64_t mLoaded;
  uint64_t mTotal;
  uint32_t mEventStreamId;
  uint32_t mStatus;
  uint16_t mReadyState;
  bool mUploadEvent;
  bool mProgressEvent;
  bool mLengthComputable;
  bool mUseCachedArrayBufferResponse;
  nsresult mResponseTextResult;
  nsresult mStatusResult;
  nsresult mResponseResult;

public:
  class StateDataAutoRooter : private JS::CustomAutoRooter
  {
    XMLHttpRequest::StateData* mStateData;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:
    explicit StateDataAutoRooter(JSContext* aCx, XMLHttpRequest::StateData* aData
                                 MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : CustomAutoRooter(aCx), mStateData(aData)
    {
      MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

  private:
    virtual void trace(JSTracer* aTrc)
    {
      JS_CallValueTracer(aTrc, &mStateData->mResponse,
                         "XMLHttpRequest::StateData::mResponse");
    }
  };

  EventRunnable(Proxy* aProxy, bool aUploadEvent, const nsString& aType,
                bool aLengthComputable, uint64_t aLoaded, uint64_t aTotal)
  : MainThreadProxyRunnable(aProxy->mWorkerPrivate, aProxy),
    StructuredCloneHelper(CloningSupported, TransferringNotSupported),
    mType(aType), mResponse(JS::UndefinedValue()), mLoaded(aLoaded),
    mTotal(aTotal), mEventStreamId(aProxy->mInnerEventStreamId), mStatus(0),
    mReadyState(0), mUploadEvent(aUploadEvent), mProgressEvent(true),
    mLengthComputable(aLengthComputable), mUseCachedArrayBufferResponse(false),
    mResponseTextResult(NS_OK), mStatusResult(NS_OK), mResponseResult(NS_OK)
  { }

  EventRunnable(Proxy* aProxy, bool aUploadEvent, const nsString& aType)
  : MainThreadProxyRunnable(aProxy->mWorkerPrivate, aProxy),
    StructuredCloneHelper(CloningSupported, TransferringNotSupported),
    mType(aType), mResponse(JS::UndefinedValue()), mLoaded(0), mTotal(0),
    mEventStreamId(aProxy->mInnerEventStreamId), mStatus(0), mReadyState(0),
    mUploadEvent(aUploadEvent), mProgressEvent(false), mLengthComputable(0),
    mUseCachedArrayBufferResponse(false), mResponseTextResult(NS_OK),
    mStatusResult(NS_OK), mResponseResult(NS_OK)
  { }

private:
  ~EventRunnable()
  { }

  virtual bool
  PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override;

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override;
};

class SyncTeardownRunnable final : public WorkerThreadProxySyncRunnable
{
public:
  SyncTeardownRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy)
  { }

private:
  ~SyncTeardownRunnable()
  { }

  virtual nsresult
  MainThreadRun() override
  {
    mProxy->Teardown(/* aSendUnpin */ true);
    MOZ_ASSERT(!mProxy->mSyncLoopTarget);
    return NS_OK;
  }
};

class SetBackgroundRequestRunnable final :
  public WorkerThreadProxySyncRunnable
{
  bool mValue;

public:
  SetBackgroundRequestRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                               bool aValue)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy), mValue(aValue)
  { }

private:
  ~SetBackgroundRequestRunnable()
  { }

  virtual nsresult
  MainThreadRun() override
  {
    return mProxy->mXHR->SetMozBackgroundRequest(mValue);
  }
};

class SetWithCredentialsRunnable final :
  public WorkerThreadProxySyncRunnable
{
  bool mValue;

public:
  SetWithCredentialsRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                             bool aValue)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy), mValue(aValue)
  { }

private:
  ~SetWithCredentialsRunnable()
  { }

  virtual nsresult
  MainThreadRun() override
  {
    return mProxy->mXHR->SetWithCredentials(mValue);
  }
};

class SetResponseTypeRunnable final : public WorkerThreadProxySyncRunnable
{
  nsString mResponseType;

public:
  SetResponseTypeRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                          const nsAString& aResponseType)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy),
    mResponseType(aResponseType)
  { }

  void
  GetResponseType(nsAString& aResponseType)
  {
    aResponseType.Assign(mResponseType);
  }

private:
  ~SetResponseTypeRunnable()
  { }

  virtual nsresult
  MainThreadRun() override
  {
    nsresult rv = mProxy->mXHR->SetResponseType(mResponseType);
    mResponseType.Truncate();
    if (NS_SUCCEEDED(rv)) {
      rv = mProxy->mXHR->GetResponseType(mResponseType);
    }
    return rv;
  }
};

class SetTimeoutRunnable final : public WorkerThreadProxySyncRunnable
{
  uint32_t mTimeout;

public:
  SetTimeoutRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                     uint32_t aTimeout)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy), mTimeout(aTimeout)
  { }

private:
  ~SetTimeoutRunnable()
  { }

  virtual nsresult
  MainThreadRun() override
  {
    return mProxy->mXHR->SetTimeout(mTimeout);
  }
};

class AbortRunnable final : public WorkerThreadProxySyncRunnable
{
public:
  AbortRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy)
  { }

private:
  ~AbortRunnable()
  { }

  virtual nsresult
  MainThreadRun() override;
};

class GetAllResponseHeadersRunnable final :
  public WorkerThreadProxySyncRunnable
{
  nsCString& mResponseHeaders;

public:
  GetAllResponseHeadersRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                                nsCString& aResponseHeaders)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy),
    mResponseHeaders(aResponseHeaders)
  { }

private:
  ~GetAllResponseHeadersRunnable()
  { }

  virtual nsresult
  MainThreadRun() override
  {
    mProxy->mXHR->GetAllResponseHeaders(mResponseHeaders);
    return NS_OK;
  }
};

class GetResponseHeaderRunnable final : public WorkerThreadProxySyncRunnable
{
  const nsCString mHeader;
  nsCString& mValue;

public:
  GetResponseHeaderRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                            const nsACString& aHeader, nsCString& aValue)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy), mHeader(aHeader),
    mValue(aValue)
  { }

private:
  ~GetResponseHeaderRunnable()
  { }

  virtual nsresult
  MainThreadRun() override
  {
    return mProxy->mXHR->GetResponseHeader(mHeader, mValue);
  }
};

class OpenRunnable final : public WorkerThreadProxySyncRunnable
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
    mURL(aURL), mBackgroundRequest(aBackgroundRequest),
    mWithCredentials(aWithCredentials), mTimeout(aTimeout)
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

private:
  ~OpenRunnable()
  { }

  virtual nsresult
  MainThreadRun() override
  {
    WorkerPrivate* oldWorker = mProxy->mWorkerPrivate;
    mProxy->mWorkerPrivate = mWorkerPrivate;

    nsresult rv = MainThreadRunInternal();

    mProxy->mWorkerPrivate = oldWorker;
    return rv;
  }

  nsresult
  MainThreadRunInternal();
};

class SetRequestHeaderRunnable final : public WorkerThreadProxySyncRunnable
{
  nsCString mHeader;
  nsCString mValue;

public:
  SetRequestHeaderRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                           const nsACString& aHeader, const nsACString& aValue)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy), mHeader(aHeader),
    mValue(aValue)
  { }

private:
  ~SetRequestHeaderRunnable()
  { }

  virtual nsresult
  MainThreadRun() override
  {
    return mProxy->mXHR->SetRequestHeader(mHeader, mValue);
  }
};

class OverrideMimeTypeRunnable final : public WorkerThreadProxySyncRunnable
{
  nsString mMimeType;

public:
  OverrideMimeTypeRunnable(WorkerPrivate* aWorkerPrivate, Proxy* aProxy,
                           const nsAString& aMimeType)
  : WorkerThreadProxySyncRunnable(aWorkerPrivate, aProxy), mMimeType(aMimeType)
  { }

private:
  ~OverrideMimeTypeRunnable()
  { }

  virtual nsresult
  MainThreadRun() override
  {
    mProxy->mXHR->OverrideMimeType(mMimeType);
    return NS_OK;
  }
};

class AutoUnpinXHR
{
  XMLHttpRequest* mXMLHttpRequestPrivate;

public:
  explicit AutoUnpinXHR(XMLHttpRequest* aXMLHttpRequestPrivate)
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
};

} // namespace

bool
Proxy::Init()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mWorkerPrivate);

  if (mXHR) {
    return true;
  }

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
                           global, mWorkerPrivate->GetBaseURI(),
                           mWorkerPrivate->GetLoadGroup()))) {
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

  return true;
}

void
Proxy::Teardown(bool aSendUnpin)
{
  AssertIsOnMainThread();

  if (mXHR) {
    Reset();

    // NB: We are intentionally dropping events coming from xhr.abort on the
    // floor.
    AddRemoveEventListeners(false, false);
    mXHR->Abort();

    if (mOutstandingSendCount) {
      if (aSendUnpin) {
        nsRefPtr<XHRUnpinRunnable> runnable =
          new XHRUnpinRunnable(mWorkerPrivate, mXMLHttpRequestPrivate);
        if (!runnable->Dispatch(nullptr)) {
          NS_RUNTIMEABORT("We're going to hang at shutdown anyways.");
        }
      }

      if (mSyncLoopTarget) {
        // We have an unclosed sync loop.  Fix that now.
        nsRefPtr<MainThreadStopSyncLoopRunnable> runnable =
          new MainThreadStopSyncLoopRunnable(mWorkerPrivate,
                                             mSyncLoopTarget.forget(),
                                             false);
        if (!runnable->Dispatch(nullptr)) {
          NS_RUNTIMEABORT("We're going to hang at shutdown anyways.");
        }
      }

      mOutstandingSendCount = 0;
    }

    mWorkerPrivate = nullptr;
    mXHRUpload = nullptr;
    mXHR = nullptr;
  }

  MOZ_ASSERT(!mWorkerPrivate);
  MOZ_ASSERT(!mSyncLoopTarget);
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

NS_IMPL_ISUPPORTS(Proxy, nsIDOMEventListener)

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
  ProgressEvent* progressEvent = aEvent->InternalDOMEvent()->AsProgressEvent();

  nsRefPtr<EventRunnable> runnable;

  if (mInOpen && type.EqualsASCII(sEventStrings[STRING_readystatechange])) {
    uint16_t readyState = 0;
    if (NS_SUCCEEDED(mXHR->GetReadyState(&readyState)) &&
        readyState == nsIXMLHttpRequest::OPENED) {
      mInnerEventStreamId++;
    }
  }

  if (progressEvent) {
    runnable = new EventRunnable(this, !!uploadTarget, type,
                                 progressEvent->LengthComputable(),
                                 progressEvent->Loaded(),
                                 progressEvent->Total());
  }
  else {
    runnable = new EventRunnable(this, !!uploadTarget, type);
  }

  {
    AutoSafeJSContext cx;
    JSAutoRequest ar(cx);

    JS::Rooted<JS::Value> value(cx);
    if (!GetOrCreateDOMReflectorNoWrap(cx, mXHR, &value)) {
      return NS_ERROR_FAILURE;
    }

    JS::Rooted<JSObject*> scope(cx, &value.toObject());
    JSAutoCompartment ac(cx, scope);

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
      if (!runnable->RegisterAndDispatch()) {
        NS_WARNING("Failed to dispatch LoadStartDetectionRunnable!");
      }
    }
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED0(WorkerThreadProxySyncRunnable, nsRunnable)

NS_IMPL_ISUPPORTS_INHERITED0(AsyncTeardownRunnable, nsRunnable)

NS_IMPL_ISUPPORTS_INHERITED(LoadStartDetectionRunnable, nsRunnable,
                                                        nsIDOMEventListener)

NS_IMETHODIMP
LoadStartDetectionRunnable::Run()
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
                                  mXMLHttpRequestPrivate, mChannelId);
      if (runnable->Dispatch(nullptr)) {
        mProxy->mWorkerPrivate = nullptr;
        mProxy->mSyncLoopTarget = nullptr;
        mProxy->mOutstandingSendCount--;
      }
    }
  }

  mProxy = nullptr;
  mXHR = nullptr;
  mXMLHttpRequestPrivate = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
LoadStartDetectionRunnable::HandleEvent(nsIDOMEvent* aEvent)
{
  AssertIsOnMainThread();

#ifdef DEBUG
  {
    nsString type;
    if (NS_SUCCEEDED(aEvent->GetType(type))) {
      MOZ_ASSERT(type == mEventType);
    }
    else {
      NS_WARNING("Failed to get event type!");
    }
  }
#endif

  mReceivedLoadStart = true;
  return NS_OK;
}

bool
EventRunnable::PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
{
  AssertIsOnMainThread();

  nsRefPtr<nsXMLHttpRequest>& xhr = mProxy->mXHR;
  MOZ_ASSERT(xhr);

  if (NS_FAILED(xhr->GetResponseType(mResponseType))) {
    MOZ_ASSERT(false, "This should never fail!");
  }

  mResponseTextResult = xhr->GetResponseText(mResponseText);
  if (NS_SUCCEEDED(mResponseTextResult)) {
    mResponseResult = mResponseTextResult;
    if (mResponseText.IsVoid()) {
      mResponse.setNull();
    }
  }
  else {
    JS::Rooted<JS::Value> response(aCx);
    mResponseResult = xhr->GetResponse(aCx, &response);
    if (NS_SUCCEEDED(mResponseResult)) {
      if (!response.isGCThing()) {
        mResponse = response;
      } else {
        bool doClone = true;
        JS::Rooted<JS::Value> transferable(aCx);
        JS::Rooted<JSObject*> obj(aCx, response.isObjectOrNull() ?
                                  response.toObjectOrNull() : nullptr);
        if (obj && JS_IsArrayBufferObject(obj)) {
          // Use cached response if the arraybuffer has been transfered.
          if (mProxy->mArrayBufferResponseWasTransferred) {
            MOZ_ASSERT(JS_IsNeuteredArrayBufferObject(obj));
            mUseCachedArrayBufferResponse = true;
            doClone = false;
          } else {
            MOZ_ASSERT(!JS_IsNeuteredArrayBufferObject(obj));
            JS::AutoValueArray<1> argv(aCx);
            argv[0].set(response);
            obj = JS_NewArrayObject(aCx, argv);
            if (obj) {
              transferable.setObject(*obj);
              // Only cache the response when the readyState is DONE.
              if (xhr->ReadyState() == nsIXMLHttpRequest::DONE) {
                mProxy->mArrayBufferResponseWasTransferred = true;
              }
            } else {
              mResponseResult = NS_ERROR_OUT_OF_MEMORY;
              doClone = false;
            }
          }
        }

        if (doClone) {
          ErrorResult rv;
          Write(aCx, response, transferable, false, rv);
          if (NS_WARN_IF(rv.Failed())) {
            NS_WARNING("Failed to clone response!");
            mResponseResult = rv.StealNSResult();
            mProxy->mArrayBufferResponseWasTransferred = false;
          }
        }
      }
    }
  }

  mStatusResult = xhr->GetStatus(&mStatus);

  xhr->GetStatusText(mStatusText);

  mReadyState = xhr->ReadyState();

  xhr->GetResponseURL(mResponseURL);

  return true;
}

bool
EventRunnable::WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
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
      if (!mProxy->mSeenLoadStart) {
        // We've already dispatched premature abort events.
        return true;
      }
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
    MOZ_ASSERT(mResponse.isUndefined() || mResponse.isNull());
    state->mResponseResult = mResponseTextResult;
    state->mResponse = mResponse;
  }
  else {
    state->mResponseResult = mResponseResult;

    if (NS_SUCCEEDED(mResponseResult)) {
      if (HasBeenWritten()) {
        MOZ_ASSERT(mResponse.isUndefined());

        ErrorResult rv;
        JS::Rooted<JS::Value> response(aCx);
        Read(nullptr, aCx, &response, rv);
        if (NS_WARN_IF(rv.Failed())) {
          rv.SuppressException();
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

  state->mResponseURL = mResponseURL;

  XMLHttpRequest* xhr = mProxy->mXMLHttpRequestPrivate;
  xhr->UpdateState(*state, mUseCachedArrayBufferResponse);

  if (mUploadEvent && !xhr->GetUploadObjectNoCreate()) {
    return true;
  }

  JS::Rooted<JSString*> type(aCx,
    JS_NewUCStringCopyN(aCx, mType.get(), mType.Length()));
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

  nsRefPtr<Event> event;
  if (mProgressEvent) {
    ProgressEventInit init;
    init.mBubbles = false;
    init.mCancelable = false;
    init.mLengthComputable = mLengthComputable;
    init.mLoaded = mLoaded;
    init.mTotal = mTotal;

    event = ProgressEvent::Constructor(target, mType, init);
  }
  else {
    event = NS_NewDOMEvent(target, nullptr, nullptr);

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

NS_IMETHODIMP
WorkerThreadProxySyncRunnable::Run()
{
  AssertIsOnMainThread();

  nsCOMPtr<nsIEventTarget> tempTarget;
  mSyncLoopTarget.swap(tempTarget);

  mProxy->mSyncEventResponseTarget.swap(tempTarget);

  nsresult rv = MainThreadRun();

  nsRefPtr<ResponseRunnable> response =
    new ResponseRunnable(mWorkerPrivate, mProxy, rv);
  if (!response->Dispatch(nullptr)) {
    MOZ_ASSERT(false, "Failed to dispatch response!");
  }

  mProxy->mSyncEventResponseTarget.swap(tempTarget);

  return NS_OK;
}

nsresult
AbortRunnable::MainThreadRun()
{
  mProxy->mInnerEventStreamId++;

  WorkerPrivate* oldWorker = mProxy->mWorkerPrivate;
  mProxy->mWorkerPrivate = mWorkerPrivate;

  mProxy->mXHR->Abort();

  mProxy->mWorkerPrivate = oldWorker;

  mProxy->Reset();

  return NS_OK;
}

nsresult
OpenRunnable::MainThreadRunInternal()
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

  MOZ_ASSERT(!mProxy->mInOpen);
  mProxy->mInOpen = true;

  ErrorResult rv2;
  mProxy->mXHR->Open(mMethod, mURL, true, mUser, mPassword, rv2);

  MOZ_ASSERT(mProxy->mInOpen);
  mProxy->mInOpen = false;

  if (rv2.Failed()) {
    return rv2.StealNSResult();
  }

  return mProxy->mXHR->SetResponseType(NS_LITERAL_STRING("text"));
}


nsresult
SendRunnable::MainThreadRun()
{
  nsCOMPtr<nsIVariant> variant;

  if (HasBeenWritten()) {
    AutoSafeJSContext cx;
    JSAutoRequest ar(cx);

    nsIXPConnect* xpc = nsContentUtils::XPConnect();
    MOZ_ASSERT(xpc);

    ErrorResult rv;

    JS::Rooted<JS::Value> body(cx);
    Read(nullptr, cx, &body, rv);
    if (NS_WARN_IF(rv.Failed())) {
      return rv.StealNSResult();
    }

    rv = xpc->JSValToVariant(cx, body, getter_AddRefs(variant));
    if (NS_WARN_IF(rv.Failed())) {
      return rv.StealNSResult();
    }
  }
  else {
    nsCOMPtr<nsIWritableVariant> wvariant =
      do_CreateInstance(NS_VARIANT_CONTRACTID);
    NS_ENSURE_TRUE(wvariant, NS_ERROR_UNEXPECTED);

    if (NS_FAILED(wvariant->SetAsAString(mStringBody))) {
      MOZ_ASSERT(false, "This should never fail!");
    }

    variant = wvariant;
  }

  // Send() has been already called.
  if (mProxy->mWorkerPrivate) {
    return NS_ERROR_FAILURE;
  }

  mProxy->mWorkerPrivate = mWorkerPrivate;

  MOZ_ASSERT(!mProxy->mSyncLoopTarget);
  mProxy->mSyncLoopTarget.swap(mSyncLoopTarget);

  if (mHasUploadListeners) {
    NS_ASSERTION(!mProxy->mUploadEventListenersAttached, "Huh?!");
    if (!mProxy->AddRemoveEventListeners(true, true)) {
      MOZ_ASSERT(false, "This should never fail!");
    }
  }

  mProxy->mArrayBufferResponseWasTransferred = false;

  mProxy->mInnerChannelId++;

  nsresult rv = mProxy->mXHR->Send(variant);

  if (NS_SUCCEEDED(rv)) {
    mProxy->mOutstandingSendCount++;

    if (!mHasUploadListeners) {
      NS_ASSERTION(!mProxy->mUploadEventListenersAttached, "Huh?!");
      if (!mProxy->AddRemoveEventListeners(true, true)) {
        MOZ_ASSERT(false, "This should never fail!");
      }
    }
  }

  return rv;
}

XMLHttpRequest::XMLHttpRequest(WorkerPrivate* aWorkerPrivate)
: mWorkerPrivate(aWorkerPrivate),
  mResponseType(XMLHttpRequestResponseType::Text), mTimeout(0),
  mRooted(false), mBackgroundRequest(false), mWithCredentials(false),
  mCanceled(false), mMozAnon(false), mMozSystem(false)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  mozilla::HoldJSObjects(this);
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
  tmp->mStateData.mResponse.setUndefined();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(XMLHttpRequest,
                                               nsXHREventTarget)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mStateData.mResponse)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

JSObject*
XMLHttpRequest::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return XMLHttpRequestBinding_workers::Wrap(aCx, this, aGivenProto);
}

// static
already_AddRefed<XMLHttpRequest>
XMLHttpRequest::Constructor(const GlobalObject& aGlobal,
                            const MozXMLHttpRequestParameters& aParams,
                            ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();
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
    aRv.Throw(NS_ERROR_FAILURE);
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

  // Only send readystatechange event when state changed.
  bool isStateChanged = false;
  if (mStateData.mReadyState != 4) {
    isStateChanged = true;
    mStateData.mReadyState = 4;
  }

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
    if (isStateChanged) {
      DispatchPrematureAbortEvent(this, NS_LITERAL_STRING("readystatechange"),
                                  false, aRv);
      if (aRv.Failed()) {
        return;
      }
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

  nsRefPtr<Event> event;
  if (aEventType.EqualsLiteral("readystatechange")) {
    event = NS_NewDOMEvent(aTarget, nullptr, nullptr);
    event->InitEvent(aEventType, false, false);
  }
  else {
    ProgressEventInit init;
    init.mBubbles = false;
    init.mCancelable = false;
    if (aUploadTarget) {
      init.mLengthComputable = mProxy->mLastUploadLengthComputable;
      init.mLoaded = mProxy->mLastUploadLoaded;
      init.mTotal = mProxy->mLastUploadTotal;
    }
    else {
      init.mLengthComputable = mProxy->mLastLengthComputable;
      init.mLoaded = mProxy->mLastLoaded;
      init.mTotal = mProxy->mLastTotal;
    }
    event = ProgressEvent::Constructor(aTarget, aEventType, init);
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
XMLHttpRequest::SendInternal(SendRunnable* aRunnable,
                             ErrorResult& aRv)
{
  MOZ_ASSERT(aRunnable);
  mWorkerPrivate->AssertIsOnWorkerThread();

  // No send() calls when open is running.
  if (mProxy->mOpenCount) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  bool hasUploadListeners = mUpload ? mUpload->HasListeners() : false;

  MaybePin(aRv);
  if (aRv.Failed()) {
    return;
  }

  AutoUnpinXHR autoUnpin(this);
  Maybe<AutoSyncLoopHolder> autoSyncLoop;

  nsCOMPtr<nsIEventTarget> syncLoopTarget;
  bool isSyncXHR = mProxy->mIsSyncXHR;
  if (isSyncXHR) {
    autoSyncLoop.emplace(mWorkerPrivate);
    syncLoopTarget = autoSyncLoop->EventTarget();
  }

  mProxy->mOuterChannelId++;

  JSContext* cx = mWorkerPrivate->GetJSContext();

  aRunnable->SetSyncLoopTarget(syncLoopTarget);
  aRunnable->SetHaveUploadListeners(hasUploadListeners);

  if (!aRunnable->Dispatch(cx)) {
    // Dispatch() may have spun the event loop and we may have already unrooted.
    // If so we don't want autoUnpin to try again.
    if (!mRooted) {
      autoUnpin.Clear();
    }
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (!isSyncXHR)  {
    autoUnpin.Clear();
    MOZ_ASSERT(!autoSyncLoop);
    return;
  }

  autoUnpin.Clear();

  if (!autoSyncLoop->Run()) {
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
    aRv.ThrowUncatchableException();
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

  ++mProxy->mOpenCount;
  if (!runnable->Dispatch(mWorkerPrivate->GetJSContext())) {
    if (!--mProxy->mOpenCount) {
      ReleaseProxy();
    }

    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  // We have been released in one of the nested Open() calls.
  if (!mProxy) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  --mProxy->mOpenCount;
  mProxy->mIsSyncXHR = !aAsync;
}

void
XMLHttpRequest::SetRequestHeader(const nsACString& aHeader,
                                 const nsACString& aValue, ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    aRv.ThrowUncatchableException();
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
    aRv.ThrowUncatchableException();
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
    aRv.ThrowUncatchableException();
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
    aRv.ThrowUncatchableException();
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
    aRv.ThrowUncatchableException();
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
    aRv.ThrowUncatchableException();
    return;
  }

  if (!mProxy) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  nsRefPtr<SendRunnable> sendRunnable =
    new SendRunnable(mWorkerPrivate, mProxy, NullString());

  // Nothing to clone.
  SendInternal(sendRunnable, aRv);
}

void
XMLHttpRequest::Send(const nsAString& aBody, ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    aRv.ThrowUncatchableException();
    return;
  }

  if (!mProxy) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  nsRefPtr<SendRunnable> sendRunnable =
    new SendRunnable(mWorkerPrivate, mProxy, aBody);

  // Nothing to clone.
  SendInternal(sendRunnable, aRv);
}

void
XMLHttpRequest::Send(JS::Handle<JSObject*> aBody, ErrorResult& aRv)
{
  JSContext* cx = mWorkerPrivate->GetJSContext();

  MOZ_ASSERT(aBody);

  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    aRv.ThrowUncatchableException();
    return;
  }

  if (!mProxy) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  JS::Rooted<JS::Value> valToClone(cx);
  if (JS_IsArrayBufferObject(aBody) || JS_IsArrayBufferViewObject(aBody)) {
    valToClone.setObject(*aBody);
  }
  else {
    JS::Rooted<JS::Value> obj(cx, JS::ObjectValue(*aBody));
    JSString* bodyStr = JS::ToString(cx, obj);
    if (!bodyStr) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    valToClone.setString(bodyStr);
  }

  nsRefPtr<SendRunnable> sendRunnable =
    new SendRunnable(mWorkerPrivate, mProxy, EmptyString());

  sendRunnable->Write(cx, valToClone, false, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  SendInternal(sendRunnable, aRv);
}

void
XMLHttpRequest::Send(Blob& aBody, ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  JSContext* cx = mWorkerPrivate->GetJSContext();

  if (mCanceled) {
    aRv.ThrowUncatchableException();
    return;
  }

  if (!mProxy) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  JS::Rooted<JS::Value> value(cx);
  if (!GetOrCreateDOMReflector(cx, &aBody, &value)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsRefPtr<BlobImpl> blobImpl = aBody.Impl();
  MOZ_ASSERT(blobImpl);

  aRv = blobImpl->SetMutable(false);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsRefPtr<SendRunnable> sendRunnable =
    new SendRunnable(mWorkerPrivate, mProxy, EmptyString());

  sendRunnable->Write(cx, value, false, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  SendInternal(sendRunnable, aRv);
}

void
XMLHttpRequest::Send(nsFormData& aBody, ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
  JSContext* cx = mWorkerPrivate->GetJSContext();

  if (mCanceled) {
    aRv.ThrowUncatchableException();
    return;
  }

  if (!mProxy) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  JS::Rooted<JS::Value> value(cx);
  if (!GetOrCreateDOMReflector(cx, &aBody, &value)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  nsRefPtr<SendRunnable> sendRunnable =
    new SendRunnable(mWorkerPrivate, mProxy, EmptyString());

  sendRunnable->Write(cx, value, false, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  SendInternal(sendRunnable, aRv);
}

void
XMLHttpRequest::Send(const ArrayBuffer& aBody, ErrorResult& aRv)
{
  JS::Rooted<JSObject*> obj(mWorkerPrivate->GetJSContext(), aBody.Obj());
  return Send(obj, aRv);
}

void
XMLHttpRequest::Send(const ArrayBufferView& aBody, ErrorResult& aRv)
{
  JS::Rooted<JSObject*> obj(mWorkerPrivate->GetJSContext(), aBody.Obj());
  return Send(obj, aRv);
}

void
XMLHttpRequest::Abort(ErrorResult& aRv)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  if (mCanceled) {
    aRv.ThrowUncatchableException();
    return;
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
    aRv.ThrowUncatchableException();
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
    aRv.ThrowUncatchableException();
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
    aRv.ThrowUncatchableException();
    return;
  }

  // We're supposed to throw if the state is not OPENED or HEADERS_RECEIVED. We
  // can detect OPENED really easily but we can't detect HEADERS_RECEIVED in a
  // non-racy way until the XHR state machine actually runs on this thread
  // (bug 671047). For now we're going to let this work only if the Send()
  // method has not been called, unless the send has been aborted.
  if (!mProxy || (SendInProgress() &&
                  (mProxy->mSeenLoadStart ||
                   mStateData.mReadyState > nsIXMLHttpRequest::OPENED))) {
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
    aRv.ThrowUncatchableException();
    return;
  }

  if (!mProxy || (SendInProgress() &&
                  (mProxy->mSeenLoadStart ||
                   mStateData.mReadyState > nsIXMLHttpRequest::OPENED))) {
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

void
XMLHttpRequest::GetResponse(JSContext* /* unused */,
                            JS::MutableHandle<JS::Value> aResponse,
                            ErrorResult& aRv)
{
  if (NS_SUCCEEDED(mStateData.mResponseTextResult) &&
      mStateData.mResponse.isUndefined()) {
    MOZ_ASSERT(NS_SUCCEEDED(mStateData.mResponseResult));

    if (mStateData.mResponseText.IsEmpty()) {
      mStateData.mResponse =
        JS_GetEmptyStringValue(mWorkerPrivate->GetJSContext());
    } else {
      JSString* str =
        JS_NewUCStringCopyN(mWorkerPrivate->GetJSContext(),
                            mStateData.mResponseText.get(),
                            mStateData.mResponseText.Length());

      if (!str) {
        aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return;
      }

      mStateData.mResponse.setString(str);
    }
  }

  JS::ExposeValueToActiveJS(mStateData.mResponse);
  aRv = mStateData.mResponseResult;
  aResponse.set(mStateData.mResponse);
}

void
XMLHttpRequest::GetResponseText(nsAString& aResponseText, ErrorResult& aRv)
{
  aRv = mStateData.mResponseTextResult;
  aResponseText = mStateData.mResponseText;
}

void
XMLHttpRequest::UpdateState(const StateData& aStateData,
                            bool aUseCachedArrayBufferResponse)
{
  if (aUseCachedArrayBufferResponse) {
    MOZ_ASSERT(mStateData.mResponse.isObject() &&
               JS_IsArrayBufferObject(&mStateData.mResponse.toObject()));

    JS::Rooted<JS::Value> response(mWorkerPrivate->GetJSContext(),
                                   mStateData.mResponse);
    mStateData = aStateData;
    mStateData.mResponse = response;
  }
  else {
    mStateData = aStateData;
  }
}
