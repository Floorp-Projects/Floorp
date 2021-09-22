/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_OGLSHADERCONFIG_H
#define GFX_OGLSHADERCONFIG_H

#include "gfxTypes.h"
#include "ImageTypes.h"
#include "mozilla/Assertions.h"  // for MOZ_ASSERT, etc
#include "mozilla/RefPtr.h"      // for RefPtr
#include "mozilla/gfx/Matrix.h"  // for Matrix4x4
#include "mozilla/gfx/Rect.h"    // for Rect
#include "mozilla/gfx/Types.h"
#include "nsDebug.h"   // for NS_ASSERTION
#include "nsPoint.h"   // for nsIntPoint
#include "nsTArray.h"  // for nsTArray
#include "mozilla/layers/CompositorTypes.h"

namespace mozilla {
namespace layers {

enum ShaderFeatures {
  ENABLE_RENDER_COLOR = 0x01,
  ENABLE_TEXTURE_RECT = 0x02,
  ENABLE_TEXTURE_EXTERNAL = 0x04,
  ENABLE_TEXTURE_YCBCR = 0x08,
  ENABLE_TEXTURE_NV12 = 0x10,
  ENABLE_TEXTURE_COMPONENT_ALPHA = 0x20,
  ENABLE_TEXTURE_NO_ALPHA = 0x40,
  ENABLE_TEXTURE_RB_SWAP = 0x80,
  ENABLE_OPACITY = 0x100,
  ENABLE_BLUR = 0x200,
  ENABLE_COLOR_MATRIX = 0x400,
  ENABLE_MASK = 0x800,
  ENABLE_NO_PREMUL_ALPHA = 0x1000,
  ENABLE_DEAA = 0x2000,
  ENABLE_DYNAMIC_GEOMETRY = 0x4000,
  ENABLE_MASK_TEXTURE_RECT = 0x8000,
  ENABLE_TEXTURE_NV12_GA_SWITCH = 0x10000,
};

class KnownUniform {
 public:
  // this needs to be kept in sync with strings in 'AddUniforms'
  enum KnownUniformName {
    NotAKnownUniform = -1,

    LayerTransform = 0,
    LayerTransformInverse,
    MaskTransform,
    BackdropTransform,
    LayerRects,
    MatrixProj,
    TextureTransform,
    TextureRects,
    RenderTargetOffset,
    LayerOpacity,
    Texture,
    YTexture,
    CbTexture,
    CrTexture,
    RenderColor,
    TexCoordMultiplier,
    CbCrTexCoordMultiplier,
    SSEdges,
    ViewportSize,
    VisibleCenter,
    YuvColorMatrix,
    YuvOffsetVector,

    KnownUniformCount
  };

  KnownUniform() {
    mName = NotAKnownUniform;
    mNameString = nullptr;
    mLocation = -1;
    memset(&mValue, 0, sizeof(mValue));
  }

  bool UpdateUniform(int32_t i1) {
    if (mLocation == -1) return false;
    if (mValue.i1 != i1) {
      mValue.i1 = i1;
      return true;
    }
    return false;
  }

  bool UpdateUniform(float f1) {
    if (mLocation == -1) return false;
    if (mValue.f1 != f1) {
      mValue.f1 = f1;
      return true;
    }
    return false;
  }

  bool UpdateUniform(float f1, float f2) {
    if (mLocation == -1) return false;
    if (mValue.f16v[0] != f1 || mValue.f16v[1] != f2) {
      mValue.f16v[0] = f1;
      mValue.f16v[1] = f2;
      return true;
    }
    return false;
  }

  bool UpdateUniform(float f1, float f2, float f3, float f4) {
    if (mLocation == -1) return false;
    if (mValue.f16v[0] != f1 || mValue.f16v[1] != f2 || mValue.f16v[2] != f3 ||
        mValue.f16v[3] != f4) {
      mValue.f16v[0] = f1;
      mValue.f16v[1] = f2;
      mValue.f16v[2] = f3;
      mValue.f16v[3] = f4;
      return true;
    }
    return false;
  }

  bool UpdateUniform(int cnt, const float* fp) {
    if (mLocation == -1) return false;
    switch (cnt) {
      case 1:
      case 2:
      case 3:
      case 4:
      case 9:
      case 16:
        if (memcmp(mValue.f16v, fp, sizeof(float) * cnt) != 0) {
          memcpy(mValue.f16v, fp, sizeof(float) * cnt);
          return true;
        }
        return false;
    }

    MOZ_ASSERT_UNREACHABLE("cnt must be 1 2 3 4 9 or 16");
    return false;
  }

  bool UpdateArrayUniform(int cnt, const float* fp) {
    if (mLocation == -1) return false;
    if (cnt > 16) {
      return false;
    }

    if (memcmp(mValue.f16v, fp, sizeof(float) * cnt) != 0) {
      memcpy(mValue.f16v, fp, sizeof(float) * cnt);
      return true;
    }
    return false;
  }

  bool UpdateArrayUniform(int cnt, const gfx::Point3D* points) {
    if (mLocation == -1) return false;
    if (cnt > 4) {
      return false;
    }

    float fp[12];
    float* d = fp;
    for (int i = 0; i < cnt; i++) {
      // Note: Do not want to make assumptions about .x, .y, .z member packing.
      // If gfx::Point3D is updated to make this guarantee, SIMD optimizations
      // may be possible
      *d++ = points[i].x;
      *d++ = points[i].y;
      *d++ = points[i].z;
    }

    if (memcmp(mValue.f16v, fp, sizeof(float) * cnt * 3) != 0) {
      memcpy(mValue.f16v, fp, sizeof(float) * cnt * 3);
      return true;
    }
    return false;
  }

  KnownUniformName mName;
  const char* mNameString;
  int32_t mLocation;

  union {
    int i1;
    float f1;
    float f16v[16];
  } mValue;
};

class ShaderConfigOGL {
 public:
  ShaderConfigOGL()
      : mFeatures(0),
        mMultiplier(1),
        mCompositionOp(gfx::CompositionOp::OP_OVER) {}

  void SetRenderColor(bool aEnabled);
  void SetTextureTarget(GLenum aTarget);
  void SetMaskTextureTarget(GLenum aTarget);
  void SetRBSwap(bool aEnabled);
  void SetNoAlpha(bool aEnabled);
  void SetOpacity(bool aEnabled);
  void SetYCbCr(bool aEnabled);
  void SetNV12(bool aEnabled);
  void SetComponentAlpha(bool aEnabled);
  void SetColorMatrix(bool aEnabled);
  void SetBlur(bool aEnabled);
  void SetMask(bool aEnabled);
  void SetDEAA(bool aEnabled);
  void SetCompositionOp(gfx::CompositionOp aOp);
  void SetNoPremultipliedAlpha();
  void SetDynamicGeometry(bool aEnabled);
  void SetColorMultiplier(uint32_t aMultiplier);

  bool operator<(const ShaderConfigOGL& other) const {
    return mFeatures < other.mFeatures ||
           (mFeatures == other.mFeatures &&
            (int)mCompositionOp < (int)other.mCompositionOp) ||
           (mFeatures == other.mFeatures &&
            (int)mCompositionOp == (int)other.mCompositionOp &&
            mMultiplier < other.mMultiplier);
  }

 public:
  void SetFeature(int aBitmask, bool aState) {
    if (aState)
      mFeatures |= aBitmask;
    else
      mFeatures &= (~aBitmask);
  }

  int mFeatures;
  uint32_t mMultiplier;
  gfx::CompositionOp mCompositionOp;
};

static inline ShaderConfigOGL ShaderConfigFromTargetAndFormat(
    GLenum aTarget, gfx::SurfaceFormat aFormat) {
  ShaderConfigOGL config;
  config.SetTextureTarget(aTarget);
  config.SetRBSwap(aFormat == gfx::SurfaceFormat::B8G8R8A8 ||
                   aFormat == gfx::SurfaceFormat::B8G8R8X8);
  config.SetNoAlpha(aFormat == gfx::SurfaceFormat::B8G8R8X8 ||
                    aFormat == gfx::SurfaceFormat::R8G8B8X8 ||
                    aFormat == gfx::SurfaceFormat::R5G6B5_UINT16);
  return config;
}

}  // namespace layers
}  // namespace mozilla

#endif  // GFX_OGLSHADERCONFIG_H
