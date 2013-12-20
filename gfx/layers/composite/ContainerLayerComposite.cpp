/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContainerLayerComposite.h"
#include <algorithm>                    // for min
#include "FrameMetrics.h"               // for FrameMetrics
#include "Units.h"                      // for LayerRect, LayerPixel, etc
#include "gfx2DGlue.h"                  // for ToMatrix4x4
#include "gfx3DMatrix.h"                // for gfx3DMatrix
#include "gfxImageSurface.h"            // for gfxImageSurface
#include "gfxMatrix.h"                  // for gfxMatrix
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxUtils.h"                   // for gfxUtils, etc
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/gfx/BaseRect.h"       // for BaseRect
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/gfx/Point.h"          // for Point, IntPoint
#include "mozilla/gfx/Rect.h"           // for IntRect, Rect
#include "mozilla/layers/Compositor.h"  // for Compositor, etc
#include "mozilla/layers/CompositorTypes.h"  // for DIAGNOSTIC_CONTAINER
#include "mozilla/layers/Effects.h"     // for Effect, EffectChain, etc
#include "mozilla/layers/TextureHost.h"  // for CompositingRenderTarget
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsUtils.h"           // for NS_ADDREF, NS_RELEASE
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for nsIntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "nsTArray.h"                   // for nsAutoTArray
#include "nsTraceRefcnt.h"              // for MOZ_COUNT_CTOR, etc
#include <vector>

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

/**
 * Returns a rectangle of content painted opaquely by aLayer. Very consertative;
 * bails by returning an empty rect in any tricky situations.
 */
static nsIntRect
GetOpaqueRect(Layer* aLayer)
{
  nsIntRect result;
  // Just bail if there's anything difficult to handle.
  if (!aLayer->GetEffectiveTransform().IsIdentity() ||
      aLayer->GetEffectiveOpacity() != 1.0f ||
      aLayer->GetMaskLayer()) {
    return result;
  }
  if (aLayer->GetContentFlags() & Layer::CONTENT_OPAQUE) {
    result = aLayer->GetEffectiveVisibleRegion().GetLargestRectangle();
  } else {
    // Drill down into RefLayers because that's what we particularly care about;
    // layer construction for aLayer will not have known about the opaqueness
    // of any RefLayer subtrees.
    RefLayer* refLayer = aLayer->AsRefLayer();
    if (refLayer && refLayer->GetFirstChild()) {
      result = GetOpaqueRect(refLayer->GetFirstChild());
    }
  }
  const nsIntRect* clipRect = aLayer->GetEffectiveClipRect();
  if (clipRect) {
    result.IntersectRect(result, *clipRect);
  }
  return result;
}

struct LayerVelocityUserData : public LayerUserData {
public:
  LayerVelocityUserData() {
    MOZ_COUNT_CTOR(LayerVelocityUserData);
  }
  ~LayerVelocityUserData() {
    MOZ_COUNT_DTOR(LayerVelocityUserData);
  }

  struct VelocityData {
    VelocityData(TimeStamp frameTime, int scrollX, int scrollY)
      : mFrameTime(frameTime)
      , mPoint(scrollX, scrollY)
    {}

    TimeStamp mFrameTime;
    gfx::Point mPoint;
  };
  std::vector<VelocityData> mData;
};

static void DrawVelGraph(const nsIntRect& aClipRect,
                         LayerManagerComposite* aManager,
                         Layer* aLayer) {
  static char sLayerVelocityUserDataKey;
  Compositor* compositor = aManager->GetCompositor();
  gfx::Rect clipRect(aClipRect.x, aClipRect.y,
                     aClipRect.width, aClipRect.height);

  TimeStamp now = TimeStamp::Now();

  void* key = reinterpret_cast<void*>(&sLayerVelocityUserDataKey);
  if (!aLayer->HasUserData(key)) {
    aLayer->SetUserData(key, new LayerVelocityUserData());
  }

  LayerVelocityUserData* velocityData =
    static_cast<LayerVelocityUserData*>(aLayer->GetUserData(key));

  if (velocityData->mData.size() >= 1 &&
    now > velocityData->mData[velocityData->mData.size() - 1].mFrameTime +
      TimeDuration::FromMilliseconds(200)) {
    // clear stale data
    velocityData->mData.clear();
  }

  nsIntPoint scrollOffset =
    aLayer->GetEffectiveVisibleRegion().GetBounds().TopLeft();
  velocityData->mData.push_back(
    LayerVelocityUserData::VelocityData(now, scrollOffset.x, scrollOffset.y));


  // XXX: Uncomment these lines to enable ScrollGraph logging. This is
  //      useful for HVGA phones or to output the data to accurate
  //      graphing software.
  //printf_stderr("ScrollGraph (%p): %i, %i\n",
  //  aLayer, scrollOffset.x, scrollOffset.y);

  // Keep a circular buffer of 100.
  size_t circularBufferSize = 100;
  if (velocityData->mData.size() > circularBufferSize) {
    velocityData->mData.erase(velocityData->mData.begin());
  }

  if (velocityData->mData.size() == 1) {
    return;
  }

  // Clear and disable the graph when it's flat
  for (size_t i = 1; i < velocityData->mData.size(); i++) {
    if (velocityData->mData[i - 1].mPoint != velocityData->mData[i].mPoint) {
      break;
    }
    if (i == velocityData->mData.size() - 1) {
      velocityData->mData.clear();
      return;
    }
  }

  if (aLayer->GetEffectiveVisibleRegion().GetBounds().width < 300 ||
      aLayer->GetEffectiveVisibleRegion().GetBounds().height < 300) {
    // Don't want a graph for smaller layers
    return;
  }

  aManager->SetDebugOverlayWantsNextFrame(true);

  gfx::Matrix4x4 transform;
  ToMatrix4x4(aLayer->GetEffectiveTransform(), transform);
  nsIntRect bounds = aLayer->GetEffectiveVisibleRegion().GetBounds();
  gfx::Rect graphBounds = gfx::Rect(bounds.x, bounds.y,
                                    bounds.width, bounds.height);
  gfx::Rect graphRect = gfx::Rect(bounds.x, bounds.y, 200, 100);

  float opacity = 1.0;
  EffectChain effects;
  effects.mPrimaryEffect = new EffectSolidColor(gfx::Color(0.2,0,0,1));
  compositor->DrawQuad(graphRect,
                       clipRect,
                       effects,
                       opacity,
                       transform);

  std::vector<gfx::Point> graph;
  int yScaleFactor = 3;
  for (int32_t i = (int32_t)velocityData->mData.size() - 2; i >= 0; i--) {
    const gfx::Point& p1 = velocityData->mData[i+1].mPoint;
    const gfx::Point& p2 = velocityData->mData[i].mPoint;
    int vel = sqrt((p1.x - p2.x) * (p1.x - p2.x) +
                   (p1.y - p2.y) * (p1.y - p2.y));
    graph.push_back(
      gfx::Point(bounds.x + graphRect.width / circularBufferSize * i,
                 graphBounds.y + graphRect.height - vel/yScaleFactor));
  }

  compositor->DrawLines(graph, clipRect, gfx::Color(0,1,0,1),
                        opacity, transform);

}

template<class ContainerT> void
ContainerRender(ContainerT* aContainer,
                LayerManagerComposite* aManager,
                const nsIntRect& aClipRect)
{
  /**
   * Setup our temporary surface for rendering the contents of this container.
   */
  RefPtr<CompositingRenderTarget> surface;

  Compositor* compositor = aManager->GetCompositor();

  RefPtr<CompositingRenderTarget> previousTarget = compositor->GetCurrentRenderTarget();

  nsIntRect visibleRect = aContainer->GetEffectiveVisibleRegion().GetBounds();

  aContainer->mSupportsComponentAlphaChildren = false;

  float opacity = aContainer->GetEffectiveOpacity();

  bool needsSurface = aContainer->UseIntermediateSurface();
  if (needsSurface) {
    SurfaceInitMode mode = INIT_MODE_CLEAR;
    bool surfaceCopyNeeded = false;
    gfx::IntRect surfaceRect = gfx::IntRect(visibleRect.x, visibleRect.y,
                                            visibleRect.width, visibleRect.height);
    gfx::IntPoint sourcePoint = gfx::IntPoint(visibleRect.x, visibleRect.y);
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
        surfaceCopyNeeded = gfxPlatform::ComponentAlphaEnabled();
        sourcePoint.x += transform.x0;
        sourcePoint.y += transform.y0;
        aContainer->mSupportsComponentAlphaChildren
          = gfxPlatform::ComponentAlphaEnabled();
      }
    }

    sourcePoint -= compositor->GetCurrentRenderTarget()->GetOrigin();
    if (surfaceCopyNeeded) {
      surface = compositor->CreateRenderTargetFromSource(surfaceRect, previousTarget, sourcePoint);
    } else {
      surface = compositor->CreateRenderTarget(surfaceRect, mode);
    }

    if (!surface) {
      return;
    }

    compositor->SetRenderTarget(surface);
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

    if (i + 1 < children.Length() &&
        layerToRender->GetLayer()->GetEffectiveTransform().IsIdentity()) {
      LayerComposite* nextLayer = static_cast<LayerComposite*>(children.ElementAt(i + 1)->ImplData());
      nsIntRect nextLayerOpaqueRect;
      if (nextLayer && nextLayer->GetLayer()) {
        nextLayerOpaqueRect = GetOpaqueRect(nextLayer->GetLayer());
      }
      if (!nextLayerOpaqueRect.IsEmpty()) {
        nsIntRegion visibleRegion;
        visibleRegion.Sub(layerToRender->GetShadowVisibleRegion(), nextLayerOpaqueRect);
        layerToRender->SetShadowVisibleRegion(visibleRegion);
        if (visibleRegion.IsEmpty()) {
          continue;
        }
      }
    }

    nsIntRect clipRect = layerToRender->GetLayer()->
        CalculateScissorRect(aClipRect, &aManager->GetWorldTransform());
    if (clipRect.IsEmpty()) {
      continue;
    }

    if (layerToRender->HasLayerBeenComposited()) {
      // Composer2D will compose this layer so skip GPU composition
      // this time & reset composition flag for next composition phase
      layerToRender->SetLayerComposited(false);
    } else {
      layerToRender->RenderLayer(clipRect);
    }

    if (gfxPlatform::GetPrefLayersScrollGraph()) {
      DrawVelGraph(clipRect, aManager, layerToRender->GetLayer());
    }
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
    LayerManagerComposite::AutoAddMaskEffect autoMaskEffect(aContainer->GetMaskLayer(),
                                                            effectChain,
                                                            !aContainer->GetTransform().CanDraw2D());

    effectChain.mPrimaryEffect = new EffectRenderTarget(surface);

    gfx::Matrix4x4 transform;
    ToMatrix4x4(aContainer->GetEffectiveTransform(), transform);

    gfx::Rect rect(visibleRect.x, visibleRect.y, visibleRect.width, visibleRect.height);
    gfx::Rect clipRect(aClipRect.x, aClipRect.y, aClipRect.width, aClipRect.height);
    aManager->GetCompositor()->DrawQuad(rect, clipRect, effectChain, opacity,
                                        transform);
  }

  if (aContainer->GetFrameMetrics().IsScrollable()) {
    gfx::Matrix4x4 transform;
    ToMatrix4x4(aContainer->GetEffectiveTransform(), transform);

    const FrameMetrics& frame = aContainer->GetFrameMetrics();
    LayerRect layerBounds = ScreenRect(frame.mCompositionBounds) * ScreenToLayerScale(1.0);
    gfx::Rect rect(layerBounds.x, layerBounds.y, layerBounds.width, layerBounds.height);
    gfx::Rect clipRect(aClipRect.x, aClipRect.y, aClipRect.width, aClipRect.height);
    aManager->GetCompositor()->DrawDiagnostics(DIAGNOSTIC_CONTAINER,
                                               rect, clipRect,
                                               transform);
  }
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
ContainerLayerComposite::RenderLayer(const nsIntRect& aClipRect)
{
  ContainerRender(this, mCompositeManager, aClipRect);
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
RefLayerComposite::RenderLayer(const nsIntRect& aClipRect)
{
  ContainerRender(this, mCompositeManager, aClipRect);
}

void
RefLayerComposite::CleanupResources()
{
}

} /* layers */
} /* mozilla */
