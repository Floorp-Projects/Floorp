/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SampledAPZCState.h"

namespace mozilla {
namespace layers {

SampledAPZCState::SampledAPZCState() {}

SampledAPZCState::SampledAPZCState(const FrameMetrics& aMetrics)
    : mLayoutViewport(aMetrics.GetLayoutViewport()),
      mScrollOffset(aMetrics.GetScrollOffset()),
      mZoom(aMetrics.GetZoom()) {}

SampledAPZCState::SampledAPZCState(const FrameMetrics& aMetrics,
                                   Maybe<CompositionPayload>&& aPayload)
    : mLayoutViewport(aMetrics.GetLayoutViewport()),
      mScrollOffset(aMetrics.GetScrollOffset()),
      mZoom(aMetrics.GetZoom()),
      mScrollPayload(std::move(aPayload)) {}

bool SampledAPZCState::operator==(const SampledAPZCState& aOther) const {
  // The payload doesn't factor into equality, that just comes along for
  // the ride.
  return mLayoutViewport.IsEqualEdges(aOther.mLayoutViewport) &&
         mScrollOffset == aOther.mScrollOffset && mZoom == aOther.mZoom;
}

bool SampledAPZCState::operator!=(const SampledAPZCState& aOther) const {
  return !(*this == aOther);
}

Maybe<CompositionPayload> SampledAPZCState::TakeScrollPayload() {
  return std::move(mScrollPayload);
}

void SampledAPZCState::UpdateScrollProperties(const FrameMetrics& aMetrics) {
  mLayoutViewport = aMetrics.GetLayoutViewport();
  mScrollOffset = aMetrics.GetScrollOffset();
}

void SampledAPZCState::UpdateZoomProperties(const FrameMetrics& aMetrics) {
  mZoom = aMetrics.GetZoom();
}

void SampledAPZCState::ClampScrollOffset(const FrameMetrics& aMetrics) {
  mScrollOffset = aMetrics.CalculateScrollRange().ClampPoint(mScrollOffset);
  FrameMetrics::KeepLayoutViewportEnclosingVisualViewport(
      CSSRect(mScrollOffset, aMetrics.CalculateCompositedSizeInCssPixels()),
      aMetrics.GetScrollableRect(), mLayoutViewport);
}

void SampledAPZCState::ZoomBy(const gfxSize& aScale) {
  mZoom.xScale *= aScale.width;
  mZoom.yScale *= aScale.height;
}

}  // namespace layers
}  // namespace mozilla
