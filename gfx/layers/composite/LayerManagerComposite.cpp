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
#include "ThebesLayerComposite.h"       // for ThebesLayerComposite
#include "TiledLayerBuffer.h"           // for TiledLayerComposer
#include "Units.h"                      // for ScreenIntRect
#include "gfx2DGlue.h"                  // for ToMatrix4x4
#include "gfx3DMatrix.h"                // for gfx3DMatrix
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
#include "mozilla/layers/LayersTypes.h"  // for etc
#include "ipc/CompositorBench.h"        // for CompositorBench
#include "ipc/ShadowLayerUtils.h"
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_WARNING, NS_RUNTIMEABORT, etc
#include "nsISupportsImpl.h"            // for Layer::AddRef, etc
#include "nsIWidget.h"                  // for nsIWidget
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for nsIntRect
#include "nsRegion.h"                   // for nsIntRegion, etc
#ifdef MOZ_WIDGET_ANDROID
#include <android/log.h>
#endif
#include "GeckoProfiler.h"
#include "TextRenderer.h"               // for TextRenderer

class gfxContext;
struct nsIntSize;


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
: mCompositor(aCompositor)
, mInTransaction(false)
, mIsCompositorReady(false)
, mDebugOverlayWantsNextFrame(false)
, mGeometryChanged(true)
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
LayerManagerComposite::UpdateRenderBounds(const nsIntRect& aRect)
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

  if (Compositor::GetBackend() == LayersBackend::LAYERS_OPENGL ||
      Compositor::GetBackend() == LayersBackend::LAYERS_BASIC) {
    mClonedLayerTreeProperties = LayerProperties::CloneFrom(GetRoot());
  }
}

void
LayerManagerComposite::BeginTransactionWithDrawTarget(DrawTarget* aTarget, const nsIntRect& aRect)
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
LayerManagerComposite::EndTransaction(DrawThebesLayerCallback aCallback,
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

    Render();
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

already_AddRefed<ThebesLayer>
LayerManagerComposite::CreateThebesLayer()
{
  NS_RUNTIMEABORT("Should only be called on the drawing side");
  return nullptr;
}

already_AddRefed<ContainerLayer>
LayerManagerComposite::CreateContainerLayer()
{
  NS_RUNTIMEABORT("Should only be called on the drawing side");
  return nullptr;
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
  NS_RUNTIMEABORT("Should only be called on the drawing side");
  return nullptr;
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

  if (drawFps) {
    if (!mFPS) {
      mFPS = new FPSState();
    }

    float fillRatio = mCompositor->GetFillRatio();
    mFPS->DrawFPS(TimeStamp::Now(), drawFrameColorBars ? 10 : 0, 0, unsigned(fillRatio), mCompositor);
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

void
LayerManagerComposite::Render()
{
  PROFILER_LABEL("LayerManagerComposite", "Render",
    js::ProfileEntry::Category::GRAPHICS);

  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return;
  }

  if (gfxPrefs::LayersDump()) {
    this->Dump();
  }

  /** Our more efficient but less powerful alter ego, if one is available. */
  nsRefPtr<Composer2D> composer2D = mCompositor->GetWidget()->GetComposer2D();

  // Set LayerScope begin/end frame
  LayerScopeAutoFrame frame(PR_Now());

  if (!mTarget && composer2D && composer2D->TryRender(mRoot, mWorldMatrix, mGeometryChanged)) {
    if (mFPS) {
      double fps = mFPS->mCompositionFps.AddFrameAndGetFps(TimeStamp::Now());
      if (gfxPrefs::LayersDrawFPS()) {
        printf_stderr("HWComposer: FPS is %g\n", fps);
      }
    }
    mCompositor->EndFrameForExternalComposition(mWorldMatrix);
    // Reset the invalid region as compositing is done
    mInvalidRegion.SetEmpty();
    return;
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

  nsIntRect clipRect;
  Rect bounds(mRenderBounds.x, mRenderBounds.y, mRenderBounds.width, mRenderBounds.height);
  Rect actualBounds;

  CompositorBench(mCompositor, bounds);

  if (mRoot->GetClipRect()) {
    clipRect = *mRoot->GetClipRect();
    WorldTransformRect(clipRect);
    Rect rect(clipRect.x, clipRect.y, clipRect.width, clipRect.height);
    mCompositor->BeginFrame(invalid, &rect, mWorldMatrix, bounds, nullptr, &actualBounds);
  } else {
    gfx::Rect rect;
    mCompositor->BeginFrame(invalid, nullptr, mWorldMatrix, bounds, &rect, &actualBounds);
    clipRect = nsIntRect(rect.x, rect.y, rect.width, rect.height);
  }

  if (actualBounds.IsEmpty()) {
    mCompositor->GetWidget()->PostRender(this);
    return;
  }

  // Allow widget to render a custom background.
  mCompositor->GetWidget()->DrawWindowUnderlay(this, nsIntRect(actualBounds.x,
                                                               actualBounds.y,
                                                               actualBounds.width,
                                                               actualBounds.height));

  // Render our layers.
  RootLayer()->RenderLayer(clipRect);

  if (!mRegionToClear.IsEmpty()) {
    nsIntRegionRectIterator iter(mRegionToClear);
    const nsIntRect *r;
    while ((r = iter.Next())) {
      mCompositor->ClearRect(Rect(r->x, r->y, r->width, r->height));
    }
  }

  // Allow widget to render a custom foreground.
  mCompositor->GetWidget()->DrawWindowOverlay(this, nsIntRect(actualBounds.x,
                                                              actualBounds.y,
                                                              actualBounds.width,
                                                              actualBounds.height));

  // Debugging
  RenderDebugOverlay(actualBounds);

  {
    PROFILER_LABEL("LayerManagerComposite", "EndFrame",
      js::ProfileEntry::Category::GRAPHICS);

    mCompositor->EndFrame();
    mCompositor->SetFBAcquireFence(mRoot);
  }

  mCompositor->GetWidget()->PostRender(this);

  RecordFrame();
}

void
LayerManagerComposite::SetWorldTransform(const gfx::Matrix& aMatrix)
{
  NS_ASSERTION(aMatrix.PreservesAxisAlignedRectangles(),
               "SetWorldTransform only accepts matrices that satisfy PreservesAxisAlignedRectangles");
  NS_ASSERTION(!aMatrix.HasNonIntegerScale(),
               "SetWorldTransform only accepts matrices with integer scale");

  mWorldMatrix = aMatrix;
}

gfx::Matrix&
LayerManagerComposite::GetWorldTransform(void)
{
  return mWorldMatrix;
}

void
LayerManagerComposite::WorldTransformRect(nsIntRect& aRect)
{
  gfx::Rect grect(aRect.x, aRect.y, aRect.width, aRect.height);
  grect = mWorldMatrix.TransformBounds(grect);
  aRect.SetRect(grect.X(), grect.Y(), grect.Width(), grect.Height());
}

static void
SubtractTransformedRegion(nsIntRegion& aRegion,
                          const nsIntRegion& aRegionToSubtract,
                          const gfx3DMatrix& aTransform)
{
  if (aRegionToSubtract.IsEmpty()) {
    return;
  }

  // For each rect in the region, find out its bounds in screen space and
  // subtract it from the screen region.
  nsIntRegionRectIterator it(aRegionToSubtract);
  while (const nsIntRect* rect = it.Next()) {
    gfxRect incompleteRect = aTransform.TransformBounds(gfxRect(*rect));
    aRegion.Sub(aRegion, nsIntRect(incompleteRect.x,
                                   incompleteRect.y,
                                   incompleteRect.width,
                                   incompleteRect.height));
  }
}

/* static */ void
LayerManagerComposite::ComputeRenderIntegrityInternal(Layer* aLayer,
                                                      nsIntRegion& aScreenRegion,
                                                      nsIntRegion& aLowPrecisionScreenRegion,
                                                      const gfx3DMatrix& aTransform)
{
  if (aLayer->GetOpacity() <= 0.f ||
      (aScreenRegion.IsEmpty() && aLowPrecisionScreenRegion.IsEmpty())) {
    return;
  }

  // If the layer's a container, recurse into all of its children
  ContainerLayer* container = aLayer->AsContainerLayer();
  if (container) {
    // Accumulate the transform of intermediate surfaces
    gfx3DMatrix transform = aTransform;
    if (container->UseIntermediateSurface()) {
      gfx::To3DMatrix(aLayer->GetEffectiveTransform(), transform);
      transform.PreMultiply(aTransform);
    }
    for (Layer* child = aLayer->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      ComputeRenderIntegrityInternal(child, aScreenRegion, aLowPrecisionScreenRegion, transform);
    }
    return;
  }

  // Only thebes layers can be incomplete
  ThebesLayer* thebesLayer = aLayer->AsThebesLayer();
  if (!thebesLayer) {
    return;
  }

  // See if there's any incomplete rendering
  nsIntRegion incompleteRegion = aLayer->GetEffectiveVisibleRegion();
  incompleteRegion.Sub(incompleteRegion, thebesLayer->GetValidRegion());

  if (!incompleteRegion.IsEmpty()) {
    // Calculate the transform to get between screen and layer space
    gfx3DMatrix transformToScreen;
    To3DMatrix(aLayer->GetEffectiveTransform(), transformToScreen);
    transformToScreen.PreMultiply(aTransform);

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

#ifdef MOZ_ANDROID_OMTC
static float
GetDisplayportCoverage(const CSSRect& aDisplayPort,
                       const gfx3DMatrix& aTransformToScreen,
                       const nsIntRect& aScreenRect)
{
  gfxRect transformedDisplayport =
    aTransformToScreen.TransformBounds(gfxRect(aDisplayPort.x,
                                               aDisplayPort.y,
                                               aDisplayPort.width,
                                               aDisplayPort.height));
  transformedDisplayport.RoundOut();
  nsIntRect displayport = nsIntRect(transformedDisplayport.x,
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
#endif // MOZ_ANDROID_OMTC

float
LayerManagerComposite::ComputeRenderIntegrity()
{
  // We only ever have incomplete rendering when progressive tiles are enabled.
  Layer* root = GetRoot();
  if (!gfxPrefs::UseProgressiveTilePainting() || !root) {
    return 1.f;
  }

  const FrameMetrics& rootMetrics = root->AsContainerLayer()->GetFrameMetrics();
  ParentLayerIntRect bounds = RoundedToInt(rootMetrics.mCompositionBounds);
  nsIntRect screenRect(bounds.x,
                       bounds.y,
                       bounds.width,
                       bounds.height);

  float lowPrecisionMultiplier = 1.0f;
  float highPrecisionMultiplier = 1.0f;

#ifdef MOZ_ANDROID_OMTC
  // Use the transform on the primary scrollable layer and its FrameMetrics
  // to find out how much of the viewport the current displayport covers
  Layer* primaryScrollable = GetPrimaryScrollableLayer();
  if (primaryScrollable) {
    // This is derived from the code in
    // AsyncCompositionManager::TransformScrollableLayer
    const FrameMetrics& metrics = primaryScrollable->AsContainerLayer()->GetFrameMetrics();
    gfx3DMatrix transform;
    gfx::To3DMatrix(primaryScrollable->GetEffectiveTransform(), transform);
    transform.ScalePost(metrics.mResolution.scale, metrics.mResolution.scale, 1);

    // Clip the screen rect to the document bounds
    gfxRect documentBounds =
      transform.TransformBounds(gfxRect(metrics.mScrollableRect.x - metrics.GetScrollOffset().x,
                                        metrics.mScrollableRect.y - metrics.GetScrollOffset().y,
                                        metrics.mScrollableRect.width,
                                        metrics.mScrollableRect.height));
    documentBounds.RoundOut();
    screenRect = screenRect.Intersect(nsIntRect(documentBounds.x, documentBounds.y,
                                                documentBounds.width, documentBounds.height));

    // If the screen rect is empty, the user has scrolled entirely into
    // over-scroll and so we can be considered to have full integrity.
    if (screenRect.IsEmpty()) {
      return 1.0f;
    }

    // Work out how much of the critical display-port covers the screen
    bool hasLowPrecision = false;
    if (!metrics.mCriticalDisplayPort.IsEmpty()) {
      hasLowPrecision = true;
      highPrecisionMultiplier =
        GetDisplayportCoverage(metrics.mCriticalDisplayPort, transform, screenRect);
    }

    // Work out how much of the display-port covers the screen
    if (!metrics.mDisplayPort.IsEmpty()) {
      if (hasLowPrecision) {
        lowPrecisionMultiplier =
          GetDisplayportCoverage(metrics.mDisplayPort, transform, screenRect);
      } else {
        lowPrecisionMultiplier = highPrecisionMultiplier =
          GetDisplayportCoverage(metrics.mDisplayPort, transform, screenRect);
      }
    }
  }

  // If none of the screen is covered, we have zero integrity.
  if (highPrecisionMultiplier <= 0.0f && lowPrecisionMultiplier <= 0.0f) {
    return 0.0f;
  }
#endif // MOZ_ANDROID_OMTC

  nsIntRegion screenRegion(screenRect);
  nsIntRegion lowPrecisionScreenRegion(screenRect);
  gfx3DMatrix transform;
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

already_AddRefed<ThebesLayerComposite>
LayerManagerComposite::CreateThebesLayerComposite()
{
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return nullptr;
  }
  return nsRefPtr<ThebesLayerComposite>(new ThebesLayerComposite(this)).forget();
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
  : mCompositable(nullptr)
{
  if (!aMaskLayer) {
    return;
  }

  mCompositable = ToLayerComposite(aMaskLayer)->GetCompositableHost();
  if (!mCompositable) {
    NS_WARNING("Mask layer with no compositable host");
    return;
  }

  if (!mCompositable->AddMaskEffect(aEffects, aMaskLayer->GetEffectiveTransform(), aIs3D)) {
    mCompositable = nullptr;
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
  , mUseShadowClipRect(false)
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
