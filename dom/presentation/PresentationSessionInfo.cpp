/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "PresentationService.h"
#include "PresentationSessionInfo.h"

using namespace mozilla;
using namespace mozilla::dom;

/*
 * Implementation of PresentationSessionInfo
 */

NS_IMPL_ISUPPORTS(PresentationSessionInfo,
                  nsIPresentationSessionTransportCallback,
                  nsIPresentationControlChannelListener);

/* virtual */ nsresult
PresentationSessionInfo::Init(nsIPresentationControlChannel* aControlChannel)
{
  SetControlChannel(aControlChannel);
  return NS_OK;
}

/* virtual */ void
PresentationSessionInfo::Shutdown(nsresult aReason)
{
  // Close the control channel if any.
  if (mControlChannel) {
    mControlChannel->SetListener(nullptr);
    NS_WARN_IF(NS_FAILED(mControlChannel->Close(aReason)));
    mControlChannel = nullptr;
  }

  // Close the data transport channel if any.
  if (mTransport) {
    mTransport->SetCallback(nullptr);
    NS_WARN_IF(NS_FAILED(mTransport->Close(aReason)));
    mTransport = nullptr;
  }

  mIsResponderReady = false;
  mIsTransportReady = false;
}

nsresult
PresentationSessionInfo::SetListener(nsIPresentationSessionListener* aListener)
{
  mListener = aListener;

  if (mListener) {
    // The transport might become ready, or might become un-ready again, before
    // the listener has registered. So notify the listener of the state change.
    uint16_t state = IsSessionReady() ?
                     nsIPresentationSessionListener::STATE_CONNECTED :
                     nsIPresentationSessionListener::STATE_DISCONNECTED;
    return mListener->NotifyStateChange(mSessionId, state);
  }

  return NS_OK;
}

nsresult
PresentationSessionInfo::Send(nsIInputStream* aData)
{
  // TODO Send data to |mTransport|.
  return NS_OK;
}

nsresult
PresentationSessionInfo::Close(nsresult aReason)
{
  // TODO Close |mTransport|.
  return NS_OK;
}

nsresult
PresentationSessionInfo::ReplySuccess()
{
  if (mListener) {
    // Notify session state change.
    nsresult rv = mListener->NotifyStateChange(mSessionId,
                                               nsIPresentationSessionListener::STATE_CONNECTED);
    NS_WARN_IF(NS_FAILED(rv));
  }

  if (mCallback) {
    NS_WARN_IF(NS_FAILED(mCallback->NotifySuccess()));
    SetCallback(nullptr);
  }

  return NS_OK;
}

nsresult
PresentationSessionInfo::ReplyError(nsresult aError)
{
  Shutdown(aError);

  if (mCallback) {
    NS_WARN_IF(NS_FAILED(mCallback->NotifyError(aError)));
    SetCallback(nullptr);
  }

  // Remove itself since it never succeeds.
  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!service)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  static_cast<PresentationService*>(service.get())->RemoveSessionInfo(mSessionId);

  return NS_OK;
}

// nsIPresentationSessionTransportCallback
NS_IMETHODIMP
PresentationSessionInfo::NotifyTransportReady()
{
  MOZ_ASSERT(NS_IsMainThread());

  mIsTransportReady = true;

  // At sender side, session might not be ready at this point (waiting for
  // receiver's answer). Yet at receiver side, session must be ready at this
  // point since the data transport channel is created after the receiver page
  // is ready for presentation use.
  if (IsSessionReady()) {
    return ReplySuccess();
  }

  return NS_OK;
}

NS_IMETHODIMP
PresentationSessionInfo::NotifyTransportClosed(nsresult aReason)
{
  MOZ_ASSERT(NS_IsMainThread());

  mTransport = nullptr;

  if (!IsSessionReady()) {
    // It happens before the session is ready. Reply the callback.
    return ReplyError(aReason);
  }

  Shutdown(aReason);

  if (mListener) {
    // It happens after the session is ready. Notify session state change.
    uint16_t state = (NS_WARN_IF(NS_FAILED(aReason))) ?
                     nsIPresentationSessionListener::STATE_DISCONNECTED :
                     nsIPresentationSessionListener::STATE_TERMINATED;
    return mListener->NotifyStateChange(mSessionId, state);
  }

  return NS_OK;
}

NS_IMETHODIMP
PresentationSessionInfo::NotifyData(const nsACString& aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  // TODO Notify the listener.

  return NS_OK;
}

/*
 * Implementation of PresentationRequesterInfo
 *
 * During presentation session establishment, the sender expects the following
 * after trying to establish the control channel: (The order between step 2 and
 * 3 is not guaranteed.)
 * 1. |Init| is called to open a socket |mServerSocket| for data transport
 *    channel and send the offer to the receiver via the control channel.
 * 2.1 |OnSocketAccepted| of |nsIServerSocketListener| is called to indicate the
 *     data transport channel is connected. Then initialize |mTransport|.
 * 2.2 |NotifyTransportReady| of |nsIPresentationSessionTransportCallback| is
 *     called.
 * 3. |OnAnswer| of |nsIPresentationControlChannelListener| is called to
 *    indicate the receiver is ready. Close the control channel since it's no
 *    longer needed.
 * 4. Once both step 2 and 3 are done, the presentation session is ready to use.
 *    So notify the listener of CONNECTED state.
 */

NS_IMPL_ISUPPORTS_INHERITED(PresentationRequesterInfo,
                            PresentationSessionInfo,
                            nsIServerSocketListener)

nsresult
PresentationRequesterInfo::Init(nsIPresentationControlChannel* aControlChannel)
{
  PresentationSessionInfo::Init(aControlChannel);

  // TODO Initialize |mServerSocket|, use |this| as the listener, and prepare to
  // send offer.

  return NS_OK;
}

void
PresentationRequesterInfo::Shutdown(nsresult aReason)
{
  PresentationSessionInfo::Shutdown(aReason);

  // Close the server socket if any.
  if (mServerSocket) {
    NS_WARN_IF(NS_FAILED(mServerSocket->Close()));
    mServerSocket = nullptr;
  }
}

// nsIPresentationControlChannelListener
NS_IMETHODIMP
PresentationRequesterInfo::OnOffer(nsIPresentationChannelDescription* aDescription)
{
  MOZ_ASSERT(false, "Sender side should not receive offer.");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PresentationRequesterInfo::OnAnswer(nsIPresentationChannelDescription* aDescription)
{
  // Close the control channel since it's no longer needed.
  nsresult rv = mControlChannel->Close(NS_OK);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return ReplyError(rv);
  }

  // Session might not be ready at this moment (waiting for the establishment of
  // the data transport channel).
  if (IsSessionReady()){
    return ReplySuccess();
  }

  return NS_OK;
}

NS_IMETHODIMP
PresentationRequesterInfo::NotifyOpened()
{
  // Do nothing and wait for receiver to be ready.
  return NS_OK;
}

NS_IMETHODIMP
PresentationRequesterInfo::NotifyClosed(nsresult aReason)
{
  MOZ_ASSERT(NS_IsMainThread());

  SetControlChannel(nullptr);

  if (NS_WARN_IF(NS_FAILED(aReason))) {
    // Reply error for an abnormal close.
    return ReplyError(aReason);
  }

  return NS_OK;
}

// nsIServerSocketListener
NS_IMETHODIMP
PresentationRequesterInfo::OnSocketAccepted(nsIServerSocket* aServerSocket,
                                            nsISocketTransport* aTransport)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Initialize |mTransport| and use |this| as the callback.
  mTransport = do_CreateInstance(PRESENTATION_SESSION_TRANSPORT_CONTRACTID);
  if (NS_WARN_IF(!mTransport)) {
    return ReplyError(NS_ERROR_NOT_AVAILABLE);
  }

  nsresult rv = mTransport->InitWithSocketTransport(aTransport, this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
PresentationRequesterInfo::OnStopListening(nsIServerSocket* aServerSocket,
                                           nsresult aStatus)
{
  MOZ_ASSERT(NS_IsMainThread());

  Shutdown(aStatus);

  if (!IsSessionReady()) {
    // It happens before the session is ready. Reply the callback.
    return ReplyError(aStatus);
  }

  // It happens after the session is ready. Notify session state change.
  if (mListener) {
    return mListener->NotifyStateChange(mSessionId,
                                        nsIPresentationSessionListener::STATE_DISCONNECTED);
  }

  return NS_OK;
}

/*
 * Implementation of PresentationResponderInfo
 */

NS_IMPL_ISUPPORTS_INHERITED0(PresentationResponderInfo,
                             PresentationSessionInfo)

// nsIPresentationControlChannelListener
NS_IMETHODIMP
PresentationResponderInfo::OnOffer(nsIPresentationChannelDescription* aDescription)
{
  // TODO Initialize |mTransport| and use |this| as the callback.
  return NS_OK;
}

NS_IMETHODIMP
PresentationResponderInfo::OnAnswer(nsIPresentationChannelDescription* aDescription)
{
  MOZ_ASSERT(false, "Receiver side should not receive answer.");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PresentationResponderInfo::NotifyOpened()
{
  // Do nothing.
  return NS_OK;
}

NS_IMETHODIMP
PresentationResponderInfo::NotifyClosed(nsresult aReason)
{
  MOZ_ASSERT(NS_IsMainThread());

  SetControlChannel(nullptr);

  if (NS_WARN_IF(NS_FAILED(aReason))) {
    // TODO Notify session failure.
  }

  return NS_OK;
}
