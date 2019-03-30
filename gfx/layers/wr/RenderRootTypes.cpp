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

void IPDLParamTraits<mozilla::layers::RenderRootDisplayListData>::Write(
    IPC::Message* aMsg, IProtocol* aActor, paramType&& aParam) {
  WriteIPDLParam(aMsg, aActor, aParam.mRenderRoot);
  WriteIPDLParam(aMsg, aActor, aParam.mRect);
  WriteIPDLParam(aMsg, aActor, aParam.mCommands);
  WriteIPDLParam(aMsg, aActor, aParam.mContentSize);
  WriteIPDLParam(aMsg, aActor, std::move(aParam.mDL));
  WriteIPDLParam(aMsg, aActor, aParam.mDLDesc);
  WriteIPDLParam(aMsg, aActor, aParam.mResourceUpdates);
  WriteIPDLParam(aMsg, aActor, aParam.mSmallShmems);
  WriteIPDLParam(aMsg, aActor, std::move(aParam.mLargeShmems));
  WriteIPDLParam(aMsg, aActor, aParam.mScrollData);
}

bool IPDLParamTraits<mozilla::layers::RenderRootDisplayListData>::Read(
    const IPC::Message* aMsg, PickleIterator* aIter, IProtocol* aActor,
    paramType* aResult) {
  if (ReadIPDLParam(aMsg, aIter, aActor, &aResult->mRenderRoot) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mRect) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mCommands) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mContentSize) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mDL) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mDLDesc) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mResourceUpdates) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mSmallShmems) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mLargeShmems) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mScrollData)) {
    return true;
  }
  return false;
}

void IPDLParamTraits<mozilla::layers::RenderRootUpdates>::Write(
    IPC::Message* aMsg, IProtocol* aActor, paramType&& aParam) {
  WriteIPDLParam(aMsg, aActor, aParam.mRenderRoot);
  WriteIPDLParam(aMsg, aActor, aParam.mCommands);
  WriteIPDLParam(aMsg, aActor, aParam.mResourceUpdates);
  WriteIPDLParam(aMsg, aActor, aParam.mSmallShmems);
  WriteIPDLParam(aMsg, aActor, std::move(aParam.mLargeShmems));
  WriteIPDLParam(aMsg, aActor, aParam.mScrollUpdates);
}

bool IPDLParamTraits<mozilla::layers::RenderRootUpdates>::Read(
    const IPC::Message* aMsg, PickleIterator* aIter, IProtocol* aActor,
    paramType* aResult) {
  if (ReadIPDLParam(aMsg, aIter, aActor, &aResult->mRenderRoot) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mCommands) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mResourceUpdates) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mSmallShmems) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mLargeShmems) &&
      ReadIPDLParam(aMsg, aIter, aActor, &aResult->mScrollUpdates)) {
    return true;
  }
  return false;
}

}  // namespace ipc
}  // namespace mozilla
