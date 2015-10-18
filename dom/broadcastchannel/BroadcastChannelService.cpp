/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BroadcastChannelService.h"
#include "BroadcastChannelParent.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/ipc/BlobParent.h"
#include "mozilla/ipc/BackgroundParent.h"

#ifdef XP_WIN
#undef PostMessage
#endif

namespace mozilla {

using namespace ipc;

namespace dom {

namespace {

BroadcastChannelService* sInstance = nullptr;

} // namespace

BroadcastChannelService::BroadcastChannelService()
{
  AssertIsOnBackgroundThread();

  // sInstance is a raw BroadcastChannelService*.
  MOZ_ASSERT(!sInstance);
  sInstance = this;
}

BroadcastChannelService::~BroadcastChannelService()
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(sInstance == this);
  MOZ_ASSERT(mAgents.Count() == 0);

  sInstance = nullptr;
}

// static
already_AddRefed<BroadcastChannelService>
BroadcastChannelService::GetOrCreate()
{
  AssertIsOnBackgroundThread();

  RefPtr<BroadcastChannelService> instance = sInstance;
  if (!instance) {
    instance = new BroadcastChannelService();
  }
  return instance.forget();
}

void
BroadcastChannelService::RegisterActor(BroadcastChannelParent* aParent)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(!mAgents.Contains(aParent));

  mAgents.PutEntry(aParent);
}

void
BroadcastChannelService::UnregisterActor(BroadcastChannelParent* aParent)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(mAgents.Contains(aParent));

  mAgents.RemoveEntry(aParent);
}

void
BroadcastChannelService::PostMessage(BroadcastChannelParent* aParent,
                                     const ClonedMessageData& aData,
                                     const nsACString& aOrigin,
                                     const nsAString& aChannel,
                                     bool aPrivateBrowsing)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(mAgents.Contains(aParent));

  // We need to keep the array alive for the life-time of this operation.
  nsTArray<RefPtr<BlobImpl>> blobs;
  if (!aData.blobsParent().IsEmpty()) {
    blobs.SetCapacity(aData.blobsParent().Length());

    for (uint32_t i = 0, len = aData.blobsParent().Length(); i < len; ++i) {
      RefPtr<BlobImpl> impl =
        static_cast<BlobParent*>(aData.blobsParent()[i])->GetBlobImpl();
     MOZ_ASSERT(impl);
     blobs.AppendElement(impl);
    }
  }

  for (auto iter = mAgents.Iter(); !iter.Done(); iter.Next()) {
    BroadcastChannelParent* parent = iter.Get()->GetKey();
    MOZ_ASSERT(parent);

    if (parent != aParent) {
      parent->CheckAndDeliver(aData, PromiseFlatCString(aOrigin),
                              PromiseFlatString(aChannel), aPrivateBrowsing);
    }
  }
}

} // namespace dom
} // namespace mozilla
