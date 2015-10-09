/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothCommon.h"
#include "BluetoothMapRequestHandle.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"

#include "mozilla/dom/BluetoothMapRequestHandleBinding.h"
#include "mozilla/dom/Promise.h"

using namespace mozilla;
using namespace dom;

USING_BLUETOOTH_NAMESPACE

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(BluetoothMapRequestHandle, mOwner)
NS_IMPL_CYCLE_COLLECTING_ADDREF(BluetoothMapRequestHandle)
NS_IMPL_CYCLE_COLLECTING_RELEASE(BluetoothMapRequestHandle)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BluetoothMapRequestHandle)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

BluetoothMapRequestHandle::BluetoothMapRequestHandle(nsPIDOMWindow* aOwner)
  : mOwner(aOwner)
{
  MOZ_ASSERT(aOwner);
}

BluetoothMapRequestHandle::~BluetoothMapRequestHandle()
{
}

already_AddRefed<BluetoothMapRequestHandle>
BluetoothMapRequestHandle::Create(nsPIDOMWindow* aOwner)
{
  MOZ_ASSERT(aOwner);

  nsRefPtr<BluetoothMapRequestHandle> handle =
    new BluetoothMapRequestHandle(aOwner);

  return handle.forget();
}

already_AddRefed<Promise>
BluetoothMapRequestHandle::ReplyToFolderListing(long aMasId,
  const nsAString& aFolderlists, ErrorResult& aRv)
{
  // TODO: Implement this fuction in Bug 1208492
  return nullptr;
}

already_AddRefed<Promise>
BluetoothMapRequestHandle::ReplyToMessagesListing(long aMasId,
                                                  Blob& aBlob,
                                                  bool aNewMessage,
                                                  const nsAString& aTimestamp,
                                                  int aSize,
                                                  ErrorResult& aRv)
{
  // TODO: Implement this fuction in Bug 1208492
  return nullptr;
}

already_AddRefed<Promise>
BluetoothMapRequestHandle::ReplyToGetMessage(long aMasId, Blob& aBlob,
                                             ErrorResult& aRv)
{
  // TODO: Implement this fuction in Bug 1208492
  return nullptr;
}

already_AddRefed<Promise>
BluetoothMapRequestHandle::ReplyToSetMessageStatus(long aMasId,
                                                   bool aStatus,
                                                   ErrorResult& aRv)
{
  // TODO: Implement this fuction in Bug 1208492
  return nullptr;
}

already_AddRefed<Promise>
BluetoothMapRequestHandle::ReplyToSendMessage(long aMasId,
                                              bool aStatus,
                                              ErrorResult& aRv)
{
  // TODO: Implement this fuction in Bug 1208492
  return nullptr;
}

already_AddRefed<Promise>
BluetoothMapRequestHandle::ReplyToMessageUpdate(long aMasId,
                                                bool aStatus,
                                                ErrorResult& aRv)
{
  // TODO: Implement this fuction in Bug 1208492
  return nullptr;
}

JSObject*
BluetoothMapRequestHandle::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto)
{
  return BluetoothMapRequestHandleBinding::Wrap(aCx, this, aGivenProto);
}
