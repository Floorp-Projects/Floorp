/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PersistentBufferProvider.h"

#include "Layers.h"
#include "mozilla/gfx/Logging.h"
#include "pratom.h"
#include "gfxPlatform.h"

namespace mozilla {

using namespace gfx;

namespace layers {

PersistentBufferProviderBasic::PersistentBufferProviderBasic(LayerManager* aManager, gfx::IntSize aSize,
                                                             gfx::SurfaceFormat aFormat, gfx::BackendType aBackend)
{
  mDrawTarget = gfxPlatform::GetPlatform()->CreateDrawTargetForBackend(aBackend, aSize, aFormat);
}

} // namespace layers
} // namespace mozilla