/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mozilla/Attributes.h"
#include "mozilla/layers/AsyncCompositionManager.h" // for ViewTransform
#include "mozilla/layers/AsyncPanZoomController.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/GeckoContentController.h"
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/APZCTreeManager.h"
#include "mozilla/Preferences.h"
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

class AsyncPanZoomControllerTester : public ::testing::Test {
protected:
  virtual void SetUp() {
    gfxPrefs::GetSingleton();
    AsyncPanZoomController::SetThreadAssertionsEnabled(false);
  }
};

class APZCTreeManagerTester : public ::testing::Test {
protected:
  virtual void SetUp() {
    gfxPrefs::GetSingleton();
    AsyncPanZoomController::SetThreadAssertionsEnabled(false);
  }
};

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
};

class TestScopedBoolPref {
public:
  TestScopedBoolPref(const char* aPref, bool aVal)
    : mPref(aPref)
  {
    mOldVal = Preferences::GetBool(aPref);
    Preferences::SetBool(aPref, aVal);
  }

  ~TestScopedBoolPref() {
    Preferences::SetBool(mPref, mOldVal);
  }

private:
  const char* mPref;
  bool mOldVal;
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


class TestAPZCContainerLayer : public ContainerLayer {
  public:
    TestAPZCContainerLayer()
      : ContainerLayer(nullptr, nullptr)
    {}
  bool RemoveChild(Layer* aChild) { return true; }
  bool InsertAfter(Layer* aChild, Layer* aAfter) { return true; }
  void ComputeEffectiveTransforms(const Matrix4x4& aTransformToSurface) {}
  bool RepositionChild(Layer* aChild, Layer* aAfter) { return true; }
};

class TestAsyncPanZoomController : public AsyncPanZoomController {
public:
  TestAsyncPanZoomController(uint64_t aLayersId, MockContentController* aMcc,
                             APZCTreeManager* aTreeManager = nullptr,
                             GestureBehavior aBehavior = DEFAULT_GESTURES)
    : AsyncPanZoomController(aLayersId, aTreeManager, aMcc, aBehavior)
  {}

  // Since touch-action-enabled property is global - setting it for each test
  // separately isn't safe from the concurrency point of view. To make tests
  // run concurrent and independent from each other we have a member variable
  // mTouchActionEnabled for each apzc and setter defined here.
  void SetTouchActionEnabled(const bool touchActionEnabled) {
    ReentrantMonitorAutoEnter lock(mMonitor);
    mTouchActionPropertyEnabled = touchActionEnabled;
  }

  void SetFrameMetrics(const FrameMetrics& metrics) {
    ReentrantMonitorAutoEnter lock(mMonitor);
    mFrameMetrics = metrics;
  }

  FrameMetrics GetFrameMetrics() {
    ReentrantMonitorAutoEnter lock(mMonitor);
    return mFrameMetrics;
  }

  void AssertStateIsReset() {
    ReentrantMonitorAutoEnter lock(mMonitor);
    EXPECT_EQ(NOTHING, mState);
  }
};

class TestAPZCTreeManager : public APZCTreeManager {
public:
  // Expose these so test code can call it directly.
  void BuildOverscrollHandoffChain(AsyncPanZoomController* aApzc) {
    APZCTreeManager::BuildOverscrollHandoffChain(aApzc);
  }
  void ClearOverscrollHandoffChain() {
    APZCTreeManager::ClearOverscrollHandoffChain();
  }
};

static
FrameMetrics TestFrameMetrics() {
  FrameMetrics fm;

  fm.mDisplayPort = CSSRect(0, 0, 10, 10);
  fm.mCompositionBounds = ParentLayerIntRect(0, 0, 10, 10);
  fm.mCriticalDisplayPort = CSSRect(0, 0, 10, 10);
  fm.mScrollableRect = CSSRect(0, 0, 100, 100);
  fm.mViewport = CSSRect(0, 0, 10, 10);

  return fm;
}

/*
 * Dispatches mock touch events to the apzc and checks whether apzc properly
 * consumed them and triggered scrolling behavior.
 */
static
void ApzcPan(AsyncPanZoomController* apzc,
             TestAPZCTreeManager* aTreeManager,
             int& aTime,
             int aTouchStartY,
             int aTouchEndY,
             bool expectIgnoredPan = false,
             bool hasTouchListeners = false,
             nsTArray<uint32_t>* aAllowedTouchBehaviors = nullptr) {

  const int TIME_BETWEEN_TOUCH_EVENT = 100;
  const int OVERCOME_TOUCH_TOLERANCE = 100;
  MultiTouchInput mti;
  nsEventStatus status;

  // Since we're passing inputs directly to the APZC instead of going through
  // the tree manager, we need to build the overscroll handoff chain explicitly
  // for panning to work correctly.
  aTreeManager->BuildOverscrollHandoffChain(apzc);

  nsEventStatus touchStartStatus;
  if (hasTouchListeners) {
    // APZC shouldn't consume the start event now, instead queueing it up
    // waiting for content's response.
    touchStartStatus = nsEventStatus_eIgnore;
  } else {
    // APZC should go into the touching state and therefore consume the event.
    touchStartStatus = nsEventStatus_eConsumeNoDefault;
  }

  mti =
    MultiTouchInput(MultiTouchInput::MULTITOUCH_START, aTime, TimeStamp(), 0);
  aTime += TIME_BETWEEN_TOUCH_EVENT;
  // Make sure the move is large enough to not be handled as a tap
  mti.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(10, aTouchStartY+OVERCOME_TOUCH_TOLERANCE), ScreenSize(0, 0), 0, 0));
  status = apzc->ReceiveInputEvent(mti);
  EXPECT_EQ(touchStartStatus, status);
  // APZC should be in TOUCHING state

  // Allowed touch behaviours must be set after sending touch-start.
  if (aAllowedTouchBehaviors) {
    apzc->SetAllowedTouchBehavior(*aAllowedTouchBehaviors);
  }

  nsEventStatus touchMoveStatus;
  if (expectIgnoredPan) {
    // APZC should ignore panning, be in TOUCHING state and therefore return eIgnore.
    // The same applies to all consequent touch move events.
    touchMoveStatus = nsEventStatus_eIgnore;
  } else {
    // APZC should go into the panning state and therefore consume the event.
    touchMoveStatus = nsEventStatus_eConsumeNoDefault;
  }

  mti =
    MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, aTime, TimeStamp(), 0);
  aTime += TIME_BETWEEN_TOUCH_EVENT;
  mti.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(10, aTouchStartY), ScreenSize(0, 0), 0, 0));
  status = apzc->ReceiveInputEvent(mti);
  EXPECT_EQ(touchMoveStatus, status);

  mti =
    MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, aTime, TimeStamp(), 0);
  aTime += TIME_BETWEEN_TOUCH_EVENT;
  mti.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(10, aTouchEndY), ScreenSize(0, 0), 0, 0));
  status = apzc->ReceiveInputEvent(mti);
  EXPECT_EQ(touchMoveStatus, status);

  mti =
    MultiTouchInput(MultiTouchInput::MULTITOUCH_END, aTime, TimeStamp(), 0);
  aTime += TIME_BETWEEN_TOUCH_EVENT;
  mti.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(10, aTouchEndY), ScreenSize(0, 0), 0, 0));
  status = apzc->ReceiveInputEvent(mti);

  // Since we've explicitly built the overscroll handoff chain before
  // touch-start, we need to explicitly clear it after touch-end.
  aTreeManager->ClearOverscrollHandoffChain();
}

static
void DoPanTest(bool aShouldTriggerScroll, bool aShouldUseTouchAction, uint32_t aBehavior)
{
  TimeStamp testStartTime = TimeStamp::Now();
  AsyncPanZoomController::SetFrameTime(testStartTime);

  nsRefPtr<MockContentController> mcc = new NiceMock<MockContentController>();
  nsRefPtr<TestAPZCTreeManager> tm = new TestAPZCTreeManager();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(0, mcc, tm);

  apzc->SetTouchActionEnabled(aShouldUseTouchAction);
  apzc->SetFrameMetrics(TestFrameMetrics());
  apzc->NotifyLayersUpdated(TestFrameMetrics(), true);

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
  ApzcPan(apzc, tm, time, touchStart, touchEnd, !aShouldTriggerScroll, false, &allowedTouchBehaviors);
  apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);

  if (aShouldTriggerScroll) {
    EXPECT_EQ(ScreenPoint(0, -(touchEnd-touchStart)), pointOut);
    EXPECT_NE(ViewTransform(), viewTransformOut);
  } else {
    EXPECT_EQ(ScreenPoint(), pointOut);
    EXPECT_EQ(ViewTransform(), viewTransformOut);
  }

  // Pan back
  ApzcPan(apzc, tm, time, touchEnd, touchStart, !aShouldTriggerScroll, false, &allowedTouchBehaviors);
  apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);

  EXPECT_EQ(ScreenPoint(), pointOut);
  EXPECT_EQ(ViewTransform(), viewTransformOut);

  apzc->Destroy();
}

static void ApzcPinchWithPinchInput(AsyncPanZoomController* aApzc,
                                    int aFocusX,
                                    int aFocusY,
                                    float aScale,
                                    bool aShouldTriggerPinch,
                                    nsTArray<uint32_t>* aAllowedTouchBehaviors = nullptr) {
  if (aAllowedTouchBehaviors) {
    aApzc->SetAllowedTouchBehavior(*aAllowedTouchBehaviors);
  }

  nsEventStatus expectedStatus = aShouldTriggerPinch
    ? nsEventStatus_eConsumeNoDefault
    : nsEventStatus_eIgnore;
  nsEventStatus actualStatus;

  actualStatus = aApzc->HandleGestureEvent(PinchGestureInput(PinchGestureInput::PINCHGESTURE_START,
                                            0,
                                            TimeStamp(),
                                            ScreenPoint(aFocusX, aFocusY),
                                            10.0,
                                            10.0,
                                            0));
  EXPECT_EQ(actualStatus, expectedStatus);
  actualStatus = aApzc->HandleGestureEvent(PinchGestureInput(PinchGestureInput::PINCHGESTURE_SCALE,
                                            0,
                                            TimeStamp(),
                                            ScreenPoint(aFocusX, aFocusY),
                                            10.0 * aScale,
                                            10.0,
                                            0));
  EXPECT_EQ(actualStatus, expectedStatus);
  aApzc->HandleGestureEvent(PinchGestureInput(PinchGestureInput::PINCHGESTURE_END,
                                            0,
                                            TimeStamp(),
                                            ScreenPoint(aFocusX, aFocusY),
                                            // note: negative values here tell APZC
                                            //       not to turn the pinch into a pan
                                            -1.0,
                                            -1.0,
                                            0));
}

static void ApzcPinchWithTouchMoveInput(AsyncPanZoomController* aApzc,
                                        int aFocusX,
                                        int aFocusY,
                                        float aScale,
                                        int& inputId,
                                        bool aShouldTriggerPinch,
                                        nsTArray<uint32_t>* aAllowedTouchBehaviors = nullptr) {
  // Having pinch coordinates in float type may cause problems with high-precision scale values
  // since SingleTouchData accepts integer value. But for trivial tests it should be ok.
  float pinchLength = 100.0;
  float pinchLengthScaled = pinchLength * aScale;

  nsEventStatus expectedStatus = aShouldTriggerPinch
    ? nsEventStatus_eConsumeNoDefault
    : nsEventStatus_eIgnore;
  nsEventStatus actualStatus;

  MultiTouchInput mtiStart =
    MultiTouchInput(MultiTouchInput::MULTITOUCH_START, 0, TimeStamp(), 0);
  mtiStart.mTouches.AppendElement(SingleTouchData(inputId, ScreenIntPoint(aFocusX, aFocusY), ScreenSize(0, 0), 0, 0));
  mtiStart.mTouches.AppendElement(SingleTouchData(inputId + 1, ScreenIntPoint(aFocusX, aFocusY), ScreenSize(0, 0), 0, 0));
  aApzc->ReceiveInputEvent(mtiStart);

  if (aAllowedTouchBehaviors) {
    aApzc->SetAllowedTouchBehavior(*aAllowedTouchBehaviors);
  }

  MultiTouchInput mtiMove1 =
    MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, 0, TimeStamp(), 0);
  mtiMove1.mTouches.AppendElement(SingleTouchData(inputId, ScreenIntPoint(aFocusX - pinchLength, aFocusY), ScreenSize(0, 0), 0, 0));
  mtiMove1.mTouches.AppendElement(SingleTouchData(inputId + 1, ScreenIntPoint(aFocusX + pinchLength, aFocusY), ScreenSize(0, 0), 0, 0));
  actualStatus = aApzc->ReceiveInputEvent(mtiMove1);
  EXPECT_EQ(actualStatus, expectedStatus);

  MultiTouchInput mtiMove2 =
    MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, 0, TimeStamp(), 0);
  mtiMove2.mTouches.AppendElement(SingleTouchData(inputId, ScreenIntPoint(aFocusX - pinchLengthScaled, aFocusY), ScreenSize(0, 0), 0, 0));
  mtiMove2.mTouches.AppendElement(SingleTouchData(inputId + 1, ScreenIntPoint(aFocusX + pinchLengthScaled, aFocusY), ScreenSize(0, 0), 0, 0));
  actualStatus = aApzc->ReceiveInputEvent(mtiMove2);
  EXPECT_EQ(actualStatus, expectedStatus);

  MultiTouchInput mtiEnd =
    MultiTouchInput(MultiTouchInput::MULTITOUCH_END, 0, TimeStamp(), 0);
  mtiEnd.mTouches.AppendElement(SingleTouchData(inputId, ScreenIntPoint(aFocusX - pinchLengthScaled, aFocusY), ScreenSize(0, 0), 0, 0));
  mtiEnd.mTouches.AppendElement(SingleTouchData(inputId + 1, ScreenIntPoint(aFocusX + pinchLengthScaled, aFocusY), ScreenSize(0, 0), 0, 0));
  aApzc->ReceiveInputEvent(mtiEnd);
  inputId += 2;
}

static
void DoPinchTest(bool aUseGestureRecognizer, bool aShouldTriggerPinch,
                 nsTArray<uint32_t> *aAllowedTouchBehaviors = nullptr) {
  nsRefPtr<MockContentController> mcc = new NiceMock<MockContentController>();
  nsRefPtr<TestAPZCTreeManager> tm = new TestAPZCTreeManager();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(0, mcc, tm,
    aUseGestureRecognizer
      ? AsyncPanZoomController::USE_GESTURE_DETECTOR
      : AsyncPanZoomController::DEFAULT_GESTURES);

  FrameMetrics fm;
  fm.mViewport = CSSRect(0, 0, 980, 480);
  fm.mCompositionBounds = ParentLayerIntRect(200, 200, 100, 200);
  fm.mScrollableRect = CSSRect(0, 0, 980, 1000);
  fm.SetScrollOffset(CSSPoint(300, 300));
  fm.SetZoom(CSSToScreenScale(2.0));
  apzc->SetFrameMetrics(fm);
  apzc->UpdateZoomConstraints(ZoomConstraints(true, true, CSSToScreenScale(0.25), CSSToScreenScale(4.0)));
  // the visible area of the document in CSS pixels is x=300 y=300 w=50 h=100

  if (aShouldTriggerPinch) {
    EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_,_)).Times(AtLeast(1));
    EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);
  } else {
    EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_,_)).Times(AtMost(2));
    EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(0);
  }

  if (aAllowedTouchBehaviors) {
    apzc->SetTouchActionEnabled(true);
  } else {
    apzc->SetTouchActionEnabled(false);
  }

  int touchInputId = 0;
  if (aUseGestureRecognizer) {
    ApzcPinchWithTouchMoveInput(apzc, 250, 300, 1.25, touchInputId, aShouldTriggerPinch, aAllowedTouchBehaviors);
  } else {
    ApzcPinchWithPinchInput(apzc, 250, 300, 1.25, aShouldTriggerPinch, aAllowedTouchBehaviors);
  }

  fm = apzc->GetFrameMetrics();

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

  if (aUseGestureRecognizer) {
    ApzcPinchWithTouchMoveInput(apzc, 250, 300, 0.5, touchInputId, aShouldTriggerPinch, aAllowedTouchBehaviors);
  } else {
    ApzcPinchWithPinchInput(apzc, 250, 300, 0.5, aShouldTriggerPinch, aAllowedTouchBehaviors);
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

  apzc->Destroy();
}

static nsEventStatus
ApzcDown(AsyncPanZoomController* apzc, int aX, int aY, int& aTime) {
  MultiTouchInput mti =
    MultiTouchInput(MultiTouchInput::MULTITOUCH_START, aTime, TimeStamp(), 0);
  mti.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(aX, aY), ScreenSize(0, 0), 0, 0));
  return apzc->ReceiveInputEvent(mti);
}

static nsEventStatus
ApzcUp(AsyncPanZoomController* apzc, int aX, int aY, int& aTime) {
  MultiTouchInput mti =
    MultiTouchInput(MultiTouchInput::MULTITOUCH_END, aTime, TimeStamp(), 0);
  mti.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(aX, aY), ScreenSize(0, 0), 0, 0));
  return apzc->ReceiveInputEvent(mti);
}

static nsEventStatus
ApzcTap(AsyncPanZoomController* apzc, int aX, int aY, int& aTime, int aTapLength, MockContentControllerDelayed* mcc = nullptr) {
  nsEventStatus status = ApzcDown(apzc, aX, aY, aTime);
  if (mcc != nullptr) {
    // There will be delayed tasks posted for the long-tap and MAX_TAP timeouts, but
    // if we were provided a non-null mcc we want to clear them.
    mcc->CheckHasDelayedTask();
    mcc->ClearDelayedTask();
    mcc->CheckHasDelayedTask();
    mcc->ClearDelayedTask();
  }
  EXPECT_EQ(nsEventStatus_eConsumeNoDefault, status);
  aTime += aTapLength;
  return ApzcUp(apzc, aX, aY, aTime);
}

TEST_F(AsyncPanZoomControllerTester, Constructor) {
  // RefCounted class can't live in the stack
  nsRefPtr<MockContentController> mcc = new NiceMock<MockContentController>();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(0, mcc);
  apzc->SetFrameMetrics(TestFrameMetrics());
}

TEST_F(AsyncPanZoomControllerTester, Pinch_DefaultGestures_NoTouchAction) {
  DoPinchTest(false, true);
}

TEST_F(AsyncPanZoomControllerTester, Pinch_DefaultGestures_TouchActionNone) {
  nsTArray<uint32_t> behaviors;
  behaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::NONE);
  behaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::NONE);
  DoPinchTest(false, false, &behaviors);
}

TEST_F(AsyncPanZoomControllerTester, Pinch_DefaultGestures_TouchActionZoom) {
  nsTArray<uint32_t> behaviors;
  behaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::PINCH_ZOOM);
  behaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::PINCH_ZOOM);
  DoPinchTest(false, true, &behaviors);
}

TEST_F(AsyncPanZoomControllerTester, Pinch_DefaultGestures_TouchActionNotAllowZoom) {
  nsTArray<uint32_t> behaviors;
  behaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN);
  behaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::PINCH_ZOOM);
  DoPinchTest(false, false, &behaviors);
}

TEST_F(AsyncPanZoomControllerTester, Pinch_UseGestureDetector_NoTouchAction) {
  DoPinchTest(true, true);
}

TEST_F(AsyncPanZoomControllerTester, Pinch_UseGestureDetector_TouchActionNone) {
  nsTArray<uint32_t> behaviors;
  behaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::NONE);
  behaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::NONE);
  DoPinchTest(true, false, &behaviors);
}

TEST_F(AsyncPanZoomControllerTester, Pinch_UseGestureDetector_TouchActionZoom) {
  nsTArray<uint32_t> behaviors;
  behaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::PINCH_ZOOM);
  behaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::PINCH_ZOOM);
  DoPinchTest(true, true, &behaviors);
}

TEST_F(AsyncPanZoomControllerTester, Pinch_UseGestureDetector_TouchActionNotAllowZoom) {
  nsTArray<uint32_t> behaviors;
  behaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN);
  behaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::PINCH_ZOOM);
  DoPinchTest(true, false, &behaviors);
}

TEST_F(AsyncPanZoomControllerTester, Overzoom) {
  nsRefPtr<MockContentController> mcc = new NiceMock<MockContentController>();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(0, mcc);

  FrameMetrics fm;
  fm.mViewport = CSSRect(0, 0, 100, 100);
  fm.mCompositionBounds = ParentLayerIntRect(0, 0, 100, 100);
  fm.mScrollableRect = CSSRect(0, 0, 125, 150);
  fm.SetScrollOffset(CSSPoint(10, 0));
  fm.SetZoom(CSSToScreenScale(1.0));
  apzc->SetFrameMetrics(fm);
  apzc->UpdateZoomConstraints(ZoomConstraints(true, true, CSSToScreenScale(0.25), CSSToScreenScale(4.0)));
  // the visible area of the document in CSS pixels is x=10 y=0 w=100 h=100

  EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_,_)).Times(AtLeast(1));
  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);

  ApzcPinchWithPinchInput(apzc, 50, 50, 0.5, true);

  fm = apzc->GetFrameMetrics();
  EXPECT_EQ(0.8f, fm.GetZoom().scale);
  // bug 936721 - PGO builds introduce rounding error so
  // use a fuzzy match instead
  EXPECT_LT(abs(fm.GetScrollOffset().x), 1e-5);
  EXPECT_LT(abs(fm.GetScrollOffset().y), 1e-5);
}

TEST_F(AsyncPanZoomControllerTester, SimpleTransform) {
  TimeStamp testStartTime = TimeStamp::Now();
  // RefCounted class can't live in the stack
  nsRefPtr<MockContentController> mcc = new NiceMock<MockContentController>();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(0, mcc);
  apzc->SetFrameMetrics(TestFrameMetrics());

  ScreenPoint pointOut;
  ViewTransform viewTransformOut;
  apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);

  EXPECT_EQ(ScreenPoint(), pointOut);
  EXPECT_EQ(ViewTransform(), viewTransformOut);
}


TEST_F(AsyncPanZoomControllerTester, ComplexTransform) {
  TimeStamp testStartTime = TimeStamp::Now();
  AsyncPanZoomController::SetFrameTime(testStartTime);

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

  nsRefPtr<MockContentController> mcc = new NiceMock<MockContentController>();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(0, mcc);
  nsRefPtr<TestAsyncPanZoomController> childApzc = new TestAsyncPanZoomController(0, mcc);

  const char* layerTreeSyntax = "c(c)";
  // LayerID                     0 1
  nsIntRegion layerVisibleRegion[] = {
    nsIntRegion(nsIntRect(0, 0, 300, 300)),
    nsIntRegion(nsIntRect(0, 0, 150, 300)),
  };
  gfx3DMatrix transforms[] = {
    gfx3DMatrix(),
    gfx3DMatrix(),
  };
  transforms[0].ScalePost(0.5f, 0.5f, 1.0f); // this results from the 2.0 resolution on the root layer
  transforms[1].ScalePost(2.0f, 1.0f, 1.0f); // this is the 2.0 x-axis CSS transform on the child layer

  nsTArray<nsRefPtr<Layer> > layers;
  nsRefPtr<LayerManager> lm;
  nsRefPtr<Layer> root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, transforms, lm, layers);

  FrameMetrics metrics;
  metrics.mCompositionBounds = ParentLayerIntRect(0, 0, 24, 24);
  metrics.mDisplayPort = CSSRect(-1, -1, 6, 6);
  metrics.mViewport = CSSRect(0, 0, 4, 4);
  metrics.SetScrollOffset(CSSPoint(10, 10));
  metrics.mScrollableRect = CSSRect(0, 0, 50, 50);
  metrics.mCumulativeResolution = LayoutDeviceToLayerScale(2);
  metrics.mResolution = ParentLayerToLayerScale(2);
  metrics.SetZoom(CSSToScreenScale(6));
  metrics.mDevPixelsPerCSSPixel = CSSToLayoutDeviceScale(3);
  metrics.SetScrollId(FrameMetrics::START_SCROLL_ID);

  FrameMetrics childMetrics = metrics;
  childMetrics.SetScrollId(FrameMetrics::START_SCROLL_ID + 1);

  layers[0]->AsContainerLayer()->SetFrameMetrics(metrics);
  layers[1]->AsContainerLayer()->SetFrameMetrics(childMetrics);

  ScreenPoint pointOut;
  ViewTransform viewTransformOut;

  // Both the parent and child layer should behave exactly the same here, because
  // the CSS transform on the child layer does not affect the SampleContentTransformForFrame code

  // initial transform
  apzc->SetFrameMetrics(metrics);
  apzc->NotifyLayersUpdated(metrics, true);
  apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(LayerPoint(), ParentLayerToScreenScale(2)), viewTransformOut);
  EXPECT_EQ(ScreenPoint(60, 60), pointOut);

  childApzc->SetFrameMetrics(childMetrics);
  childApzc->NotifyLayersUpdated(childMetrics, true);
  childApzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(LayerPoint(), ParentLayerToScreenScale(2)), viewTransformOut);
  EXPECT_EQ(ScreenPoint(60, 60), pointOut);

  // do an async scroll by 5 pixels and check the transform
  metrics.ScrollBy(CSSPoint(5, 0));
  apzc->SetFrameMetrics(metrics);
  apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(LayerPoint(-30, 0), ParentLayerToScreenScale(2)), viewTransformOut);
  EXPECT_EQ(ScreenPoint(90, 60), pointOut);

  childMetrics.ScrollBy(CSSPoint(5, 0));
  childApzc->SetFrameMetrics(childMetrics);
  childApzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(LayerPoint(-30, 0), ParentLayerToScreenScale(2)), viewTransformOut);
  EXPECT_EQ(ScreenPoint(90, 60), pointOut);

  // do an async zoom of 1.5x and check the transform
  metrics.ZoomBy(1.5f);
  apzc->SetFrameMetrics(metrics);
  apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(LayerPoint(-30, 0), ParentLayerToScreenScale(3)), viewTransformOut);
  EXPECT_EQ(ScreenPoint(135, 90), pointOut);

  childMetrics.ZoomBy(1.5f);
  childApzc->SetFrameMetrics(childMetrics);
  childApzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(LayerPoint(-30, 0), ParentLayerToScreenScale(3)), viewTransformOut);
  EXPECT_EQ(ScreenPoint(135, 90), pointOut);
}

TEST_F(AsyncPanZoomControllerTester, Pan) {
  DoPanTest(true, false, mozilla::layers::AllowedTouchBehavior::NONE);
}

// In the each of the following 4 pan tests we are performing two pan gestures: vertical pan from top
// to bottom and back - from bottom to top.
// According to the pointer-events/touch-action spec AUTO and PAN_Y touch-action values allow vertical
// scrolling while NONE and PAN_X forbid it. The first parameter of DoPanTest method specifies this
// behavior.
TEST_F(AsyncPanZoomControllerTester, PanWithTouchActionAuto) {
  DoPanTest(true, true,
            mozilla::layers::AllowedTouchBehavior::HORIZONTAL_PAN | mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN);
}

TEST_F(AsyncPanZoomControllerTester, PanWithTouchActionNone) {
  DoPanTest(false, true, 0);
}

TEST_F(AsyncPanZoomControllerTester, PanWithTouchActionPanX) {
  DoPanTest(false, true, mozilla::layers::AllowedTouchBehavior::HORIZONTAL_PAN);
}

TEST_F(AsyncPanZoomControllerTester, PanWithTouchActionPanY) {
  DoPanTest(true, true, mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN);
}

TEST_F(AsyncPanZoomControllerTester, PanWithPreventDefault) {
  TimeStamp testStartTime = TimeStamp::Now();
  AsyncPanZoomController::SetFrameTime(testStartTime);

  nsRefPtr<MockContentController> mcc = new NiceMock<MockContentController>();
  nsRefPtr<TestAPZCTreeManager> tm = new TestAPZCTreeManager();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(0, mcc, tm);

  FrameMetrics frameMetrics(TestFrameMetrics());
  frameMetrics.mMayHaveTouchListeners = true;

  apzc->SetFrameMetrics(frameMetrics);
  apzc->NotifyLayersUpdated(frameMetrics, true);

  int time = 0;
  int touchStart = 50;
  int touchEnd = 10;
  ScreenPoint pointOut;
  ViewTransform viewTransformOut;

  // Pan down
  nsTArray<uint32_t> allowedTouchBehaviors;
  allowedTouchBehaviors.AppendElement(mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN);
  apzc->SetTouchActionEnabled(true);
  ApzcPan(apzc, tm, time, touchStart, touchEnd, true, true, &allowedTouchBehaviors);

  // Send the signal that content has handled and preventDefaulted the touch
  // events. This flushes the event queue.
  apzc->ContentReceivedTouch(true);

  apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(ScreenPoint(), pointOut);
  EXPECT_EQ(ViewTransform(), viewTransformOut);

  apzc->Destroy();
}

TEST_F(AsyncPanZoomControllerTester, Fling) {
  TimeStamp testStartTime = TimeStamp::Now();
  AsyncPanZoomController::SetFrameTime(testStartTime);

  nsRefPtr<MockContentController> mcc = new NiceMock<MockContentController>();
  nsRefPtr<TestAPZCTreeManager> tm = new TestAPZCTreeManager();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(0, mcc, tm);

  apzc->SetFrameMetrics(TestFrameMetrics());
  apzc->NotifyLayersUpdated(TestFrameMetrics(), true);

  EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_,_)).Times(AtLeast(1));
  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);

  int time = 0;
  int touchStart = 50;
  int touchEnd = 10;
  ScreenPoint pointOut;
  ViewTransform viewTransformOut;

  // Fling down. Each step scroll further down
  ApzcPan(apzc, tm, time, touchStart, touchEnd);
  ScreenPoint lastPoint;
  for (int i = 1; i < 50; i+=1) {
    apzc->SampleContentTransformForFrame(testStartTime+TimeDuration::FromMilliseconds(i), &viewTransformOut, pointOut);
    EXPECT_GT(pointOut.y, lastPoint.y);
    lastPoint = pointOut;
  }

  apzc->Destroy();
}

// Start a fling, and then tap while the fling is ongoing. When
// aSlow is false, the tap will happen while the fling is at a
// high velocity, and we check that the tap doesn't trigger sending a tap
// to content. If aSlow is true, the tap will happen while the fling
// is at a slow velocity, and we check that the tap does trigger sending
// a tap to content. See bug 1022956.
void
DoFlingStopTest(bool aSlow) {
  TimeStamp testStartTime = TimeStamp::Now();
  AsyncPanZoomController::SetFrameTime(testStartTime);

  nsRefPtr<MockContentControllerDelayed> mcc = new NiceMock<MockContentControllerDelayed>();
  nsRefPtr<TestAPZCTreeManager> tm = new TestAPZCTreeManager();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(0, mcc, tm, AsyncPanZoomController::USE_GESTURE_DETECTOR);

  apzc->SetFrameMetrics(TestFrameMetrics());
  apzc->NotifyLayersUpdated(TestFrameMetrics(), true);

  int time = 0;
  int touchStart = 50;
  int touchEnd = 10;

  // Start the fling down.
  ApzcPan(apzc, tm, time, touchStart, touchEnd);
  // The touchstart from the pan will leave some cancelled tasks in the queue, clear them out
  EXPECT_EQ(2, mcc->RunThroughDelayedTasks());

  // If we want to tap while the fling is fast, let the fling advance for 10ms only. If we want
  // the fling to slow down more, advance to 2000ms. These numbers may need adjusting if our
  // friction and threshold values change, but they should be deterministic at least.
  int timeDelta = aSlow ? 2000 : 10;
  int tapCallsExpected = aSlow ? 1 : 0;
  int delayedTasksExpected = aSlow ? 3 : 2;

  // Advance the fling animation by timeDelta milliseconds.
  ScreenPoint pointOut;
  ViewTransform viewTransformOut;
  apzc->SampleContentTransformForFrame(testStartTime + TimeDuration::FromMilliseconds(timeDelta), &viewTransformOut, pointOut);

  // Deliver a tap to abort the fling. Ensure that we get a HandleSingleTap
  // call out of it if and only if the fling is slow.
  EXPECT_CALL(*mcc, HandleSingleTap(_, 0, apzc->GetGuid())).Times(tapCallsExpected);
  ApzcTap(apzc, 10, 10, time, 0, nullptr);
  EXPECT_EQ(delayedTasksExpected, mcc->RunThroughDelayedTasks());

  // Verify that we didn't advance any further after the fling was aborted, in either case.
  ScreenPoint finalPointOut;
  apzc->SampleContentTransformForFrame(testStartTime + TimeDuration::FromMilliseconds(timeDelta + 1000), &viewTransformOut, finalPointOut);
  EXPECT_EQ(pointOut.x, finalPointOut.x);
  EXPECT_EQ(pointOut.y, finalPointOut.y);

  apzc->AssertStateIsReset();
  apzc->Destroy();
}

TEST_F(AsyncPanZoomControllerTester, FlingStop) {
  DoFlingStopTest(false);
}

TEST_F(AsyncPanZoomControllerTester, FlingStopTap) {
  DoFlingStopTest(true);
}

TEST_F(AsyncPanZoomControllerTester, OverScrollPanning) {
  TestScopedBoolPref overscrollEnabledPref("apz.overscroll.enabled", true);

  TimeStamp testStartTime = TimeStamp::Now();
  AsyncPanZoomController::SetFrameTime(testStartTime);

  nsRefPtr<MockContentController> mcc = new NiceMock<MockContentController>();
  nsRefPtr<TestAPZCTreeManager> tm = new TestAPZCTreeManager();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(0, mcc, tm);

  apzc->SetFrameMetrics(TestFrameMetrics());
  apzc->NotifyLayersUpdated(TestFrameMetrics(), true);

  // Pan sufficiently to hit overscroll behavior
  int time = 0;
  int touchStart = 500;
  int touchEnd = 10;
  ApzcPan(apzc, tm, time, touchStart, touchEnd);
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
  apzc->Destroy();
}

TEST_F(AsyncPanZoomControllerTester, OverScrollAbort) {
  TestScopedBoolPref overscrollEnabledPref("apz.overscroll.enabled", true);

  TimeStamp testStartTime = TimeStamp::Now();
  AsyncPanZoomController::SetFrameTime(testStartTime);

  nsRefPtr<MockContentController> mcc = new NiceMock<MockContentController>();
  nsRefPtr<TestAPZCTreeManager> tm = new TestAPZCTreeManager();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(0, mcc, tm);

  apzc->SetFrameMetrics(TestFrameMetrics());
  apzc->NotifyLayersUpdated(TestFrameMetrics(), true);

  // Pan sufficiently to hit overscroll behavior
  int time = 0;
  int touchStart = 500;
  int touchEnd = 10;
  ApzcPan(apzc, tm, time, touchStart, touchEnd);
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

  apzc->Destroy();
}

TEST_F(AsyncPanZoomControllerTester, ShortPress) {
  nsRefPtr<MockContentControllerDelayed> mcc = new NiceMock<MockContentControllerDelayed>();
  nsRefPtr<TestAPZCTreeManager> tm = new TestAPZCTreeManager();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(
    0, mcc, tm, AsyncPanZoomController::USE_GESTURE_DETECTOR);

  apzc->SetFrameMetrics(TestFrameMetrics());
  apzc->NotifyLayersUpdated(TestFrameMetrics(), true);
  apzc->UpdateZoomConstraints(ZoomConstraints(false, false, CSSToScreenScale(1.0), CSSToScreenScale(1.0)));

  int time = 0;
  nsEventStatus status = ApzcTap(apzc, 10, 10, time, 100, mcc.get());
  EXPECT_EQ(nsEventStatus_eIgnore, status);

  // This verifies that the single tap notification is sent after the
  // touchdown is fully processed. The ordering here is important.
  mcc->CheckHasDelayedTask();

  EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(1);
  mcc->RunDelayedTask();

  apzc->AssertStateIsReset();
  apzc->Destroy();
}

TEST_F(AsyncPanZoomControllerTester, MediumPress) {
  nsRefPtr<MockContentControllerDelayed> mcc = new NiceMock<MockContentControllerDelayed>();
  nsRefPtr<TestAPZCTreeManager> tm = new TestAPZCTreeManager();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(
    0, mcc, tm, AsyncPanZoomController::USE_GESTURE_DETECTOR);

  apzc->SetFrameMetrics(TestFrameMetrics());
  apzc->NotifyLayersUpdated(TestFrameMetrics(), true);
  apzc->UpdateZoomConstraints(ZoomConstraints(false, false, CSSToScreenScale(1.0), CSSToScreenScale(1.0)));

  int time = 0;
  nsEventStatus status = ApzcTap(apzc, 10, 10, time, 400, mcc.get());
  EXPECT_EQ(nsEventStatus_eIgnore, status);

  // This verifies that the single tap notification is sent after the
  // touchdown is fully processed. The ordering here is important.
  mcc->CheckHasDelayedTask();

  EXPECT_CALL(*mcc, HandleSingleTap(CSSPoint(10, 10), 0, apzc->GetGuid())).Times(1);
  mcc->RunDelayedTask();

  apzc->AssertStateIsReset();
  apzc->Destroy();
}

void
DoLongPressTest(bool aShouldUseTouchAction, uint32_t aBehavior) {
  nsRefPtr<MockContentControllerDelayed> mcc = new MockContentControllerDelayed();
  nsRefPtr<TestAPZCTreeManager> tm = new TestAPZCTreeManager();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(
    0, mcc, tm, AsyncPanZoomController::USE_GESTURE_DETECTOR);

  apzc->SetFrameMetrics(TestFrameMetrics());
  apzc->NotifyLayersUpdated(TestFrameMetrics(), true);
  apzc->UpdateZoomConstraints(ZoomConstraints(false, false, CSSToScreenScale(1.0), CSSToScreenScale(1.0)));

  apzc->SetTouchActionEnabled(aShouldUseTouchAction);

  int time = 0;

  nsEventStatus status = ApzcDown(apzc, 10, 10, time);
  EXPECT_EQ(nsEventStatus_eConsumeNoDefault, status);

  // SetAllowedTouchBehavior() must be called after sending touch-start.
  nsTArray<uint32_t> allowedTouchBehaviors;
  allowedTouchBehaviors.AppendElement(aBehavior);
  apzc->SetAllowedTouchBehavior(allowedTouchBehaviors);

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

  mcc->CheckHasDelayedTask();

  // Manually invoke the longpress while the touch is currently down.
  check.Call("preHandleLongTap");
  mcc->RunDelayedTask();
  check.Call("postHandleLongTap");

  // Destroy pending MAX_TAP timeout task
  mcc->DestroyOldestTask();
  // There should be a TimeoutContentResponse task in the queue still
  // Clear the waiting-for-content timeout task, then send the signal that
  // content has handled this long tap. This takes the place of the
  // "contextmenu" event.
  mcc->CheckHasDelayedTask();
  mcc->ClearDelayedTask();
  apzc->ContentReceivedTouch(true);

  time += 1000;

  status = ApzcUp(apzc, 10, 10, time);
  EXPECT_EQ(nsEventStatus_eIgnore, status);

  // To get a LongTapUp event, we must kick APZC to flush its event queue. This
  // would normally happen if we had a (Tab|RenderFrame)(Parent|Child)
  // mechanism.
  check.Call("preHandleLongTapUp");
  apzc->ContentReceivedTouch(false);
  check.Call("postHandleLongTapUp");

  apzc->AssertStateIsReset();
  apzc->Destroy();
}

void
DoLongPressPreventDefaultTest(bool aShouldUseTouchAction, uint32_t aBehavior) {
  // We have to initialize both an integer time and TimeStamp time because
  // TimeStamp doesn't have any ToXXX() functions for converting back to
  // primitives.
  TimeStamp testStartTime = TimeStamp::Now();
  int time = 0;
  AsyncPanZoomController::SetFrameTime(testStartTime);

  nsRefPtr<MockContentControllerDelayed> mcc = new MockContentControllerDelayed();
  nsRefPtr<TestAPZCTreeManager> tm = new TestAPZCTreeManager();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(
    0, mcc, tm, AsyncPanZoomController::USE_GESTURE_DETECTOR);

  apzc->SetFrameMetrics(TestFrameMetrics());
  apzc->NotifyLayersUpdated(TestFrameMetrics(), true);
  apzc->UpdateZoomConstraints(ZoomConstraints(false, false, CSSToScreenScale(1.0), CSSToScreenScale(1.0)));

  apzc->SetTouchActionEnabled(aShouldUseTouchAction);

  EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_,_)).Times(0);
  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(0);

  int touchX = 10,
      touchStartY = 10,
      touchEndY = 50;

  nsEventStatus status = ApzcDown(apzc, touchX, touchStartY, time);
  EXPECT_EQ(nsEventStatus_eConsumeNoDefault, status);

  // SetAllowedTouchBehavior() must be called after sending touch-start.
  nsTArray<uint32_t> allowedTouchBehaviors;
  allowedTouchBehaviors.AppendElement(aBehavior);
  apzc->SetAllowedTouchBehavior(allowedTouchBehaviors);

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
  // Clear the waiting-for-content timeout task, then send the signal that
  // content has handled this long tap. This takes the place of the
  // "contextmenu" event.
  mcc->ClearDelayedTask();
  apzc->ContentReceivedTouch(true);

  time += 1000;

  MultiTouchInput mti =
    MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, time, TimeStamp(), 0);
  mti.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(touchX, touchEndY), ScreenSize(0, 0), 0, 0));
  status = apzc->ReceiveInputEvent(mti);
  EXPECT_EQ(nsEventStatus_eIgnore, status);

  EXPECT_CALL(*mcc, HandleLongTapUp(CSSPoint(touchX, touchEndY), 0, apzc->GetGuid())).Times(0);
  status = ApzcUp(apzc, touchX, touchEndY, time);
  EXPECT_EQ(nsEventStatus_eIgnore, status);

  // Flush the event queue. Once the "contextmenu" event is handled, any touch
  // events that come from the same series of start->n*move->end events should
  // be discarded, even if only the "contextmenu" event is preventDefaulted.
  apzc->ContentReceivedTouch(false);

  ScreenPoint pointOut;
  ViewTransform viewTransformOut;
  apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);

  EXPECT_EQ(ScreenPoint(), pointOut);
  EXPECT_EQ(ViewTransform(), viewTransformOut);

  apzc->AssertStateIsReset();
  apzc->Destroy();
}

TEST_F(AsyncPanZoomControllerTester, LongPress) {
  DoLongPressTest(false, mozilla::layers::AllowedTouchBehavior::NONE);
}

TEST_F(AsyncPanZoomControllerTester, LongPressWithTouchAction) {
  DoLongPressTest(true, mozilla::layers::AllowedTouchBehavior::HORIZONTAL_PAN
                      | mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN
                      | mozilla::layers::AllowedTouchBehavior::PINCH_ZOOM);
}

TEST_F(AsyncPanZoomControllerTester, LongPressPreventDefault) {
  DoLongPressPreventDefaultTest(false, mozilla::layers::AllowedTouchBehavior::NONE);
}

TEST_F(AsyncPanZoomControllerTester, LongPressPreventDefaultWithTouchAction) {
  DoLongPressPreventDefaultTest(true, mozilla::layers::AllowedTouchBehavior::HORIZONTAL_PAN
                                    | mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN
                                    | mozilla::layers::AllowedTouchBehavior::PINCH_ZOOM);
}

// Layer tree for HitTesting1
static already_AddRefed<mozilla::layers::Layer>
CreateTestLayerTree1(nsRefPtr<LayerManager>& aLayerManager, nsTArray<nsRefPtr<Layer> >& aLayers) {
  const char* layerTreeSyntax = "c(ttcc)";
  // LayerID                     0 1234
  nsIntRegion layerVisibleRegion[] = {
    nsIntRegion(nsIntRect(0,0,100,100)),
    nsIntRegion(nsIntRect(0,0,100,100)),
    nsIntRegion(nsIntRect(10,10,20,20)),
    nsIntRegion(nsIntRect(10,10,20,20)),
    nsIntRegion(nsIntRect(5,5,20,20)),
  };
  gfx3DMatrix transforms[] = {
    gfx3DMatrix(),
    gfx3DMatrix(),
    gfx3DMatrix(),
    gfx3DMatrix(),
    gfx3DMatrix(),
  };
  return CreateLayerTree(layerTreeSyntax, layerVisibleRegion, transforms, aLayerManager, aLayers);
}

// Layer Tree for HitTesting2
static already_AddRefed<mozilla::layers::Layer>
CreateTestLayerTree2(nsRefPtr<LayerManager>& aLayerManager, nsTArray<nsRefPtr<Layer> >& aLayers) {
  const char* layerTreeSyntax = "c(cc(c))";
  // LayerID                     0 12 3
  nsIntRegion layerVisibleRegion[] = {
    nsIntRegion(nsIntRect(0,0,100,100)),
    nsIntRegion(nsIntRect(10,10,40,40)),
    nsIntRegion(nsIntRect(10,60,40,40)),
    nsIntRegion(nsIntRect(10,60,40,40)),
  };
  gfx3DMatrix transforms[] = {
    gfx3DMatrix(),
    gfx3DMatrix(),
    gfx3DMatrix(),
    gfx3DMatrix(),
  };
  return CreateLayerTree(layerTreeSyntax, layerVisibleRegion, transforms, aLayerManager, aLayers);
}

static void
SetScrollableFrameMetrics(Layer* aLayer, FrameMetrics::ViewID aScrollId,
                          // The scrollable rect is only used in HitTesting2,
                          // HitTesting1 doesn't care about it.
                          CSSRect aScrollableRect = CSSRect(-1, -1, -1, -1))
{
  ContainerLayer* container = aLayer->AsContainerLayer();
  FrameMetrics metrics;
  metrics.SetScrollId(aScrollId);
  nsIntRect layerBound = aLayer->GetVisibleRegion().GetBounds();
  metrics.mCompositionBounds = ParentLayerIntRect(layerBound.x, layerBound.y,
                                                  layerBound.width, layerBound.height);
  metrics.mScrollableRect = aScrollableRect;
  metrics.SetScrollOffset(CSSPoint(0, 0));
  container->SetFrameMetrics(metrics);
}

static already_AddRefed<AsyncPanZoomController>
GetTargetAPZC(APZCTreeManager* manager, const ScreenPoint& aPoint,
              gfx3DMatrix& aTransformToApzcOut, gfx3DMatrix& aTransformToGeckoOut)
{
  nsRefPtr<AsyncPanZoomController> hit = manager->GetTargetAPZC(aPoint, nullptr);
  if (hit) {
    manager->GetInputTransforms(hit.get(), aTransformToApzcOut, aTransformToGeckoOut);
  }
  return hit.forget();
}

// A simple hit testing test that doesn't involve any transforms on layers.
TEST_F(APZCTreeManagerTester, HitTesting1) {
  nsTArray<nsRefPtr<Layer> > layers;
  nsRefPtr<LayerManager> lm;
  nsRefPtr<Layer> root = CreateTestLayerTree1(lm, layers);

  TimeStamp testStartTime = TimeStamp::Now();
  AsyncPanZoomController::SetFrameTime(testStartTime);
  nsRefPtr<MockContentController> mcc = new NiceMock<MockContentController>();
  ScopedLayerTreeRegistration controller(0, root, mcc);

  nsRefPtr<APZCTreeManager> manager = new TestAPZCTreeManager();
  gfx3DMatrix transformToApzc;
  gfx3DMatrix transformToGecko;

  // No APZC attached so hit testing will return no APZC at (20,20)
  nsRefPtr<AsyncPanZoomController> hit = GetTargetAPZC(manager, ScreenPoint(20, 20), transformToApzc, transformToGecko);
  AsyncPanZoomController* nullAPZC = nullptr;
  EXPECT_EQ(nullAPZC, hit.get());
  EXPECT_EQ(gfx3DMatrix(), transformToApzc);
  EXPECT_EQ(gfx3DMatrix(), transformToGecko);

  uint32_t paintSequenceNumber = 0;

  // Now we have a root APZC that will match the page
  SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID);
  manager->UpdatePanZoomControllerTree(nullptr, root, false, 0, paintSequenceNumber++);
  hit = GetTargetAPZC(manager, ScreenPoint(15, 15), transformToApzc, transformToGecko);
  EXPECT_EQ(root->AsContainerLayer()->GetAsyncPanZoomController(), hit.get());
  // expect hit point at LayerIntPoint(15, 15)
  EXPECT_EQ(gfxPoint(15, 15), transformToApzc.Transform(gfxPoint(15, 15)));
  EXPECT_EQ(gfxPoint(15, 15), transformToGecko.Transform(gfxPoint(15, 15)));

  // Now we have a sub APZC with a better fit
  SetScrollableFrameMetrics(layers[3], FrameMetrics::START_SCROLL_ID + 1);
  manager->UpdatePanZoomControllerTree(nullptr, root, false, 0, paintSequenceNumber++);
  EXPECT_NE(root->AsContainerLayer()->GetAsyncPanZoomController(), layers[3]->AsContainerLayer()->GetAsyncPanZoomController());
  hit = GetTargetAPZC(manager, ScreenPoint(15, 15), transformToApzc, transformToGecko);
  EXPECT_EQ(layers[3]->AsContainerLayer()->GetAsyncPanZoomController(), hit.get());
  // expect hit point at LayerIntPoint(15, 15)
  EXPECT_EQ(gfxPoint(15, 15), transformToApzc.Transform(gfxPoint(15, 15)));
  EXPECT_EQ(gfxPoint(15, 15), transformToGecko.Transform(gfxPoint(15, 15)));

  // Now test hit testing when we have two scrollable layers
  hit = GetTargetAPZC(manager, ScreenPoint(15, 15), transformToApzc, transformToGecko);
  EXPECT_EQ(layers[3]->AsContainerLayer()->GetAsyncPanZoomController(), hit.get());
  SetScrollableFrameMetrics(layers[4], FrameMetrics::START_SCROLL_ID + 2);
  manager->UpdatePanZoomControllerTree(nullptr, root, false, 0, paintSequenceNumber++);
  hit = GetTargetAPZC(manager, ScreenPoint(15, 15), transformToApzc, transformToGecko);
  EXPECT_EQ(layers[4]->AsContainerLayer()->GetAsyncPanZoomController(), hit.get());
  // expect hit point at LayerIntPoint(15, 15)
  EXPECT_EQ(gfxPoint(15, 15), transformToApzc.Transform(gfxPoint(15, 15)));
  EXPECT_EQ(gfxPoint(15, 15), transformToGecko.Transform(gfxPoint(15, 15)));

  // Hit test ouside the reach of layer[3,4] but inside root
  hit = GetTargetAPZC(manager, ScreenPoint(90, 90), transformToApzc, transformToGecko);
  EXPECT_EQ(root->AsContainerLayer()->GetAsyncPanZoomController(), hit.get());
  // expect hit point at LayerIntPoint(90, 90)
  EXPECT_EQ(gfxPoint(90, 90), transformToApzc.Transform(gfxPoint(90, 90)));
  EXPECT_EQ(gfxPoint(90, 90), transformToGecko.Transform(gfxPoint(90, 90)));

  // Hit test ouside the reach of any layer
  hit = GetTargetAPZC(manager, ScreenPoint(1000, 10), transformToApzc, transformToGecko);
  EXPECT_EQ(nullAPZC, hit.get());
  EXPECT_EQ(gfx3DMatrix(), transformToApzc);
  EXPECT_EQ(gfx3DMatrix(), transformToGecko);
  hit = GetTargetAPZC(manager, ScreenPoint(-1000, 10), transformToApzc, transformToGecko);
  EXPECT_EQ(nullAPZC, hit.get());
  EXPECT_EQ(gfx3DMatrix(), transformToApzc);
  EXPECT_EQ(gfx3DMatrix(), transformToGecko);

  manager->ClearTree();
}

// A more involved hit testing test that involves css and async transforms.
TEST_F(APZCTreeManagerTester, HitTesting2) {
  nsTArray<nsRefPtr<Layer> > layers;
  nsRefPtr<LayerManager> lm;
  nsRefPtr<Layer> root = CreateTestLayerTree2(lm, layers);

  TimeStamp testStartTime = TimeStamp::Now();
  AsyncPanZoomController::SetFrameTime(testStartTime);
  nsRefPtr<MockContentController> mcc = new NiceMock<MockContentController>();
  ScopedLayerTreeRegistration controller(0, root, mcc);

  nsRefPtr<TestAPZCTreeManager> manager = new TestAPZCTreeManager();
  nsRefPtr<AsyncPanZoomController> hit;
  gfx3DMatrix transformToApzc;
  gfx3DMatrix transformToGecko;

  // Set a CSS transform on one of the layers.
  Matrix4x4 transform;
  transform = transform * Matrix4x4().Scale(2, 1, 1);
  layers[2]->SetBaseTransform(transform);

  // Make some other layers scrollable.
  SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID, CSSRect(0, 0, 200, 200));
  SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID + 1, CSSRect(0, 0, 80, 80));
  SetScrollableFrameMetrics(layers[3], FrameMetrics::START_SCROLL_ID + 2, CSSRect(0, 0, 80, 80));

  manager->UpdatePanZoomControllerTree(nullptr, root, false, 0, 0);

  // At this point, the following holds (all coordinates in screen pixels):
  // layers[0] has content from (0,0)-(200,200), clipped by composition bounds (0,0)-(100,100)
  // layers[1] has content from (10,10)-(90,90), clipped by composition bounds (10,10)-(50,50)
  // layers[2] has content from (20,60)-(100,100). no clipping as it's not a scrollable layer
  // layers[3] has content from (20,60)-(180,140), clipped by composition bounds (20,60)-(100,100)

  AsyncPanZoomController* apzcroot = root->AsContainerLayer()->GetAsyncPanZoomController();
  AsyncPanZoomController* apzc1 = layers[1]->AsContainerLayer()->GetAsyncPanZoomController();
  AsyncPanZoomController* apzc3 = layers[3]->AsContainerLayer()->GetAsyncPanZoomController();

  // Hit an area that's clearly on the root layer but not any of the child layers.
  hit = GetTargetAPZC(manager, ScreenPoint(75, 25), transformToApzc, transformToGecko);
  EXPECT_EQ(apzcroot, hit.get());
  EXPECT_EQ(gfxPoint(75, 25), transformToApzc.Transform(gfxPoint(75, 25)));
  EXPECT_EQ(gfxPoint(75, 25), transformToGecko.Transform(gfxPoint(75, 25)));

  // Hit an area on the root that would be on layers[3] if layers[2]
  // weren't transformed.
  // Note that if layers[2] were scrollable, then this would hit layers[2]
  // because its composition bounds would be at (10,60)-(50,100) (and the
  // scale-only transform that we set on layers[2] would be invalid because
  // it would place the layer into overscroll, as its composition bounds
  // start at x=10 but its content at x=20).
  hit = GetTargetAPZC(manager, ScreenPoint(15, 75), transformToApzc, transformToGecko);
  EXPECT_EQ(apzcroot, hit.get());
  EXPECT_EQ(gfxPoint(15, 75), transformToApzc.Transform(gfxPoint(15, 75)));
  EXPECT_EQ(gfxPoint(15, 75), transformToGecko.Transform(gfxPoint(15, 75)));

  // Hit an area on layers[1].
  hit = GetTargetAPZC(manager, ScreenPoint(25, 25), transformToApzc, transformToGecko);
  EXPECT_EQ(apzc1, hit.get());
  EXPECT_EQ(gfxPoint(25, 25), transformToApzc.Transform(gfxPoint(25, 25)));
  EXPECT_EQ(gfxPoint(25, 25), transformToGecko.Transform(gfxPoint(25, 25)));

  // Hit an area on layers[3].
  hit = GetTargetAPZC(manager, ScreenPoint(25, 75), transformToApzc, transformToGecko);
  EXPECT_EQ(apzc3, hit.get());
  // transformToApzc should unapply layers[2]'s transform
  EXPECT_EQ(gfxPoint(12.5, 75), transformToApzc.Transform(gfxPoint(25, 75)));
  // and transformToGecko should reapply it
  EXPECT_EQ(gfxPoint(25, 75), transformToGecko.Transform(gfxPoint(12.5, 75)));

  // Hit an area on layers[3] that would be on the root if layers[2]
  // weren't transformed.
  hit = GetTargetAPZC(manager, ScreenPoint(75, 75), transformToApzc, transformToGecko);
  EXPECT_EQ(apzc3, hit.get());
  // transformToApzc should unapply layers[2]'s transform
  EXPECT_EQ(gfxPoint(37.5, 75), transformToApzc.Transform(gfxPoint(75, 75)));
  // and transformToGecko should reapply it
  EXPECT_EQ(gfxPoint(75, 75), transformToGecko.Transform(gfxPoint(37.5, 75)));

  // Pan the root layer upward by 50 pixels.
  // This causes layers[1] to scroll out of view, and an async transform
  // of -50 to be set on the root layer.
  int time = 0;
  // Silence GMock warnings about "uninteresting mock function calls".
  EXPECT_CALL(*mcc, PostDelayedTask(_,_)).Times(AtLeast(1));
  EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_,_)).Times(AtLeast(1));
  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);

  // This first pan will move the APZC by 50 pixels, and dispatch a paint request.
  // Since this paint request is in the queue to Gecko, transformToGecko will
  // take it into account.
  ApzcPan(apzcroot, manager, time, 100, 50);

  // Hit where layers[3] used to be. It should now hit the root.
  hit = GetTargetAPZC(manager, ScreenPoint(75, 75), transformToApzc, transformToGecko);
  EXPECT_EQ(apzcroot, hit.get());
  // transformToApzc doesn't unapply the root's own async transform
  EXPECT_EQ(gfxPoint(75, 75), transformToApzc.Transform(gfxPoint(75, 75)));
  // and transformToGecko unapplies it and then reapplies it, because by the
  // time the event being transformed reaches Gecko the new paint request will
  // have been handled.
  EXPECT_EQ(gfxPoint(75, 75), transformToGecko.Transform(gfxPoint(75, 75)));

  // Hit where layers[1] used to be and where layers[3] should now be.
  hit = GetTargetAPZC(manager, ScreenPoint(25, 25), transformToApzc, transformToGecko);
  EXPECT_EQ(apzc3, hit.get());
  // transformToApzc unapplies both layers[2]'s css transform and the root's
  // async transform
  EXPECT_EQ(gfxPoint(12.5, 75), transformToApzc.Transform(gfxPoint(25, 25)));
  // transformToGecko reapplies both the css transform and the async transform
  // because we have already issued a paint request with it.
  EXPECT_EQ(gfxPoint(25, 25), transformToGecko.Transform(gfxPoint(12.5, 75)));

  // This second pan will move the APZC by another 50 pixels but since the paint
  // request dispatched above has not "completed", we will not dispatch another
  // one yet. Now we have an async transform on top of the pending paint request
  // transform.
  ApzcPan(apzcroot, manager, time, 100, 50);

  // Hit where layers[3] used to be. It should now hit the root.
  hit = GetTargetAPZC(manager, ScreenPoint(75, 75), transformToApzc, transformToGecko);
  EXPECT_EQ(apzcroot, hit.get());
  // transformToApzc doesn't unapply the root's own async transform
  EXPECT_EQ(gfxPoint(75, 75), transformToApzc.Transform(gfxPoint(75, 75)));
  // transformToGecko unapplies the full async transform of -100 pixels, and then
  // reapplies the "D" transform of -50 leading to an overall adjustment of +50
  EXPECT_EQ(gfxPoint(75, 125), transformToGecko.Transform(gfxPoint(75, 75)));

  // Hit where layers[1] used to be. It should now hit the root.
  hit = GetTargetAPZC(manager, ScreenPoint(25, 25), transformToApzc, transformToGecko);
  EXPECT_EQ(apzcroot, hit.get());
  // transformToApzc doesn't unapply the root's own async transform
  EXPECT_EQ(gfxPoint(25, 25), transformToApzc.Transform(gfxPoint(25, 25)));
  // transformToGecko unapplies the full async transform of -100 pixels, and then
  // reapplies the "D" transform of -50 leading to an overall adjustment of +50
  EXPECT_EQ(gfxPoint(25, 75), transformToGecko.Transform(gfxPoint(25, 25)));

  manager->ClearTree();
}
