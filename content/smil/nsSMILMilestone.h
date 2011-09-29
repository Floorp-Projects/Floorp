/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SMIL module.
 *
 * The Initial Developer of the Original Code is Brian Birtles.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Birtles <birtles@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    : mTime(0), mIsEnd(PR_FALSE)
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
                       // end, PR_FALSE otherwise.
};

#endif // NS_SMILMILESTONE_H_
