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
 *   Bas Schouten <bschouten@mozilla.org>
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

#ifndef GFX_IMAGELAYERD3D10_H
#define GFX_IMAGELAYERD3D10_H

#include "LayerManagerD3D10.h"
#include "ImageLayers.h"
#include "yuv_convert.h"

namespace mozilla {
namespace layers {

class THEBES_API ImageLayerD3D10 : public ImageLayer,
                                   public LayerD3D10
{
public:
  ImageLayerD3D10(LayerManagerD3D10 *aManager)
    : ImageLayer(aManager, NULL)
    , LayerD3D10(aManager)
  {
    mImplData = static_cast<LayerD3D10*>(this);
  }

  // LayerD3D10 Implementation
  virtual Layer* GetLayer();

  virtual void RenderLayer();

  void AllocateTexturesYCbCr(PlanarYCbCrImage *aImage);
};

struct PlanarYCbCrD3D10BackendData : public ImageBackendData
{
  nsRefPtr<ID3D10Texture2D> mYTexture;
  nsRefPtr<ID3D10Texture2D> mCrTexture;
  nsRefPtr<ID3D10Texture2D> mCbTexture;
  nsRefPtr<ID3D10ShaderResourceView> mYView;
  nsRefPtr<ID3D10ShaderResourceView> mCbView;
  nsRefPtr<ID3D10ShaderResourceView> mCrView;
};

struct TextureD3D10BackendData : public ImageBackendData
{
  nsRefPtr<ID3D10Texture2D> mTexture;
  nsRefPtr<ID3D10ShaderResourceView> mSRView;
};

} /* layers */
} /* mozilla */
#endif /* GFX_IMAGELAYERD3D10_H */
