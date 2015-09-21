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
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BluetoothGattServer)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(BluetoothGattServer, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(BluetoothGattServer, DOMEventTargetHelper)

BluetoothGattServer::BluetoothGattServer(nsPIDOMWindow* aOwner)
  : mOwner(aOwner)
  , mServerIf(0)
  , mValid(true)
{ }

BluetoothGattServer::~BluetoothGattServer()
{
  Invalidate();
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

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  if (mServerIf > 0) {
    bs->UnregisterGattServerInternal(mServerIf, nullptr);
  }

  UnregisterBluetoothSignalHandler(mAppUuid, this);
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

  if (mAppUuid.IsEmpty()) {
    nsresult rv = GenerateUuid(mAppUuid);
    BT_ENSURE_TRUE_REJECT(NS_SUCCEEDED(rv) && !mAppUuid.IsEmpty(),
                          promise,
                          NS_ERROR_DOM_OPERATION_ERR);
    RegisterBluetoothSignalHandler(mAppUuid, this);
  }

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
