/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_UNITS_H_
#define MOZ_UNITS_H_

#include "mozilla/gfx/Coord.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/ScaleFactor.h"
#include "nsRect.h"
#include "nsMargin.h"
#include "mozilla/AppUnits.h"
#include "mozilla/TypeTraits.h"

namespace mozilla {

template <typename T>
struct IsPixel : FalseType {};

// See struct decleration for a description of each unit type
struct CSSPixel;
struct LayoutDevicePixel;
struct LayerPixel;
struct RenderTargetPixel;
struct ScreenPixel;

template<> struct IsPixel<CSSPixel>          : TrueType {};
template<> struct IsPixel<LayoutDevicePixel> : TrueType {};
template<> struct IsPixel<LayerPixel>        : TrueType {};
template<> struct IsPixel<RenderTargetPixel> : TrueType {};
template<> struct IsPixel<ScreenPixel>       : TrueType {};

typedef gfx::CoordTyped<CSSPixel> CSSCoord;
typedef gfx::IntCoordTyped<CSSPixel> CSSIntCoord;
typedef gfx::PointTyped<CSSPixel> CSSPoint;
typedef gfx::IntPointTyped<CSSPixel> CSSIntPoint;
typedef gfx::SizeTyped<CSSPixel> CSSSize;
typedef gfx::IntSizeTyped<CSSPixel> CSSIntSize;
typedef gfx::RectTyped<CSSPixel> CSSRect;
typedef gfx::IntRectTyped<CSSPixel> CSSIntRect;
typedef gfx::MarginTyped<CSSPixel> CSSMargin;
typedef gfx::IntMarginTyped<CSSPixel> CSSIntMargin;

typedef gfx::CoordTyped<LayoutDevicePixel> LayoutDeviceCoord;
typedef gfx::IntCoordTyped<LayoutDevicePixel> LayoutDeviceIntCoord;
typedef gfx::PointTyped<LayoutDevicePixel> LayoutDevicePoint;
typedef gfx::IntPointTyped<LayoutDevicePixel> LayoutDeviceIntPoint;
typedef gfx::SizeTyped<LayoutDevicePixel> LayoutDeviceSize;
typedef gfx::IntSizeTyped<LayoutDevicePixel> LayoutDeviceIntSize;
typedef gfx::RectTyped<LayoutDevicePixel> LayoutDeviceRect;
typedef gfx::IntRectTyped<LayoutDevicePixel> LayoutDeviceIntRect;
typedef gfx::MarginTyped<LayoutDevicePixel> LayoutDeviceMargin;
typedef gfx::IntMarginTyped<LayoutDevicePixel> LayoutDeviceIntMargin;

typedef gfx::CoordTyped<LayerPixel> LayerCoord;
typedef gfx::IntCoordTyped<LayerPixel> LayerIntCoord;
typedef gfx::PointTyped<LayerPixel> LayerPoint;
typedef gfx::IntPointTyped<LayerPixel> LayerIntPoint;
typedef gfx::SizeTyped<LayerPixel> LayerSize;
typedef gfx::IntSizeTyped<LayerPixel> LayerIntSize;
typedef gfx::RectTyped<LayerPixel> LayerRect;
typedef gfx::IntRectTyped<LayerPixel> LayerIntRect;
typedef gfx::MarginTyped<LayerPixel> LayerMargin;
typedef gfx::IntMarginTyped<LayerPixel> LayerIntMargin;

typedef gfx::CoordTyped<ScreenPixel> ScreenCoord;
typedef gfx::IntCoordTyped<ScreenPixel> ScreenIntCoord;
typedef gfx::PointTyped<RenderTargetPixel> RenderTargetPoint;
typedef gfx::IntPointTyped<RenderTargetPixel> RenderTargetIntPoint;
typedef gfx::SizeTyped<RenderTargetPixel> RenderTargetSize;
typedef gfx::IntSizeTyped<RenderTargetPixel> RenderTargetIntSize;
typedef gfx::RectTyped<RenderTargetPixel> RenderTargetRect;
typedef gfx::IntRectTyped<RenderTargetPixel> RenderTargetIntRect;
typedef gfx::MarginTyped<RenderTargetPixel> RenderTargetMargin;
typedef gfx::IntMarginTyped<RenderTargetPixel> RenderTargetIntMargin;

typedef gfx::PointTyped<ScreenPixel> ScreenPoint;
typedef gfx::IntPointTyped<ScreenPixel> ScreenIntPoint;
typedef gfx::SizeTyped<ScreenPixel> ScreenSize;
typedef gfx::IntSizeTyped<ScreenPixel> ScreenIntSize;
typedef gfx::RectTyped<ScreenPixel> ScreenRect;
typedef gfx::IntRectTyped<ScreenPixel> ScreenIntRect;
typedef gfx::MarginTyped<ScreenPixel> ScreenMargin;
typedef gfx::IntMarginTyped<ScreenPixel> ScreenIntMargin;

typedef gfx::ScaleFactor<CSSPixel, LayoutDevicePixel> CSSToLayoutDeviceScale;
typedef gfx::ScaleFactor<CSSPixel, LayerPixel> CSSToLayerScale;
typedef gfx::ScaleFactor<CSSPixel, ScreenPixel> CSSToScreenScale;
typedef gfx::ScaleFactor<LayoutDevicePixel, CSSPixel> LayoutDeviceToCSSScale;
typedef gfx::ScaleFactor<LayoutDevicePixel, LayerPixel> LayoutDeviceToLayerScale;
typedef gfx::ScaleFactor<LayoutDevicePixel, ScreenPixel> LayoutDeviceToScreenScale;
typedef gfx::ScaleFactor<LayerPixel, CSSPixel> LayerToCSSScale;
typedef gfx::ScaleFactor<LayerPixel, LayoutDevicePixel> LayerToLayoutDeviceScale;
typedef gfx::ScaleFactor<LayerPixel, RenderTargetPixel> LayerToRenderTargetScale;
typedef gfx::ScaleFactor<LayerPixel, ScreenPixel> LayerToScreenScale;
typedef gfx::ScaleFactor<RenderTargetPixel, ScreenPixel> RenderTargetToScreenScale;
typedef gfx::ScaleFactor<ScreenPixel, CSSPixel> ScreenToCSSScale;
typedef gfx::ScaleFactor<ScreenPixel, LayoutDevicePixel> ScreenToLayoutDeviceScale;
typedef gfx::ScaleFactor<ScreenPixel, LayerPixel> ScreenToLayerScale;

/*
 * The pixels that content authors use to specify sizes in.
 */
struct CSSPixel {

  // Conversions from app units

  static CSSPoint FromAppUnits(const nsPoint& aPoint) {
    return CSSPoint(NSAppUnitsToFloatPixels(aPoint.x, float(AppUnitsPerCSSPixel())),
                    NSAppUnitsToFloatPixels(aPoint.y, float(AppUnitsPerCSSPixel())));
  }

  static CSSRect FromAppUnits(const nsRect& aRect) {
    return CSSRect(NSAppUnitsToFloatPixels(aRect.x, float(AppUnitsPerCSSPixel())),
                   NSAppUnitsToFloatPixels(aRect.y, float(AppUnitsPerCSSPixel())),
                   NSAppUnitsToFloatPixels(aRect.width, float(AppUnitsPerCSSPixel())),
                   NSAppUnitsToFloatPixels(aRect.height, float(AppUnitsPerCSSPixel())));
  }

  static CSSMargin FromAppUnits(const nsMargin& aMargin) {
    return CSSMargin(NSAppUnitsToFloatPixels(aMargin.top, float(AppUnitsPerCSSPixel())),
                     NSAppUnitsToFloatPixels(aMargin.right, float(AppUnitsPerCSSPixel())),
                     NSAppUnitsToFloatPixels(aMargin.bottom, float(AppUnitsPerCSSPixel())),
                     NSAppUnitsToFloatPixels(aMargin.left, float(AppUnitsPerCSSPixel())));
  }

  static CSSIntPoint FromAppUnitsRounded(const nsPoint& aPoint) {
    return CSSIntPoint(NSAppUnitsToIntPixels(aPoint.x, float(AppUnitsPerCSSPixel())),
                       NSAppUnitsToIntPixels(aPoint.y, float(AppUnitsPerCSSPixel())));
  }

  static CSSIntSize FromAppUnitsRounded(const nsSize& aSize)
  {
    return CSSIntSize(NSAppUnitsToIntPixels(aSize.width, float(AppUnitsPerCSSPixel())),
                      NSAppUnitsToIntPixels(aSize.height, float(AppUnitsPerCSSPixel())));
  }

  static CSSIntRect FromAppUnitsRounded(const nsRect& aRect) {
    return CSSIntRect(NSAppUnitsToIntPixels(aRect.x, float(AppUnitsPerCSSPixel())),
                      NSAppUnitsToIntPixels(aRect.y, float(AppUnitsPerCSSPixel())),
                      NSAppUnitsToIntPixels(aRect.width, float(AppUnitsPerCSSPixel())),
                      NSAppUnitsToIntPixels(aRect.height, float(AppUnitsPerCSSPixel())));
  }

  // Conversions to app units

  static nsPoint ToAppUnits(const CSSPoint& aPoint) {
    return nsPoint(NSToCoordRoundWithClamp(aPoint.x * float(AppUnitsPerCSSPixel())),
                   NSToCoordRoundWithClamp(aPoint.y * float(AppUnitsPerCSSPixel())));
  }

  static nsPoint ToAppUnits(const CSSIntPoint& aPoint) {
    return nsPoint(NSToCoordRoundWithClamp(float(aPoint.x) * float(AppUnitsPerCSSPixel())),
                   NSToCoordRoundWithClamp(float(aPoint.y) * float(AppUnitsPerCSSPixel())));
  }

  static nsRect ToAppUnits(const CSSRect& aRect) {
    return nsRect(NSToCoordRoundWithClamp(aRect.x * float(AppUnitsPerCSSPixel())),
                  NSToCoordRoundWithClamp(aRect.y * float(AppUnitsPerCSSPixel())),
                  NSToCoordRoundWithClamp(aRect.width * float(AppUnitsPerCSSPixel())),
                  NSToCoordRoundWithClamp(aRect.height * float(AppUnitsPerCSSPixel())));
  }
};

/*
 * The pixels that are referred to as "device pixels" in layout code. In
 * general this is obtained by converting a value in app units value by the
 * AppUnitsPerDevPixel() value. The size of these pixels
 * are affected by:
 * 1) the "full zoom" (see nsPresContext::SetFullZoom)
 * 2) the "widget scale" (see nsIWidget::GetDefaultScale)
 */
struct LayoutDevicePixel {
  static LayoutDeviceIntPoint FromUntyped(const nsIntPoint& aPoint) {
    return LayoutDeviceIntPoint(aPoint.x, aPoint.y);
  }

  static nsIntPoint ToUntyped(const LayoutDeviceIntPoint& aPoint) {
    return nsIntPoint(aPoint.x, aPoint.y);
  }

  static LayoutDeviceIntRect FromUntyped(const nsIntRect& aRect) {
    return LayoutDeviceIntRect(aRect.x, aRect.y, aRect.width, aRect.height);
  }

  static nsIntRect ToUntyped(const LayoutDeviceIntRect& aRect) {
    return nsIntRect(aRect.x, aRect.y, aRect.width, aRect.height);
  }

  static LayoutDeviceRect FromAppUnits(const nsRect& aRect, nscoord aAppUnitsPerDevPixel) {
    return LayoutDeviceRect(NSAppUnitsToFloatPixels(aRect.x, float(aAppUnitsPerDevPixel)),
                            NSAppUnitsToFloatPixels(aRect.y, float(aAppUnitsPerDevPixel)),
                            NSAppUnitsToFloatPixels(aRect.width, float(aAppUnitsPerDevPixel)),
                            NSAppUnitsToFloatPixels(aRect.height, float(aAppUnitsPerDevPixel)));
  }

  static LayoutDeviceIntPoint FromAppUnitsRounded(const nsPoint& aPoint, nscoord aAppUnitsPerDevPixel) {
    return LayoutDeviceIntPoint(NSAppUnitsToIntPixels(aPoint.x, aAppUnitsPerDevPixel),
                                NSAppUnitsToIntPixels(aPoint.y, aAppUnitsPerDevPixel));
  }

  static LayoutDeviceIntPoint FromAppUnitsToNearest(const nsPoint& aPoint, nscoord aAppUnitsPerDevPixel) {
    return FromUntyped(aPoint.ToNearestPixels(aAppUnitsPerDevPixel));
  }

  static LayoutDeviceIntRect FromAppUnitsToNearest(const nsRect& aRect, nscoord aAppUnitsPerDevPixel) {
    return FromUntyped(aRect.ToNearestPixels(aAppUnitsPerDevPixel));
  }

  static nsSize ToAppUnits(const LayoutDeviceIntSize& aSize, nscoord aAppUnitsPerDevPixel) {
    return nsSize(aSize.width * aAppUnitsPerDevPixel,
                  aSize.height * aAppUnitsPerDevPixel);
  }
};

/*
 * The pixels that layout rasterizes and delivers to the graphics code.
 * These are generally referred to as "device pixels" in layout code. Layer
 * pixels are affected by:
 * 1) the "display resolution" (see nsIPresShell::SetResolution)
 * 2) the "full zoom" (see nsPresContext::SetFullZoom)
 * 3) the "widget scale" (see nsIWidget::GetDefaultScale)
 */
struct LayerPixel {
  static nsIntRect ToUntyped(const LayerIntRect& aRect) {
    return nsIntRect(aRect.x, aRect.y, aRect.width, aRect.height);
  }

  static gfx::IntRect ToUnknown(const LayerIntRect& aRect) {
    return gfx::IntRect(aRect.x, aRect.y, aRect.width, aRect.height);
  }

  static gfx::Rect ToUnknown(const LayerRect& aRect) {
    return gfx::Rect(aRect.x, aRect.y, aRect.width, aRect.height);
  }

  static LayerIntRect FromUntyped(const nsIntRect& aRect) {
    return LayerIntRect(aRect.x, aRect.y, aRect.width, aRect.height);
  }

  static LayerIntPoint FromUntyped(const nsIntPoint& aPoint) {
    return LayerIntPoint(aPoint.x, aPoint.y);
  }
};

/*
 * Layers are always composited to a render target. This unit
 * represents one pixel in the render target. Note that for the
 * root render target RenderTargetPixel == ScreenPixel. Also
 * any ContainerLayer providing an intermediate surface will
 * have RenderTargetPixel == LayerPixel.
 */
struct RenderTargetPixel {
  static nsIntPoint ToUntyped(const RenderTargetIntPoint& aPoint) {
    return nsIntPoint(aPoint.x, aPoint.y);
  }

  static nsIntRect ToUntyped(const RenderTargetIntRect& aRect) {
    return nsIntRect(aRect.x, aRect.y, aRect.width, aRect.height);
  }

  static gfx::IntRect ToUnknown(const RenderTargetIntRect& aRect) {
    return gfx::IntRect(aRect.x, aRect.y, aRect.width, aRect.height);
  }

  static gfx::Rect ToUnknown(const RenderTargetRect& aRect) {
    return gfx::Rect(aRect.x, aRect.y, aRect.width, aRect.height);
  }

  static RenderTargetRect FromUnknown(const gfx::Rect& aRect) {
    return RenderTargetRect(aRect.x, aRect.y, aRect.width, aRect.height);
  }

  static RenderTargetIntRect FromUntyped(const nsIntRect& aRect) {
    return RenderTargetIntRect(aRect.x, aRect.y, aRect.width, aRect.height);
  }
};

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
  static ScreenIntPoint FromUntyped(const nsIntPoint& aPoint) {
    return ScreenIntPoint(aPoint.x, aPoint.y);
  }
};

// Operators to apply ScaleFactors directly to Coords, Points, Rects, Sizes and Margins

template<class src, class dst>
gfx::CoordTyped<dst> operator*(const gfx::CoordTyped<src>& aCoord, const gfx::ScaleFactor<src, dst>& aScale) {
  return gfx::CoordTyped<dst>(aCoord.value * aScale.scale);
}

template<class src, class dst>
gfx::CoordTyped<dst> operator/(const gfx::CoordTyped<src>& aCoord, const gfx::ScaleFactor<dst, src>& aScale) {
  return gfx::CoordTyped<dst>(aCoord.value / aScale.scale);
}

template<class src, class dst>
gfx::PointTyped<dst> operator*(const gfx::PointTyped<src>& aPoint, const gfx::ScaleFactor<src, dst>& aScale) {
  return gfx::PointTyped<dst>(aPoint.x * aScale.scale,
                              aPoint.y * aScale.scale);
}

template<class src, class dst>
gfx::PointTyped<dst> operator/(const gfx::PointTyped<src>& aPoint, const gfx::ScaleFactor<dst, src>& aScale) {
  return gfx::PointTyped<dst>(aPoint.x / aScale.scale,
                              aPoint.y / aScale.scale);
}

template<class src, class dst>
gfx::RectTyped<dst> operator*(const gfx::RectTyped<src>& aRect, const gfx::ScaleFactor<src, dst>& aScale) {
  return gfx::RectTyped<dst>(aRect.x * aScale.scale,
                             aRect.y * aScale.scale,
                             aRect.width * aScale.scale,
                             aRect.height * aScale.scale);
}

template<class src, class dst>
gfx::RectTyped<dst> operator/(const gfx::RectTyped<src>& aRect, const gfx::ScaleFactor<dst, src>& aScale) {
  return gfx::RectTyped<dst>(aRect.x / aScale.scale,
                             aRect.y / aScale.scale,
                             aRect.width / aScale.scale,
                             aRect.height / aScale.scale);
}

template<class src, class dst>
gfx::RectTyped<dst> operator*(const gfx::IntRectTyped<src>& aRect, const gfx::ScaleFactor<src, dst>& aScale) {
  return gfx::RectTyped<dst>(float(aRect.x) * aScale.scale,
                             float(aRect.y) * aScale.scale,
                             float(aRect.width) * aScale.scale,
                             float(aRect.height) * aScale.scale);
}

template<class src, class dst>
gfx::RectTyped<dst> operator/(const gfx::IntRectTyped<src>& aRect, const gfx::ScaleFactor<dst, src>& aScale) {
  return gfx::RectTyped<dst>(float(aRect.x) / aScale.scale,
                             float(aRect.y) / aScale.scale,
                             float(aRect.width) / aScale.scale,
                             float(aRect.height) / aScale.scale);
}

template<class src, class dst>
gfx::SizeTyped<dst> operator*(const gfx::SizeTyped<src>& aSize, const gfx::ScaleFactor<src, dst>& aScale) {
  return gfx::SizeTyped<dst>(aSize.width * aScale.scale,
                             aSize.height * aScale.scale);
}

template<class src, class dst>
gfx::SizeTyped<dst> operator/(const gfx::SizeTyped<src>& aSize, const gfx::ScaleFactor<dst, src>& aScale) {
  return gfx::SizeTyped<dst>(aSize.width / aScale.scale,
                              aSize.height / aScale.scale);
}

template<class src, class dst>
gfx::SizeTyped<dst> operator*(const gfx::IntSizeTyped<src>& aSize, const gfx::ScaleFactor<src, dst>& aScale) {
  return gfx::SizeTyped<dst>(float(aSize.width) * aScale.scale,
                             float(aSize.height) * aScale.scale);
}

template<class src, class dst>
gfx::SizeTyped<dst> operator/(const gfx::IntSizeTyped<src>& aSize, const gfx::ScaleFactor<dst, src>& aScale) {
  return gfx::SizeTyped<dst>(float(aSize.width) / aScale.scale,
                             float(aSize.height) / aScale.scale);
}

template<class src, class dst>
gfx::MarginTyped<dst> operator*(const gfx::MarginTyped<src>& aMargin, const gfx::ScaleFactor<src, dst>& aScale) {
  return gfx::MarginTyped<dst>(aMargin.top * aScale.scale,
                               aMargin.right * aScale.scale,
                               aMargin.bottom * aScale.scale,
                               aMargin.left * aScale.scale);
}

template<class src, class dst>
gfx::MarginTyped<dst> operator/(const gfx::MarginTyped<src>& aMargin, const gfx::ScaleFactor<dst, src>& aScale) {
  return gfx::MarginTyped<dst>(aMargin.top / aScale.scale,
                               aMargin.right / aScale.scale,
                               aMargin.bottom / aScale.scale,
                               aMargin.left / aScale.scale);
}

}

#endif
