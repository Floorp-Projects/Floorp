/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_THEBESLAYERD3D9_H
#define GFX_THEBESLAYERD3D9_H

#include "Layers.h"
#include "LayerManagerD3D9.h"
#include "gfxImageSurface.h"
#include "ReadbackProcessor.h"

namespace mozilla {
namespace layers {

class ReadbackProcessor;
class ShadowBufferD3D9;

class ThebesLayerD3D9 : public ThebesLayer,
                        public LayerD3D9
{
public:
  ThebesLayerD3D9(LayerManagerD3D9 *aManager);
  virtual ~ThebesLayerD3D9();

  /* ThebesLayer implementation */
  void InvalidateRegion(const nsIntRegion& aRegion);

  /* LayerD3D9 implementation */
  Layer* GetLayer();
  virtual bool IsEmpty();
  virtual void RenderLayer() { RenderThebesLayer(nsnull); }
  virtual void CleanResources();
  virtual void LayerManagerDestroyed();

  void RenderThebesLayer(ReadbackProcessor* aReadback);

private:
  /*
   * D3D9 texture
   */
  nsRefPtr<IDirect3DTexture9> mTexture;
  /*
   * D3D9 texture for render-on-white when doing component alpha
   */
  nsRefPtr<IDirect3DTexture9> mTextureOnWhite;
  /**
   * Visible region bounds used when we drew the contents of the textures
   */
  nsIntRect mTextureRect;

  bool HaveTextures(SurfaceMode aMode)
  {
    return mTexture && (aMode != SURFACE_COMPONENT_ALPHA || mTextureOnWhite);
  }

  /* Checks if our surface has the right content type */
  void VerifyContentType(SurfaceMode aMode);

  /* Ensures we have the necessary texture object(s) and that they correspond
   * to mVisibleRegion.GetBounds(). This creates new texture objects as
   * necessary and also copies existing valid texture data if necessary.
   */
  void UpdateTextures(SurfaceMode aMode);

  /* Render the rectangles of mVisibleRegion with D3D9 using the currently
   * bound textures, target, shaders, etc.
   */
  void RenderRegion(const nsIntRegion& aRegion);

  /* Have a region of our layer drawn */
  void DrawRegion(nsIntRegion &aRegion, SurfaceMode aMode,
                  const nsTArray<ReadbackProcessor::Update>& aReadbackUpdates);

  /* Create a new texture */
  void CreateNewTextures(const gfxIntSize &aSize, SurfaceMode aMode);

  void CopyRegion(IDirect3DTexture9* aSrc, const nsIntPoint &aSrcOffset,
                  IDirect3DTexture9* aDest, const nsIntPoint &aDestOffset,
                  const nsIntRegion &aCopyRegion, nsIntRegion* aValidRegion);
};

class ShadowThebesLayerD3D9 : public ShadowThebesLayer,
                              public LayerD3D9
{
public:
  ShadowThebesLayerD3D9(LayerManagerD3D9 *aManager);
  virtual ~ShadowThebesLayerD3D9();

  virtual void
  Swap(const ThebesBuffer& aNewFront, const nsIntRegion& aUpdatedRegion,
       OptionalThebesBuffer* aNewBack, nsIntRegion* aNewBackValidRegion,
       OptionalThebesBuffer* aReadOnlyFront, nsIntRegion* aFrontUpdatedRegion);
  virtual void DestroyFrontBuffer();

  virtual void Disconnect();

  // LayerD3D9 impl
  Layer* GetLayer();
  virtual bool IsEmpty();
  virtual void RenderLayer() { RenderThebesLayer(); }
  virtual void CleanResources();
  virtual void LayerManagerDestroyed();

  void RenderThebesLayer();

private:
  nsRefPtr<ShadowBufferD3D9> mBuffer;
};

} /* layers */
} /* mozilla */
#endif /* GFX_THEBESLAYERD3D9_H */
