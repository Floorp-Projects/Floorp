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

#include "ThebesLayerD3D10.h"
#include "gfxPlatform.h"

#include "gfxWindowsPlatform.h"
#ifdef CAIRO_HAS_D2D_SURFACE
#include "gfxD2DSurface.h"
#endif

namespace mozilla {
namespace layers {

ThebesLayerD3D10::ThebesLayerD3D10(LayerManagerD3D10 *aManager)
  : ThebesLayer(aManager, NULL)
  , LayerD3D10(aManager)
{
  mImplData = static_cast<LayerD3D10*>(this);
}

ThebesLayerD3D10::~ThebesLayerD3D10()
{
}

/**
 * Retention threshold - amount of pixels intersection required to enable
 * layer content retention. This is a guesstimate. Profiling could be done to
 * figure out the optimal threshold.
 */
#define RETENTION_THRESHOLD 16384

void
ThebesLayerD3D10::SetVisibleRegion(const nsIntRegion &aRegion)
{
  if (aRegion.IsEqual(mVisibleRegion)) {
    return;
  }

  HRESULT hr;

  nsIntRegion oldVisibleRegion = mVisibleRegion;
  ThebesLayer::SetVisibleRegion(aRegion);

  if (!mTexture) {
    // If we don't need to retain content initialize lazily. This is good also
    // because we might get mIsOpaqueSurface set later than the first call to
    // SetVisibleRegion.
    return;
  }

  VerifyContentType();

  nsRefPtr<ID3D10Texture2D> oldTexture = mTexture;

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

  nsIntRegion retainedRegion;
  nsIntRegionRectIterator iter(oldVisibleRegion);
  const nsIntRect *r;
  while ((r = iter.Next())) {
    if (r->width * r->height > RETENTION_THRESHOLD) {
      // Calculate the retained rectangle's position on the old and the new
      // surface.
      D3D10_BOX box;
      box.left = r->x - oldBounds.x;
      box.top = r->y - oldBounds.y;
      box.right = box.left + r->width;
      box.bottom = box.top + r->height;
      box.back = 1.0f;
      box.front = 0;

      device()->CopySubresourceRegion(mTexture, 0,
                                      r->x - newBounds.x,
                                      r->y - newBounds.y,
                                      0,
                                      oldTexture, 0,
                                      &box);

      if (SUCCEEDED(hr)) {
        retainedRegion.Or(retainedRegion, *r);
      }
    }
  }

  // Areas which were valid and were retained are still valid
  mValidRegion.And(mValidRegion, retainedRegion);  
}


void
ThebesLayerD3D10::InvalidateRegion(const nsIntRegion &aRegion)
{
  mValidRegion.Sub(mValidRegion, aRegion);
}

void
ThebesLayerD3D10::RenderLayer(float aOpacity, const gfx3DMatrix &aTransform)
{
  if (!mTexture) {
    return;
  }

  nsIntRect visibleRect = mVisibleRegion.GetBounds();

  gfx3DMatrix transform = mTransform * aTransform;
  effect()->GetVariableByName("mLayerTransform")->SetRawValue(&transform._11, 0, 64);
  effect()->GetVariableByName("fLayerOpacity")->AsScalar()->SetFloat(GetOpacity() * aOpacity);

  ID3D10EffectTechnique *technique;
  if (CanUseOpaqueSurface()) {
    technique = effect()->GetTechniqueByName("RenderRGBLayerPremul");
  } else {
    technique = effect()->GetTechniqueByName("RenderRGBALayerPremul");
  }


  nsIntRegionRectIterator iter(mVisibleRegion);

  const nsIntRect *iterRect;
  if (mSRView) {
    effect()->GetVariableByName("tRGB")->AsShaderResource()->SetResource(mSRView);
  }

  while ((iterRect = iter.Next())) {
    effect()->GetVariableByName("vLayerQuad")->AsVector()->SetFloatVector(
      ShaderConstantRectD3D10(
        (float)iterRect->x,
        (float)iterRect->y,
        (float)iterRect->width,
        (float)iterRect->height)
      );

    effect()->GetVariableByName("vTextureCoords")->AsVector()->SetFloatVector(
      ShaderConstantRectD3D10(
        (float)(iterRect->x - visibleRect.x) / (float)visibleRect.width,
        (float)(iterRect->y - visibleRect.y) / (float)visibleRect.height,
        (float)iterRect->width / (float)visibleRect.width,
        (float)iterRect->height / (float)visibleRect.height)
      );

    technique->GetPassByIndex(0)->Apply(0);
    device()->Draw(4, 0);
  }

  // Set back to default.
  effect()->GetVariableByName("vTextureCoords")->AsVector()->
    SetFloatVector(ShaderConstantRectD3D10(0, 0, 1.0f, 1.0f));
}

void
ThebesLayerD3D10::Validate()
{
  if (mVisibleRegion.IsEmpty()) {
    return;
  }

  VerifyContentType();

  nsIntRect visibleRect = mVisibleRegion.GetBounds();

  if (!mTexture) {
    CreateNewTexture(gfxIntSize(visibleRect.width, visibleRect.height));
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
}

void
ThebesLayerD3D10::LayerManagerDestroyed()
{
  mD3DManager = nsnull;
}

Layer*
ThebesLayerD3D10::GetLayer()
{
  return this;
}

void
ThebesLayerD3D10::VerifyContentType()
{
  if (mD2DSurface) {
    gfxASurface::gfxContentType type = CanUseOpaqueSurface() ?
      gfxASurface::CONTENT_COLOR : gfxASurface::CONTENT_COLOR_ALPHA;

    if (type != mD2DSurface->GetContentType()) {
      mD2DSurface = new gfxD2DSurface(mTexture, type);

      if (!mD2DSurface || mD2DSurface->CairoStatus()) {
        NS_WARNING("Failed to create surface for ThebesLayerD3D10.");
        mD2DSurface = nsnull;
        return;
      }
      mValidRegion.SetEmpty();
    }
  }
}

void
ThebesLayerD3D10::DrawRegion(const nsIntRegion &aRegion)
{
  HRESULT hr;
  nsIntRect visibleRect = mVisibleRegion.GetBounds();

  if (!mD2DSurface) {
    return;
  }

  nsRefPtr<gfxContext> context = new gfxContext(mD2DSurface);

  nsIntRegionRectIterator iter(aRegion);
  context->Translate(gfxPoint(-visibleRect.x, -visibleRect.y));
  context->NewPath();
  const nsIntRect *iterRect;
  while ((iterRect = iter.Next())) {
    context->Rectangle(gfxRect(iterRect->x, iterRect->y, iterRect->width, iterRect->height));      
  }
  context->Clip();

  if (mD2DSurface->GetContentType() != gfxASurface::CONTENT_COLOR) {
    context->SetOperator(gfxContext::OPERATOR_CLEAR);
    context->Paint();
    context->SetOperator(gfxContext::OPERATOR_OVER);
  }

  LayerManagerD3D10::CallbackInfo cbInfo = mD3DManager->GetCallbackInfo();
  cbInfo.Callback(this, context, aRegion, nsIntRegion(), cbInfo.CallbackData);
}

void
ThebesLayerD3D10::CreateNewTexture(const gfxIntSize &aSize)
{
  if (aSize.width == 0 || aSize.height == 0) {
    // Nothing to do.
    return;
  }

  CD3D10_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM, aSize.width, aSize.height, 1, 1);
  desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
  desc.MiscFlags = D3D10_RESOURCE_MISC_GDI_COMPATIBLE;

  HRESULT hr = device()->CreateTexture2D(&desc, NULL, getter_AddRefs(mTexture));

  if (FAILED(hr)) {
    NS_WARNING("Failed to create new texture for ThebesLayerD3D10!");
    return;
  }

  hr = device()->CreateShaderResourceView(mTexture, NULL, getter_AddRefs(mSRView));

  if (FAILED(hr)) {
    NS_WARNING("Failed to create shader resource view for ThebesLayerD3D10.");
  }

  mD2DSurface = new gfxD2DSurface(mTexture, CanUseOpaqueSurface() ?
    gfxASurface::CONTENT_COLOR : gfxASurface::CONTENT_COLOR_ALPHA);

  if (!mD2DSurface || mD2DSurface->CairoStatus()) {
    NS_WARNING("Failed to create surface for ThebesLayerD3D10.");
    mD2DSurface = nsnull;
    return;
  }
}

} /* namespace layers */
} /* namespace mozilla */
