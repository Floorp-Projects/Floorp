/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_EFFECTS_H
#define MOZILLA_LAYERS_EFFECTS_H

#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/RefPtr.h"             // for RefPtr, TemporaryRef, etc
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/gfx/Types.h"          // for Filter, etc
#include "mozilla/layers/CompositorTypes.h"  // for EffectTypes, etc
#include "mozilla/layers/LayersTypes.h"  // for MOZ_LAYERS_HAVE_LOG
#include "mozilla/layers/TextureHost.h"  // for CompositingRenderTarget, etc
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nscore.h"                     // for nsACString

namespace mozilla {
namespace layers {

/**
 * Effects and effect chains are used by the compositor API (see Compositor.h).
 * An effect chain represents a rendering method, for example some shader and
 * the data required for that shader to run. An effect is some component of the
 * chain and its data.
 *
 * An effect chain consists of a primary effect - how the 'texture' memory should
 * be interpreted (RGBA, BGRX, YCBCR, etc.) - and any number of secondary effects
 * - any way in which rendering can be changed, e.g., applying a mask layer.
 *
 * During the rendering process, an effect chain is created by the layer being
 * rendered and the primary effect is added by the compositable host. Secondary
 * effects may be added by the layer or compositable. The effect chain is passed
 * to the compositor by the compositable host as a parameter to DrawQuad.
 */

struct Effect : public RefCounted<Effect>
{
  Effect(EffectTypes aType) : mType(aType) {}

  EffectTypes mType;

  virtual ~Effect() {}
#ifdef MOZ_LAYERS_HAVE_LOG
  virtual void PrintInfo(nsACString& aTo, const char* aPrefix) =0;
#endif
};

// Render from a texture
struct TexturedEffect : public Effect
{
  TexturedEffect(EffectTypes aType,
                 TextureSource *aTexture,
                 bool aPremultiplied,
                 gfx::Filter aFilter)
     : Effect(aType)
     , mTextureCoords(0, 0, 1.0f, 1.0f)
     , mTexture(aTexture)
     , mPremultiplied(aPremultiplied)
     , mFilter(aFilter)
  {}

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() = 0;
  virtual void PrintInfo(nsACString& aTo, const char* aPrefix);
#endif

  gfx::Rect mTextureCoords;
  TextureSource* mTexture;
  bool mPremultiplied;
  gfx::Filter mFilter;;
};

// Support an alpha mask.
struct EffectMask : public Effect
{
  EffectMask(TextureSource *aMaskTexture,
             gfx::IntSize aSize,
             const gfx::Matrix4x4 &aMaskTransform)
    : Effect(EFFECT_MASK)
    , mMaskTexture(aMaskTexture)
    , mIs3D(false)
    , mSize(aSize)
    , mMaskTransform(aMaskTransform)
  {}

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual void PrintInfo(nsACString& aTo, const char* aPrefix);
#endif

  TextureSource* mMaskTexture;
  bool mIs3D;
  gfx::IntSize mSize;
  gfx::Matrix4x4 mMaskTransform;
};

// Render to a render target rather than the screen.
struct EffectRenderTarget : public TexturedEffect
{
  EffectRenderTarget(CompositingRenderTarget *aRenderTarget)
    : TexturedEffect(EFFECT_RENDER_TARGET, aRenderTarget, true, gfx::FILTER_LINEAR)
    , mRenderTarget(aRenderTarget)
  {}

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() { return "EffectRenderTarget"; }
  virtual void PrintInfo(nsACString& aTo, const char* aPrefix);
#endif

  RefPtr<CompositingRenderTarget> mRenderTarget;
};

struct EffectBGRX : public TexturedEffect
{
  EffectBGRX(TextureSource *aBGRXTexture,
             bool aPremultiplied,
             gfx::Filter aFilter,
             bool aFlipped = false)
    : TexturedEffect(EFFECT_BGRX, aBGRXTexture, aPremultiplied, aFilter)
  {}

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() { return "EffectBGRX"; }
#endif
};

struct EffectRGBX : public TexturedEffect
{
  EffectRGBX(TextureSource *aRGBXTexture,
             bool aPremultiplied,
             gfx::Filter aFilter)
    : TexturedEffect(EFFECT_RGBX, aRGBXTexture, aPremultiplied, aFilter)
  {}

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() { return "EffectRGBX"; }
#endif
};

struct EffectBGRA : public TexturedEffect
{
  EffectBGRA(TextureSource *aBGRATexture,
             bool aPremultiplied,
             gfx::Filter aFilter)
    : TexturedEffect(EFFECT_BGRA, aBGRATexture, aPremultiplied, aFilter)
  {}

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() { return "EffectBGRA"; }
#endif
};

struct EffectRGBA : public TexturedEffect
{
  EffectRGBA(TextureSource *aRGBATexture,
             bool aPremultiplied,
             gfx::Filter aFilter)
    : TexturedEffect(EFFECT_RGBA, aRGBATexture, aPremultiplied, aFilter)
  {}

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() { return "EffectRGBA"; }
#endif
};

struct EffectYCbCr : public TexturedEffect
{
  EffectYCbCr(TextureSource *aSource, gfx::Filter aFilter)
    : TexturedEffect(EFFECT_YCBCR, aSource, false, aFilter)
  {}

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() { return "EffectYCbCr"; }
#endif
};

struct EffectComponentAlpha : public TexturedEffect
{
  EffectComponentAlpha(TextureSource *aOnBlack,
                       TextureSource *aOnWhite,
                       gfx::Filter aFilter)
    : TexturedEffect(EFFECT_COMPONENT_ALPHA, nullptr, false, aFilter)
    , mOnBlack(aOnBlack)
    , mOnWhite(aOnWhite)
  {}

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() { return "EffectComponentAlpha"; }
#endif

  TextureSource* mOnBlack;
  TextureSource* mOnWhite;
};

struct EffectSolidColor : public Effect
{
  EffectSolidColor(const gfx::Color &aColor)
    : Effect(EFFECT_SOLID_COLOR)
    , mColor(aColor)
  {}

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual void PrintInfo(nsACString& aTo, const char* aPrefix);
#endif

  gfx::Color mColor;
};

struct EffectChain
{
  RefPtr<Effect> mPrimaryEffect;
  RefPtr<Effect> mSecondaryEffects[EFFECT_MAX_SECONDARY];
};

/**
 * Create a Textured effect corresponding to aFormat and using
 * aSource as the (first) texture source.
 *
 * Note that aFormat can be different form aSource->GetFormat if, we are
 * creating an effect that takes several texture sources (like with YCBCR
 * where aFormat would be FOMRAT_YCBCR and each texture source would be
 * a one-channel A8 texture)
 */
inline TemporaryRef<TexturedEffect>
CreateTexturedEffect(gfx::SurfaceFormat aFormat,
                     TextureSource* aSource,
                     const gfx::Filter& aFilter)
{
  MOZ_ASSERT(aSource);
  RefPtr<TexturedEffect> result;
  switch (aFormat) {
  case gfx::FORMAT_B8G8R8A8:
    result = new EffectBGRA(aSource, true, aFilter);
    break;
  case gfx::FORMAT_B8G8R8X8:
    result = new EffectBGRX(aSource, true, aFilter);
    break;
  case gfx::FORMAT_R8G8B8X8:
    result = new EffectRGBX(aSource, true, aFilter);
    break;
  case gfx::FORMAT_R5G6B5:
    result = new EffectRGBX(aSource, true, aFilter);
    break;
  case gfx::FORMAT_R8G8B8A8:
    result = new EffectRGBA(aSource, true, aFilter);
    break;
  case gfx::FORMAT_YUV:
    result = new EffectYCbCr(aSource, aFilter);
    break;
  default:
    MOZ_CRASH("unhandled program type");
  }

  return result;
}

/**
 * Create a textured effect based on aSource format and the presence of
 * aSourceOnWhite.
 *
 * aSourceOnWhite can be null.
 */
inline TemporaryRef<TexturedEffect>
CreateTexturedEffect(TextureSource* aSource,
                     TextureSource* aSourceOnWhite,
                     const gfx::Filter& aFilter)
{
  MOZ_ASSERT(aSource);
  if (aSourceOnWhite) {
    MOZ_ASSERT(aSource->GetFormat() == gfx::FORMAT_R8G8B8X8 ||
               aSourceOnWhite->GetFormat() == gfx::FORMAT_B8G8R8X8);
    return new EffectComponentAlpha(aSource, aSourceOnWhite, aFilter);
  }

  return CreateTexturedEffect(aSource->GetFormat(), aSource, aFilter);
}

/**
 * Create a textured effect based on aSource format.
 *
 * This version excudes the possibility of component alpha.
 */
inline TemporaryRef<TexturedEffect>
CreateTexturedEffect(TextureSource *aTexture,
                     const gfx::Filter& aFilter)
{
  return CreateTexturedEffect(aTexture, nullptr, aFilter);
}


} // namespace layers
} // namespace mozilla

#endif
