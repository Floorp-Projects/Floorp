/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
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
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/APZCTreeManager.h"
#include "mozilla/layers/LayerMetricsWrapper.h"
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/UniquePtr.h"
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

class Task;

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

class MockContentController : public GeckoContentController {
public:
  MOCK_METHOD1(RequestContentRepaint, void(const FrameMetrics&));
  MOCK_METHOD2(RequestFlingSnap, void(const FrameMetrics::ViewID& aScrollId, const mozilla::CSSPoint& aDestination));
  MOCK_METHOD2(AcknowledgeScrollUpdate, void(const FrameMetrics::ViewID&, const uint32_t& aScrollGeneration));
  MOCK_METHOD3(HandleDoubleTap, void(const CSSPoint&, Modifiers, const ScrollableLayerGuid&));
  MOCK_METHOD3(HandleSingleTap, void(const CSSPoint&, Modifiers, const ScrollableLayerGuid&));
  MOCK_METHOD4(HandleLongTap, void(const CSSPoint&, Modifiers, const ScrollableLayerGuid&, uint64_t));
  MOCK_METHOD2(PostDelayedTask, void(Task* aTask, int aDelayMs));
  MOCK_METHOD3(NotifyAPZStateChange, void(const ScrollableLayerGuid& aGuid, APZStateChange aChange, int aArg));
  MOCK_METHOD0(NotifyFlushComplete, void());
};

class MockContentControllerDelayed : public MockContentController {
public:
  MockContentControllerDelayed()
    : mTime(TimeStamp::Now())
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

  void PostDelayedTask(Task* aTask, int aDelayMs) {
    TimeStamp runAtTime = mTime + TimeDuration::FromMilliseconds(aDelayMs);
    int insIndex = mTaskQueue.Length();
    while (insIndex > 0) {
      if (mTaskQueue[insIndex - 1].second <= runAtTime) {
        break;
      }
      insIndex--;
    }
    mTaskQueue.InsertElementAt(insIndex, std::make_pair(aTask, runAtTime));
  }

  // Run all the tasks in the queue, returning the number of tasks
  // run. Note that if a task queues another task while running, that
  // new task will not be run. Therefore, there may be still be tasks
  // in the queue after this function is called. Only when the return
  // value is 0 is the queue guaranteed to be empty.
  int RunThroughDelayedTasks() {
    nsTArray<std::pair<Task*, TimeStamp>> runQueue;
    runQueue.SwapElements(mTaskQueue);
    int numTasks = runQueue.Length();
    for (int i = 0; i < numTasks; i++) {
      mTime = runQueue[i].second;
      runQueue[i].first->Run();

      // Deleting the task is important in order to release the reference to
      // the callee object.
      delete runQueue[i].first;
    }
    return numTasks;
  }

private:
  void RunNextDelayedTask() {
    std::pair<Task*, TimeStamp> next = mTaskQueue[0];
    mTaskQueue.RemoveElementAt(0);
    mTime = next.second;
    next.first->Run();
    // Deleting the task is important in order to release the reference to
    // the callee object.
    delete next.first;
  }

  // The following array is sorted by timestamp (tasks are inserted in order by
  // timestamp).
  nsTArray<std::pair<Task*, TimeStamp>> mTaskQueue;
  TimeStamp mTime;
};

class TestAPZCTreeManager : public APZCTreeManager {
public:
  explicit TestAPZCTreeManager(MockContentControllerDelayed* aMcc) : mcc(aMcc) {}

  RefPtr<InputQueue> GetInputQueue() const {
    return mInputQueue;
  }

protected:
  AsyncPanZoomController* NewAPZCInstance(uint64_t aLayersId,
                                          GeckoContentController* aController) override;

  TimeStamp GetFrameTime() override {
    return mcc->Time();
  }

private:
  RefPtr<MockContentControllerDelayed> mcc;
};

class TestAsyncPanZoomController : public AsyncPanZoomController {
public:
  TestAsyncPanZoomController(uint64_t aLayersId, MockContentControllerDelayed* aMcc,
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
    return GetInputQueue()->ReceiveInputEvent(this, !mWaitForMainThread, aEvent, aOutInputBlockId);
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
    ReentrantMonitorAutoEnter lock(mMonitor);
    mFrameMetrics = metrics;
  }

  FrameMetrics& GetFrameMetrics() {
    ReentrantMonitorAutoEnter lock(mMonitor);
    return mFrameMetrics;
  }

  const FrameMetrics& GetFrameMetrics() const {
    ReentrantMonitorAutoEnter lock(mMonitor);
    return mFrameMetrics;
  }

  using AsyncPanZoomController::GetVelocityVector;

  void AssertStateIsReset() const {
    ReentrantMonitorAutoEnter lock(mMonitor);
    EXPECT_EQ(NOTHING, mState);
  }

  void AssertStateIsFling() const {
    ReentrantMonitorAutoEnter lock(mMonitor);
    EXPECT_EQ(FLING, mState);
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
    AsyncPanZoomController::SampleContentTransformForFrame(
      aOutTransform, aScrollOffset);
    return ret;
  }

  void SetWaitForMainThread() {
    mWaitForMainThread = true;
  }

  static TimeStamp GetStartupTime() {
    static TimeStamp sStartupTime = TimeStamp::Now();
    return sStartupTime;
  }

private:
  bool mWaitForMainThread;
  MockContentControllerDelayed* mcc;
};

AsyncPanZoomController*
TestAPZCTreeManager::NewAPZCInstance(uint64_t aLayersId,
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
  return (aTime - TestAsyncPanZoomController::GetStartupTime()).ToMilliseconds();
}

#endif // mozilla_layers_APZTestCommon_h
