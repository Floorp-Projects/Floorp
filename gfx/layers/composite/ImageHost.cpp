/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageHost.h"
#include "LayersLogging.h"              // for AppendToString
#include "composite/CompositableHost.h"  // for CompositableHost, etc
#include "ipc/IPCMessageUtils.h"        // for null_t
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/Effects.h"     // for TexturedEffect, Effect, etc
#include "nsAString.h"
#include "nsDebug.h"                    // for NS_WARNING, NS_ASSERTION
#include "nsPrintfCString.h"            // for nsPrintfCString
#include "nsString.h"                   // for nsAutoCString

class nsIntRegion;

namespace mozilla {
namespace gfx {
class Matrix4x4;
}

using namespace gfx;

namespace layers {

class ISurfaceAllocator;

ImageHost::ImageHost(const TextureInfo& aTextureInfo)
  : CompositableHost(aTextureInfo)
  , mHasPictureRect(false)
  , mLocked(false)
{}

ImageHost::~ImageHost()
{
}

void
ImageHost::UseTextureHost(TextureHost* aTexture)
{
  CompositableHost::UseTextureHost(aTexture);
  mFrontBuffer = aTexture;
  if (mFrontBuffer) {
    mFrontBuffer->Updated();
    mFrontBuffer->PrepareTextureSource(mTextureSource);
  }
}

void
ImageHost::RemoveTextureHost(TextureHost* aTexture)
{
  CompositableHost::RemoveTextureHost(aTexture);
  if (aTexture && mFrontBuffer == aTexture) {
    mFrontBuffer->UnbindTextureSource();
    mTextureSource = nullptr;
    mFrontBuffer = nullptr;
  }
}

TextureHost*
ImageHost::GetAsTextureHost()
{
  return mFrontBuffer;
}

void
ImageHost::Composite(EffectChain& aEffectChain,
                     float aOpacity,
                     const gfx::Matrix4x4& aTransform,
                     const gfx::Filter& aFilter,
                     const gfx::Rect& aClipRect,
                     const nsIntRegion* aVisibleRegion)
{
  if (!GetCompositor()) {
    // should only happen when a tab is dragged to another window and
    // async-video is still sending frames but we haven't attached the
    // set the new compositor yet.
    return;
  }
  if (!mFrontBuffer) {
    return;
  }

  // Make sure the front buffer has a compositor
  mFrontBuffer->SetCompositor(GetCompositor());

  AutoLockCompositableHost autoLock(this);
  if (autoLock.Failed()) {
    NS_WARNING("failed to lock front buffer");
    return;
  }

  if (!mFrontBuffer->BindTextureSource(mTextureSource)) {
    return;
  }

  if (!mTextureSource) {
    // BindTextureSource above should have returned false!
    MOZ_ASSERT(false);
    return;
  }

  bool isAlphaPremultiplied = !(mFrontBuffer->GetFlags() & TextureFlags::NON_PREMULTIPLIED);
  RefPtr<TexturedEffect> effect = CreateTexturedEffect(mFrontBuffer->GetFormat(),
                                                       mTextureSource.get(),
                                                       aFilter,
                                                       isAlphaPremultiplied);
  if (!effect) {
    return;
  }

  aEffectChain.mPrimaryEffect = effect;
  IntSize textureSize = mTextureSource->GetSize();
  gfx::Rect gfxPictureRect
    = mHasPictureRect ? gfx::Rect(0, 0, mPictureRect.width, mPictureRect.height)
                      : gfx::Rect(0, 0, textureSize.width, textureSize.height);

  gfx::Rect pictureRect(0, 0,
                        mPictureRect.width,
                        mPictureRect.height);
  BigImageIterator* it = mTextureSource->AsBigImageIterator();
  if (it) {

    // This iteration does not work if we have multiple texture sources here
    // (e.g. 3 YCbCr textures). There's nothing preventing the different
    // planes from having different resolutions or tile sizes. For example, a
    // YCbCr frame could have Cb and Cr planes that are half the resolution of
    // the Y plane, in such a way that the Y plane overflows the maximum
    // texture size and the Cb and Cr planes do not. Then the Y plane would be
    // split into multiple tiles and the Cb and Cr planes would just be one
    // tile each.
    // To handle the general case correctly, we'd have to create a grid of
    // intersected tiles over all planes, and then draw each grid tile using
    // the corresponding source tiles from all planes, with appropriate
    // per-plane per-tile texture coords.
    // DrawQuad currently assumes that all planes use the same texture coords.
    MOZ_ASSERT(it->GetTileCount() == 1 || !mTextureSource->GetNextSibling(),
               "Can't handle multi-plane BigImages");

    it->BeginBigImageIteration();
    do {
      IntRect tileRect = it->GetTileRect();
      gfx::Rect rect(tileRect.x, tileRect.y, tileRect.width, tileRect.height);
      if (mHasPictureRect) {
        rect = rect.Intersect(pictureRect);
        effect->mTextureCoords = Rect(Float(rect.x - tileRect.x)/ tileRect.width,
                                      Float(rect.y - tileRect.y) / tileRect.height,
                                      Float(rect.width) / tileRect.width,
                                      Float(rect.height) / tileRect.height);
      } else {
        effect->mTextureCoords = Rect(0, 0, 1, 1);
      }
      if (mFrontBuffer->GetFlags() & TextureFlags::ORIGIN_BOTTOM_LEFT) {
        effect->mTextureCoords.y = effect->mTextureCoords.YMost();
        effect->mTextureCoords.height = -effect->mTextureCoords.height;
      }
      GetCompositor()->DrawQuad(rect, aClipRect, aEffectChain,
                                aOpacity, aTransform);
      GetCompositor()->DrawDiagnostics(DiagnosticFlags::IMAGE | DiagnosticFlags::BIGIMAGE,
                                       rect, aClipRect, aTransform, mFlashCounter);
    } while (it->NextTile());
    it->EndBigImageIteration();
    // layer border
    GetCompositor()->DrawDiagnostics(DiagnosticFlags::IMAGE,
                                     gfxPictureRect, aClipRect,
                                     aTransform, mFlashCounter);
  } else {
    IntSize textureSize = mTextureSource->GetSize();
    gfx::Rect rect;
    if (mHasPictureRect) {
      effect->mTextureCoords = Rect(Float(mPictureRect.x) / textureSize.width,
                                    Float(mPictureRect.y) / textureSize.height,
                                    Float(mPictureRect.width) / textureSize.width,
                                    Float(mPictureRect.height) / textureSize.height);
      rect = pictureRect;
    } else {
      effect->mTextureCoords = Rect(0, 0, 1, 1);
      rect = gfx::Rect(0, 0, textureSize.width, textureSize.height);
    }

    if (mFrontBuffer->GetFlags() & TextureFlags::ORIGIN_BOTTOM_LEFT) {
      effect->mTextureCoords.y = effect->mTextureCoords.YMost();
      effect->mTextureCoords.height = -effect->mTextureCoords.height;
    }

    GetCompositor()->DrawQuad(rect, aClipRect, aEffectChain,
                              aOpacity, aTransform);
    GetCompositor()->DrawDiagnostics(DiagnosticFlags::IMAGE,
                                     rect, aClipRect,
                                     aTransform, mFlashCounter);
  }
}

void
ImageHost::SetCompositor(Compositor* aCompositor)
{
  if (mFrontBuffer && mCompositor != aCompositor) {
    mFrontBuffer->SetCompositor(aCompositor);
  }
  CompositableHost::SetCompositor(aCompositor);
}

void
ImageHost::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  aStream << aPrefix;
  aStream << nsPrintfCString("ImageHost (0x%p)", this).get();

  AppendToString(aStream, mPictureRect, " [picture-rect=", "]");

  if (mFrontBuffer) {
    nsAutoCString pfx(aPrefix);
    pfx += "  ";
    aStream << "\n";
    mFrontBuffer->PrintInfo(aStream, pfx.get());
  }
}

void
ImageHost::Dump(std::stringstream& aStream,
                const char* aPrefix,
                bool aDumpHtml)
{
  if (mFrontBuffer) {
    aStream << aPrefix;
    aStream << (aDumpHtml ? "<ul><li>TextureHost: "
                             : "TextureHost: ");
    DumpTextureHost(aStream, mFrontBuffer);
    aStream << (aDumpHtml ? " </li></ul> " : " ");
  }
}

LayerRenderState
ImageHost::GetRenderState()
{
  if (mFrontBuffer) {
    return mFrontBuffer->GetRenderState();
  }
  return LayerRenderState();
}

TemporaryRef<gfx::DataSourceSurface>
ImageHost::GetAsSurface()
{
  return mFrontBuffer->GetAsSurface();
}

bool
ImageHost::Lock()
{
  MOZ_ASSERT(!mLocked);
  if (!mFrontBuffer) {
    return false;
  }
  if (!mFrontBuffer->Lock()) {
    return false;
  }
  mLocked = true;
  return true;
}

void
ImageHost::Unlock()
{
  MOZ_ASSERT(mLocked);
  if (mFrontBuffer) {
    mFrontBuffer->Unlock();
  }
  mLocked = false;
}

IntSize
ImageHost::GetImageSize() const
{
  if (mHasPictureRect) {
    return IntSize(mPictureRect.width, mPictureRect.height);
  }

  if (mFrontBuffer) {
    return mFrontBuffer->GetSize();
  }

  return IntSize();
}

TemporaryRef<TexturedEffect>
ImageHost::GenEffect(const gfx::Filter& aFilter)
{
  if (!mFrontBuffer->BindTextureSource(mTextureSource)) {
    return nullptr;
  }
  bool isAlphaPremultiplied = true;
  if (mFrontBuffer->GetFlags() & TextureFlags::NON_PREMULTIPLIED)
    isAlphaPremultiplied = false;

  return CreateTexturedEffect(mFrontBuffer->GetFormat(),
                              mTextureSource,
                              aFilter,
                              isAlphaPremultiplied);
}

#ifdef MOZ_WIDGET_GONK
ImageHostOverlay::ImageHostOverlay(const TextureInfo& aTextureInfo)
  : CompositableHost(aTextureInfo)
  , mHasPictureRect(false)
{
}

ImageHostOverlay::~ImageHostOverlay()
{
}

void
ImageHostOverlay::Composite(EffectChain& aEffectChain,
                            float aOpacity,
                            const gfx::Matrix4x4& aTransform,
                            const gfx::Filter& aFilter,
                            const gfx::Rect& aClipRect,
                            const nsIntRegion* aVisibleRegion)
{
  if (!GetCompositor()) {
    return;
  }

  if (mOverlay.handle().type() == OverlayHandle::Tnull_t)
    return;
  Color hollow(0.0f, 0.0f, 0.0f, 0.0f);
  aEffectChain.mPrimaryEffect = new EffectSolidColor(hollow);
  aEffectChain.mSecondaryEffects[EffectTypes::BLEND_MODE] = new EffectBlendMode(CompositionOp::OP_SOURCE);

  gfx::Rect rect;
  gfx::Rect clipRect(aClipRect.x, aClipRect.y,
                     aClipRect.width, aClipRect.height);
  if (mHasPictureRect) {
    rect.SetRect(mPictureRect.x, mPictureRect.y,
                 mPictureRect.width, mPictureRect.height);
  } else {
    rect.SetRect(0, 0,
                 mOverlay.size().width, mOverlay.size().height);
  }

  mCompositor->DrawQuad(rect, aClipRect, aEffectChain, aOpacity, aTransform);
  mCompositor->DrawDiagnostics(DiagnosticFlags::IMAGE | DiagnosticFlags::BIGIMAGE,
                               rect, aClipRect, aTransform, mFlashCounter);
}

LayerRenderState
ImageHostOverlay::GetRenderState()
{
  LayerRenderState state;
  if (mOverlay.handle().type() == OverlayHandle::Tint32_t) {
    state.SetOverlayId(mOverlay.handle().get_int32_t());
  }
  return state;
}

void
ImageHostOverlay::UseOverlaySource(OverlaySource aOverlay)
{
  mOverlay = aOverlay;
}

void
ImageHostOverlay::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  aStream << aPrefix;
  aStream << nsPrintfCString("ImageHost (0x%p)", this).get();

  AppendToString(aStream, mPictureRect, " [picture-rect=", "]");

  if (mOverlay.handle().type() == OverlayHandle::Tint32_t) {
    nsAutoCString pfx(aPrefix);
    pfx += "  ";
    aStream << nsPrintfCString("Overlay: %d", mOverlay.handle().get_int32_t()).get();
  }
}

#endif
} // namespace layers
} // namespace mozilla
