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

#include "ContainerLayerD3D9.h"

namespace mozilla {
namespace layers {

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
  aChild->SetParent(this);
  if (!aAfter) {
    Layer *oldFirstChild = GetFirstChild();
    mFirstChild = aChild;
    aChild->SetNextSibling(oldFirstChild);
    aChild->SetPrevSibling(nsnull);
    if (oldFirstChild) {
      oldFirstChild->SetPrevSibling(aChild);
    }
    NS_ADDREF(aChild);
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
      }
      aChild->SetPrevSibling(child);
      NS_ADDREF(aChild);
      return;
    }
  }
  NS_WARNING("Failed to find aAfter layer!");
}

void
ContainerLayerD3D9::RemoveChild(Layer *aChild)
{
  if (GetFirstChild() == aChild) {
    mFirstChild = GetFirstChild()->GetNextSibling();
    if (mFirstChild) {
      mFirstChild->SetPrevSibling(nsnull);
    }
    aChild->SetNextSibling(nsnull);
    aChild->SetPrevSibling(nsnull);
    aChild->SetParent(nsnull);
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
      }
      child->SetNextSibling(nsnull);
      child->SetPrevSibling(nsnull);
      child->SetParent(nsnull);
      NS_RELEASE(aChild);
      return;
    }
    lastChild = child;
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
    return nsnull;
  }
  return static_cast<LayerD3D9*>(mFirstChild->ImplData());
}

void
ContainerLayerD3D9::RenderLayer()
{
  float opacity = GetOpacity();
  nsRefPtr<IDirect3DSurface9> previousRenderTarget;
  nsRefPtr<IDirect3DTexture9> renderTexture;
  float previousRenderTargetOffset[4];
  RECT oldClipRect;
  float renderTargetOffset[] = { 0, 0, 0, 0 };
  float oldViewMatrix[4][4];

  nsIntRect visibleRect = mVisibleRegion.GetBounds();
  PRBool useIntermediate = (opacity != 1.0 || !mTransform.IsIdentity());

  if (useIntermediate) {
    device()->GetRenderTarget(0, getter_AddRefs(previousRenderTarget));
    device()->GetScissorRect(&oldClipRect);
    device()->CreateTexture(visibleRect.width, visibleRect.height, 1,
                            D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                            D3DPOOL_DEFAULT, getter_AddRefs(renderTexture),
                            NULL);
    nsRefPtr<IDirect3DSurface9> renderSurface;
    renderTexture->GetSurfaceLevel(0, getter_AddRefs(renderSurface));
    device()->SetRenderTarget(0, renderSurface);
    device()->GetVertexShaderConstantF(12, previousRenderTargetOffset, 1);
    renderTargetOffset[0] = (float)visibleRect.x;
    renderTargetOffset[1] = (float)visibleRect.y;
    device()->SetVertexShaderConstantF(12, renderTargetOffset, 1);

    float viewMatrix[4][4];
    /*
     * Matrix to transform to viewport space ( <-1.0, 1.0> topleft,
     * <1.0, -1.0> bottomright)
     */
    memset(&viewMatrix, 0, sizeof(viewMatrix));
    viewMatrix[0][0] = 2.0f / visibleRect.width;
    viewMatrix[1][1] = -2.0f / visibleRect.height;
    viewMatrix[2][2] = 1.0f;
    viewMatrix[3][0] = -1.0f;
    viewMatrix[3][1] = 1.0f;
    viewMatrix[3][3] = 1.0f;

    device()->GetVertexShaderConstantF(8, &oldViewMatrix[0][0], 4);
    device()->SetVertexShaderConstantF(8, &viewMatrix[0][0], 4);
  }

  /*
   * Render this container's contents.
   */
  LayerD3D9 *layerToRender = GetFirstChildD3D9();
  while (layerToRender) {
    const nsIntRect *clipRect = layerToRender->GetLayer()->GetClipRect();
    RECT r;
    if (clipRect) {
      r.left = (LONG)(clipRect->x - renderTargetOffset[0]);
      r.top = (LONG)(clipRect->y - renderTargetOffset[1]);
      r.right = (LONG)(clipRect->x - renderTargetOffset[0] + clipRect->width);
      r.bottom = (LONG)(clipRect->y - renderTargetOffset[1] + clipRect->height);
    } else {
      if (useIntermediate) {
        r.left = 0;
        r.top = 0;
      } else {
        r.left = visibleRect.x;
        r.top = visibleRect.y;
      }
      r.right = r.left + visibleRect.width;
      r.bottom = r.top + visibleRect.height;
    }

    nsRefPtr<IDirect3DSurface9> renderSurface;
    device()->GetRenderTarget(0, getter_AddRefs(renderSurface));

    D3DSURFACE_DESC desc;
    renderSurface->GetDesc(&desc);

    r.left = NS_MAX<LONG>(0, r.left);
    r.top = NS_MAX<LONG>(0, r.top);
    r.bottom = NS_MIN<LONG>(r.bottom, desc.Height);
    r.right = NS_MIN<LONG>(r.right, desc.Width);

    device()->SetScissorRect(&r);

    layerToRender->RenderLayer();
    Layer *nextSibling = layerToRender->GetLayer()->GetNextSibling();
    layerToRender = nextSibling ? static_cast<LayerD3D9*>(nextSibling->
                                                          ImplData())
                                : nsnull;
  }

  if (useIntermediate) {
    device()->SetRenderTarget(0, previousRenderTarget);
    device()->SetScissorRect(&oldClipRect);
    device()->SetVertexShaderConstantF(12, previousRenderTargetOffset, 1);
    device()->SetVertexShaderConstantF(8, &oldViewMatrix[0][0], 4);

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
    quadTransform[3][0] = (float)visibleRect.x;
    quadTransform[3][1] = (float)visibleRect.y;
    quadTransform[3][3] = 1.0f;

    device()->SetVertexShaderConstantF(0, &quadTransform[0][0], 4);
    device()->SetVertexShaderConstantF(4, &mTransform._11, 4);

    float opacityVector[4];
    /*
     * We always upload a 4 component float, but the shader will use only the
     * first component since it's declared as a 'float'.
     */
    opacityVector[0] = opacity;
    device()->SetPixelShaderConstantF(0, opacityVector, 1);

    mD3DManager->SetShaderMode(DeviceManagerD3D9::RGBALAYER);

    device()->SetTexture(0, renderTexture);
    device()->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
  }
}

} /* layers */
} /* mozilla */
