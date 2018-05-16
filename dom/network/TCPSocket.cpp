/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ErrorResult.h"
#include "TCPSocket.h"
#include "TCPServerSocket.h"
#include "TCPSocketChild.h"
#include "mozilla/dom/TCPSocketBinding.h"
#include "mozilla/dom/TCPSocketErrorEvent.h"
#include "mozilla/dom/TCPSocketErrorEventBinding.h"
#include "mozilla/dom/TCPSocketEvent.h"
#include "mozilla/dom/TCPSocketEventBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsContentUtils.h"
#include "nsIArrayBufferInputStream.h"
#include "nsISocketTransportService.h"
#include "nsISocketTransport.h"
#include "nsIMultiplexInputStream.h"
#include "nsIAsyncStreamCopier.h"
#include "nsIInputStream.h"
#include "nsIBinaryInputStream.h"
#include "nsIScriptableInputStream.h"
#include "nsIInputStreamPump.h"
#include "nsIAsyncInputStream.h"
#include "nsISupportsPrimitives.h"
#include "nsITransport.h"
#include "nsIOutputStream.h"
#include "nsINSSErrorsService.h"
#include "nsISSLSocketControl.h"
#include "nsStringStream.h"
#include "secerr.h"
#include "sslerr.h"

#define BUFFER_SIZE 65536
#define NETWORK_STATS_THRESHOLD 65536

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION(LegacyMozTCPSocket, mGlobal)

NS_IMPL_CYCLE_COLLECTING_ADDREF(LegacyMozTCPSocket)
NS_IMPL_CYCLE_COLLECTING_RELEASE(LegacyMozTCPSocket)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(LegacyMozTCPSocket)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

LegacyMozTCPSocket::LegacyMozTCPSocket(nsPIDOMWindowInner* aWindow)
: mGlobal(do_QueryInterface(aWindow))
{
}

LegacyMozTCPSocket::~LegacyMozTCPSocket()
{
}

already_AddRefed<TCPSocket>
LegacyMozTCPSocket::Open(const nsAString& aHost,
                         uint16_t aPort,
                         const SocketOptions& aOptions,
                         mozilla::ErrorResult& aRv)
{
  AutoJSAPI api;
  if (NS_WARN_IF(!api.Init(mGlobal))) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  GlobalObject globalObj(api.cx(), mGlobal->GetGlobalJSObject());
  return TCPSocket::Constructor(globalObj, aHost, aPort, aOptions, aRv);
}

already_AddRefed<TCPServerSocket>
LegacyMozTCPSocket::Listen(uint16_t aPort,
                           const ServerSocketOptions& aOptions,
                           uint16_t aBacklog,
                           mozilla::ErrorResult& aRv)
{
  AutoJSAPI api;
  if (NS_WARN_IF(!api.Init(mGlobal))) {
    return nullptr;
  }
  GlobalObject globalObj(api.cx(), mGlobal->GetGlobalJSObject());
  return TCPServerSocket::Constructor(globalObj, aPort, aOptions, aBacklog, aRv);
}

bool
LegacyMozTCPSocket::WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto,
                               JS::MutableHandle<JSObject*> aReflector)
{
  return LegacyMozTCPSocketBinding::Wrap(aCx, this, aGivenProto, aReflector);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(TCPSocket)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(TCPSocket,
                                               DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(TCPSocket,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTransport)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSocketInputStream)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSocketOutputStream)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInputStreamPump)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInputStreamScriptable)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInputStreamBinary)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingDataAfterStartTLS)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSocketBridgeChild)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSocketBridgeParent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(TCPSocket,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTransport)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSocketInputStream)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSocketOutputStream)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mInputStreamPump)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mInputStreamScriptable)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mInputStreamBinary)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPendingDataAfterStartTLS)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSocketBridgeChild)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSocketBridgeParent)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(TCPSocket, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(TCPSocket, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TCPSocket)
  NS_INTERFACE_MAP_ENTRY(nsITransportEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIInputStreamCallback)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsITCPSocketCallback)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

TCPSocket::TCPSocket(nsIGlobalObject* aGlobal, const nsAString& aHost, uint16_t aPort,
                     bool aSsl, bool aUseArrayBuffers)
  : DOMEventTargetHelper(aGlobal)
  , mReadyState(TCPReadyState::Closed)
  , mUseArrayBuffers(aUseArrayBuffers)
  , mHost(aHost)
  , mPort(aPort)
  , mSsl(aSsl)
  , mAsyncCopierActive(false)
  , mWaitingForDrain(false)
  , mInnerWindowID(0)
  , mBufferedAmount(0)
  , mSuspendCount(0)
  , mTrackingNumber(0)
  , mWaitingForStartTLS(false)
  , mObserversActive(false)
{
  if (aGlobal) {
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal);
    if (window) {
      mInnerWindowID = window->WindowID();
    }
  }
}

TCPSocket::~TCPSocket()
{
  if (mObserversActive) {
    nsCOMPtr<nsIObserverService> obs = do_GetService("@mozilla.org/observer-service;1");
    if (obs) {
      obs->RemoveObserver(this, "inner-window-destroyed");
      obs->RemoveObserver(this, "profile-change-net-teardown");
    }
  }
}

nsresult
TCPSocket::CreateStream()
{
  nsresult rv = mTransport->OpenInputStream(0, 0, 0, getter_AddRefs(mSocketInputStream));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mTransport->OpenOutputStream(nsITransport::OPEN_UNBUFFERED, 0, 0, getter_AddRefs(mSocketOutputStream));
  NS_ENSURE_SUCCESS(rv, rv);

  // If the other side is not listening, we will
  // get an onInputStreamReady callback where available
  // raises to indicate the connection was refused.
  nsCOMPtr<nsIAsyncInputStream> asyncStream = do_QueryInterface(mSocketInputStream);
  NS_ENSURE_TRUE(asyncStream, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsIEventTarget> mainTarget = GetMainThreadEventTarget();
  rv = asyncStream->AsyncWait(this, nsIAsyncInputStream::WAIT_CLOSURE_ONLY, 0, mainTarget);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mUseArrayBuffers) {
    mInputStreamBinary = do_CreateInstance("@mozilla.org/binaryinputstream;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mInputStreamBinary->SetInputStream(mSocketInputStream);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    mInputStreamScriptable = do_CreateInstance("@mozilla.org/scriptableinputstream;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mInputStreamScriptable->Init(mSocketInputStream);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
TCPSocket::InitWithUnconnectedTransport(nsISocketTransport* aTransport)
{
  mReadyState = TCPReadyState::Connecting;
  mTransport = aTransport;

  MOZ_ASSERT(XRE_GetProcessType() != GeckoProcessType_Content);

  nsCOMPtr<nsIEventTarget> mainTarget = GetMainThreadEventTarget();
  mTransport->SetEventSink(this, mainTarget);

  nsresult rv = CreateStream();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
TCPSocket::Init()
{
  nsCOMPtr<nsIObserverService> obs = do_GetService("@mozilla.org/observer-service;1");
  if (obs) {
    mObserversActive = true;
    obs->AddObserver(this, "inner-window-destroyed", true); // weak reference
    obs->AddObserver(this, "profile-change-net-teardown", true); // weak ref
  }

  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    mReadyState = TCPReadyState::Connecting;

    nsCOMPtr<nsIEventTarget> target;
    if (nsCOMPtr<nsIGlobalObject> global = GetOwnerGlobal()) {
      target = global->EventTargetFor(TaskCategory::Other);
    }
    mSocketBridgeChild = new TCPSocketChild(mHost, mPort, target);
    mSocketBridgeChild->SendOpen(this, mSsl, mUseArrayBuffers);
    return NS_OK;
  }

  nsCOMPtr<nsISocketTransportService> sts =
    do_GetService("@mozilla.org/network/socket-transport-service;1");

  const char* socketTypes[1];
  if (mSsl) {
    socketTypes[0] = "ssl";
  } else {
    socketTypes[0] = "starttls";
  }
  nsCOMPtr<nsISocketTransport> transport;
  nsresult rv = sts->CreateTransport(socketTypes, 1, NS_ConvertUTF16toUTF8(mHost), mPort,
                                     nullptr, getter_AddRefs(transport));
  NS_ENSURE_SUCCESS(rv, rv);

  return InitWithUnconnectedTransport(transport);
}

void
TCPSocket::InitWithSocketChild(TCPSocketChild* aSocketBridge)
{
  mSocketBridgeChild = aSocketBridge;
  mReadyState = TCPReadyState::Open;
  mSocketBridgeChild->SetSocket(this);
  mSocketBridgeChild->GetHost(mHost);
  mSocketBridgeChild->GetPort(&mPort);
}

nsresult
TCPSocket::InitWithTransport(nsISocketTransport* aTransport)
{
  mTransport = aTransport;
  nsresult rv = CreateStream();
  NS_ENSURE_SUCCESS(rv, rv);

  mReadyState = TCPReadyState::Open;
  rv = CreateInputStreamPump();
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString host;
  mTransport->GetHost(host);
  mHost = NS_ConvertUTF8toUTF16(host);
  int32_t port;
  mTransport->GetPort(&port);
  mPort = port;

  return NS_OK;
}

void
TCPSocket::UpgradeToSecure(mozilla::ErrorResult& aRv)
{
  if (mReadyState != TCPReadyState::Open) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (mSsl) {
    return;
  }

  mSsl = true;

  if (mSocketBridgeChild) {
    mSocketBridgeChild->SendStartTLS();
    return;
  }

  if (!mAsyncCopierActive) {
    ActivateTLS();
  } else {
    mWaitingForStartTLS = true;
  }
}

namespace {
class CopierCallbacks final : public nsIRequestObserver
{
  RefPtr<TCPSocket> mOwner;
public:
  explicit CopierCallbacks(TCPSocket* aSocket) : mOwner(aSocket) {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
private:
  ~CopierCallbacks() {}
};

NS_IMPL_ISUPPORTS(CopierCallbacks, nsIRequestObserver)

NS_IMETHODIMP
CopierCallbacks::OnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  return NS_OK;
}

NS_IMETHODIMP
CopierCallbacks::OnStopRequest(nsIRequest* aRequest, nsISupports* aContext, nsresult aStatus)
{
  mOwner->NotifyCopyComplete(aStatus);
  mOwner = nullptr;
  return NS_OK;
}
} // unnamed namespace

nsresult
TCPSocket::EnsureCopying()
{
  if (mAsyncCopierActive) {
    return NS_OK;
  }

  mAsyncCopierActive = true;

  nsresult rv;

  nsCOMPtr<nsIMultiplexInputStream> multiplexStream =
    do_CreateInstance("@mozilla.org/io/multiplex-input-stream;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> stream = do_QueryInterface(multiplexStream);

  while (!mPendingData.IsEmpty()) {
    nsCOMPtr<nsIInputStream> stream = mPendingData[0];
    multiplexStream->AppendStream(stream);
    mPendingData.RemoveElementAt(0);
  }

  nsCOMPtr<nsIAsyncStreamCopier> copier =
    do_CreateInstance("@mozilla.org/network/async-stream-copier;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsISocketTransportService> sts =
      do_GetService("@mozilla.org/network/socket-transport-service;1");

  nsCOMPtr<nsIEventTarget> target = do_QueryInterface(sts);
  rv = copier->Init(stream,
                    mSocketOutputStream,
                    target,
                    true, /* source buffered */
                    false, /* sink buffered */
                    BUFFER_SIZE,
                    false, /* close source */
                    false); /* close sink */
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<CopierCallbacks> callbacks = new CopierCallbacks(this);
  rv = copier->AsyncCopy(callbacks, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
TCPSocket::NotifyCopyComplete(nsresult aStatus)
{
  mAsyncCopierActive = false;

  // Let's update the buffered amount of data.
  uint64_t bufferedAmount = 0;
  for (uint32_t i = 0, len = mPendingData.Length(); i < len; ++i) {
    nsCOMPtr<nsIInputStream> stream = mPendingData[i];
    uint64_t available = 0;
    if (NS_SUCCEEDED(stream->Available(&available))) {
      bufferedAmount += available;
    }
  }
  mBufferedAmount = bufferedAmount;

  if (mSocketBridgeParent) {
    mozilla::Unused << mSocketBridgeParent->SendUpdateBufferedAmount(BufferedAmount(),
                                                                     mTrackingNumber);
  }

  if (NS_FAILED(aStatus)) {
    MaybeReportErrorAndCloseIfOpen(aStatus);
    return;
  }

  if (bufferedAmount != 0) {
    EnsureCopying();
    return;
  }

  // Maybe we have some empty stream. We want to have an empty queue now.
  mPendingData.Clear();

  // If we are waiting for initiating starttls, we can begin to
  // activate tls now.
  if (mWaitingForStartTLS && mReadyState == TCPReadyState::Open) {
    ActivateTLS();
    mWaitingForStartTLS = false;
    // If we have pending data, we should send them, or fire
    // a drain event if we are waiting for it.
    if (!mPendingDataAfterStartTLS.IsEmpty()) {
      mPendingData.SwapElements(mPendingDataAfterStartTLS);
      EnsureCopying();
      return;
    }
  }

  // If we have a connected child, we let the child decide whether
  // ondrain should be dispatched.
  if (mWaitingForDrain && !mSocketBridgeParent) {
    mWaitingForDrain = false;
    FireEvent(NS_LITERAL_STRING("drain"));
  }

  if (mReadyState == TCPReadyState::Closing) {
    if (mSocketOutputStream) {
      mSocketOutputStream->Close();
      mSocketOutputStream = nullptr;
    }
    mReadyState = TCPReadyState::Closed;
    FireEvent(NS_LITERAL_STRING("close"));
  }
}

void
TCPSocket::ActivateTLS()
{
  nsCOMPtr<nsISupports> securityInfo;
  mTransport->GetSecurityInfo(getter_AddRefs(securityInfo));
  nsCOMPtr<nsISSLSocketControl> socketControl = do_QueryInterface(securityInfo);
  if (socketControl) {
    socketControl->StartTLS();
  }
}

NS_IMETHODIMP
TCPSocket::FireErrorEvent(const nsAString& aName, const nsAString& aType)
{
  if (mSocketBridgeParent) {
    mSocketBridgeParent->FireErrorEvent(aName, aType, mReadyState);
    return NS_OK;
  }

  TCPSocketErrorEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mName = aName;
  init.mMessage = aType;

  RefPtr<TCPSocketErrorEvent> event =
    TCPSocketErrorEvent::Constructor(this, NS_LITERAL_STRING("error"), init);
  MOZ_ASSERT(event);
  event->SetTrusted(true);
  DispatchEvent(*event);
  return NS_OK;
}

NS_IMETHODIMP
TCPSocket::FireEvent(const nsAString& aType)
{
  if (mSocketBridgeParent) {
    mSocketBridgeParent->FireEvent(aType, mReadyState);
    return NS_OK;
  }

  AutoJSAPI api;
  if (NS_WARN_IF(!api.Init(GetOwnerGlobal()))) {
    return NS_ERROR_FAILURE;
  }
  JS::Rooted<JS::Value> val(api.cx());
  return FireDataEvent(api.cx(), aType, val);
}

NS_IMETHODIMP
TCPSocket::FireDataArrayEvent(const nsAString& aType,
                              const InfallibleTArray<uint8_t>& buffer)
{
  AutoJSAPI api;
  if (NS_WARN_IF(!api.Init(GetOwnerGlobal()))) {
    return NS_ERROR_FAILURE;
  }
  JSContext* cx = api.cx();
  JS::Rooted<JS::Value> val(cx);

  bool ok = IPC::DeserializeArrayBuffer(cx, buffer, &val);
  if (ok) {
    return FireDataEvent(cx, aType, val);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
TCPSocket::FireDataStringEvent(const nsAString& aType,
                               const nsACString& aString)
{
  AutoJSAPI api;
  if (NS_WARN_IF(!api.Init(GetOwnerGlobal()))) {
    return NS_ERROR_FAILURE;
  }
  JSContext* cx = api.cx();
  JS::Rooted<JS::Value> val(cx);

  bool ok = ToJSValue(cx, NS_ConvertASCIItoUTF16(aString), &val);
  if (ok) {
    return FireDataEvent(cx, aType, val);
  }
  return NS_ERROR_FAILURE;
}

nsresult
TCPSocket::FireDataEvent(JSContext* aCx, const nsAString& aType, JS::Handle<JS::Value> aData)
{
  MOZ_ASSERT(!mSocketBridgeParent);

  RootedDictionary<TCPSocketEventInit> init(aCx);
  init.mBubbles = false;
  init.mCancelable = false;
  init.mData = aData;

  RefPtr<TCPSocketEvent> event =
    TCPSocketEvent::Constructor(this, aType, init);
  event->SetTrusted(true);
  DispatchEvent(*event);
  return NS_OK;
}

JSObject*
TCPSocket::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return TCPSocketBinding::Wrap(aCx, this, aGivenProto);
}

void
TCPSocket::GetHost(nsAString& aHost)
{
  aHost.Assign(mHost);
}

uint32_t
TCPSocket::Port()
{
  return mPort;
}

bool
TCPSocket::Ssl()
{
  return mSsl;
}

void
TCPSocket::Suspend()
{
  if (mSocketBridgeChild) {
    mSocketBridgeChild->SendSuspend();
    return;
  }
  if (mInputStreamPump) {
    mInputStreamPump->Suspend();
  }
  mSuspendCount++;
}

void
TCPSocket::Resume(mozilla::ErrorResult& aRv)
{
  if (mSocketBridgeChild) {
    mSocketBridgeChild->SendResume();
    return;
  }
  if (!mSuspendCount) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  if (mInputStreamPump) {
    mInputStreamPump->Resume();
  }
  mSuspendCount--;
}

nsresult
TCPSocket::MaybeReportErrorAndCloseIfOpen(nsresult status) {
  // If we're closed, we've already reported the error or just don't need to
  // report the error.
  if (mReadyState == TCPReadyState::Closed) {
    return NS_OK;
  }

  // go through ::Closing state and then mark ::Closed
  Close();
  mReadyState = TCPReadyState::Closed;

  if (NS_FAILED(status)) {
    // Convert the status code to an appropriate error message.

    nsString errorType, errName;

    // security module? (and this is an error)
    if ((static_cast<uint32_t>(status) & 0xFF0000) == 0x5a0000) {
      nsCOMPtr<nsINSSErrorsService> errSvc = do_GetService("@mozilla.org/nss_errors_service;1");
      // getErrorClass will throw a generic NS_ERROR_FAILURE if the error code is
      // somehow not in the set of covered errors.
      uint32_t errorClass;
      nsresult rv = errSvc->GetErrorClass(status, &errorClass);
      if (NS_FAILED(rv)) {
        errorType.AssignLiteral("SecurityProtocol");
      } else {
        switch (errorClass) {
          case nsINSSErrorsService::ERROR_CLASS_BAD_CERT:
            errorType.AssignLiteral("SecurityCertificate");
            break;
          default:
            errorType.AssignLiteral("SecurityProtocol");
            break;
        }
      }

      // NSS_SEC errors (happen below the base value because of negative vals)
      if ((static_cast<int32_t>(status) & 0xFFFF) < abs(nsINSSErrorsService::NSS_SEC_ERROR_BASE)) {
        switch (static_cast<SECErrorCodes>(status)) {
          case SEC_ERROR_EXPIRED_CERTIFICATE:
            errName.AssignLiteral("SecurityExpiredCertificateError");
            break;
          case SEC_ERROR_REVOKED_CERTIFICATE:
            errName.AssignLiteral("SecurityRevokedCertificateError");
            break;
            // per bsmith, we will be unable to tell these errors apart very soon,
            // so it makes sense to just folder them all together already.
          case SEC_ERROR_UNKNOWN_ISSUER:
          case SEC_ERROR_UNTRUSTED_ISSUER:
          case SEC_ERROR_UNTRUSTED_CERT:
          case SEC_ERROR_CA_CERT_INVALID:
            errName.AssignLiteral("SecurityUntrustedCertificateIssuerError");
            break;
          case SEC_ERROR_INADEQUATE_KEY_USAGE:
            errName.AssignLiteral("SecurityInadequateKeyUsageError");
            break;
          case SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED:
            errName.AssignLiteral("SecurityCertificateSignatureAlgorithmDisabledError");
            break;
          default:
            errName.AssignLiteral("SecurityError");
            break;
        }
      } else {
        // NSS_SSL errors
        switch (static_cast<SSLErrorCodes>(status)) {
          case SSL_ERROR_NO_CERTIFICATE:
            errName.AssignLiteral("SecurityNoCertificateError");
            break;
          case SSL_ERROR_BAD_CERTIFICATE:
            errName.AssignLiteral("SecurityBadCertificateError");
            break;
          case SSL_ERROR_UNSUPPORTED_CERTIFICATE_TYPE:
            errName.AssignLiteral("SecurityUnsupportedCertificateTypeError");
            break;
          case SSL_ERROR_UNSUPPORTED_VERSION:
            errName.AssignLiteral("SecurityUnsupportedTLSVersionError");
            break;
          case SSL_ERROR_BAD_CERT_DOMAIN:
            errName.AssignLiteral("SecurityCertificateDomainMismatchError");
            break;
          default:
            errName.AssignLiteral("SecurityError");
            break;
        }
      }
    } else {
      // must be network
      errorType.AssignLiteral("Network");

      switch (status) {
        // connect to host:port failed
        case NS_ERROR_CONNECTION_REFUSED:
          errName.AssignLiteral("ConnectionRefusedError");
          break;
          // network timeout error
        case NS_ERROR_NET_TIMEOUT:
          errName.AssignLiteral("NetworkTimeoutError");
          break;
          // hostname lookup failed
        case NS_ERROR_UNKNOWN_HOST:
          errName.AssignLiteral("DomainNotFoundError");
          break;
        case NS_ERROR_NET_INTERRUPT:
          errName.AssignLiteral("NetworkInterruptError");
          break;
        default:
          errName.AssignLiteral("NetworkError");
          break;
      }
    }

    Unused << NS_WARN_IF(NS_FAILED(FireErrorEvent(errName, errorType)));
  }

  return FireEvent(NS_LITERAL_STRING("close"));
}

void
TCPSocket::Close()
{
  CloseHelper(true);
}

void
TCPSocket::CloseImmediately()
{
  CloseHelper(false);
}

void
TCPSocket::CloseHelper(bool waitForUnsentData)
{
  if (mReadyState == TCPReadyState::Closed || mReadyState == TCPReadyState::Closing) {
    return;
  }

  mReadyState = TCPReadyState::Closing;

  if (mSocketBridgeChild) {
    mSocketBridgeChild->SendClose();
    return;
  }

  if (!mAsyncCopierActive || !waitForUnsentData) {

    mPendingData.Clear();
    mPendingDataAfterStartTLS.Clear();

    if (mSocketOutputStream) {
      mSocketOutputStream->Close();
      mSocketOutputStream = nullptr;
    }
  }

  if (mSocketInputStream) {
    mSocketInputStream->Close();
    mSocketInputStream = nullptr;
  }
}

void
TCPSocket::SendWithTrackingNumber(const nsACString& aData,
                                  const uint32_t& aTrackingNumber,
                                  mozilla::ErrorResult& aRv)
{
  MOZ_ASSERT(mSocketBridgeParent);
  mTrackingNumber = aTrackingNumber;
  // The JSContext isn't necessary for string values; it's a codegen limitation.
  Send(nullptr, aData, aRv);
}

bool
TCPSocket::Send(JSContext* aCx, const nsACString& aData, mozilla::ErrorResult& aRv)
{
  if (mReadyState != TCPReadyState::Open) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  uint64_t byteLength;
  nsCOMPtr<nsIInputStream> stream;
  if (mSocketBridgeChild) {
    mSocketBridgeChild->SendSend(aData, ++mTrackingNumber);
    byteLength = aData.Length();
  } else {
    nsresult rv = NS_NewCStringInputStream(getter_AddRefs(stream), aData);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return false;
    }
    rv = stream->Available(&byteLength);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return false;
    }
  }
  return Send(stream, byteLength);
}

void
TCPSocket::SendWithTrackingNumber(JSContext* aCx,
                                  const ArrayBuffer& aData,
                                  uint32_t aByteOffset,
                                  const Optional<uint32_t>& aByteLength,
                                  const uint32_t& aTrackingNumber,
                                  mozilla::ErrorResult& aRv)
{
  MOZ_ASSERT(mSocketBridgeParent);
  mTrackingNumber = aTrackingNumber;
  Send(aCx, aData, aByteOffset, aByteLength, aRv);
}

bool
TCPSocket::Send(JSContext* aCx,
                const ArrayBuffer& aData,
                uint32_t aByteOffset,
                const Optional<uint32_t>& aByteLength,
                mozilla::ErrorResult& aRv)
{
  if (mReadyState != TCPReadyState::Open) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  nsCOMPtr<nsIArrayBufferInputStream> stream;

  aData.ComputeLengthAndData();
  uint32_t byteLength = aByteLength.WasPassed() ? aByteLength.Value() : aData.Length();

  if (mSocketBridgeChild) {
    nsresult rv = mSocketBridgeChild->SendSend(aData, aByteOffset, byteLength, ++mTrackingNumber);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRv.Throw(rv);
      return false;
    }
  } else {
    JS::Rooted<JSObject*> obj(aCx, aData.Obj());
    JSAutoRealm ar(aCx, obj);
    JS::Rooted<JS::Value> value(aCx, JS::ObjectValue(*obj));

    stream = do_CreateInstance("@mozilla.org/io/arraybuffer-input-stream;1");
    nsresult rv = stream->SetData(value, aByteOffset, byteLength, aCx);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRv.Throw(rv);
      return false;
    }
  }
  return Send(stream, byteLength);
}

bool
TCPSocket::Send(nsIInputStream* aStream, uint32_t aByteLength)
{
  uint64_t newBufferedAmount = BufferedAmount() + aByteLength;
  bool bufferFull = newBufferedAmount > BUFFER_SIZE;
  if (bufferFull) {
    // If we buffered more than some arbitrary amount of data,
    // (65535 right now) we should tell the caller so they can
    // wait until ondrain is called if they so desire. Once all the
    // buffered data has been written to the socket, ondrain is
    // called.
    mWaitingForDrain = true;
  }

  if (mSocketBridgeChild) {
    // In the child, we just add the buffer length to our bufferedAmount and let
    // the parent update our bufferedAmount when the data have been sent.
    mBufferedAmount = newBufferedAmount;
    return !bufferFull;
  }

  if (mWaitingForStartTLS) {
    // When we are waiting for starttls, newStream is added to pendingData
    // and will be appended to multiplexStream after tls had been set up.
    mPendingDataAfterStartTLS.AppendElement(aStream);
  } else {
    mPendingData.AppendElement(aStream);
  }

  EnsureCopying();

  return !bufferFull;
}

TCPReadyState
TCPSocket::ReadyState()
{
  return mReadyState;
}

TCPSocketBinaryType
TCPSocket::BinaryType()
{
  if (mUseArrayBuffers) {
    return TCPSocketBinaryType::Arraybuffer;
  } else {
    return TCPSocketBinaryType::String;
  }
}

already_AddRefed<TCPSocket>
TCPSocket::CreateAcceptedSocket(nsIGlobalObject* aGlobal,
                                nsISocketTransport* aTransport,
                                bool aUseArrayBuffers)
{
  RefPtr<TCPSocket> socket = new TCPSocket(aGlobal, EmptyString(), 0, false, aUseArrayBuffers);
  nsresult rv = socket->InitWithTransport(aTransport);
  NS_ENSURE_SUCCESS(rv, nullptr);
  return socket.forget();
}

already_AddRefed<TCPSocket>
TCPSocket::CreateAcceptedSocket(nsIGlobalObject* aGlobal,
                                TCPSocketChild* aBridge,
                                bool aUseArrayBuffers)
{
  RefPtr<TCPSocket> socket = new TCPSocket(aGlobal, EmptyString(), 0, false, aUseArrayBuffers);
  socket->InitWithSocketChild(aBridge);
  return socket.forget();
}

already_AddRefed<TCPSocket>
TCPSocket::Constructor(const GlobalObject& aGlobal,
                       const nsAString& aHost,
                       uint16_t aPort,
                       const SocketOptions& aOptions,
                       mozilla::ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<TCPSocket> socket =
    new TCPSocket(global, aHost, aPort, aOptions.mUseSecureTransport,
                  aOptions.mBinaryType == TCPSocketBinaryType::Arraybuffer);
  nsresult rv = socket->Init();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return nullptr;
  }

  return socket.forget();
}

nsresult
TCPSocket::CreateInputStreamPump()
{
  if (!mSocketInputStream) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  nsresult rv;
  mInputStreamPump = do_CreateInstance("@mozilla.org/network/input-stream-pump;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mInputStreamPump->Init(mSocketInputStream, 0, 0, false, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  uint64_t suspendCount = mSuspendCount;
  while (suspendCount--) {
    mInputStreamPump->Suspend();
  }

  rv = mInputStreamPump->AsyncRead(this, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

NS_IMETHODIMP
TCPSocket::OnTransportStatus(nsITransport* aTransport, nsresult aStatus,
                             int64_t aProgress, int64_t aProgressMax)
{
  if (static_cast<uint32_t>(aStatus) != nsISocketTransport::STATUS_CONNECTED_TO) {
    return NS_OK;
  }

  mReadyState = TCPReadyState::Open;
  nsresult rv = CreateInputStreamPump();
  NS_ENSURE_SUCCESS(rv, rv);
  FireEvent(NS_LITERAL_STRING("open"));

  return NS_OK;
}

NS_IMETHODIMP
TCPSocket::OnInputStreamReady(nsIAsyncInputStream* aStream)
{
  // Only used for detecting if the connection was refused.

  uint64_t dummy;
  nsresult rv = aStream->Available(&dummy);
  if (NS_FAILED(rv)) {
    MaybeReportErrorAndCloseIfOpen(NS_ERROR_CONNECTION_REFUSED);
  }
  return NS_OK;
}

NS_IMETHODIMP
TCPSocket::OnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  return NS_OK;
}

NS_IMETHODIMP
TCPSocket::OnDataAvailable(nsIRequest* aRequest, nsISupports* aContext, nsIInputStream* aStream,
                           uint64_t aOffset, uint32_t aCount)
{
  if (mUseArrayBuffers) {
    nsTArray<uint8_t> buffer;
    buffer.SetCapacity(aCount);
    uint32_t actual;
    nsresult rv = aStream->Read(reinterpret_cast<char*>(buffer.Elements()), aCount, &actual);
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(actual == aCount);
    buffer.SetLength(actual);

    if (mSocketBridgeParent) {
      mSocketBridgeParent->FireArrayBufferDataEvent(buffer, mReadyState);
      return NS_OK;
    }

    AutoJSAPI api;
    if (!api.Init(GetOwnerGlobal())) {
      return NS_ERROR_FAILURE;
    }
    JSContext* cx = api.cx();

    JS::Rooted<JS::Value> value(cx);
    if (!ToJSValue(cx, TypedArrayCreator<ArrayBuffer>(buffer), &value)) {
      return NS_ERROR_FAILURE;
    }
    FireDataEvent(cx, NS_LITERAL_STRING("data"), value);
    return NS_OK;
  }

  nsCString data;
  nsresult rv = mInputStreamScriptable->ReadBytes(aCount, data);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mSocketBridgeParent) {
    mSocketBridgeParent->FireStringDataEvent(data, mReadyState);
    return NS_OK;
  }

  AutoJSAPI api;
  if (!api.Init(GetOwnerGlobal())) {
    return NS_ERROR_FAILURE;
  }
  JSContext* cx = api.cx();

  JS::Rooted<JS::Value> value(cx);
  if (!ToJSValue(cx, NS_ConvertASCIItoUTF16(data), &value)) {
    return NS_ERROR_FAILURE;
  }
  FireDataEvent(cx, NS_LITERAL_STRING("data"), value);

  return NS_OK;
}

NS_IMETHODIMP
TCPSocket::OnStopRequest(nsIRequest* aRequest, nsISupports* aContext, nsresult aStatus)
{
  mInputStreamPump = nullptr;

  if (mAsyncCopierActive && NS_SUCCEEDED(aStatus)) {
    // If we have some buffered output still, and status is not an
    // error, the other side has done a half-close, but we don't
    // want to be in the close state until we are done sending
    // everything that was buffered. We also don't want to call onclose
    // yet.
    return NS_OK;
  }

  // We call this even if there is no error.
  MaybeReportErrorAndCloseIfOpen(aStatus);
  return NS_OK;
}

void
TCPSocket::SetSocketBridgeParent(TCPSocketParent* aBridgeParent)
{
  mSocketBridgeParent = aBridgeParent;
}

NS_IMETHODIMP
TCPSocket::UpdateReadyState(uint32_t aReadyState)
{
  MOZ_ASSERT(mSocketBridgeChild);
  mReadyState = static_cast<TCPReadyState>(aReadyState);
  return NS_OK;
}

NS_IMETHODIMP
TCPSocket::UpdateBufferedAmount(uint32_t aBufferedAmount, uint32_t aTrackingNumber)
{
  if (aTrackingNumber != mTrackingNumber) {
    return NS_OK;
  }
  mBufferedAmount = aBufferedAmount;
  if (!mBufferedAmount) {
    if (mWaitingForDrain) {
      mWaitingForDrain = false;
      return FireEvent(NS_LITERAL_STRING("drain"));
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
TCPSocket::Observe(nsISupports* aSubject, const char* aTopic, const char16_t* aData)
{
  if (!strcmp(aTopic, "inner-window-destroyed")) {
    nsCOMPtr<nsISupportsPRUint64> wrapper = do_QueryInterface(aSubject);
    NS_ENSURE_TRUE(wrapper, NS_ERROR_FAILURE);
    uint64_t innerID;
    nsresult rv = wrapper->GetData(&innerID);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (innerID == mInnerWindowID) {
      Close();
    }
  } else if (!strcmp(aTopic, "profile-change-net-teardown")) {
    Close();
  }

  return NS_OK;
}

/* static */
bool
TCPSocket::ShouldTCPSocketExist(JSContext* aCx, JSObject* aGlobal)
{
  JS::Rooted<JSObject*> global(aCx, aGlobal);
  return nsContentUtils::IsSystemPrincipal(nsContentUtils::ObjectPrincipal(global));
}
