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
#include "mozilla/dom/ContentChild.h"
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

BluetoothMapRequestHandle::BluetoothMapRequestHandle(nsPIDOMWindowInner* aOwner)
  : mOwner(aOwner)
{
  MOZ_ASSERT(aOwner);
}

BluetoothMapRequestHandle::~BluetoothMapRequestHandle()
{
}

already_AddRefed<BluetoothMapRequestHandle>
BluetoothMapRequestHandle::Create(nsPIDOMWindowInner* aOwner)
{
  MOZ_ASSERT(aOwner);

  RefPtr<BluetoothMapRequestHandle> handle =
    new BluetoothMapRequestHandle(aOwner);

  return handle.forget();
}

already_AddRefed<Promise>
BluetoothMapRequestHandle::ReplyToFolderListing(long aMasId,
  const nsAString& aFolderlists, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  bs->ReplyToMapFolderListing(aMasId, aFolderlists,
    new BluetoothVoidReplyRunnable(nullptr, promise));

  return promise.forget();
}

already_AddRefed<Promise>
BluetoothMapRequestHandle::ReplyToMessagesListing(long aMasId,
                                                  Blob& aBlob,
                                                  bool aNewMessage,
                                                  const nsAString& aTimestamp,
                                                  int aSize,
                                                  ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    // In-process reply
    bs->ReplyToMapMessagesListing(aMasId, &aBlob, aNewMessage, aTimestamp,
      aSize, new BluetoothVoidReplyRunnable(nullptr, promise));
  } else {
    ContentChild *cc = ContentChild::GetSingleton();
    if (!cc) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    BlobChild* actor = cc->GetOrCreateActorForBlob(&aBlob);
    if (!actor) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    bs->ReplyToMapMessagesListing(nullptr, actor, aMasId, aNewMessage,
      aTimestamp, aSize, new BluetoothVoidReplyRunnable(nullptr, promise));
  }

  return promise.forget();
}

already_AddRefed<Promise>
BluetoothMapRequestHandle::ReplyToGetMessage(long aMasId, Blob& aBlob,
                                             ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    // In-process reply
    bs->ReplyToMapGetMessage(&aBlob, aMasId,
      new BluetoothVoidReplyRunnable(nullptr, promise));
  } else {
    ContentChild *cc = ContentChild::GetSingleton();
    if (!cc) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    BlobChild* actor = cc->GetOrCreateActorForBlob(&aBlob);
    if (!actor) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    bs->ReplyToMapGetMessage(nullptr, actor, aMasId,
      new BluetoothVoidReplyRunnable(nullptr, promise));
  }

  return promise.forget();
}

already_AddRefed<Promise>
BluetoothMapRequestHandle::ReplyToSetMessageStatus(long aMasId,
                                                   bool aStatus,
                                                   ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  bs->ReplyToMapSetMessageStatus(aMasId, aStatus,
    new BluetoothVoidReplyRunnable(nullptr, promise));

  return promise.forget();
}

already_AddRefed<Promise>
BluetoothMapRequestHandle::ReplyToSendMessage(long aMasId,
                                              const nsAString& aHandleId,
                                              bool aStatus,
                                              ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  bs->ReplyToMapSendMessage(aMasId, aHandleId, aStatus,
    new BluetoothVoidReplyRunnable(nullptr, promise));

  return promise.forget();
}

already_AddRefed<Promise>
BluetoothMapRequestHandle::ReplyToMessageUpdate(long aMasId,
                                                bool aStatus,
                                                ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetParentObject());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  NS_ENSURE_TRUE(!aRv.Failed(), nullptr);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  bs->ReplyToMapMessageUpdate(aMasId, aStatus,
    new BluetoothVoidReplyRunnable(nullptr, promise));

  return promise.forget();
}

JSObject*
BluetoothMapRequestHandle::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto)
{
  return BluetoothMapRequestHandleBinding::Wrap(aCx, this, aGivenProto);
}
