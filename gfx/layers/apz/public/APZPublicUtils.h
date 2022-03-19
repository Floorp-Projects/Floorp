/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZPublicUtils_h
#define mozilla_layers_APZPublicUtils_h

// This file is for APZ-related utilities that need to be consumed from outside
// of gfx/layers. For internal utilities, prefer APZUtils.h.

#include <stdint.h>
#include <utility>
#include "ScrollAnimationBezierPhysics.h"
#include "Units.h"
#include "mozilla/DefineEnum.h"
#include "mozilla/ScrollOrigin.h"
#include "mozilla/gfx/Point.h"

namespace mozilla {

namespace layers {

struct FrameMetrics;

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

/**
 * Returns a width and height multiplier, each of which is a power of two
 * between 1 and 8 inclusive. The multiplier is chosen based on the provided
 * base size, such that multiplier is larger when the base size is larger.
 * The exact details are somewhat arbitrary and tuned by hand.
 * This function is intended to only be used with WebRender, because that is
 * the codepath that wants to use a larger displayport alignment, because
 * moving the displayport is relatively expensive with WebRender.
 */
gfx::IntSize GetDisplayportAlignmentMultiplier(const ScreenSize& aBaseSize);

/**
 * Calculate the physics parameters for smooth scroll animations for the
 * given origin, based on pref values.
 */
ScrollAnimationBezierPhysicsSettings ComputeBezierAnimationSettingsForOrigin(
    ScrollOrigin aOrigin);

}  // namespace apz

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_APZPublicUtils_h
