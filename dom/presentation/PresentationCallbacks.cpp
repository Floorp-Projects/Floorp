/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Promise.h"
#include "nsIDocShell.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPresentationService.h"
#include "nsIWebProgress.h"
#include "nsServiceManagerUtils.h"
#include "PresentationCallbacks.h"
#include "PresentationRequest.h"
#include "PresentationConnection.h"

using namespace mozilla;
using namespace mozilla::dom;

/*
 * Implementation of PresentationRequesterCallback
 */

NS_IMPL_ISUPPORTS(PresentationRequesterCallback, nsIPresentationServiceCallback)

PresentationRequesterCallback::PresentationRequesterCallback(PresentationRequest* aRequest,
                                                             const nsAString& aUrl,
                                                             const nsAString& aSessionId,
                                                             Promise* aPromise)
  : mRequest(aRequest)
  , mSessionId(aSessionId)
  , mPromise(aPromise)
{
  MOZ_ASSERT(mRequest);
  MOZ_ASSERT(mPromise);
  MOZ_ASSERT(!mSessionId.IsEmpty());
}

PresentationRequesterCallback::~PresentationRequesterCallback()
{
}

// nsIPresentationServiceCallback
NS_IMETHODIMP
PresentationRequesterCallback::NotifySuccess()
{
  MOZ_ASSERT(NS_IsMainThread());

  // At the sender side, this function must get called after the transport
  // channel is ready. So we simply set the connection state as connected.
  nsRefPtr<PresentationConnection> connection =
    PresentationConnection::Create(mRequest->GetOwner(), mSessionId,
                                   PresentationConnectionState::Connected);
  if (NS_WARN_IF(!connection)) {
    mPromise->MaybeReject(NS_ERROR_DOM_OPERATION_ERR);
    return NS_OK;
  }

  mPromise->MaybeResolve(connection);

  return mRequest->DispatchConnectionAvailableEvent(connection);
}

NS_IMETHODIMP
PresentationRequesterCallback::NotifyError(nsresult aError)
{
  MOZ_ASSERT(NS_IsMainThread());

  mPromise->MaybeReject(aError);
  return NS_OK;
}

/*
 * Implementation of PresentationRequesterCallback
 */

NS_IMPL_ISUPPORTS(PresentationResponderLoadingCallback,
                  nsIWebProgressListener,
                  nsISupportsWeakReference)

PresentationResponderLoadingCallback::PresentationResponderLoadingCallback(const nsAString& aSessionId)
  : mSessionId(aSessionId)
{
}

PresentationResponderLoadingCallback::~PresentationResponderLoadingCallback()
{
  if (mProgress) {
    mProgress->RemoveProgressListener(this);
    mProgress = nullptr;
  }
}

nsresult
PresentationResponderLoadingCallback::Init(nsIDocShell* aDocShell)
{
  mProgress = do_GetInterface(aDocShell);
  if (NS_WARN_IF(!mProgress)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  uint32_t busyFlags = nsIDocShell::BUSY_FLAGS_NONE;
  nsresult rv = aDocShell->GetBusyFlags(&busyFlags);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if ((busyFlags == nsIDocShell::BUSY_FLAGS_NONE) ||
      (busyFlags & nsIDocShell::BUSY_FLAGS_PAGE_LOADING)) {
    // The docshell has finished loading or is receiving data (|STATE_TRANSFERRING|
    // has already been fired), so the page is ready for presentation use.
    return NotifyReceiverReady();
  }

  // Start to listen to document state change event |STATE_TRANSFERRING|.
  return mProgress->AddProgressListener(this, nsIWebProgress::NOTIFY_STATE_DOCUMENT);
}

nsresult
PresentationResponderLoadingCallback::NotifyReceiverReady()
{
  nsCOMPtr<nsPIDOMWindow> window = do_GetInterface(mProgress);
  if (NS_WARN_IF(!window || !window->GetCurrentInnerWindow())) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  uint64_t windowId = window->GetCurrentInnerWindow()->WindowID();

  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!service)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return service->NotifyReceiverReady(mSessionId, windowId);
}

// nsIWebProgressListener
NS_IMETHODIMP
PresentationResponderLoadingCallback::OnStateChange(nsIWebProgress* aWebProgress,
                                                    nsIRequest* aRequest,
                                                    uint32_t aStateFlags,
                                                    nsresult aStatus)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aStateFlags & nsIWebProgressListener::STATE_TRANSFERRING) {
    mProgress->RemoveProgressListener(this);

    return NotifyReceiverReady();
  }

  return NS_OK;
}

NS_IMETHODIMP
PresentationResponderLoadingCallback::OnProgressChange(nsIWebProgress* aWebProgress,
                                                       nsIRequest* aRequest,
                                                       int32_t aCurSelfProgress,
                                                       int32_t aMaxSelfProgress,
                                                       int32_t aCurTotalProgress,
                                                       int32_t aMaxTotalProgress)
{
  // Do nothing.
  return NS_OK;
}

NS_IMETHODIMP
PresentationResponderLoadingCallback::OnLocationChange(nsIWebProgress* aWebProgress,
                                                       nsIRequest* aRequest,
                                                       nsIURI* aURI,
                                                       uint32_t aFlags)
{
  // Do nothing.
  return NS_OK;
}

NS_IMETHODIMP
PresentationResponderLoadingCallback::OnStatusChange(nsIWebProgress* aWebProgress,
                                                     nsIRequest* aRequest,
                                                     nsresult aStatus,
                                                     const char16_t* aMessage)
{
  // Do nothing.
  return NS_OK;
}

NS_IMETHODIMP
PresentationResponderLoadingCallback::OnSecurityChange(nsIWebProgress* aWebProgress,
                                                       nsIRequest* aRequest,
                                                       uint32_t state)
{
  // Do nothing.
  return NS_OK;
}
