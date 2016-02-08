/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothCommon.h"
#include "BluetoothDevice.h"
#include "BluetoothObexAuthHandle.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"

#include "mozilla/dom/BluetoothObexAuthHandleBinding.h"
#include "mozilla/dom/Promise.h"

using namespace mozilla;
using namespace dom;

USING_BLUETOOTH_NAMESPACE

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(BluetoothObexAuthHandle, mOwner)
NS_IMPL_CYCLE_COLLECTING_ADDREF(BluetoothObexAuthHandle)
NS_IMPL_CYCLE_COLLECTING_RELEASE(BluetoothObexAuthHandle)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BluetoothObexAuthHandle)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

BluetoothObexAuthHandle::BluetoothObexAuthHandle(nsPIDOMWindowInner* aOwner)
  : mOwner(aOwner)
{
  MOZ_ASSERT(aOwner);
}

BluetoothObexAuthHandle::~BluetoothObexAuthHandle()
{
}

already_AddRefed<BluetoothObexAuthHandle>
BluetoothObexAuthHandle::Create(nsPIDOMWindowInner* aOwner)
{
  MOZ_ASSERT(aOwner);

  RefPtr<BluetoothObexAuthHandle> handle =
    new BluetoothObexAuthHandle(aOwner);

  return handle.forget();
}

already_AddRefed<Promise>
BluetoothObexAuthHandle::SetPassword(const nsAString& aPassword, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  bs->SetObexPassword(aPassword,
                      new BluetoothVoidReplyRunnable(nullptr, promise));

  return promise.forget();
}

already_AddRefed<Promise>
BluetoothObexAuthHandle::Reject(ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BluetoothService* bs = BluetoothService::Get();
  BT_ENSURE_TRUE_REJECT(bs, promise, NS_ERROR_NOT_AVAILABLE);

  bs->RejectObexAuth(new BluetoothVoidReplyRunnable(nullptr, promise));

  return promise.forget();
}

JSObject*
BluetoothObexAuthHandle::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto)
{
  return BluetoothObexAuthHandleBinding::Wrap(aCx, this, aGivenProto);
}
