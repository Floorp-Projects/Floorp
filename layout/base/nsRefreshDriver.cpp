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

/*
 * TODO:
 * Once this is hooked in to suppressing updates when the presentation
 * is not visible, we need to hook it up to FlushPendingNotifications so
 * that we flush when necessary.
 */

#define REFRESH_INTERVAL_MILLISECONDS 20

using mozilla::TimeStamp;

nsRefreshDriver::nsRefreshDriver()
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
  PRBool success = array.RemoveElement(aObserver);

  if (ObserverCount() == 0) {
    StopTimer();
  }

  return success;
}

void
nsRefreshDriver::EnsureTimerStarted()
{
  if (mTimer) {
    // It's already been started.
    return;
  }

  UpdateMostRecentRefresh();

  mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  if (!mTimer) {
    return;
  }

  nsresult rv = mTimer->InitWithCallback(this, REFRESH_INTERVAL_MILLISECONDS,
                                         nsITimer::TYPE_REPEATING_SLACK);
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
  return sum;
}

void
nsRefreshDriver::UpdateMostRecentRefresh()
{
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

NS_IMPL_ADDREF_USING_AGGREGATOR(nsRefreshDriver,
                                nsPresContext::FromRefreshDriver(this))
NS_IMPL_RELEASE_USING_AGGREGATOR(nsRefreshDriver,
                                 nsPresContext::FromRefreshDriver(this))
NS_IMPL_QUERY_INTERFACE1(nsRefreshDriver, nsITimerCallback)

/*
 * nsITimerCallback implementation
 */

NS_IMETHODIMP
nsRefreshDriver::Notify(nsITimer *aTimer)
{
  UpdateMostRecentRefresh();

  nsPresContext *presContext = nsPresContext::FromRefreshDriver(this);
  nsCOMPtr<nsIPresShell> presShell = presContext->GetPresShell();
  if (!presShell) {
    // Things are being destroyed.
    StopTimer();
    return NS_OK;
  }

  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(mObservers); ++i) {
    ObserverArray::EndLimitedIterator etor(mObservers[i]);
    while (etor.HasMore()) {
      etor.GetNext()->WillRefresh(mMostRecentRefresh);
    }
    if (i == 0) {
      // This is the Flush_Style case.
      // FIXME: Maybe we should only flush if the WillRefresh calls did
      // something?  It's probably ok as-is, though, especially as we
      // hook up more things here (or to the replacement of this class).
      // FIXME: We should probably flush for other sets of observers
      // too.  But we should only flush layout once nsRefreshDriver is
      // the driver for the interruptible layout timer (and we should
      // then Flush_InterruptibleLayout).
      presShell->FlushPendingNotifications(Flush_Style);
    }
  }

  if (ObserverCount() == 0) {
    StopTimer();
  }

  return NS_OK;
}
