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
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/Rect.h"           // for Rect, RectTyped
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/LayerMetricsWrapper.h" // for LayerMetricsWrapper
#include "mozilla/layers/LayersMessages.h"
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "LayersLogging.h"
#include "mozilla/layers/SingleTiledContentClient.h"

namespace mozilla {
namespace layers {

using gfx::Rect;
using gfx::IntRect;
using gfx::IntSize;

ClientTiledPaintedLayer::ClientTiledPaintedLayer(ClientLayerManager* const aManager,
                                               ClientLayerManager::PaintedLayerCreationHint aCreationHint)
  : PaintedLayer(aManager, static_cast<ClientLayer*>(this), aCreationHint)
  , mContentClient()
  , mHaveSingleTiledContentClient(false)
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
  ClearValidRegion();
  mContentClient = nullptr;
}

void
ClientTiledPaintedLayer::FillSpecificAttributes(SpecificLayerAttributes& aAttrs)
{
  aAttrs = PaintedLayerAttributes(GetValidRegion());
}

static Maybe<LayerRect>
ApplyParentLayerToLayerTransform(const ParentLayerToLayerMatrix4x4& aTransform,
                                 const ParentLayerRect& aParentLayerRect,
                                 const LayerRect& aClip)
{
  return UntransformBy(aTransform, aParentLayerRect, aClip);
}

static LayerToParentLayerMatrix4x4
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
      float presShellResolution = iter.GetPresShellResolution();
      transform.PostScale(presShellResolution, presShellResolution, 1.0f);
    }
  }
  return ViewAs<LayerToParentLayerMatrix4x4>(transform);
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
  mPaintData.ResetPaintData();

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
#if defined(MOZ_WIDGET_ANDROID)
    // Android are guaranteed to have a displayport set, so this
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
  ParentLayerToLayerMatrix4x4 transformDisplayPortToLayer =
    GetTransformToAncestorsParentLayer(this, displayPortAncestor).Inverse();

  LayerRect layerBounds(GetVisibleRegion().GetBounds());

  // Compute the critical display port that applies to this layer in the
  // LayoutDevice space of this layer, but only if there is no OMT animation
  // on this layer. If there is an OMT animation then we need to draw the whole
  // visible region of this layer as determined by layout, because we don't know
  // what parts of it might move into view in the compositor.
  mPaintData.mHasTransformAnimation = hasTransformAnimation;
  if (!mPaintData.mHasTransformAnimation &&
      mContentClient->GetLowPrecisionTiledBuffer()) {
    ParentLayerRect criticalDisplayPort =
      (displayportMetrics.GetCriticalDisplayPort() * displayportMetrics.GetZoom())
      + displayportMetrics.GetCompositionBounds().TopLeft();
    Maybe<LayerRect> criticalDisplayPortTransformed =
      ApplyParentLayerToLayerTransform(transformDisplayPortToLayer, criticalDisplayPort, layerBounds);
    if (criticalDisplayPortTransformed) {
      mPaintData.mCriticalDisplayPort = Some(RoundedToInt(*criticalDisplayPortTransformed));
    } else {
      mPaintData.mCriticalDisplayPort = Some(LayerIntRect(0, 0, 0, 0));
    }
  }
  TILING_LOG("TILING %p: Critical displayport %s\n", this,
             mPaintData.mCriticalDisplayPort ?
             Stringify(*mPaintData.mCriticalDisplayPort).c_str() : "not set");

  // Store the resolution from the displayport ancestor layer. Because this is Gecko-side,
  // before any async transforms have occurred, we can use the zoom for this.
  mPaintData.mResolution = displayportMetrics.GetZoom();
  TILING_LOG("TILING %p: Resolution %s\n", this, Stringify(mPaintData.mResolution).c_str());

  // Store the applicable composition bounds in this layer's Layer units.
  mPaintData.mTransformToCompBounds =
    GetTransformToAncestorsParentLayer(this, scrollAncestor);
  ParentLayerToLayerMatrix4x4 transformToBounds = mPaintData.mTransformToCompBounds.Inverse();
  Maybe<LayerRect> compositionBoundsTransformed = ApplyParentLayerToLayerTransform(
    transformToBounds, scrollMetrics.GetCompositionBounds(), layerBounds);
  if (compositionBoundsTransformed) {
    mPaintData.mCompositionBounds = *compositionBoundsTransformed;
  } else {
    mPaintData.mCompositionBounds.SetEmpty();
  }
  TILING_LOG("TILING %p: Composition bounds %s\n", this, Stringify(mPaintData.mCompositionBounds).c_str());

  // Calculate the scroll offset since the last transaction
  mPaintData.mScrollOffset = displayportMetrics.GetScrollOffset() * displayportMetrics.GetZoom();
  TILING_LOG("TILING %p: Scroll offset %s\n", this, Stringify(mPaintData.mScrollOffset).c_str());
}

bool
ClientTiledPaintedLayer::IsScrollingOnCompositor(const FrameMetrics& aParentMetrics)
{
  CompositorBridgeChild* compositor = nullptr;
  if (Manager() && Manager()->AsClientLayerManager()) {
    compositor = Manager()->AsClientLayerManager()->GetCompositorBridgeChild();
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
  if (!gfxPrefs::ProgressivePaint()) {
    // pref is disabled, so never do progressive
    return false;
  }

  if (!mContentClient->GetTiledBuffer()->SupportsProgressiveUpdate()) {
    return false;
  }

  if (ClientManager()->HasShadowTarget()) {
    // This condition is true when we are in a reftest scenario. We don't want
    // to draw progressively here because it can cause intermittent reftest
    // failures because the harness won't wait for all the tiles to be drawn.
    return false;
  }

  if (GetIsFixedPosition() || GetParent()->GetIsFixedPosition()) {
    // This layer is fixed-position and so even if it does have a scrolling
    // ancestor it will likely be entirely on-screen all the time, so we
    // should draw it all at once
    return false;
  }

  if (mPaintData.mHasTransformAnimation) {
    // The compositor is going to animate this somehow, so we want it all
    // on the screen at once.
    return false;
  }

  if (ClientManager()->AsyncPanZoomEnabled()) {
    LayerMetricsWrapper scrollAncestor;
    GetAncestorLayers(&scrollAncestor, nullptr, nullptr);
    MOZ_ASSERT(scrollAncestor); // because mPaintData.mCriticalDisplayPort is set
    if (!scrollAncestor) {
      return false;
    }
    const FrameMetrics& parentMetrics = scrollAncestor.Metrics();
    if (!IsScrollingOnCompositor(parentMetrics)) {
      return false;
    }
  }

  return true;
}

bool
ClientTiledPaintedLayer::RenderHighPrecision(const nsIntRegion& aInvalidRegion,
                                            const nsIntRegion& aVisibleRegion,
                                            LayerManager::DrawPaintedLayerCallback aCallback,
                                            void* aCallbackData)
{
  // If we have started drawing low-precision already, then we
  // shouldn't do anything there.
  if (mPaintData.mLowPrecisionPaintCount != 0) {
    return false;
  }

  // Only draw progressively when there is something to paint and the
  // resolution is unchanged
  if (!aInvalidRegion.IsEmpty() &&
      UseProgressiveDraw() &&
      mContentClient->GetTiledBuffer()->GetFrameResolution() == mPaintData.mResolution) {
    // Store the old valid region, then clear it before painting.
    // We clip the old valid region to the visible region, as it only gets
    // used to decide stale content (currently valid and previously visible)
    nsIntRegion oldValidRegion = mContentClient->GetTiledBuffer()->GetValidRegion();
    oldValidRegion.And(oldValidRegion, aVisibleRegion);
    if (mPaintData.mCriticalDisplayPort) {
      oldValidRegion.And(oldValidRegion, mPaintData.mCriticalDisplayPort->ToUnknownRect());
    }

    TILING_LOG("TILING %p: Progressive update with old valid region %s\n", this, Stringify(oldValidRegion).c_str());

    nsIntRegion drawnRegion;
    bool updatedBuffer =
      mContentClient->GetTiledBuffer()->ProgressiveUpdate(GetValidRegion(), aInvalidRegion,
                      oldValidRegion, drawnRegion, &mPaintData, aCallback, aCallbackData);
    AddToValidRegion(drawnRegion);
    return updatedBuffer;
  }

  // Otherwise do a non-progressive paint. We must do this even when
  // the region to paint is empty as the valid region may have shrunk.

  nsIntRegion validRegion = aVisibleRegion;
  if (mPaintData.mCriticalDisplayPort) {
    validRegion.AndWith(mPaintData.mCriticalDisplayPort->ToUnknownRect());
  }
  SetValidRegion(validRegion);

  TILING_LOG("TILING %p: Non-progressive paint invalid region %s\n", this, Stringify(aInvalidRegion).c_str());
  TILING_LOG("TILING %p: Non-progressive paint new valid region %s\n", this, Stringify(GetValidRegion()).c_str());

  mContentClient->GetTiledBuffer()->SetFrameResolution(mPaintData.mResolution);
  mContentClient->GetTiledBuffer()->PaintThebes(GetValidRegion(), aInvalidRegion, aInvalidRegion,
                                                aCallback, aCallbackData);
  mPaintData.mPaintFinished = true;
  return true;
}

bool
ClientTiledPaintedLayer::RenderLowPrecision(const nsIntRegion& aInvalidRegion,
                                           const nsIntRegion& aVisibleRegion,
                                           LayerManager::DrawPaintedLayerCallback aCallback,
                                           void* aCallbackData)
{
  nsIntRegion invalidRegion = aInvalidRegion;

  // Render the low precision buffer, if the visible region is larger than the
  // critical display port.
  if (!mPaintData.mCriticalDisplayPort ||
      !nsIntRegion(mPaintData.mCriticalDisplayPort->ToUnknownRect()).Contains(aVisibleRegion)) {
    nsIntRegion oldValidRegion = mContentClient->GetLowPrecisionTiledBuffer()->GetValidRegion();
    oldValidRegion.And(oldValidRegion, aVisibleRegion);

    bool updatedBuffer = false;

    // If the frame resolution or format have changed, invalidate the buffer
    if (mContentClient->GetLowPrecisionTiledBuffer()->GetFrameResolution() != mPaintData.mResolution ||
        mContentClient->GetLowPrecisionTiledBuffer()->HasFormatChanged()) {
      if (!mLowPrecisionValidRegion.IsEmpty()) {
        updatedBuffer = true;
      }
      oldValidRegion.SetEmpty();
      mLowPrecisionValidRegion.SetEmpty();
      mContentClient->GetLowPrecisionTiledBuffer()->ResetPaintedAndValidState();
      mContentClient->GetLowPrecisionTiledBuffer()->SetFrameResolution(mPaintData.mResolution);
      invalidRegion = aVisibleRegion;
    }

    // Invalidate previously valid content that is no longer visible
    if (mPaintData.mLowPrecisionPaintCount == 1) {
      mLowPrecisionValidRegion.And(mLowPrecisionValidRegion, aVisibleRegion);
    }
    mPaintData.mLowPrecisionPaintCount++;

    // Remove the valid high-precision region from the invalid low-precision
    // region. We don't want to spend time drawing things twice.
    invalidRegion.SubOut(GetValidRegion());

    TILING_LOG("TILING %p: Progressive paint: low-precision invalid region is %s\n", this, Stringify(invalidRegion).c_str());
    TILING_LOG("TILING %p: Progressive paint: low-precision old valid region is %s\n", this, Stringify(oldValidRegion).c_str());

    if (!invalidRegion.IsEmpty()) {
      nsIntRegion drawnRegion;
      updatedBuffer = mContentClient->GetLowPrecisionTiledBuffer()->ProgressiveUpdate(
                            mLowPrecisionValidRegion, invalidRegion, oldValidRegion,
                            drawnRegion, &mPaintData, aCallback, aCallbackData);
      mLowPrecisionValidRegion.OrWith(drawnRegion);
    }

    TILING_LOG("TILING %p: Progressive paint: low-precision new valid region is %s\n", this, Stringify(mLowPrecisionValidRegion).c_str());
    return updatedBuffer;
  }
  if (!mLowPrecisionValidRegion.IsEmpty()) {
    TILING_LOG("TILING %p: Clearing low-precision buffer\n", this);
    // Clear the low precision tiled buffer.
    mLowPrecisionValidRegion.SetEmpty();
    mContentClient->GetLowPrecisionTiledBuffer()->ResetPaintedAndValidState();
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

  IntSize layerSize = mVisibleRegion.ToUnknownRegion().GetBounds().Size();
  IntSize tileSize = gfx::gfxVars::TileSize();
  bool isHalfTileWidthOrHeight = layerSize.width <= tileSize.width / 2 ||
    layerSize.height <= tileSize.height / 2;

  // Use single tile when layer is not scrollable, is smaller than one
  // tile, or when more than half of the tiles' pixels in either
  // dimension would be wasted.
  bool wantSingleTiledContentClient =
      (mCreationHint == LayerManager::NONE ||
       layerSize <= tileSize ||
       isHalfTileWidthOrHeight) &&
      SingleTiledContentClient::ClientSupportsLayerSize(layerSize, ClientManager()) &&
      gfxPrefs::LayersSingleTileEnabled();

  if (mContentClient && mHaveSingleTiledContentClient && !wantSingleTiledContentClient) {
    mContentClient = nullptr;
    ClearValidRegion();
  }

  if (!mContentClient) {
    if (wantSingleTiledContentClient) {
      mContentClient = new SingleTiledContentClient(*this, ClientManager());
      mHaveSingleTiledContentClient = true;
    } else {
      mContentClient = new MultiTiledContentClient(*this, ClientManager());
      mHaveSingleTiledContentClient = false;
    }

    mContentClient->Connect();
    ClientManager()->AsShadowForwarder()->Attach(mContentClient, this);
    MOZ_ASSERT(mContentClient->GetForwarder());
  }

  if (mContentClient->GetTiledBuffer()->HasFormatChanged()) {
    ClearValidRegion();
    mContentClient->GetTiledBuffer()->ResetPaintedAndValidState();
  }

  TILING_LOG("TILING %p: Initial visible region %s\n", this, Stringify(mVisibleRegion).c_str());
  TILING_LOG("TILING %p: Initial valid region %s\n", this, Stringify(GetValidRegion()).c_str());
  TILING_LOG("TILING %p: Initial low-precision valid region %s\n", this, Stringify(mLowPrecisionValidRegion).c_str());

  nsIntRegion neededRegion = mVisibleRegion.ToUnknownRegion();
#ifndef MOZ_IGNORE_PAINT_WILL_RESAMPLE
  // This is handled by PadDrawTargetOutFromRegion in TiledContentClient for mobile
  if (MayResample()) {
    // If we're resampling then bilinear filtering can read up to 1 pixel
    // outside of our texture coords. Make the visible region a single rect,
    // and pad it out by 1 pixel (restricted to tile boundaries) so that
    // we always have valid content or transparent pixels to sample from.
    IntRect bounds = neededRegion.GetBounds();
    IntRect wholeTiles = bounds;
    wholeTiles.InflateToMultiple(gfx::gfxVars::TileSize());
    IntRect padded = bounds;
    padded.Inflate(1);
    padded.IntersectRect(padded, wholeTiles);
    neededRegion = padded;
  }
#endif

  nsIntRegion invalidRegion;
  invalidRegion.Sub(neededRegion, GetValidRegion());
  if (invalidRegion.IsEmpty()) {
    EndPaint();
    return;
  }

  if (!callback) {
    ClientManager()->SetTransactionIncomplete();
    return;
  }

  if (!ClientManager()->IsRepeatTransaction()) {
    // Only paint the mask layers on the first transaction.
    RenderMaskLayers(this);

    // For more complex cases we need to calculate a bunch of metrics before we
    // can do the paint.
    BeginPaint();
    if (mPaintData.mPaintFinished) {
      return;
    }

    // Make sure that tiles that fall outside of the visible region or outside of the
    // critical displayport are discarded on the first update. Also make sure that we
    // only draw stuff inside the critical displayport on the first update.
    nsIntRegion validRegion;
    validRegion.And(GetValidRegion(), neededRegion);
    if (mPaintData.mCriticalDisplayPort) {
      validRegion.AndWith(mPaintData.mCriticalDisplayPort->ToUnknownRect());
      invalidRegion.And(invalidRegion, mPaintData.mCriticalDisplayPort->ToUnknownRect());
    }
    SetValidRegion(validRegion);

    TILING_LOG("TILING %p: First-transaction valid region %s\n", this, Stringify(validRegion).c_str());
    TILING_LOG("TILING %p: First-transaction invalid region %s\n", this, Stringify(invalidRegion).c_str());
  } else {
    if (mPaintData.mCriticalDisplayPort) {
      invalidRegion.And(invalidRegion, mPaintData.mCriticalDisplayPort->ToUnknownRect());
    }
    TILING_LOG("TILING %p: Repeat-transaction invalid region %s\n", this, Stringify(invalidRegion).c_str());
  }

  nsIntRegion lowPrecisionInvalidRegion;
  if (mContentClient->GetLowPrecisionTiledBuffer()) {
    // Calculate the invalid region for the low precision buffer. Make sure
    // to remove the valid high-precision area so we don't double-paint it.
    lowPrecisionInvalidRegion.Sub(neededRegion, mLowPrecisionValidRegion);
    lowPrecisionInvalidRegion.Sub(lowPrecisionInvalidRegion, GetValidRegion());
  }
  TILING_LOG("TILING %p: Low-precision invalid region %s\n", this, Stringify(lowPrecisionInvalidRegion).c_str());

  bool updatedHighPrecision = RenderHighPrecision(invalidRegion,
                                                  neededRegion,
                                                  callback, data);
  if (updatedHighPrecision) {
    ClientManager()->Hold(this);
    mContentClient->UpdatedBuffer(TiledContentClient::TILED_BUFFER);

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
    mContentClient->UpdatedBuffer(TiledContentClient::LOW_PRECISION_TILED_BUFFER);

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

bool
ClientTiledPaintedLayer::IsOptimizedFor(LayerManager::PaintedLayerCreationHint aHint)
{
  // The only creation hint is whether the layer is scrollable or not, and this
  // is only respected on OSX, where it's used to determine whether to
  // use a tiled content client or not.
  // There are pretty nasty performance consequences for not using tiles on
  // large, scrollable layers, so we want the layer to be recreated in this
  // situation.
  return aHint == GetCreationHint();
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

} // namespace layers
} // namespace mozilla
