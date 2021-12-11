/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExpectedGeckoMetrics.h"
#include "FrameMetrics.h"

namespace mozilla {
namespace layers {

void ExpectedGeckoMetrics::UpdateFrom(const FrameMetrics& aMetrics) {
  mVisualScrollOffset = aMetrics.GetVisualScrollOffset();
  mZoom = aMetrics.GetZoom();
  mDevPixelsPerCSSPixel = aMetrics.GetDevPixelsPerCSSPixel();
}

void ExpectedGeckoMetrics::UpdateZoomFrom(const FrameMetrics& aMetrics) {
  mZoom = aMetrics.GetZoom();
  mDevPixelsPerCSSPixel = aMetrics.GetDevPixelsPerCSSPixel();
}

}  // namespace layers
}  // namespace mozilla
