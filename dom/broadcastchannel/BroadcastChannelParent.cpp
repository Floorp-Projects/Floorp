/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BroadcastChannelParent.h"
#include "BroadcastChannelService.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/ipc/BlobParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/unused.h"
#include "nsIScriptSecurityManager.h"

namespace mozilla {

using namespace ipc;

namespace dom {

BroadcastChannelParent::BroadcastChannelParent(const nsACString& aOrigin,
                                               const nsAString& aChannel,
                                               bool aPrivateBrowsing)
  : mService(BroadcastChannelService::GetOrCreate())
  , mOrigin(aOrigin)
  , mChannel(aChannel)
  , mPrivateBrowsing(aPrivateBrowsing)
{
  AssertIsOnBackgroundThread();
  mService->RegisterActor(this);
}

BroadcastChannelParent::~BroadcastChannelParent()
{
  AssertIsOnBackgroundThread();
}

bool
BroadcastChannelParent::RecvPostMessage(const ClonedMessageData& aData)
{
  AssertIsOnBackgroundThread();

  if (NS_WARN_IF(!mService)) {
    return false;
  }

  mService->PostMessage(this, aData, mOrigin, mChannel, mPrivateBrowsing);
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

  Unused << Send__delete__(this);

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
BroadcastChannelParent::CheckAndDeliver(const ClonedMessageData& aData,
                                        const nsCString& aOrigin,
                                        const nsString& aChannel,
                                        bool aPrivateBrowsing)
{
  AssertIsOnBackgroundThread();

  if (aOrigin == mOrigin &&
      aChannel == mChannel &&
      aPrivateBrowsing == mPrivateBrowsing) {
    // Duplicate the data for this parent.
    ClonedMessageData newData(aData);

    // Ricreate the BlobParent for this new message.
    for (uint32_t i = 0, len = newData.blobsParent().Length(); i < len; ++i) {
      RefPtr<BlobImpl> impl =
        static_cast<BlobParent*>(newData.blobsParent()[i])->GetBlobImpl();

      PBlobParent* blobParent =
        BackgroundParent::GetOrCreateActorForBlobImpl(Manager(), impl);
      if (!blobParent) {
        return;
      }

      newData.blobsParent()[i] = blobParent;
    }

    Unused << SendNotify(newData);
  }
}

} // namespace dom
} // namespace mozilla
