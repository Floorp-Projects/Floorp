/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_mlgpu_MLGDeviceTypes_h
#define mozilla_gfx_layers_mlgpu_MLGDeviceTypes_h

#include "mozilla/TypedEnumBits.h"

namespace mozilla {
namespace layers {

enum class MLGUsage
{
  // GPU read-only, CPU write once on creation and read/write never.
  Immutable,

  // GPU read-only, CPU write-only. Must be mapped with WRITE_DISCARD.
  Dynamic,

  // GPU read/write-only, no CPU access.
  Default,

  // GPU->CPU transfer, and read from the CPU.
  Staging
};

enum class MLGDepthTestMode
{
  Disabled,
  Write,
  ReadOnly,
  AlwaysWrite,
  MaxModes
};

enum class MLGBufferType : uint32_t
{
  Vertex,
  Constant
};

enum class SamplerMode
{
  // Linear filter, clamped to border.
  LinearClamp = 0,
  // Linear filter, clamped to transparent pixels.
  LinearClampToZero,
  // Point filter, clamped to border.
  Point,
  MaxModes
};

enum class MLGBlendState
{
  Copy = 0,
  Over,
  OverAndPremultiply,
  Min,
  ComponentAlpha,
  MaxStates
};

enum class MLGPrimitiveTopology
{
  Unknown = 0,
  TriangleStrip = 1,
  TriangleList = 2,
  UnitQuad = 3,
  UnitTriangle = 4
};

struct MLGMappedResource
{
  uint8_t* mData;
  uint32_t mStride;
};

enum class MLGMapType
{
  READ = 0,
  WRITE,
  READ_WRITE,
  WRITE_DISCARD
};

enum class MLGTextureFlags
{
  None,
  ShaderResource,
  RenderTarget
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(MLGTextureFlags);

enum class MLGRenderTargetFlags : uint32_t
{
  Default = 0,
  ZBuffer = (1 << 0)
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(MLGRenderTargetFlags);

// NVIDIA drivers crash when we supply too many rects to ClearView - it
// seems to cause a stack overflow >= 20 rects. We cap to 12 for now.
static const size_t kMaxClearViewRects = 12;

} // namespace layers
} // namespace mozilla

#endif // mozilla_gfx_layers_mlgpu_MLGDeviceTypes_h
