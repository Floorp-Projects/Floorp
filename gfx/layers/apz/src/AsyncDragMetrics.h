/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_DragMetrics_h
#define mozilla_layers_DragMetrics_h

#include "FrameMetrics.h"

namespace IPC {
template <typename T> struct ParamTraits;
} // namespace IPC

namespace mozilla {

namespace layers {

class AsyncDragMetrics {
  friend struct IPC::ParamTraits<mozilla::layers::AsyncDragMetrics>;

public:
  enum DragDirection {
    NONE,
    VERTICAL,
    HORIZONTAL,
    SENTINEL,
  };

  // IPC constructor
  AsyncDragMetrics()
    : mViewId(0)
    , mPresShellId(0)
    , mDragStartSequenceNumber(0)
    , mScrollbarDragOffset(0)
    , mDirection(NONE)
  {}

  AsyncDragMetrics(const FrameMetrics::ViewID& aViewId,
                   uint32_t aPresShellId,
                   uint64_t aDragStartSequenceNumber,
                   CSSCoord aScrollbarDragOffset,
                   const CSSRect& aScrollTrack,
                   DragDirection aDirection)
    : mViewId(aViewId)
    , mPresShellId(aPresShellId)
    , mDragStartSequenceNumber(aDragStartSequenceNumber)
    , mScrollbarDragOffset(aScrollbarDragOffset)
    , mScrollTrack(aScrollTrack)
    , mDirection(aDirection)
  {}

  FrameMetrics::ViewID mViewId;
  uint32_t mPresShellId;
  uint64_t mDragStartSequenceNumber;
  CSSCoord mScrollbarDragOffset;
  CSSRect mScrollTrack;
  DragDirection mDirection;
};

}
}

#endif
