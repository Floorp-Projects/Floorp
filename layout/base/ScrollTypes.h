/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ScrollTypes_h
#define mozilla_ScrollTypes_h

// Types used in main-thread scrolling interfaces such as nsIScrollableFrame.

namespace mozilla {

/**
 * Scroll modes for main-thread scroll operations. These are mostly used
 * by nsIScrollableFrame methods.
 *
 * When a scroll operation is requested, we ask for instant, smooth,
 * smooth msd, or normal scrolling.
 *
 * |eSmooth| scrolls have a symmetrical acceleration and deceleration curve
 * modeled with a set of splines that guarantee that the destination will be
 * reached over a fixed time interval.  |eSmooth| will only be smooth if smooth
 * scrolling is actually enabled.  This behavior is utilized by keyboard and
 * mouse wheel scrolling events.
 *
 * |eSmoothMsd| implements a physically based model that approximates the
 * behavior of a mass-spring-damper system.  |eSmoothMsd| scrolls have a
 * non-symmetrical acceleration and deceleration curve, can potentially
 * overshoot the destination on intermediate frames, and complete over a
 * variable time interval.  |eSmoothMsd| will only be smooth if cssom-view
 * smooth-scrolling is enabled.
 *
 * |eInstant| is always synchronous, |eNormal| can be asynchronous.
 *
 * If an |eInstant| scroll request happens while a |eSmooth| or async scroll is
 * already in progress, the async scroll is interrupted and we instantly
 * scroll to the destination.
 *
 * If an |eInstant| or |eSmooth| scroll request happens while a |eSmoothMsd|
 * scroll is already in progress, the |eSmoothMsd| scroll is interrupted without
 * first scrolling to the destination.
 */
enum class ScrollMode { Instant, Smooth, SmoothMsd, Normal };

}  // namespace mozilla

#endif  // mozilla_ScrollTypes_h
