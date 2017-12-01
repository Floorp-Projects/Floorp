/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CONTENTHOST_H
#define GFX_CONTENTHOST_H

#include <stdint.h>                     // for uint32_t
#include <stdio.h>                      // for FILE
#include "mozilla-config.h"             // for MOZ_DUMP_PAINTING
#include "CompositableHost.h"           // for CompositableHost, etc
#include "RotatedBuffer.h"              // for RotatedBuffer, etc
#include "mozilla/Attributes.h"         // for override
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/gfx/BasePoint.h"      // for BasePoint
#include "mozilla/gfx/MatrixFwd.h"      // for Matrix4x4
#include "mozilla/gfx/Point.h"          // for Point
#include "mozilla/gfx/Polygon.h"        // for Polygon
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/gfx/Types.h"          // for SamplingFilter
#include "mozilla/layers/CompositorTypes.h"  // for TextureInfo, etc
#include "mozilla/layers/ContentClient.h"  // for ContentClient
#include "mozilla/layers/ISurfaceAllocator.h"  // for ISurfaceAllocator
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/LayersTypes.h"  // for etc
#include "mozilla/layers/TextureHost.h"  // for TextureHost
#include "mozilla/mozalloc.h"           // for operator delete
#include "mozilla/UniquePtr.h"          // for UniquePtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for mozilla::gfx::IntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "nsTArray.h"                   // for nsTArray
#include "nscore.h"                     // for nsACString

namespace mozilla {
namespace layers {
class Compositor;
class ThebesBufferData;
struct EffectChain;

struct TexturedEffect;

/**
 * ContentHosts are used for compositing Painted layers, always matched by a
 * ContentClient of the same type.
 *
 * ContentHosts support only UpdateThebes(), not Update().
 */
class ContentHost : public CompositableHost
{
public:
  virtual bool UpdateThebes(const ThebesBufferData& aData,
                            const nsIntRegion& aUpdated,
                            const nsIntRegion& aOldValidRegionBack) = 0;

  virtual void SetPaintWillResample(bool aResample) { mPaintWillResample = aResample; }
  bool PaintWillResample() { return mPaintWillResample; }

  // We use this to allow TiledContentHost to invalidate regions where
  // tiles are fading in.
  virtual void AddAnimationInvalidation(nsIntRegion& aRegion) { }

  virtual gfx::IntRect GetBufferRect() {
    MOZ_ASSERT_UNREACHABLE("Must be implemented in derived class");
    return gfx::IntRect();
  }
  virtual ContentHost* AsContentHost() { return this; }

protected:
  explicit ContentHost(const TextureInfo& aTextureInfo)
    : CompositableHost(aTextureInfo)
    , mPaintWillResample(false)
  {}

  bool mPaintWillResample;
};

/**
 * Base class for non-tiled ContentHosts.
 *
 * Ownership of the SurfaceDescriptor and the resources it represents is passed
 * from the ContentClient to the ContentHost when the TextureClient/Hosts are
 * created, that is recevied here by SetTextureHosts which assigns one or two
 * texture hosts (for single and double buffering) to the ContentHost.
 *
 * It is the responsibility of the ContentHost to destroy its resources when
 * they are recreated or the ContentHost dies.
 */
class ContentHostBase : public ContentHost
{
public:
  typedef ContentClient::ContentType ContentType;
  typedef ContentClient::PaintState PaintState;

  explicit ContentHostBase(const TextureInfo& aTextureInfo);
  virtual ~ContentHostBase();

  virtual gfx::IntRect GetBufferRect() override { return mBufferRect; }

  virtual nsIntPoint GetOriginOffset()
  {
    return mBufferRect.TopLeft() - mBufferRotation;
  }

  gfx::IntPoint GetBufferRotation()
  {
    return mBufferRotation.ToUnknownPoint();
  }

protected:
  gfx::IntRect mBufferRect;
  nsIntPoint mBufferRotation;
  bool mInitialised;
};

/**
 * Shared ContentHostBase implementation for content hosts that
 * use up to two TextureHosts.
 */
class ContentHostTexture : public ContentHostBase
{
public:
  explicit ContentHostTexture(const TextureInfo& aTextureInfo)
    : ContentHostBase(aTextureInfo)
    , mLocked(false)
    , mReceivedNewHost(false)
  { }

  virtual void Composite(Compositor* aCompositor,
                         LayerComposite* aLayer,
                         EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::SamplingFilter aSamplingFilter,
                         const gfx::IntRect& aClipRect,
                         const nsIntRegion* aVisibleRegion = nullptr,
                         const Maybe<gfx::Polygon>& aGeometry = Nothing()) override;

  virtual void SetTextureSourceProvider(TextureSourceProvider* aProvider) override;

  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override;

  virtual void Dump(std::stringstream& aStream,
                    const char* aPrefix="",
                    bool aDumpHtml=false) override;

  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix) override;

  virtual void UseTextureHost(const nsTArray<TimedTexture>& aTextures) override;
  virtual void UseComponentAlphaTextures(TextureHost* aTextureOnBlack,
                                         TextureHost* aTextureOnWhite) override;

  virtual bool Lock() override {
    MOZ_ASSERT(!mLocked);
    if (!mTextureHost) {
      return false;
    }
    if (!mTextureHost->Lock()) {
      return false;
    }

    if (mTextureHostOnWhite && !mTextureHostOnWhite->Lock()) {
      return false;
    }

    mLocked = true;
    return true;
  }
  virtual void Unlock() override {
    MOZ_ASSERT(mLocked);
    mTextureHost->Unlock();
    if (mTextureHostOnWhite) {
      mTextureHostOnWhite->Unlock();
    }
    mLocked = false;
  }

  bool HasComponentAlpha() const {
    return !!mTextureHostOnWhite;
  }

  RefPtr<TextureSource> AcquireTextureSource();
  RefPtr<TextureSource> AcquireTextureSourceOnWhite();

  ContentHostTexture* AsContentHostTexture() override { return this; }

  virtual already_AddRefed<TexturedEffect> GenEffect(const gfx::SamplingFilter aSamplingFilter) override;

protected:
  CompositableTextureHostRef mTextureHost;
  CompositableTextureHostRef mTextureHostOnWhite;
  CompositableTextureSourceRef mTextureSource;
  CompositableTextureSourceRef mTextureSourceOnWhite;
  bool mLocked;
  bool mReceivedNewHost;
};

/**
 * Double buffering is implemented by swapping the front and back TextureHosts.
 * We assume that whenever we use double buffering, then we have
 * render-to-texture and thus no texture upload to do.
 */
class ContentHostDoubleBuffered : public ContentHostTexture
{
public:
  explicit ContentHostDoubleBuffered(const TextureInfo& aTextureInfo)
    : ContentHostTexture(aTextureInfo)
  {}

  virtual ~ContentHostDoubleBuffered() {}

  virtual CompositableType GetType() { return CompositableType::CONTENT_DOUBLE; }

  virtual bool UpdateThebes(const ThebesBufferData& aData,
                            const nsIntRegion& aUpdated,
                            const nsIntRegion& aOldValidRegionBack);

protected:
  nsIntRegion mValidRegionForNextBackBuffer;
};

/**
 * Single buffered, therefore we must synchronously upload the image from the
 * TextureHost in the layers transaction (i.e., in UpdateThebes).
 */
class ContentHostSingleBuffered : public ContentHostTexture
{
public:
  explicit ContentHostSingleBuffered(const TextureInfo& aTextureInfo)
    : ContentHostTexture(aTextureInfo)
  {}
  virtual ~ContentHostSingleBuffered() {}

  virtual CompositableType GetType() { return CompositableType::CONTENT_SINGLE; }

  virtual bool UpdateThebes(const ThebesBufferData& aData,
                            const nsIntRegion& aUpdated,
                            const nsIntRegion& aOldValidRegionBack);
};

} // namespace layers
} // namespace mozilla

#endif
