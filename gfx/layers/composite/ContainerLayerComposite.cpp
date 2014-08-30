/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContainerLayerComposite.h"
#include <algorithm>                    // for min
#include "FrameMetrics.h"               // for FrameMetrics
#include "Units.h"                      // for LayerRect, LayerPixel, etc
#include "gfx2DGlue.h"                  // for ToMatrix4x4
#include "gfxPrefs.h"                   // for gfxPrefs
#include "gfxUtils.h"                   // for gfxUtils, etc
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/UniquePtr.h"          // for UniquePtr
#include "mozilla/gfx/BaseRect.h"       // for BaseRect
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/gfx/Point.h"          // for Point, IntPoint
#include "mozilla/gfx/Rect.h"           // for IntRect, Rect
#include "mozilla/layers/Compositor.h"  // for Compositor, etc
#include "mozilla/layers/CompositorTypes.h"  // for DiagnosticFlags::CONTAINER
#include "mozilla/layers/Effects.h"     // for Effect, EffectChain, etc
#include "mozilla/layers/TextureHost.h"  // for CompositingRenderTarget
#include "mozilla/layers/AsyncPanZoomController.h"  // for AsyncPanZoomController
#include "mozilla/layers/AsyncCompositionManager.h" // for ViewTransform
#include "mozilla/layers/LayerMetricsWrapper.h" // for LayerMetricsWrapper
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsISupportsUtils.h"           // for NS_ADDREF, NS_RELEASE
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for nsIntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "nsTArray.h"                   // for nsAutoTArray
#include "TextRenderer.h"               // for TextRenderer
#include <vector>

#define CULLING_LOG(...)
// #define CULLING_LOG(...) printf_stderr("CULLING: " __VA_ARGS__)

namespace mozilla {
namespace layers {

using namespace gfx;

/**
 * Returns a rectangle of content painted opaquely by aLayer. Very consertative;
 * bails by returning an empty rect in any tricky situations.
 */
static nsIntRect
GetOpaqueRect(Layer* aLayer)
{
  nsIntRect result;
  gfx::Matrix matrix;
  bool is2D = aLayer->GetBaseTransform().Is2D(&matrix);

  // Just bail if there's anything difficult to handle.
  if (!is2D || aLayer->GetMaskLayer() ||
    aLayer->GetIsFixedPosition() ||
    aLayer->GetIsStickyPosition() ||
    aLayer->GetEffectiveOpacity() != 1.0f ||
    matrix.HasNonIntegerTranslation()) {
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

  // Translate our opaque region to cover the child
  gfx::Point point = matrix.GetTranslation();
  result.MoveBy(static_cast<int>(point.x), static_cast<int>(point.y));

  const nsIntRect* clipRect = aLayer->GetEffectiveClipRect();
  if (clipRect) {
    result.IntersectRect(result, *clipRect);
  }

  return result;
}

static void DrawLayerInfo(const RenderTargetIntRect& aClipRect,
                          LayerManagerComposite* aManager,
                          Layer* aLayer)
{

  if (aLayer->GetType() == Layer::LayerType::TYPE_CONTAINER) {
    // XXX - should figure out a way to render this, but for now this
    // is hard to do, since it will often get superimposed over the first
    // child of the layer, which is bad.
    return;
  }

  std::stringstream ss;
  aLayer->PrintInfo(ss, "");

  nsIntRegion visibleRegion = aLayer->GetVisibleRegion();

  uint32_t maxWidth = std::min<uint32_t>(visibleRegion.GetBounds().width, 500);

  nsIntPoint topLeft = visibleRegion.GetBounds().TopLeft();
  aManager->GetTextRenderer()->RenderText(ss.str().c_str(), gfx::IntPoint(topLeft.x, topLeft.y),
                                          aLayer->GetEffectiveTransform(), 16,
                                          maxWidth);
}

static void PrintUniformityInfo(Layer* aLayer)
{
  static TimeStamp t0 = TimeStamp::Now();

  // Don't want to print a log for smaller layers
  if (aLayer->GetEffectiveVisibleRegion().GetBounds().width < 300 ||
      aLayer->GetEffectiveVisibleRegion().GetBounds().height < 300) {
    return;
  }

  Matrix4x4 transform = aLayer->AsLayerComposite()->GetShadowTransform();
  if (!transform.Is2D()) {
    return;
  }
  Point translation = transform.As2D().GetTranslation();
  printf_stderr("UniformityInfo Layer_Move %llu %p %s\n",
      (unsigned long long)(TimeStamp::Now() - t0).ToMilliseconds(), aLayer,
      ToString(translation).c_str());
}

/* all of the per-layer prepared data we need to maintain */
struct PreparedLayer
{
  PreparedLayer(LayerComposite *aLayer, RenderTargetIntRect aClipRect, bool aRestoreVisibleRegion, nsIntRegion &aVisibleRegion) :
    mLayer(aLayer), mClipRect(aClipRect), mRestoreVisibleRegion(aRestoreVisibleRegion), mSavedVisibleRegion(aVisibleRegion) {}
  LayerComposite* mLayer;
  RenderTargetIntRect mClipRect;
  bool mRestoreVisibleRegion;
  nsIntRegion mSavedVisibleRegion;
};

/* all of the prepared data that we need in RenderLayer() */
struct PreparedData
{
  RefPtr<CompositingRenderTarget> mTmpTarget;
  nsAutoTArray<PreparedLayer, 12> mLayers;
};

// ContainerPrepare is shared between RefLayer and ContainerLayer
template<class ContainerT> void
ContainerPrepare(ContainerT* aContainer,
                 LayerManagerComposite* aManager,
                 const RenderTargetIntRect& aClipRect)
{
  aContainer->mPrepared = MakeUnique<PreparedData>();

  /**
   * Determine which layers to draw.
   */
  nsAutoTArray<Layer*, 12> children;
  aContainer->SortChildrenBy3DZOrder(children);

  for (uint32_t i = 0; i < children.Length(); i++) {
    LayerComposite* layerToRender = static_cast<LayerComposite*>(children.ElementAt(i)->ImplData());

    if (layerToRender->GetLayer()->GetEffectiveVisibleRegion().IsEmpty() &&
        !layerToRender->GetLayer()->AsContainerLayer()) {
      continue;
    }

    RenderTargetIntRect clipRect = layerToRender->GetLayer()->
        CalculateScissorRect(aClipRect, &aManager->GetWorldTransform());
    if (clipRect.IsEmpty()) {
      continue;
    }

    RenderTargetRect quad = layerToRender->GetLayer()->
      TransformRectToRenderTarget(LayerPixel::FromUntyped(
        layerToRender->GetLayer()->GetEffectiveVisibleRegion().GetBounds()));

    Compositor* compositor = aManager->GetCompositor();
    if (!layerToRender->GetLayer()->AsContainerLayer() &&
        !quad.Intersects(compositor->ClipRectInLayersCoordinates(layerToRender->GetLayer(), clipRect))) {
      continue;
    }

    CULLING_LOG("Preparing sublayer %p\n", layerToRender->GetLayer());

    nsIntRegion savedVisibleRegion;
    bool restoreVisibleRegion = false;
    gfx::Matrix matrix;
    bool is2D = layerToRender->GetLayer()->GetBaseTransform().Is2D(&matrix);
    if (i + 1 < children.Length() &&
        is2D && !matrix.HasNonIntegerTranslation()) {
      LayerComposite* nextLayer = static_cast<LayerComposite*>(children.ElementAt(i + 1)->ImplData());
      CULLING_LOG("Culling against %p\n", nextLayer->GetLayer());
      nsIntRect nextLayerOpaqueRect;
      if (nextLayer && nextLayer->GetLayer()) {
        nextLayerOpaqueRect = GetOpaqueRect(nextLayer->GetLayer());
        gfx::Point point = matrix.GetTranslation();
        nextLayerOpaqueRect.MoveBy(static_cast<int>(-point.x), static_cast<int>(-point.y));
        CULLING_LOG("  point %i, %i\n", static_cast<int>(-point.x), static_cast<int>(-point.y));
        CULLING_LOG("  opaque rect %i, %i, %i, %i\n", nextLayerOpaqueRect.x, nextLayerOpaqueRect.y, nextLayerOpaqueRect.width, nextLayerOpaqueRect.height);
      }
      if (!nextLayerOpaqueRect.IsEmpty()) {
        CULLING_LOG("  draw\n");
        savedVisibleRegion = layerToRender->GetShadowVisibleRegion();
        nsIntRegion visibleRegion;
        visibleRegion.Sub(savedVisibleRegion, nextLayerOpaqueRect);
        if (visibleRegion.IsEmpty()) {
          continue;
        }
        layerToRender->SetShadowVisibleRegion(visibleRegion);
        restoreVisibleRegion = true;
      } else {
        CULLING_LOG("  skip\n");
      }
    }
    layerToRender->Prepare(clipRect);
    aContainer->mPrepared->mLayers.AppendElement(PreparedLayer(layerToRender, clipRect, restoreVisibleRegion, savedVisibleRegion));
  }

  CULLING_LOG("Preparing container layer %p\n", aContainer->GetLayer());

  /**
   * Setup our temporary surface for rendering the contents of this container.
   */

  bool surfaceCopyNeeded;
  // DefaultComputeSupportsComponentAlphaChildren can mutate aContainer so call it unconditionally
  aContainer->DefaultComputeSupportsComponentAlphaChildren(&surfaceCopyNeeded);
  if (aContainer->UseIntermediateSurface()) {
    MOZ_PERFORMANCE_WARNING("gfx", "[%p] Container layer requires intermediate surface\n", aContainer);
    if (!surfaceCopyNeeded) {
      // If we don't need a copy we can render to the intermediate now to avoid
      // unecessary render target switching. This brings a big perf boost on mobile gpus.
      RefPtr<CompositingRenderTarget> surface = CreateTemporaryTarget(aContainer, aManager);
      RenderIntermediate(aContainer, aManager, RenderTargetPixel::ToUntyped(aClipRect), surface);
      aContainer->mPrepared->mTmpTarget = surface;
    }
  }
}

template<class ContainerT> void
RenderLayers(ContainerT* aContainer,
	     LayerManagerComposite* aManager,
	     const RenderTargetIntRect& aClipRect)
{
  Compositor* compositor = aManager->GetCompositor();

  float opacity = aContainer->GetEffectiveOpacity();

  // If this is a scrollable container layer, and it's overscrolled, the layer's
  // contents are transformed in a way that would leave blank regions in the
  // composited area. If the layer has a background color, fill these areas
  // with the background color by drawing a rectangle of the background color
  // over the entire composited area before drawing the container contents.
  // Make sure not to do this on a "scrollinfo" layer because it's just a
  // placeholder for APZ purposes.
  if (aContainer->HasScrollableFrameMetrics() && !aContainer->IsScrollInfoLayer()) {
    bool overscrolled = false;
    for (uint32_t i = 0; i < aContainer->GetFrameMetricsCount(); i++) {
      AsyncPanZoomController* apzc = aContainer->GetAsyncPanZoomController(i);
      if (apzc && apzc->IsOverscrolled()) {
        overscrolled = true;
        break;
      }
    }
    if (overscrolled) {
      gfxRGBA color = aContainer->GetBackgroundColor();
      // If the background is completely transparent, there's no point in
      // drawing anything for it. Hopefully the layers behind, if any, will
      // provide suitable content for the overscroll effect.
      if (color.a != 0.0) {
        EffectChain effectChain(aContainer);
        effectChain.mPrimaryEffect = new EffectSolidColor(ToColor(color));
        gfx::Rect clipRect(aClipRect.x, aClipRect.y, aClipRect.width, aClipRect.height);
        compositor->DrawQuad(
          RenderTargetPixel::ToUnknown(
            compositor->ClipRectInLayersCoordinates(aContainer, aClipRect)),
          clipRect, effectChain, opacity, Matrix4x4());
      }
    }
  }

  for (size_t i = 0u; i < aContainer->mPrepared->mLayers.Length(); i++) {
    PreparedLayer& preparedData = aContainer->mPrepared->mLayers[i];
    LayerComposite* layerToRender = preparedData.mLayer;
    const RenderTargetIntRect& clipRect = preparedData.mClipRect;

    if (layerToRender->HasLayerBeenComposited()) {
      // Composer2D will compose this layer so skip GPU composition
      // this time & reset composition flag for next composition phase
      layerToRender->SetLayerComposited(false);
      nsIntRect clearRect = layerToRender->GetClearRect();
      if (!clearRect.IsEmpty()) {
        // Clear layer's visible rect on FrameBuffer with transparent pixels
        gfx::Rect fbRect(clearRect.x, clearRect.y, clearRect.width, clearRect.height);
        compositor->ClearRect(fbRect);
        layerToRender->SetClearRect(nsIntRect(0, 0, 0, 0));
      }
    } else {
      layerToRender->RenderLayer(RenderTargetPixel::ToUntyped(clipRect));
    }

    if (preparedData.mRestoreVisibleRegion) {
      // Restore the region in case it's not covered by opaque content next time
      layerToRender->SetShadowVisibleRegion(preparedData.mSavedVisibleRegion);
    }

    Layer* layer = layerToRender->GetLayer();
    if (gfxPrefs::UniformityInfo()) {
      PrintUniformityInfo(layer);
    }

    if (gfxPrefs::DrawLayerInfo()) {
      DrawLayerInfo(clipRect, aManager, layer);
    }

    // Draw a border around scrollable layers.
    for (uint32_t i = 0; i < layer->GetFrameMetricsCount(); i++) {
      // A layer can be scrolled by multiple scroll frames. Draw a border
      // for each.
      if (layer->GetFrameMetrics(i).IsScrollable()) {
        // Since the composition bounds are in the parent layer's coordinates,
        // use the parent's effective transform rather than the layer's own.
        ParentLayerRect compositionBounds = layer->GetFrameMetrics(i).mCompositionBounds;
        aManager->GetCompositor()->DrawDiagnostics(DiagnosticFlags::CONTAINER,
                                                   compositionBounds.ToUnknownRect(),
                                                   gfx::Rect(aClipRect.ToUnknownRect()),
                                                   aContainer->GetEffectiveTransform());
      }
    }

    // invariant: our GL context should be current here, I don't think we can
    // assert it though
  }
}

template<class ContainerT> RefPtr<CompositingRenderTarget>
CreateTemporaryTarget(ContainerT* aContainer,
                      LayerManagerComposite* aManager)
{
  Compositor* compositor = aManager->GetCompositor();
  nsIntRect visibleRect = aContainer->GetEffectiveVisibleRegion().GetBounds();
  SurfaceInitMode mode = INIT_MODE_CLEAR;
  gfx::IntRect surfaceRect = gfx::IntRect(visibleRect.x, visibleRect.y,
                                          visibleRect.width, visibleRect.height);
  if (aContainer->GetEffectiveVisibleRegion().GetNumRects() == 1 &&
      (aContainer->GetContentFlags() & Layer::CONTENT_OPAQUE))
  {
    mode = INIT_MODE_NONE;
  }
  return compositor->CreateRenderTarget(surfaceRect, mode);
}

template<class ContainerT> RefPtr<CompositingRenderTarget>
CreateTemporaryTargetAndCopyFromBackground(ContainerT* aContainer,
                                           LayerManagerComposite* aManager)
{
  Compositor* compositor = aManager->GetCompositor();
  nsIntRect visibleRect = aContainer->GetEffectiveVisibleRegion().GetBounds();
  RefPtr<CompositingRenderTarget> previousTarget = compositor->GetCurrentRenderTarget();
  gfx::IntRect surfaceRect = gfx::IntRect(visibleRect.x, visibleRect.y,
                                          visibleRect.width, visibleRect.height);

  gfx::IntPoint sourcePoint = gfx::IntPoint(visibleRect.x, visibleRect.y);

  gfx::Matrix4x4 transform = aContainer->GetEffectiveTransform();
  DebugOnly<gfx::Matrix> transform2d;
  MOZ_ASSERT(transform.Is2D(&transform2d) && !gfx::ThebesMatrix(transform2d).HasNonIntegerTranslation());
  sourcePoint += gfx::IntPoint(transform._41, transform._42);

  sourcePoint -= compositor->GetCurrentRenderTarget()->GetOrigin();

  return compositor->CreateRenderTargetFromSource(surfaceRect, previousTarget, sourcePoint);
}

template<class ContainerT> void
RenderIntermediate(ContainerT* aContainer,
                   LayerManagerComposite* aManager,
                   const nsIntRect& aClipRect,
                   RefPtr<CompositingRenderTarget> surface)
{
  Compositor* compositor = aManager->GetCompositor();
  RefPtr<CompositingRenderTarget> previousTarget = compositor->GetCurrentRenderTarget();

  if (!surface) {
    return;
  }
  compositor->SetRenderTarget(surface);
  // pre-render all of the layers into our temporary
  RenderLayers(aContainer, aManager, RenderTargetPixel::FromUntyped(aClipRect));
  // Unbind the current surface and rebind the previous one.
  compositor->SetRenderTarget(previousTarget);
}

template<class ContainerT> void
ContainerRender(ContainerT* aContainer,
                 LayerManagerComposite* aManager,
                 const nsIntRect& aClipRect)
{
  MOZ_ASSERT(aContainer->mPrepared);
  if (aContainer->UseIntermediateSurface()) {
    RefPtr<CompositingRenderTarget> surface;

    if (!aContainer->mPrepared->mTmpTarget) {
      // we needed to copy the background so we waited until now to render the intermediate
      surface = CreateTemporaryTargetAndCopyFromBackground(aContainer, aManager);
      RenderIntermediate(aContainer, aManager,
                         aClipRect, surface);
    } else {
      surface = aContainer->mPrepared->mTmpTarget;
    }

    float opacity = aContainer->GetEffectiveOpacity();

    nsIntRect visibleRect = aContainer->GetEffectiveVisibleRegion().GetBounds();
#ifdef MOZ_DUMP_PAINTING
    if (gfxUtils::sDumpPainting) {
      RefPtr<gfx::DataSourceSurface> surf = surface->Dump(aManager->GetCompositor());
      if (surf) {
        WriteSnapshotToDumpFile(aContainer, surf);
      }
    }
#endif

    EffectChain effectChain(aContainer);
    LayerManagerComposite::AutoAddMaskEffect autoMaskEffect(aContainer->GetMaskLayer(),
                                                            effectChain,
                                                            !aContainer->GetTransform().CanDraw2D());
    if (autoMaskEffect.Failed()) {
      NS_WARNING("Failed to apply a mask effect.");
      return;
    }

    aContainer->AddBlendModeEffect(effectChain);
    effectChain.mPrimaryEffect = new EffectRenderTarget(surface);

    gfx::Rect rect(visibleRect.x, visibleRect.y, visibleRect.width, visibleRect.height);
    gfx::Rect clipRect(aClipRect.x, aClipRect.y, aClipRect.width, aClipRect.height);
    aManager->GetCompositor()->DrawQuad(rect, clipRect, effectChain, opacity,
                                        aContainer->GetEffectiveTransform());
  } else {
    RenderLayers(aContainer, aManager, RenderTargetPixel::FromUntyped(aClipRect));
  }
  aContainer->mPrepared = nullptr;
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
ContainerLayerComposite::Prepare(const RenderTargetIntRect& aClipRect)
{
  ContainerPrepare(this, mCompositeManager, aClipRect);
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
RefLayerComposite::Prepare(const RenderTargetIntRect& aClipRect)
{
  ContainerPrepare(this, mCompositeManager, aClipRect);
}


void
RefLayerComposite::CleanupResources()
{
}

} /* layers */
} /* mozilla */
