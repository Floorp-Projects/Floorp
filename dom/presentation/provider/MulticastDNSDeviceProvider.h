/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_presentation_provider_MulticastDNSDeviceProvider_h
#define mozilla_dom_presentation_provider_MulticastDNSDeviceProvider_h

#include "mozilla/nsRefPtr.h"
#include "nsCOMPtr.h"
#include "nsICancelable.h"
#include "nsIDNSServiceDiscovery.h"
#include "nsIObserver.h"
#include "nsIPresentationDeviceProvider.h"
#include "nsITCPPresentationServer.h"
#include "nsITimer.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWeakPtr.h"

namespace mozilla {
namespace dom {
namespace presentation {

class DNSServiceWrappedListener;
class MulticastDNSService;

class MulticastDNSDeviceProvider final
  : public nsIPresentationDeviceProvider
  , public nsIDNSServiceDiscoveryListener
  , public nsIDNSRegistrationListener
  , public nsIDNSServiceResolveListener
  , public nsITCPPresentationServerListener
  , public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONDEVICEPROVIDER
  NS_DECL_NSIDNSSERVICEDISCOVERYLISTENER
  NS_DECL_NSIDNSREGISTRATIONLISTENER
  NS_DECL_NSIDNSSERVICERESOLVELISTENER
  NS_DECL_NSITCPPRESENTATIONSERVERLISTENER
  NS_DECL_NSIOBSERVER

  explicit MulticastDNSDeviceProvider() = default;
  nsresult Init();
  nsresult Uninit();

private:
  enum class DeviceState : uint32_t {
    eUnknown,
    eActive
  };

  struct Device final {
    explicit Device(const nsACString& aId, DeviceState aState)
      : id(aId), state(aState)
    {
    }

    nsCString id;
    DeviceState state;
  };

  struct DeviceIdComparator {
    bool Equals(const Device& aA, const Device& aB) const {
      return aA.id == aB.id;
    }
  };

  virtual ~MulticastDNSDeviceProvider();
  nsresult RegisterService();
  nsresult UnregisterService(nsresult aReason);
  nsresult StopDiscovery(nsresult aReason);

  // device manipulation
  nsresult AddDevice(const nsACString& aServiceName,
                     const nsACString& aServiceType,
                     const nsACString& aHost,
                     const uint16_t aPort);
  nsresult UpdateDevice(const uint32_t aIndex,
                        const nsACString& aServiceName,
                        const nsACString& aServiceType,
                        const nsACString& aHost,
                        const uint16_t aPort);
  nsresult RemoveDevice(const uint32_t aIndex);
  bool FindDevice(const nsACString& aId,
                      uint32_t& aIndex);

  void MarkAllDevicesUnknown();
  void ClearUnknownDevices();
  void ClearDevices();

  // preferences
  nsresult OnDiscoveryChanged(bool aEnabled);
  nsresult OnDiscoveryTimeoutChanged(uint32_t aTimeoutMs);
  nsresult OnDiscoverableChanged(bool aEnabled);
  nsresult OnServiceNameChanged(const nsACString& aServiceName);

  bool mInitialized = false;
  nsWeakPtr mDeviceListener;
  nsCOMPtr<nsITCPPresentationServer> mPresentationServer;
  nsCOMPtr<nsIDNSServiceDiscovery> mMulticastDNS;
  nsRefPtr<DNSServiceWrappedListener> mWrappedListener;

  nsCOMPtr<nsICancelable> mDiscoveryRequest;
  nsCOMPtr<nsICancelable> mRegisterRequest;

  nsTArray<Device> mDevices;

  bool mDiscoveryEnabled = false;
  bool mIsDiscovering = false;
  uint32_t mDiscveryTimeoutMs;
  nsCOMPtr<nsITimer> mDiscoveryTimer;

  bool mDiscoverable = false;

  nsCString mServiceName;
  nsCString mRegisteredName;
};

} // namespace presentation
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_presentation_provider_MulticastDNSDeviceProvider_h
