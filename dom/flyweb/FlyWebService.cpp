/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FlyWebService.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/FlyWebPublishedServer.h"
#include "nsISocketTransportService.h"
#include "mdns/libmdns/nsDNSServiceInfo.h"
#include "nsIUUIDGenerator.h"
#include "nsStandardURL.h"
#include "mozilla/Services.h"
#include "nsISupportsPrimitives.h"
#include "mozilla/dom/FlyWebDiscoveryManagerBinding.h"
#include "prnetdb.h"
#include "DNS.h"
#include "nsSocketTransportService2.h"
#include "nsSocketTransport2.h"
#include "nsHashPropertyBag.h"
#include "nsNetUtil.h"
#include "nsISimpleEnumerator.h"
#include "nsIProperty.h"
#include "nsICertOverrideService.h"

namespace mozilla {
namespace dom {

struct FlyWebPublishOptions;

static LazyLogModule gFlyWebServiceLog("FlyWebService");
#undef LOG_I
#define LOG_I(...) MOZ_LOG(mozilla::dom::gFlyWebServiceLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#undef LOG_E
#define LOG_E(...) MOZ_LOG(mozilla::dom::gFlyWebServiceLog, mozilla::LogLevel::Error, (__VA_ARGS__))
#undef LOG_TEST_I
#define LOG_TEST_I(...) MOZ_LOG_TEST(mozilla::dom::gFlyWebServiceLog, mozilla::LogLevel::Debug)

class FlyWebMDNSService final
  : public nsIDNSServiceDiscoveryListener
  , public nsIDNSServiceResolveListener
  , public nsIDNSRegistrationListener
  , public nsITimerCallback
{
  friend class FlyWebService;

private:
  enum DiscoveryState {
    DISCOVERY_IDLE,
    DISCOVERY_STARTING,
    DISCOVERY_RUNNING,
    DISCOVERY_STOPPING
  };

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDNSSERVICEDISCOVERYLISTENER
  NS_DECL_NSIDNSSERVICERESOLVELISTENER
  NS_DECL_NSIDNSREGISTRATIONLISTENER
  NS_DECL_NSITIMERCALLBACK

  explicit FlyWebMDNSService(FlyWebService* aService,
                             const nsACString& aServiceType);

private:
  virtual ~FlyWebMDNSService() = default;

  nsresult Init();
  nsresult StartDiscovery();
  nsresult StopDiscovery();

  void ListDiscoveredServices(nsTArray<FlyWebDiscoveredService>& aServices);
  bool HasService(const nsAString& aServiceId);
  nsresult PairWithService(const nsAString& aServiceId,
                           UniquePtr<FlyWebService::PairedInfo>& aInfo);

  nsresult StartDiscoveryOf(FlyWebPublishedServer* aServer);

  void EnsureDiscoveryStarted();
  void EnsureDiscoveryStopped();

  // Cycle-breaking link to manager.
  FlyWebService* mService;
  nsCString mServiceType;

  // Indicates the desired state of the system.  If mDiscoveryActive is true,
  // it indicates that backend discovery "should be happening", and discovery
  // events should be forwarded to listeners.
  // If false, the backend discovery "should be idle", and any discovery events
  // that show up should not be forwarded to listeners.
  bool mDiscoveryActive;

  uint32_t mNumConsecutiveStartDiscoveryFailures;

  // Represents the internal discovery state as it relates to nsDNSServiceDiscovery.
  // When mDiscoveryActive is true, this state will periodically loop from
  // (IDLE => STARTING => RUNNING => STOPPING => IDLE).
  DiscoveryState mDiscoveryState;

  nsCOMPtr<nsITimer> mDiscoveryStartTimer;
  nsCOMPtr<nsITimer> mDiscoveryStopTimer;
  nsCOMPtr<nsIDNSServiceDiscovery> mDNSServiceDiscovery;
  nsCOMPtr<nsICancelable> mCancelDiscovery;
  nsTHashtable<nsStringHashKey> mNewServiceSet;

  struct DiscoveredInfo
  {
    explicit DiscoveredInfo(nsIDNSServiceInfo* aDNSServiceInfo);
    FlyWebDiscoveredService mService;
    nsCOMPtr<nsIDNSServiceInfo> mDNSServiceInfo;
  };
  nsClassHashtable<nsStringHashKey, DiscoveredInfo> mServiceMap;
};

void
LogDNSInfo(nsIDNSServiceInfo* aServiceInfo, const char* aFunc)
{
  if (!LOG_TEST_I()) {
    return;
  }

  nsCString tmp;
  aServiceInfo->GetServiceName(tmp);
  LOG_I("%s: serviceName=%s", aFunc, tmp.get());

  aServiceInfo->GetHost(tmp);
  LOG_I("%s: host=%s", aFunc, tmp.get());

  aServiceInfo->GetAddress(tmp);
  LOG_I("%s: address=%s", aFunc, tmp.get());

  uint16_t port = -2;
  aServiceInfo->GetPort(&port);
  LOG_I("%s: port=%d", aFunc, (int)port);

  nsCOMPtr<nsIPropertyBag2> attributes;
  aServiceInfo->GetAttributes(getter_AddRefs(attributes));
  if (!attributes) {
    LOG_I("%s: no attributes", aFunc);
  } else {
    nsCOMPtr<nsISimpleEnumerator> enumerator;
    attributes->GetEnumerator(getter_AddRefs(enumerator));
    MOZ_ASSERT(enumerator);

    LOG_I("%s: attributes start", aFunc);

    bool hasMoreElements;
    while (NS_SUCCEEDED(enumerator->HasMoreElements(&hasMoreElements)) &&
           hasMoreElements) {
      nsCOMPtr<nsISupports> element;
      MOZ_ALWAYS_SUCCEEDS(enumerator->GetNext(getter_AddRefs(element)));
      nsCOMPtr<nsIProperty> property = do_QueryInterface(element);
      MOZ_ASSERT(property);

      nsAutoString name;
      nsCOMPtr<nsIVariant> value;
      MOZ_ALWAYS_SUCCEEDS(property->GetName(name));
      MOZ_ALWAYS_SUCCEEDS(property->GetValue(getter_AddRefs(value)));

      nsAutoCString str;
      nsresult rv = value->GetAsACString(str);
      if (NS_SUCCEEDED(rv)) {
        LOG_I("%s: attribute name=%s value=%s", aFunc,
              NS_ConvertUTF16toUTF8(name).get(), str.get());
      } else {
        uint16_t type;
        MOZ_ALWAYS_SUCCEEDS(value->GetDataType(&type));
        LOG_I("%s: attribute *unstringifiable* name=%s type=%d", aFunc,
              NS_ConvertUTF16toUTF8(name).get(), (int)type);
      }
    }

    LOG_I("%s: attributes end", aFunc);
  }
}

NS_IMPL_ISUPPORTS(FlyWebMDNSService,
                  nsIDNSServiceDiscoveryListener,
                  nsIDNSServiceResolveListener,
                  nsIDNSRegistrationListener,
                  nsITimerCallback)

FlyWebMDNSService::FlyWebMDNSService(
        FlyWebService* aService,
        const nsACString& aServiceType)
  : mService(aService)
  , mServiceType(aServiceType)
  , mDiscoveryActive(false)
  , mNumConsecutiveStartDiscoveryFailures(0)
  , mDiscoveryState(DISCOVERY_IDLE)
{}

nsresult
FlyWebMDNSService::OnDiscoveryStarted(const nsACString& aServiceType)
{
  MOZ_ASSERT(mDiscoveryState == DISCOVERY_STARTING);
  mDiscoveryState = DISCOVERY_RUNNING;
  // Reset consecutive start discovery failures.
  mNumConsecutiveStartDiscoveryFailures = 0;
  LOG_I("===========================================");
  LOG_I("MDNSService::OnDiscoveryStarted(%s)", PromiseFlatCString(aServiceType).get());
  LOG_I("===========================================");

  // Clear the new service array.
  mNewServiceSet.Clear();

  // If service discovery is inactive, then stop network discovery immediately.
  if (!mDiscoveryActive) {
    // Set the stop timer to fire immediately.
    NS_WARN_IF(NS_FAILED(mDiscoveryStopTimer->InitWithCallback(this, 0, nsITimer::TYPE_ONE_SHOT)));
    return NS_OK;
  }

  // Otherwise, set the stop timer to fire in 5 seconds.
  NS_WARN_IF(NS_FAILED(mDiscoveryStopTimer->InitWithCallback(this, 5 * 1000, nsITimer::TYPE_ONE_SHOT)));

  return NS_OK;
}

nsresult
FlyWebMDNSService::OnDiscoveryStopped(const nsACString& aServiceType)
{
  LOG_I("///////////////////////////////////////////");
  LOG_I("MDNSService::OnDiscoveryStopped(%s)", PromiseFlatCString(aServiceType).get());
  LOG_I("///////////////////////////////////////////");
  MOZ_ASSERT(mDiscoveryState == DISCOVERY_STOPPING);
  mDiscoveryState = DISCOVERY_IDLE;

  // If service discovery is inactive, then discard all results and do not proceed.
  if (!mDiscoveryActive) {
    mServiceMap.Clear();
    mNewServiceSet.Clear();
    return NS_OK;
  }

  // Process the service map, add to the pair map.
  for (auto iter = mServiceMap.Iter(); !iter.Done(); iter.Next()) {
    DiscoveredInfo* service = iter.UserData();

    if (!mNewServiceSet.Contains(service->mService.mServiceId)) {
      iter.Remove();
    }
  }

  // Notify FlyWebService of changed service list.
  mService->NotifyDiscoveredServicesChanged();

  // Start discovery again immediately.
  NS_WARN_IF(NS_FAILED(mDiscoveryStartTimer->InitWithCallback(this, 0, nsITimer::TYPE_ONE_SHOT)));

  return NS_OK;
}

nsresult
FlyWebMDNSService::OnServiceFound(nsIDNSServiceInfo* aServiceInfo)
{
  LogDNSInfo(aServiceInfo, "FlyWebMDNSService::OnServiceFound");

  // If discovery is not active, don't do anything with the result.
  // If there is no discovery underway, ignore this.
  if (!mDiscoveryActive || mDiscoveryState != DISCOVERY_RUNNING) {
    return NS_OK;
  }

  // Discovery is underway - resolve the service.
  nsresult rv = mDNSServiceDiscovery->ResolveService(aServiceInfo, this);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
FlyWebMDNSService::OnServiceLost(nsIDNSServiceInfo* aServiceInfo)
{
  LogDNSInfo(aServiceInfo, "FlyWebMDNSService::OnServiceLost");

  return NS_OK;
}

nsresult
FlyWebMDNSService::OnStartDiscoveryFailed(const nsACString& aServiceType, int32_t aErrorCode)
{
  LOG_E("MDNSService::OnStartDiscoveryFailed(%s): %d", PromiseFlatCString(aServiceType).get(), (int) aErrorCode);

  MOZ_ASSERT(mDiscoveryState == DISCOVERY_STARTING);
  mDiscoveryState = DISCOVERY_IDLE;
  mNumConsecutiveStartDiscoveryFailures++;

  // If discovery is active, and the number of consecutive failures is < 3, try starting again.
  if (mDiscoveryActive && mNumConsecutiveStartDiscoveryFailures < 3) {
    NS_WARN_IF(NS_FAILED(mDiscoveryStartTimer->InitWithCallback(this, 0, nsITimer::TYPE_ONE_SHOT)));
  }

  return NS_OK;
}

nsresult
FlyWebMDNSService::OnStopDiscoveryFailed(const nsACString& aServiceType, int32_t aErrorCode)
{
  LOG_E("MDNSService::OnStopDiscoveryFailed(%s)", PromiseFlatCString(aServiceType).get());
  MOZ_ASSERT(mDiscoveryState == DISCOVERY_STOPPING);
  mDiscoveryState = DISCOVERY_IDLE;

  // If discovery is active, start discovery again immediately.
  if (mDiscoveryActive) {
    NS_WARN_IF(NS_FAILED(mDiscoveryStartTimer->InitWithCallback(this, 0, nsITimer::TYPE_ONE_SHOT)));
  }

  return NS_OK;
}

static bool
IsAcceptableServiceAddress(const nsCString& addr)
{
  PRNetAddr prNetAddr;
  PRStatus status = PR_StringToNetAddr(addr.get(), &prNetAddr);
  if (status == PR_FAILURE) {
    return false;
  }
  // Only allow ipv4 addreses for now.
  return prNetAddr.raw.family == PR_AF_INET;
}

nsresult
FlyWebMDNSService::OnServiceResolved(nsIDNSServiceInfo* aServiceInfo)
{
  LogDNSInfo(aServiceInfo, "FlyWebMDNSService::OnServiceResolved");

  // If discovery is not active, don't do anything with the result.
  // If there is no discovery underway, ignore this resolve.
  if (!mDiscoveryActive || mDiscoveryState != DISCOVERY_RUNNING) {
    return NS_OK;
  }

  nsresult rv;

  nsCString address;
  rv = aServiceInfo->GetAddress(address);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!IsAcceptableServiceAddress(address)) {
    return NS_OK;
  }

  // Create a new serviceInfo and stuff it in the new service array.
  UniquePtr<DiscoveredInfo> svc(new DiscoveredInfo(aServiceInfo));
  mNewServiceSet.PutEntry(svc->mService.mServiceId);

  DiscoveredInfo* existingSvc =
    mServiceMap.Get(svc->mService.mServiceId);
  if (existingSvc) {
    // Update the underlying DNS service info, but leave the old object in place.
    existingSvc->mDNSServiceInfo = aServiceInfo;
  } else {
    DiscoveredInfo* info = svc.release();
    mServiceMap.Put(info->mService.mServiceId, info);
  }

  // Notify FlyWebService of changed service list.
  mService->NotifyDiscoveredServicesChanged();

  return NS_OK;
}

FlyWebMDNSService::DiscoveredInfo::DiscoveredInfo(nsIDNSServiceInfo* aDNSServiceInfo)
  : mDNSServiceInfo(aDNSServiceInfo)
{
  nsCString tmp;
  DebugOnly<nsresult> drv = aDNSServiceInfo->GetServiceName(tmp);
  MOZ_ASSERT(NS_SUCCEEDED(drv));
  CopyUTF8toUTF16(tmp, mService.mDisplayName);

  mService.mTransport = NS_LITERAL_STRING("mdns");

  drv = aDNSServiceInfo->GetServiceType(tmp);
  MOZ_ASSERT(NS_SUCCEEDED(drv));
  CopyUTF8toUTF16(tmp, mService.mServiceType);

  nsCOMPtr<nsIPropertyBag2> attrs;
  drv = aDNSServiceInfo->GetAttributes(getter_AddRefs(attrs));
  MOZ_ASSERT(NS_SUCCEEDED(drv));
  if (attrs) {
    attrs->GetPropertyAsAString(NS_LITERAL_STRING("cert"), mService.mCert);
    attrs->GetPropertyAsAString(NS_LITERAL_STRING("path"), mService.mPath);
  }

  // Construct a service id from the name, host, address, and port.
  nsCString cHost;
  drv = aDNSServiceInfo->GetHost(cHost);
  MOZ_ASSERT(NS_SUCCEEDED(drv));

  nsCString cAddress;
  drv = aDNSServiceInfo->GetAddress(cAddress);
  MOZ_ASSERT(NS_SUCCEEDED(drv));

  uint16_t port;
  drv = aDNSServiceInfo->GetPort(&port);
  MOZ_ASSERT(NS_SUCCEEDED(drv));
  nsAutoString portStr;
  portStr.AppendInt(port, 10);

  mService.mServiceId =
    NS_ConvertUTF8toUTF16(cAddress) +
    NS_LITERAL_STRING(":") +
    portStr +
    NS_LITERAL_STRING("|") +
    mService.mServiceType +
    NS_LITERAL_STRING("|") +
    NS_ConvertUTF8toUTF16(cHost) +
    NS_LITERAL_STRING("|") +
    mService.mDisplayName;
}


nsresult
FlyWebMDNSService::OnResolveFailed(nsIDNSServiceInfo* aServiceInfo, int32_t aErrorCode)
{
  LogDNSInfo(aServiceInfo, "FlyWebMDNSService::OnResolveFailed");

  return NS_OK;
}

nsresult
FlyWebMDNSService::OnServiceRegistered(nsIDNSServiceInfo* aServiceInfo)
{
  LogDNSInfo(aServiceInfo, "FlyWebMDNSService::OnServiceRegistered");

  nsCString cName;
  if (NS_WARN_IF(NS_FAILED(aServiceInfo->GetServiceName(cName)))) {
    return NS_ERROR_FAILURE;
  }

  nsString name = NS_ConvertUTF8toUTF16(cName);
  RefPtr<FlyWebPublishedServer> existingServer =
    FlyWebService::GetOrCreate()->FindPublishedServerByName(name);
  if (!existingServer) {
    return NS_ERROR_FAILURE;
  }

  existingServer->DiscoveryStarted(NS_OK);

  return NS_OK;
}

nsresult
FlyWebMDNSService::OnServiceUnregistered(nsIDNSServiceInfo* aServiceInfo)
{
  LogDNSInfo(aServiceInfo, "FlyWebMDNSService::OnServiceUnregistered");

  nsCString cName;
  if (NS_WARN_IF(NS_FAILED(aServiceInfo->GetServiceName(cName)))) {
    return NS_ERROR_FAILURE;
  }

  nsString name = NS_ConvertUTF8toUTF16(cName);
  RefPtr<FlyWebPublishedServer> existingServer =
    FlyWebService::GetOrCreate()->FindPublishedServerByName(name);
  if (!existingServer) {
    return NS_ERROR_FAILURE;
  }

  LOG_I("OnServiceRegistered(MDNS): De-advertised server with name %s.", cName.get());

  return NS_OK;
}

nsresult
FlyWebMDNSService::OnRegistrationFailed(nsIDNSServiceInfo* aServiceInfo, int32_t errorCode)
{
  LogDNSInfo(aServiceInfo, "FlyWebMDNSService::OnRegistrationFailed");

  nsCString cName;
  if (NS_WARN_IF(NS_FAILED(aServiceInfo->GetServiceName(cName)))) {
    return NS_ERROR_FAILURE;
  }

  nsString name = NS_ConvertUTF8toUTF16(cName);
  RefPtr<FlyWebPublishedServer> existingServer =
    FlyWebService::GetOrCreate()->FindPublishedServerByName(name);
  if (!existingServer) {
    return NS_ERROR_FAILURE;
  }

  LOG_I("OnServiceRegistered(MDNS): Registration of server with name %s failed.", cName.get());

  // Remove the nsICancelable from the published server.
  existingServer->DiscoveryStarted(NS_ERROR_FAILURE);
  return NS_OK;
}

nsresult
FlyWebMDNSService::OnUnregistrationFailed(nsIDNSServiceInfo* aServiceInfo, int32_t errorCode)
{
  LogDNSInfo(aServiceInfo, "FlyWebMDNSService::OnUnregistrationFailed");

  nsCString cName;
  if (NS_WARN_IF(NS_FAILED(aServiceInfo->GetServiceName(cName)))) {
    return NS_ERROR_FAILURE;
  }

  nsString name = NS_ConvertUTF8toUTF16(cName);
  RefPtr<FlyWebPublishedServer> existingServer =
    FlyWebService::GetOrCreate()->FindPublishedServerByName(name);
  if (!existingServer) {
    return NS_ERROR_FAILURE;
  }

  LOG_I("OnServiceRegistered(MDNS): Un-Advertisement of server with name %s failed.", cName.get());
  return NS_OK;
}

nsresult
FlyWebMDNSService::Notify(nsITimer* timer)
{
  if (timer == mDiscoveryStopTimer.get()) {
    LOG_I("MDNSService::Notify() got discovery stop timeout");
    // Internet discovery stop timer has fired.
    nsresult rv = StopDiscovery();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  if (timer == mDiscoveryStartTimer.get()) {
    LOG_I("MDNSService::Notify() got discovery start timeout");
    // Internet discovery start timer has fired.
    nsresult rv = StartDiscovery();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    return NS_OK;
  }

  LOG_E("MDNSService::Notify got unknown timeout.");
  return NS_OK;
}

nsresult
FlyWebMDNSService::Init()
{
  MOZ_ASSERT(mDiscoveryState == DISCOVERY_IDLE);

  mDiscoveryStartTimer = do_CreateInstance("@mozilla.org/timer;1");
  if (!mDiscoveryStartTimer) {
    return NS_ERROR_FAILURE;
  }

  mDiscoveryStopTimer = do_CreateInstance("@mozilla.org/timer;1");
  if (!mDiscoveryStopTimer) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  mDNSServiceDiscovery = do_GetService(DNSSERVICEDISCOVERY_CONTRACT_ID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
FlyWebMDNSService::StartDiscovery()
{
  nsresult rv;

  // Always cancel the timer.
  rv = mDiscoveryStartTimer->Cancel();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG_E("FlyWeb failed to cancel DNS service discovery start timer.");
  }

  // If discovery is not idle, don't start it.
  if (mDiscoveryState != DISCOVERY_IDLE) {
    return NS_OK;
  }

  LOG_I("FlyWeb starting dicovery.");
  mDiscoveryState = DISCOVERY_STARTING;

  // start the discovery.
  rv = mDNSServiceDiscovery->StartDiscovery(mServiceType, this,
                                            getter_AddRefs(mCancelDiscovery));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG_E("FlyWeb failed to start DNS service discovery.");
    return rv;
  }

  return NS_OK;
}

nsresult
FlyWebMDNSService::StopDiscovery()
{
  nsresult rv;

  // Always cancel the timer.
  rv = mDiscoveryStopTimer->Cancel();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG_E("FlyWeb failed to cancel DNS service discovery stop timer.");
  }

  // If discovery is not running, do nothing.
  if (mDiscoveryState != DISCOVERY_RUNNING) {
    return NS_OK;
  }

  LOG_I("FlyWeb stopping dicovery.");

  // Mark service discovery as stopping.
  mDiscoveryState = DISCOVERY_STOPPING;

  if (mCancelDiscovery) {
    LOG_I("MDNSService::StopDiscovery() - mCancelDiscovery exists!");
    nsCOMPtr<nsICancelable> cancelDiscovery = mCancelDiscovery.forget();
    rv = cancelDiscovery->Cancel(NS_ERROR_ABORT);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      LOG_E("FlyWeb failed to cancel DNS stop service discovery.");
    }
  } else {
    LOG_I("MDNSService::StopDiscovery() - mCancelDiscovery does not exist!");
    mDiscoveryState = DISCOVERY_IDLE;
  }

  return NS_OK;
}

void
FlyWebMDNSService::ListDiscoveredServices(nsTArray<FlyWebDiscoveredService>& aServices)
{
  for (auto iter = mServiceMap.Iter(); !iter.Done(); iter.Next()) {
    aServices.AppendElement(iter.UserData()->mService);
  }
}

bool
FlyWebMDNSService::HasService(const nsAString& aServiceId)
{
  return mServiceMap.Contains(aServiceId);
}

nsresult
FlyWebMDNSService::PairWithService(const nsAString& aServiceId,
                                   UniquePtr<FlyWebService::PairedInfo>& aInfo)
{
  MOZ_ASSERT(HasService(aServiceId));

  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> uuidgen =
    do_GetService("@mozilla.org/uuid-generator;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsID id;
  rv = uuidgen->GenerateUUIDInPlace(&id);
  NS_ENSURE_SUCCESS(rv, rv);

  aInfo.reset(new FlyWebService::PairedInfo());

  char uuidChars[NSID_LENGTH];
  id.ToProvidedString(uuidChars);
  CopyUTF8toUTF16(Substring(uuidChars + 1, uuidChars + NSID_LENGTH - 2),
                  aInfo->mService.mHostname);

  DiscoveredInfo* discInfo = mServiceMap.Get(aServiceId);

  nsAutoString url;
  if (discInfo->mService.mCert.IsEmpty()) {
    url.AssignLiteral("http://");
  } else {
    url.AssignLiteral("https://");
  }
  url.Append(aInfo->mService.mHostname + NS_LITERAL_STRING("/"));
  nsCOMPtr<nsIURI> uiURL;
  NS_NewURI(getter_AddRefs(uiURL), url);
  MOZ_ASSERT(uiURL);
  if (!discInfo->mService.mPath.IsEmpty()) {
    nsCOMPtr<nsIURI> tmp = uiURL.forget();
    NS_NewURI(getter_AddRefs(uiURL), discInfo->mService.mPath, nullptr, tmp);
  }
  if (uiURL) {
    nsAutoCString spec;
    uiURL->GetSpec(spec);
    CopyUTF8toUTF16(spec, aInfo->mService.mUiUrl);
  }

  aInfo->mService.mDiscoveredService = discInfo->mService;
  aInfo->mDNSServiceInfo = discInfo->mDNSServiceInfo;

  return NS_OK;
}

nsresult
FlyWebMDNSService::StartDiscoveryOf(FlyWebPublishedServer* aServer)
{

  RefPtr<FlyWebPublishedServer> existingServer =
    FlyWebService::GetOrCreate()->FindPublishedServerByName(aServer->Name());
  MOZ_ASSERT(existingServer);

  // Advertise the service via mdns.
  RefPtr<net::nsDNSServiceInfo> serviceInfo(new net::nsDNSServiceInfo());

  serviceInfo->SetPort(aServer->Port());
  serviceInfo->SetServiceType(mServiceType);

  nsCString certKey;
  aServer->GetCertKey(certKey);
  nsString uiURL;
  aServer->GetUiUrl(uiURL);

  if (!uiURL.IsEmpty() || !certKey.IsEmpty()) {
    RefPtr<nsHashPropertyBag> attrs = new nsHashPropertyBag();
    if (!uiURL.IsEmpty()) {
      attrs->SetPropertyAsAString(NS_LITERAL_STRING("path"), uiURL);
    }
    if (!certKey.IsEmpty()) {
      attrs->SetPropertyAsACString(NS_LITERAL_STRING("cert"), certKey);
    }
    serviceInfo->SetAttributes(attrs);
  }

  nsCString cstrName = NS_ConvertUTF16toUTF8(aServer->Name());
  LOG_I("MDNSService::StartDiscoveryOf() advertising service %s", cstrName.get());
  serviceInfo->SetServiceName(cstrName);

  LogDNSInfo(serviceInfo, "FlyWebMDNSService::StartDiscoveryOf");

  // Advertise the service.
  nsCOMPtr<nsICancelable> cancelRegister;
  nsresult rv = mDNSServiceDiscovery->
    RegisterService(serviceInfo, this, getter_AddRefs(cancelRegister));
  NS_ENSURE_SUCCESS(rv, rv);

  // All done.
  aServer->SetCancelRegister(cancelRegister);

  return NS_OK;
}

void
FlyWebMDNSService::EnsureDiscoveryStarted()
{
  mDiscoveryActive = true;
  // If state is idle, start discovery immediately.
  if (mDiscoveryState == DISCOVERY_IDLE) {
    StartDiscovery();
  }
}

void
FlyWebMDNSService::EnsureDiscoveryStopped()
{
  // All we need to do is set the flag to false.
  // If current state is IDLE, it's already the correct state.
  // Otherwise, the handlers for the internal state
  // transitions will check this flag and drive the state
  // towards IDLE.
  mDiscoveryActive = false;
}

static StaticRefPtr<FlyWebService> gFlyWebService;

NS_IMPL_ISUPPORTS(FlyWebService, nsIObserver)

FlyWebService::FlyWebService()
  : mMonitor("FlyWebService::mMonitor")
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "inner-window-destroyed", false);
  }
}

FlyWebService::~FlyWebService()
{
}

FlyWebService*
FlyWebService::GetExisting()
{
  return gFlyWebService;
}

FlyWebService*
FlyWebService::GetOrCreate()
{
  if (!gFlyWebService) {
    gFlyWebService = new FlyWebService();
    ErrorResult rv = gFlyWebService->Init();
    if (rv.Failed()) {
      gFlyWebService = nullptr;
      return nullptr;
    }
  }
  return gFlyWebService;
}

ErrorResult
FlyWebService::Init()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mMDNSHttpService) {
    mMDNSHttpService = new FlyWebMDNSService(this, NS_LITERAL_CSTRING("_http._tcp."));
    ErrorResult rv;

    rv = mMDNSHttpService->Init();
    if (rv.Failed()) {
      LOG_E("FlyWebService failed to initialize MDNS _http._tcp.");
      mMDNSHttpService = nullptr;
      rv.SuppressException();
    }
  }

  if (!mMDNSFlywebService) {
    mMDNSFlywebService = new FlyWebMDNSService(this, NS_LITERAL_CSTRING("_flyweb._tcp."));
    ErrorResult rv;

    rv = mMDNSFlywebService->Init();
    if (rv.Failed()) {
      LOG_E("FlyWebService failed to initialize MDNS _flyweb._tcp.");
      mMDNSFlywebService = nullptr;
      rv.SuppressException();
    }
  }

  return ErrorResult(NS_OK);
}

already_AddRefed<Promise>
FlyWebService::PublishServer(const nsAString& aName,
                             const FlyWebPublishOptions& aOptions,
                             nsPIDOMWindowInner* aWindow,
                             ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aWindow);
  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Scan uiUrl for illegal characters

  RefPtr<FlyWebPublishedServer> existingServer =
    FlyWebService::GetOrCreate()->FindPublishedServerByName(aName);
  if (existingServer) {
    LOG_I("PublishServer: Trying to publish server with already-existing name %s.",
          NS_ConvertUTF16toUTF8(aName).get());
    promise->MaybeReject(NS_ERROR_FAILURE);
    return promise.forget();
  }

  RefPtr<FlyWebPublishedServer> server =
    new FlyWebPublishedServer(aWindow, aName, aOptions, promise);

  mServers.AppendElement(server);

  return promise.forget();
}

already_AddRefed<FlyWebPublishedServer>
FlyWebService::FindPublishedServerByName(
        const nsAString& aName)
{
  MOZ_ASSERT(NS_IsMainThread());
  for (FlyWebPublishedServer* publishedServer : mServers) {
    if (publishedServer->Name().Equals(aName)) {
      RefPtr<FlyWebPublishedServer> server = publishedServer;
      return server.forget();
    }
  }
  return nullptr;
}

void
FlyWebService::RegisterDiscoveryManager(FlyWebDiscoveryManager* aDiscoveryManager)
{
  MOZ_ASSERT(NS_IsMainThread());
  mDiscoveryManagerTable.PutEntry(aDiscoveryManager);
  if (mMDNSHttpService) {
    mMDNSHttpService->EnsureDiscoveryStarted();
  }
  if (mMDNSFlywebService) {
    mMDNSFlywebService->EnsureDiscoveryStarted();
  }
}

void
FlyWebService::UnregisterDiscoveryManager(FlyWebDiscoveryManager* aDiscoveryManager)
{
  MOZ_ASSERT(NS_IsMainThread());
  mDiscoveryManagerTable.RemoveEntry(aDiscoveryManager);
  if (mDiscoveryManagerTable.IsEmpty()) {
    if (mMDNSHttpService) {
      mMDNSHttpService->EnsureDiscoveryStopped();
    }
    if (mMDNSFlywebService) {
      mMDNSFlywebService->EnsureDiscoveryStopped();
    }
  }
}

NS_IMETHODIMP
FlyWebService::Observe(nsISupports* aSubject, const char* aTopic,
                       const char16_t* aData)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (strcmp(aTopic, "inner-window-destroyed")) {
    return NS_OK;
  }

  nsCOMPtr<nsISupportsPRUint64> wrapper = do_QueryInterface(aSubject);
  NS_ENSURE_TRUE(wrapper, NS_ERROR_FAILURE);

  uint64_t innerID;
  nsresult rv = wrapper->GetData(&innerID);
  NS_ENSURE_SUCCESS(rv, rv);

  for (FlyWebPublishedServer* server : mServers) {
    if (server->OwnerWindowID() == innerID) {
      server->Close();
    }
  }

  return NS_OK;
}

void
FlyWebService::UnregisterServer(FlyWebPublishedServer* aServer)
{
  MOZ_ASSERT(NS_IsMainThread());
  DebugOnly<bool> removed = mServers.RemoveElement(aServer);
  MOZ_ASSERT(removed);
}

bool
FlyWebService::HasConnectionOrServer(uint64_t aWindowID)
{
  MOZ_ASSERT(NS_IsMainThread());
  for (FlyWebPublishedServer* server : mServers) {
    nsPIDOMWindowInner* win = server->GetOwner();
    if (win && win->WindowID() == aWindowID) {
      return true;
    }
  }

  return false;
}

void
FlyWebService::NotifyDiscoveredServicesChanged()
{
  // Process the service map, add to the pair map.
  for (auto iter = mDiscoveryManagerTable.Iter(); !iter.Done(); iter.Next()) {
    iter.Get()->GetKey()->NotifyDiscoveredServicesChanged();
  }
}

void
FlyWebService::ListDiscoveredServices(nsTArray<FlyWebDiscoveredService>& aServices)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mMDNSHttpService) {
    mMDNSHttpService->ListDiscoveredServices(aServices);
  }
  if (mMDNSFlywebService) {
    mMDNSFlywebService->ListDiscoveredServices(aServices);
  }
}

void
FlyWebService::PairWithService(const nsAString& aServiceId,
                               FlyWebPairingCallback& aCallback)
{
  MOZ_ASSERT(NS_IsMainThread());
  // See if we have already paired with this service.  If so, re-use the
  // FlyWebPairedService for that.
  {
    ReentrantMonitorAutoEnter pairedMapLock(mMonitor);
    for (auto iter = mPairedServiceTable.Iter(); !iter.Done(); iter.Next()) {
      PairedInfo* pairInfo = iter.UserData();
      if (pairInfo->mService.mDiscoveredService.mServiceId.Equals(aServiceId)) {
        ErrorResult er;
        ReentrantMonitorAutoExit pairedMapRelease(mMonitor);
        aCallback.PairingSucceeded(pairInfo->mService, er);
        ENSURE_SUCCESS_VOID(er);
        return;
      }
    }
  }

  UniquePtr<PairedInfo> pairInfo;

  nsresult rv = NS_OK;
  bool notFound = false;
  if (mMDNSHttpService && mMDNSHttpService->HasService(aServiceId)) {
    rv = mMDNSHttpService->PairWithService(aServiceId, pairInfo);
  } else if (mMDNSFlywebService && mMDNSFlywebService->HasService(aServiceId)) {
    rv = mMDNSFlywebService->PairWithService(aServiceId, pairInfo);
  } else {
    notFound = true;
  }

  if (NS_FAILED(rv)) {
    ErrorResult result;
    result.Throw(rv);
    const nsAString& reason = NS_LITERAL_STRING("Error pairing.");
    aCallback.PairingFailed(reason, result);
    ENSURE_SUCCESS_VOID(result);
    return;
  }

  if (!pairInfo) {
    ErrorResult res;
    const nsAString& reason = notFound ?
      NS_LITERAL_STRING("No such service.") :
      NS_LITERAL_STRING("Error pairing.");
    aCallback.PairingFailed(reason, res);
    ENSURE_SUCCESS_VOID(res);
    return;
  }

  // Add fingerprint to certificate override database.
  if (!pairInfo->mService.mDiscoveredService.mCert.IsEmpty()) {
    nsCOMPtr<nsICertOverrideService> override =
      do_GetService("@mozilla.org/security/certoverride;1");
    if (!override ||
        NS_FAILED(override->RememberTemporaryValidityOverrideUsingFingerprint(
          NS_ConvertUTF16toUTF8(pairInfo->mService.mHostname),
          -1,
          NS_ConvertUTF16toUTF8(pairInfo->mService.mDiscoveredService.mCert),
          nsICertOverrideService::ERROR_UNTRUSTED |
          nsICertOverrideService::ERROR_MISMATCH))) {
      ErrorResult res;
      aCallback.PairingFailed(NS_LITERAL_STRING("Error adding certificate override."), res);
      ENSURE_SUCCESS_VOID(res);
      return;
    }
  }

  // Grab a weak reference to the PairedInfo so that we can
  // use it even after ownership has been transferred to mPairedServiceTable
  PairedInfo* pairInfoWeak = pairInfo.release();

  {
    ReentrantMonitorAutoEnter pairedMapLock(mMonitor);
    mPairedServiceTable.Put(
      NS_ConvertUTF16toUTF8(pairInfoWeak->mService.mHostname), pairInfoWeak);
  }

  ErrorResult er;
  aCallback.PairingSucceeded(pairInfoWeak->mService, er);
  ENSURE_SUCCESS_VOID(er);
}

nsresult
FlyWebService::CreateTransportForHost(const char **types,
                                      uint32_t typeCount,
                                      const nsACString &host,
                                      int32_t port,
                                      const nsACString &hostRoute,
                                      int32_t portRoute,
                                      nsIProxyInfo *proxyInfo,
                                      nsISocketTransport **result)
{
  // This might be called on background threads

  *result = nullptr;

  nsCString ipAddrString;
  uint16_t discPort;

  {
    ReentrantMonitorAutoEnter pairedMapLock(mMonitor);

    PairedInfo* info = mPairedServiceTable.Get(host);

    if (!info) {
      return NS_OK;
    }

    // Get the ip address of the underlying service.
    info->mDNSServiceInfo->GetAddress(ipAddrString);
    info->mDNSServiceInfo->GetPort(&discPort);
  }

  // Parse it into an NetAddr.
  PRNetAddr prNetAddr;
  PRStatus status = PR_StringToNetAddr(ipAddrString.get(), &prNetAddr);
  NS_ENSURE_FALSE(status == PR_FAILURE, NS_ERROR_FAILURE);

  // Convert PRNetAddr to NetAddr.
  mozilla::net::NetAddr netAddr;
  PRNetAddrToNetAddr(&prNetAddr, &netAddr);
  netAddr.inet.port = htons(discPort);

  RefPtr<mozilla::net::nsSocketTransport> trans = new mozilla::net::nsSocketTransport();
  nsresult rv = trans->InitPreResolved(
    types, typeCount, host, port, hostRoute, portRoute, proxyInfo, &netAddr);
  NS_ENSURE_SUCCESS(rv, rv);

  trans.forget(result);
  return NS_OK;
}

void
FlyWebService::StartDiscoveryOf(FlyWebPublishedServer* aServer)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsresult rv = mMDNSFlywebService ?
    mMDNSFlywebService->StartDiscoveryOf(aServer) :
    NS_ERROR_FAILURE;

  if (NS_FAILED(rv)) {
    aServer->DiscoveryStarted(rv);
  }
}

} // namespace dom
} // namespace mozilla
