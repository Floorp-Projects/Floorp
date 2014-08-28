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
#include "nsTObserverArray.h"
#include "nsTArray.h"
#include "nsTHashtable.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "GeckoProfiler.h"
#include "mozilla/layers/TransactionIdAllocator.h"

class nsPresContext;
class nsIPresShell;
class nsIDocument;
class imgIRequest;
class nsIRunnable;

namespace mozilla {
class RefreshDriverTimer;
}

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
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void) = 0;
  NS_IMETHOD_(MozExternalRefCountType) Release(void) = 0;

  virtual void WillRefresh(mozilla::TimeStamp aTime) = 0;
};

/**
 * An abstract base class to be implemented by callers wanting to be notified
 * that a refresh has occurred. Callers must ensure an observer is removed
 * before it is destroyed.
 */
class nsAPostRefreshObserver {
public:
  virtual void DidRefresh() = 0;
};

class nsRefreshDriver MOZ_FINAL : public mozilla::layers::TransactionIdAllocator,
                                  public nsARefreshObserver {
public:
  explicit nsRefreshDriver(nsPresContext *aPresContext);
  ~nsRefreshDriver();

  static void InitializeStatics();
  static void Shutdown();

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
   *
   * The observer will be called even if there is no other activity.
   */
  bool AddRefreshObserver(nsARefreshObserver *aObserver,
                          mozFlushType aFlushType);
  bool RemoveRefreshObserver(nsARefreshObserver *aObserver,
                             mozFlushType aFlushType);

  /**
   * Add an observer that will be called after each refresh. The caller
   * must remove the observer before it is deleted. This does not trigger
   * refresh driver ticks.
   */
  void AddPostRefreshObserver(nsAPostRefreshObserver *aObserver);
  void RemovePostRefreshObserver(nsAPostRefreshObserver *aObserver);

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
    // We only get the cause for the first observer each frame because capturing
    // a stack is expensive. This is still useful if (1) you're trying to remove
    // all flushes for a particial frame or (2) the costly flush is triggered
    // near the call site where the first observer is triggered.
    if (!mStyleCause) {
      mStyleCause = profiler_get_backtrace();
    }
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
    // We only get the cause for the first observer each frame because capturing
    // a stack is expensive. This is still useful if (1) you're trying to remove
    // all flushes for a particial frame or (2) the costly flush is triggered
    // near the call site where the first observer is triggered.
    if (!mReflowCause) {
      mReflowCause = profiler_get_backtrace();
    }
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

  bool IsFrozen() { return mFreezeCount > 0; }

  /**
   * Freeze the refresh driver.  It should stop delivering future
   * refreshes until thawed. Note that the number of calls to Freeze() must
   * match the number of calls to Thaw() in order for the refresh driver to
   * be un-frozen.
   */
  void Freeze();

  /**
   * Thaw the refresh driver.  If the number of calls to Freeze() matches the
   * number of calls to this function, the refresh driver should start
   * delivering refreshes again.
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

  bool IsInRefresh() { return mInRefresh; }

  // mozilla::layers::TransactionIdAllocator
  virtual uint64_t GetTransactionId() MOZ_OVERRIDE;
  void NotifyTransactionCompleted(uint64_t aTransactionId) MOZ_OVERRIDE;
  void RevokeTransactionId(uint64_t aTransactionId) MOZ_OVERRIDE;
  mozilla::TimeStamp GetTransactionStart() MOZ_OVERRIDE;

  bool IsWaitingForPaint(mozilla::TimeStamp aTime);

  // nsARefreshObserver
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void) { return TransactionIdAllocator::AddRef(); }
  NS_IMETHOD_(MozExternalRefCountType) Release(void) { return TransactionIdAllocator::Release(); }
  virtual void WillRefresh(mozilla::TimeStamp aTime);
private:
  typedef nsTObserverArray<nsARefreshObserver*> ObserverArray;
  typedef nsTHashtable<nsISupportsHashKey> RequestTable;
  struct ImageStartData {
    ImageStartData()
    {
    }

    mozilla::Maybe<mozilla::TimeStamp> mStartTime;
    RequestTable mEntries;
  };
  typedef nsClassHashtable<nsUint32HashKey, ImageStartData> ImageStartTable;

  void Tick(int64_t aNowEpoch, mozilla::TimeStamp aNowTime);

  void EnsureTimerStarted(bool aAdjustingTimer);
  void StopTimer();

  uint32_t ObserverCount() const;
  uint32_t ImageRequestCount() const;
  static PLDHashOperator ImageRequestEnumerator(nsISupportsHashKey* aEntry,
                                                void* aUserArg);
  static PLDHashOperator StartTableRequestCounter(const uint32_t& aKey,
                                                  ImageStartData* aEntry,
                                                  void* aUserArg);
  static PLDHashOperator StartTableRefresh(const uint32_t& aKey,
                                           ImageStartData* aEntry,
                                           void* aUserArg);
  static PLDHashOperator BeginRefreshingImages(nsISupportsHashKey* aEntry,
                                               void* aUserArg);
  ObserverArray& ArrayFor(mozFlushType aFlushType);
  // Trigger a refresh immediately, if haven't been disconnected or frozen.
  void DoRefresh();

  double GetRefreshTimerInterval() const;
  double GetRegularTimerInterval(bool *outIsDefault = nullptr) const;
  double GetThrottledTimerInterval() const;

  bool HaveFrameRequestCallbacks() const {
    return mFrameRequestCallbackDocs.Length() != 0;
  }

  void FinishedWaitingForTransaction();

  mozilla::RefreshDriverTimer* ChooseTimer() const;
  mozilla::RefreshDriverTimer *mActiveTimer;

  ProfilerBacktrace* mReflowCause;
  ProfilerBacktrace* mStyleCause;

  nsPresContext *mPresContext; // weak; pres context passed in constructor
                               // and unset in Disconnect

  nsRefPtr<nsRefreshDriver> mRootRefresh;

  // The most recently allocated transaction id.
  uint64_t mPendingTransaction;
  // The most recently completed transaction id.
  uint64_t mCompletedTransaction;

  uint32_t mFreezeCount;
  bool mThrottled;
  bool mTestControllingRefreshes;
  bool mViewManagerFlushIsPending;
  bool mRequestedHighPrecision;
  bool mInRefresh;

  // True if the refresh driver is suspended waiting for transaction
  // id's to be returned and shouldn't do any work during Tick().
  bool mWaitingForTransaction;
  // True if Tick() was skipped because of mWaitingForTransaction and
  // we should schedule a new Tick immediately when resumed instead
  // of waiting until the next interval.
  bool mSkippedPaints;

  int64_t mMostRecentRefreshEpochTime;
  mozilla::TimeStamp mMostRecentRefresh;
  mozilla::TimeStamp mMostRecentTick;
  mozilla::TimeStamp mTickStart;

  // separate arrays for each flush type we support
  ObserverArray mObservers[3];
  RequestTable mRequests;
  ImageStartTable mStartTable;

  nsAutoTArray<nsIPresShell*, 16> mStyleFlushObservers;
  nsAutoTArray<nsIPresShell*, 16> mLayoutFlushObservers;
  nsAutoTArray<nsIPresShell*, 16> mPresShellsToInvalidateIfHidden;
  // nsTArray on purpose, because we want to be able to swap.
  nsTArray<nsIDocument*> mFrameRequestCallbackDocs;
  nsTArray<nsAPostRefreshObserver*> mPostRefreshObservers;

  // Helper struct for processing image requests
  struct ImageRequestParameters {
    mozilla::TimeStamp mCurrent;
    mozilla::TimeStamp mPrevious;
    RequestTable* mRequests;
    mozilla::TimeStamp mDesired;
  };

  friend class mozilla::RefreshDriverTimer;

  // turn on or turn off high precision based on various factors
  void ConfigureHighPrecision();
  void SetHighPrecisionTimersEnabled(bool aEnable);
};

#endif /* !defined(nsRefreshDriver_h_) */
