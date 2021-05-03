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

LayerToParentLayerMatrix4x4 ComputeTransformForScrollThumb(
    const LayerToParentLayerMatrix4x4& aCurrentTransform,
    const gfx::Matrix4x4& aScrollableContentTransform,
    AsyncPanZoomController* aApzc, const FrameMetrics& aMetrics,
    const ScrollbarData& aScrollbarData, bool aScrollbarIsDescendant,
    AsyncTransformComponentMatrix* aOutClipTransform) {
  // We only apply the transform if the scroll-target layer has non-container
  // children (i.e. when it has some possibly-visible content). This is to
  // avoid moving scroll-bars in the situation that only a scroll information
  // layer has been built for a scroll frame, as this would result in a
  // disparity between scrollbars and visible content.
  if (aMetrics.IsScrollInfoLayer()) {
    return LayerToParentLayerMatrix4x4{};
  }

  MOZ_RELEASE_ASSERT(aApzc);

  AsyncTransformComponentMatrix asyncTransform =
      aApzc->GetCurrentAsyncTransform(AsyncPanZoomController::eForCompositing);

  // |asyncTransform| represents the amount by which we have scrolled and
  // zoomed since the last paint. Because the scrollbar was sized and positioned
  // based on the painted content, we need to adjust it based on asyncTransform
  // so that it reflects what the user is actually seeing now.
  AsyncTransformComponentMatrix scrollbarTransform;
  if (*aScrollbarData.mDirection == ScrollDirection::eVertical) {
    ParentLayerCoord asyncScrollY = asyncTransform._42;
    const float asyncZoomY = asyncTransform._22;

    // The scroll thumb needs to be scaled in the direction of scrolling by the
    // inverse of the async zoom. This is because zooming in decreases the
    // fraction of the whole srollable rect that is in view.
    const float yScale = 1.f / asyncZoomY;

    // Note: |metrics.GetZoom()| doesn't yet include the async zoom.
    const CSSToParentLayerScale effectiveZoom(aMetrics.GetZoom().yScale *
                                              asyncZoomY);

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

      asyncScrollY -= ((aMetrics.GetLayoutScrollOffset() -
                        aMetrics.GetVisualScrollOffset()) *
                       effectiveZoom)
                          .y;
    }

    // Here we convert the scrollbar thumb ratio into a true unitless ratio by
    // dividing out the conversion factor from the scrollframe's parent's space
    // to the scrollframe's space.
    const float ratio = aScrollbarData.mThumbRatio /
                        (aMetrics.GetPresShellResolution() * asyncZoomY);
    // The scroll thumb needs to be translated in opposite direction of the
    // async scroll. This is because scrolling down, which translates the layer
    // content up, should result in moving the scroll thumb down.
    ParentLayerCoord yTranslation = -asyncScrollY * ratio;

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
    const CSSCoord thumbOrigin = (aMetrics.GetVisualScrollOffset().y * ratio);
    const CSSCoord thumbOriginScaled = thumbOrigin * yScale;
    const CSSCoord thumbOriginDelta = thumbOriginScaled - thumbOrigin;
    const ParentLayerCoord thumbOriginDeltaPL =
        thumbOriginDelta * effectiveZoom;
    yTranslation -= thumbOriginDeltaPL;

    scrollbarTransform.PostScale(1.f, yScale, 1.f);
    scrollbarTransform.PostTranslate(0, yTranslation, 0);
  }
  if (*aScrollbarData.mDirection == ScrollDirection::eHorizontal) {
    // See detailed comments under the eVertical case.

    ParentLayerCoord asyncScrollX = asyncTransform._41;
    const float asyncZoomX = asyncTransform._11;

    const float xScale = 1.f / asyncZoomX;

    const CSSToParentLayerScale effectiveZoom(aMetrics.GetZoom().xScale *
                                              asyncZoomX);

    if (gfxPlatform::UseDesktopZoomingScrollbars()) {
      asyncScrollX -= ((aMetrics.GetLayoutScrollOffset() -
                        aMetrics.GetVisualScrollOffset()) *
                       effectiveZoom)
                          .x;
    }

    const float ratio = aScrollbarData.mThumbRatio /
                        (aMetrics.GetPresShellResolution() * asyncZoomX);
    ParentLayerCoord xTranslation = -asyncScrollX * ratio;

    const CSSCoord thumbOrigin = (aMetrics.GetVisualScrollOffset().x * ratio);
    const CSSCoord thumbOriginScaled = thumbOrigin * xScale;
    const CSSCoord thumbOriginDelta = thumbOriginScaled - thumbOrigin;
    const ParentLayerCoord thumbOriginDeltaPL =
        thumbOriginDelta * effectiveZoom;
    xTranslation -= thumbOriginDeltaPL;

    scrollbarTransform.PostScale(xScale, 1.f, 1.f);
    scrollbarTransform.PostTranslate(xTranslation, 0, 0);
  }

  LayerToParentLayerMatrix4x4 transform =
      aCurrentTransform * scrollbarTransform;

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
  if (aScrollbarIsDescendant) {
    AsyncTransformComponentMatrix overscroll =
        aApzc->GetOverscrollTransform(AsyncPanZoomController::eForCompositing);
    gfx::Matrix4x4 asyncUntransform =
        (asyncTransform * overscroll).Inverse().ToUnknownMatrix();
    const gfx::Matrix4x4& contentTransform = aScrollableContentTransform;
    gfx::Matrix4x4 contentUntransform = contentTransform.Inverse();

    compensation *= ViewAs<AsyncTransformComponentMatrix>(
        contentTransform * asyncUntransform * contentUntransform);

    // Pass the total compensation out to the caller so that it can use it
    // to transform clip transforms as needed.
    if (aOutClipTransform) {
      *aOutClipTransform = compensation;
    }
  }
  transform = transform * compensation;

  return transform;
}

}  // namespace apz
}  // namespace layers
}  // namespace mozilla
