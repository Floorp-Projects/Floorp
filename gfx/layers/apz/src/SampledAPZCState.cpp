/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SampledAPZCState.h"
#include "APZUtils.h"

namespace mozilla {
namespace layers {

SampledAPZCState::SampledAPZCState() {}

SampledAPZCState::SampledAPZCState(const FrameMetrics& aMetrics)
    : mLayoutViewport(aMetrics.GetLayoutViewport()),
      mVisualScrollOffset(aMetrics.GetVisualScrollOffset()),
      mZoom(aMetrics.GetZoom()) {
  RemoveFractionalAsyncDelta();
}

SampledAPZCState::SampledAPZCState(const FrameMetrics& aMetrics,
                                   Maybe<CompositionPayload>&& aPayload)
    : mLayoutViewport(aMetrics.GetLayoutViewport()),
      mVisualScrollOffset(aMetrics.GetVisualScrollOffset()),
      mZoom(aMetrics.GetZoom()),
      mScrollPayload(std::move(aPayload)) {
  RemoveFractionalAsyncDelta();
}

bool SampledAPZCState::operator==(const SampledAPZCState& aOther) const {
  // The payload doesn't factor into equality, that just comes along for
  // the ride.
  return mLayoutViewport.IsEqualEdges(aOther.mLayoutViewport) &&
         mVisualScrollOffset == aOther.mVisualScrollOffset &&
         mZoom == aOther.mZoom;
}

bool SampledAPZCState::operator!=(const SampledAPZCState& aOther) const {
  return !(*this == aOther);
}

Maybe<CompositionPayload> SampledAPZCState::TakeScrollPayload() {
  return std::move(mScrollPayload);
}

void SampledAPZCState::UpdateScrollProperties(const FrameMetrics& aMetrics) {
  mLayoutViewport = aMetrics.GetLayoutViewport();
  mVisualScrollOffset = aMetrics.GetVisualScrollOffset();
}

void SampledAPZCState::UpdateZoomProperties(const FrameMetrics& aMetrics) {
  mZoom = aMetrics.GetZoom();
}

void SampledAPZCState::ClampVisualScrollOffset(const FrameMetrics& aMetrics) {
  // Make sure that we use the local mZoom to do these calculations, because the
  // one on aMetrics might be newer.
  CSSRect scrollRange = FrameMetrics::CalculateScrollRange(
      aMetrics.GetScrollableRect(), aMetrics.GetCompositionBounds(), mZoom);
  mVisualScrollOffset = scrollRange.ClampPoint(mVisualScrollOffset);

  FrameMetrics::KeepLayoutViewportEnclosingVisualViewport(
      CSSRect(mVisualScrollOffset,
              FrameMetrics::CalculateCompositedSizeInCssPixels(
                  aMetrics.GetCompositionBounds(), mZoom)),
      aMetrics.GetScrollableRect(), mLayoutViewport);
}

void SampledAPZCState::ZoomBy(const gfxSize& aScale) {
  mZoom.xScale *= aScale.width;
  mZoom.yScale *= aScale.height;
}

void SampledAPZCState::RemoveFractionalAsyncDelta() {
  // This function is a performance hack. With non-WebRender, having small
  // fractional deltas between the layout offset and scroll offset on
  // container layers can trigger the creation of a temporary surface during
  // composition, because it produces a non-integer translation that doesn't
  // play well with layer clips. So we detect the case where the delta is
  // uselessly small (0.01 parentlayer pixels or less) and tweak the sampled
  // scroll offset to eliminate it. By doing this here at sample time rather
  // than elsewhere in the pipeline we are least likely to break assumptions
  // and invariants elsewhere in the code, since sampling effectively takes
  // a snapshot of APZ state (decoupling it from APZ assumptions) and provides
  // it as an input to the compositor (so all compositor state should be
  // internally consistent based on this input).
  if (mLayoutViewport.TopLeft() == mVisualScrollOffset) {
    return;
  }
  ParentLayerPoint paintedOffset = mLayoutViewport.TopLeft() * mZoom;
  ParentLayerPoint asyncOffset = mVisualScrollOffset * mZoom;
  if (FuzzyEqualsAdditive(paintedOffset.x, asyncOffset.x, COORDINATE_EPSILON) &&
      FuzzyEqualsAdditive(paintedOffset.y, asyncOffset.y, COORDINATE_EPSILON)) {
    mVisualScrollOffset = mLayoutViewport.TopLeft();
  }
}

}  // namespace layers
}  // namespace mozilla
