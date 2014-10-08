/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mozilla/Attributes.h"
#include "mozilla/layers/AsyncCompositionManager.h" // for ViewTransform
#include "mozilla/layers/AsyncPanZoomController.h"
#include "mozilla/layers/GeckoContentController.h"
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/APZCTreeManager.h"
#include "mozilla/layers/LayerMetricsWrapper.h"
#include "mozilla/UniquePtr.h"
#include "base/task.h"
#include "Layers.h"
#include "TestLayers.h"
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
  MOCK_METHOD2(AcknowledgeScrollUpdate, void(const FrameMetrics::ViewID&, const uint32_t& aScrollGeneration));
  MOCK_METHOD3(HandleDoubleTap, void(const CSSPoint&, int32_t, const ScrollableLayerGuid&));
  MOCK_METHOD3(HandleSingleTap, void(const CSSPoint&, int32_t, const ScrollableLayerGuid&));
  MOCK_METHOD3(HandleLongTap, void(const CSSPoint&, int32_t, const ScrollableLayerGuid&));
  MOCK_METHOD3(HandleLongTapUp, void(const CSSPoint&, int32_t, const ScrollableLayerGuid&));
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

class TestAsyncPanZoomController : public AsyncPanZoomController {
public:
  TestAsyncPanZoomController(uint64_t aLayersId, MockContentController* aMcc,
                             APZCTreeManager* aTreeManager = nullptr,
                             GestureBehavior aBehavior = DEFAULT_GESTURES)
    : AsyncPanZoomController(aLayersId, aTreeManager, aMcc, aBehavior)
  {}

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
                                      ScreenPoint& aScrollOffset) {
    bool ret = AdvanceAnimations(aSampleTime);
    AsyncPanZoomController::SampleContentTransformForFrame(
      aOutTransform, aScrollOffset);
    return ret;
  }
};

class TestAPZCTreeManager : public APZCTreeManager {
};

static FrameMetrics
TestFrameMetrics()
{
  FrameMetrics fm;

  fm.mDisplayPort = CSSRect(0, 0, 10, 10);
  fm.mCompositionBounds = ParentLayerRect(0, 0, 10, 10);
  fm.mCriticalDisplayPort = CSSRect(0, 0, 10, 10);
  fm.mScrollableRect = CSSRect(0, 0, 100, 100);

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
    AsyncPanZoomController::SetThreadAssertionsEnabled(false);

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

  void SetMayHaveTouchListeners()
  {
    apzc->GetFrameMetrics().mMayHaveTouchListeners = true;
  }

  void MakeApzcZoomable()
  {
    apzc->UpdateZoomConstraints(ZoomConstraints(true, true, CSSToScreenScale(0.25f), CSSToScreenScale(4.0f)));
  }

  void MakeApzcUnzoomable()
  {
    apzc->UpdateZoomConstraints(ZoomConstraints(false, false, CSSToScreenScale(1.0f), CSSToScreenScale(1.0f)));
  }

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

static nsEventStatus
ApzcDown(AsyncPanZoomController* apzc, int aX, int aY, int aTime)
{
  MultiTouchInput mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_START, aTime, TimeStamp(), 0);
  mti.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(aX, aY), ScreenSize(0, 0), 0, 0));
  return apzc->ReceiveInputEvent(mti);
}

static nsEventStatus
ApzcUp(AsyncPanZoomController* apzc, int aX, int aY, int aTime)
{
  MultiTouchInput mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_END, aTime, TimeStamp(), 0);
  mti.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(aX, aY), ScreenSize(0, 0), 0, 0));
  return apzc->ReceiveInputEvent(mti);
}

static void
ApzcTap(AsyncPanZoomController* aApzc, int aX, int aY, int& aTime, int aTapLength,
        nsEventStatus (*aOutEventStatuses)[2] = nullptr)
{
  nsEventStatus status = ApzcDown(aApzc, aX, aY, aTime);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[0] = status;
  }
  aTime += aTapLength;
  status = ApzcUp(aApzc, aX, aY, aTime);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[1] = status;
  }
}

static void
ApzcTapAndCheckStatus(AsyncPanZoomController* aApzc, int aX, int aY, int& aTime, int aTapLength)
{
  nsEventStatus statuses[2];
  ApzcTap(aApzc, aX, aY, aTime, aTapLength, &statuses);
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, statuses[0]);
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, statuses[1]);
}

static void
ApzcPan(AsyncPanZoomController* aApzc,
        int& aTime,
        int aTouchStartY,
        int aTouchEndY,
        bool aKeepFingerDown = false,
        nsTArray<uint32_t>* aAllowedTouchBehaviors = nullptr,
        nsEventStatus (*aOutEventStatuses)[4] = nullptr)
{
  const int TIME_BETWEEN_TOUCH_EVENT = 100;
  const int OVERCOME_TOUCH_TOLERANCE = 100;

  // Make sure the move is large enough to not be handled as a tap
  nsEventStatus status = ApzcDown(aApzc, 10, aTouchStartY + OVERCOME_TOUCH_TOLERANCE, aTime);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[0] = status;
  }

  aTime += TIME_BETWEEN_TOUCH_EVENT;

  // Allowed touch behaviours must be set after sending touch-start.
  if (gfxPrefs::TouchActionEnabled() && aAllowedTouchBehaviors) {
    aApzc->SetAllowedTouchBehavior(*aAllowedTouchBehaviors);
  }

  MultiTouchInput mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, aTime, TimeStamp(), 0);
  mti.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(10, aTouchStartY), ScreenSize(0, 0), 0, 0));
  status = aApzc->ReceiveInputEvent(mti);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[1] = status;
  }

  aTime += TIME_BETWEEN_TOUCH_EVENT;

  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, aTime, TimeStamp(), 0);
  mti.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(10, aTouchEndY), ScreenSize(0, 0), 0, 0));
  status = aApzc->ReceiveInputEvent(mti);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[2] = status;
  }

  aTime += TIME_BETWEEN_TOUCH_EVENT;

  if (!aKeepFingerDown) {
    status = ApzcUp(aApzc, 10, aTouchEndY, aTime);
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
static void
ApzcPanAndCheckStatus(AsyncPanZoomController* aApzc,
                      int& aTime,
                      int aTouchStartY,
                      int aTouchEndY,
                      bool aExpectConsumed,
                      nsTArray<uint32_t>* aAllowedTouchBehaviors)
{
  nsEventStatus statuses[4]; // down, move, move, up
  ApzcPan(aApzc, aTime, aTouchStartY, aTouchEndY, false, aAllowedTouchBehaviors, &statuses);

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
ApzcPanNoFling(AsyncPanZoomController* aApzc,
               int& aTime,
               int aTouchStartY,
               int aTouchEndY)
{
  ApzcPan(aApzc, aTime, aTouchStartY, aTouchEndY);
  aApzc->CancelAnimation();
}

static void
ApzcPinchWithPinchInput(AsyncPanZoomController* aApzc,
                        int aFocusX, int aFocusY, float aScale,
                        nsEventStatus (*aOutEventStatuses)[3] = nullptr)
{
  nsEventStatus actualStatus = aApzc->HandleGestureEvent(
    PinchGestureInput(PinchGestureInput::PINCHGESTURE_START,
                      0, TimeStamp(), ScreenPoint(aFocusX, aFocusY),
                      10.0, 10.0, 0));
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[0] = actualStatus;
  }
  actualStatus = aApzc->HandleGestureEvent(
    PinchGestureInput(PinchGestureInput::PINCHGESTURE_SCALE,
                      0, TimeStamp(), ScreenPoint(aFocusX, aFocusY),
                      10.0 * aScale, 10.0, 0));
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[1] = actualStatus;
  }
  actualStatus = aApzc->HandleGestureEvent(
    PinchGestureInput(PinchGestureInput::PINCHGESTURE_END,
                      0, TimeStamp(), ScreenPoint(aFocusX, aFocusY),
                      // note: negative values here tell APZC
                      //       not to turn the pinch into a pan
                      -1.0, -1.0, 0));
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[2] = actualStatus;
  }
}

static void
ApzcPinchWithPinchInputAndCheckStatus(AsyncPanZoomController* aApzc,
                                      int aFocusX, int aFocusY, float aScale,
                                      bool aShouldTriggerPinch)
{
  nsEventStatus statuses[3];  // scalebegin, scale, scaleend
  ApzcPinchWithPinchInput(aApzc, aFocusX, aFocusY, aScale, &statuses);

  nsEventStatus expectedStatus = aShouldTriggerPinch
      ? nsEventStatus_eConsumeNoDefault
      : nsEventStatus_eIgnore;
  EXPECT_EQ(expectedStatus, statuses[0]);
  EXPECT_EQ(expectedStatus, statuses[1]);
}

static void
ApzcPinchWithTouchInput(AsyncPanZoomController* aApzc,
                        int aFocusX, int aFocusY, float aScale,
                        int& inputId,
                        nsTArray<uint32_t>* aAllowedTouchBehaviors = nullptr,
                        nsEventStatus (*aOutEventStatuses)[4] = nullptr)
{
  // Having pinch coordinates in float type may cause problems with high-precision scale values
  // since SingleTouchData accepts integer value. But for trivial tests it should be ok.
  float pinchLength = 100.0;
  float pinchLengthScaled = pinchLength * aScale;

  MultiTouchInput mtiStart = MultiTouchInput(MultiTouchInput::MULTITOUCH_START, 0, TimeStamp(), 0);
  mtiStart.mTouches.AppendElement(SingleTouchData(inputId, ScreenIntPoint(aFocusX, aFocusY), ScreenSize(0, 0), 0, 0));
  mtiStart.mTouches.AppendElement(SingleTouchData(inputId + 1, ScreenIntPoint(aFocusX, aFocusY), ScreenSize(0, 0), 0, 0));
  nsEventStatus status = aApzc->ReceiveInputEvent(mtiStart);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[0] = status;
  }

  if (gfxPrefs::TouchActionEnabled() && aAllowedTouchBehaviors) {
    aApzc->SetAllowedTouchBehavior(*aAllowedTouchBehaviors);
  }

  MultiTouchInput mtiMove1 = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, 0, TimeStamp(), 0);
  mtiMove1.mTouches.AppendElement(SingleTouchData(inputId, ScreenIntPoint(aFocusX - pinchLength, aFocusY), ScreenSize(0, 0), 0, 0));
  mtiMove1.mTouches.AppendElement(SingleTouchData(inputId + 1, ScreenIntPoint(aFocusX + pinchLength, aFocusY), ScreenSize(0, 0), 0, 0));
  status = aApzc->ReceiveInputEvent(mtiMove1);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[1] = status;
  }

  MultiTouchInput mtiMove2 = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, 0, TimeStamp(), 0);
  mtiMove2.mTouches.AppendElement(SingleTouchData(inputId, ScreenIntPoint(aFocusX - pinchLengthScaled, aFocusY), ScreenSize(0, 0), 0, 0));
  mtiMove2.mTouches.AppendElement(SingleTouchData(inputId + 1, ScreenIntPoint(aFocusX + pinchLengthScaled, aFocusY), ScreenSize(0, 0), 0, 0));
  status = aApzc->ReceiveInputEvent(mtiMove2);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[2] = status;
  }

  MultiTouchInput mtiEnd = MultiTouchInput(MultiTouchInput::MULTITOUCH_END, 0, TimeStamp(), 0);
  mtiEnd.mTouches.AppendElement(SingleTouchData(inputId, ScreenIntPoint(aFocusX - pinchLengthScaled, aFocusY), ScreenSize(0, 0), 0, 0));
  mtiEnd.mTouches.AppendElement(SingleTouchData(inputId + 1, ScreenIntPoint(aFocusX + pinchLengthScaled, aFocusY), ScreenSize(0, 0), 0, 0));
  status = aApzc->ReceiveInputEvent(mtiEnd);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[3] = status;
  }

  inputId += 2;
}

static void
ApzcPinchWithTouchInputAndCheckStatus(AsyncPanZoomController* aApzc,
                                      int aFocusX, int aFocusY, float aScale,
                                      int& inputId, bool aShouldTriggerPinch,
                                      nsTArray<uint32_t>* aAllowedTouchBehaviors)
{
  nsEventStatus statuses[4];  // down, move, move, up
  ApzcPinchWithTouchInput(aApzc, aFocusX, aFocusY, aScale, inputId, aAllowedTouchBehaviors, &statuses);

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
    fm.mScrollableRect = CSSRect(0, 0, 980, 1000);
    fm.SetScrollOffset(CSSPoint(300, 300));
    fm.SetZoom(CSSToScreenScale(2.0));
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
      ApzcPinchWithTouchInputAndCheckStatus(apzc, 250, 300, 1.25, touchInputId, aShouldTriggerPinch, aAllowedTouchBehaviors);
    } else {
      ApzcPinchWithPinchInputAndCheckStatus(apzc, 250, 300, 1.25, aShouldTriggerPinch);
    }

    FrameMetrics fm = apzc->GetFrameMetrics();

    if (aShouldTriggerPinch) {
      // the visible area of the document in CSS pixels is now x=305 y=310 w=40 h=80
      EXPECT_EQ(2.5f, fm.GetZoom().scale);
      EXPECT_EQ(305, fm.GetScrollOffset().x);
      EXPECT_EQ(310, fm.GetScrollOffset().y);
    } else {
      // The frame metrics should stay the same since touch-action:none makes
      // apzc ignore pinch gestures.
      EXPECT_EQ(2.0f, fm.GetZoom().scale);
      EXPECT_EQ(300, fm.GetScrollOffset().x);
      EXPECT_EQ(300, fm.GetScrollOffset().y);
    }

    // part 2 of the test, move to the top-right corner of the page and pinch and
    // make sure we stay in the correct spot
    fm.SetZoom(CSSToScreenScale(2.0));
    fm.SetScrollOffset(CSSPoint(930, 5));
    apzc->SetFrameMetrics(fm);
    // the visible area of the document in CSS pixels is x=930 y=5 w=50 h=100

    if (mGestureBehavior == AsyncPanZoomController::USE_GESTURE_DETECTOR) {
      ApzcPinchWithTouchInputAndCheckStatus(apzc, 250, 300, 0.5, touchInputId, aShouldTriggerPinch, aAllowedTouchBehaviors);
    } else {
      ApzcPinchWithPinchInputAndCheckStatus(apzc, 250, 300, 0.5, aShouldTriggerPinch);
    }

    fm = apzc->GetFrameMetrics();

    if (aShouldTriggerPinch) {
      // the visible area of the document in CSS pixels is now x=880 y=0 w=100 h=200
      EXPECT_EQ(1.0f, fm.GetZoom().scale);
      EXPECT_EQ(880, fm.GetScrollOffset().x);
      EXPECT_EQ(0, fm.GetScrollOffset().y);
    } else {
      EXPECT_EQ(2.0f, fm.GetZoom().scale);
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
  DoPinchTest(true);
}

TEST_F(APZCPinchGestureDetectorTester, Pinch_UseGestureDetector_NoTouchAction) {
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

  SetMayHaveTouchListeners();
  MakeApzcZoomable();

  int touchInputId = 0;
  ApzcPinchWithTouchInput(apzc, 250, 300, 1.25, touchInputId);

  // Send the prevent-default notification for the touch block
  apzc->ContentReceivedTouch(true);

  // Run all pending tasks (this should include at least the
  // prevent-default timer).
  EXPECT_LE(1, mcc->RunThroughDelayedTasks());

  // verify the metrics didn't change (i.e. the pinch was ignored)
  FrameMetrics fm = apzc->GetFrameMetrics();
  EXPECT_EQ(originalMetrics.GetZoom().scale, fm.GetZoom().scale);
  EXPECT_EQ(originalMetrics.GetScrollOffset().x, fm.GetScrollOffset().x);
  EXPECT_EQ(originalMetrics.GetScrollOffset().y, fm.GetScrollOffset().y);

  apzc->AssertStateIsReset();
}

TEST_F(APZCBasicTester, Overzoom) {
  // the visible area of the document in CSS pixels is x=10 y=0 w=100 h=100
  FrameMetrics fm;
  fm.mCompositionBounds = ParentLayerRect(0, 0, 100, 100);
  fm.mScrollableRect = CSSRect(0, 0, 125, 150);
  fm.SetScrollOffset(CSSPoint(10, 0));
  fm.SetZoom(CSSToScreenScale(1.0));
  apzc->SetFrameMetrics(fm);

  MakeApzcZoomable();

  EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_,_)).Times(AtLeast(1));
  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);

  ApzcPinchWithPinchInputAndCheckStatus(apzc, 50, 50, 0.5, true);

  fm = apzc->GetFrameMetrics();
  EXPECT_EQ(0.8f, fm.GetZoom().scale);
  // bug 936721 - PGO builds introduce rounding error so
  // use a fuzzy match instead
  EXPECT_LT(abs(fm.GetScrollOffset().x), 1e-5);
  EXPECT_LT(abs(fm.GetScrollOffset().y), 1e-5);
}

TEST_F(APZCBasicTester, SimpleTransform) {
  ScreenPoint pointOut;
  ViewTransform viewTransformOut;
  apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);

  EXPECT_EQ(ScreenPoint(), pointOut);
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
  transforms[0].ScalePost(0.5f, 0.5f, 1.0f); // this results from the 2.0 resolution on the root layer
  transforms[1].ScalePost(2.0f, 1.0f, 1.0f); // this is the 2.0 x-axis CSS transform on the child layer

  nsTArray<nsRefPtr<Layer> > layers;
  nsRefPtr<LayerManager> lm;
  nsRefPtr<Layer> root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, transforms, lm, layers);

  FrameMetrics metrics;
  metrics.mCompositionBounds = ParentLayerRect(0, 0, 24, 24);
  metrics.mDisplayPort = CSSRect(-1, -1, 6, 6);
  metrics.SetScrollOffset(CSSPoint(10, 10));
  metrics.mScrollableRect = CSSRect(0, 0, 50, 50);
  metrics.mCumulativeResolution = LayoutDeviceToLayerScale(2);
  metrics.mResolution = ParentLayerToLayerScale(2);
  metrics.SetZoom(CSSToScreenScale(6));
  metrics.mDevPixelsPerCSSPixel = CSSToLayoutDeviceScale(3);
  metrics.SetScrollId(FrameMetrics::START_SCROLL_ID);

  FrameMetrics childMetrics = metrics;
  childMetrics.SetScrollId(FrameMetrics::START_SCROLL_ID + 1);

  layers[0]->SetFrameMetrics(metrics);
  layers[1]->SetFrameMetrics(childMetrics);

  ScreenPoint pointOut;
  ViewTransform viewTransformOut;

  // Both the parent and child layer should behave exactly the same here, because
  // the CSS transform on the child layer does not affect the SampleContentTransformForFrame code

  // initial transform
  apzc->SetFrameMetrics(metrics);
  apzc->NotifyLayersUpdated(metrics, true);
  apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(ParentLayerToScreenScale(2), ScreenPoint()), viewTransformOut);
  EXPECT_EQ(ScreenPoint(60, 60), pointOut);

  childApzc->SetFrameMetrics(childMetrics);
  childApzc->NotifyLayersUpdated(childMetrics, true);
  childApzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(ParentLayerToScreenScale(2), ScreenPoint()), viewTransformOut);
  EXPECT_EQ(ScreenPoint(60, 60), pointOut);

  // do an async scroll by 5 pixels and check the transform
  metrics.ScrollBy(CSSPoint(5, 0));
  apzc->SetFrameMetrics(metrics);
  apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(ParentLayerToScreenScale(2), ScreenPoint(-30, 0)), viewTransformOut);
  EXPECT_EQ(ScreenPoint(90, 60), pointOut);

  childMetrics.ScrollBy(CSSPoint(5, 0));
  childApzc->SetFrameMetrics(childMetrics);
  childApzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(ParentLayerToScreenScale(2), ScreenPoint(-30, 0)), viewTransformOut);
  EXPECT_EQ(ScreenPoint(90, 60), pointOut);

  // do an async zoom of 1.5x and check the transform
  metrics.ZoomBy(1.5f);
  apzc->SetFrameMetrics(metrics);
  apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(ParentLayerToScreenScale(3), ScreenPoint(-45, 0)), viewTransformOut);
  EXPECT_EQ(ScreenPoint(135, 90), pointOut);

  childMetrics.ZoomBy(1.5f);
  childApzc->SetFrameMetrics(childMetrics);
  childApzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(ParentLayerToScreenScale(3), ScreenPoint(-45, 0)), viewTransformOut);
  EXPECT_EQ(ScreenPoint(135, 90), pointOut);
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
    ScreenPoint pointOut;
    ViewTransform viewTransformOut;

    nsTArray<uint32_t> allowedTouchBehaviors;
    allowedTouchBehaviors.AppendElement(aBehavior);

    // Pan down
    ApzcPanAndCheckStatus(apzc, time, touchStart, touchEnd, aShouldBeConsumed, &allowedTouchBehaviors);
    apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);

    if (aShouldTriggerScroll) {
      EXPECT_EQ(ScreenPoint(0, -(touchEnd-touchStart)), pointOut);
      EXPECT_NE(ViewTransform(), viewTransformOut);
    } else {
      EXPECT_EQ(ScreenPoint(), pointOut);
      EXPECT_EQ(ViewTransform(), viewTransformOut);
    }

    // Clear the fling from the previous pan, or stopping it will
    // consume the next touchstart
    apzc->CancelAnimation();

    // Pan back
    ApzcPanAndCheckStatus(apzc, time, touchEnd, touchStart, aShouldBeConsumed, &allowedTouchBehaviors);
    apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);

    EXPECT_EQ(ScreenPoint(), pointOut);
    EXPECT_EQ(ViewTransform(), viewTransformOut);
  }

  void DoPanWithPreventDefaultTest()
  {
    SetMayHaveTouchListeners();

    int time = 0;
    int touchStart = 50;
    int touchEnd = 10;
    ScreenPoint pointOut;
    ViewTransform viewTransformOut;

    // Pan down
    nsTArray<uint32_t> allowedTouchBehaviors;
    allowedTouchBehaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN);
    ApzcPanAndCheckStatus(apzc, time, touchStart, touchEnd, true, &allowedTouchBehaviors);

    // Send the signal that content has handled and preventDefaulted the touch
    // events. This flushes the event queue.
    apzc->ContentReceivedTouch(true);
    // Run all pending tasks (this should include at least the
    // prevent-default timer).
    EXPECT_LE(1, mcc->RunThroughDelayedTasks());

    apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
    EXPECT_EQ(ScreenPoint(), pointOut);
    EXPECT_EQ(ViewTransform(), viewTransformOut);

    apzc->AssertStateIsReset();
  }
};

TEST_F(APZCPanningTester, Pan) {
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
  ScreenPoint pointOut;
  ViewTransform viewTransformOut;

  // Fling down. Each step scroll further down
  ApzcPan(apzc, time, touchStart, touchEnd);
  ScreenPoint lastPoint;
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
  ApzcPan(apzc, time, 25, 45);
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
  ApzcPan(apzc, time, 25, 45);
  apzc->AdvanceAnimationsUntilEnd(testStartTime);
  check.Call("Done");
}

TEST_F(APZCBasicTester, OverScrollPanning) {
  SCOPED_GFX_PREF(APZOverscrollEnabled, bool, true);

  // Pan sufficiently to hit overscroll behavior
  int time = 0;
  int touchStart = 500;
  int touchEnd = 10;
  ApzcPan(apzc, time, touchStart, touchEnd);
  EXPECT_TRUE(apzc->IsOverscrolled());

  // Note that in the calls to SampleContentTransformForFrame below, the time
  // increment used is sufficiently large for the animation to have completed. However,
  // any single call to SampleContentTransformForFrame will not finish an animation
  // *and* also proceed through the following animation, if there is one.
  // Therefore the minimum number of calls to go from an overscroll-inducing pan
  // to a reset state is 3; these are documented further below.

  ScreenPoint pointOut;
  ViewTransform viewTransformOut;

  // This sample will run to the end of the non-overscrolling fling animation
  // and will schedule the overscrolling fling animation.
  apzc->SampleContentTransformForFrame(testStartTime + TimeDuration::FromMilliseconds(10000), &viewTransformOut, pointOut);
  EXPECT_EQ(ScreenPoint(0, 90), pointOut);
  EXPECT_TRUE(apzc->IsOverscrolled());

  // This sample will run to the end of the overscrolling fling animation and
  // will schedule the snapback animation.
  apzc->SampleContentTransformForFrame(testStartTime + TimeDuration::FromMilliseconds(20000), &viewTransformOut, pointOut);
  EXPECT_EQ(ScreenPoint(0, 90), pointOut);
  EXPECT_TRUE(apzc->IsOverscrolled());

  // This sample will run to the end of the snapback animation and reset the state.
  apzc->SampleContentTransformForFrame(testStartTime + TimeDuration::FromMilliseconds(30000), &viewTransformOut, pointOut);
  EXPECT_EQ(ScreenPoint(0, 90), pointOut);
  EXPECT_FALSE(apzc->IsOverscrolled());

  apzc->AssertStateIsReset();
}

TEST_F(APZCBasicTester, OverScrollAbort) {
  SCOPED_GFX_PREF(APZOverscrollEnabled, bool, true);

  // Pan sufficiently to hit overscroll behavior
  int time = 0;
  int touchStart = 500;
  int touchEnd = 10;
  ApzcPan(apzc, time, touchStart, touchEnd);
  EXPECT_TRUE(apzc->IsOverscrolled());

  ScreenPoint pointOut;
  ViewTransform viewTransformOut;

  // This sample call will run to the end of the non-overscrolling fling animation
  // and will schedule the overscrolling fling animation (see comment in OverScrollPanning
  // above for more explanation).
  apzc->SampleContentTransformForFrame(testStartTime + TimeDuration::FromMilliseconds(10000), &viewTransformOut, pointOut);
  EXPECT_TRUE(apzc->IsOverscrolled());

  // At this point, we have an active overscrolling fling animation.
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
  ApzcPan(apzc, time, touchStart, touchEnd,
          true);                   // keep finger down
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
    ApzcPan(apzc, time, touchStart, touchEnd);
    // The touchstart from the pan will leave some cancelled tasks in the queue, clear them out
    while (mcc->RunThroughDelayedTasks());

    // If we want to tap while the fling is fast, let the fling advance for 10ms only. If we want
    // the fling to slow down more, advance to 2000ms. These numbers may need adjusting if our
    // friction and threshold values change, but they should be deterministic at least.
    int timeDelta = aSlow ? 2000 : 10;
    int tapCallsExpected = aSlow ? 1 : 0;

    // Advance the fling animation by timeDelta milliseconds.
    ScreenPoint pointOut;
    ViewTransform viewTransformOut;
    apzc->SampleContentTransformForFrame(testStartTime + TimeDuration::FromMilliseconds(timeDelta), &viewTransformOut, pointOut);

    // Deliver a tap to abort the fling. Ensure that we get a HandleSingleTap
    // call out of it if and only if the fling is slow.
    EXPECT_CALL(*mcc, HandleSingleTap(_, 0, apzc->GetGuid())).Times(tapCallsExpected);
    ApzcTap(apzc, 10, 10, time, 0);
    while (mcc->RunThroughDelayedTasks());

    // Verify that we didn't advance any further after the fling was aborted, in either case.
    ScreenPoint finalPointOut;
    apzc->SampleContentTransformForFrame(testStartTime + TimeDuration::FromMilliseconds(timeDelta + 1000), &viewTransformOut, finalPointOut);
    EXPECT_EQ(pointOut.x, finalPointOut.x);
    EXPECT_EQ(pointOut.y, finalPointOut.y);

    apzc->AssertStateIsReset();
  }

  void DoFlingStopWithSlowListener(bool aPreventDefault) {
    SetMayHaveTouchListeners();

    int time = 0;
    int touchStart = 50;
    int touchEnd = 10;

    // Start the fling down.
    ApzcPan(apzc, time, touchStart, touchEnd);
    apzc->ContentReceivedTouch(false);
    while (mcc->RunThroughDelayedTasks());

    // Sample the fling a couple of times to ensure it's going.
    ScreenPoint point, finalPoint;
    ViewTransform viewTransform;
    apzc->SampleContentTransformForFrame(testStartTime + TimeDuration::FromMilliseconds(10), &viewTransform, point);
    apzc->SampleContentTransformForFrame(testStartTime + TimeDuration::FromMilliseconds(20), &viewTransform, finalPoint);
    EXPECT_GT(finalPoint.y, point.y);

    // Now we put our finger down to stop the fling
    ApzcDown(apzc, 10, 10, time);

    // Re-sample to make sure it hasn't moved
    apzc->SampleContentTransformForFrame(testStartTime + TimeDuration::FromMilliseconds(30), &viewTransform, point);
    EXPECT_EQ(finalPoint.x, point.x);
    EXPECT_EQ(finalPoint.y, point.y);

    // respond to the touchdown that stopped the fling.
    // even if we do a prevent-default on it, the animation should remain stopped.
    apzc->ContentReceivedTouch(aPreventDefault);
    while (mcc->RunThroughDelayedTasks());

    // Verify the page hasn't moved
    apzc->SampleContentTransformForFrame(testStartTime + TimeDuration::FromMilliseconds(100), &viewTransform, point);
    EXPECT_EQ(finalPoint.x, point.x);
    EXPECT_EQ(finalPoint.y, point.y);

    // clean up
    ApzcUp(apzc, 10, 10, time);
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
  ApzcTapAndCheckStatus(apzc, 10, 10, time, 100);
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
  ApzcTapAndCheckStatus(apzc, 10, 10, time, 400);
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

    nsEventStatus status = ApzcDown(apzc, 10, 10, time);
    EXPECT_EQ(nsEventStatus_eConsumeDoDefault, status);

    if (gfxPrefs::TouchActionEnabled()) {
      // SetAllowedTouchBehavior() must be called after sending touch-start.
      nsTArray<uint32_t> allowedTouchBehaviors;
      allowedTouchBehaviors.AppendElement(aBehavior);
      apzc->SetAllowedTouchBehavior(allowedTouchBehaviors);
    }
    // Have content "respond" to the touchstart
    apzc->ContentReceivedTouch(false);

    MockFunction<void(std::string checkPointName)> check;

    {
      InSequence s;

      EXPECT_CALL(check, Call("preHandleLongTap"));
      EXPECT_CALL(*mcc, HandleLongTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(1);
      EXPECT_CALL(check, Call("postHandleLongTap"));

      EXPECT_CALL(check, Call("preHandleLongTapUp"));
      EXPECT_CALL(*mcc, HandleLongTapUp(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(1);
      EXPECT_CALL(check, Call("postHandleLongTapUp"));
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
    apzc->ContentReceivedTouch(false);
    mcc->CheckHasDelayedTask();
    mcc->RunDelayedTask();

    time += 1000;

    // Finally, simulate lifting the finger. Since the long-press wasn't
    // prevent-defaulted, we should get a long-tap-up event.
    check.Call("preHandleLongTapUp");
    status = ApzcUp(apzc, 10, 10, time);
    EXPECT_EQ(nsEventStatus_eConsumeDoDefault, status);
    check.Call("postHandleLongTapUp");

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
    nsEventStatus status = ApzcDown(apzc, touchX, touchStartY, time);
    EXPECT_EQ(nsEventStatus_eConsumeDoDefault, status);

    if (gfxPrefs::TouchActionEnabled()) {
      // SetAllowedTouchBehavior() must be called after sending touch-start.
      nsTArray<uint32_t> allowedTouchBehaviors;
      allowedTouchBehaviors.AppendElement(aBehavior);
      apzc->SetAllowedTouchBehavior(allowedTouchBehaviors);
    }
    // Have content "respond" to the touchstart
    apzc->ContentReceivedTouch(false);

    MockFunction<void(std::string checkPointName)> check;

    {
      InSequence s;

      EXPECT_CALL(check, Call("preHandleLongTap"));
      EXPECT_CALL(*mcc, HandleLongTap(CSSPoint(touchX, touchStartY), 0, apzc->GetGuid())).Times(1);
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
    apzc->ContentReceivedTouch(true);
    mcc->CheckHasDelayedTask();
    mcc->RunDelayedTask();

    time += 1000;

    MultiTouchInput mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, time, TimeStamp(), 0);
    mti.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(touchX, touchEndY), ScreenSize(0, 0), 0, 0));
    status = apzc->ReceiveInputEvent(mti);
    EXPECT_EQ(nsEventStatus_eConsumeDoDefault, status);

    EXPECT_CALL(*mcc, HandleLongTapUp(CSSPoint(touchX, touchEndY), 0, apzc->GetGuid())).Times(0);
    status = ApzcUp(apzc, touchX, touchEndY, time);
    EXPECT_EQ(nsEventStatus_eConsumeDoDefault, status);

    ScreenPoint pointOut;
    ViewTransform viewTransformOut;
    apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);

    EXPECT_EQ(ScreenPoint(), pointOut);
    EXPECT_EQ(ViewTransform(), viewTransformOut);

    apzc->AssertStateIsReset();
  }
};

TEST_F(APZCLongPressTester, LongPress) {
  DoLongPressTest(mozilla::layers::AllowedTouchBehavior::NONE);
}

TEST_F(APZCLongPressTester, LongPressWithTouchAction) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, true);
  DoLongPressTest(mozilla::layers::AllowedTouchBehavior::HORIZONTAL_PAN
                  | mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN
                  | mozilla::layers::AllowedTouchBehavior::PINCH_ZOOM);
}

TEST_F(APZCLongPressTester, LongPressPreventDefault) {
  DoLongPressPreventDefaultTest(mozilla::layers::AllowedTouchBehavior::NONE);
}

TEST_F(APZCLongPressTester, LongPressPreventDefaultWithTouchAction) {
  SCOPED_GFX_PREF(TouchActionEnabled, bool, true);
  DoLongPressPreventDefaultTest(mozilla::layers::AllowedTouchBehavior::HORIZONTAL_PAN
                                | mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN
                                | mozilla::layers::AllowedTouchBehavior::PINCH_ZOOM);
}

static void
ApzcDoubleTap(AsyncPanZoomController* aApzc, int aX, int aY, int& aTime,
              nsEventStatus (*aOutEventStatuses)[4] = nullptr)
{
  nsEventStatus status = ApzcDown(aApzc, aX, aY, aTime);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[0] = status;
  }
  aTime += 10;
  status = ApzcUp(aApzc, aX, aY, aTime);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[1] = status;
  }
  aTime += 10;
  status = ApzcDown(aApzc, aX, aY, aTime);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[2] = status;
  }
  aTime += 10;
  status = ApzcUp(aApzc, aX, aY, aTime);
  if (aOutEventStatuses) {
    (*aOutEventStatuses)[3] = status;
  }
}

static void
ApzcDoubleTapAndCheckStatus(AsyncPanZoomController* aApzc, int aX, int aY, int& aTime)
{
  nsEventStatus statuses[4];
  ApzcDoubleTap(aApzc, aX, aY, aTime, &statuses);
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, statuses[0]);
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, statuses[1]);
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, statuses[2]);
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, statuses[3]);
}

TEST_F(APZCGestureDetectorTester, DoubleTap) {
  SetMayHaveTouchListeners();
  MakeApzcZoomable();

  EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(0);
  EXPECT_CALL(*mcc, HandleDoubleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(1);

  int time = 0;
  ApzcDoubleTapAndCheckStatus(apzc, 10, 10, time);

  // responses to the two touchstarts
  apzc->ContentReceivedTouch(false);
  apzc->ContentReceivedTouch(false);

  while (mcc->RunThroughDelayedTasks());

  apzc->AssertStateIsReset();
}

TEST_F(APZCGestureDetectorTester, DoubleTapNotZoomable) {
  SetMayHaveTouchListeners();
  MakeApzcUnzoomable();

  EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(2);
  EXPECT_CALL(*mcc, HandleDoubleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(0);

  int time = 0;
  ApzcDoubleTapAndCheckStatus(apzc, 10, 10, time);

  // responses to the two touchstarts
  apzc->ContentReceivedTouch(false);
  apzc->ContentReceivedTouch(false);

  while (mcc->RunThroughDelayedTasks());

  apzc->AssertStateIsReset();
}

TEST_F(APZCGestureDetectorTester, DoubleTapPreventDefaultFirstOnly) {
  SetMayHaveTouchListeners();
  MakeApzcZoomable();

  EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(1);
  EXPECT_CALL(*mcc, HandleDoubleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(0);

  int time = 0;
  ApzcDoubleTapAndCheckStatus(apzc, 10, 10, time);

  // responses to the two touchstarts
  apzc->ContentReceivedTouch(true);
  apzc->ContentReceivedTouch(false);

  while (mcc->RunThroughDelayedTasks());

  apzc->AssertStateIsReset();
}

TEST_F(APZCGestureDetectorTester, DoubleTapPreventDefaultBoth) {
  SetMayHaveTouchListeners();
  MakeApzcZoomable();

  EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(0);
  EXPECT_CALL(*mcc, HandleDoubleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(0);

  int time = 0;
  ApzcDoubleTapAndCheckStatus(apzc, 10, 10, time);

  // responses to the two touchstarts
  apzc->ContentReceivedTouch(true);
  apzc->ContentReceivedTouch(true);

  while (mcc->RunThroughDelayedTasks());

  apzc->AssertStateIsReset();
}

// Test for bug 947892
// We test whether we dispatch tap event when the tap is followed by pinch.
TEST_F(APZCGestureDetectorTester, TapFollowedByPinch) {
  MakeApzcZoomable();

  EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(1);

  int time = 0;
  ApzcTap(apzc, 10, 10, time, 100);

  int inputId = 0;
  MultiTouchInput mti;
  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_START, time, TimeStamp(), 0);
  mti.mTouches.AppendElement(SingleTouchData(inputId, ScreenIntPoint(20, 20), ScreenSize(0, 0), 0, 0));
  mti.mTouches.AppendElement(SingleTouchData(inputId + 1, ScreenIntPoint(10, 10), ScreenSize(0, 0), 0, 0));
  apzc->ReceiveInputEvent(mti);

  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_END, time, TimeStamp(), 0);
  mti.mTouches.AppendElement(SingleTouchData(inputId, ScreenIntPoint(20, 20), ScreenSize(0, 0), 0, 0));
  mti.mTouches.AppendElement(SingleTouchData(inputId + 1, ScreenIntPoint(10, 10), ScreenSize(0, 0), 0, 0));
  apzc->ReceiveInputEvent(mti);

  while (mcc->RunThroughDelayedTasks());

  apzc->AssertStateIsReset();
}

TEST_F(APZCGestureDetectorTester, TapFollowedByMultipleTouches) {
  MakeApzcZoomable();

  EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(1);

  int time = 0;
  ApzcTap(apzc, 10, 10, time, 100);

  int inputId = 0;
  MultiTouchInput mti;
  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_START, time, TimeStamp(), 0);
  mti.mTouches.AppendElement(SingleTouchData(inputId, ScreenIntPoint(20, 20), ScreenSize(0, 0), 0, 0));
  apzc->ReceiveInputEvent(mti);

  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_START, time, TimeStamp(), 0);
  mti.mTouches.AppendElement(SingleTouchData(inputId, ScreenIntPoint(20, 20), ScreenSize(0, 0), 0, 0));
  mti.mTouches.AppendElement(SingleTouchData(inputId + 1, ScreenIntPoint(10, 10), ScreenSize(0, 0), 0, 0));
  apzc->ReceiveInputEvent(mti);

  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_END, time, TimeStamp(), 0);
  mti.mTouches.AppendElement(SingleTouchData(inputId, ScreenIntPoint(20, 20), ScreenSize(0, 0), 0, 0));
  mti.mTouches.AppendElement(SingleTouchData(inputId + 1, ScreenIntPoint(10, 10), ScreenSize(0, 0), 0, 0));
  apzc->ReceiveInputEvent(mti);

  while (mcc->RunThroughDelayedTasks());

  apzc->AssertStateIsReset();
}

class APZCTreeManagerTester : public ::testing::Test {
protected:
  virtual void SetUp() {
    gfxPrefs::GetSingleton();
    AsyncPanZoomController::SetThreadAssertionsEnabled(false);

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
                                        // The scrollable rect is only used in HitTesting2,
                                        // HitTesting1 doesn't care about it.
                                        CSSRect aScrollableRect = CSSRect(-1, -1, -1, -1)) {
    FrameMetrics metrics;
    metrics.SetScrollId(aScrollId);
    nsIntRect layerBound = aLayer->GetVisibleRegion().GetBounds();
    metrics.mCompositionBounds = ParentLayerRect(layerBound.x, layerBound.y,
                                                 layerBound.width, layerBound.height);
    metrics.mScrollableRect = aScrollableRect;
    metrics.SetScrollOffset(CSSPoint(0, 0));
    aLayer->SetFrameMetrics(metrics);
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
    const char* layerTreeSyntax = "c(c(c(c))c(c(c)))";
    // LayerID                     0 1 2 3  4 5 6
    root = CreateLayerTree(layerTreeSyntax, nullptr, nullptr, lm, layers);
    SetScrollableFrameMetrics(layers[0], FrameMetrics::START_SCROLL_ID);
    SetScrollableFrameMetrics(layers[2], FrameMetrics::START_SCROLL_ID + 1);
    SetScrollableFrameMetrics(layers[5], FrameMetrics::START_SCROLL_ID + 1);
    SetScrollableFrameMetrics(layers[3], FrameMetrics::START_SCROLL_ID + 2);
    SetScrollableFrameMetrics(layers[6], FrameMetrics::START_SCROLL_ID + 3);
  }
};

// A version of ApzcPan() that routes the pan through the tree manager,
// so that the tree manager has the appropriate state for testing.
static void
ApzctmPan(APZCTreeManager* aTreeManager,
          int& aTime,
          int aTouchStartY,
          int aTouchEndY,
          bool aKeepFingerDown = false)
{
  // TODO: Reuse some code between this and ApzcPan().

  // Reduce the touch start tolerance to a tiny value.
  // We can't do what ApzcPan() does to overcome the tolerance (send the
  // touch-start at (aTouchStartY + some_large_value)) because the tree manager
  // does hit testing based on the touch-start coordinates, and a different
  // APZC than the one we intend might be hit.
  SCOPED_GFX_PREF(APZTouchStartTolerance, float, 1.0f / 1000.0f);
  const int OVERCOME_TOUCH_TOLERANCE = 1;

  const int TIME_BETWEEN_TOUCH_EVENT = 100;

  // Make sure the move is large enough to not be handled as a tap
  MultiTouchInput mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_START, aTime, TimeStamp(), 0);
  mti.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(10, aTouchStartY), ScreenSize(0, 0), 0, 0));
  aTreeManager->ReceiveInputEvent(mti, nullptr);

  aTime += TIME_BETWEEN_TOUCH_EVENT;

  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, aTime, TimeStamp(), 0);
  mti.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(10, aTouchStartY + OVERCOME_TOUCH_TOLERANCE), ScreenSize(0, 0), 0, 0));
  aTreeManager->ReceiveInputEvent(mti, nullptr);

  aTime += TIME_BETWEEN_TOUCH_EVENT;

  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, aTime, TimeStamp(), 0);
  mti.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(10, aTouchEndY), ScreenSize(0, 0), 0, 0));
  aTreeManager->ReceiveInputEvent(mti, nullptr);

  aTime += TIME_BETWEEN_TOUCH_EVENT;

  if (!aKeepFingerDown) {
    mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_END, aTime, TimeStamp(), 0);
    mti.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(10, aTouchEndY), ScreenSize(0, 0), 0, 0));
    aTreeManager->ReceiveInputEvent(mti, nullptr);
  }

  aTime += TIME_BETWEEN_TOUCH_EVENT;
}

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
    const char* layerTreeSyntax = "c(ttcc)";
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
    const char* layerTreeSyntax = "c(cc(c))";
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
  AsyncPanZoomController* nullAPZC = nullptr;
  EXPECT_EQ(nullAPZC, hit.get());
  EXPECT_EQ(Matrix4x4(), transformToApzc);
  EXPECT_EQ(Matrix4x4(), transformToGecko);

  uint32_t paintSequenceNumber = 0;

  // Now we have a root APZC that will match the page
  SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID);
  manager->UpdatePanZoomControllerTree(nullptr, root, false, 0, paintSequenceNumber++);
  hit = GetTargetAPZC(ScreenPoint(15, 15));
  EXPECT_EQ(ApzcOf(root), hit.get());
  // expect hit point at LayerIntPoint(15, 15)
  EXPECT_EQ(Point(15, 15), transformToApzc * Point(15, 15));
  EXPECT_EQ(Point(15, 15), transformToGecko * Point(15, 15));

  // Now we have a sub APZC with a better fit
  SetScrollableFrameMetrics(layers[3], FrameMetrics::START_SCROLL_ID + 1);
  manager->UpdatePanZoomControllerTree(nullptr, root, false, 0, paintSequenceNumber++);
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
  manager->UpdatePanZoomControllerTree(nullptr, root, false, 0, paintSequenceNumber++);
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

  manager->UpdatePanZoomControllerTree(nullptr, root, false, 0, 0);

  // At this point, the following holds (all coordinates in screen pixels):
  // layers[0] has content from (0,0)-(200,200), clipped by composition bounds (0,0)-(100,100)
  // layers[1] has content from (10,10)-(90,90), clipped by composition bounds (10,10)-(50,50)
  // layers[2] has content from (20,60)-(100,100). no clipping as it's not a scrollable layer
  // layers[3] has content from (20,60)-(180,140), clipped by composition bounds (20,60)-(100,100)

  AsyncPanZoomController* apzcroot = ApzcOf(root);
  AsyncPanZoomController* apzc1 = ApzcOf(layers[1]);
  AsyncPanZoomController* apzc3 = ApzcOf(layers[3]);

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
  manager->UpdatePanZoomControllerTree(nullptr, root, false, 0, 0);

  AsyncPanZoomController* nullAPZC = nullptr;
  // so they should have the same APZC
  EXPECT_FALSE(layers[0]->HasScrollableFrameMetrics());
  EXPECT_NE(nullAPZC, ApzcOf(layers[1]));
  EXPECT_NE(nullAPZC, ApzcOf(layers[2]));
  EXPECT_EQ(ApzcOf(layers[1]), ApzcOf(layers[2]));

  // Change the scrollId of layers[1], and verify the APZC changes
  SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID + 1);
  manager->UpdatePanZoomControllerTree(nullptr, root, false, 0, 0);
  EXPECT_NE(ApzcOf(layers[1]), ApzcOf(layers[2]));

  // Change the scrollId of layers[2] to match that of layers[1], ensure we get the same
  // APZC for both again
  SetScrollableFrameMetrics(layers[2], FrameMetrics::START_SCROLL_ID + 1);
  manager->UpdatePanZoomControllerTree(nullptr, root, false, 0, 0);
  EXPECT_EQ(ApzcOf(layers[1]), ApzcOf(layers[2]));
}

TEST_F(APZCTreeManagerTester, Bug1068268) {
  CreatePotentiallyLeakingTree();
  ScopedLayerTreeRegistration registration(0, root, mcc);

  manager->UpdatePanZoomControllerTree(nullptr, root, false, 0, 0);
  EXPECT_EQ(ApzcOf(layers[2]), ApzcOf(layers[0])->GetLastChild());
  EXPECT_EQ(ApzcOf(layers[2]), ApzcOf(layers[0])->GetFirstChild());
  EXPECT_EQ(ApzcOf(layers[0]), ApzcOf(layers[2])->GetParent());
  EXPECT_EQ(ApzcOf(layers[2]), ApzcOf(layers[5]));

  EXPECT_EQ(ApzcOf(layers[3]), ApzcOf(layers[2])->GetFirstChild());
  EXPECT_EQ(ApzcOf(layers[6]), ApzcOf(layers[2])->GetLastChild());
  EXPECT_EQ(ApzcOf(layers[3]), ApzcOf(layers[6])->GetPrevSibling());
  EXPECT_EQ(ApzcOf(layers[2]), ApzcOf(layers[3])->GetParent());
  EXPECT_EQ(ApzcOf(layers[2]), ApzcOf(layers[6])->GetParent());
}

TEST_F(APZHitTestingTester, ComplexMultiLayerTree) {
  CreateComplexMultiLayerTree();
  ScopedLayerTreeRegistration registration(0, root, mcc);
  manager->UpdatePanZoomControllerTree(nullptr, root, false, 0, 0);

  AsyncPanZoomController* nullAPZC = nullptr;
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

  // Ensure the shape of the APZC tree is as expected
  AsyncPanZoomController* layers1_2 = ApzcOf(layers[1]);
  AsyncPanZoomController* layers4_6_8 = ApzcOf(layers[4]);
  AsyncPanZoomController* layer7 = ApzcOf(layers[7]);
  AsyncPanZoomController* layer9 = ApzcOf(layers[9]);
  EXPECT_EQ(nullptr, layers1_2->GetParent());
  EXPECT_EQ(nullptr, layers4_6_8->GetParent());
  EXPECT_EQ(layers4_6_8, layer7->GetParent());
  EXPECT_EQ(nullptr, layer9->GetParent());
  EXPECT_EQ(nullptr, layers1_2->GetPrevSibling());
  EXPECT_EQ(layers1_2, layers4_6_8->GetPrevSibling());
  EXPECT_EQ(nullptr, layer7->GetPrevSibling());
  EXPECT_EQ(layers4_6_8, layer9->GetPrevSibling());

  nsRefPtr<AsyncPanZoomController> hit = GetTargetAPZC(ScreenPoint(25, 25));
  EXPECT_EQ(ApzcOf(layers[1]), hit.get());
  hit = GetTargetAPZC(ScreenPoint(275, 375));
  EXPECT_EQ(ApzcOf(layers[9]), hit.get());
  hit = GetTargetAPZC(ScreenPoint(250, 100));
  EXPECT_EQ(ApzcOf(layers[7]), hit.get());
}

TEST_F(APZHitTestingTester, TestRepaintFlushOnNewInputBlock) {
  // The main purpose of this test is to verify that touch-start events (or anything
  // that starts a new input block) don't ever get untransformed. This should always
  // hold because the APZ code should flush repaints when we start a new input block
  // and the transform to gecko space should be empty.

  CreateSimpleScrollingLayer();
  ScopedLayerTreeRegistration registration(0, root, mcc);
  manager->UpdatePanZoomControllerTree(nullptr, root, false, 0, 0);
  AsyncPanZoomController* apzcroot = ApzcOf(root);

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

  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, manager->ReceiveInputEvent(mti, nullptr));
  EXPECT_EQ(touchPoint, mti.mTouches[0].mScreenPoint);
  check.Call("post-first-touch-start");

  // Send a touchend to clear state
  mti.mType = MultiTouchInput::MULTITOUCH_END;
  manager->ReceiveInputEvent(mti, nullptr);

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
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, manager->ReceiveInputEvent(mti, nullptr));
  EXPECT_EQ(touchPoint, mti.mTouches[0].mScreenPoint);
  check.Call("post-second-touch-start");

  mti.mType = MultiTouchInput::MULTITOUCH_END;
  EXPECT_EQ(nsEventStatus_eConsumeDoDefault, manager->ReceiveInputEvent(mti, nullptr));
  EXPECT_EQ(touchPoint, mti.mTouches[0].mScreenPoint);

  mcc->RunThroughDelayedTasks();
}

class APZOverscrollHandoffTester : public APZCTreeManagerTester {
protected:
  UniquePtr<ScopedLayerTreeRegistration> registration;
  TestAsyncPanZoomController* rootApzc;

  void SetScrollHandoff(Layer* aChild, Layer* aParent) {
    FrameMetrics metrics = aChild->GetFrameMetrics(0);
    metrics.SetScrollParentId(aParent->GetFrameMetrics(0).GetScrollId());
    aChild->SetFrameMetrics(metrics);
  }

  void CreateOverscrollHandoffLayerTree1() {
    const char* layerTreeSyntax = "c(c)";
    nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(nsIntRect(0, 0, 100, 100)),
      nsIntRegion(nsIntRect(0, 50, 100, 50))
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);
    SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID, CSSRect(0, 0, 200, 200));
    SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID + 1, CSSRect(0, 0, 100, 100));
    SetScrollHandoff(layers[1], root);
    registration = MakeUnique<ScopedLayerTreeRegistration>(0, root, mcc);
    manager->UpdatePanZoomControllerTree(nullptr, root, false, 0, 0);
    rootApzc = ApzcOf(root);
  }

  void CreateOverscrollHandoffLayerTree2() {
    const char* layerTreeSyntax = "c(c(c))";
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
    manager->UpdatePanZoomControllerTree(nullptr, root, false, 0, 0);
    rootApzc = ApzcOf(root);
  }

  void CreateOverscrollHandoffLayerTree3() {
    const char* layerTreeSyntax = "c(c(c)c(c))";
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
    manager->UpdatePanZoomControllerTree(nullptr, root, false, 0, 0);
  }

  void CreateScrollgrabLayerTree() {
    const char* layerTreeSyntax = "c(c)";
    nsIntRegion layerVisibleRegion[] = {
      nsIntRegion(nsIntRect(0, 0, 100, 100)),  // scroll-grabbing parent
      nsIntRegion(nsIntRect(0, 20, 100, 80))   // child
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, nullptr, lm, layers);
    SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID, CSSRect(0, 0, 100, 120));
    SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID + 1, CSSRect(0, 0, 100, 200));
    SetScrollHandoff(layers[1], root);
    registration = MakeUnique<ScopedLayerTreeRegistration>(0, root, mcc);
    manager->UpdatePanZoomControllerTree(nullptr, root, false, 0, 0);
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
  childApzc->GetFrameMetrics().mMayHaveTouchListeners = true;

  // Queue input events for a pan.
  int time = 0;
  ApzcPanNoFling(childApzc, time, 90, 30);

  // Allow the pan to be processed.
  childApzc->ContentReceivedTouch(false);

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
  childApzc->GetFrameMetrics().mMayHaveTouchListeners = true;

  // Queue input events for a pan.
  int time = 0;
  ApzcPanNoFling(childApzc, time, 90, 30);

  // Modify the APZC tree to insert a new APZC 'middle' into the handoff chain
  // between the child and the root.
  CreateOverscrollHandoffLayerTree2();
  nsRefPtr<Layer> middle = layers[1];
  childApzc->GetFrameMetrics().mMayHaveTouchListeners = true;
  TestAsyncPanZoomController* middleApzc = ApzcOf(middle);

  // Queue input events for another pan.
  ApzcPanNoFling(childApzc, time, 30, 90);

  // Allow the first pan to be processed.
  childApzc->ContentReceivedTouch(false);

  // Make sure things have scrolled according to the handoff chain in
  // place at the time the touch-start of the first pan was queued.
  EXPECT_EQ(50, childApzc->GetFrameMetrics().GetScrollOffset().y);
  EXPECT_EQ(10, rootApzc->GetFrameMetrics().GetScrollOffset().y);
  EXPECT_EQ(0, middleApzc->GetFrameMetrics().GetScrollOffset().y);

  // Allow the second pan to be processed.
  childApzc->ContentReceivedTouch(false);

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
  ApzctmPan(manager, time, 10, 40, true /* keep finger down */);
  EXPECT_FALSE(child->IsOverscrolled());
  EXPECT_TRUE(rootApzc->IsOverscrolled());

  // Put a second finger down.
  MultiTouchInput secondFingerDown(MultiTouchInput::MULTITOUCH_START, 0, TimeStamp(), 0);
  // Use the same touch identifier for the first touch (0) as ApzctmPan(). (A bit hacky.)
  secondFingerDown.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(10, 40), ScreenSize(0, 0), 0, 0));
  secondFingerDown.mTouches.AppendElement(SingleTouchData(1, ScreenIntPoint(30, 20), ScreenSize(0, 0), 0, 0));
  manager->ReceiveInputEvent(secondFingerDown, nullptr);

  // Release the fingers.
  MultiTouchInput fingersUp = secondFingerDown;
  fingersUp.mType = MultiTouchInput::MULTITOUCH_END;
  manager->ReceiveInputEvent(fingersUp, nullptr);

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

  TestAsyncPanZoomController* parent1 = ApzcOf(layers[1]);
  TestAsyncPanZoomController* child1 = ApzcOf(layers[2]);
  TestAsyncPanZoomController* parent2 = ApzcOf(layers[3]);
  TestAsyncPanZoomController* child2 = ApzcOf(layers[4]);

  // Pan on the lower child.
  int time = 0;
  ApzcPan(child2, time, 45, 5);

  // Pan on the upper child.
  ApzcPan(child1, time, 95, 55);

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

  TestAsyncPanZoomController* childApzc = ApzcOf(layers[1]);

  // Pan on the child, enough to fully scroll the scrollgrab parent (20 px)
  // and leave some more (another 15 px) for the child.
  int time = 0;
  ApzcPan(childApzc, time, 80, 45);

  // Check that the parent and child have scrolled as much as we expect.
  EXPECT_EQ(20, rootApzc->GetFrameMetrics().GetScrollOffset().y);
  EXPECT_EQ(15, childApzc->GetFrameMetrics().GetScrollOffset().y);
}

TEST_F(APZOverscrollHandoffTester, ScrollgrabFling) {
  // Set up the layer tree
  CreateScrollgrabLayerTree();

  TestAsyncPanZoomController* childApzc = ApzcOf(layers[1]);

  // Pan on the child, not enough to fully scroll the scrollgrab parent.
  int time = 0;
  ApzcPan(childApzc, time, 80, 70);

  // Check that it is the scrollgrab parent that's in a fling, not the child.
  rootApzc->AssertStateIsFling();
  childApzc->AssertStateIsReset();
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
