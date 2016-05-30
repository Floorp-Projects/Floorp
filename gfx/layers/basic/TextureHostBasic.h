
/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTUREHOSTBASIC_H_
#define MOZILLA_GFX_TEXTUREHOSTBASIC_H_

#include "CompositableHost.h"
#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/gfx/2D.h"

namespace mozilla {
namespace layers {

/**
 * A texture source interface that can be used by the software Compositor.
 */
class TextureSourceBasic
{
public:
  TextureSourceBasic() : mFromYCBCR(false) {}
  virtual ~TextureSourceBasic() {}
  virtual gfx::SourceSurface* GetSurface(gfx::DrawTarget* aTarget) = 0;
  bool mFromYCBCR; // we to track sources from YCBCR so we can use a less accurate fast path for video
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_GFX_TEXTUREHOSTBASIC_H_
