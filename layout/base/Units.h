/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_UNITS_H_
#define MOZ_UNITS_H_

#include "mozilla/gfx/Point.h"
#include "nsDeviceContext.h"

namespace mozilla {

/*
 * The pixels that content authors use to specify sizes in.
 */
struct CSSPixel {
  static gfx::IntPointTyped<CSSPixel> RoundToInt(const gfx::PointTyped<CSSPixel>& aPoint) {
    return gfx::IntPointTyped<CSSPixel>(NS_lround(aPoint.x),
                                        NS_lround(aPoint.y));
  }

  static gfx::PointTyped<CSSPixel> FromAppUnits(const nsPoint& aPoint) {
    return gfx::PointTyped<CSSPixel>(NSAppUnitsToFloatPixels(aPoint.x, float(nsDeviceContext::AppUnitsPerCSSPixel())),
                                     NSAppUnitsToFloatPixels(aPoint.y, float(nsDeviceContext::AppUnitsPerCSSPixel())));
  }

  static gfx::IntPointTyped<CSSPixel> FromAppUnitsRounded(const nsPoint& aPoint) {
    return gfx::IntPointTyped<CSSPixel>(NSAppUnitsToIntPixels(aPoint.x, float(nsDeviceContext::AppUnitsPerCSSPixel())),
                                        NSAppUnitsToIntPixels(aPoint.y, float(nsDeviceContext::AppUnitsPerCSSPixel())));
  }

  static nsPoint ToAppUnits(const gfx::PointTyped<CSSPixel>& aPoint) {
    return nsPoint(NSFloatPixelsToAppUnits(aPoint.x, float(nsDeviceContext::AppUnitsPerCSSPixel())),
                   NSFloatPixelsToAppUnits(aPoint.y, float(nsDeviceContext::AppUnitsPerCSSPixel())));
  }

  static nsPoint ToAppUnits(const gfx::IntPointTyped<CSSPixel>& aPoint)
  {
    return nsPoint(NSIntPixelsToAppUnits(aPoint.x, nsDeviceContext::AppUnitsPerCSSPixel()),
                   NSIntPixelsToAppUnits(aPoint.y, nsDeviceContext::AppUnitsPerCSSPixel()));
  }

  static gfx::RectTyped<CSSPixel> FromAppUnits(const nsRect& aRect) {
    return gfx::RectTyped<CSSPixel>(NSAppUnitsToFloatPixels(aRect.x, float(nsDeviceContext::AppUnitsPerCSSPixel())),
                                    NSAppUnitsToFloatPixels(aRect.y, float(nsDeviceContext::AppUnitsPerCSSPixel())),
                                    NSAppUnitsToFloatPixels(aRect.width, float(nsDeviceContext::AppUnitsPerCSSPixel())),
                                    NSAppUnitsToFloatPixels(aRect.height, float(nsDeviceContext::AppUnitsPerCSSPixel())));
  }

  static nsRect ToAppUnits(const gfx::RectTyped<CSSPixel>& aRect) {
    return nsRect(NSFloatPixelsToAppUnits(aRect.x, float(nsDeviceContext::AppUnitsPerCSSPixel())),
                  NSFloatPixelsToAppUnits(aRect.y, float(nsDeviceContext::AppUnitsPerCSSPixel())),
                  NSFloatPixelsToAppUnits(aRect.width, float(nsDeviceContext::AppUnitsPerCSSPixel())),
                  NSFloatPixelsToAppUnits(aRect.height, float(nsDeviceContext::AppUnitsPerCSSPixel())));
  }

  static gfx::IntRectTyped<CSSPixel> FromAppUnitsRounded(const nsRect& aRect)
  {
    return gfx::IntRectTyped<CSSPixel>(NSAppUnitsToIntPixels(aRect.x, float(nsDeviceContext::AppUnitsPerCSSPixel())),
                                       NSAppUnitsToIntPixels(aRect.y, float(nsDeviceContext::AppUnitsPerCSSPixel())),
                                       NSAppUnitsToIntPixels(aRect.width, float(nsDeviceContext::AppUnitsPerCSSPixel())),
                                       NSAppUnitsToIntPixels(aRect.height, float(nsDeviceContext::AppUnitsPerCSSPixel())));
  }
};

typedef gfx::PointTyped<CSSPixel> CSSPoint;
typedef gfx::IntPointTyped<CSSPixel> CSSIntPoint;
typedef gfx::SizeTyped<CSSPixel> CSSSize;
typedef gfx::RectTyped<CSSPixel> CSSRect;
typedef gfx::IntRectTyped<CSSPixel> CSSIntRect;

/*
 * The pixels that layout rasterizes and delivers to the graphics code.
 * These are generally referred to as "device pixels" in layout code. Layer
 * pixels are affected by:
 * 1) the "display resolution" (see nsIPresShell::SetResolution)
 * 2) the "full zoom" (see nsPresContext::SetFullZoom)
 * 3) the "widget scale" (nsIWidget::GetDefaultScale)
 */
struct LayerPixel {
  static gfx::IntPointTyped<LayerPixel> FromCSSPointRounded(const CSSPoint& aPoint, float aResolutionX, float aResolutionY) {
    return gfx::IntPointTyped<LayerPixel>(NS_lround(aPoint.x * aResolutionX),
                                          NS_lround(aPoint.y * aResolutionY));
  }

  static gfx::IntRectTyped<LayerPixel> RoundToInt(const gfx::RectTyped<LayerPixel>& aRect) {
    return gfx::IntRectTyped<LayerPixel>(NS_lround(aRect.x),
                                         NS_lround(aRect.y),
                                         NS_lround(aRect.width),
                                         NS_lround(aRect.height));
  }

  static gfx::RectTyped<LayerPixel> FromCSSRect(const CSSRect& aRect, float aResolutionX, float aResolutionY) {
    return gfx::RectTyped<LayerPixel>(aRect.x * aResolutionX,
                                      aRect.y * aResolutionY,
                                      aRect.width * aResolutionX,
                                      aRect.height * aResolutionY);
  }

  static gfx::IntRectTyped<LayerPixel> FromCSSRectRounded(const CSSRect& aRect, float aResolutionX, float aResolutionY) {
    return RoundToInt(FromCSSRect(aRect, aResolutionX, aResolutionY));
  }

  static CSSIntRect ToCSSIntRectRoundIn(const gfx::IntRectTyped<LayerPixel>& aRect, float aResolutionX, float aResolutionY) {
    gfx::IntRectTyped<CSSPixel> ret(aRect.x, aRect.y, aRect.width, aRect.height);
    ret.ScaleInverseRoundIn(aResolutionX, aResolutionY);
    return ret;
  }
};

typedef gfx::PointTyped<LayerPixel> LayerPoint;
typedef gfx::IntPointTyped<LayerPixel> LayerIntPoint;
typedef gfx::IntSizeTyped<LayerPixel> LayerIntSize;
typedef gfx::RectTyped<LayerPixel> LayerRect;
typedef gfx::IntRectTyped<LayerPixel> LayerIntRect;

/*
 * The pixels that are displayed on the screen.
 * On non-OMTC platforms this should be equivalent to LayerPixel units.
 * On OMTC platforms these may diverge from LayerPixel units temporarily,
 * while an asynchronous zoom is happening, but should eventually converge
 * back to LayerPixel units. Some variables (such as those representing
 * chrome UI element sizes) that are not subject to content zoom should
 * generally be represented in ScreenPixel units.
 */
struct ScreenPixel {
};

typedef gfx::PointTyped<ScreenPixel> ScreenPoint;

};

#endif
