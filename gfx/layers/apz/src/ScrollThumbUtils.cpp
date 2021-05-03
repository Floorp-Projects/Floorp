/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollThumbUtils.h"
#include "AsyncPanZoomController.h"
#include "FrameMetrics.h"
#include "mozilla/gfx/Matrix.h"

namespace mozilla {
namespace layers {
namespace apz {

struct AsyncScrollThumbTransformer {
  // Inputs
  const LayerToParentLayerMatrix4x4& mCurrentTransform;
  const gfx::Matrix4x4& mScrollableContentTransform;
  AsyncPanZoomController* mApzc;
  const FrameMetrics& mMetrics;
  const ScrollbarData& mScrollbarData;
  bool mScrollbarIsDescendant;
  AsyncTransformComponentMatrix* mOutClipTransform;

  // Intermediate results
  AsyncTransformComponentMatrix mAsyncTransform;
  AsyncTransformComponentMatrix mScrollbarTransform;

  LayerToParentLayerMatrix4x4 ComputeTransform();

 private:
  // Helper functions for ComputeTransform().

  // If the thumb's orientation is along |aAxis|, add transformations
  // of the thumb into |mScrollbarTransform|.
  void ApplyTransformForAxis(const Axis& aAxis);
};

void AsyncScrollThumbTransformer::ApplyTransformForAxis(const Axis& aAxis) {
  ParentLayerCoord asyncScroll = aAxis.GetTransformTranslation(mAsyncTransform);
  const float asyncZoom = aAxis.GetTransformScale(mAsyncTransform);

  // The scroll thumb needs to be scaled in the direction of scrolling by the
  // inverse of the async zoom. This is because zooming in decreases the
  // fraction of the whole srollable rect that is in view.
  const float scale = 1.f / asyncZoom;

  // Note: |metrics.GetZoom()| doesn't yet include the async zoom.
  const CSSToParentLayerScale effectiveZoom(
      aAxis.GetAxisScale(mMetrics.GetZoom()).scale * asyncZoom);

  if (gfxPlatform::UseDesktopZoomingScrollbars()) {
    // As computed by GetCurrentAsyncTransform, asyncScrollY is
    //   asyncScrollY = -(GetEffectiveScrollOffset -
    //   mLastContentPaintMetrics.GetLayoutScrollOffset()) *
    //   effectiveZoom
    // where GetEffectiveScrollOffset includes the visual viewport offset that
    // the main thread knows about plus any async scrolling to the visual
    // viewport offset that the main thread does not (yet) know about. We want
    // asyncScrollY to be
    //   asyncScrollY = -(GetEffectiveScrollOffset -
    //   mLastContentPaintMetrics.GetVisualScrollOffset()) * effectiveZoom
    // because the main thread positions the scrollbars at the visual viewport
    // offset that it knows about. (aMetrics is mLastContentPaintMetrics)

    asyncScroll -= aAxis.GetPointOffset(
        (mMetrics.GetLayoutScrollOffset() - mMetrics.GetVisualScrollOffset()) *
        effectiveZoom);
  }

  // Here we convert the scrollbar thumb ratio into a true unitless ratio by
  // dividing out the conversion factor from the scrollframe's parent's space
  // to the scrollframe's space.
  const float ratio = mScrollbarData.mThumbRatio /
                      (mMetrics.GetPresShellResolution() * asyncZoom);
  // The scroll thumb needs to be translated in opposite direction of the
  // async scroll. This is because scrolling down, which translates the layer
  // content up, should result in moving the scroll thumb down.
  ParentLayerCoord translation = -asyncScroll * ratio;

  // The scroll thumb additionally needs to be translated to compensate for
  // the scale applied above. The origin with respect to which the scale is
  // applied is the origin of the entire scrollbar, rather than the origin of
  // the scroll thumb (meaning, for a vertical scrollbar it's at the top of
  // the composition bounds). This means that empty space above the thumb
  // is scaled too, effectively translating the thumb. We undo that
  // translation here.
  // (One can think of the adjustment being done to the translation here as
  // a change of basis. We have a method to help with that,
  // Matrix4x4::ChangeBasis(), but it wouldn't necessarily make the code
  // cleaner in this case).
  const CSSCoord thumbOrigin =
      (aAxis.GetPointOffset(mMetrics.GetVisualScrollOffset()) * ratio);
  const CSSCoord thumbOriginScaled = thumbOrigin * scale;
  const CSSCoord thumbOriginDelta = thumbOriginScaled - thumbOrigin;
  const ParentLayerCoord thumbOriginDeltaPL = thumbOriginDelta * effectiveZoom;
  translation -= thumbOriginDeltaPL;

  aAxis.PostScale(mScrollbarTransform, scale);
  aAxis.PostTranslate(mScrollbarTransform, translation);
}

LayerToParentLayerMatrix4x4 AsyncScrollThumbTransformer::ComputeTransform() {
  // We only apply the transform if the scroll-target layer has non-container
  // children (i.e. when it has some possibly-visible content). This is to
  // avoid moving scroll-bars in the situation that only a scroll information
  // layer has been built for a scroll frame, as this would result in a
  // disparity between scrollbars and visible content.
  if (mMetrics.IsScrollInfoLayer()) {
    return LayerToParentLayerMatrix4x4{};
  }

  MOZ_RELEASE_ASSERT(mApzc);

  mAsyncTransform =
      mApzc->GetCurrentAsyncTransform(AsyncPanZoomController::eForCompositing);

  // |mAsyncTransform| represents the amount by which we have scrolled and
  // zoomed since the last paint. Because the scrollbar was sized and positioned
  // based on the painted content, we need to adjust it based on asyncTransform
  // so that it reflects what the user is actually seeing now.
  if (*mScrollbarData.mDirection == ScrollDirection::eVertical) {
    ApplyTransformForAxis(mApzc->mY);
  }
  if (*mScrollbarData.mDirection == ScrollDirection::eHorizontal) {
    ApplyTransformForAxis(mApzc->mX);
  }

  LayerToParentLayerMatrix4x4 transform =
      mCurrentTransform * mScrollbarTransform;

  AsyncTransformComponentMatrix compensation;
  // If the scrollbar layer is a child of the content it is a scrollbar for,
  // then we need to adjust for any async transform (including an overscroll
  // transform) on the content. This needs to be cancelled out because layout
  // positions and sizes the scrollbar on the assumption that there is no async
  // transform, and without this adjustment the scrollbar will end up in the
  // wrong place.
  //
  // Note that since the async transform is applied on top of the content's
  // regular transform, we need to make sure to unapply the async transform in
  // the same coordinate space. This requires applying the content transform
  // and then unapplying it after unapplying the async transform.
  if (mScrollbarIsDescendant) {
    AsyncTransformComponentMatrix overscroll =
        mApzc->GetOverscrollTransform(AsyncPanZoomController::eForCompositing);
    gfx::Matrix4x4 asyncUntransform =
        (mAsyncTransform * overscroll).Inverse().ToUnknownMatrix();
    const gfx::Matrix4x4& contentTransform = mScrollableContentTransform;
    gfx::Matrix4x4 contentUntransform = contentTransform.Inverse();

    compensation *= ViewAs<AsyncTransformComponentMatrix>(
        contentTransform * asyncUntransform * contentUntransform);

    // Pass the total compensation out to the caller so that it can use it
    // to transform clip transforms as needed.
    if (mOutClipTransform) {
      *mOutClipTransform = compensation;
    }
  }
  transform = transform * compensation;

  return transform;
}

LayerToParentLayerMatrix4x4 ComputeTransformForScrollThumb(
    const LayerToParentLayerMatrix4x4& aCurrentTransform,
    const gfx::Matrix4x4& aScrollableContentTransform,
    AsyncPanZoomController* aApzc, const FrameMetrics& aMetrics,
    const ScrollbarData& aScrollbarData, bool aScrollbarIsDescendant,
    AsyncTransformComponentMatrix* aOutClipTransform) {
  return AsyncScrollThumbTransformer{aCurrentTransform,
                                     aScrollableContentTransform,
                                     aApzc,
                                     aMetrics,
                                     aScrollbarData,
                                     aScrollbarIsDescendant,
                                     aOutClipTransform}
      .ComputeTransform();
}

}  // namespace apz
}  // namespace layers
}  // namespace mozilla
