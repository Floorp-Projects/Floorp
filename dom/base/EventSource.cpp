/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/EventSource.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/EventSourceBinding.h"
#include "mozilla/dom/MessageEvent.h"
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/UniquePtrExtensions.h"
#include "nsAutoPtr.h"
#include "nsIThreadRetargetableStreamListener.h"
#include "nsNetUtil.h"
#include "nsIAuthPrompt.h"
#include "nsIAuthPrompt2.h"
#include "nsIInputStream.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsMimeTypes.h"
#include "nsIPromptFactory.h"
#include "nsIWindowWatcher.h"
#include "nsPresContext.h"
#include "nsContentPolicyUtils.h"
#include "nsIStringBundle.h"
#include "nsIConsoleService.h"
#include "nsIObserverService.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsJSUtils.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIScriptError.h"
#include "nsIContentSecurityPolicy.h"
#include "nsContentUtils.h"
#include "mozilla/Preferences.h"
#include "xpcpublic.h"
#include "nsWrapperCacheInlines.h"
#include "mozilla/Attributes.h"
#include "nsError.h"
#include "mozilla/Encoding.h"

namespace mozilla {
namespace dom {

static LazyLogModule gEventSourceLog("EventSource");

#define SPACE_CHAR           (char16_t)0x0020
#define CR_CHAR              (char16_t)0x000D
#define LF_CHAR              (char16_t)0x000A
#define COLON_CHAR           (char16_t)0x003A

// Reconnection time related values in milliseconds. The default one is equal
// to the default value of the pref dom.server-events.default-reconnection-time
#define MIN_RECONNECTION_TIME_VALUE       500
#define DEFAULT_RECONNECTION_TIME_VALUE   5000
#define MAX_RECONNECTION_TIME_VALUE       PR_IntervalToMilliseconds(DELAY_INTERVAL_LIMIT)

class EventSourceImpl final : public nsIObserver
                            , public nsIStreamListener
                            , public nsIChannelEventSink
                            , public nsIInterfaceRequestor
                            , public nsSupportsWeakReference
                            , public nsIEventTarget
                            , public nsIThreadRetargetableStreamListener
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIEVENTTARGET_FULL
  NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER

  explicit EventSourceImpl(EventSource* aEventSource);

  enum {
    CONNECTING = 0U,
    OPEN = 1U,
    CLOSED = 2U
  };

  void Close();

  void Init(nsIPrincipal* aPrincipal, const nsAString& aURL, ErrorResult& aRv);

  nsresult GetBaseURI(nsIURI** aBaseURI);

  void SetupHttpChannel();
  nsresult SetupReferrerPolicy();
  nsresult InitChannelAndRequestEventSource();
  nsresult ResetConnection();
  void ResetDecoder();
  nsresult SetReconnectionTimeout();

  void AnnounceConnection();
  void DispatchAllMessageEvents();
  nsresult RestartConnection();
  void ReestablishConnection();
  void DispatchFailConnection();
  void FailConnection();

  nsresult Thaw();
  nsresult Freeze();

  static void TimerCallback(nsITimer* aTimer, void* aClosure);

  nsresult PrintErrorOnConsole(const char* aBundleURI,
                               const char* aError,
                               const char16_t** aFormatStrings,
                               uint32_t aFormatStringsLen);
  nsresult ConsoleError();

  static nsresult StreamReaderFunc(nsIInputStream* aInputStream,
                                   void* aClosure,
                                   const char* aFromRawSegment,
                                   uint32_t aToOffset,
                                   uint32_t aCount,
                                   uint32_t* aWriteCount);
  void ParseSegment(const char* aBuffer, uint32_t aLength);
  nsresult SetFieldAndClear();
  void ClearFields();
  nsresult ResetEvent();
  nsresult DispatchCurrentMessageEvent();
  nsresult ParseCharacter(char16_t aChr);
  nsresult CheckHealthOfRequestCallback(nsIRequest* aRequestCallback);
  nsresult OnRedirectVerifyCallback(nsresult result);
  nsresult ParseURL(const nsAString& aURL);
  nsresult AddWindowObservers();
  void RemoveWindowObservers();

  void CloseInternal();
  void CleanupOnMainThread();
  void AddRefObject();
  void ReleaseObject();

  bool CreateWorkerRef(WorkerPrivate* aWorkerPrivate);
  void ReleaseWorkerRef();

  void AssertIsOnTargetThread() const
  {
    MOZ_ASSERT(IsTargetThread());
  }

  bool IsTargetThread() const
  {
    return NS_IsMainThread() == mIsMainThread;
  }

  uint16_t ReadyState()
  {
    MutexAutoLock lock(mMutex);
    if (mEventSource) {
      return mEventSource->mReadyState;
    }
    // EventSourceImpl keeps EventSource alive. If mEventSource is null, it
    // means that the EventSource has been closed.
    return CLOSED;
  }

  void SetReadyState(uint16_t aReadyState)
  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(mEventSource);
    MOZ_ASSERT(!mIsShutDown);
    mEventSource->mReadyState = aReadyState;
  }

  bool IsFrozen()
  {
    MutexAutoLock lock(mMutex);
    return mFrozen;
  }

  void SetFrozen(bool aFrozen)
  {
    MutexAutoLock lock(mMutex);
    mFrozen = aFrozen;
  }

  bool IsClosed()
  {
    return ReadyState() == CLOSED;
  }

  void ShutDown()
  {
    MutexAutoLock lock(mMutex);
    MOZ_ASSERT(!mIsShutDown);
    mIsShutDown = true;
  }

  bool IsShutDown()
  {
    MutexAutoLock lock(mMutex);
    return mIsShutDown;
  }

  RefPtr<EventSource> mEventSource;

  /**
   * A simple state machine used to manage the event-source's line buffer
   *
   * PARSE_STATE_OFF              -> PARSE_STATE_BEGIN_OF_STREAM
   *
   * PARSE_STATE_BEGIN_OF_STREAM     -> PARSE_STATE_CR_CHAR |
   *                                 PARSE_STATE_BEGIN_OF_LINE |
   *                                 PARSE_STATE_COMMENT |
   *                                 PARSE_STATE_FIELD_NAME
   *
   * PARSE_STATE_CR_CHAR -> PARSE_STATE_CR_CHAR |
   *                        PARSE_STATE_COMMENT |
   *                        PARSE_STATE_FIELD_NAME |
   *                        PARSE_STATE_BEGIN_OF_LINE
   *
   * PARSE_STATE_COMMENT -> PARSE_STATE_CR_CHAR |
   *                        PARSE_STATE_BEGIN_OF_LINE
   *
   * PARSE_STATE_FIELD_NAME   -> PARSE_STATE_CR_CHAR |
   *                             PARSE_STATE_BEGIN_OF_LINE |
   *                             PARSE_STATE_FIRST_CHAR_OF_FIELD_VALUE
   *
   * PARSE_STATE_FIRST_CHAR_OF_FIELD_VALUE  -> PARSE_STATE_FIELD_VALUE |
   *                                           PARSE_STATE_CR_CHAR |
   *                                           PARSE_STATE_BEGIN_OF_LINE
   *
   * PARSE_STATE_FIELD_VALUE      -> PARSE_STATE_CR_CHAR |
   *                                 PARSE_STATE_BEGIN_OF_LINE
   *
   * PARSE_STATE_BEGIN_OF_LINE   -> PARSE_STATE_CR_CHAR |
   *                                PARSE_STATE_COMMENT |
   *                                PARSE_STATE_FIELD_NAME |
   *                                PARSE_STATE_BEGIN_OF_LINE
   *
   * Whenever the parser find an empty line or the end-of-file
   * it dispatches the stacked event.
   *
   */
  enum ParserStatus {
    PARSE_STATE_OFF = 0,
    PARSE_STATE_BEGIN_OF_STREAM,
    PARSE_STATE_CR_CHAR,
    PARSE_STATE_COMMENT,
    PARSE_STATE_FIELD_NAME,
    PARSE_STATE_FIRST_CHAR_OF_FIELD_VALUE,
    PARSE_STATE_FIELD_VALUE,
    PARSE_STATE_BEGIN_OF_LINE
  };

  // Connection related data members. Should only be accessed on main thread.
  nsCOMPtr<nsIURI> mSrc;
  uint32_t mReconnectionTime; // in ms
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsString mOrigin;
  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<nsIHttpChannel> mHttpChannel;

  struct Message
  {
    nsString mEventName;
    nsString mLastEventID;
    nsString mData;
  };

  // Message related data members. May be set / initialized when initializing
  // EventSourceImpl on target thread but should only be used on target thread.
  nsString mLastEventID;
  UniquePtr<Message> mCurrentMessage;
  nsDeque mMessagesToDispatch;
  ParserStatus mStatus;
  mozilla::UniquePtr<mozilla::Decoder> mUnicodeDecoder;
  nsString mLastFieldName;
  nsString mLastFieldValue;

  // EventSourceImpl internal states.
  // WorkerRef to keep the worker alive. (accessed on worker thread only)
  RefPtr<ThreadSafeWorkerRef> mWorkerRef;
  // This mutex protects mFrozen and mEventSource->mReadyState that are used in
  // different threads.
  mozilla::Mutex mMutex;
  // Whether the window is frozen. May be set on main thread and read on target
  // thread. Use mMutex to protect it before accessing it.
  bool mFrozen;
  // There are some messages are going to be dispatched when thaw.
  bool mGoingToDispatchAllMessages;
  // Whether the EventSource is run on main thread.
  bool mIsMainThread;
  // Whether the EventSourceImpl is going to be destroyed.
  bool mIsShutDown;

  // Event Source owner information:
  // - the script file name
  // - source code line number and column number where the Event Source object
  //   was constructed.
  // - the ID of the inner window where the script lives. Note that this may not
  //   be the same as the Event Source owner window.
  // These attributes are used for error reporting. Should only be accessed on
  // target thread
  nsString mScriptFile;
  uint32_t mScriptLine;
  uint32_t mScriptColumn;
  uint64_t mInnerWindowID;

private:
  // prevent bad usage
  EventSourceImpl(const EventSourceImpl& x) = delete;
  EventSourceImpl& operator=(const EventSourceImpl& x) = delete;
  ~EventSourceImpl()
  {
    if (IsClosed()) {
      return;
    }
    // If we threw during Init we never called Close
    SetReadyState(CLOSED);
    CloseInternal();
  }
};

NS_IMPL_ISUPPORTS(EventSourceImpl,
                  nsIObserver,
                  nsIStreamListener,
                  nsIRequestObserver,
                  nsIChannelEventSink,
                  nsIInterfaceRequestor,
                  nsISupportsWeakReference,
                  nsIEventTarget,
                  nsIThreadRetargetableStreamListener)

EventSourceImpl::EventSourceImpl(EventSource* aEventSource)
  : mEventSource(aEventSource)
  , mReconnectionTime(0)
  , mStatus(PARSE_STATE_OFF)
  , mMutex("EventSourceImpl::mMutex")
  , mFrozen(false)
  , mGoingToDispatchAllMessages(false)
  , mIsMainThread(NS_IsMainThread())
  , mIsShutDown(false)
  , mScriptLine(0)
  , mScriptColumn(0)
  , mInnerWindowID(0)
{
  MOZ_ASSERT(mEventSource);
  if (!mIsMainThread) {
    mEventSource->mIsMainThread = false;
  }
  SetReadyState(CONNECTING);
}

class CleanupRunnable final : public WorkerMainThreadRunnable
{
public:
  explicit CleanupRunnable(EventSourceImpl* aEventSourceImpl)
    : WorkerMainThreadRunnable(GetCurrentThreadWorkerPrivate(),
                               NS_LITERAL_CSTRING("EventSource :: Cleanup"))
    , mImpl(aEventSourceImpl)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool MainThreadRun() override
  {
    mImpl->CleanupOnMainThread();
    return true;
  }

protected:
  // Raw pointer because this runnable is sync.
  EventSourceImpl* mImpl;
};

void
EventSourceImpl::Close()
{
  if (IsClosed()) {
    return;
  }

  SetReadyState(CLOSED);
  CloseInternal();
}

void
EventSourceImpl::CloseInternal()
{
  AssertIsOnTargetThread();
  MOZ_ASSERT(IsClosed());
  if (IsShutDown()) {
    return;
  }

  // Invoke CleanupOnMainThread before cleaning any members. It will call
  // ShutDown, which is supposed to be called before cleaning any members.
  if (NS_IsMainThread()) {
    CleanupOnMainThread();
  } else {
    ErrorResult rv;
    // run CleanupOnMainThread synchronously on main thread since it touches
    // observers and members only can be accessed on main thread.
    RefPtr<CleanupRunnable> runnable = new CleanupRunnable(this);
    runnable->Dispatch(Killing, rv);
    MOZ_ASSERT(!rv.Failed());
    ReleaseWorkerRef();
  }

  while (mMessagesToDispatch.GetSize() != 0) {
    delete static_cast<Message*>(mMessagesToDispatch.PopFront());
  }
  SetFrozen(false);
  ResetDecoder();
  mUnicodeDecoder = nullptr;
  // UpdateDontKeepAlive() can release the object. Don't access to any members
  // after it.
  mEventSource->UpdateDontKeepAlive();
}

void EventSourceImpl::CleanupOnMainThread()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(IsClosed());

  // Call ShutDown before cleaning any members.
  ShutDown();

  if (mIsMainThread) {
    RemoveWindowObservers();
  }

  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }

  ResetConnection();
  mPrincipal = nullptr;
  mSrc = nullptr;
}

class InitRunnable final : public WorkerMainThreadRunnable
{
public:
  InitRunnable(WorkerPrivate* aWorkerPrivate,
               EventSourceImpl* aEventSourceImpl,
               const nsAString& aURL)
    : WorkerMainThreadRunnable(aWorkerPrivate,
                               NS_LITERAL_CSTRING("EventSource :: Init"))
    , mImpl(aEventSourceImpl)
    , mURL(aURL)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool MainThreadRun() override
  {
    // Get principal from worker's owner document or from worker.
    WorkerPrivate* wp = mWorkerPrivate;
    while (wp->GetParent()) {
      wp = wp->GetParent();
    }
    nsPIDOMWindowInner* window = wp->GetWindow();
    nsIDocument* doc = window ? window->GetExtantDoc() : nullptr;
    nsCOMPtr<nsIPrincipal> principal = doc ? doc->NodePrincipal() :
                                             wp->GetPrincipal();
    if (!principal) {
      mRv = NS_ERROR_FAILURE;
      return true;
    }
    ErrorResult rv;
    mImpl->Init(principal, mURL, rv);
    mRv = rv.StealNSResult();
    return true;
  }

  nsresult ErrorCode() const { return mRv; }

private:
  // Raw pointer because this runnable is sync.
  EventSourceImpl* mImpl;
  const nsAString& mURL;
  nsresult mRv;
};

class ConnectRunnable final : public WorkerMainThreadRunnable
{
public:
  explicit ConnectRunnable(WorkerPrivate* aWorkerPrivate,
                           EventSourceImpl* aEventSourceImpl)
    : WorkerMainThreadRunnable(aWorkerPrivate,
                               NS_LITERAL_CSTRING("EventSource :: Connect"))
    , mImpl(aEventSourceImpl)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool MainThreadRun() override
  {
    mImpl->InitChannelAndRequestEventSource();
    return true;
  }

private:
  RefPtr<EventSourceImpl> mImpl;
};

nsresult
EventSourceImpl::ParseURL(const nsAString& aURL)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(!IsShutDown());
  // get the src
  nsCOMPtr<nsIURI> baseURI;
  nsresult rv = GetBaseURI(getter_AddRefs(baseURI));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> srcURI;
  rv = NS_NewURI(getter_AddRefs(srcURI), aURL, nullptr, baseURI);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_SYNTAX_ERR);

  nsAutoString origin;
  rv = nsContentUtils::GetUTFOrigin(srcURI, origin);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString spec;
  rv = srcURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  mEventSource->mOriginalURL = NS_ConvertUTF8toUTF16(spec);
  mSrc = srcURI;
  mOrigin = origin;
  return NS_OK;
}

nsresult
EventSourceImpl::AddWindowObservers()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mIsMainThread);
  MOZ_ASSERT(!IsShutDown());
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  NS_ENSURE_STATE(os);

  nsresult rv = os->AddObserver(this, DOM_WINDOW_DESTROYED_TOPIC, true);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = os->AddObserver(this, DOM_WINDOW_FROZEN_TOPIC, true);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = os->AddObserver(this, DOM_WINDOW_THAWED_TOPIC, true);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

void
EventSourceImpl::RemoveWindowObservers()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mIsMainThread);
  MOZ_ASSERT(IsClosed());
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    os->RemoveObserver(this, DOM_WINDOW_DESTROYED_TOPIC);
    os->RemoveObserver(this, DOM_WINDOW_FROZEN_TOPIC);
    os->RemoveObserver(this, DOM_WINDOW_THAWED_TOPIC);
  }
}

void
EventSourceImpl::Init(nsIPrincipal* aPrincipal,
                      const nsAString& aURL,
                      ErrorResult& aRv)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(ReadyState() == CONNECTING);
  mPrincipal = aPrincipal;
  aRv = ParseURL(aURL);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
  // The conditional here is historical and not necessarily sane.
  if (JSContext* cx = nsContentUtils::GetCurrentJSContext()) {
    nsJSUtils::GetCallingLocation(cx, mScriptFile, &mScriptLine,
                                  &mScriptColumn);
    mInnerWindowID = nsJSUtils::GetCurrentlyRunningCodeInnerWindowID(cx);
  }

  if (mIsMainThread) {
    // we observe when the window freezes and thaws
    aRv = AddWindowObservers();
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }
  }

  mReconnectionTime =
    Preferences::GetInt("dom.server-events.default-reconnection-time",
                        DEFAULT_RECONNECTION_TIME_VALUE);

  mUnicodeDecoder = UTF_8_ENCODING->NewDecoderWithBOMRemoval();
}

//-----------------------------------------------------------------------------
// EventSourceImpl::nsIObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
EventSourceImpl::Observe(nsISupports* aSubject,
                         const char* aTopic,
                         const char16_t* aData)
{
  AssertIsOnMainThread();
  if (IsClosed()) {
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aSubject);
  if (!mEventSource->GetOwner() || window != mEventSource->GetOwner()) {
    return NS_OK;
  }

  DebugOnly<nsresult> rv;
  if (strcmp(aTopic, DOM_WINDOW_FROZEN_TOPIC) == 0) {
    rv = Freeze();
    MOZ_ASSERT(NS_SUCCEEDED(rv), "Freeze() failed");
  } else if (strcmp(aTopic, DOM_WINDOW_THAWED_TOPIC) == 0) {
    rv = Thaw();
    MOZ_ASSERT(NS_SUCCEEDED(rv), "Thaw() failed");
  } else if (strcmp(aTopic, DOM_WINDOW_DESTROYED_TOPIC) == 0) {
    Close();
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// EventSourceImpl::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
EventSourceImpl::OnStartRequest(nsIRequest* aRequest, nsISupports* aCtxt)
{
  AssertIsOnMainThread();
  if (IsClosed()) {
    return NS_ERROR_ABORT;
  }
  nsresult rv = CheckHealthOfRequestCallback(aRequest);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aRequest, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsresult status;
  rv = aRequest->GetStatus(&status);
  NS_ENSURE_SUCCESS(rv, rv);

  if (NS_FAILED(status)) {
    // EventSource::OnStopRequest will evaluate if it shall either reestablish
    // or fail the connection
    return NS_ERROR_ABORT;
  }

  uint32_t httpStatus;
  rv = httpChannel->GetResponseStatus(&httpStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  if (httpStatus != 200) {
    DispatchFailConnection();
    return NS_ERROR_ABORT;
  }

  nsAutoCString contentType;
  rv = httpChannel->GetContentType(contentType);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!contentType.EqualsLiteral(TEXT_EVENT_STREAM)) {
    DispatchFailConnection();
    return NS_ERROR_ABORT;
  }

  if (!mIsMainThread) {
    // Try to retarget to worker thread, otherwise fall back to main thread.
    nsCOMPtr<nsIThreadRetargetableRequest> rr = do_QueryInterface(httpChannel);
    if (rr) {
      rv = rr->RetargetDeliveryTo(this);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        NS_WARNING("Retargeting failed");
      }
    }
  }
  rv = Dispatch(NewRunnableMethod("dom::EventSourceImpl::AnnounceConnection",
                                  this,
                                  &EventSourceImpl::AnnounceConnection),
                NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);
  mStatus = PARSE_STATE_BEGIN_OF_STREAM;
  return NS_OK;
}

// this method parses the characters as they become available instead of
// buffering them.
nsresult
EventSourceImpl::StreamReaderFunc(nsIInputStream* aInputStream,
                                  void* aClosure,
                                  const char* aFromRawSegment,
                                  uint32_t aToOffset,
                                  uint32_t aCount,
                                  uint32_t* aWriteCount)
{
  EventSourceImpl* thisObject = static_cast<EventSourceImpl*>(aClosure);
  if (!thisObject || !aWriteCount) {
    NS_WARNING("EventSource cannot read from stream: no aClosure or aWriteCount");
    return NS_ERROR_FAILURE;
  }
  thisObject->AssertIsOnTargetThread();
  MOZ_ASSERT(!thisObject->IsShutDown());
  thisObject->ParseSegment((const char*)aFromRawSegment, aCount);
  *aWriteCount = aCount;
  return NS_OK;
}

void
EventSourceImpl::ParseSegment(const char* aBuffer, uint32_t aLength)
{
  AssertIsOnTargetThread();
  if (IsClosed()) {
    return;
  }
  char16_t buffer[1024];
  auto dst = MakeSpan(buffer);
  auto src = AsBytes(MakeSpan(aBuffer, aLength));
  // XXX EOF handling is https://bugzilla.mozilla.org/show_bug.cgi?id=1369018
  for (;;) {
    uint32_t result;
    size_t read;
    size_t written;
    bool hadErrors;
    Tie(result, read, written, hadErrors) =
      mUnicodeDecoder->DecodeToUTF16(src, dst, false);
    Unused << hadErrors;
    for (auto c : dst.To(written)) {
      nsresult rv = ParseCharacter(c);
      NS_ENSURE_SUCCESS_VOID(rv);
    }
    if (result == kInputEmpty) {
      return;
    }
    src = src.From(read);
  }
}

NS_IMETHODIMP
EventSourceImpl::OnDataAvailable(nsIRequest* aRequest,
                                 nsISupports* aContext,
                                 nsIInputStream* aInputStream,
                                 uint64_t aOffset,
                                 uint32_t aCount)
{
  AssertIsOnTargetThread();
  NS_ENSURE_ARG_POINTER(aInputStream);
  if (IsClosed()) {
    return NS_ERROR_ABORT;
  }

  nsresult rv = CheckHealthOfRequestCallback(aRequest);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t totalRead;
  return aInputStream->ReadSegments(EventSourceImpl::StreamReaderFunc, this,
                                    aCount, &totalRead);
}

NS_IMETHODIMP
EventSourceImpl::OnStopRequest(nsIRequest* aRequest,
                               nsISupports* aContext,
                               nsresult aStatusCode)
{
  AssertIsOnMainThread();

  if (IsClosed()) {
    return NS_ERROR_ABORT;
  }
  MOZ_ASSERT(mSrc);
  // "Network errors that prevents the connection from being established in the
  //  first place (e.g. DNS errors), must cause the user agent to asynchronously
  //  reestablish the connection.
  //
  //  (...) the cancelation of the fetch algorithm by the user agent (e.g. in
  //  response to window.stop() or the user canceling the network connection
  //  manually) must cause the user agent to fail the connection.

  if (NS_FAILED(aStatusCode) &&
      aStatusCode != NS_ERROR_CONNECTION_REFUSED &&
      aStatusCode != NS_ERROR_NET_TIMEOUT &&
      aStatusCode != NS_ERROR_NET_RESET &&
      aStatusCode != NS_ERROR_NET_INTERRUPT &&
      aStatusCode != NS_ERROR_PROXY_CONNECTION_REFUSED &&
      aStatusCode != NS_ERROR_DNS_LOOKUP_QUEUE_FULL) {
    DispatchFailConnection();
    return NS_ERROR_ABORT;
  }

  nsresult rv = CheckHealthOfRequestCallback(aRequest);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = Dispatch(NewRunnableMethod("dom::EventSourceImpl::ReestablishConnection",
                                  this,
                                  &EventSourceImpl::ReestablishConnection),
                NS_DISPATCH_NORMAL);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

//-----------------------------------------------------------------------------
// EventSourceImpl::nsIChannelEventSink
//-----------------------------------------------------------------------------

NS_IMETHODIMP
EventSourceImpl::AsyncOnChannelRedirect(nsIChannel* aOldChannel,
                                        nsIChannel* aNewChannel,
                                        uint32_t aFlags,
                                        nsIAsyncVerifyRedirectCallback* aCallback)
{
  AssertIsOnMainThread();
  if (IsClosed()) {
    return NS_ERROR_ABORT;
  }
  nsCOMPtr<nsIRequest> aOldRequest = do_QueryInterface(aOldChannel);
  NS_PRECONDITION(aOldRequest, "Redirect from a null request?");

  nsresult rv = CheckHealthOfRequestCallback(aOldRequest);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_PRECONDITION(aNewChannel, "Redirect without a channel?");

  nsCOMPtr<nsIURI> newURI;
  rv = NS_GetFinalChannelURI(aNewChannel, getter_AddRefs(newURI));
  NS_ENSURE_SUCCESS(rv, rv);

  bool isValidScheme =
    (NS_SUCCEEDED(newURI->SchemeIs("http", &isValidScheme)) && isValidScheme) ||
    (NS_SUCCEEDED(newURI->SchemeIs("https", &isValidScheme)) && isValidScheme);

  rv = mEventSource->CheckInnerWindowCorrectness();
  if (NS_FAILED(rv) || !isValidScheme) {
     DispatchFailConnection();
     return NS_ERROR_DOM_SECURITY_ERR;
  }

  // update our channel

  mHttpChannel = do_QueryInterface(aNewChannel);
  NS_ENSURE_STATE(mHttpChannel);

  SetupHttpChannel();
  // The HTTP impl already copies over the referrer and referrer policy on
  // redirects, so we don't need to SetupReferrerPolicy().

  if ((aFlags & nsIChannelEventSink::REDIRECT_PERMANENT) != 0) {
    rv = NS_GetFinalChannelURI(mHttpChannel, getter_AddRefs(mSrc));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  aCallback->OnRedirectVerifyCallback(NS_OK);

  return NS_OK;
}

//-----------------------------------------------------------------------------
// EventSourceImpl::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP
EventSourceImpl::GetInterface(const nsIID& aIID, void** aResult)
{
  AssertIsOnMainThread();

  if (IsClosed()) {
    return NS_ERROR_FAILURE;
  }

  if (aIID.Equals(NS_GET_IID(nsIChannelEventSink))) {
    *aResult = static_cast<nsIChannelEventSink*>(this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsIAuthPrompt)) ||
      aIID.Equals(NS_GET_IID(nsIAuthPrompt2))) {
    nsresult rv = mEventSource->CheckInnerWindowCorrectness();
    NS_ENSURE_SUCCESS(rv, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsIPromptFactory> wwatch =
      do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    // Get the an auth prompter for our window so that the parenting
    // of the dialogs works as it should when using tabs.

    nsCOMPtr<nsPIDOMWindowOuter> window;
    if (mEventSource->GetOwner()) {
      window = mEventSource->GetOwner()->GetOuterWindow();
    }

    return wwatch->GetPrompt(window, aIID, aResult);
  }

  return QueryInterface(aIID, aResult);
}

NS_IMETHODIMP
EventSourceImpl::IsOnCurrentThread(bool* aResult)
{
  *aResult = IsTargetThread();
  return NS_OK;
}

NS_IMETHODIMP_(bool)
EventSourceImpl::IsOnCurrentThreadInfallible()
{
  return IsTargetThread();
}

nsresult
EventSourceImpl::GetBaseURI(nsIURI** aBaseURI)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(!IsShutDown());
  NS_ENSURE_ARG_POINTER(aBaseURI);

  *aBaseURI = nullptr;

  nsCOMPtr<nsIURI> baseURI;

  // first we try from document->GetBaseURI()
  nsCOMPtr<nsIDocument> doc = mEventSource->GetDocumentIfCurrent();
  if (doc) {
    baseURI = doc->GetBaseURI();
  }

  // otherwise we get from the doc's principal
  if (!baseURI) {
    nsresult rv = mPrincipal->GetURI(getter_AddRefs(baseURI));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_ENSURE_STATE(baseURI);

  baseURI.forget(aBaseURI);
  return NS_OK;
}

void
EventSourceImpl::SetupHttpChannel()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(!IsShutDown());
  nsresult rv = mHttpChannel->SetRequestMethod(NS_LITERAL_CSTRING("GET"));
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  /* set the http request headers */

  rv = mHttpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
    NS_LITERAL_CSTRING(TEXT_EVENT_STREAM), false);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  // LOAD_BYPASS_CACHE already adds the Cache-Control: no-cache header

  if (mLastEventID.IsEmpty()) {
    return;
  }
  NS_ConvertUTF16toUTF8 eventId(mLastEventID);
  rv = mHttpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Last-Event-ID"),
                                      eventId, false);
#ifdef DEBUG
  if (NS_FAILED(rv)) {
    MOZ_LOG(gEventSourceLog, LogLevel::Warning,
            ("SetupHttpChannel. rv=%x (%s)", uint32_t(rv), eventId.get()));
  }
#endif
  Unused << rv;
}

nsresult
EventSourceImpl::SetupReferrerPolicy()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(!IsShutDown());
  nsCOMPtr<nsIDocument> doc = mEventSource->GetDocumentIfCurrent();
  if (doc) {
    nsresult rv = mHttpChannel->SetReferrerWithPolicy(doc->GetDocumentURI(),
                                                      doc->GetReferrerPolicy());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
EventSourceImpl::InitChannelAndRequestEventSource()
{
  AssertIsOnMainThread();
  if (IsClosed()) {
    return NS_ERROR_ABORT;
  }

  bool isValidScheme =
    (NS_SUCCEEDED(mSrc->SchemeIs("http", &isValidScheme)) && isValidScheme) ||
    (NS_SUCCEEDED(mSrc->SchemeIs("https", &isValidScheme)) && isValidScheme);

  nsresult rv = mEventSource->CheckInnerWindowCorrectness();
  if (NS_FAILED(rv) || !isValidScheme) {
    DispatchFailConnection();
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // The html spec requires we use fetch cache mode of "no-store".  This
  // maps to LOAD_BYPASS_CACHE and LOAD_INHIBIT_CACHING in necko.
  nsLoadFlags loadFlags;
  loadFlags = nsIRequest::LOAD_BACKGROUND | nsIRequest::LOAD_BYPASS_CACHE
                                          | nsIRequest::INHIBIT_CACHING;

  nsCOMPtr<nsIDocument> doc = mEventSource->GetDocumentIfCurrent();

  nsSecurityFlags securityFlags =
    nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS;

  if (mEventSource->mWithCredentials) {
    securityFlags |= nsILoadInfo::SEC_COOKIES_INCLUDE;
  }

  nsCOMPtr<nsIChannel> channel;
  // If we have the document, use it
  if (doc) {
    nsCOMPtr<nsILoadGroup> loadGroup = doc->GetDocumentLoadGroup();
    rv = NS_NewChannel(getter_AddRefs(channel),
                       mSrc,
                       doc,
                       securityFlags,
                       nsIContentPolicy::TYPE_INTERNAL_EVENTSOURCE,
                       nullptr,          // aPerformanceStorage
                       loadGroup,
                       nullptr,          // aCallbacks
                       loadFlags);       // aLoadFlags
  } else {
    // otherwise use the principal
    rv = NS_NewChannel(getter_AddRefs(channel),
                       mSrc,
                       mPrincipal,
                       securityFlags,
                       nsIContentPolicy::TYPE_INTERNAL_EVENTSOURCE,
                       nullptr,          // aPerformanceStorage
                       nullptr,          // loadGroup
                       nullptr,          // aCallbacks
                       loadFlags);       // aLoadFlags
  }

  NS_ENSURE_SUCCESS(rv, rv);

  mHttpChannel = do_QueryInterface(channel);
  NS_ENSURE_TRUE(mHttpChannel, NS_ERROR_NO_INTERFACE);

  SetupHttpChannel();
  rv = SetupReferrerPolicy();
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
  {
    nsCOMPtr<nsIInterfaceRequestor> notificationCallbacks;
    mHttpChannel->GetNotificationCallbacks(getter_AddRefs(notificationCallbacks));
    MOZ_ASSERT(!notificationCallbacks);
  }
#endif
  mHttpChannel->SetNotificationCallbacks(this);

  // Start reading from the channel
  rv = mHttpChannel->AsyncOpen2(this);
  if (NS_FAILED(rv)) {
    DispatchFailConnection();
    return rv;
  }
  // Create the connection. Ask EventSource to hold reference until Close is
  // called or network error is received.
  mEventSource->UpdateMustKeepAlive();
  return rv;
}

void
EventSourceImpl::AnnounceConnection()
{
  AssertIsOnTargetThread();
  if (ReadyState() != CONNECTING) {
    NS_WARNING("Unexpected mReadyState!!!");
    return;
  }

  // When a user agent is to announce the connection, the user agent must set
  // the readyState attribute to OPEN and queue a task to fire a simple event
  // named open at the EventSource object.

  SetReadyState(OPEN);

  nsresult rv = mEventSource->CheckInnerWindowCorrectness();
  if (NS_FAILED(rv)) {
    return;
  }
  rv = mEventSource->CreateAndDispatchSimpleEvent(NS_LITERAL_STRING("open"));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch the error event!!!");
    return;
  }
}

nsresult
EventSourceImpl::ResetConnection()
{
  AssertIsOnMainThread();
  if (mHttpChannel) {
    mHttpChannel->Cancel(NS_ERROR_ABORT);
    mHttpChannel = nullptr;
  }
  return NS_OK;
}

void
EventSourceImpl::ResetDecoder()
{
  AssertIsOnTargetThread();
  if (mUnicodeDecoder) {
    UTF_8_ENCODING->NewDecoderWithBOMRemovalInto(*mUnicodeDecoder);
  }
  mStatus = PARSE_STATE_OFF;
  ClearFields();
}

class CallRestartConnection final : public WorkerMainThreadRunnable
{
public:
  explicit CallRestartConnection(EventSourceImpl* aEventSourceImpl)
    : WorkerMainThreadRunnable(
        aEventSourceImpl->mWorkerRef->Private(),
        NS_LITERAL_CSTRING("EventSource :: RestartConnection"))
    , mImpl(aEventSourceImpl)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool MainThreadRun() override
  {
    mImpl->RestartConnection();
    return true;
  }

protected:
  // Raw pointer because this runnable is sync.
  EventSourceImpl* mImpl;
};

nsresult
EventSourceImpl::RestartConnection()
{
  AssertIsOnMainThread();
  if (IsClosed()) {
    return NS_ERROR_ABORT;
  }
  nsresult rv = ResetConnection();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetReconnectionTimeout();
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

void
EventSourceImpl::ReestablishConnection()
{
  AssertIsOnTargetThread();
  if (IsClosed()) {
    return;
  }

  nsresult rv;
  if (mIsMainThread) {
    rv = RestartConnection();
  } else {
    RefPtr<CallRestartConnection> runnable = new CallRestartConnection(this);
    ErrorResult result;
    runnable->Dispatch(Terminating, result);
    MOZ_ASSERT(!result.Failed());
    rv = result.StealNSResult();
  }
  if (NS_FAILED(rv)) {
    return;
  }

  rv = mEventSource->CheckInnerWindowCorrectness();
  if (NS_FAILED(rv)) {
    return;
  }

  SetReadyState(CONNECTING);
  ResetDecoder();
  rv = mEventSource->CreateAndDispatchSimpleEvent(NS_LITERAL_STRING("error"));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch the error event!!!");
    return;
  }
}

nsresult
EventSourceImpl::SetReconnectionTimeout()
{
  AssertIsOnMainThread();
  if (IsClosed()) {
    return NS_ERROR_ABORT;
  }

  // the timer will be used whenever the requests are going finished.
  if (!mTimer) {
    mTimer = NS_NewTimer();
    NS_ENSURE_STATE(mTimer);
  }

  nsresult rv = mTimer->InitWithNamedFuncCallback(
    TimerCallback,
    this,
    mReconnectionTime,
    nsITimer::TYPE_ONE_SHOT,
    "dom::EventSourceImpl::SetReconnectionTimeout");
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
EventSourceImpl::PrintErrorOnConsole(const char* aBundleURI,
                                     const char* aError,
                                     const char16_t** aFormatStrings,
                                     uint32_t aFormatStringsLen)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(!IsShutDown());
  nsCOMPtr<nsIStringBundleService> bundleService =
    mozilla::services::GetStringBundleService();
  NS_ENSURE_STATE(bundleService);

  nsCOMPtr<nsIStringBundle> strBundle;
  nsresult rv =
    bundleService->CreateBundle(aBundleURI, getter_AddRefs(strBundle));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIConsoleService> console(
    do_GetService(NS_CONSOLESERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIScriptError> errObj(
    do_CreateInstance(NS_SCRIPTERROR_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // Localize the error message
  nsAutoString message;
  if (aFormatStrings) {
    rv = strBundle->FormatStringFromName(aError, aFormatStrings,
                                         aFormatStringsLen, message);
  } else {
    rv = strBundle->GetStringFromName(aError, message);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  rv = errObj->InitWithWindowID(message,
                                mScriptFile,
                                EmptyString(),
                                mScriptLine, mScriptColumn,
                                nsIScriptError::errorFlag,
                                "Event Source", mInnerWindowID);
  NS_ENSURE_SUCCESS(rv, rv);

  // print the error message directly to the JS console
  rv = console->LogMessage(errObj);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
EventSourceImpl::ConsoleError()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(!IsShutDown());
  nsAutoCString targetSpec;
  nsresult rv = mSrc->GetSpec(targetSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ConvertUTF8toUTF16 specUTF16(targetSpec);
  const char16_t* formatStrings[] = { specUTF16.get() };

  if (ReadyState() == CONNECTING) {
    rv = PrintErrorOnConsole("chrome://global/locale/appstrings.properties",
                             "connectionFailure",
                             formatStrings, ArrayLength(formatStrings));
  } else {
    rv = PrintErrorOnConsole("chrome://global/locale/appstrings.properties",
                             "netInterrupt",
                             formatStrings, ArrayLength(formatStrings));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
EventSourceImpl::DispatchFailConnection()
{
  AssertIsOnMainThread();
  if (IsClosed()) {
    return;
  }
  nsresult rv = ConsoleError();
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to print to the console error");
  }
  rv = Dispatch(NewRunnableMethod("dom::EventSourceImpl::FailConnection",
                                  this,
                                  &EventSourceImpl::FailConnection),
                NS_DISPATCH_NORMAL);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
}

void
EventSourceImpl::FailConnection()
{
  AssertIsOnTargetThread();
  if (IsClosed()) {
    return;
  }
  // Must change state to closed before firing event to content.
  SetReadyState(CLOSED);
  // When a user agent is to fail the connection, the user agent must set the
  // readyState attribute to CLOSED and queue a task to fire a simple event
  // named error at the EventSource object.
  nsresult rv = mEventSource->CheckInnerWindowCorrectness();
  if (NS_SUCCEEDED(rv)) {
    rv = mEventSource->CreateAndDispatchSimpleEvent(NS_LITERAL_STRING("error"));
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to dispatch the error event!!!");
    }
  }
  // Call CloseInternal in the end of function because it may release
  // EventSourceImpl.
  CloseInternal();
}

// static
void
EventSourceImpl::TimerCallback(nsITimer* aTimer, void* aClosure)
{
  AssertIsOnMainThread();
  RefPtr<EventSourceImpl> thisObject = static_cast<EventSourceImpl*>(aClosure);

  if (thisObject->IsClosed()) {
    return;
  }

  NS_PRECONDITION(!thisObject->mHttpChannel,
                  "the channel hasn't been cancelled!!");

  if (!thisObject->IsFrozen()) {
    nsresult rv = thisObject->InitChannelAndRequestEventSource();
    if (NS_FAILED(rv)) {
      NS_WARNING("thisObject->InitChannelAndRequestEventSource() failed");
      return;
    }
  }
}

nsresult
EventSourceImpl::Thaw()
{
  AssertIsOnMainThread();
  if (IsClosed() || !IsFrozen()) {
    return NS_OK;
  }

  MOZ_ASSERT(!mHttpChannel, "the connection hasn't been closed!!!");

  SetFrozen(false);
  nsresult rv;
  if (!mGoingToDispatchAllMessages && mMessagesToDispatch.GetSize() > 0) {
    nsCOMPtr<nsIRunnable> event =
      NewRunnableMethod("dom::EventSourceImpl::DispatchAllMessageEvents",
                        this,
                        &EventSourceImpl::DispatchAllMessageEvents);
    NS_ENSURE_STATE(event);

    mGoingToDispatchAllMessages = true;

    rv = Dispatch(event.forget(), NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = InitChannelAndRequestEventSource();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
EventSourceImpl::Freeze()
{
  AssertIsOnMainThread();
  if (IsClosed() || IsFrozen()) {
    return NS_OK;
  }

  MOZ_ASSERT(!mHttpChannel, "the connection hasn't been closed!!!");
  SetFrozen(true);
  return NS_OK;
}

nsresult
EventSourceImpl::DispatchCurrentMessageEvent()
{
  AssertIsOnTargetThread();
  MOZ_ASSERT(!IsShutDown());
  UniquePtr<Message> message(Move(mCurrentMessage));
  ClearFields();

  if (!message || message->mData.IsEmpty()) {
    return NS_OK;
  }

  // removes the trailing LF from mData
  MOZ_ASSERT(message->mData.CharAt(message->mData.Length() - 1) == LF_CHAR,
             "Invalid trailing character! LF was expected instead.");
  message->mData.SetLength(message->mData.Length() - 1);

  if (message->mEventName.IsEmpty()) {
    message->mEventName.AssignLiteral("message");
  }

  if (message->mLastEventID.IsEmpty() && !mLastEventID.IsEmpty()) {
    message->mLastEventID.Assign(mLastEventID);
  }

  size_t sizeBefore = mMessagesToDispatch.GetSize();
  mMessagesToDispatch.Push(message.release());
  NS_ENSURE_TRUE(mMessagesToDispatch.GetSize() == sizeBefore + 1,
                 NS_ERROR_OUT_OF_MEMORY);


  if (!mGoingToDispatchAllMessages) {
    nsCOMPtr<nsIRunnable> event =
      NewRunnableMethod("dom::EventSourceImpl::DispatchAllMessageEvents",
                        this,
                        &EventSourceImpl::DispatchAllMessageEvents);
    NS_ENSURE_STATE(event);

    mGoingToDispatchAllMessages = true;

    return Dispatch(event.forget(), NS_DISPATCH_NORMAL);
  }

  return NS_OK;
}

void
EventSourceImpl::DispatchAllMessageEvents()
{
  AssertIsOnTargetThread();
  mGoingToDispatchAllMessages = false;

  if (IsClosed() || IsFrozen()) {
    return;
  }

  nsresult rv = mEventSource->CheckInnerWindowCorrectness();
  if (NS_FAILED(rv)) {
    return;
  }

  AutoJSAPI jsapi;
  if (mIsMainThread) {
    if (NS_WARN_IF(!jsapi.Init(mEventSource->GetOwner()))) {
      return;
    }
  } else {
    MOZ_ASSERT(mWorkerRef);
    if (NS_WARN_IF(!jsapi.Init(mWorkerRef->Private()->GlobalScope()))) {
      return;
    }
  }
  JSContext* cx = jsapi.cx();

  while (mMessagesToDispatch.GetSize() > 0) {
    UniquePtr<Message> message(static_cast<Message*>(mMessagesToDispatch.PopFront()));
    // Now we can turn our string into a jsval
    JS::Rooted<JS::Value> jsData(cx);
    {
      JSString* jsString;
      jsString = JS_NewUCStringCopyN(cx,
                                     message->mData.get(),
                                     message->mData.Length());
      NS_ENSURE_TRUE_VOID(jsString);

      jsData.setString(jsString);
    }

    // create an event that uses the MessageEvent interface,
    // which does not bubble, is not cancelable, and has no default action

    RefPtr<MessageEvent> event = new MessageEvent(mEventSource, nullptr,
                                                  nullptr);

    event->InitMessageEvent(nullptr, message->mEventName, false, false, jsData,
                            mOrigin, message->mLastEventID, nullptr,
                            Sequence<OwningNonNull<MessagePort>>());
    event->SetTrusted(true);

    IgnoredErrorResult err;
    mEventSource->DispatchEvent(*event, err);
    if (err.Failed()) {
      NS_WARNING("Failed to dispatch the message event!!!");
      return;
    }

    mLastEventID.Assign(message->mLastEventID);
    if (IsClosed() || IsFrozen()) {
      return;
    }
  }
}

void
EventSourceImpl::ClearFields()
{
  AssertIsOnTargetThread();
  mCurrentMessage = nullptr;
  mLastFieldName.Truncate();
  mLastFieldValue.Truncate();
}

nsresult
EventSourceImpl::SetFieldAndClear()
{
  MOZ_ASSERT(!IsShutDown());
  AssertIsOnTargetThread();
  if (mLastFieldName.IsEmpty()) {
    mLastFieldValue.Truncate();
    return NS_OK;
  }
  if (!mCurrentMessage) {
    mCurrentMessage = MakeUnique<Message>();
  }
  char16_t first_char;
  first_char = mLastFieldName.CharAt(0);

  // with no case folding performed
  switch (first_char) {
    case char16_t('d'):
      if (mLastFieldName.EqualsLiteral("data")) {
        // If the field name is "data" append the field value to the data
        // buffer, then append a single U+000A LINE FEED (LF) character
        // to the data buffer.
        mCurrentMessage->mData.Append(mLastFieldValue);
        mCurrentMessage->mData.Append(LF_CHAR);
      }
      break;

    case char16_t('e'):
      if (mLastFieldName.EqualsLiteral("event")) {
        mCurrentMessage->mEventName.Assign(mLastFieldValue);
      }
      break;

    case char16_t('i'):
      if (mLastFieldName.EqualsLiteral("id")) {
        mCurrentMessage->mLastEventID.Assign(mLastFieldValue);
      }
      break;

    case char16_t('r'):
      if (mLastFieldName.EqualsLiteral("retry")) {
        uint32_t newValue = 0;
        uint32_t i = 0;  // we must ensure that there are only digits
        bool assign = true;
        for (i = 0; i < mLastFieldValue.Length(); ++i) {
          if (mLastFieldValue.CharAt(i) < (char16_t)'0' ||
              mLastFieldValue.CharAt(i) > (char16_t)'9') {
            assign = false;
            break;
          }
          newValue = newValue*10 +
                     (((uint32_t)mLastFieldValue.CharAt(i))-
                       ((uint32_t)((char16_t)'0')));
        }

        if (assign) {
          if (newValue < MIN_RECONNECTION_TIME_VALUE) {
            mReconnectionTime = MIN_RECONNECTION_TIME_VALUE;
          } else if (newValue > MAX_RECONNECTION_TIME_VALUE) {
            mReconnectionTime = MAX_RECONNECTION_TIME_VALUE;
          } else {
            mReconnectionTime = newValue;
          }
        }
        break;
      }
      break;
  }

  mLastFieldName.Truncate();
  mLastFieldValue.Truncate();

  return NS_OK;
}

nsresult
EventSourceImpl::CheckHealthOfRequestCallback(nsIRequest* aRequestCallback)
{
  // This function could be run on target thread if http channel support
  // nsIThreadRetargetableRequest. otherwise, it's run on main thread.

  // check if we have been closed or if the request has been canceled
  // or if we have been frozen
  if (IsClosed() || IsFrozen() || !mHttpChannel) {
    return NS_ERROR_ABORT;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aRequestCallback);
  NS_ENSURE_STATE(httpChannel);

  if (httpChannel != mHttpChannel) {
    NS_WARNING("wrong channel from request callback");
    return NS_ERROR_ABORT;
  }

  return NS_OK;
}

nsresult
EventSourceImpl::ParseCharacter(char16_t aChr)
{
  AssertIsOnTargetThread();
  nsresult rv;

  if (IsClosed()) {
    return NS_ERROR_ABORT;
  }

  switch (mStatus) {
    case PARSE_STATE_OFF:
      NS_ERROR("Invalid state");
      return NS_ERROR_FAILURE;
      break;

    case PARSE_STATE_BEGIN_OF_STREAM:
      if (aChr == CR_CHAR) {
        mStatus = PARSE_STATE_CR_CHAR;
      } else if (aChr == LF_CHAR) {
        mStatus = PARSE_STATE_BEGIN_OF_LINE;
      } else if (aChr == COLON_CHAR) {
        mStatus = PARSE_STATE_COMMENT;
      } else {
        mLastFieldName += aChr;
        mStatus = PARSE_STATE_FIELD_NAME;
      }
      break;

    case PARSE_STATE_CR_CHAR:
      if (aChr == CR_CHAR) {
        rv = DispatchCurrentMessageEvent();  // there is an empty line (CRCR)
        NS_ENSURE_SUCCESS(rv, rv);
      } else if (aChr == LF_CHAR) {
        mStatus = PARSE_STATE_BEGIN_OF_LINE;
      } else if (aChr == COLON_CHAR) {
        mStatus = PARSE_STATE_COMMENT;
      } else {
        mLastFieldName += aChr;
        mStatus = PARSE_STATE_FIELD_NAME;
      }

      break;

    case PARSE_STATE_COMMENT:
      if (aChr == CR_CHAR) {
        mStatus = PARSE_STATE_CR_CHAR;
      } else if (aChr == LF_CHAR) {
        mStatus = PARSE_STATE_BEGIN_OF_LINE;
      }

      break;

    case PARSE_STATE_FIELD_NAME:
      if (aChr == CR_CHAR) {
        rv = SetFieldAndClear();
        NS_ENSURE_SUCCESS(rv, rv);

        mStatus = PARSE_STATE_CR_CHAR;
      } else if (aChr == LF_CHAR) {
        rv = SetFieldAndClear();
        NS_ENSURE_SUCCESS(rv, rv);

        mStatus = PARSE_STATE_BEGIN_OF_LINE;
      } else if (aChr == COLON_CHAR) {
        mStatus = PARSE_STATE_FIRST_CHAR_OF_FIELD_VALUE;
      } else {
        mLastFieldName += aChr;
      }

      break;

    case PARSE_STATE_FIRST_CHAR_OF_FIELD_VALUE:
      if (aChr == CR_CHAR) {
        rv = SetFieldAndClear();
        NS_ENSURE_SUCCESS(rv, rv);

        mStatus = PARSE_STATE_CR_CHAR;
      } else if (aChr == LF_CHAR) {
        rv = SetFieldAndClear();
        NS_ENSURE_SUCCESS(rv, rv);

        mStatus = PARSE_STATE_BEGIN_OF_LINE;
      } else if (aChr == SPACE_CHAR) {
        mStatus = PARSE_STATE_FIELD_VALUE;
      } else {
        mLastFieldValue += aChr;
        mStatus = PARSE_STATE_FIELD_VALUE;
      }

      break;

    case PARSE_STATE_FIELD_VALUE:
      if (aChr == CR_CHAR) {
        rv = SetFieldAndClear();
        NS_ENSURE_SUCCESS(rv, rv);

        mStatus = PARSE_STATE_CR_CHAR;
      } else if (aChr == LF_CHAR) {
        rv = SetFieldAndClear();
        NS_ENSURE_SUCCESS(rv, rv);

        mStatus = PARSE_STATE_BEGIN_OF_LINE;
      } else if (aChr != 0) {
        // Avoid appending the null char to the field value.
        mLastFieldValue += aChr;
      }

      break;

    case PARSE_STATE_BEGIN_OF_LINE:
      if (aChr == CR_CHAR) {
        rv = DispatchCurrentMessageEvent();  // there is an empty line
        NS_ENSURE_SUCCESS(rv, rv);

        mStatus = PARSE_STATE_CR_CHAR;
      } else if (aChr == LF_CHAR) {
        rv = DispatchCurrentMessageEvent();  // there is an empty line
        NS_ENSURE_SUCCESS(rv, rv);

        mStatus = PARSE_STATE_BEGIN_OF_LINE;
      } else if (aChr == COLON_CHAR) {
        mStatus = PARSE_STATE_COMMENT;
      } else if (aChr != 0) {
        // Avoid appending the null char to the field name.
        mLastFieldName += aChr;
        mStatus = PARSE_STATE_FIELD_NAME;
      }

      break;
  }

  return NS_OK;
}

void
EventSourceImpl::AddRefObject()
{
  AddRef();
}

void
EventSourceImpl::ReleaseObject()
{
  Release();
}

namespace {

class WorkerRunnableDispatcher final : public WorkerRunnable
{
  RefPtr<EventSourceImpl> mEventSourceImpl;

public:
  WorkerRunnableDispatcher(EventSourceImpl* aImpl,
                           WorkerPrivate* aWorkerPrivate,
                           already_AddRefed<nsIRunnable> aEvent)
    : WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
    , mEventSourceImpl(aImpl)
    , mEvent(Move(aEvent))
  {
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    aWorkerPrivate->AssertIsOnWorkerThread();
    return !NS_FAILED(mEvent->Run());
  }

  void PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aRunResult) override
  {
  }

  bool PreDispatch(WorkerPrivate* aWorkerPrivate) override
  {
    // We don't call WorkerRunnable::PreDispatch because it would assert the
    // wrong thing about which thread we're on.  We're on whichever thread the
    // channel implementation is running on (probably the main thread or
    // transport thread).
    return true;
  }

  void PostDispatch(WorkerPrivate* aWorkerPrivate,
                    bool aDispatchResult) override
  {
    // We don't call WorkerRunnable::PostDispatch because it would assert the
    // wrong thing about which thread we're on.  We're on whichever thread the
    // channel implementation is running on (probably the main thread or
    // transport thread).
  }

private:
  nsCOMPtr<nsIRunnable> mEvent;
};

} // namespace

bool EventSourceImpl::CreateWorkerRef(WorkerPrivate* aWorkerPrivate)
{
  MOZ_ASSERT(!IsShutDown());
  MOZ_ASSERT(!mWorkerRef);
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<EventSourceImpl> self = this;
  RefPtr<StrongWorkerRef> workerRef =
    StrongWorkerRef::Create(aWorkerPrivate, "EventSource", [self]() {
      self->Close();
    });

  if (NS_WARN_IF(!workerRef)) {
    return false;
  }

  mWorkerRef = new ThreadSafeWorkerRef(workerRef);
  return true;
}

void EventSourceImpl::ReleaseWorkerRef()
{
  MOZ_ASSERT(IsClosed());
  MOZ_ASSERT(IsCurrentThreadRunningWorker());
  mWorkerRef = nullptr;
}

//-----------------------------------------------------------------------------
// EventSourceImpl::nsIEventTarget
//-----------------------------------------------------------------------------
NS_IMETHODIMP
EventSourceImpl::DispatchFromScript(nsIRunnable* aEvent, uint32_t aFlags)
{
  nsCOMPtr<nsIRunnable> event(aEvent);
  return Dispatch(event.forget(), aFlags);
}

NS_IMETHODIMP
EventSourceImpl::Dispatch(already_AddRefed<nsIRunnable> aEvent, uint32_t aFlags)
{
  nsCOMPtr<nsIRunnable> event_ref(aEvent);
  if (mIsMainThread) {
    return NS_DispatchToMainThread(event_ref.forget());
  }

  if (IsShutDown()) {
    return NS_OK;
  }

  // If the target is a worker, we have to use a custom WorkerRunnableDispatcher
  // runnable.
  RefPtr<WorkerRunnableDispatcher> event =
    new WorkerRunnableDispatcher(this, mWorkerRef->Private(), event_ref.forget());

  if (!event->Dispatch()) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}


NS_IMETHODIMP
EventSourceImpl::DelayedDispatch(already_AddRefed<nsIRunnable> aEvent,
                                 uint32_t aDelayMs)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-----------------------------------------------------------------------------
// EventSourceImpl::nsIThreadRetargetableStreamListener
//-----------------------------------------------------------------------------
NS_IMETHODIMP
EventSourceImpl::CheckListenerChain()
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on the main thread!");
  return NS_OK;
}
////////////////////////////////////////////////////////////////////////////////
// EventSource
////////////////////////////////////////////////////////////////////////////////

EventSource::EventSource(nsPIDOMWindowInner* aOwnerWindow,
                         bool aWithCredentials)
  : DOMEventTargetHelper(aOwnerWindow)
  , mWithCredentials(aWithCredentials)
  , mIsMainThread(true)
  , mKeepingAlive(false)
{
  mImpl = new EventSourceImpl(this);
}

EventSource::~EventSource()
{
}

nsresult
EventSource::CreateAndDispatchSimpleEvent(const nsAString& aName)
{
  RefPtr<Event> event = NS_NewDOMEvent(this, nullptr, nullptr);
  // it doesn't bubble, and it isn't cancelable
  event->InitEvent(aName, false, false);
  event->SetTrusted(true);
  ErrorResult rv;
  DispatchEvent(*event, rv);
  return rv.StealNSResult();
}

/* static */ already_AddRefed<EventSource>
EventSource::Constructor(const GlobalObject& aGlobal, const nsAString& aURL,
                         const EventSourceInit& aEventSourceInitDict,
                         ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindowInner> ownerWindow =
    do_QueryInterface(aGlobal.GetAsSupports());

  MOZ_ASSERT(!NS_IsMainThread() || ownerWindow);

  RefPtr<EventSource> eventSource =
    new EventSource(ownerWindow, aEventSourceInitDict.mWithCredentials);
  RefPtr<EventSourceImpl> eventSourceImp = eventSource->mImpl;

  if (NS_IsMainThread()) {
    // Get principal from document and init EventSourceImpl
    nsCOMPtr<nsIScriptObjectPrincipal> scriptPrincipal =
      do_QueryInterface(aGlobal.GetAsSupports());
    if (!scriptPrincipal) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
    nsCOMPtr<nsIPrincipal> principal = scriptPrincipal->GetPrincipal();
    if (!principal) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
    eventSourceImp->Init(principal, aURL, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    eventSourceImp->InitChannelAndRequestEventSource();
    return eventSource.forget();
  }

  // Worker side.
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  RefPtr<InitRunnable> initRunnable =
    new InitRunnable(workerPrivate, eventSourceImp, aURL);
  initRunnable->Dispatch(Terminating, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  aRv = initRunnable->ErrorCode();
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // In workers we have to keep the worker alive using a WorkerRef in order
  // to dispatch messages correctly.
  if (!eventSourceImp->CreateWorkerRef(workerPrivate)) {
    // The worker is already shutting down. Let's return an already closed
    // object, but marked as Connecting.
    eventSource->Close();

    // EventSourceImpl must be released before returning the object, otherwise
    // it will set EventSource to a CLOSED state in its DTOR.
    eventSourceImp = nullptr;

    eventSource->mReadyState = EventSourceImpl::CONNECTING;
    return eventSource.forget();
  }

  // Let's connect to the server.
  RefPtr<ConnectRunnable> connectRunnable =
    new ConnectRunnable(workerPrivate, eventSourceImp);
  connectRunnable->Dispatch(Terminating, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return eventSource.forget();
}

// nsWrapperCache
JSObject*
EventSource::WrapObject(JSContext* aCx,
                        JS::Handle<JSObject*> aGivenProto)
{
  return EventSourceBinding::Wrap(aCx, this, aGivenProto);
}

void
EventSource::Close()
{
  AssertIsOnTargetThread();
  if (mImpl) {
    mImpl->Close();
  }
}

void
EventSource::UpdateMustKeepAlive()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mImpl);
  if (mKeepingAlive) {
    return;
  }
  mKeepingAlive = true;
  mImpl->AddRefObject();
}

void
EventSource::UpdateDontKeepAlive()
{
  // Here we could not have mImpl.
  MOZ_ASSERT(NS_IsMainThread() == mIsMainThread);
  if (mKeepingAlive) {
    MOZ_ASSERT(mImpl);
    mKeepingAlive = false;
    mImpl->mEventSource = nullptr;
    mImpl->ReleaseObject();
  }
  mImpl = nullptr;
}

//-----------------------------------------------------------------------------
// EventSource::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_CYCLE_COLLECTION_CLASS(EventSource)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(EventSource,
                                                  DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(EventSource,
                                                DOMEventTargetHelper)
  if (tmp->mImpl) {
    tmp->mImpl->Close();
    MOZ_ASSERT(!tmp->mImpl);
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

bool
EventSource::IsCertainlyAliveForCC() const
{
  return mKeepingAlive;
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(EventSource)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(EventSource, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(EventSource, DOMEventTargetHelper)

} // namespace dom
} // namespace mozilla
