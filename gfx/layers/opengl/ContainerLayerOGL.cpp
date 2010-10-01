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
  Destroy();
}

void
ContainerLayerOGL::Destroy()
{
  if (!mDestroyed) {
    while (mFirstChild) {
      GetFirstChildOGL()->Destroy();
      RemoveChild(mFirstChild);
    }
    mDestroyed = PR_TRUE;
  }
}

void
ContainerLayerOGL::InsertAfter(Layer* aChild, Layer* aAfter)
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
ContainerLayerOGL::RemoveChild(Layer *aChild)
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
  nsIntRect visibleRect = mVisibleRegion.GetBounds();

  gl()->PushScissorRect();

  float opacity = GetOpacity();
  bool needsFramebuffer = (opacity != 1.0) || !mTransform.IsIdentity();
  if (needsFramebuffer) {
    mOGLManager->CreateFBOWithTexture(visibleRect.width,
                                      visibleRect.height,
                                      &frameBuffer,
                                      &containerSurface);
    childOffset.x = visibleRect.x;
    childOffset.y = visibleRect.y;

    gl()->PushViewportRect();
    mOGLManager->SetupPipeline(visibleRect.width, visibleRect.height);

    gl()->fScissor(0, 0, visibleRect.width, visibleRect.height);
    gl()->fClearColor(0.0, 0.0, 0.0, 0.0);
    gl()->fClear(LOCAL_GL_COLOR_BUFFER_BIT);
  } else {
    frameBuffer = aPreviousFrameBuffer;
  }

  /**
   * Render this container's contents.
   */
  LayerOGL *layerToRender = GetFirstChildOGL();
  while (layerToRender) {
    nsIntRect scissorRect(visibleRect);

    const nsIntRect *clipRect = layerToRender->GetLayer()->GetClipRect();
    if (clipRect) {
      scissorRect = *clipRect;
    }

    if (needsFramebuffer) {
      scissorRect.MoveBy(- visibleRect.TopLeft());
    }

    if (aPreviousFrameBuffer == 0) {
      gl()->FixWindowCoordinateRect(scissorRect, mOGLManager->GetWigetSize().height);
    }

    gl()->fScissor(scissorRect.x, scissorRect.y, scissorRect.width, scissorRect.height);

    layerToRender->RenderLayer(frameBuffer, childOffset);

    Layer *nextSibling = layerToRender->GetLayer()->GetNextSibling();
    layerToRender = nextSibling ? static_cast<LayerOGL*>(nextSibling->
                                                         ImplData())
                                : nsnull;
  }

  gl()->PopScissorRect();

  if (needsFramebuffer) {
    // Unbind the current framebuffer and rebind the previous one.
    
    // Restore the viewport
    gl()->PopViewportRect();
    nsIntRect viewport = gl()->ViewportRect();
    mOGLManager->SetupPipeline(viewport.width, viewport.height);

    gl()->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, aPreviousFrameBuffer);
    gl()->fDeleteFramebuffers(1, &frameBuffer);

    gl()->fActiveTexture(LOCAL_GL_TEXTURE0);

    gl()->fBindTexture(mOGLManager->FBOTextureTarget(), containerSurface);

    ColorTextureLayerProgram *rgb = mOGLManager->GetFBOLayerProgram();

    rgb->Activate();
    rgb->SetLayerQuadRect(visibleRect);
    rgb->SetLayerTransform(mTransform);
    rgb->SetLayerOpacity(opacity);
    rgb->SetRenderOffset(aOffset);
    rgb->SetTextureUnit(0);

    if (rgb->GetTexCoordMultiplierUniformLocation() != -1) {
      // 2DRect case, get the multiplier right for a sampler2DRect
      float f[] = { float(visibleRect.width), float(visibleRect.height) };
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
