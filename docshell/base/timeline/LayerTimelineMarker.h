/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LayerTimelineMarker_h_
#define mozilla_LayerTimelineMarker_h_

#include "TimelineMarker.h"
#include "mozilla/dom/ProfileTimelineMarkerBinding.h"
#include "mozilla/mozalloc_oom.h"
#include "nsRegion.h"

namespace mozilla {

class LayerTimelineMarker : public TimelineMarker {
 public:
  explicit LayerTimelineMarker(const nsIntRegion& aRegion)
      : TimelineMarker("Layer", MarkerTracingType::HELPER_EVENT),
        mRegion(aRegion) {}

  void AddLayerRectangles(
      dom::Sequence<dom::ProfileTimelineLayerRect>& aRectangles) {
    for (auto iter = mRegion.RectIter(); !iter.Done(); iter.Next()) {
      const nsIntRect& iterRect = iter.Get();
      dom::ProfileTimelineLayerRect rect;
      rect.mX = iterRect.X();
      rect.mY = iterRect.Y();
      rect.mWidth = iterRect.Width();
      rect.mHeight = iterRect.Height();
      if (!aRectangles.AppendElement(rect, fallible)) {
        // XXX(Bug 1632090) Instead of extending the array 1-by-1 (which might
        // involve multiple reallocations) and potentially crashing here,
        // SetCapacity could be called outside the loop once.
        mozalloc_handle_oom(0);
      }
    }
  }

 private:
  nsIntRegion mRegion;
};

}  // namespace mozilla

#endif  // mozilla_LayerTimelineMarker_h_
