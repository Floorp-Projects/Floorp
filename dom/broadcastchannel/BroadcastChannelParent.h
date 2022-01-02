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
}  // namespace ipc

namespace dom {

class BroadcastChannelService;

class BroadcastChannelParent final : public PBroadcastChannelParent {
  friend class mozilla::ipc::BackgroundParentImpl;

  using PrincipalInfo = mozilla::ipc::PrincipalInfo;

 private:
  explicit BroadcastChannelParent(const nsAString& aOriginChannelKey);
  ~BroadcastChannelParent();

  virtual mozilla::ipc::IPCResult RecvPostMessage(
      const MessageData& aData) override;

  virtual mozilla::ipc::IPCResult RecvClose() override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  RefPtr<BroadcastChannelService> mService;
  const nsString mOriginChannelKey;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_BroadcastChannelParent_h
