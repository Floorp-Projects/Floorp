/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZTestCommon_h
#define mozilla_layers_APZTestCommon_h

/**
 * Defines a set of mock classes and utility functions/classes for
 * writing APZ gtests.
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mozilla/Attributes.h"
#include "mozilla/layers/AsyncCompositionManager.h" // for ViewTransform
#include "mozilla/layers/GeckoContentController.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/LayerMetricsWrapper.h"
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/TypedEnumBits.h"
#include "mozilla/UniquePtr.h"
#include "apz/src/APZCTreeManager.h"
#include "apz/src/AsyncPanZoomController.h"
#include "apz/src/HitTestingTreeNode.h"
#include "base/task.h"
#include "Layers.h"
#include "TestLayers.h"
#include "UnitTransforms.h"
#include "gfxPrefs.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::MockFunction;
using ::testing::InSequence;
typedef mozilla::layers::GeckoContentController::TapType TapType;

template<class T>
class ScopedGfxPref {
public:
  ScopedGfxPref(T (*aGetPrefFunc)(void), void (*aSetPrefFunc)(T), T aVal)
    : mSetPrefFunc(aSetPrefFunc)
  {
    mOldVal = aGetPrefFunc();
    aSetPrefFunc(aVal);
  }

  ~ScopedGfxPref() {
    mSetPrefFunc(mOldVal);
  }

private:
  void (*mSetPrefFunc)(T);
  T mOldVal;
};

#define SCOPED_GFX_PREF(prefBase, prefType, prefValue) \
  ScopedGfxPref<prefType> pref_##prefBase( \
    &(gfxPrefs::prefBase), \
    &(gfxPrefs::Set##prefBase), \
    prefValue)

static TimeStamp GetStartupTime() {
  static TimeStamp sStartupTime = TimeStamp::Now();
  return sStartupTime;
}

class MockContentController : public GeckoContentController {
public:
  MOCK_METHOD1(RequestContentRepaint, void(const FrameMetrics&));
  MOCK_METHOD2(RequestFlingSnap, void(const FrameMetrics::ViewID& aScrollId, const mozilla::CSSPoint& aDestination));
  MOCK_METHOD2(AcknowledgeScrollUpdate, void(const FrameMetrics::ViewID&, const uint32_t& aScrollGeneration));
  MOCK_METHOD5(HandleTap, void(TapType, const LayoutDevicePoint&, Modifiers, const ScrollableLayerGuid&, uint64_t));
  MOCK_METHOD4(NotifyPinchGesture, void(PinchGestureInput::PinchGestureType, const ScrollableLayerGuid&, LayoutDeviceCoord, Modifiers));
  // Can't use the macros with already_AddRefed :(
  void PostDelayedTask(already_AddRefed<Runnable> aTask, int aDelayMs) {
    RefPtr<Runnable> task = aTask;
  }
  bool IsRepaintThread() {
    return NS_IsMainThread();
  }
  void DispatchToRepaintThread(already_AddRefed<Runnable> aTask) {
    NS_DispatchToMainThread(std::move(aTask));
  }
  MOCK_METHOD3(NotifyAPZStateChange, void(const ScrollableLayerGuid& aGuid, APZStateChange aChange, int aArg));
  MOCK_METHOD0(NotifyFlushComplete, void());
  MOCK_METHOD1(NotifyAsyncScrollbarDragRejected, void(const FrameMetrics::ViewID&));
  MOCK_METHOD1(NotifyAsyncAutoscrollRejected, void(const FrameMetrics::ViewID&));
  MOCK_METHOD1(CancelAutoscroll, void(const ScrollableLayerGuid&));
};

class MockContentControllerDelayed : public MockContentController {
public:
  MockContentControllerDelayed()
    : mTime(GetStartupTime())
  {
  }

  const TimeStamp& Time() {
    return mTime;
  }

  void AdvanceByMillis(int aMillis) {
    AdvanceBy(TimeDuration::FromMilliseconds(aMillis));
  }

  void AdvanceBy(const TimeDuration& aIncrement) {
    TimeStamp target = mTime + aIncrement;
    while (mTaskQueue.Length() > 0 && mTaskQueue[0].second <= target) {
      RunNextDelayedTask();
    }
    mTime = target;
  }

  void PostDelayedTask(already_AddRefed<Runnable> aTask, int aDelayMs) {
    RefPtr<Runnable> task = aTask;
    TimeStamp runAtTime = mTime + TimeDuration::FromMilliseconds(aDelayMs);
    int insIndex = mTaskQueue.Length();
    while (insIndex > 0) {
      if (mTaskQueue[insIndex - 1].second <= runAtTime) {
        break;
      }
      insIndex--;
    }
    mTaskQueue.InsertElementAt(insIndex, std::make_pair(task, runAtTime));
  }

  // Run all the tasks in the queue, returning the number of tasks
  // run. Note that if a task queues another task while running, that
  // new task will not be run. Therefore, there may be still be tasks
  // in the queue after this function is called. Only when the return
  // value is 0 is the queue guaranteed to be empty.
  int RunThroughDelayedTasks() {
    nsTArray<std::pair<RefPtr<Runnable>, TimeStamp>> runQueue;
    runQueue.SwapElements(mTaskQueue);
    int numTasks = runQueue.Length();
    for (int i = 0; i < numTasks; i++) {
      mTime = runQueue[i].second;
      runQueue[i].first->Run();

      // Deleting the task is important in order to release the reference to
      // the callee object.
      runQueue[i].first = nullptr;
    }
    return numTasks;
  }

private:
  void RunNextDelayedTask() {
    std::pair<RefPtr<Runnable>, TimeStamp> next = mTaskQueue[0];
    mTaskQueue.RemoveElementAt(0);
    mTime = next.second;
    next.first->Run();
    // Deleting the task is important in order to release the reference to
    // the callee object.
    next.first = nullptr;
  }

  // The following array is sorted by timestamp (tasks are inserted in order by
  // timestamp).
  nsTArray<std::pair<RefPtr<Runnable>, TimeStamp>> mTaskQueue;
  TimeStamp mTime;
};

class TestAPZCTreeManager : public APZCTreeManager {
public:
  explicit TestAPZCTreeManager(MockContentControllerDelayed* aMcc)
    : APZCTreeManager(LayersId{0})
    , mcc(aMcc)
  {}

  RefPtr<InputQueue> GetInputQueue() const {
    return mInputQueue;
  }

  void ClearContentController() {
    mcc = nullptr;
  }

protected:
  AsyncPanZoomController* NewAPZCInstance(LayersId aLayersId,
                                          GeckoContentController* aController) override;

  TimeStamp GetFrameTime() override {
    return mcc->Time();
  }

private:
  RefPtr<MockContentControllerDelayed> mcc;
};

class TestAsyncPanZoomController : public AsyncPanZoomController {
public:
  TestAsyncPanZoomController(LayersId aLayersId, MockContentControllerDelayed* aMcc,
                             TestAPZCTreeManager* aTreeManager,
                             GestureBehavior aBehavior = DEFAULT_GESTURES)
    : AsyncPanZoomController(aLayersId, aTreeManager, aTreeManager->GetInputQueue(),
        aMcc, aBehavior)
    , mWaitForMainThread(false)
    , mcc(aMcc)
  {}

  nsEventStatus ReceiveInputEvent(const InputData& aEvent, ScrollableLayerGuid* aDummy, uint64_t* aOutInputBlockId) {
    // This is a function whose signature matches exactly the ReceiveInputEvent
    // on APZCTreeManager. This allows us to templates for functions like
    // TouchDown, TouchUp, etc so that we can reuse the code for dispatching
    // events into both APZC and APZCTM.
    return ReceiveInputEvent(aEvent, aOutInputBlockId);
  }

  nsEventStatus ReceiveInputEvent(const InputData& aEvent, uint64_t* aOutInputBlockId) {
    return GetInputQueue()->ReceiveInputEvent(this, TargetConfirmationFlags{!mWaitForMainThread}, aEvent, aOutInputBlockId);
  }

  void ContentReceivedInputBlock(uint64_t aInputBlockId, bool aPreventDefault) {
    GetInputQueue()->ContentReceivedInputBlock(aInputBlockId, aPreventDefault);
  }

  void ConfirmTarget(uint64_t aInputBlockId) {
    RefPtr<AsyncPanZoomController> target = this;
    GetInputQueue()->SetConfirmedTargetApzc(aInputBlockId, target);
  }

  void SetAllowedTouchBehavior(uint64_t aInputBlockId, const nsTArray<TouchBehaviorFlags>& aBehaviors) {
    GetInputQueue()->SetAllowedTouchBehavior(aInputBlockId, aBehaviors);
  }

  void SetFrameMetrics(const FrameMetrics& metrics) {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    mFrameMetrics = metrics;
  }

  FrameMetrics& GetFrameMetrics() {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    return mFrameMetrics;
  }

  ScrollMetadata& GetScrollMetadata() {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    return mScrollMetadata;
  }

  const FrameMetrics& GetFrameMetrics() const {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    return mFrameMetrics;
  }

  using AsyncPanZoomController::GetVelocityVector;

  void AssertStateIsReset() const {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    EXPECT_EQ(NOTHING, mState);
  }

  void AssertStateIsFling() const {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    EXPECT_EQ(FLING, mState);
  }

  void AssertStateIsSmoothScroll() const {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    EXPECT_EQ(SMOOTH_SCROLL, mState);
  }

  void AssertNotAxisLocked() const {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    EXPECT_EQ(PANNING, mState);
  }

  void AssertAxisLocked(ScrollDirection aDirection) const {
    RecursiveMutexAutoLock lock(mRecursiveMutex);
    switch (aDirection) {
    case ScrollDirection::eHorizontal:
      EXPECT_EQ(PANNING_LOCKED_X, mState);
      break;
    case ScrollDirection::eVertical:
      EXPECT_EQ(PANNING_LOCKED_Y, mState);
      break;
    }
  }

  void AdvanceAnimationsUntilEnd(const TimeDuration& aIncrement = TimeDuration::FromMilliseconds(10)) {
    while (AdvanceAnimations(mcc->Time())) {
      mcc->AdvanceBy(aIncrement);
    }
  }

  bool SampleContentTransformForFrame(AsyncTransform* aOutTransform,
                                      ParentLayerPoint& aScrollOffset,
                                      const TimeDuration& aIncrement = TimeDuration::FromMilliseconds(0)) {
    mcc->AdvanceBy(aIncrement);
    bool ret = AdvanceAnimations(mcc->Time());
    if (aOutTransform) {
      *aOutTransform = GetCurrentAsyncTransform(AsyncPanZoomController::eForHitTesting);
    }
    aScrollOffset = GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting);
    return ret;
  }

  void SetWaitForMainThread() {
    mWaitForMainThread = true;
  }

private:
  bool mWaitForMainThread;
  MockContentControllerDelayed* mcc;
};

class APZCTesterBase : public ::testing::Test {
public:
  APZCTesterBase() {
    mcc = new NiceMock<MockContentControllerDelayed>();
  }

  enum class PanOptions {
    None = 0,
    KeepFingerDown = 0x1,
    /*
     * Do not adjust the touch-start coordinates to overcome the touch-start
     * tolerance threshold. If this option is passed, it's up to the caller
     * to pass in coordinates that are sufficient to overcome the touch-start
     * tolerance *and* cause the desired amount of scrolling.
     */
    ExactCoordinates = 0x2
  };

  template<class InputReceiver>
  void Tap(const RefPtr<InputReceiver>& aTarget, const ScreenIntPoint& aPoint,
           TimeDuration aTapLength,
           nsEventStatus (*aOutEventStatuses)[2] = nullptr,
           uint64_t* aOutInputBlockId = nullptr);

  template<class InputReceiver>
  void TapAndCheckStatus(const RefPtr<InputReceiver>& aTarget,
                         const ScreenIntPoint& aPoint, TimeDuration aTapLength);

  template<class InputReceiver>
  void Pan(const RefPtr<InputReceiver>& aTarget,
           const ScreenIntPoint& aTouchStart,
           const ScreenIntPoint& aTouchEnd,
           PanOptions aOptions = PanOptions::None,
           nsTArray<uint32_t>* aAllowedTouchBehaviors = nullptr,
           nsEventStatus (*aOutEventStatuses)[4] = nullptr,
           uint64_t* aOutInputBlockId = nullptr);

  /*
   * A version of Pan() that only takes y coordinates rather than (x, y) points
   * for the touch start and end points, and uses 10 for the x coordinates.
   * This is for convenience, as most tests only need to pan in one direction.
  */
  template<class InputReceiver>
  void Pan(const RefPtr<InputReceiver>& aTarget, int aTouchStartY,
           int aTouchEndY, PanOptions aOptions = PanOptions::None,
           nsTArray<uint32_t>* aAllowedTouchBehaviors = nullptr,
           nsEventStatus (*aOutEventStatuses)[4] = nullptr,
           uint64_t* aOutInputBlockId = nullptr);

  /*
   * Dispatches mock touch events to the apzc and checks whether apzc properly
   * consumed them and triggered scrolling behavior.
  */
  template<class InputReceiver>
  void PanAndCheckStatus(const RefPtr<InputReceiver>& aTarget, int aTouchStartY,
                         int aTouchEndY,
                         bool aExpectConsumed,
                         nsTArray<uint32_t>* aAllowedTouchBehaviors,
                         uint64_t* aOutInputBlockId = nullptr);

  void ApzcPanNoFling(const RefPtr<TestAsyncPanZoomController>& aApzc,
                      int aTouchStartY,
                      int aTouchEndY,
                      uint64_t* aOutInputBlockId = nullptr);

  template<class InputReceiver>
  void DoubleTap(const RefPtr<InputReceiver>& aTarget,
                 const ScreenIntPoint& aPoint,
                 nsEventStatus (*aOutEventStatuses)[4] = nullptr,
                 uint64_t (*aOutInputBlockIds)[2] = nullptr);

  template<class InputReceiver>
  void DoubleTapAndCheckStatus(const RefPtr<InputReceiver>& aTarget,
                               const ScreenIntPoint& aPoint,
                               uint64_t (*aOutInputBlockIds)[2] = nullptr);

protected:
  RefPtr<MockContentControllerDelayed> mcc;
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(APZCTesterBase::PanOptions)

template<class InputReceiver>
void
APZCTesterBase::Tap(const RefPtr<InputReceiver>& aTarget,
                    const ScreenIntPoint& aPoint, TimeDuration aTapLength,
                    nsEventStatus (*aOutEventStatuses)[2],
                    uint64_t* aOutInputBlockId)
{
  // Even if the caller doesn't care about the block id, we need it to set the
  // allowed touch behaviour below, so make sure aOutInputBlockId is non-null.
  uint64_t blockId;
  if (!aOutInputBlockId) {
    aOutInputBlockId = &blockId;
  }

  nsEventStatus status = TouchDown(aTarget, aPoint, mcc->Time(), aOutInputBlockId);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[0] = status;
  }
  mcc->AdvanceBy(aTapLength);

  // If touch-action is enabled then simulate the allowed touch behaviour
  // notification that the main thread is supposed to deliver.
  if (gfxPrefs::TouchActionEnabled() && status != nsEventStatus_eConsumeNoDefault) {
    SetDefaultAllowedTouchBehavior(aTarget, *aOutInputBlockId);
  }

  status = TouchUp(aTarget, aPoint, mcc->Time());
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[1] = status;
  }
}

template<class InputReceiver>
void
APZCTesterBase::TapAndCheckStatus(const RefPtr<InputReceiver>& aTarget,
                                  const ScreenIntPoint& aPoint,
                                  TimeDuration aTapLength)
{
  nsEventStatus statuses[2];
  Tap(aTarget, aPoint, aTapLength, &statuses);
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, statuses[0]);
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, statuses[1]);
}

template<class InputReceiver>
void
APZCTesterBase::Pan(const RefPtr<InputReceiver>& aTarget,
                    const ScreenIntPoint& aTouchStart,
                    const ScreenIntPoint& aTouchEnd,
                    PanOptions aOptions,
                    nsTArray<uint32_t>* aAllowedTouchBehaviors,
                    nsEventStatus (*aOutEventStatuses)[4],
                    uint64_t* aOutInputBlockId)
{
  // Reduce the touch start and move tolerance to a tiny value.
  // We can't use a scoped pref because this value might be read at some later
  // time when the events are actually processed, rather than when we deliver
  // them.
  gfxPrefs::SetAPZTouchStartTolerance(1.0f / 1000.0f);
  gfxPrefs::SetAPZTouchMoveTolerance(0.0f);
  int overcomeTouchToleranceX = 0;
  int overcomeTouchToleranceY = 0;
  if (!(aOptions & PanOptions::ExactCoordinates)) {
    // Have the direction of the adjustment to overcome the touch tolerance
    // match the direction of the entire gesture, otherwise we run into
    // trouble such as accidentally activating the axis lock.
    if (aTouchStart.x != aTouchEnd.x) {
      overcomeTouchToleranceX = 1;
    }
    if (aTouchStart.y != aTouchEnd.y) {
      overcomeTouchToleranceY = 1;
    }
  }

  const TimeDuration TIME_BETWEEN_TOUCH_EVENT = TimeDuration::FromMilliseconds(50);

  // Even if the caller doesn't care about the block id, we need it to set the
  // allowed touch behaviour below, so make sure aOutInputBlockId is non-null.
  uint64_t blockId;
  if (!aOutInputBlockId) {
    aOutInputBlockId = &blockId;
  }

  // Make sure the move is large enough to not be handled as a tap
  nsEventStatus status = TouchDown(aTarget,
      ScreenIntPoint(aTouchStart.x + overcomeTouchToleranceX,
                     aTouchStart.y + overcomeTouchToleranceY),
      mcc->Time(), aOutInputBlockId);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[0] = status;
  }

  mcc->AdvanceBy(TIME_BETWEEN_TOUCH_EVENT);

  // Allowed touch behaviours must be set after sending touch-start.
  if (status != nsEventStatus_eConsumeNoDefault) {
    if (aAllowedTouchBehaviors) {
      EXPECT_EQ(1UL, aAllowedTouchBehaviors->Length());
      aTarget->SetAllowedTouchBehavior(*aOutInputBlockId, *aAllowedTouchBehaviors);
    } else if (gfxPrefs::TouchActionEnabled()) {
      SetDefaultAllowedTouchBehavior(aTarget, *aOutInputBlockId);
    }
  }

  status = TouchMove(aTarget, aTouchStart, mcc->Time());
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[1] = status;
  }

  mcc->AdvanceBy(TIME_BETWEEN_TOUCH_EVENT);

  status = TouchMove(aTarget, aTouchEnd, mcc->Time());
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[2] = status;
  }

  mcc->AdvanceBy(TIME_BETWEEN_TOUCH_EVENT);

  if (!(aOptions & PanOptions::KeepFingerDown)) {
    status = TouchUp(aTarget, aTouchEnd, mcc->Time());
  } else {
    status = nsEventStatus_eIgnore;
  }
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[3] = status;
  }

  // Don't increment the time here. Animations started on touch-up, such as
  // flings, are affected by elapsed time, and we want to be able to sample
  // them immediately after they start, without time having elapsed.
}

template<class InputReceiver>
void
APZCTesterBase::Pan(const RefPtr<InputReceiver>& aTarget,
                    int aTouchStartY, int aTouchEndY, PanOptions aOptions,
                    nsTArray<uint32_t>* aAllowedTouchBehaviors,
                    nsEventStatus (*aOutEventStatuses)[4],
                    uint64_t* aOutInputBlockId)
{
  Pan(aTarget, ScreenIntPoint(10, aTouchStartY), ScreenIntPoint(10, aTouchEndY),
      aOptions, aAllowedTouchBehaviors, aOutEventStatuses, aOutInputBlockId);
}

template<class InputReceiver>
void
APZCTesterBase::PanAndCheckStatus(const RefPtr<InputReceiver>& aTarget,
                                  int aTouchStartY,
                                  int aTouchEndY,
                                  bool aExpectConsumed,
                                  nsTArray<uint32_t>* aAllowedTouchBehaviors,
                                  uint64_t* aOutInputBlockId)
{
  nsEventStatus statuses[4]; // down, move, move, up
  Pan(aTarget, aTouchStartY, aTouchEndY, PanOptions::None, aAllowedTouchBehaviors, &statuses, aOutInputBlockId);

  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, statuses[0]);

  nsEventStatus touchMoveStatus;
  if (aExpectConsumed) {
    touchMoveStatus = nsEventStatus_eConsumeDoDefault;
  } else {
    touchMoveStatus = nsEventStatus_eIgnore;
  }
  EXPECT_EQ(touchMoveStatus, statuses[1]);
  EXPECT_EQ(touchMoveStatus, statuses[2]);
}

void
APZCTesterBase::ApzcPanNoFling(const RefPtr<TestAsyncPanZoomController>& aApzc,
                               int aTouchStartY, int aTouchEndY,
                               uint64_t* aOutInputBlockId)
{
  Pan(aApzc, aTouchStartY, aTouchEndY, PanOptions::None, nullptr, nullptr, aOutInputBlockId);
  aApzc->CancelAnimation();
}

template<class InputReceiver>
void
APZCTesterBase::DoubleTap(const RefPtr<InputReceiver>& aTarget,
                          const ScreenIntPoint& aPoint,
                          nsEventStatus (*aOutEventStatuses)[4],
                          uint64_t (*aOutInputBlockIds)[2])
{
  uint64_t blockId;
  nsEventStatus status = TouchDown(aTarget, aPoint, mcc->Time(), &blockId);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[0] = status;
  }
  if (aOutInputBlockIds) {
    (*aOutInputBlockIds)[0] = blockId;
  }
  mcc->AdvanceByMillis(10);

  // If touch-action is enabled then simulate the allowed touch behaviour
  // notification that the main thread is supposed to deliver.
  if (gfxPrefs::TouchActionEnabled() && status != nsEventStatus_eConsumeNoDefault) {
    SetDefaultAllowedTouchBehavior(aTarget, blockId);
  }

  status = TouchUp(aTarget, aPoint, mcc->Time());
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[1] = status;
  }
  mcc->AdvanceByMillis(10);
  status = TouchDown(aTarget, aPoint, mcc->Time(), &blockId);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[2] = status;
  }
  if (aOutInputBlockIds) {
    (*aOutInputBlockIds)[1] = blockId;
  }
  mcc->AdvanceByMillis(10);

  if (gfxPrefs::TouchActionEnabled() && status != nsEventStatus_eConsumeNoDefault) {
    SetDefaultAllowedTouchBehavior(aTarget, blockId);
  }

  status = TouchUp(aTarget, aPoint, mcc->Time());
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[3] = status;
  }
}

template<class InputReceiver>
void
APZCTesterBase::DoubleTapAndCheckStatus(const RefPtr<InputReceiver>& aTarget,
                                        const ScreenIntPoint& aPoint,
                                        uint64_t (*aOutInputBlockIds)[2])
{
  nsEventStatus statuses[4];
  DoubleTap(aTarget, aPoint, &statuses, aOutInputBlockIds);
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, statuses[0]);
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, statuses[1]);
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, statuses[2]);
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, statuses[3]);
}

AsyncPanZoomController*
TestAPZCTreeManager::NewAPZCInstance(LayersId aLayersId,
                                     GeckoContentController* aController)
{
  MockContentControllerDelayed* mcc = static_cast<MockContentControllerDelayed*>(aController);
  return new TestAsyncPanZoomController(aLayersId, mcc, this,
      AsyncPanZoomController::USE_GESTURE_DETECTOR);
}

FrameMetrics
TestFrameMetrics()
{
  FrameMetrics fm;

  fm.SetDisplayPort(CSSRect(0, 0, 10, 10));
  fm.SetCompositionBounds(ParentLayerRect(0, 0, 10, 10));
  fm.SetCriticalDisplayPort(CSSRect(0, 0, 10, 10));
  fm.SetScrollableRect(CSSRect(0, 0, 100, 100));

  return fm;
}

uint32_t
MillisecondsSinceStartup(TimeStamp aTime)
{
  return (aTime - GetStartupTime()).ToMilliseconds();
}

#endif // mozilla_layers_APZTestCommon_h
