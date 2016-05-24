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
BroadcastChannelService::RegisterActor(BroadcastChannelParent* aParent,
                                       const nsAString& aOriginChannelKey)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);

  nsTArray<BroadcastChannelParent*>* parents;
  if (!mAgents.Get(aOriginChannelKey, &parents)) {
    parents = new nsTArray<BroadcastChannelParent*>();
    mAgents.Put(aOriginChannelKey, parents);
  }

  MOZ_ASSERT(!parents->Contains(aParent));
  parents->AppendElement(aParent);
}

void
BroadcastChannelService::UnregisterActor(BroadcastChannelParent* aParent,
                                         const nsAString& aOriginChannelKey)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);

  nsTArray<BroadcastChannelParent*>* parents;
  if (!mAgents.Get(aOriginChannelKey, &parents)) {
    MOZ_CRASH("Invalid state");
  }

  parents->RemoveElement(aParent);
  if (parents->IsEmpty()) {
    mAgents.Remove(aOriginChannelKey);
  }
}

void
BroadcastChannelService::PostMessage(BroadcastChannelParent* aParent,
                                     const ClonedMessageData& aData,
                                     const nsAString& aOriginChannelKey)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);

  nsTArray<BroadcastChannelParent*>* parents;
  if (!mAgents.Get(aOriginChannelKey, &parents)) {
    MOZ_CRASH("Invalid state");
  }

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

  for (uint32_t i = 0; i < parents->Length(); ++i) {
    BroadcastChannelParent* parent = parents->ElementAt(i);
    MOZ_ASSERT(parent);

    if (parent != aParent) {
      parent->Deliver(aData);
    }
  }
}

} // namespace dom
} // namespace mozilla
