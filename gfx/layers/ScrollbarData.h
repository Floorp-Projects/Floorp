/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_ScrollbarData_h
#define mozilla_gfx_layers_ScrollbarData_h

#include "FrameMetrics.h"
#include "mozilla/Maybe.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/ScrollableLayerGuid.h"

namespace IPC {
template <typename T>
struct ParamTraits;
}  // namespace IPC

namespace mozilla {
namespace layers {

// clang-format off
MOZ_DEFINE_ENUM_CLASS_WITH_BASE(ScrollbarLayerType, uint8_t, (
  None,
  Thumb,
  Container
));
// clang-format on

/**
 *  It stores data for scroll thumb layer or container layers.
 */
struct ScrollbarData {
 private:
  /**
   * This constructor is for Thumb layer type.
   */
  ScrollbarData(ScrollDirection aDirection, float aThumbRatio,
                OuterCSSCoord aThumbStart, OuterCSSCoord aThumbLength,
                OuterCSSCoord aThumbMinLength, bool aThumbIsAsyncDraggable,
                OuterCSSCoord aScrollTrackStart,
                OuterCSSCoord aScrollTrackLength, uint64_t aTargetViewId)
      : mDirection(Some(aDirection)),
        mScrollbarLayerType(ScrollbarLayerType::Thumb),
        mThumbRatio(aThumbRatio),
        mThumbStart(aThumbStart),
        mThumbLength(aThumbLength),
        mThumbMinLength(aThumbMinLength),
        mThumbIsAsyncDraggable(aThumbIsAsyncDraggable),
        mScrollTrackStart(aScrollTrackStart),
        mScrollTrackLength(aScrollTrackLength),
        mTargetViewId(aTargetViewId) {}

  /**
   * This constructor is for Container layer type.
   */
  ScrollbarData(const Maybe<ScrollDirection>& aDirection,
                uint64_t aTargetViewId)
      : mDirection(aDirection),
        mScrollbarLayerType(ScrollbarLayerType::Container),
        mTargetViewId(aTargetViewId) {}

 public:
  ScrollbarData() = default;

  static ScrollbarData CreateForThumb(
      ScrollDirection aDirection, float aThumbRatio, OuterCSSCoord aThumbStart,
      OuterCSSCoord aThumbLength, OuterCSSCoord aThumbMinLength,
      bool aThumbIsAsyncDraggable, OuterCSSCoord aScrollTrackStart,
      OuterCSSCoord aScrollTrackLength, uint64_t aTargetViewId) {
    return ScrollbarData(aDirection, aThumbRatio, aThumbStart, aThumbLength,
                         aThumbMinLength, aThumbIsAsyncDraggable,
                         aScrollTrackStart, aScrollTrackLength, aTargetViewId);
  }

  static ScrollbarData CreateForScrollbarContainer(
      const Maybe<ScrollDirection>& aDirection, uint64_t aTargetViewId) {
    return ScrollbarData(aDirection, aTargetViewId);
  }

  /**
   * The mDirection contains a direction if mScrollbarLayerType is Thumb
   * or Container, otherwise it's empty.
   */
  Maybe<ScrollDirection> mDirection;

  /**
   * Indicate what kind of layer this data is for. All possibilities are defined
   * in enum ScrollbarLayerType
   */
  ScrollbarLayerType mScrollbarLayerType = ScrollbarLayerType::None;

  /**
   * The scrollbar thumb ratio is the ratio of the thumb position (in the CSS
   * pixels of the scrollframe's parent's space) to the scroll position (in the
   * CSS pixels of the scrollframe's space).
   */
  float mThumbRatio = 0.0f;

  OuterCSSCoord mThumbStart;
  OuterCSSCoord mThumbLength;
  OuterCSSCoord mThumbMinLength;

  /**
   * Whether the scrollbar thumb can be dragged asynchronously.
   */
  bool mThumbIsAsyncDraggable = false;

  OuterCSSCoord mScrollTrackStart;
  OuterCSSCoord mScrollTrackLength;
  uint64_t mTargetViewId = ScrollableLayerGuid::NULL_SCROLL_ID;

  bool operator==(const ScrollbarData& aOther) const {
    return mDirection == aOther.mDirection &&
           mScrollbarLayerType == aOther.mScrollbarLayerType &&
           mThumbRatio == aOther.mThumbRatio &&
           mThumbStart == aOther.mThumbStart &&
           mThumbLength == aOther.mThumbLength &&
           mThumbMinLength == aOther.mThumbMinLength &&
           mThumbIsAsyncDraggable == aOther.mThumbIsAsyncDraggable &&
           mScrollTrackStart == aOther.mScrollTrackStart &&
           mScrollTrackLength == aOther.mScrollTrackLength &&
           mTargetViewId == aOther.mTargetViewId;
  }
  bool operator!=(const ScrollbarData& aOther) const {
    return !(*this == aOther);
  }

  bool IsThumb() const {
    return mScrollbarLayerType == ScrollbarLayerType::Thumb;
  }
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_gfx_layers_ScrollbarData_h
