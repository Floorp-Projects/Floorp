/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/PLayers.h"

/* This must occur *after* layers/PLayers.h to avoid typedefs conflicts. */
#include "mozilla/Util.h"

#include "ipc/AutoOpenSurface.h"
#include "mozilla/layers/ShadowLayers.h"
#include "ShadowBufferD3D9.h"

#include "ThebesLayerD3D9.h"
#include "gfxPlatform.h"

#include "gfxWindowsPlatform.h"
#include "gfxTeeSurface.h"
#include "gfxUtils.h"
#include "ReadbackProcessor.h"
#include "ReadbackLayer.h"

namespace mozilla {
namespace layers {

ThebesLayerD3D9::ThebesLayerD3D9(LayerManagerD3D9 *aManager)
  : ThebesLayer(aManager, NULL)
  , LayerD3D9(aManager)
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
ThebesLayerD3D9::InvalidateRegion(const nsIntRegion &aRegion)
{
  mInvalidRegion.Or(mInvalidRegion, aRegion);
  mInvalidRegion.SimplifyOutward(10);
  mValidRegion.Sub(mValidRegion, mInvalidRegion);
}

void
ThebesLayerD3D9::CopyRegion(IDirect3DTexture9* aSrc, const nsIntPoint &aSrcOffset,
                            IDirect3DTexture9* aDest, const nsIntPoint &aDestOffset,
                            const nsIntRegion &aCopyRegion, nsIntRegion* aValidRegion)
{
  nsRefPtr<IDirect3DSurface9> srcSurface, dstSurface;
  aSrc->GetSurfaceLevel(0, getter_AddRefs(srcSurface));
  aDest->GetSurfaceLevel(0, getter_AddRefs(dstSurface));

  nsIntRegion retainedRegion;
  nsIntRegionRectIterator iter(aCopyRegion);
  const nsIntRect *r;
  while ((r = iter.Next())) {
    if (r->width * r->height > RETENTION_THRESHOLD) {
      RECT oldRect, newRect;

      // Calculate the retained rectangle's position on the old and the new
      // surface.
      oldRect.left = r->x - aSrcOffset.x;
      oldRect.top = r->y - aSrcOffset.y;
      oldRect.right = oldRect.left + r->width;
      oldRect.bottom = oldRect.top + r->height;

      newRect.left = r->x - aDestOffset.x;
      newRect.top = r->y - aDestOffset.y;
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
  aValidRegion->And(*aValidRegion, retainedRegion);
}

static uint64_t RectArea(const nsIntRect& aRect)
{
  return aRect.width*uint64_t(aRect.height);
}

void
ThebesLayerD3D9::UpdateTextures(SurfaceMode aMode)
{
  nsIntRect visibleRect = mVisibleRegion.GetBounds();

  if (HaveTextures(aMode)) {
    if (!mTextureRect.IsEqualInterior(visibleRect)) {
      nsRefPtr<IDirect3DTexture9> oldTexture = mTexture;
      nsRefPtr<IDirect3DTexture9> oldTextureOnWhite = mTextureOnWhite;

      NS_ASSERTION(mTextureRect.Contains(mValidRegion.GetBounds()),
                   "How can we have valid data outside the texture?");
      nsIntRegion retainRegion;
      // The region we want to retain is the valid data that is inside
      // the new visible region
      retainRegion.And(mValidRegion, mVisibleRegion);

      CreateNewTextures(gfxIntSize(visibleRect.width, visibleRect.height), aMode);

      // If our texture creation failed this can mean a device reset is pending and we
      // should silently ignore the failure. In the future when device failures
      // are properly handled we should test for the type of failure and gracefully
      // handle different failures. See bug 569081.
      if (!HaveTextures(aMode)) {
        mValidRegion.SetEmpty();
      } else {
        CopyRegion(oldTexture, mTextureRect.TopLeft(), mTexture, visibleRect.TopLeft(),
                   retainRegion, &mValidRegion);
        if (aMode == SURFACE_COMPONENT_ALPHA) {
          CopyRegion(oldTextureOnWhite, mTextureRect.TopLeft(), mTextureOnWhite, visibleRect.TopLeft(),
                     retainRegion, &mValidRegion);
        }
      }

      mTextureRect = visibleRect;
    }
  } else {
    CreateNewTextures(gfxIntSize(visibleRect.width, visibleRect.height), aMode);
    mTextureRect = visibleRect;
    
    NS_ASSERTION(mValidRegion.IsEmpty(), "Someone forgot to empty the region");
  }
}

void
ThebesLayerD3D9::RenderRegion(const nsIntRegion& aRegion)
{
  nsIntRegionRectIterator iter(aRegion);

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
        (float)(iterRect->x - mTextureRect.x) / (float)mTextureRect.width,
        (float)(iterRect->y - mTextureRect.y) / (float)mTextureRect.height,
        (float)iterRect->width / (float)mTextureRect.width,
        (float)iterRect->height / (float)mTextureRect.height), 1);

    device()->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
  }
}

void
ThebesLayerD3D9::RenderThebesLayer(ReadbackProcessor* aReadback)
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
    if (MayResample()) {
      neededRegion = newTextureRect;
      if (mode == SURFACE_OPAQUE) {
        // We're going to paint outside the visible region, but layout hasn't
        // promised that it will paint opaquely there, so we'll have to
        // treat this layer as transparent.
        mode = SURFACE_SINGLE_CHANNEL_ALPHA;
      }
    }
  }

  VerifyContentType(mode);
  UpdateTextures(mode);
  if (!HaveTextures(mode)) {
    NS_WARNING("Texture creation failed");
    return;
  }

  nsTArray<ReadbackProcessor::Update> readbackUpdates;
  nsIntRegion readbackRegion;
  if (aReadback && UsedForReadback()) {
    aReadback->GetThebesLayerUpdates(this, &readbackUpdates, &readbackRegion);
  }

  // Because updates to D3D9 ThebesLayers are rendered with the CPU, we don't
  // have to do readback from D3D9 surfaces. Instead we make sure that any area
  // needed for readback is included in the drawRegion we ask layout to render.
  // Then the readback areas we need can be copied out of the temporary
  // destinationSurface in DrawRegion.
  nsIntRegion drawRegion;
  drawRegion.Sub(neededRegion, mValidRegion);
  drawRegion.Or(drawRegion, readbackRegion);
  // NS_ASSERTION(mVisibleRegion.Contains(region), "Bad readback region!");

  if (!drawRegion.IsEmpty()) {
    LayerManagerD3D9::CallbackInfo cbInfo = mD3DManager->GetCallbackInfo();
    if (!cbInfo.Callback) {
      NS_ERROR("D3D9 should never need to update ThebesLayers in an empty transaction");
      return;
    }

    DrawRegion(drawRegion, mode, readbackUpdates);

    mValidRegion = neededRegion;
  }

  if (mD3DManager->CompositingDisabled()) {
    return;
  }

  SetShaderTransformAndOpacity();

  if (mode == SURFACE_COMPONENT_ALPHA) {
    mD3DManager->SetShaderMode(DeviceManagerD3D9::COMPONENTLAYERPASS1,
                               GetMaskLayer());
    device()->SetTexture(0, mTexture);
    device()->SetTexture(1, mTextureOnWhite);
    device()->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
    device()->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCCOLOR);
    RenderRegion(neededRegion);

    mD3DManager->SetShaderMode(DeviceManagerD3D9::COMPONENTLAYERPASS2,
                               GetMaskLayer());
    device()->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
    device()->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
    RenderRegion(neededRegion);

    // Restore defaults
    device()->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
    device()->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device()->SetTexture(1, NULL);
  } else {
    mD3DManager->SetShaderMode(DeviceManagerD3D9::RGBALAYER,
                               GetMaskLayer());
    device()->SetTexture(0, mTexture);
    RenderRegion(neededRegion);
  }

  // Set back to default.
  device()->SetVertexShaderConstantF(CBvTextureCoords,
                                     ShaderConstantRect(0, 0, 1.0f, 1.0f),
                                     1);
}

void
ThebesLayerD3D9::CleanResources()
{
  mTexture = nullptr;
  mTextureOnWhite = nullptr;
  mValidRegion.SetEmpty();
}

void
ThebesLayerD3D9::LayerManagerDestroyed()
{
  mD3DManager->deviceManager()->mLayersWithResources.RemoveElement(this);
  mD3DManager = nullptr;
}

Layer*
ThebesLayerD3D9::GetLayer()
{
  return this;
}

bool
ThebesLayerD3D9::IsEmpty()
{
  return !mTexture;
}

void
ThebesLayerD3D9::VerifyContentType(SurfaceMode aMode)
{
  if (!mTexture)
    return;

  D3DSURFACE_DESC desc;
  mTexture->GetLevelDesc(0, &desc);

  switch (aMode) {
  case SURFACE_OPAQUE:
    if (desc.Format == D3DFMT_X8R8G8B8 && !mTextureOnWhite)
      return;
    break;

  case SURFACE_SINGLE_CHANNEL_ALPHA:
    if (desc.Format == D3DFMT_A8R8G8B8 && !mTextureOnWhite)
      return;
    break;

  case SURFACE_COMPONENT_ALPHA:
    if (mTextureOnWhite) {
      NS_ASSERTION(desc.Format == D3DFMT_X8R8G8B8, "Wrong format for component alpha texture");
      return;
    }
    break;
  }

  // The new format isn't compatible with the old texture(s), toss out the old
  // texture(s).
  mTexture = nullptr;
  mTextureOnWhite = nullptr;
  mValidRegion.SetEmpty();
}

class OpaqueRenderer {
public:
  OpaqueRenderer(const nsIntRegion& aUpdateRegion) :
    mUpdateRegion(aUpdateRegion), mDC(NULL) {}
  ~OpaqueRenderer() { End(); }
  already_AddRefed<gfxWindowsSurface> Begin(LayerD3D9* aLayer);
  void End();
  IDirect3DTexture9* GetTexture() { return mTmpTexture; }

private:
  const nsIntRegion& mUpdateRegion;
  nsRefPtr<IDirect3DTexture9> mTmpTexture;
  nsRefPtr<IDirect3DSurface9> mSurface;
  HDC mDC;
};

already_AddRefed<gfxWindowsSurface>
OpaqueRenderer::Begin(LayerD3D9* aLayer)
{
  nsIntRect bounds = mUpdateRegion.GetBounds();

  HRESULT hr = aLayer->device()->
      CreateTexture(bounds.width, bounds.height, 1, 0, D3DFMT_X8R8G8B8,
                    D3DPOOL_SYSTEMMEM, getter_AddRefs(mTmpTexture), NULL);

  if (FAILED(hr)) {
    aLayer->ReportFailure(NS_LITERAL_CSTRING("Failed to create temporary texture in system memory."), hr);
    return nullptr;
  }

  hr = mTmpTexture->GetSurfaceLevel(0, getter_AddRefs(mSurface));

  if (FAILED(hr)) {
    // Uh-oh, bail.
    NS_WARNING("Failed to get texture surface level.");
    return nullptr;
  }

  hr = mSurface->GetDC(&mDC);
  if (FAILED(hr)) {
    NS_WARNING("Failed to get device context for texture surface.");
    return nullptr;
  }

  nsRefPtr<gfxWindowsSurface> result = new gfxWindowsSurface(mDC);
  return result.forget();
}

void
OpaqueRenderer::End()
{
  if (mSurface && mDC) {
    mSurface->ReleaseDC(mDC);
    mSurface = NULL;
    mDC = NULL;
  }
}

static void
FillSurface(gfxASurface* aSurface, const nsIntRegion& aRegion,
            const nsIntPoint& aOffset, const gfxRGBA& aColor)
{
  nsRefPtr<gfxContext> ctx = new gfxContext(aSurface);
  ctx->Translate(-gfxPoint(aOffset.x, aOffset.y));
  gfxUtils::ClipToRegion(ctx, aRegion);
  ctx->SetColor(aColor);
  ctx->Paint();
}

void
ThebesLayerD3D9::DrawRegion(nsIntRegion &aRegion, SurfaceMode aMode,
                            const nsTArray<ReadbackProcessor::Update>& aReadbackUpdates)
{
  HRESULT hr;
  nsIntRect visibleRect = mVisibleRegion.GetBounds();

  nsRefPtr<gfxASurface> destinationSurface;
  nsIntRect bounds = aRegion.GetBounds();
  nsRefPtr<IDirect3DTexture9> tmpTexture;
  OpaqueRenderer opaqueRenderer(aRegion);
  OpaqueRenderer opaqueRendererOnWhite(aRegion);

  switch (aMode)
  {
    case SURFACE_OPAQUE:
      destinationSurface = opaqueRenderer.Begin(this);
      break;

    case SURFACE_SINGLE_CHANNEL_ALPHA: {
      hr = device()->CreateTexture(bounds.width, bounds.height, 1,
                                   0, D3DFMT_A8R8G8B8,
                                   D3DPOOL_SYSTEMMEM, getter_AddRefs(tmpTexture), NULL);

      if (FAILED(hr)) {
        ReportFailure(NS_LITERAL_CSTRING("Failed to create temporary texture in system memory."), hr);
        return;
      }

      // XXX - We may consider retaining a SYSTEMMEM texture texture the size
      // of our DEFAULT texture and then use UpdateTexture and add dirty rects
      // to update in a single call.
      nsRefPtr<gfxWindowsSurface> dest = new gfxWindowsSurface(
          gfxIntSize(bounds.width, bounds.height), gfxASurface::ImageFormatARGB32);
      // If the contents of this layer don't require component alpha in the
      // end of rendering, it's safe to enable Cleartype since all the Cleartype
      // glyphs must be over (or under) opaque pixels.
      dest->SetSubpixelAntialiasingEnabled(!(mContentFlags & CONTENT_COMPONENT_ALPHA));
      destinationSurface = dest.forget();
      break;
    }

    case SURFACE_COMPONENT_ALPHA: {
      nsRefPtr<gfxWindowsSurface> onBlack = opaqueRenderer.Begin(this);
      nsRefPtr<gfxWindowsSurface> onWhite = opaqueRendererOnWhite.Begin(this);
      if (onBlack && onWhite) {
        FillSurface(onBlack, aRegion, bounds.TopLeft(), gfxRGBA(0.0, 0.0, 0.0, 1.0));
        FillSurface(onWhite, aRegion, bounds.TopLeft(), gfxRGBA(1.0, 1.0, 1.0, 1.0));
        gfxASurface* surfaces[2] = { onBlack.get(), onWhite.get() };
        destinationSurface = new gfxTeeSurface(surfaces, ArrayLength(surfaces));
        // Using this surface as a source will likely go horribly wrong, since
        // only the onBlack surface will really be used, so alpha information will
        // be incorrect.
        destinationSurface->SetAllowUseAsSource(false);
      }
      break;
    }
  }

  if (!destinationSurface)
    return;

  nsRefPtr<gfxContext> context = new gfxContext(destinationSurface);
  context->Translate(gfxPoint(-bounds.x, -bounds.y));
  LayerManagerD3D9::CallbackInfo cbInfo = mD3DManager->GetCallbackInfo();
  cbInfo.Callback(this, context, aRegion, nsIntRegion(), cbInfo.CallbackData);

  for (uint32_t i = 0; i < aReadbackUpdates.Length(); ++i) {
    NS_ASSERTION(aMode == SURFACE_OPAQUE,
                 "Transparent surfaces should not be used for readback");
    const ReadbackProcessor::Update& update = aReadbackUpdates[i];
    nsIntPoint offset = update.mLayer->GetBackgroundLayerOffset();
    nsRefPtr<gfxContext> ctx =
        update.mLayer->GetSink()->BeginUpdate(update.mUpdateRect + offset,
                                              update.mSequenceCounter);
    if (ctx) {
      ctx->Translate(gfxPoint(offset.x, offset.y));
      ctx->SetSource(destinationSurface, gfxPoint(bounds.x, bounds.y));
      ctx->Paint();
      update.mLayer->GetSink()->EndUpdate(ctx, update.mUpdateRect + offset);
    }
  }

  nsAutoTArray<IDirect3DTexture9*,2> srcTextures;
  nsAutoTArray<IDirect3DTexture9*,2> destTextures;
  switch (aMode)
  {
    case SURFACE_OPAQUE:
      opaqueRenderer.End();
      srcTextures.AppendElement(opaqueRenderer.GetTexture());
      destTextures.AppendElement(mTexture);
      break;

    case SURFACE_SINGLE_CHANNEL_ALPHA: {
      LockTextureRectD3D9 textureLock(tmpTexture);
      if (!textureLock.HasLock()) {
        NS_WARNING("Failed to lock ThebesLayer tmpTexture texture.");
        return;
      }

      D3DLOCKED_RECT r = textureLock.GetLockRect();

      nsRefPtr<gfxImageSurface> imgSurface =
        new gfxImageSurface((unsigned char *)r.pBits,
                            bounds.Size(),
                            r.Pitch,
                            gfxASurface::ImageFormatARGB32);

      if (destinationSurface) {
        nsRefPtr<gfxContext> context = new gfxContext(imgSurface);
        context->SetSource(destinationSurface);
        context->SetOperator(gfxContext::OPERATOR_SOURCE);
        context->Paint();
      }

      imgSurface = NULL;

      srcTextures.AppendElement(tmpTexture);
      destTextures.AppendElement(mTexture);
      break;
    }

    case SURFACE_COMPONENT_ALPHA: {
      opaqueRenderer.End();
      opaqueRendererOnWhite.End();
      srcTextures.AppendElement(opaqueRenderer.GetTexture());
      destTextures.AppendElement(mTexture);
      srcTextures.AppendElement(opaqueRendererOnWhite.GetTexture());
      destTextures.AppendElement(mTextureOnWhite);
      break;
    }
  }
  NS_ASSERTION(srcTextures.Length() == destTextures.Length(), "Mismatched lengths");

  // Copy to the texture.
  for (uint32_t i = 0; i < srcTextures.Length(); ++i) {
    nsRefPtr<IDirect3DSurface9> srcSurface;
    nsRefPtr<IDirect3DSurface9> dstSurface;

    destTextures[i]->GetSurfaceLevel(0, getter_AddRefs(dstSurface));
    srcTextures[i]->GetSurfaceLevel(0, getter_AddRefs(srcSurface));

    nsIntRegionRectIterator iter(aRegion);
    const nsIntRect *iterRect;
    while ((iterRect = iter.Next())) {
      RECT rect;
      rect.left = iterRect->x - bounds.x;
      rect.top = iterRect->y - bounds.y;
      rect.right = iterRect->XMost() - bounds.x;
      rect.bottom = iterRect->YMost() - bounds.y;

      POINT point;
      point.x = iterRect->x - visibleRect.x;
      point.y = iterRect->y - visibleRect.y;
      device()->UpdateSurface(srcSurface, &rect, dstSurface, &point);
    }
  }
}

void
ThebesLayerD3D9::CreateNewTextures(const gfxIntSize &aSize,
                                   SurfaceMode aMode)
{
  if (aSize.width == 0 || aSize.height == 0) {
    // Nothing to do.
    return;
  }

  mTexture = nullptr;
  mTextureOnWhite = nullptr;
  HRESULT hr = device()->CreateTexture(aSize.width, aSize.height, 1,
                                       D3DUSAGE_RENDERTARGET,
                                       aMode != SURFACE_SINGLE_CHANNEL_ALPHA ? D3DFMT_X8R8G8B8 : D3DFMT_A8R8G8B8,
                                       D3DPOOL_DEFAULT, getter_AddRefs(mTexture), NULL);
  if (FAILED(hr)) {
    ReportFailure(NS_LITERAL_CSTRING("ThebesLayerD3D9::CreateNewTextures(): Failed to create texture"),
                  hr);
    return;
  }

  if (aMode == SURFACE_COMPONENT_ALPHA) {
    hr = device()->CreateTexture(aSize.width, aSize.height, 1,
                                 D3DUSAGE_RENDERTARGET,
                                 D3DFMT_X8R8G8B8,
                                 D3DPOOL_DEFAULT, getter_AddRefs(mTextureOnWhite), NULL);
    if (FAILED(hr)) {
      ReportFailure(NS_LITERAL_CSTRING("ThebesLayerD3D9::CreateNewTextures(): Failed to create texture (2)"),
                    hr);
      return;
    }
  }
}

ShadowThebesLayerD3D9::ShadowThebesLayerD3D9(LayerManagerD3D9 *aManager)
  : ShadowThebesLayer(aManager, nullptr)
  , LayerD3D9(aManager)
{
  mImplData = static_cast<LayerD3D9*>(this);
}

ShadowThebesLayerD3D9::~ShadowThebesLayerD3D9()
{}

void
ShadowThebesLayerD3D9::Swap(const ThebesBuffer& aNewFront,
                           const nsIntRegion& aUpdatedRegion,
                           OptionalThebesBuffer* aNewBack,
                           nsIntRegion* aNewBackValidRegion,
                           OptionalThebesBuffer* aReadOnlyFront,
                           nsIntRegion* aFrontUpdatedRegion)
{
  if (!mBuffer) {
    mBuffer = new ShadowBufferD3D9(this);
  }

  if (mBuffer) {
    AutoOpenSurface surf(OPEN_READ_ONLY, aNewFront.buffer());
    mBuffer->Upload(surf.Get(), GetVisibleRegion().GetBounds());
  }

  *aNewBack = aNewFront;
  *aNewBackValidRegion = mValidRegion;
  *aReadOnlyFront = null_t();
  aFrontUpdatedRegion->SetEmpty();
}

void
ShadowThebesLayerD3D9::DestroyFrontBuffer()
{
  mBuffer = nullptr;
}

void
ShadowThebesLayerD3D9::Disconnect()
{
  mBuffer = nullptr;
}

Layer*
ShadowThebesLayerD3D9::GetLayer()
{
  return this;
}

bool
ShadowThebesLayerD3D9::IsEmpty()
{
  return !mBuffer;
}

void
ShadowThebesLayerD3D9::RenderThebesLayer()
{
  if (!mBuffer || mD3DManager->CompositingDisabled()) {
    return;
  }
  NS_ABORT_IF_FALSE(mBuffer, "should have a buffer here");

  mBuffer->RenderTo(mD3DManager, GetEffectiveVisibleRegion());
}

void
ShadowThebesLayerD3D9::CleanResources()
{
  mBuffer = nullptr;
  mValidRegion.SetEmpty();
}

void
ShadowThebesLayerD3D9::LayerManagerDestroyed()
{
  mD3DManager->deviceManager()->mLayersWithResources.RemoveElement(this);
  mD3DManager = nullptr;
}

} /* namespace layers */
} /* namespace mozilla */
