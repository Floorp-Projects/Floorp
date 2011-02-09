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

#include "gfxTeeSurface.h"
#include "gfxUtils.h"

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

ThebesLayerD3D10::InvalidateRegion(const nsIntRegion &aRegion)
{
  mValidRegion.Sub(mValidRegion, aRegion);
}

void ThebesLayerD3D10::CopyRegion(ID3D10Texture2D* aSrc, const nsIntPoint &aSrcOffset,
                                  ID3D10Texture2D* aDest, const nsIntPoint &aDestOffset,
                                  const nsIntRegion &aCopyRegion, nsIntRegion* aValidRegion,
                                  float aXRes, float aYRes)
{
  nsIntRegion retainedRegion;
  nsIntRegionRectIterator iter(aCopyRegion);
  const nsIntRect *r;
  while ((r = iter.Next())) {
    if (r->width * r->height > RETENTION_THRESHOLD) {
      // Calculate the retained rectangle's position on the old and the new
      // surface. We need to scale these rectangles since the visible 
      // region is in unscaled units, and the texture size has been scaled.
      D3D10_BOX box;
      box.left = UINT(floor((r->x - aSrcOffset.x) * aXRes));
      box.top = UINT(floor((r->y - aSrcOffset.y) * aYRes));
      box.right = box.left + UINT(ceil(r->width * aXRes));
      box.bottom = box.top + UINT(ceil(r->height * aYRes));
      box.back = 1;
      box.front = 0;

      device()->CopySubresourceRegion(aDest, 0,
                                      UINT(floor((r->x - aDestOffset.x) * aXRes)),
                                      UINT(floor((r->y - aDestOffset.y) * aYRes)),
                                      0,
                                      aSrc, 0,
                                      &box);

      retainedRegion.Or(retainedRegion, *r);
    }
  }

  // Areas which were valid and were retained are still valid
  aValidRegion->And(*aValidRegion, retainedRegion);  
}

void
ThebesLayerD3D10::RenderLayer()
{
  if (!mTexture) {
    return;
  }

  nsIntRect visibleRect = mVisibleRegion.GetBounds();

  SetEffectTransformAndOpacity();

  ID3D10EffectTechnique *technique;
  if (mTextureOnWhite) {
    technique = effect()->GetTechniqueByName("RenderComponentAlphaLayer");
  } else if (CanUseOpaqueSurface()) {
    technique = effect()->GetTechniqueByName("RenderRGBLayerPremul");
  } else {
    technique = effect()->GetTechniqueByName("RenderRGBALayerPremul");
  }


  nsIntRegionRectIterator iter(mVisibleRegion);

  const nsIntRect *iterRect;
  if (mSRView) {
    effect()->GetVariableByName("tRGB")->AsShaderResource()->SetResource(mSRView);
  }
  if (mSRViewOnWhite) {
    effect()->GetVariableByName("tRGBWhite")->AsShaderResource()->SetResource(mSRViewOnWhite);
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

  SurfaceMode mode = GetSurfaceMode();
  if (mode == SURFACE_COMPONENT_ALPHA &&
      (!mParent || !mParent->SupportsComponentAlphaChildren())) {
    mode = SURFACE_SINGLE_CHANNEL_ALPHA;
  }

  VerifyContentType(mode);

  float xres, yres;
  GetDesiredResolutions(xres, yres);

  // If our resolution changed, we need new sized textures, delete the old ones.
  if (ResolutionChanged(xres, yres)) {
      mTexture = nsnull;
      mTextureOnWhite = nsnull;
  }

  nsIntRect visibleRect = mVisibleRegion.GetBounds();

  if (mTexture) {
    if (!mTextureRegion.IsEqual(mVisibleRegion)) {
      nsRefPtr<ID3D10Texture2D> oldTexture = mTexture;
      mTexture = nsnull;
      nsRefPtr<ID3D10Texture2D> oldTextureOnWhite = mTextureOnWhite;
      mTextureOnWhite = nsnull;

      nsIntRegion retainRegion = mTextureRegion;
      nsIntRect oldBounds = mTextureRegion.GetBounds();
      nsIntRect newBounds = mVisibleRegion.GetBounds();

      CreateNewTextures(gfxIntSize(newBounds.width, newBounds.height), mode);

      // Old visible region will become the region that is covered by both the
      // old and the new visible region.
      retainRegion.And(retainRegion, mVisibleRegion);
      // No point in retaining parts which were not valid.
      retainRegion.And(retainRegion, mValidRegion);

      nsIntRect largeRect = retainRegion.GetLargestRectangle();

      // If we had no hardware texture before, have no retained area larger than
      // the retention threshold or the requested resolution has changed, 
      // we're not retaining and are done here. If our texture creation failed 
      // this can mean a device reset is pending and we should silently ignore the   
      // failure. In the future when device failures are properly handled we 
      // should test for the type of failure and gracefully handle different 
      // failures. See bug 569081.
      if (!oldTexture || !mTexture ||
          largeRect.width * largeRect.height < RETENTION_THRESHOLD ||
          ResolutionChanged(xres, yres)) {
        mValidRegion.SetEmpty();
      } else {
        CopyRegion(oldTexture, oldBounds.TopLeft(),
                   mTexture, newBounds.TopLeft(),
                   retainRegion, &mValidRegion,
                   xres, yres);
        if (oldTextureOnWhite) {
          CopyRegion(oldTextureOnWhite, oldBounds.TopLeft(),
                     mTextureOnWhite, newBounds.TopLeft(),
                     retainRegion, &mValidRegion,
                     xres, yres);
        }
      }
    }
  }
  mTextureRegion = mVisibleRegion;

  if (!mTexture || (mode == SURFACE_COMPONENT_ALPHA && !mTextureOnWhite)) {
    CreateNewTextures(gfxIntSize(visibleRect.width, visibleRect.height), mode);
    mValidRegion.SetEmpty();
  }

  if (!mValidRegion.IsEqual(mVisibleRegion)) {
    LayerManagerD3D10::CallbackInfo cbInfo = mD3DManager->GetCallbackInfo();
    if (!cbInfo.Callback) {
      NS_ERROR("D3D10 should never need to update ThebesLayers in an empty transaction");
      return;
    }

    /* We use the bounds of the visible region because we draw the bounds of
     * this region when we draw this entire texture. We have to make sure that
     * the areas that aren't filled with content get their background drawn.
     * This is an issue for opaque surfaces, which otherwise won't get their
     * background painted.
     */
    nsIntRegion region;
    region.Sub(mVisibleRegion, mValidRegion);

    DrawRegion(region, mode);

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
ThebesLayerD3D10::VerifyContentType(SurfaceMode aMode)
{
  if (mD2DSurface) {
    gfxASurface::gfxContentType type = aMode != SURFACE_SINGLE_CHANNEL_ALPHA ?
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
        
    if (aMode != SURFACE_COMPONENT_ALPHA && mTextureOnWhite) {
      // If we've transitioned away from component alpha, we can delete those resources.
      mD2DSurfaceOnWhite = nsnull;
      mSRViewOnWhite = nsnull;
      mTextureOnWhite = nsnull;
      mValidRegion.SetEmpty();
    }
  }
}

static void
FillSurface(gfxASurface* aSurface, const nsIntRegion& aRegion,
            const nsIntPoint& aOffset, const gfxRGBA& aColor,
            float aXRes, float aYRes)
{
  nsRefPtr<gfxContext> ctx = new gfxContext(aSurface);
  ctx->Scale(aXRes, aYRes);
  ctx->Translate(-gfxPoint(aOffset.x, aOffset.y));
  gfxUtils::PathFromRegion(ctx, aRegion);
  ctx->SetColor(aColor);
  ctx->Fill();
}

void
ThebesLayerD3D10::DrawRegion(nsIntRegion &aRegion, SurfaceMode aMode)
{
  nsIntRect visibleRect = mVisibleRegion.GetBounds();

  if (!mD2DSurface) {
    return;
  }

  float xres, yres;
  GetDesiredResolutions(xres, yres);
  aRegion.ExtendForScaling(xres, yres);

  nsRefPtr<gfxASurface> destinationSurface;
  
  if (aMode == SURFACE_COMPONENT_ALPHA) {
    FillSurface(mD2DSurface, aRegion, visibleRect.TopLeft(), gfxRGBA(0.0, 0.0, 0.0, 1.0), xres, yres);
    FillSurface(mD2DSurfaceOnWhite, aRegion, visibleRect.TopLeft(), gfxRGBA(1.0, 1.0, 1.0, 1.0), xres, yres);
    gfxASurface* surfaces[2] = { mD2DSurface.get(), mD2DSurfaceOnWhite.get() };
    destinationSurface = new gfxTeeSurface(surfaces, NS_ARRAY_LENGTH(surfaces));
    // Using this surface as a source will likely go horribly wrong, since
    // only the onBlack surface will really be used, so alpha information will
    // be incorrect.
    destinationSurface->SetAllowUseAsSource(PR_FALSE);
  } else {
    destinationSurface = mD2DSurface;
  }

  nsRefPtr<gfxContext> context = new gfxContext(destinationSurface);
  // Draw content scaled at our current resolution.
  context->Scale(xres, yres);

  nsIntRegionRectIterator iter(aRegion);
  context->Translate(gfxPoint(-visibleRect.x, -visibleRect.y));
  context->NewPath();
  const nsIntRect *iterRect;
  while ((iterRect = iter.Next())) {
    context->Rectangle(gfxRect(iterRect->x, iterRect->y, iterRect->width, iterRect->height));      
  }
  context->Clip();

  if (aMode == SURFACE_SINGLE_CHANNEL_ALPHA) {
    context->SetOperator(gfxContext::OPERATOR_CLEAR);
    context->Paint();
    context->SetOperator(gfxContext::OPERATOR_OVER);
  }

  mD2DSurface->SetSubpixelAntialiasingEnabled(!(mContentFlags & CONTENT_COMPONENT_ALPHA));

  LayerManagerD3D10::CallbackInfo cbInfo = mD3DManager->GetCallbackInfo();
  cbInfo.Callback(this, context, aRegion, nsIntRegion(), cbInfo.CallbackData);
}

void
ThebesLayerD3D10::CreateNewTextures(const gfxIntSize &aSize, SurfaceMode aMode)
{
  if (aSize.width == 0 || aSize.height == 0) {
    // Nothing to do.
    return;
  }

  // Scale the requested size (in unscaled units) to the actual
  // texture size we require.
  gfxIntSize scaledSize;
  float xres, yres;
  GetDesiredResolutions(xres, yres);
  scaledSize.width = PRInt32(ceil(aSize.width * xres));
  scaledSize.height = PRInt32(ceil(aSize.height * yres));

  CD3D10_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM, scaledSize.width, scaledSize.height, 1, 1);
  desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
  desc.MiscFlags = D3D10_RESOURCE_MISC_GDI_COMPATIBLE;
  HRESULT hr;

  if (!mTexture) {
    hr = device()->CreateTexture2D(&desc, NULL, getter_AddRefs(mTexture));

    if (FAILED(hr)) {
      NS_WARNING("Failed to create new texture for ThebesLayerD3D10!");
      return;
    }

    hr = device()->CreateShaderResourceView(mTexture, NULL, getter_AddRefs(mSRView));

    if (FAILED(hr)) {
      NS_WARNING("Failed to create shader resource view for ThebesLayerD3D10.");
    }

    mD2DSurface = new gfxD2DSurface(mTexture, aMode != SURFACE_SINGLE_CHANNEL_ALPHA ?
      gfxASurface::CONTENT_COLOR : gfxASurface::CONTENT_COLOR_ALPHA);

    if (!mD2DSurface || mD2DSurface->CairoStatus()) {
      NS_WARNING("Failed to create surface for ThebesLayerD3D10.");
      mD2DSurface = nsnull;
      return;
    }

  }

  if (aMode == SURFACE_COMPONENT_ALPHA && !mTextureOnWhite) {
    hr = device()->CreateTexture2D(&desc, NULL, getter_AddRefs(mTextureOnWhite));

    if (FAILED(hr)) {
      NS_WARNING("Failed to create new texture for ThebesLayerD3D10!");
      return;
    }

    hr = device()->CreateShaderResourceView(mTextureOnWhite, NULL, getter_AddRefs(mSRViewOnWhite));

    if (FAILED(hr)) {
      NS_WARNING("Failed to create shader resource view for ThebesLayerD3D10.");
    }

    mD2DSurfaceOnWhite = new gfxD2DSurface(mTextureOnWhite, gfxASurface::CONTENT_COLOR);

    if (!mD2DSurfaceOnWhite || mD2DSurfaceOnWhite->CairoStatus()) {
      NS_WARNING("Failed to create surface for ThebesLayerD3D10.");
      mD2DSurfaceOnWhite = nsnull;
      return;
    }
  }
  
  mXResolution = xres;
  mYResolution = yres;
}
  
void 
ThebesLayerD3D10::GetDesiredResolutions(float& aXRes, float& aYRes)
{
  const gfx3DMatrix& transform = GetLayer()->GetEffectiveTransform();
  gfxMatrix transform2d;
  if (transform.Is2D(&transform2d)) {
    gfxSize scale = transform2d.ScaleFactors(PR_TRUE);
    //Scale factors are normalized to a power of 2 to reduce the number of resolution changes
    aXRes = gfxUtils::ClampToScaleFactor(scale.width);
    aYRes = gfxUtils::ClampToScaleFactor(scale.height);
  } else {
    aXRes = 1.0;
    aYRes = 1.0;
  }
}

bool 
ThebesLayerD3D10::ResolutionChanged(float aXRes, float aYRes)
{
  return aXRes != mXResolution ||
         aYRes != mYResolution;
}

} /* namespace layers */
} /* namespace mozilla */
