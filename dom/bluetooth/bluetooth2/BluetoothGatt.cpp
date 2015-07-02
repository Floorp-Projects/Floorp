/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothUtils.h"
#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "mozilla/dom/bluetooth/BluetoothGatt.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/dom/BluetoothGattBinding.h"
#include "mozilla/dom/BluetoothGattCharacteristicEvent.h"
#include "mozilla/dom/Promise.h"
#include "nsServiceManagerUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

USING_BLUETOOTH_NAMESPACE

NS_IMPL_CYCLE_COLLECTION_CLASS(BluetoothGatt)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(BluetoothGatt,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mServices)

  /**
   * Unregister the bluetooth signal handler after unlinked.
   *
   * This is needed to avoid ending up with exposing a deleted object to JS or
   * accessing deleted objects while receiving signals from parent process
   * after unlinked. Please see Bug 1138267 for detail informations.
   */
  UnregisterBluetoothSignalHandler(tmp->mAppUuid, tmp);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(BluetoothGatt,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mServices)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BluetoothGatt)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(BluetoothGatt, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(BluetoothGatt, DOMEventTargetHelper)

BluetoothGatt::BluetoothGatt(nsPIDOMWindow* aWindow,
                             const nsAString& aDeviceAddr)
  : DOMEventTargetHelper(aWindow)
  , mAppUuid(EmptyString())
  , mClientIf(0)
  , mConnectionState(BluetoothConnectionState::Disconnected)
  , mDeviceAddr(aDeviceAddr)
  , mDiscoveringServices(false)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(!mDeviceAddr.IsEmpty());
}

BluetoothGatt::~BluetoothGatt()
{
  BluetoothService* bs = BluetoothService::Get();
  // bs can be null on shutdown, where destruction might happen.
  NS_ENSURE_TRUE_VOID(bs);

  if (mClientIf > 0) {
    nsRefPtr<BluetoothVoidReplyRunnable> result =
      new BluetoothVoidReplyRunnable(nullptr);
    bs->UnregisterGattClientInternal(mClientIf, result);
  }

  UnregisterBluetoothSignalHandler(mAppUuid, this);
}

void
BluetoothGatt::DisconnectFromOwner()
{
  DOMEventTargetHelper::DisconnectFromOwner();

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  if (mClientIf > 0) {
    nsRefPtr<BluetoothVoidReplyRunnable> result =
      new BluetoothVoidReplyRunnable(nullptr);
    bs->UnregisterGattClientInternal(mClientIf, result);
  }

  UnregisterBluetoothSignalHandler(mAppUuid, this);
}

already_AddRefed<Promise>
BluetoothGatt::Connect(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BT_ENSURE_TRUE_REJECT(
    mConnectionState == BluetoothConnectionState::Disconnected,
    promise,
    NS_ERROR_DOM_INVALID_STATE_ERR);

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  if (mAppUuid.IsEmpty()) {
    GenerateUuid(mAppUuid);
    BT_ENSURE_TRUE_REJECT(!mAppUuid.IsEmpty(),
                          promise,
                          NS_ERROR_DOM_OPERATION_ERR);
    RegisterBluetoothSignalHandler(mAppUuid, this);
  }

  UpdateConnectionState(BluetoothConnectionState::Connecting);
  bs->ConnectGattClientInternal(
    mAppUuid, mDeviceAddr, new BluetoothVoidReplyRunnable(nullptr, promise));

  return promise.forget();
}

already_AddRefed<Promise>
BluetoothGatt::Disconnect(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BT_ENSURE_TRUE_REJECT(
    mConnectionState == BluetoothConnectionState::Connected,
    promise,
    NS_ERROR_DOM_INVALID_STATE_ERR);

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  UpdateConnectionState(BluetoothConnectionState::Disconnecting);
  bs->DisconnectGattClientInternal(
    mAppUuid, mDeviceAddr, new BluetoothVoidReplyRunnable(nullptr, promise));

  return promise.forget();
}

class ReadRemoteRssiTask final : public BluetoothReplyRunnable
{
public:
  ReadRemoteRssiTask(Promise* aPromise)
    : BluetoothReplyRunnable(nullptr, aPromise)
  {
    MOZ_ASSERT(aPromise);
  }

  bool
  ParseSuccessfulReply(JS::MutableHandle<JS::Value> aValue)
  {
    aValue.setUndefined();

    const BluetoothValue& v = mReply->get_BluetoothReplySuccess().value();
    NS_ENSURE_TRUE(v.type() == BluetoothValue::Tint32_t, false);

    aValue.setInt32(static_cast<int32_t>(v.get_int32_t()));
    return true;
  }
};

already_AddRefed<Promise>
BluetoothGatt::ReadRemoteRssi(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BT_ENSURE_TRUE_REJECT(
    mConnectionState == BluetoothConnectionState::Connected,
    promise,
    NS_ERROR_DOM_INVALID_STATE_ERR);

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  bs->GattClientReadRemoteRssiInternal(
    mClientIf, mDeviceAddr, new ReadRemoteRssiTask(promise));

  return promise.forget();
}

already_AddRefed<Promise>
BluetoothGatt::DiscoverServices(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BT_ENSURE_TRUE_REJECT(
    mConnectionState == BluetoothConnectionState::Connected &&
    !mDiscoveringServices,
    promise,
    NS_ERROR_DOM_INVALID_STATE_ERR);

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  mDiscoveringServices = true;
  mServices.Clear();
  BluetoothGattBinding::ClearCachedServicesValue(this);

  bs->DiscoverGattServicesInternal(
    mAppUuid, new BluetoothVoidReplyRunnable(nullptr, promise));

  return promise.forget();
}

void
BluetoothGatt::UpdateConnectionState(BluetoothConnectionState aState)
{
  BT_LOGR("GATT connection state changes to: %d", int(aState));
  mConnectionState = aState;

  // Dispatch connectionstatechanged event to application
  nsCOMPtr<nsIDOMEvent> event;
  nsresult rv = NS_NewDOMEvent(getter_AddRefs(event), this, nullptr, nullptr);
  NS_ENSURE_SUCCESS_VOID(rv);

  rv = event->InitEvent(NS_LITERAL_STRING(GATT_CONNECTION_STATE_CHANGED_ID),
                        false,
                        false);
  NS_ENSURE_SUCCESS_VOID(rv);

  DispatchTrustedEvent(event);
}

void
BluetoothGatt::HandleServicesDiscovered(const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothGattServiceId);

  const InfallibleTArray<BluetoothGattServiceId>& serviceIds =
    aValue.get_ArrayOfBluetoothGattServiceId();

  mServices.Clear();
  for (uint32_t i = 0; i < serviceIds.Length(); i++) {
    mServices.AppendElement(new BluetoothGattService(
      GetParentObject(), mAppUuid, serviceIds[i]));
  }

  BluetoothGattBinding::ClearCachedServicesValue(this);
}

void
BluetoothGatt::HandleIncludedServicesDiscovered(const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);

  const InfallibleTArray<BluetoothNamedValue>& values =
    aValue.get_ArrayOfBluetoothNamedValue();
  MOZ_ASSERT(values.Length() == 2); // ServiceId, IncludedServices
  MOZ_ASSERT(values[0].name().EqualsLiteral("serviceId"));
  MOZ_ASSERT(values[0].value().type() ==
             BluetoothValue::TBluetoothGattServiceId);
  MOZ_ASSERT(values[1].name().EqualsLiteral("includedServices"));
  MOZ_ASSERT(values[1].value().type() ==
             BluetoothValue::TArrayOfBluetoothGattServiceId);

  size_t index = mServices.IndexOf(
    values[0].value().get_BluetoothGattServiceId());
  NS_ENSURE_TRUE_VOID(index != mServices.NoIndex);

  nsRefPtr<BluetoothGattService> service = mServices.ElementAt(index);
  service->AssignIncludedServices(
    values[1].value().get_ArrayOfBluetoothGattServiceId());
}

void
BluetoothGatt::HandleCharacteristicsDiscovered(const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);

  const InfallibleTArray<BluetoothNamedValue>& values =
    aValue.get_ArrayOfBluetoothNamedValue();
  MOZ_ASSERT(values.Length() == 2); // ServiceId, Characteristics
  MOZ_ASSERT(values[0].name().EqualsLiteral("serviceId"));
  MOZ_ASSERT(values[0].value().type() == BluetoothValue::TBluetoothGattServiceId);
  MOZ_ASSERT(values[1].name().EqualsLiteral("characteristics"));
  MOZ_ASSERT(values[1].value().type() ==
             BluetoothValue::TArrayOfBluetoothGattCharAttribute);

  size_t index = mServices.IndexOf(
    values[0].value().get_BluetoothGattServiceId());
  NS_ENSURE_TRUE_VOID(index != mServices.NoIndex);

  nsRefPtr<BluetoothGattService> service = mServices.ElementAt(index);
  service->AssignCharacteristics(
    values[1].value().get_ArrayOfBluetoothGattCharAttribute());
}

void
BluetoothGatt::HandleDescriptorsDiscovered(const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);

  const InfallibleTArray<BluetoothNamedValue>& values =
    aValue.get_ArrayOfBluetoothNamedValue();
  MOZ_ASSERT(values.Length() == 3); // ServiceId, CharacteristicId, Descriptors
  MOZ_ASSERT(values[0].name().EqualsLiteral("serviceId"));
  MOZ_ASSERT(values[0].value().type() == BluetoothValue::TBluetoothGattServiceId);
  MOZ_ASSERT(values[1].name().EqualsLiteral("characteristicId"));
  MOZ_ASSERT(values[1].value().type() == BluetoothValue::TBluetoothGattId);
  MOZ_ASSERT(values[2].name().EqualsLiteral("descriptors"));
  MOZ_ASSERT(values[2].value().type() == BluetoothValue::TArrayOfBluetoothGattId);

  size_t index = mServices.IndexOf(
    values[0].value().get_BluetoothGattServiceId());
  NS_ENSURE_TRUE_VOID(index != mServices.NoIndex);

  nsRefPtr<BluetoothGattService> service = mServices.ElementAt(index);
  service->AssignDescriptors(values[1].value().get_BluetoothGattId(),
                             values[2].value().get_ArrayOfBluetoothGattId());
}

void
BluetoothGatt::HandleCharacteristicChanged(const BluetoothValue& aValue)
{
  MOZ_ASSERT(aValue.type() == BluetoothValue::TArrayOfBluetoothNamedValue);

  const InfallibleTArray<BluetoothNamedValue>& ids =
    aValue.get_ArrayOfBluetoothNamedValue();
  MOZ_ASSERT(ids.Length() == 2); // ServiceId, CharId
  MOZ_ASSERT(ids[0].name().EqualsLiteral("serviceId"));
  MOZ_ASSERT(ids[0].value().type() == BluetoothValue::TBluetoothGattServiceId);
  MOZ_ASSERT(ids[1].name().EqualsLiteral("charId"));
  MOZ_ASSERT(ids[1].value().type() == BluetoothValue::TBluetoothGattId);

  size_t index = mServices.IndexOf(ids[0].value().get_BluetoothGattServiceId());
  NS_ENSURE_TRUE_VOID(index != mServices.NoIndex);

  nsRefPtr<BluetoothGattService> service = mServices.ElementAt(index);
  nsTArray<nsRefPtr<BluetoothGattCharacteristic>> chars;
  service->GetCharacteristics(chars);

  index = chars.IndexOf(ids[1].value().get_BluetoothGattId());
  NS_ENSURE_TRUE_VOID(index != chars.NoIndex);
  nsRefPtr<BluetoothGattCharacteristic> characteristic = chars.ElementAt(index);

  // Dispatch characteristicchanged event to application
  BluetoothGattCharacteristicEventInit init;
  init.mCharacteristic = characteristic;
  nsRefPtr<BluetoothGattCharacteristicEvent> event =
    BluetoothGattCharacteristicEvent::Constructor(
      this,
      NS_LITERAL_STRING(GATT_CHARACTERISTIC_CHANGED_ID),
      init);

  DispatchTrustedEvent(event);
}

void
BluetoothGatt::Notify(const BluetoothSignal& aData)
{
  BT_LOGD("[D] %s", NS_ConvertUTF16toUTF8(aData.name()).get());
  NS_ENSURE_TRUE_VOID(mSignalRegistered);

  BluetoothValue v = aData.value();
  if (aData.name().EqualsLiteral("ClientRegistered")) {
    MOZ_ASSERT(v.type() == BluetoothValue::Tuint32_t);
    mClientIf = v.get_uint32_t();
  } else if (aData.name().EqualsLiteral("ClientUnregistered")) {
    mClientIf = 0;
  } else if (aData.name().EqualsLiteral(GATT_CONNECTION_STATE_CHANGED_ID)) {
    MOZ_ASSERT(v.type() == BluetoothValue::Tbool);

    BluetoothConnectionState state =
      v.get_bool() ? BluetoothConnectionState::Connected
                   : BluetoothConnectionState::Disconnected;
    UpdateConnectionState(state);
  } else if (aData.name().EqualsLiteral("ServicesDiscovered")) {
    HandleServicesDiscovered(v);
  } else if (aData.name().EqualsLiteral("DiscoverCompleted")) {
    MOZ_ASSERT(v.type() == BluetoothValue::Tbool);

    bool isDiscoverSuccess = v.get_bool();
    if (!isDiscoverSuccess) { // Clean all discovered attributes if failed
      mServices.Clear();
      BluetoothGattBinding::ClearCachedServicesValue(this);
    }

    mDiscoveringServices = false;
  } else if (aData.name().EqualsLiteral("IncludedServicesDiscovered")) {
    HandleIncludedServicesDiscovered(v);
  } else if (aData.name().EqualsLiteral("CharacteristicsDiscovered")) {
    HandleCharacteristicsDiscovered(v);
  } else if (aData.name().EqualsLiteral("DescriptorsDiscovered")) {
    HandleDescriptorsDiscovered(v);
  } else if (aData.name().EqualsLiteral(GATT_CHARACTERISTIC_CHANGED_ID)) {
    HandleCharacteristicChanged(v);
  } else {
    BT_WARNING("Not handling GATT signal: %s",
               NS_ConvertUTF16toUTF8(aData.name()).get());
  }
}

JSObject*
BluetoothGatt::WrapObject(JSContext* aContext,
                          JS::Handle<JSObject*> aGivenProto)
{
  return BluetoothGattBinding::Wrap(aContext, this, aGivenProto);
}
