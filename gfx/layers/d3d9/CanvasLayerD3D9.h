/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CANVASLAYERD3D9_H
#define GFX_CANVASLAYERD3D9_H

#include "LayerManagerD3D9.h"
#include "GLContext.h"
#include "gfxASurface.h"

namespace mozilla {
namespace layers {

class ShadowBufferD3D9;

class THEBES_API CanvasLayerD3D9 :
  public CanvasLayer,
  public LayerD3D9
{
public:
  CanvasLayerD3D9(LayerManagerD3D9 *aManager)
    : CanvasLayer(aManager, NULL)
    , LayerD3D9(aManager)
    , mDataIsPremultiplied(false)
    , mNeedsYFlip(false)
    , mHasAlpha(true)
  {
      mImplData = static_cast<LayerD3D9*>(this);
      aManager->deviceManager()->mLayersWithResources.AppendElement(this);
  }

  ~CanvasLayerD3D9();

  // CanvasLayer implementation
  virtual void Initialize(const Data& aData);

  // LayerD3D9 implementation
  virtual Layer* GetLayer();
  virtual void RenderLayer();
  virtual void CleanResources();
  virtual void LayerManagerDestroyed();

  void CreateTexture();

protected:
  typedef mozilla::gl::GLContext GLContext;

  void UpdateSurface();

  nsRefPtr<gfxASurface> mSurface;
  nsRefPtr<GLContext> mGLContext;
  nsRefPtr<IDirect3DTexture9> mTexture;
  RefPtr<gfx::DrawTarget> mDrawTarget;

  PRUint32 mCanvasFramebuffer;

  bool mDataIsPremultiplied;
  bool mNeedsYFlip;
  bool mHasAlpha;

  nsAutoArrayPtr<PRUint8> mCachedTempBlob;
  PRUint32 mCachedTempBlob_Size;

  PRUint8* GetTempBlob(const PRUint32 aSize)
  {
      if (!mCachedTempBlob || aSize != mCachedTempBlob_Size) {
          mCachedTempBlob = new PRUint8[aSize];
          mCachedTempBlob_Size = aSize;
      }

      return mCachedTempBlob;
  }

  void DiscardTempBlob()
  {
      mCachedTempBlob = nullptr;
  }
};

// NB: eventually we'll have separate shadow canvas2d and shadow
// canvas3d layers, but currently they look the same from the
// perspective of the compositor process
class ShadowCanvasLayerD3D9 : public ShadowCanvasLayer,
                             public LayerD3D9
{
public:
  ShadowCanvasLayerD3D9(LayerManagerD3D9* aManager);
  virtual ~ShadowCanvasLayerD3D9();

  // CanvasLayer impl
  virtual void Initialize(const Data& aData);
  // This isn't meaningful for shadow canvas.
  virtual void Updated(const nsIntRect&) {}

  // ShadowCanvasLayer impl
  virtual void Swap(const CanvasSurface& aNewFront,
                    bool needYFlip,
                    CanvasSurface* aNewBack);
  virtual void DestroyFrontBuffer();
  virtual void Disconnect();

  virtual void Destroy();

  // LayerD3D9 implementation
  virtual Layer* GetLayer();
  virtual void RenderLayer();
  virtual void CleanResources();
  virtual void LayerManagerDestroyed();

private:
  virtual void Init(bool needYFlip);

  bool mNeedsYFlip;
  nsRefPtr<ShadowBufferD3D9> mBuffer;
};

} /* layers */
} /* mozilla */
#endif /* GFX_CANVASLAYERD3D9_H */
