/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderImageHost.h"

#include "LayersLogging.h"              // for AppendToString

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

WebRenderImageHost::WebRenderImageHost(const TextureInfo& aTextureInfo)
  : CompositableHost(aTextureInfo)
  , ImageComposite()
{}

WebRenderImageHost::~WebRenderImageHost()
{
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
}

void
WebRenderImageHost::CleanupResources()
{
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
  // XXX temporary workaround
  return TimeStamp::Now();
}

TextureHost*
WebRenderImageHost::GetAsTextureHost(IntRect* aPictureRect)
{
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
  return nullptr;
}

void WebRenderImageHost::Attach(Layer* aLayer,
                       Compositor* aCompositor,
                       AttachFlags aFlags)
{
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
}

void
WebRenderImageHost::Composite(LayerComposite* aLayer,
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
WebRenderImageHost::SetCompositor(Compositor* aCompositor)
{
  if (mCompositor != aCompositor) {
    for (auto& img : mImages) {
      img.mTextureHost->SetCompositor(aCompositor);
    }
  }
  CompositableHost::SetCompositor(aCompositor);
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

LayerRenderState
WebRenderImageHost::GetRenderState()
{
  return LayerRenderState();
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

} // namespace layers
} // namespace mozilla
