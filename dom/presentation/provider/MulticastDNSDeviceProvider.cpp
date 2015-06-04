/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MulticastDNSDeviceProvider.h"
#include "mozilla/Logging.h"
#include "nsAutoPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsIPresentationDevice.h"
#include "nsServiceManagerUtils.h"

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

#define SERVICE_TYPE "_mozilla_papi._tcp."

namespace mozilla {
namespace dom {
namespace presentation {

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
                  nsITCPPresentationServerListener)

MulticastDNSDeviceProvider::~MulticastDNSDeviceProvider()
{
  Uninit();
}

nsresult
MulticastDNSDeviceProvider::Init()
{
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

  mPresentationServer = do_CreateInstance("@mozilla.org/presentation-device/tcp-presentation-server;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (NS_WARN_IF(NS_FAILED(mPresentationServer->SetListener(mWrappedListener)))) {
    return rv;
  }
  if (NS_WARN_IF(NS_FAILED(rv = mPresentationServer->Init(EmptyCString(), 0)))) {
    return rv;
  }

  uint16_t port = 0;
  if (NS_WARN_IF(NS_FAILED(rv = mPresentationServer->GetPort(&port)))) {
    return rv;
  }

  if (NS_WARN_IF(NS_FAILED(rv = RegisterService(port)))) {
    return rv;
  }

  mInitialized = true;
  return NS_OK;
}

nsresult
MulticastDNSDeviceProvider::Uninit()
{
  if (!mInitialized) {
    return NS_OK;
  }

  if (mPresentationServer) {
    mPresentationServer->Close();
    mPresentationServer = nullptr;
  }

  if (mDiscoveryRequest) {
    mDiscoveryRequest->Cancel(NS_OK);
    mDiscoveryRequest = nullptr;
  }
  if (mRegisterRequest) {
    mRegisterRequest->Cancel(NS_OK);
    mRegisterRequest = nullptr;
  }
  mMulticastDNS = nullptr;

  if (mWrappedListener) {
    mWrappedListener->SetListener(nullptr);
    mWrappedListener = nullptr;
  }

  mInitialized = false;
  return NS_OK;
}

nsresult
MulticastDNSDeviceProvider::RegisterService(uint32_t aPort)
{
  LOG_I("RegisterService: %d", aPort);

  nsresult rv;

  nsCOMPtr<nsIDNSServiceInfo> serviceInfo = do_CreateInstance(DNSSERVICEINFO_CONTRACT_ID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (NS_WARN_IF(NS_FAILED(rv = serviceInfo->SetServiceType(NS_LITERAL_CSTRING(SERVICE_TYPE))))) {
    return rv;
  }
  if (NS_WARN_IF(NS_FAILED(rv = serviceInfo->SetPort(aPort)))) {
    return rv;
  }

  if (mRegisterRequest) {
    mRegisterRequest->Cancel(NS_OK);
    mRegisterRequest = nullptr;
  }
  return mMulticastDNS->RegisterService(serviceInfo, mWrappedListener, getter_AddRefs(mRegisterRequest));
}

// nsIPresentationDeviceProvider
NS_IMETHODIMP
MulticastDNSDeviceProvider::GetListener(nsIPresentationDeviceListener** aListener)
{
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
  LOG_I("ForceDiscovery");
  MOZ_ASSERT(mInitialized);
  MOZ_ASSERT(mMulticastDNS);

  nsresult rv;

  if (mDiscoveryRequest) {
    mDiscoveryRequest->Cancel(NS_OK);
    mDiscoveryRequest = nullptr;
  }
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
  return NS_OK;
}

NS_IMETHODIMP
MulticastDNSDeviceProvider::OnDiscoveryStopped(const nsACString& aServiceType)
{
  LOG_I("OnDiscoveryStopped");
  return NS_OK;
}

NS_IMETHODIMP
MulticastDNSDeviceProvider::OnServiceFound(nsIDNSServiceInfo* aServiceInfo)
{
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
    if (NS_WARN_IF(NS_FAILED(rv = mMulticastDNS->ResolveService(aServiceInfo, mWrappedListener)))) {
      return rv;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
MulticastDNSDeviceProvider::OnServiceLost(nsIDNSServiceInfo* aServiceInfo)
{
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
  if (NS_FAILED(mPresentationServer->GetTCPDevice(serviceName, getter_AddRefs(device)))) {
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
MulticastDNSDeviceProvider::OnStartDiscoveryFailed(const nsACString& aServiceType, int32_t aErrorCode)
{
  LOG_E("OnStartDiscoveryFailed: %d", aErrorCode);
  return NS_OK;
}

NS_IMETHODIMP
MulticastDNSDeviceProvider::OnStopDiscoveryFailed(const nsACString& aServiceType, int32_t aErrorCode)
{
  LOG_E("OnStopDiscoveryFailed: %d", aErrorCode);
  return NS_OK;
}

// nsIDNSRegistrationListener
NS_IMETHODIMP
MulticastDNSDeviceProvider::OnServiceRegistered(nsIDNSServiceInfo* aServiceInfo)
{
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
  return NS_OK;
}

NS_IMETHODIMP
MulticastDNSDeviceProvider::OnRegistrationFailed(nsIDNSServiceInfo* aServiceInfo, int32_t aErrorCode)
{
  LOG_E("OnRegistrationFailed: %d", aErrorCode);

  nsresult rv;

  if (aErrorCode == nsIDNSRegistrationListener::ERROR_SERVICE_NOT_RUNNING) {
    uint16_t port = 0;
    if (NS_WARN_IF(NS_FAILED(rv = mPresentationServer->GetPort(&port)))) {
      return rv;
    }

    if (NS_WARN_IF(NS_FAILED(rv = RegisterService(port)))) {
      return rv;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
MulticastDNSDeviceProvider::OnUnregistrationFailed(nsIDNSServiceInfo* aServiceInfo, int32_t aErrorCode)
{
  LOG_E("OnUnregistrationFailed: %d", aErrorCode);
  return NS_OK;
}

// nsIDNSServiceResolveListener
NS_IMETHODIMP
MulticastDNSDeviceProvider::OnServiceResolved(nsIDNSServiceInfo* aServiceInfo)
{
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
MulticastDNSDeviceProvider::OnResolveFailed(nsIDNSServiceInfo* aServiceInfo, int32_t aErrorCode)
{
  LOG_E("OnResolveFailed: %d", aErrorCode);
  return NS_OK;
}

// nsITCPPresentationServerListener
NS_IMETHODIMP
MulticastDNSDeviceProvider::OnClose(nsresult aReason)
{
  LOG_I("OnClose: %x", aReason);

  if (mRegisterRequest) {
    mRegisterRequest->Cancel(aReason);
    mRegisterRequest = nullptr;
  }

  nsresult rv;

  if (NS_FAILED(aReason)) {
    if (NS_WARN_IF(NS_FAILED(rv = mPresentationServer->Init(EmptyCString(), 0)))) {
      return rv;
    }

    uint16_t port = 0;
    if (NS_WARN_IF(NS_FAILED(rv = mPresentationServer->GetPort(&port)))) {
      return rv;
    }

    if (NS_WARN_IF(NS_FAILED(rv = RegisterService(port)))) {
      return rv;
    }
  }

  return NS_OK;
}

} // namespace presentation
} // namespace dom
} // namespace mozilla
