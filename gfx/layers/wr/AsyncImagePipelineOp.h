/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_AsyncImagePipelineOp_H
#define MOZILLA_GFX_AsyncImagePipelineOp_H

#include <queue>

#include "mozilla/layers/TextureHost.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "Units.h"

namespace mozilla {

namespace wr {
struct Transaction;
}  // namespace wr

namespace layers {

class AsyncImagePipelineManager;
class TextureHost;

class AsyncImagePipelineOp {
 public:
  enum class Tag {
    ApplyAsyncImageForPipeline,
  };

  const Tag mTag;

  AsyncImagePipelineManager* const mAsyncImageManager;
  const wr::PipelineId mPipelineId;
  const CompositableTextureHostRef mTextureHost;

 private:
  AsyncImagePipelineOp(const Tag aTag,
                       AsyncImagePipelineManager* aAsyncImageManager,
                       const wr::PipelineId& aPipelineId,
                       TextureHost* aTextureHost)
      : mTag(aTag),
        mAsyncImageManager(aAsyncImageManager),
        mPipelineId(aPipelineId),
        mTextureHost(aTextureHost) {
    MOZ_ASSERT(mTag == Tag::ApplyAsyncImageForPipeline);
  }

 public:
  static AsyncImagePipelineOp ApplyAsyncImageForPipeline(
      AsyncImagePipelineManager* aAsyncImageManager,
      const wr::PipelineId& aPipelineId, TextureHost* aTextureHost) {
    return AsyncImagePipelineOp(Tag::ApplyAsyncImageForPipeline,
                                aAsyncImageManager, aPipelineId, aTextureHost);
  }
};

struct AsyncImagePipelineOps {
  explicit AsyncImagePipelineOps(wr::Transaction* aTransaction)
      : mTransaction(aTransaction) {}

  void HandleOps(wr::TransactionBuilder& aTxn);

  wr::Transaction* const mTransaction;
  std::queue<AsyncImagePipelineOp> mList;
};

}  // namespace layers
}  // namespace mozilla

#endif  // MOZILLA_GFX_AsyncImagePipelineOp_H
