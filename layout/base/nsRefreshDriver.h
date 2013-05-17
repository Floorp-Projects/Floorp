/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Code to notify things that animate before a refresh, at an appropriate
 * refresh rate.  (Perhaps temporary, until replaced by compositor.)
 */

#ifndef nsRefreshDriver_h_
#define nsRefreshDriver_h_

#include "mozilla/TimeStamp.h"
#include "mozFlushType.h"
#include "nsCOMPtr.h"
#include "nsTObserverArray.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "mozilla/Attributes.h"

class nsPresContext;
class nsIPresShell;
class nsIDocument;
class imgIRequest;

/**
 * An abstract base class to be implemented by callers wanting to be
 * notified at refresh times.  When nothing needs to be painted, callers
 * may not be notified.
 */
namespace mozilla {
    class RefreshDriverTimer;
}

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

class nsRefreshDriver MOZ_FINAL : public nsISupports {
public:
  nsRefreshDriver(nsPresContext *aPresContext);
  ~nsRefreshDriver();

  static void InitializeStatics();
  static void Shutdown();

  // nsISupports implementation
  NS_DECL_ISUPPORTS

  /**
   * Methods for testing, exposed via nsIDOMWindowUtils.  See
   * nsIDOMWindowUtils.advanceTimeAndRefresh for description.
   */
  void AdvanceTimeAndRefresh(int64_t aMilliseconds);
  void RestoreNormalRefresh();
  void DoTick();
  bool IsTestControllingRefreshesEnabled() const
  {
    return mTestControllingRefreshes;
  }

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
  int64_t MostRecentRefreshEpochTime() const;

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
  bool AddRefreshObserver(nsARefreshObserver *aObserver,
                            mozFlushType aFlushType);
  bool RemoveRefreshObserver(nsARefreshObserver *aObserver,
                               mozFlushType aFlushType);

  /**
   * Add/Remove imgIRequest versions of observers.
   *
   * These are used for hooking into the refresh driver for
   * controlling animated images.
   *
   * @note The refresh driver owns a reference to these listeners.
   *
   * @note Technically, imgIRequest objects are not nsARefreshObservers, but
   * for controlling animated image repaint events, we subscribe the
   * imgIRequests to the nsRefreshDriver for notification of paint events.
   *
   * @returns whether the operation succeeded, or void in the case of removal.
   */
  bool AddImageRequest(imgIRequest* aRequest);
  void RemoveImageRequest(imgIRequest* aRequest);

  /**
   * Add / remove presshells that we should flush style and layout on
   */
  bool AddStyleFlushObserver(nsIPresShell* aShell) {
    NS_ASSERTION(!mStyleFlushObservers.Contains(aShell),
		 "Double-adding style flush observer");
    bool appended = mStyleFlushObservers.AppendElement(aShell) != nullptr;
    EnsureTimerStarted(false);
    return appended;
  }
  void RemoveStyleFlushObserver(nsIPresShell* aShell) {
    mStyleFlushObservers.RemoveElement(aShell);
  }
  bool AddLayoutFlushObserver(nsIPresShell* aShell) {
    NS_ASSERTION(!IsLayoutFlushObserver(aShell),
		 "Double-adding layout flush observer");
    bool appended = mLayoutFlushObservers.AppendElement(aShell) != nullptr;
    EnsureTimerStarted(false);
    return appended;
  }
  void RemoveLayoutFlushObserver(nsIPresShell* aShell) {
    mLayoutFlushObservers.RemoveElement(aShell);
  }
  bool IsLayoutFlushObserver(nsIPresShell* aShell) {
    return mLayoutFlushObservers.Contains(aShell);
  }
  bool AddPresShellToInvalidateIfHidden(nsIPresShell* aShell) {
    NS_ASSERTION(!mPresShellsToInvalidateIfHidden.Contains(aShell),
		 "Double-adding style flush observer");
    bool appended = mPresShellsToInvalidateIfHidden.AppendElement(aShell) != nullptr;
    EnsureTimerStarted(false);
    return appended;
  }
  void RemovePresShellToInvalidateIfHidden(nsIPresShell* aShell) {
    mPresShellsToInvalidateIfHidden.RemoveElement(aShell);
  }

  /**
   * Remember whether our presshell's view manager needs a flush
   */
  void ScheduleViewManagerFlush();
  void RevokeViewManagerFlush() {
    mViewManagerFlushIsPending = false;
  }
  bool ViewManagerFlushIsPending() {
    return mViewManagerFlushIsPending;
  }

  /**
   * Add a document for which we have nsIFrameRequestCallbacks
   */
  void ScheduleFrameRequestCallbacks(nsIDocument* aDocument);

  /**
   * Remove a document for which we have nsIFrameRequestCallbacks
   */
  void RevokeFrameRequestCallbacks(nsIDocument* aDocument);

  /**
   * Tell the refresh driver that it is done driving refreshes and
   * should stop its timer and forget about its pres context.  This may
   * be called from within a refresh.
   */
  void Disconnect() {
    StopTimer();
    mPresContext = nullptr;
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
  bool IsRefreshObserver(nsARefreshObserver *aObserver,
			   mozFlushType aFlushType);
#endif

  /**
   * Default interval the refresh driver uses, in ms.
   */
  static int32_t DefaultInterval();

  /**
   * Enable/disable paint flashing.
   */
  void SetPaintFlashing(bool aPaintFlashing) {
    mPaintFlashing = aPaintFlashing;
  }

  bool GetPaintFlashing() {
    return mPaintFlashing;
  }

private:
  typedef nsTObserverArray<nsARefreshObserver*> ObserverArray;
  typedef nsTHashtable<nsISupportsHashKey> RequestTable;

  void Tick(int64_t aNowEpoch, mozilla::TimeStamp aNowTime);

  void EnsureTimerStarted(bool aAdjustingTimer);
  void StopTimer();

  uint32_t ObserverCount() const;
  uint32_t ImageRequestCount() const;
  static PLDHashOperator ImageRequestEnumerator(nsISupportsHashKey* aEntry,
                                          void* aUserArg);
  ObserverArray& ArrayFor(mozFlushType aFlushType);
  // Trigger a refresh immediately, if haven't been disconnected or frozen.
  void DoRefresh();

  double GetRefreshTimerInterval() const;
  double GetRegularTimerInterval() const;
  double GetThrottledTimerInterval() const;

  bool HaveFrameRequestCallbacks() const {
    return mFrameRequestCallbackDocs.Length() != 0;
  }

  mozilla::RefreshDriverTimer* ChooseTimer() const;
  mozilla::RefreshDriverTimer *mActiveTimer;

  nsPresContext *mPresContext; // weak; pres context passed in constructor
                               // and unset in Disconnect

  bool mFrozen;
  bool mThrottled;
  bool mTestControllingRefreshes;
  bool mViewManagerFlushIsPending;
  bool mRequestedHighPrecision;
  bool mPaintFlashing;

  int64_t mMostRecentRefreshEpochTime;
  mozilla::TimeStamp mMostRecentRefresh;

  // separate arrays for each flush type we support
  ObserverArray mObservers[3];
  RequestTable mRequests;

  nsAutoTArray<nsIPresShell*, 16> mStyleFlushObservers;
  nsAutoTArray<nsIPresShell*, 16> mLayoutFlushObservers;
  nsAutoTArray<nsIPresShell*, 16> mPresShellsToInvalidateIfHidden;
  // nsTArray on purpose, because we want to be able to swap.
  nsTArray<nsIDocument*> mFrameRequestCallbackDocs;

  // Helper struct for processing image requests
  struct ImageRequestParameters {
      mozilla::TimeStamp ts;
  };

  friend class mozilla::RefreshDriverTimer;

  // turn on or turn off high precision based on various factors
  void ConfigureHighPrecision();
  void SetHighPrecisionTimersEnabled(bool aEnable);
};

#endif /* !defined(nsRefreshDriver_h_) */
