/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothGattServer.h"

#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothUtils.h"
#include "mozilla/dom/BluetoothStatusChangedEvent.h"
#include "mozilla/dom/bluetooth/BluetoothGattAttributeEvent.h"
#include "mozilla/dom/bluetooth/BluetoothGattCharacteristic.h"
#include "mozilla/dom/bluetooth/BluetoothGattService.h"
#include "mozilla/dom/Promise.h"

using namespace mozilla;
using namespace mozilla::dom;

USING_BLUETOOTH_NAMESPACE

NS_IMPL_CYCLE_COLLECTION_CLASS(BluetoothGattServer)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(BluetoothGattServer,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwner)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mServices)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPendingService)

  /**
   * Unregister the bluetooth signal handler after unlinked.
   *
   * This is needed to avoid ending up with exposing a deleted object to JS or
   * accessing deleted objects while receiving signals from parent process
   * after unlinked. Please see Bug 1138267 for detail informations.
   */
  UnregisterBluetoothSignalHandler(tmp->mAppUuid, tmp);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(BluetoothGattServer,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mServices)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingService)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BluetoothGattServer)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(BluetoothGattServer, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(BluetoothGattServer, DOMEventTargetHelper)

BluetoothGattServer::BluetoothGattServer(nsPIDOMWindow* aOwner)
  : mOwner(aOwner)
  , mServerIf(0)
  , mValid(true)
{
  if (NS_SUCCEEDED(GenerateUuid(mAppUuid)) && !mAppUuid.IsEmpty()) {
    RegisterBluetoothSignalHandler(mAppUuid, this);
  }
  if (!mSignalRegistered) {
    Invalidate();
  }
}

BluetoothGattServer::~BluetoothGattServer()
{
  Invalidate();
}

void BluetoothGattServer::HandleServerRegistered(const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::Tuint32_t);
  mServerIf = aValue.get_uint32_t();
}

void BluetoothGattServer::HandleServerUnregistered(const BluetoothValue& aValue)
{
  mServerIf = 0;
}

void BluetoothGattServer::HandleConnectionStateChanged(
  const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);
  const InfallibleTArray<BluetoothNamedValue>& arr =
    aValue.get_ArrayOfBluetoothNamedValue();

  MOZ_ASSERT(arr.Length() == 2 &&
             arr[0].value().type() == BluetoothValue::Tbool &&
             arr[1].value().type() == BluetoothValue::TnsString);

  BluetoothStatusChangedEventInit init;
  init.mStatus = arr[0].value().get_bool();
  init.mAddress = arr[1].value().get_nsString();

  RefPtr<BluetoothStatusChangedEvent> event =
    BluetoothStatusChangedEvent::Constructor(
      this, NS_LITERAL_STRING(GATT_CONNECTION_STATE_CHANGED_ID), init);

  DispatchTrustedEvent(event);
}

void
BluetoothGattServer::HandleServiceHandleUpdated(const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);
  const InfallibleTArray<BluetoothNamedValue>& arr =
    aValue.get_ArrayOfBluetoothNamedValue();

  MOZ_ASSERT(arr.Length() == 2 &&
    arr[0].value().type() == BluetoothValue::TBluetoothGattServiceId &&
    arr[1].value().type() == BluetoothValue::TBluetoothAttributeHandle);

  BluetoothGattServiceId serviceId =
    arr[0].value().get_BluetoothGattServiceId();
  BluetoothAttributeHandle serviceHandle =
    arr[1].value().get_BluetoothAttributeHandle();

  NS_ENSURE_TRUE_VOID(mPendingService);
  NS_ENSURE_TRUE_VOID(mPendingService->GetServiceId() == serviceId);
  mPendingService->AssignServiceHandle(serviceHandle);
}

void
BluetoothGattServer::HandleCharacteristicHandleUpdated(
  const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);
  const InfallibleTArray<BluetoothNamedValue>& arr =
    aValue.get_ArrayOfBluetoothNamedValue();

  MOZ_ASSERT(arr.Length() == 3 &&
    arr[0].value().type() == BluetoothValue::TBluetoothUuid &&
    arr[1].value().type() == BluetoothValue::TBluetoothAttributeHandle &&
    arr[2].value().type() == BluetoothValue::TBluetoothAttributeHandle);

  BluetoothUuid characteristicUuid =
    arr[0].value().get_BluetoothUuid();
  BluetoothAttributeHandle serviceHandle =
    arr[1].value().get_BluetoothAttributeHandle();
  BluetoothAttributeHandle characteristicHandle =
    arr[2].value().get_BluetoothAttributeHandle();

  NS_ENSURE_TRUE_VOID(mPendingService);
  NS_ENSURE_TRUE_VOID(mPendingService->GetServiceHandle() == serviceHandle);
  mPendingService->AssignCharacteristicHandle(characteristicUuid,
                                              characteristicHandle);
}

void
BluetoothGattServer::HandleDescriptorHandleUpdated(
  const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);
  const InfallibleTArray<BluetoothNamedValue>& arr =
    aValue.get_ArrayOfBluetoothNamedValue();

  MOZ_ASSERT(arr.Length() == 4 &&
    arr[0].value().type() == BluetoothValue::TBluetoothUuid &&
    arr[1].value().type() == BluetoothValue::TBluetoothAttributeHandle &&
    arr[2].value().type() == BluetoothValue::TBluetoothAttributeHandle &&
    arr[3].value().type() == BluetoothValue::TBluetoothAttributeHandle);

  BluetoothUuid descriptorUuid =
    arr[0].value().get_BluetoothUuid();
  BluetoothAttributeHandle serviceHandle =
    arr[1].value().get_BluetoothAttributeHandle();
  BluetoothAttributeHandle characteristicHandle =
    arr[2].value().get_BluetoothAttributeHandle();
  BluetoothAttributeHandle descriptorHandle =
    arr[3].value().get_BluetoothAttributeHandle();

  NS_ENSURE_TRUE_VOID(mPendingService);
  NS_ENSURE_TRUE_VOID(mPendingService->GetServiceHandle() == serviceHandle);
  mPendingService->AssignDescriptorHandle(descriptorUuid,
                                          characteristicHandle,
                                          descriptorHandle);
}

void
BluetoothGattServer::HandleReadWriteRequest(const BluetoothValue& aValue,
                                            const nsAString& aEventName)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);
  const InfallibleTArray<BluetoothNamedValue>& arr =
    aValue.get_ArrayOfBluetoothNamedValue();

  MOZ_ASSERT(arr.Length() == 5 &&
    arr[0].value().type() == BluetoothValue::Tint32_t &&
    arr[1].value().type() == BluetoothValue::TBluetoothAttributeHandle &&
    arr[2].value().type() == BluetoothValue::TnsString &&
    arr[3].value().type() == BluetoothValue::Tbool &&
    arr[4].value().type() == BluetoothValue::TArrayOfuint8_t);

  int32_t requestId = arr[0].value().get_int32_t();
  BluetoothAttributeHandle handle =
    arr[1].value().get_BluetoothAttributeHandle();
  nsString address = arr[2].value().get_nsString();
  bool needResponse = arr[3].value().get_bool();
  nsTArray<uint8_t> value;
  value = arr[4].value().get_ArrayOfuint8_t();

  // Find the target characteristic or descriptor from the given handle
  RefPtr<BluetoothGattCharacteristic> characteristic = nullptr;
  RefPtr<BluetoothGattDescriptor> descriptor = nullptr;
  for (uint32_t i = 0; i < mServices.Length(); i++) {
    for (uint32_t j = 0; j < mServices[i]->mCharacteristics.Length(); j++) {
      RefPtr<BluetoothGattCharacteristic> currentChar =
        mServices[i]->mCharacteristics[j];

      if (handle == currentChar->GetCharacteristicHandle()) {
        characteristic = currentChar;
        break;
      }

      size_t index = currentChar->mDescriptors.IndexOf(handle);
      if (index != currentChar->mDescriptors.NoIndex) {
        descriptor = currentChar->mDescriptors[index];
        break;
      }
    }
  }

  if (!(characteristic || descriptor)) {
    BT_WARNING("Wrong handle: no matched characteristic or descriptor");
    return;
  }

  // Save the request information for sending the response later
  RequestData data(handle,
                   characteristic,
                   descriptor);
  mRequestMap.Put(requestId, &data);

  RefPtr<BluetoothGattAttributeEvent> event =
    BluetoothGattAttributeEvent::Constructor(
      this, aEventName, address, requestId, characteristic, descriptor,
      &value, needResponse, false /* Bubble */, false /* Cancelable*/);

  DispatchTrustedEvent(event);
}

void
BluetoothGattServer::Notify(const BluetoothSignal& aData)
{
  BT_LOGD("[GattServer] %s", NS_ConvertUTF16toUTF8(aData.name()).get());
  NS_ENSURE_TRUE_VOID(mSignalRegistered);

  BluetoothValue v = aData.value();
  if (aData.name().EqualsLiteral("ServerRegistered")) {
    HandleServerRegistered(v);
  } else if (aData.name().EqualsLiteral("ServerUnregistered")) {
    HandleServerUnregistered(v);
  } else if (aData.name().EqualsLiteral(GATT_CONNECTION_STATE_CHANGED_ID)) {
    HandleConnectionStateChanged(v);
  } else if (aData.name().EqualsLiteral("ServiceHandleUpdated")) {
    HandleServiceHandleUpdated(v);
  } else if (aData.name().EqualsLiteral("CharacteristicHandleUpdated")) {
    HandleCharacteristicHandleUpdated(v);
  } else if (aData.name().EqualsLiteral("DescriptorHandleUpdated")) {
    HandleDescriptorHandleUpdated(v);
  } else if (aData.name().EqualsLiteral("ReadRequested")) {
    HandleReadWriteRequest(v, NS_LITERAL_STRING(ATTRIBUTE_READ_REQUEST));
  } else if (aData.name().EqualsLiteral("WriteRequested")) {
    HandleReadWriteRequest(v, NS_LITERAL_STRING(ATTRIBUTE_WRITE_REQUEST));
  } else {
    BT_WARNING("Not handling GATT signal: %s",
               NS_ConvertUTF16toUTF8(aData.name()).get());
  }
}

JSObject*
BluetoothGattServer::WrapObject(JSContext* aContext,
                                JS::Handle<JSObject*> aGivenProto)
{
  return BluetoothGattServerBinding::Wrap(aContext, this, aGivenProto);
}

void
BluetoothGattServer::DisconnectFromOwner()
{
  DOMEventTargetHelper::DisconnectFromOwner();
  Invalidate();
}

void
BluetoothGattServer::Invalidate()
{
  mValid = false;
  mPendingService = nullptr;
  mServices.Clear();
  mRequestMap.Clear();

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  if (mServerIf > 0) {
    bs->UnregisterGattServerInternal(mServerIf,
                                     new BluetoothVoidReplyRunnable(nullptr,
                                                                    nullptr));
  }

  if (!mAppUuid.IsEmpty() && mSignalRegistered) {
    UnregisterBluetoothSignalHandler(mAppUuid, this);
  }
}

already_AddRefed<Promise>
BluetoothGattServer::Connect(const nsAString& aAddress, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BluetoothAddress address;
  BT_ENSURE_TRUE_REJECT(
    NS_SUCCEEDED(StringToAddress(aAddress, address)),
    promise,
    NS_ERROR_INVALID_ARG);

  BT_ENSURE_TRUE_REJECT(mValid, promise, NS_ERROR_NOT_AVAILABLE);
  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  bs->GattServerConnectPeripheralInternal(
    mAppUuid, address, new BluetoothVoidReplyRunnable(nullptr, promise));

  return promise.forget();
}

already_AddRefed<Promise>
BluetoothGattServer::Disconnect(const nsAString& aAddress, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BluetoothAddress address;
  BT_ENSURE_TRUE_REJECT(
    NS_SUCCEEDED(StringToAddress(aAddress, address)),
    promise,
    NS_ERROR_INVALID_ARG);

  BT_ENSURE_TRUE_REJECT(mValid, promise, NS_ERROR_NOT_AVAILABLE);
  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  bs->GattServerDisconnectPeripheralInternal(
    mAppUuid, address, new BluetoothVoidReplyRunnable(nullptr, promise));

  return promise.forget();
}

class BluetoothGattServer::AddIncludedServiceTask final
  : public BluetoothReplyTaskQueue::SubTask
{
public:
  AddIncludedServiceTask(BluetoothReplyTaskQueue* aRootQueue,
                         BluetoothGattServer* aServer,
                         BluetoothGattService* aService,
                         BluetoothGattService* aIncludedService)
    : BluetoothReplyTaskQueue::SubTask(aRootQueue, nullptr)
    , mServer(aServer)
    , mService(aService)
    , mIncludedService(aIncludedService)
  { }

  bool Execute() override
  {
    BluetoothService* bs = BluetoothService::Get();
    if (NS_WARN_IF(!bs)) {
      return false;
    }

    bs->GattServerAddIncludedServiceInternal(
      mServer->mAppUuid,
      mService->GetServiceHandle(),
      mIncludedService->GetServiceHandle(),
      GetReply());

    return true;
  }

private:
  RefPtr<BluetoothGattServer> mServer;
  RefPtr<BluetoothGattService> mService;
  RefPtr<BluetoothGattService> mIncludedService;
};

class BluetoothGattServer::AddCharacteristicTask final
  : public BluetoothReplyTaskQueue::SubTask
{
public:
  AddCharacteristicTask(BluetoothReplyTaskQueue* aRootQueue,
                        BluetoothGattServer* aServer,
                        BluetoothGattService* aService,
                        BluetoothGattCharacteristic* aCharacteristic)
    : BluetoothReplyTaskQueue::SubTask(aRootQueue, nullptr)
    , mServer(aServer)
    , mService(aService)
    , mCharacteristic(aCharacteristic)
  { }

  bool Execute() override
  {
    BluetoothService* bs = BluetoothService::Get();
    if (NS_WARN_IF(!bs)) {
      return false;
    }

    BluetoothUuid uuid;
    mCharacteristic->GetUuid(uuid);
    bs->GattServerAddCharacteristicInternal(
      mServer->mAppUuid,
      mService->GetServiceHandle(),
      uuid,
      mCharacteristic->GetPermissions(),
      mCharacteristic->GetProperties(),
      GetReply());

    return true;
  }

private:
  RefPtr<BluetoothGattServer> mServer;
  RefPtr<BluetoothGattService> mService;
  RefPtr<BluetoothGattCharacteristic> mCharacteristic;
};

class BluetoothGattServer::AddDescriptorTask final
  : public BluetoothReplyTaskQueue::SubTask
{
public:
  AddDescriptorTask(BluetoothReplyTaskQueue* aRootQueue,
                    BluetoothGattServer* aServer,
                    BluetoothGattService* aService,
                    BluetoothGattCharacteristic* aCharacteristic,
                    BluetoothGattDescriptor* aDescriptor)
    : BluetoothReplyTaskQueue::SubTask(aRootQueue, nullptr)
    , mServer(aServer)
    , mService(aService)
    , mCharacteristic(aCharacteristic)
    , mDescriptor(aDescriptor)
  { }

  bool Execute() override
  {
    BluetoothService* bs = BluetoothService::Get();
    if (NS_WARN_IF(!bs)) {
      return false;
    }

    BluetoothUuid uuid;
    mDescriptor->GetUuid(uuid);
    bs->GattServerAddDescriptorInternal(
      mServer->mAppUuid,
      mService->GetServiceHandle(),
      mCharacteristic->GetCharacteristicHandle(),
      uuid,
      mDescriptor->GetPermissions(),
      GetReply());

    return true;
  }

private:
  RefPtr<BluetoothGattServer> mServer;
  RefPtr<BluetoothGattService> mService;
  RefPtr<BluetoothGattCharacteristic> mCharacteristic;
  RefPtr<BluetoothGattDescriptor> mDescriptor;
};

class BluetoothGattServer::StartServiceTask final
  : public BluetoothReplyTaskQueue::SubTask
{
public:
  StartServiceTask(BluetoothReplyTaskQueue* aRootQueue,
                   BluetoothGattServer* aServer,
                   BluetoothGattService* aService)
    : BluetoothReplyTaskQueue::SubTask(aRootQueue, nullptr)
    , mServer(aServer)
    , mService(aService)
  { }

  bool Execute() override
  {
    BluetoothService* bs = BluetoothService::Get();
    if (NS_WARN_IF(!bs)) {
      return false;
    }

    bs->GattServerStartServiceInternal(
      mServer->mAppUuid,
      mService->GetServiceHandle(),
      GetReply());

    return true;
  }

private:
  RefPtr<BluetoothGattServer> mServer;
  RefPtr<BluetoothGattService> mService;
};

/*
 * CancelAddServiceTask is used when failing to completely add a service. No
 * matter CancelAddServiceTask executes successfully or not, the promose should
 * be rejected because we fail to adding the service eventually.
 */
class BluetoothGattServer::CancelAddServiceTask final
  : public BluetoothVoidReplyRunnable
{
public:
  CancelAddServiceTask(BluetoothGattServer* aServer,
                       BluetoothGattService* aService,
                       Promise* aPromise)
    : BluetoothVoidReplyRunnable(nullptr, nullptr)
      /* aPromise is not managed by BluetoothVoidReplyRunnable. It would be
       * rejected after this task has been executed anyway. */
    , mServer(aServer)
    , mService(aService)
    , mPromise(aPromise)
  {
    MOZ_ASSERT(mPromise);
  }

  void ReleaseMembers() override
  {
    BluetoothVoidReplyRunnable::ReleaseMembers();
    mServer = nullptr;
    mService = nullptr;
    mPromise = nullptr;
  }

private:
  void OnSuccessFired() override
  {
    mServer->mPendingService = nullptr;
    mPromise->MaybeReject(NS_ERROR_FAILURE);
  }

  void OnErrorFired() override
  {
    mServer->mPendingService = nullptr;
    mPromise->MaybeReject(NS_ERROR_FAILURE);
  }

  RefPtr<BluetoothGattServer> mServer;
  RefPtr<BluetoothGattService> mService;
  RefPtr<Promise> mPromise;
};

class BluetoothGattServer::AddServiceTaskQueue final
  : public BluetoothReplyTaskQueue
{
public:
  AddServiceTaskQueue(BluetoothGattServer* aServer,
                      BluetoothGattService* aService,
                      Promise* aPromise)
    : BluetoothReplyTaskQueue(nullptr)
    , mServer(aServer)
    , mService(aService)
    , mPromise(aPromise)
  {
    /* add included services */
    nsTArray<RefPtr<BluetoothGattService>> includedServices;
    mService->GetIncludedServices(includedServices);
    for (size_t i = 0; i < includedServices.Length(); ++i) {
      RefPtr<SubTask> includedServiceTask =
        new AddIncludedServiceTask(this, mServer, mService, includedServices[i]);
      AppendTask(includedServiceTask.forget());
    }

    /* add characteristics */
    nsTArray<RefPtr<BluetoothGattCharacteristic>> characteristics;
    mService->GetCharacteristics(characteristics);
    for (size_t i = 0; i < characteristics.Length(); ++i) {
      RefPtr<SubTask> characteristicTask =
        new AddCharacteristicTask(this, mServer, mService, characteristics[i]);
      AppendTask(characteristicTask.forget());

      /* add descriptors */
      nsTArray<RefPtr<BluetoothGattDescriptor>> descriptors;
      characteristics[i]->GetDescriptors(descriptors);
      for (size_t j = 0; j < descriptors.Length(); ++j) {
        RefPtr<SubTask> descriptorTask =
          new AddDescriptorTask(this,
                                mServer,
                                mService,
                                characteristics[i],
                                descriptors[j]);

        AppendTask(descriptorTask.forget());
      }
    }

    /* start service */
    RefPtr<SubTask> startTask = new StartServiceTask(this, mServer, mService);
    AppendTask(startTask.forget());
  }

protected:
  virtual ~AddServiceTaskQueue()
  { }

private:
  void OnSuccessFired() override
  {
    mServer->mPendingService = nullptr;
    mServer->mServices.AppendElement(mService);
    mPromise->MaybeResolve(JS::UndefinedHandleValue);
  }

  void OnErrorFired() override
  {
    BluetoothService* bs = BluetoothService::Get();
    BT_ENSURE_TRUE_REJECT_VOID(bs, mPromise, NS_ERROR_NOT_AVAILABLE);

    bs->GattServerRemoveServiceInternal(
      mServer->mAppUuid,
      mService->GetServiceHandle(),
      new CancelAddServiceTask(mServer, mService, mPromise));
  }

  RefPtr<BluetoothGattServer> mServer;
  RefPtr<BluetoothGattService> mService;
  RefPtr<Promise> mPromise;
};

class BluetoothGattServer::AddServiceTask final
  : public BluetoothVoidReplyRunnable
{
public:
  AddServiceTask(BluetoothGattServer* aServer,
                 BluetoothGattService* aService,
                 Promise* aPromise)
    : BluetoothVoidReplyRunnable(nullptr, nullptr)
      /* aPromise is not managed by BluetoothVoidReplyRunnable. It would be
       * passed to other tasks after this task executes successfully. */
    , mServer(aServer)
    , mService(aService)
    , mPromise(aPromise)
  {
    MOZ_ASSERT(mServer);
    MOZ_ASSERT(mService);
    MOZ_ASSERT(mPromise);
  }

  void ReleaseMembers() override
  {
    BluetoothReplyRunnable::ReleaseMembers();
    mServer = nullptr;
    mService = nullptr;
    mPromise = nullptr;
  }

private:
  virtual void OnSuccessFired() override
  {
    mService->AssignAppUuid(mServer->mAppUuid);

    RefPtr<nsRunnable> runnable = new AddServiceTaskQueue(mServer,
                                                            mService,
                                                            mPromise);
    nsresult rv = NS_DispatchToMainThread(runnable.forget());

    if (NS_WARN_IF(NS_FAILED(rv))) {
      mServer->mPendingService = nullptr;
      mPromise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
    }
  }

  virtual void OnErrorFired() override
  {
    mServer->mPendingService = nullptr;
    mPromise->MaybeReject(NS_ERROR_NOT_AVAILABLE);
  }

  RefPtr<BluetoothGattServer> mServer;
  RefPtr<BluetoothGattService> mService;
  RefPtr<Promise> mPromise;
};

already_AddRefed<Promise>
BluetoothGattServer::AddService(BluetoothGattService& aService,
                                ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BT_ENSURE_TRUE_REJECT(mValid, promise, NS_ERROR_NOT_AVAILABLE);
  BT_ENSURE_TRUE_REJECT(!aService.IsActivated(),
                        promise,
                        NS_ERROR_INVALID_ARG);

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  mPendingService = &aService;

  bs->GattServerAddServiceInternal(mAppUuid,
                                   mPendingService->GetServiceId(),
                                   mPendingService->GetHandleCount(),
                                   new AddServiceTask(this,
                                                      mPendingService,
                                                      promise));

  return promise.forget();
}

class BluetoothGattServer::RemoveServiceTask final
  : public BluetoothReplyRunnable
{
public:
  RemoveServiceTask(BluetoothGattServer* aServer,
                    BluetoothGattService* aService,
                    Promise* aPromise)
    : BluetoothReplyRunnable(nullptr, aPromise)
    , mServer(aServer)
    , mService(aService)
  {
    MOZ_ASSERT(mServer);
    MOZ_ASSERT(mService);
  }

  void ReleaseMembers() override
  {
    BluetoothReplyRunnable::ReleaseMembers();
    mServer = nullptr;
    mService = nullptr;
  }

protected:
  bool ParseSuccessfulReply(JS::MutableHandle<JS::Value> aValue) override
  {
    aValue.setUndefined();

    mServer->mServices.RemoveElement(mService);

    return true;
  }

private:
  RefPtr<BluetoothGattServer> mServer;
  RefPtr<BluetoothGattService> mService;
};

already_AddRefed<Promise>
BluetoothGattServer::RemoveService(BluetoothGattService& aService,
                                   ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BT_ENSURE_TRUE_REJECT(mValid, promise, NS_ERROR_NOT_AVAILABLE);
  BT_ENSURE_TRUE_REJECT(mServices.Contains(&aService),
                        promise,
                        NS_ERROR_INVALID_ARG);

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  bs->GattServerRemoveServiceInternal(
    mAppUuid, aService.GetServiceHandle(), new RemoveServiceTask(this,
                                                                 &aService,
                                                                 promise));

  return promise.forget();
}

already_AddRefed<Promise>
BluetoothGattServer::NotifyCharacteristicChanged(
  const nsAString& aAddress,
  BluetoothGattCharacteristic& aCharacteristic,
  bool aConfirm,
  ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BT_ENSURE_TRUE_REJECT(mValid, promise, NS_ERROR_NOT_AVAILABLE);

  BluetoothAddress address;
  BT_ENSURE_TRUE_REJECT(
    NS_SUCCEEDED(StringToAddress(aAddress, address)),
    promise,
    NS_ERROR_INVALID_ARG);

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  RefPtr<BluetoothGattService> service = aCharacteristic.Service();
  BT_ENSURE_TRUE_REJECT(service, promise, NS_ERROR_NOT_AVAILABLE);
  BT_ENSURE_TRUE_REJECT(mServices.Contains(service),
                        promise,
                        NS_ERROR_NOT_AVAILABLE);

  bs->GattServerSendIndicationInternal(
    mAppUuid, address, aCharacteristic.GetCharacteristicHandle(), aConfirm,
    aCharacteristic.GetValue(),
    new BluetoothVoidReplyRunnable(nullptr, promise));

  return promise.forget();
}

already_AddRefed<Promise>
BluetoothGattServer::SendResponse(const nsAString& aAddress,
                                  uint16_t aStatus,
                                  int32_t aRequestId,
                                  ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BluetoothAddress address;
  BT_ENSURE_TRUE_REJECT(
    NS_SUCCEEDED(StringToAddress(aAddress, address)),
    promise,
    NS_ERROR_INVALID_ARG);

  BT_ENSURE_TRUE_REJECT(mValid, promise, NS_ERROR_NOT_AVAILABLE);

  RequestData* requestData;
  mRequestMap.Get(aRequestId, &requestData);
  BT_ENSURE_TRUE_REJECT(requestData, promise, NS_ERROR_UNEXPECTED);

  BluetoothGattResponse response;
  memset(&response, 0, sizeof(response));
  response.mHandle = requestData->mHandle;

  if (requestData->mCharacteristic) {
    const nsTArray<uint8_t>& value = requestData->mCharacteristic->GetValue();
    response.mLength = value.Length();
    memcpy(&response.mValue, value.Elements(), response.mLength);
  } else if (requestData->mDescriptor) {
    const nsTArray<uint8_t>& value = requestData->mDescriptor->GetValue();
    response.mLength = value.Length();
    memcpy(&response.mValue, value.Elements(), response.mLength);
  } else {
    MOZ_ASSERT_UNREACHABLE(
      "There should be at least one characteristic or descriptor in the "
      "request data.");

    promise->MaybeReject(NS_ERROR_INVALID_ARG);
    return promise.forget();
  }

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  bs->GattServerSendResponseInternal(
    mAppUuid,
    address,
    aStatus,
    aRequestId,
    response,
    new BluetoothVoidReplyRunnable(nullptr, promise));

  return promise.forget();
}
