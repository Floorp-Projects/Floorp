/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_RENDERROOTTYPES_H
#define GFX_RENDERROOTTYPES_H

#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "mozilla/layers/WebRenderMessages.h"
#include "mozilla/layers/WebRenderScrollData.h"

namespace mozilla {

namespace layers {

struct DisplayListData {
  wr::IdNamespace mIdNamespace;
  LayoutDeviceRect mRect;
  nsTArray<WebRenderParentCommand> mCommands;
  Maybe<mozilla::ipc::ByteBuf> mDLItems;
  Maybe<mozilla::ipc::ByteBuf> mDLCache;
  Maybe<mozilla::ipc::ByteBuf> mDLSpatialTree;
  wr::BuiltDisplayListDescriptor mDLDesc;
  nsTArray<OpUpdateResource> mResourceUpdates;
  nsTArray<RefCountedShmem> mSmallShmems;
  nsTArray<mozilla::ipc::Shmem> mLargeShmems;
  Maybe<WebRenderScrollData> mScrollData;
};

struct TransactionData {
  wr::IdNamespace mIdNamespace;
  nsTArray<WebRenderParentCommand> mCommands;
  nsTArray<OpUpdateResource> mResourceUpdates;
  nsTArray<RefCountedShmem> mSmallShmems;
  nsTArray<mozilla::ipc::Shmem> mLargeShmems;
  ScrollUpdatesMap mScrollUpdates;
  uint32_t mPaintSequenceNumber;
};

typedef Maybe<TransactionData> MaybeTransactionData;

}  // namespace layers

namespace ipc {

template <>
struct IPDLParamTraits<mozilla::layers::DisplayListData> {
  typedef mozilla::layers::DisplayListData paramType;

  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    paramType&& aParam);

  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   paramType* aResult);
};

template <>
struct IPDLParamTraits<mozilla::layers::TransactionData> {
  typedef mozilla::layers::TransactionData paramType;

  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    paramType&& aParam);

  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   paramType* aResult);
};

}  // namespace ipc
}  // namespace mozilla

#endif /* GFX_RENDERROOTTYPES_H */
