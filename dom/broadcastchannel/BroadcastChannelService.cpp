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

} // anonymous namespace

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

  nsRefPtr<BroadcastChannelService> instance = sInstance;
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

namespace {

struct MOZ_STACK_CLASS PostMessageData final
{
  PostMessageData(BroadcastChannelParent* aParent,
                  const ClonedMessageData& aData,
                  const nsAString& aOrigin,
                  uint64_t aAppId,
                  bool aIsInBrowserElement,
                  const nsAString& aChannel,
                  bool aPrivateBrowsing)
    : mParent(aParent)
    , mData(aData)
    , mOrigin(aOrigin)
    , mChannel(aChannel)
    , mAppId(aAppId)
    , mIsInBrowserElement(aIsInBrowserElement)
    , mPrivateBrowsing(aPrivateBrowsing)
  {
    MOZ_ASSERT(aParent);
    MOZ_COUNT_CTOR(PostMessageData);

    // We need to keep the array alive for the life-time of this
    // PostMessageData.
    if (!aData.blobsParent().IsEmpty()) {
      mFiles.SetCapacity(aData.blobsParent().Length());

      for (uint32_t i = 0, len = aData.blobsParent().Length(); i < len; ++i) {
        nsRefPtr<FileImpl> impl =
          static_cast<BlobParent*>(aData.blobsParent()[i])->GetBlobImpl();
       MOZ_ASSERT(impl);
       mFiles.AppendElement(impl);
      }
    }
  }

  ~PostMessageData()
  {
    MOZ_COUNT_DTOR(PostMessageData);
  }

  BroadcastChannelParent* mParent;
  const ClonedMessageData& mData;
  nsTArray<nsRefPtr<FileImpl>> mFiles;
  const nsString mOrigin;
  const nsString mChannel;
  uint64_t mAppId;
  bool mIsInBrowserElement;
  bool mPrivateBrowsing;
};

PLDHashOperator
PostMessageEnumerator(nsPtrHashKey<BroadcastChannelParent>* aKey, void* aPtr)
{
  AssertIsOnBackgroundThread();

  auto* data = static_cast<PostMessageData*>(aPtr);
  BroadcastChannelParent* parent = aKey->GetKey();
  MOZ_ASSERT(parent);

  if (parent != data->mParent) {
    parent->CheckAndDeliver(data->mData, data->mOrigin, data->mAppId,
                            data->mIsInBrowserElement, data->mChannel,
                            data->mPrivateBrowsing);
  }

  return PL_DHASH_NEXT;
}

} // anonymous namespace

void
BroadcastChannelService::PostMessage(BroadcastChannelParent* aParent,
                                     const ClonedMessageData& aData,
                                     const nsAString& aOrigin,
                                     uint64_t aAppId,
                                     bool aIsInBrowserElement,
                                     const nsAString& aChannel,
                                     bool aPrivateBrowsing)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(mAgents.Contains(aParent));

  PostMessageData data(aParent, aData, aOrigin, aAppId, aIsInBrowserElement,
                       aChannel, aPrivateBrowsing);
  mAgents.EnumerateEntries(PostMessageEnumerator, &data);
}

} // dom namespace
} // mozilla namespace
