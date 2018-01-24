/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerInterceptController.h"

#include "mozilla/BasePrincipal.h"
#include "nsContentUtils.h"
#include "nsIChannel.h"
#include "ServiceWorkerManager.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(ServiceWorkerInterceptController, nsINetworkInterceptController)

NS_IMETHODIMP
ServiceWorkerInterceptController::ShouldPrepareForIntercept(nsIURI* aURI,
                                                            nsIChannel* aChannel,
                                                            bool* aShouldIntercept)
{
  *aShouldIntercept = false;

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
  if (!loadInfo) {
    return NS_OK;
  }

  // For subresource requests we base our decision solely on the client's
  // controller value.  Any settings that would have blocked service worker
  // access should have been set before the initial navigation created the
  // window.
  if (!nsContentUtils::IsNonSubresourceRequest(aChannel)) {
    const Maybe<ServiceWorkerDescriptor>& controller = loadInfo->GetController();
    *aShouldIntercept = controller.isSome();
    return NS_OK;
  }

  if (nsContentUtils::StorageAllowedForChannel(aChannel) !=
      nsContentUtils::StorageAccess::eAllow) {
    return NS_OK;
  }

  nsCOMPtr<nsIPrincipal> principal =
    BasePrincipal::CreateCodebasePrincipal(aURI,
                                           loadInfo->GetOriginAttributes());

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    return NS_OK;
  }

  // We're allowed to control a window, so check with the ServiceWorkerManager
  // for a matching service worker.
  *aShouldIntercept = swm->IsAvailable(principal, aURI);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerInterceptController::ChannelIntercepted(nsIInterceptedChannel* aChannel)
{
  // Note, do not cancel the interception here.  The caller will try to
  // ResetInterception() on error.

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult error;
  swm->DispatchFetchEvent(aChannel, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
