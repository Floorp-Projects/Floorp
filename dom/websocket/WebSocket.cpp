/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebSocket.h"
#include "mozilla/dom/WebSocketBinding.h"
#include "mozilla/net/WebSocketChannel.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "mozilla/Atomics.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/net/WebSocketChannel.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/MessageEvent.h"
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/nsCSPContext.h"
#include "mozilla/dom/nsCSPUtils.h"
#include "mozilla/dom/nsHTTPSOnlyUtils.h"
#include "mozilla/dom/nsMixedContentBlocker.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/SerializedStackHolder.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/dom/WindowContext.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/LoadInfo.h"
#include "nsGlobalWindow.h"
#include "nsIScriptGlobalObject.h"
#include "mozilla/dom/Document.h"
#include "nsXPCOM.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "nsICookieJarSettings.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIURL.h"
#include "nsThreadUtils.h"
#include "nsIPromptFactory.h"
#include "nsIWindowWatcher.h"
#include "nsIPrompt.h"
#include "nsIStringBundle.h"
#include "nsIConsoleService.h"
#include "mozilla/dom/CloseEvent.h"
#include "mozilla/net/WebSocketEventService.h"
#include "nsJSUtils.h"
#include "nsIScriptError.h"
#include "nsNetUtil.h"
#include "nsIAuthPrompt.h"
#include "nsIAuthPrompt2.h"
#include "nsILoadGroup.h"
#include "mozilla/Preferences.h"
#include "xpcpublic.h"
#include "nsContentPolicyUtils.h"
#include "nsWrapperCacheInlines.h"
#include "nsIObserverService.h"
#include "nsIEventTarget.h"
#include "nsIInterfaceRequestor.h"
#include "nsIObserver.h"
#include "nsIRequest.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsIWebSocketChannel.h"
#include "nsIWebSocketListener.h"
#include "nsProxyRelease.h"
#include "nsWeakReference.h"
#include "nsIWebSocketImpl.h"

#define OPEN_EVENT_STRING u"open"_ns
#define MESSAGE_EVENT_STRING u"message"_ns
#define ERROR_EVENT_STRING u"error"_ns
#define CLOSE_EVENT_STRING u"close"_ns

using namespace mozilla::net;

namespace mozilla {
namespace dom {

class WebSocketImpl;

// This class is responsible for proxying nsIObserver and nsIWebSocketImpl
// interfaces to WebSocketImpl. WebSocketImplProxy should be only accessed on
// main thread, so we can let it support weak reference.
class WebSocketImplProxy final : public nsIObserver,
                                 public nsSupportsWeakReference,
                                 public nsIWebSocketImpl {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIWEBSOCKETIMPL

  explicit WebSocketImplProxy(WebSocketImpl* aOwner) : mOwner(aOwner) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  void Disconnect() {
    MOZ_ASSERT(NS_IsMainThread());

    mOwner = nullptr;
  }

 private:
  ~WebSocketImplProxy() = default;

  RefPtr<WebSocketImpl> mOwner;
};

class WebSocketImpl final : public nsIInterfaceRequestor,
                            public nsIWebSocketListener,
                            public nsIObserver,
                            public nsIRequest,
                            public nsIEventTarget,
                            public nsIWebSocketImpl {
 public:
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIWEBSOCKETLISTENER
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIREQUEST
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEVENTTARGET_FULL
  NS_DECL_NSIWEBSOCKETIMPL

  explicit WebSocketImpl(WebSocket* aWebSocket)
      : mWebSocket(aWebSocket),
        mIsServerSide(false),
        mSecure(false),
        mOnCloseScheduled(false),
        mFailed(false),
        mDisconnectingOrDisconnected(false),
        mCloseEventWasClean(false),
        mCloseEventCode(nsIWebSocketChannel::CLOSE_ABNORMAL),
        mPort(0),
        mScriptLine(0),
        mScriptColumn(0),
        mInnerWindowID(0),
        mPrivateBrowsing(false),
        mIsChromeContext(false),
        mIsMainThread(true),
        mMutex("WebSocketImpl::mMutex"),
        mWorkerShuttingDown(false) {
    if (!NS_IsMainThread()) {
      mIsMainThread = false;
    }
  }

  void AssertIsOnTargetThread() const { MOZ_ASSERT(IsTargetThread()); }

  bool IsTargetThread() const;

  nsresult Init(JSContext* aCx, nsIPrincipal* aLoadingPrincipal,
                nsIPrincipal* aPrincipal, bool aIsServerSide,
                const nsAString& aURL, nsTArray<nsString>& aProtocolArray,
                const nsACString& aScriptFile, uint32_t aScriptLine,
                uint32_t aScriptColumn);

  nsresult AsyncOpen(nsIPrincipal* aPrincipal, uint64_t aInnerWindowID,
                     nsITransportProvider* aTransportProvider,
                     const nsACString& aNegotiatedExtensions,
                     UniquePtr<SerializedStackHolder> aOriginStack);

  nsresult ParseURL(const nsAString& aURL);
  nsresult InitializeConnection(nsIPrincipal* aPrincipal,
                                nsICookieJarSettings* aCookieJarSettings);

  // These methods when called can release the WebSocket object
  void FailConnection(uint16_t reasonCode,
                      const nsACString& aReasonString = ""_ns);
  nsresult CloseConnection(uint16_t reasonCode,
                           const nsACString& aReasonString = ""_ns);
  void Disconnect();
  void DisconnectInternal();

  nsresult ConsoleError();
  void PrintErrorOnConsole(const char* aBundleURI, const char* aError,
                           nsTArray<nsString>&& aFormatStrings);

  nsresult DoOnMessageAvailable(const nsACString& aMsg, bool isBinary);

  // ConnectionCloseEvents: 'error' event if needed, then 'close' event.
  nsresult ScheduleConnectionCloseEvents(nsISupports* aContext,
                                         nsresult aStatusCode);
  // 2nd half of ScheduleConnectionCloseEvents, run in its own event.
  void DispatchConnectionCloseEvents();

  nsresult UpdateURI();

  void AddRefObject();
  void ReleaseObject();

  bool RegisterWorkerRef(WorkerPrivate* aWorkerPrivate);
  void UnregisterWorkerRef();

  nsresult CancelInternal();

  nsresult GetLoadingPrincipal(nsIPrincipal** aPrincipal);

  RefPtr<WebSocket> mWebSocket;

  nsCOMPtr<nsIWebSocketChannel> mChannel;

  bool mIsServerSide;  // True if we're implementing the server side of a
                       // websocket connection

  bool mSecure;  // if true it is using SSL and the wss scheme,
                 // otherwise it is using the ws scheme with no SSL

  bool mOnCloseScheduled;
  bool mFailed;
  Atomic<bool> mDisconnectingOrDisconnected;

  // Set attributes of DOM 'onclose' message
  bool mCloseEventWasClean;
  nsString mCloseEventReason;
  uint16_t mCloseEventCode;

  nsCString mAsciiHost;  // hostname
  uint32_t mPort;
  nsCString mResource;  // [filepath[?query]]
  nsString mUTF16Origin;

  nsCString mURI;
  nsCString mRequestedProtocolList;

  nsWeakPtr mOriginDocument;

  // Web Socket owner information:
  // - the script file name, UTF8 encoded.
  // - source code line number and column number where the Web Socket object
  //   was constructed.
  // - the ID of the Web Socket owner window. Note that this may not
  //   be the same as the inner window where the script lives.
  //   e.g within iframes
  // These attributes are used for error reporting.
  nsCString mScriptFile;
  uint32_t mScriptLine;
  uint32_t mScriptColumn;
  uint64_t mInnerWindowID;
  bool mPrivateBrowsing;
  bool mIsChromeContext;

  RefPtr<ThreadSafeWorkerRef> mWorkerRef;

  nsWeakPtr mWeakLoadGroup;

  bool mIsMainThread;

  // This mutex protects mWorkerShuttingDown.
  mozilla::Mutex mMutex;
  bool mWorkerShuttingDown;

  RefPtr<WebSocketEventService> mService;
  nsCOMPtr<nsIPrincipal> mLoadingPrincipal;

  // For dispatching runnables to main thread.
  nsCOMPtr<nsIEventTarget> mMainThreadEventTarget;

  RefPtr<WebSocketImplProxy> mImplProxy;

 private:
  ~WebSocketImpl() {
    MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread() == mIsMainThread ||
                          mDisconnectingOrDisconnected);

    // If we threw during Init we never called disconnect
    if (!mDisconnectingOrDisconnected) {
      Disconnect();
    }
  }
};

NS_IMPL_ISUPPORTS(WebSocketImplProxy, nsIObserver, nsISupportsWeakReference,
                  nsIWebSocketImpl)

NS_IMETHODIMP
WebSocketImplProxy::Observe(nsISupports* aSubject, const char* aTopic,
                            const char16_t* aData) {
  if (!mOwner) {
    return NS_OK;
  }

  return mOwner->Observe(aSubject, aTopic, aData);
}

NS_IMETHODIMP
WebSocketImplProxy::SendMessage(const nsAString& aMessage) {
  if (!mOwner) {
    return NS_OK;
  }

  return mOwner->SendMessage(aMessage);
}

NS_IMPL_ISUPPORTS(WebSocketImpl, nsIInterfaceRequestor, nsIWebSocketListener,
                  nsIObserver, nsIRequest, nsIEventTarget, nsIWebSocketImpl)

class CallDispatchConnectionCloseEvents final : public DiscardableRunnable {
 public:
  explicit CallDispatchConnectionCloseEvents(WebSocketImpl* aWebSocketImpl)
      : DiscardableRunnable("dom::CallDispatchConnectionCloseEvents"),
        mWebSocketImpl(aWebSocketImpl) {
    aWebSocketImpl->AssertIsOnTargetThread();
  }

  NS_IMETHOD Run() override {
    mWebSocketImpl->AssertIsOnTargetThread();
    mWebSocketImpl->DispatchConnectionCloseEvents();
    return NS_OK;
  }

 private:
  RefPtr<WebSocketImpl> mWebSocketImpl;
};

//-----------------------------------------------------------------------------
// WebSocketImpl
//-----------------------------------------------------------------------------

namespace {

class PrintErrorOnConsoleRunnable final : public WorkerMainThreadRunnable {
 public:
  PrintErrorOnConsoleRunnable(WebSocketImpl* aImpl, const char* aBundleURI,
                              const char* aError,
                              nsTArray<nsString>&& aFormatStrings)
      : WorkerMainThreadRunnable(aImpl->mWorkerRef->Private(),
                                 "WebSocket :: print error on console"_ns),
        mImpl(aImpl),
        mBundleURI(aBundleURI),
        mError(aError),
        mFormatStrings(std::move(aFormatStrings)) {}

  bool MainThreadRun() override {
    mImpl->PrintErrorOnConsole(mBundleURI, mError, std::move(mFormatStrings));
    return true;
  }

 private:
  // Raw pointer because this runnable is sync.
  WebSocketImpl* mImpl;

  const char* mBundleURI;
  const char* mError;
  nsTArray<nsString> mFormatStrings;
};

}  // namespace

void WebSocketImpl::PrintErrorOnConsole(const char* aBundleURI,
                                        const char* aError,
                                        nsTArray<nsString>&& aFormatStrings) {
  // This method must run on the main thread.

  if (!NS_IsMainThread()) {
    MOZ_ASSERT(mWorkerRef);

    RefPtr<PrintErrorOnConsoleRunnable> runnable =
        new PrintErrorOnConsoleRunnable(this, aBundleURI, aError,
                                        std::move(aFormatStrings));
    ErrorResult rv;
    runnable->Dispatch(Killing, rv);
    // XXXbz this seems totally broken.  We should be propagating this out, but
    // none of our callers really propagate anything usefully.  Come to think of
    // it, why is this a syncrunnable anyway?  Can't this be a fire-and-forget
    // runnable??
    rv.SuppressException();
    return;
  }

  nsresult rv;
  nsCOMPtr<nsIStringBundleService> bundleService =
      do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS_VOID(rv);

  nsCOMPtr<nsIStringBundle> strBundle;
  rv = bundleService->CreateBundle(aBundleURI, getter_AddRefs(strBundle));
  NS_ENSURE_SUCCESS_VOID(rv);

  nsCOMPtr<nsIConsoleService> console(
      do_GetService(NS_CONSOLESERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS_VOID(rv);

  nsCOMPtr<nsIScriptError> errorObject(
      do_CreateInstance(NS_SCRIPTERROR_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS_VOID(rv);

  // Localize the error message
  nsAutoString message;
  if (!aFormatStrings.IsEmpty()) {
    rv = strBundle->FormatStringFromName(aError, aFormatStrings, message);
  } else {
    rv = strBundle->GetStringFromName(aError, message);
  }
  NS_ENSURE_SUCCESS_VOID(rv);

  if (mInnerWindowID) {
    rv = errorObject->InitWithWindowID(
        message, NS_ConvertUTF8toUTF16(mScriptFile), u""_ns, mScriptLine,
        mScriptColumn, nsIScriptError::errorFlag, "Web Socket", mInnerWindowID);
  } else {
    rv =
        errorObject->Init(message, NS_ConvertUTF8toUTF16(mScriptFile), u""_ns,
                          mScriptLine, mScriptColumn, nsIScriptError::errorFlag,
                          "Web Socket", mPrivateBrowsing, mIsChromeContext);
  }

  NS_ENSURE_SUCCESS_VOID(rv);

  // print the error message directly to the JS console
  rv = console->LogMessage(errorObject);
  NS_ENSURE_SUCCESS_VOID(rv);
}

namespace {

class CancelWebSocketRunnable final : public Runnable {
 public:
  CancelWebSocketRunnable(nsIWebSocketChannel* aChannel, uint16_t aReasonCode,
                          const nsACString& aReasonString)
      : Runnable("dom::CancelWebSocketRunnable"),
        mChannel(aChannel),
        mReasonCode(aReasonCode),
        mReasonString(aReasonString) {}

  NS_IMETHOD Run() override {
    nsresult rv = mChannel->Close(mReasonCode, mReasonString);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to dispatch the close message");
    }
    return NS_OK;
  }

 private:
  nsCOMPtr<nsIWebSocketChannel> mChannel;
  uint16_t mReasonCode;
  nsCString mReasonString;
};

class MOZ_STACK_CLASS MaybeDisconnect {
 public:
  explicit MaybeDisconnect(WebSocketImpl* aImpl) : mImpl(aImpl) {}

  ~MaybeDisconnect() {
    bool toDisconnect = false;

    {
      MutexAutoLock lock(mImpl->mMutex);
      toDisconnect = mImpl->mWorkerShuttingDown;
    }

    if (toDisconnect) {
      mImpl->Disconnect();
    }
  }

 private:
  WebSocketImpl* mImpl;
};

class CloseConnectionRunnable final : public Runnable {
 public:
  CloseConnectionRunnable(WebSocketImpl* aImpl, uint16_t aReasonCode,
                          const nsACString& aReasonString)
      : Runnable("dom::CloseConnectionRunnable"),
        mImpl(aImpl),
        mReasonCode(aReasonCode),
        mReasonString(aReasonString) {}

  NS_IMETHOD Run() override {
    return mImpl->CloseConnection(mReasonCode, mReasonString);
  }

 private:
  RefPtr<WebSocketImpl> mImpl;
  uint16_t mReasonCode;
  const nsCString mReasonString;
};

}  // namespace

nsresult WebSocketImpl::CloseConnection(uint16_t aReasonCode,
                                        const nsACString& aReasonString) {
  if (!IsTargetThread()) {
    nsCOMPtr<nsIRunnable> runnable =
        new CloseConnectionRunnable(this, aReasonCode, aReasonString);
    return Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
  }

  AssertIsOnTargetThread();

  if (mDisconnectingOrDisconnected) {
    return NS_OK;
  }

  // If this method is called because the worker is going away, we will not
  // receive the OnStop() method and we have to disconnect the WebSocket and
  // release the ThreadSafeWorkerRef.
  MaybeDisconnect md(this);

  uint16_t readyState = mWebSocket->ReadyState();
  if (readyState == WebSocket::CLOSING || readyState == WebSocket::CLOSED) {
    return NS_OK;
  }

  // The common case...
  if (mChannel) {
    mWebSocket->SetReadyState(WebSocket::CLOSING);

    // The channel has to be closed on the main-thread.

    if (NS_IsMainThread()) {
      return mChannel->Close(aReasonCode, aReasonString);
    }

    RefPtr<CancelWebSocketRunnable> runnable =
        new CancelWebSocketRunnable(mChannel, aReasonCode, aReasonString);
    return NS_DispatchToMainThread(runnable);
  }

  // No channel, but not disconnected: canceled or failed early
  MOZ_ASSERT(readyState == WebSocket::CONNECTING,
             "Should only get here for early websocket cancel/error");

  // Server won't be sending us a close code, so use what's passed in here.
  mCloseEventCode = aReasonCode;
  CopyUTF8toUTF16(aReasonString, mCloseEventReason);

  mWebSocket->SetReadyState(WebSocket::CLOSING);

  ScheduleConnectionCloseEvents(
      nullptr, (aReasonCode == nsIWebSocketChannel::CLOSE_NORMAL ||
                aReasonCode == nsIWebSocketChannel::CLOSE_GOING_AWAY)
                   ? NS_OK
                   : NS_ERROR_FAILURE);

  return NS_OK;
}

nsresult WebSocketImpl::ConsoleError() {
  AssertIsOnTargetThread();

  {
    MutexAutoLock lock(mMutex);
    if (mWorkerShuttingDown) {
      // Too late to report anything, bail out.
      return NS_OK;
    }
  }

  nsTArray<nsString> formatStrings;
  CopyUTF8toUTF16(mURI, *formatStrings.AppendElement());

  if (mWebSocket->ReadyState() < WebSocket::OPEN) {
    PrintErrorOnConsole("chrome://global/locale/appstrings.properties",
                        "connectionFailure", std::move(formatStrings));
  } else {
    PrintErrorOnConsole("chrome://global/locale/appstrings.properties",
                        "netInterrupt", std::move(formatStrings));
  }
  /// todo some specific errors - like for message too large
  return NS_OK;
}

void WebSocketImpl::FailConnection(uint16_t aReasonCode,
                                   const nsACString& aReasonString) {
  AssertIsOnTargetThread();

  if (mDisconnectingOrDisconnected) {
    return;
  }

  ConsoleError();
  mFailed = true;
  CloseConnection(aReasonCode, aReasonString);

  if (NS_IsMainThread() && mImplProxy) {
    mImplProxy->Disconnect();
    mImplProxy = nullptr;
  }
}

namespace {

class DisconnectInternalRunnable final : public WorkerMainThreadRunnable {
 public:
  explicit DisconnectInternalRunnable(WebSocketImpl* aImpl)
      : WorkerMainThreadRunnable(GetCurrentThreadWorkerPrivate(),
                                 "WebSocket :: disconnect"_ns),
        mImpl(aImpl) {}

  bool MainThreadRun() override {
    mImpl->DisconnectInternal();
    return true;
  }

 private:
  // A raw pointer because this runnable is sync.
  WebSocketImpl* mImpl;
};

}  // namespace

void WebSocketImpl::Disconnect() {
  MOZ_DIAGNOSTIC_ASSERT(NS_IsMainThread() == mIsMainThread);

  if (mDisconnectingOrDisconnected) {
    return;
  }

  // DontKeepAliveAnyMore() and DisconnectInternal() can release the object. So
  // hold a reference to this until the end of the method.
  RefPtr<WebSocketImpl> kungfuDeathGrip = this;

  // Disconnect can be called from some control event (such as a callback from
  // StrongWorkerRef). This will be schedulated before any other sync/async
  // runnable. In order to prevent some double Disconnect() calls, we use this
  // boolean.
  mDisconnectingOrDisconnected = true;

  // DisconnectInternal touches observers and nsILoadGroup and it must run on
  // the main thread.

  if (NS_IsMainThread()) {
    DisconnectInternal();

    // If we haven't called WebSocket::DisconnectFromOwner yet, update
    // web socket count here.
    if (mWebSocket->GetOwner()) {
      mWebSocket->GetOwner()->UpdateWebSocketCount(-1);
    }
  } else {
    RefPtr<DisconnectInternalRunnable> runnable =
        new DisconnectInternalRunnable(this);
    ErrorResult rv;
    runnable->Dispatch(Killing, rv);
    // XXXbz this seems totally broken.  We should be propagating this out, but
    // where to, exactly?
    rv.SuppressException();
  }

  NS_ReleaseOnMainThread("WebSocketImpl::mChannel", mChannel.forget());
  NS_ReleaseOnMainThread("WebSocketImpl::mService", mService.forget());

  mWebSocket->DontKeepAliveAnyMore();
  mWebSocket->mImpl = nullptr;

  if (mWorkerRef) {
    UnregisterWorkerRef();
  }

  // We want to release the WebSocket in the correct thread.
  mWebSocket = nullptr;
}

void WebSocketImpl::DisconnectInternal() {
  AssertIsOnMainThread();

  nsCOMPtr<nsILoadGroup> loadGroup = do_QueryReferent(mWeakLoadGroup);
  if (loadGroup) {
    loadGroup->RemoveRequest(this, nullptr, NS_OK);
    // mWeakLoadGroup has to be release on main-thread because WeakReferences
    // are not thread-safe.
    mWeakLoadGroup = nullptr;
  }

  if (!mWorkerRef) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->RemoveObserver(mImplProxy, DOM_WINDOW_DESTROYED_TOPIC);
      os->RemoveObserver(mImplProxy, DOM_WINDOW_FROZEN_TOPIC);
    }
  }

  if (mImplProxy) {
    mImplProxy->Disconnect();
    mImplProxy = nullptr;
  }
}

//-----------------------------------------------------------------------------
// WebSocketImpl::nsIWebSocketImpl
//-----------------------------------------------------------------------------

NS_IMETHODIMP
WebSocketImpl::SendMessage(const nsAString& aMessage) {
  nsString message(aMessage);
  nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction(
      "WebSocketImpl::SendMessage",
      [self = RefPtr<WebSocketImpl>(this), message = std::move(message)]() {
        ErrorResult IgnoredErrorResult;
        self->mWebSocket->Send(message, IgnoredErrorResult);
      });
  return Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
}

//-----------------------------------------------------------------------------
// WebSocketImpl::nsIWebSocketListener methods:
//-----------------------------------------------------------------------------

nsresult WebSocketImpl::DoOnMessageAvailable(const nsACString& aMsg,
                                             bool isBinary) {
  AssertIsOnTargetThread();

  if (mDisconnectingOrDisconnected) {
    return NS_OK;
  }

  int16_t readyState = mWebSocket->ReadyState();
  if (readyState == WebSocket::CLOSED) {
    NS_ERROR("Received message after CLOSED");
    return NS_ERROR_UNEXPECTED;
  }

  if (readyState == WebSocket::OPEN) {
    // Dispatch New Message
    nsresult rv = mWebSocket->CreateAndDispatchMessageEvent(aMsg, isBinary);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to dispatch the message event");
    }

    return NS_OK;
  }

  // CLOSING should be the only other state where it's possible to get msgs
  // from channel: Spec says to drop them.
  MOZ_ASSERT(readyState == WebSocket::CLOSING,
             "Received message while CONNECTING or CLOSED");
  return NS_OK;
}

NS_IMETHODIMP
WebSocketImpl::OnMessageAvailable(nsISupports* aContext,
                                  const nsACString& aMsg) {
  AssertIsOnTargetThread();

  if (mDisconnectingOrDisconnected) {
    return NS_OK;
  }

  return DoOnMessageAvailable(aMsg, false);
}

NS_IMETHODIMP
WebSocketImpl::OnBinaryMessageAvailable(nsISupports* aContext,
                                        const nsACString& aMsg) {
  AssertIsOnTargetThread();

  if (mDisconnectingOrDisconnected) {
    return NS_OK;
  }

  return DoOnMessageAvailable(aMsg, true);
}

NS_IMETHODIMP
WebSocketImpl::OnStart(nsISupports* aContext) {
  AssertIsOnTargetThread();

  if (mDisconnectingOrDisconnected) {
    return NS_OK;
  }

  int16_t readyState = mWebSocket->ReadyState();

  // This is the only function that sets OPEN, and should be called only once
  MOZ_ASSERT(readyState != WebSocket::OPEN,
             "readyState already OPEN! OnStart called twice?");

  // Nothing to do if we've already closed/closing
  if (readyState != WebSocket::CONNECTING) {
    return NS_OK;
  }

  // Attempt to kill "ghost" websocket: but usually too early for check to fail
  nsresult rv = mWebSocket->CheckCurrentGlobalCorrectness();
  if (NS_FAILED(rv)) {
    CloseConnection(nsIWebSocketChannel::CLOSE_GOING_AWAY);
    return rv;
  }

  if (!mRequestedProtocolList.IsEmpty()) {
    rv = mChannel->GetProtocol(mWebSocket->mEstablishedProtocol);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  rv = mChannel->GetExtensions(mWebSocket->mEstablishedExtensions);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  UpdateURI();

  mWebSocket->SetReadyState(WebSocket::OPEN);

  mService->WebSocketOpened(
      mChannel->Serial(), mInnerWindowID, mWebSocket->mEffectiveURL,
      mWebSocket->mEstablishedProtocol, mWebSocket->mEstablishedExtensions,
      mChannel->HttpChannelId());

  // Let's keep the object alive because the webSocket can be CCed in the
  // onopen callback.
  RefPtr<WebSocket> webSocket = mWebSocket;

  // Call 'onopen'
  rv = webSocket->CreateAndDispatchSimpleEvent(OPEN_EVENT_STRING);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch the open event");
  }

  webSocket->UpdateMustKeepAlive();
  return NS_OK;
}

NS_IMETHODIMP
WebSocketImpl::OnStop(nsISupports* aContext, nsresult aStatusCode) {
  AssertIsOnTargetThread();

  if (mDisconnectingOrDisconnected) {
    return NS_OK;
  }

  // We can be CONNECTING here if connection failed.
  // We can be OPEN if we have encountered a fatal protocol error
  // We can be CLOSING if close() was called and/or server initiated close.
  MOZ_ASSERT(mWebSocket->ReadyState() != WebSocket::CLOSED,
             "Shouldn't already be CLOSED when OnStop called");

  return ScheduleConnectionCloseEvents(aContext, aStatusCode);
}

nsresult WebSocketImpl::ScheduleConnectionCloseEvents(nsISupports* aContext,
                                                      nsresult aStatusCode) {
  AssertIsOnTargetThread();

  // no-op if some other code has already initiated close event
  if (!mOnCloseScheduled) {
    mCloseEventWasClean = NS_SUCCEEDED(aStatusCode);

    if (aStatusCode == NS_BASE_STREAM_CLOSED) {
      // don't generate an error event just because of an unclean close
      aStatusCode = NS_OK;
    }

    if (aStatusCode == NS_ERROR_NET_INADEQUATE_SECURITY) {
      // TLS negotiation failed so we need to set status code to 1015.
      mCloseEventCode = 1015;
    }

    if (NS_FAILED(aStatusCode)) {
      ConsoleError();
      mFailed = true;
    }

    mOnCloseScheduled = true;

    NS_DispatchToCurrentThread(new CallDispatchConnectionCloseEvents(this));
  }

  return NS_OK;
}

NS_IMETHODIMP
WebSocketImpl::OnAcknowledge(nsISupports* aContext, uint32_t aSize) {
  AssertIsOnTargetThread();

  if (mDisconnectingOrDisconnected) {
    return NS_OK;
  }

  MOZ_RELEASE_ASSERT(mWebSocket->mOutgoingBufferedAmount.isValid());
  if (aSize > mWebSocket->mOutgoingBufferedAmount.value()) {
    return NS_ERROR_UNEXPECTED;
  }

  CheckedUint64 outgoingBufferedAmount = mWebSocket->mOutgoingBufferedAmount;
  outgoingBufferedAmount -= aSize;
  if (!outgoingBufferedAmount.isValid()) {
    return NS_ERROR_UNEXPECTED;
  }

  mWebSocket->mOutgoingBufferedAmount = outgoingBufferedAmount;
  MOZ_RELEASE_ASSERT(mWebSocket->mOutgoingBufferedAmount.isValid());

  return NS_OK;
}

NS_IMETHODIMP
WebSocketImpl::OnServerClose(nsISupports* aContext, uint16_t aCode,
                             const nsACString& aReason) {
  AssertIsOnTargetThread();

  if (mDisconnectingOrDisconnected) {
    return NS_OK;
  }

  int16_t readyState = mWebSocket->ReadyState();

  MOZ_ASSERT(readyState != WebSocket::CONNECTING,
             "Received server close before connected?");
  MOZ_ASSERT(readyState != WebSocket::CLOSED,
             "Received server close after already closed!");

  // store code/string for onclose DOM event
  mCloseEventCode = aCode;
  CopyUTF8toUTF16(aReason, mCloseEventReason);

  if (readyState == WebSocket::OPEN) {
    // Server initiating close.
    // RFC 6455, 5.5.1: "When sending a Close frame in response, the endpoint
    // typically echos the status code it received".
    // But never send certain codes, per section 7.4.1
    if (aCode == 1005 || aCode == 1006 || aCode == 1015) {
      CloseConnection(0, ""_ns);
    } else {
      CloseConnection(aCode, aReason);
    }
  } else {
    // We initiated close, and server has replied: OnStop does rest of the work.
    MOZ_ASSERT(readyState == WebSocket::CLOSING, "unknown state");
  }

  return NS_OK;
}

NS_IMETHODIMP
WebSocketImpl::OnError() {
  if (!IsTargetThread()) {
    return Dispatch(
        NS_NewRunnableFunction("dom::FailConnectionRunnable",
                               [self = RefPtr{this}]() {
                                 self->FailConnection(
                                     nsIWebSocketChannel::CLOSE_ABNORMAL);
                               }),
        NS_DISPATCH_NORMAL);
  }

  AssertIsOnTargetThread();
  FailConnection(nsIWebSocketChannel::CLOSE_ABNORMAL);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// WebSocketImpl::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP
WebSocketImpl::GetInterface(const nsIID& aIID, void** aResult) {
  AssertIsOnMainThread();

  if (!mWebSocket || mWebSocket->ReadyState() == WebSocket::CLOSED) {
    return NS_ERROR_FAILURE;
  }

  if (aIID.Equals(NS_GET_IID(nsIAuthPrompt)) ||
      aIID.Equals(NS_GET_IID(nsIAuthPrompt2))) {
    nsCOMPtr<nsPIDOMWindowInner> win = mWebSocket->GetWindowIfCurrent();
    if (!win) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    nsresult rv;
    nsCOMPtr<nsIPromptFactory> wwatch =
        do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsPIDOMWindowOuter> outerWindow = win->GetOuterWindow();
    return wwatch->GetPrompt(outerWindow, aIID, aResult);
  }

  return QueryInterface(aIID, aResult);
}

////////////////////////////////////////////////////////////////////////////////
// WebSocket
////////////////////////////////////////////////////////////////////////////////

WebSocket::WebSocket(nsIGlobalObject* aGlobal)
    : DOMEventTargetHelper(aGlobal),
      mIsMainThread(true),
      mKeepingAlive(false),
      mCheckMustKeepAlive(true),
      mOutgoingBufferedAmount(0),
      mBinaryType(dom::BinaryType::Blob),
      mMutex("WebSocket::mMutex"),
      mReadyState(CONNECTING) {
  MOZ_ASSERT(aGlobal);

  mImpl = new WebSocketImpl(this);
  mIsMainThread = mImpl->mIsMainThread;
}

WebSocket::~WebSocket() = default;

mozilla::Maybe<EventCallbackDebuggerNotificationType>
WebSocket::GetDebuggerNotificationType() const {
  return mozilla::Some(EventCallbackDebuggerNotificationType::Websocket);
}

JSObject* WebSocket::WrapObject(JSContext* cx,
                                JS::Handle<JSObject*> aGivenProto) {
  return WebSocket_Binding::Wrap(cx, this, aGivenProto);
}

//---------------------------------------------------------------------------
// WebIDL
//---------------------------------------------------------------------------

// Constructor:
already_AddRefed<WebSocket> WebSocket::Constructor(
    const GlobalObject& aGlobal, const nsAString& aUrl,
    const StringOrStringSequence& aProtocols, ErrorResult& aRv) {
  if (aProtocols.IsStringSequence()) {
    return WebSocket::ConstructorCommon(
        aGlobal, aUrl, aProtocols.GetAsStringSequence(), nullptr, ""_ns, aRv);
  }

  Sequence<nsString> protocols;
  if (!protocols.AppendElement(aProtocols.GetAsString(), fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  return WebSocket::ConstructorCommon(aGlobal, aUrl, protocols, nullptr, ""_ns,
                                      aRv);
}

already_AddRefed<WebSocket> WebSocket::CreateServerWebSocket(
    const GlobalObject& aGlobal, const nsAString& aUrl,
    const Sequence<nsString>& aProtocols,
    nsITransportProvider* aTransportProvider,
    const nsAString& aNegotiatedExtensions, ErrorResult& aRv) {
  return WebSocket::ConstructorCommon(
      aGlobal, aUrl, aProtocols, aTransportProvider,
      NS_ConvertUTF16toUTF8(aNegotiatedExtensions), aRv);
}

namespace {

// This class is used to clear any exception.
class MOZ_STACK_CLASS ClearException {
 public:
  explicit ClearException(JSContext* aCx) : mCx(aCx) {}

  ~ClearException() { JS_ClearPendingException(mCx); }

 private:
  JSContext* mCx;
};

class WebSocketMainThreadRunnable : public WorkerMainThreadRunnable {
 public:
  WebSocketMainThreadRunnable(WorkerPrivate* aWorkerPrivate,
                              const nsACString& aTelemetryKey)
      : WorkerMainThreadRunnable(aWorkerPrivate, aTelemetryKey) {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool MainThreadRun() override {
    AssertIsOnMainThread();

    // Walk up to our containing page
    WorkerPrivate* wp = mWorkerPrivate;
    while (wp->GetParent()) {
      wp = wp->GetParent();
    }

    nsPIDOMWindowInner* window = wp->GetWindow();
    if (window) {
      return InitWithWindow(window);
    }

    return InitWindowless(wp);
  }

 protected:
  virtual bool InitWithWindow(nsPIDOMWindowInner* aWindow) = 0;

  virtual bool InitWindowless(WorkerPrivate* aTopLevelWorkerPrivate) = 0;
};

class InitRunnable final : public WebSocketMainThreadRunnable {
 public:
  InitRunnable(WorkerPrivate* aWorkerPrivate, WebSocketImpl* aImpl,
               bool aIsServerSide, const nsAString& aURL,
               nsTArray<nsString>& aProtocolArray,
               const nsACString& aScriptFile, uint32_t aScriptLine,
               uint32_t aScriptColumn)
      : WebSocketMainThreadRunnable(aWorkerPrivate, "WebSocket :: init"_ns),
        mImpl(aImpl),
        mIsServerSide(aIsServerSide),
        mURL(aURL),
        mProtocolArray(aProtocolArray),
        mScriptFile(aScriptFile),
        mScriptLine(aScriptLine),
        mScriptColumn(aScriptColumn),
        mErrorCode(NS_OK) {
    MOZ_ASSERT(mWorkerPrivate);
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  nsresult ErrorCode() const { return mErrorCode; }

 protected:
  virtual bool InitWithWindow(nsPIDOMWindowInner* aWindow) override {
    AutoJSAPI jsapi;
    if (NS_WARN_IF(!jsapi.Init(aWindow))) {
      mErrorCode = NS_ERROR_FAILURE;
      return true;
    }

    ClearException ce(jsapi.cx());

    Document* doc = aWindow->GetExtantDoc();
    if (!doc) {
      mErrorCode = NS_ERROR_FAILURE;
      return true;
    }

    mErrorCode =
        mImpl->Init(jsapi.cx(), mWorkerPrivate->GetPrincipal(),
                    doc->NodePrincipal(), mIsServerSide, mURL, mProtocolArray,
                    mScriptFile, mScriptLine, mScriptColumn);
    return true;
  }

  virtual bool InitWindowless(WorkerPrivate* aTopLevelWorkerPrivate) override {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aTopLevelWorkerPrivate && !aTopLevelWorkerPrivate->GetWindow());

    mErrorCode =
        mImpl->Init(nullptr, mWorkerPrivate->GetPrincipal(),
                    aTopLevelWorkerPrivate->GetPrincipal(), mIsServerSide, mURL,
                    mProtocolArray, mScriptFile, mScriptLine, mScriptColumn);
    return true;
  }

  // Raw pointer. This worker runnable runs synchronously.
  WebSocketImpl* mImpl;

  bool mIsServerSide;
  const nsAString& mURL;
  nsTArray<nsString>& mProtocolArray;
  nsCString mScriptFile;
  uint32_t mScriptLine;
  uint32_t mScriptColumn;
  nsresult mErrorCode;
};

class ConnectRunnable final : public WebSocketMainThreadRunnable {
 public:
  ConnectRunnable(WorkerPrivate* aWorkerPrivate, WebSocketImpl* aImpl)
      : WebSocketMainThreadRunnable(aWorkerPrivate, "WebSocket :: init"_ns),
        mImpl(aImpl),
        mConnectionFailed(true) {
    MOZ_ASSERT(mWorkerPrivate);
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool ConnectionFailed() const { return mConnectionFailed; }

 protected:
  virtual bool InitWithWindow(nsPIDOMWindowInner* aWindow) override {
    Document* doc = aWindow->GetExtantDoc();
    if (!doc) {
      return true;
    }

    mConnectionFailed = NS_FAILED(mImpl->InitializeConnection(
        doc->NodePrincipal(), mWorkerPrivate->CookieJarSettings()));
    return true;
  }

  virtual bool InitWindowless(WorkerPrivate* aTopLevelWorkerPrivate) override {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aTopLevelWorkerPrivate && !aTopLevelWorkerPrivate->GetWindow());

    mConnectionFailed = NS_FAILED(
        mImpl->InitializeConnection(aTopLevelWorkerPrivate->GetPrincipal(),
                                    mWorkerPrivate->CookieJarSettings()));
    return true;
  }

  // Raw pointer. This worker runnable runs synchronously.
  WebSocketImpl* mImpl;

  bool mConnectionFailed;
};

class AsyncOpenRunnable final : public WebSocketMainThreadRunnable {
 public:
  explicit AsyncOpenRunnable(WebSocketImpl* aImpl,
                             UniquePtr<SerializedStackHolder> aOriginStack)
      : WebSocketMainThreadRunnable(aImpl->mWorkerRef->Private(),
                                    "WebSocket :: AsyncOpen"_ns),
        mImpl(aImpl),
        mOriginStack(std::move(aOriginStack)),
        mErrorCode(NS_OK) {
    MOZ_ASSERT(mWorkerPrivate);
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  nsresult ErrorCode() const { return mErrorCode; }

 protected:
  virtual bool InitWithWindow(nsPIDOMWindowInner* aWindow) override {
    AssertIsOnMainThread();
    MOZ_ASSERT(aWindow);

    Document* doc = aWindow->GetExtantDoc();
    if (!doc) {
      mErrorCode = NS_ERROR_FAILURE;
      return true;
    }

    nsCOMPtr<nsIPrincipal> principal = doc->PartitionedPrincipal();
    if (!principal) {
      mErrorCode = NS_ERROR_FAILURE;
      return true;
    }

    uint64_t windowID = 0;
    if (WindowContext* wc = aWindow->GetWindowContext()) {
      windowID = wc->InnerWindowId();
    }

    mErrorCode = mImpl->AsyncOpen(principal, windowID, nullptr, ""_ns,
                                  std::move(mOriginStack));
    return true;
  }

  virtual bool InitWindowless(WorkerPrivate* aTopLevelWorkerPrivate) override {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aTopLevelWorkerPrivate && !aTopLevelWorkerPrivate->GetWindow());

    mErrorCode =
        mImpl->AsyncOpen(aTopLevelWorkerPrivate->GetPartitionedPrincipal(), 0,
                         nullptr, ""_ns, nullptr);
    return true;
  }

 private:
  // Raw pointer. This worker runs synchronously.
  WebSocketImpl* mImpl;

  UniquePtr<SerializedStackHolder> mOriginStack;

  nsresult mErrorCode;
};

}  // namespace

already_AddRefed<WebSocket> WebSocket::ConstructorCommon(
    const GlobalObject& aGlobal, const nsAString& aUrl,
    const Sequence<nsString>& aProtocols,
    nsITransportProvider* aTransportProvider,
    const nsACString& aNegotiatedExtensions, ErrorResult& aRv) {
  MOZ_ASSERT_IF(!aTransportProvider, aNegotiatedExtensions.IsEmpty());
  nsCOMPtr<nsIPrincipal> principal;
  nsCOMPtr<nsIPrincipal> partitionedPrincipal;

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (NS_WARN_IF(!global)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (NS_IsMainThread()) {
    nsCOMPtr<nsIScriptObjectPrincipal> scriptPrincipal =
        do_QueryInterface(aGlobal.GetAsSupports());
    if (!scriptPrincipal) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    principal = scriptPrincipal->GetPrincipal();
    partitionedPrincipal = scriptPrincipal->PartitionedPrincipal();
    if (!principal || !partitionedPrincipal) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
  }

  nsTArray<nsString> protocolArray;

  for (uint32_t index = 0, len = aProtocols.Length(); index < len; ++index) {
    const nsString& protocolElement = aProtocols[index];

    if (protocolElement.IsEmpty()) {
      aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
      return nullptr;
    }
    if (protocolArray.Contains(protocolElement)) {
      aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
      return nullptr;
    }
    if (protocolElement.FindChar(',') != -1) /* interferes w/list */ {
      aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
      return nullptr;
    }

    protocolArray.AppendElement(protocolElement);
  }

  RefPtr<WebSocket> webSocket = new WebSocket(global);
  RefPtr<WebSocketImpl> webSocketImpl = webSocket->mImpl;

  bool connectionFailed = true;

  if (NS_IsMainThread()) {
    // We're keeping track of all main thread web sockets to be able to
    // avoid throttling timeouts when we have active web sockets.
    if (webSocket->GetOwner()) {
      webSocket->GetOwner()->UpdateWebSocketCount(1);
    }

    nsCOMPtr<nsIPrincipal> loadingPrincipal;
    aRv = webSocketImpl->GetLoadingPrincipal(getter_AddRefs(loadingPrincipal));
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    aRv = webSocketImpl->Init(aGlobal.Context(), loadingPrincipal, principal,
                              !!aTransportProvider, aUrl, protocolArray, ""_ns,
                              0, 0);

    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    nsCOMPtr<Document> doc = webSocket->GetDocumentIfCurrent();

    // the constructor should throw a SYNTAX_ERROR only if it fails to parse the
    // url parameter, so don't throw if InitializeConnection fails, and call
    // onerror/onclose asynchronously
    connectionFailed = NS_FAILED(webSocketImpl->InitializeConnection(
        principal, doc ? doc->CookieJarSettings() : nullptr));
  } else {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    unsigned lineno, column;
    JS::AutoFilename file;
    if (!JS::DescribeScriptedCaller(aGlobal.Context(), &file, &lineno,
                                    &column)) {
      NS_WARNING("Failed to get line number and filename in workers.");
    }

    RefPtr<InitRunnable> runnable = new InitRunnable(
        workerPrivate, webSocketImpl, !!aTransportProvider, aUrl, protocolArray,
        nsDependentCString(file.get()), lineno, column);
    runnable->Dispatch(Canceling, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    aRv = runnable->ErrorCode();
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    if (NS_WARN_IF(!webSocketImpl->RegisterWorkerRef(workerPrivate))) {
      // The worker is shutting down.
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    RefPtr<ConnectRunnable> connectRunnable =
        new ConnectRunnable(workerPrivate, webSocketImpl);
    connectRunnable->Dispatch(Canceling, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    connectionFailed = connectRunnable->ConnectionFailed();
  }

  // It can be that we have been already disconnected because the WebSocket is
  // gone away while we where initializing the webSocket.
  if (!webSocket->mImpl) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // We don't return an error if the connection just failed. Instead we dispatch
  // an event.
  if (connectionFailed) {
    webSocket->mImpl->FailConnection(nsIWebSocketChannel::CLOSE_ABNORMAL);
  }

  // If we don't have a channel, the connection is failed and onerror() will be
  // called asynchrounsly.
  if (!webSocket->mImpl->mChannel) {
    return webSocket.forget();
  }

  class MOZ_STACK_CLASS ClearWebSocket {
   public:
    explicit ClearWebSocket(WebSocketImpl* aWebSocketImpl)
        : mWebSocketImpl(aWebSocketImpl), mDone(false) {}

    void Done() { mDone = true; }

    ~ClearWebSocket() {
      if (!mDone) {
        mWebSocketImpl->mChannel = nullptr;
        mWebSocketImpl->FailConnection(nsIWebSocketChannel::CLOSE_ABNORMAL);
      }
    }

    WebSocketImpl* mWebSocketImpl;
    bool mDone;
  };

  ClearWebSocket cws(webSocket->mImpl);

  // This operation must be done on the correct thread. The rest must run on the
  // main-thread.
  aRv = webSocket->mImpl->mChannel->SetNotificationCallbacks(webSocket->mImpl);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (NS_IsMainThread()) {
    MOZ_ASSERT(principal);
    MOZ_ASSERT(partitionedPrincipal);

    nsCOMPtr<nsPIDOMWindowInner> ownerWindow = do_QueryInterface(global);

    UniquePtr<SerializedStackHolder> stack;
    uint64_t windowID = 0;

    if (ownerWindow) {
      BrowsingContext* browsingContext = ownerWindow->GetBrowsingContext();
      if (browsingContext && browsingContext->WatchedByDevTools()) {
        stack = GetCurrentStackForNetMonitor(aGlobal.Context());
      }

      if (WindowContext* wc = ownerWindow->GetWindowContext()) {
        windowID = wc->InnerWindowId();
      }
    }

    aRv = webSocket->mImpl->AsyncOpen(partitionedPrincipal, windowID,
                                      aTransportProvider, aNegotiatedExtensions,
                                      std::move(stack));
  } else {
    MOZ_ASSERT(!aTransportProvider && aNegotiatedExtensions.IsEmpty(),
               "not yet implemented");

    UniquePtr<SerializedStackHolder> stack;
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    if (workerPrivate->IsWatchedByDevTools()) {
      stack = GetCurrentStackForNetMonitor(aGlobal.Context());
    }

    RefPtr<AsyncOpenRunnable> runnable =
        new AsyncOpenRunnable(webSocket->mImpl, std::move(stack));
    runnable->Dispatch(Canceling, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    aRv = runnable->ErrorCode();
  }

  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // It can be that we have been already disconnected because the WebSocket is
  // gone away while we where initializing the webSocket.
  if (!webSocket->mImpl) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // Let's inform devtools about this new active WebSocket.
  webSocket->mImpl->mService->WebSocketCreated(
      webSocket->mImpl->mChannel->Serial(), webSocket->mImpl->mInnerWindowID,
      webSocket->mURI, webSocket->mImpl->mRequestedProtocolList);
  cws.Done();

  return webSocket.forget();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(WebSocket)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(WebSocket,
                                                  DOMEventTargetHelper)
  if (tmp->mImpl) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mImpl->mChannel)
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(WebSocket, DOMEventTargetHelper)
  if (tmp->mImpl) {
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mImpl->mChannel)
    tmp->mImpl->Disconnect();
    MOZ_ASSERT(!tmp->mImpl);
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

bool WebSocket::IsCertainlyAliveForCC() const { return mKeepingAlive; }

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebSocket)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(WebSocket, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(WebSocket, DOMEventTargetHelper)

void WebSocket::DisconnectFromOwner() {
  // If we haven't called WebSocketImpl::Disconnect yet, update web
  // socket count here.
  if (NS_IsMainThread() && mImpl && !mImpl->mDisconnectingOrDisconnected &&
      GetOwner()) {
    GetOwner()->UpdateWebSocketCount(-1);
  }

  DOMEventTargetHelper::DisconnectFromOwner();

  if (mImpl) {
    mImpl->CloseConnection(nsIWebSocketChannel::CLOSE_GOING_AWAY);
  }

  DontKeepAliveAnyMore();
}

//-----------------------------------------------------------------------------
// WebSocketImpl:: initialization
//-----------------------------------------------------------------------------

nsresult WebSocketImpl::Init(JSContext* aCx, nsIPrincipal* aLoadingPrincipal,
                             nsIPrincipal* aPrincipal, bool aIsServerSide,
                             const nsAString& aURL,
                             nsTArray<nsString>& aProtocolArray,
                             const nsACString& aScriptFile,
                             uint32_t aScriptLine, uint32_t aScriptColumn) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrincipal);

  mService = WebSocketEventService::GetOrCreate();

  // We need to keep the implementation alive in case the init disconnects it
  // because of some error.
  RefPtr<WebSocketImpl> kungfuDeathGrip = this;

  // Attempt to kill "ghost" websocket: but usually too early for check to fail
  nsresult rv = mWebSocket->CheckCurrentGlobalCorrectness();
  NS_ENSURE_SUCCESS(rv, rv);

  // Shut down websocket if window is frozen or destroyed (only needed for
  // "ghost" websockets--see bug 696085)
  RefPtr<WebSocketImplProxy> proxy;
  if (mIsMainThread) {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (NS_WARN_IF(!os)) {
      return NS_ERROR_FAILURE;
    }

    proxy = new WebSocketImplProxy(this);
    rv = os->AddObserver(proxy, DOM_WINDOW_DESTROYED_TOPIC, true);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = os->AddObserver(proxy, DOM_WINDOW_FROZEN_TOPIC, true);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!mIsMainThread) {
    mScriptFile = aScriptFile;
    mScriptLine = aScriptLine;
    mScriptColumn = aScriptColumn;
  } else {
    MOZ_ASSERT(aCx);

    unsigned lineno, column;
    JS::AutoFilename file;
    if (JS::DescribeScriptedCaller(aCx, &file, &lineno, &column)) {
      mScriptFile = file.get();
      mScriptLine = lineno;
      mScriptColumn = column;
    }
  }

  mIsServerSide = aIsServerSide;

  // If we don't have aCx, we are window-less, so we don't have a
  // inner-windowID. This can happen in sharedWorkers and ServiceWorkers or in
  // DedicateWorkers created by JSM.
  if (aCx) {
    if (nsPIDOMWindowInner* ownerWindow = mWebSocket->GetOwner()) {
      mInnerWindowID = ownerWindow->WindowID();
    }
  }

  mPrivateBrowsing = !!aPrincipal->OriginAttributesRef().mPrivateBrowsingId;
  mIsChromeContext = aPrincipal->IsSystemPrincipal();

  // parses the url
  rv = ParseURL(aURL);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<Document> originDoc = mWebSocket->GetDocumentIfCurrent();
  if (!originDoc) {
    rv = mWebSocket->CheckCurrentGlobalCorrectness();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  mOriginDocument = do_GetWeakReference(originDoc);

  if (!mIsServerSide) {
    nsCOMPtr<nsIURI> uri;
    {
      nsresult rv = NS_NewURI(getter_AddRefs(uri), mURI);

      // We crash here because we are sure that mURI is a valid URI, so either
      // we are OOM'ing or something else bad is happening.
      if (NS_WARN_IF(NS_FAILED(rv))) {
        MOZ_CRASH();
      }
    }

    // The 'real' nsHttpChannel of the websocket gets opened in the parent.
    // Since we don't serialize the CSP within child and parent and also not
    // the context, we have to perform content policy checks here instead of
    // AsyncOpen().
    // Please note that websockets can't follow redirects, hence there is no
    // need to perform a CSP check after redirects.
    nsCOMPtr<nsILoadInfo> secCheckLoadInfo = new net::LoadInfo(
        aPrincipal,  // loading principal
        aPrincipal,  // triggering principal
        originDoc, nsILoadInfo::SEC_ONLY_FOR_EXPLICIT_CONTENTSEC_CHECK,
        nsIContentPolicy::TYPE_WEBSOCKET);

    int16_t shouldLoad = nsIContentPolicy::ACCEPT;
    rv = NS_CheckContentLoadPolicy(uri, secCheckLoadInfo, ""_ns, &shouldLoad,
                                   nsContentUtils::GetContentPolicy());
    NS_ENSURE_SUCCESS(rv, rv);

    if (NS_CP_REJECTED(shouldLoad)) {
      // Disallowed by content policy
      return NS_ERROR_CONTENT_BLOCKED;
    }

    // If the HTTPS-Only mode is enabled, we need to upgrade the websocket
    // connection from ws:// to wss:// and mark it as secure.
    if (!mSecure && originDoc &&
        !nsMixedContentBlocker::IsPotentiallyTrustworthyLoopbackURL(
            originDoc->GetDocumentURI())) {
      nsCOMPtr<nsIURI> uri;
      nsresult rv = NS_NewURI(getter_AddRefs(uri), mURI);
      NS_ENSURE_SUCCESS(rv, rv);

      // secCheckLoadInfo is only used for the triggering principal, so this
      // is okay.
      if (nsHTTPSOnlyUtils::ShouldUpgradeWebSocket(uri, secCheckLoadInfo)) {
        mURI.ReplaceSubstring("ws://", "wss://");
        if (NS_WARN_IF(mURI.Find("wss://") != 0)) {
          return NS_OK;
        }
        mSecure = true;
      }
    }
  }

  // Potentially the page uses the CSP directive 'upgrade-insecure-requests'.
  // In such a case we have to upgrade ws: to wss: and also update mSecure
  // to reflect that upgrade. Please note that we can not upgrade from ws:
  // to wss: before performing content policy checks because CSP needs to
  // send reports in case the scheme is about to be upgraded.
  if (!mIsServerSide && !mSecure && originDoc &&
      originDoc->GetUpgradeInsecureRequests(false) &&
      !nsMixedContentBlocker::IsPotentiallyTrustworthyLoopbackURL(
          originDoc->GetDocumentURI())) {
    // let's use the old specification before the upgrade for logging
    AutoTArray<nsString, 2> params;
    CopyUTF8toUTF16(mURI, *params.AppendElement());

    // upgrade the request from ws:// to wss:// and mark as secure
    mURI.ReplaceSubstring("ws://", "wss://");
    if (NS_WARN_IF(mURI.Find("wss://") != 0)) {
      return NS_OK;
    }
    mSecure = true;

    params.AppendElement(u"wss"_ns);
    CSP_LogLocalizedStr("upgradeInsecureRequest", params,
                        u""_ns,  // aSourceFile
                        u""_ns,  // aScriptSample
                        0,       // aLineNumber
                        0,       // aColumnNumber
                        nsIScriptError::warningFlag,
                        "upgradeInsecureRequest"_ns, mInnerWindowID,
                        mPrivateBrowsing);
  }

  // Don't allow https:// to open ws://
  if (!mIsServerSide && !mSecure &&
      !Preferences::GetBool("network.websocket.allowInsecureFromHTTPS",
                            false) &&
      !nsMixedContentBlocker::IsPotentiallyTrustworthyLoopbackHost(
          mAsciiHost)) {
    if (aLoadingPrincipal->SchemeIs("https")) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }
  }

  // Assign the sub protocol list and scan it for illegal values
  for (uint32_t index = 0; index < aProtocolArray.Length(); ++index) {
    for (uint32_t i = 0; i < aProtocolArray[index].Length(); ++i) {
      if (aProtocolArray[index][i] < static_cast<char16_t>(0x0021) ||
          aProtocolArray[index][i] > static_cast<char16_t>(0x007E)) {
        return NS_ERROR_DOM_SYNTAX_ERR;
      }
    }

    if (!mRequestedProtocolList.IsEmpty()) {
      mRequestedProtocolList.AppendLiteral(", ");
    }

    AppendUTF16toUTF8(aProtocolArray[index], mRequestedProtocolList);
  }

  if (mIsMainThread) {
    mImplProxy = std::move(proxy);
  }
  return NS_OK;
}

nsresult WebSocketImpl::AsyncOpen(
    nsIPrincipal* aPrincipal, uint64_t aInnerWindowID,
    nsITransportProvider* aTransportProvider,
    const nsACString& aNegotiatedExtensions,
    UniquePtr<SerializedStackHolder> aOriginStack) {
  MOZ_ASSERT(NS_IsMainThread(), "Not running on main thread");
  MOZ_ASSERT_IF(!aTransportProvider, aNegotiatedExtensions.IsEmpty());

  nsCString asciiOrigin;
  nsresult rv = aPrincipal->GetAsciiOrigin(asciiOrigin);
  if (NS_FAILED(rv)) {
    asciiOrigin.AssignLiteral("null");
  }

  if (aTransportProvider) {
    rv = mChannel->SetServerParameters(aTransportProvider,
                                       aNegotiatedExtensions);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  ToLowerCase(asciiOrigin);

  nsCOMPtr<nsIURI> uri;
  if (!aTransportProvider) {
    rv = NS_NewURI(getter_AddRefs(uri), mURI);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  rv = mChannel->AsyncOpenNative(uri, asciiOrigin,
                                 aPrincipal->OriginAttributesRef(),
                                 aInnerWindowID, this, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_CONTENT_BLOCKED;
  }

  NotifyNetworkMonitorAlternateStack(mChannel, std::move(aOriginStack));

  mInnerWindowID = aInnerWindowID;

  return NS_OK;
}

//-----------------------------------------------------------------------------
// WebSocketImpl methods:
//-----------------------------------------------------------------------------

class nsAutoCloseWS final {
 public:
  explicit nsAutoCloseWS(WebSocketImpl* aWebSocketImpl)
      : mWebSocketImpl(aWebSocketImpl) {}

  ~nsAutoCloseWS() {
    if (!mWebSocketImpl->mChannel) {
      mWebSocketImpl->CloseConnection(
          nsIWebSocketChannel::CLOSE_INTERNAL_ERROR);
    }
  }

 private:
  RefPtr<WebSocketImpl> mWebSocketImpl;
};

nsresult WebSocketImpl::InitializeConnection(
    nsIPrincipal* aPrincipal, nsICookieJarSettings* aCookieJarSettings) {
  AssertIsOnMainThread();
  MOZ_ASSERT(!mChannel, "mChannel should be null");

  nsCOMPtr<nsIWebSocketChannel> wsChannel;
  nsAutoCloseWS autoClose(this);
  nsresult rv;

  if (mSecure) {
    wsChannel =
        do_CreateInstance("@mozilla.org/network/protocol;1?name=wss", &rv);
  } else {
    wsChannel =
        do_CreateInstance("@mozilla.org/network/protocol;1?name=ws", &rv);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // add ourselves to the document's load group and
  // provide the http stack the loadgroup info too
  nsCOMPtr<nsILoadGroup> loadGroup;
  rv = GetLoadGroup(getter_AddRefs(loadGroup));
  if (loadGroup) {
    rv = wsChannel->SetLoadGroup(loadGroup);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = loadGroup->AddRequest(this, nullptr);
    NS_ENSURE_SUCCESS(rv, rv);

    mWeakLoadGroup = do_GetWeakReference(loadGroup);
  }

  // manually adding loadinfo to the channel since it
  // was not set during channel creation.
  nsCOMPtr<Document> doc = do_QueryReferent(mOriginDocument);

  // mOriginDocument has to be release on main-thread because WeakReferences
  // are not thread-safe.
  mOriginDocument = nullptr;

  // The TriggeringPrincipal for websockets must always be a script.
  // Let's make sure that the doc's principal (if a doc exists)
  // and aPrincipal are same origin.
  MOZ_ASSERT(!doc || doc->NodePrincipal()->Equals(aPrincipal));

  rv = wsChannel->InitLoadInfoNative(
      doc, doc ? doc->NodePrincipal() : aPrincipal, aPrincipal,
      aCookieJarSettings,
      nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
      nsIContentPolicy::TYPE_WEBSOCKET, 0);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  if (!mRequestedProtocolList.IsEmpty()) {
    rv = wsChannel->SetProtocol(mRequestedProtocolList);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIThreadRetargetableRequest> rr = do_QueryInterface(wsChannel);
  NS_ENSURE_TRUE(rr, NS_ERROR_FAILURE);

  rv = rr->RetargetDeliveryTo(this);
  NS_ENSURE_SUCCESS(rv, rv);

  mChannel = wsChannel;

  if (mIsMainThread) {
    MOZ_ASSERT(mImplProxy);
    mService->AssociateWebSocketImplWithSerialID(mImplProxy,
                                                 mChannel->Serial());
  }

  if (mIsMainThread && doc) {
    mMainThreadEventTarget = doc->EventTargetFor(TaskCategory::Other);
  }

  return NS_OK;
}

void WebSocketImpl::DispatchConnectionCloseEvents() {
  AssertIsOnTargetThread();

  if (mDisconnectingOrDisconnected) {
    return;
  }

  mWebSocket->SetReadyState(WebSocket::CLOSED);

  // Let's keep the object alive because the webSocket can be CCed in the
  // onerror or in the onclose callback.
  RefPtr<WebSocket> webSocket = mWebSocket;

  // Call 'onerror' if needed
  if (mFailed) {
    nsresult rv = webSocket->CreateAndDispatchSimpleEvent(ERROR_EVENT_STRING);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to dispatch the error event");
    }
  }

  nsresult rv = webSocket->CreateAndDispatchCloseEvent(
      mCloseEventWasClean, mCloseEventCode, mCloseEventReason);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch the close event");
  }

  webSocket->UpdateMustKeepAlive();
  Disconnect();
}

nsresult WebSocket::CreateAndDispatchSimpleEvent(const nsAString& aName) {
  MOZ_ASSERT(mImpl);
  AssertIsOnTargetThread();

  nsresult rv = CheckCurrentGlobalCorrectness();
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  RefPtr<Event> event = NS_NewDOMEvent(this, nullptr, nullptr);

  // it doesn't bubble, and it isn't cancelable
  event->InitEvent(aName, false, false);
  event->SetTrusted(true);

  ErrorResult err;
  DispatchEvent(*event, err);
  return err.StealNSResult();
}

nsresult WebSocket::CreateAndDispatchMessageEvent(const nsACString& aData,
                                                  bool aIsBinary) {
  MOZ_ASSERT(mImpl);
  AssertIsOnTargetThread();

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(GetOwnerGlobal()))) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();

  nsresult rv = CheckCurrentGlobalCorrectness();
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  uint16_t messageType = nsIWebSocketEventListener::TYPE_STRING;

  // Create appropriate JS object for message
  JS::Rooted<JS::Value> jsData(cx);
  if (aIsBinary) {
    if (mBinaryType == dom::BinaryType::Blob) {
      messageType = nsIWebSocketEventListener::TYPE_BLOB;

      RefPtr<Blob> blob =
          Blob::CreateStringBlob(GetOwnerGlobal(), aData, u""_ns);
      if (NS_WARN_IF(!blob)) {
        return NS_ERROR_FAILURE;
      }

      if (!ToJSValue(cx, blob, &jsData)) {
        return NS_ERROR_FAILURE;
      }

    } else if (mBinaryType == dom::BinaryType::Arraybuffer) {
      messageType = nsIWebSocketEventListener::TYPE_ARRAYBUFFER;

      JS::Rooted<JSObject*> arrayBuf(cx);
      nsresult rv =
          nsContentUtils::CreateArrayBuffer(cx, aData, arrayBuf.address());
      NS_ENSURE_SUCCESS(rv, rv);
      jsData.setObject(*arrayBuf);
    } else {
      MOZ_CRASH("Unknown binary type!");
      return NS_ERROR_UNEXPECTED;
    }
  } else {
    // JS string
    nsAutoString utf16Data;
    if (!AppendUTF8toUTF16(aData, utf16Data, mozilla::fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    JSString* jsString;
    jsString = JS_NewUCStringCopyN(cx, utf16Data.get(), utf16Data.Length());
    NS_ENSURE_TRUE(jsString, NS_ERROR_FAILURE);

    jsData.setString(jsString);
  }

  mImpl->mService->WebSocketMessageAvailable(
      mImpl->mChannel->Serial(), mImpl->mInnerWindowID, aData, messageType);

  // create an event that uses the MessageEvent interface,
  // which does not bubble, is not cancelable, and has no default action

  RefPtr<MessageEvent> event = new MessageEvent(this, nullptr, nullptr);

  event->InitMessageEvent(nullptr, MESSAGE_EVENT_STRING, CanBubble::eNo,
                          Cancelable::eNo, jsData, mImpl->mUTF16Origin, u""_ns,
                          nullptr, Sequence<OwningNonNull<MessagePort>>());
  event->SetTrusted(true);

  ErrorResult err;
  DispatchEvent(*event, err);
  return err.StealNSResult();
}

nsresult WebSocket::CreateAndDispatchCloseEvent(bool aWasClean, uint16_t aCode,
                                                const nsAString& aReason) {
  AssertIsOnTargetThread();

  // This method is called by a runnable and it can happen that, in the
  // meantime, GC unlinked this object, so mImpl could be null.
  if (mImpl && mImpl->mChannel) {
    mImpl->mService->WebSocketClosed(mImpl->mChannel->Serial(),
                                     mImpl->mInnerWindowID, aWasClean, aCode,
                                     aReason);
  }

  nsresult rv = CheckCurrentGlobalCorrectness();
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  CloseEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mWasClean = aWasClean;
  init.mCode = aCode;
  init.mReason = aReason;

  RefPtr<CloseEvent> event =
      CloseEvent::Constructor(this, CLOSE_EVENT_STRING, init);
  event->SetTrusted(true);

  ErrorResult err;
  DispatchEvent(*event, err);
  return err.StealNSResult();
}

nsresult WebSocketImpl::ParseURL(const nsAString& aURL) {
  AssertIsOnMainThread();
  NS_ENSURE_TRUE(!aURL.IsEmpty(), NS_ERROR_DOM_SYNTAX_ERR);

  if (mIsServerSide) {
    mWebSocket->mURI = aURL;
    CopyUTF16toUTF8(mWebSocket->mURI, mURI);

    return NS_OK;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURL);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_SYNTAX_ERR);

  nsCOMPtr<nsIURL> parsedURL = do_QueryInterface(uri, &rv);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_SYNTAX_ERR);

  bool hasRef;
  rv = parsedURL->GetHasRef(&hasRef);
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && !hasRef, NS_ERROR_DOM_SYNTAX_ERR);

  nsAutoCString scheme;
  rv = parsedURL->GetScheme(scheme);
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && !scheme.IsEmpty(),
                 NS_ERROR_DOM_SYNTAX_ERR);

  nsAutoCString host;
  rv = parsedURL->GetAsciiHost(host);
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && !host.IsEmpty(), NS_ERROR_DOM_SYNTAX_ERR);

  int32_t port;
  rv = parsedURL->GetPort(&port);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_SYNTAX_ERR);

  nsAutoCString filePath;
  rv = parsedURL->GetFilePath(filePath);
  if (filePath.IsEmpty()) {
    filePath.Assign('/');
  }
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_SYNTAX_ERR);

  nsAutoCString query;
  rv = parsedURL->GetQuery(query);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_SYNTAX_ERR);

  if (scheme.LowerCaseEqualsLiteral("ws")) {
    mSecure = false;
    mPort = (port == -1) ? DEFAULT_WS_SCHEME_PORT : port;
  } else if (scheme.LowerCaseEqualsLiteral("wss")) {
    mSecure = true;
    mPort = (port == -1) ? DEFAULT_WSS_SCHEME_PORT : port;
  } else {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  rv = nsContentUtils::GetUTFOrigin(parsedURL, mUTF16Origin);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_SYNTAX_ERR);

  mAsciiHost = host;
  ToLowerCase(mAsciiHost);

  mResource = filePath;
  if (!query.IsEmpty()) {
    mResource.Append('?');
    mResource.Append(query);
  }
  uint32_t length = mResource.Length();
  uint32_t i;
  for (i = 0; i < length; ++i) {
    if (mResource[i] < static_cast<char16_t>(0x0021) ||
        mResource[i] > static_cast<char16_t>(0x007E)) {
      return NS_ERROR_DOM_SYNTAX_ERR;
    }
  }

  rv = parsedURL->GetSpec(mURI);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  CopyUTF8toUTF16(mURI, mWebSocket->mURI);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// Methods that keep alive the WebSocket object when:
//   1. the object has registered event listeners that can be triggered
//      ("strong event listeners");
//   2. there are outgoing not sent messages.
//-----------------------------------------------------------------------------

void WebSocket::UpdateMustKeepAlive() {
  // Here we could not have mImpl.
  MOZ_ASSERT(NS_IsMainThread() == mIsMainThread);

  if (!mCheckMustKeepAlive || !mImpl) {
    return;
  }

  bool shouldKeepAlive = false;
  uint16_t readyState = ReadyState();

  if (mListenerManager) {
    switch (readyState) {
      case CONNECTING: {
        if (mListenerManager->HasListenersFor(OPEN_EVENT_STRING) ||
            mListenerManager->HasListenersFor(MESSAGE_EVENT_STRING) ||
            mListenerManager->HasListenersFor(ERROR_EVENT_STRING) ||
            mListenerManager->HasListenersFor(CLOSE_EVENT_STRING)) {
          shouldKeepAlive = true;
        }
      } break;

      case OPEN:
      case CLOSING: {
        if (mListenerManager->HasListenersFor(MESSAGE_EVENT_STRING) ||
            mListenerManager->HasListenersFor(ERROR_EVENT_STRING) ||
            mListenerManager->HasListenersFor(CLOSE_EVENT_STRING) ||
            mOutgoingBufferedAmount.value() != 0) {
          shouldKeepAlive = true;
        }
      } break;

      case CLOSED: {
        shouldKeepAlive = false;
      }
    }
  }

  if (mKeepingAlive && !shouldKeepAlive) {
    mKeepingAlive = false;
    mImpl->ReleaseObject();
  } else if (!mKeepingAlive && shouldKeepAlive) {
    mKeepingAlive = true;
    mImpl->AddRefObject();
  }
}

void WebSocket::DontKeepAliveAnyMore() {
  // Here we could not have mImpl.
  MOZ_ASSERT(NS_IsMainThread() == mIsMainThread);

  if (mKeepingAlive) {
    MOZ_ASSERT(mImpl);

    mKeepingAlive = false;
    mImpl->ReleaseObject();
  }

  mCheckMustKeepAlive = false;
}

void WebSocketImpl::AddRefObject() {
  AssertIsOnTargetThread();
  AddRef();
}

void WebSocketImpl::ReleaseObject() {
  AssertIsOnTargetThread();
  Release();
}

bool WebSocketImpl::RegisterWorkerRef(WorkerPrivate* aWorkerPrivate) {
  MOZ_ASSERT(aWorkerPrivate);

  RefPtr<WebSocketImpl> self = this;

  // In workers we have to keep the worker alive using a strong reference in
  // order to dispatch messages correctly.
  RefPtr<StrongWorkerRef> workerRef =
      StrongWorkerRef::Create(aWorkerPrivate, "WebSocketImpl", [self]() {
        {
          MutexAutoLock lock(self->mMutex);
          self->mWorkerShuttingDown = true;
        }

        self->CloseConnection(nsIWebSocketChannel::CLOSE_GOING_AWAY, ""_ns);
      });
  if (NS_WARN_IF(!workerRef)) {
    return false;
  }

  mWorkerRef = new ThreadSafeWorkerRef(workerRef);
  MOZ_ASSERT(mWorkerRef);

  return true;
}

void WebSocketImpl::UnregisterWorkerRef() {
  MOZ_ASSERT(mDisconnectingOrDisconnected);
  MOZ_ASSERT(mWorkerRef);
  mWorkerRef->Private()->AssertIsOnWorkerThread();

  {
    MutexAutoLock lock(mMutex);
    mWorkerShuttingDown = true;
  }

  // The DTOR of this StrongWorkerRef will release the worker for us.
  mWorkerRef = nullptr;
}

nsresult WebSocketImpl::UpdateURI() {
  AssertIsOnTargetThread();

  // Check for Redirections
  RefPtr<BaseWebSocketChannel> channel;
  channel = static_cast<BaseWebSocketChannel*>(mChannel.get());
  MOZ_ASSERT(channel);

  channel->GetEffectiveURL(mWebSocket->mEffectiveURL);
  mSecure = channel->IsEncrypted();

  return NS_OK;
}

void WebSocket::EventListenerAdded(nsAtom* aType) {
  AssertIsOnTargetThread();
  UpdateMustKeepAlive();
}

void WebSocket::EventListenerRemoved(nsAtom* aType) {
  AssertIsOnTargetThread();
  UpdateMustKeepAlive();
}

//-----------------------------------------------------------------------------
// WebSocket - methods
//-----------------------------------------------------------------------------

// webIDL: readonly attribute unsigned short readyState;
uint16_t WebSocket::ReadyState() {
  MutexAutoLock lock(mMutex);
  return mReadyState;
}

void WebSocket::SetReadyState(uint16_t aReadyState) {
  MutexAutoLock lock(mMutex);
  mReadyState = aReadyState;
}

// webIDL: readonly attribute unsigned long long bufferedAmount;
uint64_t WebSocket::BufferedAmount() const {
  AssertIsOnTargetThread();
  MOZ_RELEASE_ASSERT(mOutgoingBufferedAmount.isValid());
  return mOutgoingBufferedAmount.value();
}

// webIDL: attribute BinaryType binaryType;
dom::BinaryType WebSocket::BinaryType() const {
  AssertIsOnTargetThread();
  return mBinaryType;
}

// webIDL: attribute BinaryType binaryType;
void WebSocket::SetBinaryType(dom::BinaryType aData) {
  AssertIsOnTargetThread();
  mBinaryType = aData;
}

// webIDL: readonly attribute DOMString url
void WebSocket::GetUrl(nsAString& aURL) {
  AssertIsOnTargetThread();

  if (mEffectiveURL.IsEmpty()) {
    aURL = mURI;
  } else {
    aURL = mEffectiveURL;
  }
}

// webIDL: readonly attribute DOMString extensions;
void WebSocket::GetExtensions(nsAString& aExtensions) {
  AssertIsOnTargetThread();
  CopyUTF8toUTF16(mEstablishedExtensions, aExtensions);
}

// webIDL: readonly attribute DOMString protocol;
void WebSocket::GetProtocol(nsAString& aProtocol) {
  AssertIsOnTargetThread();
  CopyUTF8toUTF16(mEstablishedProtocol, aProtocol);
}

// webIDL: void send(DOMString data);
void WebSocket::Send(const nsAString& aData, ErrorResult& aRv) {
  AssertIsOnTargetThread();

  nsAutoCString msgString;
  if (!AppendUTF16toUTF8(aData, msgString, mozilla::fallible_t())) {
    aRv.Throw(NS_ERROR_FILE_TOO_BIG);
    return;
  }
  Send(nullptr, msgString, msgString.Length(), false, aRv);
}

void WebSocket::Send(Blob& aData, ErrorResult& aRv) {
  AssertIsOnTargetThread();

  nsCOMPtr<nsIInputStream> msgStream;
  aData.CreateInputStream(getter_AddRefs(msgStream), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  uint64_t msgLength = aData.GetSize(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (msgLength > UINT32_MAX) {
    aRv.Throw(NS_ERROR_FILE_TOO_BIG);
    return;
  }

  Send(msgStream, ""_ns, msgLength, true, aRv);
}

void WebSocket::Send(const ArrayBuffer& aData, ErrorResult& aRv) {
  AssertIsOnTargetThread();

  aData.ComputeState();

  static_assert(sizeof(*aData.Data()) == 1, "byte-sized data required");

  uint32_t len = aData.Length();
  char* data = reinterpret_cast<char*>(aData.Data());

  nsDependentCSubstring msgString;
  if (!msgString.Assign(data, len, mozilla::fallible_t())) {
    aRv.Throw(NS_ERROR_FILE_TOO_BIG);
    return;
  }
  Send(nullptr, msgString, len, true, aRv);
}

void WebSocket::Send(const ArrayBufferView& aData, ErrorResult& aRv) {
  AssertIsOnTargetThread();

  aData.ComputeState();

  static_assert(sizeof(*aData.Data()) == 1, "byte-sized data required");

  uint32_t len = aData.Length();
  char* data = reinterpret_cast<char*>(aData.Data());

  nsDependentCSubstring msgString;
  if (!msgString.Assign(data, len, mozilla::fallible_t())) {
    aRv.Throw(NS_ERROR_FILE_TOO_BIG);
    return;
  }
  Send(nullptr, msgString, len, true, aRv);
}

void WebSocket::Send(nsIInputStream* aMsgStream, const nsACString& aMsgString,
                     uint32_t aMsgLength, bool aIsBinary, ErrorResult& aRv) {
  AssertIsOnTargetThread();

  int64_t readyState = ReadyState();
  if (readyState == CONNECTING) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  CheckedUint64 outgoingBufferedAmount = mOutgoingBufferedAmount;
  outgoingBufferedAmount += aMsgLength;
  if (!outgoingBufferedAmount.isValid()) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  // Always increment outgoing buffer len, even if closed
  mOutgoingBufferedAmount = outgoingBufferedAmount;
  MOZ_RELEASE_ASSERT(mOutgoingBufferedAmount.isValid());

  if (readyState == CLOSING || readyState == CLOSED) {
    return;
  }

  // We must have mImpl when connected.
  MOZ_ASSERT(mImpl);
  MOZ_ASSERT(readyState == OPEN, "Unknown state in WebSocket::Send");

  nsresult rv;
  if (aMsgStream) {
    rv = mImpl->mChannel->SendBinaryStream(aMsgStream, aMsgLength);
  } else {
    if (aIsBinary) {
      rv = mImpl->mChannel->SendBinaryMsg(aMsgString);
    } else {
      rv = mImpl->mChannel->SendMsg(aMsgString);
    }
  }

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  UpdateMustKeepAlive();
}

// webIDL: void close(optional unsigned short code, optional DOMString reason):
void WebSocket::Close(const Optional<uint16_t>& aCode,
                      const Optional<nsAString>& aReason, ErrorResult& aRv) {
  AssertIsOnTargetThread();

  // the reason code is optional, but if provided it must be in a specific range
  uint16_t closeCode = 0;
  if (aCode.WasPassed()) {
    if (aCode.Value() != 1000 &&
        (aCode.Value() < 3000 || aCode.Value() > 4999)) {
      aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
      return;
    }
    closeCode = aCode.Value();
  }

  nsCString closeReason;
  if (aReason.WasPassed()) {
    CopyUTF16toUTF8(aReason.Value(), closeReason);

    // The API requires the UTF-8 string to be 123 or less bytes
    if (closeReason.Length() > 123) {
      aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
      return;
    }
  }

  int64_t readyState = ReadyState();
  if (readyState == CLOSING || readyState == CLOSED) {
    return;
  }

  // If we don't have mImpl, we are in a shutting down worker where we are still
  // in CONNECTING state, but already disconnected internally.
  if (!mImpl) {
    MOZ_ASSERT(readyState == CONNECTING);
    SetReadyState(CLOSING);
    return;
  }

  if (readyState == CONNECTING) {
    mImpl->FailConnection(closeCode, closeReason);
    return;
  }

  MOZ_ASSERT(readyState == OPEN);
  mImpl->CloseConnection(closeCode, closeReason);
}

//-----------------------------------------------------------------------------
// WebSocketImpl::nsIObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
WebSocketImpl::Observe(nsISupports* aSubject, const char* aTopic,
                       const char16_t* aData) {
  AssertIsOnMainThread();

  int64_t readyState = mWebSocket->ReadyState();
  if ((readyState == WebSocket::CLOSING) || (readyState == WebSocket::CLOSED)) {
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aSubject);
  if (!mWebSocket->GetOwner() || window != mWebSocket->GetOwner()) {
    return NS_OK;
  }

  if ((strcmp(aTopic, DOM_WINDOW_FROZEN_TOPIC) == 0) ||
      (strcmp(aTopic, DOM_WINDOW_DESTROYED_TOPIC) == 0)) {
    CloseConnection(nsIWebSocketChannel::CLOSE_GOING_AWAY);
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// WebSocketImpl::nsIRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
WebSocketImpl::GetName(nsACString& aName) {
  AssertIsOnMainThread();

  CopyUTF16toUTF8(mWebSocket->mURI, aName);
  return NS_OK;
}

NS_IMETHODIMP
WebSocketImpl::IsPending(bool* aValue) {
  AssertIsOnTargetThread();

  int64_t readyState = mWebSocket->ReadyState();
  *aValue = (readyState != WebSocket::CLOSED);
  return NS_OK;
}

NS_IMETHODIMP
WebSocketImpl::GetStatus(nsresult* aStatus) {
  AssertIsOnTargetThread();

  *aStatus = NS_OK;
  return NS_OK;
}

namespace {

class CancelRunnable final : public MainThreadWorkerRunnable {
 public:
  CancelRunnable(ThreadSafeWorkerRef* aWorkerRef, WebSocketImpl* aImpl)
      : MainThreadWorkerRunnable(aWorkerRef->Private()), mImpl(aImpl) {}

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    aWorkerPrivate->AssertIsOnWorkerThread();
    return !NS_FAILED(mImpl->CancelInternal());
  }

 private:
  RefPtr<WebSocketImpl> mImpl;
};

}  // namespace

// Window closed, stop/reload button pressed, user navigated away from page,
// etc.
NS_IMETHODIMP
WebSocketImpl::Cancel(nsresult aStatus) {
  AssertIsOnMainThread();

  if (!mIsMainThread) {
    MOZ_ASSERT(mWorkerRef);
    RefPtr<CancelRunnable> runnable = new CancelRunnable(mWorkerRef, this);
    if (!runnable->Dispatch()) {
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }

  return CancelInternal();
}

nsresult WebSocketImpl::CancelInternal() {
  AssertIsOnTargetThread();

  // If CancelInternal is called by a runnable, we may already be disconnected
  // by the time it runs.
  if (mDisconnectingOrDisconnected) {
    return NS_OK;
  }

  int64_t readyState = mWebSocket->ReadyState();
  if (readyState == WebSocket::CLOSING || readyState == WebSocket::CLOSED) {
    return NS_OK;
  }

  return CloseConnection(nsIWebSocketChannel::CLOSE_GOING_AWAY);
}

NS_IMETHODIMP
WebSocketImpl::Suspend() {
  AssertIsOnMainThread();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WebSocketImpl::Resume() {
  AssertIsOnMainThread();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WebSocketImpl::GetLoadGroup(nsILoadGroup** aLoadGroup) {
  AssertIsOnMainThread();

  *aLoadGroup = nullptr;

  if (mIsMainThread) {
    nsCOMPtr<Document> doc = mWebSocket->GetDocumentIfCurrent();
    if (doc) {
      *aLoadGroup = doc->GetDocumentLoadGroup().take();
    }

    return NS_OK;
  }

  MOZ_ASSERT(mWorkerRef);

  // Walk up to our containing page
  WorkerPrivate* wp = mWorkerRef->Private();
  while (wp->GetParent()) {
    wp = wp->GetParent();
  }

  nsPIDOMWindowInner* window = wp->GetWindow();
  if (!window) {
    return NS_OK;
  }

  Document* doc = window->GetExtantDoc();
  if (doc) {
    *aLoadGroup = doc->GetDocumentLoadGroup().take();
  }

  return NS_OK;
}

NS_IMETHODIMP
WebSocketImpl::SetLoadGroup(nsILoadGroup* aLoadGroup) {
  AssertIsOnMainThread();
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
WebSocketImpl::GetLoadFlags(nsLoadFlags* aLoadFlags) {
  AssertIsOnMainThread();

  *aLoadFlags = nsIRequest::LOAD_BACKGROUND;
  return NS_OK;
}

NS_IMETHODIMP
WebSocketImpl::SetLoadFlags(nsLoadFlags aLoadFlags) {
  AssertIsOnMainThread();

  // we won't change the load flags at all.
  return NS_OK;
}

NS_IMETHODIMP
WebSocketImpl::GetTRRMode(nsIRequest::TRRMode* aTRRMode) {
  return GetTRRModeImpl(aTRRMode);
}

NS_IMETHODIMP
WebSocketImpl::SetTRRMode(nsIRequest::TRRMode aTRRMode) {
  return SetTRRModeImpl(aTRRMode);
}

namespace {

class WorkerRunnableDispatcher final : public WorkerRunnable {
  RefPtr<WebSocketImpl> mWebSocketImpl;

 public:
  WorkerRunnableDispatcher(WebSocketImpl* aImpl,
                           ThreadSafeWorkerRef* aWorkerRef,
                           already_AddRefed<nsIRunnable> aEvent)
      : WorkerRunnable(aWorkerRef->Private(), WorkerThreadUnchangedBusyCount),
        mWebSocketImpl(aImpl),
        mEvent(std::move(aEvent)) {}

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    aWorkerPrivate->AssertIsOnWorkerThread();

    // No messages when disconnected.
    if (mWebSocketImpl->mDisconnectingOrDisconnected) {
      NS_WARNING("Dispatching a WebSocket event after the disconnection!");
      return true;
    }

    return !NS_FAILED(mEvent->Run());
  }

  void PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aRunResult) override {}

  bool PreDispatch(WorkerPrivate* aWorkerPrivate) override {
    // We don't call WorkerRunnable::PreDispatch because it would assert the
    // wrong thing about which thread we're on.  We're on whichever thread the
    // channel implementation is running on (probably the main thread or socket
    // transport thread).
    return true;
  }

  void PostDispatch(WorkerPrivate* aWorkerPrivate,
                    bool aDispatchResult) override {
    // We don't call WorkerRunnable::PreDispatch because it would assert the
    // wrong thing about which thread we're on.  We're on whichever thread the
    // channel implementation is running on (probably the main thread or socket
    // transport thread).
  }

 private:
  nsCOMPtr<nsIRunnable> mEvent;
};

}  // namespace

NS_IMETHODIMP
WebSocketImpl::DispatchFromScript(nsIRunnable* aEvent, uint32_t aFlags) {
  nsCOMPtr<nsIRunnable> event(aEvent);
  return Dispatch(event.forget(), aFlags);
}

NS_IMETHODIMP
WebSocketImpl::Dispatch(already_AddRefed<nsIRunnable> aEvent, uint32_t aFlags) {
  nsCOMPtr<nsIRunnable> event_ref(aEvent);
  // If the target is the main-thread, we should try to dispatch the runnable
  // to a labeled event target.
  if (mIsMainThread) {
    return mMainThreadEventTarget
               ? mMainThreadEventTarget->Dispatch(event_ref.forget())
               : GetMainThreadEventTarget()->Dispatch(event_ref.forget());
  }

  MutexAutoLock lock(mMutex);
  if (mWorkerShuttingDown) {
    return NS_OK;
  }

  MOZ_DIAGNOSTIC_ASSERT(mWorkerRef);

  // If the target is a worker, we have to use a custom WorkerRunnableDispatcher
  // runnable.
  RefPtr<WorkerRunnableDispatcher> event =
      new WorkerRunnableDispatcher(this, mWorkerRef, event_ref.forget());

  if (!event->Dispatch()) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
WebSocketImpl::DelayedDispatch(already_AddRefed<nsIRunnable>, uint32_t) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WebSocketImpl::IsOnCurrentThread(bool* aResult) {
  *aResult = IsTargetThread();
  return NS_OK;
}

NS_IMETHODIMP_(bool)
WebSocketImpl::IsOnCurrentThreadInfallible() { return IsTargetThread(); }

bool WebSocketImpl::IsTargetThread() const {
  return NS_IsMainThread() == mIsMainThread;
}

void WebSocket::AssertIsOnTargetThread() const {
  MOZ_ASSERT(NS_IsMainThread() == mIsMainThread);
}

nsresult WebSocketImpl::GetLoadingPrincipal(nsIPrincipal** aPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mIsMainThread);

  // Check the principal's uri to determine if we were loaded from https.
  nsCOMPtr<nsIGlobalObject> globalObject(GetEntryGlobal());
  nsCOMPtr<nsIPrincipal> principal;

  if (globalObject) {
    principal = globalObject->PrincipalOrNull();
  }

  nsCOMPtr<nsPIDOMWindowInner> innerWindow = do_QueryInterface(globalObject);
  if (!innerWindow) {
    // If we are in a XPConnect sandbox or in a JS component,
    // innerWindow will be null. There is nothing on top of this to be
    // considered.
    principal.forget(aPrincipal);
    return NS_OK;
  }

  RefPtr<WindowContext> windowContext = innerWindow->GetWindowContext();

  while (true) {
    if (principal && !principal->GetIsNullPrincipal()) {
      break;
    }

    if (NS_WARN_IF(!windowContext)) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }

    if (windowContext->IsTop()) {
      if (!windowContext->GetBrowsingContext()->HadOriginalOpener()) {
        break;
      }
      // We are at the top. Let's see if we have an opener window.
      RefPtr<BrowsingContext> opener =
          windowContext->GetBrowsingContext()->GetOpener();
      if (!opener) {
        break;
      }
      windowContext = opener->GetCurrentWindowContext();
    } else {
      // If we're not a top window get the parent window context instead.
      windowContext = windowContext->GetParentWindowContext();
    }

    if (NS_WARN_IF(!windowContext)) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }

    nsCOMPtr<Document> document = windowContext->GetExtantDoc();
    if (NS_WARN_IF(!document)) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }

    principal = document->NodePrincipal();
  }

  principal.forget(aPrincipal);
  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
