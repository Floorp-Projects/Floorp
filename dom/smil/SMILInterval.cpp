/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SMILInterval.h"

#include "mozilla/DebugOnly.h"

namespace mozilla {

SMILInterval::SMILInterval() : mBeginFixed(false), mEndFixed(false) {}

SMILInterval::SMILInterval(const SMILInterval& aOther)
    : mBegin(aOther.mBegin),
      mEnd(aOther.mEnd),
      mBeginFixed(false),
      mEndFixed(false) {
  MOZ_ASSERT(aOther.mDependentTimes.IsEmpty(),
             "Attempt to copy-construct an interval with dependent times; this "
             "will lead to instance times being shared between intervals.");

  // For the time being we don't allow intervals with fixed endpoints to be
  // copied since we only ever copy-construct to establish a new current
  // interval. If we ever need to copy historical intervals we may need to move
  // the ReleaseFixedEndpoint calls from Unlink to the dtor.
  MOZ_ASSERT(!aOther.mBeginFixed && !aOther.mEndFixed,
             "Attempt to copy-construct an interval with fixed endpoints");
}

SMILInterval::~SMILInterval() {
  MOZ_ASSERT(mDependentTimes.IsEmpty(),
             "Destroying interval without disassociating dependent instance "
             "times. Unlink was not called");
}

void SMILInterval::Unlink(bool aFiltered) {
  for (int32_t i = mDependentTimes.Length() - 1; i >= 0; --i) {
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
  mBegin = nullptr;
  if (mEnd && mEndFixed) {
    mEnd->ReleaseFixedEndpoint();
  }
  mEnd = nullptr;
}

SMILInstanceTime* SMILInterval::Begin() {
  MOZ_ASSERT(mBegin && mEnd, "Requesting Begin() on un-initialized interval.");
  return mBegin;
}

SMILInstanceTime* SMILInterval::End() {
  MOZ_ASSERT(mBegin && mEnd, "Requesting End() on un-initialized interval.");
  return mEnd;
}

void SMILInterval::SetBegin(SMILInstanceTime& aBegin) {
  MOZ_ASSERT(aBegin.Time().IsDefinite(),
             "Attempt to set unresolved or indefinite begin time on interval");
  MOZ_ASSERT(!mBeginFixed,
             "Attempt to set begin time but the begin point is fixed");
  // Check that we're not making an instance time dependent on itself. Such an
  // arrangement does not make intuitive sense and should be detected when
  // creating or updating intervals.
  MOZ_ASSERT(!mBegin || aBegin.GetBaseTime() != mBegin,
             "Attempt to make self-dependent instance time");

  mBegin = &aBegin;
}

void SMILInterval::SetEnd(SMILInstanceTime& aEnd) {
  MOZ_ASSERT(!mEndFixed, "Attempt to set end time but the end point is fixed");
  // As with SetBegin, check we're not making an instance time dependent on
  // itself.
  MOZ_ASSERT(!mEnd || aEnd.GetBaseTime() != mEnd,
             "Attempting to make self-dependent instance time");

  mEnd = &aEnd;
}

void SMILInterval::FixBegin() {
  MOZ_ASSERT(mBegin && mEnd, "Fixing begin point on un-initialized interval");
  MOZ_ASSERT(!mBeginFixed, "Duplicate calls to FixBegin()");
  mBeginFixed = true;
  mBegin->AddRefFixedEndpoint();
}

void SMILInterval::FixEnd() {
  MOZ_ASSERT(mBegin && mEnd, "Fixing end point on un-initialized interval");
  MOZ_ASSERT(mBeginFixed,
             "Fixing the end of an interval without a fixed begin");
  MOZ_ASSERT(!mEndFixed, "Duplicate calls to FixEnd()");
  mEndFixed = true;
  mEnd->AddRefFixedEndpoint();
}

void SMILInterval::AddDependentTime(SMILInstanceTime& aTime) {
  RefPtr<SMILInstanceTime>* inserted =
      mDependentTimes.InsertElementSorted(&aTime);
  if (!inserted) {
    NS_WARNING("Insufficient memory to insert instance time.");
  }
}

void SMILInterval::RemoveDependentTime(const SMILInstanceTime& aTime) {
  DebugOnly<bool> found = mDependentTimes.RemoveElementSorted(&aTime);
  MOZ_ASSERT(found, "Couldn't find instance time to delete.");
}

void SMILInterval::GetDependentTimes(InstanceTimeList& aTimes) {
  aTimes = mDependentTimes.Clone();
}

bool SMILInterval::IsDependencyChainLink() const {
  if (!mBegin || !mEnd)
    return false;  // Not yet initialised so it can't be part of a chain

  if (mDependentTimes.IsEmpty()) return false;  // No dependents, chain end

  // So we have dependents, but we're still only a link in the chain (as opposed
  // to the end of the chain) if one of our endpoints is dependent on an
  // interval other than ourselves.
  return (mBegin->IsDependent() && mBegin->GetBaseInterval() != this) ||
         (mEnd->IsDependent() && mEnd->GetBaseInterval() != this);
}

}  // namespace mozilla
