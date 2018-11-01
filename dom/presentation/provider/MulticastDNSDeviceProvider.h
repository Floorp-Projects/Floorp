/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_presentation_provider_MulticastDNSDeviceProvider_h
#define mozilla_dom_presentation_provider_MulticastDNSDeviceProvider_h

#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsICancelable.h"
#include "nsIDNSServiceDiscovery.h"
#include "nsIObserver.h"
#include "nsIPresentationDevice.h"
#include "nsIPresentationDeviceProvider.h"
#include "nsIPresentationControlService.h"
#include "nsITimer.h"
#include "nsIWeakReferenceUtils.h"
#include "nsString.h"
#include "nsTArray.h"

class nsITCPDeviceInfo;

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
  , public nsIPresentationControlServerListener
  , public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONDEVICEPROVIDER
  NS_DECL_NSIDNSSERVICEDISCOVERYLISTENER
  NS_DECL_NSIDNSREGISTRATIONLISTENER
  NS_DECL_NSIDNSSERVICERESOLVELISTENER
  NS_DECL_NSIPRESENTATIONCONTROLSERVERLISTENER
  NS_DECL_NSIOBSERVER

  explicit MulticastDNSDeviceProvider() = default;
  nsresult Init();
  nsresult Uninit();

private:
  enum class DeviceState : uint32_t {
    eUnknown,
    eActive
  };

  class Device final : public nsIPresentationDevice
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPRESENTATIONDEVICE

    explicit Device(const nsACString& aId,
                    const nsACString& aName,
                    const nsACString& aType,
                    const nsACString& aAddress,
                    const uint16_t aPort,
                    const nsACString& aCertFingerprint,
                    DeviceState aState,
                    MulticastDNSDeviceProvider* aProvider)
      : mId(aId)
      , mName(aName)
      , mType(aType)
      , mAddress(aAddress)
      , mPort(aPort)
      , mCertFingerprint(aCertFingerprint)
      , mState(aState)
      , mProvider(aProvider)
    {
    }

    const nsCString& Id() const
    {
      return mId;
    }

    const nsCString& Address() const
    {
      return mAddress;
    }

    uint16_t Port() const
    {
      return mPort;
    }

    const nsCString& CertFingerprint() const
    {
      return mCertFingerprint;
    }

    DeviceState State() const
    {
      return mState;
    }

    void ChangeState(DeviceState aState)
    {
      mState = aState;
    }

    void Update(const nsACString& aName,
                const nsACString& aType,
                const nsACString& aAddress,
                const uint16_t aPort,
                const nsACString& aCertFingerprint)
    {
      mName = aName;
      mType = aType;
      mAddress = aAddress;
      mPort = aPort;
      mCertFingerprint = aCertFingerprint;
    }

  private:
    virtual ~Device() = default;

    nsCString mId;
    nsCString mName;
    nsCString mType;
    nsCString mAddress;
    uint16_t mPort;
    nsCString mCertFingerprint;
    DeviceState mState;
    MulticastDNSDeviceProvider* mProvider;
  };

  struct DeviceIdComparator {
    bool Equals(const RefPtr<Device>& aA, const RefPtr<Device>& aB) const {
      return aA->Id() == aB->Id();
    }
  };

  struct DeviceAddressComparator {
    bool Equals(const RefPtr<Device>& aA, const RefPtr<Device>& aB) const {
      return aA->Address() == aB->Address();
    }
  };

  virtual ~MulticastDNSDeviceProvider();
  nsresult StartServer();
  nsresult StopServer();
  void AbortServerRetry();
  nsresult RegisterMDNSService();
  nsresult UnregisterMDNSService(nsresult aReason);
  nsresult StopDiscovery(nsresult aReason);
  nsresult Connect(Device* aDevice,
                   nsIPresentationControlChannel** aRetVal);
  bool IsCompatibleServer(nsIDNSServiceInfo* aServiceInfo);

  // device manipulation
  nsresult AddDevice(const nsACString& aId,
                     const nsACString& aServiceName,
                     const nsACString& aServiceType,
                     const nsACString& aAddress,
                     const uint16_t aPort,
                     const nsACString& aCertFingerprint);
  nsresult UpdateDevice(const uint32_t aIndex,
                        const nsACString& aServiceName,
                        const nsACString& aServiceType,
                        const nsACString& aAddress,
                        const uint16_t aPort,
                        const nsACString& aCertFingerprint);
  nsresult RemoveDevice(const uint32_t aIndex);
  bool FindDeviceById(const nsACString& aId,
                      uint32_t& aIndex);

  bool FindDeviceByAddress(const nsACString& aAddress,
                           uint32_t& aIndex);

  already_AddRefed<Device>
  GetOrCreateDevice(nsITCPDeviceInfo* aDeviceInfo);

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
  nsCOMPtr<nsIPresentationControlService> mPresentationService;
  nsCOMPtr<nsIDNSServiceDiscovery> mMulticastDNS;
  RefPtr<DNSServiceWrappedListener> mWrappedListener;

  nsCOMPtr<nsICancelable> mDiscoveryRequest;
  nsCOMPtr<nsICancelable> mRegisterRequest;

  nsTArray<RefPtr<Device>> mDevices;

  bool mDiscoveryEnabled = false;
  bool mIsDiscovering = false;
  uint32_t mDiscoveryTimeoutMs;
  nsCOMPtr<nsITimer> mDiscoveryTimer;

  bool mDiscoverable = false;
  bool mDiscoverableEncrypted = false;
  bool mIsServerRetrying = false;
  uint32_t mServerRetryMs;
  nsCOMPtr<nsITimer> mServerRetryTimer;

  nsCString mServiceName;
  nsCString mRegisteredName;
};

} // namespace presentation
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_presentation_provider_MulticastDNSDeviceProvider_h
