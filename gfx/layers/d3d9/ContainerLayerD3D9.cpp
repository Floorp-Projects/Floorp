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

const nsIntRect&
ContainerLayerD3D9::GetVisibleRect()
{
  return mVisibleRect;
}

void
ContainerLayerD3D9::SetVisibleRegion(const nsIntRegion &aRegion)
{
  mVisibleRect = aRegion.GetBounds();
}

void
ContainerLayerD3D9::InsertAfter(Layer* aChild, Layer* aAfter)
{
  LayerD3D9 *newChild = static_cast<LayerD3D9*>(aChild->ImplData());
  aChild->SetParent(this);
  if (!aAfter) {
    LayerD3D9 *oldFirstChild = GetFirstChildD3D9();
    mFirstChild = newChild->GetLayer();
    newChild->SetNextSibling(oldFirstChild);
    return;
  }
  for (LayerD3D9 *child = GetFirstChildD3D9(); 
    child; child = child->GetNextSibling()) {
    if (aAfter == child->GetLayer()) {
      LayerD3D9 *oldNextSibling = child->GetNextSibling();
      child->SetNextSibling(newChild);
      child->GetNextSibling()->SetNextSibling(oldNextSibling);
      return;
    }
  }
  NS_WARNING("Failed to find aAfter layer!");
}

void
ContainerLayerD3D9::RemoveChild(Layer *aChild)
{
  if (GetFirstChild() == aChild) {
    mFirstChild = GetFirstChildD3D9()->GetNextSibling() ?
      GetFirstChildD3D9()->GetNextSibling()->GetLayer() : nsnull;
    return;
  }
  LayerD3D9 *lastChild = NULL;
  for (LayerD3D9 *child = GetFirstChildD3D9(); child; 
    child = child->GetNextSibling()) {
    if (child->GetLayer() == aChild) {
      // We're sure this is not our first child. So lastChild != NULL.
      lastChild->SetNextSibling(child->GetNextSibling());
      child->SetNextSibling(NULL);
      child->GetLayer()->SetParent(NULL);
      return;
    }
    lastChild = child;
  }
}

LayerD3D9::LayerType
ContainerLayerD3D9::GetType()
{
  return TYPE_CONTAINER;
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
  float renderTargetOffset[] = { 0, 0, 0, 0 };
  float oldViewMatrix[4][4];

  PRBool useIntermediate = (opacity != 1.0 || !mTransform.IsIdentity());

  if (useIntermediate) {
    device()->GetRenderTarget(0, getter_AddRefs(previousRenderTarget));
    device()->CreateTexture(mVisibleRect.width, mVisibleRect.height, 1,
			    D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
			    D3DPOOL_DEFAULT, getter_AddRefs(renderTexture),
                            NULL);
    nsRefPtr<IDirect3DSurface9> renderSurface;
    renderTexture->GetSurfaceLevel(0, getter_AddRefs(renderSurface));
    device()->SetRenderTarget(0, renderSurface);
    device()->GetVertexShaderConstantF(12, previousRenderTargetOffset, 1);
    renderTargetOffset[0] = (float)GetVisibleRect().x;
    renderTargetOffset[1] = (float)GetVisibleRect().y;
    device()->SetVertexShaderConstantF(12, renderTargetOffset, 1);

    float viewMatrix[4][4];
    /*
     * Matrix to transform to viewport space ( <-1.0, 1.0> topleft, 
     * <1.0, -1.0> bottomright)
     */
    memset(&viewMatrix, 0, sizeof(viewMatrix));
    viewMatrix[0][0] = 2.0f / mVisibleRect.width;
    viewMatrix[1][1] = -2.0f / mVisibleRect.height;
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
        r.left = GetVisibleRect().x;
        r.top = GetVisibleRect().y;
      }
      r.right = r.left + GetVisibleRect().width;
      r.bottom = r.top + GetVisibleRect().height;
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
    layerToRender = layerToRender->GetNextSibling();
  }

  if (useIntermediate) {
    device()->SetRenderTarget(0, previousRenderTarget);
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
    quadTransform[0][0] = (float)GetVisibleRect().width;
    quadTransform[1][1] = (float)GetVisibleRect().height;
    quadTransform[2][2] = 1.0f;
    quadTransform[3][0] = (float)GetVisibleRect().x - 0.5f;
    quadTransform[3][1] = (float)GetVisibleRect().y - 0.5f;
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

    mD3DManager->SetShaderMode(LayerManagerD3D9::RGBLAYER);

    device()->SetTexture(0, renderTexture);
    device()->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
  }
}

} /* layers */
} /* mozilla */
