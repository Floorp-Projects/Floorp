/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BroadcastChannelService.h"
#include "BroadcastChannelParent.h"
#include "mozilla/dom/BlobImpl.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/ipc/BackgroundParent.h"

#ifdef XP_WIN
#  undef PostMessage
#endif

namespace mozilla {

using namespace ipc;

namespace dom {

namespace {

BroadcastChannelService* sInstance = nullptr;

ClonedMessageData CloneClonedMessageData(const ClonedMessageData& aOther) {
  auto cloneData = SerializedStructuredCloneBuffer{};
  cloneData.data.initScope(aOther.data().data.scope());
  const bool res = cloneData.data.Append(aOther.data().data);
  MOZ_RELEASE_ASSERT(res, "out of memory");
  return {std::move(cloneData), aOther.blobs(), aOther.inputStreams(),
          aOther.identifiers()};
}

MessageData CloneMessageData(const MessageData& aOther) {
  switch (aOther.data().type()) {
    case MessageDataType::TClonedMessageData:
      return {aOther.agentClusterId(),
              CloneClonedMessageData(aOther.data().get_ClonedMessageData())};
    case MessageDataType::TRefMessageData:
      return {aOther.agentClusterId(), aOther.data().get_RefMessageData()};
    default:
      MOZ_CRASH("Unexpected MessageDataType type");
  }
}

}  // namespace

BroadcastChannelService::BroadcastChannelService() {
  AssertIsOnBackgroundThread();

  // sInstance is a raw BroadcastChannelService*.
  MOZ_ASSERT(!sInstance);
  sInstance = this;
}

BroadcastChannelService::~BroadcastChannelService() {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(sInstance == this);
  MOZ_ASSERT(mAgents.Count() == 0);

  sInstance = nullptr;
}

// static
already_AddRefed<BroadcastChannelService>
BroadcastChannelService::GetOrCreate() {
  AssertIsOnBackgroundThread();

  RefPtr<BroadcastChannelService> instance = sInstance;
  if (!instance) {
    instance = new BroadcastChannelService();
  }
  return instance.forget();
}

void BroadcastChannelService::RegisterActor(
    BroadcastChannelParent* aParent, const nsAString& aOriginChannelKey) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);

  auto* const parents = mAgents.GetOrInsertNew(aOriginChannelKey);

  MOZ_ASSERT(!parents->Contains(aParent));
  parents->AppendElement(aParent);
}

void BroadcastChannelService::UnregisterActor(
    BroadcastChannelParent* aParent, const nsAString& aOriginChannelKey) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);

  if (auto entry = mAgents.Lookup(aOriginChannelKey)) {
    entry.Data()->RemoveElement(aParent);
    // remove the entry if the array is now empty
    if (entry.Data()->IsEmpty()) {
      entry.Remove();
    }
  } else {
    MOZ_CRASH("Invalid state");
  }
}

void BroadcastChannelService::PostMessage(BroadcastChannelParent* aParent,
                                          const MessageData& aData,
                                          const nsAString& aOriginChannelKey) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aParent);

  nsTArray<BroadcastChannelParent*>* parents;
  if (!mAgents.Get(aOriginChannelKey, &parents)) {
    MOZ_CRASH("Invalid state");
  }

  // We need to keep the array alive for the life-time of this operation.
  nsTArray<RefPtr<BlobImpl>> blobImpls;
  if (aData.data().type() == MessageDataType::TClonedMessageData) {
    const nsTArray<IPCBlob>& blobs =
        aData.data().get_ClonedMessageData().blobs();
    if (!blobs.IsEmpty()) {
      blobImpls.SetCapacity(blobs.Length());

      for (uint32_t i = 0, len = blobs.Length(); i < len; ++i) {
        RefPtr<BlobImpl> impl = IPCBlobUtils::Deserialize(blobs[i]);

        MOZ_ASSERT(impl);
        blobImpls.AppendElement(impl);
      }
    }
  }

  uint32_t selectedActorsOnSamePid = 0;

  // For each parent actor, we notify the message.
  for (uint32_t i = 0; i < parents->Length(); ++i) {
    BroadcastChannelParent* parent = parents->ElementAt(i);
    MOZ_ASSERT(parent);

    if (parent == aParent) {
      continue;
    }

    if (parent->OtherPid() == aParent->OtherPid()) {
      ++selectedActorsOnSamePid;
    }

    // We need to have a copy of the data for this parent.
    MessageData newData = CloneMessageData(aData);
    MOZ_ASSERT(newData.data().type() == aData.data().type());

    if (!blobImpls.IsEmpty()) {
      nsTArray<IPCBlob>& newBlobImpls =
          newData.data().get_ClonedMessageData().blobs();
      MOZ_ASSERT(blobImpls.Length() == newBlobImpls.Length());

      // Serialize Blob objects for this message.
      for (uint32_t i = 0, len = blobImpls.Length(); i < len; ++i) {
        nsresult rv = IPCBlobUtils::Serialize(blobImpls[i], newBlobImpls[i]);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return;
        }
      }
    }

    Unused << parent->SendNotify(newData);
  }

  // If this is a refMessageData, we need to know when it can be released.
  if (aData.data().type() == MessageDataType::TRefMessageData) {
    Unused << aParent->SendRefMessageDelivered(
        aData.data().get_RefMessageData().uuid(), selectedActorsOnSamePid);
  }
}

}  // namespace dom
}  // namespace mozilla
