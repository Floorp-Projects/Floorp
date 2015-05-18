/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayerManagerComposite.h"
#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint16_t, uint32_t
#include "CanvasLayerComposite.h"       // for CanvasLayerComposite
#include "ColorLayerComposite.h"        // for ColorLayerComposite
#include "Composer2D.h"                 // for Composer2D
#include "CompositableHost.h"           // for CompositableHost
#include "ContainerLayerComposite.h"    // for ContainerLayerComposite, etc
#include "FPSCounter.h"                 // for FPSState, FPSCounter
#include "FrameMetrics.h"               // for FrameMetrics
#include "GeckoProfiler.h"              // for profiler_set_frame_number, etc
#include "ImageLayerComposite.h"        // for ImageLayerComposite
#include "Layers.h"                     // for Layer, ContainerLayer, etc
#include "LayerScope.h"                 // for LayerScope Tool
#include "protobuf/LayerScopePacket.pb.h" // for protobuf (LayerScope)
#include "PaintedLayerComposite.h"      // for PaintedLayerComposite
#include "TiledLayerBuffer.h"           // for TiledLayerComposer
#include "Units.h"                      // for ScreenIntRect
#include "UnitTransforms.h"             // for ViewAs
#include "gfx2DGlue.h"                  // for ToMatrix4x4
#include "gfxPrefs.h"                   // for gfxPrefs
#ifdef XP_MACOSX
#include "gfxPlatformMac.h"
#endif
#include "gfxRect.h"                    // for gfxRect
#include "gfxUtils.h"                   // for frame color util
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/RefPtr.h"             // for RefPtr, TemporaryRef
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
#include "ipc/CompositorBench.h"        // for CompositorBench
#include "ipc/ShadowLayerUtils.h"
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "nsAppRunner.h"
#include "nsRefPtr.h"                   // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_WARNING, NS_RUNTIMEABORT, etc
#include "nsISupportsImpl.h"            // for Layer::AddRef, etc
#include "nsIWidget.h"                  // for nsIWidget
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for mozilla::gfx::IntRect
#include "nsRegion.h"                   // for nsIntRegion, etc
#ifdef MOZ_WIDGET_ANDROID
#include <android/log.h>
#include "AndroidBridge.h"
#include "opengl/CompositorOGL.h"
#include "GLContextEGL.h"
#include "GLContextProvider.h"
#include "ScopedGLHelpers.h"
#endif
#include "GeckoProfiler.h"
#include "TextRenderer.h"               // for TextRenderer

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
  ToLayerComposite(aLayer)->CleanupResources();
  for (Layer* child = aLayer->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    ClearSubtree(child);
  }
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

/**
 * LayerManagerComposite
 */
LayerManagerComposite::LayerManagerComposite(Compositor* aCompositor)
: mWarningLevel(0.0f)
, mUnusedApzTransformWarning(false)
, mCompositor(aCompositor)
, mInTransaction(false)
, mIsCompositorReady(false)
, mDebugOverlayWantsNextFrame(false)
, mGeometryChanged(true)
, mLastFrameMissedHWC(false)
{
  mTextRenderer = new TextRenderer(aCompositor);
  MOZ_ASSERT(aCompositor);
}

LayerManagerComposite::~LayerManagerComposite()
{
  Destroy();
}


bool
LayerManagerComposite::Initialize()
{
  bool result = mCompositor->Initialize();
  return result;
}

void
LayerManagerComposite::Destroy()
{
  if (!mDestroyed) {
    mCompositor->GetWidget()->CleanupWindowEffects();
    if (mRoot) {
      RootLayer()->Destroy();
    }
    mRoot = nullptr;
    mDestroyed = true;
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
  return Compositor::GetBackend() != LayersBackend::LAYERS_BASIC &&
         LayerManager::AreComponentAlphaLayersEnabled();
}

void
LayerManagerComposite::BeginTransaction()
{
  mInTransaction = true;
  
  if (!mCompositor->Ready()) {
    return;
  }
  
  mIsCompositorReady = true;

  mClonedLayerTreeProperties = LayerProperties::CloneFrom(GetRoot());
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

void
LayerManagerComposite::ApplyOcclusionCulling(Layer* aLayer, nsIntRegion& aOpaqueRegion)
{
  nsIntRegion localOpaque;
  Matrix transform2d;
  bool isTranslation = false;
  // If aLayer has a simple transform (only an integer translation) then we
  // can easily convert aOpaqueRegion into pre-transform coordinates and include
  // that region.
  if (aLayer->GetLocalTransform().Is2D(&transform2d)) {
    if (transform2d.IsIntegerTranslation()) {
      isTranslation = true;
      localOpaque = aOpaqueRegion;
      localOpaque.MoveBy(-transform2d._31, -transform2d._32);
    }
  }

  // Subtract any areas that we know to be opaque from our
  // visible region.
  LayerComposite *composite = aLayer->AsLayerComposite();
  if (!localOpaque.IsEmpty()) {
    nsIntRegion visible = composite->GetShadowVisibleRegion();
    visible.Sub(visible, localOpaque);
    composite->SetShadowVisibleRegion(visible);
  }

  // Compute occlusions for our descendants (in front-to-back order) and allow them to
  // contribute to localOpaque.
  for (Layer* child = aLayer->GetLastChild(); child; child = child->GetPrevSibling()) {
    ApplyOcclusionCulling(child, localOpaque);
  }

  // If we have a simple transform, then we can add our opaque area into
  // aOpaqueRegion.
  if (isTranslation &&
      !aLayer->GetMaskLayer() &&
      aLayer->GetLocalOpacity() == 1.0f) {
    if (aLayer->GetContentFlags() & Layer::CONTENT_OPAQUE) {
      localOpaque.Or(localOpaque, composite->GetFullyRenderedRegion());
    }
    localOpaque.MoveBy(transform2d._31, transform2d._32);
    const Maybe<ParentLayerIntRect>& clip = aLayer->GetEffectiveClipRect();
    if (clip) {
      localOpaque.And(localOpaque, ParentLayerIntRect::ToUntyped(*clip));
    }
    aOpaqueRegion.Or(aOpaqueRegion, localOpaque);
  }
}

bool
LayerManagerComposite::EndEmptyTransaction(EndTransactionFlags aFlags)
{
  NS_ASSERTION(mInTransaction, "Didn't call BeginTransaction?");
  if (!mRoot) {
    mInTransaction = false;
    mIsCompositorReady = false;
    return false;
  }

  EndTransaction(nullptr, nullptr);
  return true;
}

void
LayerManagerComposite::EndTransaction(DrawPaintedLayerCallback aCallback,
                                      void* aCallbackData,
                                      EndTransactionFlags aFlags)
{
  NS_ASSERTION(mInTransaction, "Didn't call BeginTransaction?");
  NS_ASSERTION(!aCallback && !aCallbackData, "Not expecting callbacks here");
  mInTransaction = false;

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

  if (mRoot && mClonedLayerTreeProperties) {
    MOZ_ASSERT(!mTarget);
    nsIntRegion invalid =
      mClonedLayerTreeProperties->ComputeDifferences(mRoot, nullptr, &mGeometryChanged);
    mClonedLayerTreeProperties = nullptr;

    mInvalidRegion.Or(mInvalidRegion, invalid);
  } else if (!mTarget) {
    mInvalidRegion.Or(mInvalidRegion, mRenderBounds);
  }

  if (mRoot && !(aFlags & END_NO_IMMEDIATE_REDRAW)) {
    if (aFlags & END_NO_COMPOSITE) {
      // Apply pending tree updates before recomputing effective
      // properties.
      mRoot->ApplyPendingUpdatesToSubtree();
    }

    // The results of our drawing always go directly into a pixel buffer,
    // so we don't need to pass any global transform here.
    mRoot->ComputeEffectiveTransforms(gfx::Matrix4x4());

    nsIntRegion opaque;
    ApplyOcclusionCulling(mRoot, opaque);

    Render();
#ifdef MOZ_WIDGET_ANDROID
    RenderToPresentationSurface();
#endif
    mGeometryChanged = false;
  } else {
    // Modified layer tree
    mGeometryChanged = true;
  }

  mCompositor->ClearTargetContext();
  mTarget = nullptr;

#ifdef MOZ_LAYERS_HAVE_LOG
  Log();
  MOZ_LAYERS_LOG(("]----- EndTransaction"));
#endif
}

TemporaryRef<DrawTarget>
LayerManagerComposite::CreateOptimalMaskDrawTarget(const IntSize &aSize)
{
  NS_RUNTIMEABORT("Should only be called on the drawing side");
  return nullptr;
}

already_AddRefed<PaintedLayer>
LayerManagerComposite::CreatePaintedLayer()
{
  MOZ_ASSERT(gIsGtest, "Unless you're testing the compositor using GTest,"
                       "this should only be called on the drawing side");
  nsRefPtr<PaintedLayer> layer = new PaintedLayerComposite(this);
  return layer.forget();
}

already_AddRefed<ContainerLayer>
LayerManagerComposite::CreateContainerLayer()
{
  MOZ_ASSERT(gIsGtest, "Unless you're testing the compositor using GTest,"
                       "this should only be called on the drawing side");
  nsRefPtr<ContainerLayer> layer = new ContainerLayerComposite(this);
  return layer.forget();
}

already_AddRefed<ImageLayer>
LayerManagerComposite::CreateImageLayer()
{
  NS_RUNTIMEABORT("Should only be called on the drawing side");
  return nullptr;
}

already_AddRefed<ColorLayer>
LayerManagerComposite::CreateColorLayer()
{
  MOZ_ASSERT(gIsGtest, "Unless you're testing the compositor using GTest,"
                       "this should only be called on the drawing side");
  nsRefPtr<ColorLayer> layer = new ColorLayerComposite(this);
  return layer.forget();
}

already_AddRefed<CanvasLayer>
LayerManagerComposite::CreateCanvasLayer()
{
  NS_RUNTIMEABORT("Should only be called on the drawing side");
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

static uint16_t sFrameCount = 0;
void
LayerManagerComposite::RenderDebugOverlay(const Rect& aBounds)
{
  bool drawFps = gfxPrefs::LayersDrawFPS();
  bool drawFrameCounter = gfxPrefs::DrawFrameCounter();
  bool drawFrameColorBars = gfxPrefs::CompositorDrawColorBars();

  TimeStamp now = TimeStamp::Now();

  if (drawFps) {
    if (!mFPS) {
      mFPS = MakeUnique<FPSState>();
    }

    float alpha = 1;
#ifdef ANDROID
    // Draw a translation delay warning overlay
    int width;
    int border;
    if ((now - mWarnTime).ToMilliseconds() < kVisualWarningDuration) {
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

    float fillRatio = mCompositor->GetFillRatio();
    mFPS->DrawFPS(now, drawFrameColorBars ? 10 : 1, 2, unsigned(fillRatio), mCompositor);

    if (mUnusedApzTransformWarning) {
      // If we have an unused APZ transform on this composite, draw a 20x20 red box
      // in the top-right corner
      EffectChain effects;
      effects.mPrimaryEffect = new EffectSolidColor(gfx::Color(1, 0, 0, 1));
      mCompositor->DrawQuad(gfx::Rect(aBounds.width - 20, 0, aBounds.width, 20),
                            aBounds, effects, alpha, gfx::Matrix4x4());

      mUnusedApzTransformWarning = false;
      SetDebugOverlayWantsNextFrame(true);
    }
  } else {
    mFPS = nullptr;
  }

  if (drawFrameColorBars) {
    gfx::Rect sideRect(0, 0, 10, aBounds.height);

    EffectChain effects;
    effects.mPrimaryEffect = new EffectSolidColor(gfxUtils::GetColorForFrameNumber(sFrameCount));
    mCompositor->DrawQuad(sideRect,
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
    gfx::Rect clip(0,0, bitWidth*640, bitWidth*640);

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

  gfx::Rect clipRectF(aClipRect.x, aClipRect.y, aClipRect.width, aClipRect.height);
  mCompositor->DrawQuad(Rect(Point(0, 0), Size(mTwoPassTmpTarget->GetSize())), clipRectF, effectChain, 1.,
                        Matrix4x4());
}

void
LayerManagerComposite::Render()
{
  PROFILER_LABEL("LayerManagerComposite", "Render",
    js::ProfileEntry::Category::GRAPHICS);

  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return;
  }

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
    this->Dump();
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

  /** Our more efficient but less powerful alter ego, if one is available. */
  nsRefPtr<Composer2D> composer2D;
  composer2D = mCompositor->GetWidget()->GetComposer2D();

  // We can't use composert2D if we have layer effects
  if (!mTarget && !haveLayerEffects &&
      gfxPrefs::Composer2DCompositionEnabled() &&
      composer2D && composer2D->HasHwc() && composer2D->TryRenderWithHwc(mRoot, mGeometryChanged))
  {
    LayerScope::SetHWComposed();
    if (mFPS) {
      double fps = mFPS->mCompositionFps.AddFrameAndGetFps(TimeStamp::Now());
      if (gfxPrefs::LayersDrawFPS()) {
        printf_stderr("HWComposer: FPS is %g\n", fps);
      }
    }
    mCompositor->EndFrameForExternalComposition(Matrix());
    // Reset the invalid region as compositing is done
    mInvalidRegion.SetEmpty();
    mLastFrameMissedHWC = false;
    return;
  } else if (!mTarget && !haveLayerEffects) {
    mLastFrameMissedHWC = !!composer2D;
  }

  {
    PROFILER_LABEL("LayerManagerComposite", "PreRender",
      js::ProfileEntry::Category::GRAPHICS);

    if (!mCompositor->GetWidget()->PreRender(this)) {
      return;
    }
  }

  nsIntRegion invalid;
  if (mTarget) {
    invalid = mTargetBounds;
  } else {
    invalid = mInvalidRegion;
    // Reset the invalid region now that we've begun compositing.
    mInvalidRegion.SetEmpty();
  }

  ParentLayerIntRect clipRect;
  Rect bounds(mRenderBounds.x, mRenderBounds.y, mRenderBounds.width, mRenderBounds.height);
  Rect actualBounds;

  CompositorBench(mCompositor, bounds);

  if (mRoot->GetClipRect()) {
    clipRect = *mRoot->GetClipRect();
    Rect rect(clipRect.x, clipRect.y, clipRect.width, clipRect.height);
    mCompositor->BeginFrame(invalid, &rect, bounds, nullptr, &actualBounds);
  } else {
    gfx::Rect rect;
    mCompositor->BeginFrame(invalid, nullptr, bounds, &rect, &actualBounds);
    clipRect = ParentLayerIntRect(rect.x, rect.y, rect.width, rect.height);
  }

  if (actualBounds.IsEmpty()) {
    mCompositor->GetWidget()->PostRender(this);
    return;
  }

  // Allow widget to render a custom background.
  mCompositor->GetWidget()->DrawWindowUnderlay(this, IntRect(actualBounds.x,
                                                               actualBounds.y,
                                                               actualBounds.width,
                                                               actualBounds.height));

  RefPtr<CompositingRenderTarget> previousTarget;
  if (haveLayerEffects) {
    previousTarget = PushGroupForLayerEffects();
  } else {
    mTwoPassTmpTarget = nullptr;
  }

  // Render our layers.
  RootLayer()->Prepare(ViewAs<RenderTargetPixel>(clipRect, PixelCastJustification::RenderTargetIsParentLayerForRoot));
  RootLayer()->RenderLayer(ParentLayerIntRect::ToUntyped(clipRect));

  if (!mRegionToClear.IsEmpty()) {
    nsIntRegionRectIterator iter(mRegionToClear);
    const IntRect *r;
    while ((r = iter.Next())) {
      mCompositor->ClearRect(Rect(r->x, r->y, r->width, r->height));
    }
  }

  if (mTwoPassTmpTarget) {
    MOZ_ASSERT(haveLayerEffects);
    PopGroupForLayerEffects(previousTarget, ParentLayerIntRect::ToUntyped(clipRect),
                            grayscaleVal, invertVal, contrastVal);
  }

  // Allow widget to render a custom foreground.
  mCompositor->GetWidget()->DrawWindowOverlay(this, IntRect(actualBounds.x,
                                                              actualBounds.y,
                                                              actualBounds.width,
                                                              actualBounds.height));

  // Debugging
  RenderDebugOverlay(actualBounds);

  {
    PROFILER_LABEL("LayerManagerComposite", "EndFrame",
      js::ProfileEntry::Category::GRAPHICS);

    mCompositor->EndFrame();
    mCompositor->SetDispAcquireFence(mRoot);
  }

  if (composer2D) {
    composer2D->Render();
  }

  mCompositor->GetWidget()->PostRender(this);

  RecordFrame();
}

#ifdef MOZ_WIDGET_ANDROID
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
  if (!AndroidBridge::Bridge()) {
    return;
  }

  void* window = AndroidBridge::Bridge()->GetPresentationWindow();

  if (!window) {
    return;
  }

  EGLSurface surface = AndroidBridge::Bridge()->GetPresentationSurface();

  if (!surface) {
    //create surface;
    surface = GLContextProviderEGL::CreateEGLSurface(window);
    if (!surface) {
      return;
    }

    AndroidBridge::Bridge()->SetPresentationSurface(surface);
  }

  CompositorOGL* compositor = static_cast<CompositorOGL*>(mCompositor.get());
  GLContext* gl = compositor->gl();
  GLContextEGL* egl = GLContextEGL::Cast(gl);

  if (!egl) {
    return;
  }

  const IntSize windowSize = AndroidBridge::Bridge()->GetNativeWindowSize(window);

  if ((windowSize.width <= 0) || (windowSize.height <= 0)) {
    return;
  }

  const int actualWidth = windowSize.width;
  const int actualHeight = windowSize.height;

  const gfx::IntSize originalSize = compositor->GetDestinationSurfaceSize();

  const int pageWidth = originalSize.width;
  const int pageHeight = originalSize.height;

  float scale = 1.0;

  if ((pageWidth > actualWidth) || (pageHeight > actualHeight)) {
    const float scaleWidth = (float)actualWidth / (float)pageWidth;
    const float scaleHeight = (float)actualHeight / (float)pageHeight;
    scale = scaleWidth <= scaleHeight ? scaleWidth : scaleHeight;
  }

  const gfx::IntSize actualSize(actualWidth, actualHeight);
  ScopedCompostitorSurfaceSize overrideSurfaceSize(compositor, actualSize);

  const ScreenPoint offset((actualWidth - (int)(scale * pageWidth)) / 2, 0);
  ScopedCompositorRenderOffset overrideRenderOffset(compositor, offset);
  ScopedContextSurfaceOverride overrideSurface(egl, surface);

  nsIntRegion invalid;
  Rect bounds(0.0f, 0.0f, scale * pageWidth, (float)actualHeight);
  Rect rect, actualBounds;

  mCompositor->BeginFrame(invalid, nullptr, bounds, &rect, &actualBounds);

  // Override the projection matrix since the presentation frame buffer
  // is probably not the same size as the device frame buffer. The override
  // projection matrix also scales the content to fit into the presentation
  // frame buffer.
  Matrix viewMatrix;
  viewMatrix.PreTranslate(-1.0, 1.0);
  viewMatrix.PreScale((2.0f * scale) / (float)actualWidth, (2.0f * scale) / (float)actualHeight);
  viewMatrix.PreScale(1.0f, -1.0f);
  viewMatrix.PreTranslate((int)((float)offset.x / scale), offset.y);

  Matrix4x4 projMatrix = Matrix4x4::From2D(viewMatrix);

  ScopedCompositorProjMatrix overrideProjMatrix(compositor, projMatrix);

  // The Java side of Fennec sets a scissor rect that accounts for
  // chrome such as the URL bar. Override that so that the entire frame buffer
  // is cleared.
  ScopedScissorRect screen(egl, 0, 0, actualWidth, actualHeight);
  egl->fClearColor(0.0, 0.0, 0.0, 0.0);
  egl->fClear(LOCAL_GL_COLOR_BUFFER_BIT);

  const IntRect clipRect = IntRect(0, 0, (int)(scale * pageWidth), actualHeight);
  RootLayer()->Prepare(RenderTargetPixel::FromUntyped(clipRect));
  RootLayer()->RenderLayer(clipRect);

  mCompositor->EndFrame();
  mCompositor->SetDispAcquireFence(mRoot);
}
#endif

static void
SubtractTransformedRegion(nsIntRegion& aRegion,
                          const nsIntRegion& aRegionToSubtract,
                          const Matrix4x4& aTransform)
{
  if (aRegionToSubtract.IsEmpty()) {
    return;
  }

  // For each rect in the region, find out its bounds in screen space and
  // subtract it from the screen region.
  nsIntRegionRectIterator it(aRegionToSubtract);
  while (const IntRect* rect = it.Next()) {
    Rect incompleteRect = aTransform.TransformBounds(ToRect(*rect));
    aRegion.Sub(aRegion, IntRect(incompleteRect.x,
                                   incompleteRect.y,
                                   incompleteRect.width,
                                   incompleteRect.height));
  }
}

/* static */ void
LayerManagerComposite::ComputeRenderIntegrityInternal(Layer* aLayer,
                                                      nsIntRegion& aScreenRegion,
                                                      nsIntRegion& aLowPrecisionScreenRegion,
                                                      const Matrix4x4& aTransform)
{
  if (aLayer->GetOpacity() <= 0.f ||
      (aScreenRegion.IsEmpty() && aLowPrecisionScreenRegion.IsEmpty())) {
    return;
  }

  // If the layer's a container, recurse into all of its children
  ContainerLayer* container = aLayer->AsContainerLayer();
  if (container) {
    // Accumulate the transform of intermediate surfaces
    Matrix4x4 transform = aTransform;
    if (container->UseIntermediateSurface()) {
      transform = aLayer->GetEffectiveTransform();
      transform = aTransform * transform;
    }
    for (Layer* child = aLayer->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      ComputeRenderIntegrityInternal(child, aScreenRegion, aLowPrecisionScreenRegion, transform);
    }
    return;
  }

  // Only painted layers can be incomplete
  PaintedLayer* paintedLayer = aLayer->AsPaintedLayer();
  if (!paintedLayer) {
    return;
  }

  // See if there's any incomplete rendering
  nsIntRegion incompleteRegion = aLayer->GetEffectiveVisibleRegion();
  incompleteRegion.Sub(incompleteRegion, paintedLayer->GetValidRegion());

  if (!incompleteRegion.IsEmpty()) {
    // Calculate the transform to get between screen and layer space
    Matrix4x4 transformToScreen = aLayer->GetEffectiveTransform();
    transformToScreen = aTransform * transformToScreen;

    SubtractTransformedRegion(aScreenRegion, incompleteRegion, transformToScreen);

    // See if there's any incomplete low-precision rendering
    TiledLayerComposer* composer = nullptr;
    LayerComposite* shadow = aLayer->AsLayerComposite();
    if (shadow) {
      composer = shadow->GetTiledLayerComposer();
      if (composer) {
        incompleteRegion.Sub(incompleteRegion, composer->GetValidLowPrecisionRegion());
        if (!incompleteRegion.IsEmpty()) {
          SubtractTransformedRegion(aLowPrecisionScreenRegion, incompleteRegion, transformToScreen);
        }
      }
    }

    // If we can't get a valid low precision region, assume it's the same as
    // the high precision region.
    if (!composer) {
      SubtractTransformedRegion(aLowPrecisionScreenRegion, incompleteRegion, transformToScreen);
    }
  }
}

#ifdef MOZ_WIDGET_ANDROID
static float
GetDisplayportCoverage(const CSSRect& aDisplayPort,
                       const Matrix4x4& aTransformToScreen,
                       const IntRect& aScreenRect)
{
  Rect transformedDisplayport =
    aTransformToScreen.TransformBounds(aDisplayPort.ToUnknownRect());

  transformedDisplayport.RoundOut();
  IntRect displayport = IntRect(transformedDisplayport.x,
                                    transformedDisplayport.y,
                                    transformedDisplayport.width,
                                    transformedDisplayport.height);
  if (!displayport.Contains(aScreenRect)) {
    nsIntRegion coveredRegion;
    coveredRegion.And(aScreenRect, displayport);
    return coveredRegion.Area() / (float)(aScreenRect.width * aScreenRect.height);
  }

  return 1.0f;
}
#endif // MOZ_WIDGET_ANDROID

float
LayerManagerComposite::ComputeRenderIntegrity()
{
  // We only ever have incomplete rendering when progressive tiles are enabled.
  Layer* root = GetRoot();
  if (!gfxPlatform::GetPlatform()->UseProgressivePaint() || !root) {
    return 1.f;
  }

  FrameMetrics rootMetrics = LayerMetricsWrapper::TopmostScrollableMetrics(root);
  if (!rootMetrics.IsScrollable()) {
    // The root may not have any scrollable metrics, in which case rootMetrics
    // will just be an empty FrameMetrics. Instead use the actual metrics from
    // the root layer.
    rootMetrics = LayerMetricsWrapper(root).Metrics();
  }
  ParentLayerIntRect bounds = RoundedToInt(rootMetrics.GetCompositionBounds());
  IntRect screenRect(bounds.x,
                       bounds.y,
                       bounds.width,
                       bounds.height);

  float lowPrecisionMultiplier = 1.0f;
  float highPrecisionMultiplier = 1.0f;

#ifdef MOZ_WIDGET_ANDROID
  // Use the transform on the primary scrollable layer and its FrameMetrics
  // to find out how much of the viewport the current displayport covers
  nsTArray<Layer*> rootScrollableLayers;
  GetRootScrollableLayers(rootScrollableLayers);
  if (rootScrollableLayers.Length() > 0) {
    // This is derived from the code in
    // AsyncCompositionManager::TransformScrollableLayer
    Layer* rootScrollable = rootScrollableLayers[0];
    const FrameMetrics& metrics = LayerMetricsWrapper::TopmostScrollableMetrics(rootScrollable);
    Matrix4x4 transform = rootScrollable->GetEffectiveTransform();
    transform.PostScale(metrics.GetPresShellResolution(), metrics.GetPresShellResolution(), 1);

    // Clip the screen rect to the document bounds
    Rect documentBounds =
      transform.TransformBounds(Rect(metrics.GetScrollableRect().x - metrics.GetScrollOffset().x,
                                     metrics.GetScrollableRect().y - metrics.GetScrollOffset().y,
                                     metrics.GetScrollableRect().width,
                                     metrics.GetScrollableRect().height));
    documentBounds.RoundOut();
    screenRect = screenRect.Intersect(IntRect(documentBounds.x, documentBounds.y,
                                                documentBounds.width, documentBounds.height));

    // If the screen rect is empty, the user has scrolled entirely into
    // over-scroll and so we can be considered to have full integrity.
    if (screenRect.IsEmpty()) {
      return 1.0f;
    }

    // Work out how much of the critical display-port covers the screen
    bool hasLowPrecision = false;
    if (!metrics.GetCriticalDisplayPort().IsEmpty()) {
      hasLowPrecision = true;
      highPrecisionMultiplier =
        GetDisplayportCoverage(metrics.GetCriticalDisplayPort(), transform, screenRect);
    }

    // Work out how much of the display-port covers the screen
    if (!metrics.GetDisplayPort().IsEmpty()) {
      if (hasLowPrecision) {
        lowPrecisionMultiplier =
          GetDisplayportCoverage(metrics.GetDisplayPort(), transform, screenRect);
      } else {
        lowPrecisionMultiplier = highPrecisionMultiplier =
          GetDisplayportCoverage(metrics.GetDisplayPort(), transform, screenRect);
      }
    }
  }

  // If none of the screen is covered, we have zero integrity.
  if (highPrecisionMultiplier <= 0.0f && lowPrecisionMultiplier <= 0.0f) {
    return 0.0f;
  }
#endif // MOZ_WIDGET_ANDROID

  nsIntRegion screenRegion(screenRect);
  nsIntRegion lowPrecisionScreenRegion(screenRect);
  Matrix4x4 transform;
  ComputeRenderIntegrityInternal(root, screenRegion,
                                 lowPrecisionScreenRegion, transform);

  if (!screenRegion.IsEqual(screenRect)) {
    // Calculate the area of the region. All rects in an nsRegion are
    // non-overlapping.
    float screenArea = screenRect.width * screenRect.height;
    float highPrecisionIntegrity = screenRegion.Area() / screenArea;
    float lowPrecisionIntegrity = 1.f;
    if (!lowPrecisionScreenRegion.IsEqual(screenRect)) {
      lowPrecisionIntegrity = lowPrecisionScreenRegion.Area() / screenArea;
    }

    return ((highPrecisionIntegrity * highPrecisionMultiplier) +
            (lowPrecisionIntegrity * lowPrecisionMultiplier)) / 2;
  }

  return 1.f;
}

already_AddRefed<PaintedLayerComposite>
LayerManagerComposite::CreatePaintedLayerComposite()
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return nsRefPtr<PaintedLayerComposite>(new PaintedLayerComposite(this)).forget();
}

already_AddRefed<ContainerLayerComposite>
LayerManagerComposite::CreateContainerLayerComposite()
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return nsRefPtr<ContainerLayerComposite>(new ContainerLayerComposite(this)).forget();
}

already_AddRefed<ImageLayerComposite>
LayerManagerComposite::CreateImageLayerComposite()
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return nsRefPtr<ImageLayerComposite>(new ImageLayerComposite(this)).forget();
}

already_AddRefed<ColorLayerComposite>
LayerManagerComposite::CreateColorLayerComposite()
{
  if (LayerManagerComposite::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return nsRefPtr<ColorLayerComposite>(new ColorLayerComposite(this)).forget();
}

already_AddRefed<CanvasLayerComposite>
LayerManagerComposite::CreateCanvasLayerComposite()
{
  if (LayerManagerComposite::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return nsRefPtr<CanvasLayerComposite>(new CanvasLayerComposite(this)).forget();
}

already_AddRefed<RefLayerComposite>
LayerManagerComposite::CreateRefLayerComposite()
{
  if (LayerManagerComposite::mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return nsRefPtr<RefLayerComposite>(new RefLayerComposite(this)).forget();
}

LayerManagerComposite::AutoAddMaskEffect::AutoAddMaskEffect(Layer* aMaskLayer,
                                                            EffectChain& aEffects,
                                                            bool aIs3D)
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

  if (!mCompositable->AddMaskEffect(aEffects, aMaskLayer->GetEffectiveTransform(), aIs3D)) {
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

TemporaryRef<DrawTarget>
LayerManagerComposite::CreateDrawTarget(const IntSize &aSize,
                                        SurfaceFormat aFormat)
{
#ifdef XP_MACOSX
  // We don't want to accelerate if the surface is too small which indicates
  // that it's likely used for an icon/static image. We also don't want to
  // accelerate anything that is above the maximum texture size of weakest gpu.
  // Safari uses 5000 area as the minimum for acceleration, we decided 64^2 is more logical.
  bool useAcceleration = aSize.width <= 4096 && aSize.height <= 4096 &&
                         aSize.width > 64 && aSize.height > 64 &&
                         gfxPlatformMac::GetPlatform()->UseAcceleratedCanvas();
  if (useAcceleration) {
    return Factory::CreateDrawTarget(BackendType::COREGRAPHICS_ACCELERATED,
                                     aSize, aFormat);
  }
#endif
  return LayerManager::CreateDrawTarget(aSize, aFormat);
}

LayerComposite::LayerComposite(LayerManagerComposite *aManager)
  : mCompositeManager(aManager)
  , mCompositor(aManager->GetCompositor())
  , mShadowOpacity(1.0)
  , mShadowTransformSetByAnimation(false)
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
  if (mFPS) {
    mFPS->NotifyShadowTreeTransaction();
  }
}

void
LayerComposite::SetLayerManager(LayerManagerComposite* aManager)
{
  mCompositeManager = aManager;
  mCompositor = aManager->GetCompositor();
}

nsIntRegion
LayerComposite::GetFullyRenderedRegion() {
  if (TiledLayerComposer* tiled = GetTiledLayerComposer()) {
    nsIntRegion shadowVisibleRegion = GetShadowVisibleRegion();
    // Discard the region which hasn't been drawn yet when doing
    // progressive drawing. Note that if the shadow visible region
    // shrunk the tiled valig region may not have discarded this yet.
    shadowVisibleRegion.And(shadowVisibleRegion, tiled->GetValidRegion());
    return shadowVisibleRegion;
  } else {
    return GetShadowVisibleRegion();
  }
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

} /* layers */
} /* mozilla */
