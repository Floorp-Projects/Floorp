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
ClientTiledThebesLayer::BeginPaint()
{
  if (ClientManager()->IsRepeatTransaction()) {
    return;
  }

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

  if (!displayPortAncestor || !scrollAncestor) {
    // No displayport or scroll ancestor, so we can't do progressive rendering.
#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_B2G)
    // Both Android and b2g are guaranteed to have a displayport set, so this
    // should never happen.
    NS_WARNING("Tiled Thebes layer with no scrollable container ancestor");
#endif
    return;
  }

  TILING_PRLOG(("TILING 0x%p: Found scrollAncestor 0x%p and displayPortAncestor 0x%p\n", this,
    scrollAncestor, displayPortAncestor));

  const FrameMetrics& scrollMetrics = scrollAncestor->GetFrameMetrics();
  const FrameMetrics& displayportMetrics = displayPortAncestor->GetFrameMetrics();

  // Calculate the transform required to convert ParentLayer space of our
  // display port ancestor to the Layer space of this layer.
  gfx3DMatrix transformToDisplayPort =
    GetTransformToAncestorsParentLayer(this, displayPortAncestor);

  mPaintData.mTransformDisplayPortToLayer = transformToDisplayPort.Inverse();

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
    ApplyParentLayerToLayerTransform(mPaintData.mTransformDisplayPortToLayer, criticalDisplayPort));
  TILING_PRLOG_OBJ(("TILING 0x%p: Critical displayport %s\n", this, tmpstr.get()), mPaintData.mCriticalDisplayPort);

  // Compute the viewport that applies to this layer in the LayoutDevice
  // space of this layer.
  ParentLayerRect viewport =
    (displayportMetrics.mViewport * displayportMetrics.GetZoomToParent())
    + displayportMetrics.mCompositionBounds.TopLeft();
  mPaintData.mViewport = ApplyParentLayerToLayerTransform(
    mPaintData.mTransformDisplayPortToLayer, viewport);
  TILING_PRLOG_OBJ(("TILING 0x%p: Viewport %s\n", this, tmpstr.get()), mPaintData.mViewport);

  // Store the resolution from the displayport ancestor layer. Because this is Gecko-side,
  // before any async transforms have occurred, we can use the zoom for this.
  mPaintData.mResolution = displayportMetrics.GetZoomToParent();
  TILING_PRLOG(("TILING 0x%p: Resolution %f\n", this, mPaintData.mResolution.scale));

  // Store the applicable composition bounds in this layer's Layer units.
  gfx3DMatrix transformToCompBounds =
    GetTransformToAncestorsParentLayer(this, scrollAncestor);
  mPaintData.mCompositionBounds = ApplyParentLayerToLayerTransform(
    transformToCompBounds.Inverse(), ParentLayerRect(scrollMetrics.mCompositionBounds));
  TILING_PRLOG_OBJ(("TILING 0x%p: Composition bounds %s\n", this, tmpstr.get()), mPaintData.mCompositionBounds);

  // Calculate the scroll offset since the last transaction
  mPaintData.mScrollOffset = displayportMetrics.GetScrollOffset() * displayportMetrics.GetZoomToParent();
  TILING_PRLOG_OBJ(("TILING 0x%p: Scroll offset %s\n", this, tmpstr.get()), mPaintData.mScrollOffset);
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
  TILING_PRLOG(("TILING 0x%p: Paint finished\n", this));
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

  TILING_PRLOG_OBJ(("TILING 0x%p: Initial visible region %s\n", this, tmpstr.get()), mVisibleRegion);
  TILING_PRLOG_OBJ(("TILING 0x%p: Initial valid region %s\n", this, tmpstr.get()), mValidRegion);

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

  bool isFixed = GetIsFixedPosition() || GetParent()->GetIsFixedPosition();

  // Fast path for no progressive updates, no low-precision updates and no
  // critical display-port set, or no display-port set, or this is a fixed
  // position layer/contained in a fixed position layer
  const FrameMetrics& parentMetrics = GetParent()->GetFrameMetrics();
  if ((!gfxPrefs::UseProgressiveTilePainting() &&
       !gfxPrefs::UseLowPrecisionBuffer() &&
       parentMetrics.mCriticalDisplayPort.IsEmpty()) ||
       parentMetrics.mDisplayPort.IsEmpty() ||
       isFixed) {
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

  TILING_PRLOG_OBJ(("TILING 0x%p: Valid region %s\n", this, tmpstr.get()), mValidRegion);
  TILING_PRLOG_OBJ(("TILING 0x%p: Visible region %s\n", this, tmpstr.get()), mVisibleRegion);

  // Make sure that tiles that fall outside of the visible region are
  // discarded on the first update.
  if (!ClientManager()->IsRepeatTransaction()) {
    mValidRegion.And(mValidRegion, mVisibleRegion);
    if (!mPaintData.mCriticalDisplayPort.IsEmpty()) {
      // Make sure that tiles that fall outside of the critical displayport are
      // discarded on the first update.
      mValidRegion.And(mValidRegion, LayerIntRect::ToUntyped(mPaintData.mCriticalDisplayPort));
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
    invalidRegion.And(invalidRegion, LayerIntRect::ToUntyped(mPaintData.mCriticalDisplayPort));
    if (invalidRegion.IsEmpty() && lowPrecisionInvalidRegion.IsEmpty()) {
      EndPaint(true);
      return;
    }
  }

  TILING_PRLOG_OBJ(("TILING 0x%p: Invalid region %s\n", this, tmpstr.get()), invalidRegion);

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
        oldValidRegion.And(oldValidRegion, LayerIntRect::ToUntyped(mPaintData.mCriticalDisplayPort));
      }

      TILING_PRLOG_OBJ(("TILING 0x%p: Progressive update with old valid region %s\n", this, tmpstr.get()), oldValidRegion);

      updatedBuffer =
        mContentClient->mTiledBuffer.ProgressiveUpdate(mValidRegion, invalidRegion,
                                                       oldValidRegion, &mPaintData,
                                                       callback, data);
    } else {
      updatedBuffer = true;
      mValidRegion = mVisibleRegion;
      if (!mPaintData.mCriticalDisplayPort.IsEmpty()) {
        mValidRegion.And(mValidRegion, LayerIntRect::ToUntyped(mPaintData.mCriticalDisplayPort));
      }

      TILING_PRLOG_OBJ(("TILING 0x%p: Painting: valid region %s\n", this, tmpstr.get()), mValidRegion);
      TILING_PRLOG_OBJ(("TILING 0x%p: and invalid region %s\n", this, tmpstr.get()), invalidRegion);

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

  TILING_PRLOG_OBJ(("TILING 0x%p: Low-precision valid region is %s\n", this, tmpstr.get()), mLowPrecisionValidRegion);
  TILING_PRLOG_OBJ(("TILING 0x%p: Low-precision invalid region is %s\n", this, tmpstr.get()), lowPrecisionInvalidRegion);

  // Render the low precision buffer, if there's area to invalidate and the
  // visible region is larger than the critical display port.
  bool updatedLowPrecision = false;
  if (!lowPrecisionInvalidRegion.IsEmpty() &&
      !nsIntRegion(LayerIntRect::ToUntyped(mPaintData.mCriticalDisplayPort)).Contains(mVisibleRegion)) {
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
    mContentClient->mLowPrecisionTiledBuffer.ResetPaintedAndValidState();
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
