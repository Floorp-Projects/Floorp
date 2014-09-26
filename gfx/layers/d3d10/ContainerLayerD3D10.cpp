/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContainerLayerD3D10.h"

#include "PaintedLayerD3D10.h"
#include "ReadbackProcessor.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

ContainerLayerD3D10::ContainerLayerD3D10(LayerManagerD3D10 *aManager)
  : ContainerLayer(aManager, nullptr)
  , LayerD3D10(aManager)
{
  mImplData = static_cast<LayerD3D10*>(this);
}

ContainerLayerD3D10::~ContainerLayerD3D10()
{
  while (mFirstChild) {
    RemoveChild(mFirstChild);
  }
}

Layer*
ContainerLayerD3D10::GetLayer()
{
  return this;
}

LayerD3D10*
ContainerLayerD3D10::GetFirstChildD3D10()
{
  if (!mFirstChild) {
    return nullptr;
  }
  return static_cast<LayerD3D10*>(mFirstChild->ImplData());
}

void
ContainerLayerD3D10::RenderLayer()
{
  float renderTargetOffset[] = { 0, 0 };

  nsIntRect visibleRect = mVisibleRegion.GetBounds();
  float opacity = GetEffectiveOpacity();
  bool useIntermediate = UseIntermediateSurface();

  nsRefPtr<ID3D10RenderTargetView> previousRTView;
  nsRefPtr<ID3D10Texture2D> renderTexture;
  nsRefPtr<ID3D10RenderTargetView> rtView;
  float previousRenderTargetOffset[2];
  nsIntSize previousViewportSize;

  gfx3DMatrix oldViewMatrix;

  if (useIntermediate) {
    device()->OMGetRenderTargets(1, getter_AddRefs(previousRTView), nullptr);
 
    D3D10_TEXTURE2D_DESC desc;
    memset(&desc, 0, sizeof(D3D10_TEXTURE2D_DESC));
    desc.ArraySize = 1;
    desc.MipLevels = 1;
    desc.Width = visibleRect.width;
    desc.Height = visibleRect.height;
    desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
    desc.SampleDesc.Count = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    HRESULT hr;
    hr = device()->CreateTexture2D(&desc, nullptr, getter_AddRefs(renderTexture));

    if (FAILED(hr)) {
      LayerManagerD3D10::ReportFailure(NS_LITERAL_CSTRING("Failed to create new texture for ContainerLayerD3D10!"), 
                                       hr);
      return;
    }

    hr = device()->CreateRenderTargetView(renderTexture, nullptr, getter_AddRefs(rtView));
    NS_ASSERTION(SUCCEEDED(hr), "Failed to create render target view for ContainerLayerD3D10!");

    effect()->GetVariableByName("vRenderTargetOffset")->
      GetRawValue(previousRenderTargetOffset, 0, 8);

    previousViewportSize = mD3DManager->GetViewport();

    if (mVisibleRegion.GetNumRects() != 1 || !(GetContentFlags() & CONTENT_OPAQUE)) {
      Matrix4x4 transform3D = GetEffectiveTransform();
      Matrix transform;
      // If we have an opaque ancestor layer, then we can be sure that
      // all the pixels we draw into are either opaque already or will be
      // covered by something opaque. Otherwise copying up the background is
      // not safe.
      if (mSupportsComponentAlphaChildren) {
        bool is2d = transform3D.Is2D(&transform);
        NS_ASSERTION(is2d, "Transform should be 2d when mSupportsComponentAlphaChildren.");

        // Copy background up from below. This applies any 2D transform that is
        // applied to use relative to our parent, and compensates for the offset
        // that was applied on our parent's rendering.
        D3D10_BOX srcBox;
        srcBox.left = std::max<int32_t>(visibleRect.x + int32_t(transform._31) - int32_t(previousRenderTargetOffset[0]), 0);
        srcBox.top = std::max<int32_t>(visibleRect.y + int32_t(transform._32) - int32_t(previousRenderTargetOffset[1]), 0);
        srcBox.right = std::min<int32_t>(srcBox.left + visibleRect.width, previousViewportSize.width);
        srcBox.bottom = std::min<int32_t>(srcBox.top + visibleRect.height, previousViewportSize.height);
        srcBox.back = 1;
        srcBox.front = 0;

        nsRefPtr<ID3D10Resource> srcResource;
        previousRTView->GetResource(getter_AddRefs(srcResource));

        device()->CopySubresourceRegion(renderTexture, 0,
                                        0, 0, 0,
                                        srcResource, 0,
                                        &srcBox);
      } else {
        float black[] = { 0, 0, 0, 0};
        device()->ClearRenderTargetView(rtView, black);
      }
    }

    ID3D10RenderTargetView *rtViewPtr = rtView;
    device()->OMSetRenderTargets(1, &rtViewPtr, nullptr);

    renderTargetOffset[0] = (float)visibleRect.x;
    renderTargetOffset[1] = (float)visibleRect.y;
    effect()->GetVariableByName("vRenderTargetOffset")->
      SetRawValue(renderTargetOffset, 0, 8);

    mD3DManager->SetViewport(nsIntSize(visibleRect.Size()));
  }
    
  D3D10_RECT oldD3D10Scissor;
  UINT numRects = 1;
  device()->RSGetScissorRects(&numRects, &oldD3D10Scissor);
  // Convert scissor to an nsIntRect. D3D10_RECT's are exclusive
  // on the bottom and right values.
  nsIntRect oldScissor(oldD3D10Scissor.left,
                       oldD3D10Scissor.top,
                       oldD3D10Scissor.right - oldD3D10Scissor.left,
                       oldD3D10Scissor.bottom - oldD3D10Scissor.top);

  nsAutoTArray<Layer*, 12> children;
  SortChildrenBy3DZOrder(children);

  /*
   * Render this container's contents.
   */
  for (uint32_t i = 0; i < children.Length(); i++) {
    LayerD3D10* layerToRender = static_cast<LayerD3D10*>(children.ElementAt(i)->ImplData());

    if (layerToRender->GetLayer()->GetEffectiveVisibleRegion().IsEmpty()) {
      continue;
    }

    nsIntRect scissorRect =
        RenderTargetPixel::ToUntyped(layerToRender->GetLayer()->CalculateScissorRect(RenderTargetPixel::FromUntyped(oldScissor)));
    if (scissorRect.IsEmpty()) {
      continue;
    }

    D3D10_RECT d3drect;
    d3drect.left = scissorRect.x;
    d3drect.top = scissorRect.y;
    d3drect.right = scissorRect.x + scissorRect.width;
    d3drect.bottom = scissorRect.y + scissorRect.height;
    device()->RSSetScissorRects(1, &d3drect);

    layerToRender->RenderLayer();
  }

  device()->RSSetScissorRects(1, &oldD3D10Scissor);

  if (useIntermediate) {
    mD3DManager->SetViewport(previousViewportSize);
    ID3D10RenderTargetView *rtView = previousRTView;
    device()->OMSetRenderTargets(1, &rtView, nullptr);
    effect()->GetVariableByName("vRenderTargetOffset")->
      SetRawValue(previousRenderTargetOffset, 0, 8);

    SetEffectTransformAndOpacity();

    ID3D10EffectTechnique *technique;
    if (LoadMaskTexture()) {
      if (GetTransform().CanDraw2D()) {
        technique = SelectShader(SHADER_RGBA | SHADER_PREMUL | SHADER_MASK);
      } else {
        technique = SelectShader(SHADER_RGBA | SHADER_PREMUL | SHADER_MASK_3D);
      }
    } else {
        technique = SelectShader(SHADER_RGBA | SHADER_PREMUL | SHADER_NO_MASK);
    }

    effect()->GetVariableByName("vLayerQuad")->AsVector()->SetFloatVector(
      ShaderConstantRectD3D10(
        (float)visibleRect.x,
        (float)visibleRect.y,
        (float)visibleRect.width,
        (float)visibleRect.height)
      );

    technique->GetPassByIndex(0)->Apply(0);

    ID3D10ShaderResourceView *view;
    device()->CreateShaderResourceView(renderTexture, nullptr, &view);
    device()->PSSetShaderResources(0, 1, &view);    
    device()->Draw(4, 0);
    view->Release();
  }
}

void
ContainerLayerD3D10::LayerManagerDestroyed()
{
  while (mFirstChild) {
    GetFirstChildD3D10()->LayerManagerDestroyed();
    RemoveChild(mFirstChild);
  }
}

void
ContainerLayerD3D10::Validate()
{
  nsIntRect visibleRect = mVisibleRegion.GetBounds();

  mSupportsComponentAlphaChildren = false;

  if (UseIntermediateSurface()) {
    Matrix4x4 transform3D = GetEffectiveTransform();
    Matrix transform;

    if (mVisibleRegion.GetNumRects() == 1 && (GetContentFlags() & CONTENT_OPAQUE)) {
      // don't need a background, we're going to paint all opaque stuff
      mSupportsComponentAlphaChildren = true;
    } else {
      if (HasOpaqueAncestorLayer(this) &&
          transform3D.Is2D(&transform) && !ThebesMatrix(transform).HasNonIntegerTranslation() &&
          GetParent()->GetEffectiveVisibleRegion().GetBounds().Contains(visibleRect))
      {
        // In this case we can copy up the background. See RenderLayer.
        mSupportsComponentAlphaChildren = true;
      }
    }
  } else {
    mSupportsComponentAlphaChildren = (GetContentFlags() & CONTENT_OPAQUE) ||
        (mParent && mParent->SupportsComponentAlphaChildren());
  }

  ReadbackProcessor readback;
  readback.BuildUpdates(this);

  Layer *layer = GetFirstChild();
  while (layer) {
    if (layer->GetType() == TYPE_PAINTED) {
      static_cast<PaintedLayerD3D10*>(layer)->Validate(&readback);
    } else {
      static_cast<LayerD3D10*>(layer->ImplData())->Validate();
    }
    layer = layer->GetNextSibling();
  }
}

} /* layers */
} /* mozilla */
