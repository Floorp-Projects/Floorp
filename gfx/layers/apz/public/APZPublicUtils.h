/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZPublicUtils_h
#define mozilla_layers_APZPublicUtils_h

// This file is for APZ-related utilities that need to be consumed from outside
// of gfx/layers. For internal utilities, prefer APZUtils.h.

#include <stdint.h>  // for uint32_t
#include <type_traits>
#include "gfxTypes.h"
#include "FrameMetrics.h"
#include "LayersTypes.h"
#include "UnitTransforms.h"
#include "mozilla/gfx/CompositorHitTestInfo.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/DefineEnum.h"
#include "mozilla/EnumSet.h"
#include "mozilla/FloatingPoint.h"

namespace mozilla {

struct ExternalPixel;

template <>
struct IsPixel<ExternalPixel> : std::true_type {};

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

typedef gfx::Matrix4x4Typed<ExternalPixel, ParentLayerPixel>
    ExternalToParentLayerMatrix4x4;

struct ExternalPixel {};

namespace layers {

// clang-format off
MOZ_DEFINE_ENUM_CLASS_WITH_BASE(APZWheelAction, uint8_t, (
    Scroll,
    PinchZoom
))
// clang-format on

namespace apz {

/**
 * Initializes the global state used in AsyncPanZoomController.
 * This is normally called when it is first needed in the constructor
 * of APZCTreeManager, but can be called manually to force it to be
 * initialized earlier.
 */
void InitializeGlobalState();

/**
 * See AsyncPanZoomController::CalculatePendingDisplayPort. This
 * function simply delegates to that one, so that non-layers code
 * never needs to include AsyncPanZoomController.h
 */
const ScreenMargin CalculatePendingDisplayPort(
    const FrameMetrics& aFrameMetrics, const ParentLayerPoint& aVelocity);

}  // namespace apz

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_APZPublicUtils_h
