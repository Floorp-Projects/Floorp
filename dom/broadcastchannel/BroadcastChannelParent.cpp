/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BroadcastChannelParent.h"
#include "BroadcastChannelService.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/unused.h"

namespace mozilla {

using namespace ipc;

namespace dom {

BroadcastChannelParent::BroadcastChannelParent(
                                            const nsAString& aOrigin,
                                            const nsAString& aChannel)
  : mService(BroadcastChannelService::GetOrCreate())
  , mOrigin(aOrigin)
  , mChannel(aChannel)
{
  AssertIsOnBackgroundThread();
  mService->RegisterActor(this);
}

BroadcastChannelParent::~BroadcastChannelParent()
{
  AssertIsOnBackgroundThread();
}

bool
BroadcastChannelParent::RecvPostMessage(
                                       const BroadcastChannelMessageData& aData)
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!mService)) {
    return false;
  }

  mService->PostMessage(this, aData, mOrigin, mChannel);
  return true;
}

bool
BroadcastChannelParent::RecvClose()
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!mService)) {
    return false;
  }

  mService->UnregisterActor(this);
  mService = nullptr;

  unused << Send__delete__(this);

  return true;
}

void
BroadcastChannelParent::ActorDestroy(ActorDestroyReason aWhy)
{
  AssertIsOnBackgroundThread();

  if (mService) {
    // This object is about to be released and with it, also mService will be
    // released too.
    mService->UnregisterActor(this);
  }
}

void
BroadcastChannelParent::CheckAndDeliver(
                                       const BroadcastChannelMessageData& aData,
                                       const nsString& aOrigin,
                                       const nsString& aChannel)
{
  AssertIsOnBackgroundThread();

  if (aOrigin == mOrigin && aChannel == mChannel) {
    unused << SendNotify(aData);
  }
}

} // dom namespace
} // mozilla namespace
