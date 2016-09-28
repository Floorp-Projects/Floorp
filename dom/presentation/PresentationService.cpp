/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PresentationService.h"

#include "ipc/PresentationIPCService.h"
#include "mozilla/Services.h"
#include "mozIApplication.h"
#include "nsGlobalWindow.h"
#include "nsIAppsService.h"
#include "nsIMutableArray.h"
#include "nsIObserverService.h"
#include "nsIPresentationControlChannel.h"
#include "nsIPresentationDeviceManager.h"
#include "nsIPresentationDevicePrompt.h"
#include "nsIPresentationListener.h"
#include "nsIPresentationRequestUIGlue.h"
#include "nsIPresentationSessionRequest.h"
#include "nsIPresentationTerminateRequest.h"
#include "nsISupportsPrimitives.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsXPCOMCID.h"
#include "nsXULAppAPI.h"
#include "PresentationLog.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {
namespace dom {

static bool
IsSameDevice(nsIPresentationDevice* aDevice, nsIPresentationDevice* aDeviceAnother) {
  if (!aDevice || !aDeviceAnother) {
    return false;
  }

  nsAutoCString deviceId;
  aDevice->GetId(deviceId);
  nsAutoCString anotherId;
  aDeviceAnother->GetId(anotherId);
  if (!deviceId.Equals(anotherId)) {
    return false;
  }

  nsAutoCString deviceType;
  aDevice->GetType(deviceType);
  nsAutoCString anotherType;
  aDeviceAnother->GetType(anotherType);
  if (!deviceType.Equals(anotherType)) {
    return false;
  }

  return true;
}

static nsresult
ConvertURLArrayHelper(const nsTArray<nsString>& aUrls, nsIArray** aResult)
{
  if (!aResult) {
    return NS_ERROR_INVALID_POINTER;
  }

  *aResult = nullptr;

  nsresult rv;
  nsCOMPtr<nsIMutableArray> urls =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  for (const auto& url : aUrls) {
    nsCOMPtr<nsISupportsString> isupportsString =
      do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = isupportsString->SetData(url);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    rv = urls->AppendElement(isupportsString, false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  urls.forget(aResult);
  return NS_OK;
}

/*
 * Implementation of PresentationDeviceRequest
 */

class PresentationDeviceRequest final : public nsIPresentationDeviceRequest
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONDEVICEREQUEST

  PresentationDeviceRequest(
              const nsTArray<nsString>& aUrls,
              const nsAString& aId,
              const nsAString& aOrigin,
              uint64_t aWindowId,
              nsIDOMEventTarget* aEventTarget,
              nsIPresentationServiceCallback* aCallback,
              nsIPresentationTransportBuilderConstructor* aBuilderConstructor);

private:
  virtual ~PresentationDeviceRequest() = default;
  nsresult CreateSessionInfo(nsIPresentationDevice* aDevice,
                             const nsAString& aSelectedRequestUrl);

  nsTArray<nsString> mRequestUrls;
  nsString mId;
  nsString mOrigin;
  uint64_t mWindowId;
  nsWeakPtr mChromeEventHandler;
  nsCOMPtr<nsIPresentationServiceCallback> mCallback;
  nsCOMPtr<nsIPresentationTransportBuilderConstructor> mBuilderConstructor;
};

LazyLogModule gPresentationLog("Presentation");

} // namespace dom
} // namespace mozilla

NS_IMPL_ISUPPORTS(PresentationDeviceRequest, nsIPresentationDeviceRequest)

PresentationDeviceRequest::PresentationDeviceRequest(
               const nsTArray<nsString>& aUrls,
               const nsAString& aId,
               const nsAString& aOrigin,
               uint64_t aWindowId,
               nsIDOMEventTarget* aEventTarget,
               nsIPresentationServiceCallback* aCallback,
               nsIPresentationTransportBuilderConstructor* aBuilderConstructor)
  : mRequestUrls(aUrls)
  , mId(aId)
  , mOrigin(aOrigin)
  , mWindowId(aWindowId)
  , mChromeEventHandler(do_GetWeakReference(aEventTarget))
  , mCallback(aCallback)
  , mBuilderConstructor(aBuilderConstructor)
{
  MOZ_ASSERT(!mRequestUrls.IsEmpty());
  MOZ_ASSERT(!mId.IsEmpty());
  MOZ_ASSERT(!mOrigin.IsEmpty());
  MOZ_ASSERT(mCallback);
  MOZ_ASSERT(mBuilderConstructor);
}

NS_IMETHODIMP
PresentationDeviceRequest::GetOrigin(nsAString& aOrigin)
{
  aOrigin = mOrigin;
  return NS_OK;
}

NS_IMETHODIMP
PresentationDeviceRequest::GetRequestURLs(nsIArray** aUrls)
{
  return ConvertURLArrayHelper(mRequestUrls, aUrls);
}

NS_IMETHODIMP
PresentationDeviceRequest::GetChromeEventHandler(nsIDOMEventTarget** aChromeEventHandler)
{
  nsCOMPtr<nsIDOMEventTarget> handler(do_QueryReferent(mChromeEventHandler));
  handler.forget(aChromeEventHandler);
  return NS_OK;
}

NS_IMETHODIMP
PresentationDeviceRequest::Select(nsIPresentationDevice* aDevice)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (NS_WARN_IF(!aDevice)) {
    MOZ_ASSERT(false, "|aDevice| should noe be null.");
    mCallback->NotifyError(NS_ERROR_DOM_OPERATION_ERR);
    return NS_ERROR_INVALID_ARG;
  }

  // Select the most suitable URL for starting the presentation.
  nsAutoString selectedRequestUrl;
  for (const auto& url : mRequestUrls) {
    bool isSupported;
    if (NS_SUCCEEDED(aDevice->IsRequestedUrlSupported(url, &isSupported)) &&
        isSupported) {
      selectedRequestUrl.Assign(url);
      break;
    }
  }

  if (selectedRequestUrl.IsEmpty()) {
    return mCallback->NotifyError(NS_ERROR_DOM_NOT_FOUND_ERR);
  }

  if (NS_WARN_IF(NS_FAILED(CreateSessionInfo(aDevice, selectedRequestUrl)))) {
    return mCallback->NotifyError(NS_ERROR_DOM_OPERATION_ERR);
  }

  return mCallback->NotifySuccess(selectedRequestUrl);
}

nsresult
PresentationDeviceRequest::CreateSessionInfo(
                                          nsIPresentationDevice* aDevice,
                                          const nsAString& aSelectedRequestUrl)
{
  nsCOMPtr<nsIPresentationService> service =
    do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!service)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Create the controlling session info
  RefPtr<PresentationSessionInfo> info =
    static_cast<PresentationService*>(service.get())->
      CreateControllingSessionInfo(aSelectedRequestUrl, mId, mWindowId);
  if (NS_WARN_IF(!info)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  info->SetDevice(aDevice);

  // Establish a control channel. If we failed to do so, the callback is called
  // with an error message.
  nsCOMPtr<nsIPresentationControlChannel> ctrlChannel;
  nsresult rv = aDevice->EstablishControlChannel(getter_AddRefs(ctrlChannel));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return info->ReplyError(NS_ERROR_DOM_OPERATION_ERR);
  }

  // Initialize the session info with the control channel.
  rv = info->Init(ctrlChannel);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return info->ReplyError(NS_ERROR_DOM_OPERATION_ERR);
  }

  info->SetTransportBuilderConstructor(mBuilderConstructor);
  return NS_OK;
}

NS_IMETHODIMP
PresentationDeviceRequest::Cancel(nsresult aReason)
{
  return mCallback->NotifyError(aReason);
}

/*
 * Implementation of PresentationService
 */

NS_IMPL_ISUPPORTS(PresentationService,
                  nsIPresentationService,
                  nsIObserver)

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
  rv = obs->AddObserver(this, PRESENTATION_TERMINATE_REQUEST_TOPIC, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }
  rv = obs->AddObserver(this, PRESENTATION_RECONNECT_REQUEST_TOPIC, false);
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
  } else if (!strcmp(aTopic, PRESENTATION_TERMINATE_REQUEST_TOPIC)) {
    nsCOMPtr<nsIPresentationTerminateRequest> request(do_QueryInterface(aSubject));
    if (NS_WARN_IF(!request)) {
      return NS_ERROR_FAILURE;
    }

    return HandleTerminateRequest(request);
  } else if (!strcmp(aTopic, PRESENTATION_RECONNECT_REQUEST_TOPIC)) {
    nsCOMPtr<nsIPresentationSessionRequest> request(do_QueryInterface(aSubject));
    if (NS_WARN_IF(!request)) {
      return NS_ERROR_FAILURE;
    }

    return HandleReconnectRequest(request);
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

  Shutdown();

  mAvailabilityListeners.Clear();
  mSessionInfoAtController.Clear();
  mSessionInfoAtReceiver.Clear();

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    obs->RemoveObserver(this, PRESENTATION_DEVICE_CHANGE_TOPIC);
    obs->RemoveObserver(this, PRESENTATION_SESSION_REQUEST_TOPIC);
    obs->RemoveObserver(this, PRESENTATION_TERMINATE_REQUEST_TOPIC);
    obs->RemoveObserver(this, PRESENTATION_RECONNECT_REQUEST_TOPIC);
  }
}

nsresult
PresentationService::HandleDeviceChange()
{
  PRES_DEBUG("%s\n", __func__);

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
    ctrlChannel->Disconnect(rv);
    return rv;
  }

  nsAutoString sessionId;
  rv = aRequest->GetPresentationId(sessionId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ctrlChannel->Disconnect(rv);
    return rv;
  }

  nsCOMPtr<nsIPresentationDevice> device;
  rv = aRequest->GetDevice(getter_AddRefs(device));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ctrlChannel->Disconnect(rv);
    return rv;
  }

#ifdef MOZ_WIDGET_GONK
  // Verify the existence of the app if necessary.
  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), url);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ctrlChannel->Disconnect(NS_ERROR_DOM_BAD_URI);
    return rv;
  }

  bool isApp;
  rv = uri->SchemeIs("app", &isApp);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ctrlChannel->Disconnect(rv);
    return rv;
  }

  if (NS_WARN_IF(isApp && !IsAppInstalled(uri))) {
    ctrlChannel->Disconnect(NS_ERROR_DOM_NOT_FOUND_ERR);
    return NS_OK;
  }
#endif

  // Create or reuse session info.
  RefPtr<PresentationSessionInfo> info =
    GetSessionInfo(sessionId, nsIPresentationService::ROLE_RECEIVER);

  // This is the case for reconnecting a session.
  // Update the control channel and device of the session info.
  // Call |NotifyResponderReady| to indicate the receiver page is already there.
  if (info) {
    PRES_DEBUG("handle reconnection:id[%s]\n",
               NS_ConvertUTF16toUTF8(sessionId).get());

    info->SetControlChannel(ctrlChannel);
    info->SetDevice(device);
    return static_cast<PresentationPresentingInfo*>(
      info.get())->DoReconnect();
  }

  // This is the case for a new session.
  PRES_DEBUG("handle new session:url[%d], id[%s]\n",
             NS_ConvertUTF16toUTF8(url).get(),
             NS_ConvertUTF16toUTF8(sessionId).get());

  info = new PresentationPresentingInfo(url, sessionId, device);
  rv = info->Init(ctrlChannel);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ctrlChannel->Disconnect(rv);
    return rv;
  }

  mSessionInfoAtReceiver.Put(sessionId, info);

  // Notify the receiver to launch.
  nsCOMPtr<nsIPresentationRequestUIGlue> glue =
    do_CreateInstance(PRESENTATION_REQUEST_UI_GLUE_CONTRACTID);
  if (NS_WARN_IF(!glue)) {
    ctrlChannel->Disconnect(NS_ERROR_DOM_OPERATION_ERR);
    return info->ReplyError(NS_ERROR_DOM_OPERATION_ERR);
  }
  nsCOMPtr<nsISupports> promise;
  rv = glue->SendRequest(url, sessionId, device, getter_AddRefs(promise));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ctrlChannel->Disconnect(rv);
    return info->ReplyError(NS_ERROR_DOM_OPERATION_ERR);
  }
  nsCOMPtr<Promise> realPromise = do_QueryInterface(promise);
  static_cast<PresentationPresentingInfo*>(info.get())->SetPromise(realPromise);

  return NS_OK;
}

nsresult
PresentationService::HandleTerminateRequest(nsIPresentationTerminateRequest* aRequest)
{
  nsCOMPtr<nsIPresentationControlChannel> ctrlChannel;
  nsresult rv = aRequest->GetControlChannel(getter_AddRefs(ctrlChannel));
  if (NS_WARN_IF(NS_FAILED(rv) || !ctrlChannel)) {
    return rv;
  }

  nsAutoString sessionId;
  rv = aRequest->GetPresentationId(sessionId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ctrlChannel->Disconnect(rv);
    return rv;
  }

  nsCOMPtr<nsIPresentationDevice> device;
  rv = aRequest->GetDevice(getter_AddRefs(device));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ctrlChannel->Disconnect(rv);
    return rv;
  }

  bool isFromReceiver;
  rv = aRequest->GetIsFromReceiver(&isFromReceiver);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ctrlChannel->Disconnect(rv);
    return rv;
  }

  RefPtr<PresentationSessionInfo> info;
  if (!isFromReceiver) {
    info = GetSessionInfo(sessionId, nsIPresentationService::ROLE_RECEIVER);
  } else {
    info = GetSessionInfo(sessionId, nsIPresentationService::ROLE_CONTROLLER);
  }
  if (NS_WARN_IF(!info)) {
    // Cannot terminate non-existed session.
    ctrlChannel->Disconnect(NS_ERROR_DOM_OPERATION_ERR);
    return NS_ERROR_DOM_ABORT_ERR;
  }

  // Check if terminate request comes from known device.
  RefPtr<nsIPresentationDevice> knownDevice = info->GetDevice();
  if (NS_WARN_IF(!IsSameDevice(device, knownDevice))) {
    ctrlChannel->Disconnect(NS_ERROR_DOM_OPERATION_ERR);
    return NS_ERROR_DOM_ABORT_ERR;
  }

  PRES_DEBUG("handle termination:id[%s], receiver[%d]\n", __func__,
             sessionId.get(), isFromReceiver);

  return info->OnTerminate(ctrlChannel);
}

nsresult
PresentationService::HandleReconnectRequest(nsIPresentationSessionRequest* aRequest)
{
  nsCOMPtr<nsIPresentationControlChannel> ctrlChannel;
  nsresult rv = aRequest->GetControlChannel(getter_AddRefs(ctrlChannel));
  if (NS_WARN_IF(NS_FAILED(rv) || !ctrlChannel)) {
    return rv;
  }

  nsAutoString sessionId;
  rv = aRequest->GetPresentationId(sessionId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ctrlChannel->Disconnect(rv);
    return rv;
  }

  uint64_t windowId;
  rv = GetWindowIdBySessionIdInternal(sessionId,
                                      nsIPresentationService::ROLE_RECEIVER,
                                      &windowId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ctrlChannel->Disconnect(rv);
    return rv;
  }

  RefPtr<PresentationSessionInfo> info =
    GetSessionInfo(sessionId, nsIPresentationService::ROLE_RECEIVER);
  if (NS_WARN_IF(!info)) {
    // Cannot reconnect non-existed session
    ctrlChannel->Disconnect(NS_ERROR_DOM_OPERATION_ERR);
    return NS_ERROR_DOM_ABORT_ERR;
  }

  nsAutoString url;
  rv = aRequest->GetUrl(url);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    ctrlChannel->Disconnect(rv);
    return rv;
  }

  // Make sure the url is the same as the previous one.
  if (NS_WARN_IF(!info->GetUrl().Equals(url))) {
    ctrlChannel->Disconnect(rv);
    return rv;
  }

  return HandleSessionRequest(aRequest);
}

void
PresentationService::NotifyAvailableChange(bool aIsAvailable)
{
  nsTObserverArray<nsCOMPtr<nsIPresentationAvailabilityListener>>::ForwardIterator iter(mAvailabilityListeners);
  while (iter.HasMore()) {
    nsCOMPtr<nsIPresentationAvailabilityListener> listener = iter.GetNext();
    Unused <<
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
PresentationService::StartSession(
               const nsTArray<nsString>& aUrls,
               const nsAString& aSessionId,
               const nsAString& aOrigin,
               const nsAString& aDeviceId,
               uint64_t aWindowId,
               nsIDOMEventTarget* aEventTarget,
               nsIPresentationServiceCallback* aCallback,
               nsIPresentationTransportBuilderConstructor* aBuilderConstructor)
{
  PRES_DEBUG("%s:id[%s]\n", __func__, NS_ConvertUTF16toUTF8(aSessionId).get());

  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(!aSessionId.IsEmpty());
  MOZ_ASSERT(!aUrls.IsEmpty());

  nsCOMPtr<nsIPresentationDeviceRequest> request =
    new PresentationDeviceRequest(aUrls,
                                  aSessionId,
                                  aOrigin,
                                  aWindowId,
                                  aEventTarget,
                                  aCallback,
                                  aBuilderConstructor);

  if (aDeviceId.IsVoid()) {
    // Pop up a prompt and ask user to select a device.
    nsCOMPtr<nsIPresentationDevicePrompt> prompt =
      do_GetService(PRESENTATION_DEVICE_PROMPT_CONTRACTID);
    if (NS_WARN_IF(!prompt)) {
      return aCallback->NotifyError(NS_ERROR_DOM_OPERATION_ERR);
    }

    nsresult rv = prompt->PromptDeviceSelection(request);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return aCallback->NotifyError(NS_ERROR_DOM_OPERATION_ERR);
    }

    return NS_OK;
  }

  // Find the designated device from available device list.
  nsCOMPtr<nsIPresentationDeviceManager> deviceManager =
    do_GetService(PRESENTATION_DEVICE_MANAGER_CONTRACTID);
  if (NS_WARN_IF(!deviceManager)) {
    return aCallback->NotifyError(NS_ERROR_DOM_OPERATION_ERR);
  }

  nsCOMPtr<nsIArray> presentationUrls;
  if (NS_WARN_IF(NS_FAILED(
    ConvertURLArrayHelper(aUrls, getter_AddRefs(presentationUrls))))) {
    return aCallback->NotifyError(NS_ERROR_DOM_OPERATION_ERR);
  }

  nsCOMPtr<nsIArray> devices;
  nsresult rv = deviceManager->GetAvailableDevices(presentationUrls, getter_AddRefs(devices));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return aCallback->NotifyError(NS_ERROR_DOM_OPERATION_ERR);
  }

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  rv = devices->Enumerate(getter_AddRefs(enumerator));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return aCallback->NotifyError(NS_ERROR_DOM_OPERATION_ERR);
  }

  NS_ConvertUTF16toUTF8 utf8DeviceId(aDeviceId);
  bool hasMore;
  while (NS_SUCCEEDED(enumerator->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> isupports;
    rv = enumerator->GetNext(getter_AddRefs(isupports));

    nsCOMPtr<nsIPresentationDevice> device(do_QueryInterface(isupports));
    MOZ_ASSERT(device);

    nsAutoCString id;
    if (NS_SUCCEEDED(device->GetId(id)) && id.Equals(utf8DeviceId)) {
      request->Select(device);
      return NS_OK;
    }
  }

  // Reject if designated device is not available.
  return aCallback->NotifyError(NS_ERROR_DOM_NOT_FOUND_ERR);
}

already_AddRefed<PresentationSessionInfo>
PresentationService::CreateControllingSessionInfo(const nsAString& aUrl,
                                                  const nsAString& aSessionId,
                                                  uint64_t aWindowId)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (aSessionId.IsEmpty()) {
    return nullptr;
  }

  RefPtr<PresentationSessionInfo> info =
    new PresentationControllingInfo(aUrl, aSessionId);

  mSessionInfoAtController.Put(aSessionId, info);
  AddRespondingSessionId(aWindowId,
                         aSessionId,
                         nsIPresentationService::ROLE_CONTROLLER);
  return info.forget();
}

NS_IMETHODIMP
PresentationService::SendSessionMessage(const nsAString& aSessionId,
                                        uint8_t aRole,
                                        const nsAString& aData)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aData.IsEmpty());
  MOZ_ASSERT(!aSessionId.IsEmpty());
  MOZ_ASSERT(aRole == nsIPresentationService::ROLE_CONTROLLER ||
             aRole == nsIPresentationService::ROLE_RECEIVER);

  RefPtr<PresentationSessionInfo> info = GetSessionInfo(aSessionId, aRole);
  if (NS_WARN_IF(!info)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return info->Send(aData);
}

NS_IMETHODIMP
PresentationService::SendSessionBinaryMsg(const nsAString& aSessionId,
                                          uint8_t aRole,
                                          const nsACString &aData)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aData.IsEmpty());
  MOZ_ASSERT(!aSessionId.IsEmpty());
  MOZ_ASSERT(aRole == nsIPresentationService::ROLE_CONTROLLER ||
             aRole == nsIPresentationService::ROLE_RECEIVER);

  RefPtr<PresentationSessionInfo> info = GetSessionInfo(aSessionId, aRole);
  if (NS_WARN_IF(!info)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return info->SendBinaryMsg(aData);
}

NS_IMETHODIMP
PresentationService::SendSessionBlob(const nsAString& aSessionId,
                                     uint8_t aRole,
                                     nsIDOMBlob* aBlob)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aSessionId.IsEmpty());
  MOZ_ASSERT(aRole == nsIPresentationService::ROLE_CONTROLLER ||
             aRole == nsIPresentationService::ROLE_RECEIVER);
  MOZ_ASSERT(aBlob);

  RefPtr<PresentationSessionInfo> info = GetSessionInfo(aSessionId, aRole);
  if (NS_WARN_IF(!info)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return info->SendBlob(aBlob);
}

NS_IMETHODIMP
PresentationService::CloseSession(const nsAString& aSessionId,
                                  uint8_t aRole,
                                  uint8_t aClosedReason)
{
  PRES_DEBUG("%s:id[%s], reason[%x], role[%d]\n", __func__,
             NS_ConvertUTF16toUTF8(aSessionId).get(), aClosedReason, aRole);

  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aSessionId.IsEmpty());
  MOZ_ASSERT(aRole == nsIPresentationService::ROLE_CONTROLLER ||
             aRole == nsIPresentationService::ROLE_RECEIVER);

  RefPtr<PresentationSessionInfo> info = GetSessionInfo(aSessionId, aRole);
  if (NS_WARN_IF(!info)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (aClosedReason == nsIPresentationService::CLOSED_REASON_WENTAWAY) {
    // Remove nsIPresentationSessionListener since we don't want to dispatch
    // PresentationConnectionClosedEvent if the page is went away.
    info->SetListener(nullptr);
  }

  return info->Close(NS_OK, nsIPresentationSessionListener::STATE_CLOSED);
}

NS_IMETHODIMP
PresentationService::TerminateSession(const nsAString& aSessionId,
                                      uint8_t aRole)
{
  PRES_DEBUG("%s:id[%s], role[%d]\n", __func__,
             NS_ConvertUTF16toUTF8(aSessionId).get(), aRole);

  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aSessionId.IsEmpty());
  MOZ_ASSERT(aRole == nsIPresentationService::ROLE_CONTROLLER ||
             aRole == nsIPresentationService::ROLE_RECEIVER);

  RefPtr<PresentationSessionInfo> info = GetSessionInfo(aSessionId, aRole);
  if (NS_WARN_IF(!info)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return info->Close(NS_OK, nsIPresentationSessionListener::STATE_TERMINATED);
}

NS_IMETHODIMP
PresentationService::ReconnectSession(const nsTArray<nsString>& aUrls,
                                      const nsAString& aSessionId,
                                      uint8_t aRole,
                                      nsIPresentationServiceCallback* aCallback)
{
  PRES_DEBUG("%s:id[%s]\n", __func__, NS_ConvertUTF16toUTF8(aSessionId).get());

  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aSessionId.IsEmpty());
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(!aUrls.IsEmpty());

  if (aRole != nsIPresentationService::ROLE_CONTROLLER) {
    MOZ_ASSERT(false, "Only controller can call ReconnectSession.");
    return NS_ERROR_INVALID_ARG;
  }

  if (NS_WARN_IF(!aCallback)) {
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<PresentationSessionInfo> info = GetSessionInfo(aSessionId, aRole);
  if (NS_WARN_IF(!info)) {
    return aCallback->NotifyError(NS_ERROR_DOM_NOT_FOUND_ERR);
  }

  if (NS_WARN_IF(!aUrls.Contains(info->GetUrl()))) {
    return aCallback->NotifyError(NS_ERROR_DOM_NOT_FOUND_ERR);
  }

  return static_cast<PresentationControllingInfo*>(info.get())->Reconnect(aCallback);
}

NS_IMETHODIMP
PresentationService::BuildTransport(const nsAString& aSessionId,
                                    uint8_t aRole)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aSessionId.IsEmpty());

  if (aRole != nsIPresentationService::ROLE_CONTROLLER) {
    MOZ_ASSERT(false, "Only controller can call BuildTransport.");
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<PresentationSessionInfo> info = GetSessionInfo(aSessionId, aRole);
  if (NS_WARN_IF(!info)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return static_cast<PresentationControllingInfo*>(info.get())->BuildTransport();
}

NS_IMETHODIMP
PresentationService::RegisterAvailabilityListener(nsIPresentationAvailabilityListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mAvailabilityListeners.Contains(aListener)) {
    mAvailabilityListeners.AppendElement(aListener);
  }

  // Leverage availablility change notification to assign
  // the initial value of availability object.
  Unused <<
    NS_WARN_IF(NS_FAILED(aListener->NotifyAvailableChange(mIsAvailable)));

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
                                             uint8_t aRole,
                                             nsIPresentationSessionListener* aListener)
{
  PRES_DEBUG("%s:id[%s], role[%d]\n", __func__,
             NS_ConvertUTF16toUTF8(aSessionId).get(), aRole);

  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aListener);
  MOZ_ASSERT(aRole == nsIPresentationService::ROLE_CONTROLLER ||
             aRole == nsIPresentationService::ROLE_RECEIVER);

  RefPtr<PresentationSessionInfo> info = GetSessionInfo(aSessionId, aRole);
  if (NS_WARN_IF(!info)) {
    // Notify the listener of TERMINATED since no correspondent session info is
    // available possibly due to establishment failure. This would be useful at
    // the receiver side, since a presentation session is created at beginning
    // and here is the place to realize the underlying establishment fails.
    nsresult rv = aListener->NotifyStateChange(aSessionId,
                                               nsIPresentationSessionListener::STATE_TERMINATED,
                                               NS_ERROR_NOT_AVAILABLE);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_ERROR_NOT_AVAILABLE;
  }

  return info->SetListener(aListener);
}

NS_IMETHODIMP
PresentationService::UnregisterSessionListener(const nsAString& aSessionId,
                                               uint8_t aRole)
{
  PRES_DEBUG("%s:id[%s], role[%d]\n", __func__,
             NS_ConvertUTF16toUTF8(aSessionId).get(), aRole);

  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRole == nsIPresentationService::ROLE_CONTROLLER ||
             aRole == nsIPresentationService::ROLE_RECEIVER);

  RefPtr<PresentationSessionInfo> info = GetSessionInfo(aSessionId, aRole);
  if (info) {
    // When content side decide not handling this session anymore, simply
    // close the connection. Session info is kept for reconnection.
    Unused << NS_WARN_IF(NS_FAILED(info->Close(NS_OK, nsIPresentationSessionListener::STATE_CLOSED)));
    return info->SetListener(nullptr);
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationService::RegisterRespondingListener(
  uint64_t aWindowId,
  nsIPresentationRespondingListener* aListener)
{
  PRES_DEBUG("%s:windowId[%lld]\n", __func__, aWindowId);

  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aListener);

  nsCOMPtr<nsIPresentationRespondingListener> listener;
  if (mRespondingListeners.Get(aWindowId, getter_AddRefs(listener))) {
    return (listener == aListener) ? NS_OK : NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  nsTArray<nsString> sessionIdArray;
  nsresult rv = mReceiverSessionIdManager.GetSessionIds(aWindowId,
                                                        sessionIdArray);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  for (const auto& id : sessionIdArray) {
    aListener->NotifySessionConnect(aWindowId, id);
  }

  mRespondingListeners.Put(aWindowId, aListener);
  return NS_OK;
}

NS_IMETHODIMP
PresentationService::UnregisterRespondingListener(uint64_t aWindowId)
{
  PRES_DEBUG("%s:windowId[%lld]\n", __func__, aWindowId);

  MOZ_ASSERT(NS_IsMainThread());

  mRespondingListeners.Remove(aWindowId);
  return NS_OK;
}

NS_IMETHODIMP
PresentationService::NotifyReceiverReady(
               const nsAString& aSessionId,
               uint64_t aWindowId,
               bool aIsLoading,
               nsIPresentationTransportBuilderConstructor* aBuilderConstructor)
{
  PRES_DEBUG("%s:id[%s], windowId[%lld], loading[%d]\n", __func__,
             NS_ConvertUTF16toUTF8(aSessionId).get(), aWindowId, aIsLoading);

  RefPtr<PresentationSessionInfo> info =
    GetSessionInfo(aSessionId, nsIPresentationService::ROLE_RECEIVER);
  if (NS_WARN_IF(!info)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  AddRespondingSessionId(aWindowId,
                         aSessionId,
                         nsIPresentationService::ROLE_RECEIVER);

  if (!aIsLoading) {
    return static_cast<PresentationPresentingInfo*>(
      info.get())->NotifyResponderFailure();
  }

  nsCOMPtr<nsIPresentationRespondingListener> listener;
  if (mRespondingListeners.Get(aWindowId, getter_AddRefs(listener))) {
    nsresult rv = listener->NotifySessionConnect(aWindowId, aSessionId);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  info->SetTransportBuilderConstructor(aBuilderConstructor);
  return static_cast<PresentationPresentingInfo*>(info.get())->NotifyResponderReady();
}

nsresult
PresentationService::NotifyTransportClosed(const nsAString& aSessionId,
                                           uint8_t aRole,
                                           nsresult aReason)
{
  PRES_DEBUG("%s:id[%s], reason[%x], role[%d]\n", __func__,
             NS_ConvertUTF16toUTF8(aSessionId).get(), aReason, aRole);

  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aSessionId.IsEmpty());
  MOZ_ASSERT(aRole == nsIPresentationService::ROLE_CONTROLLER ||
             aRole == nsIPresentationService::ROLE_RECEIVER);

  RefPtr<PresentationSessionInfo> info = GetSessionInfo(aSessionId, aRole);
  if (NS_WARN_IF(!info)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return info->NotifyTransportClosed(aReason);
}

NS_IMETHODIMP
PresentationService::UntrackSessionInfo(const nsAString& aSessionId,
                                        uint8_t aRole)
{
  PRES_DEBUG("%s:id[%s], role[%d]\n", __func__,
             NS_ConvertUTF16toUTF8(aSessionId).get(), aRole);

  MOZ_ASSERT(aRole == nsIPresentationService::ROLE_CONTROLLER ||
             aRole == nsIPresentationService::ROLE_RECEIVER);
  // Remove the session info.
  if (nsIPresentationService::ROLE_CONTROLLER == aRole) {
    mSessionInfoAtController.Remove(aSessionId);
  } else {
    // Terminate receiver page.
    uint64_t windowId;
    nsresult rv = GetWindowIdBySessionIdInternal(aSessionId, aRole, &windowId);
    if (NS_SUCCEEDED(rv)) {
      NS_DispatchToMainThread(NS_NewRunnableFunction([windowId]() -> void {
        PRES_DEBUG("Attempt to close window[%d]\n", windowId);

        if (auto* window = nsGlobalWindow::GetInnerWindowWithId(windowId)) {
          window->Close();
        }
      }));
    }

    mSessionInfoAtReceiver.Remove(aSessionId);
  }

  // Remove the in-process responding info if there's still any.
  RemoveRespondingSessionId(aSessionId, aRole);

  return NS_OK;
}

NS_IMETHODIMP
PresentationService::GetWindowIdBySessionId(const nsAString& aSessionId,
                                            uint8_t aRole,
                                            uint64_t* aWindowId)
{
  return GetWindowIdBySessionIdInternal(aSessionId, aRole, aWindowId);
}

NS_IMETHODIMP
PresentationService::UpdateWindowIdBySessionId(const nsAString& aSessionId,
                                               uint8_t aRole,
                                               const uint64_t aWindowId)
{
  return UpdateWindowIdBySessionIdInternal(aSessionId, aRole, aWindowId);
}

bool
PresentationService::IsSessionAccessible(const nsAString& aSessionId,
                                         const uint8_t aRole,
                                         base::ProcessId aProcessId)
{
  MOZ_ASSERT(aRole == nsIPresentationService::ROLE_CONTROLLER ||
             aRole == nsIPresentationService::ROLE_RECEIVER);
  RefPtr<PresentationSessionInfo> info = GetSessionInfo(aSessionId, aRole);
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
