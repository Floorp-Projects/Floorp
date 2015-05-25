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
  , mLocked(false)
{}

ImageHost::~ImageHost()
{}

void
ImageHost::UseTextureHost(const nsTArray<TimedTexture>& aTextures)
{
  MOZ_ASSERT(!mLocked);

  CompositableHost::UseTextureHost(aTextures);
  MOZ_ASSERT(aTextures.Length() >= 1);

  nsTArray<TimedImage> newImages;

  // Remove all mImages without an mTextureSource to recycle.
  for (int32_t i = mImages.Length() - 1; i >= 0; --i) {
    if (!mImages[i].mTextureSource) {
      mImages.RemoveElementAt(i);
    }
  }

  // Create new TimedImage entries and recycle any mTextureSources that match
  // our mFrontBuffers.
  for (auto& t : aTextures) {
    TimedImage& img = *newImages.AppendElement();
    MOZ_ASSERT(t.mTexture);
    img.mFrontBuffer = t.mTexture;
    for (uint32_t i = 0; i < mImages.Length(); ++i) {
      if (mImages[i].mFrontBuffer == img.mFrontBuffer) {
        img.mTextureSource = mImages[i].mTextureSource;
        mImages.RemoveElementAt(i);
        break;
      }
    }
    img.mTimeStamp = t.mTimeStamp;
    img.mPictureRect = t.mPictureRect;
  }
  // Recycle any leftover mTextureSources and call PrepareTextureSource on all
  // images.
  for (auto& img : newImages) {
    if (!img.mTextureSource && !mImages.IsEmpty()) {
      img.mTextureSource = mImages.LastElement().mTextureSource;
      mImages.RemoveElementAt(mImages.Length() - 1);
    }
    img.mFrontBuffer->Updated();
    img.mFrontBuffer->PrepareTextureSource(img.mTextureSource);
  }
  mImages.SwapElements(newImages);
}

void
ImageHost::RemoveTextureHost(TextureHost* aTexture)
{
  MOZ_ASSERT(!mLocked);

  CompositableHost::RemoveTextureHost(aTexture);

  for (int32_t i = mImages.Length() - 1; i >= 0; --i) {
    if (mImages[i].mFrontBuffer == aTexture) {
      aTexture->UnbindTextureSource();
      mImages.RemoveElementAt(i);
    }
  }
}

int ImageHost::ChooseImageIndex() const
{
  if (!mCompositor || mImages.IsEmpty()) {
    return -1;
  }
  TimeStamp now = mCompositor->GetCompositionTime();

  if (now.IsNull()) {
    // Not in a composition, so just return the last image we composited
    // (if it's one of the current images).
    for (uint32_t i = 0; i < mImages.Length(); ++i) {
      // For now there's only one image so we'll cheat until we can do better.
      return i;
    }
    return -1;
  }

  uint32_t result = 0;
  while (result + 1 < mImages.Length() &&
      mImages[result + 1].mTimeStamp <= now) {
    ++result;
  }
  return result;
}

const ImageHost::TimedImage* ImageHost::ChooseImage() const
{
  int index = ChooseImageIndex();
  return index >= 0 ? &mImages[index] : nullptr;
}

ImageHost::TimedImage* ImageHost::ChooseImage()
{
  int index = ChooseImageIndex();
  return index >= 0 ? &mImages[index] : nullptr;
}

TextureHost*
ImageHost::GetAsTextureHost(IntRect* aPictureRect)
{
  TimedImage* img = ChooseImage();
  if (aPictureRect && img) {
    *aPictureRect = img->mPictureRect;
  }
  return img ? img->mFrontBuffer.get() : nullptr;
}

void ImageHost::Attach(Layer* aLayer,
                       Compositor* aCompositor,
                       AttachFlags aFlags)
{
  CompositableHost::Attach(aLayer, aCompositor, aFlags);
  for (auto& img : mImages) {
    if (GetCompositor()) {
      img.mFrontBuffer->SetCompositor(GetCompositor());
    }
    img.mFrontBuffer->Updated();
    img.mFrontBuffer->PrepareTextureSource(img.mTextureSource);
  }
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
  TimedImage* img = ChooseImage();
  if (!img) {
    return;
  }

  // Make sure the front buffer has a compositor
  img->mFrontBuffer->SetCompositor(GetCompositor());

  AutoLockCompositableHost autoLock(this);
  if (autoLock.Failed()) {
    NS_WARNING("failed to lock front buffer");
    return;
  }

  if (!img->mFrontBuffer->BindTextureSource(img->mTextureSource)) {
    return;
  }

  if (!img->mTextureSource) {
    // BindTextureSource above should have returned false!
    MOZ_ASSERT(false);
    return;
  }

  bool isAlphaPremultiplied =
      !(img->mFrontBuffer->GetFlags() & TextureFlags::NON_PREMULTIPLIED);
  RefPtr<TexturedEffect> effect =
      CreateTexturedEffect(img->mFrontBuffer->GetFormat(),
          img->mTextureSource.get(), aFilter, isAlphaPremultiplied,
          GetRenderState());
  if (!effect) {
    return;
  }

  aEffectChain.mPrimaryEffect = effect;
  gfx::Rect pictureRect(0, 0, img->mPictureRect.width, img->mPictureRect.height);
  BigImageIterator* it = img->mTextureSource->AsBigImageIterator();
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
    MOZ_ASSERT(it->GetTileCount() == 1 || !img->mTextureSource->GetNextSibling(),
               "Can't handle multi-plane BigImages");

    it->BeginBigImageIteration();
    do {
      IntRect tileRect = it->GetTileRect();
      gfx::Rect rect(tileRect.x, tileRect.y, tileRect.width, tileRect.height);
      rect = rect.Intersect(pictureRect);
      effect->mTextureCoords = Rect(Float(rect.x - tileRect.x) / tileRect.width,
                                    Float(rect.y - tileRect.y) / tileRect.height,
                                    Float(rect.width) / tileRect.width,
                                    Float(rect.height) / tileRect.height);
      if (img->mFrontBuffer->GetFlags() & TextureFlags::ORIGIN_BOTTOM_LEFT) {
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
    GetCompositor()->DrawDiagnostics(DiagnosticFlags::IMAGE, pictureRect,
                                     aClipRect, aTransform, mFlashCounter);
  } else {
    IntSize textureSize = img->mTextureSource->GetSize();
    effect->mTextureCoords = Rect(Float(img->mPictureRect.x) / textureSize.width,
                                  Float(img->mPictureRect.y) / textureSize.height,
                                  Float(img->mPictureRect.width) / textureSize.width,
                                  Float(img->mPictureRect.height) / textureSize.height);

    if (img->mFrontBuffer->GetFlags() & TextureFlags::ORIGIN_BOTTOM_LEFT) {
      effect->mTextureCoords.y = effect->mTextureCoords.YMost();
      effect->mTextureCoords.height = -effect->mTextureCoords.height;
    }

    GetCompositor()->DrawQuad(pictureRect, aClipRect, aEffectChain,
                              aOpacity, aTransform);
    GetCompositor()->DrawDiagnostics(DiagnosticFlags::IMAGE,
                                     pictureRect, aClipRect,
                                     aTransform, mFlashCounter);
  }
}

void
ImageHost::SetCompositor(Compositor* aCompositor)
{
  if (mCompositor != aCompositor) {
    for (auto& img : mImages) {
      img.mFrontBuffer->SetCompositor(aCompositor);
    }
  }
  CompositableHost::SetCompositor(aCompositor);
}

void
ImageHost::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  aStream << aPrefix;
  aStream << nsPrintfCString("ImageHost (0x%p)", this).get();

  nsAutoCString pfx(aPrefix);
  pfx += "  ";
  for (auto& img : mImages) {
    aStream << "\n";
    img.mFrontBuffer->PrintInfo(aStream, pfx.get());
    AppendToString(aStream, img.mPictureRect, " [picture-rect=", "]");
  }
}

void
ImageHost::Dump(std::stringstream& aStream,
                const char* aPrefix,
                bool aDumpHtml)
{
  for (auto& img : mImages) {
    aStream << aPrefix;
    aStream << (aDumpHtml ? "<ul><li>TextureHost: "
                             : "TextureHost: ");
    DumpTextureHost(aStream, img.mFrontBuffer);
    aStream << (aDumpHtml ? " </li></ul> " : " ");
  }
}

LayerRenderState
ImageHost::GetRenderState()
{
  TimedImage* img = ChooseImage();
  if (img) {
    return img->mFrontBuffer->GetRenderState();
  }
  return LayerRenderState();
}

already_AddRefed<gfx::DataSourceSurface>
ImageHost::GetAsSurface()
{
  TimedImage* img = ChooseImage();
  if (img) {
    return img->mFrontBuffer->GetAsSurface();
  }
  return nullptr;
}

bool
ImageHost::Lock()
{
  MOZ_ASSERT(!mLocked);
  TimedImage* img = ChooseImage();
  if (!img) {
    return false;
  }
  if (!img->mFrontBuffer->Lock()) {
    return false;
  }
  mLocked = true;
  return true;
}

void
ImageHost::Unlock()
{
  MOZ_ASSERT(mLocked);
  TimedImage* img = ChooseImage();
  if (img) {
    img->mFrontBuffer->Unlock();
  }
  mLocked = false;
}

IntSize
ImageHost::GetImageSize() const
{
  const TimedImage* img = ChooseImage();
  if (img) {
    return IntSize(img->mPictureRect.width, img->mPictureRect.height);
  }
  return IntSize();
}

already_AddRefed<TexturedEffect>
ImageHost::GenEffect(const gfx::Filter& aFilter)
{
  TimedImage* img = ChooseImage();
  if (!img) {
    return nullptr;
  }
  if (!img->mFrontBuffer->BindTextureSource(img->mTextureSource)) {
    return nullptr;
  }
  bool isAlphaPremultiplied = true;
  if (img->mFrontBuffer->GetFlags() & TextureFlags::NON_PREMULTIPLIED) {
    isAlphaPremultiplied = false;
  }

  return CreateTexturedEffect(img->mFrontBuffer->GetFormat(),
                              img->mTextureSource,
                              aFilter,
                              isAlphaPremultiplied,
                              GetRenderState());
}

#ifdef MOZ_WIDGET_GONK
ImageHostOverlay::ImageHostOverlay(const TextureInfo& aTextureInfo)
  : CompositableHost(aTextureInfo)
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
  rect.SetRect(mPictureRect.x, mPictureRect.y,
               mPictureRect.width, mPictureRect.height);

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
ImageHostOverlay::UseOverlaySource(OverlaySource aOverlay,
                                   const nsIntRect& aPictureRect)
{
  mOverlay = aOverlay;
  mPictureRect = aPictureRect;
}

IntSize
ImageHostOverlay::GetImageSize() const
{
  return IntSize(mPictureRect.width, mPictureRect.height);
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
