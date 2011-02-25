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
#include "gfxUtils.h"

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

template<class Container>
static void
ContainerDestroy(Container* aContainer)
 {
  if (!aContainer->mDestroyed) {
    while (aContainer->mFirstChild) {
      aContainer->GetFirstChildOGL()->Destroy();
      aContainer->RemoveChild(aContainer->mFirstChild);
    }
    aContainer->mDestroyed = PR_TRUE;
  }
}

static inline LayerOGL*
GetNextSibling(LayerOGL* aLayer)
{
   Layer* layer = aLayer->GetLayer()->GetNextSibling();
   return layer ? static_cast<LayerOGL*>(layer->
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

template<class Container>
static void
ContainerRender(Container* aContainer,
                int aPreviousFrameBuffer,
                const nsIntPoint& aOffset,
                LayerManagerOGL* aManager)
{
  /**
   * Setup our temporary texture for rendering the contents of this container.
   */
  GLuint containerSurface;
  GLuint frameBuffer;

  nsIntPoint childOffset(aOffset);
  nsIntRect visibleRect = aContainer->GetEffectiveVisibleRegion().GetBounds();

  nsIntRect cachedScissor = aContainer->gl()->ScissorRect();
  aContainer->gl()->PushScissorRect();
  aContainer->mSupportsComponentAlphaChildren = PR_FALSE;

  float opacity = aContainer->GetEffectiveOpacity();
  const gfx3DMatrix& transform = aContainer->GetEffectiveTransform();
  bool needsFramebuffer = aContainer->UseIntermediateSurface();
  gfxMatrix contTransform;
  if (needsFramebuffer) {
    LayerManagerOGL::InitMode mode = LayerManagerOGL::InitModeClear;
    nsIntRect framebufferRect = visibleRect;
    if (aContainer->GetEffectiveVisibleRegion().GetNumRects() == 1 && 
        (aContainer->GetContentFlags() & Layer::CONTENT_OPAQUE))
    {
      // don't need a background, we're going to paint all opaque stuff
      aContainer->mSupportsComponentAlphaChildren = PR_TRUE;
      mode = LayerManagerOGL::InitModeNone;
    } else {
      const gfx3DMatrix& transform3D = aContainer->GetEffectiveTransform();
      gfxMatrix transform;
      // If we have an opaque ancestor layer, then we can be sure that
      // all the pixels we draw into are either opaque already or will be
      // covered by something opaque. Otherwise copying up the background is
      // not safe.
      if (HasOpaqueAncestorLayer(aContainer) &&
          transform3D.Is2D(&transform) && !transform.HasNonIntegerTranslation()) {
        mode = LayerManagerOGL::InitModeCopy;
        framebufferRect.x += transform.x0;
        framebufferRect.y += transform.y0;
        aContainer->mSupportsComponentAlphaChildren = PR_TRUE;
      }
    }

    aContainer->gl()->PushViewportRect();
    framebufferRect -= childOffset; 
    aManager->CreateFBOWithTexture(framebufferRect,
                                   mode,
                                   &frameBuffer,
                                   &containerSurface);
    childOffset.x = visibleRect.x;
    childOffset.y = visibleRect.y;
  } else {
    frameBuffer = aPreviousFrameBuffer;
    aContainer->mSupportsComponentAlphaChildren = (aContainer->GetContentFlags() & Layer::CONTENT_OPAQUE) ||
      (aContainer->GetParent() && aContainer->GetParent()->SupportsComponentAlphaChildren());
#ifdef DEBUG
    PRBool is2d =
#endif
    transform.Is2D(&contTransform);
    NS_ASSERTION(is2d, "Transform must be 2D");
  }

  /**
   * Render this container's contents.
   */
  for (LayerOGL* layerToRender = aContainer->GetFirstChildOGL();
       layerToRender != nsnull;
       layerToRender = GetNextSibling(layerToRender)) {

    if (layerToRender->GetLayer()->GetEffectiveVisibleRegion().IsEmpty()) {
      continue;
    }

    nsIntRect scissorRect(visibleRect);

    const nsIntRect *clipRect = layerToRender->GetLayer()->GetEffectiveClipRect();
    if (clipRect) {
      if (clipRect->IsEmpty()) {
        continue;
      }
      scissorRect = *clipRect;
      if (!needsFramebuffer) {
        gfxRect r(scissorRect.x, scissorRect.y, scissorRect.width, scissorRect.height);
        gfxRect trScissor = contTransform.TransformBounds(r);
        trScissor.Round();
        if (!gfxUtils::GfxRectToIntRect(trScissor, &scissorRect)) {
          scissorRect = visibleRect;
        }
      }
    }

    if (needsFramebuffer) {
      scissorRect.MoveBy(- visibleRect.TopLeft());
    } else if (clipRect) {
      scissorRect.IntersectRect(scissorRect, cachedScissor);
    }

    /**
     *  We can't clip to a visible region if theres no framebuffer since we might be transformed
     */
    if (needsFramebuffer || clipRect) {
      aContainer->gl()->fScissor(scissorRect.x, 
                                 scissorRect.y, 
                                 scissorRect.width, 
                                 scissorRect.height);
    } else {
      aContainer->gl()->fScissor(cachedScissor.x, 
                                 cachedScissor.y, 
                                 cachedScissor.width, 
                                 cachedScissor.height);
    }

    layerToRender->RenderLayer(frameBuffer, childOffset);
    aContainer->gl()->MakeCurrent();
  }


  if (needsFramebuffer) {
    // Unbind the current framebuffer and rebind the previous one.
    
    // Restore the viewport
    aContainer->gl()->PopViewportRect();
    nsIntRect viewport = aContainer->gl()->ViewportRect();
    aManager->SetupPipeline(viewport.width, viewport.height,
                            LayerManagerOGL::ApplyWorldTransform);
    aContainer->gl()->PopScissorRect();

    aContainer->gl()->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, aPreviousFrameBuffer);
    aContainer->gl()->fDeleteFramebuffers(1, &frameBuffer);

    aContainer->gl()->fActiveTexture(LOCAL_GL_TEXTURE0);

    aContainer->gl()->fBindTexture(aManager->FBOTextureTarget(), containerSurface);

    ColorTextureLayerProgram *rgb = aManager->GetFBOLayerProgram();

    rgb->Activate();
    rgb->SetLayerQuadRect(visibleRect);
    rgb->SetLayerTransform(transform);
    rgb->SetLayerOpacity(opacity);
    rgb->SetRenderOffset(aOffset);
    rgb->SetTextureUnit(0);

    if (rgb->GetTexCoordMultiplierUniformLocation() != -1) {
      // 2DRect case, get the multiplier right for a sampler2DRect
      float f[] = { float(visibleRect.width), float(visibleRect.height) };
      rgb->SetUniform(rgb->GetTexCoordMultiplierUniformLocation(),
                      2, f);
    }

    // Drawing is always flipped, but when copying between surfaces we want to avoid
    // this. Pass true for the flip parameter to introduce a second flip
    // that cancels the other one out.
    aManager->BindAndDrawQuad(rgb, true);

    // Clean up resources.  This also unbinds the texture.
    aContainer->gl()->fDeleteTextures(1, &containerSurface);
  } else {
    aContainer->gl()->PopScissorRect();
  }
}

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
ContainerLayerOGL::InsertAfter(Layer* aChild, Layer* aAfter)
{
  ContainerInsertAfter(this, aChild, aAfter);
}

void
ContainerLayerOGL::RemoveChild(Layer *aChild)
{
  ContainerRemoveChild(this, aChild);
}

void
ContainerLayerOGL::Destroy()
{
  ContainerDestroy(this);
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
  ContainerRender(this, aPreviousFrameBuffer, aOffset, mOGLManager);
}


#ifdef MOZ_IPC

ShadowContainerLayerOGL::ShadowContainerLayerOGL(LayerManagerOGL *aManager)
  : ShadowContainerLayer(aManager, NULL)
  , LayerOGL(aManager)
{
  mImplData = static_cast<LayerOGL*>(this);
}
 
ShadowContainerLayerOGL::~ShadowContainerLayerOGL()
{
  Destroy();
}

void
ShadowContainerLayerOGL::InsertAfter(Layer* aChild, Layer* aAfter)
{
  ContainerInsertAfter(this, aChild, aAfter);
}

void
ShadowContainerLayerOGL::RemoveChild(Layer *aChild)
{
  ContainerRemoveChild(this, aChild);
}

void
ShadowContainerLayerOGL::Destroy()
{
  ContainerDestroy(this);
}

LayerOGL*
ShadowContainerLayerOGL::GetFirstChildOGL()
{
  if (!mFirstChild) {
    return nsnull;
   }
  return static_cast<LayerOGL*>(mFirstChild->ImplData());
}
 
void
ShadowContainerLayerOGL::RenderLayer(int aPreviousFrameBuffer,
                                     const nsIntPoint& aOffset)
{
  ContainerRender(this, aPreviousFrameBuffer, aOffset, mOGLManager);
}

#endif  // MOZ_IPC


} /* layers */
} /* mozilla */
