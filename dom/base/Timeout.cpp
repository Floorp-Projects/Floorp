/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Timeout.h"

#include "mozilla/dom/TimeoutManager.h"

namespace mozilla {
namespace dom {

Timeout::Timeout()
  : mCleared(false),
    mRunning(false),
    mIsInterval(false),
    mIsTracking(false),
    mReason(Reason::eTimeoutOrInterval),
    mTimeoutId(0),
    mInterval(0),
    mFiringId(TimeoutManager::InvalidFiringId),
    mNestingLevel(0),
    mPopupState(openAllowed)
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(Timeout)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Timeout)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mScriptHandler)
  tmp->remove();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Timeout)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mScriptHandler)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(Timeout, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(Timeout, Release)

void
Timeout::SetWhenOrTimeRemaining(const TimeStamp& aBaseTime,
                                const TimeDuration& aDelay)
{
  // This must not be called on dummy timeouts.  Instead use SetDummyWhen().
  MOZ_DIAGNOSTIC_ASSERT(mWindow);

  // If we are frozen simply set mTimeRemaining to be the "time remaining" in
  // the timeout (i.e., the interval itself).  This will be used to create a
  // new mWhen time when the window is thawed.  The end effect is that time does
  // not appear to pass for frozen windows.
  if (mWindow->IsFrozen()) {
    mWhen = TimeStamp();
    mTimeRemaining = aDelay;
    mScheduledDelay = TimeDuration(0);
    return;
  }

  // Since we are not frozen we must set a precise mWhen target wakeup
  // time.  Even if we are suspended we want to use this target time so
  // that it appears time passes while suspended.
  mWhen = aBaseTime + aDelay;
  mTimeRemaining = TimeDuration(0);
  mScheduledDelay = aDelay;
}

void
Timeout::SetDummyWhen(const TimeStamp& aWhen)
{
  MOZ_DIAGNOSTIC_ASSERT(!mWindow);
  mWhen = aWhen;
}

const TimeStamp&
Timeout::When() const
{
  MOZ_DIAGNOSTIC_ASSERT(!mWhen.IsNull());
  // Note, mWindow->IsFrozen() can be true here.  The Freeze() method calls
  // When() to calculate the delay to populate mTimeRemaining.
  return mWhen;
}

const TimeDuration&
Timeout::TimeRemaining() const
{
  MOZ_DIAGNOSTIC_ASSERT(mWhen.IsNull());
  // Note, mWindow->IsFrozen() can be false here.  The Thaw() method calls
  // TimeRemaining() to calculate the new When() value.
  return mTimeRemaining;
}

const TimeDuration&
Timeout::ScheduledDelay() const
{
  MOZ_DIAGNOSTIC_ASSERT(!mWhen.IsNull());
  return mScheduledDelay;
}

} // namespace dom
} // namespace mozilla
