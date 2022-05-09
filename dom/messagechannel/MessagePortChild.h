/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MessagePortChild_h
#define mozilla_dom_MessagePortChild_h

#include "mozilla/Assertions.h"
#include "mozilla/dom/PMessagePortChild.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom {

class MessagePort;

class MessagePortChild final : public PMessagePortChild {
  friend class PMessagePortChild;

 public:
  NS_INLINE_DECL_REFCOUNTING(MessagePortChild)

  MessagePortChild();

  void SetPort(MessagePort* aPort) { mPort = aPort; }

 private:
  ~MessagePortChild() { MOZ_ASSERT(!mPort); }

  mozilla::ipc::IPCResult RecvEntangled(nsTArray<MessageData>&& aMessages);

  mozilla::ipc::IPCResult RecvReceiveData(nsTArray<MessageData>&& aMessages);

  mozilla::ipc::IPCResult RecvStopSendingDataConfirmed();

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  // This is a raw pointer because this child is owned by this MessagePort.
  MessagePort* mPort;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_MessagePortChild_h
