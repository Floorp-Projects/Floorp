/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderCompositableHolder.h"

#include "CompositableHost.h"
#include "mozilla/layers/WebRenderImageHost.h"
#include "mozilla/layers/WebRenderTextureHost.h"
#include "mozilla/webrender/WebRenderAPI.h"

namespace mozilla {

using namespace gfx;

namespace layers {

WebRenderCompositableHolder::WebRenderCompositableHolder(uint32_t aIdNamespace)
 : mIdNamespace(aIdNamespace)
 , mResourceId(0)
{
  MOZ_COUNT_CTOR(WebRenderCompositableHolder);
}

WebRenderCompositableHolder::~WebRenderCompositableHolder()
{
  MOZ_COUNT_DTOR(WebRenderCompositableHolder);
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
WebRenderCompositableHolder::RemovePipeline(const wr::PipelineId& aPipelineId, const wr::Epoch& aEpoch)
{
  PipelineTexturesHolder* holder = mPipelineTexturesHolders.Get(wr::AsUint64(aPipelineId));
  MOZ_ASSERT(holder);
  if (!holder) {
    return;
  }
  MOZ_ASSERT(holder->mDestroyedEpoch.isNothing());
  holder->mDestroyedEpoch = Some(aEpoch);
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
  if (!holder) {
    return;
  }

  // Remove Pipeline
  if (holder->mDestroyedEpoch.isSome() && holder->mDestroyedEpoch.ref() <= aEpoch) {
    mPipelineTexturesHolders.Remove(wr::AsUint64(aPipelineId));
    return;
  }

  // Release TextureHosts based on Epoch
  while (!holder->mTextureHosts.empty()) {
    if (aEpoch <= holder->mTextureHosts.front().mEpoch) {
      break;
    }
    holder->mTextureHosts.pop();
  }
}

} // namespace layers
} // namespace mozilla
