/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderImageHost.h"

#include "LayersLogging.h"
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/Effects.h"     // for TexturedEffect, Effect, etc
#include "mozilla/layers/LayerManagerComposite.h"     // for TexturedEffect, Effect, etc
#include "mozilla/layers/WebRenderBridgeParent.h"
#include "mozilla/layers/WebRenderCompositableHolder.h"
#include "nsAString.h"
#include "nsDebug.h"                    // for NS_WARNING, NS_ASSERTION
#include "nsPrintfCString.h"            // for nsPrintfCString
#include "nsString.h"                   // for nsAutoCString

namespace mozilla {

using namespace gfx;

namespace layers {

class ISurfaceAllocator;

WebRenderImageHost::WebRenderImageHost(const TextureInfo& aTextureInfo)
  : CompositableHost(aTextureInfo)
  , ImageComposite()
  , mWrBridge(nullptr)
  , mWrBridgeBindings(0)
{}

WebRenderImageHost::~WebRenderImageHost()
{
  MOZ_ASSERT(!mWrBridge);
}

void
WebRenderImageHost::UseTextureHost(const nsTArray<TimedTexture>& aTextures)
{
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

  if (mWrBridge && GetAsyncRef()) {
    mWrBridge->ScheduleComposition();
  }

  // Video producers generally send replacement images with the same frameID but
  // slightly different timestamps in order to sync with the audio clock. This
  // means that any CompositeUntil() call we made in Composite() may no longer
  // guarantee that we'll composite until the next frame is ready. Fix that here.
  if (mWrBridge && mLastFrameID >= 0) {
    MOZ_ASSERT(mWrBridge->CompositableHolder());
    for (size_t i = 0; i < mImages.Length(); ++i) {
      bool frameComesAfter = mImages[i].mFrameID > mLastFrameID ||
                             mImages[i].mProducerID != mLastProducerID;
      if (frameComesAfter && !mImages[i].mTimeStamp.IsNull()) {
        mWrBridge->CompositableHolder()->CompositeUntil(mImages[i].mTimeStamp +
                           TimeDuration::FromMilliseconds(BIAS_TIME_MS));
        break;
      }
    }
  }
}

void
WebRenderImageHost::UseComponentAlphaTextures(TextureHost* aTextureOnBlack,
                                              TextureHost* aTextureOnWhite)
{
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
}

void
WebRenderImageHost::CleanupResources()
{
  nsTArray<TimedImage> newImages;
  mImages.SwapElements(newImages);
  newImages.Clear();
  SetCurrentTextureHost(nullptr);
}

void
WebRenderImageHost::RemoveTextureHost(TextureHost* aTexture)
{
  CompositableHost::RemoveTextureHost(aTexture);

  for (int32_t i = mImages.Length() - 1; i >= 0; --i) {
    if (mImages[i].mTextureHost == aTexture) {
      aTexture->UnbindTextureSource();
      mImages.RemoveElementAt(i);
    }
  }
}

TimeStamp
WebRenderImageHost::GetCompositionTime() const
{
  TimeStamp time;
  if (mWrBridge) {
    MOZ_ASSERT(mWrBridge->CompositableHolder());
    time = mWrBridge->CompositableHolder()->GetCompositionTime();
  }
  return time;
}

TextureHost*
WebRenderImageHost::GetAsTextureHost(IntRect* aPictureRect)
{
  TimedImage* img = ChooseImage();
  if (img) {
    return img->mTextureHost;
  }
  return nullptr;
}

TextureHost*
WebRenderImageHost::GetAsTextureHostForComposite()
{
  int imageIndex = ChooseImageIndex();
  if (imageIndex < 0) {
    SetCurrentTextureHost(nullptr);
    return nullptr;
  }

  if (mWrBridge && uint32_t(imageIndex) + 1 < mImages.Length()) {
    MOZ_ASSERT(mWrBridge->CompositableHolder());
    mWrBridge->CompositableHolder()->CompositeUntil(mImages[imageIndex + 1].mTimeStamp + TimeDuration::FromMilliseconds(BIAS_TIME_MS));
  }

  TimedImage* img = &mImages[imageIndex];

  if (mLastFrameID != img->mFrameID || mLastProducerID != img->mProducerID) {
    mLastFrameID = img->mFrameID;
    mLastProducerID = img->mProducerID;
  }
  SetCurrentTextureHost(img->mTextureHost);
  return mCurrentTextureHost;
}

void
WebRenderImageHost::SetCurrentTextureHost(TextureHost* aTexture)
{
  if (aTexture == mCurrentTextureHost.get()) {
    return;
  }

  if (mWrBridge &&
      !!mCurrentTextureHost &&
      mCurrentTextureHost != aTexture &&
      mCurrentTextureHost->AsWebRenderTextureHost()) {
    MOZ_ASSERT(mWrBridge->CompositableHolder());
    wr::PipelineId piplineId = mWrBridge->PipelineId();
    wr::Epoch epoch = mWrBridge->WrEpoch();
    mWrBridge->CompositableHolder()->HoldExternalImage(
      piplineId,
      epoch,
      mCurrentTextureHost->AsWebRenderTextureHost());
  }

  mCurrentTextureHost = aTexture;
}

void WebRenderImageHost::Attach(Layer* aLayer,
                       TextureSourceProvider* aProvider,
                       AttachFlags aFlags)
{
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
}

void
WebRenderImageHost::Composite(Compositor* aCompositor,
                     LayerComposite* aLayer,
                     EffectChain& aEffectChain,
                     float aOpacity,
                     const gfx::Matrix4x4& aTransform,
                     const gfx::SamplingFilter aSamplingFilter,
                     const gfx::IntRect& aClipRect,
                     const nsIntRegion* aVisibleRegion,
                     const Maybe<gfx::Polygon>& aGeometry)
{
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
}

void
WebRenderImageHost::SetTextureSourceProvider(TextureSourceProvider* aProvider)
{
  if (mTextureSourceProvider != aProvider) {
    for (auto& img : mImages) {
      img.mTextureHost->SetTextureSourceProvider(aProvider);
    }
  }
  CompositableHost::SetTextureSourceProvider(aProvider);
}

void
WebRenderImageHost::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  aStream << aPrefix;
  aStream << nsPrintfCString("WebRenderImageHost (0x%p)", this).get();

  nsAutoCString pfx(aPrefix);
  pfx += "  ";
  for (auto& img : mImages) {
    aStream << "\n";
    img.mTextureHost->PrintInfo(aStream, pfx.get());
    AppendToString(aStream, img.mPictureRect, " [picture-rect=", "]");
  }
}

void
WebRenderImageHost::Dump(std::stringstream& aStream,
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
WebRenderImageHost::GetAsSurface()
{
  TimedImage* img = ChooseImage();
  if (img) {
    return img->mTextureHost->GetAsSurface();
  }
  return nullptr;
}

bool
WebRenderImageHost::Lock()
{
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
  return false;
}

void
WebRenderImageHost::Unlock()
{
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
}

IntSize
WebRenderImageHost::GetImageSize() const
{
  const TimedImage* img = ChooseImage();
  if (img) {
    return IntSize(img->mPictureRect.width, img->mPictureRect.height);
  }
  return IntSize();
}

void
WebRenderImageHost::SetWrBridge(WebRenderBridgeParent* aWrBridge)
{
  // For image hosts created through ImageBridgeParent, there may be multiple
  // references to it due to the order of creation and freeing of layers by
  // the layer tree. However this should be limited to things such as video
  // which will not be reused across different WebRenderBridgeParent objects.
  MOZ_ASSERT(aWrBridge);
  MOZ_ASSERT(!mWrBridge || mWrBridge == aWrBridge);
  mWrBridge = aWrBridge;
  ++mWrBridgeBindings;
}

void
WebRenderImageHost::ClearWrBridge()
{
  MOZ_ASSERT(mWrBridgeBindings > 0);
  --mWrBridgeBindings;
  if (mWrBridgeBindings == 0) {
    SetCurrentTextureHost(nullptr);
    mWrBridge = nullptr;
  }
}

} // namespace layers
} // namespace mozilla
