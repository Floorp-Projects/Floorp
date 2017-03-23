/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderCompositableHolder.h"

#include "CompositableHost.h"
#include "mozilla/layers/WebRenderImageHost.h"
#include "mozilla/layers/WebRenderTextureHost.h"

namespace mozilla {

using namespace gfx;

namespace layers {

WebRenderCompositableHolder::WebRenderCompositableHolder()
{
  MOZ_COUNT_CTOR(WebRenderCompositableHolder);
}

WebRenderCompositableHolder::~WebRenderCompositableHolder()
{
  MOZ_COUNT_DTOR(WebRenderCompositableHolder);
  MOZ_ASSERT(mPipelineTexturesHolders.IsEmpty());
}

void
WebRenderCompositableHolder::AddPipeline(const wr::PipelineId& aPipelineId)
{
  uint64_t id = wr::AsUint64(aPipelineId);

  MOZ_ASSERT(!mPipelineTexturesHolders.Get(id));
  PipelineTexturesHolder* holder = new PipelineTexturesHolder();
  mPipelineTexturesHolders.Put(id, holder);
}

void
WebRenderCompositableHolder::RemovePipeline(const wr::PipelineId& aPipelineId)
{
  uint64_t id = wr::AsUint64(aPipelineId);
  if (mPipelineTexturesHolders.Get(id)) {
    mPipelineTexturesHolders.Remove(id);
  }
}

void
WebRenderCompositableHolder::HoldExternalImage(const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch, WebRenderTextureHost* aTexture)
{
  MOZ_ASSERT(aTexture);
  PipelineTexturesHolder* holder = mPipelineTexturesHolders.Get(wr::AsUint64(aPipelineId));
  MOZ_ASSERT(holder);
  if (!holder) {
    return;
  }
  // Hold WebRenderTextureHost until end of its usage on RenderThread
  holder->mTextureHosts.push(ForwardingTextureHost(aEpoch, aTexture));
}

void
WebRenderCompositableHolder::Update(const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch)
{
  PipelineTexturesHolder* holder = mPipelineTexturesHolders.Get(wr::AsUint64(aPipelineId));
  if (!holder || holder->mTextureHosts.empty()) {
    return;
  }

  while (!holder->mTextureHosts.empty()) {
    if (aEpoch <= holder->mTextureHosts.front().mEpoch) {
      break;
    }
    holder->mTextureHosts.pop();
  }
}

} // namespace layers
} // namespace mozilla
