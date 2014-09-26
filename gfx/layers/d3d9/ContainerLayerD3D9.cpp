/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContainerLayerD3D9.h"

#include "PaintedLayerD3D9.h"
#include "ReadbackProcessor.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

ContainerLayerD3D9::ContainerLayerD3D9(LayerManagerD3D9 *aManager)
  : ContainerLayer(aManager, nullptr)
  , LayerD3D9(aManager)
{
  mImplData = static_cast<LayerD3D9*>(this);
}

ContainerLayerD3D9::~ContainerLayerD3D9()
{
  while (mFirstChild) {
    RemoveChild(mFirstChild);
  }
}

Layer*
ContainerLayerD3D9::GetLayer()
{
  return this;
}

LayerD3D9*
ContainerLayerD3D9::GetFirstChildD3D9()
{
  if (!mFirstChild) {
    return nullptr;
  }
  return static_cast<LayerD3D9*>(mFirstChild->ImplData());
}

void
ContainerLayerD3D9::RenderLayer()
{
  nsRefPtr<IDirect3DSurface9> previousRenderTarget;
  nsRefPtr<IDirect3DTexture9> renderTexture;
  float previousRenderTargetOffset[4];
  float renderTargetOffset[] = { 0, 0, 0, 0 };
  float oldViewMatrix[4][4];

  RECT containerD3D9ClipRect; 
  device()->GetScissorRect(&containerD3D9ClipRect);
  // Convert scissor to an nsIntRect. RECT's are exclusive on the bottom and
  // right values.
  nsIntRect oldScissor(containerD3D9ClipRect.left, 
                       containerD3D9ClipRect.top,
                       containerD3D9ClipRect.right - containerD3D9ClipRect.left,
                       containerD3D9ClipRect.bottom - containerD3D9ClipRect.top);

  ReadbackProcessor readback;
  readback.BuildUpdates(this);

  nsIntRect visibleRect = GetEffectiveVisibleRegion().GetBounds();
  bool useIntermediate = UseIntermediateSurface();

  mSupportsComponentAlphaChildren = false;
  if (useIntermediate) {
    nsRefPtr<IDirect3DSurface9> renderSurface;
    if (!mD3DManager->CompositingDisabled()) {
      device()->GetRenderTarget(0, getter_AddRefs(previousRenderTarget));
      HRESULT hr = device()->CreateTexture(visibleRect.width, visibleRect.height, 1,
                                           D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                           D3DPOOL_DEFAULT, getter_AddRefs(renderTexture),
                                           nullptr);
      if (FAILED(hr)) {
        ReportFailure(NS_LITERAL_CSTRING("ContainerLayerD3D9::ContainerRender(): Failed to create texture"),
                                hr);
        return;
      }

      nsRefPtr<IDirect3DSurface9> renderSurface;
      renderTexture->GetSurfaceLevel(0, getter_AddRefs(renderSurface));
      device()->SetRenderTarget(0, renderSurface);
    }

    if (mVisibleRegion.GetNumRects() == 1 && 
        (GetContentFlags() & CONTENT_OPAQUE)) {
      // don't need a background, we're going to paint all opaque stuff
      mSupportsComponentAlphaChildren = true;
    } else {
      Matrix4x4 transform3D = GetEffectiveTransform();
      Matrix transform;
      // If we have an opaque ancestor layer, then we can be sure that
      // all the pixels we draw into are either opaque already or will be
      // covered by something opaque. Otherwise copying up the background is
      // not safe.
      HRESULT hr = E_FAIL;
      if (HasOpaqueAncestorLayer(this) &&
          transform3D.Is2D(&transform) && !ThebesMatrix(transform).HasNonIntegerTranslation()) {
        // Copy background up from below
        RECT dest = { 0, 0, visibleRect.width, visibleRect.height };
        RECT src = dest;
        ::OffsetRect(&src,
                     visibleRect.x + int32_t(transform._31),
                     visibleRect.y + int32_t(transform._32));
        if (!mD3DManager->CompositingDisabled()) {
          hr = device()->
            StretchRect(previousRenderTarget, &src, renderSurface, &dest, D3DTEXF_NONE);
        }
      }
      if (hr == S_OK) {
        mSupportsComponentAlphaChildren = true;
      } else if (!mD3DManager->CompositingDisabled()) {
        device()->
          Clear(0, 0, D3DCLEAR_TARGET, D3DCOLOR_RGBA(0, 0, 0, 0), 0, 0);
      }
    }

    device()->
      GetVertexShaderConstantF(CBvRenderTargetOffset, previousRenderTargetOffset, 1);
    renderTargetOffset[0] = (float)visibleRect.x;
    renderTargetOffset[1] = (float)visibleRect.y;
    device()->
      SetVertexShaderConstantF(CBvRenderTargetOffset, renderTargetOffset, 1);

    gfx3DMatrix viewMatrix;
    /*
     * Matrix to transform to viewport space ( <-1.0, 1.0> topleft,
     * <1.0, -1.0> bottomright)
     */
    viewMatrix._11 = 2.0f / visibleRect.width;
    viewMatrix._22 = -2.0f / visibleRect.height;
    viewMatrix._41 = -1.0f;
    viewMatrix._42 = 1.0f;

    device()->
      GetVertexShaderConstantF(CBmProjection, &oldViewMatrix[0][0], 4);
    device()->
      SetVertexShaderConstantF(CBmProjection, &viewMatrix._11, 4);
  } else {
    mSupportsComponentAlphaChildren = 
        (GetContentFlags() & CONTENT_OPAQUE) ||
        (mParent && 
         mParent->SupportsComponentAlphaChildren());
  }

  nsAutoTArray<Layer*, 12> children;
  SortChildrenBy3DZOrder(children);

  /*
   * Render this container's contents.
   */
  for (uint32_t i = 0; i < children.Length(); i++) {
    LayerD3D9* layerToRender = static_cast<LayerD3D9*>(children.ElementAt(i)->ImplData());

    if (layerToRender->GetLayer()->GetEffectiveVisibleRegion().IsEmpty()) {
      continue;
    }

    nsIntRect scissorRect =
      RenderTargetPixel::ToUntyped(layerToRender->GetLayer()->CalculateScissorRect(RenderTargetPixel::FromUntyped(oldScissor)));
    if (scissorRect.IsEmpty()) {
      continue;
    }

    RECT d3drect;
    d3drect.left = scissorRect.x;
    d3drect.top = scissorRect.y;
    d3drect.right = scissorRect.x + scissorRect.width;
    d3drect.bottom = scissorRect.y + scissorRect.height;
    device()->SetScissorRect(&d3drect);

    if (layerToRender->GetLayer()->GetType() == TYPE_PAINTED) {
      static_cast<PaintedLayerD3D9*>(layerToRender)->RenderPaintedLayer(&readback);
    } else {
      layerToRender->RenderLayer();
    }
  }
    
  if (useIntermediate && !mD3DManager->CompositingDisabled()) {
    device()->SetRenderTarget(0, previousRenderTarget);
    device()->SetVertexShaderConstantF(CBvRenderTargetOffset, previousRenderTargetOffset, 1);
    device()->SetVertexShaderConstantF(CBmProjection, &oldViewMatrix[0][0], 4);

    device()->SetVertexShaderConstantF(CBvLayerQuad,
                                       ShaderConstantRect(visibleRect.x,
                                                          visibleRect.y,
                                                          visibleRect.width,
                                                          visibleRect.height),
                                       1);

    SetShaderTransformAndOpacity();
    mD3DManager->SetShaderMode(DeviceManagerD3D9::RGBALAYER,
                               GetMaskLayer(),
                               GetTransform().CanDraw2D());

    device()->SetTexture(0, renderTexture);
    device()->SetScissorRect(&containerD3D9ClipRect);
    device()->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
  } else {
    device()->SetScissorRect(&containerD3D9ClipRect);
  }
}

void
ContainerLayerD3D9::LayerManagerDestroyed()
{
  while (mFirstChild) {
    GetFirstChildD3D9()->LayerManagerDestroyed();
    RemoveChild(mFirstChild);
  }
}

} /* layers */
} /* mozilla */
