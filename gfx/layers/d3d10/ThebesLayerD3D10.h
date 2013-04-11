/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_THEBESLAYERD3D10_H
#define GFX_THEBESLAYERD3D10_H

#include "LayerManagerD3D10.h"

namespace mozilla {
namespace layers {

class ThebesLayerD3D10 : public ThebesLayer,
                         public LayerD3D10
{
public:
  ThebesLayerD3D10(LayerManagerD3D10 *aManager);
  virtual ~ThebesLayerD3D10();

  void Validate(ReadbackProcessor *aReadback);

  /* ThebesLayer implementation */
  void InvalidateRegion(const nsIntRegion& aRegion);

  /* LayerD3D10 implementation */
  virtual Layer* GetLayer();
  virtual void RenderLayer();
  virtual void Validate() { Validate(nullptr); }
  virtual void LayerManagerDestroyed();

private:
  /* Texture with our surface data */
  nsRefPtr<ID3D10Texture2D> mTexture;

  /* Shader resource view for our texture */
  nsRefPtr<ID3D10ShaderResourceView> mSRView;

  /* Texture for render-on-whitew when doing component alpha */
  nsRefPtr<ID3D10Texture2D> mTextureOnWhite;

  /* Shader resource view for our render-on-white texture */
  nsRefPtr<ID3D10ShaderResourceView> mSRViewOnWhite;

  /* Area of layer currently stored in texture(s) */
  nsIntRect mTextureRect;

  /* Last surface mode set in Validate() */
  SurfaceMode mCurrentSurfaceMode;

  /* Checks if our D2D surface has the right content type */
  void VerifyContentType(SurfaceMode aMode);

  /* This contains the thebes surface */
  nsRefPtr<gfxASurface> mD2DSurface;

  mozilla::RefPtr<mozilla::gfx::DrawTarget> mDrawTarget;

  /* This contains the thebes surface for our render-on-white texture */
  nsRefPtr<gfxASurface> mD2DSurfaceOnWhite;

  /* Have a region of our layer drawn */
  void DrawRegion(nsIntRegion &aRegion, SurfaceMode aMode);

  /* Create a new texture */
  void CreateNewTextures(const gfxIntSize &aSize, SurfaceMode aMode);

  // Fill textures with opaque black and white in the specified region.
  void FillTexturesBlackWhite(const nsIntRegion& aRegion, const nsIntPoint& aOffset);

  /* Copy a texture region */
  void CopyRegion(ID3D10Texture2D* aSrc, const nsIntPoint &aSrcOffset,
                  ID3D10Texture2D* aDest, const nsIntPoint &aDestOffset,
                  const nsIntRegion &aCopyRegion, nsIntRegion* aValidRegion);
};

} /* layers */
} /* mozilla */
#endif /* GFX_THEBESLAYERD3D10_H */
