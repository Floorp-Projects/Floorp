/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
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
 * The Original Code is nsRefreshDriver.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*
 * Code to notify things that animate before a refresh, at an appropriate
 * refresh rate.  (Perhaps temporary, until replaced by compositor.)
 */

#include "nsRefreshDriver.h"
#include "nsPresContext.h"
#include "nsComponentManagerUtils.h"
#include "prlog.h"
#include "nsAutoPtr.h"
#include "nsCSSFrameConstructor.h"
#include "nsIDocument.h"
#include "nsGUIEvent.h"
#include "nsEventDispatcher.h"
#include "jsapi.h"
#include "nsContentUtils.h"
#include "mozilla/Preferences.h"

using mozilla::TimeStamp;
using mozilla::TimeDuration;

using namespace mozilla;

#define DEFAULT_FRAME_RATE 60
#define DEFAULT_THROTTLED_FRAME_RATE 1

static bool sPrecisePref;

/* static */ void
nsRefreshDriver::InitializeStatics()
{
  Preferences::AddBoolVarCache(&sPrecisePref,
                               "layout.frame_rate.precise",
                               PR_FALSE);
}
// Compute the interval to use for the refresh driver timer, in
// milliseconds
PRInt32
nsRefreshDriver::GetRefreshTimerInterval() const
{
  const char* prefName =
    mThrottled ? "layout.throttled_frame_rate" : "layout.frame_rate";
  PRInt32 rate = Preferences::GetInt(prefName, -1);
  if (rate <= 0) {
    // TODO: get the rate from the platform
    rate = mThrottled ? DEFAULT_THROTTLED_FRAME_RATE : DEFAULT_FRAME_RATE;
  }
  NS_ASSERTION(rate > 0, "Must have positive rate here");
  PRInt32 interval = NSToIntRound(1000.0/rate);
  if (mThrottled) {
    interval = NS_MAX(interval, mLastTimerInterval * 2);
  }
  mLastTimerInterval = interval;
  return interval;
}

PRInt32
nsRefreshDriver::GetRefreshTimerType() const
{
  if (mThrottled) {
    return nsITimer::TYPE_ONE_SHOT;
  }
  if (HaveAnimationFrameListeners() || sPrecisePref) {
    return nsITimer::TYPE_REPEATING_PRECISE_CAN_SKIP;
  }
  return nsITimer::TYPE_REPEATING_SLACK;
}

nsRefreshDriver::nsRefreshDriver(nsPresContext *aPresContext)
  : mPresContext(aPresContext),
    mFrozen(false),
    mThrottled(false),
    mTestControllingRefreshes(false),
    mTimerIsPrecise(false),
    mLastTimerInterval(0)
{
}

nsRefreshDriver::~nsRefreshDriver()
{
  NS_ABORT_IF_FALSE(ObserverCount() == 0,
                    "observers should have unregistered");
  NS_ABORT_IF_FALSE(!mTimer, "timer should be gone");
}

// Method for testing.  See nsIDOMWindowUtils.advanceTimeAndRefresh
// for description.
void
nsRefreshDriver::AdvanceTimeAndRefresh(PRInt64 aMilliseconds)
{
  mTestControllingRefreshes = true;
  mMostRecentRefreshEpochTime += aMilliseconds * 1000;
  mMostRecentRefresh += TimeDuration::FromMilliseconds(aMilliseconds);
  nsCxPusher pusher;
  if (pusher.PushNull()) {
    Notify(nsnull);
    pusher.Pop();
  }
}

void
nsRefreshDriver::RestoreNormalRefresh()
{
  mTestControllingRefreshes = false;
  nsCxPusher pusher;
  if (pusher.PushNull()) {
    Notify(nsnull); // will call UpdateMostRecentRefresh()
    pusher.Pop();
  }
}

TimeStamp
nsRefreshDriver::MostRecentRefresh() const
{
  const_cast<nsRefreshDriver*>(this)->EnsureTimerStarted(false);

  return mMostRecentRefresh;
}

PRInt64
nsRefreshDriver::MostRecentRefreshEpochTime() const
{
  const_cast<nsRefreshDriver*>(this)->EnsureTimerStarted(false);

  return mMostRecentRefreshEpochTime;
}

bool
nsRefreshDriver::AddRefreshObserver(nsARefreshObserver *aObserver,
                                    mozFlushType aFlushType)
{
  ObserverArray& array = ArrayFor(aFlushType);
  bool success = array.AppendElement(aObserver) != nsnull;

  EnsureTimerStarted(false);

  return success;
}

bool
nsRefreshDriver::RemoveRefreshObserver(nsARefreshObserver *aObserver,
                                       mozFlushType aFlushType)
{
  ObserverArray& array = ArrayFor(aFlushType);
  return array.RemoveElement(aObserver);
}

void
nsRefreshDriver::EnsureTimerStarted(bool aAdjustingTimer)
{
  if (mTimer || mFrozen || !mPresContext) {
    // It's already been started, or we don't want to start it now or
    // we've been disconnected.
    return;
  }

  if (!aAdjustingTimer) {
    // If we didn't already have a timer and aAdjustingTimer is false,
    // then we just got our first observer (or an explicit call to
    // MostRecentRefresh by a caller who's likely to add an observer
    // shortly).  This means we should fake a most-recent-refresh time
    // of now so that said observer gets a reasonable refresh time, so
    // things behave as though the timer had always been running.
    UpdateMostRecentRefresh();
  }

  mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  if (!mTimer) {
    return;
  }

  PRInt32 timerType = GetRefreshTimerType();
  mTimerIsPrecise = (timerType == nsITimer::TYPE_REPEATING_PRECISE_CAN_SKIP);

  nsresult rv = mTimer->InitWithCallback(this,
                                         GetRefreshTimerInterval(),
                                         timerType);
  if (NS_FAILED(rv)) {
    mTimer = nsnull;
  }
}

void
nsRefreshDriver::StopTimer()
{
  if (!mTimer) {
    return;
  }

  mTimer->Cancel();
  mTimer = nsnull;
}

PRUint32
nsRefreshDriver::ObserverCount() const
{
  PRUint32 sum = 0;
  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(mObservers); ++i) {
    sum += mObservers[i].Length();
  }
  // Even while throttled, we need to process layout and style changes.  Style
  // changes can trigger transitions which fire events when they complete, and
  // layout changes can affect media queries on child documents, triggering
  // style changes, etc.
  sum += mStyleFlushObservers.Length();
  sum += mLayoutFlushObservers.Length();
  sum += mBeforePaintTargets.Length();
  sum += mAnimationFrameListenerDocs.Length();
  return sum;
}

void
nsRefreshDriver::UpdateMostRecentRefresh()
{
  if (mTestControllingRefreshes) {
    return;
  }

  // Call JS_Now first, since that can have nonzero latency in some rare cases.
  mMostRecentRefreshEpochTime = JS_Now();
  mMostRecentRefresh = TimeStamp::Now();
}

nsRefreshDriver::ObserverArray&
nsRefreshDriver::ArrayFor(mozFlushType aFlushType)
{
  switch (aFlushType) {
    case Flush_Style:
      return mObservers[0];
    case Flush_Layout:
      return mObservers[1];
    case Flush_Display:
      return mObservers[2];
    default:
      NS_ABORT_IF_FALSE(PR_FALSE, "bad flush type");
      return *static_cast<ObserverArray*>(nsnull);
  }
}

/*
 * nsISupports implementation
 */

NS_IMPL_ISUPPORTS1(nsRefreshDriver, nsITimerCallback)

/*
 * nsITimerCallback implementation
 */

NS_IMETHODIMP
nsRefreshDriver::Notify(nsITimer *aTimer)
{
  NS_PRECONDITION(!mFrozen, "Why are we notified while frozen?");
  NS_PRECONDITION(mPresContext, "Why are we notified after disconnection?");
  NS_PRECONDITION(!nsContentUtils::GetCurrentJSContext(),
                  "Shouldn't have a JSContext on the stack");

  if (mTestControllingRefreshes && aTimer) {
    // Ignore real refreshes from our timer (but honor the others).
    return NS_OK;
  }

  UpdateMostRecentRefresh();

  nsCOMPtr<nsIPresShell> presShell = mPresContext->GetPresShell();
  if (!presShell || ObserverCount() == 0) {
    // Things are being destroyed, or we no longer have any observers.
    // We don't want to stop the timer when observers are initially
    // removed, because sometimes observers can be added and removed
    // often depending on what other things are going on and in that
    // situation we don't want to thrash our timer.  So instead we
    // wait until we get a Notify() call when we have no observers
    // before stopping the timer.
    StopTimer();
    return NS_OK;
  }

  /*
   * The timer holds a reference to |this| while calling |Notify|.
   * However, implementations of |WillRefresh| are permitted to destroy
   * the pres context, which will cause our |mPresContext| to become
   * null.  If this happens, we must stop notifying observers.
   */
  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(mObservers); ++i) {
    ObserverArray::EndLimitedIterator etor(mObservers[i]);
    while (etor.HasMore()) {
      nsRefPtr<nsARefreshObserver> obs = etor.GetNext();
      obs->WillRefresh(mMostRecentRefresh);
      
      if (!mPresContext || !mPresContext->GetPresShell()) {
        StopTimer();
        return NS_OK;
      }
    }
    if (i == 0) {
      // Don't just loop while we have things in mBeforePaintTargets,
      // the whole point is that event handlers should readd the
      // target as needed.
      nsTArray< nsCOMPtr<nsIDocument> > targets;
      targets.SwapElements(mBeforePaintTargets);
      for (PRUint32 i = 0; i < targets.Length(); ++i) {
        targets[i]->BeforePaintEventFiring();
      }

      // Also grab all of our animation frame listeners up front.
      nsIDocument::AnimationListenerList animationListeners;
      for (PRUint32 i = 0; i < mAnimationFrameListenerDocs.Length(); ++i) {
        mAnimationFrameListenerDocs[i]->
          TakeAnimationFrameListeners(animationListeners);
      }
      // OK, now reset mAnimationFrameListenerDocs so they can be
      // readded as needed.
      mAnimationFrameListenerDocs.Clear();

      PRInt64 eventTime = mMostRecentRefreshEpochTime / PR_USEC_PER_MSEC;
      for (PRUint32 i = 0; i < targets.Length(); ++i) {
        nsEvent ev(PR_TRUE, NS_BEFOREPAINT);
        ev.time = eventTime;
        nsEventDispatcher::Dispatch(targets[i], nsnull, &ev);
      }

      for (PRUint32 i = 0; i < animationListeners.Length(); ++i) {
        animationListeners[i]->OnBeforePaint(eventTime);
      }

      // This is the Flush_Style case.
      if (mPresContext && mPresContext->GetPresShell()) {
        nsAutoTArray<nsIPresShell*, 16> observers;
        observers.AppendElements(mStyleFlushObservers);
        for (PRUint32 j = observers.Length();
             j && mPresContext && mPresContext->GetPresShell(); --j) {
          // Make sure to not process observers which might have been removed
          // during previous iterations.
          nsIPresShell* shell = observers[j - 1];
          if (!mStyleFlushObservers.Contains(shell))
            continue;
          NS_ADDREF(shell);
          mStyleFlushObservers.RemoveElement(shell);
          shell->FrameConstructor()->mObservingRefreshDriver = PR_FALSE;
          shell->FlushPendingNotifications(Flush_Style);
          NS_RELEASE(shell);
        }
      }
    } else if  (i == 1) {
      // This is the Flush_Layout case.
      if (mPresContext && mPresContext->GetPresShell()) {
        nsAutoTArray<nsIPresShell*, 16> observers;
        observers.AppendElements(mLayoutFlushObservers);
        for (PRUint32 j = observers.Length();
             j && mPresContext && mPresContext->GetPresShell(); --j) {
          // Make sure to not process observers which might have been removed
          // during previous iterations.
          nsIPresShell* shell = observers[j - 1];
          if (!mLayoutFlushObservers.Contains(shell))
            continue;
          NS_ADDREF(shell);
          mLayoutFlushObservers.RemoveElement(shell);
          shell->mReflowScheduled = PR_FALSE;
          shell->mSuppressInterruptibleReflows = PR_FALSE;
          shell->FlushPendingNotifications(Flush_InterruptibleLayout);
          NS_RELEASE(shell);
        }
      }
    }
  }

  if (mThrottled ||
      (mTimerIsPrecise !=
       (GetRefreshTimerType() == nsITimer::TYPE_REPEATING_PRECISE_CAN_SKIP))) {
    // Stop the timer now and restart it here.  Stopping is in the mThrottled
    // case ok because either it's already one-shot, and it just fired, and all
    // we need to do is null it out, or it's repeating and we need to reset it
    // to be one-shot.  Stopping and restarting in the case when we need to
    // switch from precise to slack timers or vice versa is unfortunately
    // required.

    // Note that the EnsureTimerStarted() call here is ok because
    // EnsureTimerStarted makes sure to not start the timer if it shouldn't be
    // started.
    StopTimer();
    EnsureTimerStarted(true);
  }

  return NS_OK;
}

void
nsRefreshDriver::Freeze()
{
  NS_ASSERTION(!mFrozen, "Freeze called on already-frozen refresh driver");
  StopTimer();
  mFrozen = true;
}

void
nsRefreshDriver::Thaw()
{
  NS_ASSERTION(mFrozen, "Thaw called on an unfrozen refresh driver");
  mFrozen = false;
  if (ObserverCount()) {
    // FIXME: This isn't quite right, since our EnsureTimerStarted call
    // updates our mMostRecentRefresh, but the DoRefresh call won't run
    // and notify our observers until we get back to the event loop.
    // Thus MostRecentRefresh() will lie between now and the DoRefresh.
    NS_DispatchToCurrentThread(NS_NewRunnableMethod(this, &nsRefreshDriver::DoRefresh));
    EnsureTimerStarted(false);
  }
}

void
nsRefreshDriver::SetThrottled(bool aThrottled)
{
  if (aThrottled != mThrottled) {
    mThrottled = aThrottled;
    if (mTimer) {
      // We want to switch our timer type here, so just stop and
      // restart the timer.
      StopTimer();
      EnsureTimerStarted(true);
    }
  }
}

void
nsRefreshDriver::DoRefresh()
{
  // Don't do a refresh unless we're in a state where we should be refreshing.
  if (!mFrozen && mPresContext && mTimer) {
    Notify(nsnull);
  }
}

#ifdef DEBUG
bool
nsRefreshDriver::IsRefreshObserver(nsARefreshObserver *aObserver,
                                   mozFlushType aFlushType)
{
  ObserverArray& array = ArrayFor(aFlushType);
  return array.Contains(aObserver);
}
#endif

bool
nsRefreshDriver::ScheduleBeforePaintEvent(nsIDocument* aDocument)
{
  NS_ASSERTION(mBeforePaintTargets.IndexOf(aDocument) ==
               mBeforePaintTargets.NoIndex,
               "Shouldn't have a paint event posted for this document");
  bool appended = mBeforePaintTargets.AppendElement(aDocument) != nsnull;
  EnsureTimerStarted(false);
  return appended;
}

void
nsRefreshDriver::ScheduleAnimationFrameListeners(nsIDocument* aDocument)
{
  NS_ASSERTION(mAnimationFrameListenerDocs.IndexOf(aDocument) ==
               mAnimationFrameListenerDocs.NoIndex,
               "Don't schedule the same document multiple times");
  mAnimationFrameListenerDocs.AppendElement(aDocument);
  // No need to worry about restarting our timer in precise mode if it's
  // already running; that will happen automatically when it fires.
  EnsureTimerStarted(false);
}

void
nsRefreshDriver::RevokeBeforePaintEvent(nsIDocument* aDocument)
{
  mBeforePaintTargets.RemoveElement(aDocument);
}

void
nsRefreshDriver::RevokeAnimationFrameListeners(nsIDocument* aDocument)
{
  mAnimationFrameListenerDocs.RemoveElement(aDocument);
  // No need to worry about restarting our timer in slack mode if it's already
  // running; that will happen automatically when it fires.
}
