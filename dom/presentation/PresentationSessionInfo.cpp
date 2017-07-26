/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/HTMLIFrameElementBinding.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/Logging.h"
#include "mozilla/Move.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsIDocShell.h"
#include "nsFrameLoader.h"
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
    NewRunnableMethod<nsCString>("dom::PresentationNetworkHelper::OnGetWifiIPAddress",
                                 mInfo,
                                 mFunc,
                                 aIPAddress));
  return NS_OK;
}

} // anonymous namespace

#endif // MOZ_WIDGET_ANDROID

class TCPPresentationChannelDescription final : public nsIPresentationChannelDescription
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONCHANNELDESCRIPTION

  TCPPresentationChannelDescription(const nsACString& aAddress,
                                    uint16_t aPort)
    : mAddress(aAddress)
    , mPort(aPort)
  {
  }

private:
  ~TCPPresentationChannelDescription() {}

  nsCString mAddress;
  uint16_t mPort;
};

} // namespace dom
} // namespace mozilla

NS_IMPL_ISUPPORTS(TCPPresentationChannelDescription, nsIPresentationChannelDescription)

NS_IMETHODIMP
TCPPresentationChannelDescription::GetType(uint8_t* aRetVal)
{
  if (NS_WARN_IF(!aRetVal)) {
    return NS_ERROR_INVALID_POINTER;
  }

  *aRetVal = nsIPresentationChannelDescription::TYPE_TCP;
  return NS_OK;
}

NS_IMETHODIMP
TCPPresentationChannelDescription::GetTcpAddress(nsIArray** aRetVal)
{
  if (NS_WARN_IF(!aRetVal)) {
    return NS_ERROR_INVALID_POINTER;
  }

  nsCOMPtr<nsIMutableArray> array = do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (NS_WARN_IF(!array)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // TODO bug 1228504 Take all IP addresses in PresentationChannelDescription
  // into account. And at the first stage Presentation API is only exposed on
  // Firefox OS where the first IP appears enough for most scenarios.
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
TCPPresentationChannelDescription::GetTcpPort(uint16_t* aRetVal)
{
  if (NS_WARN_IF(!aRetVal)) {
    return NS_ERROR_INVALID_POINTER;
  }

  *aRetVal = mPort;
  return NS_OK;
}

NS_IMETHODIMP
TCPPresentationChannelDescription::GetDataChannelSDP(nsAString& aDataChannelSDP)
{
  aDataChannelSDP.Truncate();
  return NS_OK;
}

/*
 * Implementation of PresentationSessionInfo
 */

NS_IMPL_ISUPPORTS(PresentationSessionInfo,
                  nsIPresentationSessionTransportCallback,
                  nsIPresentationControlChannelListener,
                  nsIPresentationSessionTransportBuilderListener);

/* virtual */ nsresult
PresentationSessionInfo::Init(nsIPresentationControlChannel* aControlChannel)
{
  SetControlChannel(aControlChannel);
  return NS_OK;
}

/* virtual */ void
PresentationSessionInfo::Shutdown(nsresult aReason)
{
  PRES_DEBUG("%s:id[%s], reason[%" PRIx32 "], role[%d]\n", __func__,
             NS_ConvertUTF16toUTF8(mSessionId).get(), static_cast<uint32_t>(aReason),
             mRole);

  NS_WARNING_ASSERTION(NS_SUCCEEDED(aReason), "bad reason");

  // Close the control channel if any.
  if (mControlChannel) {
    Unused << NS_WARN_IF(NS_FAILED(mControlChannel->Disconnect(aReason)));
  }

  // Close the data transport channel if any.
  if (mTransport) {
    // |mIsTransportReady| will be unset once |NotifyTransportClosed| is called.
    Unused << NS_WARN_IF(NS_FAILED(mTransport->Close(aReason)));
  }

  mIsResponderReady = false;
  mIsOnTerminating = false;

  ResetBuilder();
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
    return mListener->NotifyStateChange(mSessionId, mState, mReason);
  }

  return NS_OK;
}

nsresult
PresentationSessionInfo::Send(const nsAString& aData)
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
PresentationSessionInfo::SendBinaryMsg(const nsACString& aData)
{
  if (NS_WARN_IF(!IsSessionReady())) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  if (NS_WARN_IF(!mTransport)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return mTransport->SendBinaryMsg(aData);
}

nsresult
PresentationSessionInfo::SendBlob(nsIDOMBlob* aBlob)
{
  if (NS_WARN_IF(!IsSessionReady())) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  if (NS_WARN_IF(!mTransport)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return mTransport->SendBlob(aBlob);
}

nsresult
PresentationSessionInfo::Close(nsresult aReason,
                               uint32_t aState)
{
  // Do nothing if session is already terminated.
  if (nsIPresentationSessionListener::STATE_TERMINATED == mState) {
    return NS_OK;
  }

  SetStateWithReason(aState, aReason);

  switch (aState) {
    case nsIPresentationSessionListener::STATE_CLOSED: {
      Shutdown(aReason);
      break;
    }
    case nsIPresentationSessionListener::STATE_TERMINATED: {
      if (!mControlChannel) {
        nsCOMPtr<nsIPresentationControlChannel> ctrlChannel;
        nsresult rv = mDevice->EstablishControlChannel(getter_AddRefs(ctrlChannel));
        if (NS_FAILED(rv)) {
          Shutdown(rv);
          return rv;
        }

        SetControlChannel(ctrlChannel);
        return rv;
      }

      ContinueTermination();
      return NS_OK;
    }
  }

  return NS_OK;
}

nsresult
PresentationSessionInfo::OnTerminate(nsIPresentationControlChannel* aControlChannel)
{
  mIsOnTerminating = true; // Mark for terminating transport channel
  SetStateWithReason(nsIPresentationSessionListener::STATE_TERMINATED, NS_OK);
  SetControlChannel(aControlChannel);

  return NS_OK;
}

nsresult
PresentationSessionInfo::ReplySuccess()
{
  SetStateWithReason(nsIPresentationSessionListener::STATE_CONNECTED, NS_OK);
  return NS_OK;
}

nsresult
PresentationSessionInfo::ReplyError(nsresult aError)
{
  Shutdown(aError);

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

nsPIDOMWindowInner*
PresentationSessionInfo::GetWindow()
{
  nsCOMPtr<nsIPresentationService> service =
  do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!service)) {
    return nullptr;
  }
  uint64_t windowId = 0;
  if (NS_WARN_IF(NS_FAILED(service->GetWindowIdBySessionId(mSessionId,
                                                           mRole,
                                                           &windowId)))) {
    return nullptr;
  }

  auto window = nsGlobalWindow::GetInnerWindowWithId(windowId);
  if (!window) {
    return nullptr;
  }

  return window->AsInner();
}

/* virtual */ bool
PresentationSessionInfo::IsAccessible(base::ProcessId aProcessId)
{
  // No restriction by default.
  return true;
}

void
PresentationSessionInfo::ContinueTermination()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mControlChannel);

  if (NS_WARN_IF(NS_FAILED(mControlChannel->Terminate(mSessionId)))
      || mIsOnTerminating) {
    Shutdown(NS_OK);
  }
}

// nsIPresentationSessionTransportCallback
NS_IMETHODIMP
PresentationSessionInfo::NotifyTransportReady()
{
  PRES_DEBUG("%s:id[%s], role[%d], state[%d]\n", __func__,
             NS_ConvertUTF16toUTF8(mSessionId).get(), mRole, mState);

  MOZ_ASSERT(NS_IsMainThread());

  if (mState != nsIPresentationSessionListener::STATE_CONNECTING &&
      mState != nsIPresentationSessionListener::STATE_CONNECTED) {
    return NS_OK;
  }

  mIsTransportReady = true;

  // Established RTCDataChannel implies responder is ready.
  if (mTransportType == nsIPresentationChannelDescription::TYPE_DATACHANNEL) {
    mIsResponderReady = true;
  }

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
  PRES_DEBUG("%s:id[%s], reason[%" PRIx32 "], role[%d]\n", __func__,
             NS_ConvertUTF16toUTF8(mSessionId).get(), static_cast<uint32_t>(aReason),
             mRole);

  MOZ_ASSERT(NS_IsMainThread());

  // Nullify |mTransport| here so it won't try to re-close |mTransport| in
  // potential subsequent |Shutdown| calls.
  mTransport = nullptr;

  if (NS_WARN_IF(!IsSessionReady() &&
                 mState == nsIPresentationSessionListener::STATE_CONNECTING)) {
    // It happens before the session is ready. Reply the callback.
    return ReplyError(NS_ERROR_DOM_OPERATION_ERR);
  }

  // Unset |mIsTransportReady| here so it won't affect |IsSessionReady()| above.
  mIsTransportReady = false;

  if (mState == nsIPresentationSessionListener::STATE_CONNECTED) {
    // The transport channel is closed unexpectedly (not caused by a |Close| call).
    SetStateWithReason(nsIPresentationSessionListener::STATE_CLOSED, aReason);
  }

  Shutdown(aReason);

  if (mState == nsIPresentationSessionListener::STATE_TERMINATED) {
    // Directly untrack the session info from the service.
    return UntrackFromService();
  }

  return NS_OK;
}

NS_IMETHODIMP
PresentationSessionInfo::NotifyData(const nsACString& aData, bool aIsBinary)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!IsSessionReady())) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  if (NS_WARN_IF(!mListener)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return mListener->NotifyMessage(mSessionId, aData, aIsBinary);
}

// nsIPresentationSessionTransportBuilderListener
NS_IMETHODIMP
PresentationSessionInfo::OnSessionTransport(nsIPresentationSessionTransport* aTransport)
{
  PRES_DEBUG("%s:id[%s], role[%d], state[%d]\n", __func__,
             NS_ConvertUTF16toUTF8(mSessionId).get(), mRole, mState);

  ResetBuilder();

  if (mState != nsIPresentationSessionListener::STATE_CONNECTING) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(!aTransport)) {
    return NS_ERROR_INVALID_ARG;
  }

  mTransport = aTransport;

  nsresult rv = mTransport->SetCallback(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mListener) {
    mTransport->EnableDataNotification();
  }

  return NS_OK;
}

NS_IMETHODIMP
PresentationSessionInfo::OnError(nsresult aReason)
{
  PRES_DEBUG("%s:id[%s], reason[%" PRIx32 "], role[%d]\n", __func__,
             NS_ConvertUTF16toUTF8(mSessionId).get(), static_cast<uint32_t>(aReason),
             mRole);

  ResetBuilder();
  return ReplyError(aReason);
}

NS_IMETHODIMP
PresentationSessionInfo::SendOffer(nsIPresentationChannelDescription* aOffer)
{
  return mControlChannel->SendOffer(aOffer);
}

NS_IMETHODIMP
PresentationSessionInfo::SendAnswer(nsIPresentationChannelDescription* aAnswer)
{
  return mControlChannel->SendAnswer(aAnswer);
}

NS_IMETHODIMP
PresentationSessionInfo::SendIceCandidate(const nsAString& candidate)
{
  return mControlChannel->SendIceCandidate(candidate);
}

NS_IMETHODIMP
PresentationSessionInfo::Close(nsresult reason)
{
  return mControlChannel->Disconnect(reason);
}

/**
 * Implementation of PresentationControllingInfo
 *
 * During presentation session establishment, the sender expects the following
 * after trying to establish the control channel: (The order between step 3 and
 * 4 is not guaranteed.)
 * 1. |Init| is called to open a socket |mServerSocket| for data transport
 *    channel.
 * 2. |NotifyConnected| of |nsIPresentationControlChannelListener| is called to
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
    Unused << NS_WARN_IF(NS_FAILED(mServerSocket->Close()));
    mServerSocket = nullptr;
  }
}

nsresult
PresentationControllingInfo::GetAddress()
{
#if defined(MOZ_WIDGET_ANDROID)
  RefPtr<PresentationNetworkHelper> networkHelper =
    new PresentationNetworkHelper(this,
                                  &PresentationControllingInfo::OnGetAddress);
  nsresult rv = networkHelper->GetWifiIPAddress();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

#else
  nsCOMPtr<nsINetworkInfoService> networkInfo = do_GetService(NETWORKINFOSERVICE_CONTRACT_ID);
  MOZ_ASSERT(networkInfo);

  nsresult rv = networkInfo->ListNetworkAddresses(this);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
#endif

  return NS_OK;
}

nsresult
PresentationControllingInfo::OnGetAddress(const nsACString& aAddress)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!mServerSocket)) {
    return NS_ERROR_FAILURE;
  }
  if (NS_WARN_IF(!mControlChannel)) {
    return NS_ERROR_FAILURE;
  }

  // Prepare and send the offer.
  int32_t port;
  nsresult rv = mServerSocket->GetPort(&port);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<TCPPresentationChannelDescription> description =
    new TCPPresentationChannelDescription(aAddress, static_cast<uint16_t>(port));
  return mControlChannel->SendOffer(description);
}

// nsIPresentationControlChannelListener
NS_IMETHODIMP
PresentationControllingInfo::OnIceCandidate(const nsAString& aCandidate)
{
  if (mTransportType != nsIPresentationChannelDescription::TYPE_DATACHANNEL) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPresentationDataChannelSessionTransportBuilder>
    builder = do_QueryInterface(mBuilder);

  if (NS_WARN_IF(!builder)) {
    return NS_ERROR_FAILURE;
  }

  return builder->OnIceCandidate(aCandidate);
}

NS_IMETHODIMP
PresentationControllingInfo::OnOffer(nsIPresentationChannelDescription* aDescription)
{
  MOZ_ASSERT(false, "Sender side should not receive offer.");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PresentationControllingInfo::OnAnswer(nsIPresentationChannelDescription* aDescription)
{
  if (mTransportType == nsIPresentationChannelDescription::TYPE_DATACHANNEL) {
    nsCOMPtr<nsIPresentationDataChannelSessionTransportBuilder>
      builder = do_QueryInterface(mBuilder);

    if (NS_WARN_IF(!builder)) {
      return NS_ERROR_FAILURE;
    }

    return builder->OnAnswer(aDescription);
  }

  mIsResponderReady = true;

  // Close the control channel since it's no longer needed.
  nsresult rv = mControlChannel->Disconnect(NS_OK);
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
PresentationControllingInfo::NotifyConnected()
{
  PRES_DEBUG("%s:id[%s], role[%d]\n", __func__,
             NS_ConvertUTF16toUTF8(mSessionId).get(), mRole);

  MOZ_ASSERT(NS_IsMainThread());

  switch (mState) {
    case nsIPresentationSessionListener::STATE_CONNECTING: {
      if (mIsReconnecting) {
        return ContinueReconnect();
      }

      nsresult rv = mControlChannel->Launch(GetSessionId(), GetUrl());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      Unused << NS_WARN_IF(NS_FAILED(BuildTransport()));
      break;
    }
    case nsIPresentationSessionListener::STATE_TERMINATED: {
      ContinueTermination();
      break;
    }
    default:
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP
PresentationControllingInfo::NotifyReconnected()
{
  PRES_DEBUG("%s:id[%s], role[%d]\n", __func__,
             NS_ConvertUTF16toUTF8(mSessionId).get(), mRole);

  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(mState != nsIPresentationSessionListener::STATE_CONNECTING)) {
    return NS_ERROR_FAILURE;
  }

  return NotifyReconnectResult(NS_OK);
}

nsresult
PresentationControllingInfo::BuildTransport()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mState != nsIPresentationSessionListener::STATE_CONNECTING) {
    return NS_OK;
  }

  if (NS_WARN_IF(!mBuilderConstructor)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!Preferences::GetBool("dom.presentation.session_transport.data_channel.enable")) {
    // Build TCP session transport
    return GetAddress();
  }
  /**
   * Generally transport is maintained by the chrome process. However, data
   * channel should be live with the DOM , which implies RTCDataChannel in an OOP
   * page should be establish in the content process.
   *
   * |mBuilderConstructor| is responsible for creating a builder, which is for
   * building a data channel transport.
   *
   * In the OOP case, |mBuilderConstructor| would create a builder which is
   * an object of |PresentationBuilderParent|. So, |BuildDataChannelTransport|
   * triggers an IPC call to make content process establish a RTCDataChannel
   * transport.
   */

  mTransportType = nsIPresentationChannelDescription::TYPE_DATACHANNEL;
  if (NS_WARN_IF(NS_FAILED(
    mBuilderConstructor->CreateTransportBuilder(mTransportType,
                                                getter_AddRefs(mBuilder))))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIPresentationDataChannelSessionTransportBuilder>
    dataChannelBuilder(do_QueryInterface(mBuilder));
  if (NS_WARN_IF(!dataChannelBuilder)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // OOP window would be set from content process
  nsPIDOMWindowInner* window = GetWindow();

  nsresult rv = dataChannelBuilder->
         BuildDataChannelTransport(nsIPresentationService::ROLE_CONTROLLER,
                                   window,
                                   this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
PresentationControllingInfo::NotifyDisconnected(nsresult aReason)
{
  PRES_DEBUG("%s:id[%s], reason[%" PRIx32 "], role[%d]\n", __func__,
             NS_ConvertUTF16toUTF8(mSessionId).get(), static_cast<uint32_t>(aReason),
             mRole);

  MOZ_ASSERT(NS_IsMainThread());

  if (mTransportType == nsIPresentationChannelDescription::TYPE_DATACHANNEL) {
    nsCOMPtr<nsIPresentationDataChannelSessionTransportBuilder>
      builder = do_QueryInterface(mBuilder);
    if (builder) {
      Unused << NS_WARN_IF(NS_FAILED(builder->NotifyDisconnected(aReason)));
    }
  }

  // Unset control channel here so it won't try to re-close it in potential
  // subsequent |Shutdown| calls.
  SetControlChannel(nullptr);

  if (NS_WARN_IF(NS_FAILED(aReason) || !mIsResponderReady)) {
    // The presentation session instance may already exist.
    // Change the state to CLOSED if it is not terminated.
    if (nsIPresentationSessionListener::STATE_TERMINATED != mState) {
      SetStateWithReason(nsIPresentationSessionListener::STATE_CLOSED, aReason);
    }

    // If |aReason| is NS_OK, it implies that the user closes the connection
    // before becomming connected. No need to call |ReplyError| in this case.
    if (NS_FAILED(aReason)) {
      if (mIsReconnecting) {
        NotifyReconnectResult(NS_ERROR_DOM_OPERATION_ERR);
      }
      // Reply error for an abnormal close.
      return ReplyError(NS_ERROR_DOM_OPERATION_ERR);
    }
    Shutdown(aReason);
  }

  // This is the case for reconnecting a connection which is in
  // connecting state and |mTransport| is not ready.
  if (mDoReconnectAfterClose && !mTransport) {
    mDoReconnectAfterClose = false;
    return Reconnect(mReconnectCallback);
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

  if (NS_WARN_IF(!mBuilderConstructor)) {
    return ReplyError(NS_ERROR_DOM_OPERATION_ERR);
  }

  // Initialize session transport builder and use |this| as the callback.
  nsCOMPtr<nsIPresentationTCPSessionTransportBuilder> builder;
  if (NS_SUCCEEDED(mBuilderConstructor->CreateTransportBuilder(
                     nsIPresentationChannelDescription::TYPE_TCP,
                     getter_AddRefs(mBuilder)))) {
    builder = do_QueryInterface(mBuilder);
  }

  if (NS_WARN_IF(!builder)) {
    return ReplyError(NS_ERROR_DOM_OPERATION_ERR);
  }

  mTransportType = nsIPresentationChannelDescription::TYPE_TCP;
  return builder->BuildTCPSenderTransport(aTransport, this);
}

NS_IMETHODIMP
PresentationControllingInfo::OnStopListening(nsIServerSocket* aServerSocket,
                                           nsresult aStatus)
{
  PRES_DEBUG("controller %s:status[%" PRIx32 "]\n",__func__,
             static_cast<uint32_t>(aStatus));

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
  SetStateWithReason(nsIPresentationSessionListener::STATE_CLOSED, aStatus);

  return NS_OK;
}

/**
 * The steps to reconnect a session are summarized below:
 * 1. Change |mState| to CONNECTING.
 * 2. Check whether |mControlChannel| is existed or not. Usually we have to
 *    create a new control cahnnel.
 * 3.1 |mControlChannel| is null, which means we have to create a new one.
 *     |EstablishControlChannel| is called to create a new control channel.
 *     At this point, |mControlChannel| is not able to use yet. Set
 *     |mIsReconnecting| to true and wait until |NotifyConnected|.
 * 3.2 |mControlChannel| is not null and is avaliable.
 *     We can just call |ContinueReconnect| to send reconnect command.
 * 4. |NotifyReconnected| of |nsIPresentationControlChannelListener| is called
 *    to indicate the receiver is ready for reconnecting.
 * 5. Once both step 3 and 4 are done, the rest is to build a new data
 *    transport channel by following the same steps as starting a
 *    new session.
 */

nsresult
PresentationControllingInfo::Reconnect(nsIPresentationServiceCallback* aCallback)
{
  PRES_DEBUG("%s:id[%s], role[%d], state[%d]\n", __func__,
             NS_ConvertUTF16toUTF8(mSessionId).get(), mRole, mState);

  if (!aCallback) {
    return NS_ERROR_INVALID_ARG;
  }

  mReconnectCallback = aCallback;

  if (NS_WARN_IF(mState == nsIPresentationSessionListener::STATE_TERMINATED)) {
    return NotifyReconnectResult(NS_ERROR_DOM_INVALID_STATE_ERR);
  }

  // If |mState| is not CLOSED, we have to close the connection before
  // reconnecting. The process to reconnect will be continued after
  // |NotifyDisconnected| or |NotifyTransportClosed| is invoked.
  if (mState == nsIPresentationSessionListener::STATE_CONNECTING ||
      mState == nsIPresentationSessionListener::STATE_CONNECTED) {
    mDoReconnectAfterClose = true;
    return Close(NS_OK, nsIPresentationSessionListener::STATE_CLOSED);
  }

  // Make sure |mState| is closed at this point.
  MOZ_ASSERT(mState == nsIPresentationSessionListener::STATE_CLOSED);

  mState = nsIPresentationSessionListener::STATE_CONNECTING;
  mIsReconnecting = true;

  nsresult rv = NS_OK;
  if (!mControlChannel) {
    nsCOMPtr<nsIPresentationControlChannel> ctrlChannel;
    rv = mDevice->EstablishControlChannel(getter_AddRefs(ctrlChannel));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return NotifyReconnectResult(NS_ERROR_DOM_OPERATION_ERR);
    }

    rv = Init(ctrlChannel);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return NotifyReconnectResult(NS_ERROR_DOM_OPERATION_ERR);
    }
  } else {
    return ContinueReconnect();
  }

  return NS_OK;
}

nsresult
PresentationControllingInfo::ContinueReconnect()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mControlChannel);

  mIsReconnecting = false;
  if (NS_WARN_IF(NS_FAILED(mControlChannel->Reconnect(mSessionId, GetUrl())))) {
    return NotifyReconnectResult(NS_ERROR_DOM_OPERATION_ERR);
  }

  return NS_OK;
}

// nsIListNetworkAddressesListener
NS_IMETHODIMP
PresentationControllingInfo::OnListedNetworkAddresses(const char** aAddressArray,
                                                      uint32_t aAddressArraySize)
{
  if (!aAddressArraySize) {
    return OnListNetworkAddressesFailed();
  }

  // TODO bug 1228504 Take all IP addresses in PresentationChannelDescription
  // into account. And at the first stage Presentation API is only exposed on
  // Firefox OS where the first IP appears enough for most scenarios.

  nsAutoCString ip;
  ip.Assign(aAddressArray[0]);

  // On Firefox desktop, the IP address is retrieved from a callback function.
  // To make consistent code sequence, following function call is dispatched
  // into main thread instead of calling it directly.
  NS_DispatchToMainThread(NewRunnableMethod<nsCString>(
    "dom::PresentationControllingInfo::OnGetAddress",
    this,
    &PresentationControllingInfo::OnGetAddress,
    ip));

  return NS_OK;
}

NS_IMETHODIMP
PresentationControllingInfo::OnListNetworkAddressesFailed()
{
  PRES_ERROR("PresentationControllingInfo:OnListNetworkAddressesFailed");

  // In 1-UA case, transport channel can still be established
  // on loopback interface even if no network address available.
  NS_DispatchToMainThread(NewRunnableMethod<nsCString>(
    "dom::PresentationControllingInfo::OnGetAddress",
    this,
    &PresentationControllingInfo::OnGetAddress,
    "127.0.0.1"));

  return NS_OK;
}

nsresult
PresentationControllingInfo::NotifyReconnectResult(nsresult aStatus)
{
  if (!mReconnectCallback) {
    MOZ_ASSERT(false, "mReconnectCallback can not be null here.");
    return NS_ERROR_FAILURE;
  }

  mIsReconnecting = false;
  nsCOMPtr<nsIPresentationServiceCallback> callback =
    mReconnectCallback.forget();
  if (NS_FAILED(aStatus)) {
    return callback->NotifyError(aStatus);
  }

  return callback->NotifySuccess(GetUrl());
}

// nsIPresentationSessionTransportCallback
NS_IMETHODIMP
PresentationControllingInfo::NotifyTransportReady()
{
  return PresentationSessionInfo::NotifyTransportReady();
}

NS_IMETHODIMP
PresentationControllingInfo::NotifyTransportClosed(nsresult aReason)
{
  if (!mDoReconnectAfterClose) {
    return PresentationSessionInfo::NotifyTransportClosed(aReason);;
  }

  MOZ_ASSERT(mState == nsIPresentationSessionListener::STATE_CLOSED);

  mTransport = nullptr;
  mIsTransportReady = false;
  mDoReconnectAfterClose = false;
  return Reconnect(mReconnectCallback);
}

NS_IMETHODIMP
PresentationControllingInfo::NotifyData(const nsACString& aData, bool aIsBinary)
{
  return PresentationSessionInfo::NotifyData(aData, aIsBinary);
}

/**
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
                            nsITimerCallback,
                            nsINamed)

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
  mPendingCandidates.Clear();
  mPromise = nullptr;
  mHasFlushPendingEvents = false;
}

// nsIPresentationSessionTransportBuilderListener
NS_IMETHODIMP
PresentationPresentingInfo::OnSessionTransport(nsIPresentationSessionTransport* aTransport)
{
  nsresult rv = PresentationSessionInfo::OnSessionTransport(aTransport);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // The session transport is managed by content process
  if (NS_WARN_IF(!aTransport)) {
    return NS_ERROR_INVALID_ARG;
  }

  // send answer for TCP session transport
  if (mTransportType == nsIPresentationChannelDescription::TYPE_TCP) {
    // Prepare and send the answer.
    // In the current implementation of |PresentationSessionTransport|,
    // |GetSelfAddress| cannot return the real info when it's initialized via
    // |buildTCPReceiverTransport|. Yet this deficiency only affects the channel
    // description for the answer, which is not actually checked at requester side.
    nsCOMPtr<nsINetAddr> selfAddr;
    rv = mTransport->GetSelfAddress(getter_AddRefs(selfAddr));
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "GetSelfAddress failed");

    nsCString address;
    uint16_t port = 0;
    if (NS_SUCCEEDED(rv)) {
      selfAddr->GetAddress(address);
      selfAddr->GetPort(&port);
    }
    nsCOMPtr<nsIPresentationChannelDescription> description =
      new TCPPresentationChannelDescription(address, port);

    return mControlChannel->SendAnswer(description);
  }

  return NS_OK;
}

// Delegate the pending offer and ICE candidates to builder.
NS_IMETHODIMP
PresentationPresentingInfo::FlushPendingEvents(nsIPresentationDataChannelSessionTransportBuilder* builder)
{
  if (NS_WARN_IF(!builder)) {
    return NS_ERROR_FAILURE;
  }

  mHasFlushPendingEvents = true;

  if (mRequesterDescription) {
    builder->OnOffer(mRequesterDescription);
  }
  mRequesterDescription = nullptr;

  for (size_t i = 0; i < mPendingCandidates.Length(); ++i) {
    builder->OnIceCandidate(mPendingCandidates[i]);
  }
  mPendingCandidates.Clear();
  return NS_OK;
}

nsresult
PresentationPresentingInfo::InitTransportAndSendAnswer()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mState == nsIPresentationSessionListener::STATE_CONNECTING);

  uint8_t type = 0;
  nsresult rv = mRequesterDescription->GetType(&type);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!mBuilderConstructor)) {
    return ReplyError(NS_ERROR_DOM_OPERATION_ERR);
  }

  if (NS_WARN_IF(NS_FAILED(
    mBuilderConstructor->CreateTransportBuilder(type,
                                                getter_AddRefs(mBuilder))))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (type == nsIPresentationChannelDescription::TYPE_TCP) {
    // Establish a data transport channel |mTransport| to the sender and use
    // |this| as the callback.
    nsCOMPtr<nsIPresentationTCPSessionTransportBuilder> builder =
      do_QueryInterface(mBuilder);
    if (NS_WARN_IF(!builder)) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    mTransportType = nsIPresentationChannelDescription::TYPE_TCP;
    return builder->BuildTCPReceiverTransport(mRequesterDescription, this);
  }

  if (type == nsIPresentationChannelDescription::TYPE_DATACHANNEL) {
    if (!Preferences::GetBool("dom.presentation.session_transport.data_channel.enable")) {
      return NS_ERROR_NOT_IMPLEMENTED;
    }
    /**
     * Generally transport is maintained by the chrome process. However, data
     * channel should be live with the DOM , which implies RTCDataChannel in an OOP
     * page should be establish in the content process.
     *
     * |mBuilderConstructor| is responsible for creating a builder, which is for
     * building a data channel transport.
     *
     * In the OOP case, |mBuilderConstructor| would create a builder which is
     * an object of |PresentationBuilderParent|. So, |BuildDataChannelTransport|
     * triggers an IPC call to make content process establish a RTCDataChannel
     * transport.
     */

    mTransportType = nsIPresentationChannelDescription::TYPE_DATACHANNEL;

    nsCOMPtr<nsIPresentationDataChannelSessionTransportBuilder> dataChannelBuilder =
      do_QueryInterface(mBuilder);
    if (NS_WARN_IF(!dataChannelBuilder)) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    nsPIDOMWindowInner* window = GetWindow();

    rv = dataChannelBuilder->
           BuildDataChannelTransport(nsIPresentationService::ROLE_RECEIVER,
                                     window,
                                     this);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = FlushPendingEvents(dataChannelBuilder);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    return NS_OK;
  }

  MOZ_ASSERT(false, "Unknown nsIPresentationChannelDescription type!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
PresentationPresentingInfo::UntrackFromService()
{
  // Remove the OOP responding info (if it has never been used).
  if (mContentParent) {
    Unused << NS_WARN_IF(!static_cast<ContentParent*>(mContentParent.get())->SendNotifyPresentationReceiverCleanUp(mSessionId));
  }

  // Receiver device might need clean up after session termination.
  if (mDevice) {
    mDevice->Disconnect();
  }
  mDevice = nullptr;

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
  PRES_DEBUG("%s:id[%s], role[%d], state[%d]\n", __func__,
             NS_ConvertUTF16toUTF8(mSessionId).get(), mRole, mState);

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

nsresult
PresentationPresentingInfo::NotifyResponderFailure()
{
  PRES_DEBUG("%s:id[%s], role[%d]\n", __func__,
             NS_ConvertUTF16toUTF8(mSessionId).get(), mRole);

  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }

  return ReplyError(NS_ERROR_DOM_OPERATION_ERR);
}

nsresult
PresentationPresentingInfo::DoReconnect()
{
  PRES_DEBUG("%s:id[%s], role[%d]\n", __func__,
             NS_ConvertUTF16toUTF8(mSessionId).get(), mRole);

  MOZ_ASSERT(mState == nsIPresentationSessionListener::STATE_CLOSED);

  SetStateWithReason(nsIPresentationSessionListener::STATE_CONNECTING, NS_OK);

  return NotifyResponderReady();
}

// nsIPresentationControlChannelListener
NS_IMETHODIMP
PresentationPresentingInfo::OnOffer(nsIPresentationChannelDescription* aDescription)
{
  if (NS_WARN_IF(mHasFlushPendingEvents)) {
    return ReplyError(NS_ERROR_DOM_OPERATION_ERR);
  }

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
  if (!mBuilder && !mHasFlushPendingEvents) {
    mPendingCandidates.AppendElement(nsString(aCandidate));
    return NS_OK;
  }

  if (NS_WARN_IF(!mBuilder && mHasFlushPendingEvents)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPresentationDataChannelSessionTransportBuilder>
    builder = do_QueryInterface(mBuilder);

  return builder->OnIceCandidate(aCandidate);
}

NS_IMETHODIMP
PresentationPresentingInfo::NotifyConnected()
{
  PRES_DEBUG("%s:id[%s], role[%d]\n", __func__,
             NS_ConvertUTF16toUTF8(mSessionId).get(), mRole);

  if (nsIPresentationSessionListener::STATE_TERMINATED == mState) {
    ContinueTermination();
  }

  return NS_OK;
}

NS_IMETHODIMP
PresentationPresentingInfo::NotifyReconnected()
{
  MOZ_ASSERT(false, "NotifyReconnected should not be called at receiver side.");
  return NS_OK;
}

NS_IMETHODIMP
PresentationPresentingInfo::NotifyDisconnected(nsresult aReason)
{
  PRES_DEBUG("%s:id[%s], reason[%" PRIx32 "], role[%d]\n", __func__,
             NS_ConvertUTF16toUTF8(mSessionId).get(), static_cast<uint32_t>(aReason),
             mRole);

  MOZ_ASSERT(NS_IsMainThread());

  if (mTransportType == nsIPresentationChannelDescription::TYPE_DATACHANNEL) {
    nsCOMPtr<nsIPresentationDataChannelSessionTransportBuilder>
      builder = do_QueryInterface(mBuilder);
    if (builder) {
      Unused << NS_WARN_IF(NS_FAILED(builder->NotifyDisconnected(aReason)));
    }
  }

  // Unset control channel here so it won't try to re-close it in potential
  // subsequent |Shutdown| calls.
  SetControlChannel(nullptr);

  if (NS_WARN_IF(NS_FAILED(aReason))) {
    // The presentation session instance may already exist.
    // Change the state to TERMINATED since it never succeeds.
    SetStateWithReason(nsIPresentationSessionListener::STATE_TERMINATED, aReason);

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

// nsITimerCallback
NS_IMETHODIMP
PresentationPresentingInfo::GetName(nsACString& aName)
{
  aName.AssignLiteral("PresentationPresentingInfo");
  return NS_OK;
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
  // Use Element to support both HTMLIFrameElement and nsXULElement.
  Element* frame = nullptr;
  nsresult rv = UNWRAP_OBJECT(Element, &obj, frame);
  if (NS_WARN_IF(!frame)) {
    ReplyError(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  nsCOMPtr<nsIFrameLoaderOwner> owner = do_QueryInterface((nsIFrameLoaderOwner*) frame);
  if (NS_WARN_IF(!owner)) {
    ReplyError(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  nsCOMPtr<nsIFrameLoader> frameLoader = owner->GetFrameLoader();
  if (NS_WARN_IF(!frameLoader)) {
    ReplyError(NS_ERROR_DOM_OPERATION_ERR);
    return;
  }

  RefPtr<TabParent> tabParent = TabParent::GetFrom(frameLoader);
  if (tabParent) {
    // OOP frame
    // Notify the content process that a receiver page has launched, so it can
    // start monitoring the loading progress.
    mContentParent = tabParent->Manager();
    Unused << NS_WARN_IF(!static_cast<ContentParent*>(mContentParent.get())->SendNotifyPresentationReceiverLaunched(tabParent, mSessionId));
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
