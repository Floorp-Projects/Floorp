/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContainerLayerComposite.h"
#include <algorithm>                    // for min
#include "apz/src/AsyncPanZoomController.h"  // for AsyncPanZoomController
#include "FrameMetrics.h"               // for FrameMetrics
#include "Units.h"                      // for LayerRect, LayerPixel, etc
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
#include "mozilla/layers/AsyncCompositionManager.h" // for ViewTransform
#include "mozilla/layers/LayerMetricsWrapper.h" // for LayerMetricsWrapper
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "mozilla/nsRefPtr.h"                   // for nsRefPtr
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsISupportsUtils.h"           // for NS_ADDREF, NS_RELEASE
#include "nsRegion.h"                   // for nsIntRegion
#include "nsTArray.h"                   // for nsAutoTArray
#include "TextRenderer.h"               // for TextRenderer
#include <vector>
#include "GeckoProfiler.h"              // for GeckoProfiler
#ifdef MOZ_ENABLE_PROFILER_SPS
#include "ProfilerMarkers.h"            // for ProfilerMarkers
#endif

#define CULLING_LOG(...)
// #define CULLING_LOG(...) printf_stderr("CULLING: " __VA_ARGS__)

#define DUMP(...) do { if (getenv("DUMP_DEBUG")) { printf_stderr(__VA_ARGS__); } } while(0)
#define XYWH(k)  (k).x, (k).y, (k).width, (k).height
#define XY(k)    (k).x, (k).y
#define WH(k)    (k).width, (k).height

namespace mozilla {
namespace layers {

using namespace gfx;

static bool
LayerHasCheckerboardingAPZC(Layer* aLayer, gfxRGBA* aOutColor)
{
  for (LayerMetricsWrapper i(aLayer, LayerMetricsWrapper::StartAt::BOTTOM); i; i = i.GetParent()) {
    if (!i.Metrics().IsScrollable()) {
      continue;
    }
    if (i.GetApzc() && i.GetApzc()->IsCurrentlyCheckerboarding()) {
      if (aOutColor) {
        *aOutColor = i.Metrics().GetBackgroundColor();
      }
      return true;
    }
    break;
  }
  return false;
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

  IntPoint topLeft = visibleRegion.GetBounds().TopLeft();
  aManager->GetTextRenderer()->RenderText(ss.str().c_str(), topLeft,
                                          aLayer->GetEffectiveTransform(), 16,
                                          maxWidth);
}

template<class ContainerT>
static gfx::IntRect ContainerVisibleRect(ContainerT* aContainer)
{
  gfx::IntRect surfaceRect = aContainer->GetEffectiveVisibleRegion().GetBounds();
  return surfaceRect;
}

static void PrintUniformityInfo(Layer* aLayer)
{
#ifdef MOZ_ENABLE_PROFILER_SPS
  if (!profiler_is_active()) {
    return;
  }

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
  LayerTranslationPayload* payload = new LayerTranslationPayload(aLayer, translation);
  PROFILER_MARKER_PAYLOAD("LayerTranslation", payload);
#endif
}

/* all of the per-layer prepared data we need to maintain */
struct PreparedLayer
{
  PreparedLayer(LayerComposite *aLayer, RenderTargetIntRect aClipRect) :
    mLayer(aLayer), mClipRect(aClipRect) {}
  LayerComposite* mLayer;
  RenderTargetIntRect mClipRect;
};


template<class ContainerT> void
ContainerRenderVR(ContainerT* aContainer,
                  LayerManagerComposite* aManager,
                  const gfx::IntRect& aClipRect,
                  gfx::VRHMDInfo* aHMD)
{
  RefPtr<CompositingRenderTarget> surface;

  Compositor* compositor = aManager->GetCompositor();

  RefPtr<CompositingRenderTarget> previousTarget = compositor->GetCurrentRenderTarget();

  float opacity = aContainer->GetEffectiveOpacity();

  // The size of each individual eye surface
  gfx::IntSize eyeResolution = aHMD->SuggestedEyeResolution();
  gfx::IntRect eyeRect[2];
  eyeRect[0] = gfx::IntRect(0, 0, eyeResolution.width, eyeResolution.height);
  eyeRect[1] = gfx::IntRect(eyeResolution.width, 0, eyeResolution.width, eyeResolution.height);

  // The intermediate surface size; we're going to assume that we're not going to run
  // into max texture size limits
  gfx::IntRect surfaceRect = gfx::IntRect(0, 0, eyeResolution.width * 2, eyeResolution.height);

  int32_t maxTextureSize = compositor->GetMaxTextureSize();
  surfaceRect.width = std::min(maxTextureSize, surfaceRect.width);
  surfaceRect.height = std::min(maxTextureSize, surfaceRect.height);

  gfx::VRHMDRenderingSupport *vrRendering = aHMD->GetRenderingSupport();
  if (PR_GetEnv("NO_VR_RENDERING")) vrRendering = nullptr;
  if (vrRendering) {
    if (!aContainer->mVRRenderTargetSet || aContainer->mVRRenderTargetSet->size != surfaceRect.Size()) {
      aContainer->mVRRenderTargetSet = vrRendering->CreateRenderTargetSet(compositor, surfaceRect.Size());
    }

    surface = aContainer->mVRRenderTargetSet->GetNextRenderTarget();
    if (!surface) {
      NS_WARNING("GetNextRenderTarget failed");
      return;
    }
  } else {
    surface = compositor->CreateRenderTarget(surfaceRect, INIT_MODE_CLEAR);
    if (!surface) {
      return;
    }
  }

  gfx::IntRect rtBounds = previousTarget->GetRect();
  DUMP("eyeResolution: %d %d targetRT: %d %d %d %d\n", WH(eyeResolution), XYWH(rtBounds));

  compositor->SetRenderTarget(surface);

  nsAutoTArray<Layer*, 12> children;
  aContainer->SortChildrenBy3DZOrder(children);

  gfx::Matrix4x4 origTransform = aContainer->GetEffectiveTransform();

  for (uint32_t i = 0; i < children.Length(); i++) {
    LayerComposite* layerToRender = static_cast<LayerComposite*>(children.ElementAt(i)->ImplData());
    Layer* layer = layerToRender->GetLayer();
    uint32_t contentFlags = layer->GetContentFlags();

    if (layer->GetEffectiveVisibleRegion().IsEmpty() &&
        !layer->AsContainerLayer()) {
      continue;
    }

    // We flip between pre-rendered and Gecko-rendered VR based on whether
    // the child layer of this VR container layer has PRESERVE_3D or not.
    if ((contentFlags & Layer::CONTENT_PRESERVE_3D) == 0) {
      // This layer is native VR
      DUMP("%p Switching to pre-rendered VR\n", aContainer);

      // XXX we still need depth test here, but we have no way of preserving
      // depth anyway in native VR layers until we have a way to save them
      // from WebGL (and maybe depth video?)
      compositor->SetRenderTarget(surface);
      aContainer->ReplaceEffectiveTransform(origTransform);

      // If this native-VR child layer does not have sizes that match
      // the eye resolution (that is, returned by the recommended
      // render rect from the HMD device), then we need to scale it
      // up/down.
      Rect layerBounds;
      // XXX this is a hack! Canvas layers aren't reporting the
      // proper bounds here (visible region bounds are 0,0,0,0)
      // and I'm not sure if this is the bounds we want anyway.
      if (layer->GetType() == Layer::TYPE_CANVAS) {
        layerBounds = ToRect(static_cast<CanvasLayer*>(layer)->GetBounds());
      } else {
        layerBounds = ToRect(layer->GetEffectiveVisibleRegion().GetBounds());
      }
      const gfx::Matrix4x4 childTransform = layer->GetEffectiveTransform();
      layerBounds = childTransform.TransformBounds(layerBounds);

      DUMP("  layer %p [type %d] bounds [%f %f %f %f] surfaceRect [%d %d %d %d]\n", layer, (int) layer->GetType(),
           XYWH(layerBounds), XYWH(surfaceRect));

      bool restoreTransform = false;
      if ((layerBounds.width != 0 && layerBounds.height != 0) &&
          (layerBounds.width != surfaceRect.width ||
           layerBounds.height != surfaceRect.height))
      {
        DUMP("  layer %p doesn't match, prescaling by %f %f\n", layer,
             surfaceRect.width / float(layerBounds.width),
             surfaceRect.height / float(layerBounds.height));
        gfx::Matrix4x4 scaledChildTransform(childTransform);
        scaledChildTransform.PreScale(surfaceRect.width / layerBounds.width,
                                      surfaceRect.height / layerBounds.height,
                                      1.0f);

        layer->ReplaceEffectiveTransform(scaledChildTransform);
        restoreTransform = true;
      }

      // XXX these are both clip rects, which end up as scissor rects in the compositor.  So we just
      // pass the full target surface rect here.
      layerToRender->Prepare(RenderTargetIntRect(surfaceRect.x, surfaceRect.y,
                                                 surfaceRect.width, surfaceRect.height));
      layerToRender->RenderLayer(surfaceRect);

      if (restoreTransform) {
        layer->ReplaceEffectiveTransform(childTransform);
      }
    } else {
      // Gecko-rendered CSS VR -- not supported yet, so just don't render this layer!
    }
  }

  DUMP(" -- ContainerRenderVR [%p] after child layers\n", aContainer);

  // Now put back the original transfom on this container
  aContainer->ReplaceEffectiveTransform(origTransform);

  // then bind the original target and draw with distortion
  compositor->SetRenderTarget(previousTarget);

  if (vrRendering) {
    vrRendering->SubmitFrame(aContainer->mVRRenderTargetSet);
    DUMP("<<< ContainerRenderVR [used vrRendering] [%p]\n", aContainer);
    if (!gfxPrefs::VRMirrorTextures()) {
      return;
    }
  }

  gfx::Rect rect(surfaceRect.x, surfaceRect.y, surfaceRect.width, surfaceRect.height);
  gfx::Rect clipRect(aClipRect.x, aClipRect.y, aClipRect.width, aClipRect.height);

  // The VR geometry may not cover the entire area; we need to fill with a solid color
  // first.
  // XXX should DrawQuad handle this on its own?  Is there a time where we wouldn't want
  // to do this? (e.g. something like Cardboard would not require distortion so will fill
  // the entire rect)
  EffectChain solidEffect(aContainer);
  solidEffect.mPrimaryEffect = new EffectSolidColor(Color(0.0, 0.0, 0.0, 1.0));
  aManager->GetCompositor()->DrawQuad(rect, rect, solidEffect, 1.0, gfx::Matrix4x4());

  // draw the temporary surface with VR distortion to the original destination
  EffectChain vrEffect(aContainer);
  bool skipDistortion = vrRendering || PR_GetEnv("MOZ_GFX_VR_NO_DISTORTION");
  if (skipDistortion) {
    vrEffect.mPrimaryEffect = new EffectRenderTarget(surface);
  } else {
    vrEffect.mPrimaryEffect = new EffectVRDistortion(aHMD, surface);
  }

  gfx::Matrix4x4 scaleTransform = aContainer->GetEffectiveTransform();
  scaleTransform.PreScale(rtBounds.width / float(surfaceRect.width),
                          rtBounds.height / float(surfaceRect.height),
                          1.0f);

  // XXX we shouldn't use visibleRect here -- the VR distortion needs to know the
  // full rect, not just the visible one.  Luckily, right now, VR distortion is only
  // rendered when the element is fullscreen, so the visibleRect will be right anyway.
  aManager->GetCompositor()->DrawQuad(rect, clipRect, vrEffect, opacity,
                                      scaleTransform);

  DUMP("<<< ContainerRenderVR [%p]\n", aContainer);
}

/* all of the prepared data that we need in RenderLayer() */
struct PreparedData
{
  RefPtr<CompositingRenderTarget> mTmpTarget;
  nsAutoTArray<PreparedLayer, 12> mLayers;
  bool mNeedsSurfaceCopy;
};

// ContainerPrepare is shared between RefLayer and ContainerLayer
template<class ContainerT> void
ContainerPrepare(ContainerT* aContainer,
                 LayerManagerComposite* aManager,
                 const RenderTargetIntRect& aClipRect)
{
  aContainer->mPrepared = MakeUnique<PreparedData>();
  aContainer->mPrepared->mNeedsSurfaceCopy = false;

  gfx::VRHMDInfo *hmdInfo = aContainer->GetVRHMDInfo();
  if (hmdInfo && hmdInfo->GetConfiguration().IsValid()) {
    // we're not going to do anything here; instead, we'll do it all in ContainerRender.
    // XXX fix this; we can win with the same optimizations.  Specifically, we
    // want to render thebes layers only once and then composite the intermeidate surfaces
    // with different transforms twice.
    return;
  }

  /**
   * Determine which layers to draw.
   */
  nsAutoTArray<Layer*, 12> children;
  aContainer->SortChildrenBy3DZOrder(children);

  for (uint32_t i = 0; i < children.Length(); i++) {
    LayerComposite* layerToRender = static_cast<LayerComposite*>(children.ElementAt(i)->ImplData());

    RenderTargetIntRect clipRect = layerToRender->GetLayer()->
        CalculateScissorRect(aClipRect);

    // We don't want to skip container layers because otherwise their mPrepared
    // may be null which is not allowed.
    if (!layerToRender->GetLayer()->AsContainerLayer()) {
      if (layerToRender->GetLayer()->GetEffectiveVisibleRegion().IsEmpty()) {
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
    aContainer->mPrepared->mLayers.AppendElement(PreparedLayer(layerToRender, clipRect));
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
        RenderIntermediate(aContainer, aManager, RenderTargetPixel::ToUntyped(aClipRect), surface);
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
RenderMinimap(ContainerT* aContainer, LayerManagerComposite* aManager,
                   const RenderTargetIntRect& aClipRect, Layer* aLayer)
{
  Compositor* compositor = aManager->GetCompositor();

  if (aLayer->GetFrameMetricsCount() < 1) {
    return;
  }

  AsyncPanZoomController* controller = aLayer->GetAsyncPanZoomController(0);
  if (!controller) {
    return;
  }

  ViewTransform asyncTransformWithoutOverscroll;
  ParentLayerPoint scrollOffset;
  controller->SampleContentTransformForFrame(&asyncTransformWithoutOverscroll,
                                           scrollOffset);

  // Options
  const int verticalPadding = 10;
  const int horizontalPadding = 5;
  gfx::Color backgroundColor(0.3f, 0.3f, 0.3f, 0.3f);
  gfx::Color tileActiveColor(1, 1, 1, 0.5f);
  gfx::Color tileBorderColor(0, 0, 0, 0.1f);
  gfx::Color pageBorderColor(0, 0, 0);
  gfx::Color displayPortColor(0, 1.f, 0);
  gfx::Color viewPortColor(0, 0, 1.f);

  // Rects
  const FrameMetrics& fm = aLayer->GetFrameMetrics(0);
  ParentLayerRect compositionBounds = fm.GetCompositionBounds();
  LayerRect scrollRect = fm.GetScrollableRect() * fm.LayersPixelsPerCSSPixel();
  LayerRect viewRect = ParentLayerRect(scrollOffset, compositionBounds.Size()) / LayerToParentLayerScale(1);
  LayerRect dp = (fm.GetDisplayPort() + fm.GetScrollOffset()) * fm.LayersPixelsPerCSSPixel();

  // Don't render trivial minimap. They can show up from textboxes and other tiny frames.
  if (viewRect.width < 64 && viewRect.height < 64) {
    return;
  }

  // Compute a scale with an appropriate aspect ratio
  // We allocate up to 100px of width and the height of this layer.
  float scaleFactor;
  float scaleFactorX;
  float scaleFactorY;
  scaleFactorX = 100.f / scrollRect.width;
  scaleFactorY = ((viewRect.height) - 2 * verticalPadding) / scrollRect.height;
  scaleFactor = std::min(scaleFactorX, scaleFactorY);

  Matrix4x4 transform = Matrix4x4::Scaling(scaleFactor, scaleFactor, 1);
  transform.PostTranslate(horizontalPadding + compositionBounds.x, verticalPadding + compositionBounds.y, 0);

  Rect clipRect = aContainer->GetEffectiveTransform().TransformBounds(
                    transform.TransformBounds(scrollRect.ToUnknownRect()));
  clipRect.width++;
  clipRect.height++;

  Rect r;
  r = transform.TransformBounds(scrollRect.ToUnknownRect());
  compositor->FillRect(r, backgroundColor, clipRect, aContainer->GetEffectiveTransform());

  /* Disabled because on long pages SlowDrawRect becomes a bottleneck.
  int tileW = gfxPrefs::LayersTileWidth();
  int tileH = gfxPrefs::LayersTileHeight();

  for (int x = scrollRect.x; x < scrollRect.XMost(); x += tileW) {
    for (int y = scrollRect.y; y < scrollRect.YMost(); y += tileH) {
      LayerRect tileRect = LayerRect(x - x % tileW, y - y % tileH, tileW, tileH);
      r = transform.TransformBounds(tileRect.ToUnknownRect());
      if (tileRect.Intersects(dp)) {
        compositor->FillRect(r, tileActiveColor, clipRect, aContainer->GetEffectiveTransform());
      }
      compositor->SlowDrawRect(r, tileBorderColor, clipRect, aContainer->GetEffectiveTransform());
    }
  }
  */

  r = transform.TransformBounds(scrollRect.ToUnknownRect());
  compositor->SlowDrawRect(r, pageBorderColor, clipRect, aContainer->GetEffectiveTransform());
  r = transform.TransformBounds(dp.ToUnknownRect());
  compositor->FillRect(r, tileActiveColor, clipRect, aContainer->GetEffectiveTransform());
  r = transform.TransformBounds(dp.ToUnknownRect());
  compositor->SlowDrawRect(r, displayPortColor, clipRect, aContainer->GetEffectiveTransform());
  r = transform.TransformBounds(viewRect.ToUnknownRect());
  compositor->SlowDrawRect(r, viewPortColor, clipRect, aContainer->GetEffectiveTransform(), 2);
}


template<class ContainerT> void
RenderLayers(ContainerT* aContainer,
	     LayerManagerComposite* aManager,
	     const RenderTargetIntRect& aClipRect)
{
  Compositor* compositor = aManager->GetCompositor();

  for (size_t i = 0u; i < aContainer->mPrepared->mLayers.Length(); i++) {
    PreparedLayer& preparedData = aContainer->mPrepared->mLayers[i];
    LayerComposite* layerToRender = preparedData.mLayer;
    const RenderTargetIntRect& clipRect = preparedData.mClipRect;
    Layer* layer = layerToRender->GetLayer();

    gfxRGBA color;
    if ((layer->GetContentFlags() & Layer::CONTENT_OPAQUE) &&
        LayerHasCheckerboardingAPZC(layer, &color)) {
      if (gfxPrefs::APZHighlightCheckerboardedAreas()) {
        color = gfxRGBA(255 / 255.0, 188 / 255.0, 217 / 255.0, 1);  // "Cotton Candy"
      }
      // Ideally we would want to intersect the checkerboard region from the APZ with the layer bounds
      // and only fill in that area. However the layer bounds takes into account the base translation
      // for the painted layer whereas the checkerboard region does not. One does not simply
      // intersect areas in different coordinate spaces. So we do this a little more permissively
      // and only fill in the background when we know there is checkerboard, which in theory
      // should only occur transiently.
      gfx::IntRect layerBounds = layer->GetLayerBounds();
      EffectChain effectChain(layer);
      effectChain.mPrimaryEffect = new EffectSolidColor(ToColor(color));
      aManager->GetCompositor()->DrawQuad(gfx::Rect(layerBounds.x, layerBounds.y, layerBounds.width, layerBounds.height),
                                          gfx::Rect(clipRect.ToUnknownRect()),
                                          effectChain, layer->GetEffectiveOpacity(),
                                          layer->GetEffectiveTransform());
    }

    if (layerToRender->HasLayerBeenComposited()) {
      // Composer2D will compose this layer so skip GPU composition
      // this time & reset composition flag for next composition phase
      layerToRender->SetLayerComposited(false);
      gfx::IntRect clearRect = layerToRender->GetClearRect();
      if (!clearRect.IsEmpty()) {
        // Clear layer's visible rect on FrameBuffer with transparent pixels
        gfx::Rect fbRect(clearRect.x, clearRect.y, clearRect.width, clearRect.height);
        compositor->ClearRect(fbRect);
        layerToRender->SetClearRect(gfx::IntRect(0, 0, 0, 0));
      }
    } else {
      layerToRender->RenderLayer(RenderTargetPixel::ToUntyped(clipRect));
    }

    if (gfxPrefs::UniformityInfo()) {
      PrintUniformityInfo(layer);
    }

    if (gfxPrefs::DrawLayerInfo()) {
      DrawLayerInfo(clipRect, aManager, layer);
    }

    // Draw a border around scrollable layers.
    // A layer can be scrolled by multiple scroll frames. Draw a border
    // for each.
    // Within the list of scroll frames for a layer, the layer border for a
    // scroll frame lower down is affected by the async transforms on scroll
    // frames higher up, so loop from the top down, and accumulate an async
    // transform as we go along.
    Matrix4x4 asyncTransform;
    for (uint32_t i = layer->GetFrameMetricsCount(); i > 0; --i) {
      if (layer->GetFrameMetrics(i - 1).IsScrollable()) {
        // Since the composition bounds are in the parent layer's coordinates,
        // use the parent's effective transform rather than the layer's own.
        ParentLayerRect compositionBounds = layer->GetFrameMetrics(i - 1).GetCompositionBounds();
        aManager->GetCompositor()->DrawDiagnostics(DiagnosticFlags::CONTAINER,
                                                   compositionBounds.ToUnknownRect(),
                                                   gfx::Rect(aClipRect.ToUnknownRect()),
                                                   asyncTransform * aContainer->GetEffectiveTransform());
        if (AsyncPanZoomController* apzc = layer->GetAsyncPanZoomController(i - 1)) {
          asyncTransform = apzc->GetCurrentAsyncTransformWithOverscroll()
                         * asyncTransform;
        }
      }
    }

    if (gfxPrefs::APZMinimap()) {
      RenderMinimap(aContainer, aManager, aClipRect, layer);
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
  if (aContainer->GetEffectiveVisibleRegion().GetNumRects() == 1 &&
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
  gfx::IntRect visibleRect = aContainer->GetEffectiveVisibleRegion().GetBounds();
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
  RenderLayers(aContainer, aManager, RenderTargetPixel::FromUntyped(aClipRect));
  // Unbind the current surface and rebind the previous one.
  compositor->SetRenderTarget(previousTarget);
}

template<class ContainerT> void
ContainerRender(ContainerT* aContainer,
                 LayerManagerComposite* aManager,
                 const gfx::IntRect& aClipRect)
{
  MOZ_ASSERT(aContainer->mPrepared);

  gfx::VRHMDInfo *hmdInfo = aContainer->GetVRHMDInfo();
  if (hmdInfo && hmdInfo->GetConfiguration().IsValid()) {
    ContainerRenderVR(aContainer, aManager, aClipRect, hmdInfo);
    aContainer->mPrepared = nullptr;
    return;
  }

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
      aContainer->mPrepared = nullptr;
      return;
    }

    gfx::Rect visibleRect(aContainer->GetEffectiveVisibleRegion().GetBounds());
    nsRefPtr<Compositor> compositor = aManager->GetCompositor();
#ifdef MOZ_DUMP_PAINTING
    if (gfxUtils::sDumpPainting) {
      RefPtr<gfx::DataSourceSurface> surf = surface->Dump(compositor);
      if (surf) {
        WriteSnapshotToDumpFile(aContainer, surf);
      }
    }
#endif

    nsRefPtr<ContainerT> container = aContainer;
    RenderWithAllMasks(aContainer, compositor, aClipRect,
                       [&, surface, compositor, container](EffectChain& effectChain, const Rect& clipRect) {
      effectChain.mPrimaryEffect = new EffectRenderTarget(surface);
      compositor->DrawQuad(visibleRect, clipRect, effectChain,
                           container->GetEffectiveOpacity(),
                           container->GetEffectiveTransform());
    });
  } else {
    RenderLayers(aContainer, aManager, RenderTargetPixel::FromUntyped(aClipRect));
  }
  aContainer->mPrepared = nullptr;

  // If it is a scrollable container layer with no child layers, and one of the APZCs
  // attached to it has a nonempty async transform, then that transform is not applied
  // to any visible content. Display a warning box (conditioned on the FPS display being
  // enabled).
  if (gfxPrefs::LayersDrawFPS() && aContainer->IsScrollInfoLayer()) {
    // Since aContainer doesn't have any children we can just iterate from the top metrics
    // on it down to the bottom using GetFirstChild and not worry about walking onto another
    // underlying layer.
    for (LayerMetricsWrapper i(aContainer); i; i = i.GetFirstChild()) {
      if (AsyncPanZoomController* apzc = i.GetApzc()) {
        if (!apzc->GetAsyncTransformAppliedToContent()
            && !Matrix4x4(apzc->GetCurrentAsyncTransform()).IsIdentity()) {
          aManager->UnusedApzTransformWarning();
          break;
        }
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
ContainerLayerComposite::RenderLayer(const gfx::IntRect& aClipRect)
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
  mLastIntermediateSurface = nullptr;

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
RefLayerComposite::RenderLayer(const gfx::IntRect& aClipRect)
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
  mLastIntermediateSurface = nullptr;
}

} // namespace layers
} // namespace mozilla
