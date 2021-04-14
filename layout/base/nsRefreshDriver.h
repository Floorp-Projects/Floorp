/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Code to notify things that animate before a refresh, at an appropriate
 * refresh rate.  (Perhaps temporary, until replaced by compositor.)
 */

#ifndef nsRefreshDriver_h_
#define nsRefreshDriver_h_

#include "mozilla/FlushType.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"
#include "mozilla/WeakPtr.h"
#include "nsTObserverArray.h"
#include "nsTArray.h"
#include "nsTHashSet.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsRefreshObservers.h"
#include "nsThreadUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/VisualViewport.h"
#include "mozilla/layers/TransactionIdAllocator.h"
#include "LayersTypes.h"

#include "GeckoProfiler.h"  // for ProfileChunkedBuffer

class nsPresContext;

class imgIRequest;
class nsIRunnable;

namespace mozilla {
class AnimationEventDispatcher;
class PendingFullscreenEvent;
class PresShell;
class RefreshDriverTimer;
class Runnable;
class Task;
}  // namespace mozilla

class nsRefreshDriver final : public mozilla::layers::TransactionIdAllocator,
                              public nsARefreshObserver {
  using Document = mozilla::dom::Document;
  using TransactionId = mozilla::layers::TransactionId;
  using VVPResizeEvent =
      mozilla::dom::VisualViewport::VisualViewportResizeEvent;
  using VVPScrollEvent =
      mozilla::dom::VisualViewport::VisualViewportScrollEvent;
  using LogPresShellObserver = mozilla::LogPresShellObserver;

 public:
  explicit nsRefreshDriver(nsPresContext* aPresContext);
  ~nsRefreshDriver();

  /**
   * Methods for testing, exposed via nsIDOMWindowUtils.  See
   * nsIDOMWindowUtils.advanceTimeAndRefresh for description.
   */
  void AdvanceTimeAndRefresh(int64_t aMilliseconds);
  void RestoreNormalRefresh();
  void DoTick();
  bool IsTestControllingRefreshesEnabled() const {
    return mTestControllingRefreshes;
  }

  /**
   * Return the time of the most recent refresh.  This is intended to be
   * used by callers who want to start an animation now and want to know
   * what time to consider the start of the animation.  (This helps
   * ensure that multiple animations started during the same event off
   * the main event loop have the same start time.)
   */
  mozilla::TimeStamp MostRecentRefresh(bool aEnsureTimerStarted = true) const;

  /**
   * Add / remove refresh observers.
   * RemoveRefreshObserver returns true if aObserver was found.
   *
   * The flush type affects:
   *   + the order in which the observers are notified (lowest flush
   *     type to highest, in order registered)
   *   + (in the future) which observers are suppressed when the display
   *     doesn't require current position data or isn't currently
   *     painting, and, correspondingly, which get notified when there
   *     is a flush during such suppression
   * and it must be FlushType::Style, FlushType::Layout, or FlushType::Display.
   *
   * The refresh driver does NOT own a reference to these observers;
   * they must remove themselves before they are destroyed.
   *
   * The observer will be called even if there is no other activity.
   */
  void AddRefreshObserver(nsARefreshObserver* aObserver,
                          mozilla::FlushType aFlushType,
                          const char* aObserverDescription);
  bool RemoveRefreshObserver(nsARefreshObserver* aObserver,
                             mozilla::FlushType aFlushType);
  /**
   * Add / remove an observer wants to know the time when the refresh driver
   * updated the most recent refresh time due to its active timer changes.
   */
  void AddTimerAdjustmentObserver(nsATimerAdjustmentObserver* aObserver);
  void RemoveTimerAdjustmentObserver(nsATimerAdjustmentObserver* aObserver);

  void PostVisualViewportResizeEvent(VVPResizeEvent* aResizeEvent);
  void DispatchVisualViewportResizeEvents();

  void PostScrollEvent(mozilla::Runnable* aScrollEvent, bool aDelayed = false);
  void DispatchScrollEvents();

  void PostVisualViewportScrollEvent(VVPScrollEvent* aScrollEvent);
  void DispatchVisualViewportScrollEvents();

  /**
   * Add an observer that will be called after each refresh. The caller
   * must remove the observer before it is deleted. This does not trigger
   * refresh driver ticks.
   */
  void AddPostRefreshObserver(nsAPostRefreshObserver* aObserver);
  void AddPostRefreshObserver(mozilla::ManagedPostRefreshObserver*) = delete;
  void RemovePostRefreshObserver(nsAPostRefreshObserver* aObserver);
  void RemovePostRefreshObserver(mozilla::ManagedPostRefreshObserver*) = delete;

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
   * Add / remove presshells which have pending resize event.
   */
  void AddResizeEventFlushObserver(mozilla::PresShell* aPresShell,
                                   bool aDelayed = false) {
    MOZ_DIAGNOSTIC_ASSERT(
        !mResizeEventFlushObservers.Contains(aPresShell) &&
            !mDelayedResizeEventFlushObservers.Contains(aPresShell),
        "Double-adding resize event flush observer");
    if (aDelayed) {
      mDelayedResizeEventFlushObservers.AppendElement(aPresShell);
    } else {
      mResizeEventFlushObservers.AppendElement(aPresShell);
      EnsureTimerStarted();
    }
  }

  void RemoveResizeEventFlushObserver(mozilla::PresShell* aPresShell) {
    mResizeEventFlushObservers.RemoveElement(aPresShell);
    mDelayedResizeEventFlushObservers.RemoveElement(aPresShell);
  }

  /**
   * Add / remove presshells that we should flush style and layout on
   */
  void AddStyleFlushObserver(mozilla::PresShell* aPresShell) {
    MOZ_DIAGNOSTIC_ASSERT(!mStyleFlushObservers.Contains(aPresShell),
                          "Double-adding style flush observer");
    LogPresShellObserver::LogDispatch(aPresShell, this);
    mStyleFlushObservers.AppendElement(aPresShell);
    EnsureTimerStarted();
  }

  void RemoveStyleFlushObserver(mozilla::PresShell* aPresShell) {
    mStyleFlushObservers.RemoveElement(aPresShell);
  }
  void AddLayoutFlushObserver(mozilla::PresShell* aPresShell) {
    MOZ_DIAGNOSTIC_ASSERT(!IsLayoutFlushObserver(aPresShell),
                          "Double-adding layout flush observer");
    LogPresShellObserver::LogDispatch(aPresShell, this);
    mLayoutFlushObservers.AppendElement(aPresShell);
    EnsureTimerStarted();
  }
  void RemoveLayoutFlushObserver(mozilla::PresShell* aPresShell) {
    mLayoutFlushObservers.RemoveElement(aPresShell);
  }

  bool IsLayoutFlushObserver(mozilla::PresShell* aPresShell) {
    return mLayoutFlushObservers.Contains(aPresShell);
  }

  /**
   * "Early Runner" runnables will be called as the first step when refresh
   * driver tick is triggered. Runners shouldn't keep other objects alive,
   * since it isn't guaranteed they will ever get called.
   */
  void AddEarlyRunner(nsIRunnable* aRunnable) {
    mEarlyRunners.AppendElement(aRunnable);
    EnsureTimerStarted();
  }

  /**
   * Remember whether our presshell's view manager needs a flush
   */
  void ScheduleViewManagerFlush();
  void RevokeViewManagerFlush() { mViewManagerFlushIsPending = false; }
  bool ViewManagerFlushIsPending() { return mViewManagerFlushIsPending; }
  bool HasScheduleFlush() { return mHasScheduleFlush; }
  void ClearHasScheduleFlush() { mHasScheduleFlush = false; }

  /**
   * Add a document for which we have FrameRequestCallbacks
   */
  void ScheduleFrameRequestCallbacks(Document* aDocument);

  /**
   * Remove a document for which we have FrameRequestCallbacks
   */
  void RevokeFrameRequestCallbacks(Document* aDocument);

  /**
   * Queue a new fullscreen event to be dispatched in next tick before
   * the style flush
   */
  void ScheduleFullscreenEvent(
      mozilla::UniquePtr<mozilla::PendingFullscreenEvent> aEvent);

  /**
   * Cancel all pending fullscreen events scheduled by ScheduleFullscreenEvent
   * which targets any node in aDocument.
   */
  void CancelPendingFullscreenEvents(Document* aDocument);

  /**
   * Queue new animation events to dispatch in next tick.
   */
  void ScheduleAnimationEventDispatch(
      mozilla::AnimationEventDispatcher* aDispatcher) {
    NS_ASSERTION(!mAnimationEventFlushObservers.Contains(aDispatcher),
                 "Double-adding animation event flush observer");
    mAnimationEventFlushObservers.AppendElement(aDispatcher);
    EnsureTimerStarted();
  }

  /**
   * Cancel all pending animation events associated with |aDispatcher|.
   */
  void CancelPendingAnimationEvents(
      mozilla::AnimationEventDispatcher* aDispatcher);

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
  nsPresContext* GetPresContext() const;

  void CreateVsyncRefreshTimer();

#ifdef DEBUG
  /**
   * Check whether the given observer is an observer for the given flush type
   */
  bool IsRefreshObserver(nsARefreshObserver* aObserver,
                         mozilla::FlushType aFlushType);
#endif

  /**
   * Default interval the refresh driver uses, in ms.
   */
  static int32_t DefaultInterval();

  bool IsInRefresh() { return mInRefresh; }

  void SetIsResizeSuppressed() { mResizeSuppressed = true; }
  bool IsResizeSuppressed() const { return mResizeSuppressed; }

  // mozilla::layers::TransactionIdAllocator
  TransactionId GetTransactionId(bool aThrottle) override;
  TransactionId LastTransactionId() const override;
  void NotifyTransactionCompleted(TransactionId aTransactionId) override;
  void RevokeTransactionId(TransactionId aTransactionId) override;
  void ClearPendingTransactions() override;
  void ResetInitialTransactionId(TransactionId aTransactionId) override;
  mozilla::TimeStamp GetTransactionStart() override;
  mozilla::VsyncId GetVsyncId() override;
  mozilla::TimeStamp GetVsyncStart() override;

  bool IsWaitingForPaint(mozilla::TimeStamp aTime);

  // nsARefreshObserver
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void) override {
    return TransactionIdAllocator::AddRef();
  }
  NS_IMETHOD_(MozExternalRefCountType) Release(void) override {
    return TransactionIdAllocator::Release();
  }
  virtual void WillRefresh(mozilla::TimeStamp aTime) override;

  /**
   * Compute the time when the currently active refresh driver timer
   * will start its next tick.
   *
   * Expects a non-null default value that is the upper bound of the
   * expected deadline. If the next expected deadline is later than
   * the default value, the default value is returned.
   *
   * If we're animating and we have skipped paints a time in the past
   * is returned.
   */
  static mozilla::TimeStamp GetIdleDeadlineHint(mozilla::TimeStamp aDefault);

  /**
   * It returns the expected timestamp of the next tick or nothing if the next
   * tick is missed.
   */
  static mozilla::Maybe<mozilla::TimeStamp> GetNextTickHint();

  static void DispatchIdleTaskAfterTickUnlessExists(mozilla::Task* aTask);
  static void CancelIdleTask(mozilla::Task* aTask);

  void NotifyDOMContentLoaded();

  // Schedule a refresh so that any delayed events will run soon.
  void RunDelayedEventsSoon();

  void InitializeTimer() {
    MOZ_ASSERT(!mActiveTimer);
    EnsureTimerStarted();
  }

  bool HasPendingTick() const { return mActiveTimer; }

  void EnsureIntersectionObservationsUpdateHappens() {
    // This is enough to make sure that UpdateIntersectionObservations runs at
    // least once. This is presumably the intent of step 5 in [1]:
    //
    //     Schedule an iteration of the event loop in the root's browsing
    //     context.
    //
    // Though the wording of it is not quite clear to me...
    //
    // [1]:
    // https://w3c.github.io/IntersectionObserver/#dom-intersectionobserver-observe
    EnsureTimerStarted();
    mNeedToUpdateIntersectionObservations = true;
  }

  // Register a composition payload that will be forwarded to the layer manager
  // if the current or upcoming refresh tick does a paint.
  // If no paint happens, the payload is discarded.
  // Should only be called on root refresh drivers.
  void RegisterCompositionPayload(
      const mozilla::layers::CompositionPayload& aPayload);

  enum class TickReasons : uint32_t {
    eNone = 0,
    eHasObservers = 1 << 0,
    eHasImageRequests = 1 << 1,
    eNeedsToUpdateIntersectionObservations = 1 << 2,
    eHasVisualViewportResizeEvents = 1 << 3,
    eHasScrollEvents = 1 << 4,
    eHasVisualViewportScrollEvents = 1 << 5,
  };

  void AddForceNotifyContentfulPaintPresContext(nsPresContext* aPresContext);
  void FlushForceNotifyContentfulPaintPresContext();

 private:
  typedef nsTArray<RefPtr<VVPResizeEvent>> VisualViewportResizeEventArray;
  typedef nsTArray<RefPtr<mozilla::Runnable>> ScrollEventArray;
  typedef nsTArray<RefPtr<VVPScrollEvent>> VisualViewportScrollEventArray;
  typedef nsTHashSet<nsCOMPtr<nsISupports>> RequestTable;
  struct ImageStartData {
    ImageStartData() = default;

    mozilla::Maybe<mozilla::TimeStamp> mStartTime;
    RequestTable mEntries;
  };
  typedef nsClassHashtable<nsUint32HashKey, ImageStartData> ImageStartTable;

  struct ObserverData {
    nsARefreshObserver* mObserver;
    const char* mDescription;
    mozilla::TimeStamp mRegisterTime;
#ifdef MOZ_GECKO_PROFILER
    mozilla::MarkerInnerWindowId mInnerWindowId;
#endif
    mozilla::UniquePtr<mozilla::ProfileChunkedBuffer> mCause;
    mozilla::FlushType mFlushType;

    bool operator==(nsARefreshObserver* aObserver) const {
      return mObserver == aObserver;
    }
    operator RefPtr<nsARefreshObserver>() { return mObserver; }
  };
  typedef nsTObserverArray<ObserverData> ObserverArray;
  void RunFullscreenSteps();
  void DispatchAnimationEvents();
  MOZ_CAN_RUN_SCRIPT
  void RunFrameRequestCallbacks(mozilla::TimeStamp aNowTime);
  void UpdateIntersectionObservations(mozilla::TimeStamp aNowTime);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void Tick(mozilla::VsyncId aId, mozilla::TimeStamp aNowTime);

  enum EnsureTimerStartedFlags {
    eNone = 0,
    eForceAdjustTimer = 1 << 0,
    eAllowTimeToGoBackwards = 1 << 1,
    eNeverAdjustTimer = 1 << 2,
  };
  void EnsureTimerStarted(EnsureTimerStartedFlags aFlags = eNone);
  void StopTimer();

  bool HasObservers() const;
  void AppendObserverDescriptionsToString(nsACString& aStr) const;
  // Note: This should only be called in the dtor of nsRefreshDriver.
  uint32_t ObserverCount() const;
  bool HasImageRequests() const;
  bool ShouldKeepTimerRunningWhileWaitingForFirstContentfulPaint();
  ObserverArray& ArrayFor(mozilla::FlushType aFlushType);
  // Trigger a refresh immediately, if haven't been disconnected or frozen.
  void DoRefresh();

  TickReasons GetReasonsToTick() const;
  void AppendTickReasonsToString(TickReasons aReasons, nsACString& aStr) const;

  double GetRegularTimerInterval() const;
  static double GetThrottledTimerInterval();

  static mozilla::TimeDuration GetMinRecomputeVisibilityInterval();

  bool HaveFrameRequestCallbacks() const {
    return mFrameRequestCallbackDocs.Length() != 0;
  }

  void FinishedWaitingForTransaction();

  bool CanDoCatchUpTick();

  mozilla::RefreshDriverTimer* ChooseTimer();
  mozilla::RefreshDriverTimer* mActiveTimer;
  RefPtr<mozilla::RefreshDriverTimer> mOwnTimer;
  mozilla::UniquePtr<mozilla::ProfileChunkedBuffer> mRefreshTimerStartedCause;

  // nsPresContext passed in constructor and unset in Disconnect.
  mozilla::WeakPtr<nsPresContext> mPresContext;

  RefPtr<nsRefreshDriver> mRootRefresh;

  // The most recently allocated transaction id.
  TransactionId mNextTransactionId;
  // This number is mCompletedTransaction + (pending transaction count).
  // When we revoke a transaction id, we revert this number (since it's
  // no longer outstanding), but not mNextTransactionId (since we don't
  // want to reuse the number).
  TransactionId mOutstandingTransactionId;
  // The most recently completed transaction id.
  TransactionId mCompletedTransaction;

  uint32_t mFreezeCount;

  // How long we wait between ticks for throttled (which generally means
  // non-visible) documents registered with a non-throttled refresh driver.
  const mozilla::TimeDuration mThrottledFrameRequestInterval;

  // How long we wait, at a minimum, before recomputing approximate frame
  // visibility information. This is a minimum because, regardless of this
  // interval, we only recompute visibility when we've seen a layout or style
  // flush since the last time we did it.
  const mozilla::TimeDuration mMinRecomputeVisibilityInterval;

  mozilla::UniquePtr<mozilla::ProfileChunkedBuffer> mViewManagerFlushCause;

  bool mThrottled : 1;
  bool mNeedToRecomputeVisibility : 1;
  bool mTestControllingRefreshes : 1;

  // These two fields are almost the same, the only difference is that
  // mViewManagerFlushIsPending gets cleared right before calling
  // ProcessPendingUpdates, and mHasScheduleFlush gets cleared right after
  // calling ProcessPendingUpdates. It is important that mHasScheduleFlush
  // only gets cleared after, but it's not clear if mViewManagerFlushIsPending
  // needs to be cleared before.
  bool mViewManagerFlushIsPending : 1;
  bool mHasScheduleFlush : 1;

  bool mInRefresh : 1;

  // True if the refresh driver is suspended waiting for transaction
  // id's to be returned and shouldn't do any work during Tick().
  bool mWaitingForTransaction : 1;
  // True if Tick() was skipped because of mWaitingForTransaction and
  // we should schedule a new Tick immediately when resumed instead
  // of waiting until the next interval.
  bool mSkippedPaints : 1;

  // True if view managers should delay any resize request until the
  // next tick by the refresh driver. This flag will be reset at the
  // start of every tick.
  bool mResizeSuppressed : 1;

  // True if the next tick should notify DOMContentFlushed.
  bool mNotifyDOMContentFlushed : 1;

  // True if we need to flush in order to update intersection observations in
  // all our documents.
  bool mNeedToUpdateIntersectionObservations : 1;

  // Number of seconds that the refresh driver is blocked waiting for a
  // compositor transaction to be completed before we append a note to the gfx
  // critical log. The number is doubled every time the threshold is hit.
  uint64_t mWarningThreshold;
  mozilla::TimeStamp mMostRecentRefresh;
  mozilla::TimeStamp mTickStart;
  mozilla::VsyncId mTickVsyncId;
  mozilla::TimeStamp mTickVsyncTime;
  mozilla::TimeStamp mNextThrottledFrameRequestTick;
  mozilla::TimeStamp mNextRecomputeVisibilityTick;
  mozilla::TimeStamp mBeforeFirstContentfulPaintTimerRunningLimit;

  // separate arrays for each flush type we support
  ObserverArray mObservers[4];
  // These observers should NOT be included in HasObservers() since that method
  // is used to determine whether or not to stop the timer, or restore it when
  // thawing the refresh driver. On the other hand these observers are intended
  // to be called when the timer is re-started and should not influence its
  // starting or stopping.
  nsTObserverArray<nsATimerAdjustmentObserver*> mTimerAdjustmentObservers;
  nsTArray<mozilla::layers::CompositionPayload> mCompositionPayloads;
  RequestTable mRequests;
  ImageStartTable mStartTable;
  AutoTArray<nsCOMPtr<nsIRunnable>, 16> mEarlyRunners;
  VisualViewportResizeEventArray mVisualViewportResizeEvents;
  ScrollEventArray mScrollEvents;
  VisualViewportScrollEventArray mVisualViewportScrollEvents;

  // Scroll events on documents that might have events suppressed.
  ScrollEventArray mDelayedScrollEvents;

  AutoTArray<mozilla::PresShell*, 16> mResizeEventFlushObservers;
  AutoTArray<mozilla::PresShell*, 16> mDelayedResizeEventFlushObservers;
  AutoTArray<mozilla::PresShell*, 16> mStyleFlushObservers;
  AutoTArray<mozilla::PresShell*, 16> mLayoutFlushObservers;
  // nsTArray on purpose, because we want to be able to swap.
  nsTArray<Document*> mFrameRequestCallbackDocs;
  nsTArray<Document*> mThrottledFrameRequestCallbackDocs;
  nsTObserverArray<nsAPostRefreshObserver*> mPostRefreshObservers;
  nsTArray<mozilla::UniquePtr<mozilla::PendingFullscreenEvent>>
      mPendingFullscreenEvents;
  AutoTArray<mozilla::AnimationEventDispatcher*, 16>
      mAnimationEventFlushObservers;

  // nsPresContexts which `NotifyContentfulPaint` have been called,
  // however the corresponding paint doesn't come from a regular
  // rendering steps(aka tick).
  //
  // For these nsPresContexts, we invoke
  // `FlushForceNotifyContentfulPaintPresContext` in the next tick
  // to force notify contentful paint, regardless whether the tick paints
  // or not.
  nsTArray<mozilla::WeakPtr<nsPresContext>>
      mForceNotifyContentfulPaintPresContexts;

  void BeginRefreshingImages(RequestTable& aEntries,
                             mozilla::TimeStamp aDesired);

  friend class mozilla::RefreshDriverTimer;

  static void Shutdown();
};

#endif /* !defined(nsRefreshDriver_h_) */
