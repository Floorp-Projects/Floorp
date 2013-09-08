/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContainerLayerOGL.h"
#include <stdint.h>                     // for uint32_t
#include <algorithm>                    // for min
#include "GLContext.h"
#include "gfx3DMatrix.h"                // for gfx3DMatrix
#include "gfxMatrix.h"                  // for gfxMatrix
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxUtils.h"                   // for gfxUtils, etc
#include "mozilla/gfx/BaseRect.h"       // for BaseRect
#include "mozilla/layers/CompositorTypes.h"  // for MaskType, etc
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsUtils.h"           // for NS_ADDREF, NS_RELEASE
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for nsIntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "nsTArray.h"                   // for nsAutoTArray
#include "LayerManagerOGL.h"            // for LayerManagerOGL, LayerOGL, etc
#include "LayerManagerOGLProgram.h"     // for ShaderProgramOGL
class gfxImageSurface;

namespace mozilla {
namespace layers {

static inline LayerOGL*
GetNextSibling(LayerOGL* aLayer)
{
   Layer* layer = aLayer->GetLayer()->GetNextSibling();
   return layer ? static_cast<LayerOGL*>(layer->
                                         ImplData())
                 : nullptr;
}

ContainerLayerOGL::ContainerLayerOGL(LayerManagerOGL *mOGLManager)
  : ContainerLayer(mOGLManager, nullptr)
  , LayerOGL(mOGLManager)
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
    mDestroyed = true;
  }
}

LayerOGL*
ContainerLayerOGL::GetFirstChildOGL()
{
  if (!mFirstChild) {
    return nullptr;
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
  nsIntRect visibleRect = GetEffectiveVisibleRegion().GetBounds();

  nsIntRect cachedScissor = gl()->ScissorRect();
  gl()->PushScissorRect();
  mSupportsComponentAlphaChildren = false;

  float opacity = GetEffectiveOpacity();
  const gfx3DMatrix& transform = GetEffectiveTransform();
  bool needsFramebuffer = UseIntermediateSurface();
  if (needsFramebuffer) {
    nsIntRect framebufferRect = visibleRect;
    // we're about to create a framebuffer backed by textures to use as an intermediate
    // surface. What to do if its size (as given by framebufferRect) would exceed the
    // maximum texture size supported by the GL? The present code chooses the compromise
    // of just clamping the framebuffer's size to the max supported size.
    // This gives us a lower resolution rendering of the intermediate surface (children layers).
    // See bug 827170 for a discussion.
    GLint maxTexSize;
    gl()->fGetIntegerv(LOCAL_GL_MAX_TEXTURE_SIZE, &maxTexSize);
    framebufferRect.width = std::min(framebufferRect.width, maxTexSize);
    framebufferRect.height = std::min(framebufferRect.height, maxTexSize);

    LayerManagerOGL::InitMode mode = LayerManagerOGL::InitModeClear;
    if (GetEffectiveVisibleRegion().GetNumRects() == 1 && 
        (GetContentFlags() & Layer::CONTENT_OPAQUE))
    {
      // don't need a background, we're going to paint all opaque stuff
      mSupportsComponentAlphaChildren = true;
      mode = LayerManagerOGL::InitModeNone;
    } else {
      const gfx3DMatrix& transform3D = GetEffectiveTransform();
      gfxMatrix transform;
      // If we have an opaque ancestor layer, then we can be sure that
      // all the pixels we draw into are either opaque already or will be
      // covered by something opaque. Otherwise copying up the background is
      // not safe.
      if (HasOpaqueAncestorLayer(this) &&
          transform3D.Is2D(&transform) && !transform.HasNonIntegerTranslation()) {
        mode = gfxPlatform::ComponentAlphaEnabled() ?
          LayerManagerOGL::InitModeCopy :
          LayerManagerOGL::InitModeClear;
        framebufferRect.x += transform.x0;
        framebufferRect.y += transform.y0;
        mSupportsComponentAlphaChildren = gfxPlatform::ComponentAlphaEnabled();
      }
    }

    gl()->PushViewportRect();
    framebufferRect -= childOffset;
    if (!mOGLManager->CompositingDisabled()) {
      if (!mOGLManager->CreateFBOWithTexture(framebufferRect,
                                          mode,
                                          aPreviousFrameBuffer,
                                          &frameBuffer,
                                          &containerSurface)) {
        gl()->PopViewportRect();
        gl()->PopScissorRect();
        gl()->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, aPreviousFrameBuffer);
        return;
      }
    }
    childOffset.x = visibleRect.x;
    childOffset.y = visibleRect.y;
  } else {
    frameBuffer = aPreviousFrameBuffer;
    mSupportsComponentAlphaChildren = (GetContentFlags() & Layer::CONTENT_OPAQUE) ||
      (GetParent() && GetParent()->SupportsComponentAlphaChildren());
  }

  nsAutoTArray<Layer*, 12> children;
  SortChildrenBy3DZOrder(children);

  /**
   * Render this container's contents.
   */
  for (uint32_t i = 0; i < children.Length(); i++) {
    LayerOGL* layerToRender = static_cast<LayerOGL*>(children.ElementAt(i)->ImplData());

    if (layerToRender->GetLayer()->GetEffectiveVisibleRegion().IsEmpty()) {
      continue;
    }

    nsIntRect scissorRect = layerToRender->GetLayer()->
        CalculateScissorRect(cachedScissor, &mOGLManager->GetWorldTransform());
    if (scissorRect.IsEmpty()) {
      continue;
    }

    gl()->fScissor(scissorRect.x, 
                               scissorRect.y, 
                               scissorRect.width, 
                               scissorRect.height);

    layerToRender->RenderLayer(frameBuffer, childOffset);
    gl()->MakeCurrent();
  }


  if (needsFramebuffer) {
    // Unbind the current framebuffer and rebind the previous one.
#ifdef MOZ_DUMP_PAINTING
    if (gfxUtils::sDumpPainting) {
      nsRefPtr<gfxImageSurface> surf = 
        gl()->GetTexImage(containerSurface, true, mOGLManager->GetFBOTextureFormat());

      WriteSnapshotToDumpFile(this, surf);
    }
#endif
    
    // Restore the viewport
    gl()->PopViewportRect();
    nsIntRect viewport = gl()->ViewportRect();
    mOGLManager->SetupPipeline(viewport.width, viewport.height,
                            LayerManagerOGL::ApplyWorldTransform);
    gl()->PopScissorRect();

    if (!mOGLManager->CompositingDisabled()) {
      gl()->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, aPreviousFrameBuffer);
      gl()->fDeleteFramebuffers(1, &frameBuffer);

      gl()->fActiveTexture(LOCAL_GL_TEXTURE0);

      gl()->fBindTexture(mOGLManager->FBOTextureTarget(), containerSurface);

      MaskType maskType = MaskNone;
      if (GetMaskLayer()) {
        if (!GetTransform().CanDraw2D()) {
          maskType = Mask3d;
        } else {
          maskType = Mask2d;
        }
      }
      ShaderProgramOGL *rgb =
        mOGLManager->GetFBOLayerProgram(maskType);

      rgb->Activate();
      rgb->SetLayerQuadRect(visibleRect);
      rgb->SetLayerTransform(transform);
      rgb->SetTextureTransform(gfx3DMatrix());
      rgb->SetLayerOpacity(opacity);
      rgb->SetRenderOffset(aOffset);
      rgb->SetTextureUnit(0);
      rgb->LoadMask(GetMaskLayer());

      if (rgb->GetTexCoordMultiplierUniformLocation() != -1) {
        // 2DRect case, get the multiplier right for a sampler2DRect
        rgb->SetTexCoordMultiplier(visibleRect.width, visibleRect.height);
      }

      // Drawing is always flipped, but when copying between surfaces we want to avoid
      // this. Pass true for the flip parameter to introduce a second flip
      // that cancels the other one out.
      mOGLManager->BindAndDrawQuad(rgb, true);

      // Clean up resources.  This also unbinds the texture.
      gl()->fDeleteTextures(1, &containerSurface);
    }
  } else {
    gl()->PopScissorRect();
  }
}

void
ContainerLayerOGL::CleanupResources()
{
  for (Layer* l = GetFirstChild(); l; l = l->GetNextSibling()) {
    LayerOGL* layerToRender = static_cast<LayerOGL*>(l->ImplData());
    layerToRender->CleanupResources();
  }
}

} /* layers */
} /* mozilla */
