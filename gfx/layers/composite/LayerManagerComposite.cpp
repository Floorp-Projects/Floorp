/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayerManagerComposite.h"
#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint16_t, uint32_t
#include "CanvasLayerComposite.h"       // for CanvasLayerComposite
#include "ColorLayerComposite.h"        // for ColorLayerComposite
#include "CompositableHost.h"           // for CompositableHost
#include "ContainerLayerComposite.h"    // for ContainerLayerComposite, etc
#include "Diagnostics.h"
#include "FPSCounter.h"                 // for FPSState, FPSCounter
#include "FrameMetrics.h"               // for FrameMetrics
#include "GeckoProfiler.h"              // for profiler_set_frame_number, etc
#include "ImageLayerComposite.h"        // for ImageLayerComposite
#include "Layers.h"                     // for Layer, ContainerLayer, etc
#include "LayerScope.h"                 // for LayerScope Tool
#include "protobuf/LayerScopePacket.pb.h" // for protobuf (LayerScope)
#include "PaintedLayerComposite.h"      // for PaintedLayerComposite
#include "TiledContentHost.h"
#include "Units.h"                      // for ScreenIntRect
#include "UnitTransforms.h"             // for ViewAs
#include "apz/src/AsyncPanZoomController.h"  // for AsyncPanZoomController
#include "gfxPrefs.h"                   // for gfxPrefs
#ifdef XP_MACOSX
#include "gfxPlatformMac.h"
#endif
#include "gfxRect.h"                    // for gfxRect
#include "gfxUtils.h"                   // for frame color util
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/RefPtr.h"             // for RefPtr, already_AddRefed
#include "mozilla/gfx/2D.h"             // for DrawTarget
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/gfx/Point.h"          // for IntSize, Point
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/gfx/Types.h"          // for Color, SurfaceFormat
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/Effects.h"     // for Effect, EffectChain, etc
#include "mozilla/layers/LayerMetricsWrapper.h" // for LayerMetricsWrapper
#include "mozilla/layers/LayersTypes.h"  // for etc
#include "mozilla/widget/CompositorWidget.h" // for WidgetRenderingContext
#include "ipc/CompositorBench.h"        // for CompositorBench
#include "ipc/ShadowLayerUtils.h"
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "nsAppRunner.h"
#include "mozilla/RefPtr.h"                   // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_WARNING, NS_RUNTIMEABORT, etc
#include "nsISupportsImpl.h"            // for Layer::AddRef, etc
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for mozilla::gfx::IntRect
#include "nsRegion.h"                   // for nsIntRegion, etc
#if defined(MOZ_WIDGET_ANDROID)
#include <android/log.h>
#include <android/native_window.h>
#include "mozilla/widget/AndroidCompositorWidget.h"
#include "opengl/CompositorOGL.h"
#include "GLConsts.h"
#include "GLContextEGL.h"
#include "GLContextProvider.h"
#include "mozilla/Unused.h"
#include "mozilla/widget/AndroidCompositorWidget.h"
#include "ScopedGLHelpers.h"
#endif
#include "GeckoProfiler.h"
#include "TextRenderer.h"               // for TextRenderer
#include "mozilla/layers/CompositorBridgeParent.h"
#include "TreeTraversal.h"              // for ForEachNode

#ifdef USE_SKIA
#include "PaintCounter.h"               // For PaintCounter
#endif

class gfxContext;

namespace mozilla {
namespace layers {

class ImageLayer;

using namespace mozilla::gfx;
using namespace mozilla::gl;

static LayerComposite*
ToLayerComposite(Layer* aLayer)
{
  return static_cast<LayerComposite*>(aLayer->ImplData());
}

static void ClearSubtree(Layer* aLayer)
{
  ForEachNode<ForwardIterator>(
      aLayer,
      [] (Layer* layer)
      {
        ToLayerComposite(layer)->CleanupResources();
      });
}

void
LayerManagerComposite::ClearCachedResources(Layer* aSubtree)
{
  MOZ_ASSERT(!aSubtree || aSubtree->Manager() == this);
  Layer* subtree = aSubtree ? aSubtree : mRoot.get();
  if (!subtree) {
    return;
  }

  ClearSubtree(subtree);
  // FIXME [bjacob]
  // XXX the old LayerManagerOGL code had a mMaybeInvalidTree that it set to true here.
  // Do we need that?
}

HostLayerManager::HostLayerManager()
  : mDebugOverlayWantsNextFrame(false)
  , mWarningLevel(0.0f)
  , mWindowOverlayChanged(false)
  , mLastPaintTime(TimeDuration::Forever())
  , mRenderStartTime(TimeStamp::Now())
{}

HostLayerManager::~HostLayerManager()
{}

void
HostLayerManager::RecordPaintTimes(const PaintTiming& aTiming)
{
  mDiagnostics->RecordPaintTimes(aTiming);
}

void
HostLayerManager::RecordUpdateTime(float aValue)
{
  mDiagnostics->RecordUpdateTime(aValue);
}

/**
 * LayerManagerComposite
 */
LayerManagerComposite::LayerManagerComposite(Compositor* aCompositor)
: mUnusedApzTransformWarning(false)
, mDisabledApzWarning(false)
, mCompositor(aCompositor)
, mInTransaction(false)
, mIsCompositorReady(false)
, mGeometryChanged(true)
#if defined(MOZ_WIDGET_ANDROID)
, mScreenPixelsTarget(nullptr)
#endif // defined(MOZ_WIDGET_ANDROID)
{
  mTextRenderer = new TextRenderer();
  mDiagnostics = MakeUnique<Diagnostics>();
  MOZ_ASSERT(aCompositor);

#ifdef USE_SKIA
  mPaintCounter = nullptr;
#endif
}

LayerManagerComposite::~LayerManagerComposite()
{
  Destroy();
}


void
LayerManagerComposite::Destroy()
{
  if (!mDestroyed) {
    mCompositor->GetWidget()->CleanupWindowEffects();
    if (mRoot) {
      RootLayer()->Destroy();
    }
    mCompositor->CancelFrame();
    mRoot = nullptr;
    mClonedLayerTreeProperties = nullptr;
    mDestroyed = true;

#ifdef USE_SKIA
    mPaintCounter = nullptr;
#endif
  }
}

void
LayerManagerComposite::UpdateRenderBounds(const IntRect& aRect)
{
  mRenderBounds = aRect;
}

bool
LayerManagerComposite::AreComponentAlphaLayersEnabled()
{
  return mCompositor->GetBackendType() != LayersBackend::LAYERS_BASIC &&
         mCompositor->SupportsEffect(EffectTypes::COMPONENT_ALPHA) &&
         LayerManager::AreComponentAlphaLayersEnabled();
}

bool
LayerManagerComposite::BeginTransaction()
{
  mInTransaction = true;

  if (!mCompositor->Ready()) {
    return false;
  }

  mIsCompositorReady = true;
  return true;
}

void
LayerManagerComposite::BeginTransactionWithDrawTarget(DrawTarget* aTarget, const IntRect& aRect)
{
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
  mCompositor->SetTargetContext(aTarget, aRect);
  mTarget = aTarget;
  mTargetBounds = aRect;
}

/**
 * Get accumulated transform of from the context creating layer to the
 * given layer.
 */
static Matrix4x4
GetAccTransformIn3DContext(Layer* aLayer) {
  Matrix4x4 transform = aLayer->GetLocalTransform();
  for (Layer* layer = aLayer->GetParent();
       layer && layer->Extend3DContext();
       layer = layer->GetParent()) {
    transform = transform * layer->GetLocalTransform();
  }
  return transform;
}

void
LayerManagerComposite::PostProcessLayers(Layer* aLayer,
                                         nsIntRegion& aOpaqueRegion,
                                         LayerIntRegion& aVisibleRegion,
                                         const Maybe<ParentLayerIntRect>& aClipFromAncestors)
{
  if (aLayer->Extend3DContext()) {
    // For layers participating 3D rendering context, their visible
    // region should be empty (invisible), so we pass through them
    // without doing anything.

    // Direct children of the establisher may have a clip, becaue the
    // item containing it; ex. of nsHTMLScrollFrame, may give it one.
    Maybe<ParentLayerIntRect> layerClip =
      aLayer->AsHostLayer()->GetShadowClipRect();
    Maybe<ParentLayerIntRect> ancestorClipForChildren =
      IntersectMaybeRects(layerClip, aClipFromAncestors);
    MOZ_ASSERT(!layerClip || !aLayer->Combines3DTransformWithAncestors(),
               "Only direct children of the establisher could have a clip");

    for (Layer* child = aLayer->GetLastChild();
         child;
         child = child->GetPrevSibling()) {
      PostProcessLayers(child, aOpaqueRegion, aVisibleRegion,
                        ancestorClipForChildren);
    }
    return;
  }

  nsIntRegion localOpaque;
  // Treat layers on the path to the root of the 3D rendering context as
  // a giant layer if it is a leaf.
  Matrix4x4 transform = GetAccTransformIn3DContext(aLayer);
  Matrix transform2d;
  Maybe<IntPoint> integerTranslation;
  // If aLayer has a simple transform (only an integer translation) then we
  // can easily convert aOpaqueRegion into pre-transform coordinates and include
  // that region.
  if (transform.Is2D(&transform2d)) {
    if (transform2d.IsIntegerTranslation()) {
      integerTranslation = Some(IntPoint::Truncate(transform2d.GetTranslation()));
      localOpaque = aOpaqueRegion;
      localOpaque.MoveBy(-*integerTranslation);
    }
  }

  // Compute a clip that's the combination of our layer clip with the clip
  // from our ancestors.
  LayerComposite* composite = static_cast<LayerComposite*>(aLayer->AsHostLayer());
  Maybe<ParentLayerIntRect> layerClip = composite->GetShadowClipRect();
  MOZ_ASSERT(!layerClip || !aLayer->Combines3DTransformWithAncestors(),
             "The layer with a clip should not participate "
             "a 3D rendering context");
  Maybe<ParentLayerIntRect> outsideClip =
    IntersectMaybeRects(layerClip, aClipFromAncestors);

  // Convert the combined clip into our pre-transform coordinate space, so
  // that it can later be intersected with our visible region.
  // If our transform is a perspective, there's no meaningful insideClip rect
  // we can compute (it would need to be a cone).
  Maybe<LayerIntRect> insideClip;
  if (outsideClip && !transform.HasPerspectiveComponent()) {
    Matrix4x4 inverse = transform;
    if (inverse.Invert()) {
      Maybe<LayerRect> insideClipFloat =
        UntransformBy(ViewAs<ParentLayerToLayerMatrix4x4>(inverse),
                      ParentLayerRect(*outsideClip),
                      LayerRect::MaxIntRect());
      if (insideClipFloat) {
        insideClipFloat->RoundOut();
        LayerIntRect insideClipInt;
        if (insideClipFloat->ToIntRect(&insideClipInt)) {
          insideClip = Some(insideClipInt);
        }
      }
    }
  }

  Maybe<ParentLayerIntRect> ancestorClipForChildren;
  if (insideClip) {
    ancestorClipForChildren =
      Some(ViewAs<ParentLayerPixel>(*insideClip, PixelCastJustification::MovingDownToChildren));
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
  for (Layer* child = aLayer->GetLastChild(); child; child = child->GetPrevSibling()) {
    PostProcessLayers(child, localOpaque, descendantsVisibleRegion, ancestorClipForChildren);
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
  ParentLayerIntRegion visibleParentSpace = TransformBy(
      ViewAs<LayerToParentLayerMatrix4x4>(transform), visible);
  if (const Maybe<ParentLayerIntRect>& clipRect = composite->GetShadowClipRect()) {
    visibleParentSpace.AndWith(*clipRect);
  }
  aVisibleRegion.OrWith(ViewAs<LayerPixel>(visibleParentSpace,
      PixelCastJustification::MovingDownToChildren));

  // If we have a simple transform, then we can add our opaque area into
  // aOpaqueRegion.
  if (integerTranslation &&
      !aLayer->HasMaskLayers() &&
      aLayer->IsOpaqueForVisibility()) {
    if (aLayer->IsOpaque()) {
      localOpaque.OrWith(composite->GetFullyRenderedRegion());
    }
    localOpaque.MoveBy(*integerTranslation);
    if (layerClip) {
      localOpaque.AndWith(layerClip->ToUnknownRect());
    }
    aOpaqueRegion.OrWith(localOpaque);
  }
}

void
LayerManagerComposite::EndTransaction(const TimeStamp& aTimeStamp,
                                      EndTransactionFlags aFlags)
{
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
  } else {
    // Modified the layer tree.
    mGeometryChanged = true;
  }

  mCompositor->ClearTargetContext();
  mTarget = nullptr;

#ifdef MOZ_LAYERS_HAVE_LOG
  Log();
  MOZ_LAYERS_LOG(("]----- EndTransaction"));
#endif
}

void
LayerManagerComposite::UpdateAndRender()
{
  nsIntRegion invalid;
  bool didEffectiveTransforms = false;

  nsIntRegion opaque;
  LayerIntRegion visible;
  PostProcessLayers(mRoot, opaque, visible, Nothing());

  if (mClonedLayerTreeProperties) {
    // Effective transforms are needed by ComputeDifferences().
    mRoot->ComputeEffectiveTransforms(gfx::Matrix4x4());
    didEffectiveTransforms = true;

    // We need to compute layer tree differences even if we're not going to
    // immediately use the resulting damage area, since ComputeDifferences
    // is also responsible for invalidates intermediate surfaces in
    // ContainerLayers.
    nsIntRegion changed = mClonedLayerTreeProperties->ComputeDifferences(mRoot, nullptr, &mGeometryChanged);

    if (mTarget) {
      // Since we're composing to an external target, we're not going to use
      // the damage region from layers changes - we want to composite
      // everything in the target bounds. Instead we accumulate the layers
      // damage region for the next window composite.
      mInvalidRegion.Or(mInvalidRegion, changed);
    } else {
      invalid = Move(changed);
    }
  }

  if (mTarget) {
    invalid.Or(invalid, mTargetBounds);
  } else {
    // If we didn't have a previous layer tree, invalidate the entire render
    // area.
    if (!mClonedLayerTreeProperties) {
      invalid.Or(invalid, mRenderBounds);
    }

    // Add any additional invalid rects from the window manager or previous
    // damage computed during ComposeToTarget().
    invalid.Or(invalid, mInvalidRegion);
    mInvalidRegion.SetEmpty();
  }

  if (invalid.IsEmpty() && !mWindowOverlayChanged) {
    // Composition requested, but nothing has changed. Don't do any work.
    mClonedLayerTreeProperties = LayerProperties::CloneFrom(GetRoot());
    return;
  }

  // We don't want our debug overlay to cause more frames to happen
  // so we will invalidate after we've decided if something changed.
  InvalidateDebugOverlay(invalid, mRenderBounds);

  if (!didEffectiveTransforms) {
    // The results of our drawing always go directly into a pixel buffer,
    // so we don't need to pass any global transform here.
    mRoot->ComputeEffectiveTransforms(gfx::Matrix4x4());
  }

  Render(invalid, opaque);
#if defined(MOZ_WIDGET_ANDROID)
  RenderToPresentationSurface();
#endif
  mGeometryChanged = false;
  mWindowOverlayChanged = false;

  // Update cached layer tree information.
  mClonedLayerTreeProperties = LayerProperties::CloneFrom(GetRoot());
}

already_AddRefed<DrawTarget>
LayerManagerComposite::CreateOptimalMaskDrawTarget(const IntSize &aSize)
{
  MOZ_CRASH("Should only be called on the drawing side");
  return nullptr;
}

LayerComposite*
LayerManagerComposite::RootLayer() const
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }

  return ToLayerComposite(mRoot);
}

#ifdef MOZ_PROFILING
// Only build the QR feature when profiling to avoid bloating
// our data section.
// This table was generated using qrencode and is a binary
// encoding of the qrcodes 0-255.
#include "qrcode_table.h"
#endif

void
LayerManagerComposite::InvalidateDebugOverlay(nsIntRegion& aInvalidRegion, const IntRect& aBounds)
{
  bool drawFps = gfxPrefs::LayersDrawFPS();
  bool drawFrameCounter = gfxPrefs::DrawFrameCounter();
  bool drawFrameColorBars = gfxPrefs::CompositorDrawColorBars();

  if (drawFps || drawFrameCounter) {
    aInvalidRegion.Or(aInvalidRegion, nsIntRect(0, 0, 650, 400));
  }
  if (drawFrameColorBars) {
    aInvalidRegion.Or(aInvalidRegion, nsIntRect(0, 0, 10, aBounds.height));
  }

#ifdef USE_SKIA
  bool drawPaintTimes = gfxPrefs::AlwaysPaint();
  if (drawPaintTimes) {
    aInvalidRegion.Or(aInvalidRegion, nsIntRect(PaintCounter::GetPaintRect()));
  }
#endif
}

#ifdef USE_SKIA
void
LayerManagerComposite::DrawPaintTimes(Compositor* aCompositor)
{
  if (!mPaintCounter) {
    mPaintCounter = new PaintCounter();
  }

  TimeDuration compositeTime = TimeStamp::Now() - mRenderStartTime;
  mPaintCounter->Draw(aCompositor, mLastPaintTime, compositeTime);
}
#endif

static uint16_t sFrameCount = 0;
void
LayerManagerComposite::RenderDebugOverlay(const IntRect& aBounds)
{
  bool drawFps = gfxPrefs::LayersDrawFPS();
  bool drawFrameCounter = gfxPrefs::DrawFrameCounter();
  bool drawFrameColorBars = gfxPrefs::CompositorDrawColorBars();

  if (drawFps) {
    float alpha = 1;
#ifdef ANDROID
    // Draw a translation delay warning overlay
    int width;
    int border;

    TimeStamp now = TimeStamp::Now();
    if (!mWarnTime.IsNull() && (now - mWarnTime).ToMilliseconds() < kVisualWarningDuration) {
      EffectChain effects;

      // Black blorder
      border = 4;
      width = 6;
      effects.mPrimaryEffect = new EffectSolidColor(gfx::Color(0, 0, 0, 1));
      mCompositor->DrawQuad(gfx::Rect(border, border, aBounds.width - 2 * border, width),
                            aBounds, effects, alpha, gfx::Matrix4x4());
      mCompositor->DrawQuad(gfx::Rect(border, aBounds.height - border - width, aBounds.width - 2 * border, width),
                            aBounds, effects, alpha, gfx::Matrix4x4());
      mCompositor->DrawQuad(gfx::Rect(border, border + width, width, aBounds.height - 2 * border - width * 2),
                            aBounds, effects, alpha, gfx::Matrix4x4());
      mCompositor->DrawQuad(gfx::Rect(aBounds.width - border - width, border + width, width, aBounds.height - 2 * border - 2 * width),
                            aBounds, effects, alpha, gfx::Matrix4x4());

      // Content
      border = 5;
      width = 4;
      effects.mPrimaryEffect = new EffectSolidColor(gfx::Color(1, 1.f - mWarningLevel, 0, 1));
      mCompositor->DrawQuad(gfx::Rect(border, border, aBounds.width - 2 * border, width),
                            aBounds, effects, alpha, gfx::Matrix4x4());
      mCompositor->DrawQuad(gfx::Rect(border, aBounds.height - border - width, aBounds.width - 2 * border, width),
                            aBounds, effects, alpha, gfx::Matrix4x4());
      mCompositor->DrawQuad(gfx::Rect(border, border + width, width, aBounds.height - 2 * border - width * 2),
                            aBounds, effects, alpha, gfx::Matrix4x4());
      mCompositor->DrawQuad(gfx::Rect(aBounds.width - border - width, border + width, width, aBounds.height - 2 * border - 2 * width),
                            aBounds, effects, alpha, gfx::Matrix4x4());
      SetDebugOverlayWantsNextFrame(true);
    }
#endif

    GPUStats stats;
    stats.mScreenPixels = mRenderBounds.width * mRenderBounds.height;
    mCompositor->GetFrameStats(&stats);

    std::string text = mDiagnostics->GetFrameOverlayString(stats);
    mTextRenderer->RenderText(
      mCompositor,
      text,
      IntPoint(2, 5),
      Matrix4x4(),
      24,
      600,
      TextRenderer::FontType::FixedWidth);

    if (mUnusedApzTransformWarning) {
      // If we have an unused APZ transform on this composite, draw a 20x20 red box
      // in the top-right corner
      EffectChain effects;
      effects.mPrimaryEffect = new EffectSolidColor(gfx::Color(1, 0, 0, 1));
      mCompositor->DrawQuad(gfx::Rect(aBounds.width - 20, 0, 20, 20),
                            aBounds, effects, alpha, gfx::Matrix4x4());

      mUnusedApzTransformWarning = false;
      SetDebugOverlayWantsNextFrame(true);
    }
    if (mDisabledApzWarning) {
      // If we have a disabled APZ on this composite, draw a 20x20 yellow box
      // in the top-right corner, to the left of the unused-apz-transform
      // warning box
      EffectChain effects;
      effects.mPrimaryEffect = new EffectSolidColor(gfx::Color(1, 1, 0, 1));
      mCompositor->DrawQuad(gfx::Rect(aBounds.width - 40, 0, 20, 20),
                            aBounds, effects, alpha, gfx::Matrix4x4());

      mDisabledApzWarning = false;
      SetDebugOverlayWantsNextFrame(true);
    }
  }

  if (drawFrameColorBars) {
    gfx::IntRect sideRect(0, 0, 10, aBounds.height);

    EffectChain effects;
    effects.mPrimaryEffect = new EffectSolidColor(gfxUtils::GetColorForFrameNumber(sFrameCount));
    mCompositor->DrawQuad(Rect(sideRect),
                          sideRect,
                          effects,
                          1.0,
                          gfx::Matrix4x4());

  }

#ifdef MOZ_PROFILING
  if (drawFrameCounter) {
    profiler_set_frame_number(sFrameCount);
    const char* qr = sQRCodeTable[sFrameCount%256];

    int size = 21;
    int padding = 2;
    float opacity = 1.0;
    const uint16_t bitWidth = 5;
    gfx::IntRect clip(0,0, bitWidth*640, bitWidth*640);

    // Draw the white squares at once
    gfx::Color bitColor(1.0, 1.0, 1.0, 1.0);
    EffectChain effects;
    effects.mPrimaryEffect = new EffectSolidColor(bitColor);
    int totalSize = (size + padding * 2) * bitWidth;
    mCompositor->DrawQuad(gfx::Rect(0, 0, totalSize, totalSize),
                          clip,
                          effects,
                          opacity,
                          gfx::Matrix4x4());

    // Draw a black square for every bit set in qr[index]
    effects.mPrimaryEffect = new EffectSolidColor(gfx::Color(0, 0, 0, 1.0));
    for (int y = 0; y < size; y++) {
      for (int x = 0; x < size; x++) {
        // Select the right bit from the binary encoding
        int currBit = 128 >> ((x + y * 21) % 8);
        int i = (x + y * 21) / 8;
        if (qr[i] & currBit) {
          mCompositor->DrawQuad(gfx::Rect(bitWidth * (x + padding),
                                          bitWidth * (y + padding),
                                          bitWidth, bitWidth),
                                clip,
                                effects,
                                opacity,
                                gfx::Matrix4x4());
        }
      }
    }

  }
#endif

  if (drawFrameColorBars || drawFrameCounter) {
    // We intentionally overflow at 2^16.
    sFrameCount++;
  }

#ifdef USE_SKIA
  bool drawPaintTimes = gfxPrefs::AlwaysPaint();
  if (drawPaintTimes) {
    DrawPaintTimes(mCompositor);
  }
#endif
}

RefPtr<CompositingRenderTarget>
LayerManagerComposite::PushGroupForLayerEffects()
{
  // This is currently true, so just making sure that any new use of this
  // method is flagged for investigation
  MOZ_ASSERT(gfxPrefs::LayersEffectInvert() ||
             gfxPrefs::LayersEffectGrayscale() ||
             gfxPrefs::LayersEffectContrast() != 0.0);

  RefPtr<CompositingRenderTarget> previousTarget = mCompositor->GetCurrentRenderTarget();
  // make our render target the same size as the destination target
  // so that we don't have to change size if the drawing area changes.
  IntRect rect(previousTarget->GetOrigin(), previousTarget->GetSize());
  // XXX: I'm not sure if this is true or not...
  MOZ_ASSERT(rect.x == 0 && rect.y == 0);
  if (!mTwoPassTmpTarget ||
      mTwoPassTmpTarget->GetSize() != previousTarget->GetSize() ||
      mTwoPassTmpTarget->GetOrigin() != previousTarget->GetOrigin()) {
    mTwoPassTmpTarget = mCompositor->CreateRenderTarget(rect, INIT_MODE_NONE);
  }
  MOZ_ASSERT(mTwoPassTmpTarget);
  mCompositor->SetRenderTarget(mTwoPassTmpTarget);
  return previousTarget;
}
void
LayerManagerComposite::PopGroupForLayerEffects(RefPtr<CompositingRenderTarget> aPreviousTarget,
                                               IntRect aClipRect,
                                               bool aGrayscaleEffect,
                                               bool aInvertEffect,
                                               float aContrastEffect)
{
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
    Matrix5x4 grayscaleMatrix(0.2126f, 0.2126f, 0.2126f, 0,
                              0.7152f, 0.7152f, 0.7152f, 0,
                              0.0722f, 0.0722f, 0.0722f, 0,
                              0,       0,       0,       1,
                              0,       0,       0,       0);
    effectMatrix = grayscaleMatrix;
  }

  if (aInvertEffect) {
    // R' = 1 - R
    // G' = 1 - G
    // B' = 1 - B
    Matrix5x4 colorInvertMatrix(-1,  0,  0, 0,
                                 0, -1,  0, 0,
                                 0,  0, -1, 0,
                                 0,  0,  0, 1,
                                 1,  1,  1, 0);
    effectMatrix = effectMatrix * colorInvertMatrix;
  }

  if (aContrastEffect != 0.0) {
    // Multiplying with:
    // R' = (1 + c) * (R - 0.5) + 0.5
    // G' = (1 + c) * (G - 0.5) + 0.5
    // B' = (1 + c) * (B - 0.5) + 0.5
    float cP1 = aContrastEffect + 1;
    float hc = 0.5*aContrastEffect;
    Matrix5x4 contrastMatrix( cP1,   0,   0, 0,
                                0, cP1,   0, 0,
                                0,   0, cP1, 0,
                                0,   0,   0, 1,
                              -hc, -hc, -hc, 0);
    effectMatrix = effectMatrix * contrastMatrix;
  }

  effectChain.mPrimaryEffect = new EffectRenderTarget(mTwoPassTmpTarget);
  effectChain.mSecondaryEffects[EffectTypes::COLOR_MATRIX] = new EffectColorMatrix(effectMatrix);

  mCompositor->DrawQuad(Rect(Point(0, 0), Size(mTwoPassTmpTarget->GetSize())), aClipRect, effectChain, 1.,
                        Matrix4x4());
}

// Used to clear the 'mLayerComposited' flag at the beginning of each Render().
static void
ClearLayerFlags(Layer* aLayer) {
  ForEachNode<ForwardIterator>(
      aLayer,
      [] (Layer* layer)
      {
        if (layer->AsHostLayer()) {
          static_cast<LayerComposite*>(layer->AsHostLayer())->SetLayerComposited(false);
        }
      });
}

#if defined(MOZ_WIDGET_ANDROID)
class ScopedCompositorRenderOffset {
public:
  ScopedCompositorRenderOffset(CompositorOGL* aCompositor, const ScreenPoint& aOffset) :
    mCompositor(aCompositor),
    mOriginalOffset(mCompositor->GetScreenRenderOffset())
  {
    mCompositor->SetScreenRenderOffset(aOffset);
  }
  ~ScopedCompositorRenderOffset()
  {
    mCompositor->SetScreenRenderOffset(mOriginalOffset);
  }
private:
  CompositorOGL* const mCompositor;
  const ScreenPoint mOriginalOffset;
};
#endif // defined(MOZ_WIDGET_ANDROID)

void
LayerManagerComposite::Render(const nsIntRegion& aInvalidRegion, const nsIntRegion& aOpaqueRegion)
{
  PROFILER_LABEL("LayerManagerComposite", "Render",
    js::ProfileEntry::Category::GRAPHICS);

  if (mDestroyed || !mCompositor || mCompositor->IsDestroyed()) {
    NS_WARNING("Call on destroyed layer manager");
    return;
  }

  ClearLayerFlags(mRoot);

  // At this time, it doesn't really matter if these preferences change
  // during the execution of the function; we should be safe in all
  // permutations. However, may as well just get the values onces and
  // then use them, just in case the consistency becomes important in
  // the future.
  bool invertVal = gfxPrefs::LayersEffectInvert();
  bool grayscaleVal = gfxPrefs::LayersEffectGrayscale();
  float contrastVal = gfxPrefs::LayersEffectContrast();
  bool haveLayerEffects = (invertVal || grayscaleVal || contrastVal != 0.0);

  // Set LayerScope begin/end frame
  LayerScopeAutoFrame frame(PR_Now());

  // Dump to console
  if (gfxPrefs::LayersDump()) {
    this->Dump(/* aSorted= */true);
  } else if (profiler_feature_active("layersdump")) {
    std::stringstream ss;
    Dump(ss);
    profiler_log(ss.str().c_str());
  }

  // Dump to LayerScope Viewer
  if (LayerScope::CheckSendable()) {
    // Create a LayersPacket, dump Layers into it and transfer the
    // packet('s ownership) to LayerScope.
    auto packet = MakeUnique<layerscope::Packet>();
    layerscope::LayersPacket* layersPacket = packet->mutable_layers();
    this->Dump(layersPacket);
    LayerScope::SendLayerDump(Move(packet));
  }

  mozilla::widget::WidgetRenderingContext widgetContext;
#if defined(XP_MACOSX)
  widgetContext.mLayerManager = this;
#elif defined(MOZ_WIDGET_ANDROID)
  widgetContext.mCompositor = GetCompositor();
#endif

  {
    PROFILER_LABEL("LayerManagerComposite", "PreRender",
      js::ProfileEntry::Category::GRAPHICS);

    if (!mCompositor->GetWidget()->PreRender(&widgetContext)) {
      return;
    }
  }

  ParentLayerIntRect clipRect;
  IntRect bounds(mRenderBounds.x, mRenderBounds.y, mRenderBounds.width, mRenderBounds.height);
  IntRect actualBounds;

  CompositorBench(mCompositor, bounds);

  MOZ_ASSERT(mRoot->GetOpacity() == 1);
#if defined(MOZ_WIDGET_ANDROID)
  LayerMetricsWrapper wrapper = GetRootContentLayer();
  if (wrapper) {
    mCompositor->SetClearColor(wrapper.Metadata().GetBackgroundColor());
  } else {
    mCompositor->SetClearColorToDefault();
  }
#endif
  if (mRoot->GetClipRect()) {
    clipRect = *mRoot->GetClipRect();
    IntRect rect(clipRect.x, clipRect.y, clipRect.width, clipRect.height);
    mCompositor->BeginFrame(aInvalidRegion, &rect, bounds, aOpaqueRegion, nullptr, &actualBounds);
  } else {
    gfx::IntRect rect;
    mCompositor->BeginFrame(aInvalidRegion, nullptr, bounds, aOpaqueRegion, &rect, &actualBounds);
    clipRect = ParentLayerIntRect(rect.x, rect.y, rect.width, rect.height);
  }
#if defined(MOZ_WIDGET_ANDROID)
  int32_t toolbarHeight = RenderToolbar();
  // This doesn't affect the projection matrix after BeginFrame has been called.
  ScopedCompositorRenderOffset scopedOffset(mCompositor->AsCompositorOGL(), ScreenPoint(0, toolbarHeight));
#endif

  if (actualBounds.IsEmpty()) {
    mCompositor->GetWidget()->PostRender(&widgetContext);
    return;
  }

  // Allow widget to render a custom background.
  mCompositor->GetWidget()->DrawWindowUnderlay(
    &widgetContext, LayoutDeviceIntRect::FromUnknownRect(actualBounds));

  RefPtr<CompositingRenderTarget> previousTarget;
  if (haveLayerEffects) {
    previousTarget = PushGroupForLayerEffects();
  } else {
    mTwoPassTmpTarget = nullptr;
  }

  // Render our layers.
  {
    Diagnostics::Record record;
    RootLayer()->Prepare(ViewAs<RenderTargetPixel>(clipRect, PixelCastJustification::RenderTargetIsParentLayerForRoot));
    if (record.Recording()) {
      mDiagnostics->RecordPrepareTime(record.Duration());
    }
  }
  // Execute draw commands.
  {
    Diagnostics::Record record;
    RootLayer()->RenderLayer(clipRect.ToUnknownRect(), Nothing());
    if (record.Recording()) {
      mDiagnostics->RecordCompositeTime(record.Duration());
    }
  }
  RootLayer()->Cleanup();

  if (!mRegionToClear.IsEmpty()) {
    for (auto iter = mRegionToClear.RectIter(); !iter.Done(); iter.Next()) {
      const IntRect& r = iter.Get();
      mCompositor->ClearRect(Rect(r.x, r.y, r.width, r.height));
    }
  }

  if (mTwoPassTmpTarget) {
    MOZ_ASSERT(haveLayerEffects);
    PopGroupForLayerEffects(previousTarget, clipRect.ToUnknownRect(),
                            grayscaleVal, invertVal, contrastVal);
  }

  // Allow widget to render a custom foreground.
  mCompositor->GetWidget()->DrawWindowOverlay(
    &widgetContext, LayoutDeviceIntRect::FromUnknownRect(actualBounds));

  mCompositor->NormalDrawingDone();

#if defined(MOZ_WIDGET_ANDROID)
  HandlePixelsTarget();
#endif // defined(MOZ_WIDGET_ANDROID)

  // Debugging
  RenderDebugOverlay(actualBounds);

  {
    PROFILER_LABEL("LayerManagerComposite", "EndFrame",
      js::ProfileEntry::Category::GRAPHICS);

    mCompositor->EndFrame();

    // Call after EndFrame()
    mCompositor->SetDispAcquireFence(mRoot);
  }

  mCompositor->GetWidget()->PostRender(&widgetContext);

  RecordFrame();
}

#if defined(MOZ_WIDGET_ANDROID)
class ScopedCompositorProjMatrix {
public:
  ScopedCompositorProjMatrix(CompositorOGL* aCompositor, const Matrix4x4& aProjMatrix):
    mCompositor(aCompositor),
    mOriginalProjMatrix(mCompositor->GetProjMatrix())
  {
    mCompositor->SetProjMatrix(aProjMatrix);
  }

  ~ScopedCompositorProjMatrix()
  {
    mCompositor->SetProjMatrix(mOriginalProjMatrix);
  }
private:
  CompositorOGL* const mCompositor;
  const Matrix4x4 mOriginalProjMatrix;
};

class ScopedCompostitorSurfaceSize {
public:
  ScopedCompostitorSurfaceSize(CompositorOGL* aCompositor, const gfx::IntSize& aSize) :
    mCompositor(aCompositor),
    mOriginalSize(mCompositor->GetDestinationSurfaceSize())
  {
    mCompositor->SetDestinationSurfaceSize(aSize);
  }
  ~ScopedCompostitorSurfaceSize()
  {
    mCompositor->SetDestinationSurfaceSize(mOriginalSize);
  }
private:
  CompositorOGL* const mCompositor;
  const gfx::IntSize mOriginalSize;
};

class ScopedContextSurfaceOverride {
public:
  ScopedContextSurfaceOverride(GLContextEGL* aContext, void* aSurface) :
    mContext(aContext)
  {
    MOZ_ASSERT(aSurface);
    mContext->SetEGLSurfaceOverride(aSurface);
    mContext->MakeCurrent(true);
  }
  ~ScopedContextSurfaceOverride()
  {
    mContext->SetEGLSurfaceOverride(EGL_NO_SURFACE);
    mContext->MakeCurrent(true);
  }
private:
  GLContextEGL* const mContext;
};

void
LayerManagerComposite::RenderToPresentationSurface()
{
  widget::CompositorWidget* const widget = mCompositor->GetWidget();
  ANativeWindow* window = widget->AsAndroid()->GetPresentationANativeWindow();

  if (!window) {
    return;
  }

  EGLSurface surface = widget->AsAndroid()->GetPresentationEGLSurface();

  if (!surface) {
    //create surface;
    surface = GLContextProviderEGL::CreateEGLSurface(window);
    if (!surface) {
      return;
    }

    widget->AsAndroid()->SetPresentationEGLSurface(surface);
  }

  CompositorOGL* compositor = mCompositor->AsCompositorOGL();
  GLContext* gl = compositor->gl();
  GLContextEGL* egl = GLContextEGL::Cast(gl);

  if (!egl) {
    return;
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
  const nsIntRect originalRect = nsIntRect(0, 0, originalSize.width, originalSize.height);

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

  Matrix viewMatrix = ComputeTransformForRotation(originalRect,
                                                  rotation);
  viewMatrix.Invert(); // unrotate
  viewMatrix.PostScale(scale, scale);
  viewMatrix.PostTranslate(offset.x, offset.y);
  Matrix4x4 matrix = Matrix4x4::From2D(viewMatrix);

  mRoot->ComputeEffectiveTransforms(matrix);
  nsIntRegion opaque;
  LayerIntRegion visible;
  PostProcessLayers(mRoot, opaque, visible, Nothing());

  nsIntRegion invalid;
  IntRect bounds = IntRect::Truncate(0, 0, scale * pageWidth, actualHeight);
  IntRect rect, actualBounds;
  MOZ_ASSERT(mRoot->GetOpacity() == 1);
  mCompositor->BeginFrame(invalid, nullptr, bounds, nsIntRegion(), &rect, &actualBounds);

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

int32_t
LayerManagerComposite::RenderToolbar()
{
  int32_t toolbarHeight = 0;

  // If GetTargetContext returns null we are drawing to the screen so draw the toolbar offset if present.
  if (mCompositor->GetTargetContext() != nullptr) {
    return toolbarHeight;
  }

  if (CompositorBridgeParent* bridge = mCompositor->GetCompositorBridgeParent()) {
    AndroidDynamicToolbarAnimator* animator = bridge->GetAPZCTreeManager()->GetAndroidDynamicToolbarAnimator();
    MOZ_ASSERT(animator);
    toolbarHeight = animator->GetCurrentToolbarHeight();
    if (toolbarHeight == 0) {
      return toolbarHeight;
    }

    EffectChain effects;
    effects.mPrimaryEffect = animator->GetToolbarEffect(mCompositor->AsCompositorOGL());
    if (!effects.mPrimaryEffect) {
      // No toolbar texture so just draw a red square
      effects.mPrimaryEffect = new EffectSolidColor(gfx::Color(1, 0, 0));
    }
    mCompositor->DrawQuad(gfx::Rect(0, 0, mRenderBounds.width, toolbarHeight),
                          IntRect(0, 0, mRenderBounds.width, toolbarHeight), effects, 1.0, gfx::Matrix4x4());

    // Move the content down the surface by the toolbar's height so they don't overlap
    gfx::Matrix4x4 mat = mCompositor->AsCompositorOGL()->GetProjMatrix();
    mat.PreTranslate(0.0f, float(toolbarHeight), 0.0f);
    mCompositor->AsCompositorOGL()->SetProjMatrix(mat);
  }

  return toolbarHeight;
}

// Used by robocop tests to get a snapshot of the frame buffer.
void
LayerManagerComposite::HandlePixelsTarget()
{
  if (!mScreenPixelsTarget) {
    return;
  }

  int32_t bufferWidth = mRenderBounds.width;
  int32_t bufferHeight = mRenderBounds.height;
  ipc::Shmem mem;
  if (!mScreenPixelsTarget->AllocPixelBuffer(bufferWidth * bufferHeight * sizeof(uint32_t), &mem)) {
    // Failed to alloc shmem, Just bail out.
    return;
  }
  CompositorOGL* compositor = mCompositor->AsCompositorOGL();
  GLContext* gl = compositor->gl();
  MOZ_ASSERT(gl);
  gl->fReadPixels(0, 0, bufferWidth, bufferHeight, LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE, mem.get<uint8_t>());
  Unused << mScreenPixelsTarget->SendScreenPixels(mem, ScreenIntSize(bufferWidth, bufferHeight));
  mScreenPixelsTarget = nullptr;
}
#endif

class TextLayerComposite : public TextLayer,
                           public LayerComposite
{
public:
  explicit TextLayerComposite(LayerManagerComposite *aManager)
    : TextLayer(aManager, nullptr)
    , LayerComposite(aManager)
  {
    MOZ_COUNT_CTOR(TextLayerComposite);
    mImplData = static_cast<LayerComposite*>(this);
  }

protected:
  ~TextLayerComposite()
  {
    MOZ_COUNT_DTOR(TextLayerComposite);
    Destroy();
  }

public:
  // LayerComposite Implementation
  virtual Layer* GetLayer() override { return this; }

  virtual void SetLayerManager(HostLayerManager* aManager) override
  {
    LayerComposite::SetLayerManager(aManager);
    mManager = aManager;
  }

  virtual void Destroy() override { mDestroyed = true; }

  virtual void RenderLayer(const gfx::IntRect& aClipRect,
                           const Maybe<gfx::Polygon>& aGeometry) override {}
  virtual void CleanupResources() override {};

  virtual void GenEffectChain(EffectChain& aEffect) override {}

  CompositableHost* GetCompositableHost() override { return nullptr; }

  virtual HostLayer* AsHostLayer() override { return this; }

  virtual const char* Name() const override { return "TextLayerComposite"; }
};

class BorderLayerComposite : public BorderLayer,
                             public LayerComposite
{
public:
  explicit BorderLayerComposite(LayerManagerComposite *aManager)
    : BorderLayer(aManager, nullptr)
    , LayerComposite(aManager)
  {
    MOZ_COUNT_CTOR(BorderLayerComposite);
    mImplData = static_cast<LayerComposite*>(this);
  }

protected:
  ~BorderLayerComposite()
  {
    MOZ_COUNT_DTOR(BorderLayerComposite);
    Destroy();
  }

public:
  // LayerComposite Implementation
  virtual Layer* GetLayer() override { return this; }

  virtual void SetLayerManager(HostLayerManager* aManager) override
  {
    LayerComposite::SetLayerManager(aManager);
    mManager = aManager;
  }

  virtual void Destroy() override { mDestroyed = true; }

  virtual void RenderLayer(const gfx::IntRect& aClipRect,
                           const Maybe<gfx::Polygon>& aGeometry) override {}
  virtual void CleanupResources() override {};

  virtual void GenEffectChain(EffectChain& aEffect) override {}

  CompositableHost* GetCompositableHost() override { return nullptr; }

  virtual HostLayer* AsHostLayer() override { return this; }

  virtual const char* Name() const override { return "BorderLayerComposite"; }
};

already_AddRefed<PaintedLayer>
LayerManagerComposite::CreatePaintedLayer()
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return RefPtr<PaintedLayer>(new PaintedLayerComposite(this)).forget();
}

already_AddRefed<ContainerLayer>
LayerManagerComposite::CreateContainerLayer()
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return RefPtr<ContainerLayer>(new ContainerLayerComposite(this)).forget();
}

already_AddRefed<ImageLayer>
LayerManagerComposite::CreateImageLayer()
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return RefPtr<ImageLayer>(new ImageLayerComposite(this)).forget();
}

already_AddRefed<ColorLayer>
LayerManagerComposite::CreateColorLayer()
{
  if (LayerManagerComposite::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return RefPtr<ColorLayer>(new ColorLayerComposite(this)).forget();
}

already_AddRefed<CanvasLayer>
LayerManagerComposite::CreateCanvasLayer()
{
  if (LayerManagerComposite::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return RefPtr<CanvasLayer>(new CanvasLayerComposite(this)).forget();
}

already_AddRefed<RefLayer>
LayerManagerComposite::CreateRefLayer()
{
  if (LayerManagerComposite::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return RefPtr<RefLayer>(new RefLayerComposite(this)).forget();
}

already_AddRefed<TextLayer>
LayerManagerComposite::CreateTextLayer()
{
  if (LayerManagerComposite::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return RefPtr<TextLayer>(new TextLayerComposite(this)).forget();
}

already_AddRefed<BorderLayer>
LayerManagerComposite::CreateBorderLayer()
{
  if (LayerManagerComposite::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return RefPtr<BorderLayer>(new BorderLayerComposite(this)).forget();
}

LayerManagerComposite::AutoAddMaskEffect::AutoAddMaskEffect(Layer* aMaskLayer,
                                                            EffectChain& aEffects)
  : mCompositable(nullptr), mFailed(false)
{
  if (!aMaskLayer) {
    return;
  }

  mCompositable = ToLayerComposite(aMaskLayer)->GetCompositableHost();
  if (!mCompositable) {
    NS_WARNING("Mask layer with no compositable host");
    mFailed = true;
    return;
  }

  if (!mCompositable->AddMaskEffect(aEffects, aMaskLayer->GetEffectiveTransform())) {
    mCompositable = nullptr;
    mFailed = true;
  }
}

LayerManagerComposite::AutoAddMaskEffect::~AutoAddMaskEffect()
{
  if (!mCompositable) {
    return;
  }

  mCompositable->RemoveMaskEffect();
}

void
LayerManagerComposite::ChangeCompositor(Compositor* aNewCompositor)
{
  if (mCompositor) {
    mCompositor->CancelFrame(false);
  }
  mCompositor = aNewCompositor;
  mTwoPassTmpTarget = nullptr;
}

LayerComposite::LayerComposite(LayerManagerComposite *aManager)
  : HostLayer(aManager)
  , mCompositeManager(aManager)
  , mCompositor(aManager->GetCompositor())
  , mDestroyed(false)
  , mLayerComposited(false)
{ }

LayerComposite::~LayerComposite()
{
}

void
LayerComposite::Destroy()
{
  if (!mDestroyed) {
    mDestroyed = true;
    CleanupResources();
  }
}

void
LayerComposite::AddBlendModeEffect(EffectChain& aEffectChain)
{
  gfx::CompositionOp blendMode = GetLayer()->GetEffectiveMixBlendMode();
  if (blendMode == gfx::CompositionOp::OP_OVER) {
    return;
  }

  aEffectChain.mSecondaryEffects[EffectTypes::BLEND_MODE] = new EffectBlendMode(blendMode);
  return;
}

bool
LayerManagerComposite::CanUseCanvasLayerForSize(const IntSize &aSize)
{
  return mCompositor->CanUseCanvasLayerForSize(gfx::IntSize(aSize.width,
                                                            aSize.height));
}

void
LayerManagerComposite::NotifyShadowTreeTransaction()
{
  if (gfxPrefs::LayersDrawFPS()) {
    mDiagnostics->AddTxnFrame();
  }
}

void
LayerComposite::SetLayerManager(HostLayerManager* aManager)
{
  HostLayer::SetLayerManager(aManager);
  mCompositeManager = static_cast<LayerManagerComposite*>(aManager);
  mCompositor = mCompositeManager->GetCompositor();
}

bool
LayerManagerComposite::AsyncPanZoomEnabled() const
{
  if (CompositorBridgeParent* bridge = mCompositor->GetCompositorBridgeParent()) {
    return bridge->GetOptions().UseAPZ();
  }
  return false;
}

bool
LayerManagerComposite::AlwaysScheduleComposite() const
{
  return !!(mCompositor->GetDiagnosticTypes() & DiagnosticTypes::FLASH_BORDERS);
}

nsIntRegion
LayerComposite::GetFullyRenderedRegion() {
  if (TiledContentHost* tiled = GetCompositableHost() ? GetCompositableHost()->AsTiledContentHost()
                                                        : nullptr) {
    nsIntRegion shadowVisibleRegion = GetShadowVisibleRegion().ToUnknownRegion();
    // Discard the region which hasn't been drawn yet when doing
    // progressive drawing. Note that if the shadow visible region
    // shrunk the tiled valig region may not have discarded this yet.
    shadowVisibleRegion.And(shadowVisibleRegion, tiled->GetValidRegion());
    return shadowVisibleRegion;
  } else {
    return GetShadowVisibleRegion().ToUnknownRegion();
  }
}

Matrix4x4
HostLayer::GetShadowTransform() {
  Matrix4x4 transform = mShadowTransform;
  Layer* layer = GetLayer();

  transform.PostScale(layer->GetPostXScale(), layer->GetPostYScale(), 1.0f);
  if (const ContainerLayer* c = layer->AsContainerLayer()) {
    transform.PreScale(c->GetPreXScale(), c->GetPreYScale(), 1.0f);
  }

  return transform;
}

bool
LayerComposite::HasStaleCompositor() const
{
  return mCompositeManager->GetCompositor() != mCompositor;
}

static bool
LayerHasCheckerboardingAPZC(Layer* aLayer, Color* aOutColor)
{
  bool answer = false;
  for (LayerMetricsWrapper i(aLayer, LayerMetricsWrapper::StartAt::BOTTOM); i; i = i.GetParent()) {
    if (!i.Metrics().IsScrollable()) {
      continue;
    }
    if (i.GetApzc() && i.GetApzc()->IsCurrentlyCheckerboarding()) {
      if (aOutColor) {
        *aOutColor = i.Metadata().GetBackgroundColor();
      }
      answer = true;
      break;
    }
    break;
  }
  return answer;
}

bool
LayerComposite::NeedToDrawCheckerboarding(gfx::Color* aOutCheckerboardingColor)
{
  return GetLayer()->Manager()->AsyncPanZoomEnabled() &&
         (GetLayer()->GetContentFlags() & Layer::CONTENT_OPAQUE) &&
         GetLayer()->IsOpaqueForVisibility() &&
         LayerHasCheckerboardingAPZC(GetLayer(), aOutCheckerboardingColor);
}

#ifndef MOZ_HAVE_PLATFORM_SPECIFIC_LAYER_BUFFERS

/*static*/ bool
LayerManagerComposite::SupportsDirectTexturing()
{
  return false;
}

/*static*/ void
LayerManagerComposite::PlatformSyncBeforeReplyUpdate()
{
}

#endif  // !defined(MOZ_HAVE_PLATFORM_SPECIFIC_LAYER_BUFFERS)

} // namespace layers
} // namespace mozilla
