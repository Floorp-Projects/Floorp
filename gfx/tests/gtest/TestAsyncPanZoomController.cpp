/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
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
  MOCK_METHOD3(SendAsyncScrollDOMEvent, void(bool aIsRoot, const CSSRect &aContentRect, const CSSSize &aScrollableSize));
  MOCK_METHOD2(PostDelayedTask, void(Task* aTask, int aDelayMs));
  MOCK_METHOD3(NotifyAPZStateChange, void(const ScrollableLayerGuid& aGuid, APZStateChange aChange, int aArg));
};

class MockContentControllerDelayed : public MockContentController {
public:
  MockContentControllerDelayed()
  {
  }

  void PostDelayedTask(Task* aTask, int aDelayMs) {
    mTaskQueue.AppendElement(aTask);
  }

  void CheckHasDelayedTask() {
    EXPECT_TRUE(mTaskQueue.Length() > 0);
  }

  void ClearDelayedTask() {
    mTaskQueue.RemoveElementAt(0);
  }

  void DestroyOldestTask() {
    delete mTaskQueue[0];
    mTaskQueue.RemoveElementAt(0);
  }

  // Note that deleting mCurrentTask is important in order to
  // release the reference to the callee object. Without this
  // that object might be leaked. This is also why we don't
  // expose mTaskQueue to any users of MockContentControllerDelayed.
  void RunDelayedTask() {
    mTaskQueue[0]->Run();
    delete mTaskQueue[0];
    mTaskQueue.RemoveElementAt(0);
  }

  // Run all the tasks in the queue, returning the number of tasks
  // run. Note that if a task queues another task while running, that
  // new task will not be run. Therefore, there may be still be tasks
  // in the queue after this function is called. Only when the return
  // value is 0 is the queue guaranteed to be empty.
  int RunThroughDelayedTasks() {
    int numTasks = mTaskQueue.Length();
    for (int i = 0; i < numTasks; i++) {
      RunDelayedTask();
    }
    return numTasks;
  }

private:
  nsTArray<Task*> mTaskQueue;
};

class TestAPZCTreeManager : public APZCTreeManager {
public:
  nsRefPtr<InputQueue> GetInputQueue() const {
    return mInputQueue;
  }

protected:
  AsyncPanZoomController* MakeAPZCInstance(uint64_t aLayersId, GeckoContentController* aController) override;
};

class TestAsyncPanZoomController : public AsyncPanZoomController {
public:
  TestAsyncPanZoomController(uint64_t aLayersId, GeckoContentController* aMcc,
                             TestAPZCTreeManager* aTreeManager,
                             GestureBehavior aBehavior = DEFAULT_GESTURES)
    : AsyncPanZoomController(aLayersId, aTreeManager, aTreeManager->GetInputQueue(), aMcc, aBehavior)
    , mWaitForMainThread(false)
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
    nsRefPtr<AsyncPanZoomController> target = this;
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

  void AssertStateIsReset() const {
    ReentrantMonitorAutoEnter lock(mMonitor);
    EXPECT_EQ(NOTHING, mState);
  }

  void AssertStateIsFling() const {
    ReentrantMonitorAutoEnter lock(mMonitor);
    EXPECT_EQ(FLING, mState);
  }

  void AdvanceAnimationsUntilEnd(TimeStamp& aSampleTime,
                                 const TimeDuration& aIncrement = TimeDuration::FromMilliseconds(10)) {
    while (AdvanceAnimations(aSampleTime)) {
      aSampleTime += aIncrement;
    }
  }

  bool SampleContentTransformForFrame(const TimeStamp& aSampleTime,
                                      ViewTransform* aOutTransform,
                                      ParentLayerPoint& aScrollOffset) {
    bool ret = AdvanceAnimations(aSampleTime);
    AsyncPanZoomController::SampleContentTransformForFrame(
      aOutTransform, aScrollOffset);
    return ret;
  }

  void SetWaitForMainThread() {
    mWaitForMainThread = true;
  }

private:
  bool mWaitForMainThread;
};

AsyncPanZoomController*
TestAPZCTreeManager::MakeAPZCInstance(uint64_t aLayersId, GeckoContentController* aController)
{
  return new TestAsyncPanZoomController(aLayersId, aController, this, AsyncPanZoomController::USE_GESTURE_DETECTOR);
}

static FrameMetrics
TestFrameMetrics()
{
  FrameMetrics fm;

  fm.SetDisplayPort(CSSRect(0, 0, 10, 10));
  fm.mCompositionBounds = ParentLayerRect(0, 0, 10, 10);
  fm.SetCriticalDisplayPort(CSSRect(0, 0, 10, 10));
  fm.SetScrollableRect(CSSRect(0, 0, 100, 100));

  return fm;
}

class APZCBasicTester : public ::testing::Test {
public:
  explicit APZCBasicTester(AsyncPanZoomController::GestureBehavior aGestureBehavior = AsyncPanZoomController::DEFAULT_GESTURES)
    : mGestureBehavior(aGestureBehavior)
  {
  }

protected:
  virtual void SetUp()
  {
    gfxPrefs::GetSingleton();
    APZThreadUtils::SetThreadAssertionsEnabled(false);
    APZThreadUtils::SetControllerThread(MessageLoop::current());

    testStartTime = TimeStamp::Now();
    AsyncPanZoomController::SetFrameTime(testStartTime);

    mcc = new NiceMock<MockContentControllerDelayed>();
    tm = new TestAPZCTreeManager();
    apzc = new TestAsyncPanZoomController(0, mcc, tm, mGestureBehavior);
    apzc->SetFrameMetrics(TestFrameMetrics());
  }

  virtual void TearDown()
  {
    apzc->Destroy();
  }

  void MakeApzcWaitForMainThread()
  {
    apzc->SetWaitForMainThread();
  }

  void MakeApzcZoomable()
  {
    apzc->UpdateZoomConstraints(ZoomConstraints(true, true, CSSToParentLayerScale(0.25f), CSSToParentLayerScale(4.0f)));
  }

  void MakeApzcUnzoomable()
  {
    apzc->UpdateZoomConstraints(ZoomConstraints(false, false, CSSToParentLayerScale(1.0f), CSSToParentLayerScale(1.0f)));
  }

  void TestOverscroll();

  AsyncPanZoomController::GestureBehavior mGestureBehavior;
  TimeStamp testStartTime;
  nsRefPtr<MockContentControllerDelayed> mcc;
  nsRefPtr<TestAPZCTreeManager> tm;
  nsRefPtr<TestAsyncPanZoomController> apzc;
};

class APZCGestureDetectorTester : public APZCBasicTester {
public:
  APZCGestureDetectorTester()
    : APZCBasicTester(AsyncPanZoomController::USE_GESTURE_DETECTOR)
  {
  }
};

/* The InputReceiver template parameter used in the helper functions below needs
 * to be a class that implements functions with the signatures:
 * nsEventStatus ReceiveInputEvent(const InputData& aEvent,
 *                                 ScrollableLayerGuid* aGuid,
 *                                 uint64_t* aOutInputBlockId);
 * void SetAllowedTouchBehavior(uint64_t aInputBlockId,
 *                              const nsTArray<uint32_t>& aBehaviours);
 * The classes that currently implement these are APZCTreeManager and
 * TestAsyncPanZoomController. Using this template allows us to test individual
 * APZC instances in isolation and also an entire APZ tree, while using the same
 * code to dispatch input events.
 */

// Some helper functions for constructing input event objects suitable to be
// passed either to an APZC (which expects an transformed point), or to an APZTM
// (which expects an untransformed point). We handle both cases by setting both
// the transformed and untransformed fields to the same value.
static SingleTouchData
CreateSingleTouchData(int32_t aIdentifier, int aX, int aY)
{
  SingleTouchData touch(aIdentifier, ScreenIntPoint(aX, aY), ScreenSize(0, 0), 0, 0);
  touch.mLocalScreenPoint = ParentLayerPoint(aX, aY);
  return touch;
}
static PinchGestureInput
CreatePinchGestureInput(PinchGestureInput::PinchGestureType aType,
                        int aFocusX, int aFocusY,
                        float aCurrentSpan, float aPreviousSpan)
{
  PinchGestureInput result(aType, 0, TimeStamp(), ScreenPoint(aFocusX, aFocusY),
                           aCurrentSpan, aPreviousSpan, 0);
  result.mLocalFocusPoint = ParentLayerPoint(aFocusX, aFocusY);
  return result;
}

template<class InputReceiver> static void
SetDefaultAllowedTouchBehavior(const nsRefPtr<InputReceiver>& aTarget,
                               uint64_t aInputBlockId,
                               int touchPoints = 1)
{
  nsTArray<uint32_t> defaultBehaviors;
  // use the default value where everything is allowed
  for (int i = 0; i < touchPoints; i++) {
    defaultBehaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::HORIZONTAL_PAN
                                 | mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN
                                 | mozilla::layers::AllowedTouchBehavior::PINCH_ZOOM
                                 | mozilla::layers::AllowedTouchBehavior::DOUBLE_TAP_ZOOM);
  }
  aTarget->SetAllowedTouchBehavior(aInputBlockId, defaultBehaviors);
}

template<class InputReceiver> static nsEventStatus
TouchDown(const nsRefPtr<InputReceiver>& aTarget, int aX, int aY, int aTime, uint64_t* aOutInputBlockId = nullptr)
{
  MultiTouchInput mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_START, aTime, TimeStamp(), 0);
  mti.mTouches.AppendElement(CreateSingleTouchData(0, aX, aY));
  return aTarget->ReceiveInputEvent(mti, nullptr, aOutInputBlockId);
}

template<class InputReceiver> static nsEventStatus
TouchMove(const nsRefPtr<InputReceiver>& aTarget, int aX, int aY, int aTime)
{
  MultiTouchInput mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, aTime, TimeStamp(), 0);
  mti.mTouches.AppendElement(CreateSingleTouchData(0, aX, aY));
  return aTarget->ReceiveInputEvent(mti, nullptr, nullptr);
}

template<class InputReceiver> static nsEventStatus
TouchUp(const nsRefPtr<InputReceiver>& aTarget, int aX, int aY, int aTime)
{
  MultiTouchInput mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_END, aTime, TimeStamp(), 0);
  mti.mTouches.AppendElement(CreateSingleTouchData(0, aX, aY));
  return aTarget->ReceiveInputEvent(mti, nullptr, nullptr);
}

template<class InputReceiver> static void
Tap(const nsRefPtr<InputReceiver>& aTarget, int aX, int aY, int& aTime, int aTapLength,
    nsEventStatus (*aOutEventStatuses)[2] = nullptr,
    uint64_t* aOutInputBlockId = nullptr)
{
  // Even if the caller doesn't care about the block id, we need it to set the
  // allowed touch behaviour below, so make sure aOutInputBlockId is non-null.
  uint64_t blockId;
  if (!aOutInputBlockId) {
    aOutInputBlockId = &blockId;
  }

  nsEventStatus status = TouchDown(aTarget, aX, aY, aTime, aOutInputBlockId);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[0] = status;
  }
  aTime += aTapLength;

  // If touch-action is enabled then simulate the allowed touch behaviour
  // notification that the main thread is supposed to deliver.
  if (gfxPrefs::TouchActionEnabled() && status != nsEventStatus_eConsumeNoDefault) {
    SetDefaultAllowedTouchBehavior(aTarget, *aOutInputBlockId);
  }

  status = TouchUp(aTarget, aX, aY, aTime);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[1] = status;
  }
}

template<class InputReceiver> static void
TapAndCheckStatus(const nsRefPtr<InputReceiver>& aTarget, int aX, int aY, int& aTime, int aTapLength)
{
  nsEventStatus statuses[2];
  Tap(aTarget, aX, aY, aTime, aTapLength, &statuses);
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, statuses[0]);
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, statuses[1]);
}

template<class InputReceiver> static void
Pan(const nsRefPtr<InputReceiver>& aTarget,
    int& aTime,
    int aTouchStartY,
    int aTouchEndY,
    bool aKeepFingerDown = false,
    nsTArray<uint32_t>* aAllowedTouchBehaviors = nullptr,
    nsEventStatus (*aOutEventStatuses)[4] = nullptr,
    uint64_t* aOutInputBlockId = nullptr)
{
  // Reduce the touch start tolerance to a tiny value.
  // We can't use a scoped pref because this value might be read at some later
  // time when the events are actually processed, rather than when we deliver
  // them.
  gfxPrefs::SetAPZTouchStartTolerance(1.0f / 1000.0f);
  const int OVERCOME_TOUCH_TOLERANCE = 1;

  const int TIME_BETWEEN_TOUCH_EVENT = 100;

  // Even if the caller doesn't care about the block id, we need it to set the
  // allowed touch behaviour below, so make sure aOutInputBlockId is non-null.
  uint64_t blockId;
  if (!aOutInputBlockId) {
    aOutInputBlockId = &blockId;
  }

  // Make sure the move is large enough to not be handled as a tap
  nsEventStatus status = TouchDown(aTarget, 10, aTouchStartY + OVERCOME_TOUCH_TOLERANCE, aTime, aOutInputBlockId);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[0] = status;
  }

  aTime += TIME_BETWEEN_TOUCH_EVENT;

  // Allowed touch behaviours must be set after sending touch-start.
  if (status != nsEventStatus_eConsumeNoDefault) {
    if (aAllowedTouchBehaviors) {
      EXPECT_EQ(1UL, aAllowedTouchBehaviors->Length());
      aTarget->SetAllowedTouchBehavior(*aOutInputBlockId, *aAllowedTouchBehaviors);
    } else if (gfxPrefs::TouchActionEnabled()) {
      SetDefaultAllowedTouchBehavior(aTarget, *aOutInputBlockId);
    }
  }

  status = TouchMove(aTarget, 10, aTouchStartY, aTime);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[1] = status;
  }

  aTime += TIME_BETWEEN_TOUCH_EVENT;

  status = TouchMove(aTarget, 10, aTouchEndY, aTime);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[2] = status;
  }

  aTime += TIME_BETWEEN_TOUCH_EVENT;

  if (!aKeepFingerDown) {
    status = TouchUp(aTarget, 10, aTouchEndY, aTime);
  } else {
    status = nsEventStatus_eIgnore;
  }
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[3] = status;
  }

  aTime += TIME_BETWEEN_TOUCH_EVENT;
}

/*
 * Dispatches mock touch events to the apzc and checks whether apzc properly
 * consumed them and triggered scrolling behavior.
 */
template<class InputReceiver> static void
PanAndCheckStatus(const nsRefPtr<InputReceiver>& aTarget,
                  int& aTime,
                  int aTouchStartY,
                  int aTouchEndY,
                  bool aExpectConsumed,
                  nsTArray<uint32_t>* aAllowedTouchBehaviors,
                  uint64_t* aOutInputBlockId = nullptr)
{
  nsEventStatus statuses[4]; // down, move, move, up
  Pan(aTarget, aTime, aTouchStartY, aTouchEndY, false, aAllowedTouchBehaviors, &statuses, aOutInputBlockId);

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

static void
ApzcPanNoFling(const nsRefPtr<TestAsyncPanZoomController>& aApzc,
               int& aTime,
               int aTouchStartY,
               int aTouchEndY,
               uint64_t* aOutInputBlockId = nullptr)
{
  Pan(aApzc, aTime, aTouchStartY, aTouchEndY, false, nullptr, nullptr, aOutInputBlockId);
  aApzc->CancelAnimation();
}

template<class InputReceiver> static void
PinchWithPinchInput(const nsRefPtr<InputReceiver>& aTarget,
                    int aFocusX, int aFocusY, float aScale,
                    nsEventStatus (*aOutEventStatuses)[3] = nullptr)
{
  nsEventStatus actualStatus = aTarget->ReceiveInputEvent(
      CreatePinchGestureInput(PinchGestureInput::PINCHGESTURE_START,
                              aFocusX, aFocusY, 10.0, 10.0),
      nullptr);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[0] = actualStatus;
  }
  actualStatus = aTarget->ReceiveInputEvent(
      CreatePinchGestureInput(PinchGestureInput::PINCHGESTURE_SCALE,
                              aFocusX, aFocusY, 10.0 * aScale, 10.0),
      nullptr);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[1] = actualStatus;
  }
  actualStatus = aTarget->ReceiveInputEvent(
      CreatePinchGestureInput(PinchGestureInput::PINCHGESTURE_END,
                              // note: negative values here tell APZC
                              //       not to turn the pinch into a pan
                              aFocusX, aFocusY, -1.0, -1.0),
      nullptr);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[2] = actualStatus;
  }
}

template<class InputReceiver> static void
PinchWithPinchInputAndCheckStatus(const nsRefPtr<InputReceiver>& aTarget,
                                  int aFocusX, int aFocusY, float aScale,
                                  bool aShouldTriggerPinch)
{
  nsEventStatus statuses[3];  // scalebegin, scale, scaleend
  PinchWithPinchInput(aTarget, aFocusX, aFocusY, aScale, &statuses);

  nsEventStatus expectedStatus = aShouldTriggerPinch
      ? nsEventStatus_eConsumeNoDefault
      : nsEventStatus_eIgnore;
  EXPECT_EQ(expectedStatus, statuses[0]);
  EXPECT_EQ(expectedStatus, statuses[1]);
}

template<class InputReceiver> static void
PinchWithTouchInput(const nsRefPtr<InputReceiver>& aTarget,
                    int aFocusX, int aFocusY, float aScale,
                    int& inputId,
                    nsTArray<uint32_t>* aAllowedTouchBehaviors = nullptr,
                    nsEventStatus (*aOutEventStatuses)[4] = nullptr,
                    uint64_t* aOutInputBlockId = nullptr)
{
  // Having pinch coordinates in float type may cause problems with high-precision scale values
  // since SingleTouchData accepts integer value. But for trivial tests it should be ok.
  float pinchLength = 100.0;
  float pinchLengthScaled = pinchLength * aScale;

  // Even if the caller doesn't care about the block id, we need it to set the
  // allowed touch behaviour below, so make sure aOutInputBlockId is non-null.
  uint64_t blockId;
  if (!aOutInputBlockId) {
    aOutInputBlockId = &blockId;
  }

  MultiTouchInput mtiStart = MultiTouchInput(MultiTouchInput::MULTITOUCH_START, 0, TimeStamp(), 0);
  mtiStart.mTouches.AppendElement(CreateSingleTouchData(inputId, aFocusX, aFocusY));
  mtiStart.mTouches.AppendElement(CreateSingleTouchData(inputId + 1, aFocusX, aFocusY));
  nsEventStatus status = aTarget->ReceiveInputEvent(mtiStart, aOutInputBlockId);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[0] = status;
  }

  if (aAllowedTouchBehaviors) {
    EXPECT_EQ(2UL, aAllowedTouchBehaviors->Length());
    aTarget->SetAllowedTouchBehavior(*aOutInputBlockId, *aAllowedTouchBehaviors);
  } else if (gfxPrefs::TouchActionEnabled()) {
    SetDefaultAllowedTouchBehavior(aTarget, *aOutInputBlockId, 2);
  }

  MultiTouchInput mtiMove1 = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, 0, TimeStamp(), 0);
  mtiMove1.mTouches.AppendElement(CreateSingleTouchData(inputId, aFocusX - pinchLength, aFocusY));
  mtiMove1.mTouches.AppendElement(CreateSingleTouchData(inputId + 1, aFocusX + pinchLength, aFocusY));
  status = aTarget->ReceiveInputEvent(mtiMove1, nullptr);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[1] = status;
  }

  MultiTouchInput mtiMove2 = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, 0, TimeStamp(), 0);
  mtiMove2.mTouches.AppendElement(CreateSingleTouchData(inputId, aFocusX - pinchLengthScaled, aFocusY));
  mtiMove2.mTouches.AppendElement(CreateSingleTouchData(inputId + 1, aFocusX + pinchLengthScaled, aFocusY));
  status = aTarget->ReceiveInputEvent(mtiMove2, nullptr);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[2] = status;
  }

  MultiTouchInput mtiEnd = MultiTouchInput(MultiTouchInput::MULTITOUCH_END, 0, TimeStamp(), 0);
  mtiEnd.mTouches.AppendElement(CreateSingleTouchData(inputId, aFocusX - pinchLengthScaled, aFocusY));
  mtiEnd.mTouches.AppendElement(CreateSingleTouchData(inputId + 1, aFocusX + pinchLengthScaled, aFocusY));
  status = aTarget->ReceiveInputEvent(mtiEnd, nullptr);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[3] = status;
  }

  inputId += 2;
}

template<class InputReceiver> static void
PinchWithTouchInputAndCheckStatus(const nsRefPtr<InputReceiver>& aTarget,
                                  int aFocusX, int aFocusY, float aScale,
                                  int& inputId, bool aShouldTriggerPinch,
                                  nsTArray<uint32_t>* aAllowedTouchBehaviors)
{
  nsEventStatus statuses[4];  // down, move, move, up
  PinchWithTouchInput(aTarget, aFocusX, aFocusY, aScale, inputId, aAllowedTouchBehaviors, &statuses);

  nsEventStatus expectedMoveStatus = aShouldTriggerPinch
      ? nsEventStatus_eConsumeDoDefault
      : nsEventStatus_eIgnore;
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, statuses[0]);
  EXPECT_EQ(expectedMoveStatus, statuses[1]);
  EXPECT_EQ(expectedMoveStatus, statuses[2]);
}

class APZCPinchTester : public APZCBasicTester {
public:
  explicit APZCPinchTester(AsyncPanZoomController::GestureBehavior aGestureBehavior = AsyncPanZoomController::DEFAULT_GESTURES)
    : APZCBasicTester(aGestureBehavior)
  {
  }

protected:
  FrameMetrics GetPinchableFrameMetrics()
  {
    FrameMetrics fm;
    fm.mCompositionBounds = ParentLayerRect(200, 200, 100, 200);
    fm.SetScrollableRect(CSSRect(0, 0, 980, 1000));
    fm.SetScrollOffset(CSSPoint(300, 300));
    fm.SetZoom(CSSToParentLayerScale2D(2.0, 2.0));
    // APZC only allows zooming on the root scrollable frame.
    fm.SetIsRoot(true);
    // the visible area of the document in CSS pixels is x=300 y=300 w=50 h=100
    return fm;
  }

  void DoPinchTest(bool aShouldTriggerPinch,
                   nsTArray<uint32_t> *aAllowedTouchBehaviors = nullptr)
  {
    apzc->SetFrameMetrics(GetPinchableFrameMetrics());
    MakeApzcZoomable();

    if (aShouldTriggerPinch) {
      EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_,_)).Times(AtLeast(1));
      EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);
    } else {
      EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_,_)).Times(AtMost(2));
      EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(0);
    }

    int touchInputId = 0;
    if (mGestureBehavior == AsyncPanZoomController::USE_GESTURE_DETECTOR) {
      PinchWithTouchInputAndCheckStatus(apzc, 250, 300, 1.25, touchInputId, aShouldTriggerPinch, aAllowedTouchBehaviors);
    } else {
      PinchWithPinchInputAndCheckStatus(apzc, 250, 300, 1.25, aShouldTriggerPinch);
    }

    FrameMetrics fm = apzc->GetFrameMetrics();

    if (aShouldTriggerPinch) {
      // the visible area of the document in CSS pixels is now x=305 y=310 w=40 h=80
      EXPECT_EQ(2.5f, fm.GetZoom().ToScaleFactor().scale);
      EXPECT_EQ(305, fm.GetScrollOffset().x);
      EXPECT_EQ(310, fm.GetScrollOffset().y);
    } else {
      // The frame metrics should stay the same since touch-action:none makes
      // apzc ignore pinch gestures.
      EXPECT_EQ(2.0f, fm.GetZoom().ToScaleFactor().scale);
      EXPECT_EQ(300, fm.GetScrollOffset().x);
      EXPECT_EQ(300, fm.GetScrollOffset().y);
    }

    // part 2 of the test, move to the top-right corner of the page and pinch and
    // make sure we stay in the correct spot
    fm.SetZoom(CSSToParentLayerScale2D(2.0, 2.0));
    fm.SetScrollOffset(CSSPoint(930, 5));
    apzc->SetFrameMetrics(fm);
    // the visible area of the document in CSS pixels is x=930 y=5 w=50 h=100

    if (mGestureBehavior == AsyncPanZoomController::USE_GESTURE_DETECTOR) {
      PinchWithTouchInputAndCheckStatus(apzc, 250, 300, 0.5, touchInputId, aShouldTriggerPinch, aAllowedTouchBehaviors);
    } else {
      PinchWithPinchInputAndCheckStatus(apzc, 250, 300, 0.5, aShouldTriggerPinch);
    }

    fm = apzc->GetFrameMetrics();

    if (aShouldTriggerPinch) {
      // the visible area of the document in CSS pixels is now x=880 y=0 w=100 h=200
      EXPECT_EQ(1.0f, fm.GetZoom().ToScaleFactor().scale);
      EXPECT_EQ(880, fm.GetScrollOffset().x);
      EXPECT_EQ(0, fm.GetScrollOffset().y);
    } else {
      EXPECT_EQ(2.0f, fm.GetZoom().ToScaleFactor().scale);
      EXPECT_EQ(930, fm.GetScrollOffset().x);
      EXPECT_EQ(5, fm.GetScrollOffset().y);
    }
  }
};

class APZCPinchGestureDetectorTester : public APZCPinchTester {
public:
  APZCPinchGestureDetectorTester()
    : APZCPinchTester(AsyncPanZoomController::USE_GESTURE_DETECTOR)
  {
  }
};

TEST_F(APZCPinchTester, Pinch_DefaultGestures_NoTouchAction) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, false);
  DoPinchTest(true);
}

TEST_F(APZCPinchGestureDetectorTester, Pinch_UseGestureDetector_NoTouchAction) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, false);
  DoPinchTest(true);
}

TEST_F(APZCPinchGestureDetectorTester, Pinch_UseGestureDetector_TouchActionNone) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, true);
  nsTArray<uint32_t> behaviors;
  behaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::NONE);
  behaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::NONE);
  DoPinchTest(false, &behaviors);
}

TEST_F(APZCPinchGestureDetectorTester, Pinch_UseGestureDetector_TouchActionZoom) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, true);
  nsTArray<uint32_t> behaviors;
  behaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::PINCH_ZOOM);
  behaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::PINCH_ZOOM);
  DoPinchTest(true, &behaviors);
}

TEST_F(APZCPinchGestureDetectorTester, Pinch_UseGestureDetector_TouchActionNotAllowZoom) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, true);
  nsTArray<uint32_t> behaviors;
  behaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN);
  behaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::PINCH_ZOOM);
  DoPinchTest(false, &behaviors);
}

TEST_F(APZCPinchGestureDetectorTester, Pinch_PreventDefault) {
  FrameMetrics originalMetrics = GetPinchableFrameMetrics();
  apzc->SetFrameMetrics(originalMetrics);

  MakeApzcWaitForMainThread();
  MakeApzcZoomable();

  int touchInputId = 0;
  uint64_t blockId = 0;
  PinchWithTouchInput(apzc, 250, 300, 1.25, touchInputId, nullptr, nullptr, &blockId);

  // Send the prevent-default notification for the touch block
  apzc->ContentReceivedInputBlock(blockId, true);

  // Run all pending tasks (this should include at least the
  // prevent-default timer).
  EXPECT_LE(1, mcc->RunThroughDelayedTasks());

  // verify the metrics didn't change (i.e. the pinch was ignored)
  FrameMetrics fm = apzc->GetFrameMetrics();
  EXPECT_EQ(originalMetrics.GetZoom(), fm.GetZoom());
  EXPECT_EQ(originalMetrics.GetScrollOffset().x, fm.GetScrollOffset().x);
  EXPECT_EQ(originalMetrics.GetScrollOffset().y, fm.GetScrollOffset().y);

  apzc->AssertStateIsReset();
}

TEST_F(APZCBasicTester, Overzoom) {
  // the visible area of the document in CSS pixels is x=10 y=0 w=100 h=100
  FrameMetrics fm;
  fm.mCompositionBounds = ParentLayerRect(0, 0, 100, 100);
  fm.SetScrollableRect(CSSRect(0, 0, 125, 150));
  fm.SetScrollOffset(CSSPoint(10, 0));
  fm.SetZoom(CSSToParentLayerScale2D(1.0, 1.0));
  fm.SetIsRoot(true);
  apzc->SetFrameMetrics(fm);

  MakeApzcZoomable();

  EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_,_)).Times(AtLeast(1));
  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);

  PinchWithPinchInputAndCheckStatus(apzc, 50, 50, 0.5, true);

  fm = apzc->GetFrameMetrics();
  EXPECT_EQ(0.8f, fm.GetZoom().ToScaleFactor().scale);
  // bug 936721 - PGO builds introduce rounding error so
  // use a fuzzy match instead
  EXPECT_LT(abs(fm.GetScrollOffset().x), 1e-5);
  EXPECT_LT(abs(fm.GetScrollOffset().y), 1e-5);
}

TEST_F(APZCBasicTester, SimpleTransform) {
  ParentLayerPoint pointOut;
  ViewTransform viewTransformOut;
  apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);

  EXPECT_EQ(ParentLayerPoint(), pointOut);
  EXPECT_EQ(ViewTransform(), viewTransformOut);
}


TEST_F(APZCBasicTester, ComplexTransform) {
  // This test assumes there is a page that gets rendered to
  // two layers. In CSS pixels, the first layer is 50x50 and
  // the second layer is 25x50. The widget scale factor is 3.0
  // and the presShell resolution is 2.0. Therefore, these layers
  // end up being 300x300 and 150x300 in layer pixels.
  //
  // The second (child) layer has an additional CSS transform that
  // stretches it by 2.0 on the x-axis. Therefore, after applying
  // CSS transforms, the two layers are the same size in screen
  // pixels.
  //
  // The screen itself is 24x24 in screen pixels (therefore 4x4 in
  // CSS pixels). The displayport is 1 extra CSS pixel on all
  // sides.

  nsRefPtr<TestAsyncPanZoomController> childApzc = new TestAsyncPanZoomController(0, mcc, tm);

  const char* layerTreeSyntax = "c(c)";
  // LayerID                     0 1
  nsIntRegion layerVisibleRegion[] = {
    nsIntRegion(nsIntRect(0, 0, 300, 300)),
    nsIntRegion(nsIntRect(0, 0, 150, 300)),
  };
  Matrix4x4 transforms[] = {
    Matrix4x4(),
    Matrix4x4(),
  };
  transforms[0].PostScale(0.5f, 0.5f, 1.0f); // this results from the 2.0 resolution on the root layer
  transforms[1].PostScale(2.0f, 1.0f, 1.0f); // this is the 2.0 x-axis CSS transform on the child layer

  nsTArray<nsRefPtr<Layer> > layers;
  nsRefPtr<LayerManager> lm;
  nsRefPtr<Layer> root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, transforms, lm, layers);

  FrameMetrics metrics;
  metrics.mCompositionBounds = ParentLayerRect(0, 0, 24, 24);
  metrics.SetDisplayPort(CSSRect(-1, -1, 6, 6));
  metrics.SetScrollOffset(CSSPoint(10, 10));
  metrics.SetScrollableRect(CSSRect(0, 0, 50, 50));
  metrics.SetCumulativeResolution(LayoutDeviceToLayerScale2D(2, 2));
  metrics.SetPresShellResolution(2.0f);
  metrics.SetZoom(CSSToParentLayerScale2D(6, 6));
  metrics.SetDevPixelsPerCSSPixel(CSSToLayoutDeviceScale(3));
  metrics.SetScrollId(FrameMetrics::START_SCROLL_ID);

  FrameMetrics childMetrics = metrics;
  childMetrics.SetScrollId(FrameMetrics::START_SCROLL_ID + 1);

  layers[0]->SetFrameMetrics(metrics);
  layers[1]->SetFrameMetrics(childMetrics);

  ParentLayerPoint pointOut;
  ViewTransform viewTransformOut;

  // Both the parent and child layer should behave exactly the same here, because
  // the CSS transform on the child layer does not affect the SampleContentTransformForFrame code

  // initial transform
  apzc->SetFrameMetrics(metrics);
  apzc->NotifyLayersUpdated(metrics, true);
  apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(LayerToParentLayerScale(1), ParentLayerPoint()), viewTransformOut);
  EXPECT_EQ(ParentLayerPoint(60, 60), pointOut);

  childApzc->SetFrameMetrics(childMetrics);
  childApzc->NotifyLayersUpdated(childMetrics, true);
  childApzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(LayerToParentLayerScale(1), ParentLayerPoint()), viewTransformOut);
  EXPECT_EQ(ParentLayerPoint(60, 60), pointOut);

  // do an async scroll by 5 pixels and check the transform
  metrics.ScrollBy(CSSPoint(5, 0));
  apzc->SetFrameMetrics(metrics);
  apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(LayerToParentLayerScale(1), ParentLayerPoint(-30, 0)), viewTransformOut);
  EXPECT_EQ(ParentLayerPoint(90, 60), pointOut);

  childMetrics.ScrollBy(CSSPoint(5, 0));
  childApzc->SetFrameMetrics(childMetrics);
  childApzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(LayerToParentLayerScale(1), ParentLayerPoint(-30, 0)), viewTransformOut);
  EXPECT_EQ(ParentLayerPoint(90, 60), pointOut);

  // do an async zoom of 1.5x and check the transform
  metrics.ZoomBy(1.5f);
  apzc->SetFrameMetrics(metrics);
  apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(LayerToParentLayerScale(1.5), ParentLayerPoint(-45, 0)), viewTransformOut);
  EXPECT_EQ(ParentLayerPoint(135, 90), pointOut);

  childMetrics.ZoomBy(1.5f);
  childApzc->SetFrameMetrics(childMetrics);
  childApzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(LayerToParentLayerScale(1.5), ParentLayerPoint(-45, 0)), viewTransformOut);
  EXPECT_EQ(ParentLayerPoint(135, 90), pointOut);

  childApzc->Destroy();
}

class APZCPanningTester : public APZCBasicTester {
protected:
  void DoPanTest(bool aShouldTriggerScroll, bool aShouldBeConsumed, uint32_t aBehavior)
  {
    if (aShouldTriggerScroll) {
      EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_,_)).Times(AtLeast(1));
      EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);
    } else {
      EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_,_)).Times(0);
      EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(0);
    }

    int time = 0;
    int touchStart = 50;
    int touchEnd = 10;
    ParentLayerPoint pointOut;
    ViewTransform viewTransformOut;

    nsTArray<uint32_t> allowedTouchBehaviors;
    allowedTouchBehaviors.AppendElement(aBehavior);

    // Pan down
    PanAndCheckStatus(apzc, time, touchStart, touchEnd, aShouldBeConsumed, &allowedTouchBehaviors);
    apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);

    if (aShouldTriggerScroll) {
      EXPECT_EQ(ParentLayerPoint(0, -(touchEnd-touchStart)), pointOut);
      EXPECT_NE(ViewTransform(), viewTransformOut);
    } else {
      EXPECT_EQ(ParentLayerPoint(), pointOut);
      EXPECT_EQ(ViewTransform(), viewTransformOut);
    }

    // Clear the fling from the previous pan, or stopping it will
    // consume the next touchstart
    apzc->CancelAnimation();

    // Pan back
    PanAndCheckStatus(apzc, time, touchEnd, touchStart, aShouldBeConsumed, &allowedTouchBehaviors);
    apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);

    EXPECT_EQ(ParentLayerPoint(), pointOut);
    EXPECT_EQ(ViewTransform(), viewTransformOut);
  }

  void DoPanWithPreventDefaultTest()
  {
    MakeApzcWaitForMainThread();

    int time = 0;
    int touchStart = 50;
    int touchEnd = 10;
    ParentLayerPoint pointOut;
    ViewTransform viewTransformOut;
    uint64_t blockId = 0;

    // Pan down
    nsTArray<uint32_t> allowedTouchBehaviors;
    allowedTouchBehaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN);
    PanAndCheckStatus(apzc, time, touchStart, touchEnd, true, &allowedTouchBehaviors, &blockId);

    // Send the signal that content has handled and preventDefaulted the touch
    // events. This flushes the event queue.
    apzc->ContentReceivedInputBlock(blockId, true);
    // Run all pending tasks (this should include at least the
    // prevent-default timer).
    EXPECT_LE(1, mcc->RunThroughDelayedTasks());

    apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
    EXPECT_EQ(ParentLayerPoint(), pointOut);
    EXPECT_EQ(ViewTransform(), viewTransformOut);

    apzc->AssertStateIsReset();
  }
};

TEST_F(APZCPanningTester, Pan) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, false);
  DoPanTest(true, true, mozilla::layers::AllowedTouchBehavior::NONE);
}

// In the each of the following 4 pan tests we are performing two pan gestures: vertical pan from top
// to bottom and back - from bottom to top.
// According to the pointer-events/touch-action spec AUTO and PAN_Y touch-action values allow vertical
// scrolling while NONE and PAN_X forbid it. The first parameter of DoPanTest method specifies this
// behavior.
// However, the events will be marked as consumed even if the behavior in PAN_X, because the user could
// move their finger horizontally too - APZ has no way of knowing beforehand and so must consume the
// events.
TEST_F(APZCPanningTester, PanWithTouchActionAuto) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, true);
  DoPanTest(true, true, mozilla::layers::AllowedTouchBehavior::HORIZONTAL_PAN
                      | mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN);
}

TEST_F(APZCPanningTester, PanWithTouchActionNone) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, true);
  DoPanTest(false, false, 0);
}

TEST_F(APZCPanningTester, PanWithTouchActionPanX) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, true);
  DoPanTest(false, true, mozilla::layers::AllowedTouchBehavior::HORIZONTAL_PAN);
}

TEST_F(APZCPanningTester, PanWithTouchActionPanY) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, true);
  DoPanTest(true, true, mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN);
}

TEST_F(APZCPanningTester, PanWithPreventDefaultAndTouchAction) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, true);
  DoPanWithPreventDefaultTest();
}

TEST_F(APZCPanningTester, PanWithPreventDefault) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, false);
  DoPanWithPreventDefaultTest();
}

TEST_F(APZCBasicTester, Fling) {
  EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_,_)).Times(AtLeast(1));
  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);

  int time = 0;
  int touchStart = 50;
  int touchEnd = 10;
  ParentLayerPoint pointOut;
  ViewTransform viewTransformOut;

  // Fling down. Each step scroll further down
  Pan(apzc, time, touchStart, touchEnd);
  ParentLayerPoint lastPoint;
  for (int i = 1; i < 50; i+=1) {
    apzc->SampleContentTransformForFrame(testStartTime+TimeDuration::FromMilliseconds(i), &viewTransformOut, pointOut);
    EXPECT_GT(pointOut.y, lastPoint.y);
    lastPoint = pointOut;
  }
}

TEST_F(APZCBasicTester, FlingIntoOverscroll) {
  // Enable overscrolling.
  SCOPED_GFX_PREF(APZOverscrollEnabled, bool, true);

  // Scroll down by 25 px. Don't fling for simplicity.
  int time = 0;
  ApzcPanNoFling(apzc, time, 50, 25);

  // Now scroll back up by 20px, this time flinging after.
  // The fling should cover the remaining 5 px of room to scroll, then
  // go into overscroll, and finally snap-back to recover from overscroll.
  Pan(apzc, time, 25, 45);
  const TimeDuration increment = TimeDuration::FromMilliseconds(1);
  bool reachedOverscroll = false;
  bool recoveredFromOverscroll = false;
  while (apzc->AdvanceAnimations(testStartTime)) {
    if (!reachedOverscroll && apzc->IsOverscrolled()) {
      reachedOverscroll = true;
    }
    if (reachedOverscroll && !apzc->IsOverscrolled()) {
      recoveredFromOverscroll = true;
    }
    testStartTime += increment;
  }
  EXPECT_TRUE(reachedOverscroll);
  EXPECT_TRUE(recoveredFromOverscroll);
}

TEST_F(APZCBasicTester, PanningTransformNotifications) {
  SCOPED_GFX_PREF(APZOverscrollEnabled, bool, true);

  // Scroll down by 25 px. Ensure we only get one set of
  // state change notifications.
  //
  // Then, scroll back up by 20px, this time flinging after.
  // The fling should cover the remaining 5 px of room to scroll, then
  // go into overscroll, and finally snap-back to recover from overscroll.
  // Again, ensure we only get one set of state change notifications for
  // this entire procedure.

  MockFunction<void(std::string checkPointName)> check;
  {
    InSequence s;
    EXPECT_CALL(check, Call("Simple pan"));
    EXPECT_CALL(*mcc, NotifyAPZStateChange(_,GeckoContentController::APZStateChange::StartTouch,_)).Times(1);
    EXPECT_CALL(*mcc, NotifyAPZStateChange(_,GeckoContentController::APZStateChange::TransformBegin,_)).Times(1);
    EXPECT_CALL(*mcc, NotifyAPZStateChange(_,GeckoContentController::APZStateChange::StartPanning,_)).Times(1);
    EXPECT_CALL(*mcc, NotifyAPZStateChange(_,GeckoContentController::APZStateChange::EndTouch,_)).Times(1);
    EXPECT_CALL(*mcc, NotifyAPZStateChange(_,GeckoContentController::APZStateChange::TransformEnd,_)).Times(1);
    EXPECT_CALL(check, Call("Complex pan"));
    EXPECT_CALL(*mcc, NotifyAPZStateChange(_,GeckoContentController::APZStateChange::StartTouch,_)).Times(1);
    EXPECT_CALL(*mcc, NotifyAPZStateChange(_,GeckoContentController::APZStateChange::TransformBegin,_)).Times(1);
    EXPECT_CALL(*mcc, NotifyAPZStateChange(_,GeckoContentController::APZStateChange::StartPanning,_)).Times(1);
    EXPECT_CALL(*mcc, NotifyAPZStateChange(_,GeckoContentController::APZStateChange::EndTouch,_)).Times(1);
    EXPECT_CALL(*mcc, NotifyAPZStateChange(_,GeckoContentController::APZStateChange::TransformEnd,_)).Times(1);
    EXPECT_CALL(check, Call("Done"));
  }

  int time = 0;
  check.Call("Simple pan");
  ApzcPanNoFling(apzc, time, 50, 25);
  check.Call("Complex pan");
  Pan(apzc, time, 25, 45);
  apzc->AdvanceAnimationsUntilEnd(testStartTime);
  check.Call("Done");
}

void APZCBasicTester::TestOverscroll()
{
  // Pan sufficiently to hit overscroll behavior
  int time = 0;
  int touchStart = 500;
  int touchEnd = 10;
  Pan(apzc, time, touchStart, touchEnd);
  EXPECT_TRUE(apzc->IsOverscrolled());

  // Check that we recover from overscroll via an animation.
  const TimeDuration increment = TimeDuration::FromMilliseconds(1);
  bool recoveredFromOverscroll = false;
  ParentLayerPoint pointOut;
  ViewTransform viewTransformOut;
  while (apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut)) {
    // The reported scroll offset should be the same throughout.
    EXPECT_EQ(ParentLayerPoint(0, 90), pointOut);

    // Trigger computation of the overscroll tranform, to make sure
    // no assetions fire during the calculation.
    apzc->GetOverscrollTransform();

    if (!apzc->IsOverscrolled()) {
      recoveredFromOverscroll = true;
    }

    testStartTime += increment;
  }
  EXPECT_TRUE(recoveredFromOverscroll);
  apzc->AssertStateIsReset();
}


TEST_F(APZCBasicTester, OverScrollPanning) {
  SCOPED_GFX_PREF(APZOverscrollEnabled, bool, true);

  TestOverscroll();
}

// Tests that an overscroll animation doesn't trigger an assertion failure
// in the case where a sample has a velocity of zero.
TEST_F(APZCBasicTester, OverScroll_Bug1152051) {
  SCOPED_GFX_PREF(APZOverscrollEnabled, bool, true);

  // Doctor the prefs to make the velocity zero at the end of the first sample.

  // This ensures our incoming velocity to the overscroll animation is
  // a round(ish) number, 4.9 (that being the distance of the pan before
  // overscroll, which is 500 - 10 = 490 pixels, divided by the duration of
  // the pan, which is 100 ms).
  SCOPED_GFX_PREF(APZFlingFriction, float, 0);

  // To ensure the velocity after the first sample is 0, set the spring
  // stiffness to the incoming velocity (4.9) divided by the overscroll
  // (400 pixels) times the step duration (1 ms).
  SCOPED_GFX_PREF(APZOverscrollSpringStiffness, float, 0.01225f);

  TestOverscroll();
}

TEST_F(APZCBasicTester, OverScrollAbort) {
  SCOPED_GFX_PREF(APZOverscrollEnabled, bool, true);

  // Pan sufficiently to hit overscroll behavior
  int time = 0;
  int touchStart = 500;
  int touchEnd = 10;
  Pan(apzc, time, touchStart, touchEnd);
  EXPECT_TRUE(apzc->IsOverscrolled());

  ParentLayerPoint pointOut;
  ViewTransform viewTransformOut;

  // This sample call will run to the end of the fling animation
  // and will schedule the overscroll animation.
  apzc->SampleContentTransformForFrame(testStartTime + TimeDuration::FromMilliseconds(10000), &viewTransformOut, pointOut);
  EXPECT_TRUE(apzc->IsOverscrolled());

  // At this point, we have an active overscroll animation.
  // Check that cancelling the animation clears the overscroll.
  apzc->CancelAnimation();
  EXPECT_FALSE(apzc->IsOverscrolled());
  apzc->AssertStateIsReset();
}

TEST_F(APZCBasicTester, OverScrollPanningAbort) {
  SCOPED_GFX_PREF(APZOverscrollEnabled, bool, true);

  // Pan sufficiently to hit overscroll behaviour. Keep the finger down so
  // the pan does not end.
  int time = 0;
  int touchStart = 500;
  int touchEnd = 10;
  Pan(apzc, time, touchStart, touchEnd, true); // keep finger down
  EXPECT_TRUE(apzc->IsOverscrolled());

  // Check that calling CancelAnimation() while the user is still panning
  // (and thus no fling or snap-back animation has had a chance to start)
  // clears the overscroll.
  apzc->CancelAnimation();
  EXPECT_FALSE(apzc->IsOverscrolled());
  apzc->AssertStateIsReset();
}


class APZCFlingStopTester : public APZCGestureDetectorTester {
protected:
  // Start a fling, and then tap while the fling is ongoing. When
  // aSlow is false, the tap will happen while the fling is at a
  // high velocity, and we check that the tap doesn't trigger sending a tap
  // to content. If aSlow is true, the tap will happen while the fling
  // is at a slow velocity, and we check that the tap does trigger sending
  // a tap to content. See bug 1022956.
  void DoFlingStopTest(bool aSlow) {
    int time = 0;
    int touchStart = 50;
    int touchEnd = 10;

    // Start the fling down.
    Pan(apzc, time, touchStart, touchEnd);
    // The touchstart from the pan will leave some cancelled tasks in the queue, clear them out
    while (mcc->RunThroughDelayedTasks());

    // If we want to tap while the fling is fast, let the fling advance for 10ms only. If we want
    // the fling to slow down more, advance to 2000ms. These numbers may need adjusting if our
    // friction and threshold values change, but they should be deterministic at least.
    int timeDelta = aSlow ? 2000 : 10;
    int tapCallsExpected = aSlow ? 2 : 1;

    // Advance the fling animation by timeDelta milliseconds.
    ParentLayerPoint pointOut;
    ViewTransform viewTransformOut;
    apzc->SampleContentTransformForFrame(testStartTime + TimeDuration::FromMilliseconds(timeDelta), &viewTransformOut, pointOut);

    // Deliver a tap to abort the fling. Ensure that we get a HandleSingleTap
    // call out of it if and only if the fling is slow.
    EXPECT_CALL(*mcc, HandleSingleTap(_, 0, apzc->GetGuid())).Times(tapCallsExpected);
    Tap(apzc, 10, 10, time, 0);
    while (mcc->RunThroughDelayedTasks());

    // Deliver another tap, to make sure that taps are flowing properly once
    // the fling is aborted.
    time += 500;
    Tap(apzc, 100, 100, time, 0);
    while (mcc->RunThroughDelayedTasks());

    // Verify that we didn't advance any further after the fling was aborted, in either case.
    ParentLayerPoint finalPointOut;
    apzc->SampleContentTransformForFrame(testStartTime + TimeDuration::FromMilliseconds(timeDelta + 1000), &viewTransformOut, finalPointOut);
    EXPECT_EQ(pointOut.x, finalPointOut.x);
    EXPECT_EQ(pointOut.y, finalPointOut.y);

    apzc->AssertStateIsReset();
  }

  void DoFlingStopWithSlowListener(bool aPreventDefault) {
    MakeApzcWaitForMainThread();

    int time = 0;
    int touchStart = 50;
    int touchEnd = 10;
    uint64_t blockId = 0;

    // Start the fling down.
    Pan(apzc, time, touchStart, touchEnd, false, nullptr, nullptr, &blockId);
    apzc->ContentReceivedInputBlock(blockId, false);
    while (mcc->RunThroughDelayedTasks());

    // Sample the fling a couple of times to ensure it's going.
    ParentLayerPoint point, finalPoint;
    ViewTransform viewTransform;
    apzc->SampleContentTransformForFrame(testStartTime + TimeDuration::FromMilliseconds(10), &viewTransform, point);
    apzc->SampleContentTransformForFrame(testStartTime + TimeDuration::FromMilliseconds(20), &viewTransform, finalPoint);
    EXPECT_GT(finalPoint.y, point.y);

    // Now we put our finger down to stop the fling
    TouchDown(apzc, 10, 10, time, &blockId);

    // Re-sample to make sure it hasn't moved
    apzc->SampleContentTransformForFrame(testStartTime + TimeDuration::FromMilliseconds(30), &viewTransform, point);
    EXPECT_EQ(finalPoint.x, point.x);
    EXPECT_EQ(finalPoint.y, point.y);

    // respond to the touchdown that stopped the fling.
    // even if we do a prevent-default on it, the animation should remain stopped.
    apzc->ContentReceivedInputBlock(blockId, aPreventDefault);
    while (mcc->RunThroughDelayedTasks());

    // Verify the page hasn't moved
    apzc->SampleContentTransformForFrame(testStartTime + TimeDuration::FromMilliseconds(100), &viewTransform, point);
    EXPECT_EQ(finalPoint.x, point.x);
    EXPECT_EQ(finalPoint.y, point.y);

    // clean up
    TouchUp(apzc, 10, 10, time);
    while (mcc->RunThroughDelayedTasks());

    apzc->AssertStateIsReset();
  }
};

TEST_F(APZCFlingStopTester, FlingStop) {
  DoFlingStopTest(false);
}

TEST_F(APZCFlingStopTester, FlingStopTap) {
  DoFlingStopTest(true);
}

TEST_F(APZCFlingStopTester, FlingStopSlowListener) {
  DoFlingStopWithSlowListener(false);
}

TEST_F(APZCFlingStopTester, FlingStopPreventDefault) {
  DoFlingStopWithSlowListener(true);
}

TEST_F(APZCGestureDetectorTester, ShortPress) {
  MakeApzcUnzoomable();

  int time = 0;
  TapAndCheckStatus(apzc, 10, 10, time, 100);
  // There will be delayed tasks posted for the long-tap and MAX_TAP timeouts, but
  // we want to clear those.
  mcc->ClearDelayedTask();
  mcc->ClearDelayedTask();

  // This verifies that the single tap notification is sent after the
  // touchdown is fully processed. The ordering here is important.
  mcc->CheckHasDelayedTask();

  EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(1);
  mcc->RunDelayedTask();

  apzc->AssertStateIsReset();
}

TEST_F(APZCGestureDetectorTester, MediumPress) {
  MakeApzcUnzoomable();

  int time = 0;
  TapAndCheckStatus(apzc, 10, 10, time, 400);
  // There will be delayed tasks posted for the long-tap and MAX_TAP timeouts, but
  // we want to clear those.
  mcc->ClearDelayedTask();
  mcc->ClearDelayedTask();

  // This verifies that the single tap notification is sent after the
  // touchdown is fully processed. The ordering here is important.
  mcc->CheckHasDelayedTask();

  EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(1);
  mcc->RunDelayedTask();

  apzc->AssertStateIsReset();
}

class APZCLongPressTester : public APZCGestureDetectorTester {
protected:
  void DoLongPressTest(uint32_t aBehavior) {
    MakeApzcUnzoomable();

    int time = 0;
    uint64_t blockId = 0;

    nsEventStatus status = TouchDown(apzc, 10, 10, time, &blockId);
    EXPECT_EQ(nsEventStatus_eConsumeDoDefault, status);

    if (gfxPrefs::TouchActionEnabled() && status != nsEventStatus_eConsumeNoDefault) {
      // SetAllowedTouchBehavior() must be called after sending touch-start.
      nsTArray<uint32_t> allowedTouchBehaviors;
      allowedTouchBehaviors.AppendElement(aBehavior);
      apzc->SetAllowedTouchBehavior(blockId, allowedTouchBehaviors);
    }
    // Have content "respond" to the touchstart
    apzc->ContentReceivedInputBlock(blockId, false);

    MockFunction<void(std::string checkPointName)> check;

    {
      InSequence s;

      EXPECT_CALL(check, Call("preHandleLongTap"));
      blockId++;
      EXPECT_CALL(*mcc, HandleLongTap(CSSPoint(10, 10), 0, apzc->GetGuid(), blockId)).Times(1);
      EXPECT_CALL(check, Call("postHandleLongTap"));

      EXPECT_CALL(check, Call("preHandleSingleTap"));
      EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(1);
      EXPECT_CALL(check, Call("postHandleSingleTap"));
    }

    // There is a longpress event scheduled on a timeout
    mcc->CheckHasDelayedTask();

    // Manually invoke the longpress while the touch is currently down.
    check.Call("preHandleLongTap");
    mcc->RunDelayedTask();
    check.Call("postHandleLongTap");

    // Destroy pending MAX_TAP timeout task
    mcc->DestroyOldestTask();

    // Dispatching the longpress event starts a new touch block, which
    // needs a new content response and also has a pending timeout task
    // in the queue. Deal with those here. We do the content response first
    // with preventDefault=false, and then we run the timeout task which
    // "loses the race" and does nothing.
    apzc->ContentReceivedInputBlock(blockId, false);
    mcc->CheckHasDelayedTask();
    mcc->RunDelayedTask();

    time += 1000;

    // Finally, simulate lifting the finger. Since the long-press wasn't
    // prevent-defaulted, we should get a long-tap-up event.
    check.Call("preHandleSingleTap");
    status = TouchUp(apzc, 10, 10, time);
    mcc->RunDelayedTask();
    EXPECT_EQ(nsEventStatus_eConsumeDoDefault, status);
    check.Call("postHandleSingleTap");

    apzc->AssertStateIsReset();
  }

  void DoLongPressPreventDefaultTest(uint32_t aBehavior) {
    MakeApzcUnzoomable();

    EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_,_)).Times(0);
    EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(0);

    int touchX = 10,
        touchStartY = 10,
        touchEndY = 50;

    int time = 0;
    uint64_t blockId = 0;
    nsEventStatus status = TouchDown(apzc, touchX, touchStartY, time, &blockId);
    EXPECT_EQ(nsEventStatus_eConsumeDoDefault, status);

    if (gfxPrefs::TouchActionEnabled() && status != nsEventStatus_eConsumeNoDefault) {
      // SetAllowedTouchBehavior() must be called after sending touch-start.
      nsTArray<uint32_t> allowedTouchBehaviors;
      allowedTouchBehaviors.AppendElement(aBehavior);
      apzc->SetAllowedTouchBehavior(blockId, allowedTouchBehaviors);
    }
    // Have content "respond" to the touchstart
    apzc->ContentReceivedInputBlock(blockId, false);

    MockFunction<void(std::string checkPointName)> check;

    {
      InSequence s;

      EXPECT_CALL(check, Call("preHandleLongTap"));
      blockId++;
      EXPECT_CALL(*mcc, HandleLongTap(CSSPoint(touchX, touchStartY), 0, apzc->GetGuid(), blockId)).Times(1);
      EXPECT_CALL(check, Call("postHandleLongTap"));
    }

    mcc->CheckHasDelayedTask();

    // Manually invoke the longpress while the touch is currently down.
    check.Call("preHandleLongTap");
    mcc->RunDelayedTask();
    check.Call("postHandleLongTap");

    // Destroy pending MAX_TAP timeout task
    mcc->DestroyOldestTask();

    // There should be a TimeoutContentResponse task in the queue still,
    // waiting for the response from the longtap event dispatched above.
    // Send the signal that content has handled the long-tap, and then run
    // the timeout task (it will be a no-op because the content "wins" the
    // race. This takes the place of the "contextmenu" event.
    apzc->ContentReceivedInputBlock(blockId, true);
    mcc->CheckHasDelayedTask();
    mcc->RunDelayedTask();

    time += 1000;

    MultiTouchInput mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, time, TimeStamp(), 0);
    mti.mTouches.AppendElement(SingleTouchData(0, ParentLayerPoint(touchX, touchEndY), ScreenSize(0, 0), 0, 0));
    status = apzc->ReceiveInputEvent(mti, nullptr);
    EXPECT_EQ(nsEventStatus_eConsumeDoDefault, status);

    EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(touchX, touchEndY), 0, apzc->GetGuid())).Times(0);
    status = TouchUp(apzc, touchX, touchEndY, time);
    EXPECT_EQ(nsEventStatus_eConsumeDoDefault, status);

    ParentLayerPoint pointOut;
    ViewTransform viewTransformOut;
    apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);

    EXPECT_EQ(ParentLayerPoint(), pointOut);
    EXPECT_EQ(ViewTransform(), viewTransformOut);

    apzc->AssertStateIsReset();
  }
};

TEST_F(APZCLongPressTester, LongPress) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, false);
  DoLongPressTest(mozilla::layers::AllowedTouchBehavior::NONE);
}

TEST_F(APZCLongPressTester, LongPressWithTouchAction) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, true);
  DoLongPressTest(mozilla::layers::AllowedTouchBehavior::HORIZONTAL_PAN
                  | mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN
                  | mozilla::layers::AllowedTouchBehavior::PINCH_ZOOM);
}

TEST_F(APZCLongPressTester, LongPressPreventDefault) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, false);
  DoLongPressPreventDefaultTest(mozilla::layers::AllowedTouchBehavior::NONE);
}

TEST_F(APZCLongPressTester, LongPressPreventDefaultWithTouchAction) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, true);
  DoLongPressPreventDefaultTest(mozilla::layers::AllowedTouchBehavior::HORIZONTAL_PAN
                                | mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN
                                | mozilla::layers::AllowedTouchBehavior::PINCH_ZOOM);
}

template<class InputReceiver> static void
DoubleTap(const nsRefPtr<InputReceiver>& aTarget, int aX, int aY, int& aTime,
          nsEventStatus (*aOutEventStatuses)[4] = nullptr,
          uint64_t (*aOutInputBlockIds)[2] = nullptr)
{
  uint64_t blockId;
  nsEventStatus status = TouchDown(aTarget, aX, aY, aTime, &blockId);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[0] = status;
  }
  if (aOutInputBlockIds) {
    (*aOutInputBlockIds)[0] = blockId;
  }
  aTime += 10;

  // If touch-action is enabled then simulate the allowed touch behaviour
  // notification that the main thread is supposed to deliver.
  if (gfxPrefs::TouchActionEnabled() && status != nsEventStatus_eConsumeNoDefault) {
    SetDefaultAllowedTouchBehavior(aTarget, blockId);
  }

  status = TouchUp(aTarget, aX, aY, aTime);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[1] = status;
  }
  aTime += 10;
  status = TouchDown(aTarget, aX, aY, aTime, &blockId);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[2] = status;
  }
  if (aOutInputBlockIds) {
    (*aOutInputBlockIds)[1] = blockId;
  }
  aTime += 10;

  if (gfxPrefs::TouchActionEnabled() && status != nsEventStatus_eConsumeNoDefault) {
    SetDefaultAllowedTouchBehavior(aTarget, blockId);
  }

  status = TouchUp(aTarget, aX, aY, aTime);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[3] = status;
  }
}

template<class InputReceiver> static void
DoubleTapAndCheckStatus(const nsRefPtr<InputReceiver>& aTarget, int aX, int aY, int& aTime, uint64_t (*aOutInputBlockIds)[2] = nullptr)
{
  nsEventStatus statuses[4];
  DoubleTap(aTarget, aX, aY, aTime, &statuses, aOutInputBlockIds);
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, statuses[0]);
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, statuses[1]);
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, statuses[2]);
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, statuses[3]);
}

TEST_F(APZCGestureDetectorTester, DoubleTap) {
  MakeApzcWaitForMainThread();
  MakeApzcZoomable();

  EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(0);
  EXPECT_CALL(*mcc, HandleDoubleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(1);

  int time = 0;
  uint64_t blockIds[2];
  DoubleTapAndCheckStatus(apzc, 10, 10, time, &blockIds);

  // responses to the two touchstarts
  apzc->ContentReceivedInputBlock(blockIds[0], false);
  apzc->ContentReceivedInputBlock(blockIds[1], false);

  while (mcc->RunThroughDelayedTasks());

  apzc->AssertStateIsReset();
}

TEST_F(APZCGestureDetectorTester, DoubleTapNotZoomable) {
  MakeApzcWaitForMainThread();
  MakeApzcUnzoomable();

  EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(2);
  EXPECT_CALL(*mcc, HandleDoubleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(0);

  int time = 0;
  uint64_t blockIds[2];
  DoubleTapAndCheckStatus(apzc, 10, 10, time, &blockIds);

  // responses to the two touchstarts
  apzc->ContentReceivedInputBlock(blockIds[0], false);
  apzc->ContentReceivedInputBlock(blockIds[1], false);

  while (mcc->RunThroughDelayedTasks());

  apzc->AssertStateIsReset();
}

TEST_F(APZCGestureDetectorTester, DoubleTapPreventDefaultFirstOnly) {
  MakeApzcWaitForMainThread();
  MakeApzcZoomable();

  EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(1);
  EXPECT_CALL(*mcc, HandleDoubleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(0);

  int time = 0;
  uint64_t blockIds[2];
  DoubleTapAndCheckStatus(apzc, 10, 10, time, &blockIds);

  // responses to the two touchstarts
  apzc->ContentReceivedInputBlock(blockIds[0], true);
  apzc->ContentReceivedInputBlock(blockIds[1], false);

  while (mcc->RunThroughDelayedTasks());

  apzc->AssertStateIsReset();
}

TEST_F(APZCGestureDetectorTester, DoubleTapPreventDefaultBoth) {
  MakeApzcWaitForMainThread();
  MakeApzcZoomable();

  EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(0);
  EXPECT_CALL(*mcc, HandleDoubleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(0);

  int time = 0;
  uint64_t blockIds[2];
  DoubleTapAndCheckStatus(apzc, 10, 10, time, &blockIds);

  // responses to the two touchstarts
  apzc->ContentReceivedInputBlock(blockIds[0], true);
  apzc->ContentReceivedInputBlock(blockIds[1], true);

  while (mcc->RunThroughDelayedTasks());

  apzc->AssertStateIsReset();
}

// Test for bug 947892
// We test whether we dispatch tap event when the tap is followed by pinch.
TEST_F(APZCGestureDetectorTester, TapFollowedByPinch) {
  MakeApzcZoomable();

  EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(1);

  int time = 0;
  Tap(apzc, 10, 10, time, 100);

  int inputId = 0;
  MultiTouchInput mti;
  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_START, time, TimeStamp(), 0);
  mti.mTouches.AppendElement(SingleTouchData(inputId, ParentLayerPoint(20, 20), ScreenSize(0, 0), 0, 0));
  mti.mTouches.AppendElement(SingleTouchData(inputId + 1, ParentLayerPoint(10, 10), ScreenSize(0, 0), 0, 0));
  apzc->ReceiveInputEvent(mti, nullptr);

  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_END, time, TimeStamp(), 0);
  mti.mTouches.AppendElement(SingleTouchData(inputId, ParentLayerPoint(20, 20), ScreenSize(0, 0), 0, 0));
  mti.mTouches.AppendElement(SingleTouchData(inputId + 1, ParentLayerPoint(10, 10), ScreenSize(0, 0), 0, 0));
  apzc->ReceiveInputEvent(mti, nullptr);

  while (mcc->RunThroughDelayedTasks());

  apzc->AssertStateIsReset();
}

TEST_F(APZCGestureDetectorTester, TapFollowedByMultipleTouches) {
  MakeApzcZoomable();

  EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(1);

  int time = 0;
  Tap(apzc, 10, 10, time, 100);

  int inputId = 0;
  MultiTouchInput mti;
  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_START, time, TimeStamp(), 0);
  mti.mTouches.AppendElement(SingleTouchData(inputId, ParentLayerPoint(20, 20), ScreenSize(0, 0), 0, 0));
  apzc->ReceiveInputEvent(mti, nullptr);

  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_START, time, TimeStamp(), 0);
  mti.mTouches.AppendElement(SingleTouchData(inputId, ParentLayerPoint(20, 20), ScreenSize(0, 0), 0, 0));
  mti.mTouches.AppendElement(SingleTouchData(inputId + 1, ParentLayerPoint(10, 10), ScreenSize(0, 0), 0, 0));
  apzc->ReceiveInputEvent(mti, nullptr);

  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_END, time, TimeStamp(), 0);
  mti.mTouches.AppendElement(SingleTouchData(inputId, ParentLayerPoint(20, 20), ScreenSize(0, 0), 0, 0));
  mti.mTouches.AppendElement(SingleTouchData(inputId + 1, ParentLayerPoint(10, 10), ScreenSize(0, 0), 0, 0));
  apzc->ReceiveInputEvent(mti, nullptr);

  while (mcc->RunThroughDelayedTasks());

  apzc->AssertStateIsReset();
}

class APZCTreeManagerTester : public ::testing::Test {
protected:
  virtual void SetUp() {
    gfxPrefs::GetSingleton();
    APZThreadUtils::SetThreadAssertionsEnabled(false);
    APZThreadUtils::SetControllerThread(MessageLoop::current());

    testStartTime = TimeStamp::Now();
    AsyncPanZoomController::SetFrameTime(testStartTime);

    mcc = new NiceMock<MockContentControllerDelayed>();
    manager = new TestAPZCTreeManager();
  }

  virtual void TearDown() {
    manager->ClearTree();
  }

  TimeStamp testStartTime;
  nsRefPtr<MockContentControllerDelayed> mcc;

  nsTArray<nsRefPtr<Layer> > layers;
  nsRefPtr<LayerManager> lm;
  nsRefPtr<Layer> root;

  nsRefPtr<TestAPZCTreeManager> manager;

protected:
  static void SetScrollableFrameMetrics(Layer* aLayer, FrameMetrics::ViewID aScrollId,
                                        CSSRect aScrollableRect = CSSRect(-1, -1, -1, -1)) {
    FrameMetrics metrics;
    metrics.SetScrollId(aScrollId);
    nsIntRect layerBound = aLayer->GetVisibleRegion().GetBounds();
    metrics.mCompositionBounds = ParentLayerRect(layerBound.x, layerBound.y,
                                                 layerBound.width, layerBound.height);
    metrics.SetScrollableRect(aScrollableRect);
    metrics.SetScrollOffset(CSSPoint(0, 0));
    aLayer->SetFrameMetrics(metrics);
    aLayer->SetClipRect(Some(ViewAs<ParentLayerPixel>(layerBound)));
    if (!aScrollableRect.IsEqualEdges(CSSRect(-1, -1, -1, -1))) {
      // The purpose of this is to roughly mimic what layout would do in the
      // case of a scrollable frame with the event regions and clip. This lets
      // us exercise the hit-testing code in APZCTreeManager
      EventRegions er = aLayer->GetEventRegions();
      nsIntRect scrollRect = LayerIntRect::ToUntyped(RoundedToInt(aScrollableRect * metrics.LayersPixelsPerCSSPixel()));
      er.mHitRegion = nsIntRegion(nsIntRect(layerBound.TopLeft(), scrollRect.Size()));
      aLayer->SetEventRegions(er);
    }
  }

  void SetScrollHandoff(Layer* aChild, Layer* aParent) {
    FrameMetrics metrics = aChild->GetFrameMetrics(0);
    metrics.SetScrollParentId(aParent->GetFrameMetrics(0).GetScrollId());
    aChild->SetFrameMetrics(metrics);
  }

  static TestAsyncPanZoomController* ApzcOf(Layer* aLayer) {
    EXPECT_EQ(1u, aLayer->GetFrameMetricsCount());
    return (TestAsyncPanZoomController*)aLayer->GetAsyncPanZoomController(0);
  }

  void CreateSimpleScrollingLayer() {
    const char* layerTreeSyntax = "t";
    nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(nsIntRect(0,0,200,200)),
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);
    SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID, CSSRect(0, 0, 500, 500));
  }

  void CreateSimpleMultiLayerTree() {
    const char* layerTreeSyntax = "c(tt)";
    // LayerID                     0 12
    nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(nsIntRect(0,0,100,100)),
      nsIntRegion(nsIntRect(0,0,100,50)),
      nsIntRegion(nsIntRect(0,50,100,50)),
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);
  }

  void CreatePotentiallyLeakingTree() {
    const char* layerTreeSyntax = "c(c(c(t))c(c(t)))";
    // LayerID                     0 1 2 3  4 5 6
    root = CreateLayerTree(layerTreeSyntax, nullptr, nullptr, lm, layers);
    SetScrollableFrameMetrics(layers[0], FrameMetrics::START_SCROLL_ID);
    SetScrollableFrameMetrics(layers[2], FrameMetrics::START_SCROLL_ID + 1);
    SetScrollableFrameMetrics(layers[5], FrameMetrics::START_SCROLL_ID + 1);
    SetScrollableFrameMetrics(layers[3], FrameMetrics::START_SCROLL_ID + 2);
    SetScrollableFrameMetrics(layers[6], FrameMetrics::START_SCROLL_ID + 3);
  }
};

class APZHitTestingTester : public APZCTreeManagerTester {
protected:
  Matrix4x4 transformToApzc;
  Matrix4x4 transformToGecko;

  already_AddRefed<AsyncPanZoomController> GetTargetAPZC(const ScreenPoint& aPoint) {
    nsRefPtr<AsyncPanZoomController> hit = manager->GetTargetAPZC(aPoint, nullptr);
    if (hit) {
      transformToApzc = manager->GetScreenToApzcTransform(hit.get());
      transformToGecko = manager->GetApzcToGeckoTransform(hit.get());
    }
    return hit.forget();
  }

protected:
  void CreateHitTesting1LayerTree() {
    const char* layerTreeSyntax = "c(tttt)";
    // LayerID                     0 1234
    nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(nsIntRect(0,0,100,100)),
      nsIntRegion(nsIntRect(0,0,100,100)),
      nsIntRegion(nsIntRect(10,10,20,20)),
      nsIntRegion(nsIntRect(10,10,20,20)),
      nsIntRegion(nsIntRect(5,5,20,20)),
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);
  }

  void CreateHitTesting2LayerTree() {
    const char* layerTreeSyntax = "c(tc(t))";
    // LayerID                     0 12 3
    nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(nsIntRect(0,0,100,100)),
      nsIntRegion(nsIntRect(10,10,40,40)),
      nsIntRegion(nsIntRect(10,60,40,40)),
      nsIntRegion(nsIntRect(10,60,40,40)),
    };
    Matrix4x4 transforms[] = {
      Matrix4x4(),
      Matrix4x4(),
      Matrix4x4::Scaling(2, 1, 1),
      Matrix4x4(),
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, transforms, lm, layers);

    SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID, CSSRect(0, 0, 200, 200));
    SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID + 1, CSSRect(0, 0, 80, 80));
    SetScrollableFrameMetrics(layers[3], FrameMetrics::START_SCROLL_ID + 2, CSSRect(0, 0, 80, 80));
  }

  void CreateComplexMultiLayerTree() {
    const char* layerTreeSyntax = "c(tc(t)tc(c(t)tt))";
    // LayerID                     0 12 3 45 6 7 89
    nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(nsIntRect(0,0,300,400)),      // root(0)
      nsIntRegion(nsIntRect(0,0,100,100)),      // thebes(1) in top-left
      nsIntRegion(nsIntRect(50,50,200,300)),    // container(2) centered in root(0)
      nsIntRegion(nsIntRect(50,50,200,300)),    // thebes(3) fully occupying parent container(2)
      nsIntRegion(nsIntRect(0,200,100,100)),    // thebes(4) in bottom-left
      nsIntRegion(nsIntRect(200,0,100,400)),    // container(5) along the right 100px of root(0)
      nsIntRegion(nsIntRect(200,0,100,200)),    // container(6) taking up the top half of parent container(5)
      nsIntRegion(nsIntRect(200,0,100,200)),    // thebes(7) fully occupying parent container(6)
      nsIntRegion(nsIntRect(200,200,100,100)),  // thebes(8) in bottom-right (below (6))
      nsIntRegion(nsIntRect(200,300,100,100)),  // thebes(9) in bottom-right (below (8))
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);
    SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID);
    SetScrollableFrameMetrics(layers[2], FrameMetrics::START_SCROLL_ID);
    SetScrollableFrameMetrics(layers[4], FrameMetrics::START_SCROLL_ID + 1);
    SetScrollableFrameMetrics(layers[6], FrameMetrics::START_SCROLL_ID + 1);
    SetScrollableFrameMetrics(layers[7], FrameMetrics::START_SCROLL_ID + 2);
    SetScrollableFrameMetrics(layers[8], FrameMetrics::START_SCROLL_ID + 1);
    SetScrollableFrameMetrics(layers[9], FrameMetrics::START_SCROLL_ID + 3);
  }
};

// A simple hit testing test that doesn't involve any transforms on layers.
TEST_F(APZHitTestingTester, HitTesting1) {
  CreateHitTesting1LayerTree();
  ScopedLayerTreeRegistration registration(0, root, mcc);

  // No APZC attached so hit testing will return no APZC at (20,20)
  nsRefPtr<AsyncPanZoomController> hit = GetTargetAPZC(ScreenPoint(20, 20));
  TestAsyncPanZoomController* nullAPZC = nullptr;
  EXPECT_EQ(nullAPZC, hit.get());
  EXPECT_EQ(Matrix4x4(), transformToApzc);
  EXPECT_EQ(Matrix4x4(), transformToGecko);

  uint32_t paintSequenceNumber = 0;

  // Now we have a root APZC that will match the page
  SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID);
  manager->UpdateHitTestingTree(nullptr, root, false, 0, paintSequenceNumber++);
  hit = GetTargetAPZC(ScreenPoint(15, 15));
  EXPECT_EQ(ApzcOf(root), hit.get());
  // expect hit point at LayerIntPoint(15, 15)
  EXPECT_EQ(Point(15, 15), transformToApzc * Point(15, 15));
  EXPECT_EQ(Point(15, 15), transformToGecko * Point(15, 15));

  // Now we have a sub APZC with a better fit
  SetScrollableFrameMetrics(layers[3], FrameMetrics::START_SCROLL_ID + 1);
  manager->UpdateHitTestingTree(nullptr, root, false, 0, paintSequenceNumber++);
  EXPECT_NE(ApzcOf(root), ApzcOf(layers[3]));
  hit = GetTargetAPZC(ScreenPoint(25, 25));
  EXPECT_EQ(ApzcOf(layers[3]), hit.get());
  // expect hit point at LayerIntPoint(25, 25)
  EXPECT_EQ(Point(25, 25), transformToApzc * Point(25, 25));
  EXPECT_EQ(Point(25, 25), transformToGecko * Point(25, 25));

  // At this point, layers[4] obscures layers[3] at the point (15, 15) so
  // hitting there should hit the root APZC
  hit = GetTargetAPZC(ScreenPoint(15, 15));
  EXPECT_EQ(ApzcOf(root), hit.get());

  // Now test hit testing when we have two scrollable layers
  SetScrollableFrameMetrics(layers[4], FrameMetrics::START_SCROLL_ID + 2);
  manager->UpdateHitTestingTree(nullptr, root, false, 0, paintSequenceNumber++);
  hit = GetTargetAPZC(ScreenPoint(15, 15));
  EXPECT_EQ(ApzcOf(layers[4]), hit.get());
  // expect hit point at LayerIntPoint(15, 15)
  EXPECT_EQ(Point(15, 15), transformToApzc * Point(15, 15));
  EXPECT_EQ(Point(15, 15), transformToGecko * Point(15, 15));

  // Hit test ouside the reach of layer[3,4] but inside root
  hit = GetTargetAPZC(ScreenPoint(90, 90));
  EXPECT_EQ(ApzcOf(root), hit.get());
  // expect hit point at LayerIntPoint(90, 90)
  EXPECT_EQ(Point(90, 90), transformToApzc * Point(90, 90));
  EXPECT_EQ(Point(90, 90), transformToGecko * Point(90, 90));

  // Hit test ouside the reach of any layer
  hit = GetTargetAPZC(ScreenPoint(1000, 10));
  EXPECT_EQ(nullAPZC, hit.get());
  EXPECT_EQ(Matrix4x4(), transformToApzc);
  EXPECT_EQ(Matrix4x4(), transformToGecko);
  hit = GetTargetAPZC(ScreenPoint(-1000, 10));
  EXPECT_EQ(nullAPZC, hit.get());
  EXPECT_EQ(Matrix4x4(), transformToApzc);
  EXPECT_EQ(Matrix4x4(), transformToGecko);
}

// A more involved hit testing test that involves css and async transforms.
TEST_F(APZHitTestingTester, HitTesting2) {
  CreateHitTesting2LayerTree();
  ScopedLayerTreeRegistration registration(0, root, mcc);

  manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);

  // At this point, the following holds (all coordinates in screen pixels):
  // layers[0] has content from (0,0)-(200,200), clipped by composition bounds (0,0)-(100,100)
  // layers[1] has content from (10,10)-(90,90), clipped by composition bounds (10,10)-(50,50)
  // layers[2] has content from (20,60)-(100,100). no clipping as it's not a scrollable layer
  // layers[3] has content from (20,60)-(180,140), clipped by composition bounds (20,60)-(100,100)

  TestAsyncPanZoomController* apzcroot = ApzcOf(root);
  TestAsyncPanZoomController* apzc1 = ApzcOf(layers[1]);
  TestAsyncPanZoomController* apzc3 = ApzcOf(layers[3]);

  // Hit an area that's clearly on the root layer but not any of the child layers.
  nsRefPtr<AsyncPanZoomController> hit = GetTargetAPZC(ScreenPoint(75, 25));
  EXPECT_EQ(apzcroot, hit.get());
  EXPECT_EQ(Point(75, 25), transformToApzc * Point(75, 25));
  EXPECT_EQ(Point(75, 25), transformToGecko * Point(75, 25));

  // Hit an area on the root that would be on layers[3] if layers[2]
  // weren't transformed.
  // Note that if layers[2] were scrollable, then this would hit layers[2]
  // because its composition bounds would be at (10,60)-(50,100) (and the
  // scale-only transform that we set on layers[2] would be invalid because
  // it would place the layer into overscroll, as its composition bounds
  // start at x=10 but its content at x=20).
  hit = GetTargetAPZC(ScreenPoint(15, 75));
  EXPECT_EQ(apzcroot, hit.get());
  EXPECT_EQ(Point(15, 75), transformToApzc * Point(15, 75));
  EXPECT_EQ(Point(15, 75), transformToGecko * Point(15, 75));

  // Hit an area on layers[1].
  hit = GetTargetAPZC(ScreenPoint(25, 25));
  EXPECT_EQ(apzc1, hit.get());
  EXPECT_EQ(Point(25, 25), transformToApzc * Point(25, 25));
  EXPECT_EQ(Point(25, 25), transformToGecko * Point(25, 25));

  // Hit an area on layers[3].
  hit = GetTargetAPZC(ScreenPoint(25, 75));
  EXPECT_EQ(apzc3, hit.get());
  // transformToApzc should unapply layers[2]'s transform
  EXPECT_EQ(Point(12.5, 75), transformToApzc * Point(25, 75));
  // and transformToGecko should reapply it
  EXPECT_EQ(Point(25, 75), transformToGecko * Point(12.5, 75));

  // Hit an area on layers[3] that would be on the root if layers[2]
  // weren't transformed.
  hit = GetTargetAPZC(ScreenPoint(75, 75));
  EXPECT_EQ(apzc3, hit.get());
  // transformToApzc should unapply layers[2]'s transform
  EXPECT_EQ(Point(37.5, 75), transformToApzc * Point(75, 75));
  // and transformToGecko should reapply it
  EXPECT_EQ(Point(75, 75), transformToGecko * Point(37.5, 75));

  // Pan the root layer upward by 50 pixels.
  // This causes layers[1] to scroll out of view, and an async transform
  // of -50 to be set on the root layer.
  int time = 0;
  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);

  // This first pan will move the APZC by 50 pixels, and dispatch a paint request.
  // Since this paint request is in the queue to Gecko, transformToGecko will
  // take it into account.
  ApzcPanNoFling(apzcroot, time, 100, 50);

  // Hit where layers[3] used to be. It should now hit the root.
  hit = GetTargetAPZC(ScreenPoint(75, 75));
  EXPECT_EQ(apzcroot, hit.get());
  // transformToApzc doesn't unapply the root's own async transform
  EXPECT_EQ(Point(75, 75), transformToApzc * Point(75, 75));
  // and transformToGecko unapplies it and then reapplies it, because by the
  // time the event being transformed reaches Gecko the new paint request will
  // have been handled.
  EXPECT_EQ(Point(75, 75), transformToGecko * Point(75, 75));

  // Hit where layers[1] used to be and where layers[3] should now be.
  hit = GetTargetAPZC(ScreenPoint(25, 25));
  EXPECT_EQ(apzc3, hit.get());
  // transformToApzc unapplies both layers[2]'s css transform and the root's
  // async transform
  EXPECT_EQ(Point(12.5, 75), transformToApzc * Point(25, 25));
  // transformToGecko reapplies both the css transform and the async transform
  // because we have already issued a paint request with it.
  EXPECT_EQ(Point(25, 25), transformToGecko * Point(12.5, 75));

  // This second pan will move the APZC by another 50 pixels but since the paint
  // request dispatched above has not "completed", we will not dispatch another
  // one yet. Now we have an async transform on top of the pending paint request
  // transform.
  ApzcPanNoFling(apzcroot, time, 100, 50);

  // Hit where layers[3] used to be. It should now hit the root.
  hit = GetTargetAPZC(ScreenPoint(75, 75));
  EXPECT_EQ(apzcroot, hit.get());
  // transformToApzc doesn't unapply the root's own async transform
  EXPECT_EQ(Point(75, 75), transformToApzc * Point(75, 75));
  // transformToGecko unapplies the full async transform of -100 pixels, and then
  // reapplies the "D" transform of -50 leading to an overall adjustment of +50
  EXPECT_EQ(Point(75, 125), transformToGecko * Point(75, 75));

  // Hit where layers[1] used to be. It should now hit the root.
  hit = GetTargetAPZC(ScreenPoint(25, 25));
  EXPECT_EQ(apzcroot, hit.get());
  // transformToApzc doesn't unapply the root's own async transform
  EXPECT_EQ(Point(25, 25), transformToApzc * Point(25, 25));
  // transformToGecko unapplies the full async transform of -100 pixels, and then
  // reapplies the "D" transform of -50 leading to an overall adjustment of +50
  EXPECT_EQ(Point(25, 75), transformToGecko * Point(25, 25));
}

TEST_F(APZCTreeManagerTester, ScrollablePaintedLayers) {
  CreateSimpleMultiLayerTree();
  ScopedLayerTreeRegistration registration(0, root, mcc);

  // both layers have the same scrollId
  SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID);
  SetScrollableFrameMetrics(layers[2], FrameMetrics::START_SCROLL_ID);
  manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);

  TestAsyncPanZoomController* nullAPZC = nullptr;
  // so they should have the same APZC
  EXPECT_FALSE(layers[0]->HasScrollableFrameMetrics());
  EXPECT_NE(nullAPZC, ApzcOf(layers[1]));
  EXPECT_NE(nullAPZC, ApzcOf(layers[2]));
  EXPECT_EQ(ApzcOf(layers[1]), ApzcOf(layers[2]));

  // Change the scrollId of layers[1], and verify the APZC changes
  SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID + 1);
  manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);
  EXPECT_NE(ApzcOf(layers[1]), ApzcOf(layers[2]));

  // Change the scrollId of layers[2] to match that of layers[1], ensure we get the same
  // APZC for both again
  SetScrollableFrameMetrics(layers[2], FrameMetrics::START_SCROLL_ID + 1);
  manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);
  EXPECT_EQ(ApzcOf(layers[1]), ApzcOf(layers[2]));
}

TEST_F(APZCTreeManagerTester, Bug1068268) {
  CreatePotentiallyLeakingTree();
  ScopedLayerTreeRegistration registration(0, root, mcc);

  manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);
  nsRefPtr<HitTestingTreeNode> root = manager->GetRootNode();
  nsRefPtr<HitTestingTreeNode> node2 = root->GetFirstChild()->GetFirstChild();
  nsRefPtr<HitTestingTreeNode> node5 = root->GetLastChild()->GetLastChild();

  EXPECT_EQ(ApzcOf(layers[2]), node5->GetApzc());
  EXPECT_EQ(ApzcOf(layers[2]), node2->GetApzc());
  EXPECT_EQ(ApzcOf(layers[0]), ApzcOf(layers[2])->GetParent());
  EXPECT_EQ(ApzcOf(layers[2]), ApzcOf(layers[5]));

  EXPECT_EQ(node2->GetFirstChild(), node2->GetLastChild());
  EXPECT_EQ(ApzcOf(layers[3]), node2->GetLastChild()->GetApzc());
  EXPECT_EQ(node5->GetFirstChild(), node5->GetLastChild());
  EXPECT_EQ(ApzcOf(layers[6]), node5->GetLastChild()->GetApzc());
  EXPECT_EQ(ApzcOf(layers[2]), ApzcOf(layers[3])->GetParent());
  EXPECT_EQ(ApzcOf(layers[5]), ApzcOf(layers[6])->GetParent());
}

TEST_F(APZHitTestingTester, ComplexMultiLayerTree) {
  CreateComplexMultiLayerTree();
  ScopedLayerTreeRegistration registration(0, root, mcc);
  manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);

  /* The layer tree looks like this:

                0
        |----|--+--|----|
        1    2     4    5
             |         /|\
             3        6 8 9
                      |
                      7

     Layers 1,2 have the same APZC
     Layers 4,6,8 have the same APZC
     Layer 7 has an APZC
     Layer 9 has an APZC
  */

  TestAsyncPanZoomController* nullAPZC = nullptr;
  // Ensure all the scrollable layers have an APZC
  EXPECT_FALSE(layers[0]->HasScrollableFrameMetrics());
  EXPECT_NE(nullAPZC, ApzcOf(layers[1]));
  EXPECT_NE(nullAPZC, ApzcOf(layers[2]));
  EXPECT_FALSE(layers[3]->HasScrollableFrameMetrics());
  EXPECT_NE(nullAPZC, ApzcOf(layers[4]));
  EXPECT_FALSE(layers[5]->HasScrollableFrameMetrics());
  EXPECT_NE(nullAPZC, ApzcOf(layers[6]));
  EXPECT_NE(nullAPZC, ApzcOf(layers[7]));
  EXPECT_NE(nullAPZC, ApzcOf(layers[8]));
  EXPECT_NE(nullAPZC, ApzcOf(layers[9]));
  // Ensure those that scroll together have the same APZCs
  EXPECT_EQ(ApzcOf(layers[1]), ApzcOf(layers[2]));
  EXPECT_EQ(ApzcOf(layers[4]), ApzcOf(layers[6]));
  EXPECT_EQ(ApzcOf(layers[8]), ApzcOf(layers[6]));
  // Ensure those that don't scroll together have different APZCs
  EXPECT_NE(ApzcOf(layers[1]), ApzcOf(layers[4]));
  EXPECT_NE(ApzcOf(layers[1]), ApzcOf(layers[7]));
  EXPECT_NE(ApzcOf(layers[1]), ApzcOf(layers[9]));
  EXPECT_NE(ApzcOf(layers[4]), ApzcOf(layers[7]));
  EXPECT_NE(ApzcOf(layers[4]), ApzcOf(layers[9]));
  EXPECT_NE(ApzcOf(layers[7]), ApzcOf(layers[9]));
  // Ensure the APZC parent chains are set up correctly
  TestAsyncPanZoomController* layers1_2 = ApzcOf(layers[1]);
  TestAsyncPanZoomController* layers4_6_8 = ApzcOf(layers[4]);
  TestAsyncPanZoomController* layer7 = ApzcOf(layers[7]);
  TestAsyncPanZoomController* layer9 = ApzcOf(layers[9]);
  EXPECT_EQ(nullptr, layers1_2->GetParent());
  EXPECT_EQ(nullptr, layers4_6_8->GetParent());
  EXPECT_EQ(layers4_6_8, layer7->GetParent());
  EXPECT_EQ(nullptr, layer9->GetParent());
  // Ensure the hit-testing tree looks like the layer tree
  nsRefPtr<HitTestingTreeNode> root = manager->GetRootNode();
  nsRefPtr<HitTestingTreeNode> node5 = root->GetLastChild();
  nsRefPtr<HitTestingTreeNode> node4 = node5->GetPrevSibling();
  nsRefPtr<HitTestingTreeNode> node2 = node4->GetPrevSibling();
  nsRefPtr<HitTestingTreeNode> node1 = node2->GetPrevSibling();
  nsRefPtr<HitTestingTreeNode> node3 = node2->GetLastChild();
  nsRefPtr<HitTestingTreeNode> node9 = node5->GetLastChild();
  nsRefPtr<HitTestingTreeNode> node8 = node9->GetPrevSibling();
  nsRefPtr<HitTestingTreeNode> node6 = node8->GetPrevSibling();
  nsRefPtr<HitTestingTreeNode> node7 = node6->GetLastChild();
  EXPECT_EQ(nullptr, node1->GetPrevSibling());
  EXPECT_EQ(nullptr, node3->GetPrevSibling());
  EXPECT_EQ(nullptr, node6->GetPrevSibling());
  EXPECT_EQ(nullptr, node7->GetPrevSibling());
  EXPECT_EQ(nullptr, node1->GetLastChild());
  EXPECT_EQ(nullptr, node3->GetLastChild());
  EXPECT_EQ(nullptr, node4->GetLastChild());
  EXPECT_EQ(nullptr, node7->GetLastChild());
  EXPECT_EQ(nullptr, node8->GetLastChild());
  EXPECT_EQ(nullptr, node9->GetLastChild());

  nsRefPtr<AsyncPanZoomController> hit = GetTargetAPZC(ScreenPoint(25, 25));
  EXPECT_EQ(ApzcOf(layers[1]), hit.get());
  hit = GetTargetAPZC(ScreenPoint(275, 375));
  EXPECT_EQ(ApzcOf(layers[9]), hit.get());
  hit = GetTargetAPZC(ScreenPoint(250, 100));
  EXPECT_EQ(ApzcOf(layers[7]), hit.get());
}

TEST_F(APZHitTestingTester, TestRepaintFlushOnNewInputBlock) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, false);

  // The main purpose of this test is to verify that touch-start events (or anything
  // that starts a new input block) don't ever get untransformed. This should always
  // hold because the APZ code should flush repaints when we start a new input block
  // and the transform to gecko space should be empty.

  CreateSimpleScrollingLayer();
  ScopedLayerTreeRegistration registration(0, root, mcc);
  manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);
  TestAsyncPanZoomController* apzcroot = ApzcOf(root);

  // At this point, the following holds (all coordinates in screen pixels):
  // layers[0] has content from (0,0)-(500,500), clipped by composition bounds (0,0)-(200,200)

  MockFunction<void(std::string checkPointName)> check;

  {
    InSequence s;

    EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(AtLeast(1));
    EXPECT_CALL(check, Call("post-first-touch-start"));
    EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(AtLeast(1));
    EXPECT_CALL(check, Call("post-second-fling"));
    EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(AtLeast(1));
    EXPECT_CALL(check, Call("post-second-touch-start"));
  }

  int time = 0;
  // This first pan will move the APZC by 50 pixels, and dispatch a paint request.
  ApzcPanNoFling(apzcroot, time, 100, 50);

  // Verify that a touch start doesn't get untransformed
  ScreenIntPoint touchPoint(50, 50);
  MultiTouchInput mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_START, time, TimeStamp(), 0);
  mti.mTouches.AppendElement(SingleTouchData(0, touchPoint, ScreenSize(0, 0), 0, 0));

  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, manager->ReceiveInputEvent(mti, nullptr, nullptr));
  EXPECT_EQ(touchPoint, mti.mTouches[0].mScreenPoint);
  check.Call("post-first-touch-start");

  // Send a touchend to clear state
  mti.mType = MultiTouchInput::MULTITOUCH_END;
  manager->ReceiveInputEvent(mti, nullptr, nullptr);

  AsyncPanZoomController::SetFrameTime(testStartTime + TimeDuration::FromMilliseconds(1000));

  // Now do two pans. The first of these will dispatch a repaint request, as above.
  // The second will get stuck in the paint throttler because the first one doesn't
  // get marked as "completed", so this will result in a non-empty LD transform.
  // (Note that any outstanding repaint requests from the first half of this test
  // don't impact this half because we advance the time by 1 second, which will trigger
  // the max-wait-exceeded codepath in the paint throttler).
  ApzcPanNoFling(apzcroot, time, 100, 50);
  check.Call("post-second-fling");
  ApzcPanNoFling(apzcroot, time, 100, 50);

  // Ensure that a touch start again doesn't get untransformed by flushing
  // a repaint
  mti.mType = MultiTouchInput::MULTITOUCH_START;
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, manager->ReceiveInputEvent(mti, nullptr, nullptr));
  EXPECT_EQ(touchPoint, mti.mTouches[0].mScreenPoint);
  check.Call("post-second-touch-start");

  mti.mType = MultiTouchInput::MULTITOUCH_END;
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, manager->ReceiveInputEvent(mti, nullptr, nullptr));
  EXPECT_EQ(touchPoint, mti.mTouches[0].mScreenPoint);

  mcc->RunThroughDelayedTasks();
}

class APZOverscrollHandoffTester : public APZCTreeManagerTester {
protected:
  UniquePtr<ScopedLayerTreeRegistration> registration;
  TestAsyncPanZoomController* rootApzc;

  void CreateOverscrollHandoffLayerTree1() {
    const char* layerTreeSyntax = "c(t)";
    nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(nsIntRect(0, 0, 100, 100)),
      nsIntRegion(nsIntRect(0, 50, 100, 50))
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);
    SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID, CSSRect(0, 0, 200, 200));
    SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID + 1, CSSRect(0, 0, 100, 100));
    SetScrollHandoff(layers[1], root);
    registration = MakeUnique<ScopedLayerTreeRegistration>(0, root, mcc);
    manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);
    rootApzc = ApzcOf(root);
  }

  void CreateOverscrollHandoffLayerTree2() {
    const char* layerTreeSyntax = "c(c(t))";
    nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(nsIntRect(0, 0, 100, 100)),
      nsIntRegion(nsIntRect(0, 0, 100, 100)),
      nsIntRegion(nsIntRect(0, 50, 100, 50))
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);
    SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID, CSSRect(0, 0, 200, 200));
    SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID + 2, CSSRect(-100, -100, 200, 200));
    SetScrollableFrameMetrics(layers[2], FrameMetrics::START_SCROLL_ID + 1, CSSRect(0, 0, 100, 100));
    SetScrollHandoff(layers[1], root);
    SetScrollHandoff(layers[2], layers[1]);
    // No ScopedLayerTreeRegistration as that just needs to be done once per test
    // and this is the second layer tree for a particular test.
    MOZ_ASSERT(registration);
    manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);
    rootApzc = ApzcOf(root);
  }

  void CreateOverscrollHandoffLayerTree3() {
    const char* layerTreeSyntax = "c(c(t)c(t))";
    nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(nsIntRect(0, 0, 100, 100)),  // root
      nsIntRegion(nsIntRect(0, 0, 100, 50)),   // scrolling parent 1
      nsIntRegion(nsIntRect(0, 0, 100, 50)),   // scrolling child 1
      nsIntRegion(nsIntRect(0, 50, 100, 50)),  // scrolling parent 2
      nsIntRegion(nsIntRect(0, 50, 100, 50))   // scrolling child 2
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);
    SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID, CSSRect(0, 0, 100, 100));
    SetScrollableFrameMetrics(layers[2], FrameMetrics::START_SCROLL_ID + 1, CSSRect(0, 0, 100, 100));
    SetScrollableFrameMetrics(layers[3], FrameMetrics::START_SCROLL_ID + 2, CSSRect(0, 50, 100, 100));
    SetScrollableFrameMetrics(layers[4], FrameMetrics::START_SCROLL_ID + 3, CSSRect(0, 50, 100, 100));
    SetScrollHandoff(layers[2], layers[1]);
    SetScrollHandoff(layers[4], layers[3]);
    registration = MakeUnique<ScopedLayerTreeRegistration>(0, root, mcc);
    manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);
  }

  void CreateScrollgrabLayerTree() {
    const char* layerTreeSyntax = "c(t)";
    nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(nsIntRect(0, 0, 100, 100)),  // scroll-grabbing parent
      nsIntRegion(nsIntRect(0, 20, 100, 80))   // child
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);
    SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID, CSSRect(0, 0, 100, 120));
    SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID + 1, CSSRect(0, 0, 100, 200));
    SetScrollHandoff(layers[1], root);
    registration = MakeUnique<ScopedLayerTreeRegistration>(0, root, mcc);
    manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);
    rootApzc = ApzcOf(root);
    rootApzc->GetFrameMetrics().SetHasScrollgrab(true);
  }
};

// Here we test that if the processing of a touch block is deferred while we
// wait for content to send a prevent-default message, overscroll is still
// handed off correctly when the block is processed.
TEST_F(APZOverscrollHandoffTester, DeferredInputEventProcessing) {
  // Set up the APZC tree.
  CreateOverscrollHandoffLayerTree1();

  TestAsyncPanZoomController* childApzc = ApzcOf(layers[1]);

  // Enable touch-listeners so that we can separate the queueing of input
  // events from them being processed.
  childApzc->SetWaitForMainThread();

  // Queue input events for a pan.
  int time = 0;
  uint64_t blockId = 0;
  ApzcPanNoFling(childApzc, time, 90, 30, &blockId);

  // Allow the pan to be processed.
  childApzc->ContentReceivedInputBlock(blockId, false);
  childApzc->ConfirmTarget(blockId);

  // Make sure overscroll was handed off correctly.
  EXPECT_EQ(50, childApzc->GetFrameMetrics().GetScrollOffset().y);
  EXPECT_EQ(10, rootApzc->GetFrameMetrics().GetScrollOffset().y);
}

// Here we test that if the layer structure changes in between two input
// blocks being queued, and the first block is only processed after the second
// one has been queued, overscroll handoff for the first block follows
// the original layer structure while overscroll handoff for the second block
// follows the new layer structure.
TEST_F(APZOverscrollHandoffTester, LayerStructureChangesWhileEventsArePending) {
  // Set up an initial APZC tree.
  CreateOverscrollHandoffLayerTree1();

  TestAsyncPanZoomController* childApzc = ApzcOf(layers[1]);

  // Enable touch-listeners so that we can separate the queueing of input
  // events from them being processed.
  childApzc->SetWaitForMainThread();

  // Queue input events for a pan.
  int time = 0;
  uint64_t blockId = 0;
  ApzcPanNoFling(childApzc, time, 90, 30, &blockId);

  // Modify the APZC tree to insert a new APZC 'middle' into the handoff chain
  // between the child and the root.
  CreateOverscrollHandoffLayerTree2();
  nsRefPtr<Layer> middle = layers[1];
  childApzc->SetWaitForMainThread();
  TestAsyncPanZoomController* middleApzc = ApzcOf(middle);

  // Queue input events for another pan.
  uint64_t secondBlockId = 0;
  ApzcPanNoFling(childApzc, time, 30, 90, &secondBlockId);

  // Allow the first pan to be processed.
  childApzc->ContentReceivedInputBlock(blockId, false);
  childApzc->ConfirmTarget(blockId);

  // Make sure things have scrolled according to the handoff chain in
  // place at the time the touch-start of the first pan was queued.
  EXPECT_EQ(50, childApzc->GetFrameMetrics().GetScrollOffset().y);
  EXPECT_EQ(10, rootApzc->GetFrameMetrics().GetScrollOffset().y);
  EXPECT_EQ(0, middleApzc->GetFrameMetrics().GetScrollOffset().y);

  // Allow the second pan to be processed.
  childApzc->ContentReceivedInputBlock(secondBlockId, false);
  childApzc->ConfirmTarget(secondBlockId);

  // Make sure things have scrolled according to the handoff chain in
  // place at the time the touch-start of the second pan was queued.
  EXPECT_EQ(0, childApzc->GetFrameMetrics().GetScrollOffset().y);
  EXPECT_EQ(10, rootApzc->GetFrameMetrics().GetScrollOffset().y);
  EXPECT_EQ(-10, middleApzc->GetFrameMetrics().GetScrollOffset().y);
}

// Test that putting a second finger down on an APZC while a down-chain APZC
// is overscrolled doesn't result in being stuck in overscroll.
TEST_F(APZOverscrollHandoffTester, StuckInOverscroll_Bug1073250) {
  // Enable overscrolling.
  SCOPED_GFX_PREF(APZOverscrollEnabled, bool, true);

  CreateOverscrollHandoffLayerTree1();

  TestAsyncPanZoomController* child = ApzcOf(layers[1]);

  // Pan, causing the parent APZC to overscroll.
  int time = 0;
  Pan(manager, time, 10, 40, true /* keep finger down */);
  EXPECT_FALSE(child->IsOverscrolled());
  EXPECT_TRUE(rootApzc->IsOverscrolled());

  // Put a second finger down.
  MultiTouchInput secondFingerDown(MultiTouchInput::MULTITOUCH_START, 0, TimeStamp(), 0);
  // Use the same touch identifier for the first touch (0) as Pan(). (A bit hacky.)
  secondFingerDown.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(10, 40), ScreenSize(0, 0), 0, 0));
  secondFingerDown.mTouches.AppendElement(SingleTouchData(1, ScreenIntPoint(30, 20), ScreenSize(0, 0), 0, 0));
  manager->ReceiveInputEvent(secondFingerDown, nullptr, nullptr);

  // Release the fingers.
  MultiTouchInput fingersUp = secondFingerDown;
  fingersUp.mType = MultiTouchInput::MULTITOUCH_END;
  manager->ReceiveInputEvent(fingersUp, nullptr, nullptr);

  // Allow any animations to run their course.
  child->AdvanceAnimationsUntilEnd(testStartTime);
  rootApzc->AdvanceAnimationsUntilEnd(testStartTime);

  // Make sure nothing is overscrolled.
  EXPECT_FALSE(child->IsOverscrolled());
  EXPECT_FALSE(rootApzc->IsOverscrolled());
}

// Here we test that if two flings are happening simultaneously, overscroll
// is handed off correctly for each.
TEST_F(APZOverscrollHandoffTester, SimultaneousFlings) {
  // Set up an initial APZC tree.
  CreateOverscrollHandoffLayerTree3();

  nsRefPtr<TestAsyncPanZoomController> parent1 = ApzcOf(layers[1]);
  nsRefPtr<TestAsyncPanZoomController> child1 = ApzcOf(layers[2]);
  nsRefPtr<TestAsyncPanZoomController> parent2 = ApzcOf(layers[3]);
  nsRefPtr<TestAsyncPanZoomController> child2 = ApzcOf(layers[4]);

  // Pan on the lower child.
  int time = 0;
  Pan(child2, time, 45, 5);

  // Pan on the upper child.
  Pan(child1, time, 95, 55);

  // Check that child1 and child2 are in a FLING state.
  child1->AssertStateIsFling();
  child2->AssertStateIsFling();

  // Advance the animations on child1 and child2 until their end.
  TimeStamp timestamp = TimeStamp::Now();
  child1->AdvanceAnimationsUntilEnd(timestamp);
  child2->AdvanceAnimationsUntilEnd(timestamp);

  // Check that the flings have been handed off to the parents.
  child1->AssertStateIsReset();
  parent1->AssertStateIsFling();
  child2->AssertStateIsReset();
  parent2->AssertStateIsFling();
}

TEST_F(APZOverscrollHandoffTester, Scrollgrab) {
  // Set up the layer tree
  CreateScrollgrabLayerTree();

  nsRefPtr<TestAsyncPanZoomController> childApzc = ApzcOf(layers[1]);

  // Pan on the child, enough to fully scroll the scrollgrab parent (20 px)
  // and leave some more (another 15 px) for the child.
  int time = 0;
  Pan(childApzc, time, 80, 45);

  // Check that the parent and child have scrolled as much as we expect.
  EXPECT_EQ(20, rootApzc->GetFrameMetrics().GetScrollOffset().y);
  EXPECT_EQ(15, childApzc->GetFrameMetrics().GetScrollOffset().y);
}

TEST_F(APZOverscrollHandoffTester, ScrollgrabFling) {
  // Set up the layer tree
  CreateScrollgrabLayerTree();

  nsRefPtr<TestAsyncPanZoomController> childApzc = ApzcOf(layers[1]);

  // Pan on the child, not enough to fully scroll the scrollgrab parent.
  int time = 0;
  Pan(childApzc, time, 80, 70);

  // Check that it is the scrollgrab parent that's in a fling, not the child.
  rootApzc->AssertStateIsFling();
  childApzc->AssertStateIsReset();
}

class APZEventRegionsTester : public APZCTreeManagerTester {
protected:
  UniquePtr<ScopedLayerTreeRegistration> registration;
  TestAsyncPanZoomController* rootApzc;

  void CreateEventRegionsLayerTree1() {
    const char* layerTreeSyntax = "c(tt)";
    nsIntRegion layerVisibleRegions[] = {
      nsIntRegion(nsIntRect(0, 0, 200, 200)),     // root
      nsIntRegion(nsIntRect(0, 0, 100, 200)),     // left half
      nsIntRegion(nsIntRect(0, 100, 200, 100)),   // bottom half
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegions, nullptr, lm, layers);
    SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID);
    SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID + 1);
    SetScrollableFrameMetrics(layers[2], FrameMetrics::START_SCROLL_ID + 2);
    SetScrollHandoff(layers[1], root);
    SetScrollHandoff(layers[2], root);

    // Set up the event regions over a 200x200 area. The root layer has the
    // whole 200x200 as the hit region; layers[1] has the left half and
    // layers[2] has the bottom half. The bottom-left 100x100 area is also
    // in the d-t-c region for both layers[1] and layers[2] (but layers[2] is
    // on top so it gets the events by default if the main thread doesn't
    // respond).
    EventRegions regions(nsIntRegion(nsIntRect(0, 0, 200, 200)));
    root->SetEventRegions(regions);
    regions.mDispatchToContentHitRegion = nsIntRegion(nsIntRect(0, 100, 100, 100));
    regions.mHitRegion = nsIntRegion(nsIntRect(0, 0, 100, 200));
    layers[1]->SetEventRegions(regions);
    regions.mHitRegion = nsIntRegion(nsIntRect(0, 100, 200, 100));
    layers[2]->SetEventRegions(regions);

    registration = MakeUnique<ScopedLayerTreeRegistration>(0, root, mcc);
    manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);
    rootApzc = ApzcOf(root);
  }

  void CreateEventRegionsLayerTree2() {
    const char* layerTreeSyntax = "c(t)";
    nsIntRegion layerVisibleRegions[] = {
      nsIntRegion(nsIntRect(0, 0, 100, 500)),
      nsIntRegion(nsIntRect(0, 150, 100, 100)),
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegions, nullptr, lm, layers);
    SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID);

    // Set up the event regions so that the child thebes layer is positioned far
    // away from the scrolling container layer.
    EventRegions regions(nsIntRegion(nsIntRect(0, 0, 100, 100)));
    root->SetEventRegions(regions);
    regions.mHitRegion = nsIntRegion(nsIntRect(0, 150, 100, 100));
    layers[1]->SetEventRegions(regions);

    registration = MakeUnique<ScopedLayerTreeRegistration>(0, root, mcc);
    manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);
    rootApzc = ApzcOf(root);
  }

  void CreateObscuringLayerTree() {
    const char* layerTreeSyntax = "c(c(t)t)";
    // LayerID                     0 1 2 3
    // 0 is the root.
    // 1 is a parent scrollable layer.
    // 2 is a child scrollable layer.
    // 3 is the Obscurer, who ruins everything.
    nsIntRegion layerVisibleRegions[] = {
        // x coordinates are uninteresting
        nsIntRegion(nsIntRect(0,   0, 200, 200)),  // [0, 200]
        nsIntRegion(nsIntRect(0,   0, 200, 200)),  // [0, 200]
        nsIntRegion(nsIntRect(0, 100, 200,  50)),  // [100, 150]
        nsIntRegion(nsIntRect(0, 100, 200, 100))   // [100, 200]
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegions, nullptr, lm, layers);

    SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID, CSSRect(0, 0, 200, 200));
    SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID + 1, CSSRect(0, 0, 200, 300));
    SetScrollableFrameMetrics(layers[2], FrameMetrics::START_SCROLL_ID + 2, CSSRect(0, 0, 200, 100));
    SetScrollHandoff(layers[2], layers[1]);
    SetScrollHandoff(layers[1], root);

    EventRegions regions(nsIntRegion(nsIntRect(0, 0, 200, 200)));
    root->SetEventRegions(regions);
    regions.mHitRegion = nsIntRegion(nsIntRect(0, 0, 200, 300));
    layers[1]->SetEventRegions(regions);
    regions.mHitRegion = nsIntRegion(nsIntRect(0, 100, 200, 100));
    layers[2]->SetEventRegions(regions);

    registration = MakeUnique<ScopedLayerTreeRegistration>(0, root, mcc);
    manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);
    rootApzc = ApzcOf(root);
  }

  void CreateBug1119497LayerTree() {
    const char* layerTreeSyntax = "c(tt)";
    // LayerID                     0 12
    // 0 is the root and doesn't have an APZC
    // 1 is behind 2 and does have an APZC
    // 2 entirely covers 1 and should take all the input events
    nsIntRegion layerVisibleRegions[] = {
      nsIntRegion(nsIntRect(0, 0, 100, 100)),
      nsIntRegion(nsIntRect(0, 0, 100, 100)),
      nsIntRegion(nsIntRect(0, 0, 100, 100)),
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegions, nullptr, lm, layers);

    SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID + 1);

    registration = MakeUnique<ScopedLayerTreeRegistration>(0, root, mcc);
    manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);
  }

  void CreateBug1117712LayerTree() {
    const char* layerTreeSyntax = "c(c(t)t)";
    // LayerID                     0 1 2 3
    // 0 is the root
    // 1 is a container layer whose sole purpose to make a non-empty ancestor
    //   transform for 2, so that 2's screen-to-apzc and apzc-to-gecko
    //   transforms are different from 3's.
    // 2 is a small layer that is the actual target
    // 3 is a big layer obscuring 2 with a dispatch-to-content region
    nsIntRegion layerVisibleRegions[] = {
      nsIntRegion(nsIntRect(0, 0, 100, 100)),
      nsIntRegion(nsIntRect(0, 0, 0, 0)),
      nsIntRegion(nsIntRect(0, 0, 10, 10)),
      nsIntRegion(nsIntRect(0, 0, 100, 100)),
    };
    Matrix4x4 layerTransforms[] = {
      Matrix4x4(),
      Matrix4x4::Translation(50, 0, 0),
      Matrix4x4(),
      Matrix4x4(),
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegions, layerTransforms, lm, layers);

    SetScrollableFrameMetrics(layers[2], FrameMetrics::START_SCROLL_ID + 1, CSSRect(0, 0, 10, 10));
    SetScrollableFrameMetrics(layers[3], FrameMetrics::START_SCROLL_ID + 2, CSSRect(0, 0, 100, 100));

    EventRegions regions(nsIntRegion(nsIntRect(0, 0, 10, 10)));
    layers[2]->SetEventRegions(regions);
    regions.mHitRegion = nsIntRegion(nsIntRect(0, 0, 100, 100));
    regions.mDispatchToContentHitRegion = nsIntRegion(nsIntRect(0, 0, 100, 100));
    layers[3]->SetEventRegions(regions);

    registration = MakeUnique<ScopedLayerTreeRegistration>(0, root, mcc);
    manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);
  }
};

TEST_F(APZEventRegionsTester, HitRegionImmediateResponse) {
  CreateEventRegionsLayerTree1();

  TestAsyncPanZoomController* root = ApzcOf(layers[0]);
  TestAsyncPanZoomController* left = ApzcOf(layers[1]);
  TestAsyncPanZoomController* bottom = ApzcOf(layers[2]);

  MockFunction<void(std::string checkPointName)> check;
  {
    InSequence s;
    EXPECT_CALL(*mcc, HandleSingleTap(_, _, left->GetGuid())).Times(1);
    EXPECT_CALL(check, Call("Tapped on left"));
    EXPECT_CALL(*mcc, HandleSingleTap(_, _, bottom->GetGuid())).Times(1);
    EXPECT_CALL(check, Call("Tapped on bottom"));
    EXPECT_CALL(*mcc, HandleSingleTap(_, _, root->GetGuid())).Times(1);
    EXPECT_CALL(check, Call("Tapped on root"));
    EXPECT_CALL(check, Call("Tap pending on d-t-c region"));
    EXPECT_CALL(*mcc, HandleSingleTap(_, _, bottom->GetGuid())).Times(1);
    EXPECT_CALL(check, Call("Tapped on bottom again"));
    EXPECT_CALL(*mcc, HandleSingleTap(_, _, left->GetGuid())).Times(1);
    EXPECT_CALL(check, Call("Tapped on left this time"));
  }

  int time = 0;
  // Tap in the exposed hit regions of each of the layers once and ensure
  // the clicks are dispatched right away
  Tap(manager, 10, 10, time, 100);
  mcc->RunThroughDelayedTasks();    // this runs the tap event
  check.Call("Tapped on left");
  Tap(manager, 110, 110, time, 100);
  mcc->RunThroughDelayedTasks();    // this runs the tap event
  check.Call("Tapped on bottom");
  Tap(manager, 110, 10, time, 100);
  mcc->RunThroughDelayedTasks();    // this runs the tap event
  check.Call("Tapped on root");

  // Now tap on the dispatch-to-content region where the layers overlap
  Tap(manager, 10, 110, time, 100);
  mcc->RunThroughDelayedTasks();    // this runs the main-thread timeout
  check.Call("Tap pending on d-t-c region");
  mcc->RunThroughDelayedTasks();    // this runs the tap event
  check.Call("Tapped on bottom again");

  // Now let's do that again, but simulate a main-thread response
  uint64_t inputBlockId = 0;
  Tap(manager, 10, 110, time, 100, nullptr, &inputBlockId);
  nsTArray<ScrollableLayerGuid> targets;
  targets.AppendElement(left->GetGuid());
  manager->SetTargetAPZC(inputBlockId, targets);
  while (mcc->RunThroughDelayedTasks());    // this runs the tap event
  check.Call("Tapped on left this time");
}

TEST_F(APZEventRegionsTester, HitRegionAccumulatesChildren) {
  CreateEventRegionsLayerTree2();

  int time = 0;
  // Tap in the area of the child layer that's not directly included in the
  // parent layer's hit region. Verify that it comes out of the APZC's
  // content controller, which indicates the input events got routed correctly
  // to the APZC.
  EXPECT_CALL(*mcc, HandleSingleTap(_, _, rootApzc->GetGuid())).Times(1);
  Tap(manager, 10, 160, time, 100);
  mcc->RunThroughDelayedTasks();    // this runs the tap event
}

TEST_F(APZEventRegionsTester, Obscuration) {
  CreateObscuringLayerTree();
  ScopedLayerTreeRegistration registration(0, root, mcc);

  manager->UpdateHitTestingTree(nullptr, root, false, 0, 0);

  TestAsyncPanZoomController* parent = ApzcOf(layers[1]);
  TestAsyncPanZoomController* child = ApzcOf(layers[2]);

  int time = 0;
  ApzcPanNoFling(parent, time, 75, 25);

  HitTestResult result;
  nsRefPtr<AsyncPanZoomController> hit = manager->GetTargetAPZC(ScreenPoint(50, 75), &result);
  EXPECT_EQ(child, hit.get());
  EXPECT_EQ(HitTestResult::HitLayer, result);
}

TEST_F(APZEventRegionsTester, Bug1119497) {
  CreateBug1119497LayerTree();

  HitTestResult result;
  nsRefPtr<AsyncPanZoomController> hit = manager->GetTargetAPZC(ScreenPoint(50, 50), &result);
  // We should hit layers[2], so |result| will be HitLayer but there's no
  // actual APZC in that parent chain, so |hit| should be nullptr.
  EXPECT_EQ(nullptr, hit.get());
  EXPECT_EQ(HitTestResult::HitLayer, result);
}

TEST_F(APZEventRegionsTester, Bug1117712) {
  CreateBug1117712LayerTree();

  TestAsyncPanZoomController* apzc2 = ApzcOf(layers[2]);

  // These touch events should hit the dispatch-to-content region of layers[3]
  // and so get queued with that APZC as the tentative target.
  int time = 0;
  uint64_t inputBlockId = 0;
  Tap(manager, 55, 5, time, 100, nullptr, &inputBlockId);
  // But now we tell the APZ that really it hit layers[2], and expect the tap
  // to be delivered at the correct coordinates.
  EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(55, 5), 0, apzc2->GetGuid())).Times(1);

  nsTArray<ScrollableLayerGuid> targets;
  targets.AppendElement(apzc2->GetGuid());
  manager->SetTargetAPZC(inputBlockId, targets);
  while (mcc->RunThroughDelayedTasks());    // this runs the tap event
}

class TaskRunMetrics {
public:
  TaskRunMetrics()
    : mRunCount(0)
    , mCancelCount(0)
  {}

  void IncrementRunCount() {
    mRunCount++;
  }

  void IncrementCancelCount() {
    mCancelCount++;
  }

  int GetAndClearRunCount() {
    int runCount = mRunCount;
    mRunCount = 0;
    return runCount;
  }

  int GetAndClearCancelCount() {
    int cancelCount = mCancelCount;
    mCancelCount = 0;
    return cancelCount;
  }

private:
  int mRunCount;
  int mCancelCount;
};

class MockTask : public CancelableTask {
public:
  explicit MockTask(TaskRunMetrics& aMetrics)
    : mMetrics(aMetrics)
  {}

  virtual void Run() {
    mMetrics.IncrementRunCount();
  }

  virtual void Cancel() {
    mMetrics.IncrementCancelCount();
  }

private:
  TaskRunMetrics& mMetrics;
};

class APZTaskThrottlerTester : public ::testing::Test {
public:
  APZTaskThrottlerTester()
  {
    now = TimeStamp::Now();
    throttler = MakeUnique<TaskThrottler>(now, TimeDuration::FromMilliseconds(100));
  }

protected:
  TimeStamp Advance(int aMillis = 5)
  {
    now = now + TimeDuration::FromMilliseconds(aMillis);
    return now;
  }

  UniquePtr<CancelableTask> NewTask()
  {
    return MakeUnique<MockTask>(metrics);
  }

  TimeStamp now;
  UniquePtr<TaskThrottler> throttler;
  TaskRunMetrics metrics;
};

TEST_F(APZTaskThrottlerTester, BasicTest) {
  // Check that posting the first task runs right away
  throttler->PostTask(FROM_HERE, NewTask(), Advance());         // task 1
  EXPECT_EQ(1, metrics.GetAndClearRunCount());

  // Check that posting the second task doesn't run until the first one is done
  throttler->PostTask(FROM_HERE, NewTask(), Advance());         // task 2
  EXPECT_EQ(0, metrics.GetAndClearRunCount());
  throttler->TaskComplete(Advance());                           // for task 1
  EXPECT_EQ(1, metrics.GetAndClearRunCount());
  EXPECT_EQ(0, metrics.GetAndClearCancelCount());

  // Check that tasks are coalesced: dispatch 5 tasks
  // while there is still one outstanding, and ensure
  // that only one of the 5 runs
  throttler->PostTask(FROM_HERE, NewTask(), Advance());         // task 3
  throttler->PostTask(FROM_HERE, NewTask(), Advance());         // task 4
  throttler->PostTask(FROM_HERE, NewTask(), Advance());         // task 5
  throttler->PostTask(FROM_HERE, NewTask(), Advance());         // task 6
  throttler->PostTask(FROM_HERE, NewTask(), Advance());         // task 7
  EXPECT_EQ(0, metrics.GetAndClearRunCount());
  EXPECT_EQ(4, metrics.GetAndClearCancelCount());

  throttler->TaskComplete(Advance());                           // for task 2
  EXPECT_EQ(1, metrics.GetAndClearRunCount());
  throttler->TaskComplete(Advance());                           // for task 7 (tasks 3..6 were cancelled)
  EXPECT_EQ(0, metrics.GetAndClearRunCount());
  EXPECT_EQ(0, metrics.GetAndClearCancelCount());
}

TEST_F(APZTaskThrottlerTester, TimeoutTest) {
  // Check that posting the first task runs right away
  throttler->PostTask(FROM_HERE, NewTask(), Advance());         // task 1
  EXPECT_EQ(1, metrics.GetAndClearRunCount());

  // Because we let 100ms pass, the second task should
  // run immediately even though the first one isn't
  // done yet
  throttler->PostTask(FROM_HERE, NewTask(), Advance(100));      // task 2; task 1 is assumed lost
  EXPECT_EQ(1, metrics.GetAndClearRunCount());
  throttler->TaskComplete(Advance());                           // for task 1, but TaskThrottler thinks it's for task 2
  throttler->TaskComplete(Advance());                           // for task 2, TaskThrottler ignores it
  EXPECT_EQ(0, metrics.GetAndClearRunCount());
  EXPECT_EQ(0, metrics.GetAndClearCancelCount());

  // This time queue up a few tasks before the timeout expires
  // and ensure cancellation still works as expected
  throttler->PostTask(FROM_HERE, NewTask(), Advance());         // task 3
  EXPECT_EQ(1, metrics.GetAndClearRunCount());
  throttler->PostTask(FROM_HERE, NewTask(), Advance());         // task 4
  throttler->PostTask(FROM_HERE, NewTask(), Advance());         // task 5
  throttler->PostTask(FROM_HERE, NewTask(), Advance());         // task 6
  EXPECT_EQ(0, metrics.GetAndClearRunCount());
  throttler->PostTask(FROM_HERE, NewTask(), Advance(100));      // task 7; task 3 is assumed lost
  EXPECT_EQ(1, metrics.GetAndClearRunCount());
  EXPECT_EQ(3, metrics.GetAndClearCancelCount());               // tasks 4..6 should have been cancelled
  throttler->TaskComplete(Advance());                           // for task 7
  EXPECT_EQ(0, metrics.GetAndClearRunCount());
  EXPECT_EQ(0, metrics.GetAndClearCancelCount());
}
