/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayersTypes.h"

namespace mozilla {
namespace layers {

LayerRenderState::LayerRenderState()
  : mFlags(LayerRenderStateFlags::LAYER_RENDER_STATE_DEFAULT)
  , mHasOwnOffset(false)
{
}

LayerRenderState::LayerRenderState(const LayerRenderState& aOther)
  : mFlags(aOther.mFlags)
  , mHasOwnOffset(aOther.mHasOwnOffset)
  , mOffset(aOther.mOffset)
{
}

LayerRenderState::~LayerRenderState()
{
}

} // namespace layers
} // namespace mozilla
