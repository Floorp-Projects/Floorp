/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FlyWebService_h
#define mozilla_dom_FlyWebService_h

#include "nsISupportsImpl.h"
#include "mozilla/ErrorResult.h"
#include "nsIProtocolHandler.h"
#include "nsDataHashtable.h"
#include "nsClassHashtable.h"
#include "nsIObserver.h"
#include "mozilla/MozPromise.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/dom/FlyWebDiscoveryManagerBinding.h"
#include "nsITimer.h"
#include "nsICancelable.h"
#include "nsIDNSServiceDiscovery.h"

class nsPIDOMWindowInner;
class nsIProxyInfo;
class nsISocketTransport;

namespace mozilla {
namespace dom {

struct FlyWebPublishOptions;
struct FlyWebFilter;
class FlyWebPublishedServer;
class FlyWebPublishedServerImpl;
class FlyWebPairingCallback;
class FlyWebDiscoveryManager;
class FlyWebMDNSService;

typedef MozPromise<RefPtr<FlyWebPublishedServer>, nsresult, false>
  FlyWebPublishPromise;

class FlyWebService final : public nsIObserver
{
  friend class FlyWebMDNSService;
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static FlyWebService* GetExisting();
  static FlyWebService* GetOrCreate();
  static already_AddRefed<FlyWebService> GetOrCreateAddRefed()
  {
    return do_AddRef(GetOrCreate());
  }

  already_AddRefed<FlyWebPublishPromise>
    PublishServer(const nsAString& aName,
                  const FlyWebPublishOptions& aOptions,
                  nsPIDOMWindowInner* aWindow);

  void UnregisterServer(FlyWebPublishedServer* aServer);

  bool HasConnectionOrServer(uint64_t aWindowID);

  void ListDiscoveredServices(nsTArray<FlyWebDiscoveredService>& aServices);
  void PairWithService(const nsAString& aServiceId, FlyWebPairingCallback& aCallback);
  nsresult CreateTransportForHost(const char **types,
                                  uint32_t typeCount,
                                  const nsACString &host,
                                  int32_t port,
                                  const nsACString &hostRoute,
                                  int32_t portRoute,
                                  nsIProxyInfo *proxyInfo,
                                  nsISocketTransport **result);

  already_AddRefed<FlyWebPublishedServer> FindPublishedServerByName(
            const nsAString& aName);

  void RegisterDiscoveryManager(FlyWebDiscoveryManager* aDiscoveryManager);
  void UnregisterDiscoveryManager(FlyWebDiscoveryManager* aDiscoveryManager);

  // Should only be called by FlyWebPublishedServerImpl
  void StartDiscoveryOf(FlyWebPublishedServerImpl* aServer);

private:
  FlyWebService();
  ~FlyWebService();

  ErrorResult Init();

  void NotifyDiscoveredServicesChanged();

  // Might want to make these hashes for perf
  nsTArray<FlyWebPublishedServer*> mServers;

  RefPtr<FlyWebMDNSService> mMDNSHttpService;
  RefPtr<FlyWebMDNSService> mMDNSFlywebService;

  struct PairedInfo
  {
    FlyWebPairedService mService;
    nsCOMPtr<nsIDNSServiceInfo> mDNSServiceInfo;
  };
  nsClassHashtable<nsCStringHashKey, PairedInfo>
    mPairedServiceTable;
  ReentrantMonitor mMonitor; // Protecting mPairedServiceTable

  nsTHashtable<nsPtrHashKey<FlyWebDiscoveryManager>> mDiscoveryManagerTable;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FlyWebService_h
