/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContainerLayerComposite.h"
#include <algorithm>                    // for min
#include "FrameMetrics.h"               // for FrameMetrics
#include "Units.h"                      // for LayerRect, LayerPixel, etc
#include "CompositableHost.h"           // for CompositableHost
#include "gfxEnv.h"                     // for gfxEnv
#include "gfxPrefs.h"                   // for gfxPrefs
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/UniquePtr.h"          // for UniquePtr
#include "mozilla/gfx/BaseRect.h"       // for BaseRect
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/gfx/Point.h"          // for Point, IntPoint
#include "mozilla/gfx/Rect.h"           // for IntRect, Rect
#include "mozilla/layers/APZSampler.h"  // for APZSampler
#include "mozilla/layers/Compositor.h"  // for Compositor, etc
#include "mozilla/layers/CompositorTypes.h"  // for DiagnosticFlags::CONTAINER
#include "mozilla/layers/Effects.h"     // for Effect, EffectChain, etc
#include "mozilla/layers/TextureHost.h"  // for CompositingRenderTarget
#include "mozilla/layers/AsyncCompositionManager.h" // for ViewTransform
#include "mozilla/layers/LayerMetricsWrapper.h" // for LayerMetricsWrapper
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "mozilla/RefPtr.h"                   // for nsRefPtr
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsISupportsUtils.h"           // for NS_ADDREF, NS_RELEASE
#include "nsRegion.h"                   // for nsIntRegion
#include "nsTArray.h"                   // for AutoTArray
#include <stack>
#include "TextRenderer.h"               // for TextRenderer
#include <vector>
#include "GeckoProfiler.h"              // for GeckoProfiler

#ifdef MOZ_GECKO_PROFILER
#include "ProfilerMarkerPayload.h"      // for LayerTranslationMarkerPayload
#endif

#define CULLING_LOG(...)
// #define CULLING_LOG(...) printf_stderr("CULLING: " __VA_ARGS__)

#define DUMP(...) do { if (gfxEnv::DumpDebug()) { printf_stderr(__VA_ARGS__); } } while(0)
#define XYWH(k)  (k).X(), (k).Y(), (k).Width(), (k).Height()
#define XY(k)    (k).X(), (k).Y()
#define WH(k)    (k).Width(), (k).Height()

namespace mozilla {
namespace layers {

using namespace gfx;

static void
DrawLayerInfo(const RenderTargetIntRect& aClipRect,
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

  LayerIntRegion visibleRegion = aLayer->GetVisibleRegion();

  uint32_t maxWidth = std::min<uint32_t>(visibleRegion.GetBounds().Width(), 500);

  IntPoint topLeft = visibleRegion.ToUnknownRegion().GetBounds().TopLeft();
  aManager->GetTextRenderer()->RenderText(
    aManager->GetCompositor(),
    ss.str().c_str(),
    topLeft,
    aLayer->GetEffectiveTransform(), 16,
    maxWidth);
}

static void
PrintUniformityInfo(Layer* aLayer)
{
#if defined(MOZ_GECKO_PROFILER)
  if (!profiler_is_active()) {
    return;
  }

  // Don't want to print a log for smaller layers
  if (aLayer->GetLocalVisibleRegion().GetBounds().Width() < 300 ||
      aLayer->GetLocalVisibleRegion().GetBounds().Height() < 300) {
    return;
  }

  Matrix4x4 transform = aLayer->AsHostLayer()->GetShadowBaseTransform();
  if (!transform.Is2D()) {
    return;
  }

  Point translation = transform.As2D().GetTranslation();
  profiler_add_marker(
    "LayerTranslation",
    MakeUnique<LayerTranslationMarkerPayload>(aLayer, translation,
                                              TimeStamp::Now()));
#endif
}

static Maybe<gfx::Polygon>
SelectLayerGeometry(const Maybe<gfx::Polygon>& aParentGeometry,
                    const Maybe<gfx::Polygon>& aChildGeometry)
{
  // Both the parent and the child layer were split.
  if (aParentGeometry && aChildGeometry) {
    return Some(aParentGeometry->ClipPolygon(*aChildGeometry));
  }

  // The parent layer was split.
  if (aParentGeometry) {
    return aParentGeometry;
  }

  // The child layer was split.
  if(aChildGeometry) {
    return aChildGeometry;
  }

  // No split.
  return Nothing();
}

void
TransformLayerGeometry(Layer* aLayer, Maybe<gfx::Polygon>& aGeometry)
{
  Layer* parent = aLayer;
  gfx::Matrix4x4 transform;

  // Collect all parent transforms.
  while (parent != nullptr && !parent->Is3DContextLeaf()) {
    transform = transform * parent->GetLocalTransform();
    parent = parent->GetParent();
  }

  // Transform the geometry to the parent 3D context leaf coordinate space.
  transform = transform.ProjectTo2D();

  if (!transform.IsSingular()) {
    aGeometry->TransformToScreenSpace(transform.Inverse());
  } else {
    // Discard the geometry since the result might not be correct.
    aGeometry.reset();
  }
}


template<class ContainerT>
static gfx::IntRect ContainerVisibleRect(ContainerT* aContainer)
{
  gfx::IntRect surfaceRect = aContainer->GetLocalVisibleRegion().ToUnknownRegion().GetBounds();
  return surfaceRect;
}


/* all of the per-layer prepared data we need to maintain */
struct PreparedLayer
{
  PreparedLayer(Layer *aLayer,
                RenderTargetIntRect aClipRect,
                Maybe<gfx::Polygon>&& aGeometry)
  : mLayer(aLayer), mClipRect(aClipRect), mGeometry(std::move(aGeometry)) {}

  RefPtr<Layer> mLayer;
  RenderTargetIntRect mClipRect;
  Maybe<Polygon> mGeometry;
};

/* all of the prepared data that we need in RenderLayer() */
struct PreparedData
{
  RefPtr<CompositingRenderTarget> mTmpTarget;
  AutoTArray<PreparedLayer, 12> mLayers;
  bool mNeedsSurfaceCopy;
};

// ContainerPrepare is shared between RefLayer and ContainerLayer
template<class ContainerT> void
ContainerPrepare(ContainerT* aContainer,
                 LayerManagerComposite* aManager,
                 const RenderTargetIntRect& aClipRect)
{
  // We can end up calling prepare multiple times if we duplicated
  // layers due to preserve-3d plane splitting. The results
  // should be identical, so we only need to do it once.
  if (aContainer->mPrepared) {
    return;
  }
  aContainer->mPrepared = MakeUnique<PreparedData>();
  aContainer->mPrepared->mNeedsSurfaceCopy = false;

  const ContainerLayerComposite::SortMode sortMode =
    aManager->GetCompositor()->SupportsLayerGeometry()
    ? ContainerLayerComposite::SortMode::WITH_GEOMETRY
    : ContainerLayerComposite::SortMode::WITHOUT_GEOMETRY;

  nsTArray<LayerPolygon> polygons =
    aContainer->SortChildrenBy3DZOrder(sortMode);

  for (LayerPolygon& layer : polygons) {
    LayerComposite* layerToRender =
      static_cast<LayerComposite*>(layer.layer->ImplData());

    RenderTargetIntRect clipRect =
      layerToRender->GetLayer()->CalculateScissorRect(aClipRect);

    if (layerToRender->GetLayer()->IsBackfaceHidden()) {
      continue;
    }

    // We don't want to skip container layers because otherwise their mPrepared
    // may be null which is not allowed.
    if (!layerToRender->GetLayer()->AsContainerLayer()) {
      if (!layerToRender->GetLayer()->IsVisible()) {
        CULLING_LOG("Sublayer %p has no effective visible region\n", layerToRender->GetLayer());
        continue;
      }

      if (clipRect.IsEmpty()) {
        CULLING_LOG("Sublayer %p has an empty world clip rect\n", layerToRender->GetLayer());
        continue;
      }
    }

    CULLING_LOG("Preparing sublayer %p\n", layerToRender->GetLayer());

    layerToRender->Prepare(clipRect);
    aContainer->mPrepared->mLayers.AppendElement(PreparedLayer(layerToRender->GetLayer(),
                                                               clipRect,
                                                               std::move(layer.geometry)));
  }

  CULLING_LOG("Preparing container layer %p\n", aContainer->GetLayer());

  /**
   * Setup our temporary surface for rendering the contents of this container.
   */

  gfx::IntRect surfaceRect = ContainerVisibleRect(aContainer);
  if (surfaceRect.IsEmpty()) {
    return;
  }

  bool surfaceCopyNeeded;
  // DefaultComputeSupportsComponentAlphaChildren can mutate aContainer so call it unconditionally
  aContainer->DefaultComputeSupportsComponentAlphaChildren(&surfaceCopyNeeded);
  if (aContainer->UseIntermediateSurface()) {
    if (!surfaceCopyNeeded) {
      RefPtr<CompositingRenderTarget> surface = nullptr;

      RefPtr<CompositingRenderTarget>& lastSurf = aContainer->mLastIntermediateSurface;
      if (lastSurf && !aContainer->mChildrenChanged && lastSurf->GetRect().IsEqualEdges(surfaceRect)) {
        surface = lastSurf;
      }

      if (!surface) {
        // If we don't need a copy we can render to the intermediate now to avoid
        // unecessary render target switching. This brings a big perf boost on mobile gpus.
        surface = CreateOrRecycleTarget(aContainer, aManager);

        MOZ_PERFORMANCE_WARNING("gfx", "[%p] Container layer requires intermediate surface rendering\n", aContainer);
        RenderIntermediate(aContainer, aManager, aClipRect.ToUnknownRect(), surface);
        aContainer->SetChildrenChanged(false);
      }

      aContainer->mPrepared->mTmpTarget = surface;
    } else {
      MOZ_PERFORMANCE_WARNING("gfx", "[%p] Container layer requires intermediate surface copy\n", aContainer);
      aContainer->mPrepared->mNeedsSurfaceCopy = true;
      aContainer->mLastIntermediateSurface = nullptr;
    }
  } else {
    aContainer->mLastIntermediateSurface = nullptr;
  }
}

template<class ContainerT> void
RenderMinimap(ContainerT* aContainer,
              const RefPtr<APZSampler>& aSampler,
              LayerManagerComposite* aManager,
              const RenderTargetIntRect& aClipRect, Layer* aLayer)
{
  Compositor* compositor = aManager->GetCompositor();
  MOZ_ASSERT(aSampler);

  if (aLayer->GetScrollMetadataCount() < 1) {
    return;
  }

  LayerMetricsWrapper wrapper(aLayer, 0);
  if (!wrapper.GetApzc()) {
    return;
  }
  const FrameMetrics& fm = wrapper.Metrics();
  MOZ_ASSERT(fm.IsScrollable());

  ParentLayerPoint scrollOffset = aSampler->GetCurrentAsyncScrollOffset(wrapper);

  // Options
  const int verticalPadding = 10;
  const int horizontalPadding = 5;
  gfx::Color backgroundColor(0.3f, 0.3f, 0.3f, 0.3f);
  gfx::Color tileActiveColor(1, 1, 1, 0.4f);
  gfx::Color tileBorderColor(0, 0, 0, 0.1f);
  gfx::Color pageBorderColor(0, 0, 0);
  gfx::Color criticalDisplayPortColor(1.f, 1.f, 0);
  gfx::Color displayPortColor(0, 1.f, 0);
  gfx::Color viewPortColor(0, 0, 1.f, 0.3f);

  // Rects
  ParentLayerRect compositionBounds = fm.GetCompositionBounds();
  LayerRect scrollRect = fm.GetScrollableRect() * fm.LayersPixelsPerCSSPixel();
  LayerRect viewRect = ParentLayerRect(scrollOffset, compositionBounds.Size()) / LayerToParentLayerScale(1);
  LayerRect dp = (fm.GetDisplayPort() + fm.GetScrollOffset()) * fm.LayersPixelsPerCSSPixel();
  Maybe<LayerRect> cdp;
  if (!fm.GetCriticalDisplayPort().IsEmpty()) {
    cdp = Some((fm.GetCriticalDisplayPort() + fm.GetScrollOffset()) * fm.LayersPixelsPerCSSPixel());
  }

  // Don't render trivial minimap. They can show up from textboxes and other tiny frames.
  if (viewRect.Width() < 64 && viewRect.Height() < 64) {
    return;
  }

  // Compute a scale with an appropriate aspect ratio
  // We allocate up to 100px of width and the height of this layer.
  float scaleFactor;
  float scaleFactorX;
  float scaleFactorY;
  Rect dest = Rect(aClipRect.ToUnknownRect());
  if (aLayer->GetLocalClipRect()) {
    dest = Rect(aLayer->GetLocalClipRect().value().ToUnknownRect());
  } else {
    dest = aContainer->GetEffectiveTransform().Inverse().TransformBounds(dest);
  }
  dest = dest.Intersect(compositionBounds.ToUnknownRect());
  scaleFactorX = std::min(100.f, dest.Width() - (2 * horizontalPadding)) / scrollRect.Width();
  scaleFactorY = (dest.Height() - (2 * verticalPadding)) / scrollRect.Height();
  scaleFactor = std::min(scaleFactorX, scaleFactorY);
  if (scaleFactor <= 0) {
    return;
  }

  Matrix4x4 transform = Matrix4x4::Scaling(scaleFactor, scaleFactor, 1);
  transform.PostTranslate(horizontalPadding + dest.X(), verticalPadding + dest.Y(), 0);

  Rect transformedScrollRect = transform.TransformBounds(scrollRect.ToUnknownRect());

  IntRect clipRect = RoundedOut(aContainer->GetEffectiveTransform().TransformBounds(transformedScrollRect));

  // Render the scrollable area.
  compositor->FillRect(transformedScrollRect, backgroundColor, clipRect, aContainer->GetEffectiveTransform());
  compositor->SlowDrawRect(transformedScrollRect, pageBorderColor, clipRect, aContainer->GetEffectiveTransform());

  // Render the displayport.
  Rect r = transform.TransformBounds(dp.ToUnknownRect());
  compositor->FillRect(r, tileActiveColor, clipRect, aContainer->GetEffectiveTransform());
  compositor->SlowDrawRect(r, displayPortColor, clipRect, aContainer->GetEffectiveTransform());

  // Render the critical displayport if there is one
  if (cdp) {
    r = transform.TransformBounds(cdp->ToUnknownRect());
    compositor->SlowDrawRect(r, criticalDisplayPortColor, clipRect, aContainer->GetEffectiveTransform());
  }

  // Render the viewport.
  r = transform.TransformBounds(viewRect.ToUnknownRect());
  compositor->SlowDrawRect(r, viewPortColor, clipRect, aContainer->GetEffectiveTransform(), 2);
}

template<class ContainerT> void
RenderLayers(ContainerT* aContainer, LayerManagerComposite* aManager,
             const RenderTargetIntRect& aClipRect,
             const Maybe<gfx::Polygon>& aGeometry)
{
  Compositor* compositor = aManager->GetCompositor();

  RefPtr<APZSampler> sampler;
  if (CompositorBridgeParent* cbp = compositor->GetCompositorBridgeParent()) {
    sampler = cbp->GetAPZSampler();
  }

  for (size_t i = 0u; i < aContainer->mPrepared->mLayers.Length(); i++) {
    PreparedLayer& preparedData = aContainer->mPrepared->mLayers[i];

    const gfx::IntRect clipRect = preparedData.mClipRect.ToUnknownRect();
    LayerComposite* layerToRender = static_cast<LayerComposite*>(preparedData.mLayer->ImplData());
    const Maybe<gfx::Polygon>& childGeometry = preparedData.mGeometry;

    Layer* layer = layerToRender->GetLayer();

    if (layerToRender->HasStaleCompositor()) {
      continue;
    }

    if (gfxPrefs::LayersDrawFPS()) {
      for (const auto& metadata : layer->GetAllScrollMetadata()) {
        if (metadata.IsApzForceDisabled()) {
          aManager->DisabledApzWarning();
          break;
        }
      }
    }

    if (layerToRender->HasLayerBeenComposited()) {
      // Composer2D will compose this layer so skip GPU composition
      // this time. The flag will be reset for the next composition phase
      // at the beginning of LayerManagerComposite::Rener().
      gfx::IntRect clearRect = layerToRender->GetClearRect();
      if (!clearRect.IsEmpty()) {
        // Clear layer's visible rect on FrameBuffer with transparent pixels
        gfx::Rect fbRect(clearRect.X(), clearRect.Y(), clearRect.Width(), clearRect.Height());
        compositor->ClearRect(fbRect);
        layerToRender->SetClearRect(gfx::IntRect(0, 0, 0, 0));
      }
    } else {
      // Since we force an intermediate surface for nested 3D contexts,
      // aGeometry and childGeometry are both in the same coordinate space.
      Maybe<gfx::Polygon> geometry =
        SelectLayerGeometry(aGeometry, childGeometry);

      // If we are dealing with a nested 3D context, we might need to transform
      // the geometry back to the coordinate space of the current layer before
      // rendering the layer.
      ContainerLayer* container = layer->AsContainerLayer();
      const bool isLeafLayer = !container || container->UseIntermediateSurface();

      if (geometry && isLeafLayer) {
        TransformLayerGeometry(layer, geometry);
      }

      layerToRender->RenderLayer(clipRect, geometry);
    }

    if (gfxPrefs::UniformityInfo()) {
      PrintUniformityInfo(layer);
    }

    if (gfxPrefs::DrawLayerInfo()) {
      DrawLayerInfo(preparedData.mClipRect, aManager, layer);
    }

    // Draw a border around scrollable layers.
    // A layer can be scrolled by multiple scroll frames. Draw a border
    // for each.
    // Within the list of scroll frames for a layer, the layer border for a
    // scroll frame lower down is affected by the async transforms on scroll
    // frames higher up, so loop from the top down, and accumulate an async
    // transform as we go along.
    Matrix4x4 asyncTransform;
    if (sampler) {
      for (uint32_t i = layer->GetScrollMetadataCount(); i > 0; --i) {
        LayerMetricsWrapper wrapper(layer, i - 1);
        if (wrapper.GetApzc()) {
          MOZ_ASSERT(wrapper.Metrics().IsScrollable());
          // Since the composition bounds are in the parent layer's coordinates,
          // use the parent's effective transform rather than the layer's own.
          ParentLayerRect compositionBounds = wrapper.Metrics().GetCompositionBounds();
          aManager->GetCompositor()->DrawDiagnostics(DiagnosticFlags::CONTAINER,
                                                     compositionBounds.ToUnknownRect(),
                                                     aClipRect.ToUnknownRect(),
                                                     asyncTransform * aContainer->GetEffectiveTransform());
          asyncTransform =
              sampler->GetCurrentAsyncTransformWithOverscroll(wrapper).ToUnknownMatrix()
              * asyncTransform;
        }
      }

      if (gfxPrefs::APZMinimap()) {
        RenderMinimap(aContainer, sampler, aManager, aClipRect, layer);
      }
    }

    // invariant: our GL context should be current here, I don't think we can
    // assert it though
  }
}

template<class ContainerT> RefPtr<CompositingRenderTarget>
CreateOrRecycleTarget(ContainerT* aContainer,
                      LayerManagerComposite* aManager)
{
  Compositor* compositor = aManager->GetCompositor();
  SurfaceInitMode mode = INIT_MODE_CLEAR;
  gfx::IntRect surfaceRect = ContainerVisibleRect(aContainer);
  if (aContainer->GetLocalVisibleRegion().GetNumRects() == 1 &&
      (aContainer->GetContentFlags() & Layer::CONTENT_OPAQUE))
  {
    mode = INIT_MODE_NONE;
  }

  RefPtr<CompositingRenderTarget>& lastSurf = aContainer->mLastIntermediateSurface;
  if (lastSurf && lastSurf->GetRect().IsEqualEdges(surfaceRect)) {
    if (mode == INIT_MODE_CLEAR) {
      lastSurf->ClearOnBind();
    }

    return lastSurf;
  } else {
    lastSurf = compositor->CreateRenderTarget(surfaceRect, mode);

    return lastSurf;
  }
}

template<class ContainerT> RefPtr<CompositingRenderTarget>
CreateTemporaryTargetAndCopyFromBackground(ContainerT* aContainer,
                                           LayerManagerComposite* aManager)
{
  Compositor* compositor = aManager->GetCompositor();
  gfx::IntRect visibleRect = aContainer->GetLocalVisibleRegion().ToUnknownRegion().GetBounds();
  RefPtr<CompositingRenderTarget> previousTarget = compositor->GetCurrentRenderTarget();
  gfx::IntRect surfaceRect = gfx::IntRect(visibleRect.X(), visibleRect.Y(),
                                          visibleRect.Width(), visibleRect.Height());

  gfx::IntPoint sourcePoint = gfx::IntPoint(visibleRect.X(), visibleRect.Y());

  gfx::Matrix4x4 transform = aContainer->GetEffectiveTransform();
  DebugOnly<gfx::Matrix> transform2d;
  MOZ_ASSERT(transform.Is2D(&transform2d) && !gfx::ThebesMatrix(transform2d).HasNonIntegerTranslation());
  sourcePoint += gfx::IntPoint::Truncate(transform._41, transform._42);

  sourcePoint -= compositor->GetCurrentRenderTarget()->GetOrigin();

  return compositor->CreateRenderTargetFromSource(surfaceRect, previousTarget, sourcePoint);
}

template<class ContainerT> void
RenderIntermediate(ContainerT* aContainer,
                   LayerManagerComposite* aManager,
                   const gfx::IntRect& aClipRect,
                   RefPtr<CompositingRenderTarget> surface)
{
  Compositor* compositor = aManager->GetCompositor();
  RefPtr<CompositingRenderTarget> previousTarget = compositor->GetCurrentRenderTarget();

  if (!surface) {
    return;
  }

  compositor->SetRenderTarget(surface);
  // pre-render all of the layers into our temporary
  RenderLayers(aContainer, aManager,
               RenderTargetIntRect::FromUnknownRect(aClipRect),
               Nothing());

  // Unbind the current surface and rebind the previous one.
  compositor->SetRenderTarget(previousTarget);
}

template<class ContainerT> void
ContainerRender(ContainerT* aContainer,
                 LayerManagerComposite* aManager,
                 const gfx::IntRect& aClipRect,
                 const Maybe<gfx::Polygon>& aGeometry)
{
  MOZ_ASSERT(aContainer->mPrepared);

  if (aContainer->UseIntermediateSurface()) {
    RefPtr<CompositingRenderTarget> surface;

    if (aContainer->mPrepared->mNeedsSurfaceCopy) {
      // we needed to copy the background so we waited until now to render the intermediate
      surface = CreateTemporaryTargetAndCopyFromBackground(aContainer, aManager);
      RenderIntermediate(aContainer, aManager,
                         aClipRect, surface);
    } else {
      surface = aContainer->mPrepared->mTmpTarget;
    }

    if (!surface) {
      return;
    }

    gfx::Rect visibleRect(aContainer->GetLocalVisibleRegion().ToUnknownRegion().GetBounds());

    RefPtr<Compositor> compositor = aManager->GetCompositor();
#ifdef MOZ_DUMP_PAINTING
    if (gfxEnv::DumpCompositorTextures()) {
      RefPtr<gfx::DataSourceSurface> surf = surface->Dump(compositor);
      if (surf) {
        WriteSnapshotToDumpFile(aContainer, surf);
      }
    }
#endif

    RefPtr<ContainerT> container = aContainer;
    RenderWithAllMasks(aContainer, compositor, aClipRect,
                       [&, surface, compositor, container](EffectChain& effectChain, const IntRect& clipRect) {
      effectChain.mPrimaryEffect = new EffectRenderTarget(surface);

      compositor->DrawGeometry(visibleRect, clipRect, effectChain,
                               container->GetEffectiveOpacity(),
                               container->GetEffectiveTransform(), aGeometry);
    });

  } else {
    RenderLayers(aContainer, aManager,
                 RenderTargetIntRect::FromUnknownRect(aClipRect),
                 aGeometry);
  }

  // If it is a scrollable container layer with no child layers, and one of the APZCs
  // attached to it has a nonempty async transform, then that transform is not applied
  // to any visible content. Display a warning box (conditioned on the FPS display being
  // enabled).
  if (gfxPrefs::LayersDrawFPS() && aContainer->IsScrollableWithoutContent()) {
    RefPtr<APZSampler> sampler = aManager->GetCompositor()->GetCompositorBridgeParent()->GetAPZSampler();
    // Since aContainer doesn't have any children we can just iterate from the top metrics
    // on it down to the bottom using GetFirstChild and not worry about walking onto another
    // underlying layer.
    for (LayerMetricsWrapper i(aContainer); i; i = i.GetFirstChild()) {
      if (sampler->HasUnusedAsyncTransform(i)) {
        aManager->UnusedApzTransformWarning();
        break;
      }
    }
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
  RemoveAllChildren();
}

void
ContainerLayerComposite::Destroy()
{
  if (!mDestroyed) {
    while (mFirstChild) {
      GetFirstChildComposite()->Destroy();
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
  return static_cast<LayerComposite*>(mFirstChild->AsHostLayer());
}

void
ContainerLayerComposite::Cleanup()
{
  mPrepared = nullptr;

  for (Layer* l = GetFirstChild(); l; l = l->GetNextSibling()) {
    static_cast<LayerComposite*>(l->AsHostLayer())->Cleanup();
  }
}

void
ContainerLayerComposite::RenderLayer(const gfx::IntRect& aClipRect,
                                     const Maybe<gfx::Polygon>& aGeometry)
{
  ContainerRender(this, mCompositeManager, aClipRect, aGeometry);
}

void
ContainerLayerComposite::Prepare(const RenderTargetIntRect& aClipRect)
{
  ContainerPrepare(this, mCompositeManager, aClipRect);
}

void
ContainerLayerComposite::CleanupResources()
{
  mLastIntermediateSurface = nullptr;
  mPrepared = nullptr;

  for (Layer* l = GetFirstChild(); l; l = l->GetNextSibling()) {
    static_cast<LayerComposite*>(l->AsHostLayer())->CleanupResources();
  }
}

const LayerIntRegion&
ContainerLayerComposite::GetShadowVisibleRegion()
{
  if (!UseIntermediateSurface()) {
    RecomputeShadowVisibleRegionFromChildren();
  }

  return mShadowVisibleRegion;
}

const LayerIntRegion&
RefLayerComposite::GetShadowVisibleRegion()
{
  if (!UseIntermediateSurface()) {
    RecomputeShadowVisibleRegionFromChildren();
  }

  return mShadowVisibleRegion;
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
  return static_cast<LayerComposite*>(mFirstChild->AsHostLayer());
}

void
RefLayerComposite::RenderLayer(const gfx::IntRect& aClipRect,
                               const Maybe<gfx::Polygon>& aGeometry)
{
  ContainerRender(this, mCompositeManager, aClipRect, aGeometry);
}

void
RefLayerComposite::Prepare(const RenderTargetIntRect& aClipRect)
{
  ContainerPrepare(this, mCompositeManager, aClipRect);
}

void
RefLayerComposite::Cleanup()
{
  mPrepared = nullptr;

  for (Layer* l = GetFirstChild(); l; l = l->GetNextSibling()) {
    static_cast<LayerComposite*>(l->AsHostLayer())->Cleanup();
  }
}

void
RefLayerComposite::CleanupResources()
{
  mLastIntermediateSurface = nullptr;
  mPrepared = nullptr;
}

} // namespace layers
} // namespace mozilla
