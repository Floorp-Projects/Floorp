/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothGattServer_h
#define mozilla_dom_bluetooth_BluetoothGattServer_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/BluetoothGattServerBinding.h"
#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "mozilla/dom/bluetooth/BluetoothGattService.h"
#include "mozilla/dom/Promise.h"
#include "nsClassHashtable.h"
#include "nsCOMPtr.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {
class Promise;
struct BluetoothAdvertisingData;
}
}

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSignal;

class BluetoothGattServer final : public DOMEventTargetHelper
                                , public BluetoothSignalObserver
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(BluetoothGattServer,
                                           DOMEventTargetHelper)

  /****************************************************************************
   * Attribute Getters
   ***************************************************************************/
  void GetServices(
    nsTArray<RefPtr<BluetoothGattService>>& aServices) const
  {
    aServices = mServices;
  }

  /****************************************************************************
   * Event Handlers
   ***************************************************************************/
  IMPL_EVENT_HANDLER(connectionstatechanged);
  IMPL_EVENT_HANDLER(attributereadreq);
  IMPL_EVENT_HANDLER(attributewritereq);

  /****************************************************************************
   * Methods (Web API Implementation)
   ***************************************************************************/
  already_AddRefed<Promise> Connect(
    const nsAString& aAddress, ErrorResult& aRv);
  already_AddRefed<Promise> Disconnect(
    const nsAString& aAddress, ErrorResult& aRv);

  already_AddRefed<Promise> StartAdvertising(
    const BluetoothAdvertisingData& aAdvData, ErrorResult& aRv);
  already_AddRefed<Promise> StopAdvertising(ErrorResult& aRv);

  already_AddRefed<Promise> AddService(BluetoothGattService& aService,
                                       ErrorResult& aRv);
  already_AddRefed<Promise> RemoveService(BluetoothGattService& aService,
                                          ErrorResult& aRv);

  already_AddRefed<Promise> NotifyCharacteristicChanged(
    const nsAString& aAddress,
    BluetoothGattCharacteristic& aCharacteristic,
    bool aConfirm,
    ErrorResult& aRv);

  already_AddRefed<Promise> SendResponse(const nsAString& aAddress,
                                         uint16_t aStatus,
                                         int32_t aRequestId,
                                         ErrorResult& aRv);

  /****************************************************************************
   * Others
   ***************************************************************************/
  void Notify(const BluetoothSignal& aData); // BluetoothSignalObserver

  nsPIDOMWindowInner* GetParentObject() const
  {
     return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  virtual void DisconnectFromOwner() override;
  BluetoothGattServer(nsPIDOMWindowInner* aOwner);

  /* Invalidate the GATT server.
   * If the BluetoothAdapter turns off, existing BluetoothGattServer instances
   * should stop working till the end of life.
   */
  void Invalidate();

private:
  ~BluetoothGattServer();

  class StartAdvertisingTask;
  class RegisterServerAndStartAdvertisingTask;
  class StopAdvertisingTask;
  class AddIncludedServiceTask;
  class AddCharacteristicTask;
  class AddDescriptorTask;
  class StartServiceTask;
  class CancelAddServiceTask;
  class AddServiceTaskQueue;
  class AddServiceTask;
  class RemoveServiceTask;

  friend class StartAdvertisingTask;
  friend class RegisterServerAndStartAdvertisingTask;
  friend class StopAdvertisingTask;
  friend class AddIncludedServiceTask;
  friend class AddCharacteristicTask;
  friend class AddDescriptorTask;
  friend class StartServiceTask;
  friend class CancelAddServiceTask;
  friend class AddServiceTaskQueue;
  friend class AddServiceTask;
  friend class RemoveServiceTask;

  struct RequestData
  {
    RequestData(const BluetoothAttributeHandle& aHandle,
                BluetoothGattCharacteristic* aCharacteristic,
                BluetoothGattDescriptor* aDescriptor)
    : mHandle(aHandle)
    , mCharacteristic(aCharacteristic)
    , mDescriptor(aDescriptor)
    { }

    BluetoothAttributeHandle mHandle;
    RefPtr<BluetoothGattCharacteristic> mCharacteristic;
    RefPtr<BluetoothGattDescriptor> mDescriptor;
  };

  void HandleServerRegistered(const BluetoothValue& aValue);
  void HandleServerUnregistered(const BluetoothValue& aValue);
  void HandleConnectionStateChanged(const BluetoothValue& aValue);
  void HandleServiceHandleUpdated(const BluetoothValue& aValue);
  void HandleCharacteristicHandleUpdated(const BluetoothValue& aValue);
  void HandleDescriptorHandleUpdated(const BluetoothValue& aValue);
  void HandleReadWriteRequest(const BluetoothValue& aValue,
                              const nsAString& aString);

  /****************************************************************************
   * Variables
   ***************************************************************************/
  nsCOMPtr<nsPIDOMWindowInner> mOwner;

  /**
   * Random generated UUID of this GATT client.
   */
  nsString mAppUuid;

  /**
   * Id of the GATT server interface given by bluetooth stack.
   * 0 if the interface is not registered yet, nonzero otherwise.
   */
  int mServerIf;

  bool mValid;

  /**
   * Array of services for this server.
   */
  nsTArray<RefPtr<BluetoothGattService>> mServices;

  /**
   * The service that is being added to this server.
   */
  RefPtr<BluetoothGattService> mPendingService;

  /**
   * Map request information from the request ID.
   */
  nsClassHashtable<nsUint32HashKey, RequestData> mRequestMap;

  /**
   * AppUuid of the GATT client interface which is used to advertise.
   */
  BluetoothUuid mAdvertisingAppUuid;
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_BluetoothGattServer_h
