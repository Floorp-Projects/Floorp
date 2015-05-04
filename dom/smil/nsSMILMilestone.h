/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SMILMILESTONE_H_
#define NS_SMILMILESTONE_H_

/*
 * A significant moment in an nsSMILTimedElement's lifetime where a sample is
 * required.
 *
 * Animations register the next milestone in their lifetime with the time
 * container that they belong to. When the animation controller goes to run
 * a sample it first visits all the animations that have a registered milestone
 * in order of their milestone times. This allows interdependencies between
 * animations to be correctly resolved and events to fire in the proper order.
 *
 * A distinction is made between a milestone representing the end of an interval
 * and any other milestone. This is because if animation A ends at time t, and
 * animation B begins at the same time t (or has some other significant moment
 * such as firing a repeat event), SMIL's endpoint-exclusive timing model
 * implies that the interval end occurs first. In fact, interval ends can be
 * thought of as ending an infinitesimally small time before t. Therefore,
 * A should be sampled before B.
 *
 * Furthermore, this distinction between sampling the end of an interval and
 * a regular sample is used within the timing model (specifically in
 * nsSMILTimedElement) to ensure that all intervals ending at time t are sampled
 * before any new intervals are entered so that we have a fully up-to-date set
 * of instance times available before committing to a new interval. Once an
 * interval is entered, the begin time is fixed.
 */
class nsSMILMilestone
{
public:
  nsSMILMilestone(nsSMILTime aTime, bool aIsEnd)
    : mTime(aTime), mIsEnd(aIsEnd)
  { }

  nsSMILMilestone()
    : mTime(0), mIsEnd(false)
  { }

  bool operator==(const nsSMILMilestone& aOther) const
  {
    return mTime == aOther.mTime && mIsEnd == aOther.mIsEnd;
  }

  bool operator!=(const nsSMILMilestone& aOther) const
  {
    return !(*this == aOther);
  }

  bool operator<(const nsSMILMilestone& aOther) const
  {
    // Earlier times sort first, and for equal times end milestones sort first
    return mTime < aOther.mTime ||
          (mTime == aOther.mTime && mIsEnd && !aOther.mIsEnd);
  }

  bool operator<=(const nsSMILMilestone& aOther) const
  {
    return *this == aOther || *this < aOther;
  }

  bool operator>=(const nsSMILMilestone& aOther) const
  {
    return !(*this < aOther);
  }

  nsSMILTime   mTime;  // The milestone time. This may be in container time or
                       // parent container time depending on where it is used.
  bool mIsEnd; // true if this milestone corresponds to an interval
                       // end, false otherwise.
};

#endif // NS_SMILMILESTONE_H_
