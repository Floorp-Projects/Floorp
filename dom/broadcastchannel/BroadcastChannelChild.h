/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BroadcastChannelChild_h
#define mozilla_dom_BroadcastChannelChild_h

#include "mozilla/dom/PBroadcastChannelChild.h"

namespace mozilla {

namespace ipc {
class BackgroundChildImpl;
} // namespace ipc

namespace dom {

class BroadcastChannel;

class BroadcastChannelChild final : public PBroadcastChannelChild
{
  friend class mozilla::ipc::BackgroundChildImpl;

public:
  NS_INLINE_DECL_REFCOUNTING(BroadcastChannelChild)

  void SetParent(BroadcastChannel* aBC)
  {
    mBC = aBC;
  }

  virtual mozilla::ipc::IPCResult RecvNotify(const ClonedMessageData& aData) override;

  bool IsActorDestroyed() const
  {
    return mActorDestroyed;
  }

private:
  explicit BroadcastChannelChild(const nsACString& aOrigin);
  ~BroadcastChannelChild();

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  // This raw pointer is actually the parent object.
  // It's set to null when the parent object is deleted.
  BroadcastChannel* mBC;

  nsString mOrigin;

  bool mActorDestroyed;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_BroadcastChannelChild_h
