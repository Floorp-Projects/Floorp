/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DCPresentationChannelDescription.h"
#include "nsComponentManagerUtils.h"
#include "nsGlobalWindow.h"
#include "PresentationBuilderChild.h"
#include "PresentationIPCService.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(PresentationBuilderChild,
                  nsIPresentationSessionTransportBuilderListener)

PresentationBuilderChild::PresentationBuilderChild(const nsString& aSessionId,
                                                   uint8_t aRole)
  : mSessionId(aSessionId)
  , mRole(aRole)
{
}

nsresult PresentationBuilderChild::Init()
{
  mBuilder = do_CreateInstance("@mozilla.org/presentation/datachanneltransportbuilder;1");
  if (NS_WARN_IF(!mBuilder)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  uint64_t windowId = 0;

  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!service)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (NS_WARN_IF(NS_FAILED(service->GetWindowIdBySessionId(mSessionId, &windowId)))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsPIDOMWindowInner* window = nsGlobalWindow::GetInnerWindowWithId(windowId)->AsInner();
  if (NS_WARN_IF(!window)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return mBuilder->BuildDataChannelTransport(mRole, window, this);
}

void
PresentationBuilderChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mBuilder = nullptr;
  mActorDestroyed = true;
}

bool
PresentationBuilderChild::RecvOnOffer(const nsString& aSDP)
{
  if (NS_WARN_IF(!mBuilder)) {
    return false;
  }
  RefPtr<DCPresentationChannelDescription> description =
    new DCPresentationChannelDescription(aSDP);

  if (NS_WARN_IF(NS_FAILED(mBuilder->OnOffer(description)))) {
    return false;
  }
  return true;
}

bool
PresentationBuilderChild::RecvOnAnswer(const nsString& aSDP)
{
  if (NS_WARN_IF(!mBuilder)) {
    return false;
  }
  RefPtr<DCPresentationChannelDescription> description =
    new DCPresentationChannelDescription(aSDP);

  if (NS_WARN_IF(NS_FAILED(mBuilder->OnAnswer(description)))) {
    return false;
  }
  return true;
}

bool
PresentationBuilderChild::RecvOnIceCandidate(const nsString& aCandidate)
{
  if (NS_WARN_IF(NS_FAILED(mBuilder->OnIceCandidate(aCandidate)))) {
    return false;
  }
  return true;
}

// nsPresentationSessionTransportBuilderListener
NS_IMETHODIMP
PresentationBuilderChild::OnSessionTransport(nsIPresentationSessionTransport* aTransport)
{
  if (NS_WARN_IF(mActorDestroyed || !SendOnSessionTransport())){
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  NS_WARN_IF(!service);
  if (service) {
    NS_WARN_IF(NS_FAILED(static_cast<PresentationIPCService*>(service.get())->
                           NotifySessionTransport(mSessionId, mRole, aTransport)));
  }
  mBuilder = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
PresentationBuilderChild::OnError(nsresult reason)
{
  mBuilder = nullptr;

  if (NS_WARN_IF(mActorDestroyed || !SendOnSessionTransportError(reason))){
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationBuilderChild::SendOffer(nsIPresentationChannelDescription* aOffer)
{
  nsAutoString SDP;
  nsresult rv = aOffer->GetDataChannelSDP(SDP);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(mActorDestroyed || !SendSendOffer(SDP))){
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationBuilderChild::SendAnswer(nsIPresentationChannelDescription* aAnswer)
{
  nsAutoString SDP;
  nsresult rv = aAnswer->GetDataChannelSDP(SDP);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(mActorDestroyed || !SendSendAnswer(SDP))){
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationBuilderChild::SendIceCandidate(const nsAString& candidate)
{
  if (NS_WARN_IF(mActorDestroyed || !SendSendIceCandidate(nsString(candidate)))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationBuilderChild::Close(nsresult reason)
{
  if (NS_WARN_IF(mActorDestroyed || !SendClose(reason))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

} // namespace dom
} // namespace mozilla

