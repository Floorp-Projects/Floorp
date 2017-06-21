/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_IMAGEHOST_H
#define MOZILLA_GFX_IMAGEHOST_H

#include <stdio.h>                      // for FILE
#include "CompositableHost.h"           // for CompositableHost
#include "mozilla/Attributes.h"         // for override
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/gfx/MatrixFwd.h"      // for Matrix4x4
#include "mozilla/gfx/Point.h"          // for Point
#include "mozilla/gfx/Polygon.h"        // for Polygon
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/gfx/Types.h"          // for SamplingFilter
#include "mozilla/layers/CompositorTypes.h"  // for TextureInfo, etc
#include "mozilla/layers/ImageComposite.h"  // for ImageComposite
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/LayersTypes.h"  // for LayerRenderState, etc
#include "mozilla/layers/TextureHost.h"  // for TextureHost, etc
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsRect.h"                     // for mozilla::gfx::IntRect
#include "nsRegionFwd.h"                // for nsIntRegion
#include "nscore.h"                     // for nsACString

namespace mozilla {
namespace layers {

class Compositor;
struct EffectChain;
class HostLayerManager;

/**
 * ImageHost. Works with ImageClientSingle and ImageClientBuffered
 */
class ImageHost : public CompositableHost,
                  public ImageComposite
{
public:
  explicit ImageHost(const TextureInfo& aTextureInfo);
  ~ImageHost();

  virtual CompositableType GetType() override { return mTextureInfo.mCompositableType; }
  virtual ImageHost* AsImageHost() override { return this; }

  virtual void Composite(Compositor* aCompositor,
                         LayerComposite* aLayer,
                         EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::SamplingFilter aSamplingFilter,
                         const gfx::IntRect& aClipRect,
                         const nsIntRegion* aVisibleRegion = nullptr,
                         const Maybe<gfx::Polygon>& aGeometry = Nothing()) override;

  virtual void UseTextureHost(const nsTArray<TimedTexture>& aTextures) override;

  virtual void RemoveTextureHost(TextureHost* aTexture) override;

  virtual TextureHost* GetAsTextureHost(gfx::IntRect* aPictureRect = nullptr) override;

  virtual void Attach(Layer* aLayer,
                      TextureSourceProvider* aProvider,
                      AttachFlags aFlags = NO_FLAGS) override;

  virtual void SetTextureSourceProvider(TextureSourceProvider* aProvider) override;

  gfx::IntSize GetImageSize() const override;

  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix) override;

  virtual void Dump(std::stringstream& aStream,
                    const char* aPrefix = "",
                    bool aDumpHtml = false) override;

  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override;

  virtual bool Lock() override;

  virtual void Unlock() override;

  virtual already_AddRefed<TexturedEffect> GenEffect(const gfx::SamplingFilter aSamplingFilter) override;

  void SetCurrentTextureHost(TextureHost* aTexture);

  virtual void CleanupResources() override;

  bool IsOpaque();

  struct RenderInfo {
    int imageIndex;
    TimedImage* img;
    RefPtr<TextureHost> host;

    RenderInfo() : imageIndex(-1), img(nullptr)
    {}
  };

  // Acquire rendering information for the current frame.
  bool PrepareToRender(TextureSourceProvider* aProvider, RenderInfo* aOutInfo);

  // Acquire the TextureSource for the currently prepared frame.
  RefPtr<TextureSource> AcquireTextureSource(const RenderInfo& aInfo);

  // Send ImageComposite notifications and update the ChooseImage bias.
  void FinishRendering(const RenderInfo& aInfo);

  // This should only be called inside a lock, or during rendering. It is
  // infallible to enforce this.
  TextureHost* CurrentTextureHost() const {
    MOZ_ASSERT(mCurrentTextureHost);
    return mCurrentTextureHost;
  }

protected:
  // ImageComposite
  virtual TimeStamp GetCompositionTime() const override;

  // Use a simple RefPtr because the same texture is already held by a
  // a CompositableTextureHostRef in the array of TimedImage.
  // See the comment in CompositableTextureRef for more details.
  RefPtr<TextureHost> mCurrentTextureHost;
  CompositableTextureSourceRef mCurrentTextureSource;
  // When doing texture uploads it's best to alternate between two (or three)
  // texture sources so that the texture we upload to isn't being used by
  // the GPU to composite the previous frame.
  RefPtr<TextureSource> mExtraTextureSource;

  bool mLocked;
};

} // namespace layers
} // namespace mozilla

#endif
