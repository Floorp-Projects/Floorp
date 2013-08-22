/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothadapter_h__
#define mozilla_dom_bluetooth_bluetoothadapter_h__

#include "mozilla/Attributes.h"
#include "BluetoothCommon.h"
#include "BluetoothPropertyContainer.h"
#include "nsCOMPtr.h"
#include "nsDOMEventTargetHelper.h"
#include "nsIDOMBluetoothDevice.h"

namespace mozilla {
namespace dom {
class DOMRequest;
struct MediaMetaData;
struct MediaPlayStatus;
}
}

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothDevice;
class BluetoothSignal;
class BluetoothNamedValue;
class BluetoothValue;

class BluetoothAdapter : public nsDOMEventTargetHelper
                       , public BluetoothSignalObserver
                       , public BluetoothPropertyContainer
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(BluetoothAdapter,
                                                         nsDOMEventTargetHelper)

  static already_AddRefed<BluetoothAdapter>
  Create(nsPIDOMWindow* aOwner, const BluetoothValue& aValue);

  void Notify(const BluetoothSignal& aParam);

  void Unroot();
  virtual void SetPropertyByValue(const BluetoothNamedValue& aValue) MOZ_OVERRIDE;

  void GetAddress(nsString& aAddress) const
  {
    aAddress = mAddress;
  }

  uint32_t
  Class() const
  {
    return mClass;
  }

  void
  GetName(nsString& aName) const
  {
    aName = mName;
  }

  bool
  Discovering() const
  {
    return mDiscovering;
  }

  bool
  Discoverable() const
  {
    return mDiscoverable;
  }

  uint32_t
  DiscoverableTimeout() const
  {
    return mDiscoverableTimeout;
  }

  JS::Value GetDevices(JSContext* aContext, ErrorResult& aRv);
  JS::Value GetUuids(JSContext* aContext, ErrorResult& aRv);

  already_AddRefed<mozilla::dom::DOMRequest>
    SetName(const nsAString& aName, ErrorResult& aRv);

  already_AddRefed<DOMRequest>
    SetDiscoverable(bool aDiscoverable, ErrorResult& aRv);
  already_AddRefed<DOMRequest>
    SetDiscoverableTimeout(uint32_t aTimeout, ErrorResult& aRv);
  already_AddRefed<DOMRequest> StartDiscovery(ErrorResult& aRv);
  already_AddRefed<DOMRequest> StopDiscovery(ErrorResult& aRv);

  already_AddRefed<DOMRequest>
    Pair(BluetoothDevice& aDevice, ErrorResult& aRv);
  already_AddRefed<DOMRequest>
    Unpair(BluetoothDevice& aDevice, ErrorResult& aRv);
  already_AddRefed<DOMRequest>
    GetPairedDevices(ErrorResult& aRv);
  already_AddRefed<DOMRequest>
    SetPinCode(const nsAString& aDeviceAddress, const nsAString& aPinCode,
               ErrorResult& aRv);
  already_AddRefed<DOMRequest>
    SetPasskey(const nsAString& aDeviceAddress, uint32_t aPasskey,
               ErrorResult& aRv);
  already_AddRefed<DOMRequest>
    SetPairingConfirmation(const nsAString& aDeviceAddress, bool aConfirmation,
                           ErrorResult& aRv);
  already_AddRefed<DOMRequest>
    SetAuthorization(const nsAString& aDeviceAddress, bool aAllow,
                     ErrorResult& aRv);

  already_AddRefed<DOMRequest>
    Connect(const nsAString& aDeviceAddress, uint16_t aProfile,
            ErrorResult& aRv);
  already_AddRefed<DOMRequest>
    Disconnect(uint16_t aProfile, ErrorResult& aRv);
  already_AddRefed<DOMRequest>
    GetConnectedDevices(uint16_t aProfile, ErrorResult& aRv);

  already_AddRefed<DOMRequest>
    SendFile(const nsAString& aDeviceAddress, nsIDOMBlob* aBlob,
             ErrorResult& aRv);
  already_AddRefed<DOMRequest>
    StopSendingFile(const nsAString& aDeviceAddress, ErrorResult& aRv);
  already_AddRefed<DOMRequest>
    ConfirmReceivingFile(const nsAString& aDeviceAddress, bool aConfirmation,
                         ErrorResult& aRv);

  already_AddRefed<DOMRequest> ConnectSco(ErrorResult& aRv);
  already_AddRefed<DOMRequest> DisconnectSco(ErrorResult& aRv);
  already_AddRefed<DOMRequest> IsScoConnected(ErrorResult& aRv);

  already_AddRefed<DOMRequest>
    SendMediaMetaData(const MediaMetaData& aMediaMetaData, ErrorResult& aRv);
  already_AddRefed<DOMRequest>
    SendMediaPlayStatus(const MediaPlayStatus& aMediaPlayStatus, ErrorResult& aRv);

  IMPL_EVENT_HANDLER(devicefound);
  IMPL_EVENT_HANDLER(a2dpstatuschanged);
  IMPL_EVENT_HANDLER(hfpstatuschanged);
  IMPL_EVENT_HANDLER(pairedstatuschanged);
  IMPL_EVENT_HANDLER(scostatuschanged);

  nsPIDOMWindow* GetParentObject() const
  {
     return GetOwner();
  }

  virtual JSObject*
    WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

private:
  BluetoothAdapter(nsPIDOMWindow* aOwner, const BluetoothValue& aValue);
  ~BluetoothAdapter();

  void Root();

  already_AddRefed<mozilla::dom::DOMRequest>
    StartStopDiscovery(bool aStart, ErrorResult& aRv);
  already_AddRefed<mozilla::dom::DOMRequest>
    PairUnpair(bool aPair, BluetoothDevice& aDevice, ErrorResult& aRv);

  JS::Heap<JSObject*> mJsUuids;
  JS::Heap<JSObject*> mJsDeviceAddresses;
  nsString mAddress;
  nsString mName;
  bool mDiscoverable;
  bool mDiscovering;
  bool mPairable;
  bool mPowered;
  uint32_t mPairableTimeout;
  uint32_t mDiscoverableTimeout;
  uint32_t mClass;
  nsTArray<nsString> mDeviceAddresses;
  nsTArray<nsString> mUuids;
  bool mIsRooted;
};

END_BLUETOOTH_NAMESPACE

#endif
