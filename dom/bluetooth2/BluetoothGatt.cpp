/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothUtils.h"
#include "mozilla/dom/bluetooth/BluetoothGatt.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/dom/BluetoothGattBinding.h"
#include "mozilla/dom/Promise.h"
#include "nsIUUIDGenerator.h"
#include "nsServiceManagerUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

USING_BLUETOOTH_NAMESPACE

NS_IMPL_CYCLE_COLLECTION_CLASS(BluetoothGatt)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(BluetoothGatt,
                                                DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(BluetoothGatt,
                                                  DOMEventTargetHelper)
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

  bs->UnregisterBluetoothSignalHandler(mAppUuid, this);
}

void
BluetoothGatt::GenerateUuid(nsAString &aUuidString)
{
  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> uuidGenerator =
    do_GetService("@mozilla.org/uuid-generator;1", &rv);
  NS_ENSURE_SUCCESS_VOID(rv);

  nsID uuid;
  rv = uuidGenerator->GenerateUUIDInPlace(&uuid);
  NS_ENSURE_SUCCESS_VOID(rv);

  // Build a string in {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx} format
  char uuidBuffer[NSID_LENGTH];
  uuid.ToProvidedString(uuidBuffer);
  NS_ConvertASCIItoUTF16 uuidString(uuidBuffer);

  // Remove {} and the null terminator
  aUuidString.Assign(Substring(uuidString, 1, NSID_LENGTH - 3));
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

  bs->UnregisterBluetoothSignalHandler(mAppUuid, this);
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
    NS_ERROR_DOM_INVALID_STATE_ERR);

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, NS_ERROR_NOT_AVAILABLE);

  if (mAppUuid.IsEmpty()) {
    GenerateUuid(mAppUuid);
    BT_ENSURE_TRUE_REJECT(!mAppUuid.IsEmpty(),
                          NS_ERROR_DOM_OPERATION_ERR);
    bs->RegisterBluetoothSignalHandler(mAppUuid, this);
  }

  UpdateConnectionState(BluetoothConnectionState::Connecting);
  nsRefPtr<BluetoothReplyRunnable> result =
    new BluetoothVoidReplyRunnable(nullptr /* DOMRequest */,
                                   promise,
                                   NS_LITERAL_STRING("ConnectGattClient"));
  bs->ConnectGattClientInternal(mAppUuid,
                                mDeviceAddr,
                                result);

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
    NS_ERROR_DOM_INVALID_STATE_ERR);

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, NS_ERROR_NOT_AVAILABLE);

  UpdateConnectionState(BluetoothConnectionState::Disconnecting);
  nsRefPtr<BluetoothReplyRunnable> result =
    new BluetoothVoidReplyRunnable(nullptr /* DOMRequest */,
                                   promise,
                                   NS_LITERAL_STRING("DisconnectGattClient"));
  bs->DisconnectGattClientInternal(mAppUuid, mDeviceAddr, result);

  return promise.forget();
}

void
BluetoothGatt::UpdateConnectionState(BluetoothConnectionState aState)
{
  BT_API2_LOGR("GATT connection state changes to: %d", int(aState));
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
BluetoothGatt::Notify(const BluetoothSignal& aData)
{
  BT_LOGD("[D] %s", NS_ConvertUTF16toUTF8(aData.name()).get());

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
  } else {
    BT_WARNING("Not handling GATT signal: %s",
               NS_ConvertUTF16toUTF8(aData.name()).get());
  }
}

JSObject*
BluetoothGatt::WrapObject(JSContext* aContext)
{
  return BluetoothGattBinding::Wrap(aContext, this);
}
