/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_EFFECTS_H
#define MOZILLA_LAYERS_EFFECTS_H

#include "mozilla/gfx/Matrix.h"
#include "mozilla/layers/Compositor.h"
#include "LayersLogging.h"
#include "mozilla/RefPtr.h"

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


enum EffectTypes
{
  EFFECT_MASK,
  EFFECT_MAX_SECONDARY, // sentinel for the count of secondary effect types
  EFFECT_BGRX,
  EFFECT_RGBX,
  EFFECT_BGRA,
  EFFECT_RGBA,
  EFFECT_RGBA_EXTERNAL,
  EFFECT_YCBCR,
  EFFECT_COMPONENT_ALPHA,
  EFFECT_SOLID_COLOR,
  EFFECT_RENDER_TARGET,
  EFFECT_MAX  //sentinel for the count of all effect types
};

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

struct EffectRGBAExternal : public TexturedEffect
{
  EffectRGBAExternal(TextureSource *aRGBATexture,
                     const gfx::Matrix4x4 &aTextureTransform,
                     bool aPremultiplied,
                     gfx::Filter aFilter)
    : TexturedEffect(EFFECT_RGBA_EXTERNAL, aRGBATexture, aPremultiplied, aFilter)
    , mTextureTransform(aTextureTransform)
  {}

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() { return "EffectRGBAExternal"; }
#endif

  gfx::Matrix4x4 mTextureTransform;
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
  EffectComponentAlpha(TextureSource *aOnWhite, TextureSource *aOnBlack)
    : TexturedEffect(EFFECT_COMPONENT_ALPHA, nullptr, false, gfx::FILTER_LINEAR)
    , mOnWhite(aOnWhite)
    , mOnBlack(aOnBlack)
  {}

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() { return "EffectComponentAlpha"; }
#endif

  TextureSource* mOnWhite;
  TextureSource* mOnBlack;
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

inline TemporaryRef<TexturedEffect>
CreateTexturedEffect(TextureHost *aTextureHost,
                     const gfx::Filter& aFilter)
{
  RefPtr<TexturedEffect> result;
  switch (aTextureHost->GetFormat()) {
  case gfx::FORMAT_B8G8R8A8:
    result = new EffectBGRA(aTextureHost, true, aFilter);
    break;
  case gfx::FORMAT_B8G8R8X8:
    result = new EffectBGRX(aTextureHost, true, aFilter);
    break;
  case gfx::FORMAT_R8G8B8X8:
    result = new EffectRGBX(aTextureHost, true, aFilter);
    break;
  case gfx::FORMAT_R5G6B5:
    result = new EffectRGBX(aTextureHost, true, aFilter);
    break;
  case gfx::FORMAT_R8G8B8A8:
    result = new EffectRGBA(aTextureHost, true, aFilter);
    break;
  case gfx::FORMAT_YUV:
    result = new EffectYCbCr(aTextureHost, aFilter);
    break;
  default:
    MOZ_NOT_REACHED("unhandled program type");
  }

  return result;
}

} // namespace layers
} // namespace mozilla

#endif
