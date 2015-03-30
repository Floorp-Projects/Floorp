/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ipc/PresentationIPCService.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "nsIPresentationControlChannel.h"
#include "nsIPresentationDeviceManager.h"
#include "nsIPresentationDevicePrompt.h"
#include "nsIPresentationListener.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "PresentationService.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {
namespace dom {

/*
 * Implementation of PresentationDeviceRequest
 */

class PresentationDeviceRequest final : public nsIPresentationDeviceRequest
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONDEVICEREQUEST

  PresentationDeviceRequest(const nsAString& aRequestUrl,
                            const nsAString& aId,
                            const nsAString& aOrigin);

private:
  virtual ~PresentationDeviceRequest();

  nsString mRequestUrl;
  nsString mId;
  nsString mOrigin;
};

} // namespace dom
} // namespace mozilla

NS_IMPL_ISUPPORTS(PresentationDeviceRequest, nsIPresentationDeviceRequest)

PresentationDeviceRequest::PresentationDeviceRequest(const nsAString& aRequestUrl,
                                                     const nsAString& aId,
                                                     const nsAString& aOrigin)
  : mRequestUrl(aRequestUrl)
  , mId(aId)
  , mOrigin(aOrigin)
{
  MOZ_ASSERT(!mRequestUrl.IsEmpty());
  MOZ_ASSERT(!mId.IsEmpty());
  MOZ_ASSERT(!mOrigin.IsEmpty());
}

PresentationDeviceRequest::~PresentationDeviceRequest()
{
}

NS_IMETHODIMP
PresentationDeviceRequest::GetOrigin(nsAString& aOrigin)
{
  aOrigin = mOrigin;
  return NS_OK;
}

NS_IMETHODIMP
PresentationDeviceRequest::GetRequestURL(nsAString& aRequestUrl)
{
  aRequestUrl = mRequestUrl;
  return NS_OK;
}

NS_IMETHODIMP
PresentationDeviceRequest::Select(nsIPresentationDevice* aDevice)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aDevice);

  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!service)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Update device in the session info.
  nsRefPtr<PresentationSessionInfo> info =
    static_cast<PresentationService*>(service.get())->GetSessionInfo(mId);
  if (NS_WARN_IF(!info)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  info->SetDevice(aDevice);

  // Establish a control channel. If we failed to do so, the callback is called
  // with an error message.
  nsCOMPtr<nsIPresentationControlChannel> ctrlChannel;
  nsresult rv = aDevice->EstablishControlChannel(mRequestUrl, mId, getter_AddRefs(ctrlChannel));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return info->ReplyError(NS_ERROR_DOM_NETWORK_ERR);
  }

  // Initialize the session info with the control channel.
  rv = info->Init(ctrlChannel);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return info->ReplyError(NS_ERROR_DOM_NETWORK_ERR);
  }

  return NS_OK;
}

NS_IMETHODIMP
PresentationDeviceRequest::Cancel()
{
  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!service)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsRefPtr<PresentationSessionInfo> info =
    static_cast<PresentationService*>(service.get())->GetSessionInfo(mId);
  if (NS_WARN_IF(!info)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return info->ReplyError(NS_ERROR_DOM_PROP_ACCESS_DENIED);
}

/*
 * Implementation of PresentationService
 */

NS_IMPL_ISUPPORTS(PresentationService, nsIPresentationService, nsIObserver)

PresentationService::PresentationService()
  : mIsAvailable(false)
{
}

PresentationService::~PresentationService()
{
  HandleShutdown();
}

bool
PresentationService::Init()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return false;
  }

  nsresult rv = obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }
  rv = obs->AddObserver(this, PRESENTATION_DEVICE_CHANGE_TOPIC, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  nsCOMPtr<nsIPresentationDeviceManager> deviceManager =
    do_GetService(PRESENTATION_DEVICE_MANAGER_CONTRACTID);
  if (NS_WARN_IF(!deviceManager)) {
    return false;
  }

  rv = deviceManager->GetDeviceAvailable(&mIsAvailable);
  return !NS_WARN_IF(NS_FAILED(rv));
}

NS_IMETHODIMP
PresentationService::Observe(nsISupports* aSubject,
                             const char* aTopic,
                             const char16_t* aData)
{
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    HandleShutdown();
    return NS_OK;
  } else if (!strcmp(aTopic, PRESENTATION_DEVICE_CHANGE_TOPIC)) {
    return HandleDeviceChange();
  } else if (!strcmp(aTopic, "profile-after-change")) {
    // It's expected since we add and entry to |kLayoutCategories| in
    // |nsLayoutModule.cpp| to launch this service earlier.
    return NS_OK;
  }

  MOZ_ASSERT(false, "Unexpected topic for PresentationService");
  return NS_ERROR_UNEXPECTED;
}

void
PresentationService::HandleShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  mListeners.Clear();
  mSessionInfo.Clear();

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    obs->RemoveObserver(this, PRESENTATION_DEVICE_CHANGE_TOPIC);
  }
}

nsresult
PresentationService::HandleDeviceChange()
{
  nsCOMPtr<nsIPresentationDeviceManager> deviceManager =
    do_GetService(PRESENTATION_DEVICE_MANAGER_CONTRACTID);
  if (NS_WARN_IF(!deviceManager)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  bool isAvailable;
  nsresult rv = deviceManager->GetDeviceAvailable(&isAvailable);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (isAvailable != mIsAvailable) {
    mIsAvailable = isAvailable;
    NotifyAvailableChange(mIsAvailable);
  }

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

  // Create session info  and set the callback. The callback is called when the
  // request is finished.
  nsRefPtr<PresentationRequesterInfo> info =
    new PresentationRequesterInfo(aUrl, aSessionId, aCallback);
  mSessionInfo.Put(aSessionId, info);

  // Pop up a prompt and ask user to select a device.
  nsCOMPtr<nsIPresentationDevicePrompt> prompt =
    do_GetService(PRESENTATION_DEVICE_PROMPT_CONTRACTID);
  if (NS_WARN_IF(!prompt)) {
    return info->ReplyError(NS_ERROR_DOM_ABORT_ERR);
  }
  nsCOMPtr<nsIPresentationDeviceRequest> request =
    new PresentationDeviceRequest(aUrl, aSessionId, aOrigin);
  nsresult rv = prompt->PromptDeviceSelection(request);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return info->ReplyError(NS_ERROR_DOM_ABORT_ERR);
  }

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

  nsRefPtr<PresentationSessionInfo> info = GetSessionInfo(aSessionId);
  if (NS_WARN_IF(!info)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return info->SetListener(aListener);
}

NS_IMETHODIMP
PresentationService::UnregisterSessionListener(const nsAString& aSessionId)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<PresentationSessionInfo> info = GetSessionInfo(aSessionId);
  if (info) {
    return info->SetListener(nullptr);
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

  nsCOMPtr<nsIPresentationService> service;
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    service = new mozilla::dom::PresentationIPCService();
  } else {
    service = new PresentationService();
    if (NS_WARN_IF(!static_cast<PresentationService*>(service.get())->Init())) {
      return nullptr;
    }
  }

  return service.forget();
}
