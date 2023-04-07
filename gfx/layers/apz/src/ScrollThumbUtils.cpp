/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollThumbUtils.h"
#include "AsyncPanZoomController.h"
#include "FrameMetrics.h"
#include "UnitTransforms.h"
#include "Units.h"
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

  // Translate the thumb along |aAxis| by |aTranslation| in "scrollbar space"
  // (CSS pixels along the scrollbar track, similar to e.g.
  // |mScrollbarData.mThumbStart|).
  void TranslateThumb(const Axis& aAxis, OuterCSSCoord aTranslation);
};

void AsyncScrollThumbTransformer::TranslateThumb(const Axis& aAxis,
                                                 OuterCSSCoord aTranslation) {
  aAxis.PostTranslate(
      mScrollbarTransform,
      ViewAs<CSSPixel>(aTranslation,
                       PixelCastJustification::CSSPixelsOfSurroundingContent) *
          mMetrics.GetDevPixelsPerCSSPixel() *
          LayoutDeviceToParentLayerScale(1.0));
}

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
  const OuterCSSCoord scrollTrackOrigin =
      aAxis.GetPointOffset(
          mMetrics.CalculateCompositionBoundsInOuterCssPixels().TopLeft()) +
      mScrollbarData.mScrollTrackStart;
  OuterCSSCoord thumbExtent = scrollTrackOrigin + mScrollbarData.mThumbStart;
  if (aExtent == ScrollThumbExtent::End) {
    thumbExtent += mScrollbarData.mThumbLength;
  }
  const OuterCSSCoord thumbExtentScaled = thumbExtent * aScale;
  const OuterCSSCoord thumbExtentDelta = thumbExtentScaled - thumbExtent;

  aAxis.PostScale(mScrollbarTransform, aScale);
  TranslateThumb(aAxis, -thumbExtentDelta);
}

void AsyncScrollThumbTransformer::ApplyTransformForAxis(const Axis& aAxis) {
  ParentLayerCoord asyncScroll = aAxis.GetTransformTranslation(mAsyncTransform);
  const float asyncZoom = aAxis.GetTransformScale(mAsyncTransform);
  const ParentLayerCoord overscroll =
      aAxis.GetPointOffset(mApzc->GetOverscrollAmount());

  bool haveAsyncZoom = !FuzzyEqualsAdditive(asyncZoom, 1.f);
  if (!haveAsyncZoom && mApzc->IsZero(asyncScroll) &&
      mApzc->IsZero(overscroll)) {
    return;
  }

  OuterCSSCoord translation;
  float scale = 1.0;

  bool recalcMode = StaticPrefs::apz_scrollthumb_recalc();
  if (recalcMode) {
    // In this branch (taken when apz.scrollthumb.recalc=true), |translation|
    // and |scale| are computed using the approach implemented in bug 1554795
    // of fully recalculating the desired position and size using the logic
    // that attempts to closely match the main-thread calculation.

    const CSSRect visualViewportRect = mApzc->GetCurrentAsyncVisualViewport(
        AsyncPanZoomController::eForCompositing);
    const CSSCoord visualViewportLength =
        aAxis.GetRectLength(visualViewportRect);

    const CSSCoord maxMinPosDifference =
        CSSCoord(
            aAxis.GetRectLength(mMetrics.GetScrollableRect()).Truncated()) -
        visualViewportLength;

    OuterCSSCoord effectiveThumbLength = mScrollbarData.mThumbLength;

    if (haveAsyncZoom) {
      // The calculations here closely follow the main thread calculations at
      // https://searchfox.org/mozilla-central/rev/0bf957f909ae1f3d19b43fd4edfc277342554836/layout/generic/nsGfxScrollFrame.cpp#6902-6927
      // and
      // https://searchfox.org/mozilla-central/rev/0bf957f909ae1f3d19b43fd4edfc277342554836/layout/xul/nsSliderFrame.cpp#587-614
      // Any modifications there should be reflected here as well.
      const CSSCoord pageIncrementMin =
          static_cast<int>(visualViewportLength * 0.8);
      CSSCoord pageIncrement;

      CSSToLayoutDeviceScale deviceScale = mMetrics.GetDevPixelsPerCSSPixel();
      if (*mScrollbarData.mDirection == ScrollDirection::eVertical) {
        const CSSCoord lineScrollAmount =
            (mApzc->GetScrollMetadata().GetLineScrollAmount() / deviceScale)
                .height;
        const double kScrollMultiplier =
            StaticPrefs::toolkit_scrollbox_verticalScrollDistance();
        CSSCoord increment = lineScrollAmount * kScrollMultiplier;

        pageIncrement =
            std::max(visualViewportLength - increment, pageIncrementMin);
      } else {
        pageIncrement = pageIncrementMin;
      }

      float ratio = pageIncrement / (maxMinPosDifference + pageIncrement);

      OuterCSSCoord desiredThumbLength{
          std::max(mScrollbarData.mThumbMinLength,
                   mScrollbarData.mScrollTrackLength * ratio)};

      // Round the thumb length to an integer number of LayoutDevice pixels, to
      // match the main-thread behaviour.
      auto outerDeviceScale = ViewAs<OuterCSSToLayoutDeviceScale>(
          deviceScale, PixelCastJustification::CSSPixelsOfSurroundingContent);
      desiredThumbLength =
          LayoutDeviceCoord((desiredThumbLength * outerDeviceScale).Rounded()) /
          outerDeviceScale;

      effectiveThumbLength = desiredThumbLength;

      scale = desiredThumbLength / mScrollbarData.mThumbLength;
    }

    // Subtracting the offset of the scrollable rect is needed for right-to-left
    // pages.
    const CSSCoord curPos = aAxis.GetRectOffset(visualViewportRect) -
                            aAxis.GetRectOffset(mMetrics.GetScrollableRect());

    const CSSToOuterCSSScale thumbPosRatio(
        (maxMinPosDifference != 0)
            ? float((mScrollbarData.mScrollTrackLength - effectiveThumbLength) /
                    maxMinPosDifference)
            : 1.f);

    const OuterCSSCoord desiredThumbPos = curPos * thumbPosRatio;

    translation = desiredThumbPos - mScrollbarData.mThumbStart;
  } else {
    // In this branch (taken when apz.scrollthumb.recalc=false), |translation|
    // and |scale| are computed using the pre-bug1554795 approach of turning
    // the async scroll and zoom deltas into transforms to apply to the
    // main-thread thumb position and size.

    // The scroll thumb needs to be scaled in the direction of scrolling by the
    // inverse of the async zoom. This is because zooming in decreases the
    // fraction of the whole srollable rect that is in view.
    scale = 1.f / asyncZoom;

    // Note: |metrics.GetZoom()| doesn't yet include the async zoom.
    CSSToParentLayerScale effectiveZoom =
        CSSToParentLayerScale(mMetrics.GetZoom().scale * asyncZoom);

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

      asyncScroll -= aAxis.GetPointOffset((mMetrics.GetLayoutScrollOffset() -
                                           mMetrics.GetVisualScrollOffset()) *
                                          effectiveZoom);
    }

    // Here we convert the scrollbar thumb ratio into a true unitless ratio by
    // dividing out the conversion factor from the scrollframe's parent's space
    // to the scrollframe's space.
    float unitlessThumbRatio = mScrollbarData.mThumbRatio /
                               (mMetrics.GetPresShellResolution() * asyncZoom);

    // The scroll thumb needs to be translated in opposite direction of the
    // async scroll. This is because scrolling down, which translates the layer
    // content up, should result in moving the scroll thumb down.
    ParentLayerCoord translationPL = -asyncScroll * unitlessThumbRatio;

    // The translation we computed is in the scroll frame's ParentLayer space.
    // This includes the full cumulative resolution, even if we are a subframe.
    // However, the resulting transform is used in a context where the scrollbar
    // is already subject to the resolutions of enclosing scroll frames. To
    // avoid double application of these enclosing resolutions, divide them out,
    // leaving only the local resolution if any.
    translationPL /= (mMetrics.GetCumulativeResolution().scale /
                      mMetrics.GetPresShellResolution());

    // Convert translation to CSS pixels as this is what TranslateThumb expects.
    translation = ViewAs<OuterCSSPixel>(
        translationPL / (mMetrics.GetDevPixelsPerCSSPixel() *
                         LayoutDeviceToParentLayerScale(1.0)),
        PixelCastJustification::CSSPixelsOfSurroundingContent);
  }

  // When scaling the thumb to account for the async zoom, keep the position
  // of the start of the thumb (which corresponds to the scroll offset)
  // constant.
  if (haveAsyncZoom) {
    ScaleThumbBy(aAxis, scale, ScrollThumbExtent::Start);
  }

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

  TranslateThumb(aAxis, translation);
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
