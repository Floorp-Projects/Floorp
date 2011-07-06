/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
  virtual void Validate() { Validate(nsnull); }
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

  /* This contains the thebes surface for our render-on-white texture */
  nsRefPtr<gfxASurface> mD2DSurfaceOnWhite;

  /* Have a region of our layer drawn */
  void DrawRegion(nsIntRegion &aRegion, SurfaceMode aMode);

  /* Create a new texture */
  void CreateNewTextures(const gfxIntSize &aSize, SurfaceMode aMode);

  /* Copy a texture region */
  void CopyRegion(ID3D10Texture2D* aSrc, const nsIntPoint &aSrcOffset,
                  ID3D10Texture2D* aDest, const nsIntPoint &aDestOffset,
                  const nsIntRegion &aCopyRegion, nsIntRegion* aValidRegion);
};

class ShadowThebesLayerD3D10 : public ShadowThebesLayer,
                               public LayerD3D10
{
public:
  ShadowThebesLayerD3D10(LayerManagerD3D10* aManager);
  virtual ~ShadowThebesLayerD3D10();

  // ShadowThebesLayer impl
  virtual void SetFrontBuffer(const OptionalThebesBuffer& aNewFront,
                              const nsIntRegion& aValidRegion);
  virtual void
  Swap(const ThebesBuffer& aNewFront, const nsIntRegion& aUpdatedRegion,
       ThebesBuffer* aNewBack, nsIntRegion* aNewBackValidRegion,
       OptionalThebesBuffer* aReadOnlyFront, nsIntRegion* aFrontUpdatedRegion);
  virtual void DestroyFrontBuffer();

  virtual void Disconnect();

  /* LayerD3D10 implementation */
  virtual Layer* GetLayer() { return this; }
  virtual void RenderLayer();
  virtual void Validate();
  virtual void LayerManagerDestroyed();

private:
  /* Texture with our surface data */
  nsRefPtr<ID3D10Texture2D> mTexture;
};

} /* layers */
} /* mozilla */
#endif /* GFX_THEBESLAYERD3D10_H */
