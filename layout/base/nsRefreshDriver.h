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
#include "mozilla/Vector.h"

#include "mozFlushType.h"
#include "nsTObserverArray.h"
#include "nsTArray.h"
#include "nsTHashtable.h"
#include "nsTObserverArray.h"
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
class nsIDOMEvent;
class nsINode;

namespace mozilla {
class RefreshDriverTimer;
namespace layout {
class VsyncChild;
} // namespace layout
} // namespace mozilla

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

class nsRefreshDriver final : public mozilla::layers::TransactionIdAllocator,
                              public nsARefreshObserver
{
public:
  explicit nsRefreshDriver(nsPresContext *aPresContext);
  ~nsRefreshDriver();

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
    EnsureTimerStarted();

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
    EnsureTimerStarted();
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
    EnsureTimerStarted();
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
   * Add a document for which we have FrameRequestCallbacks
   */
  void ScheduleFrameRequestCallbacks(nsIDocument* aDocument);

  /**
   * Remove a document for which we have FrameRequestCallbacks
   */
  void RevokeFrameRequestCallbacks(nsIDocument* aDocument);

  /**
   * Queue a new event to dispatch in next tick before the style flush
   */
  void ScheduleEventDispatch(nsINode* aTarget, nsIDOMEvent* aEvent);

  /**
   * Cancel all pending events scheduled by ScheduleEventDispatch which
   * targets any node in aDocument.
   */
  void CancelPendingEvents(nsIDocument* aDocument);

  /**
   * Schedule a frame visibility update "soon", subject to the heuristics and
   * throttling we apply to visibility updates.
   */
  void ScheduleFrameVisibilityUpdate() { mNeedToRecomputeVisibility = true; }

  /**
   * Tell the refresh driver that it is done driving refreshes and
   * should stop its timer and forget about its pres context.  This may
   * be called from within a refresh.
   */
  void Disconnect();

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

  /**
   * PBackgroundChild actor is created asynchronously in content process.
   * We can't create vsync-based timers during PBackground startup. This
   * function will be called when PBackgroundChild actor is created. Then we can
   * do the pending vsync-based timer creation.
   */
  static void PVsyncActorCreated(mozilla::layout::VsyncChild* aVsyncChild);

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

  void SetIsResizeSuppressed() { mResizeSuppressed = true; }
  bool IsResizeSuppressed() const { return mResizeSuppressed; }

  /**
   * The latest value of process-wide jank levels.
   *
   * For each i, sJankLevels[i] counts the number of times delivery of
   * vsync to the main thread has been delayed by at least 2^i
   * ms. This data structure has been designed to make it easy to
   * determine how much jank has taken place between two instants in
   * time.
   *
   * Return `false` if `aJank` needs to be grown to accomodate the
   * data but we didn't have enough memory.
   */
  static bool GetJankLevels(mozilla::Vector<uint64_t>& aJank);

  // mozilla::layers::TransactionIdAllocator
  uint64_t GetTransactionId() override;
  uint64_t LastTransactionId() const override;
  void NotifyTransactionCompleted(uint64_t aTransactionId) override;
  void RevokeTransactionId(uint64_t aTransactionId) override;
  mozilla::TimeStamp GetTransactionStart() override;

  bool IsWaitingForPaint(mozilla::TimeStamp aTime);

  // nsARefreshObserver
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void) override { return TransactionIdAllocator::AddRef(); }
  NS_IMETHOD_(MozExternalRefCountType) Release(void) override { return TransactionIdAllocator::Release(); }
  virtual void WillRefresh(mozilla::TimeStamp aTime) override;

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

  void DispatchPendingEvents();
  void DispatchAnimationEvents();
  void RunFrameRequestCallbacks(mozilla::TimeStamp aNowTime);

  void Tick(int64_t aNowEpoch, mozilla::TimeStamp aNowTime);

  enum EnsureTimerStartedFlags {
    eNone = 0,
    eForceAdjustTimer = 1 << 0,
    eAllowTimeToGoBackwards = 1 << 1,
    eNeverAdjustTimer = 1 << 2,
  };
  void EnsureTimerStarted(EnsureTimerStartedFlags aFlags = eNone);
  void StopTimer();

  uint32_t ObserverCount() const;
  uint32_t ImageRequestCount() const;
  ObserverArray& ArrayFor(mozFlushType aFlushType);
  // Trigger a refresh immediately, if haven't been disconnected or frozen.
  void DoRefresh();

  double GetRefreshTimerInterval() const;
  double GetRegularTimerInterval(bool *outIsDefault = nullptr) const;
  static double GetThrottledTimerInterval();

  static mozilla::TimeDuration GetMinRecomputeVisibilityInterval();

  bool HaveFrameRequestCallbacks() const {
    return mFrameRequestCallbackDocs.Length() != 0;
  }

  void FinishedWaitingForTransaction();

  mozilla::RefreshDriverTimer* ChooseTimer() const;
  mozilla::RefreshDriverTimer* mActiveTimer;

  ProfilerBacktrace* mReflowCause;
  ProfilerBacktrace* mStyleCause;

  nsPresContext *mPresContext; // weak; pres context passed in constructor
                               // and unset in Disconnect

  RefPtr<nsRefreshDriver> mRootRefresh;

  // The most recently allocated transaction id.
  uint64_t mPendingTransaction;
  // The most recently completed transaction id.
  uint64_t mCompletedTransaction;

  uint32_t mFreezeCount;

  // How long we wait between ticks for throttled (which generally means
  // non-visible) documents registered with a non-throttled refresh driver.
  const mozilla::TimeDuration mThrottledFrameRequestInterval;

  // How long we wait, at a minimum, before recomputing approximate frame
  // visibility information. This is a minimum because, regardless of this
  // interval, we only recompute visibility when we've seen a layout or style
  // flush since the last time we did it.
  const mozilla::TimeDuration mMinRecomputeVisibilityInterval;

  bool mThrottled;
  bool mNeedToRecomputeVisibility;
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

  // True if view managers should delay any resize request until the
  // next tick by the refresh driver. This flag will be reset at the
  // start of every tick.
  bool mResizeSuppressed;

  int64_t mMostRecentRefreshEpochTime;
  mozilla::TimeStamp mMostRecentRefresh;
  mozilla::TimeStamp mMostRecentTick;
  mozilla::TimeStamp mTickStart;
  mozilla::TimeStamp mNextThrottledFrameRequestTick;
  mozilla::TimeStamp mNextRecomputeVisibilityTick;

  // separate arrays for each flush type we support
  ObserverArray mObservers[3];
  RequestTable mRequests;
  ImageStartTable mStartTable;

  struct PendingEvent {
    nsCOMPtr<nsINode> mTarget;
    nsCOMPtr<nsIDOMEvent> mEvent;
  };

  AutoTArray<nsIPresShell*, 16> mStyleFlushObservers;
  AutoTArray<nsIPresShell*, 16> mLayoutFlushObservers;
  AutoTArray<nsIPresShell*, 16> mPresShellsToInvalidateIfHidden;
  // nsTArray on purpose, because we want to be able to swap.
  nsTArray<nsIDocument*> mFrameRequestCallbackDocs;
  nsTArray<nsIDocument*> mThrottledFrameRequestCallbackDocs;
  nsTObserverArray<nsAPostRefreshObserver*> mPostRefreshObservers;
  nsTArray<PendingEvent> mPendingEvents;

  void BeginRefreshingImages(RequestTable& aEntries,
                             mozilla::TimeStamp aDesired);

  friend class mozilla::RefreshDriverTimer;

  // turn on or turn off high precision based on various factors
  void ConfigureHighPrecision();
  void SetHighPrecisionTimersEnabled(bool aEnable);

  static void Shutdown();

  // `true` if we are currently in jank-critical mode.
  //
  // In jank-critical mode, any iteration of the event loop that takes
  // more than 16ms to compute will cause an ongoing animation to miss
  // frames.
  //
  // For simplicity, the current implementation assumes that we are
  // in jank-critical mode if and only if the vsync driver has at least
  // one observer.
  static bool IsJankCritical();
};

#endif /* !defined(nsRefreshDriver_h_) */
