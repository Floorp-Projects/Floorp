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

BroadcastChannelParent::BroadcastChannelParent(
                                            const PrincipalInfo& aPrincipalInfo,
                                            const nsACString& aOrigin,
                                            const nsAString& aChannel,
                                            bool aPrivateBrowsing)
  : mService(BroadcastChannelService::GetOrCreate())
  , mOrigin(aOrigin)
  , mChannel(aChannel)
  , mAppId(nsIScriptSecurityManager::UNKNOWN_APP_ID)
  , mIsInBrowserElement(false)
  , mPrivateBrowsing(aPrivateBrowsing)
{
  AssertIsOnBackgroundThread();
  mService->RegisterActor(this);

  if (aPrincipalInfo.type() ==PrincipalInfo::TContentPrincipalInfo) {
    const ContentPrincipalInfo& info =
      aPrincipalInfo.get_ContentPrincipalInfo();
    mAppId = info.appId();
    mIsInBrowserElement = info.isInBrowserElement();
  }
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

  mService->PostMessage(this, aData, mOrigin, mAppId, mIsInBrowserElement,
                        mChannel, mPrivateBrowsing);
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
BroadcastChannelParent::CheckAndDeliver(const ClonedMessageData& aData,
                                        const nsCString& aOrigin,
                                        uint64_t aAppId,
                                        bool aInBrowserElement,
                                        const nsString& aChannel,
                                        bool aPrivateBrowsing)
{
  AssertIsOnBackgroundThread();

  if (aOrigin == mOrigin &&
      aAppId == mAppId &&
      aInBrowserElement == mIsInBrowserElement &&
      aChannel == mChannel &&
      aPrivateBrowsing == mPrivateBrowsing) {
    // We need to duplicate data only if we have blobs or if the manager of
    // them is different than the manager of this parent actor.
    if (aData.blobsParent().IsEmpty() ||
        static_cast<BlobParent*>(aData.blobsParent()[0])->GetBackgroundManager() == Manager()) {
      unused << SendNotify(aData);
      return;
    }

    // Duplicate the data for this parent.
    ClonedMessageData newData(aData);

    // Ricreate the BlobParent for this new message.
    for (uint32_t i = 0, len = newData.blobsParent().Length(); i < len; ++i) {
      nsRefPtr<BlobImpl> impl =
        static_cast<BlobParent*>(newData.blobsParent()[i])->GetBlobImpl();

      PBlobParent* blobParent =
        BackgroundParent::GetOrCreateActorForBlobImpl(Manager(), impl);
      if (!blobParent) {
        return;
      }

      newData.blobsParent()[i] = blobParent;
    }

    unused << SendNotify(newData);
  }
}

} // dom namespace
} // mozilla namespace
