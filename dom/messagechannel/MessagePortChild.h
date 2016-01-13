/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MessagePortChild_h
#define mozilla_dom_MessagePortChild_h

#include "mozilla/Assertions.h"
#include "mozilla/dom/PMessagePortChild.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace dom {

class MessagePort;

class MessagePortChild final : public PMessagePortChild
{
public:
  NS_INLINE_DECL_REFCOUNTING(MessagePortChild)

  MessagePortChild() : mPort(nullptr) {}

  void SetPort(MessagePort* aPort)
  {
    mPort = aPort;
  }

private:
  ~MessagePortChild()
  {
    MOZ_ASSERT(!mPort);
  }

  virtual bool
  RecvEntangled(nsTArray<MessagePortMessage>&& aMessages) override;

  virtual bool
  RecvReceiveData(nsTArray<MessagePortMessage>&& aMessages) override;

  virtual bool RecvStopSendingDataConfirmed() override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  // This is a raw pointer because this child is owned by this MessagePort.
  MessagePort* mPort;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MessagePortChild_h
