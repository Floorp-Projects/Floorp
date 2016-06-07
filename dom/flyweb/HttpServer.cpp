/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HttpServer.h"
#include "nsISocketTransport.h"
#include "nsWhitespaceTokenizer.h"
#include "nsNetUtil.h"
#include "nsIStreamTransportService.h"
#include "nsIAsyncStreamCopier2.h"
#include "nsIPipe.h"
#include "nsIOService.h"
#include "nsIHttpChannelInternal.h"
#include "Base64.h"
#include "WebSocketChannel.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsIX509Cert.h"

static NS_DEFINE_CID(kStreamTransportServiceCID, NS_STREAMTRANSPORTSERVICE_CID);

namespace mozilla {
namespace dom {

static LazyLogModule gHttpServerLog("HttpServer");
#undef LOG_I
#define LOG_I(...) MOZ_LOG(gHttpServerLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#undef LOG_V
#define LOG_V(...) MOZ_LOG(gHttpServerLog, mozilla::LogLevel::Verbose, (__VA_ARGS__))
#undef LOG_E
#define LOG_E(...) MOZ_LOG(gHttpServerLog, mozilla::LogLevel::Error, (__VA_ARGS__))


NS_IMPL_ISUPPORTS(HttpServer,
                  nsIServerSocketListener,
                  nsILocalCertGetCallback)

HttpServer::HttpServer()
{
}

HttpServer::~HttpServer()
{
}

void
HttpServer::Init(int32_t aPort, bool aHttps, HttpServerListener* aListener)
{
  mPort = aPort;
  mHttps = aHttps;
  mListener = aListener;

  nsCOMPtr<nsIPrefBranch> prefService;
  prefService = do_GetService(NS_PREFSERVICE_CONTRACTID);

  if (mHttps) {
    nsCOMPtr<nsILocalCertService> lcs =
      do_CreateInstance("@mozilla.org/security/local-cert-service;1");
    nsresult rv = lcs->GetOrCreateCert(NS_LITERAL_CSTRING("flyweb"), this);
    if (NS_FAILED(rv)) {
      NotifyStarted(rv);
    }
  } else {
    // Make sure to always have an async step before notifying callbacks
    HandleCert(nullptr, NS_OK);
  }
}

NS_IMETHODIMP
HttpServer::HandleCert(nsIX509Cert* aCert, nsresult aResult)
{
  nsresult rv = aResult;
  if (NS_SUCCEEDED(rv)) {
    rv = StartServerSocket(aCert);
  }

  if (NS_FAILED(rv) && mServerSocket) {
    mServerSocket->Close();
    mServerSocket = nullptr;
  }

  NotifyStarted(rv);

  return NS_OK;
}

void
HttpServer::NotifyStarted(nsresult aStatus)
{
  RefPtr<HttpServerListener> listener = mListener;
  nsCOMPtr<nsIRunnable> event = NS_NewRunnableFunction([listener, aStatus] ()
  {
    listener->OnServerStarted(aStatus);
  });
  NS_DispatchToCurrentThread(event);
}

nsresult
HttpServer::StartServerSocket(nsIX509Cert* aCert)
{
  nsresult rv;
  mServerSocket =
    do_CreateInstance(aCert ? "@mozilla.org/network/tls-server-socket;1"
                            : "@mozilla.org/network/server-socket;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mServerSocket->Init(mPort, false, -1);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aCert) {
    nsCOMPtr<nsITLSServerSocket> tls = do_QueryInterface(mServerSocket);
    rv = tls->SetServerCert(aCert);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = tls->SetSessionTickets(false);
    NS_ENSURE_SUCCESS(rv, rv);

    mCert = aCert;
  }

  rv = mServerSocket->AsyncListen(this);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mServerSocket->GetPort(&mPort);
  NS_ENSURE_SUCCESS(rv, rv);

  LOG_I("HttpServer::StartServerSocket(%p)", this);

  return NS_OK;
}

NS_IMETHODIMP
HttpServer::OnSocketAccepted(nsIServerSocket* aServ,
                             nsISocketTransport* aTransport)
{
  MOZ_ASSERT(SameCOMIdentity(aServ, mServerSocket));

  nsresult rv;
  RefPtr<Connection> conn = new Connection(aTransport, this, rv);
  NS_ENSURE_SUCCESS(rv, rv);

  LOG_I("HttpServer::OnSocketAccepted(%p) - Socket %p", this, conn.get());

  mConnections.AppendElement(conn.forget());

  return NS_OK;
}

NS_IMETHODIMP
HttpServer::OnStopListening(nsIServerSocket* aServ,
                            nsresult aStatus)
{
  MOZ_ASSERT(aServ == mServerSocket || !mServerSocket);

  LOG_I("HttpServer::OnStopListening(%p) - status 0x%lx", this, aStatus);

  Close();

  return NS_OK;
}

void
HttpServer::SendResponse(InternalRequest* aRequest, InternalResponse* aResponse)
{
  for (Connection* conn : mConnections) {
    if (conn->TryHandleResponse(aRequest, aResponse)) {
      return;
    }
  }

  MOZ_ASSERT(false, "Unknown request");
}

already_AddRefed<nsITransportProvider>
HttpServer::AcceptWebSocket(InternalRequest* aConnectRequest,
                            const Optional<nsAString>& aProtocol,
                            nsACString& aNegotiatedExtensions,
                            ErrorResult& aRv)
{
  for (Connection* conn : mConnections) {
    if (!conn->HasPendingWebSocketRequest(aConnectRequest)) {
      continue;
    }
    nsCOMPtr<nsITransportProvider> provider =
      conn->HandleAcceptWebSocket(aProtocol, aNegotiatedExtensions, aRv);
    if (aRv.Failed()) {
      conn->Close();
    }
    // This connection is now owned by the websocket, or we just closed it
    mConnections.RemoveElement(conn);
    return provider.forget();
  }

  aRv.Throw(NS_ERROR_UNEXPECTED);
  MOZ_ASSERT(false, "Unknown request");

  return nullptr;
}

void
HttpServer::SendWebSocketResponse(InternalRequest* aConnectRequest,
                                  InternalResponse* aResponse)
{
  for (Connection* conn : mConnections) {
    if (conn->HasPendingWebSocketRequest(aConnectRequest)) {
      conn->HandleWebSocketResponse(aResponse);
      return;
    }
  }

  MOZ_ASSERT(false, "Unknown request");
}

void
HttpServer::Close()
{
  if (mServerSocket) {
    mServerSocket->Close();
    mServerSocket = nullptr;
  }

  if (mListener) {
    RefPtr<HttpServerListener> listener = mListener.forget();
    listener->OnServerClose();
  }

  for (Connection* conn : mConnections) {
    conn->Close();
  }
  mConnections.Clear();
}

void
HttpServer::GetCertKey(nsACString& aKey)
{
  nsAutoString tmp;
  if (mCert) {
    mCert->GetSha256Fingerprint(tmp);
  }
  LossyCopyUTF16toASCII(tmp, aKey);
}

NS_IMPL_ISUPPORTS(HttpServer::TransportProvider,
                  nsITransportProvider)

HttpServer::TransportProvider::~TransportProvider()
{
}

NS_IMETHODIMP
HttpServer::TransportProvider::SetListener(nsIHttpUpgradeListener* aListener)
{
  MOZ_ASSERT(!mListener);
  MOZ_ASSERT(aListener);

  mListener = aListener;

  MaybeNotify();

  return NS_OK;
}

void
HttpServer::TransportProvider::SetTransport(nsISocketTransport* aTransport,
                                            nsIAsyncInputStream* aInput,
                                            nsIAsyncOutputStream* aOutput)
{
  MOZ_ASSERT(!mTransport);
  MOZ_ASSERT(aTransport && aInput && aOutput);

  mTransport = aTransport;
  mInput = aInput;
  mOutput = aOutput;

  MaybeNotify();
}

void
HttpServer::TransportProvider::MaybeNotify()
{
  if (mTransport && mListener) {
    RefPtr<TransportProvider> self = this;
    nsCOMPtr<nsIRunnable> event = NS_NewRunnableFunction([self, this] ()
    {
      mListener->OnTransportAvailable(mTransport, mInput, mOutput);
    });
    NS_DispatchToCurrentThread(event);
  }
}

NS_IMPL_ISUPPORTS(HttpServer::Connection,
                  nsIInputStreamCallback,
                  nsIOutputStreamCallback)

HttpServer::Connection::Connection(nsISocketTransport* aTransport,
                                   HttpServer* aServer,
                                   nsresult& rv)
  : mServer(aServer)
  , mTransport(aTransport)
  , mState(eRequestLine)
  , mCloseAfterRequest(false)
{
  nsCOMPtr<nsIInputStream> input;
  rv = mTransport->OpenInputStream(0, 0, 0, getter_AddRefs(input));
  NS_ENSURE_SUCCESS_VOID(rv);

  mInput = do_QueryInterface(input);

  nsCOMPtr<nsIOutputStream> output;
  rv = mTransport->OpenOutputStream(0, 0, 0, getter_AddRefs(output));
  NS_ENSURE_SUCCESS_VOID(rv);

  mOutput = do_QueryInterface(output);

  if (mServer->mHttps) {
    SetSecurityObserver(true);
  } else {
    mInput->AsyncWait(this, 0, 0, NS_GetCurrentThread());
  }
}

NS_IMETHODIMP
HttpServer::Connection::OnHandshakeDone(nsITLSServerSocket* aServer,
                                        nsITLSClientStatus* aStatus)
{
  LOG_I("HttpServer::Connection::OnHandshakeDone(%p)", this);

  // XXX Verify connection security

  SetSecurityObserver(false);
  mInput->AsyncWait(this, 0, 0, NS_GetCurrentThread());

  return NS_OK;
}

void
HttpServer::Connection::SetSecurityObserver(bool aListen)
{
  LOG_I("HttpServer::Connection::SetSecurityObserver(%p) - %s", this,
    aListen ? "On" : "Off");

  nsCOMPtr<nsISupports> secInfo;
  mTransport->GetSecurityInfo(getter_AddRefs(secInfo));
  nsCOMPtr<nsITLSServerConnectionInfo> tlsConnInfo =
    do_QueryInterface(secInfo);
  MOZ_ASSERT(tlsConnInfo);
  tlsConnInfo->SetSecurityObserver(aListen ? this : nullptr);
}

HttpServer::Connection::~Connection()
{
}

NS_IMETHODIMP
HttpServer::Connection::OnInputStreamReady(nsIAsyncInputStream* aStream)
{
  MOZ_ASSERT(!mInput || aStream == mInput);

  LOG_I("HttpServer::Connection::OnInputStreamReady(%p)", this);

  if (!mInput || mState == ePause) {
    return NS_OK;
  }

  uint64_t avail;
  nsresult rv = mInput->Available(&avail);
  if (NS_FAILED(rv)) {
    LOG_I("HttpServer::Connection::OnInputStreamReady(%p) - Connection closed", this);

    mServer->mConnections.RemoveElement(this);
    // Connection closed. Handle errors here.
    return NS_OK;
  }

  uint32_t numRead;
  rv = mInput->ReadSegments(ReadSegmentsFunc,
                            this,
                            UINT32_MAX,
                            &numRead);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mInput->AsyncWait(this, 0, 0, NS_GetCurrentThread());
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_METHOD
HttpServer::Connection::ReadSegmentsFunc(nsIInputStream* aIn,
                                         void* aClosure,
                                         const char* aBuffer,
                                         uint32_t aToOffset,
                                         uint32_t aCount,
                                         uint32_t* aWriteCount)
{
  const char* buffer = aBuffer;
  nsresult rv = static_cast<HttpServer::Connection*>(aClosure)->
    ConsumeInput(buffer, buffer + aCount);

  *aWriteCount = buffer - aBuffer;
  MOZ_ASSERT(*aWriteCount <= aCount);

  return rv;
}

static const char*
findCRLF(const char* aBuffer, const char* aEnd)
{
  if (aBuffer + 1 >= aEnd) {
    return nullptr;
  }

  const char* pos;
  while ((pos = static_cast<const char*>(memchr(aBuffer,
                                                '\r',
                                                aEnd - aBuffer - 1)))) {
    if (*(pos + 1) == '\n') {
      return pos;
    }
    aBuffer = pos + 1;
  }
  return nullptr;
}

nsresult
HttpServer::Connection::ConsumeInput(const char*& aBuffer,
                                     const char* aEnd)
{
  nsresult rv;
  while (mState == eRequestLine ||
         mState == eHeaders) {
    // Consume line-by-line

    // Check if buffer boundry ended up right between the CR and LF
    if (!mInputBuffer.IsEmpty() && mInputBuffer.Last() == '\r' &&
        *aBuffer == '\n') {
      aBuffer++;
      rv = ConsumeLine(mInputBuffer.BeginReading(), mInputBuffer.Length() - 1);
      NS_ENSURE_SUCCESS(rv, rv);

      mInputBuffer.Truncate();
    }

    // Look for a CRLF
    const char* pos = findCRLF(aBuffer, aEnd);
    if (!pos) {
      mInputBuffer.Append(aBuffer, aEnd - aBuffer);
      aBuffer = aEnd;
      return NS_OK;
    }

    if (!mInputBuffer.IsEmpty()) {
      mInputBuffer.Append(aBuffer, pos - aBuffer);
      aBuffer = pos + 2;
      rv = ConsumeLine(mInputBuffer.BeginReading(), mInputBuffer.Length() - 1);
      NS_ENSURE_SUCCESS(rv, rv);

      mInputBuffer.Truncate();
    } else {
      rv = ConsumeLine(aBuffer, pos - aBuffer);
      NS_ENSURE_SUCCESS(rv, rv);

      aBuffer = pos + 2;
    }
  }

  if (mState == eBody) {
    uint32_t size = std::min(mRemainingBodySize,
                             static_cast<uint32_t>(aEnd - aBuffer));
    uint32_t written = size;

    if (mCurrentRequestBody) {
      rv = mCurrentRequestBody->Write(aBuffer, size, &written);
      // Since we've given the pipe unlimited size, we should never
      // end up needing to block.
      MOZ_ASSERT(rv != NS_BASE_STREAM_WOULD_BLOCK);
      if (NS_FAILED(rv)) {
        written = size;
        mCurrentRequestBody = nullptr;
      }
    }

    aBuffer += written;
    mRemainingBodySize -= written;
    if (!mRemainingBodySize) {
      mCurrentRequestBody->Close();
      mCurrentRequestBody = nullptr;
      mState = eRequestLine;
    }
  }

  return NS_OK;
}

static bool
ContainsToken(const nsCString& aList, const nsCString& aToken)
{
  nsCCharSeparatedTokenizer tokens(aList, ',');
  bool found = false;
  while (!found && tokens.hasMoreTokens()) {
    found = tokens.nextToken().Equals(aToken);
  }
  return found;
}

static bool
IsWebSocketRequest(InternalRequest* aRequest, uint32_t aHttpVersion)
{
  if (aHttpVersion < 1) {
    return false;
  }

  nsAutoCString str;
  aRequest->GetMethod(str);
  if (!str.EqualsLiteral("GET")) {
    return false;
  }

  InternalHeaders* headers = aRequest->Headers();
  ErrorResult res;

  headers->Get(NS_LITERAL_CSTRING("upgrade"), str, res);
  MOZ_ASSERT(!res.Failed());
  if (!str.EqualsLiteral("websocket")) {
    return false;
  }

  headers->Get(NS_LITERAL_CSTRING("connection"), str, res);
  MOZ_ASSERT(!res.Failed());
  if (!ContainsToken(str, NS_LITERAL_CSTRING("Upgrade"))) {
    return false;
  }

  headers->Get(NS_LITERAL_CSTRING("sec-websocket-key"), str, res);
  MOZ_ASSERT(!res.Failed());
  nsAutoCString binary;
  if (NS_FAILED(Base64Decode(str, binary)) || binary.Length() != 16) {
    return false;
  }

  nsresult rv;
  headers->Get(NS_LITERAL_CSTRING("sec-websocket-version"), str, res);
  MOZ_ASSERT(!res.Failed());
  if (str.ToInteger(&rv) != 13 || NS_FAILED(rv)) {
    return false;
  }

  return true;
}

nsresult
HttpServer::Connection::ConsumeLine(const char* aBuffer,
                                    size_t aLength)
{
  MOZ_ASSERT(mState == eRequestLine ||
             mState == eHeaders);

  if (MOZ_LOG_TEST(gHttpServerLog, mozilla::LogLevel::Verbose)) {
    nsCString line(aBuffer, aLength);
    LOG_V("HttpServer::Connection::ConsumeLine(%p) - \"%s\"", this, line.get());
  }

  if (mState == eRequestLine) {
    LOG_V("HttpServer::Connection::ConsumeLine(%p) - Parsing request line", this);
    NS_ENSURE_FALSE(mCloseAfterRequest, NS_ERROR_UNEXPECTED);

    if (aLength == 0) {
      // Ignore empty lines before the request line
      return NS_OK;
    }
    MOZ_ASSERT(!mPendingReq);

    // Process request line
    nsCWhitespaceTokenizer tokens(Substring(aBuffer, aLength));

    NS_ENSURE_TRUE(tokens.hasMoreTokens(), NS_ERROR_UNEXPECTED);
    nsDependentCSubstring method = tokens.nextToken();
    NS_ENSURE_TRUE(NS_IsValidHTTPToken(method), NS_ERROR_UNEXPECTED);

    NS_ENSURE_TRUE(tokens.hasMoreTokens(), NS_ERROR_UNEXPECTED);
    nsDependentCSubstring url = tokens.nextToken();
    // Seems like it's also allowed to pass full urls with scheme+host+port.
    // May need to support that.
    NS_ENSURE_TRUE(url.First() == '/', NS_ERROR_UNEXPECTED);

    mPendingReq = new InternalRequest(url);
    mPendingReq->SetMethod(method);

    NS_ENSURE_TRUE(tokens.hasMoreTokens(), NS_ERROR_UNEXPECTED);
    nsDependentCSubstring version = tokens.nextToken();
    NS_ENSURE_TRUE(StringBeginsWith(version, NS_LITERAL_CSTRING("HTTP/1.")),
                   NS_ERROR_UNEXPECTED);
    nsresult rv;
    // This integer parsing is likely not strict enough.
    nsCString reqVersion;
    reqVersion = Substring(version, MOZ_ARRAY_LENGTH("HTTP/1.") - 1);
    mPendingReqVersion = reqVersion.ToInteger(&rv);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_UNEXPECTED);

    NS_ENSURE_FALSE(tokens.hasMoreTokens(), NS_ERROR_UNEXPECTED);

    LOG_V("HttpServer::Connection::ConsumeLine(%p) - Parsed request line", this);

    mState = eHeaders;

    return NS_OK;
  }

  if (aLength == 0) {
    LOG_V("HttpServer::Connection::ConsumeLine(%p) - Found end of headers", this);

    MaybeAddPendingHeader();

    ErrorResult res;
    mPendingReq->Headers()->SetGuard(HeadersGuardEnum::Immutable, res);

    // Check for WebSocket
    if (IsWebSocketRequest(mPendingReq, mPendingReqVersion)) {
      LOG_V("HttpServer::Connection::ConsumeLine(%p) - Fire OnWebSocket", this);

      mState = ePause;
      mPendingWebSocketRequest = mPendingReq.forget();
      mPendingReqVersion = 0;

      RefPtr<HttpServerListener> listener = mServer->mListener;
      RefPtr<InternalRequest> request = mPendingWebSocketRequest;
      nsCOMPtr<nsIRunnable> event =
        NS_NewRunnableFunction([listener, request] ()
      {
        listener->OnWebSocket(request);
      });
      NS_DispatchToCurrentThread(event);

      return NS_OK;
    }

    nsAutoCString header;
    mPendingReq->Headers()->Get(NS_LITERAL_CSTRING("connection"),
                                header,
                                res);
    MOZ_ASSERT(!res.Failed());
    // 1.0 defaults to closing connections.
    // 1.1 and higher defaults to keep-alive.
    if (ContainsToken(header, NS_LITERAL_CSTRING("close")) ||
        (mPendingReqVersion == 0 &&
         !ContainsToken(header, NS_LITERAL_CSTRING("keep-alive")))) {
      mCloseAfterRequest = true;
    }

    mPendingReq->Headers()->Get(NS_LITERAL_CSTRING("content-length"),
                                header,
                                res);
    MOZ_ASSERT(!res.Failed());

    LOG_V("HttpServer::Connection::ConsumeLine(%p) - content-length is \"%s\"",
          this, header.get());

    if (!header.IsEmpty()) {
      nsresult rv;
      mRemainingBodySize = header.ToInteger(&rv);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      mRemainingBodySize = 0;
    }

    if (mRemainingBodySize) {
      LOG_V("HttpServer::Connection::ConsumeLine(%p) - Starting consume body", this);
      mState = eBody;

      // We use an unlimited buffer size here to ensure
      // that we get to the next request even if the webpage hangs on
      // to the request indefinitely without consuming the body.
      nsCOMPtr<nsIInputStream> input;
      nsCOMPtr<nsIOutputStream> output;
      nsresult rv = NS_NewPipe(getter_AddRefs(input),
                               getter_AddRefs(output),
                               0,          // Segment size
                               UINT32_MAX, // Unlimited buffer size
                               false,      // not nonBlockingInput
                               true);      // nonBlockingOutput
      NS_ENSURE_SUCCESS(rv, rv);

      mCurrentRequestBody = do_QueryInterface(output);
      mPendingReq->SetBody(input);
    } else {
      LOG_V("HttpServer::Connection::ConsumeLine(%p) - No body", this);
      mState = eRequestLine;
    }

    mPendingRequests.AppendElement(PendingRequest(mPendingReq, nullptr));

    LOG_V("HttpServer::Connection::ConsumeLine(%p) - Fire OnRequest", this);

    RefPtr<HttpServerListener> listener = mServer->mListener;
    RefPtr<InternalRequest> request = mPendingReq.forget();
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableFunction([listener, request] ()
    {
      listener->OnRequest(request);
    });
    NS_DispatchToCurrentThread(event);

    mPendingReqVersion = 0;

    return NS_OK;
  }

  // Parse header line
  if (aBuffer[0] == ' ' || aBuffer[0] == '\t') {
    LOG_V("HttpServer::Connection::ConsumeLine(%p) - Add to header %s",
          this,
          mPendingHeaderName.get());

    NS_ENSURE_FALSE(mPendingHeaderName.IsEmpty(),
                    NS_ERROR_UNEXPECTED);

    // We might need to do whitespace trimming/compression here.
    mPendingHeaderValue.Append(aBuffer, aLength);
    return NS_OK;
  }

  MaybeAddPendingHeader();

  const char* colon = static_cast<const char*>(memchr(aBuffer, ':', aLength));
  NS_ENSURE_TRUE(colon, NS_ERROR_UNEXPECTED);

  ToLowerCase(Substring(aBuffer, colon - aBuffer), mPendingHeaderName);
  mPendingHeaderValue.Assign(colon + 1, aLength - (colon - aBuffer) - 1);

  NS_ENSURE_TRUE(NS_IsValidHTTPToken(mPendingHeaderName),
                 NS_ERROR_UNEXPECTED);

  LOG_V("HttpServer::Connection::ConsumeLine(%p) - Parsed header %s",
        this,
        mPendingHeaderName.get());

  return NS_OK;
}

void
HttpServer::Connection::MaybeAddPendingHeader()
{
  if (mPendingHeaderName.IsEmpty()) {
    return;
  }

  // We might need to do more whitespace trimming/compression here.
  mPendingHeaderValue.Trim(" \t");

  ErrorResult rv;
  mPendingReq->Headers()->Append(mPendingHeaderName, mPendingHeaderValue, rv);
  mPendingHeaderName.Truncate();
}

bool
HttpServer::Connection::TryHandleResponse(InternalRequest* aRequest,
                                          InternalResponse* aResponse)
{
  bool handledResponse = false;
  for (uint32_t i = 0; i < mPendingRequests.Length(); ++i) {
    PendingRequest& pending = mPendingRequests[i];
    if (pending.first() == aRequest) {
      MOZ_ASSERT(!handledResponse);
      MOZ_ASSERT(!pending.second());

      pending.second() = aResponse;
      if (i != 0) {
        return true;
      }
      handledResponse = true;
    }

    if (handledResponse && !pending.second()) {
      // Shortcut if we've handled the response, and
      // we don't have more responses to send
      return true;
    }

    if (i == 0 && pending.second()) {
      RefPtr<InternalResponse> resp = pending.second().forget();
      mPendingRequests.RemoveElementAt(0);
      QueueResponse(resp);
      --i;
    }
  }

  return handledResponse;
}

already_AddRefed<nsITransportProvider>
HttpServer::Connection::HandleAcceptWebSocket(const Optional<nsAString>& aProtocol,
                                              nsACString& aNegotiatedExtensions,
                                              ErrorResult& aRv)
{
  MOZ_ASSERT(mPendingWebSocketRequest);

  RefPtr<InternalResponse> response =
    new InternalResponse(101, NS_LITERAL_CSTRING("Switching Protocols"));

  InternalHeaders* headers = response->Headers();
  headers->Set(NS_LITERAL_CSTRING("Upgrade"),
               NS_LITERAL_CSTRING("websocket"),
               aRv);
  headers->Set(NS_LITERAL_CSTRING("Connection"),
               NS_LITERAL_CSTRING("Upgrade"),
               aRv);
  if (aProtocol.WasPassed()) {
    NS_ConvertUTF16toUTF8 protocol(aProtocol.Value());
    nsAutoCString reqProtocols;
    mPendingWebSocketRequest->Headers()->
      Get(NS_LITERAL_CSTRING("Sec-WebSocket-Protocol"), reqProtocols, aRv);
    if (!ContainsToken(reqProtocols, protocol)) {
      // Should throw a better error here
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    headers->Set(NS_LITERAL_CSTRING("Sec-WebSocket-Protocol"),
                 protocol, aRv);
  }

  nsAutoCString key, hash;
  mPendingWebSocketRequest->Headers()->
    Get(NS_LITERAL_CSTRING("Sec-WebSocket-Key"), key, aRv);
  nsresult rv = mozilla::net::CalculateWebSocketHashedSecret(key, hash);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }
  headers->Set(NS_LITERAL_CSTRING("Sec-WebSocket-Accept"), hash, aRv);

  nsAutoCString extensions;
  mPendingWebSocketRequest->Headers()->
    Get(NS_LITERAL_CSTRING("Sec-WebSocket-Extensions"), extensions, aRv);
  mozilla::net::ProcessServerWebSocketExtensions(extensions,
                                                 aNegotiatedExtensions);
  if (!aNegotiatedExtensions.IsEmpty()) {
    headers->Set(NS_LITERAL_CSTRING("Sec-WebSocket-Extensions"),
                 aNegotiatedExtensions, aRv);
  }

  RefPtr<TransportProvider> result = new TransportProvider();
  mWebSocketTransportProvider = result;

  QueueResponse(response);

  return result.forget();
}

void
HttpServer::Connection::HandleWebSocketResponse(InternalResponse* aResponse)
{
  MOZ_ASSERT(mPendingWebSocketRequest);

  mState = eRequestLine;
  mPendingWebSocketRequest = nullptr;
  mInput->AsyncWait(this, 0, 0, NS_GetCurrentThread());

  QueueResponse(aResponse);
}

void
HttpServer::Connection::QueueResponse(InternalResponse* aResponse)
{
  bool chunked = false;

  RefPtr<InternalHeaders> headers = new InternalHeaders(*aResponse->Headers());
  {
    ErrorResult res;
    headers->SetGuard(HeadersGuardEnum::None, res);
  }
  nsCOMPtr<nsIInputStream> body;
  int64_t bodySize;
  aResponse->GetBody(getter_AddRefs(body), &bodySize);

  if (body && bodySize >= 0) {
    nsCString sizeStr;
    sizeStr.AppendInt(bodySize);

    LOG_V("HttpServer::Connection::QueueResponse(%p) - "
          "Setting content-length to %s",
          this, sizeStr.get());

    ErrorResult res;
    headers->Set(NS_LITERAL_CSTRING("content-length"), sizeStr, res);
  } else if (body) {
    // Use chunked transfer encoding
    LOG_V("HttpServer::Connection::QueueResponse(%p) - Chunked transfer-encoding",
          this);

    ErrorResult res;
    headers->Set(NS_LITERAL_CSTRING("transfer-encoding"),
                 NS_LITERAL_CSTRING("chunked"),
                 res);
    headers->Delete(NS_LITERAL_CSTRING("content-length"), res);
    chunked = true;

  } else {
    LOG_V("HttpServer::Connection::QueueResponse(%p) - "
          "No body - setting content-length to 0", this);

    ErrorResult res;
    headers->Set(NS_LITERAL_CSTRING("content-length"),
                 NS_LITERAL_CSTRING("0"), res);
  }

  nsCString head(NS_LITERAL_CSTRING("HTTP/1.1 "));
  head.AppendInt(aResponse->GetStatus());
  // XXX is the statustext security checked?
  head.Append(NS_LITERAL_CSTRING(" ") +
              aResponse->GetStatusText() +
              NS_LITERAL_CSTRING("\r\n"));

  AutoTArray<InternalHeaders::Entry, 16> entries;
  headers->GetEntries(entries);

  for (auto header : entries) {
    head.Append(header.mName +
                NS_LITERAL_CSTRING(": ") +
                header.mValue +
                NS_LITERAL_CSTRING("\r\n"));
  }

  head.Append(NS_LITERAL_CSTRING("\r\n"));

  mOutputBuffers.AppendElement()->mString = head;
  if (body) {
    OutputBuffer* bodyBuffer = mOutputBuffers.AppendElement();
    bodyBuffer->mStream = body;
    bodyBuffer->mChunked = chunked;
  }

  OnOutputStreamReady(mOutput);
}

namespace {

typedef MozPromise<nsresult, bool, false> StreamCopyPromise;

class StreamCopier final : public nsIOutputStreamCallback
                         , public nsIInputStreamCallback
                         , public nsIRunnable
{
public:
  static RefPtr<StreamCopyPromise>
    Copy(nsIInputStream* aSource, nsIAsyncOutputStream* aSink,
         bool aChunked)
  {
    RefPtr<StreamCopier> copier = new StreamCopier(aSource, aSink, aChunked);

    RefPtr<StreamCopyPromise> p = copier->mPromise.Ensure(__func__);

    nsresult rv = copier->mTarget->Dispatch(copier, NS_DISPATCH_NORMAL);
    if (NS_FAILED(rv)) {
      copier->mPromise.Resolve(rv, __func__);
    }

    return p;
  }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIOUTPUTSTREAMCALLBACK
  NS_DECL_NSIRUNNABLE

private:
  StreamCopier(nsIInputStream* aSource, nsIAsyncOutputStream* aSink,
               bool aChunked)
    : mSource(aSource)
    , mAsyncSource(do_QueryInterface(aSource))
    , mSink(aSink)
    , mTarget(do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID))
    , mChunkRemaining(0)
    , mChunked(aChunked)
    , mAddedFinalSeparator(false)
    , mFirstChunk(aChunked)
    {
    }
  ~StreamCopier() {}

  static NS_METHOD FillOutputBufferHelper(nsIOutputStream* aOutStr,
                                          void* aClosure,
                                          char* aBuffer,
                                          uint32_t aOffset,
                                          uint32_t aCount,
                                          uint32_t* aCountRead);
  nsresult FillOutputBuffer(char* aBuffer,
                            uint32_t aCount,
                            uint32_t* aCountRead);

  nsCOMPtr<nsIInputStream> mSource;
  nsCOMPtr<nsIAsyncInputStream> mAsyncSource;
  nsCOMPtr<nsIAsyncOutputStream> mSink;
  MozPromiseHolder<StreamCopyPromise> mPromise;
  nsCOMPtr<nsIEventTarget> mTarget; // XXX we should cache this somewhere
  uint32_t mChunkRemaining;
  nsCString mSeparator;
  bool mChunked;
  bool mAddedFinalSeparator;
  bool mFirstChunk;
};

NS_IMPL_ISUPPORTS(StreamCopier,
                  nsIOutputStreamCallback,
                  nsIInputStreamCallback,
                  nsIRunnable)

struct WriteState
{
  StreamCopier* copier;
  nsresult sourceRv;
};

// This function only exists to enable FillOutputBuffer to be a non-static
// function where we can use member variables more easily.
NS_METHOD
StreamCopier::FillOutputBufferHelper(nsIOutputStream* aOutStr,
                                     void* aClosure,
                                     char* aBuffer,
                                     uint32_t aOffset,
                                     uint32_t aCount,
                                     uint32_t* aCountRead)
{
  WriteState* ws = static_cast<WriteState*>(aClosure);
  ws->sourceRv = ws->copier->FillOutputBuffer(aBuffer, aCount, aCountRead);
  return ws->sourceRv;
}

NS_METHOD
CheckForEOF(nsIInputStream* aIn,
            void* aClosure,
            const char* aBuffer,
            uint32_t aToOffset,
            uint32_t aCount,
            uint32_t* aWriteCount)
{
  *static_cast<bool*>(aClosure) = true;
  *aWriteCount = 0;
  return NS_BINDING_ABORTED;
}

nsresult
StreamCopier::FillOutputBuffer(char* aBuffer,
                               uint32_t aCount,
                               uint32_t* aCountRead)
{
  nsresult rv;
  while (mChunked && mSeparator.IsEmpty() && !mChunkRemaining &&
         !mAddedFinalSeparator) {
    uint64_t avail;
    rv = mSource->Available(&avail);
    if (rv == NS_BASE_STREAM_CLOSED) {
      avail = 0;
      rv = NS_OK;
    }
    NS_ENSURE_SUCCESS(rv, rv);

    mChunkRemaining = avail > UINT32_MAX ? UINT32_MAX :
                      static_cast<uint32_t>(avail);

    if (!mChunkRemaining) {
      // Either it's an non-blocking stream without any data
      // currently available, or we're at EOF. Sadly there's no way
      // to tell other than to read from the stream.
      bool hadData = false;
      uint32_t numRead;
      rv = mSource->ReadSegments(CheckForEOF, &hadData, 1, &numRead);
      if (rv == NS_BASE_STREAM_CLOSED) {
        avail = 0;
        rv = NS_OK;
      }
      NS_ENSURE_SUCCESS(rv, rv);
      MOZ_ASSERT(numRead == 0);

      if (hadData) {
        // The source received data between the call to Available and the
        // call to ReadSegments. Restart with a new call to Available
        continue;
      }

      // We're at EOF, write a separator with 0
      mAddedFinalSeparator = true;
    }

    if (mFirstChunk) {
      mFirstChunk = false;
      MOZ_ASSERT(mSeparator.IsEmpty());
    } else {
      // For all chunks except the first, add the newline at the end
      // of the previous chunk of data
      mSeparator.AssignLiteral("\r\n");
    }
    mSeparator.AppendInt(mChunkRemaining, 16);
    mSeparator.AppendLiteral("\r\n");

    if (mAddedFinalSeparator) {
      mSeparator.AppendLiteral("\r\n");
    }

    break;
  }

  // If we're doing chunked encoding, we should either have a chunk size,
  // or we should have reached the end of the input stream.
  MOZ_ASSERT_IF(mChunked, mChunkRemaining || mAddedFinalSeparator);
  // We should only have a separator if we're doing chunked encoding
  MOZ_ASSERT_IF(!mSeparator.IsEmpty(), mChunked);

  if (!mSeparator.IsEmpty()) {
    *aCountRead = std::min(mSeparator.Length(), aCount);
    memcpy(aBuffer, mSeparator.BeginReading(), *aCountRead);
    mSeparator.Cut(0, *aCountRead);
    rv = NS_OK;
  } else if (mChunked) {
    *aCountRead = 0;
    if (mChunkRemaining) {
      rv = mSource->Read(aBuffer,
                         std::min(aCount, mChunkRemaining),
                         aCountRead);
      mChunkRemaining -= *aCountRead;
    }
  } else {
    rv = mSource->Read(aBuffer, aCount, aCountRead);
  }

  if (NS_SUCCEEDED(rv) && *aCountRead == 0) {
    rv = NS_BASE_STREAM_CLOSED;
  }

  return rv;
}

NS_IMETHODIMP
StreamCopier::Run()
{
  nsresult rv;
  while (1) {
    WriteState state = { this, NS_OK };
    uint32_t written;
    rv = mSink->WriteSegments(FillOutputBufferHelper, &state,
                              mozilla::net::nsIOService::gDefaultSegmentSize,
                              &written);
    MOZ_ASSERT(NS_SUCCEEDED(rv) || NS_SUCCEEDED(state.sourceRv));
    if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
      mSink->AsyncWait(this, 0, 0, mTarget);
      return NS_OK;
    }
    if (NS_FAILED(rv)) {
      mPromise.Resolve(rv, __func__);
      return NS_OK;
    }

    if (state.sourceRv == NS_BASE_STREAM_WOULD_BLOCK) {
      MOZ_ASSERT(mAsyncSource);
      mAsyncSource->AsyncWait(this, 0, 0, mTarget);
      mSink->AsyncWait(this, nsIAsyncInputStream::WAIT_CLOSURE_ONLY,
                       0, mTarget);

      return NS_OK;
    }
    if (state.sourceRv == NS_BASE_STREAM_CLOSED) {
      // We're done!
      // No longer interested in callbacks about either stream closing
      mSink->AsyncWait(nullptr, 0, 0, nullptr);
      if (mAsyncSource) {
        mAsyncSource->AsyncWait(nullptr, 0, 0, nullptr);
      }

      mSource->Close();
      mSource = nullptr;
      mAsyncSource = nullptr;
      mSink = nullptr;

      mPromise.Resolve(NS_OK, __func__);

      return NS_OK;
    }

    if (NS_FAILED(state.sourceRv)) {
      mPromise.Resolve(state.sourceRv, __func__);
      return NS_OK;
    }
  }

  MOZ_ASSUME_UNREACHABLE_MARKER();
}

NS_IMETHODIMP
StreamCopier::OnInputStreamReady(nsIAsyncInputStream* aStream)
{
  MOZ_ASSERT(aStream == mAsyncSource ||
             (!mSource && !mAsyncSource && !mSink));
  return mSource ? Run() : NS_OK;
}

NS_IMETHODIMP
StreamCopier::OnOutputStreamReady(nsIAsyncOutputStream* aStream)
{
  MOZ_ASSERT(aStream == mSink ||
             (!mSource && !mAsyncSource && !mSink));
  return mSource ? Run() : NS_OK;
}

} // namespace

NS_IMETHODIMP
HttpServer::Connection::OnOutputStreamReady(nsIAsyncOutputStream* aStream)
{
  MOZ_ASSERT(aStream == mOutput || !mOutput);
  if (!mOutput) {
    return NS_OK;
  }

  nsresult rv;

  while (!mOutputBuffers.IsEmpty()) {
    if (!mOutputBuffers[0].mStream) {
      nsCString& buffer = mOutputBuffers[0].mString;
      while (!buffer.IsEmpty()) {
        uint32_t written = 0;
        rv = mOutput->Write(buffer.BeginReading(),
                            buffer.Length(),
                            &written);

        buffer.Cut(0, written);

        if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
          return mOutput->AsyncWait(this, 0, 0, NS_GetCurrentThread());
        }

        if (NS_FAILED(rv)) {
          Close();
          return NS_OK;
        }
      }
      mOutputBuffers.RemoveElementAt(0);
    } else {
      if (mOutputCopy) {
        // we're already copying the stream
        return NS_OK;
      }

      mOutputCopy =
        StreamCopier::Copy(mOutputBuffers[0].mStream,
                           mOutput,
                           mOutputBuffers[0].mChunked);

      RefPtr<Connection> self = this;

      mOutputCopy->
        Then(AbstractThread::MainThread(),
             __func__,
             [self, this] (nsresult aStatus) {
               MOZ_ASSERT(mOutputBuffers[0].mStream);
               LOG_V("HttpServer::Connection::OnOutputStreamReady(%p) - "
                     "Sent body. Status 0x%lx",
                     this, aStatus);

               mOutputBuffers.RemoveElementAt(0);
               mOutputCopy = nullptr;
               OnOutputStreamReady(mOutput);
             },
             [] (bool) { MOZ_ASSERT_UNREACHABLE("Reject unexpected"); });
    }
  }

  if (mPendingRequests.IsEmpty()) {
    if (mCloseAfterRequest) {
      LOG_V("HttpServer::Connection::OnOutputStreamReady(%p) - Closing channel",
            this);
      Close();
    } else if (mWebSocketTransportProvider) {
      mInput->AsyncWait(nullptr, 0, 0, nullptr);
      mOutput->AsyncWait(nullptr, 0, 0, nullptr);

      mWebSocketTransportProvider->SetTransport(mTransport, mInput, mOutput);
      mTransport = nullptr;
      mInput = nullptr;
      mOutput = nullptr;
      mWebSocketTransportProvider = nullptr;
    }
  }

  return NS_OK;
}

void
HttpServer::Connection::Close()
{
  if (!mTransport) {
    MOZ_ASSERT(!mOutput && !mInput);
    return;
  }

  mTransport->Close(NS_BINDING_ABORTED);
  if (mInput) {
    mInput->Close();
    mInput = nullptr;
  }
  if (mOutput) {
    mOutput->Close();
    mOutput = nullptr;
  }

  mTransport = nullptr;

  mInputBuffer.Truncate();
  mOutputBuffers.Clear();
  mPendingRequests.Clear();
}


} // namespace net
} // namespace mozilla
