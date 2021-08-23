/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/StackingContextHelper.h"

#include "mozilla/PresShell.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Matrix.h"
#include "UnitTransforms.h"
#include "nsDisplayList.h"
#include "mozilla/dom/BrowserChild.h"
#include "nsLayoutUtils.h"
#include "ActiveLayerTracker.h"

namespace mozilla {
namespace layers {
using namespace gfx;

StackingContextHelper::StackingContextHelper()
    : mBuilder(nullptr),
      mScale(1.0f, 1.0f),
      mAffectsClipPositioning(false),
      mDeferredTransformItem(nullptr),
      mRasterizeLocally(false) {
  // mOrigin remains at 0,0
}

static nsSize ComputeDesiredDisplaySizeForAnimation(nsIFrame* aContainerFrame) {
  // Use the size of the nearest widget as the maximum size.  This
  // is important since it might be a popup that is bigger than the
  // pres context's size.
  nsPresContext* presContext = aContainerFrame->PresContext();
  nsIWidget* widget = aContainerFrame->GetNearestWidget();
  if (widget) {
    return LayoutDevicePixel::ToAppUnits(widget->GetClientSize(),
                                         presContext->AppUnitsPerDevPixel());
  }

  return presContext->GetVisibleArea().Size();
}

/* static */
Size ChooseScale(nsIFrame* aContainerFrame, nsDisplayItem* aContainerItem,
                 const nsRect& aVisibleRect, float aXScale, float aYScale,
                 const Matrix& aTransform2d, bool aCanDraw2D) {
  Size scale;
  // XXX Should we do something for 3D transforms?
  if (aCanDraw2D && !aContainerFrame->Combines3DTransformWithAncestors() &&
      !aContainerFrame->HasPerspective()) {
    // If the container's transform is animated off main thread, fix a suitable
    // scale size for animation
    if (aContainerItem &&
        aContainerItem->GetType() == DisplayItemType::TYPE_TRANSFORM &&
        // FIXME: What we need is only transform, rotate, and scale, not
        // translate, so it's be better to use a property set, instead of
        // display item type here.
        EffectCompositor::HasAnimationsForCompositor(
            aContainerFrame, DisplayItemType::TYPE_TRANSFORM)) {
      nsSize displaySize =
          ComputeDesiredDisplaySizeForAnimation(aContainerFrame);
      // compute scale using the animation on the container, taking ancestors in
      // to account
      nsSize scaledVisibleSize = nsSize(aVisibleRect.Width() * aXScale,
                                        aVisibleRect.Height() * aYScale);
      scale = nsLayoutUtils::ComputeSuitableScaleForAnimation(
          aContainerFrame, scaledVisibleSize, displaySize);
      // multiply by the scale inherited from ancestors--we use a uniform
      // scale factor to prevent blurring when the layer is rotated.
      float incomingScale = std::max(aXScale, aYScale);
      scale.width *= incomingScale;
      scale.height *= incomingScale;
    } else {
      // Scale factors are normalized to a power of 2 to reduce the number of
      // resolution changes
      scale = aTransform2d.ScaleFactors();
      // For frames with a changing scale transform round scale factors up to
      // nearest power-of-2 boundary so that we don't keep having to redraw
      // the content as it scales up and down. Rounding up to nearest
      // power-of-2 boundary ensures we never scale up, only down --- avoiding
      // jaggies. It also ensures we never scale down by more than a factor of
      // 2, avoiding bad downscaling quality.
      Matrix frameTransform;
      if (ActiveLayerTracker::IsScaleSubjectToAnimation(aContainerFrame)) {
        scale.width = gfxUtils::ClampToScaleFactor(scale.width);
        scale.height = gfxUtils::ClampToScaleFactor(scale.height);

        // Limit animated scale factors to not grow excessively beyond the
        // display size.
        nsSize maxScale(4, 4);
        if (!aVisibleRect.IsEmpty()) {
          nsSize displaySize =
              ComputeDesiredDisplaySizeForAnimation(aContainerFrame);
          maxScale = Max(maxScale, displaySize / aVisibleRect.Size());
        }
        if (scale.width > maxScale.width) {
          scale.width = gfxUtils::ClampToScaleFactor(maxScale.width, true);
        }
        if (scale.height > maxScale.height) {
          scale.height = gfxUtils::ClampToScaleFactor(maxScale.height, true);
        }
      } else {
        // XXX Do we need to move nearly-integer values to integers here?
      }
    }
    // If the scale factors are too small, just use 1.0. The content is being
    // scaled out of sight anyway.
    if (fabs(scale.width) < 1e-8 || fabs(scale.height) < 1e-8) {
      scale = Size(1.0, 1.0);
    }
  } else {
    scale = Size(1.0, 1.0);
  }

  // Prevent the scale from getting too large, to avoid excessive memory
  // allocation. Usually memory allocation is limited by the visible region,
  // which should be restricted to the display port. But at very large scales
  // the visible region itself can become excessive due to rounding errors.
  // Clamping the scale here prevents that.
  scale =
      Size(std::min(scale.width, 32768.0f), std::min(scale.height, 32768.0f));

  return scale;
}

StackingContextHelper::StackingContextHelper(
    const StackingContextHelper& aParentSC, const ActiveScrolledRoot* aAsr,
    nsIFrame* aContainerFrame, nsDisplayItem* aContainerItem,
    wr::DisplayListBuilder& aBuilder, const wr::StackingContextParams& aParams,
    const LayoutDeviceRect& aBounds)
    : mBuilder(&aBuilder),
      mScale(1.0f, 1.0f),
      mDeferredTransformItem(aParams.mDeferredTransformItem),
      mRasterizeLocally(aParams.mRasterizeLocally ||
                        aParentSC.mRasterizeLocally) {
  MOZ_ASSERT(!aContainerItem || aContainerItem->CreatesStackingContextHelper());

  mOrigin = aParentSC.mOrigin + aBounds.TopLeft();
  // Compute scale for fallback rendering. We don't try to guess a scale for 3d
  // transformed items

  if (aParams.mBoundTransform) {
    gfx::Matrix transform2d;
    bool canDraw2D = aParams.mBoundTransform->CanDraw2D(&transform2d);
    if (canDraw2D &&
        aParams.reference_frame_kind != wr::WrReferenceFrameKind::Perspective &&
        !aContainerFrame->Combines3DTransformWithAncestors()) {
      mInheritedTransform = transform2d * aParentSC.mInheritedTransform;

      int32_t apd = aContainerFrame->PresContext()->AppUnitsPerDevPixel();
      nsRect r = LayoutDevicePixel::ToAppUnits(aBounds, apd);
      mScale = ChooseScale(aContainerFrame, aContainerItem, r,
                           aParentSC.mScale.width, aParentSC.mScale.height,
                           mInheritedTransform,
                           /* aCanDraw2D = */ true);
    } else {
      mScale = gfx::Size(1.0f, 1.0f);
      mInheritedTransform = gfx::Matrix::Scaling(1.f, 1.f);
    }

    if (aParams.mAnimated) {
      mSnappingSurfaceTransform =
          gfx::Matrix::Scaling(mScale.width, mScale.height);
    } else {
      mSnappingSurfaceTransform =
          transform2d * aParentSC.mSnappingSurfaceTransform;
    }

  } else if (aParams.reference_frame_kind ==
                 wr::WrReferenceFrameKind::Transform &&
             aContainerItem &&
             aContainerItem->GetType() == DisplayItemType::TYPE_ASYNC_ZOOM &&
             aContainerItem->Frame()) {
    double resolution = aContainerItem->Frame()->PresShell()->GetResolution();
    gfx::Matrix transform = gfx::Matrix::Scaling(resolution, resolution);

    mInheritedTransform = transform * aParentSC.mInheritedTransform;
    mScale = resolution * aParentSC.mScale;

    MOZ_ASSERT(!aParams.mAnimated);
    mSnappingSurfaceTransform = transform * aParentSC.mSnappingSurfaceTransform;

  } else if (!aAsr && !aContainerFrame && !aContainerItem &&
             aParams.mRootReferenceFrame) {
    // this is the root stacking context helper
    float resolutionX = 1.f;
    float resolutionY = 1.f;

    // If we are in a remote browser, then apply scaling from ancestor browsers
    if (mozilla::dom::BrowserChild* browserChild =
            mozilla::dom::BrowserChild::GetFrom(
                aParams.mRootReferenceFrame->PresShell())) {
      resolutionX *= browserChild->GetEffectsInfo().mScaleX;
      resolutionY *= browserChild->GetEffectsInfo().mScaleY;
    }

    gfx::Matrix transform = gfx::Matrix::Scaling(resolutionX, resolutionY);

    mInheritedTransform = transform * aParentSC.mInheritedTransform;
    mScale = gfx::Size(aParentSC.mScale.width * resolutionX,
                       aParentSC.mScale.height * resolutionY);

    MOZ_ASSERT(!aParams.mAnimated);
    mSnappingSurfaceTransform = transform * aParentSC.mSnappingSurfaceTransform;

  } else {
    mInheritedTransform = aParentSC.mInheritedTransform;
    mScale = aParentSC.mScale;
  }

  auto rasterSpace =
      mRasterizeLocally
          ? wr::RasterSpace::Local(std::max(mScale.width, mScale.height))
          : wr::RasterSpace::Screen();

  MOZ_ASSERT(!aParams.clip.IsNone());
  mReferenceFrameId = mBuilder->PushStackingContext(
      aParams, wr::ToLayoutRect(aBounds), rasterSpace);

  if (mReferenceFrameId) {
    mSpaceAndClipChainHelper.emplace(aBuilder, mReferenceFrameId.ref());
  }

  mAffectsClipPositioning =
      mReferenceFrameId.isSome() || (aBounds.TopLeft() != LayoutDevicePoint());

  // If the parent stacking context has a deferred transform item, inherit it
  // into this stacking context, as long as the ASR hasn't changed. Refer to
  // the comments on StackingContextHelper::mDeferredTransformItem for an
  // explanation of what goes in these fields.
  if (aParentSC.mDeferredTransformItem &&
      aAsr == aParentSC.mDeferredTransformItem->GetActiveScrolledRoot()) {
    if (mDeferredTransformItem) {
      // If we are deferring another transform, put the combined transform from
      // all the ancestor deferred items into mDeferredAncestorTransform
      mDeferredAncestorTransform = aParentSC.GetDeferredTransformMatrix();
    } else {
      // We are not deferring another transform, so we can just inherit the
      // parent stacking context's deferred data without any modification.
      mDeferredTransformItem = aParentSC.mDeferredTransformItem;
      mDeferredAncestorTransform = aParentSC.mDeferredAncestorTransform;
    }
  }
}

StackingContextHelper::~StackingContextHelper() {
  if (mBuilder) {
    mSpaceAndClipChainHelper.reset();
    mBuilder->PopStackingContext(mReferenceFrameId.isSome());
  }
}

nsDisplayTransform* StackingContextHelper::GetDeferredTransformItem() const {
  return mDeferredTransformItem;
}

Maybe<gfx::Matrix4x4> StackingContextHelper::GetDeferredTransformMatrix()
    const {
  if (mDeferredTransformItem) {
    // See the comments on StackingContextHelper::mDeferredTransformItem for
    // an explanation of what's stored in mDeferredTransformItem and
    // mDeferredAncestorTransform. Here we need to return the combined transform
    // transform from all the deferred ancestors, including
    // mDeferredTransformItem.
    gfx::Matrix4x4 result = mDeferredTransformItem->GetTransform().GetMatrix();
    if (mDeferredAncestorTransform) {
      result = result * *mDeferredAncestorTransform;
    }
    return Some(result);
  } else {
    return Nothing();
  }
}

}  // namespace layers
}  // namespace mozilla
