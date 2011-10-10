/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

  // ShadowThebesLayer impl
  virtual void SetFrontBuffer(const OptionalThebesBuffer& aNewFront,
                              const nsIntRegion& aValidRegion);
  virtual void
  Swap(const ThebesBuffer& aNewFront, const nsIntRegion& aUpdatedRegion,
       ThebesBuffer* aNewBack, nsIntRegion* aNewBackValidRegion,
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
