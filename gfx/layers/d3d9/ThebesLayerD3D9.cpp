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

#include "gfxWindowsPlatform.h"
#ifdef CAIRO_HAS_D2D_SURFACE
#include "gfxD2DSurface.h"
#endif

namespace mozilla {
namespace layers {

ThebesLayerD3D9::ThebesLayerD3D9(LayerManagerD3D9 *aManager)
  : ThebesLayer(aManager, NULL)
  , LayerD3D9(aManager)
  , mD2DSurfaceInitialized(false)
{
  mImplData = static_cast<LayerD3D9*>(this);
  aManager->deviceManager()->mLayersWithResources.AppendElement(this);
}

ThebesLayerD3D9::~ThebesLayerD3D9()
{
  if (mD3DManager) {
    mD3DManager->deviceManager()->mLayersWithResources.RemoveElement(this);
  }
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
  ThebesLayer::SetVisibleRegion(aRegion);

  if (!mTexture) {
    // If we don't need to retain content initialize lazily. This is good also
    // because we might get mIsOpaqueSurface set later than the first call to
    // SetVisibleRegion.
    return;
  }

  D3DFORMAT fmt = (CanUseOpaqueSurface() && !mD2DSurface) ?
                    D3DFMT_X8R8G8B8 : D3DFMT_A8R8G8B8;

  D3DSURFACE_DESC desc;
  mTexture->GetLevelDesc(0, &desc);

  if (fmt != desc.Format) {
    // The new format isn't compatible with the old texture, toss out the old
    // texture.
    mTexture = nsnull;
  }

  VerifyContentType();

  nsRefPtr<IDirect3DTexture9> oldTexture = mTexture;

  nsIntRect oldBounds = oldVisibleRegion.GetBounds();
  nsIntRect newBounds = mVisibleRegion.GetBounds();

  CreateNewTexture(gfxIntSize(newBounds.width, newBounds.height));

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

  // We differentiate between these formats since D3D9 will only allow us to
  // call GetDC on an opaque surface.
  D3DFORMAT fmt = (CanUseOpaqueSurface() && !mD2DSurface) ?
                    D3DFMT_X8R8G8B8 : D3DFMT_A8R8G8B8;

  if (mTexture) {
    D3DSURFACE_DESC desc;
    mTexture->GetLevelDesc(0, &desc);

    if (fmt != desc.Format) {
      // The new format isn't compatible with the old texture, toss out the old
      // texture.
      mTexture = nsnull;
      mValidRegion.SetEmpty();
    }
  }

  VerifyContentType();

  if (!mTexture) {
    CreateNewTexture(gfxIntSize(visibleRect.width, visibleRect.height));
    
    if (!mTexture) {
	NS_WARNING("Failed to create texture for thebes layer - not drawing.");
	return;
    }

    mValidRegion.SetEmpty();
  }

  if (!mValidRegion.IsEqual(mVisibleRegion)) {
    /* We use the bounds of the visible region because we draw the bounds of
     * this region when we draw this entire texture. We have to make sure that
     * the areas that aren't filled with content get their background drawn.
     * This is an issue for opaque surfaces, which otherwise won't get their
     * background painted.
     */
    nsIntRegion region;
    region.Sub(mVisibleRegion, mValidRegion);

    DrawRegion(region);

    mValidRegion = mVisibleRegion;
  }

  SetShaderTransformAndOpacity();

#ifdef CAIRO_HAS_D2D_SURFACE
  if (mD2DSurface && CanUseOpaqueSurface()) {
    mD3DManager->SetShaderMode(DeviceManagerD3D9::RGBLAYER);
  } else
#endif
  mD3DManager->SetShaderMode(DeviceManagerD3D9::RGBALAYER);

  device()->SetTexture(0, mTexture);

  nsIntRegionRectIterator iter(mVisibleRegion);

  const nsIntRect *iterRect;
  while ((iterRect = iter.Next())) {
    device()->SetVertexShaderConstantF(CBvLayerQuad,
                                       ShaderConstantRect(iterRect->x,
                                                          iterRect->y,
                                                          iterRect->width,
                                                          iterRect->height),
                                       1);

    device()->SetVertexShaderConstantF(CBvTextureCoords,
      ShaderConstantRect(
        (float)(iterRect->x - visibleRect.x) / (float)visibleRect.width,
        (float)(iterRect->y - visibleRect.y) / (float)visibleRect.height,
        (float)iterRect->width / (float)visibleRect.width,
        (float)iterRect->height / (float)visibleRect.height), 1);

    device()->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
  }

  // Set back to default.
  device()->SetVertexShaderConstantF(CBvTextureCoords,
                                     ShaderConstantRect(0, 0, 1.0f, 1.0f),
                                     1);
}

void
ThebesLayerD3D9::CleanResources()
{
  mTexture = nsnull;
}

void
ThebesLayerD3D9::LayerManagerDestroyed()
{
  mD3DManager->deviceManager()->mLayersWithResources.RemoveElement(this);
  mD3DManager = nsnull;
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

void
ThebesLayerD3D9::VerifyContentType()
{
#ifdef CAIRO_HAS_D2D_SURFACE
  if (mD2DSurface) {
    gfxASurface::gfxContentType type = CanUseOpaqueSurface() ?
      gfxASurface::CONTENT_COLOR : gfxASurface::CONTENT_COLOR_ALPHA;

    if (type != mD2DSurface->GetContentType()) {
      // We could choose to recreate only the D2D surface, but since we can't
      // use retention the synchronisation overhead probably isn't worth it.
      mD2DSurface = nsnull;
      mTexture = nsnull;
    }
  }
#endif
}

void
ThebesLayerD3D9::DrawRegion(const nsIntRegion &aRegion)
{
  HRESULT hr;
  nsIntRect visibleRect = mVisibleRegion.GetBounds();
  nsRefPtr<gfxContext> context;

#ifdef CAIRO_HAS_D2D_SURFACE
  if (mD2DSurface) {
    context = new gfxContext(mD2DSurface);
    nsIntRegionRectIterator iter(aRegion);

    context->Translate(gfxPoint(-visibleRect.x, -visibleRect.y));
    context->NewPath();
    const nsIntRect *iterRect;
    while ((iterRect = iter.Next())) {
      context->Rectangle(gfxRect(iterRect->x, iterRect->y, iterRect->width, iterRect->height));      
    }
    context->Clip();
    if (!mD2DSurfaceInitialized || 
        mD2DSurface->GetContentType() != gfxASurface::CONTENT_COLOR) {
      context->SetOperator(gfxContext::OPERATOR_CLEAR);
      context->Paint();
      context->SetOperator(gfxContext::OPERATOR_OVER);
      mD2DSurfaceInitialized = true;
    }

    LayerManagerD3D9::CallbackInfo cbInfo = mD3DManager->GetCallbackInfo();
    cbInfo.Callback(this, context, aRegion, nsIntRegion(), cbInfo.CallbackData);
    mD2DSurface->Flush();

    // XXX - This call is quite expensive, we may want to consider doing our
    // drawing in a seperate 'validation' iteration. And then flushing once for
    // all the D2D surfaces we might have drawn, before doing our D3D9 rendering
    // loop.
    cairo_d2d_finish_device(gfxWindowsPlatform::GetPlatform()->GetD2DDevice());
    return;
  }
#endif

  D3DFORMAT fmt = CanUseOpaqueSurface() ? D3DFMT_X8R8G8B8 : D3DFMT_A8R8G8B8;
  nsIntRect bounds = aRegion.GetBounds();

  gfxASurface::gfxImageFormat imageFormat = gfxASurface::ImageFormatARGB32;
  nsRefPtr<gfxASurface> destinationSurface;

  nsRefPtr<IDirect3DTexture9> tmpTexture;
  hr = device()->CreateTexture(bounds.width, bounds.height, 1,
                               0, fmt,
                               D3DPOOL_SYSTEMMEM, getter_AddRefs(tmpTexture), NULL);

  if (FAILED(hr)) {
    ReportFailure(NS_LITERAL_CSTRING("Failed to create temporary texture in system memory."), hr);
    return;
  }

  nsRefPtr<IDirect3DSurface9> surf;
  HDC dc;
  if (CanUseOpaqueSurface()) {
    hr = tmpTexture->GetSurfaceLevel(0, getter_AddRefs(surf));

    if (FAILED(hr)) {
      // Uh-oh, bail.
      NS_WARNING("Failed to get texture surface level.");
      return;
    }

    hr = surf->GetDC(&dc);

    if (FAILED(hr)) {
      NS_WARNING("Failed to get device context for texture surface.");
      return;
    }

    destinationSurface = new gfxWindowsSurface(dc);
  } else {
    // XXX - We may consider retaining a SYSTEMMEM texture texture the size
    // of our DEFAULT texture and then use UpdateTexture and add dirty rects
    // to update in a single call.
    destinationSurface =
    gfxPlatform::GetPlatform()->
      CreateOffscreenSurface(gfxIntSize(bounds.width,
                                        bounds.height),
                             gfxASurface::ContentFromFormat(imageFormat));
  }

  context = new gfxContext(destinationSurface);
  context->Translate(gfxPoint(-bounds.x, -bounds.y));
  LayerManagerD3D9::CallbackInfo cbInfo = mD3DManager->GetCallbackInfo();
  cbInfo.Callback(this, context, aRegion, nsIntRegion(), cbInfo.CallbackData);

  if (CanUseOpaqueSurface()) {
    surf->ReleaseDC(dc);
  } else {
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
  }

  nsRefPtr<IDirect3DSurface9> srcSurface;
  nsRefPtr<IDirect3DSurface9> dstSurface;

  mTexture->GetSurfaceLevel(0, getter_AddRefs(dstSurface));
  tmpTexture->GetSurfaceLevel(0, getter_AddRefs(srcSurface));

  nsIntRegionRectIterator iter(aRegion);
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
}

void
ThebesLayerD3D9::CreateNewTexture(const gfxIntSize &aSize)
{
  if (aSize.width == 0 | aSize.height == 0) {
    // Nothing to do.
    return;
  }

  mTexture = nsnull;
  PRBool canUseOpaqueSurface = CanUseOpaqueSurface();
#ifdef CAIRO_HAS_D2D_SURFACE
  if (gfxWindowsPlatform::GetPlatform()->GetRenderMode() ==
      gfxWindowsPlatform::RENDER_DIRECT2D) {
        if (mD3DManager->deviceManager()->IsD3D9Ex()) {
          // We should have D3D9Ex where we have D2D.
          HANDLE sharedHandle = 0;
          device()->CreateTexture(aSize.width, aSize.height, 1,
                                  D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                  D3DPOOL_DEFAULT, getter_AddRefs(mTexture), &sharedHandle);

          mD2DSurfaceInitialized = false;
          mD2DSurface = new gfxD2DSurface(sharedHandle, canUseOpaqueSurface ?
            gfxASurface::CONTENT_COLOR : gfxASurface::CONTENT_COLOR_ALPHA);

          // If there's an error, go on and do what we always do.
          if (mD2DSurface->CairoStatus()) {
            mD2DSurface = nsnull;
            mTexture = nsnull;
          }
        }
  }
#endif
  if (!mTexture) {
    device()->CreateTexture(aSize.width, aSize.height, 1,
                            D3DUSAGE_RENDERTARGET, canUseOpaqueSurface ? D3DFMT_X8R8G8B8 : D3DFMT_A8R8G8B8,
                            D3DPOOL_DEFAULT, getter_AddRefs(mTexture), NULL);
  }
}

} /* namespace layers */
} /* namespace mozilla */
