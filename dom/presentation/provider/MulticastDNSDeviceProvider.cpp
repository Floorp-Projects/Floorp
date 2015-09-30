/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MulticastDNSDeviceProvider.h"
#include "MainThreadUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/unused.h"
#include "nsAutoPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsIObserverService.h"
#include "nsIPresentationDevice.h"
#include "nsServiceManagerUtils.h"

#define PREF_PRESENTATION_DISCOVERY "dom.presentation.discovery.enabled"
#define PREF_PRESENTATION_DISCOVERY_TIMEOUT_MS "dom.presentation.discovery.timeout_ms"
#define PREF_PRESENTATION_DISCOVERABLE "dom.presentation.discoverable"
#define PREF_PRESENTATION_DEVICE_NAME "dom.presentation.device.name"

#define TCP_PRESENTATION_SERVER_CONTACT_ID \
  "@mozilla.org/presentation-device/tcp-presentation-server;1"

#define SERVICE_TYPE "_mozilla_papi._tcp."

inline static PRLogModuleInfo*
GetProviderLog()
{
  static PRLogModuleInfo* log = PR_NewLogModule("MulticastDNSDeviceProvider");
  return log;
}
#undef LOG_I
#define LOG_I(...) MOZ_LOG(GetProviderLog(), mozilla::LogLevel::Debug, (__VA_ARGS__))
#undef LOG_E
#define LOG_E(...) MOZ_LOG(GetProviderLog(), mozilla::LogLevel::Error, (__VA_ARGS__))

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

/**
 * This wrapper is used to break circular-reference problem.
 */
class DNSServiceWrappedListener final
  : public nsIDNSServiceDiscoveryListener
  , public nsIDNSRegistrationListener
  , public nsIDNSServiceResolveListener
  , public nsITCPPresentationServerListener
{
public:
  NS_DECL_ISUPPORTS
  NS_FORWARD_SAFE_NSIDNSSERVICEDISCOVERYLISTENER(mListener)
  NS_FORWARD_SAFE_NSIDNSREGISTRATIONLISTENER(mListener)
  NS_FORWARD_SAFE_NSIDNSSERVICERESOLVELISTENER(mListener)
  NS_FORWARD_SAFE_NSITCPPRESENTATIONSERVERLISTENER(mListener)

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
                  nsITCPPresentationServerListener)

NS_IMPL_ISUPPORTS(MulticastDNSDeviceProvider,
                  nsIPresentationDeviceProvider,
                  nsIDNSServiceDiscoveryListener,
                  nsIDNSRegistrationListener,
                  nsIDNSServiceResolveListener,
                  nsITCPPresentationServerListener,
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
  if (NS_WARN_IF(!mWrappedListener)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (NS_WARN_IF(NS_FAILED(rv = mWrappedListener->SetListener(this)))) {
    return rv;
  }

  mPresentationServer = do_CreateInstance(TCP_PRESENTATION_SERVER_CONTACT_ID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mDiscoveryTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  Preferences::AddStrongObservers(this, kObservedPrefs);

  mDiscoveryEnabled = Preferences::GetBool(PREF_PRESENTATION_DISCOVERY);
  mDiscveryTimeoutMs = Preferences::GetUint(PREF_PRESENTATION_DISCOVERY_TIMEOUT_MS);
  mDiscoverable = Preferences::GetBool(PREF_PRESENTATION_DISCOVERABLE);
  mServiceName = Preferences::GetCString(PREF_PRESENTATION_DEVICE_NAME);

  if (mDiscoveryEnabled && NS_WARN_IF(NS_FAILED(rv = ForceDiscovery()))) {
    return rv;
  }

  if (mDiscoverable && NS_WARN_IF(NS_FAILED(rv = RegisterService()))) {
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

  Preferences::RemoveObservers(this, kObservedPrefs);

  StopDiscovery(NS_OK);
  UnregisterService(NS_OK);

  mMulticastDNS = nullptr;

  if (mWrappedListener) {
    mWrappedListener->SetListener(nullptr);
    mWrappedListener = nullptr;
  }

  mInitialized = false;
  return NS_OK;
}

nsresult
MulticastDNSDeviceProvider::RegisterService()
{
  LOG_I("RegisterService: %s (%d)", mServiceName.get(), mDiscoverable);
  MOZ_ASSERT(NS_IsMainThread());

  if (!mDiscoverable) {
    return NS_OK;
  }

  MOZ_ASSERT(!mRegisterRequest);

  nsresult rv;
  if (NS_WARN_IF(NS_FAILED(rv = mPresentationServer->SetListener(mWrappedListener)))) {
    return rv;
  }
  if (NS_WARN_IF(NS_FAILED(rv = mPresentationServer->Init(EmptyCString(), 0)))) {
    return rv;
  }
  uint16_t servicePort;
  if (NS_WARN_IF(NS_FAILED(rv = mPresentationServer->GetPort(&servicePort)))) {
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

  return mMulticastDNS->RegisterService(serviceInfo,
                                        mWrappedListener,
                                        getter_AddRefs(mRegisterRequest));
}

nsresult
MulticastDNSDeviceProvider::UnregisterService(nsresult aReason)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mRegisterRequest) {
    mRegisterRequest->Cancel(aReason);
    mRegisterRequest = nullptr;
  }

  if (mPresentationServer) {
    mPresentationServer->SetListener(nullptr);
    mPresentationServer->Close();
  }

  return NS_OK;
}

nsresult
MulticastDNSDeviceProvider::StopDiscovery(nsresult aReason)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDiscoveryTimer);

  unused << mDiscoveryTimer->Cancel();

  if (mDiscoveryRequest) {
    mDiscoveryRequest->Cancel(aReason);
    mDiscoveryRequest = nullptr;
  }

  return NS_OK;
}

// nsIPresentationDeviceProvider
NS_IMETHODIMP
MulticastDNSDeviceProvider::GetListener(nsIPresentationDeviceListener** aListener)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_WARN_IF(!aListener)) {
    return NS_ERROR_INVALID_POINTER;
  }

  nsCOMPtr<nsIPresentationDeviceListener> listener = do_QueryReferent(mDeviceListener);
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

  MOZ_ASSERT(mMulticastDNS);

  StopDiscovery(NS_OK);

  nsresult rv;
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

  nsresult rv;
  if (NS_WARN_IF(NS_FAILED(rv = mDiscoveryTimer->Init(this,
                                                      mDiscveryTimeoutMs,
                                                      nsITimer::TYPE_ONE_SHOT)))) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
MulticastDNSDeviceProvider::OnDiscoveryStopped(const nsACString& aServiceType)
{
  LOG_I("OnDiscoveryStopped");
  MOZ_ASSERT(NS_IsMainThread());

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

  if (mRegisteredName == serviceName) {
    LOG_I("ignore self");
    return NS_OK;
  }

  nsCOMPtr<nsIPresentationDevice> device;
  if (NS_SUCCEEDED(mPresentationServer->GetTCPDevice(serviceName,
                                                     getter_AddRefs(device)))) {
    LOG_I("device exists");
    return NS_OK;
  }

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

  nsCOMPtr<nsIPresentationDevice> device;
  if (NS_FAILED(mPresentationServer->GetTCPDevice(serviceName,
                                                  getter_AddRefs(device)))) {
    return NS_OK; // ignore non-existing device;
  }

  NS_WARN_IF(NS_FAILED(mPresentationServer->RemoveTCPDevice(serviceName)));

  nsCOMPtr<nsIPresentationDeviceListener> listener;
  GetListener(getter_AddRefs(listener));
  if (listener) {
    listener->RemoveDevice(device);
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

  if (NS_WARN_IF(NS_FAILED(rv = mPresentationServer->SetId(name)))) {
    return rv;
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

  nsresult rv;

  if (aErrorCode == nsIDNSRegistrationListener::ERROR_SERVICE_NOT_RUNNING) {
    if (NS_WARN_IF(NS_FAILED(rv = RegisterService()))) {
      return rv;
    }
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

  nsCOMPtr<nsIPresentationDevice> device;
  nsCOMPtr<nsIPresentationDeviceListener> listener;
  GetListener(getter_AddRefs(listener));

  if (NS_SUCCEEDED(mPresentationServer->GetTCPDevice(serviceName,
                                                     getter_AddRefs(device)))) {
    NS_WARN_IF(NS_FAILED(mPresentationServer->RemoveTCPDevice(serviceName)));
    if (listener) {
      NS_WARN_IF(NS_FAILED(listener->RemoveDevice(device)));
    }
  }

  nsAutoCString host;
  if (NS_WARN_IF(NS_FAILED(rv = aServiceInfo->GetHost(host)))) {
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

  if (NS_WARN_IF(NS_FAILED(rv = mPresentationServer->CreateTCPDevice(serviceName,
                                                                     serviceName,
                                                                     serviceType,
                                                                     host,
                                                                     port,
                                                                     getter_AddRefs(device))))) {
    return rv;
  }

  if (listener) {
    listener->AddDevice(device);
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

// nsITCPPresentationServerListener
NS_IMETHODIMP
MulticastDNSDeviceProvider::OnClose(nsresult aReason)
{
  LOG_I("OnClose: %x", aReason);
  MOZ_ASSERT(NS_IsMainThread());

  UnregisterService(aReason);

  nsresult rv;

  if (mDiscoveryEnabled && NS_WARN_IF(NS_FAILED(rv = ForceDiscovery()))) {
    return rv;
  }

  if (mDiscoverable && NS_WARN_IF(NS_FAILED(rv = RegisterService()))) {
    return rv;
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
      OnServiceNameChanged(Preferences::GetCString(PREF_PRESENTATION_DEVICE_NAME));
    }
  } else if (!strcmp(aTopic, NS_TIMER_CALLBACK_TOPIC)) {
    StopDiscovery(NS_OK);
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

  mDiscveryTimeoutMs = aTimeoutMs;

  return NS_OK;
}

nsresult
MulticastDNSDeviceProvider::OnDiscoverableChanged(bool aEnabled)
{
  LOG_I("Discoverable = %d\n", aEnabled);
  MOZ_ASSERT(NS_IsMainThread());

  mDiscoverable = aEnabled;

  if (mDiscoverable) {
    return RegisterService();
  }

  return UnregisterService(NS_OK);
}

nsresult
MulticastDNSDeviceProvider::OnServiceNameChanged(const nsCString& aServiceName)
{
  LOG_I("serviceName = %s\n", aServiceName.get());
  MOZ_ASSERT(NS_IsMainThread());

  mServiceName = aServiceName;

  nsresult rv;
  if (NS_WARN_IF(NS_FAILED(rv = UnregisterService(NS_OK)))) {
    return rv;
  }

  if (mDiscoverable) {
    return RegisterService();
  }

  return NS_OK;
}

} // namespace presentation
} // namespace dom
} // namespace mozilla
