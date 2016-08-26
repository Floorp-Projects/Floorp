/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DisplayDeviceProvider.h"

#include "DeviceProviderHelpers.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/Unused.h"
#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "nsIWindowWatcher.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsSimpleURI.h"
#include "nsTCPDeviceInfo.h"
#include "nsThreadUtils.h"

static mozilla::LazyLogModule gDisplayDeviceProviderLog("DisplayDeviceProvider");

#define LOG(format) MOZ_LOG(gDisplayDeviceProviderLog, mozilla::LogLevel::Debug, format)

#define DISPLAY_CHANGED_NOTIFICATION "display-changed"
#define DEFAULT_CHROME_FEATURES_PREF "toolkit.defaultChromeFeatures"
#define CHROME_REMOTE_URL_PREF       "b2g.multiscreen.chrome_remote_url"
#define PREF_PRESENTATION_DISCOVERABLE_RETRY_MS "dom.presentation.discoverable.retry_ms"

namespace mozilla {
namespace dom {
namespace presentation {

/**
 * This wrapper is used to break circular-reference problem.
 */
class DisplayDeviceProviderWrappedListener final
  : public nsIPresentationControlServerListener
{
public:
  NS_DECL_ISUPPORTS
  NS_FORWARD_SAFE_NSIPRESENTATIONCONTROLSERVERLISTENER(mListener)

  explicit DisplayDeviceProviderWrappedListener() = default;

  nsresult SetListener(DisplayDeviceProvider* aListener)
  {
    mListener = aListener;
    return NS_OK;
  }

private:
  virtual ~DisplayDeviceProviderWrappedListener() = default;

  DisplayDeviceProvider* mListener = nullptr;
};

NS_IMPL_ISUPPORTS(DisplayDeviceProviderWrappedListener,
                  nsIPresentationControlServerListener)

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
                     ::EstablishControlChannel(nsIPresentationControlChannel** aControlChannel)
{
  nsresult rv = OpenTopLevelWindow();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<DisplayDeviceProvider> provider = mProvider.get();
  if (NS_WARN_IF(!provider)) {
    return NS_ERROR_FAILURE;
  }
  return provider->Connect(this, aControlChannel);
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

NS_IMETHODIMP
DisplayDeviceProvider::HDMIDisplayDevice::IsRequestedUrlSupported(
                                                 const nsAString& aRequestedUrl,
                                                 bool* aRetVal)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!aRetVal) {
    return NS_ERROR_INVALID_POINTER;
  }

  // 1-UA device only supports HTTP/HTTPS hosted receiver page.
  *aRetVal = DeviceProviderHelpers::IsCommonlySupportedScheme(aRequestedUrl);

  return NS_OK;
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
                  nsIPresentationDeviceProvider,
                  nsIPresentationControlServerListener)

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

  nsresult rv;

  mServerRetryMs = Preferences::GetUint(PREF_PRESENTATION_DISCOVERABLE_RETRY_MS);
  mServerRetryTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  MOZ_ASSERT(obs);

  obs->AddObserver(this, DISPLAY_CHANGED_NOTIFICATION, false);

  mDevice = new HDMIDisplayDevice(this);

  mWrappedListener = new DisplayDeviceProviderWrappedListener();
  rv = mWrappedListener->SetListener(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mPresentationService = do_CreateInstance(PRESENTATION_CONTROL_SERVICE_CONTACT_ID,
                                           &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = StartTCPService();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

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

  AbortServerRetry();

  mInitialized = false;
  mWrappedListener->SetListener(nullptr);
  return NS_OK;
}

nsresult
DisplayDeviceProvider::StartTCPService()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;
  rv =  mPresentationService->SetId(NS_LITERAL_CSTRING("DisplayDeviceProvider"));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint16_t servicePort;
  rv = mPresentationService->GetPort(&servicePort);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  /*
   * If |servicePort| is non-zero, it means PresentationServer is running.
   * Otherwise, we should make it start serving.
   */
  if (servicePort) {
    mPort = servicePort;
    return NS_OK;
  }

  rv = mPresentationService->SetListener(mWrappedListener);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  AbortServerRetry();

  // 1-UA doesn't need encryption.
  rv = mPresentationService->StartServer(/* aEncrypted = */ false,
                                         /* aPort = */ 0);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

void
DisplayDeviceProvider::AbortServerRetry()
{
  if (mIsServerRetrying) {
    mIsServerRetrying = false;
    mServerRetryTimer->Cancel();
  }
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

// nsIPresentationControlServerListener
NS_IMETHODIMP
DisplayDeviceProvider::OnServerReady(uint16_t aPort,
                                     const nsACString& aCertFingerprint)
{
  MOZ_ASSERT(NS_IsMainThread());
  mPort = aPort;

  return NS_OK;
}

NS_IMETHODIMP
DisplayDeviceProvider::OnServerStopped(nsresult aResult)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Try restart server if it is stopped abnormally.
  if (NS_FAILED(aResult)) {
    mIsServerRetrying = true;
    mServerRetryTimer->Init(this, mServerRetryMs, nsITimer::TYPE_ONE_SHOT);
  }

  return NS_OK;
}

NS_IMETHODIMP
DisplayDeviceProvider::OnSessionRequest(nsITCPDeviceInfo* aDeviceInfo,
                                      const nsAString& aUrl,
                                      const nsAString& aPresentationId,
                                      nsIPresentationControlChannel* aControlChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aDeviceInfo);
  MOZ_ASSERT(aControlChannel);

  nsresult rv;

  nsCOMPtr<nsIPresentationDeviceListener> listener;
  rv = GetListener(getter_AddRefs(listener));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(!listener);

  rv = listener->OnSessionRequest(mDevice,
                                  aUrl,
                                  aPresentationId,
                                  aControlChannel);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
DisplayDeviceProvider::OnTerminateRequest(nsITCPDeviceInfo* aDeviceInfo,
                                          const nsAString& aPresentationId,
                                          nsIPresentationControlChannel* aControlChannel,
                                          bool aIsFromReceiver)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aDeviceInfo);
  MOZ_ASSERT(aControlChannel);

  nsresult rv;

  nsCOMPtr<nsIPresentationDeviceListener> listener;
  rv = GetListener(getter_AddRefs(listener));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(!listener);

  rv = listener->OnTerminateRequest(mDevice,
                                    aPresentationId,
                                    aControlChannel,
                                    aIsFromReceiver);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
DisplayDeviceProvider::OnReconnectRequest(nsITCPDeviceInfo* aDeviceInfo,
                                          const nsAString& aUrl,
                                          const nsAString& aPresentationId,
                                          nsIPresentationControlChannel* aControlChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aDeviceInfo);
  MOZ_ASSERT(aControlChannel);

  nsresult rv;

  nsCOMPtr<nsIPresentationDeviceListener> listener;
  rv = GetListener(getter_AddRefs(listener));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(!listener);

  rv = listener->OnReconnectRequest(mDevice,
                                    aUrl,
                                    aPresentationId,
                                    aControlChannel);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

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
  } else if (!strcmp(aTopic, NS_TIMER_CALLBACK_TOPIC)) {
    nsCOMPtr<nsITimer> timer = do_QueryInterface(aSubject);
    if (!timer) {
      return NS_ERROR_UNEXPECTED;
    }

    if (timer == mServerRetryTimer) {
      mIsServerRetrying = false;
      StartTCPService();
    }
  }

  return NS_OK;
}

nsresult
DisplayDeviceProvider::Connect(HDMIDisplayDevice* aDevice,
                               nsIPresentationControlChannel** aControlChannel)
{
  MOZ_ASSERT(aDevice);
  MOZ_ASSERT(mPresentationService);
  NS_ENSURE_ARG_POINTER(aControlChannel);
  *aControlChannel = nullptr;

  nsCOMPtr<nsITCPDeviceInfo> deviceInfo = new TCPDeviceInfo(aDevice->Id(),
                                                            aDevice->Address(),
                                                            mPort,
                                                            EmptyCString());

  return mPresentationService->Connect(deviceInfo, aControlChannel);
}

} // namespace presentation
} // namespace dom
} // namespace mozilla
