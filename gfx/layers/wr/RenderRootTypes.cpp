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
    IPC::MessageWriter* aWriter, IProtocol* aActor, paramType&& aParam) {
  WriteIPDLParam(aWriter, aActor, aParam.mIdNamespace);
  WriteIPDLParam(aWriter, aActor, aParam.mRect);
  WriteIPDLParam(aWriter, aActor, aParam.mCommands);
  WriteIPDLParam(aWriter, aActor, std::move(aParam.mDLItems));
  WriteIPDLParam(aWriter, aActor, std::move(aParam.mDLCache));
  WriteIPDLParam(aWriter, aActor, std::move(aParam.mDLSpatialTree));
  WriteIPDLParam(aWriter, aActor, aParam.mDLDesc);
  WriteIPDLParam(aWriter, aActor, aParam.mResourceUpdates);
  WriteIPDLParam(aWriter, aActor, aParam.mSmallShmems);
  WriteIPDLParam(aWriter, aActor, std::move(aParam.mLargeShmems));
  WriteIPDLParam(aWriter, aActor, aParam.mScrollData);
}

bool IPDLParamTraits<mozilla::layers::DisplayListData>::Read(
    IPC::MessageReader* aReader, IProtocol* aActor, paramType* aResult) {
  if (ReadIPDLParam(aReader, aActor, &aResult->mIdNamespace) &&
      ReadIPDLParam(aReader, aActor, &aResult->mRect) &&
      ReadIPDLParam(aReader, aActor, &aResult->mCommands) &&
      ReadIPDLParam(aReader, aActor, &aResult->mDLItems) &&
      ReadIPDLParam(aReader, aActor, &aResult->mDLCache) &&
      ReadIPDLParam(aReader, aActor, &aResult->mDLSpatialTree) &&
      ReadIPDLParam(aReader, aActor, &aResult->mDLDesc) &&
      ReadIPDLParam(aReader, aActor, &aResult->mResourceUpdates) &&
      ReadIPDLParam(aReader, aActor, &aResult->mSmallShmems) &&
      ReadIPDLParam(aReader, aActor, &aResult->mLargeShmems) &&
      ReadIPDLParam(aReader, aActor, &aResult->mScrollData)) {
    return true;
  }
  return false;
}

void WriteScrollUpdates(IPC::MessageWriter* aWriter, IProtocol* aActor,
                        layers::ScrollUpdatesMap& aParam) {
  // ICK: we need to manually serialize this map because
  // nsDataHashTable doesn't support it (and other maps cause other issues)
  WriteIPDLParam(aWriter, aActor, aParam.Count());
  for (auto it = aParam.ConstIter(); !it.Done(); it.Next()) {
    WriteIPDLParam(aWriter, aActor, it.Key());
    WriteIPDLParam(aWriter, aActor, it.Data());
  }
}

bool ReadScrollUpdates(IPC::MessageReader* aReader, IProtocol* aActor,
                       layers::ScrollUpdatesMap* aResult) {
  // Manually deserialize mScrollUpdates as a stream of K,V pairs
  uint32_t count;
  if (!ReadIPDLParam(aReader, aActor, &count)) {
    return false;
  }

  layers::ScrollUpdatesMap map(count);
  for (size_t i = 0; i < count; ++i) {
    layers::ScrollableLayerGuid::ViewID key;
    nsTArray<mozilla::ScrollPositionUpdate> data;
    if (!ReadIPDLParam(aReader, aActor, &key) ||
        !ReadIPDLParam(aReader, aActor, &data)) {
      return false;
    }
    map.InsertOrUpdate(key, std::move(data));
  }

  MOZ_RELEASE_ASSERT(map.Count() == count);
  *aResult = std::move(map);
  return true;
}

void IPDLParamTraits<mozilla::layers::TransactionData>::Write(
    IPC::MessageWriter* aWriter, IProtocol* aActor, paramType&& aParam) {
  WriteIPDLParam(aWriter, aActor, aParam.mIdNamespace);
  WriteIPDLParam(aWriter, aActor, aParam.mCommands);
  WriteIPDLParam(aWriter, aActor, aParam.mResourceUpdates);
  WriteIPDLParam(aWriter, aActor, aParam.mSmallShmems);
  WriteIPDLParam(aWriter, aActor, std::move(aParam.mLargeShmems));
  WriteScrollUpdates(aWriter, aActor, aParam.mScrollUpdates);
  WriteIPDLParam(aWriter, aActor, aParam.mPaintSequenceNumber);
}

bool IPDLParamTraits<mozilla::layers::TransactionData>::Read(
    IPC::MessageReader* aReader, IProtocol* aActor, paramType* aResult) {
  if (ReadIPDLParam(aReader, aActor, &aResult->mIdNamespace) &&
      ReadIPDLParam(aReader, aActor, &aResult->mCommands) &&
      ReadIPDLParam(aReader, aActor, &aResult->mResourceUpdates) &&
      ReadIPDLParam(aReader, aActor, &aResult->mSmallShmems) &&
      ReadIPDLParam(aReader, aActor, &aResult->mLargeShmems) &&
      ReadScrollUpdates(aReader, aActor, &aResult->mScrollUpdates) &&
      ReadIPDLParam(aReader, aActor, &aResult->mPaintSequenceNumber)) {
    return true;
  }
  return false;
}

}  // namespace ipc
}  // namespace mozilla
