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
#include "nsServiceManagerUtils.h"

#define PREF_PRESENTATION_DISCOVERY "dom.presentation.discovery.enabled"
#define PREF_PRESENTATION_DISCOVERY_TIMEOUT_MS "dom.presentation.discovery.timeout_ms"
#define PREF_PRESENTATION_DISCOVERABLE "dom.presentation.discoverable"
#define PREF_PRESENTATION_DEVICE_NAME "dom.presentation.device.name"

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

namespace {

class TCPDeviceInfo final : public nsITCPDeviceInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITCPDEVICEINFO

  explicit TCPDeviceInfo(const nsACString& aId,
                         const nsACString& aHost,
                         const uint16_t aPort)
    : mId(aId)
    , mHost(aHost)
    , mPort(aPort)
  {
  }

private:
  virtual ~TCPDeviceInfo() {}

  nsCString mId;
  nsCString mHost;
  uint16_t mPort;
};

NS_IMPL_ISUPPORTS(TCPDeviceInfo,
                  nsITCPDeviceInfo)

// nsITCPDeviceInfo
NS_IMETHODIMP
TCPDeviceInfo::GetId(nsACString& aId)
{
  aId = mId;
  return NS_OK;
}

NS_IMETHODIMP
TCPDeviceInfo::GetHost(nsACString& aHost)
{
  aHost = mHost;
  return NS_OK;
}

NS_IMETHODIMP
TCPDeviceInfo::GetPort(uint16_t* aPort)
{
  *aPort = mPort;
  return NS_OK;
}

} //anonymous namespace

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

  ClearDevices();

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
  LOG_I("StopDiscovery (0x%08x)", aReason);

  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDiscoveryTimer);

  unused << mDiscoveryTimer->Cancel();

  if (mDiscoveryRequest) {
    mDiscoveryRequest->Cancel(aReason);
    mDiscoveryRequest = nullptr;
  }

  return NS_OK;
}

nsresult
MulticastDNSDeviceProvider::RequestSession(Device* aDevice,
                                           const nsAString& aUrl,
                                           const nsAString& aPresentationId,
                                           nsIPresentationControlChannel** aRetVal)
{
  MOZ_ASSERT(aDevice);
  MOZ_ASSERT(mPresentationServer);

  RefPtr<TCPDeviceInfo> deviceInfo = new TCPDeviceInfo(aDevice->Id(),
                                                       aDevice->Host(),
                                                       aDevice->Port());

  return mPresentationServer->RequestSession(deviceInfo, aUrl, aPresentationId, aRetVal);
}

nsresult
MulticastDNSDeviceProvider::AddDevice(const nsACString& aServiceName,
                                      const nsACString& aServiceType,
                                      const nsACString& aHost,
                                      const uint16_t aPort)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPresentationServer);

  RefPtr<Device> device = new Device(aHost, /* ID */
                                     aServiceName,
                                     aServiceType,
                                     aHost,
                                     aPort,
                                     DeviceState::eActive,
                                     this);

  nsCOMPtr<nsIPresentationDeviceListener> listener;
  if (NS_SUCCEEDED(GetListener(getter_AddRefs(listener))) && listener) {
    unused << listener->AddDevice(device);
  }

  mDevices.AppendElement(device);

  return NS_OK;
}

nsresult
MulticastDNSDeviceProvider::UpdateDevice(const uint32_t aIndex,
                                         const nsACString& aServiceName,
                                         const nsACString& aServiceType,
                                         const nsACString& aHost,
                                         const uint16_t aPort)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPresentationServer);

  if (NS_WARN_IF(aIndex >= mDevices.Length())) {
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<Device> device = mDevices[aIndex];
  device->Update(aServiceName, aServiceType, aHost, aPort);
  device->ChangeState(DeviceState::eActive);

  nsCOMPtr<nsIPresentationDeviceListener> listener;
  if (NS_SUCCEEDED(GetListener(getter_AddRefs(listener))) && listener) {
    unused << listener->UpdateDevice(device);
  }

  return NS_OK;
}

nsresult
MulticastDNSDeviceProvider::RemoveDevice(const uint32_t aIndex)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPresentationServer);

  if (NS_WARN_IF(aIndex >= mDevices.Length())) {
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<Device> device = mDevices[aIndex];

  LOG_I("RemoveDevice: %s", device->Id().get());
  mDevices.RemoveElementAt(aIndex);

  nsCOMPtr<nsIPresentationDeviceListener> listener;
  if (NS_SUCCEEDED(GetListener(getter_AddRefs(listener))) && listener) {
    unused << listener->RemoveDevice(device);
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
MulticastDNSDeviceProvider::FindDeviceByHost(const nsACString& aHost,
                                             uint32_t& aIndex)
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<Device> device = new Device(/* aId = */ EmptyCString(),
                                     /* aName = */ EmptyCString(),
                                     /* aType = */ EmptyCString(),
                                     aHost,
                                     /* aPort = */ 0,
                                     /* aState = */ DeviceState::eUnknown,
                                     /* aProvider = */ nullptr);
  size_t index = mDevices.IndexOf(device, 0, DeviceHostComparator());

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
      NS_WARN_IF(NS_FAILED(RemoveDevice(i)));
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
    NS_WARN_IF(NS_FAILED(RemoveDevice(i)));
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
  if (mIsDiscovering) {
    unused << mDiscoveryTimer->Cancel();

    NS_WARN_IF(NS_FAILED(mDiscoveryTimer->Init(this,
                                               mDiscveryTimeoutMs,
                                               nsITimer::TYPE_ONE_SHOT)));
    return NS_OK;
  }

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

  MarkAllDevicesUnknown();

  nsresult rv;
  if (NS_WARN_IF(NS_FAILED(rv = mDiscoveryTimer->Init(this,
                                                      mDiscveryTimeoutMs,
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

  if (mRegisteredName == serviceName) {
    LOG_I("ignore self");
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

  uint32_t index;
  if (FindDeviceById(host, index)) {
    return UpdateDevice(index,
                        serviceName,
                        serviceType,
                        host,
                        port);
  } else {
    return AddDevice(serviceName,
                     serviceType,
                     host,
                     port);
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

  if (mDiscoverable && NS_WARN_IF(NS_FAILED(rv = RegisterService()))) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
MulticastDNSDeviceProvider::OnSessionRequest(nsITCPDeviceInfo* aDeviceInfo,
                                             const nsAString& aUrl,
                                             const nsAString& aPresentationId,
                                             nsIPresentationControlChannel* aControlChannel)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoCString host;
  unused << aDeviceInfo->GetHost(host);

  LOG_I("OnSessionRequest: %s", host.get());

  RefPtr<Device> device;
  uint32_t index;
  if (FindDeviceByHost(host, index)) {
    device = mDevices[index];
  } else {
    // create a one-time device object for non-discoverable controller
    // this device will not be listed in available device list and cannot
    // be used for requesting session.
    nsAutoCString id;
    unused << aDeviceInfo->GetId(id);
    uint16_t port;
    unused << aDeviceInfo->GetPort(&port);

    device = new Device(id,
                        /* aName = */ id,
                        /* aType = */ EmptyCString(),
                        host,
                        port,
                        DeviceState::eActive,
                        /* aProvider = */ nullptr);
  }

  nsCOMPtr<nsIPresentationDeviceListener> listener;
  if (NS_SUCCEEDED(GetListener(getter_AddRefs(listener))) && listener) {
    unused << listener->OnSessionRequest(device, aUrl, aPresentationId,
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
MulticastDNSDeviceProvider::OnServiceNameChanged(const nsACString& aServiceName)
{
  LOG_I("serviceName = %s\n", PromiseFlatCString(aServiceName).get());
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
MulticastDNSDeviceProvider::Device::EstablishControlChannel(const nsAString& aUrl,
                                                            const nsAString& aPresentationId,
                                                            nsIPresentationControlChannel** aRetVal)
{
  if (!mProvider) {
    return NS_ERROR_FAILURE;
  }

  return mProvider->RequestSession(this, aUrl, aPresentationId, aRetVal);
}

} // namespace presentation
} // namespace dom
} // namespace mozilla
