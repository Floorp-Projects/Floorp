/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_COPYABLECANVASLAYER_H
#define GFX_COPYABLECANVASLAYER_H

#include "Layers.h"
#include "mozilla/layers/CanvasClient.h"
#include "mozilla/Preferences.h"

#include "gfxPlatform.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

class CanvasClient2D;
class CanvasClientWebGL;

/**
 * A shared CanvasLayer implementation that supports copying
 * its contents into a gfxASurface using UpdateSurface.
 */
class CopyableCanvasLayer : public CanvasLayer
{
public:
  CopyableCanvasLayer(LayerManager* aLayerManager, void *aImplData) :
    CanvasLayer(aLayerManager, aImplData)
  {
    MOZ_COUNT_CTOR(CopyableCanvasLayer);
    mForceReadback = Preferences::GetBool("webgl.force-layers-readback", false);
  }
  virtual ~CopyableCanvasLayer()
  {
    MOZ_COUNT_DTOR(CopyableCanvasLayer);
  }

  virtual void Initialize(const Data& aData);
  

protected:
  void PaintWithOpacity(gfxContext* aContext,
                        float aOpacity,
                        Layer* aMaskLayer,
                        gfxContext::GraphicsOperator aOperator = gfxContext::OPERATOR_OVER);

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
};

}
}

#endif
