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

#ifndef nsRefreshDriver_h_
#define nsRefreshDriver_h_

#include "mozilla/TimeStamp.h"
#include "mozFlushType.h"
#include "nsITimer.h"
#include "nsCOMPtr.h"
#include "nsTObserverArray.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"

class nsPresContext;
class nsIPresShell;
class nsIDocument;

/**
 * An abstract base class to be implemented by callers wanting to be
 * notified at refresh times.  When nothing needs to be painted, callers
 * may not be notified.
 */
class nsARefreshObserver {
public:
  // AddRef and Release signatures that match nsISupports.  Implementors
  // must implement reference counting, and those that do implement
  // nsISupports will already have methods with the correct signature.
  //
  // The refresh driver does NOT hold references to refresh observers
  // except while it is notifying them.
  NS_IMETHOD_(nsrefcnt) AddRef(void) = 0;
  NS_IMETHOD_(nsrefcnt) Release(void) = 0;

  virtual void WillRefresh(mozilla::TimeStamp aTime) = 0;
};

class nsRefreshDriver : public nsITimerCallback {
public:
  nsRefreshDriver(nsPresContext *aPresContext);
  ~nsRefreshDriver();

  // nsISupports implementation
  NS_DECL_ISUPPORTS

  // nsITimerCallback implementation
  NS_DECL_NSITIMERCALLBACK

  /**
   * Return the time of the most recent refresh.  This is intended to be
   * used by callers who want to start an animation now and want to know
   * what time to consider the start of the animation.  (This helps
   * ensure that multiple animations started during the same event off
   * the main event loop have the same start time.)
   */
  mozilla::TimeStamp MostRecentRefresh() const;
  /**
   * Same thing, but in microseconds since the epoch.
   */
  PRInt64 MostRecentRefreshEpochTime() const;

  /**
   * Add / remove refresh observers.  Returns whether the operation
   * succeeded.
   *
   * The flush type affects:
   *   + the order in which the observers are notified (lowest flush
   *     type to highest, in order registered)
   *   + (in the future) which observers are suppressed when the display
   *     doesn't require current position data or isn't currently
   *     painting, and, correspondingly, which get notified when there
   *     is a flush during such suppression
   * and it must be either Flush_Style, Flush_Layout, or Flush_Display.
   *
   * The refresh driver does NOT own a reference to these observers;
   * they must remove themselves before they are destroyed.
   */
  PRBool AddRefreshObserver(nsARefreshObserver *aObserver,
                            mozFlushType aFlushType);
  PRBool RemoveRefreshObserver(nsARefreshObserver *aObserver,
                               mozFlushType aFlushType);

  /**
   * Add / remove presshells that we should flush style and layout on
   */
  PRBool AddStyleFlushObserver(nsIPresShell* aShell) {
    NS_ASSERTION(!mStyleFlushObservers.Contains(aShell),
		 "Double-adding style flush observer");
    PRBool appended = mStyleFlushObservers.AppendElement(aShell) != nsnull;
    EnsureTimerStarted();
    return appended;
  }
  void RemoveStyleFlushObserver(nsIPresShell* aShell) {
    mStyleFlushObservers.RemoveElement(aShell);
  }
  PRBool AddLayoutFlushObserver(nsIPresShell* aShell) {
    NS_ASSERTION(!IsLayoutFlushObserver(aShell),
		 "Double-adding layout flush observer");
    PRBool appended = mLayoutFlushObservers.AppendElement(aShell) != nsnull;
    EnsureTimerStarted();
    return appended;
  }
  void RemoveLayoutFlushObserver(nsIPresShell* aShell) {
    mLayoutFlushObservers.RemoveElement(aShell);
  }
  PRBool IsLayoutFlushObserver(nsIPresShell* aShell) {
    return mLayoutFlushObservers.Contains(aShell);
  }

  /**
   * Add a document for which we should fire a MozBeforePaint event.
   */
  PRBool ScheduleBeforePaintEvent(nsIDocument* aDocument);

  /**
   * Add a document for which we have nsIAnimationFrameListeners
   */
  void ScheduleAnimationFrameListeners(nsIDocument* aDocument);

  /**
   * Remove a document for which we should fire a MozBeforePaint event.
   */
  void RevokeBeforePaintEvent(nsIDocument* aDocument);

  /**
   * Remove a document for which we have nsIAnimationFrameListeners
   */
  void RevokeAnimationFrameListeners(nsIDocument* aDocument);

  /**
   * Tell the refresh driver that it is done driving refreshes and
   * should stop its timer and forget about its pres context.  This may
   * be called from within a refresh.
   */
  void Disconnect() {
    StopTimer();
    mPresContext = nsnull;
  }

  /**
   * Freeze the refresh driver.  It should stop delivering future
   * refreshes until thawed.
   */
  void Freeze();

  /**
   * Thaw the refresh driver.  If needed, it should start delivering
   * refreshes again.
   */
  void Thaw();

  /**
   * Throttle or unthrottle the refresh driver.  This is done if the
   * corresponding presshell is hidden or shown.
   */
  void SetThrottled(bool aThrottled);

  /**
   * Return the prescontext we were initialized with
   */
  nsPresContext* PresContext() const { return mPresContext; }

#ifdef DEBUG
  /**
   * Check whether the given observer is an observer for the given flush type
   */
  PRBool IsRefreshObserver(nsARefreshObserver *aObserver,
			   mozFlushType aFlushType);
#endif

private:
  typedef nsTObserverArray<nsARefreshObserver*> ObserverArray;

  void EnsureTimerStarted();
  void StopTimer();
  PRUint32 ObserverCount() const;
  void UpdateMostRecentRefresh();
  ObserverArray& ArrayFor(mozFlushType aFlushType);
  // Trigger a refresh immediately, if haven't been disconnected or frozen.
  void DoRefresh();

  nsCOMPtr<nsITimer> mTimer;
  mozilla::TimeStamp mMostRecentRefresh; // only valid when mTimer non-null
  PRInt64 mMostRecentRefreshEpochTime;   // same thing as mMostRecentRefresh,
                                         // but in microseconds since the epoch.

  nsPresContext *mPresContext; // weak; pres context passed in constructor
                               // and unset in Disconnect

  bool mFrozen;
  bool mThrottled;

  // separate arrays for each flush type we support
  ObserverArray mObservers[3];
  nsAutoTArray<nsIPresShell*, 16> mStyleFlushObservers;
  nsAutoTArray<nsIPresShell*, 16> mLayoutFlushObservers;
  // nsTArray on purpose, because we want to be able to swap.
  nsTArray<nsIDocument*> mBeforePaintTargets;
  // nsTArray on purpose, because we want to be able to swap.
  nsTArray<nsIDocument*> mAnimationFrameListenerDocs;
};

#endif /* !defined(nsRefreshDriver_h_) */
