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
#include "mozilla/layers/LayerMetricsWrapper.h" // for LayerMetricsWrapper
#include "mozilla/layers/LayersMessages.h"
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsRect.h"                     // for nsIntRect
#include "LayersLogging.h"

namespace mozilla {
namespace layers {


ClientTiledPaintedLayer::ClientTiledPaintedLayer(ClientLayerManager* const aManager,
                                               ClientLayerManager::PaintedLayerCreationHint aCreationHint)
  : PaintedLayer(aManager,
                static_cast<ClientLayer*>(MOZ_THIS_IN_INITIALIZER_LIST()),
                aCreationHint)
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
    // If the layer has a non-transient async transform then we need to apply it here
    // because it will get applied by the APZ in the compositor as well
    const FrameMetrics& metrics = iter.Metrics();
    transform = transform * gfx::Matrix4x4().Scale(metrics.mResolution.scale, metrics.mResolution.scale, 1.f);
  }
  return transform;
}

void
ClientTiledPaintedLayer::GetAncestorLayers(LayerMetricsWrapper* aOutScrollAncestor,
                                          LayerMetricsWrapper* aOutDisplayPortAncestor)
{
  LayerMetricsWrapper scrollAncestor;
  LayerMetricsWrapper displayPortAncestor;
  for (LayerMetricsWrapper ancestor(this, LayerMetricsWrapper::StartAt::BOTTOM); ancestor; ancestor = ancestor.GetParent()) {
    const FrameMetrics& metrics = ancestor.Metrics();
    if (!scrollAncestor && metrics.GetScrollId() != FrameMetrics::NULL_SCROLL_ID) {
      scrollAncestor = ancestor;
    }
    if (!metrics.mDisplayPort.IsEmpty()) {
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
  GetAncestorLayers(&scrollAncestor, &displayPortAncestor);

  if (!displayPortAncestor || !scrollAncestor) {
    // No displayport or scroll ancestor, so we can't do progressive rendering.
#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_B2G)
    // Both Android and b2g are guaranteed to have a displayport set, so this
    // should never happen.
    NS_WARNING("Tiled Thebes layer with no scrollable container ancestor");
#endif
    return;
  }

  TILING_LOG("TILING %p: Found scrollAncestor %p and displayPortAncestor %p\n", this,
    scrollAncestor.GetLayer(), displayPortAncestor.GetLayer());

  const FrameMetrics& scrollMetrics = scrollAncestor.Metrics();
  const FrameMetrics& displayportMetrics = displayPortAncestor.Metrics();

  // Calculate the transform required to convert ParentLayer space of our
  // display port ancestor to the Layer space of this layer.
  gfx::Matrix4x4 transformDisplayPortToLayer =
    GetTransformToAncestorsParentLayer(this, displayPortAncestor);
  transformDisplayPortToLayer.Invert();

  // Note that below we use GetZoomToParent() in a number of places. Because this
  // code runs on the client side, the mTransformScale field of the FrameMetrics
  // will not have been set. This can result in incorrect values being returned
  // by GetZoomToParent() when we have CSS transforms set on some of these layers.
  // This code should be audited and updated as part of fixing bug 993525.

  // Compute the critical display port that applies to this layer in the
  // LayoutDevice space of this layer.
  ParentLayerRect criticalDisplayPort =
    (displayportMetrics.mCriticalDisplayPort * displayportMetrics.GetZoomToParent())
    + displayportMetrics.mCompositionBounds.TopLeft();
  mPaintData.mCriticalDisplayPort = RoundedOut(
    ApplyParentLayerToLayerTransform(transformDisplayPortToLayer, criticalDisplayPort));
  TILING_LOG("TILING %p: Critical displayport %s\n", this, Stringify(mPaintData.mCriticalDisplayPort).c_str());

  // Store the resolution from the displayport ancestor layer. Because this is Gecko-side,
  // before any async transforms have occurred, we can use the zoom for this.
  mPaintData.mResolution = displayportMetrics.GetZoomToParent();
  TILING_LOG("TILING %p: Resolution %f\n", this, mPaintData.mResolution.scale);

  // Store the applicable composition bounds in this layer's Layer units.
  mPaintData.mTransformToCompBounds =
    GetTransformToAncestorsParentLayer(this, scrollAncestor);
  gfx::Matrix4x4 transformToBounds = mPaintData.mTransformToCompBounds;
  transformToBounds.Invert();
  mPaintData.mCompositionBounds = ApplyParentLayerToLayerTransform(
    transformToBounds, scrollMetrics.mCompositionBounds);
  TILING_LOG("TILING %p: Composition bounds %s\n", this, Stringify(mPaintData.mCompositionBounds).c_str());

  // Calculate the scroll offset since the last transaction
  mPaintData.mScrollOffset = displayportMetrics.GetScrollOffset() * displayportMetrics.GetZoomToParent();
  TILING_LOG("TILING %p: Scroll offset %s\n", this, Stringify(mPaintData.mScrollOffset).c_str());
}

bool
ClientTiledPaintedLayer::UseFastPath()
{
  LayerMetricsWrapper scrollAncestor;
  GetAncestorLayers(&scrollAncestor, nullptr);
  if (!scrollAncestor) {
    return true;
  }
  const FrameMetrics& parentMetrics = scrollAncestor.Metrics();

  bool multipleTransactionsNeeded = gfxPrefs::UseProgressiveTilePainting()
                                 || gfxPrefs::UseLowPrecisionBuffer()
                                 || !parentMetrics.mCriticalDisplayPort.IsEmpty();
  bool isFixed = GetIsFixedPosition() || GetParent()->GetIsFixedPosition();
  return !multipleTransactionsNeeded || isFixed || parentMetrics.mDisplayPort.IsEmpty();
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

  // Only draw progressively when the resolution is unchanged, and we're not
  // in a reftest scenario (that's what the HasShadowManager() check is for).
  if (gfxPrefs::UseProgressiveTilePainting() &&
      !ClientManager()->HasShadowTarget() &&
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
#ifndef MOZ_GFX_OPTIMIZE_MOBILE
  // This is handled by PadDrawTargetOutFromRegion in TiledContentClient for mobile
  if (MayResample()) {
    // If we're resampling then bilinear filtering can read up to 1 pixel
    // outside of our texture coords. Make the visible region a single rect,
    // and pad it out by 1 pixel (restricted to tile boundaries) so that
    // we always have valid content or transparent pixels to sample from.
    nsIntRect bounds = neededRegion.GetBounds();
    nsIntRect wholeTiles = bounds;
    wholeTiles.Inflate(nsIntSize(gfxPrefs::LayersTileWidth(), gfxPrefs::LayersTileHeight()));
    nsIntRect padded = bounds;
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

    // In some cases we can take a fast path and just be done with it.
    if (UseFastPath()) {
      TILING_LOG("TILING %p: Taking fast-path\n", this);
      mValidRegion = neededRegion;
      mContentClient->mTiledBuffer.PaintThebes(mValidRegion, invalidRegion, callback, data);
      ClientManager()->Hold(this);
      mContentClient->UseTiledLayerBuffer(TiledContentClient::TILED_BUFFER);
      return;
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

} // mozilla
} // layers
