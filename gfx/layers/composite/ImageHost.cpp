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
#include "mozilla/layers/LayerManagerComposite.h"     // for TexturedEffect, Effect, etc
#include "nsAString.h"
#include "nsDebug.h"                    // for NS_WARNING, NS_ASSERTION
#include "nsPrintfCString.h"            // for nsPrintfCString
#include "nsString.h"                   // for nsAutoCString

namespace mozilla {

using namespace gfx;

namespace layers {

class ISurfaceAllocator;

ImageHost::ImageHost(const TextureInfo& aTextureInfo)
  : CompositableHost(aTextureInfo)
  , ImageComposite()
  , mLocked(false)
{}

ImageHost::~ImageHost()
{
}

void
ImageHost::UseTextureHost(const nsTArray<TimedTexture>& aTextures)
{
  MOZ_ASSERT(!mLocked);

  CompositableHost::UseTextureHost(aTextures);
  MOZ_ASSERT(aTextures.Length() >= 1);

  nsTArray<TimedImage> newImages;

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
    img.mTextureHost = t.mTexture;
    img.mTimeStamp = t.mTimeStamp;
    img.mPictureRect = t.mPictureRect;
    img.mFrameID = t.mFrameID;
    img.mProducerID = t.mProducerID;
    img.mTextureHost->SetCropRect(img.mPictureRect);
    img.mTextureHost->Updated();
  }

  mImages.SwapElements(newImages);
  newImages.Clear();

  // If we only have one image we can upload it right away, otherwise we'll upload
  // on-demand during composition after we have picked the proper timestamp.
  if (mImages.Length() == 1) {
    SetCurrentTextureHost(mImages[0].mTextureHost);
  }

  HostLayerManager* lm = GetLayerManager();

  // Video producers generally send replacement images with the same frameID but
  // slightly different timestamps in order to sync with the audio clock. This
  // means that any CompositeUntil() call we made in Composite() may no longer
  // guarantee that we'll composite until the next frame is ready. Fix that here.
  if (lm && mLastFrameID >= 0) {
    for (size_t i = 0; i < mImages.Length(); ++i) {
      bool frameComesAfter = mImages[i].mFrameID > mLastFrameID ||
                             mImages[i].mProducerID != mLastProducerID;
      if (frameComesAfter && !mImages[i].mTimeStamp.IsNull()) {
        lm->CompositeUntil(mImages[i].mTimeStamp +
                           TimeDuration::FromMilliseconds(BIAS_TIME_MS));
        break;
      }
    }
  }
}

void
ImageHost::SetCurrentTextureHost(TextureHost* aTexture)
{
  if (aTexture == mCurrentTextureHost.get()) {
    return;
  }

  bool swapTextureSources = !!mCurrentTextureHost && !!mCurrentTextureSource
                            && mCurrentTextureHost->HasIntermediateBuffer();

  if (swapTextureSources) {
    auto dataSource = mCurrentTextureSource->AsDataTextureSource();
    if (dataSource) {
      // The current textureHost has an internal buffer in the form of the
      // DataTextureSource. Removing the ownership of the texture source
      // will enable the next texture host we bind to the texture source to
      // acquire it instead of creating a new one. This is desirable in
      // ImageHost because the current texture won't be used again with the
      // same content. It wouldn't be desirable with ContentHost for instance,
      // because the latter reuses the texture's valid regions.
      dataSource->SetOwner(nullptr);
    }

    RefPtr<TextureSource> tmp = mExtraTextureSource;
    mExtraTextureSource = mCurrentTextureSource.get();
    mCurrentTextureSource = tmp;
  } else {
    mExtraTextureSource = nullptr;
  }

  mCurrentTextureHost = aTexture;
  mCurrentTextureHost->PrepareTextureSource(mCurrentTextureSource);
}

void
ImageHost::CleanupResources()
{
  mExtraTextureSource = nullptr;
  mCurrentTextureSource = nullptr;
  mCurrentTextureHost = nullptr;
}

void
ImageHost::RemoveTextureHost(TextureHost* aTexture)
{
  MOZ_ASSERT(!mLocked);

  CompositableHost::RemoveTextureHost(aTexture);

  for (int32_t i = mImages.Length() - 1; i >= 0; --i) {
    if (mImages[i].mTextureHost == aTexture) {
      aTexture->UnbindTextureSource();
      mImages.RemoveElementAt(i);
    }
  }
}

TimeStamp
ImageHost::GetCompositionTime() const
{
  TimeStamp time;
  if (HostLayerManager* lm = GetLayerManager()) {
    time = lm->GetCompositionTime();
  }
  return time;
}

TextureHost*
ImageHost::GetAsTextureHost(IntRect* aPictureRect)
{
  TimedImage* img = ChooseImage();
  if (img) {
    SetCurrentTextureHost(img->mTextureHost);
  }
  if (aPictureRect && img) {
    *aPictureRect = img->mPictureRect;
  }
  return img ? img->mTextureHost.get() : nullptr;
}

void ImageHost::Attach(Layer* aLayer,
                       TextureSourceProvider* aProvider,
                       AttachFlags aFlags)
{
  CompositableHost::Attach(aLayer, aProvider, aFlags);
  for (auto& img : mImages) {
    img.mTextureHost->SetTextureSourceProvider(aProvider);
    img.mTextureHost->Updated();
  }
}

void
ImageHost::Composite(Compositor* aCompositor,
                     LayerComposite* aLayer,
                     EffectChain& aEffectChain,
                     float aOpacity,
                     const gfx::Matrix4x4& aTransform,
                     const gfx::SamplingFilter aSamplingFilter,
                     const gfx::IntRect& aClipRect,
                     const nsIntRegion* aVisibleRegion,
                     const Maybe<gfx::Polygon>& aGeometry)
{
  RenderInfo info;
  if (!PrepareToRender(aCompositor, &info)) {
    return;
  }

  TimedImage* img = info.img;

  {
    AutoLockCompositableHost autoLock(this);
    if (autoLock.Failed()) {
      NS_WARNING("failed to lock front buffer");
      return;
    }

    if (!mCurrentTextureHost->BindTextureSource(mCurrentTextureSource)) {
      return;
    }

    if (!mCurrentTextureSource) {
      // BindTextureSource above should have returned false!
      MOZ_ASSERT(false);
      return;
    }

    bool isAlphaPremultiplied =
        !(mCurrentTextureHost->GetFlags() & TextureFlags::NON_PREMULTIPLIED);
    RefPtr<TexturedEffect> effect =
        CreateTexturedEffect(mCurrentTextureHost,
            mCurrentTextureSource.get(), aSamplingFilter, isAlphaPremultiplied);
    if (!effect) {
      return;
    }

    if (!aCompositor->SupportsEffect(effect->mType)) {
      return;
    }

    DiagnosticFlags diagnosticFlags = DiagnosticFlags::IMAGE;
    if (effect->mType == EffectTypes::NV12) {
      diagnosticFlags |= DiagnosticFlags::NV12;
    } else if (effect->mType == EffectTypes::YCBCR) {
      diagnosticFlags |= DiagnosticFlags::YCBCR;
    }

    aEffectChain.mPrimaryEffect = effect;
    gfx::Rect pictureRect(0, 0, img->mPictureRect.width, img->mPictureRect.height);
    BigImageIterator* it = mCurrentTextureSource->AsBigImageIterator();
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
      MOZ_ASSERT(it->GetTileCount() == 1 || !mCurrentTextureSource->GetNextSibling(),
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
        if (img->mTextureHost->GetFlags() & TextureFlags::ORIGIN_BOTTOM_LEFT) {
          effect->mTextureCoords.y = effect->mTextureCoords.YMost();
          effect->mTextureCoords.height = -effect->mTextureCoords.height;
        }
        aCompositor->DrawGeometry(rect, aClipRect, aEffectChain,
                                  aOpacity, aTransform, aGeometry);
        aCompositor->DrawDiagnostics(diagnosticFlags | DiagnosticFlags::BIGIMAGE,
                                     rect, aClipRect, aTransform, mFlashCounter);
      } while (it->NextTile());
      it->EndBigImageIteration();
      // layer border
      aCompositor->DrawDiagnostics(diagnosticFlags, pictureRect,
                                   aClipRect, aTransform, mFlashCounter);
    } else {
      IntSize textureSize = mCurrentTextureSource->GetSize();
      effect->mTextureCoords = Rect(Float(img->mPictureRect.x) / textureSize.width,
                                    Float(img->mPictureRect.y) / textureSize.height,
                                    Float(img->mPictureRect.width) / textureSize.width,
                                    Float(img->mPictureRect.height) / textureSize.height);

      if (img->mTextureHost->GetFlags() & TextureFlags::ORIGIN_BOTTOM_LEFT) {
        effect->mTextureCoords.y = effect->mTextureCoords.YMost();
        effect->mTextureCoords.height = -effect->mTextureCoords.height;
      }

      aCompositor->DrawGeometry(pictureRect, aClipRect, aEffectChain,
                                aOpacity, aTransform, aGeometry);
      aCompositor->DrawDiagnostics(diagnosticFlags,
                                   pictureRect, aClipRect,
                                   aTransform, mFlashCounter);
    }
  }

  FinishRendering(info);
}

bool
ImageHost::PrepareToRender(TextureSourceProvider* aProvider, RenderInfo* aOutInfo)
{
  HostLayerManager* lm = GetLayerManager();
  if (!lm) {
    return false;
  }

  int imageIndex = ChooseImageIndex();
  if (imageIndex < 0) {
    return false;
  }

  if (uint32_t(imageIndex) + 1 < mImages.Length()) {
    lm->CompositeUntil(mImages[imageIndex + 1].mTimeStamp + TimeDuration::FromMilliseconds(BIAS_TIME_MS));
  }

  TimedImage* img = &mImages[imageIndex];
  img->mTextureHost->SetTextureSourceProvider(aProvider);
  SetCurrentTextureHost(img->mTextureHost);

  aOutInfo->imageIndex = imageIndex;
  aOutInfo->img = img;
  aOutInfo->host = mCurrentTextureHost;
  return true;
}

RefPtr<TextureSource>
ImageHost::AcquireTextureSource(const RenderInfo& aInfo)
{
  MOZ_ASSERT(aInfo.host == mCurrentTextureHost);
  if (!aInfo.host->AcquireTextureSource(mCurrentTextureSource)) {
    return nullptr;
  }
  return mCurrentTextureSource.get();
}

void
ImageHost::FinishRendering(const RenderInfo& aInfo)
{
  HostLayerManager* lm = GetLayerManager();
  TimedImage* img = aInfo.img;
  int imageIndex = aInfo.imageIndex;

  if (mLastFrameID != img->mFrameID || mLastProducerID != img->mProducerID) {
    if (mAsyncRef) {
      ImageCompositeNotificationInfo info;
      info.mImageBridgeProcessId = mAsyncRef.mProcessId;
      info.mNotification = ImageCompositeNotification(
        mAsyncRef.mHandle,
        img->mTimeStamp, lm->GetCompositionTime(),
        img->mFrameID, img->mProducerID);
      lm->AppendImageCompositeNotification(info);
    }
    mLastFrameID = img->mFrameID;
    mLastProducerID = img->mProducerID;
  }

  // Update mBias last. This can change which frame ChooseImage(Index) would
  // return, and we don't want to do that until we've finished compositing
  // since callers of ChooseImage(Index) assume the same image will be chosen
  // during a given composition. This must happen after autoLock's
  // destructor!
  mBias = UpdateBias(
      lm->GetCompositionTime(), mImages[imageIndex].mTimeStamp,
      uint32_t(imageIndex + 1) < mImages.Length() ?
          mImages[imageIndex + 1].mTimeStamp : TimeStamp(),
      mBias);
}

void
ImageHost::SetTextureSourceProvider(TextureSourceProvider* aProvider)
{
  if (mTextureSourceProvider != aProvider) {
    for (auto& img : mImages) {
      img.mTextureHost->SetTextureSourceProvider(aProvider);
    }
  }
  CompositableHost::SetTextureSourceProvider(aProvider);
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
    img.mTextureHost->PrintInfo(aStream, pfx.get());
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
    DumpTextureHost(aStream, img.mTextureHost);
    aStream << (aDumpHtml ? " </li></ul> " : " ");
  }
}

already_AddRefed<gfx::DataSourceSurface>
ImageHost::GetAsSurface()
{
  TimedImage* img = ChooseImage();
  if (img) {
    return img->mTextureHost->GetAsSurface();
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

  SetCurrentTextureHost(img->mTextureHost);

  if (!mCurrentTextureHost->Lock()) {
    return false;
  }
  mLocked = true;
  return true;
}

void
ImageHost::Unlock()
{
  MOZ_ASSERT(mLocked);

  if (mCurrentTextureHost) {
    mCurrentTextureHost->Unlock();
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

bool
ImageHost::IsOpaque()
{
  const TimedImage* img = ChooseImage();
  if (!img) {
    return false;
  }

  if (img->mPictureRect.width == 0 ||
      img->mPictureRect.height == 0 ||
      !img->mTextureHost) {
    return false;
  }

  gfx::SurfaceFormat format = img->mTextureHost->GetFormat();
  if (gfx::IsOpaque(format)) {
    return true;
  }
  return false;
}

already_AddRefed<TexturedEffect>
ImageHost::GenEffect(const gfx::SamplingFilter aSamplingFilter)
{
  TimedImage* img = ChooseImage();
  if (!img) {
    return nullptr;
  }
  SetCurrentTextureHost(img->mTextureHost);
  if (!mCurrentTextureHost->BindTextureSource(mCurrentTextureSource)) {
    return nullptr;
  }
  bool isAlphaPremultiplied = true;
  if (mCurrentTextureHost->GetFlags() & TextureFlags::NON_PREMULTIPLIED) {
    isAlphaPremultiplied = false;
  }

  return CreateTexturedEffect(mCurrentTextureHost,
                              mCurrentTextureSource,
                              aSamplingFilter,
                              isAlphaPremultiplied);
}

} // namespace layers
} // namespace mozilla
