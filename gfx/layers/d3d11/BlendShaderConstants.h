/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_LAYERS_D3D11_BLENDSHADERCONSTANTS_H_
#define MOZILLA_GFX_LAYERS_D3D11_BLENDSHADERCONSTANTS_H_

// These constants are shared between CompositorD3D11 and the blend pixel shader.
#define PS_LAYER_RGB          0
#define PS_LAYER_RGBA         1
#define PS_LAYER_YCBCR        2
#define PS_LAYER_COLOR        3

// These must be in the same order as the Mask enum.
#define PS_MASK_NONE         0
#define PS_MASK              1

// These must be in the same order as CompositionOp.
#define PS_BLEND_MULTIPLY    0
#define PS_BLEND_SCREEN      1
#define PS_BLEND_OVERLAY     2
#define PS_BLEND_DARKEN      3
#define PS_BLEND_LIGHTEN     4
#define PS_BLEND_COLOR_DODGE 5
#define PS_BLEND_COLOR_BURN  6
#define PS_BLEND_HARD_LIGHT  7
#define PS_BLEND_SOFT_LIGHT  8
#define PS_BLEND_DIFFERENCE  9
#define PS_BLEND_EXCLUSION   10
#define PS_BLEND_HUE         11
#define PS_BLEND_SATURATION  12
#define PS_BLEND_COLOR       13
#define PS_BLEND_LUMINOSITY  14

#if defined(__cplusplus)
namespace mozilla {
namespace layers {

static inline int
BlendOpToShaderConstant(gfx::CompositionOp aOp) {
  return int(aOp) - int(gfx::CompositionOp::OP_MULTIPLY);
}

} // namespace layers
} // namespace mozilla

// Sanity checks.
namespace {
static inline void BlendShaderConstantAsserts() {
  static_assert(PS_MASK_NONE == int(mozilla::layers::MaskType::MaskNone), "shader constant is out of sync");
  static_assert(PS_MASK == int(mozilla::layers::MaskType::Mask), "shader constant is out of sync");
  static_assert(int(mozilla::gfx::CompositionOp::OP_LUMINOSITY) - int(mozilla::gfx::CompositionOp::OP_MULTIPLY) == 14,
                "shader constants are out of sync");
}
} // anonymous namespace
#endif

#endif // MOZILLA_GFX_LAYERS_D3D11_BLENDSHADERCONSTANTS_H_
