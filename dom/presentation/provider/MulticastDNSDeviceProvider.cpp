/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MulticastDNSDeviceProvider.h"

#include "DeviceProviderHelpers.h"
#include "MainThreadUtils.h"
#include "mozilla/IntegerPrintfMacros.h"
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

#ifdef MOZ_WIDGET_ANDROID
#include "nsIPropertyBag2.h"
#endif // MOZ_WIDGET_ANDROID

#define PREF_PRESENTATION_DISCOVERY "dom.presentation.discovery.enabled"
#define PREF_PRESENTATION_DISCOVERY_TIMEOUT_MS "dom.presentation.discovery.timeout_ms"
#define PREF_PRESENTATION_DISCOVERABLE "dom.presentation.discoverable"
#define PREF_PRESENTATION_DISCOVERABLE_ENCRYPTED "dom.presentation.discoverable.encrypted"
#define PREF_PRESENTATION_DISCOVERABLE_RETRY_MS "dom.presentation.discoverable.retry_ms"
#define PREF_PRESENTATION_DEVICE_NAME "dom.presentation.device.name"

#define SERVICE_TYPE "_presentation-ctrl._tcp"
#define PROTOCOL_VERSION_TAG "version"
#define CERT_FINGERPRINT_TAG "certFingerprint"

static mozilla::LazyLogModule sMulticastDNSProviderLogModule("MulticastDNSDeviceProvider");

#undef LOG_I
#define LOG_I(...) MOZ_LOG(sMulticastDNSProviderLogModule, mozilla::LogLevel::Debug, (__VA_ARGS__))
#undef LOG_E
#define LOG_E(...) MOZ_LOG(sMulticastDNSProviderLogModule, mozilla::LogLevel::Error, (__VA_ARGS__))

namespace mozilla {
namespace dom {
namespace presentation {

static const char* kObservedPrefs[] = {
  PREF_PRESENTATION_DISCOVERY,
  PREF_PRESENTATION_DISCOVERY_TIMEOUT_MS,
  PREF_PRESENTATION_DISCOVERABLE,
  PREF_PRESENTATION_DEVICE_NAME,
  nullptr
};

namespace {

#ifdef MOZ_WIDGET_ANDROID
static void
GetAndroidDeviceName(nsACString& aRetVal)
{
  nsCOMPtr<nsIPropertyBag2> infoService = do_GetService("@mozilla.org/system-info;1");
  MOZ_ASSERT(infoService, "Could not find a system info service");

  Unused << NS_WARN_IF(NS_FAILED(infoService->GetPropertyAsACString(
                                   NS_LITERAL_STRING("device"), aRetVal)));
}
#endif // MOZ_WIDGET_ANDROID

} //anonymous namespace

/**
 * This wrapper is used to break circular-reference problem.
 */
class DNSServiceWrappedListener final
  : public nsIDNSServiceDiscoveryListener
  , public nsIDNSRegistrationListener
  , public nsIDNSServiceResolveListener
  , public nsIPresentationControlServerListener
{
public:
  NS_DECL_ISUPPORTS
  NS_FORWARD_SAFE_NSIDNSSERVICEDISCOVERYLISTENER(mListener)
  NS_FORWARD_SAFE_NSIDNSREGISTRATIONLISTENER(mListener)
  NS_FORWARD_SAFE_NSIDNSSERVICERESOLVELISTENER(mListener)
  NS_FORWARD_SAFE_NSIPRESENTATIONCONTROLSERVERLISTENER(mListener)

  explicit DNSServiceWrappedListener() = default;

  nsresult SetListener(MulticastDNSDeviceProvider* aListener)
  {
    mListener = aListener;
    return NS_OK;
  }

private:
  virtual ~DNSServiceWrappedListener() = default;

  MulticastDNSDeviceProvider* mListener = nullptr;
};

NS_IMPL_ISUPPORTS(DNSServiceWrappedListener,
                  nsIDNSServiceDiscoveryListener,
                  nsIDNSRegistrationListener,
                  nsIDNSServiceResolveListener,
                  nsIPresentationControlServerListener)

NS_IMPL_ISUPPORTS(MulticastDNSDeviceProvider,
                  nsIPresentationDeviceProvider,
                  nsIDNSServiceDiscoveryListener,
                  nsIDNSRegistrationListener,
                  nsIDNSServiceResolveListener,
                  nsIPresentationControlServerListener,
                  nsIObserver)

MulticastDNSDeviceProvider::~MulticastDNSDeviceProvider()
{
  Uninit();
}

nsresult
MulticastDNSDeviceProvider::Init()
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

  mPresentationService = do_CreateInstance(PRESENTATION_CONTROL_SERVICE_CONTACT_ID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mDiscoveryTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mServerRetryTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  Preferences::AddStrongObservers(this, kObservedPrefs);

  mDiscoveryEnabled = Preferences::GetBool(PREF_PRESENTATION_DISCOVERY);
  mDiscoveryTimeoutMs = Preferences::GetUint(PREF_PRESENTATION_DISCOVERY_TIMEOUT_MS);
  mDiscoverable = Preferences::GetBool(PREF_PRESENTATION_DISCOVERABLE);
  mDiscoverableEncrypted = Preferences::GetBool(PREF_PRESENTATION_DISCOVERABLE_ENCRYPTED);
  mServerRetryMs = Preferences::GetUint(PREF_PRESENTATION_DISCOVERABLE_RETRY_MS);
  mServiceName = Preferences::GetCString(PREF_PRESENTATION_DEVICE_NAME);

#ifdef MOZ_WIDGET_ANDROID
  // FIXME: Bug 1185806 - Provide a common device name setting.
  if (mServiceName.IsEmpty()) {
    GetAndroidDeviceName(mServiceName);
    Unused << Preferences::SetCString(PREF_PRESENTATION_DEVICE_NAME, mServiceName);
  }
#endif // MOZ_WIDGET_ANDROID

  Unused << mPresentationService->SetId(mServiceName);

  if (mDiscoveryEnabled && NS_WARN_IF(NS_FAILED(rv = ForceDiscovery()))) {
    return rv;
  }

  if (mDiscoverable && NS_WARN_IF(NS_FAILED(rv = StartServer()))) {
    return rv;
  }

  mInitialized = true;
  return NS_OK;
}

nsresult
MulticastDNSDeviceProvider::Uninit()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mInitialized) {
    return NS_OK;
  }

  ClearDevices();

  Preferences::RemoveObservers(this, kObservedPrefs);

  StopDiscovery(NS_OK);
  StopServer();

  mMulticastDNS = nullptr;

  if (mWrappedListener) {
    mWrappedListener->SetListener(nullptr);
    mWrappedListener = nullptr;
  }

  mInitialized = false;
  return NS_OK;
}

nsresult
MulticastDNSDeviceProvider::StartServer()
{
  LOG_I("StartServer: %s (%d)", mServiceName.get(), mDiscoverable);
  MOZ_ASSERT(NS_IsMainThread());

  if (!mDiscoverable) {
    return NS_OK;
  }

  nsresult rv;

  uint16_t servicePort;
  if (NS_WARN_IF(NS_FAILED(rv = mPresentationService->GetPort(&servicePort)))) {
    return rv;
  }

  /**
    * If |servicePort| is non-zero, it means PresentationControlService is running.
    * Otherwise, we should make it start serving.
    */
  if (servicePort) {
    return RegisterMDNSService();
  }

  if (NS_WARN_IF(NS_FAILED(rv = mPresentationService->SetListener(mWrappedListener)))) {
    return rv;
  }

  AbortServerRetry();

  if (NS_WARN_IF(NS_FAILED(rv = mPresentationService->StartServer(mDiscoverableEncrypted, 0)))) {
    return rv;
  }

  return NS_OK;
}

nsresult
MulticastDNSDeviceProvider::StopServer()
{
  LOG_I("StopServer: %s", mServiceName.get());
  MOZ_ASSERT(NS_IsMainThread());

  UnregisterMDNSService(NS_OK);

  AbortServerRetry();

  if (mPresentationService) {
    mPresentationService->SetListener(nullptr);
    mPresentationService->Close();
  }

  return NS_OK;
}

void
MulticastDNSDeviceProvider::AbortServerRetry()
{
  if (mIsServerRetrying) {
    mIsServerRetrying = false;
    mServerRetryTimer->Cancel();
  }
}

nsresult
MulticastDNSDeviceProvider::RegisterMDNSService()
{
  LOG_I("RegisterMDNSService: %s", mServiceName.get());

  if (!mDiscoverable) {
    return NS_OK;
  }

  // Cancel on going service registration.
  UnregisterMDNSService(NS_OK);

  nsresult rv;

  uint16_t servicePort;
  if (NS_FAILED(rv = mPresentationService->GetPort(&servicePort)) ||
      !servicePort) {
    // Abort service registration if server port is not available.
    return rv;
  }

  /**
   * Register the presentation control channel server as an mDNS service.
   */
  nsCOMPtr<nsIDNSServiceInfo> serviceInfo =
    do_CreateInstance(DNSSERVICEINFO_CONTRACT_ID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (NS_WARN_IF(NS_FAILED(rv = serviceInfo->SetServiceType(
      NS_LITERAL_CSTRING(SERVICE_TYPE))))) {
    return rv;
  }
  if (NS_WARN_IF(NS_FAILED(rv = serviceInfo->SetServiceName(mServiceName)))) {
    return rv;
  }
  if (NS_WARN_IF(NS_FAILED(rv = serviceInfo->SetPort(servicePort)))) {
    return rv;
  }

  nsCOMPtr<nsIWritablePropertyBag2> propBag =
    do_CreateInstance("@mozilla.org/hash-property-bag;1");
  MOZ_ASSERT(propBag);

  uint32_t version;
  rv = mPresentationService->GetVersion(&version);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  rv = propBag->SetPropertyAsUint32(NS_LITERAL_STRING(PROTOCOL_VERSION_TAG),
                                    version);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  if (mDiscoverableEncrypted) {
    nsAutoCString certFingerprint;
    rv = mPresentationService->GetCertFingerprint(certFingerprint);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    rv = propBag->SetPropertyAsACString(NS_LITERAL_STRING(CERT_FINGERPRINT_TAG),
                                        certFingerprint);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  if (NS_WARN_IF(NS_FAILED(rv = serviceInfo->SetAttributes(propBag)))) {
    return rv;
  }

  return mMulticastDNS->RegisterService(serviceInfo,
                                        mWrappedListener,
                                        getter_AddRefs(mRegisterRequest));
}

nsresult
MulticastDNSDeviceProvider::UnregisterMDNSService(nsresult aReason)
{
  LOG_I("UnregisterMDNSService: %s (0x%08" PRIx32 ")", mServiceName.get(),
        static_cast<uint32_t>(aReason));
  MOZ_ASSERT(NS_IsMainThread());

  if (mRegisterRequest) {
    mRegisterRequest->Cancel(aReason);
    mRegisterRequest = nullptr;
  }

  return NS_OK;
}

nsresult
MulticastDNSDeviceProvider::StopDiscovery(nsresult aReason)
{
  LOG_I("StopDiscovery (0x%08" PRIx32 ")", static_cast<uint32_t>(aReason));

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
MulticastDNSDeviceProvider::Connect(Device* aDevice,
                                    nsIPresentationControlChannel** aRetVal)
{
  MOZ_ASSERT(aDevice);
  MOZ_ASSERT(mPresentationService);

  RefPtr<TCPDeviceInfo> deviceInfo = new TCPDeviceInfo(aDevice->Id(),
                                                       aDevice->Address(),
                                                       aDevice->Port(),
                                                       aDevice->CertFingerprint());

  return mPresentationService->Connect(deviceInfo, aRetVal);
}

bool
MulticastDNSDeviceProvider::IsCompatibleServer(nsIDNSServiceInfo* aServiceInfo)
{
  MOZ_ASSERT(aServiceInfo);

  nsCOMPtr<nsIPropertyBag2> propBag;
  if (NS_WARN_IF(NS_FAILED(
          aServiceInfo->GetAttributes(getter_AddRefs(propBag)))) || !propBag) {
    return false;
  }

  uint32_t remoteVersion;
  if (NS_WARN_IF(NS_FAILED(
          propBag->GetPropertyAsUint32(NS_LITERAL_STRING(PROTOCOL_VERSION_TAG),
                                       &remoteVersion)))) {
    return false;
  }

  bool isCompatible = false;
  Unused << NS_WARN_IF(NS_FAILED(
                mPresentationService->IsCompatibleServer(remoteVersion,
                                                         &isCompatible)));

  return isCompatible;
}

nsresult
MulticastDNSDeviceProvider::AddDevice(const nsACString& aId,
                                      const nsACString& aServiceName,
                                      const nsACString& aServiceType,
                                      const nsACString& aAddress,
                                      const uint16_t aPort,
                                      const nsACString& aCertFingerprint)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPresentationService);

  RefPtr<Device> device = new Device(aId, /* ID */
                                     aServiceName,
                                     aServiceType,
                                     aAddress,
                                     aPort,
                                     aCertFingerprint,
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
MulticastDNSDeviceProvider::UpdateDevice(const uint32_t aIndex,
                                         const nsACString& aServiceName,
                                         const nsACString& aServiceType,
                                         const nsACString& aAddress,
                                         const uint16_t aPort,
                                         const nsACString& aCertFingerprint)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPresentationService);

  if (NS_WARN_IF(aIndex >= mDevices.Length())) {
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<Device> device = mDevices[aIndex];
  device->Update(aServiceName, aServiceType, aAddress, aPort, aCertFingerprint);
  device->ChangeState(DeviceState::eActive);

  nsCOMPtr<nsIPresentationDeviceListener> listener;
  if (NS_SUCCEEDED(GetListener(getter_AddRefs(listener))) && listener) {
    Unused << listener->UpdateDevice(device);
  }

  return NS_OK;
}

nsresult
MulticastDNSDeviceProvider::RemoveDevice(const uint32_t aIndex)
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
MulticastDNSDeviceProvider::FindDeviceById(const nsACString& aId,
                                           uint32_t& aIndex)
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<Device> device = new Device(aId,
                                     /* aName = */ EmptyCString(),
                                     /* aType = */ EmptyCString(),
                                     /* aHost = */ EmptyCString(),
                                     /* aPort = */ 0,
                                     /* aCertFingerprint */ EmptyCString(),
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
MulticastDNSDeviceProvider::FindDeviceByAddress(const nsACString& aAddress,
                                                uint32_t& aIndex)
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<Device> device = new Device(/* aId = */ EmptyCString(),
                                     /* aName = */ EmptyCString(),
                                     /* aType = */ EmptyCString(),
                                     aAddress,
                                     /* aPort = */ 0,
                                     /* aCertFingerprint */ EmptyCString(),
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
MulticastDNSDeviceProvider::MarkAllDevicesUnknown()
{
  MOZ_ASSERT(NS_IsMainThread());

  for (auto& device : mDevices) {
    device->ChangeState(DeviceState::eUnknown);
  }
}

void
MulticastDNSDeviceProvider::ClearUnknownDevices()
{
  MOZ_ASSERT(NS_IsMainThread());

  size_t i = mDevices.Length();
  while (i > 0) {
    --i;
    if (mDevices[i]->State() == DeviceState::eUnknown) {
      Unused << NS_WARN_IF(NS_FAILED(RemoveDevice(i)));
    }
  }
}

void
MulticastDNSDeviceProvider::ClearDevices()
{
  MOZ_ASSERT(NS_IsMainThread());

  size_t i = mDevices.Length();
  while (i > 0) {
    --i;
    Unused << NS_WARN_IF(NS_FAILED(RemoveDevice(i)));
  }
}

// nsIPresentationDeviceProvider
NS_IMETHODIMP
MulticastDNSDeviceProvider::GetListener(nsIPresentationDeviceListener** aListener)
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
MulticastDNSDeviceProvider::SetListener(nsIPresentationDeviceListener* aListener)
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
MulticastDNSDeviceProvider::ForceDiscovery()
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
      NS_LITERAL_CSTRING(SERVICE_TYPE),
      mWrappedListener,
      getter_AddRefs(mDiscoveryRequest))))) {
    return rv;
  }

  return NS_OK;
}

// nsIDNSServiceDiscoveryListener
NS_IMETHODIMP
MulticastDNSDeviceProvider::OnDiscoveryStarted(const nsACString& aServiceType)
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
MulticastDNSDeviceProvider::OnDiscoveryStopped(const nsACString& aServiceType)
{
  LOG_I("OnDiscoveryStopped");
  MOZ_ASSERT(NS_IsMainThread());

  ClearUnknownDevices();

  mIsDiscovering = false;

  return NS_OK;
}

NS_IMETHODIMP
MulticastDNSDeviceProvider::OnServiceFound(nsIDNSServiceInfo* aServiceInfo)
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
MulticastDNSDeviceProvider::OnServiceLost(nsIDNSServiceInfo* aServiceInfo)
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
MulticastDNSDeviceProvider::OnStartDiscoveryFailed(const nsACString& aServiceType,
                                                   int32_t aErrorCode)
{
  LOG_E("OnStartDiscoveryFailed: %d", aErrorCode);
  MOZ_ASSERT(NS_IsMainThread());

  return NS_OK;
}

NS_IMETHODIMP
MulticastDNSDeviceProvider::OnStopDiscoveryFailed(const nsACString& aServiceType,
                                                  int32_t aErrorCode)
{
  LOG_E("OnStopDiscoveryFailed: %d", aErrorCode);
  MOZ_ASSERT(NS_IsMainThread());

  return NS_OK;
}

// nsIDNSRegistrationListener
NS_IMETHODIMP
MulticastDNSDeviceProvider::OnServiceRegistered(nsIDNSServiceInfo* aServiceInfo)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!aServiceInfo)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsresult rv;

  nsAutoCString name;
  if (NS_WARN_IF(NS_FAILED(rv = aServiceInfo->GetServiceName(name)))) {
    return rv;
  }

  LOG_I("OnServiceRegistered (%s)",  name.get());
  mRegisteredName = name;

  if (mMulticastDNS) {
    if (NS_WARN_IF(NS_FAILED(rv = mMulticastDNS->ResolveService(
        aServiceInfo, mWrappedListener)))) {
      return rv;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
MulticastDNSDeviceProvider::OnServiceUnregistered(nsIDNSServiceInfo* aServiceInfo)
{
  LOG_I("OnServiceUnregistered");
  MOZ_ASSERT(NS_IsMainThread());

  return NS_OK;
}

NS_IMETHODIMP
MulticastDNSDeviceProvider::OnRegistrationFailed(nsIDNSServiceInfo* aServiceInfo,
                                                 int32_t aErrorCode)
{
  LOG_E("OnRegistrationFailed: %d", aErrorCode);
  MOZ_ASSERT(NS_IsMainThread());

  mRegisterRequest = nullptr;

  if (aErrorCode == nsIDNSRegistrationListener::ERROR_SERVICE_NOT_RUNNING) {
    return NS_DispatchToMainThread(NewRunnableMethod(
      "dom::presentation::MulticastDNSDeviceProvider::RegisterMDNSService",
      this,
      &MulticastDNSDeviceProvider::RegisterMDNSService));
  }

  return NS_OK;
}

NS_IMETHODIMP
MulticastDNSDeviceProvider::OnUnregistrationFailed(nsIDNSServiceInfo* aServiceInfo,
                                                   int32_t aErrorCode)
{
  LOG_E("OnUnregistrationFailed: %d", aErrorCode);
  MOZ_ASSERT(NS_IsMainThread());

  return NS_OK;
}

// nsIDNSServiceResolveListener
NS_IMETHODIMP
MulticastDNSDeviceProvider::OnServiceResolved(nsIDNSServiceInfo* aServiceInfo)
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

  if (mRegisteredName == serviceName) {
    LOG_I("ignore self");

    if (NS_WARN_IF(NS_FAILED(rv = mPresentationService->SetId(host)))) {
      return rv;
    }

    return NS_OK;
  }

  if (!IsCompatibleServer(aServiceInfo)) {
    LOG_I("ignore incompatible service: %s", serviceName.get());
    return NS_OK;
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

  nsCOMPtr<nsIPropertyBag2> propBag;
  if (NS_WARN_IF(NS_FAILED(
          aServiceInfo->GetAttributes(getter_AddRefs(propBag)))) || !propBag) {
    return rv;
  }

  nsAutoCString certFingerprint;
  Unused << propBag->GetPropertyAsACString(NS_LITERAL_STRING(CERT_FINGERPRINT_TAG),
                                           certFingerprint);

  uint32_t index;
  if (FindDeviceById(host, index)) {
    return UpdateDevice(index,
                        serviceName,
                        serviceType,
                        address,
                        port,
                        certFingerprint);
  } else {
    return AddDevice(host,
                     serviceName,
                     serviceType,
                     address,
                     port,
                     certFingerprint);
  }

  return NS_OK;
}

NS_IMETHODIMP
MulticastDNSDeviceProvider::OnResolveFailed(nsIDNSServiceInfo* aServiceInfo,
                                            int32_t aErrorCode)
{
  LOG_E("OnResolveFailed: %d", aErrorCode);
  MOZ_ASSERT(NS_IsMainThread());

  return NS_OK;
}

// nsIPresentationControlServerListener
NS_IMETHODIMP
MulticastDNSDeviceProvider::OnServerReady(uint16_t aPort,
                                          const nsACString& aCertFingerprint)
{
  LOG_I("OnServerReady: %d, %s", aPort, PromiseFlatCString(aCertFingerprint).get());
  MOZ_ASSERT(NS_IsMainThread());

  if (mDiscoverable) {
    RegisterMDNSService();
  }

  return NS_OK;
}

NS_IMETHODIMP
MulticastDNSDeviceProvider::OnServerStopped(nsresult aResult)
{
  LOG_I("OnServerStopped: (0x%08" PRIx32 ")", static_cast<uint32_t>(aResult));

  UnregisterMDNSService(aResult);

  // Try restart server if it is stopped abnormally.
  if (NS_FAILED(aResult) && mDiscoverable) {
    mIsServerRetrying = true;
    mServerRetryTimer->Init(this, mServerRetryMs, nsITimer::TYPE_ONE_SHOT);
  }

  return NS_OK;
}

// Create a new device if we were unable to find one with the address.
already_AddRefed<MulticastDNSDeviceProvider::Device>
MulticastDNSDeviceProvider::GetOrCreateDevice(nsITCPDeviceInfo* aDeviceInfo)
{
  nsAutoCString address;
  Unused << aDeviceInfo->GetAddress(address);

  RefPtr<Device> device;
  uint32_t index;
  if (FindDeviceByAddress(address, index)) {
    device = mDevices[index];
  } else {
    // Create a one-time device object for non-discoverable controller.
    // This device will not be in the list of available devices and cannot
    // be used for requesting session.
    nsAutoCString id;
    Unused << aDeviceInfo->GetId(id);
    uint16_t port;
    Unused << aDeviceInfo->GetPort(&port);

    device = new Device(id,
                        /* aName = */ id,
                        /* aType = */ EmptyCString(),
                        address,
                        port,
                        /* aCertFingerprint */ EmptyCString(),
                        DeviceState::eActive,
                        /* aProvider = */ nullptr);
  }

  return device.forget();
}

NS_IMETHODIMP
MulticastDNSDeviceProvider::OnSessionRequest(nsITCPDeviceInfo* aDeviceInfo,
                                             const nsAString& aUrl,
                                             const nsAString& aPresentationId,
                                             nsIPresentationControlChannel* aControlChannel)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoCString address;
  Unused << aDeviceInfo->GetAddress(address);

  LOG_I("OnSessionRequest: %s", address.get());

  RefPtr<Device> device = GetOrCreateDevice(aDeviceInfo);
  nsCOMPtr<nsIPresentationDeviceListener> listener;
  if (NS_SUCCEEDED(GetListener(getter_AddRefs(listener))) && listener) {
    Unused << listener->OnSessionRequest(device, aUrl, aPresentationId,
                                         aControlChannel);
  }

  return NS_OK;
}

NS_IMETHODIMP
MulticastDNSDeviceProvider::OnTerminateRequest(nsITCPDeviceInfo* aDeviceInfo,
                                               const nsAString& aPresentationId,
                                               nsIPresentationControlChannel* aControlChannel,
                                               bool aIsFromReceiver)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoCString address;
  Unused << aDeviceInfo->GetAddress(address);

  LOG_I("OnTerminateRequest: %s", address.get());

  RefPtr<Device> device = GetOrCreateDevice(aDeviceInfo);
  nsCOMPtr<nsIPresentationDeviceListener> listener;
  if (NS_SUCCEEDED(GetListener(getter_AddRefs(listener))) && listener) {
    Unused << listener->OnTerminateRequest(device, aPresentationId,
                                           aControlChannel, aIsFromReceiver);
  }

  return NS_OK;
}

NS_IMETHODIMP
MulticastDNSDeviceProvider::OnReconnectRequest(nsITCPDeviceInfo* aDeviceInfo,
                                               const nsAString& aUrl,
                                               const nsAString& aPresentationId,
                                               nsIPresentationControlChannel* aControlChannel)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoCString address;
  Unused << aDeviceInfo->GetAddress(address);

  LOG_I("OnReconnectRequest: %s", address.get());

  RefPtr<Device> device = GetOrCreateDevice(aDeviceInfo);
  nsCOMPtr<nsIPresentationDeviceListener> listener;
  if (NS_SUCCEEDED(GetListener(getter_AddRefs(listener))) && listener) {
    Unused << listener->OnReconnectRequest(device, aUrl, aPresentationId,
                                           aControlChannel);
  }

  return NS_OK;
}

// nsIObserver
NS_IMETHODIMP
MulticastDNSDeviceProvider::Observe(nsISupports* aSubject,
                                    const char* aTopic,
                                    const char16_t* aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ConvertUTF16toUTF8 data(aData);
  LOG_I("Observe: topic = %s, data = %s", aTopic, data.get());

  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    if (data.EqualsLiteral(PREF_PRESENTATION_DISCOVERY)) {
      OnDiscoveryChanged(Preferences::GetBool(PREF_PRESENTATION_DISCOVERY));
    } else if (data.EqualsLiteral(PREF_PRESENTATION_DISCOVERY_TIMEOUT_MS)) {
      OnDiscoveryTimeoutChanged(Preferences::GetUint(PREF_PRESENTATION_DISCOVERY_TIMEOUT_MS));
    } else if (data.EqualsLiteral(PREF_PRESENTATION_DISCOVERABLE)) {
      OnDiscoverableChanged(Preferences::GetBool(PREF_PRESENTATION_DISCOVERABLE));
    } else if (data.EqualsLiteral(PREF_PRESENTATION_DEVICE_NAME)) {
      nsAdoptingCString newServiceName = Preferences::GetCString(PREF_PRESENTATION_DEVICE_NAME);
      if (!mServiceName.Equals(newServiceName)) {
        OnServiceNameChanged(newServiceName);
      }
    }
  } else if (!strcmp(aTopic, NS_TIMER_CALLBACK_TOPIC)) {
    nsCOMPtr<nsITimer> timer = do_QueryInterface(aSubject);
    if (!timer) {
      return NS_ERROR_UNEXPECTED;
    }

    if (timer == mDiscoveryTimer) {
      StopDiscovery(NS_OK);
    } else if (timer == mServerRetryTimer) {
      mIsServerRetrying = false;
      StartServer();
    }
  }

  return NS_OK;
}

nsresult
MulticastDNSDeviceProvider::OnDiscoveryChanged(bool aEnabled)
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
MulticastDNSDeviceProvider::OnDiscoveryTimeoutChanged(uint32_t aTimeoutMs)
{
  LOG_I("OnDiscoveryTimeoutChanged = %d\n", aTimeoutMs);
  MOZ_ASSERT(NS_IsMainThread());

  mDiscoveryTimeoutMs = aTimeoutMs;

  return NS_OK;
}

nsresult
MulticastDNSDeviceProvider::OnDiscoverableChanged(bool aEnabled)
{
  LOG_I("Discoverable = %d\n", aEnabled);
  MOZ_ASSERT(NS_IsMainThread());

  mDiscoverable = aEnabled;

  if (mDiscoverable) {
    return StartServer();
  }

  return StopServer();
}

nsresult
MulticastDNSDeviceProvider::OnServiceNameChanged(const nsACString& aServiceName)
{
  LOG_I("serviceName = %s\n", PromiseFlatCString(aServiceName).get());
  MOZ_ASSERT(NS_IsMainThread());

  mServiceName = aServiceName;

  nsresult rv;
  if (NS_WARN_IF(NS_FAILED(rv = UnregisterMDNSService(NS_OK)))) {
    return rv;
  }

  if (mDiscoverable) {
    return RegisterMDNSService();
  }

  return NS_OK;
}

// MulticastDNSDeviceProvider::Device
NS_IMPL_ISUPPORTS(MulticastDNSDeviceProvider::Device,
                  nsIPresentationDevice)

// nsIPresentationDevice
NS_IMETHODIMP
MulticastDNSDeviceProvider::Device::GetId(nsACString& aId)
{
  aId = mId;

  return NS_OK;
}

NS_IMETHODIMP
MulticastDNSDeviceProvider::Device::GetName(nsACString& aName)
{
  aName = mName;

  return NS_OK;
}

NS_IMETHODIMP
MulticastDNSDeviceProvider::Device::GetType(nsACString& aType)
{
  aType = mType;

  return NS_OK;
}

NS_IMETHODIMP
MulticastDNSDeviceProvider::Device::EstablishControlChannel(
                                        nsIPresentationControlChannel** aRetVal)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->Connect(this, aRetVal);
}

NS_IMETHODIMP
MulticastDNSDeviceProvider::Device::Disconnect()
{
  // No need to do anything when disconnect.
  return NS_OK;
}

NS_IMETHODIMP
MulticastDNSDeviceProvider::Device::IsRequestedUrlSupported(
                                                 const nsAString& aRequestedUrl,
                                                 bool* aRetVal)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!aRetVal) {
    return NS_ERROR_INVALID_POINTER;
  }

  // TV 2.6 also supports presentation Apps and HTTP/HTTPS hosted receiver page.
  if (DeviceProviderHelpers::IsFxTVSupportedAppUrl(aRequestedUrl) ||
      DeviceProviderHelpers::IsCommonlySupportedScheme(aRequestedUrl)) {
    *aRetVal = true;
  }

  return NS_OK;
}

} // namespace presentation
} // namespace dom
} // namespace mozilla
