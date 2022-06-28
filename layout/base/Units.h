/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_UNITS_H_
#define MOZ_UNITS_H_

#include <type_traits>

#include "mozilla/gfx/Coord.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/RectAbsolute.h"
#include "mozilla/gfx/ScaleFactor.h"
#include "mozilla/gfx/ScaleFactors2D.h"
#include "nsMargin.h"
#include "nsRect.h"
#include "nsRegion.h"
#include "mozilla/AppUnits.h"

namespace mozilla {

template <typename T>
struct IsPixel : std::false_type {};

// See struct declaration for a description of each unit type.
struct CSSPixel;
struct LayoutDevicePixel;
struct LayerPixel;
struct CSSTransformedLayerPixel;
struct RenderTargetPixel;
struct ScreenPixel;
struct ParentLayerPixel;
struct DesktopPixel;
struct ImagePixel;
struct ExternalPixel;

template <>
struct IsPixel<CSSPixel> : std::true_type {};
template <>
struct IsPixel<LayoutDevicePixel> : std::true_type {};
template <>
struct IsPixel<LayerPixel> : std::true_type {};
template <>
struct IsPixel<CSSTransformedLayerPixel> : std::true_type {};
template <>
struct IsPixel<RenderTargetPixel> : std::true_type {};
template <>
struct IsPixel<ImagePixel> : std::true_type {};
template <>
struct IsPixel<ScreenPixel> : std::true_type {};
template <>
struct IsPixel<ParentLayerPixel> : std::true_type {};
template <>
struct IsPixel<DesktopPixel> : std::true_type {};
template <>
struct IsPixel<ExternalPixel> : std::true_type {};

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
typedef gfx::IntRegionTyped<CSSPixel> CSSIntRegion;

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
typedef gfx::IntRegionTyped<LayoutDevicePixel> LayoutDeviceIntRegion;

typedef gfx::CoordTyped<LayerPixel> LayerCoord;
typedef gfx::IntCoordTyped<LayerPixel> LayerIntCoord;
typedef gfx::PointTyped<LayerPixel> LayerPoint;
typedef gfx::IntPointTyped<LayerPixel> LayerIntPoint;
typedef gfx::SizeTyped<LayerPixel> LayerSize;
typedef gfx::IntSizeTyped<LayerPixel> LayerIntSize;
typedef gfx::RectTyped<LayerPixel> LayerRect;
typedef gfx::RectAbsoluteTyped<LayerPixel> LayerRectAbsolute;
typedef gfx::IntRectTyped<LayerPixel> LayerIntRect;
typedef gfx::MarginTyped<LayerPixel> LayerMargin;
typedef gfx::IntMarginTyped<LayerPixel> LayerIntMargin;
typedef gfx::IntRegionTyped<LayerPixel> LayerIntRegion;

typedef gfx::CoordTyped<CSSTransformedLayerPixel> CSSTransformedLayerCoord;
typedef gfx::IntCoordTyped<CSSTransformedLayerPixel>
    CSSTransformedLayerIntCoord;
typedef gfx::PointTyped<CSSTransformedLayerPixel> CSSTransformedLayerPoint;
typedef gfx::IntPointTyped<CSSTransformedLayerPixel>
    CSSTransformedLayerIntPoint;
typedef gfx::SizeTyped<CSSTransformedLayerPixel> CSSTransformedLayerSize;
typedef gfx::IntSizeTyped<CSSTransformedLayerPixel> CSSTransformedLayerIntSize;
typedef gfx::RectTyped<CSSTransformedLayerPixel> CSSTransformedLayerRect;
typedef gfx::IntRectTyped<CSSTransformedLayerPixel> CSSTransformedLayerIntRect;
typedef gfx::MarginTyped<CSSTransformedLayerPixel> CSSTransformedLayerMargin;
typedef gfx::IntMarginTyped<CSSTransformedLayerPixel>
    CSSTransformedLayerIntMargin;
typedef gfx::IntRegionTyped<CSSTransformedLayerPixel>
    CSSTransformedLayerIntRegion;

typedef gfx::PointTyped<RenderTargetPixel> RenderTargetPoint;
typedef gfx::IntPointTyped<RenderTargetPixel> RenderTargetIntPoint;
typedef gfx::SizeTyped<RenderTargetPixel> RenderTargetSize;
typedef gfx::IntSizeTyped<RenderTargetPixel> RenderTargetIntSize;
typedef gfx::RectTyped<RenderTargetPixel> RenderTargetRect;
typedef gfx::IntRectTyped<RenderTargetPixel> RenderTargetIntRect;
typedef gfx::MarginTyped<RenderTargetPixel> RenderTargetMargin;
typedef gfx::IntMarginTyped<RenderTargetPixel> RenderTargetIntMargin;
typedef gfx::IntRegionTyped<RenderTargetPixel> RenderTargetIntRegion;

typedef gfx::PointTyped<ImagePixel> ImagePoint;
typedef gfx::IntPointTyped<ImagePixel> ImageIntPoint;
typedef gfx::SizeTyped<ImagePixel> ImageSize;
typedef gfx::IntSizeTyped<ImagePixel> ImageIntSize;
typedef gfx::RectTyped<ImagePixel> ImageRect;
typedef gfx::IntRectTyped<ImagePixel> ImageIntRect;

typedef gfx::CoordTyped<ScreenPixel> ScreenCoord;
typedef gfx::IntCoordTyped<ScreenPixel> ScreenIntCoord;
typedef gfx::PointTyped<ScreenPixel> ScreenPoint;
typedef gfx::IntPointTyped<ScreenPixel> ScreenIntPoint;
typedef gfx::SizeTyped<ScreenPixel> ScreenSize;
typedef gfx::IntSizeTyped<ScreenPixel> ScreenIntSize;
typedef gfx::RectTyped<ScreenPixel> ScreenRect;
typedef gfx::IntRectTyped<ScreenPixel> ScreenIntRect;
typedef gfx::MarginTyped<ScreenPixel> ScreenMargin;
typedef gfx::IntMarginTyped<ScreenPixel> ScreenIntMargin;
typedef gfx::IntRegionTyped<ScreenPixel> ScreenIntRegion;

typedef gfx::CoordTyped<ParentLayerPixel> ParentLayerCoord;
typedef gfx::IntCoordTyped<ParentLayerPixel> ParentLayerIntCoord;
typedef gfx::PointTyped<ParentLayerPixel> ParentLayerPoint;
typedef gfx::IntPointTyped<ParentLayerPixel> ParentLayerIntPoint;
typedef gfx::SizeTyped<ParentLayerPixel> ParentLayerSize;
typedef gfx::IntSizeTyped<ParentLayerPixel> ParentLayerIntSize;
typedef gfx::RectTyped<ParentLayerPixel> ParentLayerRect;
typedef gfx::IntRectTyped<ParentLayerPixel> ParentLayerIntRect;
typedef gfx::MarginTyped<ParentLayerPixel> ParentLayerMargin;
typedef gfx::IntMarginTyped<ParentLayerPixel> ParentLayerIntMargin;
typedef gfx::IntRegionTyped<ParentLayerPixel> ParentLayerIntRegion;

typedef gfx::CoordTyped<DesktopPixel> DesktopCoord;
typedef gfx::IntCoordTyped<DesktopPixel> DesktopIntCoord;
typedef gfx::PointTyped<DesktopPixel> DesktopPoint;
typedef gfx::IntPointTyped<DesktopPixel> DesktopIntPoint;
typedef gfx::SizeTyped<DesktopPixel> DesktopSize;
typedef gfx::IntSizeTyped<DesktopPixel> DesktopIntSize;
typedef gfx::RectTyped<DesktopPixel> DesktopRect;
typedef gfx::IntRectTyped<DesktopPixel> DesktopIntRect;

typedef gfx::CoordTyped<ExternalPixel> ExternalCoord;
typedef gfx::IntCoordTyped<ExternalPixel> ExternalIntCoord;
typedef gfx::PointTyped<ExternalPixel> ExternalPoint;
typedef gfx::IntPointTyped<ExternalPixel> ExternalIntPoint;
typedef gfx::SizeTyped<ExternalPixel> ExternalSize;
typedef gfx::IntSizeTyped<ExternalPixel> ExternalIntSize;
typedef gfx::RectTyped<ExternalPixel> ExternalRect;
typedef gfx::IntRectTyped<ExternalPixel> ExternalIntRect;
typedef gfx::MarginTyped<ExternalPixel> ExternalMargin;
typedef gfx::IntMarginTyped<ExternalPixel> ExternalIntMargin;
typedef gfx::IntRegionTyped<ExternalPixel> ExternalIntRegion;

typedef gfx::ScaleFactor<CSSPixel, CSSPixel> CSSToCSSScale;
typedef gfx::ScaleFactor<CSSPixel, LayoutDevicePixel> CSSToLayoutDeviceScale;
typedef gfx::ScaleFactor<CSSPixel, LayerPixel> CSSToLayerScale;
typedef gfx::ScaleFactor<CSSPixel, ScreenPixel> CSSToScreenScale;
typedef gfx::ScaleFactor<CSSPixel, ParentLayerPixel> CSSToParentLayerScale;
typedef gfx::ScaleFactor<CSSPixel, DesktopPixel> CSSToDesktopScale;
typedef gfx::ScaleFactor<LayoutDevicePixel, CSSPixel> LayoutDeviceToCSSScale;
typedef gfx::ScaleFactor<LayoutDevicePixel, LayerPixel>
    LayoutDeviceToLayerScale;
typedef gfx::ScaleFactor<LayoutDevicePixel, ScreenPixel>
    LayoutDeviceToScreenScale;
typedef gfx::ScaleFactor<LayoutDevicePixel, ParentLayerPixel>
    LayoutDeviceToParentLayerScale;
typedef gfx::ScaleFactor<LayerPixel, CSSPixel> LayerToCSSScale;
typedef gfx::ScaleFactor<LayerPixel, LayoutDevicePixel>
    LayerToLayoutDeviceScale;
typedef gfx::ScaleFactor<LayerPixel, RenderTargetPixel>
    LayerToRenderTargetScale;
typedef gfx::ScaleFactor<LayerPixel, ScreenPixel> LayerToScreenScale;
typedef gfx::ScaleFactor<LayerPixel, ParentLayerPixel> LayerToParentLayerScale;
typedef gfx::ScaleFactor<RenderTargetPixel, ScreenPixel>
    RenderTargetToScreenScale;
typedef gfx::ScaleFactor<ScreenPixel, CSSPixel> ScreenToCSSScale;
typedef gfx::ScaleFactor<ScreenPixel, LayoutDevicePixel>
    ScreenToLayoutDeviceScale;
typedef gfx::ScaleFactor<ScreenPixel, LayerPixel> ScreenToLayerScale;
typedef gfx::ScaleFactor<ScreenPixel, ParentLayerPixel>
    ScreenToParentLayerScale;
typedef gfx::ScaleFactor<ParentLayerPixel, LayerPixel> ParentLayerToLayerScale;
typedef gfx::ScaleFactor<ParentLayerPixel, ScreenPixel>
    ParentLayerToScreenScale;
typedef gfx::ScaleFactor<ParentLayerPixel, ParentLayerPixel>
    ParentLayerToParentLayerScale;
typedef gfx::ScaleFactor<DesktopPixel, LayoutDevicePixel>
    DesktopToLayoutDeviceScale;
typedef gfx::ScaleFactor<LayoutDevicePixel, DesktopPixel>
    LayoutDeviceToDesktopScale;

typedef gfx::ScaleFactors2D<CSSPixel, LayoutDevicePixel>
    CSSToLayoutDeviceScale2D;
typedef gfx::ScaleFactors2D<CSSPixel, LayerPixel> CSSToLayerScale2D;
typedef gfx::ScaleFactors2D<CSSPixel, ScreenPixel> CSSToScreenScale2D;
typedef gfx::ScaleFactors2D<CSSPixel, ParentLayerPixel> CSSToParentLayerScale2D;
typedef gfx::ScaleFactors2D<LayoutDevicePixel, CSSPixel>
    LayoutDeviceToCSSScale2D;
typedef gfx::ScaleFactors2D<LayoutDevicePixel, LayerPixel>
    LayoutDeviceToLayerScale2D;
typedef gfx::ScaleFactors2D<LayoutDevicePixel, ScreenPixel>
    LayoutDeviceToScreenScale2D;
typedef gfx::ScaleFactors2D<LayoutDevicePixel, ParentLayerPixel>
    LayoutDeviceToParentLayerScale2D;
typedef gfx::ScaleFactors2D<LayerPixel, CSSPixel> LayerToCSSScale2D;
typedef gfx::ScaleFactors2D<LayerPixel, LayoutDevicePixel>
    LayerToLayoutDeviceScale2D;
typedef gfx::ScaleFactors2D<LayerPixel, RenderTargetPixel>
    LayerToRenderTargetScale2D;
typedef gfx::ScaleFactors2D<LayerPixel, ScreenPixel> LayerToScreenScale2D;
typedef gfx::ScaleFactors2D<LayerPixel, ParentLayerPixel>
    LayerToParentLayerScale2D;
typedef gfx::ScaleFactors2D<RenderTargetPixel, ScreenPixel>
    RenderTargetToScreenScale2D;
typedef gfx::ScaleFactors2D<ScreenPixel, CSSPixel> ScreenToCSSScale2D;
typedef gfx::ScaleFactors2D<ScreenPixel, LayoutDevicePixel>
    ScreenToLayoutDeviceScale2D;
typedef gfx::ScaleFactors2D<ScreenPixel, ScreenPixel> ScreenToScreenScale2D;
typedef gfx::ScaleFactors2D<ScreenPixel, LayerPixel> ScreenToLayerScale2D;
typedef gfx::ScaleFactors2D<ScreenPixel, ParentLayerPixel>
    ScreenToParentLayerScale2D;
typedef gfx::ScaleFactors2D<ParentLayerPixel, LayerPixel>
    ParentLayerToLayerScale2D;
typedef gfx::ScaleFactors2D<ParentLayerPixel, ScreenPixel>
    ParentLayerToScreenScale2D;
typedef gfx::ScaleFactors2D<ParentLayerPixel, ParentLayerPixel>
    ParentLayerToParentLayerScale2D;
typedef gfx::ScaleFactors2D<gfx::UnknownUnits, gfx::UnknownUnits> Scale2D;

typedef gfx::Matrix4x4Typed<CSSPixel, CSSPixel> CSSToCSSMatrix4x4;
typedef gfx::Matrix4x4Typed<LayoutDevicePixel, LayoutDevicePixel>
    LayoutDeviceToLayoutDeviceMatrix4x4;
typedef gfx::Matrix4x4Typed<LayoutDevicePixel, ParentLayerPixel>
    LayoutDeviceToParentLayerMatrix4x4;
typedef gfx::Matrix4x4Typed<LayerPixel, ParentLayerPixel>
    LayerToParentLayerMatrix4x4;
typedef gfx::Matrix4x4Typed<LayerPixel, ScreenPixel> LayerToScreenMatrix4x4;
typedef gfx::Matrix4x4Typed<ScreenPixel, ScreenPixel> ScreenToScreenMatrix4x4;
typedef gfx::Matrix4x4Typed<ScreenPixel, ParentLayerPixel>
    ScreenToParentLayerMatrix4x4;
typedef gfx::Matrix4x4Typed<ParentLayerPixel, LayerPixel>
    ParentLayerToLayerMatrix4x4;
typedef gfx::Matrix4x4Typed<ParentLayerPixel, ScreenPixel>
    ParentLayerToScreenMatrix4x4;
typedef gfx::Matrix4x4Typed<ParentLayerPixel, ParentLayerPixel>
    ParentLayerToParentLayerMatrix4x4;
typedef gfx::Matrix4x4Typed<ParentLayerPixel, RenderTargetPixel>
    ParentLayerToRenderTargetMatrix4x4;
typedef gfx::Matrix4x4Typed<ExternalPixel, ParentLayerPixel>
    ExternalToParentLayerMatrix4x4;

/*
 * The pixels that content authors use to specify sizes in.
 */
struct CSSPixel {
  // Conversions from app units
  static CSSCoord FromAppUnits(nscoord aCoord) {
    return NSAppUnitsToFloatPixels(aCoord, float(AppUnitsPerCSSPixel()));
  }

  static CSSPoint FromAppUnits(const nsPoint& aPoint) {
    return CSSPoint(
        NSAppUnitsToFloatPixels(aPoint.x, float(AppUnitsPerCSSPixel())),
        NSAppUnitsToFloatPixels(aPoint.y, float(AppUnitsPerCSSPixel())));
  }

  static CSSSize FromAppUnits(const nsSize& aSize) {
    return CSSSize(
        NSAppUnitsToFloatPixels(aSize.width, float(AppUnitsPerCSSPixel())),
        NSAppUnitsToFloatPixels(aSize.height, float(AppUnitsPerCSSPixel())));
  }

  static CSSRect FromAppUnits(const nsRect& aRect) {
    return CSSRect(
        NSAppUnitsToFloatPixels(aRect.x, float(AppUnitsPerCSSPixel())),
        NSAppUnitsToFloatPixels(aRect.y, float(AppUnitsPerCSSPixel())),
        NSAppUnitsToFloatPixels(aRect.Width(), float(AppUnitsPerCSSPixel())),
        NSAppUnitsToFloatPixels(aRect.Height(), float(AppUnitsPerCSSPixel())));
  }

  static CSSMargin FromAppUnits(const nsMargin& aMargin) {
    return CSSMargin(
        NSAppUnitsToFloatPixels(aMargin.top, float(AppUnitsPerCSSPixel())),
        NSAppUnitsToFloatPixels(aMargin.right, float(AppUnitsPerCSSPixel())),
        NSAppUnitsToFloatPixels(aMargin.bottom, float(AppUnitsPerCSSPixel())),
        NSAppUnitsToFloatPixels(aMargin.left, float(AppUnitsPerCSSPixel())));
  }

  static CSSIntPoint FromAppUnitsRounded(const nsPoint& aPoint) {
    return CSSIntPoint(
        NSAppUnitsToIntPixels(aPoint.x, float(AppUnitsPerCSSPixel())),
        NSAppUnitsToIntPixels(aPoint.y, float(AppUnitsPerCSSPixel())));
  }

  static CSSIntSize FromAppUnitsRounded(const nsSize& aSize) {
    return CSSIntSize(
        NSAppUnitsToIntPixels(aSize.width, float(AppUnitsPerCSSPixel())),
        NSAppUnitsToIntPixels(aSize.height, float(AppUnitsPerCSSPixel())));
  }

  static CSSIntRect FromAppUnitsRounded(const nsRect& aRect) {
    return CSSIntRect(
        NSAppUnitsToIntPixels(aRect.x, float(AppUnitsPerCSSPixel())),
        NSAppUnitsToIntPixels(aRect.y, float(AppUnitsPerCSSPixel())),
        NSAppUnitsToIntPixels(aRect.Width(), float(AppUnitsPerCSSPixel())),
        NSAppUnitsToIntPixels(aRect.Height(), float(AppUnitsPerCSSPixel())));
  }

  static CSSIntMargin FromAppUnitsRounded(const nsMargin& aMargin) {
    return CSSIntMargin(
        NSAppUnitsToIntPixels(aMargin.top, float(AppUnitsPerCSSPixel())),
        NSAppUnitsToIntPixels(aMargin.right, float(AppUnitsPerCSSPixel())),
        NSAppUnitsToIntPixels(aMargin.bottom, float(AppUnitsPerCSSPixel())),
        NSAppUnitsToIntPixels(aMargin.left, float(AppUnitsPerCSSPixel())));
  }

  static CSSIntRect FromAppUnitsToNearest(const nsRect& aRect) {
    return CSSIntRect::FromUnknownRect(
        aRect.ToNearestPixels(AppUnitsPerCSSPixel()));
  }

  static CSSIntRect FromAppUnitsToInside(const nsRect& aRect) {
    return CSSIntRect::FromUnknownRect(
        aRect.ToInsidePixels(AppUnitsPerCSSPixel()));
  }

  // Conversions to app units

  static nscoord ToAppUnits(CSSCoord aCoord) {
    return NSToCoordRoundWithClamp(aCoord * float(AppUnitsPerCSSPixel()));
  }

  static nsPoint ToAppUnits(const CSSPoint& aPoint) {
    return nsPoint(
        NSToCoordRoundWithClamp(aPoint.x * float(AppUnitsPerCSSPixel())),
        NSToCoordRoundWithClamp(aPoint.y * float(AppUnitsPerCSSPixel())));
  }

  static nsPoint ToAppUnits(const CSSIntPoint& aPoint) {
    return nsPoint(
        NSToCoordRoundWithClamp(float(aPoint.x) * float(AppUnitsPerCSSPixel())),
        NSToCoordRoundWithClamp(float(aPoint.y) *
                                float(AppUnitsPerCSSPixel())));
  }

  static nsSize ToAppUnits(const CSSSize& aSize) {
    return nsSize(
        NSToCoordRoundWithClamp(aSize.width * float(AppUnitsPerCSSPixel())),
        NSToCoordRoundWithClamp(aSize.height * float(AppUnitsPerCSSPixel())));
  }

  static nsSize ToAppUnits(const CSSIntSize& aSize) {
    return nsSize(NSToCoordRoundWithClamp(float(aSize.width) *
                                          float(AppUnitsPerCSSPixel())),
                  NSToCoordRoundWithClamp(float(aSize.height) *
                                          float(AppUnitsPerCSSPixel())));
  }

  static nsRect ToAppUnits(const CSSRect& aRect) {
    return nsRect(
        NSToCoordRoundWithClamp(aRect.x * float(AppUnitsPerCSSPixel())),
        NSToCoordRoundWithClamp(aRect.y * float(AppUnitsPerCSSPixel())),
        NSToCoordRoundWithClamp(aRect.Width() * float(AppUnitsPerCSSPixel())),
        NSToCoordRoundWithClamp(aRect.Height() * float(AppUnitsPerCSSPixel())));
  }

  static nsRect ToAppUnits(const CSSIntRect& aRect) {
    return nsRect(
        NSToCoordRoundWithClamp(float(aRect.x) * float(AppUnitsPerCSSPixel())),
        NSToCoordRoundWithClamp(float(aRect.y) * float(AppUnitsPerCSSPixel())),
        NSToCoordRoundWithClamp(float(aRect.Width()) *
                                float(AppUnitsPerCSSPixel())),
        NSToCoordRoundWithClamp(float(aRect.Height()) *
                                float(AppUnitsPerCSSPixel())));
  }

  // Conversion from a given CSS point value.
  static CSSCoord FromPoints(float aCoord) {
    // One inch / 72.
    return aCoord * 96.0f / 72.0f;
  }
};

/*
 * The pixels that are referred to as "device pixels" in layout code. In
 * general values measured in LayoutDevicePixels are obtained by dividing a
 * value in app units by AppUnitsPerDevPixel(). Conversion between CSS pixels
 * and LayoutDevicePixels is affected by:
 * 1) the "full zoom" (see nsPresContext::SetFullZoom)
 * 2) the "widget scale" (see nsIWidget::GetDefaultScale)
 */
struct LayoutDevicePixel {
  static LayoutDeviceRect FromAppUnits(const nsRect& aRect,
                                       nscoord aAppUnitsPerDevPixel) {
    return LayoutDeviceRect(
        NSAppUnitsToFloatPixels(aRect.x, float(aAppUnitsPerDevPixel)),
        NSAppUnitsToFloatPixels(aRect.y, float(aAppUnitsPerDevPixel)),
        NSAppUnitsToFloatPixels(aRect.Width(), float(aAppUnitsPerDevPixel)),
        NSAppUnitsToFloatPixels(aRect.Height(), float(aAppUnitsPerDevPixel)));
  }

  static LayoutDeviceSize FromAppUnits(const nsSize& aSize,
                                       nscoord aAppUnitsPerDevPixel) {
    return LayoutDeviceSize(
        NSAppUnitsToFloatPixels(aSize.width, aAppUnitsPerDevPixel),
        NSAppUnitsToFloatPixels(aSize.height, aAppUnitsPerDevPixel));
  }

  static LayoutDevicePoint FromAppUnits(const nsPoint& aPoint,
                                        nscoord aAppUnitsPerDevPixel) {
    return LayoutDevicePoint(
        NSAppUnitsToFloatPixels(aPoint.x, aAppUnitsPerDevPixel),
        NSAppUnitsToFloatPixels(aPoint.y, aAppUnitsPerDevPixel));
  }

  static LayoutDeviceMargin FromAppUnits(const nsMargin& aMargin,
                                         nscoord aAppUnitsPerDevPixel) {
    return LayoutDeviceMargin(
        NSAppUnitsToFloatPixels(aMargin.top, aAppUnitsPerDevPixel),
        NSAppUnitsToFloatPixels(aMargin.right, aAppUnitsPerDevPixel),
        NSAppUnitsToFloatPixels(aMargin.bottom, aAppUnitsPerDevPixel),
        NSAppUnitsToFloatPixels(aMargin.left, aAppUnitsPerDevPixel));
  }

  static LayoutDeviceIntPoint FromAppUnitsRounded(
      const nsPoint& aPoint, nscoord aAppUnitsPerDevPixel) {
    return LayoutDeviceIntPoint(
        NSAppUnitsToIntPixels(aPoint.x, aAppUnitsPerDevPixel),
        NSAppUnitsToIntPixels(aPoint.y, aAppUnitsPerDevPixel));
  }

  static LayoutDeviceIntRect FromAppUnitsRounded(const nsRect& aRect,
                                                 nscoord aAppUnitsPerDevPixel) {
    return LayoutDeviceIntRect(
        NSAppUnitsToIntPixels(aRect.x, aAppUnitsPerDevPixel),
        NSAppUnitsToIntPixels(aRect.y, aAppUnitsPerDevPixel),
        NSAppUnitsToIntPixels(aRect.Width(), aAppUnitsPerDevPixel),
        NSAppUnitsToIntPixels(aRect.Height(), aAppUnitsPerDevPixel));
  }

  static LayoutDeviceIntPoint FromAppUnitsToNearest(
      const nsPoint& aPoint, nscoord aAppUnitsPerDevPixel) {
    return LayoutDeviceIntPoint::FromUnknownPoint(
        aPoint.ToNearestPixels(aAppUnitsPerDevPixel));
  }

  static LayoutDeviceIntRect FromAppUnitsToNearest(
      const nsRect& aRect, nscoord aAppUnitsPerDevPixel) {
    return LayoutDeviceIntRect::FromUnknownRect(
        aRect.ToNearestPixels(aAppUnitsPerDevPixel));
  }

  static LayoutDeviceIntRect FromAppUnitsToInside(
      const nsRect& aRect, nscoord aAppUnitsPerDevPixel) {
    return LayoutDeviceIntRect::FromUnknownRect(
        aRect.ToInsidePixels(aAppUnitsPerDevPixel));
  }

  static LayoutDeviceIntRect FromAppUnitsToOutside(
      const nsRect& aRect, nscoord aAppUnitsPerDevPixel) {
    return LayoutDeviceIntRect::FromUnknownRect(
        aRect.ToOutsidePixels(aAppUnitsPerDevPixel));
  }

  static LayoutDeviceIntSize FromAppUnitsRounded(const nsSize& aSize,
                                                 nscoord aAppUnitsPerDevPixel) {
    return LayoutDeviceIntSize(
        NSAppUnitsToIntPixels(aSize.width, aAppUnitsPerDevPixel),
        NSAppUnitsToIntPixels(aSize.height, aAppUnitsPerDevPixel));
  }

  static LayoutDeviceIntMargin FromAppUnitsRounded(
      const nsMargin& aMargin, nscoord aAppUnitsPerDevPixel) {
    return LayoutDeviceIntMargin(
        NSAppUnitsToIntPixels(aMargin.top, aAppUnitsPerDevPixel),
        NSAppUnitsToIntPixels(aMargin.right, aAppUnitsPerDevPixel),
        NSAppUnitsToIntPixels(aMargin.bottom, aAppUnitsPerDevPixel),
        NSAppUnitsToIntPixels(aMargin.left, aAppUnitsPerDevPixel));
  }

  static nsPoint ToAppUnits(const LayoutDeviceIntPoint& aPoint,
                            nscoord aAppUnitsPerDevPixel) {
    return nsPoint(aPoint.x * aAppUnitsPerDevPixel,
                   aPoint.y * aAppUnitsPerDevPixel);
  }

  static nsSize ToAppUnits(const LayoutDeviceIntSize& aSize,
                           nscoord aAppUnitsPerDevPixel) {
    return nsSize(aSize.width * aAppUnitsPerDevPixel,
                  aSize.height * aAppUnitsPerDevPixel);
  }

  static nsSize ToAppUnits(const LayoutDeviceSize& aSize,
                           nscoord aAppUnitsPerDevPixel) {
    return nsSize(NSFloatPixelsToAppUnits(aSize.width, aAppUnitsPerDevPixel),
                  NSFloatPixelsToAppUnits(aSize.height, aAppUnitsPerDevPixel));
  }

  static nsRect ToAppUnits(const LayoutDeviceIntRect& aRect,
                           nscoord aAppUnitsPerDevPixel) {
    return nsRect(aRect.x * aAppUnitsPerDevPixel,
                  aRect.y * aAppUnitsPerDevPixel,
                  aRect.Width() * aAppUnitsPerDevPixel,
                  aRect.Height() * aAppUnitsPerDevPixel);
  }

  static nsRect ToAppUnits(const LayoutDeviceRect& aRect,
                           nscoord aAppUnitsPerDevPixel) {
    return nsRect(
        NSFloatPixelsToAppUnits(aRect.x, aAppUnitsPerDevPixel),
        NSFloatPixelsToAppUnits(aRect.y, aAppUnitsPerDevPixel),
        NSFloatPixelsToAppUnits(aRect.Width(), aAppUnitsPerDevPixel),
        NSFloatPixelsToAppUnits(aRect.Height(), aAppUnitsPerDevPixel));
  }

  static nsMargin ToAppUnits(const LayoutDeviceIntMargin& aMargin,
                             nscoord aAppUnitsPerDevPixel) {
    return nsMargin(aMargin.top * aAppUnitsPerDevPixel,
                    aMargin.right * aAppUnitsPerDevPixel,
                    aMargin.bottom * aAppUnitsPerDevPixel,
                    aMargin.left * aAppUnitsPerDevPixel);
  }
};

/*
 * The pixels that layout rasterizes and delivers to the graphics code.
 * These also are generally referred to as "device pixels" in layout code.
 * Conversion between CSS pixels and LayerPixels is affected by:
 * 1) the "display resolution" (see PresShell::SetResolution)
 * 2) the "full zoom" (see nsPresContext::SetFullZoom)
 * 3) the "widget scale" (see nsIWidget::GetDefaultScale)
 * 4) rasterizing at a different scale in the presence of some CSS transforms
 */
struct LayerPixel {};

/*
 * This is Layer coordinates with the Layer's CSS transform applied.
 * It can be thought of as intermediate between LayerPixel and
 * ParentLayerPixel, as further applying async transforms to this
 * yields ParentLayerPixels.
 */
struct CSSTransformedLayerPixel {};

/*
 * Layers are always composited to a render target. This unit
 * represents one pixel in the render target. Note that for the
 * root render target RenderTargetPixel == ScreenPixel. Also
 * any ContainerLayer providing an intermediate surface will
 * have RenderTargetPixel == LayerPixel.
 */
struct RenderTargetPixel {};

/*
 * This unit represents one pixel in an image. Image space
 * is largely independent of any other space.
 */
struct ImagePixel {};

/*
 * The pixels that are displayed on the screen.
 * On non-OMTC platforms this should be equivalent to LayerPixel units.
 * On OMTC platforms these may diverge from LayerPixel units temporarily,
 * while an asynchronous zoom is happening, but should eventually converge
 * back to LayerPixel units. Some variables (such as those representing
 * chrome UI element sizes) that are not subject to content zoom should
 * generally be represented in ScreenPixel units.
 */
struct ScreenPixel {};

/* The layer coordinates of the parent frame.
 * This can be arrived at in three ways:
 *   - Start with the CSS coordinates of the parent frame, multiply by the
 *     device scale and the cumulative resolution of the parent frame.
 *   - Start with the CSS coordinates of current frame, multiply by the device
 *     scale, the cumulative resolution of the current frame, and the scales
 *     from the CSS and async transforms of the current frame.
 *   - Start with global screen coordinates and unapply all CSS and async
 *     transforms from the root down to and including the parent.
 * It's helpful to look at
 * https://wiki.mozilla.org/Platform/GFX/APZ#Coordinate_systems to get a picture
 * of how the various coordinate systems relate to each other.
 */
struct ParentLayerPixel {};

/*
 * Pixels in the coordinate space used by the host OS to manage windows on the
 * desktop. What these mean varies between OSs:
 * - by default (unless implemented differently in platform-specific widget
 *   code) they are the same as LayoutDevicePixels
 * - on Mac OS X, they're "cocoa points", which correspond to device pixels
 *   on a non-Retina display, and to 2x device pixels on Retina
 * - on Windows *without* per-monitor DPI support, they are Windows "logical
 *   pixels", i.e. device pixels scaled according to the Windows display DPI
 *   scaling factor (typically one of 1.25, 1.5, 1.75, 2.0...)
 * - on Windows *with* per-monitor DPI support, they are physical device pixels
 *   on each screen; note that this means the scaling between CSS pixels and
 *   desktop pixels may vary across multiple displays.
 */
struct DesktopPixel {};

struct ExternalPixel {};

// Operators to apply ScaleFactors directly to Coords, Points, Rects, Sizes and
// Margins

template <class Src, class Dst>
gfx::CoordTyped<Dst> operator*(const gfx::CoordTyped<Src>& aCoord,
                               const gfx::ScaleFactor<Src, Dst>& aScale) {
  return gfx::CoordTyped<Dst>(aCoord.value * aScale.scale);
}

template <class Src, class Dst>
gfx::CoordTyped<Dst> operator/(const gfx::CoordTyped<Src>& aCoord,
                               const gfx::ScaleFactor<Dst, Src>& aScale) {
  return gfx::CoordTyped<Dst>(aCoord.value / aScale.scale);
}

template <class Src, class Dst>
gfx::PointTyped<Dst> operator*(const gfx::PointTyped<Src>& aPoint,
                               const gfx::ScaleFactor<Src, Dst>& aScale) {
  return gfx::PointTyped<Dst>(aPoint.x * aScale.scale, aPoint.y * aScale.scale);
}

template <class Src, class Dst>
gfx::PointTyped<Dst> operator/(const gfx::PointTyped<Src>& aPoint,
                               const gfx::ScaleFactor<Dst, Src>& aScale) {
  return gfx::PointTyped<Dst>(aPoint.x / aScale.scale, aPoint.y / aScale.scale);
}

template <class Src, class Dst, class F>
gfx::PointTyped<Dst, F> operator*(
    const gfx::PointTyped<Src, F>& aPoint,
    const gfx::BaseScaleFactors2D<Src, Dst, F>& aScale) {
  return gfx::PointTyped<Dst, F>(aPoint.x * aScale.xScale,
                                 aPoint.y * aScale.yScale);
}

template <class Src, class Dst, class F>
gfx::PointTyped<Dst, F> operator/(
    const gfx::PointTyped<Src, F>& aPoint,
    const gfx::BaseScaleFactors2D<Dst, Src, F>& aScale) {
  return gfx::PointTyped<Dst, F>(aPoint.x / aScale.xScale,
                                 aPoint.y / aScale.yScale);
}

template <class Src, class Dst>
gfx::PointTyped<Dst> operator*(const gfx::IntPointTyped<Src>& aPoint,
                               const gfx::ScaleFactor<Src, Dst>& aScale) {
  return gfx::PointTyped<Dst>(float(aPoint.x) * aScale.scale,
                              float(aPoint.y) * aScale.scale);
}

template <class Src, class Dst>
gfx::PointTyped<Dst> operator/(const gfx::IntPointTyped<Src>& aPoint,
                               const gfx::ScaleFactor<Dst, Src>& aScale) {
  return gfx::PointTyped<Dst>(float(aPoint.x) / aScale.scale,
                              float(aPoint.y) / aScale.scale);
}

template <class Src, class Dst, class F>
gfx::PointTyped<Dst, F> operator*(
    const gfx::IntPointTyped<Src>& aPoint,
    const gfx::BaseScaleFactors2D<Src, Dst, F>& aScale) {
  return gfx::PointTyped<Dst, F>(F(aPoint.x) * aScale.xScale,
                                 F(aPoint.y) * aScale.yScale);
}

template <class Src, class Dst, class F>
gfx::PointTyped<Dst, F> operator/(
    const gfx::IntPointTyped<Src>& aPoint,
    const gfx::BaseScaleFactors2D<Dst, Src, F>& aScale) {
  return gfx::PointTyped<Dst, F>(F(aPoint.x) / aScale.xScale,
                                 F(aPoint.y) / aScale.yScale);
}

template <class Src, class Dst>
gfx::RectTyped<Dst> operator*(const gfx::RectTyped<Src>& aRect,
                              const gfx::ScaleFactor<Src, Dst>& aScale) {
  return gfx::RectTyped<Dst>(aRect.x * aScale.scale, aRect.y * aScale.scale,
                             aRect.Width() * aScale.scale,
                             aRect.Height() * aScale.scale);
}

template <class Src, class Dst>
gfx::RectTyped<Dst> operator/(const gfx::RectTyped<Src>& aRect,
                              const gfx::ScaleFactor<Dst, Src>& aScale) {
  return gfx::RectTyped<Dst>(aRect.x / aScale.scale, aRect.y / aScale.scale,
                             aRect.Width() / aScale.scale,
                             aRect.Height() / aScale.scale);
}

template <class Src, class Dst, class F>
gfx::RectTyped<Dst, F> operator*(
    const gfx::RectTyped<Src, F>& aRect,
    const gfx::BaseScaleFactors2D<Src, Dst, F>& aScale) {
  return gfx::RectTyped<Dst, F>(
      aRect.x * aScale.xScale, aRect.y * aScale.yScale,
      aRect.Width() * aScale.xScale, aRect.Height() * aScale.yScale);
}

template <class Src, class Dst, class F>
gfx::RectTyped<Dst, F> operator/(
    const gfx::RectTyped<Src, F>& aRect,
    const gfx::BaseScaleFactors2D<Dst, Src, F>& aScale) {
  return gfx::RectTyped<Dst, F>(
      aRect.x / aScale.xScale, aRect.y / aScale.yScale,
      aRect.Width() / aScale.xScale, aRect.Height() / aScale.yScale);
}

template <class Src, class Dst>
gfx::RectTyped<Dst> operator*(const gfx::IntRectTyped<Src>& aRect,
                              const gfx::ScaleFactor<Src, Dst>& aScale) {
  return gfx::RectTyped<Dst>(float(aRect.x) * aScale.scale,
                             float(aRect.y) * aScale.scale,
                             float(aRect.Width()) * aScale.scale,
                             float(aRect.Height()) * aScale.scale);
}

template <class Src, class Dst>
gfx::RectTyped<Dst> operator/(const gfx::IntRectTyped<Src>& aRect,
                              const gfx::ScaleFactor<Dst, Src>& aScale) {
  return gfx::RectTyped<Dst>(float(aRect.x) / aScale.scale,
                             float(aRect.y) / aScale.scale,
                             float(aRect.Width()) / aScale.scale,
                             float(aRect.Height()) / aScale.scale);
}

template <class Src, class Dst, class F>
gfx::RectTyped<Dst, F> operator*(
    const gfx::IntRectTyped<Src>& aRect,
    const gfx::BaseScaleFactors2D<Src, Dst, F>& aScale) {
  return gfx::RectTyped<Dst, F>(
      F(aRect.x) * aScale.xScale, F(aRect.y) * aScale.yScale,
      F(aRect.Width()) * aScale.xScale, F(aRect.Height()) * aScale.yScale);
}

template <class Src, class Dst, class F>
gfx::RectTyped<Dst, F> operator/(
    const gfx::IntRectTyped<Src>& aRect,
    const gfx::BaseScaleFactors2D<Dst, Src, F>& aScale) {
  return gfx::RectTyped<Dst, F>(
      F(aRect.x) / aScale.xScale, F(aRect.y) / aScale.yScale,
      F(aRect.Width()) / aScale.xScale, F(aRect.Height()) / aScale.yScale);
}

template <class Src, class Dst>
gfx::SizeTyped<Dst> operator*(const gfx::SizeTyped<Src>& aSize,
                              const gfx::ScaleFactor<Src, Dst>& aScale) {
  return gfx::SizeTyped<Dst>(aSize.width * aScale.scale,
                             aSize.height * aScale.scale);
}

template <class Src, class Dst>
gfx::SizeTyped<Dst> operator/(const gfx::SizeTyped<Src>& aSize,
                              const gfx::ScaleFactor<Dst, Src>& aScale) {
  return gfx::SizeTyped<Dst>(aSize.width / aScale.scale,
                             aSize.height / aScale.scale);
}

template <class Src, class Dst, class F>
gfx::SizeTyped<Dst, F> operator*(
    const gfx::SizeTyped<Src, F>& aSize,
    const gfx::BaseScaleFactors2D<Src, Dst, F>& aScale) {
  return gfx::SizeTyped<Dst, F>(aSize.width * aScale.xScale,
                                aSize.height * aScale.yScale);
}

template <class Src, class Dst, class F>
gfx::SizeTyped<Dst, F> operator/(
    const gfx::SizeTyped<Src, F>& aSize,
    const gfx::BaseScaleFactors2D<Dst, Src, F>& aScale) {
  return gfx::SizeTyped<Dst, F>(aSize.width / aScale.xScale,
                                aSize.height / aScale.yScale);
}

template <class Src, class Dst>
gfx::SizeTyped<Dst> operator*(const gfx::IntSizeTyped<Src>& aSize,
                              const gfx::ScaleFactor<Src, Dst>& aScale) {
  return gfx::SizeTyped<Dst>(float(aSize.width) * aScale.scale,
                             float(aSize.height) * aScale.scale);
}

template <class Src, class Dst>
gfx::SizeTyped<Dst> operator/(const gfx::IntSizeTyped<Src>& aSize,
                              const gfx::ScaleFactor<Dst, Src>& aScale) {
  return gfx::SizeTyped<Dst>(float(aSize.width) / aScale.scale,
                             float(aSize.height) / aScale.scale);
}

template <class Src, class Dst, class F>
gfx::SizeTyped<Dst, F> operator*(
    const gfx::IntSizeTyped<Src>& aSize,
    const gfx::BaseScaleFactors2D<Src, Dst, F>& aScale) {
  return gfx::SizeTyped<Dst, F>(F(aSize.width) * aScale.xScale,
                                F(aSize.height) * aScale.yScale);
}

template <class Src, class Dst, class F>
gfx::SizeTyped<Dst, F> operator/(
    const gfx::IntSizeTyped<Src>& aSize,
    const gfx::BaseScaleFactors2D<Dst, Src, F>& aScale) {
  return gfx::SizeTyped<Dst, F>(F(aSize.width) / aScale.xScale,
                                F(aSize.height) / aScale.yScale);
}

template <class Src, class Dst>
gfx::MarginTyped<Dst> operator*(const gfx::MarginTyped<Src>& aMargin,
                                const gfx::ScaleFactor<Src, Dst>& aScale) {
  return gfx::MarginTyped<Dst>(
      aMargin.top * aScale.scale, aMargin.right * aScale.scale,
      aMargin.bottom * aScale.scale, aMargin.left * aScale.scale);
}

template <class Src, class Dst>
gfx::MarginTyped<Dst> operator/(const gfx::MarginTyped<Src>& aMargin,
                                const gfx::ScaleFactor<Dst, Src>& aScale) {
  return gfx::MarginTyped<Dst>(
      aMargin.top / aScale.scale, aMargin.right / aScale.scale,
      aMargin.bottom / aScale.scale, aMargin.left / aScale.scale);
}

template <class Src, class Dst, class F>
gfx::MarginTyped<Dst, F> operator*(
    const gfx::MarginTyped<Src, F>& aMargin,
    const gfx::BaseScaleFactors2D<Src, Dst, F>& aScale) {
  return gfx::MarginTyped<Dst, F>(
      aMargin.top * aScale.yScale, aMargin.right * aScale.xScale,
      aMargin.bottom * aScale.yScale, aMargin.left * aScale.xScale);
}

template <class Src, class Dst, class F>
gfx::MarginTyped<Dst, F> operator/(
    const gfx::MarginTyped<Src, F>& aMargin,
    const gfx::BaseScaleFactors2D<Dst, Src, F>& aScale) {
  return gfx::MarginTyped<Dst, F>(
      aMargin.top / aScale.yScale, aMargin.right / aScale.xScale,
      aMargin.bottom / aScale.yScale, aMargin.left / aScale.xScale);
}

// Calculate the max or min or the ratios of the widths and heights of two
// sizes, returning a scale factor in the correct units.

template <class Src, class Dst>
gfx::ScaleFactor<Src, Dst> MaxScaleRatio(const gfx::SizeTyped<Dst>& aDestSize,
                                         const gfx::SizeTyped<Src>& aSrcSize) {
  MOZ_ASSERT(aSrcSize.width != 0 && aSrcSize.height != 0,
             "Caller must verify aSrcSize has nonzero components, "
             "to avoid division by 0 here");
  return gfx::ScaleFactor<Src, Dst>(std::max(
      aDestSize.width / aSrcSize.width, aDestSize.height / aSrcSize.height));
}

template <class Src, class Dst>
gfx::ScaleFactor<Src, Dst> MinScaleRatio(const gfx::SizeTyped<Dst>& aDestSize,
                                         const gfx::SizeTyped<Src>& aSrcSize) {
  MOZ_ASSERT(aSrcSize.width != 0 && aSrcSize.height != 0,
             "Caller must verify aSrcSize has nonzero components, "
             "to avoid division by 0 here");
  return gfx::ScaleFactor<Src, Dst>(std::min(
      aDestSize.width / aSrcSize.width, aDestSize.height / aSrcSize.height));
}

template <typename T>
struct CoordOfImpl;

template <typename Units>
struct CoordOfImpl<gfx::PointTyped<Units>> {
  typedef gfx::CoordTyped<Units> Type;
};

template <typename Units>
struct CoordOfImpl<gfx::IntPointTyped<Units>> {
  typedef gfx::IntCoordTyped<Units> Type;
};

template <typename Units>
struct CoordOfImpl<gfx::RectTyped<Units>> {
  typedef gfx::CoordTyped<Units> Type;
};

template <typename Units>
struct CoordOfImpl<gfx::IntRectTyped<Units>> {
  typedef gfx::IntCoordTyped<Units> Type;
};

template <typename Units>
struct CoordOfImpl<gfx::SizeTyped<Units>> {
  typedef gfx::CoordTyped<Units> Type;
};

template <typename T>
using CoordOf = typename CoordOfImpl<T>::Type;

}  // namespace mozilla

#endif
