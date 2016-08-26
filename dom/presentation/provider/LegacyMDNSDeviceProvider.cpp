/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LegacyMDNSDeviceProvider.h"

#include "DeviceProviderHelpers.h"
#include "MainThreadUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/Unused.h"
#include "nsComponentManagerUtils.h"
#include "nsIObserverService.h"
#include "nsIWritablePropertyBag2.h"
#include "nsServiceManagerUtils.h"
#include "nsTCPDeviceInfo.h"
#include "nsThreadUtils.h"
#include "nsIPropertyBag2.h"

#define PREF_PRESENTATION_DISCOVERY_LEGACY "dom.presentation.discovery.legacy.enabled"
#define PREF_PRESENTATION_DISCOVERY_TIMEOUT_MS "dom.presentation.discovery.timeout_ms"
#define PREF_PRESENTATION_DEVICE_NAME "dom.presentation.device.name"

#define LEGACY_SERVICE_TYPE "_mozilla_papi._tcp"

#define LEGACY_PRESENTATION_CONTROL_SERVICE_CONTACT_ID "@mozilla.org/presentation/legacy-control-service;1"

static mozilla::LazyLogModule sLegacyMDNSProviderLogModule("LegacyMDNSDeviceProvider");

#undef LOG_I
#define LOG_I(...) MOZ_LOG(sLegacyMDNSProviderLogModule, mozilla::LogLevel::Debug, (__VA_ARGS__))
#undef LOG_E
#define LOG_E(...) MOZ_LOG(sLegacyMDNSProviderLogModule, mozilla::LogLevel::Error, (__VA_ARGS__))

namespace mozilla {
namespace dom {
namespace presentation {
namespace legacy {

static const char* kObservedPrefs[] = {
  PREF_PRESENTATION_DISCOVERY_LEGACY,
  PREF_PRESENTATION_DISCOVERY_TIMEOUT_MS,
  PREF_PRESENTATION_DEVICE_NAME,
  nullptr
};

namespace {

static void
GetAndroidDeviceName(nsACString& aRetVal)
{
  nsCOMPtr<nsIPropertyBag2> infoService = do_GetService("@mozilla.org/system-info;1");
  MOZ_ASSERT(infoService, "Could not find a system info service");

  Unused << NS_WARN_IF(NS_FAILED(infoService->GetPropertyAsACString(
                                   NS_LITERAL_STRING("device"), aRetVal)));
}

} //anonymous namespace

/**
 * This wrapper is used to break circular-reference problem.
 */
class DNSServiceWrappedListener final
  : public nsIDNSServiceDiscoveryListener
  , public nsIDNSServiceResolveListener
{
public:
  NS_DECL_ISUPPORTS
  NS_FORWARD_SAFE_NSIDNSSERVICEDISCOVERYLISTENER(mListener)
  NS_FORWARD_SAFE_NSIDNSSERVICERESOLVELISTENER(mListener)

  explicit DNSServiceWrappedListener() = default;

  nsresult SetListener(LegacyMDNSDeviceProvider* aListener)
  {
    mListener = aListener;
    return NS_OK;
  }

private:
  virtual ~DNSServiceWrappedListener() = default;

  LegacyMDNSDeviceProvider* mListener = nullptr;
};

NS_IMPL_ISUPPORTS(DNSServiceWrappedListener,
                  nsIDNSServiceDiscoveryListener,
                  nsIDNSServiceResolveListener)

NS_IMPL_ISUPPORTS(LegacyMDNSDeviceProvider,
                  nsIPresentationDeviceProvider,
                  nsIDNSServiceDiscoveryListener,
                  nsIDNSServiceResolveListener,
                  nsIObserver)

LegacyMDNSDeviceProvider::~LegacyMDNSDeviceProvider()
{
  Uninit();
}

nsresult
LegacyMDNSDeviceProvider::Init()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mInitialized) {
    return NS_OK;
  }

  nsresult rv;

  mMulticastDNS = do_GetService(DNSSERVICEDISCOVERY_CONTRACT_ID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mWrappedListener = new DNSServiceWrappedListener();
  if (NS_WARN_IF(NS_FAILED(rv = mWrappedListener->SetListener(this)))) {
    return rv;
  }

  mPresentationService = do_CreateInstance(LEGACY_PRESENTATION_CONTROL_SERVICE_CONTACT_ID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mDiscoveryTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  Preferences::AddStrongObservers(this, kObservedPrefs);

  mDiscoveryEnabled = Preferences::GetBool(PREF_PRESENTATION_DISCOVERY_LEGACY);
  mDiscoveryTimeoutMs = Preferences::GetUint(PREF_PRESENTATION_DISCOVERY_TIMEOUT_MS);
  mServiceName = Preferences::GetCString(PREF_PRESENTATION_DEVICE_NAME);

  // FIXME: Bug 1185806 - Provide a common device name setting.
  if (mServiceName.IsEmpty()) {
    GetAndroidDeviceName(mServiceName);
    Unused << Preferences::SetCString(PREF_PRESENTATION_DEVICE_NAME, mServiceName);
  }

  Unused << mPresentationService->SetId(mServiceName);

  if (mDiscoveryEnabled && NS_WARN_IF(NS_FAILED(rv = ForceDiscovery()))) {
    return rv;
  }

  mInitialized = true;
  return NS_OK;
}

nsresult
LegacyMDNSDeviceProvider::Uninit()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mInitialized) {
    return NS_OK;
  }

  ClearDevices();

  Preferences::RemoveObservers(this, kObservedPrefs);

  StopDiscovery(NS_OK);

  mMulticastDNS = nullptr;

  if (mWrappedListener) {
    mWrappedListener->SetListener(nullptr);
    mWrappedListener = nullptr;
  }

  mInitialized = false;
  return NS_OK;
}

nsresult
LegacyMDNSDeviceProvider::StopDiscovery(nsresult aReason)
{
  LOG_I("StopDiscovery (0x%08x)", aReason);

  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDiscoveryTimer);

  Unused << mDiscoveryTimer->Cancel();

  if (mDiscoveryRequest) {
    mDiscoveryRequest->Cancel(aReason);
    mDiscoveryRequest = nullptr;
  }

  return NS_OK;
}

nsresult
LegacyMDNSDeviceProvider::Connect(Device* aDevice,
                                  nsIPresentationControlChannel** aRetVal)
{
  MOZ_ASSERT(aDevice);
  MOZ_ASSERT(mPresentationService);

  RefPtr<TCPDeviceInfo> deviceInfo = new TCPDeviceInfo(aDevice->Id(),
                                                       aDevice->Address(),
                                                       aDevice->Port(),
                                                       EmptyCString());

  return mPresentationService->Connect(deviceInfo, aRetVal);
}

nsresult
LegacyMDNSDeviceProvider::AddDevice(const nsACString& aId,
                                    const nsACString& aServiceName,
                                    const nsACString& aServiceType,
                                    const nsACString& aAddress,
                                    const uint16_t aPort)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPresentationService);

  RefPtr<Device> device = new Device(aId, /* ID */
                                     aServiceName,
                                     aServiceType,
                                     aAddress,
                                     aPort,
                                     DeviceState::eActive,
                                     this);

  nsCOMPtr<nsIPresentationDeviceListener> listener;
  if (NS_SUCCEEDED(GetListener(getter_AddRefs(listener))) && listener) {
    Unused << listener->AddDevice(device);
  }

  mDevices.AppendElement(device);

  return NS_OK;
}

nsresult
LegacyMDNSDeviceProvider::UpdateDevice(const uint32_t aIndex,
                                       const nsACString& aServiceName,
                                       const nsACString& aServiceType,
                                       const nsACString& aAddress,
                                       const uint16_t aPort)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPresentationService);

  if (NS_WARN_IF(aIndex >= mDevices.Length())) {
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<Device> device = mDevices[aIndex];
  device->Update(aServiceName, aServiceType, aAddress, aPort);
  device->ChangeState(DeviceState::eActive);

  nsCOMPtr<nsIPresentationDeviceListener> listener;
  if (NS_SUCCEEDED(GetListener(getter_AddRefs(listener))) && listener) {
    Unused << listener->UpdateDevice(device);
  }

  return NS_OK;
}

nsresult
LegacyMDNSDeviceProvider::RemoveDevice(const uint32_t aIndex)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPresentationService);

  if (NS_WARN_IF(aIndex >= mDevices.Length())) {
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<Device> device = mDevices[aIndex];

  LOG_I("RemoveDevice: %s", device->Id().get());
  mDevices.RemoveElementAt(aIndex);

  nsCOMPtr<nsIPresentationDeviceListener> listener;
  if (NS_SUCCEEDED(GetListener(getter_AddRefs(listener))) && listener) {
    Unused << listener->RemoveDevice(device);
  }

  return NS_OK;
}

bool
LegacyMDNSDeviceProvider::FindDeviceById(const nsACString& aId,
                                         uint32_t& aIndex)
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<Device> device = new Device(aId,
                                     /* aName = */ EmptyCString(),
                                     /* aType = */ EmptyCString(),
                                     /* aHost = */ EmptyCString(),
                                     /* aPort = */ 0,
                                     /* aState = */ DeviceState::eUnknown,
                                     /* aProvider = */ nullptr);
  size_t index = mDevices.IndexOf(device, 0, DeviceIdComparator());

  if (index == mDevices.NoIndex) {
    return false;
  }

  aIndex = index;
  return true;
}

bool
LegacyMDNSDeviceProvider::FindDeviceByAddress(const nsACString& aAddress,
                                              uint32_t& aIndex)
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<Device> device = new Device(/* aId = */ EmptyCString(),
                                     /* aName = */ EmptyCString(),
                                     /* aType = */ EmptyCString(),
                                     aAddress,
                                     /* aPort = */ 0,
                                     /* aState = */ DeviceState::eUnknown,
                                     /* aProvider = */ nullptr);
  size_t index = mDevices.IndexOf(device, 0, DeviceAddressComparator());

  if (index == mDevices.NoIndex) {
    return false;
  }

  aIndex = index;
  return true;
}

void
LegacyMDNSDeviceProvider::MarkAllDevicesUnknown()
{
  MOZ_ASSERT(NS_IsMainThread());

  for (auto& device : mDevices) {
    device->ChangeState(DeviceState::eUnknown);
  }
}

void
LegacyMDNSDeviceProvider::ClearUnknownDevices()
{
  MOZ_ASSERT(NS_IsMainThread());

  size_t i = mDevices.Length();
  while (i > 0) {
    --i;
    if (mDevices[i]->State() == DeviceState::eUnknown) {
      NS_WARN_IF(NS_FAILED(RemoveDevice(i)));
    }
  }
}

void
LegacyMDNSDeviceProvider::ClearDevices()
{
  MOZ_ASSERT(NS_IsMainThread());

  size_t i = mDevices.Length();
  while (i > 0) {
    --i;
    NS_WARN_IF(NS_FAILED(RemoveDevice(i)));
  }
}

// nsIPresentationDeviceProvider
NS_IMETHODIMP
LegacyMDNSDeviceProvider::GetListener(nsIPresentationDeviceListener** aListener)
{
  MOZ_ASSERT(NS_IsMainThread());

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
LegacyMDNSDeviceProvider::SetListener(nsIPresentationDeviceListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());

  mDeviceListener = do_GetWeakReference(aListener);

  nsresult rv;
  if (mDeviceListener) {
    if (NS_WARN_IF(NS_FAILED(rv = Init()))) {
      return rv;
    }
  } else {
    if (NS_WARN_IF(NS_FAILED(rv = Uninit()))) {
      return rv;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
LegacyMDNSDeviceProvider::ForceDiscovery()
{
  LOG_I("ForceDiscovery (%d)", mDiscoveryEnabled);
  MOZ_ASSERT(NS_IsMainThread());

  if (!mDiscoveryEnabled) {
    return NS_OK;
  }

  MOZ_ASSERT(mDiscoveryTimer);
  MOZ_ASSERT(mMulticastDNS);

  // if it's already discovering, extend existing discovery timeout.
  nsresult rv;
  if (mIsDiscovering) {
    Unused << mDiscoveryTimer->Cancel();

    if (NS_WARN_IF(NS_FAILED( rv = mDiscoveryTimer->Init(this,
        mDiscoveryTimeoutMs,
        nsITimer::TYPE_ONE_SHOT)))) {
        return rv;
    }
    return NS_OK;
  }

  StopDiscovery(NS_OK);

  if (NS_WARN_IF(NS_FAILED(rv = mMulticastDNS->StartDiscovery(
      NS_LITERAL_CSTRING(LEGACY_SERVICE_TYPE),
      mWrappedListener,
      getter_AddRefs(mDiscoveryRequest))))) {
    return rv;
  }

  return NS_OK;
}

// nsIDNSServiceDiscoveryListener
NS_IMETHODIMP
LegacyMDNSDeviceProvider::OnDiscoveryStarted(const nsACString& aServiceType)
{
  LOG_I("OnDiscoveryStarted");
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDiscoveryTimer);

  MarkAllDevicesUnknown();

  nsresult rv;
  if (NS_WARN_IF(NS_FAILED(rv = mDiscoveryTimer->Init(this,
                                                      mDiscoveryTimeoutMs,
                                                      nsITimer::TYPE_ONE_SHOT)))) {
    return rv;
  }

  mIsDiscovering = true;

  return NS_OK;
}

NS_IMETHODIMP
LegacyMDNSDeviceProvider::OnDiscoveryStopped(const nsACString& aServiceType)
{
  LOG_I("OnDiscoveryStopped");
  MOZ_ASSERT(NS_IsMainThread());

  ClearUnknownDevices();

  mIsDiscovering = false;

  return NS_OK;
}

NS_IMETHODIMP
LegacyMDNSDeviceProvider::OnServiceFound(nsIDNSServiceInfo* aServiceInfo)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!aServiceInfo)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv ;

  nsAutoCString serviceName;
  if (NS_WARN_IF(NS_FAILED(rv = aServiceInfo->GetServiceName(serviceName)))) {
    return rv;
  }

  LOG_I("OnServiceFound: %s", serviceName.get());

  if (mMulticastDNS) {
    if (NS_WARN_IF(NS_FAILED(rv = mMulticastDNS->ResolveService(
        aServiceInfo, mWrappedListener)))) {
      return rv;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
LegacyMDNSDeviceProvider::OnServiceLost(nsIDNSServiceInfo* aServiceInfo)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!aServiceInfo)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;

  nsAutoCString serviceName;
  if (NS_WARN_IF(NS_FAILED(rv = aServiceInfo->GetServiceName(serviceName)))) {
    return rv;
  }

  LOG_I("OnServiceLost: %s", serviceName.get());

  nsAutoCString host;
  if (NS_WARN_IF(NS_FAILED(rv = aServiceInfo->GetHost(host)))) {
    return rv;
  }

  uint32_t index;
  if (!FindDeviceById(host, index)) {
    // given device was not found
    return NS_OK;
  }

  if (NS_WARN_IF(NS_FAILED(rv = RemoveDevice(index)))) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
LegacyMDNSDeviceProvider::OnStartDiscoveryFailed(const nsACString& aServiceType,
                                                 int32_t aErrorCode)
{
  LOG_E("OnStartDiscoveryFailed: %d", aErrorCode);
  MOZ_ASSERT(NS_IsMainThread());

  return NS_OK;
}

NS_IMETHODIMP
LegacyMDNSDeviceProvider::OnStopDiscoveryFailed(const nsACString& aServiceType,
                                                int32_t aErrorCode)
{
  LOG_E("OnStopDiscoveryFailed: %d", aErrorCode);
  MOZ_ASSERT(NS_IsMainThread());

  return NS_OK;
}

// nsIDNSServiceResolveListener
NS_IMETHODIMP
LegacyMDNSDeviceProvider::OnServiceResolved(nsIDNSServiceInfo* aServiceInfo)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!aServiceInfo)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;

  nsAutoCString serviceName;
  if (NS_WARN_IF(NS_FAILED(rv = aServiceInfo->GetServiceName(serviceName)))) {
    return rv;
  }

  LOG_I("OnServiceResolved: %s", serviceName.get());

  nsAutoCString host;
  if (NS_WARN_IF(NS_FAILED(rv = aServiceInfo->GetHost(host)))) {
    return rv;
  }

  nsAutoCString address;
  if (NS_WARN_IF(NS_FAILED(rv = aServiceInfo->GetAddress(address)))) {
    return rv;
  }

  uint16_t port;
  if (NS_WARN_IF(NS_FAILED(rv = aServiceInfo->GetPort(&port)))) {
    return rv;
  }

  nsAutoCString serviceType;
  if (NS_WARN_IF(NS_FAILED(rv = aServiceInfo->GetServiceType(serviceType)))) {
    return rv;
  }

  uint32_t index;
  if (FindDeviceById(host, index)) {
    return UpdateDevice(index,
                        serviceName,
                        serviceType,
                        address,
                        port);
  } else {
    return AddDevice(host,
                     serviceName,
                     serviceType,
                     address,
                     port);
  }

  return NS_OK;
}

NS_IMETHODIMP
LegacyMDNSDeviceProvider::OnResolveFailed(nsIDNSServiceInfo* aServiceInfo,
                                          int32_t aErrorCode)
{
  LOG_E("OnResolveFailed: %d", aErrorCode);
  MOZ_ASSERT(NS_IsMainThread());

  return NS_OK;
}

// nsIObserver
NS_IMETHODIMP
LegacyMDNSDeviceProvider::Observe(nsISupports* aSubject,
                                  const char* aTopic,
                                  const char16_t* aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ConvertUTF16toUTF8 data(aData);
  LOG_I("Observe: topic = %s, data = %s", aTopic, data.get());

  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    if (data.EqualsLiteral(PREF_PRESENTATION_DISCOVERY_LEGACY)) {
      OnDiscoveryChanged(Preferences::GetBool(PREF_PRESENTATION_DISCOVERY_LEGACY));
    } else if (data.EqualsLiteral(PREF_PRESENTATION_DISCOVERY_TIMEOUT_MS)) {
      OnDiscoveryTimeoutChanged(Preferences::GetUint(PREF_PRESENTATION_DISCOVERY_TIMEOUT_MS));
    } else if (data.EqualsLiteral(PREF_PRESENTATION_DEVICE_NAME)) {
      nsAdoptingCString newServiceName = Preferences::GetCString(PREF_PRESENTATION_DEVICE_NAME);
      if (!mServiceName.Equals(newServiceName)) {
        OnServiceNameChanged(newServiceName);
      }
    }
  } else if (!strcmp(aTopic, NS_TIMER_CALLBACK_TOPIC)) {
    StopDiscovery(NS_OK);
  }

  return NS_OK;
}

nsresult
LegacyMDNSDeviceProvider::OnDiscoveryChanged(bool aEnabled)
{
  LOG_I("DiscoveryEnabled = %d\n", aEnabled);
  MOZ_ASSERT(NS_IsMainThread());

  mDiscoveryEnabled = aEnabled;

  if (mDiscoveryEnabled) {
    return ForceDiscovery();
  }

  return StopDiscovery(NS_OK);
}

nsresult
LegacyMDNSDeviceProvider::OnDiscoveryTimeoutChanged(uint32_t aTimeoutMs)
{
  LOG_I("OnDiscoveryTimeoutChanged = %d\n", aTimeoutMs);
  MOZ_ASSERT(NS_IsMainThread());

  mDiscoveryTimeoutMs = aTimeoutMs;

  return NS_OK;
}

nsresult
LegacyMDNSDeviceProvider::OnServiceNameChanged(const nsACString& aServiceName)
{
  LOG_I("serviceName = %s\n", PromiseFlatCString(aServiceName).get());
  MOZ_ASSERT(NS_IsMainThread());

  mServiceName = aServiceName;
  mPresentationService->SetId(mServiceName);

  return NS_OK;
}

// LegacyMDNSDeviceProvider::Device
NS_IMPL_ISUPPORTS(LegacyMDNSDeviceProvider::Device,
                  nsIPresentationDevice)

// nsIPresentationDevice
NS_IMETHODIMP
LegacyMDNSDeviceProvider::Device::GetId(nsACString& aId)
{
  aId = mId;

  return NS_OK;
}

NS_IMETHODIMP
LegacyMDNSDeviceProvider::Device::GetName(nsACString& aName)
{
  aName = mName;

  return NS_OK;
}

NS_IMETHODIMP
LegacyMDNSDeviceProvider::Device::GetType(nsACString& aType)
{
  aType = mType;

  return NS_OK;
}

NS_IMETHODIMP
LegacyMDNSDeviceProvider::Device::EstablishControlChannel(
                                        nsIPresentationControlChannel** aRetVal)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->Connect(this, aRetVal);
}

NS_IMETHODIMP
LegacyMDNSDeviceProvider::Device::Disconnect()
{
  // No need to do anything when disconnect.
  return NS_OK;
}

NS_IMETHODIMP
LegacyMDNSDeviceProvider::Device::IsRequestedUrlSupported(
                                                 const nsAString& aRequestedUrl,
                                                 bool* aRetVal)
{
  if (!aRetVal) {
    return NS_ERROR_INVALID_POINTER;
  }

  // Legacy TV 2.5 device only support a fixed set of presentation Apps.
  *aRetVal = DeviceProviderHelpers::IsFxTVSupportedAppUrl(aRequestedUrl);

  return NS_OK;
}

} // namespace legacy
} // namespace presentation
} // namespace dom
} // namespace mozilla
