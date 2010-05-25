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
 *   Bas Schouten <bschouten@mozilla.org>
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

#include "ContainerLayerOGL.h"

namespace mozilla {
namespace layers {

ContainerLayerOGL::ContainerLayerOGL(LayerManagerOGL *aManager)
  : ContainerLayer(aManager, NULL)
  , LayerOGL(aManager)
{
  mImplData = static_cast<LayerOGL*>(this);
}

ContainerLayerOGL::~ContainerLayerOGL()
{
  LayerOGL *nextChild;
  for (LayerOGL *child = GetFirstChildOGL(); child; child = nextChild) {
    nextChild = child->GetNextSibling();
    child->GetLayer()->Release();
  }
}

const nsIntRect&
ContainerLayerOGL::GetVisibleRect()
{
  return mVisibleRect;
}

void
ContainerLayerOGL::SetVisibleRegion(const nsIntRegion &aRegion)
{
  mVisibleRect = aRegion.GetBounds();
}

void
ContainerLayerOGL::InsertAfter(Layer* aChild, Layer* aAfter)
{
  LayerOGL *newChild = static_cast<LayerOGL*>(aChild->ImplData());
  aChild->SetParent(this);
  if (!aAfter) {
    NS_ADDREF(aChild);
    LayerOGL *oldFirstChild = GetFirstChildOGL();
    mFirstChild = newChild->GetLayer();
    newChild->SetNextSibling(oldFirstChild);
    return;
  }
  for (LayerOGL *child = GetFirstChildOGL(); 
    child; child = child->GetNextSibling()) {
    if (aAfter == child->GetLayer()) {
      NS_ADDREF(aChild);
      LayerOGL *oldNextSibling = child->GetNextSibling();
      child->SetNextSibling(newChild);
      child->GetNextSibling()->SetNextSibling(oldNextSibling);
      return;
    }
  }
  NS_WARNING("Failed to find aAfter layer!");
}

void
ContainerLayerOGL::RemoveChild(Layer *aChild)
{
  if (GetFirstChild() == aChild) {
    mFirstChild = GetFirstChildOGL()->GetNextSibling()->GetLayer();
    NS_RELEASE(aChild);
    return;
  }
  LayerOGL *lastChild = NULL;
  for (LayerOGL *child = GetFirstChildOGL(); child; 
    child = child->GetNextSibling()) {
    if (child->GetLayer() == aChild) {
      // We're sure this is not our first child. So lastChild != NULL.
      lastChild->SetNextSibling(child->GetNextSibling());
      child->SetNextSibling(NULL);
      child->GetLayer()->SetParent(NULL);
      NS_RELEASE(aChild);
      return;
    }
    lastChild = child;
  }
}

LayerOGL::LayerType
ContainerLayerOGL::GetType()
{
  return TYPE_CONTAINER;
}

Layer*
ContainerLayerOGL::GetLayer()
{
  return this;
}

LayerOGL*
ContainerLayerOGL::GetFirstChildOGL()
{
  if (!mFirstChild) {
    return nsnull;
  }
  return static_cast<LayerOGL*>(mFirstChild->ImplData());
}

void
ContainerLayerOGL::RenderLayer(int aPreviousFrameBuffer,
                               const nsIntPoint& aOffset)
{
  /**
   * Setup our temporary texture for rendering the contents of this container.
   */
  GLuint containerSurface;
  GLuint frameBuffer;

  nsIntPoint childOffset(aOffset);
  bool needsFramebuffer = false;

  float opacity = GetOpacity();
  if (opacity != 1.0) {
    mOGLManager->CreateFBOWithTexture(mVisibleRect.width,
                                      mVisibleRect.height,
                                      &frameBuffer,
                                      &containerSurface);
    childOffset.x = mVisibleRect.x;
    childOffset.y = mVisibleRect.y;
  } else {
    frameBuffer = aPreviousFrameBuffer;
  }

  /**
   * Render this container's contents.
   */
  LayerOGL *layerToRender = GetFirstChildOGL();
  while (layerToRender) {
    const nsIntRect *clipRect = layerToRender->GetLayer()->GetClipRect();
    if (clipRect) {
      gl()->fScissor(clipRect->x - mVisibleRect.x,
                     clipRect->y - mVisibleRect.y,
                     clipRect->width,
                     clipRect->height);
    } else {
      gl()->fScissor(0, 0, mVisibleRect.width, mVisibleRect.height);
    }

    layerToRender->RenderLayer(frameBuffer, childOffset);

    layerToRender = layerToRender->GetNextSibling();
  }

  if (opacity != 1.0) {
    // Unbind the current framebuffer and rebind the previous one.
    gl()->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, aPreviousFrameBuffer);
    gl()->fDeleteFramebuffers(1, &frameBuffer);

    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);

    gl()->fBindTexture(mOGLManager->FBOTextureTarget(), containerSurface);

    ColorTextureLayerProgram *rgb = mOGLManager->GetFBOLayerProgram();

    rgb->Activate();
    rgb->SetLayerQuadRect(mVisibleRect);
    rgb->SetLayerTransform(mTransform);
    rgb->SetLayerOpacity(opacity);
    rgb->SetRenderOffset(aOffset);
    rgb->SetTextureUnit(0);

    if (rgb->GetTexCoordMultiplierUniformLocation() != -1) {
      // 2DRect case, get the multiplier right for a sampler2DRect
      float f[] = { float(mVisibleRect.width), float(mVisibleRect.height) };
      rgb->SetUniform(rgb->GetTexCoordMultiplierUniformLocation(),
                      2, f);
    }

    DEBUG_GL_ERROR_CHECK(gl());

    mOGLManager->BindAndDrawQuad(rgb);

    DEBUG_GL_ERROR_CHECK(gl());

    // Clean up resources.  This also unbinds the texture.
    gl()->fDeleteTextures(1, &containerSurface);

    DEBUG_GL_ERROR_CHECK(gl());
  }
}

} /* layers */
} /* mozilla */
