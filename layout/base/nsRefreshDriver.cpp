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

using mozilla::TimeStamp;

#define DEFAULT_FRAME_RATE 60
#define DEFAULT_THROTTLED_FRAME_RATE 1

// Compute the interval to use for the refresh driver timer, in
// milliseconds
static PRInt32
GetRefreshTimerInterval(bool aThrottled)
{
  const char* prefName =
    aThrottled ? "layout.throttled_frame_rate" : "layout.frame_rate";
  PRInt32 rate = nsContentUtils::GetIntPref(prefName, -1);
  if (rate <= 0) {
    // TODO: get the rate from the platform
    rate = aThrottled ? DEFAULT_THROTTLED_FRAME_RATE : DEFAULT_FRAME_RATE;
  }
  NS_ASSERTION(rate > 0, "Must have positive rate here");
  return NSToIntRound(1000.0/rate);
}

static PRInt32
GetRefreshTimerType()
{
  PRBool precise =
    nsContentUtils::GetBoolPref("layout.frame_rate.precise", PR_FALSE);
  return precise ? (PRInt32)nsITimer::TYPE_REPEATING_PRECISE
                 : (PRInt32)nsITimer::TYPE_REPEATING_SLACK;
}

nsRefreshDriver::nsRefreshDriver(nsPresContext *aPresContext)
  : mPresContext(aPresContext),
    mFrozen(false),
    mThrottled(false)
{
}

nsRefreshDriver::~nsRefreshDriver()
{
  NS_ABORT_IF_FALSE(ObserverCount() == 0,
                    "observers should have unregistered");
  NS_ABORT_IF_FALSE(!mTimer, "timer should be gone");
}

TimeStamp
nsRefreshDriver::MostRecentRefresh() const
{
  const_cast<nsRefreshDriver*>(this)->EnsureTimerStarted();

  return mMostRecentRefresh;
}

PRInt64
nsRefreshDriver::MostRecentRefreshEpochTime() const
{
  const_cast<nsRefreshDriver*>(this)->EnsureTimerStarted();

  return mMostRecentRefreshEpochTime;
}

PRBool
nsRefreshDriver::AddRefreshObserver(nsARefreshObserver *aObserver,
                                    mozFlushType aFlushType)
{
  ObserverArray& array = ArrayFor(aFlushType);
  PRBool success = array.AppendElement(aObserver) != nsnull;

  EnsureTimerStarted();

  return success;
}

PRBool
nsRefreshDriver::RemoveRefreshObserver(nsARefreshObserver *aObserver,
                                       mozFlushType aFlushType)
{
  ObserverArray& array = ArrayFor(aFlushType);
  return array.RemoveElement(aObserver);
}

void
nsRefreshDriver::EnsureTimerStarted()
{
  if (mTimer || mFrozen || !mPresContext) {
    // It's already been started, or we don't want to start it now or
    // we've been disconnected.
    return;
  }

  UpdateMostRecentRefresh();

  mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  if (!mTimer) {
    return;
  }

  nsresult rv = mTimer->InitWithCallback(this,
                                         GetRefreshTimerInterval(mThrottled),
                                         GetRefreshTimerType());
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
nsRefreshDriver::Notify(nsITimer * /* unused */)
{
  NS_PRECONDITION(!mFrozen, "Why are we notified while frozen?");
  NS_PRECONDITION(mPresContext, "Why are we notified after disconnection?");

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
      nsTArray<nsIDocument*> targets;
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
      while (!mStyleFlushObservers.IsEmpty() &&
             mPresContext && mPresContext->GetPresShell()) {
        PRUint32 idx = mStyleFlushObservers.Length() - 1;
        nsCOMPtr<nsIPresShell> shell = mStyleFlushObservers[idx];
        mStyleFlushObservers.RemoveElementAt(idx);
        shell->FrameConstructor()->mObservingRefreshDriver = PR_FALSE;
        shell->FlushPendingNotifications(Flush_Style);
      }
    } else if  (i == 1) {
      // This is the Flush_Layout case.
      while (!mLayoutFlushObservers.IsEmpty() &&
             mPresContext && mPresContext->GetPresShell()) {
        PRUint32 idx = mLayoutFlushObservers.Length() - 1;
        nsCOMPtr<nsIPresShell> shell = mLayoutFlushObservers[idx];
        mLayoutFlushObservers.RemoveElementAt(idx);
        shell->mReflowScheduled = PR_FALSE;
        shell->mSuppressInterruptibleReflows = PR_FALSE;
        shell->FlushPendingNotifications(Flush_InterruptibleLayout);
      }
    }
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
    NS_DispatchToCurrentThread(NS_NewRunnableMethod(this, &nsRefreshDriver::DoRefresh));
    EnsureTimerStarted();
  }
}

void
nsRefreshDriver::SetThrottled(bool aThrottled)
{
  if (aThrottled != mThrottled) {
    mThrottled = aThrottled;
    if (mTimer) {
      // Stopping and restarting the timer would update our most recent refresh
      // time, which isn't quite right.  Luckily, we can just reschedule the
      // timer.
      mTimer->SetDelay(GetRefreshTimerInterval(mThrottled));
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
PRBool
nsRefreshDriver::IsRefreshObserver(nsARefreshObserver *aObserver,
                                   mozFlushType aFlushType)
{
  ObserverArray& array = ArrayFor(aFlushType);
  return array.Contains(aObserver);
}
#endif

PRBool
nsRefreshDriver::ScheduleBeforePaintEvent(nsIDocument* aDocument)
{
  NS_ASSERTION(mBeforePaintTargets.IndexOf(aDocument) ==
               mBeforePaintTargets.NoIndex,
               "Shouldn't have a paint event posted for this document");
  PRBool appended = mBeforePaintTargets.AppendElement(aDocument) != nsnull;
  EnsureTimerStarted();
  return appended;
}

void
nsRefreshDriver::ScheduleAnimationFrameListeners(nsIDocument* aDocument)
{
  NS_ASSERTION(mAnimationFrameListenerDocs.IndexOf(aDocument) ==
               mAnimationFrameListenerDocs.NoIndex,
               "Don't schedule the same document multiple times");
  mAnimationFrameListenerDocs.AppendElement(aDocument);
  EnsureTimerStarted();
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
}
