/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_CANVASLAYERD3D9_H
#define GFX_CANVASLAYERD3D9_H

#include "LayerManagerD3D9.h"
#include "GLContextTypes.h"

namespace mozilla {
namespace layers {


class CanvasLayerD3D9 :
  public CanvasLayer,
  public LayerD3D9
{
public:
  CanvasLayerD3D9(LayerManagerD3D9 *aManager);
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

  nsRefPtr<GLContext> mGLContext;
  nsRefPtr<IDirect3DTexture9> mTexture;
  RefPtr<gfx::DrawTarget> mDrawTarget;

  bool mDataIsPremultiplied;
  bool mNeedsYFlip;
  bool mHasAlpha;

  nsAutoArrayPtr<uint8_t> mCachedTempBlob;
  uint32_t mCachedTempBlob_Size;

  uint8_t* GetTempBlob(const uint32_t aSize)
  {
      if (!mCachedTempBlob || aSize != mCachedTempBlob_Size) {
          mCachedTempBlob = new uint8_t[aSize];
          mCachedTempBlob_Size = aSize;
      }

      return mCachedTempBlob;
  }

  void DiscardTempBlob()
  {
      mCachedTempBlob = nullptr;
  }
};

} /* layers */
} /* mozilla */
#endif /* GFX_CANVASLAYERD3D9_H */
