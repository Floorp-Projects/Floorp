/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_IMAGEHOST_H
#define MOZILLA_GFX_IMAGEHOST_H

#include <stdio.h>                      // for FILE
#include "CompositableHost.h"           // for CompositableHost
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
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
#include "nsRect.h"                     // for nsIntRect
#include "nscore.h"                     // for nsACString

class nsIntRegion;

namespace mozilla {
namespace gfx {
class Matrix4x4;
}
namespace layers {

class Compositor;
class ISurfaceAllocator;
struct EffectChain;

/**
 * ImageHost. Works with ImageClientSingle and ImageClientBuffered
 */
class ImageHost : public CompositableHost
{
public:
  ImageHost(const TextureInfo& aTextureInfo);
  ~ImageHost();

  virtual CompositableType GetType() { return mTextureInfo.mCompositableType; }

  virtual void Composite(EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::Filter& aFilter,
                         const gfx::Rect& aClipRect,
                         const nsIntRegion* aVisibleRegion = nullptr,
                         TiledLayerProperties* aLayerProperties = nullptr) MOZ_OVERRIDE;

  virtual void UseTextureHost(TextureHost* aTexture) MOZ_OVERRIDE;

  virtual void RemoveTextureHost(TextureHost* aTexture) MOZ_OVERRIDE;

  virtual TextureHost* GetAsTextureHost() MOZ_OVERRIDE;

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

  virtual void SetPictureRect(const nsIntRect& aPictureRect) MOZ_OVERRIDE
  {
    mPictureRect = aPictureRect;
    mHasPictureRect = true;
  }

  virtual LayerRenderState GetRenderState() MOZ_OVERRIDE;

  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix);

#ifdef MOZ_DUMP_PAINTING
  virtual void Dump(std::stringstream& aStream,
                    const char* aPrefix = "",
                    bool aDumpHtml = false) MOZ_OVERRIDE;

  virtual TemporaryRef<gfx::DataSourceSurface> GetAsSurface() MOZ_OVERRIDE;
#endif

  virtual bool Lock() MOZ_OVERRIDE;

  virtual void Unlock() MOZ_OVERRIDE;

  virtual TemporaryRef<NewTextureSource> GetTextureSource();

  virtual TemporaryRef<TexturedEffect> GenEffect(const gfx::Filter& aFilter) MOZ_OVERRIDE;

protected:

  RefPtr<TextureHost> mFrontBuffer;
  nsIntRect mPictureRect;
  bool mHasPictureRect;
  bool mLocked;
};

}
}

#endif
