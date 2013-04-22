/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/PLayersChild.h"
#include "BasicTiledThebesLayer.h"
#include "gfxImageSurface.h"
#include "GeckoProfiler.h"
#include "gfxPlatform.h"


namespace mozilla {
namespace layers {


BasicTiledThebesLayer::BasicTiledThebesLayer(BasicShadowLayerManager* const aManager)
  : ThebesLayer(aManager, static_cast<BasicImplData*>(this))
  , mContentClient()
{
  MOZ_COUNT_CTOR(BasicTiledThebesLayer);
  mPaintData.mLastScrollOffset = gfx::Point(0, 0);
  mPaintData.mFirstPaint = true;
}

BasicTiledThebesLayer::~BasicTiledThebesLayer()
{
  MOZ_COUNT_DTOR(BasicTiledThebesLayer);
}

void
BasicTiledThebesLayer::FillSpecificAttributes(SpecificLayerAttributes& aAttrs)
{
  aAttrs = ThebesLayerAttributes(GetValidRegion());
}

void
BasicTiledThebesLayer::BeginPaint()
{
  if (BasicManager()->IsRepeatTransaction()) {
    return;
  }

  mPaintData.mLowPrecisionPaintCount = 0;
  mPaintData.mPaintFinished = false;

  // Calculate the transform required to convert screen space into layer space
  mPaintData.mTransformScreenToLayer = GetEffectiveTransform();
  // XXX Not sure if this code for intermediate surfaces is correct.
  //     It rarely gets hit though, and shouldn't have terrible consequences
  //     even if it is wrong.
  for (ContainerLayer* parent = GetParent(); parent; parent = parent->GetParent()) {
    if (parent->UseIntermediateSurface()) {
      mPaintData.mTransformScreenToLayer.PreMultiply(parent->GetEffectiveTransform());
    }
  }
  mPaintData.mTransformScreenToLayer.Invert();

  // Compute the critical display port in layer space.
  mPaintData.mLayerCriticalDisplayPort.SetEmpty();
  const gfx::Rect& criticalDisplayPort = GetParent()->GetFrameMetrics().mCriticalDisplayPort;
  if (!criticalDisplayPort.IsEmpty()) {
    gfxRect transformedCriticalDisplayPort =
      mPaintData.mTransformScreenToLayer.TransformBounds(
        gfxRect(criticalDisplayPort.x, criticalDisplayPort.y,
                criticalDisplayPort.width, criticalDisplayPort.height));
    transformedCriticalDisplayPort.RoundOut();
    mPaintData.mLayerCriticalDisplayPort = nsIntRect(transformedCriticalDisplayPort.x,
                                             transformedCriticalDisplayPort.y,
                                             transformedCriticalDisplayPort.width,
                                             transformedCriticalDisplayPort.height);
  }

  // Calculate the frame resolution.
  mPaintData.mResolution.SizeTo(1, 1);
  for (ContainerLayer* parent = GetParent(); parent; parent = parent->GetParent()) {
    const FrameMetrics& metrics = parent->GetFrameMetrics();
    mPaintData.mResolution.width *= metrics.mResolution.width;
    mPaintData.mResolution.height *= metrics.mResolution.height;
  }

  // Calculate the scroll offset since the last transaction, and the
  // composition bounds.
  mPaintData.mCompositionBounds.SetEmpty();
  mPaintData.mScrollOffset.MoveTo(0, 0);
  Layer* primaryScrollable = BasicManager()->GetPrimaryScrollableLayer();
  if (primaryScrollable) {
    const FrameMetrics& metrics = primaryScrollable->AsContainerLayer()->GetFrameMetrics();
    mPaintData.mScrollOffset = metrics.mScrollOffset;
    gfxRect transformedViewport = mPaintData.mTransformScreenToLayer.TransformBounds(
      gfxRect(metrics.mCompositionBounds.x, metrics.mCompositionBounds.y,
              metrics.mCompositionBounds.width, metrics.mCompositionBounds.height));
    transformedViewport.RoundOut();
    mPaintData.mCompositionBounds =
      nsIntRect(transformedViewport.x, transformedViewport.y,
                transformedViewport.width, transformedViewport.height);
  }
}

void
BasicTiledThebesLayer::EndPaint(bool aFinish)
{
  if (!aFinish && !mPaintData.mPaintFinished) {
    return;
  }

  mPaintData.mLastScrollOffset = mPaintData.mScrollOffset;
  mPaintData.mPaintFinished = true;
}

void
BasicTiledThebesLayer::PaintThebes(gfxContext* aContext,
                                   Layer* aMaskLayer,
                                   LayerManager::DrawThebesLayerCallback aCallback,
                                   void* aCallbackData,
                                   ReadbackProcessor* aReadback)
{
  if (!HasShadow()) {
    NS_ASSERTION(false, "Shadow requested for painting\n");
    return;
  }

  if (!aCallback) {
    BasicManager()->SetTransactionIncomplete();
    return;
  }

  if (!mContentClient) {
    mContentClient = new TiledContentClient(this, BasicManager());

    mContentClient->Connect();
    BasicManager()->Attach(mContentClient, this);
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
  if (aMaskLayer && !BasicManager()->IsRepeatTransaction()) {
    static_cast<BasicImplData*>(aMaskLayer->ImplData())->Paint(aContext, nullptr);
  }

  // Fast path for no progressive updates, no low-precision updates and no
  // critical display-port set.
  if (!gfxPlatform::UseProgressiveTilePainting() &&
      !gfxPlatform::UseLowPrecisionBuffer() &&
      GetParent()->GetFrameMetrics().mCriticalDisplayPort.IsEmpty()) {
    mValidRegion = mVisibleRegion;

    NS_ASSERTION(!BasicManager()->IsRepeatTransaction(), "Didn't paint our mask layer");

    mContentClient->mTiledBuffer.PaintThebes(mValidRegion, invalidRegion,
                                             aCallback, aCallbackData);

    BasicManager()->Hold(this);
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
  if (!BasicManager()->IsRepeatTransaction()) {
    mValidRegion.And(mValidRegion, mVisibleRegion);
    if (!mPaintData.mLayerCriticalDisplayPort.IsEmpty()) {
      // Make sure that tiles that fall outside of the critical displayport are
      // discarded on the first update.
      mValidRegion.And(mValidRegion, mPaintData.mLayerCriticalDisplayPort);
    }
  }

  nsIntRegion lowPrecisionInvalidRegion;
  if (!mPaintData.mLayerCriticalDisplayPort.IsEmpty()) {
    if (gfxPlatform::UseLowPrecisionBuffer()) {
      // Calculate the invalid region for the low precision buffer
      lowPrecisionInvalidRegion.Sub(mVisibleRegion, mLowPrecisionValidRegion);

      // Remove the valid region from the low precision valid region (we don't
      // validate this part of the low precision buffer).
      lowPrecisionInvalidRegion.Sub(lowPrecisionInvalidRegion, mValidRegion);
    }

    // Clip the invalid region to the critical display-port
    invalidRegion.And(invalidRegion, mPaintData.mLayerCriticalDisplayPort);
    if (invalidRegion.IsEmpty() && lowPrecisionInvalidRegion.IsEmpty()) {
      EndPaint(true);
      return;
    }
  }

  if (!invalidRegion.IsEmpty() && mPaintData.mLowPrecisionPaintCount == 0) {
    bool updatedBuffer = false;
    // Only draw progressively when the resolution is unchanged.
    if (gfxPlatform::UseProgressiveTilePainting() &&
        !BasicManager()->HasShadowTarget() &&
        mContentClient->mTiledBuffer.GetFrameResolution() == mPaintData.mResolution) {
      // Store the old valid region, then clear it before painting.
      // We clip the old valid region to the visible region, as it only gets
      // used to decide stale content (currently valid and previously visible)
      nsIntRegion oldValidRegion = mContentClient->mTiledBuffer.GetValidRegion();
      oldValidRegion.And(oldValidRegion, mVisibleRegion);
      if (!mPaintData.mLayerCriticalDisplayPort.IsEmpty()) {
        oldValidRegion.And(oldValidRegion, mPaintData.mLayerCriticalDisplayPort);
      }

      updatedBuffer =
        mContentClient->mTiledBuffer.ProgressiveUpdate(mValidRegion, invalidRegion,
                                                       oldValidRegion, &mPaintData,
                                                       aCallback, aCallbackData);
    } else {
      updatedBuffer = true;
      mValidRegion = mVisibleRegion;
      if (!mPaintData.mLayerCriticalDisplayPort.IsEmpty()) {
        mValidRegion.And(mValidRegion, mPaintData.mLayerCriticalDisplayPort);
      }
      mContentClient->mTiledBuffer.SetFrameResolution(mPaintData.mResolution);
      mContentClient->mTiledBuffer.PaintThebes(mValidRegion, invalidRegion,
                                               aCallback, aCallbackData);
    }

    if (updatedBuffer) {
      mPaintData.mFirstPaint = false;
      BasicManager()->Hold(this);
      mContentClient->LockCopyAndWrite(TiledContentClient::TILED_BUFFER);

      // If there are low precision updates, mark the paint as unfinished and
      // request a repeat transaction.
      if (!lowPrecisionInvalidRegion.IsEmpty() && mPaintData.mPaintFinished) {
        BasicManager()->SetRepeatTransaction();
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
      !nsIntRegion(mPaintData.mLayerCriticalDisplayPort).Contains(mVisibleRegion)) {
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
                                                 aCallback, aCallbackData);
    }
  } else if (!mLowPrecisionValidRegion.IsEmpty()) {
    // Clear the low precision tiled buffer
    updatedLowPrecision = true;
    mLowPrecisionValidRegion.SetEmpty();
    mContentClient->mLowPrecisionTiledBuffer.PaintThebes(mLowPrecisionValidRegion,
                                                         mLowPrecisionValidRegion,
                                                         aCallback, aCallbackData);
  }

  // We send a Painted callback if we clear the valid region of the low
  // precision buffer, so that the shadow buffer's valid region can be updated
  // and the associated resources can be freed.
  if (updatedLowPrecision) {
    BasicManager()->Hold(this);
    mContentClient->LockCopyAndWrite(TiledContentClient::LOW_PRECISION_TILED_BUFFER);
  }

  EndPaint(false);
}

} // mozilla
} // layers
