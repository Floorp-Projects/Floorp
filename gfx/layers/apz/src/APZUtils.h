/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZUtils_h
#define mozilla_layers_APZUtils_h

#include <stdint.h>                     // for uint32_t
#include "mozilla/gfx/Point.h"
#include "mozilla/FloatingPoint.h"

namespace mozilla {
namespace layers {

enum HitTestResult {
  HitNothing,
  HitLayer,
  HitDispatchToContentRegion,
};

enum CancelAnimationFlags : uint32_t {
  Default = 0,            /* Cancel all animations */
  ExcludeOverscroll = 1   /* Don't clear overscroll */
};

enum class ScrollSource {
  // scrollTo() or something similar.
  DOM,

  // Touch-screen or trackpad with gesture support.
  Touch,

  // Mouse wheel.
  Wheel
};

typedef uint32_t TouchBehaviorFlags;

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

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_APZUtils_h
