/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContainerLayerD3D9.h"
#include "gfxUtils.h"
#include "nsRect.h"
#include "ThebesLayerD3D9.h"
#include "ReadbackProcessor.h"

namespace mozilla {
namespace layers {

template<class Container>
static void
ContainerInsertAfter(Container* aContainer, Layer* aChild, Layer* aAfter)
{
  aChild->SetParent(aContainer);
  if (!aAfter) {
    Layer *oldFirstChild = aContainer->GetFirstChild();
    aContainer->mFirstChild = aChild;
    aChild->SetNextSibling(oldFirstChild);
    aChild->SetPrevSibling(nsnull);
    if (oldFirstChild) {
      oldFirstChild->SetPrevSibling(aChild);
    } else {
      aContainer->mLastChild = aChild;
    }
    NS_ADDREF(aChild);
    aContainer->DidInsertChild(aChild);
    return;
  }
  for (Layer *child = aContainer->GetFirstChild(); 
       child; child = child->GetNextSibling()) {
    if (aAfter == child) {
      Layer *oldNextSibling = child->GetNextSibling();
      child->SetNextSibling(aChild);
      aChild->SetNextSibling(oldNextSibling);
      if (oldNextSibling) {
        oldNextSibling->SetPrevSibling(aChild);
      } else {
        aContainer->mLastChild = aChild;
      }
      aChild->SetPrevSibling(child);
      NS_ADDREF(aChild);
      aContainer->DidInsertChild(aChild);
      return;
    }
  }
  NS_WARNING("Failed to find aAfter layer!");
}

template<class Container>
static void
ContainerRemoveChild(Container* aContainer, Layer* aChild)
{
  if (aContainer->GetFirstChild() == aChild) {
    aContainer->mFirstChild = aContainer->GetFirstChild()->GetNextSibling();
    if (aContainer->mFirstChild) {
      aContainer->mFirstChild->SetPrevSibling(nsnull);
    } else {
      aContainer->mLastChild = nsnull;
    }
    aChild->SetNextSibling(nsnull);
    aChild->SetPrevSibling(nsnull);
    aChild->SetParent(nsnull);
    aContainer->DidRemoveChild(aChild);
    NS_RELEASE(aChild);
    return;
  }
  Layer *lastChild = nsnull;
  for (Layer *child = aContainer->GetFirstChild(); child; 
       child = child->GetNextSibling()) {
    if (child == aChild) {
      // We're sure this is not our first child. So lastChild != NULL.
      lastChild->SetNextSibling(child->GetNextSibling());
      if (child->GetNextSibling()) {
        child->GetNextSibling()->SetPrevSibling(lastChild);
      } else {
        aContainer->mLastChild = lastChild;
      }
      child->SetNextSibling(nsnull);
      child->SetPrevSibling(nsnull);
      child->SetParent(nsnull);
      aContainer->DidRemoveChild(aChild);
      NS_RELEASE(aChild);
      return;
    }
    lastChild = child;
  }
}

static inline LayerD3D9*
GetNextSibling(LayerD3D9* aLayer)
{
   Layer* layer = aLayer->GetLayer()->GetNextSibling();
   return layer ? static_cast<LayerD3D9*>(layer->
                                         ImplData())
                 : nsnull;
}

static bool
HasOpaqueAncestorLayer(Layer* aLayer)
{
  for (Layer* l = aLayer->GetParent(); l; l = l->GetParent()) {
    if (l->GetContentFlags() & Layer::CONTENT_OPAQUE)
      return true;
  }
  return false;
}

static inline LayerD3D9*
GetNextSiblingD3D9(LayerD3D9* aLayer)
{
   Layer* layer = aLayer->GetLayer()->GetNextSibling();
   return layer ? static_cast<LayerD3D9*>(layer->
                                          ImplData())
                 : nsnull;
}

template<class Container>
static void
ContainerRender(Container* aContainer,
                LayerManagerD3D9* aManager)
{
  nsRefPtr<IDirect3DSurface9> previousRenderTarget;
  nsRefPtr<IDirect3DTexture9> renderTexture;
  float previousRenderTargetOffset[4];
  float renderTargetOffset[] = { 0, 0, 0, 0 };
  float oldViewMatrix[4][4];

  RECT containerD3D9ClipRect; 
  aManager->device()->GetScissorRect(&containerD3D9ClipRect);
  // Convert scissor to an nsIntRect. RECT's are exclusive on the bottom and
  // right values.
  nsIntRect oldScissor(containerD3D9ClipRect.left, 
                       containerD3D9ClipRect.top,
                       containerD3D9ClipRect.right - containerD3D9ClipRect.left,
                       containerD3D9ClipRect.bottom - containerD3D9ClipRect.top);

  ReadbackProcessor readback;
  readback.BuildUpdates(aContainer);

  nsIntRect visibleRect = aContainer->GetEffectiveVisibleRegion().GetBounds();
  bool useIntermediate = aContainer->UseIntermediateSurface();

  aContainer->mSupportsComponentAlphaChildren = false;
  if (useIntermediate) {
    aManager->device()->GetRenderTarget(0, getter_AddRefs(previousRenderTarget));
    HRESULT hr = aManager->device()->CreateTexture(visibleRect.width, visibleRect.height, 1,
                                                   D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                                   D3DPOOL_DEFAULT, getter_AddRefs(renderTexture),
                                                   NULL);
    if (FAILED(hr)) {
      aManager->ReportFailure(NS_LITERAL_CSTRING("ContainerLayerD3D9::ContainerRender(): Failed to create texture"),
                              hr);
      return;
    }

    nsRefPtr<IDirect3DSurface9> renderSurface;
    renderTexture->GetSurfaceLevel(0, getter_AddRefs(renderSurface));
    aManager->device()->SetRenderTarget(0, renderSurface);

    if (aContainer->mVisibleRegion.GetNumRects() == 1 && 
        (aContainer->GetContentFlags() & aContainer->CONTENT_OPAQUE)) {
      // don't need a background, we're going to paint all opaque stuff
      aContainer->mSupportsComponentAlphaChildren = true;
    } else {
      const gfx3DMatrix& transform3D = aContainer->GetEffectiveTransform();
      gfxMatrix transform;
      // If we have an opaque ancestor layer, then we can be sure that
      // all the pixels we draw into are either opaque already or will be
      // covered by something opaque. Otherwise copying up the background is
      // not safe.
      HRESULT hr = E_FAIL;
      if (HasOpaqueAncestorLayer(aContainer) &&
          transform3D.Is2D(&transform) && !transform.HasNonIntegerTranslation()) {
        // Copy background up from below
        RECT dest = { 0, 0, visibleRect.width, visibleRect.height };
        RECT src = dest;
        ::OffsetRect(&src,
                     visibleRect.x + PRInt32(transform.x0),
                     visibleRect.y + PRInt32(transform.y0));
        hr = aManager->device()->
          StretchRect(previousRenderTarget, &src, renderSurface, &dest, D3DTEXF_NONE);
      }
      if (hr == S_OK) {
        aContainer->mSupportsComponentAlphaChildren = true;
      } else {
        aManager->device()->
          Clear(0, 0, D3DCLEAR_TARGET, D3DCOLOR_RGBA(0, 0, 0, 0), 0, 0);
      }
    }

    aManager->device()->
      GetVertexShaderConstantF(CBvRenderTargetOffset, previousRenderTargetOffset, 1);
    renderTargetOffset[0] = (float)visibleRect.x;
    renderTargetOffset[1] = (float)visibleRect.y;
    aManager->device()->
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

    aManager->device()->
      GetVertexShaderConstantF(CBmProjection, &oldViewMatrix[0][0], 4);
    aManager->device()->
      SetVertexShaderConstantF(CBmProjection, &viewMatrix._11, 4);
  } else {
    aContainer->mSupportsComponentAlphaChildren = 
        (aContainer->GetContentFlags() & aContainer->CONTENT_OPAQUE) ||
        (aContainer->mParent && 
         aContainer->mParent->SupportsComponentAlphaChildren());
  }

  nsAutoTArray<Layer*, 12> children;
  aContainer->SortChildrenBy3DZOrder(children);

  /*
   * Render this container's contents.
   */
  for (PRUint32 i = 0; i < children.Length(); i++) {
    LayerD3D9* layerToRender = static_cast<LayerD3D9*>(children.ElementAt(i)->ImplData());

    if (layerToRender->GetLayer()->GetEffectiveVisibleRegion().IsEmpty()) {
      continue;
    }
    
    nsIntRect scissorRect =
      layerToRender->GetLayer()->CalculateScissorRect(oldScissor, nsnull);
    if (scissorRect.IsEmpty()) {
      continue;
    }

    RECT d3drect;
    d3drect.left = scissorRect.x;
    d3drect.top = scissorRect.y;
    d3drect.right = scissorRect.x + scissorRect.width;
    d3drect.bottom = scissorRect.y + scissorRect.height;
    aManager->device()->SetScissorRect(&d3drect);

    if (layerToRender->GetLayer()->GetType() == aContainer->TYPE_THEBES) {
      static_cast<ThebesLayerD3D9*>(layerToRender)->RenderThebesLayer(&readback);
    } else {
      layerToRender->RenderLayer();
    }
  }
    
  aManager->device()->SetScissorRect(&containerD3D9ClipRect);

  if (useIntermediate) {
    aManager->device()->SetRenderTarget(0, previousRenderTarget);
    aManager->device()->SetVertexShaderConstantF(CBvRenderTargetOffset, previousRenderTargetOffset, 1);
    aManager->device()->SetVertexShaderConstantF(CBmProjection, &oldViewMatrix[0][0], 4);

    aManager->device()->SetVertexShaderConstantF(CBvLayerQuad,
                                       ShaderConstantRect(visibleRect.x,
                                                          visibleRect.y,
                                                          visibleRect.width,
                                                          visibleRect.height),
                                       1);

    aContainer->SetShaderTransformAndOpacity();

    aManager->SetShaderMode(DeviceManagerD3D9::RGBALAYER,
                            aContainer->GetMaskLayer(),
                            aContainer->GetTransform().CanDraw2D());

    aManager->device()->SetTexture(0, renderTexture);
    aManager->device()->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
  }
}


ContainerLayerD3D9::ContainerLayerD3D9(LayerManagerD3D9 *aManager)
  : ContainerLayer(aManager, NULL)
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

void
ContainerLayerD3D9::InsertAfter(Layer* aChild, Layer* aAfter)
{
  ContainerInsertAfter(this, aChild, aAfter);
}

void
ContainerLayerD3D9::RemoveChild(Layer *aChild)
{
  ContainerRemoveChild(this, aChild);
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
    return nsnull;
  }
  return static_cast<LayerD3D9*>(mFirstChild->ImplData());
}

void
ContainerLayerD3D9::RenderLayer()
{
  ContainerRender(this, mD3DManager);
}

void
ContainerLayerD3D9::LayerManagerDestroyed()
{
  while (mFirstChild) {
    GetFirstChildD3D9()->LayerManagerDestroyed();
    RemoveChild(mFirstChild);
  }
}

ShadowContainerLayerD3D9::ShadowContainerLayerD3D9(LayerManagerD3D9 *aManager)
  : ShadowContainerLayer(aManager, NULL)
  , LayerD3D9(aManager)
{
  mImplData = static_cast<LayerD3D9*>(this);
}
 
ShadowContainerLayerD3D9::~ShadowContainerLayerD3D9()
{
  Destroy();
}

void
ShadowContainerLayerD3D9::InsertAfter(Layer* aChild, Layer* aAfter)
{
  ContainerInsertAfter(this, aChild, aAfter);
}

void
ShadowContainerLayerD3D9::RemoveChild(Layer *aChild)
{
  ContainerRemoveChild(this, aChild);
}

void
ShadowContainerLayerD3D9::Destroy()
{
  while (mFirstChild) {
    RemoveChild(mFirstChild);
  }
}

LayerD3D9*
ShadowContainerLayerD3D9::GetFirstChildD3D9()
{
  if (!mFirstChild) {
    return nsnull;
   }
  return static_cast<LayerD3D9*>(mFirstChild->ImplData());
}
 
void
ShadowContainerLayerD3D9::RenderLayer()
{
  ContainerRender(this, mD3DManager);
}

} /* layers */
} /* mozilla */
