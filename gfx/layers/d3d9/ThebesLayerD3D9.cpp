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

/**
 * Retention threshold - amount of pixels intersection required to enable
 * layer content retention. This is a guesstimate. Profiling could be done to
 * figure out the optimal threshold.
 */
#define RETENTION_THRESHOLD 16384

void
ThebesLayerD3D9::SetVisibleRegion(const nsIntRegion &aRegion)
{
  if (aRegion.IsEqual(mVisibleRegion)) {
    return;
  }
  nsIntRegion oldVisibleRegion = mVisibleRegion;
  nsRefPtr<IDirect3DTexture9> oldTexture = mTexture;

  ThebesLayer::SetVisibleRegion(aRegion);

  nsIntRect oldBounds = oldVisibleRegion.GetBounds();
  nsIntRect newBounds = mVisibleRegion.GetBounds();
  
  device()->CreateTexture(newBounds.width, newBounds.height, 1,
                          D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                          D3DPOOL_DEFAULT, getter_AddRefs(mTexture), NULL);

  // Old visible region will become the region that is covered by both the
  // old and the new visible region.
  oldVisibleRegion.And(oldVisibleRegion, mVisibleRegion);
  // No point in retaining parts which were not valid.
  oldVisibleRegion.And(oldVisibleRegion, mValidRegion);

  nsIntRect largeRect = oldVisibleRegion.GetLargestRectangle();

  // If we had no hardware texture before or have no retained area larger than
  // the retention threshold, we're not retaining and are done here. If our
  // texture creation failed this can mean a device reset is pending and we
  // should silently ignore the failure. In the future when device failures
  // are properly handled we should test for the type of failure and gracefully
  // handle different failures. See bug 569081.
  if (!oldTexture || !mTexture ||
      largeRect.width * largeRect.height < RETENTION_THRESHOLD) {
    mValidRegion.SetEmpty();
    return;
  }

  nsRefPtr<IDirect3DSurface9> srcSurface, dstSurface;
  oldTexture->GetSurfaceLevel(0, getter_AddRefs(srcSurface));
  mTexture->GetSurfaceLevel(0, getter_AddRefs(dstSurface));

  nsIntRegion retainedRegion;
  nsIntRegionRectIterator iter(oldVisibleRegion);
  const nsIntRect *r;
  while ((r = iter.Next())) {
    if (r->width * r->height > RETENTION_THRESHOLD) {
      RECT oldRect, newRect;

      // Calculate the retained rectangle's position on the old and the new
      // surface.
      oldRect.left = r->x - oldBounds.x;
      oldRect.top = r->y - oldBounds.y;
      oldRect.right = oldRect.left + r->width;
      oldRect.bottom = oldRect.top + r->height;

      newRect.left = r->x - newBounds.x;
      newRect.top = r->y - newBounds.y;
      newRect.right = newRect.left + r->width;
      newRect.bottom = newRect.top + r->height;

      // Copy data from our old texture to the new one
      HRESULT hr = device()->
        StretchRect(srcSurface, &oldRect, dstSurface, &newRect, D3DTEXF_NONE);

      if (SUCCEEDED(hr)) {
        retainedRegion.Or(retainedRegion, *r);
      }
    }
  }

  // Areas which were valid and were retained are still valid
  mValidRegion.And(mValidRegion, retainedRegion);  
}


void
ThebesLayerD3D9::InvalidateRegion(const nsIntRegion &aRegion)
{
  mValidRegion.Sub(mValidRegion, aRegion);
}

void
ThebesLayerD3D9::RenderLayer()
{
  if (mVisibleRegion.IsEmpty()) {
    return;
  }
  nsIntRect visibleRect = mVisibleRegion.GetBounds();

  if (!mTexture) {
    device()->CreateTexture(visibleRect.width, visibleRect.height, 1,
                            D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                            D3DPOOL_DEFAULT, getter_AddRefs(mTexture), NULL);
    mValidRegion.SetEmpty();
  }

  if (!mValidRegion.IsEqual(mVisibleRegion)) {
    nsIntRegion region;
    region.Sub(mVisibleRegion, mValidRegion);
    nsIntRect bounds = region.GetBounds();

    gfxASurface::gfxImageFormat imageFormat = gfxASurface::ImageFormatARGB32;;
    nsRefPtr<gfxASurface> destinationSurface;
    nsRefPtr<gfxContext> context;

    // XXX - Should optimize here to use IDirect3DSurface9::GetDC() for GDI
    // rendering since we're always on windows. We may consider retaining a
    // SYSTEMMEM texture texture the size of our DEFAULT texture and then use
    // UpdateTexture and add dirty rects to update in a single call.
    destinationSurface =
      gfxPlatform::GetPlatform()->
        CreateOffscreenSurface(gfxIntSize(bounds.width,
                                          bounds.height),
                               imageFormat);

    context = new gfxContext(destinationSurface);
    context->Translate(gfxPoint(-bounds.x, -bounds.y));
    LayerManagerD3D9::CallbackInfo cbInfo = mD3DManager->GetCallbackInfo();
    cbInfo.Callback(this, context, region, nsIntRegion(), cbInfo.CallbackData);

    nsRefPtr<IDirect3DTexture9> tmpTexture;
    device()->CreateTexture(bounds.width, bounds.height, 1,
                            0, D3DFMT_A8R8G8B8,
                            D3DPOOL_SYSTEMMEM, getter_AddRefs(tmpTexture), NULL);

    D3DLOCKED_RECT r;
    tmpTexture->LockRect(0, &r, NULL, 0);

    nsRefPtr<gfxImageSurface> imgSurface =
      new gfxImageSurface((unsigned char *)r.pBits,
                          gfxIntSize(bounds.width,
                                     bounds.height),
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

    nsIntRegionRectIterator iter(region);
    const nsIntRect *iterRect;
    while ((iterRect = iter.Next())) {
      RECT rect;
      rect.left = iterRect->x - bounds.x;
      rect.top = iterRect->y - bounds.y;
      rect.right = rect.left + iterRect->width;
      rect.bottom = rect.top + iterRect->height;
      POINT point;
      point.x = iterRect->x - visibleRect.x;
      point.y = iterRect->y - visibleRect.y;
      device()->UpdateSurface(srcSurface, &rect, dstSurface, &point);
    }
    mValidRegion = mVisibleRegion;
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
  quadTransform[0][0] = (float)visibleRect.width;
  quadTransform[1][1] = (float)visibleRect.height;
  quadTransform[2][2] = 1.0f;
  quadTransform[3][0] = (float)visibleRect.x - 0.5f;
  quadTransform[3][1] = (float)visibleRect.y - 0.5f;
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
