/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderRootTypes.h"
#include "mozilla/layers/WebRenderMessageUtils.h"
#include "mozilla/layers/WebRenderBridgeChild.h"

namespace mozilla {
namespace ipc {

void IPDLParamTraits<mozilla::layers::DisplayListData>::Write(
    IPC::Message* aMsg, IProtocol* aActor, paramType&& aParam) {
  WriteIPDLParam(aMsg, aActor, aParam.mIdNamespace);
  WriteIPDLParam(aMsg, aActor, aParam.mRect);
  WriteIPDLParam(aMsg, aActor, aParam.mCommands);
  WriteIPDLParam(aMsg, aActor, std::move(aParam.mDLItems));
  WriteIPDLParam(aMsg, aActor, std::move(aParam.mDLCache));
  WriteIPDLParam(aMsg, aActor, std::move(aParam.mDLSpatialTree));
  WriteIPDLParam(aMsg, aActor, aParam.mDLDesc);
  WriteIPDLParam(aMsg, aActor, aParam.mRemotePipelineIds);
  WriteIPDLParam(aMsg, aActor, aParam.mResourceUpdates);
  WriteIPDLParam(aMsg, aActor, aParam.mSmallShmems);
  WriteIPDLParam(aMsg, aActor, std::move(aParam.mLargeShmems));
  WriteIPDLParam(aMsg, aActor, aParam.mScrollData);
}

bool IPDLParamTraits<mozilla::layers::DisplayListData>::Read(
    const IPC::Message* aMsg, PickleIterator* aIter, IProtocol* aActor,
    paramType* aResult) {
  if (ReadIPDLParam(aMsg, aIter, aActor, &aResult->mIdNamespace) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mRect) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mCommands) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mDLItems) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mDLCache) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mDLSpatialTree) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mDLDesc) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mRemotePipelineIds) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mResourceUpdates) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mSmallShmems) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mLargeShmems) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mScrollData)) {
    return true;
  }
  return false;
}

void WriteScrollUpdates(IPC::Message* aMsg, IProtocol* aActor,
                        layers::ScrollUpdatesMap& aParam) {
  // ICK: we need to manually serialize this map because
  // nsDataHashTable doesn't support it (and other maps cause other issues)
  WriteIPDLParam(aMsg, aActor, aParam.Count());
  for (auto it = aParam.ConstIter(); !it.Done(); it.Next()) {
    WriteIPDLParam(aMsg, aActor, it.Key());
    WriteIPDLParam(aMsg, aActor, it.Data());
  }
}

bool ReadScrollUpdates(const IPC::Message* aMsg, PickleIterator* aIter,
                       IProtocol* aActor, layers::ScrollUpdatesMap* aResult) {
  // Manually deserialize mScrollUpdates as a stream of K,V pairs
  uint32_t count;
  if (!ReadIPDLParam(aMsg, aIter, aActor, &count)) {
    return false;
  }

  layers::ScrollUpdatesMap map(count);
  for (size_t i = 0; i < count; ++i) {
    layers::ScrollableLayerGuid::ViewID key;
    nsTArray<mozilla::ScrollPositionUpdate> data;
    if (!ReadIPDLParam(aMsg, aIter, aActor, &key) ||
        !ReadIPDLParam(aMsg, aIter, aActor, &data)) {
      return false;
    }
    map.InsertOrUpdate(key, std::move(data));
  }

  MOZ_RELEASE_ASSERT(map.Count() == count);
  *aResult = std::move(map);
  return true;
}

void IPDLParamTraits<mozilla::layers::TransactionData>::Write(
    IPC::Message* aMsg, IProtocol* aActor, paramType&& aParam) {
  WriteIPDLParam(aMsg, aActor, aParam.mIdNamespace);
  WriteIPDLParam(aMsg, aActor, aParam.mCommands);
  WriteIPDLParam(aMsg, aActor, aParam.mResourceUpdates);
  WriteIPDLParam(aMsg, aActor, aParam.mSmallShmems);
  WriteIPDLParam(aMsg, aActor, std::move(aParam.mLargeShmems));
  WriteScrollUpdates(aMsg, aActor, aParam.mScrollUpdates);
  WriteIPDLParam(aMsg, aActor, aParam.mPaintSequenceNumber);
}

bool IPDLParamTraits<mozilla::layers::TransactionData>::Read(
    const IPC::Message* aMsg, PickleIterator* aIter, IProtocol* aActor,
    paramType* aResult) {
  if (ReadIPDLParam(aMsg, aIter, aActor, &aResult->mIdNamespace) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mCommands) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mResourceUpdates) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mSmallShmems) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mLargeShmems) &&
      ReadScrollUpdates(aMsg, aIter, aActor, &aResult->mScrollUpdates) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mPaintSequenceNumber)) {
    return true;
  }
  return false;
}

}  // namespace ipc
}  // namespace mozilla
