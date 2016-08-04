/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsServiceManagerUtils.h"
#include "PresentationContentSessionInfo.h"
#include "PresentationIPCService.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(PresentationContentSessionInfo,
                  nsIPresentationSessionTransportCallback);

nsresult
PresentationContentSessionInfo::Init() {
  if (NS_WARN_IF(NS_FAILED(mTransport->SetCallback(this)))) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  if (NS_WARN_IF(NS_FAILED(mTransport->EnableDataNotification()))) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return NS_OK;
}

nsresult
PresentationContentSessionInfo::Send(const nsAString& aData)
{
  if (!mTransport) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return mTransport->Send(aData);
}

nsresult
PresentationContentSessionInfo::Close(nsresult aReason)
{
  if (!mTransport) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return mTransport->Close(aReason);
}

// nsIPresentationSessionTransportCallback
NS_IMETHODIMP
PresentationContentSessionInfo::NotifyTransportReady()
{
  // do nothing since |onSessionTransport| implies this
  return NS_OK;
}

NS_IMETHODIMP
PresentationContentSessionInfo::NotifyTransportClosed(nsresult aReason)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Nullify |mTransport| here so it won't try to re-close |mTransport| in
  // potential subsequent |Shutdown| calls.
  mTransport = nullptr;
  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!service)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return static_cast<PresentationIPCService*>(service.get())->
           NotifyTransportClosed(mSessionId, mRole, aReason);
}

NS_IMETHODIMP
PresentationContentSessionInfo::NotifyData(const nsACString& aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!service)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return static_cast<PresentationIPCService*>(service.get())->
           NotifyMessage(mSessionId, aData);
}

} // namespace dom
} // namespace mozilla
