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
#include "mozilla/layers/ImageContainerParent.h"
#include "mozilla/layers/LayerManagerComposite.h"     // for TexturedEffect, Effect, etc
#include "nsAString.h"
#include "nsDebug.h"                    // for NS_WARNING, NS_ASSERTION
#include "nsPrintfCString.h"            // for nsPrintfCString
#include "nsString.h"                   // for nsAutoCString

class nsIntRegion;

#define BIAS_TIME_MS 1.0

namespace mozilla {
namespace gfx {
class Matrix4x4;
} // namespace gfx

using namespace gfx;

namespace layers {

class ISurfaceAllocator;

ImageHost::ImageHost(const TextureInfo& aTextureInfo)
  : CompositableHost(aTextureInfo)
  , mImageContainer(nullptr)
  , mLastFrameID(-1)
  , mLastProducerID(-1)
  , mBias(BIAS_NONE)
  , mLocked(false)
{}

ImageHost::~ImageHost()
{
  SetImageContainer(nullptr);
}

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
  for (uint32_t i = 0; i < aTextures.Length(); ++i) {
    const TimedTexture& t = aTextures[i];
    MOZ_ASSERT(t.mTexture);
    if (i + 1 < aTextures.Length() &&
        t.mProducerID == mLastProducerID && t.mFrameID < mLastFrameID) {
      // Ignore frames before a frame that we already composited. We don't
      // ever want to display these frames. This could be important if
      // the frame producer adjusts timestamps (e.g. to track the audio clock)
      // and the new frame times are earlier.
      continue;
    }
    TimedImage& img = *newImages.AppendElement();
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
    img.mFrameID = t.mFrameID;
    img.mProducerID = t.mProducerID;
  }
  // Recycle any leftover mTextureSources and call PrepareTextureSource on all
  // images.
  for (auto& img : newImages) {
    if (!img.mTextureSource && !mImages.IsEmpty()) {
      img.mTextureSource = mImages.LastElement().mTextureSource;
      mImages.RemoveElementAt(mImages.Length() - 1);
    }
    // SetCropRect() affects only on a specific platform.
    // If it is not implemented, it does nothing.
    img.mFrontBuffer->SetCropRect(img.mPictureRect);
    img.mFrontBuffer->Updated();
    img.mFrontBuffer->PrepareTextureSource(img.mTextureSource);
  }
  mImages.SwapElements(newImages);

  // Video producers generally send replacement images with the same frameID but
  // slightly different timestamps in order to sync with the audio clock. This
  // means that any CompositeUntil() call we made in Composite() may no longer
  // guarantee that we'll composite until the next frame is ready. Fix that here.
  if (GetCompositor() && mLastFrameID >= 0) {
    for (size_t i = 0; i < mImages.Length(); ++i) {
      bool frameComesAfter = mImages[i].mFrameID > mLastFrameID ||
                             mImages[i].mProducerID != mLastProducerID;
      if (frameComesAfter && !mImages[i].mTimeStamp.IsNull()) {
        GetCompositor()->CompositeUntil(mImages[i].mTimeStamp +
                                        TimeDuration::FromMilliseconds(BIAS_TIME_MS));
        break;
      }
    }
  }
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

static TimeStamp
GetBiasedTime(const TimeStamp& aInput, ImageHost::Bias aBias)
{
  switch (aBias) {
  case ImageHost::BIAS_NEGATIVE:
    return aInput - TimeDuration::FromMilliseconds(BIAS_TIME_MS);
  case ImageHost::BIAS_POSITIVE:
    return aInput + TimeDuration::FromMilliseconds(BIAS_TIME_MS);
  default:
    return aInput;
  }
}

static ImageHost::Bias
UpdateBias(const TimeStamp& aCompositionTime,
           const TimeStamp& aCompositedImageTime,
           const TimeStamp& aNextImageTime, // may be null
           ImageHost::Bias aBias)
{
  if (aCompositedImageTime.IsNull()) {
    return ImageHost::BIAS_NONE;
  }
  TimeDuration threshold = TimeDuration::FromMilliseconds(1.0);
  if (aCompositionTime - aCompositedImageTime < threshold &&
      aCompositionTime - aCompositedImageTime > -threshold) {
    // The chosen frame's time is very close to the composition time (probably
    // just before the current composition time, but due to previously set
    // negative bias, it could be just after the current composition time too).
    // If the inter-frame time is almost exactly equal to (a multiple of)
    // the inter-composition time, then we're in a dangerous situation because
    // jitter might cause frames to fall one side or the other of the
    // composition times, causing many frames to be skipped or duplicated.
    // Try to prevent that by adding a negative bias to the frame times during
    // the next composite; that should ensure the next frame's time is treated
    // as falling just before a composite time.
    return ImageHost::BIAS_NEGATIVE;
  }
  if (!aNextImageTime.IsNull() &&
      aNextImageTime - aCompositionTime < threshold &&
      aNextImageTime - aCompositionTime > -threshold) {
    // The next frame's time is very close to our composition time (probably
    // just after the current composition time, but due to previously set
    // positive bias, it could be just before the current composition time too).
    // We're in a dangerous situation because jitter might cause frames to
    // fall one side or the other of the composition times, causing many frames
    // to be skipped or duplicated.
    // Try to prevent that by adding a negative bias to the frame times during
    // the next composite; that should ensure the next frame's time is treated
    // as falling just before a composite time.
    return ImageHost::BIAS_POSITIVE;
  }
  return ImageHost::BIAS_NONE;
}

int ImageHost::ChooseImageIndex() const
{
  if (!GetCompositor() || mImages.IsEmpty()) {
    return -1;
  }
  TimeStamp now = GetCompositor()->GetCompositionTime();

  if (now.IsNull()) {
    // Not in a composition, so just return the last image we composited
    // (if it's one of the current images).
    for (uint32_t i = 0; i < mImages.Length(); ++i) {
      if (mImages[i].mFrameID == mLastFrameID &&
          mImages[i].mProducerID == mLastProducerID) {
        return i;
      }
    }
    return -1;
  }

  uint32_t result = 0;
  while (result + 1 < mImages.Length() &&
      GetBiasedTime(mImages[result + 1].mTimeStamp, mBias) <= now) {
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
ImageHost::Composite(LayerComposite* aLayer,
                     EffectChain& aEffectChain,
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
  int imageIndex = ChooseImageIndex();
  if (imageIndex < 0) {
    return;
  }

  if (uint32_t(imageIndex) + 1 < mImages.Length()) {
    GetCompositor()->CompositeUntil(mImages[imageIndex + 1].mTimeStamp + TimeDuration::FromMilliseconds(BIAS_TIME_MS));
  }

  TimedImage* img = &mImages[imageIndex];
  // Make sure the front buffer has a compositor
  img->mFrontBuffer->SetCompositor(GetCompositor());
  if (img->mTextureSource) {
    img->mTextureSource->SetCompositor(GetCompositor());
  }

  {
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

    if (!GetCompositor()->SupportsEffect(effect->mType)) {
      return;
    }

    if (mLastFrameID != img->mFrameID || mLastProducerID != img->mProducerID) {
      if (mImageContainer) {
        aLayer->GetLayerManager()->
            AppendImageCompositeNotification(ImageCompositeNotification(
                mImageContainer, nullptr,
                img->mTimeStamp, GetCompositor()->GetCompositionTime(),
                img->mFrameID, img->mProducerID));
      }
      mLastFrameID = img->mFrameID;
      mLastProducerID = img->mProducerID;
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

  // Update mBias last. This can change which frame ChooseImage(Index) would
  // return, and we don't want to do that until we've finished compositing
  // since callers of ChooseImage(Index) assume the same image will be chosen
  // during a given composition. This must happen after autoLock's
  // destructor!
  mBias = UpdateBias(
      GetCompositor()->GetCompositionTime(), mImages[imageIndex].mTimeStamp,
      uint32_t(imageIndex + 1) < mImages.Length() ?
          mImages[imageIndex + 1].mTimeStamp : TimeStamp(),
      mBias);
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

void
ImageHost::SetImageContainer(ImageContainerParent* aImageContainer)
{
  if (mImageContainer) {
    mImageContainer->mImageHosts.RemoveElement(this);
  }
  mImageContainer = aImageContainer;
  if (mImageContainer) {
    mImageContainer->mImageHosts.AppendElement(this);
  }
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
ImageHostOverlay::Composite(LayerComposite* aLayer,
                            EffectChain& aEffectChain,
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
