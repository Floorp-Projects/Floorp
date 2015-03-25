/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "nsIPresentationListener.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "PresentationService.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(PresentationService, nsIPresentationService, nsIObserver)

PresentationService::PresentationService()
{
}

PresentationService::~PresentationService()
{
}

bool
PresentationService::Init()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return false;
  }

  // TODO: Add observers and get available devices.

  return true;
}

NS_IMETHODIMP
PresentationService::Observe(nsISupports* aSubject,
                             const char* aTopic,
                             const char16_t* aData)
{
  // TODO: Handle device availability changes can call |NotifyAvailableChange|.

  return NS_OK;
}

void
PresentationService::NotifyAvailableChange(bool aIsAvailable)
{
  nsTObserverArray<nsCOMPtr<nsIPresentationListener> >::ForwardIterator iter(mListeners);
  while (iter.HasMore()) {
    nsCOMPtr<nsIPresentationListener> listener = iter.GetNext();
    NS_WARN_IF(NS_FAILED(listener->NotifyAvailableChange(aIsAvailable)));
  }
}

NS_IMETHODIMP
PresentationService::StartSession(const nsAString& aUrl,
                                  const nsAString& aSessionId,
                                  const nsAString& aOrigin,
                                  nsIPresentationServiceCallback* aCallback)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(!aSessionId.IsEmpty());

  // TODO: Reply the callback.

  return NS_OK;
}

NS_IMETHODIMP
PresentationService::SendSessionMessage(const nsAString& aSessionId,
                                        nsIInputStream* aStream)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aStream);
  MOZ_ASSERT(!aSessionId.IsEmpty());

  // TODO: Send input stream to the session.

  return NS_OK;
}

NS_IMETHODIMP
PresentationService::Terminate(const nsAString& aSessionId)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aSessionId.IsEmpty());

  // TODO: Terminate the session.

  return NS_OK;
}

NS_IMETHODIMP
PresentationService::RegisterListener(nsIPresentationListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(mListeners.Contains(aListener))) {
    return NS_OK;
  }

  mListeners.AppendElement(aListener);
  return NS_OK;
}

NS_IMETHODIMP
PresentationService::UnregisterListener(nsIPresentationListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());

  mListeners.RemoveElement(aListener);
  return NS_OK;
}

NS_IMETHODIMP
PresentationService::RegisterSessionListener(const nsAString& aSessionId,
                                             nsIPresentationSessionListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aListener);

  PresentationSessionInfo* info = mSessionInfo.Get(aSessionId);
  if (NS_WARN_IF(!info)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  info->SetListener(aListener);
  return NS_OK;
}

NS_IMETHODIMP
PresentationService::UnregisterSessionListener(const nsAString& aSessionId)
{
  MOZ_ASSERT(NS_IsMainThread());

  PresentationSessionInfo* info = mSessionInfo.Get(aSessionId);
  if (info) {
    info->SetListener(nullptr);
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationService::GetExistentSessionIdAtLaunch(nsAString& aSessionId)
{
  // TODO: Return the value based on it's a sender or a receiver.

  return NS_OK;
}

NS_IMETHODIMP
PresentationService::NotifyReceiverReady(const nsAString& aSessionId)
{
  // TODO: Notify the correspondent session info.

  return NS_OK;
}

already_AddRefed<nsIPresentationService>
NS_CreatePresentationService()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIPresentationService> service = new PresentationService();
  return NS_WARN_IF(!static_cast<PresentationService*>(service.get())->Init()) ?
         nullptr : service.forget();
}
