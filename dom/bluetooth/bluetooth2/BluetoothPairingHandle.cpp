/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothCommon.h"
#include "BluetoothDevice.h"
#include "BluetoothPairingHandle.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"

#include "mozilla/dom/BluetoothPairingHandleBinding.h"
#include "mozilla/dom/Promise.h"

using namespace mozilla;
using namespace dom;

USING_BLUETOOTH_NAMESPACE

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(BluetoothPairingHandle, mOwner)
NS_IMPL_CYCLE_COLLECTING_ADDREF(BluetoothPairingHandle)
NS_IMPL_CYCLE_COLLECTING_RELEASE(BluetoothPairingHandle)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BluetoothPairingHandle)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

BluetoothPairingHandle::BluetoothPairingHandle(nsPIDOMWindow* aOwner,
                                               const nsAString& aDeviceAddress,
                                               const nsAString& aType,
                                               const nsAString& aPasskey)
  : mOwner(aOwner)
  , mDeviceAddress(aDeviceAddress)
  , mType(aType)
  , mPasskey(aPasskey)
{
  MOZ_ASSERT(aOwner && !aDeviceAddress.IsEmpty() && !aType.IsEmpty());

  if (aType.EqualsLiteral(PAIRING_REQ_TYPE_DISPLAYPASSKEY) ||
      aType.EqualsLiteral(PAIRING_REQ_TYPE_CONFIRMATION)) {
    MOZ_ASSERT(!aPasskey.IsEmpty());
  } else {
    MOZ_ASSERT(aPasskey.IsEmpty());
  }
}

BluetoothPairingHandle::~BluetoothPairingHandle()
{
}

already_AddRefed<BluetoothPairingHandle>
BluetoothPairingHandle::Create(nsPIDOMWindow* aOwner,
                               const nsAString& aDeviceAddress,
                               const nsAString& aType,
                               const nsAString& aPasskey)
{
  MOZ_ASSERT(aOwner && !aDeviceAddress.IsEmpty() && !aType.IsEmpty());

  nsRefPtr<BluetoothPairingHandle> handle =
    new BluetoothPairingHandle(aOwner, aDeviceAddress, aType, aPasskey);

  return handle.forget();
}

already_AddRefed<Promise>
BluetoothPairingHandle::SetPinCode(const nsAString& aPinCode, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BT_ENSURE_TRUE_REJECT(mType.EqualsLiteral(PAIRING_REQ_TYPE_ENTERPINCODE),
                        NS_ERROR_DOM_INVALID_STATE_ERR);

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, NS_ERROR_NOT_AVAILABLE);

  nsRefPtr<BluetoothReplyRunnable> result =
    new BluetoothVoidReplyRunnable(nullptr /* DOMRequest */,
                                   promise,
                                   NS_LITERAL_STRING("SetPinCode"));
  bs->PinReplyInternal(mDeviceAddress, true /* accept */, aPinCode, result);

  return promise.forget();
}

already_AddRefed<Promise>
BluetoothPairingHandle::Accept(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BT_ENSURE_TRUE_REJECT(mType.EqualsLiteral(PAIRING_REQ_TYPE_CONFIRMATION) ||
                        mType.EqualsLiteral(PAIRING_REQ_TYPE_CONSENT),
                        NS_ERROR_DOM_INVALID_STATE_ERR);

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, NS_ERROR_NOT_AVAILABLE);

  BluetoothSspVariant variant;
  BT_ENSURE_TRUE_REJECT(GetSspVariant(variant), NS_ERROR_DOM_OPERATION_ERR);

  nsRefPtr<BluetoothReplyRunnable> result =
    new BluetoothVoidReplyRunnable(nullptr /* DOMRequest */,
                                   promise,
                                   NS_LITERAL_STRING("Accept"));
  bs->SspReplyInternal(
    mDeviceAddress, variant, true /* aAccept */, result);

  return promise.forget();
}

already_AddRefed<Promise>
BluetoothPairingHandle::Reject(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, NS_ERROR_NOT_AVAILABLE);

  nsRefPtr<BluetoothReplyRunnable> result =
    new BluetoothVoidReplyRunnable(nullptr /* DOMRequest */,
                                   promise,
                                   NS_LITERAL_STRING("Reject"));

  if (mType.EqualsLiteral(PAIRING_REQ_TYPE_ENTERPINCODE)) { // Pin request
    bs->PinReplyInternal(
      mDeviceAddress, false /* aAccept */, EmptyString(), result);
  } else { // Ssp request
    BluetoothSspVariant variant;
    BT_ENSURE_TRUE_REJECT(GetSspVariant(variant), NS_ERROR_DOM_OPERATION_ERR);

    bs->SspReplyInternal(
      mDeviceAddress, variant, false /* aAccept */, result);
  }

  return promise.forget();
}

bool
BluetoothPairingHandle::GetSspVariant(BluetoothSspVariant& aVariant)
{
  if (mType.EqualsLiteral(PAIRING_REQ_TYPE_DISPLAYPASSKEY)) {
    aVariant = BluetoothSspVariant::SSP_VARIANT_PASSKEY_NOTIFICATION;
  } else if (mType.EqualsLiteral(PAIRING_REQ_TYPE_CONFIRMATION)) {
    aVariant = BluetoothSspVariant::SSP_VARIANT_PASSKEY_CONFIRMATION;
  } else if (mType.EqualsLiteral(PAIRING_REQ_TYPE_CONSENT)) {
    aVariant = BluetoothSspVariant::SSP_VARIANT_CONSENT;
  } else {
    BT_LOGR("Invalid SSP variant name: %s",
            NS_ConvertUTF16toUTF8(mType).get());
    aVariant = SSP_VARIANT_PASSKEY_CONFIRMATION; // silences compiler warning
    return false;
  }

  return true;
}

JSObject*
BluetoothPairingHandle::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto)
{
  return BluetoothPairingHandleBinding::Wrap(aCx, this, aGivenProto);
}
