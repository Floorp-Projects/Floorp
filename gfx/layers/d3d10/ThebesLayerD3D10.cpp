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

#include "mozilla/layers/PLayers.h"

/* This must occur *after* layers/PLayers.h to avoid typedefs conflicts. */
#include "mozilla/Util.h"

#include "ThebesLayerD3D10.h"
#include "gfxPlatform.h"

#include "gfxWindowsPlatform.h"
#ifdef CAIRO_HAS_D2D_SURFACE
#include "gfxD2DSurface.h"
#endif

#include "../d3d9/Nv3DVUtils.h"
#include "gfxTeeSurface.h"
#include "gfxUtils.h"
#include "ReadbackLayer.h"
#include "ReadbackProcessor.h"

namespace mozilla {
namespace layers {

ThebesLayerD3D10::ThebesLayerD3D10(LayerManagerD3D10 *aManager)
  : ThebesLayer(aManager, NULL)
  , LayerD3D10(aManager)
  , mCurrentSurfaceMode(SURFACE_OPAQUE)
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
                                  const nsIntRegion &aCopyRegion, nsIntRegion* aValidRegion)
{
  nsIntRegion retainedRegion;
  nsIntRegionRectIterator iter(aCopyRegion);
  const nsIntRect *r;
  while ((r = iter.Next())) {
    if (r->width * r->height > RETENTION_THRESHOLD) {
      // Calculate the retained rectangle's position on the old and the new
      // surface.
      D3D10_BOX box;
      box.left = r->x - aSrcOffset.x;
      box.top = r->y - aSrcOffset.y;
      box.right = box.left + r->width;
      box.bottom = box.top + r->height;
      box.back = 1;
      box.front = 0;

      device()->CopySubresourceRegion(aDest, 0,
                                      r->x - aDestOffset.x,
                                      r->y - aDestOffset.y,
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

  SetEffectTransformAndOpacity();

  ID3D10EffectTechnique *technique;
  switch (mCurrentSurfaceMode) {
  case SURFACE_COMPONENT_ALPHA:
    technique = effect()->GetTechniqueByName("RenderComponentAlphaLayer");
    break;
  case SURFACE_OPAQUE:
    technique = effect()->GetTechniqueByName("RenderRGBLayerPremul");
    break;
  case SURFACE_SINGLE_CHANNEL_ALPHA:
    technique = effect()->GetTechniqueByName("RenderRGBALayerPremul");
    break;
  default:
    NS_ERROR("Unknown mode");
    return;
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
        (float)(iterRect->x - mTextureRect.x) / (float)mTextureRect.width,
        (float)(iterRect->y - mTextureRect.y) / (float)mTextureRect.height,
        (float)iterRect->width / (float)mTextureRect.width,
        (float)iterRect->height / (float)mTextureRect.height)
      );

    technique->GetPassByIndex(0)->Apply(0);
    device()->Draw(4, 0);
  }

  // Set back to default.
  effect()->GetVariableByName("vTextureCoords")->AsVector()->
    SetFloatVector(ShaderConstantRectD3D10(0, 0, 1.0f, 1.0f));
}

void
ThebesLayerD3D10::Validate(ReadbackProcessor *aReadback)
{
  if (mVisibleRegion.IsEmpty()) {
    return;
  }

  nsIntRect newTextureRect = mVisibleRegion.GetBounds();

  SurfaceMode mode = GetSurfaceMode();
  if (mode == SURFACE_COMPONENT_ALPHA &&
      (!mParent || !mParent->SupportsComponentAlphaChildren())) {
    mode = SURFACE_SINGLE_CHANNEL_ALPHA;
  }
  // If we have a transform that requires resampling of our texture, then
  // we need to make sure we don't sample pixels that haven't been drawn.
  // We clamp sample coordinates to the texture rect, but when the visible region
  // doesn't fill the entire texture rect we need to make sure we draw all the
  // pixels in the texture rect anyway in case they get sampled.
  nsIntRegion neededRegion = mVisibleRegion;
  if (!neededRegion.GetBounds().IsEqualInterior(newTextureRect) ||
      neededRegion.GetNumRects() > 1) {
    gfxMatrix transform2d;
    if (!GetEffectiveTransform().Is2D(&transform2d) ||
        transform2d.HasNonIntegerTranslation()) {
      neededRegion = newTextureRect;
      if (mode == SURFACE_OPAQUE) {
        // We're going to paint outside the visible region, but layout hasn't
        // promised that it will paint opaquely there, so we'll have to
        // treat this layer as transparent.
        mode = SURFACE_SINGLE_CHANNEL_ALPHA;
      }
    }
  }
  mCurrentSurfaceMode = mode;

  VerifyContentType(mode);

  nsTArray<ReadbackProcessor::Update> readbackUpdates;
  nsIntRegion readbackRegion;
  if (aReadback && UsedForReadback()) {
    aReadback->GetThebesLayerUpdates(this, &readbackUpdates, &readbackRegion);
  }

  if (mTexture) {
    if (!mTextureRect.IsEqualInterior(newTextureRect)) {
      nsRefPtr<ID3D10Texture2D> oldTexture = mTexture;
      mTexture = nsnull;
      nsRefPtr<ID3D10Texture2D> oldTextureOnWhite = mTextureOnWhite;
      mTextureOnWhite = nsnull;

      nsIntRegion retainRegion = mTextureRect;
      // Old visible region will become the region that is covered by both the
      // old and the new visible region.
      retainRegion.And(retainRegion, mVisibleRegion);
      // No point in retaining parts which were not valid.
      retainRegion.And(retainRegion, mValidRegion);

      CreateNewTextures(gfxIntSize(newTextureRect.width, newTextureRect.height), mode);

      nsIntRect largeRect = retainRegion.GetLargestRectangle();

      // If we had no hardware texture before, or have no retained area larger than
      // the retention threshold, we're not retaining and are done here.
      // If our texture creation failed this can mean a device reset is pending
      // and we should silently ignore the failure. In the future when device
      // failures are properly handled we should test for the type of failure
      // and gracefully handle different failures. See bug 569081.
      if (!oldTexture || !mTexture ||
          largeRect.width * largeRect.height < RETENTION_THRESHOLD) {
        mValidRegion.SetEmpty();
      } else {
        CopyRegion(oldTexture, mTextureRect.TopLeft(),
                   mTexture, newTextureRect.TopLeft(),
                   retainRegion, &mValidRegion);
        if (oldTextureOnWhite) {
          CopyRegion(oldTextureOnWhite, mTextureRect.TopLeft(),
                     mTextureOnWhite, newTextureRect.TopLeft(),
                     retainRegion, &mValidRegion);
        }
      }
    }
  }
  mTextureRect = newTextureRect;

  if (!mTexture || (mode == SURFACE_COMPONENT_ALPHA && !mTextureOnWhite)) {
    CreateNewTextures(gfxIntSize(newTextureRect.width, newTextureRect.height), mode);
    mValidRegion.SetEmpty();
  }

  nsIntRegion drawRegion;
  drawRegion.Sub(neededRegion, mValidRegion);

  if (!drawRegion.IsEmpty()) {
    LayerManagerD3D10::CallbackInfo cbInfo = mD3DManager->GetCallbackInfo();
    if (!cbInfo.Callback) {
      NS_ERROR("D3D10 should never need to update ThebesLayers in an empty transaction");
      return;
    }

    DrawRegion(drawRegion, mode);

    if (readbackUpdates.Length() > 0) {
      CD3D10_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM,
                                 newTextureRect.width, newTextureRect.height,
                                 1, 1, 0, D3D10_USAGE_STAGING,
                                 D3D10_CPU_ACCESS_READ);

      nsRefPtr<ID3D10Texture2D> readbackTexture;
      HRESULT hr = device()->CreateTexture2D(&desc, NULL, getter_AddRefs(readbackTexture));
      if (FAILED(hr)) {
        LayerManagerD3D10::ReportFailure(NS_LITERAL_CSTRING("ThebesLayerD3D10::Validate(): Failed to create texture"),
                                         hr);
        return;
      }

      device()->CopyResource(readbackTexture, mTexture);

      for (PRUint32 i = 0; i < readbackUpdates.Length(); i++) {
        mD3DManager->readbackManager()->PostTask(readbackTexture,
                                                 &readbackUpdates[i],
                                                 gfxPoint(newTextureRect.x, newTextureRect.y));
      }
    }

    mValidRegion = neededRegion;
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
            const nsIntPoint& aOffset, const gfxRGBA& aColor)
{
  nsRefPtr<gfxContext> ctx = new gfxContext(aSurface);
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

  nsRefPtr<gfxASurface> destinationSurface;
  
  if (aMode == SURFACE_COMPONENT_ALPHA) {
    FillSurface(mD2DSurface, aRegion, visibleRect.TopLeft(), gfxRGBA(0.0, 0.0, 0.0, 1.0));
    FillSurface(mD2DSurfaceOnWhite, aRegion, visibleRect.TopLeft(), gfxRGBA(1.0, 1.0, 1.0, 1.0));
    gfxASurface* surfaces[2] = { mD2DSurface.get(), mD2DSurfaceOnWhite.get() };
    destinationSurface = new gfxTeeSurface(surfaces, ArrayLength(surfaces));
    // Using this surface as a source will likely go horribly wrong, since
    // only the onBlack surface will really be used, so alpha information will
    // be incorrect.
    destinationSurface->SetAllowUseAsSource(false);
  } else {
    destinationSurface = mD2DSurface;
  }

  nsRefPtr<gfxContext> context = new gfxContext(destinationSurface);

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

  CD3D10_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM, aSize.width, aSize.height, 1, 1);
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
}
 
ShadowThebesLayerD3D10::ShadowThebesLayerD3D10(LayerManagerD3D10* aManager)
  : ShadowThebesLayer(aManager, NULL)
  , LayerD3D10(aManager)
{
  mImplData = static_cast<LayerD3D10*>(this);
}

ShadowThebesLayerD3D10::~ShadowThebesLayerD3D10()
{
}

void
ShadowThebesLayerD3D10::Swap(
  const ThebesBuffer& aNewFront, const nsIntRegion& aUpdatedRegion,
  OptionalThebesBuffer* aNewBack, nsIntRegion* aNewBackValidRegion,
  OptionalThebesBuffer* aReadOnlyFront, nsIntRegion* aFrontUpdatedRegion)
{
  nsRefPtr<ID3D10Texture2D> newBackBuffer = mTexture;

  mTexture = OpenForeign(mD3DManager->device(), aNewFront.buffer());
  NS_ABORT_IF_FALSE(mTexture, "Couldn't open foreign texture");

  // The content process tracks back/front buffers on its own, so
  // the newBack is in essence unused.
  aNewBack->get_ThebesBuffer().buffer() = aNewFront.buffer();

  // The content process doesn't need to read back from the front
  // buffer (yet).
  *aReadOnlyFront = null_t();

  // FIXME/bug 662109: synchronize using KeyedMutex
}

void
ShadowThebesLayerD3D10::DestroyFrontBuffer()
{
}

void
ShadowThebesLayerD3D10::Disconnect()
{
}

void
ShadowThebesLayerD3D10::RenderLayer()
{
  if (!mTexture) {
    return;
  }

  // FIXME/bug 662109: synchronize using KeyedMutex
  
  nsRefPtr<ID3D10ShaderResourceView> srView;
  HRESULT hr = device()->CreateShaderResourceView(mTexture, NULL, getter_AddRefs(srView));
  if (FAILED(hr)) {
      NS_WARNING("Failed to create shader resource view for ThebesLayerD3D10.");
  }


  SetEffectTransformAndOpacity();

  ID3D10EffectTechnique *technique =
      effect()->GetTechniqueByName("RenderRGBLayerPremul");

  effect()->GetVariableByName("tRGB")->AsShaderResource()->SetResource(srView);

  nsIntRect textureRect = GetVisibleRegion().GetBounds();

  nsIntRegionRectIterator iter(mVisibleRegion);
  while (const nsIntRect *iterRect = iter.Next()) {
    effect()->GetVariableByName("vLayerQuad")->AsVector()->SetFloatVector(
      ShaderConstantRectD3D10(
        (float)iterRect->x,
        (float)iterRect->y,
        (float)iterRect->width,
        (float)iterRect->height)
      );

    effect()->GetVariableByName("vTextureCoords")->AsVector()->SetFloatVector(
      ShaderConstantRectD3D10(
        (float)(iterRect->x - textureRect.x) / (float)textureRect.width,
        (float)(iterRect->y - textureRect.y) / (float)textureRect.height,
        (float)iterRect->width / (float)textureRect.width,
        (float)iterRect->height / (float)textureRect.height)
      );

    technique->GetPassByIndex(0)->Apply(0);
    device()->Draw(4, 0);
  }

  // FIXME/bug 662109: synchronize using KeyedMutex

  // Set back to default.
  effect()->GetVariableByName("vTextureCoords")->AsVector()->
    SetFloatVector(ShaderConstantRectD3D10(0, 0, 1.0f, 1.0f));
}

void
ShadowThebesLayerD3D10::Validate()
{
}

void
ShadowThebesLayerD3D10::LayerManagerDestroyed()
{
}

} /* namespace layers */
} /* namespace mozilla */
