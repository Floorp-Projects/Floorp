/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayersTypes.h"

#ifdef MOZ_WIDGET_GONK
#include <ui/GraphicBuffer.h>
#endif

namespace mozilla {
namespace layers {

LayerRenderState::LayerRenderState()
  : mFlags(LayerRenderStateFlags::LAYER_RENDER_STATE_DEFAULT)
  , mHasOwnOffset(false)
#ifdef MOZ_WIDGET_GONK
  , mSurface(nullptr)
  , mOverlayId(INVALID_OVERLAY)
  , mTexture(nullptr)
#endif
{
}

LayerRenderState::LayerRenderState(const LayerRenderState& aOther)
  : mFlags(aOther.mFlags)
  , mHasOwnOffset(aOther.mHasOwnOffset)
  , mOffset(aOther.mOffset)
#ifdef MOZ_WIDGET_GONK
  , mSurface(aOther.mSurface)
  , mOverlayId(aOther.mOverlayId)
  , mSize(aOther.mSize)
  , mTexture(aOther.mTexture)
#endif
{
}

LayerRenderState::~LayerRenderState()
{
}

#ifdef MOZ_WIDGET_GONK
LayerRenderState::LayerRenderState(android::GraphicBuffer* aSurface,
                                   const gfx::IntSize& aSize,
                                   LayerRenderStateFlags aFlags,
                                   TextureHost* aTexture)
  : mFlags(aFlags)
  , mHasOwnOffset(false)
  , mSurface(aSurface)
  , mOverlayId(INVALID_OVERLAY)
  , mSize(aSize)
  , mTexture(aTexture)
{}
#endif

} // namespace
} // namespace
