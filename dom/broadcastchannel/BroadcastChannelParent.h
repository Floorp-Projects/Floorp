/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BroadcastChannelParent_h
#define mozilla_dom_BroadcastChannelParent_h

#include "mozilla/dom/PBroadcastChannelParent.h"

namespace mozilla {

namespace ipc {
class BackgroundParentImpl;
}

namespace dom {

class BroadcastChannelService;

class BroadcastChannelParent MOZ_FINAL : public PBroadcastChannelParent
{
  friend class mozilla::ipc::BackgroundParentImpl;

public:
  void CheckAndDeliver(const ClonedMessageData& aData,
                       const nsString& aOrigin,
                       const nsString& aChannel);

private:
  BroadcastChannelParent(const nsAString& aOrigin,
                         const nsAString& aChannel);
  ~BroadcastChannelParent();

  virtual bool
  RecvPostMessage(const ClonedMessageData& aData) MOZ_OVERRIDE;

  virtual bool RecvClose() MOZ_OVERRIDE;

  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  nsRefPtr<BroadcastChannelService> mService;
  nsString mOrigin;
  nsString mChannel;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_BroadcastChannelParent_h
