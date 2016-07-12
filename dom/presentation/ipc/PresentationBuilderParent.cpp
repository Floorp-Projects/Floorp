/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DCPresentationChannelDescription.h"
#include "PresentationBuilderParent.h"
#include "PresentationSessionInfo.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(PresentationBuilderParent,
                  nsIPresentationSessionTransportBuilder,
                  nsIPresentationDataChannelSessionTransportBuilder)

PresentationBuilderParent::PresentationBuilderParent(PresentationParent* aParent)
  : mParent(aParent)
{
  MOZ_COUNT_CTOR(PresentationBuilderParent);
}

PresentationBuilderParent::~PresentationBuilderParent()
{
  MOZ_COUNT_DTOR(PresentationBuilderParent);

  if (mNeedDestroyActor) {
    NS_WARN_IF(!Send__delete__(this));
  }
}

NS_IMETHODIMP
PresentationBuilderParent::BuildDataChannelTransport(
                      uint8_t aRole,
                      mozIDOMWindow* aWindow, /* unused */
                      nsIPresentationSessionTransportBuilderListener* aListener)
{
  mBuilderListener = aListener;

  RefPtr<PresentationSessionInfo> info = static_cast<PresentationSessionInfo*>(aListener);
  if (NS_WARN_IF(!mParent->SendPPresentationBuilderConstructor(this,
                                                               nsString(info->GetSessionId()),
                                                               aRole))) {
    return NS_ERROR_FAILURE;
  }
  mNeedDestroyActor = true;
  mParent = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
PresentationBuilderParent::OnIceCandidate(const nsAString& aCandidate)
{
  if (NS_WARN_IF(!SendOnIceCandidate(nsString(aCandidate)))){
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationBuilderParent::OnOffer(nsIPresentationChannelDescription* aDescription)
{
  nsAutoString SDP;
  nsresult rv = aDescription->GetDataChannelSDP(SDP);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!SendOnOffer(SDP))){
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationBuilderParent::OnAnswer(nsIPresentationChannelDescription* aDescription)
{
  nsAutoString SDP;
  nsresult rv = aDescription->GetDataChannelSDP(SDP);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!SendOnAnswer(SDP))){
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationBuilderParent::NotifyDisconnected(nsresult aReason)
{
  return NS_OK;
}

void
PresentationBuilderParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mNeedDestroyActor = false;
  mParent = nullptr;
  mBuilderListener = nullptr;
}

bool
PresentationBuilderParent::RecvSendOffer(const nsString& aSDP)
{
  RefPtr<DCPresentationChannelDescription> description =
    new DCPresentationChannelDescription(aSDP);
  if (NS_WARN_IF(!mBuilderListener ||
                 NS_FAILED(mBuilderListener->SendOffer(description)))) {
    return false;
  }
  return true;
}

bool
PresentationBuilderParent::RecvSendAnswer(const nsString& aSDP)
{
  RefPtr<DCPresentationChannelDescription> description =
    new DCPresentationChannelDescription(aSDP);
  if (NS_WARN_IF(!mBuilderListener ||
                 NS_FAILED(mBuilderListener->SendAnswer(description)))) {
    return false;
  }
  return true;
}

bool
PresentationBuilderParent::RecvSendIceCandidate(const nsString& aCandidate)
{
  if (NS_WARN_IF(!mBuilderListener ||
                 NS_FAILED(mBuilderListener->SendIceCandidate(aCandidate)))) {
    return false;
  }
  return true;
}

bool
PresentationBuilderParent::RecvClose(const nsresult& aReason)
{
  if (NS_WARN_IF(!mBuilderListener ||
                 NS_FAILED(mBuilderListener->Close(aReason)))) {
    return false;
  }
  return true;
}

// Delegate to nsIPresentationSessionTransportBuilderListener
bool
PresentationBuilderParent::RecvOnSessionTransport()
{
  // To avoid releasing |this| in this method
  NS_DispatchToMainThread(NS_NewRunnableFunction([this]() -> void {
    NS_WARN_IF(!mBuilderListener ||
               NS_FAILED(mBuilderListener->OnSessionTransport(nullptr)));
  }));

  nsCOMPtr<nsIPresentationSessionTransportCallback>
    callback(do_QueryInterface(mBuilderListener));

  callback->NotifyTransportReady();
  return true;
}

bool
PresentationBuilderParent::RecvOnSessionTransportError(const nsresult& aReason)
{
  if (NS_WARN_IF(!mBuilderListener ||
                 NS_FAILED(mBuilderListener->OnError(aReason)))) {
    return false;
  }
  return true;
}

} // namespace dom
} // namespace mozilla
