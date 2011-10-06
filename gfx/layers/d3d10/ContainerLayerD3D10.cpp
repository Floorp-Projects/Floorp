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

#include "ContainerLayerD3D10.h"
#include "nsAlgorithm.h"
#include "gfxUtils.h"
#include "nsRect.h"

#include "../d3d9/Nv3DVUtils.h"
#include "ThebesLayerD3D10.h"
#include "ReadbackProcessor.h"

namespace mozilla {
namespace layers {

ContainerLayerD3D10::ContainerLayerD3D10(LayerManagerD3D10 *aManager)
  : ContainerLayer(aManager, NULL)
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

void
ContainerLayerD3D10::InsertAfter(Layer* aChild, Layer* aAfter)
{
  aChild->SetParent(this);
  if (!aAfter) {
    Layer *oldFirstChild = GetFirstChild();
    mFirstChild = aChild;
    aChild->SetNextSibling(oldFirstChild);
    aChild->SetPrevSibling(nsnull);
    if (oldFirstChild) {
      oldFirstChild->SetPrevSibling(aChild);
    } else {
      mLastChild = aChild;
    }
    NS_ADDREF(aChild);
    DidInsertChild(aChild);
    return;
  }
  for (Layer *child = GetFirstChild();
       child; child = child->GetNextSibling()) {
    if (aAfter == child) {
      Layer *oldNextSibling = child->GetNextSibling();
      child->SetNextSibling(aChild);
      aChild->SetNextSibling(oldNextSibling);
      if (oldNextSibling) {
        oldNextSibling->SetPrevSibling(aChild);
      } else {
        mLastChild = aChild;
      }
      aChild->SetPrevSibling(child);
      NS_ADDREF(aChild);
      DidInsertChild(aChild);
      return;
    }
  }
  NS_WARNING("Failed to find aAfter layer!");
}

void
ContainerLayerD3D10::RemoveChild(Layer *aChild)
{
  if (GetFirstChild() == aChild) {
    mFirstChild = GetFirstChild()->GetNextSibling();
    if (mFirstChild) {
      mFirstChild->SetPrevSibling(nsnull);
    } else {
      mLastChild = nsnull;
    }
    aChild->SetNextSibling(nsnull);
    aChild->SetPrevSibling(nsnull);
    aChild->SetParent(nsnull);
    DidRemoveChild(aChild);
    NS_RELEASE(aChild);
    return;
  }
  Layer *lastChild = nsnull;
  for (Layer *child = GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (child == aChild) {
      // We're sure this is not our first child. So lastChild != NULL.
      lastChild->SetNextSibling(child->GetNextSibling());
      if (child->GetNextSibling()) {
        child->GetNextSibling()->SetPrevSibling(lastChild);
      } else {
        mLastChild = lastChild;
      }
      child->SetNextSibling(nsnull);
      child->SetPrevSibling(nsnull);
      child->SetParent(nsnull);
      DidRemoveChild(aChild);
      NS_RELEASE(aChild);
      return;
    }
    lastChild = child;
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
    return nsnull;
  }
  return static_cast<LayerD3D10*>(mFirstChild->ImplData());
}

static inline LayerD3D10*
GetNextSiblingD3D10(LayerD3D10* aLayer)
{
   Layer* layer = aLayer->GetLayer()->GetNextSibling();
   return layer ? static_cast<LayerD3D10*>(layer->
                                           ImplData())
                : nsnull;
}

static PRBool
HasOpaqueAncestorLayer(Layer* aLayer)
{
  for (Layer* l = aLayer->GetParent(); l; l = l->GetParent()) {
    if (l->GetContentFlags() & Layer::CONTENT_OPAQUE)
      return PR_TRUE;
  }
  return PR_FALSE;
}

void
ContainerLayerD3D10::RenderLayer()
{
  float renderTargetOffset[] = { 0, 0 };

  nsIntRect visibleRect = mVisibleRegion.GetBounds();
  float opacity = GetEffectiveOpacity();
  PRBool useIntermediate = UseIntermediateSurface();

  nsRefPtr<ID3D10RenderTargetView> previousRTView;
  nsRefPtr<ID3D10Texture2D> renderTexture;
  nsRefPtr<ID3D10RenderTargetView> rtView;
  float previousRenderTargetOffset[2];
  nsIntSize previousViewportSize;

  gfx3DMatrix oldViewMatrix;

  if (useIntermediate) {
    device()->OMGetRenderTargets(1, getter_AddRefs(previousRTView), NULL);
 
    D3D10_TEXTURE2D_DESC desc;
    memset(&desc, 0, sizeof(D3D10_TEXTURE2D_DESC));
    desc.ArraySize = 1;
    desc.MipLevels = 1;
    desc.Width = visibleRect.width;
    desc.Height = visibleRect.height;
    desc.BindFlags = D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE;
    desc.SampleDesc.Count = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    device()->CreateTexture2D(&desc, NULL, getter_AddRefs(renderTexture));
    
    device()->CreateRenderTargetView(renderTexture, NULL, getter_AddRefs(rtView));

    effect()->GetVariableByName("vRenderTargetOffset")->
      GetRawValue(previousRenderTargetOffset, 0, 8);

    if (mVisibleRegion.GetNumRects() != 1 || !(GetContentFlags() & CONTENT_OPAQUE)) {
      const gfx3DMatrix& transform3D = GetEffectiveTransform();
      gfxMatrix transform;
      // If we have an opaque ancestor layer, then we can be sure that
      // all the pixels we draw into are either opaque already or will be
      // covered by something opaque. Otherwise copying up the background is
      // not safe.
      if (mSupportsComponentAlphaChildren) {
        PRBool is2d = transform3D.Is2D(&transform);
        NS_ASSERTION(is2d, "Transform should be 2d when mSupportsComponentAlphaChildren.");

        // Copy background up from below. This applies any 2D transform that is
        // applied to use relative to our parent, and compensates for the offset
        // that was applied on our parent's rendering.
        D3D10_BOX srcBox;
        srcBox.left = visibleRect.x + PRInt32(transform.x0) - PRInt32(previousRenderTargetOffset[0]);
        srcBox.top = visibleRect.y + PRInt32(transform.y0) - PRInt32(previousRenderTargetOffset[1]);
        srcBox.right = srcBox.left + visibleRect.width;
        srcBox.bottom = srcBox.top + visibleRect.height;
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
    device()->OMSetRenderTargets(1, &rtViewPtr, NULL);

    renderTargetOffset[0] = (float)visibleRect.x;
    renderTargetOffset[1] = (float)visibleRect.y;
    effect()->GetVariableByName("vRenderTargetOffset")->
      SetRawValue(renderTargetOffset, 0, 8);

    previousViewportSize = mD3DManager->GetViewport();
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

  /*
   * Render this container's contents.
   */
  for (LayerD3D10* layerToRender = GetFirstChildD3D10();
       layerToRender != nsnull;
       layerToRender = GetNextSiblingD3D10(layerToRender)) {

    if (layerToRender->GetLayer()->GetEffectiveVisibleRegion().IsEmpty()) {
      continue;
    }
    
    nsIntRect scissorRect =
        layerToRender->GetLayer()->CalculateScissorRect(oldScissor, nsnull);
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
    device()->OMSetRenderTargets(1, &rtView, NULL);
    effect()->GetVariableByName("vRenderTargetOffset")->
      SetRawValue(previousRenderTargetOffset, 0, 8);

    SetEffectTransformAndOpacity();

    ID3D10EffectTechnique *technique;
    technique = effect()->GetTechniqueByName("RenderRGBALayerPremul");

    effect()->GetVariableByName("vLayerQuad")->AsVector()->SetFloatVector(
      ShaderConstantRectD3D10(
        (float)visibleRect.x,
        (float)visibleRect.y,
        (float)visibleRect.width,
        (float)visibleRect.height)
      );

    technique->GetPassByIndex(0)->Apply(0);

    ID3D10ShaderResourceView *view;
    device()->CreateShaderResourceView(renderTexture, NULL, &view);
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

  mSupportsComponentAlphaChildren = PR_FALSE;

  if (UseIntermediateSurface()) {
    const gfx3DMatrix& transform3D = GetEffectiveTransform();
    gfxMatrix transform;

    if (mVisibleRegion.GetNumRects() == 1 && (GetContentFlags() & CONTENT_OPAQUE)) {
      // don't need a background, we're going to paint all opaque stuff
      mSupportsComponentAlphaChildren = PR_TRUE;
    } else {
      if (HasOpaqueAncestorLayer(this) &&
          transform3D.Is2D(&transform) && !transform.HasNonIntegerTranslation() &&
          GetParent()->GetEffectiveVisibleRegion().GetBounds().Contains(visibleRect))
      {
        // In this case we can copy up the background. See RenderLayer.
        mSupportsComponentAlphaChildren = PR_TRUE;
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
    if (layer->GetType() == TYPE_THEBES) {
      static_cast<ThebesLayerD3D10*>(layer)->Validate(&readback);
    } else {
      static_cast<LayerD3D10*>(layer->ImplData())->Validate();
    }
    layer = layer->GetNextSibling();
  }
}

ShadowContainerLayerD3D10::ShadowContainerLayerD3D10(LayerManagerD3D10 *aManager) 
  : ShadowContainerLayer(aManager, NULL)
  , LayerD3D10(aManager)
{
  mImplData = static_cast<LayerD3D10*>(this);
}

ShadowContainerLayerD3D10::~ShadowContainerLayerD3D10() {}

void
ShadowContainerLayerD3D10::InsertAfter(Layer* aChild, Layer* aAfter)
{
  mFirstChild = aChild;
}

void
ShadowContainerLayerD3D10::RemoveChild(Layer* aChild)
{

}

void
ShadowContainerLayerD3D10::ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface)
{
  DefaultComputeEffectiveTransforms(aTransformToSurface);
}

LayerD3D10*
ShadowContainerLayerD3D10::GetFirstChildD3D10()
{
  return static_cast<LayerD3D10*>(mFirstChild->ImplData());
}

void
ShadowContainerLayerD3D10::RenderLayer()
{
  LayerD3D10* layerToRender = GetFirstChildD3D10();
  layerToRender->RenderLayer();
}

void
ShadowContainerLayerD3D10::Validate()
{
}
 
void
ShadowContainerLayerD3D10::LayerManagerDestroyed()
{
}

} /* layers */
} /* mozilla */
