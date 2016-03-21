/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/HTMLIFrameElementBinding.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/Function.h"
#include "mozilla/Logging.h"
#include "mozilla/Move.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "nsContentUtils.h"
#include "nsIDocShell.h"
#include "nsIFrameLoader.h"
#include "nsIMutableArray.h"
#include "nsINetAddr.h"
#include "nsISocketTransport.h"
#include "nsISupportsPrimitives.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "PresentationLog.h"
#include "PresentationService.h"
#include "PresentationSessionInfo.h"

#ifdef MOZ_WIDGET_ANDROID
#include "nsIPresentationNetworkHelper.h"
#endif // MOZ_WIDGET_ANDROID

#ifdef MOZ_WIDGET_GONK
#include "nsINetworkInterface.h"
#include "nsINetworkManager.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::services;

/*
 * Implementation of PresentationChannelDescription
 */

namespace mozilla {
namespace dom {

#ifdef MOZ_WIDGET_ANDROID

namespace {

class PresentationNetworkHelper final : public nsIPresentationNetworkHelperListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONNETWORKHELPERLISTENER

  using Function = nsresult(PresentationControllingInfo::*)(const nsACString&);

  explicit PresentationNetworkHelper(PresentationControllingInfo* aInfo,
                                     const Function& aFunc);

  nsresult GetWifiIPAddress();

private:
  ~PresentationNetworkHelper() = default;

  RefPtr<PresentationControllingInfo> mInfo;
  Function mFunc;
};

NS_IMPL_ISUPPORTS(PresentationNetworkHelper,
                  nsIPresentationNetworkHelperListener)

PresentationNetworkHelper::PresentationNetworkHelper(PresentationControllingInfo* aInfo,
                                                     const Function& aFunc)
  : mInfo(aInfo)
  , mFunc(aFunc)
{
  MOZ_ASSERT(aInfo);
  MOZ_ASSERT(aFunc);
}

nsresult
PresentationNetworkHelper::GetWifiIPAddress()
{
  nsresult rv;

  nsCOMPtr<nsIPresentationNetworkHelper> networkHelper =
    do_GetService(PRESENTATION_NETWORK_HELPER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return networkHelper->GetWifiIPAddress(this);
}

NS_IMETHODIMP
PresentationNetworkHelper::OnError(const nsACString & aReason)
{
  PRES_ERROR("PresentationNetworkHelper::OnError: %s",
    nsPromiseFlatCString(aReason).get());
  return NS_OK;
}

NS_IMETHODIMP
PresentationNetworkHelper::OnGetWifiIPAddress(const nsACString& aIPAddress)
{
  MOZ_ASSERT(mInfo);
  MOZ_ASSERT(mFunc);

  NS_DispatchToMainThread(
    NS_NewRunnableMethodWithArg<nsCString>(mInfo,
                                           mFunc,
                                           aIPAddress));
  return NS_OK;
}

} // anonymous namespace

#endif // MOZ_WIDGET_ANDROID

class PresentationChannelDescription final : public nsIPresentationChannelDescription
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONCHANNELDESCRIPTION

  PresentationChannelDescription(const nsACString& aAddress,
                                 uint16_t aPort)
    : mAddress(aAddress)
    , mPort(aPort)
  {
  }

private:
  ~PresentationChannelDescription() {}

  nsCString mAddress;
  uint16_t mPort;
};

} // namespace dom
} // namespace mozilla

NS_IMPL_ISUPPORTS(PresentationChannelDescription, nsIPresentationChannelDescription)

NS_IMETHODIMP
PresentationChannelDescription::GetType(uint8_t* aRetVal)
{
  if (NS_WARN_IF(!aRetVal)) {
    return NS_ERROR_INVALID_POINTER;
  }

  // TODO bug 1148307 Implement PresentationSessionTransport with DataChannel.
  // Only support TCP socket for now.
  *aRetVal = nsIPresentationChannelDescription::TYPE_TCP;
  return NS_OK;
}

NS_IMETHODIMP
PresentationChannelDescription::GetTcpAddress(nsIArray** aRetVal)
{
  if (NS_WARN_IF(!aRetVal)) {
    return NS_ERROR_INVALID_POINTER;
  }

  nsCOMPtr<nsIMutableArray> array = do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (NS_WARN_IF(!array)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // TODO bug 1148307 Implement PresentationSessionTransport with DataChannel.
  // Ultimately we may use all the available addresses. DataChannel appears
  // more robust upon handling ICE. And at the first stage Presentation API is
  // only exposed on Firefox OS where the first IP appears enough for most
  // scenarios.
  nsCOMPtr<nsISupportsCString> address = do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID);
  if (NS_WARN_IF(!address)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  address->SetData(mAddress);

  array->AppendElement(address, false);
  array.forget(aRetVal);

  return NS_OK;
}

NS_IMETHODIMP
PresentationChannelDescription::GetTcpPort(uint16_t* aRetVal)
{
  if (NS_WARN_IF(!aRetVal)) {
    return NS_ERROR_INVALID_POINTER;
  }

  *aRetVal = mPort;
  return NS_OK;
}

NS_IMETHODIMP
PresentationChannelDescription::GetDataChannelSDP(nsAString& aDataChannelSDP)
{
  // TODO bug 1148307 Implement PresentationSessionTransport with DataChannel.
  // Only support TCP socket for now.
  aDataChannelSDP.Truncate();
  return NS_OK;
}

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
  NS_WARN_IF(NS_FAILED(aReason));

  // Close the control channel if any.
  if (mControlChannel) {
    NS_WARN_IF(NS_FAILED(mControlChannel->Close(aReason)));
  }

  // Close the data transport channel if any.
  if (mTransport) {
    // |mIsTransportReady| will be unset once |NotifyTransportClosed| is called.
    NS_WARN_IF(NS_FAILED(mTransport->Close(aReason)));
  }

  mIsResponderReady = false;
}

nsresult
PresentationSessionInfo::SetListener(nsIPresentationSessionListener* aListener)
{
  mListener = aListener;

  if (mListener) {
    // Enable data notification for the transport channel if it's available.
    if (mTransport) {
      nsresult rv = mTransport->EnableDataNotification();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }

    // The transport might become ready, or might become un-ready again, before
    // the listener has registered. So notify the listener of the state change.
    return mListener->NotifyStateChange(mSessionId, mState);
  }

  return NS_OK;
}

nsresult
PresentationSessionInfo::Send(nsIInputStream* aData)
{
  if (NS_WARN_IF(!IsSessionReady())) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  if (NS_WARN_IF(!mTransport)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return mTransport->Send(aData);
}

nsresult
PresentationSessionInfo::Close(nsresult aReason,
                               uint32_t aState)
{
  if (NS_WARN_IF(!IsSessionReady())) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  SetState(aState);

  Shutdown(aReason);
  return NS_OK;
}

nsresult
PresentationSessionInfo::ReplySuccess()
{
  SetState(nsIPresentationSessionListener::STATE_CONNECTED);

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
  return UntrackFromService();
}

/* virtual */ nsresult
PresentationSessionInfo::UntrackFromService()
{
  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!service)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  static_cast<PresentationService*>(service.get())->UntrackSessionInfo(mSessionId, mRole);

  return NS_OK;
}

/* virtual */ bool
PresentationSessionInfo::IsAccessible(base::ProcessId aProcessId)
{
  // No restriction by default.
  return true;
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

  // Nullify |mTransport| here so it won't try to re-close |mTransport| in
  // potential subsequent |Shutdown| calls.
  mTransport = nullptr;

  if (NS_WARN_IF(!IsSessionReady())) {
    // It happens before the session is ready. Reply the callback.
    return ReplyError(NS_ERROR_DOM_OPERATION_ERR);
  }

  // Unset |mIsTransportReady| here so it won't affect |IsSessionReady()| above.
  mIsTransportReady = false;

  if (mState == nsIPresentationSessionListener::STATE_CONNECTED) {
    // The transport channel is closed unexpectedly (not caused by a |Close| call).
    SetState(nsIPresentationSessionListener::STATE_CLOSED);
  }

  Shutdown(aReason);

  if (mState == nsIPresentationSessionListener::STATE_TERMINATED) {
    // Directly untrack the session info from the service.
    return UntrackFromService();
  }

  return NS_OK;
}

NS_IMETHODIMP
PresentationSessionInfo::NotifyData(const nsACString& aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!IsSessionReady())) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  if (NS_WARN_IF(!mListener)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return mListener->NotifyMessage(mSessionId, aData);
}

/*
 * Implementation of PresentationControllingInfo
 *
 * During presentation session establishment, the sender expects the following
 * after trying to establish the control channel: (The order between step 3 and
 * 4 is not guaranteed.)
 * 1. |Init| is called to open a socket |mServerSocket| for data transport
 *    channel.
 * 2. |NotifyOpened| of |nsIPresentationControlChannelListener| is called to
 *    indicate the control channel is ready to use. Then send the offer to the
 *    receiver via the control channel.
 * 3.1 |OnSocketAccepted| of |nsIServerSocketListener| is called to indicate the
 *     data transport channel is connected. Then initialize |mTransport|.
 * 3.2 |NotifyTransportReady| of |nsIPresentationSessionTransportCallback| is
 *     called.
 * 4. |OnAnswer| of |nsIPresentationControlChannelListener| is called to
 *    indicate the receiver is ready. Close the control channel since it's no
 *    longer needed.
 * 5. Once both step 3 and 4 are done, the presentation session is ready to use.
 *    So notify the listener of CONNECTED state.
 */

NS_IMPL_ISUPPORTS_INHERITED(PresentationControllingInfo,
                            PresentationSessionInfo,
                            nsIServerSocketListener)

nsresult
PresentationControllingInfo::Init(nsIPresentationControlChannel* aControlChannel)
{
  PresentationSessionInfo::Init(aControlChannel);

  // Initialize |mServerSocket| for bootstrapping the data transport channel and
  // use |this| as the listener.
  mServerSocket = do_CreateInstance(NS_SERVERSOCKET_CONTRACTID);
  if (NS_WARN_IF(!mServerSocket)) {
    return ReplyError(NS_ERROR_DOM_OPERATION_ERR);
  }

  nsresult rv = mServerSocket->Init(-1, false, -1);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mServerSocket->AsyncListen(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  int32_t port;
  rv = mServerSocket->GetPort(&port);
  if (!NS_WARN_IF(NS_FAILED(rv))) {
    PRES_DEBUG("%s:ServerSocket created.port[%d]\n",__func__, port);
  }

  return NS_OK;
}

void
PresentationControllingInfo::Shutdown(nsresult aReason)
{
  PresentationSessionInfo::Shutdown(aReason);

  // Close the server socket if any.
  if (mServerSocket) {
    NS_WARN_IF(NS_FAILED(mServerSocket->Close()));
    mServerSocket = nullptr;
  }
}

nsresult
PresentationControllingInfo::GetAddress()
{
#if defined(MOZ_WIDGET_GONK)
  nsCOMPtr<nsINetworkManager> networkManager =
    do_GetService("@mozilla.org/network/manager;1");
  if (NS_WARN_IF(!networkManager)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsINetworkInfo> activeNetworkInfo;
  networkManager->GetActiveNetworkInfo(getter_AddRefs(activeNetworkInfo));
  if (NS_WARN_IF(!activeNetworkInfo)) {
    return NS_ERROR_FAILURE;
  }

  char16_t** ips = nullptr;
  uint32_t* prefixes = nullptr;
  uint32_t count = 0;
  activeNetworkInfo->GetAddresses(&ips, &prefixes, &count);
  if (NS_WARN_IF(!count)) {
    NS_Free(prefixes);
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, ips);
    return NS_ERROR_FAILURE;
  }

  // TODO bug 1148307 Implement PresentationSessionTransport with DataChannel.
  // Ultimately we may use all the available addresses. DataChannel appears
  // more robust upon handling ICE. And at the first stage Presentation API is
  // only exposed on Firefox OS where the first IP appears enough for most
  // scenarios.
  nsAutoString ip;
  ip.Assign(ips[0]);

  // On Android platform, the IP address is retrieved from a callback function.
  // To make consistent code sequence, following function call is dispatched
  // into main thread instead of calling it directly.
  NS_DispatchToMainThread(
    NS_NewRunnableMethodWithArg<nsCString>(
      this,
      &PresentationControllingInfo::OnGetAddress,
      NS_ConvertUTF16toUTF8(ip)));

  NS_Free(prefixes);
  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, ips);

#elif defined(MOZ_WIDGET_ANDROID)
  RefPtr<PresentationNetworkHelper> networkHelper =
    new PresentationNetworkHelper(this,
                                  &PresentationControllingInfo::OnGetAddress);
  nsresult rv = networkHelper->GetWifiIPAddress();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#elif defined(MOZ_MULET)
  // In simulator,we need to use the "127.0.0.1" as target address.
  NS_DispatchToMainThread(
    NS_NewRunnableMethodWithArg<nsCString>(
      this,
      &PresentationControllingInfo::OnGetAddress,
      "127.0.0.1"));

#else
  // TODO Get host IP via other platforms.

  NS_DispatchToMainThread(
    NS_NewRunnableMethodWithArg<nsCString>(
      this,
      &PresentationControllingInfo::OnGetAddress,
      EmptyCString()));
#endif

  return NS_OK;
}

NS_IMETHODIMP
PresentationControllingInfo::OnIceCandidate(const nsAString& aCandidate)
{
  MOZ_ASSERT(false, "Should not receive ICE candidates.");
  return NS_ERROR_FAILURE;
}

nsresult
PresentationControllingInfo::OnGetAddress(const nsACString& aAddress)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Prepare and send the offer.
  int32_t port;
  nsresult rv = mServerSocket->GetPort(&port);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<PresentationChannelDescription> description =
    new PresentationChannelDescription(aAddress, static_cast<uint16_t>(port));
  return mControlChannel->SendOffer(description);
}

// nsIPresentationControlChannelListener
NS_IMETHODIMP
PresentationControllingInfo::OnOffer(nsIPresentationChannelDescription* aDescription)
{
  MOZ_ASSERT(false, "Sender side should not receive offer.");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PresentationControllingInfo::OnAnswer(nsIPresentationChannelDescription* aDescription)
{
  mIsResponderReady = true;

  // Close the control channel since it's no longer needed.
  nsresult rv = mControlChannel->Close(NS_OK);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return ReplyError(NS_ERROR_DOM_OPERATION_ERR);
  }

  // Session might not be ready at this moment (waiting for the establishment of
  // the data transport channel).
  if (IsSessionReady()){
    return ReplySuccess();
  }

  return NS_OK;
}

NS_IMETHODIMP
PresentationControllingInfo::NotifyOpened()
{
  MOZ_ASSERT(NS_IsMainThread());
  return GetAddress();
}

NS_IMETHODIMP
PresentationControllingInfo::NotifyClosed(nsresult aReason)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Unset control channel here so it won't try to re-close it in potential
  // subsequent |Shutdown| calls.
  SetControlChannel(nullptr);

  if (NS_WARN_IF(NS_FAILED(aReason) || !mIsResponderReady)) {
    // The presentation session instance may already exist.
    // Change the state to TERMINATED since it never succeeds.
    SetState(nsIPresentationSessionListener::STATE_TERMINATED);

    // Reply error for an abnormal close.
    return ReplyError(NS_ERROR_DOM_OPERATION_ERR);
  }

  return NS_OK;
}

// nsIServerSocketListener
NS_IMETHODIMP
PresentationControllingInfo::OnSocketAccepted(nsIServerSocket* aServerSocket,
                                            nsISocketTransport* aTransport)
{
  int32_t port;
  nsresult rv = aTransport->GetPort(&port);
  if (!NS_WARN_IF(NS_FAILED(rv))) {
    PRES_DEBUG("%s:receive from port[%d]\n",__func__, port);
  }

  MOZ_ASSERT(NS_IsMainThread());

  // Initialize |mTransport| and use |this| as the callback.
  mTransport = do_CreateInstance(PRESENTATION_SESSION_TRANSPORT_CONTRACTID);
  if (NS_WARN_IF(!mTransport)) {
    return ReplyError(NS_ERROR_DOM_OPERATION_ERR);
  }

  rv = mTransport->InitWithSocketTransport(aTransport, this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Enable data notification if the listener has been registered.
  if (mListener) {
    return mTransport->EnableDataNotification();
  }

  return NS_OK;
}

NS_IMETHODIMP
PresentationControllingInfo::OnStopListening(nsIServerSocket* aServerSocket,
                                           nsresult aStatus)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aStatus == NS_BINDING_ABORTED) { // The server socket was manually closed.
    return NS_OK;
  }

  Shutdown(aStatus);

  if (NS_WARN_IF(!IsSessionReady())) {
    // It happens before the session is ready. Reply the callback.
    return ReplyError(NS_ERROR_DOM_OPERATION_ERR);
  }

  // It happens after the session is ready. Change the state to CLOSED.
  SetState(nsIPresentationSessionListener::STATE_CLOSED);

  return NS_OK;
}

/*
 * Implementation of PresentationPresentingInfo
 *
 * During presentation session establishment, the receiver expects the following
 * after trying to launch the app by notifying "presentation-launch-receiver":
 * (The order between step 2 and 3 is not guaranteed.)
 * 1. |Observe| of |nsIObserver| is called with "presentation-receiver-launched".
 *    Then start listen to document |STATE_TRANSFERRING| event.
 * 2. |NotifyResponderReady| is called to indicate the receiver page is ready
 *    for presentation use.
 * 3. |OnOffer| of |nsIPresentationControlChannelListener| is called.
 * 4. Once both step 2 and 3 are done, establish the data transport channel and
 *    send the answer. (The control channel will be closed by the sender once it
 *    receives the answer.)
 * 5. |NotifyTransportReady| of |nsIPresentationSessionTransportCallback| is
 *    called. The presentation session is ready to use, so notify the listener
 *    of CONNECTED state.
 */

NS_IMPL_ISUPPORTS_INHERITED(PresentationPresentingInfo,
                            PresentationSessionInfo,
                            nsITimerCallback)

nsresult
PresentationPresentingInfo::Init(nsIPresentationControlChannel* aControlChannel)
{
  PresentationSessionInfo::Init(aControlChannel);

  // Add a timer to prevent waiting indefinitely in case the receiver page fails
  // to become ready.
  nsresult rv;
  int32_t timeout =
    Preferences::GetInt("presentation.receiver.loading.timeout", 10000);
  mTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  rv = mTimer->InitWithCallback(this, timeout, nsITimer::TYPE_ONE_SHOT);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void
PresentationPresentingInfo::Shutdown(nsresult aReason)
{
  PresentationSessionInfo::Shutdown(aReason);

  if (mTimer) {
    mTimer->Cancel();
  }

  mLoadingCallback = nullptr;
  mRequesterDescription = nullptr;
  mPromise = nullptr;
}

nsresult
PresentationPresentingInfo::InitTransportAndSendAnswer()
{
  // Establish a data transport channel |mTransport| to the sender and use
  // |this| as the callback.
  mTransport = do_CreateInstance(PRESENTATION_SESSION_TRANSPORT_CONTRACTID);
  if (NS_WARN_IF(!mTransport)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv = mTransport->InitWithChannelDescription(mRequesterDescription, this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Enable data notification if the listener has been registered.
  if (mListener) {
    rv = mTransport->EnableDataNotification();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  // Prepare and send the answer.
  // TODO bug 1148307 Implement PresentationSessionTransport with DataChannel.
  // In the current implementation of |PresentationSessionTransport|,
  // |GetSelfAddress| cannot return the real info when it's initialized via
  // |InitWithChannelDescription|. Yet this deficiency only affects the channel
  // description for the answer, which is not actually checked at requester side.
  nsCOMPtr<nsINetAddr> selfAddr;
  rv = mTransport->GetSelfAddress(getter_AddRefs(selfAddr));
  NS_WARN_IF(NS_FAILED(rv));

  nsCString address;
  uint16_t port = 0;
  if (NS_SUCCEEDED(rv)) {
    selfAddr->GetAddress(address);
    selfAddr->GetPort(&port);
  }
  nsCOMPtr<nsIPresentationChannelDescription> description =
    new PresentationChannelDescription(address, port);

  return mControlChannel->SendAnswer(description);
}

nsresult
PresentationPresentingInfo::UntrackFromService()
{
  // Remove the OOP responding info (if it has never been used).
  if (mContentParent) {
    NS_WARN_IF(!static_cast<ContentParent*>(mContentParent.get())->SendNotifyPresentationReceiverCleanUp(mSessionId));
  }

  // Remove the session info (and the in-process responding info if there's any).
  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!service)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  static_cast<PresentationService*>(service.get())->UntrackSessionInfo(mSessionId, mRole);

  return NS_OK;
}

bool
PresentationPresentingInfo::IsAccessible(base::ProcessId aProcessId)
{
  // Only the specific content process should access the responder info.
  return (mContentParent) ?
          aProcessId == static_cast<ContentParent*>(mContentParent.get())->OtherPid() :
          false;
}

nsresult
PresentationPresentingInfo::NotifyResponderReady()
{
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }

  mIsResponderReady = true;

  // Initialize |mTransport| and send the answer to the sender if sender's
  // description is already offered.
  if (mRequesterDescription) {
    nsresult rv = InitTransportAndSendAnswer();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return ReplyError(NS_ERROR_DOM_OPERATION_ERR);
    }
  }

  return NS_OK;
}

// nsIPresentationControlChannelListener
NS_IMETHODIMP
PresentationPresentingInfo::OnOffer(nsIPresentationChannelDescription* aDescription)
{
  if (NS_WARN_IF(!aDescription)) {
    return ReplyError(NS_ERROR_DOM_OPERATION_ERR);
  }

  mRequesterDescription = aDescription;

  // Initialize |mTransport| and send the answer to the sender if the receiver
  // page is ready for presentation use.
  if (mIsResponderReady) {
    nsresult rv = InitTransportAndSendAnswer();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return ReplyError(NS_ERROR_DOM_OPERATION_ERR);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
PresentationPresentingInfo::OnAnswer(nsIPresentationChannelDescription* aDescription)
{
  MOZ_ASSERT(false, "Receiver side should not receive answer.");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PresentationPresentingInfo::OnIceCandidate(const nsAString& aCandidate)
{
  MOZ_ASSERT(false, "Should not receive ICE candidates.");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PresentationPresentingInfo::NotifyOpened()
{
  // Do nothing.
  return NS_OK;
}

NS_IMETHODIMP
PresentationPresentingInfo::NotifyClosed(nsresult aReason)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Unset control channel here so it won't try to re-close it in potential
  // subsequent |Shutdown| calls.
  SetControlChannel(nullptr);

  if (NS_WARN_IF(NS_FAILED(aReason))) {
    // The presentation session instance may already exist.
    // Change the state to TERMINATED since it never succeeds.
    SetState(nsIPresentationSessionListener::STATE_TERMINATED);

    // Reply error for an abnormal close.
    return ReplyError(NS_ERROR_DOM_OPERATION_ERR);
  }

  return NS_OK;
}

// nsITimerCallback
NS_IMETHODIMP
PresentationPresentingInfo::Notify(nsITimer* aTimer)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_WARNING("The receiver page fails to become ready before timeout.");

  mTimer = nullptr;
  return ReplyError(NS_ERROR_DOM_TIMEOUT_ERR);
}

// PromiseNativeHandler
void
PresentationPresentingInfo::ResolvedCallback(JSContext* aCx,
                                            JS::Handle<JS::Value> aValue)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!aValue.isObject())) {
    ReplyError(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
  if (NS_WARN_IF(!obj)) {
    ReplyError(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  // Start to listen to document state change event |STATE_TRANSFERRING|.
  HTMLIFrameElement* frame = nullptr;
  nsresult rv = UNWRAP_OBJECT(HTMLIFrameElement, obj, frame);
  if (NS_WARN_IF(!frame)) {
    ReplyError(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  nsCOMPtr<nsIFrameLoaderOwner> owner = do_QueryInterface((nsIFrameLoaderOwner*) frame);
  if (NS_WARN_IF(!owner)) {
    ReplyError(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  nsCOMPtr<nsIFrameLoader> frameLoader;
  rv = owner->GetFrameLoader(getter_AddRefs(frameLoader));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ReplyError(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  RefPtr<TabParent> tabParent = TabParent::GetFrom(frameLoader);
  if (tabParent) {
    // OOP frame
    // Notify the content process that a receiver page has launched, so it can
    // start monitoring the loading progress.
    mContentParent = tabParent->Manager();
    NS_WARN_IF(!static_cast<ContentParent*>(mContentParent.get())->SendNotifyPresentationReceiverLaunched(tabParent, mSessionId));
  } else {
    // In-process frame
    nsCOMPtr<nsIDocShell> docShell;
    rv = frameLoader->GetDocShell(getter_AddRefs(docShell));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      ReplyError(NS_ERROR_DOM_OPERATION_ERR);
      return;
    }

    // Keep an eye on the loading progress of the receiver page.
    mLoadingCallback = new PresentationResponderLoadingCallback(mSessionId);
    rv = mLoadingCallback->Init(docShell);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      ReplyError(NS_ERROR_DOM_OPERATION_ERR);
      return;
    }
  }
}

void
PresentationPresentingInfo::RejectedCallback(JSContext* aCx,
                                            JS::Handle<JS::Value> aValue)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_WARNING("Launching the receiver page has been rejected.");

  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }

  ReplyError(NS_ERROR_DOM_OPERATION_ERR);
}
