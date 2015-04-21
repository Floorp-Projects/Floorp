/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientTiledPaintedLayer.h"
#include "FrameMetrics.h"               // for FrameMetrics
#include "Units.h"                      // for ScreenIntRect, CSSPoint, etc
#include "UnitTransforms.h"             // for TransformTo
#include "ClientLayerManager.h"         // for ClientLayerManager, etc
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxPrefs.h"                   // for gfxPrefs
#include "gfxRect.h"                    // for gfxRect
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/gfx/Rect.h"           // for Rect, RectTyped
#include "mozilla/layers/CompositorChild.h"
#include "mozilla/layers/LayerMetricsWrapper.h" // for LayerMetricsWrapper
#include "mozilla/layers/LayersMessages.h"
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "LayersLogging.h"

namespace mozilla {
namespace layers {

ClientTiledPaintedLayer::ClientTiledPaintedLayer(ClientLayerManager* const aManager,
                                               ClientLayerManager::PaintedLayerCreationHint aCreationHint)
  : PaintedLayer(aManager, static_cast<ClientLayer*>(this), aCreationHint)
  , mContentClient()
{
  MOZ_COUNT_CTOR(ClientTiledPaintedLayer);
  mPaintData.mLastScrollOffset = ParentLayerPoint(0, 0);
  mPaintData.mFirstPaint = true;
}

ClientTiledPaintedLayer::~ClientTiledPaintedLayer()
{
  MOZ_COUNT_DTOR(ClientTiledPaintedLayer);
}

void
ClientTiledPaintedLayer::ClearCachedResources()
{
  if (mContentClient) {
    mContentClient->ClearCachedResources();
  }
  mValidRegion.SetEmpty();
  mContentClient = nullptr;
}

void
ClientTiledPaintedLayer::FillSpecificAttributes(SpecificLayerAttributes& aAttrs)
{
  aAttrs = PaintedLayerAttributes(GetValidRegion());
}

static LayerRect
ApplyParentLayerToLayerTransform(const gfx::Matrix4x4& aTransform, const ParentLayerRect& aParentLayerRect)
{
  return TransformTo<LayerPixel>(aTransform, aParentLayerRect);
}

static gfx::Matrix4x4
GetTransformToAncestorsParentLayer(Layer* aStart, const LayerMetricsWrapper& aAncestor)
{
  gfx::Matrix4x4 transform;
  const LayerMetricsWrapper& ancestorParent = aAncestor.GetParent();
  for (LayerMetricsWrapper iter(aStart, LayerMetricsWrapper::StartAt::BOTTOM);
       ancestorParent ? iter != ancestorParent : iter.IsValid();
       iter = iter.GetParent()) {
    transform = transform * iter.GetTransform();

    if (gfxPrefs::LayoutUseContainersForRootFrames()) {
      // When scrolling containers, layout adds a post-scale into the transform
      // of the displayport-ancestor (which we pick up in GetTransform() above)
      // to cancel out the pres shell resolution (for historical reasons). The
      // compositor in turn cancels out this post-scale (i.e., scales by the
      // pres shell resolution), and to get correct calculations, we need to do
      // so here, too.
      //
      // With containerless scrolling, the offending post-scale is on the
      // parent layer of the displayport-ancestor, which we don't reach in this
      // loop, so we don't need to worry about it.
      const FrameMetrics& metrics = iter.Metrics();
      transform.PostScale(metrics.GetPresShellResolution(), metrics.GetPresShellResolution(), 1.f);
    }
  }
  return transform;
}

void
ClientTiledPaintedLayer::GetAncestorLayers(LayerMetricsWrapper* aOutScrollAncestor,
                                           LayerMetricsWrapper* aOutDisplayPortAncestor,
                                           bool* aOutHasTransformAnimation)
{
  LayerMetricsWrapper scrollAncestor;
  LayerMetricsWrapper displayPortAncestor;
  bool hasTransformAnimation = false;
  for (LayerMetricsWrapper ancestor(this, LayerMetricsWrapper::StartAt::BOTTOM); ancestor; ancestor = ancestor.GetParent()) {
    hasTransformAnimation |= ancestor.HasTransformAnimation();
    const FrameMetrics& metrics = ancestor.Metrics();
    if (!scrollAncestor && metrics.GetScrollId() != FrameMetrics::NULL_SCROLL_ID) {
      scrollAncestor = ancestor;
    }
    if (!metrics.GetDisplayPort().IsEmpty()) {
      displayPortAncestor = ancestor;
      // Any layer that has a displayport must be scrollable, so we can break
      // here.
      break;
    }
  }
  if (aOutScrollAncestor) {
    *aOutScrollAncestor = scrollAncestor;
  }
  if (aOutDisplayPortAncestor) {
    *aOutDisplayPortAncestor = displayPortAncestor;
  }
  if (aOutHasTransformAnimation) {
    *aOutHasTransformAnimation = hasTransformAnimation;
  }
}

void
ClientTiledPaintedLayer::BeginPaint()
{
  mPaintData.mLowPrecisionPaintCount = 0;
  mPaintData.mPaintFinished = false;
  mPaintData.mCompositionBounds.SetEmpty();
  mPaintData.mCriticalDisplayPort.SetEmpty();

  if (!GetBaseTransform().Is2D()) {
    // Give up if there is a complex CSS transform on the layer. We might
    // eventually support these but for now it's too complicated to handle
    // given that it's a pretty rare scenario.
    return;
  }

  // Get the metrics of the nearest scrollable layer and the nearest layer
  // with a displayport.
  LayerMetricsWrapper scrollAncestor;
  LayerMetricsWrapper displayPortAncestor;
  bool hasTransformAnimation;
  GetAncestorLayers(&scrollAncestor, &displayPortAncestor, &hasTransformAnimation);

  if (!displayPortAncestor || !scrollAncestor) {
    // No displayport or scroll ancestor, so we can't do progressive rendering.
#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_B2G)
    // Both Android and b2g are guaranteed to have a displayport set, so this
    // should never happen.
    NS_WARNING("Tiled PaintedLayer with no scrollable container ancestor");
#endif
    return;
  }

  TILING_LOG("TILING %p: Found scrollAncestor %p, displayPortAncestor %p, transform %d\n", this,
    scrollAncestor.GetLayer(), displayPortAncestor.GetLayer(), hasTransformAnimation);

  const FrameMetrics& scrollMetrics = scrollAncestor.Metrics();
  const FrameMetrics& displayportMetrics = displayPortAncestor.Metrics();

  // Calculate the transform required to convert ParentLayer space of our
  // display port ancestor to the Layer space of this layer.
  gfx::Matrix4x4 transformDisplayPortToLayer =
    GetTransformToAncestorsParentLayer(this, displayPortAncestor);
  transformDisplayPortToLayer.Invert();

  // Compute the critical display port that applies to this layer in the
  // LayoutDevice space of this layer, but only if there is no OMT animation
  // on this layer. If there is an OMT animation then we need to draw the whole
  // visible region of this layer as determined by layout, because we don't know
  // what parts of it might move into view in the compositor.
  if (!hasTransformAnimation) {
    ParentLayerRect criticalDisplayPort =
      (displayportMetrics.GetCriticalDisplayPort() * displayportMetrics.GetZoom())
      + displayportMetrics.mCompositionBounds.TopLeft();
    mPaintData.mCriticalDisplayPort = RoundedToInt(
      ApplyParentLayerToLayerTransform(transformDisplayPortToLayer, criticalDisplayPort));
  }
  TILING_LOG("TILING %p: Critical displayport %s\n", this, Stringify(mPaintData.mCriticalDisplayPort).c_str());

  // Store the resolution from the displayport ancestor layer. Because this is Gecko-side,
  // before any async transforms have occurred, we can use the zoom for this.
  mPaintData.mResolution = displayportMetrics.GetZoom();
  TILING_LOG("TILING %p: Resolution %s\n", this, Stringify(mPaintData.mResolution).c_str());

  // Store the applicable composition bounds in this layer's Layer units.
  mPaintData.mTransformToCompBounds =
    GetTransformToAncestorsParentLayer(this, scrollAncestor);
  gfx::Matrix4x4 transformToBounds = mPaintData.mTransformToCompBounds;
  transformToBounds.Invert();
  mPaintData.mCompositionBounds = ApplyParentLayerToLayerTransform(
    transformToBounds, scrollMetrics.mCompositionBounds);
  TILING_LOG("TILING %p: Composition bounds %s\n", this, Stringify(mPaintData.mCompositionBounds).c_str());

  // Calculate the scroll offset since the last transaction
  mPaintData.mScrollOffset = displayportMetrics.GetScrollOffset() * displayportMetrics.GetZoom();
  TILING_LOG("TILING %p: Scroll offset %s\n", this, Stringify(mPaintData.mScrollOffset).c_str());
}

bool
ClientTiledPaintedLayer::IsScrollingOnCompositor(const FrameMetrics& aParentMetrics)
{
  CompositorChild* compositor = nullptr;
  if (Manager() && Manager()->AsClientLayerManager()) {
    compositor = Manager()->AsClientLayerManager()->GetCompositorChild();
  }

  if (!compositor) {
    return false;
  }

  FrameMetrics compositorMetrics;
  if (!compositor->LookupCompositorFrameMetrics(aParentMetrics.GetScrollId(),
                                                compositorMetrics)) {
    return false;
  }

  // 1 is a tad high for a fuzzy equals epsilon however if our scroll delta
  // is so small then we have nothing to gain from using paint heuristics.
  float COORDINATE_EPSILON = 1.f;

  return !FuzzyEqualsAdditive(compositorMetrics.GetScrollOffset().x,
                              aParentMetrics.GetScrollOffset().x,
                              COORDINATE_EPSILON) ||
         !FuzzyEqualsAdditive(compositorMetrics.GetScrollOffset().y,
                              aParentMetrics.GetScrollOffset().y,
                              COORDINATE_EPSILON);
}

bool
ClientTiledPaintedLayer::UseProgressiveDraw() {
  if (!gfxPlatform::GetPlatform()->UseProgressivePaint()) {
    // pref is disabled, so never do progressive
    return false;
  }

  if (ClientManager()->HasShadowTarget()) {
    // This condition is true when we are in a reftest scenario. We don't want
    // to draw progressively here because it can cause intermittent reftest
    // failures because the harness won't wait for all the tiles to be drawn.
    return false;
  }

  if (mPaintData.mCriticalDisplayPort.IsEmpty()) {
    // This catches three scenarios:
    // 1) This layer doesn't have a scrolling ancestor
    // 2) This layer is subject to OMTA transforms
    // 3) Low-precision painting is disabled
    // In all of these cases, we don't want to draw this layer progressively.
    return false;
  }

  if (GetIsFixedPosition() || GetParent()->GetIsFixedPosition()) {
    // This layer is fixed-position and so even if it does have a scrolling
    // ancestor it will likely be entirely on-screen all the time, so we
    // should draw it all at once
    return false;
  }

  if (gfxPrefs::AsyncPanZoomEnabled()) {
    LayerMetricsWrapper scrollAncestor;
    GetAncestorLayers(&scrollAncestor, nullptr, nullptr);
    MOZ_ASSERT(scrollAncestor); // because mPaintData.mCriticalDisplayPort is non-empty
    const FrameMetrics& parentMetrics = scrollAncestor.Metrics();
    if (!IsScrollingOnCompositor(parentMetrics)) {
      return false;
    }
  }

  return true;
}

bool
ClientTiledPaintedLayer::RenderHighPrecision(nsIntRegion& aInvalidRegion,
                                            const nsIntRegion& aVisibleRegion,
                                            LayerManager::DrawPaintedLayerCallback aCallback,
                                            void* aCallbackData)
{
  // If we have no high-precision stuff to draw, or we have started drawing low-precision
  // already, then we shouldn't do anything there.
  if (aInvalidRegion.IsEmpty() || mPaintData.mLowPrecisionPaintCount != 0) {
    return false;
  }

  // Only draw progressively when the resolution is unchanged
  if (UseProgressiveDraw() &&
      mContentClient->mTiledBuffer.GetFrameResolution() == mPaintData.mResolution) {
    // Store the old valid region, then clear it before painting.
    // We clip the old valid region to the visible region, as it only gets
    // used to decide stale content (currently valid and previously visible)
    nsIntRegion oldValidRegion = mContentClient->mTiledBuffer.GetValidRegion();
    oldValidRegion.And(oldValidRegion, aVisibleRegion);
    if (!mPaintData.mCriticalDisplayPort.IsEmpty()) {
      oldValidRegion.And(oldValidRegion, LayerIntRect::ToUntyped(mPaintData.mCriticalDisplayPort));
    }

    TILING_LOG("TILING %p: Progressive update with old valid region %s\n", this, Stringify(oldValidRegion).c_str());

    return mContentClient->mTiledBuffer.ProgressiveUpdate(mValidRegion, aInvalidRegion,
                      oldValidRegion, &mPaintData, aCallback, aCallbackData);
  }

  // Otherwise do a non-progressive paint

  mValidRegion = aVisibleRegion;
  if (!mPaintData.mCriticalDisplayPort.IsEmpty()) {
    mValidRegion.And(mValidRegion, LayerIntRect::ToUntyped(mPaintData.mCriticalDisplayPort));
  }

  TILING_LOG("TILING %p: Non-progressive paint invalid region %s\n", this, Stringify(aInvalidRegion).c_str());
  TILING_LOG("TILING %p: Non-progressive paint new valid region %s\n", this, Stringify(mValidRegion).c_str());

  mContentClient->mTiledBuffer.SetFrameResolution(mPaintData.mResolution);
  mContentClient->mTiledBuffer.PaintThebes(mValidRegion, aInvalidRegion, aCallback, aCallbackData);
  mPaintData.mPaintFinished = true;
  return true;
}

bool
ClientTiledPaintedLayer::RenderLowPrecision(nsIntRegion& aInvalidRegion,
                                           const nsIntRegion& aVisibleRegion,
                                           LayerManager::DrawPaintedLayerCallback aCallback,
                                           void* aCallbackData)
{
  // Render the low precision buffer, if the visible region is larger than the
  // critical display port.
  if (!nsIntRegion(LayerIntRect::ToUntyped(mPaintData.mCriticalDisplayPort)).Contains(aVisibleRegion)) {
    nsIntRegion oldValidRegion = mContentClient->mLowPrecisionTiledBuffer.GetValidRegion();
    oldValidRegion.And(oldValidRegion, aVisibleRegion);

    bool updatedBuffer = false;

    // If the frame resolution or format have changed, invalidate the buffer
    if (mContentClient->mLowPrecisionTiledBuffer.GetFrameResolution() != mPaintData.mResolution ||
        mContentClient->mLowPrecisionTiledBuffer.HasFormatChanged()) {
      if (!mLowPrecisionValidRegion.IsEmpty()) {
        updatedBuffer = true;
      }
      oldValidRegion.SetEmpty();
      mLowPrecisionValidRegion.SetEmpty();
      mContentClient->mLowPrecisionTiledBuffer.ResetPaintedAndValidState();
      mContentClient->mLowPrecisionTiledBuffer.SetFrameResolution(mPaintData.mResolution);
      aInvalidRegion = aVisibleRegion;
    }

    // Invalidate previously valid content that is no longer visible
    if (mPaintData.mLowPrecisionPaintCount == 1) {
      mLowPrecisionValidRegion.And(mLowPrecisionValidRegion, aVisibleRegion);
    }
    mPaintData.mLowPrecisionPaintCount++;

    // Remove the valid high-precision region from the invalid low-precision
    // region. We don't want to spend time drawing things twice.
    aInvalidRegion.Sub(aInvalidRegion, mValidRegion);

    TILING_LOG("TILING %p: Progressive paint: low-precision invalid region is %s\n", this, Stringify(aInvalidRegion).c_str());
    TILING_LOG("TILING %p: Progressive paint: low-precision old valid region is %s\n", this, Stringify(oldValidRegion).c_str());

    if (!aInvalidRegion.IsEmpty()) {
      updatedBuffer = mContentClient->mLowPrecisionTiledBuffer.ProgressiveUpdate(
                            mLowPrecisionValidRegion, aInvalidRegion, oldValidRegion,
                            &mPaintData, aCallback, aCallbackData);
    }

    TILING_LOG("TILING %p: Progressive paint: low-precision new valid region is %s\n", this, Stringify(mLowPrecisionValidRegion).c_str());
    return updatedBuffer;
  }
  if (!mLowPrecisionValidRegion.IsEmpty()) {
    TILING_LOG("TILING %p: Clearing low-precision buffer\n", this);
    // Clear the low precision tiled buffer.
    mLowPrecisionValidRegion.SetEmpty();
    mContentClient->mLowPrecisionTiledBuffer.ResetPaintedAndValidState();
    // Return true here so we send a Painted callback after clearing the valid
    // region of the low precision buffer. This allows the shadow buffer's valid
    // region to be updated and the associated resources to be freed.
    return true;
  }
  return false;
}

void
ClientTiledPaintedLayer::EndPaint()
{
  mPaintData.mLastScrollOffset = mPaintData.mScrollOffset;
  mPaintData.mPaintFinished = true;
  mPaintData.mFirstPaint = false;
  TILING_LOG("TILING %p: Paint finished\n", this);
}

void
ClientTiledPaintedLayer::RenderLayer()
{
  LayerManager::DrawPaintedLayerCallback callback =
    ClientManager()->GetPaintedLayerCallback();
  void *data = ClientManager()->GetPaintedLayerCallbackData();
  if (!callback) {
    ClientManager()->SetTransactionIncomplete();
    return;
  }

  if (!mContentClient) {
    mContentClient = new TiledContentClient(this, ClientManager());

    mContentClient->Connect();
    ClientManager()->AsShadowForwarder()->Attach(mContentClient, this);
    MOZ_ASSERT(mContentClient->GetForwarder());
  }

  if (mContentClient->mTiledBuffer.HasFormatChanged()) {
    mValidRegion = nsIntRegion();
    mContentClient->mTiledBuffer.ResetPaintedAndValidState();
  }

  TILING_LOG("TILING %p: Initial visible region %s\n", this, Stringify(mVisibleRegion).c_str());
  TILING_LOG("TILING %p: Initial valid region %s\n", this, Stringify(mValidRegion).c_str());
  TILING_LOG("TILING %p: Initial low-precision valid region %s\n", this, Stringify(mLowPrecisionValidRegion).c_str());

  nsIntRegion neededRegion = mVisibleRegion;
#ifndef MOZ_IGNORE_PAINT_WILL_RESAMPLE
  // This is handled by PadDrawTargetOutFromRegion in TiledContentClient for mobile
  if (MayResample()) {
    // If we're resampling then bilinear filtering can read up to 1 pixel
    // outside of our texture coords. Make the visible region a single rect,
    // and pad it out by 1 pixel (restricted to tile boundaries) so that
    // we always have valid content or transparent pixels to sample from.
    IntRect bounds = neededRegion.GetBounds();
    IntRect wholeTiles = bounds;
    wholeTiles.InflateToMultiple(IntSize(
      gfxPlatform::GetPlatform()->GetTileWidth(),
      gfxPlatform::GetPlatform()->GetTileHeight()));
    IntRect padded = bounds;
    padded.Inflate(1);
    padded.IntersectRect(padded, wholeTiles);
    neededRegion = padded;
  }
#endif

  nsIntRegion invalidRegion;
  invalidRegion.Sub(neededRegion, mValidRegion);
  if (invalidRegion.IsEmpty()) {
    EndPaint();
    return;
  }

  if (!ClientManager()->IsRepeatTransaction()) {
    // Only paint the mask layer on the first transaction.
    if (GetMaskLayer()) {
      ToClientLayer(GetMaskLayer())->RenderLayer();
    }

    // For more complex cases we need to calculate a bunch of metrics before we
    // can do the paint.
    BeginPaint();
    if (mPaintData.mPaintFinished) {
      return;
    }

    // Make sure that tiles that fall outside of the visible region or outside of the
    // critical displayport are discarded on the first update. Also make sure that we
    // only draw stuff inside the critical displayport on the first update.
    mValidRegion.And(mValidRegion, neededRegion);
    if (!mPaintData.mCriticalDisplayPort.IsEmpty()) {
      mValidRegion.And(mValidRegion, LayerIntRect::ToUntyped(mPaintData.mCriticalDisplayPort));
      invalidRegion.And(invalidRegion, LayerIntRect::ToUntyped(mPaintData.mCriticalDisplayPort));
    }

    TILING_LOG("TILING %p: First-transaction valid region %s\n", this, Stringify(mValidRegion).c_str());
    TILING_LOG("TILING %p: First-transaction invalid region %s\n", this, Stringify(invalidRegion).c_str());
  } else {
    if (!mPaintData.mCriticalDisplayPort.IsEmpty()) {
      invalidRegion.And(invalidRegion, LayerIntRect::ToUntyped(mPaintData.mCriticalDisplayPort));
    }
    TILING_LOG("TILING %p: Repeat-transaction invalid region %s\n", this, Stringify(invalidRegion).c_str());
  }

  nsIntRegion lowPrecisionInvalidRegion;
  if (gfxPrefs::UseLowPrecisionBuffer()) {
    // Calculate the invalid region for the low precision buffer. Make sure
    // to remove the valid high-precision area so we don't double-paint it.
    lowPrecisionInvalidRegion.Sub(neededRegion, mLowPrecisionValidRegion);
    lowPrecisionInvalidRegion.Sub(lowPrecisionInvalidRegion, mValidRegion);
  }
  TILING_LOG("TILING %p: Low-precision invalid region %s\n", this, Stringify(lowPrecisionInvalidRegion).c_str());

  bool updatedHighPrecision = RenderHighPrecision(invalidRegion,
                                                  neededRegion,
                                                  callback, data);
  if (updatedHighPrecision) {
    ClientManager()->Hold(this);
    mContentClient->UseTiledLayerBuffer(TiledContentClient::TILED_BUFFER);

    if (!mPaintData.mPaintFinished) {
      // There is still more high-res stuff to paint, so we're not
      // done yet. A subsequent transaction will take care of this.
      ClientManager()->SetRepeatTransaction();
      return;
    }
  }

  // If there is nothing to draw in low-precision, then we're done.
  if (lowPrecisionInvalidRegion.IsEmpty()) {
    EndPaint();
    return;
  }

  if (updatedHighPrecision) {
    // If there are low precision updates, but we just did some high-precision
    // updates, then mark the paint as unfinished and request a repeat transaction.
    // This is so that we don't perform low-precision updates in the same transaction
    // as high-precision updates.
    TILING_LOG("TILING %p: Scheduling repeat transaction for low-precision painting\n", this);
    ClientManager()->SetRepeatTransaction();
    mPaintData.mLowPrecisionPaintCount = 1;
    mPaintData.mPaintFinished = false;
    return;
  }

  bool updatedLowPrecision = RenderLowPrecision(lowPrecisionInvalidRegion,
                                                neededRegion,
                                                callback, data);
  if (updatedLowPrecision) {
    ClientManager()->Hold(this);
    mContentClient->UseTiledLayerBuffer(TiledContentClient::LOW_PRECISION_TILED_BUFFER);

    if (!mPaintData.mPaintFinished) {
      // There is still more low-res stuff to paint, so we're not
      // done yet. A subsequent transaction will take care of this.
      ClientManager()->SetRepeatTransaction();
      return;
    }
  }

  // If we get here, we've done all the high- and low-precision
  // paints we wanted to do, so we can finish the paint and chill.
  EndPaint();
}

void
ClientTiledPaintedLayer::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  PaintedLayer::PrintInfo(aStream, aPrefix);
  if (mContentClient) {
    aStream << "\n";
    nsAutoCString pfx(aPrefix);
    pfx += "  ";
    mContentClient->PrintInfo(aStream, pfx.get());
  }
}

} // mozilla
} // layers
