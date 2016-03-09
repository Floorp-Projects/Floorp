/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MessageChannel.h"

#include "mozilla/dom/MessageChannelBinding.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "nsContentUtils.h"
#include "nsIDocument.h"
#include "nsIGlobalObject.h"
#include "nsIPrincipal.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MessageChannel, mGlobal, mPort1, mPort2)
NS_IMPL_CYCLE_COLLECTING_ADDREF(MessageChannel)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MessageChannel)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MessageChannel)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

MessageChannel::MessageChannel(nsIGlobalObject* aGlobal)
  : mGlobal(aGlobal)
{
  MOZ_ASSERT(aGlobal);
}

MessageChannel::~MessageChannel()
{
}

JSObject*
MessageChannel::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MessageChannelBinding::Wrap(aCx, this, aGivenProto);
}

/* static */ already_AddRefed<MessageChannel>
MessageChannel::Constructor(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  return Constructor(global, aRv);
}

/* static */ already_AddRefed<MessageChannel>
MessageChannel::Constructor(nsIGlobalObject* aGlobal, ErrorResult& aRv)
{
  MOZ_ASSERT(aGlobal);

  nsID portUUID1;
  aRv = nsContentUtils::GenerateUUIDInPlace(portUUID1);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsID portUUID2;
  aRv = nsContentUtils::GenerateUUIDInPlace(portUUID2);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<MessageChannel> channel = new MessageChannel(aGlobal);

  channel->mPort1 = MessagePort::Create(aGlobal, portUUID1, portUUID2, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  channel->mPort2 = MessagePort::Create(aGlobal, portUUID2, portUUID1, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  channel->mPort1->UnshippedEntangle(channel->mPort2);
  channel->mPort2->UnshippedEntangle(channel->mPort1);

  return channel.forget();
}

} // namespace dom
} // namespace mozilla
