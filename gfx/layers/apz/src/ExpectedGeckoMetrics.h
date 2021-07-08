/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ExpectedGeckoMetrics_h
#define mozilla_layers_ExpectedGeckoMetrics_h

#include "Units.h"

namespace mozilla {
namespace layers {

struct FrameMetrics;

// A class that stores a subset of the FrameMetrics information
// than an APZC instance expects Gecko to have (either the
// metrics that were most recently sent to Gecko, or the ones
// most recently received from Gecko).
class ExpectedGeckoMetrics {
 public:
  ExpectedGeckoMetrics() = default;
  void UpdateFrom(const FrameMetrics& aMetrics);
  void UpdateZoomFrom(const FrameMetrics& aMetrics);

  const CSSPoint& GetVisualScrollOffset() const { return mVisualScrollOffset; }
  const CSSToParentLayerScale2D& GetZoom() const { return mZoom; }
  const CSSToLayoutDeviceScale& GetDevPixelsPerCSSPixel() const {
    return mDevPixelsPerCSSPixel;
  }

 private:
  CSSPoint mVisualScrollOffset;
  CSSToParentLayerScale2D mZoom;
  CSSToLayoutDeviceScale mDevPixelsPerCSSPixel;
};

}  // namespace layers
}  // namespace mozilla

#endif
