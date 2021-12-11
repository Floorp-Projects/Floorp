/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SMIL_SMILINTERVAL_H_
#define DOM_SMIL_SMILINTERVAL_H_

#include "mozilla/SMILInstanceTime.h"
#include "nsTArray.h"

namespace mozilla {

//----------------------------------------------------------------------
// SMILInterval class
//
// A structure consisting of a begin and end time. The begin time must be
// resolved (i.e. not indefinite or unresolved).
//
// For an overview of how this class is related to other SMIL time classes see
// the documentation in SMILTimeValue.h

class SMILInterval {
 public:
  SMILInterval();
  SMILInterval(const SMILInterval& aOther);
  ~SMILInterval();
  void Unlink(bool aFiltered = false);

  const SMILInstanceTime* Begin() const {
    MOZ_ASSERT(mBegin && mEnd,
               "Requesting Begin() on un-initialized instance time");
    return mBegin;
  }
  SMILInstanceTime* Begin();

  const SMILInstanceTime* End() const {
    MOZ_ASSERT(mBegin && mEnd,
               "Requesting End() on un-initialized instance time");
    return mEnd;
  }
  SMILInstanceTime* End();

  void SetBegin(SMILInstanceTime& aBegin);
  void SetEnd(SMILInstanceTime& aEnd);
  void Set(SMILInstanceTime& aBegin, SMILInstanceTime& aEnd) {
    SetBegin(aBegin);
    SetEnd(aEnd);
  }

  void FixBegin();
  void FixEnd();

  using InstanceTimeList = nsTArray<RefPtr<SMILInstanceTime>>;

  void AddDependentTime(SMILInstanceTime& aTime);
  void RemoveDependentTime(const SMILInstanceTime& aTime);
  void GetDependentTimes(InstanceTimeList& aTimes);

  // Cue for assessing if this interval can be filtered
  bool IsDependencyChainLink() const;

 private:
  RefPtr<SMILInstanceTime> mBegin;
  RefPtr<SMILInstanceTime> mEnd;

  // SMILInstanceTimes to notify when this interval is changed or deleted.
  InstanceTimeList mDependentTimes;

  // Indicates if the end points of the interval are fixed or not.
  //
  // Note that this is not the same as having an end point whose TIME is fixed
  // (i.e. SMILInstanceTime::IsFixed() returns true). This is because it is
  // possible to have an end point with a fixed TIME and yet still update the
  // end point to refer to a different SMILInstanceTime object.
  //
  // However, if mBegin/EndFixed is true, then BOTH the SMILInstanceTime
  // OBJECT returned for that end point and its TIME value will not change.
  bool mBeginFixed;
  bool mEndFixed;
};

}  // namespace mozilla

#endif  // DOM_SMIL_SMILINTERVAL_H_
