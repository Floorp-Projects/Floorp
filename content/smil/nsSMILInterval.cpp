/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSMILInterval.h"

nsSMILInterval::nsSMILInterval()
:
  mBeginFixed(false),
  mEndFixed(false)
{
}

nsSMILInterval::nsSMILInterval(const nsSMILInterval& aOther)
:
  mBegin(aOther.mBegin),
  mEnd(aOther.mEnd),
  mBeginFixed(false),
  mEndFixed(false)
{
  NS_ABORT_IF_FALSE(aOther.mDependentTimes.IsEmpty(),
      "Attempting to copy-construct an interval with dependent times, "
      "this will lead to instance times being shared between intervals.");

  // For the time being we don't allow intervals with fixed endpoints to be
  // copied since we only ever copy-construct to establish a new current
  // interval. If we ever need to copy historical intervals we may need to move
  // the ReleaseFixedEndpoint calls from Unlink to the dtor.
  NS_ABORT_IF_FALSE(!aOther.mBeginFixed && !aOther.mEndFixed,
      "Attempting to copy-construct an interval with fixed endpoints");
}

nsSMILInterval::~nsSMILInterval()
{
  NS_ABORT_IF_FALSE(mDependentTimes.IsEmpty(),
      "Destroying interval without disassociating dependent instance times. "
      "Unlink was not called");
}

void
nsSMILInterval::Unlink(bool aFiltered)
{
  for (PRInt32 i = mDependentTimes.Length() - 1; i >= 0; --i) {
    if (aFiltered) {
      mDependentTimes[i]->HandleFilteredInterval();
    } else {
      mDependentTimes[i]->HandleDeletedInterval();
    }
  }
  mDependentTimes.Clear();
  if (mBegin && mBeginFixed) {
    mBegin->ReleaseFixedEndpoint();
  }
  mBegin = nsnull;
  if (mEnd && mEndFixed) {
    mEnd->ReleaseFixedEndpoint();
  }
  mEnd = nsnull;
}

nsSMILInstanceTime*
nsSMILInterval::Begin()
{
  NS_ABORT_IF_FALSE(mBegin && mEnd,
      "Requesting Begin() on un-initialized interval.");
  return mBegin;
}

nsSMILInstanceTime*
nsSMILInterval::End()
{
  NS_ABORT_IF_FALSE(mBegin && mEnd,
      "Requesting End() on un-initialized interval.");
  return mEnd;
}

void
nsSMILInterval::SetBegin(nsSMILInstanceTime& aBegin)
{
  NS_ABORT_IF_FALSE(aBegin.Time().IsDefinite(),
      "Attempting to set unresolved or indefinite begin time on interval");
  NS_ABORT_IF_FALSE(!mBeginFixed,
      "Attempting to set begin time but the begin point is fixed");
  // Check that we're not making an instance time dependent on itself. Such an
  // arrangement does not make intuitive sense and should be detected when
  // creating or updating intervals.
  NS_ABORT_IF_FALSE(!mBegin || aBegin.GetBaseTime() != mBegin,
      "Attempting to make self-dependent instance time");

  mBegin = &aBegin;
}

void
nsSMILInterval::SetEnd(nsSMILInstanceTime& aEnd)
{
  NS_ABORT_IF_FALSE(!mEndFixed,
      "Attempting to set end time but the end point is fixed");
  // As with SetBegin, check we're not making an instance time dependent on
  // itself.
  NS_ABORT_IF_FALSE(!mEnd || aEnd.GetBaseTime() != mEnd,
      "Attempting to make self-dependent instance time");

  mEnd = &aEnd;
}

void
nsSMILInterval::FixBegin()
{
  NS_ABORT_IF_FALSE(mBegin && mEnd,
      "Fixing begin point on un-initialized interval");
  NS_ABORT_IF_FALSE(!mBeginFixed, "Duplicate calls to FixBegin()");
  mBeginFixed = true;
  mBegin->AddRefFixedEndpoint();
}

void
nsSMILInterval::FixEnd()
{
  NS_ABORT_IF_FALSE(mBegin && mEnd,
      "Fixing end point on un-initialized interval");
  NS_ABORT_IF_FALSE(mBeginFixed,
      "Fixing the end of an interval without a fixed begin");
  NS_ABORT_IF_FALSE(!mEndFixed, "Duplicate calls to FixEnd()");
  mEndFixed = true;
  mEnd->AddRefFixedEndpoint();
}

void
nsSMILInterval::AddDependentTime(nsSMILInstanceTime& aTime)
{
  nsRefPtr<nsSMILInstanceTime>* inserted =
    mDependentTimes.InsertElementSorted(&aTime);
  if (!inserted) {
    NS_WARNING("Insufficient memory to insert instance time.");
  }
}

void
nsSMILInterval::RemoveDependentTime(const nsSMILInstanceTime& aTime)
{
#ifdef DEBUG
  bool found =
#endif
    mDependentTimes.RemoveElementSorted(&aTime);
  NS_ABORT_IF_FALSE(found, "Couldn't find instance time to delete.");
}

void
nsSMILInterval::GetDependentTimes(InstanceTimeList& aTimes)
{
  aTimes = mDependentTimes;
}

bool
nsSMILInterval::IsDependencyChainLink() const
{
  if (!mBegin || !mEnd)
    return false; // Not yet initialised so it can't be part of a chain

  if (mDependentTimes.IsEmpty())
    return false; // No dependents, chain end

  // So we have dependents, but we're still only a link in the chain (as opposed
  // to the end of the chain) if one of our endpoints is dependent on an
  // interval other than ourselves.
  return (mBegin->IsDependent() && mBegin->GetBaseInterval() != this) ||
         (mEnd->IsDependent() && mEnd->GetBaseInterval() != this);
}
