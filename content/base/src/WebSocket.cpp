/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebSocket.h"
#include "mozilla/dom/WebSocketBinding.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/OldDebugAPI.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"
#include "nsIDocument.h"
#include "nsXPCOM.h"
#include "nsIXPConnect.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsEventDispatcher.h"
#include "nsError.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIURL.h"
#include "nsIUnicodeEncoder.h"
#include "nsThreadUtils.h"
#include "nsIDOMMessageEvent.h"
#include "nsIPromptFactory.h"
#include "nsIWindowWatcher.h"
#include "nsIPrompt.h"
#include "nsIStringBundle.h"
#include "nsIConsoleService.h"
#include "nsIDOMCloseEvent.h"
#include "nsICryptoHash.h"
#include "nsJSUtils.h"
#include "nsIScriptError.h"
#include "nsNetUtil.h"
#include "nsILoadGroup.h"
#include "mozilla/Preferences.h"
#include "nsDOMLists.h"
#include "xpcpublic.h"
#include "nsContentPolicyUtils.h"
#include "nsDOMFile.h"
#include "nsWrapperCacheInlines.h"
#include "nsDOMEventTargetHelper.h"
#include "nsIObserverService.h"
#include "nsIWebSocketChannel.h"
#include "GeneratedEvents.h"

namespace mozilla {
namespace dom {

#define UTF_8_REPLACEMENT_CHAR    static_cast<char16_t>(0xFFFD)

class CallDispatchConnectionCloseEvents: public nsRunnable
{
public:
CallDispatchConnectionCloseEvents(WebSocket* aWebSocket)
  : mWebSocket(aWebSocket)
  {}

  NS_IMETHOD Run()
  {
    mWebSocket->DispatchConnectionCloseEvents();
    return NS_OK;
  }

private:
  nsRefPtr<WebSocket> mWebSocket;
};

//-----------------------------------------------------------------------------
// WebSocket
//-----------------------------------------------------------------------------

nsresult
WebSocket::PrintErrorOnConsole(const char *aBundleURI,
                               const char16_t *aError,
                               const char16_t **aFormatStrings,
                               uint32_t aFormatStringsLen)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  nsresult rv;
  nsCOMPtr<nsIStringBundleService> bundleService =
    do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStringBundle> strBundle;
  rv = bundleService->CreateBundle(aBundleURI, getter_AddRefs(strBundle));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIConsoleService> console(
    do_GetService(NS_CONSOLESERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIScriptError> errorObject(
    do_CreateInstance(NS_SCRIPTERROR_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  // Localize the error message
  nsXPIDLString message;
  if (aFormatStrings) {
    rv = strBundle->FormatStringFromName(aError, aFormatStrings,
                                         aFormatStringsLen,
                                         getter_Copies(message));
  } else {
    rv = strBundle->GetStringFromName(aError, getter_Copies(message));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  rv = errorObject->InitWithWindowID(message,
                                     NS_ConvertUTF8toUTF16(mScriptFile),
                                     EmptyString(), mScriptLine, 0,
                                     nsIScriptError::errorFlag, "Web Socket",
                                     mInnerWindowID);
  NS_ENSURE_SUCCESS(rv, rv);

  // print the error message directly to the JS console
  rv = console->LogMessage(errorObject);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
WebSocket::CloseConnection(uint16_t aReasonCode,
                             const nsACString& aReasonString)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  if (mReadyState == WebSocket::CLOSING ||
      mReadyState == WebSocket::CLOSED) {
    return NS_OK;
  }

  // The common case...
  if (mChannel) {
    mReadyState = WebSocket::CLOSING;
    return mChannel->Close(aReasonCode, aReasonString);
  }

  // No channel, but not disconnected: canceled or failed early
  //
  MOZ_ASSERT(mReadyState == WebSocket::CONNECTING,
             "Should only get here for early websocket cancel/error");

  // Server won't be sending us a close code, so use what's passed in here.
  mCloseEventCode = aReasonCode;
  CopyUTF8toUTF16(aReasonString, mCloseEventReason);

  mReadyState = WebSocket::CLOSING;

  // Can be called from Cancel() or Init() codepaths, so need to dispatch
  // onerror/onclose asynchronously
  ScheduleConnectionCloseEvents(
                    nullptr,
                    (aReasonCode == nsIWebSocketChannel::CLOSE_NORMAL ||
                     aReasonCode == nsIWebSocketChannel::CLOSE_GOING_AWAY) ?
                     NS_OK : NS_ERROR_FAILURE,
                    false);

  return NS_OK;
}

nsresult
WebSocket::ConsoleError()
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  nsAutoCString targetSpec;
  nsresult rv = mURI->GetSpec(targetSpec);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to get targetSpec");
  } else {
    NS_ConvertUTF8toUTF16 specUTF16(targetSpec);
    const char16_t* formatStrings[] = { specUTF16.get() };

    if (mReadyState < WebSocket::OPEN) {
      PrintErrorOnConsole("chrome://global/locale/appstrings.properties",
                          MOZ_UTF16("connectionFailure"),
                          formatStrings, ArrayLength(formatStrings));
    } else {
      PrintErrorOnConsole("chrome://global/locale/appstrings.properties",
                          MOZ_UTF16("netInterrupt"),
                          formatStrings, ArrayLength(formatStrings));
    }
  }
  /// todo some specific errors - like for message too large
  return rv;
}


void
WebSocket::FailConnection(uint16_t aReasonCode,
                          const nsACString& aReasonString)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  ConsoleError();
  mFailed = true;
  CloseConnection(aReasonCode, aReasonString);
}

nsresult
WebSocket::Disconnect()
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  if (mDisconnected)
    return NS_OK;

  nsCOMPtr<nsILoadGroup> loadGroup;
  GetLoadGroup(getter_AddRefs(loadGroup));
  if (loadGroup)
    loadGroup->RemoveRequest(this, nullptr, NS_OK);

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    os->RemoveObserver(this, DOM_WINDOW_DESTROYED_TOPIC);
    os->RemoveObserver(this, DOM_WINDOW_FROZEN_TOPIC);
  }

  // DontKeepAliveAnyMore() can release the object. So hold a reference to this
  // until the end of the method.
  nsRefPtr<WebSocket> kungfuDeathGrip = this;

  DontKeepAliveAnyMore();
  mChannel = nullptr;
  mDisconnected = true;

  return NS_OK;
}

//-----------------------------------------------------------------------------
// WebSocket::nsIWebSocketListener methods:
//-----------------------------------------------------------------------------

nsresult
WebSocket::DoOnMessageAvailable(const nsACString& aMsg, bool isBinary)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  if (mReadyState == WebSocket::CLOSED) {
    NS_ERROR("Received message after CLOSED");
    return NS_ERROR_UNEXPECTED;
  }

  if (mReadyState == WebSocket::OPEN) {
    // Dispatch New Message
    nsresult rv = CreateAndDispatchMessageEvent(aMsg, isBinary);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to dispatch the message event");
    }
  } else {
    // CLOSING should be the only other state where it's possible to get msgs
    // from channel: Spec says to drop them.
    MOZ_ASSERT(mReadyState == WebSocket::CLOSING,
               "Received message while CONNECTING or CLOSED");
  }

  return NS_OK;
}

NS_IMETHODIMP
WebSocket::OnMessageAvailable(nsISupports* aContext, const nsACString& aMsg)
{
  return DoOnMessageAvailable(aMsg, false);
}

NS_IMETHODIMP
WebSocket::OnBinaryMessageAvailable(nsISupports* aContext,
                                    const nsACString& aMsg)
{
  return DoOnMessageAvailable(aMsg, true);
}

NS_IMETHODIMP
WebSocket::OnStart(nsISupports* aContext)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  // This is the only function that sets OPEN, and should be called only once
  MOZ_ASSERT(mReadyState != WebSocket::OPEN,
             "readyState already OPEN! OnStart called twice?");

  // Nothing to do if we've already closed/closing
  if (mReadyState != WebSocket::CONNECTING) {
    return NS_OK;
  }

  // Attempt to kill "ghost" websocket: but usually too early for check to fail
  nsresult rv = CheckInnerWindowCorrectness();
  if (NS_FAILED(rv)) {
    CloseConnection(nsIWebSocketChannel::CLOSE_GOING_AWAY);
    return rv;
  }

  if (!mRequestedProtocolList.IsEmpty()) {
    mChannel->GetProtocol(mEstablishedProtocol);
  }

  mChannel->GetExtensions(mEstablishedExtensions);
  UpdateURI();

  mReadyState = WebSocket::OPEN;

  // Call 'onopen'
  rv = CreateAndDispatchSimpleEvent(NS_LITERAL_STRING("open"));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch the open event");
  }

  UpdateMustKeepAlive();

  return NS_OK;
}

NS_IMETHODIMP
WebSocket::OnStop(nsISupports* aContext, nsresult aStatusCode)
{
  // We can be CONNECTING here if connection failed.
  // We can be OPEN if we have encountered a fatal protocol error
  // We can be CLOSING if close() was called and/or server initiated close.
  MOZ_ASSERT(mReadyState != WebSocket::CLOSED,
             "Shouldn't already be CLOSED when OnStop called");

  // called by network stack, not JS, so can dispatch JS events synchronously
  return ScheduleConnectionCloseEvents(aContext, aStatusCode, true);
}

nsresult
WebSocket::ScheduleConnectionCloseEvents(nsISupports* aContext,
                                         nsresult aStatusCode,
                                         bool sync)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  // no-op if some other code has already initiated close event
  if (!mOnCloseScheduled) {
    mCloseEventWasClean = NS_SUCCEEDED(aStatusCode);

    if (aStatusCode == NS_BASE_STREAM_CLOSED) {
      // don't generate an error event just because of an unclean close
      aStatusCode = NS_OK;
    }

    if (NS_FAILED(aStatusCode)) {
      ConsoleError();
      mFailed = true;
    }

    mOnCloseScheduled = true;

    if (sync) {
      DispatchConnectionCloseEvents();
    } else {
      NS_DispatchToMainThread(new CallDispatchConnectionCloseEvents(this),
                              NS_DISPATCH_NORMAL);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
WebSocket::OnAcknowledge(nsISupports *aContext, uint32_t aSize)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  if (aSize > mOutgoingBufferedAmount)
    return NS_ERROR_UNEXPECTED;

  mOutgoingBufferedAmount -= aSize;
  return NS_OK;
}

NS_IMETHODIMP
WebSocket::OnServerClose(nsISupports *aContext, uint16_t aCode,
                           const nsACString &aReason)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  MOZ_ASSERT(mReadyState != WebSocket::CONNECTING,
             "Received server close before connected?");
  MOZ_ASSERT(mReadyState != WebSocket::CLOSED,
             "Received server close after already closed!");

  // store code/string for onclose DOM event
  mCloseEventCode = aCode;
  CopyUTF8toUTF16(aReason, mCloseEventReason);

  if (mReadyState == WebSocket::OPEN) {
    // Server initiating close.
    // RFC 6455, 5.5.1: "When sending a Close frame in response, the endpoint
    // typically echos the status code it received".
    // But never send certain codes, per section 7.4.1
    if (aCode == 1005 || aCode == 1006 || aCode == 1015) {
      CloseConnection(0, EmptyCString());
    } else {
      CloseConnection(aCode, aReason);
    }
  } else {
    // We initiated close, and server has replied: OnStop does rest of the work.
    MOZ_ASSERT(mReadyState == WebSocket::CLOSING, "unknown state");
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// WebSocket::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP
WebSocket::GetInterface(const nsIID& aIID, void** aResult)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  if (mReadyState == WebSocket::CLOSED)
    return NS_ERROR_FAILURE;

  if (aIID.Equals(NS_GET_IID(nsIAuthPrompt)) ||
      aIID.Equals(NS_GET_IID(nsIAuthPrompt2))) {
    nsresult rv;
    nsIScriptContext* sc = GetContextForEventHandlers(&rv);
    nsCOMPtr<nsIDocument> doc =
      nsContentUtils::GetDocumentFromScriptContext(sc);
    if (!doc)
      return NS_ERROR_NOT_AVAILABLE;

    nsCOMPtr<nsIPromptFactory> wwatch =
      do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsPIDOMWindow> outerWindow = doc->GetWindow();
    return wwatch->GetPrompt(outerWindow, aIID, aResult);
  }

  return QueryInterface(aIID, aResult);
}

////////////////////////////////////////////////////////////////////////////////
// WebSocket
////////////////////////////////////////////////////////////////////////////////

WebSocket::WebSocket(nsPIDOMWindow* aOwnerWindow)
: nsDOMEventTargetHelper(aOwnerWindow),
  mKeepingAlive(false),
  mCheckMustKeepAlive(true),
  mOnCloseScheduled(false),
  mFailed(false),
  mDisconnected(false),
  mCloseEventWasClean(false),
  mCloseEventCode(nsIWebSocketChannel::CLOSE_ABNORMAL),
  mReadyState(WebSocket::CONNECTING),
  mOutgoingBufferedAmount(0),
  mBinaryType(dom::BinaryType::Blob),
  mScriptLine(0),
  mInnerWindowID(0)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  MOZ_ASSERT(aOwnerWindow);
  MOZ_ASSERT(aOwnerWindow->IsInnerWindow());
}

WebSocket::~WebSocket()
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  // If we threw during Init we never called disconnect
  if (!mDisconnected) {
    Disconnect();
  }
}

JSObject*
WebSocket::WrapObject(JSContext* cx, JS::Handle<JSObject*> scope)
{
  return WebSocketBinding::Wrap(cx, scope, this);
}

//---------------------------------------------------------------------------
// WebIDL
//---------------------------------------------------------------------------

// Constructor:
already_AddRefed<WebSocket>
WebSocket::Constructor(const GlobalObject& aGlobal,
                       const nsAString& aUrl,
                       ErrorResult& aRv)
{
  Sequence<nsString> protocols;
  return WebSocket::Constructor(aGlobal, aUrl, protocols, aRv);
}

already_AddRefed<WebSocket>
WebSocket::Constructor(const GlobalObject& aGlobal,
                       const nsAString& aUrl,
                       const nsAString& aProtocol,
                       ErrorResult& aRv)
{
  Sequence<nsString> protocols;
  protocols.AppendElement(aProtocol);
  return WebSocket::Constructor(aGlobal, aUrl, protocols, aRv);
}

already_AddRefed<WebSocket>
WebSocket::Constructor(const GlobalObject& aGlobal,
                       const nsAString& aUrl,
                       const Sequence<nsString>& aProtocols,
                       ErrorResult& aRv)
{
  if (!PrefEnabled()) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

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

  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aGlobal.GetAsSupports());
  if (!sgo) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindow> ownerWindow = do_QueryInterface(aGlobal.GetAsSupports());
  if (!ownerWindow) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
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
    if (protocolElement.FindChar(',') != -1)  /* interferes w/list */ {
      aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
      return nullptr;
    }

    protocolArray.AppendElement(protocolElement);
  }

  nsRefPtr<WebSocket> webSocket = new WebSocket(ownerWindow);
  nsresult rv = webSocket->Init(aGlobal.GetContext(), principal,
                                aUrl, protocolArray);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }

  return webSocket.forget();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(WebSocket)

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(WebSocket)
  bool isBlack = tmp->IsBlack();
  if (isBlack|| tmp->mKeepingAlive) {
    if (tmp->mListenerManager) {
      tmp->mListenerManager->MarkForCC();
    }
    if (!isBlack && tmp->PreservingWrapper()) {
      // This marks the wrapper black.
      tmp->GetWrapper();
    }
    return true;
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(WebSocket)
  return tmp->IsBlack();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(WebSocket)
  return tmp->IsBlack();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(WebSocket,
                                               nsDOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(WebSocket,
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPrincipal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mURI)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChannel)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(WebSocket,
                                                nsDOMEventTargetHelper)
  tmp->Disconnect();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPrincipal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mURI)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChannel)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(WebSocket)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIWebSocketListener)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIRequest)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(WebSocket, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(WebSocket, nsDOMEventTargetHelper)

void
WebSocket::DisconnectFromOwner()
{
  nsDOMEventTargetHelper::DisconnectFromOwner();
  CloseConnection(nsIWebSocketChannel::CLOSE_GOING_AWAY);
  DontKeepAliveAnyMore();
}

//-----------------------------------------------------------------------------
// WebSocket:: initialization
//-----------------------------------------------------------------------------

nsresult
WebSocket::Init(JSContext* aCx,
                nsIPrincipal* aPrincipal,
                const nsAString& aURL,
                nsTArray<nsString>& aProtocolArray)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  MOZ_ASSERT(aPrincipal);

  if (!PrefEnabled()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  mPrincipal = aPrincipal;

  // Attempt to kill "ghost" websocket: but usually too early for check to fail
  nsresult rv = CheckInnerWindowCorrectness();
  NS_ENSURE_SUCCESS(rv, rv);

  // Shut down websocket if window is frozen or destroyed (only needed for
  // "ghost" websockets--see bug 696085)
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  NS_ENSURE_STATE(os);
  rv = os->AddObserver(this, DOM_WINDOW_DESTROYED_TOPIC, true);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = os->AddObserver(this, DOM_WINDOW_FROZEN_TOPIC, true);
  NS_ENSURE_SUCCESS(rv, rv);

  unsigned lineno;
  JS::Rooted<JSScript*> script(aCx);
  if (JS_DescribeScriptedCaller(aCx, &script, &lineno)) {
    mScriptFile = JS_GetScriptFilename(aCx, script);
    mScriptLine = lineno;
  }

  // Get WindowID
  mInnerWindowID = nsJSUtils::GetCurrentlyRunningCodeInnerWindowID(aCx);

  // parses the url
  rv = ParseURL(PromiseFlatString(aURL));
  NS_ENSURE_SUCCESS(rv, rv);

  nsIScriptContext* sc = GetContextForEventHandlers(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocument> originDoc = nsContentUtils::GetDocumentFromScriptContext(sc);

  // Don't allow https:// to open ws://
  if (!mSecure &&
      !Preferences::GetBool("network.websocket.allowInsecureFromHTTPS",
                            false)) {
    // Confirmed we are opening plain ws:// and want to prevent this from a
    // secure context (e.g. https). Check the security context of the document
    // associated with this script, which is the same as associated with mOwner.
    if (originDoc && originDoc->GetSecurityInfo()) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }
  }

  // Assign the sub protocol list and scan it for illegal values
  for (uint32_t index = 0; index < aProtocolArray.Length(); ++index) {
    for (uint32_t i = 0; i < aProtocolArray[index].Length(); ++i) {
      if (aProtocolArray[index][i] < static_cast<char16_t>(0x0021) ||
          aProtocolArray[index][i] > static_cast<char16_t>(0x007E))
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    if (!mRequestedProtocolList.IsEmpty()) {
      mRequestedProtocolList.Append(NS_LITERAL_CSTRING(", "));
    }

    AppendUTF16toUTF8(aProtocolArray[index], mRequestedProtocolList);
  }

  // Check content policy.
  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_WEBSOCKET,
                                 mURI,
                                 mPrincipal,
                                 originDoc,
                                 EmptyCString(),
                                 nullptr,
                                 &shouldLoad,
                                 nsContentUtils::GetContentPolicy(),
                                 nsContentUtils::GetSecurityManager());
  NS_ENSURE_SUCCESS(rv, rv);
  if (NS_CP_REJECTED(shouldLoad)) {
    // Disallowed by content policy.
    return NS_ERROR_CONTENT_BLOCKED;
  }

  // the constructor should throw a SYNTAX_ERROR only if it fails to parse the
  // url parameter, so don't throw if EstablishConnection fails, and call
  // onerror/onclose asynchronously
  if (NS_FAILED(EstablishConnection())) {
    FailConnection(nsIWebSocketChannel::CLOSE_ABNORMAL);
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// WebSocket methods:
//-----------------------------------------------------------------------------

class nsAutoCloseWS
{
public:
  nsAutoCloseWS(WebSocket* aWebSocket)
    : mWebSocket(aWebSocket)
  {}

  ~nsAutoCloseWS()
  {
    if (!mWebSocket->mChannel) {
      mWebSocket->CloseConnection(nsIWebSocketChannel::CLOSE_INTERNAL_ERROR);
    }
  }
private:
  nsRefPtr<WebSocket> mWebSocket;
};

nsresult
WebSocket::EstablishConnection()
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  NS_ABORT_IF_FALSE(!mChannel, "mChannel should be null");

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

  rv = wsChannel->SetNotificationCallbacks(this);
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
  }

  if (!mRequestedProtocolList.IsEmpty()) {
    rv = wsChannel->SetProtocol(mRequestedProtocolList);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCString asciiOrigin;
  rv = nsContentUtils::GetASCIIOrigin(mPrincipal, asciiOrigin);
  NS_ENSURE_SUCCESS(rv, rv);

  ToLowerCase(asciiOrigin);

  rv = wsChannel->AsyncOpen(mURI, asciiOrigin, this, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  mChannel = wsChannel;

  return NS_OK;
}

void
WebSocket::DispatchConnectionCloseEvents()
{
  mReadyState = WebSocket::CLOSED;

  // Call 'onerror' if needed
  if (mFailed) {
    nsresult rv = CreateAndDispatchSimpleEvent(NS_LITERAL_STRING("error"));
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to dispatch the error event");
    }
  }

  nsresult rv = CreateAndDispatchCloseEvent(mCloseEventWasClean, mCloseEventCode,
                                            mCloseEventReason);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch the close event");
  }

  UpdateMustKeepAlive();
  Disconnect();
}

nsresult
WebSocket::CreateAndDispatchSimpleEvent(const nsString& aName)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  nsresult rv = CheckInnerWindowCorrectness();
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMEvent> event;
  rv = NS_NewDOMEvent(getter_AddRefs(event), this, nullptr, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  // it doesn't bubble, and it isn't cancelable
  rv = event->InitEvent(aName, false, false);
  NS_ENSURE_SUCCESS(rv, rv);

  event->SetTrusted(true);

  return DispatchDOMEvent(nullptr, event, nullptr, nullptr);
}

nsresult
WebSocket::CreateAndDispatchMessageEvent(const nsACString& aData,
                                         bool isBinary)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  nsresult rv = CheckInnerWindowCorrectness();
  if (NS_FAILED(rv))
    return NS_OK;

  // Get the JSContext
  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(GetOwner());
  NS_ENSURE_TRUE(sgo, NS_ERROR_FAILURE);

  nsIScriptContext* scriptContext = sgo->GetContext();
  NS_ENSURE_TRUE(scriptContext, NS_ERROR_FAILURE);

  AutoPushJSContext cx(scriptContext->GetNativeContext());
  NS_ENSURE_TRUE(cx, NS_ERROR_FAILURE);

  // Create appropriate JS object for message
  JS::Rooted<JS::Value> jsData(cx);
  if (isBinary) {
    if (mBinaryType == dom::BinaryType::Blob) {
      rv = nsContentUtils::CreateBlobBuffer(cx, aData, &jsData);
      NS_ENSURE_SUCCESS(rv, rv);
    } else if (mBinaryType == dom::BinaryType::Arraybuffer) {
      JS::Rooted<JSObject*> arrayBuf(cx);
      rv = nsContentUtils::CreateArrayBuffer(cx, aData, arrayBuf.address());
      NS_ENSURE_SUCCESS(rv, rv);
      jsData = OBJECT_TO_JSVAL(arrayBuf);
    } else {
      NS_RUNTIMEABORT("Unknown binary type!");
      return NS_ERROR_UNEXPECTED;
    }
  } else {
    // JS string
    NS_ConvertUTF8toUTF16 utf16Data(aData);
    JSString* jsString;
    jsString = JS_NewUCStringCopyN(cx, utf16Data.get(), utf16Data.Length());
    NS_ENSURE_TRUE(jsString, NS_ERROR_FAILURE);

    jsData = STRING_TO_JSVAL(jsString);
  }

  // create an event that uses the MessageEvent interface,
  // which does not bubble, is not cancelable, and has no default action

  nsCOMPtr<nsIDOMEvent> event;
  rv = NS_NewDOMMessageEvent(getter_AddRefs(event), this, nullptr, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMMessageEvent> messageEvent = do_QueryInterface(event);
  rv = messageEvent->InitMessageEvent(NS_LITERAL_STRING("message"),
                                      false, false,
                                      jsData,
                                      mUTF16Origin,
                                      EmptyString(), nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  event->SetTrusted(true);

  return DispatchDOMEvent(nullptr, event, nullptr, nullptr);
}

nsresult
WebSocket::CreateAndDispatchCloseEvent(bool aWasClean,
                                       uint16_t aCode,
                                       const nsString &aReason)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  nsresult rv = CheckInnerWindowCorrectness();
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  // create an event that uses the CloseEvent interface,
  // which does not bubble, is not cancelable, and has no default action

  nsCOMPtr<nsIDOMEvent> event;
  rv = NS_NewDOMCloseEvent(getter_AddRefs(event), this, nullptr, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMCloseEvent> closeEvent = do_QueryInterface(event);
  rv = closeEvent->InitCloseEvent(NS_LITERAL_STRING("close"),
                                  false, false,
                                  aWasClean, aCode, aReason);
  NS_ENSURE_SUCCESS(rv, rv);

  event->SetTrusted(true);

  return DispatchDOMEvent(nullptr, event, nullptr, nullptr);
}

bool
WebSocket::PrefEnabled()
{
  return Preferences::GetBool("network.websocket.enabled", true);
}

nsresult
WebSocket::ParseURL(const nsString& aURL)
{
  NS_ENSURE_TRUE(!aURL.IsEmpty(), NS_ERROR_DOM_SYNTAX_ERR);

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURL);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_SYNTAX_ERR);

  nsCOMPtr<nsIURL> parsedURL = do_QueryInterface(uri, &rv);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_SYNTAX_ERR);

  nsAutoCString fragment;
  rv = parsedURL->GetRef(fragment);
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && fragment.IsEmpty(),
                 NS_ERROR_DOM_SYNTAX_ERR);

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

  rv = NS_CheckPortSafety(port, scheme.get());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_SYNTAX_ERR);

  nsAutoCString filePath;
  rv = parsedURL->GetFilePath(filePath);
  if (filePath.IsEmpty()) {
    filePath.AssignLiteral("/");
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
    mResource.AppendLiteral("?");
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

  mOriginalURL = aURL;
  mURI = parsedURL;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// Methods that keep alive the WebSocket object when:
//   1. the object has registered event listeners that can be triggered
//      ("strong event listeners");
//   2. there are outgoing not sent messages.
//-----------------------------------------------------------------------------

void
WebSocket::UpdateMustKeepAlive()
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  if (!mCheckMustKeepAlive) {
    return;
  }

  bool shouldKeepAlive = false;

  if (mListenerManager) {
    switch (mReadyState)
    {
      case WebSocket::CONNECTING:
      {
        if (mListenerManager->HasListenersFor(nsGkAtoms::onopen) ||
            mListenerManager->HasListenersFor(nsGkAtoms::onmessage) ||
            mListenerManager->HasListenersFor(nsGkAtoms::onerror) ||
            mListenerManager->HasListenersFor(nsGkAtoms::onclose)) {
          shouldKeepAlive = true;
        }
      }
      break;

      case WebSocket::OPEN:
      case WebSocket::CLOSING:
      {
        if (mListenerManager->HasListenersFor(nsGkAtoms::onmessage) ||
            mListenerManager->HasListenersFor(nsGkAtoms::onerror) ||
            mListenerManager->HasListenersFor(nsGkAtoms::onclose) ||
            mOutgoingBufferedAmount != 0) {
          shouldKeepAlive = true;
        }
      }
      break;

      case WebSocket::CLOSED:
      {
        shouldKeepAlive = false;
      }
    }
  }

  if (mKeepingAlive && !shouldKeepAlive) {
    mKeepingAlive = false;
    static_cast<EventTarget*>(this)->Release();
  } else if (!mKeepingAlive && shouldKeepAlive) {
    mKeepingAlive = true;
    static_cast<EventTarget*>(this)->AddRef();
  }
}

void
WebSocket::DontKeepAliveAnyMore()
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");
  if (mKeepingAlive) {
    mKeepingAlive = false;
    static_cast<EventTarget*>(this)->Release();
  }
  mCheckMustKeepAlive = false;
}

nsresult
WebSocket::UpdateURI()
{
  // Check for Redirections
  nsCOMPtr<nsIURI> uri;
  nsresult rv = mChannel->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString spec;
  rv = uri->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);
  CopyUTF8toUTF16(spec, mEffectiveURL);

  bool isWSS = false;
  rv = uri->SchemeIs("wss", &isWSS);
  NS_ENSURE_SUCCESS(rv, rv);
  mSecure = isWSS ? true : false;

  return NS_OK;
}

void
WebSocket::EventListenerAdded(nsIAtom* aType)
{
  UpdateMustKeepAlive();
}

void
WebSocket::EventListenerRemoved(nsIAtom* aType)
{
  UpdateMustKeepAlive();
}

//-----------------------------------------------------------------------------
// WebSocket - methods
//-----------------------------------------------------------------------------

// webIDL: readonly attribute DOMString url
void
WebSocket::GetUrl(nsAString& aURL)
{
  if (mEffectiveURL.IsEmpty()) {
    aURL = mOriginalURL;
  } else {
    aURL = mEffectiveURL;
  }
}

// webIDL: readonly attribute DOMString extensions;
void
WebSocket::GetExtensions(nsAString& aExtensions)
{
  CopyUTF8toUTF16(mEstablishedExtensions, aExtensions);
}

// webIDL: readonly attribute DOMString protocol;
void
WebSocket::GetProtocol(nsAString& aProtocol)
{
  CopyUTF8toUTF16(mEstablishedProtocol, aProtocol);
}

// webIDL: void send(DOMString data);
void
WebSocket::Send(const nsAString& aData,
                ErrorResult& aRv)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  NS_ConvertUTF16toUTF8 msgString(aData);
  Send(nullptr, msgString, msgString.Length(), false, aRv);
}

void
WebSocket::Send(nsIDOMBlob* aData,
                ErrorResult& aRv)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  nsCOMPtr<nsIInputStream> msgStream;
  nsresult rv = aData->GetInternalStream(getter_AddRefs(msgStream));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  uint64_t msgLength;
  rv = aData->GetSize(&msgLength);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  if (msgLength > UINT32_MAX) {
    aRv.Throw(NS_ERROR_FILE_TOO_BIG);
    return;
  }

  Send(msgStream, EmptyCString(), msgLength, true, aRv);
}

void
WebSocket::Send(const ArrayBuffer& aData,
                ErrorResult& aRv)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  MOZ_ASSERT(sizeof(*aData.Data()) == 1);
  uint32_t len = aData.Length();
  char* data = reinterpret_cast<char*>(aData.Data());

  nsDependentCSubstring msgString(data, len);
  Send(nullptr, msgString, len, true, aRv);
}

void
WebSocket::Send(const ArrayBufferView& aData,
                ErrorResult& aRv)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  MOZ_ASSERT(sizeof(*aData.Data()) == 1);
  uint32_t len = aData.Length();
  char* data = reinterpret_cast<char*>(aData.Data());

  nsDependentCSubstring msgString(data, len);
  Send(nullptr, msgString, len, true, aRv);
}

void
WebSocket::Send(nsIInputStream* aMsgStream,
                const nsACString& aMsgString,
                uint32_t aMsgLength,
                bool aIsBinary,
                ErrorResult& aRv)
{
  if (mReadyState == WebSocket::CONNECTING) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  // Always increment outgoing buffer len, even if closed
  mOutgoingBufferedAmount += aMsgLength;

  if (mReadyState == WebSocket::CLOSING ||
      mReadyState == WebSocket::CLOSED) {
    return;
  }

  MOZ_ASSERT(mReadyState == WebSocket::OPEN,
             "Unknown state in WebSocket::Send");

  nsresult rv;
  if (aMsgStream) {
    rv = mChannel->SendBinaryStream(aMsgStream, aMsgLength);
  } else {
    if (aIsBinary) {
      rv = mChannel->SendBinaryMsg(aMsgString);
    } else {
      rv = mChannel->SendMsg(aMsgString);
    }
  }

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return;
  }

  UpdateMustKeepAlive();
}

// webIDL: void close(optional unsigned short code, optional DOMString reason):
void
WebSocket::Close(const Optional<uint16_t>& aCode,
                 const Optional<nsAString>& aReason,
                 ErrorResult& aRv)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  // the reason code is optional, but if provided it must be in a specific range
  uint16_t closeCode = 0;
  if (aCode.WasPassed()) {
    if (aCode.Value() != 1000 && (aCode.Value() < 3000 || aCode.Value() > 4999)) {
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

  if (mReadyState == WebSocket::CLOSING ||
      mReadyState == WebSocket::CLOSED) {
    return;
  }

  if (mReadyState == WebSocket::CONNECTING) {
    FailConnection(closeCode, closeReason);
    return;
  }

  MOZ_ASSERT(mReadyState == WebSocket::OPEN);
  CloseConnection(closeCode, closeReason);
}

//-----------------------------------------------------------------------------
// WebSocket::nsIObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
WebSocket::Observe(nsISupports* aSubject,
                   const char* aTopic,
                   const char16_t* aData)
{
  if ((mReadyState == WebSocket::CLOSING) ||
      (mReadyState == WebSocket::CLOSED)) {
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aSubject);
  if (!GetOwner() || window != GetOwner()) {
    return NS_OK;
  }

  if ((strcmp(aTopic, DOM_WINDOW_FROZEN_TOPIC) == 0) ||
      (strcmp(aTopic, DOM_WINDOW_DESTROYED_TOPIC) == 0))
  {
    CloseConnection(nsIWebSocketChannel::CLOSE_GOING_AWAY);
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// WebSocket::nsIRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
WebSocket::GetName(nsACString& aName)
{
  CopyUTF16toUTF8(mOriginalURL, aName);
  return NS_OK;
}

NS_IMETHODIMP
WebSocket::IsPending(bool* aValue)
{
  *aValue = (mReadyState != WebSocket::CLOSED);
  return NS_OK;
}

NS_IMETHODIMP
WebSocket::GetStatus(nsresult* aStatus)
{
  *aStatus = NS_OK;
  return NS_OK;
}

// Window closed, stop/reload button pressed, user navigated away from page, etc.
NS_IMETHODIMP
WebSocket::Cancel(nsresult aStatus)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Not running on main thread");

  if (mReadyState == CLOSING || mReadyState == CLOSED) {
    return NS_OK;
  }

  ConsoleError();

  return CloseConnection(nsIWebSocketChannel::CLOSE_GOING_AWAY);
}

NS_IMETHODIMP
WebSocket::Suspend()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WebSocket::Resume()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WebSocket::GetLoadGroup(nsILoadGroup** aLoadGroup)
{
  *aLoadGroup = nullptr;

  nsresult rv;
  nsIScriptContext* sc = GetContextForEventHandlers(&rv);
  nsCOMPtr<nsIDocument> doc =
    nsContentUtils::GetDocumentFromScriptContext(sc);

  if (doc) {
    *aLoadGroup = doc->GetDocumentLoadGroup().get();  // already_AddRefed
  }

  return NS_OK;
}

NS_IMETHODIMP
WebSocket::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
WebSocket::GetLoadFlags(nsLoadFlags* aLoadFlags)
{
  *aLoadFlags = nsIRequest::LOAD_BACKGROUND;
  return NS_OK;
}

NS_IMETHODIMP
WebSocket::SetLoadFlags(nsLoadFlags aLoadFlags)
{
  // we won't change the load flags at all.
  return NS_OK;
}

} // dom namespace
} // mozilla namespace
