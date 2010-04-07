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

#include "glWrapper.h"

namespace mozilla {
namespace layers {

ContainerLayerOGL::ContainerLayerOGL(LayerManager *aManager)
  : ContainerLayer(aManager, NULL)
{
  mImplData = static_cast<LayerOGL*>(this);
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
    LayerOGL *oldFirstChild = GetFirstChildOGL();
    mFirstChild = newChild->GetLayer();
    newChild->SetNextSibling(oldFirstChild);
    return;
  }
  for (LayerOGL *child = GetFirstChildOGL(); 
    child; child = child->GetNextSibling()) {
    if (aAfter == child->GetLayer()) {
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
ContainerLayerOGL::RenderLayer(int aPreviousFrameBuffer)
{
  /**
   * Setup our temporary texture for rendering the contents of this container.
   */
  GLuint containerSurface;
  GLuint frameBuffer;
  RGBLayerProgram *rgbProgram =
    static_cast<LayerManagerOGL*>(mManager)->GetRGBLayerProgram();
  YCbCrLayerProgram *yCbCrProgram =
    static_cast<LayerManagerOGL*>(mManager)->GetYCbCrLayerProgram();

  if (GetOpacity() != 1.0) {
    sglWrapper.GenTextures(1, &containerSurface);
    sglWrapper.BindTexture(LOCAL_GL_TEXTURE_2D, containerSurface);
    sglWrapper.TexImage2D(LOCAL_GL_TEXTURE_2D,
			    0,
			    LOCAL_GL_RGBA,
			    mVisibleRect.width,
			    mVisibleRect.height,
			    0,
			    LOCAL_GL_BGRA,
			    LOCAL_GL_UNSIGNED_BYTE,
			    NULL);
    sglWrapper.TexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
    sglWrapper.TexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);

    /**
     * Create the framebuffer and bind it to make our content render into our
     * framebuffer.
     */
    sglWrapper.GenFramebuffersEXT(1, &frameBuffer);
    sglWrapper.BindFramebufferEXT(LOCAL_GL_FRAMEBUFFER_EXT, frameBuffer);
    sglWrapper.FramebufferTexture2DEXT(LOCAL_GL_FRAMEBUFFER_EXT,
					 LOCAL_GL_COLOR_ATTACHMENT0_EXT,
					 LOCAL_GL_TEXTURE_2D,
					 containerSurface,
					 0);

    NS_ASSERTION(
	  sglWrapper.CheckFramebufferStatusEXT(LOCAL_GL_FRAMEBUFFER_EXT) ==
	    LOCAL_GL_FRAMEBUFFER_COMPLETE, "Error setting up framebuffer.");

    /**
     * Store old shader program variables and set the ones used for rendering
     * this container's content.
     */
    
    rgbProgram->Activate();
    rgbProgram->PushRenderTargetOffset((GLfloat)GetVisibleRect().x,
					 (GLfloat)GetVisibleRect().y);
    yCbCrProgram->Activate();
    yCbCrProgram->PushRenderTargetOffset((GLfloat)GetVisibleRect().x,
					   (GLfloat)GetVisibleRect().y);
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
      sglWrapper.Scissor(clipRect->x - GetVisibleRect().x,
                clipRect->y - GetVisibleRect().y,
                clipRect->width,
                clipRect->height);
    } else {
      sglWrapper.Scissor(0, 0, GetVisibleRect().width, GetVisibleRect().height);
    }

    layerToRender->RenderLayer(frameBuffer);
    layerToRender = layerToRender->GetNextSibling();
  }

  if (GetOpacity() != 1.0) {
    // Unbind the current framebuffer and rebind the previous one.
    sglWrapper.BindFramebufferEXT(LOCAL_GL_FRAMEBUFFER_EXT, aPreviousFrameBuffer);
    sglWrapper.DeleteFramebuffersEXT(1, &frameBuffer);

    // Restore old shader program variables.
    yCbCrProgram->Activate();
    yCbCrProgram->PopRenderTargetOffset();

    rgbProgram->Activate();
    rgbProgram->PopRenderTargetOffset();

    /**
     * Render the contents of this container to our destination.
     */
    float quadTransform[4][4];
    /*
     * Matrix to transform the <0.0,0.0>, <1.0,1.0> quad to the correct position
     * and size.
     */
    memset(&quadTransform, 0, sizeof(quadTransform));
    quadTransform[0][0] = (float)GetVisibleRect().width;
    quadTransform[1][1] = (float)GetVisibleRect().height;
    quadTransform[2][2] = 1.0f;
    quadTransform[3][0] = (float)GetVisibleRect().x;
    quadTransform[3][1] = (float)GetVisibleRect().y;
    quadTransform[3][3] = 1.0f;

    rgbProgram->SetLayerQuadTransform(&quadTransform[0][0]);

    sglWrapper.BindTexture(LOCAL_GL_TEXTURE_2D, containerSurface);

    rgbProgram->SetLayerOpacity(GetOpacity());
    rgbProgram->SetLayerTransform(&mTransform._11);
    rgbProgram->Apply();

    sglWrapper.DrawArrays(LOCAL_GL_TRIANGLE_STRIP, 0, 4);

    // Clean up resources.
    sglWrapper.DeleteTextures(1, &containerSurface);
  }
}

} /* layers */
} /* mozilla */
