/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThebesLayerComposite.h"
#include "CompositableHost.h"           // for TiledLayerProperties, etc
#include "FrameMetrics.h"               // for FrameMetrics
#include "Units.h"                      // for CSSRect, LayerPixel, etc
#include "gfx2DGlue.h"                  // for ToMatrix4x4
#include "gfx3DMatrix.h"                // for gfx3DMatrix
#include "gfxImageSurface.h"            // for gfxImageSurface
#include "gfxUtils.h"                   // for gfxUtils, etc
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/gfx/Point.h"          // for Point
#include "mozilla/gfx/Rect.h"           // for RoundedToInt, Rect
#include "mozilla/gfx/Types.h"          // for Filter::FILTER_LINEAR
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/ContentHost.h"  // for ContentHost
#include "mozilla/layers/Effects.h"     // for EffectChain
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsAString.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsMathUtils.h"                // for NS_lround
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for nsIntRect
#include "nsSize.h"                     // for nsIntSize
#include "nsString.h"                   // for nsAutoCString
#include "nsTraceRefcnt.h"              // for MOZ_COUNT_CTOR, etc
#include "GeckoProfiler.h"

namespace mozilla {
namespace layers {

class TiledLayerComposer;

ThebesLayerComposite::ThebesLayerComposite(LayerManagerComposite *aManager)
  : ThebesLayer(aManager, nullptr)
  , LayerComposite(aManager)
  , mBuffer(nullptr)
  , mRequiresTiledProperties(false)
{
  MOZ_COUNT_CTOR(ThebesLayerComposite);
  mImplData = static_cast<LayerComposite*>(this);
}

ThebesLayerComposite::~ThebesLayerComposite()
{
  MOZ_COUNT_DTOR(ThebesLayerComposite);
  CleanupResources();
}

void
ThebesLayerComposite::SetCompositableHost(CompositableHost* aHost)
{
  mBuffer = static_cast<ContentHost*>(aHost);
}

void
ThebesLayerComposite::Disconnect()
{
  Destroy();
}

void
ThebesLayerComposite::Destroy()
{
  if (!mDestroyed) {
    CleanupResources();
    mDestroyed = true;
  }
}

Layer*
ThebesLayerComposite::GetLayer()
{
  return this;
}

TiledLayerComposer*
ThebesLayerComposite::GetTiledLayerComposer()
{
  MOZ_ASSERT(mBuffer && mBuffer->IsAttached());
  return mBuffer->AsTiledLayerComposer();
}

LayerRenderState
ThebesLayerComposite::GetRenderState()
{
  if (!mBuffer || !mBuffer->IsAttached() || mDestroyed) {
    return LayerRenderState();
  }
  return mBuffer->GetRenderState();
}

void
ThebesLayerComposite::RenderLayer(const nsIntPoint& aOffset,
                                  const nsIntRect& aClipRect)
{
  if (!mBuffer || !mBuffer->IsAttached()) {
    return;
  }
  PROFILER_LABEL("ThebesLayerComposite", "RenderLayer");

  MOZ_ASSERT(mBuffer->GetCompositor() == mCompositeManager->GetCompositor() &&
             mBuffer->GetLayer() == this,
             "buffer is corrupted");

  gfx::Matrix4x4 transform;
  ToMatrix4x4(GetEffectiveTransform(), transform);
  gfx::Rect clipRect(aClipRect.x, aClipRect.y, aClipRect.width, aClipRect.height);

#ifdef MOZ_DUMP_PAINTING
  if (gfxUtils::sDumpPainting) {
    nsRefPtr<gfxImageSurface> surf = mBuffer->GetAsSurface();
    WriteSnapshotToDumpFile(this, surf);
  }
#endif

  EffectChain effectChain;
  LayerManagerComposite::AutoAddMaskEffect autoMaskEffect(mMaskLayer, effectChain);

  nsIntRegion visibleRegion = GetEffectiveVisibleRegion();

  TiledLayerProperties tiledLayerProps;
  if (mRequiresTiledProperties) {
    // calculating these things can be a little expensive, so don't
    // do them if we don't have to
    tiledLayerProps.mVisibleRegion = visibleRegion;
    tiledLayerProps.mDisplayPort = GetDisplayPort();
    tiledLayerProps.mEffectiveResolution = GetEffectiveResolution();
    tiledLayerProps.mCompositionBounds = GetCompositionBounds();
    tiledLayerProps.mRetainTiles = !(mIsFixedPosition || mStickyPositionData);
    tiledLayerProps.mValidRegion = mValidRegion;
  }

  mBuffer->SetPaintWillResample(MayResample());

  mBuffer->Composite(effectChain,
                     GetEffectiveOpacity(),
                     transform,
                     gfx::Point(aOffset.x, aOffset.y),
                     gfx::FILTER_LINEAR,
                     clipRect,
                     &visibleRegion,
                     mRequiresTiledProperties ? &tiledLayerProps
                                              : nullptr);


  if (mRequiresTiledProperties) {
    mValidRegion = tiledLayerProps.mValidRegion;
  }

  mCompositeManager->GetCompositor()->MakeCurrent();
}

CompositableHost*
ThebesLayerComposite::GetCompositableHost()
{
  if ( mBuffer && mBuffer->IsAttached()) {
    return mBuffer.get();
  }

  return nullptr;
}

void
ThebesLayerComposite::CleanupResources()
{
  if (mBuffer) {
    mBuffer->Detach(this);
  }
  mBuffer = nullptr;
}

gfxSize
ThebesLayerComposite::GetEffectiveResolution()
{
  // Work out render resolution by multiplying the resolution of our ancestors.
  // Only container layers can have frame metrics, so we start off with a
  // resolution of 1, 1.
  // XXX For large layer trees, it would be faster to do this once from the
  //     root node upwards and store the value on each layer.
  gfxSize resolution(1, 1);
  for (ContainerLayer* parent = GetParent(); parent; parent = parent->GetParent()) {
    const FrameMetrics& metrics = parent->GetFrameMetrics();
    resolution.width *= metrics.mResolution.scale;
    resolution.height *= metrics.mResolution.scale;
  }

  return resolution;
}

gfxRect
ThebesLayerComposite::GetDisplayPort()
{
  // We use GetTransform instead of GetEffectiveTransform in this function
  // as we want the transform of the shadowable layers and not that of the
  // shadow layers, which may have been modified due to async scrolling/
  // zooming.
  gfx3DMatrix transform = GetTransform();

  // Find out the area of the nearest display-port to invalidate retained
  // tiles.
  gfxRect displayPort;
  gfxSize parentResolution = GetEffectiveResolution();
  for (ContainerLayer* parent = GetParent(); parent; parent = parent->GetParent()) {
    const FrameMetrics& metrics = parent->GetFrameMetrics();
    if (displayPort.IsEmpty()) {
      if (!metrics.mDisplayPort.IsEmpty()) {
          // We use the bounds to cut down on complication/computation time.
          // This will be incorrect when the transform involves rotation, but
          // it'd be quite hard to retain invalid tiles correctly in this
          // situation anyway.
          displayPort = gfxRect(metrics.mDisplayPort.x,
                                metrics.mDisplayPort.y,
                                metrics.mDisplayPort.width,
                                metrics.mDisplayPort.height);
          displayPort.ScaleRoundOut(parentResolution.width, parentResolution.height);
      }
      parentResolution.width /= metrics.mResolution.scale;
      parentResolution.height /= metrics.mResolution.scale;
    }
    if (parent->UseIntermediateSurface()) {
      transform.PreMultiply(parent->GetTransform());
    }
  }

  // If no display port was found, use the widget size from the layer manager.
  if (displayPort.IsEmpty()) {
    LayerManagerComposite* manager = static_cast<LayerManagerComposite*>(Manager());
    const nsIntSize& widgetSize = manager->GetWidgetSize();
    displayPort.width = widgetSize.width;
    displayPort.height = widgetSize.height;
  }

  // Transform the display port into layer space.
  displayPort = transform.Inverse().TransformBounds(displayPort);

  return displayPort;
}

gfxRect
ThebesLayerComposite::GetCompositionBounds()
{
  // Walk up the tree, looking for a display-port - if we find one, we know
  // that this layer represents a content node and we can use its first
  // scrollable child, in conjunction with its content area and viewport offset
  // to establish the screen coordinates to which the content area will be
  // rendered.
  gfxRect compositionBounds;
  ContainerLayer* scrollableLayer = nullptr;
  for (ContainerLayer* parent = GetParent(); parent; parent = parent->GetParent()) {
    const FrameMetrics& parentMetrics = parent->GetFrameMetrics();
    if (parentMetrics.IsScrollable())
      scrollableLayer = parent;
    if (!parentMetrics.mDisplayPort.IsEmpty() && scrollableLayer) {
      // Get the composition bounds, so as not to waste rendering time.
      compositionBounds = gfxRect(parentMetrics.mCompositionBounds.x,
                                  parentMetrics.mCompositionBounds.y,
                                  parentMetrics.mCompositionBounds.width,
                                  parentMetrics.mCompositionBounds.height);

      // Calculate the scale transform applied to the root layer to determine
      // the content resolution.
      Layer* rootLayer = Manager()->GetRoot();
      const gfx3DMatrix& rootTransform = rootLayer->GetTransform();
      LayerToCSSScale scale(rootTransform.GetXScale(),
                            rootTransform.GetYScale());

      // Get the content document bounds, in screen-space.
      const FrameMetrics& metrics = scrollableLayer->GetFrameMetrics();
      const LayerIntRect content = RoundedToInt(metrics.mScrollableRect / scale);
      // !!! WTF. this code is just wrong. See bug 881451.
      gfx::Point scrollOffset =
        gfx::Point((metrics.mScrollOffset.x * metrics.LayersPixelsPerCSSPixel().scale) / scale.scale,
                   (metrics.mScrollOffset.y * metrics.LayersPixelsPerCSSPixel().scale) / scale.scale);
      const nsIntPoint contentOrigin(
        content.x - NS_lround(scrollOffset.x),
        content.y - NS_lround(scrollOffset.y));
      gfxRect contentRect = gfxRect(contentOrigin.x, contentOrigin.y,
                                    content.width, content.height);
      gfxRect contentBounds = scrollableLayer->GetEffectiveTransform().
        TransformBounds(contentRect);

      // Clip the composition bounds to the content bounds
      compositionBounds.IntersectRect(compositionBounds, contentBounds);
      break;
    }
  }

  return compositionBounds;
}

#ifdef MOZ_LAYERS_HAVE_LOG
nsACString&
ThebesLayerComposite::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  ThebesLayer::PrintInfo(aTo, aPrefix);
  aTo += "\n";
  if (mBuffer && mBuffer->IsAttached()) {
    nsAutoCString pfx(aPrefix);
    pfx += "  ";
    mBuffer->PrintInfo(aTo, pfx.get());
  }
  return aTo;
}
#endif

} /* layers */
} /* mozilla */
