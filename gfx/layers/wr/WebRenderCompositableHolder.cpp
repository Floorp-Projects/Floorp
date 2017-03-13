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
  MOZ_ASSERT(mWebRenderTextureHosts.empty());
}

void
WebRenderCompositableHolder::Destroy()
{
  while (!mWebRenderTextureHosts.empty()) {
    mWebRenderTextureHosts.pop();
  }
}

void
WebRenderCompositableHolder::HoldExternalImage(const wr::Epoch& aEpoch, WebRenderTextureHost* aTexture)
{
  MOZ_ASSERT(aTexture);
  // Hold WebRenderTextureHost until end of its usage on RenderThread
  mWebRenderTextureHosts.push(ForwardingTextureHosts(aEpoch, aTexture));
}

void
WebRenderCompositableHolder::Update(const wr::Epoch& aEpoch)
{
  if (mWebRenderTextureHosts.empty()) {
    return;
  }
  while (!mWebRenderTextureHosts.empty()) {
    if (aEpoch <= mWebRenderTextureHosts.front().mEpoch) {
      break;
    }
    mWebRenderTextureHosts.pop();
  }
}

} // namespace layers
} // namespace mozilla
