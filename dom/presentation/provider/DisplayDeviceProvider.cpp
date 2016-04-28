/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DisplayDeviceProvider.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/unused.h"
#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "nsIWindowWatcher.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsSimpleURI.h"
#include "nsThreadUtils.h"

static mozilla::LazyLogModule gDisplayDeviceProviderLog("DisplayDeviceProvider");

#define LOG(format) MOZ_LOG(gDisplayDeviceProviderLog, mozilla::LogLevel::Debug, format)

#define DISPLAY_CHANGED_NOTIFICATION "display-changed"
#define DEFAULT_CHROME_FEATURES_PREF "toolkit.defaultChromeFeatures"
#define CHROME_REMOTE_URL_PREF       "b2g.multiscreen.chrome_remote_url"

namespace mozilla {
namespace dom {
namespace presentation {

NS_IMPL_ISUPPORTS(DisplayDeviceProvider::HDMIDisplayDevice,
                  nsIPresentationDevice,
                  nsIPresentationLocalDevice)

// nsIPresentationDevice
NS_IMETHODIMP
DisplayDeviceProvider::HDMIDisplayDevice::GetId(nsACString& aId)
{
  aId = mWindowId;
  return NS_OK;
}

NS_IMETHODIMP
DisplayDeviceProvider::HDMIDisplayDevice::GetName(nsACString& aName)
{
  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP
DisplayDeviceProvider::HDMIDisplayDevice::GetType(nsACString& aType)
{
  aType = mType;
  return NS_OK;
}

NS_IMETHODIMP
DisplayDeviceProvider::HDMIDisplayDevice::GetWindowId(nsACString& aWindowId)
{
  aWindowId = mWindowId;
  return NS_OK;
}

NS_IMETHODIMP
DisplayDeviceProvider::HDMIDisplayDevice
                     ::EstablishControlChannel(const nsAString& aUrl,
                                               const nsAString& aPresentationId,
                                               nsIPresentationControlChannel** aControlChannel)
{
  nsresult rv = OpenTopLevelWindow();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<DisplayDeviceProvider> provider = mProvider.get();
  if (NS_WARN_IF(!provider)) {
    return NS_ERROR_FAILURE;
  }
  return provider->RequestSession(this, aUrl, aPresentationId, aControlChannel);
}

NS_IMETHODIMP
DisplayDeviceProvider::HDMIDisplayDevice::Disconnect()
{
  nsresult rv = CloseTopLevelWindow();
  if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
  }
  return NS_OK;;
}

nsresult
DisplayDeviceProvider::HDMIDisplayDevice::OpenTopLevelWindow()
{
  MOZ_ASSERT(!mWindow);

  nsresult rv;
  nsAutoCString flags(Preferences::GetCString(DEFAULT_CHROME_FEATURES_PREF));
  if (flags.IsEmpty()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  flags.AppendLiteral(",mozDisplayId=");
  flags.AppendInt(mScreenId);

  nsAutoCString remoteShellURLString(Preferences::GetCString(CHROME_REMOTE_URL_PREF));
  remoteShellURLString.AppendLiteral("#");
  remoteShellURLString.Append(mWindowId);

  // URI validation
  nsCOMPtr<nsIURI> remoteShellURL;
  rv = NS_NewURI(getter_AddRefs(remoteShellURL), remoteShellURLString);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = remoteShellURL->GetSpec(remoteShellURLString);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIWindowWatcher> ww = do_GetService(NS_WINDOWWATCHER_CONTRACTID);
  MOZ_ASSERT(ww);

  rv = ww->OpenWindow(nullptr,
                      remoteShellURLString.get(),
                      "_blank",
                      flags.get(),
                      nullptr,
                      getter_AddRefs(mWindow));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
DisplayDeviceProvider::HDMIDisplayDevice::CloseTopLevelWindow()
{
  MOZ_ASSERT(mWindow);

  nsCOMPtr<nsPIDOMWindowOuter> piWindow = nsPIDOMWindowOuter::From(mWindow);
  nsresult rv = piWindow->Close();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(DisplayDeviceProvider,
                  nsIObserver,
                  nsIPresentationDeviceProvider)

DisplayDeviceProvider::~DisplayDeviceProvider()
{
  Uninit();
}

nsresult
DisplayDeviceProvider::Init()
{
  // Provider must be initialized only once.
  if (mInitialized) {
    return NS_OK;
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  MOZ_ASSERT(obs);

  obs->AddObserver(this, DISPLAY_CHANGED_NOTIFICATION, false);

  mDevice = new HDMIDisplayDevice(this);

  mInitialized = true;
  return NS_OK;
}

nsresult
DisplayDeviceProvider::Uninit()
{
  // Provider must be deleted only once.
  if (!mInitialized) {
    return NS_OK;
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, DISPLAY_CHANGED_NOTIFICATION);
  }

  // Remove device from device manager when the provider is uninit
  RemoveExternalScreen();

  mInitialized = false;
  return NS_OK;
}

nsresult
DisplayDeviceProvider::AddExternalScreen()
{
  MOZ_ASSERT(mDeviceListener);

  nsresult rv;
  nsCOMPtr<nsIPresentationDeviceListener> listener;
  rv = GetListener(getter_AddRefs(listener));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = listener->AddDevice(mDevice);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
DisplayDeviceProvider::RemoveExternalScreen()
{
  MOZ_ASSERT(mDeviceListener);

  nsresult rv;
  nsCOMPtr<nsIPresentationDeviceListener> listener;
  rv = GetListener(getter_AddRefs(listener));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = listener->RemoveDevice(mDevice);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mDevice->Disconnect();
  return NS_OK;
}

// nsIPresentationDeviceProvider
NS_IMETHODIMP
DisplayDeviceProvider::GetListener(nsIPresentationDeviceListener** aListener)
{
  if (NS_WARN_IF(!aListener)) {
    return NS_ERROR_INVALID_POINTER;
  }

  nsresult rv;
  nsCOMPtr<nsIPresentationDeviceListener> listener =
    do_QueryReferent(mDeviceListener, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
  }

  listener.forget(aListener);

  return NS_OK;
}

NS_IMETHODIMP
DisplayDeviceProvider::SetListener(nsIPresentationDeviceListener* aListener)
{
  mDeviceListener = do_GetWeakReference(aListener);
  nsresult rv = mDeviceListener ? Init() : Uninit();
  if(NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
DisplayDeviceProvider::ForceDiscovery()
{
  return NS_OK;
}

// nsIObserver
NS_IMETHODIMP
DisplayDeviceProvider::Observe(nsISupports* aSubject,
                               const char* aTopic,
                               const char16_t* aData)
{
  if (!strcmp(aTopic, DISPLAY_CHANGED_NOTIFICATION)) {
    nsCOMPtr<nsIDisplayInfo> displayInfo = do_QueryInterface(aSubject);
    MOZ_ASSERT(displayInfo);

    int32_t type;
    bool isConnected;
    displayInfo->GetConnected(&isConnected);
    // XXX The ID is as same as the type of display.
    // See Bug 1138287 and nsScreenManagerGonk::AddScreen() for more detail.
    displayInfo->GetId(&type);

    if (type == DisplayType::DISPLAY_EXTERNAL) {
      nsresult rv = isConnected ? AddExternalScreen() : RemoveExternalScreen();
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  return NS_OK;
}

nsresult
DisplayDeviceProvider::RequestSession(HDMIDisplayDevice* aDevice,
                                    const nsAString& aUrl,
                                    const nsAString& aPresentationId,
                                    nsIPresentationControlChannel** aControlChannel)
{
  // Implement in part 3
  return NS_OK;
}

} // namespace presentation
} // namespace dom
} // namespace mozilla
