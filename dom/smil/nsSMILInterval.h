/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SMILINTERVAL_H_
#define NS_SMILINTERVAL_H_

#include "nsSMILInstanceTime.h"
#include "nsTArray.h"

//----------------------------------------------------------------------
// nsSMILInterval class
//
// A structure consisting of a begin and end time. The begin time must be
// resolved (i.e. not indefinite or unresolved).
//
// For an overview of how this class is related to other SMIL time classes see
// the documentation in nsSMILTimeValue.h

class nsSMILInterval
{
public:
  nsSMILInterval();
  nsSMILInterval(const nsSMILInterval& aOther);
  ~nsSMILInterval();
  void Unlink(bool aFiltered = false);

  const nsSMILInstanceTime* Begin() const
  {
    MOZ_ASSERT(mBegin && mEnd,
               "Requesting Begin() on un-initialized instance time");
    return mBegin;
  }
  nsSMILInstanceTime* Begin();

  const nsSMILInstanceTime* End() const
  {
    MOZ_ASSERT(mBegin && mEnd,
               "Requesting End() on un-initialized instance time");
    return mEnd;
  }
  nsSMILInstanceTime* End();

  void SetBegin(nsSMILInstanceTime& aBegin);
  void SetEnd(nsSMILInstanceTime& aEnd);
  void Set(nsSMILInstanceTime& aBegin, nsSMILInstanceTime& aEnd)
  {
    SetBegin(aBegin);
    SetEnd(aEnd);
  }

  void FixBegin();
  void FixEnd();

  typedef nsTArray<nsRefPtr<nsSMILInstanceTime> > InstanceTimeList;

  void AddDependentTime(nsSMILInstanceTime& aTime);
  void RemoveDependentTime(const nsSMILInstanceTime& aTime);
  void GetDependentTimes(InstanceTimeList& aTimes);

  // Cue for assessing if this interval can be filtered
  bool IsDependencyChainLink() const;

private:
  nsRefPtr<nsSMILInstanceTime> mBegin;
  nsRefPtr<nsSMILInstanceTime> mEnd;

  // nsSMILInstanceTimes to notify when this interval is changed or deleted.
  InstanceTimeList mDependentTimes;

  // Indicates if the end points of the interval are fixed or not.
  //
  // Note that this is not the same as having an end point whose TIME is fixed
  // (i.e. nsSMILInstanceTime::IsFixed() returns true). This is because it is
  // possible to have an end point with a fixed TIME and yet still update the
  // end point to refer to a different nsSMILInstanceTime object.
  //
  // However, if mBegin/EndFixed is true, then BOTH the nsSMILInstanceTime
  // OBJECT returned for that end point and its TIME value will not change.
  bool mBeginFixed;
  bool mEndFixed;
};

#endif // NS_SMILINTERVAL_H_
