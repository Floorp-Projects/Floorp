/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollThumbUtils.h"
#include "AsyncPanZoomController.h"
#include "FrameMetrics.h"
#include "gfxPlatform.h"
#include "mozilla/gfx/Matrix.h"
#include "mozilla/StaticPrefs_toolkit.h"

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

  // Intermediate results
  AsyncTransformComponentMatrix mAsyncTransform;
  AsyncTransformComponentMatrix mScrollbarTransform;

  LayerToParentLayerMatrix4x4 ComputeTransform();

 private:
  // Helper functions for ComputeTransform().

  // If the thumb's orientation is along |aAxis|, add transformations
  // of the thumb into |mScrollbarTransform|.
  void ApplyTransformForAxis(const Axis& aAxis);

  enum class ScrollThumbExtent { Start, End };

  // Scale the thumb by |aScale| along |aAxis|, while keeping constant the
  // position of the top denoted by |aExtent|.
  void ScaleThumbBy(const Axis& aAxis, float aScale, ScrollThumbExtent aExtent);
};

void AsyncScrollThumbTransformer::ScaleThumbBy(const Axis& aAxis, float aScale,
                                               ScrollThumbExtent aExtent) {
  // To keep the position of the top of the thumb constant, the thumb needs to
  // translated to compensate for the scale applied. The origin with respect to
  // which the scale is applied is the origin of the layer tree, rather than
  // the origin of the scroll thumb. This means that the space between the
  // origin and the top of thumb (including the part of the scrollbar track
  // above the thumb, the part of the scrollbar above the track (i.e. a
  // scrollbar button if present), plus whatever content is above the scroll
  // frame) is scaled too, effectively translating the thumb. We undo that
  // translation here. (One can think of the adjustment being done to the
  // translation here as a change of basis. We have a method to help with that,
  // Matrix4x4::ChangeBasis(), but it wouldn't necessarily make the code cleaner
  // in this case).
  const CSSCoord scrollTrackOrigin =
      aAxis.GetPointOffset(
          mMetrics.CalculateCompositionBoundsInCssPixelsOfSurroundingContent()
              .TopLeft()) +
      mScrollbarData.mScrollTrackStart;
  CSSCoord thumbExtent = scrollTrackOrigin + mScrollbarData.mThumbStart;
  if (aExtent == ScrollThumbExtent::End) {
    thumbExtent += mScrollbarData.mThumbLength;
  }
  const CSSCoord thumbExtentScaled = thumbExtent * aScale;
  const CSSCoord thumbExtentDelta = thumbExtentScaled - thumbExtent;
  const ParentLayerCoord thumbExtentDeltaPL =
      thumbExtentDelta * mMetrics.GetDevPixelsPerCSSPixel() *
      LayoutDeviceToParentLayerScale(1.0);

  aAxis.PostScale(mScrollbarTransform, aScale);
  aAxis.PostTranslate(mScrollbarTransform, -thumbExtentDeltaPL);
}

void AsyncScrollThumbTransformer::ApplyTransformForAxis(const Axis& aAxis) {
  const ParentLayerCoord asyncScroll =
      aAxis.GetTransformTranslation(mAsyncTransform);
  const float asyncZoom = aAxis.GetTransformScale(mAsyncTransform);
  const ParentLayerCoord overscroll =
      aAxis.GetPointOffset(mApzc->GetOverscrollAmount());

  if (FuzzyEqualsAdditive(asyncZoom, 1.f) && mApzc->IsZero(asyncScroll) &&
      mApzc->IsZero(overscroll)) {
    return;
  }

  const CSSRect visualViewportRect = mApzc->GetCurrentAsyncVisualViewport(
      AsyncPanZoomController::eForCompositing);
  const CSSCoord visualViewportLength = aAxis.GetRectLength(visualViewportRect);
  // The calculations here closely follow the main thread calculations at
  // https://searchfox.org/mozilla-central/rev/0bf957f909ae1f3d19b43fd4edfc277342554836/layout/generic/nsGfxScrollFrame.cpp#6902-6927
  // and
  // https://searchfox.org/mozilla-central/rev/0bf957f909ae1f3d19b43fd4edfc277342554836/layout/xul/nsSliderFrame.cpp#587-614
  // Any modifications there should be reflected here as well.
  const CSSCoord pageIncrementMin =
      static_cast<int>(visualViewportLength * 0.8);
  CSSCoord pageIncrement;

  if (*mScrollbarData.mDirection == ScrollDirection::eVertical) {
    const CSSCoord lineScrollAmount =
        mApzc->GetScrollMetadata().GetLineScrollAmount().height;
    const double kScrollMultiplier =
        StaticPrefs::toolkit_scrollbox_verticalScrollDistance();
    CSSCoord increment = lineScrollAmount * kScrollMultiplier;

    pageIncrement =
        std::max(visualViewportLength - increment, pageIncrementMin);
  } else {
    pageIncrement = pageIncrementMin;
  }

  const CSSCoord maxMinPosDifference =
      CSSCoord(aAxis.GetRectLength(mMetrics.GetScrollableRect()).Truncated()) -
      visualViewportLength;

  float ratio = pageIncrement / (maxMinPosDifference + pageIncrement);

  CSSCoord desiredThumbLength{
      std::max(mScrollbarData.mThumbMinLength.Rounded(),
               (mScrollbarData.mScrollTrackLength * ratio).Rounded())};

  const float scale = desiredThumbLength / mScrollbarData.mThumbLength;

  // Subtracting the offset of the scrollable rect is needed for right-to-left
  // pages.
  const CSSCoord curPos = aAxis.GetRectOffset(visualViewportRect) -
                          aAxis.GetRectOffset(mMetrics.GetScrollableRect());

  const float thumbPosRatio =
      (maxMinPosDifference != 0)
          ? float((mScrollbarData.mScrollTrackLength - desiredThumbLength) /
                  maxMinPosDifference)
          : 1.f;

  const CSSCoord desiredThumbPos = curPos * thumbPosRatio;

  // When scaling the thumb to account for the async zoom, keep the position
  // of the start of the thumb (which corresponds to the scroll offset)
  // constant.
  ScaleThumbBy(aAxis, scale, ScrollThumbExtent::Start);

  // If the page is overscrolled, additionally squish the thumb in accordance
  // with the overscroll amount.
  if (overscroll != 0) {
    float overscrollScale =
        1.0f - (std::abs(overscroll.value) /
                aAxis.GetRectLength(mMetrics.GetCompositionBounds()));
    MOZ_ASSERT(overscrollScale > 0.0f && overscrollScale <= 1.0f);
    // If we're overscrolled at the top, keep the top of the thumb in place
    // as we squish it. If we're overscrolled at the bottom, keep the bottom of
    // the thumb in place.
    ScaleThumbBy(
        aAxis, overscrollScale,
        overscroll < 0 ? ScrollThumbExtent::Start : ScrollThumbExtent::End);
  }

  aAxis.PostTranslate(mScrollbarTransform,
                      (desiredThumbPos - mScrollbarData.mThumbStart) *
                          mMetrics.GetDevPixelsPerCSSPixel() *
                          LayoutDeviceToParentLayerScale(1.0));
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
  }
  transform = transform * compensation;

  return transform;
}

LayerToParentLayerMatrix4x4 ComputeTransformForScrollThumb(
    const LayerToParentLayerMatrix4x4& aCurrentTransform,
    const gfx::Matrix4x4& aScrollableContentTransform,
    AsyncPanZoomController* aApzc, const FrameMetrics& aMetrics,
    const ScrollbarData& aScrollbarData, bool aScrollbarIsDescendant) {
  return AsyncScrollThumbTransformer{
      aCurrentTransform, aScrollableContentTransform, aApzc, aMetrics,
      aScrollbarData,    aScrollbarIsDescendant}
      .ComputeTransform();
}

}  // namespace apz
}  // namespace layers
}  // namespace mozilla
