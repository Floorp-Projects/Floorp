/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSMILInstanceTime.h"
#include "nsSMILInterval.h"
#include "nsSMILTimeValueSpec.h"
#include "mozilla/AutoRestore.h"

//----------------------------------------------------------------------
// Implementation

nsSMILInstanceTime::nsSMILInstanceTime(const nsSMILTimeValue& aTime,
                                       nsSMILInstanceTimeSource aSource,
                                       nsSMILTimeValueSpec* aCreator,
                                       nsSMILInterval* aBaseInterval)
  : mTime(aTime),
    mFlags(0),
    mVisited(false),
    mFixedEndpointRefCnt(0),
    mSerial(0),
    mCreator(aCreator),
    mBaseInterval(nullptr) // This will get set to aBaseInterval in a call to
                          // SetBaseInterval() at end of constructor
{
  switch (aSource) {
    case SOURCE_NONE:
      // No special flags
      break;

    case SOURCE_DOM:
      mFlags = kDynamic | kFromDOM;
      break;

    case SOURCE_SYNCBASE:
      mFlags = kMayUpdate;
      break;

    case SOURCE_EVENT:
      mFlags = kDynamic;
      break;
  }

  SetBaseInterval(aBaseInterval);
}

nsSMILInstanceTime::~nsSMILInstanceTime()
{
  NS_ABORT_IF_FALSE(!mBaseInterval,
      "Destroying instance time without first calling Unlink()");
  NS_ABORT_IF_FALSE(mFixedEndpointRefCnt == 0,
      "Destroying instance time that is still used as the fixed endpoint of an "
      "interval");
}

void
nsSMILInstanceTime::Unlink()
{
  nsRefPtr<nsSMILInstanceTime> deathGrip(this);
  if (mBaseInterval) {
    mBaseInterval->RemoveDependentTime(*this);
    mBaseInterval = nullptr;
  }
  mCreator = nullptr;
}

void
nsSMILInstanceTime::HandleChangedInterval(
    const nsSMILTimeContainer* aSrcContainer,
    bool aBeginObjectChanged,
    bool aEndObjectChanged)
{
  // It's possible a sequence of notifications might cause our base interval to
  // be updated and then deleted. Furthermore, the delete might happen whilst
  // we're still in the queue to be notified of the change. In any case, if we
  // don't have a base interval, just ignore the change.
  if (!mBaseInterval)
    return;

  NS_ABORT_IF_FALSE(mCreator, "Base interval is set but creator is not.");

  if (mVisited) {
    // Break the cycle here
    Unlink();
    return;
  }

  bool objectChanged = mCreator->DependsOnBegin() ? aBeginObjectChanged :
                                                      aEndObjectChanged;

  mozilla::AutoRestore<bool> setVisited(mVisited);
  mVisited = true;

  nsRefPtr<nsSMILInstanceTime> deathGrip(this);
  mCreator->HandleChangedInstanceTime(*GetBaseTime(), aSrcContainer, *this,
                                      objectChanged);
}

void
nsSMILInstanceTime::HandleDeletedInterval()
{
  NS_ABORT_IF_FALSE(mBaseInterval,
      "Got call to HandleDeletedInterval on an independent instance time");
  NS_ABORT_IF_FALSE(mCreator, "Base interval is set but creator is not");

  mBaseInterval = nullptr;
  mFlags &= ~kMayUpdate; // Can't update without a base interval

  nsRefPtr<nsSMILInstanceTime> deathGrip(this);
  mCreator->HandleDeletedInstanceTime(*this);
  mCreator = nullptr;
}

void
nsSMILInstanceTime::HandleFilteredInterval()
{
  NS_ABORT_IF_FALSE(mBaseInterval,
      "Got call to HandleFilteredInterval on an independent instance time");

  mBaseInterval = nullptr;
  mFlags &= ~kMayUpdate; // Can't update without a base interval
  mCreator = nullptr;
}

bool
nsSMILInstanceTime::ShouldPreserve() const
{
  return mFixedEndpointRefCnt > 0 || (mFlags & kWasDynamicEndpoint);
}

void
nsSMILInstanceTime::UnmarkShouldPreserve()
{
  mFlags &= ~kWasDynamicEndpoint;
}

void
nsSMILInstanceTime::AddRefFixedEndpoint()
{
  NS_ABORT_IF_FALSE(mFixedEndpointRefCnt < PR_UINT16_MAX,
      "Fixed endpoint reference count upper limit reached");
  ++mFixedEndpointRefCnt;
  mFlags &= ~kMayUpdate; // Once fixed, always fixed
}

void
nsSMILInstanceTime::ReleaseFixedEndpoint()
{
  NS_ABORT_IF_FALSE(mFixedEndpointRefCnt > 0, "Duplicate release");
  --mFixedEndpointRefCnt;
  if (mFixedEndpointRefCnt == 0 && IsDynamic()) {
    mFlags |= kWasDynamicEndpoint;
  }
}

bool
nsSMILInstanceTime::IsDependentOn(const nsSMILInstanceTime& aOther) const
{
  if (mVisited)
    return false;

  const nsSMILInstanceTime* myBaseTime = GetBaseTime();
  if (!myBaseTime)
    return false;

  if (myBaseTime == &aOther)
    return true;

  // mVisited is mutable
  mozilla::AutoRestore<bool> setVisited(const_cast<nsSMILInstanceTime*>(this)->mVisited);
  const_cast<nsSMILInstanceTime*>(this)->mVisited = true;
  return myBaseTime->IsDependentOn(aOther);
}

const nsSMILInstanceTime*
nsSMILInstanceTime::GetBaseTime() const
{
  if (!mBaseInterval) {
    return nullptr;
  }

  NS_ABORT_IF_FALSE(mCreator, "Base interval is set but there is no creator.");
  if (!mCreator) {
    return nullptr;
  }

  return mCreator->DependsOnBegin() ? mBaseInterval->Begin() :
                                      mBaseInterval->End();
}

void
nsSMILInstanceTime::SetBaseInterval(nsSMILInterval* aBaseInterval)
{
  NS_ABORT_IF_FALSE(!mBaseInterval,
      "Attempting to reassociate an instance time with a different interval.");

  if (aBaseInterval) {
    NS_ABORT_IF_FALSE(mCreator,
        "Attempting to create a dependent instance time without reference "
        "to the creating nsSMILTimeValueSpec object.");
    if (!mCreator)
      return;

    aBaseInterval->AddDependentTime(*this);
  }

  mBaseInterval = aBaseInterval;
}
