/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_SampledAPZCState_h
#define mozilla_layers_SampledAPZCState_h

#include "FrameMetrics.h"
#include "mozilla/Maybe.h"

namespace mozilla {
namespace layers {

class SampledAPZCState {
 public:
  SampledAPZCState();
  explicit SampledAPZCState(const FrameMetrics& aMetrics);
  explicit SampledAPZCState(const FrameMetrics& aMetrics,
                            Maybe<CompositionPayload>&& aPayload);

  bool operator==(const SampledAPZCState& aOther) const;
  bool operator!=(const SampledAPZCState& aOther) const;

  CSSRect GetLayoutViewport() const { return mLayoutViewport; }
  CSSPoint GetScrollOffset() const { return mScrollOffset; }
  CSSToParentLayerScale2D GetZoom() const { return mZoom; }
  Maybe<CompositionPayload> TakeScrollPayload();

  void UpdateScrollProperties(const FrameMetrics& aMetrics);
  void UpdateZoomProperties(const FrameMetrics& aMetrics);

  /**
   * Re-clamp mScrollOffset to the scroll range specified by the provided
   * metrics. This only needs to be called if the scroll offset changes
   * outside of AsyncPanZoomController::SampleCompositedAsyncTransform().
   * It also recalculates mLayoutViewport so that it continues to enclose
   * the visual viewport. This only needs to be called if the
   * layout viewport changes outside of SampleCompositedAsyncTransform().
   */
  void ClampScrollOffset(const FrameMetrics& aMetrics);

  void ZoomBy(const gfxSize& aScale);

 private:
  // These variables cache the layout viewport, scroll offset, and zoom stored
  // in |Metrics()| at the time this class was constructed.
  CSSRect mLayoutViewport;
  CSSPoint mScrollOffset;
  CSSToParentLayerScale2D mZoom;
  // An optional payload that rides along with the sampled state.
  Maybe<CompositionPayload> mScrollPayload;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_SampledAPZCState_h
