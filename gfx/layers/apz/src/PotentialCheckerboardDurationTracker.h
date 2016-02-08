/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_PotentialCheckerboardDurationTracker_h
#define mozilla_layers_PotentialCheckerboardDurationTracker_h

#include "mozilla/TimeStamp.h"

namespace mozilla {
namespace layers {

/**
 * This class allows the owner to track the duration of time considered
 * "potentially checkerboarding". This is the union of two possibly-intersecting
 * sets of time periods. The first set is that in which checkerboarding was
 * actually happening, since by definition it could potentially be happening.
 * The second set is that in which the APZC is actively transforming content
 * in the compositor, since it could potentially transform it so as to display
 * checkerboarding to the user.
 * The caller of this class calls the appropriate methods to indicate the start
 * and stop of these two sets, and this class manages accumulating the union
 * of the various durations.
 */
class PotentialCheckerboardDurationTracker {
public:
  PotentialCheckerboardDurationTracker();

  /**
   * This should be called if checkerboarding is encountered. It can be called
   * multiple times during a checkerboard event.
   */
  void CheckerboardSeen();
  /**
   * This should be called when checkerboarding is done. It must have been
   * preceded by one or more calls to CheckerboardSeen().
   */
  void CheckerboardDone();

  /**
   * This should be called at composition time, to indicate if the APZC is in
   * a transforming state or not.
   */
  void InTransform(bool aInTransform);

private:
  bool Tracking() const;

private:
  bool mInCheckerboard;
  bool mInTransform;

  TimeStamp mCurrentPeriodStart;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_PotentialCheckerboardDurationTracker_h
