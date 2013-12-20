/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientTiledThebesLayer.h"
#include "FrameMetrics.h"               // for FrameMetrics
#include "Units.h"                      // for ScreenIntRect, CSSPoint, etc
#include "ClientLayerManager.h"         // for ClientLayerManager, etc
#include "gfx3DMatrix.h"                // for gfx3DMatrix
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxRect.h"                    // for gfxRect
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/gfx/Rect.h"           // for Rect, RectTyped
#include "mozilla/layers/LayersMessages.h"
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsRect.h"                     // for nsIntRect
#include "nsTraceRefcnt.h"              // for MOZ_COUNT_CTOR, etc

namespace mozilla {
namespace layers {


ClientTiledThebesLayer::ClientTiledThebesLayer(ClientLayerManager* const aManager)
  : ThebesLayer(aManager,
                static_cast<ClientLayer*>(MOZ_THIS_IN_INITIALIZER_LIST()))
  , mContentClient()
{
  MOZ_COUNT_CTOR(ClientTiledThebesLayer);
  mPaintData.mLastScrollOffset = ScreenPoint(0, 0);
  mPaintData.mFirstPaint = true;
}

ClientTiledThebesLayer::~ClientTiledThebesLayer()
{
  MOZ_COUNT_DTOR(ClientTiledThebesLayer);
}

void
ClientTiledThebesLayer::FillSpecificAttributes(SpecificLayerAttributes& aAttrs)
{
  aAttrs = ThebesLayerAttributes(GetValidRegion());
}

static LayoutDeviceRect
ApplyScreenToLayoutTransform(const gfx3DMatrix& aTransform, const ScreenRect& aScreenRect)
{
  gfxRect input(aScreenRect.x, aScreenRect.y, aScreenRect.width, aScreenRect.height);
  gfxRect output = aTransform.TransformBounds(input);
  return LayoutDeviceRect(output.x, output.y, output.width, output.height);
}

void
ClientTiledThebesLayer::BeginPaint()
{
  if (ClientManager()->IsRepeatTransaction()) {
    return;
  }

  mPaintData.mLowPrecisionPaintCount = 0;
  mPaintData.mPaintFinished = false;

  // Get the metrics of the nearest scroll container.
  ContainerLayer* scrollParent = nullptr;
  for (ContainerLayer* parent = GetParent(); parent; parent = parent->GetParent()) {
    const FrameMetrics& metrics = parent->GetFrameMetrics();
    if (metrics.mScrollId != FrameMetrics::NULL_SCROLL_ID) {
      scrollParent = parent;
      break;
    }
  }

  if (!scrollParent) {
    // XXX I don't think this can happen, but if it does, warn and set the
    //     composition bounds to empty so that progressive updates are disabled.
    NS_WARNING("Tiled Thebes layer with no scrollable container parent");
    mPaintData.mCompositionBounds.SetEmpty();
    return;
  }

  const FrameMetrics& metrics = scrollParent->GetFrameMetrics();

  // Calculate the transform required to convert screen space into transformed
  // layout device space.
  gfx3DMatrix layoutToScreen = GetEffectiveTransform();
  for (ContainerLayer* parent = GetParent(); parent; parent = parent->GetParent()) {
    if (parent->UseIntermediateSurface()) {
      layoutToScreen *= parent->GetEffectiveTransform();
    }
  }
  layoutToScreen.ScalePost(metrics.mCumulativeResolution.scale,
                           metrics.mCumulativeResolution.scale,
                           1.f);

  mPaintData.mTransformScreenToLayout = layoutToScreen.Inverse();

  // Compute the critical display port in layer space.
  mPaintData.mLayoutCriticalDisplayPort.SetEmpty();
  if (!metrics.mCriticalDisplayPort.IsEmpty()) {
    // Convert the display port to screen space first so that we can transform
    // it into layout device space.
    const ScreenRect& criticalDisplayPort = metrics.mCriticalDisplayPort * metrics.mZoom;
    LayoutDeviceRect transformedCriticalDisplayPort =
      ApplyScreenToLayoutTransform(mPaintData.mTransformScreenToLayout, criticalDisplayPort);
    mPaintData.mLayoutCriticalDisplayPort =
      LayoutDeviceIntRect::ToUntyped(RoundedOut(transformedCriticalDisplayPort));
  }

  // Calculate the frame resolution. Because this is Gecko-side, before any
  // async transforms have occurred, we can use mZoom for this.
  mPaintData.mResolution = metrics.mZoom;

  // Calculate the scroll offset since the last transaction, and the
  // composition bounds.
  mPaintData.mCompositionBounds.SetEmpty();
  mPaintData.mScrollOffset.MoveTo(0, 0);
  Layer* primaryScrollable = ClientManager()->GetPrimaryScrollableLayer();
  if (primaryScrollable) {
    const FrameMetrics& metrics = primaryScrollable->AsContainerLayer()->GetFrameMetrics();
    mPaintData.mScrollOffset = metrics.mScrollOffset * metrics.mZoom;
    mPaintData.mCompositionBounds =
      ApplyScreenToLayoutTransform(mPaintData.mTransformScreenToLayout,
                                   ScreenRect(metrics.mCompositionBounds));
  }
}

void
ClientTiledThebesLayer::EndPaint(bool aFinish)
{
  if (!aFinish && !mPaintData.mPaintFinished) {
    return;
  }

  mPaintData.mLastScrollOffset = mPaintData.mScrollOffset;
  mPaintData.mPaintFinished = true;
  mPaintData.mFirstPaint = false;
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

  nsIntRegion invalidRegion = mVisibleRegion;
  invalidRegion.Sub(invalidRegion, mValidRegion);
  if (invalidRegion.IsEmpty()) {
    EndPaint(true);
    return;
  }

  // Only paint the mask layer on the first transaction.
  if (GetMaskLayer() && !ClientManager()->IsRepeatTransaction()) {
    ToClientLayer(GetMaskLayer())->RenderLayer();
  }

  // Fast path for no progressive updates, no low-precision updates and no
  // critical display-port set, or no display-port set.
  const FrameMetrics& parentMetrics = GetParent()->GetFrameMetrics();
  if ((!gfxPlatform::UseProgressiveTilePainting() &&
       !gfxPlatform::UseLowPrecisionBuffer() &&
       parentMetrics.mCriticalDisplayPort.IsEmpty()) ||
       parentMetrics.mDisplayPort.IsEmpty()) {
    mValidRegion = mVisibleRegion;

    NS_ASSERTION(!ClientManager()->IsRepeatTransaction(), "Didn't paint our mask layer");

    mContentClient->mTiledBuffer.PaintThebes(mValidRegion, invalidRegion,
                                             callback, data);

    ClientManager()->Hold(this);
    mContentClient->LockCopyAndWrite(TiledContentClient::TILED_BUFFER);

    return;
  }

  // Calculate everything we need to perform the paint.
  BeginPaint();
  if (mPaintData.mPaintFinished) {
    return;
  }

  // Make sure that tiles that fall outside of the visible region are
  // discarded on the first update.
  if (!ClientManager()->IsRepeatTransaction()) {
    mValidRegion.And(mValidRegion, mVisibleRegion);
    if (!mPaintData.mLayoutCriticalDisplayPort.IsEmpty()) {
      // Make sure that tiles that fall outside of the critical displayport are
      // discarded on the first update.
      mValidRegion.And(mValidRegion, mPaintData.mLayoutCriticalDisplayPort);
    }
  }

  nsIntRegion lowPrecisionInvalidRegion;
  if (!mPaintData.mLayoutCriticalDisplayPort.IsEmpty()) {
    if (gfxPlatform::UseLowPrecisionBuffer()) {
      // Calculate the invalid region for the low precision buffer
      lowPrecisionInvalidRegion.Sub(mVisibleRegion, mLowPrecisionValidRegion);

      // Remove the valid region from the low precision valid region (we don't
      // validate this part of the low precision buffer).
      lowPrecisionInvalidRegion.Sub(lowPrecisionInvalidRegion, mValidRegion);
    }

    // Clip the invalid region to the critical display-port
    invalidRegion.And(invalidRegion, mPaintData.mLayoutCriticalDisplayPort);
    if (invalidRegion.IsEmpty() && lowPrecisionInvalidRegion.IsEmpty()) {
      EndPaint(true);
      return;
    }
  }

  if (!invalidRegion.IsEmpty() && mPaintData.mLowPrecisionPaintCount == 0) {
    bool updatedBuffer = false;
    // Only draw progressively when the resolution is unchanged.
    if (gfxPlatform::UseProgressiveTilePainting() &&
        !ClientManager()->HasShadowTarget() &&
        mContentClient->mTiledBuffer.GetFrameResolution() == mPaintData.mResolution) {
      // Store the old valid region, then clear it before painting.
      // We clip the old valid region to the visible region, as it only gets
      // used to decide stale content (currently valid and previously visible)
      nsIntRegion oldValidRegion = mContentClient->mTiledBuffer.GetValidRegion();
      oldValidRegion.And(oldValidRegion, mVisibleRegion);
      if (!mPaintData.mLayoutCriticalDisplayPort.IsEmpty()) {
        oldValidRegion.And(oldValidRegion, mPaintData.mLayoutCriticalDisplayPort);
      }

      updatedBuffer =
        mContentClient->mTiledBuffer.ProgressiveUpdate(mValidRegion, invalidRegion,
                                                       oldValidRegion, &mPaintData,
                                                       callback, data);
    } else {
      updatedBuffer = true;
      mValidRegion = mVisibleRegion;
      if (!mPaintData.mLayoutCriticalDisplayPort.IsEmpty()) {
        mValidRegion.And(mValidRegion, mPaintData.mLayoutCriticalDisplayPort);
      }
      mContentClient->mTiledBuffer.SetFrameResolution(mPaintData.mResolution);
      mContentClient->mTiledBuffer.PaintThebes(mValidRegion, invalidRegion,
                                               callback, data);
    }

    if (updatedBuffer) {
      ClientManager()->Hold(this);
      mContentClient->LockCopyAndWrite(TiledContentClient::TILED_BUFFER);

      // If there are low precision updates, mark the paint as unfinished and
      // request a repeat transaction.
      if (!lowPrecisionInvalidRegion.IsEmpty() && mPaintData.mPaintFinished) {
        ClientManager()->SetRepeatTransaction();
        mPaintData.mLowPrecisionPaintCount = 1;
        mPaintData.mPaintFinished = false;
      }

      // Return so that low precision updates aren't performed in the same
      // transaction as high-precision updates.
      EndPaint(false);
      return;
    }
  }

  // Render the low precision buffer, if there's area to invalidate and the
  // visible region is larger than the critical display port.
  bool updatedLowPrecision = false;
  if (!lowPrecisionInvalidRegion.IsEmpty() &&
      !nsIntRegion(mPaintData.mLayoutCriticalDisplayPort).Contains(mVisibleRegion)) {
    nsIntRegion oldValidRegion =
      mContentClient->mLowPrecisionTiledBuffer.GetValidRegion();
    oldValidRegion.And(oldValidRegion, mVisibleRegion);

    // If the frame resolution or format have changed, invalidate the buffer
    if (mContentClient->mLowPrecisionTiledBuffer.GetFrameResolution() != mPaintData.mResolution ||
        mContentClient->mLowPrecisionTiledBuffer.HasFormatChanged()) {
      if (!mLowPrecisionValidRegion.IsEmpty()) {
        updatedLowPrecision = true;
      }
      oldValidRegion.SetEmpty();
      mLowPrecisionValidRegion.SetEmpty();
      mContentClient->mLowPrecisionTiledBuffer.SetFrameResolution(mPaintData.mResolution);
      lowPrecisionInvalidRegion = mVisibleRegion;
    }

    // Invalidate previously valid content that is no longer visible
    if (mPaintData.mLowPrecisionPaintCount == 1) {
      mLowPrecisionValidRegion.And(mLowPrecisionValidRegion, mVisibleRegion);
    }
    mPaintData.mLowPrecisionPaintCount++;

    // Remove the valid high-precision region from the invalid low-precision
    // region. We don't want to spend time drawing things twice.
    lowPrecisionInvalidRegion.Sub(lowPrecisionInvalidRegion, mValidRegion);

    if (!lowPrecisionInvalidRegion.IsEmpty()) {
      updatedLowPrecision = mContentClient->mLowPrecisionTiledBuffer
                              .ProgressiveUpdate(mLowPrecisionValidRegion,
                                                 lowPrecisionInvalidRegion,
                                                 oldValidRegion, &mPaintData,
                                                 callback, data);
    }
  } else if (!mLowPrecisionValidRegion.IsEmpty()) {
    // Clear the low precision tiled buffer
    updatedLowPrecision = true;
    mLowPrecisionValidRegion.SetEmpty();
    mContentClient->mLowPrecisionTiledBuffer.PaintThebes(mLowPrecisionValidRegion,
                                                         mLowPrecisionValidRegion,
                                                         callback, data);
  }

  // We send a Painted callback if we clear the valid region of the low
  // precision buffer, so that the shadow buffer's valid region can be updated
  // and the associated resources can be freed.
  if (updatedLowPrecision) {
    ClientManager()->Hold(this);
    mContentClient->LockCopyAndWrite(TiledContentClient::LOW_PRECISION_TILED_BUFFER);
  }

  EndPaint(false);
}

} // mozilla
} // layers
