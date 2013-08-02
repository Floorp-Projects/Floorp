/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContainerLayerComposite.h"
#include "gfxUtils.h"
#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/LayersTypes.h"
#include "gfx2DGlue.h"

namespace mozilla {
namespace layers {

// HasOpaqueAncestorLayer and ContainerRender are shared between RefLayer and ContainerLayer
static bool
HasOpaqueAncestorLayer(Layer* aLayer)
{
  for (Layer* l = aLayer->GetParent(); l; l = l->GetParent()) {
    if (l->GetContentFlags() & Layer::CONTENT_OPAQUE)
      return true;
  }
  return false;
}

template<class ContainerT> void
ContainerRender(ContainerT* aContainer,
                const nsIntPoint& aOffset,
                LayerManagerComposite* aManager,
                const nsIntRect& aClipRect)
{
  /**
   * Setup our temporary surface for rendering the contents of this container.
   */
  RefPtr<CompositingRenderTarget> surface;

  Compositor* compositor = aManager->GetCompositor();

  RefPtr<CompositingRenderTarget> previousTarget = compositor->GetCurrentRenderTarget();

  nsIntPoint childOffset(aOffset);
  nsIntRect visibleRect = aContainer->GetEffectiveVisibleRegion().GetBounds();

  aContainer->mSupportsComponentAlphaChildren = false;

  float opacity = aContainer->GetEffectiveOpacity();

  bool needsSurface = aContainer->UseIntermediateSurface();
  if (needsSurface) {
    SurfaceInitMode mode = INIT_MODE_CLEAR;
    bool surfaceCopyNeeded = false;
    gfx::IntRect surfaceRect = gfx::IntRect(visibleRect.x, visibleRect.y,
                                            visibleRect.width, visibleRect.height);
    // we're about to create a framebuffer backed by textures to use as an intermediate
    // surface. What to do if its size (as given by framebufferRect) would exceed the
    // maximum texture size supported by the GL? The present code chooses the compromise
    // of just clamping the framebuffer's size to the max supported size.
    // This gives us a lower resolution rendering of the intermediate surface (children layers).
    // See bug 827170 for a discussion.
    int32_t maxTextureSize = compositor->GetMaxTextureSize();
    surfaceRect.width = std::min(maxTextureSize, surfaceRect.width);
    surfaceRect.height = std::min(maxTextureSize, surfaceRect.height);
    if (aContainer->GetEffectiveVisibleRegion().GetNumRects() == 1 &&
        (aContainer->GetContentFlags() & Layer::CONTENT_OPAQUE))
    {
      // don't need a background, we're going to paint all opaque stuff
      aContainer->mSupportsComponentAlphaChildren = true;
      mode = INIT_MODE_NONE;
    } else {
      const gfx3DMatrix& transform3D = aContainer->GetEffectiveTransform();
      gfxMatrix transform;
      // If we have an opaque ancestor layer, then we can be sure that
      // all the pixels we draw into are either opaque already or will be
      // covered by something opaque. Otherwise copying up the background is
      // not safe.
      if (HasOpaqueAncestorLayer(aContainer) &&
          transform3D.Is2D(&transform) && !transform.HasNonIntegerTranslation()) {
        mode = gfxPlatform::GetPlatform()->UsesSubpixelAATextRendering() ?
                                            INIT_MODE_COPY : INIT_MODE_CLEAR;
        surfaceCopyNeeded = (mode == INIT_MODE_COPY);
        surfaceRect.x += transform.x0;
        surfaceRect.y += transform.y0;
        aContainer->mSupportsComponentAlphaChildren
          = gfxPlatform::GetPlatform()->UsesSubpixelAATextRendering();
      }
    }

    surfaceRect -= gfx::IntPoint(aOffset.x, aOffset.y);
    if (surfaceCopyNeeded) {
      surface = compositor->CreateRenderTargetFromSource(surfaceRect, previousTarget);
    } else {
      surface = compositor->CreateRenderTarget(surfaceRect, mode);
    }
    compositor->SetRenderTarget(surface);
    childOffset.x = visibleRect.x;
    childOffset.y = visibleRect.y;
  } else {
    surface = previousTarget;
    aContainer->mSupportsComponentAlphaChildren = (aContainer->GetContentFlags() & Layer::CONTENT_OPAQUE) ||
      (aContainer->GetParent() && aContainer->GetParent()->SupportsComponentAlphaChildren());
  }

  nsAutoTArray<Layer*, 12> children;
  aContainer->SortChildrenBy3DZOrder(children);

  /**
   * Render this container's contents.
   */
  for (uint32_t i = 0; i < children.Length(); i++) {
    LayerComposite* layerToRender = static_cast<LayerComposite*>(children.ElementAt(i)->ImplData());

    if (layerToRender->GetLayer()->GetEffectiveVisibleRegion().IsEmpty() &&
        !layerToRender->GetLayer()->AsContainerLayer()) {
      continue;
    }

    nsIntRect clipRect = layerToRender->GetLayer()->
        CalculateScissorRect(aClipRect, &aManager->GetWorldTransform());
    if (clipRect.IsEmpty()) {
      continue;
    }

    layerToRender->RenderLayer(childOffset, clipRect);
    // invariant: our GL context should be current here, I don't think we can
    // assert it though
  }

  if (needsSurface) {
    // Unbind the current surface and rebind the previous one.
#ifdef MOZ_DUMP_PAINTING
    if (gfxUtils::sDumpPainting) {
      nsRefPtr<gfxImageSurface> surf = surface->Dump(aManager->GetCompositor());
      WriteSnapshotToDumpFile(aContainer, surf);
    }
#endif

    compositor->SetRenderTarget(previousTarget);
    EffectChain effectChain;
    LayerManagerComposite::AddMaskEffect(aContainer->GetMaskLayer(),
                                         effectChain,
                                         !aContainer->GetTransform().CanDraw2D());

    effectChain.mPrimaryEffect = new EffectRenderTarget(surface);

    gfx::Matrix4x4 transform;
    ToMatrix4x4(aContainer->GetEffectiveTransform(), transform);

    gfx::Rect rect(visibleRect.x, visibleRect.y, visibleRect.width, visibleRect.height);
    gfx::Rect clipRect(aClipRect.x, aClipRect.y, aClipRect.width, aClipRect.height);
    aManager->GetCompositor()->DrawQuad(rect, clipRect, effectChain, opacity,
                                        transform, gfx::Point(aOffset.x, aOffset.y));
  }

  if (aContainer->GetFrameMetrics().IsScrollable()) {
    gfx::Matrix4x4 transform;
    ToMatrix4x4(aContainer->GetEffectiveTransform(), transform);

    const FrameMetrics& frame = aContainer->GetFrameMetrics();
    LayerRect layerViewport = frame.mViewport * frame.LayersPixelsPerCSSPixel();
    gfx::Rect rect(layerViewport.x, layerViewport.y, layerViewport.width, layerViewport.height);
    gfx::Rect clipRect(aClipRect.x, aClipRect.y, aClipRect.width, aClipRect.height);
    aManager->GetCompositor()->DrawDiagnostics(DIAGNOSTIC_CONTAINER,
                                               rect, clipRect,
                                               transform, gfx::Point(aOffset.x, aOffset.y));
  }

  LayerManagerComposite::RemoveMaskEffect(aContainer->GetMaskLayer());
}

ContainerLayerComposite::ContainerLayerComposite(LayerManagerComposite *aManager)
  : ContainerLayer(aManager, nullptr)
  , LayerComposite(aManager)
{
  MOZ_COUNT_CTOR(ContainerLayerComposite);
  mImplData = static_cast<LayerComposite*>(this);
}

ContainerLayerComposite::~ContainerLayerComposite()
{
  MOZ_COUNT_DTOR(ContainerLayerComposite);

  // We don't Destroy() on destruction here because this destructor
  // can be called after remote content has crashed, and it may not be
  // safe to free the IPC resources of our children.  Those resources
  // are automatically cleaned up by IPDL-generated code.
  //
  // In the common case of normal shutdown, either
  // LayerManagerComposite::Destroy(), a parent
  // *ContainerLayerComposite::Destroy(), or Disconnect() will trigger
  // cleanup of our resources.
  while (mFirstChild) {
    RemoveChild(mFirstChild);
  }
}

void
ContainerLayerComposite::InsertAfter(Layer* aChild, Layer* aAfter)
{
  NS_ASSERTION(aChild->Manager() == Manager(),
               "Child has wrong manager");
  NS_ASSERTION(!aChild->GetParent(),
               "aChild already in the tree");
  NS_ASSERTION(!aChild->GetNextSibling() && !aChild->GetPrevSibling(),
               "aChild already has siblings?");
  NS_ASSERTION(!aAfter ||
               (aAfter->Manager() == Manager() &&
                aAfter->GetParent() == this),
               "aAfter is not our child");

  aChild->SetParent(this);
  if (aAfter == mLastChild) {
    mLastChild = aChild;
  }
  if (!aAfter) {
    aChild->SetNextSibling(mFirstChild);
    if (mFirstChild) {
      mFirstChild->SetPrevSibling(aChild);
    }
    mFirstChild = aChild;
    NS_ADDREF(aChild);
    DidInsertChild(aChild);
    return;
  }

  Layer* next = aAfter->GetNextSibling();
  aChild->SetNextSibling(next);
  aChild->SetPrevSibling(aAfter);
  if (next) {
    next->SetPrevSibling(aChild);
  }
  aAfter->SetNextSibling(aChild);
  NS_ADDREF(aChild);
  DidInsertChild(aChild);
}

void
ContainerLayerComposite::RemoveChild(Layer *aChild)
{
  NS_ASSERTION(aChild->Manager() == Manager(),
               "Child has wrong manager");
  NS_ASSERTION(aChild->GetParent() == this,
               "aChild not our child");

  Layer* prev = aChild->GetPrevSibling();
  Layer* next = aChild->GetNextSibling();
  if (prev) {
    prev->SetNextSibling(next);
  } else {
    this->mFirstChild = next;
  }
  if (next) {
    next->SetPrevSibling(prev);
  } else {
    this->mLastChild = prev;
  }

  aChild->SetNextSibling(nullptr);
  aChild->SetPrevSibling(nullptr);
  aChild->SetParent(nullptr);

  this->DidRemoveChild(aChild);
  NS_RELEASE(aChild);
}

void
ContainerLayerComposite::Destroy()
{
  if (!mDestroyed) {
    while (mFirstChild) {
      static_cast<LayerComposite*>(GetFirstChild()->ImplData())->Destroy();
      RemoveChild(mFirstChild);
    }
    mDestroyed = true;
  }
}

LayerComposite*
ContainerLayerComposite::GetFirstChildComposite()
{
  if (!mFirstChild) {
    return nullptr;
   }
  return static_cast<LayerComposite*>(mFirstChild->ImplData());
}

void
ContainerLayerComposite::RepositionChild(Layer* aChild, Layer* aAfter)
{
  NS_ASSERTION(aChild->Manager() == Manager(),
               "Child has wrong manager");
  NS_ASSERTION(aChild->GetParent() == this,
               "aChild not our child");
  NS_ASSERTION(!aAfter ||
               (aAfter->Manager() == Manager() &&
                aAfter->GetParent() == this),
               "aAfter is not our child");

  Layer* prev = aChild->GetPrevSibling();
  Layer* next = aChild->GetNextSibling();
  if (prev == aAfter) {
    // aChild is already in the correct position, nothing to do.
    return;
  }
  if (prev) {
    prev->SetNextSibling(next);
  }
  if (next) {
    next->SetPrevSibling(prev);
  }
  if (!aAfter) {
    aChild->SetPrevSibling(nullptr);
    aChild->SetNextSibling(mFirstChild);
    if (mFirstChild) {
      mFirstChild->SetPrevSibling(aChild);
    }
    mFirstChild = aChild;
    return;
  }

  Layer* afterNext = aAfter->GetNextSibling();
  if (afterNext) {
    afterNext->SetPrevSibling(aChild);
  } else {
    mLastChild = aChild;
  }
  aAfter->SetNextSibling(aChild);
  aChild->SetPrevSibling(aAfter);
  aChild->SetNextSibling(afterNext);
}

void
ContainerLayerComposite::RenderLayer(const nsIntPoint& aOffset,
                                     const nsIntRect& aClipRect)
{
  ContainerRender(this, aOffset, mCompositeManager, aClipRect);
}

void
ContainerLayerComposite::CleanupResources()
{
  for (Layer* l = GetFirstChild(); l; l = l->GetNextSibling()) {
    LayerComposite* layerToCleanup = static_cast<LayerComposite*>(l->ImplData());
    layerToCleanup->CleanupResources();
  }
}

RefLayerComposite::RefLayerComposite(LayerManagerComposite* aManager)
  : RefLayer(aManager, nullptr)
  , LayerComposite(aManager)
{
  mImplData = static_cast<LayerComposite*>(this);
}

RefLayerComposite::~RefLayerComposite()
{
  Destroy();
}

void
RefLayerComposite::Destroy()
{
  MOZ_ASSERT(!mFirstChild);
  mDestroyed = true;
}

LayerComposite*
RefLayerComposite::GetFirstChildComposite()
{
  if (!mFirstChild) {
    return nullptr;
   }
  return static_cast<LayerComposite*>(mFirstChild->ImplData());
}

void
RefLayerComposite::RenderLayer(const nsIntPoint& aOffset,
                               const nsIntRect& aClipRect)
{
  ContainerRender(this, aOffset, mCompositeManager, aClipRect);
}

void
RefLayerComposite::CleanupResources()
{
}

} /* layers */
} /* mozilla */
