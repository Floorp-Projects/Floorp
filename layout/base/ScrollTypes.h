/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ScrollTypes_h
#define mozilla_ScrollTypes_h

#include "mozilla/TypedEnumBits.h"

// Types used in main-thread scrolling interfaces such as ScrollContainerFrame.

namespace mozilla {

/**
 * Scroll modes for main-thread scroll operations. These are mostly used
 * by ScrollContainerFrame methods.
 *
 * When a scroll operation is requested, we ask for instant, smooth,
 * smooth msd, or normal scrolling.
 *
 * |Smooth| scrolls have a symmetrical acceleration and deceleration curve
 * modeled with a set of splines that guarantee that the destination will be
 * reached over a fixed time interval.  |Smooth| will only be smooth if smooth
 * scrolling is actually enabled.  This behavior is utilized by keyboard and
 * mouse wheel scrolling events.
 *
 * |SmoothMsd| implements a physically based model that approximates the
 * behavior of a mass-spring-damper system.  |SmoothMsd| scrolls have a
 * non-symmetrical acceleration and deceleration curve, can potentially
 * overshoot the destination on intermediate frames, and complete over a
 * variable time interval.  |SmoothMsd| will only be smooth if cssom-view
 * smooth-scrolling is enabled.
 *
 * |Instant| is always synchronous, |Normal| can be asynchronous.
 *
 * If an |Instant| scroll request happens while a |Smooth| or async scroll is
 * already in progress, the async scroll is interrupted and we instantly
 * scroll to the destination.
 *
 * If an |Instant| or |Smooth| scroll request happens while a |SmoothMsd|
 * scroll is already in progress, the |SmoothMsd| scroll is interrupted without
 * first scrolling to the destination.
 */
enum class ScrollMode { Instant, Smooth, SmoothMsd, Normal };

/**
 * When scrolling by a relative amount, we can choose various units.
 */
enum class ScrollUnit { DEVICE_PIXELS, LINES, PAGES, WHOLE };

/**
 * Representing whether there's an on-going animation in APZC and it was
 * triggered by script or by user input.
 */
enum class APZScrollAnimationType {
  No,                   // No animation.
  TriggeredByScript,    // Animation triggered by script.
  TriggeredByUserInput  // Animation triggered by user input.
};

enum class ScrollSnapFlags : uint8_t {
  Disabled = 0,
  // https://drafts.csswg.org/css-scroll-snap/#intended-end-position
  IntendedEndPosition = 1 << 0,
  // https://drafts.csswg.org/css-scroll-snap/#intended-direction
  IntendedDirection = 1 << 1
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(ScrollSnapFlags);

}  // namespace mozilla

#endif  // mozilla_ScrollTypes_h
