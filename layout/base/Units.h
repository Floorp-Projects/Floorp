/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_UNITS_H_
#define MOZ_UNITS_H_

#include "mozilla/gfx/Point.h"
#include "nsDeviceContext.h"

namespace mozilla {

// The pixels that content authors use to specify sizes in.
struct CSSPixel {
  static gfx::PointTyped<CSSPixel> FromAppUnits(const nsPoint &pt) {
    return gfx::PointTyped<CSSPixel>(NSAppUnitsToFloatPixels(pt.x, float(nsDeviceContext::AppUnitsPerCSSPixel())),
                                     NSAppUnitsToFloatPixels(pt.y, float(nsDeviceContext::AppUnitsPerCSSPixel())));
  }

  static nsPoint ToAppUnits(const gfx::PointTyped<CSSPixel> &pt) {
    return nsPoint(NSFloatPixelsToAppUnits(pt.x, float(nsDeviceContext::AppUnitsPerCSSPixel())),
                   NSFloatPixelsToAppUnits(pt.y, float(nsDeviceContext::AppUnitsPerCSSPixel())));
  }
};

typedef gfx::PointTyped<CSSPixel> CSSPoint;

};

#endif
