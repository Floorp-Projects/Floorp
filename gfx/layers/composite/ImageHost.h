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
#include "mozilla/gfx/Point.h"          // for Point
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/gfx/Types.h"          // for Filter
#include "mozilla/layers/CompositorTypes.h"  // for TextureInfo, etc
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/LayersTypes.h"  // for LayerRenderState, etc
#include "mozilla/layers/TextureHost.h"  // for TextureHost, etc
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsRect.h"                     // for mozilla::gfx::IntRect
#include "nscore.h"                     // for nsACString

class nsIntRegion;

namespace mozilla {
namespace gfx {
class Matrix4x4;
} // namespace gfx
namespace layers {

class Compositor;
struct EffectChain;
class ImageContainerParent;

/**
 * ImageHost. Works with ImageClientSingle and ImageClientBuffered
 */
class ImageHost : public CompositableHost
{
public:
  explicit ImageHost(const TextureInfo& aTextureInfo);
  ~ImageHost();

  virtual CompositableType GetType() override { return mTextureInfo.mCompositableType; }

  virtual void Composite(LayerComposite* aLayer,
                         EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::Filter& aFilter,
                         const gfx::Rect& aClipRect,
                         const nsIntRegion* aVisibleRegion = nullptr) override;

  virtual void UseTextureHost(const nsTArray<TimedTexture>& aTextures) override;

  virtual void RemoveTextureHost(TextureHost* aTexture) override;

  virtual TextureHost* GetAsTextureHost(gfx::IntRect* aPictureRect = nullptr) override;

  virtual void Attach(Layer* aLayer,
                      Compositor* aCompositor,
                      AttachFlags aFlags = NO_FLAGS) override;

  virtual void SetCompositor(Compositor* aCompositor) override;

  virtual void SetImageContainer(ImageContainerParent* aImageContainer) override;

  gfx::IntSize GetImageSize() const override;

  virtual LayerRenderState GetRenderState() override;

  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix) override;

  virtual void Dump(std::stringstream& aStream,
                    const char* aPrefix = "",
                    bool aDumpHtml = false) override;

  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override;

  virtual bool Lock() override;

  virtual void Unlock() override;

  virtual already_AddRefed<TexturedEffect> GenEffect(const gfx::Filter& aFilter) override;

  int32_t GetFrameID()
  {
    const TimedImage* img = ChooseImage();
    return img ? img->mFrameID : -1;
  }

  enum Bias {
    // Don't apply bias to frame times
    BIAS_NONE,
    // Apply a negative bias to frame times to keep them before the vsync time
    BIAS_NEGATIVE,
    // Apply a positive bias to frame times to keep them after the vsync time
    BIAS_POSITIVE,
  };

protected:
  struct TimedImage {
    CompositableTextureHostRef mFrontBuffer;
    CompositableTextureSourceRef mTextureSource;
    TimeStamp mTimeStamp;
    gfx::IntRect mPictureRect;
    int32_t mFrameID;
    int32_t mProducerID;
  };

  /**
   * ChooseImage is guaranteed to return the same TimedImage every time it's
   * called during the same composition, up to the end of Composite() ---
   * it depends only on mImages, mCompositor->GetCompositionTime(), and mBias.
   * mBias is updated at the end of Composite().
   */
  const TimedImage* ChooseImage() const;
  TimedImage* ChooseImage();
  int ChooseImageIndex() const;

  nsTArray<TimedImage> mImages;
  // Weak reference, will be null if mImageContainer has been destroyed.
  ImageContainerParent* mImageContainer;
  int32_t mLastFrameID;
  int32_t mLastProducerID;
  /**
   * Bias to apply to the next frame.
   */
  Bias mBias;

  bool mLocked;
};

#ifdef MOZ_WIDGET_GONK

/**
 * ImageHostOverlay works with ImageClientOverlay
 */
class ImageHostOverlay : public CompositableHost {
public:
  ImageHostOverlay(const TextureInfo& aTextureInfo);
  ~ImageHostOverlay();

  virtual CompositableType GetType() { return mTextureInfo.mCompositableType; }

  virtual void Composite(LayerComposite* aLayer,
                         EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::Filter& aFilter,
                         const gfx::Rect& aClipRect,
                         const nsIntRegion* aVisibleRegion = nullptr) override;
  virtual LayerRenderState GetRenderState() override;
  virtual void UseOverlaySource(OverlaySource aOverlay,
                                const gfx::IntRect& aPictureRect) override;
  virtual gfx::IntSize GetImageSize() const override;
  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix);
protected:
  gfx::IntRect mPictureRect;
  OverlaySource mOverlay;
};

#endif

} // namespace layers
} // namespace mozilla

#endif
