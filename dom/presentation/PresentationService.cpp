/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ipc/PresentationIPCService.h"
#include "mozilla/Services.h"
#include "mozIApplication.h"
#include "nsIAppsService.h"
#include "nsIObserverService.h"
#include "nsIPresentationControlChannel.h"
#include "nsIPresentationDeviceManager.h"
#include "nsIPresentationDevicePrompt.h"
#include "nsIPresentationListener.h"
#include "nsIPresentationRequestUIGlue.h"
#include "nsIPresentationSessionRequest.h"
#include "nsNetUtil.h"
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
    return info->ReplyError(NS_ERROR_DOM_OPERATION_ERR);
  }

  // Initialize the session info with the control channel.
  rv = info->Init(ctrlChannel);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return info->ReplyError(NS_ERROR_DOM_OPERATION_ERR);
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

  return info->ReplyError(NS_ERROR_DOM_ABORT_ERR);
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
  rv = obs->AddObserver(this, PRESENTATION_SESSION_REQUEST_TOPIC, false);
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
  } else if (!strcmp(aTopic, PRESENTATION_SESSION_REQUEST_TOPIC)) {
    nsCOMPtr<nsIPresentationSessionRequest> request(do_QueryInterface(aSubject));
    if (NS_WARN_IF(!request)) {
      return NS_ERROR_FAILURE;
    }

    return HandleSessionRequest(request);
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

  mAvailabilityListeners.Clear();
  mRespondingListeners.Clear();
  mSessionInfo.Clear();
  mRespondingSessionIds.Clear();

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    obs->RemoveObserver(this, PRESENTATION_DEVICE_CHANGE_TOPIC);
    obs->RemoveObserver(this, PRESENTATION_SESSION_REQUEST_TOPIC);
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

nsresult
PresentationService::HandleSessionRequest(nsIPresentationSessionRequest* aRequest)
{
  nsCOMPtr<nsIPresentationControlChannel> ctrlChannel;
  nsresult rv = aRequest->GetControlChannel(getter_AddRefs(ctrlChannel));
  if (NS_WARN_IF(NS_FAILED(rv) || !ctrlChannel)) {
    return rv;
  }

  nsAutoString url;
  rv = aRequest->GetUrl(url);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ctrlChannel->Close(rv);
    return rv;
  }

  nsAutoString sessionId;
  rv = aRequest->GetPresentationId(sessionId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ctrlChannel->Close(rv);
    return rv;
  }

  nsCOMPtr<nsIPresentationDevice> device;
  rv = aRequest->GetDevice(getter_AddRefs(device));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ctrlChannel->Close(rv);
    return rv;
  }

#ifdef MOZ_WIDGET_GONK
  // Verify the existence of the app if necessary.
  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), url);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ctrlChannel->Close(NS_ERROR_DOM_BAD_URI);
    return rv;
  }

  bool isApp;
  rv = uri->SchemeIs("app", &isApp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ctrlChannel->Close(rv);
    return rv;
  }

  if (NS_WARN_IF(isApp && !IsAppInstalled(uri))) {
    ctrlChannel->Close(NS_ERROR_DOM_NOT_FOUND_ERR);
    return NS_OK;
  }
#endif

  // Create or reuse session info.
  nsRefPtr<PresentationSessionInfo> info = GetSessionInfo(sessionId);
  if (NS_WARN_IF(info)) {
    // TODO Bug 1195605. Update here after session join/resume becomes supported.
    ctrlChannel->Close(NS_ERROR_DOM_OPERATION_ERR);
    return NS_ERROR_DOM_ABORT_ERR;
  }

  info = new PresentationPresentingInfo(url, sessionId, device);
  rv = info->Init(ctrlChannel);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ctrlChannel->Close(rv);
    return rv;
  }

  mSessionInfo.Put(sessionId, info);

  // Notify the receiver to launch.
  nsCOMPtr<nsIPresentationRequestUIGlue> glue =
    do_CreateInstance(PRESENTATION_REQUEST_UI_GLUE_CONTRACTID);
  if (NS_WARN_IF(!glue)) {
    ctrlChannel->Close(NS_ERROR_DOM_OPERATION_ERR);
    return info->ReplyError(NS_ERROR_DOM_OPERATION_ERR);
  }
  nsCOMPtr<nsISupports> promise;
  rv = glue->SendRequest(url, sessionId, getter_AddRefs(promise));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ctrlChannel->Close(rv);
    return info->ReplyError(NS_ERROR_DOM_OPERATION_ERR);
  }
  nsCOMPtr<Promise> realPromise = do_QueryInterface(promise);
  static_cast<PresentationPresentingInfo*>(info.get())->SetPromise(realPromise);

  return NS_OK;
}

void
PresentationService::NotifyAvailableChange(bool aIsAvailable)
{
  nsTObserverArray<nsCOMPtr<nsIPresentationAvailabilityListener>>::ForwardIterator iter(mAvailabilityListeners);
  while (iter.HasMore()) {
    nsCOMPtr<nsIPresentationAvailabilityListener> listener = iter.GetNext();
    NS_WARN_IF(NS_FAILED(listener->NotifyAvailableChange(aIsAvailable)));
  }
}

bool
PresentationService::IsAppInstalled(nsIURI* aUri)
{
  nsAutoCString prePath;
  nsresult rv = aUri->GetPrePath(prePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  nsAutoString manifestUrl;
  AppendUTF8toUTF16(prePath, manifestUrl);
  manifestUrl.AppendLiteral("/manifest.webapp");

  nsCOMPtr<nsIAppsService> appsService = do_GetService(APPS_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!appsService)) {
    return false;
  }

  nsCOMPtr<mozIApplication> app;
  appsService->GetAppByManifestURL(manifestUrl, getter_AddRefs(app));
  if (NS_WARN_IF(!app)) {
    return false;
  }

  return true;
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
  nsRefPtr<PresentationSessionInfo> info =
    new PresentationControllingInfo(aUrl, aSessionId, aCallback);
  mSessionInfo.Put(aSessionId, info);

  // Pop up a prompt and ask user to select a device.
  nsCOMPtr<nsIPresentationDevicePrompt> prompt =
    do_GetService(PRESENTATION_DEVICE_PROMPT_CONTRACTID);
  if (NS_WARN_IF(!prompt)) {
    return info->ReplyError(NS_ERROR_DOM_OPERATION_ERR);
  }
  nsCOMPtr<nsIPresentationDeviceRequest> request =
    new PresentationDeviceRequest(aUrl, aSessionId, aOrigin);
  nsresult rv = prompt->PromptDeviceSelection(request);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return info->ReplyError(NS_ERROR_DOM_OPERATION_ERR);
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

  nsRefPtr<PresentationSessionInfo> info = GetSessionInfo(aSessionId);
  if (NS_WARN_IF(!info)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return info->Send(aStream);
}

NS_IMETHODIMP
PresentationService::Terminate(const nsAString& aSessionId)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aSessionId.IsEmpty());

  nsRefPtr<PresentationSessionInfo> info = GetSessionInfo(aSessionId);
  if (NS_WARN_IF(!info)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return info->Close(NS_OK);
}

NS_IMETHODIMP
PresentationService::RegisterAvailabilityListener(nsIPresentationAvailabilityListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(mAvailabilityListeners.Contains(aListener))) {
    return NS_OK;
  }

  mAvailabilityListeners.AppendElement(aListener);
  return NS_OK;
}

NS_IMETHODIMP
PresentationService::UnregisterAvailabilityListener(nsIPresentationAvailabilityListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());

  mAvailabilityListeners.RemoveElement(aListener);
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
    // Notify the listener of TERMINATED since no correspondent session info is
    // available possibly due to establishment failure. This would be useful at
    // the receiver side, since a presentation session is created at beginning
    // and here is the place to realize the underlying establishment fails.
    nsresult rv = aListener->NotifyStateChange(aSessionId,
                                               nsIPresentationSessionListener::STATE_TERMINATED);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
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
    NS_WARN_IF(NS_FAILED(info->Close(NS_OK)));
    UntrackSessionInfo(aSessionId);
    return info->SetListener(nullptr);
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationService::RegisterRespondingListener(uint64_t aWindowId,
                                                nsIPresentationRespondingListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aListener);

  nsCOMPtr<nsIPresentationRespondingListener> listener;
  if (mRespondingListeners.Get(aWindowId, getter_AddRefs(listener))) {
    return (listener == aListener) ? NS_OK : NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  mRespondingListeners.Put(aWindowId, aListener);
  return NS_OK;
}

NS_IMETHODIMP
PresentationService::UnregisterRespondingListener(uint64_t aWindowId)
{
  MOZ_ASSERT(NS_IsMainThread());

  mRespondingListeners.Remove(aWindowId);
  return NS_OK;
}

NS_IMETHODIMP
PresentationService::GetExistentSessionIdAtLaunch(uint64_t aWindowId,
                                                  nsAString& aSessionId)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsString* sessionId = mRespondingSessionIds.Get(aWindowId);
  if (sessionId) {
    aSessionId.Assign(*sessionId);
  } else {
    aSessionId.Truncate();
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationService::NotifyReceiverReady(const nsAString& aSessionId,
                                         uint64_t aWindowId)
{
  nsRefPtr<PresentationSessionInfo> info = GetSessionInfo(aSessionId);
  if (NS_WARN_IF(!info)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Only track the responding info when an actual window ID, which would never
  // be 0, is provided (for an in-process receiver page).
  if (aWindowId != 0) {
    mRespondingSessionIds.Put(aWindowId, new nsAutoString(aSessionId));
    mRespondingWindowIds.Put(aSessionId, aWindowId);
  }

  return static_cast<PresentationPresentingInfo*>(info.get())->NotifyResponderReady();
}

NS_IMETHODIMP
PresentationService::UntrackSessionInfo(const nsAString& aSessionId)
{
  // Remove the session info.
  mSessionInfo.Remove(aSessionId);

  // Remove the in-process responding info if there's still any.
  uint64_t windowId = 0;
  if(mRespondingWindowIds.Get(aSessionId, &windowId)) {
    mRespondingWindowIds.Remove(aSessionId);
    mRespondingSessionIds.Remove(windowId);
  }

  return NS_OK;
}

bool
PresentationService::IsSessionAccessible(const nsAString& aSessionId,
                                         base::ProcessId aProcessId)
{
  nsRefPtr<PresentationSessionInfo> info = GetSessionInfo(aSessionId);
  if (NS_WARN_IF(!info)) {
    return false;
  }
  return info->IsAccessible(aProcessId);
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
