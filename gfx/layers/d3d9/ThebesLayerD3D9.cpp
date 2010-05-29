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

#include "ThebesLayerD3D9.h"
#include "gfxPlatform.h"

namespace mozilla {
namespace layers {

ThebesLayerD3D9::ThebesLayerD3D9(LayerManagerD3D9 *aManager)
  : ThebesLayer(aManager, NULL)
  , LayerD3D9(aManager)
{
  mImplData = static_cast<LayerD3D9*>(this);
  aManager->mThebesLayers.AppendElement(this);
}

ThebesLayerD3D9::~ThebesLayerD3D9()
{
  mD3DManager->mThebesLayers.RemoveElement(this);
}


void
ThebesLayerD3D9::SetVisibleRegion(const nsIntRegion &aRegion)
{
  if (aRegion.GetBounds() == mVisibleRect) {
    return;
  }
  mVisibleRect = aRegion.GetBounds();

  device()->CreateTexture(mVisibleRect.width, mVisibleRect.height, 1,
                          D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                          D3DPOOL_DEFAULT, getter_AddRefs(mTexture), NULL);

  mInvalidatedRect = mVisibleRect;
}


void
ThebesLayerD3D9::InvalidateRegion(const nsIntRegion &aRegion)
{
  nsIntRegion invalidatedRegion;
  invalidatedRegion.Or(aRegion, mInvalidatedRect);
  invalidatedRegion.And(invalidatedRegion, mVisibleRect);
  mInvalidatedRect = invalidatedRegion.GetBounds();
}

LayerD3D9::LayerType
ThebesLayerD3D9::GetType()
{
  return TYPE_THEBES;
}

const nsIntRect&
ThebesLayerD3D9::GetVisibleRect()
{
  return mVisibleRect;
}

void
ThebesLayerD3D9::RenderLayer()
{
  if (!mTexture) {
    device()->CreateTexture(mVisibleRect.width, mVisibleRect.height, 1,
                            D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                            D3DPOOL_DEFAULT, getter_AddRefs(mTexture), NULL);
    mInvalidatedRect = mVisibleRect;
  }
  if (!mInvalidatedRect.IsEmpty()) {
    nsIntRegion region = mInvalidatedRect;

    gfxASurface::gfxImageFormat imageFormat = gfxASurface::ImageFormatARGB32;;
    nsRefPtr<gfxASurface> destinationSurface;
    nsRefPtr<gfxContext> context;

    destinationSurface =
      gfxPlatform::GetPlatform()->
        CreateOffscreenSurface(gfxIntSize(mInvalidatedRect.width,
                                          mInvalidatedRect.height),
                               imageFormat);

    context = new gfxContext(destinationSurface);
    context->Translate(gfxPoint(-mInvalidatedRect.x, -mInvalidatedRect.y));
    LayerManagerD3D9::CallbackInfo cbInfo = mD3DManager->GetCallbackInfo();
    cbInfo.Callback(this, context, region, cbInfo.CallbackData);

    nsRefPtr<IDirect3DTexture9> tmpTexture;
    device()->CreateTexture(mInvalidatedRect.width, mInvalidatedRect.height, 1,
                            0, D3DFMT_A8R8G8B8,
                            D3DPOOL_SYSTEMMEM, getter_AddRefs(tmpTexture), NULL);

    D3DLOCKED_RECT r;
    tmpTexture->LockRect(0, &r, NULL, 0);

    nsRefPtr<gfxImageSurface> imgSurface =
      new gfxImageSurface((unsigned char *)r.pBits,
                          gfxIntSize(mInvalidatedRect.width,
                                     mInvalidatedRect.height),
                          r.Pitch,
                          imageFormat);

    context = new gfxContext(imgSurface);
    context->SetSource(destinationSurface);
    context->SetOperator(gfxContext::OPERATOR_SOURCE);
    context->Paint();

    imgSurface = NULL;

    tmpTexture->UnlockRect(0);

    nsRefPtr<IDirect3DSurface9> srcSurface;
    nsRefPtr<IDirect3DSurface9> dstSurface;

    mTexture->GetSurfaceLevel(0, getter_AddRefs(dstSurface));
    tmpTexture->GetSurfaceLevel(0, getter_AddRefs(srcSurface));

    POINT point;
    point.x = mInvalidatedRect.x - mVisibleRect.x;
    point.y = mInvalidatedRect.y - mVisibleRect.y;
    device()->UpdateSurface(srcSurface, NULL, dstSurface, &point);
  }

  float quadTransform[4][4];
  /*
   * Matrix to transform the <0.0,0.0>, <1.0,1.0> quad to the correct position
   * and size. To get pixel perfect mapping we offset the quad half a pixel
   * to the top-left.
   *
   * See: http://msdn.microsoft.com/en-us/library/bb219690%28VS.85%29.aspx
   */
  memset(&quadTransform, 0, sizeof(quadTransform));
  quadTransform[0][0] = (float)GetVisibleRect().width;
  quadTransform[1][1] = (float)GetVisibleRect().height;
  quadTransform[2][2] = 1.0f;
  quadTransform[3][0] = (float)GetVisibleRect().x - 0.5f;
  quadTransform[3][1] = (float)GetVisibleRect().y - 0.5f;
  quadTransform[3][3] = 1.0f;

  device()->SetVertexShaderConstantF(0, &quadTransform[0][0], 4);
  device()->SetVertexShaderConstantF(4, &mTransform._11, 4);

  float opacity[4];
  /*
   * We always upload a 4 component float, but the shader will use only the
   * first component since it's declared as a 'float'.
   */
  opacity[0] = GetOpacity();
  device()->SetPixelShaderConstantF(0, opacity, 1);

  mD3DManager->SetShaderMode(LayerManagerD3D9::RGBLAYER);

  device()->SetTexture(0, mTexture);
  device()->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
}

void
ThebesLayerD3D9::CleanResources()
{
  mTexture = nsnull;
}

const nsIntRect&
ThebesLayerD3D9::GetInvalidatedRect()
{
  return mInvalidatedRect;
}

Layer*
ThebesLayerD3D9::GetLayer()
{
  return this;
}

PRBool
ThebesLayerD3D9::IsEmpty()
{
  return !mTexture;
}

} /* namespace layers */
} /* namespace mozilla */