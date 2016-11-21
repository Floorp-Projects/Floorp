/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BroadcastChannelParent_h
#define mozilla_dom_BroadcastChannelParent_h

#include "mozilla/dom/PBroadcastChannelParent.h"

namespace mozilla {

namespace ipc {
class BackgroundParentImpl;
class PrincipalInfo;
} // namespace ipc

namespace dom {

class BroadcastChannelParentMessage;
class BroadcastChannelService;

class BroadcastChannelParent final : public PBroadcastChannelParent
{
  NS_INLINE_DECL_REFCOUNTING(BroadcastChannelParent)

  friend class mozilla::ipc::BackgroundParentImpl;

  typedef mozilla::ipc::PrincipalInfo PrincipalInfo;

public:
  void Deliver(BroadcastChannelParentMessage* aMsg);

  bool Destroyed() const
  {
    return mStatus == eDestroyed;
  }

  void Start();

private:
  explicit BroadcastChannelParent(const nsAString& aOriginChannelKey);
  ~BroadcastChannelParent();

  virtual mozilla::ipc::IPCResult
  RecvPostMessage(const ClonedMessageData& aData) override;

  virtual mozilla::ipc::IPCResult RecvClose() override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  void DeliverInternal(BroadcastChannelParentMessage* aMsg);

  void Shutdown();

  RefPtr<BroadcastChannelService> mService;
  const nsString mOriginChannelKey;

  // Messages received from the child actor, but not broadcasted yet.
  nsTArray<RefPtr<BroadcastChannelParentMessage>> mPendingMessages;

  // Messages to be sent to the child actor.
  nsTArray<RefPtr<BroadcastChannelParentMessage>> mDeliveringMessages;

  enum {
    // This is the default state when the actor is created. All the received
    // messages will be stored in mPendingMessages. All the broadcasted messages
    // will be stored in mDeliveringMessages.
    // If RecvClose() is called, we move the state to mClosing.
    // This state can change only if ActorDestroy() or Start() is called.
    eInitializing,

    // This state is set by Start() if we where in eInitializing state.
    // The connection with BroadcastChannelService is done and all the pending
    // messages are dispatched/broadcasted.
    eInitialized,

    // If RecvClose() is called when we are in eInitializing state, we still
    // need to wait for Start() (or ActorDestroy()).
    // When Start() is called, we will call Send__delete__ after flushing the
    // pending queue.
    eClosing,

    // The actor has been destroyed.
    eDestroyed
  } mStatus;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_BroadcastChannelParent_h
