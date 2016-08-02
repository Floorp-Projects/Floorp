/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PresentationDeviceManager.h"

#include "mozilla/Services.h"
#include "MainThreadUtils.h"
#include "nsCategoryCache.h"
#include "nsCOMPtr.h"
#include "nsIMutableArray.h"
#include "nsIObserverService.h"
#include "nsXULAppAPI.h"
#include "PresentationSessionRequest.h"
#include "PresentationTerminateRequest.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(PresentationDeviceManager,
                  nsIPresentationDeviceManager,
                  nsIPresentationDeviceListener,
                  nsIObserver,
                  nsISupportsWeakReference)

PresentationDeviceManager::PresentationDeviceManager()
{
}

PresentationDeviceManager::~PresentationDeviceManager()
{
  UnloadDeviceProviders();
  mDevices.Clear();
}

void
PresentationDeviceManager::Init()
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  }

  LoadDeviceProviders();
}

void
PresentationDeviceManager::Shutdown()
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  }

  UnloadDeviceProviders();
}

void
PresentationDeviceManager::LoadDeviceProviders()
{
  MOZ_ASSERT(mProviders.IsEmpty());

  nsCategoryCache<nsIPresentationDeviceProvider> providerCache(PRESENTATION_DEVICE_PROVIDER_CATEGORY);
  providerCache.GetEntries(mProviders);

  for (uint32_t i = 0; i < mProviders.Length(); ++i) {
    mProviders[i]->SetListener(this);
  }
}

void
PresentationDeviceManager::UnloadDeviceProviders()
{
  for (uint32_t i = 0; i < mProviders.Length(); ++i) {
    mProviders[i]->SetListener(nullptr);
  }

  mProviders.Clear();
}

void
PresentationDeviceManager::NotifyDeviceChange(nsIPresentationDevice* aDevice,
                                              const char16_t* aType)
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(aDevice,
                         PRESENTATION_DEVICE_CHANGE_TOPIC,
                         aType);
  }
}

// nsIPresentationDeviceManager
NS_IMETHODIMP
PresentationDeviceManager::ForceDiscovery()
{
  MOZ_ASSERT(NS_IsMainThread());

  for (uint32_t i = 0; i < mProviders.Length(); ++i) {
    mProviders[i]->ForceDiscovery();
  }

  return NS_OK;
}

NS_IMETHODIMP
PresentationDeviceManager::AddDeviceProvider(nsIPresentationDeviceProvider* aProvider)
{
  NS_ENSURE_ARG(aProvider);
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(mProviders.Contains(aProvider))) {
    return NS_OK;
  }

  mProviders.AppendElement(aProvider);
  aProvider->SetListener(this);

  return NS_OK;
}

NS_IMETHODIMP
PresentationDeviceManager::RemoveDeviceProvider(nsIPresentationDeviceProvider* aProvider)
{
  NS_ENSURE_ARG(aProvider);
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!mProviders.RemoveElement(aProvider))) {
    return NS_ERROR_FAILURE;
  }

  aProvider->SetListener(nullptr);

  return NS_OK;
}

NS_IMETHODIMP
PresentationDeviceManager::GetDeviceAvailable(bool* aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);
  MOZ_ASSERT(NS_IsMainThread());

  *aRetVal = !mDevices.IsEmpty();

  return NS_OK;
}

NS_IMETHODIMP
PresentationDeviceManager::GetAvailableDevices(nsIArray** aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);
  MOZ_ASSERT(NS_IsMainThread());

  // Bug 1194049: some providers may discontinue discovery after timeout.
  // Call |ForceDiscovery()| here to make sure device lists are updated.
  NS_DispatchToMainThread(
      NewRunnableMethod(this, &PresentationDeviceManager::ForceDiscovery));

  nsCOMPtr<nsIMutableArray> devices = do_CreateInstance(NS_ARRAY_CONTRACTID);
  for (uint32_t i = 0; i < mDevices.Length(); ++i) {
    devices->AppendElement(mDevices[i], false);
  }

  devices.forget(aRetVal);

  return NS_OK;
}

// nsIPresentationDeviceListener
NS_IMETHODIMP
PresentationDeviceManager::AddDevice(nsIPresentationDevice* aDevice)
{
  NS_ENSURE_ARG(aDevice);
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(mDevices.Contains(aDevice))) {
    return NS_ERROR_FAILURE;
  }

  mDevices.AppendElement(aDevice);

  NotifyDeviceChange(aDevice, u"add");

  return NS_OK;
}

NS_IMETHODIMP
PresentationDeviceManager::RemoveDevice(nsIPresentationDevice* aDevice)
{
  NS_ENSURE_ARG(aDevice);
  MOZ_ASSERT(NS_IsMainThread());

  int32_t index = mDevices.IndexOf(aDevice);
  if (NS_WARN_IF(index < 0)) {
    return NS_ERROR_FAILURE;
  }

  mDevices.RemoveElementAt(index);

  NotifyDeviceChange(aDevice, u"remove");

  return NS_OK;
}

NS_IMETHODIMP
PresentationDeviceManager::UpdateDevice(nsIPresentationDevice* aDevice)
{
  NS_ENSURE_ARG(aDevice);
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!mDevices.Contains(aDevice))) {
    return NS_ERROR_FAILURE;
  }

  NotifyDeviceChange(aDevice, u"update");

  return NS_OK;
}

NS_IMETHODIMP
PresentationDeviceManager::OnSessionRequest(nsIPresentationDevice* aDevice,
                                            const nsAString& aUrl,
                                            const nsAString& aPresentationId,
                                            nsIPresentationControlChannel* aControlChannel)
{
  NS_ENSURE_ARG(aDevice);
  NS_ENSURE_ARG(aControlChannel);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE(obs, NS_ERROR_FAILURE);

  RefPtr<PresentationSessionRequest> request =
    new PresentationSessionRequest(aDevice, aUrl, aPresentationId, aControlChannel);
  obs->NotifyObservers(request,
                       PRESENTATION_SESSION_REQUEST_TOPIC,
                       nullptr);

  return NS_OK;
}

NS_IMETHODIMP
PresentationDeviceManager::OnTerminateRequest(nsIPresentationDevice* aDevice,
                                              const nsAString& aPresentationId,
                                              nsIPresentationControlChannel* aControlChannel,
                                              bool aIsFromReceiver)
{
  NS_ENSURE_ARG(aDevice);
  NS_ENSURE_ARG(aControlChannel);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE(obs, NS_ERROR_FAILURE);

  RefPtr<PresentationTerminateRequest> request =
    new PresentationTerminateRequest(aDevice, aPresentationId,
                                     aControlChannel, aIsFromReceiver);
  obs->NotifyObservers(request,
                       PRESENTATION_TERMINATE_REQUEST_TOPIC,
                       nullptr);

  return NS_OK;
}

NS_IMETHODIMP
PresentationDeviceManager::OnReconnectRequest(nsIPresentationDevice* aDevice,
                                              const nsAString& aUrl,
                                              const nsAString& aPresentationId,
                                              nsIPresentationControlChannel* aControlChannel)
{
  NS_ENSURE_ARG(aDevice);
  NS_ENSURE_ARG(aControlChannel);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE(obs, NS_ERROR_FAILURE);

  RefPtr<PresentationSessionRequest> request =
    new PresentationSessionRequest(aDevice, aUrl, aPresentationId, aControlChannel);
  obs->NotifyObservers(request,
                       PRESENTATION_RECONNECT_REQUEST_TOPIC,
                       nullptr);

  return NS_OK;
}

// nsIObserver
NS_IMETHODIMP
PresentationDeviceManager::Observe(nsISupports *aSubject,
                                   const char *aTopic,
                                   const char16_t *aData)
{
  if (!strcmp(aTopic, "profile-after-change")) {
    Init();
  } else if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    Shutdown();
  }

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
