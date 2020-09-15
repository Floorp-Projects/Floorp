/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayerManagerComposite.h"
#include <stddef.h>                   // for size_t
#include <stdint.h>                   // for uint16_t, uint32_t
#include "CanvasLayerComposite.h"     // for CanvasLayerComposite
#include "ColorLayerComposite.h"      // for ColorLayerComposite
#include "CompositableHost.h"         // for CompositableHost
#include "ContainerLayerComposite.h"  // for ContainerLayerComposite, etc
#include "Diagnostics.h"
#include "FPSCounter.h"                    // for FPSState, FPSCounter
#include "FrameMetrics.h"                  // for FrameMetrics
#include "GeckoProfiler.h"                 // for profiler_*
#include "ImageLayerComposite.h"           // for ImageLayerComposite
#include "Layers.h"                        // for Layer, ContainerLayer, etc
#include "LayerScope.h"                    // for LayerScope Tool
#include "protobuf/LayerScopePacket.pb.h"  // for protobuf (LayerScope)
#include "PaintedLayerComposite.h"         // for PaintedLayerComposite
#include "TiledContentHost.h"
#include "Units.h"                           // for ScreenIntRect
#include "UnitTransforms.h"                  // for ViewAs
#include "apz/src/AsyncPanZoomController.h"  // for AsyncPanZoomController
#include "gfxEnv.h"                          // for gfxEnv

#ifdef XP_MACOSX
#  include "gfxPlatformMac.h"
#endif
#include "gfxRect.h"             // for gfxRect
#include "gfxUtils.h"            // for frame color util
#include "mozilla/Assertions.h"  // for MOZ_ASSERT, etc
#include "mozilla/RefPtr.h"      // for RefPtr, already_AddRefed
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_layers.h"
#include "mozilla/gfx/2D.h"             // for DrawTarget
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/gfx/Point.h"          // for IntSize, Point
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/gfx/Types.h"          // for Color, SurfaceFormat
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/CompositorOGL.h"
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/Effects.h"              // for Effect, EffectChain, etc
#include "mozilla/layers/LayerMetricsWrapper.h"  // for LayerMetricsWrapper
#include "mozilla/layers/LayersTypes.h"          // for etc
#include "mozilla/widget/CompositorWidget.h"     // for WidgetRenderingContext
#include "ipc/CompositorBench.h"                 // for CompositorBench
#include "ipc/ShadowLayerUtils.h"
#include "mozilla/mozalloc.h"  // for operator new, etc
#include "nsAppRunner.h"
#include "mozilla/RefPtr.h"   // for nsRefPtr
#include "nsCOMPtr.h"         // for already_AddRefed
#include "nsDebug.h"          // for NS_WARNING, etc
#include "nsISupportsImpl.h"  // for Layer::AddRef, etc
#include "nsPoint.h"          // for nsIntPoint
#include "nsRect.h"           // for mozilla::gfx::IntRect
#include "nsRegion.h"         // for nsIntRegion, etc
#if defined(MOZ_WIDGET_ANDROID)
#  include <android/log.h>
#  include <android/native_window.h>
#  include "mozilla/jni/Utils.h"
#  include "mozilla/widget/AndroidCompositorWidget.h"
#  include "GLConsts.h"
#  include "GLContextEGL.h"
#  include "GLContextProvider.h"
#  include "mozilla/Unused.h"
#  include "ScopedGLHelpers.h"
#endif
#include "GeckoProfiler.h"
#include "TextRenderer.h"  // for TextRenderer
#include "mozilla/layers/CompositorBridgeParent.h"
#include "TreeTraversal.h"  // for ForEachNode
#include "CompositionRecorder.h"

#ifdef USE_SKIA
#  include "PaintCounter.h"  // For PaintCounter
#endif

class gfxContext;

namespace mozilla {
namespace layers {

class ImageLayer;

using namespace mozilla::gfx;
using namespace mozilla::gl;

class WindowLMC : public profiler_screenshots::Window {
 public:
  explicit WindowLMC(Compositor* aCompositor) : mCompositor(aCompositor) {}

  already_AddRefed<profiler_screenshots::RenderSource> GetWindowContents(
      const IntSize& aWindowSize) override;
  already_AddRefed<profiler_screenshots::DownscaleTarget> CreateDownscaleTarget(
      const IntSize& aSize) override;
  already_AddRefed<profiler_screenshots::AsyncReadbackBuffer>
  CreateAsyncReadbackBuffer(const IntSize& aSize) override;

 protected:
  Compositor* mCompositor;
};

static LayerComposite* ToLayerComposite(Layer* aLayer) {
  return static_cast<LayerComposite*>(aLayer->ImplData());
}

static void ClearSubtree(Layer* aLayer) {
  ForEachNode<ForwardIterator>(aLayer, [](Layer* layer) {
    ToLayerComposite(layer)->CleanupResources();
  });
}

void LayerManagerComposite::ClearCachedResources(Layer* aSubtree) {
  MOZ_ASSERT(!aSubtree || aSubtree->Manager() == this);
  Layer* subtree = aSubtree ? aSubtree : mRoot.get();
  if (!subtree) {
    return;
  }

  ClearSubtree(subtree);
  // FIXME [bjacob]
  // XXX the old LayerManagerOGL code had a mMaybeInvalidTree that it set to
  // true here. Do we need that?
}

HostLayerManager::HostLayerManager()
    : mDebugOverlayWantsNextFrame(false),
      mWarningLevel(0.0f),
      mCompositorBridgeID(0),
      mLastPaintTime(TimeDuration::Forever()),
      mRenderStartTime(TimeStamp::Now()) {}

HostLayerManager::~HostLayerManager() = default;

void HostLayerManager::RecordPaintTimes(const PaintTiming& aTiming) {
  mDiagnostics->RecordPaintTimes(aTiming);
}

void HostLayerManager::RecordUpdateTime(float aValue) {
  mDiagnostics->RecordUpdateTime(aValue);
}

void HostLayerManager::WriteCollectedFrames() {
  if (mCompositionRecorder) {
    mCompositionRecorder->WriteCollectedFrames();
    mCompositionRecorder = nullptr;
  }
}

Maybe<CollectedFrames> HostLayerManager::GetCollectedFrames() {
  Maybe<CollectedFrames> maybeFrames;

  if (mCompositionRecorder) {
    maybeFrames.emplace(mCompositionRecorder->GetCollectedFrames());
    mCompositionRecorder = nullptr;
  }

  return maybeFrames;
}

/**
 * LayerManagerComposite
 */
LayerManagerComposite::LayerManagerComposite(Compositor* aCompositor)
    : mUnusedApzTransformWarning(false),
      mDisabledApzWarning(false),
      mCompositor(aCompositor),
      mInTransaction(false),
      mIsCompositorReady(false)
#if defined(MOZ_WIDGET_ANDROID)
      ,
      mScreenPixelsTarget(nullptr)
#endif  // defined(MOZ_WIDGET_ANDROID)
{
  mTextRenderer = new TextRenderer();
  mDiagnostics = MakeUnique<Diagnostics>();
  MOZ_ASSERT(aCompositor);
  mNativeLayerRoot = aCompositor->GetWidget()->GetNativeLayerRoot();
  if (mNativeLayerRoot) {
    mSurfacePoolHandle = aCompositor->GetSurfacePoolHandle();
    MOZ_RELEASE_ASSERT(mSurfacePoolHandle);
  }

#ifdef USE_SKIA
  mPaintCounter = nullptr;
#endif
}

LayerManagerComposite::~LayerManagerComposite() { Destroy(); }

void LayerManagerComposite::Destroy() {
  if (!mDestroyed) {
    mCompositor->GetWidget()->CleanupWindowEffects();
    if (mRoot) {
      RootLayer()->Destroy();
    }
    mCompositor->CancelFrame();
    mRoot = nullptr;
    mClonedLayerTreeProperties = nullptr;
    mProfilerScreenshotGrabber.Destroy();

    if (mNativeLayerRoot) {
      if (mGPUStatsLayer) {
        mNativeLayerRoot->RemoveLayer(mGPUStatsLayer);
        mGPUStatsLayer = nullptr;
      }
      if (mUnusedTransformWarningLayer) {
        mNativeLayerRoot->RemoveLayer(mUnusedTransformWarningLayer);
        mUnusedTransformWarningLayer = nullptr;
      }
      if (mDisabledApzWarningLayer) {
        mNativeLayerRoot->RemoveLayer(mDisabledApzWarningLayer);
        mDisabledApzWarningLayer = nullptr;
      }
      for (const auto& nativeLayer : mNativeLayers) {
        mNativeLayerRoot->RemoveLayer(nativeLayer);
      }
      mNativeLayers.clear();
      mNativeLayerRoot = nullptr;
    }
    mDestroyed = true;

#ifdef USE_SKIA
    mPaintCounter = nullptr;
#endif
  }
}

void LayerManagerComposite::UpdateRenderBounds(const IntRect& aRect) {
  mRenderBounds = aRect;
}

bool LayerManagerComposite::AreComponentAlphaLayersEnabled() {
  return mCompositor->GetBackendType() != LayersBackend::LAYERS_BASIC &&
         mCompositor->SupportsEffect(EffectTypes::COMPONENT_ALPHA) &&
         LayerManager::AreComponentAlphaLayersEnabled();
}

bool LayerManagerComposite::BeginTransaction(const nsCString& aURL) {
  mInTransaction = true;

  if (!mCompositor->Ready()) {
    return false;
  }

  mIsCompositorReady = true;
  return true;
}

void LayerManagerComposite::BeginTransactionWithDrawTarget(
    DrawTarget* aTarget, const IntRect& aRect) {
  mInTransaction = true;

  if (!mCompositor->Ready()) {
    return;
  }

#ifdef MOZ_LAYERS_HAVE_LOG
  MOZ_LAYERS_LOG(("[----- BeginTransaction"));
  Log();
#endif

  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return;
  }

  mIsCompositorReady = true;
  mTarget = aTarget;
  mTargetBounds = aRect;
}

template <typename Units>
static IntRectTyped<Units> TransformRect(const IntRectTyped<Units>& aRect,
                                         const Matrix& aTransform,
                                         bool aRoundIn = false) {
  if (aRect.IsEmpty()) {
    return IntRectTyped<Units>();
  }

  Rect rect(aRect.X(), aRect.Y(), aRect.Width(), aRect.Height());
  rect = aTransform.TransformBounds(rect);
  if (aRoundIn) {
    MOZ_ASSERT(aTransform.PreservesAxisAlignedRectangles());
    rect.RoundIn();
  } else {
    rect.RoundOut();
  }

  IntRect intRect;
  if (!rect.ToIntRect(&intRect)) {
    intRect = IntRect::MaxIntRect();
  }

  return ViewAs<Units>(intRect);
}

template <typename Units>
static IntRectTyped<Units> TransformRect(const IntRectTyped<Units>& aRect,
                                         const Matrix4x4& aTransform,
                                         bool aRoundIn = false) {
  if (aRect.IsEmpty()) {
    return IntRectTyped<Units>();
  }

  Rect rect(aRect.X(), aRect.Y(), aRect.Width(), aRect.Height());
  rect = aTransform.TransformAndClipBounds(rect, Rect::MaxIntRect());
  if (aRoundIn) {
    rect.RoundIn();
  } else {
    rect.RoundOut();
  }

  IntRect intRect;
  if (!rect.ToIntRect(&intRect)) {
    intRect = IntRect::MaxIntRect();
  }

  return ViewAs<Units>(intRect);
}

template <typename Units, typename MatrixType>
static IntRectTyped<Units> TransformRectRoundIn(
    const IntRectTyped<Units>& aRect, const MatrixType& aTransform) {
  return TransformRect(aRect, aTransform, true);
}

template <typename Units, typename MatrixType>
static void AddTransformedRegion(IntRegionTyped<Units>& aDest,
                                 const IntRegionTyped<Units>& aSource,
                                 const MatrixType& aTransform) {
  for (auto iter = aSource.RectIter(); !iter.Done(); iter.Next()) {
    aDest.Or(aDest, TransformRect(iter.Get(), aTransform));
  }
  aDest.SimplifyOutward(20);
}

template <typename Units, typename MatrixType>
static void AddTransformedRegionRoundIn(IntRegionTyped<Units>& aDest,
                                        const IntRegionTyped<Units>& aSource,
                                        const MatrixType& aTransform) {
  for (auto iter = aSource.RectIter(); !iter.Done(); iter.Next()) {
    aDest.Or(aDest, TransformRectRoundIn(iter.Get(), aTransform));
  }
}

void LayerManagerComposite::PostProcessLayers(nsIntRegion& aOpaqueRegion) {
  LayerIntRegion visible;
  LayerComposite* rootComposite =
      static_cast<LayerComposite*>(mRoot->AsHostLayer());
  PostProcessLayers(
      mRoot, aOpaqueRegion, visible,
      ViewAs<RenderTargetPixel>(
          rootComposite->GetShadowClipRect(),
          PixelCastJustification::RenderTargetIsParentLayerForRoot),
      Nothing(), true);
}

// We want to skip directly through ContainerLayers that don't have an
// intermediate surface. We compute occlusions for leaves and intermediate
// surfaces against the layer that they actually composite into so that we can
// use the final (snapped) effective transform.
static bool ShouldProcessLayer(Layer* aLayer) {
  if (!aLayer->AsContainerLayer()) {
    return true;
  }

  return aLayer->AsContainerLayer()->UseIntermediateSurface();
}

void LayerManagerComposite::PostProcessLayers(
    Layer* aLayer, nsIntRegion& aOpaqueRegion, LayerIntRegion& aVisibleRegion,
    const Maybe<RenderTargetIntRect>& aRenderTargetClip,
    const Maybe<ParentLayerIntRect>& aClipFromAncestors,
    bool aCanContributeOpaque) {
  // Compute a clip that's the combination of our layer clip with the clip
  // from our ancestors.
  LayerComposite* composite =
      static_cast<LayerComposite*>(aLayer->AsHostLayer());
  Maybe<ParentLayerIntRect> layerClip = composite->GetShadowClipRect();
  MOZ_ASSERT(!layerClip || !aLayer->Combines3DTransformWithAncestors(),
             "The layer with a clip should not participate "
             "a 3D rendering context");
  Maybe<ParentLayerIntRect> outsideClip =
      IntersectMaybeRects(layerClip, aClipFromAncestors);

  Maybe<LayerIntRect> insideClip;
  if (aLayer->Extend3DContext()) {
    // If we're preserve-3d just pass the clip rect down directly, and we'll do
    // the conversion at the preserve-3d leaf Layer.
    if (outsideClip) {
      insideClip = Some(ViewAs<LayerPixel>(
          *outsideClip, PixelCastJustification::MovingDownToChildren));
    }
  } else if (outsideClip) {
    // Convert the combined clip into our pre-transform coordinate space, so
    // that it can later be intersected with our visible region.
    // If our transform is a perspective, there's no meaningful insideClip rect
    // we can compute (it would need to be a cone).
    Matrix4x4 localTransform = aLayer->ComputeTransformToPreserve3DRoot();
    if (!localTransform.HasPerspectiveComponent() && localTransform.Invert()) {
      LayerRect insideClipFloat =
          UntransformBy(ViewAs<ParentLayerToLayerMatrix4x4>(localTransform),
                        ParentLayerRect(*outsideClip), LayerRect::MaxIntRect())
              .valueOr(LayerRect());
      insideClipFloat.RoundOut();
      LayerIntRect insideClipInt;
      if (insideClipFloat.ToIntRect(&insideClipInt)) {
        insideClip = Some(insideClipInt);
      }
    }
  }

  Maybe<ParentLayerIntRect> ancestorClipForChildren;
  if (insideClip) {
    ancestorClipForChildren = Some(ViewAs<ParentLayerPixel>(
        *insideClip, PixelCastJustification::MovingDownToChildren));
  }

  nsIntRegion dummy;
  nsIntRegion& opaqueRegion = aOpaqueRegion;
  if (aLayer->Extend3DContext() || aLayer->Combines3DTransformWithAncestors()) {
    opaqueRegion = dummy;
  }

  if (!ShouldProcessLayer(aLayer)) {
    MOZ_ASSERT(aLayer->AsContainerLayer() &&
               !aLayer->AsContainerLayer()->UseIntermediateSurface());
    // For layers participating 3D rendering context, their visible
    // region should be empty (invisible), so we pass through them
    // without doing anything.
    for (Layer* child = aLayer->GetLastChild(); child;
         child = child->GetPrevSibling()) {
      LayerComposite* childComposite =
          static_cast<LayerComposite*>(child->AsHostLayer());
      Maybe<RenderTargetIntRect> renderTargetClip = aRenderTargetClip;
      if (childComposite->GetShadowClipRect()) {
        RenderTargetIntRect clip = TransformBy(
            ViewAs<ParentLayerToRenderTargetMatrix4x4>(
                aLayer->GetEffectiveTransform(),
                PixelCastJustification::RenderTargetIsParentLayerForRoot),
            *childComposite->GetShadowClipRect());
        renderTargetClip = IntersectMaybeRects(renderTargetClip, Some(clip));
      }

      PostProcessLayers(
          child, opaqueRegion, aVisibleRegion, renderTargetClip,
          ancestorClipForChildren,
          aCanContributeOpaque &
              !(aLayer->GetContentFlags() & Layer::CONTENT_BACKFACE_HIDDEN));
    }
    return;
  }

  nsIntRegion localOpaque;
  // Treat layers on the path to the root of the 3D rendering context as
  // a giant layer if it is a leaf.
  Matrix4x4 transform = aLayer->GetEffectiveTransform();
  Matrix transform2d;
  bool canTransformOpaqueRegion = false;
  // If aLayer has a simple transform (only an integer translation) then we
  // can easily convert aOpaqueRegion into pre-transform coordinates and include
  // that region.
  if (aCanContributeOpaque &&
      !(aLayer->GetContentFlags() & Layer::CONTENT_BACKFACE_HIDDEN) &&
      transform.Is2D(&transform2d) &&
      transform2d.PreservesAxisAlignedRectangles()) {
    Matrix inverse = transform2d;
    inverse.Invert();
    AddTransformedRegionRoundIn(localOpaque, opaqueRegion, inverse);
    canTransformOpaqueRegion = true;
  }

  // Save the value of localOpaque, which currently stores the region obscured
  // by siblings (and uncles and such), before our descendants contribute to it.
  nsIntRegion obscured = localOpaque;

  // Recurse on our descendants, in front-to-back order. In this process:
  //  - Occlusions are computed for them, and they contribute to localOpaque.
  //  - They recalculate their visible regions, taking ancestorClipForChildren
  //    into account, and accumulate them into descendantsVisibleRegion.
  LayerIntRegion descendantsVisibleRegion;

  bool hasPreserve3DChild = false;
  for (Layer* child = aLayer->GetLastChild(); child;
       child = child->GetPrevSibling()) {
    MOZ_ASSERT(aLayer->AsContainerLayer()->UseIntermediateSurface());
    LayerComposite* childComposite =
        static_cast<LayerComposite*>(child->AsHostLayer());
    PostProcessLayers(
        child, localOpaque, descendantsVisibleRegion,
        ViewAs<RenderTargetPixel>(
            childComposite->GetShadowClipRect(),
            PixelCastJustification::RenderTargetIsParentLayerForRoot),
        ancestorClipForChildren, true);
    if (child->Extend3DContext()) {
      hasPreserve3DChild = true;
    }
  }

  // Recalculate our visible region.
  LayerIntRegion visible = composite->GetShadowVisibleRegion();

  // If we have descendants, throw away the visible region stored on this
  // layer, and use the region accumulated by our descendants instead.
  if (aLayer->GetFirstChild() && !hasPreserve3DChild) {
    visible = descendantsVisibleRegion;
  }

  // Subtract any areas that we know to be opaque.
  if (!obscured.IsEmpty()) {
    visible.SubOut(LayerIntRegion::FromUnknownRegion(obscured));
  }

  // Clip the visible region using the combined clip.
  if (insideClip) {
    visible.AndWith(*insideClip);
  }
  composite->SetShadowVisibleRegion(visible);

  // Transform the newly calculated visible region into our parent's space,
  // apply our clip to it (if any), and accumulate it into |aVisibleRegion|
  // for the caller to use.
  ParentLayerIntRegion visibleParentSpace =
      TransformBy(ViewAs<LayerToParentLayerMatrix4x4>(transform), visible);
  aVisibleRegion.OrWith(ViewAs<LayerPixel>(
      visibleParentSpace, PixelCastJustification::MovingDownToChildren));

  // If we have a simple transform, then we can add our opaque area into
  // aOpaqueRegion.
  if (canTransformOpaqueRegion && !aLayer->HasMaskLayers() &&
      aLayer->IsOpaqueForVisibility()) {
    if (aLayer->IsOpaque()) {
      localOpaque.OrWith(composite->GetFullyRenderedRegion());
    }
    nsIntRegion parentSpaceOpaque;
    AddTransformedRegionRoundIn(parentSpaceOpaque, localOpaque, transform2d);
    if (aRenderTargetClip) {
      parentSpaceOpaque.AndWith(aRenderTargetClip->ToUnknownRect());
    }
    opaqueRegion.OrWith(parentSpaceOpaque);
  }
}

void LayerManagerComposite::EndTransaction(const TimeStamp& aTimeStamp,
                                           EndTransactionFlags aFlags) {
  NS_ASSERTION(mInTransaction, "Didn't call BeginTransaction?");
  NS_ASSERTION(!(aFlags & END_NO_COMPOSITE),
               "Shouldn't get END_NO_COMPOSITE here");
  mInTransaction = false;
  mRenderStartTime = TimeStamp::Now();

  if (!mIsCompositorReady) {
    return;
  }
  mIsCompositorReady = false;

#ifdef MOZ_LAYERS_HAVE_LOG
  MOZ_LAYERS_LOG(("  ----- (beginning paint)"));
  Log();
#endif

  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return;
  }

  // Set composition timestamp here because we need it in
  // ComputeEffectiveTransforms (so the correct video frame size is picked) and
  // also to compute invalid regions properly.
  SetCompositionTime(aTimeStamp);

  if (mRoot && !(aFlags & END_NO_IMMEDIATE_REDRAW)) {
    MOZ_ASSERT(!aTimeStamp.IsNull());
    UpdateAndRender();
    mCompositor->FlushPendingNotifyNotUsed();
  }

  mTarget = nullptr;

#ifdef MOZ_LAYERS_HAVE_LOG
  Log();
  MOZ_LAYERS_LOG(("]----- EndTransaction"));
#endif
}

void LayerManagerComposite::UpdateAndRender() {
  mCompositionOpportunityId = mCompositionOpportunityId.Next();

  if (gfxEnv::SkipComposition()) {
    mInvalidRegion.SetEmpty();
    return;
  }

  // The results of our drawing always go directly into a pixel buffer,
  // so we don't need to pass any global transform here.
  mRoot->ComputeEffectiveTransforms(gfx::Matrix4x4());

  nsIntRegion opaque;
  PostProcessLayers(opaque);

  if (mClonedLayerTreeProperties) {
    // We need to compute layer tree differences even if we're not going to
    // immediately use the resulting damage area, since ComputeDifferences
    // is also responsible for invalidates intermediate surfaces in
    // ContainerLayers.

    nsIntRegion changed;
    const bool overflowed = !mClonedLayerTreeProperties->ComputeDifferences(
        mRoot, changed, nullptr);

    if (overflowed) {
      changed = mRenderBounds;
    }

    mInvalidRegion.Or(mInvalidRegion, changed);
  }

  nsIntRegion invalid;
  if (mTarget) {
    // Since we're composing to an external target, we're not going to use
    // the damage region from layers changes - we want to composite
    // everything in the target bounds. The layers damage region has been
    // stored in mInvalidRegion and will be picked up by the next window
    // composite.
    invalid = mTargetBounds;
  } else {
    if (!mClonedLayerTreeProperties) {
      // If we didn't have a previous layer tree, invalidate the entire render
      // area.
      mInvalidRegion = mRenderBounds;
    }

    invalid = mInvalidRegion;
  }

  if (invalid.IsEmpty()) {
    // Composition requested, but nothing has changed. Don't do any work.
    mClonedLayerTreeProperties = LayerProperties::CloneFrom(GetRoot());
    mProfilerScreenshotGrabber.NotifyEmptyFrame();

    // Discard the current payloads. These payloads did not require a composite
    // (they caused no changes to anything visible), so we don't want to measure
    // their latency.
    mPayload.Clear();

    return;
  }

  // We don't want our debug overlay to cause more frames to happen
  // so we will invalidate after we've decided if something changed.
  // Only invalidate if we're not using native layers. When using native layers,
  // UpdateDebugOverlayNativeLayers will repaint the appropriate layer areas.
  if (!mNativeLayerRoot) {
    InvalidateDebugOverlay(invalid, mRenderBounds);
  }

  bool rendered = Render(invalid, opaque);
#if defined(MOZ_WIDGET_ANDROID)
  RenderToPresentationSurface();
#endif

  if (!mTarget && rendered) {
    mInvalidRegion.SetEmpty();
  }

  // Update cached layer tree information.
  mClonedLayerTreeProperties = LayerProperties::CloneFrom(GetRoot());
}

already_AddRefed<DrawTarget> LayerManagerComposite::CreateOptimalMaskDrawTarget(
    const IntSize& aSize) {
  MOZ_CRASH("Should only be called on the drawing side");
  return nullptr;
}

LayerComposite* LayerManagerComposite::RootLayer() const {
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }

  return ToLayerComposite(mRoot);
}

void LayerManagerComposite::InvalidateDebugOverlay(nsIntRegion& aInvalidRegion,
                                                   const IntRect& aBounds) {
  bool drawFps = StaticPrefs::layers_acceleration_draw_fps();
  bool drawFrameColorBars = StaticPrefs::gfx_draw_color_bars();

  if (drawFps) {
    aInvalidRegion.Or(aInvalidRegion, nsIntRect(0, 0, 650, 400));
  }
  if (drawFrameColorBars) {
    aInvalidRegion.Or(aInvalidRegion, nsIntRect(0, 0, 10, aBounds.Height()));
  }

#ifdef USE_SKIA
  bool drawPaintTimes = StaticPrefs::gfx_content_always_paint();
  if (drawPaintTimes) {
    aInvalidRegion.Or(aInvalidRegion, nsIntRect(PaintCounter::GetPaintRect()));
  }
#endif
}

#ifdef USE_SKIA
void LayerManagerComposite::DrawPaintTimes(Compositor* aCompositor) {
  if (!mPaintCounter) {
    mPaintCounter = new PaintCounter();
  }

  TimeDuration compositeTime = TimeStamp::Now() - mRenderStartTime;
  mPaintCounter->Draw(aCompositor, mLastPaintTime, compositeTime);
}
#endif

static Rect RectWithEdges(int32_t aTop, int32_t aRight, int32_t aBottom,
                          int32_t aLeft) {
  return Rect(aLeft, aTop, aRight - aLeft, aBottom - aTop);
}

void LayerManagerComposite::DrawBorder(const IntRect& aOuter,
                                       int32_t aBorderWidth,
                                       const DeviceColor& aColor,
                                       const Matrix4x4& aTransform) {
  EffectChain effects;
  effects.mPrimaryEffect = new EffectSolidColor(aColor);

  IntRect inner(aOuter);
  inner.Deflate(aBorderWidth);
  // Top and bottom border sides
  mCompositor->DrawQuad(
      RectWithEdges(aOuter.Y(), aOuter.XMost(), inner.Y(), aOuter.X()), aOuter,
      effects, 1, aTransform);
  mCompositor->DrawQuad(
      RectWithEdges(inner.YMost(), aOuter.XMost(), aOuter.YMost(), aOuter.X()),
      aOuter, effects, 1, aTransform);
  // Left and right border sides
  mCompositor->DrawQuad(
      RectWithEdges(inner.Y(), inner.X(), inner.YMost(), aOuter.X()), aOuter,
      effects, 1, aTransform);
  mCompositor->DrawQuad(
      RectWithEdges(inner.Y(), aOuter.XMost(), inner.YMost(), inner.XMost()),
      aOuter, effects, 1, aTransform);
}

void LayerManagerComposite::DrawTranslationWarningOverlay(
    const IntRect& aBounds) {
  // Black blorder
  IntRect blackBorderBounds(aBounds);
  blackBorderBounds.Deflate(4);
  DrawBorder(blackBorderBounds, 6, DeviceColor(0, 0, 0, 1), Matrix4x4());

  // Warning border, yellow to red
  IntRect warnBorder(aBounds);
  warnBorder.Deflate(5);
  DrawBorder(warnBorder, 4, DeviceColor(1, 1.f - mWarningLevel, 0, 1),
             Matrix4x4());
}

static uint16_t sFrameCount = 0;
void LayerManagerComposite::RenderDebugOverlay(const IntRect& aBounds) {
  bool drawFps = StaticPrefs::layers_acceleration_draw_fps();
  bool drawFrameColorBars = StaticPrefs::gfx_draw_color_bars();

  // Don't draw diagnostic overlays if we want to snapshot the output.
  if (mTarget) {
    return;
  }

  if (drawFps) {
#ifdef ANDROID
    // Draw a translation delay warning overlay
    if (!mWarnTime.IsNull() && (TimeStamp::Now() - mWarnTime).ToMilliseconds() <
                                   kVisualWarningDuration) {
      DrawTranslationWarningOverlay(aBounds);
      SetDebugOverlayWantsNextFrame(true);
    }
#endif

    GPUStats stats;
    stats.mScreenPixels = mRenderBounds.Width() * mRenderBounds.Height();
    mCompositor->GetFrameStats(&stats);

    std::string text = mDiagnostics->GetFrameOverlayString(stats);
    mTextRenderer->RenderText(mCompositor, text, IntPoint(2, 5), Matrix4x4(),
                              24, 600, TextRenderer::FontType::FixedWidth);

    float alpha = 1;
    if (mUnusedApzTransformWarning) {
      // If we have an unused APZ transform on this composite, draw a 20x20 red
      // box in the top-right corner
      EffectChain effects;
      effects.mPrimaryEffect =
          new EffectSolidColor(gfx::DeviceColor(1, 0, 0, 1));
      mCompositor->DrawQuad(gfx::Rect(aBounds.Width() - 20, 0, 20, 20), aBounds,
                            effects, alpha, gfx::Matrix4x4());

      mUnusedApzTransformWarning = false;
      SetDebugOverlayWantsNextFrame(true);
    }
    if (mDisabledApzWarning) {
      // If we have a disabled APZ on this composite, draw a 20x20 yellow box
      // in the top-right corner, to the left of the unused-apz-transform
      // warning box
      EffectChain effects;
      effects.mPrimaryEffect =
          new EffectSolidColor(gfx::DeviceColor(1, 1, 0, 1));
      mCompositor->DrawQuad(gfx::Rect(aBounds.Width() - 40, 0, 20, 20), aBounds,
                            effects, alpha, gfx::Matrix4x4());

      mDisabledApzWarning = false;
      SetDebugOverlayWantsNextFrame(true);
    }
  }

  if (drawFrameColorBars) {
    gfx::IntRect sideRect(0, 0, 10, aBounds.Height());

    EffectChain effects;
    effects.mPrimaryEffect =
        new EffectSolidColor(gfxUtils::GetColorForFrameNumber(sFrameCount));
    mCompositor->DrawQuad(Rect(sideRect), sideRect, effects, 1.0,
                          gfx::Matrix4x4());

    // We intentionally overflow at 2^16.
    sFrameCount++;
  }

#ifdef USE_SKIA
  bool drawPaintTimes = StaticPrefs::gfx_content_always_paint();
  if (drawPaintTimes) {
    DrawPaintTimes(mCompositor);
  }
#endif
}

void LayerManagerComposite::UpdateDebugOverlayNativeLayers() {
  // Remove all debug layers first because PlaceNativeLayers might have changed
  // the z-order. By removing and re-adding, we keep the debug overlay layers
  // on top.
  if (mGPUStatsLayer) {
    mNativeLayerRoot->RemoveLayer(mGPUStatsLayer);
  }
  if (mUnusedTransformWarningLayer) {
    mNativeLayerRoot->RemoveLayer(mUnusedTransformWarningLayer);
  }
  if (mDisabledApzWarningLayer) {
    mNativeLayerRoot->RemoveLayer(mDisabledApzWarningLayer);
  }

  bool drawFps = StaticPrefs::layers_acceleration_draw_fps();

  if (drawFps) {
    GPUStats stats;
    stats.mScreenPixels = mRenderBounds.Area();
    mCompositor->GetFrameStats(&stats);

    std::string text = mDiagnostics->GetFrameOverlayString(stats);
    IntSize size = mTextRenderer->ComputeSurfaceSize(
        text, 600, TextRenderer::FontType::FixedWidth);

    if (!mGPUStatsLayer || mGPUStatsLayer->GetSize() != size) {
      mGPUStatsLayer =
          mNativeLayerRoot->CreateLayer(size, false, mSurfacePoolHandle);
    }

    mGPUStatsLayer->SetPosition(IntPoint(2, 5));
    IntRect bounds({}, size);
    RefPtr<DrawTarget> dt = mGPUStatsLayer->NextSurfaceAsDrawTarget(
        bounds, bounds, BackendType::SKIA);
    mTextRenderer->RenderTextToDrawTarget(dt, text, 600,
                                          TextRenderer::FontType::FixedWidth);
    mGPUStatsLayer->NotifySurfaceReady();
    mNativeLayerRoot->AppendLayer(mGPUStatsLayer);

    IntSize square(20, 20);
    // The two warning layers are created on demand and their content is only
    // drawn once. After that, they only get moved (if the window size changes)
    // and conditionally shown.
    // The drawing would be unnecessary if we had native "color layers".
    if (mUnusedApzTransformWarning) {
      // If we have an unused APZ transform on this composite, draw a 20x20 red
      // box in the top-right corner.
      if (!mUnusedTransformWarningLayer) {
        mUnusedTransformWarningLayer =
            mNativeLayerRoot->CreateLayer(square, true, mSurfacePoolHandle);
        RefPtr<DrawTarget> dt =
            mUnusedTransformWarningLayer->NextSurfaceAsDrawTarget(
                IntRect({}, square), IntRect({}, square), BackendType::SKIA);
        dt->FillRect(Rect(0, 0, 20, 20), ColorPattern(DeviceColor(1, 0, 0, 1)));
        mUnusedTransformWarningLayer->NotifySurfaceReady();
      }
      mUnusedTransformWarningLayer->SetPosition(
          IntPoint(mRenderBounds.XMost() - 20, mRenderBounds.Y()));
      mNativeLayerRoot->AppendLayer(mUnusedTransformWarningLayer);

      mUnusedApzTransformWarning = false;
      SetDebugOverlayWantsNextFrame(true);
    }

    if (mDisabledApzWarning) {
      // If we have a disabled APZ on this composite, draw a 20x20 yellow box
      // in the top-right corner, to the left of the unused-apz-transform
      // warning box.
      if (!mDisabledApzWarningLayer) {
        mDisabledApzWarningLayer =
            mNativeLayerRoot->CreateLayer(square, true, mSurfacePoolHandle);
        RefPtr<DrawTarget> dt =
            mDisabledApzWarningLayer->NextSurfaceAsDrawTarget(
                IntRect({}, square), IntRect({}, square), BackendType::SKIA);
        dt->FillRect(Rect(0, 0, 20, 20), ColorPattern(DeviceColor(1, 1, 0, 1)));
        mDisabledApzWarningLayer->NotifySurfaceReady();
      }
      mDisabledApzWarningLayer->SetPosition(
          IntPoint(mRenderBounds.XMost() - 40, mRenderBounds.Y()));
      mNativeLayerRoot->AppendLayer(mDisabledApzWarningLayer);

      mDisabledApzWarning = false;
      SetDebugOverlayWantsNextFrame(true);
    }
  } else {
    mGPUStatsLayer = nullptr;
    mUnusedTransformWarningLayer = nullptr;
    mDisabledApzWarningLayer = nullptr;
  }
}

RefPtr<CompositingRenderTarget>
LayerManagerComposite::PushGroupForLayerEffects() {
  // This is currently true, so just making sure that any new use of this
  // method is flagged for investigation
  MOZ_ASSERT(StaticPrefs::layers_effect_invert() ||
             StaticPrefs::layers_effect_grayscale() ||
             StaticPrefs::layers_effect_contrast() != 0.0);

  RefPtr<CompositingRenderTarget> previousTarget =
      mCompositor->GetCurrentRenderTarget();
  // make our render target the same size as the destination target
  // so that we don't have to change size if the drawing area changes.
  IntRect rect(previousTarget->GetOrigin(), previousTarget->GetSize());
  // XXX: I'm not sure if this is true or not...
  MOZ_ASSERT(rect.IsEqualXY(0, 0));
  if (!mTwoPassTmpTarget ||
      mTwoPassTmpTarget->GetSize() != previousTarget->GetSize() ||
      mTwoPassTmpTarget->GetOrigin() != previousTarget->GetOrigin()) {
    mTwoPassTmpTarget = mCompositor->CreateRenderTarget(rect, INIT_MODE_NONE);
  }
  MOZ_ASSERT(mTwoPassTmpTarget);
  mCompositor->SetRenderTarget(mTwoPassTmpTarget);
  return previousTarget;
}

void LayerManagerComposite::PopGroupForLayerEffects(
    RefPtr<CompositingRenderTarget> aPreviousTarget, IntRect aClipRect,
    bool aGrayscaleEffect, bool aInvertEffect, float aContrastEffect) {
  MOZ_ASSERT(mTwoPassTmpTarget);

  // This is currently true, so just making sure that any new use of this
  // method is flagged for investigation
  MOZ_ASSERT(aInvertEffect || aGrayscaleEffect || aContrastEffect != 0.0);

  mCompositor->SetRenderTarget(aPreviousTarget);

  EffectChain effectChain(RootLayer());
  Matrix5x4 effectMatrix;
  if (aGrayscaleEffect) {
    // R' = G' = B' = luminance
    // R' = 0.2126*R + 0.7152*G + 0.0722*B
    // G' = 0.2126*R + 0.7152*G + 0.0722*B
    // B' = 0.2126*R + 0.7152*G + 0.0722*B
    Matrix5x4 grayscaleMatrix(0.2126f, 0.2126f, 0.2126f, 0, 0.7152f, 0.7152f,
                              0.7152f, 0, 0.0722f, 0.0722f, 0.0722f, 0, 0, 0, 0,
                              1, 0, 0, 0, 0);
    effectMatrix = grayscaleMatrix;
  }

  if (aInvertEffect) {
    // R' = 1 - R
    // G' = 1 - G
    // B' = 1 - B
    Matrix5x4 colorInvertMatrix(-1, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0,
                                1, 1, 1, 1, 0);
    effectMatrix = effectMatrix * colorInvertMatrix;
  }

  if (aContrastEffect != 0.0) {
    // Multiplying with:
    // R' = (1 + c) * (R - 0.5) + 0.5
    // G' = (1 + c) * (G - 0.5) + 0.5
    // B' = (1 + c) * (B - 0.5) + 0.5
    float cP1 = aContrastEffect + 1;
    float hc = 0.5 * aContrastEffect;
    Matrix5x4 contrastMatrix(cP1, 0, 0, 0, 0, cP1, 0, 0, 0, 0, cP1, 0, 0, 0, 0,
                             1, -hc, -hc, -hc, 0);
    effectMatrix = effectMatrix * contrastMatrix;
  }

  effectChain.mPrimaryEffect = new EffectRenderTarget(mTwoPassTmpTarget);
  effectChain.mSecondaryEffects[EffectTypes::COLOR_MATRIX] =
      new EffectColorMatrix(effectMatrix);

  mCompositor->DrawQuad(Rect(Point(0, 0), Size(mTwoPassTmpTarget->GetSize())),
                        aClipRect, effectChain, 1., Matrix4x4());
}

void LayerManagerComposite::PlaceNativeLayers(
    const IntRegion& aRegion, bool aOpaque,
    std::deque<RefPtr<NativeLayer>>* aLayersToRecycle,
    IntRegion* aWindowInvalidRegion) {
  IntSize tileSize(StaticPrefs::layers_compositing_tiles_width(),
                   StaticPrefs::layers_compositing_tiles_height());
  IntRect regionBounds = aRegion.GetBounds();
  for (int32_t y = 0; y < regionBounds.YMost(); y += tileSize.height) {
    for (int32_t x = 0; x < regionBounds.XMost(); x += tileSize.width) {
      IntRegion tileRegion;
      tileRegion.And(aRegion, IntRect(IntPoint(x, y), tileSize));
      for (auto iter = tileRegion.RectIter(); !iter.Done(); iter.Next()) {
        PlaceNativeLayer(iter.Get(), aOpaque, aLayersToRecycle,
                         aWindowInvalidRegion);
      }
    }
  }
}

void LayerManagerComposite::PlaceNativeLayer(
    const IntRect& aRect, bool aOpaque,
    std::deque<RefPtr<NativeLayer>>* aLayersToRecycle,
    IntRegion* aWindowInvalidRegion) {
  RefPtr<NativeLayer> layer;
  if (aLayersToRecycle->empty() ||
      aLayersToRecycle->front()->GetSize() != aRect.Size() ||
      aLayersToRecycle->front()->IsOpaque() != aOpaque) {
    layer = mNativeLayerRoot->CreateLayer(aRect.Size(), aOpaque,
                                          mSurfacePoolHandle);
    mNativeLayerRoot->AppendLayer(layer);
    aWindowInvalidRegion->OrWith(aRect);
  } else {
    layer = aLayersToRecycle->front();
    aLayersToRecycle->pop_front();
    IntRect oldRect = layer->GetRect();
    if (!aRect.IsEqualInterior(oldRect)) {
      aWindowInvalidRegion->OrWith(oldRect);
      aWindowInvalidRegion->OrWith(aRect);
    }
  }
  layer->SetPosition(aRect.TopLeft());
  mNativeLayers.push_back(layer);
}

// Used to clear the 'mLayerComposited' flag at the beginning of each Render().
static void ClearLayerFlags(Layer* aLayer) {
  ForEachNode<ForwardIterator>(aLayer, [](Layer* layer) {
    if (layer->AsHostLayer()) {
      static_cast<LayerComposite*>(layer->AsHostLayer())
          ->SetLayerComposited(false);
    }
  });
}

bool LayerManagerComposite::Render(const nsIntRegion& aInvalidRegion,
                                   const nsIntRegion& aOpaqueRegion) {
  AUTO_PROFILER_LABEL("LayerManagerComposite::Render", GRAPHICS);

  if (mDestroyed || !mCompositor || mCompositor->IsDestroyed()) {
    NS_WARNING("Call on destroyed layer manager");
    return false;
  }

  mCompositor->RequestAllowFrameRecording(!!mCompositionRecorder);

  ClearLayerFlags(mRoot);

  // At this time, it doesn't really matter if these preferences change
  // during the execution of the function; we should be safe in all
  // permutations. However, may as well just get the values onces and
  // then use them, just in case the consistency becomes important in
  // the future.
  bool invertVal = StaticPrefs::layers_effect_invert();
  bool grayscaleVal = StaticPrefs::layers_effect_grayscale();
  float contrastVal = StaticPrefs::layers_effect_contrast();
  bool haveLayerEffects = (invertVal || grayscaleVal || contrastVal != 0.0);

  // Set LayerScope begin/end frame
  LayerScopeAutoFrame frame(PR_Now());

  // If you're looking for the code to dump the layer tree, it was moved
  // to CompositorBridgeParent::CompositeToTarget().

  // Dump to LayerScope Viewer
  if (LayerScope::CheckSendable()) {
    // Create a LayersPacket, dump Layers into it and transfer the
    // packet('s ownership) to LayerScope.
    auto packet = MakeUnique<layerscope::Packet>();
    layerscope::LayersPacket* layersPacket = packet->mutable_layers();
    this->Dump(layersPacket);
    LayerScope::SendLayerDump(std::move(packet));
  }

  mozilla::widget::WidgetRenderingContext widgetContext;
#if defined(XP_MACOSX)
  if (CompositorOGL* compositorOGL = mCompositor->AsCompositorOGL()) {
    widgetContext.mGL = compositorOGL->gl();
  }
#endif

  {
    AUTO_PROFILER_LABEL("LayerManagerComposite::Render:Prerender", GRAPHICS);

    if (!mCompositor->GetWidget()->PreRender(&widgetContext)) {
      return false;
    }
  }

  CompositorBench(mCompositor, mRenderBounds);

  MOZ_ASSERT(mRoot->GetOpacity() == 1);
#if defined(MOZ_WIDGET_ANDROID)
  LayerMetricsWrapper wrapper = GetRootContentLayer();
  if (wrapper) {
    mCompositor->SetClearColor(wrapper.Metadata().GetBackgroundColor());
  } else {
    mCompositor->SetClearColorToDefault();
  }
#endif

  Maybe<IntRect> rootLayerClip = mRoot->GetClipRect().map(
      [](const ParentLayerIntRect& r) { return r.ToUnknownRect(); });
  Maybe<IntRect> maybeBounds;
  bool usingNativeLayers = false;
  if (mTarget) {
    maybeBounds = mCompositor->BeginFrameForTarget(
        aInvalidRegion, rootLayerClip, mRenderBounds, aOpaqueRegion, mTarget,
        mTargetBounds);
  } else if (mNativeLayerRoot) {
    mSurfacePoolHandle->OnBeginFrame();
    if (aInvalidRegion.Intersects(mRenderBounds)) {
      mCompositor->BeginFrameForNativeLayers();
      maybeBounds = Some(mRenderBounds);
      usingNativeLayers = true;
    }
  } else {
    maybeBounds = mCompositor->BeginFrameForWindow(
        aInvalidRegion, rootLayerClip, mRenderBounds, aOpaqueRegion);
  }

  if (!maybeBounds) {
    mProfilerScreenshotGrabber.NotifyEmptyFrame();
    mCompositor->GetWidget()->PostRender(&widgetContext);

    // Discard the current payloads. These payloads did not require a composite
    // (they caused no changes to anything visible), so we don't want to measure
    // their latency.
    mPayload.Clear();

    return true;
  }

  IntRect bounds = *maybeBounds;
  IntRect clipRect = rootLayerClip.valueOr(bounds);

  // Prepare our layers.
  {
    Diagnostics::Record record(mRenderStartTime);
    RootLayer()->Prepare(RenderTargetIntRect::FromUnknownRect(clipRect));
    if (record.Recording()) {
      mDiagnostics->RecordPrepareTime(record.Duration());
    }
  }

  auto RenderOnce = [&](const IntRect& aClipRect) {
    RefPtr<CompositingRenderTarget> previousTarget;
    if (haveLayerEffects) {
      previousTarget = PushGroupForLayerEffects();
    } else {
      mTwoPassTmpTarget = nullptr;
    }

    // Execute draw commands.
    RootLayer()->RenderLayer(aClipRect, Nothing());

    if (mTwoPassTmpTarget) {
      MOZ_ASSERT(haveLayerEffects);
      PopGroupForLayerEffects(previousTarget, aClipRect, grayscaleVal,
                              invertVal, contrastVal);
    }
    if (!mRegionToClear.IsEmpty()) {
      for (auto iter = mRegionToClear.RectIter(); !iter.Done(); iter.Next()) {
        mCompositor->ClearRect(Rect(iter.Get()));
      }
    }
    mCompositor->NormalDrawingDone();
  };

  {
    Diagnostics::Record record;

    if (usingNativeLayers) {
      // Update the placement of our native layers, so that transparent and
      // opaque parts of the window are covered by different layers and we can
      // update those parts separately.
      IntRegion opaqueRegion;
      opaqueRegion.And(aOpaqueRegion, mRenderBounds);

      // Limit the complexity of these regions. Usually, opaqueRegion should be
      // only one or two rects, so this SimplifyInward call will not change the
      // region if everything looks as expected.
      opaqueRegion.SimplifyInward(4);

      IntRegion transparentRegion;
      transparentRegion.Sub(mRenderBounds, opaqueRegion);
      std::deque<RefPtr<NativeLayer>> layersToRecycle =
          std::move(mNativeLayers);
      IntRegion invalidRegion = aInvalidRegion;
      PlaceNativeLayers(opaqueRegion, true, &layersToRecycle, &invalidRegion);
      PlaceNativeLayers(transparentRegion, false, &layersToRecycle,
                        &invalidRegion);
      for (const auto& unusedLayer : layersToRecycle) {
        mNativeLayerRoot->RemoveLayer(unusedLayer);
      }

      for (const auto& nativeLayer : mNativeLayers) {
        Maybe<IntRect> maybeLayerRect =
            mCompositor->BeginRenderingToNativeLayer(
                invalidRegion, rootLayerClip, aOpaqueRegion, nativeLayer);
        if (!maybeLayerRect) {
          continue;
        }

        if (rootLayerClip) {
          RenderOnce(rootLayerClip->Intersect(*maybeLayerRect));
        } else {
          RenderOnce(*maybeLayerRect);
        }
        mCompositor->EndRenderingToNativeLayer();
      }
    } else {
      RenderOnce(clipRect);
    }

    if (record.Recording()) {
      mDiagnostics->RecordCompositeTime(record.Duration());
    }
  }

  RootLayer()->Cleanup();

  WindowLMC window(mCompositor);
  mProfilerScreenshotGrabber.MaybeGrabScreenshot(window, bounds.Size());

  if (mCompositionRecorder) {
    bool hasContentPaint = std::any_of(
        mPayload.begin(), mPayload.end(), [](CompositionPayload& payload) {
          return payload.mType == CompositionPayloadType::eContentPaint;
        });

    if (hasContentPaint) {
      if (RefPtr<RecordedFrame> frame =
              mCompositor->RecordFrame(TimeStamp::Now())) {
        mCompositionRecorder->RecordFrame(frame);
      }
    }
  }

  if (usingNativeLayers) {
    UpdateDebugOverlayNativeLayers();
  } else {
#if defined(MOZ_WIDGET_ANDROID)
    HandlePixelsTarget();
#endif  // defined(MOZ_WIDGET_ANDROID)

    // Debugging
    RenderDebugOverlay(bounds);
  }

  {
    AUTO_PROFILER_LABEL("LayerManagerComposite::Render:EndFrame", GRAPHICS);

    mCompositor->EndFrame();

    if (usingNativeLayers) {
      mNativeLayerRoot->CommitToScreen();
    }
  }

  mCompositor->GetWidget()->PostRender(&widgetContext);

  mProfilerScreenshotGrabber.MaybeProcessQueue();

  RecordFrame();

  PayloadPresented();

  // Our payload has now been presented.
  mPayload.Clear();

  if (usingNativeLayers) {
    mSurfacePoolHandle->OnEndFrame();
  }

  mCompositor->WaitForGPU();

  return true;
}

#if defined(MOZ_WIDGET_ANDROID)
class ScopedCompostitorSurfaceSize {
 public:
  ScopedCompostitorSurfaceSize(CompositorOGL* aCompositor,
                               const gfx::IntSize& aSize)
      : mCompositor(aCompositor),
        mOriginalSize(mCompositor->GetDestinationSurfaceSize()) {
    mCompositor->SetDestinationSurfaceSize(aSize);
  }
  ~ScopedCompostitorSurfaceSize() {
    mCompositor->SetDestinationSurfaceSize(mOriginalSize);
  }

 private:
  CompositorOGL* const mCompositor;
  const gfx::IntSize mOriginalSize;
};

class ScopedContextSurfaceOverride {
 public:
  ScopedContextSurfaceOverride(GLContextEGL* aContext, void* aSurface)
      : mContext(aContext) {
    MOZ_ASSERT(aSurface);
    mContext->SetEGLSurfaceOverride(aSurface);
    mContext->MakeCurrent(true);
  }
  ~ScopedContextSurfaceOverride() {
    mContext->SetEGLSurfaceOverride(EGL_NO_SURFACE);
    mContext->MakeCurrent(true);
  }

 private:
  GLContextEGL* const mContext;
};

void LayerManagerComposite::RenderToPresentationSurface() {
  if (!mCompositor) {
    return;
  }

  widget::CompositorWidget* const widget = mCompositor->GetWidget();

  if (!widget) {
    return;
  }

  ANativeWindow* window = widget->AsAndroid()->GetPresentationANativeWindow();

  if (!window) {
    return;
  }

  CompositorOGL* compositor = mCompositor->AsCompositorOGL();
  GLContext* gl = compositor->gl();
  GLContextEGL* egl = GLContextEGL::Cast(gl);

  if (!egl) {
    return;
  }

  EGLSurface surface = widget->AsAndroid()->GetPresentationEGLSurface();

  if (!surface) {
    // create surface;
    surface = egl->CreateCompatibleSurface(window);
    if (!surface) {
      return;
    }

    widget->AsAndroid()->SetPresentationEGLSurface(surface);
  }

  const IntSize windowSize(ANativeWindow_getWidth(window),
                           ANativeWindow_getHeight(window));

  if ((windowSize.width <= 0) || (windowSize.height <= 0)) {
    return;
  }

  ScreenRotation rotation = compositor->GetScreenRotation();

  const int actualWidth = windowSize.width;
  const int actualHeight = windowSize.height;

  const gfx::IntSize originalSize = compositor->GetDestinationSurfaceSize();
  const nsIntRect originalRect =
      nsIntRect(0, 0, originalSize.width, originalSize.height);

  int pageWidth = originalSize.width;
  int pageHeight = originalSize.height;
  if (rotation == ROTATION_90 || rotation == ROTATION_270) {
    pageWidth = originalSize.height;
    pageHeight = originalSize.width;
  }

  float scale = 1.0;

  if ((pageWidth > actualWidth) || (pageHeight > actualHeight)) {
    const float scaleWidth = (float)actualWidth / (float)pageWidth;
    const float scaleHeight = (float)actualHeight / (float)pageHeight;
    scale = scaleWidth <= scaleHeight ? scaleWidth : scaleHeight;
  }

  const gfx::IntSize actualSize(actualWidth, actualHeight);
  ScopedCompostitorSurfaceSize overrideSurfaceSize(compositor, actualSize);

  const ScreenPoint offset((actualWidth - (int)(scale * pageWidth)) / 2, 0);
  ScopedContextSurfaceOverride overrideSurface(egl, surface);

  Matrix viewMatrix = ComputeTransformForRotation(originalRect, rotation);
  viewMatrix.Invert();  // unrotate
  viewMatrix.PostScale(scale, scale);
  viewMatrix.PostTranslate(offset.x, offset.y);
  Matrix4x4 matrix = Matrix4x4::From2D(viewMatrix);

  mRoot->ComputeEffectiveTransforms(matrix);
  nsIntRegion opaque;
  PostProcessLayers(opaque);

  nsIntRegion invalid;
  IntRect bounds = IntRect::Truncate(0, 0, scale * pageWidth, actualHeight);
  MOZ_ASSERT(mRoot->GetOpacity() == 1);
  Unused << mCompositor->BeginFrameForWindow(invalid, Nothing(), bounds,
                                             nsIntRegion());

  // The Java side of Fennec sets a scissor rect that accounts for
  // chrome such as the URL bar. Override that so that the entire frame buffer
  // is cleared.
  ScopedScissorRect scissorRect(egl, 0, 0, actualWidth, actualHeight);
  egl->fClearColor(0.0, 0.0, 0.0, 0.0);
  egl->fClear(LOCAL_GL_COLOR_BUFFER_BIT);

  const IntRect clipRect = IntRect::Truncate(0, 0, actualWidth, actualHeight);

  RootLayer()->Prepare(RenderTargetIntRect::FromUnknownRect(clipRect));
  RootLayer()->RenderLayer(clipRect, Nothing());

  mCompositor->EndFrame();
}

// Used by robocop tests to get a snapshot of the frame buffer.
void LayerManagerComposite::HandlePixelsTarget() {
  if (!mScreenPixelsTarget) {
    return;
  }

  int32_t bufferWidth = mRenderBounds.width;
  int32_t bufferHeight = mRenderBounds.height;
  ipc::Shmem mem;
  if (!mScreenPixelsTarget->AllocPixelBuffer(
          bufferWidth * bufferHeight * sizeof(uint32_t), &mem)) {
    // Failed to alloc shmem, Just bail out.
    return;
  }
  CompositorOGL* compositor = mCompositor->AsCompositorOGL();
  GLContext* gl = compositor->gl();
  MOZ_ASSERT(gl);
  gl->fReadPixels(0, 0, bufferWidth, bufferHeight, LOCAL_GL_RGBA,
                  LOCAL_GL_UNSIGNED_BYTE, mem.get<uint8_t>());
  Unused << mScreenPixelsTarget->SendScreenPixels(
      std::move(mem), ScreenIntSize(bufferWidth, bufferHeight), true);
  mScreenPixelsTarget = nullptr;
}
#endif

already_AddRefed<PaintedLayer> LayerManagerComposite::CreatePaintedLayer() {
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return RefPtr<PaintedLayer>(new PaintedLayerComposite(this)).forget();
}

already_AddRefed<ContainerLayer> LayerManagerComposite::CreateContainerLayer() {
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return RefPtr<ContainerLayer>(new ContainerLayerComposite(this)).forget();
}

already_AddRefed<ImageLayer> LayerManagerComposite::CreateImageLayer() {
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return RefPtr<ImageLayer>(new ImageLayerComposite(this)).forget();
}

already_AddRefed<ColorLayer> LayerManagerComposite::CreateColorLayer() {
  if (LayerManagerComposite::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return RefPtr<ColorLayer>(new ColorLayerComposite(this)).forget();
}

already_AddRefed<CanvasLayer> LayerManagerComposite::CreateCanvasLayer() {
  if (LayerManagerComposite::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return RefPtr<CanvasLayer>(new CanvasLayerComposite(this)).forget();
}

already_AddRefed<RefLayer> LayerManagerComposite::CreateRefLayer() {
  if (LayerManagerComposite::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return RefPtr<RefLayer>(new RefLayerComposite(this)).forget();
}

LayerManagerComposite::AutoAddMaskEffect::AutoAddMaskEffect(
    Layer* aMaskLayer, EffectChain& aEffects)
    : mCompositable(nullptr), mFailed(false) {
  if (!aMaskLayer) {
    return;
  }

  mCompositable = ToLayerComposite(aMaskLayer)->GetCompositableHost();
  if (!mCompositable) {
    NS_WARNING("Mask layer with no compositable host");
    mFailed = true;
    return;
  }

  if (!mCompositable->AddMaskEffect(aEffects,
                                    aMaskLayer->GetEffectiveTransform())) {
    mCompositable = nullptr;
    mFailed = true;
  }
}

LayerManagerComposite::AutoAddMaskEffect::~AutoAddMaskEffect() {
  if (!mCompositable) {
    return;
  }

  mCompositable->RemoveMaskEffect();
}

bool LayerManagerComposite::IsCompositingToScreen() const { return !mTarget; }

LayerComposite::LayerComposite(LayerManagerComposite* aManager)
    : HostLayer(aManager),
      mCompositeManager(aManager),
      mCompositor(aManager->GetCompositor()),
      mDestroyed(false),
      mLayerComposited(false) {}

LayerComposite::~LayerComposite() = default;

void LayerComposite::Destroy() {
  if (!mDestroyed) {
    mDestroyed = true;
    CleanupResources();
  }
}

void LayerComposite::AddBlendModeEffect(EffectChain& aEffectChain) {
  gfx::CompositionOp blendMode = GetLayer()->GetEffectiveMixBlendMode();
  if (blendMode == gfx::CompositionOp::OP_OVER) {
    return;
  }

  aEffectChain.mSecondaryEffects[EffectTypes::BLEND_MODE] =
      new EffectBlendMode(blendMode);
}

bool LayerManagerComposite::CanUseCanvasLayerForSize(const IntSize& aSize) {
  return mCompositor->CanUseCanvasLayerForSize(
      gfx::IntSize(aSize.width, aSize.height));
}

void LayerManagerComposite::NotifyShadowTreeTransaction() {
  if (StaticPrefs::layers_acceleration_draw_fps()) {
    mDiagnostics->AddTxnFrame();
  }
}

void LayerComposite::SetLayerManager(HostLayerManager* aManager) {
  HostLayer::SetLayerManager(aManager);
  mCompositeManager = static_cast<LayerManagerComposite*>(aManager);
  mCompositor = mCompositeManager->GetCompositor();
}

bool LayerManagerComposite::AsyncPanZoomEnabled() const {
  if (CompositorBridgeParent* bridge =
          mCompositor->GetCompositorBridgeParent()) {
    return bridge->GetOptions().UseAPZ();
  }
  return false;
}

bool LayerManagerComposite::AlwaysScheduleComposite() const {
  return !!(mCompositor->GetDiagnosticTypes() & DiagnosticTypes::FLASH_BORDERS);
}

nsIntRegion LayerComposite::GetFullyRenderedRegion() {
  if (TiledContentHost* tiled =
          GetCompositableHost() ? GetCompositableHost()->AsTiledContentHost()
                                : nullptr) {
    nsIntRegion shadowVisibleRegion =
        GetShadowVisibleRegion().ToUnknownRegion();
    // Discard the region which hasn't been drawn yet when doing
    // progressive drawing. Note that if the shadow visible region
    // shrunk the tiled valig region may not have discarded this yet.
    shadowVisibleRegion.And(shadowVisibleRegion, tiled->GetValidRegion());
    return shadowVisibleRegion;
  } else {
    return GetShadowVisibleRegion().ToUnknownRegion();
  }
}

Matrix4x4 HostLayer::GetShadowTransform() {
  Matrix4x4 transform = mShadowTransform;
  Layer* layer = GetLayer();

  transform.PostScale(layer->GetPostXScale(), layer->GetPostYScale(), 1.0f);
  if (const ContainerLayer* c = layer->AsContainerLayer()) {
    transform.PreScale(c->GetPreXScale(), c->GetPreYScale(), 1.0f);
  }

  return transform;
}

// Async animations can move child layers without updating our visible region.
// PostProcessLayers will recompute visible regions for layers with an
// intermediate surface, but otherwise we need to do it now.
static void ComputeVisibleRegionForChildren(ContainerLayer* aContainer,
                                            LayerIntRegion& aResult) {
  for (Layer* l = aContainer->GetFirstChild(); l; l = l->GetNextSibling()) {
    if (l->Extend3DContext()) {
      MOZ_ASSERT(l->AsContainerLayer());
      ComputeVisibleRegionForChildren(l->AsContainerLayer(), aResult);
    } else {
      AddTransformedRegion(aResult, l->GetLocalVisibleRegion(),
                           l->ComputeTransformToPreserve3DRoot());
    }
  }
}

void HostLayer::RecomputeShadowVisibleRegionFromChildren() {
  mShadowVisibleRegion.SetEmpty();
  ContainerLayer* container = GetLayer()->AsContainerLayer();
  MOZ_ASSERT(container);
  // Layers that extend a 3d context have a local visible region
  // that can only be represented correctly in 3d space. Since
  // we can't do that, leave it empty instead to stop anyone
  // from trying to use it.
  NS_ASSERTION(
      !GetLayer()->Extend3DContext(),
      "Can't compute visible region for layers that extend a 3d context");
  if (container && !GetLayer()->Extend3DContext()) {
    ComputeVisibleRegionForChildren(container, mShadowVisibleRegion);
  }
}

bool LayerComposite::HasStaleCompositor() const {
  return mCompositeManager->GetCompositor() != mCompositor;
}

#ifndef MOZ_HAVE_PLATFORM_SPECIFIC_LAYER_BUFFERS

/*static*/
bool LayerManagerComposite::SupportsDirectTexturing() { return false; }

/*static*/
void LayerManagerComposite::PlatformSyncBeforeReplyUpdate() {}

#endif  // !defined(MOZ_HAVE_PLATFORM_SPECIFIC_LAYER_BUFFERS)

class RenderSourceLMC : public profiler_screenshots::RenderSource {
 public:
  explicit RenderSourceLMC(CompositingRenderTarget* aRT)
      : RenderSource(aRT->GetSize()), mRT(aRT) {}

  const auto& RenderTarget() { return mRT; }

 protected:
  virtual ~RenderSourceLMC() {}

  RefPtr<CompositingRenderTarget> mRT;
};

class DownscaleTargetLMC : public profiler_screenshots::DownscaleTarget {
 public:
  explicit DownscaleTargetLMC(CompositingRenderTarget* aRT,
                              Compositor* aCompositor)
      : profiler_screenshots::DownscaleTarget(aRT->GetSize()),
        mRenderSource(new RenderSourceLMC(aRT)),
        mCompositor(aCompositor) {}

  already_AddRefed<profiler_screenshots::RenderSource> AsRenderSource()
      override {
    return do_AddRef(mRenderSource);
  }

  bool DownscaleFrom(profiler_screenshots::RenderSource* aSource,
                     const IntRect& aSourceRect,
                     const IntRect& aDestRect) override {
    MOZ_RELEASE_ASSERT(aSourceRect.TopLeft() == IntPoint());
    MOZ_RELEASE_ASSERT(aDestRect.TopLeft() == IntPoint());
    RefPtr<CompositingRenderTarget> previousTarget =
        mCompositor->GetCurrentRenderTarget();

    mCompositor->SetRenderTarget(mRenderSource->RenderTarget());
    bool result = mCompositor->BlitRenderTarget(
        static_cast<RenderSourceLMC*>(aSource)->RenderTarget(),
        aSourceRect.Size(), aDestRect.Size());

    // Restore the old render target.
    mCompositor->SetRenderTarget(previousTarget);

    return result;
  }

 protected:
  virtual ~DownscaleTargetLMC() {}

  RefPtr<RenderSourceLMC> mRenderSource;
  Compositor* mCompositor;
};

class AsyncReadbackBufferLMC
    : public profiler_screenshots::AsyncReadbackBuffer {
 public:
  AsyncReadbackBufferLMC(mozilla::layers::AsyncReadbackBuffer* aARB,
                         Compositor* aCompositor)
      : profiler_screenshots::AsyncReadbackBuffer(aARB->GetSize()),
        mARB(aARB),
        mCompositor(aCompositor) {}
  void CopyFrom(profiler_screenshots::RenderSource* aSource) override {
    mCompositor->ReadbackRenderTarget(
        static_cast<RenderSourceLMC*>(aSource)->RenderTarget(), mARB);
  }
  bool MapAndCopyInto(DataSourceSurface* aSurface,
                      const IntSize& aReadSize) override {
    return mARB->MapAndCopyInto(aSurface, aReadSize);
  }

 protected:
  virtual ~AsyncReadbackBufferLMC() {}

  RefPtr<mozilla::layers::AsyncReadbackBuffer> mARB;
  Compositor* mCompositor;
};

already_AddRefed<profiler_screenshots::RenderSource>
WindowLMC::GetWindowContents(const IntSize& aWindowSize) {
  RefPtr<CompositingRenderTarget> rt = mCompositor->GetWindowRenderTarget();
  if (!rt) {
    return nullptr;
  }
  return MakeAndAddRef<RenderSourceLMC>(rt);
}

already_AddRefed<profiler_screenshots::DownscaleTarget>
WindowLMC::CreateDownscaleTarget(const IntSize& aSize) {
  RefPtr<CompositingRenderTarget> rt =
      mCompositor->CreateRenderTarget(IntRect({}, aSize), INIT_MODE_NONE);
  return MakeAndAddRef<DownscaleTargetLMC>(rt, mCompositor);
}

already_AddRefed<profiler_screenshots::AsyncReadbackBuffer>
WindowLMC::CreateAsyncReadbackBuffer(const IntSize& aSize) {
  RefPtr<AsyncReadbackBuffer> carb =
      mCompositor->CreateAsyncReadbackBuffer(aSize);
  if (!carb) {
    return nullptr;
  }
  return MakeAndAddRef<AsyncReadbackBufferLMC>(carb, mCompositor);
}

}  // namespace layers
}  // namespace mozilla
