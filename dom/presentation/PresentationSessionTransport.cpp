/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsArrayUtils.h"
#include "nsIAsyncStreamCopier.h"
#include "nsIInputStreamPump.h"
#include "nsIMultiplexInputStream.h"
#include "nsIMutableArray.h"
#include "nsIOutputStream.h"
#include "nsIPresentationControlChannel.h"
#include "nsIScriptableInputStream.h"
#include "nsISocketTransport.h"
#include "nsISocketTransportService.h"
#include "nsISupportsPrimitives.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"
#include "PresentationSessionTransport.h"

#define BUFFER_SIZE 65536

using namespace mozilla;
using namespace mozilla::dom;

class CopierCallbacks final : public nsIRequestObserver
{
public:
  explicit CopierCallbacks(PresentationSessionTransport* aTransport)
    : mOwner(aTransport)
  {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
private:
  ~CopierCallbacks() {}

  nsRefPtr<PresentationSessionTransport> mOwner;
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
  return NS_OK;
}

NS_IMPL_ISUPPORTS(PresentationSessionTransport,
                  nsIPresentationSessionTransport,
                  nsITransportEventSink,
                  nsIInputStreamCallback,
                  nsIStreamListener,
                  nsIRequestObserver)

PresentationSessionTransport::PresentationSessionTransport()
  : mReadyState(CLOSED)
  , mAsyncCopierActive(false)
  , mCloseStatus(NS_OK)
  , mDataNotificationEnabled(false)
{
}

PresentationSessionTransport::~PresentationSessionTransport()
{
}

NS_IMETHODIMP
PresentationSessionTransport::InitWithSocketTransport(nsISocketTransport* aTransport,
                                                      nsIPresentationSessionTransportCallback* aCallback)
{
  if (NS_WARN_IF(!aCallback)) {
    return NS_ERROR_INVALID_ARG;
  }
  mCallback = aCallback;

  if (NS_WARN_IF(!aTransport)) {
    return NS_ERROR_INVALID_ARG;
  }
  mTransport = aTransport;

  nsresult rv = CreateStream();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  SetReadyState(OPEN);

  if (IsReadyToNotifyData()) {
    return CreateInputStreamPump();
  }

  return NS_OK;
}

NS_IMETHODIMP
PresentationSessionTransport::InitWithChannelDescription(nsIPresentationChannelDescription* aDescription,
                                                         nsIPresentationSessionTransportCallback* aCallback)
{
  if (NS_WARN_IF(!aCallback)) {
    return NS_ERROR_INVALID_ARG;
  }
  mCallback = aCallback;

  if (NS_WARN_IF(!aDescription)) {
    return NS_ERROR_INVALID_ARG;
  }

  uint16_t serverPort;
  nsresult rv = aDescription->GetTcpPort(&serverPort);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIArray> serverHosts;
  rv = aDescription->GetTcpAddress(getter_AddRefs(serverHosts));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // TODO bug 1148307 Implement PresentationSessionTransport with DataChannel.
  // Ultimately we may use all the available addresses. DataChannel appears
  // more robust upon handling ICE. And at the first stage Presentation API is
  // only exposed on Firefox OS where the first IP appears enough for most
  // scenarios.
  nsCOMPtr<nsISupportsCString> supportStr = do_QueryElementAt(serverHosts, 0);
  if (NS_WARN_IF(!supportStr)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoCString serverHost;
  supportStr->GetData(serverHost);
  if (serverHost.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  SetReadyState(CONNECTING);

  nsCOMPtr<nsISocketTransportService> sts =
    do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID);
  if (NS_WARN_IF(!sts)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  rv = sts->CreateTransport(nullptr, 0, serverHost, serverPort, nullptr,
                            getter_AddRefs(mTransport));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIThread> mainThread;
  NS_GetMainThread(getter_AddRefs(mainThread));

  mTransport->SetEventSink(this, mainThread);

  rv = CreateStream();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
PresentationSessionTransport::CreateStream()
{
  nsresult rv = mTransport->OpenInputStream(0, 0, 0, getter_AddRefs(mSocketInputStream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  rv = mTransport->OpenOutputStream(nsITransport::OPEN_UNBUFFERED, 0, 0, getter_AddRefs(mSocketOutputStream));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // If the other side is not listening, we will get an |onInputStreamReady|
  // callback where |available| raises to indicate the connection was refused.
  nsCOMPtr<nsIAsyncInputStream> asyncStream = do_QueryInterface(mSocketInputStream);
  if (NS_WARN_IF(!asyncStream)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIThread> mainThread;
  NS_GetMainThread(getter_AddRefs(mainThread));

  rv = asyncStream->AsyncWait(this, nsIAsyncInputStream::WAIT_CLOSURE_ONLY, 0, mainThread);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mInputStreamScriptable = do_CreateInstance("@mozilla.org/scriptableinputstream;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  rv = mInputStreamScriptable->Init(mSocketInputStream);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mMultiplexStream = do_CreateInstance("@mozilla.org/io/multiplex-input-stream;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mMultiplexStreamCopier = do_CreateInstance("@mozilla.org/network/async-stream-copier;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsISocketTransportService> sts =
    do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID);
  if (NS_WARN_IF(!sts)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIEventTarget> target = do_QueryInterface(sts);
  rv = mMultiplexStreamCopier->Init(mMultiplexStream,
                                    mSocketOutputStream,
                                    target,
                                    true, /* source buffered */
                                    false, /* sink buffered */
                                    BUFFER_SIZE,
                                    false, /* close source */
                                    false); /* close sink */
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
PresentationSessionTransport::CreateInputStreamPump()
{
  nsresult rv;
  mInputStreamPump = do_CreateInstance(NS_INPUTSTREAMPUMP_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mInputStreamPump->Init(mSocketInputStream, -1, -1, 0, 0, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mInputStreamPump->AsyncRead(this, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
PresentationSessionTransport::EnableDataNotification()
{
  if (NS_WARN_IF(!mCallback)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  if (mDataNotificationEnabled) {
    return NS_OK;
  }

  mDataNotificationEnabled = true;

  if (IsReadyToNotifyData()) {
    return CreateInputStreamPump();
  }

  return NS_OK;
}

NS_IMETHODIMP
PresentationSessionTransport::GetCallback(nsIPresentationSessionTransportCallback** aCallback)
{
  nsCOMPtr<nsIPresentationSessionTransportCallback> callback = mCallback;
  callback.forget(aCallback);
  return NS_OK;
}

NS_IMETHODIMP
PresentationSessionTransport::SetCallback(nsIPresentationSessionTransportCallback* aCallback)
{
  mCallback = aCallback;
  return NS_OK;
}

NS_IMETHODIMP
PresentationSessionTransport::GetSelfAddress(nsINetAddr** aSelfAddress)
{
  if (NS_WARN_IF(!mTransport)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  return mTransport->GetScriptableSelfAddr(aSelfAddress);
}

void
PresentationSessionTransport::EnsureCopying()
{
  if (mAsyncCopierActive) {
    return;
  }

  mAsyncCopierActive = true;
  nsRefPtr<CopierCallbacks> callbacks = new CopierCallbacks(this);
  NS_WARN_IF(NS_FAILED(mMultiplexStreamCopier->AsyncCopy(callbacks, nullptr)));
}

void
PresentationSessionTransport::NotifyCopyComplete(nsresult aStatus)
{
  mAsyncCopierActive = false;
  mMultiplexStream->RemoveStream(0);
  if (NS_WARN_IF(NS_FAILED(aStatus))) {
    if (mReadyState != CLOSED) {
      mCloseStatus = aStatus;
      SetReadyState(CLOSED);
    }
    return;
  }

  uint32_t count;
  nsresult rv = mMultiplexStream->GetCount(&count);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  if (count) {
    EnsureCopying();
    return;
  }

  if (mReadyState == CLOSING) {
    mSocketOutputStream->Close();
    mCloseStatus = NS_OK;
    SetReadyState(CLOSED);
  }
}

NS_IMETHODIMP
PresentationSessionTransport::Send(nsIInputStream* aData)
{
  if (NS_WARN_IF(mReadyState != OPEN)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  mMultiplexStream->AppendStream(aData);

  EnsureCopying();

  return NS_OK;
}

NS_IMETHODIMP
PresentationSessionTransport::Close(nsresult aReason)
{
  if (mReadyState == CLOSED || mReadyState == CLOSING) {
    return NS_OK;
  }

  mCloseStatus = aReason;
  SetReadyState(CLOSING);

  uint32_t count = 0;
  mMultiplexStream->GetCount(&count);
  if (!count) {
    mSocketOutputStream->Close();
  }

  mSocketInputStream->Close();
  mDataNotificationEnabled = false;

  return NS_OK;
}

void
PresentationSessionTransport::SetReadyState(ReadyState aReadyState)
{
  mReadyState = aReadyState;

  if (mReadyState == OPEN && mCallback) {
    // Notify the transport channel is ready.
    NS_WARN_IF(NS_FAILED(mCallback->NotifyTransportReady()));
  } else if (mReadyState == CLOSED && mCallback) {
    // Notify the transport channel has been shut down.
    NS_WARN_IF(NS_FAILED(mCallback->NotifyTransportClosed(mCloseStatus)));
  }
}

// nsITransportEventSink
NS_IMETHODIMP
PresentationSessionTransport::OnTransportStatus(nsITransport* aTransport,
                                                nsresult aStatus,
                                                int64_t aProgress,
                                                int64_t aProgressMax)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aStatus != NS_NET_STATUS_CONNECTED_TO) {
    return NS_OK;
  }

  SetReadyState(OPEN);

  if (IsReadyToNotifyData()) {
    return CreateInputStreamPump();
  }

  return NS_OK;
}

// nsIInputStreamCallback
NS_IMETHODIMP
PresentationSessionTransport::OnInputStreamReady(nsIAsyncInputStream* aStream)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Only used for detecting if the connection was refused.
  uint64_t dummy;
  nsresult rv = aStream->Available(&dummy);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    if (mReadyState != CLOSED) {
      mCloseStatus = NS_ERROR_CONNECTION_REFUSED;
      SetReadyState(CLOSED);
    }
  }

  return NS_OK;
}

// nsIRequestObserver
NS_IMETHODIMP
PresentationSessionTransport::OnStartRequest(nsIRequest* aRequest,
                                             nsISupports* aContext)
{
  // Do nothing.
  return NS_OK;
}

NS_IMETHODIMP
PresentationSessionTransport::OnStopRequest(nsIRequest* aRequest,
                                            nsISupports* aContext,
                                            nsresult aStatusCode)
{
  MOZ_ASSERT(NS_IsMainThread());

  uint32_t count;
  nsresult rv = mMultiplexStream->GetCount(&count);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mInputStreamPump = nullptr;

  if (count != 0 && NS_SUCCEEDED(aStatusCode)) {
    // If we have some buffered output still, and status is not an error, the
    // other side has done a half-close, but we don't want to be in the close
    // state until we are done sending everything that was buffered. We also
    // don't want to call |NotifyTransportClosed| yet.
    return NS_OK;
  }

  // We call this even if there is no error.
  if (mReadyState != CLOSED) {
    mCloseStatus = aStatusCode;
    SetReadyState(CLOSED);
  }
  return NS_OK;
}

// nsIStreamListener
NS_IMETHODIMP
PresentationSessionTransport::OnDataAvailable(nsIRequest* aRequest,
                                              nsISupports* aContext,
                                              nsIInputStream* aStream,
                                              uint64_t aOffset,
                                              uint32_t aCount)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!mCallback)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCString data;
  nsresult rv = mInputStreamScriptable->ReadBytes(aCount, data);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Pass the incoming data to the listener.
  return mCallback->NotifyData(data);
}
