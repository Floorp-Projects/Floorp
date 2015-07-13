/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BroadcastChannelService_h
#define mozilla_dom_BroadcastChannelService_h

#include "nsISupportsImpl.h"
#include "nsHashKeys.h"
#include "nsTHashtable.h"

#ifdef XP_WIN
#undef PostMessage
#endif

namespace mozilla {
namespace dom {

class BroadcastChannelParent;
class ClonedMessageData;

class BroadcastChannelService final
{
public:
  NS_INLINE_DECL_REFCOUNTING(BroadcastChannelService)

  static already_AddRefed<BroadcastChannelService> GetOrCreate();

  void RegisterActor(BroadcastChannelParent* aParent);
  void UnregisterActor(BroadcastChannelParent* aParent);

  void PostMessage(BroadcastChannelParent* aParent,
                   const ClonedMessageData& aData,
                   const nsACString& aOrigin,
                   uint64_t aAppId,
                   bool aIsInBrowserElement,
                   const nsAString& aChannel,
                   bool aPrivateBrowsing);

private:
  BroadcastChannelService();
  ~BroadcastChannelService();

  nsTHashtable<nsPtrHashKey<BroadcastChannelParent>> mAgents;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_BroadcastChannelService_h
