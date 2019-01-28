/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SMILInstanceTime.h"

#include "mozilla/AutoRestore.h"
#include "mozilla/SMILInterval.h"
#include "mozilla/SMILTimeValueSpec.h"

namespace mozilla {

//----------------------------------------------------------------------
// Implementation

SMILInstanceTime::SMILInstanceTime(const SMILTimeValue& aTime,
                                   SMILInstanceTimeSource aSource,
                                   SMILTimeValueSpec* aCreator,
                                   SMILInterval* aBaseInterval)
    : mTime(aTime),
      mFlags(0),
      mVisited(false),
      mFixedEndpointRefCnt(0),
      mSerial(0),
      mCreator(aCreator),
      mBaseInterval(nullptr)  // This will get set to aBaseInterval in a call to
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

SMILInstanceTime::~SMILInstanceTime() {
  MOZ_ASSERT(!mBaseInterval,
             "Destroying instance time without first calling Unlink()");
  MOZ_ASSERT(mFixedEndpointRefCnt == 0,
             "Destroying instance time that is still used as the fixed "
             "endpoint of an interval");
}

void SMILInstanceTime::Unlink() {
  RefPtr<SMILInstanceTime> deathGrip(this);
  if (mBaseInterval) {
    mBaseInterval->RemoveDependentTime(*this);
    mBaseInterval = nullptr;
  }
  mCreator = nullptr;
}

void SMILInstanceTime::HandleChangedInterval(
    const SMILTimeContainer* aSrcContainer, bool aBeginObjectChanged,
    bool aEndObjectChanged) {
  // It's possible a sequence of notifications might cause our base interval to
  // be updated and then deleted. Furthermore, the delete might happen whilst
  // we're still in the queue to be notified of the change. In any case, if we
  // don't have a base interval, just ignore the change.
  if (!mBaseInterval) return;

  MOZ_ASSERT(mCreator, "Base interval is set but creator is not.");

  if (mVisited) {
    // Break the cycle here
    Unlink();
    return;
  }

  bool objectChanged =
      mCreator->DependsOnBegin() ? aBeginObjectChanged : aEndObjectChanged;

  RefPtr<SMILInstanceTime> deathGrip(this);
  mozilla::AutoRestore<bool> setVisited(mVisited);
  mVisited = true;

  mCreator->HandleChangedInstanceTime(*GetBaseTime(), aSrcContainer, *this,
                                      objectChanged);
}

void SMILInstanceTime::HandleDeletedInterval() {
  MOZ_ASSERT(mBaseInterval,
             "Got call to HandleDeletedInterval on an independent instance "
             "time");
  MOZ_ASSERT(mCreator, "Base interval is set but creator is not");

  mBaseInterval = nullptr;
  mFlags &= ~kMayUpdate;  // Can't update without a base interval

  RefPtr<SMILInstanceTime> deathGrip(this);
  mCreator->HandleDeletedInstanceTime(*this);
  mCreator = nullptr;
}

void SMILInstanceTime::HandleFilteredInterval() {
  MOZ_ASSERT(mBaseInterval,
             "Got call to HandleFilteredInterval on an independent instance "
             "time");

  mBaseInterval = nullptr;
  mFlags &= ~kMayUpdate;  // Can't update without a base interval
  mCreator = nullptr;
}

bool SMILInstanceTime::ShouldPreserve() const {
  return mFixedEndpointRefCnt > 0 || (mFlags & kWasDynamicEndpoint);
}

void SMILInstanceTime::UnmarkShouldPreserve() {
  mFlags &= ~kWasDynamicEndpoint;
}

void SMILInstanceTime::AddRefFixedEndpoint() {
  MOZ_ASSERT(mFixedEndpointRefCnt < UINT16_MAX,
             "Fixed endpoint reference count upper limit reached");
  ++mFixedEndpointRefCnt;
  mFlags &= ~kMayUpdate;  // Once fixed, always fixed
}

void SMILInstanceTime::ReleaseFixedEndpoint() {
  MOZ_ASSERT(mFixedEndpointRefCnt > 0, "Duplicate release");
  --mFixedEndpointRefCnt;
  if (mFixedEndpointRefCnt == 0 && IsDynamic()) {
    mFlags |= kWasDynamicEndpoint;
  }
}

bool SMILInstanceTime::IsDependentOn(const SMILInstanceTime& aOther) const {
  if (mVisited) return false;

  const SMILInstanceTime* myBaseTime = GetBaseTime();
  if (!myBaseTime) return false;

  if (myBaseTime == &aOther) return true;

  mozilla::AutoRestore<bool> setVisited(mVisited);
  mVisited = true;
  return myBaseTime->IsDependentOn(aOther);
}

const SMILInstanceTime* SMILInstanceTime::GetBaseTime() const {
  if (!mBaseInterval) {
    return nullptr;
  }

  MOZ_ASSERT(mCreator, "Base interval is set but there is no creator.");
  if (!mCreator) {
    return nullptr;
  }

  return mCreator->DependsOnBegin() ? mBaseInterval->Begin()
                                    : mBaseInterval->End();
}

void SMILInstanceTime::SetBaseInterval(SMILInterval* aBaseInterval) {
  MOZ_ASSERT(!mBaseInterval,
             "Attempting to reassociate an instance time with a different "
             "interval.");

  if (aBaseInterval) {
    MOZ_ASSERT(mCreator,
               "Attempting to create a dependent instance time without "
               "reference to the creating SMILTimeValueSpec object.");
    if (!mCreator) return;

    aBaseInterval->AddDependentTime(*this);
  }

  mBaseInterval = aBaseInterval;
}

}  // namespace mozilla
