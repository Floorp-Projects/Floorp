/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

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

namespace mozilla {
namespace layers {


ClientTiledThebesLayer::ClientTiledThebesLayer(ClientLayerManager* const aManager)
  : ThebesLayer(aManager,
                static_cast<ClientLayer*>(MOZ_THIS_IN_INITIALIZER_LIST()))
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
}

void
ClientTiledThebesLayer::FillSpecificAttributes(SpecificLayerAttributes& aAttrs)
{
  aAttrs = ThebesLayerAttributes(GetValidRegion());
}

static LayoutDeviceRect
ApplyParentLayerToLayoutTransform(const gfx3DMatrix& aTransform, const ParentLayerRect& aParentLayerRect)
{
  return TransformTo<LayoutDevicePixel>(aTransform, aParentLayerRect);
}

void
ClientTiledThebesLayer::BeginPaint()
{
  if (ClientManager()->IsRepeatTransaction()) {
    return;
  }

  mPaintData.mLowPrecisionPaintCount = 0;
  mPaintData.mPaintFinished = false;
  mPaintData.mCompositionBounds.SetEmpty();
  mPaintData.mCriticalDisplayPort.SetEmpty();

  if (!GetBaseTransform().Is2DIntegerTranslation()) {
    // Give up if the layer is transformed. The code below assumes that there
    // is no transform set, and not making that assumption would cause huge
    // complication to handle a quite rare case.
    //
    // FIXME The intention is to bail out of this function when there's a CSS
    //       transform set on the layer, but unfortunately there's no way to
    //       distinguish transforms due to scrolling from transforms due to
    //       CSS transforms.
    //
    //       Because of this, there may be unintended behaviour when setting
    //       2d CSS translations on the children of scrollable displayport
    //       layers.
    return;
  }

  // Get the metrics of the nearest scrollable layer and the nearest layer
  // with a displayport.
  ContainerLayer* displayPortParent = nullptr;
  ContainerLayer* scrollParent = nullptr;
  for (ContainerLayer* parent = GetParent(); parent; parent = parent->GetParent()) {
    const FrameMetrics& metrics = parent->GetFrameMetrics();
    if (!scrollParent && metrics.GetScrollId() != FrameMetrics::NULL_SCROLL_ID) {
      scrollParent = parent;
    }
    if (!metrics.mDisplayPort.IsEmpty()) {
      displayPortParent = parent;
      // Any layer that has a displayport must be scrollable, so we can break
      // here.
      break;
    }
  }

  if (!displayPortParent || !scrollParent) {
    // No displayport or scroll parent, so we can't do progressive rendering.
    // Just set the composition bounds to empty and return.
#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_B2G)
    // Both Android and b2g are guaranteed to have a displayport set, so this
    // should never happen.
    NS_WARNING("Tiled Thebes layer with no scrollable container parent");
#endif
    return;
  }

  // Note, not handling transformed layers lets us assume that LayoutDevice
  // space of the scroll parent layer is the same as LayoutDevice space of
  // this layer.
  const FrameMetrics& scrollMetrics = scrollParent->GetFrameMetrics();
  const FrameMetrics& displayportMetrics = displayPortParent->GetFrameMetrics();

  // Calculate the transform required to convert ParentLayer space of our
  // display port parent to LayoutDevice space of this layer.
  gfx::Matrix4x4 transform = scrollParent->GetTransform();
  ContainerLayer* displayPortParentParent = displayPortParent->GetParent() ?
    displayPortParent->GetParent()->GetParent() : nullptr;
  for (ContainerLayer* parent = scrollParent->GetParent();
       parent != displayPortParentParent;
       parent = parent->GetParent()) {
    transform = transform * parent->GetTransform();
  }
  gfx3DMatrix layoutDeviceToScrollParentLayer;
  gfx::To3DMatrix(transform, layoutDeviceToScrollParentLayer);
  layoutDeviceToScrollParentLayer.ScalePost(scrollMetrics.mCumulativeResolution.scale,
                                            scrollMetrics.mCumulativeResolution.scale,
                                            1.f);

  mPaintData.mTransformParentLayerToLayoutDevice = layoutDeviceToScrollParentLayer.Inverse();

  // Compute the critical display port of the display port layer in
  // LayoutDevice space of this layer.
  ParentLayerRect criticalDisplayPort =
    (displayportMetrics.mCriticalDisplayPort + displayportMetrics.GetScrollOffset()) *
    displayportMetrics.GetZoomToParent();
  mPaintData.mCriticalDisplayPort = LayoutDeviceIntRect::ToUntyped(RoundedOut(
    ApplyParentLayerToLayoutTransform(mPaintData.mTransformParentLayerToLayoutDevice,
                                      criticalDisplayPort)));

  // Compute the viewport of the display port layer in LayoutDevice space of
  // this layer.
  ParentLayerRect viewport =
    (displayportMetrics.mViewport + displayportMetrics.GetScrollOffset()) *
    displayportMetrics.GetZoomToParent();
  mPaintData.mViewport = ApplyParentLayerToLayoutTransform(
    mPaintData.mTransformParentLayerToLayoutDevice, viewport);

  // Store the scroll parent resolution. Because this is Gecko-side, before any
  // async transforms have occurred, we can use the zoom for this.
  mPaintData.mResolution = displayportMetrics.GetZoomToParent();

  // Store the parent composition bounds in LayoutDevice units.
  // This is actually in LayoutDevice units of the scrollParent's parent layer,
  // but because there is no transform, we can assume that these are the same.
  mPaintData.mCompositionBounds =
    scrollMetrics.mCompositionBounds / scrollMetrics.GetParentResolution();

  // Calculate the scroll offset since the last transaction
  mPaintData.mScrollOffset = displayportMetrics.GetScrollOffset() * displayportMetrics.GetZoomToParent();
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
  if ((!gfxPrefs::UseProgressiveTilePainting() &&
       !gfxPrefs::UseLowPrecisionBuffer() &&
       parentMetrics.mCriticalDisplayPort.IsEmpty()) ||
       parentMetrics.mDisplayPort.IsEmpty()) {
    mValidRegion = mVisibleRegion;

    NS_ASSERTION(!ClientManager()->IsRepeatTransaction(), "Didn't paint our mask layer");

    mContentClient->mTiledBuffer.PaintThebes(mValidRegion, invalidRegion,
                                             callback, data);

    ClientManager()->Hold(this);
    mContentClient->UseTiledLayerBuffer(TiledContentClient::TILED_BUFFER);

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
    if (!mPaintData.mCriticalDisplayPort.IsEmpty()) {
      // Make sure that tiles that fall outside of the critical displayport are
      // discarded on the first update.
      mValidRegion.And(mValidRegion, mPaintData.mCriticalDisplayPort);
    }
  }

  nsIntRegion lowPrecisionInvalidRegion;
  if (!mPaintData.mCriticalDisplayPort.IsEmpty()) {
    if (gfxPrefs::UseLowPrecisionBuffer()) {
      // Calculate the invalid region for the low precision buffer
      lowPrecisionInvalidRegion.Sub(mVisibleRegion, mLowPrecisionValidRegion);

      // Remove the valid region from the low precision valid region (we don't
      // validate this part of the low precision buffer).
      lowPrecisionInvalidRegion.Sub(lowPrecisionInvalidRegion, mValidRegion);
    }

    // Clip the invalid region to the critical display-port
    invalidRegion.And(invalidRegion, mPaintData.mCriticalDisplayPort);
    if (invalidRegion.IsEmpty() && lowPrecisionInvalidRegion.IsEmpty()) {
      EndPaint(true);
      return;
    }
  }

  if (!invalidRegion.IsEmpty() && mPaintData.mLowPrecisionPaintCount == 0) {
    bool updatedBuffer = false;
    // Only draw progressively when the resolution is unchanged.
    if (gfxPrefs::UseProgressiveTilePainting() &&
        !ClientManager()->HasShadowTarget() &&
        mContentClient->mTiledBuffer.GetFrameResolution() == mPaintData.mResolution) {
      // Store the old valid region, then clear it before painting.
      // We clip the old valid region to the visible region, as it only gets
      // used to decide stale content (currently valid and previously visible)
      nsIntRegion oldValidRegion = mContentClient->mTiledBuffer.GetValidRegion();
      oldValidRegion.And(oldValidRegion, mVisibleRegion);
      if (!mPaintData.mCriticalDisplayPort.IsEmpty()) {
        oldValidRegion.And(oldValidRegion, mPaintData.mCriticalDisplayPort);
      }

      updatedBuffer =
        mContentClient->mTiledBuffer.ProgressiveUpdate(mValidRegion, invalidRegion,
                                                       oldValidRegion, &mPaintData,
                                                       callback, data);
    } else {
      updatedBuffer = true;
      mValidRegion = mVisibleRegion;
      if (!mPaintData.mCriticalDisplayPort.IsEmpty()) {
        mValidRegion.And(mValidRegion, mPaintData.mCriticalDisplayPort);
      }
      mContentClient->mTiledBuffer.SetFrameResolution(mPaintData.mResolution);
      mContentClient->mTiledBuffer.PaintThebes(mValidRegion, invalidRegion,
                                               callback, data);
    }

    if (updatedBuffer) {
      ClientManager()->Hold(this);
      mContentClient->UseTiledLayerBuffer(TiledContentClient::TILED_BUFFER);

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
      !nsIntRegion(mPaintData.mCriticalDisplayPort).Contains(mVisibleRegion)) {
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
    mContentClient->UseTiledLayerBuffer(TiledContentClient::LOW_PRECISION_TILED_BUFFER);
  }

  EndPaint(false);
}

} // mozilla
} // layers
