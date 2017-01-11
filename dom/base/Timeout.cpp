/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Timeout.h"

#include "nsGlobalWindow.h"
#include "nsITimeoutHandler.h"
#include "nsITimer.h"
#include "mozilla/dom/TimeoutManager.h"

namespace mozilla {
namespace dom {

Timeout::Timeout()
  : mCleared(false),
    mRunning(false),
    mIsInterval(false),
    mReason(Reason::eTimeoutOrInterval),
    mTimeoutId(0),
    mInterval(0),
    mFiringDepth(0),
    mNestingLevel(0),
    mPopupState(openAllowed)
{
  MOZ_COUNT_CTOR(Timeout);
}

Timeout::~Timeout()
{
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }

  MOZ_COUNT_DTOR(Timeout);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(Timeout)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Timeout)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPrincipal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mScriptHandler)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Timeout)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPrincipal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mScriptHandler)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(Timeout, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(Timeout, Release)

namespace {

void
TimerCallback(nsITimer*, void* aClosure)
{
  RefPtr<Timeout> timeout = (Timeout*)aClosure;
  timeout->mWindow->AsInner()->TimeoutManager().RunTimeout(timeout);
}

void
TimerNameCallback(nsITimer* aTimer, void* aClosure, char* aBuf, size_t aLen)
{
  RefPtr<Timeout> timeout = (Timeout*)aClosure;

  const char* filename;
  uint32_t lineNum, column;
  timeout->mScriptHandler->GetLocation(&filename, &lineNum, &column);
  snprintf(aBuf, aLen, "[content] %s:%u:%u", filename, lineNum, column);
}

} // anonymous namespace

nsresult
Timeout::InitTimer(nsIEventTarget* aTarget, uint32_t aDelay)
{
  // If the given target does not match the timer's current target
  // then we need to override it before the Init.  Note that GetTarget()
  // will return the current thread after setting the target to nullptr.
  // So we need to special case the nullptr target comparison.
  nsCOMPtr<nsIEventTarget> currentTarget;
  MOZ_ALWAYS_SUCCEEDS(mTimer->GetTarget(getter_AddRefs(currentTarget)));
  if ((aTarget && currentTarget != aTarget) ||
      (!aTarget && currentTarget != NS_GetCurrentThread())) {
    // Always call Cancel() in case we are re-using a timer.  Otherwise
    // the subsequent SetTarget() may fail.
    MOZ_ALWAYS_SUCCEEDS(mTimer->Cancel());
    MOZ_ALWAYS_SUCCEEDS(mTimer->SetTarget(aTarget));
  }

  return mTimer->InitWithNameableFuncCallback(
    TimerCallback, this, aDelay, nsITimer::TYPE_ONE_SHOT, TimerNameCallback);
}

// Return true if this timeout has a refcount of aCount. This is used to check
// that dummy_timeout doesn't leak from nsGlobalWindow::RunTimeout.
#ifdef DEBUG
bool
Timeout::HasRefCnt(uint32_t aCount) const
{
  return mRefCnt.get() == aCount;
}
#endif // DEBUG

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
    return;
  }

  // Since we are not frozen we must set a precise mWhen target wakeup
  // time.  Even if we are suspended we want to use this target time so
  // that it appears time passes while suspended.
  mWhen = aBaseTime + aDelay;
  mTimeRemaining = TimeDuration(0);
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

} // namespace dom
} // namespace mozilla
