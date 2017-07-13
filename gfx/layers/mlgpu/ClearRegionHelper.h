/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_mlgpu_ClearRegionHelper_h
#define mozilla_gfx_layers_mlgpu_ClearRegionHelper_h

#include "SharedBufferMLGPU.h"

namespace mozilla {
namespace layers {

// This is a helper class for issuing a clear operation based on either
// a shader or an API like ClearView. It also works when the depth
// buffer is enabled.
struct ClearRegionHelper
{
  // If using ClearView-based clears, we fill mRegions.
  nsTArray<gfx::IntRect> mRects;

  // If using shader-based clears, we fill these buffers.
  VertexBufferSection mInput;
  ConstantBufferSection mVSBuffer;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_gfx_layers_mlgpu_ClearRegionHelper_h
