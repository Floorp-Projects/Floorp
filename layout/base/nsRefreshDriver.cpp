/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Code to notify things that animate before a refresh, at an appropriate
 * refresh rate.  (Perhaps temporary, until replaced by compositor.)
 */

#include "mozilla/Util.h"

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
#include "nsIViewManager.h"

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
                               false);
}

/* static */ PRInt32
nsRefreshDriver::DefaultInterval()
{
  return NSToIntRound(1000.0 / DEFAULT_FRAME_RATE);
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
  if (HaveFrameRequestCallbacks() || sPrecisePref) {
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
    mViewManagerFlushIsPending(false),
    mLastTimerInterval(0)
{
  mRequests.Init();
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

bool
nsRefreshDriver::AddImageRequest(imgIRequest* aRequest)
{
  if (!mRequests.PutEntry(aRequest)) {
    return false;
  }

  EnsureTimerStarted(false);

  return true;
}

void
nsRefreshDriver::RemoveImageRequest(imgIRequest* aRequest)
{
  mRequests.RemoveEntry(aRequest);
}

void nsRefreshDriver::ClearAllImageRequests()
{
  mRequests.Clear();
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
  for (PRUint32 i = 0; i < ArrayLength(mObservers); ++i) {
    sum += mObservers[i].Length();
  }

  // Even while throttled, we need to process layout and style changes.  Style
  // changes can trigger transitions which fire events when they complete, and
  // layout changes can affect media queries on child documents, triggering
  // style changes, etc.
  sum += mStyleFlushObservers.Length();
  sum += mLayoutFlushObservers.Length();
  sum += mFrameRequestCallbackDocs.Length();
  sum += mViewManagerFlushIsPending;
  return sum;
}

PRUint32
nsRefreshDriver::ImageRequestCount() const
{
  return mRequests.Count();
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
      NS_ABORT_IF_FALSE(false, "bad flush type");
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
  if (!presShell || (ObserverCount() == 0 && ImageRequestCount() == 0)) {
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
  for (PRUint32 i = 0; i < ArrayLength(mObservers); ++i) {
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
      // Grab all of our frame request callbacks up front.
      nsIDocument::FrameRequestCallbackList frameRequestCallbacks;
      for (PRUint32 i = 0; i < mFrameRequestCallbackDocs.Length(); ++i) {
        mFrameRequestCallbackDocs[i]->
          TakeFrameRequestCallbacks(frameRequestCallbacks);
      }
      // OK, now reset mFrameRequestCallbackDocs so they can be
      // readded as needed.
      mFrameRequestCallbackDocs.Clear();

      PRInt64 eventTime = mMostRecentRefreshEpochTime / PR_USEC_PER_MSEC;
      for (PRUint32 i = 0; i < frameRequestCallbacks.Length(); ++i) {
        nsAutoMicroTask mt;
        frameRequestCallbacks[i]->Sample(eventTime);
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
          shell->FrameConstructor()->mObservingRefreshDriver = false;
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
          shell->mReflowScheduled = false;
          shell->mSuppressInterruptibleReflows = false;
          shell->FlushPendingNotifications(Flush_InterruptibleLayout);
          NS_RELEASE(shell);
        }
      }
    }
  }

  /*
   * Perform notification to imgIRequests subscribed to listen
   * for refresh events.
   */

  ImageRequestParameters parms = {mMostRecentRefresh};
  if (mRequests.Count()) {
    mRequests.EnumerateEntries(nsRefreshDriver::ImageRequestEnumerator, &parms);
    EnsureTimerStarted(false);
  }

  if (mViewManagerFlushIsPending) {
    mViewManagerFlushIsPending = false;
    mPresContext->GetPresShell()->GetViewManager()->ProcessPendingUpdates();
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

PLDHashOperator
nsRefreshDriver::ImageRequestEnumerator(nsISupportsHashKey* aEntry,
                                        void* aUserArg)
{
  ImageRequestParameters* parms =
    static_cast<ImageRequestParameters*> (aUserArg);
  mozilla::TimeStamp mostRecentRefresh = parms->ts;
  imgIRequest* req = static_cast<imgIRequest*>(aEntry->GetKey());
  NS_ABORT_IF_FALSE(req, "Unable to retrieve the image request");
  nsCOMPtr<imgIContainer> image;
  req->GetImage(getter_AddRefs(image));
  if (image) {
    image->RequestRefresh(mostRecentRefresh);
  }

  return PL_DHASH_NEXT;
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
  if (ObserverCount() || ImageRequestCount()) {
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

void
nsRefreshDriver::ScheduleFrameRequestCallbacks(nsIDocument* aDocument)
{
  NS_ASSERTION(mFrameRequestCallbackDocs.IndexOf(aDocument) ==
               mFrameRequestCallbackDocs.NoIndex,
               "Don't schedule the same document multiple times");
  mFrameRequestCallbackDocs.AppendElement(aDocument);
  // No need to worry about restarting our timer in precise mode if it's
  // already running; that will happen automatically when it fires.
  EnsureTimerStarted(false);
}

void
nsRefreshDriver::RevokeFrameRequestCallbacks(nsIDocument* aDocument)
{
  mFrameRequestCallbackDocs.RemoveElement(aDocument);
  // No need to worry about restarting our timer in slack mode if it's already
  // running; that will happen automatically when it fires.
}
