/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZUtils_h
#define mozilla_layers_APZUtils_h

#include <stdint.h>                     // for uint32_t
#include "FrameMetrics.h"
#include "LayersTypes.h"
#include "UnitTransforms.h"
#include "mozilla/gfx/CompositorHitTestInfo.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/DefineEnum.h"
#include "mozilla/EnumSet.h"
#include "mozilla/FloatingPoint.h"

namespace mozilla {
namespace layers {

enum CancelAnimationFlags : uint32_t {
  Default = 0x0,             /* Cancel all animations */
  ExcludeOverscroll = 0x1,   /* Don't clear overscroll */
  ScrollSnap = 0x2,          /* Snap to snap points */
  ExcludeWheel = 0x4,        /* Don't stop wheel smooth-scroll animations */
  TriggeredExternally = 0x8, /* Cancellation was not triggered by APZ in
                                response to an input event */
};

inline CancelAnimationFlags
operator|(CancelAnimationFlags a, CancelAnimationFlags b)
{
  return static_cast<CancelAnimationFlags>(static_cast<int>(a)
                                         | static_cast<int>(b));
}

typedef EnumSet<ScrollDirection> ScrollDirections;

enum class ScrollSource {
  // scrollTo() or something similar.
  DOM,

  // Touch-screen or trackpad with gesture support.
  Touch,

  // Mouse wheel.
  Wheel,

  // Keyboard
  Keyboard,
};

MOZ_DEFINE_ENUM_CLASS_WITH_BASE(APZWheelAction, uint8_t, (
    Scroll,
    PinchZoom
))

// Epsilon to be used when comparing 'float' coordinate values
// with FuzzyEqualsAdditive. The rationale is that 'float' has 7 decimal
// digits of precision, and coordinate values should be no larger than in the
// ten thousands. Note also that the smallest legitimate difference in page
// coordinates is 1 app unit, which is 1/60 of a (CSS pixel), so this epsilon
// isn't too large.
const float COORDINATE_EPSILON = 0.01f;

template <typename Units>
static bool IsZero(const gfx::PointTyped<Units>& aPoint)
{
  return FuzzyEqualsAdditive(aPoint.x, 0.0f, COORDINATE_EPSILON)
      && FuzzyEqualsAdditive(aPoint.y, 0.0f, COORDINATE_EPSILON);
}

// Deem an AsyncTransformComponentMatrix (obtained by multiplying together
// one or more AsyncTransformComponentMatrix objects) as constituting a
// complete async transform.
inline AsyncTransformMatrix
CompleteAsyncTransform(const AsyncTransformComponentMatrix& aMatrix)
{
  return ViewAs<AsyncTransformMatrix>(aMatrix,
      PixelCastJustification::MultipleAsyncTransforms);
}

struct TargetConfirmationFlags {
  explicit TargetConfirmationFlags(bool aTargetConfirmed)
    : mTargetConfirmed(aTargetConfirmed)
    , mRequiresTargetConfirmation(false)
  {}

  explicit TargetConfirmationFlags(gfx::CompositorHitTestInfo aHitTestInfo)
    : mTargetConfirmed(aHitTestInfo != gfx::CompositorHitTestInfo::eInvisibleToHitTest &&
                       !(aHitTestInfo & gfx::CompositorHitTestInfo::eDispatchToContent))
    , mRequiresTargetConfirmation(aHitTestInfo & gfx::CompositorHitTestInfo::eRequiresTargetConfirmation)
  {}

  bool mTargetConfirmed : 1;
  bool mRequiresTargetConfirmation : 1;
};

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
const ScreenMargin CalculatePendingDisplayPort(const FrameMetrics& aFrameMetrics,
                                               const ParentLayerPoint& aVelocity);

} // namespace apz

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_APZUtils_h
