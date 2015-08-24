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
BluetoothGattServer::Notify(const BluetoothSignal& aData)
{
  BT_LOGD("[GattServer] %s", NS_ConvertUTF16toUTF8(aData.name()).get());
  NS_ENSURE_TRUE_VOID(mSignalRegistered);

  BluetoothValue v = aData.value();
  if (aData.name().EqualsLiteral("ServerRegistered")) {
    MOZ_ASSERT(v.type() == BluetoothValue::Tuint32_t);
    mServerIf = v.get_uint32_t();
  } else if (aData.name().EqualsLiteral("ServerUnregistered")) {
    mServerIf = 0;
  } else if (aData.name().EqualsLiteral(GATT_CONNECTION_STATE_CHANGED_ID)) {
    MOZ_ASSERT(v.type() == BluetoothValue::TArrayOfBluetoothNamedValue);
    const InfallibleTArray<BluetoothNamedValue>& arr =
      v.get_ArrayOfBluetoothNamedValue();

    MOZ_ASSERT(arr.Length() == 2 &&
               arr[0].value().type() == BluetoothValue::Tbool &&
               arr[1].value().type() == BluetoothValue::TnsString);

    BluetoothStatusChangedEventInit init;
    init.mStatus = arr[0].value().get_bool();
    init.mAddress = arr[1].value().get_nsString();

    nsRefPtr<BluetoothStatusChangedEvent> event =
      BluetoothStatusChangedEvent::Constructor(
        this, NS_LITERAL_STRING(GATT_CONNECTION_STATE_CHANGED_ID), init);

    DispatchTrustedEvent(event);
  } else if (aData.name().EqualsLiteral("ServiceHandleUpdated")) {
    HandleServiceHandleUpdated(v);
  } else if (aData.name().EqualsLiteral("CharacteristicHandleUpdated")) {
    HandleCharacteristicHandleUpdated(v);
  } else if (aData.name().EqualsLiteral("DescriptorHandleUpdated")) {
    HandleDescriptorHandleUpdated(v);
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
  mServices.Clear();
  mPendingService = nullptr;

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

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BT_ENSURE_TRUE_REJECT(mValid, promise, NS_ERROR_NOT_AVAILABLE);
  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  bs->GattServerConnectPeripheralInternal(
    mAppUuid, aAddress, new BluetoothVoidReplyRunnable(nullptr, promise));

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

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BT_ENSURE_TRUE_REJECT(mValid, promise, NS_ERROR_NOT_AVAILABLE);
  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  bs->GattServerDisconnectPeripheralInternal(
    mAppUuid, aAddress, new BluetoothVoidReplyRunnable(nullptr, promise));

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
  nsRefPtr<BluetoothGattServer> mServer;
  nsRefPtr<BluetoothGattService> mService;
  nsRefPtr<BluetoothGattService> mIncludedService;
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
  nsRefPtr<BluetoothGattServer> mServer;
  nsRefPtr<BluetoothGattService> mService;
  nsRefPtr<BluetoothGattCharacteristic> mCharacteristic;
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
  nsRefPtr<BluetoothGattServer> mServer;
  nsRefPtr<BluetoothGattService> mService;
  nsRefPtr<BluetoothGattCharacteristic> mCharacteristic;
  nsRefPtr<BluetoothGattDescriptor> mDescriptor;
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
  nsRefPtr<BluetoothGattServer> mServer;
  nsRefPtr<BluetoothGattService> mService;
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

  nsRefPtr<BluetoothGattServer> mServer;
  nsRefPtr<BluetoothGattService> mService;
  nsRefPtr<Promise> mPromise;
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
    nsTArray<nsRefPtr<BluetoothGattService>> includedServices;
    mService->GetIncludedServices(includedServices);
    for (size_t i = 0; i < includedServices.Length(); ++i) {
      nsRefPtr<SubTask> includedServiceTask =
        new AddIncludedServiceTask(this, mServer, mService, includedServices[i]);
      AppendTask(includedServiceTask.forget());
    }

    /* add characteristics */
    nsTArray<nsRefPtr<BluetoothGattCharacteristic>> characteristics;
    mService->GetCharacteristics(characteristics);
    for (size_t i = 0; i < characteristics.Length(); ++i) {
      nsRefPtr<SubTask> characteristicTask =
        new AddCharacteristicTask(this, mServer, mService, characteristics[i]);
      AppendTask(characteristicTask.forget());

      /* add descriptors */
      nsTArray<nsRefPtr<BluetoothGattDescriptor>> descriptors;
      characteristics[i]->GetDescriptors(descriptors);
      for (size_t j = 0; j < descriptors.Length(); ++j) {
        nsRefPtr<SubTask> descriptorTask =
          new AddDescriptorTask(this,
                                mServer,
                                mService,
                                characteristics[i],
                                descriptors[j]);

        AppendTask(descriptorTask.forget());
      }
    }

    /* start service */
    nsRefPtr<SubTask> startTask = new StartServiceTask(this, mServer, mService);
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

  nsRefPtr<BluetoothGattServer> mServer;
  nsRefPtr<BluetoothGattService> mService;
  nsRefPtr<Promise> mPromise;
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

    nsRefPtr<nsRunnable> runnable = new AddServiceTaskQueue(mServer,
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

  nsRefPtr<BluetoothGattServer> mServer;
  nsRefPtr<BluetoothGattService> mService;
  nsRefPtr<Promise> mPromise;
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

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
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
  nsRefPtr<BluetoothGattServer> mServer;
  nsRefPtr<BluetoothGattService> mService;
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

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
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
