/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_BASICCANVASLAYER_H
#define GFX_BASICCANVASLAYER_H

#include "BasicLayersImpl.h"
#include "nsXULAppAPI.h"
#include "BasicLayers.h"
#include "BasicImplData.h"
#include "mozilla/layers/CanvasClient.h"
#include "mozilla/Preferences.h"

#include "gfxPlatform.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

class CanvasClient2D;
class CanvasClientWebGL;

class BasicCanvasLayer : public CanvasLayer,
                         public BasicImplData
{
public:
  BasicCanvasLayer(BasicLayerManager* aLayerManager) :
    CanvasLayer(aLayerManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicCanvasLayer);
    mForceReadback = Preferences::GetBool("webgl.force-layers-readback", false);
  }
  virtual ~BasicCanvasLayer()
  {
    MOZ_COUNT_DTOR(BasicCanvasLayer);
  }

  virtual void SetVisibleRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    CanvasLayer::SetVisibleRegion(aRegion);
  }

  virtual void Initialize(const Data& aData);
  virtual void Paint(gfxContext* aContext, Layer* aMaskLayer);

  virtual void PaintWithOpacity(gfxContext* aContext,
                                float aOpacity,
                                Layer* aMaskLayer);

protected:
  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }
  void UpdateSurface(gfxASurface* aDestSurface = nullptr, Layer* aMaskLayer = nullptr);

  nsRefPtr<gfxASurface> mSurface;
  nsRefPtr<mozilla::gl::GLContext> mGLContext;
  mozilla::RefPtr<mozilla::gfx::DrawTarget> mDrawTarget;
  
  uint32_t mCanvasFramebuffer;

  bool mIsGLAlphaPremult;
  bool mNeedsYFlip;
  bool mForceReadback;

  nsRefPtr<gfxImageSurface> mCachedTempSurface;
  gfxIntSize mCachedSize;
  gfxImageFormat mCachedFormat;

  gfxImageSurface* GetTempSurface(const gfxIntSize& aSize, const gfxImageFormat aFormat)
  {
    if (!mCachedTempSurface ||
        aSize.width != mCachedSize.width ||
        aSize.height != mCachedSize.height ||
        aFormat != mCachedFormat)
    {
      mCachedTempSurface = new gfxImageSurface(aSize, aFormat);
      mCachedSize = aSize;
      mCachedFormat = aFormat;
    }

    MOZ_ASSERT(mCachedTempSurface->Stride() == mCachedTempSurface->Width() * 4);
    return mCachedTempSurface;
  }

  void DiscardTempSurface()
  {
    mCachedTempSurface = nullptr;
  }

  friend class CanvasClient2D;
  friend class CanvasClientWebGL;
};

class BasicShadowableCanvasLayer : public BasicCanvasLayer,
                                   public BasicShadowableLayer
{
public:
  BasicShadowableCanvasLayer(BasicShadowLayerManager* aManager) :
    BasicCanvasLayer(aManager),
    mBufferIsOpaque(false),
    mCanvasClient(nullptr)
  {
    MOZ_COUNT_CTOR(BasicShadowableCanvasLayer);
  }
  virtual ~BasicShadowableCanvasLayer()
  {
    MOZ_COUNT_DTOR(BasicShadowableCanvasLayer);
  }

  virtual void Initialize(const Data& aData);
  virtual void Paint(gfxContext* aContext, Layer* aMaskLayer);

  virtual void FillSpecificAttributes(SpecificLayerAttributes& aAttrs)
  {
    aAttrs = CanvasLayerAttributes(mFilter, mBounds);
  }

  virtual Layer* AsLayer() { return this; }
  virtual ShadowableLayer* AsShadowableLayer() { return this; }

  /*virtual void SetBackBuffer(const SurfaceDescriptor& aBuffer)
  {
    NS_ERROR("Should not be called.");
  }*/

  virtual void Disconnect()
  {
    mCanvasClient = nullptr;
    BasicShadowableLayer::Disconnect();
  }

  virtual CompositableClient* GetCompositableClient() MOZ_OVERRIDE
  {
    return mCanvasClient;
  }
private:
  BasicShadowLayerManager* BasicManager()
  {
    return static_cast<BasicShadowLayerManager*>(mManager);
  }

  CompositableType GetCompositableClientType()
  {
    if (mGLContext && XRE_GetProcessType() == GeckoProcessType_Default) {
      return BUFFER_IMAGE_BUFFERED;
    }
    return BUFFER_IMAGE_SINGLE;
  }

  bool mBufferIsOpaque;
  RefPtr<CanvasClient> mCanvasClient;
};

}
}

#endif
