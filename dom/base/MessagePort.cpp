/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MessagePort.h"
#include "mozilla/dom/MessageChannel.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(MessagePort, nsDOMEventTargetHelper,
                                     mEntangledPort)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(MessagePort)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(MessagePort, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MessagePort, nsDOMEventTargetHelper)

MessagePort::MessagePort(nsPIDOMWindow* aWindow)
  : nsDOMEventTargetHelper(aWindow)
{
  MOZ_COUNT_CTOR(MessagePort);
  SetIsDOMBinding();
}

MessagePort::~MessagePort()
{
  MOZ_COUNT_DTOR(MessagePort);
  Close();
}

JSObject*
MessagePort::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return MessagePortBinding::Wrap(aCx, aScope, this);
}

void
MessagePort::PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                         const Optional<JS::Handle<JS::Value> >& aTransfer)
{
  if (!mEntangledPort) {
    return;
  }

  // TODO
}

void
MessagePort::Start()
{
  // TODO
}

void
MessagePort::Close()
{
  if (!mEntangledPort) {
    return;
  }

  // This avoids loops.
  nsRefPtr<MessagePort> port = mEntangledPort;
  mEntangledPort = nullptr;

  // Let's disentangle the 2 ports symmetrically.
  port->Close();
}

void
MessagePort::Entangle(MessagePort* aMessagePort)
{
  MOZ_ASSERT(aMessagePort);
  MOZ_ASSERT(aMessagePort != this);

  Close();

  mEntangledPort = aMessagePort;
}

} // namespace dom
} // namespace mozilla
