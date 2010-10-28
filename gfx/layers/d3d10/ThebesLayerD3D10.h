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

#ifndef GFX_THEBESLAYERD3D10_H
#define GFX_THEBESLAYERD3D10_H

#include "Layers.h"
#include "LayerManagerD3D10.h"

namespace mozilla {
namespace layers {

class ThebesLayerD3D10 : public ThebesLayer,
                         public LayerD3D10
{
public:
  ThebesLayerD3D10(LayerManagerD3D10 *aManager);
  virtual ~ThebesLayerD3D10();

  /* Layer implementation */
  void SetVisibleRegion(const nsIntRegion& aRegion);

  /* ThebesLayer implementation */
  void InvalidateRegion(const nsIntRegion& aRegion);

  /* LayerD3D10 implementation */
  virtual Layer* GetLayer();
  virtual void RenderLayer(float aOpacity, const gfx3DMatrix &aTransform);
  virtual void Validate();
  virtual void LayerManagerDestroyed();

private:
  /* Texture with our surface data */
  nsRefPtr<ID3D10Texture2D> mTexture;

  /* Shader resource view for our texture */
  nsRefPtr<ID3D10ShaderResourceView> mSRView;

  /* Checks if our D2D surface has the right content type */
  void VerifyContentType();

  /* This contains the thebes surface */
  nsRefPtr<gfxASurface> mD2DSurface;

  /* Have a region of our layer drawn */
  void DrawRegion(const nsIntRegion &aRegion);

  /* Create a new texture */
  void CreateNewTexture(const gfxIntSize &aSize);
};

} /* layers */
} /* mozilla */
#endif /* GFX_THEBESLAYERD3D10_H */
