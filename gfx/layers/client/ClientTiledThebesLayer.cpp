/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Uncomment this to enable the TILING_PRLOG stuff in this file
// for release builds. To get the output you need to have
// NSPR_LOG_MODULES=tiling:5 in your environment at runtime.
// #define FORCE_PR_LOG

#include "ClientTiledThebesLayer.h"
#include "FrameMetrics.h"               // for FrameMetrics
#include "Units.h"                      // for ScreenIntRect, CSSPoint, etc
#include "UnitTransforms.h"             // for TransformTo
#include "ClientLayerManager.h"         // for ClientLayerManager, etc
#include "gfx3DMatrix.h"                // for gfx3DMatrix
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxPrefs.h"                   // for gfxPrefs
#include "gfxRect.h"                    // for gfxRect
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/gfx/Rect.h"           // for Rect, RectTyped
#include "mozilla/layers/LayersMessages.h"
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsRect.h"                     // for nsIntRect
#include "LayersLogging.h"

namespace mozilla {
namespace layers {


ClientTiledThebesLayer::ClientTiledThebesLayer(ClientLayerManager* const aManager,
                                               ClientLayerManager::ThebesLayerCreationHint aCreationHint)
  : ThebesLayer(aManager,
                static_cast<ClientLayer*>(MOZ_THIS_IN_INITIALIZER_LIST()),
                aCreationHint)
  , mContentClient()
{
  MOZ_COUNT_CTOR(ClientTiledThebesLayer);
  mPaintData.mLastScrollOffset = ParentLayerPoint(0, 0);
  mPaintData.mFirstPaint = true;
}

ClientTiledThebesLayer::~ClientTiledThebesLayer()
{
  MOZ_COUNT_DTOR(ClientTiledThebesLayer);
}

void
ClientTiledThebesLayer::ClearCachedResources()
{
  if (mContentClient) {
    mContentClient->ClearCachedResources();
  }
  mValidRegion.SetEmpty();
  mContentClient = nullptr;
}

void
ClientTiledThebesLayer::FillSpecificAttributes(SpecificLayerAttributes& aAttrs)
{
  aAttrs = ThebesLayerAttributes(GetValidRegion());
}

static LayerRect
ApplyParentLayerToLayerTransform(const gfx3DMatrix& aTransform, const ParentLayerRect& aParentLayerRect)
{
  return TransformTo<LayerPixel>(aTransform, aParentLayerRect);
}

static gfx3DMatrix
GetTransformToAncestorsParentLayer(Layer* aStart, Layer* aAncestor)
{
  gfx::Matrix4x4 transform;
  Layer* ancestorParent = aAncestor->GetParent();
  for (Layer* iter = aStart; iter != ancestorParent; iter = iter->GetParent()) {
    if (iter->AsContainerLayer()) {
      // If the layer has a non-transient async transform then we need to apply it here
      // because it will get applied by the APZ in the compositor as well
      const FrameMetrics& metrics = iter->AsContainerLayer()->GetFrameMetrics();
      transform = transform * gfx::Matrix4x4().Scale(metrics.mResolution.scale, metrics.mResolution.scale, 1.f);
    }
    transform = transform * iter->GetTransform();
  }
  gfx3DMatrix ret;
  gfx::To3DMatrix(transform, ret);
  return ret;
}

void
ClientTiledThebesLayer::GetAncestorLayers(ContainerLayer** aOutScrollAncestor,
                                          ContainerLayer** aOutDisplayPortAncestor)
{
  ContainerLayer* scrollAncestor = nullptr;
  ContainerLayer* displayPortAncestor = nullptr;
  for (ContainerLayer* ancestor = GetParent(); ancestor; ancestor = ancestor->GetParent()) {
    const FrameMetrics& metrics = ancestor->GetFrameMetrics();
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
ClientTiledThebesLayer::BeginPaint()
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
  ContainerLayer* scrollAncestor = nullptr;
  ContainerLayer* displayPortAncestor = nullptr;
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

  TILING_PRLOG(("TILING %p: Found scrollAncestor %p and displayPortAncestor %p\n", this,
    scrollAncestor, displayPortAncestor));

  const FrameMetrics& scrollMetrics = scrollAncestor->GetFrameMetrics();
  const FrameMetrics& displayportMetrics = displayPortAncestor->GetFrameMetrics();

  // Calculate the transform required to convert ParentLayer space of our
  // display port ancestor to the Layer space of this layer.
  gfx3DMatrix transformDisplayPortToLayer =
    GetTransformToAncestorsParentLayer(this, displayPortAncestor).Inverse();

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
  TILING_PRLOG_OBJ(("TILING %p: Critical displayport %s\n", this, tmpstr.get()), mPaintData.mCriticalDisplayPort);

  // Store the resolution from the displayport ancestor layer. Because this is Gecko-side,
  // before any async transforms have occurred, we can use the zoom for this.
  mPaintData.mResolution = displayportMetrics.GetZoomToParent();
  TILING_PRLOG(("TILING %p: Resolution %f\n", this, mPaintData.mResolution.scale));

  // Store the applicable composition bounds in this layer's Layer units.
  mPaintData.mTransformToCompBounds =
    GetTransformToAncestorsParentLayer(this, scrollAncestor);
  mPaintData.mCompositionBounds = ApplyParentLayerToLayerTransform(
    mPaintData.mTransformToCompBounds.Inverse(), scrollMetrics.mCompositionBounds);
  TILING_PRLOG_OBJ(("TILING %p: Composition bounds %s\n", this, tmpstr.get()), mPaintData.mCompositionBounds);

  // Calculate the scroll offset since the last transaction
  mPaintData.mScrollOffset = displayportMetrics.GetScrollOffset() * displayportMetrics.GetZoomToParent();
  TILING_PRLOG_OBJ(("TILING %p: Scroll offset %s\n", this, tmpstr.get()), mPaintData.mScrollOffset);
}

bool
ClientTiledThebesLayer::UseFastPath()
{
  const FrameMetrics& parentMetrics = GetParent()->GetFrameMetrics();
  bool multipleTransactionsNeeded = gfxPrefs::UseProgressiveTilePainting()
                                 || gfxPrefs::UseLowPrecisionBuffer()
                                 || !parentMetrics.mCriticalDisplayPort.IsEmpty();
  bool isFixed = GetIsFixedPosition() || GetParent()->GetIsFixedPosition();
  return !multipleTransactionsNeeded || isFixed || parentMetrics.mDisplayPort.IsEmpty();
}

bool
ClientTiledThebesLayer::RenderHighPrecision(nsIntRegion& aInvalidRegion,
                                            LayerManager::DrawThebesLayerCallback aCallback,
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
    oldValidRegion.And(oldValidRegion, mVisibleRegion);
    if (!mPaintData.mCriticalDisplayPort.IsEmpty()) {
      oldValidRegion.And(oldValidRegion, LayerIntRect::ToUntyped(mPaintData.mCriticalDisplayPort));
    }

    TILING_PRLOG_OBJ(("TILING %p: Progressive update with old valid region %s\n", this, tmpstr.get()), oldValidRegion);

    return mContentClient->mTiledBuffer.ProgressiveUpdate(mValidRegion, aInvalidRegion,
                      oldValidRegion, &mPaintData, aCallback, aCallbackData);
  }

  // Otherwise do a non-progressive paint

  mValidRegion = mVisibleRegion;
  if (!mPaintData.mCriticalDisplayPort.IsEmpty()) {
    mValidRegion.And(mValidRegion, LayerIntRect::ToUntyped(mPaintData.mCriticalDisplayPort));
  }

  TILING_PRLOG_OBJ(("TILING %p: Non-progressive paint invalid region %s\n", this, tmpstr.get()), aInvalidRegion);
  TILING_PRLOG_OBJ(("TILING %p: Non-progressive paint new valid region %s\n", this, tmpstr.get()), mValidRegion);

  mContentClient->mTiledBuffer.SetFrameResolution(mPaintData.mResolution);
  mContentClient->mTiledBuffer.PaintThebes(mValidRegion, aInvalidRegion, aCallback, aCallbackData);
  return true;
}

bool
ClientTiledThebesLayer::RenderLowPrecision(nsIntRegion& aInvalidRegion,
                                           LayerManager::DrawThebesLayerCallback aCallback,
                                           void* aCallbackData)
{
  // Render the low precision buffer, if the visible region is larger than the
  // critical display port.
  if (!nsIntRegion(LayerIntRect::ToUntyped(mPaintData.mCriticalDisplayPort)).Contains(mVisibleRegion)) {
    nsIntRegion oldValidRegion = mContentClient->mLowPrecisionTiledBuffer.GetValidRegion();
    oldValidRegion.And(oldValidRegion, mVisibleRegion);

    bool updatedBuffer = false;

    // If the frame resolution or format have changed, invalidate the buffer
    if (mContentClient->mLowPrecisionTiledBuffer.GetFrameResolution() != mPaintData.mResolution ||
        mContentClient->mLowPrecisionTiledBuffer.HasFormatChanged()) {
      if (!mLowPrecisionValidRegion.IsEmpty()) {
        updatedBuffer = true;
      }
      oldValidRegion.SetEmpty();
      mLowPrecisionValidRegion.SetEmpty();
      mContentClient->mLowPrecisionTiledBuffer.SetFrameResolution(mPaintData.mResolution);
      aInvalidRegion = mVisibleRegion;
    }

    // Invalidate previously valid content that is no longer visible
    if (mPaintData.mLowPrecisionPaintCount == 1) {
      mLowPrecisionValidRegion.And(mLowPrecisionValidRegion, mVisibleRegion);
    }
    mPaintData.mLowPrecisionPaintCount++;

    // Remove the valid high-precision region from the invalid low-precision
    // region. We don't want to spend time drawing things twice.
    aInvalidRegion.Sub(aInvalidRegion, mValidRegion);

    TILING_PRLOG_OBJ(("TILING %p: Progressive paint: low-precision invalid region is %s\n", this, tmpstr.get()), aInvalidRegion);
    TILING_PRLOG_OBJ(("TILING %p: Progressive paint: low-precision old valid region is %s\n", this, tmpstr.get()), oldValidRegion);

    if (!aInvalidRegion.IsEmpty()) {
      updatedBuffer = mContentClient->mLowPrecisionTiledBuffer.ProgressiveUpdate(
                            mLowPrecisionValidRegion, aInvalidRegion, oldValidRegion,
                            &mPaintData, aCallback, aCallbackData);
    }

    TILING_PRLOG_OBJ(("TILING %p: Progressive paint: low-precision new valid region is %s\n", this, tmpstr.get()), mLowPrecisionValidRegion);
    return updatedBuffer;
  }
  if (!mLowPrecisionValidRegion.IsEmpty()) {
    TILING_PRLOG(("TILING %p: Clearing low-precision buffer\n", this));
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
ClientTiledThebesLayer::EndPaint()
{
  mPaintData.mLastScrollOffset = mPaintData.mScrollOffset;
  mPaintData.mPaintFinished = true;
  mPaintData.mFirstPaint = false;
  TILING_PRLOG(("TILING %p: Paint finished\n", this));
}

void
ClientTiledThebesLayer::RenderLayer()
{
  LayerManager::DrawThebesLayerCallback callback =
    ClientManager()->GetThebesLayerCallback();
  void *data = ClientManager()->GetThebesLayerCallbackData();
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
  }

  TILING_PRLOG_OBJ(("TILING %p: Initial visible region %s\n", this, tmpstr.get()), mVisibleRegion);
  TILING_PRLOG_OBJ(("TILING %p: Initial valid region %s\n", this, tmpstr.get()), mValidRegion);
  TILING_PRLOG_OBJ(("TILING %p: Initial low-precision valid region %s\n", this, tmpstr.get()), mLowPrecisionValidRegion);

  nsIntRegion invalidRegion;
  invalidRegion.Sub(mVisibleRegion, mValidRegion);
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
      TILING_PRLOG(("TILING %p: Taking fast-path\n", this));
      mValidRegion = mVisibleRegion;
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
    mValidRegion.And(mValidRegion, mVisibleRegion);
    if (!mPaintData.mCriticalDisplayPort.IsEmpty()) {
      mValidRegion.And(mValidRegion, LayerIntRect::ToUntyped(mPaintData.mCriticalDisplayPort));
      invalidRegion.And(invalidRegion, LayerIntRect::ToUntyped(mPaintData.mCriticalDisplayPort));
    }

    TILING_PRLOG_OBJ(("TILING %p: First-transaction valid region %s\n", this, tmpstr.get()), mValidRegion);
    TILING_PRLOG_OBJ(("TILING %p: First-transaction invalid region %s\n", this, tmpstr.get()), invalidRegion);
  } else {
    if (!mPaintData.mCriticalDisplayPort.IsEmpty()) {
      invalidRegion.And(invalidRegion, LayerIntRect::ToUntyped(mPaintData.mCriticalDisplayPort));
    }
    TILING_PRLOG_OBJ(("TILING %p: Repeat-transaction invalid region %s\n", this, tmpstr.get()), invalidRegion);
  }

  nsIntRegion lowPrecisionInvalidRegion;
  if (gfxPrefs::UseLowPrecisionBuffer()) {
    // Calculate the invalid region for the low precision buffer. Make sure
    // to remove the valid high-precision area so we don't double-paint it.
    lowPrecisionInvalidRegion.Sub(mVisibleRegion, mLowPrecisionValidRegion);
    lowPrecisionInvalidRegion.Sub(lowPrecisionInvalidRegion, mValidRegion);
  }
  TILING_PRLOG_OBJ(("TILING %p: Low-precision invalid region %s\n", this, tmpstr.get()), lowPrecisionInvalidRegion);

  bool updatedHighPrecision = RenderHighPrecision(invalidRegion, callback, data);
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
    TILING_PRLOG(("TILING %p: Scheduling repeat transaction for low-precision painting\n", this));
    ClientManager()->SetRepeatTransaction();
    mPaintData.mLowPrecisionPaintCount = 1;
    mPaintData.mPaintFinished = false;
    return;
  }

  bool updatedLowPrecision = RenderLowPrecision(lowPrecisionInvalidRegion, callback, data);
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
